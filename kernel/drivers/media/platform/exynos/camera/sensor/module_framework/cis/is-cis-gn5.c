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
#include "is-cis-gn5.h"
#include "is-cis-gn5-setA.h"

#include "is-helper-ixc.h"
#include "is-sec-define.h"

#include "interface/is-interface-library.h"
#include "is-interface-sensor.h"
#define SENSOR_NAME "GN5"

/* temp change, remove once PDAF settings done */
//#define PDAF_DISABLE
#ifdef PDAF_DISABLE
const u16 sensor_gn5_pdaf_off[] = {
	0x6000, 0x0005, 0x02,
	0xFCFC, 0x4000, 0x02,
	0x0720, 0x0000, 0x02,
	0x6000, 0x0085, 0x02,
};

const struct sensor_regs pdaf_off = SENSOR_REGS(sensor_gn5_pdaf_off);
#endif

void sensor_gn5_cis_set_mode_group(u32 mode)
{
	sensor_gn5_mode_groups[SENSOR_GN5_MODE_NORMAL] = mode;
	sensor_gn5_mode_groups[SENSOR_GN5_MODE_RMS_CROP] = MODE_GROUP_NONE;
	sensor_gn5_mode_groups[SENSOR_GN5_MODE_LN4] = MODE_GROUP_NONE;
	sensor_gn5_mode_groups[SENSOR_GN5_MODE_IDCG] = MODE_GROUP_NONE;

	switch (mode) {
	case SENSOR_GN5_4080X3060_30FPS_R12:
		sensor_gn5_mode_groups[SENSOR_GN5_MODE_RMS_CROP] =
			SENSOR_GN5_4080X3060_30FPS_REMOSAIC_CROP_R12;
		sensor_gn5_mode_groups[SENSOR_GN5_MODE_LN4] =
			SENSOR_GN5_4080X3060_20FPS_LN4_R12;
		break;
	case SENSOR_GN5_4080X2296_30FPS_R12:
		sensor_gn5_mode_groups[SENSOR_GN5_MODE_RMS_CROP] =
			SENSOR_GN5_4080X2296_30FPS_REMOSAIC_CROP_R12;
		sensor_gn5_mode_groups[SENSOR_GN5_MODE_LN4] =
			SENSOR_GN5_4080X2296_30FPS_LN4_R12;
		break;
	case SENSOR_GN5_4080X3060_30FPS_IDCG_R12:
		sensor_gn5_mode_groups[SENSOR_GN5_MODE_IDCG] =
			SENSOR_GN5_4080X3060_30FPS_IDCG_R12;
		break;
	case SENSOR_GN5_4080X2296_30FPS_IDCG_R12:
		sensor_gn5_mode_groups[SENSOR_GN5_MODE_IDCG] =
			SENSOR_GN5_4080X2296_30FPS_IDCG_R12;
		sensor_gn5_mode_groups[SENSOR_GN5_MODE_LN4] =
			SENSOR_GN5_4080X2296_30FPS_LN4_R12;
		break;
	}

	info("[%s] normal(%d) ln4(%d) idcg(%d) rms_crop(%d)\n", __func__,
		sensor_gn5_mode_groups[SENSOR_GN5_MODE_NORMAL],
		sensor_gn5_mode_groups[SENSOR_GN5_MODE_LN4],
		sensor_gn5_mode_groups[SENSOR_GN5_MODE_IDCG],
		sensor_gn5_mode_groups[SENSOR_GN5_MODE_RMS_CROP]);
}

