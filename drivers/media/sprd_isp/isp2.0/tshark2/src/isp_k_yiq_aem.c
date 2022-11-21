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

static int32_t isp_k_yiq_aem_block(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t val = 0;
	struct isp_dev_yiq_aem_info_v1 yaem_info;

	memset(&yaem_info, 0x00, sizeof(yaem_info));

	ret = copy_from_user((void *)&yaem_info, param->property_param, sizeof(yaem_info));
	if (0 != ret) {
		printk("isp_k_yiq_aem_block: copy error, ret=0x%x\n", (uint32_t)ret);
		return -1;
	}

	REG_MWR(ISP_YIQ_AEM_PARAM, BIT_7, yaem_info.ygamma_bypass << 7);

	val = ((yaem_info.gamma_xnode[0] & 0xFF)<<24) | ((yaem_info.gamma_xnode[1] & 0xFF)<<16)
		| ((yaem_info.gamma_xnode[2] & 0xFF)<<8) | (yaem_info.gamma_xnode[3] & 0xFF);
	REG_WR(ISP_YIQ_YGAMMA_NODE_X0 , val);
	val = ((yaem_info.gamma_xnode[4] & 0xFF)<<24) | ((yaem_info.gamma_xnode[5] & 0xFF)<<16)
		| ((yaem_info.gamma_xnode[6] & 0xFF)<<8) | (yaem_info.gamma_xnode[7] & 0xFF);
	REG_WR(ISP_YIQ_YGAMMA_NODE_X1 , val);

	val = ((yaem_info.gamma_ynode[0] & 0xFF)<<24) | ((yaem_info.gamma_ynode[1] & 0xFF)<<16)
		| ((yaem_info.gamma_ynode[2] & 0xFF)<<8) | (yaem_info.gamma_ynode[3] & 0xFF);
	REG_WR( ISP_YIQ_YGAMMA_NODE_Y0 , val);
	val = ((yaem_info.gamma_ynode[4] & 0xFF)<<24) | ((yaem_info.gamma_ynode[5] & 0xFF)<<16)
		| ((yaem_info.gamma_ynode[6] & 0xFF)<<8) | (yaem_info.gamma_ynode[7] & 0xFF);
	REG_WR(ISP_YIQ_YGAMMA_NODE_Y1 , val);
	val = ((yaem_info.gamma_ynode[8] & 0xFF)<<24) | ((yaem_info.gamma_ynode[9] & 0xFF)<<16);
	REG_WR(ISP_YIQ_YGAMMA_NODE_Y2 , val);

	val = ((yaem_info.gamma_node_idx[1]&0x7) << 24) | ((yaem_info.gamma_node_idx[2]&0x7) << 21) |
			((yaem_info.gamma_node_idx[3]&0x7) << 18) | ((yaem_info.gamma_node_idx[4]&0x7) << 15) |
			((yaem_info.gamma_node_idx[5]&0x7) << 12) | ((yaem_info.gamma_node_idx[6]&0x7) <<  9) |
			((yaem_info.gamma_node_idx[7]&0x7) <<  6) | ((yaem_info.gamma_node_idx[8]&0x7) <<  3) |
			(yaem_info.gamma_node_idx[9]&0x7);
	REG_WR( ISP_YIQ_YGAMMA_NODE_IDX, val);

	REG_MWR(ISP_YIQ_AEM_PARAM, BIT_0, yaem_info.aem_bypass);

	REG_MWR(ISP_YIQ_AEM_PARAM, BIT_1, yaem_info.aem_mode << 1);

	REG_MWR(ISP_YIQ_AEM_PARAM, 0xF << 2, yaem_info.aem_skip_num << 2);

	val = (yaem_info.offset_x & 0xFFFF) | ((yaem_info.offset_y & 0xFFFF) << 16);
	REG_WR( ISP_YIQ_AEM_OFFSET, val);

	val = (yaem_info.width& 0x1FF) | ((yaem_info.height& 0x1FF) << 9);
	REG_WR( ISP_YIQ_AEM_BLK_SIZE, val);

	return ret;

}

static int32_t isp_k_yiq_slice_size(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t val = 0;
	struct isp_img_size slice_size= {0, 0};

	ret = copy_from_user((void *)&slice_size, param->property_param, sizeof(slice_size));
	if (0 != ret) {
		printk("isp_k_ae_skip_num: copy_from_user error, ret = 0x%x\n", (uint32_t)ret);
		return -1;
	}

	val = (slice_size.width&  0xFFFF) | ((slice_size.height&  0xFFFF) << 16);
	REG_WR( ISP_YIQ_AEM_SLICE_SIZE, val);


	return ret;
}

static int32_t isp_k_yiq_aem_bypass(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t bypass = 0;

	ret = copy_from_user((void *)&bypass, param->property_param, sizeof(bypass));
	if (0 != ret) {
		printk("isp_k_raw_aem_bypass: copy error, ret=0x%x\n", (uint32_t)ret);
		return -1;
	}

	REG_MWR(ISP_YIQ_AEM_PARAM, BIT_0, bypass);
	return ret;
}

