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
	uint32_t i = 0, val = 0;
	uint32_t addr_r = 0, addr_g = 0, addr_b = 0, addr_l = 0;
	struct isp_dev_nlc_info nlc_info;

	memset(&nlc_info, 0x00, sizeof(nlc_info));

	ret = copy_from_user((void *)&nlc_info, param->property_param, sizeof(nlc_info));
	if (0 != ret) {
		printk("isp_k_nlc_block: copy error, ret=0x%x\n", (uint32_t)ret);
		return -1;
	}

	addr_r = ISP_NLC_N_PARAM_R0;
	addr_g = ISP_NLC_N_PARAM_G0;
	addr_b = ISP_NLC_N_PARAM_B0;
	addr_l = ISP_NLC_N_PARAM_L0;
	for (i = 0; i <= 24; i += 3) {
		val = ((nlc_info.r_node[i + 2] & 0x3FF) << 20) | ((nlc_info.r_node[i + 1] & 0x3FF) << 10) | (nlc_info.r_node[i] & 0x3FF);
		REG_MWR(addr_r, 0x3FFFFFFF, val);

		val = ((nlc_info.g_node[i + 2] & 0x3FF) << 20) | ((nlc_info.g_node[i + 1] & 0x3FF) << 10) | (nlc_info.g_node[i] & 0x3FF);
		REG_MWR(addr_g, 0x3FFFFFFF, val);

		val = ((nlc_info.b_node[i + 2] & 0x3FF) << 20) | ((nlc_info.b_node[i + 1] & 0x3FF) << 10) | (nlc_info.b_node[i] & 0x3FF);
		REG_MWR(addr_b, 0x3FFFFFFF, val);

		val = ((nlc_info.l_node[i + 2] & 0x3FF) << 20) | ((nlc_info.l_node[i + 1] & 0x3FF) << 10) | (nlc_info.l_node[i] & 0x3FF);
		REG_MWR(addr_l, 0x3FFFFFFF, val);

		addr_r += 4;
		addr_g += 4;
		addr_b += 4;
		addr_l += 4;
	}

	val = ((nlc_info.r_node[28] & 0x3FF) << 20) | ((nlc_info.r_node[27] & 0x3FF) << 10);
	REG_MWR(ISP_NLC_N_PARAM_R9, 0x3FFFFC00, val);
	val = ((nlc_info.g_node[28] & 0x3FF) << 20) | ((nlc_info.g_node[27] & 0x3FF) << 10);
	REG_MWR(ISP_NLC_N_PARAM_G9, 0x3FFFFC00, val);
	val = ((nlc_info.b_node[28] & 0x3FF) << 20) | ((nlc_info.b_node[27] & 0x3FF) << 10);
	REG_MWR(ISP_NLC_N_PARAM_B9, 0x3FFFFC00, val);

	if (nlc_info.bypass) {
		REG_OWR(ISP_NLC_PARAM, BIT_0);
	} else {
		REG_MWR(ISP_NLC_PARAM, BIT_0, 0);
	}

	return ret;
}

static int32_t isp_k_nlc_bypass(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t bypass = 0;

	ret = copy_from_user((void *)&bypass, param->property_param, sizeof(bypass));
	if (0 != ret) {
		printk("isp_k_nlc_bypass: copy error, ret=0x%x\n", (uint32_t)ret);
		return -1;
	}

	if (bypass) {
		REG_OWR(ISP_NLC_PARAM, BIT_0);
	} else {
		REG_MWR(ISP_NLC_PARAM, BIT_0, 0);
	}

	return ret;
}

static int32_t isp_k_nlc_r_node(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t i = 0, val = 0, addr = 0;
	struct isp_nlc_r_node node;

	ret = copy_from_user((void *)(node.r_node), param->property_param, sizeof(node.r_node));
	if (0 != ret) {
		printk("isp_k_nlc_r_node: copy error, ret=0x%x\n", (uint32_t)ret);
		return -1;
	}


	addr = ISP_NLC_N_PARAM_R0;
	for (i = 0; i <= 24; i += 3) {
		val = ((node.r_node[i + 2] & 0x3FF) << 20) | ((node.r_node[i + 1] & 0x3FF) << 10) | (node.r_node[i] & 0x3FF);
		REG_MWR(addr, 0x3FFFFFFF, val);
		addr += 4;
	}
	val = ((node.r_node[28] & 0x3FF) << 20) | ((node.r_node[27] & 0x3FF) << 10);
	REG_MWR(ISP_NLC_N_PARAM_R9, 0x3FFFFC00, val);

	return ret;
}

