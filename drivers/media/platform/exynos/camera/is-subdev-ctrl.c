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
	memset(subdev->name, 0x0, sizeof(subdev->name));
	strncpy(subdev->name, name, sizeof(char[3]));
	subdev->state = 0L;

	/* for internal use */
	frame_manager_probe(&subdev->internal_framemgr, subdev->id, name);

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
	subdev->lossy_byte32num = 0;

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
	clear_bit(IS_SUBDEV_VOTF_USE, &subdev->state);

	/* for internal use */
	clear_bit(IS_SUBDEV_INTERNAL_S_FMT, &subdev->state);

p_err:
	return ret;
}

int is_sensor_subdev_close(struct is_device_sensor *device,
	struct is_video_ctx *vctx)
{
	int ret = 0;
	struct is_subdev *subdev;
	struct is_queue *iq = &vctx->queue;

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

int pablo_subdev_buffer_init(struct is_subdev *is, struct vb2_buffer *vb)
{
	struct is_framemgr *framemgr = GET_SUBDEV_FRAMEMGR(is);
	unsigned int index = vb->index;
	struct is_device_ischain *idi = GET_DEVICE(is->vctx);

	if (index >= framemgr->num_frames) {
		mserr("out of range (%d >= %d)", idi, is, index, framemgr->num_frames);
		return -EINVAL;
	}

	return 0;
}
KUNIT_EXPORT_SYMBOL(pablo_subdev_buffer_init);

int is_subdev_buffer_queue(struct is_subdev *subdev,
	struct vb2_buffer *vb)
{
	int ret = 0;
	unsigned long flags;
	struct is_framemgr *framemgr = GET_SUBDEV_FRAMEMGR(subdev);
	struct is_frame *frame;
	unsigned int index = vb->index;

	FIMC_BUG(!subdev);
	FIMC_BUG(!framemgr);
	FIMC_BUG(index >= framemgr->num_frames);

	frame = &framemgr->frames[index];

	/* 1. check frame validation */
	if (unlikely(!test_bit(FRAME_MEM_INIT, &frame->mem_state))) {
		mserr("frame %d is NOT init", subdev, subdev, index);
		ret = -EINVAL;
		goto p_err;
	}

	/* 2. update frame manager */
	framemgr_e_barrier_irqs(framemgr, FMGR_IDX_17, flags);

	if (frame->state == FS_FREE) {
		trans_frame(framemgr, frame, FS_REQUEST);
	} else {
		mserr("frame %d is invalid state(%d)\n", subdev, subdev, index, frame->state);
		frame_manager_print_queues(framemgr);
		ret = -EINVAL;
	}

	framemgr_x_barrier_irqr(framemgr, FMGR_IDX_17, flags);

p_err:
	return ret;
}
KUNIT_EXPORT_SYMBOL(is_subdev_buffer_queue);

int is_subdev_buffer_finish(struct is_subdev *subdev,
	struct vb2_buffer *vb)
{
	int ret = 0;
	struct is_device_ischain *device;
	struct is_framemgr *framemgr;
	struct is_frame *frame;
	unsigned int index = vb->index;

	if (!subdev) {
		warn("subdev is NULL(%d)", index);
		ret = -EINVAL;
		return ret;
	}

	if (unlikely(!test_bit(IS_SUBDEV_OPEN, &subdev->state))) {
		warn("subdev was closed..(%d)", index);
		ret = -EINVAL;
		return ret;
	}

	FIMC_BUG(!subdev->vctx);

	framemgr = GET_SUBDEV_FRAMEMGR(subdev);

	FIMC_BUG(index >= framemgr->num_frames);

	frame = &framemgr->frames[index];
	framemgr_e_barrier_irq(framemgr, FMGR_IDX_18);

	if (frame->state == FS_COMPLETE) {
		trans_frame(framemgr, frame, FS_FREE);
	} else {
		device = GET_DEVICE(subdev->vctx);
		merr("frame is empty from complete", device);
		merr("frame(%d) is not com state(%d)", device,
					index, frame->state);
		frame_manager_print_queues(framemgr);
		ret = -EINVAL;
	}

	framemgr_x_barrier_irq(framemgr, FMGR_IDX_18);

	return ret;
}
KUNIT_EXPORT_SYMBOL(is_subdev_buffer_finish);

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
	struct is_video_ctx *ivc = container_of(iq, struct is_video_ctx, queue);
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
	struct is_video_ctx *ivc = container_of(iq, struct is_video_ctx, queue);
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
	clear_bit(IS_SUBDEV_VOTF_USE, &is->state);

