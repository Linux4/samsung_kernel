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
#include "is-cis-gc08a3.h"
#include "is-cis-gc08a3-setA.h"

#include "is-helper-ixc.h"

#define SENSOR_NAME "GC08A3"
/* #define DEBUG_GC08A3_PLL */

#define MAX_WAIT_STREAM_ON_CNT (100)
#define MAX_WAIT_STREAM_OFF_CNT (100)

#define GC08A3_ADDR_FRAME_COUNT                                 0x146
#define GC08A3_OTP_ACCESS_ADDR_HIGH                             0xA69
#define GC08A3_OTP_ACCESS_ADDR_LOW                              0xA6A
#define GC08A3_OTP_READ_ADDR                                    0xA6C
#define GC08A3_OTP_MODE_SEL_ADDR                                0x313
#define GC08A3_READ_MODE                                         0x20
#define GC08A3_OTP_ACC                                           0x12

#define GC08A3_BANK_SELECT_ADDR                                 0x15A0
#define GC08A3_OTP_START_ADDR_BANK1                             0x15C0
#define GC08A3_OTP_START_ADDR_BANK2                             0x4AC0
#define GC08A3_OTP_START_ADDR_BANK3                             0x7FC0

#define GC08A3_OTP_USED_CAL_SIZE                                ((0x4A9F - 0x15C0 + 0x1) / 8)

/*************************************************
 *  [GC08A3 Analog / Digital gain formular]
 *
 *  Analog / Digital Gain = (Reg value) / 1024
 *
 *  Analog / Digital Gain Range = x1.0 to x16.0
 *
 *************************************************/

u32 sensor_gc08a3_cis_calc_gain_code(u32 permile)
{
	return ((permile * 1024) / 1000);
}

u32 sensor_gc08a3_cis_calc_gain_permile(u32 code)
{
	return ((code * 1000 / 1024));
}

/* CIS OPS */
int sensor_gc08a3_cis_init(struct v4l2_subdev *subdev)
{
	int ret = 0;
	ktime_t st = ktime_get();
	struct is_cis *cis = sensor_cis_get_cis(subdev);

	cis->cis_data->stream_on = false;
	cis->cis_data->cur_width = cis->sensor_info->max_width;
	cis->cis_data->cur_height = cis->sensor_info->max_height;
	cis->cis_data->low_expo_start = 33000;
	cis->need_mode_change = false;

	cis->mipi_clock_index_cur = CAM_MIPI_NOT_INITIALIZED;
	cis->mipi_clock_index_new = CAM_MIPI_NOT_INITIALIZED;
	cis->cis_data->sens_config_index_pre = SENSOR_GC08A3_MODE_MAX;
	cis->cis_data->sens_config_index_cur = 0;

	CALL_CISOPS(cis, cis_data_calculation, subdev, cis->cis_data->sens_config_index_cur);

	is_vendor_set_mipi_mode(cis);

	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %ldus", __func__, PABLO_KTIME_US_DELTA_NOW(st));

	return ret;
}

static const struct is_cis_log log_gc08a3[] = {
	{I2C_READ, 16, 0x0000, 0, "model_id"},
	{I2C_READ, 8, 0x0002, 0, "rev_number"},
	{I2C_READ, 16, GC08A3_ADDR_FRAME_COUNT, 0, "frame_count"},
	{I2C_READ, 8, 0x0100, 0, "mode_select"},
	{I2C_READ, 16, 0x0136, 0, ""},
	{I2C_READ, 16, 0x0202, 0, "cit"},
	{I2C_READ, 16, 0x0204, 0, "again"},
	{I2C_READ, 16, 0x0340, 0, "fll"},
	{I2C_READ, 16, 0x0342, 0, "llp"},
	{I2C_READ, 8, 0x3C02, 0, ""},
	{I2C_READ, 8, 0x3C03, 0, ""},
	{I2C_READ, 8, 0x3C05, 0, ""},
	{I2C_READ, 8, 0x3500, 0, ""},
};

int sensor_gc08a3_cis_log_status(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_cis *cis = sensor_cis_get_cis(subdev);

	sensor_cis_log_status(cis, log_gc08a3, ARRAY_SIZE(log_gc08a3), (char *)__func__);

	return ret;
}

int sensor_gc08a3_cis_set_global_setting(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_cis *cis = sensor_cis_get_cis(subdev);
	struct sensor_gc08a3_private_data *priv = (struct sensor_gc08a3_private_data *)cis->sensor_info->priv;

	info("[%s] start\n", __func__);

	ret = sensor_cis_write_registers_locked(subdev, priv->global);
	if (ret < 0)
		err("global setting fail!!");

	info("[%s] done\n", __func__);

	return ret;
}

