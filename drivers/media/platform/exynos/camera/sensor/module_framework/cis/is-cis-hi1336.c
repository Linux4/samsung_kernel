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
#include <linux/videodev2_exynos_camera.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/platform_device.h>
#include <linux/of_gpio.h>
#include <linux/syscalls.h>
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
#include "is-cis-hi1336.h"

#if defined(USE_HI1336B_SETFILE)
#include "is-cis-hi1336B-setA.h"
#else
/* Add setfiles in the case below.
 * USE_HI1336_12M_FULL_SETFILE "is-cis-hi1336-12M-full-setA.h"
 * USE_HI1336C_SETFILE is-cis-hi1336C-setA.h
 * Default is-cis-hi1336-setA.h
 */
#include "is-cis-hi1336B-setA.h"
#endif

#include "is-helper-ixc.h"
#ifdef CONFIG_CAMERA_VENDER_MCD_V2
#include "is-sec-define.h"
#endif

#include "interface/is-interface-library.h"

#define SENSOR_NAME "HI1336"

#define POLL_TIME_MS 5
#define STREAM_MAX_POLL_CNT 60

static const u32 *sensor_hi1336_global;
static u32 sensor_hi1336_global_size;
static struct setfile_info *sensor_hi1336_setfile_fsync_info;

/*************************************************
 *  [HI1336 Analog gain formular]
 *
 *  Analog Gain = (Reg value)/16 + 1
 *
 *  Analog Gain Range = x1.0 to x16.0
 *
 *************************************************/

u32 sensor_hi1336_cis_calc_again_code(u32 permile)
{
	return ((permile - 1000) * 16 / 1000);
}

u32 sensor_hi1336_cis_calc_again_permile(u32 code)
{
	return ((code * 1000 / 16 + 1000));
}

/*************************************************
 *  [HI1336 Digital gain formular]
 *
 *  Digital Gain = bit[12:9] + bit[8:0]/512 (Gr, Gb, R, B)
 *
 *  Digital Gain Range = x1.0 to x15.99
 *
 *************************************************/

u32 sensor_hi1336_cis_calc_dgain_code(u32 permile)
{
	u32 buf[2] = {0, 0};
	buf[0] = ((permile / 1000) & 0x0F) << 9;
	buf[1] = ((((permile % 1000) * 512) / 1000) & 0x1FF);

	return (buf[0] | buf[1]);
}

u32 sensor_hi1336_cis_calc_dgain_permile(u32 code)
{
	return (((code & 0x1E00) >> 9) * 1000) + ((code & 0x01FF) * 1000 / 512);
}

/* CIS OPS */
int sensor_hi1336_cis_init(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_cis *cis = sensor_cis_get_cis(subdev);
	ktime_t st = ktime_get();

	cis->cis_data->stream_on = false;
	cis->cis_data->cur_width = cis->sensor_info->max_width;
	cis->cis_data->cur_height = cis->sensor_info->max_height;
	cis->cis_data->low_expo_start = 33000;
	cis->need_mode_change = false;
	cis->long_term_mode.sen_strm_off_on_step = 0;

	cis->cis_data->sens_config_index_pre = SENSOR_HI1336_MODE_MAX;
	cis->cis_data->sens_config_index_cur = 0;

	CALL_CISOPS(cis, cis_data_calculation, subdev, cis->cis_data->sens_config_index_cur);

	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %ldus\n", __func__, PABLO_KTIME_US_DELTA_NOW(st));

	return ret;
}

static const struct is_cis_log log_hi1336[] = {
	{I2C_READ, 16, 0x0716, 0, "model_id"},
	{I2C_READ, 16, 0x0808, 0, "mode_select"},
	{I2C_READ, 16, 0x020E, 0, "fll"},
	{I2C_READ, 16, 0x0206, 0, "llp"},
};

