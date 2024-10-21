/*
 * Samsung Exynos5 SoC series Sensor driver
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
#include "is-cis-jn1.h"
#include "is-cis-jn1-setA.h"

#include "is-helper-ixc.h"
#include "is-vender.h"
#include "is-sec-define.h"

#include "interface/is-interface-library.h"
#include "is-interface-sensor.h"
#define SENSOR_NAME "JN1"

/* temp change, remove once PDAF settings done */
//#define PDAF_DISABLE
#ifdef PDAF_DISABLE
const u16 sensor_jn1_pdaf_off[] = {
	/* Tail Mode off : Nomal Mode Operation (AF implant) */
	0x0D00, 0x0100, 0x02,
	0x0D02, 0x0001, 0x02,
};

const struct sensor_regs pdaf_off = SENSOR_REGS(sensor_jn1_pdaf_off);
#endif

void sensor_jn1_cis_set_mode_group(u32 mode)
{
	sensor_jn1_mode_groups[SENSOR_JN1_MODE_DEFAULT] = mode;
	sensor_jn1_mode_groups[SENSOR_JN1_MODE_RMS_CROP] = MODE_GROUP_NONE;

	switch (mode) {
	case SENSOR_JN1_4SUM_4080x3060_30FPS:
		sensor_jn1_mode_groups[SENSOR_JN1_MODE_DEFAULT] =
			SENSOR_JN1_4SUM_4080x3060_30FPS;
		sensor_jn1_mode_groups[SENSOR_JN1_MODE_RMS_CROP] =
			SENSOR_JN1_4SUM_4080x3060_30FPS;
		break;
	case SENSOR_JN1_4SUM_4080x2296_30FPS:
		sensor_jn1_mode_groups[SENSOR_JN1_MODE_DEFAULT] =
			SENSOR_JN1_4SUM_4080x2296_30FPS;
		sensor_jn1_mode_groups[SENSOR_JN1_MODE_RMS_CROP] =
			SENSOR_JN1_4SUM_4080x2296_30FPS;
		break;
	}

	info("[%s] default(%d) rms_crop(%d)\n", __func__,
		sensor_jn1_mode_groups[SENSOR_JN1_MODE_DEFAULT],
		sensor_jn1_mode_groups[SENSOR_JN1_MODE_RMS_CROP]);
}

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
	cis->cis_data->pre_remosaic_zoom_ratio = 0;
	cis->cis_data->cur_remosaic_zoom_ratio = 0;
	cis->long_term_mode.sen_strm_off_on_step = 0;
	cis->long_term_mode.sen_strm_off_on_enable = false;

	cis->mipi_clock_index_cur = CAM_MIPI_NOT_INITIALIZED;
	cis->mipi_clock_index_new = CAM_MIPI_NOT_INITIALIZED;
	cis->cis_data->cur_pattern_mode = SENSOR_TEST_PATTERN_MODE_OFF;

	sensor_jn1_mode_groups[SENSOR_JN1_MODE_DEFAULT] = MODE_GROUP_NONE;
	sensor_jn1_mode_groups[SENSOR_JN1_MODE_RMS_CROP] = MODE_GROUP_NONE;

	cis->cis_data->sens_config_index_pre = SENSOR_JN1_MODE_MAX;
	cis->cis_data->sens_config_index_cur = 0;
	CALL_CISOPS(cis, cis_data_calculation, subdev, cis->cis_data->sens_config_index_cur);

	is_vendor_set_mipi_mode(cis);

	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %ldus", __func__, PABLO_KTIME_US_DELTA_NOW(st));

	return ret;
}

static const struct is_cis_log log_jn1[] = {
	{I2C_READ, 16, 0x0000, 0, "model_id"},
	{I2C_READ, 8, 0x0002, 0, "rev_number"},
	{I2C_READ, 8, 0x0005, 0, "frame_count"},
	{I2C_READ, 16, 0x0100, 0, "mode_select"},
	{I2C_READ, 16, 0x0340, 0, "fll"},
	{I2C_READ, 16, 0x0342, 0, "llp"},
};

int sensor_jn1_cis_log_status(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_cis *cis = sensor_cis_get_cis(subdev);

	sensor_cis_log_status(cis, log_jn1, ARRAY_SIZE(log_jn1), (char *)__func__);

	return ret;
}

#if WRITE_SENSOR_CAL_FOR_HW_GGC
int sensor_jn1_cis_HW_GGC_write(struct v4l2_subdev *subdev)
{
	int ret = 0;
	int i = 0;
	struct is_cis *cis = sensor_cis_get_cis(subdev);

	char *rom_cal_buf = NULL;

	u16 start_addr, data_size, write_data;

	FIMC_BUG(!subdev);

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

p_err:
	return ret;
}
#endif

