/*
 * Samsung Exynos5 SoC series IS driver
 *
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef IS_SEC_DEFINE_H
#define IS_SEC_DEFINE_H

#include <linux/module.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <asm/cacheflush.h>
#include <asm/pgtable.h>
#include <linux/firmware.h>
#include <linux/dma-mapping.h>
#include <linux/scatterlist.h>
#include <linux/videodev2.h>
#include <videodev2_exynos_camera.h>
#include <linux/v4l2-mediabus.h>
#include <linux/bug.h>
#include <linux/syscalls.h>
#include <linux/vmalloc.h>
#include <linux/firmware.h>
#include <linux/gpio.h>
#include <linux/regulator/consumer.h>
#include <linux/zlib.h>

#include "is-core.h"
#include "is-cmd.h"
#include "is-err.h"
#include "is-video.h"

#include "is-device-sensor.h"
#include "is-device-ischain.h"
#include "crc32.h"
#include "is-device-from.h"
#include "is-dt.h"
#include "is-device-ois.h"

#define FW_CORE_VER		0
#define FW_PIXEL_SIZE		1
#define FW_ISP_COMPANY		3
#define FW_SENSOR_MAKER		4
#define FW_PUB_YEAR		5
#define FW_PUB_MON		6
#define FW_PUB_NUM		7
#define FW_MODULE_COMPANY	9
#define FW_VERSION_INFO		10

#define IS_DDK				"is_lib.bin"
#define IS_RTA				"is_rta.bin"

#define IS_HEADER_VER_SIZE	  11
#define IS_HEADER_VER_OFFSET	(IS_HEADER_VER_SIZE + IS_SIGNATURE_LEN)
#define IS_CAM_VER_SIZE		 11
#define IS_OEM_VER_SIZE		 11
#define IS_AWB_VER_SIZE		 11
#define IS_SHADING_VER_SIZE	 11
#define IS_CAL_MAP_VER_SIZE	 4
#define IS_PROJECT_NAME_SIZE	8
#define IS_ISP_SETFILE_VER_SIZE 6
#define IS_SENSOR_ID_SIZE	   16
#define IS_CAL_DLL_VERSION_SIZE 4
#define IS_MODULE_ID_SIZE	   10
#define IS_RESOLUTION_DATA_SIZE 54
#define IS_AWB_MASTER_DATA_SIZE 8
#define IS_AWB_MODULE_DATA_SIZE 8
#define IS_OIS_GYRO_DATA_SIZE 4
#define IS_OIS_COEF_DATA_SIZE 2
#define IS_OIS_SUPPERSSION_RATIO_DATA_SIZE 2
#define IS_OIS_CAL_MARK_DATA_SIZE 1

#define IS_PAF_OFFSET_MID_OFFSET		(0x0730) /* REAR PAF OFFSET MID (30CM, WIDE) */
#define IS_PAF_OFFSET_MID_SIZE			936
#define IS_PAF_OFFSET_PAN_OFFSET		(0x0CD0) /* REAR PAF OFFSET FAR (1M, WIDE) */
#define IS_PAF_OFFSET_PAN_SIZE			234
#define IS_PAF_CAL_ERR_CHECK_OFFSET	0x14

#define IS_ROM_CRC_MAX_LIST 150
#define IS_ROM_DUAL_TILT_MAX_LIST 10
#define IS_DUAL_TILT_PROJECT_NAME_SIZE 20
#define IS_ROM_OIS_MAX_LIST 14
#define IS_ROM_OIS_SINGLE_MODULE_MAX_LIST 7

enum i2c_write_format {
	I2C_WRITE_FORMAT_ADDR8_DATA8 = 0x0,
	I2C_WRITE_FORMAT_ADDR16_DATA8,
	I2C_WRITE_FORMAT_ADDR16_DATA16
};


/*********HI556 OTP*********/
#define HI556_OTP_ACCESS_ADDR_HIGH							0x010A
#define HI556_OTP_ACCESS_ADDR_LOW							0x010B
#define HI556_OTP_MODE_ADDR									0x0102
#define HI556_OTP_READ_ADDR									0x0108
#define HI556_OTP_USED_CAL_SIZE								(0x098F - 0x0404 + 0x1)
#define HI556_BANK_SELECT_ADDR								0x0400
#define HI556_OTP_START_ADDR_BANK1							0x0404
#define HI556_OTP_START_ADDR_BANK2							0x0994
#define HI556_OTP_START_ADDR_BANK3							0x0F24
#define HI556_OTP_START_ADDR_BANK4							0x14B4
#define HI556_OTP_START_ADDR_BANK5							0x1A44

