// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * Exynos Pablo image subsystem functions
 *
 * Copyright (c) 2022 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "pablo-kunit-test.h"

#include "is-core.h"
#include "is-device-sensor-peri.h"
#include "is-interface-sensor.h"
#include "sensor/module_framework/cis/is-cis-2ld.h"
#include "sensor/module_framework/cis/is-cis-2ld-setA.h"
#include "sensor/module_framework/cis/is-cis-2ld-setA-19p2.h"
#include <videodev2_exynos_camera.h>

/* Define the test cases. */
static struct is_core *core;
static struct is_device_sensor *sensor;
static struct i2c_client *client;
static struct is_module_enum *module;
static struct is_device_sensor_peri *sensor_peri;
static struct is_cis *cis;
static struct v4l2_subdev *subdev;
static struct is_cis_ops *cis_ops;

#define REV_LIST_MAX		7
#define MCLK_LIST_MAX		2
#define MIN_FPS_LIST_MAX	2

static void pablo_sensor_2ld_cis_init_kunit_test(struct kunit *test)
{
	int ret, i, j;
	const u32 *setfile;
	u32 rev_list[REV_LIST_MAX] = {0xA000, 0xA001, 0xA101, 0xA102, 0xA201, 0xA202, 0xA301};
	u32 mclk_list[MCLK_LIST_MAX] = {19200, 26000};
	u32 mclk_freq_khz;
	struct exynos_platform_is_module *pdata;

	/* init setting case */
	ret = cis_ops->cis_init(subdev);
	KUNIT_EXPECT_EQ(test, ret, 0);

	KUNIT_EXPECT_EQ(test, cis->cis_data->stream_on, (unsigned int)false);
	KUNIT_EXPECT_EQ(test, cis->cis_data->cur_width, (unsigned int)SENSOR_2LD_MAX_WIDTH);
	KUNIT_EXPECT_EQ(test, cis->cis_data->cur_height, (unsigned int)SENSOR_2LD_MAX_HEIGHT);
	KUNIT_EXPECT_EQ(test, cis->cis_data->low_expo_start, (unsigned int)33000);
	KUNIT_EXPECT_EQ(test, cis->cis_data->pre_lownoise_mode, (unsigned int)IS_CIS_LNOFF);
	KUNIT_EXPECT_EQ(test, cis->cis_data->cur_lownoise_mode, (unsigned int)IS_CIS_LNOFF);
	KUNIT_EXPECT_EQ(test, cis->cis_data->pre_hdr_mode, (unsigned int)SENSOR_AEB_MODE_OFF);
	KUNIT_EXPECT_EQ(test, cis->cis_data->cur_hdr_mode, (unsigned int)SENSOR_AEB_MODE_OFF);
	KUNIT_EXPECT_EQ(test, cis->need_mode_change, (bool)false);
	KUNIT_EXPECT_EQ(test, cis->long_term_mode.sen_strm_off_on_step, (unsigned int)0);
	KUNIT_EXPECT_EQ(test, cis->long_term_mode.sen_strm_off_on_enable, (bool)false);
	KUNIT_EXPECT_EQ(test, cis->mipi_clock_index_cur, CAM_MIPI_NOT_INITIALIZED);
	KUNIT_EXPECT_EQ(test, cis->mipi_clock_index_new, CAM_MIPI_NOT_INITIALIZED);
	KUNIT_EXPECT_EQ(test, cis->cis_data->lte_multi_capture_mode, (bool)false);
	KUNIT_EXPECT_EQ(test, cis->cis_data->sen_frame_id, (unsigned int)0x0);

	/* setfile set case */
	pdata = module->pdata;
	for (j = 0; j < MCLK_LIST_MAX; j++) {
		mclk_freq_khz = pdata->mclk_freq_khz = mclk_list[j];
		for (i = 0; i < REV_LIST_MAX; i++) {
			cis->cis_data->cis_rev = rev_list[i];
			ret = cis_ops->cis_init(subdev);
			KUNIT_EXPECT_EQ(test, ret, 0);

			setfile = pablo_get_cis_2ld_setfile();
			switch (cis->cis_data->cis_rev) {
				case 0xA000:
				case 0xA001:
				case 0xA101:
				case 0xA102:
				case 0xA201:
				case 0xA202:
					KUNIT_EXPECT_PTR_EQ(test, setfile, (const u32 *)sensor_2ld_setfile_A_Global_A2);
					break;
				case 0xA301:
					if (mclk_freq_khz == 19200)
						KUNIT_EXPECT_PTR_EQ(test, setfile, (const u32 *)sensor_2ld_setfile_A_19p2_Global_A3);
					else
						KUNIT_EXPECT_PTR_EQ(test, setfile, (const u32 *)sensor_2ld_setfile_A_Global_A3);
					break;
				default:
					break;
			}
		}
	}
}

static void pablo_sensor_2ld_cis_deinit_kunit_test(struct kunit *test)
{
	int ret;

	ret = cis_ops->cis_deinit(subdev);
	KUNIT_EXPECT_EQ(test, ret, 0);
}

