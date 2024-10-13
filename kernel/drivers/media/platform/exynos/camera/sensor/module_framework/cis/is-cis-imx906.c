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
#include "is-vendor.h"
#include "is-cis-imx906.h"
#include "is-cis-imx906-setA.h"

#include "is-helper-ixc.h"
#include "is-sec-define.h"

#include "interface/is-interface-library.h"
#include "is-interface-sensor.h"
#define SENSOR_NAME "IMX906"

/* temp change, remove once PDAF settings done */
/* #define PDAF_DISABLE */
#ifdef PDAF_DISABLE
const u16 sensor_imx906_pdaf_off[] = {
	0x3104, 0x00, 0x01,
};

const struct sensor_regs pdaf_off = SENSOR_REGS(sensor_imx906_pdaf_off);
#endif

void sensor_imx906_cis_set_mode_group(u32 mode)
{
	sensor_imx906_mode_groups[SENSOR_IMX906_MODE_NORMAL] = mode;
	sensor_imx906_mode_groups[SENSOR_IMX906_MODE_RMS_CROP] = MODE_GROUP_NONE;
	sensor_imx906_mode_groups[SENSOR_IMX906_MODE_DSG] = MODE_GROUP_NONE;
	sensor_imx906_mode_groups[SENSOR_IMX906_MODE_LN4] = MODE_GROUP_NONE;

	switch (mode) {
	case SENSOR_IMX906_4080X3060_30FPS_R12:
		sensor_imx906_mode_groups[SENSOR_IMX906_MODE_RMS_CROP] =
			SENSOR_IMX906_4080X3060_30FPS_REMOSAIC_CROP_R12;
		sensor_imx906_mode_groups[SENSOR_IMX906_MODE_LN4] =
			SENSOR_IMX906_4080X3060_24FPS_LN4_R12;
		break;
	case SENSOR_IMX906_4080X2296_30FPS_R12:
		sensor_imx906_mode_groups[SENSOR_IMX906_MODE_RMS_CROP] =
			SENSOR_IMX906_4080X2296_30FPS_REMOSAIC_CROP_R12;
		sensor_imx906_mode_groups[SENSOR_IMX906_MODE_LN4] =
			SENSOR_IMX906_4080X2296_30FPS_LN4_R12;
		break;
	case SENSOR_IMX906_4080X3060_30FPS_DSG_R12:
		sensor_imx906_mode_groups[SENSOR_IMX906_MODE_DSG] =
			SENSOR_IMX906_4080X3060_30FPS_DSG_R12;
		break;
	case SENSOR_IMX906_4080X2296_30FPS_DSG_R12:
		sensor_imx906_mode_groups[SENSOR_IMX906_MODE_DSG] =
			SENSOR_IMX906_4080X2296_30FPS_DSG_R12;
		sensor_imx906_mode_groups[SENSOR_IMX906_MODE_LN4] =
			SENSOR_IMX906_4080X2296_30FPS_LN4_R12;
		break;
	}

	info("[%s] normal(%d) rms_crop(%d) DCG(%d) LN4(%d)\n", __func__,
		sensor_imx906_mode_groups[SENSOR_IMX906_MODE_NORMAL],
		sensor_imx906_mode_groups[SENSOR_IMX906_MODE_RMS_CROP],
		sensor_imx906_mode_groups[SENSOR_IMX906_MODE_DSG],
		sensor_imx906_mode_groups[SENSOR_IMX906_MODE_LN4]);
}

#if IS_ENABLED(CIS_CALIBRATION)
int sensor_imx906_cis_set_cal(struct v4l2_subdev *subdev)
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

	cal_size = rom_info->rom_xtc_cal_data_size;
	start_addr = rom_info->rom_xtc_cal_data_start_addr;
	if (start_addr < 0 || cal_size <= 0) {
		err("Not available DT, skip set cal");
		return 0;
	}
 
	cal_buf = rom_info->buf;

	IXC_MUTEX_LOCK(cis->ixc_lock);
	ret = cis->ixc_ops->write8_sequential(cis->client, IMX906_QSC_ADDR, (u8 *)&cal_buf[start_addr], cal_size);
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
 *  [IMX906 Analog gain formular]
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

