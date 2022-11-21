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

static int32_t isp_k_afm_block(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t val = 0;
	uint32_t i = 0;
	int max_num = ISP_AFM_WIN_NUM;
	struct isp_dev_afm_info afm_info;

	ret = copy_from_user((void *)&afm_info, param->property_param, sizeof(afm_info));
	if (0 != ret) {
		printk("isp_k_aca_block: copy error, ret=0x%x\n", (uint32_t)ret);
		return -1;
	}

	REG_MWR(ISP_AFM_PARAM, 0x3E, (afm_info.shift << 1));

	if (afm_info.mode) {
		REG_OWR(ISP_AFM_PARAM, BIT_6);
	} else {
		REG_MWR(ISP_AFM_PARAM, BIT_6, 0);
	}

	REG_MWR(ISP_AFM_PARAM, 0x780, (afm_info.skip_num << 7));

	if (afm_info.skip_num_clr) {
		REG_OWR(ISP_AFM_PARAM, BIT_11);
	} else {
		REG_MWR(ISP_AFM_PARAM, BIT_11, 0);
	}

	for (i = 0; i < max_num; i++) {
		val = ((afm_info.win[i].start_y << 16) & 0xFFF0000)
				| (afm_info.win[i].start_x & 0xfff);
		REG_WR((ISP_AFM_WIN_RANGE0 + 4 * 2 * i), val);

		val = ((afm_info.win[i].end_y << 16) & 0xFFF0000)
			| (afm_info.win[i].end_x & 0xFFF);
		REG_WR((ISP_AFM_WIN_RANGE0 + 4 * (2 * i + 1)), val);
	}

	if (afm_info.bypass) {
		REG_OWR(ISP_AFM_PARAM, BIT_0);
	} else {
		REG_MWR(ISP_AFM_PARAM, BIT_0, 0);
	}

	return ret;
}

static int32_t isp_k_afm_bypass(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t bypass = 0;

	ret = copy_from_user((void *)&bypass, param->property_param, sizeof(bypass));
	if (0 != ret) {
		printk("isp_k_afm_bypass: copy error, ret=0x%x\n", (uint32_t)ret);
		return -1;
	}

	if (bypass) {
		REG_OWR(ISP_AFM_PARAM, BIT_0);
	} else {
		REG_MWR(ISP_AFM_PARAM, BIT_0, 0);
	}

	return ret;
}

static int32_t isp_k_afm_shift(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t shift = 0;

	ret = copy_from_user((void *)&shift, param->property_param, sizeof(shift));
	if (0 != ret) {
		printk("isp_k_afm_shift: copy error, ret=0x%x\n", (uint32_t)ret);
		return -1;
	}

	REG_MWR(ISP_AFM_PARAM, 0x3E, (shift << 1));

	return ret;
}

static int32_t isp_k_afm_mode(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t mode = 0;

	ret = copy_from_user((void *)&mode, param->property_param, sizeof(mode));
	if (0 != ret) {
		printk("isp_k_afm_mode: copy error, ret=0x%x\n", (uint32_t)ret);
		return -1;
	}

	if (mode) {
		REG_OWR(ISP_AFM_PARAM, BIT_6);
	} else {
		REG_MWR(ISP_AFM_PARAM, BIT_6, 0);
	}

	return ret;
}

static int32_t isp_k_afm_skip_num(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t skip_num = 0;

	ret = copy_from_user((void *)&skip_num, param->property_param, sizeof(skip_num));
	if (0 != ret) {
		printk("isp_k_afm_skip_num: copy error, ret=0x%x\n", (uint32_t)ret);
		return -1;
	}

	REG_MWR(ISP_AFM_PARAM, 0x780, (skip_num << 7));

	return ret;
}

