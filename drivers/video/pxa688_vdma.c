/*
 * linux/drivers/video/pxa168fb.c -- Marvell PXA168 LCD Controller
 *
 *  Copyright (C) 2008 Marvell International Ltd.
 *  All rights reserved.
 *
 *  2009-02-16  adapted from original version for PXA168
 *		Kevin Liu <kliu5@marvell.com>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of this archive for
 * more details.
 */

#include <linux/videodev2.h>

#ifdef CONFIG_PXA688_VDMA

#include "pxa168fb_common.h"
#include <mach/regs-apmu.h>
#include <mach/sram.h>

static DEFINE_SPINLOCK(ctrl_lock);

static struct pxa168fb_vdma_info vdma0 = {
	.ch = 0,
	.path = 0,
	.sram_paddr = 0,
	.sram_size = 0,
	.enable = 0,
};
static struct pxa168fb_vdma_info vdma1 = {
	.ch = 1,
	.path = 1,
	.sram_paddr = 0,
	.sram_size = 0,
	.enable = 0,
};

/* convert pix fmt to vmode */
static FBVideoMode pixfmt_to_vmode(int pix_fmt)
{
	switch (pix_fmt) {
	case PIX_FMT_RGB565:
	case V4L2_PIX_FMT_RGB565X:
		return FB_VMODE_RGB565;
	case PIX_FMT_BGR565:
		return FB_VMODE_BGR565;
	case PIX_FMT_RGB1555:
	case V4L2_PIX_FMT_RGB555X:
		return FB_VMODE_RGB1555;
	case PIX_FMT_BGR1555:
		return FB_VMODE_BGR1555;
	case PIX_FMT_RGB888PACK:
	case V4L2_PIX_FMT_RGB24:
		return FB_VMODE_RGB888PACK;
	case PIX_FMT_BGR888PACK:
	case V4L2_PIX_FMT_BGR24:
		return FB_VMODE_BGR888PACK;
	case PIX_FMT_RGB888UNPACK:
		return FB_VMODE_RGB888UNPACK;
	case PIX_FMT_BGR888UNPACK:
		return FB_VMODE_BGR888UNPACK;
	case PIX_FMT_RGBA888:
	case V4L2_PIX_FMT_RGB32:
		return FB_VMODE_RGBA888;
	case PIX_FMT_BGRA888:
	case V4L2_PIX_FMT_BGR32:
		return FB_VMODE_BGRA888;

	case PIX_FMT_YUV422PACK:
	case V4L2_PIX_FMT_UYVY:
		return FB_VMODE_YUV422PACKED;
	case PIX_FMT_YVU422PACK:
		return FB_VMODE_YUV422PACKED_SWAPUV;
	case PIX_FMT_YUV422PLANAR:
	case V4L2_PIX_FMT_YUV422P:
		return FB_VMODE_YUV422PLANAR;
	case PIX_FMT_YVU422PLANAR:
		return FB_VMODE_YUV422PLANAR_SWAPUV;
	case PIX_FMT_YUV420PLANAR:
	case V4L2_PIX_FMT_YUV420:
		return FB_VMODE_YUV420PLANAR;
	case PIX_FMT_YVU420PLANAR:
	case V4L2_PIX_FMT_YVU420:
		return FB_VMODE_YUV420PLANAR_SWAPUV;
	case PIX_FMT_YUYV422PACK:
	case V4L2_PIX_FMT_YUYV:
		return FB_VMODE_YUV422PACKED_SWAPYUorV;
	case PIX_FMT_YUV422PACK_IRE_90_270:
		return FB_VMODE_YUV422PACKED_IRE_90_270;
	default:
		return -1;
	};
}

#define vdma_ctrl(id)		(id ? VDMA_CTRL_2 : VDMA_CTRL_1)

static u32 lcd_pitch_read(struct pxa168fb_vdma_info *lcd_vdma)
{
	struct lcd_regs *regs = get_regs(lcd_vdma->path);
	u32 reg = (u32)&regs->g_pitch;

	if (lcd_vdma->vid)
		reg = (u32)&regs->v_pitch_yc;

	return __raw_readl(reg) & 0xffff;
}

static u32 lcd_height_read(struct pxa168fb_vdma_info *lcd_vdma)
{
	struct lcd_regs *regs = get_regs(lcd_vdma->path);
	u32 reg = (u32)&regs->g_size;

	if (lcd_vdma->vid)
		reg = (u32)&regs->v_size;

	return (__raw_readl(reg) & 0xfff0000) >> 16;
}

