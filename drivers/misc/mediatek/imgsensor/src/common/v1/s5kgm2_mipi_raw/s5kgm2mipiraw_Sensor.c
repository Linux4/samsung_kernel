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
 *	 s5kgm2mipiraw_Sensor.c
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
#define PFX "S5KGM2"
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

#include "imgsensor_sysfs.h"

#include "s5kgm2mipiraw_Sensor.h"

#define WRITE_SENSOR_CAL_FOR_GGC				(1)

#define MULTI_WRITE 1
#if MULTI_WRITE
static const int I2C_BUFFER_LEN = 1020;
#else
static const int I2C_BUFFER_LEN = 4;
#endif

#define INDIRECT_BURST 1
#if INDIRECT_BURST
static const int I2C_BURST_BUFFER_LEN = 1400;
#endif

/**************************** Modify end *****************************/

#define LOG_DEBUG 1
#if LOG_DEBUG
#define LOG_INF(format, args...) pr_info(PFX "[%s] " format, __func__, ##args)
#endif

static DEFINE_SPINLOCK(imgsensor_drv_lock);
//Sensor ID Value: 0x08D2//record sensor id defined in Kd_imgsensor.h
static struct imgsensor_info_struct imgsensor_info = {
	.sensor_id = S5KGM2_SENSOR_ID,
	.checksum_value = 0xc98e6b72,
	.pre = {
		.pclk = 962000000,
		.linelength  = 9984,
		.framelength = 3186,
		.startx = 0,
		.starty = 0,
		.grabwindow_width  = 4000,
		.grabwindow_height = 3000,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 300,
		.mipi_pixel_rate = 540800000,
	},
	.cap = {
		.pclk = 962000000,
		.linelength  = 9984,
		.framelength = 3186,
		.startx = 0,
		.starty = 0,
		.grabwindow_width  = 4000,
		.grabwindow_height = 3000,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 300,
		.mipi_pixel_rate = 540800000,
	},
	.normal_video = {
		.pclk = 962000000,
		.linelength  = 9984,
		.framelength = 3186,
		.startx = 0,
		.starty = 0,
		.grabwindow_width  = 4000,
		.grabwindow_height = 2256,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 300,
		.mipi_pixel_rate = 540800000,
	},
	.hs_video = {
		.pclk = 641330000,
		.linelength  = 4224,
		.framelength = 1264,
		.startx = 0,
		.starty = 0,
		.grabwindow_width  = 1984,
		.grabwindow_height = 1112,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 1200,
		.mipi_pixel_rate = 540800000,
	},
	.slim_video = {
		.pclk = 641330000,
		.linelength  = 10240,
		.framelength = 6115,
		.startx = 0,
		.starty = 0,
		.grabwindow_width  = 8000,
		.grabwindow_height = 6000,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 100,
		.mipi_pixel_rate = 821600000,
	},
	.custom1 = {
		.pclk = 641330000,
		.linelength  = 4224,
		.framelength = 2530,
		.startx = 0,
		.starty = 0,
		.grabwindow_width  = 1920,
		.grabwindow_height = 1440,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 600,
		.mipi_pixel_rate = 540800000,
	},
	.custom2 = {
		.pclk = 641330000,
		.linelength  = 10240,
		.framelength = 6115,
		.startx = 0,
		.starty = 0,
		.grabwindow_width  = 8000,
		.grabwindow_height = 6000,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 100,
		.mipi_pixel_rate = 821600000,
	},
	.custom3 = { // NOT USED
		.pclk = 962000000,
		.linelength  = 9984,
		.framelength = 3186,
		.startx = 0,
		.starty = 0,
		.grabwindow_width  = 4000,
		.grabwindow_height = 3000,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 300,
		.mipi_pixel_rate = 540800000,
	},
	.custom4 = { // NOT USED
		.pclk = 962000000,
		.linelength  = 9984,
		.framelength = 3186,
		.startx = 0,
		.starty = 0,
		.grabwindow_width  = 4000,
		.grabwindow_height = 3000,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 300,
		.mipi_pixel_rate = 540800000,
	},
	.custom5 = { // NOT USED
		.pclk = 962000000,
		.linelength  = 9984,
		.framelength = 3186,
		.startx = 0,
		.starty = 0,
		.grabwindow_width  = 4000,
		.grabwindow_height = 3000,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 300,
		.mipi_pixel_rate = 540800000,
	},
	.margin = 10,
	.min_shutter = 8,
	.max_frame_length = 0xFFFF,

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

#ifdef CONFIG_IMPROVE_CAMERA_PERFORMANCE
	.custom1_delay_frame = 1,
	.custom2_delay_frame = 1,
	.custom3_delay_frame = 1,
	.custom4_delay_frame = 1,
	.custom5_delay_frame = 1,
	.slim_video_delay_frame = 1,
#else
	.custom1_delay_frame = 3,
	.custom2_delay_frame = 3,
	.custom3_delay_frame = 3,
	.custom4_delay_frame = 3,
	.custom5_delay_frame = 3,
	.slim_video_delay_frame = 3,
#endif

