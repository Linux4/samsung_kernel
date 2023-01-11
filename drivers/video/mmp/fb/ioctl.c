/*
 * linux/drivers/video/mmp/fb/ioctl.c
 * Framebuffer driver for Marvell Display controller.
 *
 * Copyright (C) 2013 Marvell Technology Group Ltd.
 * Authors: Yu Xu <yuxu@marvell.com>
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

#include <linux/device.h>
#include <linux/uaccess.h>
#include <video/mmp_disp.h>
#include <video/mmp_ioctl.h>
#include <video/mmp_trace.h>
#include <video/mmpfb.h>
#include "mmpfb.h"

void check_pitch(struct mmp_surface *surface)
{
	struct mmp_win *win = &surface->win;
	u32 pitch[3];

	pitch[0] = win->pitch[0];
	pitch[1] = win->pitch[1];
	pitch[2] = win->pitch[2];

	switch (win->pix_fmt) {
	case PIXFMT_UYVY:
	case PIXFMT_VYUY:
	case PIXFMT_YUYV:
	case PIXFMT_RGB565:
	case PIXFMT_BGR565:
	case PIXFMT_RGB1555:
	case PIXFMT_BGR1555:
	case PIXFMT_RGB888PACK:
	case PIXFMT_BGR888PACK:
	case PIXFMT_RGB888UNPACK:
	case PIXFMT_BGR888UNPACK:
	case PIXFMT_RGBA888:
	case PIXFMT_BGRA888:
		pitch[0] = pitch[0] ? pitch[0] : (win->xsrc
				* pixfmt_to_stride(win->pix_fmt));
		pitch[1] = pitch[2] = 0;
		break;
	case PIXFMT_YUV422P:
	case PIXFMT_YVU422P:
	case PIXFMT_YUV420P:
	case PIXFMT_YVU420P:
		pitch[0] = pitch[0] ? pitch[0] : win->xsrc;
		pitch[1] = pitch[1] ? pitch[1] : win->xsrc >> 1;
		pitch[2] = pitch[2] ? pitch[2] : win->xsrc >> 1;
		break;
	case PIXFMT_YUV420SP:
	case PIXFMT_YVU420SP:
		pitch[0] = pitch[0] ? pitch[0] : win->xsrc;
		pitch[1] = pitch[1] ? pitch[1] : win->xsrc;
		pitch[2] = 0;
		break;
	default:
		pr_info("pix format 0x%x is not support\n", win->pix_fmt);
	}

	win->pitch[0] = pitch[0];
	win->pitch[1] = pitch[1];
	win->pitch[2] = pitch[2];
}

#ifdef CONFIG_MMP_FENCE
static int flip_buffer(struct fb_info *info, unsigned long arg)
{
	return mmpfb_ioctl_flip_fence(info, arg);
}
#endif

static int flip_buffer_vsync(struct fb_info *info, unsigned long arg)
{
	void __user *argp = (void __user *)arg;
	struct mmpfb_info *fbi = info->par;
	struct mmp_surface surface;

	if (copy_from_user(&surface, argp, sizeof(struct mmp_surface)))
		return -EFAULT;

	check_pitch(&surface);
	trace_surface(fbi->overlay->id, &surface);
	mmp_overlay_set_surface(fbi->overlay, &surface);
	return 0;
}

static int set_gamma(struct fb_info *info, unsigned long arg)
{
	void __user *argp = (void __user *)arg;
	struct mmpfb_info *fbi = info->par;
	struct mmp_gamma gamma;

	if (copy_from_user(&gamma, argp, sizeof(gamma)))
		return -EFAULT;

	return mmp_path_set_gamma(fbi->path, gamma.flag, gamma.table);
}

static int set_colorkey_alpha(struct fb_info *info, unsigned long arg)
{
	void __user *argp = (void __user *)arg;
	struct mmpfb_info *fbi = info->par;

	if (copy_from_user(&fbi->ca, argp, sizeof(struct mmp_colorkey_alpha)))
		return -EFAULT;

	return mmp_overlay_set_colorkey_alpha(fbi->overlay, &fbi->ca);
}

static int get_colorkey_alpha(struct fb_info *info, unsigned long arg)
{
	void __user *argp = (void __user *)arg;
	struct mmpfb_info *fbi = info->par;

	if (copy_to_user(argp, &fbi->ca, sizeof(struct mmp_colorkey_alpha)))
		return -EFAULT;

	return 0;
}

static int vsmooth_en(struct fb_info *info, unsigned long arg)
{
	void __user *argp = (void __user *)arg;
	struct mmpfb_info *fbi = info->par;
	u32 vsmooth_en;

	if (copy_from_user(&vsmooth_en, argp, sizeof(u32)))
		return -EFAULT;

	mmp_overlay_vsmooth_en(fbi->overlay, vsmooth_en);

	return 0;
}

static int enable_dma(struct fb_info *info, unsigned long arg)
{
	void __user *argp = (void __user *)arg;
	struct mmpfb_info *fbi = info->par;
	u32 dma_on;

	if (copy_from_user(&dma_on, argp, sizeof(u32)))
		return -EFAULT;

	if (!dma_on)
		mmpfb_fence_pause(fbi);

	mmp_overlay_set_status(fbi->overlay, dma_on ?
				MMP_ON : MMP_OFF);

	dev_dbg(info->dev, "fbi %d dma_on %d\n", fbi->id, dma_on);

	return 0;
}

static int get_global_info(struct fb_info *info, unsigned long arg)
{
	void __user *argp = (void __user *)arg;
	struct mmpfb_global_info global_info;
	int i = 0;

	global_info.version = FB_VERSION_1;
	while (registered_fb[i])
		i++;
	global_info.fb_counts = i;

	if (copy_to_user(argp, &global_info, sizeof(struct mmpfb_global_info)))
		return -EFAULT;

	return 0;
}

static int enable_commit_dma(struct fb_info *info, unsigned long arg)
{
	void __user *argp = (void __user *)arg;
	struct mmpfb_info *fbi = info->par;
	u32 dma_on;

	if (copy_from_user(&dma_on, argp, sizeof(u32)))
		return -EFAULT;

	if (!dma_on)
		mmpfb_fence_pause(fbi);

	mmp_overlay_set_status(fbi->overlay, dma_on ?
			MMP_ON_DMA : MMP_OFF_DMA);

	dev_dbg(info->dev, "fbi %d dma_on %d\n", fbi->id, dma_on);

	return 0;
}

static int enable_commit(struct fb_info *info, unsigned long arg)
{
	struct mmpfb_info *fbi = info->par;

	mmp_path_set_commit(fbi->overlay->path);

	mmpfb_fence_store_commit_id(fbi);

	/*
	 * Enable irq once in after flip buffer
	 * IRQ will be disabled in irq handler.
	 */
	if (!atomic_read(&fbi->path->irq_en_count)) {
		atomic_inc(&fbi->path->irq_en_count);
		mmp_path_set_irq(fbi->path, 1);
	}

	return 0;
}

