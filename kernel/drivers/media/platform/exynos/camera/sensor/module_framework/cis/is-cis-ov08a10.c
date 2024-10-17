// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series Sensor driver
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
#include "is-cis-ov08a10.h"
#include "is-vendor.h"
#include "is-cis-ov08a10-setA-19p2.h"
#include "is-helper-ixc.h"

#define SENSOR_NAME "OV08A10"

int sensor_ov08a10_cis_init(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_cis *cis = sensor_cis_get_cis(subdev);

	cis->cis_data->cur_width = cis->sensor_info->max_width;
	cis->cis_data->cur_height = cis->sensor_info->max_height;
	cis->cis_data->low_expo_start = 33000;

	cis->cis_data->sens_config_index_pre = SENSOR_OV08A10_MODE_MAX;
	cis->cis_data->sens_config_index_cur = 0;
	CALL_CISOPS(cis, cis_data_calculation, subdev, cis->cis_data->sens_config_index_cur);

	return ret;
}

static const struct is_cis_log log_ov08a10[] = {
	{I2C_READ, 16, 0x485E, 0, "frame_count"},
	{I2C_READ, 8, 0x0100, 0, "0x0100"},
	{I2C_READ, 16, 0x380E, 0, "FLL"},
	{I2C_READ, 16, 0x3501, 0, "CIT"},
	{I2C_READ, 16, 0x3508, 0, "A_GAIN"},
	{I2C_READ, 16, 0x350A, 0, "D_GAIN"},
	{I2C_READ, 8, 0x3208, 0, "gph"},
	{I2C_READ, 8, 0x3410, 0, "lte_mode"},
	{I2C_READ, 8, 0x3400, 0, "PSV"},
};

int sensor_ov08a10_cis_log_status(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_cis *cis = sensor_cis_get_cis(subdev);

	sensor_cis_log_status(cis, log_ov08a10, ARRAY_SIZE(log_ov08a10), (char *)__func__);

	return ret;
}

int sensor_ov08a10_cis_set_global_setting(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_cis *cis = sensor_cis_get_cis(subdev);
	struct sensor_ov08a10_private_data *priv = (struct sensor_ov08a10_private_data *)cis->sensor_info->priv;

	info("[%s] global setting start\n", __func__);

	ret = sensor_cis_write_registers_locked(subdev, priv->global);
	if (ret < 0) {
		err("global setting fail!!");
		return ret;
	}

	info("[%s] global setting done\n", __func__);
	return ret;
}

int sensor_ov08a10_cis_mode_change(struct v4l2_subdev *subdev, u32 mode)
{
	int ret = 0;
	const struct sensor_cis_mode_info *mode_info;
	struct is_cis *cis = sensor_cis_get_cis(subdev);

	if (mode >= cis->sensor_info->mode_count) {
		err("invalid mode(%d)!!", mode);
		return -EINVAL;
	}

	mode_info = cis->sensor_info->mode_infos[mode];

	ret = sensor_cis_write_registers_locked(subdev, mode_info->setfile);
	if (ret < 0)
		err("mode(%d) setting fail!!", mode);

	cis->cis_data->sens_config_index_pre = mode;

	info("[%s] mode changed(%d)\n", __func__, mode);

	return ret;
}

int sensor_ov08a10_cis_stream_on(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_device_sensor *device;
	struct is_cis *cis = sensor_cis_get_cis(subdev);

	device = (struct is_device_sensor *)v4l2_get_subdev_hostdata(subdev);
	WARN_ON(!device);

	info("%s\n", __func__);

	/* update mipi rate */
	is_vendor_set_mipi_clock(device);

	IXC_MUTEX_LOCK(cis->ixc_lock);
	cis->ixc_ops->write8(cis->client, 0x0100, 0x01);
	IXC_MUTEX_UNLOCK(cis->ixc_lock);

	cis->cis_data->stream_on = true;

	return ret;
}

