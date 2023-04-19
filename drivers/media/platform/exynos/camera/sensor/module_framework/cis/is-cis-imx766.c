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
#include "is-cis-imx766.h"
#include "is-cis-imx766-setA.h"

#include "is-helper-ixc.h"
#include "is-sec-define.h"

#include "interface/is-interface-library.h"
#include "is-interface-sensor.h"
#define SENSOR_NAME "IMX766"

static bool sensor_imx766_cal_write_flag;

void sensor_imx766_cis_set_mode_group(u32 mode)
{
	sensor_imx766_mode_groups[SENSOR_IMX766_MODE_DEFAULT] = mode;
	sensor_imx766_mode_groups[SENSOR_IMX766_MODE_RMS_CROP] = MODE_GROUP_NONE;

	switch (mode) {
	case SENSOR_IMX766_4080X3060_30FPS:
		sensor_imx766_mode_groups[SENSOR_IMX766_MODE_DEFAULT] =
			SENSOR_IMX766_4080X3060_30FPS_FCM;
		sensor_imx766_mode_groups[SENSOR_IMX766_MODE_RMS_CROP] =
			SENSOR_IMX766_REMOSAIC_12MPCROP_4080x3060_30FPS_FCM;
		break;
	case SENSOR_IMX766_4080X2296_30FPS:
		sensor_imx766_mode_groups[SENSOR_IMX766_MODE_DEFAULT] =
			SENSOR_IMX766_4080X2296_30FPS_FCM;
		sensor_imx766_mode_groups[SENSOR_IMX766_MODE_RMS_CROP] =
			SENSOR_IMX766_REMOSAIC_8MPCROP_4080x2296_30FPS_FCM;
		break;
	}

	info("[%s] default(%d) rms_crop(%d)\n", __func__,
		sensor_imx766_mode_groups[SENSOR_IMX766_MODE_DEFAULT],
		sensor_imx766_mode_groups[SENSOR_IMX766_MODE_RMS_CROP]);
}

#if SENSOR_IMX766_WRITE_SENSOR_CAL
int sensor_imx766_cis_QuadSensCal_write(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_cis *cis = sensor_cis_get_cis(subdev);
	struct is_device_sensor_peri *sensor_peri = NULL;

	int position;
	ulong cal_addr;
	u8 cal_data[IMX766_QSC_SIZE] = {0, };
	char *rom_cal_buf = NULL;
	ktime_t st = ktime_get();

	sensor_peri = container_of(cis, struct is_device_sensor_peri, cis);
	WARN_ON(!sensor_peri);

	position = sensor_peri->module->position;

	ret = is_sec_get_cal_buf(&rom_cal_buf, position);
	if (ret < 0)
		goto p_err;

	cal_addr = (ulong)rom_cal_buf;
	if (position == SENSOR_POSITION_REAR) {
		cal_addr += IMX766_QSC_BASE_REAR;
	} else {
		err("cis_imx766 position(%d) is invalid!\n", position);
		goto p_err;
	}

	memcpy(cal_data, (u16 *)cal_addr, IMX766_QSC_SIZE);

	ret = cis->ixc_ops->write8_sequential(cis->client, IMX766_QSC_ADDR, cal_data, IMX766_QSC_SIZE);
	if (ret < 0)
		err("cis_imx766 QSC write Error(%d)\n", ret);

	ret = cis->ixc_ops->write8(cis->client, 0x86A9, 0x4E);

	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %ldus", __func__, PABLO_KTIME_US_DELTA_NOW(st));

p_err:
	return ret;
}
#endif

