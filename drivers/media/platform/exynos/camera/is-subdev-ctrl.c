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
#include "is-debug.h"
#if defined(SDC_HEADER_GEN)
#include "is-sdc-header.h"
#endif

struct is_subdev * video2subdev(enum is_subdev_device_type device_type,
	void *device, u32 vid)
{
	struct is_subdev *subdev = NULL;
	struct is_device_sensor *sensor = NULL;
	struct is_device_ischain *ischain = NULL;

	if (device_type == IS_SENSOR_SUBDEV) {
		sensor = (struct is_device_sensor *)device;
	} else {
		ischain = (struct is_device_ischain *)device;
		sensor = ischain->sensor;
	}

	switch (vid) {
	case IS_VIDEO_SS0VC0_NUM:
	case IS_VIDEO_SS1VC0_NUM:
	case IS_VIDEO_SS2VC0_NUM:
	case IS_VIDEO_SS3VC0_NUM:
	case IS_VIDEO_SS4VC0_NUM:
	case IS_VIDEO_SS5VC0_NUM:
		subdev = &sensor->ssvc0;
		break;
	case IS_VIDEO_SS0VC1_NUM:
	case IS_VIDEO_SS1VC1_NUM:
	case IS_VIDEO_SS2VC1_NUM:
	case IS_VIDEO_SS3VC1_NUM:
	case IS_VIDEO_SS4VC1_NUM:
	case IS_VIDEO_SS5VC1_NUM:
		subdev = &sensor->ssvc1;
		break;
	case IS_VIDEO_SS0VC2_NUM:
	case IS_VIDEO_SS1VC2_NUM:
	case IS_VIDEO_SS2VC2_NUM:
	case IS_VIDEO_SS3VC2_NUM:
	case IS_VIDEO_SS4VC2_NUM:
	case IS_VIDEO_SS5VC2_NUM:
		subdev = &sensor->ssvc2;
		break;
	case IS_VIDEO_SS0VC3_NUM:
	case IS_VIDEO_SS1VC3_NUM:
	case IS_VIDEO_SS2VC3_NUM:
	case IS_VIDEO_SS3VC3_NUM:
	case IS_VIDEO_SS4VC3_NUM:
	case IS_VIDEO_SS5VC3_NUM:
		subdev = &sensor->ssvc3;
		break;
	case IS_VIDEO_30S_NUM:
	case IS_VIDEO_31S_NUM:
	case IS_VIDEO_32S_NUM:
		subdev = &ischain->group_3aa.leader;
		break;
	case IS_VIDEO_30C_NUM:
	case IS_VIDEO_31C_NUM:
	case IS_VIDEO_32C_NUM:
		subdev = &ischain->txc;
		break;
	case IS_VIDEO_30P_NUM:
	case IS_VIDEO_31P_NUM:
	case IS_VIDEO_32P_NUM:
		subdev = &ischain->txp;
		break;
	case IS_VIDEO_30F_NUM:
	case IS_VIDEO_31F_NUM:
	case IS_VIDEO_32F_NUM:
		subdev = &ischain->txf;
		break;
	case IS_VIDEO_30G_NUM:
	case IS_VIDEO_31G_NUM:
	case IS_VIDEO_32G_NUM:
		subdev = &ischain->txg;
		break;
	case IS_VIDEO_ORB0C_NUM:
	case IS_VIDEO_ORB1C_NUM:
		subdev = &ischain->orbxc;
		break;
	case IS_VIDEO_I0S_NUM:
	case IS_VIDEO_I1S_NUM:
		subdev = &ischain->group_isp.leader;
		break;
	case IS_VIDEO_I0C_NUM:
	case IS_VIDEO_I1C_NUM:
		subdev = &ischain->ixc;
		break;
	case IS_VIDEO_I0P_NUM:
	case IS_VIDEO_I1P_NUM:
		subdev = &ischain->ixp;
		break;
	case IS_VIDEO_I0T_NUM:
		subdev = &ischain->ixt;
		break;
	case IS_VIDEO_I0G_NUM:
		subdev = &ischain->ixg;
		break;
	case IS_VIDEO_I0V_NUM:
		subdev = &ischain->ixv;
		break;
	case IS_VIDEO_I0W_NUM:
		subdev = &ischain->ixw;
		break;
	case IS_VIDEO_ME0C_NUM:
	case IS_VIDEO_ME1C_NUM:
		subdev = &ischain->mexc;
		break;
	case IS_VIDEO_D0S_NUM:
	case IS_VIDEO_D1S_NUM:
		subdev = &ischain->group_dis.leader;
		break;
	case IS_VIDEO_D0C_NUM:
	case IS_VIDEO_D1C_NUM:
		subdev = &ischain->dxc;
		break;
	case IS_VIDEO_DCP0S_NUM:
		subdev = &ischain->group_dcp.leader;
		break;
	case IS_VIDEO_DCP1S_NUM:
		subdev = &ischain->dc1s;
		break;
	case IS_VIDEO_DCP0C_NUM:
		subdev = &ischain->dc0c;
		break;
	case IS_VIDEO_DCP1C_NUM:
		subdev = &ischain->dc1c;
		break;
	case IS_VIDEO_DCP2C_NUM:
		subdev = &ischain->dc2c;
		break;
	case IS_VIDEO_DCP3C_NUM:
		subdev = &ischain->dc3c;
		break;
	case IS_VIDEO_DCP4C_NUM:
		subdev = &ischain->dc4c;
		break;
	case IS_VIDEO_M0S_NUM:
	case IS_VIDEO_M1S_NUM:
		subdev = &ischain->group_mcs.leader;
		break;
	case IS_VIDEO_M0P_NUM:
		subdev = &ischain->m0p;
		break;
	case IS_VIDEO_M1P_NUM:
		subdev = &ischain->m1p;
		break;
	case IS_VIDEO_M2P_NUM:
		subdev = &ischain->m2p;
		break;
	case IS_VIDEO_M3P_NUM:
		subdev = &ischain->m3p;
		break;
	case IS_VIDEO_M4P_NUM:
		subdev = &ischain->m4p;
		break;
	case IS_VIDEO_M5P_NUM:
		subdev = &ischain->m5p;
		break;
	case IS_VIDEO_VRA_NUM:
		subdev = &ischain->group_vra.leader;
		break;
	case IS_VIDEO_CLH0S_NUM:
		subdev = &ischain->group_clh.leader;
		break;
	case IS_VIDEO_CLH0C_NUM:
		subdev = &ischain->clhc;
		break;
	default:
		err("[%d] vid %d is NOT found", ((device_type == IS_SENSOR_SUBDEV) ?
				 (ischain ? ischain->instance : 0) : (sensor ? sensor->device_id : 0)), vid);
		break;
	}

	return subdev;
}

