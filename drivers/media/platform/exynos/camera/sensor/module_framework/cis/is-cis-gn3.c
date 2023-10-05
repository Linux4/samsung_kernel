// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series Sensor driver
 *
 *
 * Copyright (c) 2021 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/version.h>
#include <linux/gpio.h>
#include <linux/clk.h>
#include <linux/regulator/consumer.h>
#include <linux/videodev2.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/platform_device.h>
#include <linux/of_gpio.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-device.h>
#include <media/v4l2-subdev.h>

#include <exynos-is-sensor.h>
#include <videodev2_exynos_camera.h>
#include "is-hw.h"
#include "is-core.h"
#include "is-param.h"
#include "is-device-sensor.h"
#include "is-device-sensor-peri.h"
#include "is-resourcemgr.h"
#include "is-dt.h"
#include "is-cis-gn3.h"
#ifndef CONFIG_CAMERA_VENDER_MCD
#include "is-cis-gn3-setA.h"
#endif
#include "is-cis-gn3-setA-19p2.h"
#include "is-helper-ixc.h"

#define SENSOR_NAME "S5KGN3"
/* #define DEBUG_GN3_PLL */
/* #define DEBUG_CAL_WRITE */
static const u32 *sensor_gn3_global;
static u32 sensor_gn3_global_size;
static const u32 **sensor_gn3_setfiles;
static const u32 *sensor_gn3_setfile_sizes;
static const struct sensor_pll_info_compact **sensor_gn3_pllinfos;
static u32 sensor_gn3_max_setfile_num;
#if IS_ENABLED(USE_CAMERA_SENSOR_RETENTION)
static const u32 *sensor_gn3_global_retention;
static u32 sensor_gn3_global_retention_size;
static const u32 **sensor_gn3_retention;
static const u32 *sensor_gn3_retention_size;
static u32 sensor_gn3_max_retention_num;
static const u32 **sensor_gn3_load_sram;
static const u32 *sensor_gn3_load_sram_size;
#endif

/* For Recovery */
static u32 sensor_gn3_frame_duration_backup;
static struct ae_param sensor_gn3_again_backup;
static struct ae_param sensor_gn3_dgain_backup;
static struct ae_param sensor_gn3_target_exp_backup;

static bool sensor_gn3_eeprom_cal_available;
static bool sensor_gn3_first_entrance;
static bool sensor_gn3_load_retention;
static bool sensor_gn3_need_stream_on_retention;

#define CIS_CALIBRATION 0

#if IS_ENABLED(CIS_CALIBRATION)
int sensor_gn3_cis_set_cal(struct v4l2_subdev *subdev);
#endif

#if IS_ENABLED(USE_CAMERA_SENSOR_RETENTION)
void sensor_gn3_cis_set_mode_group(u32 mode)
{
	sensor_gn3_mode_groups[SENSOR_GN3_MODE_DEFAULT] = mode;
	sensor_gn3_mode_groups[SENSOR_GN3_MODE_IDCG] = MODE_GROUP_NONE;
	sensor_gn3_mode_groups[SENSOR_GN3_MODE_LN] = MODE_GROUP_NONE;

	switch (mode) {
	default:
		sensor_gn3_mode_groups[SENSOR_GN3_MODE_LN] = MODE_GROUP_NONE;
	}

	switch (mode) {
	default:
		sensor_gn3_mode_groups[SENSOR_GN3_MODE_IDCG] = MODE_GROUP_NONE;
	}
}
#endif

static void sensor_gn3_set_integration_min(u32 mode, cis_shared_data *cis_data)
{
	WARN_ON(!cis_data);

	if (mode < 0 || mode >= SENSOR_GN3_MODE_MAX) {
		err("invalid mode(%d)!!", mode);
		return;
	}

	cis_data->min_coarse_integration_time = sensor_gn3_common_mode_attr[mode].min_coarse_integration_time;

	dbg_sensor(1, "min_coarse_integration_time(%d)\n", cis_data->min_coarse_integration_time);
}

static void sensor_gn3_set_integration_max_margin(u32 mode, cis_shared_data *cis_data)
{
	WARN_ON(!cis_data);

	if (mode < 0 || mode >= SENSOR_GN3_MODE_MAX) {
		err("invalid mode(%d)!!", mode);
		return;
	}

	cis_data->max_margin_coarse_integration_time
		= sensor_gn3_common_mode_attr[mode].max_margin_coarse_integration_time;

	dbg_sensor(1, "max_margin_coarse_integration_time(%d)\n", cis_data->max_margin_coarse_integration_time);
}

static void sensor_gn3_cis_data_calculation(const struct sensor_pll_info_compact *pll_info_compact,
						cis_shared_data *cis_data)
{
	u64 vt_pix_clk_hz = 0;
	u32 frame_rate = 0, max_fps = 0, frame_valid_us = 0;

	WARN_ON(!pll_info_compact);

	/* 1. get pclk value from pll info */
	vt_pix_clk_hz = pll_info_compact->pclk;

	dbg_sensor(1, "ext_clock(%d), mipi_datarate(%ld), pclk(%ld)\n",
			pll_info_compact->ext_clk, pll_info_compact->mipi_datarate, pll_info_compact->pclk);

	/* 2. the time of processing one frame calculation (us) */
	cis_data->min_frame_us_time = (((u64)pll_info_compact->frame_length_lines)
					* pll_info_compact->line_length_pck * 1000 / (vt_pix_clk_hz / 1000));
	cis_data->cur_frame_us_time = cis_data->min_frame_us_time;
#ifdef CAMERA_REAR2
	cis_data->min_sync_frame_us_time = cis_data->min_frame_us_time;
#endif
	/* 3. FPS calculation */
	frame_rate = vt_pix_clk_hz / (pll_info_compact->frame_length_lines * pll_info_compact->line_length_pck);
	dbg_sensor(1, "frame_rate (%d) = vt_pix_clk_hz(%lld) / "
		KERN_CONT "(pll_info_compact->frame_length_lines(%d) * pll_info_compact->line_length_pck(%d))\n",
		frame_rate, vt_pix_clk_hz, pll_info_compact->frame_length_lines, pll_info_compact->line_length_pck);

	/* calculate max fps */
	max_fps = (vt_pix_clk_hz * 10) / (pll_info_compact->frame_length_lines * pll_info_compact->line_length_pck);
	max_fps = (max_fps % 10 >= 5 ? frame_rate + 1 : frame_rate);

	cis_data->pclk = vt_pix_clk_hz;
	cis_data->max_fps = max_fps;
	cis_data->frame_length_lines = pll_info_compact->frame_length_lines;
	cis_data->line_length_pck = pll_info_compact->line_length_pck;
	cis_data->line_readOut_time = (u64)cis_data->line_length_pck * 1000
					* 1000 * 1000 / cis_data->pclk;

	/* Frame valid time calcuration */
	frame_valid_us = (u64)cis_data->cur_height * cis_data->line_length_pck
				* 1000 * 1000 / cis_data->pclk;
	cis_data->frame_valid_us_time = (unsigned int)frame_valid_us;

	dbg_sensor(1, "%s\n", __func__);
	dbg_sensor(1, "Sensor size(%d x %d) setting: SUCCESS!\n",
					cis_data->cur_width, cis_data->cur_height);
	dbg_sensor(1, "Frame Valid(us): %d\n", frame_valid_us);

	dbg_sensor(1, "Fps: %d, max fps(%d)\n", frame_rate, cis_data->max_fps);
	dbg_sensor(1, "min_frame_time(%d us)\n", cis_data->min_frame_us_time);
	dbg_sensor(1, "Pixel rate(Kbps): %d\n", cis_data->pclk / 1000);

	/* Frame period calculation */
	cis_data->frame_time = (cis_data->line_readOut_time * cis_data->cur_height / 1000);
	switch (cis_data->cur_lownoise_mode) {
	case IS_CIS_LN2:
		cis_data->rolling_shutter_skew = (cis_data->cur_height - 1) * cis_data->line_readOut_time * 2;
		break;
	case IS_CIS_LN4:
		cis_data->rolling_shutter_skew = (cis_data->cur_height - 1) * cis_data->line_readOut_time * 4;
		break;
	case IS_CIS_LNOFF:
	default:
		cis_data->rolling_shutter_skew = (cis_data->cur_height - 1) * cis_data->line_readOut_time;
		break;
	}

	dbg_sensor(1, "[%s] frame_time(%d), rolling_shutter_skew(%lld)\n", __func__,
		cis_data->frame_time, cis_data->rolling_shutter_skew);

	/* Constant values */
	cis_data->min_fine_integration_time = SENSOR_GN3_FINE_INTEGRATION_TIME_MIN;
	cis_data->max_fine_integration_time = SENSOR_GN3_FINE_INTEGRATION_TIME_MAX;
}

int sensor_gn3_cis_wait_streamon(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_cis *cis;
	struct i2c_client *client;
	int retry_count = 0;
	int max_retry_count = 10;

	WARN_ON(!subdev);

	if (sensor_gn3_need_stream_on_retention) {
		for (retry_count = 0; retry_count < max_retry_count; retry_count++) {
			ret = sensor_cis_wait_streamon(subdev);
			if (ret < 0)
				err("[%s] wait failed retry %d", __func__, retry_count);
			else
				break;
		}
		if (ret < 0)
			goto p_err;
	} else {
		ret = sensor_cis_wait_streamon(subdev);
		if (ret < 0)
			goto p_err;
	}

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);
	if (unlikely(!cis)) {
		err("cis is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	client = cis->client;
	if (unlikely(!client)) {
		err("client is NULL");
		ret = -EINVAL;
		goto p_err;
	}

p_err:
	return ret;
}

void sensor_gn3_cis_data_calc(struct v4l2_subdev *subdev, u32 mode)
{
	struct is_cis *cis = NULL;

	WARN_ON(!subdev);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);
	WARN_ON(!cis);
	WARN_ON(!cis->cis_data);

	if (mode >= sensor_gn3_max_setfile_num) {
		err("invalid mode(%d/%d)!!", mode, sensor_gn3_max_setfile_num);
		return;
	}

	if (cis->cis_data->stream_on) {
		info("[%s] call mode change in stream on state\n", __func__);
		sensor_gn3_cis_wait_streamon(subdev);
		sensor_gn3_cis_stream_off(subdev);
		sensor_cis_wait_streamoff(subdev);
		info("[%s] stream off done\n", __func__);
	}

	sensor_gn3_cis_data_calculation(sensor_gn3_pllinfos[mode], cis->cis_data);
}

static int sensor_gn3_wait_stream_off_status(cis_shared_data *cis_data)
{
	int ret = 0;
	u32 timeout = 0;

	WARN_ON(!cis_data);

#define STREAM_OFF_WAIT_TIME 250
	while (timeout < STREAM_OFF_WAIT_TIME) {
		if (cis_data->is_active_area == false &&
				cis_data->stream_on == false) {
			pr_debug("actual stream off\n");
			break;
		}
		timeout++;
	}

	if (timeout == STREAM_OFF_WAIT_TIME) {
		pr_err("actual stream off wait timeout\n");
		ret = -1;
	}

	return ret;
}

int sensor_gn3_cis_select_setfile(struct v4l2_subdev *subdev)
{
	int ret = 0;
	u16 rev = 0;
	u8 feature_id = 0;
	struct is_cis *cis = NULL;

	WARN_ON(!subdev);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);
	WARN_ON(!cis);
	WARN_ON(!cis->cis_data);

	rev = cis->cis_data->cis_rev;

	ret |= cis->ixc_ops->write16(cis->client, 0xFCFC, 0x4000);
	ret |= cis->ixc_ops->read8(cis->client, 0x000E, &feature_id);
	if (ret < 0)
		err("[%s] read feature_id fail!!", __func__);

	info("gn3 sensor revision(%#x)\n", rev);
	info("gn3 sensor feature id(%#x)\n", feature_id);

	if (rev == 0xA000)
		info("gn3 rev EVT0 (%#x)\n", rev);
	else if (rev == 0xB000)
		info("gn3 rev EVT1 (%#x)\n", rev);

	if (feature_id == 0x03)
		info("gn3 feature id (%#x)\n", feature_id);
	else if (feature_id == 0x04)
		info("gn3+ feature id (%#x)\n", feature_id);

	return ret;
}

/* CIS OPS */
int sensor_gn3_cis_init(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_cis *cis;
	u32 setfile_index = 0;
	cis_setting_info setinfo;

	setinfo.param = NULL;
	setinfo.return_value = 0;

	WARN_ON(!subdev);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);
	if (!cis) {
		err("cis is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	WARN_ON(!cis->cis_data);
#if !defined(CONFIG_CAMERA_VENDER_MCD)
	memset(cis->cis_data, 0, sizeof(cis_shared_data));

	ret = sensor_cis_check_rev(cis);
	if (ret < 0) {
		warn("sensor_gn3_check_rev is fail when cis init");
		ret = -EINVAL;
		goto p_err;
	}
#endif

	sensor_gn3_first_entrance = true;
	cis->cis_data->sens_config_index_pre = SENSOR_GN3_MODE_MAX;

	sensor_gn3_cis_select_setfile(subdev);

	cis->cis_data->stream_on = false;
	cis->cis_data->cur_width = SENSOR_GN3_MAX_WIDTH;
	cis->cis_data->cur_height = SENSOR_GN3_MAX_HEIGHT;
	cis->cis_data->low_expo_start = 33000;
	cis->cis_data->highres_capture_mode = false;
	cis->cis_data->pre_12bit_mode = SENSOR_12BIT_STATE_OFF;
	cis->cis_data->cur_12bit_mode = SENSOR_12BIT_STATE_OFF;
	cis->cis_data->pre_lownoise_mode = IS_CIS_LNOFF;
	cis->cis_data->cur_lownoise_mode = IS_CIS_LNOFF;
	cis->need_mode_change = false;
	cis->long_term_mode.sen_strm_off_on_step = 0;
	cis->long_term_mode.sen_strm_off_on_enable = false;
	cis->mipi_clock_index_cur = CAM_MIPI_NOT_INITIALIZED;
	cis->mipi_clock_index_new = CAM_MIPI_NOT_INITIALIZED;

	sensor_gn3_load_retention = false;

	sensor_gn3_cis_data_calculation(sensor_gn3_pllinfos[setfile_index], cis->cis_data);
	sensor_gn3_set_integration_min(setfile_index, cis->cis_data);
	sensor_gn3_set_integration_max_margin(setfile_index, cis->cis_data);

	setinfo.return_value = 0;
	CALL_CISOPS(cis, cis_get_min_exposure_time, subdev, &setinfo.return_value);
	dbg_sensor(1, "[%s] min exposure time : %d\n", __func__, setinfo.return_value);
	setinfo.return_value = 0;
	CALL_CISOPS(cis, cis_get_max_exposure_time, subdev, &setinfo.return_value);
	dbg_sensor(1, "[%s] max exposure time : %d\n", __func__, setinfo.return_value);
	setinfo.return_value = 0;
	CALL_CISOPS(cis, cis_get_min_analog_gain, subdev, &setinfo.return_value);
	dbg_sensor(1, "[%s] min again : %d\n", __func__, setinfo.return_value);
	setinfo.return_value = 0;
	CALL_CISOPS(cis, cis_get_max_analog_gain, subdev, &setinfo.return_value);
	dbg_sensor(1, "[%s] max again : %d\n", __func__, setinfo.return_value);
	setinfo.return_value = 0;
	CALL_CISOPS(cis, cis_get_min_digital_gain, subdev, &setinfo.return_value);
	dbg_sensor(1, "[%s] min dgain : %d\n", __func__, setinfo.return_value);
	setinfo.return_value = 0;
	CALL_CISOPS(cis, cis_get_max_digital_gain, subdev, &setinfo.return_value);
	dbg_sensor(1, "[%s] max dgain : %d\n", __func__, setinfo.return_value);

	/* CALL_CISOPS(cis, cis_log_status, subdev); */

	/* if (IS_ENABLED(DEBUG_SENSOR_TIME)) */
	/* dbg_sensor(1, "[%s] time %ldus", __func__, PABLO_KTIME_US_DELTA_NOW(st));*/ /* pamir bringup */

p_err:
	return ret;
}

int sensor_gn3_cis_deinit(struct v4l2_subdev *subdev)
{
	int ret = 0;

#if IS_ENABLED(USE_CAMERA_SENSOR_RETENTION)
	if (sensor_gn3_load_retention == false) {
		pr_info("%s: need to load retention\n", __func__);
		sensor_gn3_cis_stream_on(subdev);
		sensor_gn3_cis_wait_streamon(subdev);
		sensor_gn3_cis_stream_off(subdev);
		sensor_cis_wait_streamoff(subdev);
		pr_info("%s: complete to load retention\n", __func__);
	}

	/* retention mode CRC wait calculation */
	usleep_range(10000, 10000);
#endif

	return ret;
}

static const struct is_cis_log log_gn3[] = {
	/* 4000 page */
	{I2C_WRITE, 16, 0x6000, 0x0005, "page unlock"},
	{I2C_WRITE, 16, 0xFCFC, 0x4000, "0x4000 page"},
	{I2C_READ, 16, 0x0000, 0, "model_id"},
	{I2C_READ, 16, 0x0002, 0, "revision_number"},
	{I2C_READ, 8, 0x0005, 0, "frame_count"},
	{I2C_READ, 16, 0x0100, 0, "0x0100"},
	{I2C_READ, 16, 0x0202, 0, "0x0202"},
	{I2C_READ, 16, 0x0702, 0, "0x0702"},
	{I2C_READ, 16, 0x0704, 0, "0x0704"},
	{I2C_READ, 16, 0x0340, 0, "0x0340"},
	{I2C_READ, 16, 0x0342, 0, "0x0342"},
	{I2C_READ, 16, 0x0B30, 0, "0x0B30"},
	/* restore 4000 page */
	{I2C_WRITE, 16, 0xFCFC, 0x4000, "0x4000 page"},
	{I2C_WRITE, 16, 0x6000, 0x0085, "page lock"},
};

int sensor_gn3_cis_log_status(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_cis *cis;
	struct i2c_client *client = NULL;
#if IS_ENABLED(USE_CAMERA_SENSOR_RETENTION)
	struct is_module_enum *module;
	struct is_device_sensor_peri *sensor_peri = NULL;
	struct sensor_open_extended *ext_info;
#endif

	WARN_ON(!subdev);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);
	if (!cis) {
		err("cis is NULL");
		ret = -ENODEV;
		goto p_err;
	}

	client = cis->client;
	if (unlikely(!client)) {
		err("client is NULL");
		ret = -ENODEV;
		goto p_err;
	}

#if IS_ENABLED(USE_CAMERA_SENSOR_RETENTION)
	sensor_peri = container_of(cis, struct is_device_sensor_peri, cis);
	module = sensor_peri->module;
	ext_info = &module->ext;
	FIMC_BUG(!ext_info);

	if (ext_info->use_retention_mode != SENSOR_RETENTION_UNSUPPORTED)
		ext_info->use_retention_mode = SENSOR_RETENTION_INACTIVE;
#endif

	sensor_cis_log_status(cis, client, log_gn3,
			ARRAY_SIZE(log_gn3), (char *)__func__);

p_err:
	return ret;
}

