/*
 * linux/drivers/video/mmp/hw/mmp_ctrl.c
 * Marvell MMP series Display Controller support
 *
 * Copyright (C) 2012 Marvell Technology Group Ltd.
 * Authors:  Guoqing Li <ligq@marvell.com>
 *          Lisa Du <cldu@marvell.com>
 *          Zhou Zhu <zzhu3@marvell.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/dma-mapping.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/vmalloc.h>
#include <linux/uaccess.h>
#include <linux/kthread.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/pm_runtime.h>
#include <linux/clk-private.h>

#include "mmp_ctrl.h"
#define CREATE_TRACE_POINTS
#include <video/mmp_trace.h>

static BLOCKING_NOTIFIER_HEAD(mmp_panel_notifier_list);

static int path_ctrl_safe(struct mmp_path *path);
int mmp_notifier(unsigned long state, void *val)
{
	return blocking_notifier_call_chain(&mmp_panel_notifier_list, state, val);
}

int mmp_register_notifier(struct notifier_block *nb)
{
	return blocking_notifier_chain_register(&mmp_panel_notifier_list, nb);
}

static void path_vsync_sync(struct mmp_path *path)
{
	struct mmp_path *slave = path->slave;
	u32 dma_ctrl1;

	if (slave) {
		/* If slave path use the same CLK with master path,
		 * sync the timing
		 */
		dma_ctrl1 = readl_relaxed(ctrl_regs(slave) +
			dma_ctrl(1, slave->id));
		dma_ctrl1 &= ~CFG_TMSYNC_ENA_MASK;
		dma_ctrl1 |= CFG_TMSYNC_ENA(1);
		writel_relaxed(dma_ctrl1, ctrl_regs(slave) +
			dma_ctrl(1, slave->id));
		dma_ctrl1 &= ~CFG_TMSYNC_ENA_MASK;
		writel_relaxed(dma_ctrl1, ctrl_regs(slave) +
			dma_ctrl(1, slave->id));
	}
}

static void path_hw_trigger(struct mmp_path *path)
{
	struct mmp_overlay *overlay;
	struct mmp_vdma_info *vdma;
	int i, vsync_enable = 0;
	int to_trigger_vdma = 0;
	u32 tmp;
	unsigned long flags;

	if (DISP_GEN4(path_to_ctrl(path)->version)) {
		tmp = readl_relaxed(ctrl_regs(path) + LCD_SHADOW_CTRL) |
			SHADOW_TRIG(path->id);
		if (!(DISP_GEN4_LITE(path_to_ctrl(path)->version) ||
			DISP_GEN4_PLUS(path_to_ctrl(path)->version)))
			writel_relaxed(tmp, ctrl_regs(path) + LCD_SHADOW_CTRL);

		for (i = 0; i < path->overlay_num; i++) {
			overlay = &path->overlays[i];
			vdma = overlay->vdma;
			if (vdma) {
				spin_lock_irqsave(&vdma->status_lock, flags);
				if (vdma->status == VDMA_TO_DISABLE)
					/* For this status, vdma channel
					 * was disabled, but shadow registers
					 * not triggered, it will be triggered
					 * later and then vdma channel clock
					 * will be disabled in following
					 * display done irq handler */
					vsync_enable = 1;

				if (vdma->status >= VDMA_ON && vdma->status < VDMA_DISABLED)
					/*
					 * Only enable VDMA channel configure
					 * registers after it is on and before
					 * it is disabeld
					 */
					to_trigger_vdma = 1;
				spin_unlock_irqrestore(&vdma->status_lock,
						flags);
				if (to_trigger_vdma && vdma->ops && vdma->ops->trigger)
					vdma->ops->trigger(vdma);
				if (vsync_enable &&
						!DISP_GEN4_LITE(path_to_ctrl(path)->version))
					/* Enable irq that vdma channel clock
					 * will be disabled in irq handler */
					if (!atomic_read(&path->irq_en_count)) {
						atomic_inc(&path->irq_en_count);
						mmp_path_set_irq(path, 1);
					}
			}
		}
	}
}

static void gamma_write(struct mmp_path *path, u32 addr, u32 gamma_id, u32 val)
{
	writel(val, ctrl_regs(path) + LCD_SPU_SRAM_WRDAT);
	val = (0x8 << 12) | (gamma_id << 8) | addr;
	writel(val, ctrl_regs(path) + LCD_SPU_SRAM_CTRL);
}

static u32 gamma_read(struct mmp_path *path, u32 addr, int gamma_id, int id)
{
	int count = 10000, val, pn2 = (id == 2) ? (1 << 16) : 0;

	val = pn2 | (0x0 << 12) | (gamma_id << 8) | addr;
	writel(val, ctrl_regs(path) + LCD_SPU_SRAM_CTRL);
	while ((readl(ctrl_regs(path) + LCD_SPU_SRAM_CTRL) & (1<<31))
		&& count--)
		;

	if (count > 0)
		val = readl(ctrl_regs(path) + (((path->id) & 1) ?
				LCD_TV_GAMMA_RDDAT : LCD_SPU_GAMMA_RDDAT))
			& CFG_GAMMA_RDDAT_MASK;
	else
		val = -1;

	return val;
}

static void gamma_dump(struct mmp_path *path, int lines)
{
	u32 i = 0, val;
	int id = path->id;

	if (!(readl_relaxed(ctrl_regs(path) + dma_ctrl(0, path->id))
		& CFG_GAMMA_ENA_MASK))
		dev_info(path->dev, "gamma correction not enabled yet\n");

	/* enable gamma correction table update */
	val = readl_relaxed(ctrl_regs(path) + LCD_SPU_SRAM_PARA1)
			| CFG_CSB_256x8_MASK;
	writel(val, ctrl_regs(path) + LCD_SPU_SRAM_PARA1);

	for (; i < lines; i++)
		dev_info(path->dev, "%3d: yr %3d, ug %3d, vb %3d\n", i,
			gamma_read(path, i, gamma_id_yr(id), id),
			gamma_read(path, i, gamma_id_ug(id), id),
			gamma_read(path, i, gamma_id_vb(id), id));

	val = readl_relaxed(ctrl_regs(path) + LCD_SPU_SRAM_PARA1)
			& ~CFG_CSB_256x8_MASK;
	writel(val, ctrl_regs(path) + LCD_SPU_SRAM_PARA1);
}

static int path_set_gamma(struct mmp_path *path, int flag, char *gamma_table)
{
	u32 tmp, val, i;

	/* disable gamma correction */
	tmp = readl_relaxed(ctrl_regs(path) + dma_ctrl(0, path->id));
	tmp &= ~CFG_GAMMA_ENA_MASK;
	tmp |= CFG_GAMMA_ENA(0);
	writel(tmp, ctrl_regs(path) + dma_ctrl(0, path->id));

	if (!(flag & GAMMA_ENABLE))
		goto dump;

	/* enable gamma correction table update */
	val = readl_relaxed(ctrl_regs(path) + LCD_SPU_SRAM_PARA1)
			| CFG_CSB_256x8_MASK;
	writel(val, ctrl_regs(path) + LCD_SPU_SRAM_PARA1);

	/* write gamma corrrection table */
	for (i = 0; i < GAMMA_TABLE_LEN; i++) {
		gamma_write(path, i, gamma_id_yr(path->id), gamma_table[i]);
		gamma_write(path, i, gamma_id_ug(path->id), gamma_table[i]);
		gamma_write(path, i, gamma_id_vb(path->id), gamma_table[i]);
	}

	val = readl_relaxed(ctrl_regs(path) + LCD_SPU_SRAM_PARA1)
			& ~CFG_CSB_256x8_MASK;
	writel(val, ctrl_regs(path) + LCD_SPU_SRAM_PARA1);

	/* enable gamma correction table */
	tmp = readl_relaxed(ctrl_regs(path) + dma_ctrl(0, path->id));
	tmp &= ~CFG_GAMMA_ENA_MASK;
	tmp |= CFG_GAMMA_ENA(1);
	writel(tmp, ctrl_regs(path) + dma_ctrl(0, path->id));

dump:
	if (flag & GAMMA_DUMP)
		gamma_dump(path, GAMMA_TABLE_LEN);

	return 0;
}

static int is_rbswap(struct mmp_overlay *overlay)
{
	int fmt = overlay->win.pix_fmt;
	if (fmt == PIXFMT_RGB565
		|| fmt == PIXFMT_RGB1555
		|| fmt == PIXFMT_RGB888PACK
		|| fmt == PIXFMT_RGB888UNPACK
		|| fmt == PIXFMT_RGBA888)
		return 1;
	else
		return 0;
}

static int overlay_set_colorkey_alpha(struct mmp_overlay *overlay,
					struct mmp_colorkey_alpha *ca)
{
	struct mmp_path *path = overlay->path;
	struct lcd_regs *regs = path_regs(overlay->path);
	struct mmphw_ctrl *ctrl = overlay_to_ctrl(overlay);
	u32 rb, x, layer, dma0, shift, r, b;

	if (DISP_GEN4_LITE(ctrl->version)) {
		dev_info(ctrl->dev, "ERROR: DC4_LITE didn't support colorkey alpha\n");
		return -1;
	}

	dma0 = readl_relaxed(ctrl_regs(path) + dma_ctrl(0, path->id));
	shift = path->id ? 20 : 18;
	rb = layer = 0;
	r = ca->y_coloralpha;
	b = ca->v_coloralpha;

	/* reset to 0x0 to disable color key. */
	x = readl_relaxed(ctrl_regs(path) + dma_ctrl(1, path->id))
			& ~(CFG_COLOR_KEY_MASK | CFG_ALPHA_MODE_MASK
			| CFG_ALPHA_MASK);

