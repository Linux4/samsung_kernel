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
#include "is-cis-gn8.h"
#include "is-cis-gn8-setA.h"

#include "is-helper-ixc.h"
#include "is-vender.h"
#include "is-sec-define.h"

#include "interface/is-interface-library.h"
#include "is-interface-sensor.h"
#define SENSOR_NAME "GN8"

static u16 firstBoot;

/* temp change, remove once PDAF settings done */
//#define PDAF_DISABLE
#ifdef PDAF_DISABLE
const u16 sensor_gn8_pdaf_off[] = {
	0xFCFC,	0x2000,	0x02,
	0x4751, 0x0000, 0x02,

	0x6000, 0x0005, 0x02,
	0xFCFC, 0x4000, 0x02,
	0x0115, 0x0000, 0x02,
	0x0720, 0x0000, 0x02,
	0x6000, 0x0085, 0x02,
};

const struct sensor_regs pdaf_off = SENSOR_REGS(sensor_gn8_pdaf_off);
#endif

void sensor_gn8_cis_set_mode_group(u32 mode)
{
	sensor_gn8_mode_groups[SENSOR_GN8_MODE_DEFAULT] = mode;
	sensor_gn8_mode_groups[SENSOR_GN8_MODE_RMS_CROP] = MODE_GROUP_NONE;

	switch (mode) {
	case SENSOR_GN8_4080X3060_4SUM_2HVBIN_30FPS:
		sensor_gn8_mode_groups[SENSOR_GN8_MODE_DEFAULT] =
			SENSOR_GN8_4080X3060_4SUM_2HVBIN_30FPS_FCM;
		sensor_gn8_mode_groups[SENSOR_GN8_MODE_RMS_CROP] =
			SENSOR_GN8_4080X3060_REMOSAIC_CROP_30FPS_FCM;
		break;
	case SENSOR_GN8_4080X2296_4SUM_2HVBIN_30FPS:
		sensor_gn8_mode_groups[SENSOR_GN8_MODE_DEFAULT] =
			SENSOR_GN8_4080X2296_4SUM_2HVBIN_30FPS_FCM;
		sensor_gn8_mode_groups[SENSOR_GN8_MODE_RMS_CROP] =
			SENSOR_GN8_4080X2296_REMOSAIC_CROP_30FPS_FCM;
		break;
	}

	info("[%s] default(%d) rms_crop(%d)\n", __func__,
		sensor_gn8_mode_groups[SENSOR_GN8_MODE_DEFAULT],
		sensor_gn8_mode_groups[SENSOR_GN8_MODE_RMS_CROP]);
}

