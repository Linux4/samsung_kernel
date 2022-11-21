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

#include <linux/vmalloc.h>
#include <linux/uaccess.h>
#include <linux/sprd_mm.h>
#include <video/sprd_isp.h>
#include "isp_reg.h"
#include "isp_drv.h"

static int32_t isp_k_awb_block(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t val = 0;
	struct isp_dev_awb_info awb_info;

	memset(&awb_info, 0x00, sizeof(awb_info));

	ret = copy_from_user((void *)&awb_info, param->property_param, sizeof(awb_info));
	if (0 != ret) {
		printk("isp_k_awbm_bypass: copy error, ret=0x%x\n", (uint32_t)ret);
		return -1;
	}

	/*AWBM*/
	if (awb_info.mode) {
		REG_OWR(ISP_AWBM_PARAM, BIT_1);
	} else {
		REG_MWR(ISP_AWBM_PARAM, BIT_1, 0);
	}

	REG_MWR(ISP_AWBM_PARAM, (BIT_5 | BIT_4 | BIT_3 | BIT_2), (awb_info.skip_num << 2));

	val = ((awb_info.offset_y & 0xFFFF) << 16) | (awb_info.offset_x & 0xFFFF);
	REG_WR(ISP_AWBM_BLOCK_OFFSET, val);

	val = ((awb_info.win_h & 0x1FF) << 9) | (awb_info.win_w & 0x1FF);
	REG_MWR(ISP_AWBM_BLOCK_SIZE, 0x1FFFF, val);

	REG_MWR(ISP_AWBM_BLOCK_SIZE, (0x1F << 18), (awb_info.shift << 18));

	if (awb_info.awbm_bypass) {
		REG_OWR(ISP_AWBM_PARAM, BIT_0);
	} else {
		REG_MWR(ISP_AWBM_PARAM, BIT_0, 0);
	}

	/*AWBC*/
	val = ((awb_info.b_gain & 0xFFFF) << 16) | (awb_info.r_gain& 0xFFFF);
	REG_WR(ISP_AWBC_GAIN0, val);

	val = ((awb_info.g_gain & 0xFFFF) << 16) | (awb_info.g_gain & 0xFFFF);
	REG_WR(ISP_AWBC_GAIN1, val);

	val = ((awb_info.b_thrd& 0x3FF) << 20) | ((awb_info.g_thrd & 0x3FF) << 10) | (awb_info.r_thrd & 0x3FF);
	REG_WR(ISP_AWBC_THRD, val);

	val = ((awb_info.b_offset & 0xFFFF) << 16) | (awb_info.r_offset & 0xFFFF);
	REG_WR(ISP_AWBC_OFFSET0, val);

	val = ((awb_info.g_offset & 0xFFFF) << 16) | (awb_info.g_offset & 0xFFFF);
	REG_WR(ISP_AWBC_OFFSET1, val);

	if (awb_info.awbc_bypass) {
		REG_OWR(ISP_AWBC_PARAM, BIT_0);
	} else {
		REG_MWR(ISP_AWBC_PARAM, BIT_0, 0);
	}

	return ret;
}


static int32_t isp_k_awbm_statistics(struct isp_io_param *param)
{
	int32_t ret = 0, i = 0;
	uint32_t val0 = 0, val1 = 0, addr = 0;
	struct isp_awbm_statistics *awbm_statistics = NULL;

	awbm_statistics = vzalloc(sizeof(struct isp_awbm_statistics));/*To be optimized later*/
	if (!awbm_statistics) {
		ret = -1;
		printk("isp_k_awbm_info: alloc memory error.\n");
		return -1;
	}

	addr = ISP_AWBM_OUTPUT;
	for (i = 0x00; i < ISP_AWBM_ITEM; i++) {
		val0 = REG_RD(addr);
		val1 = REG_RD(addr + 4);
		awbm_statistics->r[i]= (val1 >> 11) & 0x1fffff;
		awbm_statistics->g[i]= val0 & 0x3fffff;
		awbm_statistics->b[i]= ((val1 & 0x7ff) << 10) | ((val0 >> 22) & 0x3ff);
		addr += 8;
	}

	ret = copy_to_user(param->property_param, (void*)awbm_statistics, sizeof(struct isp_awbm_statistics));
	if (0 != ret) {
		ret = -1;
		printk("isp_k_awbm_info: copy error, ret=0x%x\n", (uint32_t)ret);
	}

	vfree(awbm_statistics);

	return ret;
}