	/* switch to color key mode */
	switch (ca->mode) {
	case FB_DISABLE_COLORKEY_MODE:
		/* do nothing */
		break;
	case FB_ENABLE_Y_COLORKEY_MODE:
		x |= CFG_COLOR_KEY_MODE(0x1);
		break;
	case FB_ENABLE_U_COLORKEY_MODE:
		x |= CFG_COLOR_KEY_MODE(0x2);
		break;
	case FB_ENABLE_V_COLORKEY_MODE:
		x |= CFG_COLOR_KEY_MODE(0x4);
		dev_info(ctrl->dev,
			"V colorkey not supported, Chroma key instead\n");
		break;
	case FB_ENABLE_RGB_COLORKEY_MODE:
		x |= CFG_COLOR_KEY_MODE(0x3);
		rb = 1;
		break;
	case FB_ENABLE_R_COLORKEY_MODE:
		x |= CFG_COLOR_KEY_MODE(0x1);
		rb = 1;
		break;
	case FB_ENABLE_G_COLORKEY_MODE:
		x |= CFG_COLOR_KEY_MODE(0x6);
		dev_info(ctrl->dev,
			"G colorkey not supported, Luma key instead\n");
		break;
	case FB_ENABLE_B_COLORKEY_MODE:
		x |= CFG_COLOR_KEY_MODE(0x7);
		rb = 1;
		break;
	default:
		dev_info(ctrl->dev, "unknown mode\n");
		return -1;
	}

	/* switch to alpha path selection */
	switch (ca->alphapath) {
	case FB_VID_PATH_ALPHA:
		x |= CFG_ALPHA_MODE(0x0);
		layer = CFG_CKEY_DMA;
		if (rb) {
			rb = ((dma0 & CFG_DMA_SWAPRB_MASK) >> 4) ^
				(is_rbswap(overlay));
		}
		break;
	case FB_GRA_PATH_ALPHA:
		x |= CFG_ALPHA_MODE(0x1);
		layer = CFG_CKEY_GRA;
		if (rb) {
			rb = ((dma0 & CFG_GRA_SWAPRB_MASK) >> 12) ^
				(is_rbswap(overlay));
		}
		break;
	case FB_CONFIG_ALPHA:
		x |= CFG_ALPHA_MODE(0x2);
		rb = 0;
		break;
	default:
		dev_info(ctrl->dev, "unknown alpha path");
		return -1;
	}

	/* check whether DMA turn on RB swap for this pixelformat. */
	if (rb) {
		if (ca->mode == FB_ENABLE_R_COLORKEY_MODE) {
			x &= ~CFG_COLOR_KEY_MODE(0x1);
			x |= CFG_COLOR_KEY_MODE(0x7);
		}

		if (ca->mode == FB_ENABLE_B_COLORKEY_MODE) {
			x &= ~CFG_COLOR_KEY_MODE(0x7);
			x |= CFG_COLOR_KEY_MODE(0x1);
		}

		/* exchange r b fields. */
		r = ca->v_coloralpha;
		b = ca->y_coloralpha;

		/* only alpha_Y take effect, switch back from V */
		if (ca->mode == FB_ENABLE_RGB_COLORKEY_MODE) {
			r &= 0xffffff00;
			r |= (ca->y_coloralpha & 0xff);
		}
	}

	/* configure alpha */
	x |= CFG_ALPHA((ca->config & 0xff));
	writel(x, ctrl_regs(path) + dma_ctrl(1, path->id));
	writel(r, &regs->v_colorkey_y);
	writel(ca->u_coloralpha, &regs->v_colorkey_u);
	writel(b, &regs->v_colorkey_v);

	if (DISP_GEN4(ctrl->version) && (!path->id)) {
		shift = 1;
		x = readl_relaxed(ctrl_regs(path) + dma_ctrl(2, path->id));
		x &= ~(3 << shift);
		x |= layer << shift;
		writel(x, ctrl_regs(path) + dma_ctrl(2, path->id));
	} else if (path->id != 2) {
		/*
		 * enable DMA colorkey on graphics/video layer
		 * in panel/TV path. On GEN4 TV path keeps the same setting.
		 */
		x = readl_relaxed(ctrl_regs(path) + LCD_TV_CTRL1);
		x &= ~(3 << shift);
		x |= layer << shift;
		writel(x, ctrl_regs(path) + LCD_TV_CTRL1);
	}

	return 0;
}

static int path_set_alpha(struct mmp_overlay *overlay,
					struct mmp_alpha *pa)
{
	struct mmp_path *path = overlay->path;
	u32 alpha_mode = 0x0, mask, val;

	if ((pa->alphapath & ALPHA_TV_GRA_AND_TV_VID) ||
		(pa->alphapath & ALPHA_PN_GRA_AND_PN_VID)) {
		if (pa->config & ALPHA_PATH_VID_PATH_ALPHA)
			alpha_mode = 0x0;
		else if (pa->config & ALPHA_PATH_GRA_PATH_ALPHA)
			alpha_mode = 0x1;
		val = readl_relaxed(ctrl_regs(path) + dma_ctrl(1, path->id));
		mask = CFG_ALPHA_MODE_MASK;
		val &= ~mask;
		val |= CFG_ALPHA_MODE(alpha_mode);
		writel(val, ctrl_regs(path) + dma_ctrl(1, path->id));
		val = readl_relaxed(ctrl_regs(path) + LCD_AFA_ALL2ONE);
		mask = CFG_OVTOP_MASK | CFG_OVNXT_MASK;
		val &= ~mask;
		if (pa->alphapath == ALPHA_PN_GRA_AND_PN_VID)
			val |= CFG_OVTOP_SEL(1) | CFG_OVNXT_SEL(0);
		else
			val |= CFG_OVTOP_SEL(3) | CFG_OVNXT_SEL(2);
		writel(val, ctrl_regs(path) + LCD_AFA_ALL2ONE);
	} else if (pa->alphapath & ALPHA_PN_GRA_AND_TV_GRA) {
		if (pa->config & ALPHA_PATH_VID_PATH_ALPHA)
			alpha_mode = 0x0;
		else if (pa->config & ALPHA_PATH_GRA_PATH_ALPHA)
			alpha_mode = 0x1;
		val = readl_relaxed(ctrl_regs(path) + LCD_AFA_ALL2ONE);
		mask = CFG_GRATVG_MASK | CFG_OVTOP_MASK
			| CFG_OVNXT_MASK;
		val &= ~mask;
		val |= CFG_GRATVG_AMOD(alpha_mode) | CFG_OVTOP_SEL(1)
			| CFG_OVNXT_SEL(3);
		writel(val, ctrl_regs(path) + LCD_AFA_ALL2ONE);
	} else if (pa->alphapath & ALPHA_PN_GRA_AND_TV_VID) {
		if (pa->config & ALPHA_PATH_VID_PATH_ALPHA)
			alpha_mode = 0x0;
		else if (pa->config & ALPHA_PATH_GRA_PATH_ALPHA)
			alpha_mode = 0x1;
		val = readl_relaxed(ctrl_regs(path) + LCD_AFA_ALL2ONE);
		mask = CFG_GRATVD_MASK | CFG_OVTOP_MASK
			| CFG_OVNXT_MASK;
		val &= ~mask;
		val |= CFG_GRATVD_AMOD(alpha_mode) | CFG_OVTOP_SEL(1)
			| CFG_OVNXT_SEL(2);
		writel(val, ctrl_regs(path) + LCD_AFA_ALL2ONE);
	} else if (pa->alphapath & ALPHA_PN_VID_AND_TV_GRA) {
		if (pa->config & ALPHA_PATH_PN_PATH_ALPHA)
			alpha_mode = 0x0;
		else if (pa->config & ALPHA_PATH_TV_PATH_ALPHA)
			alpha_mode = 0x1;
		val = readl_relaxed(ctrl_regs(path) + LCD_AFA_ALL2ONE);
		mask = CFG_DMATVG_MASK | CFG_OVTOP_MASK
			| CFG_OVNXT_MASK;
		val &= ~mask;
		val |= CFG_DMATVG_AMOD(alpha_mode) | CFG_OVTOP_SEL(0)
			| CFG_OVNXT_SEL(3);
		writel(val, ctrl_regs(path) + LCD_AFA_ALL2ONE);
	} else if (pa->alphapath & ALPHA_PN_VID_AND_TV_VID) {
		if (pa->config & ALPHA_PATH_PN_PATH_ALPHA)
			alpha_mode = 0x0;
		else if (pa->config & ALPHA_PATH_TV_PATH_ALPHA)
			alpha_mode = 0x1;
		val = readl_relaxed(ctrl_regs(path) + LCD_AFA_ALL2ONE);
		mask = CFG_DMATVD_MASK | CFG_OVTOP_MASK
			| CFG_OVNXT_MASK;
		val &= ~mask;
		val |= CFG_DMATVD_AMOD(alpha_mode) | CFG_OVTOP_SEL(0)
			| CFG_OVNXT_SEL(2);
		writel(val, ctrl_regs(path) + LCD_AFA_ALL2ONE);
	} else {
		dev_info(overlay_to_ctrl(overlay)->dev, "unknown alpha path");
		return -1;
	}
	return 0;
}

static int overlay_set_path_alpha(struct mmp_overlay *overlay,
					struct mmp_alpha *pa)
{
	struct mmp_shadow *shadow = overlay->shadow;
	struct mmp_path *path = overlay->path;
	struct mmphw_ctrl *ctrl = path_to_ctrl(path);

	if (DISP_GEN4_LITE(ctrl->version)) {
		pr_err("ERROR: DC4_LITE didn't support path alpha\n");
		return -1;
	}

	if (overlay->ops->trigger) {
		if (shadow && shadow->ops && shadow->ops->set_alpha)
			shadow->ops->set_alpha(shadow, pa);
	} else
		path_set_alpha(overlay, pa);

	return 0;
}

static int ctrl_irq_set(struct mmp_path *path, u32 mask, u32 set)
{
	u32 tmp1, tmp2;
	int retry = 0;

	tmp1 = readl_relaxed(ctrl_regs(path) + SPU_IRQ_ENA);
	tmp2 = tmp1;
	tmp1 &= ~mask;
	tmp1 |= set;
	if (tmp1 != tmp2)
		writel_relaxed(tmp1, ctrl_regs(path) + SPU_IRQ_ENA);

	/* FIXME: irq register write failure */
	while (tmp1 != readl(ctrl_regs(path) + SPU_IRQ_ENA)
		&& retry < 10000) {
		retry++;
		writel_relaxed(tmp1, ctrl_regs(path)
			+ SPU_IRQ_ENA);
	}
	return retry;
}

