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

static int32_t isp_k_grgb_block(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t val = 0;
	struct isp_dev_grgb_info_v1 grgb_info;

	memset(&grgb_info, 0x00, sizeof(grgb_info));

	ret = copy_from_user((void *)&grgb_info, param->property_param, sizeof(grgb_info));
	if (0 != ret) {
		printk("isp_k_grgb_block: copy error, ret=0x%x\n", (uint32_t)ret);
		return -1;
	}

	REG_MWR(ISP_GRGB_PARAM, BIT_0, grgb_info.bypass);

	val = ((grgb_info.diff & 0x3FF) << 7) | ((grgb_info.edge & 0x3F) << 1);
	REG_MWR(ISP_GRGB_PARAM, 0x1FFFE, val);
	val = grgb_info.grid & 0xFFF;
	REG_MWR(ISP_GRGB_GRID, 0xFFF, val);

	return ret;

}

int32_t isp_k_cfg_grgb(struct isp_io_param *param)
{
	int32_t ret = 0;

	if (!param) {
		printk("isp_k_cfg_grgb: param is null error.\n");
		return -1;
	}

	if (NULL == param->property_param) {
		printk("isp_k_cfg_grgb: property_param is null error.\n");
		return -1;
	}

	switch(param->property) {
	case ISP_PRO_GRGB_BLOCK:
		ret = isp_k_grgb_block(param);
		break;
	default:
		printk("isp_k_cfg_grgb: fail cmd id:%d, not supported.\n", param->property);
		break;
	}

	return ret;
}