int is_subdev_probe(struct is_subdev *subdev,
	u32 instance,
	u32 id,
	char *name,
	const struct is_subdev_ops *sops)
{
	FIMC_BUG(!subdev);
	FIMC_BUG(!name);

	subdev->id = id;
	subdev->instance = instance;
	subdev->ops = sops;
	memset(subdev->name, 0x0, sizeof(subdev->name));
	strncpy(subdev->name, name, sizeof(char[3]));
	clear_bit(IS_SUBDEV_OPEN, &subdev->state);
	clear_bit(IS_SUBDEV_RUN, &subdev->state);
	clear_bit(IS_SUBDEV_START, &subdev->state);
	clear_bit(IS_SUBDEV_VOTF_USE, &subdev->state);

	/* for internal use */
	clear_bit(IS_SUBDEV_INTERNAL_S_FMT, &subdev->state);
	frame_manager_probe(&subdev->internal_framemgr, BIT(subdev->id), name);

	return 0;
}

int is_subdev_open(struct is_subdev *subdev,
	struct is_video_ctx *vctx,
	void *ctl_data)
{
	int ret = 0;
	struct is_video *video;
	const struct param_control *init_ctl = (const struct param_control *)ctl_data;

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

	if (init_ctl) {
		set_bit(IS_SUBDEV_START, &subdev->state);

		if (subdev->id == ENTRY_VRA) {
			/* vra only control by command for enabling or disabling */
			if (init_ctl->cmd == CONTROL_COMMAND_STOP)
				clear_bit(IS_SUBDEV_RUN, &subdev->state);
			else
				set_bit(IS_SUBDEV_RUN, &subdev->state);
		} else {
			if (init_ctl->bypass == CONTROL_BYPASS_ENABLE)
				clear_bit(IS_SUBDEV_RUN, &subdev->state);
			else
				set_bit(IS_SUBDEV_RUN, &subdev->state);
		}
	} else {
		clear_bit(IS_SUBDEV_START, &subdev->state);
		clear_bit(IS_SUBDEV_RUN, &subdev->state);
	}

	set_bit(IS_SUBDEV_OPEN, &subdev->state);

p_err:
	return ret;
}

/*
 * DMA abstraction:
 * A overlapped use case of DMA should be detected.
 */
static int is_sensor_check_subdev_open(struct is_device_sensor *device,
	struct is_subdev *subdev,
	struct is_video_ctx *vctx)
{
	int i;
	struct is_core *core;
	struct is_device_sensor *all_sensor;
	struct is_device_sensor *each_sensor;

	FIMC_BUG(!device);
	FIMC_BUG(!subdev);

