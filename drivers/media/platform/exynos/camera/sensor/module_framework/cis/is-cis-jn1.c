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
#include "is-cis-jn1.h"
#include "is-cis-jn1-setA.h"

#include "is-helper-ixc.h"
#ifdef CONFIG_CAMERA_VENDER_MCD_V2
#include "is-sec-define.h"
#endif

#include "interface/is-interface-library.h"
#include "is-interface-sensor.h"
#define SENSOR_NAME "S5KJN1"
/* #define DEBUG_JN1_PLL */

static const u32 *sensor_jn1_global;
static u32 sensor_jn1_global_size;
static const u32 *sensor_jn1_dual_sync;
static u32 sensor_jn1_dual_sync_size;

#if WRITE_SENSOR_CAL_FOR_HW_GGC
int sensor_jn1_cis_HW_GGC_write(struct v4l2_subdev *subdev)
{
	int ret = 0;
	int i = 0;
	struct is_cis *cis = sensor_cis_get_cis(subdev);

	char *rom_cal_buf = NULL;
	u16 start_addr, data_size, write_data;

	ret = is_sec_get_cal_buf(&rom_cal_buf, ROM_ID_REAR);
	if (ret < 0) {
		goto p_err;
	}

	/* Big Endian */
	start_addr = SENSOR_JN1_HW_GGC_CAL_BASE_REAR;
	data_size = SENSOR_JN1_HW_GGC_CAL_SIZE;
	rom_cal_buf += start_addr;

#if SENSOR_JN1_CAL_DEBUG
	ret = sensor_jn1_cis_cal_dump(SENSOR_JN1_GGC_DUMP_NAME, (char *)rom_cal_buf, (size_t)SENSOR_JN1_HW_GGC_CAL_SIZE);
	if (ret < 0) {
		err("sensor_jn1_cis_cal_dump fail(%d)!\n", ret);
		goto p_err;
	}
#endif

	if (rom_cal_buf[0] == 0xFF && rom_cal_buf[1] == 0x00) {
		cis->ixc_ops->write16(cis->client, 0x6028, 0x2400);
		cis->ixc_ops->write16(cis->client, 0x602A, 0x0CFC);
		
		for (i = 0; i < (data_size/2) ; i++) {
			write_data = ((rom_cal_buf[2*i] << 8) | rom_cal_buf[2*i + 1]);
			cis->ixc_ops->write16(cis->client, 0x6F12, write_data);
		}
		cis->ixc_ops->write16(cis->client, 0x6028, 0x2400);
		cis->ixc_ops->write16(cis->client, 0x602A, 0x2138);
		cis->ixc_ops->write16(cis->client, 0x6F12, 0x4000);
	} else {
		err("sensor_jn1_cis_GGC_write skip : (%#x, %#x) \n", rom_cal_buf[0] , rom_cal_buf[1]);
		goto p_err;
	}

	dbg_sensor(1, "[%s] HW GGC write done\n", __func__);

/* for Test
	sensor_jn1_i2c_read16(client, SENSOR_JN1_HW_GGC_CAL_BASE_REAR, 5);
*/

p_err:
	return ret;
}
#endif

/* CIS OPS */
int sensor_jn1_cis_init(struct v4l2_subdev *subdev)
{
	int ret = 0;
	ktime_t st = ktime_get();
	struct is_cis *cis = sensor_cis_get_cis(subdev);

	cis->cis_data->stream_on = false;
	cis->cis_data->cur_width = cis->sensor_info->max_width;
	cis->cis_data->cur_height = cis->sensor_info->max_height;
	cis->cis_data->low_expo_start = 33000;
	cis->need_mode_change = false;
	cis->long_term_mode.sen_strm_off_on_step = 0;
	cis->long_term_mode.sen_strm_off_on_enable = false;

	cis->mipi_clock_index_cur = CAM_MIPI_NOT_INITIALIZED;
	cis->mipi_clock_index_new = CAM_MIPI_NOT_INITIALIZED;
	cis->cis_data->sens_config_index_pre = SENSOR_JN1_MODE_MAX;
	cis->cis_data->sens_config_index_cur = 0;

	CALL_CISOPS(cis, cis_data_calculation, subdev, cis->cis_data->sens_config_index_cur);

	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %ldus", __func__, PABLO_KTIME_US_DELTA_NOW(st));

	return ret;
}

