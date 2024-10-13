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
#include "is-cis-imx616.h"
#include "is-vendor.h"
#include "is-cis-imx616-setA-19p2.h"

#include "is-helper-ixc.h"

#include "interface/is-interface-library.h"
#include "is-sec-define.h"

#define SENSOR_NAME "IMX616"

static bool sensor_imx616_cal_write_flag;

#if SENSOR_IMX616_EBD_LINE_DISABLE
static int sensor_imx616_set_ebd(struct is_cis *cis)
{
	int ret = 0;
	u8 enabled_ebd = 0;

	enabled_ebd = 0;	//default disabled
	ret = cis->ixc_ops->write8(cis->client, SENSOR_IMX616_EBD_CONTROL_ADDR, enabled_ebd);
	if (ret < 0) {
		err("is_sensor_read is fail, (ret %d)", ret);
		ret = -EAGAIN;
		goto p_err;
	}

p_err:
	return ret;
}
#endif

#if IS_ENABLED(CIS_CALIBRATION)
int sensor_imx616_cis_set_cal(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_cis *cis = sensor_cis_get_cis(subdev);
	struct is_device_sensor_peri *sensor_peri = NULL;

	struct is_vendor_rom *rom_info = NULL;
	char *cal_buf = NULL;
	int rom_id = 0;
	int start_addr = 0;
	int cal_size = 0;
	ktime_t st = ktime_get();

	sensor_peri = container_of(cis, struct is_device_sensor_peri, cis);
	WARN_ON(!sensor_peri);

	rom_id = is_vendor_get_rom_id_from_position(sensor_peri->module->position);
	info("[%s] start : rom_id(%d) \n", __func__, rom_id);

	is_sec_get_rom_info(&rom_info, rom_id);
 	if (!rom_info->read_done) {
		err("eeprom read fail status, skip set cal");
 		return 0;
	}

 	start_addr = rom_info->rom_xtc_cal_data_start_addr;
	cal_size = rom_info->rom_xtc_cal_data_size;
	if (start_addr < 0 || cal_size <= 0) {
		err("Not available DT, skip set cal");
		return 0;
	}

	cal_buf = rom_info->buf;

	IXC_MUTEX_LOCK(cis->ixc_lock);
	ret = cis->ixc_ops->write8_sequential(cis->client, IMX616_QSC_ADDR, (u8 *)&cal_buf[start_addr], cal_size);
	IXC_MUTEX_UNLOCK(cis->ixc_lock);

	if (ret < 0)
		err("QSC write Error(%d)", ret);

	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %lldus", __func__, PABLO_KTIME_US_DELTA_NOW(st));

	info("[%s] done \n", __func__);

	return ret;
}
#endif

/*************************************************
 *  [IMX616 Analog gain formular]
 *
 *  m0: [0x008c:0x008d] fixed to 0
 *  m1: [0x0090:0x0091] fixed to -1
 *  c0: [0x008e:0x008f] fixed to 1024
 *  c1: [0x0092:0x0093] fixed to 1024
 *  X : [0x0204:0x0205] Analog gain setting value
 *
 *  Analog Gain = (m0 * X + c0) / (m1 * X + c1)
 *              = 1024 / (1024 - X)
 *
 *  Analog Gain Range = 112 to 1008 (1dB to 26dB)
 *
 *************************************************/

u32 sensor_imx616_cis_calc_again_code(u32 permille)
{
	return (u32)(((u64)(1024 * 1000) - ((u64)(1024000 * 1000 / permille))) / 1000);
}

u32 sensor_imx616_cis_calc_again_permile(u32 code)
{
	return 1024000 / (1024 - code);
}

/* CIS OPS */
int sensor_imx616_cis_init(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_cis *cis = sensor_cis_get_cis(subdev);

	info("[%s] init %s\n", __func__, cis->use_3hdr ? "(Use 3HDR)" : "");

/***********************************************************************
***** Check that QSC and DPC Cal is written for Remosaic Capture.
***** false : Not yet write the QSC and DPC
***** true  : Written the QSC and DPC Or Skip
***********************************************************************/
	sensor_imx616_cal_write_flag = false;

	cis->cis_data->cur_width = cis->sensor_info->max_width;
	cis->cis_data->cur_height = cis->sensor_info->max_height;
	cis->cis_data->low_expo_start = 33000;
	cis->need_mode_change = false;
	cis->long_term_mode.sen_strm_off_on_step = 0;
	cis->long_term_mode.sen_strm_off_on_enable = false;

	cis->cis_data->sens_config_index_pre = SENSOR_IMX616_MODE_MAX;
	cis->cis_data->sens_config_index_cur = 0;
	CALL_CISOPS(cis, cis_data_calculation, subdev, cis->cis_data->sens_config_index_cur);

	return ret;
}


int sensor_imx616_cis_init_state(struct v4l2_subdev *subdev, int mode)
{
	struct is_cis *cis = sensor_cis_get_cis(subdev);

	cis->cis_data->stream_on = false;
	cis->mipi_clock_index_cur = CAM_MIPI_NOT_INITIALIZED;
	cis->mipi_clock_index_new = CAM_MIPI_NOT_INITIALIZED;
	cis->cis_data->cur_pattern_mode = SENSOR_TEST_PATTERN_MODE_OFF;

	return 0;
}

