/*
 * Samsung Exynos5 SoC series FIMC-IS driver
 *
 * exynos5 fimc-is video functions
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <asm/cacheflush.h>

#include "is-core.h"
#include "is-param.h"
#include "is-device-ischain.h"
#include "pablo-debug.h"
#include "pablo-mem.h"
#include "is-hw-common-dma.h"
#include "is-device-csi.h"

static int is_sensor_subdev_stop(void *device, struct is_queue *iq);

int is_subdev_probe(struct is_subdev *subdev,
	u32 instance,
	u32 id,
	const char *name,
	const struct is_subdev_ops *sops)
{
	FIMC_BUG(!subdev);
	FIMC_BUG(!name);

	subdev->id = id;
	subdev->instance = instance;
	subdev->ops = sops;
	subdev->name = name;
	subdev->state = 0L;

	return 0;
}
KUNIT_EXPORT_SYMBOL(is_subdev_probe);

int is_subdev_open(struct is_subdev *subdev, struct is_video_ctx *vctx, u32 instance)
{
	int ret = 0;
	struct is_video *video;

	FIMC_BUG(!subdev);

	if (test_bit(IS_SUBDEV_OPEN, &subdev->state)) {
		mserr("already open", subdev, subdev);
		ret = -EPERM;
		goto p_err;
	}

	/* If it is internal VC, skip vctx setting. */
	if (vctx) {
		subdev->vctx = vctx;
		video = GET_VIDEO(vctx);
		subdev->vid = (video) ? video->id : 0;
	}

	subdev->stm = pablo_lib_get_stream_prefix(instance);
	subdev->instance = instance;
	subdev->cid = CAPTURE_NODE_MAX;
	subdev->input.width = 0;
	subdev->input.height = 0;
	subdev->input.crop.x = 0;
	subdev->input.crop.y = 0;
	subdev->input.crop.w = 0;
	subdev->input.crop.h = 0;
	subdev->output.width = 0;
	subdev->output.height = 0;
	subdev->output.crop.x = 0;
	subdev->output.crop.y = 0;
	subdev->output.crop.w = 0;
	subdev->output.crop.h = 0;
	subdev->bits_per_pixel = 0;
	subdev->memory_bitwidth = 0;
	subdev->sbwc_type = 0;

	clear_bit(IS_SUBDEV_START, &subdev->state);
	clear_bit(IS_SUBDEV_RUN, &subdev->state);
	set_bit(IS_SUBDEV_OPEN, &subdev->state);

p_err:
	return ret;
}

int is_sensor_subdev_open(struct is_device_sensor *device,
	struct is_video_ctx *vctx)
{
	int ret = 0;
	struct is_subdev *subdev;

	FIMC_BUG(!device);
	FIMC_BUG(!vctx);
	FIMC_BUG(!GET_VIDEO(vctx));

	subdev = vctx->subdev;

	ret = is_subdev_open(subdev, vctx, device->instance);
	if (ret) {
		mserr("is_subdev_open is fail(%d)", subdev, subdev, ret);
		goto err_subdev_open;
	}

	set_bit(IS_SUBDEV_EXTERNAL_USE, &subdev->state);

	return 0;

err_subdev_open:
	return ret;
}
KUNIT_EXPORT_SYMBOL(is_sensor_subdev_open);

int is_ischain_subdev_open(struct is_device_ischain *device,
	struct is_video_ctx *vctx)
{
	int ret = 0;
	int ret_err = 0;
	struct is_subdev *subdev;

	FIMC_BUG(!device);
	FIMC_BUG(!vctx);
	FIMC_BUG(!GET_VIDEO(vctx));

	subdev = vctx->subdev;

	ret = is_subdev_open(subdev, vctx, device->instance);
	if (ret) {
		merr("is_subdev_open is fail(%d)", device, ret);
		goto err_subdev_open;
	}

	set_bit(IS_SUBDEV_EXTERNAL_USE, &subdev->state);

	ret = is_ischain_open_wrap(device, false);
	if (ret) {
		merr("is_ischain_open_wrap is fail(%d)", device, ret);
		goto err_ischain_open;
	}

	return 0;

err_ischain_open:
	ret_err = is_subdev_close(subdev);
	if (ret_err)
		merr("is_subdev_close is fail(%d)", device, ret_err);