static int alpha_mode_to_ctrl(struct mmp_alpha *pa1, struct mmp_alpha *pa2)
{
	u32 val1 = 0, val2 = 0, id1 = 0, id2 = 0, alpha_id = 0;
	struct fb_info *info1, *info2, *info3;
	struct mmpfb_info *fb1, *fb2, *fb3;

	switch (pa2->alphapath) {
	case FB_FB0_AND_FB1:
		id1 = 0;
		id2 = 1;
		if (pa2->config == FB_FB0_ALPHA)
			alpha_id = 0;
		else if (pa2->config == FB_FB1_ALPHA)
			alpha_id = 1;
		else
			BUG_ON(1);
		break;
	case FB_FB0_AND_FB2:
		id1 = 0;
		id2 = 2;
		if (pa2->config == FB_FB0_ALPHA)
			alpha_id = 0;
		else if (pa2->config == FB_FB2_ALPHA)
			alpha_id = 2;
		else
			BUG_ON(1);
		break;
	case FB_FB0_AND_FB3:
		id1 = 0;
		id2 = 3;
		if (pa2->config == FB_FB0_ALPHA)
			alpha_id = 0;
		else if (pa2->config == FB_FB3_ALPHA)
			alpha_id = 3;
		else
			BUG_ON(1);
		break;
	case FB_FB1_AND_FB2:
		id1 = 1;
		id2 = 2;
		if (pa2->config == FB_FB1_ALPHA)
			alpha_id = 1;
		else if (pa2->config == FB_FB2_ALPHA)
			alpha_id = 2;
		else
			BUG_ON(1);
		break;
	case FB_FB1_AND_FB3:
		id1 = 1;
		id2 = 3;
		if (pa2->config == FB_FB1_ALPHA)
			alpha_id = 1;
		else if (pa2->config == FB_FB3_ALPHA)
			alpha_id = 3;
		else
			BUG_ON(1);
		break;
	case FB_FB2_AND_FB3:
		id1 = 2;
		id2 = 3;
		if (pa2->config == FB_FB2_ALPHA)
			alpha_id = 2;
		else if (pa2->config == FB_FB3_ALPHA)
			alpha_id = 3;
		else
			BUG_ON(1);
		break;
	default:
		val1 |= ALPHA_PN_GRA_AND_PN_VID;
		val2 |= ALPHA_PATH_GRA_PATH_ALPHA;
		return 0;
	}

	info1 = registered_fb[id1];
	info2 = registered_fb[id2];
	info3 = registered_fb[alpha_id];
	fb1 = info1->par;
	fb2 = info2->par;
	fb3 = info3->par;

	if ((fb1->overlay->id == PN_GRA) &&
			(fb2->overlay->id == PN_VID)) {
		val1 |= ALPHA_PN_GRA_AND_PN_VID;
		if (fb3->overlay->id == PN_GRA)
			val2 |= ALPHA_PATH_GRA_PATH_ALPHA;
		else if (fb3->overlay->id == PN_VID)
			val2 |= ALPHA_PATH_VID_PATH_ALPHA;
		else
			BUG_ON(1);
	} else if ((fb1->overlay->id == PN_GRA) &&
		(fb2->overlay->id == TV_GRA)) {
		val1 |= ALPHA_PN_GRA_AND_TV_GRA;
		if (fb3->overlay->id == PN_GRA)
			val2 |= ALPHA_PATH_PN_PATH_ALPHA;
		else if (fb3->overlay->id == TV_GRA)
			val2 |= ALPHA_PATH_TV_PATH_ALPHA;
		else
			BUG_ON(1);
	} else if ((fb1->overlay->id == PN_GRA) &&
		(fb2->overlay->id == TV_VID)) {
		val1 |= ALPHA_PN_GRA_AND_TV_VID;
		if (fb3->overlay->id == PN_GRA)
			val2 |= ALPHA_PATH_PN_PATH_ALPHA;
		else if (fb3->overlay->id == TV_VID)
			val2 |= ALPHA_PATH_TV_PATH_ALPHA;
		else
			BUG_ON(1);
	} else if ((fb1->overlay->id == PN_VID) &&
		(fb2->overlay->id == TV_GRA)) {
		val1 |= ALPHA_PN_VID_AND_TV_GRA;
		if (fb3->overlay->id == PN_VID)
			val2 |= ALPHA_PATH_PN_PATH_ALPHA;
		else if (fb3->overlay->id == TV_GRA)
			val2 |= ALPHA_PATH_TV_PATH_ALPHA;
		else
			BUG_ON(1);
	} else if ((fb1->overlay->id == PN_VID) &&
		(fb2->overlay->id == TV_VID)) {
		val1 |= ALPHA_PN_VID_AND_TV_VID;
		if (fb3->overlay->id == PN_VID)
			val2 |= ALPHA_PATH_PN_PATH_ALPHA;
		else if (fb3->overlay->id == TV_VID)
			val2 |= ALPHA_PATH_TV_PATH_ALPHA;
		else
			BUG_ON(1);
	} else if ((fb1->overlay->id == TV_GRA) &&
		(fb2->overlay->id == TV_VID)) {
		val1 |= ALPHA_TV_GRA_AND_TV_VID;
		if (fb3->overlay->id == TV_GRA)
			val2 |= ALPHA_PATH_GRA_PATH_ALPHA;
		else if (fb3->overlay->id == TV_VID)
			val2 |= ALPHA_PATH_VID_PATH_ALPHA;
		else
			BUG_ON(1);
	} else {
		val1 |= ALPHA_PN_GRA_AND_PN_VID;
		val2 |= ALPHA_PATH_GRA_PATH_ALPHA;
	}

	pa1->alphapath = val1;
	pa1->config = val2;

	return 0;
}