static const u32 otp_read_initial_setting_hi556[] = {
	0x0E00,	0x0102,	0x02,
	0x0E02,	0x0102,	0x02,
	0x0E0C,	0x0100,	0x02,
	0x2000,	0x4031,	0x02,
	0x2002,	0x8400,	0x02,
	0x2004,	0x12B0,	0x02,
	0x2006,	0xE292,	0x02,
	0x2008,	0x12B0,	0x02,
	0x200A,	0xE2B0,	0x02,
	0x200C,	0x40B2,	0x02,
	0x200E,	0x0250,	0x02,
	0x2010,	0x8070,	0x02,
	0x2012,	0x40B2,	0x02,
	0x2014,	0x01B8,	0x02,
	0x2016,	0x8072,	0x02,
	0x2018,	0x93D2,	0x02,
	0x201A,	0x00BD,	0x02,
	0x201C,	0x240F,	0x02,
	0x201E,	0x0900,	0x02,
	0x2020,	0x7312,	0x02,
	0x2022,	0x43D2,	0x02,
	0x2024,	0x00BD,	0x02,
	0x2026,	0x12B0,	0x02,
	0x2028,	0xE60E,	0x02,
	0x202A,	0x12B0,	0x02,
	0x202C,	0xE64C,	0x02,
	0x202E,	0x12B0,	0x02,
	0x2030,	0xEB28,	0x02,
	0x2032,	0x12B0,	0x02,
	0x2034,	0xE662,	0x02,
	0x2036,	0x12B0,	0x02,
	0x2038,	0xF84A,	0x02,
	0x203A,	0x3FFF,	0x02,
	0x203C,	0x12B0,	0x02,
	0x203E,	0xE5BE,	0x02,
	0x2040,	0x12B0,	0x02,
	0x2042,	0xE5E6,	0x02,
	0x2044,	0x3FEC,	0x02,
	0x2046,	0x4030,	0x02,
	0x2048,	0xF770,	0x02,
	0x204A,	0x120B,	0x02,
	0x204C,	0x430B,	0x02,
	0x204E,	0x4392,	0x02,
	0x2050,	0x7326,	0x02,
	0x2052,	0x90F2,	0x02,
	0x2054,	0x0010,	0x02,
	0x2056,	0x00BE,	0x02,
	0x2058,	0x201E,	0x02,
	0x205A,	0x43D2,	0x02,
	0x205C,	0x0180,	0x02,
	0x205E,	0x4392,	0x02,
	0x2060,	0x760E,	0x02,
	0x2062,	0x9382,	0x02,
	0x2064,	0x760C,	0x02,
	0x2066,	0x2002,	0x02,
	0x2068,	0x0C64,	0x02,
	0x206A,	0x3FFB,	0x02,
	0x206C,	0x421F,	0x02,
	0x206E,	0x760A,	0x02,
	0x2070,	0x931F,	0x02,
	0x2072,	0x200C,	0x02,
	0x2074,	0x421B,	0x02,
	0x2076,	0x018A,	0x02,
	0x2078,	0x4B82,	0x02,
	0x207A,	0x7600,	0x02,
	0x207C,	0x12B0,	0x02,
	0x207E,	0xE79E,	0x02,
	0x2080,	0x4B0F,	0x02,
	0x2082,	0x12B0,	0x02,
	0x2084,	0xE786,	0x02,
	0x2086,	0x4FC2,	0x02,
	0x2088,	0x0188,	0x02,
	0x208A,	0x3FE9,	0x02,
	0x208C,	0x903F,	0x02,
	0x208E,	0x0201,	0x02,
	0x2090,	0x23E6,	0x02,
	0x2092,	0x531B,	0x02,
	0x2094,	0x3FF5,	0x02,
	0x2096,	0x413B,	0x02,
	0x2098,	0x4130,	0x02,
	0x27FE,	0xE000,	0x02,
	0x3000,	0x60F8,	0x02,
	0x3002,	0x187F,	0x02,
	0x3004,	0x7060,	0x02,
	0x3006,	0x0114,	0x02,
	0x3008,	0x60B0,	0x02,
	0x300A,	0x1473,	0x02,
	0x300C,	0x0013,	0x02,
	0x300E,	0x140F,	0x02,
	0x3010,	0x0040,	0x02,
	0x3012,	0x100F,	0x02,
	0x3014,	0x60F8,	0x02,
	0x3016,	0x187F,	0x02,
	0x3018,	0x7060,	0x02,
	0x301A,	0x0114,	0x02,
	0x301C,	0x60B0,	0x02,
	0x301E,	0x1473,	0x02,
	0x3020,	0x0013,	0x02,
	0x3022,	0x140F,	0x02,
	0x3024,	0x0040,	0x02,
	0x3026,	0x000F,	0x02,
	0x3000,	0x60F8,	0x02,
	0x3002,	0x187F,	0x02,
	0x3004,	0x7060,	0x02,
	0x3006,	0x0114,	0x02,
	0x3008,	0x60B0,	0x02,
	0x300A,	0x1473,	0x02,
	0x300C,	0x0013,	0x02,
	0x300E,	0x140F,	0x02,
	0x3010,	0x0040,	0x02,
	0x3012,	0x100F,	0x02,
	0x3014,	0x60F8,	0x02,
	0x3016,	0x187F,	0x02,
	0x3018,	0x7060,	0x02,
	0x301A,	0x0114,	0x02,
	0x301C,	0x60B0,	0x02,
	0x301E,	0x1473,	0x02,
	0x3020,	0x0013,	0x02,
	0x3022,	0x140F,	0x02,
	0x3024,	0x0040,	0x02,
	0x3026,	0x000F,	0x02,
	0x0B00,	0x0000,	0x02,
	0x0B02,	0x0045,	0x02,
	0x0B04,	0xB405,	0x02,
	0x0B06,	0xC403,	0x02,
	0x0B08,	0x0081,	0x02,
	0x0B0A,	0x8252,	0x02,
	0x0B0C,	0xF814,	0x02,
	0x0B0E,	0xC618,	0x02,
	0x0B10,	0xA828,	0x02,
	0x0B12,	0x002C,	0x02,
	0x0B14,	0x4068,	0x02,
	0x0B16,	0x0000,	0x02,
	0x0F30,	0x6E25,	0x02,
	0x0F32,	0x7067,	0x02,
	0x0954,	0x0009,	0x02,
	0x0956,	0x0000,	0x02,
	0x0958,	0xBB80,	0x02,
	0x095A,	0x5140,	0x02,
	0x0C00,	0x1110,	0x02,
	0x0C02,	0x0011,	0x02,
	0x0C04,	0x0000,	0x02,
	0x0C06,	0x0200,	0x02,
	0x0C10,	0x0040,	0x02,
	0x0C12,	0x0040,	0x02,
	0x0C14,	0x0040,	0x02,
	0x0C16,	0x0040,	0x02,
	0x0A10,	0x4000,	0x02,
	0x3068,	0xFFFF,	0x02,
	0x306A,	0xFFFF,	0x02,
	0x006C,	0x0300,	0x02,
	0x005E,	0x0200,	0x02,
	0x000E,	0x0100,	0x02,
	0x0E0A,	0x0001,	0x02,
	0x004A,	0x0100,	0x02,
	0x004C,	0x0000,	0x02,
	0x000C,	0x0022,	0x02,
	0x0008,	0x0B00,	0x02,
	0x005A,	0x0202,	0x02,
	0x0012,	0x000E,	0x02,
	0x0018,	0x0A31,	0x02,
	0x0022,	0x0008,	0x02,
	0x0028,	0x0017,	0x02,
	0x0024,	0x0028,	0x02,
	0x002A,	0x002D,	0x02,
	0x0026,	0x0030,	0x02,
	0x002C,	0x07C7,	0x02,
	0x002E,	0x1111,	0x02,
	0x0030,	0x1111,	0x02,
	0x0032,	0x1111,	0x02,
	0x0006,	0x0823,	0x02,
	0x0116,	0x07B6,	0x02,
	0x0A22,	0x0000,	0x02,
	0x0A12,	0x0A20,	0x02,
	0x0A14,	0x0798,	0x02,
	0x003E,	0x0000,	0x02,
	0x0074,	0x0821,	0x02,
	0x0070,	0x0410,	0x02,
	0x0002,	0x0000,	0x02,
	0x0A02,	0x0000,	0x02,
	0x0A24,	0x0100,	0x02,
	0x0046,	0x0000,	0x02,
	0x0076,	0x0000,	0x02,
	0x0060,	0x0000,	0x02,
	0x0062,	0x0530,	0x02,
	0x0064,	0x0500,	0x02,
	0x0066,	0x0530,	0x02,
	0x0068,	0x0500,	0x02,
	0x0122,	0x0300,	0x02,
	0x015A,	0xFF08,	0x02,
	0x0804,	0x0200,	0x02,
	0x005C,	0x0100,	0x02,
	0x0A1A,	0x0800,	0x02,
	0x004C,	0x0000,	0x02,
	0x004E,	0x0100,	0x02,
	0x0040,	0x0000,	0x02,
	0x0042,	0x0100,	0x02,
	0x003E,	0x0000,	0x02,
};

