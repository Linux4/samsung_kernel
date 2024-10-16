/*
 * Samsung Exynos5 SoC series Sensor driver
 *
 *
 * Copyright (c) 2024 Samsung Electronics Co., Ltd
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
#include <linux/videodev2_exynos_camera.h>
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
#include "is-cis-gc13a0.h"
#include "is-cis-gc13a0-setA.h"

#include "is-helper-ixc.h"
#include "is-vender-specific.h"

#define SENSOR_NAME "GC13A0"

#define POLL_TIME_MS (1)
#define POLL_TIME_US (2000)
#define STREAM_OFF_POLL_TIME_MS (500)
#define STREAM_ON_POLL_TIME_MS (500)
#define STREAM_OFF_WAIT_TIME 250

#define MAX_WAIT_STREAM_ON_CNT (100)

#define GC13A0_ADDR_FRAME_COUNT                                 0x0146
#define GC13A0_ADDR_MIPI_CLK_STATUS                             0x0180
#define GC13A0_BIT_MIPI_CLK_ENABLE                              (6)

static const u32 *sensor_gc13a0_global;
static u32 sensor_gc13a0_global_size;
static const u32 **sensor_gc13a0_setfiles;
static const u32 *sensor_gc13a0_setfile_sizes;
static const struct sensor_pll_info_compact **sensor_gc13a0_pllinfos;
static u32 sensor_gc13a0_max_setfile_num;
static const u32 *sensor_gc13a0_fsync_master;
static u32 sensor_gc13a0_fsync_master_size;
static const u32 *sensor_gc13a0_fsync_slave;
static u32 sensor_gc13a0_fsync_slave_size;


/*************************************************
 *  [GC13A0 Analog / Digital gain formular]
 *
 *  Analog / Digital Gain = (Reg value) / 1024
 *
 *  Analog / Digital Gain Range = x1.0 to x16.0
 *
 *************************************************/

u32 sensor_gc13a0_cis_calc_gain_code(u32 permile)
{
	return ((permile * 1024) / 1000);
}

u32 sensor_gc13a0_cis_calc_gain_permile(u32 code)
{
	return ((code * 1000 / 1024));
}

static bool sensor_gc13a0_check_master_stream_off(struct is_core *core)
{
	if (test_bit(IS_SENSOR_OPEN, &(core->sensor[0].state)) &&	/* Dual mode and master stream off */
			!test_bit(IS_SENSOR_FRONT_START, &(core->sensor[0].state)))
		return true;
	else
		return false;
}

static void sensor_gc13a0_cis_data_calculation(const struct sensor_pll_info_compact *pll_info, cis_shared_data *cis_data)
{
	u32 vt_pix_clk_hz = 0;
	u32 frame_rate = 0, max_fps = 0, frame_valid_us = 0;

	FIMC_BUG_VOID(!pll_info);

	/* 1. get pclk value from pll info */
	vt_pix_clk_hz = pll_info->pclk;

	/* 2. the time of processing one frame calculation (us) */
	cis_data->min_frame_us_time = ((pll_info->frame_length_lines * pll_info->line_length_pck)
								/ (vt_pix_clk_hz / (1000 * 1000)));
	cis_data->cur_frame_us_time = cis_data->min_frame_us_time;

	/* 3. FPS calculation */
	frame_rate = vt_pix_clk_hz / (pll_info->frame_length_lines * pll_info->line_length_pck);
	dbg_sensor(2, "frame_rate (%d) = vt_pix_clk_hz(%d) / "
		KERN_CONT "(pll_info->frame_length_lines(%d) * pll_info->line_length_pck(%d))\n",
		frame_rate, vt_pix_clk_hz, pll_info->frame_length_lines, pll_info->line_length_pck);

	/* calculate max fps */
	max_fps = (vt_pix_clk_hz * 10) / (pll_info->frame_length_lines * pll_info->line_length_pck);
	max_fps = (max_fps % 10 >= 5 ? frame_rate + 1 : frame_rate);

	cis_data->pclk = vt_pix_clk_hz;
	cis_data->max_fps = max_fps;
	cis_data->frame_length_lines = pll_info->frame_length_lines;
	cis_data->line_length_pck = pll_info->line_length_pck;
	cis_data->line_readOut_time = sensor_cis_do_div64((u64)cis_data->line_length_pck * (u64)(1000 * 1000 * 1000), cis_data->pclk);
	cis_data->rolling_shutter_skew = (cis_data->cur_height - 1) * cis_data->line_readOut_time;
	cis_data->stream_on = false;

	/* Frame valid time calcuration */
	frame_valid_us = sensor_cis_do_div64((u64)cis_data->cur_height * (u64)cis_data->line_length_pck * (u64)(1000 * 1000), cis_data->pclk);
	cis_data->frame_valid_us_time = (int)frame_valid_us;

	dbg_sensor(2, "%s\n", __func__);
	dbg_sensor(2, "Sensor size(%d x %d) setting: SUCCESS!\n", cis_data->cur_width, cis_data->cur_height);
	dbg_sensor(2, "Frame Valid(us): %d\n", frame_valid_us);
	dbg_sensor(2, "rolling_shutter_skew: %lld\n", cis_data->rolling_shutter_skew);

	dbg_sensor(2, "Fps: %d, max fps(%d)\n", frame_rate, cis_data->max_fps);
	dbg_sensor(2, "min_frame_time(%d us)\n", cis_data->min_frame_us_time);
	dbg_sensor(2, "Pixel rate(Mbps): %d\n", cis_data->pclk / 1000000);

	/* Frame period calculation */
	cis_data->frame_time = (cis_data->line_readOut_time * cis_data->cur_height / 1000);
	cis_data->rolling_shutter_skew = (cis_data->cur_height - 1) * cis_data->line_readOut_time;

	dbg_sensor(2, "[%s] frame_time(%d), rolling_shutter_skew(%lld)\n", __func__,
	cis_data->frame_time, cis_data->rolling_shutter_skew);

	/* Constant values */
	cis_data->min_fine_integration_time = SENSOR_GC13A0_FINE_INTEGRATION_TIME_MIN;
	cis_data->max_fine_integration_time = SENSOR_GC13A0_FINE_INTEGRATION_TIME_MAX;
	cis_data->min_coarse_integration_time = SENSOR_GC13A0_COARSE_INTEGRATION_TIME_MIN;
	cis_data->max_margin_coarse_integration_time = SENSOR_GC13A0_COARSE_INTEGRATION_TIME_MAX_MARGIN;
	info("%s: done", __func__);
}

static int sensor_gc13a0_wait_stream_off_status(cis_shared_data *cis_data)
{
	int ret = 0;
	u32 timeout = 0;

	FIMC_BUG(!cis_data);

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

int sensor_gc13a0_cis_set_exposure_time(struct v4l2_subdev *subdev, u16 multiple_exp)
{
	int ret = 0;
	struct is_cis *cis;
	struct i2c_client *client;
	cis_shared_data *cis_data;
	u16 frame_length_lines = 0;
	ktime_t st = ktime_get();

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);

	FIMC_BUG(!cis);
	FIMC_BUG(!cis->cis_data);

	client = cis->client;
	if (unlikely(!client)) {
		err("client is NULL");
		ret = -EINVAL;
		return ret;
	}

	cis_data = cis->cis_data;

	dbg_sensor(2, "[%s] multiple_exp %d\n", __func__, multiple_exp);

	if (multiple_exp > (cis_data->frame_length_lines - cis_data->max_margin_coarse_integration_time))
		frame_length_lines = multiple_exp + cis_data->max_margin_coarse_integration_time;
	else
		frame_length_lines = cis_data->frame_length_lines;

	I2C_MUTEX_LOCK(cis->i2c_lock);

	/* Frame length line */
	ret = cis->ixc_ops->write8(client, 0x0340, (frame_length_lines >> 8) & 0xff);
	if (ret < 0)
		goto p_err;
	ret = cis->ixc_ops->write8(client, 0x0341, (frame_length_lines & 0xff));
	if (ret < 0)
		goto p_err;

	/* Exposure */
	ret = cis->ixc_ops->write8(client, 0x0202, (multiple_exp >> 8) & 0xff);
	if (ret < 0)
		goto p_err;
	ret = cis->ixc_ops->write8(client, 0x0203, (multiple_exp & 0xff));
	if (ret < 0)
		goto p_err;

	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %ldus\n", __func__, PABLO_KTIME_US_DELTA_NOW(st));

p_err:
	I2C_MUTEX_UNLOCK(cis->i2c_lock);

	return ret;
}

