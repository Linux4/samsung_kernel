/*
 * Samsung Exynos5 SoC series Sensor driver
 *
 *
 * Copyright (c) 2022 Samsung Electronics Co., Ltd
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
#include "is-cis-sc201.h"
#include "is-cis-sc201-setA.h"
#include "is-cis-sc201-setB.h"

#include "is-helper-i2c.h"
#include "is-vender-specific.h"
#if defined(CONFIG_VENDER_MCD_V2)
#include "is-vender-rom-config.h"
extern const struct is_vender_rom_addr *vender_rom_addr[SENSOR_POSITION_MAX];
#endif

#define SENSOR_NAME "SC201"
#define GET_CLOSEST(x1, x2, x3) (x3 - x1 >= x2 - x3 ? x2 : x1)
#define MULTIPLE_OF_4(val) ((val >> 2) << 2)

#define POLL_TIME_MS (1)
#define POLL_TIME_US (1000)
#define STREAM_OFF_POLL_TIME_MS (500)
#define STREAM_ON_POLL_TIME_MS (500)

#define BASEGAIN (1000000)

static const struct v4l2_subdev_ops subdev_ops;

static const u32 *sensor_sc201_global;
static u32 sensor_sc201_global_size;
static const u32 **sensor_sc201_setfiles;
static const u32 *sensor_sc201_setfile_sizes;
static const struct sensor_pll_info_compact **sensor_sc201_pllinfos;
static u32 sensor_sc201_max_setfile_num;
static const u32 *sensor_sc201_fsync_master;
static u32 sensor_sc201_fsync_master_size;
static const u32 *sensor_sc201_fsync_slave;
static u32 sensor_sc201_fsync_slave_size;
static int check_uninit_value = 0;

static bool sensor_sc201_check_master_stream_off(struct is_core *core)
{
	if (test_bit(IS_SENSOR_OPEN, &(core->sensor[0].state)) &&	/* Dual mode and master stream off */
			!test_bit(IS_SENSOR_FRONT_START, &(core->sensor[0].state)))
		return true;
	else
		return false;
}

static void sensor_sc201_cis_data_calculation(const struct sensor_pll_info_compact *pll_info, cis_shared_data *cis_data)
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
	dbg_sensor(2, "frame_rate (%d) = vt_pix_clk_hz(%lld) / "
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
	dbg_sensor(2, "Pixel rate(Kbps): %d\n", cis_data->pclk / 1000);

	/* Frame period calculation */
	cis_data->frame_time = (cis_data->line_readOut_time * cis_data->cur_height / 1000);
	cis_data->rolling_shutter_skew = (cis_data->cur_height - 1) * cis_data->line_readOut_time;

	dbg_sensor(2, "[%s] frame_time(%d), rolling_shutter_skew(%lld)\n", __func__,
	cis_data->frame_time, cis_data->rolling_shutter_skew);

	/* Constant values */
	cis_data->min_fine_integration_time = SENSOR_SC201_FINE_INTEGRATION_TIME_MIN;
	cis_data->max_fine_integration_time = SENSOR_SC201_FINE_INTEGRATION_TIME_MAX;
	cis_data->min_coarse_integration_time = SENSOR_SC201_COARSE_INTEGRATION_TIME_MIN;
	cis_data->max_margin_coarse_integration_time = SENSOR_SC201_COARSE_INTEGRATION_TIME_MAX_MARGIN;
	info("%s: done", __func__);
}

static int sensor_sc201_wait_stream_off_status(cis_shared_data *cis_data)
{
	int ret = 0;
	u32 timeout = 0;

	FIMC_BUG(!cis_data);

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

int sensor_sc201_cis_check_rev(struct v4l2_subdev *subdev)
{
	int ret = 0;
	u8 rev = 0;
	struct i2c_client *client;
	struct is_cis *cis = NULL;

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);

	FIMC_BUG(!cis);
	FIMC_BUG(!cis->cis_data);

	client = cis->client;
	if (unlikely(!client)) {
		err("client is NULL");
		ret = -EINVAL;
		return ret;
	}
	probe_info("sc201 cis_check_rev start\n");

	I2C_MUTEX_LOCK(cis->i2c_lock);

	cis->cis_data->cis_rev = rev;
	pr_info("%s : [%d] Sensor version. Rev 0x%X\n", __func__, cis->id, cis->cis_data->cis_rev);

	I2C_MUTEX_UNLOCK(cis->i2c_lock);

	return ret;
}

/* CIS OPS */
int sensor_sc201_cis_init(struct v4l2_subdev *subdev)
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

	cis->cis_data->cur_width = SENSOR_SC201_MAX_WIDTH;
	cis->cis_data->cur_height = SENSOR_SC201_MAX_HEIGHT;
	cis->cis_data->low_expo_start = 33000;
	cis->need_mode_change = false;

	sensor_sc201_cis_data_calculation(sensor_sc201_pllinfos[setfile_index], cis->cis_data);

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

#ifdef DEBUG_SENSOR_TIME
	do_gettimeofday(&end);
	dbg_sensor(2, "[%s] time %lu us\n", __func__, (end.tv_sec - st.tv_sec)*1000000 + (end.tv_usec - st.tv_usec));
#endif

p_err:
	return ret;
}

int sensor_sc201_cis_log_status(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_cis *cis;
	struct i2c_client *client = NULL;
	// u8 sensor_fcount_msb = 0, sensor_fcount_lsb = 0;

	FIMC_BUG(!subdev);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);
	if (!cis) {
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

	pr_info("[%s] *******************************\n", __func__);
	I2C_MUTEX_LOCK(cis->i2c_lock);
	sensor_cis_dump_registers(subdev, sensor_sc201_setfiles[0], sensor_sc201_setfile_sizes[0]);
	I2C_MUTEX_UNLOCK(cis->i2c_lock);
	pr_info("[%s] *******************************\n", __func__);

p_err:
	return ret;
}

int sensor_sc201_cis_set_global_setting(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_cis *cis = NULL;

	FIMC_BUG(!subdev);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);
	FIMC_BUG(!cis);

	info("[%s] global setting start\n", __func__);

	I2C_MUTEX_LOCK(cis->i2c_lock);
	ret = sensor_cis_set_registers(subdev, sensor_sc201_global, sensor_sc201_global_size);
	if (ret < 0) {
		err("sensor_sc201_set_registers fail!!");
		goto p_err;
	}

	if (check_uninit_value == 1)
		check_uninit_value = 0;
	info("[%s] global setting done\n", __func__);

p_err:
	I2C_MUTEX_UNLOCK(cis->i2c_lock);

	return ret;
}

int sensor_sc201_cis_mode_change(struct v4l2_subdev *subdev, u32 mode)
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

	if (mode > sensor_sc201_max_setfile_num) {
		err("invalid mode(%d)!!", mode);
		ret = -EINVAL;
		goto p_err;
	}

	sensor_sc201_cis_data_calculation(sensor_sc201_pllinfos[mode], cis->cis_data);

	I2C_MUTEX_LOCK(cis->i2c_lock);
	info("[%s] sensor mode(%d)\n", __func__, mode);
	ret = sensor_cis_set_registers(subdev, sensor_sc201_setfiles[mode], sensor_sc201_setfile_sizes[mode]);
	if (ret < 0) {
		err("sensor_sc201_set_registers fail!!");
		goto p_i2c_err;
	}