static u32 lcd_width_read(struct pxa168fb_vdma_info *lcd_vdma)
{
	struct lcd_regs *regs = get_regs(lcd_vdma->path);
	u32 reg = (u32)&regs->g_size;

	if (lcd_vdma->vid)
		reg = (u32)&regs->v_size;

	return __raw_readl(reg) & 0xfff;
}

static u32 vdma_ctrl_read(struct pxa168fb_vdma_info *lcd_vdma)
{
	u32 reg = (u32)lcd_vdma->reg_base + vdma_ctrl(lcd_vdma->ch);

	return __raw_readl(reg);
}

static int __get_vdma_rot_ctrl(int vmode, int angle, int yuv_format)
{
	int rotation, flag = 0, reg = 0;

	if (!angle || (angle == 1))
		return 0;

	switch (angle) {
	case 90:
		rotation = 1;
		break;
	case 270:
		rotation = 0;
		break;
	case 180:
		rotation = 2;
		break;
	default:
		rotation = 0;
		break;
		/* return rotation; */
	}

	switch (vmode) {
	case FB_VMODE_RGB565:
	case FB_VMODE_BGR565:
	case FB_VMODE_RGB1555:
	case FB_VMODE_BGR1555:
		reg = 4 << 2;
		reg |= rotation << 11;
		break;
	case FB_VMODE_YUV422PACKED:
	case FB_VMODE_YUV422PACKED_SWAPUV:
	case FB_VMODE_YUV422PACKED_SWAPYUorV:
		flag = 1;
	case FB_VMODE_YUV422PACKED_IRE_90_270:
		reg = 1 << 5;
		reg |= 1 << 7;
		reg |= 3 << 2;
		reg |= rotation << 11;
		break;
	default:
		reg = 6 << 2;
		reg |= rotation << 11;
		reg |= 0 << 7;
		break;
	}

	if (vmode == FB_VMODE_YUV422PACKED_IRE_90_270 || flag == 1) {
		if (rotation == 2)
			reg |= 2 << 21;
		else
			reg |= 1 << 21;

		if (vmode == FB_VMODE_YUV422PACKED)
			yuv_format = 1;
		if (vmode == FB_VMODE_YUV422PACKED_SWAPUV)
			yuv_format = 2;
		if (vmode == FB_VMODE_YUV422PACKED_SWAPYUorV)
			yuv_format = 4;

		switch (yuv_format) {
		case 1:/*YUV_FORMAT_UYVY*/
			reg |= 1 << 9;
			break;
		case 2:/*YUV_FORMAT_VYUY*/
			reg |= 0 << 9;
			break;
		case 3:/*YUV_FORAMT_YVYU*/
			reg |= 3 << 9;
			break;
		case 4:/*YUV_FORMAT_YUYV*/
			reg |= 2 << 9;
			break;
		}
	}

	return reg;
}

static int __get_vdma_src_sz(struct pxa168fb_vdma_info *lcd_vdma,
		int vmode, int width, int height)
{
	int res = lcd_pitch_read(lcd_vdma) * height;

	if (vmode == FB_VMODE_YUV422PACKED_IRE_90_270)
		return res >> 1;
	else
		return res;
}

static int __get_vdma_sa(struct pxa168fb_vdma_info *lcd_vdma, int vmode,
		int width, int height, int bpp, int rotation)
{
	struct lcd_regs *regs = get_regs(lcd_vdma->path);
	int addr = 0;

	if (vmode == FB_VMODE_YUV422PACKED_IRE_90_270)
		bpp = 2;

	if (lcd_vdma->vid)
		addr = readl(&regs->v_y0);
	else
		addr = readl(&regs->g_0);

	switch (rotation) {
	case 90:
		break;
	case 180:
		addr += width * height * bpp - 1;
		break;
	case 270:
		addr += height * (width - 1) * bpp;
		break;
	}
	return addr;

}

static int __get_vdma_sz(struct pxa168fb_vdma_info *lcd_vdma, int vmode,
		int rotation, int width, int height, int bpp)
{
	int src_pitch = lcd_pitch_read(lcd_vdma);

	if (vmode == FB_VMODE_YUV422PACKED_IRE_90_270)
		bpp = 2;
	switch (rotation) {
	case 0:
	case 1:
		return height << 16 | src_pitch;
	case 90:
		return width << 16 | height;
	case 180:
		return width | height << 16;
	case 270:
		return width << 16 | height;
	}
	return 0;

}

