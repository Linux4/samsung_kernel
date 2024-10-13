/*
 * Samsung Exynos SoC series Sensor driver
 *
 *
 * Copyright (c) 2023 Samsung Electronics Co., Ltd
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
#include "is-cis-hi847.h"
#include "is-cis-hi847-setA-19p2.h"
#include "is-helper-i2c.h"
#ifdef CONFIG_CAMERA_VENDER_MCD_V2
#include "is-sec-define.h"
#endif

#include "interface/is-interface-library.h"

#define SENSOR_NAME "HI847"

#define POLL_TIME_MS 5
#define STREAM_MAX_POLL_CNT 60

static const u32 *sensor_hi847_global;
static u32 sensor_hi847_global_size;
static const u32 **sensor_hi847_setfiles;
static const u32 *sensor_hi847_setfile_sizes;
static const struct sensor_pll_info_compact **sensor_hi847_pllinfos;
static u32 sensor_hi847_max_setfile_num;

static void sensor_hi847_cis_data_calculation(const struct sensor_pll_info_compact
	*pll_info_compact, cis_shared_data *cis_data)
{
	u64 vt_pix_clk_hz;
	u32 frame_rate, max_fps, frame_valid_us;

	FIMC_BUG_VOID(!pll_info_compact);

	/* 1. get pclk value from pll info */
	vt_pix_clk_hz = pll_info_compact->pclk;

	dbg_sensor(1, "ext_clock(%d), mipi_datarate(%llu), pclk(%llu)\n",
			pll_info_compact->ext_clk, pll_info_compact->mipi_datarate, pll_info_compact->pclk);

	/* 2. the time of processing one frame calculation (us) */
	cis_data->min_frame_us_time = (((u64)pll_info_compact->frame_length_lines) *
			pll_info_compact->line_length_pck * 1000
					/ (vt_pix_clk_hz / 1000));
	cis_data->cur_frame_us_time = cis_data->min_frame_us_time;

#ifdef CAMERA_REAR2
	cis_data->min_sync_frame_us_time = cis_data->min_frame_us_time;
#endif

	/* 3. FPS calculation */
	frame_rate = vt_pix_clk_hz / (pll_info_compact->frame_length_lines * pll_info_compact->line_length_pck);
	dbg_sensor(1, "frame_rate (%d) = vt_pix_clk_hz(%llu) / "
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
	cis_data->rolling_shutter_skew = (cis_data->cur_height - 1) * cis_data->line_readOut_time;
	cis_data->stream_on = false;

	/* Frame valid time calcuration */
	frame_valid_us = (u64)cis_data->cur_height * cis_data->line_length_pck
				* 1000 * 1000 / cis_data->pclk;
	cis_data->frame_valid_us_time = (unsigned int)frame_valid_us;

	dbg_sensor(1, "%s\n", __func__);
	dbg_sensor(1, "Sensor size(%d x %d) setting: SUCCESS!\n",
					cis_data->cur_width, cis_data->cur_height);
	dbg_sensor(1, "Frame Valid(us): %d\n", frame_valid_us);
	dbg_sensor(1, "rolling_shutter_skew: %lld\n", cis_data->rolling_shutter_skew);

	dbg_sensor(1, "Fps: %d, max fps(%d)\n", frame_rate, cis_data->max_fps);
	dbg_sensor(1, "min_frame_time(%d us)\n", cis_data->min_frame_us_time);
	dbg_sensor(1, "Pixel rate(Kbps): %d\n", cis_data->pclk / 1000);

	/* Frame period calculation */
	cis_data->frame_time = (cis_data->line_readOut_time * cis_data->cur_height / 1000);
	cis_data->rolling_shutter_skew = (cis_data->cur_height - 1) * cis_data->line_readOut_time;

	dbg_sensor(1, "[%s] frame_time(%d), rolling_shutter_skew(%lld)\n", __func__,
		cis_data->frame_time, cis_data->rolling_shutter_skew);

	/* Constant values */
	cis_data->min_fine_integration_time = SENSOR_HI847_FINE_INTEGRATION_TIME_MIN;
	cis_data->max_fine_integration_time = SENSOR_HI847_FINE_INTEGRATION_TIME_MAX;
	cis_data->min_coarse_integration_time = SENSOR_HI847_COARSE_INTEGRATION_TIME_MIN;
	cis_data->max_margin_coarse_integration_time = SENSOR_HI847_COARSE_INTEGRATION_TIME_MAX_MARGIN;
}

/*************************************************
 *  [HI847 Analog gain formular]
 *
 *  Analog Gain = (Reg value)/16 + 1
 *
 *  Analog Gain Range = x1.0 to x16.0
 *
 *************************************************/

u32 sensor_hi847_cis_calc_again_code(u32 permile)
{
	return ((permile - 1000) * 16 / 1000);
}

u32 sensor_hi847_cis_calc_again_permile(u32 code)
{
	return ((code * 1000 / 16) + 1000);
}

/*************************************************
 *  [HI847 Digital gain formular]
 *
 *  Digital Gain = bit[12:9] + bit[8:0]/512 (Gr, Gb, R, B)
 *
 *  Digital Gain Range = x1.0 to x15.99
 *
 *************************************************/

u32 sensor_hi847_cis_calc_dgain_code(u32 permile)
{
	u32 buf[2] = {0, 0};

	if (permile > 15990)
		permile = 15990;

	buf[0] = ((permile / 1000) & 0x0F) << 9;
	buf[1] = ((((permile % 1000) * 512) / 1000) & 0x1FF);

	return (buf[0] | buf[1]);
}

u32 sensor_hi847_cis_calc_dgain_permile(u32 code)
{
	return (((code & 0x1E00) >> 9) * 1000) + ((code & 0x01FF) * 1000 / 512);
}

/* CIS OPS */
int sensor_hi847_cis_init(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_cis *cis;
	u32 setfile_index = 0;
	cis_setting_info setinfo;
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

	FIMC_BUG(!cis->cis_data);
#if !defined(CONFIG_CAMERA_VENDER_MCD)
	memset(cis->cis_data, 0, sizeof(cis_shared_data));

	ret = sensor_cis_check_rev(cis);
	if (ret < 0) {
		warn("sensor_hi847_check_rev is fail when cis init, ret(%d)", ret);
		ret = -EINVAL;
		goto p_err;
	}
#endif

	cis->cis_data->stream_on = false;
	cis->cis_data->cur_width = SENSOR_HI847_MAX_WIDTH;
	cis->cis_data->cur_height = SENSOR_HI847_MAX_HEIGHT;
	cis->cis_data->low_expo_start = 33000;
	cis->need_mode_change = false;
	cis->long_term_mode.sen_strm_off_on_step = 0;
	cis->long_term_mode.sen_strm_off_on_enable = false;

	cis->cis_data->cur_pattern_mode = SENSOR_TEST_PATTERN_MODE_OFF;

	cis->mipi_clock_index_cur = CAM_MIPI_NOT_INITIALIZED;
	cis->mipi_clock_index_new = CAM_MIPI_NOT_INITIALIZED;

	cis->cis_data->sens_config_index_pre = SENSOR_HI847_MODE_MAX;
	cis->cis_data->sens_config_index_cur = 0;

	sensor_hi847_cis_data_calculation(sensor_hi847_pllinfos[setfile_index], cis->cis_data);

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
	setinfo.return_value = 0;

	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %ldus", __func__, PABLO_KTIME_US_DELTA_NOW(st));

p_err:
	return ret;
}