	clear_bit(IS_SUBDEV_EXTERNAL_USE, &subdev->state);

err_subdev_open:
	return ret;
}
KUNIT_EXPORT_SYMBOL(is_ischain_subdev_open);

int is_subdev_close(struct is_subdev *subdev)
{
	int ret = 0;

	if (!test_bit(IS_SUBDEV_OPEN, &subdev->state)) {
		mswarn("subdev is already close", subdev, subdev);
		goto p_err;
	}

	subdev->leader = NULL;
	subdev->vctx = NULL;
	subdev->vid = 0;

	clear_bit(IS_SUBDEV_OPEN, &subdev->state);
	clear_bit(IS_SUBDEV_RUN, &subdev->state);
	clear_bit(IS_SUBDEV_START, &subdev->state);
	clear_bit(IS_SUBDEV_FORCE_SET, &subdev->state);

p_err:
	return ret;
}

int is_sensor_subdev_close(struct is_device_sensor *device,
	struct is_video_ctx *vctx)
{
	int ret = 0;
	struct is_subdev *subdev;
	struct is_queue *iq = vctx->queue;

	FIMC_BUG(!device);
	FIMC_BUG(!vctx);

	subdev = vctx->subdev;
	if (!subdev) {
		merr("subdev is NULL", device);
		ret = -EINVAL;
		goto p_err;
	}

	if (test_bit(IS_SENSOR_FRONT_START, &device->state)) {
		merr("sudden close, call sensor_front_stop()", device);

		ret = is_sensor_front_stop(device, true);
		if (ret)
			merr("is_sensor_front_stop is fail(%d)", device, ret);
	}

	if (test_bit(IS_SUBDEV_START, &subdev->state)) {
		merr("sudden close, call is_sensor_subdev_stop()", device);

		ret = is_sensor_subdev_stop(device, iq);
		if (ret)
			merr("is_sensor_subdev_stop is fail(%d)", device, ret);
	}

	ret = is_subdev_close(subdev);
	if (ret)
		merr("is_subdev_close is fail(%d)", device, ret);

	vctx->subdev = NULL;

	clear_bit(IS_SUBDEV_EXTERNAL_USE, &subdev->state);

p_err:
	return ret;
}
KUNIT_EXPORT_SYMBOL(is_sensor_subdev_close);

int is_ischain_subdev_close(struct is_device_ischain *device,
	struct is_video_ctx *vctx)
{
	int ret = 0;
	struct is_subdev *subdev;

	FIMC_BUG(!device);
	FIMC_BUG(!vctx);

	subdev = vctx->subdev;
	if (!subdev) {
		merr("subdev is NULL", device);
		ret = -EINVAL;
		goto p_err;
	}

	vctx->subdev = NULL;

	ret = is_subdev_close(subdev);
	if (ret)
		merr("is_subdev_close is fail(%d)", device, ret);

	clear_bit(IS_SUBDEV_EXTERNAL_USE, &subdev->state);

	ret = is_ischain_close_wrap(device);
	if (ret) {
		merr("is_ischain_close_wrap is fail(%d)", device, ret);
		goto p_err;
	}

p_err:
	return ret;
}
KUNIT_EXPORT_SYMBOL(is_ischain_subdev_close);

static int is_subdev_start(struct is_subdev *is)
{
	struct is_subdev *leader;

	FIMC_BUG(!is->leader);

	if (test_bit(IS_SUBDEV_START, &is->state)) {
		mserr("already start", is, is);
		return 0;
	}

	leader = is->leader;
	if (test_bit(IS_SUBDEV_START, &leader->state)) {
		mserr("leader%d is ALREADY started", is, is, leader->id);
		return -EINVAL;
	}

	set_bit(IS_SUBDEV_START, &is->state);

	return 0;
}