static int __get_vdma_pitch(struct pxa168fb_vdma_info *lcd_vdma, int vmode,
		int rotation, int width, int height, int bpp)
{
	int src_bpp = bpp, src_pitch = lcd_pitch_read(lcd_vdma);

	if (vmode == FB_VMODE_YUV422PACKED_IRE_90_270)
		src_bpp = 2;
	switch (rotation) {
	case 0:
	case 1:
		return src_pitch | (width * bpp << 16);
	case 90:
		return (width * bpp) << 16 | (height * src_bpp);
	case 180:
		return width * src_bpp | (width * bpp << 16);
	case 270:
		return (width * bpp) << 16 | (height * src_bpp);
	}
	return 0;

}

static int __get_vdma_ctrl(struct pxa168fb_vdma_info *lcd_vdma,
				int rotation, int line)
{
	if (!rotation || (rotation == 1))
		return (line << 8) | 0xa1;
	else
		return (line << 8) | 0xa5;
}

static void pxa688_vdma_clkset(int en)
{
	if (en)
		writel(readl(APMU_LCD2_CLK_RES_CTRL) | 0x11b,
				APMU_LCD2_CLK_RES_CTRL);
}

static int pxa688_vdma_get_linenum(struct pxa168fb_vdma_info *lcd_vdma)
{
	int mulfactor, lines, lines_exp;
	int pitch = lcd_pitch_read(lcd_vdma);
	int height = lcd_height_read(lcd_vdma);
	int angle = lcd_vdma->rotation;

	if (!pitch)
		return 0;
	if (!angle || (angle == 1))
		/* no rotation */
		mulfactor = 2;
	else
		mulfactor = 16;
	lines = (lcd_vdma->sram_size / pitch)&(~(mulfactor-1));

	if (lines < 2)
		return 0; /* at least 2 lines*/
	if (lines > 64)
		lines = 64;

	for (lines_exp = 0; lines_exp < lines; lines_exp += mulfactor) {
		if (height%(lines-lines_exp) == 0)
			break;
	}
	if (lines_exp >= lines)
		return 32;
	lines -= lines_exp;
	pr_debug("lines %d lines_exp %d angle %d mulfactor %d height %d"
		" pitch %d sram_size %d\n", lines, lines_exp, angle,
		mulfactor, height, pitch, lcd_vdma->sram_size);
	return lines;
}