static const struct is_cis_log log_hi847[] = {
	{I2C_READ, 16, SENSOR_HI847_MODEL_ID_ADDR, 0, "model_id"},
	{I2C_READ, 16, SENSOR_HI847_STREAM_MODE_ADDR, 0, "streaming mode"},
	{I2C_READ, 16, SENSOR_HI847_ISP_EN_ADDR, 0, "ISP EN"},
	{I2C_READ, 16, SENSOR_HI847_COARSE_INTEG_TIME_ADDR, 0, "coarse_integration_time"},
	{I2C_READ, 16, SENSOR_HI847_ANALOG_GAIN_ADDR, 0, "gain_code_global"},
	{I2C_READ, 16, SENSOR_HI847_DIGITAL_GAIN_GR_ADDR, 0, "digital gain GR"},
	{I2C_READ, 16, SENSOR_HI847_DIGITAL_GAIN_GB_ADDR, 0, "digital gain GB"},
	{I2C_READ, 16, SENSOR_HI847_DIGITAL_GAIN_R_ADDR, 0, "digital gain R"},
	{I2C_READ, 16, SENSOR_HI847_DIGITAL_GAIN_B_ADDR, 0, "digital gain B"},
	{I2C_READ, 16, SENSOR_HI847_FRAME_LENGTH_LINE_ADDR, 0, "frame_length_line"},
	{I2C_READ, 16, SENSOR_HI847_LINE_LENGTH_PCK_ADDR, 0, "line_length_pck"},
	{I2C_READ, 8, 0x1004, 0, "mipi img data id ctrl"},
	{I2C_READ, 8, 0x1005, 0, "mipi pd data id ctrl "},
	{I2C_READ, 8, 0x1038, 0, "mipi virtual channel ctrl"},
	{I2C_READ, 8, 0x1042, 0, "mipi pd seperation ctrl"},
};

int sensor_hi847_cis_log_status(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_cis *cis;
	struct i2c_client *client = NULL;

	FIMC_BUG(!subdev);

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

	sensor_cis_log_status(cis, client, log_hi847,
			ARRAY_SIZE(log_hi847), (char *)__func__);

p_err:
	return ret;
}

int sensor_hi847_cis_set_global_setting(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_cis *cis = NULL;
	struct i2c_client *client;

	FIMC_BUG(!subdev);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);
	FIMC_BUG(!cis);

	I2C_MUTEX_LOCK(cis->i2c_lock);

	client = cis->client;
	if (unlikely(!client)) {
		err("client is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	/* setfile global setting is at camera entrance */
	info("[%d][%s] global setting enter\n", cis->id, __func__);
	ret = sensor_cis_set_registers(subdev, sensor_hi847_global, sensor_hi847_global_size);
	if (ret < 0) {
		err("[%d][%d]fail!!", cis->id, __func__);
		goto p_err;
	}
	info("[%d][%s] global setting done\n", cis->id, __func__);

p_err:
	I2C_MUTEX_UNLOCK(cis->i2c_lock);

	return ret;
}

int sensor_hi847_cis_mode_change(struct v4l2_subdev *subdev, u32 mode)
{
	int ret = 0;
	struct is_cis *cis = NULL;
	struct is_device_sensor *device;
	struct is_device_sensor_peri *sensor_peri;

	FIMC_BUG(!subdev);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);
	FIMC_BUG(!cis);
	FIMC_BUG(!cis->cis_data);

	device = (struct is_device_sensor *)v4l2_get_subdev_hostdata(subdev);
	if (unlikely(!device)) {
		err("device sensor is null");
		return -EINVAL;
	}

	sensor_peri = container_of(cis, struct is_device_sensor_peri, cis);
	if (unlikely(!sensor_peri)) {
		err("sensor peri is NULL");
		return -EINVAL;
	}

	if (mode >= sensor_hi847_max_setfile_num) {
		err("invalid mode(%d)!!", mode);
		ret = -EINVAL;
		goto p_err;
	}

	cis->mipi_clock_index_cur = CAM_MIPI_NOT_INITIALIZED;

	I2C_MUTEX_LOCK(cis->i2c_lock);

	ret = sensor_cis_set_registers(subdev, sensor_hi847_setfiles[mode], sensor_hi847_setfile_sizes[mode]);
	if (ret < 0) {
		err("[%d]sensor_hi847_set_registers fail!!", cis->id);
		goto p_err_i2c_unlock;
	}

	info("[%d][%s] mode changed(%d)\n", cis->id, __func__, mode);


p_err_i2c_unlock:
	I2C_MUTEX_UNLOCK(cis->i2c_lock);

p_err:
	info("[%s] X\n", __func__);
	return ret;
}

#if USE_GROUP_PARAM_HOLD
static int sensor_hi847_cis_group_param_hold_func(struct v4l2_subdev *subdev, unsigned int hold)
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

	ret = is_sensor_write8(client, SENSOR_HI847_GROUP_PARAM_HOLD_ADDR, hold); /* api_rw_general_setup_grouped_parameter_hold */
	dbg_sensor(1, "%s : hold = %d, ret = %d\n", __func__, hold, ret);
	if (ret < 0)
		goto p_err;

	cis->cis_data->group_param_hold = hold;
	ret = hold;
p_err:
	return ret;
}
#else
static inline int sensor_hi847_cis_group_param_hold_func(struct v4l2_subdev *subdev, unsigned int hold)
{
	return 0;
}
#endif


/* Input
 *	hold : true - hold, flase - no hold
 * Output
 *	  return: 0 - no effect(already hold or no hold)
 *		positive - setted by request
 *		negative - ERROR value
 */
int sensor_hi847_cis_group_param_hold(struct v4l2_subdev *subdev, bool hold)
{
	int ret = 0;
	struct is_cis *cis = NULL;

	WARN_ON(!subdev);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);

	WARN_ON(!cis);
	WARN_ON(!cis->cis_data);

	I2C_MUTEX_LOCK(cis->i2c_lock);
	ret = sensor_hi847_cis_group_param_hold_func(subdev, hold);
	if (ret < 0)
		goto p_err;

p_err:
	I2C_MUTEX_UNLOCK(cis->i2c_lock);
	return ret;
}

int sensor_hi847_cis_stream_on(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_cis *cis;
	struct i2c_client *client;
	cis_shared_data *cis_data;
	ktime_t st = ktime_get();
#ifdef USE_CAMERA_MCD_SW_DUAL_SYNC
	struct is_core *core;

	core = is_get_is_core();
	if (!core) {
		err("core is NULL");
		return -EINVAL;
	}
#endif

	FIMC_BUG(!subdev);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);

	FIMC_BUG(!cis);
	FIMC_BUG(!cis->cis_data);

	client = cis->client;
	FIMC_BUG(!client);

	cis_data = cis->cis_data;

	dbg_sensor(1, "[MOD:D:%d] %s\n", cis->id, __func__);

	ret = sensor_hi847_cis_group_param_hold_func(subdev, 0x0);
	if (ret < 0)
		err("group_param_hold_func failed at stream off");

	I2C_MUTEX_LOCK(cis->i2c_lock);

