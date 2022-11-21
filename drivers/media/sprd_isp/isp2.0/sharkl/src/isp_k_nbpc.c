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

static int32_t isp_k_nbpc_block(struct isp_io_param *param)
{
	int32_t ret = 0;
	struct isp_dev_nbpc_info nbpc_info;

	memset(&nbpc_info, 0x00, sizeof(nbpc_info));

	ret = copy_from_user((void *)&nbpc_info, param->property_param, sizeof(nbpc_info));
	if (0 != ret) {
		printk("isp_k_nbpc_block: copy error, ret=0x%x\n", (uint32_t)ret);
		return -1;
	}

	if (nbpc_info.bypass) {
		REG_OWR(ISP_NBPC_PARAM, BIT_0);
	} else {
		REG_MWR(ISP_NBPC_PARAM, BIT_0, 0);
	}

	return ret;
}

static int32_t isp_k_nbpc_bypass(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t bypass = 0;

	ret = copy_from_user((void *)&bypass, param->property_param, sizeof(bypass));
	if (0 != ret) {
		printk("isp_k_nbpc_bypass: copy error, ret=0x%x\n", (uint32_t)ret);
		return -1;
	}

	if (bypass) {
		REG_OWR(ISP_NBPC_PARAM, BIT_0);
	} else {
		REG_MWR(ISP_NBPC_PARAM, BIT_0, 0);
	}

	return ret;
}

int32_t isp_k_cfg_nbpc(struct isp_io_param *param)
{
	int32_t ret = 0;

	if (!param) {
		printk("isp_k_cfg_nbpc: param is null error.\n");
		return -1;
	}

	if (NULL == param->property_param) {
		printk("isp_k_cfg_nbpc: property_param is null error.\n");
		return -1;
	}

	switch(param->property) {
	case ISP_PRO_NBPC_BLOCK:
		ret = isp_k_nbpc_block(param);
		break;
	case ISP_PRO_NBPC_BYPASS:
		ret = isp_k_nbpc_bypass(param);
		break;
	default:
		printk("isp_k_cfg_nbpc: fail cmd id:%d, not supported.\n", param->property);
		break;
	}

	return ret;
}