static const u32 otp_mode_on_setting_hi556[] = {
	0x0F02,	0x00,	0x01,
	0x011A,	0x01,	0x01,
	0x011B,	0x09,	0x01,
	0x0D04,	0x01,	0x01,
	0x0D02,	0x07,	0x01,
	0x003E,	0x10,	0x01,
	0x0114,	0x01,	0x01,
};

static const u32 otp_mode_off_setting_hi556[] = {
	0x004A,	0x00,	0x01,
	0x0D04,	0x00,	0x01,
	0x003E,	0x00,	0x01,
	0x004A,	0x01,	0x01,
	0x0114,	0x01,	0x01,
};

static const u32 otp_read_initial_setting_hi556_size = ARRAY_SIZE(otp_read_initial_setting_hi556);
static const u32 otp_mode_on_setting_hi556_size = ARRAY_SIZE(otp_mode_on_setting_hi556);
static const u32 otp_mode_off_setting_hi556_size = ARRAY_SIZE(otp_mode_off_setting_hi556);

/********* GC5035 OTP *********/
#define GC5035_OTP_PAGE_ADDR						 0xfe
#define GC5035_OTP_MODE_ADDR						 0xf3
#define GC5035_OTP_BUSY_ADDR						 0x6f
#define GC5035_OTP_PAGE							  0x02
#define GC5035_OTP_ACCESS_ADDR_HIGH				  0x69

#define GC5035_OTP_ACCESS_ADDR_LOW				   0x6a
#define GC5035_OTP_READ_ADDR						 0x6c
#define GC5035_BANK_SELECT_ADDR					  0x1000

#define GC5035_BOKEH_OTP_START_ADDR_BANK1			0x1080
#define GC5035_BOKEH_OTP_START_ADDR_BANK2			0x1480
#define GC5035_BOKEH_OTP_USED_CAL_SIZE			   (0x6F + 0x1)

#define GC5035_UW_OTP_START_ADDR_BANK1			   0x1020
#define GC5035_UW_OTP_START_ADDR_BANK2			   0x17C0
#define GC5035_UW_OTP_USED_CAL_SIZE				  (0xF3 + 0x1)

#define GC5035_MACRO_OTP_START_ADDR_BANK1			0x1020
#define GC5035_MACRO_OTP_START_ADDR_BANK2			0x17C0
#define GC5035_MACRO_OTP_USED_CAL_SIZE			   (0xF3 + 0x1)

static const u32 sensor_gc5035_setfile_otp_global[] = {
	0xfc, 0x01, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0xf4, 0x40, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0xf5, 0xe9, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0xf6, 0x14, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0xf8, 0x45, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0xf9, 0x82, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0xfa, 0x00, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0xfc, 0x81, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0xfe, 0x00, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0x36, 0x01, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0xd3, 0x87, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0x36, 0x00, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0x33, 0x00, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0xfe, 0x03, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0x01, 0x00, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0xf7, 0x01, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0xfc, 0x8f, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0xfc, 0x8f, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0xfc, 0x8e, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0xfe, 0x00, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0xee, 0x30, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0x87, 0x18, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0xfe, 0x01, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0x8c, 0x90, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0xfe, 0x00, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0x11, 0x02, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0x17, 0x80, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0x19, 0x05, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0xfe, 0x02, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0x30, 0x03, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0x31, 0x03, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0xfe, 0x00, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0xd9, 0xc0, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0x1b, 0x20, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0x21, 0x48, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0x28, 0x22, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0x29, 0x58, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0x44, 0x20, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0x4b, 0x10, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0x4e, 0x1a, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0x50, 0x11, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0x52, 0x33, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0x53, 0x44, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0x55, 0x10, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0x5b, 0x11, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0xc5, 0x02, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0x8c, 0x1a, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0xfe, 0x02, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0x33, 0x05, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0x32, 0x38, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0xfe, 0x00, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0x16, 0x0c, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0x1a, 0x1a, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0x20, 0x10, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0x46, 0x83, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0x4a, 0x04, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0x54, 0x02, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0x62, 0x00, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0x72, 0x8f, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0x73, 0x89, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0x7a, 0x05, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0x7c, 0xa0, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0x7d, 0xcc, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0x90, 0x00, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0xce, 0x18, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0xd2, 0x40, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0xe6, 0xe0, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0xfe, 0x02, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0x12, 0x01, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0x13, 0x01, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0x14, 0x01, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0x15, 0x02, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0x22, 0x7c, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0xfe, 0x00, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0xfc, 0x88, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0xfe, 0x10, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0xfe, 0x00, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0xfc, 0x8e, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0xfe, 0x00, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0xfe, 0x00, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0xfe, 0x00, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0xfc, 0x88, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0xfe, 0x10, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0xfe, 0x00, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0xfc, 0x8e, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0xfe, 0x00, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0xb0, 0x6e, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0xb1, 0x01, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0xb2, 0x00, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0xb3, 0x00, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0xb4, 0x00, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0xb6, 0x00, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0xfe, 0x01, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0x53, 0x00, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0x87, 0x51, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0x89, 0x03, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0x60, 0x40, I2C_WRITE_FORMAT_ADDR8_DATA8,
};

static const u32 sensor_gc5035_setfile_otp_init[] = {
	0xfa, 0x10, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0xf5, 0xe9, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0xfe, 0x02, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0x67, 0xc0, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0x59, 0x3f, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0x55, 0x80, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0x65, 0x80, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0x66, 0x03, I2C_WRITE_FORMAT_ADDR8_DATA8,
};

/********* GC02M1 OTP *********/
#define GC02M1_OTP_PAGE_ADDR						 0xfe
#define GC02M1_OTP_MODE_ADDR						 0xf3
#define GC02M1_OTP_PAGE							  0x02
#define GC02M1_OTP_ACCESS_ADDR					   0x17
#define GC02M1_OTP_READ_ADDR						 0x19

#define GC02M1_OTP_START_ADDR						0x0078
#define GC02M1_OTP_USED_CAL_SIZE					 0x11 	//((0x00FF - 0x0078 + 0x1) / 8)

