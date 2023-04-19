// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * Copyright (c) 2022 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "is-device-ischain.h"
#include "is-subdev-ctrl.h"
#include "is-config.h"
#include "is-param.h"
#include "is-video.h"
#include "is-type.h"
#include "is-stripe.h"
#include "is-votf-id-table.h"
#include "is-core.h"
#include "is-hw-common-dma.h"
#include "pablo-lib.h"

static void __mcs_in_stripe_cfg(struct is_frame *frame,
		struct camera2_node *node,
		struct is_crop *incrop,
		struct is_crop *otcrop)
{
	incrop->w = frame->stripe_info.out.crop_width;

	/**
	 * Output crop configuration.
	 * No crop & scale.
	 */
	otcrop->x = 0;
	otcrop->y = 0;
	otcrop->w = incrop->w;
	otcrop->h = incrop->h;

	mlvdbgs(3, "[MCS][F%d] stripe_in_crop[%d][%d, %d, %d, %d]\n", frame, node->vid,
			frame->fcount, frame->stripe_info.region_id,
			incrop->x, incrop->y, incrop->w, incrop->h);
	mlvdbgs(3, "[MCS][F%d] stripe_ot_crop[%d][%d, %d, %d, %d]\n", frame, node->vid,
			frame->fcount, frame->stripe_info.region_id,
			otcrop->x, otcrop->y, otcrop->w, otcrop->h);
}

static int __ischain_mcsc_slot(struct camera2_node *node, u32 *pindex)
{
	int ret = 0;

	switch (node->vid) {
	case IS_LVN_MCSC_P0:
		*pindex = PARAM_MCS_OUTPUT0;
		ret = 2;
		break;
	case IS_LVN_MCSC_P1:
		*pindex = PARAM_MCS_OUTPUT1;
		ret = 2;
		break;
	case IS_LVN_MCSC_P2:
		*pindex = PARAM_MCS_OUTPUT2;
		ret = 2;
		break;
	case IS_LVN_MCSC_P3:
		*pindex = PARAM_MCS_OUTPUT3;
		ret = 2;
		break;
	case IS_LVN_MCSC_P4:
		*pindex = PARAM_MCS_OUTPUT4;
		ret = 2;
		break;
	case IS_LVN_MCSC_P5:
		*pindex = PARAM_MCS_OUTPUT5;
		ret = 2;
		break;
	default:
		ret = 0;
		break;
	}

	return ret;
}

