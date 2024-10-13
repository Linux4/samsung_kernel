// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * Copyright (c) 2023 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "is-core.h"
#include "is-device-sensor-peri.h"
#include "is-cis.h"
#include "is-helper-ixc.h"
#include "exynos-is-module.h"
#include "is-vendor.h"
#include "punit-test-cis-result.h"

#define MIN_FRAME_DURATION_30FPS 33000

#define PUNIT_ASSERT_ERR_RET(condition)                                                            \
	{                                                                                          \
		if (unlikely(condition)) {                                                         \
			info("[ERR][%s] %s:%d(%s)\n", __FILE__, __func__, __LINE__, #condition);   \
			return -EINVAL;                                                            \
		}                                                                                  \
	}

#define PUNIT_ASSERT_ZERO_RET(condition)                                                           \
	{                                                                                          \
		if (unlikely(condition)) {                                                         \
			info("[WARN][%s] %s:%d(%s)\n", __FILE__, __func__, __LINE__, #condition);  \
			return 0;                                                                  \
		}                                                                                  \
	}

#define PUNIT_EXPECT(condition, msg)                                                               \
	{                                                                                          \
		if (unlikely(condition))                                                           \
			info("[WARN][%s] %s:%d(%s)\n", __FILE__, __func__, __LINE__, msg);         \
	}

/*
 * @cis self test
 */
static int pablo_test_set_cis_run(const char *val, const struct kernel_param *kp);
static int pablo_test_get_cis_run(char *buffer, const struct kernel_param *kp);
static const struct kernel_param_ops pablo_param_ops_cis_run = {
	.set = pablo_test_set_cis_run,
	.get = pablo_test_get_cis_run,
};

module_param_cb(test_cis_run, &pablo_param_ops_cis_run, NULL, 0644);

enum pablo_type_sensor_run {
	PABLO_CIS_RUN_STOP = 0,
	PABLO_CIS_RUN_START,
};

enum cis_test_type {
	CIS_TEST_I2C = 0,
	CIS_TEST_I3C,
	CIS_TEST_LONG_EXPOSURE,
	CIS_TEST_CONTROL_PER_FRAME,
	CIS_TEST_RECOVERY,
	CIS_TEST_ADAPTIVE_MIPI,
	CIS_TEST_UPDATE_SEAMLESS_LN_MODE,
	CIS_TEST_COMPENSATE_EXPOSURE,
	CIS_TEST_SENSOR_INFO,
};

struct pablo_info_cis_run {
	u32 act;
	u32 type;
};

static struct pablo_info_cis_run cr_info = {
	.act = PABLO_CIS_RUN_STOP,
	.type = CIS_TEST_I2C,
};

static struct is_device_sensor *device_sensor;
static struct is_device_sensor_peri *sensor_peri;
static struct v4l2_subdev *subdev_cis;
static struct is_cis *cis;

static int pst_cis_result;

static int cis_search_basic_sensor_mode(struct is_module_enum *module)
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
		hwformat = module->cfg[i].output[0][CSI_VIRTUAL_CH_0].hwformat;

		/* search basic sensor mode */
		if (width > 3000 && width < 5000 && framerate >= 30 && framerate <= 60 &&
		    ex_mode == EX_NONE && hwformat == HW_FORMAT_RAW10) {
			select_module_index = i;
			break;
		}
	}

	PUNIT_ASSERT_ERR_RET(select_module_index >= module->cfgs);

	device = &core->sensor[module->device];
	device->cfg = &module->cfg[select_module_index];
	device->subdev_module = module->subdev;

	return select_module_index;
}

static int cis_set_global_sensor_peri(struct is_module_enum *module)
{
	device_sensor = (struct is_device_sensor *)v4l2_get_subdev_hostdata(module->subdev);
	if (!device_sensor) {
		err("device_sensor is NULL.");
		return -EINVAL;
	}

	sensor_peri = (struct is_device_sensor_peri *)module->private_data;
	if (!sensor_peri) {
		err("sensor_peri is NULL.");
		return -EINVAL;
	}

	subdev_cis = sensor_peri->subdev_cis;
	if (!subdev_cis) {
		err("subdev_cis is NULL.");
		return -EINVAL;
	}

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev_cis);
	if (!cis) {
		err("cis is NULL.");
		return -EINVAL;
	}

	return 0;
}

