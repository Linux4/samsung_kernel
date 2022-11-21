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
	uint32_t val = 0;
	struct isp_dev_aca_info aca_info;

	ret = copy_from_user((void *)&aca_info, param->property_param, sizeof(aca_info));
	if (0 != ret) {
		printk("isp_k_aca_block: copy error, ret=0x%x\n", (uint32_t)ret);
		return -1;
	}

	if (aca_info.mode) {
		REG_OWR(ISP_AUTOCONT_PARAM, BIT_1);
	} else {
		REG_MWR(ISP_AUTOCONT_PARAM, BIT_1, 0);
	}

	val = ((aca_info.out_max & 0xFF) << 24)
		| ((aca_info.out_min & 0xFF) << 16)
		| ((aca_info.in_max & 0xFF) << 8)
		| (aca_info.in_min & 0xFF);
	REG_WR(ISP_AUTOCONT_MAX_MIN, val);


	if (aca_info.bypass) {
		REG_OWR(ISP_AUTOCONT_PARAM, BIT_0);
	} else {
		REG_MWR(ISP_AUTOCONT_PARAM, BIT_0, 0);
	}

	return ret;
}

static int32_t isp_k_aca_bypass(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t bypass = 0;

	ret = copy_from_user((void *)&bypass, param->property_param, sizeof(bypass));
	if (0 != ret) {
		printk("isp_k_aca_bypass: copy error, ret=0x%x\n", (uint32_t)ret);
		return -1;
	}

	if (bypass) {
		REG_OWR(ISP_AUTOCONT_PARAM, BIT_0);
	} else {
		REG_MWR(ISP_AUTOCONT_PARAM, BIT_0, 0);
	}

	return ret;
}

static int32_t isp_k_aca_mode(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t mode = 0;

	ret = copy_from_user((void *)&mode, param->property_param, sizeof(mode));
	if (0 != ret) {
		printk("isp_k_aca_mode: copy error, ret=0x%x\n", (uint32_t)ret);
		return -1;
	}

	if (mode) {
		REG_OWR(ISP_AUTOCONT_PARAM, BIT_1);
	} else {
		REG_MWR(ISP_AUTOCONT_PARAM, BIT_1, 0);
	}

	return ret;
}

static int32_t isp_k_aca_maxmin(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t val = 0;
	struct isp_aca_maxmin maxmin;

	memset(&maxmin, 0, sizeof(maxmin));

	ret = copy_from_user((void *)&maxmin, param->property_param, sizeof(maxmin));
	if (0 != ret) {
		printk("isp_k_aca_maxmin: copy error, ret=0x%x\n", (uint32_t)ret);
		return -1;
	}

	val = ((maxmin.out_max & 0xFF) << 24)
		| ((maxmin.out_min & 0xFF) << 16)
		| ((maxmin.in_max & 0xFF) << 8)
		| (maxmin.in_min & 0xFF);
	REG_WR(ISP_AUTOCONT_MAX_MIN, val);

	return ret;
}

static int32_t isp_k_aca_adjust(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t val = 0;
	struct isp_aca_adjust adjust;

	memset(&adjust, 0, sizeof(adjust));

	ret = copy_from_user((void *)&adjust, param->property_param, sizeof(adjust));
	if (0 != ret) {
		printk("isp_k_aca_adjust: copy error, ret=0x%x\n", (uint32_t)ret);
		return -1;
	}

	val = ((adjust.big & 0xFF) << 16)
		| ((adjust.small & 0xFF) << 8)
		| (adjust.diff & 0xFF);
	REG_WR(ISP_AUTOCONT_ADJUST, val);

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
	case ISP_PRO_ACA_BYPASS:
		ret = isp_k_aca_bypass(param);
		break;
	case ISP_PRO_ACA_MODE:
		ret = isp_k_aca_mode(param);
		break;
	case ISP_PRO_ACA_MAXMIN:
		ret = isp_k_aca_maxmin(param);
		break;
	case ISP_PRO_ACA_ADJUST:
		ret = isp_k_aca_adjust(param);
		break;
	default:
		printk("isp_k_cfg_aca: fail cmd id:%d, not supported.\n", param->property);
		break;
	}

	return ret;
}
