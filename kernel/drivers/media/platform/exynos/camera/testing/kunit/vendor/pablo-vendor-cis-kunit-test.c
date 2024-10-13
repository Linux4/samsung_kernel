// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * Exynos Pablo image subsystem functions
 *
 * Copyright (c) 2021 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "pablo-kunit-test.h"
#include "is-core.h"
#include "is-device-sensor-peri.h"
#include "is-cis.h"
#include "is-helper-ixc.h"
#include "exynos-is-module.h"

#define MIN_FRAME_DURATION_30FPS 33000
#define GAIN_TOLERANCE 10

enum cis_test_type {
	CIS_TEST_I2C = 0,
	CIS_TEST_I3C,
	CIS_TEST_LONG_EXPOSURE,
	CIS_TEST_SENSOR_INFO,
	CIS_TEST_CONTROL_PER_FRAME,
	CIS_TEST_RECOVERY,
	CIS_TEST_COMPENSATE_EXPOSURE,
	CIS_TEST_ADAPTIVE_MIPI,
	CIS_TEST_UPDATE_SEAMLESS_LN_MODE,
};

static struct is_device_sensor *device_sensor;
static struct is_device_sensor_peri *sensor_peri;
static struct v4l2_subdev *subdev_cis;
static struct is_cis *cis;

int get_cis_from_position(struct kunit *test, int position, struct is_cis **cis)
{
	struct is_core *core;
	struct is_module_enum *module = NULL;
	struct is_device_sensor_peri *peri;
	int i = 0;

	*cis = NULL;

	core = is_get_is_core();
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, core);

	for (i = 0; i < IS_SENSOR_COUNT; i++) {
		is_search_sensor_module_with_position(&core->sensor[i], position, &module);
		if (module)
			break;
	}

	if (module == NULL)
		return -EINVAL;

	peri = (struct is_device_sensor_peri *)module->private_data;
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, peri);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, peri->subdev_cis);
	*cis = (struct is_cis *)v4l2_get_subdevdata(peri->subdev_cis);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, *cis);

	return 0;
}

static void cis_set_global_sensor_peri(struct kunit *test, struct is_module_enum *module)
{
	device_sensor = (struct is_device_sensor *)v4l2_get_subdev_hostdata(module->subdev);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, device_sensor);

	sensor_peri = (struct is_device_sensor_peri *)module->private_data;
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, sensor_peri);

	subdev_cis = sensor_peri->subdev_cis;
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, subdev_cis);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev_cis);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, cis);
}

static int cis_search_basic_sensor_mode(struct kunit *test, struct is_module_enum *module)
{
	int i;
	int select_module_index = 0;
	u32 width, framerate, ex_mode, hwformat;
	struct is_device_sensor *device;
	struct is_core *core = is_get_is_core();

	for (i = 0; i < module->cfgs; i++) {
		width = module->cfg[i].width;
		framerate = module->cfg[i].framerate;
		ex_mode = module->cfg[i].ex_mode;
		hwformat = module->cfg[i].output[CSI_VIRTUAL_CH_0].hwformat;

		/* search basic sensor mode */
		if (width > 3000 && width < 5000
			&& framerate >= 30 && framerate <= 60
			&& ex_mode == EX_NONE
			&& hwformat == HW_FORMAT_RAW10) {
			select_module_index = i;
			break;
		}
	}

	KUNIT_ASSERT_LT(test, select_module_index, module->cfgs);

	device = &core->sensor[module->device];
	device->cfg = &module->cfg[select_module_index];
	device->subdev_module = module->subdev;

	return select_module_index;
}

static void cis_control_power(struct kunit *test, struct is_module_enum *module, bool power_on)
{
	int ret = 0;
	u32 scenario = SENSOR_SCENARIO_NORMAL;
	u32 gpio_scenario = GPIO_SCENARIO_ON;
	struct is_core *core = is_get_is_core();

	/* to support retention */
	if (!power_on) {
		gpio_scenario = GPIO_SCENARIO_OFF;
		ret = is_vendor_sensor_gpio_off_sel(&core->vendor, scenario, &gpio_scenario, module);
	}

	ret |= module->pdata->gpio_cfg(module, scenario, gpio_scenario);

	KUNIT_ASSERT_EQ(test, ret, 0);
}

static void cis_prepare_sensor(struct kunit *test, struct is_module_enum *module)
{
	struct is_core *core = is_get_is_core();
	u32 i2c_channel;
	int ret = 0;

	i2c_channel = module->pdata->sensor_i2c_ch;
	KUNIT_ASSERT_LT(test, i2c_channel, SENSOR_CONTROL_I2C_MAX);

	sensor_peri->cis.ixc_lock = &core->ixc_lock[i2c_channel];

	ret = CALL_CISOPS(cis, cis_check_rev_on_init, subdev_cis);
	ret |= CALL_CISOPS(cis, cis_init, subdev_cis);
	ret |= CALL_CISOPS(cis, cis_set_global_setting, subdev_cis);
	KUNIT_EXPECT_EQ(test, ret, 0);
}

