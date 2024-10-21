/*
 * Samsung Exynos SoC series Sensor driver
 *
 *
 * Copyright (c) 2023 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef IS_CIS_HI847_H
#define IS_CIS_HI847_H

#include "is-cis.h"

/********* HI847 OTP *********/
#define IS_READ_MAX_HI847_OTP_CAL_SIZE	(6912)
#define HI847_OTP_BANK_SEL_ADDR	(0x0700)
#define HI847_OTP_START_ADDR_BANK1	(0x0704)
#define HI847_OTP_START_ADDR_BANK2	(0x0D04)
#define HI847_OTP_START_ADDR_BANK3	(0x1304)
#define HI847_OTP_START_ADDR_BANK4	(0x1904)
#define HI847_OTP_BANK_DATA_ADDR		(0x0308)

#define EXT_CLK_Mhz	(26)

#define SENSOR_HI847_MAX_WIDTH	(3264)
#define SENSOR_HI847_MAX_HEIGHT	(2448)

#define USE_GROUP_PARAM_HOLD	(0)

#define SENSOR_HI847_FINE_INTEGRATION_TIME_MIN			(0x0)	//Not Support
#define SENSOR_HI847_FINE_INTEGRATION_TIME_MAX			(0x0)	//Not Support
#define SENSOR_HI847_COARSE_INTEGRATION_TIME_MIN		(4)
#define SENSOR_HI847_COARSE_INTEGRATION_TIME_MAX_MARGIN	(4)

#define SENSOR_HI847_MIN_ANALOG_GAIN_SET_VALUE			(0)		//x1.0
#define SENSOR_HI847_MAX_ANALOG_GAIN_SET_VALUE			(0xF0)	//x16.0
#define SENSOR_HI847_MIN_DIGITAL_GAIN_SET_VALUE			(0x200)	//x1.0
#define SENSOR_HI847_MAX_DIGITAL_GAIN_SET_VALUE			(0x1FFB)	//x15.99

#define SENSOR_HI847_GROUP_PARAM_HOLD_ADDR		(0x0208)
#define SENSOR_HI847_COARSE_INTEG_TIME_ADDR		(0x020A)
#define SENSOR_HI847_FRAME_LENGTH_LINE_ADDR		(0x020E)
#define SENSOR_HI847_LINE_LENGTH_PCK_ADDR		(0x0206)
#define SENSOR_HI847_ANALOG_GAIN_ADDR			(0x0213)
#define SENSOR_HI847_DIGITAL_GAIN_ADDR			(0x0214)
#define SENSOR_HI847_DIGITAL_GAIN_GR_ADDR		(0x0214)
#define SENSOR_HI847_DIGITAL_GAIN_GB_ADDR		(0x0216)
#define SENSOR_HI847_DIGITAL_GAIN_R_ADDR			(0x0218)
#define SENSOR_HI847_DIGITAL_GAIN_B_ADDR			(0x021A)

#define SENSOR_HI847_MODEL_ID_ADDR				(0x0716)
#define SENSOR_HI847_STREAM_MODE_ADDR			(0x0B00)
#define SENSOR_HI847_ISP_EN_ADDR					(0x0B04)	// B[8]: PCM, B[5]: Hscaler, B[4]: Digital gain, B[3]: DPC, B[1]: LSC, B[0]: TestPattern
#define SENSOR_HI847_MIPI_TX_OP_MODE_ADDR		(0x1002)
#define SENSOR_HI847_ISP_PLL_ENABLE_ADDR		(0x0702)

#define SENSOR_HI847_DEBUG_INFO (1)
#define SENSOR_HI847_PDAF_DISABLE (0)
#define SENSOR_HI847_OTP_READ (0)

static const u32 sensor_otp_hi847_global[] = {
	0x0B00, 0x0000, 0x02,
	0x027E, 0x0000, 0x02,
	0x0700, 0x0117, 0x02,
	0x0700, 0x0017, 0x02,
	0x0790, 0x0100, 0x02,
	0x2000, 0x0001, 0x02,
	0x2002, 0x0058, 0x02,
	0x2006, 0x1292, 0x02,
	0x2008, 0x8446, 0x02,
	0x200A, 0x90F2, 0x02,
	0x200C, 0x0010, 0x02,
	0x200E, 0x0260, 0x02,
	0x2010, 0x23FC, 0x02,
	0x2012, 0x1292, 0x02,
	0x2014, 0x84BC, 0x02,
	0x2016, 0x3FF9, 0x02,
	0x2018, 0x4130, 0x02,
	0x0708, 0xEF82, 0x02,
	0x070C, 0x0000, 0x02,
	0x0732, 0x0300, 0x02,
	0x0734, 0x0300, 0x02,
	0x0736, 0x0064, 0x02,
	0x0738, 0x0003, 0x02,
	0x0742, 0x0300, 0x02,
	0x0746, 0x00FA, 0x02,
	0x0748, 0x0003, 0x02,
	0x074C, 0x0000, 0x02,
	0x0266, 0x0000, 0x02,
	0x0360, 0x2C8E, 0x02,
	0x027E, 0x0100, 0x02,
	0x0B00, 0x0000, 0x02,
};

enum sensor_hi847_mode_enum {
	SENSOR_HI847_3264X2448_30FPS,
	SENSOR_HI847_3264X1836_30FPS,
	SENSOR_HI847_1632X1224_30FPS,
	SENSOR_HI847_1632X1224_60FPS,
	SENSOR_HI847_1632X916_30FPS,
	SENSOR_HI847_MODE_MAX
};

const u32 sensor_hi847_rev00_mipirate[] = {
	958,		//SENSOR_HI847_3264X2448_30FPS
	910,		//SENSOR_HI847_3264X1836_30FPS, REV00 is not supported.
	455,		//SENSOR_HI847_1632X1224_30FPS, REV00 is not supported.
	400,		//SENSOR_HI847_1632X1224_60FPS
};

struct sensor_hi847_private_data {
	const struct sensor_regs global;
};

static const struct sensor_reg_addr sensor_hi847_reg_addr = {
	.fll = 0x020E,
	.fll_shifter = 0, /* not supported */
	.cit = 0x020A,
	.cit_shifter = 0, /* not supported */
	.again = 0x0213, /* 8bit */
	.dgain = 0x0214, /* Gr:0x214 ,Gb:0x216, R:0x218, B:0x21A */
};
#endif
