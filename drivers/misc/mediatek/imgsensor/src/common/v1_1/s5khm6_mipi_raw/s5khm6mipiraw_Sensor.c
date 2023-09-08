/*
 * Copyright (C) 2021 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

/*****************************************************************************
 *
 * Filename:
 * ---------
 *	 s5khm6mipiraw_Sensor.c
 *
 * Project:
 * --------
 *	 ALPS
 *
 * Description:
 * ------------
 *	 Source code of Sensor driver
 *
 *
 *------------------------------------------------------------------------------
 * Upper this line, this part is controlled by CC/CQ. DO NOT MODIFY!!
 *============================================================================
 ****************************************************************************/
#define PFX "S5KHM6 D/D"
#define pr_fmt(fmt) PFX "[%s] " fmt, __func__

#include <linux/videodev2.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/atomic.h>
#include <linux/types.h>
#include <linux/slab.h>

#include "kd_camera_typedef.h"
#include "kd_imgsensor.h"

#include "kd_imgsensor_define.h"
#include "kd_imgsensor_errcode.h"
#include "kd_imgsensor_sysfs_adapter.h"

#include "imgsensor_sysfs.h"

#include "s5khm6mipiraw_Sensor.h"
#include "s5khm6mipiraw_setfile_otp.h"
#include "s5khm6mipiraw_setfile.h"
#include "s5khm6mipiraw_uxtc.h"

#define CONFIG_TRAN_CAMERA_SYNC_AWB_TO_KERNEL

#define MULTI_WRITE                                   (1)

#define UINT64 unsigned long long

#if MULTI_WRITE
static const int I2C_BUFFER_LEN = 1020;
#else
static const int I2C_BUFFER_LEN = 4;
#endif

#if INDIRECT_BURST
static const int I2C_BURST_BUFFER_LEN = 1400;
#endif

#define SENSOR_HM6_COARSE_INTEGRATION_TIME_MAX_MARGIN (19)
#define SENSOR_HM6_MAX_COARSE_INTEGRATION_TIME        (65535)

static int sNightHyperlapseSpeed;

static bool enable_adaptive_mipi;
static int adaptive_mipi_index;
static bool mIsPartialXTCCal = KAL_FALSE;
static bool mIsFullXTCCal = KAL_FALSE;
#ifdef USE_RAW12
static enum IMGSENSOR_FCM_SRAM cur_fcm_sram = IMGSENSOR_FCM_SRAM_NOT_SET;
#endif

#if WRITE_SENSOR_CAL_XTC
static void sensor_xtc_write(bool isRemosaic);
static void sensor_xtc_write_otp(bool isRemosaic);
#endif
#define LOG_DBG(format, args...) pr_debug(format, ##args)
#define LOG_INF(format, args...) pr_info(format, ##args)
#define LOG_ERR(format, args...) pr_err(format, ##args)

static DEFINE_SPINLOCK(imgsensor_drv_lock);
//Sensor ID Value: 0x1AD6, sensor id defined in Kd_imgsensor.h
static struct imgsensor_info_struct imgsensor_info = {
	.sensor_id = S5KHM6_SENSOR_ID,
	.checksum_value = 0xa4c32546,
	.pre = { //#3-1
		.pclk = 1634285714,
		.linelength  = 8512,
		.framelength = 6390,
		.startx = 0,
		.starty = 0,
		.grabwindow_width  = 4000,
		.grabwindow_height = 3000,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 300,
		.mipi_pixel_rate = 606450000, // .1386171429 bps (sps * 16 / 7 = bps)
	},
	.cap = { //#3-1
		.pclk = 1634285714,
		.linelength  = 8512,
		.framelength = 6390,
		.startx = 0,
		.starty = 0,
		.grabwindow_width  = 4000,
		.grabwindow_height = 3000,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 300,
		.mipi_pixel_rate = 606450000,
	},
	.normal_video = { //#3-5
		.pclk = 1634285714,
		.linelength  = 8512,
		.framelength = 6390,
		.startx = 0,
		.starty = 0,
		.grabwindow_width  = 4000,
		.grabwindow_height = 2252,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 300,
		.mipi_pixel_rate = 606450000,
	},
	.hs_video = { //#4-2
		.pclk = 1634285714,
		.linelength  = 6744,
		.framelength = 1012,
		.startx = 0,
		.starty = 0,
		.grabwindow_width  = 1280,
		.grabwindow_height = 720,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 2400,
		.mipi_pixel_rate = 606450000,
	},
	.slim_video = { //NOT USED
		.pclk = 1634285714,
		.linelength  = 8512,
		.framelength = 6414,
		.startx = 0,
		.starty = 0,
		.grabwindow_width  = 4000,
		.grabwindow_height = 3000,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 300,
		.mipi_pixel_rate = 606450000,
	},
	.custom1 = { //#5-1
		.pclk = 1634285714,
		.linelength  = 6744,
		.framelength = 4048, //Frame length x2 : 60fps
		.startx = 0,
		.starty = 0,
		.grabwindow_width  = 2000,
		.grabwindow_height = 1500,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 600,
		.mipi_pixel_rate = 606450000,
	},
	.custom2 = { //#1-1
		.pclk = 1634285714,
		.linelength  = 19176,
		.framelength = 10676,
		.startx = 0,
		.starty = 0,
		.grabwindow_width  = 12000,
		.grabwindow_height = 9000,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 80, // 8fps
		.mipi_pixel_rate = 606450000,
	},
	.custom3 = { //#1-2 remosaic crop
		.pclk = 1634285714,
		.linelength  = 19032,
		.framelength = 5378,
		.startx = 0,
		.starty = 0,
		.grabwindow_width  = 4000,
		.grabwindow_height = 3000,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 160,
		.mipi_pixel_rate = 606450000,
	},
	.custom4 = {
		.pclk = 1634285714,
#ifdef USE_RAW12 //#2-2 IDCG
		.linelength  = 17360,
		.framelength = 3132, //3144 -> 3132 for 30 fps
#else //#3-6(4000x2252), 9sum - pseudo RAW12
		.linelength  = 9968,
		.framelength = 5456, //5476 -> 5456 for 30 fps
#endif
		.startx = 0,
		.starty = 0,
		.grabwindow_width  = 4000,
		.grabwindow_height = 2252,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 300,
		.mipi_pixel_rate = 505375000,
	},
	.custom5 = {
		.pclk = 1634285714,
#ifdef USE_RAW12 //#2-1 IDCG
		.linelength  = 17360,
		.framelength = 3132, //3144 -> 3132 for 30 fps
#else //#3-2(4000x3000), 9sum - pseudo RAW12
		.linelength  = 9968,
		.framelength = 5456, //5476 -> 5456 for 30 fps
#endif
		.startx = 0,
		.starty = 0,
		.grabwindow_width  = 4000,
		.grabwindow_height = 3000,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 300,
		.mipi_pixel_rate = 505375000,
	},
	.custom6 = { //#3-3
		.pclk = 1634285714,
		.linelength  = 8512,
		.framelength = 3198,
		.startx = 0,
		.starty = 0,
		.grabwindow_width  = 4000,
		.grabwindow_height = 2252,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 600,
		.mipi_pixel_rate = 606450000,
	},
	.margin = 10,
	.min_shutter = 8,
	.min_gain = 64,
	.max_gain = 4096,
	.min_gain_iso = 20,
	.exp_step = 1,
	.gain_step = 2,
	.gain_type = 2,
	//.max_frame_length = 0xFFFF,
	.max_frame_length = 0x1F0000,

	/* shutter delay frame for AE cycle,
	 * 2 frame with ispGain_delay-shut_delay=2-0=2
	 */
	.ae_shut_delay_frame = 0,

	/* sensor gain delay frame for AE cycle,
	 * 2 frame with ispGain_delay-sensor_gain_delay=2-0=2
	 */
	.ae_sensor_gain_delay_frame = 0,
	.ae_ispGain_delay_frame = 2,

	.frame_time_delay_frame = 1,
	.ihdr_support = 0,
	.temperature_support = 0,/* 1, support; 0,not support */
	.ihdr_le_firstline = 0,
	.sensor_mode_num = 11,

#ifdef CONFIG_IMPROVE_CAMERA_PERFORMANCE
	.cap_delay_frame = 2,
	.pre_delay_frame = 1,
	.video_delay_frame = 2,
#else
	.cap_delay_frame = 3,
	.pre_delay_frame = 3,
	.video_delay_frame = 3,
#endif

	.hs_video_delay_frame = 3,

#ifdef CONFIG_IMPROVE_CAMERA_PERFORMANCE
	.custom1_delay_frame = 1,
	.custom2_delay_frame = 1,
	.custom3_delay_frame = 1,
	.custom4_delay_frame = 1,
	.custom5_delay_frame = 1,
	.custom6_delay_frame = 2,
	.slim_video_delay_frame = 1,
#else
	.custom1_delay_frame = 3,
	.custom2_delay_frame = 3,
	.custom3_delay_frame = 3,
	.custom4_delay_frame = 3,
	.custom5_delay_frame = 3,
	.custom6_delay_frame = 3,
	.slim_video_delay_frame = 3,
#endif

	.isp_driving_current = ISP_DRIVING_8MA,
	.sensor_interface_type = SENSOR_INTERFACE_TYPE_MIPI,
	.mipi_sensor_type = MIPI_CPHY,
	.mipi_settle_delay_mode = 1,
	.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_4CELL_HW_BAYER_Gr,
	.mclk = 26,
	.mipi_lane_num = SENSOR_MIPI_3_LANE,
	.i2c_speed = 1000,

	/* record sensor support all write id addr,
	 * only supprt 4 must end with 0xff
	 */
	.i2c_addr_table = {0x5A, 0xff},
};


static struct imgsensor_struct imgsensor = {
	.mirror = IMAGE_NORMAL,

	/* IMGSENSOR_MODE enum value,record current sensor mode,such as:
	 * INIT, Preview, Capture, Video,High Speed Video, Slim Video
	 */
	.sensor_mode = IMGSENSOR_MODE_INIT,
	.shutter = 0x3D0,
	.gain = 0x100,
	.dummy_pixel = 0,
	.dummy_line = 0,
	.current_fps = 0,

	/* auto flicker enable: KAL_FALSE for disable auto flicker,
	 * KAL_TRUE for enable auto flicker
	 */
	.autoflicker_en = KAL_FALSE,

	/* test pattern mode or not.
	 * KAL_FALSE for in test pattern mode,
	 * KAL_TRUE for normal output
	 */
	.test_pattern = KAL_FALSE,
	.current_scenario_id = MSDK_SCENARIO_ID_CAMERA_PREVIEW,
	.ihdr_mode = KAL_FALSE,
	.i2c_write_id = 0x5A,
};

static struct SENSOR_VC_INFO_STRUCT SENSOR_VC_INFO[10] = {
	/* Preview mode setting */
	{0x02, 0x0a, 0x00, 0x08, 0x40, 0x00,
	 0x00, 0x2B, 0x0FA0, 0x0BB8,/*VC0, RAW10(0x2B), 4000x3000 pixel*/
	 0x00, 0x00, 0x0000, 0x0000,/*VC1*/
	 0x01, 0x2B, 0x0FA0, 0x02EE,/*VC2, 4000x750 pixel -> 5000x750 byte*/
	 0x00, 0x00, 0x0000, 0x0000},/*VC3*/
	/* cap = pre mode setting */
	{0x02, 0x0a, 0x00, 0x08, 0x40, 0x00,
	 0x00, 0x2B, 0x0FA0, 0x0BB8,/*VC0, RAW10(0x2B), 4000x3000 pixel*/
	 0x00, 0x00, 0x0000, 0x0000,/*VC1*/
	 0x01, 0x2B, 0x0FA0, 0x02EE,/*VC2, 4000x750 pixel -> 5000x750 byte*/
	 0x00, 0x00, 0x0000, 0x0000},/*VC3*/
	/* Video mode setting */
	{0x02, 0x0a, 0x00, 0x08, 0x40, 0x00,
	 0x00, 0x2B, 0x0FA0, 0x08CC,/*VC0, RAW10(0x2B), 4000x2252 pixel*/
	 0x00, 0x00, 0x0000, 0x0000,/*VC1*/
	 0x01, 0x2B, 0x0FA0, 0x0232,/*VC2, 4000x562 pixel -> 5000x562 byte*/
	 0x00, 0x00, 0x0000, 0x0000},/*VC3*/
	/* hs mode setting */
	{0x02, 0x0a, 0x00, 0x08, 0x40, 0x00,
	 0x00, 0x2B, 0x0500, 0x02D0,/*VC0, RAW10(0x2B), 1280x720 pixel*/
	 0x00, 0x00, 0x0000, 0x0000,/*VC1*/
	 0x01, 0x2B, 0x0500, 0x00B4,/*VC2, 1280x180 pixel -> 1600x180 byte*/
	 0x00, 0x00, 0x0000, 0x0000},/*VC3*/
	/* custom1 mode setting */
	{0x01, 0x0a, 0x00, 0x08, 0x40, 0x00,
	 0x00, 0x2B, 0x07D0, 0x05DC,/*VC0, RAW10(0x2B), 2000x1500 pixel*/
	 0x00, 0x00, 0x0000, 0x0000,/*VC1*/
	 0x00, 0x00, 0x0000, 0x0000,/*VC2*/
	 0x00, 0x00, 0x0000, 0x0000},/*VC3*/
	/* custom2 mode setting */
	{0x01, 0x0a, 0x00, 0x08, 0x40, 0x00,
	 0x00, 0x2B, 0x2EE0, 0x2328,/*VC0, RAW10(0x2B), 12000x9000 pixel*/
	 0x00, 0x00, 0x0000, 0x0000,/*VC1*/
	 0x00, 0x00, 0x0000, 0x0000,/*VC2*/
	 0x00, 0x00, 0x0000, 0x0000},/*VC3*/
	/* custom3 mode setting */
	{0x01, 0x0a, 0x00, 0x08, 0x40, 0x00,
	 0x00, 0x2B, 0x0FA0, 0x0BB8,/*VC0, RAW10(0x2B), 4000x3000 pixel*/
	 0x00, 0x00, 0x0000, 0x0000,/*VC1*/
	 0x00, 0x00, 0x0000, 0x0000,/*VC2*/
	 0x00, 0x00, 0x0000, 0x0000},/*VC3*/
	/* custom4 mode setting */
	{0x02, 0x0a, 0x00, 0x08, 0x40, 0x00,
	 0x00, 0x2C, 0x0FA0, 0x08CC,/*VC0, RAW12(0x2C), 4000x2252 pixel*/
	 0x00, 0x00, 0x0000, 0x0000,/*VC1*/
	 0x01, 0x2B, 0x0FA0, 0x0232,/*VC2, 4000x562 pixel -> 5000x562 byte*/
	 0x00, 0x00, 0x0000, 0x0000},/*VC3*/
	/* custom5 mode setting */
	{0x02, 0x0a, 0x00, 0x08, 0x40, 0x00,
	 0x00, 0x2C, 0x0FA0, 0x0BB8,/*VC0, RAW12(0x2C), 4000x3000 pixel*/
	 0x00, 0x00, 0x0000, 0x0000,/*VC1*/
	 0x01, 0x2B, 0x0FA0, 0x02EE,/*VC2, 4000x750 pixel -> 5000x750 byte*/
	 0x00, 0x00, 0x0000, 0x0000},/*VC3*/
	/* custom6 mode setting */
	{0x02, 0x0a, 0x00, 0x08, 0x40, 0x00,
	 0x00, 0x2B, 0x0FA0, 0x08CC,/*VC0, RAW10(0x2B), 4000x2252 pixel*/
	 0x00, 0x00, 0x0000, 0x0000,/*VC1*/
	 0x01, 0x2B, 0x0FA0, 0x0232,/*VC2, 4000x562 pixel -> 5000x562 byte*/
	 0x00, 0x00, 0x0000, 0x0000},/*VC3*/
};

