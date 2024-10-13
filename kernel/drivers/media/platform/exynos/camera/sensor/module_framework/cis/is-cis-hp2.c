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
#include "is-cis-hp2.h"
#include "is-cis-hp2-setA-19p2.h"
#include "is-helper-ixc.h"
#ifdef CONFIG_CAMERA_VENDOR_MCD
#include "is-sec-define.h"
#endif

#define SENSOR_NAME "S5KHP2"

static unsigned int sensor_hp2_dcg_cal_disable;

#ifdef USE_SENSOR_DEBUG
static const struct kernel_param_ops param_ops_dcg_cal_disable = {
	.set = param_set_uint,
	.get = NULL,
};

/**
 * Command : adb shell "echo 1 > /sys/module/is_cis_hp2/parameters/dcg_cal_disable"
 * @param 0 Enable[0] Disable[1] dcg cal
 */
module_param_cb(dcg_cal_disable, &param_ops_dcg_cal_disable, &sensor_hp2_dcg_cal_disable, 0644);
#endif

void sensor_hp2_cis_set_mode_group(u32 mode)
{
	sensor_hp2_mode_groups[SENSOR_HP2_MODE_DEFAULT] = mode;
	sensor_hp2_mode_groups[SENSOR_HP2_MODE_LN2] = MODE_GROUP_NONE;
	sensor_hp2_mode_groups[SENSOR_HP2_MODE_LN4] = MODE_GROUP_NONE;
	sensor_hp2_mode_groups[SENSOR_HP2_MODE_IDCG] = MODE_GROUP_NONE;
	sensor_hp2_mode_groups[SENSOR_HP2_MODE_RMS_CROP] = MODE_GROUP_NONE;

	sensor_hp2_rms_zoom_mode[SENSOR_HP2_RMS_ZOOM_MODE_X_17] = SENSOR_HP2_RMS_ZOOM_MODE_NONE;
	sensor_hp2_rms_zoom_mode[SENSOR_HP2_RMS_ZOOM_MODE_X_20] = SENSOR_HP2_RMS_ZOOM_MODE_NONE;

	switch (mode) {
	case SENSOR_HP2_4080x3060_60FPS_VPD:
		sensor_hp2_mode_groups[SENSOR_HP2_MODE_LN4] = SENSOR_HP2_4080x3060_15FPS_VPD_LN4;
		break;

	case SENSOR_HP2_4080x3060_60FPS_12BIT_VPD:
		sensor_hp2_mode_groups[SENSOR_HP2_MODE_LN4] = SENSOR_HP2_4080x3060_15FPS_12BIT_VPD_LN4;
		break;

	case SENSOR_HP2_4000x3000_60FPS_VPD:
		sensor_hp2_mode_groups[SENSOR_HP2_MODE_LN4] = SENSOR_HP2_4000x3000_15FPS_VPD_LN4;
		sensor_hp2_mode_groups[SENSOR_HP2_MODE_RMS_CROP] = MODE_GROUP_RMS;
		sensor_hp2_rms_zoom_mode[SENSOR_HP2_RMS_ZOOM_MODE_X_17] = SENSOR_HP2_4000x3000_30FPS_4SUM_BDS_VPD;
		sensor_hp2_rms_zoom_mode[SENSOR_HP2_RMS_ZOOM_MODE_X_20] = SENSOR_HP2_4000x3000_60FPS_4SUM_VPD;
		break;

	case SENSOR_HP2_4000x3000_60FPS_12BIT:
		sensor_hp2_mode_groups[SENSOR_HP2_MODE_IDCG] = SENSOR_HP2_4000x3000_60FPS_12BIT_IDCG;
		sensor_hp2_mode_groups[SENSOR_HP2_MODE_RMS_CROP] = MODE_GROUP_RMS;
		sensor_hp2_rms_zoom_mode[SENSOR_HP2_RMS_ZOOM_MODE_X_17] = SENSOR_HP2_4000x3000_30FPS_4SUM_BDS;
		break;

	case SENSOR_HP2_4000x2252_60FPS_12BIT:
		sensor_hp2_mode_groups[SENSOR_HP2_MODE_LN2] = SENSOR_HP2_4000x2252_60FPS_12BIT_LN2;
		sensor_hp2_mode_groups[SENSOR_HP2_MODE_LN4] = SENSOR_HP2_4000x2252_30FPS_12BIT_LN4;
		sensor_hp2_mode_groups[SENSOR_HP2_MODE_IDCG] = SENSOR_HP2_4000x2252_60FPS_12BIT_IDCG;
		sensor_hp2_mode_groups[SENSOR_HP2_MODE_RMS_CROP] = MODE_GROUP_RMS;
		sensor_hp2_rms_zoom_mode[SENSOR_HP2_RMS_ZOOM_MODE_X_17] = SENSOR_HP2_4000x2252_60FPS_4SUM_BDS;
		sensor_hp2_rms_zoom_mode[SENSOR_HP2_RMS_ZOOM_MODE_X_20] = SENSOR_HP2_4000x2252_60FPS_4SUM;
		break;
	}

	info("[%s] default(%d) idcg(%d) ln2(%d) ln4(%d) rms(%d)\n", __func__,
		sensor_hp2_mode_groups[SENSOR_HP2_MODE_DEFAULT],
		sensor_hp2_mode_groups[SENSOR_HP2_MODE_IDCG],
		sensor_hp2_mode_groups[SENSOR_HP2_MODE_LN2],
		sensor_hp2_mode_groups[SENSOR_HP2_MODE_LN4],
		sensor_hp2_mode_groups[SENSOR_HP2_MODE_RMS_CROP]);
}

int sensor_hp2_cis_select_setfile(struct v4l2_subdev *subdev)
{
	int ret = 0;
	u16 rev = 0;
	u8 feature_id = 0;
	struct is_cis *cis = sensor_cis_get_cis(subdev);

	rev = cis->cis_data->cis_rev;
	info("[%s] hp2 sensor revision(%#x)\n", __func__, rev);

	IXC_MUTEX_LOCK(cis->ixc_lock);

	ret |= cis->ixc_ops->read8(cis->client, REG_HP2_SENSOR_FEATURE_ID_1, &feature_id);
	info("[%s] hp2 sensor revision 0x000D(%#x)\n", __func__, feature_id);
	ret |= cis->ixc_ops->read8(cis->client, REG_HP2_SENSOR_FEATURE_ID_2, &feature_id);
	info("[%s] hp2 sensor revision 0x000E(%#x)\n", __func__, feature_id);

	IXC_MUTEX_UNLOCK(cis->ixc_lock);

	return ret;
}

void sensor_hp2_cis_init_private(struct v4l2_subdev *subdev)
{
	struct is_cis *cis = sensor_cis_get_cis(subdev);
	struct sensor_hp2_private_runtime *priv_runtime;

	priv_runtime = (struct sensor_hp2_private_runtime *)cis->priv_runtime;

	priv_runtime->seamless_update_fe_cnt = 0;
	priv_runtime->load_retention = false;
}