#ifdef APPLY_MIRROR_VERTICAL_FLIP
	// Apply Mirror and Vertical Flip.
	ret = is_sensor_write8(client, 0x3221, 0x66);
	if (ret < 0)
		goto p_i2c_err;
#endif

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
int sensor_sc201_cis_set_size(struct v4l2_subdev *subdev, cis_shared_data *cis_data)
{
	int ret = 0;
	bool binning = false;
	u32 ratio_w = 0, ratio_h = 0, start_x = 0, start_y = 0, end_x = 0, end_y = 0;
	struct i2c_client *client = NULL;
	struct is_cis *cis = NULL;
#ifdef DEBUG_SENSOR_TIME
	struct timeval st, end;
	do_gettimeofday(&st);
#endif
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
	ret = sensor_sc201_wait_stream_off_status(cis_data);
	if (ret) {
		err("Must stream off\n");
		ret = -EINVAL;
		goto p_err;
	}

	binning = cis_data->binning;
	if (binning) {
		ratio_w = (SENSOR_SC201_MAX_WIDTH / cis_data->cur_width);
		ratio_h = (SENSOR_SC201_MAX_HEIGHT / cis_data->cur_height);
	} else {
		ratio_w = 1;
		ratio_h = 1;
	}

	if (((cis_data->cur_width * ratio_w) > SENSOR_SC201_MAX_WIDTH) ||
		((cis_data->cur_height * ratio_h) > SENSOR_SC201_MAX_HEIGHT)) {
		err("Config max sensor size over~!!\n");
		ret = -EINVAL;
		goto p_err;
	}

	/* pixel address region setting */
	start_x = ((SENSOR_SC201_MAX_WIDTH - cis_data->cur_width * ratio_w) / 2) & (~0x1);
	start_y = ((SENSOR_SC201_MAX_HEIGHT - cis_data->cur_height * ratio_h) / 2) & (~0x1);
	end_x = start_x + (cis_data->cur_width * ratio_w - 1);
	end_y = start_y + (cis_data->cur_height * ratio_h - 1);

	if (!(end_x & (0x1)) || !(end_y & (0x1))) {
		err("Sensor pixel end address must odd\n");
		ret = -EINVAL;
		goto p_err;
	}

#ifdef DEBUG_SENSOR_TIME
	do_gettimeofday(&end);
	dbg_sensor(2, "[%s] time %lu us\n", __func__, (end.tv_sec - st.tv_sec) * 1000000 + (end.tv_usec - st.tv_usec));
#endif

p_err:
	return ret;
}

int sensor_sc201_cis_stream_on(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_core *core = NULL;
	struct is_cis *cis = NULL;
	struct i2c_client *client = NULL;
	cis_shared_data *cis_data = NULL;
	struct is_device_sensor *this_device = NULL;
	bool single_mode = true; /* default single */

#ifdef DEBUG_SENSOR_TIME
	struct timeval st, end;
	do_gettimeofday(&st);
#endif

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
		return ret;
	}

	core = (struct is_core *)dev_get_drvdata(is_dev);
	if (!core) {
		err("The core device is null");
		ret = -EINVAL;
		return ret;
	}

	cis_data = cis->cis_data;

#if !defined(DISABLE_DUAL_SYNC)
	if ((this_device != &core->sensor[0]) && test_bit(IS_SENSOR_OPEN, &(core->sensor[0].state))) {
		single_mode = false;
	}
#endif

	dbg_sensor(2, "[MOD:D:%d] %s\n", cis->id, __func__);

	I2C_MUTEX_LOCK(cis->i2c_lock);

	/* Sensor Dual sync on/off */
	if (single_mode == false) {
		info("[%s] dual sync slave mode\n", __func__);
		ret = sensor_cis_set_registers(subdev, sensor_sc201_fsync_slave, sensor_sc201_fsync_slave_size);
		if (ret < 0)
			err("[%s] sensor_sc201_fsync_slave fail\n", __func__);

		/* The delay which can change the frame-length of first frame was removed here*/
	}

	/* Sensor stream on */
	info("%s (single_mode : %d)\n", __func__, single_mode);
	ret = is_sensor_write8(client, 0x0100, 0x01);
	if (ret < 0) {
		err("i2c transfer fail addr(%x), val(%x), ret(%d)\n", 0x0100, 0x01, ret);
		goto p_err;
	}

	cis_data->stream_on = true;

#ifdef DEBUG_SENSOR_TIME
	do_gettimeofday(&end);
	dbg_sensor(2, "[%s] time %lu us\n", __func__, (end.tv_sec - st.tv_sec)*1000000 + (end.tv_usec - st.tv_usec));
#endif

p_err:
	I2C_MUTEX_UNLOCK(cis->i2c_lock);

	return ret;
}

int sensor_sc201_cis_stream_off(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_cis *cis;
	struct i2c_client *client;
	cis_shared_data *cis_data;

#ifdef DEBUG_SENSOR_TIME
	struct timeval st, end;
	do_gettimeofday(&st);
#endif

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

	dbg_sensor(2, "[MOD:D:%d] %s\n", cis->id, __func__);

	I2C_MUTEX_LOCK(cis->i2c_lock);

	/* Sensor stream off */
	info("%s\n", __func__);
	ret = is_sensor_write8(client, 0x0100, 0x00);
	if (ret < 0) {
		err("i2c treansfer fail addr(%x), val(%x), ret(%d)\n", 0x0100, 0x00, ret);
		goto p_err;
	}

	cis_data->stream_on = false;
	check_uninit_value = 0;

#ifdef DEBUG_SENSOR_TIME
	do_gettimeofday(&end);
	dbg_sensor(2, "[%s] time %lu us\n", __func__, (end.tv_sec - st.tv_sec)*1000000 + (end.tv_usec - st.tv_usec));
#endif

p_err:
	I2C_MUTEX_UNLOCK(cis->i2c_lock);
	return ret;
}

int sensor_sc201_cis_set_exposure_time(struct v4l2_subdev *subdev, u16 multiple_exp)
{
	int ret = 0;
	struct is_cis *cis;
	struct i2c_client *client;
	cis_shared_data *cis_data = NULL;
	u16 frame_length_lines = 0;

#ifdef DEBUG_SENSOR_TIME
	struct timeval st, end;
	do_gettimeofday(&st);
#endif

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

	if (frame_length_lines > SENSOR_SC201_MAX_FRAME_LENGTH_LINE_VALUE)
		frame_length_lines = SENSOR_SC201_MAX_FRAME_LENGTH_LINE_VALUE;

	multiple_exp = (multiple_exp < cis_data->min_coarse_integration_time) ? cis_data->min_coarse_integration_time : multiple_exp;
	multiple_exp = (multiple_exp > cis_data->max_coarse_integration_time) ? cis_data->max_coarse_integration_time : multiple_exp;

	I2C_MUTEX_LOCK(cis->i2c_lock);

	ret = is_sensor_write8(client, 0x320e, (frame_length_lines >> 8) & 0x7F);
	if (ret < 0)
		goto p_err;

	ret = is_sensor_write8(client, 0x320f, (frame_length_lines & 0xFF));
	if (ret < 0)
		goto p_err;

	ret = is_sensor_write8(client, 0x3e00, (multiple_exp >> 12) & 0x0F);
	if (ret < 0)
		goto p_err;

	ret = is_sensor_write8(client, 0x3e01, (multiple_exp >> 4) & 0xFF);
	if (ret < 0)
		goto p_err;

	ret = is_sensor_write8(client, 0x3e02, (multiple_exp << 4) & 0xF0);
	if (ret < 0)
		goto p_err;

#ifdef DEBUG_SENSOR_TIME
	do_gettimeofday(&end);
	dbg_sensor(2, "[%s] time %lu us\n", __func__, (end.tv_sec - st.tv_sec)*1000000 + (end.tv_usec - st.tv_usec));
#endif

p_err:
	I2C_MUTEX_UNLOCK(cis->i2c_lock);

	return ret;
}