static const struct is_cis_log log_imx616[] = {
	{I2C_READ, 16, 0x0016, 0, "model_id"},
	{I2C_READ, 8, 0x0018, 0, "rev_number"},
	{I2C_READ, 8, 0x0005, 0, "frame_count"},
	{I2C_READ, 16, 0x0100, 0, "mode_select"},
	{I2C_READ, 16, 0x0340, 0, "fll"},
	{I2C_READ, 16, 0x0342, 0, "llp"},
	{I2C_READ, 16, 0x0202, 0, "cit"},
	{I2C_READ, 16, 0x3100, 0, "shf"},
	{I2C_READ, 16, 0x0204, 0, "again"},
	{I2C_READ, 16, 0x020E, 0, "dgain"},
	{I2C_READ, 16, 0x0110, 0, "0x0110"},
	{I2C_READ, 16, 0x0112, 0, "0x0112"},
	{I2C_READ, 16, 0x0114, 0, "0x0114"},
	{I2C_READ, 16, 0x0136, 0, "0x0136"},
	{I2C_READ, 16, 0x0300, 0, "0x0300"},
	{I2C_READ, 16, 0x0302, 0, "0x0302"},
	{I2C_READ, 16, 0x0304, 0, "0x0304"},
	{I2C_READ, 16, 0x0306, 0, "0x0306"},
	{I2C_READ, 16, 0x030A, 0, "0x030A"},
	{I2C_READ, 16, 0x030C, 0, "0x030C"},
	{I2C_READ, 16, 0x030E, 0, "0x030E"},
	{I2C_READ, 16, 0x0344, 0, "0x0344"},
	{I2C_READ, 16, 0x0346, 0, "0x0346"},
	{I2C_READ, 16, 0x0348, 0, "0x0348"},
	{I2C_READ, 16, 0x034A, 0, "0x034A"},
	{I2C_READ, 16, 0x034C, 0, "0x034C"},
	{I2C_READ, 16, 0x034E, 0, "0x034E"},
	{I2C_READ, 16, 0x0350, 0, "0x0350"},
	{I2C_READ, 16, 0x0408, 0, "0x0408"},
	{I2C_READ, 16, 0x040A, 0, "0x040A"},
	{I2C_READ, 16, 0x040C, 0, "0x040C"},
	{I2C_READ, 16, 0x040E, 0, "0x040E"},
	{I2C_READ, 16, 0x0808, 0, "0x0808"},
	{I2C_READ, 16, 0x080A, 0, "0x080A"},
	{I2C_READ, 16, 0x080C, 0, "0x080C"},
	{I2C_READ, 16, 0x080E, 0, "0x080E"},
	{I2C_READ, 16, 0x0810, 0, "0x0810"},
	{I2C_READ, 16, 0x0812, 0, "0x0812"},
	{I2C_READ, 16, 0x0814, 0, "0x0814"},
	{I2C_READ, 16, 0x0816, 0, "0x0816"},
	{I2C_READ, 16, 0x0818, 0, "0x0818"},
	{I2C_READ, 16, 0x0820, 0, "0x0820"},
	{I2C_READ, 16, 0x0822, 0, "0x0822"},
	{I2C_READ, 16, 0x0824, 0, "0x0824"},
	{I2C_READ, 16, 0x0826, 0, "0x0826"},
	{I2C_READ, 16, 0x0830, 0, "0x0830"},
	{I2C_READ, 16, 0x0832, 0, "0x0832"},
	{I2C_READ, 16, 0x0842, 0, "0x0842"},
	{I2C_READ, 16, 0x0844, 0, "0x0844"},
	{I2C_READ, 16, 0x0846, 0, "0x0846"},
	{I2C_READ, 16, 0x0848, 0, "0x0848"},
	{I2C_READ, 16, 0x084E, 0, "0x084E"},
	{I2C_READ, 16, 0x0850, 0, "0x0850"},
	{I2C_READ, 16, 0x0852, 0, "0x0852"},
	{I2C_READ, 16, 0x0854, 0, "0x0854"},
	{I2C_READ, 16, 0x0856, 0, "0x0856"},
	{I2C_READ, 16, 0x0858, 0, "0x0858"},
	{I2C_READ, 16, 0x0860, 0, "0x0860"},
	{I2C_READ, 16, 0x0862, 0, "0x0862"},
	{I2C_READ, 16, 0x0900, 0, "0x0900"},
	{I2C_READ, 16, 0x0E00, 0, "0x0E00"},
	{I2C_READ, 16, 0x0E28, 0, "0x0E28"},
	{I2C_READ, 16, 0x0E58, 0, "0x0E58"},
	{I2C_READ, 16, 0x0E88, 0, "0x0E88"},
	{I2C_READ, 16, 0x0EB8, 0, "0x0EB8"},
	{I2C_READ, 16, 0x0F08, 0, "0x0F08"},
	{I2C_READ, 16, 0x0F38, 0, "0x0F38"},
	{I2C_READ, 16, 0x0F68, 0, "0x0F68"},
	{I2C_READ, 16, 0x0F98, 0, "0x0F98"},
	{I2C_READ, 16, 0x3004, 0, "0x3004"},
	{I2C_READ, 16, 0x3090, 0, "0x3090"},
	{I2C_READ, 16, 0x3092, 0, "0x3092"},
	{I2C_READ, 16, 0x3094, 0, "0x3094"},
	{I2C_READ, 16, 0x3096, 0, "0x3096"},
	{I2C_READ, 16, 0x3098, 0, "0x3098"},
	{I2C_READ, 16, 0x3102, 0, "0x3102"},
	{I2C_READ, 16, 0x3104, 0, "0x3104"},
	{I2C_READ, 16, 0x3106, 0, "0x3106"},
	{I2C_READ, 16, 0x310B, 0, "0x310B"},
	{I2C_READ, 16, 0x3160, 0, "0x3160"},
	{I2C_READ, 16, 0x3190, 0, "0x3190"},
	{I2C_READ, 16, 0x319A, 0, "0x319A"},
	{I2C_READ, 16, 0x319C, 0, "0x319C"},
	{I2C_READ, 16, 0x319E, 0, "0x319E"},
	{I2C_READ, 16, 0x31A6, 0, "0x31A6"},
	{I2C_READ, 16, 0x31B0, 0, "0x31B0"},
	{I2C_READ, 16, 0x31C0, 0, "0x31C0"},
	{I2C_READ, 16, 0x3238, 0, "0x3238"},
	{I2C_READ, 16, 0x3300, 0, "0x3300"},
	{I2C_READ, 16, 0x3304, 0, "0x3304"},
	{I2C_READ, 16, 0x3310, 0, "0x3310"},
	{I2C_READ, 16, 0x3322, 0, "0x3322"},
	{I2C_READ, 16, 0x3328, 0, "0x3328"},
	{I2C_READ, 16, 0x332A, 0, "0x332A"},
	{I2C_READ, 16, 0x332C, 0, "0x332C"},
	{I2C_READ, 16, 0x332E, 0, "0x332E"},
	{I2C_READ, 16, 0x3372, 0, "0x3372"},
	{I2C_READ, 16, 0x3378, 0, "0x3378"},
	{I2C_READ, 16, 0x337A, 0, "0x337A"},
	{I2C_READ, 16, 0x337C, 0, "0x337C"},
	{I2C_READ, 16, 0x3384, 0, "0x3384"},
	{I2C_READ, 16, 0x3386, 0, "0x3386"},
	{I2C_READ, 16, 0x3388, 0, "0x3388"},
	{I2C_READ, 16, 0x338A, 0, "0x338A"},
	{I2C_READ, 16, 0x338C, 0, "0x338C"},
	{I2C_READ, 16, 0x338E, 0, "0x338E"},
	{I2C_READ, 16, 0x3390, 0, "0x3390"},
	{I2C_READ, 16, 0x3392, 0, "0x3392"},
	{I2C_READ, 16, 0x3750, 0, "0x3750"},
	{I2C_READ, 16, 0x3802, 0, "0x3802"},
	{I2C_READ, 16, 0x3806, 0, "0x3806"},
	{I2C_READ, 16, 0x380A, 0, "0x380A"},
	{I2C_READ, 16, 0x380E, 0, "0x380E"},
	{I2C_READ, 16, 0x3878, 0, "0x3878"},
};

int sensor_imx616_cis_log_status(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_cis *cis = sensor_cis_get_cis(subdev);

	sensor_cis_log_status(cis, log_imx616, ARRAY_SIZE(log_imx616), (char *)__func__);

	return ret;
}

int sensor_imx616_cis_set_global_setting(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_cis *cis = sensor_cis_get_cis(subdev);
	struct sensor_imx616_private_data *priv = (struct sensor_imx616_private_data *)cis->sensor_info->priv;

	info("[%s] global setting start\n", __func__);

	ret = sensor_cis_write_registers_locked(subdev, priv->global);
	if (ret < 0)
		err("global setting fail!!");

	info("[%s] global setting done\n", __func__);

	/* Check that QSC and DPC Cal is written for Remosaic Capture.
	   false : Not yet write the QSC and DPC
	   true  : Written the QSC and DPC */
	sensor_imx616_cal_write_flag = false;
	return ret;
}