static int __mcsc_in_cfg(struct is_device_ischain *device,
	struct is_subdev *leader,
	struct is_frame *ldr_frame,
	struct camera2_node *node,
	u32 pindex,
	IS_DECLARE_PMAP(pmap))
{
	int i;
	struct is_group *group;
	struct param_control *control;
	struct param_mcs_input *mscs_input;
	struct param_stripe_input *stripe_input;
	struct is_crop *incrop, *otcrop;
	struct is_crop incrop_cfg, otcrop_cfg;
	struct is_fmt *fmt;

	FIMC_BUG(!device);
	FIMC_BUG(!ldr_frame);
	FIMC_BUG(!node);

	group = &device->group_mcs;
	incrop = (struct is_crop *)node->input.cropRegion;
	if (IS_NULL_CROP(incrop)) {
		mlverr("[MCS] incrop is NULL", device, node->vid);
		return -EINVAL;
	}

	otcrop = (struct is_crop *)node->output.cropRegion;
	if (IS_NULL_CROP(otcrop)) {
		mlverr("[MCS] otcrop is NULL", device, node->vid);
		return -EINVAL;
	}
	incrop_cfg = *incrop;
	otcrop_cfg = *otcrop;

	fmt = is_find_format(node->pixelformat, node->flags);
	if (!fmt) {
		mlverr("[MCS] Invalid pixelformat 0x%x flags 0x%x", device, node->vid,
				node->pixelformat, node->flags);
		return -EINVAL;
	}

	if (IS_ENABLED(CHAIN_STRIPE_PROCESSING) && ldr_frame->stripe_info.region_num)
		__mcs_in_stripe_cfg(ldr_frame, node, &incrop_cfg, &otcrop_cfg);

	stripe_input = is_itf_g_param(device, ldr_frame, PARAM_MCS_STRIPE_INPUT);
	if (ldr_frame && ldr_frame->stripe_info.region_num) {
		stripe_input->index = ldr_frame->stripe_info.region_id;
		stripe_input->total_count = ldr_frame->stripe_info.region_num;
		if (!ldr_frame->stripe_info.region_id) {
			stripe_input->stripe_roi_start_pos_x[0] = 0;
			for (i = 1; i < ldr_frame->stripe_info.region_num; ++i)
				stripe_input->stripe_roi_start_pos_x[i] =
					ldr_frame->stripe_info.h_pix_num[i - 1] - incrop_cfg.x;
			stripe_input->stripe_roi_start_pos_x[i] = incrop->w;
		}
		stripe_input->left_margin = ldr_frame->stripe_info.out.left_margin;
		stripe_input->right_margin = ldr_frame->stripe_info.out.right_margin;
		stripe_input->full_width = incrop->w;
		stripe_input->full_height = incrop->h;
	} else {
		stripe_input->index = 0;
		stripe_input->total_count = 0;
		stripe_input->left_margin = 0;
		stripe_input->right_margin = 0;
		stripe_input->full_width = 0;
		stripe_input->full_height = 0;
	}

	pablo_update_repeat_param(ldr_frame, stripe_input);

	set_bit(PARAM_MCS_STRIPE_INPUT, pmap);

	mscs_input = is_itf_g_param(device, ldr_frame, pindex);
	if (test_bit(IS_GROUP_OTF_INPUT, &group->state)) {
		if (mscs_input->otf_cmd  != OTF_INPUT_COMMAND_ENABLE)
		mlvinfo("[MCS][F%d] OTF enable: %d -> %d\n", device,
			node->vid, ldr_frame->fcount,
			mscs_input->otf_cmd, OTF_INPUT_COMMAND_ENABLE);
		mscs_input->otf_cmd = OTF_INPUT_COMMAND_ENABLE;
		mscs_input->dma_cmd = DMA_INPUT_COMMAND_DISABLE;
		if (!ldr_frame->stripe_info.region_num &&
			((mscs_input->width != otcrop_cfg.w) || (mscs_input->height != otcrop_cfg.h)))
			mlvinfo("[MCS][F%d] OTF incrop[%d, %d, %d, %d]\n", device,
				node->vid, ldr_frame->fcount,
				otcrop_cfg.x, otcrop_cfg.y, otcrop_cfg.w, otcrop_cfg.h);
		mscs_input->width = otcrop_cfg.w;
		mscs_input->height = otcrop_cfg.h;
	} else {
		mscs_input->otf_cmd = OTF_INPUT_COMMAND_DISABLE;
		mscs_input->dma_cmd = DMA_INPUT_COMMAND_ENABLE;
		mscs_input->width = leader->input.width;
		mscs_input->height = leader->input.height;
		mscs_input->dma_crop_offset_x = leader->input.canv.x
						+ incrop_cfg.x;
		mscs_input->dma_crop_offset_y = leader->input.canv.y
						+ incrop_cfg.y;
		mscs_input->dma_crop_width = incrop_cfg.w;
		mscs_input->dma_crop_height = incrop_cfg.h;

		mscs_input->dma_format = fmt->hw_format;
		mscs_input->dma_bitwidth = fmt->hw_bitwidth;
		mscs_input->dma_order = fmt->hw_order;
		mscs_input->plane = fmt->hw_plane;

		mscs_input->dma_stride_y = max(mscs_input->dma_crop_width * fmt->bitsperpixel[0] / BITS_PER_BYTE,
					node->bytesperline[0]);
		mscs_input->dma_stride_c = max(mscs_input->dma_crop_width * fmt->bitsperpixel[1] / BITS_PER_BYTE,
					node->bytesperline[1]);
	}

	mscs_input->otf_format = OTF_INPUT_FORMAT_YUV422;
	mscs_input->otf_bitwidth = OTF_INPUT_BIT_WIDTH_10BIT;
	mscs_input->otf_order = OTF_INPUT_ORDER_BAYER_GR_BG;

	mscs_input->stripe_in_start_pos_x = stripe_input->stripe_roi_start_pos_x[stripe_input->index]
									- stripe_input->left_margin;
	mscs_input->stripe_roi_start_pos_x = stripe_input->stripe_roi_start_pos_x[stripe_input->index];
	mscs_input->stripe_roi_end_pos_x = stripe_input->stripe_roi_start_pos_x[stripe_input->index + 1];
	mscs_input->stripe_in_end_pos_x = stripe_input->stripe_roi_start_pos_x[stripe_input->index + 1]
									+ stripe_input->right_margin;
	set_bit(pindex, pmap);

	/* Configure Control */
	control = is_itf_g_param(device, ldr_frame, PARAM_MCS_CONTROL);
	control->cmd = CONTROL_COMMAND_START;

	set_bit(PARAM_MCS_CONTROL, pmap);

	return 0;
}

static int __mcsc_alloc_votf_buffer(struct pablo_internal_subdev *sd,
				struct param_mcs_output *mcs_output)
{
	int ret;
	struct is_core *core = is_get_is_core();
	struct is_hardware *hw = &core->hardware;
	struct is_mem *mem = CALL_HW_CHAIN_INFO_OPS(hw, get_iommu_mem, GROUP_ID_MCS0);
	u32 payload_size, header_size;