#if USE_GROUP_PARAM_HOLD
static int sensor_gn3_cis_group_param_hold_func(struct v4l2_subdev *subdev, unsigned int hold)
{
	int ret = 0;
	struct is_cis *cis = NULL;
	struct i2c_client *client = NULL;

	WARN_ON(!subdev);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);

	WARN_ON(!cis);
	WARN_ON(!cis->cis_data);

	client = cis->client;
	if (unlikely(!client)) {
		err("client is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	if (hold == cis->cis_data->group_param_hold) {
		dbg_sensor(1, "%s : already group_param_hold (%d)\n", __func__, cis->cis_data->group_param_hold);
		goto p_err;
	}

	ret = cis->ixc_ops->write8(client, 0x0104, hold); /* api_rw_general_setup_grouped_parameter_hold */
	dbg_sensor(1, "%s : hold = %d, ret = %d\n", __func__, hold, ret);
	if (ret < 0)
		goto p_err;

	cis->cis_data->group_param_hold = hold;
	ret = hold;
p_err:
	return ret;
}
#else
static inline int sensor_gn3_cis_group_param_hold_func(struct v4l2_subdev *subdev, unsigned int hold)
{ return 0; }
#endif

/* Input
 *	hold : true - hold, flase - no hold
 * Output
 *	  return: 0 - no effect(already hold or no hold)
 *		positive - setted by request
 *		negative - ERROR value
 */
int sensor_gn3_cis_group_param_hold(struct v4l2_subdev *subdev, bool hold)
{
	int ret = 0;
	struct is_cis *cis = NULL;

	WARN_ON(!subdev);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);

	WARN_ON(!cis);
	WARN_ON(!cis->cis_data);

	I2C_MUTEX_LOCK(cis->i2c_lock);
	ret = sensor_gn3_cis_group_param_hold_func(subdev, hold);
	if (ret < 0)
		goto p_err;

p_err:
	I2C_MUTEX_UNLOCK(cis->i2c_lock);
	return ret;
}


int sensor_gn3_cis_set_global_setting_internal(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_cis *cis = NULL;
	struct is_module_enum *module;
	struct is_device_sensor_peri *sensor_peri = NULL;
	struct sensor_open_extended *ext_info;

	WARN_ON(!subdev);
	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);
	WARN_ON(!cis);
	sensor_peri = container_of(cis, struct is_device_sensor_peri, cis);
	module = sensor_peri->module;
	ext_info = &module->ext;
	WARN_ON(!ext_info);

	I2C_MUTEX_LOCK(cis->i2c_lock);
	info("[%s] global setting internal start\n", __func__);
	/* setfile global setting is at camera entrance */
	ret |= sensor_cis_set_registers(subdev, sensor_gn3_global, sensor_gn3_global_size);
	if (ret < 0) {
		err("sensor_gn3_set_registers fail!!");
		goto p_err;
	}

	info("[%s] global setting internal done\n", __func__);

	if (ext_info->use_retention_mode == SENSOR_RETENTION_UNSUPPORTED) {
#if IS_ENABLED(CIS_CALIBRATION)
		ret = sensor_gn3_cis_set_cal(subdev);
		if (ret < 0) {
			err("sensor_gn3_cis_set_cal fail!!");
			goto p_err;
		}
#endif
	}

p_err:
	I2C_MUTEX_UNLOCK(cis->i2c_lock);
	return ret;
}

#if IS_ENABLED(USE_CAMERA_SENSOR_RETENTION)
int sensor_gn3_cis_set_global_setting_retention(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_cis *cis = NULL;

	WARN_ON(!subdev);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);
	WARN_ON(!cis);

	I2C_MUTEX_LOCK(cis->i2c_lock);
	info("[%s] global retention setting start\n", __func__);
	/* setfile global retention setting is at camera entrance */
	ret = sensor_cis_set_registers(subdev, sensor_gn3_global_retention, sensor_gn3_global_retention_size);
	if (ret < 0) {
		err("sensor_gn3_set_registers fail!!");
		goto p_err;
	}

	info("[%s] global retention setting done\n", __func__);

p_err:
	I2C_MUTEX_UNLOCK(cis->i2c_lock);

	return ret;
}

#ifdef CAMERA_REAR2
int sensor_gn3_cis_retention_crc_enable(struct v4l2_subdev *subdev, u32 mode)
{
	int ret = 0;
	struct is_cis *cis = NULL;
	struct i2c_client *client;

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);
	WARN_ON(!cis);
	WARN_ON(!cis->cis_data);

	client = cis->client;
	if (unlikely(!client)) {
		err("client is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	switch (mode) {
	default:
		/* Sensor stream on */
		cis->ixc_ops->write16(client, 0x0100, 0x0103);

		/* retention mode CRC check register enable */
		cis->ixc_ops->write8(client, 0x010E, 0x01); /* api_rw_general_setup_checksum_on_ram_enable */
		info("[MOD:D:%d] %s : retention enable CRC check\n", cis->id, __func__);

		/* Sensor stream off */
		cis->ixc_ops->write8(client, 0x0100, 0x00);
		break;
	}

p_err:
	return ret;
}
#endif
#endif

bool sensor_gn3_cis_get_lownoise_supported(struct v4l2_subdev *subdev)
{
	struct is_cis *cis = NULL;
	struct is_module_enum *module;
	struct is_device_sensor_peri *sensor_peri = NULL;
	struct sensor_open_extended *ext_info;
	u32 mode = 0;

	WARN_ON(!subdev);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);
	WARN_ON(!cis);

	sensor_peri = container_of(cis, struct is_device_sensor_peri, cis);
	module = sensor_peri->module;
	ext_info = &module->ext;
	WARN_ON(!ext_info);

	if (ext_info->use_retention_mode != SENSOR_RETENTION_ACTIVATED)
		return false;

	WARN_ON(!cis->cis_data);

	mode = cis->cis_data->sens_config_index_cur;

	if (mode < 0 || mode >= SENSOR_GN3_MODE_MAX) {
		err("invalid mode(%d)!!", mode);
		return false;
	}

	return sensor_gn3_spec_mode_attr[mode].ln_support;
}

#ifdef GN3_BURST_WRITE
int sensor_gn3_cis_write16_burst(struct i2c_client *client, u16 addr, u8 *val, u32 num, bool endian)
{
	int ret = 0;
	struct i2c_msg msg[1];
	int i = 0;
	u8 *wbuf;

	if (val == NULL) {
		pr_err("val array is null\n");
		ret = -ENODEV;
		goto p_err;
	}

	if (!client->adapter) {
		pr_err("Could not find adapter!\n");
		ret = -ENODEV;
		goto p_err;
	}

	wbuf = kmalloc((2 + (num * 2)), GFP_KERNEL);
	if (!wbuf) {
		ret = -ENODEV;
		goto p_err;
	}

	msg->addr = client->addr;
	msg->flags = 0;
	msg->len = 2 + (num * 2);
	msg->buf = wbuf;
	wbuf[0] = (addr & 0xFF00) >> 8;
	wbuf[1] = (addr & 0xFF);
	if (endian == GN3_BIG_ENDIAN) {
		for (i = 0; i < num; i++) {
			wbuf[(i * 2) + 2] = (val[(i * 2)] & 0xFF);
			wbuf[(i * 2) + 3] = (val[(i * 2) + 1] & 0xFF);
			ixc_info("I2CW16(%d) [0x%04x] : 0x%x%x\n",
				client->addr, addr, (val[(i * 2)] & 0xFF), (val[(i * 2) + 1] & 0xFF));
		}
	} else {
		for (i = 0; i < num; i++) {
			wbuf[(i * 2) + 2] = (val[(i * 2) + 1] & 0xFF);
			wbuf[(i * 2) + 3] = (val[(i * 2)] & 0xFF);
			ixc_info("I2CW16(%d) [0x%04x] : 0x%x%x\n",
				client->addr, addr, (val[(i * 2)] & 0xFF), (val[(i * 2) + 1] & 0xFF));
		}
	}

	ret = is_i2c_transfer(client->adapter, msg, 1);
	if (ret < 0) {
		pr_err("i2c transfer fail(%d)", ret);
		goto p_err_free;
	}

	kfree(wbuf);
	return 0;

p_err_free:
	kfree(wbuf);
p_err:
	return ret;
}
#endif

#if IS_ENABLED(CIS_CALIBRATION)
int sensor_gn3_cis_set_cal(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_rom_info *finfo = NULL;
	char *cal_buf = NULL;
	struct is_cis *cis = NULL;
	bool endian = GN3_BIG_ENDIAN;
	int start_addr = 0, end_addr = 0;
#ifdef GN3_BURST_WRITE
	int cal_size = 0;
#endif
	int i = 0;
	u16 val = 0;
	int len = 0;

	info("[%s] E", __func__);

	if (sensor_gn3_eeprom_cal_available)
		return 0;

	is_sec_get_sysfs_finfo(&finfo, ROM_ID_REAR);

	if (!test_bit(IS_ROM_STATE_CAL_READ_DONE, &finfo->rom_state)) {
		err("eeprom read fail status, skip set cal");
		sensor_gn3_eeprom_cal_available = false;
		return 0;
	}

	info("[%s] eeprom read, start set cal\n", __func__);
	is_sec_get_cal_buf(&cal_buf, ROM_ID_REAR);

#ifdef CAMERA_GN3_CAL_MODULE_VERSION
	if (finfo->header_ver[10] < CAMERA_GN3_CAL_MODULE_VERSION) {
		start_addr = finfo->rom_xtc_cal_data_addr_list[GN3_CAL_START_ADDR];
		if (cal_buf[start_addr + 2] == 0xFF && cal_buf[start_addr + 3] == 0xFF &&
			cal_buf[start_addr + 4] == 0xFF && cal_buf[start_addr + 5] == 0xFF &&
			cal_buf[start_addr + 6] == 0xFF && cal_buf[start_addr + 7] == 0xFF &&
			cal_buf[start_addr + 8] == 0xFF && cal_buf[start_addr + 9] == 0xFF &&
			cal_buf[start_addr + 10] == 0xFF && cal_buf[start_addr + 11] == 0xFF &&
			cal_buf[start_addr + 12] == 0xFF && cal_buf[start_addr + 13] == 0xFF &&
			cal_buf[start_addr + 14] == 0xFF && cal_buf[start_addr + 15] == 0xFF &&
			cal_buf[start_addr + 16] == 0xFF && cal_buf[start_addr + 17] == 0xFF) {
			info("empty Cal - cal offset[0x%04X] = val[0x%02X], cal offset[0x%04X] = val[0x%02X]",
				start_addr + 2, cal_buf[start_addr + 2], start_addr + 17, cal_buf[start_addr + 17]);
			info("[%s] empty Cal", __func__);
			return 0;
		}

		len = (finfo->rom_xtc_cal_data_addr_list_len / GN3_CAL_ROW_LEN) - 1;
		if (len >= 0) {
			end_addr = finfo->rom_xtc_cal_data_addr_list[len * GN3_CAL_ROW_LEN + GN3_CAL_END_ADDR];
			if (end_addr >= 15) {
				if (cal_buf[end_addr] == 0xFF && cal_buf[end_addr - 1] == 0xFF &&
					cal_buf[end_addr - 2] == 0xFF && cal_buf[end_addr - 3] == 0xFF &&
					cal_buf[end_addr - 4] == 0xFF && cal_buf[end_addr - 5] == 0xFF &&
					cal_buf[end_addr - 6] == 0xFF && cal_buf[end_addr - 7] == 0xFF &&
					cal_buf[end_addr - 8] == 0xFF && cal_buf[end_addr - 9] == 0xFF &&
					cal_buf[end_addr - 10] == 0xFF && cal_buf[end_addr - 11] == 0xFF &&
					cal_buf[end_addr - 12] == 0xFF && cal_buf[end_addr - 13] == 0xFF &&
					cal_buf[end_addr - 14] == 0xFF && cal_buf[end_addr - 15] == 0xFF) {
					info("empty Cal - cal offset[0x%04X] = val[0x%02X]",
						end_addr, cal_buf[end_addr]);
					info("cal offset[0x%04X] = val[0x%02X]",
						end_addr - 15, cal_buf[end_addr - 15]);
					info("[%s] empty Cal", __func__);
					return 0;
				}
			}
		}
	}
#endif

	WARN_ON(!subdev);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);
	WARN_ON(!cis);

	if (finfo->rom_pdxtc_cal_data_addr_list_len <= 0
		|| finfo->rom_gcc_cal_data_addr_list_len <= 0
		|| finfo->rom_xtc_cal_data_addr_list_len <= 0) {
		err("Not available DT, skip set cal");
		sensor_gn3_eeprom_cal_available = false;
		return 0;
	}

	ret |= cis->ixc_ops->write16(cis->client, 0xFCFC, 0x4000);
	ret |= cis->ixc_ops->write16(cis->client, 0x6000, 0x0085);

	info("[%s] PDXTC start\n", __func__);
	start_addr = finfo->rom_pdxtc_cal_data_addr_list[GN3_CAL_START_ADDR];
	if (finfo->rom_pdxtc_cal_endian_check) {
		if (cal_buf[start_addr] == 0xFF && cal_buf[start_addr + 1] == 0x00)
			endian = GN3_BIG_ENDIAN;
		else
			endian = GN3_LITTLE_ENDIAN;

		start_addr = start_addr + 2;
	} else {
		endian = GN3_BIG_ENDIAN;
	}

	for (len = 0; len < finfo->rom_pdxtc_cal_data_addr_list_len / GN3_CAL_ROW_LEN; len++) {
		ret |= sensor_cis_set_registers(subdev, sensor_gn3_pre_PDXTC[len], sensor_gn3_pre_PDXTC_size[len]);

		dbg_sensor(1, "[%s] PDXTC Calibration Data E\n", __func__);
		if (len != 0)
			start_addr = finfo->rom_pdxtc_cal_data_addr_list[len * GN3_CAL_ROW_LEN + GN3_CAL_START_ADDR];
		end_addr = finfo->rom_pdxtc_cal_data_addr_list[len * GN3_CAL_ROW_LEN + GN3_CAL_END_ADDR];

#ifdef GN3_BURST_WRITE
		if (finfo->rom_pdxtc_cal_data_addr_list[len * GN3_CAL_ROW_LEN + GN3_CAL_BURST_CHECK]) {
			cal_size = (end_addr - start_addr) / 2 + 1;
			dbg_sensor(1, "[%s] rom_pdxtc_cal burst write start(0x%X) end(0x%X)\n",
				__func__, start_addr, end_addr);
			ret = sensor_gn3_cis_write16_burst(cis->client, 0x6F12,
				(u8 *)&cal_buf[start_addr], cal_size, endian);
			if (ret < 0) {
				err("sensor_gn3_cis_write16_burst fail!!");
				goto p_err;
			}
		} else
#endif
		{
			for (i = start_addr; i <= end_addr; i += 2) {
				val = GN3_ENDIAN(cal_buf[i], cal_buf[i + 1], endian);
				ret = cis->ixc_ops->write16(cis->client, 0x6F12, val);
				if (ret < 0) {
					err("is_sensor_write16 fail!!");
					goto p_err;
				}
#ifdef DEBUG_CAL_WRITE
				info("cal offset[0x%04X] , val[0x%04X]", i, val);
#endif
			}
		}

		dbg_sensor(1, "[%s] PDXTC Calibration Data X\n", __func__);

		ret |= sensor_cis_set_registers(subdev, sensor_gn3_post_PDXTC[len], sensor_gn3_post_PDXTC_size[len]);
	}

	info("[%s] PDXTC end\n", __func__);

	info("[%s] GCC start\n", __func__);
	start_addr = finfo->rom_gcc_cal_data_addr_list[GN3_CAL_START_ADDR];
	if (finfo->rom_gcc_cal_endian_check) {
		if (cal_buf[start_addr] == 0xFF && cal_buf[start_addr + 1] == 0x00)
			endian = GN3_BIG_ENDIAN;
		else
			endian = GN3_LITTLE_ENDIAN;

		start_addr = start_addr + 2;
	} else {
		endian = GN3_BIG_ENDIAN;
	}

	for (len = 0; len < finfo->rom_gcc_cal_data_addr_list_len / GN3_CAL_ROW_LEN; len++) {
		ret |= sensor_cis_set_registers(subdev, sensor_gn3_pre_GCC[len], sensor_gn3_pre_GCC_size[len]);

		dbg_sensor(1, "[%s] GCC Calibration Data E\n", __func__);
		if (len != 0)
			start_addr = finfo->rom_gcc_cal_data_addr_list[len * GN3_CAL_ROW_LEN + GN3_CAL_START_ADDR];
		end_addr = finfo->rom_gcc_cal_data_addr_list[len * GN3_CAL_ROW_LEN + GN3_CAL_END_ADDR];

#ifdef GN3_BURST_WRITE
		if (finfo->rom_gcc_cal_data_addr_list[len * GN3_CAL_ROW_LEN + GN3_CAL_BURST_CHECK]) {
			cal_size = (end_addr - start_addr) / 2 + 1;
			dbg_sensor(1, "[%s] rom_gcc_cal burst write start(0x%X) end(0x%X)\n",
				__func__, start_addr, end_addr);
			ret = sensor_gn3_cis_write16_burst(cis->client, 0x6F12,
				(u8 *)&cal_buf[start_addr], cal_size, endian);
			if (ret < 0) {
				err("sensor_gn3_cis_write16_burst fail!!");
				goto p_err;
			}
		} else
#endif
		{
			for (i = start_addr; i <= end_addr; i += 2) {
				val = GN3_ENDIAN(cal_buf[i], cal_buf[i + 1], endian);
				ret = cis->ixc_ops->write16(cis->client, 0x6F12, val);
				if (ret < 0) {
					err("is_sensor_write16 fail!!");
					goto p_err;
				}
#ifdef DEBUG_CAL_WRITE
				info("cal offset[0x%04X] , val[0x%04X]", i, val);
#endif
			}
		}

		dbg_sensor(1, "[%s] GCC Calibration Data X\n", __func__);

		ret |= sensor_cis_set_registers(subdev, sensor_gn3_post_GCC[len], sensor_gn3_post_GCC_size[len]);
	}
	info("[%s] GCC end\n", __func__);

	info("[%s] XTC start\n", __func__);
	start_addr = finfo->rom_xtc_cal_data_addr_list[GN3_CAL_START_ADDR];
	if (finfo->rom_xtc_cal_endian_check) {
		if (cal_buf[start_addr] == 0xFF && cal_buf[start_addr + 1] == 0x00)
			endian = GN3_BIG_ENDIAN;
		else
			endian = GN3_LITTLE_ENDIAN;

		start_addr = start_addr + 2;
	} else {
		endian = GN3_BIG_ENDIAN;
	}

	for (len = 0; len < finfo->rom_xtc_cal_data_addr_list_len / GN3_CAL_ROW_LEN; len++) {
		ret |= sensor_cis_set_registers(subdev, sensor_gn3_pre_XTC[len], sensor_gn3_pre_XTC_size[len]);

		dbg_sensor(1, "[%s] XTC Calibration Data E\n", __func__);
		if (len != 0)
			start_addr = finfo->rom_xtc_cal_data_addr_list[len * GN3_CAL_ROW_LEN + GN3_CAL_START_ADDR];
		end_addr = finfo->rom_xtc_cal_data_addr_list[len * GN3_CAL_ROW_LEN + GN3_CAL_END_ADDR];

#ifdef GN3_BURST_WRITE
		if (finfo->rom_xtc_cal_data_addr_list[len * GN3_CAL_ROW_LEN + GN3_CAL_BURST_CHECK]) {
			cal_size = (end_addr - start_addr) / 2 + 1;
			dbg_sensor(1, "[%s] rom_xtc_cal burst write start(0x%X) end(0x%X) size(%d)\n",
				__func__, start_addr, end_addr, cal_size);
			ret = sensor_gn3_cis_write16_burst(cis->client, 0x6F12,
				(u8 *)&cal_buf[start_addr], cal_size, endian);
			if (ret < 0) {
				err("sensor_gn3_cis_write16_burst fail!!");
				goto p_err;
			}
		} else
#endif
		{
			for (i = start_addr; i <= end_addr; i += 2) {
				val = GN3_ENDIAN(cal_buf[i], cal_buf[i + 1], endian);
				ret = cis->ixc_ops->write16(cis->client, 0x6F12, val);
				if (ret < 0) {
					err("is_sensor_write16 fail!!");
					goto p_err;
				}
#ifdef DEBUG_CAL_WRITE
				info("cal offset[0x%04X] , val[0x%04X]", i, val);
#endif
			}
		}

		/* For handle last 8bit */
		if (len + 1 == finfo->rom_xtc_cal_data_addr_list_len / GN3_CAL_ROW_LEN) {
#define GN3_XTC_LAST_8BIT_ADDR 0x59E0

			ret = cis->ixc_ops->write16(cis->client, 0x602A, 0x6E3A);
			ret |= cis->ixc_ops->write8(cis->client, 0x6F12, cal_buf[GN3_XTC_LAST_8BIT_ADDR]);

			info("last 8 bit addr[0x%04X] , val[0x%04X]",
				GN3_XTC_LAST_8BIT_ADDR, cal_buf[GN3_XTC_LAST_8BIT_ADDR]);

			if (ret < 0) {
				err("is_sensor_write fail!!");
				goto p_err;
			}
		}

		dbg_sensor(1, "[%s] XTC Calibration Data X\n", __func__);

		ret |= sensor_cis_set_registers(subdev, sensor_gn3_post_XTC[len], sensor_gn3_post_XTC_size[len]);
	}
	info("[%s] XTC end\n", __func__);

	sensor_gn3_eeprom_cal_available = true;

	info("[%s] X", __func__);

