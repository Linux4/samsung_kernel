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

static int32_t isp_k_prefilter_block(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t val = 0;
	struct isp_dev_prefilter_info prefilter_info;

	memset(&prefilter_info, 0x00, sizeof(prefilter_info));

	ret = copy_from_user((void *)&prefilter_info, param->property_param, sizeof(prefilter_info));
	if (0 != ret) {
		printk("isp_k_prefilter_block: copy error, ret=0x%x\n", (uint32_t)ret);
		return -1;
	}

	val= (prefilter_info.writeback & 0x03) << 1;
	REG_MWR(ISP_PREF_PARAM, (BIT_2 | BIT_1), val);

	val = ((prefilter_info.v_thrd & 0xFF) << 16)
		| ((prefilter_info.u_thrd & 0xFF) << 8)
		| (prefilter_info.y_thrd & 0xFF);
	REG_WR(ISP_PREF_THRD, (val & 0xFFFFFF));

	if (prefilter_info.bypass) {
		REG_OWR(ISP_PREF_PARAM, BIT_0);
	} else {
		REG_MWR(ISP_PREF_PARAM, BIT_0, 0);
	}

	return ret;
}

static int32_t isp_k_prefilter_bypass(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t bypass = 0;

	ret = copy_from_user((void *)&bypass, param->property_param, sizeof(bypass));
	if (0 != ret) {
		printk("isp_k_prefilter_bypass: copy error, ret=0x%x\n", (uint32_t)ret);
		return -1;
	}

	if (bypass) {
		REG_OWR(ISP_PREF_PARAM, BIT_0);
	} else {
		REG_MWR(ISP_PREF_PARAM, BIT_0, 0);
	}

	return ret;
}

static int32_t isp_k_prefilter_writeback(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t val = 0;

	ret = copy_from_user((void *)&val, param->property_param, sizeof(val));
	if (0 != ret) {
		printk("isp_k_prefilter_writeback: copy error, ret=0x%x\n", (uint32_t)ret);
		return -1;
	}

	REG_MWR(ISP_PREF_PARAM, (BIT_2 | BIT_1), ((val & 0x03) << 1));

	return ret;
}

static int32_t isp_k_prefilter_thrd(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t val = 0;
	struct isp_prefilter_thrd thrd;

	memset(&thrd, 0x00, sizeof(thrd));

	ret = copy_from_user((void *)&thrd, param->property_param, sizeof(thrd));
	if (0 != ret) {
		printk("isp_k_prefilter_thrd: copy error, ret=0x%x\n", (uint32_t)ret);
		return -1;
	}

	val = ((thrd.v_thrd & 0xFF) << 16)
		| ((thrd.u_thrd & 0xFF) << 8)
		| (thrd.y_thrd & 0xFF);

	REG_WR(ISP_PREF_THRD, (val & 0xFFFFFF));

	return ret;
}

static int32_t isp_k_prefilter_slice_size(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t val = 0;
	struct isp_img_size size;

	ret = copy_from_user((void *)&size, param->property_param, sizeof(size));
	if (0 != ret) {
		printk("isp_k_prefilter_slice_size: copy error, ret=0x%x\n", (uint32_t)ret);
		return -1;
	}

	val = ((size.height & 0xFFFF) << 16) | (size.width & 0xFFFF);
	REG_WR(ISP_PREF_SLICE_SIZE, val);

	return ret;
}

static int32_t isp_k_prefilter_slice_info(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t info = 0;

	ret = copy_from_user((void *)&info, param->property_param, sizeof(info));
	if (0 != ret) {
		printk("isp_k_prefilter_slice_info: copy error, ret=0x%x\n", (uint32_t)ret);
		return -1;
	}

	REG_WR(ISP_PREF_SLICE_INFO, info);

	return ret;
}

int32_t isp_k_cfg_prefilter(struct isp_io_param *param)
{
	int32_t ret = 0;

	if (!param) {
		printk("isp_k_cfg_prefilter: param is null error.\n");
		return -1;
	}

	if (NULL == param->property_param) {
		printk("isp_k_cfg_prefilter: property_param is null error.\n");
		return -1;
	}

	switch(param->property) {
	case ISP_PRO_PREF_BLOCK:
		ret = isp_k_prefilter_block(param);
		break;
	case ISP_PRO_PREF_BYPASS:
		ret = isp_k_prefilter_bypass(param);
		break;
	case ISP_PRO_PREF_WRITEBACK:
		ret = isp_k_prefilter_writeback(param);
		break;
	case ISP_PRO_PREF_THRD:
		ret = isp_k_prefilter_thrd(param);
		break;
	case ISP_PRO_PREF_SLICE_SIZE:
		ret = isp_k_prefilter_slice_size(param);
		break;
	case ISP_PRO_PREF_SLICE_INFO:
		ret = isp_k_prefilter_slice_info(param);
		break;
	default:
		printk("isp_k_cfg_prefilter: fail cmd id:%d, not supported.\n", param->property);
		break;
	}

	return ret;
}