static int32_t isp_k_yiq_aem_ygamma_bypass(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t bypass = 0;

	ret = copy_from_user((void *)&bypass, param->property_param, sizeof(bypass));
	if (0 != ret) {
		printk("isp_k_raw_aem_bypass: copy error, ret=0x%x\n", (uint32_t)ret);
		return -1;
	}

	REG_MWR(ISP_YIQ_AEM_PARAM, BIT_7, bypass << 7);
	return ret;
}

static int32_t isp_k_yiq_aem_statistics(struct isp_io_param *param,
		struct isp_k_private *isp_private)
{
	int32_t ret = 0, i = 0;
	uint32_t val = 0;
	unsigned long addr = 0;
	struct isp_aem_statistics *yiq_aem_statistics = NULL;

	yiq_aem_statistics = (struct isp_aem_statistics *)isp_private->yiq_aem_buf_addr;
	if (!yiq_aem_statistics) {
		ret = -1;
		printk("isp_k_yiq_aem_statistics: alloc memory error.\n");
		return -1;
	}

	addr = ISP_YIQ_AEM_OUTPUT;
	for (i = 0x00; i < ISP_YIQ_AEM_ITEM; i++) {
		val = REG_RD(addr);
		yiq_aem_statistics->val[i]= val & 0x3fffff;
		addr += 4;
	}

	ret = copy_to_user(param->property_param, (void*)yiq_aem_statistics, sizeof(struct isp_aem_statistics));
	if (0 != ret) {
		ret = -1;
		printk("isp_k_yiq_aem_statistics: copy error, ret=0x%x\n", (uint32_t)ret);
	}

	return ret;
}

static int32_t isp_k_yiq_aem_skip_num(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t val = 0;
	uint32_t skip_num;

	ret = copy_from_user((void *)&skip_num, param->property_param, sizeof(skip_num));
	if (0 != ret) {
		printk("isp_k_yiq_aem_skip_num: copy error, ret=0x%x\n", (uint32_t)ret);
		return -1;
	}

	val = (skip_num & 0xF) << 2;
	REG_MWR(ISP_YIQ_AEM_PARAM, 0x3C, val);

	return ret;
}

static int32_t isp_k_yiq_aem_offset(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t val = 0;
	struct img_offset offset;

	ret = copy_from_user((void *)&offset, param->property_param, sizeof(struct img_offset));
	if (0 != ret) {
		printk("isp_k_yiq_aem_offset: read copy_from_user error, ret = 0x%x\n", (uint32_t)ret);
		return -1;
	}

	val = ((offset.y & 0xFFFF) << 16) | (offset.x & 0xFFFF);
	REG_WR(ISP_YIQ_AEM_OFFSET, val);

	return ret;
}

static int32_t isp_k_yiq_aem_blk_size(struct isp_io_param *param)
{
	int32_t ret = 0;
	struct isp_img_size size;
	uint32_t val = 0;

	ret = copy_from_user((void *)&size, param->property_param, sizeof(struct isp_img_size));
	if (0 != ret) {
		printk("isp_k_yiq_aem_blk_size: read copy_from_user error, ret = 0x%x\n", (uint32_t)ret);
		return -1;
	}

	val = ((size.height & 0x1FF) << 9) | (size.width & 0x1FF);
	REG_MWR(ISP_YIQ_AEM_BLK_SIZE, 0x3FFFF, val);

	return ret;
}

int32_t isp_k_cfg_yiq_aem(struct isp_io_param *param,
		struct isp_k_private *isp_private)
{
	int32_t ret = 0;

	if (!param) {
		printk("isp_k_cfg_yiq: param is null error.\n");
		return -1;
	}

	if (NULL == param->property_param) {
		printk("isp_k_cfg_yiq: property_param is null error.\n");
		return -1;
	}

	switch (param->property) {
	case ISP_PRO_YIQ_AEM_BLOCK:
		ret = isp_k_yiq_aem_block(param);
		break;
	case ISP_PRO_YIQ_AEM_YGAMMA_BYPASS:
		ret = isp_k_yiq_aem_ygamma_bypass(param);
		break;
	case ISP_PRO_YIQ_AEM_BYPASS:
		ret = isp_k_yiq_aem_bypass(param);
		break;
	case ISP_PRO_YIQ_AEM_STATISTICS:
		ret = isp_k_yiq_aem_statistics(param, isp_private);
		break;
	case ISP_PRO_YIQ_AEM_SKIP_NUM:
		ret = isp_k_yiq_aem_skip_num(param);
		break;
	case ISP_PRO_YIQ_AEM_OFFSET:
		ret = isp_k_yiq_aem_offset(param);
		break;
	case ISP_PRO_YIQ_AEM_BLK_SIZE:
		ret = isp_k_yiq_aem_blk_size(param);
		break;
	case ISP_PRO_YIQ_AEM_SLICE_SIZE:
		ret = isp_k_yiq_slice_size(param);
		break;
	default:
		printk("isp_k_cfg_yiq: fail cmd id:%d, not supported.\n", param->property);
		break;
	}

	return ret;
}