static const u32 sensor_gc02m1_setfile_otp_global[] = {
	0xfc, 0x01, I2C_WRITE_FORMAT_ADDR8_DATA8, 
	0xf4, 0x41, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0xf5, 0xc0, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0xf6, 0x44, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0xf8, 0x34, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0xf9, 0x82, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0xfa, 0x00, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0xfd, 0x80, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0xfc, 0x81, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0xfe, 0x03, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0x01, 0x20, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0xf7, 0x01, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0xfc, 0x80, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0xfc, 0x80, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0xfc, 0x80, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0xfc, 0x8e, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0xfe, 0x00, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0x87, 0x09, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0xee, 0x72, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0xfe, 0x01, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0x8c, 0x90, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0xfe, 0x00, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0x17, 0x80, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0x19, 0x04, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0x56, 0x20, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0x5b, 0x00, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0x5e, 0x01, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0x21, 0x3c, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0x29, 0x40, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0x44, 0x20, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0x4b, 0x10, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0x55, 0x1a, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0xcc, 0x01, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0x27, 0x30, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0x2b, 0x00, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0x33, 0x00, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0xe6, 0x50, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0x39, 0x07, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0x43, 0x04, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0x46, 0x2a, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0x7c, 0xa0, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0xd0, 0xbe, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0xd1, 0x60, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0xd2, 0x40, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0xd3, 0xf3, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0xde, 0x1d, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0xcd, 0x05, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0xce, 0x6f, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0xfc, 0x88, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0xfe, 0x10, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0xfe, 0x00, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0xfc, 0x8e, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0xfe, 0x00, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0xfe, 0x00, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0xfe, 0x00, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0xfe, 0x00, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0xfc, 0x88, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0xfe, 0x10, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0xfe, 0x00, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0xfc, 0x8e, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0xfe, 0x04, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0xe0, 0x01, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0xfe, 0x00, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0xfe, 0x00, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0x3e, 0x00, I2C_WRITE_FORMAT_ADDR8_DATA8,
};

static const u32 sensor_gc02m1_setfile_otp_mode[] = {
	0xfc, 0x01, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0xf4, 0x41, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0xf5, 0xc0, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0xf6, 0x44, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0xf8, 0x34, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0xf9, 0x82, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0xfa, 0x00, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0xfd, 0x80, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0xfc, 0x81, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0xfe, 0x03, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0x01, 0x20, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0xf7, 0x01, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0xfc, 0x80, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0xfc, 0x80, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0xfc, 0x80, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0xfc, 0x8e, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0xfe, 0x00, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0x87, 0x09, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0xee, 0x72, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0xfe, 0x01, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0x8c, 0x90, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0xfe, 0x00, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0x90, 0x00, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0x03, 0x04, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0x04, 0x83, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0x41, 0x04, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0x42, 0xf4, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0x05, 0x04, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0x06, 0x48, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0x07, 0x00, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0x08, 0x18, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0x9d, 0x18, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0x09, 0x00, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0x0a, 0x02, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0x0b, 0x00, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0x0c, 0x00, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0x0d, 0x04, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0x0e, 0xbc, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0x0f, 0x06, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0x10, 0x4c, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0x24, 0x00, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0x1a, 0x04, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0x1f, 0x11, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0x53, 0x90, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0xfc, 0x88, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0xfe, 0x10, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0xfe, 0x00, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0xfc, 0x8e, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0xfe, 0x00, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0xfe, 0x00, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0xfe, 0x00, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0xfe, 0x00, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0xfc, 0x88, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0xfe, 0x10, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0xfe, 0x00, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0xfc, 0x8e, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0xfe, 0x04, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0xe0, 0x01, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0xfe, 0x00, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0xfe, 0x01, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0x53, 0x44, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0x87, 0x51, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0x89, 0x03, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0xfe, 0x00, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0xb0, 0x74, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0xb1, 0x04, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0xb2, 0x00, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0xb6, 0x00, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0xfe, 0x04, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0xd8, 0x00, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0xc0, 0x40, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0xc0, 0x00, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0xc0, 0x00, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0xc0, 0x00, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0xc0, 0x60, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0xc0, 0x00, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0xc0, 0xc0, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0xc0, 0x2a, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0xc0, 0x80, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0xc0, 0x00, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0xc0, 0x00, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0xc0, 0x40, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0xc0, 0xa0, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0xc0, 0x00, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0xc0, 0x90, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0xc0, 0x19, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0xc0, 0xc0, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0xc0, 0x00, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0xc0, 0xD0, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0xc0, 0x2F, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0xc0, 0xe0, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0xc0, 0x00, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0xc0, 0x90, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0xc0, 0x39, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0xc0, 0x00, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0xc0, 0x01, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0xc0, 0x20, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0xc0, 0x04, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0xc0, 0x20, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0xc0, 0x01, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0xc0, 0xe0, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0xc0, 0x0f, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0xc0, 0x40, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0xc0, 0x01, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0xc0, 0xe0, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0xc0, 0x1a, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0xc0, 0x60, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0xc0, 0x01, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0xc0, 0x20, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0xc0, 0x25, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0xc0, 0x80, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0xc0, 0x01, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0xc0, 0xa0, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0xc0, 0x2c, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0xc0, 0xa0, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0xc0, 0x01, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0xc0, 0xe0, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0xc0, 0x32, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0xc0, 0xc0, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0xc0, 0x01, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0xc0, 0x20, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0xc0, 0x38, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0xc0, 0xe0, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0xc0, 0x01, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0xc0, 0x60, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0xc0, 0x3c, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0xc0, 0x00, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0xc0, 0x02, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0xc0, 0xa0, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0xc0, 0x40, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0xc0, 0x80, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0xc0, 0x02, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0xc0, 0x18, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0xc0, 0x5c, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0xfe, 0x00, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0x9f, 0x10, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0xfe, 0x00, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0x26, 0x20, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0xfe, 0x01, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0x40, 0x22, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0x46, 0x7f, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0x49, 0x0f, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0x4a, 0xf0, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0xfe, 0x04, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0x14, 0x80, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0x15, 0x80, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0x16, 0x80, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0x17, 0x80, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0xfe, 0x01, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0x41, 0x20, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0x4c, 0x00, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0x4d, 0x0c, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0x44, 0x08, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0x48, 0x03, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0xfe, 0x01, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0x90, 0x01, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0x91, 0x00, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0x92, 0x06, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0x93, 0x00, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0x94, 0x06, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0x95, 0x04, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0x96, 0xb0, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0x97, 0x06, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0x98, 0x40, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0x99, 0x00, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0xfe, 0x03, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0x01, 0x20, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0x03, 0xce, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0x04, 0x48, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0x15, 0x00, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0x21, 0x10, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0x22, 0x05, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0x23, 0x20, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0x25, 0x20, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0x26, 0x08, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0x29, 0x06, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0x2a, 0x0c, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0x2b, 0x08, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0xfe, 0x01, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0x8c, 0x10, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0xfe, 0x00, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0x3e, 0x00, I2C_WRITE_FORMAT_ADDR8_DATA8,
};

