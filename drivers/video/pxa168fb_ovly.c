/*
 * linux/drivers/video/pxa168fb_ovly.c -- Marvell PXA168 LCD Controller
 *
 * Copyright (C) Marvell Semiconductor Company.  All rights reserved.
 *
 * 2009-03-19   adapted from original version for PXA168
 *		Green Wan <gwan@marvell.com>
 *		Kevin Liu <kliu5@marvell.com>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of this archive for
 * more details.
 */

/*
 * 1. Adapted from:  linux/drivers/video/skeletonfb.c
 * 2. Merged from: linux/drivers/video/dovefb.c (Lennert Buytenhek)
 */
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/ioport.h>
#include <linux/cpufreq.h>
#include <linux/platform_device.h>
#include <linux/dma-mapping.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/uaccess.h>
#include <linux/console.h>
#include <linux/timer.h>

#include "pxa168fb_common.h"

#ifdef OVLY_DVFM_CONSTRAINT
static int dvfm_dev_idx;
#include <mach/dvfm.h>
#endif

#define RESET_BUF	0x1
#define FREE_ENTRY	0x2

static int pxa168fb_set_par(struct fb_info *fi);
static int pxa168fb_pan_display(struct fb_var_screeninfo *var,
				 struct fb_info *fi);

static unsigned int max_fb_size = 0;
static unsigned int fb_size_from_cmd = 0;
static struct _sOvlySurface gOvlySurface;

#define xp		pr_info("%s, line %d\n", __func__, __LINE__)
struct fbi_info ovly_info;

#ifdef FB_PM_DEBUG
static unsigned int g_regs[1024];
static unsigned int g_regs1[1024];
static unsigned int pxa168fb_rw_all_regs(struct pxa168fb_info *fbi,
		unsigned int *regs, int is_read)
{
	u32 i;
	u32 reg;

	for (i = 0xC0; i <= 0x01C4; i += 4) {
		if (is_read) {
			reg = readl(fbi->reg_base + i);
			regs[i] = reg;
		} else {
			writel(regs[i], fbi->reg_base + i);
		}
	}

	return 0;
}
#endif

#if 0
static struct fb_videomode *
find_best_mode(struct pxa168fb_info *fbi, struct fb_var_screeninfo *var)
{
	struct pxa168fb_mach_info *mi = fbi->dev->platform_data;
	struct fb_videomode *best_mode;
	int i;

	dev_dbg(fbi->fb_info->dev, "Enter %s\n", __func__);
	best_mode = NULL;
	for (i = 0; i < mi->num_modes; i++) {
		struct fb_videomode *m = mi->modes + i;

		/*
		 * Check whether this mode is suitable.
		 */
		if (var->xres > m->xres)
			continue;
		if (var->yres > m->yres)
			continue;

		/*
		 * Check whether this mode is more suitable than
		 * the best mode so far.
		 */
		if (best_mode != NULL &&
		    (best_mode->xres < m->xres ||
		     best_mode->yres < m->yres ||
		     best_mode->pixclock > m->pixclock))
			continue;

		best_mode = m;
	}

	return best_mode;
}
#endif

static u32 pxa168fb_ovly_set_colorkeyalpha(struct pxa168fb_info *fbi)
{
	struct _sColorKeyNAlpha color_a = fbi->ckey_alpha;
	unsigned int rb, x, layer, dma0, shift, r, b;
	struct pxa168fb_mach_info *mi;
	struct lcd_regs *regs;

	dev_dbg(fbi->fb_info->dev, "Enter %s\n", __func__);

	mi = fbi->dev->platform_data;
	regs = get_regs(fbi->id);
	dma0 = dma_ctrl_read(fbi->id, 0);
	shift = fbi->id ? 20 : 18;
	rb = layer = 0;
	r = color_a.Y_ColorAlpha;
	b = color_a.V_ColorAlpha;

	/* reset to 0x0 to disable color key. */
	x = dma_ctrl_read(fbi->id, 1) & ~(CFG_COLOR_KEY_MASK |
			CFG_ALPHA_MODE_MASK | CFG_ALPHA_MASK);

	/* switch to color key mode */
	switch (color_a.mode) {
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
		pr_info("V colorkey not supported, Chroma key instead\n");
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
		pr_info("G colorkey not supported, Luma key instead\n");
		break;
	case FB_ENABLE_B_COLORKEY_MODE:
		x |= CFG_COLOR_KEY_MODE(0x7);
		rb = 1;
		break;
	default:
		pr_info("unknown mode");
		return -1;
	}


	/* switch to alpha path selection */
	switch (color_a.alphapath) {
	case FB_VID_PATH_ALPHA:
		x |= CFG_ALPHA_MODE(0x0);
		layer = CFG_CKEY_DMA;
		if (rb)
			rb = ((dma0 & CFG_DMA_SWAPRB_MASK) >> 4) ^
				(mi->panel_rbswap);
		break;
	case FB_GRA_PATH_ALPHA:
		x |= CFG_ALPHA_MODE(0x1);
		layer = CFG_CKEY_GRA;
		if (rb)
			rb = ((dma0 & CFG_GRA_SWAPRB_MASK) >> 12) ^
				(mi->panel_rbswap);
		break;
	case FB_CONFIG_ALPHA:
		x |= CFG_ALPHA_MODE(0x2);
		rb = 0;
		break;
	default:
		pr_info("unknown alpha path");
		return -1;
	}

	/* check whether DMA turn on RB swap for this pixelformat. */
	if (rb) {
		if (color_a.mode == FB_ENABLE_R_COLORKEY_MODE) {
			x &= ~CFG_COLOR_KEY_MODE(0x1);
			x |= CFG_COLOR_KEY_MODE(0x7);
		}

		if (color_a.mode == FB_ENABLE_B_COLORKEY_MODE) {
			x &= ~CFG_COLOR_KEY_MODE(0x7);
			x |= CFG_COLOR_KEY_MODE(0x1);
		}

		/* exchange r b fields. */
		r = color_a.V_ColorAlpha;
		b = color_a.Y_ColorAlpha;

		/* only alpha_Y take effect, switch back from V */
		if (color_a.mode == FB_ENABLE_RGB_COLORKEY_MODE) {
			r &= 0xffffff00;
			r |= (color_a.Y_ColorAlpha & 0xff);
		}
	}

	/* configure alpha */
	x |= CFG_ALPHA((color_a.config & 0xff));
	dma_ctrl_write(fbi->id, 1, x);
	writel(r, &regs->v_colorkey_y);
	writel(color_a.U_ColorAlpha, &regs->v_colorkey_u);
	writel(b, &regs->v_colorkey_v);

	if (fbi->id != 2) {
		/* enable DMA colorkey on graphics/video layer
		 * in panel/TV path */
		x = readl(fbi->reg_base + LCD_TV_CTRL1);
		x &= ~(3<<shift); x |= layer<<shift;
		writel(x, fbi->reg_base + LCD_TV_CTRL1);
	}

	return 0;
}