static void pxa688_vdma_set(struct pxa168fb_vdma_info *lcd_vdma)
{
	struct vdma_regs *vdma = (struct vdma_regs *)((u32)lcd_vdma->reg_base
			+ VDMA_ARBR_CTRL);
	struct vdma_ch_regs *vdma_ch = lcd_vdma->ch ? &vdma->ch2 : &vdma->ch1;
	u32 psqu = lcd_vdma->sram_paddr;
	unsigned int lines = lcd_vdma->vdma_lines;
	int pix_fmt = lcd_vdma->pix_fmt;
	int rotation = lcd_vdma->rotation;
	unsigned format = lcd_vdma->yuv_format;
	unsigned int reg, width, height, bpp;
	FBVideoMode vmode;

	if (lines < 2) {
		pr_warn("%s lines = %d < 2???\n", __func__, lines);
		return;
	}

	width = lcd_width_read(lcd_vdma);
	height = lcd_height_read(lcd_vdma);
	bpp = lcd_pitch_read(lcd_vdma) / width;
	vmode = pixfmt_to_vmode(pix_fmt);
	if (vmode < 0) {
		pr_err("%s pix_fmt %d not supported\n", __func__, pix_fmt);
		return;
	}

	if (!psqu) {
		pxa688_vdma_release(lcd_vdma->path, lcd_vdma->vid);
		return;
	} else {
		/* select video layer or graphics layer */
		reg = readl(lcd_vdma->reg_base + LCD_PN2_SQULN2_CTRL);
		if (lcd_vdma->vid)
			reg |= 1 << (24 + lcd_vdma->path);
		else
			reg &= ~(1 << (24 + lcd_vdma->path));
		writel(reg, lcd_vdma->reg_base + LCD_PN2_SQULN2_CTRL);
	}
	reg = (u32)psqu | ((lines/2-1)<<1 | 0x1);
	writel(reg, lcd_vdma->reg_base + squln_ctrl(lcd_vdma->path));

	pr_debug("%s psqu %x reg %x, width %d height %d bpp %d lines %d\n",
		__func__, psqu, reg, width, height, bpp, lines);

	/* src/dst addr */
	reg = __get_vdma_sa(lcd_vdma, vmode, width, height, bpp, rotation);
	writel(reg, &vdma_ch->src_addr);
	writel((u32)psqu, &vdma_ch->dst_addr);

	/* source size */
	reg = __get_vdma_src_sz(lcd_vdma, vmode, width, height);
	writel(reg, &vdma_ch->src_size);

	/* size */
	reg = __get_vdma_sz(lcd_vdma, vmode, rotation, width, height, bpp);
	writel(reg, &vdma_ch->dst_size);

	/* pitch */
	reg = __get_vdma_pitch(lcd_vdma, vmode, rotation, width, height, bpp);
	writel(reg, &vdma_ch->pitch);

	/* rotation ctrl */
	reg = __get_vdma_rot_ctrl(vmode, rotation, format);
	writel(reg, &vdma_ch->rot_ctrl);

	if (vmode == FB_VMODE_YUV422PACKED_SWAPYUorV && rotation == 180) {
		reg = dma_ctrl_read(lcd_vdma->path, 0);
		reg &= ~(0x1 << 2);
		dma_ctrl_write(lcd_vdma->path, 0, reg);
	}

	/* control */
	reg = __get_vdma_ctrl(lcd_vdma, rotation, lines);
	writel(reg, &vdma_ch->ctrl);
}

static void vdma_sel_config(struct pxa168fb_vdma_info *lcd_vdma)
{
	u32 reg = readl(lcd_vdma->reg_base + LCD_PN2_SQULN2_CTRL);

	switch (lcd_vdma->path) {
	case 0:
		reg &= ~(1 << 30);
		break;
	case 1:
		reg &= ~(1 << 31);
		break;
	case 2:
		reg |= 1 << (lcd_vdma->ch ? 31 : 30);
	default:
		break;
	}
	writel(reg, lcd_vdma->reg_base + LCD_PN2_SQULN2_CTRL);
}

struct pxa168fb_vdma_info *request_vdma(int path, int vid)
{
	struct pxa168fb_vdma_info *vdma = 0;

	if ((vdma0.path == path) && (vdma0.vid == vid) && vdma0.enable)
		return &vdma0;
	else if ((vdma1.path == path) && (vdma1.vid == vid) && vdma1.enable)
		return &vdma1;

	switch (path) {
	case 0:
		vdma = vdma0.enable ? 0 : &vdma0;
		break;
	case 1:
		vdma = vdma1.enable ? 0 : &vdma1;
		break;
	case 2:
		if (!vdma1.enable)
			vdma = &vdma1;
		else if (!vdma0.enable)
			vdma = &vdma0;
	default:
		break;
	}

	if (vdma) {
		vdma->path = path;
		vdma->vid = vid;
	}
	return vdma;
}

void pxa688_vdma_config(struct pxa168fb_vdma_info *lcd_vdma)
{
	pr_debug("%s path %d vid %d vdma_enable %d active %d\n",
		__func__, lcd_vdma->path, lcd_vdma->vid,
		lcd_vdma->enable, lcd_vdma->active);
	if (lcd_vdma->enable && lcd_vdma->active) {
		if (lcd_vdma->dma_on) {
			lcd_vdma->vdma_lines =
				pxa688_vdma_get_linenum(lcd_vdma);
			pxa688_vdma_set(lcd_vdma);
		} else
			pxa688_vdma_release(lcd_vdma->path, lcd_vdma->vid);
	}
}

