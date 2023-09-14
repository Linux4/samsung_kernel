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

static int32_t isp_k_raw_sizer_bypass(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t bypass = 0;

	ret = copy_from_user((void *)&bypass, param->property_param, sizeof(bypass));
	if (0 != ret) {
		printk("isp_k_raw_sizer_bypass: copy_from_user error, ret = 0x%x\n", (uint32_t)ret);
		return -1;
	}

	if (bypass) {
		REG_OWR(ISP_RAW_SIZER_PARAM, BIT_0);
	} else {
		REG_MWR(ISP_RAW_SIZER_PARAM, BIT_0, 0);
	}

	return ret;
}

static int32_t isp_k_raw_sizer_bpc_bypass(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t bypass = 0;

	ret = copy_from_user((void *)&bypass, param->property_param, sizeof(bypass));
	if (0 != ret) {
		printk("isp_k_raw_sizer_bpc_bypass: copy_from_user error, ret = 0x%x\n", (uint32_t)ret);
		return -1;
	}

	if (bypass) {
		REG_OWR(ISP_RAW_SIZER_PARAM, BIT_1);
	} else {
		REG_MWR(ISP_RAW_SIZER_PARAM, BIT_1, 0);
	}

	return ret;
}

static int32_t isp_k_raw_sizer_crop_bypass(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t bypass = 0;

	ret = copy_from_user((void *)&bypass, param->property_param, sizeof(bypass));
	if (0 != ret) {
		printk("isp_k_raw_sizer_crop_bypass: copy_from_user error, ret = 0x%x\n", (uint32_t)ret);
		return -1;
	}

	if (bypass) {
		REG_OWR(ISP_RAW_SIZER_PARAM, BIT_2);
	} else {
		REG_MWR(ISP_RAW_SIZER_PARAM, BIT_2, 0);
	}

	return ret;
}

static int32_t isp_k_raw_sizer_crop_src_size(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t val = 0;
	struct isp_img_size src_size;

	ret = copy_from_user((void *)&src_size, param->property_param, sizeof(src_size));
	if (0 != ret) {
		printk("isp_k_raw_sizer_crop_src_size: copy_from_user error, ret = 0x%x\n", (uint32_t)ret);
		return -1;
	}

	val = (src_size.height & 0xFFFF)
		| ((src_size.width & 0xFFFF) << 16);
	REG_WR(ISP_RAW_SIZER_CROP_SRC, val);

	return ret;
}

static int32_t isp_k_raw_sizer_crop_dest_size(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t val = 0;
	struct isp_img_size dest_size;

	ret = copy_from_user((void *)&dest_size, param->property_param, sizeof(dest_size));
	if (0 != ret) {
		printk("isp_k_raw_sizer_crop_dest_size: copy_from_user error, ret = 0x%x\n", (uint32_t)ret);
		return -1;
	}

	val = (dest_size.height & 0xFFFF)
		| ((dest_size.width & 0xFFFF) << 16);
	REG_WR(ISP_RAW_SIZER_CROP_DST, val);

	return ret;
}

static int32_t isp_k_raw_sizer_crop_start(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t val = 0;
	struct isp_img_offset crop_start;

	ret = copy_from_user((void *)&crop_start, param->property_param, sizeof(crop_start));
	if (0 != ret) {
		printk("isp_k_raw_sizer_crop_start: copy_from_user error, ret = 0x%x\n", (uint32_t)ret);
		return -1;
	}

	val = (crop_start.y& 0xFFFF)
		| ((crop_start.x & 0xFFFF) << 16);
	REG_WR(ISP_RAW_SIZER_CROP_START, val);

	return ret;
}

static int32_t isp_k_raw_sizer_dest_size(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t val = 0;
	struct isp_img_size dest_size;

	ret = copy_from_user((void *)&dest_size, param->property_param, sizeof(dest_size));
	if (0 != ret) {
		printk("isp_k_raw_sizer_dest_size: copy_from_user error, ret = 0x%x\n", (uint32_t)ret);
		return -1;
	}

	val = (dest_size.height & 0xFFFF)
		| ((dest_size.width & 0xFFFF) << 16);
	REG_WR(ISP_RAW_SIZER_DST, val);

	return ret;
}

