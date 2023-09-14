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
static int32_t isp_k_raw_afm_block(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t val = 0;
	uint32_t i = 0;
	int max_num = 0;
	unsigned long  addr = 0;
	struct isp_dev_rgb_afm_info_v1 rafm_info;

	memset(&rafm_info, 0x00, sizeof(rafm_info));

	ret = copy_from_user((void *)&rafm_info, param->property_param, sizeof(rafm_info));
	if (0 != ret) {
		printk("isp_k_raw_afm_block: error, ret=0x%x\n", (uint32_t)ret);
		return -1;
	}

	REG_MWR(ISP_RGB_AFM_PARAM, BIT_0, rafm_info.bypass);

	REG_MWR(ISP_RGB_AFM_PARAM, BIT_1, rafm_info.mode << 1);

	REG_MWR(ISP_RGB_AFM_PARAM, 0xF<<2, (rafm_info.skip_num << 2));

	REG_MWR(ISP_RGB_AFM_PARAM, BIT_6, rafm_info.skip_num_clear << 6);

	REG_MWR(ISP_RGB_AFM_PARAM, BIT_7, rafm_info.spsmd_rtgbot_enable << 7);

	REG_MWR(ISP_RGB_AFM_PARAM, BIT_8, rafm_info.spsmd_diagonal_enable << 8);

#if defined(CONFIG_ARCH_SCX30G3)

	max_num = ISP_AFM_WIN_NUM_R6P9;
	addr = ISP_RGB_AFM_WINDOW_RANGE;

	REG_MWR(ISP_RGB_AFM_PARAM, BIT_9, rafm_info.spsmd_square_en << 9);

	REG_MWR(ISP_RGB_AFM_PARAM, BIT_12, rafm_info.overflow_protect_en << 12);

	REG_MWR(ISP_RGB_AFM_PARAM, BIT_13, rafm_info.subfilter.average << 13);

	REG_MWR(ISP_RGB_AFM_PARAM, BIT_14, rafm_info.subfilter.median << 14);

	REG_MWR(ISP_RGB_AFM_PARAM, BIT_15, rafm_info.spsmd_touch_mode << 15);

	REG_MWR(ISP_RGB_AFM_PARAM, (BIT_18) | (BIT_17) | (BIT_16), rafm_info.shift.shift_spsmd << 16);

	REG_MWR(ISP_RGB_AFM_PARAM, (BIT_21) | (BIT_20) | (BIT_19), rafm_info.shift.shift_sobel9 << 19);

	REG_MWR(ISP_RGB_AFM_PARAM, (BIT_27) | (BIT_26) | (BIT_25), rafm_info.shift.shift_sobel5 << 25);

	val = ((rafm_info.thrd.sobel5_thr_max_red << 16) & 0x3FFF0000)
			| (rafm_info.thrd.sobel5_thr_min_red & 0x0FFF);
	REG_WR(ISP_RGB_AFM_SOBEL5_THR_RED, val);

	val = ((rafm_info.thrd.sobel5_thr_max_green << 16) & 0x3FFF0000)
			| (rafm_info.thrd.sobel5_thr_min_green & 0x0FFF);
	REG_WR(ISP_RGB_AFM_SOBEL5_THR_GREEN, val);

	val = ((rafm_info.thrd.sobel5_thr_max_blue << 16) & 0x3FFF0000)
			| (rafm_info.thrd.sobel5_thr_min_blue & 0x0FFF);
	REG_WR(ISP_RGB_AFM_SOBEL5_THR_BLUE, val);

	val = ((rafm_info.thrd.sobel9_thr_max_red << 16) & 0x3FFF0000)
			| (rafm_info.thrd.sobel9_thr_min_red & 0x0FFF);
	REG_WR(ISP_RGB_AFM_SOBEL9_THR_RED, val);

	val = ((rafm_info.thrd.sobel9_thr_max_green << 16) & 0x3FFF0000)
			| (rafm_info.thrd.sobel9_thr_min_green & 0x0FFF);
	REG_WR(ISP_RGB_AFM_SOBEL9_THR_GREEN, val);

	val = ((rafm_info.thrd.sobel9_thr_max_blue << 16) & 0x3FFF0000)
			| (rafm_info.thrd.sobel9_thr_min_blue & 0x0FFF);
	REG_WR(ISP_RGB_AFM_SOBEL9_THR_BLUE, val);

	val = ((rafm_info.thrd.spsmd_thr_max_red << 12) & 0x7FFFF000)
			| (rafm_info.thrd.spsmd_thr_min_red & 0x0FFF);
	REG_WR(ISP_RGB_AFM_SPSMD_THR_RED, val);

	val = ((rafm_info.thrd.spsmd_thr_max_green << 12) & 0x7FFFF000)
			| (rafm_info.thrd.spsmd_thr_min_green & 0x0FFF);
	REG_WR(ISP_RGB_AFM_SPSMD_THR_GREEN, val);

	val = ((rafm_info.thrd.spsmd_thr_max_blue << 12) & 0x7FFFF000)
			| (rafm_info.thrd.spsmd_thr_min_blue & 0x0FFF);
	REG_WR(ISP_RGB_AFM_SPSMD_THR_BLUE, val);

#else
	max_num = ISP_AFM_WIN_NUM_V1;
	addr = ISP_RGB_AFM_WIN_RANGE0;

	REG_MWR(ISP_RGB_AFM_PARAM, BIT_9, rafm_info.spsmd_cal_mode << 9);

	REG_MWR(ISP_RGB_AFM_PARAM, BIT_10, rafm_info.af_sel_filter1 << 10);

	REG_MWR(ISP_RGB_AFM_PARAM, BIT_11, rafm_info.af_sel_filter2 << 11);

	REG_MWR(ISP_RGB_AFM_PARAM, BIT_12, rafm_info.sobel_type << 12);

	REG_MWR(ISP_RGB_AFM_PARAM, (BIT_13)|(BIT_14), rafm_info.spsmd_type << 13);

	val = ((rafm_info.sobel_threshold_max << 16) & 0xffff0000)
			| (rafm_info.sobel_threshold_min & 0xffff);
	REG_WR(ISP_RGB_AFM_SOBEL_THRESHOLD, val);

	REG_WR(ISP_RGB_AFM_SPSMD_THRESHOLD_MIN, rafm_info.spsmd_threshold_min);
	REG_WR(ISP_RGB_AFM_SPSMD_THRESHOLD_MAX, rafm_info.spsmd_threshold_max);
#endif

	for (i = 0; i < max_num; i++) {
		val = ((rafm_info.coord[i].start_y << 16) & 0xFFFF0000)
				| (rafm_info.coord[i].start_x & 0xFFFF);
		REG_WR((addr + 8 * i), val);

		val = ((rafm_info.coord[i].end_y << 16) & 0xFFFF0000)
			| (rafm_info.coord[i].end_x & 0xFFFF);
		REG_WR((addr + 8 * i + 4), val);
	}

	return ret;
}

