/*
 * Samsung Exynos5 SoC series Sensor driver
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
#include "is-cis-3l6.h"
#include "is-cis-3l6-setA.h"
#include "is-cis-3l6-12M-setA-19p2.h"

#include "is-helper-i2c.h"

#define SENSOR_NAME "S5K3L6"
/* #define DEBUG_3L6_PLL */

/* Value at address 0x344 in set file*/
#define CURR_X_INDEX_3L6 ((12 * 3) + 1)
/* Value at address 0x346 in set file*/
#define CURR_Y_INDEX_3L6 ((14 * 3) + 1)

#define CALIBRATE_CUR_X_3L6 48
#define CALIBRATE_CUR_Y_3L6 20

static const struct v4l2_subdev_ops subdev_ops;

static const u32 *sensor_3l6_global;
static u32 sensor_3l6_global_size;
static const u32 **sensor_3l6_setfiles;
static const u32 *sensor_3l6_setfile_sizes;
static u32 sensor_3l6_max_setfile_num;
static const struct sensor_pll_info_compact **sensor_3l6_pllinfos;

/* For checking frame count */
static u32 sensor_3l6_fcount;

static void sensor_3l6_cis_data_calculation(const struct sensor_pll_info_compact *pll_info, cis_shared_data *cis_data)
{
	u64 vt_pix_clk_hz = 0;
	u32 frame_rate = 0, max_fps = 0, frame_valid_us = 0;

	WARN_ON(!pll_info);

	/* 1. pixel rate calculation (Mpps) */
	vt_pix_clk_hz = pll_info->pclk;

	/* 2. the time of processing one frame calculation (us) */
	cis_data->min_frame_us_time = (((u64)pll_info->frame_length_lines) * pll_info->line_length_pck * 1000
					/ (vt_pix_clk_hz / 1000));

	cis_data->cur_frame_us_time = cis_data->min_frame_us_time;

#ifdef CAMERA_REAR2
	cis_data->min_sync_frame_us_time = cis_data->min_frame_us_time;
#endif

	/* 3. FPS calculation */
	frame_rate = vt_pix_clk_hz / (pll_info->frame_length_lines * pll_info->line_length_pck);
	dbg_sensor(1, "frame_rate (%d) = vt_pix_clk_hz(%d) / "
		"(pll_info_compact->frame_length_lines(%u) * pll_info_compact->line_length_pck(%u))\n",
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

	dbg_sensor(1, "[3L6 data calculation] Sensor size(%d x %d) setting SUCCESS!\n",
				cis_data->cur_width, cis_data->cur_height);
	dbg_sensor(1, "[3L6 data calculation] Frame Valid(%d us)\n", frame_valid_us);
	dbg_sensor(1, "[3L6 data calculation] rolling_shutter_skew(%lld)\n", cis_data->rolling_shutter_skew);
	dbg_sensor(1, "[3L6 data calculation] fps(%d), max fps(%d)\n", frame_rate, cis_data->max_fps);
	dbg_sensor(1, "[3L6 data calculation] min_frame_time(%d us)\n", cis_data->min_frame_us_time);
	dbg_sensor(1, "[3L6 data calculation] pixel rate (%d Kbps)\n", cis_data->pclk / 1000);

	/* Frame period calculation */
	cis_data->frame_time = (cis_data->line_readOut_time * cis_data->cur_height / 1000);
	cis_data->rolling_shutter_skew = (cis_data->cur_height - 1) * cis_data->line_readOut_time;

	dbg_sensor(1, "[3L6 data calculation] frame_time(%d), rolling_shutter_skew(%lld)\n",
		cis_data->frame_time, cis_data->rolling_shutter_skew);

	/* Constant values */
	cis_data->min_fine_integration_time = SENSOR_3L6_FINE_INTEGRATION_TIME_MIN;
	cis_data->max_fine_integration_time = SENSOR_3L6_FINE_INTEGRATION_TIME_MAX;
	cis_data->min_coarse_integration_time = SENSOR_3L6_COARSE_INTEGRATION_TIME_MIN;
	cis_data->max_margin_coarse_integration_time = SENSOR_3L6_COARSE_INTEGRATION_TIME_MAX_MARGIN;
}

void sensor_3l6_cis_data_calc(struct v4l2_subdev *subdev, u32 mode)
{
	struct is_cis *cis = sensor_cis_get_cis(subdev);

	if (mode >= sensor_3l6_max_setfile_num) {
		err("invalid mode(%d)!!", mode);
		return;
	}

	sensor_3l6_cis_data_calculation(sensor_3l6_pllinfos[mode], cis->cis_data);
}

static int sensor_3l6_wait_stream_off_status(cis_shared_data *cis_data)
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

/* CIS OPS */
int sensor_3l6_cis_init(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_cis *cis = sensor_cis_get_cis(subdev);
	u32 setfile_index = 0;

	cis->cis_data->cur_width = SENSOR_3L6_MAX_WIDTH;
	cis->cis_data->cur_height = SENSOR_3L6_MAX_HEIGHT;
	cis->cis_data->cur_pattern_mode = SENSOR_TEST_PATTERN_MODE_OFF;
	cis->cis_data->low_expo_start = 33000;
	cis->need_mode_change = false;
	sensor_3l6_cis_data_calculation(sensor_3l6_pllinfos[setfile_index], cis->cis_data);

	return ret;
}

static const struct is_cis_log log_3l6[] = {
	{I2C_READ, 16, SENSOR_3L6_MODEL_ID_ADDR, 0, "model_id"},
	{I2C_READ, 8, SENSOR_3L6_REVISION_NUMBER_ADDR, 0, "rev_number"},
	{I2C_READ, 8, SENSOR_3L6_FRAME_COUNT_ADDR, 0, "frame_count"},
	{I2C_READ, 8, SENSOR_3L6_MODE_SELECT_ADDR, 0, "mode_select"},
	{I2C_READ, 16, SENSOR_3L6_FRAME_LENGTH_LINE_ADDR, 0, "fll"},
	{I2C_READ, 16, SENSOR_3L6_COARSE_INTEGRATION_TIME_ADDR, 0, "cit"},
	{I2C_READ, 16, 0x3402, 0, "bpc"},
};

int sensor_3l6_cis_log_status(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_cis *cis = sensor_cis_get_cis(subdev);

	sensor_cis_log_status(cis, cis->client, log_3l6,
			ARRAY_SIZE(log_3l6), (char *)__func__);

	return ret;
}

int sensor_3l6_cis_set_global_setting(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_cis *cis = sensor_cis_get_cis(subdev);

	I2C_MUTEX_LOCK(cis->i2c_lock);
	/* 3ms delay to operate sensor FW */
	usleep_range(8000, 8000);

	/* setfile global setting is at camera entrance */
	ret = sensor_cis_set_registers(subdev, sensor_3l6_global, sensor_3l6_global_size);
	if (ret < 0) {
		err("sensor_3l6_global setting fail!!");
		goto p_err_unlock;
	}

	dbg_sensor(1, "[%s] global setting done\n", __func__);

p_err_unlock:
	I2C_MUTEX_UNLOCK(cis->i2c_lock);
	return ret;
}

#if defined (USE_MS_PDAF)
static bool sensor_3l6_is_padf_enable(u32 mode)
{
	switch(mode)
	{
		case SENSOR_3L6_MODE_1280x720_120FPS:
		case SENSOR_3L6_MODE_1028x772_120FPS:
		case SENSOR_3L6_MODE_1024x768_120FPS:
			return false;
		default:
			return true;
	}
}
#endif

int sensor_3l6_cis_mode_change(struct v4l2_subdev *subdev, u32 mode)
{
	int ret = 0;
	u32 max_setfile_num = 0;
	struct is_cis *cis = sensor_cis_get_cis(subdev);

	max_setfile_num = sensor_3l6_max_setfile_num;

	if (mode > max_setfile_num) {
		err("invalid mode(%d)!!", mode);
		ret = -EINVAL;
		goto p_err;
	}

#if defined (USE_MS_PDAF)
	/*PDAF and binning are connected.
	if there is binning then padf is not applied.
	check the setfile xls for binning information.
	 */
	dbg_sensor(1, "USE_MS_PDAF: [%s] mode [%d]\n", __func__, mode);
	cis->use_pdaf = sensor_3l6_is_padf_enable(mode);
	if (cis->use_pdaf) {
		dbg_sensor(1, "USE_MS_PDAF: [%s] using pdaf\n", __func__);
		cis->cis_data->cur_pos_x = sensor_3l6_setfiles[mode][CURR_X_INDEX_3L6] - CALIBRATE_CUR_X_3L6;
		cis->cis_data->cur_pos_y = sensor_3l6_setfiles[mode][CURR_Y_INDEX_3L6] - CALIBRATE_CUR_Y_3L6;
	}
	else {
		dbg_sensor(1, "USE_MS_PDAF: [%s] NOT using pdaf\n", __func__);
		cis->cis_data->cur_pos_x = 0;
		cis->cis_data->cur_pos_y = 0;
	}
	dbg_sensor(1, "USE_MS_PDAF: [%s] cur_pos_x [%d] cur_pos_y [%d] \n", __func__, cis->cis_data->cur_pos_x, cis->cis_data->cur_pos_y);
#endif

	I2C_MUTEX_LOCK(cis->i2c_lock);
	ret = sensor_cis_set_registers(subdev, sensor_3l6_setfiles[mode], sensor_3l6_setfile_sizes[mode]);
	if (ret < 0) {
		err("mode(%d) setting fail!!", mode);
		goto p_err_unlock;
	}

	if (!cis->use_pdaf)
		is_sensor_write16(cis->client, 0x3402, 0x4E46); /* enable BPC */

	info("[%s] sensor mode changed(%d)\n", __func__, mode);

p_err_unlock:
	I2C_MUTEX_UNLOCK(cis->i2c_lock);

p_err:
	return ret;
}

/* TODO: Sensor set size sequence(sensor done, sensor stop, 3AA done in FW case */
int sensor_3l6_cis_set_size(struct v4l2_subdev *subdev, cis_shared_data *cis_data)
{
	int ret = 0;
	bool binning = false;
	u32 ratio_w = 0, ratio_h = 0, start_x = 0, start_y = 0, end_x = 0, end_y = 0;
	u32 even_x= 0, odd_x = 0, even_y = 0, odd_y = 0;
	struct is_cis *cis = sensor_cis_get_cis(subdev);
	ktime_t st = ktime_get();

	dbg_sensor(1, "[MOD:D:%d] %s\n", cis->id, __func__);

	/* Wait actual stream off */
	ret = sensor_3l6_wait_stream_off_status(cis_data);
	if (ret) {
		err("Must stream off");
		ret = -EINVAL;
		goto p_err;
	}

	binning = cis_data->binning;
	if (binning) {
		ratio_w = (SENSOR_3L6_MAX_WIDTH / cis_data->cur_width);
		ratio_h = (SENSOR_3L6_MAX_HEIGHT / cis_data->cur_height);
	} else {
		ratio_w = 1;
		ratio_h = 1;
	}

	if (((cis_data->cur_width * ratio_w) > SENSOR_3L6_MAX_WIDTH) ||
		((cis_data->cur_height * ratio_h) > SENSOR_3L6_MAX_HEIGHT)) {
		err("Config max sensor size over~!!");
		ret = -EINVAL;
		goto p_err;
	}

	I2C_MUTEX_LOCK(cis->i2c_lock);
	/* 1. page_select */
	ret = is_sensor_write16(cis->client, SENSOR_3L6_PAGE_SELECT_ADDR, 0x2000);
	if (ret < 0)
		goto p_err_unlock;

	/* 2. pixel address region setting */
	start_x = ((SENSOR_3L6_MAX_WIDTH - cis_data->cur_width * ratio_w) / 2) & (~0x1);
	start_y = ((SENSOR_3L6_MAX_HEIGHT - cis_data->cur_height * ratio_h) / 2) & (~0x1);
	end_x = start_x + (cis_data->cur_width * ratio_w - 1);
	end_y = start_y + (cis_data->cur_height * ratio_h - 1);

	if (!(end_x & (0x1)) || !(end_y & (0x1))) {
		err("Sensor pixel end address must odd");
		ret = -EINVAL;
		goto p_err_unlock;
	}

	ret = is_sensor_write16(cis->client, SENSOR_3L6_X_ADDR_START_ADDR, start_x);
	if (ret < 0)
		goto p_err_unlock;
	ret = is_sensor_write16(cis->client, SENSOR_3L6_Y_ADDR_START_ADDR, start_y);
	if (ret < 0)
		goto p_err_unlock;
	ret = is_sensor_write16(cis->client, SENSOR_3L6_X_ADDR_END_ADDR, end_x);
	if (ret < 0)
		goto p_err_unlock;
	ret = is_sensor_write16(cis->client, SENSOR_3L6_Y_ADDR_END_ADDR, end_y);
	if (ret < 0)
		goto p_err_unlock;

	/* 3. output address setting */
	ret = is_sensor_write16(cis->client, SENSOR_3L6_X_OUTPUT_SIZE_ADDR, cis_data->cur_width);
	if (ret < 0)
		goto p_err_unlock;
	ret = is_sensor_write16(cis->client, SENSOR_3L6_Y_OUTPUT_SIZE_ADDR, cis_data->cur_height);
	if (ret < 0)
		goto p_err_unlock;

	/* If not use to binning, sensor image should set only crop */
	if (!binning) {
		dbg_sensor(1, "Sensor size set is not binning\n");
		goto p_err_unlock;
	}

	/* 4. sub sampling setting */
	even_x = 1;	/* 1: not use to even sampling */
	even_y = 1;
	odd_x = (ratio_w * 2) - even_x;
	odd_y = (ratio_h * 2) - even_y;

	ret = is_sensor_write16(cis->client, SENSOR_3L6_X_EVEN_INC_ADDR, even_x);
	if (ret < 0)
		goto p_err_unlock;
	ret = is_sensor_write16(cis->client,  SENSOR_3L6_X_ODD_INC_ADDR, odd_x);
	if (ret < 0)
		goto p_err_unlock;
	ret = is_sensor_write16(cis->client, SENSOR_3L6_Y_EVEN_INC_ADDR, even_y);
	if (ret < 0)
		goto p_err_unlock;
	ret = is_sensor_write16(cis->client, SENSOR_3L6_Y_ODD_INC_ADDR, odd_y);
	if (ret < 0)
		goto p_err_unlock;

	/* 5. binnig setting */
	ret = is_sensor_write8(cis->client, SENSOR_3L6_BINNING_MODE_ADDR, binning);	/* 1:  binning enable, 0: disable */
	if (ret < 0)
		goto p_err_unlock;
	ret = is_sensor_write8(cis->client, SENSOR_3L6_BINNING_TYPE_ADDR, (ratio_w << 4) | ratio_h);
	if (ret < 0)
		goto p_err_unlock;

	/* 6. scaling setting: but not use */
	/* scaling_mode (0: No scaling, 1: Horizontal, 2: Full) */
	ret = is_sensor_write16(cis->client, SENSOR_3L6_SCALING_MODE_ADDR, 0x0000);
	if (ret < 0)
		goto p_err_unlock;
	/* down_scale_m: 1 to 16 upwards (scale_n: 16(fixed)) */
	/* down scale factor = down_scale_m / down_scale_n */
	ret = is_sensor_write16(cis->client, SENSOR_3L6_DOWN_SCALE_M_ADDR, 0x0010);
	if (ret < 0)
		goto p_err_unlock;

	cis_data->frame_time = (cis_data->line_readOut_time * cis_data->cur_height / 1000);
	cis->cis_data->rolling_shutter_skew = (cis->cis_data->cur_height - 1) * cis->cis_data->line_readOut_time;
	dbg_sensor(1, "[%s] frame_time(%d), rolling_shutter_skew(%lld)\n", __func__,
		cis->cis_data->frame_time, cis->cis_data->rolling_shutter_skew);

	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %ldus\n", __func__, PABLO_KTIME_US_DELTA_NOW(st));

p_err_unlock:
	I2C_MUTEX_UNLOCK(cis->i2c_lock);

p_err:
	return ret;
}

int sensor_3l6_cis_stream_on(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_cis *cis = sensor_cis_get_cis(subdev);
	cis_shared_data *cis_data;
	ktime_t st = ktime_get();

	cis_data = cis->cis_data;

	dbg_sensor(1, "[MOD:D:%d] %s\n", cis->id, __func__);
	I2C_MUTEX_LOCK(cis->i2c_lock);

#ifdef USE_CAMERA_MCD_SW_DUAL_SYNC
	if (cis->cis_data->is_data.scene_mode == AA_SCENE_MODE_LIVE_OUTFOCUS) {
		info("[%s] s/w dual sync slave\n", __func__);
		cis->dual_sync_mode = DUAL_SYNC_SLAVE;
		cis->dual_sync_type = DUAL_SYNC_TYPE_SW;
	} else {
		info("[%s] s/w dual sync none\n", __func__);
		cis->dual_sync_mode = DUAL_SYNC_NONE;
		cis->dual_sync_type = DUAL_SYNC_TYPE_MAX;
	}
#endif

	/* Sensor stream on */
	ret = is_sensor_write16(cis->client, SENSOR_3L6_PLL_POWER_CONTROL_ADDR, 0x0100);
	if (ret < 0)
		err("i2c_w_fail addr(%x), val(%x), ret = %d", SENSOR_3L6_PLL_POWER_CONTROL_ADDR, 0x0100, ret);

	usleep_range(3000, 3000);
	ret = is_sensor_write16(cis->client, SENSOR_3L6_MODE_SELECT_ADDR, 0x0100);
	if (ret < 0)
		err("i2c_w_fail addr(%x), val(%x), ret = %d", SENSOR_3L6_MODE_SELECT_ADDR, 0x0100, ret);

	usleep_range(3000, 3000);
	ret = is_sensor_write16(cis->client, SENSOR_3L6_PLL_POWER_CONTROL_ADDR, 0x0000);
	if (ret < 0)
		err("i2c_w_fail addr(%x), val(%x), ret = %d", SENSOR_3L6_PLL_POWER_CONTROL_ADDR, 0x0000, ret);

	usleep_range(3000, 3000);
	I2C_MUTEX_UNLOCK(cis->i2c_lock);

	cis_data->stream_on = true;
	sensor_3l6_fcount = 0;

	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %ldus\n", __func__, PABLO_KTIME_US_DELTA_NOW(st));

	return ret;
}

int sensor_3l6_cis_stream_off(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_cis *cis = sensor_cis_get_cis(subdev);
	cis_shared_data *cis_data;
	u8 cur_frame_count = 0;
	ktime_t st = ktime_get();

	cis_data = cis->cis_data;

	dbg_sensor(1, "[MOD:D:%d] %s\n", cis->id, __func__);

	I2C_MUTEX_LOCK(cis->i2c_lock);

	/* Sensor stream off */
	ret = is_sensor_read8(cis->client, SENSOR_3L6_FRAME_COUNT_ADDR, &cur_frame_count);
	if (ret < 0)
		err("i2c_r_fail addr(%x), val(%0x%x), ret = %d", SENSOR_3L6_FRAME_COUNT_ADDR, cur_frame_count, ret);
	info("%s: frame_count(0x%x)\n", __func__, cur_frame_count);
	sensor_3l6_fcount = cur_frame_count;

	ret = is_sensor_write16(cis->client, SENSOR_3L6_MODE_SELECT_ADDR, 0x0000);
	if (ret < 0)
		err("i2c_w_fail addr(%x), val(%x), ret = %d", SENSOR_3L6_MODE_SELECT_ADDR, 0x0000, ret);

	I2C_MUTEX_UNLOCK(cis->i2c_lock);

	cis_data->stream_on = false;

	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %ldus\n", __func__, PABLO_KTIME_US_DELTA_NOW(st));

	return ret;
}

int sensor_3l6_cis_set_exposure_time(struct v4l2_subdev *subdev, struct ae_param *target_exposure)
{
	int ret = 0;
	struct is_cis *cis = sensor_cis_get_cis(subdev);
	cis_shared_data *cis_data;

	u32 vt_pic_clk_freq_mhz = 0;
	u16 long_coarse_int = 0;
	u16 short_coarse_int = 0;
	u32 line_length_pck = 0;
	u32 min_fine_int = 0;
	ktime_t st = ktime_get();

	WARN_ON(!target_exposure);

	if ((target_exposure->long_val <= 0) || (target_exposure->short_val <= 0)) {
		err("invalid target exposure(%d, %d)",
			target_exposure->long_val, target_exposure->short_val);
		ret = -EINVAL;
		goto p_err;
	}

	cis_data = cis->cis_data;

	dbg_sensor(1, "[MOD:D:%d] %s, vsync_cnt(%d), target long(%d), short(%d)\n", cis->id, __func__,
			cis_data->sen_vsync_count, target_exposure->long_val, target_exposure->short_val);

	vt_pic_clk_freq_mhz = cis_data->pclk / (1000 * 1000);
	line_length_pck = cis_data->line_length_pck;
	min_fine_int = cis_data->min_fine_integration_time;

	long_coarse_int = ((target_exposure->long_val * vt_pic_clk_freq_mhz) - min_fine_int) / line_length_pck;
	short_coarse_int = ((target_exposure->short_val * vt_pic_clk_freq_mhz) - min_fine_int) / line_length_pck;

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

	if (long_coarse_int < cis_data->min_coarse_integration_time) {
		dbg_sensor(1, "[MOD:D:%d] %s, vsync_cnt(%d), long coarse(%d) min(%d)\n", cis->id, __func__,
			cis_data->sen_vsync_count, long_coarse_int, cis_data->min_coarse_integration_time);
		long_coarse_int = cis_data->min_coarse_integration_time;
	}

	if (short_coarse_int < cis_data->min_coarse_integration_time) {
		dbg_sensor(1, "[MOD:D:%d] %s, vsync_cnt(%d), short coarse(%d) min(%d)\n", cis->id, __func__,
			cis_data->sen_vsync_count, short_coarse_int, cis_data->min_coarse_integration_time);
		short_coarse_int = cis_data->min_coarse_integration_time;
	}

	/* Short exposure */
	ret = is_sensor_write16(cis->client, SENSOR_3L6_COARSE_INTEGRATION_TIME_ADDR, short_coarse_int);
	if (ret < 0)
		goto p_err;

	/* Long exposure */
	if (is_vender_wdr_mode_on(cis_data)) {
		ret = is_sensor_write16(cis->client, 0x021E, long_coarse_int);
		if (ret < 0)
			goto p_err;
	}

	dbg_sensor(1, "[MOD:D:%d] %s, vsync_cnt(%d), vt_pic_clk_freq_mhz (%d), line_length_pck(%d), min_fine_int (%d)\n",
		cis->id, __func__, cis_data->sen_vsync_count, vt_pic_clk_freq_mhz, line_length_pck, min_fine_int);
	dbg_sensor(1, "[MOD:D:%d] %s, vsync_cnt(%d), frame_length_lines(%#x), long_coarse_int %#x, short_coarse_int %#x\n",
		cis->id, __func__, cis_data->sen_vsync_count, cis_data->frame_length_lines,
		long_coarse_int, short_coarse_int);

	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %ldus\n", __func__, PABLO_KTIME_US_DELTA_NOW(st));

p_err:
	return ret;
}

int sensor_3l6_cis_get_min_exposure_time(struct v4l2_subdev *subdev, u32 *min_expo)
{
	int ret = 0;
	struct is_cis *cis = sensor_cis_get_cis(subdev);
	cis_shared_data *cis_data = NULL;
	u32 min_integration_time = 0;
	u32 min_coarse = 0;
	u32 min_fine = 0;
	u32 vt_pic_clk_freq_mhz = 0;
	u32 line_length_pck = 0;
	ktime_t st = ktime_get();

	WARN_ON(!min_expo);

	cis_data = cis->cis_data;

	vt_pic_clk_freq_mhz = cis_data->pclk / (1000 * 1000);
	if (vt_pic_clk_freq_mhz == 0) {
		err("Invalid vt_pic_clk_freq_mhz(%d)", vt_pic_clk_freq_mhz);
		goto p_err;
	}
	line_length_pck = cis_data->line_length_pck;
	min_coarse = cis_data->min_coarse_integration_time;
	min_fine = cis_data->min_fine_integration_time;

	min_integration_time = ((line_length_pck * min_coarse) + min_fine) / vt_pic_clk_freq_mhz;
	*min_expo = min_integration_time;

	dbg_sensor(1, "[%s] min integration time %d\n", __func__, min_integration_time);

	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %ldus\n", __func__, PABLO_KTIME_US_DELTA_NOW(st));

p_err:
	return ret;
}

int sensor_3l6_cis_get_max_exposure_time(struct v4l2_subdev *subdev, u32 *max_expo)
{
	int ret = 0;
	struct is_cis *cis = sensor_cis_get_cis(subdev);
	cis_shared_data *cis_data;
	u32 max_integration_time = 0;
	u32 max_coarse_margin = 0;
	u32 max_fine_margin = 0;
	u32 max_coarse = 0;
	u32 max_fine = 0;
	u32 vt_pic_clk_freq_mhz = 0;
	u32 line_length_pck = 0;
	u32 frame_length_lines = 0;
	ktime_t st = ktime_get();

	WARN_ON(!max_expo);

	cis_data = cis->cis_data;

	vt_pic_clk_freq_mhz = cis_data->pclk / (1000 * 1000);
	if (vt_pic_clk_freq_mhz == 0) {
		pr_err("[MOD:D:%d] %s, Invalid vt_pic_clk_freq_mhz(%d)\n", cis->id, __func__, vt_pic_clk_freq_mhz);
		goto p_err;
	}
	line_length_pck = cis_data->line_length_pck;
	frame_length_lines = cis_data->frame_length_lines;

	max_coarse_margin = cis_data->max_margin_coarse_integration_time;
	max_fine_margin = line_length_pck - cis_data->min_fine_integration_time;
	max_coarse = frame_length_lines - max_coarse_margin;
	max_fine = cis_data->max_fine_integration_time;

	max_integration_time = ((line_length_pck * max_coarse) + max_fine) / vt_pic_clk_freq_mhz;

	*max_expo = max_integration_time;

	/* TODO: Is this values update hear? */
	cis_data->max_margin_fine_integration_time = max_fine_margin;
	cis_data->max_coarse_integration_time = max_coarse;

	dbg_sensor(1, "[%s] max integration time %d, max margin fine integration %d, max coarse integration %d\n",
			__func__, max_integration_time, cis_data->max_margin_fine_integration_time,
			cis_data->max_coarse_integration_time);

	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %ldus\n", __func__, PABLO_KTIME_US_DELTA_NOW(st));

p_err:
	return ret;
}

int sensor_3l6_cis_adjust_frame_duration(struct v4l2_subdev *subdev,
						u32 input_exposure_time,
						u32 *target_duration)
{
	int ret = 0;
	struct is_cis *cis = sensor_cis_get_cis(subdev);
	cis_shared_data *cis_data;

	u32 vt_pic_clk_freq_mhz = 0;
	u32 line_length_pck = 0;
	u32 frame_length_lines = 0;
	u32 frame_duration = 0;
	ktime_t st = ktime_get();

	WARN_ON(!target_duration);

	cis_data = cis->cis_data;

	vt_pic_clk_freq_mhz = cis_data->pclk / (1000 * 1000);
	line_length_pck = cis_data->line_length_pck;
	frame_length_lines = ((vt_pic_clk_freq_mhz * input_exposure_time) / line_length_pck);
	frame_length_lines += cis_data->max_margin_coarse_integration_time;

	frame_duration = (frame_length_lines * line_length_pck) / vt_pic_clk_freq_mhz;

	dbg_sensor(1, "[%s](vsync cnt = %d) input exp(%d), adj duration, frame duraion(%d), min_frame_us(%d)\n",
			__func__, cis_data->sen_vsync_count, input_exposure_time,
			frame_duration, cis_data->min_frame_us_time);
	dbg_sensor(1, "[%s](vsync cnt = %d) adj duration, frame duraion(%d), min_frame_us(%d)\n",
			__func__, cis_data->sen_vsync_count, frame_duration, cis_data->min_frame_us_time);

	*target_duration = MAX(frame_duration, cis_data->min_frame_us_time);

	dbg_sensor(1, "[%s] calculated frame_duration(%d), adjusted frame_duration(%d)\n", 
			__func__, frame_duration, *target_duration);

	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %ldus\n", __func__, PABLO_KTIME_US_DELTA_NOW(st));

	return ret;
}

int sensor_3l6_cis_set_frame_duration(struct v4l2_subdev *subdev, u32 frame_duration)
{
	int ret = 0;
	struct is_cis *cis = sensor_cis_get_cis(subdev);
	cis_shared_data *cis_data;

	u32 vt_pic_clk_freq_mhz = 0;
	u32 line_length_pck = 0;
	u16 frame_length_lines = 0;
	u32 max_coarse_integration_time = 0;
	ktime_t st = ktime_get();

	WARN_ON(!frame_duration);

	cis_data = cis->cis_data;

	if (frame_duration < cis_data->min_frame_us_time) {
		dbg_sensor(1, "frame duration is less than min(%d)\n", frame_duration);
		frame_duration = cis_data->min_frame_us_time;
	}

	vt_pic_clk_freq_mhz = cis_data->pclk / (1000 * 1000);
	line_length_pck = cis_data->line_length_pck;

	frame_length_lines = (u16)((vt_pic_clk_freq_mhz * frame_duration) / line_length_pck);

	dbg_sensor(1, "[MOD:D:%d] %s, vt_pic_clk_freq_mhz(%#x) frame_duration = %d us,"
		KERN_CONT "(line_length_pck%#x), frame_length_lines(%#x)\n",
		cis->id, __func__, vt_pic_clk_freq_mhz, frame_duration, line_length_pck, frame_length_lines);

	ret = is_sensor_write16(cis->client, SENSOR_3L6_FRAME_LENGTH_LINE_ADDR, frame_length_lines);
	if (ret < 0)
		goto p_err;

	cis_data->cur_frame_us_time = frame_duration;
	cis_data->frame_length_lines = frame_length_lines;

	max_coarse_integration_time = cis_data->frame_length_lines - cis_data->max_margin_coarse_integration_time;
	cis_data->max_coarse_integration_time = max_coarse_integration_time;

	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %ldus\n", __func__, PABLO_KTIME_US_DELTA_NOW(st));

p_err:
	return ret;
}

int sensor_3l6_cis_set_frame_rate(struct v4l2_subdev *subdev, u32 min_fps)
{
	int ret = 0;
	struct is_cis *cis = sensor_cis_get_cis(subdev);
	cis_shared_data *cis_data;

	u32 frame_duration = 0;
	ktime_t st = ktime_get();

	cis_data = cis->cis_data;

	if (min_fps > cis_data->max_fps) {
		err("request FPS is too high(%d), set to max(%d)", min_fps, cis_data->max_fps);
		min_fps = cis_data->max_fps;
	}

	if (min_fps == 0) {
		err("request FPS is 0, set to min FPS(1)");
		min_fps = 1;
	}

	frame_duration = (1 * 1000 * 1000) / min_fps;

	dbg_sensor(1, "[MOD:D:%d] %s, set FPS(%d), frame duration(%d)\n",
			cis->id, __func__, min_fps, frame_duration);

	ret = sensor_3l6_cis_set_frame_duration(subdev, frame_duration);
	if (ret < 0) {
		err("set frame duration is fail(%d)", ret);
		goto p_err;
	}

	cis_data->min_frame_us_time = frame_duration;

	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %ldus\n", __func__, PABLO_KTIME_US_DELTA_NOW(st));

p_err:

	return ret;
}

int sensor_3l6_cis_adjust_analog_gain(struct v4l2_subdev *subdev, u32 input_again, u32 *target_permile)
{
	int ret = 0;
	struct is_cis *cis = sensor_cis_get_cis(subdev);
	cis_shared_data *cis_data;

	u32 again_code = 0;
	u32 again_permile = 0;
	ktime_t st = ktime_get();

	WARN_ON(!target_permile);

	cis_data = cis->cis_data;

	again_code = sensor_cis_calc_again_code(input_again);

	if (again_code > cis_data->max_analog_gain[0]) {
		again_code = cis_data->max_analog_gain[0];
	} else if (again_code < cis_data->min_analog_gain[0]) {
		again_code = cis_data->min_analog_gain[0];
	}

	again_permile = sensor_cis_calc_again_permile(again_code);

	dbg_sensor(1, "[%s] min again(%d), max(%d), input_again(%d), code(%d), permile(%d)\n", __func__,
			cis_data->max_analog_gain[0],
			cis_data->min_analog_gain[0],
			input_again,
			again_code,
			again_permile);

	*target_permile = again_permile;

	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %ldus\n", __func__, PABLO_KTIME_US_DELTA_NOW(st));

	return ret;
}

int sensor_3l6_cis_set_analog_gain(struct v4l2_subdev *subdev, struct ae_param *again)
{
	int ret = 0;
	struct is_cis *cis = sensor_cis_get_cis(subdev);

	u16 analog_gain = 0;
	ktime_t st = ktime_get();

	WARN_ON(!again);

	analog_gain = (u16)sensor_cis_calc_again_code(again->val);

	if (analog_gain < cis->cis_data->min_analog_gain[0]) {
		analog_gain = cis->cis_data->min_analog_gain[0];
	}

	if (analog_gain > cis->cis_data->max_analog_gain[0]) {
		analog_gain = cis->cis_data->max_analog_gain[0];
	}

	dbg_sensor(1, "[MOD:D:%d] %s(vsync cnt = %d), input_again = %d us, analog_gain(%#x)\n",
		cis->id, __func__, cis->cis_data->sen_vsync_count, again->val, analog_gain);

	ret = is_sensor_write16(cis->client, SENSOR_3L6_ANALOG_GAIN_ADDR, analog_gain);
	if (ret < 0)
		goto p_err;

	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %ldus\n", __func__, PABLO_KTIME_US_DELTA_NOW(st));

p_err:
	return ret;
}

int sensor_3l6_cis_get_analog_gain(struct v4l2_subdev *subdev, u32 *again)
{
	int ret = 0;
	struct is_cis *cis = sensor_cis_get_cis(subdev);

	u16 analog_gain = 0;
	ktime_t st = ktime_get();

	WARN_ON(!again);

	ret = is_sensor_read16(cis->client, SENSOR_3L6_ANALOG_GAIN_ADDR, &analog_gain);
	if (ret < 0)
		goto p_err;

	*again = sensor_cis_calc_again_permile(analog_gain);

	dbg_sensor(1, "[MOD:D:%d] %s, cur_again = %d us, analog_gain(%#x)\n",
			cis->id, __func__, *again, analog_gain);

	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %ldus\n", __func__, PABLO_KTIME_US_DELTA_NOW(st));

p_err:
	return ret;
}

int sensor_3l6_cis_get_min_analog_gain(struct v4l2_subdev *subdev, u32 *min_again)
{
	int ret = 0;
	struct is_cis *cis = sensor_cis_get_cis(subdev);
	cis_shared_data *cis_data;
	ktime_t st = ktime_get();

	WARN_ON(!min_again);

	cis_data = cis->cis_data;

	cis_data->min_analog_gain[0] = 0x0020;//read_value;
	cis_data->min_analog_gain[1] = sensor_cis_calc_again_permile(cis_data->min_analog_gain[0]);
	*min_again = cis_data->min_analog_gain[1];

	dbg_sensor(1, "[%s] code %d, permile %d\n", __func__,
		cis_data->min_analog_gain[0], cis_data->min_analog_gain[1]);

	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %ldus\n", __func__, PABLO_KTIME_US_DELTA_NOW(st));

	return ret;
}

int sensor_3l6_cis_get_max_analog_gain(struct v4l2_subdev *subdev, u32 *max_again)
{
	int ret = 0;
	struct is_cis *cis = sensor_cis_get_cis(subdev);
	cis_shared_data *cis_data;
	ktime_t st = ktime_get();

	WARN_ON(!max_again);

	cis_data = cis->cis_data;

	cis_data->max_analog_gain[0] = 0x0200;//read_value;
	cis_data->max_analog_gain[1] = sensor_cis_calc_again_permile(cis_data->max_analog_gain[0]);
	*max_again = cis_data->max_analog_gain[1];

	dbg_sensor(1, "[%s] code %d, permile %d\n", __func__,
		cis_data->max_analog_gain[0], cis_data->max_analog_gain[1]);

	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %ldus\n", __func__, PABLO_KTIME_US_DELTA_NOW(st));

	return ret;
}

int sensor_3l6_cis_set_digital_gain(struct v4l2_subdev *subdev, struct ae_param *dgain)
{
	int ret = 0;
	struct is_cis *cis = sensor_cis_get_cis(subdev);
	cis_shared_data *cis_data;

	u16 long_gain = 0;
	u16 short_gain = 0;
	u16 dgains[4] = {0};
	ktime_t st = ktime_get();

	WARN_ON(!dgain);

	cis_data = cis->cis_data;

	long_gain = (u16)sensor_cis_calc_dgain_code(dgain->long_val);
	short_gain = (u16)sensor_cis_calc_dgain_code(dgain->short_val);

	if (long_gain < cis->cis_data->min_digital_gain[0]) {
		long_gain = cis->cis_data->min_digital_gain[0];
	}
	if (long_gain > cis->cis_data->max_digital_gain[0]) {
		long_gain = cis->cis_data->max_digital_gain[0];
	}

	if (short_gain < cis->cis_data->min_digital_gain[0]) {
		short_gain = cis->cis_data->min_digital_gain[0];
	}
	if (short_gain > cis->cis_data->max_digital_gain[0]) {
		short_gain = cis->cis_data->max_digital_gain[0];
	}

	dbg_sensor(1, "[MOD:D:%d] %s(vsync cnt = %d), input_dgain = %d/%d us, long_gain(%#x), short_gain(%#x)\n",
			cis->id, __func__, cis->cis_data->sen_vsync_count, dgain->long_val, dgain->short_val,
			long_gain, short_gain);

	dgains[0] = dgains[1] = dgains[2] = dgains[3] = short_gain;
	/* Short digital gain */
	ret = is_sensor_write16_array(cis->client, SENSOR_3L6_DIGITAL_GAIN_ADDR, dgains, 4);
	if (ret < 0)
		goto p_err;

	/* Long digital gain */
	if (is_vender_wdr_mode_on(cis_data)) {
		dgains[0] = dgains[1] = dgains[2] = dgains[3] = long_gain;
		ret = is_sensor_write16_array(cis->client, 0x3062, dgains, 4);
		if (ret < 0)
			goto p_err;
	}

	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %ldus\n", __func__, PABLO_KTIME_US_DELTA_NOW(st));

p_err:
	return ret;
}

int sensor_3l6_cis_get_digital_gain(struct v4l2_subdev *subdev, u32 *dgain)
{
	int ret = 0;
	struct is_cis *cis = sensor_cis_get_cis(subdev);

	u16 digital_gain = 0;
	ktime_t st = ktime_get();

	WARN_ON(!dgain);

	I2C_MUTEX_LOCK(cis->i2c_lock);
	ret = is_sensor_read16(cis->client, SENSOR_3L6_DIGITAL_GAIN_ADDR, &digital_gain);
	if (ret < 0)
		goto p_err_unlock;

	*dgain = sensor_cis_calc_dgain_permile(digital_gain);

	dbg_sensor(1, "[MOD:D:%d] %s, cur_dgain = %d us, digital_gain(%#x)\n",
			cis->id, __func__, *dgain, digital_gain);

	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %ldus\n", __func__, PABLO_KTIME_US_DELTA_NOW(st));

p_err_unlock:
	I2C_MUTEX_UNLOCK(cis->i2c_lock);
	return ret;
}

int sensor_3l6_cis_get_min_digital_gain(struct v4l2_subdev *subdev, u32 *min_dgain)
{
	int ret = 0;
	struct is_cis *cis = sensor_cis_get_cis(subdev);
	cis_shared_data *cis_data;
	ktime_t st = ktime_get();

	WARN_ON(!min_dgain);

	cis_data = cis->cis_data;

	/* 3L6 cannot read min/max digital gain */
	cis_data->min_digital_gain[0] = 0x0100;
	cis_data->min_digital_gain[1] = 1000;
	*min_dgain = cis_data->min_digital_gain[1];

	dbg_sensor(1, "[%s] code %d, permile %d\n", __func__,
		cis_data->min_digital_gain[0], cis_data->min_digital_gain[1]);

	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %ldus\n", __func__, PABLO_KTIME_US_DELTA_NOW(st));

	return ret;
}

int sensor_3l6_cis_get_max_digital_gain(struct v4l2_subdev *subdev, u32 *max_dgain)
{
	int ret = 0;
	struct is_cis *cis = sensor_cis_get_cis(subdev);
	cis_shared_data *cis_data;
	ktime_t st = ktime_get();

	WARN_ON(!max_dgain);

	cis_data = cis->cis_data;

	/* 3L6 cannot read min/max digital gain */
	cis_data->max_digital_gain[0] = 0x1000;
	cis_data->max_digital_gain[1] = 16000;
	*max_dgain = cis_data->max_digital_gain[1];

	dbg_sensor(1, "[%s] code %d, permile %d\n", __func__,
		cis_data->max_digital_gain[0], cis_data->max_digital_gain[1]);

	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %ldus\n", __func__, PABLO_KTIME_US_DELTA_NOW(st));

	return ret;
}

int sensor_3l6_cis_set_test_pattern(struct v4l2_subdev *subdev, camera2_sensor_ctl_t *sensor_ctl)
{
	int ret = 0;
	struct is_cis *cis = sensor_cis_get_cis(subdev);

	dbg_sensor(1, "[MOD:D:%d] %s, cur_pattern_mode(%d), testPatternMode(%d)\n", cis->id, __func__,
			cis->cis_data->cur_pattern_mode, sensor_ctl->testPatternMode);

	if (cis->cis_data->cur_pattern_mode != sensor_ctl->testPatternMode) {
		if (sensor_ctl->testPatternMode == SENSOR_TEST_PATTERN_MODE_OFF) {
			info("%s: set DEFAULT pattern! (testpatternmode : %d)\n", __func__, sensor_ctl->testPatternMode);

			I2C_MUTEX_LOCK(cis->i2c_lock);
			is_sensor_write16(cis->client, 0x0600, 0x0000);
			I2C_MUTEX_UNLOCK(cis->i2c_lock);

			cis->cis_data->cur_pattern_mode = sensor_ctl->testPatternMode;
		} else if (sensor_ctl->testPatternMode == SENSOR_TEST_PATTERN_MODE_BLACK) {
			info("%s: set BLACK pattern! (testpatternmode :%d), Data : 0x(%x, %x, %x, %x)\n",
				__func__, sensor_ctl->testPatternMode,
				(unsigned short)sensor_ctl->testPatternData[0],
				(unsigned short)sensor_ctl->testPatternData[1],
				(unsigned short)sensor_ctl->testPatternData[2],
				(unsigned short)sensor_ctl->testPatternData[3]);

			I2C_MUTEX_LOCK(cis->i2c_lock);
			is_sensor_write16(cis->client, 0x0600, 0x0001);
			is_sensor_write16(cis->client, 0x0602, 0x0000);
			is_sensor_write16(cis->client, 0x0604, 0x0000);
			is_sensor_write16(cis->client, 0x0606, 0x0000);
			is_sensor_write16(cis->client, 0x0608, 0x0000);
			I2C_MUTEX_UNLOCK(cis->i2c_lock);

			cis->cis_data->cur_pattern_mode = sensor_ctl->testPatternMode;
		}
	}

	return ret;
}

int sensor_3l6_cis_wait_streamoff(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_cis *cis = sensor_cis_get_cis(subdev);

	u32 wait_cnt = 0, time_out_cnt = 250;
	u8 sensor_fcount = 0;
	u32 i2c_fail_cnt = 0, i2c_fail_max_cnt = 5;

	/*
	 * Read sensor frame counter (sensor_fcount address = 0x0005)
	 * stream on (0x00 ~ 0xFF), stream off (0xFF)
	 */
	do {
		I2C_MUTEX_LOCK(cis->i2c_lock);
		ret = is_sensor_read8(cis->client, SENSOR_3L6_FRAME_COUNT_ADDR, &sensor_fcount);
		I2C_MUTEX_UNLOCK(cis->i2c_lock);
		if (ret < 0) {
			i2c_fail_cnt++;
			err("i2c_r_fail addr(%x), val(%x), try(%d), ret = %d",
				SENSOR_3L6_FRAME_COUNT_ADDR, sensor_fcount, i2c_fail_cnt, ret);

			if (i2c_fail_cnt >= i2c_fail_max_cnt) {
				err("[MOD:D:%d] i2c_fail_cnt(%d) >= i2c_fail_max_cnt(%d), sensor_fcount(%d)",
						cis->id, i2c_fail_cnt, i2c_fail_max_cnt, sensor_fcount);
				ret = -EINVAL;
				goto p_err;
			}
		}
		
		/* If fcount is '0xfe' or '0xff' in streamoff, delay by 33 ms. */
		if (sensor_3l6_fcount >= 0xFE && sensor_fcount == 0xFF) {
			usleep_range(33000, 33000);
			info("[%s] delay by 33 ms (stream_off fcount : %d, wait_stream_off fcount : %d",
				__func__, sensor_3l6_fcount, sensor_fcount);
		}

		usleep_range(CIS_STREAM_OFF_WAIT_TIME, CIS_STREAM_OFF_WAIT_TIME);
		wait_cnt++;

		if (wait_cnt >= time_out_cnt) {
			err("[MOD:D:%d] time out, wait_limit(%d) > time_out(%d), sensor_fcount(%d)",
					cis->id, wait_cnt, time_out_cnt, sensor_fcount);
			ret = -EINVAL;
			goto p_err;
		}

		dbg_sensor(1, "[MOD:D:%d] %s, sensor_fcount(%d), (wait_limit(%d) < time_out(%d))\n",
				cis->id, __func__, sensor_fcount, wait_cnt, time_out_cnt);
	}while (sensor_fcount != 0xFF);

	info("[MOD:D:%d] %s, sensor_fcount(%d), (wait_limit(%d) < time_out(%d))\n",
			cis->id, __func__, sensor_fcount, wait_cnt, time_out_cnt);
p_err:
	return ret;
}

int sensor_3l6_cis_compensate_gain_for_extremely_br(struct v4l2_subdev *subdev, u32 expo, u32 *again, u32 *dgain)
{
	int ret = 0;
	struct is_cis *cis;
	cis_shared_data *cis_data;

	u64 vt_pic_clk_freq_khz = 0;
	u32 line_length_pck = 0;
	u32 min_fine_int = 0;
	u16 coarse_int = 0;
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

	vt_pic_clk_freq_khz = cis_data->pclk / 1000;
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

	if (coarse_int <= 256) {
		compensated_again = (*again * ((expo * vt_pic_clk_freq_khz) / 1000 - min_fine_int)) / (line_length_pck * coarse_int);

		if (compensated_again < cis_data->min_analog_gain[1]) {
			*again = cis_data->min_analog_gain[1];
		} else if (*again >= cis_data->max_analog_gain[1]) {
			*dgain = (*dgain * ((expo * vt_pic_clk_freq_khz) / 1000 - min_fine_int)) / (line_length_pck * coarse_int);
		} else {
			//*again = compensated_again;
			*dgain = (*dgain * ((expo * vt_pic_clk_freq_khz) / 1000 - min_fine_int)) / (line_length_pck * coarse_int);
		}

		dbg_sensor(1, "[%s] exp(%d), again(%d), dgain(%d), coarse_int(%d), compensated_again(%d)\n",
			__func__, expo, *again, *dgain, coarse_int, compensated_again);
	}

p_err:
	return ret;
}


static struct is_cis_ops cis_ops = {
	.cis_init = sensor_3l6_cis_init,
	.cis_log_status = sensor_3l6_cis_log_status,
	.cis_set_global_setting = sensor_3l6_cis_set_global_setting,
	.cis_mode_change = sensor_3l6_cis_mode_change,
	.cis_set_size = sensor_3l6_cis_set_size,
	.cis_stream_on = sensor_3l6_cis_stream_on,
	.cis_stream_off = sensor_3l6_cis_stream_off,
	.cis_set_exposure_time = sensor_3l6_cis_set_exposure_time,
	.cis_get_min_exposure_time = sensor_3l6_cis_get_min_exposure_time,
	.cis_get_max_exposure_time = sensor_3l6_cis_get_max_exposure_time,
	.cis_adjust_frame_duration = sensor_3l6_cis_adjust_frame_duration,
	.cis_set_frame_duration = sensor_3l6_cis_set_frame_duration,
	.cis_set_frame_rate = sensor_3l6_cis_set_frame_rate,
	.cis_adjust_analog_gain = sensor_3l6_cis_adjust_analog_gain,
	.cis_set_analog_gain = sensor_3l6_cis_set_analog_gain,
	.cis_get_analog_gain = sensor_3l6_cis_get_analog_gain,
	.cis_get_min_analog_gain = sensor_3l6_cis_get_min_analog_gain,
	.cis_get_max_analog_gain = sensor_3l6_cis_get_max_analog_gain,
	.cis_set_digital_gain = sensor_3l6_cis_set_digital_gain,
	.cis_get_digital_gain = sensor_3l6_cis_get_digital_gain,
	.cis_get_min_digital_gain = sensor_3l6_cis_get_min_digital_gain,
	.cis_get_max_digital_gain = sensor_3l6_cis_get_max_digital_gain,
	.cis_compensate_gain_for_extremely_br = sensor_3l6_cis_compensate_gain_for_extremely_br,
	.cis_wait_streamoff = sensor_3l6_cis_wait_streamoff,
	.cis_wait_streamon = sensor_cis_wait_streamon,
	.cis_data_calculation = sensor_3l6_cis_data_calc,
	.cis_check_rev_on_init = sensor_cis_check_rev_on_init,
	.cis_set_test_pattern = sensor_3l6_cis_set_test_pattern,
};

int cis_3l6_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	int ret;
	u32 mclk_freq_khz;
	struct is_cis *cis;
	struct is_device_sensor_peri *sensor_peri;
	char const *setfile;
	struct device_node *dnode = client->dev.of_node;

	ret = sensor_cis_probe(client, id, &sensor_peri);
	if (ret) {
		probe_info("%s: Asensor_cis_probe ret(%d)\n", __func__, ret);
		return ret;
	}

	cis = &sensor_peri->cis;
	cis->ctrl_delay = N_PLUS_TWO_FRAME;
	cis->cis_ops = &cis_ops;

	/* belows are depend on sensor cis. MUST check sensor spec */
	cis->bayer_order = OTF_INPUT_ORDER_BAYER_GR_BG;

	ret = of_property_read_string(dnode, "setfile", &setfile);
	if (ret) {
		err("setfile index read fail(%d), take default setfile!!", ret);
		setfile = "default";
	}

	mclk_freq_khz = sensor_peri->module->pdata->mclk_freq_khz;

	if (mclk_freq_khz == 19200) {
		if (strcmp(setfile, "default") == 0 || strcmp(setfile, "setA") == 0)
			probe_info("%s setfile_A mclk: 19.2MHz\n", __func__);
		else
			err("setfile index out of bound, take default (setfile_A mclk: 19.2MHz)");

		sensor_3l6_global = sensor_3l6_setfile_A_19p2_Global;
		sensor_3l6_global_size = ARRAY_SIZE(sensor_3l6_setfile_A_19p2_Global);
		sensor_3l6_setfiles = sensor_3l6_setfiles_A_19p2;
		sensor_3l6_setfile_sizes = sensor_3l6_setfile_A_19p2_sizes;
		sensor_3l6_pllinfos = sensor_3l6_pllinfos_A_19p2;
		sensor_3l6_max_setfile_num = ARRAY_SIZE(sensor_3l6_setfiles_A_19p2);
	} else {
		if (strcmp(setfile, "default") == 0 ||
				strcmp(setfile, "setA") == 0) {
			probe_info("%s setfile_A\n", __func__);
			sensor_3l6_global = sensor_3l6_setfile_A_Global;
			sensor_3l6_global_size = ARRAY_SIZE(sensor_3l6_setfile_A_Global);
			sensor_3l6_setfiles = sensor_3l6_setfiles_A;
			sensor_3l6_setfile_sizes = sensor_3l6_setfile_A_sizes;
			sensor_3l6_max_setfile_num = ARRAY_SIZE(sensor_3l6_setfiles_A);
			sensor_3l6_pllinfos = sensor_3l6_pllinfos_A;
		} else {
			err("setfile index out of bound, take default (setfile_A)");
			sensor_3l6_global = sensor_3l6_setfile_A_Global;
			sensor_3l6_global_size = ARRAY_SIZE(sensor_3l6_setfile_A_Global);
			sensor_3l6_setfiles = sensor_3l6_setfiles_A;
			sensor_3l6_setfile_sizes = sensor_3l6_setfile_A_sizes;
			sensor_3l6_pllinfos = sensor_3l6_pllinfos_A;
			sensor_3l6_max_setfile_num = ARRAY_SIZE(sensor_3l6_setfiles_A);
		}
	}

	probe_info("%s done\n", __func__);

	return ret;
}

static int cis_3l6_remove(struct i2c_client *client)
{
	int ret = 0;
	return ret;
}

static const struct of_device_id exynos_is_cis_3l6_match[] = {
	{
		.compatible = "samsung,exynos-is-cis-3l6",
	},
	{},
};
MODULE_DEVICE_TABLE(of, exynos_is_cis_3l6_match);

static const struct i2c_device_id cis_3l6_idt[] = {
	{ SENSOR_NAME, 0 },
	{},
};

static struct i2c_driver cis_3l6_driver = {
	.driver = {
		.name	= SENSOR_NAME,
		.owner	= THIS_MODULE,
		.of_match_table = exynos_is_cis_3l6_match
	},
	.probe	= cis_3l6_probe,
	.remove	= cis_3l6_remove,
	.id_table = cis_3l6_idt
};
module_i2c_driver(cis_3l6_driver);

MODULE_LICENSE("GPL");
