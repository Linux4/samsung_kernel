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

static int32_t isp_k_gamma_block(struct isp_io_param *param)
{
	int32_t ret = 0, i = 0;
	uint32_t addr = 0;
	struct isp_dev_gamma_info gamma_info;

	memset(&gamma_info, 0x00, sizeof(gamma_info));

	ret = copy_from_user((void *)&gamma_info, param->property_param, sizeof(gamma_info));
	if (0 != ret) {
		printk("isp_k_gamma_block: copy error, ret=0x%x\n", (uint32_t)ret);
		return -1;
	}

	/*Gamma Correction 1: Red*/
	gamma_info.node[0] = (0x0480 << 16) |0x0000;
	gamma_info.node[1] = (0x0400 << 16) | 0x0001;
	gamma_info.node[2] = (0x0380 << 16) | 0x0003;
	gamma_info.node[3] = (0x0380 << 16) | 0x0003;
	gamma_info.node[4] = (0x0280 << 16) | 0x000B;
	gamma_info.node[5] = (0x0220 << 16) | 0x0010;
	gamma_info.node[6] = (0x0280 << 16) | 0x0008;
	gamma_info.node[7] = (0x0220 << 16) | 0x0011;
	gamma_info.node[8] = (0x0180 << 16) | 0x002A;
	gamma_info.node[9] = (0x0110 << 16) | 0x0043;
	gamma_info.node[10] = (0x00D5 << 16) | 0x0053;
	gamma_info.node[11] = (0x00B8 << 16) | 0x005E;
	gamma_info.node[12] = (0x00A0 << 16) | 0x006A;
	gamma_info.node[13] = (0x0090 << 16) | 0x0074;
	gamma_info.node[14] = (0x0075 << 16) | 0x0088;
	gamma_info.node[15] = (0x0092 << 16) | 0x006D;
	addr = ISP_GAMMA_NODE_R0;
	for (i = 0; i < 16; i++) {
		REG_WR(addr, gamma_info.node[i]);
		addr += 4;
	}
	/*Gamma Correction 1: Green*/
	gamma_info.node[16] = (0x0480 << 16) |0x0000;
	gamma_info.node[17] = (0x0400 << 16) |0x0001;
	gamma_info.node[18] = (0x0380 << 16) |0x0003;
	gamma_info.node[19] = (0x0380 << 16) |0x0003;
	gamma_info.node[20] = (0x0280 << 16) |0x000B;
	gamma_info.node[21] = (0x0220 << 16) |0x0010;
	gamma_info.node[22] = (0x0280 << 16) |0x0008;
	gamma_info.node[23] = (0x0220 << 16) |0x0011;
	gamma_info.node[24] = (0x0180 << 16) |0x002A;
	gamma_info.node[25] = (0x0110 << 16) |0x0043;
	gamma_info.node[26] = (0x00D5 << 16) |0x0053;
	gamma_info.node[27] = (0x00B8 << 16) |0x005E;
	gamma_info.node[28] = (0x00A0 << 16) |0x006A;
	gamma_info.node[29] = (0x0090 << 16) |0x0074;
	gamma_info.node[30] = (0x0075 << 16) |0x0088;
	gamma_info.node[31] = (0x0092 << 16) |0x006D;
	addr = ISP_GAMMA_NODE_G0;
	for (i = 16; i < 32; i++) {
		REG_WR(addr, gamma_info.node[i]);
		addr += 4;
	}
	/*Gamma Correction 1: Blue*/
	gamma_info.node[32] = (0x0480 << 16) |0x0000;
	gamma_info.node[33] = (0x0400 << 16) |0x0001;
	gamma_info.node[34] = (0x0380 << 16) |0x0003;
	gamma_info.node[35] = (0x0380 << 16) |0x0003;
	gamma_info.node[36] = (0x0280 << 16) |0x000B;
	gamma_info.node[37] = (0x0220 << 16) |0x0010;
	gamma_info.node[38] = (0x0280 << 16) |0x0008;
	gamma_info.node[39] = (0x0220 << 16) |0x0011;
	gamma_info.node[40] = (0x0180 << 16) |0x002A;
	gamma_info.node[41] = (0x0110 << 16) |0x0043;
	gamma_info.node[42] = (0x00D5 << 16) |0x0053;
	gamma_info.node[43] = (0x00B8 << 16) |0x005E;
	gamma_info.node[44] = (0x00A0 << 16) |0x006A;
	gamma_info.node[45] = (0x0090 << 16) |0x0074;
	gamma_info.node[46] = (0x0075 << 16) |0x0088;
	gamma_info.node[47] = (0x0092 << 16) |0x006D;
	addr = ISP_GAMMA_NODE_B0;
	for (i = 32; i < 48; i++) {
		REG_WR(addr, gamma_info.node[i]);
		addr += 4;
	}
	/*Gamma Correction 2: Red X*/
	gamma_info.node[48] = ((8 & 0x3FF) << 20) |((16 & 0x3FF) << 10) |(24 & 0x3FF);
	gamma_info.node[49] = ((32 & 0x3FF) << 20) |((48 & 0x3FF) << 10) |(80 & 0x3FF);
	gamma_info.node[50] = ((96 & 0x3FF) << 20) |((160 & 0x3FF) << 10) |(224 & 0x3FF);
	gamma_info.node[51] = ((288 & 0x3FF) << 20) |((384 & 0x3FF) << 10) |(512 & 0x3FF);
	gamma_info.node[52] = ((640 & 0x3FF) << 20) |((768 & 0x3FF) << 10) |(960 & 0x3FF);
	gamma_info.node[53] = (1023 & 0x3FF) << 20;
	addr = ISP_GAMMA_NODE_RX0;
	for (i = 48; i < 54; i++) {
		REG_WR(addr, gamma_info.node[i]);
		addr += 4;
	}
	/*Gamma Correction 2: Green X*/
	gamma_info.node[54] = ((8 & 0x3FF) << 20) |((16 & 0x3FF) << 10) |(24 & 0x3FF);
	gamma_info.node[55] = ((32 & 0x3FF) << 20) |((48 & 0x3FF) << 10) |(80 & 0x3FF);
	gamma_info.node[56] = ((96 & 0x3FF) << 20) |((160 & 0x3FF) << 10) |(224 & 0x3FF);
	gamma_info.node[57] = ((288 & 0x3FF) << 20) |((384 & 0x3FF) << 10) |(512 & 0x3FF);
	gamma_info.node[58] = ((640 & 0x3FF) << 20) |((768 & 0x3FF) << 10) |(960 & 0x3FF);
	gamma_info.node[59] = (1023 & 0x3FF) << 20;
	addr = ISP_GAMMA_NODE_GX0;
	for (i = 54; i < 60; i++) {
		REG_WR(addr, gamma_info.node[i]);
		addr += 4;
	}
	/*Gamma Correction 2: Blue X*/
	gamma_info.node[60] = ((8 & 0x3FF) << 20) |((16 & 0x3FF) << 10) |(24 & 0x3FF);
	gamma_info.node[61] = ((32 & 0x3FF) << 20) |((48 & 0x3FF) << 10) |(80 & 0x3FF);
	gamma_info.node[62] = ((96 & 0x3FF) << 20) |((160 & 0x3FF) << 10) |(224 & 0x3FF);
	gamma_info.node[63] = ((288 & 0x3FF) << 20) |((384 & 0x3FF) << 10) |(512 & 0x3FF);
	gamma_info.node[64] = ((640 & 0x3FF) << 20) |((768 & 0x3FF) << 10) |(960 & 0x3FF);
	gamma_info.node[65] = (1023 & 0x3FF) << 20;
	addr = ISP_GAMMA_NODE_BX0;
	for (i = 60; i < 66; i++) {
		REG_WR(addr, gamma_info.node[i]);
		addr += 4;
	}

	if (gamma_info.bypass) {
		REG_OWR(ISP_GAMMA_PARAM, BIT_0);
	} else {
		REG_MWR(ISP_GAMMA_PARAM, BIT_0, 0);
	}

	return ret;
}