static void cis_select_sensor_mode(struct kunit *test, int select_module_index)
{
	int ret;

	cis->cis_data->sens_config_index_cur = select_module_index;
	cis->cis_data->sens_config_ex_mode_cur = EX_NONE;

	CALL_CISOPS(cis, cis_data_calculation, subdev_cis, select_module_index);
	ret = CALL_CISOPS(cis, cis_mode_change, subdev_cis, select_module_index);

	KUNIT_EXPECT_EQ(test, ret, 0);
}

static int cis_get_frame_count(u8 *sensor_fcount)
{
	return cis->ixc_ops->read8(cis->client, 0x0005, sensor_fcount);
}

static void cis_deinit_sensor(struct kunit *test)
{
	int ret;

	/* to support retention */
	ret = CALL_CISOPS(cis, cis_deinit, subdev_cis);
	KUNIT_EXPECT_EQ(test, ret, 0);
}

static void cis_ixc_transfer_kunit_test(struct kunit *test, struct is_module_enum *module)
{
	cis_control_power(test, module, true);
	cis_prepare_sensor(test, module);
	cis_deinit_sensor(test);
	cis_control_power(test, module, false);
}

static void cis_long_exposure_kunit_test(struct kunit *test, struct is_module_enum *module)
{
	int ret = 0;
	int select_module_index = 0;
	long timeout_us;
	u32 frame_duration_us;
	u64 timestamp_diff_ns = 0;
	u8 sensor_fcount;
	const u32 check_interval_us = 5000;
	const u32 lte_duration_us = 1000000; /* 1s */
	struct ae_param expo;

	select_module_index = cis_search_basic_sensor_mode(test, module);

	cis_control_power(test, module, true);
	cis_prepare_sensor(test, module);
	cis_select_sensor_mode(test, select_module_index);

	/* 33ms, basic sensor mode is 30fps ~ 60fps */
	frame_duration_us = MIN_FRAME_DURATION_30FPS;
	expo.long_val = expo.short_val = expo.middle_val = frame_duration_us;

	ret |= CALL_CISOPS(cis, cis_set_frame_duration, subdev_cis, frame_duration_us);
	ret |= CALL_CISOPS(cis, cis_set_exposure_time, subdev_cis, &expo);
	ret |= CALL_CISOPS(cis, cis_stream_on, subdev_cis);
	KUNIT_EXPECT_EQ(test, ret, 0);

	usleep_range(frame_duration_us, frame_duration_us + 1);

	frame_duration_us = lte_duration_us;
	expo.long_val = expo.short_val = expo.middle_val = frame_duration_us;
	timeout_us = lte_duration_us * 10;

	ret |= CALL_CISOPS(cis, cis_set_frame_duration, subdev_cis, frame_duration_us);
	ret |= CALL_CISOPS(cis, cis_set_exposure_time, subdev_cis, &expo);
	KUNIT_EXPECT_EQ(test, ret, 0);

	do {
		ret = cis_get_frame_count(&sensor_fcount);
		KUNIT_EXPECT_EQ(test, ret, 0);

		/* first frame count has different timing, use 2nd ~ 3rd frame time */
		if (timestamp_diff_ns == 0 && sensor_fcount == 2)
			timestamp_diff_ns = ktime_get_boottime_ns();

		if (sensor_fcount == 3) {
			timestamp_diff_ns = ktime_get_boottime_ns() - timestamp_diff_ns;
			break;
		}

		usleep_range(check_interval_us, check_interval_us + 1);
		timeout_us -= check_interval_us;
	} while (timeout_us > 0 && (sensor_fcount == 0xff || sensor_fcount < 3));

	/* allow 99% ~ 101% */
	KUNIT_EXPECT_GT(test, timestamp_diff_ns, lte_duration_us * 990);
	KUNIT_EXPECT_LT(test, timestamp_diff_ns, lte_duration_us * 1010);

	ret = CALL_CISOPS(cis, cis_stream_off, subdev_cis);
	ret |= CALL_CISOPS(cis, cis_wait_streamoff, subdev_cis);
	KUNIT_EXPECT_EQ(test, ret, 0);

	cis_control_power(test, module, false);
}