	core = device->private_data;
	all_sensor = is_get_sensor_device(core);
	for (i = 0; i < IS_SENSOR_COUNT; i++) {
		each_sensor = &all_sensor[i];
		if (each_sensor == device)
			continue;

		if (each_sensor->dma_abstract == false)
			continue;

		if (test_bit(IS_SENSOR_OPEN, &each_sensor->state)) {
			if (test_bit(IS_SUBDEV_OPEN, &each_sensor->ssvc0.state)) {
				if (each_sensor->ssvc0.dma_ch[0] == subdev->dma_ch[0]) {
					merr("vc0 dma(%d) is overlapped with another sensor(I:%d).\n",
						device, subdev->dma_ch[0], i);
					goto err_check_vc_open;
				}
			}

			if (test_bit(IS_SUBDEV_OPEN, &each_sensor->ssvc1.state)) {
				if (each_sensor->ssvc1.dma_ch[0] == subdev->dma_ch[0]) {
					merr("vc1 dma(%d) is overlapped with another sensor(I:%d).\n",
						device, subdev->dma_ch[0], i);
					goto err_check_vc_open;
				}
			}

			if (test_bit(IS_SUBDEV_OPEN, &each_sensor->ssvc2.state)) {
				if (each_sensor->ssvc2.dma_ch[0] == subdev->dma_ch[0]) {
					merr("vc2 dma(%d) is overlapped with another sensor(I:%d).\n",
						device, subdev->dma_ch[0], i);
					goto err_check_vc_open;
				}
			}

			if (test_bit(IS_SUBDEV_OPEN, &each_sensor->ssvc3.state)) {
				if (each_sensor->ssvc3.dma_ch[0] == subdev->dma_ch[0]) {
					merr("vc3 dma(%d) is overlapped with another sensor(I:%d).\n",
						device, subdev->dma_ch[0], i);
					goto err_check_vc_open;
				}
			}
		}
	}

	is_put_sensor_device(core);

	return 0;

err_check_vc_open:
	is_put_sensor_device(core);
	return -EBUSY;
}
int is_sensor_subdev_open(struct is_device_sensor *device,
	struct is_video_ctx *vctx)
{
	int ret = 0;
	struct is_subdev *subdev;

	FIMC_BUG(!device);
	FIMC_BUG(!vctx);
	FIMC_BUG(!GET_VIDEO(vctx));

	subdev = video2subdev(IS_SENSOR_SUBDEV, (void *)device, GET_VIDEO(vctx)->id);
	if (!subdev) {
		merr("video2subdev is fail", device);
		ret = -EINVAL;
		goto err_video2subdev;
	}

	ret = is_sensor_check_subdev_open(device, subdev, vctx);
	if (ret) {
		mserr("is_sensor_check_subdev_open is fail", subdev, subdev);
		ret = -EINVAL;
		goto err_check_subdev_open;
	}

	ret = is_subdev_open(subdev, vctx, NULL);
	if (ret) {
		mserr("is_subdev_open is fail(%d)", subdev, subdev, ret);
		goto err_subdev_open;
	}

	vctx->subdev = subdev;

	return 0;

err_subdev_open:
err_check_subdev_open:
err_video2subdev:
	return ret;
}

int is_ischain_subdev_open(struct is_device_ischain *device,
	struct is_video_ctx *vctx)
{
	int ret = 0;
	int ret_err = 0;
	struct is_subdev *subdev;

	FIMC_BUG(!device);
	FIMC_BUG(!vctx);
	FIMC_BUG(!GET_VIDEO(vctx));

	subdev = video2subdev(IS_ISCHAIN_SUBDEV, (void *)device, GET_VIDEO(vctx)->id);
	if (!subdev) {
		merr("video2subdev is fail", device);
		ret = -EINVAL;
		goto err_video2subdev;
	}

	vctx->subdev = subdev;

	ret = is_subdev_open(subdev, vctx, NULL);
	if (ret) {
		merr("is_subdev_open is fail(%d)", device, ret);
		goto err_subdev_open;
	}

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
err_subdev_open:
err_video2subdev:
	return ret;
}

int is_subdev_close(struct is_subdev *subdev)
{
	int ret = 0;

	if (!test_bit(IS_SUBDEV_OPEN, &subdev->state)) {
		mserr("subdev is already close", subdev, subdev);
		ret = -EINVAL;
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
	return 0;
}

int is_sensor_subdev_close(struct is_device_sensor *device,
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

	if (test_bit(IS_SENSOR_FRONT_START, &device->state)) {
		merr("sudden close, call sensor_front_stop()\n", device);

		ret = is_sensor_front_stop(device);
		if (ret)
			merr("is_sensor_front_stop is fail(%d)", device, ret);
	}

	ret = is_subdev_close(subdev);
	if (ret)
		merr("is_subdev_close is fail(%d)", device, ret);

p_err:
	return ret;
}

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
	if (ret) {
		merr("is_subdev_close is fail(%d)", device, ret);
		goto p_err;
	}

	ret = is_ischain_close_wrap(device);
	if (ret) {
		merr("is_ischain_close_wrap is fail(%d)", device, ret);
		goto p_err;
	}

p_err:
	return ret;
}