int sensor_ov08a10_cis_stream_off(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_cis *cis = sensor_cis_get_cis(subdev);
	u16 cur_frame_count = 0;

	dbg_sensor(1, "[MOD:D:%d] %s\n", cis->id, __func__);

	cis->cis_data->stream_on = false;

	IXC_MUTEX_LOCK(cis->ixc_lock);
	cis->ixc_ops->read16(cis->client, 0x485E, &cur_frame_count);

	/* Group0 hold */
	cis->ixc_ops->write8(cis->client, 0x3208, 0x00);

	/* Sensor stream off */
	cis->ixc_ops->write8(cis->client, 0x0100, 0x00);

	/* Group0 hold relese */
	cis->ixc_ops->write8(cis->client, 0x3208, 0x10);

	/* Delay auto launch group0 */
	cis->ixc_ops->write8(cis->client, 0x3208, 0xa0);

	IXC_MUTEX_UNLOCK(cis->ixc_lock);

	info("%s done frame_count(%d)\n", __func__, cur_frame_count);

	return ret;
}

int sensor_ov08a10_cis_wait_streamon(struct v4l2_subdev *subdev)
{
	int ret = 0;
	int ret_err = 0;
	struct is_cis *cis = sensor_cis_get_cis(subdev);
	u32 wait_cnt = 0, time_out_cnt = 250;
	u16 sensor_fcount = 0;
	u32 i2c_fail_cnt = 0;

	if (cis->cis_data->cur_frame_us_time > 300000 && cis->cis_data->cur_frame_us_time < 2000000)
		time_out_cnt = (cis->cis_data->cur_frame_us_time / CIS_STREAM_ON_WAIT_TIME) + 100; // for Hyperlapse night mode

	IXC_MUTEX_LOCK(cis->ixc_lock);
	ret = cis->ixc_ops->read16(cis->client, 0x485E, &sensor_fcount);
	IXC_MUTEX_UNLOCK(cis->ixc_lock);
	if (ret < 0)
		err("i2c transfer fail addr(%x), val(%x), ret = %d\n", 0x485E, sensor_fcount, ret);

	/*
	 * Read sensor frame counter (sensor_fcount address = 0x485E)
	 * Stream on: 0x0001 ~ 0xFFFF
	 *stream off: 0x0000 (Change after entering sw standby state after stream off command)
	 */
	while (sensor_fcount == 0x0000) {
		usleep_range(CIS_STREAM_ON_WAIT_TIME, CIS_STREAM_ON_WAIT_TIME);
		wait_cnt++;

		IXC_MUTEX_LOCK(cis->ixc_lock);
		ret = cis->ixc_ops->read16(cis->client, 0x485E, &sensor_fcount);
		IXC_MUTEX_UNLOCK(cis->ixc_lock);
		if (ret < 0) {
			i2c_fail_cnt++;
			err("i2c transfer fail addr(%x), val(%x), try(%d), ret = %d\n",
				0x485E, sensor_fcount, i2c_fail_cnt, ret);
		}
#if defined(USE_RECOVER_I2C_TRANS)
		if (i2c_fail_cnt >= USE_RECOVER_I2C_TRANS) {
			sensor_cis_recover_i2c_fail(subdev);
			err("[mod:d:%d] %s, i2c transfer, forcely power down/up",
				cis->id, __func__);
			ret = -EINVAL;
			goto p_err;
		}
#endif
		if (wait_cnt >= time_out_cnt) {
			err("[MOD:D:%d] %s, stream on wait failed (%d), wait_cnt(%d) > time_out_cnt(%d)",
				cis->id, __func__, cis->cis_data->cur_frame_us_time, wait_cnt, time_out_cnt);
			ret = -EINVAL;
			goto p_err;
		}

		dbg_sensor(1, "[MOD:D:%d] %s, sensor_fcount(%d), (wait_limit(%d) < time_out(%d))\n",
				cis->id, __func__, sensor_fcount, wait_cnt, time_out_cnt);
	}

	return 0;

p_err:
	CALL_CISOPS(cis, cis_log_status, subdev);

	ret_err = CALL_CISOPS(cis, cis_stream_off, subdev);
	if (ret_err < 0) {
		err("[MOD:D:%d] stream off fail", cis->id);
	} else {
		ret_err = CALL_CISOPS(cis, cis_wait_streamoff, subdev);
		if (ret_err < 0)
			err("[MOD:D:%d] sensor wait stream off fail", cis->id);
	}

	return ret;
}

