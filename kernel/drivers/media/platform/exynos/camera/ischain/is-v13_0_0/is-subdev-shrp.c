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

#include <linux/videodev2_exynos_media.h>
#include "pablo-dvfs.h"
#include "is-device-ischain.h"
#include "is-device-sensor.h"
#include "is-subdev-ctrl.h"
#include "is-config.h"
#include "is-param.h"
#include "is-video.h"
#include "is-type.h"
#include "is-hw-dvfs.h"
#include "is-hw-shrp.h"
#include "is-stripe.h"
#include "is-votf-id-table.h"
#include "is-dvfs-config.h"

#define SHRP_CMD_HF_OTF 4

static int __ischain_shrp_slot(struct camera2_node *node, u32 *pindex)
{
	int ret = 0;

	switch (node->vid) {
	case IS_LVN_SHRP_HF:
		*pindex = PARAM_SHRP_HF_DMA_INPUT;
		ret = 1;
		break;
	case IS_LVN_SHRP_SEG:
		*pindex = PARAM_SHRP_SEG;
		ret = 1;
		break;
	default:
		ret = 0;
		break;
	}

	return ret;
}

static inline int __check_cropRegion(struct is_device_ischain *device, struct camera2_node *node)
{
	if (IS_NULL_CROP((struct is_crop *)node->input.cropRegion)) {
		mlverr("incrop is NULL", device, node->vid);
		return -EINVAL;
	}

	if (IS_NULL_CROP((struct is_crop *)node->output.cropRegion)) {
		mlverr("otcrop is NULL", device, node->vid);
		return -EINVAL;
	}

	return 0;
}

static int __shrp_dma_out_cfg(struct is_device_ischain *device,
	struct is_subdev *leader,
	struct is_frame *frame,
	struct camera2_node *node,
	u32 pindex,
	IS_DECLARE_PMAP(pmap))
{
	struct is_fmt *fmt;
	struct param_dma_output *dma_output;
	struct is_crop *incrop, *otcrop;
	u32 hw_format = DMA_INOUT_FORMAT_YUV422;
	u32 hw_bitwidth = DMA_INOUT_BIT_WIDTH_10BIT;
	u32 hw_msb = (DMA_INOUT_BIT_WIDTH_10BIT - 1);
	u32 hw_sbwc = DMA_INPUT_SBWC_DISABLE;
	u32 hw_order, hw_plane = 2;
	u32 width, flag_extra;
	int ret = 0;

	FIMC_BUG(!frame);
	FIMC_BUG(!node);

	dma_output = is_itf_g_param(device, frame, pindex);

	if (dma_output->cmd != node->request)
		mlvinfo("[F%d] WDMA enable: %d -> %d\n", device,
			node->vid, frame->fcount,
			dma_output->cmd, node->request);
	dma_output->cmd = DMA_OUTPUT_COMMAND_DISABLE;

	set_bit(pindex, pmap);

	if (!node->request)
		return 0;

	ret = __check_cropRegion(device, node);
	if (ret)
		return ret;

	incrop = (struct is_crop *)node->input.cropRegion;
	otcrop = (struct is_crop *)node->output.cropRegion;

	mdbgs_ischain(4, "%s : otcrop (w x h) = (%d x %d)\n", device, __func__, otcrop->w, otcrop->h);
	width = incrop->w;

	fmt = is_find_format(node->pixelformat, node->flags);
	if (!fmt) {
		merr("pixel format(0x%x) is not found", device, node->pixelformat);
		return -EINVAL;
	}

	hw_format = fmt->hw_format;
	/* FIXME */
	/* hw_bitwidth = fmt->hw_bitwidth; */
	hw_msb = hw_bitwidth - 1;
	hw_order = fmt->hw_order;
	hw_plane = fmt->hw_plane;
	flag_extra = (node->flags & PIXEL_TYPE_EXTRA_MASK)
			>> PIXEL_TYPE_EXTRA_SHIFT;
	if (flag_extra)
		hw_sbwc = (SBWC_BASE_ALIGN_MASK | flag_extra);

	if (dma_output->format != fmt->hw_format)
		mlvinfo("[F%d]WDMA format: %d -> %d\n", device,
			node->vid, frame->fcount,
			dma_output->format, fmt->hw_format);
	if (dma_output->bitwidth != fmt->hw_bitwidth)
		mlvinfo("[F%d]WDMA bitwidth: %d -> %d\n", device,
			node->vid, frame->fcount,
			dma_output->bitwidth, fmt->hw_bitwidth);
	if (dma_output->sbwc_type != hw_sbwc)
		mlvinfo("[F%d][%d] WDMA sbwc_type: %d -> %d\n", device,
			node->vid, frame->fcount, pindex,
			dma_output->sbwc_type, hw_sbwc);
	if (!frame->stripe_info.region_num && ((dma_output->width != otcrop->w) ||
		(dma_output->height != otcrop->h)))
		mlvinfo("[F%d][%d] WDMA otcrop[%d, %d, %d, %d]\n", device,
			node->vid, frame->fcount, pindex,
			otcrop->x, otcrop->y, otcrop->w, otcrop->h);

	dma_output->cmd = DMA_OUTPUT_COMMAND_ENABLE;
	dma_output->bitwidth = fmt->hw_bitwidth;
	dma_output->msb = fmt->hw_bitwidth - 1;
	dma_output->format = fmt->hw_format;
	dma_output->order = fmt->hw_order;
	dma_output->plane = fmt->hw_plane;

	dma_output->dma_crop_offset_x = incrop->x;
	dma_output->dma_crop_offset_y = incrop->y;
	dma_output->dma_crop_width = incrop->w;
	dma_output->dma_crop_height = incrop->h;

	dma_output->width = otcrop->w;
	dma_output->height = otcrop->h;

	dma_output->stride_plane0 = max(node->bytesperline[0], (otcrop->w * fmt->bitsperpixel[0]) / BITS_PER_BYTE);
	dma_output->stride_plane1 = max(node->bytesperline[1], (otcrop->w * fmt->bitsperpixel[1]) / BITS_PER_BYTE);

	return ret;
}