static int32_t isp_k_gamma_bypass(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t bypass = 0;

	ret = copy_from_user((void *)&bypass, param->property_param, sizeof(bypass));
	if (0 != ret) {
		printk("isp_k_gamma_bypass: copy error, ret=0x%x\n", (uint32_t)ret);
		return -1;
	}

	if (bypass) {
		REG_OWR(ISP_GAMMA_PARAM, BIT_0);
	} else {
		REG_MWR(ISP_GAMMA_PARAM, BIT_0, 0);
	}

	return ret;
}

static int32_t isp_k_gamma_node(struct isp_io_param *param)
{
	int32_t ret = 0, i = 0;
	uint32_t addr = 0;
	struct isp_gamma_node node;

	memset(&node, 0x00, sizeof(node));

	ret = copy_from_user((void *)&(node.val), param->property_param, sizeof(node.val));
	if (0 != ret) {
		printk("isp_k_gamma_node: copy error, ret=0x%x\n", (uint32_t)ret);
		return -1;
	}

	/*Gamma Correction 1: Red*/
	node.val[0] = (0x0480 << 16) |0x0000;
	node.val[1] = (0x0400 << 16) | 0x0001;
	node.val[2] = (0x0380 << 16) | 0x0003;
	node.val[3] = (0x0380 << 16) | 0x0003;
	node.val[4] = (0x0280 << 16) | 0x000B;
	node.val[5] = (0x0220 << 16) | 0x0010;
	node.val[6] = (0x0280 << 16) | 0x0008;
	node.val[7] = (0x0220 << 16) | 0x0011;
	node.val[8] = (0x0180 << 16) | 0x002A;
	node.val[9] = (0x0110 << 16) | 0x0043;
	node.val[10] = (0x00D5 << 16) | 0x0053;
	node.val[11] = (0x00B8 << 16) | 0x005E;
	node.val[12] = (0x00A0 << 16) | 0x006A;
	node.val[13] = (0x0090 << 16) | 0x0074;
	node.val[14] = (0x0075 << 16) | 0x0088;
	node.val[15] = (0x0092 << 16) | 0x006D;
	addr = ISP_GAMMA_NODE_R0;
	for (i = 0; i < 16; i++) {
		REG_WR(addr, node.val[i]);
		addr += 4;
	}
	/*Gamma Correction 1: Green*/
	node.val[16] = (0x0480 << 16) |0x0000;
	node.val[17] = (0x0400 << 16) |0x0001;
	node.val[18] = (0x0380 << 16) |0x0003;
	node.val[19] = (0x0380 << 16) |0x0003;
	node.val[20] = (0x0280 << 16) |0x000B;
	node.val[21] = (0x0220 << 16) |0x0010;
	node.val[22] = (0x0280 << 16) |0x0008;
	node.val[23] = (0x0220 << 16) |0x0011;
	node.val[24] = (0x0180 << 16) |0x002A;
	node.val[25] = (0x0110 << 16) |0x0043;
	node.val[26] = (0x00D5 << 16) |0x0053;
	node.val[27] = (0x00B8 << 16) |0x005E;
	node.val[28] = (0x00A0 << 16) |0x006A;
	node.val[29] = (0x0090 << 16) |0x0074;
	node.val[30] = (0x0075 << 16) |0x0088;
	node.val[31] = (0x0092 << 16) |0x006D;
	addr = ISP_GAMMA_NODE_G0;
	for (i = 16; i < 32; i++) {
		REG_WR(addr, node.val[i]);
		addr += 4;
	}
	/*Gamma Correction 1: Blue*/
	node.val[32] = (0x0480 << 16) |0x0000;
	node.val[33] = (0x0400 << 16) |0x0001;
	node.val[34] = (0x0380 << 16) |0x0003;
	node.val[35] = (0x0380 << 16) |0x0003;
	node.val[36] = (0x0280 << 16) |0x000B;
	node.val[37] = (0x0220 << 16) |0x0010;
	node.val[38] = (0x0280 << 16) |0x0008;
	node.val[39] = (0x0220 << 16) |0x0011;
	node.val[40] = (0x0180 << 16) |0x002A;
	node.val[41] = (0x0110 << 16) |0x0043;
	node.val[42] = (0x00D5 << 16) |0x0053;
	node.val[43] = (0x00B8 << 16) |0x005E;
	node.val[44] = (0x00A0 << 16) |0x006A;
	node.val[45] = (0x0090 << 16) |0x0074;
	node.val[46] = (0x0075 << 16) |0x0088;
	node.val[47] = (0x0092 << 16) |0x006D;
	addr = ISP_GAMMA_NODE_B0;
	for (i = 32; i < 48; i++) {
		REG_WR(addr, node.val[i]);
		addr += 4;
	}
	/*Gamma Correction 2: Red X*/
	node.val[48] = ((8 & 0x3FF) << 20) |((16 & 0x3FF) << 10) |(24 & 0x3FF);
	node.val[49] = ((32 & 0x3FF) << 20) |((48 & 0x3FF) << 10) |(80 & 0x3FF);
	node.val[50] = ((96 & 0x3FF) << 20) |((160 & 0x3FF) << 10) |(224 & 0x3FF);
	node.val[51] = ((288 & 0x3FF) << 20) |((384 & 0x3FF) << 10) |(512 & 0x3FF);
	node.val[52] = ((640 & 0x3FF) << 20) |((768 & 0x3FF) << 10) |(960 & 0x3FF);
	node.val[53] = (1023 & 0x3FF) << 20;
	addr = ISP_GAMMA_NODE_RX0;
	for (i = 48; i < 54; i++) {
		REG_WR(addr, node.val[i]);
		addr += 4;
	}
	/*Gamma Correction 2: Green X*/
	node.val[54] = ((8 & 0x3FF) << 20) |((16 & 0x3FF) << 10) |(24 & 0x3FF);
	node.val[55] = ((32 & 0x3FF) << 20) |((48 & 0x3FF) << 10) |(80 & 0x3FF);
	node.val[56] = ((96 & 0x3FF) << 20) |((160 & 0x3FF) << 10) |(224 & 0x3FF);
	node.val[57] = ((288 & 0x3FF) << 20) |((384 & 0x3FF) << 10) |(512 & 0x3FF);
	node.val[58] = ((640 & 0x3FF) << 20) |((768 & 0x3FF) << 10) |(960 & 0x3FF);
	node.val[59] = (1023 & 0x3FF) << 20;
	addr = ISP_GAMMA_NODE_GX0;
	for (i = 54; i < 60; i++) {
		REG_WR(addr, node.val[i]);
		addr += 4;
	}
	/*Gamma Correction 2: Blue X*/
	node.val[60] = ((8 & 0x3FF) << 20) |((16 & 0x3FF) << 10) |(24 & 0x3FF);
	node.val[61] = ((32 & 0x3FF) << 20) |((48 & 0x3FF) << 10) |(80 & 0x3FF);
	node.val[62] = ((96 & 0x3FF) << 20) |((160 & 0x3FF) << 10) |(224 & 0x3FF);
	node.val[63] = ((288 & 0x3FF) << 20) |((384 & 0x3FF) << 10) |(512 & 0x3FF);
	node.val[64] = ((640 & 0x3FF) << 20) |((768 & 0x3FF) << 10) |(960 & 0x3FF);
	node.val[65] = (1023 & 0x3FF) << 20;
	addr = ISP_GAMMA_NODE_BX0;
	for (i = 60; i < 66; i++) {
		REG_WR(addr, node.val[i]);
		addr += 4;
	}

	return ret;
}

int32_t isp_k_cfg_gamma(struct isp_io_param *param)
{
	int32_t ret = 0;

	if (!param) {
		printk("isp_k_cfg_gamma: param is null error.\n");
		return -1;
	}

	if (NULL == param->property_param) {
		printk("isp_k_cfg_gamma: property_param is null error.\n");
		return -1;
	}

	switch(param->property) {
	case ISP_PRO_GAMMA_BLOCK:
		ret = isp_k_gamma_block(param);
		break;
	case ISP_PRO_GAMMA_BYPASS:
		ret = isp_k_gamma_bypass(param);
		break;
	case ISP_PRO_GAMMA_NODE:
		ret = isp_k_gamma_node(param);
		break;
			default:
		printk("isp_k_cfg_gamma: fail cmd id:%d, not supported.\n", param->property);
		break;
	}

	return ret;
}
