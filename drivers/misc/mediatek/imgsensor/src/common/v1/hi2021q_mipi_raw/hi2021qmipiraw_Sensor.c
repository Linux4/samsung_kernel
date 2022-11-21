/*
 * Copyright (C) 2019 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
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
 *	 hi2021qmipiraw_Sensor.c
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

#include "hi2021qmipiraw_Sensor.h"
//#include "imgsensor_common.h"

#define PFX "HI2021Q_camera_sensor"
#define LOG_INF(format, args...)    pr_debug(PFX "[%s] " format, __FUNCTION__, ##args)
#define hi2021q_MaxGain 16
#define hi2021q_OTP_OB 64

static DEFINE_SPINLOCK(imgsensor_drv_lock);

#ifndef VENDOR_EDIT
#define VENDOR_EDIT
#endif

void wait_stream_onoff(bool stream_onoff);

static imgsensor_info_struct imgsensor_info = {
	.sensor_id = HI2021Q_SENSOR_ID,
	.checksum_value = 0xdf4593fd,

	.pre = {
		.pclk = 98150000,
		.linelength = 776,
		.framelength = 4212,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 2592,
		.grabwindow_height = 1944,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 300,
		.mipi_pixel_rate = 399100000,
	},

	.cap = {
		.pclk = 98150000,
		.linelength = 776,
		.framelength = 4212,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 2592,
		.grabwindow_height = 1944,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 300,
		.mipi_pixel_rate = 399100000,
	},

	.normal_video = {
		.pclk = 98150000,
		.linelength = 1700,
		.framelength = 1924,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 2592,
		.grabwindow_height = 1460,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 300,
		.mipi_pixel_rate = 399100000,
	},

	.hs_video = {
		.pclk = 98150000,
		.linelength = 776,
		.framelength = 1052,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 1728,
		.grabwindow_height = 972,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 1200,
		.mipi_pixel_rate = 399100000,
	},

	.slim_video = {
		.pclk = 98150000,
		.linelength = 776,
		.framelength = 4212,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 5184,
		.grabwindow_height = 3888,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 300,
		.mipi_pixel_rate = 798200000,
	},

	.custom1 = {
		.pclk = 98150000,
		.linelength = 776,
		.framelength = 2104,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 1296,
		.grabwindow_height = 972,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 600,
		.mipi_pixel_rate = 399100000,
	},

	.custom2 = {
		.pclk = 98150000,
		.linelength = 1536,
		.framelength = 2124,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 2304,
		.grabwindow_height = 1584,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 300,
		.mipi_pixel_rate = 399100000,
	},

	.custom3 = {
		.pclk = 98150000,
		.linelength = 1536,
		.framelength = 2124,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 2304,
		.grabwindow_height = 1584,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 300,
		.mipi_pixel_rate = 399100000,
	},

	.custom4 = {
		.pclk = 98150000,
		.linelength = 1536,
		.framelength = 2124,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 2304,
		.grabwindow_height = 1584,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 300,
		.mipi_pixel_rate = 399100000,
	},
	.custom5 = {
		.pclk = 98150000,
		.linelength = 776,
		.framelength = 4212,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 4224,
		.grabwindow_height = 3168,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 300,
		.mipi_pixel_rate = 798200000,
	},

	.margin = 4,
	.min_shutter = 6,
	.min_gain = 64, /*1x gain*/
	.max_gain = 1024, /*16x gain*/
	.min_gain_iso = 100,
	.gain_step = 64,
	.gain_type = 3,/*to be modify,no gain table for hynix*/
	.max_frame_length = 0xffffff,
	.ae_shut_delay_frame = 0,
	.ae_sensor_gain_delay_frame = 0, // per-frame : 0 , No perframe : 1
	.ae_ispGain_delay_frame = 2,
	.ihdr_support = 0,      //1, support; 0,not support
	.ihdr_le_firstline = 0,  //1,le first ; 0, se first
	.sensor_mode_num = 10,

	.cap_delay_frame = 2,
	.pre_delay_frame = 2,
	.video_delay_frame = 2,
	.hs_video_delay_frame = 3,    //enter high speed video  delay frame num
	.slim_video_delay_frame = 3,//enter slim video delay frame num
	.custom1_delay_frame = 3,//enter slim video delay frame num
	.custom2_delay_frame = 3,//custom2 video delay frame num
	.custom3_delay_frame = 3,//custom3 video delay frame num
	.custom4_delay_frame = 3,
	.custom5_delay_frame = 3,
	.frame_time_delay_frame = 2,

	.isp_driving_current = ISP_DRIVING_4MA,
	.sensor_interface_type = SENSOR_INTERFACE_TYPE_MIPI,
	.mipi_sensor_type = MIPI_OPHY_NCSI2,
	//0,MIPI_OPHY_NCSI2;  1,MIPI_OPHY_CSI2
	.mipi_settle_delay_mode = 1,
	//0,MIPI_SETTLEDELAY_AUTO; 1,MIPI_SETTLEDELAY_MANNUAL
	.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_4CELL_Gr,
	.mclk = 26,
	.mipi_lane_num = SENSOR_MIPI_4_LANE,
	.i2c_addr_table = {0x40, 0x42, 0xff},
	.i2c_speed = 1000,
};

static imgsensor_struct imgsensor = {
	.mirror = IMAGE_NORMAL,				//mirrorflip information
	.sensor_mode = IMGSENSOR_MODE_INIT, //IMGSENSOR_MODE enum value,record current sensor mode,such as: INIT, Preview, Capture, Video,High Speed Video, Slim Video
	.shutter = 0x0100,					//current shutter
	.gain = 0xe0,						//current gain
	.dummy_pixel = 0,					//current dummypixel
	.dummy_line = 0,					//current dummyline
	.current_fps = 300,  //full size current fps : 24fps for PIP, 30fps for Normal or ZSD
	.autoflicker_en = KAL_FALSE,  //auto flicker enable: KAL_FALSE for disable auto flicker, KAL_TRUE for enable auto flicker
	.test_pattern = KAL_FALSE,		//test pattern mode or not. KAL_TRUE for in test pattern mode, KAL_FALSE for normal output
	.enable_secure = KAL_FALSE,
	.current_scenario_id = MSDK_SCENARIO_ID_CAMERA_PREVIEW,//current scenario id
	.ihdr_en = 0,
	.i2c_write_id = 0x40,
};


/* Sensor output window information */
static struct SENSOR_WINSIZE_INFO_STRUCT imgsensor_winsize_info[10] =
{
	{ 5184, 3888,    0,    0, 5184, 3888, 2592, 1944,
	     0,    0, 2592, 1944,    0,    0, 2592, 1944 },
	{ 5184, 3888,    0,    0, 5184, 3888, 2592, 1944,
	     0,    0, 2592, 1944,    0,    0, 2592, 1944 },
	{ 5184, 3888,    0,    0, 5184, 3888, 2592, 1944,
	     0,  242, 2592, 1460,    0,    0, 2592, 1460 },
	{ 5184, 3888,    0,    0, 5184, 3888, 2592, 1944,
	   432,  486, 1728,  972,    0,    0, 1728,  972 },
	{ 5184, 3888,    0,    0, 5184, 3888, 5184, 3888,
	     0,    0, 5184, 3888,    0,    0, 5184, 3888 },
	{ 5184, 3888,    0,    0, 5184, 3888, 2592, 1944,
	   648,  486, 1296,  972,    0,    0, 1296,  972 },
	{ 5184, 3888,    0,    0, 5184, 3888, 2592, 1944,
	   144,  180, 2304, 1584,    0,    0, 2304, 1584 },
	{ 5184, 3888,    0,    0, 5184, 3888, 2592, 1944,
	   144,  180, 2304, 1584,    0,    0, 2304, 1584 },
	{ 5184, 3888,    0,    0, 5184, 3888, 2592, 1944,
	   144,  180, 2304, 1584,    0,    0, 2304, 1584 },
	{ 5184, 3888,    0,    0, 5184, 3888, 5184, 3888,
	   480,  360, 4224, 3168,    0,    0, 4224, 3168 },
};

static kal_uint16 read_cmos_sensor(kal_uint32 addr)
{
	kal_uint16 get_byte=0;
	char pu_send_cmd[2] = {(char)(addr >> 8) , (char)(addr & 0xFF) };

	//kdSetI2CSpeed(400);

	iReadRegI2CTiming(pu_send_cmd, 2, (u8 *)&get_byte, 1,
		imgsensor.i2c_write_id, imgsensor_info.i2c_speed);

	return get_byte;
}

static void write_cmos_sensor(kal_uint32 addr, kal_uint32 para)
{
	char pu_send_cmd[4] = {(char)(addr >> 8), (char)(addr & 0xFF), (char)(para >> 8),(char)(para & 0xFF)};
	//kdSetI2CSpeed(400);
	iWriteRegI2CTiming(pu_send_cmd, 4,
		imgsensor.i2c_write_id, imgsensor_info.i2c_speed);
}

static void write_cmos_sensor_8(kal_uint32 addr, kal_uint32 para)
{
	char pu_send_cmd[4] = {(char)(addr >> 8), (char)(addr & 0xFF), (char)(para & 0xFF)};
	//kdSetI2CSpeed(400);
	iWriteRegI2CTiming(pu_send_cmd, 3,
		imgsensor.i2c_write_id, imgsensor_info.i2c_speed);
}

static kal_uint8 hi2021q_otp_read_byte(kal_uint16 addr)
{
	write_cmos_sensor_8(0x030A, (addr >> 8) & 0xFF); //High address
	write_cmos_sensor_8(0x030B, addr & 0xFF); //Low address
	write_cmos_sensor_8(0x0302, 0x01);
	return (kal_uint8)read_cmos_sensor(0x0308);
}

static void set_dummy(void)
{
	LOG_INF("dummyline = %d, dummypixels = %d \n", imgsensor.dummy_line, imgsensor.dummy_pixel);
	write_cmos_sensor_8(0x0211, (imgsensor.frame_length >> 16) & 0xFF);
	write_cmos_sensor_8(0x020E, (imgsensor.frame_length >> 8) & 0xFF);
	write_cmos_sensor_8(0x020F, imgsensor.frame_length & 0xFF);
	write_cmos_sensor_8(0x0206, (imgsensor.line_length >> 8) & 0xFF);
	write_cmos_sensor_8(0x0207, imgsensor.line_length & 0xFF);
}	/*	set_dummy  */
/*
static void set_mirror_flip(kal_uint8 image_mirror)
{
	kal_uint8 itemp;

	LOG_INF("image_mirror = %d\n", image_mirror);
	itemp = read_cmos_sensor(0x000E);
	itemp &= ~0x0300;

	switch (image_mirror) {

	case IMAGE_NORMAL:
	write_cmos_sensor(0x000E, itemp);
	break;

	case IMAGE_V_MIRROR:
	write_cmos_sensor_8(0x000E, itemp | 0x0200);
	break;

	case IMAGE_H_MIRROR:
	write_cmos_sensor_8(0x000E, itemp | 0x0100);
	break;

	case IMAGE_HV_MIRROR:
	write_cmos_sensor_8(0x000E, itemp | 0x0300);
	break;
	}
}
*/
static kal_uint32 return_sensor_id(void)
{
	return ((read_cmos_sensor(0x0716) << 8) | read_cmos_sensor(0x0717));
}
static void set_max_framerate(UINT16 framerate, kal_bool min_framelength_en)
{
	//kal_int16 dummy_line;
	kal_uint32 frame_length = imgsensor.frame_length;
	//unsigned long flags;

	LOG_INF("framerate = %d, min framelength should enable = %d\n", framerate,min_framelength_en);

	frame_length = imgsensor.pclk / framerate * 10 / imgsensor.line_length;
	spin_lock(&imgsensor_drv_lock);
	imgsensor.frame_length = (frame_length > imgsensor.min_frame_length) ? frame_length : imgsensor.min_frame_length;
	imgsensor.dummy_line = imgsensor.frame_length - imgsensor.min_frame_length;

	if (imgsensor.frame_length > imgsensor_info.max_frame_length) {
		imgsensor.frame_length = imgsensor_info.max_frame_length;
		imgsensor.dummy_line = imgsensor.frame_length - imgsensor.min_frame_length;
	}
	if (min_framelength_en) {
		imgsensor.min_frame_length = imgsensor.frame_length;
	}

	spin_unlock(&imgsensor_drv_lock);
	set_dummy();
}	/*	set_max_framerate  */