static int path_set_irq(struct mmp_path *path, int on)
{
	struct mmphw_ctrl *ctrl = path_to_ctrl(path);
	u32 tmp;
	u32 mask = display_done_imask(path->id) | vsync_imask(path->id) |
		gfx_udflow_imask(path->id) | vid_udflow_imask(path->id);
	int retry = 0;
	unsigned long flags, common_reg_flags;

	spin_lock_irqsave(&path->irq_lock, flags);

	/*
	 * It is possible to disable/enable irq anytime, enable HCLK for it.
	 * If the function is called in irq handler, we need not to enable clk
	 * because clk_prepare_enable will cal mutex_lock which is forbidden in
	 * irq handler.
	 */
	if (ctrl->clk && !in_irq())
		clk_prepare_enable(ctrl->clk);

	if (!on) {
		if (atomic_read(&path->irq_en_ref))
			if (atomic_dec_and_test(&path->irq_en_ref)) {
				spin_lock_irqsave(&ctrl->common_regs_lock, common_reg_flags);
				if (!path_ctrl_safe(path)) {
					/*
					 * if it is suspended, we can't access real register,
					 * should only change the stored value to let them be
					 * valid in resume.
					 */
					tmp = ctrl->regs_store[SPU_IRQ_ENA / 4];
					tmp &= ~mask;
					ctrl->regs_store[SPU_IRQ_ENA / 4] = tmp;
				} else {
					retry = ctrl_irq_set(path, mask, 0);
					if (retry == 10000) {
						pr_info("disable irq: write LCD IRQ failure\n");
						/*
						 * if disable irq failure, we should keep the
						 * irq_en_ref the same, so irq would be enabled
						 * always and try to disable irq.
						 */
						atomic_inc(&path->irq_en_ref);
					}
				}
				spin_unlock_irqrestore(&ctrl->common_regs_lock, common_reg_flags);
				dev_dbg(path->dev, "%s eof intr off\n", path->name);
			}
	} else {
		if (!atomic_read(&path->irq_en_ref)) {
			/*
			 * FIXME: clear the interrupt status.
			 * It would not trigger handler if the status on
			 * before enable.
			 */
			spin_lock_irqsave(&ctrl->common_regs_lock, common_reg_flags);
			if (!path_ctrl_safe(path)) {
				if (!DISP_GEN4(ctrl->version)) {
					tmp = ctrl->regs_store[SPU_IRQ_ISR / 4];
					tmp &= ~mask;
					ctrl->regs_store[SPU_IRQ_ISR / 4] = tmp;
				} else
					tmp = mask;
				ctrl->regs_store[SPU_IRQ_ISR / 4] = tmp;

				/*
				 * if it is suspended, we can't access real register,
				 * should only change the stored value to let them be
				 * valid in resume.
				 */
				tmp = ctrl->regs_store[SPU_IRQ_ENA / 4];
				tmp |= mask;
				ctrl->regs_store[SPU_IRQ_ENA / 4] = tmp;
			} else {
				if (!DISP_GEN4(ctrl->version)) {
					tmp = readl(ctrl_regs(path) + SPU_IRQ_ISR);
					tmp &= ~mask;
				} else
					tmp = mask;
				writel_relaxed(tmp, ctrl_regs(path) + SPU_IRQ_ISR);

				retry = ctrl_irq_set(path, 0, mask);
			}
			spin_unlock_irqrestore(&ctrl->common_regs_lock, common_reg_flags);
			dev_dbg(path->dev, "%s eof intr on\n", path->name);
		}
		/*
		 * if enable irq failure, we shouldn't irq_en_ref++,
		 * or there will be no chance to enable irq.
		 */
		if (retry == 10000)
			pr_info("enable irq: write LCD IRQ failure\n");
		else
			atomic_inc(&path->irq_en_ref);
	}

	if (ctrl->clk && !in_irq())
		clk_disable_unprepare(ctrl->clk);

	spin_unlock_irqrestore(&path->irq_lock, flags);

	return atomic_read(&path->irq_en_ref) > 0;
}

static void path_trigger(struct mmp_path *path)
{
	struct mmp_overlay *overlay;
	int i;

	/* Called in display(DMA) done interrupt,
	 * set shadow buffer to registers */
	for (i = 0; i < path->overlay_num; i++) {
		overlay = &path->overlays[i];
		if (overlay->ops->trigger)
			overlay->ops->trigger(overlay);
	}
}

static irqreturn_t ctrl_handle_irq(int irq, void *dev_id)
{
	struct mmphw_ctrl *ctrl = (struct mmphw_ctrl *)dev_id;
	struct mmp_path *path, *slave;
	u32 isr_en, disp_done, id, vsync_done;

	isr_en = readl_relaxed(ctrl->reg_base + SPU_IRQ_ISR) &
		readl_relaxed(ctrl->reg_base + SPU_IRQ_ENA);

	do {
		/* clear enabled irqs */
		if (!DISP_GEN4(ctrl->version))
			writel_relaxed(~isr_en, ctrl->reg_base + SPU_IRQ_ISR);
		else
			writel_relaxed(isr_en, ctrl->reg_base + SPU_IRQ_ISR);

		disp_done = isr_en & display_done_imasks;
		vsync_done = isr_en & vsync_imasks;

		for (id = 0; id < ctrl->path_num; id++) {
			path = ctrl->path_plats[id].path;
			if (path->irq_count.vsync_check) {
				path->irq_count.irq_count++;
				if (disp_done & display_done_imask(id))
					path->irq_count.dispd_count++;
				if (vsync_done & vsync_imask(id))
					path->irq_count.vsync_count++;
			}

			if (isr_en & (gfx_udflow_imask(id) | vid_udflow_imask(id)))
				trace_underflow(path, isr_en);
		}

		if (!disp_done) {
			if (!vsync_done)
				return IRQ_HANDLED;
			else {
				/*
				 * If it is only vsync irq, should call
				 * vsync.handle_irq.
				 */
				for (id = 0; id < ctrl->path_num; id++) {
					path = ctrl->path_plats[id].path;
					slave = path->slave;
					/*
					 * add spin lock portect with pan_display
					 */
					spin_lock(&path->vcnt_lock);
					if (path && path->vsync.handle_irq)
						path->vsync.handle_irq(&path->vsync);
					if (slave && slave->vsync.handle_irq)
						slave->vsync.handle_irq(&slave->vsync);
					if (!(path->irq_count.vsync_check) &&
						atomic_read(&path->irq_en_count) &&
						atomic_dec_and_test(
						&path->irq_en_count))
						mmp_path_set_irq(path, 0);
					spin_unlock(&path->vcnt_lock);
				}
				return IRQ_HANDLED;
			}
		}
		for (id = 0; id < ctrl->path_num; id++) {
			if (!(disp_done & display_done_imask(id)))
				continue;
			path = ctrl->path_plats[id].path;
			slave = path->slave;
			/*
			 * For special vsync which means dispd irq happen
			 * while vsync irq not happen, it indicate hardware
			 * v-blank peroid.
			 */
			if (!(vsync_done & vsync_imask(id))) {
				spin_lock(&path->commit_lock);
				if (path && atomic_read(&path->commit)) {
					path_trigger(path);
					/* update slave path */
					if (path->slave)
						path_trigger(path->slave);
					atomic_set(&path->commit, 0);
					trace_commit(path, 0);
				}
				spin_unlock(&path->commit_lock);
				/* Some special usage need wait this
				 * kind of vsync */
				if (path && path->special_vsync.handle_irq)
					path->special_vsync.handle_irq(
					&path->special_vsync);
			} else {
				/*
				 * add spin lock portect with pan_display
				 */
				spin_lock(&path->vcnt_lock);
				/*
				 * If vsync and dipd irq both happen, need to
				 * ensure vsync.handle_irq to be called.
				 */
				if (path && path->vsync.handle_irq)
					path->vsync.handle_irq(&path->vsync);
				if (slave && slave->vsync.handle_irq)
					slave->vsync.handle_irq(&slave->vsync);
				if (!(path->irq_count.vsync_check) &&
					atomic_read(&path->irq_en_count) &&
					atomic_dec_and_test(
					&path->irq_en_count))
					mmp_path_set_irq(path, 0);
				spin_unlock(&path->vcnt_lock);
			}
		}
	} while ((isr_en = readl_relaxed(ctrl->reg_base + SPU_IRQ_ISR) &
				readl_relaxed(ctrl->reg_base + SPU_IRQ_ENA)));

	return IRQ_HANDLED;
}

static u32 fmt_to_reg(int overlay_id, int pix_fmt)
{
	u32 rbswap = 0, uvswap = 0, yuvswap = 0,
		csc_en = 0, val = 0,
		vid = overlay_is_vid(overlay_id);

	switch (pix_fmt) {
	case PIXFMT_BGR565:
	case PIXFMT_BGR1555:
	case PIXFMT_BGR888PACK:
	case PIXFMT_BGR888UNPACK:
	case PIXFMT_BGRA888:
		rbswap = 1;
		break;
	case PIXFMT_VYUY:
	case PIXFMT_YVU422P:
	case PIXFMT_YVU420P:
		uvswap = 1;
		break;
	case PIXFMT_YUYV:
		yuvswap = 1;
		break;
	default:
		break;
	}

	switch (pix_fmt) {
	case PIXFMT_RGB565:
	case PIXFMT_BGR565:
		break;
	case PIXFMT_RGB1555:
	case PIXFMT_BGR1555:
		val = 0x1;
		break;
	case PIXFMT_RGB888PACK:
	case PIXFMT_BGR888PACK:
		val = 0x2;
		break;
	case PIXFMT_RGB888UNPACK:
	case PIXFMT_BGR888UNPACK:
		val = 0x3;
		break;
	case PIXFMT_RGBA888:
	case PIXFMT_BGRA888:
		val = 0x4;
		break;
	case PIXFMT_UYVY:
	case PIXFMT_VYUY:
	case PIXFMT_YUYV:
		val = 0x5;
		csc_en = 1;
		break;
	case PIXFMT_YUV422P:
	case PIXFMT_YVU422P:
		val = 0x6;
		csc_en = 1;
		break;
	case PIXFMT_YUV420P:
	case PIXFMT_YVU420P:
		val = 0x7;
		csc_en = 1;
		break;
	case PIXFMT_YUV420SP:
	case PIXFMT_YVU420SP:
		val = 0xc;
		csc_en = 1;
		break;
	default:
		break;
	}

	return (dma_palette(0) | dma_fmt(vid, val) |
		dma_swaprb(vid, rbswap) | dma_swapuv(vid, uvswap) |
		dma_swapyuv(vid, yuvswap) | dma_csc(vid, csc_en));
}