p_err:
	return ret;
}
#endif

#if IS_ENABLED(USE_CAMERA_SENSOR_RETENTION)
int sensor_gn3_cis_update_seamless_mode(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_cis *cis = NULL;
	unsigned int mode = 0;
	struct is_module_enum *module;
	struct is_device_sensor_peri *sensor_peri = NULL;
	struct sensor_open_extended *ext_info;
	unsigned int next_mode = sensor_gn3_mode_groups[SENSOR_GN3_MODE_DEFAULT];
	u16 fast_change_idx = SENSOR_GN3_FAST_CHANGE_IDX_NONE;
	u32 dummy_value = 0;

	WARN_ON(!subdev);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);
	WARN_ON(!cis);
	WARN_ON(!cis->cis_data);

	mode = cis->cis_data->sens_config_index_cur;
	sensor_peri = container_of(cis, struct is_device_sensor_peri, cis);
	module = sensor_peri->module;
	ext_info = &module->ext;
	if (ext_info->use_retention_mode != SENSOR_RETENTION_ACTIVATED) {
		err("retention is not activated");
		ret = -1;
		return ret;
	}

	/* LN check */
	if (sensor_gn3_cis_get_lownoise_supported(subdev)) {
		switch (cis->cis_data->cur_lownoise_mode) {
		case IS_CIS_LN2:
		case IS_CIS_LN4:
			next_mode = sensor_gn3_mode_groups[SENSOR_GN3_MODE_LN];
			break;
		}
	}

	/* IDCG check */
	if (sensor_gn3_mode_groups[SENSOR_GN3_MODE_IDCG] != MODE_GROUP_NONE) {
		switch (cis->cis_data->cur_12bit_mode) {
		case SENSOR_12BIT_STATE_REAL_12BIT:
			next_mode = sensor_gn3_mode_groups[SENSOR_GN3_MODE_IDCG];
		case SENSOR_12BIT_STATE_PSEUDO_12BIT:
			break;
		}
	}

	if (next_mode != MODE_GROUP_NONE && mode != next_mode) {
		ret = sensor_gn3_cis_group_param_hold_func(subdev, 0x01);
		if (ret < 0)
			err("group_param_hold_func failed");

		fast_change_idx = sensor_gn3_spec_mode_retention_attr[next_mode].fast_change_idx;
		ret |= cis->ixc_ops->write16(cis->client, 0xFCFC, 0x4000);
		ret |= cis->ixc_ops->write16(cis->client, 0x0B30, fast_change_idx);
		if (ret < 0)
			err("sensor_gn3_set_registers fail!!");

		sensor_gn3_cis_data_calculation(sensor_gn3_pllinfos[next_mode], cis->cis_data);

		ret = sensor_gn3_cis_group_param_hold_func(subdev, 0x00);
		if (ret < 0)
			err("group_param_hold_func failed");

		cis->cis_data->sens_config_index_pre = cis->cis_data->sens_config_index_cur;
		cis->cis_data->sens_config_index_cur = next_mode;
		info("[%s]pre mode(%d) -> cur mode(%d), 12bit mode(%d), LN mode(%d), fast_change_idx(0x%x)\n", __func__,
			cis->cis_data->sens_config_index_pre, cis->cis_data->sens_config_index_cur,
			cis->cis_data->cur_12bit_mode,
			cis->cis_data->cur_lownoise_mode,
			fast_change_idx);

		CALL_CISOPS(cis, cis_get_min_analog_gain, subdev, &dummy_value);
		dbg_sensor(1, "[%s] min again : %d\n", __func__, dummy_value);
		CALL_CISOPS(cis, cis_get_max_analog_gain, subdev, &dummy_value);
		dbg_sensor(1, "[%s] max again : %d\n", __func__, dummy_value);
		CALL_CISOPS(cis, cis_get_min_digital_gain, subdev, &dummy_value);
		dbg_sensor(1, "[%s] min dgain : %d\n", __func__, dummy_value);
		CALL_CISOPS(cis, cis_get_max_digital_gain, subdev, &dummy_value);
		dbg_sensor(1, "[%s] max dgain : %d\n", __func__, dummy_value);
	}

	return ret;
}
#endif

int sensor_gn3_cis_get_max_analog_gain(struct v4l2_subdev *subdev, u32 *max_again)
{
	int ret = 0;
	struct is_cis *cis;
	cis_shared_data *cis_data;
	ktime_t st = ktime_get();
	u32 mode;

	WARN_ON(!subdev);
	WARN_ON(!max_again);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);

	WARN_ON(!cis);
	WARN_ON(!cis->cis_data);

	cis_data = cis->cis_data;
	mode = cis_data->sens_config_index_cur;

#if IS_ENABLED(USE_CAMERA_SENSOR_RETENTION)
	if (cis_data->cur_12bit_mode == SENSOR_12BIT_STATE_REAL_12BIT
		&& mode != sensor_gn3_mode_groups[SENSOR_GN3_MODE_IDCG]) {
		cis_data->max_analog_gain[0] =
			sensor_gn3_common_mode_attr[sensor_gn3_mode_groups[SENSOR_GN3_MODE_IDCG]].max_analog_gain;
	} else if (cis_data->cur_12bit_mode == SENSOR_12BIT_STATE_PSEUDO_12BIT
		&& mode == sensor_gn3_mode_groups[SENSOR_GN3_MODE_IDCG]) {
		if (cis_data->cur_lownoise_mode != IS_CIS_LNOFF) {
			cis_data->max_analog_gain[0] =
			sensor_gn3_common_mode_attr[sensor_gn3_mode_groups[SENSOR_GN3_MODE_LN]].max_analog_gain;
		} else {
			cis_data->max_analog_gain[0] =
			sensor_gn3_common_mode_attr[sensor_gn3_mode_groups[SENSOR_GN3_MODE_DEFAULT]].max_analog_gain;
		}
	} else
#endif
	{
		cis_data->max_analog_gain[0] = sensor_gn3_common_mode_attr[mode].max_analog_gain;
	}
	cis_data->max_analog_gain[1] = sensor_cis_calc_again_permile(cis_data->max_analog_gain[0]);

	*max_again = cis_data->max_analog_gain[1];

	dbg_sensor(1, "[%s] code %d, permile %d\n", __func__, cis_data->max_analog_gain[0],
		cis_data->max_analog_gain[1]);

	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %ldus", __func__, PABLO_KTIME_US_DELTA_NOW(st));

	return ret;
}

int sensor_gn3_cis_mode_change(struct v4l2_subdev *subdev, u32 mode)
{
	int ret = 0;
	struct is_cis *cis = NULL;
	struct is_module_enum *module;
	struct is_device_sensor_peri *sensor_peri = NULL;
	struct sensor_open_extended *ext_info = NULL;
	struct is_device_sensor *device;
	u32 ex_mode;
	u32 dummy_max_again;
	u16 fast_change_idx = 0x00FF;
#if IS_ENABLED(USE_CAMERA_SENSOR_RETENTION)
	int load_sram_idx = -1;
	int load_sram_idx_ln = -1;
#endif

	WARN_ON(!subdev);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);
	WARN_ON(!cis);
	WARN_ON(!cis->cis_data);

	device = (struct is_device_sensor *)v4l2_get_subdev_hostdata(subdev);
	if (unlikely(!device)) {
		err("device sensor is null");
		return -EINVAL;
	}

	if (mode >= sensor_gn3_max_setfile_num) {
		err("invalid mode(%d)!!", mode);
		ret = -EINVAL;
		goto p_err;
	}

	sensor_peri = container_of(cis, struct is_device_sensor_peri, cis);
	module = sensor_peri->module;
	ext_info = &module->ext;
	WARN_ON(!ext_info);

	info("[%s] E\n", __func__);

	sensor_gn3_cis_get_max_analog_gain(subdev, &dummy_max_again);

	sensor_gn3_set_integration_min(mode, cis->cis_data);
	sensor_gn3_set_integration_max_margin(mode, cis->cis_data);

	cis->mipi_clock_index_cur = CAM_MIPI_NOT_INITIALIZED;

	I2C_MUTEX_LOCK(cis->i2c_lock);

	ex_mode = is_sensor_g_ex_mode(device);

	info("[%s] sensor mode(%d) ex_mode(%d)\n", __func__, mode, ex_mode);

#if IS_ENABLED(USE_CAMERA_SENSOR_RETENTION)
	/* Retention mode sensor mode select */
	if (ext_info->use_retention_mode == SENSOR_RETENTION_ACTIVATED) {
		sensor_gn3_load_retention = false;

		sensor_gn3_cis_set_mode_group(mode);

		fast_change_idx = sensor_gn3_spec_mode_retention_attr[mode].fast_change_idx;

		if (fast_change_idx != SENSOR_GN3_FAST_CHANGE_IDX_NONE) {
			load_sram_idx = sensor_gn3_spec_mode_retention_attr[mode].load_sram_idx;

			if (sensor_gn3_mode_groups[SENSOR_GN3_MODE_LN] != MODE_GROUP_NONE) {
				load_sram_idx_ln
			= sensor_gn3_spec_mode_retention_attr[sensor_gn3_mode_groups[SENSOR_GN3_MODE_LN]].load_sram_idx;
			} else {
				load_sram_idx_ln = SENSOR_GN3_LOAD_SRAM_IDX_NONE;
			}

			if (load_sram_idx != SENSOR_GN3_LOAD_SRAM_IDX_NONE) {
				ret = sensor_cis_set_registers(subdev,
					sensor_gn3_load_sram[load_sram_idx],
					sensor_gn3_load_sram_size[load_sram_idx]);
				if (ret < 0) {
					err("sensor_gn3_set_registers fail!!");
					goto p_err_i2c_unlock;
				}
			}

			if (load_sram_idx_ln != SENSOR_GN3_LOAD_SRAM_IDX_NONE) {
				ret = sensor_cis_set_registers(subdev,
					sensor_gn3_load_sram[load_sram_idx_ln],
					sensor_gn3_load_sram_size[load_sram_idx_ln]);
				if (ret < 0) {
					err("sensor_gn3_set_registers fail!!");
					goto p_err_i2c_unlock;
				}
			}

			ret |= cis->ixc_ops->write16(cis->client, 0xFCFC, 0x4000);
			ret |= cis->ixc_ops->write16(cis->client, 0x0B30, fast_change_idx);
			if (ret < 0) {
				err("sensor_gn3_set_registers fail!!");
				goto p_err_i2c_unlock;
			}
		} else {
			info("[%s] not support retention sensor mode(%d)\n", __func__, mode);

			/* Fast Change disable */
			ret |= cis->ixc_ops->write16(cis->client, 0xFCFC, 0x4000);
			ret |= cis->ixc_ops->write16(cis->client, 0x0B30, 0x00FF);

			ret |= sensor_cis_set_registers(subdev, sensor_gn3_setfiles[mode],
								sensor_gn3_setfile_sizes[mode]);

			if (ret < 0) {
				err("sensor_gn3_set_registers fail!!");
				goto p_err_i2c_unlock;
			}
		}
	} else
#endif
	{
		info("[%s] not support retention sensor mode(%d)\n", __func__, mode);

		/* Fast Change disable */
		ret |= cis->ixc_ops->write16(cis->client, 0xFCFC, 0x4000);
		ret |= cis->ixc_ops->write16(cis->client, 0x0B30, 0x00FF);

		ret |= sensor_cis_set_registers(subdev, sensor_gn3_setfiles[mode],
								sensor_gn3_setfile_sizes[mode]);

		if (ret < 0) {
			err("sensor_gn3_set_registers fail!!");
			goto p_err_i2c_unlock;
		}
	}

	info("[%s] sensor mode done(%d), fast_change_idx(0x%x)\n", __func__, mode, fast_change_idx);

	cis->cis_data->sens_config_index_pre = mode;

	info("[%s] dual sync always master\n", __func__);
	/* ret = sensor_cis_set_registers(subdev, sensor_gn3_cis_dual_master_settings, */
	/*	sensor_gn3_cis_dual_master_settings_size); */

	cis->cis_data->highres_capture_mode = sensor_gn3_spec_mode_attr[mode].highres_capture_mode;

	cis->cis_data->pre_12bit_mode = sensor_gn3_spec_mode_attr[mode].status_12bit;
	cis->cis_data->cur_12bit_mode = sensor_gn3_spec_mode_attr[mode].status_12bit;

	cis->cis_data->pre_lownoise_mode = IS_CIS_LNOFF;
	cis->cis_data->cur_lownoise_mode = IS_CIS_LNOFF;

#if IS_ENABLED(USE_CAMERA_SENSOR_RETENTION)
	sensor_gn3_cis_update_seamless_mode(subdev);
#endif

p_err_i2c_unlock:
	I2C_MUTEX_UNLOCK(cis->i2c_lock);

p_err:
	/* sensor_gn3_cis_log_status(subdev); */
	info("[%s] X\n", __func__);

	return ret;
}

int sensor_gn3_cis_set_global_setting(struct v4l2_subdev *subdev)
{
	int ret = 0;

#if IS_ENABLED(USE_CAMERA_SENSOR_RETENTION)
	struct is_cis *cis = NULL;
	struct is_module_enum *module;
	struct is_device_sensor_peri *sensor_peri = NULL;
	struct sensor_open_extended *ext_info;

	WARN_ON(!subdev);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);
	WARN_ON(!cis);

	sensor_peri = container_of(cis, struct is_device_sensor_peri, cis);
	module = sensor_peri->module;
	ext_info = &module->ext;
	WARN_ON(!ext_info);

	/* setfile global setting is at camera entrance */
	if (ext_info->use_retention_mode == SENSOR_RETENTION_INACTIVE) {
		sensor_gn3_cis_set_global_setting_internal(subdev);
		sensor_gn3_cis_retention_prepare(subdev);
	} else if (ext_info->use_retention_mode == SENSOR_RETENTION_ACTIVATED) {
		sensor_gn3_cis_retention_crc_check(subdev);
	} else { /* SENSOR_RETENTION_UNSUPPORTED */
		sensor_gn3_eeprom_cal_available = false;
		sensor_gn3_cis_set_global_setting_internal(subdev);
	}
#else
	WARN_ON(!subdev);

	info("[%s] global setting start\n", __func__);
	/* setfile global setting is at camera entrance */
	ret = sensor_gn3_cis_set_global_setting_internal(subdev);
	if (ret < 0) {
		err("sensor_gn3_set_registers fail!!");
		goto p_err;
	}
	info("[%s] global setting done\n", __func__);
#endif

#if IS_ENABLED(USE_CAMERA_SENSOR_RETENTION)
#else
p_err:
#endif
	return ret;
}


#if IS_ENABLED(USE_CAMERA_SENSOR_RETENTION)
int sensor_gn3_cis_retention_prepare(struct v4l2_subdev *subdev)
{
	int ret = 0;
	int i = 0;
	struct is_cis *cis = NULL;
	struct is_module_enum *module;
	struct is_device_sensor_peri *sensor_peri = NULL;
	struct sensor_open_extended *ext_info;

	WARN_ON(!subdev);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);
	WARN_ON(!cis);

	sensor_peri = container_of(cis, struct is_device_sensor_peri, cis);
	module = sensor_peri->module;
	ext_info = &module->ext;
	WARN_ON(!ext_info);

	I2C_MUTEX_LOCK(cis->i2c_lock);
	for (i = 0; i < sensor_gn3_max_retention_num; i++) {
		ret = sensor_cis_set_registers(subdev, sensor_gn3_retention[i], sensor_gn3_retention_size[i]);
		if (ret < 0) {
			err("sensor_gn3_set_registers fail!!");
			goto p_err;
		}
	}