static const u32 sensor_gc02m1_setfile_otp_init[] = {
	0xfc, 0x01, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0xf4, 0x41, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0xf5, 0xc0, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0xf6, 0x44, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0xf8, 0x38, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0xf9, 0x82, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0xfa, 0x00, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0xfd, 0x80, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0xfc, 0x81, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0xfe, 0x03, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0x01, 0x20, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0xf7, 0x01, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0xfc, 0x80, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0xfc, 0x80, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0xfc, 0x80, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0xfc, 0x8e, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0xfe, 0x00, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0x87, 0x09, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0xee, 0x72, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0xfe, 0x01, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0x8c, 0x90, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0xf3, 0x30, I2C_WRITE_FORMAT_ADDR8_DATA8,
	0xfe, 0x02, I2C_WRITE_FORMAT_ADDR8_DATA8,
};

/********* HI1336 OTP *********/
#define IS_READ_MAX_HI1336_OTP_CAL_SIZE	(924)
#define HI1336_OTP_START_ADDR_BANK1	(0x0404)
#define HI1336_OTP_START_ADDR_BANK2	(0x07A4)
#define HI1336_OTP_START_ADDR_BANK3	(0x0B44)
#define HI1336_OTP_START_ADDR_BANK4	(0x0EE4)
#define HI1336_OTP_START_ADDR_BANK5	(0x1284)

/********* S5K3L6 OTP *********/
#define IS_READ_MAX_S5K3L6_OTP_CAL_SIZE	(380)
#define S5K3L6_OTP_PAGE_SIZE			(64)
#define S5K3L6_OTP_BANK_READ_PAGE		(0x34)
#define S5K3L6_OTP_PAGE_START_ADDR	(0x0A04)
#define S5K3L6_OTP_START_PAGE_BANK1	(0x34)
#define S5K3L6_OTP_START_PAGE_BANK2	(0x3A)
#define S5K3L6_OTP_READ_START_ADDR	(0x0A08)

/********* S5K4HA OTP *********/
#define S5K4HA_STANDBY_ADDR						 0x0136
#define S5K4HA_OTP_R_W_MODE_ADDR					0x0A00
#define S5K4HA_OTP_CHECK_ADDR					   0x0A01
#define S5K4HA_OTP_PAGE_SELECT_ADDR				 0x0A02
#define S5K4HA_OTP_USED_CAL_SIZE					(0x0A43 + 0x0440 - 0x0A08 + 0x1)
#define S5K4HA_OTP_PAGE_ADDR_L					  0x0A04
#define S5K4HA_OTP_PAGE_ADDR_H					  0x0A43
#define S5K4HA_OTP_BANK_SELECT					  0x0A04
#define S5K4HA_OTP_START_PAGE_BANK1				 0x11
#define S5K4HA_OTP_START_PAGE_BANK2				 0x2C

#define S5K4HA_OTP_START_ADDR					   0x0A08

static const u32 sensor_otp_4ha_global[] = {
	0x0100, 0x00, I2C_WRITE_FORMAT_ADDR16_DATA8,
	0x0A02, 0x7F, I2C_WRITE_FORMAT_ADDR16_DATA8,
	0x3B45, 0x01, I2C_WRITE_FORMAT_ADDR16_DATA8,
	0x3264, 0x01, I2C_WRITE_FORMAT_ADDR16_DATA8,
	0x3290, 0x10, I2C_WRITE_FORMAT_ADDR16_DATA8,
	0x0B05, 0x01, I2C_WRITE_FORMAT_ADDR16_DATA8,
	0x3069, 0xC7, I2C_WRITE_FORMAT_ADDR16_DATA8,
	0x3074, 0x06, I2C_WRITE_FORMAT_ADDR16_DATA8,
	0x3075, 0x32, I2C_WRITE_FORMAT_ADDR16_DATA8,
	0x3068, 0xF7, I2C_WRITE_FORMAT_ADDR16_DATA8,
	0x30C6, 0x01, I2C_WRITE_FORMAT_ADDR16_DATA8,
	0x301F, 0x20, I2C_WRITE_FORMAT_ADDR16_DATA8,
	0x306B, 0x9A, I2C_WRITE_FORMAT_ADDR16_DATA8,
	0x3091, 0x1F, I2C_WRITE_FORMAT_ADDR16_DATA8,
	0x306E, 0x71, I2C_WRITE_FORMAT_ADDR16_DATA8,
	0x306F, 0x28, I2C_WRITE_FORMAT_ADDR16_DATA8,
	0x306D, 0x08, I2C_WRITE_FORMAT_ADDR16_DATA8,
	0x3084, 0x16, I2C_WRITE_FORMAT_ADDR16_DATA8,
	0x3070, 0x0F, I2C_WRITE_FORMAT_ADDR16_DATA8,
	0x306A, 0x79, I2C_WRITE_FORMAT_ADDR16_DATA8,
	0x30B0, 0xFF, I2C_WRITE_FORMAT_ADDR16_DATA8,
	0x30C2, 0x05, I2C_WRITE_FORMAT_ADDR16_DATA8,
	0x30C4, 0x06, I2C_WRITE_FORMAT_ADDR16_DATA8,
	0x3081, 0x07, I2C_WRITE_FORMAT_ADDR16_DATA8,
	0x307B, 0x85, I2C_WRITE_FORMAT_ADDR16_DATA8,
	0x307A, 0x0A, I2C_WRITE_FORMAT_ADDR16_DATA8,
	0x3079, 0x0A, I2C_WRITE_FORMAT_ADDR16_DATA8,
	0x308A, 0x20, I2C_WRITE_FORMAT_ADDR16_DATA8,
	0x308B, 0x08, I2C_WRITE_FORMAT_ADDR16_DATA8,
	0x308C, 0x0B, I2C_WRITE_FORMAT_ADDR16_DATA8,
	0x392F, 0x01, I2C_WRITE_FORMAT_ADDR16_DATA8,
	0x3930, 0x00, I2C_WRITE_FORMAT_ADDR16_DATA8,
	0x3924, 0x7F, I2C_WRITE_FORMAT_ADDR16_DATA8,
	0x3925, 0xFD, I2C_WRITE_FORMAT_ADDR16_DATA8,
	0x3C08, 0xFF, I2C_WRITE_FORMAT_ADDR16_DATA8,
	0x3C09, 0xFF, I2C_WRITE_FORMAT_ADDR16_DATA8,
	0x3C31, 0xFF, I2C_WRITE_FORMAT_ADDR16_DATA8,
	0x3C32, 0xFF, I2C_WRITE_FORMAT_ADDR16_DATA8,
};

static const u32 sensor_otp_4ha_global_size = ARRAY_SIZE(sensor_otp_4ha_global);
/*******************************/