#ifdef GN8_BURST_WRITE
int sensor_gn8_cis_write16_burst(struct i2c_client *client, u16 addr, u8 *val, u32 num, bool endian)
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
	wbuf[0] = (addr & 0xFF00) >> 8;
	wbuf[1] = (addr & 0xFF);
	if (endian == GN8_BIG_ENDIAN) {
		memcpy(wbuf+2, val, num * 2);
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

int sensor_gn8_cis_XTC_write(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_cis *cis = sensor_cis_get_cis(subdev);
	struct is_device_sensor_peri *sensor_peri = NULL;
	int position, cal_size = 0;
	u16 start_addr, end_addr;

	ulong cal_addr;
	u8 *cal_data = NULL;
	char *cal_buf = NULL;
	struct sensor_gn8_private_data *priv = (struct sensor_gn8_private_data *)cis->sensor_info->priv;

	sensor_peri = container_of(cis, struct is_device_sensor_peri, cis);
	WARN_ON(!sensor_peri);

	position = sensor_peri->module->position;

	ret = is_sec_get_cal_buf(&cal_buf, position);
	if (ret < 0)
		goto p_err;

	cal_addr = (ulong)cal_buf;

	if (position == SENSOR_POSITION_REAR) {
		cal_addr += SENSOR_GN8_UXTC_SENS_CAL_ADDR;
		start_addr = SENSOR_GN8_UXTC_SENS_CAL_ADDR;
		cal_size = SENSOR_GN8_UXTC_SENS_CAL_SIZE;
	} else {
		err("cis_gn8 position(%d) is invalid!\n", position);
		goto p_err;
	}

	cal_data = (u8 *)cal_addr;
	end_addr = start_addr + cal_size;

	info("[%s] xtc_prefix write start\n", __func__);
	ret = sensor_cis_write_registers(subdev, priv->xtc_prefix);

#ifdef GN8_BURST_WRITE
	ret |= cis->ixc_ops->write16(cis->client, 0xFCFC, 0x4000);
	ret |= cis->ixc_ops->write16(cis->client, 0x6004, 0x0001); // Burst mode enable
	ret |= cis->ixc_ops->write16(cis->client, 0x6028, 0x2003);

	ret |= cis->ixc_ops->write16(cis->client, 0x602A, 0xB330);

	/* XTC data */
	info("[%s] rom_xtc_cal burst write start(0x%X) end(0x%X) size(%d)\n",
			__func__, start_addr, end_addr, cal_size);
	ret = sensor_gn8_cis_write16_burst(cis->client, 0x6F12,
			cal_data, cal_size/2, GN8_BIG_ENDIAN);
	if (ret < 0) {
		err("sensor_gn8_cis_write16_burst fail!!");
		goto p_err;
	}
	ret |= cis->ixc_ops->write16(cis->client, 0xFCFC, 0x4000);
	ret |= cis->ixc_ops->write16(cis->client, 0x6004, 0x0000); // Burst mode disable
#else
	ret = cis->ixc_ops->write8_sequential(cis->client, 0xB330, cal_data, cal_size);
#endif

	info("[%s] eeprom write done, xtc_postfix write start\n", __func__);

	ret = sensor_cis_write_registers(subdev, priv->xtc_postfix);

p_err:
	info("[%s] done (ret:%d)\n", __func__, ret);
	return ret;
}

/* CIS OPS */
int sensor_gn8_cis_init(struct v4l2_subdev *subdev)
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

	sensor_gn8_mode_groups[SENSOR_GN8_MODE_DEFAULT] = MODE_GROUP_NONE;
	sensor_gn8_mode_groups[SENSOR_GN8_MODE_RMS_CROP] = MODE_GROUP_NONE;
	firstBoot = 0;

	cis->cis_data->sens_config_index_pre = SENSOR_GN8_MODE_MAX;
	cis->cis_data->sens_config_index_cur = 0;
	CALL_CISOPS(cis, cis_data_calculation, subdev, cis->cis_data->sens_config_index_cur);

	is_vendor_set_mipi_mode(cis);

	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %ldus", __func__, PABLO_KTIME_US_DELTA_NOW(st));

	return ret;
}

static const struct is_cis_log log_gn8[] = {
	{I2C_READ, 16, 0x0000, 0, "model_id"},
	{I2C_READ, 8, 0x0002, 0, "rev_number"},
	{I2C_READ, 8, 0x0005, 0, "frame_count"},
	{I2C_READ, 16, 0x0100, 0, "mode_select"},
	{I2C_READ, 16, 0x0340, 0, "fll"},
	{I2C_READ, 16, 0x0342, 0, "llp"},
};

int sensor_gn8_cis_log_status(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_cis *cis = sensor_cis_get_cis(subdev);

	sensor_cis_log_status(cis, log_gn8, ARRAY_SIZE(log_gn8), (char *)__func__);

	return ret;
}

