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

static int32_t isp_k_hist2_block(struct isp_io_param *param)
{
	int32_t ret = 0;
	struct isp_dev_hist2_info_v1 hist2_info;
	uint32_t val = 0;
	uint32_t i = 0;

	memset(&hist2_info, 0x00, sizeof(hist2_info));

	ret = copy_from_user((void *)&hist2_info, param->property_param, sizeof(hist2_info));
	if (0 != ret) {
		printk("isp_k_hist2_block: copy_from_user error, ret = 0x%x\n", (uint32_t)ret);
		return -1;
	}

	if (hist2_info.skip_num_clr) {
		REG_OWR(ISP_HIST2_PARAM, BIT_8);
	} else {
		REG_MWR(ISP_HIST2_PARAM, BIT_8, 0);
	}

	REG_MWR(ISP_HIST2_PARAM, 0xF0, hist2_info.skip_num << 4);

	if (hist2_info.mode) {
		REG_OWR(ISP_HIST2_PARAM, BIT_3);
	} else {
		REG_MWR(ISP_HIST2_PARAM, BIT_3, 0);
	}

	for (i = 0; i < 4; i++) {
		val = (hist2_info.hist_roi_y_s[i] & 0xFFFF) | ((hist2_info.hist_roi_x_s[i] & 0xFFFF) << 16);
		REG_WR(ISP_HIST2_ROI_S0 + i * 8, val);

		val = (hist2_info.hist_roi_y_e[i] & 0xFFFF) | ((hist2_info.hist_roi_x_e[i] & 0xFFFF) << 16);
		REG_WR(ISP_HIST2_ROI_E0 + i * 8, val);
	}

	if (hist2_info.en) {
		REG_OWR(ISP_HIST2_PARAM, BIT_1);
	} else {
		REG_MWR(ISP_HIST2_PARAM, BIT_1, 0);
	}

	if (hist2_info.bypass) {
		REG_OWR(ISP_HIST2_PARAM, BIT_0);
	} else {
		REG_MWR(ISP_HIST2_PARAM, BIT_0, 0);
	}

	return ret;
}

int32_t isp_k_cfg_hist2(struct isp_io_param *param)
{
	int32_t ret = 0;

	if (!param) {
		printk("isp_k_cfg_hist2: param is null error.\n");
		return -1;
	}

	if (NULL == param->property_param) {
		printk("isp_k_cfg_hist2: property_param is null error.\n");
		return -1;
	}

	switch (param->property) {
	case ISP_PRO_HIST2_BLOCK:
		ret = isp_k_hist2_block(param);
		break;
	default:
		printk("isp_k_cfg_hist2: fail cmd id:%d, not supported.\n", param->property);
		break;
	}

	return ret;
}