static const struct is_cis_log log_jn1[] = {
	{I2C_READ, 16, 0x0000, 0, "model_id"},
	{I2C_READ, 8, 0x0002, 0, "rev_number"},
	{I2C_READ, 8, 0x0005, 0, "frame_count"},
	/* 4000 page */
	{I2C_WRITE, 16, 0x6000, 0x0005, "page unlock"},
	{I2C_WRITE, 16, 0xFCFC, 0x4000, "0x4000 page"},
	{I2C_READ, 16, 0x0100, 0, "mode_select"},
	{I2C_READ, 16, 0x0300, 0, "0x0300"},
	{I2C_READ, 16, 0x0302, 0, "0x0302"},
	{I2C_READ, 16, 0x0304, 0, "0x0304"},
	{I2C_READ, 16, 0x0306, 0, "0x0306"},
	{I2C_READ, 16, 0x0308, 0, "0x0308"},
	{I2C_READ, 16, 0x030a, 0, "0x030A"},
	{I2C_READ, 16, 0x030c, 0, "0x030C"},
	{I2C_READ, 16, 0x030e, 0, "0x030E"},
	{I2C_READ, 16, 0x0310, 0, "0x0310"},
	{I2C_READ, 16, 0x0340, 0, "fll"},
	{I2C_READ, 16, 0x0342, 0, "llp"},
	/* 0x2400 page */
	{I2C_WRITE, 16, 0xFCFC, 0x2400, "0x2400 page"},
	{I2C_READ, 16, 0x37C0, 0, "0x37C0"}, /* m_sys_debug_first_error */
	{I2C_READ, 16, 0x37C4, 0, "0x37C4"}, /* m_sys_debug_first_error_info */
	{I2C_READ, 16, 0x37C8, 0, "0x37C8"}, /* m_sys_debug_last_error */
	{I2C_READ, 16, 0x37CC, 0, "0x37CC"}, /* m_sys_debug_last_error_info */
	/* restore 4000 page */
	{I2C_WRITE, 16, 0xFCFC, 0x4000, "0x4000 page"},
	{I2C_WRITE, 16, 0x6000, 0x0085, "page lock"},
};

int sensor_jn1_cis_log_status(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_cis *cis = sensor_cis_get_cis(subdev);

	sensor_cis_log_status(cis, cis->client, log_jn1, ARRAY_SIZE(log_jn1), (char *)__func__);

	return ret;
}

static int sensor_jn1_cis_group_param_hold_func(struct v4l2_subdev *subdev, unsigned int hold)
{
#if USE_GROUP_PARAM_HOLD
	int ret = 0;
	struct is_cis *cis = sensor_cis_get_cis(subdev);
	struct i2c_client *client = NULL;

	client = cis->client;
	if (unlikely(!client)) {
		err("client is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	if (hold == cis->cis_data->group_param_hold) {
		pr_debug("already group_param_hold (%d)\n", cis->cis_data->group_param_hold);
		goto p_err;
	}

	ret = cis->ixc_ops->write8(client, 0x0104, hold);
	if (ret < 0)
		goto p_err;

	cis->cis_data->group_param_hold = hold;
	ret = 1;
p_err:
	return ret;
#else
	return 0;
#endif
}

/*
  hold control register for updating multiple-parameters within the same frame. 
  true : hold, flase : no hold/release
*/
#if USE_GROUP_PARAM_HOLD
int sensor_jn1_cis_group_param_hold(struct v4l2_subdev *subdev, bool hold)
{
	int ret = 0;
	struct is_cis *cis = NULL;
	u32 mode;

	FIMC_BUG(!subdev);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);

	FIMC_BUG(!cis);
	FIMC_BUG(!cis->cis_data);
 
	if (cis->cis_data->stream_on == false && hold == true) {
		ret = 0;
		dbg_sensor(1, "%s : sensor stream off skip group_param_hold", __func__);
		goto p_err;
	}

	mode = cis->cis_data->sens_config_index_cur;

	if (mode == SENSOR_JN1_A2A2_2032x1524_120FPS) {
		ret = 0;
		dbg_sensor(1, "%s : fast ae skip group_param_hold", __func__);
		goto p_err;
	}

	I2C_MUTEX_LOCK(cis->i2c_lock);

	ret = sensor_jn1_cis_group_param_hold_func(subdev, hold);
	if (ret < 0)
		goto p_err_unlock;
	
p_err_unlock:
	I2C_MUTEX_UNLOCK(cis->i2c_lock);

p_err:
	return ret;
}
#endif

int sensor_jn1_cis_set_global_setting(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_cis *cis = sensor_cis_get_cis(subdev);

	dbg_sensor(1, "[%s] start\n", __func__);
	I2C_MUTEX_LOCK(cis->i2c_lock);

	ret = sensor_cis_set_registers(subdev, sensor_jn1_global, sensor_jn1_global_size);

	if (ret < 0) {
		err("sensor_jn1_global fail!!");
		goto p_err_unlock;
	}

	dbg_sensor(1, "[%s] done\n", __func__);

#if WRITE_SENSOR_CAL_FOR_HW_GGC
	sensor_jn1_cis_HW_GGC_write(subdev);
#endif

p_err_unlock:
	I2C_MUTEX_UNLOCK(cis->i2c_lock);
	return ret;
}