static void write_shutter(kal_uint32 shutter)
{
	kal_uint16 realtime_fps = 0;

	//kal_uint32 frame_length = 0;

	/* 0x3500, 0x3501, 0x3502 will increase VBLANK to get exposure larger than frame exposure */
	/* AE doesn't update sensor gain at capture mode, thus extra exposure lines must be updated here. */

	// OV Recommend Solution
	// if shutter bigger than frame_length, should extend frame length first
	LOG_INF("shutter %d line %d\n", shutter, __LINE__);
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
		realtime_fps = imgsensor.pclk * 10 / (imgsensor.line_length * imgsensor.frame_length);
		if (realtime_fps >= 297 && realtime_fps <= 305) {
			set_max_framerate(296, 0);
		} else if (realtime_fps >= 147 && realtime_fps <= 150) {
			set_max_framerate(146, 0);
		} else {
			write_cmos_sensor_8(0x0211,
				(imgsensor.frame_length >> 16) & 0xFF);
			write_cmos_sensor_8(0x020E,
				(imgsensor.frame_length >> 8) & 0xFF);
			write_cmos_sensor_8(0x020F,
				imgsensor.frame_length & 0xFF);
		}
	} else {
		// Extend frame length
		write_cmos_sensor_8(0x0211,
			(imgsensor.frame_length >> 16) & 0xFF);
		write_cmos_sensor_8(0x020E,
			(imgsensor.frame_length >> 8) & 0xFF);
		write_cmos_sensor_8(0x020F,
			imgsensor.frame_length & 0xFF);
	}

	// Update Shutter
	write_cmos_sensor_8(0x020D, (shutter >> 16) & 0xFF);
	write_cmos_sensor_8(0x020A, (shutter >> 8) & 0xFF);
	write_cmos_sensor_8(0x020B, shutter & 0xFF);
	LOG_INF("shutter =%d, framelength =%d", shutter,imgsensor.frame_length);

}	/* write_shutter  */

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

	LOG_INF("set_shutter");

	spin_lock_irqsave(&imgsensor_drv_lock, flags);
	imgsensor.shutter = shutter;
	spin_unlock_irqrestore(&imgsensor_drv_lock, flags);

	write_shutter(shutter);
}	/*	set_shutter */

/*************************************************************************
 * FUNCTION
 *	set_shutter_frame_length
 *
 * DESCRIPTION
 *	for frame & 3A sync
 *
 *************************************************************************/
static void set_shutter_frame_length(kal_uint16 shutter, kal_uint16 frame_length)
{
	kal_uint16 realtime_fps = 0;
	kal_int32 dummy_line = 0;

	spin_lock(&imgsensor_drv_lock);
	/* Change frame time */
	if (frame_length > 1)
		dummy_line = frame_length - imgsensor.frame_length;

	imgsensor.frame_length = imgsensor.frame_length + dummy_line;

	if (shutter > imgsensor.frame_length - imgsensor_info.margin)
		imgsensor.frame_length = shutter + imgsensor_info.margin;

	if (imgsensor.frame_length > imgsensor_info.max_frame_length)
		imgsensor.frame_length = imgsensor_info.max_frame_length;
	spin_unlock(&imgsensor_drv_lock);
	shutter = (shutter < imgsensor_info.min_shutter)
			? imgsensor_info.min_shutter : shutter;
	shutter =
	(shutter > (imgsensor_info.max_frame_length - imgsensor_info.margin))
		? (imgsensor_info.max_frame_length - imgsensor_info.margin)
		: shutter;

	if (imgsensor.autoflicker_en) {
		realtime_fps = imgsensor.pclk * 10 / (imgsensor.line_length * imgsensor.frame_length);
		if (realtime_fps >= 297 && realtime_fps <= 305) {
			set_max_framerate(296, 0);
		} else if (realtime_fps >= 147 && realtime_fps <= 150) {
			set_max_framerate(146, 0);
		} else {
			write_cmos_sensor_8(0x0211,
				(imgsensor.frame_length >> 16) & 0xFF);
			write_cmos_sensor_8(0x020E,
				(imgsensor.frame_length >> 8) & 0xFF);
			write_cmos_sensor_8(0x020F,
				imgsensor.frame_length & 0xFF);
		}
	} else {
		// Extend frame length
		write_cmos_sensor_8(0x0211,
			(imgsensor.frame_length >> 16) & 0xFF);
		write_cmos_sensor_8(0x020E,
			(imgsensor.frame_length >> 8) & 0xFF);
		write_cmos_sensor_8(0x020F,
			imgsensor.frame_length & 0xFF);
	}

	// Update Shutter
	write_cmos_sensor_8(0x020D, (shutter >> 16) & 0xFF);
	write_cmos_sensor_8(0x020A, (shutter >> 8) & 0xFF);
	write_cmos_sensor_8(0x020B, shutter & 0xFF);
	LOG_INF("shutter =%d, framelength =%d", shutter,imgsensor.frame_length);

}	/* set_shutter_frame_length */


static kal_uint16 gain2reg(const kal_uint16 gain)
{
	kal_uint16 reg_gain = 0x0000;
	reg_gain = gain / 4 - 16;

	return (kal_uint16)reg_gain;
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

	/* 0x350A[0:1], 0x350B[0:7] AGC real gain */
	/* [0:3] = N meams N /16 X    */
	/* [4:9] = M meams M X         */
	/* Total gain = M + N /16 X   */

	if (gain < imgsensor_info.min_gain || gain > imgsensor_info.max_gain) {
		pr_debug("Error gain setting");

		if (gain < imgsensor_info.min_gain)
			gain = imgsensor_info.min_gain;
		else if (gain > imgsensor_info.max_gain)
			gain = imgsensor_info.max_gain;
	}

	reg_gain = gain2reg(gain);
	spin_lock(&imgsensor_drv_lock);
	imgsensor.gain = reg_gain;
	spin_unlock(&imgsensor_drv_lock);
	LOG_INF("gain = %d , reg_gain = 0x%x\n ", gain, reg_gain);

	write_cmos_sensor_8(0x0213, reg_gain & 0xFF);

	return gain;

}	/*	set_gain  */

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
static void night_mode(kal_bool enable)
{
/*No Need to implement this function*/
}	/*	night_mode	*/
static kal_uint32 streaming_control(kal_bool enable)
{
	LOG_INF("streaming_enable(0=Sw Standby,1=streaming): %d\n", enable);
	if (enable) {
		write_cmos_sensor(0x0b00, 0x0100);
		mdelay(1);
	} else {
		write_cmos_sensor(0x0b00, 0x0000);
	}
	return ERROR_NONE;
}

#define I2C_BUFFER_LEN 225	/* trans# max is 255, each 3 bytes */
static kal_uint16 table_write_cmos_sensor(kal_uint16 *para, kal_uint32 len)
{
	char puSendCmd[I2C_BUFFER_LEN];
	kal_uint32 tosend, IDX;
	kal_uint16 addr = 0, addr_last = 0, data;
	kal_uint8 ret = 0;
	kal_uint8 i = 0;
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

		/* Write when remain buffer size is less than 4 bytes or reach end of data */
	if ((I2C_BUFFER_LEN - tosend) < 4 || IDX == len || addr != addr_last) {
		for (i = 0; i < 3; i++) {
			LOG_INF("i = %d\n", i);
			ret = iBurstWriteReg_multi(puSendCmd,
						tosend,
						imgsensor.i2c_write_id,
						4,
						imgsensor_info.i2c_speed);
			LOG_INF("ret = %d\n", ret);
			if (ret == 0)
				break;
		}
			tosend = 0;
		}
	}
	return 0;
}

static kal_uint16 addr_data_pair_global[] = {
	0x0790, 0x0100,
	0x2000, 0x1001,
	0x2002, 0x0000,
	0x2006, 0x40B2,
	0x2008, 0xB00E,
	0x200A, 0x843A,
	0x200C, 0x4130,
	0x200E, 0x425F,
	0x2010, 0x0360,
	0x2012, 0xF35F,
	0x2014, 0xD25F,
	0x2016, 0x85E0,
	0x2018, 0x934F,
	0x201A, 0x2407,
	0x201C, 0x421F,
	0x201E, 0x731A,
	0x2020, 0xC312,
	0x2022, 0x100F,
	0x2024, 0x822F,
	0x2026, 0x4F82,
	0x2028, 0x86DE,
	0x202A, 0x1292,
	0x202C, 0xC00A,
	0x202E, 0x4130,
	0x026A, 0xFFFF,
	0x026C, 0x00FF,
	0x026E, 0x0000,
	0x0360, 0x0E8E,
	0x036E, 0xF092,
	0x0370, 0x002A,
	0x0412, 0x01EB,
	0x0440, 0x0018,
	0x0604, 0x8048,
	0x0676, 0x07FF,
	0x0678, 0x0002,
	0x06A8, 0x01A8,
	0x06AA, 0x015E,
	0x06AC, 0x0041,
	0x06AE, 0x03FC,
	0x06B4, 0x3FFF,
	0x0524, 0x5050,
	0x0526, 0x4A4A,
	0x0F00, 0x0000,
	0x0B02, 0x0000,
	0x1102, 0x0008,
	0x11C2, 0x0400,
	0x0904, 0x0002,
	0x0906, 0x0222,
	0x0A04, 0xB4C5,
	0x0A06, 0xC400,
	0x0A08, 0xB88F,
	0x0A0A, 0xA289,
	0x0A0E, 0xFEC0,
	0x0A10, 0xB040,
	0x0A12, 0x0700,
	0x0A18, 0x0350,
	0x0A1A, 0x7E34,
	0x0A1E, 0x0013,
	0x0780, 0x010E,
	0x1202, 0x1E00,
	0x1204, 0xD700,
	0x1210, 0x8028,
	0x1216, 0xA0A0,
	0x1218, 0x00A0,
	0x121A, 0x0000,
	0x121C, 0x4128,
	0x121E, 0x0000,
	0x1220, 0x0000,
	0x1222, 0x28FA,
	0x105E, 0x00C9,
	0x1060, 0x0F08,
	0x1068, 0x0000,
	0x36F0, 0x0045,
	0x36F2, 0x0045,
	0x36F4, 0x0045,
	0x36F6, 0x0048,
	0x36F8, 0x004E,
	0x36FA, 0x0054,
	0x36FC, 0x005C,
	0x36FE, 0x0062,
	0x3700, 0x006A,
	0x3702, 0x006E,
	0x3704, 0x0073,
	0x3706, 0x007C,
	0x3708, 0x0082,
	0x370A, 0x0087,
	0x370C, 0x008C,
	0x370E, 0x0090,
	0x3710, 0x0096,
	0x3712, 0x0098,
	0x3714, 0x00A0,
	0x3716, 0x00A8,
	0x3718, 0x00B0,
	0x371A, 0x00BE,
	0x371C, 0x00C8,
	0x371E, 0x00D2,
	0x3720, 0x00DC,
	0x3722, 0x00E1,
	0x3724, 0x00F0,
	0x3726, 0x00FA,
	0x3728, 0x0104,
	0x372A, 0x010E,
	0x372C, 0x0118,
	0x372E, 0x0122,
	0x1958, 0x0040,
	0x195A, 0x004E,
	0x195C, 0x0097,
	0x195E, 0x0221,
	0x1980, 0x0080,
	0x1982, 0x0030,
	0x1984, 0x2018,
	0x1986, 0x0008,
	0x1988, 0x0120,
	0x198A, 0x0000,
	0x198C, 0x0500,
	0x198E, 0x0000,
	0x1990, 0x2000,
	0x1992, 0x0000,
	0x1994, 0x1800,
	0x1996, 0x0002,
	0x1962, 0x0040,
	0x1964, 0x004E,
	0x1966, 0x0097,
	0x1968, 0x0221,
	0x19C0, 0x0080,
	0x19C2, 0x0030,
	0x19C4, 0x2018,
	0x19C6, 0x0008,
	0x19C8, 0x0120,
	0x19CA, 0x0000,
	0x19CC, 0x0500,
	0x19CE, 0x0000,
	0x19D0, 0x2000,
	0x19D2, 0x0000,
	0x19D4, 0x1800,
	0x19D6, 0x0002,
	0x196C, 0x0040,
	0x196E, 0x004E,
	0x1970, 0x0097,
	0x1972, 0x0221,
	0x1A00, 0x0040,
	0x1A02, 0x0000,
	0x1A04, 0x2018,
	0x1A06, 0x0000,
	0x1A08, 0x00C0,
	0x1A0A, 0x0000,
	0x1A0C, 0x0640,
	0x1A0E, 0x0000,
	0x1A10, 0x1000,
	0x1A12, 0x0000,
	0x1A14, 0x2000,
	0x1A16, 0x0002,
	0x1976, 0x0040,
	0x1978, 0x004E,
	0x197A, 0x0097,
	0x197C, 0x0221,
	0x1A40, 0x0040,
	0x1A42, 0x0000,
	0x1A44, 0x2018,
	0x1A46, 0x0000,
	0x1A48, 0x00C0,
	0x1A4A, 0x0000,
	0x1A4C, 0x0440,
	0x1A4E, 0x0000,
	0x1A50, 0x1000,
	0x1A52, 0x0000,
	0x1A54, 0x2000,
	0x1A56, 0x0002,
	0x0600, 0x1133,
	0x06CC, 0x00FF,
	0x1960, 0x03FE,
	0x196A, 0x03FE,
	0x1974, 0x03FE,
	0x197E, 0x03FE,
	0x19BC, 0x2000,
	0x19FC, 0x2000,
	0x1A3C, 0x2000,
	0x1A7C, 0x2000,
	0x027E, 0x0100,
};