static int is_subdev_start(struct is_subdev *subdev)
{
	int ret = 0;
	struct is_subdev *leader;

	FIMC_BUG(!subdev);
	FIMC_BUG(!subdev->leader);

	leader = subdev->leader;

	if (test_bit(IS_SUBDEV_START, &subdev->state)) {
		mserr("already start", subdev, subdev);
		goto p_err;
	}

	if (test_bit(IS_SUBDEV_START, &leader->state)) {
		mserr("leader%d is ALREADY started", subdev, subdev, leader->id);
		goto p_err;
	}

	set_bit(IS_SUBDEV_START, &subdev->state);

p_err:
	return ret;
}

static int is_sensor_subdev_start(void *qdevice,
	struct is_queue *queue)
{
	int ret = 0;
	struct is_device_sensor *device = qdevice;
	struct is_video_ctx *vctx;
	struct is_subdev *subdev;

	FIMC_BUG(!device);
	FIMC_BUG(!queue);

	vctx = container_of(queue, struct is_video_ctx, queue);
	subdev = vctx->subdev;
	if (!subdev) {
		merr("subdev is NULL", device);
		ret = -EINVAL;
		goto p_err;
	}

	if (!test_bit(IS_SENSOR_S_INPUT, &device->state)) {
		mserr("device is not yet init", device, subdev);
		ret = -EINVAL;
		goto p_err;
	}

	if (test_bit(IS_SUBDEV_START, &subdev->state)) {
		mserr("already start", subdev, subdev);
		goto p_err;
	}

	set_bit(IS_SUBDEV_START, &subdev->state);

p_err:
	return ret;
}

static int is_ischain_subdev_start(void *qdevice,
	struct is_queue *queue)
{
	int ret = 0;
	struct is_device_ischain *device = qdevice;
	struct is_video_ctx *vctx;
	struct is_subdev *subdev;

	FIMC_BUG(!device);
	FIMC_BUG(!queue);

	vctx = container_of(queue, struct is_video_ctx, queue);
	subdev = vctx->subdev;
	if (!subdev) {
		merr("subdev is NULL", device);
		ret = -EINVAL;
		goto p_err;
	}

	if (!test_bit(IS_ISCHAIN_INIT, &device->state)) {
		mserr("device is not yet init", device, subdev);
		ret = -EINVAL;
		goto p_err;
	}

	ret = is_subdev_start(subdev);
	if (ret) {
		mserr("is_subdev_start is fail(%d)", device, subdev, ret);
		goto p_err;
	}

p_err:
	return ret;
}

static int is_subdev_stop(struct is_subdev *subdev)
{
	int ret = 0;
	unsigned long flags;
	struct is_subdev *leader;
	struct is_framemgr *framemgr;
	struct is_frame *frame;
	struct is_device_ischain *device;

	FIMC_BUG(!subdev);
	FIMC_BUG(!subdev->leader);
	FIMC_BUG(!subdev->vctx);

	leader = subdev->leader;
	framemgr = GET_SUBDEV_FRAMEMGR(subdev);
	device = GET_DEVICE(subdev->vctx);
	FIMC_BUG(!framemgr);

	if (!test_bit(IS_SUBDEV_START, &subdev->state)) {
		merr("already stop", device);
		goto p_err;
	}

	if (test_bit(IS_SUBDEV_START, &leader->state)) {
		merr("leader%d is NOT stopped", device, leader->id);
		goto p_err;
	}

	framemgr_e_barrier_irqs(framemgr, FMGR_IDX_16, flags);

	if (framemgr->queued_count[FS_PROCESS] > 0) {
		framemgr_x_barrier_irqr(framemgr, FMGR_IDX_16, flags);
		merr("being processed, can't stop", device);
		ret = -EINVAL;
		goto p_err;
	}

	frame = peek_frame(framemgr, FS_REQUEST);
	while (frame) {
		CALL_VOPS(subdev->vctx, done, frame->index, VB2_BUF_STATE_ERROR);
		trans_frame(framemgr, frame, FS_COMPLETE);
		frame = peek_frame(framemgr, FS_REQUEST);
	}

	framemgr_x_barrier_irqr(framemgr, FMGR_IDX_16, flags);

	clear_bit(IS_SUBDEV_RUN, &subdev->state);
	clear_bit(IS_SUBDEV_START, &subdev->state);
	clear_bit(IS_SUBDEV_VOTF_USE, &subdev->state);

p_err:
	return ret;
}

static int is_sensor_subdev_stop(void *qdevice,
	struct is_queue *queue)
{
	int ret = 0;
	unsigned long flags;
	struct is_device_sensor *device = qdevice;
	struct is_video_ctx *vctx;
	struct is_subdev *subdev;
	struct is_framemgr *framemgr;
	struct is_frame *frame;

	FIMC_BUG(!device);
	FIMC_BUG(!queue);