u32 sensor_imx906_cis_calc_again_code(u32 permille)
{
	return (16384*10 - (16384000*10 / permille))/10;
}

u32 sensor_imx906_cis_calc_again_permile(u32 code)
{
	return 16384000 / (16384 - code);
}

/* CIS OPS */
int sensor_imx906_cis_init(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_cis *cis = sensor_cis_get_cis(subdev);

	cis->cis_data->cur_width = cis->sensor_info->max_width;
	cis->cis_data->cur_height = cis->sensor_info->max_height;
	cis->cis_data->low_expo_start = 33000;
	cis->need_mode_change = false;

	cis->cis_data->sens_config_index_pre = SENSOR_IMX906_MODE_MAX;
	cis->cis_data->sens_config_index_cur = 0;
	CALL_CISOPS(cis, cis_data_calculation, subdev, cis->cis_data->sens_config_index_cur);

	return ret;
}

static const struct is_cis_log log_imx906[] = {
	{I2C_READ, 16, 0x0016, 0, "model_id"},
	{I2C_READ, 8, 0x0018, 0, "rev_number"},
	{I2C_READ, 8, 0x0005, 0, "frame_count"},
	{I2C_READ, 8, 0x0104, 0, "gph"},
	{I2C_READ, 16, 0x0100, 0, "mode_select"},
	{I2C_READ, 16, 0x0340, 0, "fll"},
	{I2C_READ, 16, 0x0342, 0, "llp"},
	{I2C_READ, 16, 0x0202, 0, "cit"},
	{I2C_READ, 16, 0x3160, 0, "shf"},
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

int sensor_imx906_cis_log_status(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_cis *cis = sensor_cis_get_cis(subdev);

	sensor_cis_log_status(cis, log_imx906, ARRAY_SIZE(log_imx906), (char *)__func__);

	return ret;
}

int sensor_imx906_cis_set_global_setting(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_cis *cis = sensor_cis_get_cis(subdev);
	struct sensor_imx906_private_data *priv = (struct sensor_imx906_private_data *)cis->sensor_info->priv;

	info("[%s] start\n", __func__);

	ret = sensor_cis_write_registers_locked(subdev, priv->global);
	if (ret < 0)
		err("global setting fail!!");

	info("[%s] done\n", __func__);

#if IS_ENABLED(CIS_CALIBRATION)
	sensor_imx906_cis_set_cal(subdev);
#endif

	return ret;
}

int sensor_imx906_cis_check_12bit(cis_shared_data *cis_data, u32 *next_mode)
{
	int ret = 0;

	if (sensor_imx906_mode_groups[SENSOR_IMX906_MODE_DSG] == MODE_GROUP_NONE) {
		ret = -1;
		goto EXIT;
	}

	switch (cis_data->cur_12bit_mode) {
	case SENSOR_12BIT_STATE_REAL_12BIT:
		*next_mode = sensor_imx906_mode_groups[SENSOR_IMX906_MODE_DSG];
		break;
	case SENSOR_12BIT_STATE_PSEUDO_12BIT:
	default:
		ret = -1;
		break;
	}
EXIT:
	return ret;
}

int sensor_imx906_cis_check_lownoise(cis_shared_data *cis_data, u32 *next_mode) {
	int ret = 0;
	u32 temp_mode = MODE_GROUP_NONE;

	if (sensor_imx906_mode_groups[SENSOR_IMX906_MODE_LN4] == MODE_GROUP_NONE) {
		ret = -1;
		goto EXIT;
	}

	switch (cis_data->cur_lownoise_mode) {
	case IS_CIS_LN4:
		temp_mode = sensor_imx906_mode_groups[SENSOR_IMX906_MODE_LN4];
		break;
	case IS_CIS_LN2:
	case IS_CIS_LNOFF:
	default:
		break;
	}

	if (temp_mode == MODE_GROUP_NONE) {
		ret = -1;
	}

	if (ret == 0) {
		*next_mode = temp_mode;
	}

EXIT:
	return ret;
}

int sensor_imx906_cis_check_cropped_remosaic(cis_shared_data *cis_data, unsigned int mode, unsigned int *next_mode)
{
	int ret = 0;
	u32 zoom_ratio = 0;

	if (sensor_imx906_mode_groups[SENSOR_IMX906_MODE_RMS_CROP] == MODE_GROUP_NONE)
		return -EINVAL;

	zoom_ratio = cis_data->cur_remosaic_zoom_ratio;

	if (zoom_ratio != IMX906_REMOSAIC_ZOOM_RATIO_X_2)
		return -EINVAL; //goto default

	*next_mode = sensor_imx906_mode_groups[SENSOR_IMX906_MODE_RMS_CROP];

	if (zoom_ratio == cis_data->pre_remosaic_zoom_ratio) {
		dbg_sensor(1, "%s : cur zoom ratio is same pre zoom ratio (%u)", __func__, zoom_ratio);
		return ret;
	}

	info("%s : zoom_ratio %u turn on cropped remosaic\n", __func__, zoom_ratio);

	return ret;
}

int sensor_imx906_cis_update_seamless_mode(struct v4l2_subdev *subdev)
{
	int ret = 0;
	unsigned int mode = 0;
	unsigned int next_mode = 0;
	cis_shared_data *cis_data;
	const struct sensor_cis_mode_info *next_mode_info;
	struct is_cis *cis = sensor_cis_get_cis(subdev);
	ktime_t st = ktime_get();

	cis_data = cis->cis_data;
	mode = cis_data->sens_config_index_cur;
	next_mode = mode;

	next_mode = sensor_imx906_mode_groups[SENSOR_IMX906_MODE_NORMAL];
	if (next_mode == MODE_GROUP_NONE) {
		err("mode group is none");
		return -EINVAL;
	}

	if (cis->cis_data->stream_on == false
		&& cis->cis_data->seamless_ctl_before_stream.mode == 0) {
		info("[%s] skip update seamless mode in stream off\n", __func__);
		return 0;
	}

	cis->cis_data->seamless_ctl_before_stream.mode = 0;

	sensor_imx906_cis_check_12bit(cis->cis_data, &next_mode);
	sensor_imx906_cis_check_lownoise(cis->cis_data, &next_mode);
	sensor_imx906_cis_check_cropped_remosaic(cis->cis_data, mode, &next_mode);

	if ((mode == next_mode) || (next_mode == MODE_GROUP_NONE))
		return ret;

	next_mode_info = cis->sensor_info->mode_infos[next_mode];
	if (next_mode_info->setfile_fcm.size == 0) {
		err("check the fcm setting for mode(%d)", next_mode);
		return ret;
	}

	dbg_sensor(1, "[%s][%d] mode(%d) changing to next_mode(%d)",
		__func__, cis->cis_data->sen_vsync_count, mode, next_mode);
	IXC_MUTEX_LOCK(cis->ixc_lock);

	ret |= sensor_cis_write_registers(subdev, next_mode_info->setfile_fcm);
	IXC_MUTEX_UNLOCK(cis->ixc_lock);

	cis->cis_data->sens_config_index_pre = cis->cis_data->sens_config_index_cur;
	cis->cis_data->sens_config_index_cur = next_mode;

	CALL_CISOPS(cis, cis_data_calculation, subdev, next_mode);

	cis->cis_data->pre_12bit_mode = cis->cis_data->cur_12bit_mode;
	cis->cis_data->pre_lownoise_mode = cis->cis_data->cur_lownoise_mode;

	info("[%s][%d] pre(%d)->cur(%d), 12bit[%d] LN[%d] AEB[%d] ZOOM[%d], time %lldus\n",
		__func__, cis->cis_data->sen_vsync_count,
		cis->cis_data->sens_config_index_pre, cis->cis_data->sens_config_index_cur,
		cis->cis_data->cur_12bit_mode,
		cis->cis_data->cur_lownoise_mode,
		cis->cis_data->cur_hdr_mode,
		cis_data->cur_remosaic_zoom_ratio,
		PABLO_KTIME_US_DELTA_NOW(st));

	cis->cis_data->seamless_mode_changed = true;

	return ret;
}

int sensor_imx906_cis_get_seamless_mode_info(struct v4l2_subdev *subdev)
{
	struct is_cis *cis = sensor_cis_get_cis(subdev);
	cis_shared_data *cis_data = cis->cis_data;
	int ret = 0, cnt = 0;

	if (sensor_imx906_mode_groups[SENSOR_IMX906_MODE_LN4] != MODE_GROUP_NONE) {
		cis_data->seamless_mode_info[cnt].mode = SENSOR_MODE_LN4;
		sensor_cis_get_mode_info(subdev, sensor_imx906_mode_groups[SENSOR_IMX906_MODE_LN4],
			&cis_data->seamless_mode_info[cnt]);
		cnt++;
	}

	if (sensor_imx906_mode_groups[SENSOR_IMX906_MODE_DSG] != MODE_GROUP_NONE) {
		cis_data->seamless_mode_info[cnt].mode = SENSOR_MODE_REAL_12BIT;
		sensor_cis_get_mode_info(subdev, sensor_imx906_mode_groups[SENSOR_IMX906_MODE_DSG],
			&cis_data->seamless_mode_info[cnt]);
		cnt++;
	}

	if (sensor_imx906_mode_groups[SENSOR_IMX906_MODE_RMS_CROP] != MODE_GROUP_NONE) {
		cis_data->seamless_mode_info[cnt].mode = SENSOR_MODE_CROPPED_RMS;
		cis_data->seamless_mode_info[cnt].extra[0] = IMX906_REMOSAIC_ZOOM_RATIO_X_2;
		sensor_cis_get_mode_info(subdev, sensor_imx906_mode_groups[SENSOR_IMX906_MODE_RMS_CROP],
			&cis_data->seamless_mode_info[cnt]);
		cnt++;
	}

	cis_data->seamless_mode_cnt = cnt;

	return ret;
}

static int sensor_imx906_set_ebd(struct is_cis *cis)
{
	int ret = 0;
	u8 enabled_ebd = 0;

	enabled_ebd = 0;	/* 0: no EBD, 2: EBD 2-line */
	ret = cis->ixc_ops->write8(cis->client, 0x3870, enabled_ebd);
	if (ret < 0) {
		err("is_sensor_read is fail, (ret %d)", ret);
		ret = -EAGAIN;
		goto p_err;
	}

p_err:
	return ret;
}

int sensor_imx906_cis_mode_change(struct v4l2_subdev *subdev, u32 mode)
{
	int ret = 0;
	struct is_cis *cis = sensor_cis_get_cis(subdev);
	struct is_device_sensor *device;
	const struct sensor_cis_mode_info *mode_info;
	ktime_t st = ktime_get();

	device = (struct is_device_sensor *)v4l2_get_subdev_hostdata(subdev);
	WARN_ON(!device);

	if (cis->cis_data->use_print_control_log)
		cis->cis_data->use_print_control_log = false;

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
		err("sensor_imx906_set_registers fail!!");
		goto p_err;
	}

	sensor_imx906_cis_set_mode_group(mode);
	sensor_imx906_cis_get_seamless_mode_info(subdev);

	cis->cis_data->sens_config_index_pre = mode;
	cis->cis_data->remosaic_mode = mode_info->remosaic_mode;
	cis->cis_data->pre_lownoise_mode = IS_CIS_LNOFF;
	cis->cis_data->pre_12bit_mode = mode_info->state_12bit;
	cis->cis_data->pre_remosaic_zoom_ratio = 0;

	if (cis->cis_data->seamless_ctl_before_stream.mode & SENSOR_MODE_LN2)
		cis->cis_data->cur_lownoise_mode = IS_CIS_LN2;
	else if (cis->cis_data->seamless_ctl_before_stream.mode & SENSOR_MODE_LN4)
		cis->cis_data->cur_lownoise_mode = IS_CIS_LN4;
	else
		cis->cis_data->cur_lownoise_mode = IS_CIS_LNOFF;

	if (cis->cis_data->seamless_ctl_before_stream.mode & SENSOR_MODE_REAL_12BIT)
		cis->cis_data->cur_12bit_mode = SENSOR_12BIT_STATE_REAL_12BIT;
	else
		cis->cis_data->cur_12bit_mode = mode_info->state_12bit;

	if (cis->cis_data->seamless_ctl_before_stream.mode & SENSOR_MODE_CROPPED_RMS)
		cis->cis_data->cur_remosaic_zoom_ratio =
			cis->cis_data->seamless_ctl_before_stream.remosaic_crop_zoom_ratio;
	else
		cis->cis_data->cur_remosaic_zoom_ratio = 0;

#ifdef PDAF_DISABLE
	sensor_cis_write_registers(subdev, pdaf_off);
	info("[%s] test only (pdaf_off)\n", __func__);
#endif

	/* Disable EBD*/
	sensor_imx906_set_ebd(cis);

	info("[%s] mode changed(%d)\n", __func__, mode);

	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %lldus", __func__, PABLO_KTIME_US_DELTA_NOW(st));

p_err:
	return ret;
}

