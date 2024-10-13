// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * Exynos Pablo image subsystem functions
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/platform_device.h>
#include <linux/spi/spi.h>
#include <linux/vmalloc.h>
#include <linux/firmware.h>
#include <linux/i2c.h>
#include <linux/gpio.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/io.h>
#include <linux/clk.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#include <linux/delay.h>
#include <linux/file.h>
#include <soc/samsung/exynos-pmu-if.h>

#include <exynos-is-sensor.h>
#include "is-device-sensor-peri.h"
#include "pablo-hw-api-common.h"
#include "is-hw-api-ois-mcu.h"
#include "is-vendor-ois.h"
#include "is-device-ois.h"
#include "is-sfr-ois-mcu-v1_1_1.h"
#ifdef CONFIG_AF_HOST_CONTROL
#include "is-device-af.h"
#endif
#include "is-vendor-private.h"
#include "is-sec-define.h"
#if defined(CONFIG_CAMERA_USE_INTERNAL_MCU)
#include "is-vendor-ois-internal-mcu.h"
#elif defined(CONFIG_CAMERA_USE_EXTERNAL_MCU)
#include "is-vendor-ois-external-mcu.h"
#elif defined(CONFIG_CAMERA_USE_AOIS)
#include "is-vendor-aois.h"
#endif

static const struct v4l2_subdev_ops subdev_ops;

int is_ois_read_u8(int cmd, u8 *data) {
#if defined(CONFIG_CAMERA_USE_INTERNAL_MCU)
	struct is_core *core = is_get_is_core();
	struct ois_mcu_dev *mcu = core->mcu;
	void __iomem *reg = mcu->regs[OM_REG_CORE];
#elif defined(CONFIG_CAMERA_USE_EXTERNAL_MCU)
	int ret = 0;
	struct i2c_client *client = is_mcu_i2c_get_client();
#elif defined(CONFIG_CAMERA_USE_AOIS)
	int ret = 0;
	u8 rxbuf[1];
#endif

#if defined(CONFIG_CAMERA_USE_INTERNAL_MCU)
	*data = is_hw_get_reg_u8(reg, &ois_mcu_regs[cmd]);
#elif defined(CONFIG_CAMERA_USE_EXTERNAL_MCU)
	ret = is_ois_i2c_read(client, ois_mcu_regs[cmd].sfr_offset, data);
#elif defined(CONFIG_CAMERA_USE_AOIS)
	ret = cam_ois_reg_read_notifier_call_chain(0, ois_mcu_regs[cmd].sfr_offset, &rxbuf[0], 1);
	*data = rxbuf[0];
#endif

	dbg_ois("[GET_REG] reg:[%s][0x%04X], reg_value(R):[0x%02X]\n",
		ois_mcu_regs[cmd].reg_name, ois_mcu_regs[cmd].sfr_offset, *data);

#if defined(CONFIG_CAMERA_USE_EXTERNAL_MCU) || defined(CONFIG_CAMERA_USE_AOIS)
	if (unlikely(ret != 2)) {
		err_mcu("get fail (%s:%X)", ois_mcu_regs[cmd].reg_name, ois_mcu_regs[cmd].sfr_offset);
		return -EIO;
	}
#endif
	return 0;
}

int is_ois_read_multi(int cmd, u8 *data, size_t size)
{
	int i;

#if defined(CONFIG_CAMERA_USE_EXTERNAL_MCU)
	struct i2c_client *client = is_mcu_i2c_get_client();
	int ret = 0;
#elif defined(CONFIG_CAMERA_USE_AOIS)
	u8 rxbuf[256];
	int ret = 0;
#endif

#if defined(CONFIG_CAMERA_USE_EXTERNAL_MCU)
	ret = is_ois_i2c_read_multi(client, ois_mcu_regs[cmd].sfr_offset, data, size);
#elif defined(CONFIG_CAMERA_USE_AOIS)
	ret = cam_ois_reg_read_notifier_call_chain(0, ois_mcu_regs[cmd].sfr_offset, rxbuf, size);
	memcpy(data, rxbuf, size);
#endif

	for (i = 0; i < size; i++) {
		dbg_ois("[GET_REG] reg:[%s][0x%04X], reg_value(R):[0x%02X]\n",
			ois_mcu_regs[cmd].reg_name, ois_mcu_regs[cmd].sfr_offset + i, data[i]);
	}

#if defined(CONFIG_CAMERA_USE_EXTERNAL_MCU) || defined(CONFIG_CAMERA_USE_AOIS)
	if (unlikely(ret != 2)) {
		err_mcu("get multi fail (%s:%X)", ois_mcu_regs[cmd].reg_name, ois_mcu_regs[cmd].sfr_offset);
		return -EIO;
	}
#endif
	return 0;
}

int is_ois_read_u16(int cmd, u8 *data)
{
	return is_ois_read_multi(cmd, data, 2);
}

int is_ois_write_u8(int cmd, u8 data)
{
#if defined(CONFIG_CAMERA_USE_INTERNAL_MCU)
	struct is_core *core = is_get_is_core();
	struct ois_mcu_dev *mcu = core->mcu;
	void __iomem *reg = mcu->regs[OM_REG_CORE];
#elif defined(CONFIG_CAMERA_USE_EXTERNAL_MCU)
	struct i2c_client *client = is_mcu_i2c_get_client();
	int ret = 0;
#elif defined(CONFIG_CAMERA_USE_AOIS)
	int ret = 0;
#endif

#if defined(CONFIG_CAMERA_USE_INTERNAL_MCU)
	is_hw_set_reg_u8(reg, &ois_mcu_regs[cmd], data);
#elif defined(CONFIG_CAMERA_USE_EXTERNAL_MCU)
	ret = is_ois_i2c_write(client, ois_mcu_regs[cmd].sfr_offset, data);
#elif defined(CONFIG_CAMERA_USE_AOIS)
	ret = cam_ois_cmd_notifier_call_chain(0, ois_mcu_regs[cmd].sfr_offset, &data, 1);
#endif

	dbg_ois("[SET_REG] reg:[%s][0x%04X], reg_value(W):[0x%02X]\n",
		ois_mcu_regs[cmd].reg_name, ois_mcu_regs[cmd].sfr_offset, data);

#if defined(CONFIG_CAMERA_USE_EXTERNAL_MCU) || defined(CONFIG_CAMERA_USE_AOIS)
	if (unlikely(ret != 1)) {
		err_mcu("set fail (%s:%X)", ois_mcu_regs[cmd].reg_name, ois_mcu_regs[cmd].sfr_offset);
		return -EIO;
	}
#endif
	return 0;
}

int is_ois_write_multi(int cmd, u8 *data, size_t size)
{
#if defined(CONFIG_CAMERA_USE_EXTERNAL_MCU) || defined(CONFIG_CAMERA_USE_AOIS)
	int ret = 0;
#endif
	int i;

#if defined(CONFIG_CAMERA_USE_EXTERNAL_MCU)
	ret = is_ois_i2c_write_multi(client, ois_mcu_regs[cmd].sfr_offset, data, size);
#elif defined(CONFIG_CAMERA_USE_AOIS)
	ret = cam_ois_cmd_notifier_call_chain(0, ois_mcu_regs[cmd].sfr_offset, data, size);
#endif

	for (i = 0 ; i < size; i++) {
		dbg_ois("[SET_REG] reg:[%s][0x%04X], reg_value(W):[0x%02X]\n",
			ois_mcu_regs[cmd].reg_name, ois_mcu_regs[cmd].sfr_offset + i, data[i]);
	}

#if defined(CONFIG_CAMERA_USE_EXTERNAL_MCU) || defined(CONFIG_CAMERA_USE_AOIS)
	if (unlikely(ret != 1)) {
		err_mcu("set multi fail (%s:%X)", ois_mcu_regs[cmd].reg_name, ois_mcu_regs[cmd].sfr_offset);
		return -EIO;
	}
#endif
	return 0;
}

int is_ois_write_u16(int cmd, u8 *data)
{
	return is_ois_write_multi(cmd, data, 2);
}

