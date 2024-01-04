// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 Samsung Electronics Co., Ltd
 */

#include <linux/videodev2.h>
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

#include "sc501csmipiraw_Sensor.h"
#include "sc501csmipiraw_setfile.h"

/************************** Modify Following Strings for Debug **************************/
#define PFX "SC501CS D/D"
#define LOG_1 LOG_INF("SC501CSMIPI, 2LANE\n")
/****************************   Modify end	*******************************************/

#define LOG_DBG(format, args...) pr_debug(PFX "[%s] " format, __func__, ##args)
#define LOG_INF(format, args...) pr_info(PFX "[%s] " format, __func__, ##args)
#define LOG_ERR(format, args...) pr_err(PFX "[%s] " format, __func__, ##args)

#ifndef VENDOR_EDIT
#define VENDOR_EDIT
#endif

static DEFINE_SPINLOCK(imgsensor_drv_lock);

#define TRANSFER_UNIT (3)
#define MULTI_WRITE 1
#if MULTI_WRITE
#define I2C_BUFFER_LEN (TRANSFER_UNIT * 255) /* trans# max is 255, (TRANSFER_UNIT) bytes */
#else
#define I2C_BUFFER_LEN (TRANSFER_UNIT)
#endif

static kal_uint32 Dgain_ratio = 256;

/*
 * Image Sensor Scenario
 * pre : 4:3
 * cap : Check Active Array
 * normal_video : 16:9
 * hs_video: NOT USED
 * slim_video: NOT USED
 * custom1 : FAST AE
 * custom2 : NOT USED
 * custom3 : NOT USED
 * custom4 : NOT USED
 * custom5 : NOT USED
 *
 */

static struct imgsensor_info_struct imgsensor_info = {
	.sensor_id = SC501CS_SENSOR_ID,
	.checksum_value = 0xcde448ca,

	.pre = {
		.pclk = 89700000,
		.linelength = 1495,
		.framelength = 2000,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 2576,
		.grabwindow_height = 1932,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 179400000,
		.max_framerate = 300,
	},
	.cap = {
		.pclk = 89700000,
		.linelength = 1495,
		.framelength = 2000,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 2576,
		.grabwindow_height = 1932,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 179400000,
		.max_framerate = 300,
	},
	.normal_video = {
		.pclk = 89700000,
		.linelength = 1495,
		.framelength = 2000,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 2560,
		.grabwindow_height = 1440,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 179400000,
		.max_framerate = 300,
	},
	.hs_video = { // NOT USED
		.pclk = 89700000,
		.linelength = 1495,
		.framelength = 2000,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 2576,
		.grabwindow_height = 1932,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 179400000,
		.max_framerate = 300,
	},
	.slim_video = { // NOT USED
		.pclk = 89700000,
		.linelength = 1495,
		.framelength = 2000,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 2576,
		.grabwindow_height = 1932,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 179400000,
		.max_framerate = 300,
	},
	.custom1 = {
		.pclk = 89700000,
		.linelength = 1457,
		.framelength = 1026,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 1280,
		.grabwindow_height = 960,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 89700000,
		.max_framerate = 600,
	},
	.custom2 = { // NOT USED
		.pclk = 89700000,
		.linelength = 1495,
		.framelength = 2000,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 2576,
		.grabwindow_height = 1932,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 179400000,
		.max_framerate = 300,
	},
	.custom3 = { // NOT USED
		.pclk = 89700000,
		.linelength = 1495,
		.framelength = 2000,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 2576,
		.grabwindow_height = 1932,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 179400000,
		.max_framerate = 300,
	},
	.custom4 = { // NOT USED
		.pclk = 89700000,
		.linelength = 1495,
		.framelength = 2000,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 2576,
		.grabwindow_height = 1932,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 179400000,
		.max_framerate = 300,
	},
	.custom5 = { // NOT USED
		.pclk = 89700000,
		.linelength = 1495,
		.framelength = 2000,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 2576,
		.grabwindow_height = 1932,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 179400000,
		.max_framerate = 300,
	},

	// set sensor_mode
	.mode_info[IMGSENSOR_MODE_PREVIEW]          = &imgsensor_info.pre,
	.mode_info[IMGSENSOR_MODE_CAPTURE]          = &imgsensor_info.cap,
	.mode_info[IMGSENSOR_MODE_VIDEO]            = &imgsensor_info.normal_video,
	.mode_info[IMGSENSOR_MODE_HIGH_SPEED_VIDEO] = &imgsensor_info.hs_video,
	.mode_info[IMGSENSOR_MODE_SLIM_VIDEO]       = &imgsensor_info.slim_video,
	.mode_info[IMGSENSOR_MODE_CUSTOM1]          = &imgsensor_info.custom1,
	.mode_info[IMGSENSOR_MODE_CUSTOM2]          = &imgsensor_info.custom2,
	.mode_info[IMGSENSOR_MODE_CUSTOM3]          = &imgsensor_info.custom3,
	.mode_info[IMGSENSOR_MODE_CUSTOM4]          = &imgsensor_info.custom4,
	.mode_info[IMGSENSOR_MODE_CUSTOM5]          = &imgsensor_info.custom5,

	.is_invalid_mode = false,

	.margin = 16,
	/*sensor framelength & shutter margin*/
	.max_frame_length = 0x7FFFF,
	/*max framelength by sensor register's limitation*/	 /*5fps*/
	.ae_shut_delay_frame = 0,
	/*shutter delay frame for AE cycle, 2 frame with ispGain_delay-shut_delay=2-0=2*/
	.ae_sensor_gain_delay_frame = 0,
	/*sensor gain delay frame for AE cycle, 2 frame with ispGain_delay-sensor_gain_delay=2-0=2*/
	.ae_ispGain_delay_frame = 2,	/*isp gain delay frame for AE cycle*/
	.ihdr_support = 0,				/*1 support; 0 not support*/
	.temperature_support = 0,		/* 1, support; 0,not support */
	.ihdr_le_firstline = 0,			/*1 le first ; 0, se first*/
	.sensor_mode_num = 10,			/*support sensor mode num*/
	.min_shutter = 4,
	.min_gain = 64,
	.max_gain = 1024,
	.min_gain_iso = 50,
	.exp_step = 2,
	.gain_step = 1,
	.gain_type = 4,
	.cap_delay_frame = 2,			/*enter capture delay frame num*/
	.pre_delay_frame = 2,			/*enter preview delay frame num*/
	.video_delay_frame = 2,			/*enter video delay frame num*/
	.hs_video_delay_frame = 2,		/*enter high speed video  delay frame num*/
	.slim_video_delay_frame = 2,	/*enter slim video delay frame num*/
	.custom1_delay_frame = 2,		/*enter custom1 delay frame num*/
	.custom2_delay_frame = 2,		/*enter custom2 delay frame num*/
	.custom3_delay_frame = 2,		/*enter custom3 delay frame num*/
	.custom4_delay_frame = 2,		/*enter custom4 delay frame num*/
	.custom5_delay_frame = 2,		/*enter custom5 delay frame num*/
	.frame_time_delay_frame = 2,	/*enter custom1 delay frame num*/