#ifdef SUPPORT_SENSOR_SEAMLESS_3HDR
/**
 * @brief sensor_imx616_cis_get_seamless_trasition_mode
 *  : get simplified tansition mode matched with recieved mode.
 * @return
 *  : transtion mode if success.
 */
int sensor_imx616_cis_get_seamless_trasition_mode(u32 mode)
{
	int pair_mode = -1;

	switch (mode) {
	case IMX616_MODE_QBCHDR_SEAMLESS_3264x1836_30FPS:
		pair_mode = IMX616_MODE_TRANSITION_2X2BIN_to_QBCHDR;
		break;
	case IMX616_MODE_2X2BIN_SEAMLESS_3264x1836_30FPS:
		pair_mode = IMX616_MODE_TRANSITION_QBCHDR_to_2X2BIN;
		break;
	default:
		err("invalid mode %d", mode);
		break;
	}

	if (pair_mode >= 0)
		info("[imx616] seamless trasition mode %d\n", mode);

	return pair_mode;
}
#endif

int sensor_imx616_cis_mode_change(struct v4l2_subdev *subdev, u32 mode)
{
	int ret = 0;
	struct is_cis *cis = sensor_cis_get_cis(subdev);
	const struct sensor_cis_mode_info *mode_info;

	u16 aehist_linear_threth[] = { VAL(AEHIST_LINEAR_LO_THRETH),
				VAL(AEHIST_LINEAR_UP_THRETH) };
	u16 aehist_log_threth[] = { VAL(AEHIST_LOG_LO_THRETH),
				VAL(AEHIST_LOG_UP_THRETH) };

	if (mode > cis->sensor_info->mode_count) {
		err("invalid mode(%d)!!", mode);
		ret = -EINVAL;
		goto p_err;
	}

	cis->mipi_clock_index_cur = CAM_MIPI_NOT_INITIALIZED;

	info("[%s] mode=%d, mode change setting start\n", __func__, mode);
	mode_info = cis->sensor_info->mode_infos[mode];

#if IS_ENABLED(CIS_CALIBRATION)
	if (sensor_imx616_cal_write_flag == false && mode_info->remosaic_mode) {
		sensor_imx616_cal_write_flag = true;
		ret = sensor_imx616_cis_set_cal(subdev);
		CHECK_ERR_GOTO(ret < 0, p_err, "sensor_imx616_cis_set_cal fail!! (%d)", ret);
	}
#endif

	ret = sensor_cis_write_registers_locked(subdev, mode_info->setfile);
	if (ret < 0) {
		err("mode(%d) setfile fail!!", mode);
		goto p_err;
	}

	cis->cis_data->sens_config_index_pre = mode;
	cis->cis_data->remosaic_mode = mode_info->remosaic_mode;

	/* Set AEHIST manual */
	IXC_MUTEX_LOCK(cis->ixc_lock);
	if (IS_3HDR_MODE(cis)) {
		info("[imx616] set AEHIST manual\n");
		ret = cis->ixc_ops->write8(cis->client, REG(AEHIST_LN_AUTO_THRETH), 0x0);
		CHECK_GOTO(ret < 0, p_i2c_err);
		ret = cis->ixc_ops->write16_array(cis->client, REG(AEHIST_LN_THRETH_START),
					aehist_linear_threth, ARRAY_SIZE(aehist_linear_threth));
		CHECK_ERR_GOTO(ret < 0, p_i2c_err, "failed to set linear");

		ret = cis->ixc_ops->write8(cis->client, REG(AEHIST_LOG_AUTO_THRETH), 0x0);
		CHECK_GOTO(ret < 0, p_i2c_err);
		ret = cis->ixc_ops->write16_array(cis->client, REG(AEHIST_LOG_THRETH_START),
					aehist_log_threth, ARRAY_SIZE(aehist_log_threth));
		CHECK_ERR_GOTO(ret < 0, p_i2c_err, "failed to set log");

		/* Addtional Settings */
		ret = sensor_cis_set_registers(subdev, sensor_imx616_exposure_nr_on,
					ARRAY_SIZE(sensor_imx616_exposure_nr_on));
		CHECK_ERR_GOTO(ret < 0, p_i2c_err, "set exp fail!!");
	} else {
		/* Addtional Settings */
		ret = sensor_cis_set_registers(subdev, sensor_imx616_exposure_nr_off,
					ARRAY_SIZE(sensor_imx616_exposure_nr_off));
		CHECK_ERR_GOTO(ret < 0, p_i2c_err, "set exp fail!!");
	}

#if SENSOR_IMX616_EBD_LINE_DISABLE
	/* To Do :  if the mode is needed EBD, add the condition with mode. */
	ret = sensor_imx616_set_ebd(cis);
	if (ret < 0) {
		err("sensor_imx616_set_registers fail!!");
		goto p_i2c_err;
	}
#endif

	info("[%s] X\n", __func__);

p_i2c_err:
	IXC_MUTEX_UNLOCK(cis->ixc_lock);

p_err:
	return ret;
}

#ifdef SUPPORT_SENSOR_SEAMLESS_3HDR
int sensor_imx616_cis_mode_change_seamless(struct v4l2_subdev *subdev, u32 mode)
{
	int ret = 0;
	int transition_mode = -1;
	struct is_cis *cis = NULL;
	struct i2c_client *client = NULL;

	FIMC_BUG(!subdev);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);
	FIMC_BUG(!cis);
	FIMC_BUG(!cis->cis_data);

	client = cis->client;
	CHECK_ERR_RET(!client, -EINVAL, "client is NULL");

	if ( !IS_3HDR(cis, mode) || mode > sensor_imx616_max_setfile_num) {
		err("invalid mode(%d)!!", mode);
		return -EINVAL;
	}

	/*sensor_imx616_set_integration_max_margin(mode, cis->cis_data);
	sensor_imx616_set_integration_min(mode, cis->cis_data); */

	info("[%s] mode=%d, seamless mode change start\n", __func__, mode);

	IXC_MUTEX_LOCK(cis->ixc_lock);

	/* Transition to 3HDR or seamless-tetra mode */
	if (IS_3HDR_SEAMLESS(cis, mode)) {
		/* info("%s: 0x3020, 0x01\n", __func__);*/
		ret = cis->ixc_ops->write8(client, 0x3020, 0x01);
		CHECK_ERR_GOTO(ret < 0, p_err_i2c, "transition cmd fail!!");
	} else {
		/* info("%s: 0x3020, 0x00\n", __func__);*/
		ret = cis->ixc_ops->write8(client, 0x3020, 0x00);
		CHECK_ERR_GOTO(ret < 0, p_err_i2c, "transition cmd fail!!");
	}

	transition_mode = sensor_imx616_cis_get_seamless_trasition_mode(mode);
	CHECK_ERR_GOTO(transition_mode < 0, p_err_i2c, "get transition mode fail!!");

	ret = sensor_cis_set_registers(subdev, sensor_imx616_setfiles[transition_mode],
					sensor_imx616_setfile_sizes[transition_mode]);
	CHECK_ERR_GOTO(ret < 0, p_err_i2c, "seamless transition settings fail!!");

