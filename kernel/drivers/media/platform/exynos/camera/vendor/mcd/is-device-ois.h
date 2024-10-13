/*
 * Samsung Exynos5 SoC series FIMC-IS OIS driver
 *
 * exynos5 fimc-is core functions
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef IS_DEVICE_OIS_H
#define IS_DEVICE_OIS_H

#include "is-interface-sensor.h"

struct is_ois_exif {
	int error_data;
	int status_data;
};

struct ois_rom_data {
	char xgg[5];
	char ygg[5];
	char xcoef[3];
	char ycoef[3];
	char supperssion_xratio[3];
	char supperssion_yratio[3];
	u8 cal_mark[2];
};

struct is_ois_info {
	char header_ver[9];
	struct ois_rom_data wide_romdata;
#ifdef CAMERA_2ND_OIS
	struct ois_rom_data tele_romdata;
#endif
#ifdef CAMERA_3RD_OIS
	struct ois_rom_data tele2_romdata;
#endif
#ifdef CAMERA_2ND_OIS
	struct ois_rom_data tele_tilt_romdata;
#endif
	u8 checksum;
	u8 caldata;
	bool reset_check;
};

#define FIMC_OIS_FW_NAME_SEC		"ois_fw_sec.bin"
#define FIMC_OIS_FW_NAME_SEC_2		"ois_fw_sec_2.bin"
#define FIMC_OIS_FW_NAME_DOM		"ois_fw_dom.bin"
#define IS_OIS_SDCARD_PATH		"/data/vendor/camera/"

bool is_ois_offset_test(struct is_core *core, long *raw_data_x, long *raw_data_y, long *raw_data_z);
int is_ois_self_test(struct is_core *core);
int is_ois_gpio_on(struct is_core *core);
int is_ois_gpio_off(struct is_core *core);
int is_ois_get_module_version(struct is_ois_info **minfo);
int is_ois_get_phone_version(struct is_ois_info **minfo);
int is_ois_get_user_version(struct is_ois_info **uinfo);
int is_ois_get_exif_data(struct is_ois_exif **exif_info);
bool is_ois_check_fw(struct is_core *core);
bool is_ois_auto_test(struct is_core *core,
				int threshold, bool *x_result, bool *y_result, int *sin_x, int *sin_y,
				bool *x_result_2nd, bool *y_result_2nd, int *sin_x_2nd, int *sin_y_2nd,
				bool *x_result_3rd, bool *y_result_3rd, int *sin_x_3rd, int *sin_y_3rd);
#ifdef CAMERA_2ND_OIS
bool is_ois_auto_test_rear2(struct is_core *core,
				int threshold, bool *x_result, bool *y_result, int *sin_x, int *sin_y,
				bool *x_result_2nd, bool *y_result_2nd, int *sin_x_2nd, int *sin_y_2nd);
#endif
void is_ois_gyro_sleep(struct is_core *core);
bool is_ois_gyrocal_test(struct is_core *core, long *raw_data_x, long *raw_data_y, long *raw_data_z);
bool is_ois_gyronoise_test(struct is_core *core, long *raw_data_x, long *raw_data_y);
void is_ois_get_hall_pos(struct is_core *core, u16 *targetPos, u16 *hallPos);
void is_ois_set_mode(struct is_core *core, int mode);
void is_ois_check_cross_talk(struct is_core *core, u16 *hall_data);
void is_ois_check_hall_cal(struct is_core *core, u16 *hall_cal_data);
void is_ois_check_valid(struct is_core *core, u8 *value);
int is_ois_read_ext_clock(struct is_core *core, u32 *clock);
#if defined(CAMERA_3RD_OIS)
void is_ois_init_rear2(struct is_core *core);
#endif
void is_ois_init_factory(struct is_core *core);
void is_ois_parsing_raw_data(struct is_core *core, uint8_t *buf, long efs_size, long *raw_data_x, long *raw_data_y, long *raw_data_z);
int is_ois_control_gpio(struct is_core *core, int position, int onoff);
void is_ois_get_hall_data(struct is_core *core, struct is_ois_hall_data *halldata);
void is_ois_set_center_shift(struct is_core *core, int16_t *value);
#endif