static void pxa168fb_clear_framebuffer(struct fb_info *fi)
{
	struct pxa168fb_info *fbi = fi->par;

	memset(fbi->fb_start, 0, fbi->fb_size);
}

#ifdef CONFIG_DYNAMIC_PRINTK_DEBUG
static void debug_identify_called_ioctl(struct fb_info *fi, int cmd,
	 unsigned long arg)
{
	switch (cmd) {
	case FB_IOCTL_CLEAR_FRAMEBUFFER:
		dev_dbg(fi->dev, "FB_IOCTL_CLEAR_FRAMEBUFFER\n");
		break;
	case FB_IOCTL_WAIT_VSYNC:
		dev_dbg(fi->dev, "FB_IOCTL_WAIT_VSYNC\n");
		break;
	case FB_IOCTL_GET_VIEWPORT_INFO:
		dev_dbg(fi->dev, "FB_IOCTL_GET_VIEWPORT_INFO with arg =\
			 %08x\n", (unsigned int)arg);
		break;
	case FB_IOCTL_SET_VIEWPORT_INFO:
		dev_dbg(fi->dev, "FB_IOCTL_SET_VIEWPORT_INFO with arg =\
			 %08x\n", (unsigned int)arg);
		break;
	case FB_IOCTL_SET_VIDEO_MODE:
		dev_dbg(fi->dev, "FB_IOCTL_SET_VIDEO_MODE with arg =\
			 %08x\n", (unsigned int)arg);
		break;
	case FB_IOCTL_GET_VIDEO_MODE:
		dev_dbg(fi->dev, "FB_IOCTL_GET_VIDEO_MODE with arg =\
			 %08x\n", (unsigned int)arg);
		break;
	case FB_IOCTL_FLIP_VID_BUFFER:
		dev_dbg(fi->dev, "FB_IOCTL_FLIP_VID_BUFFER with arg =\
			 %08x\n", (unsigned int)arg);
		break;

	case FB_IOCTL_FLIP_VSYNC:
		dev_dbg(fi->dev, "FB_IOCTL_FLIP_VSYNC with arg =\
			 %08x\n", (unsigned int)arg);
		break;

	case FB_IOCTL_GET_FREELIST:
		dev_dbg(fi->dev, "FB_IOCTL_GET_FREELIST with arg =\
			 %08x\n", (unsigned int)arg);
		break;
	case FB_IOCTL_GET_BUFF_ADDR:
		dev_dbg(fi->dev, "FB_IOCTL_GET_BUFF_ADDR with arg =\
			 %08x\n", (unsigned int)arg);
		break;
	case FB_IOCTL_SET_VID_OFFSET:
		dev_dbg(fi->dev, "FB_IOCTL_SET_VID_OFFSET with arg =\
			 %08x\n", (unsigned int)arg);
		break;
	case FB_IOCTL_GET_VID_OFFSET:
		dev_dbg(fi->dev, "FB_IOCTL_GET_VID_OFFSET with arg =\
			 %08x\n", (unsigned int)arg);
		break;
	case FB_IOCTL_SET_MEMORY_TOGGLE:
		dev_dbg(fi->dev, "FB_IOCTL_SET_MEMORY_TOGGLE with arg =\
			 %08x\n", (unsigned int) arg);
		break;
	case FB_IOCTL_SET_COLORKEYnALPHA:
		dev_dbg(fi->dev, "FB_IOCTL_SET_COLORKEYnALPHA with arg =\
			 %08x\n", (unsigned int)arg);
		break;
	case FB_IOCTL_GET_CHROMAKEYS:
		dev_dbg(fi->dev, "FB_IOCTL_GET_CHROMAKEYS with arg =\
			 %08x\n", (unsigned int)arg);
		break;
	case FB_IOCTL_PUT_CHROMAKEYS:
		dev_dbg(fi->dev, "FB_IOCTL_PUT_CHROMAKEYS with arg =\
			 %08x\n", (unsigned int)arg);
		break;
	case FB_IOCTL_GET_COLORKEYnALPHA:
		dev_dbg(fi->dev, "FB_IOCTL_GET_COLORKEYnALPHA with arg =\
			 %08x\n", (unsigned int)arg);
		break;
	case FB_IOCTL_SWITCH_VID_OVLY:
		dev_dbg(fi->dev, "FB_IOCTL_SWITCH_VID_OVLY with arg =\
			 %08x\n", (unsigned int)arg);
		break;
	case FB_IOCTL_SWAP_VIDEO_RED_BLUE:
		dev_dbg(fi->dev, "FB_IOCTL_PUT_SWAP_VIDEO_RED_BLUE with arg =\
			 %08x\n", (unsigned int)arg);
		break;
	case FB_IOCTL_SWAP_VIDEO_U_V:
		dev_dbg(fi->dev, "FB_IOCTL_PUT_SWAP_VIDEO_U_V with arg =\
			 %08x\n", (unsigned int)arg);
		break;
	case FB_IOCTL_SWAP_VIDEO_Y_UV:
		dev_dbg(fi->dev, "FB_IOCTL_PUT_SWAP_VIDEO_Y_UV with arg =\
			 %08x\n", (unsigned int)arg);
		break;
	case FB_IOCTL_PUT_VIDEO_ALPHABLEND:
		dev_dbg(fi->dev, "FB_IOCTL_PUT_VIDEO_ALPHABLEND with arg =\
			 %08x\n", (unsigned int)arg);
		break;
	case FB_IOCTL_PUT_GLOBAL_ALPHABLEND:
		dev_dbg(fi->dev, "FB_IOCTL_PUT_GLOBAL_ALPHABLEND with arg =\
			 %08x\n", (unsigned int)arg);
		break;
	case FB_IOCTL_PUT_GRAPHIC_ALPHABLEND:
		dev_dbg(fi->dev, "FB_IOCTL_PUT_GRAPHIC_ALPHABLEND with arg =\
			 %08x\n", (unsigned int)arg);
		break;

	}
}
#endif