int sensor_gc08a3_cis_mode_change(struct v4l2_subdev *subdev, u32 mode)
{
	int ret = 0;
	const struct sensor_cis_mode_info *mode_info;
	struct is_cis *cis = sensor_cis_get_cis(subdev);

	if (mode >= cis->sensor_info->mode_count) {
		err("invalid mode(%d)!!", mode);
		return -EINVAL;
	}

	cis->mipi_clock_index_cur = CAM_MIPI_NOT_INITIALIZED;

	info("[%s] sensor mode(%d)\n", __func__, mode);

	mode_info = cis->sensor_info->mode_infos[mode];

	ret = sensor_cis_write_registers_locked(subdev, mode_info->setfile);
	if (ret < 0) {
		err("sensor_setfiles fail!!");
		return ret;
	}

	cis->cis_data->sens_config_index_pre = mode;

	info("[%s] mode changed(%d)\n", __func__, mode);

	return ret;
}

int sensor_gc08a3_cis_stream_on(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_device_sensor *device;
	struct is_cis *cis = sensor_cis_get_cis(subdev);
	cis_shared_data *cis_data;
	ktime_t st = ktime_get();

	device = (struct is_device_sensor *)v4l2_get_subdev_hostdata(subdev);
	WARN_ON(!device);

	cis_data = cis->cis_data;

	dbg_sensor(1, "[MOD:D:%d] %s\n", cis->id, __func__);

	is_vendor_set_mipi_clock(device);

#ifdef DEBUG_GC08A3_PLL
	{
	u16 pll;
	IXC_MUTEX_LOCK(cis->ixc_lock);
	cis->ixc_ops->read16(cis->client, 0x0300, &pll);
	dbg_sensor(1, "______ vt_pix_clk_div(%x)\n", pll);
	cis->ixc_ops->read16(cis->client, 0x0302, &pll);
	dbg_sensor(1, "______ vt_sys_clk_div(%x)\n", pll);
	cis->ixc_ops->read16(cis->client, 0x0304, &pll);
	dbg_sensor(1, "______ pre_pll_clk_div(%x)\n", pll);
	cis->ixc_ops->read16(cis->client, 0x0306, &pll);
	dbg_sensor(1, "______ pll_multiplier(%x)\n", pll);
	cis->ixc_ops->read16(cis->client, 0x0308, &pll);
	dbg_sensor(1, "______ op_pix_clk_div(%x)\n", pll);
	cis->ixc_ops->read16(cis->client, 0x030a, &pll);
	dbg_sensor(1, "______ op_sys_clk_div(%x)\n", pll);

	cis->ixc_ops->read16(cis->client, 0x030c, &pll);
	dbg_sensor(1, "______ secnd_pre_pll_clk_div(%x)\n", pll);
	cis->ixc_ops->read16(cis->client, 0x030e, &pll);
	dbg_sensor(1, "______ secnd_pll_multiplier(%x)\n", pll);
	cis->ixc_ops->read16(cis->client, 0x0340, &pll);
	dbg_sensor(1, "______ frame_length_lines(%x)\n", pll);
	cis->ixc_ops->read16(cis->client, 0x0342, &pll);
	dbg_sensor(1, "______ line_length_pck(%x)\n", pll);
	IXC_MUTEX_UNLOCK(cis->ixc_lock);
	}
#endif

	/* first frame should be delayed */
	/* To resolve blinking issue */
	if ((cis->cis_data->sens_config_index_cur == SENSOR_GC08A3_1632X1224_60FPS)
		|| (cis_data->is_data.scene_mode == AA_SCENE_MODE_PANORAMA)
		|| (cis_data->is_data.scene_mode == AA_SCENE_MODE_SUPER_NIGHT)
		|| IS_VIDEO_SCENARIO(device->ischain->setfile & IS_SETFILE_MASK))
		msleep(30);

	IXC_MUTEX_LOCK(cis->ixc_lock);
	/* Sensor stream on */
	cis->ixc_ops->write8(cis->client, 0x0100, 0x01);
	IXC_MUTEX_UNLOCK(cis->ixc_lock);

	info("%s\n", __func__);

	cis_data->stream_on = true;

	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %ldus", __func__, PABLO_KTIME_US_DELTA_NOW(st));

	return ret;
}