#if IS_ENABLED(CIS_CALIBRATION)
	ret = sensor_gn3_cis_set_cal(subdev);
	if (ret < 0) {
		err("sensor_gn3_cis_set_cal fail!!");
		goto p_err;
	}
#endif

	/* FAST AE Setting */
	ret = sensor_cis_set_registers(subdev, sensor_gn3_setfiles[SENSOR_GN3_992X744_120FPS],
						sensor_gn3_setfile_sizes[SENSOR_GN3_992X744_120FPS]);
	if (ret < 0) {
		err("sensor_gn3_set_registers fail!!");
		goto p_err;
	}

	ret |= cis->ixc_ops->write16(cis->client, 0xFCFC, 0x2000);
	ret |= cis->ixc_ops->write16(cis->client, 0x16DA, 0x0001);
	ret |= cis->ixc_ops->write16(cis->client, 0x16DE, 0x2002);
	ret |= cis->ixc_ops->write16(cis->client, 0x16E0, 0x8400);
	ret |= cis->ixc_ops->write16(cis->client, 0x16E2, 0x8B00);
	ret |= cis->ixc_ops->write16(cis->client, 0x16E4, 0x9200);
	ret |= cis->ixc_ops->write16(cis->client, 0x16E6, 0x9900);
	ret |= cis->ixc_ops->write16(cis->client, 0x16E8, 0xA000);
	ret |= cis->ixc_ops->write16(cis->client, 0x16EA, 0xA700);
	ret |= cis->ixc_ops->write16(cis->client, 0x16EC, 0xAE00);
	ret |= cis->ixc_ops->write16(cis->client, 0x16EE, 0xB500);
	ret |= cis->ixc_ops->write16(cis->client, 0x16F0, 0xBC00);
	ret |= cis->ixc_ops->write16(cis->client, 0xFCFC, 0x4000);
	ret |= cis->ixc_ops->write16(cis->client, 0x0B30, 0x01FF);
	ret |= cis->ixc_ops->write16(cis->client, 0x010E, 0x0100);
	ret |= cis->ixc_ops->write16(cis->client, 0x6000, 0x0085);

	I2C_MUTEX_UNLOCK(cis->i2c_lock);

	if (sensor_gn3_need_stream_on_retention) {
		I2C_MUTEX_LOCK(cis->i2c_lock);
		ret |= cis->ixc_ops->write16(cis->client, 0x0100, 0x0103); /* stream on */
		I2C_MUTEX_UNLOCK(cis->i2c_lock);

		sensor_gn3_cis_wait_streamon(subdev);
		msleep(100); /* decompression cal data */

		I2C_MUTEX_LOCK(cis->i2c_lock);
		ret |= cis->ixc_ops->write8(cis->client, 0x0100, 0x00); /* stream off */
		I2C_MUTEX_UNLOCK(cis->i2c_lock);

		sensor_cis_wait_streamoff(subdev);
		sensor_gn3_need_stream_on_retention = false;
	}

	if (ret < 0) {
		err("is_sensor_write fail!!");
		goto p_err;
	}

	usleep_range(10000, 10000);

	ext_info->use_retention_mode = SENSOR_RETENTION_ACTIVATED;

	info("[%s] retention sensor RAM write done\n", __func__);

p_err:

	return ret;
}

int sensor_gn3_cis_retention_crc_check(struct v4l2_subdev *subdev)
{
	int ret = 0;
	u16 crc_check = 0;
	struct is_cis *cis = NULL;

	WARN_ON(!subdev);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);
	WARN_ON(!cis);
	WARN_ON(!cis->cis_data);

	I2C_MUTEX_LOCK(cis->i2c_lock);

	/* retention mode CRC check */
	cis->ixc_ops->read16(cis->client, 0x19C2, &crc_check); /* api_ro_checksum_on_ram_passed */

	I2C_MUTEX_UNLOCK(cis->i2c_lock);

	if (crc_check == 0x0100) {
		info("[%s] retention SRAM CRC check: pass!\n", __func__);
	} else {
		info("[%s] retention SRAM CRC check: fail!\n", __func__);
		info("retention CRC Check register value: 0x%x\n", crc_check);
		info("[%s] rewrite retention modes to SRAM\n", __func__);

		ret = sensor_gn3_cis_set_global_setting_internal(subdev);
		if (ret < 0) {
			err("CRC error recover: rewrite sensor global setting failed");
			goto p_err;
		}

		sensor_gn3_eeprom_cal_available = false;

		ret = sensor_gn3_cis_retention_prepare(subdev);
		if (ret < 0) {
			err("CRC error recover: retention prepare failed");
			goto p_err;
		}
	}

p_err:

	return ret;
}
#endif