static int pxa168fb_ovly_ioctl(struct fb_info *fi, unsigned int cmd,
			unsigned long arg)
{
	void __user *argp = (void __user *)arg;
	struct pxa168fb_info *fbi = (struct pxa168fb_info *)fi->par;
	struct pxa168fb_mach_info *mi = fbi->dev->platform_data;
	int vid_on = 1;
	int val = 0, mask = 0;
	unsigned char param;
	int blendval = 0;
	int res, tmp;
	int ret = 0;
	unsigned long flags;

#ifdef CONFIG_DYNAMIC_PRINTK_DEBUG
	debug_identify_called_ioctl(fi, cmd, arg);
#endif

	switch (cmd) {
	case FB_IOCTL_CLEAR_FRAMEBUFFER:
		if (!mi->mmap)
			return -EINVAL;
		pxa168fb_clear_framebuffer(fi);
		return 0;
		break;
	case FB_IOCTL_WAIT_VSYNC:
		param = (arg & 0x3);
		wait_for_vsync(fbi, param);
		break;
	case FB_IOCTL_GET_VIEWPORT_INFO:/*if rotate 90/270, w/h swap*/
		mutex_lock(&fbi->access_ok);
		if (fbi->surface.viewPortInfo.rotation == 90 ||
			fbi->surface.viewPortInfo.rotation == 270) {
			tmp = fbi->surface.viewPortInfo.srcWidth;
			fbi->surface.viewPortInfo.srcWidth =
			fbi->surface.viewPortInfo.srcHeight;
			fbi->surface.viewPortInfo.srcHeight = tmp;
			fbi->surface.viewPortInfo.rotation = 360 -
			fbi->surface.viewPortInfo.rotation;
		}
		res = copy_to_user(argp, &fbi->surface.viewPortInfo,
			sizeof(struct _sViewPortInfo)) ? -EFAULT : 0;
		if (fbi->surface.viewPortInfo.rotation == 90 ||
			fbi->surface.viewPortInfo.rotation == 270) {
			tmp = fbi->surface.viewPortInfo.srcWidth;
			fbi->surface.viewPortInfo.srcWidth =
			fbi->surface.viewPortInfo.srcHeight;
			fbi->surface.viewPortInfo.srcHeight = tmp;
			fbi->surface.viewPortInfo.rotation = 360 -
			fbi->surface.viewPortInfo.rotation;
		}
		mutex_unlock(&fbi->access_ok);
		return res;
	case FB_IOCTL_SET_VIEWPORT_INFO:/*if rotate 90/270, w/h swap*/
		mutex_lock(&fbi->access_ok);
		memset(&gOvlySurface, 0, sizeof(gOvlySurface));
		gOvlySurface.videoMode = -1;
		if (copy_from_user(&gOvlySurface.viewPortInfo, argp,
				sizeof(gOvlySurface.viewPortInfo))) {
			mutex_unlock(&fbi->access_ok);
			return -EFAULT;
		}
		if (unsupport_format(fbi, gOvlySurface.viewPortInfo, -1)) {
			mutex_unlock(&fbi->access_ok);
			return -EFAULT;
		}
		gOvlySurface.viewPortInfo.rotation =
		 (360 - gOvlySurface.viewPortInfo.rotation) % 360;
		if (gOvlySurface.viewPortInfo.rotation == 90 ||
			gOvlySurface.viewPortInfo.rotation == 270) {
			tmp = gOvlySurface.viewPortInfo.srcWidth;
			gOvlySurface.viewPortInfo.srcWidth =
			gOvlySurface.viewPortInfo.srcHeight;
			gOvlySurface.viewPortInfo.srcHeight = tmp;
		}

		ret = check_surface(fi, &gOvlySurface);
		if (ret > 0) {
			pxa168fb_set_par(fi);
			ret = 0;
		} else if (ret < 0) {
			pr_err("fbi %d (line %d): vid %d, check surface"
				"return error\n", fbi->id, __LINE__, fbi->vid);
			ret = -EFAULT;
		}
		mutex_unlock(&fbi->access_ok);
		return ret;
		break;
	case FB_IOCTL_SET_VIDEO_MODE:
		/*
		 * Get data from user space.
		 */
		memset(&gOvlySurface, 0, sizeof(gOvlySurface));
		if (copy_from_user(&gOvlySurface.videoMode, argp,
				 sizeof(gOvlySurface.videoMode)))
			return -EFAULT;

		if (unsupport_format(fbi, gOvlySurface.viewPortInfo,
			 gOvlySurface.videoMode))
			return -EFAULT;
		ret = check_surface(fi, &gOvlySurface);
		if (ret > 0) {
			pxa168fb_set_par(fi);
			ret = 0;
		} else if (ret < 0) {
			pr_err("fbi %d (line %d): vid %d, check surface"
				"return error\n", fbi->id, __LINE__, fbi->vid);
			ret = -EFAULT;
		}
		return ret;
		break;
	case FB_IOCTL_GET_VIDEO_MODE:
		return copy_to_user(argp, &fbi->surface.videoMode,
			sizeof(u32)) ? -EFAULT : 0;
	case FB_IOCTL_FLIP_VID_BUFFER:
		return flip_buffer(fi, arg);
	case FB_IOCTL_GET_FREELIST:
		return get_freelist(fi, arg);
		
	case FB_IOCTL_FLIP_VSYNC:
		return flip_buffer_vsync(fi, arg);
	case FB_IOCTL_GET_BUFF_ADDR:
	{
		return copy_to_user(argp, &fbi->surface.videoBufferAddr,
			sizeof(struct _sVideoBufferAddr)) ? -EFAULT : 0;
	}
	case FB_IOCTL_SET_VID_OFFSET:
		mutex_lock(&fbi->access_ok);
		memset(&gOvlySurface, 0, sizeof(gOvlySurface));
		gOvlySurface.videoMode = -1;
		if (copy_from_user(&gOvlySurface.viewPortOffset, argp,
				sizeof(gOvlySurface.viewPortOffset))) {
			mutex_unlock(&fbi->access_ok);
			return -EFAULT;
		}

		ret = check_surface(fi, &gOvlySurface);
		if (ret > 0) {
			pxa168fb_set_par(fi);
			ret = 0;
		} else if (ret < 0) {
			pr_err("fbi %d (line %d): vid %d, check surface"
				"return error\n", fbi->id, __LINE__, fbi->vid);
			ret = -EFAULT;
		}
		mutex_unlock(&fbi->access_ok);
		return ret;
		break;
	case FB_IOCTL_GET_VID_OFFSET:
		return copy_to_user(argp, &fbi->surface.viewPortOffset,
			sizeof(struct _sViewPortOffset)) ? -EFAULT : 0;

	case FB_IOCTL_SET_SURFACE:
	{
		mutex_lock(&fbi->access_ok);
		/* Get user-mode data. */
		if (copy_from_user(&fbi->surface_bak, argp,
					sizeof(struct _sOvlySurface))) {
			mutex_unlock(&fbi->access_ok);
			return -EFAULT;
		}
		fbi->surface_set = 1;

		mutex_unlock(&fbi->access_ok);
		return 0;
	}
	case FB_IOCTL_GET_SURFACE:
	{
		mutex_lock(&fbi->access_ok);
		if (fbi->surface_set) {
			ret = copy_to_user(argp, &fbi->surface_bak,
				sizeof(struct _sOvlySurface));
		} else {
		    ret = copy_to_user(argp, &fbi->surface,
				sizeof(struct _sOvlySurface));
		}

		ret = (ret ? -EFAULT : 0);
		mutex_unlock(&fbi->access_ok);
		return ret;
	}

	case FB_IOCTL_SET_COLORKEYnALPHA:
		if (copy_from_user(&fbi->ckey_alpha, argp,
		    sizeof(struct _sColorKeyNAlpha)))
			return -EFAULT;

		pxa168fb_ovly_set_colorkeyalpha(fbi);
		break;

	case FB_IOCTL_GET_COLORKEYnALPHA:
		if (copy_to_user(argp, &fbi->ckey_alpha,
		    sizeof(struct _sColorKeyNAlpha)))
			return -EFAULT;
		break;
	case FB_IOCTL_SWITCH_VID_OVLY:
		if (copy_from_user(&vid_on, argp, sizeof(int)))
			return -EFAULT;
		spin_lock_irqsave(&fbi->var_lock, flags);
		mask = CFG_DMA_ENA_MASK;

		fbi->dma_on = vid_on ? 1 : 0;
		val = CFG_DMA_ENA(check_modex_active(fbi));
		if (!val && gfx_info.fbi[0]->active) {
			pxa688_vdma_release(fbi->id, fbi->vid);
			/* switch off, disable DMA */
			dma_ctrl_set(fbi->id, 0, mask, val);
		} else if (list_empty(&fbi->buf_waitlist.dma_queue) &&
			!fbi->buf_current)
			/* switch on, but no buf flipped, return error */
			; /* ret = -EAGAIN; */

		printk(KERN_DEBUG "SWITCH_VID_OVLY fbi %d dma_on %d,"
			" val %d, waitlist empty %d buf_current %p, ret %d\n",
			fbi->id, fbi->dma_on, val,
			list_empty(&fbi->buf_waitlist.dma_queue),
			fbi->buf_current, ret);

		pxa688fb_vsmooth_set(fbi->id, 1, vid_vsmooth & vid_on);

		spin_unlock_irqrestore(&fbi->var_lock, flags);
		return ret;
		break;

	case FB_IOCTL_SWAP_VIDEO_RED_BLUE:
		param = (arg & 0x1);
		dma_ctrl_set(fbi->id, 0, CFG_DMA_SWAPRB_MASK,
				 CFG_DMA_SWAPRB(param));
		return 0;
		break;

	case FB_IOCTL_SWAP_VIDEO_U_V:
		param = (arg & 0x1);
		dma_ctrl_set(fbi->id, 0, CFG_DMA_SWAPUV_MASK,
				 CFG_DMA_SWAPUV(param));
		return 0;
		break;

	case FB_IOCTL_SWAP_VIDEO_Y_UV:
		param = (arg & 0x1);
		dma_ctrl_set(fbi->id, 0, CFG_DMA_SWAPYU_MASK,
				 CFG_DMA_SWAPYU(param));
		return 0;
		break;

	case FB_IOCTL_PUT_VIDEO_ALPHABLEND:
		/*
		 *  This puts the blending control to the Video layer.
		 */
		mask = CFG_ALPHA_MODE_MASK | CFG_ALPHA_MASK;
		val = CFG_ALPHA_MODE(0) | CFG_ALPHA(0xff);
		dma_ctrl_set(fbi->id, 1, mask, val);
		return 0;
		break;

	case FB_IOCTL_PUT_GLOBAL_ALPHABLEND:
		/*
		 *  The userspace application can specify a byte value for the
		 *  amount of global blend between the video layer and thei
		 *  graphic layer.
		 *
		 *  The alpha blending is per the formula below:
		 *  P = (V[P] * blendval/255) + (G[P] * (1 - blendval/255))
		 *  where: P = Pixel value, V = Video Layer,
		 *  and G = Graphic Layer
		 */
		blendval = (arg & 0xff);
		mask = CFG_ALPHA_MODE_MASK | CFG_ALPHA_MASK;
		val = CFG_ALPHA_MODE(2) | CFG_ALPHA(blendval);
		dma_ctrl_set(fbi->id, 1, mask, val);
		return 0;
		break;

	case FB_IOCTL_PUT_GRAPHIC_ALPHABLEND:
		/*
		 *  This puts the blending back to the default mode of allowing
		 *  the graphic layer to do pixel level blending.
		 */
		mask = CFG_ALPHA_MODE_MASK | CFG_ALPHA_MASK;
		val = CFG_ALPHA_MODE(1) | CFG_ALPHA(0x0);
		dma_ctrl_set(fbi->id, 1, mask, val);
		return 0;
		break;

	default:
		break;
	}

	return 0;
}