int sensor_hi1336_cis_log_status(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_cis *cis = sensor_cis_get_cis(subdev);

	sensor_cis_log_status(cis, cis->client, log_hi1336, ARRAY_SIZE(log_hi1336), (char *)__func__);

	return ret;
}

static int sensor_hi1336_cis_group_param_hold_func(struct v4l2_subdev *subdev, bool hold)
{
#if USE_GROUP_PARAM_HOLD
	int ret = 0;
	struct is_cis *cis = NULL;
	struct i2c_client *client = NULL;
	u16 hold_set = 0;

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

	if (hold == (bool)cis->cis_data->group_param_hold) {
		pr_debug("already group_param_hold (%d)\n", cis->cis_data->group_param_hold);
		goto p_err;
	}

	ret = cis->ixc_ops->read16(client, SENSOR_HI1336_GROUP_PARAM_HOLD_ADDR, &hold_set);
	if (ret < 0) {
		err("[%s: is_sensor_read] fail!!", __func__);
		goto p_err;
	}

	// set hold setting, bit[8]: Grouped parameter hold
	if (hold == true) {
		hold_set |= (0x1) << 8;
	} else {
		hold_set &= ~((0x1) << 8);
	}
	dbg_sensor(1, "[%s] GPH: %s \n", __func__, (hold_set & (0x1 << 8)) == (0x1 << 8) ? "ON": "OFF");

	ret = cis->ixc_ops->write16(client, SENSOR_HI1336_GROUP_PARAM_HOLD_ADDR, hold_set);
	if (ret < 0) {
		err("[%s: is_sensor_write] fail!!", __func__);
		goto p_err;
	}

	cis->cis_data->group_param_hold = (u32)hold;
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
int sensor_hi1336_cis_group_param_hold(struct v4l2_subdev *subdev, bool hold)
{
	int ret = 0;
	struct is_cis *cis = NULL;
	u32 mode = 0;

	FIMC_BUG(!subdev);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);

	FIMC_BUG(!cis);
	FIMC_BUG(!cis->cis_data);

	if (cis->cis_data->stream_on == false && hold == true) {
		ret = 0;
		dbg_sensor(1,"%s : sensor stream off skip group_param_hold", __func__);
		goto p_err;
	}

	mode = cis->cis_data->sens_config_index_cur;

/* TODO :  add the fasten_ae mode
	if (mode == SENSOR_HI1336_1024x768_120FPS) {
		ret = 0;
		dbg_sensor(1,"%s : fast ae skip group_param_hold", __func__);
		goto p_err;
	}
*/
	I2C_MUTEX_LOCK(cis->i2c_lock);
	ret = sensor_hi1336_cis_group_param_hold_func(subdev, hold);
	if (ret < 0)
		goto p_err_unlock;

p_err_unlock:
	I2C_MUTEX_UNLOCK(cis->i2c_lock);

p_err:
	return ret;
}
#endif

int sensor_hi1336_cis_set_global_setting(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_cis *cis = sensor_cis_get_cis(subdev);

	dbg_sensor(1, "[%s] start\n", __func__);

	I2C_MUTEX_LOCK(cis->i2c_lock);

	ret = sensor_cis_set_registers(subdev, sensor_hi1336_global, sensor_hi1336_global_size);
	if (ret < 0) {
		err("sensor_hi1336_global fail!!");
		goto p_err_unlock;
	}

	dbg_sensor(1, "[%s] done\n", __func__);

p_err_unlock:
	I2C_MUTEX_UNLOCK(cis->i2c_lock);

	return ret;
}

int sensor_hi1336_cis_mode_change(struct v4l2_subdev *subdev, u32 mode)
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

	I2C_MUTEX_LOCK(cis->i2c_lock);

	info("[%s] sensor mode(%d)\n", __func__, mode);
	ret = sensor_cis_set_registers(subdev, mode_info->setfile, mode_info->setfile_size);
	if (ret < 0) {
		err("sensor_setfiles fail!!");
		goto p_err_unlock;
	}

	// Can change position later
	ret = sensor_cis_set_registers(subdev, sensor_hi1336_setfile_fsync_info[HI1336_FSYNC_NORMAL].file,
			sensor_hi1336_setfile_fsync_info[HI1336_FSYNC_NORMAL].file_size);
	if (ret < 0) {
		err("sensor_hi1336_setfile_fsync_info(normal mode) fail");
		goto p_err_unlock;
	}