void is_vendor_ois_parsing_raw_data(uint8_t *buf, long efs_size, long *raw_data_x, long *raw_data_y, long *raw_data_z)
{
	int ret;
	int i = 0, j = 0;
	char efs_data_pre[MAX_GYRO_EFS_DATA_LENGTH + 1];
	char efs_data_post[MAX_GYRO_EFS_DATA_LENGTH + 1];
	bool detect_point = false;
	int sign = 1;
	long raw_pre = 0, raw_post = 0;

	memset(efs_data_pre, 0x0, sizeof(efs_data_pre));
	memset(efs_data_post, 0x0, sizeof(efs_data_post));
	i = 0;
	j = 0;
	while ((*(buf + i)) != ',') {
		if (((char)*(buf + i)) == '-' ) {
			sign = -1;
			i++;
		}

		if (((char)*(buf + i)) == '.') {
			detect_point = true;
			i++;
			j = 0;
		}

		if (detect_point) {
			memcpy(efs_data_post + j, buf + i, 1);
			j++;
		} else {
			memcpy(efs_data_pre + j, buf + i, 1);
			j++;
		}

		if (++i > MAX_GYRO_EFS_DATA_LENGTH) {
			err_mcu("wrong EFS data.");
			break;
		}
	}
	i++;
	ret = kstrtol(efs_data_pre, 10, &raw_pre);
	ret = kstrtol(efs_data_post, 10, &raw_post);
	*raw_data_x = sign * (raw_pre * 1000 + raw_post);

	detect_point = false;
	j = 0;
	raw_pre = 0;
	raw_post = 0;
	sign = 1;
	memset(efs_data_pre, 0x0, sizeof(efs_data_pre));
	memset(efs_data_post, 0x0, sizeof(efs_data_post));
	while ((*(buf + i)) != ',') {
		if (((char)*(buf + i)) == '-' ) {
			sign = -1;
			i++;
		}

		if (((char)*(buf + i)) == '.') {
			detect_point = true;
			i++;
			j = 0;
		}

		if (detect_point) {
			memcpy(efs_data_post + j, buf + i, 1);
			j++;
		} else {
			memcpy(efs_data_pre + j, buf + i, 1);
			j++;
		}

		if (++i > MAX_GYRO_EFS_DATA_LENGTH) {
			err_mcu("wrong EFS data.");
			break;
		}
	}
	ret = kstrtol(efs_data_pre, 10, &raw_pre);
	ret = kstrtol(efs_data_post, 10, &raw_post);
	*raw_data_y = sign * (raw_pre * 1000 + raw_post);

	detect_point = false;
	j = 0;
	raw_pre = 0;
	raw_post = 0;
	sign = 1;
	memset(efs_data_pre, 0x0, sizeof(efs_data_pre));
	memset(efs_data_post, 0x0, sizeof(efs_data_post));
	while (i < efs_size) {
		if (((char)*(buf + i)) == '-' ) {
			sign = -1;
			i++;
		}

		if (((char)*(buf + i)) == '.') {
			detect_point = true;
			i++;
			j = 0;
		}

		if (detect_point) {
			memcpy(efs_data_post + j, buf + i, 1);
			j++;
		} else {
			memcpy(efs_data_pre + j, buf + i, 1);
			j++;
		}

		if (i++ > MAX_GYRO_EFS_DATA_LENGTH) {
			err_mcu("wrong EFS data.");
			break;
		}
	}
	ret = kstrtol(efs_data_pre, 10, &raw_pre);
	ret = kstrtol(efs_data_post, 10, &raw_post);
	*raw_data_z = sign * (raw_pre * 1000 + raw_post);

	info_mcu("%s : X raw_x = %ld, raw_y = %ld, raw_z = %ld\n", __func__, *raw_data_x, *raw_data_y, *raw_data_z);
}

long is_vendor_ois_get_efs_data(struct ois_mcu_dev *mcu, long *raw_data_x, long *raw_data_y, long *raw_data_z)
{
	long efs_size = 0;
	struct is_core *core = NULL;
	struct is_vendor_private *vendor_priv;

	core = is_get_is_core();
	vendor_priv = core->vendor.private_data;

	info_mcu("%s : E\n", __func__);

	efs_size = vendor_priv->gyro_efs_size;

	if (efs_size == 0) {
		err_mcu("efs read failed.");
		goto p_err;
	}

	is_vendor_ois_parsing_raw_data(vendor_priv->gyro_efs_data, efs_size, raw_data_x, raw_data_y, raw_data_z);

p_err:
	return efs_size;
}

int is_vendor_ois_set_ggfadeupdown(struct v4l2_subdev *subdev, int up, int down)
{
	int ret = 0;
	struct is_ois *ois = NULL;
	struct is_mcu *is_mcu = NULL;
	struct ois_mcu_dev *mcu = NULL;
	u8 status = 0;
	int retries = 100;
	u8 data[2];

	WARN_ON(!subdev);

	mcu = (struct ois_mcu_dev *)v4l2_get_subdevdata(subdev);
	if(!mcu) {
		err("%s, mcu subdev is NULL", __func__);
		ret = -EINVAL;
		return ret;
	}

	is_mcu = (struct is_mcu *)v4l2_get_subdev_hostdata(subdev);
	if(!is_mcu) {
		err("%s, is_mcu is NULL", __func__);
		ret = -EINVAL;
		return ret;
	}

	ois = is_mcu->ois;

	dbg_ois("%s up:%d down:%d\n", __func__, up, down);

	/* Wide af position value */
	is_ois_write_u8(OIS_CMD_REAR_AF, MCU_AF_INIT_POSITION);

#if defined(CAMERA_2ND_OIS)
	/* Tele af position value */
	is_ois_write_u8(OIS_CMD_REAR2_AF, MCU_AF_INIT_POSITION);
#endif
#if defined(CAMERA_3RD_OIS)
	/* Tele2 af position value */
	is_ois_write_u8(OIS_CMD_REAR3_AF, MCU_AF_INIT_POSITION);
#endif

	is_ois_write_u8(OIS_CMD_CACTRL_WRITE, 0x01);

	/* set fadeup */
	data[0] = up & 0xFF;
	data[1] = (up >> 8) & 0xFF;
	is_ois_write_u8(OIS_CMD_FADE_UP1, data[0]);
	is_ois_write_u8(OIS_CMD_FADE_UP2, data[1]);

	/* set fadedown */
	data[0] = down & 0xFF;
	data[1] = (down >> 8) & 0xFF;
	is_ois_write_u8(OIS_CMD_FADE_DOWN1, data[0]);
	is_ois_write_u8(OIS_CMD_FADE_DOWN2, data[1]);

	/* wait idle status
	 * 100msec delay is needed between "ois_power_on" and "ois_mode_s6".
	 */
	do {
		is_ois_read_u8(OIS_CMD_STATUS, &status);
		if (status == 0x01 || status == 0x13)
			break;
		if (--retries < 0) {
			err("%s : read register fail!. status: 0x%x\n", __func__, status);
			ret = -1;
			break;
		}
		usleep_range(1000, 1100);
	} while (status != 0x01);

	dbg_ois("%s retryCount = %d , status = 0x%x\n", __func__, 100 - retries, status);

	return ret;
}

int is_vendor_ois_set_mode(struct v4l2_subdev *subdev, int mode)
{
	int ret = 0;
	struct is_ois *ois = NULL;
	struct is_mcu *is_mcu = NULL;
	struct ois_mcu_dev *mcu = NULL;

	WARN_ON(!subdev);

	is_mcu = (struct is_mcu *)v4l2_get_subdev_hostdata(subdev);
	if(!is_mcu) {
		err("%s, is_mcu is NULL", __func__);
		ret = -EINVAL;
		return ret;
	}

	mcu = (struct ois_mcu_dev *)v4l2_get_subdevdata(subdev);
	if(!mcu) {
		err("%s, mcu subdev is NULL", __func__);
		ret = -EINVAL;
		return ret;
	}

#ifndef CONFIG_SEC_FACTORY
	if (!mcu->ois_wide_init
#if defined(CAMERA_2ND_OIS)
		&& !mcu->ois_tele_init
#endif
#if defined(CAMERA_3RD_OIS)
		&& !mcu->ois_tele2_init
#endif
	)
		return 0;
#endif

	ois = is_mcu->ois;

	if (ois->fadeupdown == false) {
		if (mcu->ois_fadeupdown == false) {
			mcu->ois_fadeupdown = true;
			is_vendor_ois_set_ggfadeupdown(subdev, 1000, 1000);
		}
		ois->fadeupdown = true;
	}

	if (mode == ois->pre_ois_mode) {
		return ret;
	}

	ois->pre_ois_mode = mode;
	info_mcu("%s: ois_mode value(%d)\n", __func__, mode);

	switch(mode) {
		case OPTICAL_STABILIZATION_MODE_STILL:
			is_ois_write_u8(OIS_CMD_MODE, 0x00);
			is_ois_write_u8(OIS_CMD_START, 0x01);
			break;
		case OPTICAL_STABILIZATION_MODE_VIDEO:
			is_ois_write_u8(OIS_CMD_MODE, 0x01);
			is_ois_write_u8(OIS_CMD_START, 0x01);
			break;
		case OPTICAL_STABILIZATION_MODE_CENTERING:
			is_ois_write_u8(OIS_CMD_MODE, 0x05);
			is_ois_write_u8(OIS_CMD_START, 0x01);
			break;
		case OPTICAL_STABILIZATION_MODE_HOLD:
			is_ois_write_u8(OIS_CMD_MODE, 0x06);
			is_ois_write_u8(OIS_CMD_START, 0x01);
			break;
		case OPTICAL_STABILIZATION_MODE_STILL_ZOOM:
			is_ois_write_u8(OIS_CMD_MODE, 0x13);
			is_ois_write_u8(OIS_CMD_START, 0x01);
			break;
		case OPTICAL_STABILIZATION_MODE_VDIS:
			is_ois_write_u8(OIS_CMD_MODE, 0x14);
			is_ois_write_u8(OIS_CMD_START, 0x01);
			break;
		case OPTICAL_STABILIZATION_MODE_VDIS_ASR:
			is_ois_write_u8(OIS_CMD_MODE, 0x15);
			is_ois_write_u8(OIS_CMD_START, 0x01);
			break;
		case OPTICAL_STABILIZATION_MODE_SINE_X:
			is_ois_write_u8(OIS_CMD_SINE_1, 0x01);
			is_ois_write_u8(OIS_CMD_SINE_2, 0x01);
			is_ois_write_u8(OIS_CMD_SINE_3, 0x2D);
			is_ois_write_u8(OIS_CMD_MODE, 0x03);
			msleep(20);
			is_ois_write_u8(OIS_CMD_START, 0x01);
			break;
		case OPTICAL_STABILIZATION_MODE_SINE_Y:
			is_ois_write_u8(OIS_CMD_SINE_1, 0x02);
			is_ois_write_u8(OIS_CMD_SINE_2, 0x01);
			is_ois_write_u8(OIS_CMD_SINE_3, 0x2D);
			is_ois_write_u8(OIS_CMD_MODE, 0x03);
			msleep(20);
			is_ois_write_u8(OIS_CMD_START, 0x01);
			break;
		default:
			dbg_ois("%s: ois_mode value(%d)\n", __func__, mode);
			break;
	}

#ifdef USE_OIS_STABILIZATION_DELAY
	if (!mcu->is_mcu_active) {
		usleep_range(USE_OIS_STABILIZATION_DELAY, USE_OIS_STABILIZATION_DELAY + 10);
		mcu->is_mcu_active = true;
		info_mcu("%s : Stabilization delay applied\n", __func__);
	}
#endif

	return ret;
}

