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

static int32_t isp_k_rgb_gain_block(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t val = 0;
	struct isp_dev_rgb_gain_info_v1 gain_info;

	ret = copy_from_user((void *)&gain_info, param->property_param, sizeof(gain_info));
	if (0 != ret) {
		printk("isp_k_rgb_gain_block: copy error, ret=0x%x\n", (uint32_t)ret);
		return -1;
	}

	val = (gain_info.global_gain & 0xFFFF) << 16;
	REG_MWR(ISP_RGBG_PARAM, 0xFFFF0000, val);

	val = ((gain_info.r_gain & 0xFFFF) << 16) | (gain_info.b_gain & 0xFFFF);
	REG_WR(ISP_RGBG_RB, val);
	val = gain_info.g_gain & 0xFFFF;
	REG_WR(ISP_RGBG_G, val);

	REG_MWR(ISP_RGBG_PARAM, BIT_0, gain_info.bypass);

	return ret;

}

int32_t isp_k_cfg_rgb_gain(struct isp_io_param *param)
{
	int32_t ret = 0;

	if (!param) {
		printk("isp_k_cfg_rgb_gain: param is null error.\n");
		return -1;
	}

	if (NULL == param->property_param) {
		printk("isp_k_cfg_rgb_gain: property_param is null error.\n");
		return -1;
	}

	switch(param->property) {
	case ISP_PRO_RGB_GAIN_BLOCK:
		ret = isp_k_rgb_gain_block(param);
		break;
	default:
		printk("isp_k_cfg_rgb_gain: fail cmd id:%d, not supported.\n", param->property);
		break;
	}

	return ret;
}
