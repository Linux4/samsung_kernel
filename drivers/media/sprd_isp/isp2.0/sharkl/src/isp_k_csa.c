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

static int32_t isp_k_csa_block(struct isp_io_param *param)
{
	int32_t ret = 0;
	struct isp_dev_csa_info csa_info;

	memset(&csa_info, 0x00, sizeof(csa_info));

	ret = copy_from_user((void *)&csa_info, param->property_param, sizeof(csa_info));
	if (0 != ret) {
		printk("isp_k_csa_block: copy error, ret=0x%x\n", (uint32_t)ret);
		return -1;
	}

	REG_MWR(ISP_CSA_PARAM, 0x1FE, (csa_info.factor << 1));

	if (csa_info.bypass) {
		REG_OWR(ISP_CSA_PARAM, BIT_0);
	} else {
		REG_MWR(ISP_CSA_PARAM, BIT_0, 0);
	}

	return ret;
}

static int32_t isp_k_csa_bypass(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t bypass = 0;

	ret = copy_from_user((void *)&bypass, param->property_param, sizeof(bypass));
	if (0 != ret) {
		printk("isp_k_csa_bypass: copy error, ret=0x%x\n", (uint32_t)ret);
		return -1;
	}

	if (bypass) {
		REG_OWR(ISP_CSA_PARAM, BIT_0);
	} else {
		REG_MWR(ISP_CSA_PARAM, BIT_0, 0);
	}

	return ret;
}

static int32_t isp_k_csa_factor(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t factor = 0;

	ret = copy_from_user((void *)&factor, param->property_param, sizeof(factor));
	if (0 != ret) {
		printk("isp_k_csa_factor: copy error, ret=0x%x\n", (uint32_t)ret);
		return -1;
	}

	REG_MWR(ISP_CSA_PARAM, 0x1FE, (factor << 1));

	return ret;
}

int32_t isp_k_cfg_csa(struct isp_io_param *param)
{
	int32_t ret = 0;

	if (!param) {
		printk("isp_k_cfg_csa: param is null error.\n");
		return -1;
	}

	if (NULL == param->property_param) {
		printk("isp_k_cfg_csa: property_param is null error.\n");
		return -1;
	}

	switch (param->property) {
	case ISP_PRO_CSA_BLOCK:
		ret = isp_k_csa_block(param);
		break;
	case ISP_PRO_CSA_BYPASS:
		ret = isp_k_csa_bypass(param);
		break;
	case ISP_PRO_CSA_FACTOR:
		ret = isp_k_csa_factor(param);
		break;
	default:
		printk("isp_k_cfg_csa: fail cmd id:%d, not supported.\n", param->property);
		break;
	}

	return ret;
}