static void sensor_init(void)
{
	int ret;
#ifdef IMGSENSOR_HW_PARAM
	struct cam_hw_param *hw_param = NULL;
#endif

	LOG_INF("Enter init v16\n");
	ret = table_write_cmos_sensor(addr_data_pair_global,
		sizeof(addr_data_pair_global)
			/ sizeof(kal_uint16));

#ifdef IMGSENSOR_HW_PARAM
	if(ret != 0) {
		imgsensor_sec_get_hw_param(&hw_param, SENSOR_POSITION_FRONT);
		if (hw_param)
			hw_param->i2c_sensor_err_cnt++;
	}
#endif

	LOG_INF("E init done\n");
}

static kal_uint16 addr_data_pair_preview[] = {
	0x0B00, 0x0000,
	0x0204, 0x0408,
	0x0206, 0x0308,
	0x020A, 0x1070,
	0x020E, 0x1074,
	0x0220, 0x0008,
	0x0222, 0x0FA0,
	0x0224, 0x002C,
	0x022A, 0x0016,
	0x022C, 0x0FBE,
	0x022E, 0x0F62,
	0x0234, 0x2222,
	0x0236, 0x2222,
	0x0238, 0x2222,
	0x023A, 0x2222,
	0x0250, 0x0000,
	0x0254, 0x0000,
	0x0260, 0x0000,
	0x0262, 0x0E00,
	0x0268, 0x0127,
	0x0400, 0x0E10,
	0x040A, 0x0000,
	0x040C, 0x1458,
	0x0D00, 0x4000,
	0x0D28, 0x0000,
	0x0D2A, 0x0A2F,
	0x0602, 0x3112,
	0x0F04, 0x0008,
	0x0F06, 0x0002,
	0x0F08, 0x0011,
	0x0F0A, 0x2233,
	0x0F0C, 0x4455,
	0x0F0E, 0x6677,
	0x0B04, 0x0094,
	0x0B0A, 0x0408,
	0x0B12, 0x0A20,
	0x0B14, 0x0798,
	0x0B20, 0x0140,
	0x0B30, 0x0000,
	0x1108, 0x0402,
	0x1118, 0x0000,
	0x0C00, 0x0021,
	0x0C14, 0x0008,
	0x0C16, 0x0002,
	0x0C18, 0x0A20,
	0x0C1A, 0x0800,
	0x0708, 0xEF83,
	0x070C, 0x0000,
	0x0732, 0x0300,
	0x0736, 0x0097,
	0x0738, 0x0004,
	0x0742, 0x0300,
	0x0746, 0x0133,
	0x0748, 0x0003,
	0x074C, 0x0100,
	0x1200, 0x4946,
	0x1206, 0x1800,
	0x122E, 0x0146,
	0x1010, 0x018F,
	0x1012, 0x002F,
	0x1020, 0xC105,
	0x1022, 0x0512,
	0x1024, 0x0305,
	0x1026, 0x0707,
	0x1028, 0x0C06,
	0x102A, 0x080A,
	0x102C, 0x0B0A,
	0x107A, 0x01A6,
	0x1600, 0x0400,
	0x067A, 0x0E0E,
	0x067C, 0x0E0E,
	0x06DE, 0x0E0E,
	0x06E0, 0x0E0E,
	0x06E4, 0x8900,
	0x06E6, 0x8900,
	0x06E8, 0x8900,
	0x06EA, 0x8900,
};

static void preview_setting(void)
{
	LOG_INF("E preview\n");
	table_write_cmos_sensor(addr_data_pair_preview,
		sizeof(addr_data_pair_preview)
			/ sizeof(kal_uint16));
}

static void capture_setting(kal_uint16 currefps)
{
	LOG_INF("E capture, currefps = %d\n", currefps);

	table_write_cmos_sensor(addr_data_pair_preview,
		sizeof(addr_data_pair_preview)
			/ sizeof(kal_uint16));
}

static kal_uint16 addr_data_pair_normal_video[] = {
	0x0B00, 0x0000,
	0x0204, 0x0408,
	0x0206, 0x06A4,
	0x020A, 0x0780,
	0x020E, 0x0784,
	0x0220, 0x0008,
	0x0222, 0x0FA0,
	0x0224, 0x0210,
	0x022A, 0x0016,
	0x022C, 0x0FAE,
	0x022E, 0x0D7E,
	0x0234, 0x2222,
	0x0236, 0x2222,
	0x0238, 0x2222,
	0x023A, 0x2222,
	0x0250, 0x0000,
	0x0254, 0x0000,
	0x0260, 0x0000,
	0x0262, 0x0E00,
	0x0268, 0x008B,
	0x0400, 0x0E10,
	0x040A, 0x0000,
	0x040C, 0x1458,
	0x0D00, 0x4000,
	0x0D28, 0x0000,
	0x0D2A, 0x0A2F,
	0x0602, 0x3112,
	0x0F04, 0x0008,
	0x0F06, 0x0002,
	0x0F08, 0x0011,
	0x0F0A, 0x2233,
	0x0F0C, 0x4455,
	0x0F0E, 0x6677,
	0x0B04, 0x0094,
	0x0B0A, 0x0408,
	0x0B12, 0x0A20,
	0x0B14, 0x05B4,
	0x0B20, 0x0140,
	0x0B30, 0x0000,
	0x1108, 0x0002,
	0x1118, 0x029C,
	0x0C00, 0x0021,
	0x0C14, 0x0008,
	0x0C16, 0x0002,
	0x0C18, 0x0A20,
	0x0C1A, 0x0600,
	0x0708, 0xEF83,
	0x070C, 0x0000,
	0x0732, 0x0300,
	0x0736, 0x0097,
	0x0738, 0x0004,
	0x0742, 0x0300,
	0x0746, 0x0133,
	0x0748, 0x0003,
	0x074C, 0x0100,
	0x1200, 0x4946,
	0x1206, 0x1800,
	0x122E, 0x0146,
	0x1010, 0x03DA,
	0x1012, 0x027A,
	0x1020, 0xC105,
	0x1022, 0x0512,
	0x1024, 0x0305,
	0x1026, 0x0707,
	0x1028, 0x0C06,
	0x102A, 0x080A,
	0x102C, 0x0B0A,
	0x107A, 0x03F1,
	0x1600, 0x0400,
	0x067A, 0x0E0E,
	0x067C, 0x0E0E,
	0x06DE, 0x0E0E,
	0x06E0, 0x0E0E,
	0x06E4, 0x8900,
	0x06E6, 0x8900,
	0x06E8, 0x8900,
	0x06EA, 0x8900,
};

static void normal_video_setting(void)
{
	LOG_INF("%s\n", __func__);

	table_write_cmos_sensor(addr_data_pair_normal_video,
		sizeof(addr_data_pair_normal_video)
			/ sizeof(kal_uint16));
};

static kal_uint16 addr_data_pair_hs_video[] = {
	0x0B00, 0x0000,
	0x0204, 0x0408,
	0x0206, 0x0308,
	0x020A, 0x0418,
	0x020E, 0x041C,
	0x0220, 0x0008,
	0x0222, 0x0FA0,
	0x0224, 0x03F8,
	0x022A, 0x0016,
	0x022C, 0x0FBE,
	0x022E, 0x0B96,
	0x0234, 0x2222,
	0x0236, 0x2222,
	0x0238, 0x2222,
	0x023A, 0x2222,
	0x0250, 0x0000,
	0x0254, 0x0000,
	0x0260, 0x0000,
	0x0262, 0x0E00,
	0x0268, 0x0127,
	0x0400, 0x0E10,
	0x040A, 0x0000,
	0x040C, 0x1458,
	0x0D00, 0x4000,
	0x0D28, 0x0000,
	0x0D2A, 0x0A2F,
	0x0602, 0x3112,
	0x0F04, 0x01B8,
	0x0F06, 0x0002,
	0x0F08, 0x0011,
	0x0F0A, 0x2233,
	0x0F0C, 0x4455,
	0x0F0E, 0x6677,
	0x0B04, 0x0094,
	0x0B0A, 0x0408,
	0x0B12, 0x06C0,
	0x0B14, 0x03CC,
	0x0B20, 0x0140,
	0x0B30, 0x0000,
	0x1108, 0x0002,
	0x1118, 0x0540,
	0x0C00, 0x0021,
	0x0C14, 0x01B8,
	0x0C16, 0x0002,
	0x0C18, 0x06C0,
	0x0C1A, 0x0400,
	0x0708, 0xEF83,
	0x070C, 0x0000,
	0x0732, 0x0300,
	0x0736, 0x0097,
	0x0738, 0x0004,
	0x0742, 0x0300,
	0x0746, 0x0133,
	0x0748, 0x0003,
	0x074C, 0x0100,
	0x1200, 0x4946,
	0x1206, 0x1800,
	0x122E, 0x0146,
	0x1010, 0x018F,
	0x1012, 0x00B6,
	0x1020, 0xC105,
	0x1022, 0x0512,
	0x1024, 0x0305,
	0x1026, 0x0707,
	0x1028, 0x0C06,
	0x102A, 0x080A,
	0x102C, 0x0B0A,
	0x107A, 0x01A6,
	0x1600, 0x0400,
	0x067A, 0x0E0E,
	0x067C, 0x0E0E,
	0x06DE, 0x0E0E,
	0x06E0, 0x0E0E,
	0x06E4, 0x8900,
	0x06E6, 0x8900,
	0x06E8, 0x8900,
	0x06EA, 0x8900,
};