int is_vendor_ois_shift_compensation(struct v4l2_subdev *subdev, int position, int resolution)
{
	int ret = 0;
	struct is_ois *ois;
	struct ois_mcu_dev *mcu = NULL;
	struct is_mcu *is_mcu = NULL;
	struct is_module_enum *module = NULL;
	struct is_device_sensor_peri *sensor_peri = NULL;
	int position_changed;

	WARN_ON(!subdev);

	mcu = (struct ois_mcu_dev *)v4l2_get_subdevdata(subdev);
	if(!mcu) {
		err("%s, mcu subdev is NULL", __func__);
		ret = -EINVAL;
		goto p_err;
	}

	is_mcu = (struct is_mcu *)v4l2_get_subdev_hostdata(subdev);
	if(!is_mcu) {
		err("%s, is_mcu is NULL", __func__);
		ret = -EINVAL;
		return ret;
	}

	sensor_peri = is_mcu->sensor_peri;
	if (!sensor_peri) {
		err("%s, sensor_peri is NULL", __func__);
		ret = -EINVAL;
		return ret;
	}

	module = sensor_peri->module;
	if (!module) {
		err("%s, module is NULL", __func__);
		ret = -EINVAL;
		return ret;
	}

	ois = is_mcu->ois;

	position_changed = position >> 4;

	if (position_changed < MIN_AF_POSITION)
		position_changed = MIN_AF_POSITION;

	if (module->position == SENSOR_POSITION_REAR && ois->af_pos_wide != position_changed) {
		/* Wide af position value */
		is_ois_write_u8(OIS_CMD_REAR_AF, (u8)position_changed);
		ois->af_pos_wide = position_changed;
	}
#if !defined(USE_TELE_OIS_AF_COMMON_INTERFACE) && defined(CAMERA_2ND_OIS)
	else if (module->position == SENSOR_POSITION_REAR2 && ois->af_pos_tele != position_changed) {
		/* Tele af position value */
		is_ois_write_u8(OIS_CMD_REAR2_AF, (u8)position_changed);
		ois->af_pos_tele = position_changed;
	}
#elif !defined(USE_TELE2_OIS_AF_COMMON_INTERFACE) && defined(CAMERA_3RD_OIS)
	else if (module->position == SENSOR_POSITION_REAR4 && ois->af_pos_tele2 != position_changed) {
		/* Tele af position value */
		is_ois_write_u8(OIS_CMD_REAR3_AF, (u8)position_changed);
		ois->af_pos_tele2 = position_changed;
	}
#endif

p_err:
	return ret;
}

int is_vendor_ois_self_test(struct is_core *core)
{
	u8 val = 0;
	u8 reg_val = 0, x = 0, y = 0, z = 0;
	u16 x_gyro_log = 0, y_gyro_log = 0, z_gyro_log = 0;
	int retries = 30;
	struct ois_mcu_dev *mcu = NULL;

	info_mcu("%s : E\n", __func__);

	mcu = core->mcu;

	is_ois_write_u8(OIS_CMD_GYRO_CAL, 0x08);

	do {
		is_ois_read_u8(OIS_CMD_GYRO_CAL, &val);
		msleep(50);
		if (--retries < 0) {
			err("Read register failed!!!!, data = 0x%04x\n", val);
			break;
		}
	} while (val);

	is_ois_read_u8(OIS_CMD_GYRO_RESULT, &val);

	/* Gyro selfTest result */
	is_ois_read_u8(OIS_CMD_GYRO_VAL_X, &reg_val);
	x = reg_val;
	is_ois_read_u8(OIS_CMD_GYRO_LOG_X, &reg_val);
	x_gyro_log = (reg_val << 8) | x;

	is_ois_read_u8(OIS_CMD_GYRO_VAL_Y, &reg_val);
	y = reg_val;
	is_ois_read_u8(OIS_CMD_GYRO_LOG_Y, &reg_val);
	y_gyro_log = (reg_val << 8) | y;

	is_ois_read_u8(OIS_CMD_GYRO_VAL_Z, &reg_val);
	z = reg_val;
	is_ois_read_u8(OIS_CMD_GYRO_LOG_Z, &reg_val);
	z_gyro_log = (reg_val << 8) | z;

	info_mcu("%s(GSTLOG0=%d, GSTLOG1=%d, GSTLOG2=%d)\n", __func__, x_gyro_log, y_gyro_log, z_gyro_log);

	info_mcu("%s(%d) : X\n", __func__, val);
	return (int)val;
}

#if defined(USE_TELE_OIS_AF_COMMON_INTERFACE) || defined(USE_TELE2_OIS_AF_COMMON_INTERFACE)
int is_vendor_ois_af_move_lens(struct is_core *core)
{
	struct ois_mcu_dev *mcu = NULL;

	mcu = core->mcu;

	info_mcu("%s : E\n", __func__);

	is_ois_write_u8(OIS_CMD_CTRL_AF, MCU_AF_MODE_ACTIVE);
#if defined(USE_TELE_OIS_AF_COMMON_INTERFACE)
	is_ois_write_u8(OIS_CMD_POS1_REAR2_AF, 0x80);
	is_ois_write_u8(OIS_CMD_POS2_REAR2_AF, 0x00);
#elif defined(USE_TELE2_OIS_AF_COMMON_INTERFACE)
	is_ois_write_u8(OIS_CMD_POS1_REAR3_AF, 0x80);
	is_ois_write_u8(OIS_CMD_POS2_REAR3_AF, 0x00);
#endif

	info_mcu("%s : X\n", __func__);

	return 0;
}
#endif