static void __shrp_update_yuvsc_otf_out(struct is_device_ischain *device,
					struct is_frame *ldr_frame,
					struct param_otf_output *otf_out)
{
	int i = 0;
	struct camera2_node *cap_node = NULL;
	struct {u32 w; u32 h;} sc_target = { 0, };
	struct {u32 h; u32 v;} zoom_ratio = { GET_ZOOM_RATIO(10, 10), GET_ZOOM_RATIO(10, 10) };
	u32 target_ratio = GET_ZOOM_RATIO(10, 10);

	for (i = 0; i < CAPTURE_NODE_MAX; i++) {
		cap_node = &ldr_frame->shot_ext->node_group.capture[i];
		switch (cap_node->vid) {
		case IS_LVN_MCSC_P0:
		case IS_LVN_MCSC_P1:
		case IS_LVN_MCSC_P2:
		case IS_LVN_MCSC_P3:
		case IS_LVN_MCSC_P4:
			if (cap_node->request & BIT(0)) {
				if (sc_target.w < ((struct is_crop *)cap_node->output.cropRegion)->w)
					sc_target.w = ((struct is_crop *)cap_node->output.cropRegion)->w;

				if (sc_target.h < ((struct is_crop *)cap_node->output.cropRegion)->h)
					sc_target.h = ((struct is_crop *)cap_node->output.cropRegion)->h;
			}
			break;
		case IS_LVN_MCSC_P5:
		default:
			break;
		}

	}

	if (otf_out->width < sc_target.w)
		zoom_ratio.h = GET_ZOOM_RATIO(otf_out->width, sc_target.w);

	if (otf_out->height < sc_target.h)
		zoom_ratio.v = GET_ZOOM_RATIO(otf_out->height, sc_target.h);

	target_ratio = min(zoom_ratio.h, zoom_ratio.v);
	/* SHRP SC scale up limitation: >= 1.1 && <= 2.5 */
	if (target_ratio > GET_ZOOM_RATIO(10, 11))
		target_ratio = GET_ZOOM_RATIO(10, 10);
	else if (target_ratio < GET_ZOOM_RATIO(10, 25))
		target_ratio = GET_ZOOM_RATIO(10, 25);

	if (target_ratio < GET_ZOOM_RATIO(10, 10)) {
		mdbgs_ischain(4, "%s: max mcsc output size(%dx%d), zoom ratio %d",
				device, __func__, sc_target.w, sc_target.h, target_ratio);
		otf_out->width = ALIGN(GET_SCALED_SIZE(otf_out->width, target_ratio), 2);
		otf_out->height = ALIGN(GET_SCALED_SIZE(otf_out->height, target_ratio), 2);
	}

}

static void __shrp_update_size_for_mcsc(struct is_device_ischain *device,
					struct is_frame *frame,
					u32 width, u32 height)
{
	struct param_mcs_input *mscs_input;

	mscs_input = is_itf_g_param(device, NULL, PARAM_MCSC_OTF_INPUT);

	mdbgs_ischain(4, "%s: SHRP size in MCSC OTF INPUT PARAM: %dx%d -> %dx%d", device, __func__,
			mscs_input->shrp_width, mscs_input->shrp_height,
			width, height);

	mscs_input->shrp_width = width;
	mscs_input->shrp_height = height;
}

static inline int __get_dvfs_scenario_mode(struct is_device_ischain *device)
{
	int mode;
	int dvfs_scenario;

	if (!device)
		return 0;

	if (!device->resourcemgr)
		return 0;

	dvfs_scenario = device->resourcemgr->dvfs_ctrl.dvfs_scenario;
	mode = dvfs_scenario & IS_DVFS_SCENARIO_COMMON_MODE_MASK;

	return mode;
}

static bool __shrp_need_yuvsc(struct is_device_ischain *device,
				struct is_frame *frame)
{
	bool ret = true;
	int scenario_mode;

	/* TODO: Strip processing cannot use SC */
	if (frame->stripe_info.region_num)
		ret = false;

