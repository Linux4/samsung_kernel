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

static int32_t isp_k_dispatch_block(struct isp_io_param *param)
{
	int32_t ret = 0;
	struct isp_dev_dispatch_info_v1 dispatch_info;

	memset(&dispatch_info, 0x00, sizeof(dispatch_info));

	ret = copy_from_user((void *)&dispatch_info, param->property_param, sizeof(dispatch_info));
	if (0 != ret) {
		printk("isp_k_dispatch_block: copy_from_user error, ret = 0x%x\n", (uint32_t)ret);
		return -1;
	}

	REG_WR(ISP_DISPATCH_CH0_BAYER, dispatch_info.bayer_ch0 & 0x3);
	REG_WR(ISP_DISPATCH_CH1_BAYER, dispatch_info.bayer_ch1 & 0x3);

	return ret;
}

static int32_t isp_k_dispatch_ch0_bayer(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t val = 0;

	ret = copy_from_user((void *)&val, param->property_param, sizeof(uint32_t));
	if (0 != ret) {
		printk("isp_k_dispatch_ch0_bayer: copy_from_user error, ret = 0x%x\n", (uint32_t)ret);
		return -1;
	}

	REG_WR(ISP_DISPATCH_CH0_BAYER, val & 0x3);

	return ret;
}

static int32_t isp_k_dispatch_ch1_bayer(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t val = 0;

	ret = copy_from_user((void *)&val, param->property_param, sizeof(uint32_t));
	if (0 != ret) {
		printk("isp_k_dispatch_ch1_bayer: copy_from_user error, ret = 0x%x\n", (uint32_t)ret);
		return -1;
	}

	REG_WR(ISP_DISPATCH_CH1_BAYER, val & 0x3);

	return ret;
}

static int32_t isp_k_dispatch_ch0_size(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t val = 0;
	struct isp_img_size size = {0};

	ret = copy_from_user((void *)&size, param->property_param, sizeof(size));
	if (0 != ret) {
		printk("isp_k_dispatch_ch0_size: copy_from_user error, ret = 0x%x\n", (uint32_t)ret);
		return -1;
	}

	val = ((size.height & 0xFFFF) << 16) | (size.width & 0xFFFF);
	REG_WR(ISP_DISPATCH_CH0_SIZE, val);

	return ret;
}

static int32_t isp_k_dispatch_ch1_size(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t val = 0;
	struct isp_img_size size = {0};

	ret = copy_from_user((void *)&size, param->property_param, sizeof(size));
	if (0 != ret) {
		printk("isp_k_dispatch_ch1_size: copy_from_user error, ret = 0x%x\n", (uint32_t)ret);
		return -1;
	}

	val = ((size.height & 0xFFFF) << 16) | (size.width & 0xFFFF);
	REG_WR(ISP_DISPATCH_CH1_SIZE, val);

	return ret;
}

int32_t isp_k_cfg_dispatch(struct isp_io_param *param)
{
	int32_t ret = 0;

	if (!param) {
		printk("isp_k_cfg_dispatch: param is null error.\n");
		return -1;
	}

	if (NULL == param->property_param) {
		printk("isp_k_cfg_dispatch: property_param is null error.\n");
		return -1;
	}

	switch (param->property) {
	case ISP_PRO_DISPATCH_BLOCK:
		ret = isp_k_dispatch_block(param);
		break;
	case ISP_PRO_DISPATCH_CH0_BAYER:
		ret = isp_k_dispatch_ch0_bayer(param);
		break;
	case ISP_PRO_DISPATCH_CH1_BAYER:
		ret = isp_k_dispatch_ch1_bayer(param);
		break;
	case ISP_PRO_DISPATCH_CH0_SIZE:
		ret = isp_k_dispatch_ch0_size(param);
		break;
	case ISP_PRO_DISPATCH_CH1_SIZE:
		ret = isp_k_dispatch_ch1_size(param);
		break;
	default:
		printk("isp_k_cfg_dispatch: fail cmd id:%d, not supported.\n", param->property);
		break;
	}

	return ret;
}

