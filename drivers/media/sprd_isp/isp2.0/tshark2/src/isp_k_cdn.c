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

static int32_t isp_k_yuv_cdn_block(struct isp_io_param *param)
{
	int32_t ret = 0;
	struct isp_dev_yuv_cdn_info cdn_info;
	uint32_t val = 0;
	uint32_t i = 0;

	memset(&cdn_info, 0x00, sizeof(cdn_info));

	ret = copy_from_user((void *)&cdn_info, param->property_param, sizeof(cdn_info));
	if (0 != ret) {
		printk("isp_k_yuv_cdn_block: copy_from_user error, ret = 0x%x\n", (uint32_t)ret);
		return -1;
	}

	if (cdn_info.median_writeback_en) {
		REG_OWR(ISP_CDN_PARAM, BIT_2);
	} else {
		REG_MWR(ISP_CDN_PARAM, BIT_2, 0);
	}

	REG_MWR(ISP_CDN_PARAM, 0x38, cdn_info.median_mode << 3);

	REG_MWR(ISP_CDN_PARAM, BIT_7 | BIT_6, cdn_info.gaussian_mode << 6);

	REG_MWR(ISP_CDN_PARAM, 0x3FFF00, cdn_info.median_thr << 8);

	val = (cdn_info.median_thru0 & 0x7F) | ((cdn_info.median_thru1 & 0xFF) << 8)
		| ((cdn_info.median_thrv0 & 0x7F) << 16) | ((cdn_info.median_thrv1 & 0xFF) << 24);
	REG_WR(ISP_CDN_THRUV, val);

	for (i = 0; i < 7; i++) {
		val = (cdn_info.rangewu[i*4] & 0x3F) | ((cdn_info.rangewu[i*4+1] & 0x3F) << 8)
			| ((cdn_info.rangewu[i*4+2] & 0x3F) << 16) | ((cdn_info.rangewu[i*4+3] & 0x3F) << 24);
		REG_WR(ISP_CDN_U_RANWEI_0 + i * 4, val);
	}

	val = (cdn_info.rangewu[28] & 0x3F) | ((cdn_info.rangewu[29] & 0x3F) << 8)
		| ((cdn_info.rangewu[30] & 0x3F) << 16);
	REG_WR(ISP_CDN_U_RANWEI_7, val);

	for (i = 0; i < 7; i++) {
		val = (cdn_info.rangewv[i*4] & 0x3F) | ((cdn_info.rangewv[i*4+1] & 0x3F) << 8)
			| ((cdn_info.rangewv[i*4+2] & 0x3F) << 16) | ((cdn_info.rangewv[i*4+3] & 0x3F) << 24);
		REG_WR(ISP_CDN_V_RANWEI_0 + i * 4, val);
	}

	val = (cdn_info.rangewv[28] & 0x3F) | ((cdn_info.rangewv[29] & 0x3F) << 8)
		| ((cdn_info.rangewv[30] & 0x3F) << 16);
	REG_WR(ISP_CDN_V_RANWEI_7, val);

	if (cdn_info.filter_bypass) {
		REG_OWR(ISP_CDN_PARAM, BIT_1);
	} else {
		REG_MWR(ISP_CDN_PARAM, BIT_1, 0);
	}

	if (cdn_info.bypass) {
		REG_OWR(ISP_CDN_PARAM, BIT_0);
	} else {
		REG_MWR(ISP_CDN_PARAM, BIT_0, 0);
	}

	return ret;
}

int32_t isp_k_cfg_yuv_cdn(struct isp_io_param *param)
{
	int32_t ret = 0;

	if (!param) {
		printk("isp_k_cfg_yuv_cdn: param is null error.\n");
		return -1;
	}

	if (NULL == param->property_param) {
		printk("isp_k_cfg_yuv_cdn: property_param is null error.\n");
		return -1;
	}

	switch(param->property) {
	case ISP_PRO_YUV_CDN_BLOCK:
		ret = isp_k_yuv_cdn_block(param);
		break;
	default:
		printk("isp_k_cfg_yuv_cdn: fail cmd id:%d, not supported.\n", param->property);
		break;
	}

	return ret;
}
