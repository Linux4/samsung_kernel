// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series Sensor driver
 *
 *
 * Copyright (c) 2016 Samsung Electronics Co., Ltd
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
#include <videodev2_exynos_camera.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/platform_device.h>
#include <linux/of_gpio.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-device.h>
#include <media/v4l2-subdev.h>

#include <exynos-is-sensor.h>
#include "is-hw.h"
#include "is-core.h"
#include "is-param.h"
#include "is-device-sensor.h"
#include "is-device-sensor-peri.h"
#include "is-resourcemgr.h"
#include "is-dt.h"
#include "is-cis-imx754.h"
#include "is-vendor.h"
#include "is-cis-imx754-setA-19p2.h"
#include "is-cis-imx754-setB-19p2.h"
#include "is-helper-ixc.h"
#ifdef CAMERA_REAR2_SENSOR_SHIFT_CROP
#include "is-sec-define.h"
#include "is-vendor-private.h"
#endif

#define SENSOR_NAME "IMX754"

static const struct v4l2_subdev_ops subdev_ops;

u32 sensor_imx754_cis_calc_again_code(u32 permille)
{
	return 16384 - (16384000 / permille);
}

u32 sensor_imx754_cis_calc_again_permile(u32 code)
{
	return 16384000 / (16384 - code);
}

void sensor_imx754_cis_set_mode_group(u32 mode)
{
	sensor_imx754_mode_groups[SENSOR_IMX754_MODE_DEFAULT] = mode;
	sensor_imx754_mode_groups[SENSOR_IMX754_MODE_AEB] = MODE_GROUP_NONE;
	sensor_imx754_mode_groups[SENSOR_IMX754_MODE_LN2] = MODE_GROUP_NONE;
	sensor_imx754_mode_groups[SENSOR_IMX754_MODE_LN4] = MODE_GROUP_NONE;

	switch (mode) {
	case SENSOR_IMX754_3648X2736_60FPS:
	case SENSOR_IMX754_3648X2736_30FPS:
		sensor_imx754_mode_groups[SENSOR_IMX754_MODE_DEFAULT] = SENSOR_IMX754_3648X2736_30FPS_NORMAL;
		sensor_imx754_mode_groups[SENSOR_IMX754_MODE_AEB] = SENSOR_IMX754_3648X2736_30FPS;
		sensor_imx754_mode_groups[SENSOR_IMX754_MODE_LN4] = SENSOR_IMX754_3648X2736_30FPS_LN4;
		break;

	case SENSOR_IMX754_3840X2160_60FPS:
	case SENSOR_IMX754_3840X2160_30FPS:
		sensor_imx754_mode_groups[SENSOR_IMX754_MODE_DEFAULT] = SENSOR_IMX754_3840X2160_30FPS_NORMAL;
		sensor_imx754_mode_groups[SENSOR_IMX754_MODE_LN4] = SENSOR_IMX754_3840X2160_30FPS_LN4;
		break;

	case SENSOR_IMX754_3648X2052_60FPS:
		sensor_imx754_mode_groups[SENSOR_IMX754_MODE_DEFAULT] = SENSOR_IMX754_3648X2052_30FPS_NORMAL;
		sensor_imx754_mode_groups[SENSOR_IMX754_MODE_LN4] = SENSOR_IMX754_3648X2052_30FPS_LN4;
		break;
	}

	info("[%s] default(%d) aeb(%d) ln2(%d) ln4(%d)\n", __func__,
		sensor_imx754_mode_groups[SENSOR_IMX754_MODE_DEFAULT],
		sensor_imx754_mode_groups[SENSOR_IMX754_MODE_AEB],
		sensor_imx754_mode_groups[SENSOR_IMX754_MODE_LN2],
		sensor_imx754_mode_groups[SENSOR_IMX754_MODE_LN4]);
}

int sensor_imx754_cis_init(struct v4l2_subdev *subdev)
{
	int ret = 0;
	u32 idx;
	struct is_cis *cis = sensor_cis_get_cis(subdev);
	ktime_t st = ktime_get();

#if !defined(CONFIG_CAMERA_VENDOR_MCD)
	memset(cis->cis_data, 0, sizeof(cis_shared_data));

	ret = sensor_cis_check_rev(cis);
	if (ret < 0) {
		warn("sensor_imx754_check_rev is fail when cis init, ret(%d)", ret);
		return -EINVAL;
	}
#endif

	cis->cis_data->cur_width = cis->sensor_info->max_width;
	cis->cis_data->cur_height = cis->sensor_info->max_height;
	cis->cis_data->low_expo_start = 33000;

	for (idx = 0; idx < SENSOR_IMX754_MODE_MAX; ++idx)
		start_pos_infos[idx].x_start = start_pos_infos[idx].y_start = 0;

	cis->cis_data->sens_config_index_pre = SENSOR_IMX754_MODE_MAX;
	cis->cis_data->sens_config_index_cur = 0;
	CALL_CISOPS(cis, cis_data_calculation, subdev, cis->cis_data->sens_config_index_cur);

	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %lldus\n", __func__, PABLO_KTIME_US_DELTA_NOW(st));

	return ret;
}