#ifdef APPLY_MIRROR_VERTICAL_FLIP
	/* Apply Mirror and Vertical Flip */
	ret = cis->ixc_ops->write16(cis->client, 0x0202, 0x0200);
	if (ret < 0) {
		err("[%s] mirror vertical flip write fail\n", __func__);
		goto p_err_unlock;
	}
#endif
	dbg_sensor(1, "[%s] mode changed(%d)\n", __func__, mode);

p_err_unlock:
	I2C_MUTEX_UNLOCK(cis->i2c_lock);

p_err:
	return ret;
}

#if 0
int sensor_hi1336_cis_enable_frame_cnt(struct i2c_client *client, bool enable)
{
	int ret = -1;
	u8 frame_cnt_ctrl = 0;

	if (unlikely(!client)) {
		err("client is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	ret = cis->ixc_ops->read8(client, SENSOR_HI1336_MIPI_TX_OP_MODE_ADDR,
			&frame_cnt_ctrl);
	if (ret < 0) {
		err("i2c transfer fail addr(%x) ret = %d\n",
				SENSOR_HI1336_MIPI_TX_OP_MODE_ADDR, ret);
		goto p_err;
	}

	if (enable == true) {
		frame_cnt_ctrl |= (0x1 << 2);  //enable frame count
		frame_cnt_ctrl &= ~(0x1 << 0); //frame count reset off
	} else {
		frame_cnt_ctrl &= ~(0x1 << 2); //disable frame count
		frame_cnt_ctrl |= (0x1 << 0);  //frame count reset on
	}

	ret = cis->ixc_ops->write8(client, SENSOR_HI1336_MIPI_TX_OP_MODE_ADDR,
			frame_cnt_ctrl);
	if (ret < 0) {
		err("i2c transfer fail addr(%x) ret = %d\n",
				SENSOR_HI1336_MIPI_TX_OP_MODE_ADDR, ret);
	}

p_err:
	return ret;
}
#endif

int sensor_hi1336_cis_stream_on(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_cis *cis = sensor_cis_get_cis(subdev);
	cis_shared_data *cis_data = NULL;
	ktime_t st = ktime_get();

	cis_data = cis->cis_data;

	dbg_sensor(1, "[MOD:D:%d] %s\n", cis->id, __func__);

	I2C_MUTEX_LOCK(cis->i2c_lock);

	/* Sensor stream on */
	ret = cis->ixc_ops->write16(cis->client, SENSOR_HI1336_STREAM_ONOFF_ADDR, 0x0001);
	I2C_MUTEX_UNLOCK(cis->i2c_lock);
	if (ret < 0) {
		err("i2c transfer fail addr(%x) ret = %d\n",
				SENSOR_HI1336_STREAM_ONOFF_ADDR, ret);
		goto p_err;
	}

	cis_data->stream_on = true;
	info("%s done\n", __func__);

	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %ldus\n", __func__, PABLO_KTIME_US_DELTA_NOW(st));

p_err:
	return ret;
}

int sensor_hi1336_cis_stream_off(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_cis *cis = sensor_cis_get_cis(subdev);
	cis_shared_data *cis_data = NULL;
	ktime_t st = ktime_get();

	cis_data = cis->cis_data;
	dbg_sensor(1, "[MOD:D:%d] %s\n", cis->id, __func__);

	cis_data->stream_on = false;

	I2C_MUTEX_LOCK(cis->i2c_lock);

	ret = sensor_hi1336_cis_group_param_hold_func(subdev, false);
	if (ret < 0)
		err("group_param_hold_func failed at stream off");

	ret = cis->ixc_ops->write16(cis->client, SENSOR_HI1336_STREAM_ONOFF_ADDR, 0x0000);
	I2C_MUTEX_UNLOCK(cis->i2c_lock);
	if (ret < 0) {
		err("i2c transfer fail addr(%x) ret = %d\n",
				SENSOR_HI1336_STREAM_ONOFF_ADDR, ret);
	}

	info("%s done\n", __func__);

	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %ldus\n", __func__, PABLO_KTIME_US_DELTA_NOW(st));

	return ret;
}

int sensor_hi1336_cis_wait_streamoff(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_cis *cis = sensor_cis_get_cis(subdev);
	u32 wait_cnt = 0;
	u16 PLL_en = 0;

	do {
		ret = cis->ixc_ops->read16(cis->client, SENSOR_HI1336_ISP_PLL_ENABLE_ADDR, &PLL_en);
		if (ret < 0) {
			err("i2c transfer fail addr(%x) ret = %d\n",
					SENSOR_HI1336_ISP_PLL_ENABLE_ADDR, ret);
			goto p_err;
		}

		dbg_sensor(1, "%s: PLL_en 0x%x \n", __func__, PLL_en);
		/* pll_bypass */
		if (!(PLL_en & 0x0100))
			break;

		wait_cnt++;
		msleep(POLL_TIME_MS);
	} while (wait_cnt < STREAM_MAX_POLL_CNT);

	if (wait_cnt < STREAM_MAX_POLL_CNT) {
		info("%s: finished after %d ms\n", __func__, (wait_cnt + 1) * POLL_TIME_MS);
	} else {
		warn("%s: finished : polling timeout occured after %d ms\n", __func__,
				(wait_cnt + 1) * POLL_TIME_MS);
	}

p_err:
	return ret;
}

int sensor_hi1336_cis_wait_streamon(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_cis *cis = sensor_cis_get_cis(subdev);
	u32 wait_cnt = 0;
	u16 PLL_en = 0;

	do {
		ret = cis->ixc_ops->read16(cis->client, SENSOR_HI1336_ISP_PLL_ENABLE_ADDR, &PLL_en);
		if (ret < 0) {
			err("i2c transfer fail addr(%x) ret = %d\n",
					SENSOR_HI1336_ISP_PLL_ENABLE_ADDR, ret);
			goto p_err;
		}

		dbg_sensor(1, "%s: PLL_en 0x%x\n", __func__, PLL_en);
		/* pll_enable */
		if (PLL_en & 0x0100)
			break;

		wait_cnt++;
		msleep(POLL_TIME_MS);
	} while (wait_cnt < STREAM_MAX_POLL_CNT);

	if (wait_cnt < STREAM_MAX_POLL_CNT) {
		info("%s: finished after %d ms\n", __func__, (wait_cnt + 1) * POLL_TIME_MS);
	} else {
		warn("%s: finished : polling timeout occured after %d ms\n", __func__,
				(wait_cnt + 1) * POLL_TIME_MS);
	}

p_err:
	return ret;
}

int sensor_hi1336_cis_set_analog_digital_gain(struct v4l2_subdev *subdev, struct ae_param *again, struct ae_param *dgain)
{
	int ret = 0;
	struct is_cis *cis = sensor_cis_get_cis(subdev);
	cis_shared_data *cis_data = NULL;

	u32 cal_analog_val1 = 0;
	u32 cal_analog_val2 = 0;
	u32 cal_digital = 0;
	u8 analog_gain = 0;

	u16 dgain_code = 0;
	u16 dgains[4] = {0};
	u16 read_val = 0;
	u16 enable_dgain = 0;
	ktime_t st = ktime_get();

	cis_data = cis->cis_data;

	analog_gain = (u8)sensor_hi1336_cis_calc_again_code(again->val);

	if (analog_gain < cis_data->min_analog_gain[0]) {
		info("[%s] not proper analog_gain value, reset to min_analog_gain\n", __func__);
		analog_gain = cis_data->min_analog_gain[0];
	}

	if (analog_gain > cis_data->max_analog_gain[0]) {
		info("[%s] not proper analog_gain value, reset to max_analog_gain\n", __func__);
		analog_gain = cis_data->max_analog_gain[0];
	}

	dbg_sensor(1, "%s [%d] VCNT[%d] input_again[%d] analog_gain[%d]\n",
		__func__, cis->id, cis_data->sen_vsync_count, again->val, analog_gain);

	analog_gain &= 0xFF;

	/* Set Analog gains */
	I2C_MUTEX_LOCK(cis->i2c_lock);
	ret = cis->ixc_ops->write8(cis->client, cis->reg_addr->again, analog_gain);
	if (ret < 0)
		goto p_err_unlock;

	/* Set Digital gains */
	if (analog_gain == cis_data->max_analog_gain[0]) {
		dgain_code = (u16)sensor_hi1336_cis_calc_dgain_code(dgain->val);
		if (dgain_code < cis_data->min_digital_gain[0]) {
			info("[%s] not proper dgain_code value, reset to min_digital_gain\n", __func__);
			dgain_code = cis_data->min_digital_gain[0];
		}
		if (dgain_code > cis_data->max_digital_gain[0]) {
			info("[%s] not proper dgain_code value, reset to max_digital_gain\n", __func__);
			dgain_code = cis_data->max_digital_gain[0];
		}
	} else {
		cal_analog_val1 = sensor_hi1336_cis_calc_again_code(again->val);
		cal_analog_val2 = sensor_hi1336_cis_calc_again_permile(cal_analog_val1);
		cal_digital = (again->val * 1000) / cal_analog_val2;
		dgain_code = (u16)sensor_hi1336_cis_calc_dgain_code(cal_digital);
		if (cal_digital < 0) {
			err("calculate dgain is fail");
			goto p_err_unlock;
		}
		dbg_sensor(1, "[%s] dgain compensation : input_again = %d, calculated_analog_gain = %d\n",
			__func__, again->val, cal_analog_val2);
	}

	dbg_sensor(1, "[%s] cal_digital = %d, dgain_code = %d(%#x)\n", __func__, cal_digital, dgain_code, dgain_code);
	dgains[0] = dgains[1] = dgains[2] = dgains[3] = dgain_code;
	ret = cis->ixc_ops->write16_array(cis->client, cis->reg_addr->dgain, dgains, 4);
	if (ret < 0)
		goto p_err_unlock;

	ret = cis->ixc_ops->read16(cis->client, SENSOR_HI1336_ISP_ENABLE_ADDR, &read_val);
	if (ret < 0)
		goto p_err_unlock;

	enable_dgain = read_val | (0x1 << 4); /* [4]: D gain enable */
	ret = cis->ixc_ops->write16(cis->client, SENSOR_HI1336_ISP_ENABLE_ADDR, enable_dgain);
	if (ret < 0)
		goto p_err_unlock;

	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %ldus\n", __func__, PABLO_KTIME_US_DELTA_NOW(st));

p_err_unlock:
	I2C_MUTEX_UNLOCK(cis->i2c_lock);
	return ret;
}

int sensor_hi1336_cis_get_analog_gain(struct v4l2_subdev *subdev, u32 *again)
{
	int ret = 0;
	struct is_cis *cis = sensor_cis_get_cis(subdev);

	u8 analog_gain = 0;
	ktime_t st = ktime_get();

	I2C_MUTEX_LOCK(cis->i2c_lock);
	ret = cis->ixc_ops->read8(cis->client, SENSOR_HI1336_ANALOG_GAIN_ADDR, &analog_gain);
	if (ret < 0)
		goto p_err_unlock;

	*again = sensor_hi1336_cis_calc_again_permile((u32)analog_gain);

	dbg_sensor(1, "[MOD:D:%d] %s, cur_again = %d us, analog_gain(%#x)\n",
		cis->id, __func__, *again, analog_gain);

	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %ldus\n", __func__, PABLO_KTIME_US_DELTA_NOW(st));

p_err_unlock:
	I2C_MUTEX_UNLOCK(cis->i2c_lock);
	return ret;
}

int sensor_hi1336_cis_set_digital_gain(struct v4l2_subdev *subdev, struct ae_param *dgain)
{
	int ret = 0;
	struct is_cis *cis = sensor_cis_get_cis(subdev);
	cis_shared_data *cis_data = NULL;

	u16 dgain_code = 0;
	u16 dgains[4] = {0};
	u16 read_val = 0;
	u16 enable_dgain = 0;
	ktime_t st = ktime_get();

	cis_data = cis->cis_data;

	dgain_code = (u16)sensor_hi1336_cis_calc_dgain_code(dgain->val);

	if (dgain_code < cis->cis_data->min_digital_gain[0]) {
		info("[%s] not proper dgain_code value, reset to min_analog_gain\n", __func__);
		dgain_code = cis->cis_data->min_digital_gain[0];
	}
	if (dgain_code > cis->cis_data->max_digital_gain[0]) {
		info("[%s] not proper dgain_code value, reset to min_analog_gain\n", __func__);
		dgain_code = cis->cis_data->max_digital_gain[0];
	}

	dbg_sensor(1, "[MOD:D:%d] %s(vsync cnt = %d), input_dgain = %d, dgain_code(%#x)\n",
			cis->id, __func__, cis->cis_data->sen_vsync_count, dgain->val, dgain_code);

	I2C_MUTEX_LOCK(cis->i2c_lock);

	dgains[0] = dgains[1] = dgains[2] = dgains[3] = dgain_code;
	ret = cis->ixc_ops->write16_array(cis->client, SENSOR_HI1336_DIG_GAIN_ADDR, dgains, 4);
	if (ret < 0) {
		goto p_err_unlock;
	}

	ret = cis->ixc_ops->read16(cis->client, SENSOR_HI1336_ISP_ENABLE_ADDR, &read_val);
	if (ret < 0) {
		goto p_err_unlock;
	}

	enable_dgain = read_val | (0x1 << 4); // [4]: D gain enable
	ret = cis->ixc_ops->write16(cis->client, SENSOR_HI1336_ISP_ENABLE_ADDR, enable_dgain);
	if (ret < 0) {
		goto p_err_unlock;
	}

	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %ldus\n", __func__, PABLO_KTIME_US_DELTA_NOW(st));

p_err_unlock:
	I2C_MUTEX_UNLOCK(cis->i2c_lock);

	return ret;
}

int sensor_hi1336_cis_set_exposure_time(struct v4l2_subdev *subdev, struct ae_param *target_exposure)
{
	int ret = 0;
	struct is_cis *cis = sensor_cis_get_cis(subdev);
	cis_shared_data *cis_data = NULL;

	u64 pix_rate_freq_khz = 0;
	u16 coarse_int = 0;
	u32 line_length_pck = 0;
	u32 min_fine_int = 0;
	ktime_t st = ktime_get();

	if ((target_exposure->long_val <= 0) || (target_exposure->short_val <= 0)) {
		err("invalid target exposure(%d, %d)", target_exposure->long_val, target_exposure->short_val);
		ret = -EINVAL;
		goto p_err;
	}

	cis_data = cis->cis_data;
	cis_data->last_exp = *target_exposure;

	pix_rate_freq_khz = cis_data->pclk / 1000;
	line_length_pck = cis_data->line_length_pck;
	min_fine_int = cis_data->min_fine_integration_time;

	coarse_int = (u16)(((target_exposure->val * pix_rate_freq_khz) - min_fine_int) / (line_length_pck * 1000));

	if (coarse_int > cis_data->max_coarse_integration_time) {
		dbg_sensor(1, "%s [%d] VCNT[%d] coarse(%d) max(%d)\n",
			__func__, cis->id, cis_data->sen_vsync_count, coarse_int, cis_data->max_coarse_integration_time);
		coarse_int = cis_data->max_coarse_integration_time;
	}

	if (coarse_int < cis_data->min_coarse_integration_time) {
		dbg_sensor(1, "%s [%d] VCNT[%d] coarse(%d) min(%d)\n",
			__func__, cis->id, cis_data->sen_vsync_count, coarse_int, cis_data->min_coarse_integration_time);
		coarse_int = cis_data->min_coarse_integration_time;
	}

	cis_data->cur_exposure_coarse = coarse_int;
	cis_data->cur_long_exposure_coarse = coarse_int;
	cis_data->cur_short_exposure_coarse = coarse_int;

	I2C_MUTEX_LOCK(cis->i2c_lock);
	ret = cis->ixc_ops->write16(cis->client, cis->reg_addr->cit, coarse_int);
		if (ret < 0)
			goto p_err_unlock;

	dbg_sensor(1, "%s [%d] VCNT[%d] target[%d] => FLL[%#x] CIT[%#x]\n",
		__func__, cis->id, cis_data->sen_vsync_count, target_exposure->val, cis_data->frame_length_lines, coarse_int);

	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %ldus\n", __func__, PABLO_KTIME_US_DELTA_NOW(st));

p_err_unlock:
	I2C_MUTEX_UNLOCK(cis->i2c_lock);

p_err:
	return ret;
}

int sensor_hi1336_cis_set_totalgain(struct v4l2_subdev *subdev, struct ae_param *target_exposure, 
	struct ae_param *again, struct ae_param *dgain)
{
	int ret = 0;
	
	/* Set Exposure Time */
	ret = sensor_hi1336_cis_set_exposure_time(subdev, target_exposure);

	/* Set Analog & Digital gains */
	ret |= sensor_hi1336_cis_set_analog_digital_gain(subdev, again, dgain);
	return ret;
}

static struct is_cis_ops cis_ops_hi1336 = {
	.cis_init = sensor_hi1336_cis_init,
	.cis_log_status = sensor_hi1336_cis_log_status,
#if USE_GROUP_PARAM_HOLD
	.cis_group_param_hold = sensor_hi1336_cis_group_param_hold,
#endif
	.cis_set_global_setting = sensor_hi1336_cis_set_global_setting,
	.cis_mode_change = sensor_hi1336_cis_mode_change,
	.cis_stream_on = sensor_hi1336_cis_stream_on,
	.cis_stream_off = sensor_hi1336_cis_stream_off,
	.cis_wait_streamoff = sensor_hi1336_cis_wait_streamoff,
	.cis_wait_streamon = sensor_hi1336_cis_wait_streamon,
	.cis_adjust_frame_duration = sensor_cis_adjust_frame_duration,
	.cis_set_frame_duration = sensor_cis_set_frame_duration,
	.cis_set_frame_rate = sensor_cis_set_frame_rate,
	.cis_set_exposure_time = NULL,
	.cis_get_min_exposure_time = sensor_cis_get_min_exposure_time,
	.cis_get_max_exposure_time = sensor_cis_get_max_exposure_time,
	.cis_adjust_analog_gain = sensor_cis_adjust_analog_gain,
	.cis_set_analog_gain = NULL,
	.cis_get_analog_gain = sensor_hi1336_cis_get_analog_gain,
	.cis_get_min_analog_gain = sensor_cis_get_min_analog_gain,
	.cis_get_max_analog_gain = sensor_cis_get_max_analog_gain,
	.cis_calc_again_code = sensor_hi1336_cis_calc_again_code,
	.cis_calc_again_permile = sensor_hi1336_cis_calc_again_permile,
	.cis_set_digital_gain = NULL,
	.cis_get_digital_gain = sensor_cis_get_digital_gain,
	.cis_get_min_digital_gain = sensor_cis_get_min_digital_gain,
	.cis_get_max_digital_gain = sensor_cis_get_max_digital_gain,
	.cis_calc_dgain_code = sensor_hi1336_cis_calc_dgain_code,
	.cis_calc_dgain_permile = sensor_hi1336_cis_calc_dgain_permile,
	.cis_set_totalgain = sensor_hi1336_cis_set_totalgain,
	.cis_compensate_gain_for_extremely_br = sensor_cis_compensate_gain_for_extremely_br,
	.cis_data_calculation = sensor_cis_data_calculation,
	.cis_check_rev_on_init = sensor_cis_check_rev_on_init,
};

int cis_hi1336_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	int ret;
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
	cis->cis_ops = &cis_ops_hi1336;
	/* belows are depend on sensor cis. MUST check sensor spec */
#ifdef APPLY_MIRROR_VERTICAL_FLIP
	cis->bayer_order = OTF_INPUT_ORDER_BAYER_GR_BG;
#else
	cis->bayer_order = OTF_INPUT_ORDER_BAYER_GB_RG;
#endif
	cis->use_total_gain = true;
	cis->use_wb_gain = true;
	cis->reg_addr = &sensor_hi1336_reg_addr;

	ret = of_property_read_string(dnode, "setfile", &setfile);
	if (ret) {
		err("setfile index read fail(%d), take default setfile!!", ret);
		setfile = "default";
	}

	if (strcmp(setfile, "default") == 0 || strcmp(setfile, "setA") == 0) {
		probe_info("%s setfile_A\n", __func__);
		sensor_hi1336_global = sensor_hi1336_setfile_A_Global;
		sensor_hi1336_global_size = ARRAY_SIZE(sensor_hi1336_setfile_A_Global);
		sensor_hi1336_setfile_fsync_info = sensor_hi1336_setfile_A_fsync_info;
		cis->sensor_info = &sensor_hi1336_info_A;
	} else {
		err("%s setfile index out of bound, take default (setfile_A)", __func__);
		sensor_hi1336_global = sensor_hi1336_setfile_A_Global;
		sensor_hi1336_global_size = ARRAY_SIZE(sensor_hi1336_setfile_A_Global);
		sensor_hi1336_setfile_fsync_info = sensor_hi1336_setfile_A_fsync_info;
		cis->sensor_info = &sensor_hi1336_info_A;
	}

	probe_info("%s done\n", __func__);
	return ret;
}