	.isp_driving_current = ISP_DRIVING_8MA,
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
	.current_fps = 0,	// full size current fps : 24fps for PIP, 30fps for Normal or ZSD

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

/*VC1 for L-PD(DT=0X34) , VC2 for R-PD(DT=0X31), unit : 10bit */
static struct SENSOR_VC_INFO_STRUCT SENSOR_VC_INFO[4] = {
	/* Preview mode setting */
	{0x02, 0x0a, 0x00, 0x08, 0x40, 0x00,
	 0x00, 0x2b, 0x0fa0, 0x0bb8,/*VC0*/
	 0x00, 0x00, 0x0000, 0x0000,/*VC1*/
	 0x01, 0x2b, 0x0271, 0x0ba0,/*VC2*/
	 0x00, 0x00, 0x0000, 0x0000},/*VC3*/
	/* cap = pre mode setting */
	{0x02, 0x0a, 0x00, 0x08, 0x40, 0x00,
	 0x00, 0x2b, 0x0fa0, 0x0bb8,/*VC0*/
	 0x00, 0x00, 0x0000, 0x0000,/*VC1*/
	 0x01, 0x2b, 0x0271, 0x0ba0,/*VC2*/
	 0x00, 0x00, 0x0000, 0x0000},/*VC3*/
	/* Video mode setting */
	{0x02, 0x0a, 0x00, 0x08, 0x40, 0x00,
	 0x00, 0x2b, 0x0fa0, 0x08d0,
	 0x00, 0x00, 0x0000, 0x0000,
	 0x01, 0x2b, 0x0271, 0x08c0,
	 0x00, 0x00, 0x0000, 0x0000},
	/* hs mode setting */
	{0x01, 0x0a, 0x00, 0x08, 0x40, 0x00,
	 0x00, 0x2b, 0x07d0, 0x0468,
	 0x00, 0x00, 0x00, 0x00,
	 0x00, 0x00, 0x00, 0x00,
	 0x00, 0x00, 0x0000, 0x0000},
};

static struct SENSOR_WINSIZE_INFO_STRUCT imgsensor_winsize_info[] = {
	{ 8000, 6000,    0,    0, 8000, 6000, 4000, 3000,
	     0,    0, 4000, 3000,    0,    0, 4000, 3000 }, /* Preview */
	{ 8000, 6000,    0,    0, 8000, 6000, 4000, 3000,
	     0,    0, 4000, 3000,    0,    0, 4000, 3000 }, /* capture */
	{ 8000, 6000,    0,  744, 8000, 4512, 4000, 2256,
	     0,  0,   4000, 2256,    0,    0, 4000, 2256 }, /* video */
	{ 8000, 6000,   32,  776, 7936, 4448, 1984, 1112,
	     0,    0, 1984, 1112,    0,    0, 1984, 1112 }, /* hs video */
	{ 8000, 6000,    0,    0, 8000, 6000, 8000, 6000,
	     0,    0, 8000, 6000,    0,    0, 8000, 6000 }, /* slim video */
	{ 8000, 6000,  160,  120, 7680, 5760, 1920, 1440,
	     0,    0, 1920, 1440,    0,    0, 1920, 1440 }, /* custom1 */
	{ 8000, 6000,    0,    0, 8000, 6000, 8000, 6000,
	     0,    0, 8000, 6000,    0,    0, 8000, 6000 }, /* custom2 */
	{ 8000, 6000,	 0,    0, 8000, 6000, 8000, 6000,
		 0,    0, 8000, 6000,	 0,    0, 8000, 6000 }, /* custom3 - NOT USED */
	{ 8000, 6000,	 0,    0, 8000, 6000, 8000, 6000,
		 0,    0, 8000, 6000,	 0,    0, 8000, 6000 }, /* custom4 - NOT USED */
	{ 8000, 6000,	 0,    0, 8000, 6000, 8000, 6000,
		 0,    0, 8000, 6000,	 0,    0, 8000, 6000 }, /* custom5 - NOT USED */
};

static struct SET_PD_BLOCK_INFO_T imgsensor_pd_info = {
	.i4OffsetX	= 0,
	.i4OffsetY	= 12,
	.i4PitchX	= 16,
	.i4PitchY	= 16,
	.i4PairNum	= 16,
	.i4SubBlkW	= 8,
	.i4SubBlkH	= 2,
	.i4BlockNumX	= 250,
	.i4BlockNumY	= 186,
	.iMirrorFlip	= 0,
	.i4PosR = {
			{0, 12}, {8, 12}, {2, 15}, {10, 15},
			{6, 16}, {14, 16}, {4, 19}, {12, 19},
			{0, 20}, {8, 20}, {2, 23}, {10, 23},
			{6, 24}, {14, 24}, {4, 27}, {12, 27},
		},
	.i4PosL = {
			{1, 12}, {9, 12}, {3, 15}, {11, 15},
			{7, 16}, {15, 16}, {5, 19}, {13, 19},
			{1, 20}, {9, 20}, {3, 23}, {11, 23},
			{7, 24}, {15, 24}, {5, 27}, {13, 27},
		},
	.i4Crop = { {0, 0}, {0, 0}, {0, 372}, {0, 0}, {0, 0},
		    {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0} }
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

static kal_uint16 read_cmos_sensor_8(kal_uint16 addr)
{
	kal_uint16 get_byte = 0;
	char pusendcmd[2] = { (char)(addr >> 8), (char)(addr & 0xFF) };

	iReadRegI2CTiming(pusendcmd, 2, (u8 *) &get_byte, 1,
		imgsensor.i2c_write_id, imgsensor_info.i2c_speed);
	return get_byte;
}

static void write_cmos_sensor_8(kal_uint16 addr, kal_uint8 para)
{
	char pusendcmd[3] = {
		(char)(addr >> 8), (char)(addr & 0xFF), (char)(para & 0xFF) };

	iWriteRegI2CTiming(pusendcmd, 3,
		imgsensor.i2c_write_id, imgsensor_info.i2c_speed);
}

static void set_dummy(void)
{
	pr_debug("dummyline = %d, dummypixels = %d\n",
		imgsensor.dummy_line, imgsensor.dummy_pixel);

	write_cmos_sensor(0x0340, imgsensor.frame_length);
	write_cmos_sensor(0x0342, imgsensor.line_length);
}

#if INDIRECT_BURST
static kal_int32 burst_table_write_cmos_sensor(kal_uint16 *para, kal_uint32 len)
{
	char puSendCmd[I2C_BURST_BUFFER_LEN];
	kal_uint32 tosend = 0, data_count = 0;
	int ret = -1;
	kal_uint16 data = 0;

	while (len > data_count) {
		data = para[data_count++];
		puSendCmd[tosend++] = (char)(data >> 8);
		puSendCmd[tosend++] = (char)(data & 0xFF);
	}

	ret = iBurstWriteReg_multi(puSendCmd, tosend,
		imgsensor.i2c_write_id, tosend, imgsensor_info.i2c_speed);
	if (ret < 0)
		pr_err("%s: fail!\n", __func__);

	return ret;
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

	mDELAY(3);
	for (i = 0; i < timeout; i++) {
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

	if (i >= timeout)
		pr_err("%s[onoff:%d] timeout, wait_time:%d\n", __func__, onoff, i * check_delay + 3);
	else
		LOG_INF("%s[onoff:%d], wait_time:%d\n", __func__, onoff, i * check_delay + 3);
}

static kal_uint32 streaming_control(kal_bool enable)
{
	LOG_INF("streaming_enable(0=Sw Standby,1=streaming): %d\n", enable);

	if (enable) {
		LOG_INF("%s enable = %d\n", __func__, enable);
		write_cmos_sensor(0x0100, 0x0100);
	} else {
		LOG_INF("%s streamoff enable = %d the frame_cnt = %d \n",
				__func__, enable, read_cmos_sensor_8(0x0005));
		write_cmos_sensor(0x0100, 0x0000);
		// check only stream off
		wait_stream_onoff(enable);
	}
	return ERROR_NONE;
}

static void write_shutter(kal_uint16 shutter)
{
	kal_uint16 realtime_fps = 0;

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

	if (imgsensor.autoflicker_en) {
		realtime_fps = imgsensor.pclk /
		imgsensor.line_length * 10 / imgsensor.frame_length;

		if (realtime_fps >= 297 && realtime_fps <= 305)
			set_max_framerate(296, 0);
		else if (realtime_fps >= 147 && realtime_fps <= 150)
			set_max_framerate(146, 0);
		else
			write_cmos_sensor(0x0340, imgsensor.frame_length & 0xFFFF);
	} else {
		write_cmos_sensor(0x0340, imgsensor.frame_length & 0xFFFF);
	}

	write_cmos_sensor(0x0202, shutter & 0xFFFF);
	pr_debug("Exit! shutter =%d, framelength =%d\n", shutter, imgsensor.frame_length);
}

static void set_shutter_frame_length(kal_uint16 shutter,
			kal_uint16 frame_length)
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
static void set_shutter(kal_uint16 shutter)
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

	if (gain < BASEGAIN || gain > 32 * BASEGAIN) {
		pr_err("Error gain setting");
		if (gain < BASEGAIN)
			gain = BASEGAIN;
		else if (gain > 32 * BASEGAIN)
			gain = 32 * BASEGAIN;
	}

	reg_gain = gain2reg(gain);
	spin_lock(&imgsensor_drv_lock);
	imgsensor.gain = reg_gain;
	spin_unlock(&imgsensor_drv_lock);
	pr_debug("set_gain = %d , reg_gain = 0x%x\n ", gain, reg_gain);

	write_cmos_sensor(0x0204, (reg_gain&0xFFFF));
	return gain;
}

static void set_mirror_flip(kal_uint8 image_mirror)
{
	kal_uint8 itemp;

	LOG_INF("image_mirror = %d\n", image_mirror);
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
 *************************************************************************/
#if INDIRECT_BURST
static kal_uint16 addr_data_pair_burst_init_gm2[] = {
	0x6F12, //addr
	0x10B5, //data ...
	0x0022,
	0x3F49,
	0x4048,
	0x00F0,
	0xE3F8,
	0x3F4C,
	0x0022,
	0x3F49,
	0x6060,
	0x3F48,
	0x00F0,
	0xDCF8,
	0x2060,
	0x0122,
	0x3E49,
	0x3E48,
	0x00F0,
	0x5DFA,
	0x0122,
	0x3D49,
	0x3E48,
	0x00F0,
	0x58FA,
	0x0022,
	0x3D49,
	0x3D48,
	0x00F0,
	0x53FA,
	0x0022,
	0x3C49,
	0x3D48,
	0x00F0,
	0xC7F8,
	0xE060,
	0x0022,
	0x3B49,
	0x3C48,
	0x00F0,
	0xC1F8,
	0xA060,
	0x0022,
	0x3A49,
	0x3B48,
	0x00F0,
	0xBBF8,
	0x2061,
	0x0022,
	0x3949,
	0x3A48,
	0x00F0,
	0xB5F8,
	0x6061,
	0x3949,
	0x0020,
	0x0246,
	0x0870,
	0x3849,
	0x3848,
	0x00F0,
	0x33FA,
	0x0022,
	0x3749,
	0x3848,
	0x00F0,
	0x2EFA,
	0x0022,
	0x3749,
	0x3748,
	0x00F0,
	0xA2F8,
	0xA061,
	0x0022,
	0x3649,
	0x3648,
	0x00F0,
	0x9CF8,
	0xE061,
	0x0022,
	0x3549,
	0x3548,
	0x00F0,
	0x96F8,
	0x2062,
	0x0022,
	0x3449,
	0x3448,
	0x00F0,
	0x90F8,
	0xA062,
	0x0022,
	0x3349,
	0x3348,
	0x00F0,
	0x8AF8,
	0x6062,
	0x0022,
	0x3249,
	0x3248,
	0x00F0,
	0x84F8,
	0xE062,
	0x0022,
	0x3149,
	0x3148,
	0x00F0,
	0x7EF8,
	0x2063,
	0x0022,
	0x3049,
	0x3048,
	0x00F0,
	0x78F8,
	0x6063,
	0x0022,
	0x2F49,
	0x2F48,
	0x00F0,
	0x72F8,
	0xA063,
	0x0022,
	0x2E49,
	0x2E48,
	0x00F0,
	0x6CF8,
	0xE063,
	0xBDE8,
	0x1040,
	0x00F0,
	0x5EB8,
	0x2000,
	0x5433,
	0x0000,
	0xEDF5,
	0x2000,
	0x6B80,
	0x2000,
	0x53B7,
	0x0000,
	0xE83D,
	0x2000,
	0x5523,
	0x0000,
	0xE19F,
	0x2000,
	0x557F,
	0x0000,
	0xDA7F,
	0x2000,
	0x649F,
	0x0000,
	0x9CC3,
	0x2000,
	0x5865,
	0x0000,
	0xF9F1,
	0x2000,
	0x55B9,
	0x0001,
	0x332D,
	0x2000,
	0x5939,
	0x0000,
	0x87B5,
	0x2000,
	0x599F,
	0x0000,
	0x689B,
	0x2000,
	0x5F90,
	0x2000,
	0x59C9,
	0x0000,
	0x5017,
	0x2000,
	0x5A63,
	0x0000,
	0x4F91,
	0x2000,
	0x5A17,
	0x0000,
	0x52BD,
	0x2000,
	0x5B5D,
	0x0001,
	0x03C1,
	0x2000,
	0x5C91,
	0x0000,
	0xF635,
	0x2000,
	0x5D91,
	0x0000,
	0x212D,
	0x2000,
	0x5D23,
	0x0000,
	0xF889,
	0x2000,
	0x63DB,
	0x0001,
	0x3AC7,
	0x2000,
	0x5AA9,
	0x0001,
	0x41D5,
	0x2000,
	0x62A7,
	0x0000,
	0xD15F,
	0x2000,
	0x5AFB,
	0x0000,
	0xFE7F,
	0x2000,
	0x5B2D,
	0x0001,
	0x4927,
	0x0000,
	0x0000,
	0x0000,
	0x0000,
	0x0000,
	0x0000,
	0xB849,
	0xB748,
	0xB84A,
	0xC1F8,
	0xDC06,
	0x101A,
	0xA1F8,
	0xE006,
	0x7047,
	0xB64B,
	0x1847,
	0x2DE9,
	0xFC5F,
	0x8346,
	0xB448,
	0x8A46,
	0xDDE9,
	0x0C56,
	0x416B,
	0x9146,
	0x0C0C,
	0x8FB2,
	0x9846,
	0x0022,
	0x3946,
	0x2046,
	0xFFF7,
	0xC2FD,
	0xCDE9,
	0x0056,
	0x4346,
	0x4A46,
	0x5146,
	0x5846,
	0x00F0,
	0x71F9,
	0x8046,
	0x0122,
	0x3946,
	0x2046,
	0xFFF7,
	0xB4FD,
	0xA74E,
	0x96F8,
	0xE802,
	0x18B9,
	0x96F8,
	0xC002,
	0x0028,
	0x6FD0,
	0x96F8,
	0x0E01,
	0x0028,
	0x6BD1,
	0x0025,
	0x4FF4,
	0x1A47,
	0x2C46,
	0x3A46,
	0x1821,
	0x9F48,
	0x00F0,
	0x5BF9,
	0x9E48,
	0x3A46,
	0x7821,
	0x4030,
	0x00F0,
	0x5AF9,
	0x0020,
	0x06EB,
	0x8001,
	0x401C,
	0xD1F8,
	0x6C12,
	0x1028,
	0x0D44,
	0xF7DB,
	0x0020,
	0x06EB,
	0x4001,
	0x401C,
	0xB1F8,
	0xAC12,
	0x0428,
	0x0C44,
	0xF7DB,
	0x04B9,
	0x0124,
	0xA000,
	0x95FB,
	0xF0F3,
	0x9048,
	0x914C,
	0x018B,
	0x4A05,
	0x00D5,
	0x2143,
	0x428B,
	0x5505,
	0x00D5,
	0x2243,
	0x1944,
	0x1A44,
	0x96F8,
	0x4631,
	0xC1F3,
	0x0A01,
	0xC2F3,
	0x0A02,
	0xC3B9,
	0x894B,
	0x1980,
	0x9B1C,
	0x1A80,
	0x874C,
	0x838B,
	0x241D,
	0x2380,
	0xA41C,
	0xC38B,
	0x2380,
	0xA41C,
	0x038C,
	0x2380,
	0xA41C,
	0x438C,
	0x2380,
	0xA41C,
	0x90F8,
	0x2430,
	0x2380,
	0xA41C,
	0x90F8,
	0x2530,
	0x2380,
	0x96F8,
	0x5031,
	0xB3B9,
	0x7C4B,
	0x1980,
	0x991C,
	0x0A80,
	0x1A1D,
	0x30F8,
	0x1C1F,
	0x1180,
	0x921C,
	0x4188,
	0x1180,
	0x921C,
	0x8188,
	0x1180,
	0x921C,
	0xC188,
	0x1180,
	0x921C,
	0x017A,
	0x1180,
	0x911C,
	0x407A,
	0x0880,
	0x4046,
	0xBDE8,
	0xFC9F,
	0x38B5,
	0x654C,
	0x0020,
	0x0090,
	0x94F8,
	0xD822,
	0x94F8,
	0xD912,
	0x94F8,
	0xDB02,
	0x6B46,
	0x04F5,
	0x3674,
	0x00F0,
	0xF0F8,
	0xE178,
	0x0844,
	0xA178,
	0xA4F5,
	0x3674,
	0x0D18,
	0x00F0,
	0xEDF8,
	0x10B1,
	0x6448,
	0x0078,
	0x00B1,
	0x0120,
	0x5C49,
	0x81F8,
	0xDC00,
	0x94F8,
	0x7725,
	0x02B1,
	0x00B1,
	0x0120,
	0x81F8,
	0x0E01,
	0x5E49,
	0x0978,
	0x0229,
	0x11D1,
	0x80B9,
	0xB4F8,
	0x0203,
	0xB4F8,
	0xCA20,
	0x411E,
	0x0A44,
	0xB2FB,
	0xF0F2,
	0x84F8,
	0x2727,
	0xB4F8,
	0xCC20,
	0x1144,
	0xB1FB,
	0xF0F0,
	0x84F8,
	0x2807,
	0xB4F8,
	0xBE00,
	0x94F8,
	0xDA12,
	0x401A,
	0x0028,
	0x00DC,
	0x0020,
	0x84F8,
	0xDC02,
	0x00F0,
	0xC1F8,
	0x94F8,
	0xDC12,
	0x0844,
	0x4C49,
	0x91F8,
	0x9612,
	0x0844,
	0x4B49,
	0xC0B2,
	0x84F8,
	0xDC02,
	0x91F8,
	0x7320,
	0x42B1,
	0x91F8,
	0x7110,
	0x0429,
	0x04D1,
	0x00F0,
	0x0101,
	0x0844,
	0x84F8,
	0xDC02,
	0xC0B2,
	0x2844,
	0x38BD,
	0x10B5,
	0x384C,
	0x3C4A,
	0x94F8,
	0x0E01,
	0x94F8,
	0x4611,
	0x0143,
	0x4E3A,
	0x1180,
	0x94F8,
	0x5011,
	0x384A,
	0x0143,
	0x4E3A,
	0x1180,
	0x04F5,
	0x8774,
	0x0028,
	0x53D1,
	0x2046,
	0x00F0,
	0x95F8,
	0x2046,
	0x00F0,
	0x97F8,
	0x364A,
	0x608A,
	0x2D4B,
	0x9188,
	0x401A,
	0x80B2,
	0x6082,
	0x998D,
	0x09B1,
	0x0846,
	0x04E0,
	0x94F8,
	0x3610,
	0x0229,
	0x00D1,
	0x4008,
	0xE085,
	0x188E,
	0x58B9,
	0x94F8,
	0x3600,
	0x0228,
	0x03D0,
	0x0128,
	0x608A,
	0x03D0,
	0x03E0,
	0x608A,
	0x8008,
	0x00E0,
	0x4008,
	0x2086,
	0x6088,
	0x1188,
	0x0844,
	0x6080,
	0xE088,
	0x1188,
	0x401A,
	0xE080,
	0xA08A,
	0xD188,
	0x401A,
	0x80B2,
	0xA082,
	0x598D,
	0x09B1,
	0x0846,
	0x04E0,
	0x94F8,
	0x3610,
	0x0229,
	0x00D1,
	0x4000,
	0x6086,
	0xD88D,
	0x58B9,
	0x94F8,
	0x3600,
	0x0228,
	0x03D0,
	0x0128,
	0xA08A,
	0x03D0,
	0x03E0,
	0xA08A,
	0x8000,
	0x00E0,
	0x4000,
	0xA086,
	0xA088,
	0x5188,
	0x0844,
	0xA080,
	0x2089,
	0x5188,
	0x401A,
	0x2081,
	0x10BD,
	0x2000,
	0x6624,
	0x2000,
	0x3CC0,
	0x2000,
	0x6C00,
	0x0001,
	0x3051,
	0x2000,
	0x6B80,
	0x2000,
	0x4AC0,
	0x2000,
	0x4D2C,
	0x2000,
	0x34C0,
	0xFFFF,
	0xF800,
	0x4000,
	0xAE4E,
	0x4000,
	0xAF4E,
	0x2000,
	0x33B0,
	0x2000,
	0x2A80,
	0x2000,
	0x0E00,
	0x2000,
	0x4680,
	0x2000,
	0x6C00,
	0x43F2,
	0x510C,
	0xC0F2,
	0x010C,
	0x6047,
	0x4DF2,
	0x5F1C,
	0xC0F2,
	0x000C,
	0x6047,
	0x4BF6,
	0x1D2C,
	0xC0F2,
	0x000C,
	0x6047,
	0x4BF6,
	0x572C,
	0xC0F2,
	0x000C,
	0x6047,
	0x4FF6,
	0x335C,
	0xC0F2,
	0x000C,
	0x6047,
	0x47F2,
	0x0B0C,
	0xC0F2,
	0x000C,
	0x6047,
	0x43F6,
	0x8F2C,
	0xC0F2,
	0x010C,
	0x6047,
	0x49F6,
	0x911C,
	0xC0F2,
	0x000C,
	0x6047,
	0x49F6,
	0x613C,
	0xC0F2,
	0x000C,
	0x6047,
	0x0000,
	0x0000,
	0x0000,
	0x08D2,
	0x018F,
	0x0001,
	0xFFFF,
};

static kal_uint16 addr_data_pair_other_init_gm2[] = {
	0x602A, 0x6C00,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0155,
	0x602A, 0x1EA6,
	0x6F12, 0x4C09,
	0xF44C, 0x0A0C,
	0x6028, 0x2000,
	0x602A, 0x11E6,
	0x6F12, 0x000D,
	0x6F12, 0x0009,
	0x6F12, 0x0008,
	0x602A, 0x11EE,
	0x6F12, 0x0000,
	0x6F12, 0x000F,
	0x602A, 0x122E,
	0x6F12, 0x0012,
	0x6F12, 0x000C,
	0x6F12, 0x000B,
	0x602A, 0x12D6,
	0x6F12, 0xFFF9,
	0x602A, 0x12E6,
	0x6F12, 0x000F,
	0x602A, 0x146E,
	0x6F12, 0x003B,
	0x6F12, 0x003C,
	0x6F12, 0x003D,
	0x602A, 0x1EA4,
	0x6F12, 0x0018,
	0x602A, 0x0E38,
	0x6F12, 0x0040,
	0x602A, 0x1E92,
	0x6F12, 0x0907,
	0x6F12, 0x070B,
	0x602A, 0x1E9E,
	0x6F12, 0x0303,
	0x6F12, 0x0305,
	0x602A, 0x1E9A,
	0x6F12, 0x0B0B,
	0x6F12, 0x0715,
	0x602A, 0x1E96,
	0x6F12, 0x0F0C,
	0x6F12, 0x0C04,
	0x602A, 0x1EAE,
	0x6F12, 0x3D68,
	0x602A, 0x2440,
	0x6F12, 0x0080,
	0x602A, 0x243E,
	0x6F12, 0x006F,
	0x602A, 0x243C,
	0x6F12, 0x44C2,
	0xF412, 0x0000,
	0xF45A, 0x0FFE,
	0x6028, 0x2000,
	0x602A, 0x10B0,
	0x6F12, 0x0100,
	0x602A, 0x10B4,
	0x6F12, 0x003F,
	0x602A, 0x10BA,
	0x6F12, 0x0040,
	0x602A, 0x10C4,
	0x6F12, 0xF480,
	0x602A, 0x3B20,
	0x6F12, 0x0100,
	0x602A, 0x3B26,
	0x6F12, 0x0008,
	0x602A, 0x3B22,
	0x6F12, 0x0080,
	0x9C14, 0x0000,
	0x9C1A, 0x0000,
	0x9C06, 0x0303,
	0x9C1C, 0x0020,
	0x9C1E, 0x0400,
	0x6028, 0x2000,
	0x602A, 0x239A,
	0x6F12, 0x00F0,
	0x602A, 0x2398,
	0x6F12, 0x00F0,
	0x602A, 0x2396,
	0x6F12, 0x00F0,
	0x602A, 0x2394,
	0x6F12, 0x00F0,
	0x602A, 0x2392,
	0x6F12, 0x00F0,
	0x602A, 0x2386,
	0x6F12, 0x00F0,
	0x602A, 0x2384,
	0x6F12, 0x00F0,
	0x602A, 0x2382,
	0x6F12, 0x00F0,
	0x602A, 0x2380,
	0x6F12, 0x00F0,
	0x602A, 0x237E,
	0x6F12, 0x00F0,
	0x9C24, 0x0100,
	0x6028, 0x2000,
	0x602A, 0x1EB2,
	0x6F12, 0x6A2D,
	0x602A, 0x117A,
	0x6F12, 0x0088,
	0x6F12, 0x0098,
	0x602A, 0x29F0,
	0x6F12, 0x0000,
	0x602A, 0x1076,
	0x6F12, 0x0200,
	0x0136, 0x1800,
	0x0304, 0x0006,
	0x030C, 0x0000,
	0x0302, 0x0001,
	0x030E, 0x0003,
	0x0312, 0x0000,
	0x030A, 0x0001,
	0x0308, 0x0008,
	0x0202, 0x0020,
	0x6028, 0x2000,
	0x602A, 0x1170,
	0x6F12, 0x0005,
	0x602A, 0x33FA,
	0x6F12, 0x0100,
	0x602A, 0x33F8,
	0x6F12, 0x0000,
	0x0B06, 0x0101,
	0x6028, 0x2000,
	0x602A, 0x1094,
	0x6F12, 0x0002,
	0x602A, 0x3020,
	0x6F12, 0x0040,
	0x0FEA, 0x0500,
	0x6028, 0x2000,
	0x602A, 0x1E80,
	0x6F12, 0x0100,
	0x602A, 0x2516,
	0x6F12, 0x01C0,
	0x6F12, 0x01C2,
	0x6F12, 0x02C8,
	0x6F12, 0x02CA,
	0x6F12, 0x02D0,
	0x6F12, 0x02D2,
	0x6F12, 0x01D8,
	0x6F12, 0x01DA,
	0x6F12, 0x01E0,
	0x6F12, 0x01E2,
	0x6F12, 0x02E8,
	0x6F12, 0x02EA,
	0x6F12, 0x02F0,
	0x6F12, 0x02F2,
	0x6F12, 0x01F8,
	0x6F12, 0x01FA,
	0x6F12, 0x01C0,
	0x6F12, 0x01C2,
	0x6F12, 0x02C8,
	0x6F12, 0x02CA,
	0x6F12, 0x02D0,
	0x6F12, 0x02D2,
	0x6F12, 0x01D8,
	0x6F12, 0x01DA,
	0x6F12, 0x01E0,
	0x6F12, 0x01E2,
	0x6F12, 0x02E8,
	0x6F12, 0x02EA,
	0x6F12, 0x02F0,
	0x6F12, 0x02F2,
	0x6F12, 0x01F8,
	0x6F12, 0x01FA,
	0x602A, 0x2596,
	0x6F12, 0x01C0,
	0x6F12, 0x01C2,
	0x6F12, 0x01C8,
	0x6F12, 0x01CA,
	0x6F12, 0x01D0,
	0x6F12, 0x01D2,
	0x6F12, 0x01D8,
	0x6F12, 0x01DA,
	0x6F12, 0x01E0,
	0x6F12, 0x01E2,
	0x6F12, 0x01E8,
	0x6F12, 0x01EA,
	0x6F12, 0x01F0,
	0x6F12, 0x01F2,
	0x6F12, 0x01F8,
	0x6F12, 0x01FA,
	0x6F12, 0x01C0,
	0x6F12, 0x01C2,
	0x6F12, 0x01C8,
	0x6F12, 0x01CA,
	0x6F12, 0x01D0,
	0x6F12, 0x01D2,
	0x6F12, 0x01D8,
	0x6F12, 0x01DA,
	0x6F12, 0x01E0,
	0x6F12, 0x01E2,
	0x6F12, 0x01E8,
	0x6F12, 0x01EA,
	0x6F12, 0x01F0,
	0x6F12, 0x01F2,
	0x6F12, 0x01F8,
	0x6F12, 0x01FA,
	0x602A, 0x0E7C,
	0x6F12, 0x0400,
	0x0118, 0x0104,
	0x010C, 0x0000,
	0x011A, 0x0401,
	0x6028, 0x2000,
	0x602A, 0x1070,
	0x6F12, 0x0004,
	0x602A, 0x0EFC,
	0x6F12, 0x0100,
	0x602A, 0x0ED4,
	0x6F12, 0x0003,
	0x602A, 0x0F00,
	0x6F12, 0x0100,
	0x6F12, 0x0800,
	0x602A, 0x2634,
	0x6F12, 0x0500,
	0x602A, 0x34E4,
	0x6F12, 0x0000,
	0x602A, 0x34DC,
	0x6F12, 0x07C0,
	0x6F12, 0x07C0,
	0x602A, 0x34D0,
	0x6F12, 0x0100,
	0x602A, 0x39A8,
	0x6F12, 0x0001,
	0x602A, 0x39B0,
	0x6F12, 0x001E,
	0x6F12, 0x001E,
	0x6F12, 0x001E,
	0x6F12, 0x001E,
	0x6F12, 0x0005,
	0x602A, 0x39C2,
	0x6F12, 0x0001,
	0x602A, 0x3992,
	0x6F12, 0x0002,
	0x602A, 0x39CC,
	0x6F12, 0x0001,
	0x602A, 0x37EC,
	0x6F12, 0x0023,
	0x6F12, 0x0023,
	0x602A, 0x37FA,
	0x6F12, 0x0000,
	0x602A, 0x3806,
	0x6F12, 0x0023,
	0x6F12, 0x0023,
	0x602A, 0x3814,
	0x6F12, 0x0000,
	0x602A, 0x39DA,
	0x6F12, 0x104D,
	0x6F12, 0x0000,
	0x6F12, 0x0B00,
	0x6F12, 0x0B80,
	0x6F12, 0x2274,
	0x6F12, 0x0000,
	0x6F12, 0x0812,
	0x6F12, 0x1F2D,
	0x6F12, 0x3944,
	0x6F12, 0x4E56,
	0x6F12, 0x010A,
	0x6F12, 0x1521,
	0x6F12, 0x2E3E,
	0x6F12, 0x4C57,
	0x6F12, 0x626B,
	0x6F12, 0x0E18,
	0x6F12, 0x2330,
	0x6F12, 0x4050,
	0x6F12, 0x606D,
	0x6F12, 0x7882,
	0x6F12, 0x1E2A,
	0x6F12, 0x3645,
	0x6F12, 0x5669,
	0x6F12, 0x7A89,
	0x6F12, 0x96A1,
	0x6F12, 0x313E,
	0x6F12, 0x4C5D,
	0x6F12, 0x7085,
	0x6F12, 0x98A8,
	0x6F12, 0xB7C4,
	0x6F12, 0x4250,
	0x6F12, 0x5F71,
	0x6F12, 0x869D,
	0x6F12, 0xB2C4,
	0x6F12, 0xD4E2,
	0x6F12, 0x4F5E,
	0x6F12, 0x6F82,
	0x6F12, 0x98B1,
	0x6F12, 0xC7DA,
	0x6F12, 0xEBFA,
	0x6F12, 0x5A6A,
	0x6F12, 0x7B90,
	0x6F12, 0xA7C1,
	0x6F12, 0xD9ED,
	0x6F12, 0xFFFF,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6028, 0x2000,
	0x602A, 0x1F1E,
	0x6F12, 0x00F0,
	0x6F12, 0x00F0,
	0x6F12, 0x00F0,
	0x6F12, 0x00F0,
	0x6F12, 0x00F0,
	0x602A, 0x1F32,
	0x6F12, 0x00F0,
	0x6F12, 0x00F0,
	0x6F12, 0x00F0,
	0x6F12, 0x00F0,
	0x6F12, 0x00F0,
	0x602A, 0x214E,
	0x6F12, 0x00F0,
	0x6F12, 0x00F0,
	0x6F12, 0x00F0,
	0x6F12, 0x00F0,
	0x6F12, 0x00F0,
	0x602A, 0x2162,
	0x6F12, 0x00F0,
	0x6F12, 0x00F0,
	0x6F12, 0x00F0,
	0x6F12, 0x00F0,
	0x6F12, 0x00F0,
	0x0116, 0x2B00,
};
#else
static kal_uint16 addr_data_pair_init_gm2[] = {
	0x6214, 0xF9F3,
	0x6028, 0x2000,
	0x602A, 0x60D0,
	0x6F12, 0x10B5,
	0x6F12, 0x0022,
	0x6F12, 0x3F49,
	0x6F12, 0x4048,
	0x6F12, 0x00F0,
	0x6F12, 0xE3F8,
	0x6F12, 0x3F4C,
	0x6F12, 0x0022,
	0x6F12, 0x3F49,
	0x6F12, 0x6060,
	0x6F12, 0x3F48,
	0x6F12, 0x00F0,
	0x6F12, 0xDCF8,
	0x6F12, 0x2060,
	0x6F12, 0x0122,
	0x6F12, 0x3E49,
	0x6F12, 0x3E48,
	0x6F12, 0x00F0,
	0x6F12, 0x5DFA,
	0x6F12, 0x0122,
	0x6F12, 0x3D49,
	0x6F12, 0x3E48,
	0x6F12, 0x00F0,
	0x6F12, 0x58FA,
	0x6F12, 0x0022,
	0x6F12, 0x3D49,
	0x6F12, 0x3D48,
	0x6F12, 0x00F0,
	0x6F12, 0x53FA,
	0x6F12, 0x0022,
	0x6F12, 0x3C49,
	0x6F12, 0x3D48,
	0x6F12, 0x00F0,
	0x6F12, 0xC7F8,
	0x6F12, 0xE060,
	0x6F12, 0x0022,
	0x6F12, 0x3B49,
	0x6F12, 0x3C48,
	0x6F12, 0x00F0,
	0x6F12, 0xC1F8,
	0x6F12, 0xA060,
	0x6F12, 0x0022,
	0x6F12, 0x3A49,
	0x6F12, 0x3B48,
	0x6F12, 0x00F0,
	0x6F12, 0xBBF8,
	0x6F12, 0x2061,
	0x6F12, 0x0022,
	0x6F12, 0x3949,
	0x6F12, 0x3A48,
	0x6F12, 0x00F0,
	0x6F12, 0xB5F8,
	0x6F12, 0x6061,
	0x6F12, 0x3949,
	0x6F12, 0x0020,
	0x6F12, 0x0246,
	0x6F12, 0x0870,
	0x6F12, 0x3849,
	0x6F12, 0x3848,
	0x6F12, 0x00F0,
	0x6F12, 0x33FA,
	0x6F12, 0x0022,
	0x6F12, 0x3749,
	0x6F12, 0x3848,
	0x6F12, 0x00F0,
	0x6F12, 0x2EFA,
	0x6F12, 0x0022,
	0x6F12, 0x3749,
	0x6F12, 0x3748,
	0x6F12, 0x00F0,
	0x6F12, 0xA2F8,
	0x6F12, 0xA061,
	0x6F12, 0x0022,
	0x6F12, 0x3649,
	0x6F12, 0x3648,
	0x6F12, 0x00F0,
	0x6F12, 0x9CF8,
	0x6F12, 0xE061,
	0x6F12, 0x0022,
	0x6F12, 0x3549,
	0x6F12, 0x3548,
	0x6F12, 0x00F0,
	0x6F12, 0x96F8,
	0x6F12, 0x2062,
	0x6F12, 0x0022,
	0x6F12, 0x3449,
	0x6F12, 0x3448,
	0x6F12, 0x00F0,
	0x6F12, 0x90F8,
	0x6F12, 0xA062,
	0x6F12, 0x0022,
	0x6F12, 0x3349,
	0x6F12, 0x3348,
	0x6F12, 0x00F0,
	0x6F12, 0x8AF8,
	0x6F12, 0x6062,
	0x6F12, 0x0022,
	0x6F12, 0x3249,
	0x6F12, 0x3248,
	0x6F12, 0x00F0,
	0x6F12, 0x84F8,
	0x6F12, 0xE062,
	0x6F12, 0x0022,
	0x6F12, 0x3149,
	0x6F12, 0x3148,
	0x6F12, 0x00F0,
	0x6F12, 0x7EF8,
	0x6F12, 0x2063,
	0x6F12, 0x0022,
	0x6F12, 0x3049,
	0x6F12, 0x3048,
	0x6F12, 0x00F0,
	0x6F12, 0x78F8,
	0x6F12, 0x6063,
	0x6F12, 0x0022,
	0x6F12, 0x2F49,
	0x6F12, 0x2F48,
	0x6F12, 0x00F0,
	0x6F12, 0x72F8,
	0x6F12, 0xA063,
	0x6F12, 0x0022,
	0x6F12, 0x2E49,
	0x6F12, 0x2E48,
	0x6F12, 0x00F0,
	0x6F12, 0x6CF8,
	0x6F12, 0xE063,
	0x6F12, 0xBDE8,
	0x6F12, 0x1040,
	0x6F12, 0x00F0,
	0x6F12, 0x5EB8,
	0x6F12, 0x2000,
	0x6F12, 0x5433,
	0x6F12, 0x0000,
	0x6F12, 0xEDF5,
	0x6F12, 0x2000,
	0x6F12, 0x6B80,
	0x6F12, 0x2000,
	0x6F12, 0x53B7,
	0x6F12, 0x0000,
	0x6F12, 0xE83D,
	0x6F12, 0x2000,
	0x6F12, 0x5523,
	0x6F12, 0x0000,
	0x6F12, 0xE19F,
	0x6F12, 0x2000,
	0x6F12, 0x557F,
	0x6F12, 0x0000,
	0x6F12, 0xDA7F,
	0x6F12, 0x2000,
	0x6F12, 0x649F,
	0x6F12, 0x0000,
	0x6F12, 0x9CC3,
	0x6F12, 0x2000,
	0x6F12, 0x5865,
	0x6F12, 0x0000,
	0x6F12, 0xF9F1,
	0x6F12, 0x2000,
	0x6F12, 0x55B9,
	0x6F12, 0x0001,
	0x6F12, 0x332D,
	0x6F12, 0x2000,
	0x6F12, 0x5939,
	0x6F12, 0x0000,
	0x6F12, 0x87B5,
	0x6F12, 0x2000,
	0x6F12, 0x599F,
	0x6F12, 0x0000,
	0x6F12, 0x689B,
	0x6F12, 0x2000,
	0x6F12, 0x5F90,
	0x6F12, 0x2000,
	0x6F12, 0x59C9,
	0x6F12, 0x0000,
	0x6F12, 0x5017,
	0x6F12, 0x2000,
	0x6F12, 0x5A63,
	0x6F12, 0x0000,
	0x6F12, 0x4F91,
	0x6F12, 0x2000,
	0x6F12, 0x5A17,
	0x6F12, 0x0000,
	0x6F12, 0x52BD,
	0x6F12, 0x2000,
	0x6F12, 0x5B5D,
	0x6F12, 0x0001,
	0x6F12, 0x03C1,
	0x6F12, 0x2000,
	0x6F12, 0x5C91,
	0x6F12, 0x0000,
	0x6F12, 0xF635,
	0x6F12, 0x2000,
	0x6F12, 0x5D91,
	0x6F12, 0x0000,
	0x6F12, 0x212D,
	0x6F12, 0x2000,
	0x6F12, 0x5D23,
	0x6F12, 0x0000,
	0x6F12, 0xF889,
	0x6F12, 0x2000,
	0x6F12, 0x63DB,
	0x6F12, 0x0001,
	0x6F12, 0x3AC7,
	0x6F12, 0x2000,
	0x6F12, 0x5AA9,
	0x6F12, 0x0001,
	0x6F12, 0x41D5,
	0x6F12, 0x2000,
	0x6F12, 0x62A7,
	0x6F12, 0x0000,
	0x6F12, 0xD15F,
	0x6F12, 0x2000,
	0x6F12, 0x5AFB,
	0x6F12, 0x0000,
	0x6F12, 0xFE7F,
	0x6F12, 0x2000,
	0x6F12, 0x5B2D,
	0x6F12, 0x0001,
	0x6F12, 0x4927,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0xB849,
	0x6F12, 0xB748,
	0x6F12, 0xB84A,
	0x6F12, 0xC1F8,
	0x6F12, 0xDC06,
	0x6F12, 0x101A,
	0x6F12, 0xA1F8,
	0x6F12, 0xE006,
	0x6F12, 0x7047,
	0x6F12, 0xB64B,
	0x6F12, 0x1847,
	0x6F12, 0x2DE9,
	0x6F12, 0xFC5F,
	0x6F12, 0x8346,
	0x6F12, 0xB448,
	0x6F12, 0x8A46,
	0x6F12, 0xDDE9,
	0x6F12, 0x0C56,
	0x6F12, 0x416B,
	0x6F12, 0x9146,
	0x6F12, 0x0C0C,
	0x6F12, 0x8FB2,
	0x6F12, 0x9846,
	0x6F12, 0x0022,
	0x6F12, 0x3946,
	0x6F12, 0x2046,
	0x6F12, 0xFFF7,
	0x6F12, 0xC2FD,
	0x6F12, 0xCDE9,
	0x6F12, 0x0056,
	0x6F12, 0x4346,
	0x6F12, 0x4A46,
	0x6F12, 0x5146,
	0x6F12, 0x5846,
	0x6F12, 0x00F0,
	0x6F12, 0x71F9,
	0x6F12, 0x8046,
	0x6F12, 0x0122,
	0x6F12, 0x3946,
	0x6F12, 0x2046,
	0x6F12, 0xFFF7,
	0x6F12, 0xB4FD,
	0x6F12, 0xA74E,
	0x6F12, 0x96F8,
	0x6F12, 0xE802,
	0x6F12, 0x18B9,
	0x6F12, 0x96F8,
	0x6F12, 0xC002,
	0x6F12, 0x0028,
	0x6F12, 0x6FD0,
	0x6F12, 0x96F8,
	0x6F12, 0x0E01,
	0x6F12, 0x0028,
	0x6F12, 0x6BD1,
	0x6F12, 0x0025,
	0x6F12, 0x4FF4,
	0x6F12, 0x1A47,
	0x6F12, 0x2C46,
	0x6F12, 0x3A46,
	0x6F12, 0x1821,
	0x6F12, 0x9F48,
	0x6F12, 0x00F0,
	0x6F12, 0x5BF9,
	0x6F12, 0x9E48,
	0x6F12, 0x3A46,
	0x6F12, 0x7821,
	0x6F12, 0x4030,
	0x6F12, 0x00F0,
	0x6F12, 0x5AF9,
	0x6F12, 0x0020,
	0x6F12, 0x06EB,
	0x6F12, 0x8001,
	0x6F12, 0x401C,
	0x6F12, 0xD1F8,
	0x6F12, 0x6C12,
	0x6F12, 0x1028,
	0x6F12, 0x0D44,
	0x6F12, 0xF7DB,
	0x6F12, 0x0020,
	0x6F12, 0x06EB,
	0x6F12, 0x4001,
	0x6F12, 0x401C,
	0x6F12, 0xB1F8,
	0x6F12, 0xAC12,
	0x6F12, 0x0428,
	0x6F12, 0x0C44,
	0x6F12, 0xF7DB,
	0x6F12, 0x04B9,
	0x6F12, 0x0124,
	0x6F12, 0xA000,
	0x6F12, 0x95FB,
	0x6F12, 0xF0F3,
	0x6F12, 0x9048,
	0x6F12, 0x914C,
	0x6F12, 0x018B,
	0x6F12, 0x4A05,
	0x6F12, 0x00D5,
	0x6F12, 0x2143,
	0x6F12, 0x428B,
	0x6F12, 0x5505,
	0x6F12, 0x00D5,
	0x6F12, 0x2243,
	0x6F12, 0x1944,
	0x6F12, 0x1A44,
	0x6F12, 0x96F8,
	0x6F12, 0x4631,
	0x6F12, 0xC1F3,
	0x6F12, 0x0A01,
	0x6F12, 0xC2F3,
	0x6F12, 0x0A02,
	0x6F12, 0xC3B9,
	0x6F12, 0x894B,
	0x6F12, 0x1980,
	0x6F12, 0x9B1C,
	0x6F12, 0x1A80,
	0x6F12, 0x874C,
	0x6F12, 0x838B,
	0x6F12, 0x241D,
	0x6F12, 0x2380,
	0x6F12, 0xA41C,
	0x6F12, 0xC38B,
	0x6F12, 0x2380,
	0x6F12, 0xA41C,
	0x6F12, 0x038C,
	0x6F12, 0x2380,
	0x6F12, 0xA41C,
	0x6F12, 0x438C,
	0x6F12, 0x2380,
	0x6F12, 0xA41C,
	0x6F12, 0x90F8,
	0x6F12, 0x2430,
	0x6F12, 0x2380,
	0x6F12, 0xA41C,
	0x6F12, 0x90F8,
	0x6F12, 0x2530,
	0x6F12, 0x2380,
	0x6F12, 0x96F8,
	0x6F12, 0x5031,
	0x6F12, 0xB3B9,
	0x6F12, 0x7C4B,
	0x6F12, 0x1980,
	0x6F12, 0x991C,
	0x6F12, 0x0A80,
	0x6F12, 0x1A1D,
	0x6F12, 0x30F8,
	0x6F12, 0x1C1F,
	0x6F12, 0x1180,
	0x6F12, 0x921C,
	0x6F12, 0x4188,
	0x6F12, 0x1180,
	0x6F12, 0x921C,
	0x6F12, 0x8188,
	0x6F12, 0x1180,
	0x6F12, 0x921C,
	0x6F12, 0xC188,
	0x6F12, 0x1180,
	0x6F12, 0x921C,
	0x6F12, 0x017A,
	0x6F12, 0x1180,
	0x6F12, 0x911C,
	0x6F12, 0x407A,
	0x6F12, 0x0880,
	0x6F12, 0x4046,
	0x6F12, 0xBDE8,
	0x6F12, 0xFC9F,
	0x6F12, 0x38B5,
	0x6F12, 0x654C,
	0x6F12, 0x0020,
	0x6F12, 0x0090,
	0x6F12, 0x94F8,
	0x6F12, 0xD822,
	0x6F12, 0x94F8,
	0x6F12, 0xD912,
	0x6F12, 0x94F8,
	0x6F12, 0xDB02,
	0x6F12, 0x6B46,
	0x6F12, 0x04F5,
	0x6F12, 0x3674,
	0x6F12, 0x00F0,
	0x6F12, 0xF0F8,
	0x6F12, 0xE178,
	0x6F12, 0x0844,
	0x6F12, 0xA178,
	0x6F12, 0xA4F5,
	0x6F12, 0x3674,
	0x6F12, 0x0D18,
	0x6F12, 0x00F0,
	0x6F12, 0xEDF8,
	0x6F12, 0x10B1,
	0x6F12, 0x6448,
	0x6F12, 0x0078,
	0x6F12, 0x00B1,
	0x6F12, 0x0120,
	0x6F12, 0x5C49,
	0x6F12, 0x81F8,
	0x6F12, 0xDC00,
	0x6F12, 0x94F8,
	0x6F12, 0x7725,
	0x6F12, 0x02B1,
	0x6F12, 0x00B1,
	0x6F12, 0x0120,
	0x6F12, 0x81F8,
	0x6F12, 0x0E01,
	0x6F12, 0x5E49,
	0x6F12, 0x0978,
	0x6F12, 0x0229,
	0x6F12, 0x11D1,
	0x6F12, 0x80B9,
	0x6F12, 0xB4F8,
	0x6F12, 0x0203,
	0x6F12, 0xB4F8,
	0x6F12, 0xCA20,
	0x6F12, 0x411E,
	0x6F12, 0x0A44,
	0x6F12, 0xB2FB,
	0x6F12, 0xF0F2,
	0x6F12, 0x84F8,
	0x6F12, 0x2727,
	0x6F12, 0xB4F8,
	0x6F12, 0xCC20,
	0x6F12, 0x1144,
	0x6F12, 0xB1FB,
	0x6F12, 0xF0F0,
	0x6F12, 0x84F8,
	0x6F12, 0x2807,
	0x6F12, 0xB4F8,
	0x6F12, 0xBE00,
	0x6F12, 0x94F8,
	0x6F12, 0xDA12,
	0x6F12, 0x401A,
	0x6F12, 0x0028,
	0x6F12, 0x00DC,
	0x6F12, 0x0020,
	0x6F12, 0x84F8,
	0x6F12, 0xDC02,
	0x6F12, 0x00F0,
	0x6F12, 0xC1F8,
	0x6F12, 0x94F8,
	0x6F12, 0xDC12,
	0x6F12, 0x0844,
	0x6F12, 0x4C49,
	0x6F12, 0x91F8,
	0x6F12, 0x9612,
	0x6F12, 0x0844,
	0x6F12, 0x4B49,
	0x6F12, 0xC0B2,
	0x6F12, 0x84F8,
	0x6F12, 0xDC02,
	0x6F12, 0x91F8,
	0x6F12, 0x7320,
	0x6F12, 0x42B1,
	0x6F12, 0x91F8,
	0x6F12, 0x7110,
	0x6F12, 0x0429,
	0x6F12, 0x04D1,
	0x6F12, 0x00F0,
	0x6F12, 0x0101,
	0x6F12, 0x0844,
	0x6F12, 0x84F8,
	0x6F12, 0xDC02,
	0x6F12, 0xC0B2,
	0x6F12, 0x2844,
	0x6F12, 0x38BD,
	0x6F12, 0x10B5,
	0x6F12, 0x384C,
	0x6F12, 0x3C4A,
	0x6F12, 0x94F8,
	0x6F12, 0x0E01,
	0x6F12, 0x94F8,
	0x6F12, 0x4611,
	0x6F12, 0x0143,
	0x6F12, 0x4E3A,
	0x6F12, 0x1180,
	0x6F12, 0x94F8,
	0x6F12, 0x5011,
	0x6F12, 0x384A,
	0x6F12, 0x0143,
	0x6F12, 0x4E3A,
	0x6F12, 0x1180,
	0x6F12, 0x04F5,
	0x6F12, 0x8774,
	0x6F12, 0x0028,
	0x6F12, 0x53D1,
	0x6F12, 0x2046,
	0x6F12, 0x00F0,
	0x6F12, 0x95F8,
	0x6F12, 0x2046,
	0x6F12, 0x00F0,
	0x6F12, 0x97F8,
	0x6F12, 0x364A,
	0x6F12, 0x608A,
	0x6F12, 0x2D4B,
	0x6F12, 0x9188,
	0x6F12, 0x401A,
	0x6F12, 0x80B2,
	0x6F12, 0x6082,
	0x6F12, 0x998D,
	0x6F12, 0x09B1,
	0x6F12, 0x0846,
	0x6F12, 0x04E0,
	0x6F12, 0x94F8,
	0x6F12, 0x3610,
	0x6F12, 0x0229,
	0x6F12, 0x00D1,
	0x6F12, 0x4008,
	0x6F12, 0xE085,
	0x6F12, 0x188E,
	0x6F12, 0x58B9,
	0x6F12, 0x94F8,
	0x6F12, 0x3600,
	0x6F12, 0x0228,
	0x6F12, 0x03D0,
	0x6F12, 0x0128,
	0x6F12, 0x608A,
	0x6F12, 0x03D0,
	0x6F12, 0x03E0,
	0x6F12, 0x608A,
	0x6F12, 0x8008,
	0x6F12, 0x00E0,
	0x6F12, 0x4008,
	0x6F12, 0x2086,
	0x6F12, 0x6088,
	0x6F12, 0x1188,
	0x6F12, 0x0844,
	0x6F12, 0x6080,
	0x6F12, 0xE088,
	0x6F12, 0x1188,
	0x6F12, 0x401A,
	0x6F12, 0xE080,
	0x6F12, 0xA08A,
	0x6F12, 0xD188,
	0x6F12, 0x401A,
	0x6F12, 0x80B2,
	0x6F12, 0xA082,
	0x6F12, 0x598D,
	0x6F12, 0x09B1,
	0x6F12, 0x0846,
	0x6F12, 0x04E0,
	0x6F12, 0x94F8,
	0x6F12, 0x3610,
	0x6F12, 0x0229,
	0x6F12, 0x00D1,
	0x6F12, 0x4000,
	0x6F12, 0x6086,
	0x6F12, 0xD88D,
	0x6F12, 0x58B9,
	0x6F12, 0x94F8,
	0x6F12, 0x3600,
	0x6F12, 0x0228,
	0x6F12, 0x03D0,
	0x6F12, 0x0128,
	0x6F12, 0xA08A,
	0x6F12, 0x03D0,
	0x6F12, 0x03E0,
	0x6F12, 0xA08A,
	0x6F12, 0x8000,
	0x6F12, 0x00E0,
	0x6F12, 0x4000,
	0x6F12, 0xA086,
	0x6F12, 0xA088,
	0x6F12, 0x5188,
	0x6F12, 0x0844,
	0x6F12, 0xA080,
	0x6F12, 0x2089,
	0x6F12, 0x5188,
	0x6F12, 0x401A,
	0x6F12, 0x2081,
	0x6F12, 0x10BD,
	0x6F12, 0x2000,
	0x6F12, 0x6624,
	0x6F12, 0x2000,
	0x6F12, 0x3CC0,
	0x6F12, 0x2000,
	0x6F12, 0x6C00,
	0x6F12, 0x0001,
	0x6F12, 0x3051,
	0x6F12, 0x2000,
	0x6F12, 0x6B80,
	0x6F12, 0x2000,
	0x6F12, 0x4AC0,
	0x6F12, 0x2000,
	0x6F12, 0x4D2C,
	0x6F12, 0x2000,
	0x6F12, 0x34C0,
	0x6F12, 0xFFFF,
	0x6F12, 0xF800,
	0x6F12, 0x4000,
	0x6F12, 0xAE4E,
	0x6F12, 0x4000,
	0x6F12, 0xAF4E,
	0x6F12, 0x2000,
	0x6F12, 0x33B0,
	0x6F12, 0x2000,
	0x6F12, 0x2A80,
	0x6F12, 0x2000,
	0x6F12, 0x0E00,
	0x6F12, 0x2000,
	0x6F12, 0x4680,
	0x6F12, 0x2000,
	0x6F12, 0x6C00,
	0x6F12, 0x43F2,
	0x6F12, 0x510C,
	0x6F12, 0xC0F2,
	0x6F12, 0x010C,
	0x6F12, 0x6047,
	0x6F12, 0x4DF2,
	0x6F12, 0x5F1C,
	0x6F12, 0xC0F2,
	0x6F12, 0x000C,
	0x6F12, 0x6047,
	0x6F12, 0x4BF6,
	0x6F12, 0x1D2C,
	0x6F12, 0xC0F2,
	0x6F12, 0x000C,
	0x6F12, 0x6047,
	0x6F12, 0x4BF6,
	0x6F12, 0x572C,
	0x6F12, 0xC0F2,
	0x6F12, 0x000C,
	0x6F12, 0x6047,
	0x6F12, 0x4FF6,
	0x6F12, 0x335C,
	0x6F12, 0xC0F2,
	0x6F12, 0x000C,
	0x6F12, 0x6047,
	0x6F12, 0x47F2,
	0x6F12, 0x0B0C,
	0x6F12, 0xC0F2,
	0x6F12, 0x000C,
	0x6F12, 0x6047,
	0x6F12, 0x43F6,
	0x6F12, 0x8F2C,
	0x6F12, 0xC0F2,
	0x6F12, 0x010C,
	0x6F12, 0x6047,
	0x6F12, 0x49F6,
	0x6F12, 0x911C,
	0x6F12, 0xC0F2,
	0x6F12, 0x000C,
	0x6F12, 0x6047,
	0x6F12, 0x49F6,
	0x6F12, 0x613C,
	0x6F12, 0xC0F2,
	0x6F12, 0x000C,
	0x6F12, 0x6047,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x08D2,
	0x6F12, 0x018F,
	0x6F12, 0x0001,
	0x6F12, 0xFFFF,
	0x602A, 0x6C00,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0155,
	0x602A, 0x1EA6,
	0x6F12, 0x4C09,
	0xF44C, 0x0A0C,
	0x6028, 0x2000,
	0x602A, 0x11E6,
	0x6F12, 0x000D,
	0x6F12, 0x0009,
	0x6F12, 0x0008,
	0x602A, 0x11EE,
	0x6F12, 0x0000,
	0x6F12, 0x000F,
	0x602A, 0x122E,
	0x6F12, 0x0012,
	0x6F12, 0x000C,
	0x6F12, 0x000B,
	0x602A, 0x12D6,
	0x6F12, 0xFFF9,
	0x602A, 0x12E6,
	0x6F12, 0x000F,
	0x602A, 0x146E,
	0x6F12, 0x003B,
	0x6F12, 0x003C,
	0x6F12, 0x003D,
	0x602A, 0x1EA4,
	0x6F12, 0x0018,
	0x602A, 0x0E38,
	0x6F12, 0x0040,
	0x602A, 0x1E92,
	0x6F12, 0x0907,
	0x6F12, 0x070B,
	0x602A, 0x1E9E,
	0x6F12, 0x0303,
	0x6F12, 0x0305,
	0x602A, 0x1E9A,
	0x6F12, 0x0B0B,
	0x6F12, 0x0715,
	0x602A, 0x1E96,
	0x6F12, 0x0F0C,
	0x6F12, 0x0C04,
	0x602A, 0x1EAE,
	0x6F12, 0x3D68,
	0x602A, 0x2440,
	0x6F12, 0x0080,
	0x602A, 0x243E,
	0x6F12, 0x006F,
	0x602A, 0x243C,
	0x6F12, 0x44C2,
	0xF412, 0x0000,
	0xF45A, 0x0FFE,
	0x6028, 0x2000,
	0x602A, 0x10B0,
	0x6F12, 0x0100,
	0x602A, 0x10B4,
	0x6F12, 0x003F,
	0x602A, 0x10BA,
	0x6F12, 0x0040,
	0x602A, 0x10C4,
	0x6F12, 0xF480,
	0x602A, 0x3B20,
	0x6F12, 0x0100,
	0x602A, 0x3B26,
	0x6F12, 0x0008,
	0x602A, 0x3B22,
	0x6F12, 0x0080,
	0x9C14, 0x0000,
	0x9C1A, 0x0000,
	0x9C06, 0x0303,
	0x9C1C, 0x0020,
	0x9C1E, 0x0400,
	0x6028, 0x2000,
	0x602A, 0x239A,
	0x6F12, 0x00F0,
	0x602A, 0x2398,
	0x6F12, 0x00F0,
	0x602A, 0x2396,
	0x6F12, 0x00F0,
	0x602A, 0x2394,
	0x6F12, 0x00F0,
	0x602A, 0x2392,
	0x6F12, 0x00F0,
	0x602A, 0x2386,
	0x6F12, 0x00F0,
	0x602A, 0x2384,
	0x6F12, 0x00F0,
	0x602A, 0x2382,
	0x6F12, 0x00F0,
	0x602A, 0x2380,
	0x6F12, 0x00F0,
	0x602A, 0x237E,
	0x6F12, 0x00F0,
	0x9C24, 0x0100,
	0x6028, 0x2000,
	0x602A, 0x1EB2,
	0x6F12, 0x6A2D,
	0x602A, 0x117A,
	0x6F12, 0x0088,
	0x6F12, 0x0098,
	0x602A, 0x29F0,
	0x6F12, 0x0000,
	0x602A, 0x1076,
	0x6F12, 0x0200,
	0x0136, 0x1800,
	0x0304, 0x0006,
	0x030C, 0x0000,
	0x0302, 0x0001,
	0x030E, 0x0003,
	0x0312, 0x0000,
	0x030A, 0x0001,
	0x0308, 0x0008,
	0x0202, 0x0020,
	0x6028, 0x2000,
	0x602A, 0x1170,
	0x6F12, 0x0005,
	0x602A, 0x33FA,
	0x6F12, 0x0100,
	0x602A, 0x33F8,
	0x6F12, 0x0000,
	0x0B06, 0x0101,
	0x6028, 0x2000,
	0x602A, 0x1094,
	0x6F12, 0x0002,
	0x602A, 0x3020,
	0x6F12, 0x0040,
	0x0FEA, 0x0500,
	0x6028, 0x2000,
	0x602A, 0x1E80,
	0x6F12, 0x0100,
	0x602A, 0x2516,
	0x6F12, 0x01C0,
	0x6F12, 0x01C2,
	0x6F12, 0x02C8,
	0x6F12, 0x02CA,
	0x6F12, 0x02D0,
	0x6F12, 0x02D2,
	0x6F12, 0x01D8,
	0x6F12, 0x01DA,
	0x6F12, 0x01E0,
	0x6F12, 0x01E2,
	0x6F12, 0x02E8,
	0x6F12, 0x02EA,
	0x6F12, 0x02F0,
	0x6F12, 0x02F2,
	0x6F12, 0x01F8,
	0x6F12, 0x01FA,
	0x6F12, 0x01C0,
	0x6F12, 0x01C2,
	0x6F12, 0x02C8,
	0x6F12, 0x02CA,
	0x6F12, 0x02D0,
	0x6F12, 0x02D2,
	0x6F12, 0x01D8,
	0x6F12, 0x01DA,
	0x6F12, 0x01E0,
	0x6F12, 0x01E2,
	0x6F12, 0x02E8,
	0x6F12, 0x02EA,
	0x6F12, 0x02F0,
	0x6F12, 0x02F2,
	0x6F12, 0x01F8,
	0x6F12, 0x01FA,
	0x602A, 0x2596,
	0x6F12, 0x01C0,
	0x6F12, 0x01C2,
	0x6F12, 0x01C8,
	0x6F12, 0x01CA,
	0x6F12, 0x01D0,
	0x6F12, 0x01D2,
	0x6F12, 0x01D8,
	0x6F12, 0x01DA,
	0x6F12, 0x01E0,
	0x6F12, 0x01E2,
	0x6F12, 0x01E8,
	0x6F12, 0x01EA,
	0x6F12, 0x01F0,
	0x6F12, 0x01F2,
	0x6F12, 0x01F8,
	0x6F12, 0x01FA,
	0x6F12, 0x01C0,
	0x6F12, 0x01C2,
	0x6F12, 0x01C8,
	0x6F12, 0x01CA,
	0x6F12, 0x01D0,
	0x6F12, 0x01D2,
	0x6F12, 0x01D8,
	0x6F12, 0x01DA,
	0x6F12, 0x01E0,
	0x6F12, 0x01E2,
	0x6F12, 0x01E8,
	0x6F12, 0x01EA,
	0x6F12, 0x01F0,
	0x6F12, 0x01F2,
	0x6F12, 0x01F8,
	0x6F12, 0x01FA,
	0x602A, 0x0E7C,
	0x6F12, 0x0400,
	0x0118, 0x0104,
	0x010C, 0x0000,
	0x011A, 0x0401,
	0x6028, 0x2000,
	0x602A, 0x1070,
	0x6F12, 0x0004,
	0x602A, 0x0EFC,
	0x6F12, 0x0100,
	0x602A, 0x0ED4,
	0x6F12, 0x0003,
	0x602A, 0x0F00,
	0x6F12, 0x0100,
	0x6F12, 0x0800,
	0x602A, 0x2634,
	0x6F12, 0x0500,
	0x602A, 0x34E4,
	0x6F12, 0x0000,
	0x602A, 0x34DC,
	0x6F12, 0x07C0,
	0x6F12, 0x07C0,
	0x602A, 0x34D0,
	0x6F12, 0x0100,
	0x602A, 0x39A8,
	0x6F12, 0x0001,
	0x602A, 0x39B0,
	0x6F12, 0x001E,
	0x6F12, 0x001E,
	0x6F12, 0x001E,
	0x6F12, 0x001E,
	0x6F12, 0x0005,
	0x602A, 0x39C2,
	0x6F12, 0x0001,
	0x602A, 0x3992,
	0x6F12, 0x0002,
	0x602A, 0x39CC,
	0x6F12, 0x0001,
	0x602A, 0x37EC,
	0x6F12, 0x0023,
	0x6F12, 0x0023,
	0x602A, 0x37FA,
	0x6F12, 0x0000,
	0x602A, 0x3806,
	0x6F12, 0x0023,
	0x6F12, 0x0023,
	0x602A, 0x3814,
	0x6F12, 0x0000,
	0x602A, 0x39DA,
	0x6F12, 0x104D,
	0x6F12, 0x0000,
	0x6F12, 0x0B00,
	0x6F12, 0x0B80,
	0x6F12, 0x2274,
	0x6F12, 0x0000,
	0x6F12, 0x0812,
	0x6F12, 0x1F2D,
	0x6F12, 0x3944,
	0x6F12, 0x4E56,
	0x6F12, 0x010A,
	0x6F12, 0x1521,
	0x6F12, 0x2E3E,
	0x6F12, 0x4C57,
	0x6F12, 0x626B,
	0x6F12, 0x0E18,
	0x6F12, 0x2330,
	0x6F12, 0x4050,
	0x6F12, 0x606D,
	0x6F12, 0x7882,
	0x6F12, 0x1E2A,
	0x6F12, 0x3645,
	0x6F12, 0x5669,
	0x6F12, 0x7A89,
	0x6F12, 0x96A1,
	0x6F12, 0x313E,
	0x6F12, 0x4C5D,
	0x6F12, 0x7085,
	0x6F12, 0x98A8,
	0x6F12, 0xB7C4,
	0x6F12, 0x4250,
	0x6F12, 0x5F71,
	0x6F12, 0x869D,
	0x6F12, 0xB2C4,
	0x6F12, 0xD4E2,
	0x6F12, 0x4F5E,
	0x6F12, 0x6F82,
	0x6F12, 0x98B1,
	0x6F12, 0xC7DA,
	0x6F12, 0xEBFA,
	0x6F12, 0x5A6A,
	0x6F12, 0x7B90,
	0x6F12, 0xA7C1,
	0x6F12, 0xD9ED,
	0x6F12, 0xFFFF,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6028, 0x2000,
	0x602A, 0x1F1E,
	0x6F12, 0x00F0,
	0x6F12, 0x00F0,
	0x6F12, 0x00F0,
	0x6F12, 0x00F0,
	0x6F12, 0x00F0,
	0x602A, 0x1F32,
	0x6F12, 0x00F0,
	0x6F12, 0x00F0,
	0x6F12, 0x00F0,
	0x6F12, 0x00F0,
	0x6F12, 0x00F0,
	0x602A, 0x214E,
	0x6F12, 0x00F0,
	0x6F12, 0x00F0,
	0x6F12, 0x00F0,
	0x6F12, 0x00F0,
	0x6F12, 0x00F0,
	0x602A, 0x2162,
	0x6F12, 0x00F0,
	0x6F12, 0x00F0,
	0x6F12, 0x00F0,
	0x6F12, 0x00F0,
	0x6F12, 0x00F0,
	0x0116, 0x2B00,
};
#endif

static void sensor_init(void)
{
	int ret = 0;
	struct cam_hw_param *hw_param = NULL;

	LOG_INF("%s", __func__);
	ret = write_cmos_sensor(0xFCFC, 0x4000);
	ret |= write_cmos_sensor(0x6010, 0x0001);

	mDELAY(11);

#if INDIRECT_BURST
	write_cmos_sensor(0x6214, 0xF9F3);

	/* Disable Auto increment */
	write_cmos_sensor(0x6004, 0x0001);

	write_cmos_sensor(0x6028, 0x2000);
	write_cmos_sensor(0x602A, 0x60D0);


	/* Start burst mode */
	ret |= burst_table_write_cmos_sensor(addr_data_pair_burst_init_gm2,
		sizeof(addr_data_pair_burst_init_gm2) / sizeof(kal_uint16));

	/* Enable Auto increment */
	write_cmos_sensor(0x6028, 0x4000);
	write_cmos_sensor(0x6004, 0x0000);

	write_cmos_sensor(0x6028, 0x2000);
	ret |= table_write_cmos_sensor(addr_data_pair_other_init_gm2,
		sizeof(addr_data_pair_other_init_gm2) / sizeof(kal_uint16));
#else
	ret |= table_write_cmos_sensor(addr_data_pair_init_gm2,
		sizeof(addr_data_pair_init_gm2) / sizeof(kal_uint16));
#endif
	if (ret != 0) {
		imgsensor_sec_get_hw_param(&hw_param, SENSOR_POSITION_REAR);
		if (hw_param)
			hw_param->i2c_sensor_err_cnt++;
	}

	write_cmos_sensor_8(0x0138, 0x01);/*enable temperature*/
}

#if WRITE_SENSOR_CAL_FOR_GGC
extern int imgsensor_get_cal_buf_by_sensor_idx(int sensor_idx, char **buf);


#define SENSOR_GM2_GGC_CAL_BASE_REAR           (0x2A96)
#define SENSOR_GM2_GGC_CAL_SIZE                (92)
#define SENSOR_GM2_GGC_REG_ADDR                (0x39DA)

static void sensor_GGC_write(void)
{
	int i = 0;
	char *rom_cal_buf = NULL;
	char cal_data[SENSOR_GM2_GGC_CAL_SIZE-2] = {0};

	int start_addr = SENSOR_GM2_GGC_REG_ADDR;
	int data_size = SENSOR_GM2_GGC_CAL_SIZE-2;

	// TODO : sensor_dev_id needs to be updated according to actual id.
	int sensor_dev_id = IMGSENSOR_SENSOR_IDX_MAIN;

	LOG_INF("%s", __func__);

	if (imgsensor_get_cal_buf_by_sensor_idx(sensor_dev_id, &rom_cal_buf)) {
		pr_err("%s : cal data is NULL", __func__);
		return;
	}
	rom_cal_buf += SENSOR_GM2_GGC_CAL_BASE_REAR;
	if (rom_cal_buf[0] == 0xFF && rom_cal_buf[1] == 0x00) {
		// Big Endian
		memcpy(cal_data, &rom_cal_buf[2], SENSOR_GM2_GGC_CAL_SIZE-2);
	} else if (rom_cal_buf[0] == 0x00 && rom_cal_buf[1] == 0xFF) {
		// Little Endian
		rom_cal_buf += 2;
		for (i = 0; i + 1 < SENSOR_GM2_GGC_CAL_SIZE - 2; i += 2) {
			cal_data[i] = rom_cal_buf[i+1];
			cal_data[i+1] = rom_cal_buf[i];
		}
		if ((SENSOR_GM2_GGC_CAL_SIZE - 2) & 0x1)
			cal_data[i] = rom_cal_buf[i];
	} else {
		pr_err("%s : endian check fail (%X, %X)", __func__, rom_cal_buf[0], rom_cal_buf[1]);
		return;
	}

	write_cmos_sensor(0x6028, 0x2000);
	write_cmos_sensor(0x602A, start_addr);
#if 0
	//data size is even
	for (i = 0; i < data_size / 2 ; i++)
		write_cmos_sensor(0x6F12, cal_data[2*i]);
#else
	for (i = 0; i < data_size ; i++)
		write_cmos_sensor_8(0x6F12, cal_data[i]);
#endif
	pr_info("%s done writing %d bytes", __func__, data_size);
}
#endif

static kal_uint16 addr_data_pair_pre_gm2[] = {
	0x6214, 0xF9F3,
	0x0340, 0x0C8A,
	0x0342, 0x2700,
	0x0344, 0x0000,
	0x0346, 0x0000,
	0x0348, 0x1F3F,
	0x034A, 0x176F,
	0x034C, 0x0FA0,
	0x034E, 0x0BB8,
	0x0350, 0x0000,
	0x0352, 0x0000,
	0x0900, 0x0112,
	0x0380, 0x0001,
	0x0382, 0x0001,
	0x0384, 0x0002,
	0x0386, 0x0002,
	0x0400, 0x2010,
	0x0402, 0x1010,
	0x0404, 0x1000,
	0x0136, 0x1A00,
	0x0300, 0x0008,
	0x0302, 0x0001,
	0x0304, 0x0006,
	0x0306, 0x00DE,
	0x0308, 0x0008,
	0x030A, 0x0001,
	0x030C, 0x0000,
	0x030E, 0x0004,
	0x0310, 0x0068,
	0x0312, 0x0000,
	0x0702, 0x0000,
	0x0202, 0x0020,
	0x6028, 0x2000,
	0x602A, 0x0E36,
	0x6F12, 0x000A,
	0x602A, 0x1250,
	0x6F12, 0x0002,
	0xF44A, 0x0007,
	0xF454, 0x0011,
	0x6028, 0x2000,
	0x602A, 0x10C0,
	0x6F12, 0xBFC0,
	0x6F12, 0xBFC1,
	0x602A, 0x3B24,
	0x6F12, 0x000A,
	0x9C02, 0x1FC0,
	0x9C04, 0x1FC7,
	0x6028, 0x2000,
	0x602A, 0x3630,
	0x6F12, 0x0000,
	0x602A, 0x3632,
	0x6F12, 0x00B0,
	0x602A, 0x11B4,
	0x6F12, 0x0190,
	0x6028, 0x2000,
	0x602A, 0x0DD6,
	0x6F12, 0x0008,
	0x602A, 0x0DDC,
	0x6F12, 0x0000,
	0x602A, 0x1EAC,
	0x6F12, 0x0096,
	0x6028, 0x2000,
	0x602A, 0x33B0,
	0x6F12, 0x0004,
	0x602A, 0x33B6,
	0x6F12, 0x0001,
	0x602A, 0x33FC,
	0x6F12, 0x0604,
	0x602A, 0x33FE,
	0x6F12, 0x0704,
	0x602A, 0x3462,
	0x6F12, 0x0501,
	0x602A, 0x347C,
	0x6F12, 0x0000,
	0x602A, 0x34A6,
	0x6F12, 0x5555,
	0x602A, 0x34F2,
	0x6F12, 0x0000,
	0x0D00, 0x0101,
	0x0D02, 0x0101,
	0x0114, 0x0301,
	0x6028, 0x2000,
	0x602A, 0x116E,
	0x6F12, 0x0001,
	0x602A, 0x1172,
	0x6F12, 0x0100,
	0x602A, 0x6C0E,
	0x6F12, 0x0200,
	0x602A, 0x2A10,
	0x6F12, 0x0100,
	0x602A, 0x2A80,
	0x6F12, 0x0100,
	0x602A, 0x0E6E,
	0x6F12, 0x0045,
	0x602A, 0x6C0A,
	0x6F12, 0x00CF,
	0x6F12, 0x00A8,
	0xB134, 0x0000,
	0x0BC8, 0x0102,
	0x6028, 0x2000,
	0x602A, 0x39AA,
	0x6F12, 0x0003,
	0x6F12, 0x0001,
	0x6F12, 0x0001,
	0x602A, 0x39BA,
	0x6F12, 0x0001,
	0x6F12, 0x0000,
	0x6F12, 0x0001,
	0x6F12, 0x0001,
	0x602A, 0x39C4,
	0x6F12, 0x0001,
	0x6028, 0x2000,
	0x602A, 0x2266,
	0x6F12, 0x0030,
	0x6F12, 0x0030,
	0x6F12, 0x0030,
	0x6F12, 0x0030,
	0x6F12, 0x0030,
	0x602A, 0x227A,
	0x6F12, 0x0030,
	0x6F12, 0x0030,
	0x6F12, 0x0030,
	0x6F12, 0x0030,
	0x6F12, 0x0030,
	0x602A, 0x0E26,
	0x6F12, 0x1250,
	0x6F12, 0x0ECC,
	0x602A, 0x1E7E,
	0x6F12, 0x0400,
	0x602A, 0x2450,
	0x6F12, 0x0000,
	0x602A, 0x25F8,
	0x6F12, 0x0000,
	0x602A, 0x34D8,
	0x6F12, 0x07C0,
	0x6F12, 0x07C0,
	0x6028, 0x2000,
	0x602A, 0x39B0,
	0x6F12, 0x0014,
	0x6F12, 0x001E,
	0x6F12, 0x0014,
	0x6F12, 0x001E,
};


static void preview_setting(void)
{
	LOG_INF("%s", __func__);

	table_write_cmos_sensor(addr_data_pair_pre_gm2,
			sizeof(addr_data_pair_pre_gm2) / sizeof(kal_uint16));
}

static kal_uint16 addr_data_pair_cap_gm2[] = {
	0x6214, 0xF9F3,
	0x0340, 0x0C8A,
	0x0342, 0x2700,
	0x0344, 0x0000,
	0x0346, 0x0000,
	0x0348, 0x1F3F,
	0x034A, 0x176F,
	0x034C, 0x0FA0,
	0x034E, 0x0BB8,
	0x0350, 0x0000,
	0x0352, 0x0000,
	0x0900, 0x0112,
	0x0380, 0x0001,
	0x0382, 0x0001,
	0x0384, 0x0002,
	0x0386, 0x0002,
	0x0400, 0x2010,
	0x0402, 0x1010,
	0x0404, 0x1000,
	0x0136, 0x1A00,
	0x0300, 0x0008,
	0x0302, 0x0001,
	0x0304, 0x0006,
	0x0306, 0x00DE,
	0x0308, 0x0008,
	0x030A, 0x0001,
	0x030C, 0x0000,
	0x030E, 0x0004,
	0x0310, 0x0068,
	0x0312, 0x0000,
	0x0702, 0x0000,
	0x0202, 0x0020,
	0x6028, 0x2000,
	0x602A, 0x0E36,
	0x6F12, 0x000A,
	0x602A, 0x1250,
	0x6F12, 0x0002,
	0xF44A, 0x0007,
	0xF454, 0x0011,
	0x6028, 0x2000,
	0x602A, 0x10C0,
	0x6F12, 0xBFC0,
	0x6F12, 0xBFC1,
	0x602A, 0x3B24,
	0x6F12, 0x000A,
	0x9C02, 0x1FC0,
	0x9C04, 0x1FC7,
	0x6028, 0x2000,
	0x602A, 0x3630,
	0x6F12, 0x0000,
	0x602A, 0x3632,
	0x6F12, 0x00B0,
	0x602A, 0x11B4,
	0x6F12, 0x0190,
	0x6028, 0x2000,
	0x602A, 0x0DD6,
	0x6F12, 0x0008,
	0x602A, 0x0DDC,
	0x6F12, 0x0000,
	0x602A, 0x1EAC,
	0x6F12, 0x0096,
	0x6028, 0x2000,
	0x602A, 0x33B0,
	0x6F12, 0x0004,
	0x602A, 0x33B6,
	0x6F12, 0x0001,
	0x602A, 0x33FC,
	0x6F12, 0x0604,
	0x602A, 0x33FE,
	0x6F12, 0x0704,
	0x602A, 0x3462,
	0x6F12, 0x0501,
	0x602A, 0x347C,
	0x6F12, 0x0000,
	0x602A, 0x34A6,
	0x6F12, 0x5555,
	0x602A, 0x34F2,
	0x6F12, 0x0000,
	0x0D00, 0x0101,
	0x0D02, 0x0101,
	0x0114, 0x0301,
	0x6028, 0x2000,
	0x602A, 0x116E,
	0x6F12, 0x0001,
	0x602A, 0x1172,
	0x6F12, 0x0100,
	0x602A, 0x6C0E,
	0x6F12, 0x0200,
	0x602A, 0x2A10,
	0x6F12, 0x0100,
	0x602A, 0x2A80,
	0x6F12, 0x0100,
	0x602A, 0x0E6E,
	0x6F12, 0x0045,
	0x602A, 0x6C0A,
	0x6F12, 0x00CF,
	0x6F12, 0x00A8,
	0xB134, 0x0000,
	0x0BC8, 0x0102,
	0x6028, 0x2000,
	0x602A, 0x39AA,
	0x6F12, 0x0003,
	0x6F12, 0x0001,
	0x6F12, 0x0001,
	0x602A, 0x39BA,
	0x6F12, 0x0001,
	0x6F12, 0x0000,
	0x6F12, 0x0001,
	0x6F12, 0x0001,
	0x602A, 0x39C4,
	0x6F12, 0x0001,
	0x6028, 0x2000,
	0x602A, 0x2266,
	0x6F12, 0x0030,
	0x6F12, 0x0030,
	0x6F12, 0x0030,
	0x6F12, 0x0030,
	0x6F12, 0x0030,
	0x602A, 0x227A,
	0x6F12, 0x0030,
	0x6F12, 0x0030,
	0x6F12, 0x0030,
	0x6F12, 0x0030,
	0x6F12, 0x0030,
	0x602A, 0x0E26,
	0x6F12, 0x1250,
	0x6F12, 0x0ECC,
	0x602A, 0x1E7E,
	0x6F12, 0x0400,
	0x602A, 0x2450,
	0x6F12, 0x0000,
	0x602A, 0x25F8,
	0x6F12, 0x0000,
	0x602A, 0x34D8,
	0x6F12, 0x07C0,
	0x6F12, 0x07C0,
	0x6028, 0x2000,
	0x602A, 0x39B0,
	0x6F12, 0x0014,
	0x6F12, 0x001E,
	0x6F12, 0x0014,
	0x6F12, 0x001E,
};

static kal_uint16 addr_data_pair_custom2_slim_gm2[] = {
	0x6214, 0xF9F3,
	0x0340, 0x17E3,
	0x0342, 0x2800,
	0x0344, 0x0000,
	0x0346, 0x0000,
	0x0348, 0x1F3F,
	0x034A, 0x176F,
	0x034C, 0x1F40,
	0x034E, 0x1770,
	0x0350, 0x0000,
	0x0352, 0x0000,
	0x0900, 0x0111,
	0x0380, 0x0001,
	0x0382, 0x0001,
	0x0384, 0x0001,
	0x0386, 0x0001,
	0x0400, 0x1010,
	0x0402, 0x1010,
	0x0404, 0x1000,
	0x0136, 0x1A00,
	0x0300, 0x0005,
	0x0302, 0x0001,
	0x0304, 0x0006,
	0x0306, 0x00B9,
	0x0308, 0x0008,
	0x030A, 0x0001,
	0x030C, 0x0000,
	0x030E, 0x0004,
	0x0310, 0x009E,
	0x0312, 0x0000,
	0x0702, 0x0000,
	0x0202, 0x0020,
	0x6028, 0x2000,
	0x602A, 0x0E36,
	0x6F12, 0x000A,
	0x602A, 0x1250,
	0x6F12, 0x0004,
	0xF44A, 0x0005,
	0xF454, 0x0011,
	0x6028, 0x2000,
	0x602A, 0x10C0,
	0x6F12, 0xBFC0,
	0x6F12, 0xBFC0,
	0x602A, 0x3B24,
	0x6F12, 0x0006,
	0x9C02, 0x1FC0,
	0x9C04, 0x1FC7,
	0x6028, 0x2000,
	0x602A, 0x3630,
	0x6F12, 0x0000,
	0x602A, 0x3632,
	0x6F12, 0x0020,
	0x602A, 0x11B4,
	0x6F12, 0x0020,
	0x6028, 0x2000,
	0x602A, 0x0DD6,
	0x6F12, 0x000A,
	0x602A, 0x0DDC,
	0x6F12, 0x0001,
	0x602A, 0x1EAC,
	0x6F12, 0x0096,
	0x6028, 0x2000,
	0x602A, 0x33B0,
	0x6F12, 0x0004,
	0x602A, 0x33B6,
	0x6F12, 0x0000,
	0x602A, 0x33FC,
	0x6F12, 0x0604,
	0x602A, 0x33FE,
	0x6F12, 0x0704,
	0x602A, 0x3462,
	0x6F12, 0x1401,
	0x602A, 0x347C,
	0x6F12, 0x0000,
	0x602A, 0x34A6,
	0x6F12, 0x5555,
	0x602A, 0x34F2,
	0x6F12, 0x0000,
	0x0D00, 0x0101,
	0x0D02, 0x0001,
	0x0114, 0x0300,
	0x6028, 0x2000,
	0x602A, 0x116E,
	0x6F12, 0x0003,
	0x602A, 0x1172,
	0x6F12, 0x0000,
	0x602A, 0x6C0E,
	0x6F12, 0x0200,
	0x602A, 0x2A10,
	0x6F12, 0x0000,
	0x602A, 0x2A80,
	0x6F12, 0x0200,
	0x602A, 0x0E6E,
	0x6F12, 0x0100,
	0x602A, 0x6C0A,
	0x6F12, 0x00CF,
	0x6F12, 0x00A8,
	0xB134, 0x0100,
	0x0BC8, 0x0002,
	0x6028, 0x2000,
	0x602A, 0x39AA,
	0x6F12, 0x0003,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x602A, 0x39BA,
	0x6F12, 0x0000,
	0x6F12, 0x0001,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x602A, 0x39C4,
	0x6F12, 0x0000,
	0x6028, 0x2000,
	0x602A, 0x2266,
	0x6F12, 0x0030,
	0x6F12, 0x0030,
	0x6F12, 0x0030,
	0x6F12, 0x0030,
	0x6F12, 0x0030,
	0x602A, 0x227A,
	0x6F12, 0x0030,
	0x6F12, 0x0030,
	0x6F12, 0x0030,
	0x6F12, 0x0030,
	0x6F12, 0x0030,
	0x602A, 0x0E26,
	0x6F12, 0x0FD2,
	0x6F12, 0x0C4E,
	0x602A, 0x1E7E,
	0x6F12, 0x0401,
	0x602A, 0x2450,
	0x6F12, 0x0000,
	0x602A, 0x25F8,
	0x6F12, 0x0000,
	0x602A, 0x34D8,
	0x6F12, 0x0040,
	0x6F12, 0x0040,
	0x6028, 0x2000,
	0x602A, 0x39B0,
	0x6F12, 0x001E,
	0x6F12, 0x001E,
	0x6F12, 0x001E,
	0x6F12, 0x001E,
};

static kal_uint16 addr_data_pair_custom3_gm2[] = {
// NOT USED
};

static kal_uint16 addr_data_pair_custom4_gm2[] = {
// NOT USED
};

static kal_uint16 addr_data_pair_custom5_gm2[] = {
// NOT USED
};

static void capture_setting(kal_uint16 currefps)
{
	LOG_INF("capture_setting() E! currefps:%d\n", currefps);

	table_write_cmos_sensor(addr_data_pair_cap_gm2,
			sizeof(addr_data_pair_cap_gm2) / sizeof(kal_uint16));
}

static kal_uint16 addr_data_pair_video_gm2[] = {
	0x6214, 0xF9F3,
	0x0340, 0x0C8A,
	0x0342, 0x2700,
	0x0344, 0x0000,
	0x0346, 0x02E8,
	0x0348, 0x1F3F,
	0x034A, 0x1487,
	0x034C, 0x0FA0,
	0x034E, 0x08D0,
	0x0350, 0x0000,
	0x0352, 0x0000,
	0x0900, 0x0112,
	0x0380, 0x0001,
	0x0382, 0x0001,
	0x0384, 0x0002,
	0x0386, 0x0002,
	0x0400, 0x2010,
	0x0402, 0x1010,
	0x0404, 0x1000,
	0x0136, 0x1A00,
	0x0300, 0x0008,
	0x0302, 0x0001,
	0x0304, 0x0006,
	0x0306, 0x00DE,
	0x0308, 0x0008,
	0x030A, 0x0001,
	0x030C, 0x0000,
	0x030E, 0x0004,
	0x0310, 0x0068,
	0x0312, 0x0000,
	0x0702, 0x0000,
	0x0202, 0x0020,
	0x6028, 0x2000,
	0x602A, 0x0E36,
	0x6F12, 0x000A,
	0x602A, 0x1250,
	0x6F12, 0x0002,
	0xF44A, 0x0007,
	0xF454, 0x0011,
	0x6028, 0x2000,
	0x602A, 0x10C0,
	0x6F12, 0xBFC0,
	0x6F12, 0xBFC1,
	0x602A, 0x3B24,
	0x6F12, 0x000A,
	0x9C02, 0x1FC0,
	0x9C04, 0x1FC7,
	0x6028, 0x2000,
	0x602A, 0x3630,
	0x6F12, 0x0000,
	0x602A, 0x3632,
	0x6F12, 0x00B0,
	0x602A, 0x11B4,
	0x6F12, 0x0190,
	0x6028, 0x2000,
	0x602A, 0x0DD6,
	0x6F12, 0x0008,
	0x602A, 0x0DDC,
	0x6F12, 0x0000,
	0x602A, 0x1EAC,
	0x6F12, 0x0096,
	0x6028, 0x2000,
	0x602A, 0x33B0,
	0x6F12, 0x0004,
	0x602A, 0x33B6,
	0x6F12, 0x0001,
	0x602A, 0x33FC,
	0x6F12, 0x0604,
	0x602A, 0x33FE,
	0x6F12, 0x0704,
	0x602A, 0x3462,
	0x6F12, 0x0501,
	0x602A, 0x347C,
	0x6F12, 0x0000,
	0x602A, 0x34A6,
	0x6F12, 0x5555,
	0x602A, 0x34F2,
	0x6F12, 0x0000,
	0x0D00, 0x0101,
	0x0D02, 0x0101,
	0x0114, 0x0301,
	0x6028, 0x2000,
	0x602A, 0x116E,
	0x6F12, 0x0001,
	0x602A, 0x1172,
	0x6F12, 0x0100,
	0x602A, 0x6C0E,
	0x6F12, 0x0200,
	0x602A, 0x2A10,
	0x6F12, 0x0100,
	0x602A, 0x2A80,
	0x6F12, 0x0100,
	0x602A, 0x0E6E,
	0x6F12, 0x0045,
	0x602A, 0x6C0A,
	0x6F12, 0x00CF,
	0x6F12, 0x00A8,
	0xB134, 0x0000,
	0x0BC8, 0x0102,
	0x6028, 0x2000,
	0x602A, 0x39AA,
	0x6F12, 0x0003,
	0x6F12, 0x0001,
	0x6F12, 0x0001,
	0x602A, 0x39BA,
	0x6F12, 0x0001,
	0x6F12, 0x0000,
	0x6F12, 0x0001,
	0x6F12, 0x0001,
	0x602A, 0x39C4,
	0x6F12, 0x0001,
	0x6028, 0x2000,
	0x602A, 0x2266,
	0x6F12, 0x0030,
	0x6F12, 0x0030,
	0x6F12, 0x0030,
	0x6F12, 0x0030,
	0x6F12, 0x0030,
	0x602A, 0x227A,
	0x6F12, 0x0030,
	0x6F12, 0x0030,
	0x6F12, 0x0030,
	0x6F12, 0x0030,
	0x6F12, 0x0030,
	0x602A, 0x0E26,
	0x6F12, 0x1250,
	0x6F12, 0x0ECC,
	0x602A, 0x1E7E,
	0x6F12, 0x0400,
	0x602A, 0x2450,
	0x6F12, 0x0000,
	0x602A, 0x25F8,
	0x6F12, 0x0000,
	0x602A, 0x34D8,
	0x6F12, 0x07C0,
	0x6F12, 0x07C0,
	0x6028, 0x2000,
	0x602A, 0x39B0,
	0x6F12, 0x0014,
	0x6F12, 0x001E,
	0x6F12, 0x0014,
	0x6F12, 0x001E,
};

static void normal_video_setting(kal_uint16 currefps)
{
	LOG_INF("normal_video_setting() E! currefps:%d\n", currefps);

	table_write_cmos_sensor(addr_data_pair_video_gm2,
		sizeof(addr_data_pair_video_gm2) / sizeof(kal_uint16));
}

static kal_uint16 addr_data_pair_hs_gm2[] = {
	0x6214, 0xF9F3,
	0x0340, 0x04F0,
	0x0342, 0x1080,
	0x0344, 0x0020,
	0x0346, 0x0308,
	0x0348, 0x1F1F,
	0x034A, 0x1467,
	0x034C, 0x07C0,
	0x034E, 0x0458,
	0x0350, 0x0000,
	0x0352, 0x0000,
	0x0900, 0x0124,
	0x0380, 0x0002,
	0x0382, 0x0002,
	0x0384, 0x0002,
	0x0386, 0x0006,
	0x0400, 0x1010,
	0x0402, 0x1010,
	0x0404, 0x2000,
	0x0136, 0x1A00,
	0x0300, 0x0005,
	0x0302, 0x0001,
	0x0304, 0x0006,
	0x0306, 0x00B9,
	0x0308, 0x0008,
	0x030A, 0x0001,
	0x030C, 0x0000,
	0x030E, 0x0004,
	0x0310, 0x0068,
	0x0312, 0x0000,
	0x0702, 0x0000,
	0x0202, 0x0020,
	0x6028, 0x2000,
	0x602A, 0x0E36,
	0x6F12, 0x000A,
	0x602A, 0x1250,
	0x6F12, 0x0002,
	0xF44A, 0x0007,
	0xF454, 0x0011,
	0x6028, 0x2000,
	0x602A, 0x10C0,
	0x6F12, 0xBFC2,
	0x6F12, 0xBFC3,
	0x602A, 0x3B24,
	0x6F12, 0x0008,
	0x9C02, 0x0FE0,
	0x9C04, 0x0FE7,
	0x6028, 0x2000,
	0x602A, 0x3630,
	0x6F12, 0x0000,
	0x602A, 0x3632,
	0x6F12, 0x0240,
	0x602A, 0x11B4,
	0x6F12, 0x0040,
	0x6028, 0x2000,
	0x602A, 0x0DD6,
	0x6F12, 0x000A,
	0x602A, 0x0DDC,
	0x6F12, 0x0001,
	0x602A, 0x1EAC,
	0x6F12, 0x0096,
	0x6028, 0x2000,
	0x602A, 0x33B0,
	0x6F12, 0x0008,
	0x602A, 0x33B6,
	0x6F12, 0x0004,
	0x602A, 0x33FC,
	0x6F12, 0x0604,
	0x602A, 0x33FE,
	0x6F12, 0x0704,
	0x602A, 0x3462,
	0x6F12, 0x7701,
	0x602A, 0x347C,
	0x6F12, 0x0000,
	0x602A, 0x34A6,
	0x6F12, 0x5555,
	0x602A, 0x34F2,
	0x6F12, 0x0001,
	0x0D00, 0x0000,
	0x0D02, 0x0001,
	0x0114, 0x0300,
	0x6028, 0x2000,
	0x602A, 0x116E,
	0x6F12, 0x0000,
	0x602A, 0x1172,
	0x6F12, 0x0100,
	0x602A, 0x6C0E,
	0x6F12, 0x0200,
	0x602A, 0x2A10,
	0x6F12, 0x0100,
	0x602A, 0x2A80,
	0x6F12, 0x0200,
	0x602A, 0x0E6E,
	0x6F12, 0xFFFF,
	0x602A, 0x6C0A,
	0x6F12, 0x01CF,
	0x6F12, 0x00A8,
	0xB134, 0x0100,
	0x0BC8, 0x0002,
	0x6028, 0x2000,
	0x602A, 0x39AA,
	0x6F12, 0x0002,
	0x6F12, 0x0001,
	0x6F12, 0x0001,
	0x602A, 0x39BA,
	0x6F12, 0x0000,
	0x6F12, 0x0001,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x602A, 0x39C4,
	0x6F12, 0x0000,
	0x6028, 0x2000,
	0x602A, 0x2266,
	0x6F12, 0x0030,
	0x6F12, 0x0030,
	0x6F12, 0x0030,
	0x6F12, 0x0030,
	0x6F12, 0x0030,
	0x602A, 0x227A,
	0x6F12, 0x0030,
	0x6F12, 0x0030,
	0x6F12, 0x0030,
	0x6F12, 0x0030,
	0x6F12, 0x0030,
	0x602A, 0x0E26,
	0x6F12, 0x0440,
	0x6F12, 0x0440,
	0x602A, 0x1E7E,
	0x6F12, 0x0401,
	0x602A, 0x2450,
	0x6F12, 0x0005,
	0x602A, 0x25F8,
	0x6F12, 0xAAAA,
	0x602A, 0x34D8,
	0x6F12, 0x07C0,
	0x6F12, 0x07C0,
	0x6028, 0x2000,
	0x602A, 0x39B0,
	0x6F12, 0x0014,
	0x6F12, 0x001E,
	0x6F12, 0x0014,
	0x6F12, 0x001E,
};

static void hs_video_setting(void)
{
	LOG_INF("hs_video_setting() E\n");

	table_write_cmos_sensor(addr_data_pair_hs_gm2,
			sizeof(addr_data_pair_hs_gm2) / sizeof(kal_uint16));
}

static void slim_video_setting(void)
{
	LOG_INF("slim_video_setting() E\n");

	table_write_cmos_sensor(addr_data_pair_custom2_slim_gm2,
		sizeof(addr_data_pair_custom2_slim_gm2) / sizeof(kal_uint16));
}

static kal_uint16 addr_data_pair_custom1_gm2[] = {
	0x6214, 0xF9F3,
	0x0340, 0x09E2, // frame length: 1612 -> 2530
	0x0342, 0x1080,
	0x0344, 0x00A0,
	0x0346, 0x0078,
	0x0348, 0x1E9F,
	0x034A, 0x16F7,
	0x034C, 0x0780,
	0x034E, 0x05A0,
	0x0350, 0x0000,
	0x0352, 0x0000,
	0x0900, 0x0124,
	0x0380, 0x0002,
	0x0382, 0x0002,
	0x0384, 0x0002,
	0x0386, 0x0006,
	0x0400, 0x1010,
	0x0402, 0x1010,
	0x0404, 0x2000,
	0x0136, 0x1A00,
	0x0300, 0x0005,
	0x0302, 0x0001,
	0x0304, 0x0006,
	0x0306, 0x00B9,
	0x0308, 0x0008,
	0x030A, 0x0001,
	0x030C, 0x0000,
	0x030E, 0x0004,
	0x0310, 0x0068,
	0x0312, 0x0000,
	0x0702, 0x0000,
	0x0202, 0x0020,
	0x6028, 0x2000,
	0x602A, 0x0E36,
	0x6F12, 0x000A,
	0x602A, 0x1250,
	0x6F12, 0x0002,
	0xF44A, 0x0007,
	0xF454, 0x0011,
	0x6028, 0x2000,
	0x602A, 0x10C0,
	0x6F12, 0xBFC2,
	0x6F12, 0xBFC3,
	0x602A, 0x3B24,
	0x6F12, 0x0008,
	0x9C02, 0x0FE0,
	0x9C04, 0x0FE7,
	0x6028, 0x2000,
	0x602A, 0x3630,
	0x6F12, 0x0000,
	0x602A, 0x3632,
	0x6F12, 0x0240,
	0x602A, 0x11B4,
	0x6F12, 0x0040,
	0x6028, 0x2000,
	0x602A, 0x0DD6,
	0x6F12, 0x000A,
	0x602A, 0x0DDC,
	0x6F12, 0x0001,
	0x602A, 0x1EAC,
	0x6F12, 0x0096,
	0x6028, 0x2000,
	0x602A, 0x33B0,
	0x6F12, 0x0008,
	0x602A, 0x33B6,
	0x6F12, 0x0004,
	0x602A, 0x33FC,
	0x6F12, 0x0604,
	0x602A, 0x33FE,
	0x6F12, 0x0704,
	0x602A, 0x3462,
	0x6F12, 0x7701,
	0x602A, 0x347C,
	0x6F12, 0x0000,
	0x602A, 0x34A6,
	0x6F12, 0x5555,
	0x602A, 0x34F2,
	0x6F12, 0x0001,
	0x0D00, 0x0000,
	0x0D02, 0x0001,
	0x0114, 0x0300,
	0x6028, 0x2000,
	0x602A, 0x116E,
	0x6F12, 0x0000,
	0x602A, 0x1172,
	0x6F12, 0x0100,
	0x602A, 0x6C0E,
	0x6F12, 0x0200,
	0x602A, 0x2A10,
	0x6F12, 0x0100,
	0x602A, 0x2A80,
	0x6F12, 0x0200,
	0x602A, 0x0E6E,
	0x6F12, 0xFFFF,
	0x602A, 0x6C0A,
	0x6F12, 0x01CF,
	0x6F12, 0x00A8,
	0xB134, 0x0100,
	0x0BC8, 0x0002,
	0x6028, 0x2000,
	0x602A, 0x39AA,
	0x6F12, 0x0002,
	0x6F12, 0x0001,
	0x6F12, 0x0001,
	0x602A, 0x39BA,
	0x6F12, 0x0000,
	0x6F12, 0x0001,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x602A, 0x39C4,
	0x6F12, 0x0000,
	0x6028, 0x2000,
	0x602A, 0x2266,
	0x6F12, 0x0030,
	0x6F12, 0x0030,
	0x6F12, 0x0030,
	0x6F12, 0x0030,
	0x6F12, 0x0030,
	0x602A, 0x227A,
	0x6F12, 0x0030,
	0x6F12, 0x0030,
	0x6F12, 0x0030,
	0x6F12, 0x0030,
	0x6F12, 0x0030,
	0x602A, 0x0E26,
	0x6F12, 0x0440,
	0x6F12, 0x0440,
	0x602A, 0x1E7E,
	0x6F12, 0x0401,
	0x602A, 0x2450,
	0x6F12, 0x0005,
	0x602A, 0x25F8,
	0x6F12, 0xAAAA,
	0x602A, 0x34D8,
	0x6F12, 0x07C0,
	0x6F12, 0x07C0,
	0x6028, 0x2000,
	0x602A, 0x39B0,
	0x6F12, 0x0014,
	0x6F12, 0x001E,
	0x6F12, 0x0014,
	0x6F12, 0x001E,
};

static void custom1_setting(void)
{
	LOG_INF("custom1_setting() use custom1\n");
	table_write_cmos_sensor(addr_data_pair_custom1_gm2,
			sizeof(addr_data_pair_custom1_gm2) / sizeof(kal_uint16));
}

static void custom2_setting(void)
{
	LOG_INF("use custom2\n");
	table_write_cmos_sensor(addr_data_pair_custom2_slim_gm2,
		sizeof(addr_data_pair_custom2_slim_gm2) / sizeof(kal_uint16));
}

static void custom3_setting(void)
{
	LOG_INF("use custom3\n");
	table_write_cmos_sensor(addr_data_pair_custom3_gm2,
		sizeof(addr_data_pair_custom3_gm2) / sizeof(kal_uint16));
}

static void custom4_setting(void)
{
	LOG_INF("use custom4\n");
	table_write_cmos_sensor(addr_data_pair_custom4_gm2,
		sizeof(addr_data_pair_custom4_gm2) / sizeof(kal_uint16));
}

static void custom5_setting(void)
{
	LOG_INF("use custom5\n");
	table_write_cmos_sensor(addr_data_pair_custom5_gm2,
		sizeof(addr_data_pair_custom5_gm2) / sizeof(kal_uint16));
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
				LOG_INF("i2c write id: 0x%x, sensor id: 0x%x\n", imgsensor.i2c_write_id, *sensor_id);
			return ERROR_NONE;
			}
			pr_err("Read sensor id fail, id: 0x%x\n", imgsensor.i2c_write_id);
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
	kal_uint8 i = 0;
	kal_uint8 retry = 2;
	kal_uint16 sensor_id = 0;

	LOG_INF("%s", __func__);

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
				pr_err("%s:i2c write id: 0x%x, sensor id: 0x%x\n",
						__func__, imgsensor.i2c_write_id, sensor_id);
				break;
			}

