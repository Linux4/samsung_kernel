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

static int32_t isp_k_edge_block(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t val = 0;
	struct isp_dev_edge_info edge_info;

	memset(&edge_info, 0x00, sizeof(edge_info));

	ret = copy_from_user((void *)&edge_info, param->property_param, sizeof(edge_info));
	if (0 != ret) {
		printk("isp_k_edge_block: copy error, ret=0x%x\n", (uint32_t)ret);
		return -1;
	}

	val = ((edge_info.strength & 0x3F) << 17)
		| ((edge_info.smooth_thrd & 0xFF) << 9)
		| ((edge_info.detail_thrd & 0xFF) << 1);
	REG_MWR(ISP_EDGE_PARAM, 0x7FFFFE, val);

	if (edge_info.bypass) {
		REG_OWR(ISP_EDGE_PARAM, BIT_0);
	} else {
		REG_MWR(ISP_EDGE_PARAM, BIT_0, 0);
	}

	return ret;
}

static int32_t isp_k_edge_bypass(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t bypass = 0;

	ret = copy_from_user((void *)&bypass, param->property_param, sizeof(bypass));
	if (0 != ret) {
		printk("isp_k_edge_bypass: copy error, ret=0x%x\n", (uint32_t)ret);
		return -1;
	}

	if (bypass) {
		REG_OWR(ISP_EDGE_PARAM, BIT_0);
	} else {
		REG_MWR(ISP_EDGE_PARAM, BIT_0, 0);
	}

	return ret;
}

static int32_t isp_k_edge_param(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t val = 0;
	struct isp_edge_thrd edge_thrd;

	memset(&edge_thrd, 0x00, sizeof(edge_thrd));

	ret = copy_from_user((void *)&edge_thrd, param->property_param, sizeof(edge_thrd));
	if (0 != ret) {
		printk("isp_k_edge_param: copy error, ret=0x%x\n", (uint32_t)ret);
		return -1;
	}

	val = ((edge_thrd.strength & 0x3F) << 17)
		| ((edge_thrd.smooth & 0xFF) << 9)
		| ((edge_thrd.detail & 0xFF) << 1);
	REG_MWR(ISP_EDGE_PARAM, 0x7FFFFE, val);

	return ret;
}

int32_t isp_k_cfg_edge(struct isp_io_param *param)
{
	int32_t ret = 0;

	if (!param) {
		printk("isp_k_cfg_edge: param is null error.\n");
		return -1;
	}

	if (NULL == param->property_param) {
		printk("isp_k_cfg_edge: property_param is null error.\n");
		return -1;
	}

	switch (param->property) {
	case ISP_PRO_EDGE_BLOCK:
		ret = isp_k_edge_block(param);
		break;
	case ISP_PRO_EDGE_BYPASS:
		ret = isp_k_edge_bypass(param);
		break;
	case ISP_PRO_EDGE_PARAM:
		ret = isp_k_edge_param(param);
		break;
	default:
		printk("isp_k_cfg_edge: fail cmd id:%d, not supported.\n", param->property);
		break;
	}

	return ret;
}