int sensor_gn8_cis_set_global_setting(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_cis *cis = sensor_cis_get_cis(subdev);
	struct sensor_gn8_private_data *priv = (struct sensor_gn8_private_data *)cis->sensor_info->priv;
	int i = 0;

	info("[%s] start\n", __func__);

	ret = sensor_cis_write_registers_locked(subdev, priv->global);
	if (ret < 0)
		err("global setting fail!!");

	info("[%s] global setting done\n", __func__);

	ret = cis->ixc_ops->write16(cis->client, 0xFCFC, 0x4000);
	/* load sram index */
	for (i = 0; i < priv->max_load_sram_num; i++) {
		info("[%s] max_load_sram_num%d", __func__, priv->max_load_sram_num);
		IXC_MUTEX_LOCK(cis->ixc_lock);
		ret = sensor_cis_write_registers(subdev, priv->load_sram[i]);
		IXC_MUTEX_UNLOCK(cis->ixc_lock);
		if (ret < 0)
			err("sensor_gn8_set_registers fail!!");
	}

	/* XTC caliberation write */
	sensor_gn8_cis_XTC_write(subdev);

	ret = sensor_cis_write_registers_locked(subdev, priv->prepare_fcm); //prepare FCM settings from #5 sequence
	if (ret < 0)
		err("prepare_fcm fail!!");

	firstBoot = 0;

	info("[%s] prepare_fcm done\n", __func__);

	return ret;
}

int sensor_gn8_cis_check_cropped_remosaic(cis_shared_data *cis_data, unsigned int mode, unsigned int *next_mode)
{
	int ret = 0;
	u32 zoom_ratio = 0;

	if (sensor_gn8_mode_groups[SENSOR_GN8_MODE_RMS_CROP] == MODE_GROUP_NONE)
		return -EINVAL;

	zoom_ratio = cis_data->pre_remosaic_zoom_ratio;

	if (zoom_ratio != GN8_REMOSAIC_ZOOM_RATIO_X_2)
		return -EINVAL; //goto default

	*next_mode = sensor_gn8_mode_groups[SENSOR_GN8_MODE_RMS_CROP];

	if (*next_mode != cis_data->sens_config_index_cur)
		info("[%s] zoom_ratio %u turn on cropped remosaic\n", __func__, zoom_ratio);

	return ret;
}

int sensor_gn8_cis_update_seamless_mode_on_vsync(struct v4l2_subdev *subdev)
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

	next_mode = sensor_gn8_mode_groups[SENSOR_GN8_MODE_DEFAULT];
	if (next_mode == MODE_GROUP_NONE) {
		err("mode group is none");
		return -EINVAL;
	}

	sensor_gn8_cis_check_cropped_remosaic(cis->cis_data, mode, &next_mode);

	if (mode == next_mode || next_mode == MODE_GROUP_NONE)
		return ret;

	next_mode_info = cis->sensor_info->mode_infos[next_mode];

	/* New mode settings Update */
	info("%s mode(%d) next_mode(%d)\n", __func__, mode, next_mode);

	IXC_MUTEX_LOCK(cis->ixc_lock);

	cis->ixc_ops->write16(cis->client, 0xFCFC, 0x4000);
	cis->ixc_ops->write16(cis->client, 0x6000, 0x0005);
	cis->ixc_ops->write16(cis->client, cis->reg_addr->group_param_hold, 0x0101);

	ret = sensor_cis_write_registers(subdev, next_mode_info->setfile);
	if (ret < 0)
		err("sensor_gn8_set_registers fail!!");

	cis->ixc_ops->write16(cis->client, 0xFCFC, 0x4000);
	cis->ixc_ops->write16(cis->client, 0x0B32, 0x0000);

	ret = cis->ixc_ops->write16(cis->client, 0x0B30, next_mode_info->setfile_fcm_index);
	ret |= cis->ixc_ops->write16(cis->client, 0x0340, next_mode_info->frame_length_lines);
	cis->ixc_ops->write16(cis->client, 0x6000, 0x0085);

	IXC_MUTEX_UNLOCK(cis->ixc_lock);

	if (ret < 0)
		err("setting fcm index fail!!");

	cis_data->sens_config_index_pre = cis->cis_data->sens_config_index_cur;
	cis_data->sens_config_index_cur = next_mode;
	cis->cis_data->pre_12bit_mode = cis->cis_data->cur_12bit_mode;

	CALL_CISOPS(cis, cis_data_calculation, subdev, next_mode);

	ret = CALL_CISOPS(cis, cis_set_analog_gain, cis->subdev, &cis_data->last_again);
	if (ret < 0)
		err("err!!! cis_set_analog_gain ret(%d)", ret);

	ret = CALL_CISOPS(cis, cis_set_digital_gain, cis->subdev, &cis_data->last_dgain);
	if (ret < 0)
		err("err!!! cis_set_digital_gain ret(%d)", ret);

	ret = CALL_CISOPS(cis, cis_set_exposure_time, cis->subdev, &cis_data->last_exp);
	if (ret < 0)
		err("err!!! cis_set_exposure_time ret(%d)", ret);

