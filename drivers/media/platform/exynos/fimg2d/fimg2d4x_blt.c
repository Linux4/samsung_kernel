/* linux/drivers/media/video/exynos/fimg2d/fimg2d4x_blt.c
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd.
 *	http://www.samsung.com/
 *
 * Samsung Graphics 2D driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/sched.h>
#include <linux/uaccess.h>
#include <linux/atomic.h>
#include <linux/dma-mapping.h>
#include <linux/rmap.h>
#include <linux/fs.h>
#include <linux/exynos_iovmm.h>
#include <linux/clk-private.h>
#include <asm/cacheflush.h>
#ifdef CONFIG_PM_RUNTIME
#include <plat/devs.h>
#include <linux/pm_runtime.h>
#endif
#include "fimg2d.h"
#include "fimg2d_clk.h"
#include "fimg2d4x.h"
#include "fimg2d_ctx.h"
#include "fimg2d_cache.h"
#include "fimg2d_helper.h"

#define BLIT_TIMEOUT	msecs_to_jiffies(8000)

#define MAX_PREFBUFS	6
static int nbufs;
static struct sysmmu_prefbuf prefbuf[MAX_PREFBUFS];

#ifdef CONFIG_PM_RUNTIME
static int fimg2d4x_get_clk_cnt(struct clk *clk)
{
	return __clk_is_enabled(clk);
}
#endif

static void fimg2d4x_cleanup_pgtable(struct fimg2d_control *ctrl,
					struct fimg2d_bltcmd *cmd,
					enum image_object idx,
					bool plane2)
{
	if (cmd->dma[idx].base.size > 0) {
		exynos_sysmmu_unmap_user_pages(ctrl->dev,
				cmd->ctx->mm, cmd->dma[idx].base.addr,
				cmd->dma[idx].base.size);
	}

	if (plane2 && cmd->dma[idx].plane2.size > 0) {
		exynos_sysmmu_unmap_user_pages(ctrl->dev,
				cmd->ctx->mm, cmd->dma[idx].plane2.addr,
				cmd->dma[idx].plane2.size);
	}
}

static int fimg2d4x_blit_wait(struct fimg2d_control *ctrl,
		struct fimg2d_bltcmd *cmd)
{
	int ret;

	ret = wait_event_timeout(ctrl->wait_q, !atomic_read(&ctrl->busy),
			BLIT_TIMEOUT);
	if (!ret) {
		fimg2d_err("blit wait timeout\n");

		fimg2d4x_disable_irq(ctrl);
		if (!fimg2d4x_blit_done_status(ctrl))
			fimg2d_err("blit not finished\n");

		fimg2d_debug_command(cmd);
		fimg2d4x_reset(ctrl);

		return -1;
	}
	return 0;
}

static void fimg2d4x_pre_bitblt(struct fimg2d_control *ctrl,
		struct fimg2d_bltcmd *cmd)
{
	switch (ctrl->pdata->ip_ver) {
	case IP_VER_G2D_5AR2:
	/* disable cci path */
		g2d_cci_snoop_control(ctrl->pdata->ip_ver,
			       NON_SHAREABLE_PATH, SHARED_G2D_SEL);
		break;

	case IP_VER_G2D_5H:
	case IP_VER_G2D_5HP:
#ifndef CCI_SNOOP
		/* disable cci path */
		g2d_cci_snoop_control(ctrl->pdata->ip_ver,
				NON_SHAREABLE_PATH, SHARED_FROM_SYSMMU);
		fimg2d_debug("disable cci\n");
#endif
#ifdef CCI_SNOOP
		/* enable cci path */
		g2d_cci_snoop_control(ctrl->pdata->ip_ver,
				SHAREABLE_PATH, SHARED_G2D_SEL);
		fimg2d_debug("enable cci\n");
#endif
		break;

	default:
		fimg2d_err("g2d_cci_snoop_control is not called\n");
		break;
	}
}

