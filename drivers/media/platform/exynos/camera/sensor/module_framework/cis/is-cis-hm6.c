/*
 * Samsung Exynos SoC series Sensor driver
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
#include "is-cis-hm6.h"
#include "is-cis-hm6-setA.h"
#include "is-helper-ixc.h"
#include "is-sec-define.h"

#define SENSOR_NAME "S5KHM6"

static bool sensor_hm6_eeprom_cal_available;
static bool sensor_hm6_first_entrance;

static bool sensor_hm6_cal_write_flag;
static bool sensor_hm6_cal_noremosaic_write_flag;

#ifdef HM6_BURST_WRITE
int sensor_hm6_cis_write16_burst(struct i2c_client *client, u16 addr, u8 *val, u32 num, bool endian)
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
	if (endian == HM6_BIG_ENDIAN) {
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

int sensor_hm6_cis_uXTC_write(struct v4l2_subdev *subdev, bool isRemosaic)
{
	int ret = 0;
	struct is_cis *cis = sensor_cis_get_cis(subdev);
	struct is_device_sensor_peri *sensor_peri = NULL;

	int position, cal_size = 0;
	u16 start_addr, end_addr;

#if !defined(HM6_BURST_WRITE)
	u16 val;
	u8 i = 0;
#endif

	ulong cal_addr;
	u8 cal_data[SENSOR_HM6_UXTC_FULL_SENS_CAL_SIZE] = {0, };
	char *cal_buf = NULL;
	struct sensor_hm6_private_data *priv = (struct sensor_hm6_private_data *)cis->sensor_info->priv;

	sensor_peri = container_of(cis, struct is_device_sensor_peri, cis);
	WARN_ON(!sensor_peri);

	position = sensor_peri->module->position;

	ret = is_sec_get_cal_buf(&cal_buf, position);
	if (ret < 0)
		goto p_err;

	cal_addr = (ulong)cal_buf;

	if (position == SENSOR_POSITION_REAR && isRemosaic) {
		cal_addr += SENSOR_HM6_UXTC_FULL_SENS_CAL_ADDR;
		start_addr = SENSOR_HM6_UXTC_FULL_SENS_CAL_ADDR;
		cal_size = SENSOR_HM6_UXTC_FULL_SENS_CAL_SIZE;
	} else if (position == SENSOR_POSITION_REAR && !isRemosaic) {
		cal_addr += SENSOR_HM6_UXTC_PARTIAL_SENS_CAL_ADDR;
		start_addr = SENSOR_HM6_UXTC_PARTIAL_SENS_CAL_ADDR;
		cal_size = SENSOR_HM6_UXTC_PARTIAL_SENS_CAL_SIZE;
	} else {
		err("cis_hm6 position(%d) is invalid!\n", position);
		goto p_err;
	}

	memcpy(cal_data, (u16 *)cal_addr, cal_size);

	ret = sensor_cis_write_registers(subdev, priv->xtc_prefix);

	/* EEPROM data */
	ret |= cis->ixc_ops->write16(cis->client, 0xFCFC, 0x4000);
	ret |= cis->ixc_ops->write16(cis->client, 0x6004, 0x0001); // Burst mode enable
	ret |= cis->ixc_ops->write16(cis->client, 0x6028, 0x2023);

	/* XTC data */
	end_addr = start_addr + cal_size;

	if (isRemosaic)
		ret |= cis->ixc_ops->write16(cis->client, 0x602A, 0x0000);
	else
		ret |= cis->ixc_ops->write16(cis->client, 0x602A, 0x173C);

#ifdef HM6_BURST_WRITE
	info("[%s] rom_xtc_cal burst write start(0x%X) end(0x%X) size(%d)\n",
			__func__, start_addr, end_addr, cal_size);
	ret = sensor_hm6_cis_write16_burst(cis->client, 0x6F12,
		cal_data, cal_size/2, HM6_BIG_ENDIAN);
	if (ret < 0) {
		err("sensor_hm6_cis_write16_burst fail!!");
		goto p_err;
	}
