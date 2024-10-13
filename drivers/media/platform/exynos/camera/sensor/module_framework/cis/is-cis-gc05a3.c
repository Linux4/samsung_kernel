/*
 * Samsung Exynos5 SoC series Sensor driver
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
#include "is-cis-gc05a3.h"
#include "is-cis-gc05a3-setA.h"

#include "is-helper-ixc.h"

#define SENSOR_NAME "GC05A3"

#define STREAM_OFF_WAIT_TIME 2000	/* 2ms */
#define STREAM_ON_WAIT_TIME 2000	/* 2ms */

/* CIS OPS */
int sensor_gc05a3_cis_init(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_cis *cis = sensor_cis_get_cis(subdev);

	info("[%s] init\n", __func__);

	cis->cis_data->stream_on = false;
	cis->cis_data->cur_width = cis->sensor_info->max_width;
	cis->cis_data->cur_height = cis->sensor_info->max_height;
	cis->cis_data->low_expo_start = 33000;
	cis->need_mode_change = false;

	cis->mipi_clock_index_cur = CAM_MIPI_NOT_INITIALIZED;
	cis->mipi_clock_index_new = CAM_MIPI_NOT_INITIALIZED;
	cis->cis_data->cur_pattern_mode = SENSOR_TEST_PATTERN_MODE_OFF;

	cis->cis_data->sens_config_index_pre = SENSOR_GC05A3_MODE_MAX;
	cis->cis_data->sens_config_index_cur = 0;
	CALL_CISOPS(cis, cis_data_calculation, subdev, cis->cis_data->sens_config_index_cur);

	is_vendor_set_mipi_mode(cis);

	return ret;
}

static const struct is_cis_log log_gc05a3[] = {
	{I2C_READ, 16, 0x0000, 0, "model_id"},
	{I2C_READ, 16, 0x0115, 0, "frame_count"},
	{I2C_READ, 16, 0x0100, 0, "mode_select"},
	{I2C_READ, 16, 0x0340, 0, "fll"},
	{I2C_READ, 16, 0x0342, 0, "llp"},
};

int sensor_gc05a3_cis_log_status(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_cis *cis = sensor_cis_get_cis(subdev);

	sensor_cis_log_status(cis, log_gc05a3, ARRAY_SIZE(log_gc05a3), (char *)__func__);

	return ret;
}

int sensor_gc05a3_cis_set_global_setting(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_cis *cis = sensor_cis_get_cis(subdev);
	struct sensor_gc05a3_private_data *priv = (struct sensor_gc05a3_private_data *)cis->sensor_info->priv;

	info("[%s] start\n", __func__);

	ret = sensor_cis_write_registers_locked(subdev, priv->global);
	if (ret < 0)
		err("global setting fail!!");

	info("[%s] done\n", __func__);

	return ret;
}

int sensor_gc05a3_cis_mode_change(struct v4l2_subdev *subdev, u32 mode)
{
	int ret = 0;
	struct is_cis *cis = sensor_cis_get_cis(subdev);
	struct is_device_sensor *device;
	const struct sensor_cis_mode_info *mode_info;

	device = (struct is_device_sensor *)v4l2_get_subdev_hostdata(subdev);
	WARN_ON(!device);

	if (mode >= cis->sensor_info->mode_count) {
		err("invalid mode(%d)!!", mode);
		ret = -EINVAL;
		goto p_err;
	}

	cis->mipi_clock_index_cur = CAM_MIPI_NOT_INITIALIZED;

	info("[%s] sensor mode(%d)\n", __func__, mode);

	mode_info = cis->sensor_info->mode_infos[mode];
	ret = sensor_cis_write_registers_locked(subdev, mode_info->setfile);
	if (ret < 0) {
		err("sensor_gc05a3_set_registers fail!!");
		goto p_err;
	}

	cis->cis_data->sens_config_index_pre = mode;

	info("[%s] mode changed(%d)\n", __func__, mode);

p_err:
	return ret;
}

