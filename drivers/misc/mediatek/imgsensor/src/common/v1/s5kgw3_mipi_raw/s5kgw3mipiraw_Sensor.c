/*
 * Copyright (C) 2018 MediaTek Inc.
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
 *	 s5kgw3mipiraw_Sensor.c
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
#define PFX "S5KGW3"
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

#include "kd_camera_typedef.h"
#include "kd_imgsensor.h"

#include "kd_imgsensor_define.h"
#include "kd_imgsensor_errcode.h"

#include "kd_imgsensor_sysfs_adapter.h"

#include "s5kgw3mipiraw_Sensor.h"
#include "s5kgw3mipiraw_setfile.h"
#include "gc607_setfile.h"

#define WRITE_COMPANION_CAL                     (1)
#define ENABLE_COMPANION_I2C_BURST_WRITE        (1)

#define MULTI_WRITE 1
#if MULTI_WRITE
static const int I2C_BUFFER_LEN = 1020;
#else
static const int I2C_BUFFER_LEN = 4;
#endif

static unsigned short check_sensor_version;

static kal_uint32 iGr_gain;
static kal_uint32 iR_gain;
static kal_uint32 iB_gain;
static kal_uint32 iGb_gain;
static kal_uint32 iGain;

#if WRITE_COMPANION_CAL
static bool remosaic_cal_loading_completed;
#endif

#define ENABLE_GC607_DEBUG_INFO
#ifdef ENABLE_GC607_DEBUG_INFO
#define GC607_PLL_STATE 0xFFF9
#define GC607_LDO_STATE 0xFFFA
// RX size H = H_1 << 8 | H_2
// RX size W = W_1 << 8 | W_2
#define GC607_RX_SIZE_H_1 0x2055
#define GC607_RX_SIZE_H_2 0x2056
#define GC607_RX_SIZE_W_1 0x2057
#define GC607_RX_SIZE_W_2 0x2058
#define GC607_TX_ERR_CHK  0x300F
static bool check_done;
#endif

/**************************** Modify end *****************************/

#define LOG_DEBUG 1
#if LOG_DEBUG
#define LOG_INF(format, args...) pr_info(PFX "[%s] " format, __func__, ##args)
#endif

#define SENSOR_GW3_COARSE_INTEGRATION_TIME_MAX_MARGIN       (5)
#define SENSOR_GW3_MAX_COARSE_INTEGRATION_TIME              (65503)

static kal_bool sIsNightHyperlapse = KAL_FALSE;
static bool use_companion_gc607;

static DEFINE_SPINLOCK(imgsensor_drv_lock);
//Sensor ID Value: 0x7309//record sensor id defined in Kd_imgsensor.h
static struct imgsensor_info_struct imgsensor_info = {
	.sensor_id = S5KGW3_SENSOR_ID,
	.checksum_value = 0xc98e6b72,
	.pre = {
		.pclk = 797300000,
		.linelength  = 7312,
		.framelength = 3634,
		.startx = 0,
		.starty = 0,
		.grabwindow_width  = 4624,
		.grabwindow_height = 3468,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 300,
		.mipi_pixel_rate = 821600000,
	},
	.cap = {
		.pclk = 797300000,
		.linelength  = 7312,
		.framelength = 3634,
		.startx = 0,
		.starty = 0,
		.grabwindow_width  = 4624,
		.grabwindow_height = 3468,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 300,
		.mipi_pixel_rate = 821600000,
	},
	.normal_video = {
		.pclk = 797300000,
		.linelength  = 9624,
		.framelength = 2760,
		.startx = 0,
		.starty = 0,
		.grabwindow_width  = 4624,
		.grabwindow_height = 2604,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 300,
		.mipi_pixel_rate = 821600000,
	},
	.hs_video = {
		.pclk = 797300000,
		.linelength  = 2692,
		.framelength = 2452,
		.startx = 0,
		.starty = 0,
		.grabwindow_width  = 1920,
		.grabwindow_height = 1080,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 1200,
		.mipi_pixel_rate = 821600000,
	},
	.slim_video = {
		.pclk = 797300000,
		.linelength  = 4224,
		.framelength = 1612,
		.startx = 0,
		.starty = 0,
		.grabwindow_width  = 1920,
		.grabwindow_height = 1440,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 940,
		.mipi_pixel_rate = 821600000,
	},
	.custom1 = {
		.pclk = 797300000,
		.linelength  = 3288,
		.framelength = 4040,
		.startx = 0,
		.starty = 0,
		.grabwindow_width  = 2304,
		.grabwindow_height = 1728,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 600,
		.mipi_pixel_rate = 821600000,
	},
	.custom2 = {
		.pclk = 797300000,
		.linelength  = 11184,
		.framelength = 7116,
		.startx = 0,
		.starty = 0,
		.grabwindow_width  = 9248,
		.grabwindow_height = 6936,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 100,
		.mipi_pixel_rate = 821600000,
	},
	.custom3 = { // NOT USED
		.pclk = 797300000,
		.linelength  = 8416,
		.framelength = 3155,
		.startx = 0,
		.starty = 0,
		.grabwindow_width  = 4000,
		.grabwindow_height = 3000,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 300,
		.mipi_pixel_rate = 821600000,
	},
	.custom4 = { // NOT USED
		.pclk = 797300000,
		.linelength  = 8416,
		.framelength = 3155,
		.startx = 0,
		.starty = 0,
		.grabwindow_width  = 4000,
		.grabwindow_height = 3000,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 300,
		.mipi_pixel_rate = 821600000,
	},
	.custom5 = {
		.pclk = 797300000,
		.linelength  = 8416,
		.framelength = 3155,
		.startx = 0,
		.starty = 0,
		.grabwindow_width  = 4000,
		.grabwindow_height = 3000,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 300,
		.mipi_pixel_rate = 821600000,
	},
	.margin = 10,
	.min_shutter = 8,
	.max_frame_length = 0xFFFF,

	.min_gain = 73,
	.max_gain = 4096,
	.min_gain_iso = 40,
	.exp_step = 1,
	.gain_step = 2,
	.gain_type = 2,

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
	.sensor_mode_num = 10,

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
	.slim_video_delay_frame = 3,

#ifdef CONFIG_IMPROVE_CAMERA_PERFORMANCE
	.custom1_delay_frame = 1,
	.custom2_delay_frame = 1,
	.custom5_delay_frame = 1,
#else
	.custom1_delay_frame = 3,
	.custom2_delay_frame = 3,
	.custom5_delay_frame = 3,
#endif

	.isp_driving_current = ISP_DRIVING_4MA,
	.sensor_interface_type = SENSOR_INTERFACE_TYPE_MIPI,
	.mipi_sensor_type = MIPI_OPHY_NCSI2,
	.mipi_settle_delay_mode = 1,
	.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_4CELL_Gr,
	.mclk = 26,
	.mipi_lane_num = SENSOR_MIPI_4_LANE,
	.i2c_speed = 1000,

	/* record sensor support all write id addr,
	 * only supprt 4 must end with 0xff
	 */
	.i2c_addr_table = {0x5a, 0xff},//see moudle spec
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
	.i2c_companion_write_id = 0x42,
};

/*VC1 for L-PD(DT=0X34) , VC2 for R-PD(DT=0X31), unit : 10bit */
struct SENSOR_VC_INFO_STRUCT SENSOR_VC_INFO[6] = {
	/* Preview mode setting */
	{0x02, 0x0a, 0x00, 0x08, 0x40, 0x00,
	 0x00, 0x2b, 0x1210, 0x0d8c,/*VC0*/
	 0x00, 0x00, 0x0000, 0x0000,/*VC1*/
	 0x01, 0x2b, 0x02DA, 0x0d80,/*VC2, 584x3456(by pixel) 736x3456(by byte)*/
	 0x00, 0x00, 0x0000, 0x0000},/*VC3*/
	/* cap = pre mode setting */
	{0x02, 0x0a, 0x00, 0x08, 0x40, 0x00,
	 0x00, 0x2b, 0x1210, 0x0d8c,/*VC0*/
	 0x00, 0x00, 0x0000, 0x0000,/*VC1*/
	 0x01, 0x2b, 0x02DA, 0x0d80,/*VC2, 584x3456(by pixel) 736x3456(by byte)*/
	 0x00, 0x00, 0x0000, 0x0000},/*VC3*/
	/* Video mode setting */
	{0x02, 0x0a, 0x00, 0x08, 0x40, 0x00,
	 0x00, 0x2b, 0x1210, 0x0a2c,
	 0x00, 0x00, 0x0000, 0x0000,
	 0x01, 0x2b, 0x02DA, 0x0a20,/*584x2592(by pixel) 736x2592(by byte)*/
	 0x00, 0x00, 0x0000, 0x0000},
	/* hs mode setting */
	{0x01, 0x0a, 0x00, 0x08, 0x40, 0x00,
	 0x00, 0x2b, 0x0780, 0x0438,
	 0x00, 0x00, 0x00, 0x00,
	 0x00, 0x00, 0x00, 0x00,
	 0x00, 0x00, 0x0000, 0x0000},
	/* slim mode setting */
	{0x01, 0x0a, 0x00, 0x08, 0x40, 0x00,
	 0x00, 0x2b, 0x0780, 0x0438,
	 0x00, 0x00, 0x00, 0x00,
	 0x00, 0x00, 0x0000, 0x0000,
	 0x00, 0x00, 0x0000, 0x0000},
	/* Custom5 mode setting */
	{0x02, 0x0a, 0x00, 0x08, 0x40, 0x00,
	 0x00, 0x2b, 0x0FA0, 0x0BB8,
	 0x00, 0x00, 0x0000, 0x0000,
	 0x01, 0x2b, 0x02DA, 0x0d80,/*504x2992 (by pixel) crop for 4-alignment 628*2992 (by byte)*/
	 0x00, 0x00, 0x0000, 0x0000}
};