static void pablo_sensor_2ld_cis_log_status_kunit_test(struct kunit *test)
{
	int ret;

	ret = cis_ops->cis_log_status(subdev);
	KUNIT_EXPECT_EQ(test, ret, 0);
}

static void pablo_sensor_2ld_cis_group_param_hold_kunit_test(struct kunit *test)
{
	int ret;
	int hold = 0;

	ret = cis_ops->cis_group_param_hold(subdev, hold);
	KUNIT_EXPECT_EQ(test, ret, 0);
}

static void pablo_sensor_2ld_cis_set_global_setting_kunit_test(struct kunit *test)
{
	int ret;

	ret = cis_ops->cis_set_global_setting(subdev);
	KUNIT_EXPECT_EQ(test, ret, 0);
	KUNIT_EXPECT_EQ(test, cis->stream_state, (enum cis_stream_state)CIS_STREAM_SET_DONE);
}

static void pablo_sensor_2ld_cis_mode_change_kunit_test(struct kunit *test)
{
	int ret, i;
	u32 mode;

	for (i = 0; i < SENSOR_2LD_MODE_MAX; i++) {
		mode = i;
		ret = cis_ops->cis_mode_change(subdev, mode);
		KUNIT_EXPECT_EQ(test, ret, 0);
	}

	/* Error case */
	mode = SENSOR_2LD_MODE_MAX;
	ret = cis_ops->cis_mode_change(subdev, mode);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);
}

static void pablo_sensor_2ld_cis_set_size_kunit_test(struct kunit *test)
{
	int ret;
	cis_shared_data cis_data;

	cis_data.cur_width = SENSOR_2LD_MAX_WIDTH;
	cis_data.cur_height = cis->cis_data->cur_height = SENSOR_2LD_MAX_HEIGHT;
	cis_data.line_readOut_time = 1;
	cis_data.binning = 1000;

	ret = cis_ops->cis_set_size(subdev, &cis_data);
	KUNIT_EXPECT_EQ(test, ret, 0);

	KUNIT_EXPECT_EQ(test, cis_data.frame_time,
		cis_data.line_readOut_time * cis_data.cur_height / 1000);
	KUNIT_EXPECT_EQ(test, cis->cis_data->rolling_shutter_skew,
		(unsigned long long)((cis->cis_data->cur_height - 1) * cis->cis_data->line_readOut_time));

	/* Size err case */
	cis_data.cur_width = SENSOR_2LD_MAX_WIDTH + 1;
	cis_data.cur_height = SENSOR_2LD_MAX_HEIGHT + 1;
	cis_data.binning = 0;

	ret = cis_ops->cis_set_size(subdev, &cis_data);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);

}

static void pablo_sensor_2ld_cis_stream_on_kunit_test(struct kunit *test)
{
	int ret;

	ret = cis_ops->cis_stream_on(subdev);
	KUNIT_EXPECT_EQ(test, ret, 0);
	KUNIT_EXPECT_EQ(test, cis->cis_data->stream_on, (unsigned int)true);
}

static void pablo_sensor_2ld_cis_stream_off_kunit_test(struct kunit *test)
{
	int ret;

	ret = cis_ops->cis_stream_off(subdev);
	KUNIT_EXPECT_EQ(test, ret, 0);
	KUNIT_EXPECT_EQ(test, cis->cis_data->stream_on, (unsigned int)false);
}

