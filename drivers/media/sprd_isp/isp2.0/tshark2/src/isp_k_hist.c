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
	struct isp_dev_hist_info_v1 hist_info;
	uint32_t val = 0;

	memset(&hist_info, 0x00, sizeof(hist_info));

	ret = copy_from_user((void *)&hist_info, param->property_param, sizeof(hist_info));
	if (0 != ret) {
		printk("isp_k_hist_block: copy_from_user error, ret = 0x%x\n", (uint32_t)ret);
		return -1;
	}

	if (hist_info.buf_rst_en) {
		REG_OWR(ISP_HIST_PARAM, BIT_1);
	} else {
		REG_MWR(ISP_HIST_PARAM, BIT_1, 0);
	}

	if (hist_info.pof_rst_en) {
		REG_OWR(ISP_HIST_PARAM, BIT_9);
	} else {
		REG_MWR(ISP_HIST_PARAM, BIT_9, 0);
	}

	if (hist_info.skip_num_clr) {
		REG_OWR(ISP_HIST_PARAM, BIT_8);
	} else {
		REG_MWR(ISP_HIST_PARAM, BIT_8, 0);
	}

	REG_MWR(ISP_HIST_PARAM, 0xF0, hist_info.skip_num << 4);

	if (hist_info.mode) {
		REG_OWR(ISP_HIST_PARAM, BIT_3);
	} else {
		REG_MWR(ISP_HIST_PARAM, BIT_3, 0);
	}

	val = ((hist_info.high_ratio & 0xFFFF) << 16)
			| (hist_info.low_ratio & 0xFFFF);

	REG_WR(ISP_HIST_RATIO, val);

	val = (hist_info.dif_adj & 0xFF) | ((hist_info.small_adj & 0xFF) << 8)
		| ((hist_info.big_adj & 0xFF) << 16);

	REG_WR(ISP_HIST_ADJUST, val);

	if (hist_info.off) {
		REG_OWR(ISP_HIST_PARAM, BIT_2);
	} else {
		REG_MWR(ISP_HIST_PARAM, BIT_2, 0);
	}

	if (hist_info.bypass) {
		REG_OWR(ISP_HIST_PARAM, BIT_0);
	} else {
		REG_MWR(ISP_HIST_PARAM, BIT_0, 0);
	}

	return ret;
}

static int32_t isp_k_hist_slice_size(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t val = 0;
	struct isp_img_size size = {0};

	ret = copy_from_user((void *)&size, param->property_param, sizeof(size));
	if (0 != ret) {
		printk("isp_k_hist_slice_size: read copy_from_user error, ret = 0x%x\n", (uint32_t)ret);
		return -1;
	}

	val = ((size.height & 0xFFFF) << 16)
			| (size.width & 0xFFFF);

	REG_WR(ISP_HIST_SLICE_SIZE, val);

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
	case ISP_PRO_HIST_SLICE_SIZE:
		ret = isp_k_hist_slice_size(param);
		break;
	default:
		printk("isp_k_cfg_hist: fail cmd id:%d, not supported.\n", param->property);
		break;
	}

	return ret;
}