static int32_t isp_k_raw_afm_frame_range(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t val = 0;
	struct isp_img_size  frame_size = {0, 0};

	ret = copy_from_user((void *)&frame_size, param->property_param, sizeof(frame_size));
	if (0 != ret) {
		printk("isp_k_raw_afm_frame_range: error, ret = 0x%x\n", (uint32_t)ret);
		return -1;
	}

	val = (frame_size.height & 0xFFFF) | ((frame_size.width & 0xFFFF) << 16);
	REG_WR(ISP_RGB_AFM_FRAME_RANGE, val);
	return ret;
}

static int32_t isp_k_raw_afm_type1_statistic(struct isp_io_param *param)
{
	int32_t ret = 0;

#if defined(CONFIG_ARCH_SCX30G3)

#else
	int i = 0;
	int max_item = ISP_AFM_WIN_NUM_V1;
	struct isp_raw_afm_statistic item;

	memset(&item, 0, sizeof(struct isp_raw_afm_statistic));
	for (i = 0; i < max_item; i++) {
		item.val[i] = REG_RD((ISP_RGB_AFM_TYPE1_VAL00 + 4 * i));
	}
	ret = copy_to_user(param->property_param, (void*)&item, sizeof(item));
	if (0 != ret) {
		ret = -1;
		printk("isp_k_raw_afm_type1_statistic: error, ret = 0x%x\n", (uint32_t)ret);
	}
#endif

	return ret;
}

