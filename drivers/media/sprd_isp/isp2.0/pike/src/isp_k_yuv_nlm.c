/*
 * Copyright (C) 2012 Spreadtrum Communications Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/uaccess.h>
#include <linux/sprd_mm.h>
#include <video/sprd_isp.h>
#include "isp_reg.h"

static int32_t isp_k_yuv_nlm_block(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t i =0;
	uint32_t val = 0;
	struct isp_dev_yuv_nlm_info ynlm_info;

	memset(&ynlm_info, 0x00, sizeof(ynlm_info));

	ret = copy_from_user((void *)&ynlm_info, param->property_param, sizeof(ynlm_info));
	if (0 != ret) {
		printk("isp_k_yuv_nlm_block: copy error, ret = 0x%x\n", (uint32_t)ret);
		return -1;
	}

	REG_MWR(ISP_YNLM_PARAM, BIT_0, ynlm_info.nlm_bypass);
	REG_MWR(ISP_YNLM_PARAM, BIT_1, ynlm_info.nlm_radial_bypass << 1);
	REG_MWR(ISP_YNLM_PARAM, BIT_2, ynlm_info.nlm_adaptive_bypass << 2);
	REG_MWR(ISP_YNLM_PARAM, BIT_3, ynlm_info.nlm_vst_bypass << 3);

	for (i = 0;i < 7; i++) {
		val = (ynlm_info.edge_str_cmp[i] & 0x3FF) | ((ynlm_info.edge_str_req[i] & 0x7F) << 10);
		REG_WR(ISP_YNLM_ADAPT_PARAM1 + i * 4, val);
	}

	val = (ynlm_info.edge_range_l & 0xFF) | ((ynlm_info.edge_range_h & 0xFF) << 8) |
		((ynlm_info.edge_time_str & 0x3) << 16) | ((ynlm_info.avg_mode & 0x3) << 18);
	REG_WR(ISP_YNLM_ADAPT_PARAM8, val);

	REG_MWR(ISP_YNLM_DEN_STERNGTH, 0x7F, ynlm_info.den_strength);

	val = (ynlm_info.center_x & 0x1FFF) | ((ynlm_info.center_y & 0xFFF) << 13);
	REG_WR(ISP_YNLM_CENTER, val);

	val = ynlm_info.center_x2 & 0x3FFFFFF;
	REG_WR(ISP_YNLM_CENTER_X2, val);

	val = ynlm_info.center_y2 & 0xFFFFFF;
	REG_WR(ISP_YNLM_CENTER_Y2, val);

	val = ynlm_info.radius & 0x3FFFFFF;
	REG_WR(ISP_YNLM_RADIUS, val);

	val = (ynlm_info.radius_p1 & 0x7FF) | ((ynlm_info.radius_p2 & 0x3FFF) << 11);
	REG_WR(ISP_YNLM_RADIUS_RANGE, val);

	val = ynlm_info.gain_max & 0xFFF;
	REG_WR(ISP_YNLM_GAIN_MAX, val);

	val = (ynlm_info.start_raw & 0xFFF) | ((ynlm_info.start_col & 0x1FFF) << 12);
	REG_WR(ISP_YNLM_IMG_PARAM1, val);

	val = ynlm_info.add_back & 0x7F;
	REG_WR(ISP_YNLM_ADD_BACK, val);

	for (i = 0; i < 24; i++) {
		val = ynlm_info.lut_w[i] & 0x3FFFFFFF;
		REG_WR(ISP_YNLM_LUT_W_0 + i * 4, val);
	}

	for (i = 0; i < 64; i++) {
		val = ynlm_info.lut_vs[i];
		REG_WR(ISP_YNLM_LUT_VS_0 + i * 4, val);
	}

	REG_WR(ISP_YNLM_IMG_PARAM0, (((2592 & 0xFFF) << 12) | (1944 & 0xFFF)));

	return ret;

}

static int32_t isp_k_yuv_nlm_slice_size(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t val = 0;
	struct isp_img_size slice_size;

	ret = copy_from_user((void *)&slice_size, param->property_param, sizeof(slice_size));
	if (0 != ret) {
		printk("isp_k_yuv_nlm_slice_size: copy_from_user error, ret = 0x%x\n",(uint32_t)ret);
		return -1;
	}

	val = (slice_size.height & 0xFFF) | ((slice_size.width & 0x1FFF) << 12);
	REG_WR(ISP_YNLM_IMG_PARAM0, val);

	return -1;
}

int32_t isp_k_cfg_yuv_nlm(struct isp_io_param *param)
{
	int32_t ret = 0;

	if (!param) {
		printk("isp_k_cfg_yuv_nlm: param is null error \n");
		return -1;
	}

	if (!param->property_param) {
		printk("isp_k_cfg_yuv_nlm: property_param is null error \n");
		return -1;
	}

	switch (param->property) {
	case ISP_PRO_YUV_NLM_BLOCK:
		ret = isp_k_yuv_nlm_block(param);
		break;
	case ISP_PRO_YUV_NLM_SLICE_SIZE:
		ret = isp_k_yuv_nlm_slice_size(param);
		break;
	default:
		printk("isp_k_cfg_yuv_nlm: fail cmd id is %d, not supported \n", param->property);
		break;
	}

	return ret;
}