#else
	for (i = start_addr; i <= end_addr; i += 2) {
		val = HM6_ENDIAN(cal_buf[i], cal_buf[i + 1], HM6_BIG_ENDIAN);
		ret = cis->ixc_ops->write16(cis->client, 0x6F12, val);
		if (ret < 0) {
			err("is_sensor_write16 fail!!");
			goto p_err;
		}
#ifdef DEBUG_CAL_WRITE
		info("cal offset[0x%04X] , val[0x%04X]", i, val);
#endif
	}
#endif
	ret |= cis->ixc_ops->write16(cis->client, 0xFCFC, 0x4000);
	ret |= cis->ixc_ops->write16(cis->client, 0x6004, 0x0000); // Burst mode disable
	info("[%s] eeprom write done\n", __func__);

	ret = sensor_cis_write_registers(subdev, priv->xtc_fw_testvector_golden);

	ret = sensor_cis_write_registers(subdev, priv->xtc_postfix);

	ret |= cis->ixc_ops->write16(cis->client, 0xFCFC, 0x2001);
	if (isRemosaic)
		ret |= cis->ixc_ops->write16(cis->client, 0x3AEC, 0x0000);
	else
		ret |= cis->ixc_ops->write16(cis->client, 0x3AEC, 0x0100);

p_err:
	info("[%s] done (ret:%d)\n", __func__, ret);
	return ret;
}

/* CIS OPS */
int sensor_hm6_cis_init(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_cis *cis = sensor_cis_get_cis(subdev);

	sensor_hm6_cal_write_flag = false;
	sensor_hm6_cal_noremosaic_write_flag = false;
	sensor_hm6_first_entrance = true;

	cis->cis_data->stream_on = false;
	cis->cis_data->cur_width = cis->sensor_info->max_width;
	cis->cis_data->cur_height = cis->sensor_info->max_height;
	cis->cis_data->low_expo_start = 33000;
	cis->cis_data->remosaic_mode = false;
	cis->cis_data->pre_12bit_mode = SENSOR_12BIT_STATE_OFF;
	cis->cis_data->cur_12bit_mode = SENSOR_12BIT_STATE_OFF;
	cis->cis_data->pre_lownoise_mode = IS_CIS_LNOFF;
	cis->cis_data->cur_lownoise_mode = IS_CIS_LNOFF;
	cis->need_mode_change = false;
	cis->long_term_mode.sen_strm_off_on_step = 0;
	cis->long_term_mode.sen_strm_off_on_enable = false;
	cis->mipi_clock_index_cur = CAM_MIPI_NOT_INITIALIZED;
	cis->mipi_clock_index_new = CAM_MIPI_NOT_INITIALIZED;
	cis->cis_data->cur_pattern_mode = SENSOR_TEST_PATTERN_MODE_OFF;

	cis->cis_data->sens_config_index_pre = SENSOR_HM6_MODE_MAX;
	cis->cis_data->sens_config_index_cur = 0;
	CALL_CISOPS(cis, cis_data_calculation, subdev, cis->cis_data->sens_config_index_cur);

	return ret;
}

int sensor_hm6_cis_deinit(struct v4l2_subdev *subdev)
{
	int ret = 0;

	return ret;
}

static const struct is_cis_log log_hm6[] = {
	/* 4000 page */
	{I2C_WRITE, 16, 0x6000, 0x0005, "page unlock"},
	{I2C_WRITE, 16, 0xFCFC, 0x4000, "0x4000 page"},
	{I2C_READ, 16, 0x0000, 0, "model_id"},
	{I2C_READ, 16, 0x0002, 0, "revision_number"},
	{I2C_READ, 8, 0x0005, 0, "frame_count"},
	{I2C_READ, 16, 0x0100, 0, "0x0100"},
	{I2C_READ, 8, 0x0104, 0, "0x0104"},
	{I2C_READ, 16, 0x0118, 0, "0x0118"},
	{I2C_READ, 16, 0x021E, 0, "0x021E"},
	{I2C_READ, 16, 0x0202, 0, "0x0202"},
	{I2C_READ, 16, 0x0702, 0, "0x0702"},
	{I2C_READ, 16, 0x0704, 0, "0x0704"},
	{I2C_READ, 16, 0x0204, 0, "0x0204"},
	{I2C_READ, 16, 0x020E, 0, "0x020E"},
	{I2C_READ, 16, 0x0340, 0, "0x0340"},
	{I2C_READ, 16, 0x0342, 0, "0x0342"},
	{I2C_READ, 16, 0x034C, 0, "0x034C"},
	{I2C_READ, 16, 0x034E, 0, "0x034E"},
	{I2C_READ, 16, 0x0112, 0, "0x0112"},
	{I2C_READ, 16, 0x0B30, 0, "0x0B30"},
	{I2C_READ, 16, 0x0B32, 0, "0x0B32"},
	{I2C_READ, 16, 0x0E00, 0, "0x0E00"},
	{I2C_READ, 16, 0x19C2, 0, "0x19C2"},
	{I2C_WRITE, 16, 0xFCFC, 0x2001, "0x2001 page"},
	{I2C_READ, 16, 0x96A0, 0, "0x96A0"},
	{I2C_READ, 16, 0x96A2, 0, "0x96A2"},
	{I2C_READ, 16, 0x96A4, 0, "0x96A4"},
	{I2C_READ, 16, 0x96A6, 0, "0x96A6"},
	{I2C_READ, 16, 0x96AA, 0, "0x96AA"},
	/* restore 4000 page */
	{I2C_WRITE, 16, 0xFCFC, 0x4000, "0x4000 page"},
	{I2C_WRITE, 16, 0x6000, 0x0085, "page lock"},
};

