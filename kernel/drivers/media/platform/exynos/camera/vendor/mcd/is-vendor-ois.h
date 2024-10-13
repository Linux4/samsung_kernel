/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef IS_VENDOR_OIS_H
#define IS_VENDOR_OIS_H

#define	GYRO_CAL_VALUE_FROM_EFS	"/efs/FactoryApp/camera_ois_gyro_cal"
#define	MAX_GYRO_EFS_DATA_LENGTH	30
#define	MIN_AF_POSITION	1

#if defined(CONFIG_CAMERA_USE_INTERNAL_MCU)
struct mcu_default_data {
	u32 ois_gyro_direction[5];
	u32 ois_gyro_direction_len;
};
#else
struct mcu_default_data {
	u32 ois_gyro_direction[5];
	u32 ois_gyro_direction_len;
	u32 aperture_delay_list[2];
	u32 aperture_delay_list_len;
};
#endif

int is_ois_read_u8(int cmd, u8 *data);
int is_ois_read_u16(int cmd, u8 *data);
int is_ois_read_multi(int cmd, u8 *data, size_t size);
int is_ois_write_u8(int cmd, u8 data);
int is_ois_write_u16(int cmd, u8 *data);
int is_ois_write_multi(int cmd, u8 *data, size_t size);

/*
 * APIs
 */
long is_vendor_ois_get_efs_data(struct ois_mcu_dev *mcu, long *raw_data_x, long *raw_data_y, long *raw_data_z);
void is_vendor_ois_get_ops(struct is_ois_ops **ois_ops);

/*
 * log
 */
#define err_mcu(fmt, args...) \
	pr_err("[@][OIS_MCU]%s:%d:" fmt "\n", __func__, __LINE__, ##args)

#define warning_mcu(fmt, args...) \
	pr_warn("[@][OIS_MCU]%s:%d:" fmt "\n", __func__, __LINE__, ##args)

#define info_mcu(fmt, args...) \
	pr_info("[@][OIS_MCU]" fmt, ##args)

#define dbg_mcu(fmt, args...) \
	pr_debug("[@][OIS_MCU]" fmt, ##args)

#define MCU_ERR_PRINT(fmt, args...) \
	pr_err("[@][OIS_MCU]%s:%d:" fmt "\n", __func__, __LINE__, ##args)

#define MCU_GET_ERR_PRINT(idx) \
	MCU_ERR_PRINT("%s: get fail (%s:%X)", __func__, ois_mcu_regs[idx].reg_name, ois_mcu_regs[idx].sfr_offset)
#define MCU_SET_ERR_PRINT(idx) \
	MCU_ERR_PRINT("%s: set fail (%s:%X)", __func__, ois_mcu_regs[idx].reg_name, ois_mcu_regs[idx].sfr_offset)
#endif