struct SENSOR_VC_INFO_STRUCT SENSOR_COMPANION_VC_INFO[6] = {
	/* Preview mode setting */
	{0x02, 0x0a, 0x00, 0x08, 0x40, 0x00,
	 0x00, 0x2b, 0x1210, 0x0d8c,/*VC0*/
	 0x00, 0x00, 0x0000, 0x0000,/*VC1*/
	 0x00, 0x30, 0x02D8, 0x0d80,/*VC2, 584x3456(by pixel) 736x3456(by byte)*/
	 0x00, 0x00, 0x0000, 0x0000},/*VC3*/
	/* cap = pre mode setting */
	{0x02, 0x0a, 0x00, 0x08, 0x40, 0x00,
	 0x00, 0x2b, 0x1210, 0x0d8c,/*VC0*/
	 0x00, 0x00, 0x0000, 0x0000,/*VC1*/
	 0x00, 0x30, 0x02D8, 0x0d80,/*VC2, 584x3456(by pixel) 736x3456(by byte)*/
	 0x00, 0x00, 0x0000, 0x0000},/*VC3*/
	/* Video mode setting */
	{0x02, 0x0a, 0x00, 0x08, 0x40, 0x00,
	 0x00, 0x2b, 0x1210, 0x0a2c,
	 0x00, 0x00, 0x0000, 0x0000,
	 0x00, 0x30, 0x02D8, 0x0a20,/*584x2592(by pixel) 736x2592(by byte)*/
	 0x00, 0x00, 0x0000, 0x0000},
	/* hs mode setting */
	{0x01, 0x0a, 0x00, 0x08, 0x40, 0x00,
	 0x00, 0x2b, 0x0780, 0x0438,
	 0x00, 0x00, 0x00, 0x00,
	 0x00, 0x00, 0x00, 0x00,
	 0x00, 0x00, 0x0000, 0x0000},
	/* slim mode setting */
	{0x01, 0x0a, 0x00, 0x08, 0x40, 0x00,
	 0x00, 0x2b, 0x0780, 0x0438,
	 0x00, 0x00, 0x00, 0x00,
	 0x00, 0x00, 0x0000, 0x0000,
	 0x00, 0x00, 0x0000, 0x0000},
	/* Custom5 mode setting */
	{0x02, 0x0a, 0x00, 0x08, 0x40, 0x00,
	 0x00, 0x2b, 0x0FA0, 0x0BB8,
	 0x00, 0x00, 0x0000, 0x0000,
	 0x00, 0x30, 0x0274, 0x0BB0,/*504x2992 (by pixel) crop for 4-alignment 628*2992 (by byte)*/
	 0x00, 0x00, 0x0000, 0x0000}
};

static struct SENSOR_WINSIZE_INFO_STRUCT imgsensor_winsize_info[10] = {
	{ 9312, 6976,   16,    0, 9280, 6976, 4640, 3488,
	     8,   10, 4624, 3468,    0,    0, 4624, 3468 }, /* preview */
	{ 9312, 6976,   16,    0, 9280, 6976, 4640, 3488,
	     8,   10, 4624, 3468,    0,    0, 4624, 3468 }, /* capture */
	{ 9312, 6976,   16,  864, 9280, 5248, 4640, 2624,
	     8,   10, 4624, 2604,    0,    0, 4624, 2604 }, /* video */
	{ 9312, 6976,  800, 1312, 7712, 4352, 1928, 1088,
	     4,    4, 1920, 1080,    0,    0, 1920, 1080 }, /* hs video */
	{ 9312, 6976,  800, 1312, 7712, 4352, 1928, 1088,
	     4,    4, 1920, 1080,    0,    0, 1920, 1080 }, /* slim video */
	{ 9312, 6976,   32,   16, 9248, 6944, 2312, 1736,
	     4,    4, 2304, 1728,    0,    0, 2304, 1728 }, /* custom1 */
	{ 9312, 6976,   32,   20, 9248, 6936, 9248, 6936,
	     0,    0, 9248, 6936,    0,    0, 9248, 6936 }, /* custom2 */
	{ 9312, 6976,   32,   16, 9248, 6944, 2312, 1736,
	     4,    4, 2304, 1728,    0,    0, 2304, 1728 }, /* custom3 - NOT USED */
	{ 9312, 6976,   16,    0, 9280, 6976, 9280, 6976,
	    16,   20, 9248, 6936,    0,    0, 9248, 6936 }, /* custom4 - NOT USED */
	{ 9312, 6976,  640,  464, 8032, 6048, 4016, 3024,
	     8,   12, 4000, 3000,    0,    0, 4000, 3000 }, /* custom5 */
};

static struct SET_PD_BLOCK_INFO_T imgsensor_pd_info = {
	.i4OffsetX	= 0,
	.i4OffsetY	= 6,
	.i4PitchX	= 8,
	.i4PitchY	= 8,
	.i4PairNum	= 4,
	.i4SubBlkW	= 8,
	.i4SubBlkH	= 2,
	.i4BlockNumX	= 578,
	.i4BlockNumY	= 432,
	.iMirrorFlip	= 0,
	.i4PosR = {
		{0, 6}, {2, 9}, {6, 10}, {4, 13}
	},
	.i4PosL = {
		{1, 6}, {3, 9}, {7, 10}, {5, 13}
	},
	.i4Crop = {
		{0, 0}, {0, 0}, {0, 432}, {0, 0}, {0, 0},
		{0, 0}, {0, 0}, {0, 0},   {0, 0}, {312, 234}
	}
};

void write_cmos_sensor(kal_uint16 addr, kal_uint16 para)
{
	char pusendcmd[4] = {
		(char)(addr >> 8), (char)(addr & 0xFF),
		(char)(para >> 8), (char)(para & 0xFF) };
	iWriteRegI2CTiming(pusendcmd, 4,
		imgsensor.i2c_write_id, imgsensor_info.i2c_speed);
}

static kal_uint16 read_cmos_sensor_8(kal_uint16 addr)
{
	kal_uint16 get_byte = 0;
	char pusendcmd[2] = { (char)(addr >> 8), (char)(addr & 0xFF) };

	iReadRegI2CTiming(pusendcmd, 2, (u8 *) &get_byte, 1,
		imgsensor.i2c_write_id, imgsensor_info.i2c_speed);
	return get_byte;
}

static kal_uint16 read_companion_8(kal_uint16 addr)
{
	kal_uint16 get_byte = 0;
	char pusendcmd[2] = { (char)(addr >> 8), (char)(addr & 0xFF) };

	iReadRegI2CTiming(pusendcmd, 2, (u8 *) &get_byte, 1,
		imgsensor.i2c_companion_write_id, imgsensor_info.i2c_speed);

	return get_byte;
}

static void write_cmos_sensor_8(kal_uint16 addr, kal_uint8 para)
{
	char pusendcmd[3] = {
		(char)(addr >> 8), (char)(addr & 0xFF), (char)(para & 0xFF) };

	iWriteRegI2CTiming(pusendcmd, 3,
		imgsensor.i2c_write_id, imgsensor_info.i2c_speed);
}

static void write_companion_8(kal_uint16 addr, kal_uint8 para)
{
	char pusendcmd[3] = {
		(char)(addr >> 8), (char)(addr & 0xFF), (char)(para & 0xFF) };

	iWriteRegI2CTiming(pusendcmd, 3,
		imgsensor.i2c_companion_write_id, imgsensor_info.i2c_speed);
}

#ifdef ENABLE_GC607_DEBUG_INFO
static bool check_flow_of_gc607(void)
{
	kal_uint32 i = 0;
	kal_uint16 value = 0;
	bool ret = true;

	// skip remosaic capture case
	if (imgsensor.current_scenario_id == MSDK_SCENARIO_ID_CUSTOM2)
		return true;

	//write_companion_8(GC607_TX_ERR_CHK, 0x0F);

	value = read_companion_8(GC607_PLL_STATE);
	pr_info("GC607 PLL state (expect data : 0x07), addr:data (%#06x:%#04x)\n",
		GC607_PLL_STATE, value);
	if (value != 0x07)
		ret = false;

	value = read_companion_8(GC607_LDO_STATE);
	pr_info("GC607 LDO state (expect data : 0x07), addr:data (%#06x:%#04x)\n",
		GC607_LDO_STATE, value);
	if (value != 0x07)
		ret = false;

	for (i = 0; i < 3; i++) {
		value = read_companion_8(GC607_RX_SIZE_H_1) << 8;
		value += read_companion_8(GC607_RX_SIZE_H_2);
		pr_info("GC607 image height : %d\n", value);
		if (i == 2 && (value < 1080 || value > 6936))
			ret = false;

		value = read_companion_8(GC607_RX_SIZE_W_1) << 8;
		value += read_companion_8(GC607_RX_SIZE_W_2);
		pr_info("GC607 image width : %d\n", value * 4);
		if (i == 2 && ((value < (2304 >> 2)) || value > 9248))
			ret = false;
	}

	write_companion_8(GC607_TX_ERR_CHK, 0x0F);

	value = read_companion_8(GC607_TX_ERR_CHK);
	pr_info("GC607 Check output block (expect data : 0x00), addr:data (%#06x:%#04x)\n",
		GC607_TX_ERR_CHK, value);
	if (value != 0x00)
		ret = false;

	return ret;
}
#endif

static void set_dummy(void)
{
	pr_debug("dummyline = %d, dummypixels = %d\n",
		imgsensor.dummy_line, imgsensor.dummy_pixel);

	write_cmos_sensor_8(0x702, 0); // FLL Lshift addr
	write_cmos_sensor_8(0x704, 0); // CIT Lshift addr
	write_cmos_sensor(0x0340, imgsensor.frame_length);
	write_cmos_sensor(0x0342, imgsensor.line_length);
}

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

