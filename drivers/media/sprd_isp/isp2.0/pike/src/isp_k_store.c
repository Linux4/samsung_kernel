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

static int32_t isp_k_store_block(struct isp_io_param *param)
{
	int32_t ret = 0;
	struct isp_dev_store_info_v1 store_info;

	memset(&store_info, 0x00, sizeof(store_info));

	ret = copy_from_user((void *)&store_info, param->property_param, sizeof(store_info));
	if (0 != ret) {
		printk("isp_k_store_block: copy_from_user error, ret = 0x%x\n", (uint32_t)ret);
		return -1;
	}

	if (store_info.substract) {
		REG_OWR(ISP_STORE_PARAM, BIT_1);
	} else {
		REG_MWR(ISP_STORE_PARAM, BIT_1, 0);
	}

	REG_MWR(ISP_STORE_PARAM, 0xF0, (store_info.color_format << 4));

	REG_WR(ISP_STORE_Y_ADDR, store_info.addr.chn0);
	REG_WR(ISP_STORE_Y_PITCH, store_info.pitch.chn0);
	REG_WR(ISP_STORE_U_ADDR, store_info.addr.chn1);
	REG_WR(ISP_STORE_U_PITCH, store_info.pitch.chn1);
	REG_WR(ISP_STORE_V_ADDR, store_info.addr.chn2);
	REG_WR(ISP_STORE_V_PITCH, store_info.pitch.chn2);

	if (store_info.bypass) {
		REG_OWR(ISP_STORE_PARAM, BIT_0);
	} else {
		REG_MWR(ISP_STORE_PARAM, BIT_0, 0);
	}

	return ret;
}

static int32_t isp_k_store_slice_size(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t val = 0;
	struct isp_img_size size = {0, 0};

	ret = copy_from_user((void *)&size, param->property_param, sizeof(size));
	if (0 != ret) {
		printk("isp_k_store_slice_size: copy_from_user error, ret = 0x%x\n", (uint32_t)ret);
		return -1;
	}

	val = ((size.height& 0xFFFF) << 16) | (size.width & 0xFFFF);
	REG_WR(ISP_STORE_SLICE_SIZE, val);

	return ret;
}

int32_t isp_k_cfg_store(struct isp_io_param *param)
{
	int32_t ret = 0;

	if (!param) {
		printk("isp_k_cfg_store: param is null error.\n");
		return -1;
	}

	if (NULL == param->property_param) {
		printk("isp_k_cfg_store: property_param is null error.\n");
		return -1;
	}

	switch (param->property) {
	case ISP_PRO_STORE_BLOCK:
		ret = isp_k_store_block(param);
		break;
	case ISP_PRO_STORE_SLICE_SIZE:
		ret = isp_k_store_slice_size(param);
		break;
	default:
		printk("isp_k_cfg_store: fail cmd id:%d, not supported.\n", param->property);
		break;
	}

	return ret;
}