bool is_vendor_ois_sine_wavecheck_all(struct is_core *core,
					int threshold, int *sinx, int *siny, int *result,
					int *sinx_2nd, int *siny_2nd, int *sinx_3rd, int *siny_3rd)
{
	u8 buf[2] = {0, }, val = 0;
	int retries = 10;
	int sinx_count = 0, siny_count = 0;
#if defined(CAMERA_2ND_OIS)
	int sinx_count_2nd = 0, siny_count_2nd = 0;
#endif
#if defined(CAMERA_3RD_OIS)
	int sinx_count_3rd = 0, siny_count_3rd = 0;
#endif
	u8 u8_sinx_count[2] = {0, }, u8_siny_count[2] = {0, };
	u8 u8_sinx[2] = {0, }, u8_siny[2] = {0, };
	struct ois_mcu_dev *mcu = NULL;

	mcu = core->mcu;

	/* OIS SEL (wide: 1, tele: 2, w/t: 3, tele2: 4, all: 7) */
#if defined(CAMERA_3RD_OIS)
	is_ois_write_u8(OIS_CMD_OIS_SEL, 0x07);
#elif defined(CAMERA_2ND_OIS)
	is_ois_write_u8(OIS_CMD_OIS_SEL, 0x03);
#else
	is_ois_write_u8(OIS_CMD_OIS_SEL, 0x01);
#endif

	/* Error threshold level */
	is_ois_write_u8(OIS_CMD_THRESH_ERR_LEV, (u8)threshold);
#if defined(CAMERA_2ND_OIS)
	is_ois_write_u8(OIS_CMD_THRESH_ERR_LEV_M2, (u8)threshold);
#endif
#if defined(CAMERA_3RD_OIS)
	is_ois_write_u8(OIS_CMD_THRESH_ERR_LEV_M3, (u8)threshold);
#endif

	/* count value for error judgement level */
	is_ois_write_u8(OIS_CMD_ERR_VAL_CNT, 0x00);

	/* frequency level for measurement */
	is_ois_write_u8(OIS_CMD_FREQ_LEV, 0x05);

	/* amplitude level for measurement */
	is_ois_write_u8(OIS_CMD_AMPLI_LEV, 0x2A);

	/* dummy pulse setting */
	is_ois_write_u8(OIS_CMD_DUM_PULSE, 0x03);

	/* vyvle level for measurement */
	is_ois_write_u8(OIS_CMD_VYVLE_LEV, 0x02);

	/* start sine wave check operation */
	is_ois_write_u8(OIS_CMD_START_WAVE_CHECK, 0x01);

	retries = 22;
	do {
		is_ois_read_u8(OIS_CMD_START_WAVE_CHECK, &val);
		msleep(100);
		if (--retries < 0) {
			err("sine wave operation fail, val = 0x%02x.\n", val);
			break;
		}
	} while (val);

	is_ois_read_u8(OIS_CMD_MCERR_W, &buf[0]);
	is_ois_read_u8(OIS_CMD_MCERR_W2, &buf[1]);

	*result = (buf[1] << 8) | buf[0];

	is_ois_read_u8(OIS_CMD_REAR_SINX_COUNT1, &u8_sinx_count[0]);
	is_ois_read_u8(OIS_CMD_REAR_SINX_COUNT2, &u8_sinx_count[1]);
	sinx_count = (u8_sinx_count[1] << 8) | u8_sinx_count[0];
	if (sinx_count > 0x7FFF) {
		sinx_count = -((sinx_count ^ 0xFFFF) + 1);
	}
	is_ois_read_u8(OIS_CMD_REAR_SINY_COUNT1, &u8_siny_count[0]);
	is_ois_read_u8(OIS_CMD_REAR_SINY_COUNT2, &u8_siny_count[1]);
	siny_count = (u8_siny_count[1] << 8) | u8_siny_count[0];
	if (siny_count > 0x7FFF) {
		siny_count = -((siny_count ^ 0xFFFF) + 1);
	}
	is_ois_read_u8(OIS_CMD_REAR_SINX_DIFF1, &u8_sinx[0]);
	is_ois_read_u8(OIS_CMD_REAR_SINX_DIFF2, &u8_sinx[1]);
	*sinx = (u8_sinx[1] << 8) | u8_sinx[0];
	if (*sinx > 0x7FFF) {
		*sinx = -((*sinx ^ 0xFFFF) + 1);
	}
	is_ois_read_u8(OIS_CMD_REAR_SINY_DIFF1, &u8_siny[0]);
	is_ois_read_u8(OIS_CMD_REAR_SINY_DIFF2, &u8_siny[1]);
	*siny = (u8_siny[1] << 8) | u8_siny[0];
	if (*siny > 0x7FFF) {
		*siny = -((*siny ^ 0xFFFF) + 1);
	}

	info_mcu("%s threshold = %d, sinx = %d, siny = %d, sinx_count = %d, syny_count = %d, MCERR result = 0x%04x\n",
		__func__, threshold, *sinx, *siny, sinx_count, siny_count, *result);

#if defined(CAMERA_2ND_OIS)
	is_ois_read_u8(OIS_CMD_REAR2_SINX_COUNT1, &u8_sinx_count[0]);
	is_ois_read_u8(OIS_CMD_REAR2_SINX_COUNT2, &u8_sinx_count[1]);
	sinx_count_2nd = (u8_sinx_count[1] << 8) | u8_sinx_count[0];
	if (sinx_count_2nd > 0x7FFF) {
		sinx_count_2nd = -((sinx_count_2nd ^ 0xFFFF) + 1);
	}
	is_ois_read_u8(OIS_CMD_REAR2_SINY_COUNT1, &u8_siny_count[0]);
	is_ois_read_u8(OIS_CMD_REAR2_SINY_COUNT2, &u8_siny_count[1]);
	siny_count_2nd = (u8_siny_count[1] << 8) | u8_siny_count[0];
	if (siny_count_2nd > 0x7FFF) {
		siny_count_2nd = -((siny_count_2nd ^ 0xFFFF) + 1);
	}
	is_ois_read_u8(OIS_CMD_REAR2_SINX_DIFF1, &u8_sinx[0]);
	is_ois_read_u8(OIS_CMD_REAR2_SINX_DIFF2, &u8_sinx[1]);
	*sinx_2nd = (u8_sinx[1] << 8) | u8_sinx[0];
	if (*sinx_2nd > 0x7FFF) {
		*sinx_2nd = -((*sinx_2nd ^ 0xFFFF) + 1);
	}
	is_ois_read_u8(OIS_CMD_REAR2_SINY_DIFF1, &u8_siny[0]);
	is_ois_read_u8(OIS_CMD_REAR2_SINY_DIFF2, &u8_siny[1]);
	*siny_2nd = (u8_siny[1] << 8) | u8_siny[0];
	if (*siny_2nd > 0x7FFF) {
		*siny_2nd = -((*siny_2nd ^ 0xFFFF) + 1);
	}

	info_mcu("%s threshold = %d, sinx_2nd = %d, siny_2nd = %d, sinx_count_2nd = %d, syny_count_2nd = %d\n",
		__func__, threshold, *sinx_2nd, *siny_2nd, sinx_count_2nd, siny_count_2nd);
#endif

#if defined(CAMERA_3RD_OIS)
	is_ois_read_u8(OIS_CMD_REAR3_SINX_COUNT1, &u8_sinx_count);
	is_ois_read_u8(OIS_CMD_REAR3_SINX_COUNT2, &u8_sinx_count);
	sinx_count_3rd = (u8_sinx_count[1] << 8) | u8_sinx_count[0];
	if (sinx_count_3rd > 0x7FFF) {
		sinx_count_3rd = -((sinx_count_3rd ^ 0xFFFF) + 1);
	}
	is_ois_read_u8(OIS_CMD_REAR3_SINY_COUNT1, &u8_siny_count);
	is_ois_read_u8(OIS_CMD_REAR3_SINY_COUNT2, &u8_siny_count);
	siny_count_3rd = (u8_siny_count[1] << 8) | u8_siny_count[0];
	if (siny_count_3rd > 0x7FFF) {
		siny_count_3rd = -((siny_count_3rd ^ 0xFFFF) + 1);
	}
	is_ois_read_u8(OIS_CMD_REAR3_SINX_DIFF1, &u8_sinx[0]);
	is_ois_read_u8(OIS_CMD_REAR3_SINX_DIFF2, &u8_sinx[1]);
	*sinx_3rd = (u8_sinx[1] << 8) | u8_sinx[0];
	if (*sinx_3rd > 0x7FFF) {
		*sinx_3rd = -((*sinx_3rd ^ 0xFFFF) + 1);
	}
	is_ois_read_u8(OIS_CMD_REAR3_SINY_DIFF1, &u8_siny[0]);
	is_ois_read_u8(OIS_CMD_REAR3_SINY_DIFF2, &u8_siny[1]);
	*siny_3rd = (u8_siny[1] << 8) | u8_siny[0];
	if (*siny_3rd > 0x7FFF) {
		*siny_3rd = -((*siny_3rd ^ 0xFFFF) + 1);
	}

	info_mcu("%s threshold = %d, sinx_3rd = %d, siny_3rd = %d, sinx_count_3rd = %d, syny_count_3rd = %d\n",
		__func__, threshold, *sinx_3rd, *siny_3rd, sinx_count_3rd, siny_count_3rd);
#endif

	if (*result == 0x0) {
		return true;
	} else {
		return false;
	}
}

bool is_vendor_ois_auto_test_all(struct is_core *core,
					int threshold, bool *x_result, bool *y_result, int *sin_x, int *sin_y,
					bool *x_result_2nd, bool *y_result_2nd, int *sin_x_2nd, int *sin_y_2nd,
					bool *x_result_3rd, bool *y_result_3rd, int *sin_x_3rd, int *sin_y_3rd)
{
	int result = 0;
	bool value = false;

	info_mcu("%s : E\n", __func__);

#ifdef CONFIG_AF_HOST_CONTROL
#if defined(CAMERA_2ND_OIS)
#ifdef USE_TELE_OIS_AF_COMMON_INTERFACE
	is_vendor_ois_af_move_lens(core);
#else
	is_af_move_lens(core, SENSOR_POSITION_REAR2);
#endif
	msleep(100);
#endif
#if defined(CAMERA_3RD_OIS)
#ifdef USE_TELE2_OIS_AF_COMMON_INTERFACE
	is_vendor_ois_af_move_lens(core);
#else
	is_af_move_lens(core, SENSOR_POSITION_REAR4);
#endif
	msleep(100);
#endif
	is_af_move_lens(core, SENSOR_POSITION_REAR);
	msleep(100);
#endif /* CONFIG_AF_HOST_CONTROL */

	value = is_vendor_ois_sine_wavecheck_all(core, threshold, sin_x, sin_y, &result,
				sin_x_2nd, sin_y_2nd, sin_x_3rd, sin_y_3rd);

	if (*sin_x == -1 && *sin_y == -1) {
		err_mcu("OIS device is not prepared.");
		*x_result = false;
		*y_result = false;

		return false;
	}
#if defined(CAMERA_2ND_OIS)
	if (*sin_x_2nd == -1 && *sin_y_2nd == -1) {
		err_mcu("OIS 2 device is not prepared.");
		*x_result_2nd = false;
		*y_result_2nd = false;

		return false;
	}
#endif
#if defined(CAMERA_3RD_OIS)
	if (*sin_x_3rd == -1 && *sin_y_3rd == -1) {
		err_mcu("OIS 3 device is not prepared.");
		*x_result_3rd = false;
		*y_result_3rd = false;

		return false;
	}
#endif

	if (value == true) {
		*x_result = true;
		*y_result = true;
#if defined(CAMERA_2ND_OIS)
		*x_result_2nd = true;
		*y_result_2nd = true;
#endif
#if defined(CAMERA_3RD_OIS)
		*x_result_3rd = true;
		*y_result_3rd = true;
#endif
		return true;
	} else {
		err("OIS autotest is failed result = 0x%04x\n", result);
		if ((result & 0x03) == 0x00) {
			*x_result = true;
			*y_result = true;
		} else if ((result & 0x03) == 0x01) {
			*x_result = false;
			*y_result = true;
		} else if ((result & 0x03) == 0x02) {
			*x_result = true;
			*y_result = false;
		} else {
			*x_result = false;
			*y_result = false;
		}
#if defined(CAMERA_2ND_OIS)
		if ((result & 0x30) == 0x00) {
			*x_result_2nd = true;
			*y_result_2nd = true;
		} else if ((result & 0x30) == 0x10) {
			*x_result_2nd = false;
			*y_result_2nd = true;
		} else if ((result & 0x30) == 0x20) {
			*x_result_2nd = true;
			*y_result_2nd = false;
		} else {
			*x_result_2nd = false;
			*y_result_2nd = false;
		}
#endif
#if defined(CAMERA_3RD_OIS)
		if ((result & 0x300) == 0x00) {
			*x_result_3rd = true;
			*y_result_3rd = true;
		} else if ((result & 0x300) == 0x100) {
			*x_result_3rd = false;
			*y_result_3rd = true;
		} else if ((result & 0x300) == 0x200) {
			*x_result_3rd = true;
			*y_result_3rd = false;
		} else {
			*x_result_3rd = false;
			*y_result_3rd = false;
		}
#endif
		return false;
	}
}