int sensor_gc05a3_cis_stream_on(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_cis *cis = sensor_cis_get_cis(subdev);
	struct is_device_sensor *device;

	device = (struct is_device_sensor *)v4l2_get_subdev_hostdata(subdev);
	WARN_ON(!device);

	is_vendor_set_mipi_clock(device);

	/* Sensor stream on */
	IXC_MUTEX_LOCK(cis->ixc_lock);
	info("%s\n", __func__);
	ret = cis->ixc_ops->write8(cis->client, 0x0100, 0x01);
	IXC_MUTEX_UNLOCK(cis->ixc_lock);

	cis->cis_data->stream_on = true;

	return ret;
}

int sensor_gc05a3_cis_stream_off(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_cis *cis = sensor_cis_get_cis(subdev);
	u16 frame_count = 0;

	dbg_sensor(1, "[MOD:D:%d] %s\n", cis->id, __func__);

	/* Sensor stream off */
	IXC_MUTEX_LOCK(cis->ixc_lock);
	cis->ixc_ops->read16(cis->client, 0x0115, &frame_count);
	ret = cis->ixc_ops->write8(cis->client, 0x0100, 0x00);
	IXC_MUTEX_UNLOCK(cis->ixc_lock);

	info("%s done frame_count(%d)\n", __func__, frame_count);

	cis->cis_data->stream_on = false;

	return ret;
}

int sensor_gc05a3_cis_wait_streamon(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_cis *cis = sensor_cis_get_cis(subdev);
	u16 frame_count = 0;
	u16 curr_frame_count = 0;
	u32 wait_cnt = 0, time_out_cnt = 250;
	struct is_device_sensor *device;

	device = (struct is_device_sensor *)v4l2_get_subdev_hostdata(subdev);
	WARN_ON(!device);

	IXC_MUTEX_LOCK(cis->ixc_lock);
	ret = cis->ixc_ops->read16(cis->client, 0x0115, &frame_count);
	IXC_MUTEX_UNLOCK(cis->ixc_lock);
	if (ret < 0) {
		err("read frame_count fail, ret = %d\n", ret);
		goto p_err;
	}

	/* Checking stream on */
	do {
		IXC_MUTEX_LOCK(cis->ixc_lock);
		ret = cis->ixc_ops->read16(cis->client, 0x0115, &curr_frame_count);
		IXC_MUTEX_UNLOCK(cis->ixc_lock);

		if (curr_frame_count != frame_count)
			break;

		frame_count = curr_frame_count;
		usleep_range(STREAM_ON_WAIT_TIME, STREAM_ON_WAIT_TIME);
		wait_cnt++;

		dbg_sensor(1, "[MOD:D:%d] %s, sensor_fcount(%d), (wait_limit(%d) < time_out(%d))\n",
				cis->id, __func__, curr_frame_count, wait_cnt, time_out_cnt);
	} while (wait_cnt < time_out_cnt);

	if (wait_cnt < time_out_cnt)
		info("%s: finished after wait_cnt(%d)\n", __func__, wait_cnt);
	else
		warn("%s: failed, wait_cnt(%d) > time_out_cnt(%d)\n", __func__, wait_cnt, time_out_cnt);

p_err:
	return ret;
}

int sensor_gc05a3_cis_wait_streamoff(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_cis *cis = sensor_cis_get_cis(subdev);
	u32 wait_cnt = 0, time_out_cnt = 250;
	u16 curr_frame_count = 0;
	struct is_device_sensor *device;

	device = (struct is_device_sensor *)v4l2_get_subdev_hostdata(subdev);
	WARN_ON(!device);

	/* Checking stream off */
	do {
		IXC_MUTEX_LOCK(cis->ixc_lock);
		ret = cis->ixc_ops->read16(cis->client, 0x0115, &curr_frame_count);
		IXC_MUTEX_UNLOCK(cis->ixc_lock);

		if (curr_frame_count == 0x0000)
			break;

		usleep_range(STREAM_OFF_WAIT_TIME, STREAM_OFF_WAIT_TIME);
		wait_cnt++;

		dbg_sensor(1, "[MOD:D:%d] %s, sensor_fcount(%d), (wait_limit(%d) < time_out(%d))\n",
				cis->id, __func__, curr_frame_count, wait_cnt, time_out_cnt);
	} while (wait_cnt < time_out_cnt);

	if (wait_cnt < time_out_cnt)
		info("%s: finished after wait_cnt(%d)\n", __func__, wait_cnt);
	else
		warn("%s: failed, wait_cnt(%d) > time_out_cnt(%d)\n", __func__, wait_cnt, time_out_cnt);

	return ret;
}