static int pxa168fb_open(struct fb_info *fi, int user)
{
	struct pxa168fb_mach_info *mi;
	struct pxa168fb_info *fbi = (struct pxa168fb_info *)fi->par;
	struct fb_var_screeninfo *var = &fi->var;
	struct _sVideoBufferAddr *new_addr = &fbi->surface.videoBufferAddr;

	pr_info("%s Video layer, fbi %d opened %d times ----\n",
		__func__, fbi->id, atomic_read(&fbi->op_count));

#ifdef OVLY_DVFM_CONSTRAINT
	dvfm_disable_lowpower(dvfm_dev_idx);
#endif

	mi = fbi->dev->platform_data;
	memset(new_addr, 0, sizeof(struct _sVideoBufferAddr));
	fi->fix.smem_start = fbi->fb_start_dma;
	fi->screen_base = fbi->fb_start;
	fbi->surface.videoMode = -1;
	fbi->surface.viewPortInfo.srcWidth = var->xres;
	fbi->surface.viewPortInfo.srcHeight = var->yres;

	fbi->active = 1;
	set_pix_fmt(var, fbi->pix_fmt);

	if (mutex_is_locked(&fbi->access_ok))
		mutex_unlock(&fbi->access_ok);

	/* clear buffer list. */
	clear_buffer(fbi);

	/* increase open count */
	atomic_inc(&fbi->op_count);

	return 0;
}

