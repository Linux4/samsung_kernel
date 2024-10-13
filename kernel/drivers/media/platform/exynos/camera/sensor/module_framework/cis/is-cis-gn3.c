/*
 * Samsung Exynos SoC series Sensor driver
 *
 *
 * Copyright (c) 2016 Samsung Electronics Co., Ltd
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
#include "is-cis-gn3.h"
#include "is-cis-gn3-setA-19p2.h"
#include "is-helper-ixc.h"
#ifdef CONFIG_CAMERA_VENDOR_MCD
#include "is-sec-define.h"
#endif

#define SENSOR_NAME "S5KGN3"
/* #define DEBUG_CAL_WRITE */

int sensor_gn3_cis_set_global_setting(struct v4l2_subdev *subdev);
int sensor_gn3_cis_wait_streamon(struct v4l2_subdev *subdev);
int sensor_gn3_cis_wait_streamoff(struct v4l2_subdev *subdev);

#if IS_ENABLED(CIS_CALIBRATION)
int sensor_gn3_cis_set_cal(struct v4l2_subdev *subdev);
#endif

#if IS_ENABLED(USE_CAMERA_SENSOR_RETENTION)
void sensor_gn3_cis_set_mode_group(u32 mode)
{
	sensor_gn3_mode_groups[SENSOR_GN3_MODE_NORMAL] = mode;
	sensor_gn3_mode_groups[SENSOR_GN3_MODE_IDCG] = MODE_GROUP_NONE;
	sensor_gn3_mode_groups[SENSOR_GN3_MODE_LN2] = MODE_GROUP_NONE;
	sensor_gn3_mode_groups[SENSOR_GN3_MODE_LN4] = MODE_GROUP_NONE;
	sensor_gn3_mode_groups[SENSOR_GN3_MODE_RMS_CROP] = MODE_GROUP_NONE;

	switch (mode) {
	case SENSOR_GN3_4080X3060_60FPS_R10 :
		sensor_gn3_mode_groups[SENSOR_GN3_MODE_NORMAL] = SENSOR_GN3_4080X3060_60FPS_R10;
		sensor_gn3_mode_groups[SENSOR_GN3_MODE_LN2] = SENSOR_GN3_4080X3060_53FPS_LN2_R10;
		sensor_gn3_mode_groups[SENSOR_GN3_MODE_LN4] = SENSOR_GN3_4080X3060_26FPS_LN4_R10;
		sensor_gn3_mode_groups[SENSOR_GN3_MODE_RMS_CROP] = MODE_GROUP_RMS;
		sensor_gn3_rms_zoom_mode[SENSOR_GN3_RMS_ZOOM_MODE_X_17] = SENSOR_GN3_4080X3060_30FPS_CROP_R10_BDS;
		sensor_gn3_rms_zoom_mode[SENSOR_GN3_RMS_ZOOM_MODE_X_20] = SENSOR_GN3_4080X3060_30FPS_CROP_R10;
		break;

	case SENSOR_GN3_4080X3060_60FPS_IDCG_R12 :
		sensor_gn3_mode_groups[SENSOR_GN3_MODE_LN2] = SENSOR_GN3_4080X3060_53FPS_LN2_R12;
		sensor_gn3_mode_groups[SENSOR_GN3_MODE_LN4] = SENSOR_GN3_4080X3060_26FPS_LN4_R12;
		sensor_gn3_mode_groups[SENSOR_GN3_MODE_IDCG] = SENSOR_GN3_4080X3060_60FPS_IDCG_R12;
		sensor_gn3_mode_groups[SENSOR_GN3_MODE_RMS_CROP] = MODE_GROUP_RMS;
		sensor_gn3_rms_zoom_mode[SENSOR_GN3_RMS_ZOOM_MODE_X_17] = SENSOR_GN3_4080X3060_30FPS_CROP_R12_BDS;
		sensor_gn3_rms_zoom_mode[SENSOR_GN3_RMS_ZOOM_MODE_X_20] = SENSOR_GN3_4080X3060_30FPS_CROP_R12;
		break;
	case SENSOR_GN3_4080X2720_60FPS_R10 :
		sensor_gn3_mode_groups[SENSOR_GN3_MODE_LN2] = SENSOR_GN3_4080X2720_30FPS_LN2_R10;
		break;

	case SENSOR_GN3_4080X2720_60FPS_IDCG_R12 :
		sensor_gn3_mode_groups[SENSOR_GN3_MODE_LN2] = SENSOR_GN3_4080X2720_30FPS_LN2_R12;
		sensor_gn3_mode_groups[SENSOR_GN3_MODE_IDCG] = SENSOR_GN3_4080X2720_60FPS_IDCG_R12;
		break;

	case SENSOR_GN3_4080X2296_60FPS_R10 :
		sensor_gn3_mode_groups[SENSOR_GN3_MODE_NORMAL] = SENSOR_GN3_4080X2296_60FPS_R10;
		sensor_gn3_mode_groups[SENSOR_GN3_MODE_LN2] = SENSOR_GN3_4080X2296_60FPS_LN2_R10;
		sensor_gn3_mode_groups[SENSOR_GN3_MODE_LN4] = SENSOR_GN3_4080X2296_30FPS_LN4_R10;
		sensor_gn3_mode_groups[SENSOR_GN3_MODE_RMS_CROP] = MODE_GROUP_RMS;
		sensor_gn3_rms_zoom_mode[SENSOR_GN3_RMS_ZOOM_MODE_X_20] = SENSOR_GN3_4080X2296_60FPS_CROP_R10;
		break;

	case SENSOR_GN3_4080X2296_60FPS_IDCG_R12 :
		sensor_gn3_mode_groups[SENSOR_GN3_MODE_LN2] = SENSOR_GN3_4080X2296_60FPS_LN2_R12;
		sensor_gn3_mode_groups[SENSOR_GN3_MODE_LN4] = SENSOR_GN3_4080X2296_30FPS_LN4_R12;
		sensor_gn3_mode_groups[SENSOR_GN3_MODE_IDCG] = SENSOR_GN3_4080X2296_60FPS_IDCG_R12;
		sensor_gn3_mode_groups[SENSOR_GN3_MODE_RMS_CROP] = MODE_GROUP_RMS;
		sensor_gn3_rms_zoom_mode[SENSOR_GN3_RMS_ZOOM_MODE_X_17] = SENSOR_GN3_4080X2296_30FPS_CROP_R12_BDS;
		sensor_gn3_rms_zoom_mode[SENSOR_GN3_RMS_ZOOM_MODE_X_20] = SENSOR_GN3_4080X2296_60FPS_CROP_R12;
		break;

	case SENSOR_GN3_3840X2160_60FPS_IDCG_R12 :
		sensor_gn3_mode_groups[SENSOR_GN3_MODE_LN2] = SENSOR_GN3_3840X2160_60FPS_LN2_R12;
		sensor_gn3_mode_groups[SENSOR_GN3_MODE_IDCG] = SENSOR_GN3_3840X2160_60FPS_IDCG_R12;
		sensor_gn3_mode_groups[SENSOR_GN3_MODE_RMS_CROP] = MODE_GROUP_RMS;
		sensor_gn3_rms_zoom_mode[SENSOR_GN3_RMS_ZOOM_MODE_X_20] = SENSOR_GN3_3840X2160_60FPS_CROP_R12;
		break;
	}

	info("[%s] normal(%d) idcg(%d) ln2(%d) ln4(%d) rms(%d)\n", __func__,
		sensor_gn3_mode_groups[SENSOR_GN3_MODE_NORMAL],
		sensor_gn3_mode_groups[SENSOR_GN3_MODE_IDCG],
		sensor_gn3_mode_groups[SENSOR_GN3_MODE_LN2],
		sensor_gn3_mode_groups[SENSOR_GN3_MODE_LN4],
		sensor_gn3_mode_groups[SENSOR_GN3_MODE_RMS_CROP]);
}
#endif

int sensor_gn3_cis_select_setfile(struct v4l2_subdev *subdev)
{
	int ret = 0;
	u16 rev = 0;
	u8 feature_id = 0;
	struct is_cis *cis = sensor_cis_get_cis(subdev);

	rev = cis->cis_data->cis_rev;
	info("gn3 sensor revision(%#x)\n", rev);

	IXC_MUTEX_LOCK(cis->ixc_lock);
	ret |= cis->ixc_ops->read8(cis->client, 0x000D, &feature_id);
	info("gn3 sensor revision 0x000D(%#x)\n", feature_id);
	ret |= cis->ixc_ops->read8(cis->client, 0x000E, &feature_id);
	info("gn3 sensor revision 0x000E(%#x)\n", feature_id);
	IXC_MUTEX_UNLOCK(cis->ixc_lock);

	return ret;
}

void sensor_gn3_cis_init_private(struct v4l2_subdev *subdev)
{
	struct is_cis *cis = sensor_cis_get_cis(subdev);
	struct sensor_gn3_private_runtime *priv_runtime;

	priv_runtime = (struct sensor_gn3_private_runtime *)cis->priv_runtime;

	priv_runtime->first_entrance = true;
	priv_runtime->load_retention = false;
	priv_runtime->is_frame_duration_on = false;
}

/* CIS OPS */
int sensor_gn3_cis_init(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_cis *cis = sensor_cis_get_cis(subdev);
	struct sensor_gn3_private_runtime *priv_runtime = (struct sensor_gn3_private_runtime *)cis->priv_runtime;

#if !defined(CONFIG_CAMERA_VENDOR_MCD)
	memset(cis->cis_data, 0, sizeof(cis_shared_data));

	ret = sensor_cis_check_rev(cis);
	if (ret < 0) {
		warn("sensor_gn3_check_rev is fail when cis init");
		return -EINVAL;
	}
#endif

	sensor_gn3_cis_select_setfile(subdev);
	sensor_gn3_cis_init_private(subdev);

	priv_runtime->crop_shift_offset_x = 0;
	priv_runtime->crop_shift_offset_y = 0;
	priv_runtime->crop_shift_enabled = false;

	cis->cis_data->cur_width = cis->sensor_info->max_width;
	cis->cis_data->cur_height = cis->sensor_info->max_height;
	cis->cis_data->low_expo_start = 33000;

	cis->cis_data->sens_config_index_pre = SENSOR_GN3_MODE_MAX;
	cis->cis_data->sens_config_index_cur = 0;
	CALL_CISOPS(cis, cis_data_calculation, subdev, cis->cis_data->sens_config_index_cur);

	return ret;
}

int sensor_gn3_cis_deinit(struct v4l2_subdev *subdev)
{
	int ret = 0;

#if IS_ENABLED(USE_CAMERA_SENSOR_RETENTION)
	struct is_cis *cis = sensor_cis_get_cis(subdev);
	struct sensor_gn3_private_runtime *priv_runtime = (struct sensor_gn3_private_runtime *)cis->priv_runtime;

	if (priv_runtime->load_retention == false) {
		info("%s: need to load retention\n", __func__);
		sensor_gn3_cis_stream_on(subdev);
		sensor_gn3_cis_wait_streamon(subdev);
		sensor_gn3_cis_stream_off(subdev);
		sensor_gn3_cis_wait_streamoff(subdev);
		info("%s: complete to load retention\n", __func__);
	}

	/* retention mode CRC wait calculation */
	usleep_range(10000, 10000);
#endif

	return ret;
}