/* TODO: Sensor set size sequence(sensor done, sensor stop, 3AA done in FW case */
int sensor_gn3_cis_set_size(struct v4l2_subdev *subdev, cis_shared_data *cis_data)
{
	int ret = 0;
	bool binning = false;
	u32 ratio_w = 0, ratio_h = 0, start_x = 0, start_y = 0, end_x = 0, end_y = 0;
	u32 even_x = 0, odd_x = 0, even_y = 0, odd_y = 0;
	struct i2c_client *client = NULL;
	struct is_cis *cis = NULL;
	ktime_t st = ktime_get();

	WARN_ON(!subdev);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);
	WARN_ON(!cis);

	dbg_sensor(1, "[MOD:D:%d] %s\n", cis->id, __func__);

	if (unlikely(!cis_data)) {
		err("cis data is NULL");
		if (unlikely(!cis->cis_data)) {
			ret = -EINVAL;
			goto p_err;
		} else {
			cis_data = cis->cis_data;
		}
	}

	client = cis->client;
	if (unlikely(!client)) {
		err("client is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	/* Wait actual stream off */
	ret = sensor_gn3_wait_stream_off_status(cis_data);
	if (ret) {
		err("Must stream off\n");
		ret = -EINVAL;
		goto p_err;
	}

	binning = cis_data->binning;
	if (binning) {
		ratio_w = (SENSOR_GN3_MAX_WIDTH / cis_data->cur_width);
		ratio_h = (SENSOR_GN3_MAX_HEIGHT / cis_data->cur_height);
	} else {
		ratio_w = 1;
		ratio_h = 1;
	}

	if (((cis_data->cur_width * ratio_w) > SENSOR_GN3_MAX_WIDTH) ||
		((cis_data->cur_height * ratio_h) > SENSOR_GN3_MAX_HEIGHT)) {
		err("Config max sensor size over~!!\n");
		ret = -EINVAL;
		goto p_err;
	}

	I2C_MUTEX_LOCK(cis->i2c_lock);
	/* 1. page_select */
	ret = cis->ixc_ops->write16(client, 0xFCFC, 0x4000);
	if (ret < 0)
		goto p_err_i2c_unlock;

	/* 2. pixel address region setting */
	start_x = ((SENSOR_GN3_MAX_WIDTH - cis_data->cur_width * ratio_w) / 2) & (~0x1);
	start_y = ((SENSOR_GN3_MAX_HEIGHT - cis_data->cur_height * ratio_h) / 2) & (~0x1);
	end_x = start_x + (cis_data->cur_width * ratio_w - 1);
	end_y = start_y + (cis_data->cur_height * ratio_h - 1);

	if (!(end_x & (0x1)) || !(end_y & (0x1))) {
		err("Sensor pixel end address must odd\n");
		ret = -EINVAL;
		goto p_err_i2c_unlock;
	}

	ret = cis->ixc_ops->write16(client, 0x0344, start_x);
	if (ret < 0)
		goto p_err_i2c_unlock;
	ret = cis->ixc_ops->write16(client, 0x0346, start_y);
	if (ret < 0)
		goto p_err_i2c_unlock;
	ret = cis->ixc_ops->write16(client, 0x0348, end_x);
	if (ret < 0)
		goto p_err_i2c_unlock;
	ret = cis->ixc_ops->write16(client, 0x034A, end_y);
	if (ret < 0)
		goto p_err_i2c_unlock;

	/* 3. output address setting */
	ret = cis->ixc_ops->write16(client, 0x034C, cis_data->cur_width);
	if (ret < 0)
		goto p_err_i2c_unlock;
	ret = cis->ixc_ops->write16(client, 0x034E, cis_data->cur_height);
	if (ret < 0)
		goto p_err_i2c_unlock;

	/* If not use to binning, sensor image should set only crop */
	if (!binning) {
		dbg_sensor(1, "Sensor size set is not binning\n");
		goto p_err_i2c_unlock;
	}

	/* 4. sub sampling setting */
	even_x = 1;	/* 1: not use to even sampling */
	even_y = 1;
	odd_x = (ratio_w * 2) - even_x;
	odd_y = (ratio_h * 2) - even_y;

	ret = cis->ixc_ops->write16(client, 0x0380, even_x);
	if (ret < 0)
		goto p_err_i2c_unlock;
	ret = cis->ixc_ops->write16(client, 0x0382, odd_x);
	if (ret < 0)
		goto p_err_i2c_unlock;
	ret = cis->ixc_ops->write16(client, 0x0384, even_y);
	if (ret < 0)
		goto p_err_i2c_unlock;
	ret = cis->ixc_ops->write16(client, 0x0386, odd_y);
	if (ret < 0)
		goto p_err_i2c_unlock;

	/* 6. scaling setting: but not use */
	/* scaling_mode (0: No scaling, 1: Horizontal, 2: Full, 4:Separate vertical) */
	ret = cis->ixc_ops->write16(client, 0x0402, 0x0000);
	if (ret < 0)
		goto p_err_i2c_unlock;
	/* down_scale_m: 1 to 16 upwards (scale_n: 16(fixed))
	 * down scale factor = down_scale_m / down_scale_n
	 */
	ret = cis->ixc_ops->write16(client, 0x0404, 0x10);
	if (ret < 0)
		goto p_err_i2c_unlock;

	cis_data->frame_time = (cis_data->line_readOut_time * cis_data->cur_height / 1000);
	cis->cis_data->rolling_shutter_skew =
		(cis->cis_data->cur_height - 1) * cis->cis_data->line_readOut_time;
	dbg_sensor(1, "[%s] frame_time(%d), rolling_shutter_skew(%lld)\n",
		__func__, cis->cis_data->frame_time, cis->cis_data->rolling_shutter_skew);

	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %ldus", __func__, PABLO_KTIME_US_DELTA_NOW(st));

p_err_i2c_unlock:
	I2C_MUTEX_UNLOCK(cis->i2c_lock);

p_err:
	return ret;
}

int sensor_gn3_cis_stream_on(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_cis *cis;
	struct i2c_client *client;
	cis_shared_data *cis_data;
	struct is_device_sensor_peri *sensor_peri = NULL;
	struct is_device_sensor *device;
	u16 fast_change_idx = 0x00FF;
	ktime_t st = ktime_get();

	WARN_ON(!subdev);

	device = (struct is_device_sensor *)v4l2_get_subdev_hostdata(subdev);
	WARN_ON(!device);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);

	WARN_ON(!cis);
	WARN_ON(!cis->cis_data);

	sensor_peri = container_of(cis, struct is_device_sensor_peri, cis);
	WARN_ON(!sensor_peri);

	client = cis->client;
	if (unlikely(!client)) {
		err("client is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	cis_data = cis->cis_data;

	dbg_sensor(1, "[MOD:D:%d] %s\n", cis->id, __func__);

	is_vendor_set_mipi_clock(device);

	I2C_MUTEX_LOCK(cis->i2c_lock);

#ifdef DEBUG_GN3_PLL
	{
		u16 pll;

		cis->ixc_ops->read16(client, 0x0300, &pll);
		dbg_sensor(1, "______ vt_pix_clk_div(0x%x)\n", pll);
		cis->ixc_ops->read16(client, 0x0302, &pll);
		dbg_sensor(1, "______ vt_sys_clk_div(0x%x)\n", pll);
		cis->ixc_ops->read16(client, 0x0304, &pll);
		dbg_sensor(1, "______ vt_pre_pll_clk_div(0x%x)\n", pll);
		cis->ixc_ops->read16(client, 0x0306, &pll);
		dbg_sensor(1, "______ vt_pll_multiplier(0x%x)\n", pll);
		cis->ixc_ops->read16(client, 0x0308, &pll);
		dbg_sensor(1, "______ op_pix_clk_div(0x%x)\n", pll);
		cis->ixc_ops->read16(client, 0x030a, &pll);
		dbg_sensor(1, "______ op_sys_clk_div(0x%x)\n", pll);

		cis->ixc_ops->read16(client, 0x030c, &pll);
		dbg_sensor(1, "______ vt_pll_post_scaler(0x%x)\n", pll);
		cis->ixc_ops->read16(client, 0x030e, &pll);
		dbg_sensor(1, "______ op_pre_pll_clk_dv(0x%x)\n", pll);
		cis->ixc_ops->read16(client, 0x0310, &pll);
		dbg_sensor(1, "______ op_pll_multiplier(0x%x)\n", pll);
		cis->ixc_ops->read16(client, 0x0312, &pll);
		dbg_sensor(1, "______ op_pll_post_scalar(0x%x)\n", pll);

		cis->ixc_ops->read16(client, 0x0340, &pll);
		dbg_sensor(1, "______ frame_length_lines(0x%x)\n", pll);
		cis->ixc_ops->read16(client, 0x0342, &pll);
		dbg_sensor(1, "______ line_length_pck(0x%x)\n", pll);
	}
#endif

	/*
	 * If a companion is used,
	 * then 8 ms waiting is needed before the StreamOn of a sensor (S5KGN3).
	 */
	if (test_bit(IS_SENSOR_PREPROCESSOR_AVAILABLE, &sensor_peri->peri_state))
		mdelay(8);

	/* EMB Header off */
	ret = cis->ixc_ops->write8(client, 0x0118, 0x00);
	if (ret < 0)
		err("EMB header off fail");

	/* Page lock */
	cis->ixc_ops->write16(client, 0xFCFC, 0x4000);
	cis->ixc_ops->write16(client, 0x6000, 0x0085);

#ifndef CONFIG_SEC_FACTORY
	if (!sensor_gn3_eeprom_cal_available) {
		cis->ixc_ops->write8(client, 0x0D0B, 0x00); /* PDXTC */
		cis->ixc_ops->write8(client, 0x0D0A, 0x00); /* GGC */
		cis->ixc_ops->write8(client, 0x0B00, 0x00); /* NonaXTC */
	}
#endif

	cis->ixc_ops->read16(client, 0x0B30, &fast_change_idx);

	/* Sensor stream on */
	info("%s - set_cal_available(%d), fast_change_idx(0x%x)\n",
			__func__, sensor_gn3_eeprom_cal_available, fast_change_idx);
	cis->ixc_ops->write16(client, 0x0100, 0x0103);

	I2C_MUTEX_UNLOCK(cis->i2c_lock);

	cis_data->stream_on = true;
	sensor_gn3_load_retention = true;

	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %ldus", __func__, PABLO_KTIME_US_DELTA_NOW(st));

p_err:
	return ret;
}

int sensor_gn3_cis_stream_off(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_cis *cis;
	struct i2c_client *client;
	cis_shared_data *cis_data;
	u8 cur_frame_count = 0;
	u16 fast_change_idx = 0x00FF;
#if IS_ENABLED(USE_CAMERA_SENSOR_RETENTION)
	struct is_module_enum *module;
	struct is_device_sensor_peri *sensor_peri = NULL;
	struct sensor_open_extended *ext_info;
#endif
	ktime_t st = ktime_get();

	WARN_ON(!subdev);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);

	WARN_ON(!cis);
	WARN_ON(!cis->cis_data);

#if IS_ENABLED(USE_CAMERA_SENSOR_RETENTION)
	sensor_peri = container_of(cis, struct is_device_sensor_peri, cis);
	module = sensor_peri->module;
	ext_info = &module->ext;
	WARN_ON(!ext_info);
#endif

	client = cis->client;
	if (unlikely(!client)) {
		err("client is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	cis_data = cis->cis_data;

	dbg_sensor(1, "[MOD:D:%d] %s\n", cis->id, __func__);

	I2C_MUTEX_LOCK(cis->i2c_lock);
	ret = sensor_gn3_cis_group_param_hold_func(subdev, 0x00);
	if (ret < 0)
		err("group_param_hold_func failed at stream off");

	cis->ixc_ops->read8(client, 0x0005, &cur_frame_count);
	cis->ixc_ops->read16(client, 0x0B30, &fast_change_idx);
	info("%s: frame_count(0x%x), fast_change_idx(0x%x)\n", __func__, cur_frame_count, fast_change_idx);

#if IS_ENABLED(USE_CAMERA_SENSOR_RETENTION)
	/* retention mode CRC check register enable */
	cis->ixc_ops->write16(cis->client, 0x010E, 0x0100);
	cis->ixc_ops->write16(cis->client, 0x19C2, 0x0000);
	if (ext_info->use_retention_mode == SENSOR_RETENTION_INACTIVE)
		ext_info->use_retention_mode = SENSOR_RETENTION_ACTIVATED;

	info("[MOD:D:%d] %s : retention enable CRC check\n", cis->id, __func__);
#endif

	cis->ixc_ops->write8(client, 0x0100, 0x00);

	I2C_MUTEX_UNLOCK(cis->i2c_lock);

	cis_data->stream_on = false;

	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %ldus", __func__, PABLO_KTIME_US_DELTA_NOW(st));

p_err:
	return ret;
}

int sensor_gn3_cis_set_exposure_time(struct v4l2_subdev *subdev, struct ae_param *target_exposure)
{
	int ret = 0;
	int hold = 0;
	struct is_cis *cis;
	struct i2c_client *client;
	cis_shared_data *cis_data;

	u64 vt_pic_clk_freq_khz = 0;
	u16 long_coarse_int = 0;
	u16 short_coarse_int = 0;
	u32 line_length_pck = 0;
	u32 min_fine_int = 0;
	u16 coarse_integration_time_shifter = 0;

	u16 cit_shifter_array[33] = {0, 1, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4,
		5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 6};
	u16 cit_shifter_val = 0;
	int cit_shifter_idx = 0;
	u8 cit_denom_array[7] = {1, 2, 4, 8, 16, 32, 64};
	ktime_t st = ktime_get();

	WARN_ON(!subdev);
	WARN_ON(!target_exposure);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);

	WARN_ON(!cis);
	WARN_ON(!cis->cis_data);

	client = cis->client;
	if (unlikely(!client)) {
		err("client is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	sensor_gn3_target_exp_backup.short_val = target_exposure->short_val;
	sensor_gn3_target_exp_backup.long_val = target_exposure->long_val;

	if ((target_exposure->long_val <= 0) || (target_exposure->short_val <= 0)) {
		err("[%s] invalid target exposure(%d, %d)\n", __func__,
				target_exposure->long_val, target_exposure->short_val);
		ret = -EINVAL;
		goto p_err;
	}

	cis_data = cis->cis_data;

	if (cis->long_term_mode.sen_strm_off_on_enable == false) {
		switch (cis_data->sens_config_index_cur) {
		default:
			if (MAX(target_exposure->long_val, target_exposure->short_val) > 160000) {
				cit_shifter_idx = MIN(MAX(MAX(target_exposure->long_val,
					target_exposure->short_val) / 160000, 0), 32);
				cit_shifter_val = MAX(cit_shifter_array[cit_shifter_idx],
					cis_data->frame_length_lines_shifter);
			} else {
				cit_shifter_val = (u16)(cis_data->frame_length_lines_shifter);
			}
			target_exposure->long_val = target_exposure->long_val / cit_denom_array[cit_shifter_val];
			target_exposure->short_val = target_exposure->short_val / cit_denom_array[cit_shifter_val];
			coarse_integration_time_shifter = ((cit_shifter_val<<8) & 0xFF00) + (cit_shifter_val & 0x00FF);
			break;
		}
	}

	dbg_sensor(1, "[MOD:D:%d] %s, vsync_cnt(%d), target long(%d), short(%d)\n", cis->id, __func__,
			cis_data->sen_vsync_count, target_exposure->long_val, target_exposure->short_val);

	vt_pic_clk_freq_khz = cis_data->pclk / (1000);
	line_length_pck = cis_data->line_length_pck;
	min_fine_int = cis_data->min_fine_integration_time;

	long_coarse_int = ((target_exposure->long_val * vt_pic_clk_freq_khz) / 1000 - min_fine_int)
											/ line_length_pck;
	if (long_coarse_int < cis_data->min_coarse_integration_time) {
		dbg_sensor(1, "[MOD:D:%d] %s, vsync_cnt(%d), long coarse(%d) min(%d)\n", cis->id, __func__,
			cis_data->sen_vsync_count, long_coarse_int, cis_data->min_coarse_integration_time);
		long_coarse_int = cis_data->min_coarse_integration_time;
	}
	short_coarse_int = ((target_exposure->short_val * vt_pic_clk_freq_khz) / 1000 - min_fine_int)
											/ line_length_pck;
	if (short_coarse_int < cis_data->min_coarse_integration_time) {
		dbg_sensor(1, "[MOD:D:%d] %s, vsync_cnt(%d), short coarse(%d) min(%d)\n", cis->id, __func__,
			cis_data->sen_vsync_count, short_coarse_int, cis_data->min_coarse_integration_time);
		short_coarse_int = cis_data->min_coarse_integration_time;
	}

	if (long_coarse_int > cis_data->max_coarse_integration_time) {
		dbg_sensor(1, "[MOD:D:%d] %s, vsync_cnt(%d), long coarse(%d) max(%d)\n", cis->id, __func__,
			cis_data->sen_vsync_count, long_coarse_int, cis_data->max_coarse_integration_time);
		long_coarse_int = cis_data->max_coarse_integration_time;
	}

	if (short_coarse_int > cis_data->max_coarse_integration_time) {
		dbg_sensor(1, "[MOD:D:%d] %s, vsync_cnt(%d), short coarse(%d) max(%d)\n", cis->id, __func__,
			cis_data->sen_vsync_count, short_coarse_int, cis_data->max_coarse_integration_time);
		short_coarse_int = cis_data->max_coarse_integration_time;
	}

	cis_data->cur_long_exposure_coarse = long_coarse_int;
	cis_data->cur_short_exposure_coarse = short_coarse_int;

	I2C_MUTEX_LOCK(cis->i2c_lock);
	if (cis_data->stream_on == false)
		sensor_gn3_load_retention = false;

	hold = sensor_gn3_cis_group_param_hold_func(subdev, 0x01);
	if (hold < 0) {
		ret = hold;
		goto p_err_i2c_unlock;
	}

	/* Short exposure */
	ret = cis->ixc_ops->write16(client, 0x0202, short_coarse_int);
	if (ret < 0)
		goto p_err_i2c_unlock;

	/* CIT shifter */
	if (cis->long_term_mode.sen_strm_off_on_enable == false) {
		ret = cis->ixc_ops->write16(client, 0x0704, coarse_integration_time_shifter);
		if (ret < 0)
			goto p_err_i2c_unlock;
	}

	dbg_sensor(1, "[MOD:D:%d] %s, vsync_cnt(%d), vt_pic_clk_freq_khz (%d),"
		KERN_CONT "line_length_pck(%d), min_fine_int (%d)\n",
		cis->id, __func__, cis_data->sen_vsync_count, vt_pic_clk_freq_khz/1000,
		line_length_pck, min_fine_int);
	dbg_sensor(1, "[MOD:D:%d] %s, vsync_cnt(%d), frame_length_lines(%#x),"
		KERN_CONT "long_coarse_int %#x, short_coarse_int %#x coarse_integration_time_shifter %#x\n",
		cis->id, __func__, cis_data->sen_vsync_count, cis_data->frame_length_lines,
		long_coarse_int, short_coarse_int, coarse_integration_time_shifter);

	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %ldus", __func__, PABLO_KTIME_US_DELTA_NOW(st));

p_err_i2c_unlock:
	if (hold > 0) {
		hold = sensor_gn3_cis_group_param_hold_func(subdev, 0x00);
		if (hold < 0)
			ret = hold;
	}
	I2C_MUTEX_UNLOCK(cis->i2c_lock);

p_err:
	return ret;
}

int sensor_gn3_cis_get_min_exposure_time(struct v4l2_subdev *subdev, u32 *min_expo)
{
	int ret = 0;
	struct is_cis *cis = NULL;
	cis_shared_data *cis_data = NULL;
	u32 min_integration_time = 0;
	u32 min_coarse = 0;
	u32 min_fine = 0;
	u64 vt_pic_clk_freq_khz = 0;
	u32 line_length_pck = 0;
	ktime_t st = ktime_get();

	WARN_ON(!subdev);
	WARN_ON(!min_expo);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);

	WARN_ON(!cis);
	WARN_ON(!cis->cis_data);

	cis_data = cis->cis_data;

	vt_pic_clk_freq_khz = cis_data->pclk / (1000);
	if (vt_pic_clk_freq_khz == 0) {
		pr_err("[MOD:D:%d] %s, Invalid vt_pic_clk_freq_khz(%llu)\n", cis->id, __func__, vt_pic_clk_freq_khz/1000);
		goto p_err;
	}
	line_length_pck = cis_data->line_length_pck;
	min_coarse = cis_data->min_coarse_integration_time;
	min_fine = cis_data->min_fine_integration_time;

	min_integration_time = (u32)((u64)((line_length_pck * min_coarse) + min_fine) * 1000 / vt_pic_clk_freq_khz);
	*min_expo = min_integration_time;

	dbg_sensor(1, "[%s] min integration time %d\n", __func__, min_integration_time);

	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %ldus", __func__, PABLO_KTIME_US_DELTA_NOW(st));

p_err:
	return ret;
}

int sensor_gn3_cis_get_max_exposure_time(struct v4l2_subdev *subdev, u32 *max_expo)
{
	int ret = 0;
	struct is_cis *cis;
	cis_shared_data *cis_data;
	u32 max_integration_time = 0;
	u32 max_coarse_margin = 0;
	u32 max_fine_margin = 0;
	u32 max_coarse = 0;
	u32 max_fine = 0;
	u64 vt_pic_clk_freq_khz = 0;
	u32 line_length_pck = 0;
	u32 frame_length_lines = 0;
	ktime_t st = ktime_get();

	WARN_ON(!subdev);
	WARN_ON(!max_expo);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);

	WARN_ON(!cis);
	WARN_ON(!cis->cis_data);

	cis_data = cis->cis_data;

	vt_pic_clk_freq_khz = cis_data->pclk / (1000);
	if (vt_pic_clk_freq_khz == 0) {
		pr_err("[MOD:D:%d] %s, Invalid vt_pic_clk_freq_khz(%llu)\n", cis->id, __func__, vt_pic_clk_freq_khz/1000);
		goto p_err;
	}
	line_length_pck = cis_data->line_length_pck;
	frame_length_lines = cis_data->frame_length_lines;

	max_coarse_margin = cis_data->max_margin_coarse_integration_time;
	max_fine_margin = line_length_pck - cis_data->min_fine_integration_time;
	max_coarse = frame_length_lines - max_coarse_margin;
	max_fine = cis_data->max_fine_integration_time;

	max_integration_time = (u32)((u64)((line_length_pck * max_coarse) + max_fine) * 1000 / vt_pic_clk_freq_khz);

	*max_expo = max_integration_time;

	/* TODO: Is this values update hear? */
	cis_data->max_margin_fine_integration_time = max_fine_margin;
	cis_data->max_coarse_integration_time = max_coarse;

	dbg_sensor(1, "[%s] max integration time %d, max margin fine integration %d, max coarse integration %d\n",
			__func__, max_integration_time,
			cis_data->max_margin_fine_integration_time, cis_data->max_coarse_integration_time);

	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %ldus", __func__, PABLO_KTIME_US_DELTA_NOW(st));

p_err:
	return ret;
}

int sensor_gn3_cis_adjust_frame_duration(struct v4l2_subdev *subdev,
						u32 input_exposure_time,
						u32 *target_duration)
{
	int ret = 0;
	struct is_cis *cis;
	cis_shared_data *cis_data;

	u64 vt_pic_clk_freq_khz = 0;
	u32 line_length_pck = 0;
	u32 frame_length_lines = 0;
	u32 frame_duration = 0;
	ktime_t st = ktime_get();

	WARN_ON(!subdev);
	WARN_ON(!target_duration);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);

	WARN_ON(!cis);
	WARN_ON(!cis->cis_data);

	cis_data = cis->cis_data;

	I2C_MUTEX_LOCK(cis->i2c_lock);

#if IS_ENABLED(USE_CAMERA_SENSOR_RETENTION)
	sensor_gn3_cis_update_seamless_mode(subdev);
#endif

	I2C_MUTEX_UNLOCK(cis->i2c_lock);

	if (input_exposure_time == 0) {
		input_exposure_time  = cis_data->min_frame_us_time;
		info("[%s] Not proper exposure time(0), so apply min frame duration to exposure time forcely!!!(%d)\n",
			__func__, cis_data->min_frame_us_time);
	}

	vt_pic_clk_freq_khz = cis_data->pclk / (1000);
	line_length_pck = cis_data->line_length_pck;
	frame_length_lines = (u32)(((vt_pic_clk_freq_khz * input_exposure_time) / 1000
						- cis_data->min_fine_integration_time) / line_length_pck);
	frame_length_lines += cis_data->max_margin_coarse_integration_time;

	frame_duration = (u32)(((u64)frame_length_lines * line_length_pck) * 1000 / vt_pic_clk_freq_khz);

	dbg_sensor(1, "[%s](vsync cnt = %d) input exp(%d), adj duration, frame duraion(%d), min_frame_us(%d)\n",
			__func__, cis_data->sen_vsync_count,
			input_exposure_time, frame_duration, cis_data->min_frame_us_time);
	dbg_sensor(1, "[%s](vsync cnt = %d) adj duration, frame duraion(%d), min_frame_us(%d)\n",
			__func__, cis_data->sen_vsync_count, frame_duration, cis_data->min_frame_us_time);

	if (cis->long_term_mode.sen_strm_off_on_enable == false)
		*target_duration = MAX(frame_duration, cis_data->min_frame_us_time);
	else
		*target_duration = frame_duration;

	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %ldus", __func__, PABLO_KTIME_US_DELTA_NOW(st));

	return ret;
}

int sensor_gn3_cis_set_frame_duration(struct v4l2_subdev *subdev, u32 frame_duration)
{
	int ret = 0;
	int hold = 0;
	struct is_cis *cis;
	struct i2c_client *client;
	cis_shared_data *cis_data;

	u64 vt_pic_clk_freq_khz = 0;
	u32 line_length_pck = 0;
	u16 frame_length_lines = 0;
	u8 frame_length_lines_shifter = 0;

	u8 fll_shifter_array[33] = {0, 1, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4,
		5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 6};
	int fll_shifter_idx = 0;
	u8 fll_denom_array[7] = {1, 2, 4, 8, 16, 32, 64};
	ktime_t st = ktime_get();

	WARN_ON(!subdev);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);

	WARN_ON(!cis);
	WARN_ON(!cis->cis_data);

	client = cis->client;
	if (unlikely(!client)) {
		err("client is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	cis_data = cis->cis_data;

	if (cis->long_term_mode.sen_strm_off_on_enable == false) {
		if (frame_duration < cis_data->min_frame_us_time) {
			dbg_sensor(1, "frame duration is less than min(%d)\n", frame_duration);
			frame_duration = cis_data->min_frame_us_time;
		}
	}

	sensor_gn3_frame_duration_backup = frame_duration;
	cis_data->cur_frame_us_time = frame_duration;

	if (cis->long_term_mode.sen_strm_off_on_enable == false) {
		switch (cis_data->sens_config_index_cur) {
		default:
			if (frame_duration > 160000) {
				fll_shifter_idx = MIN(MAX(frame_duration / 160000, 0), 32);
				frame_length_lines_shifter = fll_shifter_array[fll_shifter_idx];
				frame_duration = frame_duration / fll_denom_array[frame_length_lines_shifter];
			} else {
				frame_length_lines_shifter = 0x0;
			}
			break;
		}
	}

	vt_pic_clk_freq_khz = cis_data->pclk / (1000);
	line_length_pck = cis_data->line_length_pck;

	frame_length_lines = (u16)((vt_pic_clk_freq_khz * frame_duration) / (line_length_pck * 1000));

	dbg_sensor(1, "[MOD:D:%d] %s, vt_pic_clk_freq_khz(%llu)) frame_duration = %d us,"
			KERN_CONT "(line_length_pck%#x), frame_length_lines(%#x), frame_length_lines_shifter(%#x)\n",
			cis->id, __func__, vt_pic_clk_freq_khz/1000, frame_duration,
			line_length_pck, frame_length_lines, frame_length_lines_shifter);

	I2C_MUTEX_LOCK(cis->i2c_lock);
	hold = sensor_gn3_cis_group_param_hold_func(subdev, 0x01);
	if (hold < 0) {
		ret = hold;
		goto p_err_i2c_unlock;
	}

	ret |= cis->ixc_ops->write16(client, 0x0340, frame_length_lines);

	if (ret < 0)
		goto p_err_i2c_unlock;

	/* frame duration shifter */
	if (cis->long_term_mode.sen_strm_off_on_enable == false) {
		ret = cis->ixc_ops->write8(client, 0x0702, frame_length_lines_shifter);
		if (ret < 0)
			goto p_err_i2c_unlock;
	}

	cis_data->frame_length_lines = frame_length_lines;
	cis_data->max_coarse_integration_time =
		cis_data->frame_length_lines - cis_data->max_margin_coarse_integration_time;
	cis_data->frame_length_lines_shifter = frame_length_lines_shifter;

	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %ldus", __func__, PABLO_KTIME_US_DELTA_NOW(st));

p_err_i2c_unlock:
	if (hold > 0) {
		hold = sensor_gn3_cis_group_param_hold_func(subdev, 0x00);
		if (hold < 0)
			ret = hold;
	}
	I2C_MUTEX_UNLOCK(cis->i2c_lock);

p_err:
	return ret;
}

int sensor_gn3_cis_set_frame_rate(struct v4l2_subdev *subdev, u32 min_fps)
{
	int ret = 0;
	struct is_cis *cis;
	cis_shared_data *cis_data;

	u32 frame_duration = 0;
	ktime_t st = ktime_get();

	WARN_ON(!subdev);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);

	WARN_ON(!cis);
	WARN_ON(!cis->cis_data);

	cis_data = cis->cis_data;

	if (min_fps > cis_data->max_fps) {
		err("[MOD:D:%d] %s, request FPS is too high(%d), set to max(%d)\n",
			cis->id, __func__, min_fps, cis_data->max_fps);
		min_fps = cis_data->max_fps;
	}

	if (min_fps == 0) {
		err("[MOD:D:%d] %s, request FPS is 0, set to min FPS(1)\n",
			cis->id, __func__);
		min_fps = 1;
	}

	frame_duration = (1 * 1000 * 1000) / min_fps;

	dbg_sensor(1, "[MOD:D:%d] %s, set FPS(%d), frame duration(%d)\n",
			cis->id, __func__, min_fps, frame_duration);

	ret = sensor_gn3_cis_set_frame_duration(subdev, frame_duration);
	if (ret < 0) {
		err("[MOD:D:%d] %s, set frame duration is fail(%d)\n",
			cis->id, __func__, ret);
		goto p_err;
	}

#ifdef CAMERA_REAR2
	cis_data->min_frame_us_time = MAX(frame_duration, cis_data->min_sync_frame_us_time);
#else
	cis_data->min_frame_us_time = frame_duration;
#endif

	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %ldus", __func__, PABLO_KTIME_US_DELTA_NOW(st));

p_err:

	return ret;
}

int sensor_gn3_cis_adjust_analog_gain(struct v4l2_subdev *subdev, u32 input_again, u32 *target_permile)
{
	int ret = 0;
	struct is_cis *cis;
	cis_shared_data *cis_data;
	u32 again_code = 0;
	u32 again_permile = 0;

	WARN_ON(!subdev);
	WARN_ON(!target_permile);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);

	WARN_ON(!cis);
	WARN_ON(!cis->cis_data);

	cis_data = cis->cis_data;

	again_code = sensor_cis_calc_again_code(input_again);

	if (again_code > cis_data->max_analog_gain[0])
		again_code = cis_data->max_analog_gain[0];
	else if (again_code < cis_data->min_analog_gain[0])
		again_code = cis_data->min_analog_gain[0];

	again_permile = sensor_cis_calc_again_permile(again_code);

	dbg_sensor(1, "[%s] min again(%d), max(%d), input_again(%d), code(%d), permile(%d)\n", __func__,
			cis_data->max_analog_gain[0],
			cis_data->min_analog_gain[0],
			input_again,
			again_code,
			again_permile);

	*target_permile = again_permile;

	return ret;
}

