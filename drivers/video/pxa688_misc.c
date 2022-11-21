/*
 * linux/drivers/video/pxa688fb_misc.c -- Marvell PXA668 LCD Controller
 *
 * Copyright (C) Marvell Semiconductor Company.  All rights reserved.
 *
 * 2011-05-25  Jing Xiang <jxiang@marvell.com>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of this archive for
 * more details.
 */

//#include <linux/module.h>
//#include <linux/moduleparam.h>
//#include <linux/kernel.h>
//#include <linux/sched.h>
//#include <linux/errno.h>
//#include <linux/string.h>
//#include <linux/interrupt.h>
//#include <linux/slab.h>
//#include <linux/delay.h>
//#include <linux/init.h>
//#include <linux/ioport.h>
//#include <linux/cpufreq.h>
//#include <linux/platform_device.h>
//#include <linux/dma-mapping.h>
//#include <linux/clk.h>
//#include <linux/err.h>
//#include <linux/uaccess.h>
//#include <linux/console.h>
//#include <linux/timer.h>
//#include <linux/io.h>

//#include <asm/irq.h>
//#include <mach/pxa168fb.h>

#include "pxa168fb_common.h"

/* fb_vsmooth: the path that need to do smoothing. e.g. TV
 * fb_filter: the path that used for smoothing. e.g. PN2
 */
int fb_vsmooth;
int fb_filter;
int gfx_vsmooth;
int vid_vsmooth;

static int debug;

/* graphic layer partial display, color format should be RGB565 */
int pxa688fb_partdisp_set(struct mvdisp_partdisp grap)
{
	struct pxa168fb_info *fbi = gfx_info.fbi[1];
	struct fb_info *info = fbi->fb_info;
	struct fb_var_screeninfo *var = &info->var;
	struct lcd_regs *regs;
	u32 xres, yres, yres_z, color3_0, color7_4, color11_8, color15_12;
	u32 base, mask, gfx_fmt, bytespp, shift, offset, threshold, region, tmp;

	gfx_fmt = (dma_ctrl_read(grap.id, 0) & (0xf << 16)) >> 16;
	if (gfx_fmt == PIX_FMT_RGB565 || gfx_fmt == PIX_FMT_RGB1555 >> 1 ||
		gfx_fmt == PIX_FMT_YUV422PACK >> 1)
		bytespp = 16 >> 3;
	else if (gfx_fmt == PIX_FMT_RGB888PACK >> 1)
		bytespp = 24 >> 3;
	else if (gfx_fmt == PIX_FMT_RGB888UNPACK >> 1 ||
		gfx_fmt == PIX_FMT_RGBA888 >> 1)
		bytespp = 32 >> 3;
	else
		return -EINVAL;

	regs = get_regs(grap.id);
	xres = readl(&regs->g_size) & 0xfff;
	yres = (readl(&regs->g_size) & 0xfff0000) >> 16;
	yres_z = (readl(&regs->g_size_z) & 0xfff0000) >> 16;

	if (!yres)
		return -EINVAL;

	/* partial display region should be not larger than source size*/
	if (grap.horpix_start > xres)
		grap.horpix_start = xres;
	if (grap.horpix_end > xres)
		grap.horpix_end = xres;
	if (grap.vertline_start > yres)
		grap.vertline_start = yres;
	if (grap.vertline_end > yres)
		grap.vertline_end = yres;

	if (grap.id == 1 &&
		var->vmode & FB_VMODE_INTERLACED) {
		/* tv interlace mode */
		grap.vertline_start = grap.vertline_start >> 1;
		grap.vertline_end = grap.vertline_end >> 1;
	}

	/* adjust vertical start/end lines according to zoom size */
	grap.vertline_start = grap.vertline_start * yres_z / yres;
	grap.vertline_end = grap.vertline_end * yres_z / yres;

	/* adjust hortizontal start/end pixel number according to:
	 * 1. start pixel number should be
	 *    (DMA burst length / bytes per pixel) aligned.
	 * 2. (end pixel number - start pixel number -
	 *    path threshold / bytes per pixel) should be
	 *    64 / bytes per pixel aligned.
	 */
	if (grap.horpix_end > grap.horpix_start) {
		shift = (grap.id == 1 ? 14 : 10);
		offset = (grap.id == 1 ? 16 : 1);

		/* THRESHOLD_x: the least bytes to operate for
		 * horizontal partial display
		 */
		threshold = (grap.id == 1 ? THRESHOLD_TV : THRESHOLD_PN);
		base = (u32)fbi->reg_base +
			(grap.id == 2 ? PN2_IOPAD_CONTROL : LCD_TOP_CTRL);
		mask = readl(base) & (3 << shift);
		mask = (((mask >> shift) + 1) << 6) / bytespp;

		/* adjust horizontal start pixel number */
		grap.horpix_start /= mask;
		grap.horpix_start *= mask;

		/* adjust horizontal end pixel number */
		region = grap.horpix_end - grap.horpix_start;
		if (region  > (threshold / bytespp)) {
			region -= (threshold / bytespp);

			/* BURST_LEN: AXI burst size, platform dependent */
			if (region >= (BURST_LEN / bytespp)) {
				tmp = region % (BURST_LEN / bytespp);
				region /= (BURST_LEN / bytespp);
				region *= (BURST_LEN / bytespp);
				grap.horpix_end = grap.horpix_start + region +
					threshold / bytespp;
				if (grap.id == 1 && tmp >= (THRESHOLD_PN * 2 -
					THRESHOLD_TV) / bytespp)
					/* add extra 64 /bytespp for TV path*/
					grap.horpix_end += (BURST_LEN / bytespp);
			} else
				grap.horpix_end = grap.horpix_start +
					threshold / bytespp + offset;
		} else
			grap.horpix_end = grap.horpix_start;
	}

	color3_0 = grap.color & 0x000f;
	color7_4 = (grap.color & 0x00f0) >> 4;
	color11_8 = (grap.color & 0x0f00) >> 8;
	color15_12 = (grap.color & 0xf000) >> 12;

	/* horizontal register setting */
	mask = grap.horpix_start | (color3_0 << 12)
		| (grap.horpix_end << 16) | (color7_4 << 28);
	writel(mask, (u32)fbi->reg_base + gra_partdisp_ctrl_hor(grap.id));
	/* vertical register setting */
	mask = grap.vertline_start	| (color11_8  << 12)
		| (grap.vertline_end << 16) | (color15_12 << 28);
	writel(mask, (u32)fbi->reg_base + gra_partdisp_ctrl_ver(grap.id));

	return 0;
}