			LOG_INF("%s:Read sensor id fail, id: 0x%x\n", __func__, imgsensor.i2c_write_id);
			retry--;
		} while (retry > 0);

		i++;
		if (sensor_id == imgsensor_info.sensor_id)
			break;
		retry = 2;
	}

	if (imgsensor_info.sensor_id != sensor_id)
		return ERROR_SENSOR_CONNECT_FAIL;

	sensor_init();

#if WRITE_SENSOR_CAL_FOR_GGC
	sensor_GGC_write();
#endif

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
	LOG_INF("preview E\n");

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode			= IMGSENSOR_MODE_PREVIEW;
	imgsensor.pclk					= imgsensor_info.pre.pclk;
	imgsensor.line_length			= imgsensor_info.pre.linelength;
	imgsensor.frame_length			= imgsensor_info.pre.framelength;
	imgsensor.min_frame_length		= imgsensor_info.pre.framelength;
	imgsensor.autoflicker_en		= KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);

	preview_setting();

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
	LOG_INF("capture E\n");

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_CAPTURE;
	imgsensor.pclk = imgsensor_info.cap.pclk;
	imgsensor.line_length = imgsensor_info.cap.linelength;
	imgsensor.frame_length = imgsensor_info.cap.framelength;
	imgsensor.min_frame_length = imgsensor_info.cap.framelength;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);

	capture_setting(imgsensor.current_fps);

	set_mirror_flip(imgsensor.mirror);

	return ERROR_NONE;
}