#if IS_ENABLED(CIS_CALIBRATION)
#ifdef GN5_BURST_WRITE
int sensor_gn5_cis_write16_burst(struct i2c_client *client, u16 addr, u8 *val, u32 num, bool endian)
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
	if (endian == GN5_BIG_ENDIAN) {
		wbuf[0] = (addr & 0xFF00) >> 8;
		wbuf[1] = (addr & 0xFF);
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

int sensor_gn5_cis_set_cal(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_cis *cis = sensor_cis_get_cis(subdev);
	struct is_device_sensor_peri *sensor_peri = NULL;
	int cal_size = 0;
	u16 start_addr;
	struct is_vendor_rom *rom_info = NULL;
	char *cal_buf = NULL;
	int rom_id = 0;
	struct sensor_gn5_private_data *priv = (struct sensor_gn5_private_data *)cis->sensor_info->priv;

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
	ret = sensor_cis_write_registers(subdev, priv->xtc_prefix);

#ifdef GN5_BURST_WRITE
	ret |= cis->ixc_ops->write16(cis->client, 0xFCFC, 0x4000);
	ret |= cis->ixc_ops->write16(cis->client, 0x6004, 0x0001); // Burst mode enable
	ret |= cis->ixc_ops->write16(cis->client, 0x6028, 0x2002);

	ret |= cis->ixc_ops->write16(cis->client, 0x602A, GN5_XTC_ADDR);

	/* XTC data */
	info("[%s] rom_xtc_cal burst write start(0x%X) size(%d)\n",
			__func__, start_addr, cal_size);
	ret = sensor_gn5_cis_write16_burst(cis->client, 0x6F12,
			(u8 *)&cal_buf[start_addr], cal_size/2, GN5_BIG_ENDIAN);
	if (ret < 0) {
		err("sensor_gn5_cis_write16_burst fail!!");
		goto p_err;
	}
	ret |= cis->ixc_ops->write16(cis->client, 0xFCFC, 0x4000);
	ret |= cis->ixc_ops->write16(cis->client, 0x6004, 0x0002); // Burst mode disable
#else
	ret = cis->ixc_ops->write8_sequential(cis->client, GN5_XTC_ADDR, (u8 *)&cal_buf[start_addr], cal_size);
#endif

	info("[%s] xtc cal write done\n", __func__);

	ret = sensor_cis_write_registers(subdev, priv->xtc_postfix);

p_err:
	IXC_MUTEX_UNLOCK(cis->ixc_lock);
	info("[%s] done (ret:%d)\n", __func__, ret);
	return ret;
}
#endif


/* CIS OPS */
int sensor_gn5_cis_init(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_cis *cis = sensor_cis_get_cis(subdev);

	cis->cis_data->cur_width = cis->sensor_info->max_width;
	cis->cis_data->cur_height = cis->sensor_info->max_height;
	cis->cis_data->low_expo_start = 33000;
	cis->need_mode_change = false;

	cis->cis_data->sens_config_index_pre = SENSOR_GN5_MODE_MAX;
	cis->cis_data->sens_config_index_cur = 0;
	CALL_CISOPS(cis, cis_data_calculation, subdev, cis->cis_data->sens_config_index_cur);

	return ret;
}

static const struct is_cis_log log_gn5[] = {
	{I2C_READ, 16, 0x0000, 0, "model_id"},
	{I2C_READ, 8, 0x0002, 0, "rev_number"},
	{I2C_READ, 8, 0x0005, 0, "frame_count"},
	{I2C_READ, 8, 0x0104, 0, "gph"},
	{I2C_READ, 16, 0x0100, 0, "0x0100"},
	{I2C_READ, 16, 0x6000, 0, "0x6000"},
	{I2C_READ, 16, 0x0340, 0, "fll"},
	{I2C_READ, 16, 0x0342, 0, "llp"},
	{I2C_READ, 16, 0x0B30, 0, "0x0B30"},
	{I2C_READ, 16, 0x0202, 0, "cit"},
	{I2C_READ, 16, 0x0204, 0, "again"},
	{I2C_READ, 16, 0x0702, 0, "0x0702"},
	{I2C_READ, 16, 0x0704, 0, "0x0704"},
	{I2C_READ, 16, 0x0112, 0, "0x0112"},
	{I2C_READ, 16, 0x0300, 0, "0x0300"},
	{I2C_READ, 16, 0x0302, 0, "0x0302"},
	{I2C_READ, 16, 0x0304, 0, "0x0304"},
	{I2C_READ, 16, 0x0306, 0, "0x0306"},
	{I2C_READ, 16, 0x030A, 0, "0x030A"},
	{I2C_READ, 16, 0x030C, 0, "0x030C"},
	{I2C_READ, 16, 0x030E, 0, "0x030E"},
	{I2C_READ, 16, 0xC100, 0, "0xC100"},
	/* 2001 page */
	{I2C_WRITE, 16, 0x6000, 0x0005, "page unlock"},
	{I2C_WRITE, 16, 0xFCFC, 0x2001, "0x2001 page"},
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

int sensor_gn5_cis_log_status(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_cis *cis = sensor_cis_get_cis(subdev);

	sensor_cis_log_status(cis, log_gn5, ARRAY_SIZE(log_gn5), (char *)__func__);

	return ret;
}

int sensor_gn5_cis_set_global_setting(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_cis *cis = sensor_cis_get_cis(subdev);
	struct sensor_gn5_private_data *priv = (struct sensor_gn5_private_data *)cis->sensor_info->priv;
	int i = 0;

	dbg_sensor(1, "[%s] start\n", __func__);

	ret = sensor_cis_write_registers_locked(subdev, priv->global);
	if (ret < 0)
		err("global setting fail!!");

	info("[%s] global setting done\n", __func__);

	/* load sram index */
	for (i = 0; i < priv->max_load_sram_num; i++) {
		IXC_MUTEX_LOCK(cis->ixc_lock);
		ret = sensor_cis_write_registers(subdev, priv->load_sram[i]);
		if (ret < 0)
			err("sensor_gn5_set_registers fail!!");
		IXC_MUTEX_UNLOCK(cis->ixc_lock);
	}

#if IS_ENABLED(CIS_CALIBRATION)
	/* XTC caliberation write */
	sensor_gn5_cis_set_cal(subdev);
#endif

	ret = sensor_cis_write_registers_locked(subdev, priv->prepare_fcm); //prepare FCM settings from #6 sequence
	if (ret < 0)
		err("prepare_fcm fail!!");

	info("[%s] prepare_fcm done\n", __func__);

	return ret;
}

int sensor_gn5_cis_check_12bit(cis_shared_data *cis_data, u32 *next_mode)
{
	int ret = 0;

	if (sensor_gn5_mode_groups[SENSOR_GN5_MODE_IDCG] == MODE_GROUP_NONE) {
		ret = -1;
		goto EXIT;
	}

	switch (cis_data->cur_12bit_mode) {
	case SENSOR_12BIT_STATE_REAL_12BIT:
		*next_mode = sensor_gn5_mode_groups[SENSOR_GN5_MODE_IDCG];
		break;
	case SENSOR_12BIT_STATE_PSEUDO_12BIT:
	default:
		ret = -1;
		break;
	}
EXIT:
	return ret;
}

int sensor_gn5_cis_check_lownoise(cis_shared_data *cis_data, u32 *next_mode)
{
	int ret = 0;
	u32 temp_mode = MODE_GROUP_NONE;

	if (sensor_gn5_mode_groups[SENSOR_GN5_MODE_LN4] == MODE_GROUP_NONE) {
		ret = -1;
		goto EXIT;
	}

	switch (cis_data->cur_lownoise_mode) {
	case IS_CIS_LN4:
		temp_mode = sensor_gn5_mode_groups[SENSOR_GN5_MODE_LN4];
		break;
	case IS_CIS_LN2:
	case IS_CIS_LNOFF:
	default:
		break;
	}

	if (temp_mode == MODE_GROUP_NONE)
		ret = -1;

	if (ret == 0)
		*next_mode = temp_mode;

EXIT:
	return ret;
}

int sensor_gn5_cis_check_cropped_remosaic(cis_shared_data *cis_data, unsigned int mode, unsigned int *next_mode)
{
	int ret = 0;
	u32 zoom_ratio = 0;

	if (sensor_gn5_mode_groups[SENSOR_GN5_MODE_RMS_CROP] == MODE_GROUP_NONE)
		return -EINVAL;

	zoom_ratio = cis_data->cur_remosaic_zoom_ratio;

	if (zoom_ratio != GN5_REMOSAIC_ZOOM_RATIO_X_2)
		return -EINVAL; //goto default

	*next_mode = sensor_gn5_mode_groups[SENSOR_GN5_MODE_RMS_CROP];

	if (zoom_ratio == cis_data->pre_remosaic_zoom_ratio) {
		dbg_sensor(1, "%s : cur zoom ratio is same pre zoom ratio (%u)", __func__, zoom_ratio);
		return ret;
	}

	info("%s : zoom_ratio %u turn on cropped remosaic\n", __func__, zoom_ratio);

	return ret;
}

int sensor_gn5_cis_update_seamless_mode(struct v4l2_subdev *subdev)
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

	next_mode = sensor_gn5_mode_groups[SENSOR_GN5_MODE_NORMAL];
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

	sensor_gn5_cis_check_12bit(cis->cis_data, &next_mode);
	sensor_gn5_cis_check_lownoise(cis->cis_data, &next_mode);
	sensor_gn5_cis_check_cropped_remosaic(cis->cis_data, mode, &next_mode);

	if (mode == next_mode || next_mode == MODE_GROUP_NONE)
		return ret;

	next_mode_info = cis->sensor_info->mode_infos[next_mode];
	if (next_mode_info->setfile_fcm.size == 0) {
		err("check the fcm setting for mode(%d)", next_mode);
		return ret;
	}

	dbg_sensor(1, "[%s][%d] mode(%d) changing to next_mode(%d) fcm_index(0x%X)",
		__func__, cis->cis_data->sen_vsync_count,
		mode, next_mode, next_mode_info->setfile_fcm_index);

	IXC_MUTEX_LOCK(cis->ixc_lock);

	ret |= cis->ixc_ops->write16(cis->client, 0xFCFC, 0x4000);
	ret |= cis->ixc_ops->write16(cis->client, 0x0B32, 0x0100);
	ret |= sensor_cis_write_registers(subdev, next_mode_info->setfile_fcm);
	ret |= cis->ixc_ops->write16(cis->client, 0xFCFC, 0x4000);
	ret |= cis->ixc_ops->write16(cis->client, 0x0B30, next_mode_info->setfile_fcm_index);
	ret |= cis->ixc_ops->write16(cis->client, 0x0340, next_mode_info->frame_length_lines);

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

int sensor_gn5_cis_get_seamless_mode_info(struct v4l2_subdev *subdev)
{
	struct is_cis *cis = sensor_cis_get_cis(subdev);
	cis_shared_data *cis_data = cis->cis_data;
	int ret = 0, cnt = 0;

	if (sensor_gn5_mode_groups[SENSOR_GN5_MODE_LN4] != MODE_GROUP_NONE) {
		cis_data->seamless_mode_info[cnt].mode = SENSOR_MODE_LN4;
		sensor_cis_get_mode_info(subdev, sensor_gn5_mode_groups[SENSOR_GN5_MODE_LN4],
			&cis_data->seamless_mode_info[cnt]);
		cnt++;
	}

	if (sensor_gn5_mode_groups[SENSOR_GN5_MODE_IDCG] != MODE_GROUP_NONE) {
		cis_data->seamless_mode_info[cnt].mode = SENSOR_MODE_REAL_12BIT;
		sensor_cis_get_mode_info(subdev, sensor_gn5_mode_groups[SENSOR_GN5_MODE_IDCG],
			&cis_data->seamless_mode_info[cnt]);
		cnt++;
	}

	if (sensor_gn5_mode_groups[SENSOR_GN5_MODE_RMS_CROP] != MODE_GROUP_NONE) {
		cis_data->seamless_mode_info[cnt].mode = SENSOR_MODE_CROPPED_RMS;
		sensor_cis_get_mode_info(subdev, sensor_gn5_mode_groups[SENSOR_GN5_MODE_RMS_CROP],
			&cis_data->seamless_mode_info[cnt]);
		cnt++;
	}

	cis_data->seamless_mode_cnt = cnt;

	return ret;
}

int sensor_gn5_cis_mode_change(struct v4l2_subdev *subdev, u32 mode)
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

	ret = cis->ixc_ops->write16(cis->client, 0xFCFC, 0x4000);
	ret |= cis->ixc_ops->write16(cis->client, 0x0B30, 0x01FF);

	mode_info = cis->sensor_info->mode_infos[mode];
	ret = sensor_cis_write_registers_locked(subdev, mode_info->setfile);
	if (ret < 0) {
		err("sensor_gn5_set_registers fail!!");
		goto p_err;
	}

	sensor_gn5_cis_set_mode_group(mode);
	sensor_gn5_cis_get_seamless_mode_info(subdev);

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

	info("[%s] mode changed(%d)\n", __func__, mode);
	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %lldus", __func__, PABLO_KTIME_US_DELTA_NOW(st));

p_err:
	return ret;
}

int sensor_gn5_cis_stream_on(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_cis *cis = sensor_cis_get_cis(subdev);
	struct is_device_sensor *device;
	ktime_t st = ktime_get();

	device = (struct is_device_sensor *)v4l2_get_subdev_hostdata(subdev);
	WARN_ON(!device);

	is_vendor_set_mipi_clock(device);

	/* Sensor stream on */
	info("%s\n", __func__);
	IXC_MUTEX_LOCK(cis->ixc_lock);
	ret = cis->ixc_ops->write8(cis->client, 0x0100, 0x01);
	IXC_MUTEX_UNLOCK(cis->ixc_lock);

	cis->cis_data->stream_on = true;

	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %lldus", __func__, PABLO_KTIME_US_DELTA_NOW(st));

	return ret;
}

int sensor_gn5_cis_stream_off(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_cis *cis = sensor_cis_get_cis(subdev);
	u8 frame_count = 0;
	ktime_t st = ktime_get();

	dbg_sensor(1, "[MOD:D:%d] %s\n", cis->id, __func__);

	cis->cis_data->stream_on = false;

	IXC_MUTEX_LOCK(cis->ixc_lock);
	cis->ixc_ops->read8(cis->client, 0x0005, &frame_count);
	ret = cis->ixc_ops->write8(cis->client, 0x0100, 0x00);
	IXC_MUTEX_UNLOCK(cis->ixc_lock);

	info("%s done frame_count(%d)\n", __func__, frame_count);

	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %lldus", __func__, PABLO_KTIME_US_DELTA_NOW(st));

	return ret;
}

int sensor_gn5_cis_set_wb_gain(struct v4l2_subdev *subdev, struct wb_gains wb_gains)
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
	IXC_MUTEX_UNLOCK(cis->ixc_lock);

	if (ret < 0)
		err("sensor_gn5_cis_set_wb_gain fail!!");

	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %lldus", __func__, PABLO_KTIME_US_DELTA_NOW(st));

	return ret;
}