/* for partial display, only vertical lines need be updated
 * when zoom size changed */
void pxa688fb_partdisp_update(int id)
{
	struct pxa168fb_info *fbi = gfx_info.fbi[id];
	u32 base, mask, vertline_start, vertline_end,
		screen_active, yres, yres_bak;
	struct lcd_regs *regs;

	regs = get_regs(fbi->id);
	screen_active = readl(&regs->screen_active);
	if (!fbi->scrn_act_bak)
		fbi->scrn_act_bak = screen_active;
	if (fbi->scrn_act_bak == screen_active)
		/* no need to update partial display */
		return;

	base = (u32)fbi->reg_base;
	mask = readl(base + gra_partdisp_ctrl_ver(id));

	/* get original partial display vertical setting */
	vertline_start = mask & 0xfff;
	vertline_end = (mask & 0xfff0000) >> 16;

	/* get original/new vertical lines */
	yres_bak = (fbi->scrn_act_bak & 0x0fff0000) >> 16;
	yres = (screen_active & 0x0fff0000) >> 16;

	/* adjust partial display start/end vertical lines by
	 * new / original ratio */
	vertline_start = vertline_start * yres / yres_bak;
	vertline_end = vertline_end * yres / yres_bak;

	mask &= ~0xfff0fff;
	mask |= vertline_start | (vertline_end << 16);
	writel(mask, base + gra_partdisp_ctrl_ver(id));
	fbi->scrn_act_bak = screen_active;
}

static int pxa688fb_map_layers(int src, int dst, int vid, int en)
{
	struct pxa168fb_info *fbi = gfx_info.fbi[0];
	u32 map = (u32)fbi->reg_base + LCD_IO_OVERL_MAP_CTRL;
	u32 val = readl(map), shift;
#ifdef CONFIG_PXA688_VDMA
	struct pxa168fb_vdma_info *lcd_vdma = 0;
	u32 vdma;
#endif
	/* map src path dma to dst */
	switch (dst) {
	case 0:
		if (src == 2)
			/* p2 -> pn */
			shift = vid ? 6 : 7;
		else if (src == 1) {
			/* tv -> pn */
			map = (u32)fbi->reg_base + LCD_TOP_CTRL;
			val = readl(map);
			shift = 22;
			val &= ~(3 << shift);
			if (en)
				val |= 1 << shift;
			goto top_ctrl;
		} else
			return -EINVAL;
		break;
	case 1:
		if (src == 2)
			/* p2 -> tv */
			shift = vid ? 10 : 9;
		else if (src == 0) {
			/* pn -> tv */
			map = (u32)fbi->reg_base + LCD_TOP_CTRL;
			val = readl(map);
			shift = 22;
			val &= ~(3 << shift);
			if (en)
				val |= 2 << shift;
			goto top_ctrl;
		} else
			return -EINVAL;
		break;
	case 2:
		if (src == 1)
			/* tv -> p2 */
			shift = vid ? 3 : 4;
		else if (src == 0)
			/* pn -> p2 */
			shift = vid ? 0 : 1;
		else
			return -EINVAL;
		break;
	default:
		return -EINVAL;
		break;
	}
	if (en)
		val |= 1 << shift;
	else
		val &= ~(1 << shift);
top_ctrl:
	writel(val, map);
	if (debug)
		pr_info("%s %d: src %d dst %d vid %d shift %d map(%x): 0x%x\n",
			__func__, en, src, dst, vid, shift, map & 0xfff, val);


#ifdef CONFIG_PXA688_VDMA
	lcd_vdma = request_vdma(dst, vid);
	if (lcd_vdma && lcd_vdma->enable) {
		vdma = readl((u32)fbi->reg_base + LCD_PN2_SQULN2_CTRL);
		switch (src) {
		case 2:
			/* vdma0-pn/vdma1-tv is used for p2 */
			vdma &= ~(3 << 30);
			if (en)
				vdma |= 1 << (dst ? 31 : 30);
			break;
		case 1:
		case 0:
			/* FIXME */
		default:
			pr_info("%s src %d dst %d not supported yet\n",
				__func__, src, dst);
		return -EINVAL;
		}
		writel(vdma, (u32)fbi->reg_base + LCD_PN2_SQULN2_CTRL);
		if (debug)
			pr_info("vdma 0x%x\n", vdma);
	}
#endif


	return 0;
}