static kal_uint32 normal_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
	MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("normal_video E\n");

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode			= IMGSENSOR_MODE_VIDEO;
	imgsensor.pclk					= imgsensor_info.normal_video.pclk;
	imgsensor.line_length			= imgsensor_info.normal_video.linelength;
	imgsensor.frame_length			= imgsensor_info.normal_video.framelength;
	imgsensor.min_frame_length		= imgsensor_info.normal_video.framelength;
	imgsensor.autoflicker_en		= KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);

	normal_video_setting(imgsensor.current_fps);

	set_mirror_flip(imgsensor.mirror);

	return ERROR_NONE;
}

static kal_uint32 hs_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			   MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("hs_video E\n");

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

	hs_video_setting();

	set_mirror_flip(imgsensor.mirror);

	return ERROR_NONE;
}

static kal_uint32 slim_video(
	MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
	MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("%s - E\n", __func__);

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

	slim_video_setting();

	set_mirror_flip(imgsensor.mirror);

	return ERROR_NONE;
}

static kal_uint32 custom1(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
								MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("E cur fps: %d\n", imgsensor.current_fps);

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode			= IMGSENSOR_MODE_CUSTOM1;
	imgsensor.pclk					= imgsensor_info.custom1.pclk;
	imgsensor.line_length			= imgsensor_info.custom1.linelength;
	imgsensor.frame_length			= imgsensor_info.custom1.framelength;
	imgsensor.min_frame_length		= imgsensor_info.custom1.framelength;
	imgsensor.autoflicker_en		= KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);

	custom1_setting();

	set_mirror_flip(IMAGE_NORMAL);

	return ERROR_NONE;
}