int sensor_gc05a3_cis_get_otprom_data(struct v4l2_subdev *subdev, char *buf, bool camera_running, int rom_id)
{
	int ret = 0;
	u8 otp_bank = 0;
	u16 start_addr = 0;
	int index;
	struct is_cis *cis = sensor_cis_get_cis(subdev);

	info("[%s] camera_running(%d)\n", __func__, camera_running);

	/* 1. prepare to otp read : sensor initial settings */
	if (camera_running == false) {
		ret = cis->ixc_ops->write8(cis->client, 0x031C, 0x60);
		ret |= cis->ixc_ops->write8(cis->client, 0x0315, 0x80);
		ret |= cis->ixc_ops->write8(cis->client, 0x0AF4, 0x01);
		if (ret < 0) {
			err("%s failed to set initial values", __func__);
			goto exit;
		}
	}

	ret |= sensor_cis_set_registers(subdev, sensor_otp_gc05a3_read_init, ARRAY_SIZE(sensor_otp_gc05a3_read_init));
	if (ret < 0) {
		err("%s failed to read init_setting", __func__);
		goto exit;
	}
	usleep_range(10000, 10100); /* sleep 10 msec */
	
	/* 2. select otp bank */
	ret = cis->ixc_ops->write8(cis->client, GC05A3_OTP_ACCESS_ADDR_HIGH, (GC05A3_BANK_SELECT_ADDR >> 8) & 0xFF);
	ret |= cis->ixc_ops->write8(cis->client, GC05A3_OTP_ACCESS_ADDR_LOW, GC05A3_BANK_SELECT_ADDR & 0xFF);
	ret |= cis->ixc_ops->write8(cis->client, GC05A3_OTP_READ_WO_ADDR, GC05A3_OTP_READ_WO_DATA);
	if (ret < 0) {
		err("%s failed to select otp bank", __func__);
		goto exit;
	}

	ret = cis->ixc_ops->read8(cis->client, GC05A3_OTP_READ_ADDR, &otp_bank);
	if (ret < 0) {
		err("%s failed to read otp_bank", __func__);
		goto exit;
	}

	/* select start address */
	switch (otp_bank) {
	case 0x01:
		start_addr = GC05A3_MACRO_OTP_START_ADDR_BANK1;
		break;
	case 0x03:
		start_addr = GC05A3_MACRO_OTP_START_ADDR_BANK2;
		break;
	case 0x07:
		start_addr = GC05A3_MACRO_OTP_START_ADDR_BANK3;
		break;
	default:
		start_addr = GC05A3_MACRO_OTP_START_ADDR_BANK1;
		break;
	}
	info("%s: otp_bank = %d start_addr = 0x%x\n", __func__, otp_bank, start_addr);

	/* Continuous byte reading (ACC read) */
	ret |= sensor_cis_set_registers(subdev, sensor_otp_gc05a3_acc_read_init, ARRAY_SIZE(sensor_otp_gc05a3_acc_read_init));
	if (ret < 0) {
		err("%s failed to read acc init_setting", __func__);
		goto exit;
	}
	usleep_range(10000, 10100); /* sleep 10 msec */

	/* Read CAL data */
	ret = cis->ixc_ops->write8(cis->client, GC05A3_OTP_ACCESS_ADDR_HIGH, (start_addr >> 8) & 0xFF);
	ret |= cis->ixc_ops->write8(cis->client, GC05A3_OTP_ACCESS_ADDR_LOW, start_addr & 0xFF);
	ret |= cis->ixc_ops->write8(cis->client, GC05A3_OTP_READ_WO_ADDR, GC05A3_OTP_READ_WO_DATA);
	ret |= cis->ixc_ops->write8(cis->client, GC05A3_OTP_READ_WO_ADDR, GC05A3_OTP_ACC_READ_DATA);
	if (ret < 0) {
		err("%s failed to prepare read cal data from OTP bank(%d)", __func__, ret);
		goto exit;
	}

	for (index = 0; index < GC05A3_MACRO_OTP_USED_CAL_SIZE; index++) {
		ret = cis->ixc_ops->read8(cis->client, GC05A3_OTP_READ_ADDR, &buf[index]);
		if (ret < 0) {
			err("failed to otp_read(index:%d, ret:%d)", index, ret);
			goto exit;
		}
	}
exit:
	return ret;
}