static int pxa688fb_vsmooth_config(int filter, int dst, int vid, int en)
{
	struct pxa168fb_info *fbi = gfx_info.fbi[0];
	int vsmooth = (u32)fbi->reg_base + LCD_AFA_ALL2ONE, shift, val, x;

	switch (filter) {
	case 0:
		/* pn dma used as vertical filter channel */
		shift = vid ? 16 : 18;
		if (dst == 2) {
			pr_err("%s (line %d) filter %d dst %d not supported\n",
				__func__, __LINE__, filter, dst);
			return -EINVAL;
		} else
			x = 3;
		break;
	case 1:
		/* tv dma used as vertical filter channel */
		shift = vid ? 16 : 18;
		if (dst == 2) {
			pr_err("%s (line %d) filter %d dst %d not supported\n",
				__func__, __LINE__, filter, dst);
			return -EINVAL;
		} else
			x = 2;
		break;
	case 2:
		/* p2 dma used as vertical filter channel */
		vsmooth = (u32)fbi->reg_base + LCD_PN2_LAYER_ALPHA_SEL1;
		shift = vid ? 16 : 18;
		x = dst ? 3 : 2;
		break;
	default:
		pr_err("%s (line %d) filter %d dst %d not supported\n",
			__func__, __LINE__, filter, dst);
		return -EINVAL;
	}
	val = readl(vsmooth) & ~(3 << shift);
	if (en)
		val |= x << shift;
	writel(val, vsmooth);
	if (debug)
		pr_info("%s filter %d en %d dst %d x %x shift %d"
			" vsmooth(%x) 0x%x\n\n", __func__, filter,
			en, dst, x, shift, vsmooth & 0xfff, val);
	return 0;
}

static int pxa688_colorkey_get(int id, int vid)
{
	struct pxa168fb_info *fbi = gfx_info.fbi[0];
	u32 base = (u32)fbi->reg_base, tmp = dma_ctrl_read(1, 1), en = 0;

	if (id == 0)
		en = vid ? ((tmp & (1 << 18)) >> 18) :
			((tmp & (1 << 19)) >> 19);
	else if (id == 1)
		en = vid ? ((tmp & (1 << 20)) >> 20) :
			((tmp & (1 << 21)) >> 21);
	else {
		tmp = __raw_readl(base + PN2_IOPAD_CONTROL);
		en = vid ? ((tmp & (1 << 5)) >> 5) :
			((tmp & (1 << 4)) >> 4);
	}

	return en;
}

static void pxa688_colorkey_set(int id, int vid, int en)
{
	struct pxa168fb_info *fbi = gfx_info.fbi[0];
	u32 base = (u32)fbi->reg_base, mask;

	if (id <= 1) {
		mask = id ? (vid ? (en << 20) : (en << 21)) :
			(vid ? (en << 18) : (en << 19));
		dma_ctrl_set(1, 1, mask, mask);
	} else {
		mask = __raw_readl(base + PN2_IOPAD_CONTROL);
		mask &= ~(vid ? (1 << 5) : (1 << 4));
		mask |= vid ? (en << 5) : (en << 4);
		__raw_writel(mask, base + PN2_IOPAD_CONTROL);
	}
}

/* pxa688_clone_xxx(int src, int dst,..)
 * These functions clone src path settings to dst path.
 * e.g. if use PN2 path DMA to do TV path smoothing, so
 * need to clone TV path settings to PN2.
 */

static int pxa688fb_clone_clk(int src, int dst)
{
	u32 mask = ~0;
	/* video clk shift of register LCD_PN2_TCLK_DIV,
	 * the shift is different according to MMP2/MMP3 spec,
	 * default setting is specific for MMP2.
	*/
	u32 pn2_vid_shift = 30;
	u32 pn2_gfx_shift = 2;

	if (src == 1 || dst == 1)
		/* TV path TCLK_DIV definitions different vs SCLK_DIV */
		mask = 0xd000000f;
	/* enable dst path clock */
	if (dst == 2 && src <= 1) {
		/* pn2 TCLK_DIV */
		mask = src ? 2 : 1;
#ifdef CONFIG_CPU_MMP3
		pn2_vid_shift = 29;
#endif
		lcd_clk_set(dst, clk_tclk, (mask << pn2_vid_shift) | (mask << pn2_gfx_shift) | (mask),
				(mask << pn2_vid_shift) | (mask << pn2_gfx_shift) | (mask));
	} else
		lcd_clk_set(dst, clk_sclk, lcd_clk_get(src, clk_sclk) & mask,
			lcd_clk_get(src, clk_sclk) & mask);

	return 0;
}

static void pxa688fb_clone_intf_ctrl(int src, int dst)
{
	struct pxa168fb_info *fbi = gfx_info.fbi[0];
	u32 base = (u32)fbi->reg_base, reg = 0, mask = 0;

	reg = readl(base + intf_ctrl(src));
	switch (dst) {
	case 0:
		if (src == 1) {
			/* tv -> pn */
			mask = reg & 0xf00009ff;
			if (reg & (1 << 15))
				mask |= (1 << 9);
		} else
			/* pn2 -> pn */
			mask = reg;
		break;
	case 1:
		if (src == 0 || src == 2) {
			/* pn -> tv or pn2 -> tv*/
			mask = reg & 0xf00009ff;
			if (reg & (1 << 9))
				mask |= (1 << 15);
		} else
			mask = reg;
		break;
	case 2:
		if (src == 1) {
			/* tv -> pn2 */
			mask = reg & 0xf00009ff;
			if (reg & (1 << 15))
				mask |= (1 << 9);
		} else
			/* pn -> pn2 */
			mask = reg;
		break;
	default:
		break;
	}
	writel(mask, base + intf_ctrl(dst));
}