static kal_uint16 table_companion_write_cmos_sensor(kal_uint16 *para, kal_uint32 len)
{
#if ENABLE_COMPANION_I2C_BURST_WRITE
	char puSendCmd[I2C_BUFFER_LEN];
	kal_uint32 tosend = 0, IDX = 0;
	kal_uint16 addr = 0, data = 0;

	while (len > IDX) {
		addr = para[IDX];
		if (tosend == 0) {
			puSendCmd[tosend++] = (char)(addr >> 8);
			puSendCmd[tosend++] = (char)(addr & 0xFF);
		}

		data = para[IDX + 1];
		puSendCmd[tosend++] = (char)(data & 0xFF);
		IDX += 2;

		if ((len == IDX) ||
			((len > IDX) && (para[IDX] != (addr + 1)))) {
			//address: 2 byte, data: N byte
			iBurstWriteReg_multi(puSendCmd, tosend, imgsensor.i2c_companion_write_id,
								tosend, imgsensor_info.i2c_speed);
			tosend = 0;
		}
	}
#else
	kal_uint32 i = 0, max_count = len / 2;
	char pusendcmd[3] = { 0, };

	for (i = 0; i < max_count; i++) {
		//address: 2 byte, data: 1 byte
		pusendcmd[0] = (char)(para[2 * i] >> 8);
		pusendcmd[1] = (char)(para[2 * i] & 0xFF);
		pusendcmd[2] = (char)(para[2 * i + 1] & 0xFF);
		iWriteRegI2CTiming(pusendcmd, 3,
			imgsensor.i2c_companion_write_id, imgsensor_info.i2c_speed);
	}
#endif
	return 0;
}


