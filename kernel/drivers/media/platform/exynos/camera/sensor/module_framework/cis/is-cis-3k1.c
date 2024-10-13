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
#include "is-cis-3k1.h"
#include "is-vendor.h"
#include "is-cis-3k1-setA-19p2.h"
#include "is-helper-ixc.h"
#ifdef CAMERA_REAR2_SENSOR_SHIFT_CROP
#include "is-sec-define.h"
#include "is-vendor-private.h"
#endif

#define SENSOR_NAME "S5K3K1"

void sensor_3k1_cis_set_mode_group(u32 mode)
{
	sensor_3k1_mode_groups[SENSOR_3K1_MODE_DEFAULT] = mode;
	sensor_3k1_mode_groups[SENSOR_3K1_MODE_AEB] = MODE_GROUP_NONE;

	switch (mode) {
	case SENSOR_3K1_3648x2736_60FPS:
		sensor_3k1_mode_groups[SENSOR_3K1_MODE_AEB] = SENSOR_3K1_3648x2736_60FPS;
		break;
	}

	info("[%s] default(%d) aeb(%d)\n", __func__,
		sensor_3k1_mode_groups[SENSOR_3K1_MODE_DEFAULT],
		sensor_3k1_mode_groups[SENSOR_3K1_MODE_AEB]);
}

int sensor_3k1_cis_init(struct v4l2_subdev *subdev)
{
	int ret = 0;
	u32 idx;
	struct is_cis *cis = sensor_cis_get_cis(subdev);
	ktime_t st = ktime_get();

#if !defined(CONFIG_CAMERA_VENDOR_MCD)
	memset(cis->cis_data, 0, sizeof(cis_shared_data));

	ret = sensor_cis_check_rev(cis);
	if (ret < 0) {
		warn("sensor_3k1_check_rev is fail when cis init, ret(%d)", ret);
		return -EINVAL;
	}
#endif

	cis->cis_data->cur_width = cis->sensor_info->max_width;
	cis->cis_data->cur_height = cis->sensor_info->max_height;
	cis->cis_data->low_expo_start = 33000;

	for (idx = 0; idx < SENSOR_3K1_MODE_MAX; ++idx)
		start_pos_infos[idx].x_start = start_pos_infos[idx].y_start = 0;

	cis->cis_data->sens_config_index_pre = SENSOR_3K1_MODE_MAX;
	cis->cis_data->sens_config_index_cur = 0;
	CALL_CISOPS(cis, cis_data_calculation, subdev, cis->cis_data->sens_config_index_cur);

	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %lldus\n", __func__, PABLO_KTIME_US_DELTA_NOW(st));

	return ret;
}

static const struct is_cis_log log_3k1[] = {
	{I2C_READ, 16, 0x0000, 0, "model_id"},
	{I2C_READ, 16, 0x0002, 0, "revision_number"},
	{I2C_READ, 8, 0x0005, 0, "frame_count"},
	{I2C_READ, 16, 0x0100, 0, "0x0100"},
	{I2C_READ, 8, 0x0118, 0, "0x0118"},
	{I2C_READ, 16, 0x0202, 0, "0x0202"},
	{I2C_READ, 16, 0x0204, 0, "0x0204"},
	{I2C_READ, 16, 0x020E, 0, "0x020E"},
	{I2C_READ, 16, 0x0340, 0, "0x0340"},
	{I2C_READ, 16, 0x0702, 0, "0x0702"},
	{I2C_READ, 16, 0x0704, 0, "0x0704"},
};

int sensor_3k1_cis_log_status(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_cis *cis = sensor_cis_get_cis(subdev);

	sensor_cis_log_status(cis, log_3k1, ARRAY_SIZE(log_3k1), (char *)__func__);

	return ret;
}

int sensor_3k1_cis_init_aeb(struct is_cis *cis)
{
	int ret = 0;
	struct sensor_3k1_private_data *priv = (struct sensor_3k1_private_data *)cis->sensor_info->priv;

	ret |= sensor_cis_write_registers(cis->subdev, priv->init_aeb);
	if (ret < 0){
		err("sensor_3k1_cis_init_aeb fail!! %d", ret);
		return ret;
	}

	info("[%s] init aeb done\n", __func__);
	return ret;
}

