/*
 * linux/drivers/video/mmp/fb/vsync_notify.c
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

#include "mmpfb.h"
#include <video/mmp_trace.h>

void mmpfb_wait_vsync(struct mmpfb_info *fbi)
{
	dev_dbg(fbi->dev, "wait vsync: vcnt %d, irq_en_ref: %d\n",
			atomic_read(&fbi->vsync.vcnt), atomic_read(&fbi->path->irq_en_ref));
	/*
	 * for N buffer cases,
	 * #1- (N-2) buffers passed in one frame:
	 *  no need to wait vsync
	 * #N-1 buffer need to wait vsync
	 * e.g, for two buffer case, always wait
	 * for three buffer, only 2nd buffer need wait
	 *
	 * If vcnt is 0, we can't decrease it.
	 */
	if (atomic_read(&fbi->vsync.vcnt))
		if (!atomic_dec_and_test(&fbi->vsync.vcnt))
			return;
	mmp_wait_vsync(&fbi->path->vsync);
}

static void mmpfb_vcnt_clean(struct mmpfb_info *fbi)
{
	atomic_set(&fbi->vsync.vcnt, fbi->buffer_num - 1);
}

/* Get time stamp of vsync */
static ssize_t mmpfb_vsync_ts_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct mmpfb_info *fbi = dev_get_drvdata(dev);

	return sprintf(buf, "%llx\n", fbi->vsync.ts_nano);
}
DEVICE_ATTR(vsync_ts, S_IRUGO, mmpfb_vsync_ts_show, NULL);

static ssize_t mmpfb_vsync_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct mmpfb_info *fbi = dev_get_drvdata(dev);
	int s = 0;
	s += sprintf(buf + s, "%s vsync uevent report\n",
		fbi->vsync.en ? "enable" : "disable");
	return s;
}

static ssize_t mmpfb_vsync_store(
		struct device *dev, struct device_attribute *attr,
		const char *buf, size_t size)
{
	struct mmpfb_info *fbi = dev_get_drvdata(dev);

	if (sysfs_streq(buf, "u1")) {
		fbi->vsync.en = 1;
		mmp_path_set_irq(fbi->path, 1);
	} else if (sysfs_streq(buf, "u0")) {
		fbi->vsync.en = 0;
		mmp_path_set_irq(fbi->path, 0);
	} else {
		dev_dbg(fbi->dev, "invalid input, please echo as follow:\n");
		dev_dbg(fbi->dev, "\tu1: enable vsync uevent\n");
		dev_dbg(fbi->dev, "\tu0: disable vsync uevent\n");
	}
	trace_vsync(fbi->vsync.en);
	dev_dbg(fbi->dev, "vsync_en = %d\n", fbi->vsync.en);
	return size;
}
DEVICE_ATTR(vsync, S_IRUGO | S_IWUSR, mmpfb_vsync_show, mmpfb_vsync_store);

static void mmpfb_overlay_vsync_cb(void *data)
{
	struct mmpfb_info *fbi = (struct mmpfb_info *)data;

	if (fbi)
		queue_work(fbi->vsync.wq, &fbi->vsync.fence_work);
}

static void mmpfb_vsync_cb(void *data)
{
	struct timespec vsync_time;
	struct mmpfb_info *fbi = (struct mmpfb_info *)data;

	/* in vsync callback */
	mmpfb_vcnt_clean(fbi);

	/* Get time stamp of vsync */
	ktime_get_ts(&vsync_time);
	fbi->vsync.ts_nano = ((uint64_t)vsync_time.tv_sec)
		* 1000 * 1000 * 1000 +
		((uint64_t)vsync_time.tv_nsec);

	if (atomic_read(&fbi->op_count)) {
		queue_work(fbi->vsync.wq, &fbi->vsync.work);
		queue_work(fbi->vsync.wq, &fbi->vsync.fence_work);
	}
}

static void mmpfb_vsync_notify_work(struct work_struct *work)
{
	struct mmpfb_vsync *vsync =
		container_of(work, struct mmpfb_vsync, work);
	struct mmpfb_info *fbi =
		container_of(vsync, struct mmpfb_info, vsync);

	sysfs_notify(&fbi->dev->kobj, NULL, "vsync_ts");
}

int mmpfb_overlay_vsync_notify_init(struct mmpfb_info *fbi)
{
	int ret = 0;
	struct mmp_vsync_notifier_node *notifier_node;

	notifier_node = &fbi->vsync.notifier_node;
	fbi->vsync.wq =
		alloc_workqueue(fbi->name, WQ_HIGHPRI |
				WQ_UNBOUND | WQ_MEM_RECLAIM, 1);
	if (!fbi->vsync.wq) {
		dev_err(fbi->dev, "alloc_workqueue failed\n");
		return ret;
	}

	INIT_WORK(&fbi->vsync.fence_work, mmpfb_overlay_fence_work);
	notifier_node->cb_notify = mmpfb_overlay_vsync_cb;
	notifier_node->cb_data = (void *)fbi;

	mmp_register_vsync_cb(&fbi->path->vsync,
			notifier_node);
	return ret;
}

int mmpfb_vsync_notify_init(struct mmpfb_info *fbi)
{
	int ret = 0;
	struct mmp_vsync_notifier_node *notifier_node;

	notifier_node = &fbi->vsync.notifier_node;
	ret = device_create_file(fbi->dev, &dev_attr_vsync);
	if (ret < 0) {
		dev_err(fbi->dev, "device attr create fail: %d\n", ret);
		return ret;
	}

	ret = device_create_file(fbi->dev, &dev_attr_vsync_ts);
	if (ret < 0) {
		dev_err(fbi->dev, "device attr create fail: %d\n", ret);
		return ret;
	}

	fbi->vsync.wq =
		alloc_workqueue(fbi->name, WQ_HIGHPRI |
				WQ_UNBOUND | WQ_MEM_RECLAIM, 1);
	if (!fbi->vsync.wq) {
		dev_err(fbi->dev, "alloc_workqueue failed\n");
		return ret;
	}

	INIT_WORK(&fbi->vsync.work, mmpfb_vsync_notify_work);
	INIT_WORK(&fbi->vsync.fence_work, mmpfb_overlay_fence_work);
	notifier_node->cb_notify = mmpfb_vsync_cb;
	notifier_node->cb_data = (void *)fbi;
	mmp_register_vsync_cb(&fbi->path->vsync,
				notifier_node);

	mmpfb_vcnt_clean(fbi);

	return ret;
}

void mmpfb_vsync_notify_deinit(struct mmpfb_info *fbi)
{
	mmp_unregister_vsync_cb(&fbi->vsync.notifier_node);
	device_remove_file(fbi->dev, &dev_attr_vsync_ts);
	destroy_workqueue(fbi->vsync.wq);
}