static int32_t isp_k_raw_afm_type2_statistic(struct isp_io_param *param)
{
	int32_t ret = 0;

#if defined(CONFIG_ARCH_SCX30G3)

#else
	int i = 0;
	int max_item = ISP_AFM_WIN_NUM_V1;
	struct isp_raw_afm_statistic item;

	memset(&item, 0, sizeof(struct isp_raw_afm_statistic));
	for (i = 0; i < max_item; i++) {
		item.val[i] = REG_RD((ISP_RGB_AFM_TYPE2_VAL00 + 4 * i));
	}
	ret = copy_to_user(param->property_param, (void*)&item, sizeof(item));
	if (0 != ret) {
		ret = -1;
		printk("isp_k_raw_afm_type2_statistic: error, ret = 0x%x\n", (uint32_t)ret);
	}
#endif

	return ret;
}

static int32_t isp_k_raw_afm_statistic_r6p9(struct isp_io_param *param)
{
	int32_t ret = 0;

#if defined(CONFIG_ARCH_SCX30G3)
	int i = 0, j = 0;
	int max_item = ISP_AFM_WIN_NUM_R6P9;
	unsigned long addr_sobel5 = ISP_RGB_AFM_ROI_SOBEL5X5;
	unsigned long addr_sobel9 = ISP_RGB_AFM_ROI_SOBEL9X9;
	unsigned long addr_spsmd = ISP_RGB_AFM_ROI_SPSMD;
	unsigned long addr_high_bit = ISP_RGB_AFM_HIGHT_BIT_ROI;
	struct isp_raw_afm_statistic_r6p9 item;

	memset(&item, 0, sizeof(struct isp_raw_afm_statistic_r6p9));
	for (i = 0; i < max_item; i++) {
		item.val[j] = REG_RD(addr_sobel5);
		item.val[j + 1] = REG_RD(addr_sobel5 + 4);
		item.val[j + 2] = REG_RD(addr_sobel5 + 8);

		item.val[ISP_AFM_WIN_NUM_R6P9 * 3 + j] = REG_RD(addr_sobel9);
		item.val[ISP_AFM_WIN_NUM_R6P9 * 3 + j + 1] = REG_RD(addr_sobel9 + 4);
		item.val[ISP_AFM_WIN_NUM_R6P9 * 3 + j + 2] = REG_RD(addr_sobel9 + 8);

		item.val[ISP_AFM_WIN_NUM_R6P9 * 3 * 2 + j] = REG_RD(addr_spsmd);
		item.val[ISP_AFM_WIN_NUM_R6P9 * 3 * 2 + j + 1] = REG_RD(addr_spsmd + 4);
		item.val[ISP_AFM_WIN_NUM_R6P9 * 3 * 2 + j + 2] = REG_RD(addr_spsmd + 8);

		j += 3;
		addr_sobel5 += 12;
		addr_sobel9 += 12;
		addr_spsmd += 12;
	}

	j = 0;
	addr_high_bit = ISP_RGB_AFM_HIGHT_BIT_ROI;
	for (i = 0; i < 3; i++) {
		item.val[ISP_AFM_WIN_NUM_R6P9 * 3 * 3 + j] = REG_RD(addr_high_bit);
		item.val[ISP_AFM_WIN_NUM_R6P9 * 3 * 3 + j + 1] = REG_RD(addr_high_bit + 4);
		item.val[ISP_AFM_WIN_NUM_R6P9 * 3 * 3 + j + 2] = REG_RD(addr_high_bit + 8);
		item.val[ISP_AFM_WIN_NUM_R6P9 * 3 * 3 + j + 3] = REG_RD(addr_high_bit + 12);
		item.val[ISP_AFM_WIN_NUM_R6P9 * 3 * 3 + j + 4] = REG_RD(addr_high_bit + 16);
		j += 5;
		addr_high_bit += 20;
	}

	ret = copy_to_user(param->property_param, (void*)&item, sizeof(item));
	if (0 != ret) {
		ret = -1;
		printk("isp_k_raw_afm_statistic_r6p9: error, ret = 0x%x\n", (uint32_t)ret);
	}
#endif

	return ret;
}

