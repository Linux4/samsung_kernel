/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef _CAMERA_OIS_MCU_H_
#define _CAMERA_OIS_MCU_H_

int ois_mcu_power_onoff(bool onoff);
int ois_mcu_update(void);
int ois_mcu_sysfs_gyro_cal(long *out_gyro_x, long *out_gyro_y);
int ois_mcu_sysfs_get_gyro_gain_from_eeprom(u32 *xgg, u32 *ygg);
int ois_mcu_sysfs_get_supperssion_ratio_from_eeprom(u32 *xsr, u32 *ysr);
int ois_mcu_sysfs_read_gyro_offset(long *out_gyro_x, long *out_gyro_y);
int ois_mcu_sysfs_read_gyro_offset_test(long *out_gyro_x, long *out_gyro_y);
int ois_mcu_sysfs_selftest(void);
int ois_mcu_sysfs_get_oisfw_version(char *ois_mcu_fw_ver);
int ois_mcu_sysfs_autotest(int *sin_x, int *sin_y, int *result_x, int *result_y, int threshold);
int ois_mcu_sysfs_set_mode(int mode);
int ois_mcu_sysfs_get_mcu_error(int *w_error);

#endif //_CAMERA_OIS_MCU_H_