	pablo_internal_subdev_probe(sd, 0, ENTRY_INTERNAL, mem, "VOTF_HF");

	sd->width = core->vender.isp_max_width ? core->vender.isp_max_width : mcs_output->width;
	sd->height = core->vender.isp_max_height ? core->vender.isp_max_height : mcs_output->height;
	sd->num_planes = mcs_output->plane;
	sd->num_batch = 1;
	sd->num_buffers = 1;
	sd->bits_per_pixel = mcs_output->dma_bitwidth;
	sd->memory_bitwidth = mcs_output->dma_bitwidth;

	if (mcs_output->sbwc_type == NONE) {
		sd->size[0] = ALIGN(DIV_ROUND_UP(sd->width * sd->memory_bitwidth,
				BITS_PER_BYTE), 32) * sd->height;
	} else {
		payload_size = is_hw_dma_get_payload_stride(COMP, sd->memory_bitwidth, sd->width,
				1, 0, MCSC_HF_COMP_BLOCK_WIDTH, MCSC_HF_COMP_BLOCK_HEIGHT)
				* DIV_ROUND_UP(sd->height, MCSC_HF_COMP_BLOCK_HEIGHT);

		header_size = is_hw_dma_get_header_stride(sd->width, MCSC_HF_COMP_BLOCK_WIDTH, 16)
				* DIV_ROUND_UP(sd->height, MCSC_HF_COMP_BLOCK_HEIGHT);

		sd->size[0] = payload_size + header_size;
	}

	ret = CALL_I_SUBDEV_OPS(sd, alloc, sd);
	if (ret) {
		mserr("failed to alloc(%d)", sd, sd, ret);
		return ret;
	}

	msinfo("%s\n", sd, sd, __func__);

	return 0;
}

static int is_ischain_mcs_tag_hf(struct is_device_ischain *device,
				struct is_frame *frame)
{
	struct param_mcs_output *mcs_output;
	struct param_dma_output *dma_output;
	struct is_frame *votf_frame;
	struct is_framemgr *votf_framemgr;
	dma_addr_t *trs_addr, *tws_addr;
	int i, ret;
	struct pablo_lib_platform_data *plpd = pablo_lib_get_platform_data();
	struct pablo_internal_subdev *subdev = &plpd->sd_votf[PABLO_LIB_I_SUBDEV_VOTF_HF];

	mcs_output = is_itf_g_param(device, frame, PARAM_MCS_OUTPUT5);
	dma_output = is_itf_g_param(device, frame, PARAM_RGBP_HF);
	dma_output->cmd = mcs_output->dma_cmd;
	set_bit(PARAM_RGBP_HF, frame->pmap);

	if (mcs_output->dma_cmd != DMA_OUTPUT_VOTF_ENABLE)
		return 0;

	if (!test_bit(PABLO_SUBDEV_ALLOC, &subdev->state)) {
		ret = __mcsc_alloc_votf_buffer(subdev, mcs_output);
		if (ret) {
			mserr("failed to alloc_votf_buffer", subdev, subdev);
			return ret;
		}
	}

	dma_output->plane = mcs_output->plane;
	dma_output->width = mcs_output->width;
	dma_output->height = mcs_output->height;
	dma_output->v_otf_enable = true;
	dma_output->format = mcs_output->dma_format;
	dma_output->sbwc_type = mcs_output->sbwc_type;
	dma_output->bitwidth = mcs_output->dma_bitwidth;
	dma_output->msb = mcs_output->dma_bitwidth - 1;

	votf_framemgr = GET_SUBDEV_I_FRAMEMGR(subdev);
	if (!votf_framemgr) {
		mserr("internal framemgr is NULL\n", subdev, subdev);
		return -EINVAL;
	}

	votf_frame = &votf_framemgr->frames[votf_framemgr->num_frames ?
		(frame->fcount % votf_framemgr->num_frames) : 0];
	if (!votf_frame) {
		mserr("internal frame is NULL\n", subdev, subdev);
		return -EINVAL;
	}

	ret = is_hw_get_capture_slot(frame, &trs_addr, NULL, IS_LVN_MCSC_P5);
	if (ret) {
		mrerr("Invalid capture slot(%d)", subdev, frame, IS_LVN_MCSC_P5);
		return -EINVAL;
	}

	ret = is_hw_get_capture_slot(frame, &tws_addr, NULL, IS_LVN_RGBP_HF);
	if (ret) {
		mrerr("Invalid capture slot(%d)", subdev, frame, IS_LVN_RGBP_HF);
		return -EINVAL;
	}

	for (i = 0; i < votf_frame->planes; i++)
		tws_addr[i] = trs_addr[i] = votf_frame->dvaddr_buffer[i];


	ret = is_hw_mcsc_set_votf_size_config(mcs_output->width, mcs_output->height);
	if (ret) {
		mserr("is_hw_mcsc_set_votf_size_config is fail", subdev, subdev);
		return ret;
	}