#ifdef PDAF_DISABLE
	sensor_cis_write_registers(subdev, pdaf_off);
	info("[%s] test only (pdaf_off)\n", __func__);
#endif

	IXC_MUTEX_LOCK(cis->ixc_lock);
	cis->ixc_ops->write16(cis->client, cis->reg_addr->group_param_hold, 0x0001);
	IXC_MUTEX_UNLOCK(cis->ixc_lock);

	info("[%s] pre(%d)->cur(%d), 12bit[%d] LN[%d] AEB[%d] zoom [%d]\n",
		__func__,
		cis->cis_data->sens_config_index_pre, cis->cis_data->sens_config_index_cur,
		cis->cis_data->cur_12bit_mode,
		cis->cis_data->cur_lownoise_mode,
		cis->cis_data->cur_hdr_mode,
		cis_data->cur_remosaic_zoom_ratio);

	return ret;
}

int sensor_gn8_cis_get_seamless_mode_info(struct v4l2_subdev *subdev)
{
	struct is_cis *cis = sensor_cis_get_cis(subdev);
	cis_shared_data *cis_data = cis->cis_data;
	int ret = 0, cnt = 0;

	if (sensor_gn8_mode_groups[SENSOR_GN8_MODE_RMS_CROP] != MODE_GROUP_NONE) {
		cis_data->seamless_mode_info[cnt].mode = SENSOR_MODE_CROPPED_RMS;
		sensor_cis_get_mode_info(subdev, sensor_gn8_mode_groups[SENSOR_GN8_MODE_RMS_CROP],
			&cis_data->seamless_mode_info[cnt]);
		cnt++;
	}

	cis_data->seamless_mode_cnt = cnt;

	return ret;
}