static void pablo_sensor_2ld_cis_set_exposure_time_kunit_test(struct kunit *test)
{
	int ret, i;
	struct ae_param target_exposure;
	u16 cit_shifter_array[33] = {0,1,2,2,3,3,3,3,4,4,4,4,4,4,4,4,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,6};
	u16 cit_shifter_val = 0;
	int cit_shifter_idx = 0;
	u8 cit_denom_array[7] = {1, 2, 4, 8, 16, 32, 64};
	u32 short_val_bak, long_val_bak;

	/* Error case */
	target_exposure.short_val = 0;
	target_exposure.long_val = 0;

	ret = cis_ops->cis_set_exposure_time(subdev, &target_exposure);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);

	/* Setting target_exposure case */
	for (i = 0; i < SENSOR_2LD_MODE_MAX; i++) {
		cis->cis_data->sens_config_index_cur = i;

		if ((cis->cis_data->sens_config_index_cur == SENSOR_2LD_2016X1512_30FPS) ||
		    (cis->cis_data->sens_config_index_cur == SENSOR_2LD_2016X1134_30FPS)) {
			/* case1 */
			short_val_bak = target_exposure.short_val = 80000;
			long_val_bak = target_exposure.long_val = 80000;
			cis->cis_data->frame_length_lines_shifter = 0;
			cit_shifter_val = cis->cis_data->frame_length_lines_shifter;

			ret = cis_ops->cis_set_exposure_time(subdev, &target_exposure);
			KUNIT_EXPECT_EQ(test, ret, 0);

			KUNIT_EXPECT_EQ(test, target_exposure.short_val,
				short_val_bak / cit_denom_array[cit_shifter_val]);
			KUNIT_EXPECT_EQ(test, target_exposure.long_val,
				long_val_bak / cit_denom_array[cit_shifter_val]);

			/* case2 */
			short_val_bak = target_exposure.short_val = 160000;
			long_val_bak = target_exposure.long_val = 160000;
			cit_shifter_idx = MIN(MAX(MAX(target_exposure.long_val, target_exposure.short_val) / 80000, 0), 32);
			cis->cis_data->frame_length_lines_shifter = 0;
			cit_shifter_val = MAX(cit_shifter_array[cit_shifter_idx], cis->cis_data->frame_length_lines_shifter);

			ret = cis_ops->cis_set_exposure_time(subdev, &target_exposure);
			KUNIT_EXPECT_EQ(test, ret, 0);

			KUNIT_EXPECT_EQ(test, target_exposure.short_val,
				short_val_bak / cit_denom_array[cit_shifter_val]);
			KUNIT_EXPECT_EQ(test, target_exposure.long_val,
				long_val_bak / cit_denom_array[cit_shifter_val]);
		    } else {
			short_val_bak = target_exposure.short_val = 160000;
			long_val_bak = target_exposure.long_val = 160000;
			cis->cis_data->frame_length_lines_shifter = 0;
			cit_shifter_val = cis->cis_data->frame_length_lines_shifter;

			ret = cis_ops->cis_set_exposure_time(subdev, &target_exposure);
			KUNIT_EXPECT_EQ(test, ret, 0);

			KUNIT_EXPECT_EQ(test, target_exposure.short_val,
				short_val_bak / cit_denom_array[cit_shifter_val]);
			KUNIT_EXPECT_EQ(test, target_exposure.long_val,
				long_val_bak / cit_denom_array[cit_shifter_val]);

			short_val_bak = target_exposure.short_val = 320000;
			long_val_bak = target_exposure.long_val = 320000;
			cit_shifter_idx = MIN(MAX(MAX(target_exposure.long_val, target_exposure.short_val) / 160000, 0), 32);
			cis->cis_data->frame_length_lines_shifter = 0;
			cit_shifter_val = MAX(cit_shifter_array[cit_shifter_idx], cis->cis_data->frame_length_lines_shifter);

			ret = cis_ops->cis_set_exposure_time(subdev, &target_exposure);
			KUNIT_EXPECT_EQ(test, ret, 0);

			KUNIT_EXPECT_EQ(test, target_exposure.short_val,
				short_val_bak / cit_denom_array[cit_shifter_val]);
			KUNIT_EXPECT_EQ(test, target_exposure.long_val,
				long_val_bak / cit_denom_array[cit_shifter_val]);
		    }
	}

	/* Setting exposure_coarse case */
	target_exposure.short_val = 1000;
	target_exposure.long_val = 1000;
	cis->cis_data->line_length_pck = 1;
	cis->cis_data->min_fine_integration_time = 0;
	cis->cis_data->pclk = 1000;
	cis->cis_data->min_coarse_integration_time = 0;
	cis->cis_data->max_coarse_integration_time = 1000;

	for (i = 0; i < IS_CIS_LOWNOISE_MODE_MAX; i++) {
		cis->cis_data->cur_lownoise_mode = i;
		if (cis->cis_data->cur_lownoise_mode == IS_CIS_LNOFF) {
			ret = cis_ops->cis_set_exposure_time(subdev, &target_exposure);
			KUNIT_EXPECT_EQ(test, ret, 0);
			KUNIT_EXPECT_EQ(test, cis->cis_data->cur_long_exposure_coarse, (unsigned int)2);
			KUNIT_EXPECT_EQ(test, cis->cis_data->cur_short_exposure_coarse, (unsigned int)2);
		} else if (cis->cis_data->cur_lownoise_mode == IS_CIS_LN2) {
			ret = cis_ops->cis_set_exposure_time(subdev, &target_exposure);
			KUNIT_EXPECT_EQ(test, ret, 0);
			KUNIT_EXPECT_EQ(test, cis->cis_data->cur_long_exposure_coarse, (unsigned int)5);
			KUNIT_EXPECT_EQ(test, cis->cis_data->cur_short_exposure_coarse, (unsigned int)5);
		} else if (cis->cis_data->cur_lownoise_mode == IS_CIS_LN4) {
			ret = cis_ops->cis_set_exposure_time(subdev, &target_exposure);
			KUNIT_EXPECT_EQ(test, ret, 0);
			KUNIT_EXPECT_EQ(test, cis->cis_data->cur_long_exposure_coarse, (unsigned int)7);
			KUNIT_EXPECT_EQ(test, cis->cis_data->cur_short_exposure_coarse, (unsigned int)7);
		} else {
			ret = cis_ops->cis_set_exposure_time(subdev, &target_exposure);
			KUNIT_EXPECT_EQ(test, ret, 0);
			KUNIT_EXPECT_EQ(test, cis->cis_data->cur_long_exposure_coarse, (unsigned int)2);
			KUNIT_EXPECT_EQ(test, cis->cis_data->cur_short_exposure_coarse, (unsigned int)2);
		}
	}
}