static struct SENSOR_WINSIZE_INFO_STRUCT imgsensor_winsize_info[] = {
	{12024, 9024,    24,   24, 12000, 9000,  4000, 3000,
	     0,    0,  4000, 3000,     0,    0,  4000, 3000}, /* Preview */
	{12024, 9024,    24,   24, 12000, 9000,  4000, 3000,
	     0,    0,  4000, 3000,     0,    0,  4000, 3000}, /* capture */
	{12024, 9024,    24,   24, 12000, 9000,  4000, 3000,
	     0,  374,  4000, 2252,     0,    0,  4000, 2252}, /* video */
	{12024, 9024,    24,   24, 12000, 9000,  2000, 1500,
	   360,  390,  1280,  720,     0,    0,  1280,  720}, /* hs video */
	{12024, 9024,    24,   24, 12000, 9000,  4000, 3000,
	     0,    0,  4000, 3000,     0,    0,  4000, 3000}, /* slim video - NOT USED */
	{12024, 9024,    24,   24, 12000, 9000,  2000, 1500,
	     0,    0,  2000, 1500,     0,    0,  2000, 1500}, /* custom1 */
	{12024, 9024,    24,   24, 12000, 9000, 12000, 9000,
	     0,    0, 12000, 9000,     0,    0, 12000, 9000}, /* custom2 */
	{12024, 9024,  4008, 3024,  4008, 3024,  4008, 3024,
	     4,   12,  4000, 3000,     0,    0,  4000, 3000}, /* custom3 */
	{12024, 9024,    24,   24, 12000, 9000,  4000, 3000,
	     0,  374,  4000, 2252,     0,    0,  4000, 2252}, /* custom4 - RAW12 16:9 video */
	{12024, 9024,    24,   24, 12000, 9000,  4000, 3000,
	     0,    0,  4000, 3000,     0,    0,  4000, 3000}, /* custom5 - RAW12 4:3 video */
	{12024, 9024,    24,   24, 12000, 9000,  4000, 3000,
	     0,  374,  4000, 2252,     0,    0,  4000, 2252}, /* custom6 */
};

static struct SET_PD_BLOCK_INFO_T imgsensor_pd_info = {
		.i4OffsetX   = 0,
		.i4OffsetY   = 0,
		.i4PitchX    = 4,
		.i4PitchY    = 4,
		.i4PairNum   = 2,
		.i4SubBlkW   = 2,
		.i4SubBlkH   = 4,
		.i4BlockNumX = 1000,
		.i4BlockNumY = 750,
		.iMirrorFlip = 0,
		.i4PosR = {
			{0, 0}, {2, 0},
		},
		.i4PosL = {
			{1, 0}, {3, 0},
		},
		.i4Crop = {
			{0, 0}, {0, 0}, {0, 374}, {360, 390}, {0, 0},
			{0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0},
			{0, 374}
		}
};

int write_cmos_sensor(kal_uint16 addr, kal_uint16 para)
{
	char pusendcmd[4] = {
		(char)(addr >> 8), (char)(addr & 0xFF),
		(char)(para >> 8), (char)(para & 0xFF) };
	int ret = 0;

	ret = iWriteRegI2CTiming(pusendcmd, 4,
		imgsensor.i2c_write_id, imgsensor_info.i2c_speed);
	return ret;
}

/*
static kal_uint16 read_cmos_sensor(kal_uint16 addr)
{
	kal_uint16 get_byte = 0;
	char pusendcmd[2] = { (char)(addr >> 8), (char)(addr & 0xFF) };

	iReadRegI2CTiming(pusendcmd, 2, (u8 *) &get_byte, 2,
		imgsensor.i2c_write_id, imgsensor_info.i2c_speed);
	return get_byte;
}
*/

static kal_uint16 read_cmos_sensor_8(kal_uint16 addr)
{
	kal_uint16 get_byte = 0;
	char pusendcmd[2] = { (char)(addr >> 8), (char)(addr & 0xFF) };

	iReadRegI2CTiming(pusendcmd, 2, (u8 *) &get_byte, 1,
		imgsensor.i2c_write_id, imgsensor_info.i2c_speed);
	return get_byte;
}

static kal_uint16 write_cmos_sensor_8(kal_uint16 addr, kal_uint8 para)
{
	int ret = 0;
	char pusendcmd[3] = {
		(char)(addr >> 8), (char)(addr & 0xFF), (char)(para & 0xFF) };

	ret = iWriteRegI2CTiming(pusendcmd, 3,
		imgsensor.i2c_write_id, imgsensor_info.i2c_speed);
	return ret;
}

static void set_dummy(void)
{
	LOG_DBG("dummyline = %d, dummypixels = %d\n",
		imgsensor.dummy_line, imgsensor.dummy_pixel);

	write_cmos_sensor_8(0x702, 0); // FLL Lshift reset
	write_cmos_sensor_8(0x704, 0); // CIT Lshift reset
	write_cmos_sensor(0x0340, imgsensor.frame_length);
	write_cmos_sensor(0x0342, imgsensor.line_length);
}

#if INDIRECT_BURST
static kal_uint16 s5khm6_burst_write_cmos_sensor(kal_uint16 *para, kal_uint32 len)
{
	char puSendCmd[I2C_BURST_BUFFER_LEN + 2];
	kal_uint32 tosend = 0, IDX = 0;
	kal_uint16 addr = 0, data = 0;

	while (len > IDX) {
		addr = para[IDX];
		if (tosend == 0) {
			puSendCmd[tosend++] = (char)(addr >> 8);
			puSendCmd[tosend++] = (char)(addr & 0xFF);
		}

		data = para[IDX + 1];
		puSendCmd[tosend++] = (char)(data >> 8);
		puSendCmd[tosend++] = (char)(data & 0xFF);
		IDX += 2;

		if ((len == IDX) ||
			(tosend >= I2C_BURST_BUFFER_LEN) ||
			((len > IDX) && (para[IDX] != addr))) {
			iBurstWriteReg_multi(puSendCmd, tosend,
								imgsensor.i2c_write_id,
								tosend, imgsensor_info.i2c_speed);
			tosend = 0;
		}
	}
	return 0;
}
#endif

static kal_uint16 table_write_cmos_sensor(kal_uint16 *para, kal_uint32 len)
{
	char puSendCmd[I2C_BUFFER_LEN];
	kal_uint32 tosend, IDX;
	kal_uint16 addr = 0, addr_last = 0, data;

	tosend = 0;
	IDX = 0;

	while (len > IDX) {
		addr = para[IDX];

		{
			puSendCmd[tosend++] = (char)(addr >> 8);
			puSendCmd[tosend++] = (char)(addr & 0xFF);
			data = para[IDX + 1];
			puSendCmd[tosend++] = (char)(data >> 8);
			puSendCmd[tosend++] = (char)(data & 0xFF);
			IDX += 2;
			addr_last = addr;

		}
#if MULTI_WRITE

	if ((I2C_BUFFER_LEN - tosend) < 4 || IDX == len || addr != addr_last) {
		iBurstWriteReg_multi(
		puSendCmd, tosend, imgsensor.i2c_write_id, 4,
				     imgsensor_info.i2c_speed);
		tosend = 0;
	}
#else
		iWriteRegI2CTiming(puSendCmd, 4,
			imgsensor.i2c_write_id, imgsensor_info.i2c_speed);

		tosend = 0;
#endif
	}
	return 0;
}

static void set_max_framerate(UINT16 framerate, kal_bool min_framelength_en)
{
	kal_uint32 frame_length = imgsensor.frame_length;

	LOG_DBG("framerate = %d, min framelength should enable %d\n",
		framerate, min_framelength_en);

	frame_length = imgsensor.pclk / framerate * 10 / imgsensor.line_length;
	spin_lock(&imgsensor_drv_lock);
	if (frame_length >= imgsensor.min_frame_length)
		imgsensor.frame_length = frame_length;
	else
		imgsensor.frame_length = imgsensor.min_frame_length;

	imgsensor.dummy_line =
		imgsensor.frame_length - imgsensor.min_frame_length;

	if (imgsensor.frame_length > imgsensor_info.max_frame_length) {
		imgsensor.frame_length = imgsensor_info.max_frame_length;

		imgsensor.dummy_line =
			imgsensor.frame_length - imgsensor.min_frame_length;
	}
	if (min_framelength_en)
		imgsensor.min_frame_length = imgsensor.frame_length;
	spin_unlock(&imgsensor_drv_lock);
	set_dummy();
}

static int update_mipi_info(enum MSDK_SCENARIO_ID_ENUM scenario_id)
{
	LOG_DBG("- E\n");

	adaptive_mipi_index = imgsensor_select_mipi_by_rf_channel(s5khm6_mipi_channel_full,
						ARRAY_SIZE(s5khm6_mipi_channel_full));
	if (adaptive_mipi_index < 0) {
		LOG_ERR("Not found mipi index");
		return -EIO;
	}

	LOG_DBG("pclk(%d), mipi_pixel_rate(%d), framelength(%d)\n",
		imgsensor_info.pre.pclk, imgsensor_info.pre.mipi_pixel_rate, imgsensor_info.pre.framelength);

	if (adaptive_mipi_index == CAM_HM6_SET_A_AtoD_2021p5_MHZ) {
		LOG_INF("Adaptive Mipi (2021.5 MHz) : mipi_pixel_rate (4043 Mbps)");
		if (scenario_id == MSDK_SCENARIO_ID_CAMERA_PREVIEW) {
			imgsensor_info.pre.mipi_pixel_rate = 606450000;
		} else if (scenario_id == MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG) {
			imgsensor_info.cap.mipi_pixel_rate = 606450000;
		} else if (scenario_id == MSDK_SCENARIO_ID_VIDEO_PREVIEW) {
			imgsensor_info.normal_video.mipi_pixel_rate = 606450000;
		} else if (scenario_id == MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO) {
			imgsensor_info.hs_video.mipi_pixel_rate = 606450000;
		} else if (scenario_id == MSDK_SCENARIO_ID_CUSTOM1) {
			imgsensor_info.custom1.mipi_pixel_rate = 606450000;
		} else if (scenario_id == MSDK_SCENARIO_ID_CUSTOM2) {
			imgsensor_info.custom2.mipi_pixel_rate = 606450000;
		} else if (scenario_id == MSDK_SCENARIO_ID_CUSTOM6) {
			imgsensor_info.custom6.mipi_pixel_rate = 606450000;
		} else {
			LOG_ERR("Wrong Scenario id : %d", scenario_id);
			return -EIO;
		}
	} else if (adaptive_mipi_index == CAM_HM6_SET_A_AtoD_2073p5_MHZ) {
		LOG_INF("Adaptive Mipi (2073.5 MHz) : mipi_pixel_rate (4147 Mbps)");
		if (scenario_id == MSDK_SCENARIO_ID_CAMERA_PREVIEW) {
			imgsensor_info.pre.mipi_pixel_rate = 622050000;
		} else if (scenario_id == MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG) {
			imgsensor_info.cap.mipi_pixel_rate = 622050000;
		} else if (scenario_id == MSDK_SCENARIO_ID_VIDEO_PREVIEW) {
			imgsensor_info.normal_video.mipi_pixel_rate = 622050000;
		} else if (scenario_id == MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO) {
			imgsensor_info.hs_video.mipi_pixel_rate = 622050000;
		} else if (scenario_id == MSDK_SCENARIO_ID_CUSTOM1) {
			imgsensor_info.custom1.mipi_pixel_rate = 622050000;
		} else if (scenario_id == MSDK_SCENARIO_ID_CUSTOM2) {
			imgsensor_info.custom2.mipi_pixel_rate = 622050000;
		} else if (scenario_id == MSDK_SCENARIO_ID_CUSTOM6) {
			imgsensor_info.custom6.mipi_pixel_rate = 622050000;
		} else {
			LOG_ERR("Wrong Scenario id : %d", scenario_id);
			return -EIO;
		}
	} else {//CAM_HM6_SET_A_AtoD_2151p5_MHZ
		LOG_INF("Adaptive Mipi (2151.5 MHz) : mipi_pixel_rate (4303 Mbps)");
		if (scenario_id == MSDK_SCENARIO_ID_CAMERA_PREVIEW) {
			imgsensor_info.pre.mipi_pixel_rate = 645450000;
		} else if (scenario_id == MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG) {
			imgsensor_info.cap.mipi_pixel_rate = 645450000;
		} else if (scenario_id == MSDK_SCENARIO_ID_VIDEO_PREVIEW) {
			imgsensor_info.normal_video.mipi_pixel_rate = 645450000;
		} else if (scenario_id == MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO) {
			imgsensor_info.hs_video.mipi_pixel_rate = 645450000;
		} else if (scenario_id == MSDK_SCENARIO_ID_CUSTOM1) {
			imgsensor_info.custom1.mipi_pixel_rate = 645450000;
		} else if (scenario_id == MSDK_SCENARIO_ID_CUSTOM2) {
			imgsensor_info.custom2.mipi_pixel_rate = 645450000;
		} else if (scenario_id == MSDK_SCENARIO_ID_CUSTOM6) {
			imgsensor_info.custom6.mipi_pixel_rate = 645450000;
		} else {
			LOG_ERR("Wrong Scenario id : %d", scenario_id);
			return -EIO;
		}
	}

	return adaptive_mipi_index;
}

static int set_mipi_mode(int mipi_index)
{
	LOG_DBG("- E");
	if (mipi_index == CAM_HM6_SET_A_AtoD_2073p5_MHZ) {
		LOG_INF("Set adaptive Mipi as 2073.5 MHz");
		table_write_cmos_sensor(s5khm6_mipi_full_2073p5mhz,
			ARRAY_SIZE(s5khm6_mipi_full_2073p5mhz));
	} else if (mipi_index == CAM_HM6_SET_A_AtoD_2151p5_MHZ) {
		LOG_INF("Set adaptive Mipi as 2151.5 MHz");
		table_write_cmos_sensor(s5khm6_mipi_full_2151p5mhz,
			ARRAY_SIZE(s5khm6_mipi_full_2151p5mhz));
	} else {//CAM_HM6_SET_A_AtoD_2021p5_MHZ is set as default, so no need to set.
		LOG_INF("Set adaptive Mipi as 2021.5 MHz");
	}
	LOG_DBG("- X");
	return ERROR_NONE;
}


/*
 * Read sensor frame counter (sensor_fcount address = 0x0005)
 * stream on (0x00 ~ 0xFE), stream off (0xFF)
 */
static void wait_stream_onoff_range(kal_uint32 addr, kal_uint32 min_val, kal_uint32 max_val)
{
	kal_uint8 read_val = 0;
	kal_uint16 read_addr = 0;
	kal_uint32 max_cnt = 0, cur_cnt = 0;
	unsigned long wait_delay_ms = 0;

	read_addr = addr;
	wait_delay_ms = 2;
	max_cnt = 100;
	cur_cnt = 0;

	read_val = read_cmos_sensor_8(read_addr);

	while (read_val < min_val || read_val > max_val) {
		mDELAY(wait_delay_ms);
		read_val = read_cmos_sensor_8(read_addr);
		cur_cnt++;

		if (cur_cnt >= max_cnt) {
			LOG_ERR("stream timeout: %d ms\n", (max_cnt * wait_delay_ms));
			break;
		}
	}
	LOG_INF("wait time: %d ms\n", (cur_cnt * wait_delay_ms));
}

static kal_uint32 streaming_control(kal_bool enable)
{
	LOG_INF("streaming_enable(0=Sw Standby,1=streaming): %d\n", enable);

	if (enable) {
		set_mipi_mode(adaptive_mipi_index);
		write_cmos_sensor(0x0100, 0x0100);

		if (imgsensor.current_scenario_id == MSDK_SCENARIO_ID_CUSTOM1) {
			wait_stream_onoff_range(0x0005, 0x01, 0xFE);
		}
	} else {
		write_cmos_sensor(0xFCFC, 0x4000);
		write_cmos_sensor(0x0100, 0x0000);

		//only wait stream_off
		wait_stream_onoff_range(0x0005, 0xFF, 0xFF);
	}
	return ERROR_NONE;
}