static void pxa688fb_clone_vdma(int src, int dst, int vid)
{
#ifdef CONFIG_PXA688_VDMA
	struct pxa168fb_info *fbi = gfx_info.fbi[0];
	u32 base = (u32)fbi->reg_base, mask, vdma;
	struct pxa168fb_vdma_info *lcd_vdma = 0;

	mask = readl(base + LCD_PN2_SQULN2_CTRL);
	vdma = readl(base + squln_ctrl(src));
	lcd_vdma = request_vdma(src, vid);
	if (lcd_vdma && lcd_vdma->enable) {
		if (!lcd_vdma->vid) {
			/* vdma for graphic layer */
			mask &= ~(dst ? ((dst & 1) ? (1 << 25) :
				(1 << 26)) : (1 << 24));
		} else {
			/* vdma for video layer */
			mask |= dst ? ((dst & 1) ? (1 << 25) :
				(1 << 26)) : (1 << 24);
		}
	}
	writel(vdma, base + squln_ctrl(dst));
	writel(mask, base + LCD_PN2_SQULN2_CTRL);
#endif
}

static void pxa688fb_clone_partdisp_ctrl(int src, int dst)
{
	struct pxa168fb_info *fbi = gfx_info.fbi[src];
	struct fb_info *info = fbi->fb_info;
	struct fb_var_screeninfo *var = &info->var;

	u32 base = (u32)fbi->reg_base, mask, region, bytespp,
		horpix_end_src, horpix_end_dst, horpix_start,
		threshold_src, threshold_dst;

	mask = readl(base + gra_partdisp_ctrl_hor(src));
	bytespp = var->bits_per_pixel >> 3;
	horpix_end_src = (mask & 0xfff0000) >> 16;
	horpix_start = mask & 0xfff;

	/* THRESHOLD_x: the least bytes to operate for
	 * horizontal partial display
	 */
	threshold_src = (src == 1 ? THRESHOLD_TV : THRESHOLD_PN);
	threshold_dst = (dst == 1 ? THRESHOLD_TV : THRESHOLD_PN);
	if (horpix_end_src > horpix_start) {
		region = horpix_end_src - horpix_start;

		/* BURST_LEN: AXI burst size, platform dependent */
		region = (region - threshold_src / bytespp) /
			(BURST_LEN / bytespp);
		horpix_end_dst = horpix_start + threshold_dst / bytespp +
			region * (BURST_LEN / bytespp);
		mask &= ~0xfff0000;
		mask |= horpix_end_dst << 16;
	}
	writel(mask, base + gra_partdisp_ctrl_hor(dst));
	writel(readl(base + gra_partdisp_ctrl_ver(src)),
		base + gra_partdisp_ctrl_ver(dst));
}

static void pxa688fb_clone_base(int src, int dst, int vid)
{
	struct lcd_regs *regs_src = get_regs(src);
	struct lcd_regs *regs_dst = get_regs(dst);
	struct pxa168fb_info *fbi = gfx_info.fbi[0];
	u32 base = (u32)fbi->reg_base, mask, cokey_en;

	/* screen info */
	writel(readl(&regs_src->screen_size), &regs_dst->screen_size);
	writel(readl(&regs_src->screen_active), &regs_dst->screen_active);
	writel(readl(&regs_src->screen_h_porch), &regs_dst->screen_h_porch);
	writel(readl(&regs_src->screen_v_porch), &regs_dst->screen_v_porch);
	writel(readl(&regs_src->vsync_ctrl), &regs_dst->vsync_ctrl);

	/* dma control1 */
	mask = ~0;
	if ((src == 1) || (dst ==  1))
		mask &= ~((0x1f << 18) | 0xff);
	dma_ctrl_set(dst, 1, mask, dma_ctrl_read(src, 1) & mask);

	/* dma control0 */
	mask = ~(vid ? dma0_gfx_masks : dma0_vid_masks);
	if ((src == 1) || (dst ==  1))
		mask &= ~(1 << 27);
	dma_ctrl_set(dst, 0, mask, dma_ctrl_read(src, 0) & mask);

	/* DMA burst length */
	if (dst == 2)
		writel(readl(base + PN2_IOPAD_CONTROL) |
			(3 << 10) | (3 << 8), base + PN2_IOPAD_CONTROL);

	/* DMA color key */
	cokey_en = pxa688_colorkey_get(src, vid);
	pxa688_colorkey_set(dst, vid, cokey_en);

	/* intf ctrl */
	pxa688fb_clone_intf_ctrl(src, dst);
}

/* clone src path graphics layer settings to dst path */
static int pxa688fb_clone_gfx(int src, int dst, int en)
{
	struct lcd_regs *regs_src = get_regs(src);
	struct lcd_regs *regs_dst = get_regs(dst);
	struct mvdisp_partdisp grap;

	/* enable dst path clock */
	pxa688fb_clone_clk(src, dst);

	/* if disable vsmooth, disable dst path dma directly */
	if (!en) {
		dma_ctrl_set(dst, 0, CFG_GRA_ENA_MASK, 0);
		if (debug)
			pr_info("%s disabled: dma_ctrl0 0x%x\n",
				__func__, dma_ctrl_read(dst, 0));
		grap.id = dst;
		grap.horpix_start = grap.horpix_end = 0;
		grap.vertline_start = grap.vertline_end = 0;
		grap.color = 0;
		/* disable dst partial display */
		pxa688fb_partdisp_set(grap);
		return 0;
	}

	/* configure frame address */
	writel(readl(&regs_src->g_0), &regs_dst->g_0);
	writel(readl(&regs_src->g_1), &regs_dst->g_1);

	/* partial display */
	pxa688fb_clone_partdisp_ctrl(src, dst);

	/* configure dst regs */
	writel(readl(&regs_src->g_pitch), &regs_dst->g_pitch);
	writel(readl(&regs_src->g_start), &regs_dst->g_start);
	writel(readl(&regs_src->g_size), &regs_dst->g_size);
	writel(readl(&regs_src->g_size_z), &regs_dst->g_size_z);
	/* configure dma control0/1, screen info, color key, intf ctrl.. */
	pxa688fb_clone_base(src, dst, 0);
	return 0;
}