int fimg2d4x_bitblt(struct fimg2d_control *ctrl)
{
	int ret = 0;
	enum addr_space addr_type;
	struct fimg2d_context *ctx;
	struct fimg2d_bltcmd *cmd;
	unsigned long *pgd;

	fimg2d_debug("%s : enter blitter\n", __func__);

	while (1) {
		cmd = fimg2d_get_command(ctrl);
		if (!cmd)
			break;

		ctx = cmd->ctx;
		ctx->state = CTX_READY;

#ifdef CONFIG_PM_RUNTIME
		if (fimg2d4x_get_clk_cnt(ctrl->clock) == false)
			fimg2d_err("2D clock is not set\n");
#endif

		addr_type = cmd->image[IDST].addr.type;

		atomic_set(&ctrl->busy, 1);
		perf_start(cmd, PERF_SFR);
		ret = ctrl->configure(ctrl, cmd);
		perf_end(cmd, PERF_SFR);
		if (IS_ERR_VALUE(ret)) {
			fimg2d_err("failed to configure\n");
			ctx->state = CTX_ERROR;
			goto fail_n_del;
		}

		if (fimg2d_check_pgd(ctx->mm, cmd)) {
			ret = -EFAULT;
			goto fail_n_unmap;
		}

		if (addr_type == ADDR_USER || addr_type == ADDR_USER_CONTIG) {
			if (!ctx->mm || !ctx->mm->pgd) {
				atomic_set(&ctrl->busy, 0);
				fimg2d_err("ctx->mm:0x%p or ctx->mm->pgd:0x%p\n",
					       ctx->mm,
					       (ctx->mm) ? ctx->mm->pgd : NULL);
				ret = -EPERM;
				goto fail_n_unmap;
			}
			pgd = (unsigned long *)ctx->mm->pgd;
			if (iovmm_activate(ctrl->dev)) {
				fimg2d_err("failed to iovmm activate\n");
				ret = -EPERM;
				goto fail_n_unmap;
			}
			fimg2d_debug("%s : sysmmu enable: pgd %p ctx %p seq_no(%u)\n",
				__func__, pgd, ctx, cmd->blt.seq_no);

			//exynos_sysmmu_set_pbuf(ctrl->dev, nbufs, prefbuf);
			fimg2d_debug("%s : set smmu prefbuf\n", __func__);
		}

		fimg2d4x_pre_bitblt(ctrl, cmd);

		perf_start(cmd, PERF_BLIT);
		/* start blit */
		fimg2d_debug("%s : start blit\n", __func__);
		ctrl->run(ctrl);
		ret = fimg2d4x_blit_wait(ctrl, cmd);
		perf_end(cmd, PERF_BLIT);

		if (addr_type == ADDR_USER || addr_type == ADDR_USER_CONTIG)
			iovmm_deactivate(ctrl->dev);

fail_n_unmap:
		perf_start(cmd, PERF_UNMAP);
		if (addr_type == ADDR_USER || addr_type == ADDR_USER_CONTIG) {
			fimg2d4x_cleanup_pgtable(ctrl, cmd, ISRC, true);
			fimg2d4x_cleanup_pgtable(ctrl, cmd, IMSK, false);
			fimg2d4x_cleanup_pgtable(ctrl, cmd, IDST, true);
			fimg2d_debug("sysmmu disable\n");
		}
		perf_end(cmd, PERF_UNMAP);
fail_n_del:
		fimg2d_del_command(ctrl, cmd);
	}

	fimg2d_debug("%s : exit blitter\n", __func__);

	return ret;
}

static inline bool is_opaque(enum color_format fmt)
{
	switch (fmt) {
	case CF_ARGB_8888:
	case CF_ARGB_1555:
	case CF_ARGB_4444:
		return false;

	default:
		return true;
	}
}

static int fast_op(struct fimg2d_bltcmd *cmd)
{
	int fop;
	int sa, da, ga;
	struct fimg2d_param *p;
	struct fimg2d_image *src, *msk, *dst;

	p = &cmd->blt.param;
	src = &cmd->image[ISRC];
	msk = &cmd->image[IMSK];
	dst = &cmd->image[IDST];

	fop = cmd->blt.op;

	if (msk->addr.type)
		return fop;

	ga = p->g_alpha;
	da = is_opaque(dst->fmt) ? 0xff : 0;

	if (!src->addr.type)
		sa = (p->solid_color >> 24) & 0xff;
	else
		sa = is_opaque(src->fmt) ? 0xff : 0;

	switch (cmd->blt.op) {
	case BLIT_OP_SRC_OVER:
		/* Sc + (1-Sa)*Dc = Sc */
		if (sa == 0xff && ga == 0xff)
			fop = BLIT_OP_SRC;
		break;
	case BLIT_OP_DST_OVER:
		/* (1-Da)*Sc + Dc = Dc */
		if (da == 0xff)
			fop = BLIT_OP_DST;	/* nop */
		break;
	case BLIT_OP_SRC_IN:
		/* Da*Sc = Sc */
		if (da == 0xff)
			fop = BLIT_OP_SRC;
		break;
	case BLIT_OP_DST_IN:
		/* Sa*Dc = Dc */
		if (sa == 0xff && ga == 0xff)
			fop = BLIT_OP_DST;	/* nop */
		break;
	case BLIT_OP_SRC_OUT:
		/* (1-Da)*Sc = 0 */
		if (da == 0xff)
			fop = BLIT_OP_CLR;
		break;
	case BLIT_OP_DST_OUT:
		/* (1-Sa)*Dc = 0 */
		if (sa == 0xff && ga == 0xff)
			fop = BLIT_OP_CLR;
		break;
	case BLIT_OP_SRC_ATOP:
		/* Da*Sc + (1-Sa)*Dc = Sc */
		if (sa == 0xff && da == 0xff && ga == 0xff)
			fop = BLIT_OP_SRC;
		break;
	case BLIT_OP_DST_ATOP:
		/* (1-Da)*Sc + Sa*Dc = Dc */
		if (sa == 0xff && da == 0xff && ga == 0xff)
			fop = BLIT_OP_DST;	/* nop */
		break;
	default:
		break;
	}

	if (fop == BLIT_OP_SRC && !src->addr.type && ga == 0xff)
		fop = BLIT_OP_SOLID_FILL;

	return fop;
}