static const struct is_cis_log log_imx754[] = {
	{I2C_READ, 16, 0x0000, 0, "model_id"},
	{I2C_READ, 8, 0x0018, 0, "0x0018"},
	{I2C_READ, 8, 0x0A04, 0, "0x0A04"},
	{I2C_READ, 8, 0x0005, 0, "frame_count"},
	{I2C_READ, 16, 0x0100, 0, "0x0100"},
	{I2C_READ, 16, 0x0202, 0, "0x0202"},
	{I2C_READ, 16, 0x0204, 0, "0x0204"},
	{I2C_READ, 16, 0x020E, 0, "0x020E"},
	{I2C_READ, 16, 0x0340, 0, "0x0340"},
	{I2C_READ, 8, 0x3150, 0, "0x3150"},
	{I2C_READ, 8, 0x3151, 0, "0x3151"},
	{I2C_READ, 8, 0x3860, 0, "0x3860"},
};

int sensor_imx754_cis_log_status(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_cis *cis = sensor_cis_get_cis(subdev);

	sensor_cis_log_status(cis, log_imx754, ARRAY_SIZE(log_imx754), (char *)__func__);

	return ret;
}

int sensor_imx754_cis_set_global_setting(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_cis *cis = sensor_cis_get_cis(subdev);
	struct sensor_imx754_private_data *priv = (struct sensor_imx754_private_data *)cis->sensor_info->priv;

	info("[%s] global setting start\n", __func__);
	ret = sensor_cis_write_registers_locked(subdev, priv->global);

	if (ret < 0) {
		err("sensor_imx754_set_registers fail!!");
		return ret;
	}

	info("[%s] global setting done\n", __func__);

	return ret;
}

#ifdef CAMERA_REAR2_SENSOR_SHIFT_CROP
//#define TEST_SHIFT_CROP

#ifdef TEST_SHIFT_CROP
int test_crop_num = 0;
#endif

int sensor_imx754_cis_update_crop_region(struct v4l2_subdev *subdev)
{
	int ret = 0;
	u16 x_start = 0;
	u16 y_start = 0;
	s16 delta_x, delta_y;
	long efs_size = 0;
	s16 temp_delta = 0;
	struct is_device_sensor *device;
	struct is_core *core;
	struct is_vendor_private *vendor_priv;
	struct is_cis *cis = sensor_cis_get_cis(subdev);

#ifdef TEST_SHIFT_CROP
	int16_t crop[10][2] = {
		{0,       0}, // default ROI {176, 132}
		{-176, -132}, {0, -132}, {176, -132}, // {0,   0}, {176,   0}, {352 ,  0}
		{-176,    0}, {0,    0}, {176,    0}, // {0, 132}, {176, 132}, {352, 132}
		{-176,  132}, {0,  132}, {176,  132}, // {0, 264}, {176, 264}, {352, 264}
	};
#endif

	device = (struct is_device_sensor *)v4l2_get_subdev_hostdata(subdev);
	WARN_ON(!device);

	if (device->cfg->mode == SENSOR_IMX754_1920X1080_120FPS
		|| device->cfg->mode == SENSOR_IMX754_1920X1080_240FPS
		|| device->cfg->mode == SENSOR_IMX754_912X684_120FPS) {
		info("[%s] skip crop shift in binning & fast ae sensor mode\n", __func__);
		return 0;
	}

	core = is_get_is_core();
	vendor_priv = core->vendor.private_data;

#ifdef CAMERA_IMX754_10X_FLIP
	if (cis->id == CAMERA_IMX754_10X_FLIP) {
		efs_size = vendor_priv->tilt_cal_tele2_efs_size;
		delta_x = *((s16 *)&vendor_priv->tilt_cal_tele2_efs_data[CROP_SHIFT_ADDR_X]);
		delta_y = *((s16 *)&vendor_priv->tilt_cal_tele2_efs_data[CROP_SHIFT_ADDR_Y]);
	} else
#endif
	{
		efs_size = vendor_priv->tilt_cal_tele_efs_size;
		delta_x = *((s16 *)&vendor_priv->tilt_cal_tele_efs_data[CROP_SHIFT_ADDR_X]);
		delta_y = *((s16 *)&vendor_priv->tilt_cal_tele_efs_data[CROP_SHIFT_ADDR_Y]);
	}

	if (efs_size == 0) {
		delta_x = 0;
		delta_y = 0;
	}

	info("[%s] efs_size[%ld], delta_x[%d], delta_y[%d]\n", __func__, efs_size, delta_x, delta_y);

	if (delta_x % CROP_SHIFT_DELTA_ALIGN_X) {
		temp_delta = delta_x / CROP_SHIFT_DELTA_ALIGN_X;
		delta_x = temp_delta * CROP_SHIFT_DELTA_ALIGN_X;
	}

	if (delta_y % CROP_SHIFT_DELTA_ALIGN_Y) {
		temp_delta = delta_y / CROP_SHIFT_DELTA_ALIGN_Y;
		delta_y = temp_delta * CROP_SHIFT_DELTA_ALIGN_Y;
	}

	ret = cis->ixc_ops->read16(cis->client, 0x0408, &x_start);
	ret = cis->ixc_ops->read16(cis->client, 0x040A, &y_start);

#ifdef TEST_SHIFT_CROP
	x_start += crop[test_crop_num][0];
	y_start += crop[test_crop_num][1];

	if (test_crop_num == 9)
		test_crop_num = 0;
	else
		test_crop_num++;
#else
	x_start += delta_x;
	y_start += delta_y;
#endif

	ret = cis->ixc_ops->write16(cis->client, 0x0408, x_start);
	ret = cis->ixc_ops->write16(cis->client, 0x040A, y_start);

	info("[%s] delta_x[%d], delta_y[%d] x_start(%d), y_start(%d)\n",
		__func__, delta_x, delta_y, x_start, y_start);

	start_pos_infos[device->cfg->mode].x_start = x_start;
	start_pos_infos[device->cfg->mode].y_start = y_start;

	cis->freeform_sensor_crop.enable = true;
	cis->freeform_sensor_crop.x_start = x_start;
	cis->freeform_sensor_crop.y_start = y_start;

	return ret;
}