static void cis_sensor_info_kunit_test(struct kunit *test, struct is_module_enum *module)
{
	const struct sensor_cis_mode_info *mode_info;
	int i;
	u32 mode, max_fps, ex_mode, hwformat;
	u64 pclk_per_frame;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, cis->sensor_info);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, cis->sensor_info->version);
	KUNIT_EXPECT_GT(test, cis->sensor_info->max_width, 0);
	KUNIT_EXPECT_GT(test, cis->sensor_info->max_height, 0);
	KUNIT_EXPECT_GT(test, cis->sensor_info->mode_count, 0);
	KUNIT_EXPECT_GT(test, cis->sensor_info->fine_integration_time, 0);

	for (i = 0; i < module->cfgs; i++) {
		mode = module->cfg[i].mode;
		ex_mode = module->cfg[i].ex_mode;
		max_fps = module->cfg[i].max_fps;
		KUNIT_EXPECT_GT(test, max_fps, 0);

		mode_info = cis->sensor_info->mode_infos[mode];
		hwformat = module->cfg[i].output[CSI_VIRTUAL_CH_0].hwformat;

		if (hwformat == HW_FORMAT_RAW10)
			KUNIT_EXPECT_EQ_MSG(test, mode_info->state_12bit, SENSOR_12BIT_STATE_OFF,
				"module index:%d, mode setting index:%d, version %s",
				i, mode, cis->sensor_info->version);
		else if (hwformat == HW_FORMAT_RAW12)
			KUNIT_EXPECT_NE_MSG(test, mode_info->state_12bit, SENSOR_12BIT_STATE_OFF,
				"module index:%d, mode setting index:%d, version %s",
				i, mode, cis->sensor_info->version);

		KUNIT_EXPECT_LE(test, mode, cis->sensor_info->mode_count);
		KUNIT_EXPECT_EQ(test, mode, mode_info->setfile_index);

		if (ex_mode != EX_DUALFPS_480 && ex_mode != EX_DUALFPS_960) {
			CALL_CISOPS(cis, cis_data_calculation, subdev_cis, mode);
			KUNIT_EXPECT_LE_MSG(test, max_fps, cis->cis_data->max_fps,
				"module index:%d, mode setting index:%d, version %s",
				i, mode, cis->sensor_info->version);
		}
	}

	for (i = 0; i < cis->sensor_info->mode_count; i++) {
		mode_info = cis->sensor_info->mode_infos[i];

		KUNIT_EXPECT_GT(test, mode_info->frame_length_lines, 0);
		KUNIT_EXPECT_GT(test, mode_info->line_length_pck, 0);

		pclk_per_frame = (u64)mode_info->frame_length_lines * mode_info->line_length_pck;
		KUNIT_EXPECT_GT(test, mode_info->pclk, pclk_per_frame);

		KUNIT_EXPECT_GT(test, mode_info->max_analog_gain, cis->sensor_info->min_analog_gain);
		KUNIT_EXPECT_GT(test, mode_info->max_digital_gain, cis->sensor_info->min_digital_gain);

		KUNIT_EXPECT_GT(test, mode_info->min_cit, 0);
		KUNIT_EXPECT_LT(test, mode_info->min_cit, mode_info->frame_length_lines);

		KUNIT_EXPECT_GT(test, mode_info->max_cit_margin, 0);
		KUNIT_EXPECT_LT(test, mode_info->max_cit_margin, mode_info->frame_length_lines);

		KUNIT_EXPECT_GT(test, mode_info->align_cit, 0);
	}
}

