/*
 * Samsung Exynos5 SoC series Sensor driver
 *
 *
 * Copyright (c) 2022 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef IS_CIS_HI1337_H
#define IS_CIS_HI1337_H

#include "is-cis.h"

/********* HI1337 OTP *********/
#define IS_READ_MAX_HI1337_OTP_CAL_SIZE	(6912)
#define HI1337_OTP_BANK_SEL_ADDR	(0x0700)
#define HI1337_OTP_START_ADDR_BANK1	(0x0704)
#define HI1337_OTP_START_ADDR_BANK2	(0x0D04)
#define HI1337_OTP_START_ADDR_BANK3	(0x1304)
#define HI1337_OTP_START_ADDR_BANK4	(0x1904)

#define USE_GROUP_PARAM_HOLD                      (0)

#define SENSOR_HI1337_GROUP_PARAM_HOLD_ADDR       (0x0208)
#define SENSOR_HI1337_COARSE_INTEG_TIME_ADDR      (0x020A)

#define SENSOR_HI1337_ANALOG_GAIN_ADDR            (0x0213) //8bit
#define SENSOR_HI1337_DIG_GAIN_ADDR               (0x0214) //Gr:0x214 ,Gb:0x216, R:0x218, B:0x21A
#define SENSOR_HI1337_MODEL_ID_ADDR               (0x0716)
#define SENSOR_HI1337_STREAM_ONOFF_ADDR           (0x0B00)
#define SENSOR_HI1337_ISP_ENABLE_ADDR             (0x0B04) //pdaf, mipi, fmt, Hscaler, D gain, DPC, LSC
#define SENSOR_HI1337_MIPI_TX_OP_MODE_ADDR        (0x1002)
#define SENSOR_HI1337_ISP_FRAME_CNT_ADDR          (0x1056)
#define SENSOR_HI1337_ISP_PLL_ENABLE_ADDR         (0x0702)

static const u32 sensor_otp_hi1337_global[] = {
	0x0B00, 0x0000, 0x02,
	0x027E, 0x0000, 0x02,
	0x0700, 0x0117, 0x02,
	0x0700, 0x0017, 0x02,
	0x0790, 0x0100, 0x02,
	0x2000, 0x0000, 0x02,
	0x2002, 0x0058, 0x02,
	0x2006, 0x4130, 0x02,
	0x2008, 0x403D, 0x02,
	0x200A, 0x004D, 0x02,
	0x200C, 0x403E, 0x02,
	0x200E, 0xD000, 0x02,
	0x2010, 0x403F, 0x02,
	0x2012, 0x8430, 0x02,
	0x2014, 0x12B0, 0x02,
	0x2016, 0xD6F4, 0x02,
	0x2018, 0x1292, 0x02,
	0x201A, 0x84BE, 0x02,
	0x201C, 0x40B2, 0x02,
	0x201E, 0xFFFF, 0x02,
	0x2020, 0x8708, 0x02,
	0x2022, 0x93C2, 0x02,
	0x2024, 0x0263, 0x02,
	0x2026, 0x2005, 0x02,
	0x2028, 0xB3E2, 0x02,
	0x202A, 0x0360, 0x02,
	0x202C, 0x2402, 0x02,
	0x202E, 0x1292, 0x02,
	0x2030, 0x84A0, 0x02,
	0x2032, 0x1292, 0x02,
	0x2034, 0x8446, 0x02,
	0x2036, 0x40B2, 0x02,
	0x2038, 0xF518, 0x02,
	0x203A, 0x8494, 0x02,
	0x203C, 0x90F2, 0x02,
	0x203E, 0x0010, 0x02,
	0x2040, 0x0260, 0x02,
	0x2042, 0x23FC, 0x02,
	0x2044, 0x1292, 0x02,
	0x2046, 0x84BC, 0x02,
	0x2048, 0x3FF9, 0x02,
	0x204A, 0x4130, 0x02,
	0x204C, 0xB008, 0x02,
	0x036A, 0xB008, 0x02,
	0x0708, 0xEF82, 0x02,
	0x070C, 0x0000, 0x02,
	0x0732, 0x0000, 0x02,
	0x0734, 0x0300, 0x02,
	0x0736, 0x0064, 0x02,
	0x0738, 0x0003, 0x02,
	0x0266, 0x0000, 0x02,
	0x0360, 0x2C8E, 0x02,
	0x027E, 0x0100, 0x02,
	0x0B00, 0x0000, 0x02,
};

struct setfile_info {
	const u32 *file;
	u32 file_size;
};

#define SET_SETFILE_INFO(s) {   \
	.file = s,                  \
	.file_size = ARRAY_SIZE(s), \
}

enum hi1337_fsync_enum {
	HI1337_FSYNC_NORMAL,
	HI1337_FSYNC_SLAVE,
	HI1337_FSYNC_MASTER,
};

enum sensor_hi1337_mode_enum {
	SENSOR_HI1337_4000x3000_30fps = 0,
	SENSOR_HI1337_4000x2256_30fps,
	SENSOR_HI1337_1000x748_120fps,
	SENSOR_HI1337_2000x1500_30fps,
	SENSOR_HI1337_4128x3096_30fps,
	SENSOR_HI1337_3408x2556_30fps,
	SENSOR_HI1337_3344x1880_30fps,
	SENSOR_HI1337_3200x2400_30fps,
	SENSOR_HI1337_3172x2580_30fps,
	SENSOR_HI1337_3172x2040_30fps,
	SENSOR_HI1337_4208x3120_30fps,
	SENSOR_HI1337_4128x2320_30fps,
	SENSOR_HI1337_2032x1524_30fps,
	SENSOR_HI1337_2560x1920_30fps,
	SENSOR_HI1337_2560x1440_30fps,
	SENSOR_HI1337_2560x1600_30fps,
	SENSOR_HI1337_1920x1920_30fps,
	SENSOR_HI1337_992x744_120fps,
	SENSOR_HI1337_2000x1124_30fps,
	SENSOR_HI1337_1280x960_30fps,
	SENSOR_HI1337_1280x720_30fps,
	SENSOR_HI1337_2368x1776_30fps,
	SENSOR_HI1337_2368x1332_30fps,
	SENSOR_HI1337_2368x1480_30fps,
	SENSOR_HI1337_1776x1776_30fps,
	SENSOR_HI1337_1016x762_30fps,
	SENSOR_HI1337_MODE_MAX,
};

struct sensor_hi1337_private_data {
	const struct sensor_regs global;
};

static const struct sensor_reg_addr sensor_hi1337_reg_addr = {
	.fll = 0x020E,
	.fll_shifter = 0, /* not supported */
	.cit = 0x020A,
	.cit_shifter = 0, /* not supported */
	.again = 0x0213, /* 8bit */
	.dgain = 0x0214, /* Gr:0x214 ,Gb:0x216, R:0x218, B:0x21A */
};
#endif