/* clone src path video layer settings to dst path */
static int pxa688fb_clone_ovly(int src, int dst, int en)
{
	struct lcd_regs *regs_src = get_regs(src);
	struct lcd_regs *regs_dst = get_regs(dst);

	/* enable dst path clock */
	pxa688fb_clone_clk(src, dst);

	/* if disable vsmooth, disable dst path dma directly */
	if (!en) {
		dma_ctrl_set(dst, 0, CFG_DMA_ENA_MASK, 0);
		if (debug)
			pr_info("%s disable %d: dma_ctrl0 0x%x\n",
				__func__, dst, dma_ctrl_read(dst, 0));
		return 0;
	}


	/* configure dst regs */
	writel(readl(&regs_src->v_y0), &regs_dst->v_y0);
	writel(readl(&regs_src->v_u0), &regs_dst->v_u0);
	writel(readl(&regs_src->v_v0), &regs_dst->v_v0);
	writel(readl(&regs_src->v_c0), &regs_dst->v_c0);
	writel(readl(&regs_src->v_y1), &regs_dst->v_y1);
	writel(readl(&regs_src->v_u1), &regs_dst->v_u1);
	writel(readl(&regs_src->v_v1), &regs_dst->v_v1);
	writel(readl(&regs_src->v_c1), &regs_dst->v_c1);
	writel(readl(&regs_src->v_pitch_yc), &regs_dst->v_pitch_yc);
	writel(readl(&regs_src->v_pitch_uv), &regs_dst->v_pitch_uv);
	writel(readl(&regs_src->v_start), &regs_dst->v_start);
	writel(readl(&regs_src->v_size), &regs_dst->v_size);
	writel(readl(&regs_src->v_size_z), &regs_dst->v_size_z);
	/* configure dma control0/1, screen info, color key, intf ctrl.. */
	pxa688fb_clone_base(src, dst, 1);
	return 0;
}

static int pxa168fb_vsmooth_check(int id, int src, int dst, int vid, int en)
{
	struct lcd_regs *regs;
	int x, x_z;

	if (id != fb_vsmooth) {
		if (debug)
			pr_info("%s: fbi %d != fb_vsmooth %d\n",
				__func__, id, fb_vsmooth);
		return -EINVAL;
	}

	if (src == dst || src < 0 || src > 2 || dst < 0 || dst > 1) {
		if (debug)
			pr_info("%s input err: src %d dst %d vid %d en %d\n",
				__func__, src, dst, vid, en);
		return -EINVAL;
	}

	regs = get_regs(id);
	x = (readl(vid ? &regs->v_size : &regs->g_size) >> 16) & 0xfff;
	x_z = (readl(vid ? &regs->v_size_z : &regs->g_size_z) >> 16) & 0xfff;
	if (debug)
		pr_info("%s layer %s: x 0x%x x_z 0x%x\n",
			__func__, vid ? "vid " : "gfx", x, x_z);
	return (x_z > x) ? 0 : 1;
}

/* pxa688fb_vsmooth_set
 * vid: video layer or graphics layer
 * en: enable vsmooth mode or not
 */
int pxa688fb_vsmooth_set(int id, int vid, int en)
{
	int filter = fb_filter, dst = fb_vsmooth, ret = 0;

	ret = pxa168fb_vsmooth_check(id, filter, dst, vid, en);
	if (ret) {
		if (ret == 1)
			/* not scaling, disable mapping and filter path dma */
			en = 0;
		else
			return -EINVAL;
	}

	if (vid)
		ret = pxa688fb_clone_ovly(dst, filter, en);
	else
		ret = pxa688fb_clone_gfx(dst, filter, en);
	if (ret) {
		if (debug)
			pr_info("%s clone %s err, filter %d dst %d\n",
				__func__, vid ? "ovly" : "gfx", filter, dst);
		return -EIO;
	}

	/* vdma clone */
	pxa688fb_clone_vdma(dst, filter, vid);

	pxa688fb_map_layers(filter, dst, vid, en);
	pxa688fb_vsmooth_config(filter, dst, vid, en);
	return 0;
}

/* gamma correction related functions */
#define mmpdisp_regbase		((u32)gfx_info.fbi[0]->reg_base)
#define sram_ctrl		(mmpdisp_regbase + LCD_SPU_SRAM_CTRL)
#define sram_wrdat		(mmpdisp_regbase + LCD_SPU_SRAM_WRDAT)
#define sram_para1		(mmpdisp_regbase + LCD_SPU_SRAM_PARA1)
#define gamma_rddat(path)	(mmpdisp_regbase + (((path) & 1) ? \
				LCD_TV_GAMMA_RDDAT : LCD_SPU_GAMMA_RDDAT))