#ifdef SENSOR_HI847_DEBUG_INFO
	{
	u16 pll;

	is_sensor_read16(client, 0x0702, &pll);
	dbg_sensor(1, "______ pll_cfg(%x)\n", pll);
	is_sensor_read16(client, 0x0732, &pll);
	dbg_sensor(1, "______ pll_clkgen_en_ramp(%x)\n", pll);
	is_sensor_read16(client, 0x0736, &pll);
	dbg_sensor(1, "______ pll_mdiv_ramp(%x)\n", pll);
	is_sensor_read16(client, 0x0738, &pll);
	dbg_sensor(1, "______ pll_prediv_ramp(%x)\n", pll);
	is_sensor_read16(client, 0x073C, &pll);
	dbg_sensor(1, "______ pll_tg_vt_sys_div1(%x)\n", (pll & 0x0F00));
	dbg_sensor(1, "______ pll_tg_vt_sys_div2(%x)\n", (pll & 0x0001));
	is_sensor_read16(client, 0x0742, &pll);
	dbg_sensor(1, "______ pll_clkgen_en_mipi(%x)\n", pll);
	is_sensor_read16(client, 0x0746, &pll);
	dbg_sensor(1, "______ pll_mdiv_mipi(%x)\n", pll);
	is_sensor_read16(client, 0x0748, &pll);
	dbg_sensor(1, "______ pll_prediv_mipi(%x)\n", pll);
	is_sensor_read16(client, 0x074A, &pll);
	dbg_sensor(1, "______ pll_mipi_vt_sys_div1(%x)\n", (pll & 0x0F00));
	dbg_sensor(1, "______ pll_mipi_vt_sys_div2(%x)\n", (pll & 0x0011));
	is_sensor_read16(client, 0x074C, &pll);
	dbg_sensor(1, "______ pll_mipi_div1(%x)\n", pll);
	is_sensor_read16(client, 0x074E, &pll);
	dbg_sensor(1, "______ pll_mipi_byte_div(%x)\n", pll);
	is_sensor_read16(client, 0x020E, &pll);
	dbg_sensor(1, "______ frame_length_line(%x)\n", pll);
	is_sensor_read16(client, 0x0206, &pll);
	dbg_sensor(1, "______ line_length_pck(%x)\n", pll);
	}
#endif

#if SENSOR_HI847_PDAF_DISABLE
	is_sensor_write16(client, 0x0B04, 0x00FC);
	is_sensor_write16(client, 0x1004, 0x2BB0);
	is_sensor_write16(client, 0x1038, 0x0000);
	is_sensor_write16(client, 0x1042, 0x0008);
#endif

#ifdef USE_CAMERA_MCD_SW_DUAL_SYNC
	if (cis->cis_data->is_data.scene_mode == AA_SCENE_MODE_LIVE_OUTFOCUS) {
		if (test_bit(IS_SENSOR_OPEN, &(core->sensor[0].state))) {
			info("[%s] s/w dual sync master\n", __func__);
			cis->dual_sync_mode = DUAL_SYNC_MASTER;
			cis->dual_sync_type = DUAL_SYNC_TYPE_SW;
		} else {
			info("[%s] s/w dual sync slave\n", __func__);
			cis->dual_sync_mode = DUAL_SYNC_SLAVE;
			cis->dual_sync_type = DUAL_SYNC_TYPE_SW;
		}
	} else {
		info("[%s] s/w dual sync none\n", __func__);
		cis->dual_sync_mode = DUAL_SYNC_NONE;
		cis->dual_sync_type = DUAL_SYNC_TYPE_MAX;
	}
#endif

	/* Sensor stream on */
	info("%s\n", __func__);
	ret = is_sensor_write16(client, SENSOR_HI847_STREAM_MODE_ADDR, 0x0100);
	if (ret < 0) {
		err("i2c transfer fail addr(%x) ret = %d\n",
				SENSOR_HI847_STREAM_MODE_ADDR, ret);
		goto p_i2c_err;
	}

	cis_data->stream_on = true;

	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %ldus", __func__, PABLO_KTIME_US_DELTA_NOW(st));

p_i2c_err:
	I2C_MUTEX_UNLOCK(cis->i2c_lock);

	return ret;
}

int sensor_hi847_cis_set_test_pattern(struct v4l2_subdev *subdev, camera2_sensor_ctl_t *sensor_ctl)
{
	int ret = 0;
	struct is_cis *cis;
	struct i2c_client *client;
	u16 read_val = 0;
	u16 enable_tpg = 0;

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);

	WARN_ON(!cis);
	WARN_ON(!cis->cis_data);

	client = cis->client;
	if (unlikely(!client)) {
		err("client is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	dbg_sensor(1, "[MOD:D:%d] %s, cur_pattern_mode(%d), testPatternMode(%d)\n", cis->id, __func__,
			cis->cis_data->cur_pattern_mode, sensor_ctl->testPatternMode);

	if (cis->cis_data->cur_pattern_mode != sensor_ctl->testPatternMode) {
		info("%s REG : 0x0B04 write to 0x00dd", __func__);

		I2C_MUTEX_LOCK(cis->i2c_lock);
		is_sensor_read16(client, 0x0B04, &read_val);

		enable_tpg = read_val | 0x1; // B[0]: tpg enable
		is_sensor_write16(client, 0x0B04, enable_tpg);

		I2C_MUTEX_UNLOCK(cis->i2c_lock);

		cis->cis_data->cur_pattern_mode = sensor_ctl->testPatternMode;
		if (sensor_ctl->testPatternMode == SENSOR_TEST_PATTERN_MODE_OFF) {
			info("%s: set DEFAULT pattern! (testpatternmode : %d)\n", __func__, sensor_ctl->testPatternMode);

			I2C_MUTEX_LOCK(cis->i2c_lock);
			is_sensor_write16(client, 0x0C0A, 0x0000);
			I2C_MUTEX_UNLOCK(cis->i2c_lock);

		} else if (sensor_ctl->testPatternMode == SENSOR_TEST_PATTERN_MODE_BLACK) {
			info("%s: set BLACK pattern! (testpatternmode :%d), Data : 0x(%x, %x, %x, %x)\n",
				__func__, sensor_ctl->testPatternMode,
				(unsigned short)sensor_ctl->testPatternData[0],
				(unsigned short)sensor_ctl->testPatternData[1],
				(unsigned short)sensor_ctl->testPatternData[2],
				(unsigned short)sensor_ctl->testPatternData[3]);

			I2C_MUTEX_LOCK(cis->i2c_lock);
			is_sensor_write16(client, 0x0C0A, 0x0100);
			I2C_MUTEX_UNLOCK(cis->i2c_lock);
		}
	}

p_err:
	return ret;
}

int sensor_hi847_cis_stream_off(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_cis *cis;
	struct i2c_client *client;
	cis_shared_data *cis_data;
	ktime_t st = ktime_get();

	FIMC_BUG(!subdev);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);

	FIMC_BUG(!cis);
	FIMC_BUG(!cis->cis_data);

	client = cis->client;
	FIMC_BUG(!client);

	cis_data = cis->cis_data;

	dbg_sensor(1, "[MOD:D:%d] %s\n", cis->id, __func__);

	ret = sensor_hi847_cis_group_param_hold_func(subdev, 0x0);
	if (ret < 0)
		err("group_param_hold_func failed at stream off");

	I2C_MUTEX_LOCK(cis->i2c_lock);

	info("%s\n", __func__);
	ret = is_sensor_write16(client, SENSOR_HI847_STREAM_MODE_ADDR, 0x0000);
	if (ret < 0) {
		err("i2c transfer fail addr(%x) ret = %d\n",
				SENSOR_HI847_STREAM_MODE_ADDR, ret);
		goto p_i2c_err;
	}

	cis_data->stream_on = false;

	info("[%s] Done.\n", __func__);

	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %ldus", __func__, PABLO_KTIME_US_DELTA_NOW(st));