static int32_t isp_k_awbm_bypass(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t bypass = 0;

	ret = copy_from_user((void *)&bypass, param->property_param, sizeof(bypass));
	if (0 != ret) {
		printk("isp_k_awbm_bypass: copy error, ret=0x%x\n", (uint32_t)ret);
		return -1;
	}

	if (bypass) {
		REG_OWR(ISP_AWBM_PARAM, BIT_0);
	} else {
		REG_MWR(ISP_AWBM_PARAM, BIT_0, 0);
	}

	return ret;
}

static int32_t isp_k_awbm_mode(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t mode = 0;

	ret = copy_from_user((void *)&mode, param->property_param, sizeof(mode));
	if (0 != ret) {
		printk("isp_k_awbm_mode: copy error, ret=0x%x\n", (uint32_t)ret);
		return -1;
	}

	if (mode) {
		REG_OWR(ISP_AWBM_PARAM, BIT_1);
	} else {
		REG_MWR(ISP_AWBM_PARAM, BIT_1, 0);
	}

	return ret;
}

static int32_t isp_k_awbm_skip_num(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t val = 0;
	uint32_t num = 0;

	ret = copy_from_user((void *)&num, param->property_param, sizeof(num));
	if (0 != ret) {
		printk("isp_k_awbm_skip_num: copy error, ret=0x%x\n", (uint32_t)ret);
		return -1;
	}

	val = (num & 0x0F) << 2;
	REG_MWR(ISP_AWBM_PARAM, (BIT_5 | BIT_4 | BIT_3 | BIT_2), val);

	return ret;
}

static int32_t isp_k_awbm_block_offset(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t val = 0;
	struct isp_img_offset offset = {0, 0};

	ret = copy_from_user((void *)&offset, param->property_param, sizeof(offset));
	if (0 != ret) {
		printk("isp_k_awbm_block_offset: copy error, ret=0x%x\n", (uint32_t)ret);
		return -1;
	}

	val = ((offset.y & 0xFFFF) << 16) | (offset.x & 0xFFFF);

	REG_WR(ISP_AWBM_BLOCK_OFFSET, val);

	return ret;
}

static int32_t isp_k_awbm_block_size(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t val = 0;
	struct isp_img_size size = {0, 0};

	ret = copy_from_user((void *)&size, param->property_param, sizeof(size));
	if (0 != ret) {
		printk("isp_k_awbm_block_size: copy error, ret=0x%x\n", (uint32_t)ret);
		return -1;
	}

	val = ((size.height & 0x1FF) << 9) | (size.width & 0x1FF);

	REG_MWR(ISP_AWBM_BLOCK_SIZE, 0x1FFFF, val);

	return ret;
}

static int32_t isp_k_awbm_shift(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t shift = 0;

	ret = copy_from_user((void *)&shift, param->property_param, sizeof(shift));
	if (0 != ret) {
		printk("isp_k_awbm_shift: copy error, ret=0x%x\n", (uint32_t)ret);
		return -1;
	}

	shift = (shift & 0x1F) << 18;

	REG_MWR(ISP_AWBM_BLOCK_SIZE, (0x1F << 18), shift);

	return ret;
}

static int32_t isp_k_awbc_bypass(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t bypass = 0;

	ret = copy_from_user((void *)&bypass, param->property_param, sizeof(bypass));
	if (0 != ret) {
		printk("isp_k_awbc_bypass: copy error, ret=0x%x\n", (uint32_t)ret);
		return -1;
	}

	if (bypass) {
		REG_OWR(ISP_AWBC_PARAM, BIT_0);
	} else {
		REG_MWR(ISP_AWBC_PARAM, BIT_0, 0);
	}

	return ret;
}