int sensor_3k1_cis_set_global_setting(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_cis *cis = sensor_cis_get_cis(subdev);
	struct sensor_3k1_private_data *priv = (struct sensor_3k1_private_data *)cis->sensor_info->priv;

	/* setfile global setting is at camera entrance */
	info("[%d][%s] global setting enter\n", cis->id, __func__);
	ret = sensor_cis_write_registers_locked(subdev, priv->global);
	if (ret < 0) {
		err("[%d]sensor_3k1_set_registers fail!!", cis->id);
		return ret;
	}
	info("[%d][%s] global setting done\n", cis->id, __func__);

	return ret;
}

#ifdef CAMERA_REAR2_SENSOR_SHIFT_CROP
//#define TEST_SHIFT_CROP

#ifdef TEST_SHIFT_CROP
int test_crop_num = 0;
#endif

#if defined(CAMERA_FULL_SIZE_MULTICAL)
#define LIM(a, l, u) MIN(MAX(a, l), (u))

s64 hex2float_kernel(unsigned int hex_data)
{
	const s64 scale = 10000;
	unsigned int s, e, m;
	s64 res;

	s = hex_data >> 31;
	e = (hex_data >> 23) & 0xff;
	m = hex_data & 0x7fffff;
	res = (e >= 150) ? ((scale * (8388608 + m)) << (e - 150)) : ((scale * (8388608 + m)) >> (150 - e));
	if (s == 1)
		res *= -1;

	return res/scale;
}

void sensor_3k1_cis_get_crop_shift_data(struct v4l2_subdev *subdev, struct crop_shift_info *data)
{
	struct is_cis *cis = sensor_cis_get_cis(subdev);
	struct sensor_3k1_private_runtime *priv_runtime = (struct sensor_3k1_private_runtime *)cis->priv_runtime;

	data->crop_shift_x = priv_runtime->shift_info.crop_shift_x;
	data->crop_shift_y = priv_runtime->shift_info.crop_shift_y;
	data->crop_shift_sign = priv_runtime->shift_info.crop_shift_sign;
}