int sensor_gn5_cis_get_updated_binning_ratio(struct v4l2_subdev *subdev, u32 *binning_ratio)
{
	struct is_cis *cis = sensor_cis_get_cis(subdev);
	cis_shared_data *cis_data = cis->cis_data;
	u32 rms_mode = sensor_gn5_mode_groups[SENSOR_GN5_MODE_RMS_CROP];
	u32 zoom_ratio;

	if (rms_mode == MODE_GROUP_NONE)
		return 0;

	zoom_ratio = cis_data->pre_remosaic_zoom_ratio;

	if (zoom_ratio == GN5_REMOSAIC_ZOOM_RATIO_X_2 && sensor_gn5_rms_binning_ratio[rms_mode])
		*binning_ratio = sensor_gn5_rms_binning_ratio[rms_mode];

	return 0;
}

int sensor_gn5_cis_set_group_param_hold(struct v4l2_subdev *subdev, bool hold)
{
	int ret = 0;
	struct is_cis *cis = sensor_cis_get_cis(subdev);
	cis_shared_data *cis_data = cis->cis_data;

	if (cis_data->cur_hdr_mode != SENSOR_HDR_MODE_2AEB_2VC) {
		ret = sensor_cis_set_group_param_hold(subdev, hold);

		/* skip N+1 control after update seamless at N frame */
		if (cis_data->update_seamless_state)
			cis_data->skip_control_vsync_count = cis_data->sen_vsync_count + 1;
	}

	return ret;
}