static void set_max_framerate(UINT16 framerate, kal_bool min_framelength_en)
{
	kal_uint32 frame_length = imgsensor.frame_length;

	pr_debug("framerate = %d, min framelength should enable %d\n",
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

/*
 * parameter[onoff]: true[on], false[off]
 *
 * Read sensor frame counter (sensor_fcount address = 0x0005)
 * stream on (0x00 ~ 0xFE), stream off (0xFF)
 */
static void wait_stream_onoff(bool onoff)
{
	unsigned int i = 0;
	unsigned int check_delay = onoff ? 2 : 1;
	int timeout = (10000 / imgsensor.current_fps) + 1;

	if (onoff)
		mDELAY(3);

	for (i = 0; i < timeout; i++) {
		read_cmos_sensor_8(0x0004);
		if (onoff) {
			if (read_cmos_sensor_8(0x0005) == 0xFF)
				mDELAY(check_delay);
			else
				break;
		} else {
			if (read_cmos_sensor_8(0x0005) != 0xFF)
				mDELAY(check_delay);
			else
				break;
		}
	}

	//delay for GC607
	mDELAY(1);

	if (i >= timeout)
		pr_err("%s[onoff:%d] timeout, wait_time:%d , frameCnt:0x%x\n", __func__, onoff, onoff ? i*check_delay + 4 : i*check_delay + 1, read_cmos_sensor_8(0x0005));
	else
		LOG_INF("%s[onoff:%d], wait_time:%d, frameCnt:0x%x\n", __func__, onoff, onoff ? i*check_delay + 4 : i*check_delay + 1, read_cmos_sensor_8(0x0005));
}

static kal_uint32 streaming_control(kal_bool enable)
{
	LOG_INF("streaming_enable(0=Sw Standby,1=streaming): %d\n", enable);

	if (enable) {
		LOG_INF("streaming_control() enable = %d\n", enable);
		write_cmos_sensor(0x0100, 0x0100);
	} else {
		LOG_INF("streaming_control() streamoff enable = %d\n", enable);
		write_cmos_sensor(0x0100, 0x0000);
	}
	wait_stream_onoff(enable);

	return ERROR_NONE;
}

static void write_shutter(kal_uint32 shutter)
{
	kal_uint32 realtime_fps = 0;
	kal_uint32 target_exp = 0;
	kal_uint32 target_frame_duration = 0;
	kal_uint32 frame_length_lines = 0;
	kal_uint32 cit_lshift_val = 0;
	kal_uint32 cit_lshift_count = 0;

	kal_uint32 min_fine_int = 0;
	kal_uint32 vt_pic_clk_freq_mhz = 0;
	kal_uint32 line_length_pck = 0;

	vt_pic_clk_freq_mhz = imgsensor.pclk / (1000 * 1000);
	line_length_pck = imgsensor.line_length;
	min_fine_int = 0x0100;

	target_exp = ((shutter * line_length_pck) + min_fine_int) / vt_pic_clk_freq_mhz;

	spin_lock(&imgsensor_drv_lock);
	if (shutter > imgsensor.min_frame_length - imgsensor_info.margin)
		imgsensor.frame_length = shutter + imgsensor_info.margin;
	else
		imgsensor.frame_length = imgsensor.min_frame_length;
	if (imgsensor.frame_length > imgsensor_info.max_frame_length)
		imgsensor.frame_length = imgsensor_info.max_frame_length;
	spin_unlock(&imgsensor_drv_lock);
	shutter = (shutter < imgsensor_info.min_shutter) ? imgsensor_info.min_shutter : shutter;
	shutter = (shutter > (imgsensor_info.max_frame_length - imgsensor_info.margin)) ? (imgsensor_info.max_frame_length - imgsensor_info.margin) : shutter;

	if (sIsNightHyperlapse) {
		// fps = 1/1.5
		// frame_length = pclk / (line_length * fps)
		imgsensor.frame_length = (((imgsensor.pclk / (1000 * 1000)) * (1500000))/imgsensor.line_length);
	}
	target_frame_duration = ((imgsensor.frame_length * imgsensor.line_length) / (imgsensor.pclk / 1000000));

	if (target_frame_duration > 100000) {
		int max_coarse_integration_time =
			SENSOR_GW3_MAX_COARSE_INTEGRATION_TIME - SENSOR_GW3_COARSE_INTEGRATION_TIME_MAX_MARGIN;

		cit_lshift_val = (kal_uint32)(target_frame_duration / 100000);
		while (cit_lshift_val > 1) {
			cit_lshift_val /= 2;
			target_frame_duration /= 2;
			target_exp /= 2;
			cit_lshift_count++;
		}
		if (cit_lshift_count > 11)
			cit_lshift_count = 11;

		frame_length_lines = (kal_uint32)((vt_pic_clk_freq_mhz * target_frame_duration) / line_length_pck);
		imgsensor.frame_length = frame_length_lines;

		shutter = ((target_exp * vt_pic_clk_freq_mhz) - min_fine_int) / line_length_pck;
		if (shutter > max_coarse_integration_time)
			shutter = max_coarse_integration_time;

		write_cmos_sensor(0x0340, imgsensor.frame_length & 0xFFFF);
		write_cmos_sensor_8(0x702, cit_lshift_count & 0xFF); // FLL_Lshift addr
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
	pr_debug("Exit! shutter =%d, framelength =%d\n", shutter, imgsensor.frame_length);
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
	pr_info("Add for N3D! shutter =%d, framelength =%d\n",
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

	if (gain < BASEGAIN) { //min limit
		pr_info("Error gain setting set gain %d -> %d", gain, BASEGAIN);
		gain = BASEGAIN;
	} else if (gain > 64 * BASEGAIN) { //max limit
		pr_info("Error gain setting set gain %d -> %d", gain, 64 * BASEGAIN);
		gain = 64 * BASEGAIN;
	}

	iGain = gain;
	reg_gain = gain2reg(gain);
	spin_lock(&imgsensor_drv_lock);
	imgsensor.gain = reg_gain;
	spin_unlock(&imgsensor_drv_lock);
	pr_debug("%s = %d , reg_gain = 0x%x\n ", __func__, gain, reg_gain);

	write_cmos_sensor(0x0204, (reg_gain&0xFFFF));

	if (use_companion_gc607) {
#if WRITE_COMPANION_CAL
		if (imgsensor.current_scenario_id == MSDK_SCENARIO_ID_CUSTOM2) {
			kal_uint16 cicgain = 0;

			if (gain > 20 * BASEGAIN)
				cicgain = 20 * BASEGAIN;
			else
				cicgain = gain;
			write_companion_8(0x0026, (kal_uint8)cicgain>>6);
		}
#endif
	}
	return gain;
}

static void set_awbgain(kal_uint32 g_gain, kal_uint32 r_gain, kal_uint32 b_gain, kal_bool force)
{
	pr_debug("%s enter gr_gain=0x%x->0x%x, r_gain=0x%x->0x%x, b_gain=0x%x->0x%x, gb_gain=0x%x->0x%x\n",
			__func__, iGr_gain, g_gain, iR_gain, r_gain, iB_gain, b_gain, iGb_gain, g_gain);

	if ((iGr_gain != g_gain) || (iR_gain != r_gain) || (iB_gain != b_gain) || (iGb_gain != g_gain) || force) {
		// AWB gain 1x value is 512 in HAL, 256 in sensor register.
		// So it converted with round up
		kal_uint32 sMax_gain = 16*256-1;
		kal_uint32 sGr_gain = (g_gain+1) >> 1;// The sensor register is 12 bits length, so it is limited to about 16x.
		kal_uint32 sR_gain = (r_gain+1) >> 1;
		kal_uint32 sB_gain = (b_gain+1) >> 1;
		kal_uint32 sGb_gain = (g_gain+1) >> 1;

		sGr_gain = (sGr_gain <= sMax_gain) ? sGr_gain : sMax_gain;
		sR_gain = (sR_gain <= sMax_gain) ? sR_gain : sMax_gain;
		sB_gain = (sB_gain <= sMax_gain) ? sB_gain : sMax_gain;
		sGb_gain = (sGb_gain <= sMax_gain) ? sGb_gain : sMax_gain;

		write_cmos_sensor(0x020E, sGr_gain);
		write_cmos_sensor(0x0210, sR_gain);
		write_cmos_sensor(0x0212, sB_gain);
		write_cmos_sensor(0x0214, sGb_gain);
		pr_debug("%s sensor gr_gain=0x%x, r_gain=0x%x, b_gain=0x%x, gb_gain=0x%x\n",
				__func__, sGr_gain, sR_gain, sB_gain, sGb_gain);

		if (imgsensor.current_scenario_id == MSDK_SCENARIO_ID_CUSTOM2) {
			kal_uint32 cMax_gain = 0x00ff;
			kal_uint32 cGr_gain = (g_gain+1) >> 3; // The sensor register is 8 bits length, so it is limited to about 4x.
			kal_uint32 cR_gain = (r_gain+1) >> 3;
			kal_uint32 cB_gain = (b_gain+1) >> 3;
			kal_uint32 cGb_gain = (g_gain+1) >> 3;

			cGr_gain = (cGr_gain <= cMax_gain) ? cGr_gain : cMax_gain;
			cR_gain = (cR_gain <= cMax_gain) ? cR_gain : cMax_gain;
			cB_gain = (cB_gain <= cMax_gain) ? cB_gain : cMax_gain;
			cGb_gain = (cGb_gain <= cMax_gain) ? cGb_gain : cMax_gain;

			if (use_companion_gc607) {
				write_companion_8(0x0021, (kal_uint8)cGr_gain);
				write_companion_8(0x0020, (kal_uint8)cR_gain);
				write_companion_8(0x0022, (kal_uint8)cB_gain);
				write_companion_8(0x0021, (kal_uint8)cGb_gain);
			}
			pr_debug("%s cic gr_gain=0x%x, r_gain=0x%x, b_gain=0x%x, gb_gain=0x%x\n",
					__func__, cGr_gain, cR_gain, cB_gain, cGb_gain);
		}

		iGr_gain = g_gain;
		iR_gain = r_gain;
		iB_gain = b_gain;
		iGb_gain = g_gain;
	}
}


static void set_mirror_flip(kal_uint8 image_mirror)
{
	kal_uint8 itemp;

	pr_debug("image_mirror = %d\n", image_mirror);
	itemp = read_cmos_sensor_8(0x0101);
	itemp &= ~0x03;

	switch (image_mirror) {

	case IMAGE_NORMAL:
		write_cmos_sensor_8(0x0101, itemp);
		break;

	case IMAGE_V_MIRROR:
		write_cmos_sensor_8(0x0101, itemp | 0x02);
		break;

	case IMAGE_H_MIRROR:
		write_cmos_sensor_8(0x0101, itemp | 0x01);
		break;

	case IMAGE_HV_MIRROR:
		write_cmos_sensor_8(0x0101, itemp | 0x03);
		break;
	}
}

static void sensor_init(void)
{
	LOG_INF("%s - E\n", __func__);
	if (use_companion_gc607) {
		// companion chip init
		table_companion_write_cmos_sensor(gc607_setfile_info[IMGSENSOR_MODE_INIT].setfile,
				gc607_setfile_info[IMGSENSOR_MODE_INIT].size);
		LOG_INF("%s, companion init completed\n", __func__);
	}
	write_cmos_sensor(0x6010, 0x0001);

	// init
	if (check_sensor_version == 0xA101 || check_sensor_version == 0xA102) {
		pr_info("[GW3]init-OTP write version\n");
		mDELAY(11);
		write_cmos_sensor(0x6046, 0x0085);
		write_cmos_sensor(0x6214, 0xFF7D);
		write_cmos_sensor(0x6218, 0x0000);
	} else {
		mDELAY(5);
		table_write_cmos_sensor(gw3_setfile_info[IMGSENSOR_MODE_INIT].setfile,
				gw3_setfile_info[IMGSENSOR_MODE_INIT].size);
	}

	// global
	table_write_cmos_sensor(addr_data_global_gw3,
			sizeof(addr_data_global_gw3) / sizeof(kal_uint16));
	LOG_INF("%s, sensor init completed\n", __func__);
	LOG_INF("%s - X\n", __func__);
}

#if WRITE_COMPANION_CAL

#define COMPANION_SXTC_CAL_OFFSET              (0x0170)
#define COMPANION_SXTC_CAL_SIZE                (768)

#define COMPANION_PDXTC_CAL_OFFSET             (0x0472)
#define COMPANION_PDXTC_CAL_SIZE               (108)
#define COMPANION_PDXTC_REG_ADDR               (0x021F)

kal_uint16 write_data_sxtc[2 * (COMPANION_SXTC_CAL_SIZE + 4)] = {0};
kal_uint16 write_data_pdxtc[2 * COMPANION_PDXTC_CAL_SIZE] = {0};

static void companion_cal_write(void)
{
	int i = 0;
	char *rom_cal_buf = NULL, *rom_cal_sxtc = NULL, *rom_cal_pdxtc = NULL;
	int write_size_sxtc = sizeof(write_data_sxtc) / sizeof(kal_uint16);
	int write_size_pdxtc = sizeof(write_data_pdxtc) / sizeof(kal_uint16);

	// TODO : sensor_dev_id needs to be updated according to actual id.
	int sensor_dev_id = IMGSENSOR_SENSOR_IDX_MAIN;

	LOG_INF("E");

	if (IMGSENSOR_GET_CAL_BUF_BY_SENSOR_IDX(sensor_dev_id, &rom_cal_buf)) {
		pr_err("%s : cal data is NULL, sensor_dev_id: %d", __func__, sensor_dev_id);
		return;
	}

	// SXTC write
	rom_cal_sxtc = rom_cal_buf + COMPANION_SXTC_CAL_OFFSET;
	write_data_sxtc[0] = 0x0600;
	write_data_sxtc[1] = 0x00;
	write_data_sxtc[2] = 0x0602;
	write_data_sxtc[3] = 0x00;
	write_data_sxtc[4] = 0x0603;
	write_data_sxtc[5] = 0x00;
	for (i = 0; i < (COMPANION_SXTC_CAL_SIZE/4); i++) {
		int base = (COMPANION_SXTC_CAL_SIZE/4)*(i/48)+i%48;

		write_data_sxtc[6 + 8*i] = 0x0604;
		write_data_sxtc[6 + 8*i + 1] = rom_cal_sxtc[base + 144];
		write_data_sxtc[6 + 8*i + 2] = 0x0605;
		write_data_sxtc[6 + 8*i + 3] = rom_cal_sxtc[base + 96];
		write_data_sxtc[6 + 8*i + 4] = 0x0606;
		write_data_sxtc[6 + 8*i + 5] = rom_cal_sxtc[base + 48];
		write_data_sxtc[6 + 8*i + 6] = 0x0607;
		write_data_sxtc[6 + 8*i + 7] = rom_cal_sxtc[base];
	}
	write_data_sxtc[2*COMPANION_SXTC_CAL_SIZE + 6] = 0x600;
	write_data_sxtc[2*COMPANION_SXTC_CAL_SIZE + 7] = 0x10;
	table_companion_write_cmos_sensor(write_data_sxtc, write_size_sxtc);

	// PDXTC write
	rom_cal_pdxtc = rom_cal_buf + COMPANION_PDXTC_CAL_OFFSET;
	for (i = 0; i < COMPANION_PDXTC_CAL_SIZE; i++) {
		write_data_pdxtc[2 + i] = COMPANION_PDXTC_REG_ADDR + i;
		write_data_pdxtc[2 + i + 1] = rom_cal_pdxtc[i];
	}
	table_companion_write_cmos_sensor(write_data_pdxtc, write_size_pdxtc);

	LOG_INF("X, data writing done (SXTC:%d, PDXTC:%d)", COMPANION_SXTC_CAL_SIZE, COMPANION_PDXTC_CAL_SIZE);
}
#endif

static void set_mode_setfile(enum IMGSENSOR_MODE mode)
{
	if (mode >= IMGSENSOR_MODE_MAX) {
		pr_err("%s(), invalid mode: %d\n", __func__, mode);
		return;
	}

	pr_info("%s(), mode: %s\n", __func__, gw3_setfile_info[mode].name);

	if (use_companion_gc607) {
		//set companion(GC607)
		table_companion_write_cmos_sensor(gc607_setfile_info[mode].setfile, gc607_setfile_info[mode].size);
	}
	//set sensor(GW3)
	table_write_cmos_sensor(gw3_setfile_info[mode].setfile, gw3_setfile_info[mode].size);
}

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
				pr_info("i2c write id: 0x%x, sensor id: 0x%x\n", imgsensor.i2c_write_id, *sensor_id);

				return ERROR_NONE;
			}
			pr_err("Read sensor id fail, id: 0x%x\n", imgsensor.i2c_write_id);
			retry--;
		} while (retry > 0);

		i++;
		retry = 2;
	}

	//check companion id
	pr_info("companion id: %#06x", (read_companion_8(0xFFF0) << 8) | read_companion_8(0xFFF1));

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
	kal_uint8 i = 0;
	kal_uint8 retry = 2;
	kal_uint16 sensor_id = 0;
	kal_uint16 check_companion_id = 0;
#ifdef ENABLE_GC607_DEBUG_INFO
	kal_uint16 value = 0;
#endif

	LOG_INF("%s() - E\n", __func__);

	/* sensor have two i2c address 0x6c 0x6d & 0x21 0x20,
	 * we should detect the module used i2c address
	 */
	while (imgsensor_info.i2c_addr_table[i] != 0xff) {
		spin_lock(&imgsensor_drv_lock);
		imgsensor.i2c_write_id = imgsensor_info.i2c_addr_table[i];
		spin_unlock(&imgsensor_drv_lock);

		do {
			sensor_id = ((read_cmos_sensor_8(0x0000) << 8) | read_cmos_sensor_8(0x0001));

			if (sensor_id == imgsensor_info.sensor_id) {
				pr_debug("open():i2c write id: 0x%x, sensor id: 0x%x\n", imgsensor.i2c_write_id, sensor_id);
				break;
			}

			pr_debug("%s:Read sensor id fail, id: 0x%x\n", __func__, imgsensor.i2c_write_id);
			retry--;
		} while (retry > 0);

		i++;
		if (sensor_id == imgsensor_info.sensor_id)
			break;
		retry = 2;
	}

	check_sensor_version = ((read_cmos_sensor_8(0x0002) << 8) | read_cmos_sensor_8(0x0003));
	pr_info("[GW3]check_sensor_version:%#06X\n", check_sensor_version);

	//check companion id
	check_companion_id = (read_companion_8(0xFFF0) << 8) | read_companion_8(0xFFF1);
	pr_info("companion id: %#06x", check_companion_id);

	if (check_companion_id == 0x6007)
		use_companion_gc607 = true;
	else
		use_companion_gc607 = false;

	if (imgsensor_info.sensor_id != sensor_id)
		return ERROR_SENSOR_CONNECT_FAIL;

	sensor_init();

	iGr_gain = 0;
	iR_gain = 0;
	iB_gain = 0;
	iGb_gain = 0;
	iGain = 0;

	spin_lock(&imgsensor_drv_lock);

	imgsensor.autoflicker_en	= KAL_FALSE;
	imgsensor.sensor_mode		= IMGSENSOR_MODE_INIT;
	imgsensor.shutter		= 0x3D0;
	imgsensor.gain			= 0x100;
	imgsensor.pclk			= imgsensor_info.pre.pclk;
	imgsensor.frame_length		= imgsensor_info.pre.framelength;
	imgsensor.line_length		= imgsensor_info.pre.linelength;
	imgsensor.min_frame_length 	= imgsensor_info.pre.framelength;
	imgsensor.dummy_pixel		= 0;
	imgsensor.dummy_line		= 0;
	imgsensor.ihdr_mode			= 0;
	imgsensor.test_pattern		= KAL_FALSE;
	imgsensor.current_fps		= imgsensor_info.pre.max_framerate;
	spin_unlock(&imgsensor_drv_lock);

	LOG_INF("%s - X\n", __func__);

	if (use_companion_gc607) {
#ifdef ENABLE_GC607_DEBUG_INFO
		//for checking companion status
		value = read_companion_8(GC607_PLL_STATE);
		pr_info("GC607 PLL state (expect data : 0x07), addr:data (%#06x:%#04x)\n", GC607_PLL_STATE, value);
		if (value != 0x07)
			return ERROR_SENSOR_POWER_ON_FAIL;

		value = read_companion_8(GC607_LDO_STATE);
		pr_info("GC607 LDO state (expect data : 0x07), addr:data (%#06x:%#04x)\n", GC607_LDO_STATE, value);
		if (value != 0x07)
			return ERROR_SENSOR_POWER_ON_FAIL;
#endif
	}

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
	pr_debug("%s E\n", __func__);
	streaming_control(false);
#ifdef ENABLE_GC607_DEBUG_INFO
	check_done = false;
#endif
	if (use_companion_gc607) {
#if WRITE_COMPANION_CAL
		remosaic_cal_loading_completed = false;
#endif
	}
	use_companion_gc607 = false;
	sIsNightHyperlapse = KAL_FALSE;

	return ERROR_NONE;
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
	pr_debug("%s E\n", __func__);

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode 			= IMGSENSOR_MODE_PREVIEW;
	imgsensor.pclk 					= imgsensor_info.pre.pclk;
	imgsensor.line_length 			= imgsensor_info.pre.linelength;
	imgsensor.frame_length 			= imgsensor_info.pre.framelength;
	imgsensor.min_frame_length 		= imgsensor_info.pre.framelength;
	imgsensor.autoflicker_en 		= KAL_FALSE;
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
	pr_debug("%s E\n", __func__);

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
	pr_debug("%s E\n", __func__);

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode 			= IMGSENSOR_MODE_VIDEO;
	imgsensor.pclk 					= imgsensor_info.normal_video.pclk;
	imgsensor.line_length 			= imgsensor_info.normal_video.linelength;
	imgsensor.frame_length 			= imgsensor_info.normal_video.framelength;
	imgsensor.min_frame_length 		= imgsensor_info.normal_video.framelength;
	imgsensor.autoflicker_en 		= KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);

	set_mode_setfile(imgsensor.sensor_mode);

	set_mirror_flip(imgsensor.mirror);

	return ERROR_NONE;
}