int sensor_3k1_cis_update_crop_region(struct v4l2_subdev *subdev)
{
	int ret = 0;
	long efs_size = 0;
	int copy_size = 0;
	int lim = 0;
	int hf = 0, vf = 0;
	int hm = 0, vm = 0;
	int sign_xy = 0;
	s16 px = 0, py = 0;
	s16 px3 = 0, py3 = 0;
	s16 px4 = 0, py4 = 0;
	u16 x_start = 0, y_start = 0;
	u16 x_end = 0, y_end = 0;
	int sensor_crop_x_unit_pix = 0;
	int sensor_crop_y_unit_pix = 0;
	char *buf = NULL;
	struct is_cis *cis = sensor_cis_get_cis(subdev);
	struct is_core *core;
	struct device *dev;
	struct is_vendor_private *vendor_priv;
	struct is_device_sensor *device;
	struct sensor_3k1_private_runtime *priv_runtime = (struct sensor_3k1_private_runtime *)cis->priv_runtime;

	core = is_get_is_core();
	vendor_priv = core->vendor.private_data;
	device = (struct is_device_sensor *)v4l2_get_subdev_hostdata(subdev);
	dev = &core->pdev->dev;

	if (device->cfg->mode == SENSOR_3K1_1824x1368_120FPS
		|| device->cfg->mode == SENSOR_3K1_1824x1368_30FPS) {
		info("[%s] skip crop shift in binning & fast ae sensor mode\n", __func__);
		return 0;
	}

	buf = vmalloc(MAX_MULTICAL_DATA_LENGTH);
	if (!buf) {
		err("failed to allocate memory");
		ret = -ENOMEM;
		goto p_err;
	}

	is_vendor_get_dualcal_param_from_file(dev, &buf, SENSOR_POSITION_REAR2, &copy_size);
	efs_size = vendor_priv->tilt_cal_tele_efs_size;

	if (efs_size) {
		/* multi cal output file */
		px = *((s16 *)&vendor_priv->tilt_cal_tele_efs_data[CROP_SHIFT_ADDR_X]);
		py = *((s16 *)&vendor_priv->tilt_cal_tele_efs_data[CROP_SHIFT_ADDR_Y]);
		/* multi cal input file */
		sensor_crop_x_unit_pix = *((int *)&buf[CROP_SHIFT_UNIT_PIX_X]);
		sensor_crop_y_unit_pix = *((int *)&buf[CROP_SHIFT_UNIT_PIX_Y]);
		sign_xy = *((int *)&buf[CROP_SHIFT_FLIP]);
		sensor_crop_x_unit_pix = (int)hex2float_kernel(sensor_crop_x_unit_pix);
		sensor_crop_y_unit_pix = (int)hex2float_kernel(sensor_crop_y_unit_pix);
		sign_xy = (int)hex2float_kernel(sign_xy);
		/* etc */
		hf = MULTICAL_FULL_SIZE_W;
		vf = MULTICAL_FULL_SIZE_H;
		hm = cis->cis_data->cur_width;
		vm = cis->cis_data->cur_height;

		if (!(((hf >= hm && vf > vm) || (hf > hm &&  vf >= vm))
			&& (sign_xy >= 0 && sign_xy <= 3))) {
			px3 = 0;
			py3 = 0;
			px4 = 0;
			py4 = 0;
		} else {
			if (px != 0 || py != 0) {
				lim = (((hf-hm) / 2) / sensor_crop_x_unit_pix) * sensor_crop_x_unit_pix;
				px3 = LIM(px, -lim, lim);
				lim = (((vf-vm) / 2) / sensor_crop_y_unit_pix) * sensor_crop_y_unit_pix;
				py3 = LIM(py, -lim, lim);

				px4 = ((sign_xy == 1 ||  sign_xy == 3) ? -1 : 1) * px3;
				py4 = ((sign_xy == 2 ||  sign_xy == 3) ? -1 : 1) * py3;
			} else {
				px3 = 0;
				py3 = 0;
				px4 = 0;
				py4 = 0;
			}
		}

		priv_runtime->shift_info.crop_shift_x = px4;
		priv_runtime->shift_info.crop_shift_y = py4;
		priv_runtime->shift_info.crop_shift_sign = sign_xy;
	}

	ret = cis->ixc_ops->read16(cis->client, 0x0344, &x_start);
	ret |= cis->ixc_ops->read16(cis->client, 0x0346, &y_start);
	ret |= cis->ixc_ops->read16(cis->client, 0x0348, &x_end);
	ret |= cis->ixc_ops->read16(cis->client, 0x034A, &y_end);

	x_start += px3;
	y_start += py3;
	x_end += px3;
	y_end += py3;

	ret |= cis->ixc_ops->write16(cis->client, 0x0344, x_start);
	ret |= cis->ixc_ops->write16(cis->client, 0x0346, y_start);
	ret |= cis->ixc_ops->write16(cis->client, 0x0348, x_end);
	ret |= cis->ixc_ops->write16(cis->client, 0x034A, y_end);

	info("[%s] delta_x[%d], delta_y[%d] x_start(%d), y_start(%d)\n",
		__func__, px3, py3, x_start, y_start);

	start_pos_infos[device->cfg->mode].x_start = x_start;
	start_pos_infos[device->cfg->mode].y_start = y_start;

	cis->freeform_sensor_crop.enable = true;
	cis->freeform_sensor_crop.x_start = x_start;
	cis->freeform_sensor_crop.y_start = y_start;

p_err:
	if (buf)
		vfree(buf);

	return ret;
}
#else
int sensor_3k1_cis_update_crop_region(struct v4l2_subdev *subdev)
{
	int ret = 0;
	u16 x_start = 0;
	u16 y_start = 0;
	u16 x_end = 0;
	u16 y_end = 0;
	s16 delta_x, delta_y;
	long efs_size = 0;
	s16 sign_x, sign_y;
	s16 temp_delta = 0;
	struct is_device_sensor *device;
	struct is_core *core;
	struct is_vendor_private *vendor_priv;
	struct is_cis *cis = sensor_cis_get_cis(subdev);

#ifdef TEST_SHIFT_CROP
	int16_t crop[10][2] = {
		{0,     0}, // default ROI {88, 72} Set the maximum value to 64. But possible to 72.
		{-88, -72}, {0,  -72}, {88, -72}, // {0,  0}, {88,  0}, {176,  0}
		{-88,   0}, {0,    0}, {88,   0}, // {0, 72}, {88, 72}, {176, 72}
		{-88,  72}, {0,   72}, {88,  72}, // {0,144}, {88,144}, {176,144}
	};
#endif

	device = (struct is_device_sensor *)v4l2_get_subdev_hostdata(subdev);
	WARN_ON(!device);

	if (device->cfg->mode == SENSOR_3K1_1824x1368_120FPS
		|| device->cfg->mode == SENSOR_3K1_1824x1368_30FPS) {
		info("[%s] skip crop shift in binning & fast ae sensor mode\n", __func__);
		return 0;
	}

	core = is_get_is_core();
	vendor_priv = core->vendor.private_data;

	efs_size = vendor_priv->tilt_cal_tele_efs_size;
	delta_x = *((s16 *)&vendor_priv->tilt_cal_tele_efs_data[CROP_SHIFT_ADDR_X]);
	delta_y = *((s16 *)&vendor_priv->tilt_cal_tele_efs_data[CROP_SHIFT_ADDR_Y]);

	if (efs_size == 0) {
		delta_x = 0;
		delta_y = 0;
	}

#ifdef CAMERA_3K1_MIRROR_FLIP
	sign_x = -1;
	sign_y = -1;
#else
	sign_x = 1;
	sign_y = 1;
#endif

	info("[%s] efs_size[%ld], delta_x[%d], delta_y[%d], sign_x(%d), sign_y(%d)\n",
		__func__, efs_size, delta_x, delta_y, sign_x, sign_y);

	if (delta_x > CROP_SHIFT_DELTA_MAX_X)
		delta_x = CROP_SHIFT_DELTA_MAX_X;
	else if (delta_x < CROP_SHIFT_DELTA_MIN_X)
		delta_x = CROP_SHIFT_DELTA_MIN_X;

	if (delta_y > CROP_SHIFT_DELTA_MAX_Y)
		delta_y = CROP_SHIFT_DELTA_MAX_Y;
	else if (delta_y < CROP_SHIFT_DELTA_MIN_Y)
		delta_y = CROP_SHIFT_DELTA_MIN_Y;

	if (delta_x % CROP_SHIFT_DELTA_ALIGN_X) {
		temp_delta = delta_x / CROP_SHIFT_DELTA_ALIGN_X;
		delta_x = temp_delta * CROP_SHIFT_DELTA_ALIGN_X;
	}

	if (delta_y % CROP_SHIFT_DELTA_ALIGN_Y) {
		temp_delta = delta_y / CROP_SHIFT_DELTA_ALIGN_Y;
		delta_y = temp_delta * CROP_SHIFT_DELTA_ALIGN_Y;
	}

	ret |= cis->ixc_ops->read16(cis->client, 0x0344, &x_start);
	ret |= cis->ixc_ops->read16(cis->client, 0x0346, &y_start);
	ret |= cis->ixc_ops->read16(cis->client, 0x0348, &x_end);
	ret |= cis->ixc_ops->read16(cis->client, 0x034A, &y_end);

#ifdef TEST_SHIFT_CROP
	x_start += crop[test_crop_num][0];
	y_start += crop[test_crop_num][1];
	x_end += crop[test_crop_num][0];
	y_end += crop[test_crop_num][1];

	if (test_crop_num == 9)
		test_crop_num = 0;
	else
		test_crop_num++;
#else
	x_start += delta_x;
	y_start += delta_y;
	x_end += delta_x;
	y_end += delta_y;
#endif

	ret |= cis->ixc_ops->write16(cis->client, 0x0344, x_start);
	ret |= cis->ixc_ops->write16(cis->client, 0x0346, y_start);
	ret |= cis->ixc_ops->write16(cis->client, 0x0348, x_end);
	ret |= cis->ixc_ops->write16(cis->client, 0x034A, y_end);

	info("[%s] delta_x[%d], delta_y[%d] x_start(%d), y_start(%d)\n",
		__func__, delta_x, delta_y, x_start, y_start);

	/* mirror calibration */
	x_start = x_start - delta_x + (delta_x * sign_x);

	/* flip calibration */
	y_start = y_start - delta_y + (delta_y * sign_y);

	if (x_start < 0 || y_start < 0) {
		err("freeform sensor crop start coordinate is negative");
		return -EINVAL;
	}

	info("[%s] after mirror/flip cal : delta_x[%d], delta_y[%d] x_start(%d), y_start(%d)\n",
		__func__, delta_x, delta_y, x_start, y_start);

	start_pos_infos[device->cfg->mode].x_start = x_start;
	start_pos_infos[device->cfg->mode].y_start = y_start;

	cis->freeform_sensor_crop.enable = true;
	cis->freeform_sensor_crop.x_start = x_start;
	cis->freeform_sensor_crop.y_start = y_start;

	return ret;
}
#endif