static int cis_control_power(struct is_module_enum *module, bool power_on)
{
	int ret = 0;
	u32 scenario = SENSOR_SCENARIO_NORMAL;
	u32 gpio_scenario = GPIO_SCENARIO_ON;
	struct is_core *core = is_get_is_core();

	/* to support retention */
	if (!power_on) {
		gpio_scenario = GPIO_SCENARIO_OFF;
		ret = is_vendor_sensor_gpio_off_sel(&core->vendor, scenario, &gpio_scenario,
						    module);
	}

	ret |= module->pdata->gpio_cfg(module, scenario, gpio_scenario);
	info("set cis[%s] power %s", module->sensor_name, power_on ? "on" : "off");

	return ret;
}

static int cis_prepare_sensor(struct is_module_enum *module)
{
	int ret;
	u32 i2c_channel;
	struct is_core *core = is_get_is_core();

	i2c_channel = module->pdata->sensor_i2c_ch;
	PUNIT_ASSERT_ERR_RET(i2c_channel >= SENSOR_CONTROL_I2C_MAX);

	sensor_peri->cis.ixc_lock = &core->ixc_lock[i2c_channel];

	ret = CALL_CISOPS(cis, cis_check_rev_on_init, subdev_cis);
	ret |= CALL_CISOPS(cis, cis_init, subdev_cis);
	ret |= CALL_CISOPS(cis, cis_set_global_setting, subdev_cis);

	info("[%s] %s done", __func__, module->sensor_name);

	return ret;
}

static int cis_select_sensor_mode(int select_module_index)
{
	int ret;

	cis->cis_data->sens_config_index_cur = select_module_index;
	cis->cis_data->sens_config_ex_mode_cur = EX_NONE;

	CALL_CISOPS(cis, cis_data_calculation, subdev_cis, select_module_index);
	ret = CALL_CISOPS(cis, cis_mode_change, subdev_cis, select_module_index);

	return ret;
}

static int cis_get_frame_count(u8 *sensor_fcount)
{
	return cis->ixc_ops->read8(cis->client, 0x0005, sensor_fcount);
}

static int cis_deinit_sensor(void)
{
	int ret;

	/* to support retention */
	ret = CALL_CISOPS(cis, cis_deinit, subdev_cis);
	info("[%s] cis%d done", __func__, cis->id);

	return ret;
}

static int cis_ixc_transfer_test(struct is_module_enum *module)
{
	int ret;

	ret = cis_control_power(module, true);
	ret |= cis_prepare_sensor(module);
	ret |= cis_deinit_sensor();
	ret |= cis_control_power(module, false);

	return ret;
}