static kal_uint32 hs_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	pr_debug("%s E\n", __func__);

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode 			= IMGSENSOR_MODE_HIGH_SPEED_VIDEO;
	imgsensor.pclk 				= imgsensor_info.hs_video.pclk;
	imgsensor.line_length 			= imgsensor_info.hs_video.linelength;
	imgsensor.frame_length 			= imgsensor_info.hs_video.framelength;
	imgsensor.min_frame_length 		= imgsensor_info.hs_video.framelength;

	imgsensor.dummy_line 			= 0;
	imgsensor.dummy_pixel 			= 0;
	imgsensor.autoflicker_en 		= KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);

	set_mode_setfile(imgsensor.sensor_mode);

	set_mirror_flip(imgsensor.mirror);

	return ERROR_NONE;
}

static kal_uint32 slim_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	pr_debug("slim_video E\n");

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode			= IMGSENSOR_MODE_SLIM_VIDEO;
	imgsensor.pclk				= imgsensor_info.slim_video.pclk;
	imgsensor.line_length			= imgsensor_info.slim_video.linelength;
	imgsensor.frame_length			= imgsensor_info.slim_video.framelength;
	imgsensor.min_frame_length		= imgsensor_info.slim_video.framelength;

	imgsensor.dummy_line 			= 0;
	imgsensor.dummy_pixel			= 0;
	imgsensor.autoflicker_en 		= KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);

	set_mode_setfile(imgsensor.sensor_mode);

	set_mirror_flip(imgsensor.mirror);

	return ERROR_NONE;
}

static kal_uint32 custom1(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
				MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	pr_debug("E cur fps: %d\n", imgsensor.current_fps);

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode 			= IMGSENSOR_MODE_CUSTOM1;
	imgsensor.pclk 				= imgsensor_info.custom1.pclk;
	imgsensor.line_length 			= imgsensor_info.custom1.linelength;
	imgsensor.frame_length 			= imgsensor_info.custom1.framelength;
	imgsensor.min_frame_length 		= imgsensor_info.custom1.framelength;
	imgsensor.autoflicker_en 		= KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);

	set_mode_setfile(imgsensor.sensor_mode);

	set_mirror_flip(IMAGE_NORMAL);


    return ERROR_NONE;
}

static kal_uint32 custom2(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
				MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	pr_debug("%s()-E, cur fps: %d\n", __func__, imgsensor.current_fps);
	if (use_companion_gc607) {
#if WRITE_COMPANION_CAL
		if (remosaic_cal_loading_completed == false) {
			// companion chip init
			table_companion_write_cmos_sensor(gc607_setfile_info[IMGSENSOR_MODE_INIT].setfile,
					gc607_setfile_info[IMGSENSOR_MODE_INIT].size);

			pr_info("remosaic cal loading\n", __func__);
			companion_cal_write();
			remosaic_cal_loading_completed = true;
		}
#endif
	}

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_CUSTOM2;
	imgsensor.pclk = imgsensor_info.custom2.pclk;
	imgsensor.line_length = imgsensor_info.custom2.linelength;
	imgsensor.frame_length = imgsensor_info.custom2.framelength;
	imgsensor.min_frame_length = imgsensor_info.custom2.framelength;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);

	if (use_companion_gc607) {
#ifdef WRITE_COMPANION_CAL
		set_awbgain(iGr_gain, iR_gain, iB_gain, 1);
		set_gain(iGain);
#endif
	}

	set_mode_setfile(imgsensor.sensor_mode);
	set_mirror_flip(IMAGE_NORMAL);

	return ERROR_NONE;
}

static kal_uint32 custom5(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
						MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	pr_debug("E cur fps: %d\n", imgsensor.current_fps);

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode 			= IMGSENSOR_MODE_CUSTOM5;
	imgsensor.pclk 				= imgsensor_info.custom5.pclk;
	imgsensor.line_length 			= imgsensor_info.custom5.linelength;
	imgsensor.frame_length 			= imgsensor_info.custom5.framelength;
	imgsensor.min_frame_length 		= imgsensor_info.custom5.framelength;
	imgsensor.autoflicker_en 		= KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);

	set_mode_setfile(imgsensor.sensor_mode);
	set_mirror_flip(imgsensor.mirror);

    return ERROR_NONE;
}

static kal_uint32 get_resolution(
	MSDK_SENSOR_RESOLUTION_INFO_STRUCT(*sensor_resolution))
{
	pr_debug("get_resolution E\n");
	sensor_resolution->SensorFullWidth 	= imgsensor_info.cap.grabwindow_width;
	sensor_resolution->SensorFullHeight 	= imgsensor_info.cap.grabwindow_height;

	sensor_resolution->SensorPreviewWidth 	= imgsensor_info.pre.grabwindow_width;
	sensor_resolution->SensorPreviewHeight 	= imgsensor_info.pre.grabwindow_height;

	sensor_resolution->SensorVideoWidth 	= imgsensor_info.normal_video.grabwindow_width;
	sensor_resolution->SensorVideoHeight 	= imgsensor_info.normal_video.grabwindow_height;


	sensor_resolution->SensorHighSpeedVideoWidth 	= imgsensor_info.hs_video.grabwindow_width;
	sensor_resolution->SensorHighSpeedVideoHeight 	= imgsensor_info.hs_video.grabwindow_height;

	sensor_resolution->SensorSlimVideoWidth 	= imgsensor_info.slim_video.grabwindow_width;
	sensor_resolution->SensorSlimVideoHeight 	= imgsensor_info.slim_video.grabwindow_height;

	sensor_resolution->SensorCustom1Width  = imgsensor_info.custom1.grabwindow_width;
	sensor_resolution->SensorCustom1Height = imgsensor_info.custom1.grabwindow_height;

	sensor_resolution->SensorCustom2Width  =
		imgsensor_info.custom2.grabwindow_width;
	sensor_resolution->SensorCustom2Height =
		imgsensor_info.custom2.grabwindow_height;

	sensor_resolution->SensorCustom5Width  = imgsensor_info.custom5.grabwindow_width;
	sensor_resolution->SensorCustom5Height = imgsensor_info.custom5.grabwindow_height;

	return ERROR_NONE;
}