int sensor_imx754_cis_set_crop_region(struct v4l2_subdev *subdev, u32 mode)
{
	int ret = 0;
	u16 x_start = 0;
	u16 y_start = 0;
	struct is_cis *cis = sensor_cis_get_cis(subdev);
	struct is_device_sensor *device = (struct is_device_sensor *)v4l2_get_subdev_hostdata(subdev);
	WARN_ON(!device);

	if (!(device->position == SENSOR_POSITION_REAR2 || cis->id == CAMERA_IMX754_10X_FLIP))
		return 0;

	x_start = start_pos_infos[mode].x_start;
	y_start = start_pos_infos[mode].y_start;

	info("[%d][%s] final lsc crop offset : mode(%d), x_start(%d), y_start(%d)\n",
		cis->id, __func__,
		mode, x_start, y_start);

	cis->freeform_sensor_crop.enable = true;
	cis->freeform_sensor_crop.x_start = x_start;
	cis->freeform_sensor_crop.y_start = y_start;

	return ret;
}
#endif

int sensor_imx754_cis_init_aeb(struct is_cis *cis)
{
	int ret = 0;
	struct sensor_imx754_private_data *priv = (struct sensor_imx754_private_data *)cis->sensor_info->priv;

	ret |= sensor_cis_write_registers(cis->subdev, priv->init_aeb);
	if (ret < 0)
		err("sensor_imx754_cis_init_aeb fail!! %d", ret);

	return ret;
}

int sensor_imx754_cis_check_aeb(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_device_sensor *device;
	struct is_cis *cis = sensor_cis_get_cis(subdev);

	device = (struct is_device_sensor *)v4l2_get_subdev_hostdata(subdev);
	WARN_ON(!device);

	if (cis->cis_data->stream_on == false
		|| is_sensor_g_ex_mode(device) != EX_AEB
		|| sensor_imx754_mode_groups[SENSOR_IMX754_MODE_AEB] == MODE_GROUP_NONE) {
		if (cis->cis_data->cur_hdr_mode == SENSOR_HDR_MODE_2AEB_2VC) {
			info("%s : current AEB status is enabled. need AEB disable\n", __func__);
			cis->cis_data->pre_hdr_mode = SENSOR_HDR_MODE_SINGLE;
			cis->cis_data->cur_hdr_mode = SENSOR_HDR_MODE_SINGLE;
			/* AEB disable */
			cis->ixc_ops->write8(cis->client, 0x0E00, 0x00);
			info("%s : disable AEB in not support mode\n", __func__);
		}
		return -1; //continue;
	}

	if (cis->cis_data->cur_hdr_mode == cis->cis_data->pre_hdr_mode) {
		if (cis->cis_data->cur_hdr_mode != SENSOR_HDR_MODE_2AEB_2VC)
			return -2; //continue; OFF->OFF
		else
			return 0; //return; ON->ON
	}

	if (cis->cis_data->cur_hdr_mode == SENSOR_HDR_MODE_2AEB_2VC) {
		info("%s : enable 2AEB 2VC\n", __func__);
		cis->ixc_ops->write8(cis->client, 0x0E00, 0x02);
	} else {
		info("%s : disable AEB\n", __func__);
		cis->ixc_ops->write8(cis->client, 0x0E00, 0x00);
	}

	cis->cis_data->pre_hdr_mode = cis->cis_data->cur_hdr_mode;

	info("%s : done\n", __func__);

	return ret;
}