static void hs_video_setting(void)
{
	LOG_INF("%s\n", __func__);

	table_write_cmos_sensor(addr_data_pair_hs_video,
		sizeof(addr_data_pair_hs_video)
			/ sizeof(kal_uint16));
};

static kal_uint16 addr_data_pair_slim_video[] = {
	0x0B00, 0x0000,
	0x0204, 0x0008,
	0x0206, 0x0308,
	0x020A, 0x1070,
	0x020E, 0x1074,
	0x0220, 0x0008,
	0x0222, 0x0FA0,
	0x0224, 0x0030,
	0x022A, 0x0017,
	0x022C, 0x0FAF,
	0x022E, 0x0F5F,
	0x0234, 0x1111,
	0x0236, 0x1111,
	0x0238, 0x1111,
	0x023A, 0x1111,
	0x0250, 0x0000,
	0x0254, 0x0000,
	0x0260, 0x0000,
	0x0262, 0x0E00,
	0x0268, 0x0127,
	0x0400, 0x0A10,
	0x040A, 0x0008,
	0x040C, 0x1450,
	0x0D00, 0x400B,
	0x0D28, 0x0008,
	0x0D2A, 0x1457,
	0x0602, 0x3712,
	0x0F04, 0x0008,
	0x0F06, 0x0000,
	0x0F08, 0x0022,
	0x0F0A, 0x1133,
	0x0F0C, 0x4466,
	0x0F0E, 0x5577,
	0x0B04, 0x0094,
	0x0B0A, 0x0008,
	0x0B12, 0x1440,
	0x0B14, 0x0F30,
	0x0B20, 0x0140,
	0x0B30, 0x0001,
	0x1108, 0x0001,
	0x1118, 0x0000,
	0x0C00, 0x0023,
	0x0C14, 0x0008,
	0x0C16, 0x0000,
	0x0C18, 0x1440,
	0x0C1A, 0x0F80,
	0x0708, 0xEF82,
	0x070C, 0x0000,
	0x0732, 0x0300,
	0x0736, 0x0097,
	0x0738, 0x0004,
	0x0742, 0x0300,
	0x0746, 0x0133,
	0x0748, 0x0003,
	0x074C, 0x0000,
	0x1200, 0x0946,
	0x1206, 0x0800,
	0x122E, 0x028C,
	0x1010, 0x0339,
	0x1012, 0x0068,
	0x1020, 0xC109,
	0x1022, 0x0922,
	0x1024, 0x030A,
	0x1026, 0x0E0C,
	0x1028, 0x130B,
	0x102A, 0x0E0A,
	0x102C, 0x190A,
	0x107A, 0x0364,
	0x1600, 0x0000,
	0x067A, 0x0404,
	0x067C, 0x0404,
	0x06DE, 0x0404,
	0x06E0, 0x0404,
	0x06E4, 0x8540,
	0x06E6, 0x8040,
	0x06E8, 0x8540,
	0x06EA, 0x8040,
};

static void slim_video_setting(void)
{
	table_write_cmos_sensor(addr_data_pair_slim_video,
		sizeof(addr_data_pair_slim_video)
			/ sizeof(kal_uint16));
};

static kal_uint16 addr_data_pair_custom1[] = {
	0x0B00, 0x0000,
	0x0204, 0x0408,
	0x0206, 0x0308,
	0x020A, 0x0418,
	0x020E, 0x0838,
	0x0220, 0x0008,
	0x0222, 0x0FA0,
	0x0224, 0x03F8,
	0x022A, 0x0016,
	0x022C, 0x0FBE,
	0x022E, 0x0B96,
	0x0234, 0x2222,
	0x0236, 0x2222,
	0x0238, 0x2222,
	0x023A, 0x2222,
	0x0250, 0x0000,
	0x0254, 0x0000,
	0x0260, 0x0000,
	0x0262, 0x0E00,
	0x0268, 0x0127,
	0x0400, 0x0E10,
	0x040A, 0x0000,
	0x040C, 0x1458,
	0x0D00, 0x4000,
	0x0D28, 0x0000,
	0x0D2A, 0x0A2F,
	0x0602, 0x3112,
	0x0F04, 0x0290,
	0x0F06, 0x0002,
	0x0F08, 0x0011,
	0x0F0A, 0x2233,
	0x0F0C, 0x4455,
	0x0F0E, 0x6677,
	0x0B04, 0x0094,
	0x0B0A, 0x0408,
	0x0B12, 0x0510,
	0x0B14, 0x03CC,
	0x0B20, 0x0140,
	0x0B30, 0x0000,
	0x1108, 0x0002,
	0x1118, 0x0540,
	0x0C00, 0x0021,
	0x0C14, 0x0290,
	0x0C16, 0x0002,
	0x0C18, 0x0520,
	0x0C1A, 0x0400,
	0x0708, 0xEF83,
	0x070C, 0x0000,
	0x0732, 0x0300,
	0x0736, 0x0097,
	0x0738, 0x0004,
	0x0742, 0x0300,
	0x0746, 0x0133,
	0x0748, 0x0003,
	0x074C, 0x0100,
	0x1200, 0x4946,
	0x1206, 0x1800,
	0x122E, 0x0146,
	0x1010, 0x018F,
	0x1012, 0x00F9,
	0x1020, 0xC105,
	0x1022, 0x0512,
	0x1024, 0x0305,
	0x1026, 0x0707,
	0x1028, 0x0C06,
	0x102A, 0x080A,
	0x102C, 0x0B0A,
	0x107A, 0x01A6,
	0x1600, 0x0400,
	0x067A, 0x0E0E,
	0x067C, 0x0E0E,
	0x06DE, 0x0E0E,
	0x06E0, 0x0E0E,
	0x06E4, 0x8900,
	0x06E6, 0x8900,
	0x06E8, 0x8900,
	0x06EA, 0x8900,
};

static void custom1_setting(void)
{
	table_write_cmos_sensor(addr_data_pair_custom1,
		sizeof(addr_data_pair_custom1) / sizeof(kal_uint16));
};

static kal_uint16 addr_data_pair_custom2[] = {
	0x0B00, 0x0000,
	0x0204, 0x0408,
	0x0206, 0x0600,
	0x020A, 0x0848,
	0x020E, 0x084C,
	0x0220, 0x0008,
	0x0222, 0x0FA0,
	0x0224, 0x0194,
	0x022A, 0x0016,
	0x022C, 0x0FAE,
	0x022E, 0x0DFA,
	0x0234, 0x2222,
	0x0236, 0x2222,
	0x0238, 0x2222,
	0x023A, 0x2222,
	0x0250, 0x0000,
	0x0254, 0x0000,
	0x0260, 0x0000,
	0x0262, 0x0E00,
	0x0268, 0x009A,
	0x0400, 0x0E10,
	0x040A, 0x0000,
	0x040C, 0x1458,
	0x0D00, 0x4000,
	0x0D28, 0x0000,
	0x0D2A, 0x0A2F,
	0x0602, 0x3112,
	0x0F04, 0x0098,
	0x0F06, 0x0002,
	0x0F08, 0x0011,
	0x0F0A, 0x2233,
	0x0F0C, 0x4455,
	0x0F0E, 0x6677,
	0x0B04, 0x0094,
	0x0B0A, 0x0408,
	0x0B12, 0x0900,
	0x0B14, 0x0630,
	0x0B20, 0x0140,
	0x0B30, 0x0000,
	0x1108, 0x0002,
	0x1118, 0x0220,
	0x0C00, 0x0021,
	0x0C14, 0x0098,
	0x0C16, 0x0002,
	0x0C18, 0x0900,
	0x0C1A, 0x0680,
	0x0708, 0xEF83,
	0x070C, 0x0000,
	0x0732, 0x0300,
	0x0736, 0x0097,
	0x0738, 0x0004,
	0x0742, 0x0300,
	0x0746, 0x0133,
	0x0748, 0x0003,
	0x074C, 0x0100,
	0x1200, 0x4546,
	0x1206, 0x1800,
	0x122E, 0x0146,
	0x1010, 0x0376,
	0x1012, 0x023D,
	0x1020, 0xC105,
	0x1022, 0x0512,
	0x1024, 0x0505,
	0x1026, 0x0607,
	0x1028, 0x0C07,
	0x102A, 0x070A,
	0x102C, 0x0D0A,
	0x107A, 0x0388,
	0x1600, 0x0400,
	0x067A, 0x0E0E,
	0x067C, 0x0E0E,
	0x06DE, 0x0E0E,
	0x06E0, 0x0E0E,
	0x06E4, 0x8900,
	0x06E6, 0x8900,
	0x06E8, 0x8900,
	0x06EA, 0x8900,
};

static void custom2_setting(void)
{
	table_write_cmos_sensor(addr_data_pair_custom2,
		sizeof(addr_data_pair_custom2) / sizeof(kal_uint16));
};

static void custom3_setting(void)
{
	table_write_cmos_sensor(addr_data_pair_custom2,
		sizeof(addr_data_pair_custom2) / sizeof(kal_uint16));
};

static void custom4_setting(void)
{
	table_write_cmos_sensor(addr_data_pair_custom2,
		sizeof(addr_data_pair_custom2) / sizeof(kal_uint16));
};

static kal_uint16 addr_data_pair_custom5[] = {
	0x0B00, 0x0000,
	0x0204, 0x0008,
	0x0206, 0x0308,
	0x020A, 0x1070,
	0x020E, 0x1074,
	0x0220, 0x0008,
	0x0222, 0x0FA0,
	0x0224, 0x0198,
	0x022A, 0x0017,
	0x022C, 0x0FAF,
	0x022E, 0x0DF7,
	0x0234, 0x1111,
	0x0236, 0x1111,
	0x0238, 0x1111,
	0x023A, 0x1111,
	0x0250, 0x0000,
	0x0254, 0x0000,
	0x0260, 0x0000,
	0x0262, 0x0E00,
	0x0268, 0x0127,
	0x0400, 0x0A10,
	0x040A, 0x0008,
	0x040C, 0x1450,
	0x0D00, 0x400B,
	0x0D28, 0x0008,
	0x0D2A, 0x1457,
	0x0602, 0x3712,
	0x0F04, 0x01E8,
	0x0F06, 0x0000,
	0x0F08, 0x0022,
	0x0F0A, 0x1133,
	0x0F0C, 0x4466,
	0x0F0E, 0x5577,
	0x0B04, 0x0094,
	0x0B0A, 0x0008,
	0x0B12, 0x1080,
	0x0B14, 0x0C60,
	0x0B20, 0x0140,
	0x0B30, 0x0001,
	0x1108, 0x0001,
	0x1118, 0x0224,
	0x0C00, 0x0023,
	0x0C14, 0x01E8,
	0x0C16, 0x0000,
	0x0C18, 0x1080,
	0x0C1A, 0x0C80,
	0x0708, 0xEF82,
	0x070C, 0x0000,
	0x0732, 0x0300,
	0x0736, 0x0097,
	0x0738, 0x0004,
	0x0742, 0x0300,
	0x0746, 0x0133,
	0x0748, 0x0003,
	0x074C, 0x0000,
	0x1200, 0x0946,
	0x1206, 0x0800,
	0x122E, 0x028C,
	0x1010, 0x0339,
	0x1012, 0x00FE,
	0x1020, 0xC109,
	0x1022, 0x0922,
	0x1024, 0x030A,
	0x1026, 0x0E0C,
	0x1028, 0x130B,
	0x102A, 0x0E0A,
	0x102C, 0x190A,
	0x107A, 0x0364,
	0x1600, 0x0000,
	0x067A, 0x0404,
	0x067C, 0x0404,
	0x06DE, 0x0404,
	0x06E0, 0x0404,
	0x06E4, 0x8540,
	0x06E6, 0x8040,
	0x06E8, 0x8540,
	0x06EA, 0x8040,
};