int sensor_jn1_cis_mode_change(struct v4l2_subdev *subdev, u32 mode)
{
	int ret = 0;
	const struct sensor_cis_mode_info *mode_info;
	struct is_cis *cis = sensor_cis_get_cis(subdev);

	if (mode >= cis->sensor_info->mode_count) {
		err("invalid mode(%d)!!", mode);
		ret = -EINVAL;
		goto p_err;
	}

	mode_info = cis->sensor_info->mode_infos[mode];

	cis->mipi_clock_index_cur = CAM_MIPI_NOT_INITIALIZED;

	I2C_MUTEX_LOCK(cis->i2c_lock);

	info("[%s] sensor mode(%d)\n", __func__, mode);

	ret = sensor_cis_set_registers(subdev, mode_info->setfile, mode_info->setfile_size);
	if (ret < 0) {
		err("sensor_setfiles fail!!");
		goto p_err_unlock;
	}

#ifdef APPLY_MIRROR_VERTICAL_FLIP
	/* Page Select */
	ret = cis->ixc_ops->write16(cis->client, 0x6028, 0x4000);
	if (ret < 0)
		 goto p_err_unlock;
	/* Apply Mirror and Vertical Flip */
	ret = cis->ixc_ops->write8(cis->client, 0x0101, 0x03);
	if (ret < 0)
		 goto p_err_unlock;
#endif

	dbg_sensor(1, "[%s] mode changed(%d)\n", __func__, mode);

p_err_unlock:
	I2C_MUTEX_UNLOCK(cis->i2c_lock);

p_err:
	return ret;
}

int sensor_jn1_cis_stream_on(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_cis *cis = sensor_cis_get_cis(subdev);
	cis_shared_data *cis_data;
	struct is_device_sensor *device;
	ktime_t st = ktime_get();

	device = (struct is_device_sensor *)v4l2_get_subdev_hostdata(subdev);
	WARN_ON(!device);

	cis_data = cis->cis_data;

	dbg_sensor(1, "[MOD:D:%d] %s\n", cis->id, __func__);

	if (cis->stream_state != CIS_STREAM_SET_DONE) {
		err("%s: cis stream state id %d", __func__, cis->stream_state);
		return -EINVAL;
	}

	//is_vendor_set_mipi_clock(device);

	I2C_MUTEX_LOCK(cis->i2c_lock);

	/* Sensor stream on */
	ret = cis->ixc_ops->write16(cis->client, 0x6028, 0x4000);
	ret |= cis->ixc_ops->write8(cis->client, 0x0100, 0x01);
	cis_data->stream_on = true;
	info("%s done\n", __func__);

	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %ldus", __func__, PABLO_KTIME_US_DELTA_NOW(st));

	I2C_MUTEX_UNLOCK(cis->i2c_lock);
	return ret;
}

int sensor_jn1_cis_stream_off(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_cis *cis = sensor_cis_get_cis(subdev);
	cis_shared_data *cis_data;
	u8 cur_frame_count = 0;
	ktime_t st = ktime_get();

	cis_data = cis->cis_data;

	dbg_sensor(1, "[MOD:D:%d] %s\n", cis->id, __func__);

	cis_data->stream_on = false; /* for not working group_param_hold after stream off */

	I2C_MUTEX_LOCK(cis->i2c_lock);

	ret = sensor_jn1_cis_group_param_hold_func(subdev, 0x00);
	if (ret < 0)
		err("group_param_hold_func failed at stream off");

	cis->ixc_ops->read8(cis->client, 0x0005, &cur_frame_count);

	/* Sensor stream off */
	ret = cis->ixc_ops->write16(cis->client, 0x6028, 0x4000);
	ret |= cis->ixc_ops->write8(cis->client, 0x0100, 0x00);
	info("%s done, frame_count(%d)\n", __func__, cur_frame_count);

	I2C_MUTEX_UNLOCK(cis->i2c_lock);
	
	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %ldus", __func__, PABLO_KTIME_US_DELTA_NOW(st));

	return ret;
}