int sensor_gc13a0_cis_set_analog_gain(struct v4l2_subdev *subdev, struct ae_param *again)
{
	int ret = 0;
	struct is_cis *cis;
	struct i2c_client *client;
	cis_shared_data *cis_data;

	u32 again_code = 0;
	ktime_t st = ktime_get();

	FIMC_BUG(!subdev);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);

	FIMC_BUG(!cis);
	FIMC_BUG(!cis->cis_data);

	client = cis->client;
	if (unlikely(!client)) {
		err("client is NULL");
		ret = -EINVAL;
		return ret;
	}

	cis_data = cis->cis_data;

	again_code = sensor_gc13a0_cis_calc_gain_code(again->long_val);

	if (again_code > cis_data->max_analog_gain[0]) {
		again_code = cis_data->max_analog_gain[0];
	} else if (again_code < cis_data->min_analog_gain[0]) {
		again_code = cis_data->min_analog_gain[0];
	}

	dbg_sensor(2, "[MOD:D:%d] %s, input_again = %d us, again_code(%#x)\n",
			cis->id, __func__, again->long_val, again_code);

	I2C_MUTEX_LOCK(cis->i2c_lock);
	ret = cis->ixc_ops->write8(client, 0x0204, (again_code >> 8) & 0xff);
	ret = cis->ixc_ops->write8(client, 0x0205, again_code & 0xff);
	if (ret < 0)
		goto p_err;

	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %ldus", __func__, PABLO_KTIME_US_DELTA_NOW(st));

p_err:
	I2C_MUTEX_UNLOCK(cis->i2c_lock);
	return ret;
}

int sensor_gc13a0_cis_set_digital_gain(struct v4l2_subdev *subdev, struct ae_param *dgain)
{
	int ret = 0;
	struct is_cis *cis;
	struct i2c_client *client;
	cis_shared_data *cis_data;

	u16 dgain_code = 0;
	ktime_t st = ktime_get();

	FIMC_BUG(!subdev);
	FIMC_BUG(!dgain);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);

	FIMC_BUG(!cis);
	FIMC_BUG(!cis->cis_data);

	client = cis->client;
	if (unlikely(!client)) {
		err("client is NULL");
		ret = -EINVAL;
		return ret;
	}

	cis_data = cis->cis_data;

	dgain_code = (u16)sensor_gc13a0_cis_calc_gain_code(dgain->long_val);

	if (dgain_code < cis->cis_data->min_digital_gain[0]) {
		dgain_code = cis->cis_data->min_digital_gain[0];
	}
	if (dgain_code > cis->cis_data->max_digital_gain[0]) {
		dgain_code = cis->cis_data->max_digital_gain[0];
	}

	dbg_sensor(2, "[MOD:D:%d] %s, input_dgain = %d us, dgain_code(%#x)\n",
			cis->id, __func__, dgain->long_val, dgain_code);

	I2C_MUTEX_LOCK(cis->i2c_lock);

	ret = cis->ixc_ops->write8(client, 0x020e, (dgain_code >> 8) & 0xff);
	ret = cis->ixc_ops->write8(client, 0x020f, dgain_code & 0xff);
	if (ret < 0)
		goto p_err;

	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %ldus", __func__, PABLO_KTIME_US_DELTA_NOW(st));

p_err:
	I2C_MUTEX_UNLOCK(cis->i2c_lock);

	return ret;
}

/* CIS OPS */
int sensor_gc13a0_cis_init(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_cis *cis;
	u32 setfile_index = 0;
	cis_setting_info setinfo;

#if USE_OTP_AWB_CAL_DATA
	struct i2c_client *client = NULL;
	u8 selected_page;
	u16 data16[4];
	u8 cal_map_ver[4];
	bool skip_cal_write = false;
#endif
	ktime_t st = ktime_get();

	setinfo.param = NULL;
	setinfo.return_value = 0;

	FIMC_BUG(!subdev);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);
	if (!cis) {
		err("cis is NULL");
		ret = -EINVAL;
		goto p_err;
	}

#if USE_OTP_AWB_CAL_DATA
	client = cis->client;
	if (unlikely(!client)) {
		err("client is NULL");
		ret = -EINVAL;
		goto p_err;
	}
#endif

	cis->cis_data->cur_width = SENSOR_GC13A0_MAX_WIDTH;
	cis->cis_data->cur_height = SENSOR_GC13A0_MAX_HEIGHT;
	cis->cis_data->low_expo_start = 33000;
	cis->need_mode_change = false;

	sensor_gc13a0_cis_data_calculation(sensor_gc13a0_pllinfos[setfile_index], cis->cis_data);

	setinfo.return_value = 0;
	CALL_CISOPS(cis, cis_get_min_exposure_time, subdev, &setinfo.return_value);
	dbg_sensor(2, "[%s] min exposure time : %d\n", __func__, setinfo.return_value);
	setinfo.return_value = 0;
	CALL_CISOPS(cis, cis_get_max_exposure_time, subdev, &setinfo.return_value);
	dbg_sensor(2, "[%s] max exposure time : %d\n", __func__, setinfo.return_value);
	setinfo.return_value = 0;
	CALL_CISOPS(cis, cis_get_min_analog_gain, subdev, &setinfo.return_value);
	dbg_sensor(2, "[%s] min again : %d\n", __func__, setinfo.return_value);
	setinfo.return_value = 0;
	CALL_CISOPS(cis, cis_get_max_analog_gain, subdev, &setinfo.return_value);
	dbg_sensor(2, "[%s] max again : %d\n", __func__, setinfo.return_value);
	setinfo.return_value = 0;
	CALL_CISOPS(cis, cis_get_min_digital_gain, subdev, &setinfo.return_value);
	dbg_sensor(2, "[%s] min dgain : %d\n", __func__, setinfo.return_value);
	setinfo.return_value = 0;
	CALL_CISOPS(cis, cis_get_max_digital_gain, subdev, &setinfo.return_value);
	dbg_sensor(2, "[%s] max dgain : %d\n", __func__, setinfo.return_value);

	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %ldus", __func__, PABLO_KTIME_US_DELTA_NOW(st));

p_err:
	return ret;
}

int sensor_gc13a0_cis_log_status(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_cis *cis;

	FIMC_BUG(!subdev);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);
	if (!cis) {
		err("cis is NULL");
		return -ENODEV;
	}

	I2C_MUTEX_LOCK(cis->i2c_lock);
	ret = sensor_cis_dump_registers(subdev, sensor_gc13a0_setfiles[0], sensor_gc13a0_setfile_sizes[0]);
	I2C_MUTEX_UNLOCK(cis->i2c_lock);

	return ret;
}

int sensor_gc13a0_cis_set_global_setting(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_cis *cis = NULL;

	FIMC_BUG(!subdev);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);
	FIMC_BUG(!cis);

	info("[%s] start\n", __func__);

	I2C_MUTEX_LOCK(cis->i2c_lock);
	/* setfile global setting is at camera entrance */
	ret = sensor_cis_set_registers(subdev, sensor_gc13a0_global, sensor_gc13a0_global_size);
	if (ret < 0) {
		err("sensor_gc13a0_global fail!!");
		goto p_err;
	}

	info("[%s] done\n", __func__);

