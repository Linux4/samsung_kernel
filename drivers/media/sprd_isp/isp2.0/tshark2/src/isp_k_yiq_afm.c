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

#define ISP_AFM_WIN_NUM_V1  25

static int32_t isp_k_yiq_afm_block(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t val = 0;
	uint32_t i = 0;
	int max_num = ISP_AFM_WIN_NUM_V1;
	struct isp_dev_yiq_afm_info_v1 afm_info;

	memset(&afm_info, 0x00, sizeof(afm_info));

	ret = copy_from_user((void *)&afm_info, param->property_param, sizeof(afm_info));
	if (0 != ret) {
		printk("isp_k_yiq_afm_block: copy error, ret=0x%x\n", (uint32_t)ret);
		return -1;
	}

	REG_MWR(ISP_YIQ_AFM_PARAM, BIT_6, afm_info.mode << 6);

	REG_MWR(ISP_COMMON_3A_CTRL0, BIT_0, afm_info.source_pos);

	REG_MWR(ISP_YIQ_AFM_PARAM, 0x1F<<1, (afm_info.shift << 1));

	REG_MWR(ISP_YIQ_AFM_PARAM, 0xF<<7, (afm_info.skip_num << 7));

	REG_MWR(ISP_YIQ_AFM_PARAM, BIT_11, afm_info.skip_num_clear << 11);

	REG_MWR(ISP_YIQ_AFM_PARAM, 0x7<<13, (afm_info.format << 13));

	REG_MWR(ISP_YIQ_AFM_PARAM, BIT_16, afm_info.iir_bypass << 16);

	for (i = 0; i < max_num; i++) {
		val = ((afm_info.coord[i].start_y <<16) & 0xffff0000)
				| (afm_info.coord[i].start_x & 0xffff);
		REG_WR((ISP_YIQ_AFM_WIN_RANGE0 + 4*2*i), val);

		val = ((afm_info.coord[i].end_y <<16) & 0xffff0000)
			| (afm_info.coord[i].end_x & 0xffff);
		REG_WR((ISP_YIQ_AFM_WIN_RANGE0 + 4*(2*i+1)), val);
	}

	for (i = 0; i < 10; i+= 2) {
		val = (afm_info.IIR_c[i] & 0xFFF) | ((afm_info.IIR_c[i + 1] & 0xFFF) << 16);
		REG_WR(ISP_YIQ_AFM_COEF0_1 + (i / 2) * 4, val);
	}
	val = afm_info.IIR_c[10] & 0xFFF;
	REG_WR(ISP_YIQ_AFM_COEF10, val);

	REG_MWR(ISP_YIQ_AFM_PARAM, BIT_0, afm_info.bypass);

	return ret;

}

static int32_t isp_k_yiq_afm_statistic(struct isp_io_param *param)
{
	int32_t ret = 0;
	int i = 0;
	int max_item = ISP_AFM_WIN_NUM_V1*4;
	struct isp_yiq_afm_statistic item;

	memset(&item, 0, sizeof(struct isp_yiq_afm_statistic));
	for (i = 0; i < max_item; i++) {
		item.val[i] = REG_RD((ISP_YIQ_AFM_LAPLACE0 + 4*i));
	}
	ret = copy_to_user(param->property_param, (void*)&item, sizeof(item));
	if (0 != ret) {
		ret = -1;
		printk("isp_k_yiq_afm_statistic: copy_to_user error, ret = 0x%x\n", (uint32_t)ret);
	}

	return ret;
}

static int32_t isp_k_yiq_afm_slice_size(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t val = 0;
	struct isp_img_size slice_size= {0, 0};

	ret = copy_from_user((void *)&slice_size, param->property_param, sizeof(slice_size));
	if (0 != ret) {
		printk("isp_k_ae_skip_num: copy_from_user error, ret = 0x%x\n", (uint32_t)ret);
		return -1;
	}

	val = (slice_size.width&  0xFFFF) | ((slice_size.height&  0xFFFF) << 16);
	REG_WR( ISP_YIQ_AFM_SLICE_SIZE, val);


	return ret;
}

static int32_t isp_k_yiq_afm_win(struct isp_io_param *param)
{
	int32_t ret = 0;
	int i = 0;
	int max_num = ISP_AFM_WIN_NUM_V1;
	struct isp_coord coord[ISP_AFM_WIN_NUM_V1];
	uint32_t val = 0;


	memset(coord, 0, sizeof(coord));
	ret = copy_from_user((void *)&coord, param->property_param, sizeof(coord));
	if (0 != ret) {
		printk("isp_k_yiq_afm_win: read copy_from_user error, ret = 0x%x\n", (uint32_t)ret);
		return -1;
	}

	for (i = 0; i < max_num; i++) {
		val = ((coord[i].start_y <<16) & 0xffff0000)
				| (coord[i].start_x & 0xffff);
		REG_WR((ISP_YIQ_AFM_WIN_RANGE0 + 4*2*i), val);

		val = ((coord[i].end_y <<16) & 0xffff0000)
			| (coord[i].end_x & 0xffff);
		REG_WR((ISP_YIQ_AFM_WIN_RANGE0 + 4*(2*i+1)), val);
	}

	return ret;
}

static int32_t isp_k_yiq_afm_win_num(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t num = ISP_AFM_WIN_NUM_V1;

	ret = copy_to_user(param->property_param, (void*)&num, sizeof(num));
	if (0 != ret) {
		ret = -1;
		printk("isp_k_yiq_afm_win_num: copy_to_user error, ret = 0x%x\n", (uint32_t)ret);
	}

	return ret;
}


int32_t isp_k_cfg_yiq_afm(struct isp_io_param *param)
{
	int32_t ret = 0;

	if (!param) {
		printk("isp_k_cfg_afm: param is null error.\n");
		return -1;
	}

	if (NULL == param->property_param) {
		printk("isp_k_cfg_afm: property_param is null error.\n");
		return -1;
	}

	switch (param->property) {
	case ISP_PRO_YIQ_AFM_BLOCK:
		ret = isp_k_yiq_afm_block(param);
		break;
	case ISP_PRO_YIQ_AFM_SLICE_SIZE:
		ret = isp_k_yiq_afm_slice_size(param);
		break;
	case ISP_PRO_YIQ_AFM_STATISTIC:
		ret = isp_k_yiq_afm_statistic(param);
		break;
	case ISP_PRO_YIQ_AFM_WIN_NUM:
		ret = isp_k_yiq_afm_win_num(param);
		break;
	case ISP_PRO_YIQ_AFM_WIN:
		ret = isp_k_yiq_afm_win(param);
		break;
	default:
		printk("isp_k_cfg_yiq_afm: fail cmd id:%d, not supported.\n", param->property);
		break;
	}

	return ret;
}