static void overlay_set_fmt(struct mmp_overlay *overlay)
{
	u32 tmp;
	struct mmp_path *path = overlay->path;
	int overlay_id = overlay->id;

	tmp = readl_relaxed(ctrl_regs(path) + dma_ctrl(0, path->id));
	tmp &= ~dma_mask(overlay_is_vid(overlay_id));
	tmp |= fmt_to_reg(overlay_id, overlay->win.pix_fmt);
	writel_relaxed(tmp, ctrl_regs(path) + dma_ctrl(0, path->id));
	if (is_420sp(overlay->win.pix_fmt)) {
		tmp = readl_relaxed(ctrl_regs(path) + LCD_YUV420SP_FMT_CTRL);
		tmp &= ~SWAP_420SP(path->id);
		if (overlay->win.pix_fmt == PIXFMT_YVU420SP)
			tmp |= SWAP_420SP(path->id);
		writel_relaxed(tmp, ctrl_regs(path) + LCD_YUV420SP_FMT_CTRL);
	}
}

static void overlay_set_win(struct mmp_overlay *overlay, struct mmp_win *win)
{
	struct lcd_regs *regs = path_regs(overlay->path);
	struct mmphw_ctrl *ctrl = overlay_to_ctrl(overlay);
	struct mmp_vdma_info *vdma;
	int overlay_id = overlay->id;

	/* assert win supported */
	memcpy(&overlay->win, win, sizeof(struct mmp_win));

	mutex_lock(&overlay->access_ok);

	if (overlay_is_vid(overlay_id)) {
		writel_relaxed(win->pitch[0], &regs->v_pitch_yc);
		writel_relaxed(win->pitch[2] << 16 |
				win->pitch[1], &regs->v_pitch_uv);

		writel_relaxed((win->ysrc << 16) | win->xsrc, &regs->v_size);
		writel_relaxed((win->ydst << 16) | win->xdst, &regs->v_size_z);
		writel_relaxed(win->ypos << 16 | win->xpos, &regs->v_start);
	} else {
		writel_relaxed(win->pitch[0], &regs->g_pitch);

		writel_relaxed((win->ysrc << 16) | win->xsrc, &regs->g_size);
		if (!DISP_GEN4(ctrl->version))
			writel_relaxed((win->ydst << 16) | win->xdst,
					&regs->g_size_z);
		else {
			if (win->xsrc != win->xdst || win->ysrc != win->ydst)
				dev_warn(ctrl->dev,
					"resize not support for graphic layer of GEN4\n");
		}
		writel_relaxed(win->ypos << 16 | win->xpos, &regs->g_start);
	}

	overlay_set_fmt(overlay);
	vdma = overlay->vdma;
	if (vdma && vdma->ops && vdma->ops->set_win)
		vdma->ops->set_win(vdma, win, overlay->status);
	mutex_unlock(&overlay->access_ok);
}

static void dmafetch_onoff(struct mmp_overlay *overlay, int on)
{
	int overlay_id = overlay->id;
	u32 mask = overlay_is_vid(overlay_id) ? CFG_DMA_ENA_MASK :
		CFG_GRA_ENA_MASK;
	u32 enable = overlay_is_vid(overlay_id) ? CFG_DMA_ENA(1) :
		CFG_GRA_ENA(1);
	u32 tmp;
	struct mmp_path *path = overlay->path;
	struct mmp_vdma_info *vdma;

	vdma = overlay->vdma;
	if (vdma && vdma->ops && vdma->ops->set_on)
		vdma->ops->set_on(vdma, on);
	/* dma enable control */
	tmp = readl_relaxed(ctrl_regs(path) + dma_ctrl(0, path->id));
	tmp &= ~mask;
	tmp |= (on ? enable : 0);
	writel(tmp, ctrl_regs(path) + dma_ctrl(0, path->id));
}

static void path_enabledisable(struct mmp_path *path, int on)
{
	struct mmphw_path_plat *plat = path_to_path_plat(path);
	struct clk *clk = plat->clk;
	struct mmp_apical_info *apical;
	u32 tmp;

	if (!clk)
		return;

	/* path enable control */
	tmp = readl_relaxed(ctrl_regs(path) + intf_ctrl(path->id));
	tmp &= ~CFG_DUMB_ENA_MASK;
	tmp |= (on ? CFG_DUMB_ENA(1) : 0);

	apical = path->apical;
	if (on) {
		pm_qos_update_request(&plat->qos_idle,
					plat->ctrl->lpm_qos);
		clk_prepare_enable(clk);
		writel_relaxed(tmp, ctrl_regs(path) + intf_ctrl(path->id));
		if (apical && apical->ops && apical->ops->set_on)
			apical->ops->set_on(apical, 1);
	} else {
		writel_relaxed(tmp, ctrl_regs(path) + intf_ctrl(path->id));
		if (apical && apical->ops && apical->ops->set_on)
			apical->ops->set_on(apical, 0);
		clk_disable_unprepare(clk);
		pm_qos_update_request(&plat->qos_idle,
					PM_QOS_CPUIDLE_BLOCK_DEFAULT_VALUE);
	}
}

static void path_set_timing(struct mmp_path *path)
{
	struct lcd_regs *regs = path_regs(path);
	u32 total_x, total_y, vsync_ctrl, tmp, mask, path_clk_req,
		link_config = path_to_path_plat(path)->link_config,
		h_porch, sync_ratio, master_mode;
	struct mmp_mode *mode = &path->mode;
	struct clk *clk = path_to_path_plat(path)->clk;
	struct mmphw_ctrl *ctrl = path_to_ctrl(path);
	struct mmp_dsi *dsi = mmp_path_to_dsi(path);

	if (PATH_OUT_DSI == path->output_type
			&& (!dsi || !dsi->get_sync_val)) {
		dev_err(path->dev, "dsi invalid\n");
		return;
	}

	/* polarity of timing signals */
	tmp = readl_relaxed(ctrl_regs(path) + intf_ctrl(path->id)) & 0x1;
	tmp |= mode->vsync_invert ? 0x8 : 0;
	tmp |= mode->hsync_invert ? 0x4 : 0;
	tmp |= link_config & CFG_DUMBMODE_MASK;
	tmp |= CFG_DUMB_ENA(1);
	writel_relaxed(tmp, ctrl_regs(path) + intf_ctrl(path->id));

	/* interface rb_swap setting */
	tmp = readl_relaxed(ctrl_regs(path) + intf_rbswap_ctrl(path->id)) &
		(~(CFG_INTFRBSWAP_MASK));
	tmp |= link_config & CFG_INTFRBSWAP_MASK;
	writel_relaxed(tmp, ctrl_regs(path) + intf_rbswap_ctrl(path->id));

	/* x/y res, porch, sync */
	if (PATH_OUT_PARALLEL == path->output_type) {
		h_porch = (mode->left_margin << 16) | mode->right_margin;
		total_x = mode->xres + mode->left_margin + mode->right_margin +
			mode->hsync_len;
		path_clk_req = mode->pixclock_freq;
		vsync_ctrl = (mode->left_margin << 16) | (mode->left_margin);
	} else {
		sync_ratio = dsi->get_sync_val(dsi, DSI_SYNC_RATIO);
		h_porch = (mode->xres + mode->right_margin) * sync_ratio / 10 -
			mode->xres;
		h_porch |= (mode->left_margin * sync_ratio / 10) << 16;
		total_x = (mode->xres + mode->left_margin + mode->right_margin +
			mode->hsync_len) * sync_ratio / 10;
		vsync_ctrl = (((mode->xres + mode->right_margin) * sync_ratio / 10) << 16) |
			((mode->xres + mode->right_margin) * sync_ratio / 10);
		path_clk_req = dsi->get_sync_val(dsi,
			DSI_SYNC_CLKREQ);

		/*
		 * FIXME: On ULC:
		 * For WVGA with 2 lanes whose pixel_clk is lower than
		 * esc clk, we need to set pixel >= esc clk, so that LP
		 * cmd and video mode cna work.
		 */
		if (DISP_GEN4_LITE(ctrl->version)) {
			if (path_clk_req < ESC_52M)
				path_clk_req = ESC_52M;
		}
		/* master mode setting of dsi phy */
		master_mode = dsi->get_sync_val(dsi,
			DSI_SYNC_MASTER_MODE);
		if (DISP_GEN4(ctrl->version)) {
			/*
			 * DC4_LITE, DC4 and older version have diffrent offset
			 * for master_control register.
			 */
			if (DISP_GEN4_LITE(ctrl->version)) {
				mask = MASTER_ENH_GEN4_LITE(path->id) |
					MASTER_ENV_GEN4_LITE(path->id);
				tmp = readl_relaxed(ctrl_regs(path) +
						TIMING_MASTER_CONTROL_GEN4_LITE);
				tmp &= ~mask;
				if (master_mode) {
					tmp |= MASTER_ENH_GEN4_LITE(path->id) |
						MASTER_ENV_GEN4_LITE(path->id);
					writel_relaxed(tmp, ctrl_regs(path) +
							TIMING_MASTER_CONTROL_GEN4_LITE);
				}
			} else {
				mask = MASTER_ENH_GEN4(path->id) |
					MASTER_ENV_GEN4(path->id) |
					DSI_START_SEL_MASK_GEN4(path->id);
				tmp = readl_relaxed(ctrl_regs(path) +
						TIMING_MASTER_CONTROL_GEN4);
				tmp &= ~mask;
				if (master_mode) {
					tmp |= MASTER_ENH_GEN4(path->id) |
						MASTER_ENV_GEN4(path->id) |
						DSI_START_SEL_GEN4(path->id, 0);
					writel_relaxed(tmp, ctrl_regs(path) +
							TIMING_MASTER_CONTROL_GEN4);
				}
			}
		} else {
			mask = MASTER_ENH(path->id) | MASTER_ENV(path->id) |
				DSI_START_SEL_MASK(path->id);
			tmp = readl_relaxed(ctrl_regs(path) +
					TIMING_MASTER_CONTROL);
			tmp &= ~mask;
			if (master_mode) {
				tmp |= MASTER_ENH(path->id) |
					MASTER_ENV(path->id) |
					DSI_START_SEL(path->id, 0, 0);
				writel_relaxed(tmp, ctrl_regs(path) +
						TIMING_MASTER_CONTROL);
			}
		}
	}

	writel_relaxed((mode->yres << 16) | mode->xres, &regs->screen_active);
	writel_relaxed(h_porch, &regs->screen_h_porch);
	writel_relaxed((mode->upper_margin << 16) | mode->lower_margin,
		&regs->screen_v_porch);
	total_y = mode->yres + mode->upper_margin + mode->lower_margin +
		mode->vsync_len;
	writel_relaxed((total_y << 16) | total_x, &regs->screen_size);
	writel_relaxed(vsync_ctrl, &regs->vsync_ctrl);

	/* set path_clk */
	if (clk && path_clk_req) {
		clk_set_rate(clk, path_clk_req);
		if (DISP_GEN4(ctrl->version)) {
			/*
			 * clk_set_rate use ROUND_UP to round rate, the final
			 * rate may be less than ESC_52M, so need to check it
			 * and set it again to ensure the path_clk is bigger than
			 * ESC_52M, or the panel init cmd may be failed.
			 */
			while (clk->rate < ESC_52M) {
				path_clk_req += ESC_STEP_4M;
				clk_set_rate(clk, path_clk_req);
			}
		}
	}
}