static int cis_long_exposure_test(struct is_module_enum *module)
{
	int ret;
	int select_module_index = 0;
	long timeout_us;
	u32 frame_duration_us;
	u64 timestamp_diff_ns = 0;
	u8 sensor_fcount;
	const u32 check_interval_us = 5000;
	const u32 lte_duration_us = 1000000; /* 1s */
	struct ae_param expo;

	select_module_index = cis_search_basic_sensor_mode(module);

	cis_control_power(module, true);
	cis_prepare_sensor(module);
	cis_select_sensor_mode(select_module_index);

	/* 33ms, basic sensor mode is 30fps ~ 60fps */
	frame_duration_us = MIN_FRAME_DURATION_30FPS;
	expo.long_val = expo.short_val = expo.middle_val = frame_duration_us;

	ret = CALL_CISOPS(cis, cis_set_frame_duration, subdev_cis, frame_duration_us);
	ret |= CALL_CISOPS(cis, cis_set_exposure_time, subdev_cis, &expo);
	ret |= CALL_CISOPS(cis, cis_stream_on, subdev_cis);
	PUNIT_EXPECT(ret, "Stream on failed");

	usleep_range(frame_duration_us, frame_duration_us + 1);

	frame_duration_us = lte_duration_us;
	expo.long_val = expo.short_val = expo.middle_val = frame_duration_us;
	timeout_us = lte_duration_us * 10;

	ret = CALL_CISOPS(cis, cis_set_frame_duration, subdev_cis, frame_duration_us);
	ret |= CALL_CISOPS(cis, cis_set_exposure_time, subdev_cis, &expo);
	PUNIT_EXPECT(ret, "Set exposure ime failed");

	do {
		ret = cis_get_frame_count(&sensor_fcount);
		PUNIT_EXPECT(ret, "cis_get_frame_count failed");

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
	PUNIT_EXPECT(timestamp_diff_ns < lte_duration_us * 990,
		     "Out of the range, less than 99 percents");
	PUNIT_EXPECT(timestamp_diff_ns > lte_duration_us * 1010,
		     "Out of the range, more than 101 percents");

	ret = CALL_CISOPS(cis, cis_stream_off, subdev_cis);
	ret |= CALL_CISOPS(cis, cis_wait_streamoff, subdev_cis);
	PUNIT_EXPECT(ret, "Stream off failed");

	cis_control_power(module, false);

	return ret;
}

static int cis_sensor_control_one_frame_test(u32 frame_duration_us, bool use_max)
{
	int ret;
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
	PUNIT_EXPECT(ret, "cis_set_frame_duration failed");

	ret = CALL_CISOPS(cis, cis_get_min_exposure_time, subdev_cis, &min_exp);
	PUNIT_EXPECT(ret, "cis_get_min_exposure_time failed");

	ret = CALL_CISOPS(cis, cis_get_max_exposure_time, subdev_cis, &max_exp);
	PUNIT_EXPECT(ret, "cis_get_max_exposure_time failed");

	if (use_max)
		expo.long_val = expo.short_val = expo.middle_val = max_exp;
	else
		expo.long_val = expo.short_val = expo.middle_val = min_exp;

	ret = CALL_CISOPS(cis, cis_set_exposure_time, subdev_cis, &expo);
	PUNIT_EXPECT(ret, "cis_set_exposure_time failed");

	ret = CALL_CISOPS(cis, cis_get_min_analog_gain, subdev_cis, &min_again);
	PUNIT_EXPECT(ret, "cis_get_min_analog_gain failed");

	ret = CALL_CISOPS(cis, cis_get_max_analog_gain, subdev_cis, &max_again);
	PUNIT_EXPECT(ret, "cis_get_max_analog_gain failed");

	if (use_max)
		again.long_val = again.short_val = again.middle_val = max_again;
	else
		again.long_val = again.short_val = again.middle_val = min_again;

	ret = CALL_CISOPS(cis, cis_set_analog_gain, subdev_cis, &again);
	PUNIT_EXPECT(ret, "cis_set_analog_gain failed");

	ret = CALL_CISOPS(cis, cis_get_analog_gain, subdev_cis, &ret_again);
	PUNIT_EXPECT(ret, "cis_get_analog_gain failed");
	PUNIT_EXPECT(again.long_val != ret_again, "again.long_val != ret_again.");

	ret = CALL_CISOPS(cis, cis_get_min_digital_gain, subdev_cis, &min_dgain);
	PUNIT_EXPECT(ret, "cis_get_min_digital_gain failed");

	ret = CALL_CISOPS(cis, cis_get_max_digital_gain, subdev_cis, &max_dgain);
	PUNIT_EXPECT(ret, "cis_get_max_digital_gain failed");

	info("[%s] min_exp:%d, max_exp:%d, min_again:%d, max_again:%d, min_dgain:%d, max_dgain:%d.",
	     __func__, min_exp, max_exp, min_again, max_again, min_dgain, max_dgain);

	if (use_max)
		dgain.long_val = dgain.short_val = dgain.middle_val = max_dgain;
	else
		dgain.long_val = dgain.short_val = dgain.middle_val = min_dgain;

	ret = CALL_CISOPS(cis, cis_set_digital_gain, subdev_cis, &dgain);
	PUNIT_EXPECT(ret, "cis_set_digital_gain failed");

	ret = CALL_CISOPS(cis, cis_get_digital_gain, subdev_cis, &ret_dgain);
	PUNIT_EXPECT(ret, "cis_get_digital_gain failed");
	PUNIT_EXPECT(dgain.long_val != ret_dgain, "dgain.long_val != ret_dgain.");

	return ret;
}

static int cis_sensor_control_per_frame_test(struct is_module_enum *module)
{
	int ret;
	int select_module_index;
	u32 frame_duration_us;
	u32 delay_us;
	u8 pre_fcount;
	u8 cur_fcount;

	/* 33ms, basic sensor mode is 30fps ~ 60fps */
	frame_duration_us = MIN_FRAME_DURATION_30FPS;
	delay_us = frame_duration_us * 2;

	select_module_index = cis_search_basic_sensor_mode(module);

	cis_control_power(module, true);
	cis_prepare_sensor(module);
	cis_select_sensor_mode(select_module_index);

	ret = cis_sensor_control_one_frame_test(frame_duration_us, false);
	PUNIT_EXPECT(ret, "sensor control one frame failed");

	ret = CALL_CISOPS(cis, cis_stream_on, subdev_cis);
	PUNIT_EXPECT(ret, "cis_stream_on failed");

	ret = CALL_CISOPS(cis, cis_wait_streamon, subdev_cis);
	PUNIT_EXPECT(ret, "cis_wait_streamon failed");

	ret = cis_get_frame_count(&pre_fcount);
	PUNIT_EXPECT(ret, "cis_get_frame_count failed");

	ret = cis_sensor_control_one_frame_test(frame_duration_us, true);
	PUNIT_EXPECT(ret, "sensor control one frame failed");

	usleep_range(delay_us, delay_us + 1);

	ret = cis_get_frame_count(&cur_fcount);
	PUNIT_EXPECT(ret, "cis_get_frame_count failed");
	PUNIT_EXPECT(cur_fcount == pre_fcount, "cur_fcount == pre_fcount.");

	pre_fcount = cur_fcount;

	ret = cis_sensor_control_one_frame_test(frame_duration_us, false);
	PUNIT_EXPECT(ret, "sensor control one frame failed");

	usleep_range(delay_us, delay_us + 1);

	ret = cis_get_frame_count(&cur_fcount);
	PUNIT_EXPECT(ret, "cis_get_frame_count failed");
	PUNIT_EXPECT(cur_fcount == pre_fcount, "cur_fcount == pre_fcount.");

	ret = CALL_CISOPS(cis, cis_stream_off, subdev_cis);
	PUNIT_EXPECT(ret, "cis_stream_off failed");

	ret = CALL_CISOPS(cis, cis_wait_streamoff, subdev_cis);
	PUNIT_EXPECT(ret, "cis_wait_streamoff failed");

	cis_control_power(module, false);

	return ret;
}

static int cis_sensor_recovery_test(struct is_module_enum *module)
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

	select_module_index = cis_search_basic_sensor_mode(module);

	cis_control_power(module, true);
	cis_prepare_sensor(module);
	cis_select_sensor_mode(select_module_index);

	cis_sensor_control_one_frame_test(frame_duration_us, false);

	ret = CALL_CISOPS(cis, cis_stream_on, subdev_cis);
	PUNIT_EXPECT(ret, "cis_stream_on failed");

	ret = CALL_CISOPS(cis, cis_wait_streamon, subdev_cis);
	PUNIT_EXPECT(ret, "cis_wait_streamon failed");

	ret = CALL_CISOPS(cis, cis_recover_stream_on, subdev_cis);
	PUNIT_EXPECT(ret, "cis_recover_stream_on failed");

	usleep_range(delay_us, delay_us + 1);

	ret = cis_get_frame_count(&pre_fcount);
	PUNIT_EXPECT(ret, "cis_get_frame_count failed");

	cis_sensor_control_one_frame_test(frame_duration_us, false);
	usleep_range(delay_us, delay_us + 1);

	ret = cis_get_frame_count(&cur_fcount);
	PUNIT_EXPECT(ret, "cis_get_frame_count failed");
	PUNIT_EXPECT(cur_fcount == pre_fcount, "cur_fcount == pre_fcount.");

	ret = CALL_CISOPS(cis, cis_stream_off, subdev_cis);
	PUNIT_EXPECT(ret, "cis_stream_off failed");

	ret = CALL_CISOPS(cis, cis_wait_streamoff, subdev_cis);
	PUNIT_EXPECT(ret, "cis_wait_streamoff failed");

	cis_control_power(module, false);

	return ret;
}

