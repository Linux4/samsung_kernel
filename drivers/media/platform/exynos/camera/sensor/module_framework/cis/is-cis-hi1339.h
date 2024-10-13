/*
 * Samsung Exynos5 SoC series Sensor driver
 *
 *
 * Copyright (c) 2023 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef IS_CIS_HI1339_H
#define IS_CIS_HI1339_H

#include "is-cis.h"

#define EXT_CLK_Mhz (26)

#define SENSOR_HI1339_MAX_WIDTH                   (4208 + 0)
#define SENSOR_HI1339_MAX_HEIGHT                  (3120 + 0)

#define USE_GROUP_PARAM_HOLD                      (0)
#define NUM_OF_MIPI_LANES                   (4)

/* TODO: Check below values are valid */
#define SENSOR_HI1339_FINE_INTEGRATION_TIME_MIN           (0x0) //Not supported
#define SENSOR_HI1339_FINE_INTEGRATION_TIME_MAX           (0x0) //Not supported
#define SENSOR_HI1339_COARSE_INTEGRATION_TIME_MIN         (0x04)
#define SENSOR_HI1339_COARSE_INTEGRATION_TIME_MAX_MARGIN  (0x04)

#define SENSOR_HI1339_FRAME_LENGTH_LINE_ADDR      (0x020E)
#define SENSOR_HI1339_LINE_LENGTH_PCK_ADDR        (0x0206)
#define SENSOR_HI1339_GROUP_PARAM_HOLD_ADDR       (0x0208)
#define SENSOR_HI1339_COARSE_INTEG_TIME_ADDR      (0x020A)
#define SENSOR_HI1339_ANALOG_GAIN_ADDR            (0x0213) //8bit
#define SENSOR_HI1339_DIG_GAIN_ADDR               (0x0214) //Gr:0x214 ,Gb:0x216, R:0x218, B:0x21A
#define SENSOR_HI1339_MODEL_ID_ADDR               (0x0716)
#define SENSOR_HI1339_STREAM_ONOFF_ADDR           (0x0B00)
#define SENSOR_HI1339_ISP_ENABLE_ADDR             (0x0B04) //pdaf, mipi, fmt, Hscaler, D gain, DPC, LSC
#define SENSOR_HI1339_MIPI_TX_OP_MODE_ADDR        (0x1002)
#define SENSOR_HI1339_ISP_FRAME_CNT_ADDR          (0x1056)
#define SENSOR_HI1339_ISP_PLL_ENABLE_ADDR         (0x0702)

#define SENSOR_HI1339_MIN_ANALOG_GAIN_SET_VALUE   (0)      //x1.0
#define SENSOR_HI1339_MAX_ANALOG_GAIN_SET_VALUE   (0xF0)   //x16.0
#define SENSOR_HI1339_MIN_DIGITAL_GAIN_SET_VALUE  (0x0200) //x1.0
#define SENSOR_HI1339_MAX_DIGITAL_GAIN_SET_VALUE  (0x1FFB) //x15.99

/********* HI1339 OTP *********/
#define HI1339_OTP_BANK_SEL_ADDR	(0x073A)
#define IS_READ_MAX_HI1339_OTP_CAL_SIZE	(1692)
#define HI1339_OTP_START_ADDR_BANK1	(0x0740)
#define HI1339_OTP_START_ADDR_BANK2	(0x0DE0)
#define HI1339_OTP_START_ADDR_BANK3	(0x1480)

static const u32 sensor_otp_hi1339_global[] = {
	0x0790, 0x0100, 0x02,
	0x2000, 0x0000, 0x02,
	0x2002, 0x0058, 0x02,
	0x2006, 0x40B2, 0x02,
	0x2008, 0xB00E, 0x02,
	0x200A, 0x8450, 0x02,
	0x200C, 0x4130, 0x02,
	0x200E, 0x90F2, 0x02,
	0x2010, 0x0010, 0x02,
	0x2012, 0x0260, 0x02,
	0x2014, 0x2002, 0x02,
	0x2016, 0x1292, 0x02,
	0x2018, 0x84BC, 0x02,
	0x201A, 0x1292, 0x02,
	0x201C, 0xD020, 0x02,
	0x201E, 0x4130, 0x02,
	0x027E, 0x0100, 0x02,
};

//WB gain : Not Supported ??

struct setfile_info {
	const u32 *file;
	u32 file_size;
};

#define SET_SETFILE_INFO(s) {   \
	.file = s,                  \
	.file_size = ARRAY_SIZE(s), \
}

/*
enum hi1339_fsync_enum {
	HI1339_FSYNC_NORMAL,
	HI1339_FSYNC_SLAVE,
	HI1339_FSYNC_MASTER_FULL,
	HI1339_FSYNC_MASTER_2_BINNING,
	HI1339_FSYNC_MASTER_4_BINNING,
};
*/

enum sensor_hi1339_mode_enum {
	SENSOR_HI1339_4128X3096_30FPS = 0,
	SENSOR_HI1339_4128X2324_30FPS = 1,
	SENSOR_HI1339_3712X2556_30FPS = 2,
	SENSOR_HI1339_3408X2556_30FPS = 3,
	SENSOR_HI1339_1024X768_120FPS = 4,
	SENSOR_HI1339_2064x1548_30FPS = 5,
	SENSOR_HI1339_2064x1160_30FPS = 6,
	SENSOR_HI1339_1696x1272_30FPS = 7,
	SENSOR_HI1339_1856x1044_30FPS = 8,
	SENSOR_HI1339_2064X1548_15FPS = 9,
	SENSOR_HI1339_2064X1160_15FPS = 10,
	SENSOR_HI1339_1696X1272_15FPS = 11,
	SENSOR_HI1339_1856X1044_15FPS = 12,
	SENSOR_HI1339_2064X1548_7FPS = 13,
	SENSOR_HI1339_2064X1160_7FPS = 14,
	SENSOR_HI1339_1696x1272_7FPS = 15,
	SENSOR_HI1339_1856X1044_7FPS = 16,
	SENSOR_HI1339_3408X1916_30FPS = 17,
	SENSOR_HI1339_MODE_MAX,
};

struct sensor_hi1339_private_data {
	const struct sensor_regs global;
};

static const struct sensor_reg_addr sensor_hi1339_reg_addr = {
	.fll = 0x020E,
	.fll_shifter = 0, /* not supported */
	.cit = 0x020A,
	.cit_shifter = 0, /* not supported */
	.again = 0x0213, /* 8bit */
	.dgain = 0x0214, /* Gr:0x214 ,Gb:0x216, R:0x218, B:0x21A */
};
#endif
