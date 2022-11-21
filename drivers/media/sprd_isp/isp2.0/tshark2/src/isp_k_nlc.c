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

static int32_t isp_k_nlc_block(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t val = 0;
	uint32_t i = 0;
	struct isp_dev_nlc_info_v1 nlc_info;

	memset(&nlc_info, 0x00, sizeof(nlc_info));

	ret = copy_from_user((void *)&nlc_info, param->property_param, sizeof(nlc_info));
	if (0 != ret) {
		printk("isp_k_nlc_block: copy error, ret=0x%x\n", (uint32_t)ret);
		return -1;
	}

	for (i = 0; i < 9; i++) {
		val = ((nlc_info.node.r_node[i*3] & 0x3FF) << 20) | ((nlc_info.node.r_node[i*3+1] & 0x3FF) << 10) | (nlc_info.node.r_node[i*3+2] & 0x3FF);
		REG_WR(ISP_NLC_PARA_R0 + i * 4, val);
		val = ((nlc_info.node.g_node[i*3] & 0x3FF) << 20) | ((nlc_info.node.g_node[i*3+1] & 0x3FF) << 10) | (nlc_info.node.g_node[i*3+2] & 0x3FF);
		REG_WR(ISP_NLC_PARA_G0 + i * 4, val);
		val = ((nlc_info.node.b_node[i*3] & 0x3FF) << 20) | ((nlc_info.node.b_node[i*3+1] & 0x3FF) << 10) | (nlc_info.node.b_node[i*3+2] & 0x3FF);
		REG_WR(ISP_NLC_PARA_B0 + i * 4, val);
		val = ((nlc_info.node.l_node[i*3] & 0x3FF) << 20) | ((nlc_info.node.l_node[i*3+1] & 0x3FF) << 10) | (nlc_info.node.l_node[i*3+2] & 0x3FF);
		REG_WR(ISP_NLC_PARA_L0 + i * 4, val);
	}

	val = ((nlc_info.node.r_node[27] & 0x3FF) << 20) | ((nlc_info.node.r_node[28] & 0x3FF) << 10);
	REG_WR(ISP_NLC_PARA_R9, val);
	val = ((nlc_info.node.g_node[27] & 0x3FF) << 20) | ((nlc_info.node.g_node[28] & 0x3FF) << 10);
	REG_WR(ISP_NLC_PARA_G9, val);
	val = ((nlc_info.node.b_node[27] & 0x3FF) << 20) | ((nlc_info.node.b_node[28] & 0x3FF) << 10);
	REG_WR(ISP_NLC_PARA_B9, val);

	REG_MWR(ISP_NLC_PARA, BIT_0, nlc_info.bypass);

	return ret;

}

int32_t isp_k_cfg_nlc(struct isp_io_param *param)
{
	int32_t ret = 0;

	if (!param) {
		printk("isp_k_cfg_nlc: param is null error.\n");
		return -1;
	}

	if (NULL == param->property_param) {
		printk("isp_k_cfg_nlc: property_param is null error.\n");
		return -1;
	}

	switch(param->property) {
	case ISP_PRO_NLC_BLOCK:
		ret = isp_k_nlc_block(param);
		break;
	default:
		printk("isp_k_cfg_nlc: fail cmd id:%d, not supported.\n", param->property);
		break;
	}

	return ret;
}