	scenario_mode = __get_dvfs_scenario_mode(device);
	if (scenario_mode != IS_DVFS_SCENARIO_COMMON_MODE_VIDEO)
		ret = false;

	mdbgs_ischain(4, "%s: SHRP need SC(%d) region_num(%d), dvfs scenario mode(%d)",
			device, __func__, ret,
			frame->stripe_info.region_num,
			scenario_mode);

	return ret;
}

static int __shrp_otf_cfg(struct is_device_ischain *device,
		struct is_group *group,
		struct is_frame *ldr_frame,
		struct camera2_node *node,
		struct is_stripe_info *stripe_info,
		IS_DECLARE_PMAP(pmap))
{
	int ret;
	struct param_otf_input *otf_in;
	struct param_otf_output *otf_out;
	struct is_crop *otcrop;

	otf_in = is_itf_g_param(device, ldr_frame, PARAM_SHRP_OTF_INPUT);
	if (!test_bit(IS_GROUP_OTF_INPUT, &group->state))
		otf_in->cmd = OTF_INPUT_COMMAND_DISABLE;
	else
		otf_in->cmd = OTF_INPUT_COMMAND_ENABLE;

	set_bit(PARAM_SHRP_OTF_INPUT, pmap);

	ret = __check_cropRegion(device, node);
	if (ret)
		return ret;

	if (!otf_in->cmd)
		return 0;

	otcrop = (struct is_crop *)node->output.cropRegion;

	if (!ldr_frame->stripe_info.region_num &&
	    ((otf_in->width != otcrop->w) || (otf_in->height != otcrop->h)))
		mlvinfo("[F%d] OTF incrop[%d, %d, %d, %d]\n",
				device, IS_VIDEO_SHRP, ldr_frame->fcount,
				otcrop->x, otcrop->y,
				otcrop->w, otcrop->h);

	if (IS_ENABLED(CHAIN_STRIPE_PROCESSING) && ldr_frame->stripe_info.region_num)
		otf_in->width = ldr_frame->stripe_info.out.crop_width;
	else
		otf_in->width = otcrop->w;
	otf_in->height = otcrop->h;
	otf_in->offset_x = 0;
	otf_in->offset_y = 0;
	otf_in->physical_width = otf_in->width;
	otf_in->physical_height = otf_in->height;
	otf_in->format = OTF_INPUT_FORMAT_YUV422;
	otf_in->bayer_crop_offset_x = 0;
	otf_in->bayer_crop_offset_y = 0;
	otf_in->bayer_crop_width = otf_in->width;
	otf_in->bayer_crop_height = otf_in->height;

	otf_out = is_itf_g_param(device, ldr_frame, PARAM_SHRP_OTF_OUTPUT);
	otf_out->cmd = OTF_OUTPUT_COMMAND_ENABLE;
	otf_out->width = otf_in->width;
	otf_out->height = otf_in->height;
	set_bit(PARAM_SHRP_OTF_OUTPUT, pmap);

	if (__shrp_need_yuvsc(device, ldr_frame))
		__shrp_update_yuvsc_otf_out(device, ldr_frame, otf_out);

	mdbgs_ischain(4, "%s : otf_in (%d x %d), otf_out (%d x %d)\n", device, __func__,
			otf_in->width, otf_in->height,
			otf_out->width, otf_out->height);

	__shrp_update_size_for_mcsc(device, ldr_frame, otf_in->width, otf_in->height);

	return 0;
}

static int __shrp_alloc_votf_buffer(struct pablo_internal_subdev *sd,
				struct param_dma_input *dma_input)
{
	int ret;
	struct is_core *core = is_get_is_core();
	struct is_hardware *hw = &core->hardware;
	struct is_mem *mem = CALL_HW_CHAIN_INFO_OPS(hw, get_iommu_mem, GROUP_ID_SHRP);
	u32 payload_size, header_size;

	pablo_internal_subdev_probe(sd, 0, mem, "VOTF_HF");

	sd->width = core->vendor.isp_max_width ? core->vendor.isp_max_width : dma_input->width;
	sd->height = core->vendor.isp_max_height ? core->vendor.isp_max_height : dma_input->height;
	sd->num_planes = dma_input->plane;
	sd->num_batch = 1;
	sd->num_buffers = 1;
	sd->bits_per_pixel = dma_input->bitwidth;
	sd->memory_bitwidth = dma_input->bitwidth;

