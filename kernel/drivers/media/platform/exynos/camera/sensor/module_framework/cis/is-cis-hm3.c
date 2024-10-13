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
#include "is-cis-hm3.h"
#include "is-cis-hm3-setA-19p2.h"
#include "is-helper-ixc.h"
#include "is-sec-define.h"

#define SENSOR_NAME "S5KHM3"
/* #define DEBUG_HM3_PLL */
/* #define DEBUG_CAL_WRITE */

#define CIS_CALIBRATION 1

#if IS_ENABLED(CIS_CALIBRATION)
int sensor_hm3_cis_set_cal(struct v4l2_subdev *subdev);
#endif

#if IS_ENABLED(USE_CAMERA_SENSOR_RETENTION)
void sensor_hm3_cis_set_mode_group(u32 mode)
{
	sensor_hm3_mode_groups[SENSOR_HM3_MODE_DEFAULT] = mode;
	sensor_hm3_mode_groups[SENSOR_HM3_MODE_IDCG] = MODE_GROUP_NONE;
	sensor_hm3_mode_groups[SENSOR_HM3_MODE_LN2] = MODE_GROUP_NONE;
	sensor_hm3_mode_groups[SENSOR_HM3_MODE_LN4] = MODE_GROUP_NONE;
	sensor_hm3_mode_groups[SENSOR_HM3_MODE_RMS_CROP] = MODE_GROUP_NONE;

	switch (mode) {
	case SENSOR_HM3_4000X3000_15FPS_DL_LN4:
	case SENSOR_HM3_4000X3000_30FPS_DL:
	case SENSOR_HM3_4000X3000_30FPS_DL_LN2:
	case SENSOR_HM3_4000X3000_60FPS_DL:
		sensor_hm3_mode_groups[SENSOR_HM3_MODE_LN2] = SENSOR_HM3_4000X3000_30FPS_DL_LN2;
		break;
	case SENSOR_HM3_4000X3000_15FPS_DL_LN4_R12:
	case SENSOR_HM3_4000X3000_30FPS_DL_LN2_R12:
	case SENSOR_HM3_4000X3000_30FPS_DL_R12:
	case SENSOR_HM3_4000X3000_30FPS_IDCG_R12:
	case SENSOR_HM3_4000X3000_60FPS_DL_R12:
		sensor_hm3_mode_groups[SENSOR_HM3_MODE_LN2] = SENSOR_HM3_4000X3000_30FPS_DL_LN2_R12;
		break;
	case SENSOR_HM3_4000X2252_15FPS_DL_LN4:
	case SENSOR_HM3_4000X2252_30FPS_DL:
	case SENSOR_HM3_4000X2252_30FPS_DL_LN2:
	case SENSOR_HM3_4000X2252_60FPS_DL:
		sensor_hm3_mode_groups[SENSOR_HM3_MODE_LN2] = SENSOR_HM3_4000X2252_30FPS_DL_LN2;
		break;
	case SENSOR_HM3_4000X2252_15FPS_DL_LN4_R12:
	case SENSOR_HM3_4000X2252_30FPS_DL_LN2_R12:
	case SENSOR_HM3_4000X2252_30FPS_DL_R12:
	case SENSOR_HM3_4000X2252_30FPS_IDCG_R12:
	case SENSOR_HM3_4000X2252_60FPS_DL_R12:
		sensor_hm3_mode_groups[SENSOR_HM3_MODE_LN2] = SENSOR_HM3_4000X2252_30FPS_DL_LN2_R12;
		break;
	default:
		sensor_hm3_mode_groups[SENSOR_HM3_MODE_LN2] = MODE_GROUP_NONE;
	}

	switch (mode) {
	case SENSOR_HM3_4000X3000_15FPS_DL_LN4:
	case SENSOR_HM3_4000X3000_30FPS_DL:
	case SENSOR_HM3_4000X3000_30FPS_DL_LN2:
	case SENSOR_HM3_4000X3000_60FPS_DL:
		sensor_hm3_mode_groups[SENSOR_HM3_MODE_LN4] = SENSOR_HM3_4000X3000_15FPS_DL_LN4;
		break;
	case SENSOR_HM3_4000X3000_15FPS_DL_LN4_R12:
	case SENSOR_HM3_4000X3000_30FPS_DL_LN2_R12:
	case SENSOR_HM3_4000X3000_30FPS_DL_R12:
	case SENSOR_HM3_4000X3000_30FPS_IDCG_R12:
	case SENSOR_HM3_4000X3000_60FPS_DL_R12:
		sensor_hm3_mode_groups[SENSOR_HM3_MODE_LN4] = SENSOR_HM3_4000X3000_15FPS_DL_LN4_R12;
		break;
	case SENSOR_HM3_4000X2252_15FPS_DL_LN4:
	case SENSOR_HM3_4000X2252_30FPS_DL:
	case SENSOR_HM3_4000X2252_30FPS_DL_LN2:
	case SENSOR_HM3_4000X2252_60FPS_DL:
		sensor_hm3_mode_groups[SENSOR_HM3_MODE_LN4] = SENSOR_HM3_4000X2252_15FPS_DL_LN4;
		break;
	case SENSOR_HM3_4000X2252_15FPS_DL_LN4_R12:
	case SENSOR_HM3_4000X2252_30FPS_DL_LN2_R12:
	case SENSOR_HM3_4000X2252_30FPS_DL_R12:
	case SENSOR_HM3_4000X2252_30FPS_IDCG_R12:
	case SENSOR_HM3_4000X2252_60FPS_DL_R12:
		sensor_hm3_mode_groups[SENSOR_HM3_MODE_LN4] = SENSOR_HM3_4000X2252_15FPS_DL_LN4_R12;
		break;
	case SENSOR_HM3_4000X3000_30FPS_3S3A:
	case SENSOR_HM3_4000X3000_30FPS_3S3A_LN:
	case SENSOR_HM3_4000X3000_60FPS_3S3A:
		sensor_hm3_mode_groups[SENSOR_HM3_MODE_LN4] = SENSOR_HM3_4000X3000_30FPS_3S3A_LN;
		break;
	case SENSOR_HM3_4000X3000_30FPS_3S3A_LN_R12:
	case SENSOR_HM3_4000X3000_30FPS_IDCG_3S3A_R12:
	case SENSOR_HM3_4000X3000_60FPS_3S3A_R12:
		sensor_hm3_mode_groups[SENSOR_HM3_MODE_LN4] = SENSOR_HM3_4000X3000_30FPS_3S3A_LN_R12;
		break;
	case SENSOR_HM3_4000X2668_30FPS_3S3A:
	case SENSOR_HM3_4000X2668_30FPS_3S3A_LN:
	case SENSOR_HM3_4000X2668_60FPS_3S3A:
		sensor_hm3_mode_groups[SENSOR_HM3_MODE_LN4] = SENSOR_HM3_4000X2668_30FPS_3S3A_LN;
		break;
	case SENSOR_HM3_4000X2668_30FPS_3S3A_LN_R12:
	case SENSOR_HM3_4000X2668_30FPS_3S3A_R12:
	case SENSOR_HM3_4000X2668_30FPS_IDCG_3S3A_R12:
	case SENSOR_HM3_4000X2668_60FPS_3S3A_R12:
		sensor_hm3_mode_groups[SENSOR_HM3_MODE_LN4] = SENSOR_HM3_4000X2668_30FPS_3S3A_LN_R12;
		break;
	case SENSOR_HM3_4000X2252_30FPS_3S3A:
	case SENSOR_HM3_4000X2252_30FPS_3S3A_LN:
	case SENSOR_HM3_4000X2252_60FPS_3S3A:
		sensor_hm3_mode_groups[SENSOR_HM3_MODE_LN4] = SENSOR_HM3_4000X2252_30FPS_3S3A_LN;
		break;
	case SENSOR_HM3_4000X2252_30FPS_3S3A_LN_R12:
	case SENSOR_HM3_4000X2252_30FPS_IDCG_3S3A_R12:
	case SENSOR_HM3_4000X2252_60FPS_3S3A_R12:
		sensor_hm3_mode_groups[SENSOR_HM3_MODE_LN4] = SENSOR_HM3_4000X2252_30FPS_3S3A_LN_R12;
		break;
	default:
		sensor_hm3_mode_groups[SENSOR_HM3_MODE_LN4] = MODE_GROUP_NONE;
	}

	switch (mode) {
	case SENSOR_HM3_4000X3000_15FPS_DL_LN4_R12:
	case SENSOR_HM3_4000X3000_30FPS_DL_LN2_R12:
	case SENSOR_HM3_4000X3000_30FPS_DL_R12:
	case SENSOR_HM3_4000X3000_30FPS_IDCG_R12:
	case SENSOR_HM3_4000X3000_60FPS_DL_R12:
		sensor_hm3_mode_groups[SENSOR_HM3_MODE_IDCG] = SENSOR_HM3_4000X3000_30FPS_IDCG_R12;
		break;
	case SENSOR_HM3_4000X3000_30FPS_3S3A_LN_R12:
	case SENSOR_HM3_4000X3000_30FPS_IDCG_3S3A_R12:
	case SENSOR_HM3_4000X3000_60FPS_3S3A_R12:
		sensor_hm3_mode_groups[SENSOR_HM3_MODE_IDCG] = SENSOR_HM3_4000X3000_30FPS_IDCG_3S3A_R12;
		break;
	case SENSOR_HM3_4000X2252_15FPS_DL_LN4_R12:
	case SENSOR_HM3_4000X2252_30FPS_DL_LN2_R12:
	case SENSOR_HM3_4000X2252_30FPS_DL_R12:
	case SENSOR_HM3_4000X2252_30FPS_IDCG_R12:
	case SENSOR_HM3_4000X2252_60FPS_DL_R12:
		sensor_hm3_mode_groups[SENSOR_HM3_MODE_IDCG] = SENSOR_HM3_4000X2252_30FPS_IDCG_R12;
		break;
	case SENSOR_HM3_4000X2668_30FPS_3S3A_LN_R12:
	case SENSOR_HM3_4000X2668_30FPS_3S3A_R12:
	case SENSOR_HM3_4000X2668_30FPS_IDCG_3S3A_R12:
	case SENSOR_HM3_4000X2668_60FPS_3S3A_R12:
		sensor_hm3_mode_groups[SENSOR_HM3_MODE_IDCG] = SENSOR_HM3_4000X2668_30FPS_IDCG_3S3A_R12;
		break;
	case SENSOR_HM3_4000X2252_30FPS_3S3A_LN_R12:
	case SENSOR_HM3_4000X2252_30FPS_IDCG_3S3A_R12:
	case SENSOR_HM3_4000X2252_60FPS_3S3A_R12:
		sensor_hm3_mode_groups[SENSOR_HM3_MODE_IDCG] = SENSOR_HM3_4000X2252_30FPS_IDCG_3S3A_R12;
		break;
	default:
		sensor_hm3_mode_groups[SENSOR_HM3_MODE_IDCG] = MODE_GROUP_NONE;
	}

	switch (mode) {
	case SENSOR_HM3_4000X3000_30FPS_DL:
		sensor_hm3_mode_groups[SENSOR_HM3_MODE_RMS_CROP] = SENSOR_HM3_4000X3000_23FPS_RM;
		break;
	}

	info("[%s] default(%d) idcg(%d) ln2(%d) ln4(%d) abs(%d)\n", __func__,
		sensor_hm3_mode_groups[SENSOR_HM3_MODE_DEFAULT],
		sensor_hm3_mode_groups[SENSOR_HM3_MODE_IDCG],
		sensor_hm3_mode_groups[SENSOR_HM3_MODE_LN2],
		sensor_hm3_mode_groups[SENSOR_HM3_MODE_LN4],
		sensor_hm3_mode_groups[SENSOR_HM3_MODE_RMS_CROP]);
}
#endif