int sensor_ov08a10_cis_wait_streamoff(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_cis *cis = sensor_cis_get_cis(subdev);

	u32 wait_cnt = 0, time_out_cnt = 250;
	u16 sensor_fcount = 0;
	u32 i2c_fail_cnt = 0, i2c_fail_max_cnt = 5;

	IXC_MUTEX_LOCK(cis->ixc_lock);
	ret = cis->ixc_ops->read16(cis->client, 0x485E, &sensor_fcount);
	IXC_MUTEX_UNLOCK(cis->ixc_lock);

	if (ret < 0)
		err("i2c transfer fail addr(%x), val(%x), ret = %d\n", 0x485E, sensor_fcount, ret);

	/*
	 * Read sensor frame counter (sensor_fcount address = 0x485E)
	 * Stream on: 0x0001 ~ 0xFFFF
	 * stream off: 0x0000 (Change after entering sw standby state after stream off command)
	 */
	while (sensor_fcount != 0x0000) {
		IXC_MUTEX_LOCK(cis->ixc_lock);
		ret = cis->ixc_ops->read16(cis->client, 0x485E, &sensor_fcount);
		IXC_MUTEX_UNLOCK(cis->ixc_lock);
		if (ret < 0) {
			i2c_fail_cnt++;
			err("i2c transfer fail addr(%x), val(%x), try(%d), ret = %d\n",
				0x4D5E, sensor_fcount, i2c_fail_cnt, ret);

			if (i2c_fail_cnt >= i2c_fail_max_cnt) {
				err("[MOD:D:%d] %s, i2c fail, i2c_fail_cnt(%d) >= i2c_fail_max_cnt(%d), sensor_fcount(%d)",
						cis->id, __func__, i2c_fail_cnt, i2c_fail_max_cnt, sensor_fcount);
				ret = -EINVAL;
				goto p_err;
			}
		}
#if defined(USE_RECOVER_I2C_TRANS)
		if (i2c_fail_cnt >= USE_RECOVER_I2C_TRANS) {
			sensor_cis_recover_i2c_fail(subdev);
			err("[mod:d:%d] %s, i2c transfer, forcely power down/up",
				cis->id, __func__);
			ret = -EINVAL;
			goto p_err;
		}
#endif
		usleep_range(CIS_STREAM_OFF_WAIT_TIME, CIS_STREAM_OFF_WAIT_TIME + 10);
		wait_cnt++;

		if (wait_cnt >= time_out_cnt) {
			err("[MOD:D:%d] %s, time out, wait_limit(%d) > time_out(%d), sensor_fcount(%d)",
					cis->id, __func__, wait_cnt, time_out_cnt, sensor_fcount);
			ret = -EINVAL;
			goto p_err;
		}

		dbg_sensor(1, "[MOD:D:%d] %s, sensor_fcount(%d), (wait_limit(%d) < time_out(%d))\n",
				cis->id, __func__, sensor_fcount, wait_cnt, time_out_cnt);
	}

p_err:
	return ret;
}

u32 sensor_ov08a10_cis_calc_again_permile(u32 code)
{
	u32 ret = 0;
	int i;

	for (i = 0; i <= MAX_GAIN_INDEX; i++) {
		if (sensor_ov08a10_analog_gain[i][0] == code) {
			ret = sensor_ov08a10_analog_gain[i][1];
			break;
		}
	}

	return ret;
}

u32 sensor_ov08a10_cis_calc_again_closest(u32 permile)
{
	int i;

	if (permile <= sensor_ov08a10_analog_gain[CODE_GAIN_INDEX][PERMILE_GAIN_INDEX])
		return sensor_ov08a10_analog_gain[CODE_GAIN_INDEX][PERMILE_GAIN_INDEX];

	if (permile >= sensor_ov08a10_analog_gain[MAX_GAIN_INDEX][PERMILE_GAIN_INDEX])
		return sensor_ov08a10_analog_gain[MAX_GAIN_INDEX][PERMILE_GAIN_INDEX];

	for (i = 0; i <= MAX_GAIN_INDEX; i++) {
		if (sensor_ov08a10_analog_gain[i][PERMILE_GAIN_INDEX] == permile)
			return sensor_ov08a10_analog_gain[i][PERMILE_GAIN_INDEX];

		if ((int)(permile - sensor_ov08a10_analog_gain[i][PERMILE_GAIN_INDEX]) < 0)
			return sensor_ov08a10_analog_gain[i-1][PERMILE_GAIN_INDEX];
	}

	return sensor_ov08a10_analog_gain[MAX_GAIN_INDEX][PERMILE_GAIN_INDEX];
}