/*************************************************
 *  [IMX766 Analog gain formular]
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

u32 sensor_imx766_cis_calc_again_code(u32 permille)
{
	return 16384 - (16384000 / permille);
}

u32 sensor_imx766_cis_calc_again_permile(u32 code)
{
	return 16384000 / (16384 - code);
}

/* CIS OPS */
int sensor_imx766_cis_init(struct v4l2_subdev *subdev)
{
	int ret = 0;
	ktime_t st = ktime_get();
	struct is_cis *cis = sensor_cis_get_cis(subdev);

/***********************************************************************
***** Check that QSC Cal is written for Remosaic Capture.
***** false : Not yet write the QSC
***** true  : Written the QSC Or Skip
***********************************************************************/
	sensor_imx766_cal_write_flag = false;

	cis->cis_data->stream_on = false;
	cis->cis_data->cur_width = cis->sensor_info->max_width;
	cis->cis_data->cur_height = cis->sensor_info->max_height;
	cis->cis_data->low_expo_start = 33000;
	cis->need_mode_change = false;
	cis->cis_data->pre_remosaic_zoom_ratio = 0;
	cis->cis_data->cur_remosaic_zoom_ratio = 0;
	cis->long_term_mode.sen_strm_off_on_step = 0;
	cis->long_term_mode.sen_strm_off_on_enable = false;

	cis->mipi_clock_index_cur = CAM_MIPI_NOT_INITIALIZED;
	cis->mipi_clock_index_new = CAM_MIPI_NOT_INITIALIZED;
	cis->cis_data->cur_pattern_mode = SENSOR_TEST_PATTERN_MODE_OFF;

	cis->cis_data->sens_config_index_pre = SENSOR_IMX766_MODE_MAX;
	cis->cis_data->sens_config_index_cur = 0;
	CALL_CISOPS(cis, cis_data_calculation, subdev, cis->cis_data->sens_config_index_cur);

	is_vendor_set_mipi_mode(cis);

	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %ldus", __func__, PABLO_KTIME_US_DELTA_NOW(st));

	return ret;
}

static const struct is_cis_log log_imx766[] = {
	{I2C_READ, 16, 0x0000, 0, "model_id"},
	{I2C_READ, 8, 0x0002, 0, "rev_number"},
	{I2C_READ, 8, 0x0005, 0, "frame_count"},
	{I2C_READ, 16, 0x0100, 0, "mode_select"},
	{I2C_READ, 8, 0x0808, 0, "phy_ctrl"},
	{I2C_READ, 16, 0x084E, 0, "0x084E"},
	{I2C_READ, 16, 0x0850, 0, "0x0850"},
	{I2C_READ, 16, 0x0852, 0, "0x0852"},
	{I2C_READ, 16, 0x0854, 0, "0x0854"},
	{I2C_READ, 16, 0x0858, 0, "0x0858"},
	{I2C_READ, 16, 0x0340, 0, "fll"},
	{I2C_READ, 16, 0x0342, 0, "llp"},
};

int sensor_imx766_cis_log_status(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_cis *cis = sensor_cis_get_cis(subdev);

	sensor_cis_log_status(cis, log_imx766, ARRAY_SIZE(log_imx766), (char *)__func__);

	return ret;
}

int sensor_imx766_cis_set_global_setting(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_cis *cis = sensor_cis_get_cis(subdev);
	struct sensor_imx766_private_data *priv = (struct sensor_imx766_private_data *)cis->sensor_info->priv;

	dbg_sensor(1, "[%s] start\n", __func__);

	ret = sensor_cis_write_registers_locked(subdev, priv->global);
	if (ret < 0)
		err("global setting fail!!");

	info("[%s] global setting done\n", __func__);

	// Check that QSC and DPC Cal is written for Remosaic Capture.
	// false : Not yet write the QSC and DPC
	// true  : Written the QSC and DPC
	sensor_imx766_cal_write_flag = false;
	return ret;
}

int sensor_imx766_cis_check_cropped_remosaic(cis_shared_data *cis_data, unsigned int mode, unsigned int *next_mode)
{
	int ret = 0;
	u32 zoom_ratio = 0;

	if (sensor_imx766_mode_groups[SENSOR_IMX766_MODE_RMS_CROP] == MODE_GROUP_NONE)
		return -EINVAL;

	zoom_ratio = cis_data->pre_remosaic_zoom_ratio;

	if (zoom_ratio != IMX766_REMOSAIC_ZOOM_RATIO_X_2)
		return -EINVAL; //goto default

	*next_mode = sensor_imx766_mode_groups[SENSOR_IMX766_MODE_RMS_CROP];

	if (*next_mode != cis_data->sens_config_index_cur)
		info("[%s] zoom_ratio %u turn on cropped remosaic\n", __func__, zoom_ratio);

	return ret;
}