int sensor_hm6_cis_log_status(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_cis *cis = sensor_cis_get_cis(subdev);

	sensor_cis_log_status(cis, log_hm6, ARRAY_SIZE(log_hm6), (char *)__func__);

	return ret;
}

int sensor_hm6_cis_mode_change(struct v4l2_subdev *subdev, u32 mode)
{
	int ret = 0;
	struct is_cis *cis = sensor_cis_get_cis(subdev);
	struct is_device_sensor *device;
	const struct sensor_cis_mode_info *mode_info;
	struct sensor_hm6_private_data *priv = (struct sensor_hm6_private_data *)cis->sensor_info->priv;
	u32 ex_mode;
	//u16 fast_change_idx = 0x00FF;

	device = (struct is_device_sensor *)v4l2_get_subdev_hostdata(subdev);
	WARN_ON(!device);

	if (mode >= cis->sensor_info->mode_count) {
		err("invalid mode(%d)!!", mode);
		ret = -EINVAL;
		goto p_err;
	}

	ex_mode = is_sensor_g_ex_mode(device);

	info("[%s] sensor mode(%d) ex_mode(%d)\n", __func__, mode, ex_mode);

	cis->mipi_clock_index_cur = CAM_MIPI_NOT_INITIALIZED;

	IXC_MUTEX_LOCK(cis->ixc_lock);

	mode_info = cis->sensor_info->mode_infos[mode];

	//uXTC call for sensor mode
	if (sensor_hm6_cal_write_flag == false &&  mode_info->remosaic_mode == true) {
		sensor_hm6_cal_write_flag = true;

		info("[%s] Remosaic Mode !! Write uXTC data.\n", __func__);

		sensor_hm6_cis_uXTC_write(subdev, mode_info->remosaic_mode);
	} else if(sensor_hm6_cal_noremosaic_write_flag == false && mode_info->remosaic_mode == false) {
		sensor_hm6_cal_noremosaic_write_flag = true;

		info("[%s] Non-Remosaic mode !! Write uXTC data.\n", __func__);

		sensor_hm6_cis_uXTC_write(subdev, mode_info->remosaic_mode);
	} else {
		info("[%s] uXTC data already read\n", __func__);
	}

	//Fast Change disable
	ret |= cis->ixc_ops->write16(cis->client, 0xFCFC, 0x4000);
	ret |= cis->ixc_ops->write16(cis->client, 0x0B30, 0x00FF);

	if (cis->cis_data->sens_config_index_pre == SENSOR_HM6_2000X1500_120FPS_FASTAE
		&& mode == SENSOR_HM6_4000X3000_30FPS_9SUM) {
		info("[%s] fastaeTo9sum mode\n", __func__);
		ret |= sensor_cis_write_registers(subdev, priv->fastaeTo9sum);
	} else {
		ret |= sensor_cis_write_registers(subdev, mode_info->setfile);
	}

	if (ret < 0) {
		err("sensor_hm6_set_registers fail!!");
		goto p_err_i2c_unlock;
	}

	cis->cis_data->sens_config_index_pre = mode;
	cis->cis_data->remosaic_mode = mode_info->remosaic_mode;

	cis->cis_data->pre_12bit_mode = mode_info->state_12bit;
	cis->cis_data->cur_12bit_mode = mode_info->state_12bit;

	cis->cis_data->pre_lownoise_mode = IS_CIS_LNOFF;
	cis->cis_data->cur_lownoise_mode = IS_CIS_LNOFF;

	cis->cis_data->pre_hdr_mode = SENSOR_HDR_MODE_SINGLE;
	cis->cis_data->cur_hdr_mode = SENSOR_HDR_MODE_SINGLE;

p_err_i2c_unlock:
	IXC_MUTEX_UNLOCK(cis->ixc_lock);

p_err:
	/* sensor_hm6_cis_log_status(subdev); */
	info("[%s] mode changed(%d)\n", __func__, mode);

	return ret;
}