#if SENSOR_IMX616_EBD_LINE_DISABLE
	/* To Do :  if the mode is needed EBD, add the condition with mode. */
	ret = sensor_imx616_set_ebd(cis);
	if (ret < 0) {
		err("sensor_imx616_set_registers fail!!");
		goto p_err_i2c;
	}
#endif

	dbg_sensor(1, "[%s] mode changed(%d)\n", __func__, mode);

p_err_i2c:
	IXC_MUTEX_UNLOCK(cis->ixc_lock);

	return ret;
}

int sensor_imx616_cis_mode_change_wrap(struct v4l2_subdev *subdev, u32 mode)
{
	int ret = 0;
	struct is_cis *cis = NULL;

	FIMC_BUG(!subdev);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);
	FIMC_BUG(!cis);
	FIMC_BUG(!cis->cis_data);

	if (cis->cis_data->stream_on) {
		if (!IS_3HDR(cis, mode)) {
			err("not allowed to change mode(%d) during stream on", mode);
			return -EPERM;
		}

		ret = sensor_imx616_cis_mode_change_seamless(subdev, mode);
	} else {
		ret = sensor_imx616_cis_mode_change(subdev, mode);
	}

	return ret;
}
#endif

int sensor_imx616_cis_stream_on(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_cis *cis = sensor_cis_get_cis(subdev);
	struct is_device_sensor_peri *sensor_peri = NULL;
	struct is_device_sensor *device;
	ktime_t st = ktime_get();

	sensor_peri = container_of(cis, struct is_device_sensor_peri, cis);
	WARN_ON(!sensor_peri);

	device = (struct is_device_sensor *)v4l2_get_subdev_hostdata(subdev);
	WARN_ON(!device);

	dbg_sensor(1, "[MOD:D:%d] %s\n", cis->id, __func__);

	is_vendor_set_mipi_clock(device);

	IXC_MUTEX_LOCK(cis->ixc_lock);

#ifdef SUPPORT_SENSOR_SEAMLESS_3HDR
	if (IS_3HDR_MODE(cis)) {
		if (IS_3HDR_SEAMLESS_MODE(cis)) {
			/* info("%s: 0x3020, 0x01\n", __func__); */
			ret = cis->ixc_ops->write8(cis->client, 0x3020, 0x01);
		} else {
			/* info("%s: 0x3020, 0x00\n", __func__); */
			ret = cis->ixc_ops->write8(cis->client, 0x3020, 0x00);
		}

		ret |= cis->ixc_ops->write8(cis->client, 0x3021, 0x01);
		if (ret)
			err("seamless 3hdr enable fail!!");
	}
#endif
	/* Sensor stream on */
	info("%s\n", __func__);
	cis->ixc_ops->write8(cis->client, 0x0100, 0x01);

	IXC_MUTEX_UNLOCK(cis->ixc_lock);

	cis->cis_data->stream_on = true;

	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %lldus", __func__, PABLO_KTIME_US_DELTA_NOW(st));

	return ret;
}

int sensor_imx616_cis_stream_off(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_cis *cis = sensor_cis_get_cis(subdev);
	u8 frame_count = 0;
	ktime_t st = ktime_get();

	dbg_sensor(1, "[MOD:D:%d] %s\n", cis->id, __func__);

	IXC_MUTEX_LOCK(cis->ixc_lock);
#ifdef SUPPORT_SENSOR_SEAMLESS_3HDR
	if (IS_3HDR_MODE(cis)) {
		ret = cis->ixc_ops->write8(cis->client, 0x3020, 0x00);
		ret |= cis->ixc_ops->write8(cis->client, 0x3021, 0x00);
		if (ret)
			err("seamless 3hdr off fail!!");
	}
#endif
	/* stream off */
	cis->ixc_ops->read8(cis->client, 0x0005, &frame_count);
	ret = cis->ixc_ops->write8(cis->client, 0x0100, 0x00);
	IXC_MUTEX_UNLOCK(cis->ixc_lock);

	info("%s done frame_count(%d)\n", __func__, frame_count);

	cis->cis_data->stream_on = false;

	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %lldus", __func__, PABLO_KTIME_US_DELTA_NOW(st));

	return ret;
}

int sensor_imx616_cis_set_wb_gain(struct v4l2_subdev *subdev, struct wb_gains wb_gains)
{
	int ret = 0;
	int mode = 0;
	struct is_cis *cis = sensor_cis_get_cis(subdev);
	const struct sensor_cis_mode_info *mode_info;
	u16 abs_gains[4] = {0, };	//[0]=gr, [1]=r, [2]=b, [3]=gb
	u32 div = 0;
	ktime_t st = ktime_get();

	if (!cis->use_wb_gain)
		return ret;

	mode = cis->cis_data->sens_config_index_cur;
	mode_info = cis->sensor_info->mode_infos[mode];

	if (!mode_info->wb_gain_support)
		return ret;

	if (wb_gains.gr != wb_gains.gb) {
		err("gr, gb not euqal"); /* check DDK layer */
		return -EINVAL;
	}

	if (wb_gains.gr == 1024)
		div = 4;
	else if (wb_gains.gr == 2048)
		div = 8;
	else {
		err("invalid gr,gb %d", wb_gains.gr); /* check DDK layer */
		return -EINVAL;
	}

	dbg_sensor(1, "[SEN:%d]%s:DDK vlaue: wb_gain_gr(%d), wb_gain_r(%d), wb_gain_b(%d), wb_gain_gb(%d)\n",
		cis->id, __func__, wb_gains.gr, wb_gains.r, wb_gains.b, wb_gains.gb);

	abs_gains[0] = (u16)((wb_gains.gr / div) & 0xFFFF);
	abs_gains[1] = (u16)((wb_gains.r / div) & 0xFFFF);
	abs_gains[2] = (u16)((wb_gains.b / div) & 0xFFFF);
	abs_gains[3] = (u16)((wb_gains.gb / div) & 0xFFFF);

	dbg_sensor(1, "[SEN:%d]%s, abs_gain_gr(0x%4X), abs_gain_r(0x%4X), abs_gain_b(0x%4X), abs_gain_gb(0x%4X)\n",
		cis->id, __func__, abs_gains[0], abs_gains[1], abs_gains[2], abs_gains[3]);

	IXC_MUTEX_LOCK(cis->ixc_lock);
	ret = cis->ixc_ops->write16_array(cis->client, IMX616_ABS_GAIN_GR_SET_ADDR, abs_gains, 4);
	IXC_MUTEX_UNLOCK(cis->ixc_lock);

	if (ret < 0)
		err("write16_array fail!!");

	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %lldus", __func__, PABLO_KTIME_US_DELTA_NOW(st));

	return ret;
}

#ifdef SUPPORT_SENSOR_SEAMLESS_3HDR
/**
 * sensor_imx616_cis_set_3hdr_flk_roi
 * : set flicker roi
 */