static void path_onoff(struct mmp_path *path, int on)
{
	struct mmp_dsi *dsi = mmp_path_to_dsi(path);

	if (path->status == on) {
		dev_info(path->dev, "path %s is already %s\n",
				path->name, status_name(path->status));
		return;
	}

	mutex_lock(&path->access_ok);

	if (on) {
		path_enabledisable(path, 1);
		path_vsync_sync(path);
		if (dsi && dsi->set_status)
			/* panel onoff would be called in dsi onoff if exist */
			dsi->set_status(dsi, MMP_ON);
		else if (path->panel && path->panel->set_status)
			path->panel->set_status(path->panel, MMP_ON);
	} else {
		if (dsi && dsi->set_status)
			/* panel onoff would be called in dsi onoff if exist */
			dsi->set_status(dsi, MMP_OFF);
		else if (path->panel && path->panel->set_status)
			path->panel->set_status(path->panel, MMP_OFF);

		path_enabledisable(path, 0);
	}
	path->status = on;

	mutex_unlock(&path->access_ok);
}

static void overlay_do_onoff(struct mmp_overlay *overlay, int status)
{
	struct mmphw_ctrl *ctrl = path_to_ctrl(overlay->path);
	int hw_trigger = 0;
	int on = status_is_on(status);
	struct mmp_path *path = overlay->path;
	struct mmp_dsi *dsi = mmp_path_to_dsi(path);
	struct mmp_vdma_info *vdma = overlay->vdma;

	mutex_lock(&ctrl->access_ok);

	overlay->status = on;

	if (status == MMP_ON_REDUCED) {
		path_enabledisable(path, 1);
		if (dsi && dsi->set_status)
			dsi->set_status(dsi, status);
		if (path->panel && path->panel->set_status)
			path->panel->set_status(path->panel, status);
		if (vdma && vdma->ops && vdma->ops->set_on)
			vdma->ops->set_on(vdma, status);
		path->status = on;
	} else if (on) {
		if (path->ops.check_status(path) != path->status) {
			if (!(DISP_GEN4_LITE(path_to_ctrl(path)->version) ||
				DISP_GEN4_PLUS(path_to_ctrl(path)->version)))
				path_onoff(path, on);
			hw_trigger = 1;
		}

		dmafetch_onoff(overlay, on);
		if (hw_trigger) {
			/* if path DMA enabled the first time, we need set hw
			 * trigger immediately */
			path_hw_trigger(path);
			/* in ulc and helan3, we need to move dump_en after bit
			 * trigger */
			if (DISP_GEN4_LITE(path_to_ctrl(path)->version) ||
				DISP_GEN4_PLUS(path_to_ctrl(path)->version))
				path_onoff(path, on);
		}
	} else {
		dmafetch_onoff(overlay, on);

		if (path->ops.check_status(path) != path->status)
			path_onoff(path, on);
	}

	mutex_unlock(&ctrl->access_ok);
}

static void overlay_set_status(struct mmp_overlay *overlay, int status)
{
	int on = status_is_on(status);
	mutex_lock(&overlay->access_ok);
	switch (status) {
	case MMP_ON:
	case MMP_ON_REDUCED:
		if (!atomic_read(&overlay->on_count))
			overlay_do_onoff(overlay, status);
		atomic_inc(&overlay->on_count);
		break;
	case MMP_OFF:
		if (atomic_dec_and_test(&overlay->on_count))
			overlay_do_onoff(overlay, status);
		break;
	case MMP_ON_DMA:
	case MMP_OFF_DMA:
		overlay->status = on;
		dmafetch_onoff(overlay, on);
		break;
	default:
		break;
	}
	dev_dbg(overlay_to_ctrl(overlay)->dev, "set %s: count %d\n",
		status_name(status), atomic_read(&overlay->on_count));
	mutex_unlock(&overlay->access_ok);
}

static int overlay_set_addr(struct mmp_overlay *overlay, struct mmp_addr *addr)
{
	struct lcd_regs *regs = path_regs(overlay->path);
	struct mmp_vdma_info *vdma;
	int overlay_id = overlay->id;

	/* FIXME: assert addr supported */
	memcpy(&overlay->addr, addr, sizeof(struct mmp_addr));

	if (overlay_is_vid(overlay_id)) {
		writel_relaxed(addr->phys[0], &regs->v_y0);
		writel_relaxed(addr->phys[1], &regs->v_u0);
		writel_relaxed(addr->phys[2], &regs->v_v0);
	} else
		writel_relaxed(addr->phys[0], &regs->g_0);

	vdma = overlay->vdma;
	if (vdma && vdma->ops && vdma->ops->set_addr)
		vdma->ops->set_addr(vdma, addr, -1, overlay->status);

	return overlay->addr.phys[0];
}

static void overlay_set_vsmooth_en(struct mmp_overlay *overlay, int en)
{
	struct mmp_path *path = overlay->path;
	struct mmphw_ctrl *ctrl = path_to_ctrl(path);
	u32 tmp;

	if (!DISP_GEN4(ctrl->version))
		return;
	/* only video layer support vertical smooth */
	if (!overlay_is_vid(overlay->id))
		return;
	mutex_lock(&overlay->access_ok);
	tmp = readl_relaxed(ctrl_regs(path) + dma_ctrl(2, path->id));
	tmp &= ~1;
	tmp |= !!en;
	writel(tmp, ctrl_regs(path) + dma_ctrl(2, path->id));
	mutex_unlock(&overlay->access_ok);
}

static int overlay_set_surface(struct mmp_overlay *overlay,
	struct mmp_surface *surface)
{
	struct lcd_regs *regs = path_regs(overlay->path);
	struct mmp_vdma_info *vdma;
	struct mmp_shadow *shadow;
	struct mmp_win *win = &surface->win;
	struct mmp_addr *addr = &surface->addr;
	struct mmp_path *path = overlay->path;
	int overlay_id = overlay->id, count = 0;
	int fd = surface->fd;

	if (unlikely(atomic_read(&path->commit))) {
		while (atomic_read(&path->commit) && count < 10) {
			mmp_wait_vsync(&path->vsync);
			count++;
		}
		if (count >= 10)
			pr_warn("WARN: fb commit faild after tried 10 times\n");
	}

	/* FIXME: assert addr supported */
	memcpy(&overlay->addr, addr, sizeof(struct mmp_addr));
	memcpy(&overlay->win, win, sizeof(struct mmp_win));