static int fimg2d4x_configure(struct fimg2d_control *ctrl,
		struct fimg2d_bltcmd *cmd)
{
	int op;
	enum image_sel srcsel, dstsel;
	struct fimg2d_param *p;
	struct fimg2d_image *src, *msk, *dst;
	struct sysmmu_prefbuf *pbuf;
	int ret;

	fimg2d_debug("ctx %p seq_no(%u)\n", cmd->ctx, cmd->blt.seq_no);

	p = &cmd->blt.param;
	src = &cmd->image[ISRC];
	msk = &cmd->image[IMSK];
	dst = &cmd->image[IDST];

	fimg2d4x_init(ctrl);

	/* src and dst select */
	srcsel = dstsel = IMG_MEMORY;

	op = fast_op(cmd);

	switch (op) {
	case BLIT_OP_SOLID_FILL:
		srcsel = dstsel = IMG_FGCOLOR;
		fimg2d4x_set_fgcolor(ctrl, p->solid_color);
		break;
	case BLIT_OP_CLR:
		srcsel = dstsel = IMG_FGCOLOR;
		fimg2d4x_set_color_fill(ctrl, 0);
		break;
	case BLIT_OP_DST:
		srcsel = dstsel = IMG_FGCOLOR;
		break;
	default:
		if (!src->addr.type) {
			srcsel = IMG_FGCOLOR;
			fimg2d4x_set_fgcolor(ctrl, p->solid_color);
		}

		if (op == BLIT_OP_SRC)
			dstsel = IMG_FGCOLOR;

		fimg2d4x_enable_alpha(ctrl, p->g_alpha);
		fimg2d4x_set_alpha_composite(ctrl, op, p->g_alpha);
		if (p->premult == NON_PREMULTIPLIED)
			fimg2d4x_set_premultiplied(ctrl);
		break;
	}

	fimg2d4x_set_src_type(ctrl, srcsel);
	fimg2d4x_set_dst_type(ctrl, dstsel);

	nbufs = 0;
	pbuf = &prefbuf[nbufs];

	/* src */
	if (src->addr.type) {
		fimg2d4x_set_src_image(ctrl, src);
		fimg2d4x_set_src_rect(ctrl, &src->rect);
		fimg2d4x_set_src_repeat(ctrl, &p->repeat);
		if (p->scaling.mode)
			fimg2d4x_set_src_scaling(ctrl, &p->scaling, &p->repeat);

		/* prefbuf */
		pbuf->base = cmd->dma[ISRC].base.addr;
		pbuf->size = cmd->dma[ISRC].base.size;
		pbuf->config = SYSMMU_PBUFCFG_DEFAULT_INPUT;
		nbufs++;
		pbuf++;
		if (src->order == P2_CRCB || src->order == P2_CBCR) {
			pbuf->base = cmd->dma[ISRC].plane2.addr;
			pbuf->size = cmd->dma[ISRC].plane2.size;
			pbuf->config = SYSMMU_PBUFCFG_DEFAULT_INPUT;
			nbufs++;
			pbuf++;
		}

		ret = exynos_sysmmu_map_user_pages(
				ctrl->dev, cmd->ctx->mm,
				cmd->dma[ISRC].base.addr,
				cmd->dma[ISRC].base.size, 0);
		if (IS_ERR_VALUE(ret)) {
			fimg2d_err("s/w fallback (%d-0:%d)\n", ISRC, ret);
			return ret;
		}

		if (src->order == P2_CRCB || src->order == P2_CBCR) {
			ret = exynos_sysmmu_map_user_pages(
					ctrl->dev, cmd->ctx->mm,
					cmd->dma[ISRC].plane2.addr,
					cmd->dma[ISRC].plane2.size, 0);
			if (IS_ERR_VALUE(ret)) {
				fimg2d_err("s/w fallback (%d-1:%d)\n", ISRC, ret);
				fimg2d4x_cleanup_pgtable(ctrl, cmd, ISRC, false);
				return ret;
			}
		}
	}

