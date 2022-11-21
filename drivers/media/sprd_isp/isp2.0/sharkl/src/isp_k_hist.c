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

static int32_t isp_k_hist_block(struct isp_io_param *param)
{
	int32_t ret = 0;
	struct isp_dev_hist_info hist_info ;

	memset(&hist_info, 0x00, sizeof(hist_info));

	ret = copy_from_user((void *)&hist_info, param->property_param, sizeof(hist_info));
	if (0 != ret) {
		printk("isp_k_hist_block: copy error, ret=0x%x\n", (uint32_t)ret);
		return -1;
	}

	if (hist_info.bypass) {
		REG_OWR(ISP_HIST_PARAM, BIT_0);
	} else {
		REG_MWR(ISP_HIST_PARAM, BIT_0, 0);
	}

	return ret;
}

static int32_t isp_k_hist_bypass(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t bypass = 0;

	ret = copy_from_user((void *)&bypass, param->property_param, sizeof(bypass));
	if (0 != ret) {
		printk("isp_k_hist_bypass: copy error, ret=0x%x\n", (uint32_t)ret);
		return -1;
	}

	if (bypass) {
		REG_OWR(ISP_HIST_PARAM, BIT_0);
	} else {
		REG_MWR(ISP_HIST_PARAM, BIT_0, 0);
	}

	return ret;
}

static int32_t isp_k_hist_auto_rst_disable(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t off = 0;

	ret = copy_from_user((void *)&off, param->property_param, sizeof(off));
	if (0 != ret) {
		printk("isp_k_hist_auto_rst_disable: copy error, ret=0x%x\n", (uint32_t)ret);
		return -1;
	}

	if (off) {
		REG_OWR(ISP_HIST_PARAM, BIT_1);
	} else {
		REG_MWR(ISP_HIST_PARAM, BIT_1, 0);
	}


	return ret;
}

static int32_t isp_k_hist_mode(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t mode = 0;

	ret = copy_from_user((void *)&mode, param->property_param, sizeof(mode));
	if (0 != ret) {
		printk("isp_k_hist_mode: copy error, ret=0x%x\n", (uint32_t)ret);
		return -1;
	}

	if (mode) {
		REG_OWR(ISP_HIST_PARAM, BIT_2);
	} else {
		REG_MWR(ISP_HIST_PARAM, BIT_2, 0);
	}

	return ret;
}

static int32_t isp_k_hist_ratio(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t val = 0;
	struct isp_hist_ratio ratio;

	memset(&ratio, 0, sizeof(ratio));

	ret = copy_from_user((void *)&ratio, param->property_param, sizeof(ratio));
	if (0 != ret) {
		printk("isp_k_hist_ratio: copy error, ret=0x%x\n", (uint32_t)ret);
		return -1;
	}

	val = ((ratio.high_ratio & 0xFFFF) << 16)
		| (ratio.low_ratio & 0xFFFF);

	REG_WR(ISP_HIST_RATIO, val);

	return ret;
}

static int32_t isp_k_hist_maxmin(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t val = 0;
	struct isp_hist_maxmin maxmin;

	memset(&maxmin, 0, sizeof(maxmin));

	ret = copy_from_user((void *)&maxmin, param->property_param, sizeof(maxmin));
	if (0 != ret) {
		printk("isp_k_hist_maxmin: copy error, ret=0x%x\n", (uint32_t)ret);
		return -1;
	}

	val = ((maxmin.out_max & 0xFF) << 24)
		| ((maxmin.out_min & 0xFF) << 16)
		| ((maxmin.in_max & 0xFF) << 8)
		| (maxmin.in_min & 0xFF);

	REG_WR(ISP_HIST_MAX_MIN, val);

	return ret;
}

static int32_t isp_k_hist_clear_eb(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t eb = 0;

	ret = copy_from_user((void *)&eb, param->property_param, sizeof(eb));
	if (0 != ret) {
		printk("isp_k_hist_clear_eb: copy error, ret=0x%x\n", (uint32_t)ret);
		return -1;
	}

	if (eb) {
		REG_OWR(ISP_HIST_CLEAR_EB, BIT_0);
	} else {
		REG_MWR(ISP_HIST_CLEAR_EB, BIT_0, 0);
	}

	return ret;
}

static int32_t isp_k_hist_statistic(struct isp_io_param *param)
{
	int32_t ret = 0;
	int i = 0;
	int max_item = ISP_HIST_ITEM;
	uint32_t item[max_item];

	memset(item, 0, sizeof(item));
	for (i = 0; i < max_item; i++) {
		item[i] = REG_RD((ISP_HIST_OUTPUT + 4 * i));
	}
	ret = copy_to_user(param->property_param, (void*)&item, sizeof(item));
	if (0 != ret) {
		ret = -1;
		printk("isp_k_hist_statistic: copy error, ret=0x%x\n", (uint32_t)ret);
	}


	return ret;
}

static int32_t isp_k_hist_statistic_num(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t num = ISP_HIST_ITEM;

	ret = copy_to_user(param->property_param, (void*)&num, sizeof(num));
	if (0 != ret) {
		ret = -1;
		printk("isp_k_hist_statistic_num: copy error, ret=0x%x\n", (uint32_t)ret);
	}

	return ret;
}

int32_t isp_k_cfg_hist(struct isp_io_param *param)
{
	int32_t ret = 0;

	if (!param) {
		printk("isp_k_cfg_hist: param is null error.\n");
		return -1;
	}

	if (NULL == param->property_param) {
		printk("isp_k_cfg_hist: property_param is null error.\n");
		return -1;
	}

	switch (param->property) {
	case ISP_PRO_HIST_BLOCK:
		ret = isp_k_hist_block(param);
		break;
	case ISP_PRO_HIST_BYPASS:
		ret = isp_k_hist_bypass(param);
		break;
	case ISP_PRO_HIST_AUTO_RST_DISABLE:
		ret = isp_k_hist_auto_rst_disable(param);
		break;
	case ISP_PRO_HIST_MODE:
		ret = isp_k_hist_mode(param);
		break;
	case ISP_PRO_HIST_RATIO:
		ret = isp_k_hist_ratio(param);
		break;
	case ISP_PRO_HIST_MAXMIN:
		ret = isp_k_hist_maxmin(param);
		break;
	case ISP_PRO_HIST_CLEAR_EB:
		ret = isp_k_hist_clear_eb(param);
		break;
	case ISP_PRO_HIST_STATISTIC:
		ret = isp_k_hist_statistic(param);
		break;
	case ISP_PRO_HIST_STATISTIC_NUM:
		ret = isp_k_hist_statistic_num(param);
		break;
	default:
		printk("isp_k_cfg_hist: fail cmd id:%d, not supported.\n", param->property);
		break;
	}

	return ret;
}