	vctx = container_of(queue, struct is_video_ctx, queue);
	subdev = vctx->subdev;
	if (!subdev) {
		merr("subdev is NULL", device);
		ret = -EINVAL;
		goto p_err;
	}

	if (!test_bit(IS_SUBDEV_START, &subdev->state)) {
		merr("already stop", device);
		goto p_err;
	}

	framemgr = GET_SUBDEV_FRAMEMGR(subdev);
	if (!framemgr) {
		merr("framemgr is NULL", device);
		ret = -EINVAL;
		goto p_err;
	}
	framemgr_e_barrier_irqs(framemgr, FMGR_IDX_16, flags);

	frame = peek_frame(framemgr, FS_PROCESS);
	while (frame) {
		CALL_VOPS(subdev->vctx, done, frame->index, VB2_BUF_STATE_ERROR);
		trans_frame(framemgr, frame, FS_COMPLETE);
		frame = peek_frame(framemgr, FS_PROCESS);
	}

	frame = peek_frame(framemgr, FS_REQUEST);
	while (frame) {
		CALL_VOPS(subdev->vctx, done, frame->index, VB2_BUF_STATE_ERROR);
		trans_frame(framemgr, frame, FS_COMPLETE);
		frame = peek_frame(framemgr, FS_REQUEST);
	}

	framemgr_x_barrier_irqr(framemgr, FMGR_IDX_16, flags);

	clear_bit(IS_SUBDEV_RUN, &subdev->state);
	clear_bit(IS_SUBDEV_START, &subdev->state);
	clear_bit(IS_SUBDEV_VOTF_USE, &subdev->state);

p_err:
	return ret;
}

static int is_ischain_subdev_stop(void *qdevice,
	struct is_queue *queue)
{
	int ret = 0;
	struct is_device_ischain *device = qdevice;
	struct is_video_ctx *vctx;
	struct is_subdev *subdev;

	FIMC_BUG(!device);
	FIMC_BUG(!queue);

	vctx = container_of(queue, struct is_video_ctx, queue);
	subdev = vctx->subdev;
	if (!subdev) {
		merr("subdev is NULL", device);
		ret = -EINVAL;
		goto p_err;
	}

	ret = is_subdev_stop(subdev);
	if (ret) {
		merr("is_subdev_stop is fail(%d)", device, ret);
		goto p_err;
	}

p_err:
	return ret;
}

static int is_subdev_s_format(struct is_subdev *subdev,
	struct is_queue *queue)
{
	int ret = 0;
	u32 pixelformat = 0, width, height;

	FIMC_BUG(!subdev);
	FIMC_BUG(!subdev->vctx);
	FIMC_BUG(!queue);
	FIMC_BUG(!queue->framecfg.format);

	pixelformat = queue->framecfg.format->pixelformat;

	width = queue->framecfg.width;
	height = queue->framecfg.height;

	switch (subdev->id) {
	case ENTRY_M0P:
	case ENTRY_M1P:
	case ENTRY_M2P:
	case ENTRY_M3P:
	case ENTRY_M4P:
		if (width % 8) {
			mserr("width(%d) of format(%d) is not multiple of 8: need to check size",
				subdev, subdev, width, pixelformat);
		}
		break;
	default:
		break;
	}

	subdev->output.width = width;
	subdev->output.height = height;

	subdev->output.crop.x = 0;
	subdev->output.crop.y = 0;
	subdev->output.crop.w = subdev->output.width;
	subdev->output.crop.h = subdev->output.height;

	return ret;
}

static int is_sensor_subdev_s_format(void *qdevice,
	struct is_queue *queue)
{
	int ret = 0;
	struct is_device_sensor *device = qdevice;
	struct is_video_ctx *vctx;
	struct is_subdev *subdev;

	FIMC_BUG(!device);
	FIMC_BUG(!queue);

	vctx = container_of(queue, struct is_video_ctx, queue);
	subdev = vctx->subdev;
	if (!subdev) {
		merr("subdev is NULL", device);
		ret = -EINVAL;
		goto p_err;
	}

	if (test_bit(IS_SUBDEV_INTERNAL_USE, &subdev->state)) {
		mswarn("%s: It is sharing with internal use.", subdev, subdev, __func__);
	} else {
		ret = is_subdev_s_format(subdev, queue);
		if (ret) {
			merr("is_subdev_s_format is fail(%d)", device, ret);
			goto p_err;
		}
	}

p_err:
	return ret;
}

static int is_ischain_subdev_s_format(void *qdevice,
	struct is_queue *queue)
{
	int ret = 0;
	struct is_device_ischain *device = qdevice;
	struct is_video_ctx *vctx;
	struct is_subdev *subdev;

	FIMC_BUG(!device);
	FIMC_BUG(!queue);