int sensor_gn3_cis_set_analog_gain(struct v4l2_subdev *subdev, struct ae_param *again)
{
	int ret = 0;
	int hold = 0;
	struct is_cis *cis;
	struct i2c_client *client;

	u16 analog_gain = 0;
	ktime_t st = ktime_get();

	WARN_ON(!subdev);
	WARN_ON(!again);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);

	WARN_ON(!cis);

	client = cis->client;
	if (unlikely(!client)) {
		err("client is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	sensor_gn3_again_backup.short_val = again->short_val;
	sensor_gn3_again_backup.long_val = again->long_val;

	analog_gain = (u16)sensor_cis_calc_again_code(again->val);

	if (analog_gain < cis->cis_data->min_analog_gain[0]) {
		info("[%s] not proper analog_gain value, reset to min_analog_gain\n", __func__);
		analog_gain = cis->cis_data->min_analog_gain[0];
	}

	if (analog_gain > cis->cis_data->max_analog_gain[0]) {
		info("[%s] not proper analog_gain value, reset to max_analog_gain\n", __func__);
		analog_gain = cis->cis_data->max_analog_gain[0];
	}

	dbg_sensor(1, "[MOD:D:%d] %s(vsync cnt = %d), input_again = %d us, analog_gain(%#x)\n",
		cis->id, __func__, cis->cis_data->sen_vsync_count, again->val, analog_gain);

	I2C_MUTEX_LOCK(cis->i2c_lock);
	if (cis->cis_data->stream_on == false)
		sensor_gn3_load_retention = false;


	hold = sensor_gn3_cis_group_param_hold_func(subdev, 0x01);
	if (hold < 0) {
		ret = hold;
		goto p_err_i2c_unlock;
	}

	ret = cis->ixc_ops->write16(client, 0x0204, analog_gain);
	if (ret < 0)
		goto p_err_i2c_unlock;

	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %ldus", __func__, PABLO_KTIME_US_DELTA_NOW(st));

p_err_i2c_unlock:
	if (hold > 0) {
		hold = sensor_gn3_cis_group_param_hold_func(subdev, 0x00);
		if (hold < 0)
			ret = hold;
	}
	I2C_MUTEX_UNLOCK(cis->i2c_lock);

p_err:
	return ret;
}

int sensor_gn3_cis_get_analog_gain(struct v4l2_subdev *subdev, u32 *again)
{
	int ret = 0;
	int hold = 0;
	struct is_cis *cis;
	struct i2c_client *client;

	u16 analog_gain = 0;
	ktime_t st = ktime_get();

	WARN_ON(!subdev);
	WARN_ON(!again);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);

	WARN_ON(!cis);

	client = cis->client;
	if (unlikely(!client)) {
		err("client is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	I2C_MUTEX_LOCK(cis->i2c_lock);
	hold = sensor_gn3_cis_group_param_hold_func(subdev, 0x01);
	if (hold < 0) {
		ret = hold;
		goto p_err_i2c_unlock;
	}

	ret = cis->ixc_ops->read16(client, 0x0204, &analog_gain);
	if (ret < 0)
		goto p_err_i2c_unlock;

	*again = sensor_cis_calc_again_permile(analog_gain);

	dbg_sensor(1, "[MOD:D:%d] %s, cur_again = %d us, analog_gain(%#x)\n",
			cis->id, __func__, *again, analog_gain);

	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %ldus", __func__, PABLO_KTIME_US_DELTA_NOW(st));

p_err_i2c_unlock:
	if (hold > 0) {
		hold = sensor_gn3_cis_group_param_hold_func(subdev, 0x00);
		if (hold < 0)
			ret = hold;
	}
	I2C_MUTEX_UNLOCK(cis->i2c_lock);

p_err:
	return ret;
}

int sensor_gn3_cis_get_min_analog_gain(struct v4l2_subdev *subdev, u32 *min_again)
{
	int ret = 0;
	struct is_cis *cis;
	cis_shared_data *cis_data;
	ktime_t st = ktime_get();

	WARN_ON(!subdev);
	WARN_ON(!min_again);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);

	WARN_ON(!cis);
	WARN_ON(!cis->cis_data);

	cis_data = cis->cis_data;
	cis_data->min_analog_gain[0] = 0x20; /* x1, gain=x/0x20 */
	cis_data->min_analog_gain[1] = sensor_cis_calc_again_permile(cis_data->min_analog_gain[0]);

	*min_again = cis_data->min_analog_gain[1];

	dbg_sensor(1, "[%s] code %d, permile %d\n", __func__, cis_data->min_analog_gain[0],
		cis_data->min_analog_gain[1]);

	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %ldus", __func__, PABLO_KTIME_US_DELTA_NOW(st));

	return ret;
}

int sensor_gn3_cis_set_digital_gain(struct v4l2_subdev *subdev, struct ae_param *dgain)
{
	int ret = 0;
	int hold = 0;
	struct is_cis *cis;
	struct i2c_client *client;
	cis_shared_data *cis_data;

	u16 long_gain = 0;
	u16 short_gain = 0;
	ktime_t st = ktime_get();

	WARN_ON(!subdev);
	WARN_ON(!dgain);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);

	WARN_ON(!cis);
	WARN_ON(!cis->cis_data);

	client = cis->client;
	if (unlikely(!client)) {
		err("client is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	sensor_gn3_dgain_backup.short_val = dgain->short_val;
	sensor_gn3_dgain_backup.long_val = dgain->long_val;

	cis_data = cis->cis_data;

	long_gain = (u16)sensor_cis_calc_dgain_code(dgain->long_val);
	short_gain = (u16)sensor_cis_calc_dgain_code(dgain->short_val);

	if (long_gain < cis_data->min_digital_gain[0]) {
		info("[%s] not proper long_gain value, reset to min_digital_gain\n", __func__);
		long_gain = cis_data->min_digital_gain[0];
	}

	if (long_gain > cis_data->max_digital_gain[0]) {
		info("[%s] not proper long_gain value, reset to max_digital_gain\n", __func__);
		long_gain = cis_data->max_digital_gain[0];
	}

	if (short_gain < cis_data->min_digital_gain[0]) {
		info("[%s] not proper short_gain value, reset to min_digital_gain\n", __func__);
		short_gain = cis_data->min_digital_gain[0];
	}

	if (short_gain > cis_data->max_digital_gain[0]) {
		info("[%s] not proper short_gain value, reset to max_digital_gain\n", __func__);
		short_gain = cis_data->max_digital_gain[0];
	}

	dbg_sensor(1, "[MOD:D:%d] %s(vsync cnt = %d), input_dgain = %d/%d us,"
			KERN_CONT "long_gain(%#x), short_gain(%#x)\n",
			cis->id, __func__, cis->cis_data->sen_vsync_count,
			dgain->long_val, dgain->short_val, long_gain, short_gain);

	I2C_MUTEX_LOCK(cis->i2c_lock);
	if (cis_data->stream_on == false)
		sensor_gn3_load_retention = false;

	hold = sensor_gn3_cis_group_param_hold_func(subdev, 0x01);
	if (hold < 0) {
		ret = hold;
		goto p_err_i2c_unlock;
	}

	/* Short digital gain */
	ret = cis->ixc_ops->write16(client, 0x020E, short_gain);
	if (ret < 0)
		goto p_err_i2c_unlock;

	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %ldus", __func__, PABLO_KTIME_US_DELTA_NOW(st));

p_err_i2c_unlock:
	if (hold > 0) {
		hold = sensor_gn3_cis_group_param_hold_func(subdev, 0x00);
		if (hold < 0)
			ret = hold;
	}
	I2C_MUTEX_UNLOCK(cis->i2c_lock);

p_err:
	return ret;
}

int sensor_gn3_cis_get_digital_gain(struct v4l2_subdev *subdev, u32 *dgain)
{
	int ret = 0;
	int hold = 0;
	struct is_cis *cis;
	struct i2c_client *client;

	u16 digital_gain = 0;
	ktime_t st = ktime_get();

	WARN_ON(!subdev);
	WARN_ON(!dgain);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);

	WARN_ON(!cis);

	client = cis->client;
	if (unlikely(!client)) {
		err("client is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	I2C_MUTEX_LOCK(cis->i2c_lock);
	hold = sensor_gn3_cis_group_param_hold_func(subdev, 0x01);
	if (hold < 0) {
		ret = hold;
		goto p_err_i2c_unlock;
	}


	/* NOTE : In S5KGN3, digital gain is long/short separated, should set 2 registers like below, */
	/* Write same value to : 0x020E : short */ /* GreenB */
	/* Write same value to : 0x0214 : short */ /* GreenR */
	/* Write same value to : Need To find : long */

	ret = cis->ixc_ops->read16(client, 0x020E, &digital_gain);
	if (ret < 0)
		goto p_err_i2c_unlock;

	*dgain = sensor_cis_calc_dgain_permile(digital_gain);

	dbg_sensor(1, "[MOD:D:%d] %s, cur_dgain = %d us, digital_gain(%#x)\n",
			cis->id, __func__, *dgain, digital_gain);

	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %ldus", __func__, PABLO_KTIME_US_DELTA_NOW(st));

p_err_i2c_unlock:
	if (hold > 0) {
		hold = sensor_gn3_cis_group_param_hold_func(subdev, 0x00);
		if (hold < 0)
			ret = hold;
	}
	I2C_MUTEX_UNLOCK(cis->i2c_lock);

p_err:
	return ret;
}

int sensor_gn3_cis_get_min_digital_gain(struct v4l2_subdev *subdev, u32 *min_dgain)
{
	int ret = 0;
	struct is_cis *cis;
	cis_shared_data *cis_data;
	ktime_t st = ktime_get();

	WARN_ON(!subdev);
	WARN_ON(!min_dgain);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);

	WARN_ON(!cis);
	WARN_ON(!cis->cis_data);

	cis_data = cis->cis_data;
	cis_data->min_digital_gain[0] = 0x100;
	cis_data->min_digital_gain[1] = sensor_cis_calc_dgain_permile(cis_data->min_digital_gain[0]);

	*min_dgain = cis_data->min_digital_gain[1];

	dbg_sensor(1, "[%s] code %d, permile %d\n", __func__, cis_data->min_digital_gain[0],
		cis_data->min_digital_gain[1]);

	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %ldus", __func__, PABLO_KTIME_US_DELTA_NOW(st));

	return ret;
}

int sensor_gn3_cis_get_max_digital_gain(struct v4l2_subdev *subdev, u32 *max_dgain)
{
	int ret = 0;
	struct is_cis *cis;
	cis_shared_data *cis_data;
	ktime_t st = ktime_get();
	u32 mode;

	WARN_ON(!subdev);
	WARN_ON(!max_dgain);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);

	WARN_ON(!cis);
	WARN_ON(!cis->cis_data);

	cis_data = cis->cis_data;
	mode = cis_data->sens_config_index_cur;
	cis_data->max_digital_gain[0] = sensor_gn3_common_mode_attr[mode].max_digital_gain;
	cis_data->max_digital_gain[1] = sensor_cis_calc_dgain_permile(cis_data->max_digital_gain[0]);

	*max_dgain = cis_data->max_digital_gain[1];

	dbg_sensor(1, "[%s] code %d, permile %d\n", __func__, cis_data->max_digital_gain[0],
		cis_data->max_digital_gain[1]);

	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %ldus", __func__, PABLO_KTIME_US_DELTA_NOW(st));

	return ret;
}

int sensor_gn3_cis_long_term_exposure(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_cis *cis;
	struct is_long_term_expo_mode *lte_mode;
	unsigned char cit_lshift_val = 0;
	unsigned char shift_count = 0;
#ifdef USE_SENSOR_LONG_EXPOSURE_SHOT
	u32 lte_expousre = 0;
#endif
	WARN_ON(!subdev);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);
	lte_mode = &cis->long_term_mode;

	I2C_MUTEX_LOCK(cis->i2c_lock);
	/* LTE mode or normal mode set */
	if (lte_mode->sen_strm_off_on_enable) {
		if (lte_mode->expo[0] > 125000) {
#ifdef USE_SENSOR_LONG_EXPOSURE_SHOT
			lte_expousre = lte_mode->expo[0];
			cit_lshift_val = (unsigned char)(lte_mode->expo[0] / 125000);
			while (cit_lshift_val) {
				cit_lshift_val = cit_lshift_val / 2;
				lte_expousre = lte_expousre / 2;
				shift_count++;
			}
			lte_mode->expo[0] = lte_expousre;
#else
			cit_lshift_val = (unsigned char)(lte_mode->expo[0] / 125000);
			while (cit_lshift_val) {
				cit_lshift_val = cit_lshift_val / 2;
				if (cit_lshift_val > 0)
					shift_count++;
			}
			lte_mode->expo[0] = 125000;
#endif
			ret |= cis->ixc_ops->write16(cis->client, 0xFCFC, 0x4000);
			ret |= cis->ixc_ops->write8(cis->client, 0x0702, shift_count);
			ret |= cis->ixc_ops->write8(cis->client, 0x0704, shift_count);
		}
	} else {
		cit_lshift_val = 0;
		ret |= cis->ixc_ops->write16(cis->client, 0xFCFC, 0x4000);
		ret |= cis->ixc_ops->write8(cis->client, 0x0702, cit_lshift_val);
		ret |= cis->ixc_ops->write8(cis->client, 0x0704, cit_lshift_val);
	}

	I2C_MUTEX_UNLOCK(cis->i2c_lock);

	info("%s enable(%d) shift_count(%d) exp(%d)",
		__func__, lte_mode->sen_strm_off_on_enable, shift_count, lte_mode->expo[0]);

	if (ret < 0) {
		pr_err("ERR[%s]: LTE register setting fail\n", __func__);
		return ret;
	}

	return ret;
}

int sensor_gn3_cis_set_frs_control(struct v4l2_subdev *subdev, u32 command)
{
	int ret = 0;
	struct is_cis *cis = NULL;
	struct is_module_enum *module;
	struct is_device_sensor_peri *sensor_peri = NULL;
	struct sensor_open_extended *ext_info = NULL;

	WARN_ON(!subdev);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);
	WARN_ON(!cis);
	WARN_ON(!cis->cis_data);

	sensor_peri = container_of(cis, struct is_device_sensor_peri, cis);
	module = sensor_peri->module;
	ext_info = &module->ext;
	WARN_ON(!ext_info);

	switch (command) {
	case FRS_SSM_START:
		pr_info("[%s] SUPER_SLOW_MOTION_START\n", __func__);
		ret |= cis->ixc_ops->write8(cis->client, 0x0A52, 0x01); /* start_user_record */
		/* ret |= cis->ixc_ops->write8(cis->client, 0x0A51, 0x01); */ /* enable_preview_during_recording */
		/* ret |= cis->ixc_ops->write8(cis->client, 0x0A55, 0x08); */ /* tg_to_oif_ratio */
		/* ret |= cis->ixc_ops->write8(cis->client, 0x0A56, 0x02); */ /* tg_to_sg_ratio */
		/* ret |= cis->ixc_ops->write16(cis->client, 0x0A58, 0x0010); */ /* q_mask_frames */
		/* ret |= cis->ixc_ops->write16(cis->client, 0x0A5A, 0x0010); */ /* before_q_frames */
		/* ret |= cis->ixc_ops->write16(cis->client, 0x0A60, 0x0050); */ /* dram_frame_num */
		break;
	case FRS_SSM_MANUAL_CUE_ENABLE:
		pr_info("[%s] SUPER_SLOW_MOTION_START_MANUAL_CUE\n", __func__);
		ret |= cis->ixc_ops->write8(cis->client, 0x0A54, 0x01); /* Manual Q Enable */
		break;
	case FRS_SSM_STOP:
		pr_info("[%s] SUPER_SLOW_MOTION_STOP\n", __func__);
		ret |= cis->ixc_ops->write8(cis->client, 0x0A51, 0x01); /* stop_user_record */
		break;
	case FRS_SSM_MODE_AUTO_MANUAL_CUE_16:
		pr_info("[%s] SUPER_SLOW_MOTION_MODE_AUTO_MANUAL_CUE_16\n", __func__);
		ret |= cis->ixc_ops->write8(cis->client, 0x0A50, 0x02);    /* Enable Manual /Auto Q */
		ret |= cis->ixc_ops->write16(cis->client, 0x0A5A, 0x0010); /* before_q_frames = 16 */
		break;
	case FRS_SSM_MODE_AUTO_MANUAL_CUE_32:
		pr_info("[%s] SUPER_SLOW_MOTION_MODE_AUTO_MANUAL_CUE_32\n", __func__);
		ret |= cis->ixc_ops->write8(cis->client, 0x0A50, 0x02);    /* Enable Manual /Auto Q */
		ret |= cis->ixc_ops->write16(cis->client, 0x0A5A, 0x0020); /* before_q_frames = 32 */
		break;
	case FRS_SSM_MODE_AUTO_MANUAL_CUE_48:
		pr_info("[%s] SUPER_SLOW_MOTION_MODE_AUTO_MANUAL_CUE_48\n", __func__);
		ret |= cis->ixc_ops->write8(cis->client, 0x0A50, 0x02);    /* Enable Manual /Auto Q */
		ret |= cis->ixc_ops->write16(cis->client, 0x0A5A, 0x0030); /* before_q_frames = 48 */
		break;
	case FRS_SSM_MODE_AUTO_MANUAL_CUE_64:
		pr_info("[%s] SUPER_SLOW_MOTION_MODE_AUTO_MANUAL_CUE_64\n", __func__);
		ret |= cis->ixc_ops->write8(cis->client, 0x0A50, 0x02);    /* Enable Manual /Auto Q */
		ret |= cis->ixc_ops->write16(cis->client, 0x0A5A, 0x0040); /* before_q_frames = 64 */
		break;
	case FRS_SSM_MODE_ONLY_MANUAL_CUE:
		ret |= cis->ixc_ops->write8(cis->client, 0x0A50, 0x01);    /* Enable Manual Q Only */
		ret |= cis->ixc_ops->write16(cis->client, 0x0A58, 0x0000); /* q_mask_frames */
		ret |= cis->ixc_ops->write16(cis->client, 0x0A5A, 0x0000); /* before_q_frames = 0 */
		break;
	case FRS_SSM_MODE_FACTORY_TEST:
		pr_info("[%s] SUPER_SLOW_MOTION_MODE_FACTORY_TEST\n", __func__);
		ext_info->use_retention_mode = SENSOR_RETENTION_INACTIVE;
		break;
	case FRS_SSM_MODE_FLICKER_DETECT_OFF:
		pr_info("[%s] FRS_SSM_MODE_FLICKER_DETECT_OFF\n", __func__);
		ret |= cis->ixc_ops->write16(cis->client, 0xFCFC, 0x2000);
		ret |= cis->ixc_ops->write8(cis->client, 0xC26E, 0x01); /* 0 : on, 1 : bypass */
		ret |= cis->ixc_ops->write16(cis->client, 0xFCFC, 0x4000);
		break;
	case FRS_SSM_MODE_FLICKER_DETECT_ENABLE:
		pr_info("[%s] FRS_SSM_MODE_FLICKER_DETECT_ON\n", __func__);
		ret |= cis->ixc_ops->write16(cis->client, 0xFCFC, 0x2000);
		ret |= cis->ixc_ops->write8(cis->client, 0xC26E, 0x00); /* 0 : on, 1 : bypass */
		ret |= cis->ixc_ops->write16(cis->client, 0xFCFC, 0x4000);
		break;
	case FRS_SSM_MODE_FPS_960:
		pr_info("[%s] SUPER_SLOW_MOTION_MODE__FPS_960\n", __func__);
		ret |= cis->ixc_ops->write8(cis->client, 0x0A53, 0x02); /* ssm fps = 960 */
		break;
	case FRS_SSM_MODE_FPS_480:
		pr_info("[%s] SUPER_SLOW_MOTION_MODE__FPS_480\n", __func__);
		ret |= cis->ixc_ops->write8(cis->client, 0x0A53, 0x01); /* ssm fps = 480 */
		break;
	case FRS_SSM_MANUAL_MODE_START:
		pr_info("[%s] SUPER_SLOW_MOTION_MANUAL_MODE_START\n", __func__);
		ret |= cis->ixc_ops->write8(cis->client, 0x0A52, 0x01); /* Start Manual Q Only */
		break;
	case FRS_SSM_AUTO_MODE_START:
		pr_info("[%s] SUPER_SLOW_MOTION_AUTO_MODE_START\n", __func__);
		ret |= cis->ixc_ops->write8(cis->client, 0x0A50, 0x01); /* Start Auto Q Only */
		break;
	case FRS_SSM_MODE_MITIGATION_ENABLE:
		pr_info("[%s] SUPER_SLOW_MOTION_MODE_MITIGATION_ENABLE\n", __func__);
		ret |= cis->ixc_ops->write8(cis->client, 0x0A50, 0x02); /* burst output w/o MD/GMC */
		break;
	default:
		pr_info("[%s] not support command(%d)\n", __func__, command);
	}

	if (ret < 0) {
		pr_err("ERR[%s]: super slow control setting fail\n", __func__);
		return ret;
	}

	return ret;
}