u32 sensor_ov08a10_cis_calc_again_code(u32 permile)
{
	u32 ret = 0, nearest_val = 0;
	int i;

	nearest_val = sensor_ov08a10_cis_calc_again_closest(permile);

	dbg_sensor(1, "[%s] permile %d nearest_val %d\n", __func__, permile, nearest_val);

	for (i = 0; i <= MAX_GAIN_INDEX; i++) {
		if (sensor_ov08a10_analog_gain[i][1] == nearest_val) {
			ret = sensor_ov08a10_analog_gain[i][0];
			break;
		}
	}

	return ret;
}

int sensor_ov08a10_cis_get_digital_gain(struct v4l2_subdev *subdev, u32 *dgain)
{
	int ret = 0;
	struct is_cis *cis = sensor_cis_get_cis(subdev);
	u16 digital_gain = 0;
	u8 dgain_lsb, dgain_hsb;

	WARN_ON(!dgain);

	IXC_MUTEX_LOCK(cis->ixc_lock);

	ret = cis->ixc_ops->read8(cis->client, cis->reg_addr->dgain, &dgain_hsb);
	ret |= cis->ixc_ops->read8(cis->client, cis->reg_addr->dgain + 1, &dgain_lsb);
	if (ret < 0)
		goto p_err_i2c_unlock;

	digital_gain = (dgain_hsb << 8) + dgain_lsb;
	*dgain = CALL_CISOPS(cis, cis_calc_dgain_permile, digital_gain);

	dbg_sensor(1, "%s [%d][%s] cur_dgain = x%d.%03d, digital_gain(%#x)\n",
			__func__, cis->id, cis->sensor_info->name, (*dgain / 1000), (*dgain % 1000), digital_gain);

p_err_i2c_unlock:
	IXC_MUTEX_UNLOCK(cis->ixc_lock);

	return ret;
}

int sensor_ov08a10_cis_set_digital_gain(struct v4l2_subdev *subdev, struct ae_param *dgain)
{
	int ret = 0;
	struct is_cis *cis = sensor_cis_get_cis(subdev);
	u16 dgain_code = 0;

	WARN_ON(!dgain);

	cis->cis_data->last_dgain = *dgain;

	dgain_code = (u16)CALL_CISOPS(cis, cis_calc_dgain_code, dgain->val);

	if (dgain_code < cis->cis_data->min_digital_gain[0]) {
		dgain_code = cis->cis_data->min_digital_gain[0];
	}
	if (dgain_code > cis->cis_data->max_digital_gain[0]) {
		dgain_code = cis->cis_data->max_digital_gain[0];
	}

	IXC_MUTEX_LOCK(cis->ixc_lock);

	ret = cis->ixc_ops->write8(cis->client, cis->reg_addr->dgain, (dgain_code >> 8));
	ret = cis->ixc_ops->write8(cis->client, cis->reg_addr->dgain + 1, dgain_code);

	IXC_MUTEX_UNLOCK(cis->ixc_lock);

	dbg_sensor(1, "[MOD:D:%d] %s(vsync cnt = %d), input_dgain = %d, dgain_code(%#x)\n",
		cis->id, __func__, cis->cis_data->sen_vsync_count, dgain->val, dgain_code);

	return ret;
}

int sensor_ov08a10_cis_set_test_pattern(struct v4l2_subdev *subdev, camera2_sensor_ctl_t *sensor_ctl)
{
	int ret = 0;
	struct is_cis *cis = sensor_cis_get_cis(subdev);

	dbg_sensor(1, "[MOD:D:%d] %s, cur_pattern_mode(%d), testPatternMode(%d)\n", cis->id, __func__,
			cis->cis_data->cur_pattern_mode, sensor_ctl->testPatternMode);

	if (cis->cis_data->cur_pattern_mode != sensor_ctl->testPatternMode) {
		if (sensor_ctl->testPatternMode == SENSOR_TEST_PATTERN_MODE_OFF) {
			info("%s: set DEFAULT pattern! (testpatternmode : %d)\n", __func__, sensor_ctl->testPatternMode);

			IXC_MUTEX_LOCK(cis->ixc_lock);
			cis->ixc_ops->write8(cis->client, 0x5081, 0x00);
			cis->ixc_ops->write8(cis->client, 0x5082, 0x00);
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
			cis->ixc_ops->write8(cis->client, 0x5081, 0x01);
			cis->ixc_ops->write8(cis->client, 0x5082, 0x01);
			IXC_MUTEX_UNLOCK(cis->ixc_lock);

			cis->cis_data->cur_pattern_mode = sensor_ctl->testPatternMode;
		}
	}

	return ret;
}

