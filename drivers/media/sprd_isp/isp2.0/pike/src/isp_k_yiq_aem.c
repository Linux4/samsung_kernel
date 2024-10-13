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

static int32_t isp_k_yiq_aem_block(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t val = 0;
	struct isp_dev_yiq_aem_info_v2 yaem_info;

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

	val = (yaem_info.width& 0x1FF) | ((yaem_info.height& 0x1FF) << 9) | ((yaem_info.avgshift & 0x1F) << 18);
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

int32_t isp_k_cfg_yiq_aem(struct isp_io_param *param)
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
	case ISP_PRO_YIQ_AEM_SLICE_SIZE:
		ret = isp_k_yiq_slice_size(param);
		break;
	default:
		printk("isp_k_cfg_yiq: fail cmd id:%d, not supported.\n", param->property);
		break;
	}

	return ret;
}

