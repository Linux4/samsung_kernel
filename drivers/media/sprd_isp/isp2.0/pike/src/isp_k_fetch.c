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

static int32_t isp_k_fetch_block(struct isp_io_param *param)
{
	int32_t ret = 0;
	struct isp_dev_fetch_info_v1 fetch_info;

	memset(&fetch_info, 0x00, sizeof(fetch_info));

	ret = copy_from_user((void *)&fetch_info, param->property_param, sizeof(fetch_info));
	if (0 != ret) {
		printk("isp_k_fetch_block: copy_from_user error, ret = 0x%x\n", (uint32_t)ret);
		return -1;
	}

	if (fetch_info.substract) {
		REG_OWR(ISP_FETCH_PARAM, BIT_1);
	} else {
		REG_MWR(ISP_FETCH_PARAM, BIT_1, 0);
	}

	REG_MWR(ISP_FETCH_PARAM, 0xF0, (fetch_info.color_format << 4));

	REG_WR(ISP_FETCH_SLICE_Y_ADDR, fetch_info.addr.chn0);
	REG_WR(ISP_FETCH_SLICE_Y_PITCH, fetch_info.pitch.chn0);
	REG_WR(ISP_FETCH_SLICE_U_ADDR, fetch_info.addr.chn1);
	REG_WR(ISP_FETCH_SLICE_U_PITCH, fetch_info.pitch.chn1);
	REG_WR(ISP_FETCH_SLICE_V_ADDR, fetch_info.addr.chn2);
	REG_WR(ISP_FETCH_SLICE_V_PITCH, fetch_info.pitch.chn2);
	REG_WR(ISP_FETCH_MIPI_WORD_INFO, (fetch_info.mipi_word_num & 0xFFFF));
	REG_WR(ISP_FETCH_MIPI_BYTE_INFO, (fetch_info.mipi_byte_rel_pos & 0x0F));
	REG_WR(ISP_FETCH_BAYER_MODE, 0x2);

	if (fetch_info.bypass) {
		REG_OWR(ISP_FETCH_PARAM, BIT_0);
	} else {
		REG_MWR(ISP_FETCH_PARAM, BIT_0, 0);
	}

	return ret;
}

static int32_t isp_k_fetch_slice_size(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t size = 0;
	struct isp_img_size slice_size = {0, 0};

	ret = copy_from_user((void *)&slice_size, param->property_param, sizeof(struct isp_img_size));
	if (0 != ret) {
		printk("isp_k_fetch_slice_size: read copy_from_user error, ret = 0x%x\n", (uint32_t)ret);
		return -1;
	}

	size = ((slice_size.height & 0xFFFF) << 16) | (slice_size.width & 0xFFFF);

	REG_WR(ISP_FETCH_SLICE_SIZE, size);

	return ret;
}

static int32_t isp_k_fetch_start_isp(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t fetch_start = 0;

	ret = copy_from_user((void *)&fetch_start, param->property_param, sizeof(uint32_t));
	if (0 != ret) {
		printk("isp_k_fetch_start_isp: copy_from_user error, ret = 0x%x\n", (uint32_t)ret);
		return -1;
	}

	if (fetch_start) {
		REG_OWR(ISP_FETCH_START, BIT_0);
	} else {
		REG_MWR(ISP_FETCH_START, BIT_0, 0);
	}

	return ret;
}

int32_t isp_k_cfg_fetch(struct isp_io_param *param)
{
	int32_t ret = 0;

	if (!param) {
		printk("isp_k_cfg_fetch: param is null error.\n");
		return -1;
	}

	if (NULL == param->property_param) {
		printk("isp_k_cfg_fetch: property_param is null error.\n");
		return -1;
	}

	switch(param->property) {
	case ISP_PRO_FETCH_BLOCK:
		ret = isp_k_fetch_block(param);
		break;
	case ISP_PRO_FETCH_SLICE_SIZE:
		ret = isp_k_fetch_slice_size(param);
		break;
	case ISP_PRO_FETCH_START_ISP:
		ret = isp_k_fetch_start_isp(param);
		break;
	default:
		printk("isp_k_cfg_fetch: fail cmd id:%d, not supported.\n", param->property);
		break;
	}

	return ret;
}