void sensor_imx754_set_seamless_fast_transit(struct is_cis *cis, u8 value)
{
	int ret = 0;
	struct sensor_imx754_private_runtime *priv_runtime;

	priv_runtime = (struct sensor_imx754_private_runtime *)cis->priv_runtime;
	if (value)
		priv_runtime->seamless_fast_transit = true;
	else
		priv_runtime->seamless_fast_transit = false;

	ret = cis->ixc_ops->write8(cis->client, 0x3010, value);
	if (ret < 0) {
		err("sensor_imx754_set_registers fail!!");
	}
}

int sensor_imx754_cis_check_lownoise(struct is_cis *cis, u32 *next_mode)
{
	int ret = 0;
	u32 temp_mode = MODE_GROUP_NONE;
	cis_shared_data *cis_data = cis->cis_data;
	struct sensor_imx754_private_runtime *priv_runtime;

	priv_runtime = (struct sensor_imx754_private_runtime *)cis->priv_runtime;
	if (priv_runtime->seamless_fast_transit)
		sensor_imx754_set_seamless_fast_transit(cis, 0x00);

	if ((sensor_imx754_mode_groups[SENSOR_IMX754_MODE_LN2] == MODE_GROUP_NONE
		&& sensor_imx754_mode_groups[SENSOR_IMX754_MODE_LN4] == MODE_GROUP_NONE)) {
		return -1;
	}

	switch (cis_data->cur_lownoise_mode) {
	case IS_CIS_LN2:
		temp_mode = sensor_imx754_mode_groups[SENSOR_IMX754_MODE_LN2];
		break;
	case IS_CIS_LN4:
		temp_mode = sensor_imx754_mode_groups[SENSOR_IMX754_MODE_LN4];
		break;
	case IS_CIS_LNOFF:
		if (cis->cis_data->pre_lownoise_mode != IS_CIS_LNOFF)
			temp_mode = sensor_imx754_mode_groups[SENSOR_IMX754_MODE_DEFAULT];
		break;
	default:
		break;
	}

	if (temp_mode == MODE_GROUP_NONE)
		ret = -1;

	if (ret == 0)
		*next_mode = temp_mode;

	return ret;
}

int sensor_imx754_cis_update_seamless_mode(struct v4l2_subdev *subdev)
{
	int ret = 0;
	unsigned int mode = 0;
	unsigned int next_mode = 0;
	const struct sensor_cis_mode_info *next_mode_info;
	struct is_cis *cis = sensor_cis_get_cis(subdev);

	mode = cis->cis_data->sens_config_index_cur;

	next_mode = sensor_imx754_mode_groups[SENSOR_IMX754_MODE_DEFAULT];
	if (next_mode == MODE_GROUP_NONE) {
		err("mode group is none");
		return ret;
	}

	if (cis->cis_data->cur_pattern_mode != SENSOR_TEST_PATTERN_MODE_OFF) {
		dbg_sensor(1, "[%s] cur_pattern_mode (%d)", __func__, cis->cis_data->cur_pattern_mode);
		return ret;
	}

	IXC_MUTEX_LOCK(cis->ixc_lock);

	if (sensor_imx754_cis_check_aeb(subdev) == 0)
		goto EXIT;

	sensor_imx754_cis_check_lownoise(cis, &next_mode);

	if (mode == next_mode || next_mode == MODE_GROUP_NONE)
		goto EXIT;

	info("[%s] sensor mode(%d)\n", __func__, next_mode);
	next_mode_info = cis->sensor_info->mode_infos[next_mode];

	cis->ixc_ops->write8(cis->client, cis->reg_addr->group_param_hold, DATA_IMX754_GPH_HOLD);
	sensor_imx754_set_seamless_fast_transit(cis, 0x02);
	ret |= sensor_cis_write_registers(subdev, next_mode_info->setfile);
	if (ret < 0) {
		err("sensor_imx754_set_registers fail!!");
		goto EXIT;
	}
	cis->ixc_ops->write8(cis->client, cis->reg_addr->group_param_hold, DATA_IMX754_GPH_RELEASE);

	cis->cis_data->sens_config_index_pre = cis->cis_data->sens_config_index_cur;
	cis->cis_data->sens_config_index_cur = next_mode;

	CALL_CISOPS(cis, cis_data_calculation, subdev, next_mode);

	info("[%s] pre(%d)->cur(%d), LN[%d] AEB[%d]\n",
		__func__,
		cis->cis_data->sens_config_index_pre, cis->cis_data->sens_config_index_cur,
		cis->cis_data->cur_lownoise_mode,
		cis->cis_data->cur_hdr_mode);

	cis->cis_data->seamless_mode_changed = true;

EXIT:
	IXC_MUTEX_UNLOCK(cis->ixc_lock);

	return ret;
}