static int32_t isp_k_raw_afm_bypass(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t bypass = 0;

	ret = copy_from_user((void *)&bypass, param->property_param, sizeof(bypass));
	if (0 != ret) {
		printk("isp_k_raw_afm_bypass: error, ret = 0x%x\n", (uint32_t)ret);
		return -1;
	}

	if (bypass) {
		REG_OWR(ISP_RGB_AFM_PARAM, BIT_0);
	} else {
		REG_MWR(ISP_RGB_AFM_PARAM, BIT_0, 0);
	}

	return ret;
}

static int32_t isp_k_raw_afm_mode(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t mode = 0;

	ret = copy_from_user((void *)&mode, param->property_param, sizeof(mode));
	if (0 != ret) {
		printk("isp_k_raw_afm_mode: error, ret = 0x%x\n", (uint32_t)ret);
		return -1;
	}

	if (mode) {
		REG_OWR(ISP_RGB_AFM_PARAM, BIT_1);
	} else {
		REG_MWR(ISP_RGB_AFM_PARAM, BIT_1, 0);
	}

	return ret;
}

static int32_t isp_k_raw_afm_skip_num(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t skip_num = 0;

	ret = copy_from_user((void *)&skip_num, param->property_param, sizeof(skip_num));
	if (0 != ret) {
		printk("isp_k_raw_afm_skip_num: error, ret = 0x%x\n", (uint32_t)ret);
		return -1;
	}

	REG_MWR(ISP_RGB_AFM_PARAM, 0xF << 2, (skip_num << 2));

	return ret;
}

static int32_t isp_k_raw_afm_skip_num_clr(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t is_clear = 0;

	ret = copy_from_user((void *)&is_clear, param->property_param, sizeof(is_clear));
	if (0 != ret) {
		printk("isp_k_raw_afm_skip_num_clr: error, ret = 0x%x\n", (uint32_t)ret);
		return -1;
	}

	if (is_clear) {
		REG_OWR(ISP_RGB_AFM_PARAM, BIT_6);
	} else {
		REG_MWR(ISP_RGB_AFM_PARAM, BIT_6, 0);
	}

	return ret;
}

static int32_t isp_k_raw_afm_spsmd_rtgbot_enable(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t is_enable = 0;

	ret = copy_from_user((void *)&is_enable, param->property_param, sizeof(is_enable));
	if (0 != ret) {
		printk("isp_k_raw_afm_spsmd_rtgbot_enable: error, ret = 0x%x\n", (uint32_t)ret);
		return -1;
	}

	if (is_enable) {
		REG_OWR(ISP_RGB_AFM_PARAM, BIT_7);
	} else {
		REG_MWR(ISP_RGB_AFM_PARAM, BIT_7, 0);
	}

	return ret;
}

static int32_t isp_k_raw_afm_spsmd_diagonal_enable(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t is_enable = 0;

	ret = copy_from_user((void *)&is_enable, param->property_param, sizeof(is_enable));
	if (0 != ret) {
		printk("isp_k_raw_afm_spsmd_diagonal_enable: error, ret = 0x%x\n", (uint32_t)ret);
		return -1;
	}

	if (is_enable) {
		REG_OWR(ISP_RGB_AFM_PARAM, BIT_8);
	} else {
		REG_MWR(ISP_RGB_AFM_PARAM, BIT_8, 0);
	}

	return ret;
}

static int32_t isp_k_raw_afm_spsmd_cal_mode(struct isp_io_param *param)
{
	int32_t ret = 0;

#if defined(CONFIG_ARCH_SCX30G3)

#else
	uint32_t cal_mode = 0;

	ret = copy_from_user((void *)&cal_mode, param->property_param, sizeof(cal_mode));
	if (0 != ret) {
		printk("isp_k_raw_afm_spsmd_cal_mode: error, ret = 0x%x\n", (uint32_t)ret);
		return -1;
	}
	if (cal_mode) {
		REG_OWR(ISP_RGB_AFM_PARAM, BIT_9);
	} else {
		REG_MWR(ISP_RGB_AFM_PARAM, BIT_9, 0);
	}
#endif

	return ret;
}

static int32_t isp_k_raw_afm_spsmd_square_en(struct isp_io_param *param)
{
	int32_t ret = 0;

#if defined(CONFIG_ARCH_SCX30G3)
	uint32_t val = 0;

	ret = copy_from_user((void *)&val, param->property_param, sizeof(val));
	if (0 != ret) {
		printk("isp_k_raw_afm_spsmd_square_en: error, ret = 0x%x\n", (uint32_t)ret);
		return -1;
	}
	if (val) {
		REG_OWR(ISP_RGB_AFM_PARAM, BIT_9);
	} else {
		REG_MWR(ISP_RGB_AFM_PARAM, BIT_9, 0);
	}
#endif

	return ret;
}