static void write_shutter(kal_uint32 shutter)
{
	kal_uint32 realtime_fps = 0;
	kal_uint32 cit_lshift_val = 0;
	kal_uint32 cit_lshift_count = 0;

	kal_uint32 pre_frame_length = 0;

	pre_frame_length = imgsensor.frame_length;

	spin_lock(&imgsensor_drv_lock);
	if (shutter > imgsensor.min_frame_length - imgsensor_info.margin) //max shutter value: 1919754(0x001D4B0A)
		imgsensor.frame_length = shutter + imgsensor_info.margin;
	else
		imgsensor.frame_length = imgsensor.min_frame_length;

	if (imgsensor.frame_length > imgsensor_info.max_frame_length)
		imgsensor.frame_length = imgsensor_info.max_frame_length;
	spin_unlock(&imgsensor_drv_lock);
	shutter = (shutter < imgsensor_info.min_shutter) ? imgsensor_info.min_shutter : shutter;
	shutter = (shutter > (imgsensor_info.max_frame_length - imgsensor_info.margin)) ? (imgsensor_info.max_frame_length - imgsensor_info.margin) : shutter;

	// frame_length = pclk / (line_length * fps)
	if (sNightHyperlapseSpeed)
		imgsensor.frame_length = ((imgsensor.pclk / 30) * sNightHyperlapseSpeed) / imgsensor.line_length;

	if (imgsensor.frame_length > SENSOR_HM6_MAX_COARSE_INTEGRATION_TIME) {
		int max_coarse_integration_time =
			SENSOR_HM6_MAX_COARSE_INTEGRATION_TIME - SENSOR_HM6_COARSE_INTEGRATION_TIME_MAX_MARGIN;

		cit_lshift_val = (kal_uint32)(imgsensor.frame_length / SENSOR_HM6_MAX_COARSE_INTEGRATION_TIME);
		while (cit_lshift_val >= 1) {
			cit_lshift_val /= 2;
			imgsensor.frame_length /= 2;
			shutter /= 2;
			cit_lshift_count++;
		}
		if (cit_lshift_count > 11)
			cit_lshift_count = 11;

		if (shutter > max_coarse_integration_time)
			shutter = max_coarse_integration_time;

		write_cmos_sensor(0x0340, imgsensor.frame_length & 0xFFFF);
		write_cmos_sensor_8(0x702, cit_lshift_count & 0xFF); // FLL Lshift addr
		write_cmos_sensor_8(0x704, cit_lshift_count & 0xFF); // CIT Lshift addr
	} else {
		if (imgsensor.autoflicker_en) {
			realtime_fps = imgsensor.pclk /
			imgsensor.line_length * 10 / imgsensor.frame_length;

			if (realtime_fps >= 297 && realtime_fps <= 305)
				set_max_framerate(296, 0);
			else if (realtime_fps >= 147 && realtime_fps <= 150)
				set_max_framerate(146, 0);
			else {
				write_cmos_sensor(0x0340, imgsensor.frame_length & 0xFFFF);
				write_cmos_sensor_8(0x702, 0); // FLL Lshift reset
				write_cmos_sensor_8(0x704, 0); // CIT Lshift reset
			}
		} else {
			write_cmos_sensor(0x0340, imgsensor.frame_length & 0xFFFF);
			write_cmos_sensor_8(0x702, 0); // FLL Lshift reset
			write_cmos_sensor_8(0x704, 0); // CIT Lshift reset
		}
	}
	write_cmos_sensor(0x0202, shutter & 0xFFFF);

	if (pre_frame_length != imgsensor.frame_length || cit_lshift_count > 0) {
		LOG_INF("Exit! shutter =%d, framelength =%d, cit_lshift: x%d\n", shutter, imgsensor.frame_length, 0x1 << cit_lshift_count);
	}
}

static void set_shutter_frame_length(kal_uint32 shutter,
			kal_uint32 frame_length)
{
	unsigned long flags;
	kal_uint16 realtime_fps = 0;
	kal_int32 dummy_line = 0;

	spin_lock_irqsave(&imgsensor_drv_lock, flags);
	imgsensor.shutter = shutter;
	spin_unlock_irqrestore(&imgsensor_drv_lock, flags);

	spin_lock(&imgsensor_drv_lock);
	/*Change frame time*/
	dummy_line = frame_length - imgsensor.frame_length;
	imgsensor.frame_length = imgsensor.frame_length + dummy_line;
	imgsensor.min_frame_length = imgsensor.frame_length;
	//
	if (shutter > imgsensor.min_frame_length - imgsensor_info.margin)
		imgsensor.frame_length = shutter + imgsensor_info.margin;
	else
		imgsensor.frame_length = imgsensor.min_frame_length;

	if (imgsensor.frame_length > imgsensor_info.max_frame_length)
		imgsensor.frame_length = imgsensor_info.max_frame_length;

	spin_unlock(&imgsensor_drv_lock);
	shutter = (shutter < imgsensor_info.min_shutter) ?
		imgsensor_info.min_shutter : shutter;
	shutter = (shutter > (imgsensor_info.max_frame_length -
		imgsensor_info.margin)) ? (imgsensor_info.max_frame_length -
		imgsensor_info.margin) : shutter;

	if (imgsensor.autoflicker_en) {
		realtime_fps = imgsensor.pclk /
			imgsensor.line_length * 10 / imgsensor.frame_length;
		if (realtime_fps >= 297 && realtime_fps <= 305)
			set_max_framerate(296, 0);
		else if (realtime_fps >= 147 && realtime_fps <= 150)
			set_max_framerate(146, 0);
		else {
			// Extend frame length
			write_cmos_sensor(0x0340,
				imgsensor.frame_length & 0xffff);
		}
	} else {
		// Extend frame length
		write_cmos_sensor(0x0340, imgsensor.frame_length & 0xffff);
	}

	// Update Shutter
	write_cmos_sensor(0x0202, shutter & 0xffff);
	LOG_DBG("Add for N3D! shutter =%d, framelength =%d\n",
		shutter, imgsensor.frame_length);
}


/*************************************************************************
 * FUNCTION
 *	set_shutter
 *
 * DESCRIPTION
 *	This function set e-shutter of sensor to change exposure time.
 *
 * PARAMETERS
 *	iShutter : exposured lines
 *
 * RETURNS
 *	None
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/
static void set_shutter(kal_uint32 shutter)
{
	unsigned long flags;

	spin_lock_irqsave(&imgsensor_drv_lock, flags);
	imgsensor.shutter = shutter;
	spin_unlock_irqrestore(&imgsensor_drv_lock, flags);

	write_shutter(shutter);
}

static kal_uint16 gain2reg(const kal_uint16 gain)
{
	kal_uint16 reg_gain = 0x0;

	reg_gain = gain / 2;
	return (kal_uint16) reg_gain;
}

static kal_uint16 get_max_gain_step(enum IMGSENSOR_MODE mode)
{
	kal_uint16 max_gain_step = 64;
	switch (mode)
	{
	case IMGSENSOR_MODE_CUSTOM2:
		max_gain_step = 16;
		break;

	default:
		break;
	}

	return max_gain_step;
}
/*************************************************************************
 * FUNCTION
 *	set_gain
 *
 * DESCRIPTION
 *	This function is to set global gain to sensor.
 *
 * PARAMETERS
 *	iGain : sensor global gain(base: 0x40)
 *
 * RETURNS
 *	the actually gain set to sensor.
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/
static kal_uint16 set_gain(kal_uint16 gain)
{
	kal_uint16 reg_gain;
	int max_gain_step = 64;

	max_gain_step = get_max_gain_step(imgsensor.sensor_mode);

	if (gain < BASEGAIN || gain > max_gain_step * BASEGAIN) {
		LOG_ERR("Error gain setting");
		if (gain < BASEGAIN)
			gain = BASEGAIN;
		else if (gain > max_gain_step * BASEGAIN)
			gain = max_gain_step * BASEGAIN;
	}

	reg_gain = gain2reg(gain);
	spin_lock(&imgsensor_drv_lock);
	imgsensor.gain = reg_gain;
	spin_unlock(&imgsensor_drv_lock);
	LOG_DBG("gain = %d , reg_gain = 0x%x\n ", gain, reg_gain);

	write_cmos_sensor(0x0204, (reg_gain&0xFFFF));
	return gain;
}

static void set_mirror_flip(kal_uint8 image_mirror)
{
#if 0
	kal_uint8 itemp;

	LOG_INF("image_mirror = %d\n", image_mirror);
	itemp = read_cmos_sensor_8(0x0101);
	itemp &= ~0x03;

	switch (image_mirror) {

	case IMAGE_NORMAL:
		write_cmos_sensor_8(0x0101, itemp);
		break;

	case IMAGE_H_MIRROR:
		write_cmos_sensor_8(0x0101, itemp | 0x01);
		break;

	case IMAGE_V_MIRROR:
		write_cmos_sensor_8(0x0101, itemp | 0x02);
		break;

	case IMAGE_HV_MIRROR:
		write_cmos_sensor_8(0x0101, itemp | 0x03);
		break;
	}
#endif
}

// Check module revision addr: 0x0002, val: 0xA0(EVT0), 0xB0(EVT1), 0xB1(OTP)
static bool has_otp(void)
{
	unsigned short module_rev;

	write_cmos_sensor(0xFCFC, 0x4000);
	module_rev = read_cmos_sensor_8(0x0002);
	LOG_INF("module rev 0x%x", module_rev);
	if (module_rev != 0xA0 && module_rev != 0xB0)
		return true;

	return false;
}

#ifdef USE_RAW12
static void fastchange_set_mode(enum IMGSENSOR_FC_SET_MODE mode)
{
	switch (mode) {
	case IMGSENSOR_FC_SET_MODE_IDCG:
		LOG_INF("mode: IMGSENSOR_FC_SET_MODE_IDCG");
		write_cmos_sensor(0xFCFC, 0x4000);
		write_cmos_sensor(0x0B30, 0x0100);
		break;
	case IMGSENSOR_FC_SET_MODE_LN4:
		LOG_INF("mode: IMGSENSOR_FC_SET_MODE_LN4");
		write_cmos_sensor(0xFCFC, 0x4000);
		write_cmos_sensor(0x0B30, 0x0101);
		break;
	case IMGSENSOR_FC_SET_MODE_DISABLE:
		LOG_INF("mode: IMGSENSOR_FC_SET_MODE_DISABLE");
		write_cmos_sensor(0xFCFC, 0x4000);
		write_cmos_sensor(0x0B30, 0x01FF);
		break;
	default:
		break;
	}
}

static void fastchange_set_addr(void)
{
	kal_uint16 addr_index0, addr_index1, addr_index2;

	LOG_INF("E");

	addr_index0 =  0x8300;
	addr_index1 =  0x9376;
	addr_index2 =  0xA3EC;

	write_cmos_sensor(0xFCFC, 0x2000);
	write_cmos_sensor(0x1BE8, 0x0001);
	write_cmos_sensor(0x1BEC, 0x2002);

	write_cmos_sensor(0x1BEE, addr_index0);
	write_cmos_sensor(0x1BF0, addr_index1);
	write_cmos_sensor(0x1BF4, addr_index2);

	write_cmos_sensor(0xFCFC, 0x4000);
	write_cmos_sensor(0x0B30, 0x01FF);
	write_cmos_sensor(0x6000, 0x0085);

	LOG_INF("X");
}

static void fastchange_init(enum IMGSENSOR_MODE mode)
{
	LOG_INF("E");
	if (mode == IMGSENSOR_MODE_CUSTOM4 && cur_fcm_sram != IMGSENSOR_FCM_SRAM_16_9) {
		LOG_INF("write FC mode: %s\n", s5khm6_setfile_fc_otp[IMGSENSOR_FC_MODE_16_9_IDCG].name);
		s5khm6_burst_write_cmos_sensor(s5khm6_setfile_fc_otp[IMGSENSOR_FC_MODE_16_9_IDCG].setfile,
			s5khm6_setfile_fc_otp[IMGSENSOR_FC_MODE_16_9_IDCG].size);
		LOG_INF("write FC mode: %s\n", s5khm6_setfile_fc_otp[IMGSENSOR_FC_MODE_16_9_LN4].name);
		s5khm6_burst_write_cmos_sensor(s5khm6_setfile_fc_otp[IMGSENSOR_FC_MODE_16_9_LN4].setfile,
			s5khm6_setfile_fc_otp[IMGSENSOR_FC_MODE_16_9_LN4].size);

		fastchange_set_addr();
		cur_fcm_sram = IMGSENSOR_FCM_SRAM_16_9;
		LOG_INF("FCM SRAM 16:9 setting");
	} else if (mode == IMGSENSOR_MODE_CUSTOM5 && cur_fcm_sram != IMGSENSOR_FCM_SRAM_4_3) {
		LOG_INF("write FC mode: %s\n", s5khm6_setfile_fc_otp[IMGSENSOR_FC_MODE_4_3_IDCG].name);
		s5khm6_burst_write_cmos_sensor(s5khm6_setfile_fc_otp[IMGSENSOR_FC_MODE_4_3_IDCG].setfile,
			s5khm6_setfile_fc_otp[IMGSENSOR_FC_MODE_4_3_IDCG].size);
		LOG_INF("write FC mode: %s\n", s5khm6_setfile_fc_otp[IMGSENSOR_FC_MODE_4_3_LN4].name);
		s5khm6_burst_write_cmos_sensor(s5khm6_setfile_fc_otp[IMGSENSOR_FC_MODE_4_3_LN4].setfile,
			s5khm6_setfile_fc_otp[IMGSENSOR_FC_MODE_4_3_LN4].size);

		fastchange_set_addr();
		cur_fcm_sram = IMGSENSOR_FCM_SRAM_4_3;
		LOG_INF("FCM SRAM 4:3 setting");
	}
	LOG_INF("X");
}
#endif

static void set_mode_setfile(enum IMGSENSOR_MODE mode)
{
	if (mode >= IMGSENSOR_MODE_MAX) {
		LOG_ERR("invalid mode: %d\n", mode);
		return;
	}

#if WRITE_SENSOR_CAL_XTC
	if ((mode == IMGSENSOR_MODE_CUSTOM2 || mode == IMGSENSOR_MODE_CUSTOM3) && mIsFullXTCCal == KAL_FALSE) {
		if (has_otp())
			sensor_xtc_write_otp(true);
		else
			sensor_xtc_write(true);
		mIsFullXTCCal = KAL_TRUE;
	} else if (mode != IMGSENSOR_MODE_INIT && mIsPartialXTCCal == KAL_FALSE) {
		if (has_otp())
			sensor_xtc_write_otp(false);
		else
			sensor_xtc_write(false);
		mIsPartialXTCCal = KAL_TRUE;
	}
#endif

	LOG_INF(" - E");

#ifdef USE_RAW12
	if (has_otp() &&
		(mode == IMGSENSOR_MODE_CUSTOM4 || mode == IMGSENSOR_MODE_CUSTOM5)) {
		fastchange_init(mode);
		//fastchange_set_addr();
		fastchange_set_mode(IMGSENSOR_FC_SET_MODE_IDCG);
	} else {
#endif
		if (has_otp()) {
#ifdef USE_RAW12
			fastchange_set_mode(IMGSENSOR_FC_SET_MODE_DISABLE);
#endif
			LOG_INF("mode: %s\n", s5khm6_setfile_info_otp[mode].name);
			table_write_cmos_sensor(s5khm6_setfile_info_otp[mode].setfile,
				s5khm6_setfile_info_otp[mode].size);
		} else {
			LOG_INF("mode: %s\n", s5khm6_setfile_info[mode].name);
			table_write_cmos_sensor(s5khm6_setfile_info[mode].setfile,
				s5khm6_setfile_info[mode].size);
		}
#ifdef USE_RAW12
	}
#endif
	LOG_INF(" - X");
}

static void sensor_init(void)
{
	int ret = 0;
#ifdef IMGSENSOR_HW_PARAM
	struct cam_hw_param *hw_param = NULL;
#endif

	LOG_INF(" - E");

	ret = write_cmos_sensor(0xFCFC, 0x4000);
	ret |= write_cmos_sensor(0x6010, 0x0001);

	//15 ms delay
	mDELAY(15);
#if INDIRECT_BURST
	/* Start burst mode */
	//divide burst data and single data
	if (has_otp()) {
		ret |= s5khm6_burst_write_cmos_sensor(addr_data_burst_init_s5khm6_otp,
			sizeof(addr_data_burst_init_s5khm6_otp) / sizeof(kal_uint16));
		set_mode_setfile(IMGSENSOR_MODE_INIT);
	} else {
		ret |= s5khm6_burst_write_cmos_sensor(addr_data_burst_init_s5khm6,
			sizeof(addr_data_burst_init_s5khm6) / sizeof(kal_uint16));
		set_mode_setfile(IMGSENSOR_MODE_INIT);
	}
