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

static int32_t isp_k_arbiter_block(struct isp_io_param *param)
{
	int32_t ret = 0;
	struct isp_dev_arbiter_info_v1 arbiter_info;
	uint32_t val = 0;

	memset(&arbiter_info, 0x00, sizeof(arbiter_info));

	ret = copy_from_user((void *)&arbiter_info, param->property_param, sizeof(arbiter_info));
	if (0 != ret) {
		printk("isp_k_arbiter_block: copy_from_user error, ret = 0x%x\n", (uint32_t)ret);
		return -1;
	}

	val = ((arbiter_info.endian_ch0.bpc_endian & 0x3) << 8) | ((arbiter_info.endian_ch0.lens_endian & 0x3) << 6)
		| ((arbiter_info.endian_ch0.store_endian & 0x3) << 4) | ((arbiter_info.endian_ch0.fetch_bit_reorder & 0x1) << 2)
		| (arbiter_info.endian_ch0.fetch_endian & 0x3);
	REG_WR(ISP_ARBITER_ENDIAN_CH0, val);

	val = ((arbiter_info.endian_ch1.bpc_endian & 0x3) << 8) | ((arbiter_info.endian_ch1.lens_endian & 0x3) << 6)
		| ((arbiter_info.endian_ch1.store_endian & 0x3) << 4) | ((arbiter_info.endian_ch1.fetch_bit_reorder & 0x1) << 2)
		| (arbiter_info.endian_ch1.fetch_endian & 0x3);
	REG_WR(ISP_ARBITER_ENDIAN_CH1, val);

	val = (arbiter_info.para.pause_cycle & 0xFF) | ((arbiter_info.para.reset & 0x1) << 8);
	REG_WR(ISP_ARBITER_PARAM, val);

	return ret;
}

static int32_t isp_k_arbiter_wr_status(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t status = 0;

	status = REG_RD(ISP_PRO_ARBITER_WR_STATUS);

	ret = copy_to_user(param->property_param, (void*)&status, sizeof(status));
	if (0 != ret) {
		ret = -1;
		printk("isp_k_arbiter_wr_status: copy_to_user error, ret = 0x%x\n", (uint32_t)ret);
	}

	return ret;
}

static int32_t isp_k_arbiter_rd_status(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t status = 0;

	status = REG_RD(ISP_PRO_ARBITER_RD_STATUS);

	ret = copy_to_user(param->property_param, (void*)&status, sizeof(status));
	if (0 != ret) {
		ret = -1;
		printk("isp_k_arbiter_rd_status: copy_to_user error, ret = 0x%x\n", (uint32_t)ret);
	}

	return ret;
}

static int32_t isp_k_arbiter_endian_ch0(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t val = 0;
	struct arbiter_endian_v1 endian = {0};

	ret = copy_from_user((void *)&endian, param->property_param, sizeof(endian));
	if (0 != ret) {
		printk("isp_k_arbiter_endian_ch0: copy_from_user error, ret = 0x%x\n", (uint32_t)ret);
		return -1;
	}

	val = ((endian.bpc_endian & 0x3) << 8) | ((endian.lens_endian & 0x3) << 6) | ((endian.store_endian & 0x3) << 4)
		| ((endian.fetch_bit_reorder & 0x1) << 2) | (endian.fetch_endian & 0x3);
	REG_WR(ISP_ARBITER_ENDIAN_CH0, val);

	return ret;
}

static int32_t isp_k_arbiter_endian_ch1(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t val = 0;
	struct arbiter_endian_v1 endian = {0};

	ret = copy_from_user((void *)&endian, param->property_param, sizeof(endian));
	if (0 != ret) {
		printk("isp_k_arbiter_endian_ch1: copy_from_user error, ret = 0x%x\n", (uint32_t)ret);
		return -1;
	}

	val = ((endian.bpc_endian & 0x3) << 8) | ((endian.lens_endian & 0x3) << 6) | ((endian.store_endian & 0x3) << 4)
		| ((endian.fetch_bit_reorder & 0x1) << 2) | (endian.fetch_endian & 0x3);
	REG_WR(ISP_ARBITER_ENDIAN_CH1, val);

	return ret;
}

static int32_t isp_k_arbiter_param(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t val = 0;
	struct arbiter_param_v1 para = {0};

	ret = copy_from_user((void *)&para, param->property_param, sizeof(para));
	if (0 != ret) {
		printk("isp_k_arbiter_param: copy_from_user error, ret = 0x%x\n", (uint32_t)ret);
		return -1;
	}

	val = (para.pause_cycle & 0xFF) | ((para.reset & 0x1) << 8);
	REG_WR(ISP_ARBITER_PARAM, val);

	return ret;
}

int32_t isp_k_cfg_arbiter(struct isp_io_param *param)
{
	int32_t ret = 0;

	if (!param) {
		printk("isp_k_cfg_arbiter: param is null error.\n");
		return -1;
	}

	if (NULL == param->property_param) {
		printk("isp_k_cfg_arbiter: property_param is null error.\n");
		return -1;
	}

	switch (param->property) {
	case ISP_PRO_ARBITER_BLOCK:
		ret = isp_k_arbiter_block(param);
		break;
	case ISP_PRO_ARBITER_WR_STATUS:
		ret = isp_k_arbiter_wr_status(param);
		break;
	case ISP_PRO_ARBITER_RD_STATUS:
		ret = isp_k_arbiter_rd_status(param);
		break;
	case ISP_PRO_ARBITER_PARAM:
		ret = isp_k_arbiter_param(param);
		break;
	case ISP_PRO_ARBITER_ENDIAN_CH0:
		ret = isp_k_arbiter_endian_ch0(param);
		break;
	case ISP_PRO_ARBITER_ENDIAN_CH1:
		ret = isp_k_arbiter_endian_ch1(param);
		break;
	default:
		printk("isp_k_cfg_arbiter: fail cmd id:%d, not supported.\n", param->property);
		break;
	}

	return ret;
}