int sensor_gn8_cis_mode_change(struct v4l2_subdev *subdev, u32 mode)
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

	sensor_gn8_cis_set_mode_group(mode);
	if (SENSOR_GN8_4080X2296_4SUM_2HVBIN_30FPS == mode || (SENSOR_GN8_4080X3060_4SUM_2HVBIN_30FPS == mode && firstBoot == 0)) {
		info("[%s] sensor mode(%d) default mode(%d)\n", __func__, mode, sensor_gn8_mode_groups[SENSOR_GN8_MODE_DEFAULT]);
		mode = sensor_gn8_mode_groups[SENSOR_GN8_MODE_DEFAULT];
	}

	cis->mipi_clock_index_cur = CAM_MIPI_NOT_INITIALIZED;
	info("[%s] sensor mode(%d)\n", __func__, mode);

	ret = cis->ixc_ops->write16(cis->client, 0xFCFC, 0x4000);
	if (mode == SENSOR_GN8_4080X3060_4SUM_2HVBIN_30FPS_FCM || mode == SENSOR_GN8_4080X2296_4SUM_2HVBIN_30FPS_FCM) {
		ret |= cis->ixc_ops->write16(cis->client, 0x0B30, 0x0101);
		ret |= cis->ixc_ops->write16(cis->client, 0x0340, 0x19A0);
	} else {
		ret |= cis->ixc_ops->write16(cis->client, 0x0B30, 0x01FF);
	}


	mode_info = cis->sensor_info->mode_infos[mode];
	ret = sensor_cis_write_registers_locked(subdev, mode_info->setfile);
	if (ret < 0) {
		err("sensor_gn8_set_registers fail!!");
		goto EXIT;
	}

	cis->cis_data->sens_config_index_cur = sensor_gn8_mode_groups[SENSOR_GN8_MODE_DEFAULT];

	if (sensor_gn8_mode_groups[SENSOR_GN8_MODE_RMS_CROP] == MODE_GROUP_NONE)
		cis->use_notify_vsync = false;
	else
		cis->use_notify_vsync = true;

	cis->cis_data->sens_config_index_pre = mode;
	cis->cis_data->remosaic_mode = mode_info->remosaic_mode;
	cis->cis_data->pre_remosaic_zoom_ratio = 0;
	cis->cis_data->cur_remosaic_zoom_ratio = 0;
	cis->cis_data->pre_12bit_mode = mode_info->state_12bit;

	/* Disable Embedded Data Line */
	IXC_MUTEX_LOCK(cis->ixc_lock);
	cis->ixc_ops->write8(cis->client, 0x0118, 0x00);
	IXC_MUTEX_UNLOCK(cis->ixc_lock);

#ifdef PDAF_DISABLE
	sensor_cis_write_registers(subdev, pdaf_off);
	info("[%s] test only (pdaf_off)\n", __func__);
#endif

	info("[%s] mode changed(%d)\n", __func__, mode);

	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %ldus", __func__, PABLO_KTIME_US_DELTA_NOW(st));

	firstBoot = 0;
	if (SENSOR_GN8_2040X1532_4SUM_4HVBIN_120FPS == mode) {
		firstBoot = 1;
	}

EXIT:
	sensor_gn8_cis_get_seamless_mode_info(subdev);
p_err:
	return ret;
}

int sensor_gn8_cis_stream_on(struct v4l2_subdev *subdev)
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
	ret = cis->ixc_ops->write16(cis->client, 0xFCFC, 0x4000);
	ret = cis->ixc_ops->write16(cis->client, 0x6000, 0x0005);
	ret = cis->ixc_ops->write16(cis->client, 0x0100, 0x0100);
	ret = cis->ixc_ops->write16(cis->client, 0x6000, 0x0085);

	IXC_MUTEX_UNLOCK(cis->ixc_lock);

	cis->cis_data->stream_on = true;

	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %ldus", __func__, PABLO_KTIME_US_DELTA_NOW(st));

	return ret;
}

int sensor_gn8_cis_stream_off(struct v4l2_subdev *subdev)
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
	ret = cis->ixc_ops->write8(cis->client, GN8_SETUP_MODE_SELECT_ADDR, 0x00);
	IXC_MUTEX_UNLOCK(cis->ixc_lock);

	info("%s done frame_count(%d)\n", __func__, frame_count);

	cis->cis_data->stream_on = false;

	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %ldus", __func__, PABLO_KTIME_US_DELTA_NOW(st));

	return ret;
}

int sensor_gn8_cis_set_wb_gain(struct v4l2_subdev *subdev, struct wb_gains wb_gains)
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
		return ret;

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
		err("sensor_gn8_cis_set_wb_gain fail!!");

	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %ldus", __func__, PABLO_KTIME_US_DELTA_NOW(st));

	return ret;
}