int sensor_3k1_cis_set_crop_region(struct v4l2_subdev *subdev, u32 mode)
{
	int ret = 0;
	u16 x_start = 0;
	u16 y_start = 0;
	struct is_cis *cis = sensor_cis_get_cis(subdev);
	struct is_device_sensor *device = (struct is_device_sensor *)v4l2_get_subdev_hostdata(subdev);

	if (!(device->position == SENSOR_POSITION_REAR2))
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

bool sensor_3k1_cis_check_aeb(struct v4l2_subdev *subdev)
{
	struct is_device_sensor *device;
	struct is_cis *cis = sensor_cis_get_cis(subdev);

	device = (struct is_device_sensor *)v4l2_get_subdev_hostdata(subdev);
	WARN_ON(!device);

	if (cis->cis_data->stream_on == false
		|| !is_sensor_check_aeb_mode(device)
		|| sensor_3k1_mode_groups[SENSOR_3K1_MODE_AEB] == MODE_GROUP_NONE) {
		if (cis->cis_data->cur_hdr_mode == SENSOR_HDR_MODE_2AEB_2VC) {
			info("[%s] current AEB status is enabled. need AEB disable\n", __func__);
			cis->cis_data->pre_hdr_mode = SENSOR_HDR_MODE_SINGLE;
			cis->cis_data->cur_hdr_mode = SENSOR_HDR_MODE_SINGLE;
			/* AEB disable */
			cis->ixc_ops->write16(cis->client, 0xFCFC, 0x4000);
			cis->ixc_ops->write16(cis->client, 0x0E00, 0x0003);
			info("[%s] disable AEB in not support mode\n", __func__);
		}
		return false; //continue;
	}

	if (cis->cis_data->cur_hdr_mode == cis->cis_data->pre_hdr_mode)
		return false;

	if (cis->cis_data->cur_hdr_mode == SENSOR_HDR_MODE_2AEB_2VC) {
		info("[%s] enable 2AEB 2VC\n", __func__);
		cis->ixc_ops->write16(cis->client, 0x0E00, 0x0203);
	} else {
		info("[%s] disable AEB\n", __func__);
		cis->ixc_ops->write16(cis->client, 0x0E00, 0x0003);
	}

	return true;
}

int sensor_3k1_cis_update_seamless_mode(struct v4l2_subdev *subdev)
{
	int ret = 0;
	unsigned int mode = 0;
	unsigned int next_mode = 0;
	struct is_cis *cis = sensor_cis_get_cis(subdev);

	mode = cis->cis_data->sens_config_index_cur;

	next_mode = sensor_3k1_mode_groups[SENSOR_3K1_MODE_DEFAULT];
	if (next_mode == MODE_GROUP_NONE) {
		err("mode group is none");
		return -1;
	}

	if (cis->cis_data->cur_pattern_mode != SENSOR_TEST_PATTERN_MODE_OFF) {
		dbg_sensor(1, "[%s] cur_pattern_mode (%d)", __func__, cis->cis_data->cur_pattern_mode);
		return ret;
	}

	IXC_MUTEX_LOCK(cis->ixc_lock);

	if (sensor_3k1_cis_check_aeb(subdev) == true) {
		cis->cis_data->pre_hdr_mode = cis->cis_data->cur_hdr_mode;
		cis->cis_data->seamless_mode_changed = true;

		info("[%s][%d] pre(%d)->cur(%d), AEB[%d]\n",
			__func__, cis->cis_data->sen_vsync_count,
			cis->cis_data->sens_config_index_pre, cis->cis_data->sens_config_index_cur,
			cis->cis_data->cur_hdr_mode);
	}

	IXC_MUTEX_UNLOCK(cis->ixc_lock);

	return ret;
}

int sensor_3k1_cis_get_seamless_mode_info(struct v4l2_subdev *subdev)
{
	struct is_cis *cis = sensor_cis_get_cis(subdev);
	cis_shared_data *cis_data = cis->cis_data;
	int ret = 0, cnt = 0;
	u32 mode;

	mode = cis_data->sens_config_index_cur;
	if (cis->sensor_info->mode_infos[mode]->aeb_support) {
		cis_data->seamless_mode_info[cnt].mode = SENSOR_MODE_AEB;
		sensor_cis_get_mode_info(subdev, mode, &cis_data->seamless_mode_info[cnt]);
		cnt++;
	}

	cis_data->seamless_mode_cnt = cnt;

	return ret;
}

int sensor_3k1_cis_mode_change(struct v4l2_subdev *subdev, u32 mode)
{
	int ret = 0;
	u32 ex_mode;
	struct is_device_sensor *device;
	const struct sensor_cis_mode_info *mode_info;
	struct is_cis *cis = sensor_cis_get_cis(subdev);

	device = (struct is_device_sensor *)v4l2_get_subdev_hostdata(subdev);
	WARN_ON(!device);

	if (mode >= cis->sensor_info->mode_count) {
		err("invalid mode(%d)!!", mode);
		return -EINVAL;
	}

	IXC_MUTEX_LOCK(cis->ixc_lock);

	ex_mode = is_sensor_g_ex_mode(device);

	sensor_3k1_cis_set_mode_group(mode);
	sensor_3k1_cis_get_seamless_mode_info(subdev);

	mode_info = cis->sensor_info->mode_infos[mode];

	ret = sensor_cis_write_registers(subdev, mode_info->setfile);
	if (ret < 0) {
		err("[%d]sensor_3k1_set_registers fail!!", cis->id);
		goto EXIT;
	}

	cis->cis_data->sens_config_index_pre = mode;
	/* AEB disable */
	ret |= cis->ixc_ops->write16(cis->client, 0xFCFC, 0x4000);
	ret |= cis->ixc_ops->write16(cis->client, 0x0E00, 0x0003);

	info("[%s] mode[%d] AEB[%d]\n", __func__, mode, cis->cis_data->cur_hdr_mode);

	/* EMB Header off */
	ret |= cis->ixc_ops->write8(cis->client, 0x0118, 0x00);

#if defined(CAMERA_REAR2_SENSOR_SHIFT_CROP)
	if (device->position == SENSOR_POSITION_REAR2) {
		ret = sensor_3k1_cis_update_crop_region(subdev);
		if (ret < 0) {
			err("sensor_3k1_cis_update_crop_region fail!!");
			goto EXIT;
		}
	}
#endif

EXIT:
	IXC_MUTEX_UNLOCK(cis->ixc_lock);

	return ret;
}

int sensor_3k1_cis_stream_on(struct v4l2_subdev *subdev)
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

	/* AEB init */
	if (is_sensor_check_aeb_mode(device))
		sensor_3k1_cis_init_aeb(cis);

	cis->ixc_ops->write16(cis->client, 0x6000, 0x0085);

	/* Sensor stream on */
	cis->ixc_ops->write8(cis->client, 0x0100, 0x01);

	IXC_MUTEX_UNLOCK(cis->ixc_lock);

	info("[%d]%s\n", cis->id, __func__);

	cis->cis_data->stream_on = true;

	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %lldus\n", __func__, PABLO_KTIME_US_DELTA_NOW(st));


	return ret;
}