p_i2c_err:
	I2C_MUTEX_UNLOCK(cis->i2c_lock);

	return ret;
}

int sensor_hi847_cis_set_digital_gain(struct v4l2_subdev *subdev, struct ae_param *dgain)
{
	int ret = 0;
	int hold = 0;
	struct is_cis *cis = NULL;
	struct i2c_client *client = NULL;
	cis_shared_data *cis_data = NULL;

	u16 dgain_code = 0;
	u16 dgains[4] = {0};
	u16 read_val = 0;
	u16 enable_dgain = 0;
	ktime_t st = ktime_get();

	FIMC_BUG(!subdev);
	FIMC_BUG(!dgain);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);

	FIMC_BUG(!cis);
	FIMC_BUG(!cis->cis_data);

	client = cis->client;
	FIMC_BUG(!client);

	cis_data = cis->cis_data;

	dgain_code = (u16)sensor_hi847_cis_calc_dgain_code(dgain->val);

	if (dgain_code < cis_data->min_digital_gain[0]) {
		info("[%s] not proper dgain_code value, reset to min_digital_gain\n", __func__);
		dgain_code = cis_data->min_digital_gain[0];
	}
	if (dgain_code > cis_data->max_digital_gain[0]) {
		info("[%s] not proper dgain_code value, reset to max_digital_gain\n", __func__);
		dgain_code = cis_data->max_digital_gain[0];
	}

	dbg_sensor(1, "[MOD:D:%d] %s(vsync cnt = %d), input_dgain permile(%d), dgain_code(0x%X)\n",
			cis->id, __func__, cis_data->sen_vsync_count, dgain->val, dgain_code);


	I2C_MUTEX_LOCK(cis->i2c_lock);
	hold = sensor_hi847_cis_group_param_hold_func(subdev, 0x1);
	if (hold < 0) {
		ret = hold;
		goto p_i2c_err;
	}

	dgains[0] = dgains[1] = dgains[2] = dgains[3] = dgain_code;
	ret = is_sensor_write16_array(client, SENSOR_HI847_DIGITAL_GAIN_ADDR, dgains, 4);
	if (ret < 0)
		goto p_i2c_err;

	ret = is_sensor_read16(client, SENSOR_HI847_ISP_EN_ADDR, &read_val);
	if (ret < 0)
		goto p_i2c_err;

	enable_dgain = read_val | (0x1 << 4); // B[4]: D gain enable
	ret = is_sensor_write16(client, SENSOR_HI847_ISP_EN_ADDR, enable_dgain);
	if (ret < 0)
		goto p_i2c_err;

	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %ldus", __func__, PABLO_KTIME_US_DELTA_NOW(st));

p_i2c_err:
	if (hold > 0) {
		hold = sensor_hi847_cis_group_param_hold_func(subdev, 0x0);
		if (hold < 0)
			ret = hold;
	}
	I2C_MUTEX_UNLOCK(cis->i2c_lock);

	return ret;
}

/* Deprecated */
static int sensor_hi847_cis_set_size(struct v4l2_subdev *subdev, cis_shared_data *cis_data)
{
	return 0;
}

int sensor_hi847_cis_wait_streamoff(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_cis *cis = NULL;
	struct i2c_client *client = NULL;
	u32 wait_cnt = 0;
	u16 PLL_en = 0;

	FIMC_BUG(!subdev);

	cis = (struct is_cis *) v4l2_get_subdevdata(subdev);
	FIMC_BUG(!cis);

	client = cis->client;
	FIMC_BUG(!client);

	do {
		ret = is_sensor_read16(client, SENSOR_HI847_ISP_PLL_ENABLE_ADDR, &PLL_en);
		if (ret < 0) {
			err("i2c transfer fail addr(%x) ret = %d\n",
					SENSOR_HI847_ISP_PLL_ENABLE_ADDR, ret);
			goto p_err;
		}

		dbg_sensor(1, "%s: PLL_en 0x%x\n", __func__, PLL_en);
		/* pll_bypass */
		if (!(PLL_en & 0x0100))
			break;

		wait_cnt++;
		msleep(POLL_TIME_MS);
	} while (wait_cnt < STREAM_MAX_POLL_CNT);

	if (wait_cnt < STREAM_MAX_POLL_CNT)
		info("%s: finished after %d ms\n", __func__, (wait_cnt + 1) * POLL_TIME_MS);
	else
		warn("%s: finished : polling timeout occurred after %d ms\n", __func__,
				(wait_cnt + 1) * POLL_TIME_MS);

p_err:
	return ret;
}

int sensor_hi847_cis_wait_streamon(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_cis *cis = NULL;
	struct i2c_client *client = NULL;
	u32 wait_cnt = 0;
	u16 PLL_en = 0;

	FIMC_BUG(!subdev);

	cis = (struct is_cis *) v4l2_get_subdevdata(subdev);
	FIMC_BUG(!cis);

	client = cis->client;
	FIMC_BUG(!client);

	probe_info("[%s] start\n", __func__);

	do {
		ret = is_sensor_read16(client, SENSOR_HI847_ISP_PLL_ENABLE_ADDR, &PLL_en);
		if (ret < 0) {
			err("i2c transfer fail addr(%x) ret = %d\n", SENSOR_HI847_ISP_PLL_ENABLE_ADDR, ret);
			goto p_err;
		}

		dbg_sensor(1, "%s: PLL_en 0x%x\n", __func__, PLL_en);
		/* pll_enable */
		if (PLL_en & 0x0100)
			break;

		wait_cnt++;
		msleep(POLL_TIME_MS);
	} while (wait_cnt < STREAM_MAX_POLL_CNT);

	if (wait_cnt < STREAM_MAX_POLL_CNT)
		info("%s: finished after %d ms\n", __func__, (wait_cnt + 1) * POLL_TIME_MS);
	else
		warn("%s: finished : polling timeout occurred after %d ms\n", __func__, (wait_cnt + 1) * POLL_TIME_MS);

p_err:
	return ret;
}

void sensor_hi847_cis_data_calc(struct v4l2_subdev *subdev, u32 mode)
{
	struct is_cis *cis = NULL;

	WARN_ON(!subdev);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);
	WARN_ON(!cis);
	WARN_ON(!cis->cis_data);

	if (mode >= sensor_hi847_max_setfile_num) {
		err("invalid mode(%d)!!", mode);
		return;
	}

	sensor_hi847_cis_data_calculation(sensor_hi847_pllinfos[mode], cis->cis_data);
}