static kal_uint32 custom2(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
				MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("E cur fps: %d\n", imgsensor.current_fps);

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_CUSTOM2;
	imgsensor.pclk = imgsensor_info.custom2.pclk;
	imgsensor.line_length = imgsensor_info.custom2.linelength;
	imgsensor.frame_length = imgsensor_info.custom2.framelength;
	imgsensor.min_frame_length = imgsensor_info.custom2.framelength;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);

	custom2_setting();

	set_mirror_flip(IMAGE_NORMAL);

	return ERROR_NONE;
}

static kal_uint32 custom3(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
				MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("E cur fps: %d\n", imgsensor.current_fps);

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_CUSTOM3;
	imgsensor.pclk = imgsensor_info.custom3.pclk;
	imgsensor.line_length = imgsensor_info.custom3.linelength;
	imgsensor.frame_length = imgsensor_info.custom3.framelength;
	imgsensor.min_frame_length = imgsensor_info.custom3.framelength;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);

	custom3_setting();

	set_mirror_flip(IMAGE_NORMAL);

	return ERROR_NONE;
}
static kal_uint32 custom4(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
				MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("E cur fps: %d\n", imgsensor.current_fps);

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_CUSTOM4;
	imgsensor.pclk = imgsensor_info.custom4.pclk;
	imgsensor.line_length = imgsensor_info.custom4.linelength;
	imgsensor.frame_length = imgsensor_info.custom4.framelength;
	imgsensor.min_frame_length = imgsensor_info.custom4.framelength;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);

	custom4_setting();

	set_mirror_flip(IMAGE_NORMAL);

	return ERROR_NONE;
}
static kal_uint32 custom5(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
				MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("E cur fps: %d\n", imgsensor.current_fps);

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_CUSTOM5;
	imgsensor.pclk = imgsensor_info.custom5.pclk;
	imgsensor.line_length = imgsensor_info.custom5.linelength;
	imgsensor.frame_length = imgsensor_info.custom5.framelength;
	imgsensor.min_frame_length = imgsensor_info.custom5.framelength;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);

	custom5_setting();

	set_mirror_flip(IMAGE_NORMAL);

	return ERROR_NONE;
}