int sensor_imx906_cis_stream_on(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_cis *cis = sensor_cis_get_cis(subdev);
	struct is_device_sensor *device;
	ktime_t st = ktime_get();

	device = (struct is_device_sensor *)v4l2_get_subdev_hostdata(subdev);
	WARN_ON(!device);

	is_vendor_set_mipi_clock(device);

	/* Sensor stream on */
	IXC_MUTEX_LOCK(cis->ixc_lock);
	info("%s\n", __func__);
	ret = cis->ixc_ops->write8(cis->client, IMX906_SETUP_MODE_SELECT_ADDR, 0x01);
	IXC_MUTEX_UNLOCK(cis->ixc_lock);

	cis->cis_data->stream_on = true;

	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %lldus", __func__, PABLO_KTIME_US_DELTA_NOW(st));

	return ret;
}

int sensor_imx906_cis_stream_off(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_cis *cis = sensor_cis_get_cis(subdev);
	u8 frame_count = 0;
	ktime_t st = ktime_get();
	struct sensor_imx906_private_runtime *priv_runtime = (struct sensor_imx906_private_runtime *)cis->priv_runtime;

	dbg_sensor(1, "[MOD:D:%d] %s\n", cis->id, __func__);

	cis->cis_data->stream_on = false;

	if (cis->long_term_mode.sen_strm_off_on_enable == true) {
		cis->cis_data->use_print_control_log = true;
		priv_runtime->lte_vsync = cis->cis_data->sen_vsync_count;
	}

	IXC_MUTEX_LOCK(cis->ixc_lock);
	cis->ixc_ops->read8(cis->client, 0x0005, &frame_count);
	ret = cis->ixc_ops->write8(cis->client, IMX906_SETUP_MODE_SELECT_ADDR, 0x00);
	IXC_MUTEX_UNLOCK(cis->ixc_lock);

	info("%s done frame_count(%d)\n", __func__, frame_count);

	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %lldus", __func__, PABLO_KTIME_US_DELTA_NOW(st));

	return ret;
}