int sensor_sc201_cis_get_min_exposure_time(struct v4l2_subdev *subdev, u32 *min_expo)
{
	int ret = 0;
	struct is_cis *cis = NULL;
	cis_shared_data *cis_data = NULL;
	u32 min_integration_time = 0;
	u32 min_coarse = 0;
	u32 min_fine = 0;
	u64 vt_pix_clk_freq_khz = 0;
	u32 line_length_pck = 0;

#ifdef DEBUG_SENSOR_TIME
	struct timeval st, end;
	do_gettimeofday(&st);
#endif

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

#ifdef DEBUG_SENSOR_TIME
	do_gettimeofday(&end);
	dbg_sensor(2, "[%s] time %lu us\n", __func__, (end.tv_sec - st.tv_sec)*1000000 + (end.tv_usec - st.tv_usec));
#endif

p_err:
	return ret;
}

int sensor_sc201_cis_get_max_exposure_time(struct v4l2_subdev *subdev, u32 *max_expo)
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

#ifdef DEBUG_SENSOR_TIME
	struct timeval st, end;
	do_gettimeofday(&st);
#endif

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

#ifdef DEBUG_SENSOR_TIME
	do_gettimeofday(&end);
	dbg_sensor(2, "[%s] time %lu us\n", __func__, (end.tv_sec - st.tv_sec)*1000000 + (end.tv_usec - st.tv_usec));
#endif

p_err:
	return ret;
}

int sensor_sc201_cis_adjust_frame_duration(struct v4l2_subdev *subdev,
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

#ifdef DEBUG_SENSOR_TIME
	struct timeval st, end;
	do_gettimeofday(&st);
#endif

	FIMC_BUG(!subdev);
	FIMC_BUG(!target_duration);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);

	FIMC_BUG(!cis);
	FIMC_BUG(!cis->cis_data);

	cis_data = cis->cis_data;

	if (input_exposure_time == 0) {
		input_exposure_time  = cis_data->min_frame_us_time;
		info("[%s] Not proper exposure time(0), so apply min frame duration to exposure time forcely!!!(%d)\n",
			__func__, cis_data->min_frame_us_time);
	}

	vt_pix_clk_freq_khz = cis_data->pclk / 1000;
	line_length_pck = cis_data->line_length_pck;
	frame_length_lines = (u32)((vt_pix_clk_freq_khz * input_exposure_time) / (line_length_pck * 1000));
	frame_length_lines += cis_data->max_margin_coarse_integration_time;

	max_frame_us_time = 1000000 / cis->min_fps;
	frame_duration = (u32)((u64)(frame_length_lines * line_length_pck) * 1000 / vt_pix_clk_freq_khz);

	dbg_sensor(2, "[%s](vsync cnt = %d) input exp(%d), adj duration - frame duraion(%d), min_frame_us(%d)\n",
			__func__, cis_data->sen_vsync_count, input_exposure_time, frame_duration, cis_data->min_frame_us_time);

	*target_duration = MAX(frame_duration, cis_data->min_frame_us_time);
	if(cis->min_fps == cis->max_fps) {
		*target_duration = MIN(frame_duration, max_frame_us_time);
	}

	dbg_sensor(2, "[%s] requested min_fps(%d), max_fps(%d) from HAL, calculated frame_duration(%d), adjusted frame_duration(%d)\n", __func__, cis->min_fps, cis->max_fps, frame_duration, *target_duration);

#ifdef DEBUG_SENSOR_TIME
	do_gettimeofday(&end);
	dbg_sensor(2, "[%s] time %lu us\n", __func__, (end.tv_sec - st.tv_sec)*1000000 + (end.tv_usec - st.tv_usec));
#endif

	return ret;
}

int sensor_sc201_cis_set_frame_duration(struct v4l2_subdev *subdev, u32 frame_duration)
{
	int ret = 0;
	struct is_cis *cis;
	struct i2c_client *client;
	cis_shared_data *cis_data;
	struct is_core *core;

	u64 vt_pix_clk_freq_khz = 0;
	u32 line_length_pck = 0;
	u16 frame_length_lines = 0;

#ifdef DEBUG_SENSOR_TIME
	struct timeval st, end;
	do_gettimeofday(&st);
#endif

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

	core = (struct is_core *)dev_get_drvdata(is_dev);
	if (!core) {
		err("core device is null");
		ret = -EINVAL;
		return ret;
	}

	if (sensor_sc201_check_master_stream_off(core)) {
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

	if (frame_length_lines > SENSOR_SC201_MAX_FRAME_LENGTH_LINE_VALUE) {
		warn("%s: frame_length_lines is above the maximum value : 0x%04x (should be lower than 0x%04x)\n", __func__, frame_length_lines, SENSOR_SC201_MAX_FRAME_LENGTH_LINE_VALUE);
		frame_length_lines = SENSOR_SC201_MAX_FRAME_LENGTH_LINE_VALUE;
	}

	dbg_sensor(2, "[MOD:D:%d] %s, vt_pix_clk_freq_khz(%#x) frame_duration = %d us,"
		KERN_CONT "line_length_pck(%#x), frame_length_lines(%#x)\n",
		cis->id, __func__, vt_pix_clk_freq_khz, frame_duration, line_length_pck, frame_length_lines);

	I2C_MUTEX_LOCK(cis->i2c_lock);

	ret = is_sensor_write8(client, 0x320e, (frame_length_lines >> 8) & 0x7F);
	if (ret < 0)
		goto p_err;
	ret = is_sensor_write8(client, 0x320f, (frame_length_lines & 0xff));
	if (ret < 0)
		goto p_err;

	cis_data->cur_frame_us_time = frame_duration;

	cis_data->frame_length_lines = frame_length_lines;
	cis_data->max_coarse_integration_time = cis_data->frame_length_lines - cis_data->max_margin_coarse_integration_time;

#ifdef DEBUG_SENSOR_TIME
	do_gettimeofday(&end);
	dbg_sensor(2, "[%s] time %lu us\n", __func__, (end.tv_sec - st.tv_sec)*1000000 + (end.tv_usec - st.tv_usec));
#endif

p_err:
	I2C_MUTEX_UNLOCK(cis->i2c_lock);

	return ret;
}

int sensor_sc201_cis_set_frame_rate(struct v4l2_subdev *subdev, u32 min_fps)
{
	int ret = 0;
	struct is_cis *cis;
	cis_shared_data *cis_data;

	u32 frame_duration = 0;

#ifdef DEBUG_SENSOR_TIME
	struct timeval st, end;
	do_gettimeofday(&st);
#endif

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

	ret = sensor_sc201_cis_set_frame_duration(subdev, frame_duration);
	if (ret < 0) {
		err("[MOD:D:%d] %s, set frame duration is fail(%d)\n",
			cis->id, __func__, ret);
		goto p_err;
	}

	cis_data->min_frame_us_time = frame_duration;

#ifdef DEBUG_SENSOR_TIME
	do_gettimeofday(&end);
	dbg_sensor(2, "[%s] time %lu us\n", __func__, (end.tv_sec - st.tv_sec)*1000000 + (end.tv_usec - st.tv_usec));
#endif

p_err:

	return ret;
}

int sensor_sc201_cis_adjust_analog_gain(struct v4l2_subdev *subdev, u32 input_again, u32 *target_permile)
{
	int ret = 0;
	struct is_cis *cis;
	cis_shared_data *cis_data;

	u32 again_code = 0;
	u32 again_permile = 0;

#ifdef DEBUG_SENSOR_TIME
	struct timeval st, end;
	do_gettimeofday(&st);
#endif

	FIMC_BUG(!subdev);
	FIMC_BUG(!target_permile);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);

	FIMC_BUG(!cis);
	FIMC_BUG(!cis->cis_data);

	cis_data = cis->cis_data;

	again_code = sensor_cis_calc_again_code(input_again);

	if (again_code > cis_data->max_analog_gain[0]) {
		again_code = cis_data->max_analog_gain[0];
	} else if (again_code < cis_data->min_analog_gain[0]) {
		again_code = cis_data->min_analog_gain[0];
	}

	again_permile = sensor_cis_calc_again_permile(again_code);

	dbg_sensor(2, "[%s] min again(%d), max(%d), input_again(%d), code(%d), permile(%d)\n", __func__,
			cis_data->max_analog_gain[0],
			cis_data->min_analog_gain[0],
			input_again,
			again_code,
			again_permile);

	*target_permile = again_permile;

	return ret;
}

