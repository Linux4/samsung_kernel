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

static int32_t isp_k_cce_block_uv(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t val = 0;
	struct isp_dev_cce_uvd_info cce_uvd_info;

	memset(&cce_uvd_info, 0x00, sizeof(cce_uvd_info));

	ret = copy_from_user((void *)&cce_uvd_info, param->property_param, sizeof(cce_uvd_info));
	if (0 != ret) {
		printk("isp_k_cce_block_uv: copy error, ret=0x%x\n", (uint32_t)ret);
		return -1;
	}

	val = (cce_uvd_info.lum_th_h_len & 0xF)
		| ((cce_uvd_info.lum_th_h & 0xFF) << 8)
		| ((cce_uvd_info.lum_th_l_len & 0xF) << 16)
		| ((cce_uvd_info.lum_th_l & 0xFF) << 24);
	REG_WR(ISP_CCE_UVD_PARAM0, val);

	val = (cce_uvd_info.chroma_min_h & 0xFF)
		| ((cce_uvd_info.chroma_min_l & 0xFF) << 8)
		| ((cce_uvd_info.chroma_max_h & 0xFF) << 16)
		| ((cce_uvd_info.chroma_max_l & 0xFF) << 24);
	REG_WR(ISP_CCE_UVD_PARAM1, val);

	val = (cce_uvd_info.u_th1_h & 0xFF)
		| ((cce_uvd_info.u_th1_l & 0xFF) << 8)
		| ((cce_uvd_info.u_th0_h & 0xFF) << 16)
		| ((cce_uvd_info.u_th0_l & 0xFF) << 24);
	REG_WR(ISP_CCE_UVD_PARAM2, val);

	val = (cce_uvd_info.v_th1_h & 0xFF)
		| ((cce_uvd_info.v_th1_l & 0xFF) << 8)
		| ((cce_uvd_info.v_th0_h & 0xFF) << 16)
		| ((cce_uvd_info.v_th0_l & 0xFF) << 24);
	REG_WR(ISP_CCE_UVD_PARAM3, val);

	val = (cce_uvd_info.ratio[7] & 0xF)
		| ((cce_uvd_info.ratio[6] & 0xF) << 4)
		| ((cce_uvd_info.ratio[5] & 0xF) << 8)
		| ((cce_uvd_info.ratio[4] & 0xF) << 12)
		| ((cce_uvd_info.ratio[3] & 0xF) << 16)
		| ((cce_uvd_info.ratio[2] & 0xF) << 20)
		| ((cce_uvd_info.ratio[1] & 0xF) << 24)
		| ((cce_uvd_info.ratio[0] & 0xF) << 28);
	REG_WR(ISP_CCE_UVD_PARAM4, val);

	val = (cce_uvd_info.base & 0xF)
		| ((cce_uvd_info.ratio[8] & 0xF) << 8);
	REG_WR(ISP_CCE_UVD_PARAM5, val);

	REG_MWR(ISP_CCE_PARAM, BIT_0, cce_uvd_info.uvdiv_bypass);

	return ret;
}

static int32_t isp_k_cce_block_matrix(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t val = 0;
	uint32_t val1 = 0;
	struct isp_dev_cce_info_v1 cce_info;

	memset(&cce_info, 0x00, sizeof(cce_info));

	ret = copy_from_user((void *)&cce_info, param->property_param, sizeof(cce_info));
	if (0 != ret) {
		printk("isp_k_cce_block_matrix: copy error, ret=0x%x\n", (uint32_t)ret);
		return -1;
	}

	val = ((cce_info.matrix[1] & 0x7FF) << 11) | (cce_info.matrix[0] & 0x7FF);
	val1 = ((cce_info.matrix[3] & 0x7FF) << 11) | (cce_info.matrix[2] & 0x7FF);
	if ((0 == val)
		|| (0 == val1)) {
		printk("isp_k_cce_block_matrix: copy error, 0x%x, 0x%x, 0x%x, 0x%x\n", cce_info.matrix[0], cce_info.matrix[1], cce_info.matrix[2], cce_info.matrix[3]);
		return ret;
	}

	REG_MWR(ISP_CCE_PARAM, BIT_1, cce_info.mode << 1);

	val = ((cce_info.matrix[1] & 0x7FF) << 11) | (cce_info.matrix[0] & 0x7FF);
	REG_WR(ISP_CCE_MATRIX0, val);
	val = ((cce_info.matrix[3] & 0x7FF) << 11) | (cce_info.matrix[2] & 0x7FF);
	REG_WR(ISP_CCE_MATRIX1, val);
	val = ((cce_info.matrix[5] & 0x7FF) << 11) | (cce_info.matrix[4] & 0x7FF);
	REG_WR(ISP_CCE_MATRIX2, val);
	val = ((cce_info.matrix[7] & 0x7FF) << 11) | (cce_info.matrix[6] & 0x7FF);
	REG_WR(ISP_CCE_MATRIX3, val);
	val = cce_info.matrix[8] & 0x7FF;
	REG_WR(ISP_CCE_MATRIX4, val);

	val = (cce_info.y_offset & 0x1FF) | ((cce_info.u_offset & 0x1FF) << 9)
		| ((cce_info.v_offset & 0x1FF) << 18);
	REG_WR(ISP_CCE_SHIFT, (val & 0x7FFFFFF));

	return ret;
}

int32_t isp_k_cfg_cce(struct isp_io_param *param)
{
	int32_t ret = 0;

	if (!param) {
		printk("isp_k_cfg_cce: param is null error.\n");
		return -1;
	}

	if (NULL == param->property_param) {
		printk("isp_k_cfg_cce: property_param is null error.\n");
		return -1;
	}

	switch(param->property) {
	case ISP_PRO_CCE_BLOCK_MATRIX:
		ret = isp_k_cce_block_matrix(param);
		break;
	case ISP_PRO_CCE_BLOCK_UV:
		ret = isp_k_cce_block_uv(param);
		break;
	default:
		printk("isp_k_cfg_cce: fail cmd id:%d, not supported.\n", param->property);
		break;
	}

	return ret;
}