int sensor_gc08a3_cis_stream_off(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_cis *cis = sensor_cis_get_cis(subdev);
	u16 frame_count = 0;
	ktime_t st = ktime_get();

	dbg_sensor(1, "[MOD:D:%d] %s\n", cis->id, __func__);

	IXC_MUTEX_LOCK(cis->ixc_lock);
	cis->ixc_ops->read16(cis->client, GC08A3_ADDR_FRAME_COUNT, &frame_count);
	/* Sensor stream off */
	ret = cis->ixc_ops->write8(cis->client, 0x0100, 0x00);
	IXC_MUTEX_UNLOCK(cis->ixc_lock);

	info("%s done frame_count(%d)\n", __func__, frame_count);

	cis->cis_data->stream_on = false;

	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %lldus\n", __func__, PABLO_KTIME_US_DELTA_NOW(st));

	return ret;
}

int sensor_gc08a3_cis_wait_streamon(struct v4l2_subdev *subdev)
{
	int ret = 0;
	u32 polling_cnt = 0;
	struct is_cis *cis;
	struct i2c_client *client;
	cis_shared_data *cis_data;
	u16 prev_frame_value = 0;
	u16 cur_frame_value = 0;

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

	IXC_MUTEX_LOCK(cis->ixc_lock);
	ret = cis->ixc_ops->read16(client, GC08A3_ADDR_FRAME_COUNT, &prev_frame_value);
	IXC_MUTEX_UNLOCK(cis->ixc_lock);

	if (ret < 0) {
		err("i2c transfer fail addr(%x), val(%x), ret = %d\n", GC08A3_ADDR_FRAME_COUNT, prev_frame_value, ret);
		goto p_err;
	}

	/* Checking stream on */
	do {
		IXC_MUTEX_LOCK(cis->ixc_lock);
		ret = cis->ixc_ops->read16(client, GC08A3_ADDR_FRAME_COUNT, &cur_frame_value);
		IXC_MUTEX_UNLOCK(cis->ixc_lock);
		if (ret < 0) {
			err("i2c transfer fail addr(%x), val(%x), ret = %d\n", GC08A3_ADDR_FRAME_COUNT, cur_frame_value, ret);
			break;
		}

		if (cur_frame_value != prev_frame_value)
			break;

		prev_frame_value = cur_frame_value;

		usleep_range(2000, 2100);
		polling_cnt++;
		dbg_sensor(1, "[MOD:D:%d] %s, fcount(%d), (polling_cnt(%d) < MAX_WAIT_STREAM_ON_CNT(%d))\n",
				cis->id, __func__, prev_frame_value, polling_cnt, MAX_WAIT_STREAM_ON_CNT);
	} while (polling_cnt < MAX_WAIT_STREAM_ON_CNT);

	if (polling_cnt < MAX_WAIT_STREAM_ON_CNT)
		info("%s: finished after %d ms\n", __func__, polling_cnt);
	else
		warn("%s: finished : polling timeout occurred after %d ms\n", __func__, polling_cnt);

p_err:
	return ret;
}

int sensor_gc08a3_cis_recover_stream_on(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_cis *cis = sensor_cis_get_cis(subdev);;

	info("%s start\n", __func__);

	ret = sensor_gc08a3_cis_set_global_setting(subdev);
	if (ret < 0)
		goto p_err;
	ret = sensor_gc08a3_cis_mode_change(subdev, cis->cis_data->sens_config_index_cur);
	if (ret < 0)
		goto p_err;
	ret = sensor_gc08a3_cis_stream_on(subdev);
	if (ret < 0)
		goto p_err;
	ret = sensor_gc08a3_cis_wait_streamon(subdev);
	if (ret < 0)
		goto p_err;

	info("%s end\n", __func__);
p_err:
	return ret;
}