#define gamma_id_yr(path)	((path) ? (((path) & 1) ? 0x4 : 0x9) : 0x0)
#define gamma_id_ug(path)	((path) ? (((path) & 1) ? 0x5 : 0xa) : 0x1)
#define gamma_id_vb(path)	((path) ? (((path) & 1) ? 0x6 : 0xb) : 0x2)
static u32 gamma_read(u32 addr, int gamma_id, int path)
{
	int count = 10000, val, pn2 = (path == 2) ? (1 << 16) : 0;

	val = pn2 | (0x0 << 12) | (gamma_id << 8) | addr;
	__raw_writel(val, sram_ctrl);
	while (__raw_readl(sram_ctrl) & (1<<31) && count--);

	if (count > 0)
		val = __raw_readl(gamma_rddat(path)) & CFG_GAMMA_RDDAT_MASK;
	else
		val = -1;

	return val;
}

static void gamma_write(u32 addr, u32 gamma_id, u32 val)
{
	__raw_writel(val, sram_wrdat);
	val = (0x8 << 12) | (gamma_id << 8 ) | addr;
	__raw_writel(val, sram_ctrl);
}

void gamma_dump(int path, int lines)
{
	u32 i = 0, val;

	if (!(dma_ctrl_read(path, 0) & CFG_GAMMA_ENA_MASK)) {
		pr_info("gamma correction not enabled yet\n");
	}

	/* enable gamma correction table update */
	val = __raw_readl(sram_para1) | CFG_CSB_256x8_MASK;
	__raw_writel(val, sram_para1);

	for (; i < lines; i++)
		pr_info("%3d: yr %3d, ug %3d, vb %3d\n", i,
			gamma_read(i, gamma_id_yr(path), path),
			gamma_read(i, gamma_id_ug(path), path),
			gamma_read(i, gamma_id_vb(path), path));

	val = __raw_readl(sram_para1) & ~CFG_CSB_256x8_MASK;
	__raw_writel(val, sram_para1);
}

int gamma_set(int path, int flag, char *gamma_table)
{
	u32 i = 0, val;

	/* disable gamma correction */
	dma_ctrl_set(path, 0, CFG_GAMMA_ENA_MASK, CFG_GAMMA_ENA(0));

	if (!(flag & GAMMA_ENABLE))
		goto dump;

	/* check as only 2 gamma correction table avialable */
	if (((path == 2) && (CFG_GAMMA_ENA(1) &
			dma_ctrl_read(0, 0) & dma_ctrl_read(1, 0))) ||
	    ((path == 1) && (CFG_GAMMA_ENA(1) &
			dma_ctrl_read(2, 0) & dma_ctrl_read(0, 0))) ||
	    ((path == 0) && (CFG_GAMMA_ENA(1) &
			dma_ctrl_read(1, 0) & dma_ctrl_read(2, 0)))) {
		pr_err("path %d gamma correction not avialable, pls "
			"disable other path's and try again\n", path);
		return -EINVAL;
	}

	/* enable gamma correction table update */
	val = __raw_readl(sram_para1) | CFG_CSB_256x8_MASK;
	__raw_writel(val, sram_para1);

	/* write gamma corrrection table */
	for (; i < GAMMA_TABLE_LEN; i++) {
		gamma_write(i, gamma_id_yr(path), gamma_table[i]);
		gamma_write(i, gamma_id_ug(path), gamma_table[i]);
		gamma_write(i, gamma_id_vb(path), gamma_table[i]);
	}

	val = __raw_readl(sram_para1) & ~CFG_CSB_256x8_MASK;
	__raw_writel(val, sram_para1);

	/* enable gamma correction table */
	dma_ctrl_set(path, 0, CFG_GAMMA_ENA_MASK, CFG_GAMMA_ENA(1));

dump:
	if (flag & GAMMA_DUMP)
		gamma_dump(path, GAMMA_TABLE_LEN);

	return 0;
}

static void dither_dump(struct pxa168fb_info *fbi)
{
	u32 base = (u32)fbi->reg_base;
	u32 mask = readl(base + LCD_DITHER_CTRL);
	int enabled, mode, table;

	enabled = mask & (fbi->id ? DITHER_EN2 : DITHER_EN1);
	if (!enabled)
		pr_info("fbi%d dither was disabled\n", fbi->id);
	else {
		mode = mask & (fbi->id ? DITHER_MODE2(7) : DITHER_MODE1(7));
		mode = mode >> (fbi->id ? DITHER_MODE2_SHIFT :
			DITHER_MODE1_SHIFT);
		table = mask & (fbi->id ? DITHER_4X8_EN2 : DITHER_4X8_EN1);
		table = table >> (fbi->id ? DITHER_4X8_EN2_SHIFT :
			DITHER_4X8_EN1_SHIFT);

		pr_info("fbi%d dither mode:%d, table:%d\n",
			fbi->id, mode, table);
		mask &= ~DITHER_TBL_INDEX_SEL(3);
		if (!table) {
			writel(mask, base + LCD_DITHER_CTRL);
			pr_info("4x4table index0:%x",
				readl(base + LCD_DITHER_TBL_DATA));
			writel(mask | DITHER_TBL_INDEX_SEL(1),
				base + LCD_DITHER_CTRL);
			pr_info("4x4table index1:%x",
				readl(base + LCD_DITHER_TBL_DATA));
		} else {
			writel(mask, base + LCD_DITHER_CTRL);
			pr_info("4x8table index0:%x",
				readl(base + LCD_DITHER_TBL_DATA));
			writel(mask | DITHER_TBL_INDEX_SEL(1),
				base + LCD_DITHER_CTRL);
			pr_info("4x8table index1:%x",
				readl(base + LCD_DITHER_TBL_DATA));
			writel(mask | DITHER_TBL_INDEX_SEL(2),
				base + LCD_DITHER_CTRL);
			pr_info("4x8table index2:%x",
				readl(base + LCD_DITHER_TBL_DATA));
			writel(mask | DITHER_TBL_INDEX_SEL(3),
				base + LCD_DITHER_CTRL);
			pr_info("4x8table index3:%x",
				readl(base + LCD_DITHER_TBL_DATA));
		}
	}
}

