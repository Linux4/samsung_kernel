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

static int32_t isp_k_rgb2y_block(struct isp_io_param *param)
{
	int32_t ret = 0;
	struct isp_dev_rgb2y_info rgb2y_info;

	memset(&rgb2y_info, 0x00, sizeof(rgb2y_info));

	ret = copy_from_user((void *)&rgb2y_info, param->property_param, sizeof(rgb2y_info));
	if (0 != ret) {
		printk("isp_k_rgb2y_block: copy error, ret = 0x%x \n", (uint32_t)ret);
		return -1;
	}

	REG_MWR(ISP_Y_RGB2Y_PARAM, 0x3, rgb2y_info.signal_sel);

	return -1;
}

int32_t isp_k_cfg_rgb2y(struct isp_io_param *param)
{
	int32_t ret = 0;

	if (!param) {
		printk("isp_k_cfg_rgb2y: param is null error \n");
		return -1;
	}

	if (!param->property_param) {
		printk("isp_k_cfg_rgb2y: property_param is null error \n");
		return -1;
	}

	switch (param->property) {
	case ISP_PRO_RGB2Y_BLOCK:
		ret = isp_k_rgb2y_block(param);
		break;
	default:
		printk("isp_k_cfg_rgb2y: fail cmd id %d, not supported \n", param->property);
		break;
	}

	return ret;
}