/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Samsung Exynos SoC series Pablo IS driver
 *
 * Copyright (c) 2022 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "pablo-crta-bufmgr.h"
#include <linux/device.h>
#include "is-subdev-ctrl.h"
#include "pablo-debug.h"
#include "is-common-config.h"
#include "pablo-icpu-itf.h"
#include "is-hw-api-cstat.h"
#include "pablo-internal-subdev-ctrl.h"
#include "is-interface-sensor.h"

#define CRTA_BUF_SS_STATIC_SIZE		200

struct crta_bufmgr_v1 {
	u32				instance;
	struct device			*icpu_dev;
	struct pablo_internal_subdev	subdev[PABLO_CRTA_BUF_MAX];
};

struct pablo_buf_info {
	char	*name;
	u32	num;
};

static struct is_priv_buf *pb_ss_static;

static const struct pablo_buf_info subdev_info[PABLO_CRTA_BUF_MAX] = {
	/* name		num */
	{"CFI",		5},
	{"CSI",		5},
	{"CSC",		8},
	{"AF",		1},
	{"LAF",		1},
	{"PRE",		2},
	{"AE",		2},
	{"AWB",		2},
	{"HST",		2},
	{"AFM",		2},
};

static int __pablo_crta_bufmgr_get_buf_size(enum pablo_crta_buf_type buf_type,
	u32 *width, u32 *height, u32 *bpp)
{
	int ret = 0;
	struct cstat_stat_buf_info info;

	switch (buf_type) {
	case PABLO_CRTA_BUF_PCFI:
		*width = sizeof(struct pablo_crta_frame_info);
		*height = 1;
		*bpp = 8;
		break;
	case PABLO_CRTA_BUF_PCSI:
		*width = sizeof(struct pablo_crta_sensor_info);
		*height = 1;
		*bpp = 8;
		break;
	case PABLO_CRTA_BUF_SS_CTRL:
		*width = sizeof(struct pablo_sensor_control_info);
		*height = 1;
		*bpp = 8;
		break;
	case PABLO_CRTA_BUF_CDAF:
		ret = cstat_hw_g_stat_size(IS_CSTAT_CDAF, &info);
		*width = info.w;
		*height = info.h;
		*bpp = info.bpp;
		break;
	case PABLO_CRTA_BUF_LAF:
		*width = sizeof(union itf_laser_af_data);
		*height = 1;
		*bpp = 8;
		break;
	case PABLO_CRTA_BUF_PRE_THUMB:
		ret = cstat_hw_g_stat_size(IS_CSTAT_PRE_THUMB, &info);
		*width = info.w;
		*height = info.h;
		*bpp = info.bpp;
		break;
	case PABLO_CRTA_BUF_AE_THUMB:
		ret = cstat_hw_g_stat_size(IS_CSTAT_AE_THUMB, &info);
		*width = info.w;
		*height = info.h;
		*bpp = info.bpp;
		break;
	case PABLO_CRTA_BUF_AWB_THUMB:
		ret = cstat_hw_g_stat_size(IS_CSTAT_AWB_THUMB, &info);
		*width = info.w;
		*height = info.h;
		*bpp = info.bpp;
		break;
	case PABLO_CRTA_BUF_RGBY_HIST:
		ret = cstat_hw_g_stat_size(IS_CSTAT_RGBY_HIST, &info);
		*width = info.w;
		*height = info.h;
		*bpp = info.bpp;
		break;
	case PABLO_CRTA_BUF_CDAF_MW:
		ret = cstat_hw_g_stat_size(IS_CSTAT_CDAF_MW, &info);
		*width = info.w;
		*height = info.h;
		*bpp = info.bpp;
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

static int __pablo_crta_bufmgr_open_subdev(struct pablo_internal_subdev *sd, u32 instance,
	const char *name, u32 width, u32 height, u32 bpp, u32 buf_num)
{
	int ret;
	struct is_core *core = is_get_is_core();
	struct is_hardware *hw = &core->hardware;
	struct is_mem *mem = CALL_HW_CHAIN_INFO_OPS(hw, get_iommu_mem, GROUP_ID_3AA0);

	pablo_internal_subdev_probe(sd, instance, ENTRY_INTERNAL, mem, name);

	sd->width = width;
	sd->height = height;
	sd->num_planes = 1;
	sd->num_batch = 1;
	sd->num_buffers = buf_num;
	sd->bits_per_pixel = bpp;
	sd->memory_bitwidth = bpp;
	sd->size[0] = ALIGN(DIV_ROUND_UP(sd->width * sd->memory_bitwidth, BITS_PER_BYTE), 32)
			* sd->height;

	ret = CALL_I_SUBDEV_OPS(sd, alloc, sd);
	if (ret) {
		mserr("failed to alloc(%d)", sd, sd, ret);
		return ret;
	}

	return 0;
}

static void __pablo_crta_bufmgr_close_subdev(struct pablo_internal_subdev *sd)
{
	if (CALL_I_SUBDEV_OPS(sd, free, sd))
		mserr("failed to free", sd, sd);
}

static int __pablo_crta_bufmgr_alloc_buf(struct crta_bufmgr_v1 *bufmgr)
{
	int ret;
	u32 sd_id, width, height, bpp, buf_num;
	struct pablo_internal_subdev *sd;

	for (sd_id = PABLO_CRTA_BUF_BASE; sd_id < PABLO_CRTA_BUF_MAX; sd_id++) {
		sd = &bufmgr->subdev[sd_id];
		if(__pablo_crta_bufmgr_get_buf_size(sd_id, &width, &height, &bpp))
			continue;

		buf_num = subdev_info[sd_id].num;

		ret = __pablo_crta_bufmgr_open_subdev(sd, bufmgr->instance, subdev_info[sd_id].name,
			width, height, bpp, buf_num);
		if (ret)
			goto err_sd;
	}

	return 0;

err_sd:
	while (sd_id-- > PABLO_CRTA_BUF_BASE) {
		sd = &bufmgr->subdev[sd_id];
		if (test_bit(IS_SUBDEV_OPEN, &sd->state))
			__pablo_crta_bufmgr_close_subdev(sd);
	}
	return ret;
}

static void __pablo_crta_bufmgr_free_buf(struct crta_bufmgr_v1 *bufmgr)
{
	u32 sd_id;
	struct pablo_internal_subdev *sd;

	for (sd_id = PABLO_CRTA_BUF_BASE; sd_id < PABLO_CRTA_BUF_MAX; sd_id++) {
		sd = &bufmgr->subdev[sd_id];
		if (test_bit(PABLO_SUBDEV_ALLOC, &sd->state))
			__pablo_crta_bufmgr_close_subdev(sd);
	}
}

static int __pablo_crta_bufmgr_attach_buf(struct crta_bufmgr_v1 *bufmgr)
{
	int ret;
	u32 sd_id, buf_idx;
	struct pablo_internal_subdev *sd;
	struct is_priv_buf *pb;

	if (!bufmgr->icpu_dev)
		return 0;

	for (sd_id = PABLO_CRTA_BUF_BASE; sd_id < PABLO_CRTA_BUF_MAX; sd_id++) {
		sd = &bufmgr->subdev[sd_id];
		if (!test_bit(PABLO_SUBDEV_ALLOC, &sd->state))
			continue;

		for (buf_idx = 0; buf_idx < sd->num_buffers; buf_idx++) {
			pb = sd->pb[buf_idx];
			ret = CALL_BUFOP(pb, attach_ext, pb, bufmgr->icpu_dev);
			if (ret) {
				merr_adt("[%s]failed to attach_ext", bufmgr->instance, sd->name);
				goto err_attach;
			}
		}
	}

	return 0;

err_attach:
	while (buf_idx-- > 0) {
		pb = sd->pb[buf_idx];
		CALL_VOID_BUFOP(pb, detach_ext, pb);
	}
	while (sd_id-- > PABLO_CRTA_BUF_BASE) {
		sd = &bufmgr->subdev[sd_id];
		if (!test_bit(PABLO_SUBDEV_ALLOC, &sd->state))
			continue;

		for (buf_idx = 0; buf_idx < sd->num_buffers; buf_idx++) {
			pb = sd->pb[buf_idx];
			CALL_VOID_BUFOP(pb, detach_ext, pb);
		}
	}
	return ret;
}

static void __pablo_crta_bufmgr_detach_buf(struct crta_bufmgr_v1 *bufmgr)
{
	u32 sd_id, frame_idx;
	struct pablo_internal_subdev *sd;
	struct is_framemgr *framemgr;
	struct is_priv_buf *pb;

	if (!bufmgr->icpu_dev)
		return;

	for (sd_id = PABLO_CRTA_BUF_BASE; sd_id < PABLO_CRTA_BUF_MAX; sd_id++) {
		sd = &bufmgr->subdev[sd_id];
		if (!test_bit(PABLO_SUBDEV_ALLOC, &sd->state))
			continue;

		framemgr = GET_SUBDEV_I_FRAMEMGR(sd);
		for (frame_idx = 0; frame_idx < framemgr->num_frames; frame_idx++) {
			pb = framemgr->frames[frame_idx].pb_output;
			CALL_VOID_BUFOP(pb, detach_ext, pb);
		}
	}
}

static void __pablo_crta_bufmgr_flush_buf(struct is_framemgr *framemgr, u32 instance, u32 fcount)
{
	struct is_frame *frame;

	/* flush frame */
	frame = peek_frame(framemgr, FS_PROCESS);
	while (frame && (!fcount || frame->fcount < fcount)) {
		minfo_adt("[%s][F%d]drop process frame[F%d]\n", instance,
				framemgr->name, fcount, frame->fcount);
		trans_frame(framemgr, frame, FS_FREE);
		frame = peek_frame(framemgr, FS_PROCESS);
	}
}

static int pablo_crta_bufmgr_open(struct pablo_crta_bufmgr *crta_bufmgr, u32 instance)
{
	int ret;
	struct crta_bufmgr_v1 *bufmgr;

	mdbg_adt(1, "%s\n", instance, __func__);

	if (instance >= IS_STREAM_COUNT) {
		err("invalid instance(%d)", instance);
		return -EINVAL;
	}

	/* alloc priv */
	bufmgr = vzalloc(sizeof(struct crta_bufmgr_v1));
	if (!bufmgr) {
		merr_adt("failed to alloc crta_bufmgr_v1", instance);
		return -ENOMEM;
	}
	crta_bufmgr->priv = (void *)bufmgr;

	bufmgr->instance = instance;
	bufmgr->icpu_dev = pablo_itf_get_icpu_dev();

	ret = __pablo_crta_bufmgr_alloc_buf(bufmgr);
	if (ret)
		goto err_alloc_buf;

	ret = __pablo_crta_bufmgr_attach_buf(bufmgr);
	if (ret)
		goto err_attach_buf;

	return 0;

err_attach_buf:
	__pablo_crta_bufmgr_free_buf(bufmgr);
err_alloc_buf:
	vfree(crta_bufmgr->priv);
	crta_bufmgr->priv = NULL;
	return ret;
}

static int pablo_crta_bufmgr_close(struct pablo_crta_bufmgr *crta_bufmgr)
{
	u32 instance, sd_id;
	ulong flags;
	struct crta_bufmgr_v1 *bufmgr;
	struct pablo_internal_subdev *sd;
	struct is_framemgr *framemgr;

	if (!crta_bufmgr->priv) {
		err("crta_bufmgr is null");
		return -EINVAL;
	}

	bufmgr = (struct crta_bufmgr_v1 *)crta_bufmgr->priv;
	instance = bufmgr->instance;

	mdbg_adt(1, "%s\n", instance, __func__);

	/* flush buf */
	for (sd_id = PABLO_CRTA_BUF_BASE; sd_id < PABLO_CRTA_BUF_MAX; sd_id++) {
		sd = &bufmgr->subdev[sd_id];
		if (!test_bit(PABLO_SUBDEV_ALLOC, &sd->state))
			continue;

		framemgr = GET_SUBDEV_I_FRAMEMGR(sd);
		framemgr_e_barrier_irqs(framemgr, FMGR_IDX_29, flags);
		__pablo_crta_bufmgr_flush_buf(framemgr, instance, 0);
		framemgr_x_barrier_irqr(framemgr, FMGR_IDX_29, flags);
	}

	__pablo_crta_bufmgr_detach_buf(bufmgr);

	__pablo_crta_bufmgr_free_buf(bufmgr);

	vfree(crta_bufmgr->priv);
	crta_bufmgr->priv = NULL;

	return 0;
}

static int pablo_crta_bufmgr_get_free_buf(struct pablo_crta_bufmgr *crta_bufmgr,
	enum pablo_crta_buf_type buf_type, u32 fcount, bool flush, struct pablo_crta_buf_info *buf_info)
{
	int ret = 0;
	u32 instance;
	ulong flags;
	struct crta_bufmgr_v1 *bufmgr;
	struct pablo_internal_subdev *sd;
	struct is_framemgr *framemgr = NULL;
	struct is_frame *frame = NULL;

	if (!crta_bufmgr->priv) {
		err("crta_bufmgr is null");
		return -EINVAL;
	}

	if (buf_type >= PABLO_CRTA_BUF_MAX) {
		err("invalid buf_type(%d)", buf_type);
		return -EINVAL;
	}

	bufmgr = (struct crta_bufmgr_v1 *)crta_bufmgr->priv;
	sd = &bufmgr->subdev[buf_type];
	if (!test_bit(PABLO_SUBDEV_ALLOC, &sd->state))
		return -ENODEV;

	instance = bufmgr->instance;
	framemgr = GET_SUBDEV_I_FRAMEMGR(sd);
	framemgr_e_barrier_irqs(framemgr, FMGR_IDX_29, flags);
	frame = peek_frame(framemgr, FS_FREE);

	if (flush && !frame) {
		merr_adt("[%s][F%d] failed to get free frame", instance, framemgr->name, fcount);
		frame_manager_print_queues(framemgr);
		__pablo_crta_bufmgr_flush_buf(framemgr, instance, fcount);
		frame = peek_frame(framemgr, FS_FREE);
	}

	if (frame) {
		frame->instance = instance;
		frame->fcount = fcount;

		buf_info->instance = instance;
		buf_info->fcount = fcount;
		buf_info->type = buf_type;
		buf_info->id = frame->index;
		buf_info->dva = frame->pb_output->iova_ext;
		buf_info->dva_cstat = frame->pb_output->iova;
		buf_info->kva = frame->pb_output->kva;
		buf_info->frame = frame;
		buf_info->size = frame->pb_output->size;

		trans_frame(framemgr, frame, FS_PROCESS);
	} else {
		ret = -ENOMEM;
	}

	mdbg_adt(2, "[%s][F%d][I%d]get_free_buf: %pad, %pad, %p", instance,
			sd->name, buf_info->fcount, buf_info->id,
			&buf_info->dva, &buf_info->dva_cstat, buf_info->kva);

	framemgr_x_barrier_irqr(framemgr, FMGR_IDX_29, flags);

	return ret;
}

static int pablo_crta_bufmgr_get_process_buf(struct pablo_crta_bufmgr *crta_bufmgr,
	enum pablo_crta_buf_type buf_type, u32 fcount, struct pablo_crta_buf_info *buf_info)
{
	int ret = 0;
	u32 instance;
	ulong flags;
	struct crta_bufmgr_v1 *bufmgr;
	struct pablo_internal_subdev *sd;
	struct is_framemgr *framemgr = NULL;
	struct is_frame *frame = NULL;

	if (!crta_bufmgr->priv) {
		err("crta_bufmgr is null");
		return -EINVAL;
	}

	if (buf_type >= PABLO_CRTA_BUF_MAX) {
		err("invalid buf_type(%d)", buf_type);
		return -EINVAL;
	}

	bufmgr = (struct crta_bufmgr_v1 *)crta_bufmgr->priv;
	sd = &bufmgr->subdev[buf_type];
	if (!test_bit(PABLO_SUBDEV_ALLOC, &sd->state))
		return -ENODEV;

	instance = bufmgr->instance;
	framemgr = GET_SUBDEV_I_FRAMEMGR(sd);
	framemgr_e_barrier_irqs(framemgr, FMGR_IDX_29, flags);
	frame = find_frame(framemgr, FS_PROCESS, frame_fcount, (void *)(ulong)fcount);
	framemgr_x_barrier_irqr(framemgr, FMGR_IDX_29, flags);

	if (frame) {
		buf_info->instance = instance;
		buf_info->fcount = fcount;
		buf_info->type = buf_type;
		buf_info->id = frame->index;
		buf_info->dva = frame->pb_output->iova_ext;
		buf_info->dva_cstat = frame->pb_output->iova;
		buf_info->kva = frame->pb_output->kva;
		buf_info->frame = frame;
		buf_info->size = frame->pb_output->size;
	} else {
		ret = -ENOMEM;
	}

	mdbg_adt(2, "[%s][F%d][I%d]get_process_buf: %pad, %pad, %p", instance,
			sd->name, buf_info->fcount, buf_info->id,
			&buf_info->dva, &buf_info->dva_cstat, buf_info->kva);
	return ret;
}

static int pablo_crta_bufmgr_get_static_buf(struct pablo_crta_bufmgr *crta_bufmgr,
						enum pablo_crta_static_buf_type buf_type,
						struct pablo_crta_buf_info *buf_info)
{
	u32 instance;
	struct crta_bufmgr_v1 *bufmgr;

	if (!crta_bufmgr->priv) {
		err("crta_bufmgr is null");
		return -EINVAL;
	}

	if (buf_type != PABLO_CRTA_STATIC_BUF_SENSOR) {
		err("invalid buf_type(%d)", buf_type);
		return -EINVAL;
	}

	bufmgr = (struct crta_bufmgr_v1 *)crta_bufmgr->priv;
	instance = bufmgr->instance;

	buf_info->instance = instance;
	buf_info->fcount = 0;
	buf_info->id = 0;
	buf_info->dva = pb_ss_static->iova_ext;
	buf_info->size = pb_ss_static->size;

	return 0;
}

static int pablo_crta_bufmgr_put_buf(struct pablo_crta_bufmgr *crta_bufmgr,
	struct pablo_crta_buf_info *buf_info)
{
	int ret = 0;
	u32 instance, fcount;
	ulong flags;
	struct crta_bufmgr_v1 *bufmgr;
	struct pablo_internal_subdev *sd;
	struct is_framemgr *framemgr = NULL;
	struct is_frame *frame = NULL;

	if (!crta_bufmgr->priv) {
		err("crta_bufmgr is null");
		return -EINVAL;
	}

	if (!buf_info || !buf_info->frame) {
		err("buf_info is null");
		return -EINVAL;
	}

	if (buf_info->type >= PABLO_CRTA_BUF_MAX) {
		err("invalid buf_type(%d)", buf_info->type);
		return -EINVAL;
	}

	bufmgr = (struct crta_bufmgr_v1 *)crta_bufmgr->priv;
	sd = &bufmgr->subdev[buf_info->type];
	if (!test_bit(PABLO_SUBDEV_ALLOC, &sd->state))
		return 0;

	instance = bufmgr->instance;
	fcount = buf_info->fcount;
	frame = buf_info->frame;

	mdbg_adt(2, "[%s][F%d]put_buf", instance, sd->name, fcount);

	framemgr = GET_SUBDEV_I_FRAMEMGR(sd);
	framemgr_e_barrier_irqs(framemgr, FMGR_IDX_29, flags);
	if (frame->state != FS_FREE) {
		trans_frame(framemgr, frame, FS_FREE);
	} else {
		merr_adt("[%s][F%d]invalid frame state(%d)", instance, sd->name, fcount, frame->state);
		ret = -EINVAL;
	}
	framemgr_x_barrier_irqr(framemgr, FMGR_IDX_29, flags);

	return ret;
}

static int pablo_crta_bufmgr_flush_buf(struct pablo_crta_bufmgr *crta_bufmgr,
	enum pablo_crta_buf_type buf_type, u32 fcount)
{
	u32 instance;
	ulong flags;
	struct crta_bufmgr_v1 *bufmgr;
	struct pablo_internal_subdev *sd;
	struct is_framemgr *framemgr = NULL;

	if (!crta_bufmgr->priv) {
		err("crta_bufmgr is null");
		return -EINVAL;
	}

	if (buf_type >= PABLO_CRTA_BUF_MAX) {
		err("invalid buf_type(%d)", buf_type);
		return -EINVAL;
	}

	bufmgr = (struct crta_bufmgr_v1 *)crta_bufmgr->priv;
	sd = &bufmgr->subdev[buf_type];
	if (!test_bit(PABLO_SUBDEV_ALLOC, &sd->state))
		return 0;

	instance = bufmgr->instance;
	framemgr = GET_SUBDEV_I_FRAMEMGR(sd);
	framemgr_e_barrier_irqs(framemgr, FMGR_IDX_29, flags);
	minfo_adt("[%s][F%d] flush_buf (FREE%d PROCESS%d)", instance, sd->name, fcount,
		framemgr->queued_count[FS_FREE], framemgr->queued_count[FS_PROCESS]);
	__pablo_crta_bufmgr_flush_buf(framemgr, instance, fcount);
	framemgr_x_barrier_irqr(framemgr, FMGR_IDX_29, flags);

	return 0;
}

static const struct pablo_crta_bufmgr_ops crta_bufmgr_ops = {
	.open			= pablo_crta_bufmgr_open,
	.close			= pablo_crta_bufmgr_close,
	.get_free_buf		= pablo_crta_bufmgr_get_free_buf,
	.get_process_buf	= pablo_crta_bufmgr_get_process_buf,
	.get_static_buf		= pablo_crta_bufmgr_get_static_buf,
	.put_buf		= pablo_crta_bufmgr_put_buf,
	.flush_buf		= pablo_crta_bufmgr_flush_buf,
};

#if IS_ENABLED(CRTA_CALL)
int pablo_crta_bufmgr_probe(struct pablo_crta_bufmgr *crta_bufmgr)
{
	struct is_core *core;
	struct is_mem *mem;
	int ret;

	probe_info("%s", __func__);

	if (!pb_ss_static) {
		core = is_get_is_core();
		mem = &core->resourcemgr.mem;
		pb_ss_static = CALL_PTR_MEMOP(mem, alloc, mem->priv, CRTA_BUF_SS_STATIC_SIZE, NULL, 0);
		if (IS_ERR_OR_NULL(pb_ss_static)) {
			err("failed to allocate buffer for CRTA_BUF_SS_STATIC");

			return -ENOMEM;
		}

		ret = CALL_BUFOP(pb_ss_static, attach_ext, pb_ss_static, pablo_itf_get_icpu_dev());
		if (ret) {
			err("failed to attach_ext for CRTA_BUF_SS_STATIC");
			CALL_VOID_BUFOP(pb_ss_static, free, pb_ss_static);
			pb_ss_static = NULL;
			return ret;
		}
	}

	crta_bufmgr->ops = &crta_bufmgr_ops;

	return 0;
}
KUNIT_EXPORT_SYMBOL(pablo_crta_bufmgr_probe);
#endif