int sensor_jn1_cis_set_exposure_time(struct v4l2_subdev *subdev, struct ae_param *target_exposure)
{
	int ret = 0;
	struct is_cis *cis = sensor_cis_get_cis(subdev);
	cis_shared_data *cis_data;
	struct is_long_term_expo_mode *lte_mode;

	u64 vt_pic_clk_freq_khz = 0;
	u16 short_coarse_int = 0;
	u32 line_length_pck = 0;
	u32 min_fine_int = 0;
	ktime_t st = ktime_get();

	u32 target_exp = 0;
	u32 target_frame_duration = 0;
	u16 frame_length_lines = 0;
	u64 max_fll_frame_time;

	unsigned char cit_lshift_val = 0;
	int cit_lshift_count = 0;

	FIMC_BUG(!target_exposure);

	if ((target_exposure->long_val <= 0) || (target_exposure->short_val <= 0)) {
		err("[%s] invalid target exposure(%d, %d)\n", __func__,
				target_exposure->long_val, target_exposure->short_val);
		ret = -EINVAL;
		goto p_err;
	}

	cis_data = cis->cis_data;
	lte_mode = &cis->long_term_mode;

	dbg_sensor(1, "[MOD:D:%d] %s, vsync_cnt(%d), target long(%d), short(%d)\n", cis->id, __func__,
			cis_data->sen_vsync_count, target_exposure->long_val, target_exposure->short_val);

	target_exp = target_exposure->val;
	vt_pic_clk_freq_khz = cis_data->pclk / 1000;
	line_length_pck = cis_data->line_length_pck;
	min_fine_int = cis_data->min_fine_integration_time;

	dbg_sensor(1, "[MOD:D:%d] %s, vt_pic_clk_freq_khz (%d), line_length_pck(%d), min_fine_int (%d)\n",
		cis->id, __func__, vt_pic_clk_freq_khz, line_length_pck, min_fine_int);

	max_fll_frame_time = (65535 * (u64)line_length_pck) / (u64)(vt_pic_clk_freq_khz / 1000);

	/*
	 * For Long Exposure Mode without stream on_off. (ex. Night Hyper Laps: min exp. is 1.5sec)
	 * If frame duration over than 1sec, then sequence is same as below
	 * 1. set CIT_LSHFIT
	 * 2. set COARSE_INTEGRATION_TIME
	 * 3. set FRM_LENGTH_LINES
	 */
	if ((cis_data->is_data.scene_mode == AA_SCENE_MODE_SUPER_NIGHT || cis_data->is_data.scene_mode == AA_SCENE_MODE_HYPERLAPSE)
		&& lte_mode->sen_strm_off_on_enable == false && cis_data ->cur_frame_us_time > max_fll_frame_time) {
		target_frame_duration = cis_data->cur_frame_us_time;
		dbg_sensor(1, "[MOD:D:%d] %s, input frame duration(%d) for CIT SHIFT \n",
			cis->id, __func__, target_frame_duration);

		if (target_frame_duration > 100000) {
			cit_lshift_val = (unsigned char)(target_frame_duration / 100000);
			while(cit_lshift_val > 1) {
				cit_lshift_val /= 2;
				target_frame_duration /= 2;
				target_exp /= 2;
				cit_lshift_count++;
			}

			if (cit_lshift_count > SENSOR_JN1_MAX_CIT_LSHIFT_VALUE)
				cit_lshift_count = SENSOR_JN1_MAX_CIT_LSHIFT_VALUE;
		}

		frame_length_lines = (u16)((vt_pic_clk_freq_khz / 1000 * target_frame_duration) / (line_length_pck));

		cis_data->frame_length_lines = frame_length_lines;
		cis_data->frame_length_lines_shifter = cit_lshift_count;
		cis_data->max_coarse_integration_time =
			frame_length_lines - cis_data->max_margin_coarse_integration_time;

		dbg_sensor(1, "[MOD:D:%d] %s, target_frame_duration(%d), frame_length_line(%d), cit_lshift_count(%d)\n",
			cis->id, __func__, target_frame_duration, frame_length_lines, cit_lshift_count);
	}

	short_coarse_int = ((target_exp * vt_pic_clk_freq_khz) / 1000 - min_fine_int) / line_length_pck;

	if (short_coarse_int > cis_data->max_coarse_integration_time) {
		dbg_sensor(1, "[MOD:D:%d] %s, vsync_cnt(%d), short coarse(%d) max(%d)\n", cis->id, __func__,
			cis_data->sen_vsync_count, short_coarse_int, cis_data->max_coarse_integration_time);
		short_coarse_int = cis_data->max_coarse_integration_time;
	}

	if (short_coarse_int < cis_data->min_coarse_integration_time) {
		dbg_sensor(1, "[MOD:D:%d] %s, vsync_cnt(%d), short coarse(%d) min(%d)\n", cis->id, __func__,
			cis_data->sen_vsync_count, short_coarse_int, cis_data->min_coarse_integration_time);
		short_coarse_int = cis_data->min_coarse_integration_time;
	}

	I2C_MUTEX_LOCK(cis->i2c_lock);
	if (lte_mode->sen_strm_off_on_enable == false && cis_data->min_frame_us_time > max_fll_frame_time) {
		if (cit_lshift_count > 0) {
			ret = cis->ixc_ops->write8(cis->client, cis->reg_addr->cit_shifter, cit_lshift_count);
			if (ret < 0)
				goto i2c_err;

			ret = cis->ixc_ops->write8(cis->client, cis->reg_addr->fll_shifter, cit_lshift_count);
			if (ret < 0)
				goto i2c_err;
		}
	}

	/* Short exposure */
	ret = cis->ixc_ops->write16(cis->client, cis->reg_addr->cit, short_coarse_int);
	if (ret < 0)
		goto i2c_err;


	if (lte_mode->sen_strm_off_on_enable == false && cis_data->min_frame_us_time > max_fll_frame_time) {
		ret = cis->ixc_ops->write16(cis->client, cis->reg_addr->fll, frame_length_lines);
		if (ret < 0)
			goto i2c_err;
	}

	dbg_sensor(1, "[MOD:D:%d] %s, vsync_cnt(%d), vt_pic_clk_freq_khz (%d),"
		KERN_CONT "line_length_pck(%d), min_fine_int (%d)\n", cis->id, __func__,
		cis_data->sen_vsync_count, vt_pic_clk_freq_khz, line_length_pck, min_fine_int);
	dbg_sensor(1, "[MOD:D:%d] %s, pixelrate(%zu), llp(%d), fll(%d), min_fine_int(%d) cit(%d)\n", cis->id, __func__,
		vt_pic_clk_freq_khz, line_length_pck, cis_data->frame_length_lines, min_fine_int, short_coarse_int);

	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %ldus", __func__, PABLO_KTIME_US_DELTA_NOW(st));

i2c_err:
	I2C_MUTEX_UNLOCK(cis->i2c_lock);

p_err:
	return ret;
}