	vdma = overlay->vdma;
	shadow = overlay->shadow;
	/* Check whether there is software trigger */
	if (overlay->ops->trigger) {
		if (shadow && shadow->ops && shadow->ops->set_surface)
			shadow->ops->set_surface(shadow, surface);
	} else {
		if (vdma && vdma->ops && vdma->ops->set_descriptor_chain)
			vdma->ops->set_descriptor_chain(vdma,
				(fd >= 0 && fd < 1024) ? 1 : 0);

		pr_debug("%s: decompress_en : %d\n", __func__,
				!!(surface->flag & DECOMPRESS_MODE));
		if (vdma && vdma->ops && vdma->ops->set_decompress_en)
			vdma->ops->set_decompress_en(vdma,
				!!(surface->flag & DECOMPRESS_MODE));

		if (vdma && vdma->ops && vdma->ops->set_win)
			vdma->ops->set_win(vdma, win, overlay->status);

		if (vdma && vdma->ops && vdma->ops->set_addr)
			vdma->ops->set_addr(vdma, addr, fd, overlay->status);

		if (overlay_is_vid(overlay_id)) {
			writel_relaxed(addr->phys[0], &regs->v_y0);
			writel_relaxed(addr->phys[1], &regs->v_u0);
			writel_relaxed(addr->phys[2], &regs->v_v0);
		} else
			writel_relaxed(addr->phys[0], &regs->g_0);

		if (overlay_is_vid(overlay_id)) {
			writel_relaxed(win->pitch[0], &regs->v_pitch_yc);
			writel_relaxed(win->pitch[2] << 16 |
					win->pitch[1], &regs->v_pitch_uv);

			writel_relaxed((win->ysrc << 16) | win->xsrc,
				&regs->v_size);
			writel_relaxed((win->ydst << 16) | win->xdst,
				&regs->v_size_z);
			writel_relaxed(win->ypos << 16 | win->xpos,
				&regs->v_start);
		} else {
			writel_relaxed(win->pitch[0], &regs->g_pitch);

			writel_relaxed((win->ysrc << 16) | win->xsrc,
				&regs->g_size);
			writel_relaxed((win->ydst << 16) | win->xdst,
				&regs->g_size_z);
			writel_relaxed(win->ypos << 16 | win->xpos,
				&regs->g_start);
		}

		overlay_set_fmt(overlay);

	}
	return overlay->addr.phys[0];
}

static void overlay_trigger(struct mmp_overlay *overlay)
{
	struct lcd_regs *regs = path_regs(overlay->path);
	int overlay_id = overlay->id;
	struct mmp_shadow *shadow = overlay->shadow;
	struct mmp_addr *addr;
	struct mmp_win *win;
	struct mmp_shadow_dma *dma;
	struct mmp_shadow_buffer *buffer;
	struct mmp_shadow_alpha *alpha;
	struct mmp_vdma_info *vdma;

	if (!list_empty(&shadow->buffer_list.queue)) {
		buffer = list_first_entry(
				&shadow->buffer_list.queue,
				struct mmp_shadow_buffer, queue);
		list_del(&buffer->queue);

		vdma = overlay->vdma;
		if (buffer) {
			if (vdma && vdma->ops && vdma->ops->set_decompress_en)
				vdma->ops->set_decompress_en(vdma,
					!!(buffer->flags & BUFFER_DEC));
			buffer->flags &= ~BUFFER_DEC;
		}

		if (buffer && (buffer->flags & UPDATE_ADDR)) {
			addr = &buffer->addr;
			trace_addr(overlay->id, addr, 0);
			if (vdma && vdma->ops && vdma->ops->set_addr)
				vdma->ops->set_addr(vdma, addr, buffer->fd,
						overlay->status);
			if (overlay_is_vid(overlay_id)) {
				writel_relaxed(addr->phys[0], &regs->v_y0);
				writel_relaxed(addr->phys[1], &regs->v_u0);
				writel_relaxed(addr->phys[2], &regs->v_v0);
			} else
				writel_relaxed(addr->phys[0], &regs->g_0);
			buffer->flags &= ~UPDATE_ADDR;
		}

		if (buffer && (buffer->flags & UPDATE_WIN)) {
			win = &buffer->win;
			trace_win(overlay->id, win, 0);
			if (vdma && vdma->ops && vdma->ops->set_win)
				vdma->ops->set_win(vdma, win, overlay->status);
			if (overlay_is_vid(overlay_id)) {
				writel_relaxed(win->pitch[0],
					&regs->v_pitch_yc);
				writel_relaxed(win->pitch[2] << 16 |
					win->pitch[1], &regs->v_pitch_uv);
				writel_relaxed((win->ysrc << 16) | win->xsrc,
					&regs->v_size);
				writel_relaxed((win->ydst << 16) | win->xdst,
					&regs->v_size_z);
				writel_relaxed(win->ypos << 16 | win->xpos,
					&regs->v_start);
			} else {
				writel_relaxed(win->pitch[0], &regs->g_pitch);

				writel_relaxed((win->ysrc << 16) | win->xsrc,
					&regs->g_size);
				writel_relaxed((win->ydst << 16) | win->xdst,
					&regs->g_size_z);
				writel_relaxed(win->ypos << 16 | win->xpos,
					&regs->g_start);
			}

			overlay_set_fmt(overlay);
			buffer->flags &= ~UPDATE_WIN;
		}
		kfree(buffer);
	}

	if (!list_empty(&shadow->dma_list.queue)) {
		dma = list_first_entry(
			&shadow->dma_list.queue,
			struct mmp_shadow_dma, queue);
		list_del(&dma->queue);
		trace_dma(overlay->id, dma->dma_onoff, 0);
		dmafetch_onoff(overlay, dma->dma_onoff);
		dma->flags &= ~UPDATE_DMA;
		kfree(dma);
	}

	if (!list_empty(&shadow->alpha_list.queue)) {
		alpha = list_first_entry(
			&shadow->alpha_list.queue,
			struct mmp_shadow_alpha, queue);
		list_del(&alpha->queue);
		trace_alpha(overlay->id, &alpha->alpha, 0);
		path_set_alpha(overlay, &alpha->alpha);
		alpha->flags &= ~UPDATE_ALPHA;
		kfree(alpha);
	}
}

static int is_mode_changed(struct mmp_mode *dst, struct mmp_mode *src)
{
	return !src || !dst
		|| src->refresh != dst->refresh
		|| src->xres != dst->xres
		|| src->yres != dst->yres
		|| src->left_margin != dst->left_margin
		|| src->right_margin != dst->right_margin
		|| src->upper_margin != dst->upper_margin
		|| src->lower_margin != dst->lower_margin
		|| src->hsync_len != dst->hsync_len
		|| src->vsync_len != dst->vsync_len
		|| !!(src->hsync_invert) != !!(dst->hsync_invert)
		|| !!(src->vsync_invert) != !!(dst->vsync_invert)
		|| !!(src->invert_pixclock) != !!(dst->invert_pixclock)
		|| src->pixclock_freq / 1024 != src->pixclock_freq / 1024
		|| src->pix_fmt_out != src->pix_fmt_out;
}

/*
 * dynamically set mode is not supported.
 * if change mode when path on, path on/off is required.
 * or we would direct set path->mode
*/
static void path_set_mode(struct mmp_path *path, struct mmp_mode *mode)
{
	struct mmp_dsi *dsi = mmp_path_to_dsi(path);
	/* mode unchanged? do nothing */
	if (!is_mode_changed(&path->mode, mode))
		return;

	/* FIXME: assert mode supported */
	memcpy(&path->mode, mode, sizeof(struct mmp_mode));
	if (path->mode.xres != path->mode.real_xres)
		path->mode.xres = path->mode.real_xres;
	if (path->mode.yres != path->mode.real_yres)
		path->mode.yres = path->mode.real_yres;

	if (path->status) {
		path_onoff(path, 0);
		if (dsi && dsi->set_mode)
			dsi->set_mode(dsi, &path->mode);
		path_set_timing(path);
		path_onoff(path, 1);
	} else {
		if (dsi && dsi->set_mode)
			dsi->set_mode(dsi, &path->mode);
		path_set_timing(path);
	}
}

static void overlay_set_decompress_en(struct mmp_overlay *overlay, int en)
{
	struct mmp_vdma_info *vdma;

	overlay->decompress = en;
	vdma = overlay->vdma;
	if (vdma && vdma->ops && vdma->ops->set_decompress_en)
		vdma->ops->set_decompress_en(vdma, en);
}

static struct mmp_overlay_ops mmphw_overlay_ops = {
	.set_status = overlay_set_status,
	.set_win = overlay_set_win,
	.set_addr = overlay_set_addr,
	.set_surface = overlay_set_surface,
	.set_colorkey_alpha = overlay_set_colorkey_alpha,
	.set_alpha = overlay_set_path_alpha,
	.set_vsmooth_en = overlay_set_vsmooth_en,
	.trigger = overlay_trigger,
	.set_decompress_en = overlay_set_decompress_en,
};

static void ctrl_set_default(struct mmphw_ctrl *ctrl)
{
	u32 tmp, irq_mask;

	/*
	 * LCD Global control(LCD_TOP_CTRL) should be configed before
	 * any other LCD registers read/write, or there maybe issues.
	 */
	tmp = readl_relaxed(ctrl->reg_base + LCD_TOP_CTRL);

	/* If define master/slave path, set all DMA objects to master path */
	if (ctrl->master_path_name && ctrl->slave_path_name)
		tmp |= 0x50fff0;
	else
		tmp |= 0xfff0;

	writel_relaxed(tmp, ctrl->reg_base + LCD_TOP_CTRL);

	/* clear all the interrupts */
	if (!DISP_GEN4(ctrl->version))
		writel_relaxed(0x0, ctrl->reg_base + SPU_IRQ_ISR);
	else
		writel_relaxed(0xffffffff, ctrl->reg_base + SPU_IRQ_ISR);

	/* disable all interrupts */
	irq_mask = path_imasks(0) | err_imask(0) |
		   path_imasks(1) | err_imask(1);
	tmp = readl_relaxed(ctrl->reg_base + SPU_IRQ_ENA);
	tmp &= ~irq_mask;
	writel_relaxed(tmp, ctrl->reg_base + SPU_IRQ_ENA);
}