int sensor_hm3_cis_select_setfile(struct v4l2_subdev *subdev)
{
	int ret = 0;
	u16 rev = 0;
	u8 feature_id = 0;
	struct is_cis *cis = sensor_cis_get_cis(subdev);

	rev = cis->cis_data->cis_rev;

	ret |= cis->ixc_ops->write16(cis->client, 0xFCFC, 0x4000);
	ret |= cis->ixc_ops->read8(cis->client, 0x000E, &feature_id);
	if (ret < 0)
		err("read feature_id fail!!");

	info("hm3 sensor revision(%#x)\n", rev);
	info("hm3 sensor feature id(%#x)\n", feature_id);

	if (rev == 0xA000)
		info("hm3 rev EVT0 (%#x)\n", rev);
	else if (rev == 0xB000)
		info("hm3 rev EVT1 (%#x)\n", rev);

	if (feature_id == 0x03)
		info("hm3 feature id (%#x)\n", feature_id);
	else if (feature_id == 0x04)
		info("hm3+ feature id (%#x)\n", feature_id);

	return ret;
}

/* CIS OPS */
int sensor_hm3_cis_init(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_cis *cis = sensor_cis_get_cis(subdev);
	struct sensor_hm3_private_runtime *priv_runtime = (struct sensor_hm3_private_runtime *)cis->priv_runtime;

#if !defined(CONFIG_CAMERA_VENDOR_MCD)
	memset(cis->cis_data, 0, sizeof(cis_shared_data));

	ret = sensor_cis_check_rev(cis);
	if (ret < 0) {
		warn("sensor_hm3_check_rev is fail when cis init");
		return -EINVAL;
	}
#endif

	priv_runtime->first_entrance = true;

	sensor_hm3_cis_select_setfile(subdev);

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

	priv_runtime->load_retention = false;

	sensor_hm3_mode_groups[SENSOR_HM3_MODE_DEFAULT] = MODE_GROUP_NONE;
	sensor_hm3_mode_groups[SENSOR_HM3_MODE_IDCG] = MODE_GROUP_NONE;
	sensor_hm3_mode_groups[SENSOR_HM3_MODE_LN2] = MODE_GROUP_NONE;
	sensor_hm3_mode_groups[SENSOR_HM3_MODE_LN4] = MODE_GROUP_NONE;
	sensor_hm3_mode_groups[SENSOR_HM3_MODE_RMS_CROP] = MODE_GROUP_NONE;

	cis->cis_data->sens_config_index_pre = SENSOR_HM3_MODE_MAX;
	cis->cis_data->sens_config_index_cur = 0;
	CALL_CISOPS(cis, cis_data_calculation, subdev, cis->cis_data->sens_config_index_cur);

	return ret;
}

int sensor_hm3_cis_deinit(struct v4l2_subdev *subdev)
{
	int ret = 0;

#if IS_ENABLED(USE_CAMERA_SENSOR_RETENTION)
	struct is_cis *cis = sensor_cis_get_cis(subdev);
	struct sensor_hm3_private_runtime *priv_runtime = (struct sensor_hm3_private_runtime *)cis->priv_runtime;

	if (priv_runtime->load_retention == false) {
		info("%s: need to load retention\n", __func__);
		sensor_hm3_cis_stream_on(subdev);
		sensor_hm3_cis_wait_streamon(subdev);
		sensor_hm3_cis_stream_off(subdev);
		sensor_hm3_cis_wait_streamoff(subdev);
		info("%s: complete to load retention\n", __func__);
	}

	/* retention mode CRC wait calculation */
	usleep_range(10000, 10000);
#endif

	return ret;
}