int sensor_imx754_cis_get_seamless_mode_info(struct v4l2_subdev *subdev)
{
	struct is_cis *cis = sensor_cis_get_cis(subdev);
	cis_shared_data *cis_data = cis->cis_data;
	int ret = 0, cnt = 0;
	u32 mode;

	if (sensor_imx754_mode_groups[SENSOR_IMX754_MODE_LN2] != MODE_GROUP_NONE) {
		cis_data->seamless_mode_info[cnt].mode = SENSOR_MODE_LN2;
		sensor_cis_get_mode_info(subdev, sensor_imx754_mode_groups[SENSOR_IMX754_MODE_LN2],
			&cis_data->seamless_mode_info[cnt]);
		cnt++;
	}

	if (sensor_imx754_mode_groups[SENSOR_IMX754_MODE_LN4] != MODE_GROUP_NONE) {
		cis_data->seamless_mode_info[cnt].mode = SENSOR_MODE_LN4;
		sensor_cis_get_mode_info(subdev, sensor_imx754_mode_groups[SENSOR_IMX754_MODE_LN4],
			&cis_data->seamless_mode_info[cnt]);
		cnt++;
	}

	mode = cis->cis_data->sens_config_index_cur;
	if (cis->sensor_info->mode_infos[mode]->aeb_support) {
		cis_data->seamless_mode_info[cnt].mode = SENSOR_MODE_AEB;
		sensor_cis_get_mode_info(subdev, mode, &cis_data->seamless_mode_info[cnt]);
		cnt++;
	}

	cis_data->seamless_mode_cnt = cnt;

	return ret;
}

int sensor_imx754_cis_mode_change(struct v4l2_subdev *subdev, u32 mode)
{
	int ret = 0;
	struct is_device_sensor *device;
	const struct sensor_cis_mode_info *mode_info;
	struct is_cis *cis = sensor_cis_get_cis(subdev);

	device = (struct is_device_sensor *)v4l2_get_subdev_hostdata(subdev);
	WARN_ON(!device);

	if (mode >= cis->sensor_info->mode_count) {
		err("invalid mode(%d)!!", mode);
		ret = -EINVAL;
		goto p_err;
	}

	sensor_imx754_cis_set_mode_group(mode);

	IXC_MUTEX_LOCK(cis->ixc_lock);

	info("[%s] sensor mode(%d)\n", __func__, mode);

	mode_info = cis->sensor_info->mode_infos[mode];

	ret |= sensor_cis_write_registers(subdev, mode_info->setfile);
	if (ret < 0) {
		err("[%d]sensor_imx754_set_registers fail!!", cis->id);
		goto p_err_i2c_unlock;
	}

	cis->cis_data->sens_config_index_pre = mode;

	/* AEB disable */
	ret |= cis->ixc_ops->write8(cis->client, 0x0E00, 0x00);

	info("[%s] mode[%d] LN[%d] AEB[%d]\n", __func__, mode,
		cis->cis_data->cur_lownoise_mode,
		cis->cis_data->cur_hdr_mode);
	/* EMB Header off */
	ret |= cis->ixc_ops->write8(cis->client, 0x3860, 0x00);

#ifdef CAMERA_REAR2_SENSOR_SHIFT_CROP
	if (device->position == SENSOR_POSITION_REAR2 || cis->id == CAMERA_IMX754_10X_FLIP) {
		ret = sensor_imx754_cis_update_crop_region(subdev);
		if (ret < 0) {
			err("sensor_imx754_cis_update_crop_region fail!!");
			goto p_err_i2c_unlock;
		}
	}
#endif

p_err_i2c_unlock:
	IXC_MUTEX_UNLOCK(cis->ixc_lock);
	sensor_imx754_cis_update_seamless_mode(subdev);
	sensor_imx754_cis_get_seamless_mode_info(subdev);
p_err:
	return ret;
}