int sensor_jn1_cis_adjust_frame_duration(struct v4l2_subdev *subdev,
						u32 input_exposure_time,
						u32 *target_duration)
{
	int ret = 0;
	struct is_cis *cis = sensor_cis_get_cis(subdev);
	cis_shared_data *cis_data;

	u64 vt_pic_clk_freq_khz = 0;
	u32 line_length_pck = 0;
	u32 coarse_integ_time = 0;
	u32 frame_length_lines = 0;
	u32 frame_duration = 0;
	ktime_t st = ktime_get();

	FIMC_BUG(!target_duration);

	cis_data = cis->cis_data;

	if (input_exposure_time == 0) {
		input_exposure_time  = cis_data->min_frame_us_time;
		info("[%s] Not proper exposure time(0), so apply min frame duration to exposure time forcely!!!(%d)\n",
			__func__, cis_data->min_frame_us_time);
	}

	vt_pic_clk_freq_khz = cis_data->pclk / 1000;
	line_length_pck = cis_data->line_length_pck;
	coarse_integ_time = (u32)(((vt_pic_clk_freq_khz * input_exposure_time) / 1000
									- cis_data->min_fine_integration_time) / line_length_pck);

	if (cis->min_fps == cis->max_fps && cis->max_fps <= 60) {
		/*
		 * If input exposure is setted as 33333us@30fps from ddk
		 * then calculated frame duration is larger than 33333us because of CIT MARGIN.
		 */
		dbg_sensor(1, "[%s] requested min_fps(%d), max_fps(%d) from HAL\n", __func__, cis->min_fps, cis->max_fps);

		if (coarse_integ_time > cis_data->max_coarse_integration_time) {
			dbg_sensor(1, "[MOD:D:%d] %s, vsync_cnt(%d), input exp(%d) coarse(%u) max(%u)\n", cis->id, __func__,
				cis_data->sen_vsync_count, input_exposure_time, coarse_integ_time, cis_data->max_coarse_integration_time);
			coarse_integ_time = cis_data->max_coarse_integration_time;
		}

		if (coarse_integ_time < cis_data->min_coarse_integration_time) {
			dbg_sensor(1, "[MOD:D:%d] %s, vsync_cnt(%d), input exp(%d) coarse(%u) min(%u)\n", cis->id, __func__,
				cis_data->sen_vsync_count, input_exposure_time, coarse_integ_time, cis_data->min_coarse_integration_time);
			coarse_integ_time = cis_data->min_coarse_integration_time;
		}
	}

	frame_length_lines = coarse_integ_time + cis_data->max_margin_coarse_integration_time;
	frame_duration = (u32)(((u64)frame_length_lines * line_length_pck) * 1000 / vt_pic_clk_freq_khz);

	dbg_sensor(1, "[%s](vsync cnt = %d) input exp(%d), adj duration, frame duration(%d), min_frame_us(%d)\n",
			__func__, cis_data->sen_vsync_count, input_exposure_time, frame_duration, cis_data->min_frame_us_time);

	if (cis->long_term_mode.sen_strm_off_on_enable == false) {
		*target_duration = MAX(frame_duration, cis_data->min_frame_us_time);
	} else {
		*target_duration = frame_duration;
	}
	dbg_sensor(1, "[%s] calcurated frame_duration(%d), adjusted frame_duration(%d)\n", __func__, frame_duration, *target_duration);

	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %ldus", __func__, PABLO_KTIME_US_DELTA_NOW(st));

	return ret;
}