int sensor_hm6_cis_set_global_setting(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_cis *cis = sensor_cis_get_cis(subdev);
	struct sensor_hm6_private_data *priv = (struct sensor_hm6_private_data *)cis->sensor_info->priv;

	dbg_sensor(1, "[%s] start\n", __func__);

	ret = sensor_cis_write_registers_locked(subdev, priv->global);
	if (ret < 0)
		err("global setting fail!!");

	info("[%s] global setting done\n", __func__);

	return ret;
}

int sensor_hm6_cis_stream_on(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_cis *cis = sensor_cis_get_cis(subdev);
	struct is_device_sensor *device;
	ktime_t st = ktime_get();

	device = (struct is_device_sensor *)v4l2_get_subdev_hostdata(subdev);
	WARN_ON(!device);

	dbg_sensor(1, "[MOD:D:%d] %s\n", cis->id, __func__);

	is_vendor_set_mipi_clock(device);

	IXC_MUTEX_LOCK(cis->ixc_lock);
	/* Sensor stream on */
	cis->ixc_ops->write16(cis->client, 0x0100, 0x0100);
	IXC_MUTEX_UNLOCK(cis->ixc_lock);

	cis->cis_data->stream_on = true;
	info("%s done\n", __func__);

	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %ldus", __func__, PABLO_KTIME_US_DELTA_NOW(st));

	return ret;
}

int sensor_hm6_cis_stream_off(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_cis *cis = sensor_cis_get_cis(subdev);
	u8 frame_count = 0;
	u16 fast_change_idx = 0x00FF;
	ktime_t st = ktime_get();

	dbg_sensor(1, "[MOD:D:%d] %s\n", cis->id, __func__);

	ret = CALL_CISOPS(cis, cis_group_param_hold, subdev, false);
	if (ret < 0)
		err("group_param_hold_func failed at stream off");

	IXC_MUTEX_LOCK(cis->ixc_lock);
	cis->ixc_ops->read8(cis->client, 0x0005, &frame_count);
	cis->ixc_ops->read16(cis->client, 0x0B30, &fast_change_idx);
	cis->ixc_ops->write8(cis->client, 0x0100, 0x00);
	IXC_MUTEX_UNLOCK(cis->ixc_lock);

	info("%s done frame_count(%d) fast_change_idx(0x%x)\n", __func__, frame_count, fast_change_idx);
	cis->cis_data->stream_on = false;

	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %ldus", __func__, PABLO_KTIME_US_DELTA_NOW(st));

	return ret;
}

int sensor_hm6_cis_set_wb_gain(struct v4l2_subdev *subdev, struct wb_gains wb_gains)
{
	int ret = 0;
	struct is_cis *cis = sensor_cis_get_cis(subdev);
	const struct sensor_cis_mode_info *mode_info;
	int mode = 0;
	u16 abs_gains[3] = {0, }; /* R, G, B */
	u32 avg_g = 0;
	ktime_t st = ktime_get();

	if (!cis->use_wb_gain)
		return ret;

	mode = cis->cis_data->sens_config_index_cur;
	mode_info = cis->sensor_info->mode_infos[mode];

	if (!mode_info->wb_gain_support)
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

	IXC_MUTEX_LOCK(cis->ixc_lock);
	ret |= cis->ixc_ops->write16(cis->client, 0xFCFC, 0x4000);
	ret |= cis->ixc_ops->write16(cis->client, 0x0D82, abs_gains[0]);
	ret |= cis->ixc_ops->write16(cis->client, 0x0D84, abs_gains[1]);
	ret |= cis->ixc_ops->write16(cis->client, 0x0D86, abs_gains[2]);
	IXC_MUTEX_UNLOCK(cis->ixc_lock);

	if (ret < 0)
		err("sensor_hm6_set_registers fail!!");

	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %ldus", __func__, PABLO_KTIME_US_DELTA_NOW(st));

	return ret;
}