static kal_uint32 get_info(enum MSDK_SCENARIO_ID_ENUM scenario_id,
			   MSDK_SENSOR_INFO_STRUCT *sensor_info,
			   MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	pr_debug("get_info -> scenario_id = %d\n", scenario_id);

	sensor_info->SensorClockPolarity 		= SENSOR_CLOCK_POLARITY_LOW;
	sensor_info->SensorClockFallingPolarity = SENSOR_CLOCK_POLARITY_LOW;

	sensor_info->SensorHsyncPolarity = SENSOR_CLOCK_POLARITY_LOW;
	LOG_INF("get_info():sensor_info->SensorHsyncPolarity = %d\n", sensor_info->SensorHsyncPolarity);
	sensor_info->SensorVsyncPolarity = SENSOR_CLOCK_POLARITY_LOW;
	LOG_INF("get_info():sensor_info->SensorVsyncPolarity = %d\n", sensor_info->SensorVsyncPolarity);

	sensor_info->SensorInterruptDelayLines = 4;
	sensor_info->SensorResetActiveHigh = FALSE;
	sensor_info->SensorResetDelayCount = 5;

	sensor_info->SensroInterfaceType 	= imgsensor_info.sensor_interface_type;
	sensor_info->MIPIsensorType 		= imgsensor_info.mipi_sensor_type;
	sensor_info->SettleDelayMode 		= imgsensor_info.mipi_settle_delay_mode;

	if (use_companion_gc607)
		sensor_info->SensorOutputDataFormat = SENSOR_OUTPUT_FORMAT_RAW_4CELL_HW_BAYER_Gr;
	else
		sensor_info->SensorOutputDataFormat = imgsensor_info.sensor_output_dataformat;

	sensor_info->CaptureDelayFrame 	= imgsensor_info.cap_delay_frame;
	sensor_info->PreviewDelayFrame 	= imgsensor_info.pre_delay_frame;
	sensor_info->VideoDelayFrame 	= imgsensor_info.video_delay_frame;

	sensor_info->HighSpeedVideoDelayFrame	= imgsensor_info.hs_video_delay_frame;

	sensor_info->SlimVideoDelayFrame 	= imgsensor_info.slim_video_delay_frame;

	sensor_info->Custom1DelayFrame 		= imgsensor_info.custom1_delay_frame;
	sensor_info->Custom2DelayFrame = imgsensor_info.custom2_delay_frame;
	sensor_info->Custom5DelayFrame 		= imgsensor_info.custom5_delay_frame;
	sensor_info->SensorMasterClockSwitch = 0;
	sensor_info->SensorDrivingCurrent 	= imgsensor_info.isp_driving_current;

	sensor_info->AEShutDelayFrame 		= imgsensor_info.ae_shut_delay_frame;

	sensor_info->AESensorGainDelayFrame 	= imgsensor_info.ae_sensor_gain_delay_frame;

	sensor_info->AEISPGainDelayFrame 	= imgsensor_info.ae_ispGain_delay_frame;

	sensor_info->FrameTimeDelayFrame 	= imgsensor_info.frame_time_delay_frame;

	sensor_info->IHDR_Support 		= imgsensor_info.ihdr_support;
	sensor_info->IHDR_LE_FirstLine 		= imgsensor_info.ihdr_le_firstline;
	sensor_info->SensorModeNum 			= imgsensor_info.sensor_mode_num;
	sensor_info->SensorMIPILaneNumber 	= imgsensor_info.mipi_lane_num;
	sensor_info->SensorClockFreq 		= imgsensor_info.mclk;
	sensor_info->SensorClockDividCount = 3;
	sensor_info->SensorClockRisingCount 	= 0;
	sensor_info->SensorClockFallingCount	= 2;
	sensor_info->SensorPixelClockCount		= 3;
	sensor_info->SensorDataLatchCount		= 2;
	sensor_info->TEMPERATURE_SUPPORT = imgsensor_info.temperature_support;
	sensor_info->MIPIDataLowPwr2HighSpeedTermDelayCount 	= 0;
	sensor_info->MIPICLKLowPwr2HighSpeedTermDelayCount 		= 0;
	sensor_info->SensorWidthSampling	= 0;
	sensor_info->SensorHightSampling	= 0;
	sensor_info->SensorPacketECCOrder 	= 1;

#ifdef	DISABLE_PDAF_OUT
	sensor_info->PDAF_Support = PDAF_SUPPORT_NA;
#else
	sensor_info->PDAF_Support = PDAF_SUPPORT_CAMSV;
#endif

	switch (scenario_id) {
	case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
	LOG_INF("get_info():MSDK_SCENARIO_ID_CAMERA_PREVIEW scenario_id= %d\n", scenario_id);
		sensor_info->SensorGrabStartX = imgsensor_info.pre.startx;
		sensor_info->SensorGrabStartY = imgsensor_info.pre.starty;

		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
			imgsensor_info.pre.mipi_data_lp2hs_settle_dc;

		break;
	case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
	LOG_INF("get_info():MSDK_SCENARIO_ID_CAMERA_PREVIEW scenario_id= %d\n", scenario_id);
		sensor_info->SensorGrabStartX = imgsensor_info.cap.startx;
		sensor_info->SensorGrabStartY = imgsensor_info.cap.starty;

		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
			imgsensor_info.cap.mipi_data_lp2hs_settle_dc;

		break;
	case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
	LOG_INF("get_info():MSDK_SCENARIO_ID_CAMERA_PREVIEW scenario_id= %d\n", scenario_id);
		sensor_info->SensorGrabStartX = imgsensor_info.normal_video.startx;
		sensor_info->SensorGrabStartY = imgsensor_info.normal_video.starty;

		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
			imgsensor_info.normal_video.mipi_data_lp2hs_settle_dc;

		break;
	case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
	LOG_INF("get_info():MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO scenario_id= %d\n", scenario_id);
		sensor_info->SensorGrabStartX = imgsensor_info.hs_video.startx;
		sensor_info->SensorGrabStartY = imgsensor_info.hs_video.starty;

		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
			imgsensor_info.hs_video.mipi_data_lp2hs_settle_dc;

		break;
	case MSDK_SCENARIO_ID_SLIM_VIDEO:
	LOG_INF("get_info():MSDK_SCENARIO_ID_SLIM_VIDEO scenario_id= %d\n", scenario_id);
		sensor_info->SensorGrabStartX = imgsensor_info.slim_video.startx;
		sensor_info->SensorGrabStartY = imgsensor_info.slim_video.starty;

		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
			imgsensor_info.slim_video.mipi_data_lp2hs_settle_dc;

		break;

	case MSDK_SCENARIO_ID_CUSTOM1:
	LOG_INF("get_info():MSDK_SCENARIO_ID_CUSTOM1 scenario_id= %d\n", scenario_id);
		sensor_info->SensorGrabStartX = imgsensor_info.custom1.startx;
		sensor_info->SensorGrabStartY = imgsensor_info.custom1.starty;
		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
			imgsensor_info.custom1.mipi_data_lp2hs_settle_dc;

		break;
	case MSDK_SCENARIO_ID_CUSTOM2:
	LOG_INF("MSDK_SCENARIO_ID_CUSTOM2 scenario_id= %d\n",
		scenario_id);
		sensor_info->SensorGrabStartX = imgsensor_info.custom2.startx;
		sensor_info->SensorGrabStartY = imgsensor_info.custom2.starty;
		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
			imgsensor_info.custom2.mipi_data_lp2hs_settle_dc;

		break;
	case MSDK_SCENARIO_ID_CUSTOM5:
	LOG_INF("MSDK_SCENARIO_ID_CUSTOM5 scenario_id= %d\n",
		scenario_id);
		sensor_info->SensorGrabStartX = imgsensor_info.custom5.startx;
		sensor_info->SensorGrabStartY = imgsensor_info.custom5.starty;
		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
			imgsensor_info.custom5.mipi_data_lp2hs_settle_dc;

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

static kal_uint32 set_video_mode(UINT16 framerate)
{

	LOG_INF("set_video_mode():framerate = %d\n", framerate);

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
	pr_debug("enable = %d, framerate = %d\n", enable, framerate);

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

	pr_debug("scenario_id = %d, framerate = %d\n", scenario_id, framerate);

	switch (scenario_id) {
	case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
	LOG_INF("set_max_framerate_by_scenario():MSDK_SCENARIO_ID_CAMERA_PREVIEW scenario_id= %d\n", scenario_id);
		frame_length = imgsensor_info.pre.pclk /
		framerate * 10 / imgsensor_info.pre.linelength;

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
	LOG_INF("set_max_framerate_by_scenario():MSDK_SCENARIO_ID_VIDEO_PREVIEW scenario_id= %d\n", scenario_id);
		if (framerate == 0)
			return ERROR_NONE;
		frame_length = imgsensor_info.normal_video.pclk
		    / framerate * 10 / imgsensor_info.normal_video.linelength;

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
		LOG_INF("set_max_framerate_by_scenario():MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG scenario_id= %d\n", scenario_id);

		frame_length = imgsensor_info.cap.pclk /
		framerate * 10 / imgsensor_info.cap.linelength;
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
	LOG_INF("set_max_framerate_by_scenario():MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO scenario_id= %d\n", scenario_id);
		frame_length = imgsensor_info.hs_video.pclk /
		framerate * 10 / imgsensor_info.hs_video.linelength;

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
		LOG_INF("set_max_framerate_by_scenario():MSDK_SCENARIO_ID_SLIM_VIDEO scenario_id= %d\n", scenario_id);

		frame_length = imgsensor_info.slim_video.pclk /
		framerate * 10 / imgsensor_info.slim_video.linelength;

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
		LOG_INF("set_max_framerate_by_scenario():MSDK_SCENARIO_ID_CUSTOM1 scenario_id= %d\n", scenario_id);
		frame_length = imgsensor_info.custom1.pclk / framerate * 10
		/ imgsensor_info.custom1.linelength;

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
		LOG_INF(
		"MSDK_SCENARIO_ID_CUSTOM2 scenario_id= %d\n",
			scenario_id);
		frame_length = imgsensor_info.custom2.pclk
			/ framerate * 10 / imgsensor_info.custom2.linelength;

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
	case MSDK_SCENARIO_ID_CUSTOM5:
		LOG_INF("set_max_framerate_by_scenario():MSDK_SCENARIO_ID_CUSTOM5 scenario_id= %d\n", scenario_id);
		frame_length = imgsensor_info.custom5.pclk / framerate * 10
		/ imgsensor_info.custom5.linelength;

		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line = (frame_length > imgsensor_info.custom5.framelength)
			? (frame_length - imgsensor_info.custom5.framelength) : 0;

		if (imgsensor.dummy_line < 0)
			imgsensor.dummy_line = 0;

		imgsensor.frame_length = imgsensor_info.custom5.framelength + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);

		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
		break;
	default:
	frame_length =
	imgsensor_info.pre.pclk /
	framerate * 10 / imgsensor_info.pre.linelength;

		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line = (frame_length > imgsensor_info.pre.framelength)
			? (frame_length - imgsensor_info.pre.framelength) : 0;

		imgsensor.frame_length = imgsensor_info.pre.framelength + imgsensor.dummy_line;

		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);

		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();

		pr_debug("error scenario_id = %d, we use preview scenario\n", scenario_id);
		break;
	}
	return ERROR_NONE;
}