void pxa688_vdma_init(struct pxa168fb_vdma_info *lcd_vdma)
{
	struct pxa168fb_mach_info *mi = lcd_vdma->dev->platform_data;
	struct gen_pool *pool;

	if (mi->vdma_enable) {
		lcd_vdma->sram_size = mi->sram_size;
		pool = sram_get_gpool("isram");
		lcd_vdma->sram_vaddr = gen_pool_alloc(pool, mi->sram_size);
		lcd_vdma->sram_paddr = gen_pool_virt_to_phys(pool,
				lcd_vdma->sram_vaddr);
		if (lcd_vdma->sram_paddr) {
			vdma_sel_config(lcd_vdma);
			lcd_vdma->enable = mi->vdma_enable;
			pr_info("vdma enabled, sram_paddr 0x%x sram_size 0x%x\n",
				lcd_vdma->sram_paddr, mi->sram_size);
		} else
			pr_warn("%s path %d vdma enable failed\n",
				__func__, lcd_vdma->path);
	} else if (!lcd_vdma->sram_size)
		lcd_vdma->sram_size = mi->sram_size;

	pxa688_vdma_clkset(1);
}

void pxa688_vdma_release(int path, int vid)
{
	struct pxa168fb_vdma_info *lcd_vdma = request_vdma(path, vid);
	struct vdma_regs *vdma;
	struct vdma_ch_regs *vdma_ch;
	unsigned reg; /*, isr, current_time, irq_mask; */

	if (!lcd_vdma)
		return;

	vdma = (struct vdma_regs *)((u32)lcd_vdma->reg_base
			+ VDMA_ARBR_CTRL);
	vdma_ch = lcd_vdma->ch ? &vdma->ch2 : &vdma->ch1;

	if (!(vdma_ctrl_read(lcd_vdma) & 1))
		return;
#if 0
	isr = readl(lcd_vdma->reg_base + SPU_IRQ_ISR);
	if (lcd_vdma->ch == 0)
		irq_mask = DMA_FRAME_IRQ0_MASK | DMA_FRAME_IRQ1_MASK;
	else if (lcd_vdma->ch == 1)
		irq_mask = TV_DMA_FRAME_IRQ0_MASK | TV_DMA_FRAME_IRQ1_MASK;
	else
		irq_mask = PN2_DMA_FRAME_IRQ0_MASK | PN2_DMA_FRAME_IRQ1_MASK;
	irq_status_clear(lcd_vdma->ch, irq_mask);
	current_time = jiffies;
	while ((readl(lcd_vdma->reg_base + SPU_IRQ_ISR) & irq_mask) == 0) {
		if (jiffies_to_msecs(jiffies - current_time) > EOF_TIMEOUT) {
			pr_err("EOF not detected !");
			break;
		}
	}
#endif

	reg = readl(&vdma_ch->ctrl);
	reg &= ~0xF;
	writel(reg, &vdma_ch->ctrl);

	/* disable squ access */
	reg = readl(lcd_vdma->reg_base + squln_ctrl(lcd_vdma->path));
	reg &= (~0x1);
	writel(reg, lcd_vdma->reg_base + squln_ctrl(lcd_vdma->path));
#ifdef CONFIG_PXA688_MISC
	if (lcd_vdma->path == fb_vsmooth && (gfx_vsmooth || vid_vsmooth))
		writel(reg, lcd_vdma->reg_base + squln_ctrl(fb_filter));
#endif
	printk(KERN_DEBUG "%s path %d squln_ctrl %x\n",
			__func__, lcd_vdma->path,
			readl(lcd_vdma->reg_base + squln_ctrl(lcd_vdma->path)));
}