static void custom5_setting(void)
{
	table_write_cmos_sensor(addr_data_pair_custom5,
		sizeof(addr_data_pair_custom5) / sizeof(kal_uint16));
};


static kal_uint32 get_imgsensor_id(UINT32 *sensor_id)
{
	kal_uint8 i = 0;
	kal_uint8 retry = 2;
	LOG_INF("[get_imgsensor_id] ");

	while (imgsensor_info.i2c_addr_table[i] != 0xff) {
		spin_lock(&imgsensor_drv_lock);
		imgsensor.i2c_write_id = imgsensor_info.i2c_addr_table[i];
		spin_unlock(&imgsensor_drv_lock);
		do {
			*sensor_id = return_sensor_id();
			if (*sensor_id == imgsensor_info.sensor_id) {
				LOG_INF(
					"i2c write id: 0x%x, sensor id: 0x%x\n",
					imgsensor.i2c_write_id, *sensor_id);
				return ERROR_NONE;
			}
			LOG_INF(
				"Read sensor id fail, i2c write id: 0x%x, sensor id: 0x%x\n",
				imgsensor.i2c_write_id, *sensor_id);
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

	LOG_INF("[open]: PLATFORM:MT6735,MIPI 4LANE\n");
	LOG_INF("preview 1632*1224@30fps,360Mbps/lane; capture 3264*2448@30fps,720Mbps/lane\n");
	while (imgsensor_info.i2c_addr_table[i] != 0xff) {
		spin_lock(&imgsensor_drv_lock);
		imgsensor.i2c_write_id = imgsensor_info.i2c_addr_table[i];
		spin_unlock(&imgsensor_drv_lock);
		do {
			sensor_id = return_sensor_id();
			if (sensor_id == imgsensor_info.sensor_id) {
				LOG_INF("i2c write id: 0x%x, sensor id: 0x%x\n", imgsensor.i2c_write_id, sensor_id);
				break;
			}
			LOG_INF("open:Read sensor id fail open i2c write id: 0x%x, id: 0x%x\n", imgsensor.i2c_write_id, sensor_id);
			retry--;
		} while(retry > 0);
		i++;
		if (sensor_id == imgsensor_info.sensor_id) {
			break;
		}
		retry = 2;
	}
	if (imgsensor_info.sensor_id != sensor_id) {
		return ERROR_SENSOR_CONNECT_FAIL;
	}
	/* initail sequence write in  */

	sensor_init();

	spin_lock(&imgsensor_drv_lock);
	imgsensor.autoflicker_en= KAL_FALSE;
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
	return ERROR_NONE;
}	/*	open  */
static kal_uint32 close(void)
{
	return ERROR_NONE;
}	/*	close  */


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
	LOG_INF("E");
	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_PREVIEW;
	imgsensor.pclk = imgsensor_info.pre.pclk;
	imgsensor.line_length = imgsensor_info.pre.linelength;
	imgsensor.frame_length = imgsensor_info.pre.framelength;
	imgsensor.min_frame_length = imgsensor_info.pre.framelength;
	imgsensor.current_fps = imgsensor_info.pre.max_framerate;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	preview_setting();

	return ERROR_NONE;
}	/*	preview   */

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
	LOG_INF("E");
	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_CAPTURE;
	imgsensor.pclk = imgsensor_info.cap.pclk;
	imgsensor.line_length = imgsensor_info.cap.linelength;
	imgsensor.frame_length = imgsensor_info.cap.framelength;
	imgsensor.min_frame_length = imgsensor_info.cap.framelength;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	LOG_INF("Caputre fps:%d\n",imgsensor.current_fps);
	capture_setting(imgsensor.current_fps);

	return ERROR_NONE;

}	/* capture() */
static kal_uint32 normal_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
					  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("normal_video");

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_VIDEO;
	imgsensor.pclk = imgsensor_info.normal_video.pclk;
	imgsensor.line_length = imgsensor_info.normal_video.linelength;
	imgsensor.frame_length = imgsensor_info.normal_video.framelength;
	imgsensor.min_frame_length = imgsensor_info.normal_video.framelength;
	imgsensor.current_fps = 300;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	normal_video_setting();
	return ERROR_NONE;
}	/*	normal_video   */

static kal_uint32 hs_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
                      MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("hs_video\n");

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_HIGH_SPEED_VIDEO;
	imgsensor.pclk = imgsensor_info.hs_video.pclk;
	//imgsensor.video_mode = KAL_TRUE;
	imgsensor.line_length = imgsensor_info.hs_video.linelength;
	imgsensor.frame_length = imgsensor_info.hs_video.framelength;
	imgsensor.min_frame_length = imgsensor_info.hs_video.framelength;
	imgsensor.dummy_line = 0;
	imgsensor.dummy_pixel = 0;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	hs_video_setting();
	return ERROR_NONE;
}    /*    hs_video   */

static kal_uint32 slim_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
                      MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("slim_video\n");
	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_SLIM_VIDEO;
	imgsensor.pclk = imgsensor_info.slim_video.pclk;
	imgsensor.line_length = imgsensor_info.slim_video.linelength;
	imgsensor.frame_length = imgsensor_info.slim_video.framelength;
	imgsensor.min_frame_length = imgsensor_info.slim_video.framelength;
	imgsensor.dummy_line = 0;
	imgsensor.dummy_pixel = 0;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	slim_video_setting();
	return ERROR_NONE;
}    /*    slim_video     */

static kal_uint32 custom1(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
						  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_CUSTOM1;

	imgsensor.pclk = imgsensor_info.custom1.pclk;
	imgsensor.line_length = imgsensor_info.custom1.linelength;
	imgsensor.frame_length = imgsensor_info.custom1.framelength;
	imgsensor.min_frame_length = imgsensor_info.custom1.framelength;
	imgsensor.dummy_line = 0;
	imgsensor.dummy_pixel = 0;
	imgsensor.autoflicker_en = KAL_FALSE;

	spin_unlock(&imgsensor_drv_lock);
	LOG_INF("custom1 fps:%d\n",imgsensor.current_fps);
	custom1_setting();

	return ERROR_NONE;

}	/* custom1() */

static kal_uint32 custom2(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
						  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_CUSTOM2;

	imgsensor.pclk = imgsensor_info.custom2.pclk;
	imgsensor.line_length = imgsensor_info.custom2.linelength;
	imgsensor.frame_length = imgsensor_info.custom2.framelength;
	imgsensor.min_frame_length = imgsensor_info.custom2.framelength;
	imgsensor.dummy_line = 0;
	imgsensor.dummy_pixel = 0;
	imgsensor.autoflicker_en = KAL_FALSE;

	spin_unlock(&imgsensor_drv_lock);
	LOG_INF("custom2 fps:%d\n",imgsensor.current_fps);
	custom2_setting();

	return ERROR_NONE;

}	/* custom2() */

static kal_uint32 custom3(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
						  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_CUSTOM3;

	imgsensor.pclk = imgsensor_info.custom3.pclk;
	imgsensor.line_length = imgsensor_info.custom3.linelength;
	imgsensor.frame_length = imgsensor_info.custom3.framelength;
	imgsensor.min_frame_length = imgsensor_info.custom3.framelength;
	imgsensor.dummy_line = 0;
	imgsensor.dummy_pixel = 0;
	imgsensor.autoflicker_en = KAL_FALSE;

	spin_unlock(&imgsensor_drv_lock);
	LOG_INF("custom3 fps:%d\n", imgsensor.current_fps);
	custom3_setting();

	return ERROR_NONE;

}	/* custom3() */

static kal_uint32 custom4(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_CUSTOM4;

	imgsensor.pclk = imgsensor_info.custom4.pclk;
	imgsensor.line_length = imgsensor_info.custom4.linelength;
	imgsensor.frame_length = imgsensor_info.custom4.framelength;
	imgsensor.min_frame_length = imgsensor_info.custom4.framelength;
	imgsensor.dummy_line = 0;
	imgsensor.dummy_pixel = 0;
	imgsensor.autoflicker_en = KAL_FALSE;

	spin_unlock(&imgsensor_drv_lock);
	LOG_INF("%s fps:%d\n", __func__, imgsensor.current_fps);
	custom4_setting();

	return ERROR_NONE;

}	/* custom4() */

static kal_uint32 custom5(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_CUSTOM5;

	imgsensor.pclk = imgsensor_info.custom5.pclk;
	imgsensor.line_length = imgsensor_info.custom5.linelength;
	imgsensor.frame_length = imgsensor_info.custom5.framelength;
	imgsensor.min_frame_length = imgsensor_info.custom5.framelength;
	imgsensor.dummy_line = 0;
	imgsensor.dummy_pixel = 0;
	imgsensor.autoflicker_en = KAL_FALSE;

	spin_unlock(&imgsensor_drv_lock);
	LOG_INF("%s fps:%d\n", __func__, imgsensor.current_fps);
	custom5_setting();

	return ERROR_NONE;

}	/* custom4() */

static kal_uint32 get_resolution(MSDK_SENSOR_RESOLUTION_INFO_STRUCT *sensor_resolution)
{
	LOG_INF("E\n");
	sensor_resolution->SensorFullWidth = imgsensor_info.cap.grabwindow_width;
	sensor_resolution->SensorFullHeight = imgsensor_info.cap.grabwindow_height;

	sensor_resolution->SensorPreviewWidth = imgsensor_info.pre.grabwindow_width;
	sensor_resolution->SensorPreviewHeight = imgsensor_info.pre.grabwindow_height;

	sensor_resolution->SensorVideoWidth = imgsensor_info.normal_video.grabwindow_width;
	sensor_resolution->SensorVideoHeight = imgsensor_info.normal_video.grabwindow_height;


	sensor_resolution->SensorHighSpeedVideoWidth     = imgsensor_info.hs_video.grabwindow_width;
	sensor_resolution->SensorHighSpeedVideoHeight     = imgsensor_info.hs_video.grabwindow_height;

	sensor_resolution->SensorSlimVideoWidth     = imgsensor_info.slim_video.grabwindow_width;
	sensor_resolution->SensorSlimVideoHeight     = imgsensor_info.slim_video.grabwindow_height;

	sensor_resolution->SensorCustom1Width     = imgsensor_info.custom1.grabwindow_width;
	sensor_resolution->SensorCustom1Height     = imgsensor_info.custom1.grabwindow_height;

	sensor_resolution->SensorCustom2Width     = imgsensor_info.custom2.grabwindow_width;
	sensor_resolution->SensorCustom2Height     = imgsensor_info.custom2.grabwindow_height;

	sensor_resolution->SensorCustom3Width     = imgsensor_info.custom3.grabwindow_width;
	sensor_resolution->SensorCustom3Height     = imgsensor_info.custom3.grabwindow_height;

	sensor_resolution->SensorCustom4Width =
		imgsensor_info.custom4.grabwindow_width;
	sensor_resolution->SensorCustom4Height =
		imgsensor_info.custom4.grabwindow_height;

	sensor_resolution->SensorCustom5Width =
		imgsensor_info.custom5.grabwindow_width;
	sensor_resolution->SensorCustom5Height =
		imgsensor_info.custom5.grabwindow_height;

	return ERROR_NONE;
}    /*    get_resolution    */