int sensor_hp2_cis_init(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_cis *cis = sensor_cis_get_cis(subdev);
	struct sensor_hp2_private_runtime *priv_runtime;

	priv_runtime = (struct sensor_hp2_private_runtime *)cis->priv_runtime;

	sensor_hp2_cis_select_setfile(subdev);
	sensor_hp2_cis_init_private(subdev);

	cis->cis_data->cur_width = cis->sensor_info->max_width;
	cis->cis_data->cur_height = cis->sensor_info->max_height;
	cis->cis_data->low_expo_start = 33000;
	cis->cis_data->sens_config_index_cur = 0;

	CALL_CISOPS(cis, cis_data_calculation, subdev, cis->cis_data->sens_config_index_cur);

	return ret;
}

int sensor_hp2_cis_deinit(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_cis *cis = sensor_cis_get_cis(subdev);
	struct sensor_hp2_private_runtime *priv_runtime;

	priv_runtime = (struct sensor_hp2_private_runtime *)cis->priv_runtime;

	if (sensor_cis_support_retention_mode(subdev) && priv_runtime->load_retention == false &&
	    sensor_cis_get_retention_mode(subdev) == SENSOR_RETENTION_ACTIVATED) {
		info("[%s] need to load retention\n", __func__);
		CALL_CISOPS(cis, cis_stream_on, subdev);
		CALL_CISOPS(cis, cis_wait_streamon, subdev);
		CALL_CISOPS(cis, cis_stream_off, subdev);
		CALL_CISOPS(cis, cis_wait_streamoff, subdev);
		info("[%s] complete to load retention\n", __func__);
	}

	/* retention mode CRC wait calculation */
	usleep_range(10000, 10000);

	return ret;
}

static const struct is_cis_log log_hp2[] = {
	{I2C_READ, 8, 0x0005, 0, "frame_count"},
	/* 4000 page */
	{I2C_WRITE, 16, 0x6000, 0x0005, "page unlock"},
	{I2C_WRITE, 16, 0xFCFC, 0x4000, "0x4000 page"},
	{I2C_READ, 16, 0x0000, 0, "model_id"},
	{I2C_READ, 16, 0x0002, 0, "revision_number"},
	/* restore 4000 page */
	{I2C_WRITE, 16, 0xFCFC, 0x4000, "0x4000 page"},
	{I2C_WRITE, 16, 0x6000, 0x0085, "page lock"},
};

int sensor_hp2_cis_log_status(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_cis *cis = sensor_cis_get_cis(subdev);

	if (sensor_cis_support_retention_mode(subdev) && cis->cis_data->stream_on == false)
		sensor_cis_set_retention_mode(subdev, SENSOR_RETENTION_INACTIVE);

	sensor_cis_log_status(cis, log_hp2, ARRAY_SIZE(log_hp2), (char *)__func__);

	return ret;
}

#if IS_ENABLED(CONFIG_CAMERA_VENDOR_MCD)
int sensor_hp2_cis_write_calibration(struct v4l2_subdev *subdev,
	u16 start_addr, u16 end_addr, u16 *endian)
{
	int ret = 0;
	int i = 0;
	int cnt = 0;
	u16 val = 0;
	char *cal_buf = NULL;
	struct is_cis *cis = sensor_cis_get_cis(subdev);

	info("[%s] eeprom read, start set cal [0x%04X] ~ [0x%04X]\n",
		__func__, start_addr, end_addr);

	is_sec_get_cal_buf(&cal_buf, ROM_ID_REAR);

	if (*endian == HP2_CHECK_ENDIAN) {
		if (cal_buf[start_addr] == 0xFF && cal_buf[start_addr + 1] == 0x00)
			*endian = HP2_BIG_ENDIAN;
		else
			*endian = HP2_LITTLE_ENDIAN;

		start_addr = start_addr + 2;
	}

	for (i = start_addr; i <= end_addr; i += 2) {
		val = HP2_ENDIAN(cal_buf[i], cal_buf[i + 1], *endian);
		ret = cis->ixc_ops->write16(cis->client, 0x6F12, val);
		cnt = cnt + 2;
		if (ret < 0) {
			err("cis->ixc_ops->write16 fail!!");
			return ret;
		}

#ifdef DEBUG_CAL_WRITE
		info("[%s] cal offset[0x%04X], val[0x%04X]\n", __func__, i, val);
#endif
	}

	info("[%s] eeprom xtc written %d bytes\n", __func__, cnt);

	return ret;
}

int sensor_hp2_cis_set_cal(struct v4l2_subdev *subdev)
{
	int ret = 0;
	int i = 0;
	int index = 0;
	u16 cur_endian = HP2_BIG_ENDIAN;
	u32 size;
	const u16 *regs;
	struct is_rom_info *finfo = NULL;
	struct sensor_hp2_xtc_cal_info cal_info[HP2_XTC_MAX];
	struct is_cis *cis = sensor_cis_get_cis(subdev);
	struct sensor_hp2_private_data *priv;
	struct sensor_hp2_private_runtime *priv_runtime;

	priv = (struct sensor_hp2_private_data *)cis->sensor_info->priv;
	priv_runtime = (struct sensor_hp2_private_runtime *)cis->priv_runtime;

	info("[%s] E\n", __func__);

	priv_runtime->eeprom_cal_available = false;

	is_sec_get_sysfs_finfo(&finfo, ROM_ID_REAR);

	if (finfo->crc_error || !finfo->read_done) {
		err("eeprom read fail status, skip set cal");
		return -EINVAL;
	}

	for (i = 0; i < finfo->rom_xtc_cal_data_addr_list_len; i += 4) {
		if (finfo->rom_xtc_cal_data_addr_list_len < i + 4) {
			err("out of bounds - xtc cal data list");
			return -EINVAL;
		}
		index = finfo->rom_xtc_cal_data_addr_list[i];
		cal_info[index].start = finfo->rom_xtc_cal_data_addr_list[i + 1];
		cal_info[index].end = finfo->rom_xtc_cal_data_addr_list[i + 2];
		cal_info[index].endian = finfo->rom_xtc_cal_data_addr_list[i + 3];
	}

	regs = priv->xtc_cal.regs;
	size = priv->xtc_cal.size;

	IXC_MUTEX_LOCK(cis->ixc_lock);
	for (i = 0; i < size; i += HP2_IXC_NEXT) {
		switch (regs[i + HP2_IXC_ARGV0]) {
		case HP2_IXC_MODE_EEPROM:
			index = regs[i + HP2_IXC_ARGV1];

			if (cal_info[index].endian != HP2_CONTINUE_ENDIAN)
				cur_endian = cal_info[index].endian;

			sensor_hp2_cis_write_calibration(subdev,
				cal_info[index].start, cal_info[index].end, &cur_endian);

			break;

		default:
			ret = cis->ixc_ops->write16(cis->client,
				regs[i + HP2_IXC_ARGV0], regs[i + HP2_IXC_ARGV1]);
			if (ret < 0) {
				err("is_sensor_write16 fail, ret(%d), addr(%#x), data(%#x)",
					ret, regs[i + HP2_IXC_ARGV0], regs[i + HP2_IXC_ARGV1]);
				break;
			}
		}
	}

	priv_runtime->eeprom_cal_available = true;

	info("[%s] X\n", __func__);
	IXC_MUTEX_UNLOCK(cis->ixc_lock);

	return ret;
}
#else
#define sensor_hp2_cis_set_cal(subdev)	0
#endif