u32 sensor_gc05a3_cis_calc_again_permile(u32 code)
{
	return (code * 1000 + 512) / 1024;
}

u32 sensor_gc05a3_cis_calc_again_code(u32 permile)
{
	return (permile * 1024 + 500) / 1000;
}

static struct is_cis_ops cis_ops = {
	.cis_init = sensor_gc05a3_cis_init,
	.cis_log_status = sensor_gc05a3_cis_log_status,
	.cis_group_param_hold = sensor_cis_set_group_param_hold,
	.cis_set_global_setting = sensor_gc05a3_cis_set_global_setting,
	.cis_mode_change = sensor_gc05a3_cis_mode_change,
	.cis_stream_on = sensor_gc05a3_cis_stream_on,
	.cis_stream_off = sensor_gc05a3_cis_stream_off,
	.cis_data_calculation = sensor_cis_data_calculation,
	.cis_wait_streamon = sensor_gc05a3_cis_wait_streamon,
	.cis_wait_streamoff = sensor_gc05a3_cis_wait_streamoff,
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
	.cis_calc_again_code = sensor_gc05a3_cis_calc_again_code,
	.cis_calc_again_permile = sensor_gc05a3_cis_calc_again_permile,
	.cis_compensate_gain_for_extremely_br = sensor_cis_compensate_gain_for_extremely_br,
	.cis_check_rev_on_init = sensor_cis_check_rev_on_init,
	.cis_set_initial_exposure = sensor_cis_set_initial_exposure,
	.cis_get_otprom_data = sensor_gc05a3_cis_get_otprom_data,
};

int cis_gc05a3_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	int ret;
	struct is_cis *cis = NULL;
	struct is_device_sensor_peri *sensor_peri = NULL;
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

	/* belows are depend on sensor cis. MUST check sensor spec */
	cis->bayer_order = OTF_INPUT_ORDER_BAYER_GR_BG;
	cis->reg_addr = &sensor_gc05a3_reg_addr;

	ret = of_property_read_string(dnode, "setfile", &setfile);
	if (ret) {
		err("setfile index read fail(%d), take default setfile!!", ret);
		setfile = "default";
	}

	if (strcmp(setfile, "default") == 0 || strcmp(setfile, "setA") == 0)
		probe_info("[%s] setfile_A mclk: 26MzMhz \n", __func__);
	else
		err("setfile index out of bound");

	cis->sensor_info = &sensor_gc05a3_info_A;

	probe_info("%s done\n", __func__);
	return ret;
}

static const struct of_device_id sensor_cis_gc05a3_match[] = {
	{
		.compatible = "samsung,exynos-is-cis-gc05a3-macro",
	},
	{},
};
MODULE_DEVICE_TABLE(of, sensor_cis_gc05a3_match);

static const struct i2c_device_id sensor_cis_gc05a3_idt[] = {
	{ SENSOR_NAME, 0 },
	{},
};

static struct i2c_driver sensor_cis_gc05a3_driver = {
	.probe	= cis_gc05a3_probe,
	.driver = {
		.name	= SENSOR_NAME,
		.owner	= THIS_MODULE,
		.of_match_table = sensor_cis_gc05a3_match,
		.suppress_bind_attrs = true,
	},
	.id_table = sensor_cis_gc05a3_idt,
};

#ifdef MODULE
builtin_i2c_driver(sensor_cis_gc05a3_driver);
#else
static int __init sensor_cis_gc05a3_init(void)
{
	int ret;

	ret = i2c_add_driver(&sensor_cis_gc05a3_driver);
	if (ret)
		err("failed to add %s driver: %d\n",
			sensor_cis_gc05a3_driver.driver.name, ret);

	return ret;
}
late_initcall_sync(sensor_cis_gc05a3_init);
#endif

MODULE_LICENSE("GPL");
MODULE_SOFTDEP("pre: fimc-is");