static struct is_cis_ops cis_ops_hm6 = {
	.cis_init = sensor_hm6_cis_init,
	.cis_deinit = sensor_hm6_cis_deinit,
	.cis_log_status = sensor_hm6_cis_log_status,
	.cis_group_param_hold = sensor_cis_set_group_param_hold,
	.cis_set_global_setting = sensor_hm6_cis_set_global_setting,
	.cis_mode_change = sensor_hm6_cis_mode_change,
	.cis_stream_on = sensor_hm6_cis_stream_on,
	.cis_stream_off = sensor_hm6_cis_stream_off,
	.cis_wait_streamon = sensor_cis_wait_streamon,
	.cis_wait_streamoff = sensor_cis_wait_streamoff,
	.cis_data_calculation = sensor_cis_data_calculation,
	.cis_set_exposure_time = sensor_cis_set_exposure_time,
	.cis_get_min_exposure_time = sensor_cis_get_min_exposure_time,
	.cis_get_max_exposure_time = sensor_cis_get_max_exposure_time,
	.cis_set_long_term_exposure = sensor_cis_long_term_exposure,
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
	.cis_check_rev_on_init = sensor_cis_check_rev_on_init,
	.cis_set_initial_exposure = sensor_cis_set_initial_exposure,
	.cis_set_wb_gains = sensor_hm6_cis_set_wb_gain,
};

static int cis_hm6_probe_i2c(struct i2c_client *client,
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
	cis->cis_ops = &cis_ops_hm6;
	/* belows are depend on sensor cis. MUST check sensor spec */
	cis->bayer_order = OTF_INPUT_ORDER_BAYER_GR_BG;
	cis->use_wb_gain = true;
	//cis->use_seamless_mode = true;
	cis->reg_addr = &sensor_hm6_reg_addr;

	sensor_hm6_eeprom_cal_available = false;
	sensor_hm6_first_entrance = false;

	ret = of_property_read_string(dnode, "setfile", &setfile);
	if (ret) {
		err("setfile index read fail(%d), take default setfile!!", ret);
		setfile = "default";
	}

	if (strcmp(setfile, "default") == 0 || strcmp(setfile, "setA") == 0)
		probe_info("[%s] setfile_A mclk: 26Mhz \n", __func__);
	else
		err("setfile index out of bound, take default (setfile_A mclk: 26Mhz)");

	cis->sensor_info = &sensor_hm6_info_A;

	probe_info("%s done\n", __func__);

	return ret;
}

static const struct of_device_id sensor_cis_hm6_match[] = {
	{
		.compatible = "samsung,exynos-is-cis-hm6",
	},
	{},
};
MODULE_DEVICE_TABLE(of, sensor_cis_hm6_match);

static const struct i2c_device_id sensor_cis_hm6_idt[] = {
	{ SENSOR_NAME, 0 },
	{},
};

static struct i2c_driver sensor_cis_hm6_driver = {
	.probe	= cis_hm6_probe_i2c,
	.driver = {
		.name	= SENSOR_NAME,
		.owner	= THIS_MODULE,
		.of_match_table = sensor_cis_hm6_match,
		.suppress_bind_attrs = true,
	},
	.id_table = sensor_cis_hm6_idt
};

#ifdef MODULE
builtin_i2c_driver(sensor_cis_hm6_driver);
#else
static int __init sensor_cis_hm6_init(void)
{
	int ret;

	ret = i2c_add_driver(&sensor_cis_hm6_driver);
	if (ret)
		err("failed to add %s driver: %d",
			sensor_cis_hm6_driver.driver.name, ret);

	return ret;
}
late_initcall_sync(sensor_cis_hm6_init);
#endif

MODULE_LICENSE("GPL");