int sensor_imx616_cis_set_3hdr_flk_roi(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_cis *cis;
	struct i2c_client *client;
	u16 roi[4];
	ktime_t st = ktime_get();

	BUG_ON(!subdev);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);

	BUG_ON(!cis);
	BUG_ON(!cis->cis_data);

	client = cis->client;
	CHECK_ERR_RET(!client, -EINVAL, "client is NULL");
	CHECK_ERR_RET(!IS_3HDR_MODE(cis), 0, "can not set in none-3hdr");

	roi[0] = 0;
	roi[1] = 0;
	roi[2] = cis->cis_data->cur_width / NR_FLKER_BLK_W;
	roi[3] = cis->cis_data->cur_height / NR_FLKER_BLK_H;
	info("%s: (%d, %d, %d, %d) -> (%d, %d, %d, %d)", __func__,
		roi[0], roi[1], cis->cis_data->cur_width, cis->cis_data->cur_height,
		roi[0], roi[1], roi[2], roi[3]);

	IXC_MUTEX_LOCK(cis->ixc_lock);

	ret = cis->ixc_ops->write16_array(client, REG(FLK_AREA), roi, ARRAY_SIZE(roi));
	CHECK_ERR_GOTO(ret < 0, p_err_i2c, "failed to set flk_roi reg");

	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %lldus", __func__, PABLO_KTIME_US_DELTA_NOW(st));

p_err_i2c:
	IXC_MUTEX_UNLOCK(cis->ixc_lock);

	return ret;
}

int sensor_imx616_cis_set_3hdr_roi(struct v4l2_subdev *subdev, struct roi_setting_t rois)
{
	int i, ret = 0;
	struct is_cis *cis;
	struct i2c_client *client;
	u16 roi[4];
	u16 addrs[NR_ROI_AREAS] = {REG(AEHIST1), REG(AEHIST2)};
	u32 width, height;
	ktime_t st = ktime_get();

	BUG_ON(!subdev);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);

	BUG_ON(!cis);
	BUG_ON(!cis->cis_data);
	FIMC_BUG(NR_ROI_AREAS > ARRAY_SIZE(rois.roi_start_x));

	client = cis->client;
	CHECK_ERR_RET(!client, -EINVAL, "client is NULL");
	CHECK_ERR_RET(!IS_3HDR_MODE(cis), 0, "can not set in none-3hdr");

	width = cis->cis_data->cur_width;
	height = cis->cis_data->cur_height;

	for (i = 0; i < NR_ROI_AREAS; i++) {
		dbg_sensor(1, "%s: [MOD:%d] roi_control[%d] (start_x:%d, start_y:%d, end_x:%d, end_y:%d)\n",
			__func__, cis->id, i, rois.roi_start_x[i], rois.roi_start_y[i],
					rois.roi_end_x[i], rois.roi_end_y[i]);
		ASSERT(rois.roi_start_x[i] < width, "%d:start_x %d < %d", i, rois.roi_start_x[i], width);
		ASSERT(rois.roi_start_y[i] < height, "%d:start_y %d < %d", i, rois.roi_start_y[i], height);
		ASSERT(rois.roi_end_x[i] > 0 && rois.roi_end_x[i] <= width,
			"%d:end_x %d / w %d", i, rois.roi_end_x[i], width);
		ASSERT(rois.roi_end_y[i] > 0 && rois.roi_end_y[i] <= height,
			"%d:end_y %d / h %d", i, rois.roi_end_y[i], height);
		ASSERT(rois.roi_start_x[i] + rois.roi_end_x[i] <= width,
			"%d: %d + %d <= w%d", i, rois.roi_start_x[i], rois.roi_end_x[i], width);
		ASSERT(rois.roi_start_y[i] + rois.roi_end_y[i] <= height,
			"%d: %d + %d <= h%d", i, rois.roi_start_y[i], rois.roi_end_y[i], height);
	}

	IXC_MUTEX_LOCK(cis->ixc_lock);
	for (i = 0; i < NR_ROI_AREAS; i++) {
		roi[0] = rois.roi_start_x[i] >= width ? width - 1 : rois.roi_start_x[i];
		roi[1] = rois.roi_start_y[i] >= height ? height - 1 : rois.roi_start_y[i];
		roi[2] = rois.roi_end_x[i] > width ? width : rois.roi_end_x[i];
		roi[3] = rois.roi_end_y[i] > height ? height : rois.roi_end_y[i];
		/*info("%s: set [%d](%d, %d, %d, %d)\n", __func__, i, roi[0], roi[1], roi[2], roi[3]);*/

		ret = cis->ixc_ops->write16_array(client, addrs[i], roi, ARRAY_SIZE(roi));
		CHECK_ERR_GOTO(ret < 0, p_err, "failed to set roi reg");
	}

	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %lldus", __func__, PABLO_KTIME_US_DELTA_NOW(st));

p_err:
	IXC_MUTEX_UNLOCK(cis->ixc_lock);

	return ret;
}

int sensor_imx616_cis_set_roi_stat(struct v4l2_subdev *subdev, struct roi_setting_t roi_control)
{
	return sensor_imx616_cis_set_3hdr_roi(subdev, roi_control);
}

int sensor_imx616_cis_set_3hdr_stat(struct v4l2_subdev *subdev, bool streaming, void *data)
{
	int ret = 0;
	struct sensor_imx_3hdr_stat_control_per_frame *per_frame_stat = NULL;
	struct sensor_imx_3hdr_stat_control_mode_change *mode_change_stat = NULL;
	struct roi_setting_t *roi = NULL;
	ktime_t st = ktime_get();

	FIMC_BUG(!subdev);
	FIMC_BUG(!data);

	if (streaming) {
		per_frame_stat = (struct sensor_imx_3hdr_stat_control_per_frame *)data;
	} else {
		mode_change_stat = (struct sensor_imx_3hdr_stat_control_mode_change *)data;
		roi = &mode_change_stat->y_sum_roi;
		info("[imx616] 3hdr_stat: roi (%d, %d, %d, %d), (%d, %d, %d, %d)\n",
			roi->roi_start_x[0], roi->roi_start_y[0], roi->roi_end_x[0], roi->roi_end_y[0],
			roi->roi_start_x[1], roi->roi_start_y[1], roi->roi_end_x[1], roi->roi_end_y[1]);
		ret = sensor_imx616_cis_set_3hdr_roi(subdev, mode_change_stat->y_sum_roi);
		CHECK_ERR_RET(ret < 0 , ret, "failed to 3hdr_roi");

		ret = sensor_imx616_cis_set_3hdr_flk_roi(subdev);
		CHECK_ERR_RET(ret < 0 , ret, "failed to flk_roi");
	}

	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %lldus", __func__, PABLO_KTIME_US_DELTA_NOW(st));

	return ret;
}
#endif /* SUPPORT_SENSOR_SEAMLESS_3HDR */

void sensor_imx616_cis_check_wdr_mode(struct v4l2_subdev *subdev, u32 mode_idx)
{
	struct is_cis *cis = sensor_cis_get_cis(subdev);

	/* check wdr mode */
	if (IS_3HDR(cis, mode_idx))
		cis->cis_data->is_data.wdr_enable = true;
	else
		cis->cis_data->is_data.wdr_enable = false;

	dbg_sensor(1, "[%s] wdr_enable: %d\n", __func__,
				cis->cis_data->is_data.wdr_enable);
}

/**
 * sensor_imx616_cis_init_3hdr_lsc_table: set LSC table on init
 */
