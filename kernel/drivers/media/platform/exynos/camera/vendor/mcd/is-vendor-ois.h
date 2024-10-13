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

struct mcu_default_data {
	u32 ois_gyro_direction[5];
	u32 ois_gyro_direction_len;
};

/*
 * APIs
 */
long is_vendor_ois_get_efs_data(struct ois_mcu_dev *mcu, long *raw_data_x, long *raw_data_y, long *raw_data_z);
void is_vendor_ois_get_ops(struct is_ois_ops **ois_ops);

/*
 * log
 */
#define __mcu_err(fmt, ...)	pr_err(fmt, ##__VA_ARGS__)
#define __mcu_warning(fmt, ...)	pr_warn(fmt, ##__VA_ARGS__)
#define __mcu_log(fmt, ...)	pr_info(fmt, ##__VA_ARGS__)

#define err_mcu(fmt, args...) \
	__mcu_err("[@][OIS_MCU]" fmt, ##args)

#define warning_mcu(fmt, args...) \
	__mcu_warning("[@][OIS_MCU]" fmt, ##args)

#define info_mcu(fmt, args...) \
	__mcu_log("[@][OIS_MCU]" fmt, ##args)

#endif
