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
#include "is-cis-gd2.h"
#include "is-cis-gd2-setA.h"

#include "is-helper-ixc.h"
#include "is-sec-define.h"

#define SENSOR_NAME "S5KGD2"
/* #define DEBUG_GD2_PLL */

static bool sensor_gd2_cal_write_flag;

int sensor_gd2_cis_check_rev_on_init(struct v4l2_subdev *subdev)
{
	int ret = 0;
	u16 rev = 0;
	u8 status = 0;
	struct is_cis *cis = sensor_cis_get_cis(subdev);

	memset(cis->cis_data, 0, sizeof(cis_shared_data));

	IXC_MUTEX_LOCK(cis->ixc_lock);

	cis->ixc_ops->write16(cis->client, 0x6028, 0x4000);
	//cis->ixc_ops->write8(cis->client, 0x0100, 0x01);

	/* Specify OTP Page Address for READ - Page0(dec) */
	ret = cis->ixc_ops->write16(cis->client, SENSOR_GD2_OTP_PAGE_SETUP_ADDR, 0x0000);
	/* Turn ON OTP Read MODE - Read Start */
	ret |= cis->ixc_ops->write16(cis->client, SENSOR_GD2_OTP_READ_TRANSFER_MODE_ADDR,  0x0100);
	if (ret < 0) {
		err("cis->ixc_ops->write8 fail (ret %d)", ret);
		IXC_MUTEX_UNLOCK(cis->ixc_lock);
		return -EAGAIN;
	}

	/* Check status - 0x00 : read ready*/
	cis->ixc_ops->read8(cis->client, SENSOR_GD2_OTP_STATUS_REGISTER_ADDR,  &status);
	if (status != 0x0)
		err("status fail, (%#x)", status);
	err("status (%#x)", status);

	ret = cis->ixc_ops->read16(cis->client, SENSOR_GD2_OTP_CHIP_REVISION_ADDR, &rev);
	if (ret < 0) {
		err("cis->ixc_ops->read8 fail (ret %d)", ret);
		IXC_MUTEX_UNLOCK(cis->ixc_lock);
		return -EAGAIN;
	}
	ret |= cis->ixc_ops->write16(cis->client, SENSOR_GD2_OTP_READ_TRANSFER_MODE_ADDR,  0x0000);

	IXC_MUTEX_UNLOCK(cis->ixc_lock);

	cis->cis_data->cis_rev = rev;

	pr_info("%s : [%d] Sensor version. Rev(addr:0x%X). 0x%X\n",
		__func__, cis->id, SENSOR_GD2_OTP_CHIP_REVISION_ADDR, cis->cis_data->cis_rev);
	return ret;
}

/* CIS OPS */
int sensor_gd2_cis_init(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_cis *cis = sensor_cis_get_cis(subdev);
	ktime_t st = ktime_get();

	sensor_gd2_cal_write_flag = false;

	cis->cis_data->cur_width = cis->sensor_info->max_width;
	cis->cis_data->cur_height = cis->sensor_info->max_height;
	cis->cis_data->low_expo_start = 33000;
	cis->need_mode_change = false;
	cis->long_term_mode.sen_strm_off_on_step = 0;
	cis->long_term_mode.sen_strm_off_on_enable = false;

	cis->mipi_clock_index_cur = CAM_MIPI_NOT_INITIALIZED;
	cis->mipi_clock_index_new = CAM_MIPI_NOT_INITIALIZED;
	cis->cis_data->cur_pattern_mode = SENSOR_TEST_PATTERN_MODE_OFF;

	cis->cis_data->sens_config_index_pre = SENSOR_GD2_MODE_MAX;
	cis->cis_data->sens_config_index_cur = 0;
	CALL_CISOPS(cis, cis_data_calculation, subdev, cis->cis_data->sens_config_index_cur);

	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %ldus", __func__, PABLO_KTIME_US_DELTA_NOW(st));

	return ret;
}