p_err:
	I2C_MUTEX_UNLOCK(cis->i2c_lock);

	return ret;
}

int sensor_gc13a0_cis_mode_change(struct v4l2_subdev *subdev, u32 mode)
{
	int ret = 0;
	struct is_cis *cis = NULL;
	struct i2c_client *client;

	FIMC_BUG(!subdev);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);
	FIMC_BUG(!cis);
	FIMC_BUG(!cis->cis_data);

	client = cis->client;
	if (unlikely(!client)) {
		err("client is NULL");
		ret = -EINVAL;
		return ret;
	}

	if (mode > sensor_gc13a0_max_setfile_num) {
		err("invalid mode(%d)!!", mode);
		ret = -EINVAL;
		goto p_err;
	}
	info("[%s] sensor mode(%d)\n", __func__, mode);

#ifdef USE_CAMERA_ADAPTIVE_MIPI
	cis->mipi_clock_index_cur = CAM_MIPI_NOT_INITIALIZED;
#endif

	sensor_gc13a0_cis_data_calculation(sensor_gc13a0_pllinfos[mode], cis->cis_data);

	I2C_MUTEX_LOCK(cis->i2c_lock);
	ret = sensor_cis_set_registers(subdev, sensor_gc13a0_setfiles[mode], sensor_gc13a0_setfile_sizes[mode]);
	if (ret < 0) {
		err("sensor_gc13a0_setfiles fail!!");
		goto p_i2c_err;
	}

	cis->cis_data->frame_time = (cis->cis_data->line_readOut_time * cis->cis_data->cur_height / 1000);
	cis->cis_data->rolling_shutter_skew = (cis->cis_data->cur_height - 1) * cis->cis_data->line_readOut_time;
	dbg_sensor(2, "[%s] frame_time(%d), rolling_shutter_skew(%lld)\n", __func__,
		cis->cis_data->frame_time, cis->cis_data->rolling_shutter_skew);

	info("[%s] mode changed(%d)\n", __func__, mode);

p_i2c_err:
	I2C_MUTEX_UNLOCK(cis->i2c_lock);

p_err:
	return ret;
}

/* TODO: Sensor set size sequence(sensor done, sensor stop, 3AA done in FW case */
int sensor_gc13a0_cis_set_size(struct v4l2_subdev *subdev, cis_shared_data *cis_data)
{
	int ret = 0;
	bool binning = false;
	u32 ratio_w = 0, ratio_h = 0, start_x = 0, start_y = 0, end_x = 0, end_y = 0;
	struct i2c_client *client = NULL;
	struct is_cis *cis = NULL;
	ktime_t st = ktime_get();

	FIMC_BUG(!subdev);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);
	FIMC_BUG(!cis);

	dbg_sensor(2, "[MOD:D:%d] %s\n", cis->id, __func__);

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
	ret = sensor_gc13a0_wait_stream_off_status(cis_data);
	if (ret) {
		err("Must stream off\n");
		ret = -EINVAL;
		goto p_err;
	}

	binning = cis_data->binning;
	if (binning) {
		ratio_w = (SENSOR_GC13A0_MAX_WIDTH / cis_data->cur_width);
		ratio_h = (SENSOR_GC13A0_MAX_HEIGHT / cis_data->cur_height);
	} else {
		ratio_w = 1;
		ratio_h = 1;
	}

	if (((cis_data->cur_width * ratio_w) > SENSOR_GC13A0_MAX_WIDTH) ||
		((cis_data->cur_height * ratio_h) > SENSOR_GC13A0_MAX_HEIGHT)) {
		err("Config max sensor size over~!!\n");
		ret = -EINVAL;
		goto p_err;
	}

	/* pixel address region setting */
	start_x = ((SENSOR_GC13A0_MAX_WIDTH - cis_data->cur_width * ratio_w) / 2) & (~0x1);
	start_y = ((SENSOR_GC13A0_MAX_HEIGHT - cis_data->cur_height * ratio_h) / 2) & (~0x1);
	end_x = start_x + (cis_data->cur_width * ratio_w - 1);
	end_y = start_y + (cis_data->cur_height * ratio_h - 1);

	if (!(end_x & (0x1)) || !(end_y & (0x1))) {
		err("Sensor pixel end address must odd\n");
		ret = -EINVAL;
		goto p_err;
	}

	I2C_MUTEX_LOCK(cis->i2c_lock);

	/* 1. page_select */
	ret = cis->ixc_ops->addr8_write8(client, 0xfe, 0x00);
	if (ret < 0)
		 goto p_i2c_err;

	/*2 byte address for writing the start_x*/
	ret = cis->ixc_ops->addr8_write8(client, 0x0b, (start_x >> 8));
	if (ret < 0)
		 goto p_i2c_err;

	ret = cis->ixc_ops->addr8_write8(client, 0x0c, (start_x & 0xff));
	if (ret < 0)
		 goto p_i2c_err;

	/*2 byte address for writing the start_y*/
	ret = cis->ixc_ops->addr8_write8(client, 0x09, (start_y >> 8));
	if (ret < 0)
		 goto p_i2c_err;

	ret = cis->ixc_ops->addr8_write8(client, 0x0a, (start_y & 0xff));
	if (ret < 0)
		 goto p_i2c_err;

	/*2 byte address for writing the end_x*/
	ret = cis->ixc_ops->addr8_write8(client, 0x0f, (end_x >> 8));
	if (ret < 0)
		 goto p_i2c_err;
	ret = cis->ixc_ops->addr8_write8(client, 0x10, (end_x & 0xff));
	if (ret < 0)
		 goto p_i2c_err;

	/*2 byte address for writing the end_y*/
	ret = cis->ixc_ops->addr8_write8(client, 0x0d, (end_y >> 8));
	if (ret < 0)
		 goto p_i2c_err;
	ret = cis->ixc_ops->addr8_write8(client, 0x0e, (end_y & 0xff));
	if (ret < 0)
		 goto p_i2c_err;

	/* 3. page_select */
	ret = cis->ixc_ops->addr8_write8(client, 0xfe, 0x01);
	if (ret < 0)
		 goto p_i2c_err;

	/* 4. output address setting width*/
	ret = cis->ixc_ops->addr8_write8(client, 0x97, (cis_data->cur_width >> 8));
	if (ret < 0)
		 goto p_i2c_err;
	ret = cis->ixc_ops->addr8_write8(client, 0x98, (cis_data->cur_width & 0xff));
	if (ret < 0)
		 goto p_i2c_err;

	/* 4. output address setting height*/
	ret = cis->ixc_ops->addr8_write8(client, 0x95, (cis_data->cur_height >> 8));
	if (ret < 0)
		 goto p_i2c_err;
	ret = cis->ixc_ops->addr8_write8(client, 0x96, (cis_data->cur_height & 0xff));
	if (ret < 0)
		 goto p_i2c_err;

	cis_data->frame_time = (cis_data->line_readOut_time * cis_data->cur_height / 1000);
	cis->cis_data->rolling_shutter_skew = (cis->cis_data->cur_height - 1) * cis->cis_data->line_readOut_time;
	dbg_sensor(2, "[%s] frame_time(%d), rolling_shutter_skew(%lld)\n", __func__,
		cis->cis_data->frame_time, cis->cis_data->rolling_shutter_skew);

	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %ldus", __func__, PABLO_KTIME_US_DELTA_NOW(st));

p_i2c_err:
	I2C_MUTEX_UNLOCK(cis->i2c_lock);
	
p_err:
	return ret;
}