static int cis_sensor_adaptive_mipi_test(struct is_module_enum *module)
{
	int ret;
	int select_module_index;
	u32 frame_duration_us;
	u32 delay_us;
	u8 pre_fcount;
	u8 cur_fcount;

	/* 33ms, basic sensor mode is 30fps ~ 60fps */
	frame_duration_us = MIN_FRAME_DURATION_30FPS;
	delay_us = frame_duration_us * 2;

	select_module_index = cis_search_basic_sensor_mode(module);

	ret = is_vendor_set_mipi_mode(cis);
	PUNIT_EXPECT(ret, "Set mipi mode failed");

	cis_control_power(module, true);
	cis_prepare_sensor(module);
	cis_select_sensor_mode(select_module_index);

	ret = is_vendor_update_mipi_info(device_sensor);
	PUNIT_EXPECT(ret, "Update mipi info failed");

	cis_sensor_control_one_frame_test(frame_duration_us, false);

	ret = CALL_CISOPS(cis, cis_stream_on, subdev_cis);
	PUNIT_EXPECT(ret, "cis_stream_on failed");

	ret = CALL_CISOPS(cis, cis_wait_streamon, subdev_cis);
	PUNIT_EXPECT(ret, "cis_wait_streamon failed");

	ret = cis_get_frame_count(&pre_fcount);
	PUNIT_EXPECT(ret, "cis_get_frame_count failed");

	cis_sensor_control_one_frame_test(frame_duration_us, false);
	usleep_range(delay_us, delay_us + 1);

	ret = cis_get_frame_count(&cur_fcount);
	PUNIT_EXPECT(ret, "cis_get_frame_count failed");
	PUNIT_EXPECT(cur_fcount == pre_fcount, "cur_fcount == pre_fcount.");

	ret = CALL_CISOPS(cis, cis_stream_off, subdev_cis);
	PUNIT_EXPECT(ret, "cis_stream_off failed");

	ret = CALL_CISOPS(cis, cis_wait_streamoff, subdev_cis);
	PUNIT_EXPECT(ret, "cis_wait_streamoff failed");

	cis_control_power(module, false);

	return ret;
}