static int32_t isp_k_awbc_gain(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t val = 0;
	struct isp_awbc_rgb gain= {0, 0, 0};

	ret = copy_from_user((void *)&gain, param->property_param, sizeof(gain));
	if (0 != ret) {
		printk("isp_k_awbc_gain: copy error, ret=0x%x\n", (uint32_t)ret);
		return -1;
	}

	val = ((gain.b & 0xFFFF) << 16) | (gain.r & 0xFFFF);
	REG_WR(ISP_AWBC_GAIN0, val);

	val = ((gain.g & 0xFFFF) << 16) | (gain.g & 0xFFFF);
	REG_WR(ISP_AWBC_GAIN1, val);

	return ret;
}

static int32_t isp_k_awbc_thrd(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t val = 0;
	struct isp_awbc_rgb thrd= {0, 0, 0};

	ret = copy_from_user((void *)&thrd, param->property_param, sizeof(thrd));
	if (0 != ret) {
		printk("isp_k_awbc_thrd: copy error, ret=0x%x\n", (uint32_t)ret);
		return -1;
	}

	val = ((thrd.b & 0x3FF) << 20) | ((thrd.g & 0x3FF) << 10) | (thrd.r & 0x3FF);
	REG_WR(ISP_AWBC_THRD, val);

	return ret;
}

static int32_t isp_k_awbc_gain_offset(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t val = 0;
	struct isp_awbc_rgb gain_offset= {0, 0, 0};

	ret = copy_from_user((void *)&gain_offset, param->property_param, sizeof(gain_offset));
	if (0 != ret) {
		printk("isp_k_awbc_gain_offset: copy error, ret=0x%x\n", (uint32_t)ret);
		return -1;
	}

	val = ((gain_offset.b & 0xFFFF) << 16) | (gain_offset.r & 0xFFFF);
	REG_WR(ISP_AWBC_OFFSET0, val);

	val = ((gain_offset.g & 0xFFFF) << 16) | (gain_offset.g & 0xFFFF);
	REG_WR(ISP_AWBC_OFFSET1, val);

	return ret;
}

int32_t isp_k_cfg_awb(struct isp_io_param *param,
	struct isp_k_private *isp_private)
{
	int32_t ret = 0;

	if (!param) {
		printk("isp_k_cfg_awb: param is null error.\n");
		return -1;
	}

	if (NULL == param->property_param) {
		printk("isp_k_cfg_awb: property_param is null error.\n");
		return -1;
	}

	switch(param->property) {
	case ISP_PRO_AWB_BLOCK:
		ret = isp_k_awb_block(param);
		break;
	case ISP_PRO_AWBM_STATISTICS:
		ret = isp_k_awbm_statistics(param);
		break;
	case ISP_PRO_AWBM_BYPASS:
		ret = isp_k_awbm_bypass(param);
		break;
	case ISP_PRO_AWBM_MODE:
		ret = isp_k_awbm_mode(param);
		break;
	case ISP_PRO_AWBM_SKIP_NUM:
		ret = isp_k_awbm_skip_num(param);
		break;
	case ISP_PRO_AWBM_BLOCK_OFFSET:
		ret = isp_k_awbm_block_offset(param);
		break;
	case ISP_PRO_AWBM_BLOCK_SIZE:
		ret = isp_k_awbm_block_size(param);
		break;
	case ISP_PRO_AWBM_SHIFT:
		ret = isp_k_awbm_shift(param);
		break;
	case ISP_PRO_AWBC_BYPASS:
		ret = isp_k_awbc_bypass(param);
		break;
	case ISP_PRO_AWBC_GAIN:
		ret = isp_k_awbc_gain(param);
		break;
	case ISP_PRO_AWBC_THRD:
		ret = isp_k_awbc_thrd(param);
		break;
	case ISP_PRO_AWBC_GAIN_OFFSET:
		ret = isp_k_awbc_gain_offset(param);
		break;
	default:
		printk("isp_k_cfg_awb: fail cmd id:%d, not supported.\n", param->property);
		break;
	}

	return ret;
}