	vctx = container_of(queue, struct is_video_ctx, queue);
	subdev = vctx->subdev;
	if (!subdev) {
		merr("subdev is NULL", device);
		ret = -EINVAL;
		goto p_err;
	}

	ret = is_subdev_s_format(subdev, queue);
	if (ret) {
		merr("is_subdev_s_format is fail(%d)", device, ret);
		goto p_err;
	}

p_err:
	return ret;
}

int is_sensor_subdev_reqbuf(void *qdevice,
	struct is_queue *queue, u32 count)
{
	int i = 0;
	int ret = 0;
	struct is_device_sensor *device = qdevice;
	struct is_video_ctx *vctx;
	struct is_subdev *subdev;
	struct is_framemgr *framemgr;
	struct is_frame *frame;

	FIMC_BUG(!device);
	FIMC_BUG(!queue);

	if (!count)
		goto p_err;

	vctx = container_of(queue, struct is_video_ctx, queue);
	subdev = vctx->subdev;
	if (!subdev) {
		merr("subdev is NULL", device);
		ret = -EINVAL;
		goto p_err;
	}

	framemgr = GET_SUBDEV_FRAMEMGR(subdev);
	if (!framemgr) {
		merr("framemgr is NULL", device);
		ret = -EINVAL;
		goto p_err;
	}

	for (i = 0; i < count; i++) {
		frame = &framemgr->frames[i];
		frame->subdev = subdev;
	}

p_err:
	return ret;
}

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
		ret = EINVAL;
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

	device = GET_DEVICE(subdev->vctx);
	framemgr = GET_SUBDEV_FRAMEMGR(subdev);
	if (unlikely(!framemgr)) {
		warn("subdev's framemgr is null..(%d)", index);
		ret = -EINVAL;
		return ret;
	}

	FIMC_BUG(index >= framemgr->num_frames);

	frame = &framemgr->frames[index];
	framemgr_e_barrier_irq(framemgr, FMGR_IDX_18);

	if (frame->state == FS_COMPLETE) {
		trans_frame(framemgr, frame, FS_FREE);
	} else {
		merr("frame is empty from complete", device);
		merr("frame(%d) is not com state(%d)", device,
					index, frame->state);
		frame_manager_print_queues(framemgr);
		ret = -EINVAL;
	}

	framemgr_x_barrier_irq(framemgr, FMGR_IDX_18);

	return ret;
}

const struct is_queue_ops is_sensor_subdev_ops = {
	.start_streaming	= is_sensor_subdev_start,
	.stop_streaming		= is_sensor_subdev_stop,
	.s_format		= is_sensor_subdev_s_format,
	.request_bufs		= is_sensor_subdev_reqbuf,
};

const struct is_queue_ops is_ischain_subdev_ops = {
	.start_streaming	= is_ischain_subdev_start,
	.stop_streaming		= is_ischain_subdev_stop,
	.s_format		= is_ischain_subdev_s_format
};

void is_subdev_dnr_start(struct is_device_ischain *device,
	struct is_frame *frame, u32 *lindex, u32 *hindex, u32 *indexes)
{
	/* this function is for dnr start */
}

void is_subdev_dnr_stop(struct is_device_ischain *device,
	struct is_frame *frame, u32 *lindex, u32 *hindex, u32 *indexes)
{
	/* this function is for dnr stop */
}

void is_subdev_dnr_bypass(struct is_device_ischain *device,
	struct is_frame *frame, u32 *lindex, u32 *hindex, u32 *indexes, bool bypass)
{
	struct param_tpu_config *config_param;

	FIMC_BUG_VOID(!device);
	FIMC_BUG_VOID(!lindex);
	FIMC_BUG_VOID(!hindex);
	FIMC_BUG_VOID(!indexes);

	config_param = is_itf_g_param(device, frame, PARAM_TPU_CONFIG);
	config_param->tdnr_bypass = bypass ? CONTROL_BYPASS_ENABLE : CONTROL_BYPASS_DISABLE;
	*lindex |= LOWBIT_OF(PARAM_TPU_CONFIG);
	*hindex |= HIGHBIT_OF(PARAM_TPU_CONFIG);
	(*indexes)++;
}

static int is_subdev_internal_alloc_buffer(struct is_subdev *subdev,
	struct is_mem *mem)
{
	int ret;
	int i, j;
	u32 buffer_size; /* single buffer */
	u32 total_size; /* multi-buffer for FRO */
	struct is_frame *frame;
	struct is_device_ischain *device;
	u32 batch_num;

	FIMC_BUG(!subdev);

	device = GET_DEVICE(subdev->vctx);
	if (subdev->buffer_num > SUBDEV_INTERNAL_BUF_MAX || subdev->buffer_num <= 0) {
		merr("invalid internal buffer num size(%d)",
			device, subdev->buffer_num);
		return -EINVAL;
	}

	buffer_size = subdev->output.width * subdev->output.height
				* subdev->bytes_per_pixel;

