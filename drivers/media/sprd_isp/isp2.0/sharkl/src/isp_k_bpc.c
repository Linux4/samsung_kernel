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

static int32_t isp_k_bpc_block(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t val = 0;
	struct isp_dev_bpc_info bpc_info;

	memset(&bpc_info, 0x00, sizeof(bpc_info));

	ret = copy_from_user((void *)&bpc_info, param->property_param, sizeof(bpc_info));
	if (0 != ret) {
		printk("isp_k_bpc_block: copy error, ret=0x%x\n", (uint32_t)ret);
		return -1;
	}

	if (bpc_info.mode) {
		REG_OWR(ISP_BPC_PARAM, BIT_1);
	} else {
		REG_MWR(ISP_BPC_PARAM, BIT_1, 0);
	}

	val = (((bpc_info.texture_thrd & 0x3FF) << 20))
			| ((bpc_info.std_thrd & 0x3FF) << 10)
			| (bpc_info.flat_thrd & 0x3FF);
	REG_WR(ISP_BPC_THRD, val);

	REG_WR(ISP_BPC_MAP_ADDR, bpc_info.map_addr);

	if (bpc_info.bypass) {
		REG_OWR(ISP_BPC_PARAM, BIT_0);
	} else {
		REG_MWR(ISP_BPC_PARAM, BIT_0, 0);
	}

	return ret;
}

static int32_t isp_k_bpc_bypass(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t bypass = 0;

	ret = copy_from_user((void *)&bypass, param->property_param, sizeof(bypass));
	if (0 != ret) {
		printk("isp_k_pc_bypass: copy error, ret=0x%x\n", (uint32_t)ret);
		return -1;
	}

	if (bypass) {
		REG_OWR(ISP_BPC_PARAM, BIT_0);
	} else {
		REG_MWR(ISP_BPC_PARAM, BIT_0, 0);
	}

	return ret;
}

static int32_t isp_k_bpc_mode(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t mode = 0;

	ret = copy_from_user((void *)&mode, param->property_param, sizeof(mode));
	if (0 != ret) {
		printk("isp_k_bpc_mode: copy error, ret=0x%x\n", (uint32_t)ret);
		return -1;
	}

	if (mode) {
		REG_OWR(ISP_BPC_PARAM, BIT_1);
	} else {
		REG_MWR(ISP_BPC_PARAM, BIT_1, 0);
	}

	return ret;
}

static int32_t isp_k_bpc_param_common(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t val = 0;
	struct isp_bpc_common bpc_param= {0, 0, 0};

	ret = copy_from_user((void *)&bpc_param, param->property_param, sizeof(bpc_param));
	if (0 != ret) {
		printk("isp_k_bpc_param_common: copy error, ret=0x%x\n", (uint32_t)ret);
		return -1;
	}

	val = ((bpc_param.super_bad_thrd & 0x3FF) << 20)
			| ((bpc_param.detect_thrd & 0x3FF) << 10)
			| ((bpc_param.pattern_type & 0x3F) << 2);

	REG_MWR(ISP_BPC_PARAM, 0x3FFFFFFC, val);

	return ret;
}

static int32_t isp_k_bpc_thrd(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t val = 0;
	struct isp_bpc_thrd thrd= {0, 0, 0};

	ret = copy_from_user((void *)&thrd, param->property_param, sizeof(thrd));
	if (0 != ret) {
		printk("isp_k_bpc_thrd: copy error, ret=0x%x\n", (uint32_t)ret);
		return -1;
	}

	val = (((thrd.texture & 0x3FF) << 20))
			| ((thrd.std & 0x3FF) << 10)
			| (thrd.flat & 0x3FF);

	REG_WR(ISP_BPC_THRD, val);

	return ret;
}

static int32_t isp_k_bpc_map_addr(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t addr = 0;

	ret = copy_from_user((void *)&addr, param->property_param, sizeof(addr));
	if (0 != ret) {
		printk("isp_k_bpc_map_addr: copy error, ret=0x%x\n", (uint32_t)ret);
		return -1;
	}

	REG_WR(ISP_BPC_MAP_ADDR, addr);

	return ret;
}

static int32_t isp_k_bpc_pixel_num(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t pixel_num = 0;

	ret = copy_from_user((void *)&pixel_num, param->property_param, sizeof(pixel_num));
	if (0 != ret) {
		printk("isp_k_bpc_pixel_num: copy error, ret=0x%x\n", (uint32_t)ret);
		return -1;
	}

	REG_WR(ISP_BPC_PIXEL_NUM, (pixel_num & 0x3FFF));

	return ret;
}

int32_t isp_k_cfg_bpc(struct isp_io_param *param)
{
	int32_t ret = 0;

	if (!param) {
		printk("isp_k_cfg_bpc: param is null error.\n");
		return -1;
	}

	if (NULL == param->property_param) {
		printk("isp_k_cfg_bpc: property_param is null error.\n");
		return -1;
	}

	switch(param->property) {
	case ISP_PRO_BPC_BLOCK:
		ret = isp_k_bpc_block(param);
		break;
	case ISP_PRO_BPC_BYPASS:
		ret = isp_k_bpc_bypass(param);
		break;
	case ISP_PRO_BPC_MODE:
		ret = isp_k_bpc_mode(param);
		break;
	case ISP_PRO_BPC_PARAM_COMMON:
		ret = isp_k_bpc_param_common(param);
		break;
	case ISP_PRO_BPC_THRD:
		ret = isp_k_bpc_thrd(param);
		break;
	case ISP_PRO_BPC_MAP_ADDR:
		ret = isp_k_bpc_map_addr(param);
		break;
	case ISP_PRO_BPC_PIXEL_NUM:
		ret = isp_k_bpc_pixel_num(param);
		break;
	default:
		printk("isp_k_cfg_bpc: fail cmd id:%d, not supported.\n", param->property);
		break;
	}

	return ret;
}