static void cis_sensor_control_one_frame_kunit_test(struct kunit *test,
	u32 frame_duration_us, bool use_max)
{
	int ret = 0;
	struct ae_param expo;
	struct ae_param again;
	struct ae_param dgain;
	u32 min_exp, max_exp;
	u32 min_again, max_again, ret_again;
	u32 min_dgain, max_dgain, ret_dgain;

	min_exp = max_exp = 0;
	min_again = max_again = ret_again = 0;
	min_dgain = max_dgain = ret_dgain = 0;

	ret = CALL_CISOPS(cis, cis_set_frame_duration, subdev_cis, frame_duration_us);
	KUNIT_EXPECT_EQ(test, ret, 0);

	ret = CALL_CISOPS(cis, cis_get_min_exposure_time, subdev_cis, &min_exp);
	KUNIT_EXPECT_EQ(test, ret, 0);
	ret = CALL_CISOPS(cis, cis_get_max_exposure_time, subdev_cis, &max_exp);
	KUNIT_EXPECT_EQ(test, ret, 0);
	KUNIT_EXPECT_LT(test, min_exp, max_exp);

	if (use_max)
		expo.long_val = expo.short_val = expo.middle_val = max_exp;
	else
		expo.long_val = expo.short_val = expo.middle_val = min_exp;

	ret = CALL_CISOPS(cis, cis_set_exposure_time, subdev_cis, &expo);
	KUNIT_EXPECT_EQ(test, ret, 0);

	ret = CALL_CISOPS(cis, cis_get_min_analog_gain, subdev_cis, &min_again);
	KUNIT_EXPECT_EQ(test, ret, 0);
	ret = CALL_CISOPS(cis, cis_get_max_analog_gain, subdev_cis, &max_again);
	KUNIT_EXPECT_EQ(test, ret, 0);
	KUNIT_EXPECT_LT(test, min_again, max_again);

	if (use_max)
		again.long_val = again.short_val = again.middle_val = max_again;
	else
		again.long_val = again.short_val = again.middle_val = min_again;

	ret = CALL_CISOPS(cis, cis_set_analog_gain, subdev_cis, &again);
	KUNIT_EXPECT_EQ(test, ret, 0);
	ret = CALL_CISOPS(cis, cis_get_analog_gain, subdev_cis, &ret_again);
	KUNIT_EXPECT_EQ(test, ret, 0);
	KUNIT_EXPECT_EQ(test, again.long_val / GAIN_TOLERANCE, ret_again / GAIN_TOLERANCE);

	ret = CALL_CISOPS(cis, cis_get_min_digital_gain, subdev_cis, &min_dgain);
	KUNIT_EXPECT_EQ(test, ret, 0);
	ret = CALL_CISOPS(cis, cis_get_max_digital_gain, subdev_cis, &max_dgain);
	KUNIT_EXPECT_EQ(test, ret, 0);
	KUNIT_EXPECT_LT(test, min_dgain, max_dgain);

	if (use_max)
		dgain.long_val = dgain.short_val = dgain.middle_val = max_dgain;
	else
		dgain.long_val = dgain.short_val = dgain.middle_val = min_dgain;

	ret = CALL_CISOPS(cis, cis_set_digital_gain, subdev_cis, &dgain);
	KUNIT_EXPECT_EQ(test, ret, 0);
	ret = CALL_CISOPS(cis, cis_get_digital_gain, subdev_cis, &ret_dgain);
	KUNIT_EXPECT_EQ(test, ret, 0);
	KUNIT_EXPECT_EQ(test, dgain.long_val / GAIN_TOLERANCE, ret_dgain / GAIN_TOLERANCE);
}

static void cis_sensor_control_per_frame_kunit_test(struct kunit *test, struct is_module_enum *module)
{
	int ret = 0;
	int select_module_index = 0;
	u32 frame_duration_us;
	u32 delay_us;
	u8 pre_fcount;
	u8 cur_fcount;

	/* 33ms, basic sensor mode is 30fps ~ 60fps */
	frame_duration_us = MIN_FRAME_DURATION_30FPS;
	delay_us = frame_duration_us * 2;

	select_module_index = cis_search_basic_sensor_mode(test, module);

	cis_control_power(test, module, true);
	cis_prepare_sensor(test, module);
	cis_select_sensor_mode(test, select_module_index);

	cis_sensor_control_one_frame_kunit_test(test, frame_duration_us, false);

	ret = CALL_CISOPS(cis, cis_stream_on, subdev_cis);
	KUNIT_EXPECT_EQ(test, ret, 0);
	ret = CALL_CISOPS(cis, cis_wait_streamon, subdev_cis);
	KUNIT_EXPECT_EQ(test, ret, 0);

	ret = cis_get_frame_count(&pre_fcount);
	KUNIT_EXPECT_EQ(test, ret, 0);

	cis_sensor_control_one_frame_kunit_test(test, frame_duration_us, true);
	usleep_range(delay_us, delay_us + 1);

	ret = cis_get_frame_count(&cur_fcount);
	KUNIT_EXPECT_EQ(test, ret, 0);
	KUNIT_EXPECT_NE(test, cur_fcount, pre_fcount);
	pre_fcount = cur_fcount;

	cis_sensor_control_one_frame_kunit_test(test, frame_duration_us, false);
	usleep_range(delay_us, delay_us + 1);

	ret = cis_get_frame_count(&cur_fcount);
	KUNIT_EXPECT_EQ(test, ret, 0);
	KUNIT_EXPECT_NE(test, cur_fcount, pre_fcount);

	ret = CALL_CISOPS(cis, cis_stream_off, subdev_cis);
	KUNIT_EXPECT_EQ(test, ret, 0);
	ret = CALL_CISOPS(cis, cis_wait_streamoff, subdev_cis);
	KUNIT_EXPECT_EQ(test, ret, 0);

	cis_control_power(test, module, false);
}

