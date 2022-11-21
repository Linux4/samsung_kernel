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
#include <asm/cacheflush.h>
#include "isp_reg.h"
#include "isp_drv.h"

static int32_t isp_k_binging_param_update(struct isp_k_private *isp_private)
{
	int32_t ret = 0;
	uint32_t addr = 0;
#ifndef CONFIG_64BIT
	void *buf_ptr = (void *)isp_private->bing4awb_buf_addr;
#endif
	uint32_t buf_len = isp_private->bing4awb_buf_len;

	if((0x00 != isp_private->bing4awb_buf_addr) && (0x00 != isp_private->bing4awb_buf_len)) {

#ifndef CONFIG_64BIT
		dmac_flush_range(buf_ptr, buf_ptr + buf_len);
		outer_flush_range(__pa(buf_ptr), __pa(buf_ptr) + buf_len);
#endif
		addr = (uint32_t)__pa(isp_private->bing4awb_buf_addr);

		REG_WR(ISP_BINNING_MEM_ADDR, addr);
	}

	return ret;
}

static int32_t isp_k_binning_block(struct isp_io_param *param,
			struct isp_k_private *isp_private)
{
	int32_t ret = 0;
	uint32_t val = 0;
	int32_t i = 0;
	struct isp_dev_binning4awb_info_v1 binning_info;

	ret = copy_from_user((void *)&binning_info, param->property_param, sizeof(binning_info));
	if (0 != ret) {
		printk("isp_k_binning_block: copy error, ret=0x%x\n", (uint32_t)ret);
		return -1;
	}

//	isp_k_binging_param_update(isp_private);

	val = (binning_info.burst_mode & 0x3) << 2;
	REG_MWR(ISP_BINNING_PARAM, 0xC, val);

	val = (binning_info.endian & 0x1) << 1;
	REG_MWR(ISP_BINNING_PARAM, BIT_1, val);

	val = (binning_info.mem_fifo_clr & 0x1) << 7;
	REG_MWR(ISP_BINNING_PARAM, 0x80, val);

	val = ((binning_info.hx & 0x7) << 4) | ((binning_info.vx & 0x7) << 8);
	REG_MWR(ISP_BINNING_PARAM, 0x770, val);

	REG_WR(ISP_BINNING_PITCH, binning_info.pitch);

	REG_MWR(ISP_BINNING_PARAM, BIT_0, binning_info.bypass);

	return ret;

}

static int32_t isp_k_binging_bypass(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t bypass = 0;
	ret = copy_from_user((void *)&bypass, param->property_param, sizeof(uint32_t));
	if (0 != ret) {
		printk("isp_k_binning_scaling_ratio: copy_from_user error, ret = 0x%x\n",(uint32_t)ret);
		return -1;
	}

	REG_MWR(ISP_BINNING_PARAM, BIT_0, bypass);

	return ret;
}

static int32_t isp_k_binning_scaling_ratio(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t val = 0;
	struct isp_scaling_ratio scaling_ratio;
	ret = copy_from_user((void *)&scaling_ratio, param->property_param, sizeof(struct isp_scaling_ratio));
	if (0 != ret) {
		printk("isp_k_binning_scaling_ratio: copy_from_user error, ret = 0x%x\n",(uint32_t)ret);
		return -1;
	}

	val = ((scaling_ratio.vertical & 0x7)<< 8)| ((scaling_ratio.horizontal & 0x7) << 4);
	REG_MWR(ISP_BINNING_PARAM, 0x770, val);

	return ret;
}

static int32_t isp_k_binging_mem_addr(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t addr = 0;

	ret = copy_from_user((void *)&addr, param->property_param, sizeof(uint32_t));
	if (0 != ret) {
		printk("isp_k_binning_scaling_ratio: copy_from_user error, ret = 0x%x\n",(uint32_t)ret);
		return -1;
	}

	REG_WR(ISP_BINNING_MEM_ADDR, addr);

	return ret;
}

static int32_t isp_k_binning_statistics_buf(struct isp_io_param *param,
			struct isp_k_private *isp_private)
{
	int32_t ret = 0;
	uint32_t buf_id = 0;

	if (isp_private->b4awb_buf[0].buf_flag == 0) {
		buf_id = isp_private->b4awb_buf[0].buf_id;
	} else if (isp_private->b4awb_buf[1].buf_flag == 0) {
		buf_id = isp_private->b4awb_buf[0].buf_id;
	}

	ret = copy_to_user(param->property_param, (void*)&buf_id, sizeof(uint32_t));
	if (0 != ret) {
		ret = -1;
		printk("isp_k_binning_statistics_buf: copy error, ret=0x%x\n", (uint32_t)ret);
	}

	return ret;
}

static int32_t isp_k_binning_buffaddr(struct isp_io_param *param,
			struct isp_k_private *isp_private)
{
	int32_t ret = 0;
	int32_t i = 0;
	unsigned long phys_addr[2];
	struct isp_b4awb_phys phy_addr;

	ret = copy_from_user((void *)&phy_addr, param->property_param, sizeof(struct isp_b4awb_phys));

	phys_addr[0] = phy_addr.phys0;
	phys_addr[1] = phy_addr.phys1;

	for (i = 0; i < ISP_BING4AWB_NUM; i++) {
		isp_private->b4awb_buf[i].buf_id = i;
		isp_private->b4awb_buf[i].buf_flag = 0;
		isp_private->b4awb_buf[i].buf_phys_addr = phys_addr[i];
	}

	/*config buf_id 0 as default*/
	isp_private->b4awb_buf[i].buf_flag = 1;
	REG_WR(ISP_BINNING_MEM_ADDR, isp_private->b4awb_buf[i].buf_phys_addr);

	printk("eric phys_addr 0x%x, 0x%x\n",phys_addr[0], phys_addr[1]);

	return ret;
}

int32_t isp_k_cfg_binning(struct isp_io_param *param, struct isp_k_private *isp_private)
{
	int32_t ret = 0;

	if (!param) {
		printk("isp_k_cfg_binning: param is null error.\n");
		return -1;
	}

	if (NULL == param->property_param) {
		printk("isp_k_cfg_binning: property_param is null error.\n");
		return -1;
	}

	switch(param->property) {
	case ISP_PRO_BINNING4AWB_BLOCK:
		ret = isp_k_binning_block(param, isp_private);
		break;
	case ISP_PRO_BINNING4AWB_BYPASS:
		ret = isp_k_binging_bypass(param);
		break;
	case ISP_PRO_BINNING4AWB_SCALING_RATIO:
		ret = isp_k_binning_scaling_ratio(param);
		break;
	case ISP_PRO_BINNING4AWB_MEM_ADDR:
		ret = isp_k_binging_mem_addr(param);
		break;
	case ISP_PRO_BINNING4AWB_STATISTICS_BUF:
		ret = isp_k_binning_statistics_buf(param, isp_private);
		break;
	case ISP_PRO_BINNING4AWB_TRANSADDR:
		ret = isp_k_binning_buffaddr(param, isp_private);
		break;
	default:
		printk("isp_k_cfg_binning: fail cmd id:%d, not supported.\n", param->property);
		break;
	}

	return ret;
}