static int32_t isp_k_raw_sizer_bpc_shift_flactor(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t shift = 0;

	ret = copy_from_user((void *)&shift, param->property_param, sizeof(shift));
	if (0 != ret) {
		printk("isp_k_raw_sizer_bpc_shift_flactor: copy_from_user error, ret = 0x%x\n", (uint32_t)ret);
		return -1;
	}

	REG_MWR(ISP_RAW_SIZER_BPC, 0x7, shift);

	return ret;
}

static int32_t isp_k_raw_sizer_bpc_multi_flactor(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t mult_flactor = 0;

	ret = copy_from_user((void *)&mult_flactor, param->property_param, sizeof(mult_flactor));
	if (0 != ret) {
		printk("isp_k_raw_sizer_bpc_multi_flactor: copy_from_user error, ret = 0x%x\n", (uint32_t)ret);
		return -1;
	}

	REG_MWR(ISP_RAW_SIZER_BPC, (0x7 << 4), (mult_flactor << 4));

	return ret;
}

static int32_t isp_k_raw_sizer_bpc_min_diff(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t min_diff = 0;

	ret = copy_from_user((void *)&min_diff, param->property_param, sizeof(min_diff));
	if (0 != ret) {
		printk("isp_k_raw_sizer_bpc_multi_flactor: copy_from_user error, ret = 0x%x\n", (uint32_t)ret);
		return -1;
	}

	REG_MWR(ISP_RAW_SIZER_BPC, (0xFF << 8), (min_diff << 8));

	return ret;
}

static int32_t isp_k_raw_sizer_hcoeff(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t coeff[64] ;
	uint32_t i = 0 ;
	uint32_t val = 0 ;

	memset(&coeff, 0, sizeof(coeff));
	ret = copy_from_user((void *)&coeff, param->property_param, sizeof(coeff));
	if (0 != ret) {
		printk("isp_k_raw_sizer_hcoeff: copy_from_user error, ret = 0x%x\n", (uint32_t)ret);
		return -1;
	}

	for (i = 0; i < 21; i++) {
		val = (coeff[i * 3] & 0x1FF)
			| ((coeff[i * 3 + 1] & 0x1FF) << 9)
			| ((coeff[i * 3 + 2] & 0x1FF) << 18);
		REG_WR(ISP_RAW_SIZER_HCOEFF0 + i * 4, val);
	}
	val = (coeff[i * 3] & 0x1FF);
	REG_WR( ISP_RAW_SIZER_HCOEFF0 + i * 4, val);

	return ret;
}

static int32_t isp_k_raw_sizer_vcoeff(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t coeff[32] ;
	uint32_t i = 0 ;
	uint32_t val = 0 ;

	memset(&coeff, 0, sizeof(coeff));
	ret = copy_from_user((void *)&coeff, param->property_param, sizeof(coeff));
	if (0 != ret) {
		printk("isp_k_raw_sizer_vcoeff: copy_from_user error, ret = 0x%x\n", (uint32_t)ret);
		return -1;
	}

	for (i = 0; i < 10; i++) {
		val = (coeff[i * 3] & 0x1FF)
			| ((coeff[i * 3 + 1] & 0x1FF) << 9)
			| ((coeff[i * 3 + 2] & 0x1FF) << 18);
		REG_WR(ISP_RAW_SIZER_VCOEFF0 + i * 4, val);
	}
	val = (coeff[i * 3] & 0x1FF)
		| ((coeff[i * 3 + 1] & 0x1FF) << 9);
	REG_WR( ISP_RAW_SIZER_VCOEFF0 + i * 4, val);

	return ret;
}

static int32_t isp_k_raw_sizer_h_init_para(struct isp_io_param *param)
{
	int32_t ret = 0;
	struct isp_raw_sizer_init_para init_para;
	uint32_t val = 0 ;

	ret = copy_from_user((void *)&init_para, param->property_param, sizeof(init_para));
	if (0 != ret) {
		printk("isp_k_raw_sizer_h_init_para: copy_from_user error, ret = 0x%x\n", (uint32_t)ret);
		return -1;
	}

	val = (init_para.residual_half_wd & 0xFFFF) | ((init_para.half_dst_wd_remain & 0xFFFF) << 16);
	REG_WR(ISP_RAW_SIZER_INIT_HOR0, val);

	val = (init_para.adv_a_init_residual & 0xFFFF) | ((init_para.adv_b_init_residual & 0xFFFF) << 16);
	REG_WR(ISP_RAW_SIZER_INIT_HOR1, val);

	val = (init_para.offset_base_incr & 0xFF) | ((init_para.offset_incr_init_a & 0xFF) << 8)
	| ((init_para.offset_incr_init_b & 0xFF) << 16);
	REG_WR( ISP_RAW_SIZER_INIT_HOR2, val);

	return ret;
}