int sensor_gn5_cis_set_analog_gain(struct v4l2_subdev *subdev, struct ae_param *again)
{
	int ret = 0;
	struct is_cis *cis = sensor_cis_get_cis(subdev);
	cis_shared_data *cis_data = cis->cis_data;

	if (cis_data->is_data.scene_mode == AA_SCENE_MODE_PRO_MODE)
		cis_data->min_analog_gain[0] = 0x30;

	ret = sensor_cis_set_analog_gain(subdev, again);

	return ret;
}

int sensor_gn5_cis_wait_seamless_update_delay(struct v4l2_subdev *subdev)
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

int sensor_gn5_cis_wait_streamoff(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_cis *cis = sensor_cis_get_cis(subdev);
	u32 mode = 0;
	ktime_t st = ktime_get();

	ret = sensor_cis_wait_streamoff(subdev);
	if (ret < 0)
		goto p_err;

	/* HACK : stream_on failure when changing from ssm to 4:3 mode. */
	mode = cis->cis_data->sens_config_index_cur;
	if (mode == SENSOR_GN5_2040X1148_240FPS_R10)
		msleep(20);

p_err:
	info("[%s] : ret(%d), mode(%d), time(%lldus)\n", __func__, ret, mode, PABLO_KTIME_US_DELTA_NOW(st));

	return ret;
}