#if defined(CAMERA_2ND_OIS)
bool is_vendor_ois_sine_wavecheck_rear2(struct is_core *core,
					int threshold, int *sinx, int *siny, int *result,
					int *sinx_2nd, int *siny_2nd)
{
	u8 buf = 0, val = 0;
	int retries = 10;
	int sinx_count = 0, siny_count = 0;
	int sinx_count_2nd = 0, siny_count_2nd = 0;
	u8 u8_sinx_count[2] = {0, }, u8_siny_count[2] = {0, };
	u8 u8_sinx[2] = {0, }, u8_siny[2] = {0, };
	struct ois_mcu_dev *mcu = NULL;

	mcu = core->mcu;

	/* OIS SEL (wide: 1, tele: 2, w/t: 3, tele2: 4, all: 7) */
	is_ois_write_u8(OIS_CMD_OIS_SEL, 0x03);
	/* error threshold level */
	is_ois_write_u8(OIS_CMD_THRESH_ERR_LEV, (u8)threshold);
	/* error threshold level */
	is_ois_write_u8(OIS_CMD_THRESH_ERR_LEV_M2, (u8)threshold);
	/* count value for error judgement level */
	is_ois_write_u8(OIS_CMD_ERR_VAL_CNT, 0x00);
	/* frequency level for measurement */
	is_ois_write_u8(OIS_CMD_FREQ_LEV, 0x05);
	/* amplitude level for measurement */
	is_ois_write_u8(OIS_CMD_AMPLI_LEV, 0x2A);
	/* dummy pulse setting */
	is_ois_write_u8(OIS_CMD_DUM_PULSE, 0x03);
	/* vyvle level for measurement */
	is_ois_write_u8(OIS_CMD_VYVLE_LEV, 0x02);
	/* start sine wave check operation */
	is_ois_write_u8(OIS_CMD_START_WAVE_CHECK, 0x01);

	retries = 22;
	do {
		is_ois_read_u8(OIS_CMD_START_WAVE_CHECK, &val);
		msleep(100);
		if (--retries < 0) {
			err("sine wave operation fail.\n");
			break;
		}
	} while (val);

	is_ois_read_u8(OIS_CMD_MCERR_W, &buf);

	*result = (int)buf;

	is_ois_read_u8(OIS_CMD_REAR_SINX_COUNT1, &u8_sinx_count[0]);
	is_ois_read_u8(OIS_CMD_REAR_SINX_COUNT2, &u8_sinx_count[1]);
	sinx_count = (u8_sinx_count[1] << 8) | u8_sinx_count[0];
	if (sinx_count > 0x7FFF) {
		sinx_count = -((sinx_count ^ 0xFFFF) + 1);
	}
	is_ois_read_u8(OIS_CMD_REAR_SINY_COUNT1, &u8_siny_count[0]);
	is_ois_read_u8(OIS_CMD_REAR_SINY_COUNT2, &u8_siny_count[1]);
	siny_count = (u8_siny_count[1] << 8) | u8_siny_count[0];
	if (siny_count > 0x7FFF) {
		siny_count = -((siny_count ^ 0xFFFF) + 1);
	}
	is_ois_read_u8(OIS_CMD_REAR_SINX_DIFF1, &u8_sinx[0]);
	is_ois_read_u8(OIS_CMD_REAR_SINX_DIFF2, &u8_sinx[1]);
	*sinx = (u8_sinx[1] << 8) | u8_sinx[0];
	if (*sinx > 0x7FFF) {
		*sinx = -((*sinx ^ 0xFFFF) + 1);
	}
	is_ois_read_u8(OIS_CMD_REAR_SINY_DIFF1, &u8_siny[0]);
	is_ois_read_u8(OIS_CMD_REAR_SINY_DIFF2, &u8_siny[1]);
	*siny = (u8_siny[1] << 8) | u8_siny[0];
	if (*siny > 0x7FFF) {
		*siny = -((*siny ^ 0xFFFF) + 1);
	}

	is_ois_read_u8(OIS_CMD_REAR2_SINX_COUNT1, &u8_sinx_count[0]);
	is_ois_read_u8(OIS_CMD_REAR2_SINX_COUNT2, &u8_sinx_count[1]);
	sinx_count_2nd = (u8_sinx_count[1] << 8) | u8_sinx_count[0];
	if (sinx_count_2nd > 0x7FFF) {
		sinx_count_2nd = -((sinx_count_2nd ^ 0xFFFF) + 1);
	}
	is_ois_read_u8(OIS_CMD_REAR2_SINY_COUNT1, &u8_siny_count[0]);
	is_ois_read_u8(OIS_CMD_REAR2_SINY_COUNT2, &u8_siny_count[1]);
	siny_count_2nd = (u8_siny_count[1] << 8) | u8_siny_count[0];
	if (siny_count_2nd > 0x7FFF) {
		siny_count_2nd = -((siny_count_2nd ^ 0xFFFF) + 1);
	}
	is_ois_read_u8(OIS_CMD_REAR2_SINX_DIFF1, &u8_sinx[0]);
	is_ois_read_u8(OIS_CMD_REAR2_SINX_DIFF2, &u8_sinx[1]);
	*sinx_2nd = (u8_sinx[1] << 8) | u8_sinx[0];
	if (*sinx_2nd > 0x7FFF) {
		*sinx_2nd = -((*sinx_2nd ^ 0xFFFF) + 1);
	}
	is_ois_read_u8(OIS_CMD_REAR2_SINY_DIFF1, &u8_siny[0]);
	is_ois_read_u8(OIS_CMD_REAR2_SINY_DIFF2, &u8_siny[1]);
	*siny_2nd = (u8_siny[1] << 8) | u8_siny[0];
	if (*siny_2nd > 0x7FFF) {
		*siny_2nd = -((*siny_2nd ^ 0xFFFF) + 1);
	}

	info_mcu("threshold = %d, sinx = %d, siny = %d, sinx_count = %d, syny_count = %d\n",
		threshold, *sinx, *siny, sinx_count, siny_count);

	info_mcu("threshold = %d, sinx_2nd = %d, siny_2nd = %d, sinx_count_2nd = %d, syny_count_2nd = %d\n",
		threshold, *sinx_2nd, *siny_2nd, sinx_count_2nd, siny_count_2nd);

	if (buf == 0x0) {
		return true;
	} else {
		return false;
	}
}