#else
	set_mode_setfile(IMGSENSOR_MODE_INIT);
#endif

#ifdef IMGSENSOR_HW_PARAM
	if (ret != 0) {
		imgsensor_sec_get_hw_param(&hw_param, S5KHM6_CAL_SENSOR_POSITION);
		if (hw_param)
			hw_param->i2c_sensor_err_cnt++;
	}
#endif
	LOG_INF("- X");
}

#if WRITE_SENSOR_CAL_XTC
#define UXTC_DUMP_CAL_DATA                  (0)
static const int XTC_I2C_BURST_BUFFER_LEN = 1400;

static kal_uint16 xtc_burst_write_cmos_sensor(kal_uint16 addr, kal_uint8 *para, kal_uint32 len)
{

	char puSendCmd[XTC_I2C_BURST_BUFFER_LEN + 2];
	kal_uint32 tosend = 0, IDX = 0;
	kal_uint8 addr_upper = 0, addr_down = 0;

	addr_upper = (addr >> 8) & 0xFF;
	addr_down = addr & 0xFF;

	while (len > IDX) {
		if (tosend == 0) {
			puSendCmd[tosend++] = addr_upper;
			puSendCmd[tosend++] = addr_down;
		}
		puSendCmd[tosend++] = para[IDX++];

		if ((len == IDX) ||
			(tosend >= XTC_I2C_BURST_BUFFER_LEN)) {
			iBurstWriteReg_multi(puSendCmd, tosend,
								imgsensor.i2c_write_id,
								tosend, imgsensor_info.i2c_speed);
			tosend = 0;
		}
	}
	LOG_INF("write done: %d/%d", IDX, len);
	return 0;
}

#if UXTC_DUMP_CAL_DATA == 1
static void dump_xtc_cal_data(char *rom_cal_buf, int data_size)
{
	int i = 0;

	for (i = 0; i < data_size; i += 2)
		LOG_INF("0x%04x", ((rom_cal_buf[i] << 8) & 0xFF00) | (rom_cal_buf[i + 1] & 0XFF));
}
#endif

static void sensor_xtc_write_otp(bool isRemosaic)
{
	const struct rom_cal_addr *xtc_buf = NULL;
	char *rom_cal_buf = NULL;
	int data_size;
	enum crosstalk_cal_name xtc_name;
	int32_t xtc_cal_addr_start = 0;
	// TODO : sensor_dev_id needs to be updated according to actual id.
	int sensor_dev_id = IMGSENSOR_SENSOR_IDX_MAIN;

	LOG_INF("E : remosaic(%d)", isRemosaic);

	if (IMGSENSOR_GET_CAL_BUF_BY_SENSOR_IDX(sensor_dev_id, &rom_cal_buf) < 0) {
		LOG_ERR("rom_cal_buf is NULL");
		return;
	}

	if (isRemosaic)
		xtc_name = CROSSTALK_CAL_XTC;
	else
		xtc_name = CROSSTALK_CAL_PARTIAL_XTC;

	xtc_buf = IMGSENSOR_GET_CAL_ADDR_BY_SENSOR_IDX(sensor_dev_id, GET_CAL_CROSSTALK);
	while (xtc_buf != NULL) {
		if (xtc_buf->name == CROSSTALK_CAL_XTC)
			xtc_cal_addr_start = xtc_buf->addr;

		if (xtc_buf->name == xtc_name)
			break;

		xtc_buf = xtc_buf->next;
	}
	if (xtc_buf == NULL) {
		LOG_ERR("xtc data is NULL, sensor_dev_id: %d", sensor_dev_id);
		return;
	}

	rom_cal_buf += xtc_buf->addr;
	data_size = xtc_buf->size;

	//Prefix_uXTC_cal
	table_write_cmos_sensor(xtc_prefix_hm6_otp, ARRAY_SIZE(xtc_prefix_hm6_otp));

	//EEPROM data
	write_cmos_sensor(0xFCFC, 0x4000);
	write_cmos_sensor(0x6004, 0x0001);
	write_cmos_sensor(0x6028, 0x2023);

	LOG_INF("xtc cal size: %d, read addr: 0x%04x, write addr: 0x%04x", data_size,
		xtc_buf->addr, xtc_buf->addr - xtc_cal_addr_start);
	write_cmos_sensor(0x602A, xtc_buf->addr - xtc_cal_addr_start);
	xtc_burst_write_cmos_sensor(0x6F12, rom_cal_buf, data_size);

#if UXTC_DUMP_CAL_DATA == 1
	dump_xtc_cal_data(rom_cal_buf, data_size);
#endif
	write_cmos_sensor(0xFCFC, 0x4000);
	write_cmos_sensor(0x6004, 0x0000);

	//XTC_FW_TESTVECTOR_GOLDEN
	table_write_cmos_sensor(xtc_fw_testvector_golden_hm6_otp,
		ARRAY_SIZE(xtc_fw_testvector_golden_hm6_otp));

	//Postfix_uXTC_cal
	table_write_cmos_sensor(xtc_postfix_hm6_otp, ARRAY_SIZE(xtc_postfix_hm6_otp));

	write_cmos_sensor(0xFCFC, 0x2001);
	if (isRemosaic)
		write_cmos_sensor(0x3AEC, 0x0000);
	else
		write_cmos_sensor(0x3AEC, 0x0100);

	LOG_INF("X");
}

static void sensor_xtc_write(bool isRemosaic)
{
	const struct rom_cal_addr *xtc_buf = NULL;
	char *rom_cal_buf = NULL;
	int data_size;
	enum crosstalk_cal_name xtc_name;
	int32_t xtc_cal_addr_start = 0;
	// TODO : sensor_dev_id needs to be updated according to actual id.
	int sensor_dev_id = IMGSENSOR_SENSOR_IDX_MAIN;

	LOG_INF("E : remosaic(%d)", isRemosaic);

	if (IMGSENSOR_GET_CAL_BUF_BY_SENSOR_IDX(sensor_dev_id, &rom_cal_buf) < 0) {
		LOG_ERR("rom_cal_buf is NULL");
		return;
	}

	if (isRemosaic)
		xtc_name = CROSSTALK_CAL_XTC;
	else
		xtc_name = CROSSTALK_CAL_PARTIAL_XTC;

	xtc_buf = IMGSENSOR_GET_CAL_ADDR_BY_SENSOR_IDX(sensor_dev_id, GET_CAL_CROSSTALK);
	while (xtc_buf != NULL) {
		if (xtc_buf->name == CROSSTALK_CAL_XTC)
			xtc_cal_addr_start = xtc_buf->addr;

		if (xtc_buf->name == xtc_name)
			break;

		xtc_buf = xtc_buf->next;
	}
	if (xtc_buf == NULL) {
		LOG_ERR("xtc data is NULL, sensor_dev_id: %d", sensor_dev_id);
		return;
	}

	rom_cal_buf += xtc_buf->addr;
	data_size = xtc_buf->size;

	//Prefix_uXTC_cal
	table_write_cmos_sensor(xtc_prefix_hm6, ARRAY_SIZE(xtc_prefix_hm6));

	//EEPROM data
	write_cmos_sensor(0xFCFC, 0x4000);
	write_cmos_sensor(0x6004, 0x0001);
	write_cmos_sensor(0x6028, 0x2023);

	LOG_INF("xtc cal size: %d, read addr: 0x%04x, write addr: 0x%04x", data_size,
		xtc_buf->addr, xtc_buf->addr - xtc_cal_addr_start);
	write_cmos_sensor(0x602A, xtc_buf->addr - xtc_cal_addr_start);
	xtc_burst_write_cmos_sensor(0x6F12, rom_cal_buf, data_size);

#if UXTC_DUMP_CAL_DATA == 1
	dump_xtc_cal_data(rom_cal_buf, data_size);
#endif
	write_cmos_sensor(0xFCFC, 0x4000);
	write_cmos_sensor(0x6004, 0x0000);

	//XTC_FW_TESTVECTOR_GOLDEN
	table_write_cmos_sensor(xtc_fw_testvector_golden_hm6,
		ARRAY_SIZE(xtc_fw_testvector_golden_hm6));

	//Postfix_uXTC_cal
	table_write_cmos_sensor(xtc_postfix_hm6, ARRAY_SIZE(xtc_postfix_hm6));

	write_cmos_sensor(0xFCFC, 0x2001);
	if (isRemosaic)
		write_cmos_sensor(0x3AEC, 0x0000);
	else
		write_cmos_sensor(0x3AEC, 0x0100);

	LOG_INF("X");
}
#endif



/*************************************************************************
 * FUNCTION
 *	get_imgsensor_id
 *
 * DESCRIPTION
 *	This function get the sensor ID
 *
 * PARAMETERS
 *	*sensorID : return the sensor ID
 *
 * RETURNS
 *	None
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/
static kal_uint32 get_imgsensor_id(UINT32 *sensor_id)
{
	kal_uint8 i = 0;
	kal_uint8 retry = 2;

	/* sensor have two i2c address 0x6c 0x6d & 0x21 0x20,
	 *we should detect the module used i2c address
	 */
	while (imgsensor_info.i2c_addr_table[i] != 0xff) {
		spin_lock(&imgsensor_drv_lock);
		imgsensor.i2c_write_id = imgsensor_info.i2c_addr_table[i];
		spin_unlock(&imgsensor_drv_lock);

		do {
			*sensor_id = ((read_cmos_sensor_8(0x0000) << 8) | read_cmos_sensor_8(0x0001));
			if (*sensor_id == imgsensor_info.sensor_id) {
				LOG_INF("i2c write id: 0x%x, sensor id: 0x%x\n", imgsensor.i2c_write_id, *sensor_id);
			return ERROR_NONE;
			}
			LOG_ERR("Read sensor id fail, id: 0x%x, sensor_id: 0x%x\n", imgsensor.i2c_write_id, *sensor_id);
			retry--;
		} while (retry > 0);

		i++;
		retry = 2;
	}

	if (*sensor_id != imgsensor_info.sensor_id) {
		*sensor_id = 0xFFFFFFFF;

		return ERROR_SENSOR_CONNECT_FAIL;
	}
	return ERROR_NONE;
}


