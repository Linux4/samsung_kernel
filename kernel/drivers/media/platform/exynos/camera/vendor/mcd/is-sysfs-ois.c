/*
 * Samsung Exynos5 SoC series FIMC-IS driver
 *
 * exynos5 fimc-is core functions
 *
 * Copyright (c) 2023 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/device.h>

#include "is-core.h"
#include "is-sec-define.h"
#include "is-device-ois.h"
#include "is-vendor-private.h"
#include "is-vendor-ois.h"
#if defined(CONFIG_CAMERA_USE_EXTERNAL_MCU)
#include "is-vendor-ois-external-mcu.h"
#elif defined(CONFIG_CAMERA_USE_AOIS)
#include "is-vendor-aois.h"
#endif
#include "is-sysfs-ois.h"

struct device *camera_ois_dev;

bool check_ois_power;
bool check_shaking_noise;
int ois_power_count;
int shaking_power_count;
int ois_threshold = 150;
#ifdef CAMERA_2ND_OIS
int ois_threshold_rear2 = 150;
#endif
long x_init_raw;
long y_init_raw;
long z_init_raw;

void ois_factory_resource_clean(void)
{
	struct is_core *core = is_get_is_core();
	struct is_vendor *vendor;

	vendor = &core->vendor;

	info("%s ois power count = %d, shaking power count = %d\n", __func__, ois_power_count, shaking_power_count);

	if (check_ois_power) {
		is_ois_gpio_off(core);
		check_ois_power = false;
		info("%s ois_power off before entering suspend.\n", __func__);
	} else if (check_shaking_noise) {
		is_vendor_shaking_gpio_off(vendor);
		info("%s shaking_power off before entering suspend.\n", __func__);
	}
}

void ois_power_control(int onoff)
{
	struct is_core *core = is_get_is_core();
	bool camera_running;
#if defined(CAMERA_2ND_OIS)
	bool camera_running2;
#endif
#if defined(CAMERA_3RD_OIS)
	bool camera_running3;
#endif
	struct is_vendor *vendor;

	vendor = &core->vendor;

	is_vendor_check_hw_init_running();

	camera_running = is_vendor_check_camera_running(SENSOR_POSITION_REAR);
#if defined(CAMERA_2ND_OIS)
	camera_running2 = is_vendor_check_camera_running(SENSOR_POSITION_REAR2);
#endif
#if defined(CAMERA_3RD_OIS)
	camera_running3 = is_vendor_check_camera_running(SENSOR_POSITION_REAR4);
#endif

	switch (onoff) {
	case 0:
		if (!camera_running
#if defined(CAMERA_2ND_OIS)
			&& !camera_running2
#endif
#if defined(CAMERA_3RD_OIS)
			&& !camera_running3
#endif
			&& check_ois_power
		) {
			check_ois_power = false;
			is_ois_gpio_off(core);
			pr_info("%s ois_power off.\n", __func__);
		} else {
			pr_info("%s Unable to power off OIS\n", __func__);
		}
		break;
	case 1:
		if (!camera_running
#if defined(CAMERA_2ND_OIS)
			&& !camera_running2
#endif
#if defined(CAMERA_3RD_OIS)
			&& !camera_running3
#endif
			&& !check_ois_power
		) {
			check_ois_power = true;

			if (check_shaking_noise) {
				is_vendor_shaking_gpio_off(vendor);
				pr_info("%s shaking off before start ois power on.\n", __func__);
			}

			is_ois_gpio_on(core);
			msleep(150);
			pr_info("%s ois_power on.\n", __func__);
		} else {
			pr_info("%s Unable to power on OIS\n", __func__);
		}
		ois_power_count++;
		break;
	default:
		pr_debug("%s\n", __func__);
		break;
	}
}

bool read_ois_version(void)
{
	bool ret = true;
	struct is_core *core = is_get_is_core();
	struct is_vendor_private *vendor_priv = core->vendor.private_data;

	pr_info("%s\n", __func__);

	if (!vendor_priv->ois_ver_read) {
#ifndef CONFIG_CAMERA_USE_INTERNAL_MCU
		ois_power_control(1);
#endif
		ret = is_ois_check_fw(core);
#ifndef CONFIG_CAMERA_USE_INTERNAL_MCU
		ois_power_control(0);
#endif
	}

	return ret;
}

static ssize_t autotest_show(struct device *dev,
		struct device_attribute *attr, char *buf)

{
	struct is_core *core = is_get_is_core();
	bool x_result = false, y_result = false;
	bool x_result_2nd = false, y_result_2nd = false;
	bool x_result_3rd = false, y_result_3rd = false;
	int sin_x = 0, sin_y = 0;
	int sin_x_2nd = 0, sin_y_2nd = 0;
	int sin_x_3rd = 0, sin_y_3rd = 0;
	char wide_buffer[90] = {0, };
#ifdef CAMERA_2ND_OIS
	char tele_buffer[30] = {0, };
#endif
#ifdef CAMERA_3RD_OIS
	char tele2_buffer[30] = {0, };
#endif

	if (check_ois_power == false) {
		err("OIS power is not enabled.");
		sprintf(wide_buffer, "%s,%d,%s,%d", "fail", -1, "fail", -1);
#ifdef CAMERA_2ND_OIS
		sprintf(tele_buffer, ",%s,%d,%s,%d", "fail", -1, "fail", -1);
		strncat(wide_buffer, tele_buffer, strlen(tele_buffer));
#endif
#ifdef CAMERA_3RD_OIS
		sprintf(tele2_buffer, ",%s,%d,%s,%d", "fail", -1, "fail", -1);
		strncat(wide_buffer, tele2_buffer, strlen(tele2_buffer));
#endif
		info("%s result =  %s\n", __func__, wide_buffer);
		return sprintf(buf, "%s\n", wide_buffer);
	}

	is_ois_auto_test(core, ois_threshold,
		&x_result, &y_result, &sin_x, &sin_y,
		&x_result_2nd, &y_result_2nd, &sin_x_2nd, &sin_y_2nd,
		&x_result_3rd, &y_result_3rd, &sin_x_3rd, &sin_y_3rd);

	if (x_result && y_result)
		sprintf(wide_buffer, "%s,%d,%s,%d,", "pass", 0, "pass", 0);
	else if (x_result)
		sprintf(wide_buffer, "%s,%d,%s,%d,", "pass", 0, "fail", sin_y);
	else if (y_result)
		sprintf(wide_buffer, "%s,%d,%s,%d,", "fail", sin_x, "pass", 0);
	else
		sprintf(wide_buffer, "%s,%d,%s,%d,", "fail", sin_x, "fail", sin_y);
#ifdef CAMERA_2ND_OIS
	if (x_result_2nd && y_result_2nd)
		sprintf(tele_buffer, "%s,%d,%s,%d", "pass", 0, "pass", 0);
	else if (x_result_2nd)
		sprintf(tele_buffer, "%s,%d,%s,%d", "pass", 0, "fail", sin_y_2nd);
	else if (y_result_2nd)
		sprintf(tele_buffer, "%s,%d,%s,%d", "fail", sin_x_2nd, "pass", 0);
	else
		sprintf(tele_buffer, "%s,%d,%s,%d", "fail", sin_x_2nd, "fail", sin_y_2nd);

	strncat(wide_buffer, tele_buffer, strlen(tele_buffer));
#endif
#ifdef CAMERA_3RD_OIS
	if (x_result_3rd && y_result_3rd)
		sprintf(tele2_buffer, ",%s,%d,%s,%d", "pass", 0, "pass", 0);
	else if (x_result_3rd)
		sprintf(tele2_buffer, ",%s,%d,%s,%d", "pass", 0, "fail", sin_y_3rd);
	else if (y_result_3rd)
		sprintf(tele2_buffer, ",%s,%d,%s,%d", "fail", sin_x_3rd, "pass", 0);
	else
		sprintf(tele2_buffer, ",%s,%d,%s,%d", "fail", sin_x_3rd, "fail", sin_y_3rd);

	strncat(wide_buffer, tele2_buffer, strlen(tele2_buffer));
#endif
	info("%s result =  %s\n", __func__, wide_buffer);

	return sprintf(buf, "%s\n", wide_buffer);
}
static ssize_t autotest_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)

{
	int value = 0;

	if (kstrtoint(buf, 10, &value))
		err("convert fail");

	ois_threshold = value;

	return count;
}
static DEVICE_ATTR_RW(autotest);

static ssize_t calibrationtest_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct is_core *core = is_get_is_core();
	bool result = false;
	long raw_data_x = 0, raw_data_y = 0, raw_data_z = 0;
	char x_buf[90] = {0, };
	char y_buf[30] = {0, };
	char z_buf[30] = {0, };

	if (check_ois_power == false) {
		err("OIS power is not enabled.");
		return sprintf(buf, "%d,%ld.%03ld,%ld.%03ld,%ld.%03ld\n", result, raw_data_x / 1000,
			 raw_data_x % 1000, raw_data_y / 1000, raw_data_y % 1000, raw_data_z / 1000, raw_data_z % 1000);
	}

	is_ois_init_factory(core);
	result = is_ois_gyrocal_test(core, &raw_data_x, &raw_data_y, &raw_data_z);

	if (raw_data_x < 0)
		sprintf(x_buf, "%d,-%ld.%03ld", result, abs(raw_data_x / 1000), abs(raw_data_x % 1000));
	else if (raw_data_x >= 0)
		sprintf(x_buf, "%d,%ld.%03ld", result, abs(raw_data_x / 1000), abs(raw_data_x % 1000));

	if (raw_data_y < 0)
		sprintf(y_buf, ",-%ld.%03ld", abs(raw_data_y / 1000), abs(raw_data_y % 1000));
	else if (raw_data_y >= 0)
		sprintf(y_buf, ",%ld.%03ld", abs(raw_data_y / 1000), abs(raw_data_y % 1000));

	strncat(x_buf, y_buf, strlen(y_buf));

	if (raw_data_z < 0)
		sprintf(z_buf, ",-%ld.%03ld", abs(raw_data_z / 1000), abs(raw_data_z % 1000));
	else if (raw_data_z >= 0)
		sprintf(z_buf, ",%ld.%03ld", abs(raw_data_z / 1000), abs(raw_data_z % 1000));

	strncat(x_buf, z_buf, strlen(z_buf));

	info("%s raw_data_x = %ld, raw_data_y = %ld, raw_data_z = %ld\n", __func__,
		raw_data_x, raw_data_y, raw_data_z);

	return sprintf(buf, "%s\n", x_buf);
}
static DEVICE_ATTR_RO(calibrationtest);

static ssize_t check_cross_talk_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct is_core *core = is_get_is_core();
	u16 hall_data[11] = {0, };
	int result = 0;

	if (check_ois_power) {
		is_ois_check_cross_talk(core, hall_data);
		result = 1;
	} else {
		err("OIS power is not enabled.");
		result = 0;
	}

	return sprintf(buf, "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\n", result, hall_data[0], hall_data[1],
		hall_data[2], hall_data[3], hall_data[4], hall_data[5],
		hall_data[6], hall_data[7], hall_data[8], hall_data[9]);
}
static DEVICE_ATTR_RO(check_cross_talk);

static ssize_t check_hall_cal_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct is_core *core = is_get_is_core();
	u16 hall_cal_data[9] = {0, };
	int result = 0;

	if (check_ois_power) {
		is_ois_check_hall_cal(core, hall_cal_data);
		result = 1;

		info("[%s] %d,%d,%d,%d,%d,%d,%d,%d\n", __func__, hall_cal_data[0], hall_cal_data[1],
			hall_cal_data[2], hall_cal_data[3], hall_cal_data[4],
			hall_cal_data[5], hall_cal_data[6], hall_cal_data[7]);
	} else {
		err("OIS power is not enabled.");
		result = 0;
	}

	return sprintf(buf, "%d,%d,%d,%d,%d,%d,%d,%d,%d\n", result, hall_cal_data[0], hall_cal_data[1],
		hall_cal_data[2], hall_cal_data[3], hall_cal_data[4],
		hall_cal_data[5], hall_cal_data[6], hall_cal_data[7]);
}
static DEVICE_ATTR_RO(check_hall_cal);

static ssize_t check_ois_valid_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct is_core *core = is_get_is_core();
	u8 value = 0;
	u8 wide_val = 0;
	u8 tele_val = 0;
	u8 tele2_val = 0;

	if (check_ois_power == false) {
		err("OIS power is not enabled.");
		return sprintf(buf, "0x%02x,0x%02x,0x%02x\n", 0x06, 0x18, 0x60);
	}

	is_ois_check_valid(core, &value);

	wide_val = value & 0x06;
	tele_val = value & 0x18;
	tele2_val = value & 0x60;

	info("[%s] 0x%02x,0x%02x,0x%02x\n", __func__, wide_val, tele_val, tele2_val);
	return sprintf(buf, "0x%02x,0x%02x,0x%02x\n", wide_val, tele_val, tele2_val);
}
static DEVICE_ATTR_RO(check_ois_valid);

static ssize_t ois_center_shift_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int ret = 0;
	int16_t shiftValue[7] = {0, };
	struct is_core *core = is_get_is_core();

	ret = sscanf(buf, "%hd,%hd,%hd,%hd,%hd,%hd",
		&shiftValue[0], &shiftValue[1], &shiftValue[2], &shiftValue[3], &shiftValue[4], &shiftValue[5]);

	is_ois_set_center_shift(core, shiftValue);

	return count;
}
static DEVICE_ATTR_WO(ois_center_shift);

static ssize_t ois_exif_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct is_ois_info *ois_minfo = NULL;
	struct is_ois_info *ois_pinfo = NULL;
	struct is_ois_info *ois_uinfo = NULL;
	struct is_ois_exif *ois_exif = NULL;

	is_ois_get_module_version(&ois_minfo);
	is_ois_get_phone_version(&ois_pinfo);
	is_ois_get_user_version(&ois_uinfo);
	is_ois_get_exif_data(&ois_exif);

	return sprintf(buf, "%s %s %s %d %d", ois_minfo->header_ver, ois_pinfo->header_ver,
		ois_uinfo->header_ver, ois_exif->error_data, ois_exif->status_data);
}
static DEVICE_ATTR_RO(ois_exif);

static ssize_t ois_ext_clk_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct is_core *core = is_get_is_core();
	u32 clock = 0;

	if (check_ois_power)
		is_ois_read_ext_clock(core, &clock);
	else
		err("OIS power is not enabled.");

	return sprintf(buf, "%u\n", clock);
}
static DEVICE_ATTR_RO(ois_ext_clk);

static ssize_t ois_gain_rear_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct is_ois_info *ois_pinfo = NULL;
	u32 xgain = 0;
	u32 ygain = 0;

	is_ois_get_phone_version(&ois_pinfo);
	xgain = (ois_pinfo->wide_romdata.xgg[3] << 24)
			| (ois_pinfo->wide_romdata.xgg[2] << 16)
			| (ois_pinfo->wide_romdata.xgg[1] << 8)
			| (ois_pinfo->wide_romdata.xgg[0]);
	ygain = (ois_pinfo->wide_romdata.ygg[3] << 24)
			| (ois_pinfo->wide_romdata.ygg[2] << 16)
			| (ois_pinfo->wide_romdata.ygg[1] << 8)
			| (ois_pinfo->wide_romdata.ygg[0]);

	info("%s xgain/ygain = 0x%08x/0x%08x", __func__, xgain, ygain);

	if ((xgain == 0xFFFFFFFF) && (ygain == 0xFFFFFFFF))
		return sprintf(buf, "%d\n", 2);
	else if (ois_pinfo->wide_romdata.cal_mark[0] != 0xBB)
		return sprintf(buf, "%d\n", 1);
	else
		return sprintf(buf, "%d,0x%08x,0x%08x\n", 0, xgain, ygain);
}
static DEVICE_ATTR_RO(ois_gain_rear);

#ifdef CAMERA_2ND_OIS
static ssize_t ois_gain_rear3_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct is_ois_info *ois_pinfo = NULL;
	u32 xgain = 0;
	u32 ygain = 0;

	is_ois_get_phone_version(&ois_pinfo);
	xgain = (ois_pinfo->tele_romdata.xgg[3] << 24)
			| (ois_pinfo->tele_romdata.xgg[2] << 16)
			| (ois_pinfo->tele_romdata.xgg[1] << 8)
			| (ois_pinfo->tele_romdata.xgg[0]);
	ygain = (ois_pinfo->tele_romdata.ygg[3] << 24)
			| (ois_pinfo->tele_romdata.ygg[2] << 16)
			| (ois_pinfo->tele_romdata.ygg[1] << 8)
			| (ois_pinfo->tele_romdata.ygg[0]);

	info("%s xgain/ygain = 0x%08x/0x%08x", __func__, xgain, ygain);

	if ((xgain == 0xFFFFFFFF) && (ygain == 0xFFFFFFFF))
		return sprintf(buf, "%d\n", 2);
	else if (ois_pinfo->tele_romdata.cal_mark[0] != 0xBB)
		return sprintf(buf, "%d\n", 1);
	else
		return sprintf(buf, "%d,0x%08x,0x%08x\n", 0, xgain, ygain);
}
static DEVICE_ATTR_RO(ois_gain_rear3);
#endif

#ifdef CAMERA_3RD_OIS
static ssize_t ois_gain_rear4_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct is_ois_info *ois_pinfo = NULL;
	u32 xgain = 0;
	u32 ygain = 0;

	is_ois_get_phone_version(&ois_pinfo);
	xgain = (ois_pinfo->tele2_romdata.xgg[3] << 24)
			| (ois_pinfo->tele2_romdata.xgg[2] << 16)
			| (ois_pinfo->tele2_romdata.xgg[1] << 8)
			| (ois_pinfo->tele2_romdata.xgg[0]);
	ygain = (ois_pinfo->tele2_romdata.ygg[3] << 24)
			| (ois_pinfo->tele2_romdata.ygg[2] << 16)
			| (ois_pinfo->tele2_romdata.ygg[1] << 8)
			| (ois_pinfo->tele2_romdata.ygg[0]);

	info("%s xgain/ygain = 0x%08x/0x%08x", __func__, xgain, ygain);

	if ((xgain == 0xFFFFFFFF) && (ygain == 0xFFFFFFFF))
		return sprintf(buf, "%d\n", 2);
	else if (ois_pinfo->tele2_romdata.cal_mark[0] != 0xBB)
		return sprintf(buf, "%d\n", 1);
	else
		return sprintf(buf, "%d,0x%08x,0x%08x\n", 0, xgain, ygain);
}
static DEVICE_ATTR_RO(ois_gain_rear4);
#endif

static ssize_t ois_hall_position_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct is_core *core = is_get_is_core();
	u16 targetPos[6] = {0, };
	u16 hallPos[6] = {0, };
	bool camera_running;
#ifdef CAMERA_2ND_OIS
	bool camera_running2;
#endif
#ifdef CAMERA_3RD_OIS
	bool camera_running3;
#endif

	camera_running = is_vendor_check_camera_running(SENSOR_POSITION_REAR);
#ifdef CAMERA_2ND_OIS
	camera_running2 = is_vendor_check_camera_running(SENSOR_POSITION_REAR2);
#endif
#ifdef CAMERA_3RD_OIS
	camera_running3 = is_vendor_check_camera_running(SENSOR_POSITION_REAR4);
#endif

	if (camera_running
#ifdef CAMERA_2ND_OIS
		|| camera_running2
#endif
#ifdef CAMERA_3RD_OIS
		|| camera_running3
#endif
		)
		is_ois_get_hall_pos(core, targetPos, hallPos);
	else
		err("Camera power is not enabled.");

#ifdef CAMERA_3RD_OIS
	info("[%s] %u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u", __func__,
		targetPos[0], targetPos[1], targetPos[2], targetPos[3], targetPos[4], targetPos[5],
		hallPos[0], hallPos[1], hallPos[2], hallPos[3], hallPos[4], hallPos[5]);

	return sprintf(buf, "%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u",
		targetPos[0], targetPos[1], targetPos[2], targetPos[3], targetPos[4], targetPos[5],
		hallPos[0], hallPos[1], hallPos[2], hallPos[3], hallPos[4], hallPos[5]);
#elif defined(CAMERA_2ND_OIS)
	info("[%s] %u,%u,%u,%u,%u,%u,%u,%u", __func__,
		targetPos[0], targetPos[1], targetPos[2], targetPos[3],
		hallPos[0], hallPos[1], hallPos[2], hallPos[3]);

	return sprintf(buf, "%u,%u,%u,%u,%u,%u,%u,%u",
		targetPos[0], targetPos[1], targetPos[2], targetPos[3],
		hallPos[0], hallPos[1], hallPos[2], hallPos[3]);
#else
	info("[%s] %u,%u,%u,%u", __func__,
		targetPos[0], targetPos[1],
		hallPos[0], hallPos[1]);

	return sprintf(buf, "%u,%u,%u,%u,%u,%u,%u,%u",
		targetPos[0], targetPos[1], 2048, 2048,
		hallPos[0], hallPos[1], 2048, 2048);
#endif
}
static DEVICE_ATTR_RO(ois_hall_position);

static ssize_t ois_noise_stdev_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct is_core *core = is_get_is_core();
	bool result = false;
	long raw_data_x = 0, raw_data_y = 0;
	bool camera_running;
#ifdef CAMERA_2ND_OIS
	bool camera_running2;
#endif
#ifdef CAMERA_3RD_OIS
	bool camera_running3;
#endif
	bool check_power = false;

	camera_running = is_vendor_check_camera_running(SENSOR_POSITION_REAR);
#ifdef CAMERA_2ND_OIS
	camera_running2 = is_vendor_check_camera_running(SENSOR_POSITION_REAR2);
#endif
#ifdef CAMERA_3RD_OIS
	camera_running3 = is_vendor_check_camera_running(SENSOR_POSITION_REAR4);
#endif

	if (!camera_running
#ifdef CAMERA_2ND_OIS
		&& !camera_running2
#endif
#ifdef CAMERA_3RD_OIS
		&& !camera_running3
#endif
	) {
		check_power = check_ois_power;
		if (check_power)
			is_ois_init_factory(core);
	} else {
		check_power = true;
	}

	if (check_power == false) {
		err("OIS power is not enabled.");
		return sprintf(buf, "%d,%ld.%03ld,%ld.%03ld\n", result, raw_data_x / 1000,
			 raw_data_x % 1000, raw_data_y / 1000, raw_data_y % 1000);
	}

	result = is_ois_gyronoise_test(core, &raw_data_x, &raw_data_y);

	info("%s raw_data_x = %ld, raw_data_y = %ld\n", __func__, raw_data_x, raw_data_y);

	if (raw_data_x < 0 && raw_data_y < 0)
		return sprintf(buf, "%d,-%ld.%03ld,-%ld.%03ld\n", result, abs(raw_data_x / 1000),
			abs(raw_data_x % 1000), abs(raw_data_y / 1000), abs(raw_data_y % 1000));
	else if (raw_data_x < 0)
		return sprintf(buf, "%d,-%ld.%03ld,%ld.%03ld\n", result, abs(raw_data_x / 1000),
			abs(raw_data_x % 1000), raw_data_y / 1000, raw_data_y % 1000);
	else if (raw_data_y < 0)
		return sprintf(buf, "%d,%ld.%03ld,-%ld.%03ld\n", result, raw_data_x / 1000,
			raw_data_x % 1000, abs(raw_data_y / 1000), abs(raw_data_y % 1000));
	else
		return sprintf(buf, "%d,%ld.%03ld,%ld.%03ld\n", result, raw_data_x / 1000,
			raw_data_x % 1000, raw_data_y / 1000, raw_data_y % 1000);
}
static DEVICE_ATTR_RO(ois_noise_stdev);

static ssize_t ois_power_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)

{
	pr_info("%s: %c\n", __func__, buf[0]);

	switch (buf[0]) {
	case '0':
		ois_power_control(0);
		break;
	case '1':
		ois_power_control(1);
		break;
	default:
		pr_debug("%s: %c\n", __func__, buf[0]);
		break;
	}

	return count;
}
static DEVICE_ATTR_WO(ois_power);

static ssize_t ois_rawdata_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	long raw_data_x = 0, raw_data_y = 0, raw_data_z = 0;
	char x_buf[90] = {0, };
	char y_buf[30] = {0, };
	char z_buf[30] = {0, };

	if (check_ois_power == false) {
		err("OIS power is not enabled.");
		return sprintf(buf, "%ld.%03ld,%ld.%03ld,%ld.%03ld\n", raw_data_x / 1000, raw_data_x % 1000,
			raw_data_y / 1000, raw_data_y % 1000, raw_data_z / 1000, raw_data_z % 1000);
	}

	raw_data_x = x_init_raw;
	raw_data_y = y_init_raw;
	raw_data_z = z_init_raw;

	if (raw_data_x < 0)
		sprintf(x_buf, "-%ld.%03ld", abs(raw_data_x / 1000), abs(raw_data_x % 1000));
	else if (raw_data_x >= 0)
		sprintf(x_buf, "%ld.%03ld", abs(raw_data_x / 1000), abs(raw_data_x % 1000));

	if (raw_data_y < 0)
		sprintf(y_buf, ",-%ld.%03ld", abs(raw_data_y / 1000), abs(raw_data_y % 1000));
	else if (raw_data_y >= 0)
		sprintf(y_buf, ",%ld.%03ld", abs(raw_data_y / 1000), abs(raw_data_y % 1000));

	strncat(x_buf, y_buf, strlen(y_buf));

	if (raw_data_z < 0)
		sprintf(z_buf, ",-%ld.%03ld", abs(raw_data_z / 1000), abs(raw_data_z % 1000));
	else if (raw_data_z >= 0)
		sprintf(z_buf, ",%ld.%03ld", abs(raw_data_z / 1000), abs(raw_data_z % 1000));

	strncat(x_buf, z_buf, strlen(z_buf));

	info("%s raw_data_x = %ld, raw_data_y = %ld, raw_data_z = %ld\n", __func__,
		raw_data_x, raw_data_y, raw_data_z);

	return sprintf(buf, "%s\n", x_buf);
}
static ssize_t ois_rawdata_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct is_core *core = is_get_is_core();
	uint8_t raw_data[MAX_GYRO_EFS_DATA_LENGTH + 1] = {0, };
	long raw_data_x = 0, raw_data_y = 0, raw_data_z = 0;
	long efs_size = 0;

	if (check_ois_power == false) {
		err("%s OIS power is not enabled.", __func__);
		return 0;
	}

	if (count > MAX_GYRO_EFS_DATA_LENGTH || count == 0) {
		err("%s count is abnormal, count = %zu", __func__, count);
		return 0;
	}

	sscanf(buf, "%s", raw_data);

	efs_size = strlen(raw_data);

	is_ois_parsing_raw_data(core, raw_data, efs_size, &raw_data_x, &raw_data_y, &raw_data_z);

	x_init_raw = raw_data_x;
	y_init_raw = raw_data_y;
	z_init_raw = raw_data_z;

	info("%s efs data = %s, size = %ld, raw x = %ld, raw y = %ld, raw z = %ld", __func__,
		buf, efs_size, raw_data_x, raw_data_y, raw_data_z);

	return count;
}
static DEVICE_ATTR_RW(ois_rawdata);

static ssize_t ois_set_mode_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct is_core *core = is_get_is_core();
	int value = 0;

	if (kstrtoint(buf, 10, &value))
		err("convert fail");

	if (check_ois_power)
		is_ois_set_mode(core, value);
	else
		err("OIS power is not enabled.");

	return count;
}
static DEVICE_ATTR_WO(ois_set_mode);

static ssize_t ois_supperssion_ratio_rear_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct is_ois_info *ois_pinfo = NULL;
	u16 xratio = 0;
	u16 yratio = 0;

	is_ois_get_phone_version(&ois_pinfo);
	xratio = (ois_pinfo->wide_romdata.supperssion_xratio[1] << 8)
			| (ois_pinfo->wide_romdata.supperssion_xratio[0]);
	yratio = (ois_pinfo->wide_romdata.supperssion_yratio[1] << 8)
			| (ois_pinfo->wide_romdata.supperssion_yratio[0]);

	info("%s xratio/yratio = %d.%d/%d.%d", __func__, xratio / 100, xratio % 100, yratio / 100, yratio % 100);

	if ((xratio == 0xFFFF) && (yratio == 0xFFFF))
		return sprintf(buf, "%d\n", 2);
	else if (ois_pinfo->wide_romdata.cal_mark[0] != 0xBB)
		return sprintf(buf, "%d\n", 1);
	else
		return sprintf(buf, "%d,%d.%d,%d.%d\n", 0, xratio / 100, xratio % 100, yratio / 100, yratio % 100);
}
static DEVICE_ATTR_RO(ois_supperssion_ratio_rear);

#ifdef CAMERA_2ND_OIS
static ssize_t ois_supperssion_ratio_rear3_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct is_ois_info *ois_pinfo = NULL;
	u16 xratio = 0;
	u16 yratio = 0;

	is_ois_get_phone_version(&ois_pinfo);
	xratio = (ois_pinfo->tele_romdata.supperssion_xratio[1] << 8)
			| (ois_pinfo->tele_romdata.supperssion_xratio[0]);
	yratio = (ois_pinfo->tele_romdata.supperssion_yratio[1] << 8)
			| (ois_pinfo->tele_romdata.supperssion_yratio[0]);

	info("%s xratio/yratio = %d.%d/%d.%d", __func__, xratio / 100, xratio % 100, yratio / 100, yratio % 100);

	if ((xratio == 0xFFFF) && (yratio == 0xFFFF))
		return sprintf(buf, "%d\n", 2);
	else if (ois_pinfo->tele_romdata.cal_mark[0] != 0xBB)
		return sprintf(buf, "%d\n", 1);
	else
		return sprintf(buf, "%d,%d.%d,%d.%d\n", 0, xratio / 100, xratio % 100, yratio / 100, yratio % 100);
}
static DEVICE_ATTR_RO(ois_supperssion_ratio_rear3);
#endif

#ifdef CAMERA_3RD_OIS
static ssize_t ois_supperssion_ratio_rear4_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct is_ois_info *ois_pinfo = NULL;
	u16 xratio = 0;
	u16 yratio = 0;

	is_ois_get_phone_version(&ois_pinfo);
	xratio = (ois_pinfo->tele2_romdata.supperssion_xratio[1] << 8)
			| (ois_pinfo->tele2_romdata.supperssion_xratio[0]);
	yratio = (ois_pinfo->tele2_romdata.supperssion_yratio[1] << 8)
			| (ois_pinfo->tele2_romdata.supperssion_yratio[0]);

	info("%s xratio/yratio = %d.%d/%d.%d", __func__, xratio / 100, xratio % 100, yratio / 100, yratio % 100);

	if ((xratio == 0xFFFF) && (yratio == 0xFFFF))
		return sprintf(buf, "%d\n", 2);
	else if (ois_pinfo->tele2_romdata.cal_mark[0] != 0xBB)
		return sprintf(buf, "%d\n", 1);
	else
		return sprintf(buf, "%d,%d.%d,%d.%d\n", 0, xratio / 100, xratio % 100, yratio / 100, yratio % 100);
}
static DEVICE_ATTR_RO(ois_supperssion_ratio_rear4);
#endif

static ssize_t oisfw_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct is_ois_info *ois_minfo = NULL;
	struct is_ois_info *ois_pinfo = NULL;
	bool ret = false;

	is_vendor_check_hw_init_running();
	ret = read_ois_version();
	is_ois_get_module_version(&ois_minfo);
	is_ois_get_phone_version(&ois_pinfo);

	info("%s ois fw info = %s/%s", __func__, ois_minfo->header_ver, ois_pinfo->header_ver);

	if (ois_minfo->checksum != 0x00 || !ret)
		return sprintf(buf, "%s %s\n", "NG_FW2", "NULL");
	else if (ois_minfo->caldata != 0x00)
		return sprintf(buf, "%s %s\n", "NG_CD2", ois_pinfo->header_ver);
	else
		return sprintf(buf, "%s %s\n", ois_minfo->header_ver, ois_pinfo->header_ver);
}
static DEVICE_ATTR_RO(oisfw);

static ssize_t prevent_shaking_noise_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct is_core *core = is_get_is_core();
	bool camera_running;
	bool camera_running2;
	struct is_vendor *vendor;

	vendor = &core->vendor;

#if defined(CONFIG_SEC_FACTORY)
	return 0;
#endif
	pr_info("%s: %c\n", __func__, buf[0]);

	is_vendor_check_hw_init_running();

	camera_running = is_vendor_check_camera_running(SENSOR_POSITION_REAR);
	camera_running2 = is_vendor_check_camera_running(SENSOR_POSITION_REAR2);

	switch (buf[0]) {
	case '0':
		if (!camera_running && !camera_running2 && check_shaking_noise && !check_ois_power) {
			is_vendor_shaking_gpio_off(vendor);
			pr_info("%s shaking_power off.\n", __func__);
		} else {
			pr_info("%s Do not set shaking power off.\n", __func__);
		}

		check_shaking_noise = false;
		break;
	case '1':
		if (!camera_running && !camera_running2 && !check_shaking_noise && !check_ois_power) {
			is_vendor_shaking_gpio_on(vendor);
			pr_info("%s shaking_power on.\n", __func__);
		} else {
			pr_info("%s Do not set shaking power on.\n", __func__);
		}

		shaking_power_count++;
		break;
	default:
		pr_debug("%s: %c\n", __func__, buf[0]);
		break;
	}
	return count;
}
static DEVICE_ATTR_WO(prevent_shaking_noise);

#ifdef CAMERA_2ND_OIS
static ssize_t rear3_read_cross_talk_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct is_ois_info *ois_pinfo = NULL;
	int xvalue = 0;
	int yvalue = 0;
	int xvalue_pre = 0;
	int yvalue_pre = 0;
	int xvalue_post = 0;
	int yvalue_post = 0;

	is_ois_get_phone_version(&ois_pinfo);
	xvalue = (ois_pinfo->tele_romdata.xcoef[1] << 8) | (ois_pinfo->tele_romdata.xcoef[0]);
	yvalue = (ois_pinfo->tele_romdata.ycoef[1] << 8) | (ois_pinfo->tele_romdata.ycoef[0]);

	info("%s value =  0x%02x, 0x%02x, 0x%02x, 0x%02x", __func__,
		ois_pinfo->tele_romdata.xcoef[0], ois_pinfo->tele_romdata.xcoef[1],
		ois_pinfo->tele_romdata.ycoef[0], ois_pinfo->tele_romdata.ycoef[1]);

	if ((ois_pinfo->tele_romdata.xcoef[0] == 0xFF && ois_pinfo->tele_romdata.xcoef[1] == 0xFF) &&
		(ois_pinfo->tele_romdata.ycoef[0] == 0xFF && ois_pinfo->tele_romdata.ycoef[1] == 0xFF)) {
		return sprintf(buf, "NONE\n");
	}

	if (ois_pinfo->tele_romdata.cal_mark[0] != 0xBB)
		return sprintf(buf, "NONE\n");

	xvalue_pre = xvalue / 100;
	yvalue_pre = yvalue / 100;
	xvalue_post = abs(xvalue) - abs(xvalue_pre) * 100;
	yvalue_post = abs(yvalue) - abs(yvalue_pre) * 100;

	return sprintf(buf, "%d.%d%d,%d.%d%d\n", xvalue_pre, xvalue_post / 10, xvalue_post % 10,
		yvalue_pre, yvalue_post / 10, yvalue_post % 10);
}
static DEVICE_ATTR_RO(rear3_read_cross_talk);
#endif

static ssize_t reset_check_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct is_ois_info *ois_pinfo = NULL;

	is_ois_get_phone_version(&ois_pinfo);

	return sprintf(buf, "%d", ois_pinfo->reset_check);
}
static DEVICE_ATTR_RO(reset_check);

static ssize_t selftest_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct is_core *core = is_get_is_core();
	int result_total = 0;
	bool result_offset = 0, result_selftest = 0;
	int selftest_ret = 0, offsettest_ret = 0;
	long raw_data_x = 0, raw_data_y = 0, raw_data_z = 0;
	char x_buf[90] = {0, };
	char y_buf[30] = {0, };
	char z_buf[30] = {0, };

	if (check_ois_power == false) {
		err("OIS power is not enabled.");
		return sprintf(buf, "%d,%ld.%03ld,%ld.%03ld,%ld.%03ld\n", result_total,
			raw_data_x / 1000, raw_data_x % 1000,
			raw_data_y / 1000, raw_data_y % 1000,
			raw_data_z / 1000, raw_data_z % 1000);
	}

	is_ois_init_factory(core);
	offsettest_ret = is_ois_offset_test(core, &raw_data_x, &raw_data_y, &raw_data_z);
	msleep(50);
	selftest_ret = is_ois_self_test(core);

	if (selftest_ret == 0x0)
		result_selftest = true;
	else
		result_selftest = false;

	if (abs(raw_data_x) > CAMERA_OIS_GYRO_OFFSET_SPEC || abs(raw_data_y) > CAMERA_OIS_GYRO_OFFSET_SPEC
		|| abs(raw_data_z) > CAMERA_OIS_GYRO_OFFSET_SPEC
		|| abs(raw_data_x - x_init_raw) > CAMERA_OIS_GYRO_OFFSET_SPEC
		|| abs(raw_data_y - y_init_raw) > CAMERA_OIS_GYRO_OFFSET_SPEC
		|| abs(raw_data_z - z_init_raw) > CAMERA_OIS_GYRO_OFFSET_SPEC
		|| offsettest_ret == false)  {
		result_offset = false;
	} else {
		result_offset = true;
	}

	if (result_offset && result_selftest)
		result_total = 0;
	else if (!result_offset && !result_selftest)
		result_total = 3;
	else if (!result_offset)
		result_total = 1;
	else if (!result_selftest)
		result_total = 2;

	if (raw_data_x < 0)
		sprintf(x_buf, "%d,-%ld.%03ld", result_total, abs(raw_data_x / 1000), abs(raw_data_x % 1000));
	else if (raw_data_x >= 0)
		sprintf(x_buf, "%d,%ld.%03ld", result_total, abs(raw_data_x / 1000), abs(raw_data_x % 1000));

	if (raw_data_y < 0)
		sprintf(y_buf, ",-%ld.%03ld", abs(raw_data_y / 1000), abs(raw_data_y % 1000));
	else if (raw_data_y >= 0)
		sprintf(y_buf, ",%ld.%03ld", abs(raw_data_y / 1000), abs(raw_data_y % 1000));

	strncat(x_buf, y_buf, strlen(y_buf));

	if (raw_data_z < 0)
		sprintf(z_buf, ",-%ld.%03ld", abs(raw_data_z / 1000), abs(raw_data_z % 1000));
	else if (raw_data_z >= 0)
		sprintf(z_buf, ",%ld.%03ld", abs(raw_data_z / 1000), abs(raw_data_z % 1000));

	strncat(x_buf, z_buf, strlen(z_buf));

	info("%s result_total =  %d, result_selftest = %d, result_offset = %d, "
		KERN_CONT "raw_data_x = %ld, raw_data_y = %ld, raw_data_z = %ld\n",
		__func__, result_total, result_selftest, result_offset, raw_data_x, raw_data_y, raw_data_z);

	return sprintf(buf, "%s\n", x_buf);
}
static DEVICE_ATTR_RO(selftest);

static struct attribute *is_sysfs_attr_entries_ois[] = {
	&dev_attr_autotest.attr,
	&dev_attr_calibrationtest.attr,
	&dev_attr_check_cross_talk.attr,
	&dev_attr_check_hall_cal.attr,
	&dev_attr_check_ois_valid.attr,
	&dev_attr_ois_center_shift.attr,
	&dev_attr_ois_exif.attr,
	&dev_attr_ois_ext_clk.attr,
	&dev_attr_ois_gain_rear.attr,
#ifdef CAMERA_2ND_OIS
	&dev_attr_ois_gain_rear3.attr,
#endif
#ifdef CAMERA_3RD_OIS
	&dev_attr_ois_gain_rear4.attr,
#endif
	&dev_attr_ois_hall_position.attr,
	&dev_attr_ois_noise_stdev.attr,
	&dev_attr_ois_power.attr,
	&dev_attr_ois_rawdata.attr,
	&dev_attr_ois_set_mode.attr,
	&dev_attr_ois_supperssion_ratio_rear.attr,
#ifdef CAMERA_2ND_OIS
	&dev_attr_ois_supperssion_ratio_rear3.attr,
#endif
#ifdef CAMERA_3RD_OIS
	&dev_attr_ois_supperssion_ratio_rear4.attr,
#endif
	&dev_attr_oisfw.attr,
	&dev_attr_prevent_shaking_noise.attr,
#ifdef CAMERA_2ND_OIS
	&dev_attr_rear3_read_cross_talk.attr,
#endif
	&dev_attr_reset_check.attr,
	&dev_attr_selftest.attr,
	NULL,
};

#if IS_ENABLED(CONFIG_PABLO_KUNIT_TEST)
struct attribute **pablo_vendor_sysfs_get_ois_attr_tbl(void)
{
	return is_sysfs_attr_entries_ois;
}
KUNIT_EXPORT_SYMBOL(pablo_vendor_sysfs_get_ois_attr_tbl);
#endif

static const struct attribute_group is_sysfs_attr_group_ois = {
	.attrs	= is_sysfs_attr_entries_ois,
};

static const struct attribute_group *is_sysfs_attr_groups_ois[] = {
	&is_sysfs_attr_group_ois,
	NULL,
};

void is_create_ois_sysfs(struct class *camera_class)
{
	camera_ois_dev = device_create_with_groups(camera_class, NULL, 2, NULL, is_sysfs_attr_groups_ois, "ois");

	if (IS_ERR(camera_ois_dev))
		pr_err("failed to create ois device!\n");
}

void is_destroy_ois_sysfs(struct class *camera_class)
{
	sysfs_remove_group(&camera_ois_dev->kobj, &is_sysfs_attr_group_ois);

	if (camera_class && camera_ois_dev)
		device_destroy(camera_class, camera_ois_dev->devt);
}