extern void enable_graphic_layer(int id);

static int pxa168fb_release(struct fb_info *fi, int user)
{
	struct pxa168fb_info *fbi = (struct pxa168fb_info *)fi->par;
	struct fb_var_screeninfo *var = &fi->var;
	u32 mask;

	pr_info("%s Video layer, fbi %d opened %d times ----\n",
		__func__, fbi->id, atomic_read(&fbi->op_count));

	/* Force Video DMA engine off at release and reset the DMA format.*/
	if (atomic_dec_and_test(&fbi->op_count)) {
		pxa688_vdma_release(fbi->id, fbi->vid);
		mask = CFG_DMA_ENA_MASK | CFG_DMAFORMAT_MASK;
		dma_ctrl_set(fbi->id, 0, mask, 0);
		pxa688fb_vsmooth_set(fbi->id, 1, 0);
	}

	/* Turn off compatibility mode */
	var->nonstd &= ~0xff000000;
	fbi->compat_mode = 0;

	/* make sure graphics layer is enabled */
	enable_graphic_layer(fbi->id);

	/* clear buffer list. */
	clear_buffer(fbi);

	/* clear some globals */
	memset(&fbi->surface, 0, sizeof(struct _sOvlySurface));
	fbi->surface.videoMode = -1;
	fbi->active = fbi->dma_on = 0;

#ifdef OVLY_DVFM_CONSTRAINT
	dvfm_enable_lowpower(dvfm_dev_idx);
#endif

	return 0;
}