static void pablo_sensor_2ld_cis_adjust_frame_duration_kunit_test(struct kunit *test)
{
	int ret;
	u32 input_exposure_time = 1000;
	u32 target_duration;
	u64 vt_pic_clk_freq_khz = 0;
	u32 line_length_pck = 0;
	u32 frame_length_lines = 0;
	u32 frame_duration = 0;

	cis->cis_data->line_length_pck = 1;
	cis->cis_data->min_fine_integration_time = 0;
	cis->cis_data->pclk = 1000;
	cis->cis_data->max_margin_coarse_integration_time = 0;
	cis->cis_data->min_frame_us_time = 0;

	/* sen_strm_off_on_enable false case */
	cis->long_term_mode.sen_strm_off_on_enable = false;

	ret = cis_ops->cis_adjust_frame_duration(subdev, input_exposure_time, &target_duration);
	KUNIT_EXPECT_EQ(test, ret, 0);

	vt_pic_clk_freq_khz = cis->cis_data->pclk / (1000);
	line_length_pck = cis->cis_data->line_length_pck;
	frame_length_lines = (u32)(((vt_pic_clk_freq_khz * input_exposure_time) / 1000
						- cis->cis_data->min_fine_integration_time) / line_length_pck);
	frame_length_lines += cis->cis_data->max_margin_coarse_integration_time;

	frame_duration = (u32)(((u64)frame_length_lines * line_length_pck) * 1000 / vt_pic_clk_freq_khz);

	KUNIT_EXPECT_EQ(test, target_duration, MAX(frame_duration, cis->cis_data->min_frame_us_time));

	/* sen_strm_off_on_enable true case */
	cis->long_term_mode.sen_strm_off_on_enable = true;

	ret = cis_ops->cis_adjust_frame_duration(subdev, input_exposure_time, &target_duration);
	KUNIT_EXPECT_EQ(test, ret, 0);

	vt_pic_clk_freq_khz = cis->cis_data->pclk / (1000);
	line_length_pck = cis->cis_data->line_length_pck;
	frame_length_lines = (u32)(((vt_pic_clk_freq_khz * input_exposure_time) / 1000
						- cis->cis_data->min_fine_integration_time) / line_length_pck);
	frame_length_lines += cis->cis_data->max_margin_coarse_integration_time;

	frame_duration = (u32)(((u64)frame_length_lines * line_length_pck) * 1000 / vt_pic_clk_freq_khz);

	KUNIT_EXPECT_EQ(test, target_duration, frame_duration);
}

