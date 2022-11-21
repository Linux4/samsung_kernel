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

static int32_t isp_k_cmc_block(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t val = 0;
	struct isp_dev_cmc_info cmc_info;

	memset(&cmc_info, 0x00, sizeof(cmc_info));

	ret = copy_from_user((void *)&cmc_info, param->property_param, sizeof(cmc_info));
	if (0 != ret) {
		printk("isp_k_cmc_block: copy error, ret=0x%x\n", (uint32_t)ret);
		return -1;
	}

	val = ((cmc_info.matrix[1] & 0x3FFF) << 14) | (cmc_info.matrix[0] & 0x3FFF);
	REG_WR(ISP_CMC_MATRIX0, val);

	val = ((cmc_info.matrix[3] & 0x3FFF) << 14) | (cmc_info.matrix[2] & 0x3FFF);
	REG_WR(ISP_CMC_MATRIX1, val);

	val = ((cmc_info.matrix[5] & 0x3FFF) << 14) | (cmc_info.matrix[4] & 0x3FFF);
	REG_WR(ISP_CMC_MATRIX2, val);

	val = ((cmc_info.matrix[7] & 0x3FFF) << 14) | (cmc_info.matrix[6] & 0x3FFF);
	REG_WR(ISP_CMC_MATRIX3, val);

	val = cmc_info.matrix[8] & 0x3FFF;
	REG_WR(ISP_CMC_MATRIX4, val);


	if (cmc_info.bypass) {
		REG_OWR(ISP_CMC_PARAM, BIT_0);
	} else {
		REG_MWR(ISP_CMC_PARAM, BIT_0, 0);
	}

	return ret;
}

static int32_t isp_k_cmc_bypass(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t bypass = 0;

	ret = copy_from_user((void *)&bypass, param->property_param, sizeof(bypass));
	if (0 != ret) {
		printk("isp_k_cmc_bypass: copy error, ret=0x%x\n", (uint32_t)ret);
		return -1;
	}

	if (bypass) {
		REG_OWR(ISP_CMC_PARAM, BIT_0);
	} else {
		REG_MWR(ISP_CMC_PARAM, BIT_0, 0);
	}

	return ret;
}

static int32_t isp_k_cmc_matrix(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t val = 0;
	struct isp_cmc_matrix_tab matrix_tab;

	memset(&matrix_tab, 0x00, sizeof(matrix_tab));

	ret = copy_from_user((void *)&(matrix_tab.val), param->property_param, sizeof(matrix_tab.val));
	if (0 != ret) {
		printk("isp_k_cmc_matrix: copy error, ret=0x%x\n", (uint32_t)ret);
		return -1;
	}

	val = ((matrix_tab.val[1] & 0x3FFF) << 14) | (matrix_tab.val[0] & 0x3FFF);
	REG_WR(ISP_CMC_MATRIX0, val);

	val = ((matrix_tab.val[3] & 0x3FFF) << 14) | (matrix_tab.val[2] & 0x3FFF);
	REG_WR(ISP_CMC_MATRIX1, val);

	val = ((matrix_tab.val[5] & 0x3FFF) << 14) | (matrix_tab.val[4] & 0x3FFF);
	REG_WR(ISP_CMC_MATRIX2, val);

	val = ((matrix_tab.val[7] & 0x3FFF) << 14) | (matrix_tab.val[6] & 0x3FFF);
	REG_WR(ISP_CMC_MATRIX3, val);

	val = matrix_tab.val[8] & 0x3FFF;
	REG_WR(ISP_CMC_MATRIX4, val);

	return ret;
}

int32_t isp_k_cfg_cmc(struct isp_io_param *param)
{
	int32_t ret = 0;

	if (!param) {
		printk("isp_k_cfg_cmc: param is null error.\n");
		return -1;
	}

	if (NULL == param->property_param) {
		printk("isp_k_cfg_cmc: property_param is null error.\n");
		return -1;
	}

	switch(param->property) {
	case ISP_PRO_CMC_BLOCK:
		ret = isp_k_cmc_block(param);
		break;
	case ISP_PRO_CMC_BYPASS:
		ret = isp_k_cmc_bypass(param);
		break;
	case ISP_PRO_CMC_MATRIX:
		ret = isp_k_cmc_matrix(param);
		break;
	default:
		printk("isp_k_cfg_cmc: fail cmd id:%d, not supported.\n", param->property);
		break;
	}

	return ret;
}