int sensor_gc13a0_cis_stream_on(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_core *core = NULL;
	struct is_cis *cis = NULL;
	struct i2c_client *client = NULL;
	cis_shared_data *cis_data = NULL;
	struct is_device_sensor *this_device = NULL;
	bool single_mode = true; /* default single */
	ktime_t st = ktime_get();

	FIMC_BUG(!subdev);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);

	FIMC_BUG(!cis);
	FIMC_BUG(!cis->cis_data);

	this_device = (struct is_device_sensor *)v4l2_get_subdev_hostdata(subdev);
	FIMC_BUG(!this_device);

	client = cis->client;
	if (unlikely(!client)) {
		err("client is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	core = is_get_is_core();
	if (!core) {
		err("The core device is null");
		ret = -EINVAL;
		goto p_err;
	}

	cis_data = cis->cis_data;

#if !defined(DISABLE_DUAL_SYNC)
	if ((this_device != &core->sensor[0]) && test_bit(IS_SENSOR_OPEN, &(core->sensor[0].state))) {
		single_mode = false;
	}
#endif
#ifdef USE_CAMERA_ADAPTIVE_MIPI
	is_vendor_set_mipi_clock(this_device);
#endif

	dbg_sensor(2, "[MOD:D:%d] %s\n", cis->id, __func__);

	/* Sensor Dual sync on/off */
	info("[%s] %s mode\n", __func__, single_mode ? "master" : "dual sync slave");
	I2C_MUTEX_LOCK(cis->i2c_lock);
	if (single_mode == false) {
		ret = sensor_cis_set_registers(subdev, sensor_gc13a0_fsync_slave, sensor_gc13a0_fsync_slave_size);
		if (ret < 0)
			err("[%s] sensor_gc13a0_fsync_slave fail\n", __func__);
		/* The delay which can change the frame-length of first frame was removed here*/
	} else {
		ret = sensor_cis_set_registers(subdev, sensor_gc13a0_fsync_master, sensor_gc13a0_fsync_master_size);
		if (ret < 0)
			err("[%s] sensor_gc13a0_fsync_master fail\n", __func__);
	}

	/* Sensor stream on */
	ret = cis->ixc_ops->write8(cis->client, 0x0100, 0x01);
	I2C_MUTEX_UNLOCK(cis->i2c_lock);
	if (ret < 0) {
		err("[%s] sensor_gc13a0_stream_on fail\n", __func__);
		goto p_err;
	}
	cis_data->stream_on = true;
	info("%s done\n", __func__);

	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %ldus", __func__, PABLO_KTIME_US_DELTA_NOW(st));

p_err:
	return ret;
}

int sensor_gc13a0_cis_stream_off(struct v4l2_subdev *subdev)
{
	int ret = 0;
	u16 frame_count = 0;
	struct is_cis *cis;
	struct i2c_client *client;
	cis_shared_data *cis_data;
	ktime_t st = ktime_get();

	FIMC_BUG(!subdev);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);

	FIMC_BUG(!cis);
	FIMC_BUG(!cis->cis_data);

	client = cis->client;
	if (unlikely(!client)) {
		err("client is NULL");
		return -EINVAL;
	}

	cis_data = cis->cis_data;

	dbg_sensor(2, "[MOD:D:%d] %s\n", cis->id, __func__);

	I2C_MUTEX_LOCK(cis->i2c_lock);

	ret = cis->ixc_ops->read16(cis->client, GC13A0_ADDR_FRAME_COUNT, &frame_count);
	/* Sensor stream off */
	ret = cis->ixc_ops->write8(cis->client, 0x0100, 0x00);
	if (ret < 0) {
		err("i2c treansfer fail addr(%x), val(%x), ret(%d)\n", 0x0100, 0x00, ret);
		goto p_err;
	}

	cis_data->stream_on = false;
	info("%s done, frame_count(%d) cur_frame_ms_time(%d)\n",
		__func__, frame_count, cis_data->cur_frame_us_time/1000);

	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %ldus", __func__, PABLO_KTIME_US_DELTA_NOW(st));

p_err:
	I2C_MUTEX_UNLOCK(cis->i2c_lock);
	return ret;
}

int sensor_gc13a0_cis_get_min_exposure_time(struct v4l2_subdev *subdev, u32 *min_expo)
{
	int ret = 0;
	struct is_cis *cis = NULL;
	cis_shared_data *cis_data = NULL;
	u32 min_integration_time = 0;
	u32 min_coarse = 0;
	u32 min_fine = 0;
	u64 vt_pix_clk_freq_khz = 0;
	u32 line_length_pck = 0;
	ktime_t st = ktime_get();

	FIMC_BUG(!subdev);
	FIMC_BUG(!min_expo);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);

	FIMC_BUG(!cis);
	FIMC_BUG(!cis->cis_data);

	cis_data = cis->cis_data;

	vt_pix_clk_freq_khz = cis_data->pclk / 1000;
	if (vt_pix_clk_freq_khz == 0) {
		pr_err("[MOD:D:%d] %s, Invalid vt_pix_clk_freq_khz(%d)\n", cis->id, __func__, vt_pix_clk_freq_khz);
		goto p_err;
	}
	line_length_pck = cis_data->line_length_pck;
	min_coarse = cis_data->min_coarse_integration_time;
	min_fine = cis_data->min_fine_integration_time;

	min_integration_time = (u32)((u64)((line_length_pck * min_coarse) + min_fine) * 1000 / vt_pix_clk_freq_khz);
	*min_expo = min_integration_time;

	dbg_sensor(2, "[%s] min integration time %d\n", __func__, min_integration_time);

	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %ldus", __func__, PABLO_KTIME_US_DELTA_NOW(st));

p_err:
	return ret;
}

int sensor_gc13a0_cis_get_max_exposure_time(struct v4l2_subdev *subdev, u32 *max_expo)
{
	int ret = 0;
	struct is_cis *cis;
	cis_shared_data *cis_data;
	u32 max_integration_time = 0;
	u32 max_coarse_margin = 0;
	u32 max_fine_margin = 0;
	u32 max_coarse = 0;
	u32 max_fine = 0;
	u64 vt_pix_clk_freq_khz = 0;
	u32 line_length_pck = 0;
	u32 frame_length_lines = 0;
	ktime_t st = ktime_get();

	FIMC_BUG(!subdev);
	FIMC_BUG(!max_expo);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);

	FIMC_BUG(!cis);
	FIMC_BUG(!cis->cis_data);

	cis_data = cis->cis_data;

	vt_pix_clk_freq_khz = cis_data->pclk / 1000;
	if (vt_pix_clk_freq_khz == 0) {
		pr_err("[MOD:D:%d] %s, Invalid vt_pix_clk_freq_khz(%d)\n", cis->id, __func__, vt_pix_clk_freq_khz);
		goto p_err;
	}
	line_length_pck = cis_data->line_length_pck;
	frame_length_lines = cis_data->frame_length_lines;

	max_coarse_margin = cis_data->max_margin_coarse_integration_time;
	max_fine_margin = line_length_pck - cis_data->min_fine_integration_time;
	max_coarse = frame_length_lines - max_coarse_margin;
	max_fine = cis_data->max_fine_integration_time;

	max_integration_time = (u32)((u64)((line_length_pck * max_coarse) + max_fine) * 1000 / vt_pix_clk_freq_khz);

	*max_expo = max_integration_time;

	/* TODO: Is this values update hear? */
	cis_data->max_margin_fine_integration_time = max_fine_margin;
	cis_data->max_coarse_integration_time = max_coarse;

	dbg_sensor(2, "[%s] max integration time %d, max margin fine integration %d, max coarse integration %d\n",
			__func__, max_integration_time, cis_data->max_margin_fine_integration_time, cis_data->max_coarse_integration_time);

	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %ldus", __func__, PABLO_KTIME_US_DELTA_NOW(st));