static void pablo_sensor_2ld_cis_set_frame_duration_kunit_test(struct kunit *test)
{
	int ret, i;
	u32 frame_duration;
	u64 vt_pic_clk_freq_khz = 0;
	u32 line_length_pck = 0;
	u32 frame_length_lines = 0;
	u8 frame_length_lines_shifter = 0;
	u8 fll_shifter_array[33] = {0,1,2,2,3,3,3,3,4,4,4,4,4,4,4,4,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,6};
	int fll_shifter_idx = 0;
	u8 fll_denom_array[7] = {1, 2, 4, 8, 16, 32, 64};

	cis->cis_data->min_frame_us_time = 0;
	cis->cis_data->line_length_pck = 1;
	cis->cis_data->pclk = 1000;
	cis->cis_data->max_margin_coarse_integration_time = 0;
	cis->long_term_mode.sen_strm_off_on_enable = false;

	/* Setting frame_duration case */
	for (i = 0; i < SENSOR_2LD_MODE_MAX; i++) {
		cis->cis_data->sens_config_index_cur = i;

		if ((cis->cis_data->sens_config_index_cur == SENSOR_2LD_2016X1512_30FPS) ||
		    (cis->cis_data->sens_config_index_cur == SENSOR_2LD_2016X1134_30FPS)) {
			/* case1 */
			frame_duration = 80000;

			ret = cis_ops->cis_set_frame_duration(subdev, frame_duration);
			KUNIT_EXPECT_EQ(test, ret, 0);

			vt_pic_clk_freq_khz = cis->cis_data->pclk / (1000);
			line_length_pck = cis->cis_data->line_length_pck;
			frame_length_lines = (u16)((vt_pic_clk_freq_khz * frame_duration) / (line_length_pck * 1000));

			KUNIT_EXPECT_EQ(test, cis->cis_data->cur_frame_us_time, frame_duration);
			KUNIT_EXPECT_EQ(test, cis->cis_data->frame_length_lines, frame_length_lines);
			KUNIT_EXPECT_EQ(test, cis->cis_data->max_coarse_integration_time,
				cis->cis_data->frame_length_lines - cis->cis_data->max_margin_coarse_integration_time);

			frame_length_lines_shifter = 0x0;
			KUNIT_EXPECT_EQ(test, cis->cis_data->frame_length_lines_shifter, (u32)frame_length_lines_shifter);

			/* case2 */
			frame_duration = 160000;

			ret = cis_ops->cis_set_frame_duration(subdev, frame_duration);
			KUNIT_EXPECT_EQ(test, ret, 0);

			vt_pic_clk_freq_khz = cis->cis_data->pclk / (1000);
			line_length_pck = cis->cis_data->line_length_pck;

			KUNIT_EXPECT_EQ(test, cis->cis_data->cur_frame_us_time, frame_duration);
			KUNIT_EXPECT_EQ(test, cis->cis_data->max_coarse_integration_time,
				cis->cis_data->frame_length_lines - cis->cis_data->max_margin_coarse_integration_time);

			fll_shifter_idx = MIN(MAX(frame_duration / 80000, 0), 32);
			frame_length_lines_shifter = fll_shifter_array[fll_shifter_idx];
			frame_duration = frame_duration / fll_denom_array[frame_length_lines_shifter];
			frame_length_lines = (u16)((vt_pic_clk_freq_khz * frame_duration) / (line_length_pck * 1000));

			KUNIT_EXPECT_EQ(test, cis->cis_data->frame_length_lines, frame_length_lines);
			KUNIT_EXPECT_EQ(test, cis->cis_data->frame_length_lines_shifter, (u32)frame_length_lines_shifter);
		} else if (cis->cis_data->sens_config_index_cur == SENSOR_2LD_2016X1134_60FPS_MODE2_SSM_960
		    || cis->cis_data->sens_config_index_cur == SENSOR_2LD_2016X1134_60FPS_MODE2_SSM_480
		    || cis->cis_data->sens_config_index_cur == SENSOR_2LD_1280X720_60FPS_MODE2_SSM_960
		    || cis->cis_data->sens_config_index_cur == SENSOR_2LD_1280X720_60FPS_MODE2_SSM_960_SDC_OFF) {
			ret = cis_ops->cis_set_frame_duration(subdev, frame_duration);
			KUNIT_EXPECT_EQ(test, ret, 0);
		} else {
			/* case1 */
			frame_duration = 160000;

			ret = cis_ops->cis_set_frame_duration(subdev, frame_duration);
			KUNIT_EXPECT_EQ(test, ret, 0);

			vt_pic_clk_freq_khz = cis->cis_data->pclk / (1000);
			line_length_pck = cis->cis_data->line_length_pck;
			frame_length_lines = (u16)((vt_pic_clk_freq_khz * frame_duration) / (line_length_pck * 1000));

			KUNIT_EXPECT_EQ(test, cis->cis_data->cur_frame_us_time, frame_duration);
			KUNIT_EXPECT_EQ(test, cis->cis_data->frame_length_lines, frame_length_lines);
			KUNIT_EXPECT_EQ(test, cis->cis_data->max_coarse_integration_time,
				cis->cis_data->frame_length_lines - cis->cis_data->max_margin_coarse_integration_time);

			frame_length_lines_shifter = 0x0;
			KUNIT_EXPECT_EQ(test, cis->cis_data->frame_length_lines_shifter, (u32)frame_length_lines_shifter);

			/* case2 */
			frame_duration = 320000;

			ret = cis_ops->cis_set_frame_duration(subdev, frame_duration);
			KUNIT_EXPECT_EQ(test, ret, 0);

			vt_pic_clk_freq_khz = cis->cis_data->pclk / (1000);
			line_length_pck = cis->cis_data->line_length_pck;

			KUNIT_EXPECT_EQ(test, cis->cis_data->cur_frame_us_time, frame_duration);
			KUNIT_EXPECT_EQ(test, cis->cis_data->max_coarse_integration_time,
				cis->cis_data->frame_length_lines - cis->cis_data->max_margin_coarse_integration_time);

			fll_shifter_idx = MIN(MAX(frame_duration / 160000, 0), 32);
			frame_length_lines_shifter = fll_shifter_array[fll_shifter_idx];
			frame_duration = frame_duration / fll_denom_array[frame_length_lines_shifter];
			frame_length_lines = (u16)((vt_pic_clk_freq_khz * frame_duration) / (line_length_pck * 1000));

			KUNIT_EXPECT_EQ(test, cis->cis_data->frame_length_lines_shifter, (u32)frame_length_lines_shifter);
			KUNIT_EXPECT_EQ(test, cis->cis_data->frame_length_lines, frame_length_lines);
		}
	}
}

static void pablo_sensor_2ld_cis_set_frame_rate_kunit_test(struct kunit *test)
{
	int ret, i;
	u32 min_fps;
	u32 min_fps_list[MIN_FPS_LIST_MAX] = {0, 120};
	u32 frame_duration;

	cis->cis_data->max_fps = 60;

	for (i = 0; i < MIN_FPS_LIST_MAX; i++) {
		min_fps = min_fps_list[i];
		ret = cis_ops->cis_set_frame_rate(subdev, min_fps);
		KUNIT_EXPECT_EQ(test, ret, 0);

		if (min_fps > cis->cis_data->max_fps)
			min_fps = cis->cis_data->max_fps;
		if (min_fps == 0)
			min_fps = 1;

		frame_duration = (1 * 1000 * 1000) / min_fps;
		KUNIT_EXPECT_EQ(test, cis->cis_data->min_frame_us_time, frame_duration);
	}
}

