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

#ifndef IS_VENDOR_OIS_INTERNAL_MCU_H
#define IS_VENDOR_OIS_INTERNAL_MCU_H

#include <media/v4l2-subdev.h>
#include "is-core.h"
#include "is-interface-sensor.h"
#include "is-vendor-ois.h"

#define	IS_MCU_FW_NAME		"is_mcu_fw.bin"
#define	IS_MCU_PATH		"/system/vendor/firmware/"

#define	MCU_AF_MODE_STANDBY			0x40
#ifdef USE_TELE2_OIS_AF_COMMON_INTERFACE
#define	MCU_AF_MODE_ACTIVE			0x00
#define	MCU_ACT_DEFAULT_FIRST_POSITION		2048
#endif
#define	MCU_AF_INIT_POSITION		0x7F
#if defined(USE_TELE_OIS_AF_COMMON_INTERFACE) || defined(USE_TELE2_OIS_AF_COMMON_INTERFACE)
#define	MCU_ACT_POS_SIZE_BIT		ACTUATOR_POS_SIZE_12BIT
#define	MCU_ACT_POS_MAX_SIZE		((1 << MCU_ACT_POS_SIZE_BIT) - 1)
#define	MCU_ACT_POS_DIRECTION		ACTUATOR_RANGE_INF_TO_MAC
#endif
#ifndef OIS_DUAL_CAL_DEFAULT_VALUE_TELE
#define	MCU_HALL_SHIFT_ADDR_X_M2	0x02AA
#define	MCU_HALL_SHIFT_ADDR_Y_M2	0x02AC
#endif
#if IS_ENABLED(CONFIG_CAMERA_HW_BIG_DATA)
#define	MCU_I2C_ERR_VAL				0x7E00
#define	MCU_REAR_OIS_ERR_REG		0x0600
#define	MCU_REAR_2ND_OIS_ERR_REG	0x1800
#define	MCU_REAR_3RD_OIS_ERR_REG	0x6000
#endif
#define	MCU_BYPASS_MODE_WRITE_ID	0x48
#define	MCU_BYPASS_MODE_READ_ID	0x49
#define	MCU_HW_VERSION_OFFSET		0x7C
#define	MCU_BIN_VERSION_OFFSET		0xF8
#define	MCU_SHARED_SRC_ON_COUNT		1
#define	MCU_SHARED_SRC_OFF_COUNT		0

#define MCU_OIS_GYRO_DIRECTION_MAX	8

enum is_ois_power_mode {
	OIS_POWER_MODE_NONE = 0,
	OIS_POWER_MODE_SINGLE_WIDE,
	OIS_POWER_MODE_SINGLE_TELE,
	OIS_POWER_MODE_SINGLE_TELE2,
	OIS_POWER_MODE_DUAL,
	OIS_POWER_MODE_TRIPLE,
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
	OM_REG_PERI_SETTING = 3,
	OM_REG_SFR = 4,
	OM_REG_MAX
};

enum ois_mcu_uw_mode {
	OIS_USE_UW_NONE = 0,
	OIS_USE_UW_ONLY,
	OIS_USE_UW_WIDE,
};

struct ois_mcu_dev {
	struct platform_device	*pdev;
	struct device		*dev;
	struct clk		*clk;
	struct clk		*spi_clk;
	struct mutex 	power_mutex;
	int			irq;
	void __iomem		*regs[OM_REG_MAX];
	resource_size_t		regs_start[OM_REG_MAX];
	resource_size_t		regs_end[OM_REG_MAX];

	int ois_gyro_direction[MCU_OIS_GYRO_DIRECTION_MAX];

	unsigned long		state;
	atomic_t 		shared_rsc_count;
	int			current_rsc_count;
	int			current_power_mode;
	u16			current_error_reg;
	bool			dev_ctrl_state;
	bool			need_reset_mcu;
	bool			need_af_delay;
	bool			is_mcu_active;
	bool			ois_wide_init;
	bool			ois_tele_init;
	bool			ois_tele2_init;
	bool			ois_hw_check;
	bool			ois_fadeupdown;
};

enum is_efs_state {
	IS_EFS_STATE_READ,
};

struct mcu_efs_info {
	unsigned long	efs_state;
	s16 ois_hall_shift_x;
	s16 ois_hall_shift_y;
};

/*
 * APIs
 */
int is_vendor_ois_power_ctrl(struct ois_mcu_dev *mcu, int on);
int is_vendor_ois_load_binary(struct ois_mcu_dev *mcu);
int is_vendor_ois_core_ctrl(struct ois_mcu_dev *mcu, int on);
int is_vendor_ois_dump(struct ois_mcu_dev *mcu, int type);
void is_vendor_ois_device_ctrl(struct ois_mcu_dev *mcu, u8 value);
int is_vendor_ois_init(struct v4l2_subdev *subdev);
int is_vendor_ois_init_factory(struct v4l2_subdev *subdev);
#if defined(CAMERA_3RD_OIS)
void is_vendor_ois_init_rear2(struct is_core *core);
#endif
int is_vendor_ois_deinit(struct v4l2_subdev *subdev);
#ifdef USE_TELE2_OIS_AF_COMMON_INTERFACE
int is_vendor_ois_af_set_active(struct v4l2_subdev *subdev, int enable);
#endif
#if defined(CAMERA_2ND_OIS)
int is_vendor_ois_set_power_mode(struct v4l2_subdev *subdev, int forceMode);
#endif
int is_vendor_ois_set_dev_ctrl(struct v4l2_subdev *subdev, int forceMode);
int is_vendor_ois_check_cross_talk(struct v4l2_subdev *subdev, u16 *hall_data);
int is_vendor_ois_check_hall_cal(struct v4l2_subdev *subdev, u16 *hall_cal_data);
int is_vendor_ois_read_ext_clock(struct v4l2_subdev *subdev, u32 *clock);
int is_vendor_ois_get_hall_data(struct v4l2_subdev *subdev, struct is_ois_hall_data *halldata);
bool is_vendor_ois_check_fw(struct is_core *core);

struct platform_driver *get_internal_ois_platform_driver(void);
#endif