static kal_uint32 get_info(enum MSDK_SCENARIO_ID_ENUM scenario_id,
                      MSDK_SENSOR_INFO_STRUCT *sensor_info,
                      MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("scenario_id = %d\n", scenario_id);

	sensor_info->SensorClockPolarity = SENSOR_CLOCK_POLARITY_LOW;
	sensor_info->SensorClockFallingPolarity = SENSOR_CLOCK_POLARITY_LOW; /* not use */
	sensor_info->SensorHsyncPolarity = SENSOR_CLOCK_POLARITY_LOW; // inverse with datasheet
	sensor_info->SensorVsyncPolarity = SENSOR_CLOCK_POLARITY_LOW;
	sensor_info->SensorInterruptDelayLines = 4; /* not use */
	sensor_info->SensorResetActiveHigh = FALSE; /* not use */
	sensor_info->SensorResetDelayCount = 5; /* not use */

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
	sensor_info->SensorMasterClockSwitch = 0; /* not use */
	sensor_info->SensorDrivingCurrent = imgsensor_info.isp_driving_current;

	sensor_info->AEShutDelayFrame = imgsensor_info.ae_shut_delay_frame;          /* The frame of setting shutter default 0 for TG int */
	sensor_info->AESensorGainDelayFrame = imgsensor_info.ae_sensor_gain_delay_frame;    /* The frame of setting sensor gain */
	sensor_info->AEISPGainDelayFrame = imgsensor_info.ae_ispGain_delay_frame;
	sensor_info->IHDR_Support = imgsensor_info.ihdr_support;
	sensor_info->IHDR_LE_FirstLine = imgsensor_info.ihdr_le_firstline;
	sensor_info->SensorModeNum = imgsensor_info.sensor_mode_num;

	sensor_info->SensorMIPILaneNumber = imgsensor_info.mipi_lane_num;
	sensor_info->SensorClockFreq = imgsensor_info.mclk;
	sensor_info->SensorClockDividCount = 3; /* not use */
	sensor_info->SensorClockRisingCount = 0;
	sensor_info->SensorClockFallingCount = 2; /* not use */
	sensor_info->SensorPixelClockCount = 3; /* not use */
	sensor_info->SensorDataLatchCount = 2; /* not use */

	sensor_info->MIPIDataLowPwr2HighSpeedTermDelayCount = 0;
	sensor_info->MIPICLKLowPwr2HighSpeedTermDelayCount = 0;
	sensor_info->SensorWidthSampling = 0;  // 0 is default 1x
	sensor_info->SensorHightSampling = 0;    // 0 is default 1x
	sensor_info->SensorPacketECCOrder = 1;
	sensor_info->FrameTimeDelayFrame = imgsensor_info.frame_time_delay_frame;

	switch (scenario_id) {
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
			sensor_info->SensorGrabStartX = imgsensor_info.pre.startx;
			sensor_info->SensorGrabStartY = imgsensor_info.pre.starty;

			sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount = imgsensor_info.pre.mipi_data_lp2hs_settle_dc;

			break;
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			sensor_info->SensorGrabStartX = imgsensor_info.cap.startx;
			sensor_info->SensorGrabStartY = imgsensor_info.cap.starty;

			sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount = imgsensor_info.cap.mipi_data_lp2hs_settle_dc;

			break;
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:

			sensor_info->SensorGrabStartX = imgsensor_info.normal_video.startx;
			sensor_info->SensorGrabStartY = imgsensor_info.normal_video.starty;

			sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount = imgsensor_info.normal_video.mipi_data_lp2hs_settle_dc;

			break;
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
			sensor_info->SensorGrabStartX = imgsensor_info.hs_video.startx;
			sensor_info->SensorGrabStartY = imgsensor_info.hs_video.starty;

			sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount = imgsensor_info.hs_video.mipi_data_lp2hs_settle_dc;

			break;
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
			sensor_info->SensorGrabStartX = imgsensor_info.slim_video.startx;
			sensor_info->SensorGrabStartY = imgsensor_info.slim_video.starty;

			sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount = imgsensor_info.slim_video.mipi_data_lp2hs_settle_dc;

			break;
		case MSDK_SCENARIO_ID_CUSTOM1:
			sensor_info->SensorGrabStartX = imgsensor_info.custom1.startx;
			sensor_info->SensorGrabStartY = imgsensor_info.custom1.starty;

			sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount = imgsensor_info.custom1.mipi_data_lp2hs_settle_dc;

			break;
		case MSDK_SCENARIO_ID_CUSTOM2:
			sensor_info->SensorGrabStartX = imgsensor_info.custom2.startx;
			sensor_info->SensorGrabStartY = imgsensor_info.custom2.starty;

			sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount = imgsensor_info.custom2.mipi_data_lp2hs_settle_dc;

			break;
		case MSDK_SCENARIO_ID_CUSTOM3:
			sensor_info->SensorGrabStartX = imgsensor_info.custom3.startx;
			sensor_info->SensorGrabStartY = imgsensor_info.custom3.starty;

			sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount = imgsensor_info.custom3.mipi_data_lp2hs_settle_dc;

			break;
		case MSDK_SCENARIO_ID_CUSTOM4:
			sensor_info->SensorGrabStartX =
				imgsensor_info.custom4.startx;
			sensor_info->SensorGrabStartY =
				imgsensor_info.custom4.starty;

			sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
			  imgsensor_info.custom4.mipi_data_lp2hs_settle_dc;

			break;
		case MSDK_SCENARIO_ID_CUSTOM5:
			sensor_info->SensorGrabStartX =
				imgsensor_info.custom5.startx;
			sensor_info->SensorGrabStartY =
				imgsensor_info.custom5.starty;

			sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
			  imgsensor_info.custom5.mipi_data_lp2hs_settle_dc;

			break;
		default:
			sensor_info->SensorGrabStartX = imgsensor_info.pre.startx;
			sensor_info->SensorGrabStartY = imgsensor_info.pre.starty;

			sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount = imgsensor_info.pre.mipi_data_lp2hs_settle_dc;
			break;
	}

	return ERROR_NONE;
}    /*    get_info  */


static kal_uint32 control(enum MSDK_SCENARIO_ID_ENUM scenario_id, MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
					  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("scenario_id = %d\n", scenario_id);
	spin_lock(&imgsensor_drv_lock);
	imgsensor.current_scenario_id = scenario_id;
	spin_unlock(&imgsensor_drv_lock);
	switch (scenario_id) {
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
			LOG_INF("preview\n");
			preview(image_window, sensor_config_data);
			break;
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			case MSDK_SCENARIO_ID_CAMERA_ZSD:
			capture(image_window, sensor_config_data);
			break;
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			normal_video(image_window, sensor_config_data);
			break;
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
			hs_video(image_window, sensor_config_data);
			break;
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
			slim_video(image_window, sensor_config_data);
			break;
		case MSDK_SCENARIO_ID_CUSTOM1:
			custom1(image_window, sensor_config_data);
			break;
		case MSDK_SCENARIO_ID_CUSTOM2:
			custom2(image_window, sensor_config_data);
			break;
		case MSDK_SCENARIO_ID_CUSTOM3:
			custom3(image_window, sensor_config_data);
			break;
		case MSDK_SCENARIO_ID_CUSTOM4:
			custom4(image_window, sensor_config_data);
			break;
		case MSDK_SCENARIO_ID_CUSTOM5:
			custom5(image_window, sensor_config_data);
			break;
		default:
			LOG_INF("Error ScenarioId setting");
			preview(image_window, sensor_config_data);
			return ERROR_INVALID_SCENARIO_ID;
	}
	return ERROR_NONE;
}	/* control() */


static kal_uint32 set_video_mode(UINT16 framerate)
{
	LOG_INF("framerate = %d\n ", framerate);
	// SetVideoMode Function should fix framerate
	if (framerate == 0) {
		// Dynamic frame rate
		return ERROR_NONE;
	}
	spin_lock(&imgsensor_drv_lock);
	if ((framerate == 300) && (imgsensor.autoflicker_en == KAL_TRUE)) {
		imgsensor.current_fps = 296;
	} else if ((framerate == 150) && (imgsensor.autoflicker_en == KAL_TRUE)) {
		imgsensor.current_fps = 146;
	} else {
		imgsensor.current_fps = framerate;
	}
	spin_unlock(&imgsensor_drv_lock);
	set_max_framerate(imgsensor.current_fps,1);
	set_dummy();
	return ERROR_NONE;
}


static kal_uint32 set_auto_flicker_mode(kal_bool enable, UINT16 framerate)
{
	LOG_INF("enable = %d, framerate = %d ", enable, framerate);
	spin_lock(&imgsensor_drv_lock);
	if (enable) {
		imgsensor.autoflicker_en = KAL_TRUE;
	} else { //Cancel Auto flick
		imgsensor.autoflicker_en = KAL_FALSE;
	}
	spin_unlock(&imgsensor_drv_lock);
	return ERROR_NONE;
}