int sensor_jn1_cis_set_frame_duration(struct v4l2_subdev *subdev, u32 frame_duration)
{
	int ret = 0;
	struct is_cis *cis = sensor_cis_get_cis(subdev);
	cis_shared_data *cis_data;
	struct is_long_term_expo_mode *lte_mode;

	u64 vt_pic_clk_freq_khz = 0;
	u32 line_length_pck = 0;
	u16 frame_length_lines = 0;
	u64 max_fll_frame_time;
	ktime_t st = ktime_get();

	cis_data = cis->cis_data;
	lte_mode = &cis->long_term_mode;

	if (frame_duration < cis_data->min_frame_us_time) {
		dbg_sensor(1, "frame duration is less than min(%d)\n", frame_duration);
		frame_duration = cis_data->min_frame_us_time;
	}

	vt_pic_clk_freq_khz = cis_data->pclk / 1000;
	line_length_pck = cis_data->line_length_pck;

	max_fll_frame_time = (65535 * (u64)line_length_pck) / (u64)(vt_pic_clk_freq_khz / 1000);

	/*
	 * For Long Exposure Mode without stream on_off. (ex. Night HyperLaps)
	 * If frame duration over than 1sec, then it has to be applied CIT shift.
	 * In this case, frame_duration is setted in set_exposure_time with CIT shift.
	 */
	if (lte_mode->sen_strm_off_on_enable == false && cis_data->min_frame_us_time > max_fll_frame_time) {
		cis_data->cur_frame_us_time = frame_duration;
		dbg_sensor(1, "[MOD:D:%d][%s] Skip set frame duration(%d) for CIT SHIFT.\n",
			cis->id, __func__, frame_duration);
		return ret;
	}

	frame_length_lines = (u16)((vt_pic_clk_freq_khz * frame_duration) / (line_length_pck * 1000));

	dbg_sensor(1, "[MOD:D:%d] %s, vt_pic_clk_freq_khz(%#x) frame_duration = %d us,"
		KERN_CONT "(line_length_pck%#x), frame_length_lines(%#x)\n",
		cis->id, __func__, vt_pic_clk_freq_khz, frame_duration, line_length_pck, frame_length_lines);

	I2C_MUTEX_LOCK(cis->i2c_lock);

	if (lte_mode->sen_strm_off_on_enable == false && cis_data->frame_length_lines_shifter > 0) {
		cis_data->frame_length_lines_shifter = 0;
		ret = cis->ixc_ops->write8(cis->client, cis->reg_addr->cit_shifter, 0);
		if (ret < 0) {
			goto p_err_unlock;
		}
		ret = cis->ixc_ops->write8(cis->client, cis->reg_addr->fll_shifter, 0);
		if (ret < 0)
			goto p_err_unlock;
	}

	ret = cis->ixc_ops->write16(cis->client, cis->reg_addr->fll, frame_length_lines);
	if (ret < 0)
		goto p_err_unlock;

	cis_data->cur_frame_us_time = frame_duration;
	cis_data->frame_length_lines = frame_length_lines;
	if (lte_mode->sen_strm_off_on_enable) {
		cis_data->max_coarse_integration_time =
			SENSOR_JN1_MAX_COARSE_INTEG_WITH_FRM_LENGTH_CTRL - cis_data->max_margin_coarse_integration_time;
	} else {
		cis_data->max_coarse_integration_time = cis_data->frame_length_lines - cis_data->max_margin_coarse_integration_time;
	}

	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %ldus", __func__, PABLO_KTIME_US_DELTA_NOW(st));

p_err_unlock:
	I2C_MUTEX_UNLOCK(cis->i2c_lock);
	return ret;
}