static int32_t isp_k_raw_afm_sel_filter1(struct isp_io_param *param)
{
	int32_t ret = 0;

#if defined(CONFIG_ARCH_SCX30G3)

#else
	uint32_t af_sel_filter = 0;

	ret = copy_from_user((void *)&af_sel_filter, param->property_param, sizeof(af_sel_filter));
	if (0 != ret) {
		printk("isp_k_raw_afm_sel_filter1: error, ret = 0x%x\n", (uint32_t)ret);
		return -1;
	}
	if (af_sel_filter) {
		REG_OWR(ISP_RGB_AFM_PARAM, BIT_10);
	} else {
		REG_MWR(ISP_RGB_AFM_PARAM, BIT_10, 0);
	}
#endif

	return ret;
}

static int32_t isp_k_raw_afm_sel_filter2(struct isp_io_param *param)
{
	int32_t ret = 0;

#if defined(CONFIG_ARCH_SCX30G3)

#else
	uint32_t af_sel_filter = 0;

	ret = copy_from_user((void *)&af_sel_filter, param->property_param, sizeof(af_sel_filter));
	if (0 != ret) {
		printk("isp_k_raw_afm_sel_filter2: error, ret = 0x%x\n", (uint32_t)ret);
		return -1;
	}
	if (af_sel_filter) {
		REG_OWR(ISP_RGB_AFM_PARAM, BIT_11);
	} else {
		REG_MWR(ISP_RGB_AFM_PARAM, BIT_11, 0);
	}
#endif

	return ret;
}

static int32_t isp_k_raw_afm_sobel_type(struct isp_io_param *param)
{
	int32_t ret = 0;

#if defined(CONFIG_ARCH_SCX30G3)

#else
	uint32_t sobel_type = 0;

	ret = copy_from_user((void *)&sobel_type, param->property_param, sizeof(sobel_type));
	if (0 != ret) {
		printk("isp_k_raw_afm_sobel_type: error, ret = 0x%x\n", (uint32_t)ret);
		return -1;
	}

	if (sobel_type) {
		REG_OWR(ISP_RGB_AFM_PARAM, BIT_12);
	} else {
		REG_MWR(ISP_RGB_AFM_PARAM, BIT_12, 0);
	}
#endif

	return ret;
}

static int32_t isp_k_raw_afm_overflow_protect(struct isp_io_param *param)
{
	int32_t ret = 0;

#if defined(CONFIG_ARCH_SCX30G3)
	uint32_t val = 0;

	ret = copy_from_user((void *)&val, param->property_param, sizeof(val));
	if (0 != ret) {
		printk("isp_k_raw_afm_overflow_protect: error, ret = 0x%x\n", (uint32_t)ret);
		return -1;
	}

	if (val) {
		REG_OWR(ISP_RGB_AFM_PARAM, BIT_12);
	} else {
		REG_MWR(ISP_RGB_AFM_PARAM, BIT_12, 0);
	}
#endif

	return ret;
}

static int32_t isp_k_raw_afm_spsmd_type(struct isp_io_param *param)
{
	int32_t ret = 0;

#if defined(CONFIG_ARCH_SCX30G3)

#else
	uint32_t spsmd_type = 0;

	ret = copy_from_user((void *)&spsmd_type, param->property_param, sizeof(spsmd_type));
	if (0 != ret) {
		printk("isp_k_raw_afm_spsmd_type: error, ret = 0x%x\n", (uint32_t)ret);
		return -1;
	}

	REG_MWR(ISP_RGB_AFM_PARAM, (BIT_13)|(BIT_14), spsmd_type<<13);
#endif

	return ret;
}