static kal_uint32 set_max_framerate_by_scenario(enum MSDK_SCENARIO_ID_ENUM scenario_id, MUINT32 framerate)
{
	kal_uint32 frame_length;

	LOG_INF("scenario_id = %d, framerate = %d\n", scenario_id, framerate);

	switch (scenario_id) {
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
			frame_length = imgsensor_info.pre.pclk / framerate * 10 / imgsensor_info.pre.linelength;
			spin_lock(&imgsensor_drv_lock);
			imgsensor.dummy_line = (frame_length > imgsensor_info.pre.framelength) ? (frame_length - imgsensor_info.pre.framelength) : 0;
			imgsensor.frame_length = imgsensor_info.pre.framelength + imgsensor.dummy_line;
			imgsensor.min_frame_length = imgsensor.frame_length;
			spin_unlock(&imgsensor_drv_lock);
			if (imgsensor.frame_length > imgsensor.shutter) {
				set_dummy();
			}
			break;
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			if (framerate == 0) {
				return ERROR_NONE;
			}
			frame_length = imgsensor_info.normal_video.pclk / framerate * 10 / imgsensor_info.normal_video.linelength;
			spin_lock(&imgsensor_drv_lock);
			imgsensor.dummy_line = (frame_length > imgsensor_info.normal_video.framelength) ? (frame_length - imgsensor_info.normal_video.framelength) : 0;
			imgsensor.frame_length = imgsensor_info.normal_video.framelength + imgsensor.dummy_line;
			imgsensor.min_frame_length = imgsensor.frame_length;
			spin_unlock(&imgsensor_drv_lock);
			if (imgsensor.frame_length > imgsensor.shutter) {
				set_dummy();
			}

			break;
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			frame_length =
			 imgsensor_info.cap.pclk
			 / framerate * 10 / imgsensor_info.cap.linelength;
			spin_lock(&imgsensor_drv_lock);
			imgsensor.dummy_line =
			 (frame_length > imgsensor_info.cap.framelength) ?
			 (frame_length - imgsensor_info.cap.framelength) : 0;
			imgsensor.frame_length =
			 imgsensor_info.cap.framelength
			 + imgsensor.dummy_line;
			imgsensor.min_frame_length = imgsensor.frame_length;
			spin_unlock(&imgsensor_drv_lock);

			if (imgsensor.frame_length>imgsensor.shutter) {
				set_dummy();
			}

			break;
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
			frame_length = imgsensor_info.hs_video.pclk / framerate * 10 / imgsensor_info.hs_video.linelength;
			spin_lock(&imgsensor_drv_lock);
			imgsensor.dummy_line = (frame_length > imgsensor_info.hs_video.framelength) ? (frame_length - imgsensor_info.hs_video.framelength) : 0;
			imgsensor.frame_length = imgsensor_info.hs_video.framelength + imgsensor.dummy_line;
			imgsensor.min_frame_length = imgsensor.frame_length;
			spin_unlock(&imgsensor_drv_lock);
			if (imgsensor.frame_length>imgsensor.shutter) {
				set_dummy();
			}
			break;
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
			frame_length = imgsensor_info.slim_video.pclk / framerate * 10 / imgsensor_info.slim_video.linelength;
			spin_lock(&imgsensor_drv_lock);
			imgsensor.dummy_line = (frame_length > imgsensor_info.slim_video.framelength) ? (frame_length - imgsensor_info.slim_video.framelength): 0;
			imgsensor.frame_length = imgsensor_info.slim_video.framelength + imgsensor.dummy_line;
			imgsensor.min_frame_length = imgsensor.frame_length;
			spin_unlock(&imgsensor_drv_lock);
			if (imgsensor.frame_length>imgsensor.shutter) {
				set_dummy();
			}
			break;
		case MSDK_SCENARIO_ID_CUSTOM1:
			frame_length = imgsensor_info.custom1.pclk / framerate * 10 / imgsensor_info.custom1.linelength;
			spin_lock(&imgsensor_drv_lock);
			imgsensor.dummy_line = (frame_length > imgsensor_info.custom1.framelength) ? (frame_length - imgsensor_info.custom1.framelength): 0;
			imgsensor.frame_length = imgsensor_info.custom1.framelength + imgsensor.dummy_line;
			imgsensor.min_frame_length = imgsensor.frame_length;
			spin_unlock(&imgsensor_drv_lock);
			if (imgsensor.frame_length>imgsensor.shutter) {
				set_dummy();
			}
			break;
		case MSDK_SCENARIO_ID_CUSTOM2:
			frame_length = imgsensor_info.custom2.pclk / framerate * 10 / imgsensor_info.custom2.linelength;
			spin_lock(&imgsensor_drv_lock);
			imgsensor.dummy_line = (frame_length > imgsensor_info.custom2.framelength) ? (frame_length - imgsensor_info.custom2.framelength): 0;
			imgsensor.frame_length = imgsensor_info.custom2.framelength + imgsensor.dummy_line;
			imgsensor.min_frame_length = imgsensor.frame_length;
			spin_unlock(&imgsensor_drv_lock);
			if (imgsensor.frame_length>imgsensor.shutter) {
				set_dummy();
			}
			break;
		case MSDK_SCENARIO_ID_CUSTOM3:
			frame_length = imgsensor_info.custom3.pclk / framerate * 10 / imgsensor_info.custom3.linelength;
			spin_lock(&imgsensor_drv_lock);
			imgsensor.dummy_line = (frame_length > imgsensor_info.custom3.framelength) ? (frame_length - imgsensor_info.custom3.framelength): 0;
			imgsensor.frame_length = imgsensor_info.custom3.framelength + imgsensor.dummy_line;
			imgsensor.min_frame_length = imgsensor.frame_length;
			spin_unlock(&imgsensor_drv_lock);
			if (imgsensor.frame_length>imgsensor.shutter) {
				set_dummy();
			}
			break;
		case MSDK_SCENARIO_ID_CUSTOM4:
			frame_length =
				imgsensor_info.custom4.pclk
				/ framerate * 10
				/ imgsensor_info.custom4.linelength;
			spin_lock(&imgsensor_drv_lock);
			imgsensor.dummy_line =
				(frame_length >
				imgsensor_info.custom4.framelength) ?
				(frame_length -
				imgsensor_info.custom4.framelength) : 0;
			imgsensor.frame_length =
				imgsensor_info.custom4.framelength +
				imgsensor.dummy_line;
			imgsensor.min_frame_length = imgsensor.frame_length;
			spin_unlock(&imgsensor_drv_lock);
			if (imgsensor.frame_length > imgsensor.shutter)
				set_dummy();
			break;
		case MSDK_SCENARIO_ID_CUSTOM5:
			frame_length =
				imgsensor_info.custom5.pclk
				/ framerate * 10
				/ imgsensor_info.custom5.linelength;
			spin_lock(&imgsensor_drv_lock);
			imgsensor.dummy_line =
				(frame_length >
				imgsensor_info.custom5.framelength) ?
				(frame_length -
				imgsensor_info.custom5.framelength) : 0;
			imgsensor.frame_length =
				imgsensor_info.custom5.framelength +
				imgsensor.dummy_line;
			imgsensor.min_frame_length = imgsensor.frame_length;
			spin_unlock(&imgsensor_drv_lock);
			if (imgsensor.frame_length > imgsensor.shutter)
				set_dummy();
			break;
		default:  //coding with  preview scenario by default
			frame_length = imgsensor_info.pre.pclk / framerate * 10 / imgsensor_info.pre.linelength;
			spin_lock(&imgsensor_drv_lock);
			imgsensor.dummy_line = (frame_length > imgsensor_info.pre.framelength) ? (frame_length - imgsensor_info.pre.framelength) : 0;
			imgsensor.frame_length = imgsensor_info.pre.framelength + imgsensor.dummy_line;
			imgsensor.min_frame_length = imgsensor.frame_length;
			spin_unlock(&imgsensor_drv_lock);
			if (imgsensor.frame_length>imgsensor.shutter) {
				set_dummy();
			}
			LOG_INF("error scenario_id = %d, we use preview scenario \n", scenario_id);
			break;
	}
	return ERROR_NONE;
}


static kal_uint32 get_default_framerate_by_scenario(enum MSDK_SCENARIO_ID_ENUM scenario_id, MUINT32 *framerate)
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
		default:
			break;
	}

	return ERROR_NONE;
}

static kal_uint32 set_test_pattern_mode(kal_bool enable)
{
	LOG_INF("enable: %d", enable);

	if (enable) {
		LOG_INF("enter color bar");
		// 0x5E00[8]: 1 enable,  0 disable
		// 0x5E00[1:0]; 00 Color bar, 01 Random Data, 10 Square, 11 BLACK

		write_cmos_sensor(0x0A00, 0x0000);
		write_cmos_sensor(0x0a04, 0x0141);
		write_cmos_sensor(0x020a, 0x0200);

		write_cmos_sensor(0x0A00, 0x0100);
		mdelay(1);
		//write_cmos_sensor(0x021c, 0x0000);
	} else {
		// 0x5E00[8]: 1 enable,  0 disable
		// 0x5E00[1:0]; 00 Color bar, 01 Random Data, 10 Square, 11 BLACK

		write_cmos_sensor(0x0A00, 0x0000);
		write_cmos_sensor(0x0a04, 0x0140);
		write_cmos_sensor(0x020a, 0x0000);
		write_cmos_sensor(0x0A00, 0x0100);
		mdelay(1);
		//write_cmos_sensor(0x020a, 0x0000);
	}
	spin_lock(&imgsensor_drv_lock);
	imgsensor.test_pattern = enable;
	spin_unlock(&imgsensor_drv_lock);
	return ERROR_NONE;
}