//For finding the nearest value in the gain table
u32 sensor_sc201_cis_calc_again_closest(u32 permile)
{
	int i;

	if (permile <= sensor_sc201_analog_gain[CODE_GAIN_INDEX][PERMILE_GAIN_INDEX])
		return sensor_sc201_analog_gain[CODE_GAIN_INDEX][PERMILE_GAIN_INDEX];
	if (permile >= sensor_sc201_analog_gain[MAX_GAIN_INDEX][PERMILE_GAIN_INDEX])
		return sensor_sc201_analog_gain[MAX_GAIN_INDEX][PERMILE_GAIN_INDEX];

	for (i = 0; i <= MAX_GAIN_INDEX; i++)
	{
		if (sensor_sc201_analog_gain[i][PERMILE_GAIN_INDEX] == permile)
			return sensor_sc201_analog_gain[i][PERMILE_GAIN_INDEX];

		if ((int)(permile - sensor_sc201_analog_gain[i][PERMILE_GAIN_INDEX]) < 0)
			return sensor_sc201_analog_gain[i-1][PERMILE_GAIN_INDEX];
	}

	if (i > MAX_GAIN_INDEX)
		return sensor_sc201_analog_gain[MAX_GAIN_INDEX][PERMILE_GAIN_INDEX];
	else
		return sensor_sc201_analog_gain[i][PERMILE_GAIN_INDEX];
}

u32 sensor_sc201_cis_calc_again_permile(u8 code)
{
	u32 ret = 0;
	int i;

	for (i = 0; i <= MAX_GAIN_INDEX; i++) {
		if (sensor_sc201_analog_gain[i][0] == code) {
			ret = sensor_sc201_analog_gain[i][1];
			break;
		}
	}

	return ret;
}

u32 sensor_sc201_cis_calc_again_code(u32 permile)
{
	u32 ret = 0, nearest_val = 0;
	int i;

	dbg_sensor(2, "[%s] permile %d\n", __func__, permile);
	nearest_val = sensor_sc201_cis_calc_again_closest(permile);
	dbg_sensor(2, "[%s] nearest_val %d\n", __func__, nearest_val);

	for (i = 0; i <= MAX_GAIN_INDEX; i++) {
		if (sensor_sc201_analog_gain[i][1] == nearest_val) {
			ret = sensor_sc201_analog_gain[i][0];
			break;
		}
	}

	return ret;
}

u32 sensor_sc201_cis_calc_dgain_code(u32 input_gain, u32 permile)
{
	u32 calc_value = 0;
	u32 digital_gain = 0;

	calc_value = input_gain * 1000 / permile;
	digital_gain = (calc_value * 1024) / 1000;

	dbg_sensor(2, "[%s] input_gain : %d, calc_value : %d, digital_gain : %d \n",
		__func__, input_gain, calc_value, digital_gain);

	return digital_gain;
}

u32 sensor_sc201_cis_calc_dgain_permile(u32 code, u32 fine_gain)
{
	return (code + 1) * 1000 + ((code + 1) * (fine_gain - 128) * 1000) / 128;
}

int sensor_sc201_cis_set_analog_digital_gain(struct v4l2_subdev *subdev, u32 input_gain)
{
	int ret = 0;
	struct is_cis *cis;
	struct i2c_client *client;
	cis_shared_data *cis_data;

	u32 total_again = 0;
	u32 total_dgain = 0;
	u32 ana_real_gain = 0;
	u32 total_fine_dgain = 0;

#ifdef DEBUG_SENSOR_TIME
	struct timeval st, end;
	do_gettimeofday(&st);
#endif

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

	if (input_gain < BASEGAIN)
		input_gain = BASEGAIN;
	else if (input_gain > 127 * BASEGAIN)
		input_gain = 127 * BASEGAIN;

	if((input_gain>=1 * BASEGAIN) && (input_gain < 2 * BASEGAIN)) {
		total_again = 0x00;
		total_dgain = 0x00;
		ana_real_gain = 1;
	}
	else if((input_gain>=2 * BASEGAIN) && (input_gain <4 * BASEGAIN)) {
		total_again = 0x01;
		total_dgain = 0x00;
		ana_real_gain = 2;
	}
	else if((input_gain >= 4 * BASEGAIN) && (input_gain <8 * BASEGAIN)) {
		total_again = 0x03;
		total_dgain = 0x00;
		ana_real_gain = 4;
	}
	else if((input_gain >= 8 * BASEGAIN) && (input_gain <16 * BASEGAIN)) {
		total_again = 0x07;
		total_dgain = 0x00;
		ana_real_gain = 8;
	}
	else if((input_gain >= 16 * BASEGAIN) && (input_gain <32 * BASEGAIN)) {
		total_again = 0x0f;
		total_dgain = 0x00;
		ana_real_gain = 16;
	}
	else if((input_gain >= 32 * BASEGAIN) && (input_gain <64 * BASEGAIN)) {
		total_again = 0x0f;
		total_dgain = 0x01;
		ana_real_gain = 32;
	}
	else if((input_gain >= 64 * BASEGAIN) && (input_gain <128 * BASEGAIN)) {
		total_again = 0x0f;
		total_dgain = 0x03;
		ana_real_gain = 64;
	}

	total_fine_dgain = (input_gain / ana_real_gain) * 128 / BASEGAIN;

	dbg_sensor(2, "[MOD:D:%d] %s, input_gain = %d us, total_again(%#x), total_dgain(%#x) ana_real_gain(%d) total_fine_dgain(%d)\n",
		cis->id, __func__, input_gain, total_again, total_dgain, ana_real_gain, total_fine_dgain);

	I2C_MUTEX_LOCK(cis->i2c_lock);

	ret = is_sensor_write8(client, 0x3e09, total_again);
	if (ret < 0)
		goto p_err;

	ret = is_sensor_write8(client, 0x3e06, total_dgain);
	if (ret < 0)
		goto p_err;

	ret = is_sensor_write8(client, 0x3e07, total_fine_dgain);
	if (ret < 0)
		goto p_err;

#ifdef DEBUG_SENSOR_TIME
	do_gettimeofday(&end);
	dbg_sensor(2, "[%s] time %lu us\n", __func__, (end.tv_sec - st.tv_sec)*1000000 + (end.tv_usec - st.tv_usec));
#endif

p_err:
	I2C_MUTEX_UNLOCK(cis->i2c_lock);

	return ret;
}