static kal_uint32 get_resolution(
	MSDK_SENSOR_RESOLUTION_INFO_STRUCT(*sensor_resolution))
{
	LOG_INF("get_resolution E\n");
	sensor_resolution->SensorFullWidth		= imgsensor_info.cap.grabwindow_width;
	sensor_resolution->SensorFullHeight 	= imgsensor_info.cap.grabwindow_height;

	sensor_resolution->SensorPreviewWidth 	= imgsensor_info.pre.grabwindow_width;
	sensor_resolution->SensorPreviewHeight	= imgsensor_info.pre.grabwindow_height;

	sensor_resolution->SensorVideoWidth		= imgsensor_info.normal_video.grabwindow_width;
	sensor_resolution->SensorVideoHeight	= imgsensor_info.normal_video.grabwindow_height;


	sensor_resolution->SensorHighSpeedVideoWidth	= imgsensor_info.hs_video.grabwindow_width;
	sensor_resolution->SensorHighSpeedVideoHeight	= imgsensor_info.hs_video.grabwindow_height;

	sensor_resolution->SensorSlimVideoWidth			= imgsensor_info.slim_video.grabwindow_width;
	sensor_resolution->SensorSlimVideoHeight		= imgsensor_info.slim_video.grabwindow_height;

	sensor_resolution->SensorCustom1Width  = imgsensor_info.custom1.grabwindow_width;
	sensor_resolution->SensorCustom1Height = imgsensor_info.custom1.grabwindow_height;

	sensor_resolution->SensorCustom2Width  =
		imgsensor_info.custom2.grabwindow_width;
	sensor_resolution->SensorCustom2Height =
		imgsensor_info.custom2.grabwindow_height;
	sensor_resolution->SensorCustom3Width  =
		imgsensor_info.custom3.grabwindow_width;
	sensor_resolution->SensorCustom3Height =
		imgsensor_info.custom3.grabwindow_height;
	sensor_resolution->SensorCustom4Width  =
		imgsensor_info.custom4.grabwindow_width;
	sensor_resolution->SensorCustom4Height =
		imgsensor_info.custom4.grabwindow_height;
	sensor_resolution->SensorCustom5Width  =
		imgsensor_info.custom5.grabwindow_width;
	sensor_resolution->SensorCustom5Height =
		imgsensor_info.custom5.grabwindow_height;

	return ERROR_NONE;
}