static struct is_cis_ops cis_ops_gn5 = {
	.cis_init = sensor_gn5_cis_init,
	.cis_init_state = sensor_cis_init_state,
	.cis_log_status = sensor_gn5_cis_log_status,
	.cis_group_param_hold = sensor_gn5_cis_set_group_param_hold,
	.cis_set_global_setting = sensor_gn5_cis_set_global_setting,
	.cis_mode_change = sensor_gn5_cis_mode_change,
	.cis_stream_on = sensor_gn5_cis_stream_on,
	.cis_stream_off = sensor_gn5_cis_stream_off,
	.cis_wait_streamon = sensor_cis_wait_streamon,
	.cis_wait_streamoff = sensor_gn5_cis_wait_streamoff,
	.cis_data_calculation = sensor_cis_data_calculation,
	.cis_set_exposure_time = sensor_cis_set_exposure_time,
	.cis_get_min_exposure_time = sensor_cis_get_min_exposure_time,
	.cis_get_max_exposure_time = sensor_cis_get_max_exposure_time,
	.cis_adjust_frame_duration = sensor_cis_adjust_frame_duration,
	.cis_set_frame_duration = sensor_cis_set_frame_duration,
	.cis_set_frame_rate = sensor_cis_set_frame_rate,
	.cis_adjust_analog_gain = sensor_cis_adjust_analog_gain,
	.cis_set_analog_gain = sensor_gn5_cis_set_analog_gain,
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
	.cis_set_wb_gains = sensor_gn5_cis_set_wb_gain,
	.cis_update_seamless_mode = sensor_gn5_cis_update_seamless_mode,
	.cis_seamless_ctl_before_stream = sensor_cis_seamless_ctl_before_stream,
	.cis_update_seamless_state = sensor_cis_update_seamless_state,
	.cis_wait_seamless_update_delay = sensor_gn5_cis_wait_seamless_update_delay,
	.cis_get_updated_binning_ratio = sensor_gn5_cis_get_updated_binning_ratio,
};