int sensor_gc08a3_cis_get_otprom_data(struct v4l2_subdev *subdev, char *buf, bool camera_running, int rom_id)
{
	int ret = 0;
	struct is_cis *cis = sensor_cis_get_cis(subdev);
	u8 otp_bank;
	u16 start_addr = 0;
	struct is_device_sensor_peri *sensor_peri = NULL;
	struct sensor_gc08a3_private_data *priv = (struct sensor_gc08a3_private_data *)cis->sensor_info->priv;
	int rom_type;
	u8 start_addr_h = 0;
	u8 start_addr_l = 0;
	int index;

	info("%s-E\n", __func__);
	sensor_peri = container_of(cis, struct is_device_sensor_peri, cis);
	rom_type = sensor_peri->module->pdata->rom_type;

	info("[%s] camera_running(%d) rom_type(%d)\n", __func__, camera_running, rom_type);

	/* 1. prepare to otp read : sensor initial settings */
	if (camera_running == false) {
		ret = cis->ixc_ops->write8(cis->client, 0x031C, 0x60);
		ret |= cis->ixc_ops->write8(cis->client, 0x0315, 0x80);
		if (ret < 0) {
			err("initial setting fail for OTP!!");
			ret = -EINVAL;
			goto exit;
		}
	}

	ret = sensor_cis_write_registers(subdev, priv->otprom_init);
	if (ret < 0) {
		err("otprom_init setting fail");
		ret = -EINVAL;
		goto exit;
	}

	usleep_range(10000, 10100); /* 10ms delay */

	/* 2. Read OTP page */
	ret = cis->ixc_ops->write8(cis->client, GC08A3_OTP_ACCESS_ADDR_HIGH, ((GC08A3_BANK_SELECT_ADDR >> 8) & 0xFF));
	ret |= cis->ixc_ops->write8(cis->client, GC08A3_OTP_ACCESS_ADDR_LOW, (GC08A3_BANK_SELECT_ADDR & 0xFF));
	ret |= cis->ixc_ops->write8(cis->client, GC08A3_OTP_MODE_SEL_ADDR, GC08A3_READ_MODE);
	ret |= cis->ixc_ops->read8(cis->client, GC08A3_OTP_READ_ADDR, &otp_bank);
	if (ret < 0) {
		err("failed to Read OTP page (%d)", ret);
		ret = -EINVAL;
		goto exit;
	}

	/* 3. Select start address */
	switch (otp_bank) {
	case 0x01:
		start_addr = GC08A3_OTP_START_ADDR_BANK1;
		break;
	case 0x03:
		start_addr = GC08A3_OTP_START_ADDR_BANK2;
		break;
	case 0x07:
		start_addr = GC08A3_OTP_START_ADDR_BANK3;
		break;
	default:
		start_addr = GC08A3_OTP_START_ADDR_BANK1;
		break;
	}

	info("%s: otp_bank = 0x%x, otp_start_addr = 0x%x\n", __func__, otp_bank, start_addr);

	/* 4. Read cal data */
	usleep_range(10000, 10100); /* 10ms delay */

	start_addr_h = ((start_addr >> 8) & 0xFF);
	start_addr_l = (start_addr & 0xFF);

	ret = cis->ixc_ops->write8(cis->client, GC08A3_OTP_ACCESS_ADDR_HIGH, start_addr_h);
	ret |= cis->ixc_ops->write8(cis->client, GC08A3_OTP_ACCESS_ADDR_LOW, start_addr_l);
	ret |= cis->ixc_ops->write8(cis->client, GC08A3_OTP_MODE_SEL_ADDR, GC08A3_READ_MODE);
	ret |= cis->ixc_ops->write8(cis->client, GC08A3_OTP_MODE_SEL_ADDR, GC08A3_OTP_ACC);
	if (ret < 0) {
		err("%s failed to prepare read cal data from OTP bank(%d)", __func__, ret);
		goto exit;
	}

	usleep_range(2000, 2100); /* 2ms delay */
	for (index = 0; index < GC08A3_OTP_USED_CAL_SIZE; index++) {
		ret = cis->ixc_ops->read8(cis->client, GC08A3_OTP_READ_ADDR, &buf[index]);
		if (ret < 0) {
			err("failed to otp_read(index:%d, ret:%d)", index, ret);
			goto exit;
		}
	}
exit:
	info("OTP Off\n");
	cis->ixc_ops->write8(cis->client, 0x0ACE, 0x00);
	cis->ixc_ops->write8(cis->client, GC08A3_OTP_MODE_SEL_ADDR, 0x00);
	cis->ixc_ops->write8(cis->client, 0x0A67, 0x00);
	cis->ixc_ops->write8(cis->client, 0x0316, 0x00);

	cis->ixc_ops->write8(cis->client, 0x0315, 0x00);
	cis->ixc_ops->write8(cis->client, 0x031C, 0x00);

#ifdef USE_DEFAULT_CAL_GC08A3
	if (ret < 0) {
		info("set default cal - E\n");
		memcpy((void *)buf, (void *)&gc08a3_default_cal, sizeof(gc08a3_default_cal));
		info("set default cal - X\n");
		ret = 0;
	}
#endif
	usleep_range(2000, 2100); /* 2ms delay */

	info("%s-X\n", __func__);
	return ret;
}