int sensor_jn1_cis_set_digital_gain(struct v4l2_subdev *subdev, struct ae_param *dgain)
{
	int ret = 0;
	struct is_cis *cis = sensor_cis_get_cis(subdev);
	cis_shared_data *cis_data;

	u16 dgain_code = 0;
	u16 dgains[4] = {0};
	ktime_t st = ktime_get();

	FIMC_BUG(!dgain);

	cis_data = cis->cis_data;

	dgain_code = (u16)sensor_cis_calc_dgain_code(dgain->val);

	if (dgain_code < cis_data->min_digital_gain[0])
		dgain_code = cis_data->min_digital_gain[0];

	if (dgain_code > cis_data->max_digital_gain[0])
		dgain_code = cis_data->max_digital_gain[0];

	dbg_sensor(1, "[MOD:D:%d] %s(vsync cnt = %d), input_dgain = %d us, dgain_code(%#x)\n",
			cis->id, __func__, cis->cis_data->sen_vsync_count, dgain->val, dgain_code);

	dgains[0] = dgains[1] = dgains[2] = dgains[3] = dgain_code;

	I2C_MUTEX_LOCK(cis->i2c_lock);

	ret = cis->ixc_ops->write16_array(cis->client, cis->reg_addr->dgain, dgains, 4);
	if (ret < 0)
		goto p_err_unlock;

	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %ldus", __func__, PABLO_KTIME_US_DELTA_NOW(st));

p_err_unlock:
	I2C_MUTEX_UNLOCK(cis->i2c_lock);
	return ret;
}


int sensor_jn1_cis_set_dual_setting(struct v4l2_subdev *subdev, u32 mode)
{
	int ret = 0;

	info("[%s] sync mode(%d)\n", __func__, mode);
	switch (mode) {
	case DUAL_SYNC_MASTER:
		ret = sensor_cis_set_registers(subdev, sensor_jn1_dual_sync, sensor_jn1_dual_sync_size);
		if (ret)
			err("jn1 dual sync setting fail");
		break;
	case DUAL_SYNC_SLAVE:
		break;
	case DUAL_SYNC_STANDALONE:
		break;
	default:
		err("%s: invalid mode(%d)\n", __func__, mode);
		ret = -EINVAL;
	}

	return ret;
}

int sensor_jn1_cis_long_term_exposure(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_cis *cis = sensor_cis_get_cis(subdev);
	struct is_long_term_expo_mode *lte_mode;
	unsigned char cit_lshift_val = 0;
	int cit_lshift_count = 0;
	u32 target_exp = 0;

	lte_mode = &cis->long_term_mode;

	I2C_MUTEX_LOCK(cis->i2c_lock);
	/* LTE mode or normal mode set */
	if (lte_mode->sen_strm_off_on_enable) {
		target_exp = lte_mode->expo[0];
		if (target_exp >= 125000) {
			cit_lshift_val = (unsigned char)(target_exp / 125000);
			while (cit_lshift_val > 1) {
				cit_lshift_val /= 2;
				target_exp /= 2;
				cit_lshift_count++;
			}

			lte_mode->expo[0] = target_exp;

			if (cit_lshift_count > SENSOR_JN1_MAX_CIT_LSHIFT_VALUE)
				cit_lshift_count = SENSOR_JN1_MAX_CIT_LSHIFT_VALUE;
		}
	}

	ret = cis->ixc_ops->write8(cis->client, cis->reg_addr->cit_shifter, cit_lshift_count);
	ret |= cis->ixc_ops->write8(cis->client, cis->reg_addr->fll_shifter, cit_lshift_count);

	I2C_MUTEX_UNLOCK(cis->i2c_lock);

	info("[%s] sen_strm_enable(%d), cit_lshift_count (%d), target_exp(%d)", __func__,
		lte_mode->sen_strm_off_on_enable, cit_lshift_count, lte_mode->expo[0]);

	if (ret < 0)
		pr_err("ERR[%s]: LTE register setting fail\n", __func__);
	return ret;
}

