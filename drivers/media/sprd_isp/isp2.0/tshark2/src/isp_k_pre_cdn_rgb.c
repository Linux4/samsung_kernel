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

static int32_t isp_k_pre_cdn_rgb_block(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t val = 0;
	struct isp_dev_pre_cdn_rgb_info pcr_info;

	memset(&pcr_info, 0x00, sizeof(pcr_info));

	ret = copy_from_user((void *)&pcr_info, param->property_param, sizeof(pcr_info));
	if (0 != ret) {
		printk("isp_k_pre_cdn_rgb_block: copy error, ret=0x%x\n", (uint32_t)ret);
		return -1;
	}

	REG_MWR(ISP_PRECNRNEW_CFG, BIT_0, pcr_info.bypass);

	REG_MWR(ISP_PRECNRNEW_CFG, 0x3 << 1, pcr_info.median_mode << 1);

	val = pcr_info.median_thr & 0xFFFF;
	REG_WR(ISP_PRECNRNEW_MEDIAN_THR, val);
	val = (pcr_info.thru0 & 0xFFFF) | ((pcr_info.thru1 & 0xFFFF) << 16);
	REG_WR(ISP_PRECNRNEW_THRU, val);
	val = (pcr_info.thrv0 & 0xFFFF) | ((pcr_info.thrv1 & 0xFFFF) << 16);
	REG_WR(ISP_PRECNRNEW_THRV, val);

	return ret;

}

int32_t isp_k_cfg_pre_cdn_rgb(struct isp_io_param *param)
{
	int32_t ret = 0;

	if (!param) {
		printk("isp_k_cfg_pre_cdn_rgb: param is null error.\n");
		return -1;
	}

	if (NULL == param->property_param) {
		printk("isp_k_cfg_pre_cdn_rgb: property_param is null error.\n");
		return -1;
	}

	switch(param->property) {
	case ISP_PRO_PRE_CDN_RGB_BLOCK:
		ret = isp_k_pre_cdn_rgb_block(param);
		break;
	default:
		printk("isp_k_cfg_pre_cdn_rgb: fail cmd id:%d, not supported.\n", param->property);
		break;
	}

	return ret;
}