int sensor_imx906_cis_set_wb_gain(struct v4l2_subdev *subdev, struct wb_gains wb_gains)
{
	int ret = 0;
	int mode = 0;
	struct is_cis *cis = sensor_cis_get_cis(subdev);
	const struct sensor_cis_mode_info *mode_info;
	u16 abs_gains[4] = {0, };	/* [0]=gr, [1]=r, [2]=b, [3]=gb */
	ktime_t st = ktime_get();

	if (!cis->use_wb_gain)
		return ret;

	mode = cis->cis_data->sens_config_index_cur;
	mode_info = cis->sensor_info->mode_infos[mode];

	if (!mode_info->wb_gain_support)
		return ret;

	dbg_sensor(1, "[SEN:%d]%s:DDK vlaue: wb_gain_gr(%d), wb_gain_r(%d), wb_gain_b(%d), wb_gain_gb(%d)\n",
		cis->id, __func__, wb_gains.gr, wb_gains.r, wb_gains.b, wb_gains.gb);

	abs_gains[0] = (u16)((wb_gains.gr / 4) & 0xFFFF);
	abs_gains[1] = (u16)((wb_gains.r / 4) & 0xFFFF);
	abs_gains[2] = (u16)((wb_gains.b / 4) & 0xFFFF);
	abs_gains[3] = (u16)((wb_gains.gb / 4) & 0xFFFF);

	dbg_sensor(1, "[SEN:%d]%s, abs_gain_gr(0x%4X), abs_gain_r(0x%4X), abs_gain_b(0x%4X), abs_gain_gb(0x%4X)\n",
		cis->id, __func__, abs_gains[0], abs_gains[1], abs_gains[2], abs_gains[3]);

	IXC_MUTEX_LOCK(cis->ixc_lock);
	ret = cis->ixc_ops->write16_array(cis->client, IMX906_ABS_GAIN_GR_SET_ADDR, abs_gains, 4);
	IXC_MUTEX_UNLOCK(cis->ixc_lock);

	if (ret < 0)
		err("sensor_imx906_cis_set_wb_gain fail!!");

	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %lldus", __func__, PABLO_KTIME_US_DELTA_NOW(st));

	return ret;
}

