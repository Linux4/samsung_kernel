/*
 * Samsung Exynos SoC series Sensor driver
 *
 *
 * Copyright (c) 2022 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef IS_CIS_HM6_H
#define IS_CIS_HM6_H

#include "is-cis.h"

#define SENSOR_HM6_FINE_INTEGRATION_TIME_MIN	0x100
#define SENSOR_HM6_FINE_INTEGRATION_TIME_MAX	0x100

/* Related EEPROM CAL */
#define SENSOR_HM6_UXTC_FULL_SENS_CAL_ADDR		(0x01D0)
#define SENSOR_HM6_UXTC_PARTIAL_SENS_CAL_ADDR		(0x01D0 + 0x173C)
#define SENSOR_HM6_UXTC_FULL_SENS_CAL_SIZE		(6172)
#define SENSOR_HM6_UXTC_PARTIAL_SENS_CAL_SIZE		(224)

#define SENSOR_HM6_UXTC_DUMP_NAME		"/data/vendor/camera/HM6_UXTC_DUMP.bin"

#define HM6_BURST_WRITE

enum sensor_hm6_mode_enum {
	SENSOR_HM6_4000X3000_30FPS_9SUM = 0,
	SENSOR_HM6_4000X2252_30FPS_9SUM,
	SENSOR_HM6_4000X2252_60FPS_9SUM,
	SENSOR_HM6_1280X720_240FPS_SM,
	SENSOR_HM6_2000X1500_120FPS_FASTAE,
	/* FULL Remosaic */
	SENSOR_HM6_12000X9000_8FPS,
	SENSOR_HM6_6000X4500_18FPS_REMOSAIC_LN2,
	SENSOR_HM6_6000X3376_24FPS,
	SENSOR_HM6_4000x3000_16FPS_REMOSAIC_LN2,
	SENSOR_HM6_4000x2252_34FPS_REMOSAIC_LN2,
	SENSOR_HM6_MODE_MAX
};

struct sensor_hm6_private_data {
	const struct sensor_regs global;
	const struct sensor_regs xtc_prefix;
	const struct sensor_regs xtc_fw_testvector_golden;
	const struct sensor_regs xtc_postfix;
	const struct sensor_regs fastaeTo9sum;
};

static const struct sensor_reg_addr sensor_hm6_reg_addr = {
	.fll = 0x0340,
	.fll_shifter = 0x0702,
	.cit = 0x0202,
	.cit_shifter = 0x0704,
	.again = 0x0204,
	.dgain = 0x020E,
	.group_param_hold = 0x0104,
};

enum hm6_endian {
	HM6_LITTLE_ENDIAN = 0,
	HM6_BIG_ENDIAN = 1,
};

#define HM6_ENDIAN(a, b, endian)  ((endian == HM6_BIG_ENDIAN) ? ((a << 8)|(b)) : ((a)|(b << 8)))

#endif