u16 sensor_hp2_cis_read_sensor_fcm_index(struct v4l2_subdev *subdev)
{
	u16 cur_sensor_fcm_index = HP2_FCI_NONE;
	struct is_cis *cis = sensor_cis_get_cis(subdev);

	IXC_MUTEX_LOCK(cis->ixc_lock);
	cis->ixc_ops->write16(cis->client, 0xFCFC, 0x4000);
	cis->ixc_ops->write16(cis->client, 0x6000, 0x0005);
	cis->ixc_ops->read16(cis->client, REG_HP2_FCM_INDEX, &cur_sensor_fcm_index);
	IXC_MUTEX_UNLOCK(cis->ixc_lock);

	return cur_sensor_fcm_index;
}

bool sensor_hp2_cis_is_possible_seamless_change(struct v4l2_subdev *subdev)
{
	struct is_cis *cis = sensor_cis_get_cis(subdev);
	struct sensor_hp2_private_runtime *priv_runtime;
	cis_shared_data *cis_data = cis->cis_data;

	priv_runtime = (struct sensor_hp2_private_runtime *)cis->priv_runtime;

	if (cis_data->cur_pattern_mode != SENSOR_TEST_PATTERN_MODE_OFF)
		return false;

	if (sensor_hp2_mode_groups[SENSOR_HP2_MODE_DEFAULT] == MODE_GROUP_NONE)
		return false;

	return true;
}

int sensor_hp2_cis_write_aeb_on(struct v4l2_subdev *subdev)
{
	int ret = 0;
	u32 mode = 0;
	const struct sensor_cis_mode_info *mode_info;
	struct is_cis *cis = sensor_cis_get_cis(subdev);
	struct sensor_hp2_private_data *priv;

	priv = (struct sensor_hp2_private_data *)cis->sensor_info->priv;

	mode = cis->cis_data->sens_config_index_cur;
	mode_info = cis->sensor_info->mode_infos[mode];

	IXC_MUTEX_LOCK(cis->ixc_lock);
	ret |= sensor_cis_write_registers(subdev, priv->aeb_on);

	if (mode_info->state_12bit == SENSOR_12BIT_STATE_PSEUDO_12BIT)
		ret |= sensor_cis_write_registers(subdev, priv->aeb_12bit_image_dt);
	else
		ret |= sensor_cis_write_registers(subdev, priv->aeb_10bit_image_dt);
	IXC_MUTEX_UNLOCK(cis->ixc_lock);

	return ret;
}

int sensor_hp2_cis_write_aeb_off(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_cis *cis = sensor_cis_get_cis(subdev);
	struct sensor_hp2_private_data *priv;

	priv = (struct sensor_hp2_private_data *)cis->sensor_info->priv;

	ret |= sensor_cis_write_registers_locked(subdev, priv->aeb_off);

	return ret;
}

int sensor_hp2_cis_force_aeb_off(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_cis *cis = sensor_cis_get_cis(subdev);

	if (cis->cis_data->cur_hdr_mode == SENSOR_HDR_MODE_2AEB_2VC
		&& cis->cis_data->pre_hdr_mode == SENSOR_HDR_MODE_2AEB_2VC) {
		info("[%s] current AEB status is enabled. need AEB disable\n", __func__);
		cis->cis_data->pre_hdr_mode = cis->cis_data->cur_hdr_mode = SENSOR_HDR_MODE_SINGLE;
		ret |= sensor_hp2_cis_write_aeb_off(subdev);
		info("[%s] disable AEB\n", __func__);
	}

	return ret;
}

bool sensor_hp2_cis_update_aeb(struct v4l2_subdev *subdev)
{
	bool is_aeb_on = false;
	struct is_device_sensor *device;
	struct is_cis *cis = sensor_cis_get_cis(subdev);
	struct sensor_hp2_private_runtime *priv_runtime;
	cis_shared_data *cis_data = cis->cis_data;

	priv_runtime = (struct sensor_hp2_private_runtime *)cis->priv_runtime;

	device = (struct is_device_sensor *)v4l2_get_subdev_hostdata(subdev);
	WARN_ON(!device);

	if (cis_data->cur_hdr_mode == SENSOR_HDR_MODE_2AEB_2VC) {
		is_aeb_on = true;
		if (cis_data->stream_on == false || is_sensor_g_ex_mode(device) != EX_AEB) {
			info("[%s] current AEB status is enabled. need AEB disable\n", __func__);
			cis_data->pre_hdr_mode = cis_data->cur_hdr_mode = SENSOR_HDR_MODE_SINGLE;
			sensor_hp2_cis_write_aeb_off(subdev);
			info("[%s] disable AEB\n", __func__);
			return false;
		}
	}

	if (cis_data->cur_hdr_mode == cis_data->pre_hdr_mode)
		return is_aeb_on;

	if (is_aeb_on) {
		priv_runtime->seamless_update_fe_cnt = cis_data->sen_vblank_count;
		sensor_hp2_cis_write_aeb_on(subdev);
		info("[%s] enable AEB\n", __func__);
	} else {
		priv_runtime->seamless_update_fe_cnt = 0;
		sensor_hp2_cis_write_aeb_off(subdev);
		info("[%s] disable AEB\n", __func__);
	}

	cis_data->pre_hdr_mode = cis_data->cur_hdr_mode;

	return is_aeb_on;
}