void dither_set(struct pxa168fb_info *fbi, int table, int mode, int enable)
{
	u32 base = (u32)fbi->reg_base;
	u32 mask = readl(base + LCD_DITHER_CTRL);

	if (fbi->id && fbi->id != 2) {
		pr_err("%s fbi:%d dither not support\n", __func__, fbi->id);
		return;
	}

	if (!enable) {
		mask &= ~(fbi->id ? DITHER_EN2 : DITHER_EN1);
		writel(mask, base + LCD_DITHER_CTRL);
		goto dump;
	}

	if (!fbi->id) {
		mask = table ? DITHER_4X8_EN1 : 0;
		mask |= DITHER_MODE1(mode);
		mask |= DITHER_EN1;
	} else {
		mask = table ? DITHER_4X8_EN2 : 0;
		mask |= DITHER_MODE2(mode);
		mask |= DITHER_EN2;
	}

	if (!table) {
		/* 4X4 table */
		writel(mask, base + LCD_DITHER_CTRL);
		writel(DITHER_TB_4X4_INDEX0, base + LCD_DITHER_TBL_DATA);
		writel(mask | DITHER_TBL_INDEX_SEL(1), base + LCD_DITHER_CTRL);
		writel(DITHER_TB_4X4_INDEX1, base + LCD_DITHER_TBL_DATA);
	} else {
		/* 4X8 table */
		writel(mask, base + LCD_DITHER_CTRL);
		writel(DITHER_TB_4X8_INDEX0, base + LCD_DITHER_TBL_DATA);
		writel(mask | DITHER_TBL_INDEX_SEL(1), base + LCD_DITHER_CTRL);
		writel(DITHER_TB_4X8_INDEX1, base + LCD_DITHER_TBL_DATA);
		writel(mask | DITHER_TBL_INDEX_SEL(2), base + LCD_DITHER_CTRL);
		writel(DITHER_TB_4X8_INDEX2, base + LCD_DITHER_TBL_DATA);
		writel(mask | DITHER_TBL_INDEX_SEL(3), base + LCD_DITHER_CTRL);
		writel(DITHER_TB_4X8_INDEX3, base + LCD_DITHER_TBL_DATA);
	}

dump:
	if (debug)
		dither_dump(fbi);
}

static ssize_t misc_help(char *buf)
{
	int s = 0, f = DUMP_SPRINTF;

	mvdisp_dump(f, "commands:\n");
	mvdisp_dump(f, " - dump partial display and vertical"
			" smooth settings\n");
	mvdisp_dump(f, "\tcat misc\n");
	mvdisp_dump(f, " - select path(pn/tv/pn2:0/1/2]) to be"
			" vertical smoothed\n");
	mvdisp_dump(f, "\techo s[path:0/1/2] > misc\n");
	mvdisp_dump(f, " - select path(pn/tv/pn2:0/1/2]) to work as filter"
			" when vertical smooth enabled\n");
	mvdisp_dump(f, "\techo f[path:0/1/2] > misc\n");
	mvdisp_dump(f, " - graphics layer vertical vsmooth"
			" enable[1]/disable[0]\n");
	mvdisp_dump(f, "\techo g[en/dis:1/0] > misc\n");
	mvdisp_dump(f, " - video layer vertical vsmooth"
			" enable[1]/disable[0]\n");
	mvdisp_dump(f, "\techo v[en/dis:1/0] > misc\n");
	mvdisp_dump(f, " - vertical smooth kernel debug"
			" enable[1]/disable[0]\n");
	mvdisp_dump(f, "\techo d[en/dis:1/0] > misc\n");
	mvdisp_dump(f, " - set graphics layer partial display area from pixel"
			" [h_start][v_start]\n   to pixel [h_end][v_end]"
			"  with RGB565 format [color]\n");
	mvdisp_dump(f, "\techo p [h_start] [v_start] [h_end] [v_end]"
			" [color] > misc\n");
	mvdisp_dump(f, " - dither setting\n");
	mvdisp_dump(f, "\techo i [4x4/4x8 table: 0/1]"
		" [RBG444/RGB565/RGB666 mode: 0/1/2] [en/dis:1/0] > misc\n");

	return s;
}