int sensor_3k1_cis_stream_off(struct v4l2_subdev *subdev)
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
		cis->ixc_ops->write16(cis->client, 0x0E00, 0x0003);
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

static struct is_cis_ops cis_ops = {
	.cis_init = sensor_3k1_cis_init,
	.cis_log_status = sensor_3k1_cis_log_status,
	.cis_group_param_hold = sensor_cis_set_group_param_hold,
	.cis_set_global_setting = sensor_3k1_cis_set_global_setting,
	.cis_init_state = sensor_cis_init_state,
	.cis_mode_change = sensor_3k1_cis_mode_change,
	.cis_stream_on = sensor_3k1_cis_stream_on,
	.cis_stream_off = sensor_3k1_cis_stream_off,
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
	.cis_calc_again_code = sensor_cis_calc_again_code,
	.cis_calc_again_permile = sensor_cis_calc_again_permile,
	.cis_set_digital_gain = sensor_cis_set_digital_gain,
	.cis_get_digital_gain = sensor_cis_get_digital_gain,
	.cis_get_min_digital_gain = sensor_cis_get_min_digital_gain,
	.cis_get_max_digital_gain = sensor_cis_get_max_digital_gain,
	.cis_calc_dgain_code = sensor_cis_calc_dgain_code,
	.cis_calc_dgain_permile = sensor_cis_calc_dgain_permile,
	.cis_compensate_gain_for_extremely_br = sensor_cis_compensate_gain_for_extremely_br,
	.cis_wait_streamoff = sensor_cis_wait_streamoff,
	.cis_wait_streamon = sensor_cis_wait_streamon,
	.cis_check_rev_on_init = sensor_cis_check_rev_on_init,
	.cis_set_initial_exposure = sensor_cis_set_initial_exposure,
	.cis_set_test_pattern = sensor_cis_set_test_pattern,
#ifdef CAMERA_REAR2_SENSOR_SHIFT_CROP
	.cis_set_crop_region = sensor_3k1_cis_set_crop_region,
	.cis_get_crop_shift_data = sensor_3k1_cis_get_crop_shift_data,
#endif
	.cis_update_seamless_mode = sensor_3k1_cis_update_seamless_mode,
	.cis_update_seamless_state = sensor_cis_update_seamless_state,
	.cis_wait_seamless_update_delay = sensor_cis_wait_seamless_update_delay,
	.cis_set_flip_mode = sensor_cis_set_flip_mode,
};

