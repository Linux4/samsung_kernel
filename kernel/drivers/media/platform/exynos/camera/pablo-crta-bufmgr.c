// SPDX-License-Identifier: GPL-2.0
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
	struct device			*icpu_dev;
	struct pablo_internal_subdev	subdev;
};

enum pablo_bufmgr_type {
	BUFMGR_FRAME_SINGLE,
	BUFMGR_FRAME_AEB,
	BUFMGR_SENSOR_SINGLE,
	BUFMGR_MAX,
};

struct pablo_bufmgr_info {
	char	*name;
	enum	pablo_bufmgr_type bufmgr_type;
	u32	bufmgr_num;
	u32	buf_num;
};

static const struct pablo_bufmgr_info bufmgr_info[PABLO_CRTA_BUF_MAX] = {
	/* name		bufmgr_type		bufmgr_num		buf_num */
	{"CFI",		BUFMGR_FRAME_AEB,	GROUP_ID_3AA_MAX,	5},
	{"CSI",		BUFMGR_FRAME_SINGLE,	GROUP_ID_3AA_MAX,	5},
	{"AF",		BUFMGR_FRAME_SINGLE,	GROUP_ID_3AA_MAX,	1},
	{"PRE",		BUFMGR_FRAME_SINGLE,	GROUP_ID_3AA_MAX,	2},
	{"AE",		BUFMGR_FRAME_SINGLE,	GROUP_ID_3AA_MAX,	2},
	{"AWB",		BUFMGR_FRAME_SINGLE,	GROUP_ID_3AA_MAX,	2},
	{"HST",		BUFMGR_FRAME_SINGLE,	GROUP_ID_3AA_MAX,	2},
	{"AFM",		BUFMGR_FRAME_SINGLE,	GROUP_ID_3AA_MAX,	2},
	{"CSC",		BUFMGR_SENSOR_SINGLE,	IS_STREAM_COUNT,	8},
	{"LAF",		BUFMGR_SENSOR_SINGLE,	IS_STREAM_COUNT,	1},
};

static struct pablo_crta_bufmgr *__crta_bufmgr[PABLO_CRTA_BUF_MAX];
static struct is_priv_buf *__pb_ss_static;