int sensor_imx906_cis_get_updated_binning_ratio(struct v4l2_subdev *subdev, u32 *binning_ratio)
{
	struct is_cis *cis = sensor_cis_get_cis(subdev);
	cis_shared_data *cis_data = cis->cis_data;
	u32 rms_mode = sensor_imx906_mode_groups[SENSOR_IMX906_MODE_RMS_CROP];
	u32 zoom_ratio;

	if (rms_mode == MODE_GROUP_NONE)
		return 0;

	zoom_ratio = cis_data->pre_remosaic_zoom_ratio;

	if (zoom_ratio == IMX906_REMOSAIC_ZOOM_RATIO_X_2 && sensor_imx906_rms_binning_ratio[rms_mode])
		*binning_ratio = sensor_imx906_rms_binning_ratio[rms_mode];

	return 0;
}

int sensor_imx906_cis_set_group_param_hold(struct v4l2_subdev *subdev, bool hold)
{
	int ret = 0;
	struct is_cis *cis = sensor_cis_get_cis(subdev);
	cis_shared_data *cis_data = cis->cis_data;
	struct sensor_imx906_private_runtime *priv_runtime = (struct sensor_imx906_private_runtime *)cis->priv_runtime;

	if (cis->cis_data->use_print_control_log) {
		if (cis->cis_data->sen_vsync_count >= (priv_runtime->lte_vsync + 3)) { 
			cis->cis_data->use_print_control_log = false;
			priv_runtime->lte_vsync = 0;
		}
	}

	if (cis_data->cur_hdr_mode != SENSOR_HDR_MODE_2AEB_2VC) {
		ret = sensor_cis_set_group_param_hold(subdev, hold);

		/* skip N+1 control after update seamless at N frame */
		if (cis_data->update_seamless_state)
			cis_data->skip_control_vsync_count = cis_data->sen_vsync_count + 1;
	}

	return ret;
}