int sensor_imx616_cis_init_3hdr_lsc_table(struct v4l2_subdev *subdev, void *data)
{
	int i, ret = 0;
	struct is_cis *cis = NULL;
	struct i2c_client *client = NULL;
	struct sensor_imx_3hdr_lsc_table_init *lsc = NULL;
	/* const u16 ram_addr[] = {REG(KNOT_TAB_GR), REG(KNOT_TAB_GB)}; */
	u32 table_len = RAMTABLE_LEN * 2;
	u16 addr, val;
	ktime_t st = ktime_get();

	FIMC_BUG(!subdev);
	FIMC_BUG(!data);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);

	BUG_ON(!cis);
	BUG_ON(!cis->cis_data);

	client = cis->client;
	CHECK_ERR_RET(!client, -EINVAL, "client is NULL");

	if (!IS_3HDR_MODE(cis) || IS_3HDR_SEAMLESS_MODE(cis)) {
		err("can not set in none-3hdr");
		return 0;
	}

	lsc = (struct sensor_imx_3hdr_lsc_table_init *)data;
	CHECK_ERR_RET(ARRAY_SIZE(lsc->ram_table) < table_len,
			0, "ramtable size smaller than %d", table_len);

	info("[imx616] set LSC table\n");

	for (i = 0; i < table_len; i++) {
		if (lsc->ram_table[i] > VAL(LSC_MAX_GAIN)) {
			err("ramtable[%d]: invalid gain %#X", i, lsc->ram_table[i]);
			lsc->ram_table[i] = VAL(LSC_MAX_GAIN);
		}
	}

	IXC_MUTEX_LOCK(cis->ixc_lock);
	/* In 3hdr and seamless tetra mode, LSC enabled by setfile */

	/* Write RATE, RATE_Y */
	val = (VAL(LSC_APP_RATE) << 8) | VAL(LSC_APP_RATE_Y);
	ret = cis->ixc_ops->write16(client, REG(LSC_APP_RATE), val);
	CHECK_GOTO(ret < 0, p_i2c_err);

	/* Write MODE, TABLE_SEL */
	val = (VAL(CALC_MODE_MANUAL) << 8) | VAL(TABLE_SEL_1);
	ret = cis->ixc_ops->write16(client, REG(CALC_MODE), val);
	CHECK_GOTO(ret < 0, p_i2c_err);

	for (i = 0; i < table_len; i++) {
		addr = REG(KNOT_TAB_GR) + (i * 2);
		/* info("[RAMTABLE] %03d: %#X, %#X\n", i, addr, lsc->ram_table[i]); */
		cis->ixc_ops->write16(client, addr, lsc->ram_table[i]);
		CHECK_ERR_GOTO(ret < 0, p_i2c_err, "failed to write table");
	}

	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %lldus", __func__, PABLO_KTIME_US_DELTA_NOW(st));

p_i2c_err:
	IXC_MUTEX_UNLOCK(cis->ixc_lock);

	return ret;
}

int sensor_imx616_cis_set_3hdr_tone(struct v4l2_subdev *subdev, struct sensor_imx_3hdr_tone_control tc)
{
	int i, ret = 0;
	struct is_cis *cis;
	struct i2c_client *client;
	cis_shared_data *cis_data;
	u8 nr_transit_frames[4] = {0, };
	u16 blend2_addr[5] = { REG(BLD2_TC_RATIO_1), REG(BLD2_TC_RATIO_2),
				REG(BLD2_TC_RATIO_3), REG(BLD2_TC_RATIO_4),
				REG(BLD2_TC_RATIO_5) };
	u16 blend1_ratio, blend2_ratio, blend3_ratio;
	u16 hdrtc_ratio[5] = {0, };
	u8 blend3_ratio_1_5[5] = {0, };
	ktime_t st = ktime_get();

	BUG_ON(!subdev);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);

	BUG_ON(!cis);
	BUG_ON(!cis->cis_data);

	cis_data = cis->cis_data;

	client = cis->client;
	CHECK_ERR_RET(!client, -EINVAL, "client is NULL");
	CHECK_ERR_RET(!IS_3HDR_MODE(cis), 0, "can not set in none-3hdr");

	dbg_sensor(1, "[MOD:D:%d] %s(vsync cnt %d): gmtc2 on(%d), gmtc2 ratio %#x, "
		"(%#x, %#x) - (%#x, %#x), manual tc ratio %#x, ltc ratio %#x, "
		"hdr_tc1 %#x, hdr_tc2 %#x, hdr_tc3 %#x, hdr_tc4 %#x, hdr_tc5 %#x\n",
		cis->id, __func__, cis_data->sen_vsync_count, tc.gmt_tc2_enable, tc.gmt_tc2_ratio,
		tc.manual21_frame_p1, tc.manual21_frame_p2, tc.manual12_frame_p1, tc.manual12_frame_p2,
		tc.manual_tc_ratio, tc.ltc_ratio, tc.hdr_tc_ratio_1, tc.hdr_tc_ratio_2, tc.hdr_tc_ratio_3,
		tc.hdr_tc_ratio_4, tc.hdr_tc_ratio_5);

	IXC_MUTEX_LOCK(cis->ixc_lock);

	/* Blend1 */
	ret = cis->ixc_ops->write8(client, REG(BLD1_GMTC2_EN), tc.gmt_tc2_enable ? 0x01 : 0x00);
	CHECK_GOTO(ret < 0, p_i2c_err);

	blend1_ratio = tc.gmt_tc2_ratio > 0x10 ? 0x10 : tc.gmt_tc2_ratio;
	ret = cis->ixc_ops->write8(client, REG(BLD1_GMTC2_RATIO), blend1_ratio);
	CHECK_GOTO(ret < 0, p_i2c_err);

	nr_transit_frames[0] = tc.manual21_frame_p1 & 0x7F;
	nr_transit_frames[1] = tc.manual21_frame_p2 & 0x7F;
	nr_transit_frames[2] = tc.manual12_frame_p1 & 0x7F;
	nr_transit_frames[3] = tc.manual12_frame_p2 & 0x7F;

	ret = cis->ixc_ops->write8_array(client, REG(BLD1_GMTC_NR_TRANSIT_FRM),
					nr_transit_frames, ARRAY_SIZE(nr_transit_frames));
	CHECK_GOTO(ret < 0, p_i2c_err);

	/* Blend2 */
	blend2_ratio = tc.manual_tc_ratio > 0x10 ? 0x10 : tc.manual_tc_ratio;
	for (i = 0; i < ARRAY_SIZE(blend2_addr); i++) {
		ret = cis->ixc_ops->write8(client, blend2_addr[i], blend2_ratio);
		CHECK_GOTO(ret < 0, p_i2c_err);
	}

	/* Blend3 */
	blend3_ratio = tc.ltc_ratio > 0x20 ? 0x20 : tc.ltc_ratio;
	for (i = 0; i < ARRAY_SIZE(blend3_ratio_1_5); i++) {
		blend3_ratio_1_5[i] = blend3_ratio;
	}

	ret = cis->ixc_ops->write8_array(client, REG(BLD3_LTC_RATIO_START),
					blend3_ratio_1_5, ARRAY_SIZE(blend3_ratio_1_5));
	ret |= cis->ixc_ops->write8(client, REG(BLD3_LTC_RATIO_6), blend3_ratio);
	CHECK_GOTO(ret < 0, p_i2c_err);

	if (tc.hdr_tc_ratio_4 != tc.hdr_tc_ratio_5) {
		/* DDK needs to be checked */
		err("not same: hdr_tc4 %d, hdr_tc5 %d", tc.hdr_tc_ratio_4, tc.hdr_tc_ratio_5);
	}

	/* Blend4 */
	i = 0;
	hdrtc_ratio[i++] = tc.hdr_tc_ratio_1 > 0x100 ? 0x100 : tc.hdr_tc_ratio_1;
	hdrtc_ratio[i++] = tc.hdr_tc_ratio_2 > 0x100 ? 0x100 : tc.hdr_tc_ratio_2;
	hdrtc_ratio[i++] = tc.hdr_tc_ratio_3 > 0x100 ? 0x100 : tc.hdr_tc_ratio_3;
	hdrtc_ratio[i++] = tc.hdr_tc_ratio_4 > 0x100 ? 0x100 : tc.hdr_tc_ratio_4;
	hdrtc_ratio[i] = tc.hdr_tc_ratio_5 > 0x100 ? 0x100 : tc.hdr_tc_ratio_5;

	/*
	i = 0;
	hdrtc_addr[i++] = REG(BLD4_HDR_TC_RATIO1_UP);
	hdrtc_addr[i++] = REG(BLD4_HDR_TC_RATIO2_UP);
	hdrtc_addr[i++] = REG(BLD4_HDR_TC_RATIO3_UP);
	hdrtc_addr[i++] = REG(BLD4_HDR_TC_RATIO4_UP);
	hdrtc_addr[i] = REG(BLD4_HDR_TC_RATIO5_UP); */

	ret = cis->ixc_ops->write16_array(client, REG(BLD4_HDR_TC_RATIO_START),
					hdrtc_ratio, ARRAY_SIZE(hdrtc_ratio));
	CHECK_GOTO(ret < 0, p_i2c_err);

	ASSERT((tc.gmt_tc2_ratio == blend1_ratio) && (tc.manual_tc_ratio == blend2_ratio) && (tc.ltc_ratio == blend3_ratio)
		&& (tc.manual21_frame_p1 == nr_transit_frames[0]) && (tc.manual21_frame_p2 == nr_transit_frames[1])
		&& (tc.manual12_frame_p1 == nr_transit_frames[2]) && (tc.manual12_frame_p2 == nr_transit_frames[3])
		&& (tc.hdr_tc_ratio_1 == hdrtc_ratio[0]) && (tc.hdr_tc_ratio_2 == hdrtc_ratio[1]) && (tc.hdr_tc_ratio_3 == hdrtc_ratio[2])
		&& (tc.hdr_tc_ratio_4 == hdrtc_ratio[3]) && (tc.hdr_tc_ratio_5 == hdrtc_ratio[4]),
		"changed tc values were set!");

	dbg_sensor(1, "[MOD:D:%d] %s(vsync cnt %d): gmtc2 on(%d), gmtc2 ratio %#x, "
		"(%#x, %#x) - (%#x, %#x), manual tc ratio %#x, ltc ratio %#x, "
		"hdr_tc1 %#x, hdr_tc2 %#x, hdr_tc3 %#x, hdr_tc4 %#x, hdr_tc5 %#x\n",
		cis->id, __func__, cis_data->sen_vsync_count, tc.gmt_tc2_enable, blend1_ratio,
		nr_transit_frames[0], nr_transit_frames[1], nr_transit_frames[2], nr_transit_frames[3],
		blend2_ratio, blend3_ratio, hdrtc_ratio[0], hdrtc_ratio[1], hdrtc_ratio[2],
		hdrtc_ratio[3], hdrtc_ratio[4]);

	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %lldus", __func__, PABLO_KTIME_US_DELTA_NOW(st));