bool is_vendor_ois_auto_test_rear2(struct is_core *core,
					int threshold, bool *x_result, bool *y_result, int *sin_x, int *sin_y,
					bool *x_result_2nd, bool *y_result_2nd, int *sin_x_2nd, int *sin_y_2nd)
{
	int result = 0;
	bool value = false;

#ifdef CONFIG_AF_HOST_CONTROL
#if defined(CAMERA_2ND_OIS)
#ifdef USE_TELE_OIS_AF_COMMON_INTERFACE
	is_vendor_ois_af_move_lens(core);
#else
	is_af_move_lens(core, SENSOR_POSITION_REAR2);
#endif
	msleep(100);
#endif
#if defined(CAMERA_3RD_OIS)
#ifdef USE_TELE2_OIS_AF_COMMON_INTERFACE
	is_vendor_ois_af_move_lens(core);
#else
	is_af_move_lens(core, SENSOR_POSITION_REAR4);
#endif
	msleep(100);
#endif
	is_af_move_lens(core, SENSOR_POSITION_REAR);
	msleep(100);
#endif

	value = is_vendor_ois_sine_wavecheck_rear2(core, threshold, sin_x, sin_y, &result,
				sin_x_2nd, sin_y_2nd);

	if (*sin_x == -1 && *sin_y == -1) {
		err("OIS device is not prepared.");
		*x_result = false;
		*y_result = false;

		return false;
	}

	if (*sin_x_2nd == -1 && *sin_y_2nd == -1) {
		err("OIS 2 device is not prepared.");
		*x_result_2nd = false;
		*y_result_2nd = false;

		return false;
	}

	if (value == true) {
		*x_result = true;
		*y_result = true;
		*x_result_2nd = true;
		*y_result_2nd = true;

		return true;
	} else {
		err("OIS autotest_2nd is failed result (0x0051) = 0x%x\n", result);
		if ((result & 0x03) == 0x00) {
			*x_result = true;
			*y_result = true;
		} else if ((result & 0x03) == 0x01) {
			*x_result = false;
			*y_result = true;
		} else if ((result & 0x03) == 0x02) {
			*x_result = true;
			*y_result = false;
		} else {
			*x_result = false;
			*y_result = false;
		}

		if ((result & 0x30) == 0x00) {
			*x_result_2nd = true;
			*y_result_2nd = true;
		} else if ((result & 0x30) == 0x10) {
			*x_result_2nd = false;
			*y_result_2nd = true;
		} else if ((result & 0x30) == 0x20) {
			*x_result_2nd = true;
			*y_result_2nd = false;
		} else {
			*x_result_2nd = false;
			*y_result_2nd = false;
		}

		return false;
	}
}
#endif

void is_vendor_ois_enable(struct is_core *core)
{
	struct ois_mcu_dev *mcu = NULL;

	mcu = core->mcu;

	info_mcu("%s : E\n", __func__);

	is_ois_write_u8(OIS_CMD_MODE, 0x00);

	is_ois_write_u8(OIS_CMD_START, 0x01);

	info_mcu("%s : X\n", __func__);
}

int is_vendor_ois_disable(struct v4l2_subdev *subdev)
{
	struct ois_mcu_dev *mcu = NULL;

	info_mcu("%s : E\n", __func__);

	mcu = (struct ois_mcu_dev*)v4l2_get_subdevdata(subdev);
	if(!mcu) {
		err("%s, mcu subdev is NULL", __func__);
		return -EINVAL;
	}

	if (mcu->ois_hw_check) {
		is_ois_write_u8(OIS_CMD_START, 0x00);
		usleep_range(2000, 2100);

		mcu->ois_fadeupdown = false;
		mcu->ois_hw_check = false;

		/* off all ois */
		mcu->ois_wide_init = false;
		mcu->ois_tele_init = false;
		mcu->ois_tele2_init = false;

		info_mcu("%s ois stop.X\n", __func__);
	}

	info_mcu("%s : X\n", __func__);

	return 0;
}

void is_vendor_ois_get_hall_position(struct is_core *core, u16 *targetPos, u16 *hallPos)
{
	struct ois_mcu_dev *mcu = NULL;
	u8 pos_temp[2] = {0, };
	u16 pos = 0;

	mcu = core->mcu;

	info_mcu("%s : E\n", __func__);

	/* set centering mode */
	is_ois_write_u8(OIS_CMD_MODE, 0x05);

	/* enable position data read */
	is_ois_write_u8(OIS_CMD_FWINFO_CTRL, 0x01);

	msleep(150);

	is_ois_read_u8(OIS_CMD_TARGET_POS_REAR_X, &pos_temp[0]);
	is_ois_read_u8(OIS_CMD_TARGET_POS_REAR_X2, &pos_temp[1]);
	pos = (pos_temp[1] << 8) | pos_temp[0];
	targetPos[0] = pos;

	is_ois_read_u8(OIS_CMD_TARGET_POS_REAR_Y, &pos_temp[0]);
	is_ois_read_u8(OIS_CMD_TARGET_POS_REAR_Y2, &pos_temp[1]);
	pos = (pos_temp[1] << 8) | pos_temp[0];
	targetPos[1] = pos;

	is_ois_read_u8(OIS_CMD_HALL_POS_REAR_X, &pos_temp[0]);
	is_ois_read_u8(OIS_CMD_HALL_POS_REAR_X2, &pos_temp[1]);
	pos = (pos_temp[1] << 8) | pos_temp[0];
	hallPos[0] = pos;

	is_ois_read_u8(OIS_CMD_HALL_POS_REAR_Y, &pos_temp[0]);
	is_ois_read_u8(OIS_CMD_HALL_POS_REAR_Y2, &pos_temp[1]);
	pos = (pos_temp[1] << 8) | pos_temp[0];
	hallPos[1] = pos;

	info_mcu("%s : wide pos = 0x%04x, 0x%04x, 0x%04x, 0x%04x\n", __func__, targetPos[0], targetPos[1], hallPos[0], hallPos[1]);

#if defined(CAMERA_2ND_OIS)
	is_ois_read_u8(OIS_CMD_TARGET_POS_REAR2_X, &pos_temp[0]);
	is_ois_read_u8(OIS_CMD_TARGET_POS_REAR2_X2, &pos_temp[1]);
	pos = (pos_temp[1] << 8) | pos_temp[0];
	targetPos[2] = pos;

	is_ois_read_u8(OIS_CMD_TARGET_POS_REAR2_Y, &pos_temp[0]);
	is_ois_read_u8(OIS_CMD_TARGET_POS_REAR2_Y2, &pos_temp[1]);
	pos = (pos_temp[1] << 8) | pos_temp[0];
	targetPos[3] = pos;

	is_ois_read_u8(OIS_CMD_HALL_POS_REAR2_X, &pos_temp[0]);
	is_ois_read_u8(OIS_CMD_HALL_POS_REAR2_X2, &pos_temp[1]);
	pos = (pos_temp[1] << 8) | pos_temp[0];
	hallPos[2] = pos;

	is_ois_read_u8(OIS_CMD_HALL_POS_REAR2_Y, &pos_temp[0]);
	is_ois_read_u8(OIS_CMD_HALL_POS_REAR2_Y2, &pos_temp[1]);
	pos = (pos_temp[1] << 8) | pos_temp[0];
	hallPos[3] = pos;

	info_mcu("%s : tele pos = 0x%04x, 0x%04x, 0x%04x, 0x%04x\n", __func__, targetPos[2], targetPos[3], hallPos[2], hallPos[3]);
#endif
#if defined(CAMERA_3RD_OIS)
	is_ois_read_u8(OIS_CMD_TARGET_POS_REAR3_X, &pos_temp[0]);
	is_ois_read_u8(OIS_CMD_TARGET_POS_REAR3_X2, &pos_temp[1]);
	pos = (pos_temp[1] << 8) | pos_temp[0];
	targetPos[4] = pos;

	is_ois_read_u8(OIS_CMD_TARGET_POS_REAR3_Y, &pos_temp[0]);
	is_ois_read_u8(OIS_CMD_TARGET_POS_REAR3_Y2, &pos_temp[1]);
	pos = (pos_temp[1] << 8) | pos_temp[0];
	targetPos[5] = pos;

	is_ois_read_u8(OIS_CMD_HALL_POS_REAR3_X, &pos_temp[0]);
	is_ois_read_u8(OIS_CMD_HALL_POS_REAR3_X2, &pos_temp[1]);
	pos = (pos_temp[1] << 8) | pos_temp[0];
	hallPos[4] = pos;

	is_ois_read_u8(OIS_CMD_HALL_POS_REAR3_Y, &pos_temp[0]);
	is_ois_read_u8(OIS_CMD_HALL_POS_REAR3_Y2, &pos_temp[1]);
	pos = (pos_temp[1] << 8) | pos_temp[0];
	hallPos[5] = pos;

	info_mcu("%s : tele2 pos = 0x%04x, 0x%04x, 0x%04x, 0x%04x\n", __func__, targetPos[4], targetPos[5], hallPos[4], hallPos[5]);
#endif

	/* disable position data read */
	is_ois_write_u8(OIS_CMD_FWINFO_CTRL, 0x00);

	info_mcu("%s : X\n", __func__);
}