static void cis_sensor_recovery_kunit_test(struct kunit *test, struct is_module_enum *module)
{
	int ret = 0;
	int select_module_index = 0;
	u32 frame_duration_us;
	u32 delay_us;
	u8 pre_fcount;
	u8 cur_fcount;

	/* 33ms, basic sensor mode is 30fps ~ 60fps */
	frame_duration_us = MIN_FRAME_DURATION_30FPS;
	delay_us = frame_duration_us * 2;

	select_module_index = cis_search_basic_sensor_mode(test, module);

	cis_control_power(test, module, true);
	cis_prepare_sensor(test, module);
	cis_select_sensor_mode(test, select_module_index);

	cis_sensor_control_one_frame_kunit_test(test, frame_duration_us, false);

	ret = CALL_CISOPS(cis, cis_stream_on, subdev_cis);
	KUNIT_EXPECT_EQ(test, ret, 0);
	ret = CALL_CISOPS(cis, cis_wait_streamon, subdev_cis);
	KUNIT_EXPECT_EQ(test, ret, 0);
	ret = CALL_CISOPS(cis, cis_recover_stream_on, subdev_cis);
	KUNIT_EXPECT_EQ(test, ret, 0);

	usleep_range(delay_us, delay_us + 1);

	ret = cis_get_frame_count(&pre_fcount);
	KUNIT_EXPECT_EQ(test, ret, 0);

	cis_sensor_control_one_frame_kunit_test(test, frame_duration_us, false);
	usleep_range(delay_us, delay_us + 1);

	ret = cis_get_frame_count(&cur_fcount);
	KUNIT_EXPECT_EQ(test, ret, 0);
	KUNIT_EXPECT_NE(test, cur_fcount, pre_fcount);

	ret = CALL_CISOPS(cis, cis_stream_off, subdev_cis);
	KUNIT_EXPECT_EQ(test, ret, 0);
	ret = CALL_CISOPS(cis, cis_wait_streamoff, subdev_cis);
	KUNIT_EXPECT_EQ(test, ret, 0);

	cis_control_power(test, module, false);
}