	.isp_driving_current = ISP_DRIVING_6MA,/*mclk driving current*/
	.sensor_interface_type = SENSOR_INTERFACE_TYPE_MIPI,	/*sensor_interface_type*/
	.mipi_sensor_type = MIPI_OPHY_NCSI2,/*0,MIPI_OPHY_NCSI2;  1,MIPI_OPHY_CSI2*/
	.mipi_settle_delay_mode = MIPI_SETTLEDELAY_AUTO,
	/*0,MIPI_SETTLEDELAY_AUTO; 1,MIPI_SETTLEDELAY_MANNUAL*/

	.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_B,
	/*sensor output first pixel color*/

	.mclk = 26,
	/*mclk value, suggest 24 or 26 for 24Mhz or 26Mhz*/
	.mipi_lane_num = SENSOR_MIPI_2_LANE,					/*mipi lane num*/
	.i2c_addr_table = {0x6c, 0x20, 0xff},
	.i2c_speed = 400,
	/*record sensor support all write id addr, only supprt 4must end with 0xff*/
};

static struct imgsensor_struct imgsensor = {
	.mirror = IMAGE_NORMAL,	   /*mirrorflip information*/
	.sensor_mode = IMGSENSOR_MODE_INIT,
	/* IMGSENSOR_MODE enum value, record current sensor mode,
	 * such as: INIT, Preview, Capture, Video,High Speed Video, Slim Video
	 */
	.shutter = 0x258,			  /*current shutter*/
	.gain = 0x40,				  /*current gain*/
	.dummy_pixel = 0,			  /*current dummypixel*/
	.dummy_line = 0,			  /*current dummyline*/
	/*full size current fps : 24fps for PIP, 30fps for Normal or ZSD*/
	.current_fps = 300,
	.autoflicker_en = KAL_FALSE,
	/*auto flicker enable: KAL_FALSE for disable*/
	/*auto flicker, KAL_TRUE for enable auto flicker*/
	.test_pattern = KAL_FALSE,
	/*test pattern mode or not KAL_FALSE for in test pattern mode, KAL_TRUE for normal output*/
	.current_scenario_id = MSDK_SCENARIO_ID_CAMERA_PREVIEW, /*current scenario id*/
	.ihdr_en = 0,				 /*sensor need support LE, SE with HDR feature*/
	.i2c_write_id = 0x6c,		 /*record current sensor's i2c write id*/
};

/* Sensor output window information */
static struct SENSOR_WINSIZE_INFO_STRUCT imgsensor_winsize_info[] = {
	{    0,    0,    0,    0,    0,    0,    0,    0,
	     0,    0,    0,    0,    0,    0,    0,    0}, /* init - NOT USED*/
	{ 2592, 1944,    0,    0, 2592, 1944, 2592, 1944,
	     8,    6, 2576, 1932,    0,    0, 2576, 1932 }, /*preview*/
	{ 2592, 1944,    0,    0, 2592, 1944, 2592, 1944,
	     8,    6, 2576, 1932,    0,    0, 2576, 1932 }, /*capture*/
	{ 2592, 1944,    0,    0, 2592, 1944, 2592, 1944,
	    16,  252, 2560, 1440,    0,    0, 2560, 1440 }, /*video*/
	{ 2592, 1944,    0,    0, 2592, 1944, 2592, 1944,
	     8,    6, 2576, 1932,    0,    0, 2576, 1932 }, /*hs video - Not Used*/
	{ 2592, 1944,    0,    0, 2592, 1944, 2592, 1944,
	     8,    6, 2576, 1932,    0,    0, 2576, 1932 }, /*slim video - Not Used*/
	{ 2592, 1944,    0,    0, 2592, 1944, 1296,  972,
	     8,    6, 1280,  960,    0,    0, 1280,  960 }, /*custom1*/
	{ 2592, 1944,    0,    0, 2592, 1944, 2592, 1944,
	     8,    6, 2576, 1932,    0,    0, 2576, 1932 }, /*custom2 - Not Used*/
	{ 2592, 1944,    0,    0, 2592, 1944, 2592, 1944,
	     8,    6, 2576, 1932,    0,    0, 2576, 1932 }, /*custom3 - Not Used*/
	{ 2592, 1944,    0,    0, 2592, 1944, 2592, 1944,
	     8,    6, 2576, 1932,    0,    0, 2576, 1932 }, /*custom4 - Not Used*/
	{ 2592, 1944,    0,    0, 2592, 1944, 2592, 1944,
	     8,    6, 2576, 1932,    0,    0, 2576, 1932 }, /*custom5 - Not Used*/
};

static kal_uint16 read_cmos_sensor_8(kal_uint16 addr)
{
	kal_uint16 get_byte = 0;
	char pu_send_cmd[2] = {(char)(addr >> 8), (char)(addr & 0xFF) };

	iReadRegI2C(pu_send_cmd, 2, (u8 *)&get_byte, 1, imgsensor.i2c_write_id);

	return get_byte;
}

static void write_cmos_sensor_8(kal_uint32 addr, kal_uint32 para)
{
	char pu_send_cmd[3] = {(char)(addr >> 8),
		(char)(addr & 0xFF), (char)(para & 0xFF)};

	iWriteRegI2C(pu_send_cmd, 3, imgsensor.i2c_write_id);
}

static int table_write_cmos_sensor(kal_uint16 *para, kal_uint32 len)
{
	int ret = 0;
	kal_uint8 puSendCmd[I2C_BUFFER_LEN];
	kal_uint32 tosend, IDX;
	kal_uint16 addr = 0, addr_last = 0, data;

	tosend = 0;
	IDX = 0;

	while (len > IDX) {
		addr = para[IDX];
		puSendCmd[tosend++] = (char)(addr >> 8);
		puSendCmd[tosend++] = (char)(addr & 0xFF);

		data = para[IDX + 1];
		puSendCmd[tosend++] = (char)(data & 0xFF);

		IDX += 2;
		addr_last = addr;

		if ((I2C_BUFFER_LEN - tosend) < TRANSFER_UNIT || IDX == len || addr != addr_last) {
			ret |= iBurstWriteReg_multi(puSendCmd, tosend, imgsensor.i2c_write_id,
					TRANSFER_UNIT, imgsensor_info.i2c_speed);
			tosend = 0;
		}
	}
	return 0;
}

