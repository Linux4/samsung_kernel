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

static int32_t isp_k_cce_block_matrix(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t val = 0;
	struct isp_dev_cce_matrix_info cce_matrix_info;

	memset(&cce_matrix_info, 0x00, sizeof(cce_matrix_info));

	ret = copy_from_user((void *)&cce_matrix_info, param->property_param, sizeof(cce_matrix_info));
	if (0 != ret) {
		printk("isp_k_cce_block_matrix: copy error, ret=0x%x\n", (uint32_t)ret);
		return -1;
	}

	val = ((cce_matrix_info.matrix[1] & 0x7FF) << 11)
		| (cce_matrix_info.matrix[0] & 0x7FF);
	REG_WR(ISP_CCE_MATRIX0, (val & 0x3FFFFF));

	val = ((cce_matrix_info.matrix[3] & 0x7FF) << 11)
		| (cce_matrix_info.matrix[2] & 0x7FF);
	REG_WR(ISP_CCE_MATRIX1, (val & 0x3FFFFF));

	val = ((cce_matrix_info.matrix[5] & 0x7FF) << 11)
		| (cce_matrix_info.matrix[4] & 0x7FF);
	REG_WR(ISP_CCE_MATRIX2, (val & 0x3FFFFF));

	val = ((cce_matrix_info.matrix[7] & 0x7FF) << 11)
		| (cce_matrix_info.matrix[6] & 0x7FF);
	REG_WR(ISP_CCE_MATRIX3, (val & 0x3FFFFF));

	val = cce_matrix_info.matrix[8] & 0x7FF;
	REG_WR(ISP_CCE_MATRIX4, val);

	val = ((cce_matrix_info.v_shift & 0x1FF) << 18)
		| ((cce_matrix_info.u_shift & 0x1FF) << 9)
		| (cce_matrix_info.y_shift & 0x1FF);

	REG_WR(ISP_CCE_SHIFT, (val & 0x7FFFFFF));

	return ret;
}

static int32_t isp_k_cce_block_uv(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t val = 0;
	struct isp_dev_cce_uv_info cce_uv_info;

	memset(&cce_uv_info, 0x00, sizeof(cce_uv_info));

	ret = copy_from_user((void *)&cce_uv_info, param->property_param, sizeof(cce_uv_info));
	if (0 != ret) {
		printk("isp_k_cce_block_uv: copy error, ret=0x%x\n", (uint32_t)ret);
		return -1;
	}


	val = (cce_uv_info.uvd[3] << 24)
		| (cce_uv_info.uvd[2] << 16)
		| (cce_uv_info.uvd[1] << 8)
		| cce_uv_info.uvd[0];
	REG_WR(ISP_CCE_UVD_THRD0, val);

	val = (cce_uv_info.uvd[6] << 16)
		| (cce_uv_info.uvd[5] << 8)
		| cce_uv_info.uvd[4];
	REG_WR(ISP_CCE_UVD_THRD1, (val & 0xFFFFFF));

	val = (cce_uv_info.uvc0[0] << 8)
		| cce_uv_info.uvc0[1];
	REG_WR(ISP_CCE_UVC_PARAM0, val);

	val =  (cce_uv_info.uvc1[0] << 16)
		| (cce_uv_info.uvc1[1] << 8)
		| cce_uv_info.uvc1[2];
	REG_WR(ISP_CCE_UVC_PARAM1, val);

	if (cce_uv_info.bypass) {
		REG_OWR(ISP_CCE_PARAM, BIT_1);
	} else {
		REG_MWR(ISP_CCE_PARAM, BIT_1, 0);
	}

	return ret;
}

static int32_t isp_k_cce_uvdivision_bypass(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t bypass = 0;

	ret = copy_from_user((void *)&bypass, param->property_param, sizeof(bypass));
	if (0 != ret) {
		printk("isp_k_cce_uvdivision_bypass: copy error, ret=0x%x\n", (uint32_t)ret);
		return -1;
	}

	if (bypass) {
		REG_OWR(ISP_CCE_PARAM, BIT_1);
	} else {
		REG_MWR(ISP_CCE_PARAM, BIT_1, 0);
	}

	return ret;
}

static int32_t isp_k_cce_mode(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t mode = 0;

	ret = copy_from_user((void *)&mode, param->property_param, sizeof(mode));
	if (0 != ret) {
		printk("isp_k_cce_mode: copy error, ret=0x%x\n", (uint32_t)ret);
		return -1;
	}

	if (mode) {
		REG_OWR(ISP_CCE_PARAM, BIT_2);
	} else {
		REG_MWR(ISP_CCE_PARAM, BIT_2, 0);
	}

	return ret;
}