p_err:
	return ret;
}

int sensor_gc13a0_cis_adjust_frame_duration(struct v4l2_subdev *subdev,
						u32 input_exposure_time,
						u32 *target_duration)
{
	int ret = 0;
	struct is_cis *cis;
	cis_shared_data *cis_data;

	u64 vt_pix_clk_freq_khz = 0;
	u32 line_length_pck = 0;
	u32 frame_length_lines = 0;
	u32 frame_duration = 0;
	u32 max_frame_us_time = 0;
	ktime_t st = ktime_get();

	FIMC_BUG(!subdev);
	FIMC_BUG(!target_duration);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);

	FIMC_BUG(!cis);
	FIMC_BUG(!cis->cis_data);

	cis_data = cis->cis_data;

	vt_pix_clk_freq_khz = cis_data->pclk / 1000;
	line_length_pck = cis_data->line_length_pck;
	frame_length_lines = (u32)((vt_pix_clk_freq_khz * input_exposure_time) / (line_length_pck * 1000));
	frame_length_lines += cis_data->max_margin_coarse_integration_time;

	max_frame_us_time = 1000000/cis->min_fps;
	frame_duration = (u32)((u64)(frame_length_lines * line_length_pck) * 1000 / vt_pix_clk_freq_khz);

	dbg_sensor(2, "[%s](vsync cnt = %d) input exp(%d), adj duration - frame duraion(%d), min_frame_us(%d)\n",
			__func__, cis_data->sen_vsync_count, input_exposure_time, frame_duration, cis_data->min_frame_us_time);

	*target_duration = MAX(frame_duration, cis_data->min_frame_us_time);
	if(cis->min_fps == cis->max_fps) {
		*target_duration = MIN(frame_duration, max_frame_us_time);
	}

	dbg_sensor(2, "[%s] requested min_fps(%d), max_fps(%d) from HAL, calculated frame_duration(%d), adjusted frame_duration(%d)\n", __func__, cis->min_fps, cis->max_fps, frame_duration, *target_duration);

	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %ldus", __func__, PABLO_KTIME_US_DELTA_NOW(st));

	return ret;
}

int sensor_gc13a0_cis_set_frame_duration(struct v4l2_subdev *subdev, u32 frame_duration)
{
	int ret = 0;
	struct is_cis *cis;
	struct i2c_client *client;
	cis_shared_data *cis_data;
	struct is_core *core;

	u64 vt_pix_clk_freq_khz = 0;
	u32 line_length_pck = 0;
	u16 frame_length_lines = 0;
	ktime_t st = ktime_get();

	FIMC_BUG(!subdev);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);

	FIMC_BUG(!cis);
	FIMC_BUG(!cis->cis_data);

	client = cis->client;
	if (unlikely(!client)) {
		err("client is NULL");
		ret = -EINVAL;
		return ret;
	}

	cis_data = cis->cis_data;

	core = is_get_is_core();
	if (!core) {
		err("core device is null");
		ret = -EINVAL;
		return ret;
	}

	if (sensor_gc13a0_check_master_stream_off(core)) {
		dbg_sensor(2, "%s: Master cam did not enter in stream_on yet. Stop updating frame_length_lines", __func__);
		return ret;
	}

	if (frame_duration < cis_data->min_frame_us_time) {
		dbg_sensor(2, "frame duration is less than min(%d)\n", frame_duration);
		frame_duration = cis_data->min_frame_us_time;
	}

	vt_pix_clk_freq_khz = cis_data->pclk / 1000;
	line_length_pck = cis_data->line_length_pck;

	frame_length_lines = (u16)((vt_pix_clk_freq_khz * frame_duration) / (line_length_pck * 1000));

	/* Frame length lines should be a multiple of 2 */
	frame_length_lines = (frame_length_lines / 2) * 2;

	dbg_sensor(2, "[MOD:D:%d] %s, vt_pix_clk_freq_khz(%#x) frame_duration = %d us,"
		KERN_CONT "line_length_pck(%#x), frame_length_lines(%#x)\n",
		cis->id, __func__, vt_pix_clk_freq_khz, frame_duration, line_length_pck, frame_length_lines);

	I2C_MUTEX_LOCK(cis->i2c_lock);

	ret = cis->ixc_ops->write8(client, 0x0340, (frame_length_lines >> 8) & 0xff);
	if (ret < 0)
		goto p_err;
	ret = cis->ixc_ops->write8(client, 0x0341, (frame_length_lines & 0xff));
	if (ret < 0)
		goto p_err;

	cis_data->cur_frame_us_time = frame_duration;
	cis_data->frame_length_lines = frame_length_lines;
	cis_data->max_coarse_integration_time = cis_data->frame_length_lines - cis_data->max_margin_coarse_integration_time;

	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %ldus", __func__, PABLO_KTIME_US_DELTA_NOW(st));

p_err:
	I2C_MUTEX_UNLOCK(cis->i2c_lock);

	return ret;
}

int sensor_gc13a0_cis_set_frame_rate(struct v4l2_subdev *subdev, u32 min_fps)
{
	int ret = 0;
	struct is_cis *cis;
	cis_shared_data *cis_data;

	u32 frame_duration = 0;
	ktime_t st = ktime_get();

	FIMC_BUG(!subdev);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);

	FIMC_BUG(!cis);
	FIMC_BUG(!cis->cis_data);

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

	dbg_sensor(2, "[MOD:D:%d] %s, set FPS(%d), frame duration(%d)\n",
			cis->id, __func__, min_fps, frame_duration);

	ret = sensor_gc13a0_cis_set_frame_duration(subdev, frame_duration);
	if (ret < 0) {
		err("[MOD:D:%d] %s, set frame duration is fail(%d)\n",
			cis->id, __func__, ret);
		goto p_err;
	}

	cis_data->min_frame_us_time = frame_duration;

	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %ldus", __func__, PABLO_KTIME_US_DELTA_NOW(st));

p_err:

	return ret;
}

int sensor_gc13a0_cis_adjust_analog_gain(struct v4l2_subdev *subdev, u32 input_again, u32 *target_permile)
{
	int ret = 0;
	struct is_cis *cis;
	cis_shared_data *cis_data;

	u32 again_code = 0;
	u32 again_permile = 0;
	ktime_t st = ktime_get();

	FIMC_BUG(!subdev);
	FIMC_BUG(!target_permile);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);

	FIMC_BUG(!cis);
	FIMC_BUG(!cis->cis_data);

	cis_data = cis->cis_data;

	again_code = sensor_gc13a0_cis_calc_gain_code(input_again);

	if (again_code > cis_data->max_analog_gain[0]) {
		again_code = cis_data->max_analog_gain[0];
	} else if (again_code < cis_data->min_analog_gain[0]) {
		again_code = cis_data->min_analog_gain[0];
	}

	again_permile = sensor_gc13a0_cis_calc_gain_permile(again_code);

	dbg_sensor(2, "[%s] min again(%d), max(%d), input_again(%d), code(%d), permile(%d)\n", __func__,
			cis_data->max_analog_gain[0],
			cis_data->min_analog_gain[0],
			input_again,
			again_code,
			again_permile);

	*target_permile = again_permile;

	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %ldus", __func__, PABLO_KTIME_US_DELTA_NOW(st));

	return ret;
}