static const struct is_cis_log log_gd2[] = {
	{I2C_READ, 16, 0x0000, 0, "model_id"},
	{I2C_READ, 8, 0x0002, 0, "rev_number"},
	{I2C_READ, 8, 0x0005, 0, "frame_count"},
	{I2C_READ, 16, 0x0100, 0, "mode_select"},
	{I2C_READ, 16, 0x0340, 0, "fll"},
	{I2C_READ, 16, 0x0342, 0, "llp"},
};

int sensor_gd2_cis_log_status(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_cis *cis = sensor_cis_get_cis(subdev);

	sensor_cis_log_status(cis, log_gd2, ARRAY_SIZE(log_gd2), (char *)__func__);

	return ret;
}

int sensor_gd2_apply_cross_talk_settings(struct is_cis *cis)
{
	int ret = 0;
	char *cal_buf;
	u8 *cal_data;
	struct is_rom_info *finfo = NULL;
	int i;
	int lsc_size, xtc_size;

	is_sec_get_cal_buf(&cal_buf, ROM_ID_FRONT);
	is_sec_get_sysfs_finfo(&finfo, ROM_ID_FRONT);

	if (finfo->rom_xtc_cal_data_start_addr == -1) {
		err("[%s] cal addr empty");
		return ret;
	}

	if (finfo->rom_xtc_cal_data_addr_list_len < 2) {
		err("lsc or xtc empty, size:%d", finfo->rom_xtc_cal_data_addr_list_len);
		return ret;
	}

	//calculate lsc size
	lsc_size = finfo->rom_xtc_cal_data_addr_list[1] - finfo->rom_xtc_cal_data_addr_list[0];
	xtc_size = finfo->rom_xtc_cal_data_size - lsc_size;

	//xtc data
	cal_data = &cal_buf[finfo->rom_xtc_cal_data_addr_list[1]];

	IXC_MUTEX_LOCK(cis->ixc_lock);
	cis->ixc_ops->write16(cis->client, 0xFCFC, 0x4000);
	cis->ixc_ops->write16(cis->client, 0x6000, 0x0005);
	cis->ixc_ops->write16(cis->client, 0xFCFC, 0x2002);

	for (i = 0; i < xtc_size; i += 2) {
		u16 val = (cal_data[i + 1] & 0xFF) << 8;
		val |= (cal_data[i] & 0xFF);
		ret = cis->ixc_ops->write16(cis->client, 0x3800 + i, val);
		if (ret < 0) {
			err("[lsc] i2c fail addr(%x), val(%04x), ret = %d\n",
				0x3800 + i, val, ret);
			goto p_err;
		}
	}

	info("[%s] done\n", __func__);
p_err:
	IXC_MUTEX_UNLOCK(cis->ixc_lock);
	return ret;
}

int sensor_gd2_apply_lsc_settings(struct is_cis *cis)
{
	int ret = 0;
	char *cal_buf;
	u8 *cal_data;
	struct is_rom_info *finfo = NULL;
	int i;
	int lsc_size;

	is_sec_get_cal_buf(&cal_buf, ROM_ID_FRONT);
	is_sec_get_sysfs_finfo(&finfo, ROM_ID_FRONT);

	if (finfo->rom_xtc_cal_data_start_addr == -1) {
		err("[%s] cal addr empty");
		return ret;
	}

	if (finfo->rom_xtc_cal_data_addr_list_len < 2) {
		err("lsc or xtc empty, size:%d", finfo->rom_xtc_cal_data_addr_list_len);
		return ret;
	}

	//calculate lsc size
	lsc_size = finfo->rom_xtc_cal_data_addr_list[1] - finfo->rom_xtc_cal_data_addr_list[0];

	//lsc data
	cal_data = &cal_buf[finfo->rom_xtc_cal_data_addr_list[0]];

	IXC_MUTEX_LOCK(cis->ixc_lock);
	cis->ixc_ops->write16(cis->client, 0xFCFC, 0x4000);
	cis->ixc_ops->write16(cis->client, 0x6000, 0x0085);

	for (i = 0; i < lsc_size; i += 2) {
		u16 val = (cal_data[i + 1] & 0xFF) << 8;
		val |= (cal_data[i] & 0xFF);
		ret = cis->ixc_ops->write16(cis->client, 0x6F12, val);
		if (ret < 0) {
			err("[lsc] i2c fail addr(%x), val(%04x), ret = %d\n",
				0x6F12, val, ret);
			goto p_err;
		}
	}

	info("[%s] done\n", __func__);
p_err:
	IXC_MUTEX_UNLOCK(cis->ixc_lock);
	return ret;
}