static int32_t isp_k_afm_skip_num_clr(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t is_clear = 0;

	ret = copy_from_user((void *)&is_clear, param->property_param, sizeof(is_clear));
	if (0 != ret) {
		printk("isp_k_afm_skip_num_clr: copy error, ret=0x%x\n", (uint32_t)ret);
		return -1;
	}

	if (is_clear) {
		REG_OWR(ISP_AFM_PARAM, BIT_11);
	} else {
		REG_MWR(ISP_AFM_PARAM, BIT_11, 0);
	}

	return ret;
}

static int32_t isp_k_afm_win(struct isp_io_param *param)
{
	int32_t ret = 0;
	int i = 0;
	int max_num = ISP_AFM_WIN_NUM;
	struct isp_img_coord coord[max_num];
	uint32_t val = 0;

	memset(coord, 0, sizeof(coord));
	ret = copy_from_user((void *)&coord, param->property_param, sizeof(coord));
	if (0 != ret) {
		printk("isp_k_afm_win: copy error, ret=0x%x\n", (uint32_t)ret);
		return -1;
	}

	for (i = 0; i < max_num; i++) {
		val = ((coord[i].start_y << 16) & 0xFFF0000)
				| (coord[i].start_x & 0xfff);
		REG_WR((ISP_AFM_WIN_RANGE0 + 4 * 2 * i), val);

		val = ((coord[i].end_y << 16) & 0xFFF0000)
			| (coord[i].end_x & 0xFFF);
		REG_WR((ISP_AFM_WIN_RANGE0 + 4 * (2 * i + 1)), val);
	}

	return ret;
}

static int32_t isp_k_afm_statistic(struct isp_io_param *param)
{
	int32_t ret = 0;
	int i = 0;
	int max_item = ISP_AFM_WIN_NUM;
	struct isp_afm_statistic item[max_item];
	uint32_t val_l = 0;
	uint32_t val_h = 0;

	memset(item, 0, sizeof(item));
	for (i = 0; i < max_item; i++) {
		val_h = REG_RD((ISP_AFM_STATISTIC0_L + 4 * (2 * i + 1)));
		val_l = REG_RD((ISP_AFM_STATISTIC0_L + 4 * 2 * i));
		val_l = ((val_h & 0x1) << 31)
			| ((val_l >> 1) & 0x7FFFFFFF);
		item[i].val = val_l;
	}
	ret = copy_to_user(param->property_param, (void*)&item, sizeof(item));
	if (0 != ret) {
		ret = -1;
		printk("isp_k_afm_statistic: copy error, ret=0x%x\n", (uint32_t)ret);
	}

	return ret;
}

static int32_t isp_k_afm_win_num(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t num = ISP_AFM_WIN_NUM;

	ret = copy_to_user(param->property_param, (void*)&num, sizeof(num));
	if (0 != ret) {
		ret = -1;
		printk("isp_k_afm_win_num: copy error, ret=0x%x\n", (uint32_t)ret);
	}

	return ret;
}

int32_t isp_k_cfg_afm(struct isp_io_param *param)
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
	case ISP_PRO_AFM_BLOCK:
		ret = isp_k_afm_block(param);
		break;
	case ISP_PRO_AFM_BYPASS:
		ret = isp_k_afm_bypass(param);
		break;
	case ISP_PRO_AFM_SHIFT:
		ret = isp_k_afm_shift(param);
		break;
	case ISP_PRO_AFM_MODE:
		ret = isp_k_afm_mode(param);
		break;
	case ISP_PRO_AFM_SKIP_NUM:
		ret = isp_k_afm_skip_num(param);
		break;
	case ISP_PRO_AFM_SKIP_NUM_CLR:
		ret = isp_k_afm_skip_num_clr(param);
		break;
	case ISP_PRO_AFM_WIN:
		ret = isp_k_afm_win(param);
		break;
	case ISP_PRO_AFM_STATISTIC:
		ret = isp_k_afm_statistic(param);
		break;
	case ISP_PRO_AFM_WIN_NUM:
		ret = isp_k_afm_win_num(param);
		break;
	default:
		printk("isp_k_cfg_afm: fail cmd id:%d, not supported.\n", param->property);
		break;
	}

	return ret;
}