static kal_uint32 get_default_framerate_by_scenario(
	enum MSDK_SCENARIO_ID_ENUM scenario_id, MUINT32 *framerate)
{
	pr_debug("scenario_id = %d\n", scenario_id);

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
		pr_debug("custom1 *framerate = %d\n", *framerate);
		break;
	case MSDK_SCENARIO_ID_CUSTOM2:
		*framerate = imgsensor_info.custom2.max_framerate;
		pr_debug("custom2 *framerate = %d\n", *framerate);
		break;
	case MSDK_SCENARIO_ID_CUSTOM5:
		*framerate = imgsensor_info.custom5.max_framerate;
		pr_debug("custom5 *framerate = %d\n", *framerate);
		break;
	default:
		break;
	}

	return ERROR_NONE;
}

static kal_uint32 get_sensor_temperature(void)
{
	UINT8 temperature;
	INT32 temperature_convert;

	temperature = read_cmos_sensor_8(0x000A);

	if (temperature >= 0x0 && temperature <= 0x78)
		temperature_convert = temperature;
	else
		temperature_convert = -1;

	LOG_INF("temp_c(%d), read_reg(%d), enable %d\n", temperature_convert, temperature, read_cmos_sensor_8(0x0138));

	return temperature_convert;
}


static kal_uint32 control(enum MSDK_SCENARIO_ID_ENUM scenario_id,
			  MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	pr_info("scenario_id = %d\n", scenario_id);

	spin_lock(&imgsensor_drv_lock);
	imgsensor.current_scenario_id = scenario_id;
	spin_unlock(&imgsensor_drv_lock);

	switch (scenario_id) {
	case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		LOG_INF("%s() MSDK_SCENARIO_ID_CAMERA_PREVIEW scenario_id= %d\n", __func__, scenario_id);
		preview(image_window, sensor_config_data);
		break;
	case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		LOG_INF("%s() MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG scenario_id= %d\n", __func__, scenario_id);
		capture(image_window, sensor_config_data);
		break;
	case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
		LOG_INF("%s() MSDK_SCENARIO_ID_VIDEO_PREVIEW scenario_id= %d\n", __func__, scenario_id);
		normal_video(image_window, sensor_config_data);
		break;
	case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
		LOG_INF("%s() :MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO scenario_id= %d\n", __func__, scenario_id);
		hs_video(image_window, sensor_config_data);
		break;
	case MSDK_SCENARIO_ID_SLIM_VIDEO:
		LOG_INF("%s() MSDK_SCENARIO_ID_SLIM_VIDEO scenario_id= %d\n", __func__, scenario_id);
		slim_video(image_window, sensor_config_data);
		break;

	case MSDK_SCENARIO_ID_CUSTOM1:
		LOG_INF("%s() MSDK_SCENARIO_ID_CUSTOM1 scenario_id= %d\n", __func__, scenario_id);
		custom1(image_window, sensor_config_data);
		break;
	case MSDK_SCENARIO_ID_CUSTOM2:
		LOG_INF("%s() MSDK_SCENARIO_ID_CUSTOM2 scenario_id= %d\n", __func__, scenario_id);
		custom2(image_window, sensor_config_data);
		break;
	case MSDK_SCENARIO_ID_CUSTOM5:
		LOG_INF("%s() MSDK_SCENARIO_ID_CUSTOM5 scenario_id= %d\n", __func__, scenario_id);
		custom5(image_window, sensor_config_data);
		break;
	default:
		pr_debug("Error ScenarioId setting");
		preview(image_window, sensor_config_data);
		return ERROR_INVALID_SCENARIO_ID;
	}
	return ERROR_NONE;
}