int sensor_imx766_cis_update_seamless_mode_on_vsync(struct v4l2_subdev *subdev)
{
	int ret = 0;
	unsigned int mode = 0;
	unsigned int next_mode = 0;
	cis_shared_data *cis_data;
	const struct sensor_cis_mode_info *next_mode_info;
	struct is_cis *cis = sensor_cis_get_cis(subdev);

	cis_data = cis->cis_data;
	mode = cis_data->sens_config_index_cur;
	next_mode = mode;

	next_mode = sensor_imx766_mode_groups[SENSOR_IMX766_MODE_DEFAULT];
	if (next_mode == MODE_GROUP_NONE) {
		err("mode group is none");
		return -EINVAL;
	}

	sensor_imx766_cis_check_cropped_remosaic(cis->cis_data, mode, &next_mode);
	next_mode_info = cis->sensor_info->mode_infos[next_mode];

	if (mode == next_mode || next_mode == MODE_GROUP_NONE)
		return ret;

	/* New mode settings Update */
	info("%s mode(%d) next_mode(%d)", __func__, mode, next_mode);
	IXC_MUTEX_LOCK(cis->ixc_lock);
	ret |= cis->ixc_ops->write8(cis->client, 0x0104, 0x01);
	ret |= cis->ixc_ops->write8(cis->client, 0x3010, 0x02);
	ret |= sensor_cis_write_registers(subdev, next_mode_info->setfile);
	ret |= cis->ixc_ops->write8(cis->client, 0x0104, 0x00);
	IXC_MUTEX_UNLOCK(cis->ixc_lock);

	if (ret < 0) {
		err("%s sensor_imx766_set_registers fail!!", __func__);
		return ret;
	}

	info("%s sensor_cis_set_registers done for mode %d", __func__, next_mode);

	cis_data->sens_config_index_pre = cis->cis_data->sens_config_index_cur;
	cis_data->sens_config_index_cur = next_mode;

	CALL_CISOPS(cis, cis_data_calculation, subdev, next_mode);

	info("[%s] pre(%d)->cur(%d), zoom [%d]\n",
		__func__,
		cis_data->sens_config_index_pre, cis_data->sens_config_index_cur,
		cis_data->cur_remosaic_zoom_ratio);

	return ret;
}

int sensor_imx766_cis_get_seamless_mode_info(struct v4l2_subdev *subdev)
{
	struct is_cis *cis = sensor_cis_get_cis(subdev);
	cis_shared_data *cis_data = cis->cis_data;
	int ret = 0, cnt = 0;

	if (sensor_imx766_mode_groups[SENSOR_IMX766_MODE_RMS_CROP] != MODE_GROUP_NONE) {
		cis_data->seamless_mode_info[cnt].mode = SENSOR_MODE_CROPPED_RMS;
		sensor_cis_get_mode_info(subdev, sensor_imx766_mode_groups[SENSOR_IMX766_MODE_RMS_CROP],
			&cis_data->seamless_mode_info[cnt]);
		cnt++;
	}

	cis_data->seamless_mode_cnt = cnt;

	return ret;
}

int sensor_imx766_cis_mode_change(struct v4l2_subdev *subdev, u32 mode)
{
	int ret = 0;
	struct is_cis *cis = sensor_cis_get_cis(subdev);
	struct is_device_sensor *device;
	const struct sensor_cis_mode_info *mode_info;
	ktime_t st = ktime_get();

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
		err("sensor_imx766_set_registers fail!!");
		goto EXIT;
	}

	sensor_imx766_cis_set_mode_group(mode);
	cis->cis_data->sens_config_index_cur = sensor_imx766_mode_groups[SENSOR_IMX766_MODE_DEFAULT];

	if (sensor_imx766_mode_groups[SENSOR_IMX766_MODE_RMS_CROP] == MODE_GROUP_NONE)
		cis->use_notify_vsync = false;
	else
		cis->use_notify_vsync = true;

	cis->cis_data->sens_config_index_pre = mode;
	cis->cis_data->remosaic_mode = mode_info->remosaic_mode;
	cis->cis_data->pre_remosaic_zoom_ratio = 0;
	cis->cis_data->cur_remosaic_zoom_ratio = 0;

	/* Disable Embedded Data Line */
	IXC_MUTEX_LOCK(cis->ixc_lock);
	cis->ixc_ops->write8(cis->client, IMX766_EBD_CONTROL_ADDR, 0x00);
	IXC_MUTEX_UNLOCK(cis->ixc_lock);