static int sensor_hi847_cis_set_exposure_time(struct v4l2_subdev *subdev, struct ae_param *target_exposure)
{
	int ret = 0;
	int hold = 0;
	struct is_cis *cis;
	struct i2c_client *client;
	cis_shared_data *cis_data;

	u64 vt_pic_clk_freq_khz = 0;
	u64 coarse_int = 0;
	u32 line_length_pck = 0;
	u32 min_fine_int = 0;
	u16 coarse_int_lw = 0;
	u16 coarse_int_hw = 0;
	ktime_t st = ktime_get();

	FIMC_BUG(!subdev);
	FIMC_BUG(!target_exposure);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);

	FIMC_BUG(!cis);
	FIMC_BUG(!cis->cis_data);

	client = cis->client;
	if (unlikely(!client)) {
		err("client is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	if ((target_exposure->long_val <= 0) || (target_exposure->short_val <= 0)) {
		err("[%s] invalid target exposure(%d, %d)\n", __func__,
				target_exposure->long_val, target_exposure->short_val);
		ret = -EINVAL;
		goto p_err;
	}

	cis_data = cis->cis_data;

	dbg_sensor(1, "[MOD:D:%d] %s, vsync_cnt(%d), target long(%d), short(%d)\n", cis->id, __func__,
			cis_data->sen_vsync_count, target_exposure->long_val, target_exposure->short_val);

	vt_pic_clk_freq_khz = cis_data->pclk / 1000;
	line_length_pck = cis_data->line_length_pck;
	min_fine_int = cis_data->min_fine_integration_time;

	coarse_int = ((target_exposure->val * vt_pic_clk_freq_khz) / 1000 - min_fine_int) / line_length_pck;
	coarse_int_hw = coarse_int >> 16 & 0xFF;

	if (coarse_int_hw == 0) {
		if (coarse_int > cis_data->max_coarse_integration_time) {
			dbg_sensor(1, "[MOD:D:%d] %s, vsync_cnt(%d), short coarse(%d) max(%d)\n", cis->id, __func__,
				   cis_data->sen_vsync_count, coarse_int, cis_data->max_coarse_integration_time);
			coarse_int = cis_data->max_coarse_integration_time;
		}
	}

	if (coarse_int < cis_data->min_coarse_integration_time) {
		dbg_sensor(1, "[MOD:D:%d] %s, vsync_cnt(%d), short coarse(%d) min(%d)\n", cis->id, __func__,
			cis_data->sen_vsync_count, coarse_int, cis_data->min_coarse_integration_time);
		coarse_int = cis_data->min_coarse_integration_time;
	}

	dbg_sensor(1, "[MOD:D:%d] %s, frame_length_lines(%#x), short_coarse_int %#x\n",
		cis->id, __func__, cis_data->frame_length_lines, coarse_int);

	cis_data->cur_exposure_coarse = coarse_int;
	cis_data->cur_long_exposure_coarse = coarse_int;
	cis_data->cur_short_exposure_coarse = coarse_int;

	I2C_MUTEX_LOCK(cis->i2c_lock);

	hold = sensor_hi847_cis_group_param_hold_func(subdev, 0x01);
	if (hold < 0) {
		ret = hold;
		goto p_err_i2c_unlock;
	}
	/* Short exposure */
	coarse_int_lw = coarse_int & 0xFFFF;

	ret = is_sensor_write16(client, SENSOR_HI847_COARSE_INTEG_TIME_HW_ADDR, coarse_int_hw);
	if (ret < 0)
		goto p_err_i2c_unlock;
	ret = is_sensor_write16(client, SENSOR_HI847_COARSE_INTEG_TIME_ADDR, coarse_int_lw);
	if (ret < 0)
		goto p_err_i2c_unlock;

	dbg_sensor(1, "[MOD:D:%d] %s, coarse_int(%#x), coarse_int_hw(%#02x), coarse_int_lw(%#04x)\n", cis->id, __func__, coarse_int, coarse_int_hw, coarse_int_lw);

	/* Long exposure */
	dbg_sensor(1, "[MOD:D:%d] %s, vsync_cnt(%d), vt_pic_clk_freq_khz (%d), line_length_pck(%d), min_fine_int (%d)\n",
		cis->id, __func__, cis_data->sen_vsync_count, vt_pic_clk_freq_khz, line_length_pck, min_fine_int);

	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %ldus", __func__, PABLO_KTIME_US_DELTA_NOW(st));

p_err_i2c_unlock:
	if (hold > 0) {
		hold = sensor_hi847_cis_group_param_hold_func(subdev, 0x00);
		if (hold < 0)
			ret = hold;
	}

	I2C_MUTEX_UNLOCK(cis->i2c_lock);

p_err:
	return ret;
}

int sensor_hi847_cis_get_min_exposure_time(struct v4l2_subdev *subdev, u32 *min_expo)
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

	FIMC_BUG(!subdev);
	FIMC_BUG(!min_expo);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);

	FIMC_BUG(!cis);
	FIMC_BUG(!cis->cis_data);

	cis_data = cis->cis_data;

	vt_pic_clk_freq_khz = cis_data->pclk / 1000;
	if (vt_pic_clk_freq_khz == 0) {
		err("[%d] Invalid vt_pic_clk_freq_khz(%d)", cis->id, vt_pic_clk_freq_khz);
		goto p_err;
	}
	line_length_pck = cis_data->line_length_pck;
	min_coarse = cis_data->min_coarse_integration_time;
	min_fine = cis_data->min_fine_integration_time;

	min_integration_time = (u32)((u64)((line_length_pck * min_coarse) + min_fine) * 1000 / vt_pic_clk_freq_khz);
	*min_expo = min_integration_time;

	dbg_sensor(1, "[%s] min integration time %d\n", __func__, min_integration_time);

	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %ldus\n", __func__, PABLO_KTIME_US_DELTA_NOW(st));

p_err:
	return ret;
}

int sensor_hi847_cis_get_max_exposure_time(struct v4l2_subdev *subdev, u32 *max_expo)
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

	FIMC_BUG(!subdev);
	FIMC_BUG(!max_expo);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);

	FIMC_BUG(!cis);
	FIMC_BUG(!cis->cis_data);

	cis_data = cis->cis_data;

	vt_pic_clk_freq_khz = cis_data->pclk / 1000;
	if (vt_pic_clk_freq_khz == 0) {
		err("[MOD:D:%d] Invalid vt_pic_clk_freq_khz(%d)", cis->id, vt_pic_clk_freq_khz);
		goto p_err;
	}
	line_length_pck = cis_data->line_length_pck;
	frame_length_lines = cis_data->frame_length_lines;

	max_coarse_margin = cis_data->max_margin_coarse_integration_time;

	if (line_length_pck > cis_data->min_fine_integration_time)
		max_fine_margin = line_length_pck - cis_data->min_fine_integration_time;
	else
		err("[MOD:D:%d] min_fine_integration_time(%d) is larger than line_length_pck(%d)", cis->id, cis_data->min_fine_integration_time, line_length_pck);

	if (frame_length_lines > max_coarse_margin)
		max_coarse = frame_length_lines - max_coarse_margin;
	else
		err("[MOD:D:%d] max_coarse_margin(%d) is larger than frame_length_lines(%d)", cis->id, max_coarse_margin, frame_length_lines);

	max_fine = cis_data->max_fine_integration_time;

	max_integration_time = (u32)((u64)((line_length_pck * max_coarse) + max_fine) * 1000 / vt_pic_clk_freq_khz);

	*max_expo = max_integration_time;

	cis_data->max_margin_fine_integration_time = max_fine_margin;
	cis_data->max_coarse_integration_time = max_coarse;

	dbg_sensor(1, "[%s] max integration time %d, max margin fine integration %d, max coarse integration %d\n",
			__func__, max_integration_time,
			cis_data->max_margin_fine_integration_time,
			cis_data->max_coarse_integration_time);

	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %ldus\n", __func__, PABLO_KTIME_US_DELTA_NOW(st));

p_err:
	return ret;
}