static struct is_cis_ops cis_ops = {
	.cis_init = sensor_gc08a3_cis_init,
	.cis_log_status = sensor_gc08a3_cis_log_status,
	.cis_set_global_setting = sensor_gc08a3_cis_set_global_setting,
	.cis_mode_change = sensor_gc08a3_cis_mode_change,
	.cis_stream_on = sensor_gc08a3_cis_stream_on,
	.cis_stream_off = sensor_gc08a3_cis_stream_off,
	.cis_wait_streamon = sensor_gc08a3_cis_wait_streamon,
	.cis_wait_streamoff = sensor_cis_wait_streamoff_mipi_end,
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
	.cis_calc_again_code = sensor_gc08a3_cis_calc_gain_code,
	.cis_calc_again_permile = sensor_gc08a3_cis_calc_gain_code,
	.cis_calc_dgain_code = sensor_gc08a3_cis_calc_gain_code,
	.cis_calc_dgain_permile = sensor_gc08a3_cis_calc_gain_code,
	.cis_set_digital_gain = sensor_cis_set_digital_gain,
	.cis_get_digital_gain = sensor_cis_get_digital_gain,
	.cis_get_min_digital_gain = sensor_cis_get_min_digital_gain,
	.cis_get_max_digital_gain = sensor_cis_get_max_digital_gain,
	.cis_compensate_gain_for_extremely_br = sensor_cis_compensate_gain_for_extremely_br,
	.cis_set_initial_exposure = sensor_cis_set_initial_exposure,
#ifdef USE_CAMERA_RECOVER_GC08A3
	.cis_recover_stream_on = sensor_gc08a3_cis_recover_stream_on,
#endif
	.cis_check_rev_on_init = sensor_cis_check_rev_on_init,
	.cis_get_otprom_data = sensor_gc08a3_cis_get_otprom_data,
};

int cis_gc08a3_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	int ret = 0;
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
	cis->bayer_order = OTF_INPUT_ORDER_BAYER_RG_GB;
	cis->reg_addr = &sensor_gc08a3_reg_addr;
	cis->use_dgain = false;
	cis->hdr_ctrl_by_again = false;

	cis->use_initial_ae = of_property_read_bool(dnode, "use_initial_ae");
	probe_info("%s use initial_ae(%d)\n", __func__, cis->use_initial_ae);

	ret = of_property_read_string(dnode, "setfile", &setfile);
	if (ret) {
		err("setfile index read fail(%d), take default setfile!!", ret);
		setfile = "default";
	}

	if (strcmp(setfile, "default") == 0 || strcmp(setfile, "setA") == 0)
		probe_info("[%s] setfile_A mclk: 26Mhz \n", __func__);
	else
		err("setfile index out of bound, take default (setfile_A mclk: 26Mhz)");

	cis->sensor_info = &sensor_gc08a3_info_A;

	is_vendor_set_mipi_mode(cis);

	probe_info("%s done\n", __func__);

	return ret;
}

static int cis_gc08a3_remove(struct i2c_client *client)
{
	int ret = 0;
	return ret;
}

static const struct of_device_id sensor_cis_gc08a3_match[] = {
	{
		.compatible = "samsung,exynos-is-cis-gc08a3",
	},
	{},
};
MODULE_DEVICE_TABLE(of, sensor_cis_gc08a3_match);

static const struct i2c_device_id sensor_cis_gc08a3_idt[] = {
	{ SENSOR_NAME, 0 },
	{},
};

static struct i2c_driver sensor_cis_gc08a3_driver = {
	.driver = {
		.name	= SENSOR_NAME,
		.owner	= THIS_MODULE,
		.of_match_table = sensor_cis_gc08a3_match
	},
	.probe	= cis_gc08a3_probe,
	.remove	= cis_gc08a3_remove,
	.id_table = sensor_cis_gc08a3_idt
};


#ifdef MODULE
builtin_i2c_driver(sensor_cis_gc08a3_driver);
#else
static int __init sensor_cis_gc08a3_init(void)
{
	int ret;

	ret = i2c_add_driver(&sensor_cis_gc08a3_driver);
	if (ret)
		err("failed to add %s driver: %d\n",
			sensor_cis_gc08a3_driver.driver.name, ret);

	return ret;
}
late_initcall_sync(sensor_cis_gc08a3_init);
#endif

MODULE_LICENSE("GPL");
MODULE_SOFTDEP("pre: fimc-is");