int cis_gn5_probe_i2c(struct i2c_client *client,
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
	cis->cis_ops = &cis_ops_gn5;
	/* belows are depend on sensor cis. MUST check sensor spec */
	cis->bayer_order = OTF_INPUT_ORDER_BAYER_GR_BG;
	cis->use_wb_gain = true;
	cis->reg_addr = &sensor_gn5_reg_addr;

	ret = of_property_read_string(dnode, "setfile", &setfile);
	if (ret) {
		err("setfile index read fail(%d), take default setfile!!", ret);
		setfile = "default";
	}

	if (strcmp(setfile, "default") == 0 || strcmp(setfile, "setA") == 0)
		probe_info("[%s] setfile_A mclk: 19.2Mhz \n", __func__);
	else
		err("setfile index out of bound, take default (setfile_A mclk: 26Mhz)");

	cis->sensor_info = &sensor_gn5_info_A;

	is_vendor_set_mipi_mode(cis);

	probe_info("%s done\n", __func__);
	return ret;
}

static const struct of_device_id sensor_cis_gn5_match[] = {
	{
		.compatible = "samsung,exynos-is-cis-gn5",
	},
	{},
};
MODULE_DEVICE_TABLE(of, sensor_cis_gn5_match);

static const struct i2c_device_id sensor_cis_gn5_idt[] = {
	{ SENSOR_NAME, 0 },
	{},
};

static struct i2c_driver sensor_cis_gn5_driver = {
	.probe	= cis_gn5_probe_i2c,
	.driver = {
		.name	= SENSOR_NAME,
		.owner	= THIS_MODULE,
		.of_match_table = sensor_cis_gn5_match,
		.suppress_bind_attrs = true,
	},
	.id_table = sensor_cis_gn5_idt
};

#ifdef MODULE
builtin_i2c_driver(sensor_cis_gn5_driver);
#else
static int __init sensor_cis_gn5_init(void)
{
	int ret;

	ret = i2c_add_driver(&sensor_cis_gn5_driver);
	if (ret)
		err("failed to add %s driver: %d\n",
			sensor_cis_gn5_driver.driver.name, ret);

	return ret;
}
late_initcall_sync(sensor_cis_gn5_init);
#endif

MODULE_LICENSE("GPL");
MODULE_SOFTDEP("pre: fimc-is");