static void path_set_default(struct mmp_path *path)
{
	struct lcd_regs *regs = path_regs(path);
	struct mmphw_ctrl *ctrl = path_to_ctrl(path);
	u32 dma_ctrl1, mask, tmp, path_config;

	path_config = path_to_path_plat(path)->path_config;

	/* Configure IOPAD: should be parallel only */
	if (PATH_OUT_PARALLEL == path->output_type && !DISP_GEN4(ctrl->version)) {
		mask = CFG_IOPADMODE_MASK | CFG_BURST_MASK | CFG_BOUNDARY_MASK;
		tmp = readl_relaxed(ctrl_regs(path) + SPU_IOPAD_CONTROL);
		tmp &= ~mask;
		tmp |= path_config;
		writel_relaxed(tmp, ctrl_regs(path) + SPU_IOPAD_CONTROL);
	}

	/*
	 * For DC4_LITE, GATED_ENABLE and VSYNC_INV bits are in DUMB_CTRL,
	 * and LCD_PN_CTRL1 is removed in DC4_LITE;
	 * For DC4 and older version, they are in LCD_PN_CTRL1;
	 */
	if (DISP_GEN4_LITE(path_to_ctrl(path)->version)) {
		tmp = readl_relaxed(ctrl_regs(path) + intf_ctrl(path->id));
		tmp |= CFG_VSYNC_INV_DC4_LITE(1) |
			CFG_GATED_ENA_DC4_LITE(1);
		writel_relaxed(tmp, ctrl_regs(path) + intf_ctrl(path->id));
	} else {
		/*
		 * Configure default bits: vsync triggers DMA,
		 * power save enable, configure alpha registers to
		 * display 100% graphics, and set pixel command.
		 */
		dma_ctrl1 = 0x2032ff81;

		/* If TV path as slave, diable interlace mode */
		if (ctrl->slave_path_name &&
			(!strcmp(ctrl->slave_path_name, path->name)))
			/* Fix me, when slave path isn't TV path */
			dma_ctrl1 &= ~CFG_TV_NIB_MASK;

		dma_ctrl1 |= CFG_VSYNC_INV_MASK;
		writel_relaxed(dma_ctrl1, ctrl_regs(path) + dma_ctrl(1, path->id));
	}

	/* Configure default register values */
	writel_relaxed(0x00000000, &regs->blank_color);
	writel_relaxed(0x00000000, &regs->g_1);
	writel_relaxed(0x00000000, &regs->g_start);

	/*
	 * 1.enable multiple burst request in DMA AXI
	 * bus arbiter for faster read if not tv path;
	 * 2.enable horizontal smooth filter;
	 */
	mask = CFG_GRA_HSMOOTH_MASK | CFG_DMA_HSMOOTH_MASK | CFG_ARBFAST_ENA(1);
	tmp = readl_relaxed(ctrl_regs(path) + dma_ctrl(0, path->id));
	tmp |= mask;
	if (PATH_TV == path->id)
		tmp &= ~CFG_ARBFAST_ENA(1);
	writel_relaxed(tmp, ctrl_regs(path) + dma_ctrl(0, path->id));
}

static int path_set_commit(struct mmp_path *path)
{
	unsigned long flags;

	if (DISP_GEN4(path_to_ctrl(path)->version)) {
		path_hw_trigger(path);
		return 0;
	}

	if (unlikely(atomic_read(&path->commit))) {
		while (atomic_read(&path->commit)) {
			trace_commit(path, 2);
			mmp_wait_vsync(&path->vsync);
		}
		if (!atomic_read(&path->commit)) {
			spin_lock_irqsave(&path->commit_lock, flags);
			trace_commit(path, 1);
			atomic_set(&path->commit, 1);
			spin_unlock_irqrestore(&path->commit_lock, flags);
		} else
			BUG_ON(1);
	} else {
		spin_lock_irqsave(&path->commit_lock, flags);
		trace_commit(path, 1);
		atomic_set(&path->commit, 1);
		spin_unlock_irqrestore(&path->commit_lock, flags);
	}

	return 0;
}

static int path_ctrl_safe(struct mmp_path *path)
{
	struct mmphw_ctrl *ctrl = path_to_ctrl(path);

	if (unlikely(ctrl->status == MMP_OFF)) {
		pr_warn("WARN: LCDC already off\n");
		return 0;
	} else {
		return 1;
	}
}

static int path_get_version(struct mmp_path *path)
{
	struct mmphw_ctrl *ctrl = path_to_ctrl(path);
	return ctrl->version;
}

static int path_init(struct mmphw_path_plat *path_plat,
		struct mmp_mach_path_config *config)
{
	struct mmphw_ctrl *ctrl = path_plat->ctrl;
	struct mmp_path_info *path_info;
	struct mmp_path *path = NULL;
	int i;

	dev_info(ctrl->dev, "%s: %s\n", __func__, config->name);

	/* init driver data */
	path_info = kzalloc(sizeof(struct mmp_path_info), GFP_KERNEL);
	if (!path_info) {
		dev_err(ctrl->dev, "%s: unable to alloc path_info for %s\n",
				__func__, config->name);
		return 0;
	}
	path_info->name = config->name;
	path_info->id = path_plat->id;
	path_info->dev = ctrl->dev;
	path_info->output_type = config->output_type;
	path_info->overlay_num = config->overlay_num;
	path_info->overlay_table = config->overlay_table;
	if (DISP_GEN4(ctrl->version))
		mmphw_overlay_ops.trigger = NULL;
	path_info->overlay_ops = &mmphw_overlay_ops;
	path_info->plat_data = path_plat;

	/* create/register platform device */
	path = mmp_register_path(path_info);
	if (!path) {
		kfree(path_info);
		return 0;
	}
	path->apical = mmp_apical_alloc(path->id);
	for (i = 0; i < path->overlay_num; i++) {
		path->overlays[i].vdma =
			mmp_vdma_alloc(path->overlays[i].id, 0);
		if (path->overlays[i].ops->trigger)
			path->overlays[i].shadow =
				mmp_shadow_alloc(&path->overlays[i]);
	}
	path_plat->path = path;
	path_plat->path_config = config->path_config;
	path_plat->link_config = config->link_config;
	for (i = 0; i < path->overlay_num; i++) {
		if (path->overlays[i].ops->trigger)
			path->overlays[i].shadow =
				mmp_shadow_alloc(&path->overlays[i]);
	}
	/* get clock: path clock name same as path name */
	path_plat->clk = devm_clk_get(ctrl->dev, config->name);
	if (IS_ERR(path_plat->clk)) {
		/* it's possible to not have path_plat->clk */
		dev_info(ctrl->dev, "unable to get clk %s\n", config->name);
		path_plat->clk = NULL;
	}
	path_plat->qos_idle.name = path->name;
	pm_qos_add_request(&path_plat->qos_idle, PM_QOS_CPUIDLE_BLOCK,
				PM_QOS_CPUIDLE_BLOCK_DEFAULT_VALUE);
	/* add operations after path set */
	path->vsync.path = path;
	path->vsync.type = LCD_VSYNC;
	mmp_vsync_init(&path->vsync);
	path->special_vsync.path = path;
	path->special_vsync.type = LCD_SPECIAL_VSYNC;
	mmp_vsync_init(&path->special_vsync);
	path->ops.set_mode = path_set_mode;
	path->ops.set_irq = path_set_irq;
	path->ops.set_gamma = path_set_gamma;
	path->ops.set_commit = path_set_commit;
	path->ops.set_trigger = path_hw_trigger;
	path->ops.ctrl_safe = path_ctrl_safe;
	path->ops.get_version = path_get_version;

	path_set_default(path);

	kfree(path_info);
	return 1;
}

static void path_deinit(struct mmphw_path_plat *path_plat)
{
	int i = 0;

	if (!path_plat)
		return;
	pm_qos_remove_request(&path_plat->qos_idle);

	if (path_plat->clk)
		devm_clk_put(path_plat->ctrl->dev, path_plat->clk);

	if (path_plat->path) {
		mmp_vsync_deinit(&path_plat->path->vsync);
		mmp_vsync_deinit(&path_plat->path->special_vsync);
		mmp_unregister_path(path_plat->path);
	}

	if (path_plat->path->overlays[i].ops->trigger) {
		for (i = 0; i < path_plat->path->overlay_num; i++)
			mmp_shadow_free(path_plat->path->overlays[i].shadow);
	}
}

#ifdef CONFIG_OF
static const struct of_device_id mmp_disp_dt_match[] = {
	{ .compatible = "marvell,mmp-disp" },
	{},
};
#endif