static int32_t isp_k_raw_afm_subfilter(struct isp_io_param *param)
{
	int32_t ret = 0;

#if defined(CONFIG_ARCH_SCX30G3)
	struct afm_subfilter subfilter;

	ret = copy_from_user((void *)&subfilter, param->property_param, sizeof(subfilter));
	if (0 != ret) {
		printk("isp_k_raw_afm_subfilter: error, ret = 0x%x\n", (uint32_t)ret);
		return -1;
	}

	if (subfilter.average) {
		REG_OWR(ISP_RGB_AFM_PARAM, BIT_13);
	} else {
		REG_MWR(ISP_RGB_AFM_PARAM, BIT_13, 0);
	}

	if (subfilter.median) {
		REG_OWR(ISP_RGB_AFM_PARAM, BIT_14);
	} else {
		REG_MWR(ISP_RGB_AFM_PARAM, BIT_14, 0);
	}
#endif

	return ret;
}

static int32_t isp_k_raw_afm_spsmd_touch_mode(struct isp_io_param *param)
{
	int32_t ret = 0;

#if defined(CONFIG_ARCH_SCX30G3)
	uint32_t val = 0;

	ret = copy_from_user((void *)&val, param->property_param, sizeof(val));
	if (0 != ret) {
		printk("isp_k_raw_afm_spsmd_touch_mode: error, ret = 0x%x\n", (uint32_t)ret);
		return -1;
	}

	if (val) {
		REG_OWR(ISP_RGB_AFM_PARAM, BIT_15);
	} else {
		REG_MWR(ISP_RGB_AFM_PARAM, BIT_15, 0);
	}
#endif

	return ret;
}

static int32_t isp_k_raw_afm_shift(struct isp_io_param *param)
{
	int32_t ret = 0;

#if defined(CONFIG_ARCH_SCX30G3)
	struct afm_shift shift;

	ret = copy_from_user((void *)&shift, param->property_param, sizeof(shift));
	if (0 != ret) {
		printk("isp_k_raw_afm_shift: error, ret = 0x%x\n", (uint32_t)ret);
		return -1;
	}

	REG_MWR(ISP_RGB_AFM_PARAM, (BIT_18 | BIT_17 | BIT_16), shift.shift_spsmd << 16);
	REG_MWR(ISP_RGB_AFM_PARAM, (BIT_21 | BIT_20 | BIT_19), shift.shift_sobel9 << 19);
	REG_MWR(ISP_RGB_AFM_PARAM, (BIT_27 | BIT_26 | BIT_25), shift.shift_sobel5 << 25);
#endif

	return ret;
}

static int32_t isp_k_raw_afm_sobel_threshold(struct isp_io_param *param)
{
	int32_t ret = 0;

#if defined(CONFIG_ARCH_SCX30G3)

#else
	uint32_t val = 0;
	struct sobel_thrd sobel_threshold = {0, 0};

	ret = copy_from_user((void *)&sobel_threshold, param->property_param, sizeof(sobel_threshold));
	if (0 != ret) {
		printk("isp_k_raw_afm_sobel_threshold: error, ret = 0x%x\n", (uint32_t)ret);
		return -1;
	}
	val = ((sobel_threshold.max<<16) & 0xffff0000)
			| (sobel_threshold.min & 0xffff);
	REG_WR(ISP_RGB_AFM_SOBEL_THRESHOLD, val);
#endif

	return ret;
}

static int32_t isp_k_raw_afm_spsmd_threshold(struct isp_io_param *param)
{
	int32_t ret = 0;

#if defined(CONFIG_ARCH_SCX30G3)

#else
	struct spsmd_thrd spsmd_threshold = {0, 0};

	ret = copy_from_user((void *)&spsmd_threshold, param->property_param, sizeof(spsmd_threshold));
	if (0 != ret) {
		printk("isp_k_raw_afm_spsmd_threshold: error, ret = 0x%x\n", (uint32_t)ret);
		return -1;
	}
	REG_WR(ISP_RGB_AFM_SPSMD_THRESHOLD_MIN, spsmd_threshold.min);
	REG_WR(ISP_RGB_AFM_SPSMD_THRESHOLD_MAX, spsmd_threshold.max);
#endif

	return ret;
}

