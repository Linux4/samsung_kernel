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

static int32_t isp_k_emboss_block(struct isp_io_param *param)
{
	int32_t ret = 0;
	struct isp_dev_emboss_info emboss_info;

	memset(&emboss_info, 0x00, sizeof(emboss_info));

	ret = copy_from_user((void *)&emboss_info, param->property_param, sizeof(emboss_info));
	if (0 != ret) {
		printk("isp_k_emboss_block: copy error, ret=0x%x\n", (uint32_t)ret);
		return -1;
	}

	REG_MWR(ISP_EMBOSS_PARAM, 0x7E, (emboss_info.step << 1));

	if (emboss_info.bypass) {
		REG_OWR(ISP_EMBOSS_PARAM, BIT_0);
	} else {
		REG_MWR(ISP_EMBOSS_PARAM, BIT_0, 0);
	}

	return ret;
}

static int32_t isp_k_emboss_bypass(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t bypass = 0;

	ret = copy_from_user((void *)&bypass, param->property_param, sizeof(bypass));
	if (0 != ret) {
		printk("isp_k_emboss_bypass: copy error, ret=0x%x\n", (uint32_t)ret);
		return -1;
	}

	if (bypass) {
		REG_OWR(ISP_EMBOSS_PARAM, BIT_0);
	} else {
		REG_MWR(ISP_EMBOSS_PARAM, BIT_0, 0);
	}

	return ret;
}

static int32_t isp_k_emboss_param(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t step = 0;

	ret = copy_from_user((void *)&step, param->property_param, sizeof(step));
	if (0 != ret) {
		printk("isp_k_emboss_param: copy error, ret=0x%x\n", (uint32_t)ret);
		return -1;
	}

	REG_MWR(ISP_EMBOSS_PARAM, 0x7E, (step << 1));

	return ret;
}

int32_t isp_k_cfg_emboss(struct isp_io_param *param)
{
	int32_t ret = 0;

	if (!param) {
		printk("isp_k_cfg_emboss: param is null error.\n");
		return -1;
	}

	if (NULL == param->property_param) {
		printk("isp_k_cfg_emboss: property_param is null error.\n");
		return -1;
	}

	switch (param->property) {
	case ISP_PRO_EMBOSS_BLOCK:
		ret = isp_k_emboss_block(param);
		break;
	case ISP_PRO_EMBOSS_BYPASS:
		ret = isp_k_emboss_bypass(param);
		break;
	case ISP_PRO_EMBOSS_PARAM:
		ret = isp_k_emboss_param(param);
		break;
	default:
		printk("isp_k_cfg_emboss: fail cmd id:%d, not supported.\n", param->property);
		break;
	}

	return ret;
}
