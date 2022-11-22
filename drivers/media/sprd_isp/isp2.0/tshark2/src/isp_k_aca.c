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

static int32_t isp_k_aca_block(struct isp_io_param *param)
{
	int32_t ret = 0;
	struct isp_dev_autocont_info_v1 autocont_info;
	uint32_t val = 0;

	memset(&autocont_info, 0x00, sizeof(autocont_info));

	ret = copy_from_user((void *)&autocont_info, param->property_param, sizeof(autocont_info));
	if (0 != ret) {
		printk("isp_k_aca_block: copy_from_user error, ret = 0x%x\n", (uint32_t)ret);
		return -1;
	}

	val = ((autocont_info.out_max & 0xFF) << 24)
			| ((autocont_info.out_min & 0xFF) << 16)
			| ((autocont_info.in_max & 0xFF) << 8)
			| (autocont_info.in_min & 0xFF);

	REG_WR(ISP_AUTOCONT_MAX_MIN, val);

	if (autocont_info.mode) {
		REG_OWR(ISP_AUTOCONT_PARAM, BIT_1);
	} else {
		REG_MWR(ISP_AUTOCONT_PARAM, BIT_1, 0);
	}

	if (autocont_info.bypass) {
		REG_OWR(ISP_AUTOCONT_PARAM, BIT_0);
	} else {
		REG_MWR(ISP_AUTOCONT_PARAM, BIT_0, 0);
	}

	return ret;
}

int32_t isp_k_cfg_aca(struct isp_io_param *param)
{
	int32_t ret = 0;

	if (!param) {
		printk("isp_k_cfg_aca: param is null error.\n");
		return -1;
	}

	if (NULL == param->property_param) {
		printk("isp_k_cfg_aca: property_param is null error.\n");
		return -1;
	}

	switch (param->property) {
	case ISP_PRO_ACA_BLOCK:
		ret = isp_k_aca_block(param);
		break;
	default:
		printk("isp_k_cfg_aca: fail cmd id:%d, not supported.\n", param->property);
		break;
	}

	return ret;
}