static int32_t isp_k_cce_matrix(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t val = 0;
	struct isp_cce_matrix_tab matrix_tab;

	memset(&matrix_tab, 0x00, sizeof(matrix_tab));

	ret = copy_from_user((void *)&(matrix_tab.matrix), param->property_param, sizeof(matrix_tab.matrix));
	if (0 != ret) {
		printk("isp_k_cce_matrix: copy error, ret=0x%x\n", (uint32_t)ret);
		return -1;
	}

	val = ((matrix_tab.matrix[1] & 0x7FF) << 11)
		| (matrix_tab.matrix[0] & 0x7FF);
	REG_WR(ISP_CCE_MATRIX0, (val & 0x3FFFFF));

	val = ((matrix_tab.matrix[3] & 0x7FF) << 11)
		| (matrix_tab.matrix[2] & 0x7FF);
	REG_WR(ISP_CCE_MATRIX1, (val & 0x3FFFFF));

	val = ((matrix_tab.matrix[5] & 0x7FF) << 11)
		| (matrix_tab.matrix[4] & 0x7FF);
	REG_WR(ISP_CCE_MATRIX2, (val & 0x3FFFFF));

	val = ((matrix_tab.matrix[7] & 0x7FF) << 11)
		| (matrix_tab.matrix[6] & 0x7FF);
	REG_WR(ISP_CCE_MATRIX3, (val & 0x3FFFFF));

	val = matrix_tab.matrix[8] & 0x7FF;
	REG_WR(ISP_CCE_MATRIX4, val);

	return ret;
}

static int32_t isp_k_cce_shift(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t val = 0;
	struct isp_cce_shift cce_shift;

	memset(&cce_shift, 0x00, sizeof(cce_shift));

	ret = copy_from_user((void *)&cce_shift, param->property_param, sizeof(cce_shift));
	if (0 != ret) {
		printk("isp_k_cce_shift: copy error, ret=0x%x\n", (uint32_t)ret);
		return -1;
	}

	val = ((cce_shift.v_shift & 0x1FF) << 18)
		| ((cce_shift.u_shift & 0x1FF) << 9)
		| (cce_shift.y_shift & 0x1FF);

	REG_WR(ISP_CCE_SHIFT, (val & 0x7FFFFFF));

	return ret;
}

static int32_t isp_k_cce_uvd(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t val = 0;
	struct isp_cce_uvd cce_uvd;

	memset(&cce_uvd, 0x00, sizeof(cce_uvd));

	ret = copy_from_user((void *)&(cce_uvd.uvd), param->property_param, sizeof(cce_uvd.uvd));
	if (0 != ret) {
		printk("isp_k_cce_uvd: copy error, ret=0x%x\n", (uint32_t)ret);
		return -1;
	}

	val = (cce_uvd.uvd[3] << 24)
		| (cce_uvd.uvd[2] << 16)
		| (cce_uvd.uvd[1] << 8)
		| cce_uvd.uvd[0];
	REG_WR(ISP_CCE_UVD_THRD0, val);

	val = (cce_uvd.uvd[6] << 16)
		| (cce_uvd.uvd[5] << 8)
		| cce_uvd.uvd[4];
	REG_WR(ISP_CCE_UVD_THRD1, (val & 0xFFFFFF));

	return ret;
}

static int32_t isp_k_cce_uvc(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t val = 0;
	struct isp_cce_uvc cce_uvc;

	memset(&cce_uvc, 0x00, sizeof(cce_uvc));

	val = sizeof(cce_uvc.uvc0) + sizeof(cce_uvc.uvc1);
	ret = copy_from_user((void *)&cce_uvc, param->property_param, sizeof(cce_uvc));
	if (0 != ret) {
		printk("isp_k_cce_uvc: copy error, ret=0x%x\n", (uint32_t)ret);
		return -1;
	}

	val = (cce_uvc.uvc0[0] << 8)
		| cce_uvc.uvc0[1];
	REG_WR(ISP_CCE_UVC_PARAM0, val);

	val =  (cce_uvc.uvc1[0] << 16)
		| (cce_uvc.uvc1[1] << 8)
		| cce_uvc.uvc1[2];
	REG_WR(ISP_CCE_UVC_PARAM1, val);

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
	case ISP_PRO_CCE_UVDIVISION_BYPASS:
		ret = isp_k_cce_uvdivision_bypass(param);
		break;
	case ISP_PRO_CCE_MODE:
		ret = isp_k_cce_mode(param);
		break;
	case ISP_PRO_CCE_MATRIX:
		ret = isp_k_cce_matrix(param);
		break;
	case ISP_PRO_CCE_SHIFT:
		ret = isp_k_cce_shift(param);
		break;
	case ISP_PRO_CCE_UVD_THRD:
		ret = isp_k_cce_uvd(param);
		break;
	case ISP_PRO_CCE_UVC_PARAM:
		ret = isp_k_cce_uvc(param);
		break;
	default:
		printk("isp_k_cfg_cce: fail cmd id:%d, not supported.\n", param->property);
		break;
	}

	return ret;
}