static void cis_sensor_compensate_exposure_kunit_test(struct kunit *test, struct is_module_enum *module)
{
	int ret = 0;
	int select_module_index = 0;
	u32 frame_duration_us;
	u32 delay_us;
	u64 ns_per_cit;
	u32 expo_ms, again_milli, dgain_milli, input_again_milli, input_dgain_milli;
	u32 cit_compensation_threshold;
	struct ae_param expo;
	struct ae_param again;
	struct ae_param dgain;
	const struct sensor_cis_mode_info *mode_info;

	/* 33ms, basic sensor mode is 30fps ~ 60fps */
	frame_duration_us = MIN_FRAME_DURATION_30FPS;
	delay_us = frame_duration_us * 2;

	select_module_index = cis_search_basic_sensor_mode(test, module);

	cis_control_power(test, module, true);
	cis_prepare_sensor(test, module);
	cis_select_sensor_mode(test, select_module_index);

	cis_sensor_control_one_frame_kunit_test(test, frame_duration_us, false);

	ret = CALL_CISOPS(cis, cis_stream_on, subdev_cis);
	KUNIT_EXPECT_EQ(test, ret, 0);

	ret = CALL_CISOPS(cis, cis_wait_streamon, subdev_cis);
	KUNIT_EXPECT_EQ(test, ret, 0);

	ret = CALL_CISOPS(cis, cis_recover_stream_on, subdev_cis);
	KUNIT_EXPECT_EQ(test, ret, 0);

	ret = CALL_CISOPS(cis, cis_set_frame_duration, subdev_cis, frame_duration_us);
	KUNIT_EXPECT_EQ(test, ret, 0);

	mode_info = cis->sensor_info->mode_infos[select_module_index];
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, mode_info);
	KUNIT_EXPECT_GE(test, mode_info->align_cit, 1);
	KUNIT_EXPECT_GT(test, cis->sensor_info->cit_compensation_threshold, 0);

	ns_per_cit = (u64)1000000 * 1000 * cis->cis_data->line_length_pck / cis->cis_data->pclk;
	KUNIT_EXPECT_GT(test, ns_per_cit, 0);

	cit_compensation_threshold = cis->sensor_info->cit_compensation_threshold * mode_info->align_cit;

	/* test 200% longer exposure than cit_compensation_threshold */
	expo_ms = (cit_compensation_threshold * 2) * ns_per_cit / 1000;
	expo.long_val = expo.short_val = expo.middle_val = expo_ms;
	again_milli = 1000; // x1 analog gain
	dgain_milli = 1000; // x1 digital gain

	ret = CALL_CISOPS(cis, cis_set_exposure_time, subdev_cis, &expo);
	KUNIT_EXPECT_EQ(test, ret, 0);

	ret = CALL_CISOPS(cis, cis_compensate_gain_for_extremely_br, subdev_cis, expo_ms, &again_milli, &dgain_milli);
	KUNIT_EXPECT_EQ(test, ret, 0);

	/* again & dgain should be same with input value */
	KUNIT_EXPECT_EQ(test, again_milli, 1000);
	KUNIT_EXPECT_EQ(test, dgain_milli, 1000);

	again.long_val = again.short_val = again.middle_val = again_milli;
	dgain.long_val = dgain.short_val = dgain.middle_val = dgain_milli;

	ret = CALL_CISOPS(cis, cis_set_analog_gain, subdev_cis, &again);
	KUNIT_EXPECT_EQ(test, ret, 0);

	ret = CALL_CISOPS(cis, cis_set_digital_gain, subdev_cis, &dgain);
	KUNIT_EXPECT_EQ(test, ret, 0);

	usleep_range(delay_us, delay_us + 1);

	/* test ns_per_cit/2 shorter exposure than cit_compensation_threshold */
	expo_ms = (cit_compensation_threshold * ns_per_cit / 1000) - (ns_per_cit / 1000 / 2);
	expo.long_val = expo.short_val = expo.middle_val = expo_ms;
	again_milli = 1000; // x1 analog gain
	dgain_milli = 1000; // x1 digital gain

	ret = CALL_CISOPS(cis, cis_set_exposure_time, subdev_cis, &expo);
	KUNIT_EXPECT_EQ(test, ret, 0);

	ret = CALL_CISOPS(cis, cis_compensate_gain_for_extremely_br, subdev_cis, expo_ms, &again_milli, &dgain_milli);
	KUNIT_EXPECT_EQ(test, ret, 0);

	/* again or dgain should compensate exposure */
	KUNIT_EXPECT_TRUE(test, ((again_milli > 1000) || (dgain_milli > 1000)));

	again.long_val = again.short_val = again.middle_val = again_milli;
	dgain.long_val = dgain.short_val = dgain.middle_val = dgain_milli;

	ret = CALL_CISOPS(cis, cis_set_analog_gain, subdev_cis, &again);
	KUNIT_EXPECT_EQ(test, ret, 0);

	ret = CALL_CISOPS(cis, cis_set_digital_gain, subdev_cis, &dgain);
	KUNIT_EXPECT_EQ(test, ret, 0);

	usleep_range(delay_us, delay_us + 1);

	/* test proximate max again to check again & dgain compensation */
	expo_ms = ((mode_info->min_cit + 1) * ns_per_cit / 1000) + (ns_per_cit / 1000 / 2);
	expo.long_val = expo.short_val = expo.middle_val = expo_ms;

	again_milli = CALL_CISOPS(cis, cis_calc_again_permile, mode_info->max_analog_gain);
	KUNIT_EXPECT_GE(test, again_milli, 10);
	again_milli -= 10; // max analog gain - x0.01x
	input_again_milli = again_milli;
	input_dgain_milli = dgain_milli = 1000; // x1 digital gain

	ret = CALL_CISOPS(cis, cis_set_exposure_time, subdev_cis, &expo);
	KUNIT_EXPECT_EQ(test, ret, 0);

	ret = CALL_CISOPS(cis, cis_compensate_gain_for_extremely_br, subdev_cis, expo_ms, &again_milli, &dgain_milli);
	KUNIT_EXPECT_EQ(test, ret, 0);

	/* again or dgain should compensate exposure */
	KUNIT_EXPECT_NE(test, again_milli, input_again_milli);
	KUNIT_EXPECT_NE(test, dgain_milli, input_dgain_milli);

	again.long_val = again.short_val = again.middle_val = again_milli;
	dgain.long_val = dgain.short_val = dgain.middle_val = dgain_milli;

	ret = CALL_CISOPS(cis, cis_set_analog_gain, subdev_cis, &again);
	KUNIT_EXPECT_EQ(test, ret, 0);

	ret = CALL_CISOPS(cis, cis_set_digital_gain, subdev_cis, &dgain);
	KUNIT_EXPECT_EQ(test, ret, 0);

	ret = CALL_CISOPS(cis, cis_stream_off, subdev_cis);
	KUNIT_EXPECT_EQ(test, ret, 0);

	ret = CALL_CISOPS(cis, cis_wait_streamoff, subdev_cis);
	KUNIT_EXPECT_EQ(test, ret, 0);

	cis_control_power(test, module, false);
}