static int set_path_alpha(struct fb_info *info, unsigned long arg)
{
	void __user *argp = (void __user *)arg;
	struct mmpfb_info *fbi = info->par;
	struct mmp_alpha pa;

	if (copy_from_user(&fbi->pa, argp, sizeof(struct mmp_alpha)))
		return -EFAULT;

	alpha_mode_to_ctrl(&pa, &fbi->pa);

	return mmp_overlay_set_path_alpha(fbi->overlay, &pa);
}

static int set_dfc_rate(struct fb_info *info, unsigned long arg)
{
	void __user *argp = (void __user *)arg;
	struct mmpfb_info *fbi = info->par;
	struct mmp_path *path = fbi->path;
	unsigned int fps;

	if (copy_from_user(&fps, argp, sizeof(unsigned int)))
		return -EFAULT;

	path->rate = path->original_rate * fps / FPS_60;
	queue_work(path->vsync.dfc_wq, &path->vsync.dfc_work);
	return 0;
}

static int get_dfc_rate(struct fb_info *info, unsigned long arg)
{
	void __user *argp = (void __user *)arg;
	struct mmpfb_info *fbi = info->par;
	struct mmp_path *path = fbi->path;
	unsigned int fps;
	unsigned int rate;

	rate = mmp_path_get_dfc_rate(fbi->path);
	fps = rate * FPS_60 / path->original_rate;
	if (rate == 0) {
		dev_dbg(info->dev, "Invalid dfc rate\n");
		return -EFAULT;
	}

	if (copy_to_user(argp, &fps, sizeof(sizeof(unsigned int))))
		return -EFAULT;

	return 0;
}