int sensor_sc201_cis_get_analog_gain(struct v4l2_subdev *subdev, u32 *again)
{
	int ret = 0;
	struct is_cis *cis;
	struct i2c_client *client;

	u8 analog_gain = 0;

#ifdef DEBUG_SENSOR_TIME
	struct timeval st, end;
	do_gettimeofday(&st);
#endif

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

	/* page_select */
	ret = is_sensor_write8(client, 0xfe, 0x00);
	if (ret < 0)
		goto p_err;

	ret = is_sensor_read8(client, 0xb6, &analog_gain);
	if (ret < 0)
		goto p_err;

	*again = sensor_sc201_cis_calc_again_permile(analog_gain);

	dbg_sensor(2, "[MOD:D:%d] %s, cur_again = %d us, analog_gain(%#x)\n",
			cis->id, __func__, *again, analog_gain);

#ifdef DEBUG_SENSOR_TIME
	do_gettimeofday(&end);
	dbg_sensor(2, "[%s] time %lu us\n", __func__, (end.tv_sec - st.tv_sec)*1000000 + (end.tv_usec - st.tv_usec));
#endif

p_err:
	I2C_MUTEX_UNLOCK(cis->i2c_lock);

	return ret;
}

int sensor_sc201_cis_get_min_analog_gain(struct v4l2_subdev *subdev, u32 *min_again)
{
	int ret = 0;
	struct is_cis *cis;
	struct i2c_client *client;
	cis_shared_data *cis_data;

	u16 read_value = 0;

#ifdef DEBUG_SENSOR_TIME
	struct timeval st, end;
	do_gettimeofday(&st);
#endif

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

	read_value = SENSOR_SC201_MIN_ANALOG_GAIN_SET_VALUE;

	cis_data->min_analog_gain[0] = read_value;

	cis_data->min_analog_gain[1] = sensor_sc201_cis_calc_again_permile(read_value);

	*min_again = cis_data->min_analog_gain[1];

	dbg_sensor(2, "[%s] code %d, permile %d\n", __func__,
		cis_data->min_analog_gain[0], cis_data->min_analog_gain[1]);

#ifdef DEBUG_SENSOR_TIME
	do_gettimeofday(&end);
	dbg_sensor(2, "[%s] time %lu us\n", __func__, (end.tv_sec - st.tv_sec)*1000000 + (end.tv_usec - st.tv_usec));
#endif

p_err:
	return ret;
}

int sensor_sc201_cis_get_max_analog_gain(struct v4l2_subdev *subdev, u32 *max_again)
{
	int ret = 0;
	struct is_cis *cis;
	struct i2c_client *client;
	cis_shared_data *cis_data;

	u8 read_value = 0;

#ifdef DEBUG_SENSOR_TIME
	struct timeval st, end;
	do_gettimeofday(&st);
#endif

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

	read_value = SENSOR_SC201_MAX_ANALOG_GAIN_SET_VALUE;

	cis_data->max_analog_gain[0] = read_value;

	cis_data->max_analog_gain[1] = sensor_sc201_cis_calc_again_permile(read_value);

	*max_again = cis_data->max_analog_gain[1];

	dbg_sensor(2, "[%s] code %d, permile %d\n", __func__,
		cis_data->max_analog_gain[0], cis_data->max_analog_gain[1]);

#ifdef DEBUG_SENSOR_TIME
	do_gettimeofday(&end);
	dbg_sensor(2, "[%s] time %lu us\n", __func__, (end.tv_sec - st.tv_sec)*1000000 + (end.tv_usec - st.tv_usec));
#endif

p_err:
	return ret;
}

int sensor_sc201_cis_get_min_digital_gain(struct v4l2_subdev *subdev, u32 *min_dgain)
{
	int ret = 0;
	struct is_cis *cis;
	struct i2c_client *client;
	cis_shared_data *cis_data;

#ifdef DEBUG_SENSOR_TIME
	struct timeval st, end;
	do_gettimeofday(&st);
#endif

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

	cis_data->min_digital_gain[0] = SENSOR_SC201_MIN_DIGITAL_GAIN_SET_VALUE;
	cis_data->min_digital_gain[1] = sensor_sc201_cis_calc_dgain_permile(cis_data->min_digital_gain[0], SENSOR_SC201_MIN_DIGITAL_FINE_GAIN_SET_VALUE);

	*min_dgain = cis_data->min_digital_gain[1];

	dbg_sensor(2, "[%s] code %d, permile %d\n", __func__,
		cis_data->min_digital_gain[0], cis_data->min_digital_gain[1]);

#ifdef DEBUG_SENSOR_TIME
	do_gettimeofday(&end);
	dbg_sensor(2, "[%s] time %lu us\n", __func__, (end.tv_sec - st.tv_sec)*1000000 + (end.tv_usec - st.tv_usec));
#endif

p_err:
	return ret;
}

int sensor_sc201_cis_get_max_digital_gain(struct v4l2_subdev *subdev, u32 *max_dgain)
{
	int ret = 0;
	struct is_cis *cis;
	struct i2c_client *client;
	cis_shared_data *cis_data;

#ifdef DEBUG_SENSOR_TIME
	struct timeval st, end;
	do_gettimeofday(&st);
#endif

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

	cis_data->max_digital_gain[0] = SENSOR_SC201_MAX_DIGITAL_GAIN_SET_VALUE;
	cis_data->max_digital_gain[1] = sensor_sc201_cis_calc_dgain_permile(cis_data->max_digital_gain[0], SENSOR_SC201_MAX_DIGITAL_FINE_GAIN_SET_VALUE);

	*max_dgain = cis_data->max_digital_gain[1];

	dbg_sensor(2, "[%s] code %d, permile %d\n", __func__,
		cis_data->max_digital_gain[0], cis_data->max_digital_gain[1]);

#ifdef DEBUG_SENSOR_TIME
	do_gettimeofday(&end);
	dbg_sensor(2, "[%s] time %lu us\n", __func__, (end.tv_sec - st.tv_sec)*1000000 + (end.tv_usec - st.tv_usec));
#endif

p_err:
	return ret;
}

