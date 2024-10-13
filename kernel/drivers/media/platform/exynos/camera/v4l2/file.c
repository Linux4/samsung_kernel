// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * Copyright (c) 2023 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <media/videobuf2-dma-sg.h>

#include "is-video.h"
#include "is-core.h"
#include "is-device-ischain.h"
#include "is-device-sensor.h"
#include "is-resourcemgr.h"
#include "pablo-debug.h"
#include "pablo-lib.h"
#include "pablo-v4l2.h"
#include "pablo-v4l2-test.h"

static inline int vref_get(struct is_video *video)
{
	return atomic_inc_return(&video->refcount) - 1;
}

static inline int vref_put(struct is_video *video,
	void (*release)(struct is_video *video))
{
	int ret = 0;

	ret = atomic_sub_and_test(1, &video->refcount);
	if (ret)
		pr_debug("closed all instacne");

	return atomic_read(&video->refcount);
}

static unsigned int rsctype_vid(int vid)
{
	if (vid >= IS_VIDEO_SS0_NUM && vid <= IS_VIDEO_SS5_NUM)
		return vid - IS_VIDEO_SS0_NUM;
#if IS_ENABLED(CONFIG_PABLO_SENSOR_VC_VIDEO_NODE)
	else if (vid >= IS_VIDEO_SS0VC0_NUM && vid < IS_VIDEO_SS5VC3_NUM)
		/* FIXME: it depends on the nubmer of VC channels: 4 */
		return (vid - IS_VIDEO_SS0VC0_NUM) >> 2;
#endif
	else
		return RESOURCE_TYPE_ISCHAIN;
}

static int is_queue_close(struct is_queue *queue)
{
	frame_manager_close(&queue->framemgr);

	return 0;
}

static struct is_video_ctx *is_vctx_open(struct file *file,
	struct is_video *video,
	u32 instance,
	const char *name)
{
	struct is_video_ctx *ivc;

	if (atomic_read(&video->refcount) > IS_STREAM_COUNT) {
		err("[V%02d] can't open vctx, refcount is invalid", video->id);
		return ERR_PTR(-EINVAL);
	}

	ivc = kvzalloc(sizeof(struct is_video_ctx), GFP_KERNEL);
	if (ZERO_OR_NULL_PTR(ivc)) {
		err("[V%02d] kvzalloc is fail", video->id);
		return ERR_PTR(-ENOMEM);
	}

	ivc->refcount = vref_get(video);
	ivc->stm = pablo_lib_get_stream_prefix(instance);
	ivc->instance = instance;
	ivc->state = BIT(IS_VIDEO_CLOSE);

	file->private_data = ivc;

	return ivc;
}

static int is_vctx_close(struct file *file,
	struct is_video *video,
	struct is_video_ctx *vctx)
{
	kvfree(vctx);
	file->private_data = NULL;

	return vref_put(video, NULL);
}

static int __is_video_open(struct is_video_ctx *ivc,
	struct is_video *iv, void *device)
{
	if (!(ivc->state & BIT(IS_VIDEO_CLOSE))) {
		mverr("already open(%lX)", ivc, iv, ivc->state);
		return -EEXIST;
	}

	ivc->device		= device;
	ivc->video		= iv;
	ivc->vops.qbuf		= pablo_video_qbuf;
	ivc->vops.dqbuf		= pablo_video_dqbuf;
	ivc->vops.done		= pablo_video_buffer_done;

	ivc->state = BIT(IS_VIDEO_OPEN);

	return 0;
}

static int __is_video_close(struct is_video_ctx *ivc)
{
	struct is_video *iv = ivc->video;
	struct is_queue *iq = ivc->queue;

	if (ivc->state < BIT(IS_VIDEO_OPEN)) {
		mverr("already close(%lX)", ivc, iv, ivc->state);
		return -ENOENT;
	}

	if (iq) {
		vb2_queue_release(iq->vbq);
		kvfree(iq->vbq);
		iq->vbq = NULL;
		is_queue_close(iq);
		kvfree(iq);
	}

	/*
	 * vb2 release can call stop callback
	 * not if video node is not stream off
	 */
	ivc->device = NULL;
	ivc->state = BIT(IS_VIDEO_CLOSE);
	ivc->queue = NULL;

	return 0;
}

