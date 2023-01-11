/*
 * linux/drivers/video/mmp/fb/mmpfb.h
 * Framebuffer driver for Marvell Display controller.
 *
 * Copyright (C) 2012 Marvell Technology Group Ltd.
 * Authors: Zhou Zhu <zzhu3@marvell.com>
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

#ifndef _MMP_FB_H_
#define _MMP_FB_H_

#include <video/mmp_disp.h>
#include <linux/fb.h>
#include <video/mmp_ioctl.h>

struct mmpfb_vsync {
	int en;
	uint64_t ts_nano;
	struct work_struct work;
	struct work_struct fence_work;
	struct workqueue_struct *wq;
	struct mmp_vsync_notifier_node notifier_node;
	/* tricky: count for 3 buffer sync */
	atomic_t vcnt;
};

/* LCD controller private state. */
struct mmpfb_info {
	struct device	*dev;
	int	id;
	const char	*name;

	struct fb_info	*fb_info;
	/* basicaly videomode is for output */
	struct fb_videomode	mode;
	int	pix_fmt;

	void	*fb_start;
	int	fb_size;
	int	buffer_num;
	dma_addr_t	fb_start_dma;

	struct mmp_overlay	*overlay;
	struct mmp_path	*path;

	struct mutex	access_ok;

	unsigned int		pseudo_palette[16];
	int output_fmt;

	struct mmpfb_vsync vsync;

	/* fence */
	struct mmp_surface fence_surface;
	struct sw_sync_timeline *fence_timeline;
	unsigned int fence_next_frame_id;
	unsigned int fence_commit_id;
	struct mutex fence_mutex;

	struct mmp_alpha pa;

	struct mmp_colorkey_alpha ca;
	atomic_t	op_count;
};

static unsigned long virtual_x;
static unsigned long virtual_y;
static unsigned int	is_virtual_display;

static inline void mmpfb_check_virtual_mode(struct mmp_mode *mode)
{
	if (!mode)
		return;

#if defined(CONFIG_MMP_VIRTUAL_RESOLUTION)
	virtual_x = CONFIG_MMP_VIRTUAL_RESOLUTION_X;
	virtual_y = CONFIG_MMP_VIRTUAL_RESOLUTION_Y;
#endif

	if (virtual_x && virtual_y) {
		is_virtual_display = 1;
		mode->xres = virtual_x;
		mode->yres = virtual_y;
		mode->pixclock_freq =  (virtual_x + mode->left_margin +
			mode->right_margin + mode->hsync_len) * (virtual_y +
			mode->vsync_len + mode->upper_margin
			+ mode->lower_margin) * mode->refresh;
	} else {
		is_virtual_display = 0;
		mode->xres = mode->real_xres;
		mode->yres = mode->real_yres;
	}
}

/* FIXME: 0xc000 was for decompression header
 * pitch_align(256_align(1080*4)*1920/512)*3 */
#define MMPFB_DEFAULT_SIZE (ALIGN(1080, 64) * ALIGN(1920, 4) * 4 * 3 + 0xcc00)

extern int mmpfb_vsync_notify_init(struct mmpfb_info *fbi);
extern void mmpfb_vsync_notify_deinit(struct mmpfb_info *fbi);
extern int mmpfb_overlay_vsync_notify_init(struct mmpfb_info *fbi);
void mmpfb_wait_vsync(struct mmpfb_info *fbi);
extern int mmpfb_ioctl(struct fb_info *info,
			unsigned int cmd, unsigned long arg);
#ifdef CONFIG_COMPAT
extern int mmpfb_compat_ioctl(struct fb_info *info,
			unsigned int cmd, unsigned long arg);
#endif
extern void check_pitch(struct mmp_surface *surface);

/* fence interface */
#ifdef CONFIG_MMP_FENCE
extern int mmpfb_fence_sync_open(struct fb_info *info);
extern int mmpfb_fence_sync_release(struct fb_info *info);
extern int mmpfb_ioctl_flip_fence(struct fb_info *info, unsigned long arg);
extern void mmpfb_overlay_fence_work(struct work_struct *work);
extern void mmpfb_fence_pause(struct mmpfb_info *fbi);
extern void mmpfb_fence_store_commit_id(struct mmpfb_info *fbi);
#else
static inline int mmpfb_fence_sync_open(struct fb_info *info) { return 0; }
static inline int mmpfb_fence_sync_release(struct fb_info *info) { return 0; }
static inline int mmpfb_ioctl_flip_fence(struct fb_info *info, unsigned long arg) { return 0; }
static inline void mmpfb_overlay_fence_work(struct work_struct *work) {}
static inline void mmpfb_fence_pause(struct mmpfb_info *fbi) {}
static inline void mmpfb_fence_store_commit_id(struct mmpfb_info *fbi) {}
#endif
#endif /* _MMP_FB_H_ */