int pxa688_vdma_en(struct pxa168fb_vdma_info *lcd_vdma, int enable, int vid)
{
	u32 reg;
	unsigned long flags = 0;
	struct gen_pool *pool;

	if ((lcd_vdma->path == 1) && enable) {
		reg = dma_ctrl_read(lcd_vdma->path, 1);
		if (reg & (CFG_TV_INTERLACE_EN | CFG_TV_NIB))
			return -EFAULT;
	}

	if (enable) {
		if ((lcd_vdma->enable) && (lcd_vdma->vid == vid))
			return 0;
		reg = readl(lcd_vdma->reg_base + squln_ctrl(lcd_vdma->path));
		if ((reg & 0x1) || lcd_vdma->enable) {
			pr_warn("%s vdma has been enabled for"
				" another layer\n", __func__);
			return -EINVAL;
		}
		vdma_sel_config(lcd_vdma);
		pool = sram_get_gpool("isram");
		lcd_vdma->sram_vaddr = gen_pool_alloc(pool,
				lcd_vdma->sram_size);
		lcd_vdma->sram_paddr = gen_pool_virt_to_phys(pool,
				lcd_vdma->sram_vaddr);

		if (lcd_vdma->sram_paddr) {
			lcd_vdma->vid = vid;
			lcd_vdma->enable = 1;
		} else {
			pr_warn("%s path %d vdma enable failed\n",
				__func__, lcd_vdma->path);
			return -EFAULT;
		}
	} else {
		if (!lcd_vdma->enable || (lcd_vdma->vid != vid))
			return 0;
		spin_lock_irqsave(&ctrl_lock, flags);
		pxa688_vdma_release(lcd_vdma->path, lcd_vdma->vid);
		lcd_vdma->enable = 0;
		spin_unlock_irqrestore(&ctrl_lock, flags);
		pool = sram_get_gpool("isram");
		gen_pool_free(pool, lcd_vdma->sram_vaddr, lcd_vdma->sram_size);
		lcd_vdma->sram_paddr = 0;
		lcd_vdma->sram_vaddr = 0;
	}

	pr_debug("path %d %s layer, vdma %d %s, sram size:0x%x,\
			vaddr:0x%x, paddr:0x%x\n",
			lcd_vdma->path, lcd_vdma->vid ? "vid" : "gfx",
			lcd_vdma->ch, lcd_vdma->enable ? "enabled" : "disabled",
			lcd_vdma->sram_size, lcd_vdma->sram_vaddr,
			lcd_vdma->sram_paddr);
	return 0;
}

static ssize_t vdma_help(char *buf)
{
	int s = 0, f = DUMP_SPRINTF;

	mvdisp_dump(f, "commands:\n");
	mvdisp_dump(f, " - dump VDMA configuration, registers\n");
	mvdisp_dump(f, "\tcat vdma\n");
	mvdisp_dump(f, " - enable[1]/disable[0] vdma for path(pn/tv/pn2:"
			"[0/1/2]) graphics/video[0/1] layer\n");
	mvdisp_dump(f, "\techo e [path:0/1/2] [layer:0/1]"
			" [en/dis:1/0] > vdma\n");
	mvdisp_dump(f, " - configure vdma channel sram size with KB unit\n");
	mvdisp_dump(f, "\techo s [vdma_ch0_size](KB)"
			" [vdma_ch1_size](KB) > vdma\n");

	return s;
}