static int32_t isp_k_raw_afm_threshold_rgb(struct isp_io_param *param)
{
	int32_t ret = 0;

#if defined(CONFIG_ARCH_SCX30G3)
	uint32_t val = 0;
	struct afm_thrd_rgb thrd_rgb;

	ret = copy_from_user((void *)&thrd_rgb, param->property_param, sizeof(thrd_rgb));
	if (0 != ret) {
		printk("isp_k_raw_afm_threshold_rgb: error, ret = 0x%x\n", (uint32_t)ret);
		return -1;
	}

	val = ((thrd_rgb.sobel5_thr_max_red << 16) & 0x3FFF0000)
			| (thrd_rgb.sobel5_thr_min_red & 0x0FFF);
	REG_WR(ISP_RGB_AFM_SOBEL5_THR_RED, val);

	val = ((thrd_rgb.sobel5_thr_max_green << 16) & 0x3FFF0000)
			| (thrd_rgb.sobel5_thr_min_green & 0x0FFF);
	REG_WR(ISP_RGB_AFM_SOBEL5_THR_GREEN, val);

	val = ((thrd_rgb.sobel5_thr_max_blue << 16) & 0x3FFF0000)
			| (thrd_rgb.sobel5_thr_min_blue & 0x0FFF);
	REG_WR(ISP_RGB_AFM_SOBEL5_THR_BLUE, val);

	val = ((thrd_rgb.sobel9_thr_max_red << 16) & 0x3FFF0000)
			| (thrd_rgb.sobel9_thr_min_red & 0x0FFF);
	REG_WR(ISP_RGB_AFM_SOBEL9_THR_RED, val);

	val = ((thrd_rgb.sobel9_thr_max_green << 16) & 0x3FFF0000)
			| (thrd_rgb.sobel9_thr_min_green & 0x0FFF);
	REG_WR(ISP_RGB_AFM_SOBEL9_THR_GREEN, val);

	val = ((thrd_rgb.sobel9_thr_max_blue << 16) & 0x3FFF0000)
			| (thrd_rgb.sobel9_thr_min_blue & 0x0FFF);
	REG_WR(ISP_RGB_AFM_SOBEL9_THR_BLUE, val);

	val = ((thrd_rgb.spsmd_thr_max_red << 12) & 0x7FFFF000)
			| (thrd_rgb.spsmd_thr_min_red & 0x0FFF);
	REG_WR(ISP_RGB_AFM_SPSMD_THR_RED, val);

	val = ((thrd_rgb.spsmd_thr_max_green << 12) & 0x7FFFF000)
			| (thrd_rgb.spsmd_thr_min_green & 0x0FFF);
	REG_WR(ISP_RGB_AFM_SPSMD_THR_GREEN, val);

	val = ((thrd_rgb.spsmd_thr_max_blue << 12) & 0x7FFFF000)
			| (thrd_rgb.spsmd_thr_min_blue & 0x0FFF);
	REG_WR(ISP_RGB_AFM_SPSMD_THR_BLUE, val);
#endif

	return ret;
}

static int32_t isp_k_raw_afm_win(struct isp_io_param *param)
{
	int32_t ret = 0;
	int i = 0;
	uint32_t val = 0;
	int max_num = 0;
	unsigned long addr = 0;
	struct isp_coord coord[ISP_AFM_WIN_NUM_V1];

	memset(coord, 0, sizeof(coord));
	ret = copy_from_user((void *)&coord, param->property_param, sizeof(coord));
	if (0 != ret) {
		printk("isp_k_raw_afm_win: error, ret = 0x%x\n", (uint32_t)ret);
		return -1;
	}

#if defined(CONFIG_ARCH_SCX30G3)
	max_num = ISP_AFM_WIN_NUM_R6P9;
	addr = ISP_RGB_AFM_WINDOW_RANGE;
#else
	max_num = ISP_AFM_WIN_NUM_V1;
	addr = ISP_RGB_AFM_WIN_RANGE0;
#endif

	for (i = 0; i < max_num; i++) {
		val = ((coord[i].start_y << 16) & 0xFFFF0000)
				| (coord[i].start_x & 0xFFFF);
		REG_WR((addr + 8 * i), val);

		val = ((coord[i].end_y << 16) & 0xFFFF0000)
			| (coord[i].end_x & 0xFFFF);
		REG_WR((addr + 8 * i + 4), val);
	}

	return ret;
}

static int32_t isp_k_raw_afm_win_num(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t num = 0;

#if defined(CONFIG_ARCH_SCX30G3)
	num = ISP_AFM_WIN_NUM_R6P9;
#else
	num = ISP_AFM_WIN_NUM_V1;
#endif

	ret = copy_to_user(param->property_param, (void*)&num, sizeof(num));
	if (0 != ret) {
		ret = -1;
		printk("isp_k_raw_afm_win_num: copy_to_user error, ret = 0x%x\n", (uint32_t)ret);
	}

	return ret;
}