static int is_video_open(struct file *file)
{
	struct is_core *core;
	struct is_video *iv = video_drvdata(file);
	struct is_resourcemgr *rscmgr = iv->resourcemgr;
	void *device;
	struct is_device_ischain *idi;
	struct is_device_sensor *ids;
	struct is_video_ctx *ivc;
	u32 instance;
	int ret;

	ret = is_resource_open(rscmgr, rsctype_vid(iv->id), &device);
	if (ret) {
		err("failure in is_resource_open(): %d", ret);
		goto err_resource_open;
	}

	if (!device) {
		err("failed to get device");
		ret = -EINVAL;
		goto err_invalid_device;
	}

	if (iv->device_type == IS_DEVICE_ISCHAIN) {
		idi = (struct is_device_ischain *)device;
		instance = idi->instance;
		idi->stm = pablo_lib_get_stream_prefix(instance);

		minfo("[%s]%s\n", idi, vn_name[iv->id], __func__);
	} else {
		ids = (struct is_device_sensor *)device;

		if (ids->reboot && iv->video_type == IS_VIDEO_TYPE_LEADER) {
			warn("%s%s: failed to open sensor due to reboot",
					vn_name[iv->id], __func__);
			ret = -EINVAL;
			goto err_reboot;
		}

		core = ids->private_data;
		/* get the stream id */
		for (instance = 0; instance < IS_STREAM_COUNT; ++instance) {
			idi = &core->ischain[instance];
			if (!test_bit(IS_ISCHAIN_OPEN, &idi->state))
				break;
		}
		FIMC_BUG(instance >= IS_STREAM_COUNT);
		pablo_lib_set_stream_prefix(instance, "%s_P", ids->sensor_name);

		info("[%d:%s][%s]%s\n", instance, pablo_lib_get_stream_prefix(instance),
		     vn_name[iv->id], __func__);
	}

	ivc = is_vctx_open(file, iv, instance, vn_name[iv->id]);
	if (IS_ERR_OR_NULL(ivc)) {
		ret = PTR_ERR(ivc);
		mierr("failure in open_vctx(): %d", instance, ret);
		goto err_vctx_open;
	}

	ret = __is_video_open(ivc, iv, device);
	if (ret) {
		mierr("failure in __is_video_open(): %d", instance, ret);
		goto err_video_open;
	}

	if (iv->video_type == IS_VIDEO_TYPE_COMMON) {
		/* nothing to do */
	} else if (iv->video_type == IS_VIDEO_TYPE_LEADER) {
		if (iv->device_type == IS_DEVICE_ISCHAIN) {
			ivc->group = idi->group[iv->group_slot];
			ret = is_ischain_group_open(idi, ivc, iv->group_id);
			if (ret) {
				mierr("failure in is_ischain_group_open(): %d",
								instance, ret);
				goto err_ischain_group_open;
			}
		} else {
			ivc->group = &ids->group_sensor;
			ret = is_sensor_open(ids, ivc);
			if (ret) {
				mierr("failure in is_sensor_open(): %d",
							instance, ret);
				goto err_sensor_open;
			}
		}
	} else {
		ivc->subdev = (struct is_subdev *)((char *)device + iv->subdev_ofs);

		if (iv->device_type == IS_DEVICE_ISCHAIN) {
			ret = is_ischain_subdev_open(idi, ivc);
			if (ret) {
				mierr("failure in is_ischain_subdev_open(): %d",
								instance, ret);
				goto err_ischain_subdev_open;
			}
		} else {
			ret = is_sensor_subdev_open(ids, ivc);
			if (ret) {
				mierr("failure in is_sensor_subdev_open(): %d",
								instance, ret);
				goto err_sensor_subdev_open;
			}
		}
	}

	pablo_v4l2_test_pre(file);

	return 0;

err_sensor_subdev_open:
err_ischain_subdev_open:
err_sensor_open:
err_ischain_group_open:
	__is_video_close(ivc);

err_video_open:
	is_vctx_close(file, iv, ivc);

err_vctx_open:
err_reboot:
err_invalid_device:
err_resource_open:
	return ret;
}

