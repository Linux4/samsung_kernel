/*
 * linux/drivers/video/mmp/fb/fence.c
 * Framebuffer driver for Marvell Display controller.
 *
 * Copyright (C) 2012 Marvell Technology Group Ltd.
 * Authors: Xiaolong Ye <yexl@marvell.com>
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

#include <linux/uaccess.h>
#include <linux/fb.h>
#include <linux/file.h>
#include <video/mmp_trace.h>
#include "mmpfb.h"
#include "sync.h"
#include "sw_sync.h"

static int is_addr_same(struct mmp_addr *addr1, struct mmp_addr *addr2)
{
	int i;
	for (i = 0; i < 6; i++) {
		if (addr1->phys[i] != addr2->phys[i])
			return 0;
	}
	return 1;
}

/* create a new timeline */
int mmpfb_fence_sync_open(struct fb_info *info)
{
	struct mmpfb_info *fbi = info->par;
	struct sw_sync_timeline *timeline;
	char task_comm[TASK_COMM_LEN];

	mutex_lock(&fbi->fence_mutex);
	get_task_comm(task_comm, current);
	timeline = sw_sync_timeline_create(task_comm);
	if (timeline == NULL) {
		mutex_unlock(&fbi->fence_mutex);
		return -ENOMEM;
	}
	fbi->fence_timeline = timeline;
	fbi->fence_next_frame_id = 1;
	fbi->fence_commit_id = 1;
	mutex_unlock(&fbi->fence_mutex);
	return 0;
}
EXPORT_SYMBOL_GPL(mmpfb_fence_sync_open);

/* destroy timeline and free the buffer waitlist */
int mmpfb_fence_sync_release(struct fb_info *info)
{
	struct mmpfb_info *fbi = info->par;
	struct sw_sync_timeline *timeline = fbi->fence_timeline;

	if (!timeline)
		return 0;
	mutex_lock(&fbi->fence_mutex);
	sync_timeline_destroy(&timeline->obj);
	fbi->fence_surface.addr.phys[0] = 0;
	fbi->fence_surface.addr.phys[1] = 0;
	fbi->fence_surface.addr.phys[2] = 0;
	mutex_unlock(&fbi->fence_mutex);
	return 0;
}
EXPORT_SYMBOL_GPL(mmpfb_fence_sync_release);

#ifdef CONFIG_MMP_ADAPTIVE_FPS
void adaptive_fps_wq(struct mmpfb_info *fbi)
{
	struct mmp_path *path = fbi->path;
	struct mmp_dsi *dsi = mmp_path_to_dsi(path);

	atomic_inc(&dsi->framecnt->frame_cnt);

	if (!dsi->framecnt->is_doing_wq)
		queue_work(dsi->framecnt->wq, &dsi->framecnt->work);
}
#endif

/*
 * create a new fence each time when userspace filp a buffer, bind
 * a unused fd with this fence and return this fd to the userspace.
 */
int mmpfb_ioctl_flip_fence(struct fb_info *info, unsigned long arg)
{
	void __user *argp = (void __user *)arg;
	struct mmpfb_info *fbi = info->par;
	struct mmp_surface surface;
	struct sw_sync_timeline *timeline = fbi->fence_timeline;
	int fd;
	int err;
	struct sync_pt *pt;
	struct sync_fence *fence;
#ifdef CONFIG_MMP_ADAPTIVE_FPS
	struct mmp_path *path = fbi->path;
	struct mmp_dsi *dsi = mmp_path_to_dsi(path);
#endif

	if (copy_from_user(&surface, argp, sizeof(struct mmp_surface)))
		return -EFAULT;

	trace_surface(fbi->overlay->id, &surface);

	if (!is_addr_same(&fbi->fence_surface.addr, &surface.addr)) {

#ifdef CONFIG_MMP_ADAPTIVE_FPS
		if (dsi->framecnt->adaptive_fps_en)
			adaptive_fps_wq(fbi);
#endif
		fd = get_unused_fd();
		if (fd < 0)
			return fd;
		mutex_lock(&fbi->fence_mutex);
		fbi->fence_surface = surface;
		pt = sw_sync_pt_create(timeline, ++fbi->fence_next_frame_id);
		mutex_unlock(&fbi->fence_mutex);
		if (pt == NULL) {
			err = -ENOMEM;
			goto err;
		}

		fence = sync_fence_create("fence", pt);
		if (fence == NULL) {
			sync_pt_free(pt);
			err = -ENOMEM;
			goto err;
		}

		surface.fence_fd = fd;
		if (copy_to_user((void __user *)arg, &surface,
					sizeof(struct mmp_surface))) {
			sync_fence_put(fence);
			sync_pt_free(pt);
			err = -EFAULT;
			goto err;
		}

		sync_fence_install(fence, fd);
	}

	check_pitch(&surface);
	mmp_overlay_set_surface(fbi->overlay, &surface);

	return 0;
err:
	put_unused_fd(fd);
	return err;
}
EXPORT_SYMBOL_GPL(mmpfb_ioctl_flip_fence);

void mmpfb_overlay_fence_work(struct work_struct *work)
{
	struct mmpfb_vsync *vsync =
	       container_of(work, struct mmpfb_vsync, fence_work);
	struct mmpfb_info *fbi =
	       container_of(vsync, struct mmpfb_info, vsync);
	struct sw_sync_timeline *timeline;

	if (!atomic_read(&fbi->op_count))
		return;

	timeline = fbi->fence_timeline;
	if (!timeline)
		return;

	/*
	 *  init next_frame_id = 1, when flip one frame, next_frame_id = 2,
	 *  fence_commit_id = 2; When VSYNC comes, if time_line.value <
	 *  fence_commit_id-1, then set time_line.value = fence_commit_id
	 * -1, and signal to release PT(previous timeline point).
	 */
	mutex_lock(&fbi->fence_mutex);
	trace_fence(&timeline->obj, fbi->overlay->id, fbi->fence_commit_id,
		fbi->fence_next_frame_id, 0);
	if (timeline->value < (fbi->fence_commit_id - 1)) {
		timeline->value = fbi->fence_commit_id - 1;
		sync_timeline_signal(&timeline->obj);
	}
	mutex_unlock(&fbi->fence_mutex);
}
EXPORT_SYMBOL_GPL(mmpfb_overlay_fence_work);

/* when DMA off, need to call the fuction to signal all the waiting fence */
void mmpfb_fence_pause(struct mmpfb_info *fbi)
{
	struct sw_sync_timeline *timeline = fbi->fence_timeline;
	if (!timeline)
		return;
	mutex_lock(&fbi->fence_mutex);
	fbi->fence_next_frame_id += 1;
	fbi->fence_commit_id = fbi->fence_next_frame_id;
	trace_fence(&timeline->obj, fbi->overlay->id, fbi->fence_commit_id,
		fbi->fence_next_frame_id, 1);
	mutex_unlock(&fbi->fence_mutex);
}
EXPORT_SYMBOL_GPL(mmpfb_fence_pause);

void mmpfb_fence_store_commit_id(struct mmpfb_info *fbi)
{
	struct fb_info *info;
	struct mmpfb_info *tfbi;
	int i = 0;

	do {
		info = registered_fb[i++];
		if (info) {
			tfbi = info->par;
			mutex_lock(&tfbi->fence_mutex);
			tfbi->fence_commit_id = tfbi->fence_next_frame_id;
			mutex_unlock(&tfbi->fence_mutex);
		}
	} while (info);
}
EXPORT_SYMBOL_GPL(mmpfb_fence_store_commit_id);