int32_t isp_k_cfg_rgb_afm(struct isp_io_param *param)
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
	case ISP_PRO_RGB_AFM_BLOCK:
		ret = isp_k_raw_afm_block(param);
		break;
	case ISP_PRO_RGB_AFM_FRAME_SIZE:
		ret = isp_k_raw_afm_frame_range(param);
		break;
	case ISP_PRO_RGB_AFM_TYPE1_STATISTIC:
		ret = isp_k_raw_afm_type1_statistic(param);
		break;
	case ISP_PRO_RGB_AFM_TYPE2_STATISTIC:
		ret = isp_k_raw_afm_type2_statistic(param);
		break;
	case ISP_PRO_RGB_AFM_STATISTIC:
		ret = isp_k_raw_afm_statistic_r6p9(param);
		break;
	case ISP_PRO_RGB_AFM_BYPASS:
		ret = isp_k_raw_afm_bypass(param);
		break;
	case ISP_PRO_RGB_AFM_MODE:
		ret = isp_k_raw_afm_mode(param);
		break;
	case ISP_PRO_RGB_AFM_SKIP_NUM:
		ret = isp_k_raw_afm_skip_num(param);
		break;
	case ISP_PRO_RGB_AFM_SKIP_NUM_CLR:
		ret = isp_k_raw_afm_skip_num_clr(param);
		break;
	case ISP_PRO_RGB_AFM_SPSMD_RTGBOT_ENABLE:
		ret = isp_k_raw_afm_spsmd_rtgbot_enable(param);
		break;
	case ISP_PRO_RGB_AFM_SPSMD_DIAGONAL_ENABLE:
		ret = isp_k_raw_afm_spsmd_diagonal_enable(param);
		break;
	case ISP_PRO_RGB_AFM_SPSMD_CAL_MOD:
		ret = isp_k_raw_afm_spsmd_cal_mode(param);
		break;
	case ISP_PRO_RGB_AFM_SPSMD_SQUARE_ENABLE:
		ret = isp_k_raw_afm_spsmd_square_en(param);
		break;
	case ISP_PRO_RGB_AFM_SEL_FILTER1:
		ret = isp_k_raw_afm_sel_filter1(param);
		break;
	case ISP_PRO_RGB_AFM_SEL_FILTER2:
		ret = isp_k_raw_afm_sel_filter2(param);
		break;
	case ISP_PRO_RGB_AFM_SOBEL_TYPE:
		ret = isp_k_raw_afm_sobel_type(param);
		break;
	case ISP_PRO_RGB_AFM_OVERFLOW_PROTECT:
		ret = isp_k_raw_afm_overflow_protect(param);
		break;
	case ISP_PRO_RGB_AFM_SPSMD_TYPE:
		ret = isp_k_raw_afm_spsmd_type(param);
		break;
	case ISP_PRO_RGB_AFM_SUBFILTER:
		ret = isp_k_raw_afm_subfilter(param);
		break;
	case ISP_PRO_RGB_AFM_SPSMD_TOUCH_MODE:
		ret = isp_k_raw_afm_spsmd_touch_mode(param);
		break;
	case ISP_PRO_RGB_AFM_SHFIT:
		ret = isp_k_raw_afm_shift(param);
		break;
	case ISP_PRO_RGB_AFM_SOBEL_THRESHOLD:
		ret = isp_k_raw_afm_sobel_threshold(param);
		break;
	case ISP_PRO_RGB_AFM_SPSMD_THRESHOLD:
		ret = isp_k_raw_afm_spsmd_threshold(param);
		break;
	case ISP_PRO_RGB_AFM_THRESHOLD_RGB:
		ret = isp_k_raw_afm_threshold_rgb(param);
		break;
	case ISP_PRO_RGB_AFM_WIN:
		ret = isp_k_raw_afm_win(param);
		break;
	case ISP_PRO_RGB_AFM_WIN_NUM:
		ret = isp_k_raw_afm_win_num(param);
		break;
	default:
		printk("isp_k_cfg_afm: fail cmd id:%d, not supported.\n", param->property);
		break;
	}

	return ret;
}