static kal_uint32 feature_control(MSDK_SENSOR_FEATURE_ENUM feature_id,
                             UINT8 *feature_para,UINT32 *feature_para_len)
{
	UINT16 *feature_return_para_16=(UINT16 *) feature_para;
	UINT16 *feature_data_16=(UINT16 *) feature_para;
	UINT32 *feature_return_para_32=(UINT32 *) feature_para;
	UINT32 *feature_data_32=(UINT32 *) feature_para;
	unsigned long long *feature_data=(unsigned long long *) feature_para;
	//unsigned long long *feature_return_para=(unsigned long long *) feature_para;

	struct SENSOR_WINSIZE_INFO_STRUCT *wininfo;
	MSDK_SENSOR_REG_INFO_STRUCT *sensor_reg_data=(MSDK_SENSOR_REG_INFO_STRUCT *) feature_para;
	LOG_INF("feature_id = %d\n", feature_id);
	switch (feature_id) {
		case SENSOR_FEATURE_GET_PERIOD:
			*feature_return_para_16++ = imgsensor.line_length;
			*feature_return_para_16 = imgsensor.frame_length;
			*feature_para_len=4;
			break;
		case SENSOR_FEATURE_GET_PIXEL_CLOCK_FREQ:
			*feature_return_para_32 = imgsensor.pclk;
			*feature_para_len=4;
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
			case MSDK_SCENARIO_ID_SLIM_VIDEO:
					*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
							= imgsensor_info.slim_video.pclk;
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
			case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
			default:
					*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
							= (imgsensor_info.pre.framelength << 16)
									 + imgsensor_info.pre.linelength;
					break;
			}
		break;
		case SENSOR_FEATURE_GET_MIPI_PIXEL_RATE:
		{
			kal_uint32 rate;

			switch (*feature_data) {
			case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
				rate = imgsensor_info.cap.mipi_pixel_rate;
				break;
			case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
				rate = imgsensor_info.normal_video.mipi_pixel_rate;
				break;
			case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
				rate = imgsensor_info.hs_video.mipi_pixel_rate;
				break;
			case MSDK_SCENARIO_ID_SLIM_VIDEO:
				rate = imgsensor_info.slim_video.mipi_pixel_rate;
				break;
			case MSDK_SCENARIO_ID_CUSTOM1:
				rate = imgsensor_info.custom1.mipi_pixel_rate;
				break;
			case MSDK_SCENARIO_ID_CUSTOM2:
				rate = imgsensor_info.custom2.mipi_pixel_rate;
				break;
			case MSDK_SCENARIO_ID_CUSTOM3:
				rate = imgsensor_info.custom3.mipi_pixel_rate;
				break;
			case MSDK_SCENARIO_ID_CUSTOM4:
				rate = imgsensor_info.custom4.mipi_pixel_rate;
				break;
			case MSDK_SCENARIO_ID_CUSTOM5:
				rate = imgsensor_info.custom5.mipi_pixel_rate;
				break;
			case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
			default:
				rate = imgsensor_info.pre.mipi_pixel_rate;
				break;
			}
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) = rate;
		}
		break;
		case SENSOR_FEATURE_SET_ESHUTTER:
			set_shutter(*feature_data);
			break;
		case SENSOR_FEATURE_SET_NIGHTMODE:
			night_mode((BOOL) *feature_data);
			break;
		case SENSOR_FEATURE_SET_GAIN:
			set_gain((UINT16) *feature_data);
			break;
		case SENSOR_FEATURE_SET_FLASHLIGHT:
			break;
		case SENSOR_FEATURE_SET_ISP_MASTER_CLOCK_FREQ:
			break;
		case SENSOR_FEATURE_SET_REGISTER:
			write_cmos_sensor(sensor_reg_data->RegAddr, sensor_reg_data->RegData);
			break;
		case SENSOR_FEATURE_GET_REGISTER:
			sensor_reg_data->RegData = read_cmos_sensor(sensor_reg_data->RegAddr);
			break;
		case SENSOR_FEATURE_GET_LENS_DRIVER_ID:
			// get the lens driver ID from EEPROM or just return LENS_DRIVER_ID_DO_NOT_CARE
			// if EEPROM does not exist in camera module.
			*feature_return_para_32=LENS_DRIVER_ID_DO_NOT_CARE;
			*feature_para_len=4;
			break;
		case SENSOR_FEATURE_SET_VIDEO_MODE:
			set_video_mode(*feature_data);
			break;
		case SENSOR_FEATURE_CHECK_SENSOR_ID:
			get_imgsensor_id(feature_return_para_32);
			break;
		case SENSOR_FEATURE_SET_AUTO_FLICKER_MODE:
			set_auto_flicker_mode((BOOL)*feature_data_16,*(feature_data_16+1));
			break;
		case SENSOR_FEATURE_SET_MAX_FRAME_RATE_BY_SCENARIO:
			set_max_framerate_by_scenario((enum MSDK_SCENARIO_ID_ENUM)*feature_data, *(feature_data+1));
			break;
		case SENSOR_FEATURE_GET_DEFAULT_FRAME_RATE_BY_SCENARIO:
			get_default_framerate_by_scenario((enum MSDK_SCENARIO_ID_ENUM)*(feature_data), (MUINT32 *)(uintptr_t)(*(feature_data+1)));
			break;
		case SENSOR_FEATURE_SET_TEST_PATTERN:
			set_test_pattern_mode((BOOL)*feature_data);
			break;

		case SENSOR_FEATURE_GET_TEST_PATTERN_CHECKSUM_VALUE: //for factory mode auto testing
			*feature_return_para_32 = imgsensor_info.checksum_value;
			*feature_para_len=4;
			break;
		case SENSOR_FEATURE_SET_FRAMERATE:
			LOG_INF("current fps :%d\n", (UINT32)*feature_data_32);
			spin_lock(&imgsensor_drv_lock);
			imgsensor.current_fps = *feature_data_32;
			spin_unlock(&imgsensor_drv_lock);
			break;
			//case SENSOR_FEATURE_SET_HDR:
			//LOG_INF("ihdr enable :%d\n", *feature_data_16);
			//spin_lock(&imgsensor_drv_lock);
			//imgsensor.ihdr_en = *feature_data_16;
			//spin_unlock(&imgsensor_drv_lock);
			//break;
		case SENSOR_FEATURE_GET_CROP_INFO:
			//LOG_INF("SENSOR_FEATURE_GET_CROP_INFO scenarioId:%d\n", *feature_data_32);
			wininfo = (struct SENSOR_WINSIZE_INFO_STRUCT *)(uintptr_t)(*(feature_data+1));

			switch (*feature_data_32) {
			case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
				memcpy((void *)wininfo,
				  (void *)&imgsensor_winsize_info[1],
				  sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
				break;
			case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
				memcpy((void *)wininfo,
				  (void *)&imgsensor_winsize_info[2],
				  sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
				break;
			case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
				memcpy((void *)wininfo,
				  (void *)&imgsensor_winsize_info[3],
				  sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
				break;
			case MSDK_SCENARIO_ID_SLIM_VIDEO:
				memcpy((void *)wininfo,
				  (void *)&imgsensor_winsize_info[4],
				  sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
				break;
			case MSDK_SCENARIO_ID_CUSTOM1:
				memcpy((void *)wininfo,
				  (void *)&imgsensor_winsize_info[5],
				  sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
				break;
			case MSDK_SCENARIO_ID_CUSTOM2:
				memcpy((void *)wininfo,
				  (void *)&imgsensor_winsize_info[6],
				  sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
				break;
			case MSDK_SCENARIO_ID_CUSTOM3:
				memcpy((void *)wininfo,
				  (void *)&imgsensor_winsize_info[7],
				  sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
				break;
			case MSDK_SCENARIO_ID_CUSTOM4:
				memcpy((void *)wininfo,
				  (void *)&imgsensor_winsize_info[8],
				  sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
				break;
			case MSDK_SCENARIO_ID_CUSTOM5:
				memcpy((void *)wininfo,
				  (void *)&imgsensor_winsize_info[9],
				  sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
				break;
			case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
			default:
				memcpy((void *)wininfo,
				  (void *)&imgsensor_winsize_info[0],
				  sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
				break;
			}
			break;
		case SENSOR_FEATURE_SET_IHDR_SHUTTER_GAIN:
			LOG_INF("SENSOR_SET_SENSOR_IHDR LE=%d, SE=%d, Gain=%d\n",(UINT16)*feature_data,(UINT16)*(feature_data+1),(UINT16)*(feature_data+2));
			//ihdr_write_shutter_gain((UINT16)*feature_data,(UINT16)*(feature_data+1),(UINT16)*(feature_data+2));
			break;
		case SENSOR_FEATURE_SET_SHUTTER_FRAME_TIME:
			set_shutter_frame_length((UINT16) (*feature_data),
						(UINT16) (*(feature_data + 1)));
			break;
		case SENSOR_FEATURE_SET_STREAMING_SUSPEND:
			LOG_INF("SENSOR_FEATURE_SET_STREAMING_SUSPEND\n");
			streaming_control(KAL_FALSE);
			wait_stream_onoff(KAL_FALSE);
			break;
		case SENSOR_FEATURE_SET_STREAMING_RESUME:
			LOG_INF("SENSOR_FEATURE_SET_STREAMING_RESUME, shutter:%llu\n", *feature_data);
			if (*feature_data != 0) {
				set_shutter(*feature_data);
			}
			streaming_control(KAL_TRUE);
			wait_stream_onoff(KAL_TRUE);
			break;
		case SENSOR_FEATURE_GET_BINNING_TYPE:
			switch (*(feature_data + 1)) {
			case MSDK_SCENARIO_ID_SLIM_VIDEO:
			case MSDK_SCENARIO_ID_CUSTOM5:
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
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) = 0;
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

int hi2021q_otp_init(void)
{
	write_cmos_sensor(0x0790, 0x0100);
	write_cmos_sensor(0x2000, 0x0000);
	write_cmos_sensor(0x2002, 0x007F);
	write_cmos_sensor(0x2006, 0x40B2);
	write_cmos_sensor(0x2008, 0xB00E);
	write_cmos_sensor(0x200A, 0x8446);
	write_cmos_sensor(0x200C, 0x4130);
	write_cmos_sensor(0x200E, 0x403D);
	write_cmos_sensor(0x2010, 0x0011);
	write_cmos_sensor(0x2012, 0x403E);
	write_cmos_sensor(0x2014, 0xC092);
	write_cmos_sensor(0x2016, 0x403F);
	write_cmos_sensor(0x2018, 0x0A80);
	write_cmos_sensor(0x201A, 0x1292);
	write_cmos_sensor(0x201C, 0x8440);
	write_cmos_sensor(0x201E, 0x40F2);
	write_cmos_sensor(0x2020, 0xFFC0);
	write_cmos_sensor(0x2022, 0x0531);
	write_cmos_sensor(0x2024, 0xC3D2);
	write_cmos_sensor(0x2026, 0x0702);
	write_cmos_sensor(0x2028, 0x0C64);
	write_cmos_sensor(0x202A, 0xC3D2);
	write_cmos_sensor(0x202C, 0x0742);
	write_cmos_sensor(0x202E, 0xC3D2);
	write_cmos_sensor(0x2030, 0x0732);
	write_cmos_sensor(0x2032, 0x4130);
	write_cmos_sensor(0x0708, 0xEF82);
	write_cmos_sensor(0x070C, 0x0000);
	write_cmos_sensor(0x0732, 0x0300);
	write_cmos_sensor(0x0736, 0x0063);
	write_cmos_sensor(0x0738, 0x0002);
	write_cmos_sensor(0x0742, 0x0300);
	write_cmos_sensor(0x0746, 0x014A);
	write_cmos_sensor(0x0748, 0x0003);
	write_cmos_sensor(0x074C, 0x0000);
	write_cmos_sensor(0x0266, 0x0000);
	write_cmos_sensor(0x0360, 0xAC8E);
	write_cmos_sensor(0x027E, 0x0100);
	write_cmos_sensor(0x0B00, 0x0000);

	return 0;
}

void hi2021q_otp_off_setting()
{
	write_cmos_sensor_8(0x0B00, 0x00);
	usleep_range(10000, 10100);
	write_cmos_sensor_8(0x0260, 0x00);
	write_cmos_sensor_8(0x0B00, 0x01);
	usleep_range(1000, 1100);
}

int hi2021q_read_otp_cal(unsigned int addr, unsigned char *data, unsigned int size)
{	kal_uint32 i = 0;
	kal_uint8 otp_bank = 0;
	kal_uint32 read_addr = 0;

	LOG_INF("%s - E\n", __func__);

	if (data == NULL) {
		pr_err("%s: fail, data is NULL\n", __func__);
		return -1;
	}

	// OTP initial setting
	hi2021q_otp_init();

	write_cmos_sensor_8(0x0B00, 0x00);
	usleep_range(10000, 10100);
	write_cmos_sensor_8(0x027E, 0x00);
	usleep_range(5000, 5100);
	write_cmos_sensor_8(0x0260, 0x10);
	write_cmos_sensor_8(0x027E, 0x01);
	usleep_range(5000, 5100);
	write_cmos_sensor_8(0x0B00, 0x01);
	usleep_range(1000, 1100);

	otp_bank = hi2021q_otp_read_byte(HI2021Q_OTP_CHECK_BANK);
	pr_info("%s, check OTP bank:0x%x, size: %d\n", __func__, otp_bank, size);

	switch (otp_bank) {
	case HI2021Q_OTP_BANK1_MARK:
		read_addr = HI2021Q_OTP_BANK1_START_ADDR;
	break;
	case HI2021Q_OTP_BANK2_MARK:
		read_addr = HI2021Q_OTP_BANK2_START_ADDR;
		break;
	default:
		pr_err("%s, check bank: fail\n", __func__);
		hi2021q_otp_off_setting();
		return -1;
	}

	for (i = 0; i < size; i++) {
		*(data + i) = hi2021q_otp_read_byte(read_addr);
		LOG_INF("read data read_addr: %x, data: %x", read_addr, *(data + i));
		read_addr++;
	}

	// OTP off setting
	hi2021q_otp_off_setting();
	LOG_INF("%s - X\n", __func__);

	return (int)size;
}


/*UINT32 HI846_MIPI_RAW_SensorInit(struct SENSOR_FUNCTION_STRUCT **pfFunc)*/
UINT32 HI2021Q_MIPI_RAW_SensorInit(struct SENSOR_FUNCTION_STRUCT **pfFunc)
{
	/* To Do : Check Sensor status here */
	if (pfFunc!=NULL)
		*pfFunc=&sensor_func;
	return ERROR_NONE;
}	/*	HI2021Q_MIPI_RAW_SensorInit	*/

#define MAX_WAIT_POLL_CNT 10
#define PLL_CFG_RAMP_TG_B 0x0732
#define POLL_TIME_USEC  5000
#define PLL_CLKGEN_EN_RAMP_EN 0x01
void wait_stream_onoff(bool stream_onoff)
{
	kal_uint16 pll_cfg_ramb_tg_b = 0;
	kal_uint16 wait_poll_cnt = 0;

	if (stream_onoff) { /* stream on */

		do {
			pll_cfg_ramb_tg_b = read_cmos_sensor(PLL_CFG_RAMP_TG_B);
			if (pll_cfg_ramb_tg_b & PLL_CLKGEN_EN_RAMP_EN)
				break;
			wait_poll_cnt++;
			usleep_range(POLL_TIME_USEC, POLL_TIME_USEC);
		} while (wait_poll_cnt < MAX_WAIT_POLL_CNT);

	} else { /* stream off */

		do {
			pll_cfg_ramb_tg_b = read_cmos_sensor(PLL_CFG_RAMP_TG_B);
			if (!(pll_cfg_ramb_tg_b & PLL_CLKGEN_EN_RAMP_EN))
				break;
			wait_poll_cnt++;
			usleep_range(POLL_TIME_USEC, POLL_TIME_USEC);
		} while (wait_poll_cnt < MAX_WAIT_POLL_CNT);

	}

	return;
}