static void set_dummy(void)
{
	LOG_DBG("dummyline = %d, dummypixels = %d\n", imgsensor.dummy_line, imgsensor.dummy_pixel);
	write_cmos_sensor_8(0x320E, (imgsensor.frame_length >> 8) & 0xFF);
	write_cmos_sensor_8(0x320F, imgsensor.frame_length & 0xFF);
	write_cmos_sensor_8(0x320C, (imgsensor.line_length >> 8) & 0xFF);
	write_cmos_sensor_8(0x320D, imgsensor.line_length & 0xFF);
}

static kal_uint32 return_sensor_id(void)
{
	return ((read_cmos_sensor_8(0x3107) << 8) | read_cmos_sensor_8(0x3108));
}

static void set_max_framerate(UINT16 framerate, kal_bool min_framelength_en)
{
	kal_uint32 frame_length = imgsensor.frame_length;

	frame_length = imgsensor.pclk / framerate * 10 / imgsensor.line_length;

	spin_lock(&imgsensor_drv_lock);
	imgsensor.frame_length = (frame_length > imgsensor.min_frame_length)
		? frame_length : imgsensor.min_frame_length;
	imgsensor.dummy_line = imgsensor.frame_length - imgsensor.min_frame_length;
	if (imgsensor.frame_length > imgsensor_info.max_frame_length) {
		imgsensor.frame_length = imgsensor_info.max_frame_length;
		imgsensor.dummy_line = imgsensor.frame_length - imgsensor.min_frame_length;
	}
	LOG_DBG("framelength = %d\n", imgsensor.frame_length);
	if (min_framelength_en)
		imgsensor.min_frame_length = imgsensor.frame_length;
	spin_unlock(&imgsensor_drv_lock);
	set_dummy();
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
 ************************************************************************/
static void set_shutter(kal_uint16 shutter)
{
	unsigned long flags;
	kal_uint16 realtime_fps = 0, cal_shutter = 0;

	spin_lock_irqsave(&imgsensor_drv_lock, flags);
	shutter *= 2; //need to multiple 2 for using value in kernel, only sc501cs
	imgsensor.shutter = shutter;
	spin_unlock_irqrestore(&imgsensor_drv_lock, flags);

	/*if shutter bigger than frame_length, should extend frame length first*/
	/*Note: shutter calculation is different form other sensors */
	spin_lock(&imgsensor_drv_lock);
	if (shutter > (2 * imgsensor.min_frame_length) - imgsensor_info.margin)
		imgsensor.frame_length = (shutter + imgsensor_info.margin + 1) / 2;
	else
		imgsensor.frame_length = imgsensor.min_frame_length;

	if (imgsensor.frame_length > imgsensor_info.max_frame_length)
		imgsensor.frame_length = imgsensor_info.max_frame_length;

	spin_unlock(&imgsensor_drv_lock);
	shutter = (shutter < imgsensor_info.min_shutter) ? imgsensor_info.min_shutter : shutter;
	shutter = (shutter > ((2 * imgsensor_info.max_frame_length) - imgsensor_info.margin)) ?
		((2 * imgsensor_info.max_frame_length) - imgsensor_info.margin) : shutter;

	realtime_fps = imgsensor.pclk / imgsensor.line_length * 10 / imgsensor.frame_length;

	if (imgsensor.autoflicker_en) {
		if (realtime_fps >= 297 && realtime_fps <= 305)
			set_max_framerate(296, 0);
		else if (realtime_fps >= 147 && realtime_fps <= 150)
			set_max_framerate(146, 0);
		else
			set_max_framerate(realtime_fps, 0);
	} else {
		set_max_framerate(realtime_fps, 0);
	}

	cal_shutter = shutter >> 2;
	cal_shutter = cal_shutter << 2;
	Dgain_ratio = 256 * shutter / cal_shutter;

	write_cmos_sensor_8(0x3e00, (cal_shutter >> 12) & 0xFF); // high
	write_cmos_sensor_8(0x3e01, (cal_shutter >> 4) & 0xFF); // middle
	write_cmos_sensor_8(0x3e02, (cal_shutter << 4) & 0xF0); // low

	LOG_DBG("Exit! shutter = %d, framelength = %d\n", shutter, imgsensor.frame_length);
	LOG_DBG("Exit! cal_shutter = %d, ", cal_shutter);
}

static void set_shutter_frame_length(kal_uint16 shutter, kal_uint16 frame_length)
{
	unsigned long flags;
	kal_uint16 realtime_fps = 0, cal_shutter = 0;
	kal_int32 dummy_line = 0;

	spin_lock_irqsave(&imgsensor_drv_lock, flags);
	shutter *= 2; //need to multiple 2 for using value in kernel, only sc501cs
	imgsensor.shutter = shutter;
	spin_unlock_irqrestore(&imgsensor_drv_lock, flags);

	/*if shutter bigger than frame_length, should extend frame length first*/
	/*Note: shutter calculation is different form other sensors */
	spin_lock(&imgsensor_drv_lock);
	if (frame_length > 1)
		dummy_line = frame_length - imgsensor.frame_length;
	imgsensor.frame_length = imgsensor.frame_length + dummy_line;

	if (shutter > (2 * imgsensor.min_frame_length) - imgsensor_info.margin)
		imgsensor.frame_length = (shutter + imgsensor_info.margin + 1) / 2;

	if (imgsensor.frame_length > imgsensor_info.max_frame_length)
		imgsensor.frame_length = imgsensor_info.max_frame_length;

	spin_unlock(&imgsensor_drv_lock);
	shutter = (shutter < imgsensor_info.min_shutter) ? imgsensor_info.min_shutter : shutter;
	shutter = (shutter > ((2 * imgsensor_info.max_frame_length) - imgsensor_info.margin)) ?
		((2 * imgsensor_info.max_frame_length) - imgsensor_info.margin) : shutter;

	realtime_fps = imgsensor.pclk / imgsensor.line_length * 10 / imgsensor.frame_length;

	if (imgsensor.autoflicker_en) {
		if (realtime_fps >= 297 && realtime_fps <= 305)
			set_max_framerate(296, 0);
		else if (realtime_fps >= 147 && realtime_fps <= 150)
			set_max_framerate(146, 0);
		else
			set_max_framerate(realtime_fps, 0);
	} else {
		set_max_framerate(realtime_fps, 0);
	}

	cal_shutter = shutter >> 2;
	cal_shutter = cal_shutter << 2;
	Dgain_ratio = 256 * shutter / cal_shutter;

	write_cmos_sensor_8(0x3E00, (cal_shutter >> 12) & 0xFF); // high
	write_cmos_sensor_8(0x3E01, (cal_shutter >> 4) & 0xFF); // middle
	write_cmos_sensor_8(0x3E02, (cal_shutter << 4) & 0xF0); //low

	LOG_INF("Exit! shutter = %d, framelength = %d\n", shutter, imgsensor.frame_length);
	LOG_INF("Exit! cal_shutter = %d, ", cal_shutter);
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
 ************************************************************************/
static kal_uint16 set_gain(kal_uint16 gain)
{
	kal_uint32 total_again = 0, total_dgain = 0;
	kal_uint32 dig_fine_gain = 0;
	kal_uint32 gain_step = 0;

	if (gain < imgsensor_info.min_gain)
		gain = imgsensor_info.min_gain;
	else if (gain > imgsensor_info.max_gain)
		gain = imgsensor_info.max_gain;

	if ((gain >= BASEGAIN) && (gain < 2 * BASEGAIN)) {
		total_again = 0x00;
		total_dgain = 0x00;
		gain_step = 1;
	} else if ((gain >= 2 * BASEGAIN) && (gain < 4 * BASEGAIN)) {
		total_again = 0x08;
		total_dgain = 0x00;
		gain_step = 2;
	} else if ((gain >= 4 * BASEGAIN) && (gain < 8 * BASEGAIN)) {
		total_again = 0x09;
		total_dgain = 0x00;
		gain_step = 4;
	} else if ((gain >= 8 * BASEGAIN) && (gain < 16 * BASEGAIN)) {
		total_again = 0x0B;
		total_dgain = 0x00;
		gain_step = 8;
	} else if ((gain >= 16 * BASEGAIN) && (gain < 32 * BASEGAIN)) {
		total_again = 0x0f;
		total_dgain = 0x00;
		gain_step = 16;
	} else if ((gain >= 32 * BASEGAIN) && (gain < 64 * BASEGAIN)) {
		total_again = 0x0f;
		total_dgain = 0x01;
		gain_step = 32;
	} else if ((gain >= 64 * BASEGAIN) && (gain < 128 * BASEGAIN)) {
		total_again = 0x0f;
		total_dgain = 0x03;
		gain_step = 64;
	} else if ((gain >= 128 * BASEGAIN) && (gain < 256 * BASEGAIN)) {
		total_again = 0x0f;
		total_dgain = 0x07;
		gain_step = 128;
	} else if ((gain >= 256 * BASEGAIN) && (gain < 512 * BASEGAIN)) {
		total_again = 0x0f;
		total_dgain = 0x0f;
		gain_step = 256;
	} else {
		total_again = 0x00;
		total_dgain = 0x00;
		gain_step = 1;
	}
	dig_fine_gain = (gain / gain_step) * 128 / BASEGAIN;
	write_cmos_sensor_8(0x3e09, total_again);
	write_cmos_sensor_8(0x3e06, total_dgain);
	write_cmos_sensor_8(0x3e07, dig_fine_gain);

	LOG_INF("Input Gain: %d,total_again=%d, Digital fine gain: %d", gain, total_again, dig_fine_gain);

	return gain;
}

static void ihdr_write_shutter_gain(kal_uint16 le, kal_uint16 se, kal_uint16 gain)
{
	LOG_INF("le: 0x%x, se: 0x%x, gain: 0x%x\n", le, se, gain);
}

/*
 *static void set_mirror_flip(kal_uint8 image_mirror)
 *{
 *	LOG_INF("image_mirror = %d\n", image_mirror);
 *}
 */

/*************************************************************************
 * FUNCTION
 *	night_mode
 *
 * DESCRIPTION
 *	This function night mode of sensor.
 *
 * PARAMETERS
 *	bEnable: KAL_TRUE -> enable night mode, otherwise, disable night mode
 *
 * RETURNS
 *	None
 *
 * GLOBALS AFFECTED
 *
 ************************************************************************/
static void night_mode(kal_bool enable)
{
	/* No Need to implement this function */
}

static kal_uint32 streaming_control(kal_bool enable)
{
	//unsigned short frame_cnt = 0;
	//int timeout_cnt = 0;
	LOG_INF("streaming_enable(0=Sw Standby,1=streaming): %d\n", enable);
	if (enable) {
		write_cmos_sensor_8(0x0100, 0x01);

		//10 ms delay for stabilization
		usleep_range(10000, 10100);
	} else {
		write_cmos_sensor_8(0x0100, 0x00);
		usleep_range(10000, 10100);
	}
	return ERROR_NONE;
}

static int set_mode_setfile(enum IMGSENSOR_MODE mode)
{
	int ret = -1;

	if (mode >= IMGSENSOR_MODE_MAX) {
		LOG_ERR("invalid mode: %d", mode);
		return -1;
	}
	LOG_INF("-E");
	LOG_INF("mode: %s", sc501cs_setfile_info[mode].name);

	if ((sc501cs_setfile_info[mode].setfile == NULL) || (sc501cs_setfile_info[mode].size == 0)) {
		LOG_ERR("failed, mode: %d", mode);
		ret = -1;
	} else
		ret = table_write_cmos_sensor(sc501cs_setfile_info[mode].setfile, sc501cs_setfile_info[mode].size);

	LOG_INF("-X");
	return ret;
}

static int sensor_init(void)
{
	int ret = ERROR_NONE;

	LOG_INF("%s:fixel_pixel: %d\n", __func__, imgsensor_info.sensor_output_dataformat);
	ret = set_mode_setfile(IMGSENSOR_MODE_INIT);

	return ret;
}

static kal_uint32 set_test_pattern_mode(kal_bool enable)
{
	//test pattern is always disable
	spin_lock(&imgsensor_drv_lock);
	imgsensor.test_pattern = false;
	spin_unlock(&imgsensor_drv_lock);
	LOG_INF("test pattern not supported");
	return ERROR_NONE;
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
 ************************************************************************/
static kal_uint32 get_imgsensor_id(UINT32 *sensor_id)
{
	kal_uint8 i = 0;
	kal_uint8 retry = 2;

	while (imgsensor_info.i2c_addr_table[i] != 0xff) {
		spin_lock(&imgsensor_drv_lock);
		imgsensor.i2c_write_id = imgsensor_info.i2c_addr_table[i];
		spin_unlock(&imgsensor_drv_lock);
		do {
			*sensor_id = return_sensor_id();
			LOG_INF("i2c write id: 0x%x, sensor id: 0x%x\n",
				imgsensor.i2c_write_id, *sensor_id);
			if (*sensor_id == imgsensor_info.sensor_id)
				return ERROR_NONE;

			retry--;
		} while (retry > 0);
		i++;
		retry = 2;
	}

	if (*sensor_id != imgsensor_info.sensor_id) {
		LOG_ERR("Read sensor id fail, write id: 0x%x, id: 0x%x\n",
			imgsensor.i2c_write_id, *sensor_id);
		/*if Sensor ID is not correct, Must set *sensor_id to 0xFFFFFFFF*/
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
 ************************************************************************/
static kal_uint32 open(void)
{
	kal_uint8 i = 0;
	kal_uint8 retry = 2;
	kal_uint32 sensor_id = 0;
	kal_uint32 ret = ERROR_NONE;

	LOG_1;

	while (imgsensor_info.i2c_addr_table[i] != 0xff) {
		spin_lock(&imgsensor_drv_lock);
		imgsensor.i2c_write_id = imgsensor_info.i2c_addr_table[i];
		spin_unlock(&imgsensor_drv_lock);
		do {
			sensor_id = return_sensor_id();
			LOG_INF("i2c write id: 0x%x, sensor id: 0x%x\n",
				imgsensor.i2c_write_id, sensor_id);
			if (sensor_id == imgsensor_info.sensor_id)
				break;

			retry--;
		} while (retry > 0);
		i++;
		if (sensor_id == imgsensor_info.sensor_id)
			break;
		retry = 2;
	}
	if (imgsensor_info.sensor_id != sensor_id) {
		LOG_ERR("Read sensor id fail, write id: 0x%x, id: 0x%x\n", imgsensor.i2c_write_id, sensor_id);
		return ERROR_SENSOR_CONNECT_FAIL;
	}
	/* initail sequence write in  */
	ret = sensor_init();

	spin_lock(&imgsensor_drv_lock);
	imgsensor.autoflicker_en = KAL_FALSE;
	imgsensor.sensor_mode = IMGSENSOR_MODE_INIT;
	imgsensor.pclk = imgsensor_info.pre.pclk;
	imgsensor.frame_length = imgsensor_info.pre.framelength;
	imgsensor.line_length = imgsensor_info.pre.linelength;
	imgsensor.min_frame_length = imgsensor_info.pre.framelength;
	imgsensor.dummy_pixel = 0;
	imgsensor.dummy_line = 0;
	imgsensor.ihdr_en = 0;
	imgsensor.test_pattern = KAL_FALSE;
	imgsensor.current_fps = imgsensor_info.pre.max_framerate;
	spin_unlock(&imgsensor_drv_lock);

	return ret;
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
 ************************************************************************/
static kal_uint32 close(void)
{
	LOG_INF("E\n");
	/* No Need to implement this function */
	streaming_control(KAL_FALSE);
	return ERROR_NONE;
}

static void set_imgsensor_mode_info(enum IMGSENSOR_MODE sensor_mode)
{
	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode		= sensor_mode;
	imgsensor.pclk				= imgsensor_info.mode_info[sensor_mode]->pclk;
	imgsensor.line_length		= imgsensor_info.mode_info[sensor_mode]->linelength;
	imgsensor.frame_length		= imgsensor_info.mode_info[sensor_mode]->framelength;
	imgsensor.min_frame_length	= imgsensor_info.mode_info[sensor_mode]->framelength;
	imgsensor.autoflicker_en	= KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);

}

static enum IMGSENSOR_MODE get_imgsensor_mode(enum MSDK_SCENARIO_ID_ENUM scenario_id)
{
	enum IMGSENSOR_MODE sensor_mode = IMGSENSOR_MODE_PREVIEW;

	imgsensor_info.is_invalid_mode = false;
	switch (scenario_id) {
	case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		sensor_mode = IMGSENSOR_MODE_PREVIEW;
		break;

	case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		sensor_mode = IMGSENSOR_MODE_CAPTURE;
		break;

	case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
		sensor_mode = IMGSENSOR_MODE_VIDEO;
		break;

	case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
		sensor_mode = IMGSENSOR_MODE_HIGH_SPEED_VIDEO;
		break;

	case MSDK_SCENARIO_ID_SLIM_VIDEO:
		sensor_mode = IMGSENSOR_MODE_SLIM_VIDEO;
		break;

	case MSDK_SCENARIO_ID_CUSTOM1:
		sensor_mode = IMGSENSOR_MODE_CUSTOM1;
		break;

	case MSDK_SCENARIO_ID_CUSTOM2:
		sensor_mode = IMGSENSOR_MODE_CUSTOM2;
		break;

	case MSDK_SCENARIO_ID_CUSTOM3:
		sensor_mode = IMGSENSOR_MODE_CUSTOM3;
		break;

	case MSDK_SCENARIO_ID_CUSTOM4:
		sensor_mode = IMGSENSOR_MODE_CUSTOM4;
		break;

	case MSDK_SCENARIO_ID_CUSTOM5:
		sensor_mode = IMGSENSOR_MODE_CUSTOM5;
		break;

	default:
		LOG_DBG("error scenario_id = %d, we use preview scenario", scenario_id);
		imgsensor_info.is_invalid_mode = true;
		sensor_mode = IMGSENSOR_MODE_PREVIEW;
		break;
	}
	return sensor_mode;
}

static kal_uint32 get_resolution(
	MSDK_SENSOR_RESOLUTION_INFO_STRUCT(*sensor_resolution))
{
	LOG_INF("- E\n");
	sensor_resolution->SensorFullWidth =
		imgsensor_info.mode_info[IMGSENSOR_MODE_CAPTURE]->grabwindow_width;
	sensor_resolution->SensorFullHeight =
		imgsensor_info.mode_info[IMGSENSOR_MODE_CAPTURE]->grabwindow_height;

	sensor_resolution->SensorPreviewWidth =
		imgsensor_info.mode_info[IMGSENSOR_MODE_PREVIEW]->grabwindow_width;
	sensor_resolution->SensorPreviewHeight =
		imgsensor_info.mode_info[IMGSENSOR_MODE_PREVIEW]->grabwindow_height;

	sensor_resolution->SensorVideoWidth =
		imgsensor_info.mode_info[IMGSENSOR_MODE_VIDEO]->grabwindow_width;
	sensor_resolution->SensorVideoHeight =
		imgsensor_info.mode_info[IMGSENSOR_MODE_VIDEO]->grabwindow_height;

	sensor_resolution->SensorHighSpeedVideoWidth =
		imgsensor_info.mode_info[IMGSENSOR_MODE_HIGH_SPEED_VIDEO]->grabwindow_width;
	sensor_resolution->SensorHighSpeedVideoHeight =
		imgsensor_info.mode_info[IMGSENSOR_MODE_HIGH_SPEED_VIDEO]->grabwindow_height;

	sensor_resolution->SensorSlimVideoWidth =
		imgsensor_info.mode_info[IMGSENSOR_MODE_SLIM_VIDEO]->grabwindow_width;
	sensor_resolution->SensorSlimVideoHeight =
		imgsensor_info.mode_info[IMGSENSOR_MODE_SLIM_VIDEO]->grabwindow_height;

	sensor_resolution->SensorCustom1Width =
		imgsensor_info.mode_info[IMGSENSOR_MODE_CUSTOM1]->grabwindow_width;
	sensor_resolution->SensorCustom1Height =
		imgsensor_info.mode_info[IMGSENSOR_MODE_CUSTOM1]->grabwindow_height;

	sensor_resolution->SensorCustom2Width =
		imgsensor_info.mode_info[IMGSENSOR_MODE_CUSTOM2]->grabwindow_width;
	sensor_resolution->SensorCustom2Height =
		imgsensor_info.mode_info[IMGSENSOR_MODE_CUSTOM2]->grabwindow_height;

	sensor_resolution->SensorCustom3Width =
		imgsensor_info.mode_info[IMGSENSOR_MODE_CUSTOM3]->grabwindow_width;
	sensor_resolution->SensorCustom3Height =
		imgsensor_info.mode_info[IMGSENSOR_MODE_CUSTOM3]->grabwindow_height;

	sensor_resolution->SensorCustom4Width =
		imgsensor_info.mode_info[IMGSENSOR_MODE_CUSTOM4]->grabwindow_width;
	sensor_resolution->SensorCustom4Height =
		imgsensor_info.mode_info[IMGSENSOR_MODE_CUSTOM4]->grabwindow_height;

	sensor_resolution->SensorCustom5Width =
		imgsensor_info.mode_info[IMGSENSOR_MODE_CUSTOM5]->grabwindow_width;
	sensor_resolution->SensorCustom5Height =
		imgsensor_info.mode_info[IMGSENSOR_MODE_CUSTOM5]->grabwindow_height;

	return ERROR_NONE;
}

static kal_uint32 get_info(enum MSDK_SCENARIO_ID_ENUM scenario_id,
			   MSDK_SENSOR_INFO_STRUCT *sensor_info,
			   MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	enum IMGSENSOR_MODE sensor_mode;

	LOG_INF("scenario_id = %d\n", scenario_id);
		/*not use*/
/*sensor_info->SensorVideoFrameRate =
 *	imgsensor_info.normal_video.max_framerate/10;
 *sensor_info->SensorStillCaptureFrameRate=
 *	imgsensor_info.cap.max_framerate/10;
 *imgsensor_info->SensorWebCamCaptureFrameRate=
 *	imgsensor_info.v.max_framerate;
 */
	sensor_info->SensorClockPolarity = SENSOR_CLOCK_POLARITY_LOW;
	sensor_info->SensorClockFallingPolarity = SENSOR_CLOCK_POLARITY_LOW;	/*not use*/
	sensor_info->SensorHsyncPolarity =
		SENSOR_CLOCK_POLARITY_LOW;			/*inverse with datasheet*/
	sensor_info->SensorVsyncPolarity = SENSOR_CLOCK_POLARITY_LOW;
	sensor_info->SensorInterruptDelayLines = 4;		/*not use*/
	sensor_info->SensorResetActiveHigh = FALSE;		/*not use*/
	sensor_info->SensorResetDelayCount = 5;			/*not use*/

	sensor_info->SensroInterfaceType = imgsensor_info.sensor_interface_type;
	sensor_info->MIPIsensorType = imgsensor_info.mipi_sensor_type;
	sensor_info->SettleDelayMode = imgsensor_info.mipi_settle_delay_mode;
	sensor_info->SensorOutputDataFormat = imgsensor_info.sensor_output_dataformat;

	sensor_info->CaptureDelayFrame = imgsensor_info.cap_delay_frame;
	sensor_info->PreviewDelayFrame = imgsensor_info.pre_delay_frame;
	sensor_info->VideoDelayFrame = imgsensor_info.video_delay_frame;
	sensor_info->HighSpeedVideoDelayFrame = imgsensor_info.hs_video_delay_frame;
	sensor_info->SlimVideoDelayFrame = imgsensor_info.slim_video_delay_frame;
	sensor_info->Custom1DelayFrame = imgsensor_info.custom1_delay_frame;
	sensor_info->Custom2DelayFrame = imgsensor_info.custom2_delay_frame;
	sensor_info->Custom3DelayFrame = imgsensor_info.custom3_delay_frame;
	sensor_info->Custom4DelayFrame = imgsensor_info.custom4_delay_frame;
	sensor_info->Custom5DelayFrame = imgsensor_info.custom5_delay_frame;

	sensor_info->SensorMasterClockSwitch = 0;		/*not use*/
	sensor_info->SensorDrivingCurrent = imgsensor_info.isp_driving_current;

	sensor_info->AEShutDelayFrame = imgsensor_info.ae_shut_delay_frame;
	/*The frame of setting shutter default 0 for TG int*/
	sensor_info->AESensorGainDelayFrame = imgsensor_info.ae_sensor_gain_delay_frame;
	/*The frame of setting sensor gain*/
	sensor_info->AEISPGainDelayFrame = imgsensor_info.ae_ispGain_delay_frame;
	sensor_info->IHDR_Support = imgsensor_info.ihdr_support;
	sensor_info->IHDR_LE_FirstLine = imgsensor_info.ihdr_le_firstline;
	sensor_info->SensorModeNum = imgsensor_info.sensor_mode_num;

	sensor_info->SensorMIPILaneNumber = imgsensor_info.mipi_lane_num;
	sensor_info->SensorClockFreq = imgsensor_info.mclk;
	sensor_info->SensorClockDividCount = 3;			/*not use*/
	sensor_info->SensorClockRisingCount = 0;
	sensor_info->SensorClockFallingCount = 2;		/*not use*/
	sensor_info->SensorPixelClockCount = 3;			/*not use*/
	sensor_info->SensorDataLatchCount = 2;			/*not use*/
	sensor_info->TEMPERATURE_SUPPORT  = imgsensor_info.temperature_support;
	sensor_info->MIPIDataLowPwr2HighSpeedTermDelayCount = 0;
	sensor_info->MIPICLKLowPwr2HighSpeedTermDelayCount = 0;
	sensor_info->SensorWidthSampling = 0;			/*0 is default 1x*/
	sensor_info->SensorHightSampling = 0;			/*0 is default 1x*/
	sensor_info->SensorPacketECCOrder = 1;

	sensor_info->FrameTimeDelayFrame = imgsensor_info.frame_time_delay_frame;

	LOG_INF("scenario_id = %d\n", scenario_id);
	sensor_mode = get_imgsensor_mode(scenario_id);

	sensor_info->SensorGrabStartX = imgsensor_info.mode_info[sensor_mode]->startx;
	sensor_info->SensorGrabStartY = imgsensor_info.mode_info[sensor_mode]->starty;
	sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
		imgsensor_info.mode_info[sensor_mode]->mipi_data_lp2hs_settle_dc;

	return ERROR_NONE;
}

static kal_uint32 control(enum MSDK_SCENARIO_ID_ENUM scenario_id,
	MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
	MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	enum IMGSENSOR_MODE sensor_mode;

	LOG_INF("-E");
	LOG_INF("scenario_id = %d", scenario_id);
	spin_lock(&imgsensor_drv_lock);
	imgsensor.current_scenario_id = scenario_id;
	spin_unlock(&imgsensor_drv_lock);

	sensor_mode = get_imgsensor_mode(scenario_id);

	set_imgsensor_mode_info(sensor_mode);
	set_mode_setfile(sensor_mode);
	if (imgsensor_info.is_invalid_mode) {
		LOG_ERR("Invalid scenario id - use preview setting");
		return ERROR_INVALID_SCENARIO_ID;
	}
	LOG_INF("-X");
	return ERROR_NONE;
}

static kal_uint32 set_video_mode(UINT16 framerate)
{
	/* This Function not used after ROME */
	LOG_INF("framerate = %d\n ", framerate);
	/* SetVideoMode Function should fix framerate */
	if (framerate == 0) /* Dynamic frame rate */
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

static kal_uint32 set_auto_flicker_mode(kal_bool enable, UINT16 framerate)
{
	LOG_DBG("enable = %d, framerate = %d\n", enable, framerate);
	spin_lock(&imgsensor_drv_lock);
	if (enable) {/* enable auto flicker */
		imgsensor.autoflicker_en = KAL_TRUE;
	} else {		/* Cancel Auto flick */
		imgsensor.autoflicker_en = KAL_FALSE;
	}
	spin_unlock(&imgsensor_drv_lock);
	return ERROR_NONE;
}

static void set_frame_length(enum IMGSENSOR_MODE sensor_mode, MUINT32 framerate)
{
	kal_uint32 frame_length;

	frame_length = imgsensor_info.mode_info[sensor_mode]->pclk / framerate * 10 /
		imgsensor_info.mode_info[sensor_mode]->linelength;

	spin_lock(&imgsensor_drv_lock);

	imgsensor.dummy_line = (frame_length > imgsensor_info.mode_info[sensor_mode]->framelength)
		? (frame_length - imgsensor_info.mode_info[sensor_mode]->framelength) : 0;

	imgsensor.frame_length = imgsensor_info.mode_info[sensor_mode]->framelength + imgsensor.dummy_line;

	imgsensor.min_frame_length = imgsensor.frame_length;

	spin_unlock(&imgsensor_drv_lock);

	if (imgsensor.frame_length > imgsensor.shutter)
		set_dummy();
}

static kal_uint32 set_max_framerate_by_scenario(
		enum MSDK_SCENARIO_ID_ENUM scenario_id, MUINT32 framerate)
{
	enum IMGSENSOR_MODE sensor_mode;

	LOG_INF("scenario_id = %d, framerate = %d\n", scenario_id, framerate);

	sensor_mode = get_imgsensor_mode(scenario_id);
	set_frame_length(sensor_mode, framerate);

	return ERROR_NONE;
}

static kal_uint32 get_default_framerate_by_scenario(
		enum MSDK_SCENARIO_ID_ENUM scenario_id, MUINT32 *framerate)
{
	enum IMGSENSOR_MODE sensor_mode;

	sensor_mode = get_imgsensor_mode(scenario_id);
	if (!imgsensor_info.is_invalid_mode)
		*framerate = imgsensor_info.mode_info[sensor_mode]->max_framerate;

	LOG_INF("scenario_id = %d, framerate = %d", scenario_id, *framerate);
	return ERROR_NONE;
}

static kal_uint32 feature_control(MSDK_SENSOR_FEATURE_ENUM feature_id,
	UINT8 *feature_para, UINT32 *feature_para_len)
{
	UINT16 *feature_return_para_16 = (UINT16 *)feature_para;
	UINT16 *feature_data_16 = (UINT16 *)feature_para;
	UINT32 *feature_return_para_32 = (UINT32 *)feature_para;
	UINT32 *feature_data_32 = (UINT32 *)feature_para;
	unsigned long long *feature_data = (unsigned long long *) feature_para;
	/* unsigned long long *feature_return_para=(unsigned long long *) feature_para; */

	struct SENSOR_WINSIZE_INFO_STRUCT *wininfo;
	MSDK_SENSOR_REG_INFO_STRUCT *sensor_reg_data = (MSDK_SENSOR_REG_INFO_STRUCT *)feature_para;

	enum IMGSENSOR_MODE sensor_mode;

	LOG_DBG("feature_id = %d\n", feature_id);
	switch (feature_id) {
	case SENSOR_FEATURE_GET_PIXEL_CLOCK_FREQ_BY_SCENARIO:
		sensor_mode = get_imgsensor_mode(*feature_data);
		*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) = imgsensor_info.mode_info[sensor_mode]->pclk;
		break;
	case SENSOR_FEATURE_GET_PERIOD_BY_SCENARIO:
		sensor_mode = get_imgsensor_mode(*feature_data);
		*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) = (imgsensor_info.mode_info[sensor_mode]->framelength << 16)
			+ imgsensor_info.mode_info[sensor_mode]->linelength;
		break;
	case SENSOR_FEATURE_GET_PERIOD:
		*feature_return_para_16++ = imgsensor.line_length;
		*feature_return_para_16 = imgsensor.frame_length;
		*feature_para_len = 4;
		break;
	case SENSOR_FEATURE_GET_PIXEL_CLOCK_FREQ:
		*feature_return_para_32 = imgsensor.pclk;
		*feature_para_len = 4;
		break;

	case SENSOR_FEATURE_GET_MIPI_PIXEL_RATE:
		sensor_mode = get_imgsensor_mode(*feature_data);
		*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) = imgsensor_info.mode_info[sensor_mode]->mipi_pixel_rate;
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
	case SENSOR_FEATURE_SET_ESHUTTER:
		set_shutter(*feature_data);
		break;
	case SENSOR_FEATURE_SET_NIGHTMODE:
		night_mode((BOOL)*feature_data);
		break;
	case SENSOR_FEATURE_SET_GAIN:
		set_gain((UINT16)*feature_data);
		break;
	case SENSOR_FEATURE_SET_FLASHLIGHT:
		break;
	case SENSOR_FEATURE_SET_ISP_MASTER_CLOCK_FREQ:
		break;
	case SENSOR_FEATURE_SET_SHUTTER_FRAME_TIME:
		set_shutter_frame_length((UINT16) (*feature_data), (UINT16) (*(feature_data + 1)));
		break;
	case SENSOR_FEATURE_SET_REGISTER:
		write_cmos_sensor_8(sensor_reg_data->RegAddr, sensor_reg_data->RegData);
		break;
	case SENSOR_FEATURE_GET_REGISTER:
		sensor_reg_data->RegData = read_cmos_sensor_8(sensor_reg_data->RegAddr);
		LOG_INF("adb_i2c_read 0x%x = 0x%x\n",
			sensor_reg_data->RegAddr, sensor_reg_data->RegData);
		break;
	case SENSOR_FEATURE_GET_LENS_DRIVER_ID:
		/* get the lens driver ID from EEPROM or just return LENS_DRIVER_ID_DO_NOT_CARE */
		/* if EEPROM does not exist in camera module. */
		*feature_return_para_32 = LENS_DRIVER_ID_DO_NOT_CARE;
		*feature_para_len = 4;
		break;
	case SENSOR_FEATURE_SET_VIDEO_MODE:
		set_video_mode(*feature_data);
		break;
	case SENSOR_FEATURE_CHECK_SENSOR_ID:
		get_imgsensor_id(feature_return_para_32);
		break;
	case SENSOR_FEATURE_SET_AUTO_FLICKER_MODE:
		set_auto_flicker_mode((BOOL)*feature_data_16, *(feature_data_16 + 1));
		break;
	case SENSOR_FEATURE_SET_MAX_FRAME_RATE_BY_SCENARIO:
		 set_max_framerate_by_scenario(
				(enum MSDK_SCENARIO_ID_ENUM)*feature_data,
				*(feature_data+1));
		break;
	case SENSOR_FEATURE_GET_DEFAULT_FRAME_RATE_BY_SCENARIO:
		get_default_framerate_by_scenario((enum MSDK_SCENARIO_ID_ENUM)*(feature_data),
			(MUINT32 *)(uintptr_t)(*(feature_data + 1)));
		break;
	case SENSOR_FEATURE_SET_TEST_PATTERN:
		set_test_pattern_mode((BOOL)*feature_data);
		break;
	case SENSOR_FEATURE_GET_TEST_PATTERN_CHECKSUM_VALUE: /*for factory mode auto testing */
		*feature_return_para_32 = imgsensor_info.checksum_value;
		*feature_para_len = 4;
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
		imgsensor.ihdr_en = (UINT8)*feature_data_32;
		spin_unlock(&imgsensor_drv_lock);
		break;
	case SENSOR_FEATURE_GET_CROP_INFO:
		LOG_DBG("SENSOR_FEATURE_GET_CROP_INFO = %d\n", feature_id);
		LOG_INF("SENSOR_FEATURE_GET_CROP_INFO scenarioId:%d\n", (UINT32) *feature_data);
		wininfo = (struct SENSOR_WINSIZE_INFO_STRUCT *) (uintptr_t) (*(feature_data + 1));

		sensor_mode = get_imgsensor_mode(*feature_data);
		memcpy((void *)wininfo, (void *)&imgsensor_winsize_info[sensor_mode],
				sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
		break;
	case SENSOR_FEATURE_SET_IHDR_SHUTTER_GAIN:
		LOG_INF("SENSOR_SET_SENSOR_IHDR LE=%d, SE=%d, Gain=%d\n",
			(UINT16)*feature_data, (UINT16)*(feature_data + 1),
				(UINT16)*(feature_data + 2));
		ihdr_write_shutter_gain
			((UINT16)*feature_data, (UINT16)*(feature_data + 1),
				(UINT16)*(feature_data + 2));
		break;
	case SENSOR_FEATURE_GET_FRAME_CTRL_INFO_BY_SCENARIO:
		/*
		 * 1, if driver support new sw frame sync
		 * set_shutter_frame_length() support third para auto_extend_en
		 */
		*(feature_data + 1) = 1; /* margin info by scenario */
		*(feature_data + 2) = imgsensor_info.margin;
		break;
	case SENSOR_FEATURE_GET_AWB_REQ_BY_SCENARIO:
		*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) = 0;
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
}

static struct SENSOR_FUNCTION_STRUCT sensor_func = {
	open,
	get_info,
	get_resolution,
	feature_control,
	control,
	close
};

void sc501cs_otp_off_setting(void)
{
	write_cmos_sensor_8(0x0100, 0x00);
	write_cmos_sensor_8(0x4424, 0x01);
	usleep_range(1000, 1010);
	write_cmos_sensor_8(0x4408, 0x00);
	write_cmos_sensor_8(0x4409, 0x00);
	write_cmos_sensor_8(0x440A, 0x07);
	write_cmos_sensor_8(0x440B, 0xFF);
	write_cmos_sensor_8(0x4401, 0x1F);
	usleep_range(50000, 50100);
}

int sc501cs_read_otp_cal(unsigned int addr, unsigned char *data, unsigned int size)
{
	kal_uint32 i = 0;
	kal_uint8 otp_bank = 0;
	kal_uint32 read_addr = 0;

	u8 status = 1;
	int cnt = 0;

	LOG_INF("- E");

	if (data == NULL) {
		LOG_ERR("%s: fail, data is NULL\n", __func__);
		return -1;
	}

	streaming_control(KAL_TRUE);

	write_cmos_sensor_8(0x36B0, 0x4C);
	write_cmos_sensor_8(0x36B1, 0xD8);
	write_cmos_sensor_8(0x36B2, 0x01);

	otp_bank = (kal_uint8)read_cmos_sensor_8(SC501CS_OTP_CHECK_BANK);
	LOG_INF("check OTP bank:0x%x, size: %d\n", otp_bank, size);

	switch (otp_bank) {
	case SC501CS_OTP_BANK1_MARK:
		read_addr = SC501CS_OTP_BANK1_START_ADDR;
		write_cmos_sensor_8(0x4408, 0x00);
		write_cmos_sensor_8(0x4409, 0x00);

		write_cmos_sensor_8(0x440A, 0x07);
		write_cmos_sensor_8(0x440B, 0xFF);

		write_cmos_sensor_8(0x4401, 0x1F);

		break;
	case SC501CS_OTP_BANK2_MARK:
		read_addr = SC501CS_OTP_BANK2_START_ADDR;
		write_cmos_sensor_8(0x4408, 0x08);
		write_cmos_sensor_8(0x4409, 0x00);

		write_cmos_sensor_8(0x440A, 0x0F);
		write_cmos_sensor_8(0x440B, 0xFF);

		write_cmos_sensor_8(0x4401, 0x1E);

		break;
	default:
		LOG_ERR("check bank: fail");
		sc501cs_otp_off_setting();
		return -1;
	}

	write_cmos_sensor_8(0x4400, 0x11);

	usleep_range(10000, 10100);

	/* Read completed check */
	status = 1;
	cnt = 0;
	while (status == 0x01) {
		status = (kal_uint8)read_cmos_sensor_8(0x4420);
		status &= 0x01;
		usleep_range(1000, 1010);
		LOG_INF("1st read completed check is still ongoing (cnt:%d)", cnt++);
	}

	for (i = 0; i < size; i++) {
		*(data + i) = (kal_uint8)read_cmos_sensor_8(read_addr);
		LOG_DBG("read data read_addr: %x, data: %x", read_addr, *(data + i));
		read_addr++;
	}

	/* OTP off setting */
	sc501cs_otp_off_setting();
	LOG_INF("- X");

	return (int)size;
}

UINT32 SC501CS_MIPI_RAW_SensorInit(struct SENSOR_FUNCTION_STRUCT **pfFunc)
{
	/* To Do : Check Sensor status here */
	if (pfFunc != NULL)
		*pfFunc = &sensor_func;
	return ERROR_NONE;
}