int sensor_gn8_cis_get_updated_binning_ratio(struct v4l2_subdev *subdev, u32 *binning_ratio)
{
	struct is_cis *cis = sensor_cis_get_cis(subdev);
	cis_shared_data *cis_data = cis->cis_data;
	u32 rms_mode = sensor_gn8_mode_groups[SENSOR_GN8_MODE_RMS_CROP];
	u32 zoom_ratio;

	if (rms_mode == MODE_GROUP_NONE)
		return 0;

	zoom_ratio = cis_data->pre_remosaic_zoom_ratio;

	if (zoom_ratio == GN8_REMOSAIC_ZOOM_RATIO_X_2 && sensor_gn8_rms_binning_ratio[rms_mode])
		*binning_ratio = sensor_gn8_rms_binning_ratio[rms_mode];

	return 0;
}

int sensor_gn8_cis_notify_vsync(struct v4l2_subdev *subdev)
{
	return sensor_gn8_cis_update_seamless_mode_on_vsync(subdev);
}

static struct is_cis_ops cis_ops_gn8 = {
	.cis_init = sensor_gn8_cis_init,
	.cis_log_status = sensor_gn8_cis_log_status,
	.cis_group_param_hold = sensor_cis_set_group_param_hold,
	.cis_set_global_setting = sensor_gn8_cis_set_global_setting,
	.cis_mode_change = sensor_gn8_cis_mode_change,
	.cis_stream_on = sensor_gn8_cis_stream_on,
	.cis_stream_off = sensor_gn8_cis_stream_off,
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
	.cis_set_wb_gains = sensor_gn8_cis_set_wb_gain,
	.cis_get_updated_binning_ratio = sensor_gn8_cis_get_updated_binning_ratio,
	.cis_notify_vsync = sensor_gn8_cis_notify_vsync,
};

int cis_gn8_probe_i2c(struct i2c_client *client,
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
	cis->cis_ops = &cis_ops_gn8;
	/* belows are depend on sensor cis. MUST check sensor spec */
	cis->bayer_order = OTF_INPUT_ORDER_BAYER_GR_BG;
	cis->use_wb_gain = true;
	cis->use_seamless_mode = true;
	cis->reg_addr = &sensor_gn8_reg_addr;

	ret = of_property_read_string(dnode, "setfile", &setfile);
	if (ret) {
		err("setfile index read fail(%d), take default setfile!!", ret);
		setfile = "default";
	}

	if (strcmp(setfile, "default") == 0 || strcmp(setfile, "setA") == 0)
		probe_info("[%s] setfile_A mclk: 26Mhz \n", __func__);
	else
		err("setfile index out of bound, take default (setfile_A mclk: 26Mhz)");

	cis->sensor_info = &sensor_gn8_info_A;

	probe_info("%s done\n", __func__);
	return ret;
}

static const struct of_device_id sensor_cis_gn8_match[] = {
	{
		.compatible = "samsung,exynos-is-cis-gn8",
	},
	{},
};
MODULE_DEVICE_TABLE(of, sensor_cis_gn8_match);

static const struct i2c_device_id sensor_cis_gn8_idt[] = {
	{ SENSOR_NAME, 0 },
	{},
};

static struct i2c_driver sensor_cis_gn8_driver = {
	.probe	= cis_gn8_probe_i2c,
	.driver = {
		.name	= SENSOR_NAME,
		.owner	= THIS_MODULE,
		.of_match_table = sensor_cis_gn8_match,
		.suppress_bind_attrs = true,
	},
	.id_table = sensor_cis_gn8_idt
};

#ifdef MODULE
builtin_i2c_driver(sensor_cis_gn8_driver);
#else
static int __init sensor_cis_gn8_init(void)
{
	int ret;

	ret = i2c_add_driver(&sensor_cis_gn8_driver);
	if (ret)
		err("failed to add %s driver: %d\n",
			sensor_cis_gn8_driver.driver.name, ret);

	return ret;
}
late_initcall_sync(sensor_cis_gn8_init);
#endif

MODULE_LICENSE("GPL");
MODULE_SOFTDEP("pre: fimc-is");
