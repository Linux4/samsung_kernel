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

static int32_t isp_k_posterize_block(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t val = 0;
	uint32_t i = 0;
	struct isp_dev_posterize_info pstrz_info;

	memset(&pstrz_info, 0x00, sizeof(pstrz_info));

	ret = copy_from_user((void *)&pstrz_info, param->property_param, sizeof(pstrz_info));
	if (0 != ret) {
		printk("isp_k_posterize_block: copy error, ret=0x%x\n", (uint32_t)ret);
		return -1;
	}

	REG_MWR(ISP_PSTRZ_PARAM, BIT_0, pstrz_info.bypass);

	for (i = 0; i < 8; i++) {
		val = (pstrz_info.posterize_level_out[i] & 0xFF) | ((pstrz_info.posterize_level_top[i] & 0xFF) << 8)
			| ((pstrz_info.posterize_level_bottom[i] & 0xFF) << 16);
		REG_WR(ISP_PSTRZ_LEVEL0 + i * 4, val);
	}

	return ret;

}

int32_t isp_k_cfg_posterize(struct isp_io_param *param)
{
	int32_t ret = 0;

	if (!param) {
		printk("isp_k_cfg_posterize: param is null error.\n");
		return -1;
	}

	if (NULL == param->property_param) {
		printk("isp_k_cfg_posterize: property_param is null error.\n");
		return -1;
	}

	switch(param->property) {
	case ISP_PRO_POSTERIZE_BLOCK:
		ret = isp_k_posterize_block(param);
		break;
	default:
		printk("isp_k_cfg_posterize: fail cmd id:%d, not supported.\n", param->property);
		break;
	}

	return ret;
}