bool sensor_ov08a10_cis_check_lte(struct v4l2_subdev *subdev, u32 input_duration)
{
	struct is_cis *cis = sensor_cis_get_cis(subdev);
	cis_shared_data *cis_data = cis->cis_data;
	u64 max_duration_16bit = 0;

	if (cis->cis_data->stream_on == false)
		return false;

	if (cis_data->is_data.scene_mode != AA_SCENE_MODE_PRO_MODE)
		return false;

	max_duration_16bit = (u64)0xFFE3 * 1000000 * cis_data->line_length_pck / cis_data->pclk;
	if (input_duration > max_duration_16bit) {
		dbg_sensor(1, "[%s][%d] enable LTE : input_duration(%d), max_duration(%llu)\n",
			__func__, cis_data->sen_vsync_count, input_duration, max_duration_16bit);
		return true;
	}

	return false;
}

int sensor_ov08a10_cis_lte_on(struct v4l2_subdev *subdev, u32 input_duration)
{
	int ret = 0;
	struct is_cis *cis = sensor_cis_get_cis(subdev);
	cis_shared_data *cis_data = cis->cis_data;
	u64 pclk_khz;
	u64 lte_exposure, pre_exposure;
	u16 cit_long, fll;
	u16 hold_time_msb, hold_time_lsb;
	u16 max_cit_lte = 0x13b2;

	if (cis->cis_data->stream_on == false) {
		info("[%s][%d] already stream off input_duration(%d)\n", 
			__func__, cis->cis_data->sen_vsync_count, input_duration);
		return ret;
	}

	fll = cis_data->pre_frame_length_lines;
	cit_long = cis_data->cur_long_exposure_coarse;

	if (cit_long < max_cit_lte) {
		info("[%s][%d] wrong cit_long, reset (0x%x => 0x%x)\n",
			__func__, cis_data->sen_vsync_count, cit_long, max_cit_lte);
		cit_long = max_cit_lte;
		cis_data->cur_long_exposure_coarse = cit_long;
	}

	pclk_khz = cis_data->pclk / (1000);
	pre_exposure = (u64)((cit_long * cis_data->line_readOut_time / cis->sensor_info->readout_factor) / 1000);	
	lte_exposure = (u64)((input_duration - pre_exposure) * pclk_khz / 1000) / 256;
	hold_time_msb = (lte_exposure >> 16) & 0xFFFF;
	hold_time_lsb = lte_exposure & 0xFFFF;

	info("[%s][%d] [%dms] => FLL(0x%x) CIT(0x%x) lte_exposure(%llu : 0x%x , 0x%x)\n",
		__func__, cis_data->sen_vsync_count, input_duration / (1000), fll, cit_long, 
		lte_exposure, hold_time_msb, hold_time_lsb);

	IXC_MUTEX_LOCK(cis->ixc_lock);

	/* group 0 hold start */
	ret = cis->ixc_ops->write8(cis->client, cis->reg_addr->group_param_hold, 0x00);
	/* PSV off */
	ret |= cis->ixc_ops->write8(cis->client, 0x3400, 0x08);
	ret |= cis->ixc_ops->write16(cis->client, cis->reg_addr->fll, fll);
	ret |= cis->ixc_ops->write16(cis->client, cis->reg_addr->cit, cit_long);
	ret |= cis->ixc_ops->write16(cis->client, 0x3412, hold_time_msb);
	ret |= cis->ixc_ops->write16(cis->client, 0x3414, hold_time_lsb);
	/* lte mode on */
	ret |= cis->ixc_ops->write8(cis->client, 0x3410, 0x01);
	/* PSV on */
	ret |= cis->ixc_ops->write8(cis->client, 0x3400, 0x00);
	/* group 0 hold end */
	ret = cis->ixc_ops->write8(cis->client, cis->reg_addr->group_param_hold, 0x10);
	/* Delay auto launch group 0 */
	ret |= cis->ixc_ops->write8(cis->client, cis->reg_addr->group_param_hold, 0xa0);

	IXC_MUTEX_UNLOCK(cis->ixc_lock);

	return ret; 
}

