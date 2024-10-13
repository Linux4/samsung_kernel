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

static int32_t isp_k_raw_aem_block(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t val = 0;
	struct isp_dev_raw_aem_info aem_info;

	memset(&aem_info, 0x00, sizeof(aem_info));

	ret = copy_from_user((void *)&aem_info, param->property_param, sizeof(aem_info));
	if (0 != ret) {
		printk("isp_k_raw_aem_block: copy error, ret=0x%x\n", (uint32_t)ret);
		return -1;
	}

	REG_MWR(ISP_AEM_PARAM, BIT_0, aem_info.bypass);

	val = (aem_info.shift & 0x1F) << 18;
	if(0 != val) {
		printk("isp_k_raw_aem_block: shift error, 0x%x\n", val);
	}
	REG_MWR(ISP_AEM_BLK_SIZE, 0x7C0000, 0);

	REG_MWR(ISP_AEM_PARAM, BIT_1, aem_info.mode << 1);

	return ret;
}

static int32_t isp_k_raw_aem_bypass(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t bypass = 0;

	ret = copy_from_user((void *)&bypass, param->property_param, sizeof(bypass));
	if (0 != ret) {
		printk("isp_k_raw_aem_bypass: copy error, ret=0x%x\n", (uint32_t)ret);
		return -1;
	}

	REG_MWR(ISP_AEM_PARAM, BIT_0, bypass);

	return ret;
}

static int32_t isp_k_raw_aem_mode(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t mode = 0;

	ret = copy_from_user((void *)&mode, param->property_param, sizeof(mode));
	if (0 != ret) {
		printk("isp_k_raw_aem_mode: copy error, ret=0x%x\n", (uint32_t)ret);
		return -1;
	}

	REG_MWR(ISP_AEM_PARAM, BIT_1, mode << 1);

	return ret;
}

static int32_t isp_k_raw_aem_statistics(struct isp_io_param *param,
		struct isp_k_private *isp_private)
{
	int32_t ret = 0, i = 0;
	uint32_t val0 = 0, val1 = 0;
	unsigned long addr = 0;
	struct isp_raw_aem_statistics *aem_statistics = NULL;

	aem_statistics = (struct isp_raw_aem_statistics *)isp_private->raw_aem_buf_addr;
	if (!aem_statistics) {
		ret = -1;
		printk("isp_k_raw_aem_statistics: alloc memory error.\n");
		return -1;
	}

	addr = ISP_RAW_AEM_OUTPUT;
	for (i = 0x00; i < ISP_RAW_AEM_ITEM; i++) {
		val0 = REG_RD(addr);
		val1 = REG_RD(addr + 4);
		aem_statistics->r[i]= (val1 >> 11) & 0x1fffff;
		aem_statistics->g[i]= val0 & 0x3fffff;
		aem_statistics->b[i]= ((val1 & 0x7ff) << 10) | ((val0 >> 22) & 0x3ff);
		addr += 8;
	}

	ret = copy_to_user(param->property_param, (void*)aem_statistics, sizeof(struct isp_raw_aem_statistics));
	if (0 != ret) {
		ret = -1;
		printk("isp_k_raw_aem_statistics: copy error, ret=0x%x\n", (uint32_t)ret);
	}

	return ret;
}

static int32_t isp_k_raw_aem_skip_num(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t val = 0;
	uint32_t skip_num;

	ret = copy_from_user((void *)&skip_num, param->property_param, sizeof(skip_num));
	if (0 != ret) {
		printk("isp_k_raw_aem_skip_num: copy error, ret=0x%x\n", (uint32_t)ret);
		return -1;
	}

	val = (skip_num & 0xF) << 4;
	REG_MWR(ISP_AEM_PARAM, 0xF0, val);

	return ret;
}

