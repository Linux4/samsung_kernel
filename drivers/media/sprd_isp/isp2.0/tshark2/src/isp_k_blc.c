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

static int32_t isp_k_blc_block(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t val = 0;
	struct isp_dev_blc_info blc_info;

	ret = copy_from_user((void *)&blc_info, param->property_param, sizeof(blc_info));
	if (0 != ret) {
		printk("isp_k_blc_block: copy error, ret=0x%x\n", (uint32_t)ret);
		return -1;
	}

	val = ((blc_info.b & 0x3FF) << 10) | (blc_info.r & 0x3FF);
	REG_WR(ISP_BLC_B_PARAM_R_B, val);
	val = ((blc_info.gb & 0x3FF) << 10) | (blc_info.gr & 0x3FF);
	REG_WR(ISP_BLC_B_PARAM_G, val);

	REG_MWR(ISP_BLC_PARAM, BIT_0, blc_info.bypass);

	return ret;

}

int32_t isp_k_cfg_blc(struct isp_io_param *param)
{
	int32_t ret = 0;

	if (!param) {
		printk("isp_k_cfg_blc: param is null error.\n");
		return -1;
	}

	if (NULL == param->property_param) {
		printk("isp_k_cfg_blc: property_param is null error.\n");
		return -1;
	}

	switch(param->property) {
	case ISP_PRO_BLC_BLOCK:
		ret = isp_k_blc_block(param);
		break;
	default:
		printk("isp_k_cfg_blc: fail cmd id:%d, not supported.\n", param->property);
		break;
	}

	return ret;
}