int sensor_gd2_cis_set_global_setting(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_cis *cis = sensor_cis_get_cis(subdev);
	struct sensor_gd2_private_data *priv = (struct sensor_gd2_private_data *)cis->sensor_info->priv;

	ret = sensor_cis_write_registers_locked(subdev, priv->initial);
	if (ret < 0)
		err("initial settings fail!!");

	ret = sensor_cis_write_registers_locked(subdev, priv->tnp);
	if (ret < 0)
		err("tnp setting fail!!");

	ret = sensor_cis_write_registers_locked(subdev, priv->global);
	if (ret < 0)
		err("global setting fail!!");

	dbg_sensor(1, "[%s] global setting done\n", __func__);

	return ret;
}

int sensor_gd2_cis_mode_change(struct v4l2_subdev *subdev, u32 mode)
{
	int ret = 0;
	struct is_cis *cis = sensor_cis_get_cis(subdev);
	struct sensor_gd2_private_data *priv = (struct sensor_gd2_private_data *)cis->sensor_info->priv;
	const struct sensor_cis_mode_info *mode_info;

	if (mode > cis->sensor_info->mode_count) {
		err("invalid mode(%d)!!", mode);
		ret = -EINVAL;
		goto p_err;
	}

	cis->mipi_clock_index_cur = CAM_MIPI_NOT_INITIALIZED;

	mode_info = cis->sensor_info->mode_infos[mode];

	if (sensor_gd2_cal_write_flag == false && mode_info->remosaic_mode) {
		info("[%s] Remosaic Mode !! Write cal data.\n", __func__);
 
		ret = sensor_gd2_apply_cross_talk_settings(cis);
		if (ret < 0)
			err("sensor_gd2_apply_cross_talk_settings fail!!");

		ret = sensor_cis_write_registers_locked(subdev, priv->global_calibration);
		if (ret < 0)
			err("global_calibration fail!!");

		sensor_gd2_cal_write_flag = true;
	}

	ret = sensor_cis_write_registers_locked(subdev, mode_info->setfile);
	if (ret < 0) {
		err("mode(%d) setfile fail!!", mode);
		goto p_err;
	}

	cis->cis_data->sens_config_index_pre = mode;
	cis->cis_data->remosaic_mode = mode_info->remosaic_mode;

#ifdef DISABLE_GD2_PDAF_TAIL_MODE
	ret = sensor_cis_set_registers(subdev, sensor_gd2_pdaf_off_setfile_B, ARRAY_SIZE(sensor_gd2_pdaf_off_setfile_B));
	if (ret < 0) {
		err("sensor_gd2_pdaf_off_setfiles fail!!");
		goto p_err;
	}
#endif

	cis->cis_data->frame_time = (cis->cis_data->line_readOut_time * cis->cis_data->cur_height / 1000);
	cis->cis_data->rolling_shutter_skew = (cis->cis_data->cur_height - 1) * cis->cis_data->line_readOut_time;
	dbg_sensor(1, "[%s] frame_time(%d), rolling_shutter_skew(%lld)\n", __func__,
		cis->cis_data->frame_time, cis->cis_data->rolling_shutter_skew);

	info("[%s] mode changed(%d)\n", __func__, mode);

p_err:
	return ret;
}

int sensor_gd2_cis_stream_on(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_cis *cis = sensor_cis_get_cis(subdev);
	ktime_t st = ktime_get();

	dbg_sensor(1, "[MOD:D:%d] %s\n", cis->id, __func__);

	if (cis->stream_state != CIS_STREAM_SET_DONE) {
		err("%s: cis stream state id %d", __func__, cis->stream_state);
		return -EINVAL;
	}

	IXC_MUTEX_LOCK(cis->ixc_lock);
	cis->ixc_ops->write16(cis->client, 0x6028, 0x4000);
	cis->ixc_ops->write8(cis->client, 0x0100, 0x01);
	IXC_MUTEX_UNLOCK(cis->ixc_lock);

	cis->cis_data->stream_on = true;
	info("%s done\n", __func__);

	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %ldus", __func__, PABLO_KTIME_US_DELTA_NOW(st));

	return ret;
}