	/* msk */
	if (msk->addr.type) {
		fimg2d4x_enable_msk(ctrl);
		fimg2d4x_set_msk_image(ctrl, msk);
		fimg2d4x_set_msk_rect(ctrl, &msk->rect);
		fimg2d4x_set_msk_repeat(ctrl, &p->repeat);
		if (p->scaling.mode)
			fimg2d4x_set_msk_scaling(ctrl, &p->scaling, &p->repeat);

		/* prefbuf */
		pbuf->base = cmd->dma[IMSK].base.addr;
		pbuf->size = cmd->dma[IMSK].base.size;
		pbuf->config = SYSMMU_PBUFCFG_DEFAULT_INPUT;
		nbufs++;
		pbuf++;

		ret = exynos_sysmmu_map_user_pages(
				ctrl->dev, cmd->ctx->mm,
				cmd->dma[IMSK].base.addr,
				cmd->dma[IMSK].base.size, 0);
		if (IS_ERR_VALUE(ret)) {
			fimg2d_err("s/w fallback (%d:%d)\n", IMSK, ret);
			fimg2d4x_cleanup_pgtable(ctrl, cmd, ISRC, true);
			return ret;
		}
	}

	/* dst */
	if (dst->addr.type) {
		fimg2d4x_set_dst_image(ctrl, dst);
		fimg2d4x_set_dst_rect(ctrl, &dst->rect);
		if (p->clipping.enable)
			fimg2d4x_enable_clipping(ctrl, &p->clipping);

		/* prefbuf */
		pbuf->base = cmd->dma[IDST].base.addr;
		pbuf->size = cmd->dma[IDST].base.size;
		pbuf->config = SYSMMU_PBUFCFG_DEFAULT_OUTPUT;
		nbufs++;
		pbuf++;
		if (dst->order == P2_CRCB || dst->order == P2_CBCR) {
			pbuf->base = cmd->dma[IDST].plane2.addr;
			pbuf->size = cmd->dma[IDST].plane2.size;
			pbuf->config = SYSMMU_PBUFCFG_DEFAULT_OUTPUT;
			nbufs++;
			pbuf++;
		}

		ret = exynos_sysmmu_map_user_pages(
				ctrl->dev, cmd->ctx->mm,
				cmd->dma[IDST].base.addr,
				cmd->dma[IDST].base.size, 1);
		if (IS_ERR_VALUE(ret)) {
			fimg2d_err("s/w fallback (%d-0:%d)\n", IDST, ret);
			fimg2d4x_cleanup_pgtable(ctrl, cmd, ISRC, true);
			fimg2d4x_cleanup_pgtable(ctrl, cmd, IMSK, false);
			return ret;
		}

		if (dst->order == P2_CRCB || dst->order == P2_CBCR) {
			ret = exynos_sysmmu_map_user_pages(
					ctrl->dev, cmd->ctx->mm,
					cmd->dma[IDST].plane2.addr,
					cmd->dma[IDST].plane2.size, 1);
			if (IS_ERR_VALUE(ret)) {
				fimg2d_err("s/w fallback (%d-1:%d)\n", IDST, ret);
				fimg2d4x_cleanup_pgtable(ctrl, cmd, ISRC, true);
				fimg2d4x_cleanup_pgtable(ctrl, cmd, IMSK, false);
				fimg2d4x_cleanup_pgtable(ctrl, cmd, IDST, false);
				return ret;
			}
		}
	}

	sysmmu_set_prefetch_buffer_by_region(ctrl->dev, prefbuf, nbufs);

	/* bluescreen */
	if (p->bluscr.mode)
		fimg2d4x_set_bluescreen(ctrl, &p->bluscr);

	/* rotation */
	if (p->rotate)
		fimg2d4x_set_rotation(ctrl, p->rotate);

	/* dithering */
	if (p->dither)
		fimg2d4x_enable_dithering(ctrl);

	return 0;
}

static void fimg2d4x_run(struct fimg2d_control *ctrl)
{
	fimg2d_debug("start blit\n");
	fimg2d4x_enable_irq(ctrl);
	fimg2d4x_clear_irq(ctrl);
	fimg2d4x_start_blit(ctrl);
}

static void fimg2d4x_stop(struct fimg2d_control *ctrl)
{
	if (fimg2d4x_is_blit_done(ctrl)) {
		fimg2d_debug("blit done\n");
		fimg2d4x_disable_irq(ctrl);
		fimg2d4x_clear_irq(ctrl);
		atomic_set(&ctrl->busy, 0);
		wake_up(&ctrl->wait_q);
	}
}

static void fimg2d4x_dump(struct fimg2d_control *ctrl)
{
	fimg2d4x_dump_regs(ctrl);
}

int fimg2d_register_ops(struct fimg2d_control *ctrl)
{
	ctrl->blit = fimg2d4x_bitblt;
	ctrl->configure = fimg2d4x_configure;
	ctrl->run = fimg2d4x_run;
	ctrl->dump = fimg2d4x_dump;
	ctrl->stop = fimg2d4x_stop;

	return 0;
}