bool is_vendor_ois_offset_test(struct is_core *core, long *raw_data_x, long *raw_data_y, long *raw_data_z)
{
	int i = 0;
	u8 val = 0, x = 0, y = 0, z = 0;
	int x_sum = 0, y_sum = 0, z_sum = 0, sum = 0;
	int retries = 0, avg_count = 30;
	bool result = false;
	int scale_factor = OIS_GYRO_SCALE_FACTOR;
	struct ois_mcu_dev *mcu = NULL;

	mcu = core->mcu;

	info_mcu("%s : E\n", __func__);

	is_ois_write_u8(OIS_CMD_GYRO_CAL, 0x01);

	retries = avg_count;
	do {
		is_ois_read_u8(OIS_CMD_GYRO_CAL, &val);
		msleep(50);
		if (--retries < 0) {
			err("Read register failed!!!!, data = 0x%04x\n", val);
			break;
		}
	} while (val);

	/* Gyro result check */
	is_ois_read_u8(OIS_CMD_GYRO_RESULT, &val);

	if ((val & 0x63) == 0x0) {
		info_mcu("[%s] Gyro result check success. Result is OK. gyro value = 0x%02x", __func__, val);
		result = true;
	} else {
		info_mcu("[%s] Gyro result check fail. Result is NG. gyro value = 0x%02x", __func__, val);
		result = false;
	}

	sum = 0;
	retries = avg_count;
	for (i = 0; i < retries; retries--) {
		is_ois_read_u8(OIS_CMD_RAW_DEBUG_X1, &val);
		x = val;
		is_ois_read_u8(OIS_CMD_RAW_DEBUG_X2, &val);
		x_sum = (val << 8) | x;
		if (x_sum > 0x7FFF) {
			x_sum = -((x_sum ^ 0xFFFF) + 1);
		}
		sum += x_sum;
	}
	sum = sum * 10 / avg_count;
	*raw_data_x = sum * 1000 / scale_factor / 10;

	sum = 0;
	retries = avg_count;
	for (i = 0; i < retries; retries--) {
		is_ois_read_u8(OIS_CMD_RAW_DEBUG_Y1, &val);
		y = val;
		is_ois_read_u8(OIS_CMD_RAW_DEBUG_Y2, &val);
		y_sum = (val << 8) | y;
		if (y_sum > 0x7FFF) {
			y_sum = -((y_sum ^ 0xFFFF) + 1);
		}
		sum += y_sum;
	}
	sum = sum * 10 / avg_count;
	*raw_data_y = sum * 1000 / scale_factor / 10;

	sum = 0;
	retries = avg_count;
	for (i = 0; i < retries; retries--) {
		is_ois_read_u8(OIS_CMD_RAW_DEBUG_Z1, &val);
		z = val;
		is_ois_read_u8(OIS_CMD_RAW_DEBUG_Z2, &val);
		z_sum = (val << 8) | z;
		if (z_sum > 0x7FFF) {
			z_sum = -((z_sum ^ 0xFFFF) + 1);
		}
		sum += z_sum;
	}
	sum = sum * 10 / avg_count;
	*raw_data_z = sum * 1000 / scale_factor / 10;

	//is_mcu_fw_version(core); // TEMP_2020
	info_mcu("%s : X raw_x = %ld, raw_y = %ld, raw_z = %ld\n", __func__, *raw_data_x, *raw_data_y, *raw_data_z);

	return result;
}

void is_vendor_ois_get_offset_data(struct is_core *core, long *raw_data_x, long *raw_data_y, long *raw_data_z)
{
	u8 val = 0;
	int retries = 0, avg_count = 40;
	struct ois_mcu_dev *mcu = NULL;

	mcu = core->mcu;

	info_mcu("%s : E\n", __func__);

	/* check ois status */
	retries = avg_count;
	do {
		is_ois_read_u8(OIS_CMD_STATUS, &val);
		msleep(50);
		if (--retries < 0) {
			err_mcu("%s Read status failed!!!!, data = 0x%04x", __func__, val);
			break;
		}
	} while (val != 0x01);

	is_vendor_ois_get_efs_data(mcu, raw_data_x, raw_data_y, raw_data_z);

	return;
}

void is_vendor_ois_gyro_sleep(struct is_core *core)
{
	u8 val = 0;
	int retries = 20;
	struct ois_mcu_dev *mcu = NULL;

	mcu = core->mcu;

	is_ois_write_u8(OIS_CMD_START, 0x00);

	do {
		is_ois_read_u8(OIS_CMD_STATUS, &val);

		if (val == 0x01 || val == 0x13)
			break;

		usleep_range(1000, 1100);
	} while (--retries > 0);

	if (retries <= 0) {
		err("Read register failed!!!!, data = 0x%04x\n", val);
	}

	is_ois_write_u8(OIS_CMD_GYRO_SLEEP, 0x03);
	usleep_range(1000, 1100);

	return;
}

void is_vendor_ois_exif_data(struct is_core *core)
{
	u8 error_reg[2], status_reg;
	u16 error_sum;
	struct ois_mcu_dev *mcu = NULL;
	 struct is_ois_exif *ois_exif_data = NULL;

	mcu = core->mcu;

	is_ois_get_exif_data(&ois_exif_data);

	is_ois_read_u8(OIS_CMD_GYRO_RESULT, &error_reg[0]);
	is_ois_read_u8(OIS_CMD_CHECKSUM, &error_reg[1]);

	error_sum = (error_reg[1] << 8) | error_reg[0];

	is_ois_read_u8(OIS_CMD_STATUS, &status_reg);

	ois_exif_data->error_data = error_sum;
	ois_exif_data->status_data = status_reg;

	return;
}

u8 is_vendor_ois_read_status(struct is_core *core)
{
	u8 status = 0;
	struct ois_mcu_dev *mcu = NULL;

	mcu = core->mcu;

	is_ois_read_u8(OIS_CMD_READ_STATUS, &status);

	return status;
}

u8 is_vendor_ois_read_cal_checksum(struct is_core *core)
{
	u8 status = 0;
	struct ois_mcu_dev *mcu = NULL;

	mcu = core->mcu;

	is_ois_read_u8(OIS_CMD_CHECKSUM, &status);

	return status;
}

int is_vendor_ois_set_coef(struct v4l2_subdev *subdev, u8 coef)
{
	int ret = 0;
	struct is_ois *ois = NULL;
	struct ois_mcu_dev *mcu = NULL;
	struct is_mcu *is_mcu = NULL;

	WARN_ON(!subdev);

	is_mcu = (struct is_mcu *)v4l2_get_subdev_hostdata(subdev);
	if (!is_mcu) {
		err("%s, is_mcu is NULL", __func__);
		ret = -EINVAL;
		return ret;
	}

	mcu = (struct ois_mcu_dev *)v4l2_get_subdevdata(subdev);
	if (!mcu) {
		err("%s, mcu is NULL", __func__);
		ret = -EINVAL;
		return ret;
	}

	if (!mcu->ois_wide_init
#if defined(CAMERA_2ND_OIS)
		&& !mcu->ois_tele_init
#endif
#if defined(CAMERA_3RD_OIS)
		&& !mcu->ois_tele2_init
#endif
	)
		return 0;

	ois = is_mcu->ois;

	if (ois->pre_coef == coef)
		return ret;

	dbg_ois("%s %d\n", __func__, coef);

	is_ois_write_u8(OIS_CMD_SET_COEF, coef);

	ois->pre_coef = coef;

	return ret;
}

void is_vendor_ois_set_center_shift(struct v4l2_subdev *subdev, int16_t *shiftValue)
{
	int i = 0;
	int j = 0;
	u8 data[2];
	struct ois_mcu_dev *mcu = NULL;

	WARN_ON(!subdev);

	mcu = (struct ois_mcu_dev *)v4l2_get_subdevdata(subdev);
	if (!mcu) {
		err("%s, mcu is NULL", __func__);
		return;
	}

	info_mcu("%s wide x = %hd, wide y = %hd, tele x = %hd, tele y = %hd, tele2 x = %hd, tele2 y = %hd",
		__func__, shiftValue[0], shiftValue[1], shiftValue[2], shiftValue[3], shiftValue[4], shiftValue[5]);

	for (i = 0; i < 6; i++) {
		if (shiftValue[i]) {
			data[0] = shiftValue[i] & 0xFF;
			data[1] = (shiftValue[i] >> 8) & 0xFF;
			is_ois_write_u8(OIS_CMD_XCOEF_M1_1 + j++, data[0]);
			is_ois_write_u8(OIS_CMD_XCOEF_M1_1 + j++, data[1]);
		} else {
			j += 2;
		}
	}
}

int is_vendor_ois_set_centering(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_ois *ois = NULL;
	struct ois_mcu_dev *mcu = NULL;
	struct is_mcu *is_mcu = NULL;

	WARN_ON(!subdev);

	mcu = (struct ois_mcu_dev *)v4l2_get_subdevdata(subdev);
	if(!mcu) {
		err("%s, mcu is NULL", __func__);
		ret = -EINVAL;
		return ret;
	}

	is_mcu = (struct is_mcu *)v4l2_get_subdev_hostdata(subdev);
	if(!is_mcu) {
		err("%s, is_mcu is NULL", __func__);
		ret = -EINVAL;
		return ret;
	}

	ois = is_mcu->ois;

	is_ois_write_u8(OIS_CMD_MODE, 0x05);

	ois->pre_ois_mode = OPTICAL_STABILIZATION_MODE_CENTERING;

	return ret;
}

u8 is_vendor_ois_read_mode(struct v4l2_subdev *subdev)
{
	int ret = 0;
	u8 mode = OPTICAL_STABILIZATION_MODE_OFF;
	struct ois_mcu_dev *mcu = NULL;
	struct is_mcu *is_mcu = NULL;

	mcu = (struct ois_mcu_dev *)v4l2_get_subdevdata(subdev);
	if(!mcu) {
		err("%s, mcu is NULL", __func__);
		ret = -EINVAL;
		return ret;
	}

	is_mcu = (struct is_mcu *)v4l2_get_subdev_hostdata(subdev);
	if(!is_mcu) {
		err("%s, is_mcu is NULL", __func__);
		ret = -EINVAL;
		return ret;
	}

	is_ois_read_u8(OIS_CMD_MODE, &mode);

	switch(mode) {
		case 0x00:
			mode = OPTICAL_STABILIZATION_MODE_STILL;
			break;
		case 0x01:
			mode = OPTICAL_STABILIZATION_MODE_VIDEO;
			break;
		case 0x05:
			mode = OPTICAL_STABILIZATION_MODE_CENTERING;
			break;
		case 0x13:
			mode = OPTICAL_STABILIZATION_MODE_STILL_ZOOM;
			break;
		case 0x14:
			mode = OPTICAL_STABILIZATION_MODE_VDIS;
			break;
		default:
			dbg_ois("%s: ois_mode value(%d)\n", __func__, mode);
			break;
	}

	return mode;
}