int sensor_hi847_cis_adjust_frame_duration(struct v4l2_subdev *subdev,
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
	u32 max_frame_us_time = 0;

	ktime_t st = ktime_get();

	FIMC_BUG(!subdev);
	FIMC_BUG(!target_duration);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);

	FIMC_BUG(!cis);
	FIMC_BUG(!cis->cis_data);

	cis_data = cis->cis_data;

	if (input_exposure_time == 0) {
		input_exposure_time  = cis_data->min_frame_us_time;
		info("[%d][%s] Notproper exposure time(0), so apply min frame duration to exposure time forcely!(%d)\n",
			cis->id, __func__, cis_data->min_frame_us_time);
	}

	vt_pic_clk_freq_khz = cis_data->pclk / 1000;
	line_length_pck = cis_data->line_length_pck;
	frame_length_lines = (((vt_pic_clk_freq_khz * input_exposure_time) / 1000) / line_length_pck);
	frame_length_lines += cis_data->max_margin_coarse_integration_time;

	frame_duration = (u32)(((u64)frame_length_lines * line_length_pck) * 1000 / vt_pic_clk_freq_khz);
	max_frame_us_time = 1000000/cis->min_fps;

	dbg_sensor(1, "[%s] input exp(%d), adj duration, frame duraion(%d), min_frame_us(%d)\n",
			__func__, input_exposure_time, frame_duration, cis_data->min_frame_us_time);
	dbg_sensor(1, "[%s] adj duration, frame duraion(%d), min_frame_us(%d)\n",
			__func__, frame_duration, cis_data->min_frame_us_time);

	*target_duration = MAX(frame_duration, cis_data->min_frame_us_time);

	if (cis->min_fps == cis->max_fps)
		*target_duration = MIN(frame_duration, max_frame_us_time);

	dbg_sensor(1, "[%s] calcurated frame_duration(%d), adjusted frame_duration(%d)\n", __func__, frame_duration, *target_duration);

	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %ldus\n", __func__, PABLO_KTIME_US_DELTA_NOW(st));

	return ret;
}

int sensor_hi847_cis_set_frame_duration(struct v4l2_subdev *subdev, u32 frame_duration)
{
	int ret = 0;
	int hold = 0;
	struct is_cis *cis;
	struct i2c_client *client;
	cis_shared_data *cis_data;

	u64 vt_pic_clk_freq_khz = 0;
	u32 line_length_pck = 0;
	u32 frame_length_lines = 0;
	u32 frame_length_lines_lw = 0;
	u32 frame_length_lines_hw = 0;

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

	cis_data = cis->cis_data;

	if (frame_duration < cis_data->min_frame_us_time) {
		dbg_sensor(1, "frame duration is less than min(%d)\n", frame_duration);
		frame_duration = cis_data->min_frame_us_time;
	}

	vt_pic_clk_freq_khz = cis_data->pclk / 1000;
	line_length_pck = cis_data->line_length_pck;

	frame_length_lines = (u32)((vt_pic_clk_freq_khz * frame_duration) / (line_length_pck * 1000));

	frame_length_lines_lw = frame_length_lines & 0xFFFF;
	frame_length_lines_hw = (frame_length_lines >> 16) & 0xFF;

#ifdef USE_CAMERA_MCD_SW_DUAL_SYNC
	/* NOTE: if cis is working as master, the frame duration need to be larger than slave cis. */
	if (cis->dual_sync_mode == DUAL_SYNC_MASTER)
		frame_length_lines += 2;
#endif

	dbg_sensor(1, "[MOD:D:%d] %s, vt_pic_clk_freq_khz(%#x) frame_duration = %d us,"
			KERN_CONT "(line_length_pck%#x), frame_length_lines(%#x),"
			KERN_CONT "frame_length_lines_lw(%#04x), frame_length_lines_hw(%#04x)\n",
			cis->id, __func__, vt_pic_clk_freq_khz, frame_duration,
			line_length_pck, frame_length_lines, frame_length_lines_lw,
			frame_length_lines_hw);

	I2C_MUTEX_LOCK(cis->i2c_lock);
	hold = sensor_hi847_cis_group_param_hold_func(subdev, 0x1);
	if (hold < 0) {
		ret = hold;
		goto p_err_i2c_unlock;
	}

	ret = is_sensor_write16(client, SENSOR_HI847_FRAME_LENGTH_LINE_ADDR, frame_length_lines_lw);
	if (ret < 0)
		goto p_err_i2c_unlock;

	ret = is_sensor_write16(client, SENSOR_HI847_FRAME_LENGTH_LINE_HW_ADDR, frame_length_lines_hw);
	if (ret < 0)
		goto p_err_i2c_unlock;

	cis_data->cur_frame_us_time = frame_duration;
	cis_data->frame_length_lines = frame_length_lines;
	cis_data->max_coarse_integration_time = cis_data->frame_length_lines - cis_data->max_margin_coarse_integration_time;

	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %ldus\n", __func__, PABLO_KTIME_US_DELTA_NOW(st));

p_err_i2c_unlock:
	if (hold > 0) {
		hold = sensor_hi847_cis_group_param_hold_func(subdev, 0x0);
		if (hold < 0)
			ret = hold;
	}
	I2C_MUTEX_UNLOCK(cis->i2c_lock);

p_err:
	return ret;
}

int sensor_hi847_cis_set_frame_rate(struct v4l2_subdev *subdev, u32 min_fps)
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
		err("[%d] %s, request FPS is too high(%d), set to max(%d)\n",
			cis->id, __func__, min_fps, cis_data->max_fps);
		min_fps = cis_data->max_fps;
	}

	if (min_fps == 0) {
		err("[%d] %s, request FPS is 0, set to min FPS(1)\n",
			cis->id, __func__);
		min_fps = 1;
	}

	frame_duration = (1 * 1000 * 1000) / min_fps;

	dbg_sensor(1, "[MOD:D:%d] %s, set FPS(%d), frame duration(%d)\n",
			cis->id, __func__, min_fps, frame_duration);

	ret = sensor_hi847_cis_set_frame_duration(subdev, frame_duration);
	if (ret < 0) {
		err("[%d] %s, set frame duration is fail(%d)\n",
			cis->id, __func__, ret);
		goto p_err;
	}

#ifdef CAMERA_REAR2
	cis_data->min_frame_us_time = MAX(frame_duration, cis_data->min_sync_frame_us_time);
#else
	cis_data->min_frame_us_time = frame_duration;
#endif

	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %ldus\n", __func__, PABLO_KTIME_US_DELTA_NOW(st));

p_err:

	return ret;
}

int sensor_hi847_cis_adjust_analog_gain(struct v4l2_subdev *subdev, u32 input_again, u32 *target_permile)
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

	again_code = sensor_hi847_cis_calc_again_code(input_again);

	if (again_code > cis_data->max_analog_gain[0])
		again_code = cis_data->max_analog_gain[0];
	else if (again_code < cis_data->min_analog_gain[0])
		again_code = cis_data->min_analog_gain[0];


	again_permile = sensor_hi847_cis_calc_again_permile(again_code);

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