int sensor_imx754_cis_stream_on(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_device_sensor *device;
	struct is_cis *cis = sensor_cis_get_cis(subdev);
	ktime_t st = ktime_get();

	device = (struct is_device_sensor *)v4l2_get_subdev_hostdata(subdev);
	WARN_ON(!device);

	dbg_sensor(1, "[MOD:D:%d] %s\n", cis->id, __func__);

	is_vendor_set_mipi_clock(device);

	IXC_MUTEX_LOCK(cis->ixc_lock);

	if (is_sensor_g_ex_mode(device) == EX_AEB)
		sensor_imx754_cis_init_aeb(cis);

	/* Sensor stream on */
	cis->ixc_ops->write8(cis->client, 0x0100, 0x01);

	info("[%d]%s\n", cis->id, __func__);

	cis->cis_data->stream_on = true;

	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %lldus\n", __func__, PABLO_KTIME_US_DELTA_NOW(st));

	IXC_MUTEX_UNLOCK(cis->ixc_lock);

	return ret;
}

int sensor_imx754_cis_set_test_pattern(struct v4l2_subdev *subdev, camera2_sensor_ctl_t *sensor_ctl)
{
	int ret = 0;
	struct is_cis *cis = sensor_cis_get_cis(subdev);

	dbg_sensor(1, "[MOD:D:%d] %s, cur_pattern_mode(%d), testPatternMode(%d)\n", cis->id, __func__,
		cis->cis_data->cur_pattern_mode, sensor_ctl->testPatternMode);

	if (cis->cis_data->cur_pattern_mode != sensor_ctl->testPatternMode) {
		if (sensor_ctl->testPatternMode == SENSOR_TEST_PATTERN_MODE_OFF) {
			info("%s: set DEFAULT pattern! (testpatternmode : %d)\n", __func__, sensor_ctl->testPatternMode);

			IXC_MUTEX_LOCK(cis->ixc_lock);
			cis->ixc_ops->write16(cis->client, 0x0600, 0x0000);
			IXC_MUTEX_UNLOCK(cis->ixc_lock);

			cis->cis_data->cur_pattern_mode = sensor_ctl->testPatternMode;
		} else if (sensor_ctl->testPatternMode == SENSOR_TEST_PATTERN_MODE_BLACK) {
			info("%s: set BLACK pattern! (testpatternmode :%d), Data : 0x(%x, %x, %x, %x)\n",
				__func__, sensor_ctl->testPatternMode,
				(unsigned short)sensor_ctl->testPatternData[0],
				(unsigned short)sensor_ctl->testPatternData[1],
				(unsigned short)sensor_ctl->testPatternData[2],
				(unsigned short)sensor_ctl->testPatternData[3]);

			IXC_MUTEX_LOCK(cis->ixc_lock);
			cis->ixc_ops->write16(cis->client, 0x0600, 0x0001);
			cis->ixc_ops->write8(cis->client, 0x3787, 0x02);
			IXC_MUTEX_UNLOCK(cis->ixc_lock);

			cis->cis_data->cur_pattern_mode = sensor_ctl->testPatternMode;
		}
	}

	return ret;
}

int sensor_imx754_cis_stream_off(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_cis *cis = sensor_cis_get_cis(subdev);
	ktime_t st = ktime_get();

	dbg_sensor(1, "[MOD:D:%d] %s\n", cis->id, __func__);

	IXC_MUTEX_LOCK(cis->ixc_lock);
	if (cis->cis_data->cur_hdr_mode == SENSOR_HDR_MODE_2AEB_2VC
		&& cis->cis_data->pre_hdr_mode == SENSOR_HDR_MODE_2AEB_2VC) {
		info("[%s] current AEB status is enabled. need AEB disable\n", __func__);
		cis->cis_data->pre_hdr_mode = cis->cis_data->cur_hdr_mode = SENSOR_HDR_MODE_SINGLE;

		/* AEB disable */
		cis->ixc_ops->write8(cis->client, 0x0E00, 0x00);
		info("[%s] disable AEB\n", __func__);
	}

	/* Sensor stream off */
	cis->ixc_ops->write8(cis->client, 0x0100, 0x00);

	info("[%d]%s\n", cis->id, __func__);

	cis->cis_data->stream_on = false;

	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %lldus\n", __func__, PABLO_KTIME_US_DELTA_NOW(st));

	IXC_MUTEX_UNLOCK(cis->ixc_lock);

	return ret;
}