static const struct is_cis_log log_hm3[] = {
	/* 4000 page */
	{I2C_WRITE, 16, 0x6000, 0x0005, "page unlock"},
	{I2C_WRITE, 16, 0xFCFC, 0x4000, "0x4000 page"},
	{I2C_READ, 16, 0x0000, 0, "model_id"},
	{I2C_READ, 16, 0x0002, 0, "revision_number"},
	{I2C_READ, 8, 0x0005, 0, "frame_count"},
	{I2C_READ, 8, 0x0100, 0, "0x0100"},
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

int sensor_hm3_cis_log_status(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_cis *cis = sensor_cis_get_cis(subdev);

#if IS_ENABLED(USE_CAMERA_SENSOR_RETENTION)
	if (sensor_cis_support_retention_mode(subdev))
		sensor_cis_set_retention_mode(subdev, SENSOR_RETENTION_INACTIVE);
#endif

	sensor_cis_log_status(cis, log_hm3, ARRAY_SIZE(log_hm3), (char *)__func__);

	return ret;
}

int sensor_hm3_cis_group_param_hold(struct v4l2_subdev *subdev, bool hold)
{
	int ret = 0;
	u32 mode = 0;
	struct is_cis *cis = sensor_cis_get_cis(subdev);
	struct sensor_hm3_private_runtime *priv_runtime = (struct sensor_hm3_private_runtime *)cis->priv_runtime;

	mode = cis->cis_data->sens_config_index_cur;

	mutex_lock(&priv_runtime->stream_off_lock);

	if (cis->cis_data->stream_on == false) {
		ret = 0;
		dbg_sensor(1, "%s : sensor stream off skip group_param_hold", __func__);
		goto p_err;
	}

	if (mode == SENSOR_HM3_992X744_120FPS) {
		ret = 0;
		dbg_sensor(1, "%s : fast ae skip group_param_hold", __func__);
		goto p_err;
	}

#if IS_ENABLED(USE_CAMERA_SENSOR_RETENTION)
	if (cis->cis_data->cur_hdr_mode == cis->cis_data->pre_hdr_mode
		&& cis->cis_data->cur_hdr_mode == SENSOR_HDR_MODE_2AEB_2VC
		&& hold == true) {
		ret = 0;
		dbg_sensor(1, "%s : aeb enable skip group_param_hold", __func__);
		goto p_err;
	}
#endif

	ret = sensor_cis_set_group_param_hold(subdev, hold);

p_err:
	mutex_unlock(&priv_runtime->stream_off_lock);
	return ret;
}


int sensor_hm3_cis_set_global_setting_internal(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_cis *cis = sensor_cis_get_cis(subdev);
	struct sensor_hm3_private_data *priv = (struct sensor_hm3_private_data *)cis->sensor_info->priv;

	IXC_MUTEX_LOCK(cis->ixc_lock);
	info("[%s] global setting internal start\n", __func__);
	/* setfile global setting is at camera entrance */
	ret |= sensor_cis_write_registers(subdev, priv->global);
	if (ret < 0) {
		err("sensor_hm3_set_registers fail!!");
		goto p_err;
	}

	info("[%s] global setting internal done\n", __func__);

	if (sensor_cis_support_retention_mode(subdev)) {
#if IS_ENABLED(CIS_CALIBRATION)
		ret = sensor_hm3_cis_set_cal(subdev);
		if (ret < 0) {
			err("sensor_hm3_cis_set_cal fail!!");
			goto p_err;
		}
#endif
	}

p_err:
	IXC_MUTEX_UNLOCK(cis->ixc_lock);
	return ret;
}

#ifdef HM3_BURST_WRITE
int sensor_hm3_cis_write16_burst(void *vclient, u16 addr, u8 *val, u32 num, bool endian)
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
	if (endian == HM3_BIG_ENDIAN) {
		for (i = 0; i < num; i++) {
			wbuf[(i * 2) + 2] = (val[(i * 2)] & 0xFF);
			wbuf[(i * 2) + 3] = (val[(i * 2) + 1] & 0xFF);
			ixc_info("I2CW16(%d) [0x%04x] : 0x%x%x\n", client->addr, addr, (val[(i * 2)] & 0xFF), (val[(i * 2) + 1] & 0xFF));
		}
	} else {
		for (i = 0; i < num; i++) {
			wbuf[(i * 2) + 2] = (val[(i * 2) + 1] & 0xFF);
			wbuf[(i * 2) + 3] = (val[(i * 2)] & 0xFF);
			ixc_info("I2CW16(%d) [0x%04x] : 0x%x%x\n", client->addr, addr, (val[(i * 2)] & 0xFF), (val[(i * 2) + 1] & 0xFF));
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
int sensor_hm3_cis_set_cal(struct v4l2_subdev *subdev)
{
	int ret = 0;
	char *cal_buf = NULL;
	bool endian = HM3_BIG_ENDIAN;
	int start_addr = 0, end_addr = 0;
#ifdef HM3_BURST_WRITE
	int cal_size = 0;
#endif
	int i = 0;
	u16 val = 0;
	int len = 0;
	struct is_vendor_rom *rom_info = NULL;
	struct is_cis *cis = sensor_cis_get_cis(subdev);
	struct sensor_hm3_private_data *priv = (struct sensor_hm3_private_data *)cis->sensor_info->priv;
	struct sensor_hm3_private_runtime *priv_runtime = (struct sensor_hm3_private_runtime *)cis->priv_runtime;

	info("[%s] E\n", __func__);

	if (priv_runtime->eeprom_cal_available)
		return 0;

	is_sec_get_rom_info(&rom_info, ROM_ID_REAR);

	if (!rom_info->read_done) {
		err("eeprom read fail status, skip set cal");
		priv_runtime->eeprom_cal_available = false;
		return 0;
	}

	info("[%s] eeprom read, start set cal\n", __func__);
	cal_buf = rom_info->buf;

#ifdef CAMERA_HM3_CAL_MODULE_VERSION
	if (rom_info->header_ver[10] < CAMERA_HM3_CAL_MODULE_VERSION) {
		start_addr = rom_info->rom_xtc_cal_data_addr_list[HM3_CAL_START_ADDR];
		if (cal_buf[start_addr + 2] == 0xFF && cal_buf[start_addr + 3] == 0xFF &&
			cal_buf[start_addr + 4] == 0xFF && cal_buf[start_addr + 5] == 0xFF &&
			cal_buf[start_addr + 6] == 0xFF && cal_buf[start_addr + 7] == 0xFF &&
			cal_buf[start_addr + 8] == 0xFF && cal_buf[start_addr + 9] == 0xFF &&
			cal_buf[start_addr + 10] == 0xFF && cal_buf[start_addr + 11] == 0xFF &&
			cal_buf[start_addr + 12] == 0xFF && cal_buf[start_addr + 13] == 0xFF &&
			cal_buf[start_addr + 14] == 0xFF && cal_buf[start_addr + 15] == 0xFF &&
			cal_buf[start_addr + 16] == 0xFF && cal_buf[start_addr + 17] == 0xFF) {
			info("empty Cal - offset[0x%04X] = val[0x%02X], offset[0x%04X] = val[0x%02X]\n",
				start_addr + 2, cal_buf[start_addr + 2], start_addr + 17, cal_buf[start_addr + 17]);
			info("[%s] empty Cal\n", __func__);
			return 0;
		}

		len = (rom_info->rom_xtc_cal_data_addr_list_len / HM3_CAL_ROW_LEN) - 1;
		if (len >= 0) {
			end_addr = rom_info->rom_xtc_cal_data_addr_list[len * HM3_CAL_ROW_LEN + HM3_CAL_END_ADDR];
			if (end_addr >= 15) {
				if (cal_buf[end_addr	] == 0xFF && cal_buf[end_addr - 1] == 0xFF &&
					cal_buf[end_addr - 2] == 0xFF && cal_buf[end_addr - 3] == 0xFF &&
					cal_buf[end_addr - 4] == 0xFF && cal_buf[end_addr - 5] == 0xFF &&
					cal_buf[end_addr - 6] == 0xFF && cal_buf[end_addr - 7] == 0xFF &&
					cal_buf[end_addr - 8] == 0xFF && cal_buf[end_addr - 9] == 0xFF &&
					cal_buf[end_addr - 10] == 0xFF && cal_buf[end_addr - 11] == 0xFF &&
					cal_buf[end_addr - 12] == 0xFF && cal_buf[end_addr - 13] == 0xFF &&
					cal_buf[end_addr - 14] == 0xFF && cal_buf[end_addr - 15] == 0xFF) {
					info("empty Cal - offset[0x%04X] = val[0x%02X], offset[0x%04X] = val[0x%02X]\n",
						end_addr, cal_buf[end_addr], end_addr - 15, cal_buf[end_addr - 15]);
					info("[%s] empty Cal\n", __func__);
					return 0;
				}
			}
		}
	}
#endif

	if (rom_info->rom_pdxtc_cal_data_addr_list_len <= 0
		|| rom_info->rom_gcc_cal_data_addr_list_len <= 0
		|| rom_info->rom_xtc_cal_data_addr_list_len <= 0) {
		err("Not available DT, skip set cal");
		priv_runtime->eeprom_cal_available = false;
		return 0;
	}

	ret |= cis->ixc_ops->write16(cis->client, 0xFCFC, 0x4000);
	ret |= cis->ixc_ops->write16(cis->client, 0x6000, 0x0085);

	info("[%s] PDXTC start\n", __func__);
	start_addr = rom_info->rom_pdxtc_cal_data_addr_list[HM3_CAL_START_ADDR];
	if (rom_info->rom_pdxtc_cal_endian_check) {
		if (cal_buf[start_addr] == 0xFF && cal_buf[start_addr + 1] == 0x00)
			endian = HM3_BIG_ENDIAN;
		else
			endian = HM3_LITTLE_ENDIAN;

		start_addr = start_addr + 2;
	} else {
		endian = HM3_BIG_ENDIAN;
	}

	for (len = 0; len < rom_info->rom_pdxtc_cal_data_addr_list_len / HM3_CAL_ROW_LEN; len++) {
		ret |= sensor_cis_write_registers(subdev, priv->pre_pdxtc[len]);

		dbg_sensor(1, "[%s] PDXTC Calibration Data E\n", __func__);
		if (len != 0)
			start_addr = rom_info->rom_pdxtc_cal_data_addr_list[len * HM3_CAL_ROW_LEN + HM3_CAL_START_ADDR];
		end_addr = rom_info->rom_pdxtc_cal_data_addr_list[len * HM3_CAL_ROW_LEN + HM3_CAL_END_ADDR];

#ifdef HM3_BURST_WRITE
		if (rom_info->rom_pdxtc_cal_data_addr_list[len * HM3_CAL_ROW_LEN + HM3_CAL_BURST_CHECK]) {
			cal_size = (end_addr - start_addr) / 2 + 1;
			dbg_sensor(1, "[%s] rom_pdxtc_cal burst write start(0x%X) end(0x%X)\n", __func__, start_addr, end_addr);
			ret = sensor_hm3_cis_write16_burst(cis->client, 0x6F12, (u8 *)&cal_buf[start_addr], cal_size, endian);
			if (ret < 0) {
				err("sensor_hm3_cis_write16_burst fail!!");
				goto p_err;
			}
		} else
#endif
		{
			for(i = start_addr; i <= end_addr; i += 2) {
				val = HM3_ENDIAN(cal_buf[i], cal_buf[i + 1], endian);
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

		dbg_sensor(1, "[%s] PDXTC Calibration Data X\n", __func__);

		ret |= sensor_cis_write_registers(subdev, priv->post_pdxtc[len]);
	}

	info("[%s] PDXTC end\n", __func__);

	info("[%s] GCC start\n", __func__);
	start_addr = rom_info->rom_gcc_cal_data_addr_list[HM3_CAL_START_ADDR];
	if (rom_info->rom_gcc_cal_endian_check) {
		if (cal_buf[start_addr] == 0xFF && cal_buf[start_addr + 1] == 0x00)
			endian = HM3_BIG_ENDIAN;
		else
			endian = HM3_LITTLE_ENDIAN;

		start_addr = start_addr + 2;
	} else {
		endian = HM3_BIG_ENDIAN;
	}

	for (len = 0; len < rom_info->rom_gcc_cal_data_addr_list_len / HM3_CAL_ROW_LEN; len++) {
		ret |= sensor_cis_write_registers(subdev, priv->pre_gcc[len]);

		dbg_sensor(1, "[%s] GCC Calibration Data E\n", __func__);
		if (len != 0)
			start_addr = rom_info->rom_gcc_cal_data_addr_list[len * HM3_CAL_ROW_LEN + HM3_CAL_START_ADDR];
		end_addr = rom_info->rom_gcc_cal_data_addr_list[len * HM3_CAL_ROW_LEN + HM3_CAL_END_ADDR];

#ifdef HM3_BURST_WRITE
		if (rom_info->rom_gcc_cal_data_addr_list[len * HM3_CAL_ROW_LEN + HM3_CAL_BURST_CHECK]) {
			cal_size = (end_addr - start_addr) / 2 + 1;
			dbg_sensor(1, "[%s] rom_gcc_cal burst write start(0x%X) end(0x%X)\n", __func__, start_addr, end_addr);
			ret = sensor_hm3_cis_write16_burst(cis->client, 0x6F12, (u8 *)&cal_buf[start_addr], cal_size, endian);
			if (ret < 0) {
				err("sensor_hm3_cis_write16_burst fail!!");
				goto p_err;
			}
		} else
#endif
		{
			for(i = start_addr; i <= end_addr; i += 2) {
				val = HM3_ENDIAN(cal_buf[i], cal_buf[i + 1], endian);
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

		dbg_sensor(1, "[%s] GCC Calibration Data X\n", __func__);

		ret |= sensor_cis_write_registers(subdev, priv->post_gcc[len]);
	}
	info("[%s] GCC end\n", __func__);

	info("[%s] XTC start\n", __func__);
	start_addr = rom_info->rom_xtc_cal_data_addr_list[HM3_CAL_START_ADDR];
	if (rom_info->rom_xtc_cal_endian_check) {
		if (cal_buf[start_addr] == 0xFF && cal_buf[start_addr + 1] == 0x00)
			endian = HM3_BIG_ENDIAN;
		else
			endian = HM3_LITTLE_ENDIAN;

		start_addr = start_addr + 2;
	} else {
		endian = HM3_BIG_ENDIAN;
	}

	for (len = 0; len < rom_info->rom_xtc_cal_data_addr_list_len / HM3_CAL_ROW_LEN; len++) {
		ret |= sensor_cis_write_registers(subdev, priv->pre_xtc[len]);

		dbg_sensor(1, "[%s] XTC Calibration Data E\n", __func__);
		if (len != 0)
			start_addr = rom_info->rom_xtc_cal_data_addr_list[len * HM3_CAL_ROW_LEN + HM3_CAL_START_ADDR];
		end_addr = rom_info->rom_xtc_cal_data_addr_list[len * HM3_CAL_ROW_LEN + HM3_CAL_END_ADDR];

#ifdef HM3_BURST_WRITE
		if (rom_info->rom_xtc_cal_data_addr_list[len * HM3_CAL_ROW_LEN + HM3_CAL_BURST_CHECK]) {
			cal_size = (end_addr - start_addr) / 2 + 1;
			dbg_sensor(1, "[%s] rom_xtc_cal burst write start(0x%X) end(0x%X) size(%d)\n", __func__, start_addr, end_addr, cal_size);
			ret = sensor_hm3_cis_write16_burst(cis->client, 0x6F12, (u8 *)&cal_buf[start_addr], cal_size, endian);
			if (ret < 0) {
				err("sensor_hm3_cis_write16_burst fail!!");
				goto p_err;
			}
		} else
#endif
		{
			for(i = start_addr; i <= end_addr; i += 2) {
				val = HM3_ENDIAN(cal_buf[i], cal_buf[i + 1], endian);
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

		/* For handle last 8bit */
		if (len + 1 == rom_info->rom_xtc_cal_data_addr_list_len / HM3_CAL_ROW_LEN) {
#define HM3_XTC_LAST_8BIT_ADDR 0x57E6

			ret = cis->ixc_ops->write16(cis->client, 0x602A, 0x6C40);
			ret |= cis->ixc_ops->write8(cis->client, 0x6F12, cal_buf[HM3_XTC_LAST_8BIT_ADDR]);

			info("last 8 bit addr[0x%04X] , val[0x%04X]\n",
				HM3_XTC_LAST_8BIT_ADDR, cal_buf[HM3_XTC_LAST_8BIT_ADDR]);

			if (ret < 0) {
				err("cis->ixc_ops->write fail!!");
				goto p_err;
			}
		}

		dbg_sensor(1, "[%s] XTC Calibration Data X\n", __func__);

		ret |= sensor_cis_write_registers(subdev, priv->post_xtc[len]);
	}
	info("[%s] XTC end\n", __func__);

	priv_runtime->eeprom_cal_available = true;

	info("[%s] X\n", __func__);

p_err:
	return ret;
}
#endif

#if IS_ENABLED(USE_CAMERA_SENSOR_RETENTION)
int sensor_hm3_cis_check_12bit(cis_shared_data *cis_data,
	u32 *next_mode,
	u16 *next_fast_change_idx,
	u32 *load_sram_idx)
{
	int ret = 0;

	if (sensor_hm3_mode_groups[SENSOR_HM3_MODE_IDCG] == MODE_GROUP_NONE) {
		ret = -1;
		goto EXIT;
	}

	switch (cis_data->cur_12bit_mode) {
	case SENSOR_12BIT_STATE_REAL_12BIT:
		*next_mode = sensor_hm3_mode_groups[SENSOR_HM3_MODE_IDCG];
		*next_fast_change_idx =
			sensor_hm3_spec_mode_retention_attr[*next_mode].fast_change_idx;
		*load_sram_idx = SENSOR_HM3_LOAD_SRAM_IDX_NONE;
		break;
	case SENSOR_12BIT_STATE_PSEUDO_12BIT:
	default:
		ret = -1;
		break;
	}

EXIT:
	return ret;
}

int sensor_hm3_cis_check_lownoise(cis_shared_data *cis_data,
	u32 *next_mode,
	u16 *next_fast_change_idx,
	u32 *load_sram_idx)
{
	int ret = 0;
	u32 temp_mode = MODE_GROUP_NONE;

	if (sensor_hm3_mode_groups[SENSOR_HM3_MODE_LN2] == MODE_GROUP_NONE
		&& sensor_hm3_mode_groups[SENSOR_HM3_MODE_LN4] == MODE_GROUP_NONE) {
		ret = -1;
		goto EXIT;
	}

	switch (cis_data->cur_lownoise_mode) {
	case IS_CIS_LN2:
		temp_mode = sensor_hm3_mode_groups[SENSOR_HM3_MODE_LN2];
		break;
	case IS_CIS_LN4:
		temp_mode = sensor_hm3_mode_groups[SENSOR_HM3_MODE_LN4];
		break;
	case IS_CIS_LNOFF:
	default:
		break;
	}

	if (temp_mode == MODE_GROUP_NONE)
		ret = -1;

	if (ret == 0) {
		*next_mode = temp_mode;
		*next_fast_change_idx =
			sensor_hm3_spec_mode_retention_attr[*next_mode].fast_change_idx;
		*load_sram_idx = SENSOR_HM3_LOAD_SRAM_IDX_NONE;
	}

EXIT:
	return ret;
}

int sensor_hm3_cis_check_cropped_remosaic(cis_shared_data *cis_data,
	u32 cur_fast_change_idx,
	u32 *next_mode,
	u16 *next_fast_change_idx,
	u32 *load_sram_idx)
{
	int ret = 0;
	u32 zoom_ratio = 0;

	if (sensor_hm3_mode_groups[SENSOR_HM3_MODE_RMS_CROP] == MODE_GROUP_NONE) {
		ret = -1;
		goto EXIT;
	}

	zoom_ratio = cis_data->cur_remosaic_zoom_ratio;

	if (zoom_ratio != HM3_REMOSAIC_ZOOM_RATIO_X_2) {
		ret = -1; //goto default
		goto EXIT;
	}

	*next_mode = sensor_hm3_mode_groups[SENSOR_HM3_MODE_RMS_CROP];
	/* no need sub index */
	*next_fast_change_idx =
		sensor_hm3_spec_mode_retention_attr[*next_mode].fast_change_idx;
	*load_sram_idx = SENSOR_HM3_LOAD_SRAM_IDX_NONE;
EXIT:
	return ret;
}

static bool sensor_hm3_cis_check_aeb_status(struct v4l2_subdev *subdev)
{
	u16 aeb_check1, aeb_check2, aeb_check3, aeb_check4;
	struct is_cis *cis = sensor_cis_get_cis(subdev);

	cis->ixc_ops->write16(cis->client, 0x602C, 0x4000);
	cis->ixc_ops->write16(cis->client, 0x602E, 0x0E00);
	cis->ixc_ops->read16(cis->client, 0x6F12, &aeb_check1);
	cis->ixc_ops->write16(cis->client, 0x602C, 0x2000);
	cis->ixc_ops->write16(cis->client, 0x602E, 0x1664);
	cis->ixc_ops->read16(cis->client, 0x6F12, &aeb_check2); // 0x20001664
	cis->ixc_ops->read16(cis->client, 0x6F12, &aeb_check3); // 0x20001666
	cis->ixc_ops->read16(cis->client, 0x6F12, &aeb_check4); // 0x20001668

	// true : on , false : off
	return (aeb_check1 == (u16)0x0223 && aeb_check2 == (u16)0x0001);
}

static int sensor_hm3_cis_aeb_off(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_cis *cis = sensor_cis_get_cis(subdev);

	ret |= cis->ixc_ops->write16(cis->client, 0x6028, 0x4000);
	ret |= cis->ixc_ops->write16(cis->client, 0x602A, 0x0E00);
	ret |= cis->ixc_ops->write16(cis->client, 0x6F12, 0x0023); //off 0x0023
	ret |= cis->ixc_ops->write16(cis->client, 0x6028, 0x2000);
	ret |= cis->ixc_ops->write16(cis->client, 0x602A, 0x1664);
	ret |= cis->ixc_ops->write16(cis->client, 0x6F12, 0x0002); //off 0x0002
	ret |= cis->ixc_ops->write16(cis->client, 0x6F12, 0x0000);
	ret |= cis->ixc_ops->write16(cis->client, 0x6F12, 0x0000);

	return ret;
}

int sensor_hm3_cis_check_aeb(struct v4l2_subdev *subdev)
{
	int ret = 0;
	u32 mode = 0;
	bool is_aeb_on = false;
	struct is_device_sensor *device;
	const struct sensor_cis_mode_info *mode_info;
	struct is_cis *cis = sensor_cis_get_cis(subdev);

	device = (struct is_device_sensor *)v4l2_get_subdev_hostdata(subdev);
	WARN_ON(!device);

	mode = cis->cis_data->sens_config_index_cur;
	mode_info = cis->sensor_info->mode_infos[mode];
	is_aeb_on = sensor_hm3_cis_check_aeb_status(subdev);

	if (cis->cis_data->stream_on == false || is_sensor_g_ex_mode(device) != EX_AEB) {
		ret = -1; //continue;
		goto EXIT;
	}

	if (cis->cis_data->cur_hdr_mode == cis->cis_data->pre_hdr_mode) {
		if (cis->cis_data->cur_hdr_mode != SENSOR_HDR_MODE_2AEB_2VC) {
			if (is_aeb_on)
				ret = 0; // still AEB ON status. keep fast change idx disable AEB(0x0111)
			else
				ret = -2; //continue; OFF->OFF
		} else {
			ret = 0; //return; ON->ON
		}
		goto EXIT;
	}

	if (cis->cis_data->cur_hdr_mode == SENSOR_HDR_MODE_2AEB_2VC) {
		info("%s : enable 2AEB 2VC\n", __func__);

		//lut_1_long
		ret = cis->ixc_ops->write16(cis->client, AEB_HM3_LUT0 + AEB_HM3_OFFSET_CIT, 0x0100); // CIT
		ret = cis->ixc_ops->write16(cis->client, AEB_HM3_LUT0 + AEB_HM3_OFFSET_AGAIN, 0x0020); // A GAIN
		ret = cis->ixc_ops->write16(cis->client, AEB_HM3_LUT0 + AEB_HM3_OFFSET_DGAIN, 0x0020); // D GAIN
		ret = cis->ixc_ops->write16(cis->client, AEB_HM3_LUT0 + AEB_HM3_OFFSET_FLL, 0x0C08); // FLL 60fps

		//LUT2 - Short
		ret = cis->ixc_ops->write16(cis->client, AEB_HM3_LUT1 + AEB_HM3_OFFSET_CIT, 0x0300); // CIT
		ret = cis->ixc_ops->write16(cis->client, AEB_HM3_LUT1 + AEB_HM3_OFFSET_AGAIN, 0x0040); // A GAIN
		ret = cis->ixc_ops->write16(cis->client, AEB_HM3_LUT1 + AEB_HM3_OFFSET_DGAIN, 0x0200); // D GAIN
		ret = cis->ixc_ops->write16(cis->client, AEB_HM3_LUT1 + AEB_HM3_OFFSET_FLL, 0x0C08); // FLL 60fps

		//Fast CM apply frame N+2
		ret = cis->ixc_ops->write16(cis->client, 0x0B30, 0x0110);
		ret = cis->ixc_ops->write16(cis->client, 0x0B32, 0x0100);
		ret = cis->ixc_ops->write16(cis->client, 0x6028, 0x2000);
		ret = cis->ixc_ops->write16(cis->client, 0x602A, 0x1668);
		ret = cis->ixc_ops->write16(cis->client, 0x6F12, 0x0000);

		ret = cis->ixc_ops->write16(cis->client, 0xFCFC, 0x4000);
		ret = cis->ixc_ops->write16(cis->client, 0x0B30, 0x0110); // AEB enable fast change idx
	} else {
		info("%s : disable AEB\n", __func__);

		if (is_aeb_on) {
			//Fast CM apply frame N+1
			ret = cis->ixc_ops->write16(cis->client, 0x0B30, 0x0111);
			ret = cis->ixc_ops->write16(cis->client, 0x0B32, 0x0000);
			ret = cis->ixc_ops->write16(cis->client, 0x6028, 0x2000);
			ret = cis->ixc_ops->write16(cis->client, 0x602A, 0x1668);
			ret = cis->ixc_ops->write16(cis->client, 0x6F12, 0x0100);

			ret = cis->ixc_ops->write16(cis->client, 0xFCFC, 0x4000);
			ret = cis->ixc_ops->write16(cis->client, 0x0B30, 0x0111); // AEB disable fast change idx
		}
	}

	cis->cis_data->pre_hdr_mode = cis->cis_data->cur_hdr_mode;

	CALL_CISOPS(cis, cis_data_calculation, subdev, mode);

EXIT:
	return ret;
}

int sensor_hm3_cis_update_seamless_mode(struct v4l2_subdev *subdev)
{
	int ret = 0;
	unsigned int mode = 0;
	unsigned int next_mode = 0;
	u16 cur_fast_change_idx = NONFCM;
	u16 next_fast_change_idx = 0;
	u32 load_sram_idx = SENSOR_HM3_LOAD_SRAM_IDX_NONE;
	struct is_cis *cis = sensor_cis_get_cis(subdev);
	struct sensor_hm3_private_data *priv = (struct sensor_hm3_private_data *)cis->sensor_info->priv;

#if !IS_ENABLED(USE_CAMERA_SENSOR_RETENTION)
	IXC_MUTEX_LOCK(cis->ixc_lock);
	sensor_hm3_cis_check_aeb(subdev);
	IXC_MUTEX_UNLOCK(cis->ixc_lock);

	return ret;
#endif

	mode = cis->cis_data->sens_config_index_cur;

	next_mode = sensor_hm3_mode_groups[SENSOR_HM3_MODE_DEFAULT];
	if (next_mode == MODE_GROUP_NONE) {
		err("mode group is none");
		ret = -1;
		return ret;
	}

	next_fast_change_idx
		= sensor_hm3_spec_mode_retention_attr[next_mode].fast_change_idx;

	IXC_MUTEX_LOCK(cis->ixc_lock);

	if (sensor_hm3_cis_check_aeb(subdev) == 0)
		goto p_i2c_unlock;

	sensor_hm3_cis_check_lownoise(cis->cis_data,
		&next_mode, &next_fast_change_idx, &load_sram_idx);
	sensor_hm3_cis_check_12bit(cis->cis_data,
		&next_mode, &next_fast_change_idx, &load_sram_idx);

	cis->ixc_ops->write16(cis->client, 0xFCFC, 0x4000);
	cis->ixc_ops->read16(cis->client, 0x0B30, &cur_fast_change_idx);

	sensor_hm3_cis_check_cropped_remosaic(cis->cis_data, cur_fast_change_idx,
		&next_mode, &next_fast_change_idx, &load_sram_idx);

	if ((mode == next_mode && cur_fast_change_idx == next_fast_change_idx)
		|| next_mode == MODE_GROUP_NONE)
		goto p_i2c_unlock;

	if (load_sram_idx != SENSOR_HM3_LOAD_SRAM_IDX_NONE) {
		ret = sensor_cis_write_registers(subdev, priv->load_sram[load_sram_idx]);
		if (ret < 0)
			err("sensor_hm3_set_registers fail!!");
	}

	ret |= cis->ixc_ops->write16(cis->client, 0xFCFC, 0x4000);
	ret |= cis->ixc_ops->write16(cis->client, 0x0B30, next_fast_change_idx);
	if (ret < 0)
		err("sensor_hm3_set_registers fail!!");

	cis->cis_data->sens_config_index_pre = cis->cis_data->sens_config_index_cur;
	cis->cis_data->sens_config_index_cur = next_mode;

	CALL_CISOPS(cis, cis_data_calculation, subdev, next_mode);

	info("[%s] pre(%d)->cur(%d) 12bit[%d] LN[%d] ZOOM[%d] AEB[%d] => load_sram_idx[%d] fast_change_idx[0x%x]\n",
		 __func__,
		cis->cis_data->sens_config_index_pre, cis->cis_data->sens_config_index_cur,
		cis->cis_data->cur_12bit_mode,
		cis->cis_data->cur_lownoise_mode,
		cis->cis_data->cur_remosaic_zoom_ratio,
		cis->cis_data->cur_hdr_mode,
		load_sram_idx,
		next_fast_change_idx);

	cis->cis_data->pre_remosaic_zoom_ratio = cis->cis_data->cur_remosaic_zoom_ratio;
	cis->cis_data->pre_12bit_mode = cis->cis_data->cur_12bit_mode;
	cis->cis_data->pre_lownoise_mode = cis->cis_data->cur_lownoise_mode;

p_i2c_unlock:
	IXC_MUTEX_UNLOCK(cis->ixc_lock);

	return ret;
}
#endif

int sensor_hm3_cis_mode_change(struct v4l2_subdev *subdev, u32 mode)
{
	int ret = 0;
	u32 ex_mode;
	u16 fast_change_idx = 0x00FF;
#if IS_ENABLED(USE_CAMERA_SENSOR_RETENTION)
	int load_sram_idx = SENSOR_HM3_LOAD_SRAM_IDX_NONE;
	int load_sram_idx_ln2 = SENSOR_HM3_LOAD_SRAM_IDX_NONE;
	int load_sram_idx_ln4 = SENSOR_HM3_LOAD_SRAM_IDX_NONE;
#endif
	const struct sensor_cis_mode_info *mode_info;
	struct is_device_sensor *device;
	struct is_cis *cis = sensor_cis_get_cis(subdev);
	struct sensor_hm3_private_data *priv = (struct sensor_hm3_private_data *)cis->sensor_info->priv;
	struct sensor_hm3_private_runtime *priv_runtime = (struct sensor_hm3_private_runtime *)cis->priv_runtime;

	device = (struct is_device_sensor *)v4l2_get_subdev_hostdata(subdev);
	WARN_ON(!device);

	if (mode >= cis->sensor_info->mode_count) {
		err("invalid mode(%d)!!", mode);
		ret = -EINVAL;
		goto p_err;
	}

	info("[%s] E\n", __func__);

	cis->mipi_clock_index_cur = CAM_MIPI_NOT_INITIALIZED;

	IXC_MUTEX_LOCK(cis->ixc_lock);

	ex_mode = is_sensor_g_ex_mode(device);

	info("[%s] sensor mode(%d) ex_mode(%d)\n", __func__, mode, ex_mode);

	mode_info = cis->sensor_info->mode_infos[mode];

#if IS_ENABLED(USE_CAMERA_SENSOR_RETENTION)
	/* Retention mode sensor mode select */
	if (sensor_cis_get_retention_mode(subdev) == SENSOR_RETENTION_ACTIVATED) {
		priv_runtime->load_retention = false;

		sensor_hm3_cis_set_mode_group(mode);

		fast_change_idx = sensor_hm3_spec_mode_retention_attr[mode].fast_change_idx;

		if (fast_change_idx != NONFCM) {
			load_sram_idx = sensor_hm3_spec_mode_retention_attr[mode].load_sram_idx;

			if (sensor_hm3_mode_groups[SENSOR_HM3_MODE_LN2] != MODE_GROUP_NONE) {
				load_sram_idx_ln2 = sensor_hm3_spec_mode_retention_attr
					[sensor_hm3_mode_groups[SENSOR_HM3_MODE_LN2]].load_sram_idx;
			}

			if (sensor_hm3_mode_groups[SENSOR_HM3_MODE_LN4] != MODE_GROUP_NONE) {
				load_sram_idx_ln4 = sensor_hm3_spec_mode_retention_attr
					[sensor_hm3_mode_groups[SENSOR_HM3_MODE_LN4]].load_sram_idx;
			}

			if (load_sram_idx != SENSOR_HM3_LOAD_SRAM_IDX_NONE) {
				ret = sensor_cis_write_registers(subdev, priv->load_sram[load_sram_idx]);
				if (ret < 0) {
					err("sensor_hm3_set_registers fail!!");
					goto p_err_i2c_unlock;
				}
			}

			if (load_sram_idx_ln2 != SENSOR_HM3_LOAD_SRAM_IDX_NONE) {
				ret = sensor_cis_write_registers(subdev, priv->load_sram[load_sram_idx_ln2]);
				if (ret < 0) {
					err("sensor_hm3_set_registers fail!!");
					goto p_err_i2c_unlock;
				}
			}

			if (load_sram_idx_ln4 != SENSOR_HM3_LOAD_SRAM_IDX_NONE) {
				ret = sensor_cis_write_registers(subdev, priv->load_sram[load_sram_idx_ln4]);
				if (ret < 0) {
					err("sensor_hm3_set_registers fail!!");
					goto p_err_i2c_unlock;
				}
			}

			ret |= cis->ixc_ops->write16(cis->client, 0xFCFC, 0x4000);
			ret |= cis->ixc_ops->write16(cis->client, 0x0B30, fast_change_idx);
			if (ret < 0) {
				err("sensor_hm3_set_registers fail!!");
				goto p_err_i2c_unlock;
			}
		} else {
			info("[%s] not support retention sensor mode(%d)\n", __func__, mode);

			//Fast Change disable
			ret |= cis->ixc_ops->write16(cis->client, 0xFCFC, 0x4000);
			ret |= cis->ixc_ops->write16(cis->client, 0x0B30, 0x00FF);

			ret |= sensor_cis_write_registers(subdev, mode_info->setfile);

			if (ret < 0) {
				err("sensor_hm3_set_registers fail!!");
				goto p_err_i2c_unlock;
			}
		}
	} else
#endif
	{
		info("[%s] not support retention sensor mode(%d)\n", __func__, mode);

		//Fast Change disable
		ret |= cis->ixc_ops->write16(cis->client, 0xFCFC, 0x4000);
		ret |= cis->ixc_ops->write16(cis->client, 0x0B30, 0x00FF);

		ret |= sensor_cis_write_registers(subdev, mode_info->setfile);

		if (ret < 0) {
			err("sensor_hm3_set_registers fail!!");
			goto p_err_i2c_unlock;
		}
	}

	cis->cis_data->sens_config_index_pre = mode;

	//Remosaic Bypass
	if (ex_mode == EX_AI_REMOSAIC) {
		ret |= cis->ixc_ops->write16(cis->client, 0x6028, 0x2000);
		ret |= cis->ixc_ops->write16(cis->client, 0x602A, 0xC730);
		ret |= cis->ixc_ops->write16(cis->client, 0x6F12, 0x0100); // off 0000 on 0100
	} else {
		ret |= cis->ixc_ops->write16(cis->client, 0x6028, 0x2000);
		ret |= cis->ixc_ops->write16(cis->client, 0x602A, 0xC730);
		ret |= cis->ixc_ops->write16(cis->client, 0x6F12, 0x0000); // off 0000 on 0100
	}

	if (ret < 0) {
		err("remosaic bypass set fail!!");
		goto p_err_i2c_unlock;
	}

	cis->cis_data->remosaic_mode = mode_info->remosaic_mode;

	cis->cis_data->pre_12bit_mode = mode_info->state_12bit;
	cis->cis_data->cur_12bit_mode = mode_info->state_12bit;

	cis->cis_data->pre_lownoise_mode = IS_CIS_LNOFF;
	cis->cis_data->cur_lownoise_mode = IS_CIS_LNOFF;

	cis->cis_data->pre_remosaic_zoom_ratio = 0;
	cis->cis_data->cur_remosaic_zoom_ratio = 0;

	cis->cis_data->pre_hdr_mode = SENSOR_HDR_MODE_SINGLE;
	cis->cis_data->cur_hdr_mode = SENSOR_HDR_MODE_SINGLE;

#if IS_ENABLED(USE_CAMERA_SENSOR_RETENTION)
	ret |= cis->ixc_ops->write16(cis->client, 0x6000, 0x0085); //0x6000 0x0085 1byte, 2byte 0x0005 only 2byte

	if (ex_mode == EX_AEB) {
		// repeat the loop indefinitely
		ret = cis->ixc_ops->write16(cis->client, 0x0E02, 0x0000);
		// Bit_0:1=CIT, BIT_1:1=GAIN, BIT_11:1=Image(VC,DT), BIT_12:1= PDAF(VC,DT)
		ret = cis->ixc_ops->write16(cis->client, 0x0E04, 0x1C23);

		ret = cis->ixc_ops->write16(cis->client, 0x0E18, 0x0000); // VC1 = 0, VC2 = 0
		ret = cis->ixc_ops->write16(cis->client, 0x0E1A, 0x0000); // VC3 = 0, VC4 = 0
		ret = cis->ixc_ops->write16(cis->client, 0x0E1E, 0x0000); // DT3 = 0, DT4 = 0
		ret = cis->ixc_ops->write16(cis->client, 0x0E20, 0x0002); // PDAF, VC1 = 2, VC2 = 0
		ret = cis->ixc_ops->write16(cis->client, 0x0E22, 0x0000); // PDAF, VC3 = 0, VC4 = 0
		ret = cis->ixc_ops->write16(cis->client, 0x0E24, 0x002B); // PDAF, DT1 = 2B, DT2 = 0
		ret = cis->ixc_ops->write16(cis->client, 0x0E26, 0x0000); // PDAF, DT3 = 0, DT4 = 0

		ret = cis->ixc_ops->write16(cis->client, 0x0E30, 0x0001); // VC1 = 1, VC2 = 0
		ret = cis->ixc_ops->write16(cis->client, 0x0E32, 0x0000); // VC3 = 0, VC4 = 0
		ret = cis->ixc_ops->write16(cis->client, 0x0E36, 0x0000); // DT3 = 0, DT4 = 0
		ret = cis->ixc_ops->write16(cis->client, 0x0E38, 0x0003); // PDAF, VC1 = 3, VC2 = 0
		ret = cis->ixc_ops->write16(cis->client, 0x0E3A, 0x0000); // PDAF, VC3 = 0, VC4 = 0
		ret = cis->ixc_ops->write16(cis->client, 0x0E3C, 0x002B); // PDAF, DT1 = 2B, DT2 = 0
		ret = cis->ixc_ops->write16(cis->client, 0x0E3E, 0x0000); // PDAF, DT3 = 0, DT4 = 0

		if (mode_info->state_12bit == SENSOR_12BIT_STATE_OFF) {
			ret = cis->ixc_ops->write16(cis->client, 0x0E1C, 0x002B); // DT1 = 2B, DT2 = 0
			ret = cis->ixc_ops->write16(cis->client, 0x0E34, 0x002B); // DT1 = 2B, DT2 = 0
		} else {
			ret = cis->ixc_ops->write16(cis->client, 0x0E1C, 0x002C); // DT1 = 2C, DT2 = 0
			ret = cis->ixc_ops->write16(cis->client, 0x0E34, 0x002C); // DT1 = 2C, DT2 = 0
		}

		// lut_1_long
		ret = cis->ixc_ops->write16(cis->client, AEB_HM3_LUT0 + AEB_HM3_OFFSET_CIT, 0x0300); // CIT
		ret = cis->ixc_ops->write16(cis->client, AEB_HM3_LUT0 + AEB_HM3_OFFSET_AGAIN, 0x0040); // A GAIN
		ret = cis->ixc_ops->write16(cis->client, AEB_HM3_LUT0 + AEB_HM3_OFFSET_DGAIN, 0x0200); // D GAIN
		ret = cis->ixc_ops->write16(cis->client, AEB_HM3_LUT0 + AEB_HM3_OFFSET_FLL, 0x0C09); // FLL 60fps
		// LUT2 - Short
		ret = cis->ixc_ops->write16(cis->client, AEB_HM3_LUT1 + AEB_HM3_OFFSET_CIT, 0x0100); // CIT
		ret = cis->ixc_ops->write16(cis->client, AEB_HM3_LUT1 + AEB_HM3_OFFSET_AGAIN, 0x0020); // A GAIN
		ret = cis->ixc_ops->write16(cis->client, AEB_HM3_LUT1 + AEB_HM3_OFFSET_DGAIN, 0x0020); // D GAIN
		ret = cis->ixc_ops->write16(cis->client, AEB_HM3_LUT1 + AEB_HM3_OFFSET_FLL, 0x0C09); // FLL 60fps

	}

	//AEB OFF
	if (sensor_hm3_cis_check_aeb_status(subdev))
		sensor_hm3_cis_aeb_off(subdev);
#endif

	//Fast CM apply frame N+1
	ret = cis->ixc_ops->write16(cis->client, 0x0B30, 0x0111);
	ret = cis->ixc_ops->write16(cis->client, 0x0B32, 0x0000);
	ret = cis->ixc_ops->write16(cis->client, 0x6028, 0x2000);
	ret = cis->ixc_ops->write16(cis->client, 0x602A, 0x1668);
	ret = cis->ixc_ops->write16(cis->client, 0x6F12, 0x0100);

	info("[%s] mode[%d] 12bit[%d] LN[%d] ZOOM[%d] AEB[%d] => load_sram_idx[%d], fast_change_idx[0x%x]\n",
		__func__, mode,
		cis->cis_data->cur_12bit_mode,
		cis->cis_data->cur_lownoise_mode,
		cis->cis_data->cur_remosaic_zoom_ratio,
		cis->cis_data->cur_hdr_mode,
		load_sram_idx,
		fast_change_idx);

p_err_i2c_unlock:
	IXC_MUTEX_UNLOCK(cis->ixc_lock);

#if IS_ENABLED(USE_CAMERA_SENSOR_RETENTION)
	sensor_hm3_cis_update_seamless_mode(subdev);
#endif

p_err:
	/* sensor_hm3_cis_log_status(subdev); */
	info("[%s] X\n", __func__);

	return ret;
}

int sensor_hm3_cis_set_global_setting(struct v4l2_subdev *subdev)
{
	int ret = 0;

#if IS_ENABLED(USE_CAMERA_SENSOR_RETENTION)
	struct is_cis *cis = sensor_cis_get_cis(subdev);
	struct sensor_hm3_private_runtime *priv_runtime = (struct sensor_hm3_private_runtime *)cis->priv_runtime;
	u32 retention_mode = sensor_cis_get_retention_mode(subdev);

	/* setfile global setting is at camera entrance */
	if (retention_mode == SENSOR_RETENTION_INACTIVE) {
		sensor_hm3_cis_set_global_setting_internal(subdev);
		sensor_hm3_cis_retention_prepare(subdev);
	} else if (retention_mode == SENSOR_RETENTION_ACTIVATED) {
		sensor_hm3_cis_retention_crc_check(subdev);
	} else { /* SENSOR_RETENTION_UNSUPPORTED */
		priv_runtime->eeprom_cal_available = false;
		sensor_hm3_cis_set_global_setting_internal(subdev);
	}
#else
	WARN_ON(!subdev);

	info("[%s] global setting start\n", __func__);
	/* setfile global setting is at camera entrance */
	ret = sensor_hm3_cis_set_global_setting_internal(subdev);
	if (ret < 0) {
		err("sensor_hm3_set_registers fail!!");
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
int sensor_hm3_cis_retention_prepare(struct v4l2_subdev *subdev)
{
	int ret = 0;
	int i = 0;
	const struct sensor_cis_mode_info *mode_info;
	struct is_cis *cis = sensor_cis_get_cis(subdev);
	struct sensor_hm3_private_data *priv = (struct sensor_hm3_private_data *)cis->sensor_info->priv;
	struct sensor_hm3_private_runtime *priv_runtime = (struct sensor_hm3_private_runtime *)cis->priv_runtime;

	for (i = 0; i < priv->max_retention_num; i++) {
		IXC_MUTEX_LOCK(cis->ixc_lock);
		ret = sensor_cis_write_registers(subdev, priv->retention[i]);
		IXC_MUTEX_UNLOCK(cis->ixc_lock);
		if (ret < 0) {
			err("sensor_hm3_set_registers fail!!");
			goto p_err;
		}
	}

#if IS_ENABLED(CIS_CALIBRATION)
	IXC_MUTEX_LOCK(cis->ixc_lock);
	ret = sensor_hm3_cis_set_cal(subdev);
	IXC_MUTEX_UNLOCK(cis->ixc_lock);
	if (ret < 0) {
		err("sensor_hm3_cis_set_cal fail!!");
		goto p_err;
	}
#endif

	//FAST AE Setting
	IXC_MUTEX_LOCK(cis->ixc_lock);
	mode_info = cis->sensor_info->mode_infos[SENSOR_HM3_992X744_120FPS];
	ret = sensor_cis_write_registers(subdev, mode_info->setfile);
	IXC_MUTEX_UNLOCK(cis->ixc_lock);
	if (ret < 0) {
		err("sensor_hm3_set_registers fail!!");
		goto p_err;
	}

	IXC_MUTEX_LOCK(cis->ixc_lock);
	ret |= cis->ixc_ops->write16(cis->client, 0xFCFC, 0x2000);
	ret |= cis->ixc_ops->write16(cis->client, 0x16DA, 0x0001);
	ret |= cis->ixc_ops->write16(cis->client, 0x16DE, 0x2002);
	ret |= cis->ixc_ops->write16(cis->client, 0x16E0, 0x8400);
	ret |= cis->ixc_ops->write16(cis->client, 0x16E2, 0x8B00);
	ret |= cis->ixc_ops->write16(cis->client, 0x16E4, 0x9200);
	ret |= cis->ixc_ops->write16(cis->client, 0x16E6, 0x9900);
	ret |= cis->ixc_ops->write16(cis->client, 0x16E8, 0xA000);
	ret |= cis->ixc_ops->write16(cis->client, 0x16EA, 0xA700);
	ret |= cis->ixc_ops->write16(cis->client, 0x16EC, 0xAE00);
	ret |= cis->ixc_ops->write16(cis->client, 0x16EE, 0xB500);
	ret |= cis->ixc_ops->write16(cis->client, 0x16F0, 0xBC00);
	ret |= cis->ixc_ops->write16(cis->client, 0x16F2, 0xC300);
	ret |= cis->ixc_ops->write16(cis->client, 0x16F4, 0xCA00);
	ret |= cis->ixc_ops->write16(cis->client, 0x16F6, 0xD100);
	ret |= cis->ixc_ops->write16(cis->client, 0x16F8, 0xD800);
	ret |= cis->ixc_ops->write16(cis->client, 0x16FA, 0xDF00);
	ret |= cis->ixc_ops->write16(cis->client, 0x16FC, 0x5000);
	ret |= cis->ixc_ops->write16(cis->client, 0x16FE, 0x5700);
	ret |= cis->ixc_ops->write16(cis->client, 0xFCFC, 0x2001);
	ret |= cis->ixc_ops->write16(cis->client, 0xE54C, 0x5E00);
	ret |= cis->ixc_ops->write16(cis->client, 0xE54E, 0x5F00);
	ret |= cis->ixc_ops->write16(cis->client, 0xE550, 0x6000);
	ret |= cis->ixc_ops->write16(cis->client, 0xFCFC, 0x4000);
	ret |= cis->ixc_ops->write16(cis->client, 0x0B30, 0x01FF);
	ret |= cis->ixc_ops->write16(cis->client, 0x010E, 0x0100);
	ret |= cis->ixc_ops->write16(cis->client, 0x6000, 0x0085);
	IXC_MUTEX_UNLOCK(cis->ixc_lock);

	if (priv_runtime->need_stream_on_retention) {
		IXC_MUTEX_LOCK(cis->ixc_lock);
		ret |= cis->ixc_ops->write16(cis->client, 0x0100, 0x0100); //stream on
		IXC_MUTEX_UNLOCK(cis->ixc_lock);

		sensor_hm3_cis_wait_streamon(subdev);
		msleep(100); //decompression cal data

		IXC_MUTEX_LOCK(cis->ixc_lock);
		ret |= cis->ixc_ops->write8(cis->client, 0x0100, 0x00); //stream off
		IXC_MUTEX_UNLOCK(cis->ixc_lock);

		sensor_hm3_cis_wait_streamoff(subdev);
		priv_runtime->need_stream_on_retention = false;
	}

	if (ret < 0) {
		err("cis->ixc_ops->write fail!!");
		goto p_err;
	}

	usleep_range(10000, 10000);

	sensor_cis_set_retention_mode(subdev, SENSOR_RETENTION_ACTIVATED);

	info("[%s] retention sensor RAM write done\n", __func__);

p_err:

	return ret;
}

int sensor_hm3_cis_retention_crc_check(struct v4l2_subdev *subdev)
{
	int ret = 0;
	u16 crc_check = 0;
	struct is_cis *cis = sensor_cis_get_cis(subdev);
	struct sensor_hm3_private_runtime *priv_runtime = (struct sensor_hm3_private_runtime *)cis->priv_runtime;

	IXC_MUTEX_LOCK(cis->ixc_lock);

	/* retention mode CRC check */
	cis->ixc_ops->read16(cis->client, 0x19C2, &crc_check); /* api_ro_checksum_on_ram_passed */

	IXC_MUTEX_UNLOCK(cis->ixc_lock);

	if (crc_check == 0x0100) {
		info("[%s] retention SRAM CRC check: pass!\n", __func__);

		/* init pattern */
		cis->ixc_ops->write16(cis->client, 0x0600, 0x0000);
	} else {
		info("[%s] retention SRAM CRC check: fail!\n", __func__);
		info("retention CRC Check register value: 0x%x\n", crc_check);
		info("[%s] rewrite retention modes to SRAM\n", __func__);

		ret = sensor_hm3_cis_set_global_setting_internal(subdev);
		if (ret < 0) {
			err("CRC error recover: rewrite sensor global setting failed");
			goto p_err;
		}

		priv_runtime->eeprom_cal_available = false;

		ret = sensor_hm3_cis_retention_prepare(subdev);
		if (ret < 0) {
			err("CRC error recover: retention prepare failed");
			goto p_err;
		}
	}

p_err:

	return ret;
}
#endif

int sensor_hm3_cis_wait_streamon(struct v4l2_subdev *subdev)
{
	int ret = 0;
	int retry_count = 0;
	int max_retry_count = 10;
	struct is_cis *cis = sensor_cis_get_cis(subdev);
	struct sensor_hm3_private_runtime *priv_runtime = (struct sensor_hm3_private_runtime *)cis->priv_runtime;

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

int sensor_hm3_cis_wait_streamoff(struct v4l2_subdev *subdev)
{
	int ret = 0;

	info("%s : E\n", __func__);

	ret = sensor_cis_wait_streamoff(subdev);

	info("%s : ret=%d X\n", __func__, ret);

	return ret;
}

int sensor_hm3_cis_stream_on(struct v4l2_subdev *subdev)
{
	int ret = 0;
	cis_shared_data *cis_data;
	struct is_device_sensor *device;
	u16 fast_change_idx = 0x00FF;
	struct is_cis *cis = sensor_cis_get_cis(subdev);
	struct sensor_hm3_private_runtime *priv_runtime = (struct sensor_hm3_private_runtime *)cis->priv_runtime;
	ktime_t st = ktime_get();

	device = (struct is_device_sensor *)v4l2_get_subdev_hostdata(subdev);
	WARN_ON(!device);

	cis_data = cis->cis_data;

	dbg_sensor(1, "[MOD:D:%d] %s\n", cis->id, __func__);

	is_vendor_set_mipi_clock(device);

	IXC_MUTEX_LOCK(cis->ixc_lock);

#ifdef DEBUG_HM3_PLL
	{
		u16 pll;

		cis->ixc_ops->read16(cis->client, 0x0300, &pll);
		dbg_sensor(1, "______ vt_pix_clk_div(0x%x)\n", pll);
		cis->ixc_ops->read16(cis->client, 0x0302, &pll);
		dbg_sensor(1, "______ vt_sys_clk_div(0x%x)\n", pll);
		cis->ixc_ops->read16(cis->client, 0x0304, &pll);
		dbg_sensor(1, "______ vt_pre_pll_clk_div(0x%x)\n", pll);
		cis->ixc_ops->read16(cis->client, 0x0306, &pll);
		dbg_sensor(1, "______ vt_pll_multiplier(0x%x)\n", pll);
		cis->ixc_ops->read16(cis->client, 0x0308, &pll);
		dbg_sensor(1, "______ op_pix_clk_div(0x%x)\n", pll);
		cis->ixc_ops->read16(cis->client, 0x030a, &pll);
		dbg_sensor(1, "______ op_sys_clk_div(0x%x)\n", pll);

		cis->ixc_ops->read16(cis->client, 0x030c, &pll);
		dbg_sensor(1, "______ vt_pll_post_scaler(0x%x)\n", pll);
		cis->ixc_ops->read16(cis->client, 0x030e, &pll);
		dbg_sensor(1, "______ op_pre_pll_clk_dv(0x%x)\n", pll);
		cis->ixc_ops->read16(cis->client, 0x0310, &pll);
		dbg_sensor(1, "______ op_pll_multiplier(0x%x)\n", pll);
		cis->ixc_ops->read16(cis->client, 0x0312, &pll);
		dbg_sensor(1, "______ op_pll_post_scalar(0x%x)\n", pll);

		cis->ixc_ops->read16(cis->client, 0x0340, &pll);
		dbg_sensor(1, "______ frame_length_lines(0x%x)\n", pll);
		cis->ixc_ops->read16(cis->client, 0x0342, &pll);
		dbg_sensor(1, "______ line_length_pck(0x%x)\n", pll);
	}
#endif

	/* EMB Header off */
	ret = cis->ixc_ops->write8(cis->client, 0x0118, 0x00);
	if (ret < 0){
		err("EMB header off fail");
	}

#if IS_ENABLED(USE_CAMERA_SENSOR_RETENTION)
	if (cis_data->cur_hdr_mode == SENSOR_HDR_MODE_SINGLE
		&& cis_data->pre_hdr_mode == SENSOR_HDR_MODE_2AEB_2VC) {
		info("[%s] current AEB status is enabled. need AEB disable\n", __func__);

		cis_data->pre_hdr_mode = SENSOR_HDR_MODE_SINGLE;
		cis_data->cur_hdr_mode = SENSOR_HDR_MODE_SINGLE;

		cis->ixc_ops->read16(cis->client, 0x0B30, &fast_change_idx);
		if (sensor_hm3_cis_check_aeb_status(subdev) || fast_change_idx == 0x0110) {
			sensor_hm3_cis_aeb_off(subdev);

			/* AEB disable fast change idx */
			if (fast_change_idx == 0x0110)
				ret |= cis->ixc_ops->write16(cis->client, 0x0B30, 0x0111);

			info("[%s] disable AEB\n", __func__);
		}
	}
#endif

	/* Page lock */
	cis->ixc_ops->write16(cis->client, 0xFCFC, 0x4000);
	cis->ixc_ops->write16(cis->client, 0x6000, 0x0085);

#ifndef CONFIG_SEC_FACTORY
	if (!priv_runtime->eeprom_cal_available) {
		cis->ixc_ops->write8(cis->client, 0x0D0B, 0x00); //PDXTC
		cis->ixc_ops->write8(cis->client, 0x0D0A, 0x00); //GGC
		cis->ixc_ops->write8(cis->client, 0x0B10, 0x00); //NonaXTC
	}
#endif

#ifdef USE_CAMERA_MCD_SW_DUAL_SYNC
	if (cis->cis_data->is_data.scene_mode == AA_SCENE_MODE_LIVE_OUTFOCUS) {
		if (cis->cis_data->sens_config_index_cur == SENSOR_HM3_2800X2100_30FPS
			|| cis->cis_data->sens_config_index_cur == SENSOR_HM3_2800X2100_30FPS_R12) {
			info("[%s] s/w dual sync slave\n", __func__);
			cis->dual_sync_mode = DUAL_SYNC_SLAVE;
			cis->dual_sync_type = DUAL_SYNC_TYPE_SW;
		} else {
			info("[%s] s/w dual sync master\n", __func__);
			cis->dual_sync_mode = DUAL_SYNC_MASTER;
			cis->dual_sync_type = DUAL_SYNC_TYPE_SW;
		}
	} else {
		info("[%s] s/w dual sync none\n", __func__);
		cis->dual_sync_mode = DUAL_SYNC_NONE;
		cis->dual_sync_type = DUAL_SYNC_TYPE_MAX;
	}
#endif

	cis->ixc_ops->read16(cis->client, 0x0B30, &fast_change_idx);

	/* Sensor stream on */
	info("%s - set_cal_available(%d), fast_change_idx(0x%x)\n",
		__func__, priv_runtime->eeprom_cal_available, fast_change_idx);
	cis->ixc_ops->write16(cis->client, 0x0100, 0x0100);

	IXC_MUTEX_UNLOCK(cis->ixc_lock);

	cis_data->stream_on = true;
	priv_runtime->load_retention = true;

	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %ldus", __func__, PABLO_KTIME_US_DELTA_NOW(st));

	return ret;
}

int sensor_hm3_cis_stream_off(struct v4l2_subdev *subdev)
{
	int ret = 0;
	u8 cur_frame_count = 0;
	u16 fast_change_idx = 0x00FF;
	cis_shared_data *cis_data;
	struct is_cis *cis = sensor_cis_get_cis(subdev);
	struct sensor_hm3_private_runtime *priv_runtime = (struct sensor_hm3_private_runtime *)cis->priv_runtime;
	ktime_t st = ktime_get();

	cis_data = cis->cis_data;

	dbg_sensor(1, "[MOD:D:%d] %s\n", cis->id, __func__);

	mutex_lock(&priv_runtime->stream_off_lock);

	cis_data->stream_on = false; // for not working group_param_hold after stream off

	sensor_cis_set_group_param_hold(subdev, false);

	IXC_MUTEX_LOCK(cis->ixc_lock);

	cis->ixc_ops->read8(cis->client, 0x0005, &cur_frame_count);
	cis->ixc_ops->read16(cis->client, 0x0B30, &fast_change_idx);
	info("%s: frame_count(0x%x), fast_change_idx(0x%x)\n", __func__, cur_frame_count, fast_change_idx);

#if IS_ENABLED(USE_CAMERA_SENSOR_RETENTION)
	/* retention mode CRC check register enable */
	cis->ixc_ops->write16(cis->client, 0x010E, 0x0100);
	cis->ixc_ops->write16(cis->client, 0x19C2, 0x0000);
	if (sensor_cis_get_retention_mode(subdev) == SENSOR_RETENTION_INACTIVE)
		sensor_cis_set_retention_mode(subdev, SENSOR_RETENTION_ACTIVATED);

	info("[MOD:D:%d] %s : retention enable CRC check\n", cis->id, __func__);
#endif

	cis->ixc_ops->write8(cis->client, 0x0100, 0x00);

	IXC_MUTEX_UNLOCK(cis->ixc_lock);

	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %ldus", __func__, PABLO_KTIME_US_DELTA_NOW(st));

	mutex_unlock(&priv_runtime->stream_off_lock);

	return ret;
}

int sensor_hm3_cis_set_wb_gain(struct v4l2_subdev *subdev, struct wb_gains wb_gains)
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
	if (ret < 0) {
		err("sensor_hm3_set_registers fail!!");
		goto p_i2c_err;
	}

	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %ldus", __func__, PABLO_KTIME_US_DELTA_NOW(st));

p_i2c_err:
	IXC_MUTEX_UNLOCK(cis->ixc_lock);

	return ret;
}

static struct is_cis_ops cis_ops_hm3 = {
	.cis_init = sensor_hm3_cis_init,
	.cis_deinit = sensor_hm3_cis_deinit,
	.cis_log_status = sensor_hm3_cis_log_status,
	.cis_group_param_hold = sensor_hm3_cis_group_param_hold,
	.cis_set_global_setting = sensor_hm3_cis_set_global_setting,
	.cis_init_state = sensor_cis_init_state,
	.cis_mode_change = sensor_hm3_cis_mode_change,
	.cis_stream_on = sensor_hm3_cis_stream_on,
	.cis_stream_off = sensor_hm3_cis_stream_off,
	.cis_data_calculation = sensor_cis_data_calculation,
	.cis_set_exposure_time = sensor_cis_set_exposure_time,
	.cis_set_long_term_exposure = sensor_cis_long_term_exposure,
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
	.cis_wait_streamoff = sensor_hm3_cis_wait_streamoff,
	.cis_wait_streamon = sensor_hm3_cis_wait_streamon,
	.cis_set_wb_gains = sensor_hm3_cis_set_wb_gain,
	.cis_check_rev_on_init = sensor_cis_check_rev_on_init,
	.cis_set_initial_exposure = sensor_cis_set_initial_exposure,
//	.cis_recover_stream_on = sensor_cis_recover_stream_on,
//	.cis_recover_stream_off = sensor_cis_recover_stream_off,
	.cis_set_test_pattern = sensor_cis_set_test_pattern,
	.cis_update_seamless_mode = sensor_hm3_cis_update_seamless_mode,
	.cis_update_seamless_state = sensor_cis_update_seamless_state,
};

static int cis_hm3_probe_i2c(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	int ret;
	u32 mclk_freq_khz;
	struct is_cis *cis;
	struct is_device_sensor_peri *sensor_peri;
	char const *setfile;
	struct device_node *dnode = client->dev.of_node;
	struct sensor_hm3_private_runtime *priv_runtime;

	ret = sensor_cis_probe(client, &(client->dev), &sensor_peri, I2C_TYPE);
	if (ret) {
		probe_info("%s: sensor_cis_probe ret(%d)\n", __func__, ret);
		return ret;
	}

	cis = &sensor_peri->cis;
	cis->ctrl_delay = N_PLUS_TWO_FRAME;
	cis->cis_ops = &cis_ops_hm3;
	/* belows are depend on sensor cis. MUST check sensor spec */
	cis->bayer_order = OTF_INPUT_ORDER_BAYER_GR_BG;
	cis->use_wb_gain = true;
	cis->reg_addr = &sensor_hm3_reg_addr;
	cis->priv_runtime = kzalloc(sizeof(struct sensor_hm3_private_runtime), GFP_KERNEL);
	if (!cis->priv_runtime) {
		kfree(cis->cis_data);
		kfree(cis->subdev);
		probe_err("cis->priv_runtime is NULL");
		return -ENOMEM;
	}

	priv_runtime = (struct sensor_hm3_private_runtime *)cis->priv_runtime;
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
			cis->sensor_info = &sensor_hm3_info_A_19p2;
		}
	}

	is_vendor_set_mipi_mode(cis);

	mutex_init(&priv_runtime->stream_off_lock);

	probe_info("%s done\n", __func__);

	return ret;
}

static const struct of_device_id sensor_cis_hm3_match[] = {
	{
		.compatible = "samsung,exynos-is-cis-hm3",
	},
	{},
};
MODULE_DEVICE_TABLE(of, sensor_cis_hm3_match);

static const struct i2c_device_id sensor_cis_hm3_idt[] = {
	{ SENSOR_NAME, 0 },
	{},
};

static struct i2c_driver sensor_cis_hm3_driver = {
	.probe	= cis_hm3_probe_i2c,
	.driver = {
		.name	= SENSOR_NAME,
		.owner	= THIS_MODULE,
		.of_match_table = sensor_cis_hm3_match,
		.suppress_bind_attrs = true,
	},
	.id_table = sensor_cis_hm3_idt
};

#ifdef MODULE
builtin_i2c_driver(sensor_cis_hm3_driver);
#else
static int __init sensor_cis_hm3_init(void)
{
	int ret;

	ret = i2c_add_driver(&sensor_cis_hm3_driver);
	if (ret)
		err("failed to add %s driver: %d",
			sensor_cis_hm3_driver.driver.name, ret);

	return ret;
}
late_initcall_sync(sensor_cis_hm3_init);
#endif

MODULE_LICENSE("GPL");