int sensor_hp2_cis_set_seamless_mode(struct v4l2_subdev *subdev, u32 next_mode)
{
	int ret = 0;
	int cur_mode = 0;
	u16 sensor_fcm_index = HP2_FCI_NONE;
	const struct sensor_cis_mode_info *next_mode_info;
	struct is_cis *cis = sensor_cis_get_cis(subdev);
	struct sensor_hp2_private_data *priv;
	struct sensor_hp2_private_runtime *priv_runtime;
	cis_shared_data *cis_data = cis->cis_data;

	priv = (struct sensor_hp2_private_data *)cis->sensor_info->priv;
	priv_runtime = (struct sensor_hp2_private_runtime *)cis->priv_runtime;

	cur_mode = cis_data->sens_config_index_cur;
	if (cur_mode == next_mode || next_mode == MODE_GROUP_NONE)
		return -EINVAL;

	next_mode_info = cis->sensor_info->mode_infos[next_mode];
	sensor_fcm_index = sensor_hp2_cis_read_sensor_fcm_index(subdev);
	if (sensor_fcm_index == next_mode_info->setfile_fcm_index)
		return -EINVAL;

	IXC_MUTEX_LOCK(cis->ixc_lock);

	cis->ixc_ops->write16(cis->client, cis->reg_addr->group_param_hold, DATA_HP2_GPH_HOLD);
	ret |= sensor_cis_write_registers(subdev, priv->seamless_update_n_plus_2);
	ret |= sensor_cis_write_registers(subdev, next_mode_info->setfile_fcm);
	cis->ixc_ops->write16(cis->client, cis->reg_addr->group_param_hold, DATA_HP2_GPH_RELEASE);

	IXC_MUTEX_UNLOCK(cis->ixc_lock);

	if (ret < 0)
		err("sensor_hp2_fcm[%d] fail!!", next_mode);

	priv_runtime->seamless_update_fe_cnt = cis_data->sen_vblank_count;
	cis_data->sens_config_index_cur = next_mode;

	CALL_CISOPS(cis, cis_data_calculation, subdev, next_mode);

	info("[%s] pre(%d)->cur(%d), 12bit[%d] LN[%d] RMS[%d] AEB[%d] => fast_change_idx[0x%x]\n",
		__func__, cur_mode, next_mode,
		cis_data->cur_12bit_mode, cis_data->cur_lownoise_mode,
		cis_data->cur_remosaic_zoom_ratio, cis_data->cur_hdr_mode,
		next_mode_info->setfile_fcm_index);

	cis->cis_data->seamless_mode_changed = true;

	return ret;
}

bool sensor_hp2_cis_update_12bit(struct v4l2_subdev *subdev)
{
	bool ret = true;
	int next_mode = 0;
	struct is_cis *cis = sensor_cis_get_cis(subdev);

	switch (cis->cis_data->cur_12bit_mode) {
	case SENSOR_12BIT_STATE_REAL_12BIT:
		next_mode = sensor_hp2_mode_groups[SENSOR_HP2_MODE_IDCG];
		sensor_hp2_cis_set_seamless_mode(subdev, next_mode);
		break;
	default:
		if (cis->cis_data->pre_12bit_mode == SENSOR_12BIT_STATE_REAL_12BIT)
			sensor_hp2_cis_force_aeb_off(subdev);
		return false;
	}

	return ret;
}

bool sensor_hp2_cis_update_lownoise(struct v4l2_subdev *subdev)
{
	bool ret = true;
	int next_mode = 0;
	struct is_cis *cis = sensor_cis_get_cis(subdev);

	switch (cis->cis_data->cur_lownoise_mode) {
	case IS_CIS_LN2:
		next_mode = sensor_hp2_mode_groups[SENSOR_HP2_MODE_LN2];
		sensor_hp2_cis_set_seamless_mode(subdev, next_mode);
		break;
	case IS_CIS_LN4:
		next_mode = sensor_hp2_mode_groups[SENSOR_HP2_MODE_LN4];
		sensor_hp2_cis_set_seamless_mode(subdev, next_mode);
		break;
	default:
		return false;
	}

	return ret;
}

bool sensor_hp2_cis_update_cropped_remosaic(struct v4l2_subdev *subdev)
{
	bool ret = true;
	int next_mode = 0;
	u32 cur_zoom_ratio = 0;
	struct is_cis *cis = sensor_cis_get_cis(subdev);

	cur_zoom_ratio = cis->cis_data->cur_remosaic_zoom_ratio;

	if (cur_zoom_ratio == 0)
		return false;

	if (sensor_hp2_rms_zoom_mode[cur_zoom_ratio] == SENSOR_HP2_RMS_ZOOM_MODE_NONE)
		return false;

	next_mode = sensor_hp2_rms_zoom_mode[cur_zoom_ratio];
	sensor_hp2_cis_set_seamless_mode(subdev, next_mode);

	return ret;
}

bool sensor_hp2_cis_update_default_mode(struct v4l2_subdev *subdev)
{
	bool ret = true;
	int cur_mode = 0;
	int next_mode = 0;
	struct is_cis *cis = sensor_cis_get_cis(subdev);

	cur_mode = cis->cis_data->sens_config_index_cur;
	next_mode = sensor_hp2_mode_groups[SENSOR_HP2_MODE_DEFAULT];

	if (cur_mode == next_mode)
		return ret;

	sensor_hp2_cis_set_seamless_mode(subdev, next_mode);

	return ret;
}

int sensor_hp2_cis_update_seamless_mode(struct v4l2_subdev *subdev)
{
	int ret = 0;

	if (!sensor_hp2_cis_is_possible_seamless_change(subdev))
		return -EINVAL;

	sensor_hp2_cis_update_aeb(subdev);

	if (sensor_hp2_cis_update_cropped_remosaic(subdev))
		return ret;

	if (sensor_hp2_cis_update_12bit(subdev))
		return ret;

	if (sensor_hp2_cis_update_lownoise(subdev))
		return ret;

	if (sensor_hp2_cis_update_default_mode(subdev))
		return ret;

	return ret;
}