bool is_vendor_ois_gyro_cal(struct is_core *core, long *x_value, long *y_value, long *z_value)
{
	u8 val = 0, x = 0, y = 0, z = 0;
	int retries = 30;
	int scale_factor = OIS_GYRO_SCALE_FACTOR;
	int x_sum = 0, y_sum = 0, z_sum = 0;
	bool result = false;
	struct ois_mcu_dev *mcu = NULL;

	mcu = core->mcu;

	info_mcu("%s : E\n", __func__);

	/* check ois status */
	do {
		is_ois_read_u8(OIS_CMD_STATUS, &val);
		msleep(20);
		if (--retries < 0) {
			err_mcu("%s Read status failed!!!!, data = 0x%04x", __func__, val);
			return false;
		}
	} while (val != 0x01);

	retries = 30;

	is_ois_write_u8(OIS_CMD_GYRO_CAL, 0x01);

	do {
		is_ois_read_u8(OIS_CMD_GYRO_CAL, &val);
		msleep(15);
		if (--retries < 0) {
			err("Read register failed!!!!, data = 0x%04x\n", val);
			break;
		}
	} while (val);

	/* Gyro result check */
	is_ois_read_u8(OIS_CMD_GYRO_RESULT, &val);

	if ((val & 0x63) == 0x0) {
		info_mcu("[%s] Written cal is OK. val = 0x%02x.", __func__, val);
		result = true;
	} else {
		info_mcu("[%s] Written cal is NG. val = 0x%02x.", __func__, val);
		result = false;
	}

	is_ois_read_u8(OIS_CMD_RAW_DEBUG_X1, &val);
	x = val;
	is_ois_read_u8(OIS_CMD_RAW_DEBUG_X2, &val);
	x_sum = (val << 8) | x;
	if (x_sum > 0x7FFF) {
		x_sum = -((x_sum ^ 0xFFFF) + 1);
	}

	is_ois_read_u8(OIS_CMD_RAW_DEBUG_Y1, &val);
	y = val;
	is_ois_read_u8(OIS_CMD_RAW_DEBUG_Y2, &val);
	y_sum = (val << 8) | y;
	if (y_sum > 0x7FFF) {
		y_sum = -((y_sum ^ 0xFFFF) + 1);
	}

	is_ois_read_u8(OIS_CMD_RAW_DEBUG_Z1, &val);
	z = val;
	is_ois_read_u8(OIS_CMD_RAW_DEBUG_Z2, &val);
	z_sum = (val << 8) | z;
	if (z_sum > 0x7FFF) {
		z_sum = -((z_sum ^ 0xFFFF) + 1);
	}

	*x_value = x_sum * 1000 / scale_factor;
	*y_value = y_sum * 1000 / scale_factor;
	*z_value = z_sum * 1000 / scale_factor;

	info_mcu("%s X (x = %ld/y = %ld/z = %ld) : result = %d\n", __func__, *x_value, *y_value, *z_value, result);

	return result;
}

bool is_vendor_ois_read_gyro_noise(struct is_core *core, long *x_value, long *y_value)
{
	u8 val = 0, x = 0, y = 0;
	int retries = 30;
	int scale_factor = OIS_GYRO_SCALE_FACTOR;
	int x_sum = 0, y_sum = 0;
	bool result = true;
	struct ois_mcu_dev *mcu = NULL;

	mcu = core->mcu;

	info_mcu("%s : E\n", __func__);

	msleep(500);

	is_ois_write_u8(OIS_CMD_START, 0x00);
	usleep_range(1000, 1100);

	/* check ois status */
	do {
		is_ois_read_u8(OIS_CMD_STATUS, &val);
		msleep(20);
		if (--retries < 0) {
			err_mcu("%s Read status failed!!!!, data = 0x%04x", __func__, val);
			result = false;
			break;
		}
	} while (val != 0x01);

	is_ois_write_u8(OIS_CMD_SET_GYRO_NOISE, 0x01);

	msleep(1000);

	is_ois_read_u8(OIS_CMD_READ_GYRO_NOISE_X1, &val);
	x = val;
	is_ois_read_u8(OIS_CMD_READ_GYRO_NOISE_X2, &val);
	x_sum = (val << 8) | x;
	if (x_sum > 0x7FFF) {
		x_sum = -((x_sum ^ 0xFFFF) + 1);
	}

	is_ois_read_u8(OIS_CMD_READ_GYRO_NOISE_Y1, &val);
	y = val;
	is_ois_read_u8(OIS_CMD_READ_GYRO_NOISE_Y2, &val);
	y_sum = (val << 8) | y;
	if (y_sum > 0x7FFF) {
		y_sum = -((y_sum ^ 0xFFFF) + 1);
	}

	*x_value = x_sum * 1000 / scale_factor;
	*y_value = y_sum * 1000 / scale_factor;

	info_mcu("%s X (x = %ld/y = %ld) : result = %d\n", __func__, *x_value, *y_value, result);

	return result;
}

void is_vendor_ois_check_valid(struct v4l2_subdev *subdev, u8 *value)
{
	struct ois_mcu_dev *mcu = NULL;
	u8 error_reg[2] = {0, };

	mcu = (struct ois_mcu_dev *)v4l2_get_subdevdata(subdev);
	if(!mcu) {
		err("%s, mcu is NULL", __func__);
		return;
	}

	is_vendor_ois_init_factory(subdev);

	is_ois_read_u8(OIS_CMD_GYRO_RESULT, &error_reg[0]);
	is_ois_read_u8(OIS_CMD_CHECKSUM, &error_reg[1]);
	info_mcu("%s error reg value = 0x%02x/0x%02x", __func__, error_reg[0], error_reg[1]);

	*value = error_reg[1];

	return;
}

bool is_vendor_ois_get_active(struct v4l2_subdev *subdev)
{
	struct ois_mcu_dev *mcu = NULL;

	mcu = (struct ois_mcu_dev *)v4l2_get_subdevdata(subdev);
	if(!mcu) {
		err("%s, mcu is NULL", __func__);
		return false;
	}

	return mcu->ois_hw_check;
}

static struct is_ois_ops ois_ops_mcu = {
	.ois_init = is_vendor_ois_init,
	.ois_init_fac = is_vendor_ois_init_factory,
#if defined(CAMERA_3RD_OIS)
	.ois_init_rear2 = is_vendor_ois_init_rear2,
#endif
	.ois_deinit = is_vendor_ois_deinit,
	.ois_set_mode = is_vendor_ois_set_mode,
	.ois_shift_compensation = is_vendor_ois_shift_compensation,
	.ois_self_test = is_vendor_ois_self_test,
	.ois_auto_test = is_vendor_ois_auto_test_all,
#if defined(CAMERA_2ND_OIS)
	.ois_auto_test_rear2 = is_vendor_ois_auto_test_rear2,
	.ois_set_power_mode = is_vendor_ois_set_power_mode,
#endif
	.ois_set_dev_ctrl = is_vendor_ois_set_dev_ctrl,
	.ois_check_fw = is_vendor_ois_check_fw,
	.ois_enable = is_vendor_ois_enable,
	.ois_disable = is_vendor_ois_disable,
	.ois_offset_test = is_vendor_ois_offset_test,
	.ois_get_offset_data = is_vendor_ois_get_offset_data,
	.ois_gyro_sleep = is_vendor_ois_gyro_sleep,
	.ois_exif_data = is_vendor_ois_exif_data,
	.ois_read_status = is_vendor_ois_read_status,
	.ois_read_cal_checksum = is_vendor_ois_read_cal_checksum,
	.ois_set_coef = is_vendor_ois_set_coef,
	.ois_set_center = is_vendor_ois_set_centering,
	.ois_read_mode = is_vendor_ois_read_mode,
	.ois_calibration_test = is_vendor_ois_gyro_cal,
	.ois_read_gyro_noise = is_vendor_ois_read_gyro_noise,
#ifdef USE_TELE2_OIS_AF_COMMON_INTERFACE
	.ois_set_af_active = is_vendor_ois_af_set_active,
#endif
	.ois_get_hall_pos = is_vendor_ois_get_hall_position,
	.ois_check_cross_talk = is_vendor_ois_check_cross_talk,
	.ois_check_hall_cal = is_vendor_ois_check_hall_cal,
	.ois_check_valid = is_vendor_ois_check_valid,
#ifdef USE_OIS_HALL_DATA_FOR_VDIS
	.ois_get_hall_data = is_vendor_ois_get_hall_data,
#endif
	.ois_get_active = is_vendor_ois_get_active,
	.ois_read_ext_clock = is_vendor_ois_read_ext_clock,
	.ois_parsing_raw_data = is_vendor_ois_parsing_raw_data,
	.ois_center_shift = is_vendor_ois_set_center_shift,
};

void is_vendor_ois_get_ops(struct is_ois_ops **ois_ops)
{
	*ois_ops = &ois_ops_mcu;
}