static int cis_sensor_update_seamless_ln_mode_test(struct is_module_enum *module)
{
	int ret;
	int select_module_index;
	u32 frame_duration_us;
	u32 delay_us;
	u8 pre_fcount;
	u8 cur_fcount;
	enum is_cis_lownoise_mode ln_mode;

	/* 33ms, basic sensor mode is 30fps ~ 60fps */
	frame_duration_us = MIN_FRAME_DURATION_30FPS;
	delay_us = frame_duration_us * 2;

	select_module_index = cis_search_basic_sensor_mode(module);

	ret = is_vendor_set_mipi_mode(cis);
	PUNIT_EXPECT(ret, "Set mipi mode failed");

	cis_control_power(module, true);
	cis_prepare_sensor(module);
	cis_select_sensor_mode(select_module_index);

	cis_sensor_control_one_frame_test(frame_duration_us, false);

	ret = CALL_CISOPS(cis, cis_stream_on, subdev_cis);
	PUNIT_EXPECT(ret, "cis_stream_on failed");

	ret = CALL_CISOPS(cis, cis_wait_streamon, subdev_cis);
	PUNIT_EXPECT(ret, "cis_wait_streamon failed");

	for (ln_mode = IS_CIS_LNOFF; ln_mode <= IS_CIS_LN4; ln_mode++) {
		ret = cis_get_frame_count(&pre_fcount);
		PUNIT_EXPECT(ret, "cis_get_frame_count failed");

		cis->cis_data->cur_lownoise_mode = ln_mode;
		ret = CALL_CISOPS(cis, cis_update_seamless_mode, subdev_cis);
		PUNIT_EXPECT(ret, "cis_update_seamless_mode failed");

		cis_sensor_control_one_frame_test(frame_duration_us, false);

		usleep_range(delay_us, delay_us + 1);

		ret = cis_get_frame_count(&cur_fcount);
		PUNIT_EXPECT(ret, "cis_get_frame_count failed");
		PUNIT_EXPECT(cur_fcount == pre_fcount, "cur_fcount == pre_fcount.");
	}

	ret = CALL_CISOPS(cis, cis_stream_off, subdev_cis);
	PUNIT_EXPECT(ret, "cis_stream_off failed");

	ret = CALL_CISOPS(cis, cis_wait_streamoff, subdev_cis);
	PUNIT_EXPECT(ret, "cis_wait_streamoff failed");

	cis_control_power(module, false);

	return ret;
}