	if (buffer_size <= 0) {
		merr("wrong internal subdev buffer size(%d)",
					device, buffer_size);
		return -EINVAL;
	}

	batch_num = subdev->batch_num;
	total_size = buffer_size * batch_num;

	for (i = 0; i < subdev->buffer_num; i++) {
		subdev->pb_subdev[i] = CALL_PTR_MEMOP(mem, alloc, mem->default_ctx, total_size, NULL, 0);
		if (IS_ERR_OR_NULL(subdev->pb_subdev[i])) {
			merr("failed to allocate buffer for internal subdev",
							device);
			ret = -ENOMEM;
			goto err_allocate_pb_subdev;
		}
	}

	ret = frame_manager_open(&subdev->internal_framemgr, subdev->buffer_num);
	if (ret) {
		merr("is_frame_open is fail(%d)", device, ret);
		ret = -EINVAL;
		goto err_open_framemgr;
	}

	for (i = 0; i < subdev->buffer_num; i++) {
		frame = &subdev->internal_framemgr.frames[i];
		frame->subdev = subdev;

		/* TODO : support multi-plane */
		frame->planes = 1;
		frame->num_buffers = batch_num;
		frame->dvaddr_buffer[0] = CALL_BUFOP(subdev->pb_subdev[i], dvaddr, subdev->pb_subdev[i]);
		frame->kvaddr_buffer[0] = CALL_BUFOP(subdev->pb_subdev[i], kvaddr, subdev->pb_subdev[i]);
		frame->size[0] = buffer_size;

		set_bit(FRAME_MEM_INIT, &frame->mem_state);

		for (j = 1; j < batch_num; j++) {
			frame->dvaddr_buffer[j] = frame->dvaddr_buffer[j - 1] + buffer_size;
			frame->kvaddr_buffer[j] = frame->kvaddr_buffer[j - 1] + buffer_size;
		}

#if defined(SDC_HEADER_GEN)
		if (subdev->id == ENTRY_PAF) {
			const u32 *header = NULL;
			u32 byte_per_line;
			u32 header_size;
			u32 width = subdev->output.width;

			if (width == SDC_WIDTH_HD)
				header = is_sdc_header_hd;
			else if (width == SDC_WIDTH_FHD)
				header = is_sdc_header_fhd;
			else
				mserr("invalid SDC size: width(%d)", subdev, subdev, width);

			if (header) {
				byte_per_line = ALIGN(width / 2 * 10 / BITS_PER_BYTE, 16);
				header_size = byte_per_line * SDC_HEADER_LINE;

				memcpy((void *)frame->kvaddr_buffer[0], header, header_size);
				__flush_dcache_area((void *)frame->kvaddr_buffer[0], header_size);

				msinfo("Write SDC header: width(%d) size(%d)\n",
					subdev, subdev, width, header_size);
			}
		}
#endif
	}

	msinfo(" %s (size: %d, buffernum: %d, batch_num: %d)",
		subdev, subdev, __func__, buffer_size, subdev->buffer_num, batch_num);

	return 0;

err_open_framemgr:
err_allocate_pb_subdev:
	while (i-- > 0)
		CALL_VOID_BUFOP(subdev->pb_subdev[i], free, subdev->pb_subdev[i]);

	return ret;
};

static int is_subdev_internal_free_buffer(struct is_subdev *subdev)
{
	int ret = 0;
	int i;

	FIMC_BUG(!subdev);

	if (subdev->internal_framemgr.num_frames == 0) {
		mswarn(" already free internal buffer", subdev, subdev);
		return -EINVAL;
	}

	frame_manager_close(&subdev->internal_framemgr);

	for (i = 0; i < subdev->buffer_num; i++)
		CALL_VOID_BUFOP(subdev->pb_subdev[i], free, subdev->pb_subdev[i]);

	msinfo("%s", subdev, subdev, __func__);

	return ret;
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
	struct is_frame *frame;
	unsigned long flags;

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

	framemgr_e_barrier_irqs(framemgr, FMGR_IDX_16, flags);

	frame = peek_frame(framemgr, FS_PROCESS);
	while (frame) {
		trans_frame(framemgr, frame, FS_COMPLETE);
		frame = peek_frame(framemgr, FS_PROCESS);
	}

	frame = peek_frame(framemgr, FS_REQUEST);
	while (frame) {
		trans_frame(framemgr, frame, FS_COMPLETE);
		frame = peek_frame(framemgr, FS_REQUEST);
	}

	frame = peek_frame(framemgr, FS_COMPLETE);
	while (frame) {
		trans_frame(framemgr, frame, FS_FREE);
		frame = peek_frame(framemgr, FS_COMPLETE);
	}

	framemgr_x_barrier_irqr(framemgr, FMGR_IDX_16, flags);

	clear_bit(IS_SUBDEV_START, &subdev->state);
	clear_bit(IS_SUBDEV_VOTF_USE, &subdev->state);

p_err:

	return ret;
}