static void cis_sensor_adaptive_mipi_kunit_test(struct kunit *test, struct is_module_enum *module)
{
	int ret = 0;
	int select_module_index = 0;
	u32 frame_duration_us;
	u32 delay_us;
	u8 pre_fcount;
	u8 cur_fcount;

	/* 33ms, basic sensor mode is 30fps ~ 60fps */
	frame_duration_us = MIN_FRAME_DURATION_30FPS;
	delay_us = frame_duration_us * 2;

	select_module_index = cis_search_basic_sensor_mode(test, module);

	ret = is_vendor_set_mipi_mode(cis);
	KUNIT_EXPECT_EQ(test, ret, 0);

	cis_control_power(test, module, true);
	cis_prepare_sensor(test, module);
	cis_select_sensor_mode(test, select_module_index);

	ret = is_vendor_update_mipi_info(device_sensor);
	KUNIT_EXPECT_EQ(test, ret, 0);

	cis_sensor_control_one_frame_kunit_test(test, frame_duration_us, false);

	ret = CALL_CISOPS(cis, cis_stream_on, subdev_cis);
	KUNIT_EXPECT_EQ(test, ret, 0);
	ret = CALL_CISOPS(cis, cis_wait_streamon, subdev_cis);
	KUNIT_EXPECT_EQ(test, ret, 0);

	ret = cis_get_frame_count(&pre_fcount);
	KUNIT_EXPECT_EQ(test, ret, 0);

	cis_sensor_control_one_frame_kunit_test(test, frame_duration_us, false);
	usleep_range(delay_us, delay_us + 1);

	ret = cis_get_frame_count(&cur_fcount);
	KUNIT_EXPECT_EQ(test, ret, 0);
	KUNIT_EXPECT_NE(test, cur_fcount, pre_fcount);

	ret = CALL_CISOPS(cis, cis_stream_off, subdev_cis);
	KUNIT_EXPECT_EQ(test, ret, 0);
	ret = CALL_CISOPS(cis, cis_wait_streamoff, subdev_cis);
	KUNIT_EXPECT_EQ(test, ret, 0);

	cis_control_power(test, module, false);
}

static void cis_sensor_update_seamless_ln_mode_kunit_test(struct kunit *test, struct is_module_enum *module)
{
	int ret = 0;
	int select_module_index = 0;
	u32 frame_duration_us;
	u32 delay_us;
	u8 pre_fcount;
	u8 cur_fcount;
	enum is_cis_lownoise_mode ln_mode;

	/* 33ms, basic sensor mode is 30fps ~ 60fps */
	frame_duration_us = MIN_FRAME_DURATION_30FPS;
	delay_us = frame_duration_us * 2;

	select_module_index = cis_search_basic_sensor_mode(test, module);

	ret = is_vendor_set_mipi_mode(cis);
	KUNIT_EXPECT_EQ(test, ret, 0);

	cis_control_power(test, module, true);
	cis_prepare_sensor(test, module);
	cis_select_sensor_mode(test, select_module_index);

	cis_sensor_control_one_frame_kunit_test(test, frame_duration_us, false);

	ret = CALL_CISOPS(cis, cis_stream_on, subdev_cis);
	KUNIT_EXPECT_EQ(test, ret, 0);
	ret = CALL_CISOPS(cis, cis_wait_streamon, subdev_cis);
	KUNIT_EXPECT_EQ(test, ret, 0);

	for (ln_mode = IS_CIS_LNOFF; ln_mode <= IS_CIS_LN4; ln_mode++) {
		ret = cis_get_frame_count(&pre_fcount);
		KUNIT_EXPECT_EQ(test, ret, 0);

		cis->cis_data->cur_lownoise_mode = ln_mode;
		ret = CALL_CISOPS(cis, cis_update_seamless_mode, subdev_cis);
		KUNIT_EXPECT_EQ(test, ret, 0);

		cis_sensor_control_one_frame_kunit_test(test, frame_duration_us, false);

		usleep_range(delay_us, delay_us + 1);

		ret = cis_get_frame_count(&cur_fcount);
		KUNIT_EXPECT_EQ(test, ret, 0);
		KUNIT_EXPECT_NE(test, cur_fcount, pre_fcount);
	}

	ret = CALL_CISOPS(cis, cis_stream_off, subdev_cis);
	KUNIT_EXPECT_EQ(test, ret, 0);
	ret = CALL_CISOPS(cis, cis_wait_streamoff, subdev_cis);
	KUNIT_EXPECT_EQ(test, ret, 0);

	cis_control_power(test, module, false);
}

static void cis_test_all_camera_kunit_test(struct kunit *test, enum cis_test_type type)
{
	struct is_core *core = is_get_is_core();
	struct is_module_enum *module;
	int pos, i;

	for (pos = 0; pos < SENSOR_POSITION_MAX; pos++) {
		for (i = 0; i < IS_SENSOR_COUNT; i++) {
			is_search_sensor_module_with_position(&core->sensor[i], pos, &module);
			if (!module)
				continue;

			cis_set_global_sensor_peri(test, module);

			switch (type) {
			case CIS_TEST_I2C:
				if (module->pdata->cis_ixc_type == I2C_TYPE)
					cis_ixc_transfer_kunit_test(test, module);
				break;
			case CIS_TEST_I3C:
				if (module->pdata->cis_ixc_type == I3C_TYPE)
					cis_ixc_transfer_kunit_test(test, module);
				break;
			case CIS_TEST_LONG_EXPOSURE:
				cis_long_exposure_kunit_test(test, module);
				break;
			case CIS_TEST_SENSOR_INFO:
				cis_sensor_info_kunit_test(test, module);
				break;
			case CIS_TEST_CONTROL_PER_FRAME:
				cis_sensor_control_per_frame_kunit_test(test, module);
				break;
			case CIS_TEST_RECOVERY:
				cis_sensor_recovery_kunit_test(test, module);
				break;
			case CIS_TEST_COMPENSATE_EXPOSURE:
				cis_sensor_compensate_exposure_kunit_test(test, module);
				break;
			case CIS_TEST_ADAPTIVE_MIPI:
				cis_sensor_adaptive_mipi_kunit_test(test, module);
				break;
			case CIS_TEST_UPDATE_SEAMLESS_LN_MODE:
				cis_sensor_update_seamless_ln_mode_kunit_test(test, module);
				break;
			}
		}
	}
}

