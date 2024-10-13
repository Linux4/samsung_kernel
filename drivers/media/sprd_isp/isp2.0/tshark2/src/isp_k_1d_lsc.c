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

static int32_t isp_k_1d_lsc_block(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t val = 0;
	struct isp_dev_1d_lsc_info lnc_info;

	ret = copy_from_user((void *)&lnc_info, param->property_param, sizeof(lnc_info));
	if (0 != ret) {
		printk("isp_k_1d_lsc_block: copy error, ret=0x%x\n", (uint32_t)ret);
		return -1;
	}

	REG_MWR(ISP_1D_LNC_PARAM, BIT_0, lnc_info.bypass);

	val = (lnc_info.gain_max_thr & 0x1FFF) << 1;
	REG_MWR(ISP_1D_LNC_PARAM, 0x3FFE, val);

	val = ((lnc_info.center_r0c0_col_x & 0x1FFF) << 12) | (lnc_info.center_r0c0_row_y & 0xFFF);
	REG_WR(ISP_1D_LNC_PARAM1, val);
	val = ((lnc_info.center_r0c1_col_x & 0x1FFF) << 12) | (lnc_info.center_r0c1_row_y & 0xFFF);
	REG_WR(ISP_1D_LNC_PARAM2, val);
	val = ((lnc_info.center_r1c0_col_x & 0x1FFF) << 12) | (lnc_info.center_r1c0_row_y & 0xFFF);
	REG_WR(ISP_1D_LNC_PARAM3, val);
	val = ((lnc_info.center_r1c1_col_x & 0x1FFF) << 12) | (lnc_info.center_r1c1_row_y & 0xFFF);
	REG_WR(ISP_1D_LNC_PARAM4, val);

	val = lnc_info.delta_square_r0c0_x & 0x3FFFFFF;
	REG_WR(ISP_1D_LNC_PARAM5, val);
	val = lnc_info.delta_square_r0c1_x & 0x3FFFFFF;
	REG_WR(ISP_1D_LNC_PARAM7, val);
	val = lnc_info.delta_square_r1c0_x & 0x3FFFFFF;
	REG_WR(ISP_1D_LNC_PARAM9, val);
	val = lnc_info.delta_square_r1c1_x & 0x3FFFFFF;
	REG_WR(ISP_1D_LNC_PARAM11, val);
	val = lnc_info.delta_square_r0c0_y & 0xFFFFFF;
	REG_WR(ISP_1D_LNC_PARAM6, val);
	val = lnc_info.delta_square_r0c1_y & 0xFFFFFF;
	REG_WR(ISP_1D_LNC_PARAM8, val);
	val = lnc_info.delta_square_r1c0_y & 0xFFFFFF;
	REG_WR(ISP_1D_LNC_PARAM10, val);
	val = lnc_info.delta_square_r1c1_y & 0xFFFFFF;
	REG_WR(ISP_1D_LNC_PARAM12, val);

	val = ((lnc_info.coef_r0c0_p1 & 0xFFFF) << 11) | (lnc_info.coef_r0c0_p2 & 0x7FF);
	REG_WR(ISP_1D_LNC_PARAM13, val);
	val = ((lnc_info.coef_r0c1_p1 & 0xFFFF) << 11) | (lnc_info.coef_r0c1_p2 & 0x7FF);
	REG_WR(ISP_1D_LNC_PARAM14, val);
	val = ((lnc_info.coef_r1c0_p1 & 0xFFFF) << 11) | (lnc_info.coef_r1c0_p2 & 0x7FF);
	REG_WR(ISP_1D_LNC_PARAM15, val);
	val = ((lnc_info.coef_r1c1_p1 & 0xFFFF) << 11) | (lnc_info.coef_r1c1_p2 & 0x7FF);
	REG_WR(ISP_1D_LNC_PARAM16, val);

	val = ((lnc_info.start_pos.x & 0x1FFF) << 12) | (lnc_info.start_pos.y & 0xFFF);
	REG_WR(ISP_1D_LNC_PARAM18, val);

	return ret;
}

static int32_t isp_k_1d_lsc_slice_size(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t val = 0;
	struct isp_img_size size;

	ret = copy_from_user((void *)&size, param->property_param, sizeof(struct isp_img_size));
	if (0 != ret) {
		printk("isp_k_1d_lsc_slice_size: read copy_from_user error, ret = 0x%x\n", (uint32_t)ret);
		return -1;
	}

	val = ((size.width & 0x1FFF) << 12) | (size.height & 0xFFF);

	REG_WR(ISP_1D_LNC_PARAM17, val);

	return ret;
}

static int32_t isp_k_1d_lsc_pos(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t val = 0;
	struct img_offset pos;

	ret = copy_from_user((void *)&pos, param->property_param, sizeof(struct img_offset));
	if (0 != ret) {
		printk("isp_k_1d_lsc_pos: read copy_from_user error, ret = 0x%x\n", (uint32_t)ret);
		return -1;
	}

	val = ((pos.x & 0x1FFF) << 12) | (pos.y & 0xFFF);

	REG_WR(ISP_1D_LNC_PARAM18, val);

	return ret;
}

int32_t isp_k_cfg_1d_lsc(struct isp_io_param *param)
{
	int32_t ret = 0;

	if (!param) {
		printk("isp_k_cfg_1d_lsc: param is null error.\n");
		return -1;
	}

	if (NULL == param->property_param) {
		printk("isp_k_cfg_1d_lsc: property_param is null error.\n");
		return -1;
	}

	switch(param->property) {
	case ISP_PRO_1D_LSC_BLOCK:
		ret = isp_k_1d_lsc_block(param);
		break;
	case ISP_PRO_1D_LSC_SLICE_SIZE:
		ret = isp_k_1d_lsc_slice_size(param);
		break;
	case ISP_PRO_1D_LSC_POS:
		ret = isp_k_1d_lsc_pos(param);
		break;
	default:
		printk("isp_k_cfg_1d_lsc: fail cmd id:%d, not supported.\n", param->property);
		break;
	}

	return ret;
}