int sensor_sc201_cis_set_totalgain(struct v4l2_subdev *subdev, struct ae_param *target_exposure,
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
	u32 input_gain = 0;

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

	core = (struct is_core *)dev_get_drvdata(is_dev);
	if (!core) {
		err("core device is null");
		ret = -EINVAL;
		goto p_err;
	}

	if (sensor_sc201_check_master_stream_off(core)) {
		dbg_sensor(2, "%s: Master cam did not enter in stream_on yet. Stop updating exposure time", __func__);
		return ret;
	}

	dbg_sensor(2, "[MOD:D:%d] %s, vsync_cnt(%d), target long(%d), again(%d) dgain(%d)\n", cis->id, __func__,
			cis_data->sen_vsync_count, target_exposure->long_val, again->val, dgain->val);

	vt_pix_clk_freq_khz = cis_data->pclk / 1000;
	line_length_pck = cis_data->line_length_pck;
	min_fine_int = cis_data->min_fine_integration_time;

	dbg_sensor(2, "[MOD:D:%d] %s, vt_pix_clk_freq_khz (%d), line_length_pck(%d), min_fine_int (%d)\n",
		cis->id, __func__, vt_pix_clk_freq_khz, line_length_pck, min_fine_int);

	origin_exp = (u16)(((target_exposure->long_val * vt_pix_clk_freq_khz) - min_fine_int) / (line_length_pck * 1000));

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
	ret = sensor_sc201_cis_set_exposure_time(subdev, origin_exp);
	if (ret < 0) {
		err("[%s] sensor_sc201_cis_set_exposure_time fail\n", __func__);
		goto p_err;
	}

	/* Set Analog & Digital gains */
	input_gain = again->val * dgain->val;
	ret = sensor_sc201_cis_set_analog_digital_gain(subdev, input_gain);
	if (ret < 0) {
		err("[%s] sensor_sc201_cis_set_analog_digital_gain fail\n", __func__);
		goto p_err;
	}

	dbg_sensor(2, "[MOD:D:%d] %s, frame_length_lines:%d(%#x), origin_exp:%d(%#x), input_gain:%d(%#x)\n",
		cis->id, __func__, cis_data->frame_length_lines, cis_data->frame_length_lines,
		origin_exp, origin_exp, input_gain, input_gain);

p_err:

	return ret;
}

