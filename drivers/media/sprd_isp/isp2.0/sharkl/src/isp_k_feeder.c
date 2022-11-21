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

static int32_t isp_k_feeder_block(struct isp_io_param *param)
{
	int32_t ret = 0;
	struct isp_dev_feeder_info feeder_info;

	memset(&feeder_info, 0x00, sizeof(feeder_info));

	ret = copy_from_user((void *)&feeder_info, param->property_param, sizeof(feeder_info));
	if (0 != ret) {
		printk("isp_k_feeder_block: copy error, ret=0x%x\n", (uint32_t)ret);
		return -1;
	}

	REG_MWR(ISP_FEEDER_PARAM, (BIT_3 | BIT_2 | BIT_1), (feeder_info.data_type << 1));

	return ret;
}

static int32_t isp_k_feeder_data_type(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t data_type = 0;

	ret = copy_from_user((void *)&data_type, param->property_param, sizeof(data_type));
	if (0 != ret) {
		printk("isp_k_feeder_data_type: copy error, ret=0x%x\n", (uint32_t)ret);
		return -1;
	}

	REG_MWR(ISP_FEEDER_PARAM, (BIT_3 | BIT_2 | BIT_1), (data_type << 1));

	return ret;
}

static int32_t isp_k_feeder_slice_size(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t val = 0;
	struct isp_img_size size = {0, 0};

	ret = copy_from_user((void *)&size, param->property_param, sizeof(size));
	if (0 != ret) {
		printk("isp_k_feeder_slice_size: copy error, ret=0x%x\n", (uint32_t)ret);
		return -1;
	}

	val = ((size.height& 0xFFFF) << 16) | (size.width & 0xFFFF);
	REG_WR(ISP_FEEDER_SLICE_SIZE, val);

	return ret;
}

int32_t isp_k_cfg_feeder(struct isp_io_param *param)
{
	int32_t ret = 0;

	if (!param) {
		printk("isp_k_cfg_feeder: param is null error.\n");
		return -1;
	}

	if (NULL == param->property_param) {
		printk("isp_k_cfg_feeder: property_param is null error.\n");
		return -1;
	}

	switch (param->property) {
	case ISP_PRO_FEEDER_BLOCK:
		ret = isp_k_feeder_block(param);
		break;
	case ISP_PRO_FEEDER_DATA_TYPE:
		ret = isp_k_feeder_data_type(param);
		break;
	case ISP_PRO_FEEDER_SLICE_SIZE:
		ret = isp_k_feeder_slice_size(param);
		break;
	default:
		printk("isp_k_cfg_feeder: fail cmd id:%d, not supported.\n", param->property);
		break;
	}

	return ret;
}
