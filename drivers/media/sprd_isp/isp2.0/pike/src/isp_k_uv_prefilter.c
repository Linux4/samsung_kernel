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

static int32_t isp_k_uv_prefilter_block(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t val = 0;
	struct isp_dev_uv_prefilter_info uv_info;

	memset(&uv_info, 0x00, sizeof(uv_info));

	ret = copy_from_user((void *)&uv_info, param->property_param, sizeof(uv_info));
	if (0 != ret) {
		printk("isp_k_uv_prefilter_block: copy error, ret = 0x%x \n", (uint32_t)ret);
		return -1;
	}

	REG_MWR(ISP_UV_PREF_PARAM, BIT_1, uv_info.writeback << 1);

	val = (uv_info.nr_thr_u & 0xFF) | ((uv_info.nr_thr_v & 0xFF) << 8);
	REG_WR(ISP_UV_PREF_THRD, val);

	REG_MWR(ISP_UV_PREF_PARAM, BIT_0, uv_info.bypass);

	return ret;
}

int32_t isp_k_cfg_uv_prefilter(struct isp_io_param *param)
{
	int32_t ret = 0;

	if (!param) {
		printk("isp_k_cfg_uv_prefilter: param is null error \n");
		return -1;
	}

	if (!param->property_param) {
		printk("isp_k_cfg_uv_prefilter: property_param is null error \n");
		return -1;
	}

	switch (param->property) {
	case ISP_PRO_UV_PREFILTER_BLOCK:
		ret = isp_k_uv_prefilter_block(param);
		break;
	default:
		printk("isp_k_cfg_uv_prefilter: fail cmd id %d, not supported \n", param->property);
		break;
	}

	return ret;
}