int sensor_gd2_cis_stream_off(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_cis *cis = sensor_cis_get_cis(subdev);
	ktime_t st = ktime_get();

	dbg_sensor(1, "[MOD:D:%d] %s\n", cis->id, __func__);

	ret = CALL_CISOPS(cis, cis_group_param_hold, subdev, false);
	if (ret < 0)
		err("group_param_hold_func failed at stream off");

	IXC_MUTEX_LOCK(cis->ixc_lock);
	cis->ixc_ops->write16(cis->client, 0x6028, 0x4000);
	cis->ixc_ops->write8(cis->client, 0x0100, 0x00);
	IXC_MUTEX_UNLOCK(cis->ixc_lock);

	info("%s done\n", __func__);

	cis->cis_data->stream_on = false;

	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %ldus", __func__, PABLO_KTIME_US_DELTA_NOW(st));

	return ret;
}

static struct is_cis_ops cis_ops = {
	.cis_init = sensor_gd2_cis_init,
	.cis_log_status = sensor_gd2_cis_log_status,
	.cis_group_param_hold = sensor_cis_set_group_param_hold,
	.cis_set_global_setting = sensor_gd2_cis_set_global_setting,
	.cis_mode_change = sensor_gd2_cis_mode_change,
	.cis_stream_on = sensor_gd2_cis_stream_on,
	.cis_stream_off = sensor_gd2_cis_stream_off,
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
//	.cis_active_test = sensor_cis_active_test,
};

static int cis_gd2_probe(struct i2c_client *client,
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
	cis->cis_ops = &cis_ops;
	/* belows are depend on sensor cis. MUST check sensor spec */
	cis->bayer_order = OTF_INPUT_ORDER_BAYER_GR_BG;
	cis->reg_addr = &sensor_gd2_reg_addr;

	ret = of_property_read_string(dnode, "setfile", &setfile);
	if (ret) {
		err("setfile index read fail(%d), take default setfile!!", ret);
		setfile = "default";
	}

	if (strcmp(setfile, "default") == 0 || strcmp(setfile, "setA") == 0)
		probe_info("[%s] setfile_A mclk: 26Mhz \n", __func__);
	else
		err("setfile index out of bound, take default (setfile_A mclk: 26Mhz)");

	cis->sensor_info = &sensor_gd2_info_A;

	probe_info("%s done\n", __func__);

	return ret;
}

static const struct of_device_id sensor_cis_gd2_match[] = {
	{
		.compatible = "samsung,exynos-is-cis-gd2",
	},
	{},
};
MODULE_DEVICE_TABLE(of, sensor_cis_gd2_match);

static const struct i2c_device_id sensor_cis_gd2_idt[] = {
	{ SENSOR_NAME, 0 },
	{},
};

static struct i2c_driver sensor_cis_gd2_driver = {
	.probe	= cis_gd2_probe,
	.driver = {
		.name	= SENSOR_NAME,
		.owner	= THIS_MODULE,
		.of_match_table = sensor_cis_gd2_match,
		.suppress_bind_attrs = true,
	},
	.id_table = sensor_cis_gd2_idt
};

#ifdef MODULE
builtin_i2c_driver(sensor_cis_gd2_driver);
#else
static int __init sensor_cis_gd2_init(void)
{
	int ret;

	ret = i2c_add_driver(&sensor_cis_gd2_driver);
	if (ret)
		err("failed to add %s driver: %d\n",
			sensor_cis_gd2_driver.driver.name, ret);

	return ret;
}
late_initcall_sync(sensor_cis_gd2_init);
#endif

MODULE_LICENSE("GPL");
MODULE_SOFTDEP("pre: fimc-is");