static void pablo_vendor_cis_i2c_kunit_test(struct kunit *test)
{
	cis_test_all_camera_kunit_test(test, CIS_TEST_I2C);
}

static void pablo_vendor_cis_i3c_kunit_test(struct kunit *test)
{
	cis_test_all_camera_kunit_test(test, CIS_TEST_I3C);
}

static void pablo_vendor_cis_long_exposure_kunit_test(struct kunit *test)
{
	cis_test_all_camera_kunit_test(test, CIS_TEST_LONG_EXPOSURE);
}

static void pablo_vendor_cis_sensor_info_kunit_test(struct kunit *test)
{
	cis_test_all_camera_kunit_test(test, CIS_TEST_SENSOR_INFO);
}

static void pablo_vendor_cis_control_per_frame_kunit_test(struct kunit *test)
{
	cis_test_all_camera_kunit_test(test, CIS_TEST_CONTROL_PER_FRAME);
}

static void pablo_vendor_cis_recovery_kunit_test(struct kunit *test)
{
	cis_test_all_camera_kunit_test(test, CIS_TEST_RECOVERY);
}

static void pablo_vendor_cis_compensate_exposure_kunit_test(struct kunit *test)
{
	cis_test_all_camera_kunit_test(test, CIS_TEST_COMPENSATE_EXPOSURE);
}

static void pablo_vendor_cis_adaptive_mipi_kunit_test(struct kunit *test)
{
	cis_test_all_camera_kunit_test(test, CIS_TEST_ADAPTIVE_MIPI);
}

static void pablo_vendor_cis_update_seamless_ln_mode_kunit_test(struct kunit *test)
{
	cis_test_all_camera_kunit_test(test, CIS_TEST_UPDATE_SEAMLESS_LN_MODE);
}

static void pablo_vendor_cis_private_data_kunit_test(struct kunit *test)
{
	struct is_cis *cis_rear2; /* tele x3 */
	struct is_cis *cis_rear4; /* tele x10 */

	if (get_cis_from_position(test, SENSOR_POSITION_REAR2, &cis_rear2) < 0)
		return;

	if (get_cis_from_position(test, SENSOR_POSITION_REAR4, &cis_rear4) < 0)
		return;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, cis_rear2->sensor_info);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, cis_rear4->sensor_info);

	if (cis_rear2->sensor_info != cis_rear4->sensor_info)
		KUNIT_EXPECT_TRUE(test, cis_rear2->sensor_info->priv != cis_rear4->sensor_info->priv);
}

static int pablo_vendor_cis_kunit_test_init(struct kunit *test)
{
	int ret = 0;

	return ret;
}

static void pablo_vendor_cis_kunit_test_exit(struct kunit *test)
{
}

static struct kunit_case pablo_vendor_cis_kunit_test_cases[] = {
	KUNIT_CASE(pablo_vendor_cis_i2c_kunit_test),
	KUNIT_CASE(pablo_vendor_cis_i3c_kunit_test),
	KUNIT_CASE(pablo_vendor_cis_sensor_info_kunit_test),
	KUNIT_CASE(pablo_vendor_cis_control_per_frame_kunit_test),
	KUNIT_CASE(pablo_vendor_cis_long_exposure_kunit_test),
	KUNIT_CASE(pablo_vendor_cis_recovery_kunit_test),
	KUNIT_CASE(pablo_vendor_cis_compensate_exposure_kunit_test),
	KUNIT_CASE(pablo_vendor_cis_adaptive_mipi_kunit_test),
	KUNIT_CASE(pablo_vendor_cis_private_data_kunit_test),
	KUNIT_CASE(pablo_vendor_cis_update_seamless_ln_mode_kunit_test),
	{},
};

struct kunit_suite pablo_vendor_cis_kunit_test_suite = {
	.name = "pablo-vendor-cis-kunit-test",
	.init = pablo_vendor_cis_kunit_test_init,
	.exit = pablo_vendor_cis_kunit_test_exit,
	.test_cases = pablo_vendor_cis_kunit_test_cases,
};
define_pablo_kunit_test_suites(&pablo_vendor_cis_kunit_test_suite);

MODULE_LICENSE("GPL");