int sensor_sc201_cis_wait_streamon(struct v4l2_subdev *subdev)
{
	int ret = 0;
	u32 poll_time_ms = 0;
	struct is_cis *cis;
	struct i2c_client *client;
	cis_shared_data *cis_data;
	u8 sensor_fcount_msb = 0, sensor_fcount_lsb = 0;
	u16 cur_frame_value = 0;
	u16 next_frame_value = 0;
	u8 sensor_frame_length_lines_msb = 0, sensor_frame_length_lines_lsb = 0;
	u8 sensor_line_length_pck_msb = 0, sensor_line_length_pck_lsb = 0;
	u16 sensor_frame_length_lines_value = 0;
	u16 sensor_line_length_pck_value = 0;
	long sensor_streamon_delay = 0;

	FIMC_BUG(!subdev);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);
	if (unlikely(!cis)) {
		err("cis is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	cis_data = cis->cis_data;
	if (unlikely(!cis_data)) {
		err("cis_data is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	client = cis->client;
	if (unlikely(!client)) {
		err("client is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	ret = is_sensor_read8(client, 0x320e, &sensor_frame_length_lines_msb);
	if (ret < 0) {
		err("i2c transfer fail addr(%x), val(%x), ret = %d\n", 0x320e, sensor_frame_length_lines_msb, ret);
		goto p_err;
	}

	ret = is_sensor_read8(client, 0x320f, &sensor_frame_length_lines_lsb);
	if (ret < 0) {
		err("i2c transfer fail addr(%x), val(%x), ret = %d\n", 0x320f, sensor_frame_length_lines_lsb, ret);
		goto p_err;
	}

	sensor_frame_length_lines_value = (sensor_frame_length_lines_msb << 8) | sensor_frame_length_lines_lsb;

	ret = is_sensor_read8(client, 0x320c, &sensor_line_length_pck_msb);
	if (ret < 0) {
		err("i2c transfer fail addr(%x), val(%x), ret = %d\n", 0x320c, sensor_line_length_pck_msb, ret);
		goto p_err;
	}

	ret = is_sensor_read8(client, 0x320d, &sensor_line_length_pck_lsb);
	if (ret < 0) {
		err("i2c transfer fail addr(%x), val(%x), ret = %d\n", 0x320d, sensor_line_length_pck_lsb, ret);
		goto p_err;
	}

	sensor_line_length_pck_value = (sensor_line_length_pck_msb << 8) | sensor_line_length_pck_lsb;

	sensor_streamon_delay = ((long) sensor_frame_length_lines_value * (long) sensor_line_length_pck_value * 14) / 1000000 + 1;      //pclk period = 14 ns

	/* wait for one frame */
	msleep(sensor_streamon_delay);
	info("[sc201] WaitStreamOn delay = %d ms \n", sensor_streamon_delay);

	ret = is_sensor_read8(client, 0x3976, &sensor_fcount_lsb);
	if (ret < 0) {
		err("i2c transfer fail addr(%x), val(%x), ret = %d\n", 0x3976, sensor_fcount_lsb, ret);
		goto p_err;
	}
	cur_frame_value = (sensor_fcount_msb << 8) | sensor_fcount_lsb;

	/* Checking stream on */
	do {
		ret = is_sensor_read8(client, 0x3976, &sensor_fcount_lsb);
		if (ret < 0) {
			err("i2c transfer fail addr(%x), val(%x), ret = %d\n", 0x3976, sensor_fcount_lsb, ret);
			goto p_err;
		}

		next_frame_value = (sensor_fcount_msb << 8) | sensor_fcount_lsb;
		if (next_frame_value != cur_frame_value)
			break;

		cur_frame_value = next_frame_value;
		usleep_range(POLL_TIME_US, POLL_TIME_US + 10);
		poll_time_ms += POLL_TIME_MS;

		dbg_sensor(2, "[MOD:D:%d] %s, log sensor_fcount(%d), (poll_time_ms(%d) < STREAM_ON_POLL_TIME_MS(%d))\n",
				cis->id, __func__, cur_frame_value, poll_time_ms, STREAM_ON_POLL_TIME_MS);
	} while (poll_time_ms < STREAM_ON_POLL_TIME_MS);

	if (poll_time_ms < STREAM_ON_POLL_TIME_MS)
		info("%s: finished after %d ms\n", __func__, poll_time_ms);
	else
		warn("%s: finished : polling timeout occured after %d ms\n", __func__, poll_time_ms);

p_err:
	return ret;
}

int sensor_sc201_cis_wait_streamoff(struct v4l2_subdev *subdev)
{
	int ret = 0;
	u32 poll_time_ms = 0;
	struct is_cis *cis;
	struct i2c_client *client;
	cis_shared_data *cis_data;
	u8 sensor_blc_data_high, sensor_blc_data_mid, sensor_blc_data_low, streamoff;

	FIMC_BUG(!subdev);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);
	if (unlikely(!cis)) {
		err("cis is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	cis_data = cis->cis_data;
	if (unlikely(!cis_data)) {
		err("cis_data is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	client = cis->client;
	if (unlikely(!client)) {
		err("client is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	ret = is_sensor_read8(client, 0x0100, &streamoff);
	if (ret < 0) {
		err("i2c transfer fail addr(%x), val(%x), ret = %d\n", 0x0100, streamoff, ret);
		goto p_err;
	}

	/* Checking stream off */
	do {
		ret = is_sensor_read8(client, 0x3974, &sensor_blc_data_high);
		if (ret < 0) {
			err("i2c transfer fail addr(%x), val(%x), ret = %d\n", 0x3974, sensor_blc_data_high, ret);
			goto p_err;
		}

		sensor_blc_data_high = (sensor_blc_data_high & 0x3F);

		ret = is_sensor_read8(client, 0x3975, &sensor_blc_data_mid);
		if (ret < 0) {
			err("i2c transfer fail addr(%x), val(%x), ret = %d\n", 0x3975, sensor_blc_data_mid, ret);
			goto p_err;
		}

		ret = is_sensor_read8(client, 0x3976, &sensor_blc_data_low);
		if (ret < 0) {
			err("i2c transfer fail addr(%x), val(%x), ret = %d\n", 0x3976, sensor_blc_data_low, ret);
			goto p_err;
		}

		sensor_blc_data_low = (sensor_blc_data_low & 0x0F);

		if ((sensor_blc_data_high | sensor_blc_data_mid | sensor_blc_data_low) == 0x00) {
			break;
		}

		usleep_range(POLL_TIME_US, POLL_TIME_US + 10);
		poll_time_ms += POLL_TIME_MS;

		dbg_sensor(1, "[MOD:D:%d] %s, streamoff(%d), (poll_time_ms(%d) < STREAM_OFF_POLL_TIME_MS(%d))\n",
				cis->id, __func__, streamoff, poll_time_ms, STREAM_OFF_POLL_TIME_MS);
	} while (poll_time_ms < STREAM_OFF_POLL_TIME_MS);

	if (poll_time_ms < STREAM_OFF_POLL_TIME_MS)
		info("%s: finished after %d ms\n", __func__, poll_time_ms);
	else
		warn("%s: finished : polling timeout occured after %d ms\n", __func__, poll_time_ms);

p_err:
	return ret;
}

static struct is_cis_ops cis_ops = {
	.cis_init = sensor_sc201_cis_init,
	.cis_log_status = sensor_sc201_cis_log_status,
	.cis_set_global_setting = sensor_sc201_cis_set_global_setting,
	.cis_mode_change = sensor_sc201_cis_mode_change,
	.cis_set_size = sensor_sc201_cis_set_size,
	.cis_stream_on = sensor_sc201_cis_stream_on,
	.cis_stream_off = sensor_sc201_cis_stream_off,
	.cis_set_exposure_time = NULL,
	.cis_get_min_exposure_time = sensor_sc201_cis_get_min_exposure_time,
	.cis_get_max_exposure_time = sensor_sc201_cis_get_max_exposure_time,
	.cis_adjust_frame_duration = sensor_sc201_cis_adjust_frame_duration,
	.cis_set_frame_duration = sensor_sc201_cis_set_frame_duration,
	.cis_set_frame_rate = sensor_sc201_cis_set_frame_rate,
	// .cis_adjust_analog_gain = sensor_sc201_cis_adjust_analog_gain,
	.cis_set_analog_gain = NULL,
	// .cis_get_analog_gain = sensor_sc201_cis_get_analog_gain,
	.cis_get_min_analog_gain = sensor_sc201_cis_get_min_analog_gain,
	.cis_get_max_analog_gain = sensor_sc201_cis_get_max_analog_gain,
	.cis_set_digital_gain = NULL,
	// .cis_get_digital_gain = sensor_sc201_cis_get_digital_gain,
	.cis_get_min_digital_gain = sensor_sc201_cis_get_min_digital_gain,
	.cis_get_max_digital_gain = sensor_sc201_cis_get_max_digital_gain,
	// .cis_compensate_gain_for_extremely_br = sensor_cis_compensate_gain_for_extremely_br,
	.cis_wait_streamoff = sensor_sc201_cis_wait_streamoff,
	.cis_wait_streamon = sensor_sc201_cis_wait_streamon,
	// .cis_check_rev_on_init = sensor_sc201_cis_check_rev,
	.cis_check_rev_on_init = sensor_cis_check_rev_on_init,
	.cis_set_totalgain = sensor_sc201_cis_set_totalgain,
};

int cis_sc201_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	int ret = 0;
	struct is_core *core = NULL;
	struct v4l2_subdev *subdev_cis = NULL;
	struct is_cis *cis = NULL;
	struct is_device_sensor *device = NULL;
	struct is_device_sensor_peri *sensor_peri = NULL;
	struct device *dev;
	struct device_node *dnode;
	u32 sensor_id = 0;
	char const *setfile;

#if defined(CONFIG_VENDER_MCD_V2) || defined(CONFIG_CAMERA_OTPROM_SUPPORT_FRONT) || defined(CONFIG_CAMERA_OTPROM_SUPPORT_REAR)
	struct is_vender_specific *specific = NULL;
	u32 rom_position = 0;
#endif

	FIMC_BUG(!client);
	FIMC_BUG(!is_dev);

	core = (struct is_core *)dev_get_drvdata(is_dev);
	if (!core) {
		probe_info("core device is not yet probed");
		return -EPROBE_DEFER;
	}

	dev = &client->dev;
	dnode = dev->of_node;

	ret = of_property_read_u32(dnode, "id", &sensor_id);
	if (ret) {
		err("sensor_id read is fail(%d)", ret);
		goto p_err;
	}

	probe_info("%s sensor_id %d\n", __func__, sensor_id);

	device = &core->sensor[sensor_id];

	sensor_peri = find_peri_by_cis_id(device, SENSOR_NAME_SC201);
	if (!sensor_peri) {
		probe_info("sensor peri is not yet probed");
		return -EPROBE_DEFER;
	}

	cis = &sensor_peri->cis;
	if (!cis) {
		err("cis is NULL");
		ret = -ENOMEM;
		goto p_err;
	}

	subdev_cis = kzalloc(sizeof(struct v4l2_subdev), GFP_KERNEL);
	if (!subdev_cis) {
		probe_err("subdev_cis is NULL");
		ret = -ENOMEM;
		goto p_err;
	}
	sensor_peri->subdev_cis = subdev_cis;

	cis->id = SENSOR_NAME_SC201;
	cis->subdev = subdev_cis;
	cis->device = 0;
	cis->client = client;
	sensor_peri->module->client = cis->client;

#if defined(CONFIG_VENDER_MCD_V2)
	if (of_property_read_bool(dnode, "use_sensor_otp")) {
		ret = of_property_read_u32(dnode, "rom_position", &rom_position);
		if (ret) {
			err("sensor_id read is fail(%d)", ret);
		} else {
			specific = core->vender.private_data;
			specific->rom_client[rom_position] = cis->client;
			specific->rom_data[rom_position].rom_type = ROM_TYPE_OTPROM;
			specific->rom_data[rom_position].rom_valid = true;

			if (vender_rom_addr[rom_position]) {
				specific->rom_cal_map_addr[rom_position] = vender_rom_addr[rom_position];
				probe_info("%s: rom_id=%d, OTP Registered\n", __func__, rom_position);
			} else {
				probe_info("%s: SC201 OTP addrress not defined!\n", __func__);
			}
		}
	}
#else
#if defined(CONFIG_VENDER_MCD) && defined(CONFIG_CAMERA_OTPROM_SUPPORT_FRONT)
	rom_position = 0;
	specific = core->vender.private_data;
	specific->front_cis_client = client;
#endif
#if defined(CONFIG_VENDER_MCD) && defined(CONFIG_CAMERA_OTPROM_SUPPORT_REAR)
	rom_position = 0;
	specific = core->vender.private_data;
	specific->rear_cis_client = client;
#endif
#endif

	cis->ctrl_delay = N_PLUS_TWO_FRAME;

	cis->cis_data = kzalloc(sizeof(cis_shared_data), GFP_KERNEL);
	if (!cis->cis_data) {
		err("cis_data is NULL");
		ret = -ENOMEM;
		goto p_err;
	}

	cis->cis_ops = &cis_ops;

	/* belows are depend on sensor cis. MUST check sensor spec */
#ifdef APPLY_MIRROR_VERTICAL_FLIP
	cis->bayer_order = OTF_INPUT_ORDER_BAYER_BG_GR;
#else
	cis->bayer_order = OTF_INPUT_ORDER_BAYER_RG_GB;
#endif

	if (of_property_read_bool(dnode, "sensor_f_number")) {
		ret = of_property_read_u32(dnode, "sensor_f_number", &cis->aperture_num);
		if (ret)
			warn("f-number read is fail(%d)", ret);
	} else {
		cis->aperture_num = F2_2;
	}

	probe_info("%s f-number %d\n", __func__, cis->aperture_num);

	cis->use_dgain = true;
	cis->hdr_ctrl_by_again = false;
	cis->use_total_gain = true;

	cis->use_initial_ae = of_property_read_bool(dnode, "use_initial_ae");
	probe_info("%s use initial_ae(%d)\n", __func__, cis->use_initial_ae);

	ret = of_property_read_string(dnode, "setfile", &setfile);
	if (ret) {
		err("setfile index read fail(%d), take default setfile!!", ret);
		setfile = "default";
	}

	if (strcmp(setfile, "default") == 0 || strcmp(setfile, "setA") == 0) {
		probe_info("%s setfile_A\n", __func__);
		sensor_sc201_global = sensor_sc201_setfile_A_Global;
		sensor_sc201_global_size = ARRAY_SIZE(sensor_sc201_setfile_A_Global);
		sensor_sc201_setfiles = sensor_sc201_setfiles_A;
		sensor_sc201_setfile_sizes = sensor_sc201_setfile_A_sizes;
		sensor_sc201_pllinfos = sensor_sc201_pllinfos_A;
		sensor_sc201_max_setfile_num = ARRAY_SIZE(sensor_sc201_setfiles_A);
		sensor_sc201_fsync_master = sensor_sc201_setfile_A_Fsync_Master;
		sensor_sc201_fsync_master_size = ARRAY_SIZE(sensor_sc201_setfile_A_Fsync_Master);
		sensor_sc201_fsync_slave = sensor_sc201_setfile_A_Fsync_Slave;
		sensor_sc201_fsync_slave_size = ARRAY_SIZE(sensor_sc201_setfile_A_Fsync_Slave);
	} else if (strcmp(setfile, "setB") == 0) {
		probe_info("%s setfile_B\n", __func__);
		sensor_sc201_global = sensor_sc201_setfile_B_Global;
		sensor_sc201_global_size = ARRAY_SIZE(sensor_sc201_setfile_B_Global);
		sensor_sc201_setfiles = sensor_sc201_setfiles_B;
		sensor_sc201_setfile_sizes = sensor_sc201_setfile_B_sizes;
		sensor_sc201_pllinfos = sensor_sc201_pllinfos_B;
		sensor_sc201_max_setfile_num = ARRAY_SIZE(sensor_sc201_setfiles_B);
		sensor_sc201_fsync_master = sensor_sc201_setfile_B_Fsync_Master;
		sensor_sc201_fsync_master_size = ARRAY_SIZE(sensor_sc201_setfile_B_Fsync_Master);
		sensor_sc201_fsync_slave = sensor_sc201_setfile_B_Fsync_Slave;
		sensor_sc201_fsync_slave_size = ARRAY_SIZE(sensor_sc201_setfile_B_Fsync_Slave);
	} else {
		err("%s setfile index out of bound, take default (setfile_A)", __func__);
		sensor_sc201_global = sensor_sc201_setfile_A_Global;
		sensor_sc201_global_size = ARRAY_SIZE(sensor_sc201_setfile_A_Global);
		sensor_sc201_setfiles = sensor_sc201_setfiles_A;
		sensor_sc201_setfile_sizes = sensor_sc201_setfile_A_sizes;
		sensor_sc201_pllinfos = sensor_sc201_pllinfos_A;
		sensor_sc201_max_setfile_num = ARRAY_SIZE(sensor_sc201_setfiles_A);
		sensor_sc201_fsync_master = sensor_sc201_setfile_A_Fsync_Master;
		sensor_sc201_fsync_master_size = ARRAY_SIZE(sensor_sc201_setfile_A_Fsync_Master);
		sensor_sc201_fsync_slave = sensor_sc201_setfile_A_Fsync_Slave;
		sensor_sc201_fsync_slave_size = ARRAY_SIZE(sensor_sc201_setfile_A_Fsync_Slave);
	}

	v4l2_i2c_subdev_init(subdev_cis, client, &subdev_ops);
	v4l2_set_subdevdata(subdev_cis, cis);
	v4l2_set_subdev_hostdata(subdev_cis, device);
	snprintf(subdev_cis->name, V4L2_SUBDEV_NAME_SIZE, "cis-subdev.%d", cis->id);

	probe_info("%s done\n", __func__);

p_err:
	return ret;
}

static const struct of_device_id sensor_cis_sc201_match[] = {
	{
		.compatible = "samsung,exynos-is-cis-sc201",
	},
	{
		.compatible = "samsung,exynos-is-cis-sc201-macro",
	},
	{},
};
MODULE_DEVICE_TABLE(of, sensor_cis_sc201_match);

static const struct i2c_device_id sensor_cis_sc201_idt[] = {
	{ SENSOR_NAME, 0 },
	{},
};

static struct i2c_driver sensor_cis_sc201_driver = {
	.probe	= cis_sc201_probe,
	.driver = {
		.name	= SENSOR_NAME,
		.owner	= THIS_MODULE,
		.of_match_table = sensor_cis_sc201_match,
		.suppress_bind_attrs = true,
	},
	.id_table = sensor_cis_sc201_idt,
};

static int __init sensor_cis_sc201_init(void)
{
	int ret;

	ret = i2c_add_driver(&sensor_cis_sc201_driver);
	if (ret)
		err("failed to add %s driver: %d\n",
			sensor_cis_sc201_driver.driver.name, ret);

	return ret;
}
late_initcall_sync(sensor_cis_sc201_init);
MODULE_LICENSE("GPL");
