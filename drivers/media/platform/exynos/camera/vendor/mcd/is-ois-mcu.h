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

#ifndef IS_OIS_MCU_H
#define IS_OIS_MCU_H

#define	IS_MCU_FW_NAME		"is_mcu_fw.bin"
#define	IS_MCU_PATH		"/system/vendor/firmware/"
#ifdef CONFIG_SAMSUNG_PRODUCT_SHIP
#define IS_MCU_SDCARD_PATH	NULL
#else
#define IS_MCU_SDCARD_PATH	"/vendor/firmware/"
#endif
#define	OIS_GYRO_SCALE_FACTOR_LSM6DSO	114
#define	FW_DRIVER_IC			0
#define	FW_GYRO_SENSOR			1
#define	FW_MODULE_TYPE		2
#define	FW_PROJECT				3
#define	FW_CORE_VERSION		4
#define	OIS_BIN_LEN				45056

enum is_ois_power_mode {
	OIS_POWER_MODE_SINGLE = 0,
	OIS_POWER_MODE_DUAL,
};

struct mcu_default_data {
	u32 ois_gyro_list[5];
	u32 ois_gyro_list_len;
	u32 aperture_delay_list[2];
	u32 aperture_delay_list_len;
};

enum ois_mcu_state {
	OM_HW_NONE,
	OM_HW_FW_LOADED,
	OM_HW_RUN,
	OM_HW_SUSPENDED,
	OM_HW_END
};

enum ois_mcu_base_reg_index {
	OM_REG_CORE = 0,
	OM_REG_PERI1 = 1,
	OM_REG_PERI2 = 2,
	OM_REG_MAX
};

struct ois_mcu_dev {
	struct platform_device	*pdev;
	struct device		*dev;
	struct clk		*clk;
	int			irq;
	void __iomem		*regs[OM_REG_MAX];
	resource_size_t		regs_start[OM_REG_MAX];
	resource_size_t		regs_end[OM_REG_MAX];

	unsigned long		state;
};

/*
 * APIs
 */
int ois_mcu_power_ctrl(struct ois_mcu_dev *mcu, int on);
int ois_mcu_load_binary(struct ois_mcu_dev *mcu);
int ois_mcu_core_ctrl(struct ois_mcu_dev *mcu, int on);
int ois_mcu_dump(struct ois_mcu_dev *mcu, int type);

/*
 * log
 */
extern int debug_ois_mcu;

#define __mcu_err(fmt, ...)	pr_err(fmt, ##__VA_ARGS__)
#define __mcu_warning(fmt, ...)	pr_warn(fmt, ##__VA_ARGS__)
#define __mcu_log(fmt, ...)	pr_info(fmt, ##__VA_ARGS__)

#define err_mcu(fmt, args...) \
	__mcu_err("[@][MCU]" fmt, ##args)

#define warning_mcu(fmt, args...) \
	__mcu_warning("[@][MCU]" fmt, ##args)

#define info_mcu(fmt, args...) \
	__mcu_log("[@][MCU]" fmt, ##args)

#define dbg_mcu(level, fmt, args...)			\
	do {						\
		if (unlikely(debug_ois_mcu >= level))	\
			__mcu_log("[@][MCU]" fmt, ##args);	\
	} while (0)

#endif