static int __pablo_crta_bufmgr_get_buf_size(enum pablo_crta_buf_type buf_type,
	u32 *width, u32 *height, u32 *bpp)
{
	int ret = 0;
	struct cstat_stat_buf_info info;

	switch (buf_type) {
	case PABLO_CRTA_BUF_PCFI:
		*width = (u32)sizeof(struct pablo_crta_frame_info);
		*height = 1;
		*bpp = 8;
		break;
	case PABLO_CRTA_BUF_PCSI:
		*width = (u32)sizeof(struct pablo_crta_sensor_info);
		*height = 1;
		*bpp = 8;
		break;
	case PABLO_CRTA_BUF_SS_CTRL:
		*width = (u32)sizeof(struct pablo_sensor_control_info);
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
		*width = (u32)sizeof(union itf_laser_af_data);
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

static int __pablo_crta_bufmgr_open_subdev(struct pablo_internal_subdev *sd, u32 id,
	const char *name, u32 width, u32 height, u32 bpp, u32 buf_num)
{
	int ret;
	struct is_core *core = is_get_is_core();
	struct is_hardware *hw = &core->hardware;
	struct is_mem *mem = CALL_HW_CHAIN_INFO_OPS(hw, get_iommu_mem, GROUP_ID_3AA0);

	pablo_internal_subdev_probe(sd, id, mem, name);

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

static int __pablo_crta_bufmgr_alloc_buf(struct pablo_crta_bufmgr *crta_bufmgr,
	u32 width, u32 height, u32 bpp)
{
	int ret;
	u32 buf_num, buf_type;
	struct crta_bufmgr_v1 *bufmgr;
	struct pablo_internal_subdev *sd;

	bufmgr = (struct crta_bufmgr_v1 *)crta_bufmgr->priv;
	buf_type = crta_bufmgr->type;
	sd = &bufmgr->subdev;

	buf_num = bufmgr_info[buf_type].buf_num;

	ret = __pablo_crta_bufmgr_open_subdev(sd, crta_bufmgr->id, bufmgr_info[buf_type].name,
		width, height, bpp, buf_num);

	return ret;
}

static void __pablo_crta_bufmgr_free_buf(struct crta_bufmgr_v1 *bufmgr)
{
	struct pablo_internal_subdev *sd;

	sd = &bufmgr->subdev;
	if (test_bit(PABLO_SUBDEV_ALLOC, &sd->state))
		__pablo_crta_bufmgr_close_subdev(sd);
}

static int __pablo_crta_bufmgr_attach_buf(struct crta_bufmgr_v1 *bufmgr)
{
	int ret;
	u32 buf_idx;
	struct pablo_internal_subdev *sd;
	struct is_priv_buf *pb;

	if (!bufmgr->icpu_dev)
		return 0;

	sd = &bufmgr->subdev;
	if (!test_bit(PABLO_SUBDEV_ALLOC, &sd->state))
		return 0;

	for (buf_idx = 0; buf_idx < sd->num_buffers; buf_idx++) {
		pb = sd->pb[buf_idx];
		ret = CALL_BUFOP(pb, attach_ext, pb, bufmgr->icpu_dev);
		if (ret) {
			err_adt("[%s%d]failed to attach_ext", sd->name, sd->instance);
			goto err_attach;
		}
	}

	return 0;

err_attach:
	while (buf_idx-- > 0) {
		pb = sd->pb[buf_idx];
		CALL_VOID_BUFOP(pb, detach_ext, pb);
	}
	return ret;
}

static void __pablo_crta_bufmgr_detach_buf(struct crta_bufmgr_v1 *bufmgr)
{
	u32 frame_idx;
	struct pablo_internal_subdev *sd;
	struct is_framemgr *framemgr;
	struct is_priv_buf *pb;

	if (!bufmgr->icpu_dev)
		return;

	sd = &bufmgr->subdev;
	if (!test_bit(PABLO_SUBDEV_ALLOC, &sd->state))
		return;

	framemgr = GET_SUBDEV_I_FRAMEMGR(sd);
	for (frame_idx = 0; frame_idx < framemgr->num_frames; frame_idx++) {
		pb = framemgr->frames[frame_idx].pb_output;
		CALL_VOID_BUFOP(pb, detach_ext, pb);
	}
}

static void __pablo_crta_bufmgr_flush_buf(struct is_framemgr *framemgr, u32 id, u32 fcount)
{
	struct is_frame *frame;

	/* flush frame */
	frame = peek_frame(framemgr, FS_PROCESS);
	while (frame && (!fcount || frame->fcount < fcount)) {
		info_adt("[%s%d][F%d]drop process frame[F%d]\n",
				framemgr->name, id, fcount, frame->fcount);
		trans_frame(framemgr, frame, FS_FREE);
		frame = peek_frame(framemgr, FS_PROCESS);
	}
}

static int pablo_crta_bufmgr_open(struct pablo_crta_bufmgr *crta_bufmgr)
{
	int ret;
	u32 width, height, bpp;
	struct crta_bufmgr_v1 *bufmgr;

	dbg_adt(1, "%s\n", __func__);

	/* alloc priv */
	bufmgr = vzalloc(sizeof(struct crta_bufmgr_v1));
	if (!bufmgr) {
		err_adt("failed to alloc crta_bufmgr_v1");
		return -ENOMEM;
	}
	crta_bufmgr->priv = (void *)bufmgr;

	if (__pablo_crta_bufmgr_get_buf_size(crta_bufmgr->type, &width, &height, &bpp))
		return -ENODEV;

	ret = __pablo_crta_bufmgr_alloc_buf(crta_bufmgr, width, height, bpp);
	if (ret)
		goto err_alloc_buf;

	bufmgr->icpu_dev = pablo_itf_get_icpu_dev();

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
	ulong flags;
	struct crta_bufmgr_v1 *bufmgr;
	struct pablo_internal_subdev *sd;
	struct is_framemgr *framemgr;

	if (!crta_bufmgr->priv) {
		err("crta_bufmgr_%s is not open", bufmgr_info[crta_bufmgr->type].name);
		return -EINVAL;
	}

	bufmgr = (struct crta_bufmgr_v1 *)crta_bufmgr->priv;

	dbg_adt(1, "%s\n", __func__);

	/* flush buf */
	sd = &bufmgr->subdev;
	if (test_bit(PABLO_SUBDEV_ALLOC, &sd->state)) {
		framemgr = GET_SUBDEV_I_FRAMEMGR(sd);
		framemgr_e_barrier_irqs(framemgr, FMGR_IDX_29, flags);
		__pablo_crta_bufmgr_flush_buf(framemgr, crta_bufmgr->id, 0);
		framemgr_x_barrier_irqr(framemgr, FMGR_IDX_29, flags);

		__pablo_crta_bufmgr_detach_buf(bufmgr);

		__pablo_crta_bufmgr_free_buf(bufmgr);
	}

	vfree(crta_bufmgr->priv);
	crta_bufmgr->priv = NULL;

	return 0;
}

static int pablo_crta_bufmgr_get_free_buf(struct pablo_crta_bufmgr *crta_bufmgr,
	u32 fcount, bool flush, struct pablo_crta_buf_info *buf_info)
{
	int ret = 0;
	ulong flags;
	struct crta_bufmgr_v1 *bufmgr;
	struct pablo_internal_subdev *sd;
	struct is_framemgr *framemgr = NULL;
	struct is_frame *frame = NULL;

	if (!crta_bufmgr->priv) {
		err("crta_bufmgr_%s is not open", bufmgr_info[crta_bufmgr->type].name);
		return -EINVAL;
	}

	bufmgr = (struct crta_bufmgr_v1 *)crta_bufmgr->priv;
	sd = &bufmgr->subdev;
	if (!test_bit(PABLO_SUBDEV_ALLOC, &sd->state))
		return -ENODEV;

	framemgr = GET_SUBDEV_I_FRAMEMGR(sd);
	framemgr_e_barrier_irqs(framemgr, FMGR_IDX_29, flags);
	frame = peek_frame(framemgr, FS_FREE);

	if (flush && !frame) {
		err_adt("[%s%d][F%d] failed to get free frame", sd->name, crta_bufmgr->id, fcount);
		frame_manager_print_queues(framemgr);
		__pablo_crta_bufmgr_flush_buf(framemgr, crta_bufmgr->id, fcount);
		frame = peek_frame(framemgr, FS_FREE);
	}

	if (frame) {
		frame->fcount = fcount;

		buf_info->type = crta_bufmgr->type;
		buf_info->id = crta_bufmgr->id;
		buf_info->fcount = fcount;
		buf_info->idx = frame->index;
		buf_info->dva = frame->pb_output->iova_ext;
		buf_info->dva_cstat = frame->pb_output->iova;
		buf_info->kva = frame->pb_output->kva;
		buf_info->frame = frame;
		buf_info->size = frame->pb_output->size;

		trans_frame(framemgr, frame, FS_PROCESS);
	} else {
		ret = -ENOMEM;
	}

	dbg_adt(2, "[%s%d][F%d][I%d]get_free_buf: %pad, %pad, %p",
			sd->name, crta_bufmgr->id, buf_info->fcount, buf_info->idx,
			&buf_info->dva, &buf_info->dva_cstat, buf_info->kva);

	framemgr_x_barrier_irqr(framemgr, FMGR_IDX_29, flags);

	return ret;
}

static int pablo_crta_bufmgr_get_process_buf(struct pablo_crta_bufmgr *crta_bufmgr,
	u32 fcount, struct pablo_crta_buf_info *buf_info)
{
	int ret = 0;
	ulong flags;
	struct crta_bufmgr_v1 *bufmgr;
	struct pablo_internal_subdev *sd;
	struct is_framemgr *framemgr = NULL;
	struct is_frame *frame = NULL;

	if (!crta_bufmgr->priv) {
		err("crta_bufmgr_%s is not open", bufmgr_info[crta_bufmgr->type].name);
		return -EINVAL;
	}

	bufmgr = (struct crta_bufmgr_v1 *)crta_bufmgr->priv;
	sd = &bufmgr->subdev;
	if (!test_bit(PABLO_SUBDEV_ALLOC, &sd->state))
		return -ENODEV;

	framemgr = GET_SUBDEV_I_FRAMEMGR(sd);
	framemgr_e_barrier_irqs(framemgr, FMGR_IDX_29, flags);
	frame = find_frame(framemgr, FS_PROCESS, frame_fcount, (void *)(ulong)fcount);
	framemgr_x_barrier_irqr(framemgr, FMGR_IDX_29, flags);

	if (frame) {
		buf_info->type = crta_bufmgr->type;
		buf_info->id = crta_bufmgr->id;
		buf_info->fcount = fcount;
		buf_info->idx = frame->index;
		buf_info->dva = frame->pb_output->iova_ext;
		buf_info->dva_cstat = frame->pb_output->iova;
		buf_info->kva = frame->pb_output->kva;
		buf_info->frame = frame;
		buf_info->size = frame->pb_output->size;
	} else {
		ret = -ENOMEM;
	}

	dbg_adt(2, "[%s%d][F%d][I%d]get_process_buf: %pad, %pad, %p",
			sd->name, crta_bufmgr->id, buf_info->fcount, buf_info->idx,
			&buf_info->dva, &buf_info->dva_cstat, buf_info->kva);
	return ret;
}

static int pablo_crta_bufmgr_get_static_buf(struct pablo_crta_bufmgr *crta_bufmgr,
						struct pablo_crta_buf_info *buf_info)
{
	struct crta_bufmgr_v1 *bufmgr;

	if (!crta_bufmgr->priv) {
		err("crta_bufmgr_%s is not open", bufmgr_info[crta_bufmgr->type].name);
		return -EINVAL;
	}

	bufmgr = (struct crta_bufmgr_v1 *)crta_bufmgr->priv;

	buf_info->type = crta_bufmgr->type;
	buf_info->id = crta_bufmgr->id;
	buf_info->fcount = 0;
	buf_info->idx = 0;
	buf_info->dva = __pb_ss_static->iova_ext;
	buf_info->size = __pb_ss_static->size;

	return 0;
}

static int pablo_crta_bufmgr_put_buf(struct pablo_crta_bufmgr *crta_bufmgr,
	struct pablo_crta_buf_info *buf_info)
{
	int ret = 0;
	u32 fcount;
	ulong flags;
	struct crta_bufmgr_v1 *bufmgr;
	struct pablo_internal_subdev *sd;
	struct is_framemgr *framemgr = NULL;
	struct is_frame *frame = NULL;

	if (!crta_bufmgr->priv) {
		err("crta_bufmgr_%s is not open", bufmgr_info[crta_bufmgr->type].name);
		return -EINVAL;
	}

	bufmgr = (struct crta_bufmgr_v1 *)crta_bufmgr->priv;
	sd = &bufmgr->subdev;
	if (!test_bit(PABLO_SUBDEV_ALLOC, &sd->state))
		return -ENODEV;

	if (!buf_info || !buf_info->frame) {
		err("buf_info is null");
		return -EINVAL;
	}

	if (buf_info->type != crta_bufmgr->type) {
		err("invalid buf_type(%d/%d)", buf_info->type, crta_bufmgr->type);
		return -EINVAL;
	}

	if (buf_info->id != crta_bufmgr->id) {
		err("invalid buf_id(%d/%d)", buf_info->id, crta_bufmgr->id);
		return -EINVAL;
	}

	fcount = buf_info->fcount;
	frame = buf_info->frame;

	dbg_adt(2, "[%s%d][F%d]put_buf", sd->name, crta_bufmgr->id, fcount);

	framemgr = GET_SUBDEV_I_FRAMEMGR(sd);
	framemgr_e_barrier_irqs(framemgr, FMGR_IDX_29, flags);
	if (frame->state != FS_FREE) {
		trans_frame(framemgr, frame, FS_FREE);
	} else {
		err_adt("[%s%d][F%d]invalid frame state(%d)", sd->name, crta_bufmgr->id, fcount, frame->state);
		ret = -EINVAL;
	}
	framemgr_x_barrier_irqr(framemgr, FMGR_IDX_29, flags);

	return ret;
}

static int pablo_crta_bufmgr_flush_buf(struct pablo_crta_bufmgr *crta_bufmgr, u32 fcount)
{
	ulong flags;
	struct crta_bufmgr_v1 *bufmgr;
	struct pablo_internal_subdev *sd;
	struct is_framemgr *framemgr = NULL;

	if (!crta_bufmgr->priv) {
		err("crta_bufmgr_%s is not open", bufmgr_info[crta_bufmgr->type].name);
		return -EINVAL;
	}

	bufmgr = (struct crta_bufmgr_v1 *)crta_bufmgr->priv;
	sd = &bufmgr->subdev;
	if (!test_bit(PABLO_SUBDEV_ALLOC, &sd->state))
		return -ENODEV;

	framemgr = GET_SUBDEV_I_FRAMEMGR(sd);
	framemgr_e_barrier_irqs(framemgr, FMGR_IDX_29, flags);
	info_adt("[%s%d][F%d] flush_buf (FREE%d PROCESS%d)", sd->name, crta_bufmgr->id, fcount,
		framemgr->queued_count[FS_FREE], framemgr->queued_count[FS_PROCESS]);
	__pablo_crta_bufmgr_flush_buf(framemgr, crta_bufmgr->id, fcount);
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

int pablo_crta_bufmgr_probe(void)
{
	struct is_core *core;
	struct is_mem *mem;
	u32 buf_type, size, bufmgr_idx;
	int ret;

	if (!IS_ENABLED(CRTA_CALL)) {
		probe_info("crta is not supported");
		return 0;
	}

	probe_info("%s", __func__);

	if (!__pb_ss_static) {
		core = is_get_is_core();
		mem = &core->resourcemgr.mem;
		__pb_ss_static = CALL_PTR_MEMOP(mem, alloc, mem->priv, CRTA_BUF_SS_STATIC_SIZE, NULL, 0);
		if (IS_ERR_OR_NULL(__pb_ss_static)) {
			err("failed to allocate buffer for CRTA_BUF_SS_STATIC");

			return -ENOMEM;
		}

		ret = CALL_BUFOP(__pb_ss_static, attach_ext, __pb_ss_static, pablo_itf_get_icpu_dev());
		if (ret) {
			err("failed to attach_ext for CRTA_BUF_SS_STATIC");
			CALL_VOID_BUFOP(__pb_ss_static, free, __pb_ss_static);
			__pb_ss_static = NULL;
			return ret;
		}
	}

	for (buf_type = 0; buf_type < PABLO_CRTA_BUF_MAX; buf_type++) {
		size = array_size(sizeof(struct pablo_crta_bufmgr), bufmgr_info[buf_type].bufmgr_num);
		__crta_bufmgr[buf_type] = vzalloc(size);
		if (!__crta_bufmgr[buf_type]) {
			probe_err("vzalloc failed");
			goto err_alloc;
		}
		for (bufmgr_idx = 0; bufmgr_idx < bufmgr_info[buf_type].bufmgr_num; bufmgr_idx++) {
			__crta_bufmgr[buf_type][bufmgr_idx].ops = &crta_bufmgr_ops;
			__crta_bufmgr[buf_type][bufmgr_idx].type = buf_type;
			__crta_bufmgr[buf_type][bufmgr_idx].id = bufmgr_idx;
		}
	}

	return 0;

err_alloc:
	while (buf_type-- > 0)
		vfree(__crta_bufmgr[buf_type]);

	return -ENOMEM;
}

struct pablo_crta_bufmgr *pablo_get_crta_bufmgr(enum pablo_crta_buf_type type, u32 instance, u32 hw_id)
{
	if (!IS_ENABLED(CRTA_CALL))
		return NULL;

	if (type >= PABLO_CRTA_BUF_MAX)
		return NULL;

	mdbg_adt(1, "%s: type(%s) hw_id(%d)", instance, __func__, bufmgr_info[type].name, hw_id);

	switch (bufmgr_info[type].bufmgr_type) {
	case BUFMGR_FRAME_SINGLE:
		if (is_sensor_get_frame_type(instance) <= LIB_FRAME_HDR_LONG) {
			if (hw_id < bufmgr_info[type].bufmgr_num)
				return &__crta_bufmgr[type][hw_id];
		}
		break;
	case BUFMGR_FRAME_AEB:
		if (hw_id < bufmgr_info[type].bufmgr_num)
			return &__crta_bufmgr[type][hw_id];
		break;
	case BUFMGR_SENSOR_SINGLE:
		if (instance < bufmgr_info[type].bufmgr_num)
			return &__crta_bufmgr[type][instance];
		break;
	default:
		break;
	}

	return NULL;
}
KUNIT_EXPORT_SYMBOL(pablo_get_crta_bufmgr);