static int is_subdev_s_fmt(struct is_subdev *is, struct is_queue *iq)
{
	u32 pixelformat, width, height;

	FIMC_BUG(!iq->framecfg.format);

	pixelformat = iq->framecfg.format->pixelformat;
	width = iq->framecfg.width;
	height = iq->framecfg.height;

	is->output.width = width;
	is->output.height = height;

	is->output.crop.x = 0;
	is->output.crop.y = 0;
	is->output.crop.w = is->output.width;
	is->output.crop.h = is->output.height;

	is->bits_per_pixel = iq->framecfg.format->bitsperpixel[0];
	is->memory_bitwidth = iq->framecfg.format->hw_bitwidth;
	is->sbwc_type = (iq->framecfg.hw_pixeltype & PIXEL_TYPE_EXTRA_MASK)
			>> PIXEL_TYPE_EXTRA_SHIFT;

	return 0;
}

static int is_sensor_subdev_start(void *device, struct is_queue *iq)
{
	struct is_device_sensor *ids = (struct is_device_sensor *)device;
	struct is_video_ctx *ivc = iq->vbq->drv_priv;
	struct is_subdev *is = ivc->subdev;

	if (!is) {
		merr("subdev is NULL", ids);
		return -EINVAL;
	}

	if (!test_bit(IS_SENSOR_S_INPUT, &ids->state)) {
		mserr("device is not yet init", ids, is);
		return -EINVAL;
	}

	if (test_bit(IS_SUBDEV_START, &is->state)) {
		mserr("already start", is, is);
		return 0;
	}

	set_bit(IS_SUBDEV_START, &is->state);

	return 0;
}

static int is_sensor_subdev_stop(void *device, struct is_queue *iq)
{
	struct is_device_sensor *ids = (struct is_device_sensor *)device;
	struct is_video_ctx *ivc = iq->vbq->drv_priv;
	struct is_subdev *is = ivc->subdev;
	struct is_framemgr *framemgr;
	unsigned long flags;
	struct is_frame *frame;

	if (!is) {
		merr("subdev is NULL", ids);
		return -EINVAL;
	}

	if (!test_bit(IS_SUBDEV_START, &is->state)) {
		merr("already stop", ids);
		return 0;
	}

	framemgr = GET_SUBDEV_FRAMEMGR(is);
	if (!framemgr) {
		merr("framemgr is NULL", ids);
		return -EINVAL;
	}

	framemgr_e_barrier_irqs(framemgr, FMGR_IDX_16, flags);

	frame = peek_frame(framemgr, FS_PROCESS);
	while (frame) {
		CALL_VOPS(is->vctx, done, frame->index, VB2_BUF_STATE_ERROR);
		trans_frame(framemgr, frame, FS_COMPLETE);
		frame = peek_frame(framemgr, FS_PROCESS);
	}

	frame = peek_frame(framemgr, FS_REQUEST);
	while (frame) {
		CALL_VOPS(is->vctx, done, frame->index, VB2_BUF_STATE_ERROR);
		trans_frame(framemgr, frame, FS_COMPLETE);
		frame = peek_frame(framemgr, FS_REQUEST);
	}

	framemgr_x_barrier_irqr(framemgr, FMGR_IDX_16, flags);

	clear_bit(IS_SUBDEV_RUN, &is->state);
	clear_bit(IS_SUBDEV_START, &is->state);

	return 0;
}

static int is_sensor_subdev_s_fmt(void *device, struct is_queue *iq)
{
	struct is_device_sensor *ids = (struct is_device_sensor *)device;
	struct is_video_ctx *ivc = iq->vbq->drv_priv;
	struct is_subdev *is = ivc->subdev;
	int ret;

	if (!is) {
		merr("subdev is NULL", ids);
		return -EINVAL;
	}

	ret = is_subdev_s_fmt(is, iq);
	if (ret) {
		merr("is_subdev_s_format is fail(%d)", ids, ret);
		return ret;
	}

	return 0;
}

static int is_sensor_subdev_reqbufs(void *device, struct is_queue *iq, u32 count)
{
	struct is_device_sensor *ids = (struct is_device_sensor *)device;
	struct is_video_ctx *ivc = iq->vbq->drv_priv;
	struct is_subdev *is = ivc->subdev;
	struct is_framemgr *framemgr;
	struct is_frame *frame;
	int i;

	if (!is) {
		merr("subdev is NULL", ids);
		return -EINVAL;
	}

	framemgr = GET_SUBDEV_FRAMEMGR(is);
	if (!framemgr) {
		merr("framemgr is NULL", ids);
		return -EINVAL;
	}

	for (i = 0; i < count; i++) {
		frame = &framemgr->frames[i];
		frame->subdev = is;
	}

	return 0;
}

