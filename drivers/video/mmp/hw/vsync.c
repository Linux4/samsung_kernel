/*
 * linux/drivers/video/mmp/hw/vsync.c
 * vsync driver support
 *
 * Copyright (C) 2013 Marvell Technology Group Ltd.
 * Authors:  Yu Xu <yuxu@marvell.com>
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

#include <video/mmp_disp.h>

static void mmp_overlay_vsync_cb(void *data)
{
	struct mmp_overlay *overlay = (struct mmp_overlay *)data;
	struct mmp_vdma_info *vdma;

	vdma = overlay->vdma;
	if (vdma && vdma->ops && vdma->ops->vsync_cb)
		vdma->ops->vsync_cb(vdma);
}

static void mmp_overlay_vsync_notify_init(struct mmp_vsync *vsync, struct mmp_overlay *overlay)
{
	struct mmp_vsync_notifier_node *notifier_node;

	notifier_node = &overlay->notifier_node;
	notifier_node->cb_notify = mmp_overlay_vsync_cb;
	notifier_node->cb_data = (void *)overlay;
	mmp_register_vsync_cb(vsync, notifier_node);
}

static void mmp_set_irq(struct mmp_vsync *vsync, int on)
{
	if (vsync->type == LCD_VSYNC || vsync->type == LCD_SPECIAL_VSYNC)
		mmp_path_set_irq(vsync->path, on);
	else if (vsync->type == DSI_VSYNC || vsync->type == DSI_SPECIAL_VSYNC)
		mmp_dsi_set_irq(vsync->dsi, on);
}

static void mmp_vsync_wait(struct mmp_vsync *vsync)
{
	atomic_set(&vsync->ready, 0);
	wait_event_interruptible_timeout(vsync->waitqueue,
		atomic_read(&vsync->ready), HZ / 20);
}

static void mmp_handle_irq(struct mmp_vsync *vsync)
{
	struct mmp_vsync_notifier_node *notifier_node;

	/*
	 * The cb_notify will do some work like init vcnt,
	 * so make it called before wake_up_all to make vcnt
	 * operation be after vcnt init.
	 */
	list_for_each_entry(notifier_node, &vsync->notifier_list,
			node) {
		notifier_node->cb_notify(notifier_node->cb_data);
	}

	/*
	 * wake up waitqueue after all cb notifier done;
	 */
	if (!atomic_read(&vsync->ready)) {
		atomic_set(&vsync->ready, 1);
		wake_up_all(&vsync->waitqueue);
	}
}

static void mmp_vsync_dfc_work(struct work_struct *work)
{
	struct mmp_vsync *vsync =
		container_of(work, struct mmp_vsync, dfc_work);
	struct mmp_path *path =
		container_of(vsync, struct mmp_path, vsync);

	mmp_path_set_dfc_rate(path, path->rate, GC_REQUEST);
}

void mmp_vsync_init(struct mmp_vsync *vsync)
{
	int i;

	init_waitqueue_head(&vsync->waitqueue);
	vsync->handle_irq = mmp_handle_irq;
	vsync->wait_vsync = mmp_vsync_wait;
	vsync->set_irq = mmp_set_irq;
	INIT_LIST_HEAD(&vsync->notifier_list);

	vsync->dfc_wq =
		alloc_workqueue("LCD_DFC_WQ", WQ_HIGHPRI |
				WQ_UNBOUND | WQ_MEM_RECLAIM, 1);
	if (!vsync->dfc_wq)
		pr_err("dfc: alloc_workqueue failed\n");

	INIT_WORK(&vsync->dfc_work, mmp_vsync_dfc_work);

	if (vsync->type == LCD_VSYNC) {
		struct mmp_path *path = vsync->path;
		for (i = 0; i < path->overlay_num; i++)
			mmp_overlay_vsync_notify_init(vsync, &path->overlays[i]);
	}
}

void mmp_vsync_deinit(struct mmp_vsync *vsync)
{
	struct mmp_vsync_notifier_node *notifier_node;

	list_for_each_entry(notifier_node, &vsync->notifier_list,
			node)
		mmp_unregister_vsync_cb(notifier_node);

	vsync->handle_irq = NULL;
	vsync->wait_vsync = NULL;
	vsync->set_irq = NULL;
}
