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

static int32_t isp_k_hdr_block(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t i = 0, index = 0, addr = 0, val = 0;
	struct isp_dev_hdr_info hdr_info;

	memset(&hdr_info, 0x00, sizeof(hdr_info));

	ret = copy_from_user((void *)&hdr_info, param->property_param, sizeof(hdr_info));
	if (0 != ret) {
		printk("isp_k_hdr_block: copy error, ret=0x%x\n", (uint32_t)ret);
		return -1;
	}

	REG_MWR(ISP_HDR_PARAM, BIT_1, (hdr_info.level << 1));

	val = ((hdr_info.b_index & 0xFF) << 16)
		| ((hdr_info.g_index & 0xFF) << 8)
		| (hdr_info.r_index & 0xFF);
	REG_WR(ISP_HDR_INDEX, val);

	addr = ISP_HDR_COMP_OUTPUT;
	for (i = 0; i < ISP_HDR_COMP_ITEM; i++) {
		val = (hdr_info.com[index + 3] << 24)
			| (hdr_info.com[index + 2] << 16)
			| (hdr_info.com[index + 1] << 8)
			| hdr_info.com[index + 0];
		REG_WR(addr, val);
		addr += 4;
		index += 4;
	}

	index = 0;
	addr = ISP_HDR_P2E_OUTPUT;
	for (i = 0; i < ISP_HDR_P2E_ITEM; i++) {
		val = (hdr_info.p2e[index + 3] << 24)
			| (hdr_info.p2e[index + 2] << 16)
			| (hdr_info.p2e[index + 1] << 8)
			| hdr_info.p2e[index + 0];
		REG_WR(addr, val);
		addr += 4;
		index += 4;
	}

	index = 0;
	addr = ISP_HDR_E2P_OUTPUT;
	for (i = 0; i < ISP_HDR_E2P_ITEM; i++) {
		val = (hdr_info.e2p[index + 3] << 24)
			| (hdr_info.e2p[index + 2] << 16)
			| (hdr_info.e2p[index + 1] << 8)
			| hdr_info.e2p[index + 0];
		REG_WR(addr, val);
		addr += 4;
		index += 4;
	}

	if (hdr_info.bypass) {
		REG_OWR(ISP_HDR_PARAM, BIT_0);
	} else {
		REG_MWR(ISP_HDR_PARAM, BIT_0, 0);
	}

	return ret;
}

static int32_t isp_k_hdr_bypass(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t bypass = 0;

	ret = copy_from_user((void *)&bypass, param->property_param, sizeof(bypass));
	if (0 != ret) {
		printk("isp_k_hdr_bypass: copy error, ret=0x%x\n", (uint32_t)ret);
		return -1;
	}

	if (bypass) {
		REG_OWR(ISP_HDR_PARAM, BIT_0);
	} else {
		REG_MWR(ISP_HDR_PARAM, BIT_0, 0);
	}

	return ret;
}

static int32_t isp_k_hdr_level(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t level= 0;

	ret = copy_from_user((void *)&level, param->property_param, sizeof(level));
	if (0 != ret) {
		printk("isp_k_hdr_level: copy error, ret=0x%x\n", (uint32_t)ret);
		return -1;
	}

	REG_MWR(ISP_HDR_PARAM, BIT_1, (level << 1));

	return ret;
}

static int32_t isp_k_hdr_index(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t val = 0;
	struct isp_hdr_rgb_index index;

	memset(&index, 0x00, sizeof(index));

	ret = copy_from_user((void *)&index, param->property_param, sizeof(index));
	if (0 != ret) {
		printk("isp_k_hdr_index: copy error, ret=0x%x\n", (uint32_t)ret);
		return -1;
	}

	val = ((index.b & 0xFF) << 16)
		| ((index.g & 0xFF) << 8)
		| (index.r & 0xFF);
	REG_WR(ISP_HDR_INDEX, val);

	return ret;
}

static int32_t isp_k_hdr_tab(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t i = 0, index = 0, addr = 0, val = 0;
	struct isp_hdr_tab tab;

	memset(&tab, 0x00, sizeof(tab));

	ret = copy_from_user((void *)&tab, param->property_param, sizeof(tab));
	if (0 != ret) {
		printk("isp_k_hdr_tab: copy error, ret=0x%x\n", (uint32_t)ret);
		return -1;
	}

	addr = ISP_HDR_COMP_OUTPUT;
	for (i = 0; i < ISP_HDR_COMP_ITEM; i++) {
		val = (tab.com[index + 3] << 24)
			| (tab.com[index + 2] << 16)
			| (tab.com[index + 1] << 8)
			| tab.com[index + 0];
		REG_WR(addr, val);
		addr += 4;
		index += 4;
	}

	index = 0;
	addr = ISP_HDR_P2E_OUTPUT;
	for (i = 0; i < ISP_HDR_P2E_ITEM; i++) {
		val = (tab.p2e[index + 3] << 24)
			| (tab.p2e[index + 2] << 16)
			| (tab.p2e[index + 1] << 8)
			| tab.p2e[index + 0];
		REG_WR(addr, val);
		addr += 4;
		index += 4;
	}

	index = 0;
	addr = ISP_HDR_E2P_OUTPUT;
	for (i = 0; i < ISP_HDR_E2P_ITEM; i++) {
		val = (tab.e2p[index + 3] << 24)
			| (tab.e2p[index + 2] << 16)
			| (tab.e2p[index + 1] << 8)
			| tab.e2p[index + 0];
		REG_WR(addr, val);
		addr += 4;
		index += 4;
	}

	return ret;
}

int32_t isp_k_cfg_hdr(struct isp_io_param *param)
{
	int32_t ret = 0;

	if (!param) {
		printk("isp_k_cfg_hdr: param is null error.\n");
		return -1;
	}

	if (NULL == param->property_param) {
		printk("isp_k_cfg_hdr: property_param is null error.\n");
		return -1;
	}

	switch (param->property) {
	case ISP_PRO_HDR_BLOCK:
		ret = isp_k_hdr_block(param);
		break;
	case ISP_PRO_HDR_BYPASS:
		ret = isp_k_hdr_bypass(param);
		break;
	case ISP_PRO_HDR_LEVEL:
		ret = isp_k_hdr_level(param);
		break;
	case ISP_PRO_HDR_INDEX:
		ret = isp_k_hdr_index(param);
		break;
	case ISP_PRO_HDR_TAB:
		ret = isp_k_hdr_tab(param);
		break;
	default:
		printk("isp_k_cfg_hdr: fail cmd id:%d, not supported.\n", param->property);
		break;
	}

	return ret;
}