	return 0;
}

static int is_sensor_subdev_s_fmt(void *device, struct is_queue *iq)
{
	struct is_device_sensor *ids = (struct is_device_sensor *)device;
	struct is_video_ctx *ivc = container_of(iq, struct is_video_ctx, queue);
	struct is_subdev *is = ivc->subdev;
	int ret;

	if (!is) {
		merr("subdev is NULL", ids);
		return -EINVAL;
	}

	if (test_bit(IS_SUBDEV_INTERNAL_USE, &is->state)) {
		mswarn("%s: It is sharing with internal use.", is, is, __func__);

		is->bits_per_pixel = iq->framecfg.format->bitsperpixel[0];
	} else {
		ret = is_subdev_s_fmt(is, iq);
		if (ret) {
			merr("is_subdev_s_format is fail(%d)", ids, ret);
			return ret;
		}
	}

	return 0;
}

static int is_sensor_subdev_reqbufs(void *device, struct is_queue *iq, u32 count)
{
	struct is_device_sensor *ids = (struct is_device_sensor *)device;
	struct is_video_ctx *ivc = container_of(iq, struct is_video_ctx, queue);
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
	struct is_video_ctx *ivc = container_of(iq, struct is_video_ctx, queue);
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
	struct is_video_ctx *ivc = container_of(iq, struct is_video_ctx, queue);
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
	clear_bit(IS_SUBDEV_VOTF_USE, &is->state);

	return 0;
}