enum is_rom_state {
	IS_ROM_STATE_FINAL_MODULE,
	IS_ROM_STATE_LATEST_MODULE,
	IS_ROM_STATE_INVALID_ROM_VERSION,
	IS_ROM_STATE_OTHER_VENDOR,	/* Q module */
	IS_ROM_STATE_CAL_RELOAD,
	IS_ROM_STATE_CAL_READ_DONE,
	IS_ROM_STATE_FW_FIND_DONE,
	IS_ROM_STATE_CHECK_BIN_DONE,
	IS_ROM_STATE_SKIP_CAL_LOADING,
	IS_ROM_STATE_SKIP_CRC_CHECK,
	IS_ROM_STATE_SKIP_HEADER_LOADING,
	IS_ROM_STATE_MAX
};

enum is_crc_error {
	IS_CRC_ERROR_HEADER,
	IS_CRC_ERROR_ALL_SECTION,
	IS_CRC_ERROR_DUAL_CAMERA,
	IS_CRC_ERROR_FIRMWARE,
	IS_CRC_ERROR_SETFILE_1,
	IS_CRC_ERROR_SETFILE_2,
	IS_CRC_ERROR_HIFI_TUNNING,
	IS_ROM_ERROR_MAX
};

#ifdef CONFIG_SEC_CAL_ENABLE
struct rom_standard_cal_data {
	int32_t		rom_standard_cal_start_addr;
	int32_t		rom_standard_cal_end_addr;
	int32_t		rom_standard_cal_module_crc_addr;
	int32_t		rom_standard_cal_module_checksum_len;
	int32_t		rom_standard_cal_sec2lsi_end_addr;

	int32_t		rom_header_standard_cal_end_addr;
	int32_t		rom_header_main_shading_end_addr;

	int32_t		rom_awb_start_addr;
	int32_t		rom_awb_end_addr;
	int32_t		rom_awb_section_crc_addr;

	int32_t		rom_shading_start_addr;
	int32_t		rom_shading_end_addr;
	int32_t		rom_shading_section_crc_addr;

	int32_t		rom_factory_start_addr;
	int32_t		rom_factory_end_addr;

	int32_t		rom_awb_sec2lsi_start_addr;
	int32_t		rom_awb_sec2lsi_end_addr;
	int32_t		rom_awb_sec2lsi_checksum_addr;
	int32_t		rom_awb_sec2lsi_checksum_len;

	int32_t		rom_shading_sec2lsi_start_addr;
	int32_t		rom_shading_sec2lsi_end_addr;
	int32_t		rom_shading_sec2lsi_checksum_addr;
	int32_t		rom_shading_sec2lsi_checksum_len;

	int32_t		rom_factory_sec2lsi_start_addr;

#if defined (CONFIG_CAMERA_EEPROM_DUALIZED)
	int32_t		rom_dualized_standard_cal_start_addr;
	int32_t		rom_dualized_standard_cal_end_addr;
	int32_t		rom_dualized_standard_cal_module_crc_addr;
	int32_t		rom_dualized_standard_cal_module_checksum_len;
	int32_t		rom_dualized_standard_cal_sec2lsi_end_addr;

	int32_t		rom_dualized_header_standard_cal_end_addr;
	int32_t		rom_dualized_header_main_shading_end_addr;

	int32_t		rom_dualized_awb_start_addr;
	int32_t		rom_dualized_awb_end_addr;
	int32_t		rom_dualized_awb_section_crc_addr;

	int32_t		rom_dualized_shading_start_addr;
	int32_t		rom_dualized_shading_end_addr;
	int32_t		rom_dualized_shading_section_crc_addr;

	int32_t		rom_dualized_factory_start_addr;
	int32_t		rom_dualized_factory_end_addr;

	int32_t		rom_dualized_awb_sec2lsi_start_addr;
	int32_t		rom_dualized_awb_sec2lsi_end_addr;
	int32_t		rom_dualized_awb_sec2lsi_checksum_addr;
	int32_t		rom_dualized_awb_sec2lsi_checksum_len;

	int32_t		rom_dualized_shading_sec2lsi_start_addr;
	int32_t		rom_dualized_shading_sec2lsi_end_addr;
	int32_t		rom_dualized_shading_sec2lsi_checksum_addr;
	int32_t		rom_dualized_shading_sec2lsi_checksum_len;

	int32_t		rom_dualized_factory_sec2lsi_start_addr;
#endif
};

bool is_sec_readcal_dump_post_sec2lsi(struct is_core *core, char *buf, int position);
#endif
#ifdef USES_STANDARD_CAL_RELOAD
bool is_sec_sec2lsi_check_cal_reload(void);
void is_sec_sec2lsi_set_cal_reload(bool val);
#endif

struct is_rom_info {
	unsigned long	rom_state;
	unsigned long	crc_error;

	/* set by dts parsing */
	u32		rom_power_position;
	u32		rom_size;

	char	camera_module_es_version;
	u32		cal_map_es_version;

	u32		header_crc_check_list[IS_ROM_CRC_MAX_LIST];
	u32		header_crc_check_list_len;
	u32		crc_check_list[IS_ROM_CRC_MAX_LIST];
	u32		crc_check_list_len;
	u32		dual_crc_check_list[IS_ROM_CRC_MAX_LIST];
	u32		dual_crc_check_list_len;
	u32		rom_dualcal_slave0_tilt_list[IS_ROM_DUAL_TILT_MAX_LIST];
	u32		rom_dualcal_slave0_tilt_list_len;
	u32		rom_dualcal_slave1_tilt_list[IS_ROM_DUAL_TILT_MAX_LIST];
	u32		rom_dualcal_slave1_tilt_list_len;
	u32		rom_dualcal_slave2_tilt_list[IS_ROM_DUAL_TILT_MAX_LIST];
	u32		rom_dualcal_slave2_tilt_list_len;
	u32		rom_dualcal_slave3_tilt_list[IS_ROM_DUAL_TILT_MAX_LIST];
	u32		rom_dualcal_slave3_tilt_list_len;
	u32		rom_ois_list[IS_ROM_OIS_MAX_LIST];
	u32		rom_ois_list_len;

	/* set -1 if not support */
	int32_t		rom_header_cal_data_start_addr;
	int32_t		rom_header_version_start_addr;
	int32_t		rom_header_cal_map_ver_start_addr;
	int32_t		rom_header_isp_setfile_ver_start_addr;
	int32_t		rom_header_project_name_start_addr;
	int32_t		rom_header_module_id_addr;
	int32_t		rom_header_sensor_id_addr;
	int32_t		rom_header_mtf_data_addr;
	int32_t		rom_awb_master_addr;
	int32_t		rom_awb_module_addr;
	int32_t		rom_af_cal_addr[AF_CAL_MAX];
	int32_t		rom_af_cal_addr_len;
	int32_t		rom_af_cal_sac_addr;
	int32_t		rom_paf_cal_start_addr;
#ifdef CONFIG_SEC_CAL_ENABLE
	/* standard cal */
	bool		use_standard_cal;
	struct rom_standard_cal_data standard_cal_data;
	struct rom_standard_cal_data backup_standard_cal_data;
#endif