p_i2c_err:
	IXC_MUTEX_UNLOCK(cis->ixc_lock);

	return ret;
}

int sensor_imx616_cis_set_3hdr_ev(struct v4l2_subdev *subdev, struct sensor_imx_3hdr_ev_control ev_control)
{
	int ret = 0;
	struct is_cis *cis;
	struct i2c_client *client;
	cis_shared_data *cis_data;
	int pgain = 0, ngain = 0;
	ktime_t st = ktime_get();

	BUG_ON(!subdev);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);

	BUG_ON(!cis);
	BUG_ON(!cis->cis_data);

	cis_data = cis->cis_data;

	client = cis->client;
	CHECK_ERR_RET(!client, -EINVAL, "client is NULL");
	CHECK_ERR_RET(!IS_3HDR_MODE(cis), 0, "can not set in none-3hdr");

	dbg_sensor(1, "[MOD:D:%d] %s(vsync cnt %d): evc_pgain %#x, evc_ngain %#x\n",
		cis->id, __func__, cis_data->sen_vsync_count, ev_control.evc_pgain, ev_control.evc_ngain);

	if (ev_control.evc_pgain && ev_control.evc_ngain) {
		err("invalid ev control. pgain %d, ngain %d", ev_control.evc_pgain, ev_control.evc_ngain);
		return -EINVAL;
	}

	pgain = ev_control.evc_pgain > 0x10 ? 0x10: ev_control.evc_pgain;
	ngain = ev_control.evc_ngain > 0x10 ? 0x10: ev_control.evc_ngain;

	IXC_MUTEX_LOCK(cis->ixc_lock);
	ret = cis->ixc_ops->write8(client, REG(EVC_PGAIN), pgain);
	CHECK_GOTO(ret < 0, p_i2c_err);

	ret = cis->ixc_ops->write8(client, REG(EVC_NGAIN), ngain);
	CHECK_GOTO(ret < 0, p_i2c_err);

	if (pgain != ev_control.evc_pgain || ngain != ev_control.evc_ngain) {
		err("pgain %d -> %d, ngain %d -> %d", ev_control.evc_pgain, pgain,
						ev_control.evc_ngain, ngain);
	}

	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %lldus", __func__, PABLO_KTIME_US_DELTA_NOW(st));

p_i2c_err:
	IXC_MUTEX_UNLOCK(cis->ixc_lock);

	return ret;
}