static const struct of_device_id sensor_cis_hi1336_match[] = {
	{
		.compatible = "samsung,exynos-is-cis-hi1336",
	},
	{},
};
MODULE_DEVICE_TABLE(of, sensor_cis_hi1336_match);

static const struct i2c_device_id sensor_cis_hi1336_idt[] = {
	{ SENSOR_NAME, 0 },
	{},
};

static struct i2c_driver sensor_cis_hi1336_driver = {
	.probe	= cis_hi1336_probe,
	.driver = {
		.name	= SENSOR_NAME,
		.owner	= THIS_MODULE,
		.of_match_table = sensor_cis_hi1336_match,
		.suppress_bind_attrs = true,
	},
	.id_table = sensor_cis_hi1336_idt
};

#ifdef MODULE
builtin_i2c_driver(sensor_cis_hi1336_driver);
#else
static int __init sensor_cis_hi1336_init(void)
{
	int ret;

	ret = i2c_add_driver(&sensor_cis_hi1336_driver);
	if (ret)
		err("failed to add %s driver: %d\n",
			sensor_cis_hi1336_driver.driver.name, ret);

	return ret;
}
late_initcall_sync(sensor_cis_hi1336_init);
#endif

MODULE_LICENSE("GPL");
MODULE_SOFTDEP("pre: fimc-is");