static void set_mode(struct pxa168fb_info *fbi, struct fb_var_screeninfo *var,
		     struct fb_videomode *mode, int pix_fmt, int ystretch)
{
	dev_dbg(fbi->fb_info->dev, "Enter %s\n", __func__);
	set_pix_fmt(var, pix_fmt);

	var->xres = mode->xres;
	var->yres = mode->yres;
	var->xres_virtual = max(var->xres, var->xres_virtual);
	if (ystretch)
		var->yres_virtual = var->yres*2;
	else
		var->yres_virtual = max(var->yres, var->yres_virtual);

	var->grayscale = 0;
	var->accel_flags = FB_ACCEL_NONE;
	var->pixclock = mode->pixclock;
	var->left_margin = mode->left_margin;
	var->right_margin = mode->right_margin;
	var->upper_margin = mode->upper_margin;
	var->lower_margin = mode->lower_margin;
	var->hsync_len = mode->hsync_len;
	var->vsync_len = mode->vsync_len;
	var->sync = mode->sync;
	var->vmode = FB_VMODE_NONINTERLACED;
	var->rotate = FB_ROTATE_UR;
}

static int pxa168fb_set_par(struct fb_info *fi)
{
	struct pxa168fb_info *fbi = fi->par;
	struct fb_var_screeninfo *var = &fi->var;
	struct regshadow *shadowreg = &fbi->shadowreg;
	int pix_fmt;
	u32 flags;

	dev_dbg(fi->dev, "Enter %s, video layer\n", __func__);
	/*
	 * Determine which pixel format we're going to use.
	 */
	pix_fmt = determine_best_pix_fmt(var, fbi);
	if (pix_fmt < 0)
		return pix_fmt;
	fbi->pix_fmt = pix_fmt;
	set_pix_fmt(var, pix_fmt);

	if (!var->xres_virtual)
		var->xres_virtual = var->xres;
	if (!var->yres_virtual)
		var->yres_virtual = var->yres * 2;
	var->grayscale = 0;
	var->accel_flags = FB_ACCEL_NONE;
	var->rotate = FB_ROTATE_UR;

	/* Set additional mode info */
	if (pix_fmt == PIX_FMT_PSEUDOCOLOR)
		fi->fix.visual = FB_VISUAL_PSEUDOCOLOR;
	else
		fi->fix.visual = FB_VISUAL_TRUECOLOR;
	fi->fix.line_length = var->xres_virtual * var->bits_per_pixel / 8;

	/* when lcd is suspend, read or write lcd controller's
	* register is not effective, so just return*/
	if (!(gfx_info.fbi[fbi->id]->active)) {
		printk(KERN_DEBUG"LCD is not active, don't touch hardware\n");
		return 0;
	}

	flags = UPDATE_ADDR | UPDATE_MODE | UPDATE_VIEW;
	pxa168fb_set_var(fi, shadowreg, flags);

	if (!NEED_VSYNC(fbi))
		pxa168fb_set_regs(fbi, shadowreg);

	return 0;
}

static unsigned int chan_to_field(unsigned int chan, struct fb_bitfield *bf)
{
	return ((chan & 0xffff) >> (16 - bf->length)) << bf->offset;
}

static u32 to_rgb(u16 red, u16 green, u16 blue)
{
	red >>= 8;
	green >>= 8;
	blue >>= 8;

	return (red << 16) | (green << 8) | blue;
}

static int
pxa168fb_setcolreg(unsigned int regno, unsigned int red, unsigned int green,
		 unsigned int blue, unsigned int trans, struct fb_info *fi)
{
	struct pxa168fb_info *fbi = fi->par;
	u32 val;

	if (fi->fix.visual == FB_VISUAL_TRUECOLOR && regno < 16) {
		val =  chan_to_field(red,   &fi->var.red);
		val |= chan_to_field(green, &fi->var.green);
		val |= chan_to_field(blue , &fi->var.blue);
		fbi->pseudo_palette[regno] = val;
	}

	if (fi->fix.visual == FB_VISUAL_PSEUDOCOLOR && regno < 256) {
		val = to_rgb(red, green, blue);
		writel(val, fbi->reg_base + LCD_SPU_SRAM_WRDAT); /* FIXME */
		writel(0x8300 | regno, fbi->reg_base + LCD_SPU_SRAM_CTRL);
	}

	return 0;
}

static int pxa168fb_pan_display(struct fb_var_screeninfo *var,
				struct fb_info *fi)
{
	struct pxa168fb_info *fbi = fi->par;
	struct pxa168fb_mach_info *mi = fbi->dev->platform_data;