#if SENSOR_IMX766_WRITE_SENSOR_CAL
	/* Apply QSC for not only remosaic modes that use AF */
	if ((mode == SENSOR_IMX766_4080X3060_30FPS
		|| mode == SENSOR_IMX766_4080X2296_30FPS
		|| mode == SENSOR_IMX766_3840x2160_60FPS
		|| mode == SENSOR_IMX766_REMOSAIC_FULL_8160x6120_10FPS)
		&& sensor_imx766_cal_write_flag == false) {
		sensor_imx766_cal_write_flag = true;

		info("[%s] mode is %d. Write QSC data.\n", __func__, mode);
		IXC_MUTEX_LOCK(cis->ixc_lock);
		ret = sensor_imx766_cis_QuadSensCal_write(subdev);
		IXC_MUTEX_UNLOCK(cis->ixc_lock);
		if (ret < 0)
			err("sensor_imx576_Quad_Sens_Cal_write fail!! (%d)", ret);
	}
#endif
	info("[%s] mode changed(%d)\n", __func__, mode);

	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %ldus", __func__, PABLO_KTIME_US_DELTA_NOW(st));

EXIT:
	sensor_imx766_cis_get_seamless_mode_info(subdev);
p_err:
	return ret;
}

int sensor_imx766_cis_stream_on(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_cis *cis = sensor_cis_get_cis(subdev);
	struct is_device_sensor *device;
	u8 test0, test1, test2;
	ktime_t st = ktime_get();

	device = (struct is_device_sensor *)v4l2_get_subdev_hostdata(subdev);
	WARN_ON(!device);

	dbg_sensor(1, "[MOD:D:%d] %s\n", cis->id, __func__);

	is_vendor_set_mipi_clock(device);

	/* Sensor stream on */
	IXC_MUTEX_LOCK(cis->ixc_lock);
	ret = cis->ixc_ops->write8(cis->client, IMX766_SETUP_MODE_SELECT_ADDR, 0x01);
	cis->ixc_ops->read8(cis->client, 0x0808, &test0);
	cis->ixc_ops->read8(cis->client, 0x084F, &test1);
	cis->ixc_ops->read8(cis->client, 0x0853, &test2);
	IXC_MUTEX_UNLOCK(cis->ixc_lock);

	cis->cis_data->stream_on = true;
	info("%s done (ctrl 0x%x : 0x%x, 0x%x)\n", __func__, test0, test1, test2);

	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %ldus", __func__, PABLO_KTIME_US_DELTA_NOW(st));

	return ret;
}

int sensor_imx766_cis_stream_off(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_cis *cis = sensor_cis_get_cis(subdev);
	u8 frame_count = 0;
	ktime_t st = ktime_get();

	dbg_sensor(1, "[MOD:D:%d] %s\n", cis->id, __func__);
	
	ret = CALL_CISOPS(cis, cis_group_param_hold, subdev, false);
	if (ret < 0)
		err("group_param_hold_func failed at stream off");

	IXC_MUTEX_LOCK(cis->ixc_lock);
	cis->ixc_ops->read8(cis->client, 0x0005, &frame_count);
	ret = cis->ixc_ops->write8(cis->client, IMX766_SETUP_MODE_SELECT_ADDR, 0x00);
	IXC_MUTEX_UNLOCK(cis->ixc_lock);

	info("%s done frame_count(%d)\n", __func__, frame_count);

	cis->cis_data->stream_on = false;

	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %ldus", __func__, PABLO_KTIME_US_DELTA_NOW(st));

	return ret;
}

int sensor_imx766_cis_set_wb_gain(struct v4l2_subdev *subdev, struct wb_gains wb_gains)
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
	ret = cis->ixc_ops->write16_array(cis->client, IMX766_ABS_GAIN_GR_SET_ADDR, abs_gains, 4);
	IXC_MUTEX_UNLOCK(cis->ixc_lock);

	if (ret < 0)
		err("sensor_imx766_cis_set_wb_gain fail!!");

	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %ldus", __func__, PABLO_KTIME_US_DELTA_NOW(st));

	return ret;
}

int sensor_imx766_cis_get_updated_binning_ratio(struct v4l2_subdev *subdev, u32 *binning_ratio)
{
	struct is_cis *cis = sensor_cis_get_cis(subdev);
	cis_shared_data *cis_data = cis->cis_data;
	u32 rms_mode = sensor_imx766_mode_groups[SENSOR_IMX766_MODE_RMS_CROP];
	u32 zoom_ratio;

	if (rms_mode == MODE_GROUP_NONE)
		return 0;

	zoom_ratio = cis_data->pre_remosaic_zoom_ratio;

	if (zoom_ratio == IMX766_REMOSAIC_ZOOM_RATIO_X_2 && sensor_imx766_rms_binning_ratio[rms_mode])
		*binning_ratio = sensor_imx766_rms_binning_ratio[rms_mode];

	return 0;
}