int is_subdev_internal_start(void *device, enum is_device_type type, struct is_subdev *subdev)
{
	int ret = 0;
	struct is_device_sensor *sensor;
	struct is_device_ischain *ischain;
	struct is_mem *mem = NULL;

	FIMC_BUG(!device);

	if (!test_bit(IS_SUBDEV_INTERNAL_USE, &subdev->state)) {
		mserr("subdev is not in INTERNAL_USE state.", subdev, subdev);
		return -EINVAL;
	}

	if (!test_bit(IS_SUBDEV_INTERNAL_S_FMT, &subdev->state)) {
		mserr("subdev is not in s_fmt state.", subdev, subdev);
		return -EINVAL;
	}

	switch (type) {
	case IS_DEVICE_SENSOR:
		sensor = (struct is_device_sensor *)device;
		mem = &sensor->resourcemgr->mem;
		break;
	case IS_DEVICE_ISCHAIN:
		ischain = (struct is_device_ischain *)device;
		mem = &ischain->resourcemgr->mem;
		break;
	default:
		err("invalid device_type(%d)", type);
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

	ret = is_subdev_internal_alloc_buffer(subdev, mem);
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

int is_subdev_internal_stop(void *device, enum is_device_type type, struct is_subdev *subdev)
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

int is_subdev_internal_s_format(void *device, enum is_device_type type, struct is_subdev *subdev,
	u32 width, u32 height, u32 bytes_per_pixel, u32 buffer_num, const char *type_name)
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

	switch (type) {
	case IS_DEVICE_SENSOR:
		break;
	case IS_DEVICE_ISCHAIN:
		FIMC_BUG(!device);
		ischain = (struct is_device_ischain *)device;

		if (subdev->id == ENTRY_PAF) {
			if (test_bit(IS_ISCHAIN_REPROCESSING, &ischain->state))
				break;

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

		break;
	default:
		err("invalid device_type(%d)", type);
		ret = -EINVAL;
		break;
	}

	subdev->output.width = width;
	subdev->output.height = height;
	subdev->output.crop.x = 0;
	subdev->output.crop.y = 0;
	subdev->output.crop.w = subdev->output.width;
	subdev->output.crop.h = subdev->output.height;
	subdev->bytes_per_pixel = bytes_per_pixel;
	subdev->buffer_num = buffer_num;
	subdev->batch_num = batch_num;

	snprintf(subdev->data_type, sizeof(subdev->data_type), "%s", type_name);

	set_bit(IS_SUBDEV_INTERNAL_S_FMT, &subdev->state);

	return ret;
};

int is_subdev_internal_open(void *device, enum is_device_type type, struct is_subdev *subdev)
{
	int ret = 0;
	struct is_device_sensor *sensor = NULL;

	if (test_bit(IS_SUBDEV_INTERNAL_USE, &subdev->state)) {
		mswarn("already INTERNAL_USE state", subdev, subdev);
		goto p_err;
	}

	switch (type) {
	case IS_DEVICE_SENSOR:
		FIMC_BUG(!device);

		sensor = (struct is_device_sensor *)device;

		ret = is_sensor_check_subdev_open(sensor, subdev, NULL);
		if (ret) {
			mserr("is_sensor_check_subdev_open is fail(%d)", subdev, subdev, ret);
			goto p_err;
		}

		if (!test_bit(IS_SUBDEV_OPEN, &subdev->state)) {
			ret = is_subdev_open(subdev, NULL, NULL);
			if (ret) {
				mserr("is_subdev_open is fail(%d)", subdev, subdev, ret);
				goto p_err;
			}
		}

		msinfo("[SS%d] %s\n", sensor, subdev, sensor->device_id, __func__);
		break;
	case IS_DEVICE_ISCHAIN:
		ret = is_subdev_open(subdev, NULL, NULL);
		if (ret) {
			mserr("is_subdev_open is fail(%d)", subdev, subdev, ret);
			goto p_err;
		}
		msinfo("%s\n", subdev, subdev, __func__);
		break;
	default:
		err("invalid device_type(%d)", type);
		ret = -EINVAL;
		break;
	}

	set_bit(IS_SUBDEV_INTERNAL_USE, &subdev->state);

p_err:
	return ret;
};

int is_subdev_internal_close(void *device, enum is_device_type type, struct is_subdev *subdev)
{
	int ret = 0;

	if (!test_bit(IS_SUBDEV_INTERNAL_USE, &subdev->state)) {
		mserr("subdev is not in INTERNAL_USE state.", subdev, subdev);
		return -EINVAL;
	}

	ret = is_subdev_close(subdev);
	if (ret)
		mserr("is_subdev_close is fail(%d)", subdev, subdev, ret);

	clear_bit(IS_SUBDEV_INTERNAL_USE, &subdev->state);

	return ret;
};