static void pablo_sensor_2ld_cis_adjust_analog_gain_kunit_test(struct kunit *test)
{
	int ret;
	u32 input_again = 1000;
	u32 target_permile;
	u32 again_code = 0;
	u32 again_permile = 0;

	cis->cis_data->max_analog_gain[0] = 100000;
	cis->cis_data->min_analog_gain[0] = 0;

	ret = cis_ops->cis_adjust_analog_gain(subdev, input_again, &target_permile);
	KUNIT_EXPECT_EQ(test, ret, 0);

	again_code = sensor_cis_calc_again_code(input_again);
	again_permile = sensor_cis_calc_again_permile(again_code);

	KUNIT_EXPECT_EQ(test, target_permile, again_permile);
}

static void pablo_sensor_2ld_cis_set_get_analog_gain_kunit_test(struct kunit *test)
{
	int ret;
	struct ae_param again;
	u32 again_ret;

	again.val = 1000;
	cis->cis_data->min_analog_gain[0] = 0;
	cis->cis_data->max_analog_gain[0] = 100000;

	ret = cis_ops->cis_set_analog_gain(subdev, &again);
	KUNIT_EXPECT_EQ(test, ret, 0);

	ret = cis_ops->cis_get_analog_gain(subdev, &again_ret);
	KUNIT_EXPECT_EQ(test, ret, 0);

	KUNIT_EXPECT_EQ(test, again_ret, (u32)0);
}

static void pablo_sensor_2ld_cis_set_get_digital_gain_kunit_test(struct kunit *test)
{
	int ret;
	struct ae_param dgain;
	u32 dgain_ret;

	dgain.val = 1000;
	cis->cis_data->min_digital_gain[0] = 0;
	cis->cis_data->max_digital_gain[0] = 100000;

	ret = cis_ops->cis_set_digital_gain(subdev, &dgain);
	KUNIT_EXPECT_EQ(test, ret, 0);

	ret = cis_ops->cis_get_digital_gain(subdev, &dgain_ret);
	KUNIT_EXPECT_EQ(test, ret, 0);
}

static void pablo_sensor_2ld_cis_compensate_gain_for_extremely_br_kunit_test(struct kunit *test)
{
	int ret, i;
	u32 expo, again, dgain = 0;
	u32 again_ans, dgain_ans;
	u64 vt_pic_clk_freq_khz = 0;
	u32 line_length_pck = 0;
	u32 min_fine_int = 0;
	u32 coarse_int = 0;
	u32 compensated_again = 0;
	u32 remainder_cit = 0;

	expo = 1000;
	cis->cis_data->min_analog_gain[1] = 0;
	cis->cis_data->max_analog_gain[1] = 100000;
	cis->cis_data->line_length_pck = 1;
	cis->cis_data->pclk = 1000;
	cis->cis_data->min_fine_integration_time = 0;
	cis->cis_data->min_coarse_integration_time = 0;

	vt_pic_clk_freq_khz = cis->cis_data->pclk / (1000);
	line_length_pck = cis->cis_data->line_length_pck;
	min_fine_int = cis->cis_data->min_fine_integration_time;

	for (i = 0; i < IS_CIS_LOWNOISE_MODE_MAX; i++) {
		cis->cis_data->cur_lownoise_mode = i;
		if (cis->cis_data->cur_lownoise_mode == IS_CIS_LNOFF) {
			ret = cis_ops->cis_compensate_gain_for_extremely_br(subdev, expo, &again, &dgain);
			KUNIT_EXPECT_EQ(test, ret, 0);

			coarse_int = ((expo * vt_pic_clk_freq_khz) / 1000 - min_fine_int) / line_length_pck;
			if (coarse_int < 2) {
				coarse_int = 2;
			}
			remainder_cit = (coarse_int - 2) % 4;
			coarse_int -= remainder_cit;
		} else if (cis->cis_data->cur_lownoise_mode == IS_CIS_LN2) {
			ret = cis_ops->cis_compensate_gain_for_extremely_br(subdev, expo, &again, &dgain);
			KUNIT_EXPECT_EQ(test, ret, 0);

			coarse_int = ((expo * vt_pic_clk_freq_khz) / 1000 - min_fine_int) / line_length_pck;
			if (coarse_int < 5) {
				coarse_int = 5;
			}
			remainder_cit = (coarse_int - 5) % 8;
			coarse_int -= remainder_cit;
		} else if (cis->cis_data->cur_lownoise_mode == IS_CIS_LN4) {
			ret = cis_ops->cis_compensate_gain_for_extremely_br(subdev, expo, &again, &dgain);
			KUNIT_EXPECT_EQ(test, ret, 0);

			coarse_int = ((expo * vt_pic_clk_freq_khz) / 1000 - min_fine_int) / line_length_pck;
			if (coarse_int < 7) {
				coarse_int = 7;
			}
			remainder_cit = (coarse_int - 7) % 12;
			coarse_int -= remainder_cit;
		} else {
			ret = cis_ops->cis_compensate_gain_for_extremely_br(subdev, expo, &again, &dgain);
			KUNIT_EXPECT_EQ(test, ret, 0);

			coarse_int = ((expo * vt_pic_clk_freq_khz) / 1000 - min_fine_int) / line_length_pck;
			if (coarse_int < 2) {
				coarse_int = 2;
			}
			remainder_cit = (coarse_int - 2) % 4;
			coarse_int -= remainder_cit;
		}

		if (coarse_int <= 1024) {
			compensated_again = (again * ((expo * vt_pic_clk_freq_khz) / 1000 - min_fine_int)) / (line_length_pck * coarse_int);

			if (compensated_again < cis->cis_data->min_analog_gain[1]) {
				again_ans = cis->cis_data->min_analog_gain[1];
			} else if (again >= cis->cis_data->max_analog_gain[1]) {
				dgain_ans = (dgain * ((expo * vt_pic_clk_freq_khz) / 1000 - min_fine_int)) / (line_length_pck * coarse_int);
			} else {
				//*again = compensated_again;
				dgain_ans = (dgain * ((expo * vt_pic_clk_freq_khz) / 1000 - min_fine_int)) / (line_length_pck * coarse_int);
			}
		}
		KUNIT_EXPECT_EQ(test, again_ans, again);
		KUNIT_EXPECT_EQ(test, dgain_ans, dgain);
	}
}