int sensor_ov08a10_cis_lte_off(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_cis *cis = sensor_cis_get_cis(subdev);
	u8 lte_mode = 0;

	if (cis->cis_data->stream_on == false) {
		dbg_sensor(1, "[%s][%d] already stream off \n", __func__, cis->cis_data->sen_vsync_count);
		return ret; 
	}

	ret = cis->ixc_ops->read8(cis->client, 0x3410, &lte_mode);

	if (lte_mode) {
		dbg_sensor(1, "[%s][%d] lte_mode(%d) disable LTE \n", __func__, cis->cis_data->sen_vsync_count, lte_mode);
		/* group 0 hold start */
		ret |= cis->ixc_ops->write8(cis->client, cis->reg_addr->group_param_hold, 0x00);
		ret |= cis->ixc_ops->write8(cis->client, 0x3410, 0x00);
		/* group 0 hold end */
		ret |= cis->ixc_ops->write8(cis->client, cis->reg_addr->group_param_hold, 0x10);
		/* Delay auto launch group 0 */
		ret |= cis->ixc_ops->write8(cis->client, cis->reg_addr->group_param_hold, 0xa0);
	}

	return ret; 
}

int sensor_ov08a10_cis_set_frame_duration(struct v4l2_subdev *subdev, u32 frame_duration)
{
	int ret = 0;
	struct is_cis *cis = sensor_cis_get_cis(subdev);
	cis_shared_data *cis_data = cis->cis_data;

	if (frame_duration < cis_data->min_frame_us_time) {
		dbg_sensor(1, "frame duration is less than min(%d)\n", frame_duration);
		frame_duration = cis_data->min_frame_us_time;
	}

	if (sensor_ov08a10_cis_check_lte(subdev, frame_duration) == true) {
		cis_data->cur_frame_us_time = frame_duration;
		cis_data->last_frame_duration = frame_duration;
	} else {	
		ret = sensor_ov08a10_cis_lte_off(subdev);
		ret |= sensor_cis_set_frame_duration(subdev, frame_duration);
	}

	return ret; 
}

int sensor_ov08a10_cis_set_exposure_time(struct v4l2_subdev *subdev, struct ae_param *target_exposure)
{
	int ret = 0;
	struct is_cis *cis = sensor_cis_get_cis(subdev);
	cis_shared_data *cis_data = cis->cis_data;
	struct ae_param exp;
	u32 input_duration;

	if ((target_exposure->long_val <= 0) || (target_exposure->short_val <= 0)) {
		err("invalid target exposure(%d, %d)", target_exposure->long_val, target_exposure->short_val);
		ret = -EINVAL;
		return ret; 
	}

	cis_data->last_exp = *target_exposure;
	exp = *target_exposure;
	input_duration = MAX(exp.long_val, exp.short_val);

	if (sensor_ov08a10_cis_check_lte(subdev, input_duration) == true) {
		ret = sensor_ov08a10_cis_lte_on(subdev, input_duration);
	} else {
		ret = sensor_ov08a10_cis_lte_off(subdev);
		ret |= sensor_cis_set_exposure_time(subdev, &exp);
	}

	return ret; 
}

int sensor_ov08a10_cis_init_state(struct v4l2_subdev *subdev, int mode)
{
	struct is_cis *cis = sensor_cis_get_cis(subdev);

	cis->cis_data->stream_on = false;
	cis->mipi_clock_index_cur = CAM_MIPI_NOT_INITIALIZED;
	cis->mipi_clock_index_new = CAM_MIPI_NOT_INITIALIZED;
	cis->cis_data->cur_pattern_mode = SENSOR_TEST_PATTERN_MODE_OFF;

	return 0;
}