static const struct is_cis_log log_gn3[] = {
	{I2C_READ, 8, 0x0005, 0, "frame_count"},
	/* 4000 page */
	{I2C_WRITE, 16, 0x6000, 0x0005, "page unlock"},
	{I2C_WRITE, 16, 0xFCFC, 0x4000, "0x4000 page"},
	{I2C_READ, 16, 0x0000, 0, "model_id"},
	{I2C_READ, 16, 0x0002, 0, "revision_number"},
	{I2C_READ, 16, 0x0100, 0, "0x0100"},
	{I2C_READ, 16, 0x0110, 0, "0x0110"},
	{I2C_READ, 16, 0x0104, 0, "0x0104"},
	{I2C_READ, 16, 0x0112, 0, "0x0112"},
	{I2C_READ, 16, 0x0202, 0, "0x0202"},
	{I2C_READ, 16, 0x0264, 0, "0x0264"},
	{I2C_READ, 16, 0x0702, 0, "0x0702"},
	{I2C_READ, 16, 0x0704, 0, "0x0704"},
	{I2C_READ, 16, 0x0340, 0, "0x0340"},
	{I2C_READ, 16, 0x0342, 0, "0x0342"},
	{I2C_READ, 16, 0x0344, 0, "0x0344"},
	{I2C_READ, 16, 0x0346, 0, "0x0346"},
	{I2C_READ, 16, 0x0348, 0, "0x0348"},
	{I2C_READ, 16, 0x034A, 0, "0x034A"},
	{I2C_READ, 16, 0x034C, 0, "0x034C"},
	{I2C_READ, 16, 0x034E, 0, "0x034E"},
	{I2C_READ, 16, 0x0350, 0, "0x0350"},
	{I2C_READ, 16, 0x0352, 0, "0x0352"},
	{I2C_READ, 16, 0x0728, 0, "0x0728"},
	{I2C_READ, 16, 0x072A, 0, "0x072A"},
	{I2C_READ, 16, 0x0B30, 0, "0x0B30"},
	{I2C_READ, 16, 0x0E00, 0, "0x0E00"},
	{I2C_READ, 16, 0x0E18, 0, "0x0E18"},
	{I2C_READ, 16, 0x0E1A, 0, "0x0E1A"},
	{I2C_READ, 16, 0x0E1C, 0, "0x0E1C"},
	{I2C_READ, 16, 0x0E20, 0, "0x0E20"},
	{I2C_READ, 16, 0x0E26, 0, "0x0E26"},
	{I2C_READ, 16, 0x0E28, 0, "0x0E28"},
	{I2C_READ, 16, 0x0E2A, 0, "0x0E2A"},
	/* 2001 page */
	{I2C_WRITE, 16, 0xFCFC, 0x2001, "0x2001 page"},
	{I2C_READ, 16, 0x43E8, 0, "0x43E8"},
	{I2C_READ, 16, 0x43EA, 0, "0x43EA"},
	{I2C_READ, 16, 0x43EE, 0, "0x43EE"},
	{I2C_READ, 16, 0x43F0, 0, "0x43F0"},
	{I2C_READ, 16, 0x43F2, 0, "0x43F2"},
	{I2C_READ, 16, 0x4D82, 0, "0x4D82"},
	{I2C_READ, 16, 0x4D84, 0, "0x4D84"},
	{I2C_READ, 16, 0x4D86, 0, "0x4D86"},
	{I2C_READ, 16, 0x4D88, 0, "0x4D88"},
	{I2C_READ, 16, 0x309C, 0, "0x309C"},
	{I2C_READ, 16, 0x309E, 0, "0x309E"},
	{I2C_READ, 16, 0x4B90, 0, "0x4B90"},
	{I2C_READ, 16, 0x4B92, 0, "0x4B92"},
	{I2C_READ, 16, 0x4B94, 0, "0x4B94"},
	{I2C_READ, 16, 0x4B96, 0, "0x4B96"},
	{I2C_READ, 16, 0x4B98, 0, "0x4B98"},
	{I2C_READ, 16, 0x4B9A, 0, "0x4B9A"},
	{I2C_READ, 16, 0x4B9C, 0, "0x4B9C"},
	{I2C_READ, 16, 0x4B9E, 0, "0x4B9E"},
	{I2C_READ, 16, 0x4BA0, 0, "0x4BA0"},
	{I2C_READ, 16, 0x4BA2, 0, "0x4BA2"},
	{I2C_READ, 16, 0x4BA4, 0, "0x4BA4"},
	{I2C_READ, 16, 0x4BA6, 0, "0x4BA6"},
	{I2C_READ, 16, 0x4BA8, 0, "0x4BA8"},
	{I2C_READ, 16, 0x4BAA, 0, "0x4BAA"},
	{I2C_READ, 16, 0x4BAC, 0, "0x4BAC"},
	{I2C_READ, 16, 0x4BAE, 0, "0x4BAE"},
	{I2C_READ, 16, 0x4BB0, 0, "0x4BB0"},
	{I2C_READ, 16, 0x4BB2, 0, "0x4BB2"},
	{I2C_READ, 16, 0x4BB4, 0, "0x4BB4"},
	{I2C_READ, 16, 0x4BB6, 0, "0x4BB6"},
	{I2C_READ, 16, 0x4BB8, 0, "0x4BB8"},
	{I2C_READ, 16, 0x4BBA, 0, "0x4BBA"},
	{I2C_READ, 16, 0x4BBC, 0, "0x4BBC"},
	{I2C_READ, 16, 0x4BBE, 0, "0x4BBE"},
	{I2C_READ, 16, 0x4BC0, 0, "0x4BC0"},
	{I2C_READ, 16, 0x4BC4, 0, "0x4BC4"},
	{I2C_READ, 16, 0x4BC6, 0, "0x4BC6"},
	{I2C_READ, 16, 0x4BCA, 0, "0x4BCA"},
	{I2C_READ, 16, 0x4BD0, 0, "0x4BD0"},
	{I2C_READ, 16, 0x4BD4, 0, "0x4BD4"},
	/* restore 4000 page */
	{I2C_WRITE, 16, 0xFCFC, 0x4000, "0x4000 page"},
	{I2C_WRITE, 16, 0x6000, 0x0085, "page lock"},
};

int sensor_gn3_cis_log_status(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_cis *cis = sensor_cis_get_cis(subdev);

#if IS_ENABLED(USE_CAMERA_SENSOR_RETENTION)
	if (sensor_cis_support_retention_mode(subdev) && cis->cis_data->stream_on == false)
		sensor_cis_set_retention_mode(subdev, SENSOR_RETENTION_INACTIVE);
#endif

	sensor_cis_log_status(cis, log_gn3, ARRAY_SIZE(log_gn3), (char *)__func__);

	return ret;
}

int sensor_gn3_cis_set_global_setting_internal(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_cis *cis = sensor_cis_get_cis(subdev);
	struct sensor_gn3_private_data *priv = (struct sensor_gn3_private_data *)cis->sensor_info->priv;
	struct is_device_sensor *device;

	device = (struct is_device_sensor *)v4l2_get_subdev_hostdata(subdev);
	WARN_ON(!device);

	IXC_MUTEX_LOCK(cis->ixc_lock);
	info("[%s] global setting internal start\n", __func__);
	/* setfile global setting is at camera entrance */
	ret |= sensor_cis_write_registers(subdev, priv->global);
	if (ret < 0) {
		err("sensor_gn3_set_registers fail!!");
		goto p_err;
	}

	info("[%s] global setting internal done\n", __func__);

	if (device->pdata->scramble) { /* default disable */
		ret |= sensor_cis_write_registers(subdev, priv->enable_scramble);
		if (ret < 0) {
			err("sensor_gn3_set_registers fail!!");
			goto p_err;
		}
		info("[%s] enable_scramble\n", __func__);
	}

	if (sensor_cis_support_retention_mode(subdev)) {
#if IS_ENABLED(CIS_CALIBRATION)
		ret |= sensor_gn3_cis_set_cal(subdev);
		ret |= sensor_cis_write_registers(subdev, priv->golden_cal);
		if (ret < 0) {
			err("sensor_gn3_cis_set_cal fail!!");
			goto p_err;
		}
#endif
	}

p_err:
	IXC_MUTEX_UNLOCK(cis->ixc_lock);
	return ret;
}

