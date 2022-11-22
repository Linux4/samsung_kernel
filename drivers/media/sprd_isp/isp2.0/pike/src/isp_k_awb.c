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
#include "isp_drv.h"

static int32_t isp_k_awb_block(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t val = 0;
	uint32_t i = 0;
	struct isp_dev_awb_info_v2 awb_info;

	ret = copy_from_user((void *)&awb_info, param->property_param, sizeof(awb_info));
	if (0 != ret) {
		printk("isp_k_awbm_bypass: copy error, ret=0x%x\n", (uint32_t)ret);
		return -1;
	}

	//printk("isp_k_awb_block: %d  %d  %d  %d \n",awb_info.slice_size.width,awb_info.slice_size.height,awb_info.block_size.width,awb_info.block_size.height);
	//printk("isp_k_awb_block: gain r=%d  gr=%d  b=%d \n",awb_info.gain.r,awb_info.gain.gr,awb_info.gain.b);
	//printk("isp_k_awb_block: offset r=%d  gr=%d  b=%d  \n",awb_info.gain_offset.r,awb_info.gain_offset.gr,awb_info.gain_offset.b);
	/*AWBM*/

	REG_MWR(ISP_AWBM_PARAM, BIT_0, awb_info.awbm_bypass);

	REG_MWR(ISP_AWBM_PARAM, BIT_1, awb_info.mode << 1);

	val = (awb_info.skip_num & 0x0F) << 2;
	REG_MWR(ISP_AWBM_PARAM, (BIT_5 | BIT_4 | BIT_3 | BIT_2), val);

	val = ((awb_info.block_offset.y & 0xFFFF) << 16) | (awb_info.block_offset.x & 0xFFFF);
	REG_WR(ISP_AWBM_BLOCK_OFFSET, val);

	val = ((awb_info.block_size.height & 0x1FF) << 9) | (awb_info.block_size.width & 0x1FF);
	REG_MWR(ISP_AWBM_BLOCK_SIZE, 0x1FFFF, val);

	val = (awb_info.shift & 0x1F) << 18;
	REG_MWR(ISP_AWBM_BLOCK_SIZE, (0x1F << 18), val);

	REG_MWR(ISP_AWBM_PARAM, BIT_8, awb_info.thr_bypass << 8);

	for (i = 0; i < 5; i++) {
		val = ((awb_info.rect_pos.start_x[i] & 0x3FF) << 16) | (awb_info.rect_pos.start_y[i] & 0x3FF);
		REG_WR(ISP_AWBM_WR_0_S + i * 8, val);
		val = ((awb_info.rect_pos.end_x[i] & 0x3FF) << 16) | (awb_info.rect_pos.end_y[i] & 0x3FF);
		REG_WR(ISP_AWBM_WR_0_E + i * 8, val);
	}

	for (i = 0; i < 5; i++) {
		val = ((awb_info.circle_pos.x[i] & 0x3FF) << 18) | ((awb_info.circle_pos.y[i] & 0x3FF) << 8) | (awb_info.circle_pos.r[i] & 0xFF);
		REG_WR(ISP_AWBM_NW_0_XYR + i * 4, val);
	}

	for (i = 0; i < 5; i++) {
		val = ((awb_info.clctor_pos.start_x[i] & 0x3FF) << 16) | (awb_info.clctor_pos.start_y[i] & 0x3FF);
		REG_WR(ISP_AWBM_CLCTOR_0_S + i * 8, val);
		val = ((awb_info.clctor_pos.end_x[i] & 0x3FF) << 16) | (awb_info.clctor_pos.end_y[i] & 0x3FF);
		REG_WR(ISP_AWBM_CLCTOR_0_E + i * 8, val);
	}

	val = ((awb_info.thr.g_low & 0x3FF) << 20) | ((awb_info.thr.r_high & 0x3FF) << 10) | (awb_info.thr.r_low & 0x3FF);
	REG_WR(ISP_AWBM_THR_VALUE1, val);
	val = ((awb_info.thr.b_high & 0x3FF) << 20) | ((awb_info.thr.b_low & 0x3FF) << 10) | (awb_info.thr.g_high & 0x3FF);
	REG_WR(ISP_AWBM_THR_VALUE2, val);

//	val = ((awb_info.slice_size.height & 0xFFFF) << 16) | (awb_info.slice_size.width & 0xFFFF);
//	REG_WR(ISP_AEM_SLICE_SIZE, val);

	REG_MWR(ISP_AWBM_PARAM, BIT_6, awb_info.skip_num_clear << 6);
	REG_MWR(ISP_AWBM_POSITION_SEL, BIT_0, awb_info.position_sel);

	REG_WR(ISP_AWBM_MEM_ADDR, awb_info.mem_addr);

	/*AWBC*/

	REG_MWR(ISP_AWBC_PARAM, BIT_0, awb_info.awbc_bypass);

	REG_MWR(ISP_AWBC_PARAM, BIT_1, awb_info.alpha_bypass << 1);

	val = (awb_info.alpha_value & 0x3FF) << 4;
	REG_MWR(ISP_AWBC_PARAM, 0x3FF0, val);

	REG_MWR(ISP_AWBC_PARAM, BIT_3, awb_info.buf_sel << 3);

	val = ((awb_info.gain.b & 0x3FFF) << 16) | (awb_info.gain.r & 0x3FFF);
	REG_WR(ISP_AWBC_GAIN0, val);
	val = ((awb_info.gain.gb & 0x3FFF) << 16) | (awb_info.gain.gr & 0x3FFF);
	REG_WR(ISP_AWBC_GAIN1, val);

	val = ((awb_info.thrd.b & 0x3FF) << 20) | ((awb_info.thrd.g & 0x3FF) << 10) | (awb_info.thrd.r & 0x3FF);
	REG_WR(ISP_AWBC_THRD, val);

	val = ((awb_info.gain_offset.b & 0xFFFF) << 16) | (awb_info.gain_offset.r & 0xFFFF);
	REG_WR(ISP_AWBC_OFFSET0, val);
	val = ((awb_info.gain_offset.gb & 0xFFFF) << 16) | (awb_info.gain_offset.gr & 0xFFFF);
	REG_WR(ISP_AWBC_OFFSET1, val);

	val = ((awb_info.gain_buff.b & 0x3FFF) << 16) | (awb_info.gain_buff.r & 0x3FFF);
	REG_WR(ISP_AWBC_GAIN0_BUF, val);
	val = ((awb_info.gain_buff.gb & 0x3FFF) << 16) | (awb_info.gain_buff.gr & 0x3FFF);
	REG_WR(ISP_AWBC_GAIN1_BUF, val);

	val = ((awb_info.gain_offset_buff.b & 0xFFFF) << 16) | (awb_info.gain_offset_buff.r & 0xFFFF);
	REG_WR(ISP_AWBC_OFFSET0_BUF, val);
	val = ((awb_info.gain_offset_buff.gb & 0xFFFF) << 16) | (awb_info.gain_offset_buff.gr & 0xFFFF);
	REG_WR(ISP_AWBC_OFFSET1_BUF, val);

	return ret;
}