ssize_t pablo_video_read(struct file *file, char __user *buf,
			 size_t count, loff_t *ppos)
{
	return pablo_v4l2_test_get_result(file, buf, count);
}

ssize_t pablo_video_write(struct file *file, const char __user *buf,
			  size_t count, loff_t *ppos)
{
	return pablo_v4l2_test_run_test(file, buf, count);
}

static int is_video_close(struct file *file)
{
	struct is_video *iv = video_drvdata(file);
	struct is_video_ctx *ivc = (struct is_video_ctx *)file->private_data;
	u32 instance;
	struct is_device_ischain *idi;
	struct is_device_sensor *ids;
	int refcount;
	int ret;

	if (iv->device_type == IS_DEVICE_ISCHAIN) {
		idi = (struct is_device_ischain *)ivc->device;
		instance = idi->instance;
	} else {
		ids = (struct is_device_sensor *)ivc->device;
		instance = ids->instance;
	}

	if (iv->video_type == IS_VIDEO_TYPE_COMMON) {
		/* nothing to do */
	} else if (iv->video_type == IS_VIDEO_TYPE_LEADER) {
		if (iv->device_type == IS_DEVICE_ISCHAIN) {
			ret = is_ischain_group_close(idi, ivc->group);
			if (ret)
				mierr("failure in is_ischain_group_close(): %d",
								instance, ret);
		} else {
			ret = is_sensor_close(ids);
			if (ret)
				mierr("failure in is_sensor_close(): %d",
								instance, ret);
		}
	} else {
		if (iv->device_type == IS_DEVICE_ISCHAIN) {
			ret = is_ischain_subdev_close(idi, ivc);
			if (ret)
				mierr("failure in is_ischain_subdev_close(): %d",
								instance, ret);
		} else {
			ret = is_sensor_subdev_close(ids, ivc);
			if (ret)
				mierr("failure in is_sensor_subdev_close(): %d",
								instance, ret);
		}
	}

	ret = __is_video_close(ivc);
	if (ret)
		mierr("failure in is_video_close(): %d", instance, ret);

	refcount = is_vctx_close(file, iv, ivc);
	if (refcount < 0)
		mierr("failure in is_vctx_close(): %d", instance, refcount);

	if (iv->device_type == IS_DEVICE_ISCHAIN)
		minfo("[%s]%s(open count: %d, ref. cont: %d): %d\n", idi, vn_name[iv->id],
				__func__, atomic_read(&idi->open_cnt), refcount, ret);
	 else
		minfo("[%s]%s(ref. cont: %d): %d\n", ids, vn_name[iv->id], __func__,
									refcount, ret);

	 pablo_v4l2_test_post(file);

	return 0;
}

int is_video_mmap(struct file *file, struct vm_area_struct *vma)
{
	struct is_video_ctx *vctx = file->private_data;
	struct is_queue *queue = GET_QUEUE(vctx);

	if (!queue)
		return EPOLLERR;

	return vb2_mmap(queue->vbq, vma);
}

static struct v4l2_file_operations pablo_v4l2_file_ops_default = {
	.owner = THIS_MODULE,
	.open = is_video_open,
	.read = pablo_video_read,
	.write = pablo_video_write,
	.release = is_video_close,
	.unlocked_ioctl = video_ioctl2,
	.mmap = is_video_mmap,
};

const struct v4l2_file_operations *pablo_get_v4l2_file_ops_default(void)
{
	return &pablo_v4l2_file_ops_default;
}
EXPORT_SYMBOL_GPL(pablo_get_v4l2_file_ops_default);

MODULE_DESCRIPTION("v4l2 file module for Exynos Pablo");
MODULE_LICENSE("GPL v2");