int sensor_jn1_cis_set_global_setting(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_cis *cis = sensor_cis_get_cis(subdev);
	struct sensor_jn1_private_data *priv = (struct sensor_jn1_private_data *)cis->sensor_info->priv;

	info("[%s] start\n", __func__);

	ret = sensor_cis_write_registers_locked(subdev, priv->global);
	if (ret < 0)
		err("global setting fail!!");

	info("[%s] global setting done\n", __func__);

	dbg_sensor(1, "[%s] global setting done\n", __func__);

#ifdef WRITE_SENSOR_CAL_FOR_HW_GGC 
	sensor_jn1_cis_HW_GGC_write(subdev);
#endif

	return ret;
}

int sensor_jn1_cis_check_cropped_remosaic(cis_shared_data *cis_data, unsigned int mode, unsigned int *next_mode)
{
	int ret = 0;
	u32 zoom_ratio = 0;

	if (sensor_jn1_mode_groups[SENSOR_JN1_MODE_RMS_CROP] == MODE_GROUP_NONE)
		return -EINVAL;

	zoom_ratio = cis_data->pre_remosaic_zoom_ratio;

	if (zoom_ratio != JN1_REMOSAIC_ZOOM_RATIO_X_2)
		return -EINVAL; //goto default

	*next_mode = sensor_jn1_mode_groups[SENSOR_JN1_MODE_RMS_CROP];

	if (*next_mode != cis_data->sens_config_index_cur)
		info("[%s] zoom_ratio %u turn on cropped remosaic\n", __func__, zoom_ratio);

	return ret;
}

int sensor_jn1_cis_mode_change(struct v4l2_subdev *subdev, u32 mode)
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
		err("sensor_jn1_set_registers fail!!");
		goto p_err;
	}

	cis->cis_data->sens_config_index_pre = mode;
	cis->cis_data->remosaic_mode = mode_info->remosaic_mode;
	cis->cis_data->pre_remosaic_zoom_ratio = 0;
	cis->cis_data->cur_remosaic_zoom_ratio = 0;
	cis->cis_data->pre_12bit_mode = mode_info->state_12bit;

	info("[%s] mode changed(%d)\n", __func__, mode);

#ifdef PDAF_DISABLE
	sensor_cis_write_registers(subdev, pdaf_off);
	info("[%s] test only (pdaf_off)\n", __func__);
#endif

	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %ldus", __func__, PABLO_KTIME_US_DELTA_NOW(st));

p_err:
	return ret;
}

int sensor_jn1_cis_stream_on(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_cis *cis = sensor_cis_get_cis(subdev);
	struct is_device_sensor *device;

	ktime_t st = ktime_get();

	device = (struct is_device_sensor *)v4l2_get_subdev_hostdata(subdev);
	WARN_ON(!device);

	is_vendor_set_mipi_clock(device);

	info("%s\n", __func__);

	/* Sensor stream on */
	IXC_MUTEX_LOCK(cis->ixc_lock);
	ret = cis->ixc_ops->write16(cis->client, 0x6028, 0x4000);
	ret = cis->ixc_ops->write8(cis->client, 0x0100, 0x01);
	IXC_MUTEX_UNLOCK(cis->ixc_lock);

	cis->cis_data->stream_on = true;

	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %ldus", __func__, PABLO_KTIME_US_DELTA_NOW(st));

	return ret;
}

int sensor_jn1_cis_stream_off(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_cis *cis = sensor_cis_get_cis(subdev);
	u8 frame_count = 0;
	ktime_t st = ktime_get();

	dbg_sensor(1, "[MOD:D:%d] %s\n", cis->id, __func__);

	cis->cis_data->stream_on = false;

	IXC_MUTEX_LOCK(cis->ixc_lock);
	ret = CALL_CISOPS(cis, cis_group_param_hold, subdev, false);
	if (ret < 0)
		err("group_param_hold_func failed at stream off");

	cis->ixc_ops->read8(cis->client, 0x0005, &frame_count);
	ret = cis->ixc_ops->write16(cis->client, 0x6028, 0x4000);
	ret = cis->ixc_ops->write8(cis->client, 0x0100, 0x00);
	info("%s done frame_count(%d)\n", __func__, frame_count);

	IXC_MUTEX_UNLOCK(cis->ixc_lock);

	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %ldus", __func__, PABLO_KTIME_US_DELTA_NOW(st));

	return ret;
}

static struct is_cis_ops cis_ops_jn1 = {
	.cis_init = sensor_jn1_cis_init,
	.cis_log_status = sensor_jn1_cis_log_status,
	//.cis_group_param_hold = sensor_cis_set_group_param_hold,
	.cis_set_global_setting = sensor_jn1_cis_set_global_setting,
	.cis_mode_change = sensor_jn1_cis_mode_change,
	.cis_stream_on = sensor_jn1_cis_stream_on,
	.cis_stream_off = sensor_jn1_cis_stream_off,
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
};

int cis_jn1_probe_i2c(struct i2c_client *client,
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
	cis->cis_ops = &cis_ops_jn1;
	/* belows are depend on sensor cis. MUST check sensor spec */
	cis->bayer_order = OTF_INPUT_ORDER_BAYER_GR_BG;

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
	.probe	= cis_jn1_probe_i2c,
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