int sensor_hi847_cis_set_analog_gain(struct v4l2_subdev *subdev, struct ae_param *again)
{
	int ret = 0;
	int hold = 0;
	struct is_cis *cis;
	struct i2c_client *client;
	cis_shared_data *cis_data;
	ktime_t st = ktime_get();

	u16 short_gain = 0;
	u16 long_gain = 0;

	FIMC_BUG(!subdev);
	FIMC_BUG(!again);

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

	short_gain = (u16)sensor_hi847_cis_calc_again_code(again->short_val);
	long_gain = (u16)sensor_hi847_cis_calc_again_code(again->long_val);

	if (long_gain < cis->cis_data->min_analog_gain[0]) {
		info("[%d][%s] not proper long_gain value, reset to min_analog_gain\n", cis->id, __func__);
		long_gain = cis->cis_data->min_analog_gain[0];
	}
	if (long_gain > cis->cis_data->max_analog_gain[0]) {
		info("[%d][%s] not proper long_gain value, reset to max_analog_gain\n", cis->id, __func__);
		long_gain = cis->cis_data->max_analog_gain[0];
	}

	if (short_gain < cis->cis_data->min_analog_gain[0]) {
		info("[%d][%s] not proper short_gain value, reset to min_analog_gain\n", cis->id, __func__);
		short_gain = cis->cis_data->min_analog_gain[0];
	}
	if (short_gain > cis->cis_data->max_analog_gain[0]) {
		info("[%d][%s] not proper short_gain value, reset to max_analog_gain\n", cis->id, __func__);
		short_gain = cis->cis_data->max_analog_gain[0];
	}

	dbg_sensor(1, "[MOD:D:%d] %s(vsync cnt = %d),"
		KERN_CONT "input short gain = %d us, input long gain = %d us,"
		KERN_CONT "analog short gain(%#x), analog short gain(%#x)\n",
		cis->id, __func__, cis->cis_data->sen_vsync_count,
		again->short_val, again->long_val,
		short_gain, long_gain);

	I2C_MUTEX_LOCK(cis->i2c_lock);
	hold = sensor_hi847_cis_group_param_hold_func(subdev, 0x1);
	if (hold < 0) {
		ret = hold;
		goto p_err_i2c_unlock;
	}

	short_gain &= 0xFF;
	/* Short analog gain */
	ret = is_sensor_write16(client, SENSOR_HI847_ANALOG_GAIN_ADDR, short_gain);
	if (ret < 0)
		goto p_err_i2c_unlock;

	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %ldus\n", __func__, PABLO_KTIME_US_DELTA_NOW(st));

p_err_i2c_unlock:
	if (hold > 0) {
		hold = sensor_hi847_cis_group_param_hold_func(subdev, 0x0);
		if (hold < 0)
			ret = hold;
	}
	I2C_MUTEX_UNLOCK(cis->i2c_lock);

p_err:
	return ret;
}

int sensor_hi847_cis_get_analog_gain(struct v4l2_subdev *subdev, u32 *again)
{
	int ret = 0;
	int hold = 0;
	struct is_cis *cis;
	struct i2c_client *client;

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
		goto p_err;
	}

	I2C_MUTEX_LOCK(cis->i2c_lock);
	hold = sensor_hi847_cis_group_param_hold_func(subdev, 0x1);
	if (hold < 0) {
		ret = hold;
		goto p_err_i2c_unlock;
	}

	ret = is_sensor_read16(client, SENSOR_HI847_ANALOG_GAIN_ADDR, &analog_gain);
	if (ret < 0)
		goto p_err_i2c_unlock;

	*again = sensor_hi847_cis_calc_again_permile(analog_gain);

	dbg_sensor(1, "[MOD:D:%d] %s, cur_again = %d us, analog_gain(%#x)\n",
			cis->id, __func__, *again, analog_gain);

	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %ldus\n", __func__, PABLO_KTIME_US_DELTA_NOW(st));

p_err_i2c_unlock:
	if (hold > 0) {
		hold = sensor_hi847_cis_group_param_hold_func(subdev, 0x0);
		if (hold < 0)
			ret = hold;
	}
	I2C_MUTEX_UNLOCK(cis->i2c_lock);

p_err:
	return ret;
}

int sensor_hi847_cis_get_min_analog_gain(struct v4l2_subdev *subdev, u32 *min_again)
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

	cis_data->min_analog_gain[0] = SENSOR_HI847_MIN_ANALOG_GAIN_SET_VALUE;
	cis_data->min_analog_gain[1] = sensor_hi847_cis_calc_again_permile(cis_data->min_analog_gain[0]);

	*min_again = cis_data->min_analog_gain[1];

	dbg_sensor(1, "[%s] code %d, permile %d\n", __func__,
		cis_data->min_analog_gain[0], cis_data->min_analog_gain[1]);

	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %ldus\n", __func__, PABLO_KTIME_US_DELTA_NOW(st));

	return ret;
}

int sensor_hi847_cis_get_max_analog_gain(struct v4l2_subdev *subdev, u32 *max_again)
{
	int ret = 0;
	struct is_cis *cis;
	cis_shared_data *cis_data;
	ktime_t st = ktime_get();

	WARN_ON(!subdev);
	WARN_ON(!max_again);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);

	WARN_ON(!cis);
	WARN_ON(!cis->cis_data);

	cis_data = cis->cis_data;
	cis_data->max_analog_gain[0] = SENSOR_HI847_MAX_ANALOG_GAIN_SET_VALUE;
	cis_data->max_analog_gain[1] = sensor_hi847_cis_calc_again_permile(cis_data->max_analog_gain[0]);

	*max_again = cis_data->max_analog_gain[1];

	dbg_sensor(1, "[%s] code %d, permile %d\n", __func__,
		cis_data->max_analog_gain[0], cis_data->max_analog_gain[1]);

	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %ldus\n", __func__, PABLO_KTIME_US_DELTA_NOW(st));

	return ret;
}

int sensor_hi847_cis_get_digital_gain(struct v4l2_subdev *subdev, u32 *dgain)
{
	int ret = 0;
	int hold = 0;
	struct is_cis *cis;
	struct i2c_client *client;

	u16 digital_gain = 0;
	ktime_t st = ktime_get();

	FIMC_BUG(!subdev);
	FIMC_BUG(!dgain);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);

	FIMC_BUG(!cis);

	client = cis->client;

	FIMC_BUG(!client);

	I2C_MUTEX_LOCK(cis->i2c_lock);
	hold = sensor_hi847_cis_group_param_hold_func(subdev, 0x1);
	if (hold < 0) {
		ret = hold;
		goto p_err_i2c_unlock;
	}

	ret = is_sensor_read16(client, SENSOR_HI847_DIGITAL_GAIN_ADDR, &digital_gain);
	if (ret < 0)
		goto p_err_i2c_unlock;

	*dgain = sensor_hi847_cis_calc_dgain_permile(digital_gain);

	dbg_sensor(1, "[MOD:D:%d] %s, cur_dgain = %d us, digital_gain(%#x)\n",
			cis->id, __func__, *dgain, digital_gain);

	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %ldus\n", __func__, PABLO_KTIME_US_DELTA_NOW(st));

p_err_i2c_unlock:
	if (hold > 0) {
		hold = sensor_hi847_cis_group_param_hold_func(subdev, 0x0);
		if (hold < 0)
			ret = hold;
	}
	I2C_MUTEX_UNLOCK(cis->i2c_lock);

	return ret;
}

int sensor_hi847_cis_get_min_digital_gain(struct v4l2_subdev *subdev, u32 *min_dgain)
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
	cis_data->min_digital_gain[0] = SENSOR_HI847_MIN_DIGITAL_GAIN_SET_VALUE;
	cis_data->min_digital_gain[1] = sensor_hi847_cis_calc_dgain_permile(cis_data->min_digital_gain[0]);

	*min_dgain = cis_data->min_digital_gain[1];

	dbg_sensor(1, "[%s] code %d, permile %d\n", __func__,
		cis_data->min_digital_gain[0], cis_data->min_digital_gain[1]);

	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %ldus\n", __func__, PABLO_KTIME_US_DELTA_NOW(st));

	return ret;
}