static int32_t isp_k_raw_sizer_v_init_para(struct isp_io_param *param)
{
	int32_t ret = 0;
	struct isp_raw_sizer_init_para init_para;
	uint32_t val = 0 ;

	ret = copy_from_user((void *)&init_para, param->property_param, sizeof(init_para));
	if (0 != ret) {
		printk("isp_k_raw_sizer_v_init_para: copy_from_user error, ret = 0x%x\n", (uint32_t)ret);
		return -1;
	}

	val = (init_para.residual_half_wd & 0xFFFF) | ((init_para.half_dst_wd_remain & 0xFFFF) << 16);
	REG_WR(ISP_RAW_SIZER_INIT_VER0, val);

	val = (init_para.adv_a_init_residual & 0xFFFF) | ((init_para.adv_b_init_residual & 0xFFFF) << 16);
	REG_WR(ISP_RAW_SIZER_INIT_VER1, val);

	val = (init_para.offset_base_incr & 0xFF) | ((init_para.offset_incr_init_a & 0xFF) << 8)
	| ((init_para.offset_incr_init_b & 0xFF) << 16);
	REG_WR( ISP_RAW_SIZER_INIT_VER2, val);

	return ret;
}

int32_t isp_k_cfg_raw_sizer(struct isp_io_param *param)
{
	int32_t ret = 0;

	if (!param) {
		printk("isp_k_cfg_raw_sizer: param is null error.\n");
		return -1;
	}

	if (NULL == param->property_param) {
		printk("isp_k_cfg_raw_sizer: property_param is null error.\n");
		return -1;
	}

	switch (param->property) {
	case ISP_PRO_RAW_SIZER_BYPASS:
		ret = isp_k_raw_sizer_bypass(param);
		break;
	case ISP_PRO_RAW_SIZER_BPC_BYPASS:
		ret = isp_k_raw_sizer_bpc_bypass(param);
		break;
	case ISP_PRO_RAW_SIZER_CROP_BYPASS:
		ret = isp_k_raw_sizer_crop_bypass(param);
		break;
	case ISP_PRO_RAW_SIZER_CROP_SRC:
		ret = isp_k_raw_sizer_crop_src_size(param);
		break;
	case ISP_PRO_RAW_SIZER_CROP_DST:
		ret = isp_k_raw_sizer_crop_dest_size(param);
		break;
	case ISP_PRO_RAW_SIZER_CROP_START:
		ret = isp_k_raw_sizer_crop_start(param);
		break;
	case ISP_PRO_RAW_SIZER_DST:
		ret = isp_k_raw_sizer_dest_size(param);
		break;
	case ISP_PRO_RAW_SIZER_BPC_MULTI:
		ret = isp_k_raw_sizer_bpc_multi_flactor(param);
		break;
	case ISP_PRO_RAW_SIZER_BPC_SHIFT:
		ret = isp_k_raw_sizer_bpc_shift_flactor(param);
		break;
	case ISP_PRO_RAW_SIZER_BPC_MIN_DIFF:
		ret = isp_k_raw_sizer_bpc_min_diff(param);
		break;
	case ISP_PRO_RAW_SIZER_HCOEFF:
		ret = isp_k_raw_sizer_hcoeff(param);
		break;
	case ISP_PRO_RAW_SIZER_VCOEFF:
		ret = isp_k_raw_sizer_vcoeff(param);
		break;
	case ISP_PRO_RAW_SIZER_H_INIT_PARA:
		ret = isp_k_raw_sizer_h_init_para(param);
		break;
	case ISP_PRO_RAW_SIZER_V_INIT_PARA:
		ret = isp_k_raw_sizer_v_init_para(param);
		break;
	default:
		printk("isp_k_cfg_raw_sizer: fail cmd id:%d, not supported.\n", param->property);
		break;
	}

	return ret;
}