int sensor_hp2_cis_get_seamless_mode_info(struct v4l2_subdev *subdev)
{
	struct is_cis *cis = sensor_cis_get_cis(subdev);
	cis_shared_data *cis_data = cis->cis_data;
	int ret = 0, cnt = 0;
	u32 mode;

	if (sensor_hp2_mode_groups[SENSOR_HP2_MODE_LN2] != MODE_GROUP_NONE) {
		cis_data->seamless_mode_info[cnt].mode = SENSOR_MODE_LN2;
		sensor_cis_get_mode_info(subdev, sensor_hp2_mode_groups[SENSOR_HP2_MODE_LN2],
			&cis_data->seamless_mode_info[cnt]);
		cnt++;
	}

	if (sensor_hp2_mode_groups[SENSOR_HP2_MODE_LN4] != MODE_GROUP_NONE) {
		cis_data->seamless_mode_info[cnt].mode = SENSOR_MODE_LN4;
		sensor_cis_get_mode_info(subdev, sensor_hp2_mode_groups[SENSOR_HP2_MODE_LN4],
			&cis_data->seamless_mode_info[cnt]);
		cnt++;
	}

	if (sensor_hp2_mode_groups[SENSOR_HP2_MODE_IDCG] != MODE_GROUP_NONE) {
		cis_data->seamless_mode_info[cnt].mode = SENSOR_MODE_REAL_12BIT;
		sensor_cis_get_mode_info(subdev, sensor_hp2_mode_groups[SENSOR_HP2_MODE_IDCG],
			&cis_data->seamless_mode_info[cnt]);
		cnt++;

		mode = sensor_hp2_mode_groups[SENSOR_HP2_MODE_IDCG];
		if (cis->sensor_info->mode_infos[mode]->aeb_support) {
			cis_data->seamless_mode_info[cnt].mode = SENSOR_MODE_REAL_12BIT | SENSOR_MODE_AEB;
			sensor_cis_get_mode_info(subdev, mode, &cis_data->seamless_mode_info[cnt]);
			cnt++;
		}
	}

	if (sensor_hp2_mode_groups[SENSOR_HP2_MODE_RMS_CROP] != MODE_GROUP_NONE) {
		cis_data->seamless_mode_info[cnt].mode = SENSOR_MODE_CROPPED_RMS;
		cis_data->seamless_mode_info[cnt].extra[0] = SENSOR_HP2_RMS_ZOOM_MODE_X_20;
		sensor_cis_get_mode_info(subdev, sensor_hp2_rms_zoom_mode[SENSOR_HP2_RMS_ZOOM_MODE_X_20],
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

int sensor_hp2_cis_mode_change(struct v4l2_subdev *subdev, u32 mode)
{
	int ret = 0;
	u32 ex_mode;
	struct is_device_sensor_peri *sensor_peri = NULL;
	struct is_actuator *actuator = NULL;
	struct is_device_sensor *device;
	const struct sensor_cis_mode_info *mode_info;
	struct is_cis *cis = sensor_cis_get_cis(subdev);
	struct sensor_hp2_private_data *priv;
	struct sensor_hp2_private_runtime *priv_runtime;

	priv_runtime = (struct sensor_hp2_private_runtime *)cis->priv_runtime;
	priv = (struct sensor_hp2_private_data *)cis->sensor_info->priv;

	device = (struct is_device_sensor *)v4l2_get_subdev_hostdata(subdev);
	WARN_ON(!device);

	sensor_peri = container_of(cis, struct is_device_sensor_peri, cis);
	actuator = sensor_peri->actuator;

	if (mode >= cis->sensor_info->mode_count) {
		err("invalid mode(%d)!!", mode);
		return -EINVAL;
	}

	ex_mode = is_sensor_g_ex_mode(device);

	info("[%s] sensor mode(%d) ex_mode(%d)\n", __func__, mode, ex_mode);

	sensor_hp2_cis_set_mode_group(mode);
	sensor_hp2_cis_get_seamless_mode_info(subdev);
	sensor_hp2_cis_init_private(subdev);

	mode_info = cis->sensor_info->mode_infos[mode];

	if (sensor_cis_get_retention_mode(subdev) == SENSOR_RETENTION_ACTIVATED) {
		ret |= sensor_cis_write_registers_locked(subdev, mode_info->setfile_fcm);
	} else {
		info("[%s] not support retention sensor mode(%d)\n", __func__, mode);
		ret |= sensor_cis_write_registers_locked(subdev, priv->fcm_off);
		ret |= sensor_cis_write_registers_locked(subdev, mode_info->setfile);
	}

	ret |= sensor_hp2_cis_write_aeb_off(subdev);

	/* HW Remosaic on/off */
	if (ex_mode == EX_AI_REMOSAIC)
		// Remosaic off - Hexadeca Pattern
		ret |= sensor_cis_write_registers_locked(subdev, priv->hw_rms_off);
	else
		 // Remosaic on - Bayer Pattern
		ret |= sensor_cis_write_registers_locked(subdev, priv->hw_rms_on);

	IXC_MUTEX_LOCK(cis->ixc_lock);
	/* IMB Setting for 200M capture */
	if (actuator && mode == SENSOR_HP2_16320x12240_11FPS) {
		info("[%s] IMB set actuator position[%d]\n", __func__, actuator->position);
		ret |= cis->ixc_ops->write16(cis->client,
			REG_HP2_IMB_SETTING, actuator->position);
	} else {
		ret |= cis->ixc_ops->write16(cis->client,
			REG_HP2_IMB_SETTING, DATA_HP2_IMB_SETTING_OFF);
	}

	/* FCM N+1 Update */
	ret |= sensor_cis_write_registers(subdev, priv->seamless_update_n_plus_1);

	IXC_MUTEX_UNLOCK(cis->ixc_lock);

	if (ret < 0)
		err("i2c set registers fail!!");

	info("[%s] mode[%d] 12bit[%d] LN[%d] ZOOM[%d] AEB[%d] => fast_change_idx[0x%x]\n",
		__func__, mode,
		cis->cis_data->cur_12bit_mode,
		cis->cis_data->cur_lownoise_mode,
		cis->cis_data->cur_remosaic_zoom_ratio,
		cis->cis_data->cur_hdr_mode,
		mode_info->setfile_fcm_index);

	/* sensor_hp2_cis_log_status(subdev); */
	info("[%s] X\n", __func__);

	return ret;
}

int sensor_hp2_cis_set_global_setting_internal(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_cis *cis = sensor_cis_get_cis(subdev);
	struct sensor_hp2_private_data *priv;
	struct sensor_hp2_private_runtime *priv_runtime;

	priv = (struct sensor_hp2_private_data *)cis->sensor_info->priv;
	priv_runtime = (struct sensor_hp2_private_runtime *)cis->priv_runtime;

	info("[%s] global setting internal start\n", __func__);

	info("[%s] set init_tnp setting\n", __func__);
	ret |= sensor_cis_write_registers_locked(subdev, priv->init_tnp);

	if (IS_ENABLED(CONFIG_CAMERA_VENDOR_MCD)) {
		info("[%s] set xtc cal setting\n", __func__);
		ret |= sensor_hp2_cis_set_cal(subdev);
	}

	info("[%s] set global setting\n", __func__);
	ret |= sensor_cis_write_registers_locked(subdev, priv->global);

	/* if eeprom cal write is skipped, turn off decomp */
	if (!priv_runtime->eeprom_cal_available)
		ret |= sensor_cis_write_registers_locked(subdev, priv->xtc_cal_off);

	info("[%s] set sram for fcm setting\n", __func__);
	ret |= sensor_cis_write_registers_locked(subdev, priv->sram_fcm);

	info("[%s] set init fcm setting\n", __func__);
	ret |= sensor_cis_write_registers_locked(subdev, priv->init_fcm);

	if (ret < 0) {
		err("sensor_hp2_set_registers fail!!");
		return ret;
	}

	info("[%s] global setting internal done\n", __func__);

	return ret;
}

int sensor_hp2_cis_retention_prepare(struct v4l2_subdev *subdev)
{
	int ret = 0;
	int retry_count = 0;
	int max_retry_count = 10;
	const struct sensor_cis_mode_info *mode_info;
	struct is_cis *cis = sensor_cis_get_cis(subdev);
	struct sensor_hp2_private_data *priv;
	struct sensor_hp2_private_runtime *priv_runtime;

	priv = (struct sensor_hp2_private_data *)cis->sensor_info->priv;
	priv_runtime = (struct sensor_hp2_private_runtime *)cis->priv_runtime;

	//FAST AE Setting
	mode_info = cis->sensor_info->mode_infos[SENSOR_HP2_2040x1532_120FPS];
	ret = sensor_cis_write_registers_locked(subdev, mode_info->setfile_fcm);

	if (ret < 0) {
		err("sensor_hp2_set_registers fail!!");
		return ret;
	}

	if (priv_runtime->need_stream_on_retention) {
		ret |= sensor_cis_write_registers_locked(subdev, priv->stream_on);

		for (retry_count = 0; retry_count < max_retry_count; retry_count++) {
			ret = CALL_CISOPS(cis, cis_wait_streamon, subdev);
			if (ret < 0)
				err("wait failed retry %d", retry_count);
			else
				break;
		}

		priv_runtime->need_stream_on_retention = false;
	}

	if (ret < 0) {
		err("retention prepare stream on fail!!");
		return ret;
	}

	ret |= sensor_cis_write_registers_locked(subdev, priv->stream_off);

	ret |= CALL_CISOPS(cis, cis_wait_streamoff, subdev);
	if (ret < 0) {
		err("retention prepare stream off fail!!");
		return ret;
	}

	ret |= sensor_hp2_cis_check_register_poll(subdev, priv->verify_retention_ready);
	if (ret < 0) {
		err("check retention ready fail!!");
		return ret;
	}

	sensor_cis_set_retention_mode(subdev, SENSOR_RETENTION_ACTIVATED);

	info("[%s] retention sensor RAM write done\n", __func__);

	return ret;
}

int sensor_hp2_cis_retention_crc_check(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_cis *cis = sensor_cis_get_cis(subdev);
	struct sensor_hp2_private_data *priv;

	priv = (struct sensor_hp2_private_data *)cis->sensor_info->priv;

	ret |= sensor_cis_write_registers_locked(subdev, priv->retention_exit);

	if (sensor_hp2_cis_check_register_poll(subdev, priv->verify_retention_checksum) == 0) {
		info("[%s] retention SRAM CRC check: pass!\n", __func__);
	} else {
		err("retention SRAM CRC check: fail!");

		ret = sensor_hp2_cis_set_global_setting_internal(subdev);
		if (ret < 0) {
			err("CRC error recover: rewrite sensor global setting failed");
			return ret;
		}

		ret = sensor_hp2_cis_retention_prepare(subdev);
		if (ret < 0) {
			err("CRC error recover: retention prepare failed");
			return ret;
		}
	}

	ret |= sensor_cis_write_registers_locked(subdev, priv->retention_hw_init);

	return ret;
}

int sensor_hp2_cis_set_global_setting(struct v4l2_subdev *subdev)
{
	int ret = 0;
	u32 retention_mode;

	retention_mode = sensor_cis_get_retention_mode(subdev);

	if (sensor_cis_support_retention_mode(subdev)) {
		if (retention_mode == SENSOR_RETENTION_INACTIVE) {
			ret |= sensor_hp2_cis_set_global_setting_internal(subdev);
			ret |= sensor_hp2_cis_retention_prepare(subdev);
		} else if (retention_mode == SENSOR_RETENTION_ACTIVATED) {
			ret |= sensor_hp2_cis_retention_crc_check(subdev);
		}
	} else {
		ret |= sensor_hp2_cis_set_global_setting_internal(subdev);
	}

	if (ret < 0) {
		err("sensor_hp2_set_registers fail!!");
		return ret;
	}

	info("[%s] global setting done\n", __func__);

	return ret;
}

int sensor_hp2_cis_wait_streamoff(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_cis *cis = sensor_cis_get_cis(subdev);
	struct sensor_hp2_private_data *priv;

	priv = (struct sensor_hp2_private_data *)cis->sensor_info->priv;

	info("[%s] : E\n", __func__);

	ret = sensor_cis_wait_streamoff(subdev);
	if (ret < 0) {
		err("wait stream off: fail!");
		return ret;
	}

	if (sensor_cis_support_retention_mode(subdev)) {
		ret = sensor_hp2_cis_check_register_poll(subdev, priv->verify_retention_ready);
		if (ret < 0) {
			err("retention ready check: fail!");
			return ret;
		}

		info("[%s] retention ready check: pass!\n", __func__);
	}

	return ret;
}

int sensor_hp2_cis_stream_on(struct v4l2_subdev *subdev)
{
	int ret = 0;
	int i;
	u16 fast_change_idx = HP2_FCI_NONE;
	struct is_device_sensor_peri *sensor_peri = NULL;
	struct is_device_sensor *device;
	struct is_cis *cis = sensor_cis_get_cis(subdev);
	struct sensor_hp2_private_data *priv;
	struct sensor_hp2_private_runtime *priv_runtime;
	ktime_t st = ktime_get();

	device = (struct is_device_sensor *)v4l2_get_subdev_hostdata(subdev);
	WARN_ON(!device);

	sensor_peri = container_of(cis, struct is_device_sensor_peri, cis);
	WARN_ON(!sensor_peri);

	priv = (struct sensor_hp2_private_data *)cis->sensor_info->priv;
	priv_runtime = (struct sensor_hp2_private_runtime *)cis->priv_runtime;

	dbg_sensor(1, "[MOD:D:%d] %s\n", cis->id, __func__);

	/* disable dcg cal */
	if (sensor_hp2_dcg_cal_disable) {
		sensor_cis_write_registers_locked(subdev, priv->dcg_cal_off);
		info("[%s] disable dcg cal\n", __func__);
	}

	if (cis->vendor_use_adaptive_mipi) {
		is_vendor_set_mipi_clock(device);
	} else {
		for (i = 0; i < priv->default_mipi_list_count; i++) {
			if (priv->default_mipi_list[i].mipi_rate == device->cfg->mipi_speed) {
				info("[%s] default mipi setting is found (mipirate = %d)\n", __func__, device->cfg->mipi_speed);
				sensor_cis_write_registers_locked(subdev, priv->default_mipi_list[i].mipi_setting);
				break;
			}
		}
	}

	fast_change_idx = sensor_hp2_cis_read_sensor_fcm_index(subdev);
	info("[%s] eeprom_cal_available(%d), fast_change_idx(0x%x)\n",
		__func__, priv_runtime->eeprom_cal_available, fast_change_idx);

	/* Sensor stream on */
	ret |= sensor_cis_write_registers_locked(subdev, priv->stream_on);

	cis->cis_data->stream_on = true;
	priv_runtime->load_retention = true;

	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %lldus", __func__, PABLO_KTIME_US_DELTA_NOW(st));

	return ret;
}

int sensor_hp2_cis_wait_seamless_update_delay(struct v4l2_subdev *subdev)
{
	struct is_cis *cis = sensor_cis_get_cis(subdev);
	struct sensor_hp2_private_runtime *priv_runtime;
	unsigned int seamless_fe_done_cnt;
	u32 wait_interval = 10;
	u32 wait_cnt = 0;
	u32 max_cnt = 1000;

	priv_runtime = (struct sensor_hp2_private_runtime *)cis->priv_runtime;

	if (priv_runtime->seamless_update_fe_cnt == 0)
		return 0;

	seamless_fe_done_cnt = priv_runtime->seamless_update_fe_cnt + 2;

	if (seamless_fe_done_cnt > cis->cis_data->sen_vblank_count) {
		do {
			info("[%s] seamless_fe_done_cnt(%d) cur_fe_count(%d)\n",
				__func__, seamless_fe_done_cnt, cis->cis_data->sen_vblank_count);
			if (wait_cnt >= max_cnt) {
				err("fail wait_seamless_update_delay");
				return -EINVAL;
			}

			msleep(wait_interval);
			wait_cnt++;
		} while (seamless_fe_done_cnt > cis->cis_data->sen_vblank_count);
	}

	return 0;
}

int sensor_hp2_cis_stream_off(struct v4l2_subdev *subdev)
{
	int ret = 0;
	u8 cur_frame_count = 0;
	u16 fast_change_idx = HP2_FCI_NONE;
	struct is_cis *cis = sensor_cis_get_cis(subdev);
	struct sensor_hp2_private_data *priv;
	struct sensor_hp2_private_runtime *priv_runtime;
	ktime_t st = ktime_get();

	priv = (struct sensor_hp2_private_data *)cis->sensor_info->priv;
	priv_runtime = (struct sensor_hp2_private_runtime *)cis->priv_runtime;

	dbg_sensor(1, "[MOD:D:%d] %s\n", cis->id, __func__);

	ret = CALL_CISOPS(cis, cis_group_param_hold, subdev, false);
	if (ret < 0)
		err("group_param_hold_func failed at stream off");

	sensor_hp2_cis_force_aeb_off(subdev);

	IXC_MUTEX_LOCK(cis->ixc_lock);
	cis->ixc_ops->read8(cis->client, 0x0005, &cur_frame_count);
	IXC_MUTEX_UNLOCK(cis->ixc_lock);

	fast_change_idx = sensor_hp2_cis_read_sensor_fcm_index(subdev);
	info("[%s] frame_count(0x%x), fast_change_idx(0x%x)\n", __func__, cur_frame_count, fast_change_idx);

	if (sensor_cis_support_retention_mode(subdev)) {
		ret |= sensor_cis_write_registers_locked(subdev, priv->retention_init_checksum);

		if (sensor_cis_get_retention_mode(subdev) == SENSOR_RETENTION_INACTIVE)
			sensor_cis_set_retention_mode(subdev, SENSOR_RETENTION_ACTIVATED);
	}

	/* Sensor stream off */
	ret |= sensor_cis_write_registers_locked(subdev, priv->stream_off);

	cis->cis_data->stream_on = false;
	priv_runtime->seamless_update_fe_cnt = 0;

	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %lldus", __func__, PABLO_KTIME_US_DELTA_NOW(st));

	return ret;
}

int sensor_hp2_cis_set_wb_gain(struct v4l2_subdev *subdev, struct wb_gains wb_gains)
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
	ret |= cis->ixc_ops->write16(cis->client, REG_HP2_ABS_GAIN_R, abs_gains[0]);
	ret |= cis->ixc_ops->write16(cis->client, REG_HP2_ABS_GAIN_G, abs_gains[1]);
	ret |= cis->ixc_ops->write16(cis->client, REG_HP2_ABS_GAIN_B, abs_gains[2]);
	IXC_MUTEX_UNLOCK(cis->ixc_lock);

	if (ret < 0) {
		err("sensor_hp2_set_registers fail!!");
		return ret;
	}

	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %lldus", __func__, PABLO_KTIME_US_DELTA_NOW(st));

	return ret;
}

int sensor_hp2_cis_get_updated_binning_ratio(struct v4l2_subdev *subdev, u32 *binning_ratio)
{
	struct is_cis *cis = sensor_cis_get_cis(subdev);
	cis_shared_data *cis_data = cis->cis_data;
	u32 rms_mode, zoom_ratio;

	if (sensor_hp2_mode_groups[SENSOR_HP2_MODE_RMS_CROP] == MODE_GROUP_NONE)
		return 0;

	zoom_ratio = cis_data->pre_remosaic_zoom_ratio;
	rms_mode = sensor_hp2_rms_zoom_mode[zoom_ratio];

	if (rms_mode && sensor_hp2_rms_binning_ratio[rms_mode])
		*binning_ratio = sensor_hp2_rms_binning_ratio[rms_mode];

	return 0;
}

int sensor_hp2_sensor_hp2_cis_verify_bpc_crc(char *bpc_data)
{
	int ret = 0;
	u32 calc_bpc_crc, bpc_crc;
	u32 bpc_addr, crc_addr;
	u32 bpc_32bit;

	crc_addr = REG_HP2_BPC_OTP_CRC_S_ADDR;
	bpc_crc = bpc_data[crc_addr] << 24 | bpc_data[crc_addr + 1] << 16
			| bpc_data[crc_addr + 2] << 8 | bpc_data[crc_addr + 3];

	calc_bpc_crc = bpc_data[0] << 24 | bpc_data[1] << 16
				| bpc_data[2] << 8 | bpc_data[3];

	for (bpc_addr = 4; bpc_addr < REG_HP2_BPC_OTP_E_ADDR; bpc_addr += 4) {
		bpc_32bit = bpc_data[bpc_addr] << 24 | bpc_data[bpc_addr + 1] << 16
				| bpc_data[bpc_addr + 2] << 8 | bpc_data[bpc_addr + 3];

		calc_bpc_crc = ~(calc_bpc_crc ^ bpc_32bit);

		if (bpc_32bit == DATA_HP2_BPC_OTP_TERMINATE_CODE)
			break;
	}

	if (calc_bpc_crc != bpc_crc) {
		err("verify bpc crc fail. calc_bpc_crc(0x%x) bpc_crc(0x%x)", calc_bpc_crc, bpc_crc);
		return -EINVAL;
	}

	return ret;
}

int sensor_hp2_sensor_hp2_cis_read_bpc_otp(struct v4l2_subdev *subdev, char *bpc_data, u32 *bpc_size)
{
	int ret = 0, read_size;
	struct is_cis *cis = sensor_cis_get_cis(subdev);
	struct sensor_hp2_private_data *priv;

	if (cis->cis_data->cis_rev < REG_HP2_BPC_MIN_REV) {
		err("bpc is not applied sensor rev. %#x", cis->cis_data->cis_rev);
		return -EINVAL;
	}

	priv = (struct sensor_hp2_private_data *)cis->sensor_info->priv;
	read_size = REG_HP2_BPC_OTP_E_ADDR + 1;

	info("[%s] prepare to read bpc otp", __func__);
	ret |= sensor_cis_write_registers_locked(subdev, priv->prepare_bpc_otp_read_1st);

	if (sensor_hp2_cis_check_register_poll(subdev, priv->verify_bpc_otp_read_dma) != 0)
		return -EINVAL;

	ret |= sensor_cis_write_registers_locked(subdev, priv->prepare_bpc_otp_read_2nd);

	IXC_MUTEX_LOCK(cis->ixc_lock);
	info("[%s] read bpc otp (0x%x) ~ (0x%x)", __func__, REG_HP2_BPC_OTP_S_ADDR, read_size);
	ret |= cis->ixc_ops->write16(cis->client, 0xFCFC, REG_HP2_BPC_OTP_PAGE);
	ret |= cis->ixc_ops->read8_size(cis->client, bpc_data, REG_HP2_BPC_OTP_S_ADDR, read_size);
	IXC_MUTEX_UNLOCK(cis->ixc_lock);

	ret |= sensor_hp2_sensor_hp2_cis_verify_bpc_crc(bpc_data);

	*bpc_size = read_size;

	if (ret < 0) {
		err("bpc read fail!!");
		*bpc_size = 0;
	}

	return ret;
}

int sensor_hp2_cis_check_register_poll(struct v4l2_subdev *subdev,
	const struct sensor_hp2_check_reg *check_reg)
{
	int ret = 0;
	u32 wait_cnt = 0;
	u16 val = 0;
	struct is_cis *cis = sensor_cis_get_cis(subdev);

	IXC_MUTEX_LOCK(cis->ixc_lock);
	do {
		if (wait_cnt >= check_reg->max_cnt) {
			err("check register fail. addr(0x%x) data(0x%x)", check_reg->addr, val);
			ret = -EINVAL;
			break;
		}

		usleep_range(check_reg->interval, check_reg->interval + 10);
		cis->ixc_ops->write16(cis->client, 0x602C, check_reg->page);
		cis->ixc_ops->write16(cis->client, 0x602E, check_reg->addr);
		cis->ixc_ops->read16(cis->client, 0x6F12, &val);

		wait_cnt++;
	} while (val != check_reg->check_data);
	IXC_MUTEX_UNLOCK(cis->ixc_lock);

	return ret;
}

static struct is_cis_ops cis_ops_hp2 = {
	.cis_init = sensor_hp2_cis_init,
	.cis_deinit = sensor_hp2_cis_deinit,
	.cis_log_status = sensor_hp2_cis_log_status,
	.cis_group_param_hold = sensor_cis_set_group_param_hold,
	.cis_set_global_setting = sensor_hp2_cis_set_global_setting,
	.cis_init_state = sensor_cis_init_state,
	.cis_mode_change = sensor_hp2_cis_mode_change,
	.cis_stream_on = sensor_hp2_cis_stream_on,
	.cis_stream_off = sensor_hp2_cis_stream_off,
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
	.cis_wait_streamoff = sensor_hp2_cis_wait_streamoff,
	.cis_wait_streamon = sensor_cis_wait_streamon,
	.cis_set_wb_gains = sensor_hp2_cis_set_wb_gain,
	.cis_check_rev_on_init = sensor_cis_check_rev_on_init,
	.cis_set_initial_exposure = sensor_cis_set_initial_exposure,
	// .cis_recover_stream_on = sensor_cis_recover_stream_on,
	// .cis_recover_stream_off = sensor_cis_recover_stream_off,
	.cis_set_test_pattern = sensor_cis_set_test_pattern,
	.cis_update_seamless_mode = sensor_hp2_cis_update_seamless_mode,
	.cis_update_seamless_state = sensor_cis_update_seamless_state,
	.cis_wait_seamless_update_delay = sensor_hp2_cis_wait_seamless_update_delay,
	.cis_get_updated_binning_ratio = sensor_hp2_cis_get_updated_binning_ratio,
	.cis_read_bpc_otp = sensor_hp2_sensor_hp2_cis_read_bpc_otp,
	.cis_set_flip_mode = sensor_cis_set_flip_mode,
};

static int cis_hp2_probe_i2c(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	int ret;
	u32 mclk_freq_khz;
	struct is_cis *cis;
	struct is_device_sensor_peri *sensor_peri;
	char const *setfile;
	struct device_node *dnode = client->dev.of_node;
	struct sensor_hp2_private_runtime *priv_runtime;

	ret = sensor_cis_probe(client, &(client->dev), &sensor_peri, I2C_TYPE);
	if (ret) {
		probe_info("[%s] sensor_cis_probe ret(%d)\n", __func__, ret);
		return ret;
	}

	cis = &sensor_peri->cis;
	cis->ctrl_delay = N_PLUS_TWO_FRAME;
	cis->cis_ops = &cis_ops_hp2;

	/* belows are depend on sensor cis. MUST check sensor spec */
	cis->bayer_order = OTF_INPUT_ORDER_BAYER_GB_RG;
	cis->use_wb_gain = true;
	cis->use_bpc_otp = true;
	cis->reg_addr = &sensor_hp2_reg_addr;
	cis->priv_runtime = kzalloc(sizeof(struct sensor_hp2_private_runtime), GFP_KERNEL);
	if (!cis->priv_runtime) {
		kfree(cis->cis_data);
		kfree(cis->subdev);
		probe_err("cis->priv_runtime is NULL");
		return -ENOMEM;
	}

	priv_runtime = (struct sensor_hp2_private_runtime *)cis->priv_runtime;
	priv_runtime->need_stream_on_retention = true;

	ret = of_property_read_string(dnode, "setfile", &setfile);
	if (ret) {
		err("setfile index read fail(%d), take default setfile!!", ret);
		setfile = "default";
	}

	mclk_freq_khz = sensor_peri->module->pdata->mclk_freq_khz;

	if (mclk_freq_khz == 19200) {
		if (strcmp(setfile, "default") == 0 || strcmp(setfile, "setA") == 0)
			probe_info("[%s] setfile_A mclk: 19.2MHz\n", __func__);
		else
			err("setfile index out of bound, take default (setfile_A mclk: 19.2MHz)");

		cis->sensor_info = &sensor_hp2_info_A_19p2;
	}

	is_vendor_set_mipi_mode(cis);

	probe_info("[%s] done\n", __func__);

	return ret;
}

static const struct of_device_id sensor_cis_hp2_match[] = {
	{
		.compatible = "samsung,exynos-is-cis-hp2",
	},
	{},
};
MODULE_DEVICE_TABLE(of, sensor_cis_hp2_match);

static const struct i2c_device_id sensor_cis_hp2_idt[] = {
	{ SENSOR_NAME, 0 },
	{},
};

static struct i2c_driver sensor_cis_hp2_driver = {
	.probe	= cis_hp2_probe_i2c,
	.driver = {
		.name	= SENSOR_NAME,
		.owner	= THIS_MODULE,
		.of_match_table = sensor_cis_hp2_match,
		.suppress_bind_attrs = true,
	},
	.id_table = sensor_cis_hp2_idt
};

#ifdef MODULE
builtin_i2c_driver(sensor_cis_hp2_driver);
#else
static int __init sensor_cis_hp2_init(void)
{
	int ret;

	ret = i2c_add_driver(&sensor_cis_hp2_driver);
	if (ret)
		err("failed to add %s driver: %d",
			sensor_cis_hp2_driver.driver.name, ret);

	return ret;
}
late_initcall_sync(sensor_cis_hp2_init);
#endif

MODULE_LICENSE("GPL");
MODULE_SOFTDEP("pre: fimc-is");