int sensor_hi847_cis_get_max_digital_gain(struct v4l2_subdev *subdev, u32 *max_dgain)
{
	int ret = 0;
	struct is_cis *cis;
	cis_shared_data *cis_data;
	ktime_t st = ktime_get();

	WARN_ON(!subdev);
	WARN_ON(!max_dgain);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);

	WARN_ON(!cis);
	WARN_ON(!cis->cis_data);

	cis_data = cis->cis_data;
	cis_data->max_digital_gain[0] = SENSOR_HI847_MAX_DIGITAL_GAIN_SET_VALUE;
	cis_data->max_digital_gain[1] = sensor_hi847_cis_calc_dgain_permile(cis_data->max_digital_gain[0]);

	*max_dgain = cis_data->max_digital_gain[1];

	dbg_sensor(1, "[%s] code %d, permile %d\n", __func__,
		cis_data->max_digital_gain[0], cis_data->max_digital_gain[1]);

	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %ldus\n", __func__, PABLO_KTIME_US_DELTA_NOW(st));

	return ret;
}

static struct is_cis_ops cis_ops_hi847 = {
	.cis_init = sensor_hi847_cis_init,
	.cis_log_status = sensor_hi847_cis_log_status,
	.cis_group_param_hold = sensor_hi847_cis_group_param_hold,
	.cis_set_global_setting = sensor_hi847_cis_set_global_setting,
	.cis_mode_change = sensor_hi847_cis_mode_change,
	.cis_set_size = sensor_hi847_cis_set_size,
	.cis_stream_on = sensor_hi847_cis_stream_on,
	.cis_stream_off = sensor_hi847_cis_stream_off,
	.cis_set_exposure_time = sensor_hi847_cis_set_exposure_time,
	.cis_get_min_exposure_time = sensor_hi847_cis_get_min_exposure_time,
	.cis_get_max_exposure_time = sensor_hi847_cis_get_max_exposure_time,
	.cis_adjust_frame_duration = sensor_hi847_cis_adjust_frame_duration,
	.cis_set_frame_duration = sensor_hi847_cis_set_frame_duration,
	.cis_set_frame_rate = sensor_hi847_cis_set_frame_rate,
	.cis_adjust_analog_gain = sensor_hi847_cis_adjust_analog_gain,
	.cis_set_analog_gain = sensor_hi847_cis_set_analog_gain,
	.cis_get_analog_gain = sensor_hi847_cis_get_analog_gain,
	.cis_get_min_analog_gain = sensor_hi847_cis_get_min_analog_gain,
	.cis_get_max_analog_gain = sensor_hi847_cis_get_max_analog_gain,
	.cis_set_digital_gain = sensor_hi847_cis_set_digital_gain,
	.cis_get_digital_gain = sensor_hi847_cis_get_digital_gain,
	.cis_get_min_digital_gain = sensor_hi847_cis_get_min_digital_gain,
	.cis_get_max_digital_gain = sensor_hi847_cis_get_max_digital_gain,
	.cis_data_calculation = sensor_hi847_cis_data_calc,
	.cis_compensate_gain_for_extremely_br = sensor_cis_compensate_gain_for_extremely_br,
	.cis_wait_streamoff = sensor_hi847_cis_wait_streamoff,
	.cis_wait_streamon = sensor_hi847_cis_wait_streamon,
	.cis_check_rev_on_init = sensor_cis_check_rev_on_init,
	.cis_set_initial_exposure = sensor_cis_set_initial_exposure,
	.cis_set_test_pattern = sensor_hi847_cis_set_test_pattern,
};

static int cis_hi847_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	int ret = 0;
	u32 mclk_freq_khz;
	struct is_cis *cis = NULL;
	struct is_device_sensor_peri *sensor_peri = NULL;
	char const *setfile;
	struct device_node *dnode = client->dev.of_node;

	ret = sensor_cis_probe(client, id, &sensor_peri);
	if (ret) {
		probe_info("%s: sensor_cis_probe ret(%d)\n", __func__, ret);
		return ret;
	}

	cis = &sensor_peri->cis;
	cis->ctrl_delay = N_PLUS_TWO_FRAME;
	cis->cis_ops = &cis_ops_hi847;
	cis->rev_flag = false;
	/* belows are depend on sensor cis. MUST check sensor spec */
	// cis->bayer_order = OTF_INPUT_ORDER_BAYER_BG_GR;
	cis->bayer_order = OTF_INPUT_ORDER_BAYER_GR_BG;

	// cis->use_total_gain = true;
	// cis->use_wb_gain = true;

	ret = of_property_read_string(dnode, "setfile", &setfile);
	if (ret) {
		err("setfile index read fail(%d), take default setfile!!", ret);
		setfile = "default";
	}

	mclk_freq_khz = sensor_peri->module->pdata->mclk_freq_khz;

	if (strcmp(setfile, "default") == 0 || strcmp(setfile, "setA") == 0) {
		probe_info("%s setfile_A mclk: 19.2MHz\n", __func__);
		sensor_hi847_global = sensor_hi847_setfile_A_19p2_Global;
		sensor_hi847_global_size = ARRAY_SIZE(sensor_hi847_setfile_A_19p2_Global);
		sensor_hi847_setfiles = sensor_hi847_setfiles_A_19p2;
		sensor_hi847_setfile_sizes = sensor_hi847_setfiles_A_19p2_sizes;
		sensor_hi847_pllinfos = sensor_hi847_pllinfos_A_19p2;
		sensor_hi847_max_setfile_num = ARRAY_SIZE(sensor_hi847_pllinfos_A_19p2);
	} else {
		err("%s setfile index out of bound, take default (setfile_A)", __func__);
		sensor_hi847_global = sensor_hi847_setfile_A_19p2_Global;
		sensor_hi847_global_size = ARRAY_SIZE(sensor_hi847_setfile_A_19p2_Global);
		sensor_hi847_setfiles = sensor_hi847_setfiles_A_19p2;
		sensor_hi847_setfile_sizes = sensor_hi847_setfiles_A_19p2_sizes;
		sensor_hi847_pllinfos = sensor_hi847_pllinfos_A_19p2;
		sensor_hi847_max_setfile_num = ARRAY_SIZE(sensor_hi847_pllinfos_A_19p2);
	}

	probe_info("%s done\n", __func__);
	return ret;
}

static const struct of_device_id sensor_cis_hi847_match[] = {
	{
		.compatible = "samsung,exynos-is-cis-hi847",
	},
	{},
};
MODULE_DEVICE_TABLE(of, sensor_cis_hi847_match);

static const struct i2c_device_id sensor_cis_hi847_idt[] = {
	{ SENSOR_NAME, 0 },
	{},
};

static struct i2c_driver sensor_cis_hi847_driver = {
	.probe	= cis_hi847_probe,
	.driver = {
		.name	= SENSOR_NAME,
		.owner	= THIS_MODULE,
		.of_match_table = sensor_cis_hi847_match,
		.suppress_bind_attrs = true,
	},
	.id_table = sensor_cis_hi847_idt
};

#ifdef MODULE
builtin_i2c_driver(sensor_cis_hi847_driver);
#else
static int __init sensor_cis_hi847_init(void)
{
	int ret;

	ret = i2c_add_driver(&sensor_cis_hi847_driver);
	if (ret)
		err("failed to add %s driver: %d\n",
			sensor_cis_hi847_driver.driver.name, ret);

	return ret;
}
late_initcall_sync(sensor_cis_hi847_init);
#endif

MODULE_LICENSE("GPL");
MODULE_SOFTDEP("pre: fimc-is");