ssize_t misc_show(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	struct pxa168fb_info *fbi = dev_get_drvdata(dev);
	struct mvdisp_partdisp grap;
	u32 mask;
	int s = 0;

	grap.id = fbi->id;
again:
	mask = readl((u32)fbi->reg_base + gra_partdisp_ctrl_hor(grap.id));
	/* get horizontal start/end pixel number */
	grap.horpix_start = mask & 0xfff;
	grap.horpix_end = (mask & 0xfff0000) >> 16;
	/* get color bit 0~7 */
	grap.color = (mask & 0xf000) >> 12;
	grap.color |= ((mask & 0xf0000000) >> 28) << 4;

	mask = readl((u32)fbi->reg_base + gra_partdisp_ctrl_ver(grap.id));
	/* get vertical start/end line */
	grap.vertline_start = mask & 0xfff;
	grap.vertline_end = (mask & 0xfff0000) >> 16;
	/* get color bit 8~15 */
	grap.color |= ((mask & 0xf000) >> 12) << 8;
	grap.color |= ((mask & 0xf0000000) >> 28) << 12;

	s += sprintf(buf + s, "partial display:\n  path %d, h_start %d,"
		"v_start %d, h_end %d, v_end %d, color %d\n\n",
		grap.id, grap.horpix_start, grap.vertline_start,
		grap.horpix_end, grap.vertline_end, grap.color);

	if (gfx_vsmooth && (fb_filter != fb_vsmooth)
		&& (grap.id == fb_vsmooth)) {
		grap.id = fb_filter;
		goto again;
	}

	s += sprintf(buf + s, "vertical smooth:\n  filter(%d)->vsmooth(%d),"
		" gfx %d, vid %d, debug %d\n\n", fb_filter, fb_vsmooth,
		gfx_vsmooth, vid_vsmooth, debug);

	s += misc_help(buf + s);
	return s;
}
ssize_t misc_store(
		struct device *dev, struct device_attribute *attr,
		const char *buf, size_t size)
{
	struct pxa168fb_info *fbi = dev_get_drvdata(dev);
	struct mvdisp_partdisp grap;
	char vol[30];
	int tmp, table, mode, enable;

	if (size > 30) {
		pr_err("%s size = %d > max 30 chars\n", __func__, size);
		return size;
	}
	if ('s' == buf[0]) {
		memcpy(vol, (void *)((u32)buf + 1), size - 1);
		tmp = (int) simple_strtoul(vol, NULL, 10);
		if (tmp != fb_vsmooth) {
			/* disable vsmooth for original path */
			pxa688fb_vsmooth_set(fb_vsmooth, 0, 0);
			pxa688fb_vsmooth_set(fb_vsmooth, 1, 0);
			/* enable vsmooth for new path */
			fb_vsmooth = tmp;
			pxa688fb_vsmooth_set(fb_vsmooth, 0, gfx_vsmooth);
			pxa688fb_vsmooth_set(fb_vsmooth, 1, vid_vsmooth);
			pr_info("fb_vsmooth: %d\n", fb_vsmooth);
		}
		return size;
	} else if ('f' == buf[0]) {
		memcpy(vol, (void *)((u32)buf + 1), size - 1);
		tmp = (int) simple_strtoul(vol, NULL, 10);
		if (tmp != fb_filter) {
			/* disable vsmooth for original path */
			pxa688fb_vsmooth_set(fb_vsmooth, 0, 0);
			pxa688fb_vsmooth_set(fb_vsmooth, 1, 0);
			/* enable vsmooth for new path */
			fb_filter = tmp;
			pxa688fb_vsmooth_set(fb_vsmooth, 0, gfx_vsmooth);
			pxa688fb_vsmooth_set(fb_vsmooth, 1, vid_vsmooth);
			pr_info("fb_filter: %d\n", fb_filter);
		}
		return size;
	} else if ('g' == buf[0]) {
		memcpy(vol, (void *)((u32)buf + 1), size - 1);
		tmp = gfx_vsmooth;
		gfx_vsmooth = (int) simple_strtoul(vol, NULL, 10);
		if (tmp != gfx_vsmooth) {
			pxa688fb_vsmooth_set(fb_vsmooth, 0, gfx_vsmooth);
			pr_info("gfx_vsmooth: %d -> %d\n", tmp, gfx_vsmooth);
		}
		return size;
	} else if ('v' == buf[0]) {
		memcpy(vol, (void *)((u32)buf + 1), size - 1);
		tmp = vid_vsmooth;
		vid_vsmooth = (int) simple_strtoul(vol, NULL, 10);
		if (tmp != vid_vsmooth) {
			pxa688fb_vsmooth_set(fb_vsmooth, 1, vid_vsmooth);
			pr_info("vid_vsmooth: %d -> %d\n", tmp, vid_vsmooth);
		}
		return size;
	} else if ('d' == buf[0]) {
		memcpy(vol, (void *)((u32)buf + 1), size - 1);
		debug = (int) simple_strtoul(vol, NULL, 10);
		pr_info("debug: %d\n", debug);
		return size;
	} else if ('p' == buf[0]) {
		memcpy(vol, (void *)((u32)buf + 1), size - 1);
		if (sscanf(vol, "%u %u %u %u %hu", &grap.horpix_start,
			&grap.vertline_start, &grap.horpix_end,
			&grap.vertline_end, &grap.color) != 5) {
			pr_err("partial display cmd should be like: "
				"p horpix_start vertline_start "
				"horpix_end verline_end color\n");
			return size;
		}
		grap.id = fbi->id;
		pxa688fb_partdisp_set(grap);
		if ((grap.id == fb_vsmooth) && (gfx_vsmooth)) {
			if (dma_ctrl_read(fb_filter, 0) & CFG_GRA_ENA_MASK) {
				grap.id = fb_filter;
				pxa688fb_partdisp_set(grap);
			}
		}
		pr_info("lcd_part_disp\n");
	} else if ('i' == buf[0]) {
		memcpy(vol, (void *)((u32)buf + 1), size - 1);
		if (sscanf(vol, "%u %u %u", &table, &mode, &enable) != 3) {
			pr_err("dithering cmd should be:"
				"i table mode enable\n");
			return size;
		}
		dither_set(fbi, table, mode, enable);
		pr_info("dither setting\n");
	} else
		pr_err("%s unknown command %s\n", __func__, buf);

	return size;
}
DEVICE_ATTR(misc, S_IRUGO | S_IWUSR, misc_show, misc_store);