static kal_uint32 get_info(enum MSDK_SCENARIO_ID_ENUM scenario_id,
			   MSDK_SENSOR_INFO_STRUCT *sensor_info,
			   MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("%s -> scenario_id = %d\n", __func__, scenario_id);

	sensor_info->SensorClockPolarity		= SENSOR_CLOCK_POLARITY_LOW;
	sensor_info->SensorClockFallingPolarity = SENSOR_CLOCK_POLARITY_LOW;

	sensor_info->SensorHsyncPolarity = SENSOR_CLOCK_POLARITY_LOW;
	LOG_INF("sensor_info->SensorHsyncPolarity = %d\n", sensor_info->SensorHsyncPolarity);
	sensor_info->SensorVsyncPolarity = SENSOR_CLOCK_POLARITY_LOW;
	LOG_INF("sensor_info->SensorVsyncPolarity = %d\n", sensor_info->SensorVsyncPolarity);

	sensor_info->SensorInterruptDelayLines = 4;
	sensor_info->SensorResetActiveHigh = FALSE;
	sensor_info->SensorResetDelayCount = 5;

	sensor_info->SensroInterfaceType	= imgsensor_info.sensor_interface_type;
	sensor_info->MIPIsensorType 		= imgsensor_info.mipi_sensor_type;
	sensor_info->SettleDelayMode		= imgsensor_info.mipi_settle_delay_mode;

	sensor_info->SensorOutputDataFormat	= imgsensor_info.sensor_output_dataformat;

	sensor_info->CaptureDelayFrame	= imgsensor_info.cap_delay_frame;
	sensor_info->PreviewDelayFrame	= imgsensor_info.pre_delay_frame;
	sensor_info->VideoDelayFrame	= imgsensor_info.video_delay_frame;

	sensor_info->HighSpeedVideoDelayFrame	= imgsensor_info.hs_video_delay_frame;

	sensor_info->SlimVideoDelayFrame		= imgsensor_info.slim_video_delay_frame;

	sensor_info->Custom1DelayFrame = imgsensor_info.custom1_delay_frame;
	sensor_info->Custom2DelayFrame = imgsensor_info.custom2_delay_frame;
	sensor_info->Custom3DelayFrame = imgsensor_info.custom3_delay_frame;
	sensor_info->Custom4DelayFrame = imgsensor_info.custom4_delay_frame;
	sensor_info->Custom5DelayFrame = imgsensor_info.custom5_delay_frame;
	sensor_info->SensorMasterClockSwitch = 0;
	sensor_info->SensorDrivingCurrent	= imgsensor_info.isp_driving_current;

	sensor_info->AEShutDelayFrame		= imgsensor_info.ae_shut_delay_frame;

	sensor_info->AESensorGainDelayFrame	= imgsensor_info.ae_sensor_gain_delay_frame;

	sensor_info->AEISPGainDelayFrame	= imgsensor_info.ae_ispGain_delay_frame;

	sensor_info->FrameTimeDelayFrame	= imgsensor_info.frame_time_delay_frame;

	sensor_info->IHDR_Support			= imgsensor_info.ihdr_support;
	sensor_info->IHDR_LE_FirstLine		= imgsensor_info.ihdr_le_firstline;
	sensor_info->SensorModeNum			= imgsensor_info.sensor_mode_num;
	sensor_info->SensorMIPILaneNumber	= imgsensor_info.mipi_lane_num;
	sensor_info->SensorClockFreq 		= imgsensor_info.mclk;
	sensor_info->SensorClockDividCount		= 3;
	sensor_info->SensorClockRisingCount		= 0;
	sensor_info->SensorClockFallingCount	= 2;
	sensor_info->SensorPixelClockCount		= 3;
	sensor_info->SensorDataLatchCount		= 2;
	sensor_info->TEMPERATURE_SUPPORT		= imgsensor_info.temperature_support;
	sensor_info->MIPIDataLowPwr2HighSpeedTermDelayCount	= 0;
	sensor_info->MIPICLKLowPwr2HighSpeedTermDelayCount	= 0;
	sensor_info->SensorWidthSampling	= 0;
	sensor_info->SensorHightSampling	= 0;
	sensor_info->SensorPacketECCOrder	= 1;

	sensor_info->PDAF_Support = PDAF_SUPPORT_CAMSV;

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
	LOG_INF("MSDK_SCENARIO_ID_CUSTOM2 scenario_id= %d\n",
		scenario_id);
		sensor_info->SensorGrabStartX = imgsensor_info.custom2.startx;
		sensor_info->SensorGrabStartY = imgsensor_info.custom2.starty;
		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
			imgsensor_info.custom2.mipi_data_lp2hs_settle_dc;
		break;
	case MSDK_SCENARIO_ID_CUSTOM3:
	LOG_INF("MSDK_SCENARIO_ID_CUSTOM3 scenario_id= %d\n",
		scenario_id);
		sensor_info->SensorGrabStartX = imgsensor_info.custom3.startx;
		sensor_info->SensorGrabStartY = imgsensor_info.custom3.starty;
		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
			imgsensor_info.custom3.mipi_data_lp2hs_settle_dc;
		break;
	case MSDK_SCENARIO_ID_CUSTOM4:
	LOG_INF("MSDK_SCENARIO_ID_CUSTOM4 scenario_id= %d\n",
		scenario_id);
		sensor_info->SensorGrabStartX = imgsensor_info.custom4.startx;
		sensor_info->SensorGrabStartY = imgsensor_info.custom4.starty;
		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
			imgsensor_info.custom4.mipi_data_lp2hs_settle_dc;
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
		LOG_INF("control():MSDK_SCENARIO_ID_CAMERA_PREVIEW scenario_id= %d\n", scenario_id);
		preview(image_window, sensor_config_data);
		break;
	case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		LOG_INF("control():MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG scenario_id= %d\n", scenario_id);
		capture(image_window, sensor_config_data);
		break;
	case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
		LOG_INF("control():MSDK_SCENARIO_ID_VIDEO_PREVIEW scenario_id= %d\n", scenario_id);
		normal_video(image_window, sensor_config_data);
		break;
	case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
		LOG_INF("control():MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO scenario_id= %d\n", scenario_id);
		hs_video(image_window, sensor_config_data);
		break;
	case MSDK_SCENARIO_ID_SLIM_VIDEO:
		LOG_INF("control():MSDK_SCENARIO_ID_SLIM_VIDEO scenario_id= %d\n", scenario_id);
		slim_video(image_window, sensor_config_data);
		break;

	case MSDK_SCENARIO_ID_CUSTOM1:
		LOG_INF("control():MSDK_SCENARIO_ID_CUSTOM1 scenario_id= %d\n", scenario_id);
		custom1(image_window, sensor_config_data);
		break;
	case MSDK_SCENARIO_ID_CUSTOM2:
		LOG_INF("MSDK_SCENARIO_ID_CUSTOM2 scenario_id= %d\n",
		scenario_id);
		custom2(image_window, sensor_config_data);
		break;
	case MSDK_SCENARIO_ID_CUSTOM3:
		LOG_INF("MSDK_SCENARIO_ID_CUSTOM3 scenario_id= %d\n",
		scenario_id);
		custom3(image_window, sensor_config_data);
		break;
	case MSDK_SCENARIO_ID_CUSTOM4:
		LOG_INF("MSDK_SCENARIO_ID_CUSTOM4 scenario_id= %d\n",
		scenario_id);
		custom4(image_window, sensor_config_data);
		break;
	case MSDK_SCENARIO_ID_CUSTOM5:
		LOG_INF("MSDK_SCENARIO_ID_CUSTOM5 scenario_id= %d\n",
		scenario_id);
		custom5(image_window, sensor_config_data);
		break;
	default:
		pr_err("Error ScenarioId setting");
		preview(image_window, sensor_config_data);
		return ERROR_INVALID_SCENARIO_ID;
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
	LOG_INF("enable = %d, framerate = %d\n", enable, framerate);

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

		imgsensor.frame_length =
		imgsensor_info.custom2.framelength + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);

		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
		break;
	case MSDK_SCENARIO_ID_CUSTOM3:
		LOG_INF(
		"MSDK_SCENARIO_ID_CUSTOM3 scenario_id= %d\n",
			scenario_id);
		frame_length = imgsensor_info.custom3.pclk
			/ framerate * 10 / imgsensor_info.custom3.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line =
			(frame_length > imgsensor_info.custom3.framelength)
		? (frame_length - imgsensor_info.custom3.framelength) : 0;

		imgsensor.frame_length =
		imgsensor_info.custom3.framelength + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
		break;
	case MSDK_SCENARIO_ID_CUSTOM4:
		LOG_INF(
		"MSDK_SCENARIO_ID_CUSTOM4 scenario_id= %d\n",
			scenario_id);
		frame_length = imgsensor_info.custom4.pclk
			/ framerate * 10 / imgsensor_info.custom4.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line =
			(frame_length > imgsensor_info.custom4.framelength)
		? (frame_length - imgsensor_info.custom4.framelength) : 0;

		imgsensor.frame_length =
		imgsensor_info.custom4.framelength + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
		break;
	case MSDK_SCENARIO_ID_CUSTOM5:
		LOG_INF(
		"MSDK_SCENARIO_ID_CUSTOM5 scenario_id= %d\n",
			scenario_id);
		frame_length = imgsensor_info.custom5.pclk
			/ framerate * 10 / imgsensor_info.custom5.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line =
			(frame_length > imgsensor_info.custom5.framelength)
		? (frame_length - imgsensor_info.custom5.framelength) : 0;

		imgsensor.frame_length =
		imgsensor_info.custom5.framelength + imgsensor.dummy_line;
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
		pr_err("error scenario_id = %d, we use preview scenario\n", scenario_id);
		break;
	}
	return ERROR_NONE;
}