int sensor_imx906_cis_wait_seamless_update_delay(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_cis *cis = sensor_cis_get_cis(subdev);
	cis_shared_data *cis_data = cis->cis_data;
	struct is_device_sensor *device;
	device = (struct is_device_sensor *)v4l2_get_subdev_hostdata(subdev);

	if (cis_data->is_data.scene_mode == AA_SCENE_MODE_LIVE_OUTFOCUS)
		return 0;

	if (test_bit(IS_SENSOR_ESD_RECOVERY, &device->state))
		return 0;

	ret = sensor_cis_wait_seamless_update_delay(subdev);

	return ret;
}

static struct is_cis_ops cis_ops_imx906 = {
	.cis_init = sensor_imx906_cis_init,
	.cis_init_state = sensor_cis_init_state,
	.cis_log_status = sensor_imx906_cis_log_status,
	.cis_group_param_hold = sensor_imx906_cis_set_group_param_hold,
	.cis_set_global_setting = sensor_imx906_cis_set_global_setting,
	.cis_mode_change = sensor_imx906_cis_mode_change,
	.cis_stream_on = sensor_imx906_cis_stream_on,
	.cis_stream_off = sensor_imx906_cis_stream_off,
	.cis_wait_streamon = sensor_cis_wait_streamon,
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
	.cis_calc_again_code = sensor_imx906_cis_calc_again_code, /* imx906 has own code */
	.cis_calc_again_permile = sensor_imx906_cis_calc_again_permile, /* imx906 has own code */
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
	.cis_set_wb_gains = sensor_imx906_cis_set_wb_gain,
	.cis_update_seamless_mode = sensor_imx906_cis_update_seamless_mode,
	.cis_seamless_ctl_before_stream = sensor_cis_seamless_ctl_before_stream,
	.cis_update_seamless_state = sensor_cis_update_seamless_state,
	.cis_wait_seamless_update_delay = sensor_imx906_cis_wait_seamless_update_delay,
	.cis_get_updated_binning_ratio = sensor_imx906_cis_get_updated_binning_ratio,
};