static int cis_3k1_probe_i2c(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	int ret;
	u32 mclk_freq_khz;
	struct is_cis *cis;
	struct is_device_sensor_peri *sensor_peri;
	char const *setfile;
	struct device_node *dnode = client->dev.of_node;

	ret = sensor_cis_probe(client, &(client->dev), &sensor_peri, I2C_TYPE);
	if (ret) {
		probe_info("%s: sensor_cis_probe ret(%d)\n", __func__, ret);
		return ret;
	}

	cis = &sensor_peri->cis;
	cis->ctrl_delay = N_PLUS_TWO_FRAME;
	cis->cis_ops = &cis_ops;
	cis->bayer_order = OTF_INPUT_ORDER_BAYER_GB_RG;
	cis->reg_addr = &sensor_3k1_reg_addr;
	cis->priv_runtime = kzalloc(sizeof(struct sensor_3k1_private_runtime), GFP_KERNEL);
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
			cis->sensor_info = &sensor_3k1_info_A_19p2;
		}
	}

	is_vendor_set_mipi_mode(cis);

	probe_info("%s done\n", __func__);

	return ret;
}

static const struct of_device_id sensor_cis_3k1_match[] = {
	{
		.compatible = "samsung,exynos-is-cis-3k1",
	},
	{},
};
MODULE_DEVICE_TABLE(of, sensor_cis_3k1_match);

static const struct i2c_device_id sensor_cis_3k1_idt[] = {
	{ SENSOR_NAME, 0 },
	{},
};

static struct i2c_driver sensor_cis_3k1_driver = {
	.probe	= cis_3k1_probe_i2c,
	.driver = {
		.name	= SENSOR_NAME,
		.owner	= THIS_MODULE,
		.of_match_table = sensor_cis_3k1_match,
		.suppress_bind_attrs = true,
	},
	.id_table = sensor_cis_3k1_idt
};

#ifdef MODULE
builtin_i2c_driver(sensor_cis_3k1_driver);
#else
static int __init sensor_cis_3k1_init(void)
{
	int ret;

	ret = i2c_add_driver(&sensor_cis_3k1_driver);
	if (ret)
		err("failed to add %s driver: %d\n",
			sensor_cis_3k1_driver.driver.name, ret);

	return ret;
}
late_initcall_sync(sensor_cis_3k1_init);
#endif

MODULE_LICENSE("GPL");
MODULE_SOFTDEP("pre: fimc-is");