int sensor_gc13a0_cis_get_analog_gain(struct v4l2_subdev *subdev, u32 *again)
{
	int ret = 0;
	struct is_cis *cis;
	struct i2c_client *client;

	u8 low_analog_gain = 0;
	u8 high_analog_gain = 0;
	u16 analog_gain = 0;

	ktime_t st = ktime_get();

	FIMC_BUG(!subdev);
	FIMC_BUG(!again);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);

	FIMC_BUG(!cis);

	client = cis->client;
	if (unlikely(!client)) {
		err("client is NULL");
		ret = -EINVAL;
		return ret;
	}

	I2C_MUTEX_LOCK(cis->i2c_lock);

	ret = cis->ixc_ops->read8(client, 0x0204, &high_analog_gain);
	ret = cis->ixc_ops->read8(client, 0x0205, &low_analog_gain);
	if (ret < 0)
		goto p_err;

	analog_gain = high_analog_gain << 8 | low_analog_gain;

	*again = sensor_gc13a0_cis_calc_gain_permile(analog_gain);

	dbg_sensor(2, "[MOD:D:%d] %s, cur_again = %d us, analog_gain(%#x)\n",
			cis->id, __func__, *again, analog_gain);

	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %ldus", __func__, PABLO_KTIME_US_DELTA_NOW(st));

p_err:
	I2C_MUTEX_UNLOCK(cis->i2c_lock);

	return ret;
}