int cis_imx906_probe_i2c(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	int ret;
	struct is_cis *cis;
	struct is_device_sensor_peri *sensor_peri;
	char const *setfile;
	struct device_node *dnode = client->dev.of_node;
	struct sensor_imx906_private_runtime *priv_runtime;

	probe_info("%s started\n", __func__);

	ret = sensor_cis_probe(client, &(client->dev), &sensor_peri, I2C_TYPE);
	if (ret) {
		probe_info("%s: sensor_cis_probe ret(%d)\n", __func__, ret);
		return ret;
	}

	cis = &sensor_peri->cis;
	cis->ctrl_delay = N_PLUS_TWO_FRAME;
	cis->cis_ops = &cis_ops_imx906;
	/* belows are depend on sensor cis. MUST check sensor spec */
	cis->bayer_order = OTF_INPUT_ORDER_BAYER_RG_GB;
	cis->use_wb_gain = true;
	cis->use_total_gain = false;
	cis->reg_addr = &sensor_imx906_reg_addr;
	cis->priv_runtime = kzalloc(sizeof(struct sensor_imx906_private_runtime), GFP_KERNEL);
	if (!cis->priv_runtime) {
		kfree(cis->cis_data);
		kfree(cis->subdev);
		probe_err("cis->priv_runtime is NULL");
		return -ENOMEM;
	}

	priv_runtime = (struct sensor_imx906_private_runtime *)cis->priv_runtime;
	priv_runtime->lte_vsync = 0;

	ret = of_property_read_string(dnode, "setfile", &setfile);
	if (ret) {
		err("setfile index read fail(%d), take default setfile!!", ret);
		setfile = "default";
	}

	if (strcmp(setfile, "default") == 0 || strcmp(setfile, "setA") == 0)
		probe_info("[%s] setfile_A mclk: 19.2Mhz \n", __func__);
	else
		err("setfile index out of bound, take default (setfile_A mclk: 26Mhz)");

	cis->sensor_info = &sensor_imx906_info_A;

	is_vendor_set_mipi_mode(cis);

	probe_info("%s done\n", __func__);
	return ret;
}

static const struct of_device_id sensor_cis_imx906_match[] = {
	{
		.compatible = "samsung,exynos-is-cis-imx906",
	},
	{},
};
MODULE_DEVICE_TABLE(of, sensor_cis_imx906_match);

static const struct i2c_device_id sensor_cis_imx906_idt[] = {
	{ SENSOR_NAME, 0 },
	{},
};

static struct i2c_driver sensor_cis_imx906_driver = {
	.probe	= cis_imx906_probe_i2c,
	.driver = {
		.name	= SENSOR_NAME,
		.owner	= THIS_MODULE,
		.of_match_table = sensor_cis_imx906_match,
		.suppress_bind_attrs = true,
	},
	.id_table = sensor_cis_imx906_idt
};

#ifdef MODULE
builtin_i2c_driver(sensor_cis_imx906_driver);
#else
static int __init sensor_cis_imx906_init(void)
{
	int ret;

	ret = i2c_add_driver(&sensor_cis_imx906_driver);
	if (ret)
		err("failed to add %s driver: %d\n",
			sensor_cis_imx906_driver.driver.name, ret);

	return ret;
}
late_initcall_sync(sensor_cis_imx906_init);
#endif

MODULE_LICENSE("GPL");
MODULE_SOFTDEP("pre: fimc-is");
