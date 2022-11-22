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

static int32_t isp_k_yuv_precdn_block(struct isp_io_param *param)
{
	int32_t ret = 0;
	struct isp_dev_yuv_precdn_info pre_cdn_info;
	uint32_t val = 0;
	uint32_t i = 0;

	memset(&pre_cdn_info, 0x00, sizeof(pre_cdn_info));

	ret = copy_from_user((void *)&pre_cdn_info, param->property_param, sizeof(pre_cdn_info));
	if (0 != ret) {
		printk("isp_k_yuv_precdn_block: copy_from_user error, ret = 0x%x\n", (uint32_t)ret);
		return -1;
	}

	if (pre_cdn_info.mode) {
		REG_OWR(ISP_PRECDN_PARAM, BIT_4);
	} else {
		REG_MWR(ISP_PRECDN_PARAM, BIT_4, 0);
	}

	if (pre_cdn_info.median_writeback_en) {
		REG_OWR(ISP_PRECDN_PARAM, BIT_8);
	} else {
		REG_MWR(ISP_PRECDN_PARAM, BIT_8, 0);
	}

	REG_MWR(ISP_PRECDN_PARAM, BIT_13 | BIT_12, pre_cdn_info.median_mode << 12);

	if (pre_cdn_info.uv_joint) {
		REG_OWR(ISP_PRECDN_PARAM, BIT_20);
	} else {
		REG_MWR(ISP_PRECDN_PARAM, BIT_20, 0);
	}

	REG_MWR(ISP_PRECDN_PARAM, BIT_17 | BIT_16, pre_cdn_info.den_stren << 16);

	val = (pre_cdn_info.median_thr_u[0] & 0x7F) | ((pre_cdn_info.median_thr_u[1] & 0xFF) << 8)
		| ((pre_cdn_info.median_thr_v[0] & 0x7F) << 16) | ((pre_cdn_info.median_thr_v[1] & 0xFF) << 24);
	REG_WR(ISP_PRECDN_MEDIAN_THRUV01, val);

	val = (pre_cdn_info.median_thr & 0x1FF) | ((pre_cdn_info.uv_thr & 0xFF) << 16)
		| ((pre_cdn_info.y_thr & 0xFF) << 24);
	REG_WR(ISP_PRECDN_THRYUV, val);

	for (i = 0; i < 3; i++) {
		val = (pre_cdn_info.r_segu[0][i*2] & 0xFF) | ((pre_cdn_info.r_segu[1][i*2] & 0x7) << 8)
			| ((pre_cdn_info.r_segu[0][i*2+1] & 0xFF) << 16) | ((pre_cdn_info.r_segu[1][i*2+1] & 0x7) << 24);
		REG_WR(ISP_PRECDN_SEGU_0 + i * 4, val);
	}

	val = (pre_cdn_info.r_segu[0][6] & 0xFF) | ((pre_cdn_info.r_segu[1][6] & 0x7) << 8);
	REG_WR(ISP_PRECDN_SEGU_3, val);

	for (i = 0; i < 3; i++) {
		val = (pre_cdn_info.r_segv[0][i*2] & 0xFF) | ((pre_cdn_info.r_segv[1][i*2] & 0x7) << 8)
			| ((pre_cdn_info.r_segv[0][i*2+1] & 0xFF) << 16) | ((pre_cdn_info.r_segv[1][i*2+1] & 0x7) << 24);
		REG_WR(ISP_PRECDN_SEGV_0 + i * 4, val);
	}

	val = (pre_cdn_info.r_segv[0][6] & 0xFF) | ((pre_cdn_info.r_segv[1][6] & 0x7) << 8);
	REG_WR(ISP_PRECDN_SEGV_3, val);

	for (i = 0; i < 3; i++) {
		val = (pre_cdn_info.r_segy[0][i*2] & 0xFF) | ((pre_cdn_info.r_segy[1][i*2] & 0x7) << 8)
			| ((pre_cdn_info.r_segy[0][i*2+1] & 0xFF) << 16) | ((pre_cdn_info.r_segy[1][i*2+1] & 0x7) << 24);
		REG_WR(ISP_PRECDN_SEGY_0 + i * 4, val);
	}

	val = (pre_cdn_info.r_segy[0][6] & 0xFF) | ((pre_cdn_info.r_segy[1][6] & 0x7) << 8);
	REG_WR(ISP_PRECDN_SEGY_3, val);

	for (i = 0; i < 3; i++) {
		val = (pre_cdn_info.r_distw[i*8] & 0x7) | ((pre_cdn_info.r_distw[i*8+1] & 0x7) << 4)
			| ((pre_cdn_info.r_distw[i*8+2] & 0x7) << 8) | ((pre_cdn_info.r_distw[i*8+3] & 0x7) << 12)
			| ((pre_cdn_info.r_distw[i*8+4] & 0x7) << 16) | ((pre_cdn_info.r_distw[i*8+5] & 0x7) << 20)
			| ((pre_cdn_info.r_distw[i*8+6] & 0x7) << 24) | ((pre_cdn_info.r_distw[i*8+7] & 0x7) << 28);
		REG_WR(ISP_PRECDN_DISTW0 + i * 4, val);
	}

	val = pre_cdn_info.r_distw[24] & 0x7;
	REG_WR(ISP_PRECDN_DISTW3, val);

	if (pre_cdn_info.bypass) {
		REG_OWR(ISP_PRECDN_PARAM, BIT_0);
	} else {
		REG_MWR(ISP_PRECDN_PARAM, BIT_0, 0);
	}

	return ret;
}

int32_t isp_k_cfg_yuv_precdn(struct isp_io_param *param)
{
	int32_t ret = 0;

	if (!param) {
		printk("isp_k_cfg_yuv_precdn: param is null error.\n");
		return -1;
	}

	if (NULL == param->property_param) {
		printk("isp_k_cfg_yuv_precdn: property_param is null error.\n");
		return -1;
	}

	switch(param->property) {
	case ISP_PRO_YUV_PRECDN_BLOCK:
		ret = isp_k_yuv_precdn_block(param);
		break;
	default:
		printk("isp_k_cfg_yuv_precdn: fail cmd id:%d, not supported.\n", param->property);
		break;
	}

	return ret;
}