	if (!mi->mmap)
		return -EINVAL;

	fbi->active = 1;
	set_start_address(fi, var->xoffset, var->yoffset, &fbi->shadowreg);
	fbi->shadowreg.flags |= UPDATE_ADDR;
	pxa168fb_set_regs(fbi, &fbi->shadowreg);

	if (NEED_VSYNC(fbi))
		wait_for_vsync(fbi, SYNC_SELF);

	return 0;
}

static int pxa168fb_fb_sync(struct fb_info *info)
{
	struct pxa168fb_info *fbi = (struct pxa168fb_info *)info->par;

	wait_for_vsync(fbi, SYNC_SELF);
	return 0;
}

static struct fb_ops pxa168fb_ops = {
	.owner		= THIS_MODULE,
	.fb_open	= pxa168fb_open,
	.fb_release	= pxa168fb_release,

	.fb_check_var	= pxa168fb_check_var,
	.fb_set_par	= pxa168fb_set_par,
	.fb_setcolreg	= pxa168fb_setcolreg,
	.fb_pan_display	= pxa168fb_pan_display,
	.fb_fillrect	= cfb_fillrect,
	.fb_copyarea	= cfb_copyarea,
	.fb_imageblit	= cfb_imageblit,
	.fb_sync	= pxa168fb_fb_sync,
	.fb_ioctl	= pxa168fb_ovly_ioctl,
};

static int __init get_ovly_size(char *str)
{
	int n;
	if (!get_option(&str, &n))
		return 0;
	max_fb_size = n;
	fb_size_from_cmd = 1;
	return 1;
}
__setup("ovly_size=", get_ovly_size);

static int __devinit pxa168fb_probe(struct platform_device *pdev)
{
	struct pxa168fb_mach_info *mi;
	struct fb_info *fi;
	struct pxa168fb_info *fbi;
	struct lcd_regs *regs;
	struct resource *res;
	struct pxa168fb_vdma_info *lcd_vdma = 0;
	int ret;

	mi = pdev->dev.platform_data;
	if (mi == NULL)
		return -EINVAL;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (res == NULL)
		return -EINVAL;

	fi = framebuffer_alloc(sizeof(struct pxa168fb_info), &pdev->dev);
	if (fi == NULL) {
		pr_info("%s: no enough memory!\n", __func__);
		return -ENOMEM;
	}

	fbi = fi->par;
	fbi->id = pdev->id;
	fbi->vid = 1;
	if (!fbi->id)
		memset(&ovly_info, 0, sizeof(ovly_info));
	ovly_info.fbi[fbi->id] = fbi;

	platform_set_drvdata(pdev, fbi);
	fbi->fb_info = fi;
	fbi->dev = &pdev->dev;
	fbi->id = pdev->id;
	fbi->fb_info->dev = &pdev->dev;
	fbi->debug = 0;
	init_waitqueue_head(&fbi->w_intr_wq);
	mutex_init(&fbi->access_ok);
	spin_lock_init(&fbi->buf_lock);
	spin_lock_init(&fbi->var_lock);

	/* FIXME - video layer has no specific clk. it depend on
	 * graphic layer clk. fbi->clk = clk_get(&pdev->dev, NULL);
	 */

	/*
	 * Initialise static fb parameters.
	 */
	fi->flags = FBINFO_DEFAULT | FBINFO_PARTIAL_PAN_OK |
		    FBINFO_HWACCEL_XPAN | FBINFO_HWACCEL_YPAN;
	fi->node = -1;
	strcpy(fi->fix.id, mi->id);
	fi->fix.type = FB_TYPE_PACKED_PIXELS;
	fi->fix.type_aux = 0;
	fi->fix.xpanstep = 1;
	fi->fix.ypanstep = 1;
	fi->fix.ywrapstep = 0;
	fi->fix.mmio_start = res->start;
	fi->fix.mmio_len = resource_size(res);
	fi->fix.accel = FB_ACCEL_NONE;
	fi->fbops = &pxa168fb_ops;
	fi->pseudo_palette = fbi->pseudo_palette;

	/* Map LCD controller registers. */
	fbi->reg_base = devm_ioremap_nocache(&pdev->dev, res->start,
					     resource_size(res));

	if (fbi->reg_base == NULL) {
		pr_info("%s: no enough memory!\n", __func__);
		ret = -ENOMEM;
		goto failed;
	}

	if (mi->mmap) {
		/*
		 * Allocate framebuffer memory.
		 */
		if (!fb_size_from_cmd) {
			if (mi->max_fb_size)
				max_fb_size = mi->max_fb_size;
			else
				max_fb_size = DEFAULT_FB_SIZE;
		}
		max_fb_size = PAGE_ALIGN(max_fb_size);
		fbi->fb_size = max_fb_size;

		/*
		 * FIXME, It may fail to alloc DMA buffer from dma_alloc_xxx
		 */
		fbi->fb_start = pxa168fb_alloc_framebuffer(fbi->fb_size,
				&fbi->fb_start_dma);

		if (fbi->fb_start == NULL) {
			pr_info("%s: no enough memory!\n", __func__);
			ret = -ENOMEM;
			goto failed;
		}
		pr_info("---------FBoverlay DMA buffer phy addr : %x\n",
			(unsigned int)fbi->fb_start_dma);

		memset(fbi->fb_start, 0, fbi->fb_size);
		fi->fix.smem_start = fbi->fb_start_dma;
		fi->fix.smem_len = fbi->fb_size;
		fi->screen_base = fbi->fb_start;
		fi->screen_size = fbi->fb_size;
		fbi->wait_vsync = 1;
	}

#ifdef FB_PM_DEBUG
	pxa168fb_rw_all_regs(fbi, g_regs, 1);
#endif

	/* init vdma clock/sram, etc. */
	lcd_vdma = request_vdma(fbi->id, fbi->vid);
	if (lcd_vdma) {
		lcd_vdma->dev = fbi->dev;
		lcd_vdma->reg_base = fbi->reg_base;
		pxa688_vdma_init(lcd_vdma);
	} else
		pr_warn("path %d layer %d: request vdma fail\n", fbi->id, fbi->vid);

	/*
	 * Fill in sane defaults.
	 */
	set_mode(fbi, &fi->var, mi->modes, mi->pix_fmt, 1);
	pxa168fb_set_par(fi);

	pxa168fb_list_init(fbi);

	/*
	 * Configure default register values.
	 */
	regs = get_regs(fbi->id);
	writel(0, &regs->v_y1);
	writel(0, &regs->v_u1);
	writel(0, &regs->v_v1);
	writel(0, &regs->v_start);

	/* Set this frame off by default */
	dma_ctrl_set(fbi->id, 0, CFG_DMA_ENA_MASK, 0);

	/*
	 * Allocate color map.
	 */
	if (fb_alloc_cmap(&fi->cmap, 256, 0) < 0) {
		ret = -ENOMEM;
		goto failed;
	}

	/*
	 * Get IRQ number.
	 */
	res = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	if (res == NULL)
		return -EINVAL;

	/*
	 * Register framebuffer.
	 */
	ret = register_framebuffer(fi);
	if (ret < 0) {
		dev_err(&pdev->dev, "Failed to register pxa168fb: %d\n", ret);
		ret = -ENXIO;
		goto failed;
	}

	pr_info("pxa168fb_ovly: frame buffer device was loaded"
		" to /dev/fb%d <%s>.\n", fi->node, fi->fix.id);

#ifdef OVLY_DVFM_CONSTRAINT
	dvfm_register("overlay1", &dvfm_dev_idx);
#endif

#ifdef CONFIG_PXA688_VDMA
	ret = device_create_file(&pdev->dev, &dev_attr_vdma);
	if (ret < 0) {
		pr_err("device attr create fail: %d\n", ret);
		return ret;
	}
#endif

	ret = device_create_file(&pdev->dev, &dev_attr_lcd);
	if (ret < 0) {
		pr_err("device attr create fail: %d\n", ret);
		goto failed;
	}

	return 0;

failed:
	platform_set_drvdata(pdev, NULL);
	fb_dealloc_cmap(&fi->cmap);
	if (fbi->fb_start != NULL) {
		pxa168fb_free_framebuffer(max_fb_size, fbi->fb_start,
				&fbi->fb_start_dma);
	}
	if (fbi->reg_base != NULL)
		iounmap(fbi->reg_base);
	kfree(fbi);
	framebuffer_release(fi);
	return ret;
}