ssize_t vdma_show(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	struct pxa168fb_info *fbi = dev_get_drvdata(dev);
	struct vdma_regs *vdma = (struct vdma_regs *)((u32)fbi->reg_base
			+ VDMA_ARBR_CTRL);
	int s = 0, f = DUMP_SPRINTF;

	mvdisp_dump(f, "vdma0:\n");
	mvdisp_dump(f, "\tlcd_vdma->ch:     %d\n", vdma0.ch);
	mvdisp_dump(f, "\tlcd_vdma->path:   %d\n", vdma0.path);
	mvdisp_dump(f, "\tlcd_vdma->vid:    %d\n", vdma0.vid);
	mvdisp_dump(f, "\tlcd_vdma->enable: %d\n", vdma0.enable);
	mvdisp_dump(f, "\tvdma_lines        %d\n", vdma0.vdma_lines);
	mvdisp_dump(f, "\tsram_paddr        0x%x\n", vdma0.sram_paddr);
	mvdisp_dump(f, "\tsram_size         %dKB\n", vdma0.sram_size/1024);

	mvdisp_dump(f, "vdma1:\n");
	mvdisp_dump(f, "\tlcd_vdma->ch:     %d\n", vdma1.ch);
	mvdisp_dump(f, "\tlcd_vdma->path:   %d\n", vdma1.path);
	mvdisp_dump(f, "\tlcd_vdma->vid:    %d\n", vdma1.vid);
	mvdisp_dump(f, "\tlcd_vdma->enable: %d\n", vdma1.enable);
	mvdisp_dump(f, "\tvdma_lines     %d\n", vdma1.vdma_lines);
	mvdisp_dump(f, "\tsram_paddr     0x%x\n", vdma1.sram_paddr);
	mvdisp_dump(f, "\tsram_size      %dKB\n", vdma1.sram_size/1024);

	mvdisp_dump(f, "\nregister base: 0x%p\n", fbi->reg_base);
	mvdisp_dump(f, "\tarbr_ctr       (@%3x):\t0x%x\n",
		 (int)(&vdma->arbr_ctr)&0xfff, readl(&vdma->arbr_ctr));
	mvdisp_dump(f, "\tirq_raw        (@%3x):\t0x%x\n",
		 (int)(&vdma->irq_raw)&0xfff, readl(&vdma->irq_raw));
	mvdisp_dump(f, "\tirq_mask       (@%3x):\t0x%x\n",
		 (int)(&vdma->irq_mask)&0xfff, readl(&vdma->irq_mask));
	mvdisp_dump(f, "\tirq_status     (@%3x):\t0x%x\n",
		 (int)(&vdma->irq_status)&0xfff, readl(&vdma->irq_status));
	mvdisp_dump(f, "\tmdma_arbr_ctrl (@%3x):\t0x%x\n",
	 (int)(&vdma->mdma_arbr_ctrl)&0xfff, readl(&vdma->mdma_arbr_ctrl));

	mvdisp_dump(f, "  channel 1\n");
	mvdisp_dump(f, "\tdc_saddr       (@%3x):\t0x%x\n",
		 (int)(&vdma->ch1.dc_saddr)&0xfff, readl(&vdma->ch1.dc_saddr));
	mvdisp_dump(f, "\tdc_size        (@%3x):\t0x%x\n",
		 (int)(&vdma->ch1.dc_size)&0xfff, readl(&vdma->ch1.dc_size));
	mvdisp_dump(f, "\tctrl           (@%3x):\t0x%x\n",
		 (int)(&vdma->ch1.ctrl)&0xfff, readl(&vdma->ch1.ctrl));
	mvdisp_dump(f, "\tsrc_size       (@%3x):\t0x%x\n",
		 (int)(&vdma->ch1.src_size)&0xfff, readl(&vdma->ch1.src_size));
	mvdisp_dump(f, "\tsrc_addr       (@%3x):\t0x%x\n",
		 (int)(&vdma->ch1.src_addr)&0xfff, readl(&vdma->ch1.src_addr));
	mvdisp_dump(f, "\tdst_addr       (@%3x):\t0x%x\n",
		 (int)(&vdma->ch1.dst_addr)&0xfff, readl(&vdma->ch1.dst_addr));
	mvdisp_dump(f, "\tdst_size       (@%3x):\t0x%x\n",
		 (int)(&vdma->ch1.dst_size)&0xfff, readl(&vdma->ch1.dst_size));
	mvdisp_dump(f, "\tpitch          (@%3x):\t0x%x\n",
		 (int)(&vdma->ch1.pitch)&0xfff, readl(&vdma->ch1.pitch));
	mvdisp_dump(f, "\trot_ctrl       (@%3x):\t0x%x\n",
		 (int)(&vdma->ch1.rot_ctrl)&0xfff, readl(&vdma->ch1.rot_ctrl));
	mvdisp_dump(f, "\tram_ctrl0      (@%3x):\t0x%x\n",
	 (int)(&vdma->ch1.ram_ctrl0)&0xfff, readl(&vdma->ch1.ram_ctrl0));
	mvdisp_dump(f, "\tram_ctrl1      (@%3x):\t0x%x\n",
	 (int)(&vdma->ch1.ram_ctrl1)&0xfff, readl(&vdma->ch1.ram_ctrl1));

	mvdisp_dump(f, "  channel 2\n");
	mvdisp_dump(f, "\tdc_saddr       (@%3x):\t0x%x\n",
		 (int)(&vdma->ch2.dc_saddr)&0xfff, readl(&vdma->ch2.dc_saddr));
	mvdisp_dump(f, "\tdc_size        (@%3x):\t0x%x\n",
		 (int)(&vdma->ch2.dc_size)&0xfff, readl(&vdma->ch2.dc_size));
	mvdisp_dump(f, "\tctrl           (@%3x):\t0x%x\n",
		 (int)(&vdma->ch2.ctrl)&0xfff, readl(&vdma->ch2.ctrl));
	mvdisp_dump(f, "\tsrc_size       (@%3x):\t0x%x\n",
		 (int)(&vdma->ch2.src_size)&0xfff, readl(&vdma->ch2.src_size));
	mvdisp_dump(f, "\tsrc_addr       (@%3x):\t0x%x\n",
		 (int)(&vdma->ch2.src_addr)&0xfff, readl(&vdma->ch2.src_addr));
	mvdisp_dump(f, "\tdst_addr       (@%3x):\t0x%x\n",
		 (int)(&vdma->ch2.dst_addr)&0xfff, readl(&vdma->ch2.dst_addr));
	mvdisp_dump(f, "\tdst_size       (@%3x):\t0x%x\n",
		 (int)(&vdma->ch2.dst_size)&0xfff, readl(&vdma->ch2.dst_size));
	mvdisp_dump(f, "\tpitch          (@%3x):\t0x%x\n",
		 (int)(&vdma->ch2.pitch)&0xfff, readl(&vdma->ch2.pitch));
	mvdisp_dump(f, "\trot_ctrl       (@%3x):\t0x%x\n",
		 (int)(&vdma->ch2.rot_ctrl)&0xfff, readl(&vdma->ch2.rot_ctrl));
	mvdisp_dump(f, "\tram_ctrl0      (@%3x):\t0x%x\n",
	 (int)(&vdma->ch2.ram_ctrl0)&0xfff, readl(&vdma->ch2.ram_ctrl0));
	mvdisp_dump(f, "\tram_ctrl1      (@%3x):\t0x%x\n",
	 (int)(&vdma->ch2.ram_ctrl1)&0xfff, readl(&vdma->ch2.ram_ctrl1));

	mvdisp_dump(f, "  display controller related\n");
	mvdisp_dump(f, "\tsquln_ctrl     (@%3x):\t0x%x\n",
		 (int)(fbi->reg_base + squln_ctrl(fbi->id))&0xfff,
		 readl(fbi->reg_base + squln_ctrl(fbi->id)));
	mvdisp_dump(f, "\tpn2_squln2_ctrl(@%3x):\t0x%x\n\n",
		 (int)(fbi->reg_base + LCD_PN2_SQULN2_CTRL)&0xfff,
		 readl(fbi->reg_base + LCD_PN2_SQULN2_CTRL));

	s += vdma_help(buf + s);

	return s;
}