static int cis_sensor_compensate_exposure_test(struct is_module_enum *module)
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

	select_module_index = cis_search_basic_sensor_mode(module);

	cis_control_power(module, true);
	cis_prepare_sensor(module);
	cis_select_sensor_mode(select_module_index);

	cis_sensor_control_one_frame_test(frame_duration_us, false);

	ret = CALL_CISOPS(cis, cis_stream_on, subdev_cis);
	PUNIT_EXPECT(ret, "cis_stream_on failed");

	ret = CALL_CISOPS(cis, cis_wait_streamon, subdev_cis);
	PUNIT_EXPECT(ret, "cis_wait_streamon failed");

	ret = CALL_CISOPS(cis, cis_recover_stream_on, subdev_cis);
	PUNIT_EXPECT(ret, "cis_recover_stream_on failed");

	ret = CALL_CISOPS(cis, cis_set_frame_duration, subdev_cis, frame_duration_us);
	PUNIT_EXPECT(ret, "cis_set_frame_duration failed");

	/* keep backward compatibility for legacy driver */
	if (cis->sensor_info) {
		mode_info = cis->sensor_info->mode_infos[select_module_index];
		if (mode_info && mode_info->align_cit >= 1 &&
		    cis->sensor_info->cit_compensation_threshold > 0) {
			ns_per_cit = (u64)1000000 * 1000 * cis->cis_data->line_length_pck /
				     cis->cis_data->pclk;
			cit_compensation_threshold =
				cis->sensor_info->cit_compensation_threshold * mode_info->align_cit;
			/* test 200% longer exposure than cit_compensation_threshold */
			expo_ms = (cit_compensation_threshold * 2) * ns_per_cit / 1000;
		} else {
			expo_ms = 1000;
			cit_compensation_threshold = 0;
		}
	} else {
		mode_info = NULL;
		cit_compensation_threshold = 0;
		expo_ms = 1000;
		info("mode_info (%d) is NULL.", select_module_index);
	}

	expo.long_val = expo.short_val = expo.middle_val = expo_ms;
	again_milli = 1000; // x1 analog gain
	dgain_milli = 1000; // x1 digital gain

	ret = CALL_CISOPS(cis, cis_set_exposure_time, subdev_cis, &expo);
	PUNIT_EXPECT(ret, "cis_set_exposure_time failed");

	ret = CALL_CISOPS(cis, cis_compensate_gain_for_extremely_br, subdev_cis, expo_ms,
			  &again_milli, &dgain_milli);
	PUNIT_EXPECT(ret, "cis_compensate_gain_for_extremely_br failed");

	/* again & dgain should be same with input value */
	PUNIT_EXPECT(again_milli != 1000, "again_milli != 1000");
	PUNIT_EXPECT(dgain_milli != 1000, "dgain_milli != 1000");

	again.long_val = again.short_val = again.middle_val = again_milli;
	dgain.long_val = dgain.short_val = dgain.middle_val = dgain_milli;

	ret = CALL_CISOPS(cis, cis_set_analog_gain, subdev_cis, &again);
	PUNIT_EXPECT(ret, "cis_set_analog_gain failed");

	ret = CALL_CISOPS(cis, cis_set_digital_gain, subdev_cis, &dgain);
	PUNIT_EXPECT(ret, "cis_set_digital_gain failed");

	usleep_range(delay_us, delay_us + 1);

	/* test ns_per_cit/2 shorter exposure than cit_compensation_threshold */
	if (cit_compensation_threshold) {
		expo_ms =
			(cit_compensation_threshold * ns_per_cit / 1000) - (ns_per_cit / 1000 / 2);
		expo.long_val = expo.short_val = expo.middle_val = expo_ms;
		again_milli = 1000; // x1 analog gain
		dgain_milli = 1000; // x1 digital gain

		ret = CALL_CISOPS(cis, cis_set_exposure_time, subdev_cis, &expo);
		PUNIT_EXPECT(ret, "cis_set_exposure_time failed");

		ret = CALL_CISOPS(cis, cis_compensate_gain_for_extremely_br, subdev_cis, expo_ms,
				  &again_milli, &dgain_milli);
		PUNIT_EXPECT(ret, "cis_compensate_gain_for_extremely_br failed");

		again.long_val = again.short_val = again.middle_val = again_milli;
		dgain.long_val = dgain.short_val = dgain.middle_val = dgain_milli;

		ret = CALL_CISOPS(cis, cis_set_analog_gain, subdev_cis, &again);
		PUNIT_EXPECT(ret, "cis_set_analog_gain failed");

		ret = CALL_CISOPS(cis, cis_set_digital_gain, subdev_cis, &dgain);
		PUNIT_EXPECT(ret, "cis_set_digital_gain failed");
	}

	usleep_range(delay_us, delay_us + 1);

	/* test proximate max again to check again & dgain compensation */
	if (mode_info) {
		ns_per_cit =
			(u64)1000000 * 1000 * cis->cis_data->line_length_pck / cis->cis_data->pclk;
		expo_ms = ((mode_info->min_cit + 1) * ns_per_cit / 1000) + (ns_per_cit / 1000 / 2);
		expo.long_val = expo.short_val = expo.middle_val = expo_ms;
		again_milli = CALL_CISOPS(cis, cis_calc_again_permile, mode_info->max_analog_gain);
		again_milli -= 10; // max analog gain - x0.01x
		input_again_milli = again_milli;
		input_dgain_milli = dgain_milli = 1000; // x1 digital gain

		ret = CALL_CISOPS(cis, cis_set_exposure_time, subdev_cis, &expo);
		PUNIT_EXPECT(ret, "cis_set_exposure_time failed");

		ret = CALL_CISOPS(cis, cis_compensate_gain_for_extremely_br, subdev_cis, expo_ms,
				  &again_milli, &dgain_milli);
		PUNIT_EXPECT(ret, "cis_compensate_gain_for_extremely_br failed");

		/* again or dgain should compensate exposure */
		PUNIT_EXPECT(again_milli == input_again_milli, "again_milli == input_again_milli");
		PUNIT_EXPECT(dgain_milli == input_dgain_milli, "dgain_milli == input_dgain_milli");

		again.long_val = again.short_val = again.middle_val = again_milli;
		dgain.long_val = dgain.short_val = dgain.middle_val = dgain_milli;

		ret = CALL_CISOPS(cis, cis_set_analog_gain, subdev_cis, &again);
		PUNIT_EXPECT(ret, "cis_set_analog_gain failed");

		ret = CALL_CISOPS(cis, cis_set_digital_gain, subdev_cis, &dgain);
		PUNIT_EXPECT(ret, "cis_set_digital_gain failed");
	}

	ret = CALL_CISOPS(cis, cis_stream_off, subdev_cis);
	PUNIT_EXPECT(ret, "cis_stream_off failed");

	ret = CALL_CISOPS(cis, cis_wait_streamoff, subdev_cis);
	PUNIT_EXPECT(ret, "cis_wait_streamoff failed");

	cis_control_power(module, false);

	return ret;
}