	int32_t		rom_header_sensor2_id_addr;
	int32_t		rom_header_sensor2_version_start_addr;
	int32_t		rom_header_sensor2_mtf_data_addr;
	int32_t		rom_sensor2_awb_master_addr;
	int32_t		rom_sensor2_awb_module_addr;
	int32_t		rom_sensor2_af_cal_addr[AF_CAL_MAX];
	int32_t		rom_sensor2_af_cal_addr_len;
	int32_t		rom_sensor2_paf_cal_start_addr;

	int32_t		rom_dualcal_slave0_start_addr;
	int32_t		rom_dualcal_slave0_size;
	int32_t		rom_dualcal_slave1_start_addr;
	int32_t		rom_dualcal_slave1_size;
	int32_t		rom_dualcal_slave2_start_addr;
	int32_t		rom_dualcal_slave2_size;

	int32_t		rom_pdxtc_cal_data_start_addr;
	int32_t		rom_pdxtc_cal_data_0_size;
	int32_t		rom_pdxtc_cal_data_1_size;

	int32_t		rom_spdc_cal_data_start_addr;
	int32_t		rom_spdc_cal_data_size;

	int32_t		rom_xtc_cal_data_start_addr;
	int32_t		rom_xtc_cal_data_size;
	int32_t		rom_xgc_cal_data_start_addr;
	int32_t		rom_xgc_cal_data_0_size;
	int32_t		rom_xgc_cal_data_1_size;
	int32_t		rom_qbgc_cal_data_start_addr;
	int32_t		rom_qbgc_cal_data_size;

	int32_t		rear_remosaic_tetra_xtc_start_addr;
	int32_t		rear_remosaic_tetra_xtc_size;
	int32_t		rear_remosaic_sensor_xtc_start_addr ;
	int32_t		rear_remosaic_sensor_xtc_size;
	int32_t		rear_remosaic_pdxtc_start_addr;
	int32_t		rear_remosaic_pdxtc_size;
	int32_t		rear_remosaic_sw_ggc_start_addr;
	int32_t		rear_remosaic_sw_ggc_size;

	bool	rom_pdxtc_cal_endian_check;
	u32		rom_pdxtc_cal_data_addr_list[CROSSTALK_CAL_MAX];
	u32		rom_pdxtc_cal_data_addr_list_len;
	bool	rom_gcc_cal_endian_check;
	u32		rom_gcc_cal_data_addr_list[CROSSTALK_CAL_MAX];
	u32		rom_gcc_cal_data_addr_list_len;
	bool	rom_xtc_cal_endian_check;
	u32		rom_xtc_cal_data_addr_list[CROSSTALK_CAL_MAX];
	u32		rom_xtc_cal_data_addr_list_len;

	u16		rom_dualcal_slave1_cropshift_x_addr;
	u16		rom_dualcal_slave1_cropshift_y_addr;

	u16		rom_dualcal_slave1_oisshift_x_addr;
	u16		rom_dualcal_slave1_oisshift_y_addr;

	u16		rom_dualcal_slave1_dummy_flag_addr;

#if defined (CONFIG_CAMERA_EEPROM_DUALIZED)
	bool	is_read_dualized_values;
	u32		header_dualized_crc_check_list[IS_ROM_CRC_MAX_LIST];
	u32		header_dualized_crc_check_list_len;
	u32		crc_dualized_check_list[IS_ROM_CRC_MAX_LIST];
	u32		crc_dualized_check_list_len;
	u32		dual_crc_dualized_check_list[IS_ROM_CRC_MAX_LIST];
	u32		dual_crc_dualized_check_list_len;
	u32		rom_dualized_dualcal_slave0_tilt_list[IS_ROM_DUAL_TILT_MAX_LIST];
	u32		rom_dualized_dualcal_slave0_tilt_list_len;
	u32		rom_dualized_dualcal_slave1_tilt_list[IS_ROM_DUAL_TILT_MAX_LIST];
	u32		rom_dualized_dualcal_slave1_tilt_list_len;
	u32		rom_dualized_dualcal_slave2_tilt_list[IS_ROM_DUAL_TILT_MAX_LIST];
	u32		rom_dualized_dualcal_slave2_tilt_list_len;
	u32		rom_dualized_dualcal_slave3_tilt_list[IS_ROM_DUAL_TILT_MAX_LIST];
	u32		rom_dualized_dualcal_slave3_tilt_list_len;
	u32		rom_dualized_ois_list[IS_ROM_OIS_MAX_LIST];
	u32		rom_dualized_ois_list_len;

	/* set -1 if not support */
	int32_t		rom_dualized_header_cal_data_start_addr;
	int32_t		rom_dualized_header_version_start_addr;
	int32_t		rom_dualized_header_cal_map_ver_start_addr;
	int32_t		rom_dualized_header_isp_setfile_ver_start_addr;
	int32_t		rom_dualized_header_project_name_start_addr;
	int32_t		rom_dualized_header_module_id_addr;
	int32_t		rom_dualized_header_sensor_id_addr;
	int32_t		rom_dualized_header_mtf_data_addr;
	int32_t		rom_dualized_header_f2_mtf_data_addr;
	int32_t		rom_dualized_header_f3_mtf_data_addr;
	int32_t		rom_dualized_awb_master_addr;
	int32_t		rom_dualized_awb_module_addr;
	int32_t		rom_dualized_af_cal_addr[AF_CAL_MAX];
	int32_t		rom_dualized_af_cal_addr_len;
	int32_t		rom_dualized_af_cal_sac_addr;
	int32_t		rom_dualized_paf_cal_start_addr;
#ifdef CONFIG_SEC_CAL_ENABLE
	/* standard cal */
	bool		use_dualized_standard_cal;
	struct rom_standard_cal_data standard_dualized_cal_data;
	struct rom_standard_cal_data backup_standard_dualized_cal_data;
#endif

	int32_t		rom_dualized_header_sensor2_id_addr;
	int32_t		rom_dualized_header_sensor2_version_start_addr;
	int32_t		rom_dualized_header_sensor2_mtf_data_addr;
	int32_t		rom_dualized_sensor2_awb_master_addr;
	int32_t		rom_dualized_sensor2_awb_module_addr;
	int32_t		rom_dualized_sensor2_af_cal_addr[AF_CAL_MAX];
	int32_t		rom_dualized_sensor2_af_cal_addr_len;
	int32_t		rom_dualized_sensor2_paf_cal_start_addr;