static int32_t isp_k_nlc_g_node(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t i = 0, val = 0, addr = 0;
	struct isp_nlc_g_node node;

	ret = copy_from_user((void *)(node.g_node), param->property_param, sizeof(node.g_node));
	if (0 != ret) {
		printk("isp_k_nlc_g_node: copy error, ret=0x%x\n", (uint32_t)ret);
		return -1;
	}

	addr = ISP_NLC_N_PARAM_G0;
	for (i = 0; i <= 24; i += 3) {
		val = ((node.g_node[i + 2] & 0x3FF) << 20) | ((node.g_node[i + 1] & 0x3FF) << 10) | (node.g_node[i] & 0x3FF);
		REG_MWR(addr, 0x3FFFFFFF, val);
		addr += 4;
	}
	val = ((node.g_node[28] & 0x3FF) << 20) | ((node.g_node[27] & 0x3FF) << 10);
	REG_MWR(ISP_NLC_N_PARAM_G9, 0x3FFFFC00, val);

	return ret;
}

static int32_t isp_k_nlc_b_node(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t i = 0, val = 0, addr = 0;
	struct isp_nlc_b_node node;

	ret = copy_from_user((void *)(node.b_node), param->property_param, sizeof(node.b_node));
	if (0 != ret) {
		printk("isp_k_nlc_b_node: copy error, ret=0x%x\n", (uint32_t)ret);
		return -1;
	}

	addr = ISP_NLC_N_PARAM_B0;
	for (i = 0; i <= 24; i += 3) {
		val = ((node.b_node[i + 2] & 0x3FF) << 20) | ((node.b_node[i + 1] & 0x3FF) << 10) | (node.b_node[i] & 0x3FF);
		REG_MWR(addr, 0x3FFFFFFF, val);
		addr += 4;
	}
	val = ((node.b_node[28] & 0x3FF) << 20) | ((node.b_node[27] & 0x3FF) << 10);
	REG_MWR(ISP_NLC_N_PARAM_B9, 0x3FFFFC00, val);

	return ret;
}

static int32_t isp_k_nlc_l_node(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t i = 0, val = 0, addr = 0;
	struct isp_nlc_l_node node;

	ret = copy_from_user((void *)(node.l_node), param->property_param, sizeof(node.l_node));
	if (0 != ret) {
		printk("isp_k_nlc_l_node: copy error, ret=0x%x\n", (uint32_t)ret);
		return -1;
	}

	addr = ISP_NLC_N_PARAM_L0;
	for (i = 0; i <= 24; i += 3) {
		val = ((node.l_node[i + 2] & 0x3FF) << 20) | ((node.l_node[i + 1] & 0x3FF) << 10) | (node.l_node[i] & 0x3FF);
		REG_MWR(addr, 0x3FFFFFFF, val);
		addr += 4;
	}

	return ret;
}

int32_t isp_k_cfg_nlc(struct isp_io_param *param)
{
	int32_t ret = 0;

	if (!param) {
		printk("isp_k_nlc_cfg: param is null error.\n");
		return -1;
	}

	if (!param->property_param) {
		printk("isp_k_nlc_cfg: property_param is null error.\n");
		return -1;
	}

	switch (param->property) {
	case ISP_PRO_NLC_BLOCK:
		isp_k_nlc_block(param);
		break;
	case ISP_PRO_NLC_BYPASS:
		isp_k_nlc_bypass(param);
		break;
	case ISP_PRO_NLC_R_NODE:
		isp_k_nlc_r_node(param);
		break;
	case ISP_PRO_NLC_G_NODE:
		isp_k_nlc_g_node(param);
		break;
	case ISP_PRO_NLC_B_NODE:
		isp_k_nlc_b_node(param);
		break;
	case ISP_PRO_NLC_L_NODE:
		isp_k_nlc_l_node(param);
		break;
	default:
		printk("isp_k_nlc_cfg: fail cmd id:%d, not supported.\n", param->property);
		break;
	}

	return ret;
}