#define MMP2_VDMA_SIZE 90
#define MMP3_VDMA_SIZE 64
ssize_t vdma_store(
		struct device *dev, struct device_attribute *attr,
		const char *buf, size_t size)
{
	struct pxa168fb_vdma_info *lcd_vdma = 0;
	unsigned int path, vid, enable, tmp0, tmp1, vdma_size = 0;
	char vol[10];

	if ('s' == buf[0]) {
		if (vdma0.enable || vdma1.enable) {
			pr_err("vdma must disabled first before set size\n");
			return size;
		}
		memcpy(vol, (void *)((u32)buf + 1), size - 1);
		if (sscanf(vol, "%u %u", &tmp0, &tmp1) != 2) {
			pr_err("vdma sram size setting cmd should be like: "
				"s [vdma_ch0_size](KB) [vdma_ch1_size](KB)\n");
			return size;
		}
#ifdef CONFIG_CPU_MMP2
		vdma_size = MMP2_VDMA_SIZE;
#endif
#ifdef CONFIG_CPU_MMP3
		vdma_size = MMP3_VDMA_SIZE;
#endif
		if ((tmp0 + tmp1) > vdma_size) {
			pr_err("size exceed the max size of video sram\n");
			return size;
		}
		vdma0.sram_size = tmp0 * 1024;
		vdma1.sram_size = tmp1 * 1024;
		pr_info("vdma0 size 0x%x, vdma1 size 0x%x\n",
			vdma0.sram_size, vdma1.sram_size);
	} else if ('e' == buf[0]) {
		memcpy(vol, (void *)((u32)buf + 1), size - 1);
		if (sscanf(vol, "%u %u %u", &path,
			&vid, &enable) != 3) {
			pr_err("enable/disable vdma cmd should be like: "
				"e [0/1/2](pn/tv/pn2 path) [0/1](graphics/"
				"video layer) [1/0](enable/disable)\n");
			return size;
		}
		lcd_vdma = request_vdma(path, vid);
		if (!lcd_vdma) {
			if (enable)
				pr_err("request fail, vdma is occupied!\n");
			return size;
		}
		if (!lcd_vdma->sram_size && enable) {
			pr_err("ERR: SRAM size is 0KB!!!!\n");
			return size;
		}
		pxa688_vdma_en(lcd_vdma, enable, vid);
	}

	return size;
}
DEVICE_ATTR(vdma, S_IRUGO | S_IWUSR, vdma_show, vdma_store);
#endif