int sensor_gc13a0_cis_get_min_analog_gain(struct v4l2_subdev *subdev, u32 *min_again)
{
	int ret = 0;
	struct is_cis *cis;
	struct i2c_client *client;
	cis_shared_data *cis_data;

	u16 read_value = 0;
	ktime_t st = ktime_get();

	FIMC_BUG(!subdev);
	FIMC_BUG(!min_again);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);

	FIMC_BUG(!cis);
	FIMC_BUG(!cis->cis_data);

	client = cis->client;
	if (unlikely(!client)) {
		err("client is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	cis_data = cis->cis_data;

	read_value = SENSOR_GC13A0_MIN_ANALOG_GAIN;

	cis_data->min_analog_gain[0] = read_value;

	cis_data->min_analog_gain[1] = sensor_gc13a0_cis_calc_gain_permile(read_value);

	*min_again = cis_data->min_analog_gain[1];

	dbg_sensor(2, "[%s] code %d, permile %d\n", __func__,
		cis_data->min_analog_gain[0], cis_data->min_analog_gain[1]);

	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %ldus", __func__, PABLO_KTIME_US_DELTA_NOW(st));

p_err:
	return ret;
}

int sensor_gc13a0_cis_get_max_analog_gain(struct v4l2_subdev *subdev, u32 *max_again)
{
	int ret = 0;
	struct is_cis *cis;
	struct i2c_client *client;
	cis_shared_data *cis_data;

	u16 read_value = 0;
	ktime_t st = ktime_get();

	FIMC_BUG(!subdev);
	FIMC_BUG(!max_again);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);

	FIMC_BUG(!cis);
	FIMC_BUG(!cis->cis_data);

	client = cis->client;
	if (unlikely(!client)) {
		err("client is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	cis_data = cis->cis_data;

	read_value = SENSOR_GC13A0_MAX_ANALOG_GAIN;

	cis_data->max_analog_gain[0] = read_value;

	cis_data->max_analog_gain[1] = sensor_gc13a0_cis_calc_gain_permile(read_value);

	*max_again = cis_data->max_analog_gain[1];

	dbg_sensor(2, "[%s] code %d, permile %d\n", __func__,
		cis_data->max_analog_gain[0], cis_data->max_analog_gain[1]);

	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %ldus", __func__, PABLO_KTIME_US_DELTA_NOW(st));

p_err:
	return ret;
}

int sensor_gc13a0_cis_get_digital_gain(struct v4l2_subdev *subdev, u32 *dgain)
{
	int ret = 0;
	struct is_cis *cis;
	struct i2c_client *client;

	u8 low_digital_gain = 0;
	u8 high_digital_gain = 0;
	u16 digital_gain = 0;
	ktime_t st = ktime_get();

	FIMC_BUG(!subdev);
	FIMC_BUG(!dgain);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);

	FIMC_BUG(!cis);

	client = cis->client;
	if (unlikely(!client)) {
		err("client is NULL");
		ret = -EINVAL;
		return ret;
	}

	I2C_MUTEX_LOCK(cis->i2c_lock);

	ret = cis->ixc_ops->read8(client, 0x020e, &high_digital_gain);
	ret = cis->ixc_ops->read8(client, 0x020f, &low_digital_gain);
	if (ret < 0)
		goto p_err;

	digital_gain = high_digital_gain << 8 | low_digital_gain;

	*dgain = sensor_gc13a0_cis_calc_gain_permile(digital_gain);

	dbg_sensor(2, "[MOD:D:%d] %s, cur_dgain = %d us, digital_gain(%#x)\n",
			cis->id, __func__, *dgain, digital_gain);

	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %ldus", __func__, PABLO_KTIME_US_DELTA_NOW(st));

p_err:
	I2C_MUTEX_UNLOCK(cis->i2c_lock);

	return ret;
}

int sensor_gc13a0_cis_get_min_digital_gain(struct v4l2_subdev *subdev, u32 *min_dgain)
{
	int ret = 0;
	struct is_cis *cis;
	struct i2c_client *client;
	cis_shared_data *cis_data;
	u16 read_value = 0;
	ktime_t st = ktime_get();

	FIMC_BUG(!subdev);
	FIMC_BUG(!min_dgain);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);

	FIMC_BUG(!cis);
	FIMC_BUG(!cis->cis_data);

	client = cis->client;
	if (unlikely(!client)) {
		err("client is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	cis_data = cis->cis_data;

	read_value = SENSOR_GC13A0_MIN_DIGITAL_GAIN;

	cis_data->min_digital_gain[0] = read_value;

	cis_data->min_digital_gain[1] = sensor_gc13a0_cis_calc_gain_permile(read_value);

	*min_dgain = cis_data->min_digital_gain[1];

	dbg_sensor(2, "[%s] code %d, permile %d\n", __func__,
		cis_data->min_digital_gain[0], cis_data->min_digital_gain[1]);

	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %ldus", __func__, PABLO_KTIME_US_DELTA_NOW(st));

p_err:
	return ret;
}

int sensor_gc13a0_cis_get_max_digital_gain(struct v4l2_subdev *subdev, u32 *max_dgain)
{
	int ret = 0;
	struct is_cis *cis;
	struct i2c_client *client;
	cis_shared_data *cis_data;
	u16 read_value = 0;
	ktime_t st = ktime_get();

	FIMC_BUG(!subdev);
	FIMC_BUG(!max_dgain);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);

	FIMC_BUG(!cis);
	FIMC_BUG(!cis->cis_data);

	client = cis->client;
	if (unlikely(!client)) {
		err("client is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	cis_data = cis->cis_data;

	read_value = SENSOR_GC13A0_MAX_DIGITAL_GAIN;

	cis_data->max_digital_gain[0] = read_value;

	cis_data->max_digital_gain[1] = sensor_gc13a0_cis_calc_gain_permile(read_value);

	*max_dgain = cis_data->max_digital_gain[1];

	dbg_sensor(2, "[%s] code %d, permile %d\n", __func__,
		cis_data->max_digital_gain[0], cis_data->max_digital_gain[1]);

	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %ldus", __func__, PABLO_KTIME_US_DELTA_NOW(st));

p_err:
	return ret;
}

int sensor_gc13a0_cis_set_totalgain(struct v4l2_subdev *subdev, struct ae_param *target_exposure,
	struct ae_param *again, struct ae_param *dgain)
{
	int ret = 0;
	struct is_cis *cis;
	cis_shared_data *cis_data;
	struct is_core *core;

	u16 origin_exp = 0;
	u64 vt_pix_clk_freq_khz = 0;
	u32 line_length_pck = 0;
	u32 min_fine_int = 0;

	FIMC_BUG(!subdev);
	FIMC_BUG(!target_exposure);
	FIMC_BUG(!again);
	FIMC_BUG(!dgain);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);

	FIMC_BUG(!cis);
	FIMC_BUG(!cis->cis_data);

	if ((target_exposure->long_val <= 0) || (again->val <= 0)) {
		err("[%s] invalid target exposure & again(%d, %d)\n", __func__,
				target_exposure->long_val, again->val );
		ret = -EINVAL;
		goto p_err;
	}

	cis_data = cis->cis_data;

	core = is_get_is_core();
	if (!core) {
		err("core device is null");
		ret = -EINVAL;
		goto p_err;
	}

	if (sensor_gc13a0_check_master_stream_off(core)) {
		dbg_sensor(2, "%s: Master cam did not enter in stream_on yet. Stop updating exposure time", __func__);
		return ret;
	}

	dbg_sensor(2, "[MOD:D:%d] %s, vsync_cnt(%d), target long(%d), again(%d)\n", cis->id, __func__,
			cis_data->sen_vsync_count, target_exposure->long_val, again->val);

	vt_pix_clk_freq_khz = cis_data->pclk / 1000;
	line_length_pck = cis_data->line_length_pck;
	min_fine_int = cis_data->min_fine_integration_time;

	dbg_sensor(2, "[MOD:D:%d] %s, vt_pix_clk_freq_khz (%d), line_length_pck(%d), min_fine_int (%d)\n",
		cis->id, __func__, vt_pix_clk_freq_khz, line_length_pck, min_fine_int);

	origin_exp = (u16)(((((target_exposure->long_val * vt_pix_clk_freq_khz) - min_fine_int) / (line_length_pck * 1000))) / 2) * 2;

	if (origin_exp > cis_data->max_coarse_integration_time) {
		origin_exp = cis_data->max_coarse_integration_time;
		dbg_sensor(2, "[MOD:D:%d] %s, vsync_cnt(%d), origin exp(%d) max\n", cis->id, __func__,
			cis_data->sen_vsync_count, origin_exp);
	}

	if (origin_exp < cis_data->min_coarse_integration_time) {
		origin_exp = cis_data->min_coarse_integration_time;
		dbg_sensor(2, "[MOD:D:%d] %s, vsync_cnt(%d), origin exp(%d) min\n", cis->id, __func__,
			cis_data->sen_vsync_count, origin_exp);
	}

	/* Set Exposure Time */
	ret = sensor_gc13a0_cis_set_exposure_time(subdev, origin_exp);
	if (ret < 0) {
		err("[%s] sensor_gc13a0_cis_set_exposure_time fail\n", __func__);
		goto p_err;
	}

	/* Set Analog gain */
	ret = sensor_gc13a0_cis_set_analog_gain(subdev, again);
	if (ret < 0) {
		err("[%s] sensor_gc13a0_cis_set_analog_gain fail\n", __func__);
		goto p_err;
	}

	/* Set Digital gain */
	ret = sensor_gc13a0_cis_set_digital_gain(subdev, dgain);
	if (ret < 0) {
		err("[%s] sensor_gc13a0_cis_set_digital_gain fail\n", __func__);
		goto p_err;
	}

	dbg_sensor(2, "[MOD:D:%d] %s, frame_length_lines:%d(%#x), origin_exp:%d(%#x), input_again:%d(%#x) dgain:%d(%#x)\n",
		cis->id, __func__, cis_data->frame_length_lines, cis_data->frame_length_lines,
		origin_exp, origin_exp, again->val, again->val, again->val, again->val);

p_err:

	return ret;
}

int sensor_gc13a0_cis_wait_streamoff(struct v4l2_subdev *subdev)
{
	int ret = 0;
	u32 poll_time_ms = 0;
	struct is_cis *cis = sensor_cis_get_cis(subdev);
	struct i2c_client *client;
	cis_shared_data *cis_data;
	u8 mipi_status = 0;
	bool isStreaming = 0;

	cis_data = cis->cis_data;
	client = cis->client;

	do {
		I2C_MUTEX_LOCK(cis->i2c_lock);
		ret = cis->ixc_ops->read8(client, GC13A0_ADDR_MIPI_CLK_STATUS, &mipi_status);
		I2C_MUTEX_UNLOCK(cis->i2c_lock);
		isStreaming = (mipi_status & (0x1 << GC13A0_BIT_MIPI_CLK_ENABLE)) ? 1 : 0;
		if (!isStreaming)
			break;

		usleep_range(POLL_TIME_US, POLL_TIME_US + 10);
		poll_time_ms += POLL_TIME_MS;

		dbg_sensor(1, "[MOD:D:%d] %s, isStreaming(%d), (poll_time_ms(%d) < STREAM_OFF_POLL_TIME_MS(%d))\n",
				cis->id, __func__, isStreaming, poll_time_ms, STREAM_OFF_POLL_TIME_MS);
	} while (poll_time_ms < STREAM_OFF_POLL_TIME_MS);

	if (poll_time_ms < STREAM_OFF_POLL_TIME_MS)
		info("%s: finished after %d ms\n", __func__, poll_time_ms);
	else
		warn("%s: finished : polling timeout occured after %d ms\n", __func__, poll_time_ms);

	return ret;
}

int sensor_gc13a0_cis_wait_streamon(struct v4l2_subdev *subdev)
{
	int ret = 0;
	u32 poll_time_ms = 0;
	struct is_cis *cis = sensor_cis_get_cis(subdev);
	struct i2c_client *client;
	cis_shared_data *cis_data;
	u16 prev_frame_value = 0;
	u16 cur_frame_value = 0;

	cis_data = cis->cis_data;
	client = cis->client;

	I2C_MUTEX_LOCK(cis->i2c_lock);
	ret = cis->ixc_ops->read16(client, GC13A0_ADDR_FRAME_COUNT, &prev_frame_value);
	I2C_MUTEX_UNLOCK(cis->i2c_lock);
	if (ret < 0) {
		err("i2c transfer fail addr(%x), val(%x), ret = %d\n", GC13A0_ADDR_FRAME_COUNT, prev_frame_value, ret);
		goto p_err;
	}

	/* Checking stream on */
	do {
		I2C_MUTEX_LOCK(cis->i2c_lock);
		ret = cis->ixc_ops->read16(client, GC13A0_ADDR_FRAME_COUNT, &cur_frame_value);
		I2C_MUTEX_UNLOCK(cis->i2c_lock);
		if (ret < 0) {
			err("i2c transfer fail addr(%x), val(%x), ret = %d\n", GC13A0_ADDR_FRAME_COUNT, cur_frame_value, ret);
			break;
		}

		if (cur_frame_value != prev_frame_value)
			break;

		prev_frame_value = cur_frame_value;

		usleep_range(POLL_TIME_US, POLL_TIME_US + 100);
		poll_time_ms += POLL_TIME_MS;
		dbg_sensor(1, "[MOD:D:%d] %s, fcount(%d), (poll_time_ms(%d) < MAX_WAIT_STREAM_ON_CNT(%d))\n",
				cis->id, __func__, prev_frame_value, poll_time_ms, MAX_WAIT_STREAM_ON_CNT);
	} while (poll_time_ms < STREAM_ON_POLL_TIME_MS);

	if (poll_time_ms < STREAM_ON_POLL_TIME_MS)
		info("%s: finished after %d ms\n", __func__, poll_time_ms);
	else
		warn("%s: finished : polling timeout occured after %d ms\n", __func__, poll_time_ms);

p_err:
	return ret;
}

static struct is_cis_ops cis_ops = {
	.cis_init = sensor_gc13a0_cis_init,
	.cis_log_status = sensor_gc13a0_cis_log_status,
	.cis_set_global_setting = sensor_gc13a0_cis_set_global_setting,
	.cis_mode_change = sensor_gc13a0_cis_mode_change,
	.cis_set_size = sensor_gc13a0_cis_set_size,
	.cis_stream_on = sensor_gc13a0_cis_stream_on,
	.cis_stream_off = sensor_gc13a0_cis_stream_off,
	.cis_set_exposure_time = NULL,
	.cis_get_min_exposure_time = sensor_gc13a0_cis_get_min_exposure_time,
	.cis_get_max_exposure_time = sensor_gc13a0_cis_get_max_exposure_time,
	.cis_adjust_frame_duration = sensor_gc13a0_cis_adjust_frame_duration,
	.cis_set_frame_duration = sensor_gc13a0_cis_set_frame_duration,
	.cis_set_frame_rate = sensor_gc13a0_cis_set_frame_rate,
	.cis_adjust_analog_gain = sensor_gc13a0_cis_adjust_analog_gain,
	.cis_set_analog_gain = NULL,
	.cis_get_analog_gain = sensor_gc13a0_cis_get_analog_gain,
	.cis_get_min_analog_gain = sensor_gc13a0_cis_get_min_analog_gain,
	.cis_get_max_analog_gain = sensor_gc13a0_cis_get_max_analog_gain,
	.cis_set_digital_gain = NULL,
	.cis_get_digital_gain = sensor_gc13a0_cis_get_digital_gain,
	.cis_get_min_digital_gain = sensor_gc13a0_cis_get_min_digital_gain,
	.cis_get_max_digital_gain = sensor_gc13a0_cis_get_max_digital_gain,
	.cis_compensate_gain_for_extremely_br = sensor_cis_compensate_gain_for_extremely_br,
	.cis_wait_streamoff = sensor_gc13a0_cis_wait_streamoff,
	.cis_wait_streamon = sensor_gc13a0_cis_wait_streamon,
	.cis_check_rev_on_init = sensor_cis_check_rev_on_init,
	.cis_set_totalgain = sensor_gc13a0_cis_set_totalgain,
};

int cis_gc13a0_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	int ret;
	struct is_cis *cis;
	struct is_device_sensor_peri *sensor_peri;
	struct device_node *dnode = client->dev.of_node;
	char const *setfile;
#ifdef USE_CAMERA_ADAPTIVE_MIPI
	int j;
	int index;
	const int *verify_sensor_mode = NULL;
	int verify_sensor_mode_size = 0;
#endif
	ret = sensor_cis_probe(client, &(client->dev), &sensor_peri,I2C_TYPE);
	if (ret) {
		probe_info("%s: sensor_cis_probe ret(%d)\n", __func__, ret);
		return ret;
	}

	cis = &sensor_peri->cis;

	cis->ctrl_delay = N_PLUS_TWO_FRAME;
	cis->cis_ops = &cis_ops;

#ifdef USE_CAMERA_ADAPTIVE_MIPI
	cis->mipi_clock_index_cur = CAM_MIPI_NOT_INITIALIZED;
	cis->mipi_clock_index_new = CAM_MIPI_NOT_INITIALIZED;
#endif

	cis->bayer_order = OTF_INPUT_ORDER_BAYER_GR_BG;


	/* Use total gain instead of using dgain */
	cis->use_dgain = false;
	cis->hdr_ctrl_by_again = false;
	cis->use_total_gain = true;

	ret = of_property_read_string(dnode, "setfile", &setfile);
	if (ret) {
		err("setfile index read fail(%d), take default setfile!!", ret);
		setfile = "default";
	}

	if (strcmp(setfile, "default") == 0 || strcmp(setfile, "setA") == 0) {
		probe_info("%s setfile_A\n", __func__);
		sensor_gc13a0_global = sensor_gc13a0_setfile_A_Global;
		sensor_gc13a0_global_size = ARRAY_SIZE(sensor_gc13a0_setfile_A_Global);
		sensor_gc13a0_setfiles = sensor_gc13a0_setfiles_A;
		sensor_gc13a0_setfile_sizes = sensor_gc13a0_setfile_A_sizes;
		sensor_gc13a0_pllinfos = sensor_gc13a0_pllinfos_A;
		sensor_gc13a0_max_setfile_num = ARRAY_SIZE(sensor_gc13a0_setfiles_A);
		sensor_gc13a0_fsync_master = sensor_gc13a0_setfile_A_Fsync_Master;
		sensor_gc13a0_fsync_master_size = ARRAY_SIZE(sensor_gc13a0_setfile_A_Fsync_Master);
		sensor_gc13a0_fsync_slave = sensor_gc13a0_setfile_A_Fsync_Slave;
		sensor_gc13a0_fsync_slave_size = ARRAY_SIZE(sensor_gc13a0_setfile_A_Fsync_Slave);
	} else {
		err("%s setfile index out of bound, take default (setfile_A)", __func__);
		sensor_gc13a0_global = sensor_gc13a0_setfile_A_Global;
		sensor_gc13a0_global_size = ARRAY_SIZE(sensor_gc13a0_setfile_A_Global);
		sensor_gc13a0_setfiles = sensor_gc13a0_setfiles_A;
		sensor_gc13a0_setfile_sizes = sensor_gc13a0_setfile_A_sizes;
		sensor_gc13a0_pllinfos = sensor_gc13a0_pllinfos_A;
		sensor_gc13a0_max_setfile_num = ARRAY_SIZE(sensor_gc13a0_setfiles_A);
		sensor_gc13a0_fsync_master = sensor_gc13a0_setfile_A_Fsync_Master;
		sensor_gc13a0_fsync_master_size = ARRAY_SIZE(sensor_gc13a0_setfile_A_Fsync_Master);
		sensor_gc13a0_fsync_slave = sensor_gc13a0_setfile_A_Fsync_Slave;
		sensor_gc13a0_fsync_slave_size = ARRAY_SIZE(sensor_gc13a0_setfile_A_Fsync_Slave);
	}
#ifdef USE_CAMERA_ADAPTIVE_MIPI
		cis->mipi_sensor_mode = sensor_gc13a0_setfile_A_mipi_sensor_mode;
		cis->mipi_sensor_mode_size = ARRAY_SIZE(sensor_gc13a0_setfile_A_mipi_sensor_mode);
		verify_sensor_mode = sensor_gc13a0_setfile_A_verify_sensor_mode;
		verify_sensor_mode_size = ARRAY_SIZE(sensor_gc13a0_setfile_A_verify_sensor_mode);
#endif

#ifdef USE_CAMERA_ADAPTIVE_MIPI
	if (cis->vendor_use_adaptive_mipi) {
		for (j = 0; j < verify_sensor_mode_size; j++) {
			index = verify_sensor_mode[j];

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
#endif

	probe_info("%s done\n", __func__);

	return ret;
}

static const struct of_device_id sensor_cis_gc13a0_match[] = {
	{
		.compatible = "samsung,exynos-is-cis-gc13a0",
	},
	{},
};
MODULE_DEVICE_TABLE(of, sensor_cis_gc13a0_match);

static const struct i2c_device_id sensor_cis_gc13a0_idt[] = {
	{ SENSOR_NAME, 0 },
	{},
};

static struct i2c_driver sensor_cis_gc13a0_driver = {
	.probe	= cis_gc13a0_probe,
	.driver = {
		.name	= SENSOR_NAME,
		.owner	= THIS_MODULE,
		.of_match_table = sensor_cis_gc13a0_match,
		.suppress_bind_attrs = true,
	},
	.id_table = sensor_cis_gc13a0_idt,
};

#ifdef MODULE
builtin_i2c_driver(sensor_cis_gc13a0_driver);
#else
static int __init sensor_cis_gc13a0_init(void)
{
	int ret;

	ret = i2c_add_driver(&sensor_cis_gc13a0_driver);
	if (ret)
		err("failed to add %s driver: %d\n",
			sensor_cis_gc13a0_driver.driver.name, ret);

	return ret;
}
late_initcall_sync(sensor_cis_gc13a0_init);
#endif

MODULE_LICENSE("GPL");
MODULE_SOFTDEP("pre: fimc-is");