int sensor_gn3_cis_set_super_slow_motion_roi(struct v4l2_subdev *subdev, struct v4l2_rect *ssm_roi)
{
	int ret = 0;
	struct is_cis *cis = NULL;

	WARN_ON(!subdev);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);
	WARN_ON(!cis);
	WARN_ON(!cis->cis_data);

	pr_info("[%s] : left(%d), width(%d), top(%d), height(%d)\n", __func__,
		ssm_roi->left, ssm_roi->width, ssm_roi->top, ssm_roi->height);

	ret |= cis->ixc_ops->write16(cis->client, 0x0A62, ssm_roi->left);
	ret |= cis->ixc_ops->write16(cis->client, 0x0A64, ssm_roi->width);
	ret |= cis->ixc_ops->write16(cis->client, 0x0A66, ssm_roi->top);
	ret |= cis->ixc_ops->write16(cis->client, 0x0A68, ssm_roi->height);
	if (ret < 0) {
		pr_err("ERR[%s]: super slow roi setting fail\n", __func__);
		return ret;
	}

	return ret;
}

int sensor_gn3_cis_set_super_slow_motion_setting(struct v4l2_subdev *subdev, struct v4l2_rect *setting)
{
	int ret = 0;
	struct is_cis *cis = NULL;

	WARN_ON(!subdev);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);
	WARN_ON(!cis);
	WARN_ON(!cis->cis_data);

	if (setting->left == 0x2000 || setting->left == 0x2001) {
		if (setting->height == 0x2) {
			ret |= cis->ixc_ops->write16(cis->client, 0x6028, setting->left);
			ret |= cis->ixc_ops->write16(cis->client, 0x602A, setting->width);
			ret |= cis->ixc_ops->write16(cis->client, 0x6F12, setting->top);
			pr_info("[%s] : 0x2000 2bytes  page(%x), addr(%x), value(%x), bytes(%x)\n", __func__,
				setting->left, setting->width, setting->top, setting->height);
		} else {
			ret |= cis->ixc_ops->write16(cis->client, 0x6028, setting->left);
			ret |= cis->ixc_ops->write16(cis->client, 0x602A, setting->width);
			ret |= cis->ixc_ops->write8(cis->client, 0x6F12, setting->top);
			pr_info("[%s] :0x2000 1bytes  page(%x), addr(%x), value(%x), bytes(%x)\n", __func__,
				setting->left, setting->width, setting->top, setting->height);
		}
		ret |= cis->ixc_ops->write16(cis->client, 0xFCFC, 0x4000);
	} else {
		ret |= cis->ixc_ops->write16(cis->client, 0xFCFC, 0x4000);
		if (setting->height == 0x2) {
			ret |= cis->ixc_ops->write16(cis->client, setting->width, setting->top);
			pr_info("[%s] : 0x4000 2bytes  page(%x), addr(%x), value(%x), bytes(%x)\n", __func__,
				setting->left, setting->width, setting->top, setting->height);
		} else {
			ret |= cis->ixc_ops->write8(cis->client, setting->width, setting->top);
			pr_info("[%s] :0x4000 1bytes  page(%x), addr(%x), value(%x), bytes(%x)\n", __func__,
				setting->left, setting->width, setting->top, setting->height);
		}
	}

	if (ret < 0) {
		pr_err("ERR[%s]: super slow roi setting fail\n", __func__);
		return ret;
	}

	return ret;
}

int sensor_gn3_cis_set_super_slow_motion_threshold(struct v4l2_subdev *subdev, u32 threshold)
{
	int ret = 0;
	struct is_cis *cis = NULL;
	u8 final_threshold = (u8)threshold;

	WARN_ON(!subdev);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);
	WARN_ON(!cis);
	WARN_ON(!cis->cis_data);

	ret |= cis->ixc_ops->write16(cis->client, 0x6028, 0x2001);
	ret |= cis->ixc_ops->write16(cis->client, 0x602A, 0x2CC0);
	ret |= cis->ixc_ops->write16(cis->client, 0x6F12, final_threshold);
	ret |= cis->ixc_ops->write16(cis->client, 0xFCFC, 0x4000);
	if (ret < 0)
		pr_err("ERR[%s]: super slow threshold setting fail\n", __func__);

	pr_info("[%s] : super slow threshold(%d)\n", __func__, threshold);

	return ret;
}

int sensor_gn3_cis_get_super_slow_motion_threshold(struct v4l2_subdev *subdev, u32 *threshold)
{
	int ret = 0;
	struct is_cis *cis = NULL;
	u8 final_threshold = 0;

	WARN_ON(!subdev);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);
	WARN_ON(!cis);
	WARN_ON(!cis->cis_data);

	ret |= cis->ixc_ops->write16(cis->client, 0x602C, 0x2000);
	ret |= cis->ixc_ops->write16(cis->client, 0x602E, 0xFF75);
	ret |= cis->ixc_ops->read8(cis->client, 0x6F12, &final_threshold);
	ret |= cis->ixc_ops->write16(cis->client, 0xFCFC, 0x4000);
	if (ret < 0) {
		pr_err("ERR[%s]: super slow threshold getting fail\n", __func__);
		*threshold = 0;
		return ret;
	}

	*threshold = final_threshold;

	pr_info("[%s] : super slow threshold(%d)\n", __func__, *threshold);

	return ret;
}

int sensor_gn3_cis_get_super_slow_motion_gmc(struct v4l2_subdev *subdev, u32 *gmc)
{
	int ret = 0;
	struct is_cis *cis = NULL;
	u8 gmc_state = 0;

	WARN_ON(!subdev);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);
	WARN_ON(!cis);
	WARN_ON(!cis->cis_data);

	ret |= cis->ixc_ops->write16(cis->client, 0x602C, 0x2000);
	ret |= cis->ixc_ops->write16(cis->client, 0x602E, 0x0E484);
	ret |= cis->ixc_ops->read8(cis->client, 0x6F12, &gmc_state);
	ret |= cis->ixc_ops->write16(cis->client, 0xFCFC, 0x4000);
	if (ret < 0) {
		pr_err("ERR[%s]: super slow gmc getting fail\n", __func__);
		*gmc = 0;
		return ret;
	}

	*gmc = gmc_state;

	pr_info("[%s] : super slow gmc(%d)\n", __func__, *gmc);

	return ret;
}

int sensor_gn3_cis_get_super_slow_motion_frame_id(struct v4l2_subdev *subdev, u32 *frameid)
{
	int ret = 0;
	struct is_cis *cis = NULL;
	u8 frame_id = 0;

	WARN_ON(!subdev);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);
	WARN_ON(!cis);
	WARN_ON(!cis->cis_data);

	ret |= cis->ixc_ops->read8(cis->client, 0x000A, &frame_id);
	if (ret < 0) {
		pr_err("ERR[%s]: super slow frame id getting fail\n", __func__);
		*frameid = 0;
		return ret;
	}

	*frameid = frame_id;

	pr_info("[%s] : super slow frame_id(%d)\n", __func__, *frameid);

	return ret;
}

int sensor_gn3_cis_set_super_slow_motion_flicker(struct v4l2_subdev *subdev, u32 flicker)
{
	int ret = 0;
	struct is_cis *cis = NULL;
	u8 final_flicker = (u8)flicker;

	WARN_ON(!subdev);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);
	WARN_ON(!cis);
	WARN_ON(!cis->cis_data);

	ret |= cis->ixc_ops->write8(cis->client, 0x0C21, final_flicker);
	if (ret < 0)
		pr_err("ERR[%s]: super slow flicker setting fail\n", __func__);

	pr_info("[%s] : super slow flicker(%d)\n", __func__, flicker);

	return ret;
}

int sensor_gn3_cis_get_super_slow_motion_flicker(struct v4l2_subdev *subdev, u32 *flicker)
{
	int ret = 0;
	struct is_cis *cis = NULL;
	u8 final_flicker = 0;

	WARN_ON(!subdev);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);
	WARN_ON(!cis);
	WARN_ON(!cis->cis_data);

	ret |= cis->ixc_ops->read8(cis->client, 0x1982, &final_flicker);
	if (ret < 0) {
		pr_err("ERR[%s]: super slow threshold getting fail\n", __func__);
		*flicker = 0;
		return ret;
	}

	*flicker = final_flicker;

	pr_info("[%s] : super slow flicker(%d)\n", __func__, *flicker);

	return ret;
}

int sensor_gn3_cis_get_super_slow_motion_md_threshold(struct v4l2_subdev *subdev, u32 *threshold)
{
	int ret = 0;
	struct is_cis *cis = NULL;
	u16 md_threshold = 0;

	WARN_ON(!subdev);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);
	WARN_ON(!cis);
	WARN_ON(!cis->cis_data);

	ret |= cis->ixc_ops->read16(cis->client, 0xDB0E, &md_threshold);
	if (ret < 0) {
		pr_err("ERR[%s]: super slow threshold getting fail\n", __func__);
		*threshold = 0;
		return ret;
	}

	*threshold = md_threshold;

	pr_info("[%s] : super slow threshold(%d)\n", __func__, *threshold);

	return ret;
}

int sensor_gn3_cis_set_super_slow_motion_gmc_table_idx(struct v4l2_subdev *subdev, u32 index)
{
	int ret = 0;
	struct is_cis *cis = NULL;
	u16 table_idx = (u16)index;

	WARN_ON(!subdev);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);
	WARN_ON(!cis);
	WARN_ON(!cis->cis_data);

	ret |= cis->ixc_ops->write16(cis->client, 0x6028, 0x2001);
	ret |= cis->ixc_ops->write16(cis->client, 0x602A, 0x3D14);
	ret |= cis->ixc_ops->write16(cis->client, 0x6F12, table_idx);
	ret |= cis->ixc_ops->write16(cis->client, 0xFCFC, 0x4000);
	if (ret < 0)
		pr_err("ERR[%s]: super slow gmc table idx setting fail\n", __func__);

	pr_info("[%s] : super slow table_idx(%d)\n", __func__, table_idx);

	return ret;
}

int sensor_gn3_cis_set_super_slow_motion_gmc_block_with_md_low(struct v4l2_subdev *subdev, u32 block)
{
	int ret = 0;
	struct is_cis *cis = NULL;
	u16 gmc_block = (u16)block;

	WARN_ON(!subdev);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);
	WARN_ON(!cis);
	WARN_ON(!cis->cis_data);

	ret |= cis->ixc_ops->write16(cis->client, 0x6028, 0x2001);
	ret |= cis->ixc_ops->write16(cis->client, 0x602A, 0x3D68);
	ret |= cis->ixc_ops->write16(cis->client, 0x6F12, gmc_block);
	ret |= cis->ixc_ops->write16(cis->client, 0xFCFC, 0x4000);
	if (ret < 0)
		pr_err("ERR[%s]: super slow gmc block with md low setting fail\n", __func__);

	pr_info("[%s] : super slow gmc_block(%d)\n", __func__, gmc_block);

	return ret;
}

int sensor_gn3_cis_set_factory_control(struct v4l2_subdev *subdev, u32 command)
{
	int ret = 0;
	struct is_cis *cis = NULL;
	struct is_module_enum *module;
	struct is_device_sensor_peri *sensor_peri = NULL;
	struct sensor_open_extended *ext_info = NULL;

	WARN_ON(!subdev);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);
	WARN_ON(!cis);
	WARN_ON(!cis->cis_data);

	sensor_peri = container_of(cis, struct is_device_sensor_peri, cis);
	module = sensor_peri->module;
	ext_info = &module->ext;
	WARN_ON(!ext_info);

	switch (command) {
	case FAC_CTRL_BIT_TEST:
		pr_info("[%s] FAC_CTRL_BIT_TEST to be checked\n", __func__);

		break;
	default:
		pr_info("[%s] not support command(%d)\n", __func__, command);
	}

	return ret;
}

int sensor_gn3_cis_compensate_gain_for_extremely_br(struct v4l2_subdev *subdev, u32 expo, u32 *again, u32 *dgain)
{
	int ret = 0;
	struct is_cis *cis;
	cis_shared_data *cis_data;

	u64 vt_pic_clk_freq_khz = 0;
	u32 line_length_pck = 0;
	u32 min_fine_int = 0;
	u32 coarse_int = 0;
	u32 compensated_again = 0;

	FIMC_BUG(!subdev);
	FIMC_BUG(!again);
	FIMC_BUG(!dgain);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);
	if (!cis) {
		err("cis is NULL");
		ret = -EINVAL;
		goto p_err;
	}
	cis_data = cis->cis_data;

	vt_pic_clk_freq_khz = cis_data->pclk / (1000);
	line_length_pck = cis_data->line_length_pck;
	min_fine_int = cis_data->min_fine_integration_time;

	if (line_length_pck <= 0) {
		err("[%s] invalid line_length_pck(%d)\n", __func__, line_length_pck);
		goto p_err;
	}

	coarse_int = ((expo * vt_pic_clk_freq_khz) / 1000 - min_fine_int) / line_length_pck;
	if (coarse_int < cis_data->min_coarse_integration_time) {
		dbg_sensor(1, "[MOD:D:%d] %s, vsync_cnt(%d), long coarse(%d) min(%d)\n", cis->id, __func__,
			cis_data->sen_vsync_count, coarse_int, cis_data->min_coarse_integration_time);
		coarse_int = cis_data->min_coarse_integration_time;
	}

	if (coarse_int <= 1024) {
		compensated_again = (*again * ((expo * vt_pic_clk_freq_khz) / 1000 - min_fine_int))
			/ (line_length_pck * coarse_int);

		if (compensated_again < cis_data->min_analog_gain[1]) {
			*again = cis_data->min_analog_gain[1];
		} else if (*again >= cis_data->max_analog_gain[1]) {
			*dgain = (*dgain * ((expo * vt_pic_clk_freq_khz) / 1000 - min_fine_int))
				/ (line_length_pck * coarse_int);
		} else {
			/* *again = compensated_again; */
			*dgain = (*dgain * ((expo * vt_pic_clk_freq_khz) / 1000 - min_fine_int))
				/ (line_length_pck * coarse_int);
		}

		dbg_sensor(1, "[%s] exp(%d), again(%d), dgain(%d), coarse_int(%d), compensated_again(%d)\n",
			__func__, expo, *again, *dgain, coarse_int, compensated_again);
	}

p_err:
	return ret;
}