/*************************************************************************
 * FUNCTION
 *	open
 *
 * DESCRIPTION
 *	This function initialize the registers of CMOS sensor
 *
 * PARAMETERS
 *	None
 *
 * RETURNS
 *	None
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/
static kal_uint32 open(void)
{
	kal_uint32 sensor_id = 0;
	kal_uint32 ret = ERROR_NONE;

	LOG_INF("E");
	ret = get_imgsensor_id(&sensor_id);
	if (ret != ERROR_NONE)
		return ERROR_SENSOR_CONNECT_FAIL;

	sensor_init();

	mIsPartialXTCCal = KAL_FALSE;
	mIsFullXTCCal = KAL_FALSE;

	spin_lock(&imgsensor_drv_lock);

	imgsensor.autoflicker_en	= KAL_FALSE;
	imgsensor.sensor_mode		= IMGSENSOR_MODE_INIT;
	imgsensor.shutter			= 0x3D0;
	imgsensor.gain				= 0x100;
	imgsensor.pclk				= imgsensor_info.pre.pclk;
	imgsensor.frame_length		= imgsensor_info.pre.framelength;
	imgsensor.line_length		= imgsensor_info.pre.linelength;
	imgsensor.min_frame_length	= imgsensor_info.pre.framelength;
	imgsensor.dummy_pixel		= 0;
	imgsensor.dummy_line		= 0;
	imgsensor.ihdr_mode			= 0;
	imgsensor.test_pattern		= KAL_FALSE;
	imgsensor.current_fps		= imgsensor_info.pre.max_framerate;
	spin_unlock(&imgsensor_drv_lock);

	return ERROR_NONE;
}



/*************************************************************************
 * FUNCTION
 *	close
 *
 * DESCRIPTION
 *
 *
 * PARAMETERS
 *	None
 *
 * RETURNS
 *	None
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/
static kal_uint32 close(void)
{
	LOG_INF("close() E\n");

	streaming_control(false);
	sNightHyperlapseSpeed = 0;
	enable_adaptive_mipi = false;
	mIsPartialXTCCal = KAL_FALSE;
	mIsFullXTCCal = KAL_FALSE;
#ifdef USE_RAW12
	cur_fcm_sram = IMGSENSOR_FCM_SRAM_NOT_SET;
#endif
	return ERROR_NONE;
}

static void get_sensor_status(void)
{
	unsigned short sensor_bit;
	unsigned short sensor_color_pattern;
	sensor_bit = read_cmos_sensor_8(0x000C);
	sensor_color_pattern = read_cmos_sensor_8(0x000D);
	LOG_INF("sensor bit = 0x%x, sensor_color_pattern = 0x%x", sensor_bit, sensor_color_pattern);
}
/*************************************************************************
 * FUNCTION
 * preview
 *
 * DESCRIPTION
 *	This function start the sensor preview.
 *
 * PARAMETERS
 *	*image_window : address pointer of pixel numbers in one period of HSYNC
 *  *sensor_config_data : address pointer of line numbers in one period of VSYNC
 *
 * RETURNS
 *	None
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/
static kal_uint32 preview(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("E, current fps: %d\n", imgsensor.current_fps);

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode			= IMGSENSOR_MODE_PREVIEW;
	imgsensor.pclk					= imgsensor_info.pre.pclk;
	imgsensor.line_length			= imgsensor_info.pre.linelength;
	imgsensor.frame_length			= imgsensor_info.pre.framelength;
	imgsensor.min_frame_length		= imgsensor_info.pre.framelength;
	imgsensor.autoflicker_en		= KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);

	set_mode_setfile(imgsensor.sensor_mode);

	set_mirror_flip(imgsensor.mirror);

	return ERROR_NONE;
}

/*************************************************************************
 * FUNCTION
 *	capture
 *
 * DESCRIPTION
 *	This function setup the CMOS sensor in capture MY_OUTPUT mode
 *
 * PARAMETERS
 *
 * RETURNS
 *	None
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/
static kal_uint32 capture(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("E, current fps: %d\n", imgsensor.current_fps);

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_CAPTURE;
	imgsensor.pclk = imgsensor_info.cap.pclk;
	imgsensor.line_length = imgsensor_info.cap.linelength;
	imgsensor.frame_length = imgsensor_info.cap.framelength;
	imgsensor.min_frame_length = imgsensor_info.cap.framelength;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);

	set_mode_setfile(imgsensor.sensor_mode);

	set_mirror_flip(imgsensor.mirror);

	return ERROR_NONE;
}

static kal_uint32 normal_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
	MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("E, current fps: %d\n", imgsensor.current_fps);

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode			= IMGSENSOR_MODE_VIDEO;
	imgsensor.pclk					= imgsensor_info.normal_video.pclk;
	imgsensor.line_length			= imgsensor_info.normal_video.linelength;
	imgsensor.frame_length			= imgsensor_info.normal_video.framelength;
	imgsensor.min_frame_length		= imgsensor_info.normal_video.framelength;
	imgsensor.autoflicker_en		= KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);

	set_mode_setfile(imgsensor.sensor_mode);

	set_mirror_flip(imgsensor.mirror);

	return ERROR_NONE;
}

static kal_uint32 hs_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("E, current fps: %d\n", imgsensor.current_fps);

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode			= IMGSENSOR_MODE_HIGH_SPEED_VIDEO;
	imgsensor.pclk					= imgsensor_info.hs_video.pclk;
	imgsensor.line_length			= imgsensor_info.hs_video.linelength;
	imgsensor.frame_length			= imgsensor_info.hs_video.framelength;
	imgsensor.min_frame_length		= imgsensor_info.hs_video.framelength;

	imgsensor.dummy_line			= 0;
	imgsensor.dummy_pixel			= 0;
	imgsensor.autoflicker_en		= KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);

	set_mode_setfile(imgsensor.sensor_mode);

	set_mirror_flip(imgsensor.mirror);

	return ERROR_NONE;
}

static kal_uint32 slim_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("E, current fps: %d\n", imgsensor.current_fps);

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode			= IMGSENSOR_MODE_SLIM_VIDEO;
	imgsensor.pclk					= imgsensor_info.slim_video.pclk;
	imgsensor.line_length			= imgsensor_info.slim_video.linelength;
	imgsensor.frame_length			= imgsensor_info.slim_video.framelength;
	imgsensor.min_frame_length		= imgsensor_info.slim_video.framelength;

	imgsensor.dummy_line			= 0;
	imgsensor.dummy_pixel			= 0;
	imgsensor.autoflicker_en		= KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);

	set_mode_setfile(imgsensor.sensor_mode);

	set_mirror_flip(imgsensor.mirror);

	return ERROR_NONE;
}

static kal_uint32 custom1(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("E, current fps: %d\n", imgsensor.current_fps);

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode			= IMGSENSOR_MODE_CUSTOM1;
	imgsensor.pclk					= imgsensor_info.custom1.pclk;
	imgsensor.line_length			= imgsensor_info.custom1.linelength;
	imgsensor.frame_length			= imgsensor_info.custom1.framelength;
	imgsensor.min_frame_length		= imgsensor_info.custom1.framelength;
	imgsensor.autoflicker_en		= KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);

	set_mode_setfile(imgsensor.sensor_mode);

	set_mirror_flip(IMAGE_NORMAL);

	return ERROR_NONE;
}

static kal_uint32 custom2(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("E, current fps: %d\n", imgsensor.current_fps);

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_CUSTOM2;
	imgsensor.pclk = imgsensor_info.custom2.pclk;
	imgsensor.line_length = imgsensor_info.custom2.linelength;
	imgsensor.frame_length = imgsensor_info.custom2.framelength;
	imgsensor.min_frame_length = imgsensor_info.custom2.framelength;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);

	set_mode_setfile(imgsensor.sensor_mode);

	set_mirror_flip(IMAGE_NORMAL);

	return ERROR_NONE;
}

static kal_uint32 custom3(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("E, current fps: %d\n", imgsensor.current_fps);

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_CUSTOM3;
	imgsensor.pclk = imgsensor_info.custom3.pclk;
	imgsensor.line_length = imgsensor_info.custom3.linelength;
	imgsensor.frame_length = imgsensor_info.custom3.framelength;
	imgsensor.min_frame_length = imgsensor_info.custom3.framelength;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);

	set_mode_setfile(imgsensor.sensor_mode);

	set_mirror_flip(IMAGE_NORMAL);

	return ERROR_NONE;
}

static kal_uint32 custom4(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("E, current fps: %d\n", imgsensor.current_fps);

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_CUSTOM4;
	imgsensor.pclk = imgsensor_info.custom4.pclk;
	imgsensor.line_length = imgsensor_info.custom4.linelength;
	imgsensor.frame_length = imgsensor_info.custom4.framelength;
	imgsensor.min_frame_length = imgsensor_info.custom4.framelength;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);

	set_mode_setfile(imgsensor.sensor_mode);

	set_mirror_flip(IMAGE_NORMAL);

	return ERROR_NONE;
}

static kal_uint32 custom5(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("E, current fps: %d\n", imgsensor.current_fps);

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_CUSTOM5;
	imgsensor.pclk = imgsensor_info.custom5.pclk;
	imgsensor.line_length = imgsensor_info.custom5.linelength;
	imgsensor.frame_length = imgsensor_info.custom5.framelength;
	imgsensor.min_frame_length = imgsensor_info.custom5.framelength;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);

	set_mode_setfile(imgsensor.sensor_mode);

	set_mirror_flip(IMAGE_NORMAL);

	return ERROR_NONE;
}

static kal_uint32 custom6(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("E, current fps: %d\n", imgsensor.current_fps);

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_CUSTOM6;
	imgsensor.pclk = imgsensor_info.custom6.pclk;
	imgsensor.line_length = imgsensor_info.custom6.linelength;
	imgsensor.frame_length = imgsensor_info.custom6.framelength;
	imgsensor.min_frame_length = imgsensor_info.custom6.framelength;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);

	set_mode_setfile(imgsensor.sensor_mode);

	set_mirror_flip(IMAGE_NORMAL);

	return ERROR_NONE;
}

static kal_uint32 get_resolution(
	MSDK_SENSOR_RESOLUTION_INFO_STRUCT(*sensor_resolution))
{
	LOG_INF("E\n");
	sensor_resolution->SensorFullWidth            = imgsensor_info.cap.grabwindow_width;
	sensor_resolution->SensorFullHeight           = imgsensor_info.cap.grabwindow_height;

	sensor_resolution->SensorPreviewWidth         = imgsensor_info.pre.grabwindow_width;
	sensor_resolution->SensorPreviewHeight        = imgsensor_info.pre.grabwindow_height;

	sensor_resolution->SensorVideoWidth           = imgsensor_info.normal_video.grabwindow_width;
	sensor_resolution->SensorVideoHeight          = imgsensor_info.normal_video.grabwindow_height;


	sensor_resolution->SensorHighSpeedVideoWidth  = imgsensor_info.hs_video.grabwindow_width;
	sensor_resolution->SensorHighSpeedVideoHeight = imgsensor_info.hs_video.grabwindow_height;

	sensor_resolution->SensorSlimVideoWidth       = imgsensor_info.slim_video.grabwindow_width;
	sensor_resolution->SensorSlimVideoHeight      = imgsensor_info.slim_video.grabwindow_height;

	sensor_resolution->SensorCustom1Width         = imgsensor_info.custom1.grabwindow_width;
	sensor_resolution->SensorCustom1Height        = imgsensor_info.custom1.grabwindow_height;

	sensor_resolution->SensorCustom2Width         = imgsensor_info.custom2.grabwindow_width;
	sensor_resolution->SensorCustom2Height        = imgsensor_info.custom2.grabwindow_height;

	sensor_resolution->SensorCustom3Width         = imgsensor_info.custom3.grabwindow_width;
	sensor_resolution->SensorCustom3Height        = imgsensor_info.custom3.grabwindow_height;

	sensor_resolution->SensorCustom4Width         = imgsensor_info.custom4.grabwindow_width;
	sensor_resolution->SensorCustom4Height        = imgsensor_info.custom4.grabwindow_height;

	sensor_resolution->SensorCustom5Width         = imgsensor_info.custom5.grabwindow_width;
	sensor_resolution->SensorCustom5Height        = imgsensor_info.custom5.grabwindow_height;

	sensor_resolution->SensorCustom6Width         = imgsensor_info.custom6.grabwindow_width;
	sensor_resolution->SensorCustom6Height        = imgsensor_info.custom6.grabwindow_height;

	return ERROR_NONE;
}

static kal_uint32 get_info(enum MSDK_SCENARIO_ID_ENUM scenario_id,
			   MSDK_SENSOR_INFO_STRUCT *sensor_info,
			   MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("scenario_id = %d\n", scenario_id);

	sensor_info->SensorClockPolarity		= SENSOR_CLOCK_POLARITY_LOW;
	sensor_info->SensorClockFallingPolarity = SENSOR_CLOCK_POLARITY_LOW;

	sensor_info->SensorHsyncPolarity = SENSOR_CLOCK_POLARITY_LOW;
	LOG_INF("sensor_info->SensorHsyncPolarity = %d\n", sensor_info->SensorHsyncPolarity);
	sensor_info->SensorVsyncPolarity = SENSOR_CLOCK_POLARITY_LOW;
	LOG_INF("sensor_info->SensorVsyncPolarity = %d\n", sensor_info->SensorVsyncPolarity);

	sensor_info->SensorInterruptDelayLines = 4;
	sensor_info->SensorResetActiveHigh     = FALSE;
	sensor_info->SensorResetDelayCount     = 5;

	sensor_info->SensroInterfaceType       = imgsensor_info.sensor_interface_type;
	sensor_info->MIPIsensorType            = imgsensor_info.mipi_sensor_type;
	sensor_info->SettleDelayMode           = imgsensor_info.mipi_settle_delay_mode;

	sensor_info->SensorOutputDataFormat    = imgsensor_info.sensor_output_dataformat;

	sensor_info->CaptureDelayFrame         = imgsensor_info.cap_delay_frame;
	sensor_info->PreviewDelayFrame         = imgsensor_info.pre_delay_frame;
	sensor_info->VideoDelayFrame           = imgsensor_info.video_delay_frame;

	sensor_info->HighSpeedVideoDelayFrame  = imgsensor_info.hs_video_delay_frame;

	sensor_info->SlimVideoDelayFrame       = imgsensor_info.slim_video_delay_frame;

	sensor_info->Custom1DelayFrame         = imgsensor_info.custom1_delay_frame;
	sensor_info->Custom2DelayFrame         = imgsensor_info.custom2_delay_frame;
	sensor_info->Custom3DelayFrame         = imgsensor_info.custom3_delay_frame;
	sensor_info->Custom4DelayFrame         = imgsensor_info.custom4_delay_frame;
	sensor_info->Custom5DelayFrame         = imgsensor_info.custom5_delay_frame;
	sensor_info->Custom6DelayFrame         = imgsensor_info.custom6_delay_frame;
	sensor_info->SensorMasterClockSwitch   = 0;
	sensor_info->SensorDrivingCurrent      = imgsensor_info.isp_driving_current;

	sensor_info->AEShutDelayFrame          = imgsensor_info.ae_shut_delay_frame;

	sensor_info->AESensorGainDelayFrame    = imgsensor_info.ae_sensor_gain_delay_frame;

	sensor_info->AEISPGainDelayFrame       = imgsensor_info.ae_ispGain_delay_frame;

	sensor_info->FrameTimeDelayFrame       = imgsensor_info.frame_time_delay_frame;

	sensor_info->IHDR_Support              = imgsensor_info.ihdr_support;
	sensor_info->IHDR_LE_FirstLine         = imgsensor_info.ihdr_le_firstline;
	sensor_info->SensorModeNum             = imgsensor_info.sensor_mode_num;
	sensor_info->SensorMIPILaneNumber      = imgsensor_info.mipi_lane_num;
	sensor_info->SensorClockFreq           = imgsensor_info.mclk;
	sensor_info->SensorClockDividCount     = 3;
	sensor_info->SensorClockRisingCount    = 0;
	sensor_info->SensorClockFallingCount   = 2;
	sensor_info->SensorPixelClockCount     = 3;
	sensor_info->SensorDataLatchCount      = 2;
	sensor_info->TEMPERATURE_SUPPORT       = imgsensor_info.temperature_support;
	sensor_info->MIPIDataLowPwr2HighSpeedTermDelayCount = 0;
	sensor_info->MIPICLKLowPwr2HighSpeedTermDelayCount  = 0;
	sensor_info->SensorWidthSampling       = 0;
	sensor_info->SensorHightSampling       = 0;
	sensor_info->SensorPacketECCOrder      = 1;

#ifdef PDAF_DISABLE
	sensor_info->PDAF_Support = PDAF_SUPPORT_NA;
#else
	sensor_info->PDAF_Support = PDAF_SUPPORT_CAMSV;
#endif

	LOG_INF("scenario_id = %d\n", scenario_id);
	switch (scenario_id) {
	case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
	LOG_INF("MSDK_SCENARIO_ID_CAMERA_PREVIEW scenario_id= %d\n", scenario_id);
		sensor_info->SensorGrabStartX = imgsensor_info.pre.startx;
		sensor_info->SensorGrabStartY = imgsensor_info.pre.starty;

		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
			imgsensor_info.pre.mipi_data_lp2hs_settle_dc;

		break;
	case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
	LOG_INF("MSDK_SCENARIO_ID_CAMERA_PREVIEW scenario_id= %d\n", scenario_id);
		sensor_info->SensorGrabStartX = imgsensor_info.cap.startx;
		sensor_info->SensorGrabStartY = imgsensor_info.cap.starty;

		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
			imgsensor_info.cap.mipi_data_lp2hs_settle_dc;

		break;
	case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
	LOG_INF("MSDK_SCENARIO_ID_CAMERA_PREVIEW scenario_id= %d\n", scenario_id);
		sensor_info->SensorGrabStartX = imgsensor_info.normal_video.startx;
		sensor_info->SensorGrabStartY = imgsensor_info.normal_video.starty;

		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
			imgsensor_info.normal_video.mipi_data_lp2hs_settle_dc;

		break;
	case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
	LOG_INF("MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO scenario_id= %d\n", scenario_id);
		sensor_info->SensorGrabStartX = imgsensor_info.hs_video.startx;
		sensor_info->SensorGrabStartY = imgsensor_info.hs_video.starty;

		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
			imgsensor_info.hs_video.mipi_data_lp2hs_settle_dc;

		break;
	case MSDK_SCENARIO_ID_SLIM_VIDEO:
	LOG_INF("MSDK_SCENARIO_ID_SLIM_VIDEO scenario_id= %d\n", scenario_id);
		sensor_info->SensorGrabStartX = imgsensor_info.slim_video.startx;
		sensor_info->SensorGrabStartY = imgsensor_info.slim_video.starty;

		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
			imgsensor_info.slim_video.mipi_data_lp2hs_settle_dc;

		break;

	case MSDK_SCENARIO_ID_CUSTOM1:
	LOG_INF("MSDK_SCENARIO_ID_CUSTOM1 scenario_id= %d\n", scenario_id);
		sensor_info->SensorGrabStartX = imgsensor_info.custom1.startx;
		sensor_info->SensorGrabStartY = imgsensor_info.custom1.starty;
		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
			imgsensor_info.custom1.mipi_data_lp2hs_settle_dc;

		break;
	case MSDK_SCENARIO_ID_CUSTOM2:
	LOG_INF("MSDK_SCENARIO_ID_CUSTOM2 scenario_id= %d\n", scenario_id);
		sensor_info->SensorGrabStartX = imgsensor_info.custom2.startx;
		sensor_info->SensorGrabStartY = imgsensor_info.custom2.starty;
		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
			imgsensor_info.custom2.mipi_data_lp2hs_settle_dc;
		break;
	case MSDK_SCENARIO_ID_CUSTOM3:
	LOG_INF("MSDK_SCENARIO_ID_CUSTOM3 scenario_id= %d\n", scenario_id);
		sensor_info->SensorGrabStartX = imgsensor_info.custom3.startx;
		sensor_info->SensorGrabStartY = imgsensor_info.custom3.starty;
		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
			imgsensor_info.custom3.mipi_data_lp2hs_settle_dc;
		break;
	case MSDK_SCENARIO_ID_CUSTOM4:
	LOG_INF("MSDK_SCENARIO_ID_CUSTOM4 scenario_id= %d\n", scenario_id);
		sensor_info->SensorGrabStartX = imgsensor_info.custom4.startx;
		sensor_info->SensorGrabStartY = imgsensor_info.custom4.starty;
		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
			imgsensor_info.custom4.mipi_data_lp2hs_settle_dc;
		break;
	case MSDK_SCENARIO_ID_CUSTOM5:
	LOG_INF("MSDK_SCENARIO_ID_CUSTOM5 scenario_id= %d\n", scenario_id);
		sensor_info->SensorGrabStartX = imgsensor_info.custom5.startx;
		sensor_info->SensorGrabStartY = imgsensor_info.custom5.starty;
		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
			imgsensor_info.custom5.mipi_data_lp2hs_settle_dc;
		break;
	case MSDK_SCENARIO_ID_CUSTOM6:
	LOG_INF("MSDK_SCENARIO_ID_CUSTOM6 scenario_id= %d\n", scenario_id);
		sensor_info->SensorGrabStartX = imgsensor_info.custom6.startx;
		sensor_info->SensorGrabStartY = imgsensor_info.custom6.starty;
		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
			imgsensor_info.custom6.mipi_data_lp2hs_settle_dc;
		break;
	default:
		sensor_info->SensorGrabStartX = imgsensor_info.pre.startx;
		sensor_info->SensorGrabStartY = imgsensor_info.pre.starty;

		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
			imgsensor_info.pre.mipi_data_lp2hs_settle_dc;
		break;
	}

	return ERROR_NONE;
}


static kal_uint32 control(enum MSDK_SCENARIO_ID_ENUM scenario_id,
			  MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("scenario_id = %d\n", scenario_id);

	spin_lock(&imgsensor_drv_lock);
	imgsensor.current_scenario_id = scenario_id;
	spin_unlock(&imgsensor_drv_lock);

	switch (scenario_id) {
	case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		LOG_INF("MSDK_SCENARIO_ID_CAMERA_PREVIEW scenario_id= %d\n", scenario_id);
		preview(image_window, sensor_config_data);
		break;
	case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		LOG_INF("MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG scenario_id= %d\n", scenario_id);
		capture(image_window, sensor_config_data);
		break;
	case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
		LOG_INF("MSDK_SCENARIO_ID_VIDEO_PREVIEW scenario_id= %d\n", scenario_id);
		normal_video(image_window, sensor_config_data);
		break;
	case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
		LOG_INF("MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO scenario_id= %d\n", scenario_id);
		hs_video(image_window, sensor_config_data);
		break;
	case MSDK_SCENARIO_ID_SLIM_VIDEO:
		LOG_INF("MSDK_SCENARIO_ID_SLIM_VIDEO scenario_id= %d\n", scenario_id);
		slim_video(image_window, sensor_config_data);
		break;

	case MSDK_SCENARIO_ID_CUSTOM1:
		LOG_INF("MSDK_SCENARIO_ID_CUSTOM1 scenario_id= %d\n", scenario_id);
		custom1(image_window, sensor_config_data);
		break;
	case MSDK_SCENARIO_ID_CUSTOM2:
		LOG_INF("MSDK_SCENARIO_ID_CUSTOM2 scenario_id= %d\n", scenario_id);
		custom2(image_window, sensor_config_data);
		break;
	case MSDK_SCENARIO_ID_CUSTOM3:
		LOG_INF("MSDK_SCENARIO_ID_CUSTOM3 scenario_id= %d\n", scenario_id);
		custom3(image_window, sensor_config_data);
		break;
	case MSDK_SCENARIO_ID_CUSTOM4:
		LOG_INF("MSDK_SCENARIO_ID_CUSTOM4 scenario_id= %d\n", scenario_id);
		custom4(image_window, sensor_config_data);
		break;
	case MSDK_SCENARIO_ID_CUSTOM5:
		LOG_INF("MSDK_SCENARIO_ID_CUSTOM5 scenario_id= %d\n", scenario_id);
		custom5(image_window, sensor_config_data);
		break;
	case MSDK_SCENARIO_ID_CUSTOM6:
		LOG_INF("MSDK_SCENARIO_ID_CUSTOM6 scenario_id= %d\n", scenario_id);
		custom6(image_window, sensor_config_data);
		break;
	default:
		LOG_ERR("Error ScenarioId setting");
		preview(image_window, sensor_config_data);
		return ERROR_INVALID_SCENARIO_ID;
	}
	get_sensor_status();
	return ERROR_NONE;
}

static kal_uint32 set_video_mode(UINT16 framerate)
{

	LOG_INF("framerate = %d\n", framerate);

	if (framerate == 0)
		return ERROR_NONE;

	spin_lock(&imgsensor_drv_lock);
	if ((framerate == 300) && (imgsensor.autoflicker_en == KAL_TRUE))
		imgsensor.current_fps = 296;
	else if ((framerate == 150) && (imgsensor.autoflicker_en == KAL_TRUE))
		imgsensor.current_fps = 146;
	else
		imgsensor.current_fps = framerate;
	spin_unlock(&imgsensor_drv_lock);

	set_max_framerate(imgsensor.current_fps, 1);

	return ERROR_NONE;
}

static kal_uint32 set_auto_flicker_mode(
	kal_bool enable, UINT16 framerate)
{
	LOG_DBG("enable = %d, framerate = %d\n", enable, framerate);

	spin_lock(&imgsensor_drv_lock);
	if (enable)
		imgsensor.autoflicker_en = KAL_TRUE;
	else
		imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);

	return ERROR_NONE;
}


static kal_uint32 set_max_framerate_by_scenario(
	enum MSDK_SCENARIO_ID_ENUM scenario_id,	MUINT32 framerate)
{
	kal_uint32 frame_length;

	LOG_INF("scenario_id = %d, framerate = %d\n", scenario_id, framerate);

	switch (scenario_id) {
	case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		LOG_DBG("MSDK_SCENARIO_ID_CAMERA_PREVIEW scenario_id= %d\n", scenario_id);

		if(framerate == imgsensor_info.pre.max_framerate)
			frame_length = imgsensor_info.pre.framelength;
		else
			frame_length = imgsensor_info.pre.pclk / framerate * 10 / imgsensor_info.pre.linelength;

		spin_lock(&imgsensor_drv_lock);

		imgsensor.dummy_line = (frame_length > imgsensor_info.pre.framelength)
			? (frame_length - imgsensor_info.pre.framelength) : 0;

		imgsensor.frame_length = imgsensor_info.pre.framelength + imgsensor.dummy_line;

		imgsensor.min_frame_length = imgsensor.frame_length;

		spin_unlock(&imgsensor_drv_lock);

		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
		break;
	case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
		LOG_DBG("MSDK_SCENARIO_ID_VIDEO_PREVIEW scenario_id= %d\n", scenario_id);
		if (framerate == 0)
			return ERROR_NONE;

		if(framerate == imgsensor_info.normal_video.max_framerate)
			frame_length = imgsensor_info.normal_video.framelength;
		else
			frame_length = imgsensor_info.normal_video.pclk / framerate * 10 / imgsensor_info.normal_video.linelength;

		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line =
			(frame_length >
				imgsensor_info.normal_video.framelength) ?
			(frame_length -
				imgsensor_info.normal_video.framelength) : 0;

		imgsensor.frame_length = imgsensor_info.normal_video.framelength + imgsensor.dummy_line;

		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);

		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
		break;

	case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		LOG_DBG("MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG scenario_id= %d\n", scenario_id);

		if(framerate == imgsensor_info.cap.max_framerate)
			frame_length = imgsensor_info.cap.framelength;
		else
			frame_length = imgsensor_info.cap.pclk / framerate * 10 / imgsensor_info.cap.linelength;

		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line = (frame_length > imgsensor_info.cap.framelength)
			? (frame_length - imgsensor_info.cap.framelength) : 0;
		imgsensor.frame_length = imgsensor_info.cap.framelength + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);

		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
		break;

	case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
		LOG_DBG("MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO scenario_id= %d\n", scenario_id);

		if(framerate == imgsensor_info.hs_video.max_framerate)	
			frame_length = imgsensor_info.hs_video.framelength;
		else
			frame_length = imgsensor_info.hs_video.pclk / framerate * 10 / imgsensor_info.hs_video.linelength;

		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line = (frame_length > imgsensor_info.hs_video.framelength)
		? (frame_length - imgsensor_info.hs_video.  framelength) : 0;

		imgsensor.frame_length = imgsensor_info.hs_video.framelength + imgsensor.dummy_line;

		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);

		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
		break;
	case MSDK_SCENARIO_ID_SLIM_VIDEO:
		LOG_DBG("MSDK_SCENARIO_ID_SLIM_VIDEO scenario_id= %d\n", scenario_id);

		if(framerate == imgsensor_info.slim_video.max_framerate)
			frame_length = imgsensor_info.slim_video.framelength;
		else
			frame_length = imgsensor_info.slim_video.pclk / framerate * 10 / imgsensor_info.slim_video.linelength;

		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line = (frame_length > imgsensor_info.slim_video.framelength)
		? (frame_length - imgsensor_info.slim_video.  framelength) : 0;

		imgsensor.frame_length = imgsensor_info.slim_video.framelength + imgsensor.dummy_line;

		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);

		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
		break;

	case MSDK_SCENARIO_ID_CUSTOM1:
		LOG_DBG("MSDK_SCENARIO_ID_CUSTOM1 scenario_id= %d\n", scenario_id);

		if(framerate == imgsensor_info.custom1.max_framerate)
			frame_length = imgsensor_info.custom1.framelength;
		else
			frame_length = imgsensor_info.custom1.pclk / framerate * 10 / imgsensor_info.custom1.linelength;

		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line = (frame_length > imgsensor_info.custom1.framelength)
			? (frame_length - imgsensor_info.custom1.framelength) : 0;

		if (imgsensor.dummy_line < 0)
			imgsensor.dummy_line = 0;

		imgsensor.frame_length = imgsensor_info.custom1.framelength + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);

		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
		break;
	case MSDK_SCENARIO_ID_CUSTOM2:
		LOG_DBG("MSDK_SCENARIO_ID_CUSTOM2 scenario_id= %d\n", scenario_id);

		if(framerate == imgsensor_info.custom2.max_framerate)
			frame_length = imgsensor_info.custom2.framelength;
		else
			frame_length = imgsensor_info.custom2.pclk / framerate * 10 / imgsensor_info.custom2.linelength;

		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line =
			(frame_length > imgsensor_info.custom2.framelength)
		? (frame_length - imgsensor_info.custom2.framelength) : 0;

		if (imgsensor.dummy_line < 0)
			imgsensor.dummy_line = 0;

		imgsensor.frame_length =
		imgsensor_info.custom2.framelength + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);

		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
		break;
	case MSDK_SCENARIO_ID_CUSTOM3:
		LOG_DBG("MSDK_SCENARIO_ID_CUSTOM3 scenario_id= %d\n", scenario_id);

		if(framerate == imgsensor_info.custom3.max_framerate)
			frame_length = imgsensor_info.custom3.framelength;
		else
			frame_length = imgsensor_info.custom3.pclk / framerate * 10 / imgsensor_info.custom3.linelength;

		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line =
			(frame_length > imgsensor_info.custom3.framelength)
		? (frame_length - imgsensor_info.custom3.framelength) : 0;
		if (imgsensor.dummy_line < 0)
			imgsensor.dummy_line = 0;
		imgsensor.frame_length =
		imgsensor_info.custom3.framelength + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
		break;
	case MSDK_SCENARIO_ID_CUSTOM4:
		LOG_DBG("MSDK_SCENARIO_ID_CUSTOM4 scenario_id= %d\n", scenario_id);

		if(framerate == imgsensor_info.custom4.max_framerate)
			frame_length = imgsensor_info.custom4.framelength;
		else
			frame_length = imgsensor_info.custom4.pclk / framerate * 10 / imgsensor_info.custom4.linelength;

		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line =
			(frame_length > imgsensor_info.custom4.framelength)
		? (frame_length - imgsensor_info.custom4.framelength) : 0;
		if (imgsensor.dummy_line < 0)
			imgsensor.dummy_line = 0;
		imgsensor.frame_length =
		imgsensor_info.custom4.framelength + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
		break;
	case MSDK_SCENARIO_ID_CUSTOM5:
		LOG_DBG("MSDK_SCENARIO_ID_CUSTOM5 scenario_id= %d\n", scenario_id);

		if(framerate == imgsensor_info.custom5.max_framerate)
			frame_length = imgsensor_info.custom5.framelength;
		else
			frame_length = imgsensor_info.custom5.pclk / framerate * 10 / imgsensor_info.custom5.linelength;

		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line =
			(frame_length > imgsensor_info.custom5.framelength)
			? (frame_length - imgsensor_info.custom5.framelength) : 0;
		if (imgsensor.dummy_line < 0)
			imgsensor.dummy_line = 0;
		imgsensor.frame_length =
		imgsensor_info.custom5.framelength + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
		break;
	case MSDK_SCENARIO_ID_CUSTOM6:
		LOG_DBG("MSDK_SCENARIO_ID_CUSTOM6 scenario_id= %d\n", scenario_id);

		if(framerate == imgsensor_info.custom6.max_framerate)
			frame_length = imgsensor_info.custom6.framelength;
		else
			frame_length = imgsensor_info.custom6.pclk / framerate * 10 / imgsensor_info.custom6.linelength;

		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line =
			(frame_length > imgsensor_info.custom6.framelength)
			? (frame_length - imgsensor_info.custom6.framelength) : 0;
		if (imgsensor.dummy_line < 0)
			imgsensor.dummy_line = 0;
		imgsensor.frame_length =
		imgsensor_info.custom6.framelength + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
		break;
	default:
		if(framerate == imgsensor_info.pre.max_framerate)
			frame_length = imgsensor_info.pre.framelength;
		else
			frame_length = imgsensor_info.pre.pclk / framerate * 10 / imgsensor_info.pre.linelength;

		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line = (frame_length > imgsensor_info.pre.framelength)
			? (frame_length - imgsensor_info.pre.framelength) : 0;
		imgsensor.frame_length = imgsensor_info.pre.framelength + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
		LOG_INF("error scenario_id = %d, we use preview scenario\n", scenario_id);
		break;
	}
	return ERROR_NONE;
}


static kal_uint32 get_default_framerate_by_scenario(
	enum MSDK_SCENARIO_ID_ENUM scenario_id, MUINT32 *framerate)
{
	switch (scenario_id) {
	case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		*framerate = imgsensor_info.pre.max_framerate;
		break;
	case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
		*framerate = imgsensor_info.normal_video.max_framerate;
		break;
	case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		*framerate = imgsensor_info.cap.max_framerate;
		break;
	case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
		*framerate = imgsensor_info.hs_video.max_framerate;
		break;
	case MSDK_SCENARIO_ID_SLIM_VIDEO:
		*framerate = imgsensor_info.slim_video.max_framerate;
		break;

	case MSDK_SCENARIO_ID_CUSTOM1:
		*framerate = imgsensor_info.custom1.max_framerate;
		break;
	case MSDK_SCENARIO_ID_CUSTOM2:
		*framerate = imgsensor_info.custom2.max_framerate;
		break;
	case MSDK_SCENARIO_ID_CUSTOM3:
		*framerate = imgsensor_info.custom3.max_framerate;
		break;
	case MSDK_SCENARIO_ID_CUSTOM4:
		*framerate = imgsensor_info.custom4.max_framerate;
		break;
	case MSDK_SCENARIO_ID_CUSTOM5:
		*framerate = imgsensor_info.custom5.max_framerate;
		break;
	case MSDK_SCENARIO_ID_CUSTOM6:
		*framerate = imgsensor_info.custom6.max_framerate;
		break;
	default:
		break;
	}
	LOG_INF("scenario_id = %d, framerate = %d", scenario_id, *framerate);
	return ERROR_NONE;
}

static kal_uint32 set_test_pattern_mode(kal_bool enable)
{
#if 0
	if (enable) {
		write_cmos_sensor(0x3202, 0x0080);
		write_cmos_sensor(0x3204, 0x0080);
		write_cmos_sensor(0x3206, 0x0080);
		write_cmos_sensor(0x3208, 0x0080);
		write_cmos_sensor(0x3232, 0x0000);
		write_cmos_sensor(0x3234, 0x0000);
		write_cmos_sensor(0x32a0, 0x0100);
		write_cmos_sensor(0x3300, 0x0001);
		write_cmos_sensor(0x3400, 0x0001);
		write_cmos_sensor(0x3402, 0x4e00);
		write_cmos_sensor(0x3268, 0x0000);
		write_cmos_sensor(0x0600, 0x0002);
	} else {
		write_cmos_sensor(0x3202, 0x0000);
		write_cmos_sensor(0x3204, 0x0000);
		write_cmos_sensor(0x3206, 0x0000);
		write_cmos_sensor(0x3208, 0x0000);
		write_cmos_sensor(0x3232, 0x0000);
		write_cmos_sensor(0x3234, 0x0000);
		write_cmos_sensor(0x32a0, 0x0000);
		write_cmos_sensor(0x3300, 0x0000);
		write_cmos_sensor(0x3400, 0x0000);
		write_cmos_sensor(0x3402, 0x0000);
		write_cmos_sensor(0x3268, 0x0000);
		write_cmos_sensor(0x0600, 0x0000);
	}
	spin_lock(&imgsensor_drv_lock);
	imgsensor.test_pattern = enable;
	spin_unlock(&imgsensor_drv_lock);
#else
	//test pattern is always disable
	spin_lock(&imgsensor_drv_lock);
	imgsensor.test_pattern = false;
	spin_unlock(&imgsensor_drv_lock);
#endif
	LOG_INF("enable: %d\n", imgsensor.test_pattern);
	return ERROR_NONE;
}

#if defined(CONFIG_TRAN_CAMERA_SYNC_AWB_TO_KERNEL)
static void set_awbgain(kal_uint32 g_gain, kal_uint32 r_gain, kal_uint32 b_gain)
{
	kal_uint32 max_gain = 0x1000;
	kal_uint32 r_gain_int = 0x0;
	kal_uint32 g_gain_int = 0x0;
	kal_uint32 b_gain_int = 0x0;

	r_gain_int = r_gain * 2;
	if (r_gain_int > max_gain)
		r_gain_int = max_gain;

	g_gain_int = g_gain * 2;
	if (g_gain_int > max_gain)
		g_gain_int = max_gain;

	b_gain_int = b_gain * 2;
	if (b_gain_int > max_gain)
		b_gain_int = max_gain;

	LOG_INF("r_gain=0x%x, g_gain=0x%x, b_gain=0x%x\n", r_gain_int, g_gain_int, b_gain_int);

	write_cmos_sensor(0x0D82, r_gain_int & 0xFFFF);
	write_cmos_sensor(0x0D84, g_gain_int & 0xFFFF);
	write_cmos_sensor(0x0D86, b_gain_int & 0xFFFF);
}
#endif

static kal_uint32 feature_control(MSDK_SENSOR_FEATURE_ENUM feature_id,
				  UINT8 *feature_para, UINT32 *feature_para_len)
{
	UINT16 *feature_return_para_16	= (UINT16 *) feature_para;
	UINT16 *feature_data_16			= (UINT16 *) feature_para;
	UINT32 *feature_return_para_32	= (UINT32 *) feature_para;
	UINT32 *feature_data_32			= (UINT32 *) feature_para;
	// INT32 *feature_return_para_i32	= (INT32 *) feature_para;

	unsigned long long *feature_data = (unsigned long long *)feature_para;
	struct SENSOR_WINSIZE_INFO_STRUCT *wininfo;
	struct SET_PD_BLOCK_INFO_T *PDAFinfo;
	struct SENSOR_VC_INFO_STRUCT *pvcinfo;

	MSDK_SENSOR_REG_INFO_STRUCT *sensor_reg_data =
		(MSDK_SENSOR_REG_INFO_STRUCT *) feature_para;

	LOG_DBG("feature_id = %d\n", feature_id);

	switch (feature_id) {
	case SENSOR_FEATURE_GET_PERIOD:
		LOG_DBG("SENSOR_FEATURE_GET_PERIOD = %d\n", feature_id);
		*feature_return_para_16++	= imgsensor.line_length;
		*feature_return_para_16		= imgsensor.frame_length;
		*feature_para_len			= 4;
		break;
	case SENSOR_FEATURE_GET_PIXEL_CLOCK_FREQ:
		LOG_DBG("SENSOR_FEATURE_GET_PIXEL_CLOCK_FREQ = %d\n", feature_id);
		*feature_return_para_32 = imgsensor.pclk;
		*feature_para_len = 4;
		break;
	case SENSOR_FEATURE_GET_PIXEL_CLOCK_FREQ_BY_SCENARIO:
		switch (*feature_data) {
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.cap.pclk;
			break;
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.normal_video.pclk;
			break;
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.hs_video.pclk;
			break;
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.slim_video.pclk;
			break;
		case MSDK_SCENARIO_ID_CUSTOM1:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.custom1.pclk;
			break;
		case MSDK_SCENARIO_ID_CUSTOM2:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.custom2.pclk;
			break;
		case MSDK_SCENARIO_ID_CUSTOM3:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.custom3.pclk;
			break;
		case MSDK_SCENARIO_ID_CUSTOM4:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.custom4.pclk;
			break;
		case MSDK_SCENARIO_ID_CUSTOM5:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.custom5.pclk;
			break;
		case MSDK_SCENARIO_ID_CUSTOM6:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.custom6.pclk;
			break;
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		default:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.pre.pclk;
			break;
		}
		break;
	case SENSOR_FEATURE_GET_PERIOD_BY_SCENARIO:
		switch (*feature_data) {
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
			= (imgsensor_info.cap.framelength << 16)
				+ imgsensor_info.cap.linelength;
			break;
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
			= (imgsensor_info.normal_video.framelength << 16)
				+ imgsensor_info.normal_video.linelength;
			break;
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
			= (imgsensor_info.hs_video.framelength << 16)
				+ imgsensor_info.hs_video.linelength;
			break;
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
			= (imgsensor_info.slim_video.framelength << 16)
				+ imgsensor_info.slim_video.linelength;
			break;
		case MSDK_SCENARIO_ID_CUSTOM1:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
			= (imgsensor_info.custom1.framelength << 16)
				+ imgsensor_info.custom1.linelength;
			break;
		case MSDK_SCENARIO_ID_CUSTOM2:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
			= (imgsensor_info.custom2.framelength << 16)
				+ imgsensor_info.custom2.linelength;
			break;
		case MSDK_SCENARIO_ID_CUSTOM3:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
			= (imgsensor_info.custom3.framelength << 16)
				+ imgsensor_info.custom3.linelength;
			break;
		case MSDK_SCENARIO_ID_CUSTOM4:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
			= (imgsensor_info.custom4.framelength << 16)
				+ imgsensor_info.custom4.linelength;
			break;
		case MSDK_SCENARIO_ID_CUSTOM5:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
			= (imgsensor_info.custom5.framelength << 16)
				+ imgsensor_info.custom5.linelength;
			break;
		case MSDK_SCENARIO_ID_CUSTOM6:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
			= (imgsensor_info.custom6.framelength << 16)
				+ imgsensor_info.custom6.linelength;
			break;
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		default:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
			= (imgsensor_info.pre.framelength << 16)
				+ imgsensor_info.pre.linelength;
			break;
		}
		break;
	case SENSOR_FEATURE_SET_ESHUTTER:
		LOG_DBG("SENSOR_FEATURE_SET_ESHUTTER = %d\n", feature_id);
		set_shutter(*feature_data);
		break;
	case SENSOR_FEATURE_SET_NIGHTMODE:
		LOG_INF("SENSOR_FEATURE_SET_NIGHTMODE = %d, data=%d\n", feature_id, *feature_data);
		sNightHyperlapseSpeed = *feature_data;
		break;
	case SENSOR_FEATURE_SET_GAIN:
		LOG_DBG("SENSOR_FEATURE_SET_GAIN = %d\n", feature_id);
		set_gain((UINT16) *feature_data);
		break;
	case SENSOR_FEATURE_SET_FLASHLIGHT:
		LOG_INF("SENSOR_FEATURE_SET_FLASHLIGHT = %d\n", feature_id);
		break;
	case SENSOR_FEATURE_SET_ISP_MASTER_CLOCK_FREQ:
		LOG_INF("SENSOR_FEATURE_SET_ISP_MASTER_CLOCK_FREQ = %d\n", feature_id);
		break;

	case SENSOR_FEATURE_SET_REGISTER:
		LOG_INF("SENSOR_FEATURE_SET_REGISTER = %d\n", feature_id);
		write_cmos_sensor_8(sensor_reg_data->RegAddr, sensor_reg_data->RegData);
		break;

	case SENSOR_FEATURE_GET_REGISTER:
		LOG_INF("SENSOR_FEATURE_GET_REGISTER = %d\n", feature_id);
		sensor_reg_data->RegData = read_cmos_sensor_8(sensor_reg_data->RegAddr);
		break;

	case SENSOR_FEATURE_GET_LENS_DRIVER_ID:
		/* get the lens driver ID from EEPROM or
		 * just return LENS_DRIVER_ID_DO_NOT_CARE
		 */
		LOG_INF("SENSOR_FEATURE_GET_LENS_DRIVER_ID = %d\n", feature_id);
		*feature_return_para_32 = LENS_DRIVER_ID_DO_NOT_CARE;
		*feature_para_len		= 4;
		break;
	case SENSOR_FEATURE_SET_VIDEO_MODE:
		LOG_INF("SENSOR_FEATURE_SET_VIDEO_MODE = %d\n", feature_id);
		set_video_mode(*feature_data);
		break;
	case SENSOR_FEATURE_CHECK_SENSOR_ID:
		LOG_INF("SENSOR_FEATURE_CHECK_SENSOR_ID = %d\n", feature_id);
		get_imgsensor_id(feature_return_para_32);
		break;
	case SENSOR_FEATURE_SET_AUTO_FLICKER_MODE:
		LOG_DBG("SENSOR_FEATURE_SET_AUTO_FLICKER_MODE = %d\n", feature_id);
		set_auto_flicker_mode((BOOL) (*feature_data_16), *(feature_data_16 + 1));
		break;
	case SENSOR_FEATURE_SET_MAX_FRAME_RATE_BY_SCENARIO:
		LOG_DBG("SENSOR_FEATURE_SET_MAX_FRAME_RATE_BY_SCENARIO = %d\n", feature_id);
		set_max_framerate_by_scenario(
		(enum MSDK_SCENARIO_ID_ENUM) *feature_data, *(feature_data + 1));
		break;

	case SENSOR_FEATURE_GET_DEFAULT_FRAME_RATE_BY_SCENARIO:
		LOG_DBG("SENSOR_FEATURE_GET_DEFAULT_FRAME_RATE_BY_SCENARIO = %d\n", feature_id);
		get_default_framerate_by_scenario(
			(enum MSDK_SCENARIO_ID_ENUM) *(feature_data),
			  (MUINT32 *) (uintptr_t) (*(feature_data + 1)));
		break;
	case SENSOR_FEATURE_SET_TEST_PATTERN:
		LOG_INF("SENSOR_FEATURE_SET_TEST_PATTERN = %d\n", feature_id);
		set_test_pattern_mode((BOOL) (*feature_data));
		break;

	case SENSOR_FEATURE_GET_TEST_PATTERN_CHECKSUM_VALUE:
		LOG_INF("SENSOR_FEATURE_GET_TEST_PATTERN_CHECKSUM_VALUE = %d\n", feature_id);
		*feature_return_para_32	= imgsensor_info.checksum_value;
		*feature_para_len		= 4;
		break;
	case SENSOR_FEATURE_SET_FRAMERATE:
		LOG_DBG("SENSOR_FEATURE_SET_FRAMERATE = %d\n", feature_id);
		LOG_INF("current fps :%d\n", *feature_data_32);
		spin_lock(&imgsensor_drv_lock);
		imgsensor.current_fps = (UINT16)*feature_data_32;
		spin_unlock(&imgsensor_drv_lock);
		break;
	case SENSOR_FEATURE_SET_HDR:
		LOG_DBG("SENSOR_FEATURE_SET_HDR = %d\n", feature_id);
		LOG_INF("ihdr enable :%d\n", *feature_data_32);
		spin_lock(&imgsensor_drv_lock);
		imgsensor.ihdr_mode = (UINT8)*feature_data_32;
		spin_unlock(&imgsensor_drv_lock);
		break;

	case SENSOR_FEATURE_GET_CROP_INFO:
		LOG_DBG("SENSOR_FEATURE_GET_CROP_INFO = %d\n", feature_id);
		LOG_INF("SENSOR_FEATURE_GET_CROP_INFO scenarioId:%d\n", (UINT32) *feature_data);
		wininfo = (struct SENSOR_WINSIZE_INFO_STRUCT *) (uintptr_t) (*(feature_data + 1));

		switch (*feature_data_32) {
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			LOG_DBG("MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG = %d\n", *feature_data_32);
			memcpy((void *)wininfo, (void *)&imgsensor_winsize_info[1],
				   sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			LOG_DBG("MSDK_SCENARIO_ID_VIDEO_PREVIEW = %d\n", *feature_data_32);
			memcpy((void *)wininfo, (void *)&imgsensor_winsize_info[2],
				   sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
			LOG_DBG("MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO = %d\n", *feature_data_32);
			memcpy((void *)wininfo, (void *)&imgsensor_winsize_info[3],
				   sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
			LOG_DBG("MSDK_SCENARIO_ID_SLIM_VIDEO = %d\n", *feature_data_32);
			memcpy((void *)wininfo, (void *)&imgsensor_winsize_info[4],
				   sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;

		case MSDK_SCENARIO_ID_CUSTOM1:
			LOG_DBG("MSDK_SCENARIO_ID_CUSTOM1 = %d\n", feature_id);
			memcpy((void *)wininfo, (void *)&imgsensor_winsize_info[5],
					sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_CUSTOM2:
			LOG_DBG("MSDK_SCENARIO_ID_CUSTOM2 = %d\n", feature_id);
			memcpy((void *)wininfo,
			(void *)&imgsensor_winsize_info[6],
			sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_CUSTOM3:
			LOG_DBG("MSDK_SCENARIO_ID_CUSTOM3 = %d\n", feature_id);
			memcpy((void *)wininfo,
			(void *)&imgsensor_winsize_info[7],
			sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_CUSTOM4:
			LOG_DBG("MSDK_SCENARIO_ID_CUSTOM4 = %d\n", feature_id);
			memcpy((void *)wininfo,
			(void *)&imgsensor_winsize_info[8],
			sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_CUSTOM5:
			LOG_DBG("MSDK_SCENARIO_ID_CUSTOM5 = %d\n", feature_id);
			memcpy((void *)wininfo,
			(void *)&imgsensor_winsize_info[9],
			sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_CUSTOM6:
			LOG_DBG("MSDK_SCENARIO_ID_CUSTOM6 = %d\n", feature_id);
			memcpy((void *)wininfo,
			(void *)&imgsensor_winsize_info[10],
			sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		default:
			memcpy((void *)wininfo, (void *)&imgsensor_winsize_info[0],
				sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		}
		break;
	case SENSOR_FEATURE_GET_PDAF_INFO:
		LOG_INF("SENSOR_FEATURE_GET_PDAF_INFO, scenario id:%d\n", (UINT16)*feature_data);
		PDAFinfo = (struct SET_PD_BLOCK_INFO_T *)(uintptr_t)(*(feature_data+1));

		switch (*feature_data) {
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		case MSDK_SCENARIO_ID_CUSTOM1:
		case MSDK_SCENARIO_ID_CUSTOM2:
		case MSDK_SCENARIO_ID_CUSTOM3:
		case MSDK_SCENARIO_ID_CUSTOM5:
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
			/* TODO */
			imgsensor_pd_info.i4BlockNumX = 1000;
			imgsensor_pd_info.i4BlockNumY = 750;
			memcpy((void *)PDAFinfo, (void *)&imgsensor_pd_info, sizeof(struct SET_PD_BLOCK_INFO_T));
			break;
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
		case MSDK_SCENARIO_ID_CUSTOM4:
		case MSDK_SCENARIO_ID_CUSTOM6:
			/* TODO */
			imgsensor_pd_info.i4BlockNumX = 1000;
			imgsensor_pd_info.i4BlockNumY = 562;
			memcpy((void *)PDAFinfo, (void *)&imgsensor_pd_info, sizeof(struct SET_PD_BLOCK_INFO_T));
			break;
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
			/* TODO */
			imgsensor_pd_info.i4BlockNumX = 320;
			imgsensor_pd_info.i4BlockNumY = 180;
			memcpy((void *)PDAFinfo, (void *)&imgsensor_pd_info, sizeof(struct SET_PD_BLOCK_INFO_T));
			break;
		default:
			break;
		}
		break;
	case SENSOR_FEATURE_GET_SENSOR_PDAF_CAPACITY:
		LOG_INF("SENSOR_FEATURE_GET_SENSOR_PDAF_CAPACITY scenarioId:%d\n",
			(UINT16) *feature_data);
		/* PDAF capacity enable or not */
#ifdef PDAF_DISABLE
		*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) = 0;
#else
		switch (*feature_data) {
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
		case MSDK_SCENARIO_ID_CUSTOM4:
		case MSDK_SCENARIO_ID_CUSTOM5:
		case MSDK_SCENARIO_ID_CUSTOM6:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) = 1;
			break;
		default:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) = 0;
			break;
		}
#endif
		break;
	case SENSOR_FEATURE_GET_VC_INFO:
		LOG_INF("SENSOR_FEATURE_GET_VC_INFO, scenario id = %d\n", (UINT16) *feature_data);
		pvcinfo = (struct SENSOR_VC_INFO_STRUCT *) (uintptr_t) (*(feature_data + 1));

		switch (*feature_data_32) {
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
			memcpy((void *)pvcinfo, (void *)&SENSOR_VC_INFO[0],
				   sizeof(struct SENSOR_VC_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			memcpy((void *)pvcinfo, (void *)&SENSOR_VC_INFO[1],
				   sizeof(struct SENSOR_VC_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			memcpy((void *)pvcinfo, (void *)&SENSOR_VC_INFO[2],
				   sizeof(struct SENSOR_VC_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
			memcpy((void *)pvcinfo, (void *)&SENSOR_VC_INFO[3],
				   sizeof(struct SENSOR_VC_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_CUSTOM1:
			memcpy((void *)pvcinfo, (void *)&SENSOR_VC_INFO[4],
				   sizeof(struct SENSOR_VC_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_CUSTOM2:
			memcpy((void *)pvcinfo, (void *)&SENSOR_VC_INFO[5],
				   sizeof(struct SENSOR_VC_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_CUSTOM3:
			memcpy((void *)pvcinfo, (void *)&SENSOR_VC_INFO[6],
				   sizeof(struct SENSOR_VC_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_CUSTOM4:
			memcpy((void *)pvcinfo, (void *)&SENSOR_VC_INFO[7],
				   sizeof(struct SENSOR_VC_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_CUSTOM5:
			memcpy((void *)pvcinfo, (void *)&SENSOR_VC_INFO[8],
				   sizeof(struct SENSOR_VC_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_CUSTOM6:
			memcpy((void *)pvcinfo, (void *)&SENSOR_VC_INFO[9],
				   sizeof(struct SENSOR_VC_INFO_STRUCT));
			break;
		default:
			break;
		}
		break;
	case SENSOR_FEATURE_SET_IHDR_SHUTTER_GAIN:
		LOG_DBG("SENSOR_FEATURE_SET_IHDR_SHUTTER_GAIN = %d\n", feature_id);
		LOG_DBG("SENSOR_SET_SENSOR_IHDR LE=%d, SE=%d, Gain=%d\n",
			(UINT16) *feature_data,
			(UINT16) *(feature_data + 1),
			(UINT16) *(feature_data + 2));

/* ihdr_write_shutter_gain(	(UINT16)*feature_data,
 *				(UINT16)*(feature_data+1),
 *				(UINT16)*(feature_data+2));
 */
		break;
	case SENSOR_FEATURE_SET_AWB_GAIN:
		LOG_INF("SENSOR_FEATURE_SET_AWB_GAIN = %d\n", feature_id);
 #if defined(CONFIG_TRAN_CAMERA_SYNC_AWB_TO_KERNEL)
		set_awbgain((UINT32)(*feature_data_32),
					(UINT32)*(feature_data_32 + 1),
					(UINT32)*(feature_data_32 + 2));
 #endif
		break;
	case SENSOR_FEATURE_SET_HDR_SHUTTER:
		LOG_INF("SENSOR_FEATURE_SET_HDR_SHUTTER = %d\n", feature_id);
		LOG_INF("SENSOR_FEATURE_SET_HDR_SHUTTER LE=%d, SE=%d\n",
			(UINT16) *feature_data, (UINT16) *(feature_data + 1));
		break;

	case SENSOR_FEATURE_SET_SHUTTER_FRAME_TIME:/*lzl*/
			set_shutter_frame_length((UINT32)*feature_data,
					(UINT32)*(feature_data+1));
			break;

	case SENSOR_FEATURE_SET_STREAMING_SUSPEND:
		LOG_INF("SENSOR_FEATURE_SET_STREAMING_SUSPEND\n");
		streaming_control(KAL_FALSE);
		break;
	case SENSOR_FEATURE_SET_STREAMING_RESUME:
		LOG_INF("SENSOR_FEATURE_SET_STREAMING_RESUME, shutter:%llu\n", *feature_data);
		if (*feature_data != 0)
			set_shutter(*feature_data);
		streaming_control(KAL_TRUE);
		break;
	case SENSOR_FEATURE_GET_TEMPERATURE_VALUE:
		LOG_DBG("SENSOR_FEATURE_GET_TEMPERATURE_VALUE = %d\n", feature_id);
		break;

	case SENSOR_FEATURE_GET_PIXEL_RATE:
		LOG_INF("SENSOR_FEATURE_GET_PIXEL_RATE = %d\n", feature_id);
		switch (*feature_data) {
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			LOG_INF("MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG = %d\n", *feature_data);
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) = (imgsensor_info.cap.pclk /
			(imgsensor_info.cap.linelength - 80)) * imgsensor_info.cap.grabwindow_width;
			break;
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			LOG_INF("MSDK_SCENARIO_ID_VIDEO_PREVIEW = %d\n", *feature_data);
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) = (imgsensor_info.normal_video.pclk /
			(imgsensor_info.normal_video.linelength - 80)) * imgsensor_info.normal_video.grabwindow_width;
			break;
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
			LOG_INF("MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO = %d\n", *feature_data);
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) = (imgsensor_info.hs_video.pclk /
			(imgsensor_info.hs_video.linelength - 80)) * imgsensor_info.hs_video.grabwindow_width;

			break;
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
			LOG_INF("MSDK_SCENARIO_ID_SLIM_VIDEO = %d\n", *feature_data);
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) = (imgsensor_info.slim_video.pclk /
			(imgsensor_info.slim_video.linelength - 80)) * imgsensor_info.slim_video.grabwindow_width;
			break;
		case MSDK_SCENARIO_ID_CUSTOM1:
		LOG_INF("MSDK_SCENARIO_ID_CUSTOM1 = %d\n", *feature_data);
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) = (imgsensor_info.custom1.pclk /
			(imgsensor_info.custom1.linelength - 80)) * imgsensor_info.custom1.grabwindow_width;
			break;
		case MSDK_SCENARIO_ID_CUSTOM2:
			LOG_INF("MSDK_SCENARIO_ID_CUSTOM2 = %d\n", *feature_data);
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) = (imgsensor_info.custom2.pclk /
			(imgsensor_info.custom2.linelength - 80)) * imgsensor_info.custom2.grabwindow_width;
			break;
		case MSDK_SCENARIO_ID_CUSTOM3:
			LOG_INF("MSDK_SCENARIO_ID_CUSTOM3 = %d\n", *feature_data);
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) = (imgsensor_info.custom3.pclk /
			(imgsensor_info.custom3.linelength - 80)) * imgsensor_info.custom3.grabwindow_width;
			break;
		case MSDK_SCENARIO_ID_CUSTOM4:
			LOG_INF("MSDK_SCENARIO_ID_CUSTOM4 = %d\n", *feature_data);
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) = (imgsensor_info.custom4.pclk /
			(imgsensor_info.custom4.linelength - 80)) * imgsensor_info.custom4.grabwindow_width;
			break;
		case MSDK_SCENARIO_ID_CUSTOM5:
			LOG_INF("MSDK_SCENARIO_ID_CUSTOM5 = %d\n", *feature_data);
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) = (imgsensor_info.custom5.pclk /
			(imgsensor_info.custom5.linelength - 80)) * imgsensor_info.custom5.grabwindow_width;
			break;
		case MSDK_SCENARIO_ID_CUSTOM6:
			LOG_INF("MSDK_SCENARIO_ID_CUSTOM6 = %d\n", *feature_data);
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) = (imgsensor_info.custom6.pclk /
			(imgsensor_info.custom6.linelength - 80)) * imgsensor_info.custom6.grabwindow_width;
			break;
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
			LOG_INF("MSDK_SCENARIO_ID_CAMERA_PREVIEW = %d\n", *feature_data);
		default:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) = (imgsensor_info.pre.pclk /
			(imgsensor_info.pre.linelength - 80)) * imgsensor_info.pre.grabwindow_width;
			break;
		}
		break;

	case SENSOR_FEATURE_GET_MIPI_PIXEL_RATE:
		LOG_DBG("SENSOR_FEATURE_GET_MIPI_PIXEL_RATE = %d\n", feature_id);
		if (enable_adaptive_mipi) {
			LOG_INF("Update mipi info for adaptive mipi");
			update_mipi_info(*feature_data);
		}
		switch (*feature_data) {
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			LOG_INF("MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG = %d\n", *feature_data);
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) = imgsensor_info.cap.mipi_pixel_rate;
			break;
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			LOG_INF("MSDK_SCENARIO_ID_VIDEO_PREVIEW = %d\n", *feature_data);
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) = imgsensor_info.normal_video.mipi_pixel_rate;
			break;
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
			LOG_INF("MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO = %d\n", *feature_data);
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) = imgsensor_info.hs_video.mipi_pixel_rate;
			break;
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
			LOG_INF("MSDK_SCENARIO_ID_SLIM_VIDEO = %d\n", *feature_data);
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) = imgsensor_info.slim_video.mipi_pixel_rate;
			break;

		case MSDK_SCENARIO_ID_CUSTOM1:
			LOG_INF("MSDK_SCENARIO_ID_CUSTOM1 = %d\n", *feature_data);
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) = imgsensor_info.custom1.mipi_pixel_rate;
			break;
		case MSDK_SCENARIO_ID_CUSTOM2:
			LOG_INF("MSDK_SCENARIO_ID_CUSTOM2 = %d\n", *feature_data);
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
			imgsensor_info.custom2.mipi_pixel_rate;
			break;
		case MSDK_SCENARIO_ID_CUSTOM3:
			LOG_INF("MSDK_SCENARIO_ID_CUSTOM3 = %d\n", *feature_data);
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
			imgsensor_info.custom3.mipi_pixel_rate;
			break;
		case MSDK_SCENARIO_ID_CUSTOM4:
			LOG_INF("MSDK_SCENARIO_ID_CUSTOM4 = %d\n", *feature_data);
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
			imgsensor_info.custom4.mipi_pixel_rate;
			break;
		case MSDK_SCENARIO_ID_CUSTOM5:
			LOG_INF("MSDK_SCENARIO_ID_CUSTOM5 = %d\n", *feature_data);
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
			imgsensor_info.custom5.mipi_pixel_rate;
			break;
		case MSDK_SCENARIO_ID_CUSTOM6:
			LOG_INF("MSDK_SCENARIO_ID_CUSTOM6 = %d\n", *feature_data);
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
			imgsensor_info.custom6.mipi_pixel_rate;
			break;
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
			LOG_INF("MSDK_SCENARIO_ID_CAMERA_PREVIEW = %d\n", *feature_data);
		default:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) = imgsensor_info.pre.mipi_pixel_rate;
			break;
		}
		break;
	case SENSOR_FEATURE_GET_BINNING_TYPE:
		switch (*(feature_data + 1)) {
		case MSDK_SCENARIO_ID_CUSTOM2:
		case MSDK_SCENARIO_ID_CUSTOM3:
			*feature_return_para_32 = 1;
		break;
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		case MSDK_SCENARIO_ID_CUSTOM1:
		case MSDK_SCENARIO_ID_CUSTOM4:
		case MSDK_SCENARIO_ID_CUSTOM5:
		case MSDK_SCENARIO_ID_CUSTOM6:
		default:
			*feature_return_para_32 = 3;
			break;
		}
		LOG_INF("scenario_id %d, binning_type %d,\n", *(feature_data + 1), *feature_return_para_32);
		*feature_para_len = 4;
		break;
	case SENSOR_FEATURE_GET_AWB_REQ_BY_SCENARIO:
		switch (*feature_data) {
#if defined(CONFIG_TRAN_CAMERA_SYNC_AWB_TO_KERNEL)
		case MSDK_SCENARIO_ID_CUSTOM2:
		case MSDK_SCENARIO_ID_CUSTOM3:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) = 1;
			break;
#endif
		default:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) = 0;
			break;
		}
		break;
	case SENSOR_FEATURE_GET_RAWBIT_BY_SCENARIO:
		{
			if ((feature_data) == NULL) {
				LOG_ERR("input pScenarios is NULL!\n");
				return ERROR_INVALID_SCENARIO_ID;
			}

			switch (*feature_data) {
			case MSDK_SCENARIO_ID_CUSTOM4:
			case MSDK_SCENARIO_ID_CUSTOM5:
				*(feature_data+1) = RAW_SENSOR_12BIT;
				LOG_INF("scenario_id %d: RAW12\n", *(feature_data));
				break;
			case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
			case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
			case MSDK_SCENARIO_ID_SLIM_VIDEO:
			case MSDK_SCENARIO_ID_CUSTOM1:
			case MSDK_SCENARIO_ID_CUSTOM2:
			case MSDK_SCENARIO_ID_CUSTOM3:
			case MSDK_SCENARIO_ID_CUSTOM6:
				*(feature_data+1) = RAW_SENSOR_10BIT;
				LOG_INF("scenario_id %d: RAW10\n", *(feature_data));
				break;
			default:
				*(feature_data+1) = RAW_SENSOR_ERROR;
				LOG_ERR("Invalid scenario id: %d\n", *(feature_data));
				break;
			}
		}
		break;
	case SENSOR_FEATURE_SET_FAST_CHANGE_MODE:
		LOG_INF("Fast Change Mode:%d", *feature_data);
#ifdef USE_RAW12
		if ((imgsensor.sensor_mode == IMGSENSOR_MODE_CUSTOM4 ||
			imgsensor.sensor_mode == IMGSENSOR_MODE_CUSTOM5) &&
			has_otp() && *feature_data < IMGSENSOR_FC_SET_MODE_HAL_MAX)
			fastchange_set_mode(*feature_data);
#endif
		break;
	case SENSOR_FEATURE_GET_GAIN_RANGE_BY_SCENARIO:
		*(feature_data + 1) = imgsensor_info.min_gain;
		*(feature_data + 2) = imgsensor_info.max_gain;
		break;
	case SENSOR_FEATURE_GET_BASE_GAIN_ISO_AND_STEP:
		*(feature_data + 0) = imgsensor_info.min_gain_iso;
		*(feature_data + 1) = imgsensor_info.gain_step;
		*(feature_data + 2) = imgsensor_info.gain_type;
		break;
	case SENSOR_FEATURE_GET_MIN_SHUTTER_BY_SCENARIO:
		*(feature_data + 1) = imgsensor_info.min_shutter;
		*(feature_data + 2) = imgsensor_info.exp_step;
		break;
	case SENSOR_FEATURE_SET_ADAPTIVE_MIPI:
		if (*feature_para == 1) {
			LOG_INF("Eanble Adaptive MIPI.\n");
			enable_adaptive_mipi = true;
		}
		break;
	default:
		break;
	}

	return ERROR_NONE;
}    /*    feature_control()  */


static struct SENSOR_FUNCTION_STRUCT sensor_func = {
	open,
	get_info,
	get_resolution,
	feature_control,
	control,
	close
};

UINT32 S5KHM6_MIPI_RAW_SensorInit(struct SENSOR_FUNCTION_STRUCT **pfFunc)
{
	/* To Do : Check Sensor status here */
	if (pfFunc != NULL)
		*pfFunc = &sensor_func;
	return ERROR_NONE;
}