static kal_uint32 get_default_framerate_by_scenario(
	enum MSDK_SCENARIO_ID_ENUM scenario_id, MUINT32 *framerate)
{
	LOG_INF("scenario_id = %d\n", scenario_id);

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
		LOG_INF("custom1 *framerate = %d\n", *framerate);
		break;
	case MSDK_SCENARIO_ID_CUSTOM2:
		*framerate = imgsensor_info.custom2.max_framerate;
		LOG_INF("custom2 *framerate = %d\n", *framerate);
		break;
	case MSDK_SCENARIO_ID_CUSTOM3:
		*framerate = imgsensor_info.custom3.max_framerate;
		LOG_INF("custom3 *framerate = %d\n", *framerate);
		break;
	case MSDK_SCENARIO_ID_CUSTOM4:
		*framerate = imgsensor_info.custom4.max_framerate;
		LOG_INF("custom4 *framerate = %d\n", *framerate);
		break;
	case MSDK_SCENARIO_ID_CUSTOM5:
		*framerate = imgsensor_info.custom5.max_framerate;
		LOG_INF("custom5 *framerate = %d\n", *framerate);
		break;
	default:
		break;
	}

	return ERROR_NONE;
}

static kal_uint32 set_test_pattern_mode(kal_bool enable)
{
	LOG_INF("enable: %d\n", enable);

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

	return ERROR_NONE;
}
static kal_uint32 get_sensor_temperature(void)
{
	UINT8 temperature;
	INT32 temperature_convert;

	temperature = read_cmos_sensor_8(0x013a);

	if (temperature <= 0x78)
		temperature_convert = temperature;
	else
		temperature_convert = -1;

	LOG_INF("temp_c(%d), read_reg(%d), enable %d\n", temperature_convert, temperature, read_cmos_sensor_8(0x0138));

	return temperature_convert;
}

#if defined(CONFIG_TRAN_CAMERA_SYNC_AWB_TO_KERNEL)
static void set_awbgain(kal_uint32 g_gain, kal_uint32 r_gain, kal_uint32 b_gain)
{
	kal_uint32 r_gain_int = 0x0;
	kal_uint32 b_gain_int = 0x0;

	r_gain_int = r_gain / 2;
	b_gain_int = b_gain / 2;

	LOG_INF("%s r_gain=0x%x , b_gain=0x%x\n", __func__, r_gain, b_gain);

	write_cmos_sensor(0x0D82, r_gain_int);
	write_cmos_sensor(0x0D86, b_gain_int);
}
#endif

static kal_uint32 feature_control(MSDK_SENSOR_FEATURE_ENUM feature_id,
				  UINT8 *feature_para, UINT32 *feature_para_len)
{
	UINT16 *feature_return_para_16	= (UINT16 *) feature_para;
	UINT16 *feature_data_16			= (UINT16 *) feature_para;
	UINT32 *feature_return_para_32	= (UINT32 *) feature_para;
	UINT32 *feature_data_32			= (UINT32 *) feature_para;
	INT32 *feature_return_para_i32	= (INT32 *) feature_para;

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
		*feature_return_para_16++	= imgsensor.line_length;
		*feature_return_para_16		= imgsensor.frame_length;
		*feature_para_len			= 4;
		break;
	case SENSOR_FEATURE_GET_PIXEL_CLOCK_FREQ:
		LOG_INF("SENSOR_FEATURE_GET_PIXEL_CLOCK_FREQ = %d\n", feature_id);
		*feature_return_para_32 = imgsensor.pclk;
		*feature_para_len		= 4;
		break;
	case SENSOR_FEATURE_SET_ESHUTTER:
		pr_debug("SENSOR_FEATURE_SET_ESHUTTER = %d\n", feature_id);
		set_shutter(*feature_data);
		break;
	case SENSOR_FEATURE_SET_NIGHTMODE:
		LOG_INF("SENSOR_FEATURE_SET_NIGHTMODE = %d\n", feature_id);
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
		LOG_INF("SENSOR_FEATURE_SET_TEST_PATTERN = %d\n", feature_id);
		set_test_pattern_mode((BOOL) (*feature_data));
		break;

	case SENSOR_FEATURE_GET_TEST_PATTERN_CHECKSUM_VALUE:
		LOG_INF("SENSOR_FEATURE_GET_TEST_PATTERN_CHECKSUM_VALUE = %d\n", feature_id);
		*feature_return_para_32	= imgsensor_info.checksum_value;
		*feature_para_len		= 4;
		break;
	case SENSOR_FEATURE_SET_FRAMERATE:
		LOG_INF("SENSOR_FEATURE_SET_FRAMERATE = %d\n", feature_id);
		LOG_INF("current fps :%d\n", *feature_data_32);
		spin_lock(&imgsensor_drv_lock);
		imgsensor.current_fps = (UINT16)*feature_data_32;
		spin_unlock(&imgsensor_drv_lock);
		break;
	case SENSOR_FEATURE_SET_HDR:
		LOG_INF("SENSOR_FEATURE_SET_HDR = %d\n", feature_id);
		LOG_INF("ihdr enable :%d\n", *feature_data_32);
		spin_lock(&imgsensor_drv_lock);
		imgsensor.ihdr_mode = (UINT8)*feature_data_32;
		spin_unlock(&imgsensor_drv_lock);
		break;

	case SENSOR_FEATURE_GET_CROP_INFO:
		LOG_INF("SENSOR_FEATURE_GET_CROP_INFO = %d\n", feature_id);
		LOG_INF("SENSOR_FEATURE_GET_CROP_INFO scenarioId:%d\n", (UINT32) *feature_data);
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
		case MSDK_SCENARIO_ID_CUSTOM3:
			LOG_INF("MSDK_SCENARIO_ID_CUSTOM3 = %d\n", feature_id);
			memcpy((void *)wininfo,
			(void *)&imgsensor_winsize_info[7],
			sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_CUSTOM4:
			LOG_INF("MSDK_SCENARIO_ID_CUSTOM4 = %d\n", feature_id);
			memcpy((void *)wininfo,
			(void *)&imgsensor_winsize_info[8],
			sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_CUSTOM5:
			LOG_INF("MSDK_SCENARIO_ID_CUSTOM5 = %d\n", feature_id);
			memcpy((void *)wininfo,
			(void *)&imgsensor_winsize_info[9],
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
			case MSDK_SCENARIO_ID_CUSTOM3:
			case MSDK_SCENARIO_ID_CUSTOM4:
			case MSDK_SCENARIO_ID_CUSTOM5:
			case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
			case MSDK_SCENARIO_ID_SLIM_VIDEO:
			case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
				pr_info(
				"MSDK_SCENARIO_ID_CAMERA_PREVIEW:%d\n",
				MSDK_SCENARIO_ID_CAMERA_PREVIEW);
				imgsensor_pd_info.i4BlockNumX = 250;
				imgsensor_pd_info.i4BlockNumY = 186;
				memcpy((void *)PDAFinfo,
				(void *)&imgsensor_pd_info,
				sizeof(struct SET_PD_BLOCK_INFO_T));
				break;
			case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
				pr_info(
				"MSDK_SCENARIO_ID_CAMERA_PREVIEW:%d\n",
				MSDK_SCENARIO_ID_CAMERA_PREVIEW);
				imgsensor_pd_info.i4BlockNumX = 250;
				imgsensor_pd_info.i4BlockNumY = 140;
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
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) = 1;
			break;
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			/* video & capture use same setting */
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) = 1;
			break;
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) = 0;
			break;
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) = 0;
			break;
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) = 1;
			break;
		case MSDK_SCENARIO_ID_CUSTOM1:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) = 0;
			break;
		case MSDK_SCENARIO_ID_CUSTOM2:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) = 0;
			break;
		case MSDK_SCENARIO_ID_CUSTOM3:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) = 0;
			break;
		case MSDK_SCENARIO_ID_CUSTOM4:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) = 0;
			break;
		case MSDK_SCENARIO_ID_CUSTOM5:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) = 0;
			break;
		default:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) = 0;
			break;
		}
		break;
	case SENSOR_FEATURE_GET_VC_INFO:
		LOG_INF("SENSOR_FEATURE_GET_VC_INFO %d\n",
			(UINT16) *feature_data);

		pvcinfo =
		(struct SENSOR_VC_INFO_STRUCT *) (uintptr_t) (*(feature_data + 1));

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
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
			memcpy((void *)pvcinfo, (void *)&SENSOR_VC_INFO[0],
				   sizeof(struct SENSOR_VC_INFO_STRUCT));
			break;
		default:
			break;
		}
		break;
	case SENSOR_FEATURE_SET_IHDR_SHUTTER_GAIN:
	LOG_INF("SENSOR_FEATURE_SET_IHDR_SHUTTER_GAIN = %d\n", feature_id);
		LOG_INF("SENSOR_SET_SENSOR_IHDR LE=%d, SE=%d, Gain=%d\n",
			(UINT16) *feature_data,
			(UINT16) *(feature_data + 1),
			(UINT16) *(feature_data + 2));

/* ihdr_write_shutter_gain(	(UINT16)*feature_data,
 *				(UINT16)*(feature_data+1),
 * 				(UINT16)*(feature_data+2));
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
			set_shutter_frame_length((UINT16)*feature_data,
					(UINT16)*(feature_data+1));
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
		*feature_return_para_i32	= get_sensor_temperature();
		*feature_para_len			= 4;
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
		case MSDK_SCENARIO_ID_CUSTOM3:
			LOG_INF("MSDK_SCENARIO_ID_CUSTOM3 = %d\n", *feature_data);
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
			(imgsensor_info.custom3.pclk /
			(imgsensor_info.custom3.linelength - 80)) *
			imgsensor_info.custom3.grabwindow_width;
			break;
		case MSDK_SCENARIO_ID_CUSTOM4:
			LOG_INF("MSDK_SCENARIO_ID_CUSTOM4 = %d\n", *feature_data);
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
			(imgsensor_info.custom4.pclk /
			(imgsensor_info.custom4.linelength - 80)) *
			imgsensor_info.custom4.grabwindow_width;
			break;
		case MSDK_SCENARIO_ID_CUSTOM5:
			LOG_INF("MSDK_SCENARIO_ID_CUSTOM5 = %d\n", *feature_data);
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
			(imgsensor_info.custom5.pclk /
			(imgsensor_info.custom5.linelength - 80)) *
			imgsensor_info.custom5.grabwindow_width;
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
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
			LOG_INF("MSDK_SCENARIO_ID_CAMERA_PREVIEW = %d\n", *feature_data);
		default:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) = imgsensor_info.pre.mipi_pixel_rate;
			break;
		}
		break;
	case SENSOR_FEATURE_GET_BINNING_TYPE:
		switch (*(feature_data + 1)) {
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
			*feature_return_para_32 = 1;
			break;
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		case MSDK_SCENARIO_ID_CUSTOM1:
		case MSDK_SCENARIO_ID_CUSTOM2:
		case MSDK_SCENARIO_ID_CUSTOM3:
		case MSDK_SCENARIO_ID_CUSTOM4:
		case MSDK_SCENARIO_ID_CUSTOM5:
		default:
			*feature_return_para_32 = 2;
			break;
		}
		LOG_INF(
		"SENSOR_FEATURE_GET_BINNING_TYPE AE_binning_type:%d,\n",
		*feature_return_para_32);
		*feature_para_len = 4;
		break;
/*
 *	case SENSOR_FEATURE_GET_AWB_REQ_BY_SCENARIO:
 *		*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) = 0;
 *		break;
 */
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

UINT32 S5KGM2_MIPI_RAW_SensorInit(struct SENSOR_FUNCTION_STRUCT **pfFunc)
{
	/* To Do : Check Sensor status here */
	if (pfFunc != NULL)
		*pfFunc = &sensor_func;
	return ERROR_NONE;
}