static struct is_queue_ops is_sensor_subdev_qops = {
	.start_streaming	= is_sensor_subdev_start,
	.stop_streaming		= is_sensor_subdev_stop,
	.s_fmt			= is_sensor_subdev_s_fmt,
	.reqbufs		= is_sensor_subdev_reqbufs,
};

struct is_queue_ops *is_get_sensor_subdev_qops(void)
{
	return &is_sensor_subdev_qops;
}
KUNIT_EXPORT_SYMBOL(is_get_sensor_subdev_qops);

static int is_ischain_subdev_start(void *device, struct is_queue *iq)
{
	int ret;
	struct is_device_ischain *idi = (struct is_device_ischain *)device;
	struct is_video_ctx *ivc = iq->vbq->drv_priv;
	struct is_subdev *is = ivc->subdev;

	if (!is) {
		merr("subdev is NULL", idi);
		return -EINVAL;
	}

	if (!test_bit(IS_ISCHAIN_INIT, &idi->state)) {
		mserr("device is not yet init", idi, is);
		return -EINVAL;
	}

	ret = is_subdev_start(is);
	if (ret) {
		mserr("is_subdev_start is fail(%d)", idi, is, ret);
		return ret;
	}

	return 0;
}

static int is_ischain_subdev_stop(void *device, struct is_queue *iq)
{
	struct is_device_ischain *idi = (struct is_device_ischain *)device;
	struct is_video_ctx *ivc = iq->vbq->drv_priv;
	struct is_subdev *is = ivc->subdev;
	struct is_framemgr *framemgr;
	unsigned long flags;
	struct is_frame *frame;

	if (!is) {
		merr("subdev is NULL", idi);
		return -EINVAL;
	}

	if (!test_bit(IS_SUBDEV_START, &is->state)) {
		merr("already stop", idi);
		return 0;
	}

	if (is->leader && test_bit(IS_SUBDEV_START, &is->leader->state)) {
		merr("leader%d is NOT stopped", idi, is->leader->id);
		return 0;
	}

	framemgr = GET_SUBDEV_FRAMEMGR(is);
	if (!framemgr) {
		merr("framemgr is NULL", idi);
		return -EINVAL;
	}

	framemgr_e_barrier_irqs(framemgr, FMGR_IDX_16, flags);

	if (framemgr->queued_count[FS_PROCESS] > 0) {
		framemgr_x_barrier_irqr(framemgr, FMGR_IDX_16, flags);
		merr("being processed, can't stop", idi);
		return -EINVAL;
	}

	frame = peek_frame(framemgr, FS_REQUEST);
	while (frame) {
		CALL_VOPS(is->vctx, done, frame->index, VB2_BUF_STATE_ERROR);
		trans_frame(framemgr, frame, FS_COMPLETE);
		frame = peek_frame(framemgr, FS_REQUEST);
	}

	framemgr_x_barrier_irqr(framemgr, FMGR_IDX_16, flags);

	clear_bit(IS_SUBDEV_RUN, &is->state);
	clear_bit(IS_SUBDEV_START, &is->state);

	return 0;
}

static int is_ischain_subdev_s_fmt(void *device, struct is_queue *iq)
{
	int ret;
	struct is_device_ischain *idi = (struct is_device_ischain *)device;
	struct is_video_ctx *ivc = iq->vbq->drv_priv;
	struct is_subdev *is = ivc->subdev;

	if (!is) {
		merr("subdev is NULL", idi);
		return -EINVAL;
	}

	ret = is_subdev_s_fmt(is, iq);
	if (ret) {
		merr("is_subdev_s_format is fail(%d)", idi, ret);
		return ret;
	}

	return 0;
}

static struct is_queue_ops is_ischain_subdev_qops = {
	.start_streaming	= is_ischain_subdev_start,
	.stop_streaming		= is_ischain_subdev_stop,
	.s_fmt			= is_ischain_subdev_s_fmt,
};

struct is_queue_ops *is_get_ischain_subdev_qops(void)
{
	return &is_ischain_subdev_qops;
}
KUNIT_EXPORT_SYMBOL(is_get_ischain_subdev_qops);