static kal_uint32 feature_control(MSDK_SENSOR_FEATURE_ENUM feature_id,
				  UINT8 *feature_para, UINT32 *feature_para_len)
{
	UINT16 *feature_return_para_16 	= (UINT16 *) feature_para;
	UINT16 *feature_data_16 		= (UINT16 *) feature_para;
	UINT32 *feature_return_para_32 	= (UINT32 *) feature_para;
	UINT32 *feature_data_32 		= (UINT32 *) feature_para;
	INT32 *feature_return_para_i32 	= (INT32 *) feature_para;

	unsigned long long *feature_data = (unsigned long long *)feature_para;
	struct SENSOR_WINSIZE_INFO_STRUCT *wininfo;
	struct SET_PD_BLOCK_INFO_T *PDAFinfo;
	struct SENSOR_VC_INFO_STRUCT *pvcinfo;

	MSDK_SENSOR_REG_INFO_STRUCT *sensor_reg_data =
		(MSDK_SENSOR_REG_INFO_STRUCT *) feature_para;

	pr_debug("feature_id = %d\n", feature_id);

	switch (feature_id) {
	case SENSOR_FEATURE_GET_PERIOD:
		LOG_INF("SENSOR_FEATURE_GET_PERIOD = %d\n", feature_id);
		*feature_return_para_16++ 	= imgsensor.line_length;
		*feature_return_para_16 	= imgsensor.frame_length;
		*feature_para_len 			= 4;
		break;
	case SENSOR_FEATURE_GET_PIXEL_CLOCK_FREQ:
		LOG_INF("SENSOR_FEATURE_GET_PIXEL_CLOCK_FREQ = %d\n", feature_id);
		*feature_return_para_32 = imgsensor.pclk;
		*feature_para_len 		= 4;
		break;
	case SENSOR_FEATURE_SET_ESHUTTER:
		pr_debug("SENSOR_FEATURE_SET_ESHUTTER = %d\n", feature_id);
		set_shutter(*feature_data);
		break;
	case SENSOR_FEATURE_SET_NIGHTMODE:
		LOG_INF("SENSOR_FEATURE_SET_NIGHTMODE = %d, data=%d\n", feature_id, *feature_data);
		if (*feature_data)
			sIsNightHyperlapse = KAL_TRUE;
		else
			sIsNightHyperlapse = KAL_FALSE;
		break;
	case SENSOR_FEATURE_SET_GAIN:
		pr_debug("SENSOR_FEATURE_SET_GAIN = %d\n", feature_id);
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
		LOG_INF("SENSOR_FEATURE_SET_AUTO_FLICKER_MODE = %d\n", feature_id);
		set_auto_flicker_mode((BOOL) (*feature_data_16), *(feature_data_16 + 1));
		if (use_companion_gc607) {
#ifdef ENABLE_GC607_DEBUG_INFO
			// only checking when sensor stream on
			if (!check_done)
				check_done = check_flow_of_gc607();
#endif
		}
		break;
	case SENSOR_FEATURE_SET_MAX_FRAME_RATE_BY_SCENARIO:
		LOG_INF("SENSOR_FEATURE_SET_MAX_FRAME_RATE_BY_SCENARIO = %d\n", feature_id);
		set_max_framerate_by_scenario(
	    (enum MSDK_SCENARIO_ID_ENUM) *feature_data, *(feature_data + 1));
		break;

	case SENSOR_FEATURE_GET_DEFAULT_FRAME_RATE_BY_SCENARIO:
		LOG_INF("SENSOR_FEATURE_GET_DEFAULT_FRAME_RATE_BY_SCENARIO = %d\n", feature_id);
		get_default_framerate_by_scenario(
			(enum MSDK_SCENARIO_ID_ENUM) *(feature_data),
			  (MUINT32 *) (uintptr_t) (*(feature_data + 1)));
		break;
	case SENSOR_FEATURE_SET_TEST_PATTERN:
		LOG_INF("SENSOR_FEATURE_SET_TEST_PATTERN not implemented yet = %d\n", feature_id);
		break;

	case SENSOR_FEATURE_GET_TEST_PATTERN_CHECKSUM_VALUE:
		LOG_INF("SENSOR_FEATURE_GET_TEST_PATTERN_CHECKSUM_VALUE = %d\n", feature_id);
		*feature_return_para_32 = imgsensor_info.checksum_value;
		*feature_para_len 		= 4;
		break;
	case SENSOR_FEATURE_SET_FRAMERATE:
		LOG_INF("SENSOR_FEATURE_SET_FRAMERATE = %d\n", feature_id);
		pr_debug("current fps :%d\n", *feature_data_32);
		spin_lock(&imgsensor_drv_lock);
		imgsensor.current_fps = (UINT16)*feature_data_32;
		spin_unlock(&imgsensor_drv_lock);
		break;
	case SENSOR_FEATURE_SET_HDR:
		LOG_INF("SENSOR_FEATURE_SET_HDR = %d\n", feature_id);
		pr_debug("ihdr enable :%d\n", *feature_data_32);
		spin_lock(&imgsensor_drv_lock);
		imgsensor.ihdr_mode = (UINT8)*feature_data_32;
		spin_unlock(&imgsensor_drv_lock);
		break;

	case SENSOR_FEATURE_GET_CROP_INFO:
		LOG_INF("SENSOR_FEATURE_GET_CROP_INFO = %d\n", feature_id);
		pr_debug("SENSOR_FEATURE_GET_CROP_INFO scenarioId:%d\n", (UINT32) *feature_data);

		wininfo = (struct SENSOR_WINSIZE_INFO_STRUCT *) (uintptr_t) (*(feature_data + 1));

		switch (*feature_data_32) {
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		LOG_INF("MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG = %d\n", *feature_data_32);
			memcpy((void *)wininfo, (void *)&imgsensor_winsize_info[1],
			       sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
		LOG_INF("MSDK_SCENARIO_ID_VIDEO_PREVIEW = %d\n", *feature_data_32);
			memcpy((void *)wininfo, (void *)&imgsensor_winsize_info[2],
			       sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
		LOG_INF("MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO = %d\n", *feature_data_32);
			memcpy((void *)wininfo, (void *)&imgsensor_winsize_info[3],
			       sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
		LOG_INF("MSDK_SCENARIO_ID_SLIM_VIDEO = %d\n", *feature_data_32);
			memcpy((void *)wininfo, (void *)&imgsensor_winsize_info[4],
			       sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;

		case MSDK_SCENARIO_ID_CUSTOM1:
		LOG_INF("MSDK_SCENARIO_ID_CUSTOM1 = %d\n", feature_id);
			memcpy((void *)wininfo, (void *)&imgsensor_winsize_info[5],
					sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_CUSTOM2:
		LOG_INF("MSDK_SCENARIO_ID_CUSTOM2 = %d\n", feature_id);
			memcpy((void *)wininfo,
			(void *)&imgsensor_winsize_info[6],
			sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_CUSTOM5:
		LOG_INF("MSDK_SCENARIO_ID_CUSTOM5 = %d\n", feature_id);
			memcpy((void *)wininfo, (void *)&imgsensor_winsize_info[9],
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
		LOG_INF(
		"SENSOR_FEATURE_GET_PDAF_INFO scenarioId:%d\n",
		(UINT16)*feature_data);
		PDAFinfo =
		(struct SET_PD_BLOCK_INFO_T *)(uintptr_t)(*(feature_data+1));
			switch (*feature_data) {
			case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			case MSDK_SCENARIO_ID_CUSTOM1:
			case MSDK_SCENARIO_ID_CUSTOM2:
			case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
			case MSDK_SCENARIO_ID_SLIM_VIDEO:
			case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
				pr_info(
				"MSDK_SCENARIO_ID_CAMERA_PREVIEW:%d\n",
				MSDK_SCENARIO_ID_CAMERA_PREVIEW);
				imgsensor_pd_info.i4BlockNumX = 578;
				imgsensor_pd_info.i4BlockNumY = 432;
				memcpy((void *)PDAFinfo,
				(void *)&imgsensor_pd_info,
				sizeof(struct SET_PD_BLOCK_INFO_T));
				break;
			case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
				pr_info(
				"MSDK_SCENARIO_ID_CAMERA_PREVIEW:%d\n",
				MSDK_SCENARIO_ID_CAMERA_PREVIEW);
				imgsensor_pd_info.i4BlockNumX = 578;
				imgsensor_pd_info.i4BlockNumY = 324;
				memcpy((void *)PDAFinfo,
				(void *)&imgsensor_pd_info,
				sizeof(struct SET_PD_BLOCK_INFO_T));
				break;
			case MSDK_SCENARIO_ID_CUSTOM5:
				pr_info("MSDK_SCENARIO_ID_CUSTOM5:%d\n", MSDK_SCENARIO_ID_CUSTOM5);
				imgsensor_pd_info.i4BlockNumX = 500;
				imgsensor_pd_info.i4BlockNumY = 374;
				memcpy((void *)PDAFinfo,
				(void *)&imgsensor_pd_info,
				sizeof(struct SET_PD_BLOCK_INFO_T));
				break;
			default:
				break;
			}
		break;
	case SENSOR_FEATURE_GET_SENSOR_PDAF_CAPACITY:
		LOG_INF(
		"SENSOR_FEATURE_GET_SENSOR_PDAF_CAPACITY scenarioId:%d\n",
			(UINT16) *feature_data);
		/*PDAF capacity enable or not, 2p8 only full size support PDAF*/
		switch (*feature_data) {
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			*(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 1;
			break;
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			/* video & capture use same setting */
			*(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 1;
			break;
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
			*(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 0;
			break;
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
			*(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 0;
			break;
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
			*(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 1;
			break;
		case MSDK_SCENARIO_ID_CUSTOM1:
			*(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 0;
			break;
		case MSDK_SCENARIO_ID_CUSTOM2:
			*(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 0;
			break;
		case MSDK_SCENARIO_ID_CUSTOM5:
			*(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 1;
			break;
		default:
			*(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 0;
			break;
		}
		break;
	case SENSOR_FEATURE_GET_VC_INFO:
		pr_debug("SENSOR_FEATURE_GET_VC_INFO %d\n",
			(UINT16) *feature_data);

		pvcinfo =
	    (struct SENSOR_VC_INFO_STRUCT *) (uintptr_t) (*(feature_data + 1));

		if (use_companion_gc607) {
			switch (*feature_data_32) {
			case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
				memcpy((void *)pvcinfo, (void *)&SENSOR_COMPANION_VC_INFO[1],
						sizeof(struct SENSOR_VC_INFO_STRUCT));
				break;
			case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
				memcpy((void *)pvcinfo, (void *)&SENSOR_COMPANION_VC_INFO[2],
						sizeof(struct SENSOR_VC_INFO_STRUCT));
				break;
			case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
				memcpy((void *)pvcinfo, (void *)&SENSOR_COMPANION_VC_INFO[3],
						sizeof(struct SENSOR_VC_INFO_STRUCT));
				break;
			case MSDK_SCENARIO_ID_SLIM_VIDEO:
				memcpy((void *)pvcinfo, (void *)&SENSOR_COMPANION_VC_INFO[4],
						sizeof(struct SENSOR_VC_INFO_STRUCT));
				break;
			case MSDK_SCENARIO_ID_CUSTOM5:
				memcpy((void *)pvcinfo, (void *)&SENSOR_COMPANION_VC_INFO[5],
						sizeof(struct SENSOR_VC_INFO_STRUCT));
				break;
			case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
				memcpy((void *)pvcinfo, (void *)&SENSOR_COMPANION_VC_INFO[0],
						sizeof(struct SENSOR_VC_INFO_STRUCT));
				break;
			default:
				break;
			}
		} else {
			switch (*feature_data_32) {
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
			case MSDK_SCENARIO_ID_SLIM_VIDEO:
				memcpy((void *)pvcinfo, (void *)&SENSOR_VC_INFO[4],
						sizeof(struct SENSOR_VC_INFO_STRUCT));
				break;
			case MSDK_SCENARIO_ID_CUSTOM5:
				memcpy((void *)pvcinfo, (void *)&SENSOR_VC_INFO[5],
					sizeof(struct SENSOR_VC_INFO_STRUCT));
				break;
			case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
				memcpy((void *)pvcinfo, (void *)&SENSOR_VC_INFO[0],
						sizeof(struct SENSOR_VC_INFO_STRUCT));
				break;
			default:
				break;
			}
		}
		break;
	case SENSOR_FEATURE_SET_IHDR_SHUTTER_GAIN:
		LOG_INF("SENSOR_FEATURE_SET_IHDR_SHUTTER_GAIN = %d\n", feature_id);
		pr_debug("SENSOR_SET_SENSOR_IHDR LE=%d, SE=%d, Gain=%d\n",
			(UINT16) *feature_data,
			(UINT16) *(feature_data + 1),
			(UINT16) *(feature_data + 2));

/* ihdr_write_shutter_gain(	(UINT16)*feature_data,
 *				(UINT16)*(feature_data+1),
 * 				(UINT16)*(feature_data+2));
 */
		break;
	case SENSOR_FEATURE_SET_AWB_GAIN:
		pr_debug("SENSOR_FEATURE_SET_AWB_GAIN = %d\n", feature_id);
		set_awbgain((UINT32)(*feature_data_32),
					(UINT32)*(feature_data_32 + 1),
					(UINT32)*(feature_data_32 + 2),
					0);
		break;
	case SENSOR_FEATURE_SET_HDR_SHUTTER:
		LOG_INF("SENSOR_FEATURE_SET_HDR_SHUTTER = %d\n", feature_id);
		pr_debug("SENSOR_FEATURE_SET_HDR_SHUTTER LE=%d, SE=%d\n",
			(UINT32) *feature_data, (UINT32) *(feature_data + 1));
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
		LOG_INF("SENSOR_FEATURE_GET_TEMPERATURE_VALUE = %d\n", feature_id);
		*feature_return_para_i32 	= get_sensor_temperature();
		*feature_para_len 			= 4;
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
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
			(imgsensor_info.custom2.pclk /
			(imgsensor_info.custom2.linelength - 80)) *
			imgsensor_info.custom2.grabwindow_width;
			break;
		case MSDK_SCENARIO_ID_CUSTOM5:
		LOG_INF("MSDK_SCENARIO_ID_CUSTOM5 = %d\n", *feature_data);
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) = (imgsensor_info.custom5.pclk /
			(imgsensor_info.custom5.linelength - 80)) * imgsensor_info.custom5.grabwindow_width;
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
		LOG_INF("SENSOR_FEATURE_GET_MIPI_PIXEL_RATE = %d\n", feature_id);
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
		case MSDK_SCENARIO_ID_CUSTOM5:
		LOG_INF("MSDK_SCENARIO_ID_CUSTOM5 = %d\n", *feature_data);
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) = imgsensor_info.custom5.mipi_pixel_rate;
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
			*feature_return_para_32 = 1;
			break;
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		case MSDK_SCENARIO_ID_CUSTOM1:
		case MSDK_SCENARIO_ID_CUSTOM5:
		default:
			*feature_return_para_32 = 2;
			break;
		}

		pr_debug(
		"SENSOR_FEATURE_GET_BINNING_TYPE AE_binning_type:%d,\n",
		*feature_return_para_32);
		*feature_para_len = 4;

	    break;
	case SENSOR_FEATURE_GET_AWB_REQ_BY_SCENARIO:
		if (use_companion_gc607) {
#ifdef WRITE_COMPANION_CAL
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) = 1;
#else
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) = 0;
#endif
		} else {
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) = 0;
		}
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

UINT32 S5KGW3MIPI_RAW_SensorInit(struct SENSOR_FUNCTION_STRUCT **pfFunc)
{
	/* To Do : Check Sensor status here */
	if (pfFunc != NULL)
		*pfFunc = &sensor_func;
	return ERROR_NONE;
}