static int mmphw_probe(struct platform_device *pdev)
{
	struct mmp_mach_plat_info *mi;
	struct resource *res;
	int ret, i, size, irq, path_num;
	struct mmphw_path_plat *path_plat;
	struct mmphw_ctrl *ctrl = NULL;
	struct device_node *np = pdev->dev.of_node;
	struct device_node *child_np;
	struct mmp_mach_path_config *paths_config;
	struct mmp_mach_path_config dt_paths_config[MAX_PATH];
	u32 overlay_num[MAX_PATH][MAX_OVERLAY];
	struct mmp_path *path = NULL;

	/* get resources from platform data */
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (res == NULL) {
		dev_err(&pdev->dev, "%s: no IO memory defined\n", __func__);
		ret = -ENOENT;
		goto failed;
	}

	irq = platform_get_irq(pdev, 0);
	if (irq < 0) {
		dev_err(&pdev->dev, "%s: no IRQ defined\n", __func__);
		ret = -ENOENT;
		goto failed;
	}

	if (IS_ENABLED(CONFIG_OF)) {
		if (of_property_read_u32(np, "marvell,path-num", &path_num)) {
			ret = -EINVAL;
			goto failed;
		}
		/* allocate ctrl */
		size = sizeof(struct mmphw_ctrl) +
			sizeof(struct mmphw_path_plat) * path_num;
		ctrl = devm_kzalloc(&pdev->dev, size, GFP_KERNEL);
		if (!ctrl) {
			ret = -ENOMEM;
			goto failed;
		}

		ctrl->path_num = path_num;
		if (of_property_read_string(np, "marvell,disp-name",
					&ctrl->name)) {
			ret = -EINVAL;
			goto failed;
		}

		if (of_find_property(np, "marvell,master-path", NULL) &&
			of_property_read_string(np, "marvell,master-path",
					&ctrl->master_path_name)) {
			ret = -EINVAL;
			goto failed;
		}

		if (of_find_property(np, "marvell,slave-path", NULL) &&
			of_property_read_string(np, "marvell,slave-path",
					&ctrl->slave_path_name)) {
			ret = -EINVAL;
			goto failed;
		}

		if (of_property_read_u32(np, "lpm-qos", &ctrl->lpm_qos)) {
			ret = -EINVAL;
			dev_err(&pdev->dev, "%s: get lpm-ops failed!\n",
					__func__);
			goto failed;
		}

		if (of_get_child_count(np) != ctrl->path_num) {
			dev_err(&pdev->dev, "%s: path_num not match!\n",
					__func__);
			ret = -EINVAL;
			goto failed;
		}

		i = 0;
		for_each_child_of_node(np, child_np) {
			if (of_property_read_string(child_np,
					"marvell,path-name",
					&dt_paths_config[i].name)) {
				ret = -EINVAL;
				goto failed;
			}
			if (of_property_read_u32(child_np,
					"marvell,overlay-num",
					&dt_paths_config[i].overlay_num)) {
				ret = -EINVAL;
				goto failed;
			}
			if (of_property_read_u32_array(child_np,
					"marvell,overlay-table",
					overlay_num[i],
					dt_paths_config[i].overlay_num)) {
				ret = -EINVAL;
				goto failed;
			}
			dt_paths_config[i].overlay_table = overlay_num[i];
			if (of_property_read_u32(child_np,
					"marvell,output-type",
					&dt_paths_config[i].output_type)) {
				ret = -EINVAL;
				goto failed;
			}
			if (of_property_read_u32(child_np,
					"marvell,path-config",
					&dt_paths_config[i].path_config)) {
				ret = -EINVAL;
				goto failed;
			}
			if (of_property_read_u32(child_np,
					"marvell,link-config",
					&dt_paths_config[i].link_config)) {
				ret = -EINVAL;
				goto failed;
			}
			i++;
		}
		paths_config = dt_paths_config;
	} else {
		/* get configs from platform data */
		mi = pdev->dev.platform_data;
		if (mi == NULL || !mi->path_num || !mi->paths) {
			dev_err(&pdev->dev, "%s: no platform data defined\n",
					__func__);
			ret = -EINVAL;
			goto failed;
		}

		/* allocate ctrl */
		size = sizeof(struct mmphw_ctrl) +
			sizeof(struct mmphw_path_plat) * mi->path_num;
		ctrl = devm_kzalloc(&pdev->dev, size, GFP_KERNEL);
		if (!ctrl) {
			ret = -ENOMEM;
			goto failed;
		}

		ctrl->path_num = mi->path_num;
		ctrl->name = mi->name;
		paths_config = mi->paths;
	}

	ctrl->dev = &pdev->dev;
	ctrl->irq = irq;
	platform_set_drvdata(pdev, ctrl);
	mutex_init(&ctrl->access_ok);
	spin_lock_init(&ctrl->common_regs_lock);

	/* map registers.*/
	if (!devm_request_mem_region(ctrl->dev, res->start,
			resource_size(res), ctrl->name)) {
		dev_err(ctrl->dev,
			"can't request region for resource %pR\n", res);
		ret = -EINVAL;
		goto failed;
	}

	ctrl->reg_base = devm_ioremap_nocache(ctrl->dev,
			res->start, resource_size(res));
	if (ctrl->reg_base == NULL) {
		dev_err(ctrl->dev, "%s: res %lx - %lx map failed\n", __func__,
			(unsigned long)res->start, (unsigned long)res->end);
		ret = -ENOMEM;
		goto failed;
	}
	ctrl->regs_len = resource_size(res) / sizeof(u32);

	pm_runtime_enable(ctrl->dev);
	pm_runtime_forbid(ctrl->dev);

	ctrl->version = readl_relaxed(ctrl->reg_base + LCD_VERSION);

	/* register lcd internal clock firstly */
	if (mmp_display_clk_init(ctrl) < 0) {
		dev_err(ctrl->dev, "register clk failure\n");
		ret = -EINVAL;
		goto failed;
	}

	/* init global regs */
	ctrl_set_default(ctrl);

	/* init pathes from machine info and register them */
	for (i = 0; i < ctrl->path_num; i++) {
		/* get from config and machine info */
		path_plat = &ctrl->path_plats[i];
		path_plat->id = i;
		path_plat->ctrl = ctrl;

		/* path init */
		if (!path_init(path_plat, (paths_config + i))) {
			ret = -EINVAL;
			goto failed_path_init;
		}
	}

	for (i = 0; i < ctrl->path_num; i++) {
		path = ctrl->path_plats[i].path;
		path->master = NULL;
		path->slave = NULL;
		if (ctrl->master_path_name && ctrl->slave_path_name) {
			/* if current path is master path, assign to its master
			 *  member meanwhile, get the slave path and assign
			 *  to its slave member
			 */
			if (!strcmp(ctrl->master_path_name, path->name)) {
				path->master = path;
				path->slave =
					mmp_get_path(ctrl->slave_path_name);
			}
			/* if current path is salve path, assign to its slave
			 *  member meanwhile, get the master path and assign
			 *  to ist master member
			 */
			if (!strcmp(ctrl->slave_path_name, path->name)) {
				path->master =
					mmp_get_path(ctrl->master_path_name);
				path->slave = path;
			}
		}
	}

#ifdef CONFIG_MMP_DISP_SPI
	ret = lcd_spi_register(ctrl);
	if (ret < 0)
		goto failed_path_init;
#endif

	/* request irq */
	ret = devm_request_irq(ctrl->dev, ctrl->irq, ctrl_handle_irq,
		IRQF_SHARED, "lcd_controller", ctrl);
	if (ret < 0) {
		dev_err(ctrl->dev, "%s unable to request IRQ %d\n",
				__func__, ctrl->irq);
		ret = -ENXIO;
		goto failed_path_init;
	}

	ctrl->regs_store = devm_kzalloc(ctrl->dev,
			ctrl->regs_len * sizeof(u32), GFP_KERNEL);
	if (!ctrl->regs_store) {
		dev_err(ctrl->dev,
				"%s: unable to kzalloc memory for regs store\n",
				__func__);
		goto failed_path_init;
	}

	ret = ctrl_dbg_init(&pdev->dev);

	if (ret < 0) {
		dev_err(ctrl->dev, "%s: Failed to register ctrl dbg interface\n",
				__func__);
		goto failed_path_init;
	}

#ifdef CONFIG_MMP_DISP_DFC
	mmp_dfc_init(&pdev->dev);
#endif
	dev_info(ctrl->dev, "device init done\n");

	return 0;

failed_path_init:
	for (i = 0; i < ctrl->path_num; i++) {
		path_plat = &ctrl->path_plats[i];
		path_deinit(path_plat);
	}

	clk_disable_unprepare(ctrl->clk);
failed:
	dev_err(&pdev->dev, "device init failed\n");

	return ret;
}

static void mmphw_regs_store(struct mmphw_ctrl *ctrl)
{
	int i = 0;

	if (ctrl->regs_store)
		/* store registers */
		while (i < ctrl->regs_len) {
			ctrl->regs_store[i] =
				readl_relaxed(ctrl->reg_base + i * 4);
			i++;
		}
}

static void mmphw_regs_recovery(struct mmphw_ctrl *ctrl)
{
	int i = 0;

	if (ctrl->regs_store)
		/* recovery registers */
		while (i < ctrl->regs_len) {
			writel_relaxed(ctrl->regs_store[i],
				ctrl->reg_base + i * 4);
			i++;
		}
}

#if defined(CONFIG_PM_SLEEP) || defined(CONFIG_PM_RUNTIME)
static int mmphw_runtime_suspend(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct mmphw_ctrl *ctrl = platform_get_drvdata(pdev);
	struct mmp_path *path = ctrl->path_plats[0].path;
	struct mmp_vdma_info *vdma = path ? (path->overlays[0].vdma) : NULL;
	unsigned long flags;

	mutex_lock(&ctrl->access_ok);
	spin_lock_irqsave(&ctrl->common_regs_lock, flags);
	ctrl->status = MMP_OFF;
	mmphw_regs_store(ctrl);
	spin_unlock_irqrestore(&ctrl->common_regs_lock, flags);
	mutex_unlock(&ctrl->access_ok);

	if (vdma)
		vdma->ops->runtime_onoff(0);

	return 0;
}

static int mmphw_runtime_resume(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct mmphw_ctrl *ctrl = platform_get_drvdata(pdev);
	struct mmp_path *path = ctrl->path_plats[0].path;
	struct mmp_vdma_info *vdma = path ? (path->overlays[0].vdma) : NULL;
	unsigned long flags;

	mutex_lock(&ctrl->access_ok);
	spin_lock_irqsave(&ctrl->common_regs_lock, flags);
	mmphw_regs_recovery(ctrl);
	ctrl->status = MMP_ON;
	spin_unlock_irqrestore(&ctrl->common_regs_lock, flags);
	mutex_unlock(&ctrl->access_ok);

	if (vdma)
		vdma->ops->runtime_onoff(1);

	return 0;
}
#endif

const struct dev_pm_ops mmphw_pm_ops = {
	SET_RUNTIME_PM_OPS(mmphw_runtime_suspend,
		mmphw_runtime_resume, NULL)
};

static struct platform_driver mmphw_driver = {
	.driver		= {
		.name	= "mmp-disp",
		.owner	= THIS_MODULE,
		.pm	= &mmphw_pm_ops,
		.of_match_table = of_match_ptr(mmp_disp_dt_match),
	},
	.probe		= mmphw_probe,
};

static int mmphw_init(void)
{
	int ret;

	ret = mmp_vdma_register();
	if (ret < 0)
		return ret;

	ret = mmp_apical_register();
	if (ret < 0)
		goto apical_fail;

	ret = platform_driver_register(&mmphw_driver);
	if (ret < 0)
		goto ctrl_fail;

	ret = phy_dsi_register();
	if (ret < 0)
		goto phy_fail;

	return 0;

phy_fail:
	platform_driver_unregister(&mmphw_driver);
ctrl_fail:
	mmp_apical_unregister();
apical_fail:
	mmp_vdma_unregister();
	return ret;
}
module_init(mmphw_init);

MODULE_AUTHOR("Li Guoqing<ligq@marvell.com>");
MODULE_DESCRIPTION("Framebuffer driver for mmp");
MODULE_LICENSE("GPL");