static struct is_cis_ops cis_ops = {
	.cis_init = sensor_jn1_cis_init,
	.cis_log_status = sensor_jn1_cis_log_status,
#if USE_GROUP_PARAM_HOLD
	.cis_group_param_hold = sensor_jn1_cis_group_param_hold,
#endif
	.cis_set_global_setting = sensor_jn1_cis_set_global_setting,
	.cis_mode_change = sensor_jn1_cis_mode_change,
	.cis_stream_on = sensor_jn1_cis_stream_on,
	.cis_stream_off = sensor_jn1_cis_stream_off,
	.cis_set_exposure_time = sensor_jn1_cis_set_exposure_time,
	.cis_get_min_exposure_time = sensor_cis_get_min_exposure_time,
	.cis_get_max_exposure_time = sensor_cis_get_max_exposure_time,
	.cis_adjust_frame_duration = sensor_jn1_cis_adjust_frame_duration,
	.cis_set_frame_duration = sensor_jn1_cis_set_frame_duration,
	.cis_set_frame_rate = sensor_cis_set_frame_rate,
	.cis_adjust_analog_gain = sensor_cis_adjust_analog_gain,
	.cis_set_analog_gain = sensor_cis_set_analog_gain,
	.cis_get_analog_gain = sensor_cis_get_analog_gain,
	.cis_get_min_analog_gain = sensor_cis_get_min_analog_gain,
	.cis_get_max_analog_gain = sensor_cis_get_max_analog_gain,
	.cis_calc_again_code = sensor_cis_calc_again_code,
	.cis_calc_again_permile = sensor_cis_calc_again_permile,
	.cis_set_digital_gain = sensor_jn1_cis_set_digital_gain,
	.cis_get_digital_gain = sensor_cis_get_digital_gain,
	.cis_get_min_digital_gain = sensor_cis_get_min_digital_gain,
	.cis_get_max_digital_gain = sensor_cis_get_max_digital_gain,
	.cis_calc_dgain_code = sensor_cis_calc_dgain_code,
	.cis_calc_dgain_permile = sensor_cis_calc_dgain_permile,
	.cis_compensate_gain_for_extremely_br = sensor_cis_compensate_gain_for_extremely_br,
	.cis_wait_streamoff = sensor_cis_wait_streamoff,
	.cis_wait_streamon = sensor_cis_wait_streamon,
	.cis_set_initial_exposure = sensor_cis_set_initial_exposure,
	.cis_check_rev_on_init = sensor_cis_check_rev_on_init,
	.cis_active_test = sensor_cis_active_test,
	.cis_set_long_term_exposure = sensor_jn1_cis_long_term_exposure,
	.cis_set_dual_setting = sensor_jn1_cis_set_dual_setting,
	.cis_data_calculation = sensor_cis_data_calculation,
};

static int cis_jn1_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	int ret;
	struct is_cis *cis;
	struct is_device_sensor_peri *sensor_peri;
	char const *setfile;

	struct device_node *dnode = client->dev.of_node;

	probe_info("%s: sensor_cis_probe started\n", __func__);	//custom
	ret = sensor_cis_probe(client,  &(client->dev), &sensor_peri,I2C_TYPE);
	if (ret) {
		probe_info("%s: sensor_cis_probe ret(%d)\n", __func__, ret);
		return ret;
	}

	cis = &sensor_peri->cis;
	cis->ctrl_delay = N_PLUS_TWO_FRAME;
	cis->cis_ops = &cis_ops;
	/* belows are depend on sensor cis. MUST check sensor spec */
#ifdef APPLY_MIRROR_VERTICAL_FLIP
	cis->bayer_order = OTF_INPUT_ORDER_BAYER_GB_RG;
#else
	cis->bayer_order = OTF_INPUT_ORDER_BAYER_GR_BG;
#endif
	cis->reg_addr = &sensor_jn1_reg_addr;

	ret = of_property_read_string(dnode, "setfile", &setfile);
	if (ret) {
		err("setfile index read fail(%d), take default setfile!!", ret);
		setfile = "default";
	}

	if (strcmp(setfile, "default") == 0 || strcmp(setfile, "setA") == 0)
		probe_info("[%s] setfile_A mclk: 26Mhz \n", __func__);
	else
		err("setfile index out of bound, take default (setfile_A mclk: 26Mhz)");

	sensor_jn1_global = sensor_jn1_setfile_A_Global;
	sensor_jn1_global_size = ARRAY_SIZE(sensor_jn1_setfile_A_Global);
	sensor_jn1_dual_sync = sensor_jn1_dual_sync_setfile_A;
	sensor_jn1_dual_sync_size = ARRAY_SIZE(sensor_jn1_dual_sync_setfile_A);

	cis->sensor_info = &sensor_jn1_info_A;

	probe_info("%s done\n", __func__);
	return ret;
}

static const struct of_device_id sensor_cis_jn1_match[] = {
	{
		.compatible = "samsung,exynos-is-cis-jn1",
	},
	{},
};
MODULE_DEVICE_TABLE(of, sensor_cis_jn1_match);

static const struct i2c_device_id sensor_cis_jn1_idt[] = {
	{ SENSOR_NAME, 0 },
	{},
};

static struct i2c_driver sensor_cis_jn1_driver = {
	.probe	= cis_jn1_probe,
	.driver = {
		.name	= SENSOR_NAME,
		.owner	= THIS_MODULE,
		.of_match_table = sensor_cis_jn1_match,
		.suppress_bind_attrs = true,
	},
	.id_table = sensor_cis_jn1_idt
};

#ifdef MODULE
builtin_i2c_driver(sensor_cis_jn1_driver);
#else
static int __init sensor_cis_jn1_init(void)
{
	int ret;

	ret = i2c_add_driver(&sensor_cis_jn1_driver);
	if (ret)
		err("failed to add %s driver: %d\n",
			sensor_cis_jn1_driver.driver.name, ret);

	return ret;
}
late_initcall_sync(sensor_cis_jn1_init);
#endif

MODULE_LICENSE("GPL");
MODULE_SOFTDEP("pre: fimc-is");
