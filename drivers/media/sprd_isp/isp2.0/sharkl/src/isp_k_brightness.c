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

static int32_t isp_k_brightness_block(struct isp_io_param *param)
{
	int32_t ret = 0;
	struct isp_dev_brightness_info brightness_info;

	memset(&brightness_info, 0x00, sizeof(brightness_info));

	ret = copy_from_user((void *)&brightness_info, param->property_param, sizeof(brightness_info));
	if (0 != ret) {
		printk("isp_k_brightness_block: copy error, ret=0x%x\n", (uint32_t)ret);
		return -1;
	}

	REG_MWR(ISP_BRIGHT_PARAM, 0x1FE, ((brightness_info.factor & 0xFF) << 1));

	if (brightness_info.bypass) {
		REG_OWR(ISP_BRIGHT_PARAM, BIT_0);
	} else {
		REG_MWR(ISP_BRIGHT_PARAM, BIT_0, 0);
	}

	return ret;
}

static int32_t isp_k_brightness_slice_size(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t val = 0;
	struct isp_img_size size;

	ret = copy_from_user((void *)&size, param->property_param, sizeof(size));
	if (0 != ret) {
		printk("isp_k_brightness_slice_size: copy error, ret=0x%x\n", (uint32_t)ret);
		return -1;
	}

	val = ((size.height & 0xFFFF) << 16) | (size.width & 0xFFFF);
	REG_WR(ISP_BRIGHT_SLICE_SIZE, val);

	return ret;
}

static int32_t isp_k_brightness_slice_info(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t info = 0;

	ret = copy_from_user((void *)&info, param->property_param, sizeof(info));
	if (0 != ret) {
		printk("isp_k_brightness_slice_info: copy error, ret=0x%x\n", (uint32_t)ret);
		return -1;
	}

	REG_WR(ISP_BRIGHT_SLICE_INFO, (info & 0x0F));

	return ret;
}

int32_t isp_k_cfg_brightness(struct isp_io_param *param)
{
	int32_t ret = 0;

	if (!param) {
		printk("isp_k_cfg_brightness: param is null error.\n");
		return -1;
	}

	if (NULL == param->property_param) {
		printk("isp_k_cfg_brightness: property_param is null error.\n");
		return -1;
	}

	switch(param->property) {
	case ISP_PRO_BRIGHT_BLOCK:
		ret = isp_k_brightness_block(param);
		break;
	case ISP_PRO_BRIGHT_SLICE_SIZE:
		ret = isp_k_brightness_slice_size(param);
		break;
	case ISP_PRO_BRIGHT_SLICE_INFO:
		ret = isp_k_brightness_slice_info(param);
		break;
	default:
		printk("isp_k_cfg_brightness: fail cmd id:%d, not supported.\n", param->property);
		break;
	}

	return ret;
}