int sensor_gn3_cis_set_wb_gain(struct v4l2_subdev *subdev, struct wb_gains wb_gains)
{
	int ret = 0;
	int hold = 0;
	struct is_cis *cis;
	struct i2c_client *client;
	int mode = 0;
	u16 abs_gains[3] = {0, }; /* R, G, B */
	u32 avg_g = 0;
	ktime_t st = ktime_get();

	FIMC_BUG(!subdev);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);

	FIMC_BUG(!cis);
	FIMC_BUG(!cis->cis_data);

	client = cis->client;
	if (unlikely(!client)) {
		err("client is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	if (!cis->use_wb_gain)
		return ret;

	mode = cis->cis_data->sens_config_index_cur;
	if (!sensor_gn3_spec_mode_attr[mode].wb_gain_support)
		return 0;

	if (wb_gains.gr != wb_gains.gb) {
		err("gr, gb not euqal"); /* check DDK layer */
		return -EINVAL;
	}

	if (wb_gains.gr != 1024) {
		err("invalid gr,gb %d", wb_gains.gr); /* check DDK layer */
		return -EINVAL;
	}

	dbg_sensor(1, "[SEN:%d]%s:DDK vlaue: wb_gain_gr(%d), wb_gain_r(%d), wb_gain_b(%d), wb_gain_gb(%d)\n",
		cis->id, __func__, wb_gains.gr, wb_gains.r, wb_gains.b, wb_gains.gb);

	avg_g = (wb_gains.gr + wb_gains.gb) / 2;
	abs_gains[0] = (u16)(wb_gains.r & 0xFFFF);
	abs_gains[1] = (u16)(avg_g & 0xFFFF);
	abs_gains[2] = (u16)(wb_gains.b & 0xFFFF);

	dbg_sensor(1, "[SEN:%d]%s, abs_gain_r(0x%4X), abs_gain_g(0x%4X), abs_gain_b(0x%4X)\n",
		cis->id, __func__, abs_gains[0], abs_gains[1], abs_gains[2]);

	hold = sensor_gn3_cis_group_param_hold(subdev, 0x01);
	if (hold < 0) {
		ret = hold;
		goto p_err;
	}

	I2C_MUTEX_LOCK(cis->i2c_lock);
	ret |= cis->ixc_ops->write16(client, 0xFCFC, 0x4000);
	ret |= cis->ixc_ops->write16(client, 0x0D82, abs_gains[0]);
	ret |= cis->ixc_ops->write16(client, 0x0D84, abs_gains[1]);
	ret |= cis->ixc_ops->write16(client, 0x0D86, abs_gains[2]);
	if (ret < 0) {
		err("sensor_gn3_set_registers fail!!");
		goto p_i2c_err;
	}

	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %ldus", __func__, PABLO_KTIME_US_DELTA_NOW(st));

p_i2c_err:
	I2C_MUTEX_UNLOCK(cis->i2c_lock);

p_err:
	if (hold > 0) {
		hold = sensor_gn3_cis_group_param_hold(subdev, 0x00);
		if (hold < 0)
			ret = hold;
	}

	return ret;
}

int sensor_gn3_cis_recover_stream_on(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_cis *cis = NULL;
#if IS_ENABLED(USE_CAMERA_SENSOR_RETENTION)
	struct is_module_enum *module;
	struct is_device_sensor_peri *sensor_peri = NULL;
	struct sensor_open_extended *ext_info;
#endif

	FIMC_BUG(!subdev);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);
	FIMC_BUG(!cis);
	FIMC_BUG(!cis->cis_data);

#if IS_ENABLED(USE_CAMERA_SENSOR_RETENTION)
	sensor_peri = container_of(cis, struct is_device_sensor_peri, cis);
	module = sensor_peri->module;
	ext_info = &module->ext;
	FIMC_BUG(!ext_info);

	ext_info->use_retention_mode = SENSOR_RETENTION_INACTIVE;
#endif

	info("%s start\n", __func__);

	ret = sensor_gn3_cis_set_global_setting(subdev);
	if (ret < 0)
		goto p_err;
	ret = sensor_gn3_cis_mode_change(subdev, cis->cis_data->sens_config_index_cur);
	if (ret < 0)
		goto p_err;
	ret = sensor_gn3_cis_set_frame_duration(subdev, sensor_gn3_frame_duration_backup);
	if (ret < 0)
		goto p_err;
	ret = sensor_gn3_cis_set_analog_gain(subdev, &sensor_gn3_again_backup);
	if (ret < 0)
		goto p_err;
	ret = sensor_gn3_cis_set_digital_gain(subdev, &sensor_gn3_dgain_backup);
	if (ret < 0)
		goto p_err;
	ret = sensor_gn3_cis_set_exposure_time(subdev, &sensor_gn3_target_exp_backup);
	if (ret < 0)
		goto p_err;
	ret = sensor_gn3_cis_stream_on(subdev);
	if (ret < 0)
		goto p_err;
	ret = sensor_gn3_cis_wait_streamon(subdev);
	if (ret < 0)
		goto p_err;

	info("%s end\n", __func__);
p_err:
	return ret;
}

int sensor_gn3_cis_recover_stream_off(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_cis *cis = NULL;
#if IS_ENABLED(USE_CAMERA_SENSOR_RETENTION)
	struct is_module_enum *module;
	struct is_device_sensor_peri *sensor_peri = NULL;
	struct sensor_open_extended *ext_info;
#endif

	FIMC_BUG(!subdev);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);
	FIMC_BUG(!cis);
	FIMC_BUG(!cis->cis_data);

#if IS_ENABLED(USE_CAMERA_SENSOR_RETENTION)
	sensor_peri = container_of(cis, struct is_device_sensor_peri, cis);
	module = sensor_peri->module;
	ext_info = &module->ext;
	FIMC_BUG(!ext_info);

	ext_info->use_retention_mode = SENSOR_RETENTION_INACTIVE;
#endif

	info("%s start\n", __func__);

	ret = sensor_gn3_cis_set_global_setting(subdev);
	if (ret < 0)
		goto p_err;
	ret = sensor_gn3_cis_stream_off(subdev);
	if (ret < 0)
		goto p_err;
	ret = sensor_cis_wait_streamoff(subdev);
	if (ret < 0)
		goto p_err;

	info("%s end\n", __func__);
p_err:
	return ret;
}

static struct is_cis_ops cis_ops_gn3 = {
	.cis_init = sensor_gn3_cis_init,
	.cis_deinit = sensor_gn3_cis_deinit,
	.cis_log_status = sensor_gn3_cis_log_status,
	.cis_group_param_hold = sensor_gn3_cis_group_param_hold,
	.cis_set_global_setting = sensor_gn3_cis_set_global_setting,
	.cis_mode_change = sensor_gn3_cis_mode_change,
	.cis_set_size = sensor_gn3_cis_set_size,
	.cis_stream_on = sensor_gn3_cis_stream_on,
	.cis_stream_off = sensor_gn3_cis_stream_off,
	.cis_set_exposure_time = sensor_gn3_cis_set_exposure_time,
	.cis_get_min_exposure_time = sensor_gn3_cis_get_min_exposure_time,
	.cis_get_max_exposure_time = sensor_gn3_cis_get_max_exposure_time,
	.cis_adjust_frame_duration = sensor_gn3_cis_adjust_frame_duration,
	.cis_set_frame_duration = sensor_gn3_cis_set_frame_duration,
	.cis_set_frame_rate = sensor_gn3_cis_set_frame_rate,
	.cis_adjust_analog_gain = sensor_gn3_cis_adjust_analog_gain,
	.cis_set_analog_gain = sensor_gn3_cis_set_analog_gain,
	.cis_get_analog_gain = sensor_gn3_cis_get_analog_gain,
	.cis_get_min_analog_gain = sensor_gn3_cis_get_min_analog_gain,
	.cis_get_max_analog_gain = sensor_gn3_cis_get_max_analog_gain,
	.cis_set_digital_gain = sensor_gn3_cis_set_digital_gain,
	.cis_get_digital_gain = sensor_gn3_cis_get_digital_gain,
	.cis_get_min_digital_gain = sensor_gn3_cis_get_min_digital_gain,
	.cis_get_max_digital_gain = sensor_gn3_cis_get_max_digital_gain,
	.cis_compensate_gain_for_extremely_br = sensor_gn3_cis_compensate_gain_for_extremely_br,
	.cis_wait_streamoff = sensor_cis_wait_streamoff,
	.cis_wait_streamon = sensor_gn3_cis_wait_streamon,
	.cis_set_wb_gains = sensor_gn3_cis_set_wb_gain,
	.cis_data_calculation = sensor_gn3_cis_data_calc,
	.cis_set_long_term_exposure = sensor_gn3_cis_long_term_exposure,
	.cis_check_rev_on_init = sensor_cis_check_rev_on_init,
	.cis_set_initial_exposure = sensor_cis_set_initial_exposure,
	.cis_get_super_slow_motion_gmc = sensor_gn3_cis_get_super_slow_motion_gmc,
	.cis_get_super_slow_motion_frame_id = sensor_gn3_cis_get_super_slow_motion_frame_id,
	.cis_set_super_slow_motion_flicker = sensor_gn3_cis_set_super_slow_motion_flicker,
	.cis_get_super_slow_motion_flicker = sensor_gn3_cis_get_super_slow_motion_flicker,
	.cis_get_super_slow_motion_md_threshold = sensor_gn3_cis_get_super_slow_motion_md_threshold,
	.cis_set_super_slow_motion_gmc_table_idx = sensor_gn3_cis_set_super_slow_motion_gmc_table_idx,
	.cis_set_super_slow_motion_gmc_block_with_md_low = sensor_gn3_cis_set_super_slow_motion_gmc_block_with_md_low,
/*	.cis_recover_stream_on = sensor_gn3_cis_recover_stream_on, */
/*	.cis_recover_stream_off = sensor_gn3_cis_recover_stream_off, */
	.cis_set_factory_control = sensor_gn3_cis_set_factory_control,
};

static int cis_gn3_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	int ret;
	u32 mclk_freq_khz;
	struct is_cis *cis;
	struct is_device_sensor_peri *sensor_peri;
	char const *setfile;
	struct device_node *dnode = client->dev.of_node;
	int i;
	int index;
	const int *verify_sensor_mode = NULL;
	int verify_sensor_mode_size = 0;

	ret = sensor_cis_probe(client, &(client->dev), &sensor_peri, I2C_TYPE);
	if (ret) {
		probe_info("%s: sensor_cis_probe ret(%d)\n", __func__, ret);
		return ret;
	}

	cis = &sensor_peri->cis;
	cis->ctrl_delay = N_PLUS_TWO_FRAME;
	cis->cis_ops = &cis_ops_gn3;

	/* belows are depend on sensor cis. MUST check sensor spec */
	cis->bayer_order = OTF_INPUT_ORDER_BAYER_GB_RG;
	cis->use_wb_gain = true;

	sensor_gn3_eeprom_cal_available = false;
	sensor_gn3_first_entrance = false;
	sensor_gn3_need_stream_on_retention = true;

	ret = of_property_read_string(dnode, "setfile", &setfile);
	if (ret) {
		err("setfile index read fail(%d), take default setfile!!", ret);
		setfile = "default";
	}

	mclk_freq_khz = sensor_peri->module->pdata->mclk_freq_khz;

	if (mclk_freq_khz == 19200) {
		if (strcmp(setfile, "default") == 0 ||
				strcmp(setfile, "setA") == 0) {
			probe_info("%s setfile_A mclk: 19.2MHz\n", __func__);
			sensor_gn3_global = sensor_gn3_setfile_A_19p2_Global;
			sensor_gn3_global_size = ARRAY_SIZE(sensor_gn3_setfile_A_19p2_Global);
			sensor_gn3_setfiles = sensor_gn3_setfiles_A_19p2;
			sensor_gn3_setfile_sizes = sensor_gn3_setfile_A_19p2_sizes;
			sensor_gn3_pllinfos = sensor_gn3_pllinfos_A_19p2;
			sensor_gn3_max_setfile_num = ARRAY_SIZE(sensor_gn3_setfiles_A_19p2);
			cis->mipi_sensor_mode = sensor_gn3_setfile_A_19p2_mipi_sensor_mode;
			cis->mipi_sensor_mode_size = ARRAY_SIZE(sensor_gn3_setfile_A_19p2_mipi_sensor_mode);
			verify_sensor_mode = sensor_gn3_setfile_A_19p2_verify_sensor_mode;
			verify_sensor_mode_size = ARRAY_SIZE(sensor_gn3_setfile_A_19p2_verify_sensor_mode);
#if IS_ENABLED(USE_CAMERA_SENSOR_RETENTION)
			sensor_gn3_global_retention = sensor_gn3_setfile_A_19p2_Global;
			sensor_gn3_global_retention_size = ARRAY_SIZE(sensor_gn3_setfile_A_19p2_Global);
			sensor_gn3_retention = sensor_gn3_setfiles_A_19p2_retention;
			sensor_gn3_retention_size = sensor_gn3_setfile_A_19p2_sizes_retention;
			sensor_gn3_max_retention_num = ARRAY_SIZE(sensor_gn3_setfile_A_19p2_sizes_retention);
			sensor_gn3_load_sram = sensor_gn3_setfile_A_19p2_load_sram;
			sensor_gn3_load_sram_size = sensor_gn3_setfile_A_19p2_sizes_load_sram;
#endif
		} else {
			err("%s setfile index out of bound, take default (setfile_A mclk: 19.2MHz)", __func__);
			sensor_gn3_global = sensor_gn3_setfile_A_19p2_Global;
			sensor_gn3_global_size = ARRAY_SIZE(sensor_gn3_setfile_A_19p2_Global);
			sensor_gn3_setfiles = sensor_gn3_setfiles_A_19p2;
			sensor_gn3_setfile_sizes = sensor_gn3_setfile_A_19p2_sizes;
			sensor_gn3_pllinfos = sensor_gn3_pllinfos_A_19p2;
			sensor_gn3_max_setfile_num = ARRAY_SIZE(sensor_gn3_setfiles_A_19p2);
			cis->mipi_sensor_mode = sensor_gn3_setfile_A_19p2_mipi_sensor_mode;
			cis->mipi_sensor_mode_size = ARRAY_SIZE(sensor_gn3_setfile_A_19p2_mipi_sensor_mode);
			verify_sensor_mode = sensor_gn3_setfile_A_19p2_verify_sensor_mode;
			verify_sensor_mode_size = ARRAY_SIZE(sensor_gn3_setfile_A_19p2_verify_sensor_mode);
#if IS_ENABLED(USE_CAMERA_SENSOR_RETENTION)
			sensor_gn3_global_retention = sensor_gn3_setfile_A_19p2_Global;
			sensor_gn3_global_retention_size = ARRAY_SIZE(sensor_gn3_setfile_A_19p2_Global);
			sensor_gn3_retention = sensor_gn3_setfiles_A_19p2_retention;
			sensor_gn3_retention_size = sensor_gn3_setfile_A_19p2_sizes_retention;
			sensor_gn3_max_retention_num = ARRAY_SIZE(sensor_gn3_setfile_A_19p2_sizes_retention);
			sensor_gn3_load_sram = sensor_gn3_setfile_A_19p2_load_sram;
			sensor_gn3_load_sram_size = sensor_gn3_setfile_A_19p2_sizes_load_sram;
#endif
		}
	}
#ifndef CONFIG_CAMERA_VENDER_MCD
	else {
		if (strcmp(setfile, "default") == 0 ||
				strcmp(setfile, "setA") == 0) {
			probe_info("%s setfile_A\n", __func__);
			sensor_gn3_global = sensor_gn3_setfile_A_Global;
			sensor_gn3_global_size = ARRAY_SIZE(sensor_gn3_setfile_A_Global);
			sensor_gn3_setfiles = sensor_gn3_setfiles_A;
			sensor_gn3_setfile_sizes = sensor_gn3_setfile_A_sizes;
			sensor_gn3_pllinfos = sensor_gn3_pllinfos_A;
			sensor_gn3_max_setfile_num = ARRAY_SIZE(sensor_gn3_setfiles_A);
			cis->mipi_sensor_mode = sensor_gn3_setfile_A_mipi_sensor_mode;
			cis->mipi_sensor_mode_size = ARRAY_SIZE(sensor_gn3_setfile_A_mipi_sensor_mode);
			verify_sensor_mode = sensor_gn3_setfile_A_verify_sensor_mode;
			verify_sensor_mode_size = ARRAY_SIZE(sensor_gn3_setfile_A_verify_sensor_mode);
#if IS_ENABLED(USE_CAMERA_SENSOR_RETENTION)
			sensor_gn3_global_retention = sensor_gn3_setfile_A_Global;
			sensor_gn3_global_retention_size = ARRAY_SIZE(sensor_gn3_setfile_A_Global);
			sensor_gn3_retention = sensor_gn3_setfiles_A_retention;
			sensor_gn3_retention_size = sensor_gn3_setfile_A_sizes_retention;
			sensor_gn3_max_retention_num = ARRAY_SIZE(sensor_gn3_setfiles_A_retention);
			sensor_gn3_load_sram = sensor_gn3_setfile_A_load_sram;
			sensor_gn3_load_sram_size = sensor_gn3_setfile_A_sizes_load_sram;
#endif
		} else {
			err("%s setfile index out of bound, take default (setfile_A)", __func__);
			sensor_gn3_global = sensor_gn3_setfile_A_Global;
			sensor_gn3_global_size = ARRAY_SIZE(sensor_gn3_setfile_A_Global);
			sensor_gn3_setfiles = sensor_gn3_setfiles_A;
			sensor_gn3_setfile_sizes = sensor_gn3_setfile_A_sizes;
			sensor_gn3_pllinfos = sensor_gn3_pllinfos_A;
			sensor_gn3_max_setfile_num = ARRAY_SIZE(sensor_gn3_setfiles_A);
			cis->mipi_sensor_mode = sensor_gn3_setfile_A_mipi_sensor_mode;
			cis->mipi_sensor_mode_size = ARRAY_SIZE(sensor_gn3_setfile_A_mipi_sensor_mode);
			verify_sensor_mode = sensor_gn3_setfile_A_verify_sensor_mode;
			verify_sensor_mode_size = ARRAY_SIZE(sensor_gn3_setfile_A_verify_sensor_mode);
#if IS_ENABLED(USE_CAMERA_SENSOR_RETENTION)
			sensor_gn3_global_retention = sensor_gn3_setfile_A_Global;
			sensor_gn3_global_retention_size = ARRAY_SIZE(sensor_gn3_setfile_A_Global);
			sensor_gn3_retention = sensor_gn3_setfiles_A_retention;
			sensor_gn3_retention_size = sensor_gn3_setfile_A_sizes_retention;
			sensor_gn3_max_retention_num = ARRAY_SIZE(sensor_gn3_setfiles_A_retention);
			sensor_gn3_load_sram = sensor_gn3_setfile_A_load_sram;
			sensor_gn3_load_sram_size = sensor_gn3_setfile_A_sizes_load_sram;
#endif
		}
	}
#endif

	if (cis->vendor_use_adaptive_mipi) {
		for (i = 0; i < verify_sensor_mode_size; i++) {
			index = verify_sensor_mode[i];

			if (index >= cis->mipi_sensor_mode_size || index < 0) {
				panic("wrong mipi_sensor_mode index");
				break;
			}

			if (is_vendor_verify_mipi_channel(cis->mipi_sensor_mode[index].mipi_channel,
						cis->mipi_sensor_mode[index].mipi_channel_size)) {
				panic("wrong mipi channel");
				break;
			}
		}
	}

	probe_info("%s done\n", __func__);

	return ret;
}

static const struct of_device_id sensor_cis_gn3_match[] = {
	{
		.compatible = "samsung,exynos-is-cis-gn3",
	},
	{},
};
MODULE_DEVICE_TABLE(of, sensor_cis_gn3_match);

static const struct i2c_device_id sensor_cis_gn3_idt[] = {
	{ SENSOR_NAME, 0 },
	{},
};

static struct i2c_driver sensor_cis_gn3_driver = {
	.probe	= cis_gn3_probe,
	.driver = {
		.name	= SENSOR_NAME,
		.owner	= THIS_MODULE,
		.of_match_table = sensor_cis_gn3_match,
		.suppress_bind_attrs = true,
	},
	.id_table = sensor_cis_gn3_idt
};

#ifdef MODULE
builtin_i2c_driver(sensor_cis_gn3_driver);
#else
static int __init sensor_cis_gn3_init(void)
{
	int ret;

	ret = i2c_add_driver(&sensor_cis_gn3_driver);
	if (ret)
		err("failed to add %s driver: %d\n",
			sensor_cis_gn3_driver.driver.name, ret);

	return ret;
}
late_initcall_sync(sensor_cis_gn3_init);
#endif

MODULE_LICENSE("GPL");