int sensor_imx766_cis_notify_vsync(struct v4l2_subdev *subdev)
{
	return sensor_imx766_cis_update_seamless_mode_on_vsync(subdev);
}

static struct is_cis_ops cis_ops_imx766 = {
	.cis_init = sensor_imx766_cis_init,
	.cis_log_status = sensor_imx766_cis_log_status,
	.cis_group_param_hold = sensor_cis_set_group_param_hold,
	.cis_set_global_setting = sensor_imx766_cis_set_global_setting,
	.cis_mode_change = sensor_imx766_cis_mode_change,
	.cis_stream_on = sensor_imx766_cis_stream_on,
	.cis_stream_off = sensor_imx766_cis_stream_off,
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
	.cis_calc_again_code = sensor_imx766_cis_calc_again_code, /* imx766 has own code */
	.cis_calc_again_permile = sensor_imx766_cis_calc_again_permile, /* imx766 has own code */
	.cis_set_digital_gain = sensor_cis_set_digital_gain,
	.cis_get_digital_gain = sensor_cis_get_digital_gain,
	.cis_get_min_digital_gain = sensor_cis_get_min_digital_gain,
	.cis_get_max_digital_gain = sensor_cis_get_max_digital_gain,
	.cis_calc_dgain_code = sensor_cis_calc_dgain_code,
	.cis_calc_dgain_permile = sensor_cis_calc_dgain_permile,
	.cis_compensate_gain_for_extremely_br = sensor_cis_compensate_gain_for_extremely_br,
	.cis_check_rev_on_init = sensor_cis_check_rev_on_init,
	.cis_set_initial_exposure = sensor_cis_set_initial_exposure,
	.cis_set_wb_gains = sensor_imx766_cis_set_wb_gain,
	.cis_get_updated_binning_ratio = sensor_imx766_cis_get_updated_binning_ratio,
	.cis_notify_vsync = sensor_imx766_cis_notify_vsync,
};

int cis_imx766_probe_i2c(struct i2c_client *client,
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
	cis->cis_ops = &cis_ops_imx766;
	/* belows are depend on sensor cis. MUST check sensor spec */
	cis->bayer_order = OTF_INPUT_ORDER_BAYER_RG_GB;
	cis->use_wb_gain = true;
	cis->use_seamless_mode = true;
	cis->reg_addr = &sensor_imx766_reg_addr;

	ret = of_property_read_string(dnode, "setfile", &setfile);
	if (ret) {
		err("setfile index read fail(%d), take default setfile!!", ret);
		setfile = "default";
	}

	if (strcmp(setfile, "default") == 0 || strcmp(setfile, "setA") == 0)
		probe_info("[%s] setfile_A mclk: 26Mhz \n", __func__);
	else
		err("setfile index out of bound, take default (setfile_A mclk: 26Mhz)");

	cis->sensor_info = &sensor_imx766_info_A;

	probe_info("%s done\n", __func__);
	return ret;
}

static const struct of_device_id sensor_cis_imx766_match[] = {
	{
		.compatible = "samsung,exynos-is-cis-imx766",
	},
	{},
};
MODULE_DEVICE_TABLE(of, sensor_cis_imx766_match);

static const struct i2c_device_id sensor_cis_imx766_idt[] = {
	{ SENSOR_NAME, 0 },
	{},
};

static struct i2c_driver sensor_cis_imx766_driver = {
	.probe	= cis_imx766_probe_i2c,
	.driver = {
		.name	= SENSOR_NAME,
		.owner	= THIS_MODULE,
		.of_match_table = sensor_cis_imx766_match,
		.suppress_bind_attrs = true,
	},
	.id_table = sensor_cis_imx766_idt
};

#ifdef MODULE
builtin_i2c_driver(sensor_cis_imx766_driver);
#else
static int __init sensor_cis_imx766_init(void)
{
	int ret;

	ret = i2c_add_driver(&sensor_cis_imx766_driver);
	if (ret)
		err("failed to add %s driver: %d\n",
			sensor_cis_imx766_driver.driver.name, ret);

	return ret;
}
late_initcall_sync(sensor_cis_imx766_init);
#endif

MODULE_LICENSE("GPL");
MODULE_SOFTDEP("pre: fimc-is");