static struct is_cis_ops cis_ops = {
	.cis_init = sensor_ov08a10_cis_init,
	.cis_init_state = sensor_ov08a10_cis_init_state,
	.cis_log_status = sensor_ov08a10_cis_log_status,
	.cis_group_param_hold = NULL,
	.cis_set_global_setting = sensor_ov08a10_cis_set_global_setting,
	.cis_mode_change = sensor_ov08a10_cis_mode_change,
	.cis_stream_on = sensor_ov08a10_cis_stream_on,
	.cis_stream_off = sensor_ov08a10_cis_stream_off,
	.cis_wait_streamon = sensor_ov08a10_cis_wait_streamon,	
	.cis_wait_streamoff = sensor_ov08a10_cis_wait_streamoff,
	.cis_data_calculation = sensor_cis_data_calculation,
	.cis_set_exposure_time = sensor_ov08a10_cis_set_exposure_time,
	.cis_get_min_exposure_time = sensor_cis_get_min_exposure_time,
	.cis_get_max_exposure_time = sensor_cis_get_max_exposure_time,
	.cis_adjust_frame_duration = sensor_cis_adjust_frame_duration,
	.cis_set_frame_duration = sensor_ov08a10_cis_set_frame_duration,
	.cis_set_frame_rate = sensor_cis_set_frame_rate,
	.cis_adjust_analog_gain = sensor_cis_adjust_analog_gain,
	.cis_set_analog_gain = sensor_cis_set_analog_gain,
	.cis_get_analog_gain = sensor_cis_get_analog_gain,
	.cis_get_min_analog_gain = sensor_cis_get_min_analog_gain,
	.cis_get_max_analog_gain = sensor_cis_get_max_analog_gain,
	.cis_calc_again_code = sensor_ov08a10_cis_calc_again_code,
	.cis_calc_again_permile = sensor_ov08a10_cis_calc_again_permile,
	.cis_set_digital_gain = sensor_ov08a10_cis_set_digital_gain,
	.cis_get_digital_gain = sensor_ov08a10_cis_get_digital_gain,
	.cis_get_min_digital_gain = sensor_cis_get_min_digital_gain,
	.cis_get_max_digital_gain = sensor_cis_get_max_digital_gain,
	.cis_calc_dgain_code = sensor_cis_calc_dgain_code,
	.cis_calc_dgain_permile = sensor_cis_calc_dgain_permile,
	.cis_compensate_gain_for_extremely_br = sensor_cis_compensate_gain_for_extremely_br,
	.cis_check_rev_on_init = sensor_cis_check_rev_on_init,
	.cis_set_initial_exposure = sensor_cis_set_initial_exposure,
//	.cis_set_flip_mode = sensor_cis_set_flip_mode,
	.cis_set_test_pattern = sensor_ov08a10_cis_set_test_pattern,
};

static int cis_ov08a10_probe_i2c(struct i2c_client *client,
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
	cis->bayer_order = OTF_INPUT_ORDER_BAYER_BG_GR;
	cis->reg_addr = &sensor_ov08a10_reg_addr;

	if (of_property_read_string(dnode, "setfile", &setfile)) {
		err("setfile index read fail(%d), take default setfile!!", ret);
		setfile = "default";
	}

	mclk_freq_khz = sensor_peri->module->pdata->mclk_freq_khz;

	if (mclk_freq_khz == 19200) {
		if (strcmp(setfile, "default") == 0 || strcmp(setfile, "setA") == 0) {
			probe_info("%s setfile_A mclk: 19.2MHz\n", __func__);
			cis->sensor_info = &sensor_ov08a10_info_A_19p2;
		}
	}

	is_vendor_set_mipi_mode(cis);

	probe_info("%s done\n", __func__);

	return ret;
}

static const struct of_device_id sensor_cis_ov08a10_match[] = {
	{
		.compatible = "samsung,exynos-is-cis-ov08a10",
	},
	{},
};
MODULE_DEVICE_TABLE(of, sensor_cis_ov08a10_match);

static const struct i2c_device_id sensor_cis_ov08a10_idt[] = {
	{ SENSOR_NAME, 0 },
	{},
};

static struct i2c_driver sensor_cis_ov08a10_driver = {
	.probe	= cis_ov08a10_probe_i2c,
	.driver = {
		.name	= SENSOR_NAME,
		.owner	= THIS_MODULE,
		.of_match_table = sensor_cis_ov08a10_match,
		.suppress_bind_attrs = true,
	},
	.id_table = sensor_cis_ov08a10_idt
};

#ifdef MODULE
builtin_i2c_driver(sensor_cis_ov08a10_driver);
#else
static int __init sensor_cis_ov08a10_init(void)
{
	int ret;

	ret = i2c_add_driver(&sensor_cis_ov08a10_driver);
	if (ret)
		err("failed to add %s driver: %d\n",
			sensor_cis_ov08a10_driver.driver.name, ret);

	return ret;
}
late_initcall_sync(sensor_cis_ov08a10_init);
#endif

MODULE_LICENSE("GPL");
MODULE_SOFTDEP("pre: fimc-is");