static int cis_sensor_info_test(struct is_module_enum *module)
{
	const struct sensor_cis_mode_info *mode_info;
	int i;
	u32 mode, max_fps, ex_mode, hwformat;
	u64 pclk_per_frame;

	PUNIT_ASSERT_ZERO_RET(!cis->sensor_info);
	PUNIT_ASSERT_ZERO_RET(!cis->sensor_info->version);
	PUNIT_EXPECT(cis->sensor_info->max_width <= 0, "cis->sensor_info->max_width <= 0");
	PUNIT_EXPECT(cis->sensor_info->max_height <= 0, "cis->sensor_info->max_height <= 0");
	PUNIT_EXPECT(cis->sensor_info->mode_count <= 0, "cis->sensor_info->mode_count <= 0");
	PUNIT_EXPECT(cis->sensor_info->fine_integration_time <= 0,
		     "cis->sensor_info->fine_integration_time <= 0");

	for (i = 0; i < module->cfgs; i++) {
		mode = module->cfg[i].mode;
		ex_mode = module->cfg[i].ex_mode;
		max_fps = module->cfg[i].max_fps;
		PUNIT_EXPECT(max_fps <= 0, "max_fps <= 0");

		mode_info = cis->sensor_info->mode_infos[mode];
		PUNIT_ASSERT_ERR_RET(!mode_info);
		hwformat = module->cfg[i].output[0][CSI_VIRTUAL_CH_0].hwformat;

		info("module index:%d, mode setting index:[%d, %d, %d], max_fps:%d, version:%s, hwformat:%d",
		     i, mode, cis->sensor_info->mode_count, mode_info->setfile_index, max_fps,
		     cis->sensor_info->version, hwformat);

		if (hwformat == HW_FORMAT_RAW10) {
			PUNIT_EXPECT(mode_info->state_12bit != SENSOR_12BIT_STATE_OFF,
				     "mode_info->state_12bit != SENSOR_12BIT_STATE_OFF");
		} else if (hwformat == HW_FORMAT_RAW12) {
			PUNIT_EXPECT(mode_info->state_12bit == SENSOR_12BIT_STATE_OFF,
				     "mode_info->state_12bit == SENSOR_12BIT_STATE_OFF");
		}

		PUNIT_EXPECT(mode > cis->sensor_info->mode_count,
			     "mode > cis->sensor_info->mode_count");
		PUNIT_EXPECT(mode != mode_info->setfile_index, "mode != mode_info->setfile_index");

		if (ex_mode != EX_DUALFPS_480 && ex_mode != EX_DUALFPS_960) {
			CALL_CISOPS(cis, cis_data_calculation, subdev_cis, mode);
			PUNIT_EXPECT(max_fps > cis->cis_data->max_fps,
				     "max_fps > cis->cis_data->max_fps");
		}
	}

	for (i = 0; i < cis->sensor_info->mode_count; i++) {
		mode_info = cis->sensor_info->mode_infos[i];
		PUNIT_ASSERT_ERR_RET(!mode_info);
		PUNIT_EXPECT(mode_info->frame_length_lines <= 0,
			     "mode_info->frame_length_lines <= 0");
		PUNIT_EXPECT(mode_info->line_length_pck <= 0, "mode_info->line_length_pck <= 0");

		pclk_per_frame = (u64)mode_info->frame_length_lines * mode_info->line_length_pck;
		PUNIT_EXPECT(mode_info->pclk <= pclk_per_frame,
			     "mode_info->pclk <= pclk_per_frame");

		PUNIT_EXPECT(mode_info->max_analog_gain <= cis->sensor_info->min_analog_gain,
			     "mode_info->max_analog_gain <= cis->sensor_info->min_analog_gain");
		PUNIT_EXPECT(mode_info->max_digital_gain <= cis->sensor_info->min_digital_gain,
			     "mode_info->max_digital_gain <= cis->sensor_info->min_digital_gain");

		PUNIT_EXPECT(mode_info->min_cit <= 0, "mode_info->min_cit <= 0");
		PUNIT_EXPECT(mode_info->min_cit <= mode_info->frame_length_lines,
			     "mode_info->min_cit <= mode_info->frame_length_lines");

		PUNIT_EXPECT(mode_info->max_cit_margin <= 0, "mode_info->max_cit_margin <= 0");
		PUNIT_EXPECT(mode_info->max_cit_margin >= mode_info->frame_length_lines,
			     "mode_info->max_cit_margin >= mode_info->frame_length_lines");

		PUNIT_EXPECT(mode_info->align_cit <= 0, "mode_info->align_cit <= 0");
	}

	return 0;
}