int sensor_imx616_cis_wait_streamon(struct v4l2_subdev *subdev)
{
	int ret = 0;
	int ret_err = 0;
	struct is_cis *cis;
	cis_shared_data *cis_data;
	u32 wait_cnt = 0, time_out_cnt = 250;
	u8 sensor_fcount = 0;
	u32 i2c_fail_cnt = 0;

	WARN_ON(!subdev);

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

	if (unlikely(!cis->client)) {
		err("client is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	if (cis_data->cur_frame_us_time > 300000 && cis_data->cur_frame_us_time < 2000000)
		time_out_cnt = (cis_data->cur_frame_us_time / CIS_STREAM_ON_WAIT_TIME) + 100; // for Hyperlapse night mode

	if (cis_data->dual_slave == true || cis_data->remosaic_mode == true)
		time_out_cnt = time_out_cnt * 6;

	if (cis_data->remosaic_mode == true
		&& (cis_data->cur_frame_us_time > 300000 && cis_data->cur_frame_us_time < 31000000))
		time_out_cnt = (cis_data->cur_frame_us_time / CIS_STREAM_ON_WAIT_TIME) + 100; // for remosaic LTE

	IXC_MUTEX_LOCK(cis->ixc_lock);
	ret = cis->ixc_ops->read8(cis->client, 0x0005, &sensor_fcount);
	IXC_MUTEX_UNLOCK(cis->ixc_lock);
	if (ret < 0)
		err("i2c transfer fail addr(%x), val(%x), ret = %d\n", 0x0005, sensor_fcount, ret);

	/*
	 * Read sensor frame counter (sensor_fcount address = 0x0005)
	 * stream on (0x00 ~ 0xFE), stream off (0xFF)
	 */
	while (sensor_fcount == 0xff) {
		usleep_range(CIS_STREAM_ON_WAIT_TIME, CIS_STREAM_ON_WAIT_TIME);
		wait_cnt++;

		IXC_MUTEX_LOCK(cis->ixc_lock);
		ret = cis->ixc_ops->read8(cis->client, 0x0005, &sensor_fcount);
		IXC_MUTEX_UNLOCK(cis->ixc_lock);
		if (ret < 0) {
			i2c_fail_cnt++;
			err("i2c transfer fail addr(%x), val(%x), try(%d), ret = %d\n",
				0x0005, sensor_fcount, i2c_fail_cnt, ret);
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
				cis->id, __func__, cis_data->cur_frame_us_time, wait_cnt, time_out_cnt);
			err("framecount(%x), dual_slave(%d), remosaic_mode(%d)",
				sensor_fcount, cis_data->dual_slave, cis_data->remosaic_mode);
			ret = -EINVAL;
			goto p_err;
		}

		info("[MOD:D:%d] %s, sensor_fcount(%d), (wait_limit(%d) < time_out(%d))\n",
				cis->id, __func__, sensor_fcount, wait_cnt, time_out_cnt);
	}
	usleep_range(CIS_STREAM_ON_WAIT_TIME, CIS_STREAM_ON_WAIT_TIME); //Add delay 2ms
	info("[MOD:D:%d] %s wait_stream_on done, sensor_fcount(%d), (wait_limit(%d) < time_out(%d))\n",
			cis->id, __func__, sensor_fcount, wait_cnt, time_out_cnt);

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

static struct is_cis_ops cis_ops_imx616 = {
	.cis_init = sensor_imx616_cis_init,
	.cis_init_state = sensor_imx616_cis_init_state,
	.cis_log_status = sensor_imx616_cis_log_status,
	.cis_group_param_hold = sensor_cis_set_group_param_hold,
	.cis_set_global_setting = sensor_imx616_cis_set_global_setting,
#ifdef SUPPORT_SENSOR_SEAMLESS_3HDR
	.cis_mode_change = sensor_imx616_cis_mode_change_wrap,
#else
	.cis_mode_change = sensor_imx616_cis_mode_change,
#endif
	.cis_stream_on = sensor_imx616_cis_stream_on,
	.cis_stream_off = sensor_imx616_cis_stream_off,
	.cis_wait_streamon = sensor_imx616_cis_wait_streamon,
	.cis_wait_streamoff = sensor_cis_wait_streamoff,
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
	.cis_calc_again_code = sensor_imx616_cis_calc_again_code,
	.cis_calc_again_permile = sensor_imx616_cis_calc_again_permile,
	.cis_set_digital_gain = sensor_cis_set_digital_gain,
	.cis_get_digital_gain = sensor_cis_get_digital_gain,
	.cis_get_min_digital_gain = sensor_cis_get_min_digital_gain,
	.cis_get_max_digital_gain = sensor_cis_get_max_digital_gain,
	.cis_calc_dgain_code = sensor_cis_calc_dgain_code,
	.cis_calc_dgain_permile = sensor_cis_calc_dgain_permile,
	.cis_set_totalgain = sensor_cis_set_totalgain,
	.cis_compensate_gain_for_extremely_br = sensor_cis_compensate_gain_for_extremely_br,
	.cis_check_rev_on_init = sensor_cis_check_rev_on_init,
	.cis_set_initial_exposure = sensor_cis_set_initial_exposure,
	.cis_set_wb_gains = sensor_imx616_cis_set_wb_gain,
	.cis_check_wdr_mode = sensor_imx616_cis_check_wdr_mode,
	.cis_init_3hdr_lsc_table = sensor_imx616_cis_init_3hdr_lsc_table,
	.cis_set_tone_stat = sensor_imx616_cis_set_3hdr_tone,
	.cis_set_ev_stat = sensor_imx616_cis_set_3hdr_ev,
#ifdef SUPPORT_SENSOR_SEAMLESS_3HDR
	.cis_set_roi_stat = sensor_imx616_cis_set_roi_stat,
	.cis_set_3hdr_stat = sensor_imx616_cis_set_3hdr_stat,
#endif
};

int cis_imx616_probe_i2c(struct i2c_client *client,
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
	cis->cis_ops = &cis_ops_imx616;
	/* belows are depend on sensor cis. MUST check sensor spec */
	cis->bayer_order = OTF_INPUT_ORDER_BAYER_RG_GB;
	cis->use_wb_gain = true;
	cis->use_total_gain = true;
	cis->reg_addr = &sensor_imx616_reg_addr;

	cis->use_3hdr = of_property_read_bool(dnode, "use_3hdr");
	probe_info("%s use_3hdr(%d)\n", __func__, cis->use_3hdr);

	ret = of_property_read_string(dnode, "setfile", &setfile);
	if (ret) {
		err("setfile index read fail(%d), take default setfile!!", ret);
		setfile = "default";
	}

	if (strcmp(setfile, "default") == 0 || strcmp(setfile, "setA") == 0)
		probe_info("[%s] setfile_A mclk: 19.2Mhz\n", __func__);
	else
		err("setfile index out of bound, take default (setfile_A mclk: 26Mhz)");

	cis->sensor_info = &sensor_imx616_info_A;

	is_vendor_set_mipi_mode(cis);

	probe_info("%s done\n", __func__);

	return ret;
}

static const struct of_device_id sensor_cis_imx616_match[] = {
	{
		.compatible = "samsung,exynos-is-cis-imx616",
	},
	{},
};
MODULE_DEVICE_TABLE(of, sensor_cis_imx616_match);

static const struct i2c_device_id sensor_cis_imx616_idt[] = {
	{ SENSOR_NAME, 0 },
	{},
};

static struct i2c_driver sensor_cis_imx616_driver = {
	.probe	= cis_imx616_probe_i2c,
	.driver = {
		.name	= SENSOR_NAME,
		.owner	= THIS_MODULE,
		.of_match_table = sensor_cis_imx616_match,
		.suppress_bind_attrs = true,
	},
	.id_table = sensor_cis_imx616_idt
};

#ifdef MODULE
builtin_i2c_driver(sensor_cis_imx616_driver);
#else
static int __init sensor_cis_imx616_init(void)
{
	int ret;

	ret = i2c_add_driver(&sensor_cis_imx616_driver);
	if (ret)
		err("failed to add %s driver: %d\n",
			sensor_cis_imx616_driver.driver.name, ret);

	return ret;
}
late_initcall_sync(sensor_cis_imx616_init);
#endif

MODULE_LICENSE("GPL");
MODULE_SOFTDEP("pre: fimc-is");