#ifdef CONFIG_PM
#if 0
static int pxa168fb_vid_suspend(struct platform_device *pdev, pm_message_t mesg)
{
	struct pxa168fb_info *fbi = platform_get_drvdata(pdev);
	struct fb_info *fi = fbi->fb_info;

#ifdef FB_PM_DEBUG
	pxa168fb_rw_all_regs(fbi, g_regs1, 1);
#endif

	if (mesg.event & PM_EVENT_SLEEP)
		fb_set_suspend(fi, 1);
	pdev->dev.power.power_state = mesg;

#ifdef FB_PM_DEBUG
	pxa168fb_rw_all_regs(fbi, g_regs, 0);
#endif
	pr_info("pxa168fb_ovly.%d suspended, state = %d.\n",
		 fbi->id, mesg.event);

	return 0;
}

static int pxa168fb_vid_resume(struct platform_device *pdev)
{
	struct pxa168fb_info *fbi = platform_get_drvdata(pdev);
	struct fb_info *fi = fbi->fb_info;

	fb_set_suspend(fi, 0);

#ifdef FB_PM_DEBUG
	{
		u32 i;
		u32 reg;

		for (i = 0xC0; i <= 0x01C4; i += 4) {
			reg = readl(fbi->reg_base + i);
			if (reg != g_regs1[i])
				pr_info("Register 0x%08x: 0x%08x - 0x%08x.\n",
						i, g_regs1[i], reg);
		}
	}
#endif

	pr_info("pxa168fb_ovly.%d resumed.\n", fbi->id);

	return 0;
}
#endif
#endif


static struct platform_driver pxa168fb_driver = {
	.probe		= pxa168fb_probe,
/*	.remove		= pxa168fb_remove,		*/
#ifdef CONFIG_PM
/*	.suspend	= pxa168fb_vid_suspend, */
/*	.resume		= pxa168fb_vid_resume, */
#endif
	.driver		= {
		.name	= "pxa168fb_ovly",
		.owner	= THIS_MODULE,
	},
};

static int __devinit pxa168fb_init(void)
{
	return platform_driver_register(&pxa168fb_driver);
}

/*module_init(pxa168fb_init);*/
late_initcall(pxa168fb_init);

MODULE_DESCRIPTION("Framebuffer driver for PXA168");
MODULE_LICENSE("GPL");