static int cis_test_all_camera_punit_test(enum cis_test_type type)
{
	int ret = 0;
	struct is_core *core = is_get_is_core();
	struct is_module_enum *module;
	int pos, i;

	for (pos = 0; pos < SENSOR_POSITION_MAX; pos++) {
		for (i = 0; i < IS_SENSOR_COUNT; i++) {
			is_search_sensor_module_with_position(&core->sensor[i], pos, &module);
			if (!module)
				continue;

			ret = cis_set_global_sensor_peri(module);
			if (ret) {
				err("cis_set_global_sensor_peri error: %d", ret);
				return ret;
			}

			info("[%s] [%s] cis_ixc_type %d\n", __func__, module->sensor_name,
			     module->pdata->cis_ixc_type);

			switch (type) {
			case CIS_TEST_I2C:
				if (module->pdata->cis_ixc_type == I2C_TYPE)
					ret = cis_ixc_transfer_test(module);
				break;
			case CIS_TEST_I3C:
				if (module->pdata->cis_ixc_type == I3C_TYPE)
					ret = cis_ixc_transfer_test(module);
				break;
			case CIS_TEST_LONG_EXPOSURE:
				ret = cis_long_exposure_test(module);
				break;
			case CIS_TEST_CONTROL_PER_FRAME:
				ret = cis_sensor_control_per_frame_test(module);
				break;
			case CIS_TEST_RECOVERY:
				ret = cis_sensor_recovery_test(module);
				break;
			case CIS_TEST_ADAPTIVE_MIPI:
				ret = cis_sensor_adaptive_mipi_test(module);
				break;
			case CIS_TEST_UPDATE_SEAMLESS_LN_MODE:
				ret = cis_sensor_update_seamless_ln_mode_test(module);
				break;
			case CIS_TEST_COMPENSATE_EXPOSURE:
				ret = cis_sensor_compensate_exposure_test(module);
				break;
			case CIS_TEST_SENSOR_INFO:
				ret = cis_sensor_info_test(module);
				break;
			default:
				break;
			}
		}
	}

	return ret;
}

static int pablo_test_set_cis_run(const char *val, const struct kernel_param *kp)
{
	int ret, argc;
	char **argv;

	argv = argv_split(GFP_KERNEL, val, &argc);
	if (!argv) {
		err("No argument!");
		return -EINVAL;
	}

	ret = kstrtouint(argv[0], 0, &cr_info.act);
	if (ret) {
		err("Invalid act %d ret %d", cr_info.act, ret);
		goto func_exit;
	}

	switch (cr_info.act) {
	case PABLO_CIS_RUN_STOP:
		if (argc < 1) {
			err("Not enough parameters. %d < 1", argc);
			goto func_exit;
		}

		pst_result_cis_opt(pst_cis_result);

		info("Stop cis self test\n");
		break;
	case PABLO_CIS_RUN_START:
		if (argc < 2) {
			err("Not enough parameters. %d < 2", argc);
			err("ex) echo 1 0");
			goto func_exit;
		}

		ret = kstrtouint(argv[1], 0, &cr_info.type);
		if (ret) {
			err("Invalid parameters(ret %d)", ret);
			goto func_exit;
		}

		info("Start cis self test (type:%d)\n", cr_info.type);
		pst_cis_result = cis_test_all_camera_punit_test(cr_info.type);
		break;
	default:
		err("NOT supported act %u", cr_info.act);
		goto func_exit;
	}

func_exit:
	argv_free(argv);
	return ret;
}

static int pablo_test_get_cis_run(char *buffer, const struct kernel_param *kp)
{
	int ret;

	ret = sprintf(buffer, "> Set\n");
	ret += sprintf(buffer + ret, "# echo <act> <type> > cis_run\n");
	ret += sprintf(buffer + ret, " - act: 0=PABLO_CIS_RUN_STOP, 1=PABLO_CIS_RUN_START\n");
	ret += sprintf(buffer + ret, "act %d type %d\n", cr_info.act, cr_info.type);

	return ret;
}