#if IS_ENABLED(GN3_BURST_WRITE)
int sensor_gn3_cis_write16_burst(void *vclient, u16 addr, u8 *val, u32 num, bool endian)
{
	int ret = 0;
	struct i2c_msg msg[1];
	int i = 0;
	u8 *wbuf;
	struct i2c_client *client = (struct i2c_client *)vclient;

	if (val == NULL) {
		err("val array is null");
		ret = -ENODEV;
		goto p_err;
	}

	if (!client->adapter) {
		err("Could not find adapter!");
		ret = -ENODEV;
		goto p_err;
	}

	wbuf = kmalloc((2 + (num * 2)), GFP_KERNEL);
	if (!wbuf) {
		err("failed to alloc buffer for burst i2c");
		ret = -ENODEV;
		goto p_err;
	}

	msg->addr = client->addr;
	msg->flags = 0;
	msg->len = 2 + (num * 2);
	msg->buf = wbuf;
	wbuf[0] = (addr & 0xFF00) >> 8;
	wbuf[1] = (addr & 0xFF);
	if (endian == GN3_BIG_ENDIAN) {
		for (i = 0; i < num; i++) {
			wbuf[(i * 2) + 2] = (val[(i * 2)] & 0xFF);
			wbuf[(i * 2) + 3] = (val[(i * 2) + 1] & 0xFF);
			ixc_info("I2CW16(%d) [0x%04x] : 0x%02x%02x\n", client->addr, addr, (val[(i * 2)] & 0xFF), (val[(i * 2) + 1] & 0xFF));
		}
	} else {
		for (i = 0; i < num; i++) {
			wbuf[(i * 2) + 2] = (val[(i * 2) + 1] & 0xFF);
			wbuf[(i * 2) + 3] = (val[(i * 2)] & 0xFF);
			ixc_info("I2CW16(%d) [0x%04x] : 0x%02x%02x\n", client->addr, addr, (val[(i * 2)] & 0xFF), (val[(i * 2) + 1] & 0xFF));
		}
	}

	ret = is_i2c_transfer(client->adapter, msg, 1);
	if (ret < 0) {
		err("i2c transfer fail(%d)", ret);
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

#if IS_ENABLED(CIS_CALIBRATION)
int sensor_gn3_cis_set_cal(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_vendor_rom *rom_info = NULL;
	char *cal_buf = NULL;
	bool endian = GN3_BIG_ENDIAN;
	int start_addr = 0, end_addr = 0;
#if IS_ENABLED(GN3_BURST_WRITE)
	int cal_size = 0;
#endif
	int i = 0;
	u16 val = 0;
	int len = 0;
	struct is_cis *cis = sensor_cis_get_cis(subdev);
	struct sensor_gn3_private_data *priv = (struct sensor_gn3_private_data *)cis->sensor_info->priv;
	struct sensor_gn3_private_runtime *priv_runtime = (struct sensor_gn3_private_runtime *)cis->priv_runtime;

	info("[%s] E\n", __func__);

#if IS_ENABLED(USE_CAMERA_SENSOR_RETENTION)
	if (priv_runtime->eeprom_cal_available)
		return 0;
#endif

	is_sec_get_rom_info(&rom_info, ROM_ID_REAR);

	if (!rom_info->read_done) {
		err("eeprom read fail status, skip set cal");
		priv_runtime->eeprom_cal_available = false;
		return 0;
	}

	info("[%s] eeprom read, start set cal\n", __func__);
	cal_buf = rom_info->buf;

#ifdef CAMERA_GN3_CAL_MODULE_VERSION
	if (rom_info->header_ver[10] < CAMERA_GN3_CAL_MODULE_VERSION) {
		start_addr = rom_info->rom_xtc_cal_data_addr_list[GN3_CAL_START_ADDR];
		if (cal_buf[start_addr + 2] == 0xFF && cal_buf[start_addr + 3] == 0xFF &&
			cal_buf[start_addr + 4] == 0xFF && cal_buf[start_addr + 5] == 0xFF &&
			cal_buf[start_addr + 6] == 0xFF && cal_buf[start_addr + 7] == 0xFF &&
			cal_buf[start_addr + 8] == 0xFF && cal_buf[start_addr + 9] == 0xFF &&
			cal_buf[start_addr + 10] == 0xFF && cal_buf[start_addr + 11] == 0xFF &&
			cal_buf[start_addr + 12] == 0xFF && cal_buf[start_addr + 13] == 0xFF &&
			cal_buf[start_addr + 14] == 0xFF && cal_buf[start_addr + 15] == 0xFF &&
			cal_buf[start_addr + 16] == 0xFF && cal_buf[start_addr + 17] == 0xFF) {
			info("empty Cal - cal offset[0x%04X] = val[0x%02X], cal offset[0x%04X] = val[0x%02X]\n",
				start_addr + 2, cal_buf[start_addr + 2], start_addr + 17, cal_buf[start_addr + 17]);
			info("[%s] empty Cal\n", __func__);
			return 0;
		}

		len = (rom_info->rom_xtc_cal_data_addr_list_len / GN3_CAL_ROW_LEN) - 1;
		if (len >= 0) {
			end_addr = rom_info->rom_xtc_cal_data_addr_list[len * GN3_CAL_ROW_LEN + GN3_CAL_END_ADDR];
			if (end_addr >= 15) {
				if (cal_buf[end_addr	] == 0xFF && cal_buf[end_addr - 1] == 0xFF &&
					cal_buf[end_addr - 2] == 0xFF && cal_buf[end_addr - 3] == 0xFF &&
					cal_buf[end_addr - 4] == 0xFF && cal_buf[end_addr - 5] == 0xFF &&
					cal_buf[end_addr - 6] == 0xFF && cal_buf[end_addr - 7] == 0xFF &&
					cal_buf[end_addr - 8] == 0xFF && cal_buf[end_addr - 9] == 0xFF &&
					cal_buf[end_addr - 10] == 0xFF && cal_buf[end_addr - 11] == 0xFF &&
					cal_buf[end_addr - 12] == 0xFF && cal_buf[end_addr - 13] == 0xFF &&
					cal_buf[end_addr - 14] == 0xFF && cal_buf[end_addr - 15] == 0xFF) {
					info("empty Cal - cal offset[0x%04X] = val[0x%02X], cal offset[0x%04X] = val[0x%02X]\n",
						end_addr, cal_buf[end_addr], end_addr - 15, cal_buf[end_addr - 15]);
					info("[%s] empty Cal\n", __func__);
					return 0;
				}
			}
		}
	}
#endif

	if (rom_info->rom_xtc_cal_data_addr_list_len <= 0) {
		err("Not available DT, skip set cal");
		priv_runtime->eeprom_cal_available = false;
		return 0;
	}

	info("[%s] XTC start\n", __func__);
	start_addr = rom_info->rom_xtc_cal_data_addr_list[GN3_CAL_START_ADDR];
	if (rom_info->rom_xtc_cal_endian_check) {
		if (cal_buf[start_addr] == 0xFF && cal_buf[start_addr + 1] == 0x00)
			endian = GN3_BIG_ENDIAN;
		else
			endian = GN3_LITTLE_ENDIAN;

		start_addr = start_addr + 2;
	} else {
		endian = GN3_BIG_ENDIAN;
	}

	for (len = 0; len < rom_info->rom_xtc_cal_data_addr_list_len / GN3_CAL_ROW_LEN; len++) {
		ret |= sensor_cis_write_registers(subdev, priv->pre_XTC);

		dbg_sensor(1, "[%s] XTC Calibration Data E\n", __func__);
		if (len != 0)
			start_addr = rom_info->rom_xtc_cal_data_addr_list[len * GN3_CAL_ROW_LEN + GN3_CAL_START_ADDR];
		end_addr = rom_info->rom_xtc_cal_data_addr_list[len * GN3_CAL_ROW_LEN + GN3_CAL_END_ADDR];

#if IS_ENABLED(GN3_BURST_WRITE)
		if (rom_info->rom_xtc_cal_data_addr_list[len * GN3_CAL_ROW_LEN + GN3_CAL_BURST_CHECK]) {
			cal_size = (end_addr - start_addr) / 2 + 1;
			dbg_sensor(1, "[%s] rom_xtc_cal burst write start(0x%X) end(0x%X) size(%d)\n", __func__, start_addr, end_addr, cal_size);
			ret = sensor_gn3_cis_write16_burst(cis->client, 0x6F12, (u8 *)&cal_buf[start_addr], cal_size, endian);
			if (ret < 0) {
				err("sensor_gn3_cis_write16_burst fail!!");
				goto p_err;
			}
		} else
#endif
		{
			for(i = start_addr; i <= end_addr; i += 2) {
				val = GN3_ENDIAN(cal_buf[i], cal_buf[i + 1], endian);
				ret = cis->ixc_ops->write16(cis->client, 0x6F12, val);
				if (ret < 0) {
					err("cis->ixc_ops->write16 fail!!");
					goto p_err;
				}
#ifdef DEBUG_CAL_WRITE
				info("cal offset[0x%04X] , val[0x%04X]\n", i, val);
#endif
			}
		}

		dbg_sensor(1, "[%s] XTC Calibration Data X\n", __func__);

		ret |= sensor_cis_write_registers(subdev, priv->post_XTC);
	}
	info("[%s] XTC end\n", __func__);

	priv_runtime->eeprom_cal_available = true;

	info("[%s] X\n", __func__);

p_err:
	return ret;
}
#endif

int sensor_gn3_cis_write_aeb_on(struct v4l2_subdev *subdev)
{
	int ret = 0;
	u32 mode = 0;
	const struct sensor_cis_mode_info *mode_info;
	struct is_cis *cis = sensor_cis_get_cis(subdev);
	struct sensor_gn3_private_data *priv;

	priv = (struct sensor_gn3_private_data *)cis->sensor_info->priv;

	mode = cis->cis_data->sens_config_index_cur;
	mode_info = cis->sensor_info->mode_infos[mode];

	ret |= sensor_cis_write_registers(subdev, priv->aeb_on);

	cis->cis_data->fll_margin_ratio_aeb = 2000; /* x2*/

	return ret;
}

int sensor_gn3_cis_write_aeb_off(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_cis *cis = sensor_cis_get_cis(subdev);
	struct sensor_gn3_private_data *priv;

	priv = (struct sensor_gn3_private_data *)cis->sensor_info->priv;

	ret |= sensor_cis_write_registers(subdev, priv->aeb_off);

	return ret;
}

int sensor_gn3_cis_force_aeb_off(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_cis *cis = sensor_cis_get_cis(subdev);
	cis_shared_data *cis_data = cis->cis_data;

	if (cis_data->cur_hdr_mode == SENSOR_HDR_MODE_2AEB_2VC
		&& cis_data->pre_hdr_mode == SENSOR_HDR_MODE_2AEB_2VC) {
		info("[%s] current AEB status is enabled. need AEB disable\n", __func__);
		cis_data->pre_hdr_mode = cis_data->cur_hdr_mode = SENSOR_HDR_MODE_SINGLE;
		ret |= sensor_gn3_cis_write_aeb_off(subdev);
		info("[%s] disable AEB\n", __func__);
	}

	return ret;
}

bool sensor_gn3_cis_check_aeb(struct v4l2_subdev *subdev)
{
	struct is_device_sensor *device;
	struct is_cis *cis = sensor_cis_get_cis(subdev);
	cis_shared_data *cis_data = cis->cis_data;
	u32 mode = cis_data->sens_config_index_cur;
	const struct sensor_cis_mode_info *mode_info = cis->sensor_info->mode_infos[mode];
	struct sensor_gn3_private_data *priv = (struct sensor_gn3_private_data *)cis->sensor_info->priv;
	struct sensor_gn3_private_runtime *priv_runtime = (struct sensor_gn3_private_runtime *)cis->priv_runtime;
	ktime_t st;

	device = (struct is_device_sensor *)v4l2_get_subdev_hostdata(subdev);
	WARN_ON(!device);

	if (cis_data->cur_hdr_mode == SENSOR_HDR_MODE_2AEB_2VC
		&& (cis_data->stream_on == false
			|| !is_sensor_check_aeb_mode(device)
			|| mode_info->remosaic_mode)) {
		info("[%s] Not support AEB\n", __func__);
		cis_data->pre_hdr_mode = cis_data->cur_hdr_mode = SENSOR_HDR_MODE_SINGLE;
		sensor_gn3_cis_write_aeb_off(subdev);
		info("[%s] disable AEB\n", __func__);
		return false;
	}

	if (cis_data->cur_hdr_mode == cis_data->pre_hdr_mode)
		return false;

	if (!priv_runtime->is_frame_duration_on) {
		sensor_cis_write_registers(subdev, priv->frame_duration_on);
		info("[%s] frame_duration_on\n", __func__);
		priv_runtime->is_frame_duration_on = true;
	}

	st = ktime_get();
	if (cis_data->cur_hdr_mode == SENSOR_HDR_MODE_2AEB_2VC) {
		info("[%s][%d] enable AEB\n", __func__, cis_data->sen_vsync_count);
		sensor_gn3_cis_write_aeb_on(subdev);
	} else {
		info("[%s][%d] disable AEB\n", __func__, cis_data->sen_vsync_count);
		sensor_gn3_cis_write_aeb_off(subdev);
	}

	cis_data->pre_hdr_mode = cis_data->cur_hdr_mode;

	info("[%s][%d] done, time %lldus\n",
		__func__, cis_data->sen_vsync_count, PABLO_KTIME_US_DELTA_NOW(st));

	return true;
}

#if IS_ENABLED(USE_CAMERA_SENSOR_RETENTION)
int sensor_gn3_cis_check_12bit(cis_shared_data *cis_data,
								u32 *next_mode,
								u16 *next_fast_change_idx,
								u32 *load_sram_idx)
{
	int ret = 0;

	if (sensor_gn3_mode_groups[SENSOR_GN3_MODE_IDCG] == MODE_GROUP_NONE) {
		ret = -1;
		goto EXIT;
	}

	switch (cis_data->cur_12bit_mode) {
	case SENSOR_12BIT_STATE_REAL_12BIT:
		*next_mode = sensor_gn3_mode_groups[SENSOR_GN3_MODE_IDCG];
		*next_fast_change_idx =
			sensor_gn3_spec_mode_retention_attr[*next_mode].fast_change_idx;
		*load_sram_idx = sensor_gn3_spec_mode_retention_attr[*next_mode].load_sram_idx;
		break;
	case SENSOR_12BIT_STATE_PSEUDO_12BIT:
	default:
		ret = -1;
		break;
	}

EXIT:
	return ret;
}

int sensor_gn3_cis_check_lownoise(cis_shared_data *cis_data,
	u32 *next_mode,
	u16 *next_fast_change_idx,
	u32 *load_sram_idx)
{
	int ret = 0;
	u32 temp_mode = MODE_GROUP_NONE;

	if ((sensor_gn3_mode_groups[SENSOR_GN3_MODE_LN2] == MODE_GROUP_NONE
		&& sensor_gn3_mode_groups[SENSOR_GN3_MODE_LN4] == MODE_GROUP_NONE)) {
		ret = -1;
		goto EXIT;
	}

	switch (cis_data->cur_lownoise_mode) {
	case IS_CIS_LN2:
		temp_mode = sensor_gn3_mode_groups[SENSOR_GN3_MODE_LN2];
		break;
	case IS_CIS_LN4:
		temp_mode = sensor_gn3_mode_groups[SENSOR_GN3_MODE_LN4];
		break;
	case IS_CIS_LNOFF:
	default:
		break;
	}

	if (temp_mode == MODE_GROUP_NONE) {
		ret = -1;
	}

	if (ret == 0) {
		*next_mode = temp_mode;
		*next_fast_change_idx =
			sensor_gn3_spec_mode_retention_attr[*next_mode].fast_change_idx;
		*load_sram_idx = sensor_gn3_spec_mode_retention_attr[*next_mode].load_sram_idx;
	}

EXIT:
	return ret;
}

int sensor_gn3_cis_check_cropped_remosaic(cis_shared_data *cis_data,
	u32 cur_fast_change_idx,
	u32 *next_mode,
	u16 *next_fast_change_idx,
	u32 *load_sram_idx)
{
	int ret = 0;
	u32 zoom_ratio = 0;

	if (sensor_gn3_mode_groups[SENSOR_GN3_MODE_RMS_CROP] == MODE_GROUP_NONE) {
		ret = -1;
		goto EXIT;
	}

	zoom_ratio = cis_data->cur_remosaic_zoom_ratio;

	if (zoom_ratio == SENSOR_GN3_RMS_ZOOM_MODE_NONE || zoom_ratio >= SENSOR_GN3_RMS_ZOOM_MODE_MAX) {
		ret = -1; //goto default
		goto EXIT;
	}

	if (sensor_gn3_rms_zoom_mode[zoom_ratio] == SENSOR_GN3_RMS_ZOOM_MODE_NONE) {
		ret = -1; //goto default
		goto EXIT;
	}

	*next_mode = sensor_gn3_rms_zoom_mode[zoom_ratio];

	if (zoom_ratio == cis_data->pre_remosaic_zoom_ratio) {
		*next_fast_change_idx = cur_fast_change_idx;
		*load_sram_idx = SENSOR_GN3_LOAD_SRAM_IDX_NONE;
		ret = -2; //keep cropped remosaic
		dbg_sensor(1, "%s : cur zoom ratio is same pre zoom ratio (%u)", __func__, zoom_ratio);
		goto EXIT;
	}

	info("%s : zoom_ratio %u turn on cropped remosaic\n", __func__, zoom_ratio);

	if (cur_fast_change_idx == GN3_FCI_NONE) {
		err("fast change idx is none!!");
		ret = -1;
	} else {
		*next_fast_change_idx =
			sensor_gn3_spec_mode_retention_attr[*next_mode].fast_change_idx;
		*load_sram_idx = sensor_gn3_spec_mode_retention_attr[*next_mode].load_sram_idx;
	}

EXIT:
	return ret;
}

int sensor_gn3_cis_update_seamless_mode(struct v4l2_subdev *subdev)
{
	int ret = 0;
	unsigned int mode = 0;
	unsigned int next_mode = 0;
	u16 cur_fast_change_idx = GN3_FCI_NONE;
	u16 next_fast_change_idx = 0;
	u32 load_sram_idx = 0;
	bool is_aeb_changed = false;
	struct is_cis *cis = sensor_cis_get_cis(subdev);
	struct sensor_gn3_private_data *priv = (struct sensor_gn3_private_data *)cis->sensor_info->priv;
	struct sensor_gn3_private_runtime *priv_runtime = (struct sensor_gn3_private_runtime *)cis->priv_runtime;
	ktime_t st = ktime_get();

#if !IS_ENABLED(USE_CAMERA_SENSOR_RETENTION)
	IXC_MUTEX_LOCK(cis->ixc_lock);
	sensor_gn3_cis_check_aeb(subdev);
	IXC_MUTEX_UNLOCK(cis->ixc_lock);

	return ret;
#endif

	mode = cis->cis_data->sens_config_index_cur;

	if (sensor_cis_get_retention_mode(subdev) != SENSOR_RETENTION_ACTIVATED) {
		err("retention is not activated");
		ret = -1;
		return ret;
	}

	next_mode = sensor_gn3_mode_groups[SENSOR_GN3_MODE_NORMAL];
	if (next_mode == MODE_GROUP_NONE) {
		err("mode group is none");
		ret = -1;
		return ret;
	}
	next_fast_change_idx = sensor_gn3_spec_mode_retention_attr[next_mode].fast_change_idx;
	if (next_fast_change_idx == GN3_FCI_NONE) {
		dbg_sensor(1, "[%s] fast_change_idx is none", __func__);
		return ret;
	}

	if (cis->cis_data->cur_pattern_mode != SENSOR_TEST_PATTERN_MODE_OFF) {
		dbg_sensor(1, "[%s] cur_pattern_mode (%d)", __func__, cis->cis_data->cur_pattern_mode);
		return ret;
	}

	load_sram_idx = sensor_gn3_spec_mode_retention_attr[next_mode].load_sram_idx;

	IXC_MUTEX_LOCK(cis->ixc_lock);

	if (cis->cis_data->stream_on == false
		&& cis->cis_data->seamless_ctl_before_stream.mode == 0) {
		info("[%s] skip update seamless mode in stream off\n", __func__);
		goto p_i2c_unlock;
	}

	cis->cis_data->seamless_ctl_before_stream.mode = 0;

	if (sensor_gn3_cis_check_aeb(subdev) == true) {
		is_aeb_changed = true;
		goto p_out;
	}

	sensor_gn3_cis_check_12bit(cis->cis_data,
		&next_mode,
		&next_fast_change_idx,
		&load_sram_idx);

	sensor_gn3_cis_check_lownoise(cis->cis_data,
		&next_mode,
		&next_fast_change_idx,
		&load_sram_idx);

	cis->ixc_ops->write16(cis->client, 0xFCFC, 0x4000);
	cis->ixc_ops->read16(cis->client, 0x0B30, &cur_fast_change_idx);

	sensor_gn3_cis_check_cropped_remosaic(cis->cis_data,
		cur_fast_change_idx,
		&next_mode,
		&next_fast_change_idx,
		&load_sram_idx);

	if ((mode == next_mode && cur_fast_change_idx == next_fast_change_idx)
		|| next_mode == MODE_GROUP_NONE)
		goto p_i2c_unlock;

	sensor_gn3_cis_set_remosaic_crop_shift(subdev, next_mode);

	if (!priv_runtime->is_frame_duration_on) {
		ret |= sensor_cis_write_registers(subdev, priv->frame_duration_on);
		info("[%s] frame_duration_on\n", __func__);
		priv_runtime->is_frame_duration_on = true;
	}

	ret |= cis->ixc_ops->write16(cis->client, 0xFCFC, 0x4000);
	ret |= cis->ixc_ops->write16(cis->client, 0x6000, 0x0005);

	if (cis->cis_data->stream_on == true)
		ret |= cis->ixc_ops->write16(cis->client, 0x0B32, 0x0100);

	ret |= cis->ixc_ops->write16(cis->client, 0x0B30, next_fast_change_idx);
	ret |= cis->ixc_ops->write16(cis->client, 0x6000, 0x0085);
	if (ret < 0)
		err("sensor_gn3_set_registers fail!!");

	cis->cis_data->sens_config_index_pre = cis->cis_data->sens_config_index_cur;
	cis->cis_data->sens_config_index_cur = next_mode;

	CALL_CISOPS(cis, cis_data_calculation, subdev, next_mode);

p_out:
	info("[%s][%d] pre(%d)->cur(%d), 12bit[%d] LN[%d] ZOOM[%d] AEB[%d] => LSI[%d] FCI[0x%x], time %lldus\n",
		__func__, cis->cis_data->sen_vsync_count,
		cis->cis_data->sens_config_index_pre, cis->cis_data->sens_config_index_cur,
		cis->cis_data->cur_12bit_mode,
		cis->cis_data->cur_lownoise_mode,
		cis->cis_data->cur_remosaic_zoom_ratio,
		cis->cis_data->cur_hdr_mode,
		load_sram_idx, next_fast_change_idx,
		PABLO_KTIME_US_DELTA_NOW(st));

	cis->cis_data->seamless_mode_changed = true;

p_i2c_unlock:
	IXC_MUTEX_UNLOCK(cis->ixc_lock);

	return ret;
}
#endif

int sensor_gn3_cis_get_seamless_mode_info(struct v4l2_subdev *subdev)
{
	struct is_cis *cis = sensor_cis_get_cis(subdev);
	cis_shared_data *cis_data = cis->cis_data;
	int ret = 0, cnt = 0;
	u32 mode;

	if (sensor_gn3_mode_groups[SENSOR_GN3_MODE_LN2] != MODE_GROUP_NONE) {
		cis_data->seamless_mode_info[cnt].mode = SENSOR_MODE_LN2;
		sensor_cis_get_mode_info(subdev, sensor_gn3_mode_groups[SENSOR_GN3_MODE_LN2],
			&cis_data->seamless_mode_info[cnt]);
		cnt++;
	}

	if (sensor_gn3_mode_groups[SENSOR_GN3_MODE_LN4] != MODE_GROUP_NONE) {
		cis_data->seamless_mode_info[cnt].mode = SENSOR_MODE_LN4;
		sensor_cis_get_mode_info(subdev, sensor_gn3_mode_groups[SENSOR_GN3_MODE_LN4],
			&cis_data->seamless_mode_info[cnt]);
		cnt++;
	}

	if (sensor_gn3_mode_groups[SENSOR_GN3_MODE_IDCG] != MODE_GROUP_NONE) {
		cis_data->seamless_mode_info[cnt].mode = SENSOR_MODE_REAL_12BIT;
		sensor_cis_get_mode_info(subdev, sensor_gn3_mode_groups[SENSOR_GN3_MODE_IDCG],
			&cis_data->seamless_mode_info[cnt]);
		cnt++;

		mode = sensor_gn3_mode_groups[SENSOR_GN3_MODE_IDCG];
		if (cis->sensor_info->mode_infos[mode]->aeb_support) {
			cis_data->seamless_mode_info[cnt].mode = SENSOR_MODE_REAL_12BIT | SENSOR_MODE_AEB;
			sensor_cis_get_mode_info(subdev, mode, &cis_data->seamless_mode_info[cnt]);
			cnt++;
		}
	}

	if (sensor_gn3_mode_groups[SENSOR_GN3_MODE_RMS_CROP] != MODE_GROUP_NONE) {
		cis_data->seamless_mode_info[cnt].mode = SENSOR_MODE_CROPPED_RMS;
		cis_data->seamless_mode_info[cnt].extra[0] = SENSOR_GN3_RMS_ZOOM_MODE_X_20;
		sensor_cis_get_mode_info(subdev, sensor_gn3_rms_zoom_mode[SENSOR_GN3_RMS_ZOOM_MODE_X_20],
			&cis_data->seamless_mode_info[cnt]);
		cnt++;
	}

	mode = cis->cis_data->sens_config_index_cur;
	if (cis->sensor_info->mode_infos[mode]->aeb_support) {
		cis_data->seamless_mode_info[cnt].mode = SENSOR_MODE_AEB;
		sensor_cis_get_mode_info(subdev, mode, &cis_data->seamless_mode_info[cnt]);
		cnt++;
	}

	cis_data->seamless_mode_cnt = cnt;

	return ret;
}

int sensor_gn3_cis_mode_change(struct v4l2_subdev *subdev, u32 mode)
{
	int ret = 0;
	u32 ex_mode;
	u16 fast_change_idx = 0x00FF;
#if IS_ENABLED(USE_CAMERA_SENSOR_RETENTION)
	int load_sram_idx = SENSOR_GN3_LOAD_SRAM_IDX_NONE;
	u32 seamless_mode = 0;
	int i = 0;
#endif
	const struct sensor_cis_mode_info *mode_info;
	struct is_device_sensor *device;
	struct is_cis *cis = sensor_cis_get_cis(subdev);
	cis_shared_data *cis_data = cis->cis_data;
	struct sensor_gn3_private_data *priv = (struct sensor_gn3_private_data *)cis->sensor_info->priv;
	struct sensor_gn3_private_runtime *priv_runtime = (struct sensor_gn3_private_runtime *)cis->priv_runtime;

	device = (struct is_device_sensor *)v4l2_get_subdev_hostdata(subdev);
	WARN_ON(!device);

	if (mode >= cis->sensor_info->mode_count) {
		err("invalid mode(%d)!!", mode);
		ret = -EINVAL;
		goto p_err;
	}

	sensor_gn3_cis_set_mode_group(mode);
	sensor_gn3_cis_get_seamless_mode_info(subdev);
	sensor_gn3_cis_init_private(subdev);

	IXC_MUTEX_LOCK(cis->ixc_lock);

	ex_mode = is_sensor_g_ex_mode(device);

	info("[%s] sensor mode(%d) ex_mode(%d)\n", __func__, mode, ex_mode);

	mode_info = cis->sensor_info->mode_infos[mode];

#if IS_ENABLED(USE_CAMERA_SENSOR_RETENTION)
	/* Retention mode sensor mode select */
	if (sensor_cis_get_retention_mode(subdev) == SENSOR_RETENTION_ACTIVATED) {
		priv_runtime->load_retention = false;

		fast_change_idx = sensor_gn3_spec_mode_retention_attr[mode].fast_change_idx;

		if (fast_change_idx != GN3_FCI_NONE) {
			if(mode != SENSOR_GN3_2040X1148_480FPS) {
				ret |= cis->ixc_ops->write16(cis->client, 0xFCFC, 0x4000);
				ret |= cis->ixc_ops->write16(cis->client, 0x6000, 0x0005);
				ret |= cis->ixc_ops->write16(cis->client, 0x0118, 0x0000);
				ret |= cis->ixc_ops->write16(cis->client, 0x0A52, 0x0000);
				ret |= cis->ixc_ops->write16(cis->client, 0xFCFC, 0x2001);
				ret |= cis->ixc_ops->write16(cis->client, 0x1130, 0x0000);
				ret |= cis->ixc_ops->write16(cis->client, 0x5670, 0x0000);
			}

			for (i = 0; i < SENSOR_GN3_MODE_MODE_GROUP_MAX; i++) {
				seamless_mode = sensor_gn3_mode_groups[i];
				if (seamless_mode == MODE_GROUP_RMS)
					seamless_mode = sensor_gn3_rms_zoom_mode[SENSOR_GN3_RMS_ZOOM_MODE_X_20];
				else if (seamless_mode == MODE_GROUP_NONE)
					continue;

				load_sram_idx = sensor_gn3_spec_mode_retention_attr[seamless_mode].load_sram_idx;
				if (load_sram_idx != SENSOR_GN3_LOAD_SRAM_IDX_NONE) {
					ret = sensor_cis_write_registers(subdev, priv->load_sram[load_sram_idx]);
					if (ret < 0) {
						err("sensor_gn3_set_registers fail!!");
						goto p_err_i2c_unlock;
					}
				}
			}

			if(mode == SENSOR_GN3_2040X1148_480FPS) {
				info("[%s]SSM additional setting\n", __func__);
				ret |= cis->ixc_ops->write16(cis->client, 0xFCFC, 0x4000);
				ret |= cis->ixc_ops->write16(cis->client, 0x0340, 0x2D20);
				ret |= cis->ixc_ops->write16(cis->client, 0x0118, 0x0101);
				ret |= cis->ixc_ops->write16(cis->client, 0x0A52, 0x0001);
				ret |= cis->ixc_ops->write16(cis->client, 0xFCFC, 0x2001);
				ret |= cis->ixc_ops->write16(cis->client, 0x1130, 0x0003);
				ret |= cis->ixc_ops->write16(cis->client, 0x5670, 0x0001);
			}

			ret |= cis->ixc_ops->write16(cis->client, 0xFCFC, 0x4000);
			ret |= cis->ixc_ops->write16(cis->client, 0x0B30, fast_change_idx);
			ret |= cis->ixc_ops->write16(cis->client, 0x6000, 0x0085);

			if (ret < 0) {
				err("sensor_gn3_set_registers fail!!");
				goto p_err_i2c_unlock;
			}
		} else {
			info("[%s] not support retention sensor mode(%d)\n", __func__, mode);

			//Fast Change disable
			ret |= cis->ixc_ops->write16(cis->client, 0xFCFC, 0x4000);
			ret |= cis->ixc_ops->write16(cis->client, 0x6000, 0x0005);
			ret |= cis->ixc_ops->write16(cis->client, 0x0B30, 0x00FF);

			ret |= sensor_cis_write_registers(subdev, mode_info->setfile);

			if (ret < 0) {
				err("sensor_gn3_set_registers fail!!");
				goto p_err_i2c_unlock;
			}
		}
	} else
#endif
	{
		info("[%s] not support retention sensor mode(%d)\n", __func__, mode);

		//Fast Change disable
		ret |= cis->ixc_ops->write16(cis->client, 0xFCFC, 0x4000);
		ret |= cis->ixc_ops->write16(cis->client, 0x6000, 0x0005);
		ret |= cis->ixc_ops->write16(cis->client, 0x0B30, 0x00FF);

		ret |= sensor_cis_write_registers(subdev, mode_info->setfile);

		if (ret < 0) {
			err("sensor_gn3_set_registers fail!!");
			goto p_err_i2c_unlock;
		}
	}

	cis_data->sens_config_index_pre = mode;

	if (is_sensor_check_aeb_mode(device)) {
		if (mode_info->state_12bit != SENSOR_12BIT_STATE_OFF) {
			ret |= sensor_cis_write_registers(subdev, priv->aeb_12bit_image_dt);
			if (mode_info->state_12bit == SENSOR_12BIT_STATE_PSEUDO_12BIT)
				ret |= sensor_cis_write_registers(subdev, priv->aeb_init_perframe_normal);
			else
				ret |= sensor_cis_write_registers(subdev, priv->aeb_init_perframe_idcg);
		} else {
			ret |= sensor_cis_write_registers(subdev, priv->aeb_10bit_image_dt);
			ret |= sensor_cis_write_registers(subdev, priv->aeb_init_perframe_normal);
		}
	}

	ret |= cis->ixc_ops->write16(cis->client, 0xFCFC, 0x4000);
	ret |= cis->ixc_ops->write16(cis->client, 0x6028, 0x2000);
	ret |= cis->ixc_ops->write16(cis->client, 0x602A, 0xEB60);
	if (ex_mode == EX_AI_REMOSAIC) {
		ret |= cis->ixc_ops->write16(cis->client, 0x6F12, 0x0000); // Remosaic off - Tetra Pattern
	} else {
		ret |= cis->ixc_ops->write16(cis->client, 0x6F12, 0x0200); // Remosaic on - Bayer Pattern
	}

	cis_data->remosaic_mode = mode_info->remosaic_mode;

	cis_data->pre_lownoise_mode = IS_CIS_LNOFF;
	cis_data->pre_12bit_mode = mode_info->state_12bit;
	cis_data->pre_remosaic_zoom_ratio = 0;
	cis_data->pre_hdr_mode = SENSOR_HDR_MODE_SINGLE;

	if (cis_data->seamless_ctl_before_stream.mode & SENSOR_MODE_LN2)
		cis_data->cur_lownoise_mode = IS_CIS_LN2;
	else if (cis_data->seamless_ctl_before_stream.mode & SENSOR_MODE_LN4)
		cis_data->cur_lownoise_mode = IS_CIS_LN4;
	else
		cis_data->cur_lownoise_mode = IS_CIS_LNOFF;

	if (cis_data->seamless_ctl_before_stream.mode & SENSOR_MODE_REAL_12BIT)
		cis_data->cur_12bit_mode = SENSOR_12BIT_STATE_REAL_12BIT;
	else
		cis_data->cur_12bit_mode = mode_info->state_12bit;

	if (cis_data->seamless_ctl_before_stream.mode & SENSOR_MODE_CROPPED_RMS)
		cis_data->cur_remosaic_zoom_ratio =
			cis_data->seamless_ctl_before_stream.remosaic_crop_zoom_ratio;
	else
		cis_data->cur_remosaic_zoom_ratio = 0;

	if (cis_data->seamless_ctl_before_stream.mode & SENSOR_MODE_AEB)
		cis_data->cur_hdr_mode = SENSOR_HDR_MODE_2AEB_2VC;
	else
		cis_data->cur_hdr_mode = SENSOR_HDR_MODE_SINGLE;

	ret |= sensor_gn3_cis_write_aeb_off(subdev);

#if IS_ENABLED(USE_CAMERA_SENSOR_RETENTION)
	info("[%s] mode[%d] 12bit[%d] LN[%d] ZOOM[%d] AEB[%d] => load_sram_idx[%d] fast_change_idx[0x%x]\n",
		__func__, mode,
		cis_data->cur_12bit_mode,
		cis_data->cur_lownoise_mode,
		cis_data->cur_remosaic_zoom_ratio,
		cis_data->cur_hdr_mode,
		load_sram_idx,
		fast_change_idx);
#else
	info("[%s] mode[%d] 12bit[%d] LN[%d] ZOOM[%d] AEB[%d] => fast_change_idx[0x%x]\n",
		__func__, mode,
		cis_data->cur_12bit_mode,
		cis_data->cur_lownoise_mode,
		cis_data->cur_remosaic_zoom_ratio,
		cis_data->cur_hdr_mode,
		fast_change_idx);
#endif

p_err_i2c_unlock:
	IXC_MUTEX_UNLOCK(cis->ixc_lock);

p_err:
	/* sensor_gn3_cis_log_status(subdev); */

	return ret;
}

int sensor_gn3_cis_set_global_setting(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_cis *cis = sensor_cis_get_cis(subdev);
	struct sensor_gn3_private_runtime *priv_runtime = (struct sensor_gn3_private_runtime *)cis->priv_runtime;

#if IS_ENABLED(USE_CAMERA_SENSOR_RETENTION)
	u32 retention_mode = sensor_cis_get_retention_mode(subdev);

	/* setfile global setting is at camera entrance */
	if (retention_mode == SENSOR_RETENTION_INACTIVE) {
		sensor_gn3_cis_set_global_setting_internal(subdev);
		sensor_gn3_cis_retention_prepare(subdev);
	} else if (retention_mode == SENSOR_RETENTION_ACTIVATED) {
		sensor_gn3_cis_retention_crc_check(subdev);
	} else { /* SENSOR_RETENTION_UNSUPPORTED */
		priv_runtime->eeprom_cal_available = false;
		sensor_gn3_cis_set_global_setting_internal(subdev);
	}
#else
	info("[%s] global setting start\n", __func__);
	/* setfile global setting is at camera entrance */
	ret = sensor_gn3_cis_set_global_setting_internal(subdev);
	if (ret < 0) {
		err("sensor_gn3_set_registers fail!!");
		goto p_err;
	}
	info("[%s] global setting done\n", __func__);
#endif

#if IS_ENABLED(USE_CAMERA_SENSOR_RETENTION)
#else
p_err:
#endif
	return ret;
}


#if IS_ENABLED(USE_CAMERA_SENSOR_RETENTION)
int sensor_gn3_cis_retention_prepare(struct v4l2_subdev *subdev)
{
	int ret = 0;
	int i = 0;
	u32 wait_cnt = 0, time_out_cnt = 500;
	u16 check = 0;
	const struct sensor_cis_mode_info *mode_info;
	struct is_cis *cis = sensor_cis_get_cis(subdev);
	struct sensor_gn3_private_data *priv = (struct sensor_gn3_private_data *)cis->sensor_info->priv;
	struct sensor_gn3_private_runtime *priv_runtime = (struct sensor_gn3_private_runtime *)cis->priv_runtime;

#if IS_ENABLED(CIS_CALIBRATION)
	IXC_MUTEX_LOCK(cis->ixc_lock);
	ret |= sensor_gn3_cis_set_cal(subdev);
	ret |= sensor_cis_write_registers(subdev, priv->golden_cal);
	IXC_MUTEX_UNLOCK(cis->ixc_lock);
	if (ret < 0) {
		err("sensor_gn3_cis_set_cal fail!!");
		goto p_err;
	}
#endif

	for (i = 0; i < priv->max_retention_num; i++) {
		IXC_MUTEX_LOCK(cis->ixc_lock);
		ret = sensor_cis_write_registers(subdev, priv->retention[i]);
		IXC_MUTEX_UNLOCK(cis->ixc_lock);
		if (ret < 0) {
			err("sensor_gn3_set_registers fail!!");
			goto p_err;
		}
	}

	//FAST AE Setting
	IXC_MUTEX_LOCK(cis->ixc_lock);
	mode_info = cis->sensor_info->mode_infos[SENSOR_GN3_2040X1532_120FPS];
	ret = sensor_cis_write_registers(subdev, mode_info->setfile);
	IXC_MUTEX_UNLOCK(cis->ixc_lock);
	if (ret < 0) {
		err("sensor_gn3_set_registers fail!!");
		goto p_err;
	}

	IXC_MUTEX_LOCK(cis->ixc_lock);
	ret |= cis->ixc_ops->write16(cis->client, 0xFCFC, 0x4000);
	ret |= cis->ixc_ops->write16(cis->client, 0x6000, 0x0005);
	ret |= cis->ixc_ops->write16(cis->client, 0xFCFC, 0x2000);
	ret |= cis->ixc_ops->write16(cis->client, 0x0F74, 0x0000);
	ret |= cis->ixc_ops->write16(cis->client, 0x0F78, 0x2002);
	ret |= cis->ixc_ops->write16(cis->client, 0x0F7A, 0x2002);
	ret |= cis->ixc_ops->write16(cis->client, 0x0F7C, 0x2002);
	ret |= cis->ixc_ops->write16(cis->client, 0x0F7E, 0x2002);
	ret |= cis->ixc_ops->write16(cis->client, 0x0F80, 0x2002);
	ret |= cis->ixc_ops->write16(cis->client, 0x0F82, 0x2002);
	ret |= cis->ixc_ops->write16(cis->client, 0x0F84, 0x2002);
	ret |= cis->ixc_ops->write16(cis->client, 0x0F86, 0x2002);
	ret |= cis->ixc_ops->write16(cis->client, 0x0F88, 0x2002);
	ret |= cis->ixc_ops->write16(cis->client, 0x0F98, 0x5920);
	ret |= cis->ixc_ops->write16(cis->client, 0x0F9A, 0x5FC4);
	ret |= cis->ixc_ops->write16(cis->client, 0x0F9C, 0x6668);
	ret |= cis->ixc_ops->write16(cis->client, 0x0F9E, 0x6D0C);
	ret |= cis->ixc_ops->write16(cis->client, 0x0FA0, 0x73B0);
	ret |= cis->ixc_ops->write16(cis->client, 0x0FA2, 0x7A54);
	ret |= cis->ixc_ops->write16(cis->client, 0x0FA4, 0xA3D0);
	ret |= cis->ixc_ops->write16(cis->client, 0x0FA6, 0xAA74);
	ret |= cis->ixc_ops->write16(cis->client, 0x0FA8, 0xB118);
	ret |= cis->ixc_ops->write16(cis->client, 0xFCFC, 0x4000);
	ret |= cis->ixc_ops->write16(cis->client, 0x0B30, 0x01FF);
	ret |= cis->ixc_ops->write16(cis->client, 0x6000, 0x0085);
	IXC_MUTEX_UNLOCK(cis->ixc_lock);

	if (priv_runtime->need_stream_on_retention) {
		IXC_MUTEX_LOCK(cis->ixc_lock);
		ret |= cis->ixc_ops->write16(cis->client, 0x0100, 0x0103); //stream on
		IXC_MUTEX_UNLOCK(cis->ixc_lock);

		sensor_gn3_cis_wait_streamon(subdev);
		IXC_MUTEX_LOCK(cis->ixc_lock);
		ret |= cis->ixc_ops->write16(cis->client, 0x010E, 0x0100); //Retention checksum enable
		IXC_MUTEX_UNLOCK(cis->ixc_lock);

		IXC_MUTEX_LOCK(cis->ixc_lock);
		ret |= cis->ixc_ops->write16(cis->client, 0x0100, 0x0003); //stream off
		IXC_MUTEX_UNLOCK(cis->ixc_lock);

		sensor_gn3_cis_wait_streamoff(subdev);
		priv_runtime->need_stream_on_retention = false;
	}

	if (ret < 0) {
		err("cis->ixc_ops->write fail!!");
		goto p_err;
	}

	IXC_MUTEX_LOCK(cis->ixc_lock);
	ret |= cis->ixc_ops->read16(cis->client, 0x19C4, &check);
	IXC_MUTEX_UNLOCK(cis->ixc_lock);

	while (check != 0x0100) {
		usleep_range(500, 500);
		wait_cnt++;
		IXC_MUTEX_LOCK(cis->ixc_lock);
		ret |= cis->ixc_ops->read16(cis->client, 0x19C4, &check);
		IXC_MUTEX_UNLOCK(cis->ixc_lock);
		if (wait_cnt >= time_out_cnt) {
			err("check 0x19C4 (0x%x) fail!!", check);
			goto p_err;
		}
	}

	sensor_cis_set_retention_mode(subdev, SENSOR_RETENTION_ACTIVATED);

	info("[%s] retention sensor RAM write done\n", __func__);

p_err:

	return ret;
}

int sensor_gn3_cis_retention_crc_check(struct v4l2_subdev *subdev)
{
	int ret = 0;
	u16 crc_check = 0;
	u32 wait_cnt = 0, time_out_cnt = 15;
	u16 check = 0;
	struct is_cis *cis = sensor_cis_get_cis(subdev);
	struct sensor_gn3_private_runtime *priv_runtime = (struct sensor_gn3_private_runtime *)cis->priv_runtime;

	IXC_MUTEX_LOCK(cis->ixc_lock);
	cis->ixc_ops->write16(cis->client, 0xFCFC, 0x4000);
	cis->ixc_ops->write16(cis->client, 0x6000, 0x0005);
	cis->ixc_ops->read16(cis->client, 0x010E, &check);
	IXC_MUTEX_UNLOCK(cis->ixc_lock);

	while (check != 0x0000) {
		usleep_range(1500, 1500);
		wait_cnt++;
		IXC_MUTEX_LOCK(cis->ixc_lock);
		ret |= cis->ixc_ops->read16(cis->client, 0x010E, &check);
		IXC_MUTEX_UNLOCK(cis->ixc_lock);
		if (wait_cnt >= time_out_cnt) {
			err("check 0x010E (0x%x) fail!!", check);
			break;
		}
	}

	IXC_MUTEX_LOCK(cis->ixc_lock);
	/* retention mode CRC check */
	cis->ixc_ops->read16(cis->client, 0x19C2, &crc_check); /* api_ro_checksum_on_ram_passed */

	IXC_MUTEX_UNLOCK(cis->ixc_lock);

	if (crc_check == 0x0100) {
		info("[%s] retention SRAM CRC check: pass!\n", __func__);

		/* init pattern */
		cis->ixc_ops->write16(cis->client, 0x0600, 0x0000);
		cis->ixc_ops->write16(cis->client, 0x0620, 0x0000);
	} else {
		info("[%s] retention SRAM CRC check: fail!\n", __func__);

		CALL_CISOPS(cis, cis_log_status, subdev);

		info("retention CRC Check register value: 0x%x\n", crc_check);
		info("[%s] rewrite retention modes to SRAM\n", __func__);

		ret = sensor_gn3_cis_set_global_setting_internal(subdev);
		if (ret < 0) {
			err("CRC error recover: rewrite sensor global setting failed");
			goto p_err;
		}

		priv_runtime->eeprom_cal_available = false;

		ret = sensor_gn3_cis_retention_prepare(subdev);
		if (ret < 0) {
			err("CRC error recover: retention prepare failed");
			goto p_err;
		}
	}

p_err:
	return ret;
}
#endif

void sensor_gn3_cis_set_remosaic_crop_shift_info(struct v4l2_subdev *subdev, s32 offsetX, s32 offsetY)
{
	struct is_cis *cis = sensor_cis_get_cis(subdev);
	struct sensor_gn3_private_runtime *priv_runtime = (struct sensor_gn3_private_runtime *)cis->priv_runtime;

	priv_runtime->crop_shift_offset_x = offsetX;
	priv_runtime->crop_shift_offset_y = offsetY;

	priv_runtime->crop_shift_enabled = true;

	dbg_sensor(1, "[%s] crop_shift_enabled, offset(%d, %d)\n", __func__,
		offsetX, offsetY);
}

int sensor_gn3_cis_set_remosaic_crop_shift(struct v4l2_subdev *subdev, u32 mode)
{
	int ret = 0;
	struct is_cis *cis = sensor_cis_get_cis(subdev);
	ktime_t st = ktime_get();
	bool is_bds = false;
	bool is_4_3_ratio = false;
	u32 start_x = 0, start_y = 0;
	u32 flip_start_x = 0, flip_start_y = 0;
	u32 image_x_start, image_y_start, image_x_end, image_y_end;
	u32 x_addr_start, y_addr_start, x_addr_end, y_addr_end;
	u32 crop_x_offset, crop_y_offset;
	struct sensor_gn3_private_runtime *priv_runtime = (struct sensor_gn3_private_runtime *)cis->priv_runtime;

	if (!priv_runtime->crop_shift_enabled) {
		dbg_sensor(1, "[%s] crop_shift_disabled\n", __func__);
		return 0;
	}

	if (priv_runtime->crop_shift_offset_x == 0 && priv_runtime->crop_shift_offset_y == 0) {
		dbg_sensor(1, "[%s] offsetXY is 0\n", __func__);
		return 0;
	}

	switch (mode) {
#if GN3_RMS_CROP_SUPPORT_BDS_ON
	case SENSOR_GN3_4080X3060_30FPS_CROP_R10_BDS:
	case SENSOR_GN3_4080X3060_30FPS_CROP_R12_BDS:
		is_4_3_ratio = true;
		fallthrough;
	case SENSOR_GN3_4080X2296_30FPS_CROP_R12_BDS:
		is_bds = true;
		break;
#endif
	case SENSOR_GN3_4080X3060_30FPS_CROP_R10:
	case SENSOR_GN3_4080X3060_30FPS_CROP_R12:
		is_4_3_ratio = true;
		fallthrough;
	case SENSOR_GN3_4080X2296_60FPS_CROP_R10:
	case SENSOR_GN3_4080X2296_60FPS_CROP_R12:
	case SENSOR_GN3_3840X2160_60FPS_CROP_R12:
		break;
	default:
		cis->cis_data->crop_x = 0;
		cis->cis_data->crop_y = 0;
		dbg_sensor(1, "[%s] not RMS mode(%d)\n", __func__, mode);
		return 0;
	}

	info("[%s] mode(%d), is_4_3_ratio(%d), is_bds(%d), offsetXY(%d, %d)\n", __func__, mode,
		is_4_3_ratio, is_bds, priv_runtime->crop_shift_offset_x, priv_runtime->crop_shift_offset_y);

	if (is_4_3_ratio) {
		start_x = sensor_gn3_rms_crop_info.start_x_4_3 * sensor_gn3_rms_crop_info.multiplier + priv_runtime->crop_shift_offset_x;
		start_y = sensor_gn3_rms_crop_info.start_y_4_3 * sensor_gn3_rms_crop_info.multiplier + priv_runtime->crop_shift_offset_y;
		flip_start_x = sensor_gn3_rms_crop_info.max_x_4_3 * sensor_gn3_rms_crop_info.multiplier - start_x;
		flip_start_y = sensor_gn3_rms_crop_info.max_y_4_3 * sensor_gn3_rms_crop_info.multiplier - start_y;
	} else {
		start_x = sensor_gn3_rms_crop_info.start_x_16_9 * sensor_gn3_rms_crop_info.multiplier + priv_runtime->crop_shift_offset_x;
		start_y = sensor_gn3_rms_crop_info.start_y_16_9 * sensor_gn3_rms_crop_info.multiplier + priv_runtime->crop_shift_offset_y;
		flip_start_x = sensor_gn3_rms_crop_info.max_x_16_9 * sensor_gn3_rms_crop_info.multiplier - start_x;
		flip_start_y = sensor_gn3_rms_crop_info.max_y_16_9 * sensor_gn3_rms_crop_info.multiplier - start_y;
	}

	cis->cis_data->crop_x = start_x / sensor_gn3_rms_crop_info.multiplier;
	cis->cis_data->crop_y = start_y / sensor_gn3_rms_crop_info.multiplier;

	if (cis->cis_data->crop_x > sensor_gn3_rms_crop_info.max_x_4_3
		|| (is_4_3_ratio && cis->cis_data->crop_y > sensor_gn3_rms_crop_info.max_y_4_3)
		|| (!is_4_3_ratio && cis->cis_data->crop_y > sensor_gn3_rms_crop_info.max_y_16_9)) {
		info("[%s] skip crop shift\n", __func__);

		/* remosaic crop shift center */
		if (is_4_3_ratio) {
			cis->cis_data->crop_x = sensor_gn3_rms_crop_info.start_x_4_3;
			cis->cis_data->crop_y = sensor_gn3_rms_crop_info.start_y_4_3;
		} else {
			cis->cis_data->crop_x = sensor_gn3_rms_crop_info.start_x_16_9;
			cis->cis_data->crop_y = sensor_gn3_rms_crop_info.start_y_16_9;
		}
		return 0;
	}

	info("[%s] start_xy(%d, %d), is_4_3_ratio(%d), is_bds(%d)\n",
		__func__, start_x, start_y, is_4_3_ratio, is_bds);

	if (is_4_3_ratio) {
		image_x_start = sensor_gn3_rms_crop_info.x_4_3 * sensor_gn3_rms_crop_info.multiplier
			+ (flip_start_x * sensor_gn3_rms_crop_info.w_4x2);
		image_x_start /= sensor_gn3_rms_crop_info.multiplier;
		image_y_start = sensor_gn3_rms_crop_info.y_4_3 * sensor_gn3_rms_crop_info.multiplier
			+ (flip_start_y * sensor_gn3_rms_crop_info.h_4x2);
		image_y_start /= sensor_gn3_rms_crop_info.multiplier;
		image_x_end = image_x_start + sensor_gn3_rms_crop_info.x_4_3_size;
		image_y_end = image_y_start + sensor_gn3_rms_crop_info.y_4_3_size;
	} else {
		image_x_start = sensor_gn3_rms_crop_info.x_16_9 * sensor_gn3_rms_crop_info.multiplier
			+ (flip_start_x * sensor_gn3_rms_crop_info.w_4x2)
			+ sensor_gn3_rms_crop_info.x_min * sensor_gn3_rms_crop_info.multiplier;
		image_x_start /= sensor_gn3_rms_crop_info.multiplier;
		image_y_start = sensor_gn3_rms_crop_info.y_16_9 * sensor_gn3_rms_crop_info.multiplier
			+ (flip_start_y * sensor_gn3_rms_crop_info.h_4x2);
		image_y_start /= sensor_gn3_rms_crop_info.multiplier;
		image_x_end = image_x_start + sensor_gn3_rms_crop_info.x_16_9_size;
		image_y_end = image_y_start + sensor_gn3_rms_crop_info.y_16_9_size;
	}

	x_addr_start = (image_x_start / sensor_gn3_rms_crop_info.align) * sensor_gn3_rms_crop_info.align;
	y_addr_start = (image_y_start / sensor_gn3_rms_crop_info.align) * sensor_gn3_rms_crop_info.align;
	x_addr_end = ceil((image_x_end + 1), sensor_gn3_rms_crop_info.align) * sensor_gn3_rms_crop_info.align - 1;
	y_addr_end = ((image_y_end + 1) / sensor_gn3_rms_crop_info.align) * sensor_gn3_rms_crop_info.align - 1;

	crop_x_offset = (x_addr_end - image_x_end) / 2;
	crop_y_offset = y_addr_end - image_y_end + sensor_gn3_rms_crop_info.crop_y_offset_margin;


	dbg_sensor(1, "[%s] start(%d, %d) -> flip_start(%d, %d)\n", __func__,
		start_x / sensor_gn3_rms_crop_info.multiplier, start_y / sensor_gn3_rms_crop_info.multiplier,
		flip_start_x / sensor_gn3_rms_crop_info.multiplier, flip_start_y / sensor_gn3_rms_crop_info.multiplier);
	dbg_sensor(1, "[%s] image_start(%d, %d), image_end(%d, %d)\n",
		__func__, image_x_start, image_y_start,
		image_x_end, image_y_end);
	dbg_sensor(1, "[%s] addr_start(%d, %d), addr_end(%d, %d)\n",
		__func__, x_addr_start, y_addr_start, x_addr_end, y_addr_end);

	ret |= cis->ixc_ops->write16(cis->client,
		sensor_gn3_rms_crop_info.page_set, sensor_gn3_rms_crop_info.default_page);
	ret |= cis->ixc_ops->write16(cis->client,
		sensor_gn3_rms_crop_info.indirect_page_set, sensor_gn3_rms_crop_info.page);
	if (is_bds)
		ret |= cis->ixc_ops->write16(cis->client,
			sensor_gn3_rms_crop_info.indirect_addr_set, sensor_gn3_rms_crop_info.bds_addr);
	else
		ret |= cis->ixc_ops->write16(cis->client,
			sensor_gn3_rms_crop_info.indirect_addr_set, sensor_gn3_rms_crop_info.bds_off_addr);

	ret |= cis->ixc_ops->write16(cis->client,
		sensor_gn3_rms_crop_info.indirect_access, sensor_gn3_rms_crop_info.x_start_addr);
	ret |= cis->ixc_ops->write16(cis->client,
		sensor_gn3_rms_crop_info.indirect_access, x_addr_start);
	ret |= cis->ixc_ops->write16(cis->client,
		sensor_gn3_rms_crop_info.indirect_access, sensor_gn3_rms_crop_info.y_start_addr);
	ret |= cis->ixc_ops->write16(cis->client,
		sensor_gn3_rms_crop_info.indirect_access, y_addr_start);
	ret |= cis->ixc_ops->write16(cis->client,
		sensor_gn3_rms_crop_info.indirect_access, sensor_gn3_rms_crop_info.x_end_addr);
	ret |= cis->ixc_ops->write16(cis->client,
		sensor_gn3_rms_crop_info.indirect_access, x_addr_end);
	ret |= cis->ixc_ops->write16(cis->client,
		sensor_gn3_rms_crop_info.indirect_access, sensor_gn3_rms_crop_info.y_end_addr);
	ret |= cis->ixc_ops->write16(cis->client,
		sensor_gn3_rms_crop_info.indirect_access, y_addr_end);

	ret |= cis->ixc_ops->write16(cis->client,
		sensor_gn3_rms_crop_info.indirect_access, sensor_gn3_rms_crop_info.x_crop_offset_addr);
	ret |= cis->ixc_ops->write16(cis->client,
		sensor_gn3_rms_crop_info.indirect_access, crop_x_offset);
	ret |= cis->ixc_ops->write16(cis->client,
		sensor_gn3_rms_crop_info.indirect_access, sensor_gn3_rms_crop_info.y_crop_offset_addr);
	ret |= cis->ixc_ops->write16(cis->client,
		sensor_gn3_rms_crop_info.indirect_access, crop_y_offset);

	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %lldus", __func__, PABLO_KTIME_US_DELTA_NOW(st));

	return ret;
}

int sensor_gn3_cis_wait_streamon(struct v4l2_subdev *subdev)
{
	int ret = 0;
	int retry_count = 0;
	int max_retry_count = 10;
	struct is_cis *cis = sensor_cis_get_cis(subdev);
	struct sensor_gn3_private_runtime *priv_runtime = (struct sensor_gn3_private_runtime *)cis->priv_runtime;

	info("%s : E\n", __func__);

	if (priv_runtime->need_stream_on_retention) {
		for (retry_count = 0; retry_count < max_retry_count; retry_count++) {
			ret = sensor_cis_wait_streamon(subdev);
			if (ret < 0) {
				err("wait failed retry %d", retry_count);
			} else {
				break;
			}
		}
		if (ret < 0)
			goto p_err;
	} else {
		ret = sensor_cis_wait_streamon(subdev);
		if (ret < 0)
			goto p_err;
	}

p_err:
	info("%s : X ret[%d] retry_count[%d]\n", __func__, ret, retry_count);

	return ret;
}

int sensor_gn3_cis_wait_streamoff(struct v4l2_subdev *subdev)
{
	int ret = 0;
#if IS_ENABLED(USE_CAMERA_SENSOR_RETENTION)
	u32 wait_cnt = 0, time_out_cnt = 4;
	u16 check = 0;
	struct is_cis *cis = sensor_cis_get_cis(subdev);
#endif

	info("%s : E\n", __func__);

	ret = sensor_cis_wait_streamoff(subdev);
	if (ret < 0)
		goto p_err;

#if IS_ENABLED(USE_CAMERA_SENSOR_RETENTION)

	IXC_MUTEX_LOCK(cis->ixc_lock);
	ret |= cis->ixc_ops->read16(cis->client, 0x19C4, &check);
	IXC_MUTEX_UNLOCK(cis->ixc_lock);

	wait_cnt = 0;
	while (check != 0x0100) {
		usleep_range(500, 500);
		wait_cnt++;
		IXC_MUTEX_LOCK(cis->ixc_lock);
		ret |= cis->ixc_ops->read16(cis->client, 0x19C4, &check);
		IXC_MUTEX_UNLOCK(cis->ixc_lock);
		if (wait_cnt >= time_out_cnt) {
			err("check 0x19C4 (0x%x) fail!!", check);
			goto p_err;
		}
	}
#endif

p_err:
#if IS_ENABLED(USE_CAMERA_SENSOR_RETENTION)
	info("%s : X, ret[%d] wait_cnt[%d]\n", __func__, ret, wait_cnt);
#else
	info("%s : X, ret[%d]\n", __func__, ret);
#endif

	return ret;
}

bool sensor_gn3_cis_check_frame_duration_on(struct v4l2_subdev *subdev)
{
	struct is_device_sensor *device;
	cis_shared_data *cis_data;
	struct is_cis *cis = sensor_cis_get_cis(subdev);

	device = (struct is_device_sensor *)v4l2_get_subdev_hostdata(subdev);
	WARN_ON(!device);

	cis_data = cis->cis_data;

	if (cis_data->video_mode == true)
		return true;

	if (cis_data->is_data.op_mode != CAMERA_OP_MODE_HAL3_TW
		&& cis_data->is_data.op_mode != CAMERA_OP_MODE_HAL3_ATTACH)
		return true;

	if (cis_data->is_data.scene_mode != AA_SCENE_MODE_DISABLED
		&& cis_data->is_data.scene_mode != AA_SCENE_MODE_AI_HIGHRES)
		return true;

	if (device->image.framerate > 30)
		return true;

	if (device->image.framerate == 30 && cis_data->last_exp.val < 8000)
		return true;

	info("[%s] op_mode[%d] scene_mode[%d] fps[%d] exp[%d]\n",
		__func__, cis_data->is_data.op_mode, cis_data->is_data.scene_mode,
		device->image.framerate, cis_data->last_exp.val);

	return false;
}

int sensor_gn3_cis_stream_on(struct v4l2_subdev *subdev)
{
	int ret = 0;
	cis_shared_data *cis_data;
	struct is_device_sensor *device;
	u16 fast_change_idx = 0x00FF;
	struct is_cis *cis = sensor_cis_get_cis(subdev);
	struct sensor_gn3_private_data *priv = (struct sensor_gn3_private_data *)cis->sensor_info->priv;
	struct sensor_gn3_private_runtime *priv_runtime = (struct sensor_gn3_private_runtime *)cis->priv_runtime;
	ktime_t st = ktime_get();

	device = (struct is_device_sensor *)v4l2_get_subdev_hostdata(subdev);
	WARN_ON(!device);

	cis_data = cis->cis_data;

	dbg_sensor(1, "[MOD:D:%d] %s\n", cis->id, __func__);

	IXC_MUTEX_LOCK(cis->ixc_lock);

	cis->ixc_ops->write16(cis->client, 0xFCFC, 0x4000);
	cis->ixc_ops->write16(cis->client, 0x6000, 0x0005);
#if 0
#ifndef CONFIG_SEC_FACTORY
	if (!priv_runtime->eeprom_cal_available) {
		cis->ixc_ops->write8(client, 0x0D0B, 0x00); //PDXTC
		cis->ixc_ops->write8(client, 0x0D0A, 0x00); //GGC
		cis->ixc_ops->write8(client, 0x0B00, 0x00); //NonaXTC
	}
#endif
#endif

	if (sensor_gn3_cis_check_frame_duration_on(subdev)) {
		ret |= sensor_cis_write_registers(subdev, priv->frame_duration_on);
		info("[%s] frame_duration_on\n", __func__);
		priv_runtime->is_frame_duration_on = true;
	} else {
		ret |= sensor_cis_write_registers(subdev, priv->frame_duration_off);
		info("[%s] frame_duration_off\n", __func__);
		priv_runtime->is_frame_duration_on = false;
	}

	cis->ixc_ops->read16(cis->client, 0x0B30, &fast_change_idx);

#if IS_ENABLED(USE_CAMERA_SENSOR_RETENTION)
	cis->ixc_ops->write16(cis->client, 0x19C4, 0x0000);
	cis->ixc_ops->write16(cis->client, 0x0B32, 0x0000);
#endif
	IXC_MUTEX_UNLOCK(cis->ixc_lock);

	/* update mipi rate */
	is_vendor_set_mipi_clock(device);

	IXC_MUTEX_LOCK(cis->ixc_lock);
	cis->ixc_ops->write16(cis->client, 0x6000, 0x0085);

	/* Sensor stream on */
	info("[%s] set_cal_available(%d), FCI(0x%x), vsync_count(%d)\n",
		__func__, priv_runtime->eeprom_cal_available, fast_change_idx, cis_data->sen_vsync_count);
	cis->ixc_ops->write8(cis->client, 0x0100, 0x01);

	IXC_MUTEX_UNLOCK(cis->ixc_lock);

	cis_data->stream_on = true;
	priv_runtime->load_retention = true;

	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %lldus", __func__, PABLO_KTIME_US_DELTA_NOW(st));

	return ret;
}

int sensor_gn3_cis_stream_off(struct v4l2_subdev *subdev)
{
	int ret = 0;
	u8 cur_frame_count = 0;
	u16 fast_change_idx = 0x00FF;
	cis_shared_data *cis_data;
	struct is_cis *cis = sensor_cis_get_cis(subdev);
	ktime_t st = ktime_get();

	if (unlikely(!cis->client)) {
		err("client is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	cis_data = cis->cis_data;

	dbg_sensor(1, "[MOD:D:%d] %s\n", cis->id, __func__);

	cis_data->stream_on = false;

	IXC_MUTEX_LOCK(cis->ixc_lock);

	sensor_gn3_cis_force_aeb_off(subdev);

	cis->ixc_ops->read8(cis->client, 0x0005, &cur_frame_count);
	cis->ixc_ops->write16(cis->client, 0xFCFC, 0x4000);
	cis->ixc_ops->write16(cis->client, 0x6000, 0x0005);
	cis->ixc_ops->read16(cis->client, 0x0B30, &fast_change_idx);
	info("[%s] frame_count(0x%x), FCI(0x%x), vsync_count(%d)\n",
		__func__, cur_frame_count, fast_change_idx, cis_data->sen_vsync_count);

#if IS_ENABLED(USE_CAMERA_SENSOR_RETENTION)
	/* retention mode CRC check register enable */
	cis->ixc_ops->write16(cis->client, 0x010E, 0x0100);
	cis->ixc_ops->write16(cis->client, 0x19C2, 0x0000);
	if (sensor_cis_get_retention_mode(subdev) == SENSOR_RETENTION_INACTIVE)
		sensor_cis_set_retention_mode(subdev, SENSOR_RETENTION_ACTIVATED);

	info("[MOD:D:%d] %s : retention enable CRC check\n", cis->id, __func__);
#endif

	cis->ixc_ops->write16(cis->client, 0x6000, 0x0085);
	cis->ixc_ops->write16(cis->client, 0x0100, 0x0003);

	IXC_MUTEX_UNLOCK(cis->ixc_lock);

	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %lldus", __func__, PABLO_KTIME_US_DELTA_NOW(st));

p_err:
	return ret;
}

int sensor_gn3_cis_set_wb_gain(struct v4l2_subdev *subdev, struct wb_gains wb_gains)
{
	int ret = 0;
	int mode = 0;
	u16 abs_gains[3] = {0, }; /* R, G, B */
	u32 avg_g = 0;
	const struct sensor_cis_mode_info *mode_info;
	struct is_cis *cis = sensor_cis_get_cis(subdev);
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

	if (wb_gains.gr < 1024 || wb_gains.r < 1024 || wb_gains.b < 1024) {
		err("invalid RGB(%d, %d, %d)", wb_gains.r, wb_gains.gr, wb_gains.b); /* check DDK layer */
		return -EINVAL;
	}

	dbg_sensor(1, "%s [%d] DDK value: gr(%d), r(%d), b(%d), gb(%d)\n",
		__func__, cis->cis_data->sen_vsync_count, wb_gains.gr, wb_gains.r, wb_gains.b, wb_gains.gb);

	avg_g = (wb_gains.gr + wb_gains.gb) / 2;
	abs_gains[0] = (u16)(wb_gains.r & 0xFFFF);
	abs_gains[1] = (u16)(avg_g & 0xFFFF);
	abs_gains[2] = (u16)(wb_gains.b & 0xFFFF);

	dbg_sensor(1, "%s [%d] abs_gain_r(0x%4X), abs_gain_g(0x%4X), abs_gain_b(0x%4X)\n",
		__func__, cis->cis_data->sen_vsync_count, abs_gains[0], abs_gains[1], abs_gains[2]);

	IXC_MUTEX_LOCK(cis->ixc_lock);
	ret |= cis->ixc_ops->write16(cis->client, 0xFCFC, 0x4000);
	ret |= cis->ixc_ops->write16(cis->client, 0x0D82, abs_gains[0]);
	ret |= cis->ixc_ops->write16(cis->client, 0x0D84, abs_gains[1]);
	ret |= cis->ixc_ops->write16(cis->client, 0x0D86, abs_gains[2]);
	if (ret < 0) {
		err("sensor_gn3_set_registers fail!!");
		goto p_i2c_err;
	}

	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %lldus", __func__, PABLO_KTIME_US_DELTA_NOW(st));

p_i2c_err:
	IXC_MUTEX_UNLOCK(cis->ixc_lock);

	return ret;
}

int sensor_gn3_cis_get_updated_binning_ratio(struct v4l2_subdev *subdev, u32 *binning_ratio)
{
	struct is_cis *cis = sensor_cis_get_cis(subdev);
	cis_shared_data *cis_data = cis->cis_data;
	u32 rms_mode, zoom_ratio;

	if (sensor_gn3_mode_groups[SENSOR_GN3_MODE_RMS_CROP] == MODE_GROUP_NONE)
		return 0;

	zoom_ratio = cis_data->pre_remosaic_zoom_ratio;
	rms_mode = sensor_gn3_rms_zoom_mode[zoom_ratio];

	if (rms_mode && sensor_gn3_rms_binning_ratio[rms_mode])
		*binning_ratio = sensor_gn3_rms_binning_ratio[rms_mode];

	if (cis_data->remosaic_crop_update_vsync_cnt == cis_data->sen_vsync_count)
		info("[%s][%d] pre_zoom_ratio(%d), binning_ratio(%d)\n",
			__func__, cis_data->sen_vsync_count, zoom_ratio, *binning_ratio);
	else
		dbg_sensor(1, "[%s][%d] pre_zoom_ratio(%d), binning_ratio(%d)\n",
			__func__, cis_data->sen_vsync_count, zoom_ratio, *binning_ratio);

	return 0;
}

int sensor_gn3_cis_set_test_pattern(struct v4l2_subdev *subdev, camera2_sensor_ctl_t *sensor_ctl)
{
	int ret = 0;
	u32 mode = 0;
	struct is_cis *cis = sensor_cis_get_cis(subdev);

	mode = cis->cis_data->sens_config_index_cur;

	dbg_sensor(1, "[MOD:D:%d] %s, sensor mode(%d), cur_pattern_mode(%d), testPatternMode(%d)\n",
			cis->id, __func__, mode, cis->cis_data->cur_pattern_mode, sensor_ctl->testPatternMode);

	if (cis->cis_data->cur_pattern_mode != sensor_ctl->testPatternMode) {
		if (sensor_ctl->testPatternMode == SENSOR_TEST_PATTERN_MODE_OFF) {
			info("%s: set DEFAULT pattern! (testpatternmode : %d)\n", __func__, sensor_ctl->testPatternMode);

			IXC_MUTEX_LOCK(cis->ixc_lock);
			cis->ixc_ops->write16(cis->client, 0x0600, 0x0000);
			cis->ixc_ops->write16(cis->client, 0x0620, 0x0000);
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
			cis->ixc_ops->write16(cis->client, 0x0600, 0x0001);
			cis->ixc_ops->write16(cis->client, 0x0620, 0x0001);
			IXC_MUTEX_UNLOCK(cis->ixc_lock);

			cis->cis_data->cur_pattern_mode = sensor_ctl->testPatternMode;
		}
	}

	return ret;
}

int sensor_gn3_cis_set_group_param_hold(struct v4l2_subdev *subdev, bool hold)
{
	int ret = 0;
	struct is_cis *cis = sensor_cis_get_cis(subdev);
	cis_shared_data *cis_data = cis->cis_data;

	if (cis_data->sens_config_index_cur != SENSOR_GN3_2040X1532_120FPS
		&& cis_data->cur_hdr_mode != SENSOR_HDR_MODE_2AEB_2VC) {
		ret = sensor_cis_set_group_param_hold(subdev, hold);

		/* GN3 need skip N+1 control after update seamless at N frame */
		if (cis_data->update_seamless_state)
			cis_data->skip_control_vsync_count = cis_data->sen_vsync_count + 1;
	}

	return ret;
}

static struct is_cis_ops cis_ops_gn3 = {
	.cis_init = sensor_gn3_cis_init,
	.cis_deinit = sensor_gn3_cis_deinit,
	.cis_log_status = sensor_gn3_cis_log_status,
	.cis_group_param_hold = sensor_gn3_cis_set_group_param_hold,
	.cis_set_global_setting = sensor_gn3_cis_set_global_setting,
	.cis_init_state = sensor_cis_init_state,
	.cis_mode_change = sensor_gn3_cis_mode_change,
	.cis_stream_on = sensor_gn3_cis_stream_on,
	.cis_stream_off = sensor_gn3_cis_stream_off,
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
	.cis_calc_again_code = sensor_cis_calc_again_code,
	.cis_calc_again_permile = sensor_cis_calc_again_permile,
	.cis_set_digital_gain = sensor_cis_set_digital_gain,
	.cis_get_digital_gain = sensor_cis_get_digital_gain,
	.cis_get_min_digital_gain = sensor_cis_get_min_digital_gain,
	.cis_get_max_digital_gain = sensor_cis_get_max_digital_gain,
	.cis_calc_dgain_code = sensor_cis_calc_dgain_code,
	.cis_calc_dgain_permile = sensor_cis_calc_dgain_permile,
	.cis_compensate_gain_for_extremely_br = sensor_cis_compensate_gain_for_extremely_br,
	.cis_wait_streamoff = sensor_gn3_cis_wait_streamoff,
	.cis_wait_streamon = sensor_gn3_cis_wait_streamon,
	.cis_set_wb_gains = sensor_gn3_cis_set_wb_gain,
	.cis_check_rev_on_init = sensor_cis_check_rev_on_init,
	.cis_set_initial_exposure = sensor_cis_set_initial_exposure,
//	.cis_recover_stream_on = sensor_cis_recover_stream_on,
//	.cis_recover_stream_off = sensor_cis_recover_stream_off,
	.cis_set_test_pattern = sensor_gn3_cis_set_test_pattern,
#if IS_ENABLED(USE_CAMERA_SENSOR_RETENTION)
	.cis_update_seamless_mode = sensor_gn3_cis_update_seamless_mode,
	.cis_seamless_ctl_before_stream = sensor_cis_seamless_ctl_before_stream,
	.cis_wait_seamless_update_delay = sensor_cis_wait_seamless_update_delay,
#endif
	.cis_update_seamless_state = sensor_cis_update_seamless_state,
	.cis_get_updated_binning_ratio = sensor_gn3_cis_get_updated_binning_ratio,
	.cis_set_flip_mode = sensor_cis_set_flip_mode,
	.cis_set_remosaic_crop_shift_info = sensor_gn3_cis_set_remosaic_crop_shift_info,
};

static int cis_gn3_probe_i2c(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	int ret;
	u32 mclk_freq_khz;
	struct is_cis *cis;
	struct is_device_sensor_peri *sensor_peri;
	char const *setfile;
	struct device_node *dnode = client->dev.of_node;
	struct sensor_gn3_private_runtime *priv_runtime;

	ret = sensor_cis_probe(client, &(client->dev), &sensor_peri, I2C_TYPE);
	if (ret) {
		probe_info("%s: sensor_cis_probe ret(%d)\n", __func__, ret);
		return ret;
	}

	cis = &sensor_peri->cis;
	cis->ctrl_delay = N_PLUS_TWO_FRAME;
	cis->cis_ops = &cis_ops_gn3;

	/* belows are depend on sensor cis. MUST check sensor spec */
	cis->bayer_order = OTF_INPUT_ORDER_BAYER_GB_RG;
	cis->use_wb_gain = true;
	cis->reg_addr = &sensor_gn3_reg_addr;
	cis->priv_runtime = kzalloc(sizeof(struct sensor_gn3_private_runtime), GFP_KERNEL);
	if (!cis->priv_runtime) {
		kfree(cis->cis_data);
		kfree(cis->subdev);
		probe_err("cis->priv_runtime is NULL");
		return -ENOMEM;
	}

	priv_runtime = (struct sensor_gn3_private_runtime *)cis->priv_runtime;
	priv_runtime->eeprom_cal_available = false;
	priv_runtime->first_entrance = false;
	priv_runtime->need_stream_on_retention = true;

	ret = of_property_read_string(dnode, "setfile", &setfile);
	if (ret) {
		err("setfile index read fail(%d), take default setfile!!", ret);
		setfile = "default";
	}

	mclk_freq_khz = sensor_peri->module->pdata->mclk_freq_khz;

	if (mclk_freq_khz == 19200) {
		if (strcmp(setfile, "default") == 0 || strcmp(setfile, "setA") == 0) {
			probe_info("%s setfile_A mclk: 19.2MHz\n", __func__);
			cis->sensor_info = &sensor_gn3_info_A_19p2;
		}
	}

	is_vendor_set_mipi_mode(cis);

	probe_info("%s done\n", __func__);

	return ret;
}

static const struct of_device_id sensor_cis_gn3_match[] = {
	{
		.compatible = "samsung,exynos-is-cis-gn3",
	},
	{},
};
MODULE_DEVICE_TABLE(of, sensor_cis_gn3_match);

static const struct i2c_device_id sensor_cis_gn3_idt[] = {
	{ SENSOR_NAME, 0 },
	{},
};

static struct i2c_driver sensor_cis_gn3_driver = {
	.probe	= cis_gn3_probe_i2c,
	.driver = {
		.name	= SENSOR_NAME,
		.owner	= THIS_MODULE,
		.of_match_table = sensor_cis_gn3_match,
		.suppress_bind_attrs = true,
	},
	.id_table = sensor_cis_gn3_idt
};

#ifdef MODULE
builtin_i2c_driver(sensor_cis_gn3_driver);
#else
static int __init sensor_cis_gn3_init(void)
{
	int ret;

	ret = i2c_add_driver(&sensor_cis_gn3_driver);
	if (ret)
		err("failed to add %s driver: %d",
			sensor_cis_gn3_driver.driver.name, ret);

	return ret;
}
late_initcall_sync(sensor_cis_gn3_init);
#endif

MODULE_LICENSE("GPL");
MODULE_SOFTDEP("pre: fimc-is");