static int32_t isp_k_raw_awb_statistics(struct isp_io_param *param,
	struct isp_k_private *isp_private)
{
	int32_t ret = 0, i = 0;
	unsigned long addr = 0;
	struct isp_raw_awbm_statistics *awbm_statistics = NULL;

	awbm_statistics = (struct isp_raw_awbm_statistics *)isp_private->raw_awbm_buf_addr;
	if (!awbm_statistics) {
		ret = -1;
		printk("isp_k_raw_awb_statistics: alloc memory error.\n");
		return -1;
	}

	addr = ISP_RAW_AWBM_OUTPUT;

	for (i = 0x00; i < ISP_RAW_AWBM_ITEM; i++) {
		(awbm_statistics + i)->num4 = REG_RD(addr) >> 16;
		(awbm_statistics + i)->num_t = REG_RD(addr) & 0xffff;
		(awbm_statistics + i)->num2 = REG_RD(addr + 4) >> 16;
		(awbm_statistics + i)->num3 = REG_RD(addr + 4) & 0xffff;
		(awbm_statistics + i)->num0 = REG_RD(addr + 8) >> 16;
		(awbm_statistics + i)->num1 = REG_RD(addr + 8) & 0xffff;
		(awbm_statistics + i)->block_b = REG_RD(addr + 12);
		(awbm_statistics + i)->block_g = REG_RD(addr + 16);
		(awbm_statistics + i)->block_r = REG_RD(addr + 20);
		addr += 32;
	}

	ret = copy_to_user(param->property_param, (void*)awbm_statistics, sizeof(struct isp_raw_awbm_statistics) * ISP_RAW_AWBM_ITEM);
	if (0 != ret) {
		ret = -1;
		printk("isp_k_raw_awb_statistics: copy error, ret=0x%x\n", (uint32_t)ret);
	}

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
		ret = isp_k_raw_awb_statistics(param, isp_private);
		break;
	default:
		printk("isp_k_cfg_awb: fail cmd id:%d, not supported.\n", param->property);
		break;
	}

	return ret;
}