	int32_t		rom_dualized_dualcal_slave0_start_addr;
	int32_t		rom_dualized_dualcal_slave0_size;
	int32_t		rom_dualized_dualcal_slave1_start_addr;
	int32_t		rom_dualized_dualcal_slave1_size;
	int32_t		rom_dualized_dualcal_slave2_start_addr;
	int32_t		rom_dualized_dualcal_slave2_size;

	int32_t		rom_dualized_pdxtc_cal_data_start_addr;
	int32_t		rom_dualized_pdxtc_cal_data_0_size;
	int32_t		rom_dualized_pdxtc_cal_data_1_size;

	int32_t		rom_dualized_spdc_cal_data_start_addr;
	int32_t		rom_dualized_spdc_cal_data_size;

	int32_t		rom_dualized_xtc_cal_data_start_addr;
	int32_t		rom_dualized_xtc_cal_data_size;
	int32_t		rom_dualized_xgc_cal_data_start_addr;
	int32_t		rom_dualized_xgc_cal_data_0_size;
	int32_t		rom_dualized_xgc_cal_data_1_size;
	int32_t		rom_dualized_qbgc_cal_data_start_addr;
	int32_t		rom_dualized_qbgc_cal_data_size;

	bool	rom_dualized_pdxtc_cal_endian_check;
	u32		rom_dualized_pdxtc_cal_data_addr_list[CROSSTALK_CAL_MAX];
	u32		rom_dualized_pdxtc_cal_data_addr_list_len;
	bool	rom_dualized_gcc_cal_endian_check;
	u32		rom_dualized_gcc_cal_data_addr_list[CROSSTALK_CAL_MAX];
	u32		rom_dualized_gcc_cal_data_addr_list_len;
	bool	rom_dualized_xtc_cal_endian_check;
	u32		rom_dualized_xtc_cal_data_addr_list[CROSSTALK_CAL_MAX];
	u32		rom_dualized_xtc_cal_data_addr_list_len;

	u16		rom_dualized_dualcal_slave1_cropshift_x_addr;
	u16		rom_dualized_dualcal_slave1_cropshift_y_addr;

	u16		rom_dualized_dualcal_slave1_oisshift_x_addr;
	u16		rom_dualized_dualcal_slave1_oisshift_y_addr;

	u16		rom_dualized_dualcal_slave1_dummy_flag_addr;
#endif

	char		header_ver[IS_HEADER_VER_SIZE + 1];
	char		header2_ver[IS_HEADER_VER_SIZE + 1];
	char		cal_map_ver[IS_CAL_MAP_VER_SIZE + 1];
	char		setfile_ver[IS_ISP_SETFILE_VER_SIZE + 1];

	char		project_name[IS_PROJECT_NAME_SIZE + 1];
	char		rom_sensor_id[IS_SENSOR_ID_SIZE + 1];
	char		rom_sensor2_id[IS_SENSOR_ID_SIZE + 1];
	u8		rom_module_id[IS_MODULE_ID_SIZE + 1];
};

bool is_sec_get_force_caldata_dump(void);
int is_sec_set_force_caldata_dump(bool fcd);

ssize_t write_data_to_file(char *name, char *buf, size_t count, loff_t *pos);
ssize_t read_data_rom_file(char *name, char *buf, size_t count, loff_t *pos);
bool is_sec_file_exist(char *name);
int is_sec_get_max_cal_size(struct is_core *core, int position);
int is_sec_get_cal_buf_rom_data(char **buf, int rom_id);
int is_sec_get_sysfs_finfo(struct is_rom_info **finfo, int rom_id);
int is_sec_get_sysfs_pinfo(struct is_rom_info **pinfo, int rom_id);
int is_sec_get_front_cal_buf(char **buf, int rom_id);

int is_sec_get_cal_buf(char **buf, int rom_id);
int is_sec_get_loaded_fw(char **buf);
int is_sec_set_loaded_fw(char *buf);
int is_sec_get_loaded_c1_fw(char **buf);
int is_sec_set_loaded_c1_fw(char *buf);

int is_sec_get_camid(void);
int is_sec_set_camid(int id);
int is_sec_get_pixel_size(char *header_ver);
int is_sec_sensorid_find(struct is_core *core);
int is_sec_sensorid_find_front(struct is_core *core);
int is_sec_run_fw_sel(int rom_id);

int is_sec_readfw(struct is_core *core);
int is_sec_read_setfile(struct is_core *core);
#ifdef CAMERA_MODULE_COMPRESSED_FW_DUMP
int is_sec_inflate_fw(u8 **buf, unsigned long *size);
#endif
int is_sec_fw_sel_eeprom(int id, bool headerOnly);
int is_sec_fw_sel_otprom(int id, bool headerOnly);
#if defined (CONFIG_CAMERA_EEPROM_DUALIZED)
int is_sec_readcal_eeprom_dualized(int rom_id);
#endif
int is_sec_write_fw(struct is_core *core);
int is_sec_fw_revision(char *fw_ver);
int is_sec_fw_revision(char *fw_ver);
bool is_sec_fw_module_compare(char *fw_ver1, char *fw_ver2);
int is_sec_compare_ver(int rom_id);
bool is_sec_check_rom_ver(struct is_core *core, int rom_id);

bool is_sec_check_fw_crc32(char *buf, u32 checksum_seed, unsigned long size);
bool is_sec_check_cal_crc32(char *buf, int id);
void is_sec_make_crc32_table(u32 *table, u32 id);
#ifdef CONFIG_SEC_CAL_ENABLE
bool is_sec_check_awb_lsc_crc32_post_sec2lsi(char *buf, int position, int awb_length, int lsc_length);
#endif

int is_sec_gpio_enable(struct exynos_platform_is *pdata, char *name, bool on);
int is_sec_ldo_enable(struct device *dev, char *name, bool on);
int is_sec_rom_power_on(struct is_core *core, int position);
int is_sec_rom_power_off(struct is_core *core, int position);
void remove_dump_fw_file(void);
int is_get_dual_cal_buf(int slave_position, char **buf, int *size);
#if defined(CAMERA_UWIDE_DUALIZED)
void is_sec_set_rear3_dualized_rom_probe(void);
#endif
int is_sec_update_dualized_sensor(struct is_core *core, int position);

int is_get_remosaic_cal_buf(int sensor_position, char **buf, int *size);
#ifdef MODIFY_CAL_MAP_FOR_SWREMOSAIC_LIB
int is_modify_remosaic_cal_buf(int sensor_position, char *cal_buf, char **buf, int *size);
#endif
int is_sec_get_sysfs_finfo_by_position(int position, struct is_rom_info **finfo);

#endif /* IS_SEC_DEFINE_H */