	if (dma_input->sbwc_type == NONE) {
		sd->size[0] = ALIGN(DIV_ROUND_UP(sd->width * sd->memory_bitwidth,
				BITS_PER_BYTE), 32) * sd->height;
	} else {
		payload_size = is_hw_dma_get_payload_stride(COMP, sd->memory_bitwidth, sd->width,
				1, 0, SHRP_HF_COMP_BLOCK_WIDTH, SHRP_HF_COMP_BLOCK_HEIGHT)
				* DIV_ROUND_UP(sd->height, SHRP_HF_COMP_BLOCK_HEIGHT);

		header_size = is_hw_dma_get_header_stride(sd->width, SHRP_HF_COMP_BLOCK_WIDTH, 16)
				* DIV_ROUND_UP(sd->height, SHRP_HF_COMP_BLOCK_HEIGHT);

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

static int __shrp_set_votf_hf(struct param_dma_input *dma_input, struct is_frame *frame)
{
	struct is_frame *votf_frame;
	struct is_framemgr *votf_framemgr;
	dma_addr_t *trs_addr, *tws_addr;
	int i, ret;
	struct pablo_lib_platform_data *plpd = pablo_lib_get_platform_data();
	struct pablo_internal_subdev *subdev = &plpd->sd_votf[PABLO_LIB_I_SUBDEV_VOTF_HF_SHRP];

	if (!test_bit(PABLO_SUBDEV_ALLOC, &subdev->state)) {
		ret = __shrp_alloc_votf_buffer(subdev, dma_input);
		if (ret) {
			mserr("failed to alloc_votf_buffer", subdev, subdev);
			return ret;
		}
	}

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

	ret = is_hw_get_capture_slot(frame, &trs_addr, NULL, IS_LVN_SHRP_HF);
	if (ret) {
		mrerr("Invalid capture slot(%d)", subdev, frame, IS_LVN_SHRP_HF);
		return -EINVAL;
	}

	ret = is_hw_get_capture_slot(frame, &tws_addr, NULL, IS_LVN_RGBP_HF);
	if (ret) {
		mrerr("Invalid capture slot(%d)", subdev, frame, IS_LVN_RGBP_HF);
		return -EINVAL;
	}

	for (i = 0; i < votf_frame->planes; i++)
		tws_addr[i] = trs_addr[i] = votf_frame->dvaddr_buffer[i];

	ret = is_hw_set_hf_votf_size_config(dma_input->width, dma_input->height);
	if (ret) {
		mserr("is_hw_set_hf_votf_size_config is fail", subdev, subdev);
		return ret;
	}

	return 0;
}

static int __shrp_hf_otf_in_cfg(struct is_device_ischain *device,
	struct is_subdev *leader,
	struct is_frame *frame,
	struct camera2_node *node,
	u32 pindex,
	IS_DECLARE_PMAP(pmap))
{
	struct is_fmt *fmt;
	struct param_otf_input *otf_input;
	struct is_crop *incrop, *otcrop;
	u32 hw_format = DMA_INOUT_FORMAT_YUV422;
	u32 hw_bitwidth = DMA_INOUT_BIT_WIDTH_10BIT;
	u32 hw_msb = (DMA_INOUT_BIT_WIDTH_10BIT - 1);
	u32 hw_sbwc = DMA_INPUT_SBWC_DISABLE;
	u32 hw_order, hw_plane = 2;
	u32 width, height, flag_extra;
	int ret = 0;
	struct param_otf_output *shrp_otf_out = is_itf_g_param(device, frame, PARAM_SHRP_OTF_OUTPUT);
	struct param_otf_output *rgbp_otf_output = is_itf_g_param(device, frame, PARAM_RGBP_OTF_OUTPUT_HF);
	struct param_otf_input *mcfp_otf_input = is_itf_g_param(device, frame, PARAM_MCFP_HF);

	set_bit(pindex, pmap);

	otf_input = is_itf_g_param(device, frame, pindex);

	if (otf_input->cmd != (node->request ? OTF_INPUT_COMMAND_ENABLE : OTF_INPUT_COMMAND_DISABLE)) {
		mlvinfo("[F%d] HF OTF enable: %d -> %d\n", device,
				node->vid, frame->fcount,
				otf_input->cmd, node->request);
		if (node->request) {
			int scenario_id;
			bool reprocessing_mode;
			struct param_otf_output *hf_out =
				is_itf_g_param(device, NULL, PARAM_RGBP_OTF_OUTPUT_HF);
			hf_out->cmd = OTF_OUTPUT_COMMAND_ENABLE;
			scenario_id = is_hw_dvfs_get_scenario(device, IS_DVFS_STATIC);
			reprocessing_mode = test_bit(IS_ISCHAIN_REPROCESSING, &device->state);
			pablo_set_static_dvfs(&device->resourcemgr->dvfs_ctrl, "HF", scenario_id,
					      IS_DVFS_PATH_M2M, reprocessing_mode);
		}
	}
	otf_input->cmd = node->request ? OTF_INPUT_COMMAND_ENABLE : OTF_INPUT_COMMAND_DISABLE;

	ret = __check_cropRegion(device, node);
	if (ret)
		return ret;

	incrop = (struct is_crop *)node->input.cropRegion;
	otcrop = (struct is_crop *)node->output.cropRegion;

	if (IS_ENABLED(CHAIN_STRIPE_PROCESSING) && frame->stripe_info.region_num)
		width = frame->stripe_info.out.crop_width;
	else
		width = otcrop->w;

	height = otcrop->h;

	mdbgs_ischain(4, "%s : otcrop (w x h) = (%d x %d)\n", device, __func__, width, height);

	fmt = is_find_format(node->pixelformat, node->flags);
	if (!fmt) {
		merr("pixel format(0x%x) is not found", device, node->pixelformat);
		return -EINVAL;
	}

	hw_format = fmt->hw_format;
	/* FIXME */
	/* hw_bitwidth = fmt->hw_bitwidth; */
	hw_msb = hw_bitwidth - 1;
	hw_order = fmt->hw_order;
	hw_plane = fmt->hw_plane;
	flag_extra = (node->flags & PIXEL_TYPE_EXTRA_MASK)
			>> PIXEL_TYPE_EXTRA_SHIFT;
	if (flag_extra)
		hw_sbwc = (SBWC_BASE_ALIGN_MASK | flag_extra);

	/* TODO: Check HF strip
	if (IS_ENABLED(CHAIN_STRIPE_PROCESSING) && frame->stripe_info.region_num)
		is_ischain_g_stripe_cfg(frame, node, incrop, incrop, otcrop);
	*/

	hw_bitwidth = DMA_INOUT_BIT_WIDTH_8BIT;
	hw_msb = DMA_INOUT_BIT_WIDTH_8BIT - 1;
	otf_input->format = hw_format;
	otf_input->bitwidth = hw_bitwidth;
	otf_input->order = hw_order;
	otf_input->width = width;
	otf_input->height = height;
	otf_input->bayer_crop_offset_x = otcrop->x;
	otf_input->bayer_crop_offset_y = otcrop->y;
	otf_input->bayer_crop_width = width;
	otf_input->bayer_crop_height = height;

	mdbgs_ischain(4, "%s : HF OTF enable\n", device, __func__);

	set_bit(PARAM_RGBP_OTF_OUTPUT_HF, frame->pmap);
	set_bit(PARAM_MCFP_HF, frame->pmap);

	shrp_otf_out->cmd = OTF_OUTPUT_COMMAND_ENABLE;
	shrp_otf_out->width = otf_input->width;
	shrp_otf_out->height = otf_input->height;

	rgbp_otf_output->cmd = OTF_OUTPUT_COMMAND_ENABLE;
	rgbp_otf_output->width = otf_input->width;
	rgbp_otf_output->height = otf_input->height;
	rgbp_otf_output->format = otf_input->format;
	rgbp_otf_output->bitwidth = otf_input->bitwidth;

	mcfp_otf_input->cmd = OTF_INPUT_COMMAND_ENABLE;
	mcfp_otf_input->width = otf_input->width;
	mcfp_otf_input->height = otf_input->height;
	mcfp_otf_input->format = otf_input->format;
	mcfp_otf_input->bitwidth = otf_input->bitwidth;

	__shrp_update_size_for_mcsc(device, frame, shrp_otf_out->width, shrp_otf_out->height);

	return ret;
}

static void __reset_hf_param_cmd(struct is_device_ischain *device,
		struct is_frame *frame)
{
	struct param_otf_input *otf_input;
	struct param_otf_output *otf_output;
	struct param_dma_output *hf_dma_out;
	struct param_otf_output *hf_otf_out;

	otf_input = is_itf_g_param(device, frame, PARAM_SHRP_HF_OTF_INPUT);
	otf_input->cmd = OTF_INPUT_COMMAND_DISABLE;
	set_bit(PARAM_SHRP_HF_OTF_INPUT, frame->pmap);

	otf_output = is_itf_g_param(device, frame, PARAM_SHRP_HF_OTF_OUTPUT);
	otf_output->cmd = OTF_OUTPUT_COMMAND_DISABLE;
	set_bit(PARAM_SHRP_HF_OTF_OUTPUT, frame->pmap);

	hf_dma_out = is_itf_g_param(device, NULL, PARAM_RGBP_HF);
	hf_dma_out->cmd = DMA_OUTPUT_COMMAND_DISABLE;

	hf_otf_out = is_itf_g_param(device, NULL, PARAM_RGBP_OTF_OUTPUT_HF);
	hf_otf_out->cmd = OTF_OUTPUT_COMMAND_DISABLE;
}

static int __shrp_dma_in_cfg(struct is_device_ischain *device,
	struct is_subdev *leader,
	struct is_frame *frame,
	struct camera2_node *node,
	u32 pindex,
	IS_DECLARE_PMAP(pmap))
{
	struct is_fmt *fmt;
	struct param_dma_input *dma_input;
	struct is_crop *incrop, *otcrop;
	struct is_group *group;
	u32 hw_format = DMA_INOUT_FORMAT_YUV422;
	u32 hw_bitwidth = DMA_INOUT_BIT_WIDTH_10BIT;
	u32 hw_msb = (DMA_INOUT_BIT_WIDTH_10BIT - 1);
	u32 hw_sbwc = DMA_INPUT_SBWC_DISABLE;
	u32 hw_order, hw_plane = 2;
	u32 width, height, flag_extra;
	int ret = 0;

	if (node->vid == IS_LVN_SHRP_HF && node->request == SHRP_CMD_HF_OTF)
		return __shrp_hf_otf_in_cfg(device, leader, frame, node, PARAM_SHRP_HF_OTF_INPUT, pmap);

	set_bit(pindex, pmap);

	group = container_of(leader, struct is_group, leader);

	dma_input = is_itf_g_param(device, frame, pindex);

	if (pindex == PARAM_SHRP_DMA_INPUT && test_bit(IS_GROUP_OTF_INPUT, &group->state)) {
		dma_input->cmd = DMA_INPUT_COMMAND_DISABLE;
	} else {
		if (dma_input->cmd !=
			(node->request ? DMA_INPUT_COMMAND_ENABLE : DMA_INPUT_COMMAND_DISABLE)) {
			mlvinfo("[F%d] RDMA enable: %d -> %d\n", device,
				node->vid, frame->fcount,
				dma_input->cmd, node->request);

			if (node->vid == IS_LVN_SHRP_HF && node->request) {
				int scenario_id;
				bool reprocessing_mode;
				struct param_dma_output *hf_out = is_itf_g_param(device, NULL, PARAM_RGBP_HF);
				hf_out->cmd = DMA_OUTPUT_COMMAND_ENABLE;
				scenario_id = is_hw_dvfs_get_scenario(device, IS_DVFS_STATIC);
				reprocessing_mode =
					test_bit(IS_ISCHAIN_REPROCESSING, &device->state);
				pablo_set_static_dvfs(&device->resourcemgr->dvfs_ctrl, "HF",
						      scenario_id, IS_DVFS_PATH_M2M,
						      reprocessing_mode);
			}
		}
		dma_input->cmd = node->request ? DMA_INPUT_COMMAND_ENABLE : DMA_INPUT_COMMAND_DISABLE;

		if (!dma_input->cmd)
			return 0;
	}

	ret = __check_cropRegion(device, node);
	if (ret)
		return ret;

	incrop = (struct is_crop *)node->input.cropRegion;
	otcrop = (struct is_crop *)node->output.cropRegion;

	if (IS_ENABLED(CHAIN_STRIPE_PROCESSING) && frame->stripe_info.region_num)
		width = frame->stripe_info.out.crop_width;
	else
		width = otcrop->w;

	height = otcrop->h;

	mdbgs_ischain(4, "%s : otcrop (w x h) = (%d x %d)\n", device, __func__, width, height);

	fmt = is_find_format(node->pixelformat, node->flags);
	if (!fmt) {
		merr("pixel format(0x%x) is not found", device, node->pixelformat);
		return -EINVAL;
	}

	hw_format = fmt->hw_format;
	/* FIXME */
	/* hw_bitwidth = fmt->hw_bitwidth; */
	hw_msb = hw_bitwidth - 1;
	hw_order = fmt->hw_order;
	hw_plane = fmt->hw_plane;
	flag_extra = (node->flags & PIXEL_TYPE_EXTRA_MASK)
			>> PIXEL_TYPE_EXTRA_SHIFT;
	if (flag_extra)
		hw_sbwc = (SBWC_BASE_ALIGN_MASK | flag_extra);

	if (IS_ENABLED(CHAIN_STRIPE_PROCESSING) && frame->stripe_info.region_num &&
	    (node->vid == IS_VIDEO_SHRP))
		is_ischain_g_stripe_cfg(frame, node, incrop, incrop, otcrop);

	if (dma_input->cmd) {
		if (dma_input->sbwc_type != hw_sbwc)
			mlvinfo("[F%d]RDMA sbwc_type: %d -> %d\n", device,
				node->vid, frame->fcount,
				dma_input->sbwc_type, hw_sbwc);
		if (!frame->stripe_info.region_num && ((dma_input->width != width) ||
				(dma_input->height != height)))
			mlvinfo("[F%d]RDMA incrop[%d, %d, %d, %d]\n", device,
				node->vid, frame->fcount,
				incrop->x, incrop->y, width, height);
	}

	switch (node->vid) {
	/* RDMA */
	case IS_LVN_SHRP_HF:
		hw_bitwidth = DMA_INOUT_BIT_WIDTH_8BIT;
		hw_msb = DMA_INOUT_BIT_WIDTH_8BIT - 1;
		dma_input->v_otf_enable = OTF_OUTPUT_COMMAND_ENABLE;
		break;
	case IS_LVN_SHRP_SEG:
		hw_format = DMA_INOUT_FORMAT_SEGCONF;
		hw_bitwidth = DMA_INOUT_BIT_WIDTH_8BIT;
		hw_msb = hw_bitwidth - 1;
		dma_input->v_otf_enable = OTF_INPUT_COMMAND_DISABLE;
		break;
	default:
		if (dma_input->bitwidth != hw_bitwidth)
			mlvinfo("[F%d]RDMA bitwidth: %d -> %d\n", device,
				node->vid, frame->fcount,
				dma_input->bitwidth, hw_bitwidth);

		if (dma_input->format != hw_format)
			mlvinfo("[F%d]RDMA format: %d -> %d\n", device,
				node->vid, frame->fcount,
				dma_input->format, hw_format);
		break;
	}

	dma_input->sbwc_type= hw_sbwc;
	dma_input->format = hw_format;
	dma_input->bitwidth = hw_bitwidth;
	dma_input->msb = hw_msb;
	dma_input->order = hw_order;
	dma_input->sbwc_type = hw_sbwc;
	dma_input->plane = hw_plane;
	dma_input->width = width;
	dma_input->height = height;
	dma_input->dma_crop_offset = (incrop->x << 16) | (incrop->y << 0);
	dma_input->dma_crop_width = width;
	dma_input->dma_crop_height = height;
	dma_input->bayer_crop_offset_x = otcrop->x;
	dma_input->bayer_crop_offset_y = otcrop->y;
	dma_input->bayer_crop_width = width;
	dma_input->bayer_crop_height = height;
	dma_input->stride_plane0 = width;
	dma_input->stride_plane1 = width;
	dma_input->stride_plane2 = width;

	if (node->vid == IS_LVN_SHRP_HF) {
		struct param_otf_output *shrp_otf_out = is_itf_g_param(device, frame, PARAM_SHRP_OTF_OUTPUT);
		struct param_dma_output *dma_output = is_itf_g_param(device, frame, PARAM_RGBP_HF);

		shrp_otf_out->cmd = OTF_OUTPUT_COMMAND_ENABLE;
		shrp_otf_out->width = dma_input->width;
		shrp_otf_out->height = dma_input->height;

		mdbgs_ischain(4, "%s : HF M2M enable\n", device, __func__);

		set_bit(PARAM_RGBP_HF, frame->pmap);

		if (dma_input->v_otf_enable) {
			ret = __shrp_set_votf_hf(dma_input, frame);
			if (ret) {
				mrerr("failed to shrp_set_votf_hf, ret(%d)", device, frame, ret);
				return ret;
			}
		}

		dma_output->cmd = DMA_OUTPUT_COMMAND_ENABLE;
		dma_output->plane = dma_input->plane;
		dma_output->width = dma_input->width;
		dma_output->height = dma_input->height;
		dma_output->v_otf_enable = dma_input->v_otf_enable;
		dma_output->format = dma_input->format;
		dma_output->sbwc_type = dma_input->sbwc_type;
		dma_output->bitwidth = dma_input->bitwidth;
		dma_output->msb = dma_input->bitwidth - 1;

		__shrp_update_size_for_mcsc(device, frame, shrp_otf_out->width, shrp_otf_out->height);
	}

	if (node->vid == IS_LVN_SHRP_SEG) {
		struct param_otf_output *shrp_otf_in = is_itf_g_param(device, frame, PARAM_SHRP_OTF_INPUT);
		struct param_otf_output *shrp_otf_out = is_itf_g_param(device, frame, PARAM_SHRP_OTF_OUTPUT);

		shrp_otf_out->width = shrp_otf_in->width;
		shrp_otf_out->height = shrp_otf_in->height;
	}


	return ret;
}

static int __shrp_stripe_in_cfg(struct is_device_ischain *device,
	struct is_subdev *leader,
	struct is_frame *frame,
	struct camera2_node *node,
	u32 pindex,
	IS_DECLARE_PMAP(pmap))
{
	int ret;
	struct param_stripe_input *stripe_input;
	struct is_crop *otcrop;
	int i;

	set_bit(pindex, pmap);

	ret = __check_cropRegion(device, node);
	if (ret)
		return ret;

	otcrop = (struct is_crop *)node->output.cropRegion;

	stripe_input = is_itf_g_param(device, frame, pindex);
	if (frame->stripe_info.region_num) {
		stripe_input->index = frame->stripe_info.region_id;
		stripe_input->total_count = frame->stripe_info.region_num;
		if (!frame->stripe_info.region_id) {
			stripe_input->stripe_roi_start_pos_x[0] = 0;
			for (i = 1; i < frame->stripe_info.region_num; ++i)
				stripe_input->stripe_roi_start_pos_x[i] =
					frame->stripe_info.h_pix_num[i - 1];
		}

		stripe_input->left_margin = frame->stripe_info.out.left_margin;
		stripe_input->right_margin = frame->stripe_info.out.right_margin;
		stripe_input->full_width = otcrop->w;
		stripe_input->full_height = otcrop->h;
		stripe_input->start_pos_x = frame->stripe_info.out.crop_x +
						frame->stripe_info.out.offset_x;
	} else {
		stripe_input->index = 0;
		stripe_input->total_count = 0;
		stripe_input->left_margin = 0;
		stripe_input->right_margin = 0;
		stripe_input->full_width = 0;
		stripe_input->full_height = 0;
	}

	pablo_update_repeat_param(frame, stripe_input);

	return 0;
}

static void __shrp_control_cfg(struct is_device_ischain *device,
	struct is_group *group,
	struct is_frame *frame,
	IS_DECLARE_PMAP(pmap))
{
	struct param_control *control;

	control = is_itf_g_param(device, frame, PARAM_SHRP_CONTROL);
	if (test_bit(IS_GROUP_STRGEN_INPUT, &group->state))
		control->strgen = CONTROL_COMMAND_START;
	else
		control->strgen = CONTROL_COMMAND_STOP;

	set_bit(PARAM_SHRP_CONTROL, pmap);
}

static int is_ischain_shrp_tag(struct is_subdev *subdev,
	void *device_data,
	struct is_frame *frame,
	struct camera2_node *node)
{
	int ret = 0;
	struct is_group *group;
	struct shrp_param *shrp_param;
	struct is_crop *incrop, *otcrop;
	struct is_subdev *leader;
	struct is_device_ischain *device;
	IS_DECLARE_PMAP(pmap);
	struct camera2_node *out_node = NULL;
	struct camera2_node *cap_node = NULL;
	struct is_sub_frame *sframe;
	dma_addr_t *targetAddr;
	u64 *targetAddr_k;
	u32 n, p, pindex = 0;
	int dma_type;

	device = (struct is_device_ischain *)device_data;

	FIMC_BUG(!subdev);
	FIMC_BUG(!device);
	FIMC_BUG(!device->is_region);
	FIMC_BUG(!frame);

	incrop = (struct is_crop *)node->input.cropRegion;
	otcrop = (struct is_crop *)node->output.cropRegion;

	group = container_of(subdev, struct is_group, leader);
	leader = subdev->leader;
	IS_INIT_PMAP(pmap);
	shrp_param = &device->is_region->parameter.shrp;

	__reset_hf_param_cmd(device, frame);

	if (IS_ENABLED(LOGICAL_VIDEO_NODE)) {
		__shrp_control_cfg(device, group, frame, pmap);

		out_node = &frame->shot_ext->node_group.leader;

		ret = __shrp_otf_cfg(device, group, frame, out_node,
				&frame->stripe_info, pmap);
		if (ret) {
			mlverr("[F%d] otf_in_cfg fail. ret %d", device, node->vid,
					frame->fcount, ret);
			return ret;
		}

		ret = __shrp_stripe_in_cfg(device, subdev, frame, out_node,
				PARAM_SHRP_STRIPE_INPUT, pmap);
		if (ret) {
			mlverr("[F%d] strip_in_cfg fail. ret %d", device, node->vid,
					frame->fcount, ret);
			return ret;
		}

		out_node->result = 1;

		for (n = 0; n < CAPTURE_NODE_MAX; n++) {
			cap_node = &frame->shot_ext->node_group.capture[n];

			if (!cap_node->vid)
				continue;

			dma_type = __ischain_shrp_slot(cap_node, &pindex);
			if (dma_type == 1)
				ret = __shrp_dma_in_cfg(device, subdev, frame,
						cap_node, pindex, pmap);
			else if (dma_type == 2)
				ret = __shrp_dma_out_cfg(device, subdev, frame, cap_node,
						pindex, pmap);
			if (ret) {
				mlverr("[F%d] dma_%s_cfg error\n", device, cap_node->vid,
						frame->fcount,
						(dma_type == 1) ? "in" : "out");
				return ret;
			}

			cap_node->result = 1;
		}

		for (n = 0; n < CAPTURE_NODE_MAX; n++) {
			sframe = &frame->cap_node.sframe[n];
			if (!sframe->id)
				continue;

			if ((sframe->id >= IS_LVN_MCSC_P0) &&
					(sframe->id <= IS_LVN_MCSC_P5))
				continue;

			targetAddr = NULL;
			targetAddr_k = NULL;
			ret = is_hw_get_capture_slot(frame, &targetAddr,
					&targetAddr_k, sframe->id);
			if (ret) {
				mrerr("Invalid capture slot(%d)", device,
						frame, sframe->id);
				return -EINVAL;
			}

			if (targetAddr) {
				for (p = 0; p < sframe->num_planes; p++)
					targetAddr[p] = sframe->dva[p];
			}

			if (targetAddr_k) {
				for (p = 0; p < sframe->num_planes; p++)
					targetAddr_k[p] = sframe->kva[p];
			}

		}

		ret = is_itf_s_param(device, frame, pmap);
		if (ret) {
			mrerr("is_itf_s_param is fail(%d)", device, frame, ret);
			goto p_err;
		}

		return 0;
	}

p_err:
	return ret;
}

static int is_ischain_shrp_get(struct is_subdev *subdev,
			       struct is_frame *frame,
			       enum pablo_subdev_get_type type,
			       void *result)
{
	struct camera2_node *node;
	struct is_crop *incrop, *outcrop;

	switch (type) {
	case PSGT_REGION_NUM:
		node = &frame->shot_ext->node_group.leader;
		incrop = (struct is_crop *)node->input.cropRegion;
		outcrop = (struct is_crop *)node->output.cropRegion;

		*(int *)result = is_calc_region_num(incrop, outcrop, subdev);
		break;
	default:
		break;
	}

	return 0;
}

static const struct is_subdev_ops is_subdev_shrp_ops = {
	.bypass	= NULL,
	.cfg	= NULL,
	.tag	= is_ischain_shrp_tag,
	.get	= is_ischain_shrp_get,
};

const struct is_subdev_ops *pablo_get_is_subdev_shrp_ops(void)
{
	return &is_subdev_shrp_ops;
}
KUNIT_EXPORT_SYMBOL(pablo_get_is_subdev_shrp_ops);