	return 0;
}

static int is_ischain_mcs_tag(struct is_subdev *subdev,
	void *device_data,
	struct is_frame *frame,
	struct camera2_node *node)
{
	int ret = 0;
	struct mcs_param *mcs_param;
	struct is_crop *incrop, *otcrop;
	struct is_subdev *leader;
	struct is_device_ischain *device;
	IS_DECLARE_PMAP(pmap);
	struct is_queue *queue;
	struct camera2_node *out_node = NULL;
	struct camera2_node *cap_node = NULL;
	struct is_sub_frame *sframe;
	dma_addr_t *target_addr;
	u32 pindex = 0;
	int j, i, dma_type;

	device = (struct is_device_ischain *)device_data;

	FIMC_BUG(!subdev);
	FIMC_BUG(!device);
	FIMC_BUG(!device->is_region);
	FIMC_BUG(!frame);

	mdbgs_ischain(4, "MCSC TAG\n", device);

	incrop = (struct is_crop *)node->input.cropRegion;
	otcrop = (struct is_crop *)node->output.cropRegion;

	leader = subdev->leader;
	IS_INIT_PMAP(pmap);
	mcs_param = &device->is_region->parameter.mcs;

	if (IS_ENABLED(LOGICAL_VIDEO_NODE)) {
		queue = GET_SUBDEV_QUEUE(leader);
		if (!queue) {
			merr("queue is NULL", device);
			ret = -EINVAL;
			goto p_err;
		}

		out_node = &frame->shot_ext->node_group.leader;
		/* The input of MCSC is always OTF */
		ret = __mcsc_in_cfg(device, subdev, frame, out_node,
				PARAM_MCS_INPUT, pmap);
		if (ret) {
			mlverr("[F%d] mcsc_in_cfg fail. ret %d", device, node->vid,
					frame->fcount, ret);
			return ret;
		}

		for (i = 0; i < CAPTURE_NODE_MAX; i++) {
			cap_node = &frame->shot_ext->node_group.capture[i];
			if (!cap_node->vid)
				continue;

			dma_type = __ischain_mcsc_slot(cap_node, &pindex);
			if (dma_type == 1)
				ret = __mcsc_in_cfg(device, subdev,
						frame, cap_node,
						pindex,
						pmap);
			else if (dma_type == 2)
				ret = __mcsc_dma_out_cfg(device, frame,
						cap_node,
						pindex,
						pmap,
						i);

			if (ret) {
				mlverr("[F%d] dma_%s_cfg error\n", device, cap_node->vid,
						frame->fcount,
						(dma_type == 1) ? "in" : "out");
				return ret;
			}
		}

		for (i = 0; i < CAPTURE_NODE_MAX; i++) {
			sframe = &frame->cap_node.sframe[i];
			if (!sframe->id)
				continue;

			if ((sframe->id < IS_LVN_MCSC_P0) ||
					(sframe->id > IS_LVN_MCSC_P5))
				continue;

			target_addr = NULL;
			ret = is_hw_get_capture_slot(frame, &target_addr, NULL,
					sframe->id);
			if (ret) {
				mrerr("Invalid capture slot(%d)", device, frame,
						sframe->id);
				return -EINVAL;
			}

			if (!target_addr)
				continue;

			for (j = 0; j < sframe->num_planes; j++)
				target_addr[j] = sframe->dva[j];
		}

		ret = is_itf_s_param(device, frame, pmap);
		if (ret) {
			mrerr("is_itf_s_param is fail(%d)", device, frame, ret);
			goto p_err;
		}

		ret = is_ischain_mcs_tag_hf(device, frame);
		if (ret) {
			mrerr("is_ischain_mcs_tag_hf is fail(%d)", device, frame, ret);
			goto p_err;
		}

		return 0;
	}

p_err:
	return ret;
}

static const struct is_subdev_ops is_subdev_mcs_ops = {
	.bypass	= NULL,
	.cfg	= NULL,
	.tag	= is_ischain_mcs_tag,
};

const struct is_subdev_ops *pablo_get_is_subdev_mcs_ops(void)
{
	return &is_subdev_mcs_ops;
}
KUNIT_EXPORT_SYMBOL(pablo_get_is_subdev_mcs_ops);

#if IS_ENABLED(CONFIG_PABLO_KUNIT_TEST)
static struct pablo_kunit_subdev_mcs_func pablo_kunit_subdev_mcs_fn = {
	.mcs_tag_hf = is_ischain_mcs_tag_hf,
};

struct pablo_kunit_subdev_mcs_func *pablo_kunit_get_subdev_mcs_func(void)
{
	return &pablo_kunit_subdev_mcs_fn;
}
KUNIT_EXPORT_SYMBOL(pablo_kunit_get_subdev_mcs_func);
#endif