static void pablo_sensor_2ld_cis_data_calc_kunit_test(struct kunit *test)
{
	int i;
	u32 mode;

	for (i = 0; i < SENSOR_2LD_MODE_MAX; i++) {
		mode = i;
		cis_ops->cis_data_calculation(subdev, mode);
	}

	/* Error case */
	mode = SENSOR_2LD_MODE_MAX;
	cis_ops->cis_data_calculation(subdev, mode);
}

static void pablo_sensor_2ld_cis_long_term_exposure_kunit_test(struct kunit *test)
{
	int ret;

	cis->long_term_mode.sen_strm_off_on_enable = true;
	cis->long_term_mode.expo[0] = 1000000;
	ret = cis_ops->cis_set_long_term_exposure(subdev);
	KUNIT_EXPECT_EQ(test, ret, 0);

	cis->long_term_mode.expo[0] = 125000 + 1;
	ret = cis_ops->cis_set_long_term_exposure(subdev);
	KUNIT_EXPECT_EQ(test, ret, 0);

	cis->long_term_mode.sen_strm_off_on_enable = false;
	ret = cis_ops->cis_set_long_term_exposure(subdev);
	KUNIT_EXPECT_EQ(test, ret, 0);
}

static void pablo_sensor_2ld_cis_set_frs_control_kunit_test(struct kunit *test)
{
	int ret, i;
	u32 command;

	for (i = 0; i < FRS_CMD_MAX; i++) {
		command = i;
		ret = cis_ops->cis_set_frs_control(subdev, command);
		KUNIT_EXPECT_EQ(test, ret, 0);
	}
}

static void pablo_sensor_2ld_cis_super_slow_kunit_test(struct kunit *test)
{
	int ret, i;
	struct v4l2_rect setting;
	u32 setting_list[4][2] = {{0x2000, 0x2}, {0x2001, 0x0}, {0x2002, 0x2}, {0x2002, 0x0}};
	u32 threshold, flicker, index, block;
 	u32 gmc, frameid;

	threshold = flicker = index = block = 0;

	ret = cis_ops->cis_set_super_slow_motion_roi(subdev, &setting);
	KUNIT_EXPECT_EQ(test, ret, 0);

	for (i = 0; i < 4; i++) {
		setting.left = setting_list[i][0];
		setting.height = setting_list[i][1];

		ret = cis_ops->cis_set_super_slow_motion_setting(subdev, &setting);
		KUNIT_EXPECT_EQ(test, ret, 0);
	}

	ret = cis_ops->cis_set_super_slow_motion_threshold(subdev, threshold);
	KUNIT_EXPECT_EQ(test, ret, 0);

	ret = cis_ops->cis_get_super_slow_motion_threshold(subdev, &threshold);
	KUNIT_EXPECT_EQ(test, ret, 0);
	KUNIT_EXPECT_EQ(test, threshold, (u32)0);

	ret = cis_ops->cis_get_super_slow_motion_gmc(subdev, &gmc);
	KUNIT_EXPECT_EQ(test, ret, 0);

	ret = cis_ops->cis_get_super_slow_motion_frame_id(subdev, &frameid);
	KUNIT_EXPECT_EQ(test, ret, 0);

	ret = cis_ops->cis_set_super_slow_motion_flicker(subdev, flicker);
	KUNIT_EXPECT_EQ(test, ret, 0);

	ret = cis_ops->cis_get_super_slow_motion_flicker(subdev, &flicker);
	KUNIT_EXPECT_EQ(test, ret, 0);
	KUNIT_EXPECT_EQ(test, flicker, (u32)0);

	ret = cis_ops->cis_get_super_slow_motion_md_threshold(subdev, &threshold);
	KUNIT_EXPECT_EQ(test, ret, 0);

	ret = cis_ops->cis_set_super_slow_motion_gmc_table_idx(subdev, index);
	KUNIT_EXPECT_EQ(test, ret, 0);

	ret = cis_ops->cis_set_super_slow_motion_gmc_block_with_md_low(subdev, block);
	KUNIT_EXPECT_EQ(test, ret, 0);
}