static int sensor_imx754_cis_check_rev_on_init(struct v4l2_subdev *subdev)
{
	struct is_cis *cis = (struct is_cis *)v4l2_get_subdevdata(subdev);

	IXC_MUTEX_LOCK(cis->ixc_lock);

	if (cis->ixc_ops->write8(cis->client, 0xA02, 0x00)) {
		IXC_MUTEX_UNLOCK(cis->ixc_lock);
		return -EAGAIN;
	}

	if (cis->ixc_ops->write8(cis->client, 0xA00, 0x01)) {
		IXC_MUTEX_UNLOCK(cis->ixc_lock);
		return -EAGAIN;
	}

	IXC_MUTEX_UNLOCK(cis->ixc_lock);

	return sensor_cis_check_rev_on_init(subdev);
}

static struct is_cis_ops cis_ops = {
	.cis_init = sensor_imx754_cis_init,
	.cis_log_status = sensor_imx754_cis_log_status,
	.cis_group_param_hold = sensor_cis_set_group_param_hold,
	.cis_set_global_setting = sensor_imx754_cis_set_global_setting,
	.cis_init_state = sensor_cis_init_state,
	.cis_mode_change = sensor_imx754_cis_mode_change,
	.cis_stream_on = sensor_imx754_cis_stream_on,
	.cis_stream_off = sensor_imx754_cis_stream_off,
	.cis_data_calculation = sensor_cis_data_calculation,
	.cis_set_exposure_time = sensor_cis_set_exposure_time,
	.cis_get_min_exposure_time = sensor_cis_get_min_exposure_time,
	.cis_get_max_exposure_time = sensor_cis_get_max_exposure_time,
	.cis_adjust_frame_duration = sensor_cis_adjust_frame_duration,
	.cis_set_frame_duration = sensor_cis_set_frame_duration,
	.cis_set_frame_rate = sensor_cis_set_frame_rate,
	.cis_adjust_analog_gain = sensor_cis_adjust_analog_gain,
	.cis_set_analog_gain = sensor_cis_set_analog_gain,
	.cis_get_analog_gain = sensor_cis_get_analog_gain,
	.cis_get_min_analog_gain = sensor_cis_get_min_analog_gain,
	.cis_get_max_analog_gain = sensor_cis_get_max_analog_gain,
	.cis_calc_again_code = sensor_imx754_cis_calc_again_code, /* imx754 has own code */
	.cis_calc_again_permile = sensor_imx754_cis_calc_again_permile, /* imx754 has own code */
	.cis_set_digital_gain = sensor_cis_set_digital_gain,
	.cis_get_digital_gain = sensor_cis_get_digital_gain,
	.cis_get_min_digital_gain = sensor_cis_get_min_digital_gain,
	.cis_get_max_digital_gain = sensor_cis_get_max_digital_gain,
	.cis_calc_dgain_code = sensor_cis_calc_dgain_code,
	.cis_calc_dgain_permile = sensor_cis_calc_dgain_permile,
	.cis_compensate_gain_for_extremely_br = sensor_cis_compensate_gain_for_extremely_br,
	.cis_wait_streamoff = sensor_cis_wait_streamoff,
	.cis_wait_streamon = sensor_cis_wait_streamon,
	.cis_check_rev_on_init = sensor_imx754_cis_check_rev_on_init,
	.cis_set_initial_exposure = sensor_cis_set_initial_exposure,
	.cis_set_test_pattern = sensor_imx754_cis_set_test_pattern,
#ifdef CAMERA_REAR2_SENSOR_SHIFT_CROP
	.cis_set_crop_region = sensor_imx754_cis_set_crop_region,
#endif
	.cis_update_seamless_mode = sensor_imx754_cis_update_seamless_mode,
	.cis_update_seamless_state = sensor_cis_update_seamless_state,
	.cis_set_flip_mode = sensor_cis_set_flip_mode,
};

static int cis_imx754_probe_ixc(void *client, struct device_node *dnode,
	struct device *cdev, enum ixc_type type)
{
	int ret;
	u32 mclk_freq_khz;
	struct is_cis *cis;
	struct is_device_sensor_peri *sensor_peri;
	char const *setfile;

	ret = sensor_cis_probe(client, cdev, &sensor_peri, type);
	if (ret) {
		probe_info("%s: sensor_cis_probe ret(%d)\n", __func__, ret);
		return ret;
	}