int mmpfb_ioctl(struct fb_info *info, unsigned int cmd, unsigned long arg)
{
	struct mmpfb_info *fbi = info->par;

	dev_dbg(info->dev, "cmd 0x%x, arg 0x%lx\n", cmd, arg);

	if (!mmp_path_ctrl_safe(fbi->path))
		return -EINVAL;

	switch (cmd) {
	case FB_IOCTL_QUERY_GLOBAL_INFO:
		get_global_info(info, arg);
		break;
	case FB_IOCTL_FLIP_COMMIT:
		enable_commit(info, arg);
		break;
	case FB_IOCTL_WAIT_VSYNC:
		mmp_wait_vsync(&fbi->path->vsync);
		break;
	case FB_IOCTL_FLIP_USR_BUF:
#ifdef CONFIG_MMP_FENCE
		return flip_buffer(info, arg);
#else
		return flip_buffer_vsync(info, arg);
#endif
	case FB_IOCTL_FLIP_VSYNC:
		return flip_buffer_vsync(info, arg);
	case FB_IOCTL_GAMMA_SET:
		return set_gamma(info, arg);
	case FB_IOCTL_SET_COLORKEYnALPHA:
		return set_colorkey_alpha(info, arg);
	case FB_IOCTL_GET_COLORKEYnALPHA:
		return get_colorkey_alpha(info, arg);
	case FB_IOCTL_ENABLE_DMA:
		return enable_dma(info, arg);
	case FB_IOCTL_VSMOOTH_EN:
		return vsmooth_en(info, arg);
	/* FB_IOCTL_ENABLE_COMMIT_DMA is only for overlay commit temporarily */
	case FB_IOCTL_ENABLE_COMMIT_DMA:
		return enable_commit_dma(info, arg);
	case FB_IOCTL_SET_PATHALPHA:
		return set_path_alpha(info, arg);
	case FB_IOCTL_SET_DFC_RATE:
		return set_dfc_rate(info, arg);
	case FB_IOCTL_GET_DFC_RATE:
		return get_dfc_rate(info, arg);
	default:
		dev_info(info->dev, "unknown ioctl 0x%x\n", cmd);
		return -EINVAL;
	}
	return 0;
}

#ifdef CONFIG_COMPAT
int mmpfb_compat_ioctl(struct fb_info *info, unsigned int cmd,
		unsigned long arg)
{
	int ret = -ENOIOCTLCMD;

	switch (cmd) {
	case FB_IOCTL_QUERY_GLOBAL_INFO:
	case FB_IOCTL_FLIP_USR_BUF:
	case FB_IOCTL_GAMMA_SET:
	case FB_IOCTL_ENABLE_DMA:
	case FB_IOCTL_VSMOOTH_EN:
	case FB_IOCTL_SET_COLORKEYnALPHA:
	case FB_IOCTL_GET_COLORKEYnALPHA:
	case FB_IOCTL_FLIP_VSYNC:
	case FB_IOCTL_ENABLE_COMMIT_DMA:
	case FB_IOCTL_SET_DFC_RATE:
	case FB_IOCTL_GET_DFC_RATE:
		arg = (unsigned long)compat_ptr(arg);
	case FB_IOCTL_FLIP_COMMIT:
	case FB_IOCTL_WAIT_VSYNC:
		ret = mmpfb_ioctl(info, cmd, arg);
		break;
	default:
		dev_info(info->dev, "unknown ioctl 0x%x\n", cmd);
		break;
	}

	return ret;
}
#endif