static void pablo_sensor_cis_check_rev_on_init_kunit_test(struct kunit *test)
{
	int ret;

	ret = cis_ops->cis_check_rev_on_init(subdev);
	KUNIT_EXPECT_EQ(test, ret, 0);
}

static void pablo_sensor_cis_set_initial_exposure_kunit_test(struct kunit *test)
{
	int ret;
	ae_setting setting;

	memset(&setting, 0, sizeof(ae_setting));
	cis->use_initial_ae = true;
	cis->last_ae_setting = setting;

	ret = cis_ops->cis_set_initial_exposure(subdev);
	KUNIT_EXPECT_EQ(test, ret, 0);
	KUNIT_EXPECT_EQ(test, memcmp(&cis->init_ae_setting, &cis->last_ae_setting, sizeof(ae_setting)), 0);
}

static void pablo_sensor_2ld_cis_set_factory_control_kunit_test(struct kunit *test)
{
	int ret;
	u32 command = 0;

	ret = cis_ops->cis_set_factory_control(subdev, command);
	KUNIT_EXPECT_EQ(test, ret, 0);
}

static void pablo_sensor_2ld_cis_wait_ln_mode_delay_kunit_test(struct kunit *test)
{
	int ret;

	ret = cis_ops->cis_wait_ln_mode_delay(subdev);
	KUNIT_EXPECT_EQ(test, ret, 0);
}

static int pablo_cis_2ld_kunit_test_init(struct kunit *test)
{
	u32 i2c_ch;

	core = is_get_is_core();
	sensor = &core->sensor[RESOURCE_TYPE_SENSOR0];

	module = &sensor->module_enum[0];

	sensor_peri = (struct is_device_sensor_peri *)module->private_data;
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, sensor_peri);

	subdev = sensor_peri->subdev_cis;
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, subdev);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, cis);

	client = (struct i2c_client *)cis->client;
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, client);

	i2c_ch = module->pdata->sensor_i2c_ch;
	KUNIT_ASSERT_LE(test, i2c_ch, (u32)SENSOR_CONTROL_I2C_MAX);
	sensor_peri->cis.i2c_lock = &core->i2c_lock[i2c_ch];

	cis_ops = cis->cis_ops;

	return 0;
}

static struct kunit_case pablo_cis_2ld_kunit_test_cases[] = {
	KUNIT_CASE(pablo_sensor_2ld_cis_init_kunit_test),
	KUNIT_CASE(pablo_sensor_2ld_cis_deinit_kunit_test),
	KUNIT_CASE(pablo_sensor_2ld_cis_log_status_kunit_test),
	KUNIT_CASE(pablo_sensor_2ld_cis_group_param_hold_kunit_test),
	KUNIT_CASE(pablo_sensor_2ld_cis_set_global_setting_kunit_test),
	KUNIT_CASE(pablo_sensor_2ld_cis_mode_change_kunit_test),
	KUNIT_CASE(pablo_sensor_2ld_cis_set_size_kunit_test),
	KUNIT_CASE(pablo_sensor_2ld_cis_stream_on_kunit_test),
	KUNIT_CASE(pablo_sensor_2ld_cis_stream_off_kunit_test),
	KUNIT_CASE(pablo_sensor_2ld_cis_set_exposure_time_kunit_test),
	KUNIT_CASE(pablo_sensor_2ld_cis_adjust_frame_duration_kunit_test),
	KUNIT_CASE(pablo_sensor_2ld_cis_set_frame_duration_kunit_test),
	KUNIT_CASE(pablo_sensor_2ld_cis_set_frame_rate_kunit_test),
	KUNIT_CASE(pablo_sensor_2ld_cis_adjust_analog_gain_kunit_test),
	KUNIT_CASE(pablo_sensor_2ld_cis_set_get_analog_gain_kunit_test),
	KUNIT_CASE(pablo_sensor_2ld_cis_set_get_digital_gain_kunit_test),
	KUNIT_CASE(pablo_sensor_2ld_cis_compensate_gain_for_extremely_br_kunit_test),
	KUNIT_CASE(pablo_sensor_2ld_cis_data_calc_kunit_test),
	KUNIT_CASE(pablo_sensor_2ld_cis_long_term_exposure_kunit_test),
	KUNIT_CASE(pablo_sensor_2ld_cis_set_frs_control_kunit_test),
	KUNIT_CASE(pablo_sensor_2ld_cis_super_slow_kunit_test),
	KUNIT_CASE(pablo_sensor_cis_check_rev_on_init_kunit_test),
	KUNIT_CASE(pablo_sensor_cis_set_initial_exposure_kunit_test),
	KUNIT_CASE(pablo_sensor_2ld_cis_set_factory_control_kunit_test),
	KUNIT_CASE(pablo_sensor_2ld_cis_wait_ln_mode_delay_kunit_test),
	{},
};

struct kunit_suite pablo_cis_2ld_kunit_test_suite = {
	.name = "pablo-cis-2ld-kunit-test",
	.init = pablo_cis_2ld_kunit_test_init,
	.test_cases = pablo_cis_2ld_kunit_test_cases,
};
define_pablo_kunit_test_suites(&pablo_cis_2ld_kunit_test_suite);

MODULE_LICENSE("GPL");