	cis = &sensor_peri->cis;
	cis->ctrl_delay = N_PLUS_TWO_FRAME;
	cis->cis_ops = &cis_ops;
	cis->bayer_order = OTF_INPUT_ORDER_BAYER_GR_BG;
	cis->reg_addr = &sensor_imx754_reg_addr;
	cis->priv_runtime = kzalloc(sizeof(struct sensor_imx754_private_runtime), GFP_KERNEL);
	if (!cis->priv_runtime) {
		kfree(cis->cis_data);
		kfree(cis->subdev);
		probe_err("cis->priv_runtime is NULL");
		return -ENOMEM;
	}

	if (of_property_read_string(dnode, "setfile", &setfile)) {
		err("setfile index read fail(%d), take default setfile!!", ret);
		setfile = "default";
	}

	mclk_freq_khz = sensor_peri->module->pdata->mclk_freq_khz;

	if (mclk_freq_khz == 19200) {
		if (strcmp(setfile, "default") == 0 || strcmp(setfile, "setA") == 0) {
			probe_info("%s setfile_A mclk: 19.2MHz\n", __func__);
			cis->sensor_info = &sensor_imx754_info_A_19p2;
		} else if (strcmp(setfile, "setB") == 0) {
			probe_info("%s setfile_B mclk: 19.2MHz\n", __func__);
			cis->sensor_info = &sensor_imx754_info_B_19p2;
		} else {
			err("%s setfile index out of bound, take default (setfile_A)", __func__);
			cis->sensor_info = &sensor_imx754_info_A_19p2;
		}
	}

	is_vendor_set_mipi_mode(cis);

	if (type == I3C_TYPE)
		sensor_cis_complete_probe(sensor_peri);

	return ret;
}

static int cis_imx754_probe_i2c(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	struct device_node *dnode = client->dev.of_node;
	struct device *cdev = &(client->dev);
	int ret;

	ret = cis_imx754_probe_ixc((void *)client, dnode, cdev, I2C_TYPE);
	probe_info("%s done : %d\n", __func__, ret);

	return ret;
}

static int cis_imx754_probe_i3c(struct i3c_device *client)
{
	struct device_node *dnode = client->dev.of_node;
	struct device *cdev = &(client->dev);
	int ret;

	ret = cis_imx754_probe_ixc((void *)client, dnode, cdev, I3C_TYPE);
	probe_info("%s done : %d\n", __func__, ret);

	return ret;
}

static const struct of_device_id sensor_cis_imx754_match[] = {
	{
		.compatible = "samsung,exynos-is-cis-imx754",
	},
	{ /* sentinel */ },
};
MODULE_DEVICE_TABLE(of, sensor_cis_imx754_match);

static const struct i2c_device_id sensor_cis_imx754_idt_i2c[] = {
	{ SENSOR_NAME, 0 },
	{ /* sentinel */ },
};

/* PID[47:0] = 0x0360_0754_0000 */
static const struct i3c_device_id sensor_cis_imx754_idt_i3c[] = {
	I3C_DEVICE(0x0, 0x0754, NULL),
	{ /* sentinel */ },
};

static struct i2c_driver sensor_cis_imx754_driver_i2c = {
	.probe	= cis_imx754_probe_i2c,
	.driver = {
		.name	= SENSOR_NAME,
		.owner	= THIS_MODULE,
		.of_match_table = sensor_cis_imx754_match,
		.suppress_bind_attrs = true,
	},
	.id_table = sensor_cis_imx754_idt_i2c
};

static struct i3c_driver sensor_cis_imx754_driver_i3c = {
	.probe	= cis_imx754_probe_i3c,
	.driver = {
		.name	= SENSOR_NAME,
		.owner	= THIS_MODULE,
		.of_match_table = sensor_cis_imx754_match,
		.suppress_bind_attrs = true,
	},
	.id_table = sensor_cis_imx754_idt_i3c
};

#ifdef MODULE
module_driver(sensor_cis_imx754_driver_i3c, i3c_i2c_driver_register,
	i3c_i2c_driver_unregister, &sensor_cis_imx754_driver_i2c)
#else
static int __init sensor_cis_imx754_init(void)
{
	int ret;

	ret = i3c_i2c_driver_register(&sensor_cis_imx754_driver_i3c, &sensor_cis_imx754_driver_i2c);
	if (ret)
		err("failed to add %s i3c driver, %s i2c driver: %d",
			sensor_cis_imx754_driver_i3c.driver.name
			sensor_cis_imx754_driver_i2c.driver.name, ret);

	return ret;
}
late_initcall_sync(sensor_cis_imx754_init);
#endif

MODULE_LICENSE("GPL");
MODULE_SOFTDEP("pre: fimc-is");