static int32_t isp_k_raw_aem_shift(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t shift = 0;
	uint32_t val = 0;

	ret = copy_from_user((void *)&shift, param->property_param, sizeof(shift));
	if (0 != ret) {
		printk("isp_k_raw_aem_shift: copy_from_user error, ret = 0x%x\n", (uint32_t)ret);
		return -1;
	}

	val = (shift & 0x1F) << 18;
	if(0 != val) {
		printk("isp_k_raw_aem_shift: shift error, 0x%x\n", val);
	}
	REG_MWR(ISP_AEM_BLK_SIZE, 0x7C0000, 0);

	return ret;
}

static int32_t isp_k_raw_aem_offset(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t val = 0;
	struct img_offset offset;

	ret = copy_from_user((void *)&offset, param->property_param, sizeof(struct img_offset));
	if (0 != ret) {
		printk("isp_k_raw_aem_offset: read copy_from_user error, ret = 0x%x\n", (uint32_t)ret);
		return -1;
	}

	val = ((offset.y & 0xFFFF) << 16) | (offset.x & 0xFFFF);
	REG_WR(ISP_AEM_OFFSET, val);

	return ret;
}

static int32_t isp_k_raw_aem_blk_size(struct isp_io_param *param)
{
	int32_t ret = 0;
	struct isp_img_size size;
	uint32_t val = 0;

	ret = copy_from_user((void *)&size, param->property_param, sizeof(struct isp_img_size));
	if (0 != ret) {
		printk("isp_k_raw_aem_blk_size: read copy_from_user error, ret = 0x%x\n", (uint32_t)ret);
		return -1;
	}

	val = ((size.height & 0x1FF) << 9) | (size.width & 0x1FF);
	REG_MWR(ISP_AEM_BLK_SIZE, 0x3FFFF, val);

	return ret;
}

static int32_t isp_k_raw_aem_slice_size(struct isp_io_param *param)
{
	int32_t ret = 0;
	struct isp_img_size size;
	uint32_t val = 0;

	ret = copy_from_user((void *)&size, param->property_param, sizeof(struct isp_img_size));
	if (0 != ret) {
		printk("isp_k_raw_aem_slice_size: read copy_from_user error, ret = 0x%x\n", (uint32_t)ret);
		return -1;
	}

	val = ((size.height & 0xFFFF) << 16) | (size.width & 0xFFFF);
	REG_WR(ISP_AEM_SLICE_SIZE, val);

	return ret;
}

int32_t isp_k_cfg_raw_aem(struct isp_io_param *param,
		struct isp_k_private *isp_private)
{
	int32_t ret = 0;

	if (!param) {
		printk("isp_k_cfg_raw_aem: param is null error.\n");
		return -1;
	}

	if (NULL == param->property_param) {
		printk("isp_k_cfg_raw_aem: property_param is null error.\n");
		return -1;
	}

	switch (param->property) {
	case ISP_PRO_RAW_AEM_BLOCK:
		ret = isp_k_raw_aem_block(param);
		break;
	case ISP_PRO_RAW_AEM_BYPASS:
		ret = isp_k_raw_aem_bypass(param);
		break;
	case ISP_PRO_RAW_AEM_MODE:
		ret = isp_k_raw_aem_mode(param);
		break;
	case ISP_PRO_RAW_AEM_STATISTICS:
		ret = isp_k_raw_aem_statistics(param, isp_private);
		break;
	case ISP_PRO_RAW_AEM_SKIP_NUM:
		ret = isp_k_raw_aem_skip_num(param);
		break;
	case ISP_PRO_RAW_AEM_SHIFT:
		ret = isp_k_raw_aem_shift(param);
		break;
	case ISP_PRO_RAW_AEM_OFFSET:
		ret = isp_k_raw_aem_offset(param);
		break;
	case ISP_PRO_RAW_AEM_BLK_SIZE:
		ret = isp_k_raw_aem_blk_size(param);
		break;
	case ISP_PRO_RAW_AEM_SLICE_SIZE:
		ret = isp_k_raw_aem_slice_size(param);
		break;
	default:
		printk("isp_k_cfg_raw_aem: fail cmd id:%d, not supported.\n", param->property);
		break;
	}

	return ret;
}