static int is_ischain_subdev_s_fmt(void *device, struct is_queue *iq)
{
	int ret;
	struct is_device_ischain *idi = (struct is_device_ischain *)device;
	struct is_video_ctx *ivc = container_of(iq, struct is_video_ctx, queue);
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

static int is_subdev_internal_alloc_buffer(struct is_subdev *subdev)
{
	int ret;
	int i, j;
	unsigned int flags = 0;
	u32 width, height;
	u32 out_buf_size; /* single buffer */
	u32 total_size; /* multi-buffer for FRO */
	u32 total_alloc_size = 0;
	u32 num_planes = 1;
	struct is_mem *mem;
	struct is_frame *frame;
	u32 batch_num;
	u32 payload_size, header_size;
	u32 sbwc_block_width = 0, sbwc_block_height = 0;
	u32 sbwc_height;
	struct is_core *core;
#if defined(USE_CAMERA_HEAP)
	char heapname[25] = {0};
#endif

	FIMC_BUG(!subdev);

	if (subdev->buffer_num > SUBDEV_INTERNAL_BUF_MAX || subdev->buffer_num <= 0) {
		mserr("invalid internal buffer num size(%d)",
			subdev, subdev, subdev->buffer_num);
		return -EINVAL;
	}

	core = is_get_is_core();
	mem = is_hw_get_iommu_mem(subdev->vid);

	ret = is_subdev_internal_get_buffer_size(subdev, &width, &height,
						&sbwc_block_width, &sbwc_block_height);
	if (ret) {
		mserr("is_subdev_internal_get_buffer_size is fail(%d)", subdev, subdev, ret);
		return -EINVAL;
	}

	sbwc_height = sbwc_block_height ? DIV_ROUND_UP(height, sbwc_block_height) : 0;

	switch (subdev->sbwc_type) {
	case COMP:
		payload_size = is_hw_dma_get_payload_stride(
			COMP, subdev->bits_per_pixel, width,
			0, 0, sbwc_block_width, sbwc_block_height) *
			sbwc_height;
		header_size = is_hw_dma_get_header_stride(width, sbwc_block_width, 32) *
			sbwc_height;
		out_buf_size = payload_size + header_size;
		break;
	case COMP_LOSS:
		payload_size = is_hw_dma_get_payload_stride(
			COMP_LOSS, subdev->bits_per_pixel, width,
			0, subdev->lossy_byte32num, sbwc_block_width, sbwc_block_height) *
			sbwc_height;
		out_buf_size = payload_size;
		break;
	default:
		out_buf_size =
			ALIGN(DIV_ROUND_UP(width * subdev->memory_bitwidth, BITS_PER_BYTE), 32) * height;
		break;
	}

	if (out_buf_size <= 0) {
		mserr("wrong internal subdev buffer size(%d)",
					subdev, subdev, out_buf_size);
		return -EINVAL;
	}

	batch_num = subdev->batch_num;
	total_size = out_buf_size * batch_num * num_planes;

	ret = frame_manager_open(&subdev->internal_framemgr, subdev->buffer_num);
	if (ret) {
		mserr("is_frame_open is fail(%d)", subdev, subdev, ret);
		ret = -EINVAL;
		goto err_open_framemgr;
	}

#if defined(USE_CAMERA_HEAP)
	if (core->scenario != IS_SCENARIO_SECURE && subdev->instance == 0)
		strncpy(heapname, CAMERA_HEAP_NAME, CAMERA_HEAP_NAME_LEN);
#endif

	for (i = 0; i < subdev->buffer_num; i++) {
#if defined(USE_CAMERA_HEAP)
		subdev->pb_subdev[i] =
			CALL_PTR_MEMOP(mem, alloc, mem->priv, total_size, heapname, flags);
#else
		subdev->pb_subdev[i] =
			CALL_PTR_MEMOP(mem, alloc, mem->priv, total_size, NULL, flags);
#endif
		if (IS_ERR_OR_NULL(subdev->pb_subdev[i])) {
			mserr("failed to allocate buffer for internal subdev", subdev, subdev);
			subdev->pb_subdev[i] = NULL;
			ret = -ENOMEM;
			goto err_allocate_pb_subdev;
		}

		total_alloc_size += total_size;
	}

	for (i = 0; i < subdev->buffer_num; i++) {
		frame = &subdev->internal_framemgr.frames[i];
		frame->subdev = subdev;
		frame->pb_output = subdev->pb_subdev[i];

		/* TODO : support multi-plane */
		frame->planes = 1;
		frame->num_buffers = batch_num;

		frame->dvaddr_buffer[0] =
			CALL_BUFOP(subdev->pb_subdev[i], dvaddr, subdev->pb_subdev[i]);
		frame->kvaddr_buffer[0] =
			CALL_BUFOP(subdev->pb_subdev[i], kvaddr, subdev->pb_subdev[i]);

		frame->size[0] = out_buf_size;
		set_bit(FRAME_MEM_INIT, &frame->mem_state);

		for (j = 1; j < batch_num * num_planes; j++) {
			frame->dvaddr_buffer[j] =
				frame->dvaddr_buffer[j - 1] + out_buf_size;
			frame->kvaddr_buffer[j] =
				frame->kvaddr_buffer[j - 1] + out_buf_size;
		}
	}

	msinfo(" %s (size: %dx%d, bpp: %d, total: %d, buffer_num: %d, batch_num: %d)",
		subdev, subdev, __func__,
		width, height, subdev->bits_per_pixel,
		total_alloc_size, subdev->buffer_num, batch_num);

	return 0;

err_allocate_pb_subdev:
	while (i-- > 0) {
		if (subdev->pb_subdev[i])
			CALL_VOID_BUFOP(subdev->pb_subdev[i], free, subdev->pb_subdev[i]);
	}

	frame_manager_close(&subdev->internal_framemgr);
err_open_framemgr:
	return ret;
};

static int is_subdev_internal_free_buffer(struct is_subdev *subdev)
{
	int i;

	FIMC_BUG(!subdev);

	if (subdev->internal_framemgr.num_frames == 0) {
		mswarn(" already free internal buffer", subdev, subdev);
		return -EINVAL;
	}

	frame_manager_close(&subdev->internal_framemgr);

	for (i = 0; i < subdev->buffer_num; i++) {
		if (subdev->pb_subdev[i])
			CALL_VOID_BUFOP(subdev->pb_subdev[i], free, subdev->pb_subdev[i]);
	}

	msinfo(" %s.\n", subdev, subdev, __func__);

	return 0;
};

static int _is_subdev_internal_start(struct is_subdev *subdev)
{
	int ret = 0;
	int j;
	struct is_framemgr *framemgr;
	struct is_frame *frame;
	unsigned long flags;

	if (test_bit(IS_SUBDEV_START, &subdev->state)) {
		mswarn(" subdev already start", subdev, subdev);
		goto p_err;
	}

	/* qbuf a setting num of buffers before stream on */
	framemgr = GET_SUBDEV_I_FRAMEMGR(subdev);
	if (unlikely(!framemgr)) {
		mserr(" subdev's framemgr is null", subdev, subdev);
		ret = -EINVAL;
		goto p_err;
	}

	for (j = 0; j < framemgr->num_frames; j++) {
		frame = &framemgr->frames[j];

		/* 1. check frame validation */
		if (unlikely(!test_bit(FRAME_MEM_INIT, &frame->mem_state))) {
			mserr("frame %d is NOT init", subdev, subdev, j);
			ret = -EINVAL;
			goto p_err;
		}

		/* 2. update frame manager */
		framemgr_e_barrier_irqs(framemgr, FMGR_IDX_17, flags);

		if (frame->state != FS_FREE) {
			mserr("frame %d is invalid state(%d)\n",
				subdev, subdev, j, frame->state);
			frame_manager_print_queues(framemgr);
			ret = -EINVAL;
			goto p_err;
		}

		framemgr_x_barrier_irqr(framemgr, FMGR_IDX_17, flags);
	}

	set_bit(IS_SUBDEV_START, &subdev->state);

p_err:

	return ret;
}

static int _is_subdev_internal_stop(struct is_subdev *subdev)
{
	int ret = 0;
	struct is_framemgr *framemgr;
	enum is_frame_state state;

	if (!test_bit(IS_SUBDEV_START, &subdev->state)) {
		mserr("already stopped", subdev, subdev);
		ret = -EINVAL;
		goto p_err;
	}

	framemgr = GET_SUBDEV_I_FRAMEMGR(subdev);
	if (unlikely(!framemgr)) {
		mserr(" subdev's framemgr is null", subdev, subdev);
		ret = -EINVAL;
		goto p_err;
	}

	for (state = FS_REQUEST; state <= FS_COMPLETE; state++) {
		unsigned int retry = 10;
		unsigned int q_count = framemgr->queued_count[state];

		while (--retry && q_count) {
			mswarn("%s(%d) waiting...", subdev, subdev,
				frame_state_name[state], q_count);

			usleep_range(5000, 5100);
			q_count = framemgr->queued_count[state];
		}

		if (!retry)
			mswarn("waiting(until frame empty) is fail", subdev, subdev);
	}

	clear_bit(IS_SUBDEV_START, &subdev->state);
	clear_bit(IS_SUBDEV_VOTF_USE, &subdev->state);

p_err:

	return ret;
}

int is_subdev_internal_start(struct is_subdev *subdev)
{
	int ret = 0;

	if (!test_bit(IS_SUBDEV_INTERNAL_USE, &subdev->state)) {
		mserr("subdev is not in INTERNAL_USE state.", subdev, subdev);
		return -EINVAL;
	}

	if (!test_bit(IS_SUBDEV_INTERNAL_S_FMT, &subdev->state)) {
		mserr("subdev is not in s_fmt state.", subdev, subdev);
		return -EINVAL;
	}

	if (subdev->internal_framemgr.num_frames > 0) {
		mswarn("%s already internal buffer alloced, re-alloc after free",
			subdev, subdev, __func__);

		ret = is_subdev_internal_free_buffer(subdev);
		if (ret) {
			mserr("subdev internal free buffer is fail", subdev, subdev);
			ret = -EINVAL;
			goto p_err;
		}
	}

	ret = is_subdev_internal_alloc_buffer(subdev);
	if (ret) {
		mserr("ischain internal alloc buffer fail(%d)", subdev, subdev, ret);
		goto p_err;
	}

	ret = _is_subdev_internal_start(subdev);
	if (ret) {
		mserr("_is_subdev_internal_start fail(%d)", subdev, subdev, ret);
		goto p_err;
	}

p_err:
	msinfo(" %s(%s)(%d)\n", subdev, subdev, __func__, subdev->data_type, ret);
	return ret;
};
KUNIT_EXPORT_SYMBOL(is_subdev_internal_start);

int is_subdev_internal_stop(struct is_subdev *subdev)
{
	int ret = 0;

	if (!test_bit(IS_SUBDEV_INTERNAL_USE, &subdev->state)) {
		mserr("subdev is not in INTERNAL_USE state.", subdev, subdev);
		return -EINVAL;
	}

	ret = _is_subdev_internal_stop(subdev);
	if (ret) {
		mserr("_is_subdev_internal_stop fail(%d)", subdev, subdev, ret);
		goto p_err;
	}

	ret = is_subdev_internal_free_buffer(subdev);
	if (ret) {
		mserr("subdev internal free buffer is fail(%d)", subdev, subdev, ret);
		ret = -EINVAL;
		goto p_err;
	}

p_err:
	msinfo(" %s(%s)(%d)\n", subdev, subdev, __func__, subdev->data_type, ret);
	return ret;
};
KUNIT_EXPORT_SYMBOL(is_subdev_internal_stop);

int is_subdev_internal_s_format(struct is_subdev *subdev,
	u32 width, u32 height, u32 bits_per_pixel, u32 sbwc_type, u32 lossy_byte32num,
	u32 buffer_num, const char *type_name)
{
	int ret = 0;
	struct is_device_ischain *ischain;
	struct is_device_sensor *sensor;
	struct is_sensor_cfg *sensor_cfg;
	u32 batch_num = 1;

	if (!test_bit(IS_SUBDEV_INTERNAL_USE, &subdev->state)) {
		mserr("subdev is not in INTERNAL_USE state.", subdev, subdev);
		return -EINVAL;
	}

	if (!test_bit(IS_SUBDEV_OPEN, &subdev->state)) {
		mserr("subdev is not in OPEN state.", subdev, subdev);
		return -EINVAL;
	}

	if (width == 0 || height == 0) {
		mserr("wrong internal vc size(%d x %d)", subdev, subdev, width, height);
		return -EINVAL;
	}

	ischain = is_get_ischain_device(subdev->instance);
	if (subdev->id == ENTRY_PAF && ischain
	    && !test_bit(IS_ISCHAIN_REPROCESSING, &ischain->state)) {

		sensor = ischain->sensor;
		if (!sensor) {
			mserr("failed to get sensor", subdev, subdev);
			return -EINVAL;
		}

		sensor_cfg = sensor->cfg;
		if (!sensor_cfg) {
			mserr("failed to get senso_cfgr", subdev, subdev);
			return -EINVAL;
		}

		if (sensor_cfg->ex_mode == EX_DUALFPS_960)
			batch_num = 16;
		else if (sensor_cfg->ex_mode == EX_DUALFPS_480)
			batch_num = 8;
	}

	subdev->output.width = width;
	subdev->output.height = height;
	subdev->output.crop.x = 0;
	subdev->output.crop.y = 0;
	subdev->output.crop.w = subdev->output.width;
	subdev->output.crop.h = subdev->output.height;

	if (!subdev->bits_per_pixel)
		subdev->bits_per_pixel = bits_per_pixel;
	subdev->memory_bitwidth = bits_per_pixel;
	subdev->sbwc_type = sbwc_type;
	subdev->lossy_byte32num = lossy_byte32num;

	subdev->buffer_num = buffer_num;
	subdev->batch_num = batch_num;

	snprintf(subdev->data_type, sizeof(subdev->data_type), "%s", type_name);

	set_bit(IS_SUBDEV_INTERNAL_S_FMT, &subdev->state);

	return ret;
};

int is_subdev_internal_g_bpp(struct is_subdev *subdev,
	struct is_sensor_cfg *sensor_cfg)
{
	switch (subdev->id) {
	case ENTRY_YPP:
		return 10;
	case ENTRY_MCS:
		return 10;
	case ENTRY_PAF:
		if (!sensor_cfg)
			return BITS_PER_BYTE *2;

		return csi_hw_g_bpp(sensor_cfg->output[CSI_VIRTUAL_CH_0].hwformat);
	default:
		return BITS_PER_BYTE * 2; /* 2byte = 16bits */
	}
}
KUNIT_EXPORT_SYMBOL(is_subdev_internal_g_bpp);

int is_subdev_internal_open(u32 instance, int vid,
				struct is_subdev *subdev)
{
	int ret = 0;

	if (test_bit(IS_SUBDEV_INTERNAL_USE, &subdev->state)) {
		mswarn("already INTERNAL_USE state", subdev, subdev);
		goto p_err;
	}

	if (!test_bit(IS_SUBDEV_OPEN, &subdev->state)) {
		ret = is_subdev_open(subdev, NULL, instance);
		if (ret) {
			mserr("is_subdev_open is fail(%d)", subdev, subdev, ret);
			goto p_err;
		}

		subdev->vid = vid;
	}

	set_bit(IS_SUBDEV_INTERNAL_USE, &subdev->state);

	msinfo("[V:%d] %s\n", subdev, subdev, vid, __func__);

p_err:
	return ret;
};

int is_subdev_internal_close(struct is_subdev *subdev)
{
	int ret = 0;

	if (!test_bit(IS_SUBDEV_INTERNAL_USE, &subdev->state)) {
		mserr("subdev is not in INTERNAL_USE state.", subdev, subdev);
		return -EINVAL;
	}

	if (!test_bit(IS_SUBDEV_EXTERNAL_USE, &subdev->state)) {
		ret = is_subdev_close(subdev);
		if (ret)
			mserr("is_subdev_close is fail(%d)", subdev, subdev, ret);
	}

	clear_bit(IS_SUBDEV_INTERNAL_USE, &subdev->state);

	return ret;
};
