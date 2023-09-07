/*
 * Copyright (C) 2016 MediaTek Inc.
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
#include <linux/videodev2.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/atomic.h>
#include "kd_camera_typedef.h"
#include "kd_imgsensor.h"
#include "kd_imgsensor_define.h"
#include "kd_imgsensor_errcode.h"
#include "imx241mipimono_Sensor.h"
#define PFX "IMX241_mono_sensor"
#define LOG_INF(fmt, args...)    pr_debug(PFX "[%s] " fmt, __func__, ##args)
#define LOG_1 LOG_INF("IMX241,MIPI 2LANE\n")
#define LOG_2 LOG_INF("preview 1640*1232@30fps; capture 3280*2464@30fps,1488Mbps/lane\n")
static DEFINE_SPINLOCK(imgsensor_drv_lock);
static struct imgsensor_info_struct imgsensor_info = {
	.sensor_id = IMX241_MONO_SENSOR_ID,
	.checksum_value = 0x79e5b6af,
	.pre = {
		.pclk = 94400000,               /* record different mode's pclk */
		.linelength = 1456,				/* record different mode's linelength */
		.framelength = 2160,            /* record different mode's framelength */
		.startx = 0,					/* record different mode's startx of grabwindow */
		.starty = 0,					/* record different mode's starty of grabwindow */
		.grabwindow_width = 2592,		/* record different mode's width of grabwindow */
		.grabwindow_height = 1944,		/* record different mode's height of grabwindow */
		/*	 following for MIPIDataLowPwr2HighSpeedSettleDelayCount by different scenario	*/
		.mipi_data_lp2hs_settle_dc = 85,
		/*	 following for GetDefaultFramerateByScenario()	*/
		.max_framerate = 300,
	},
	.cap = {
		.pclk = 94400000,
		.linelength = 1456,
		.framelength = 2160,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 2592,
		.grabwindow_height = 1944,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 300,
	},
#if 0
	.cap1 = {
		.pclk = 160000000,
		.linelength = 1456,
		.framelength = 2046,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 2592,
		.grabwindow_height = 1944,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 240,
	},
#endif
	.normal_video = {
		.pclk = 94400000,
		.linelength = 1456,
		.framelength = 2160,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 2592,
		.grabwindow_height = 1944,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 300,
	},
	.hs_video = {
		.pclk = 94400000,
		.linelength = 1456,
		.framelength = 2160,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 2592,
		.grabwindow_height = 1458,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 300,
	},
	.slim_video = {
		.pclk = 94400000,
		.linelength = 1456,
		.framelength = 2160,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 1296,
		.grabwindow_height = 972,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 300,
	},
	.custom1 = {
		.pclk = 94400000,
		.linelength = 1456,
		.framelength = 2688,/*for 24fps*/
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 2592,
		.grabwindow_height = 1944,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 240,
	},
	.custom2 = {
		.pclk = 94400000,
		.linelength = 1456,
		.framelength = 2688,/*for 24fps*/
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 1296,
		.grabwindow_height = 972,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 240,
	},
	.margin = 5,
	.min_shutter = 1,
	.max_frame_length = 0x7fff,
	.ae_shut_delay_frame = 0,
	.ae_sensor_gain_delay_frame = 0,
	.ae_ispGain_delay_frame = 2,
	.ihdr_support = 0,	  /* 1, support; 0,not support */
	.ihdr_le_firstline = 0,  /* 1,le first ; 0, se first */
	.sensor_mode_num = 7,   /* support sensor mode num */
	.cap_delay_frame = 3,
	.pre_delay_frame = 3,
	.video_delay_frame = 3,
	.hs_video_delay_frame = 3,
	.slim_video_delay_frame = 3,
	.custom1_delay_frame = 2,
	.custom2_delay_frame = 2,
	.isp_driving_current = ISP_DRIVING_6MA,
	.sensor_interface_type = SENSOR_INTERFACE_TYPE_MIPI,
	.mipi_sensor_type = MIPI_OPHY_NCSI2, /* 0,MIPI_OPHY_NCSI2;  1,MIPI_OPHY_CSI2 */
	.mipi_settle_delay_mode = 1,/* 0,MIPI_SETTLEDELAY_AUTO; 1,MIPI_SETTLEDELAY_MANNUAL */
	.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_MONO,
	.mclk = 24,
	.mipi_lane_num = SENSOR_MIPI_2_LANE,
	.i2c_addr_table = {0x6e, 0x34, 0xff},
	.i2c_speed = 300, /* i2c read/write speed */
};
static struct imgsensor_struct imgsensor = {
	.mirror = IMAGE_NORMAL,				/* mirrorflip information */
	.sensor_mode = IMGSENSOR_MODE_INIT,
	.shutter = 0x0545,					/* current shutter */
	.gain = 0x100,                      /* current gain     // Danbo ?? */
	.dummy_pixel = 0,					/* current dummypixel */
	.dummy_line = 0,					/* current dummyline */
	.current_fps = 0,  /* full size current fps : 24fps for PIP, 30fps for Normal or ZSD */
	.autoflicker_en = KAL_FALSE,
	.test_pattern = KAL_FALSE,
	.current_scenario_id = MSDK_SCENARIO_ID_CAMERA_PREVIEW,/* current scenario id */
	.ihdr_en = 0, /* sensor need support LE, SE with HDR feature */
	.i2c_write_id = 0x6e,
};
/* Sensor output window information */
static SENSOR_WINSIZE_INFO_STRUCT imgsensor_winsize_info[7] = {
	{ 2592, 1944, 0,   0, 2592, 1944, 2592, 1944, 0000, 0000, 2592, 1944, 0, 0, 2592, 1944}, /*preview*/
	{ 2592, 1944, 0,   0, 2592, 1944, 2592, 1944, 0000, 0000, 2592, 1944, 0, 0, 2592, 1944}, /* capture */
	{ 2592, 1944, 0,   0, 2592, 1944, 2592, 1944, 0000, 0000, 2592, 1944, 0, 0, 2592, 1944}, /* video-4:3 */
	{ 2592, 1944, 0, 243, 2592, 1458, 2592, 1458, 0000, 0000, 2592, 1458, 0, 0, 2592, 1458}, /* video-16:9 */
	{ 2592, 1944, 0,   0, 2592, 1944, 1296, 972,  0000, 0000, 1296,  972, 0, 0, 1296,  972}, /* slim video */
	{ 2592, 1944, 0,   0, 2592, 1944, 2592, 1944, 0000, 0000, 2592, 1944, 0, 0, 2592, 1944}, /* Custom1 */
	{ 2592, 1944, 0,   0, 2592, 1944, 1296, 972,  0000, 0000, 1296,  972, 0, 0, 1296,  972}, /* Custom2 */
};/* slim video */
/* Gain Index */
#define MaxGainIndex (86)
static kal_uint16 sensorGainMapping[MaxGainIndex][2] = {
{ 64, 0  },
{ 68, 16 },
{ 71, 26 },
{ 74, 35 },
{ 77, 44 },
{ 81, 54 },
{ 84, 61 },
{ 87, 68 },
{ 90, 74 },
{ 93, 80 },
{ 96, 86 },
{ 100, 93 },
{ 103, 97 },
{ 106, 102},
{ 109, 106},
{ 113, 112},
{ 116, 115},
{ 120, 120},
{ 122, 122},
{ 125, 125},
{ 128, 128},
{ 132, 132},
{ 135, 135},
{ 138, 138},
{ 141, 140},
{ 144, 143},
{ 148, 146},
{ 151, 148},
{ 153, 149},
{ 157, 152},
{ 160, 154},
{ 164, 157},
{ 168, 159},
{ 169, 160},
{ 173, 162},
{ 176, 163},
{ 180, 165},
{ 182, 166},
{ 187, 169},
{ 189, 170},
{ 193, 172},
{ 196, 173},
{ 200, 175},
{ 203, 176},
{ 205, 177},
{ 208, 178},
{ 213, 180},
{ 216, 181},
{ 219, 182},
{ 222, 183},
{ 225, 184},
{ 228, 185},
{ 232, 186},
{ 235, 187},
{ 238, 188},
{ 241, 189},
{ 245, 190},
{ 249, 191},
{ 253, 192},
{ 256, 192},
{ 260, 193},
{ 265, 195},
{ 269, 196},
{ 274, 197},
{ 278, 198},
{ 283, 199},
{ 288, 200},
{ 293, 201},
{ 298, 202},
{ 304, 203},
{ 310, 204},
{ 322, 206},
{ 328, 207},
{ 335, 208},
{ 342, 209},
{ 349, 210},
{ 357, 211},
{ 365, 212},
{ 373, 213},
{ 400, 216},
{ 420, 217},
{ 432, 219},
{ 443, 220},
{ 468, 221},
{ 482, 223},
/* { 497,224}, */
{ 512, 224},
/* { 529,225}, */
/* { 546,226}, */
/* { 566,227}, */
/* { 585,228}, */
/* { 607,229}, */
/* { 631,230}, */
/* { 656,231}, */
/* { 683,232} */
};
static kal_uint16 read_cmos_sensor(kal_uint32 addr)
{
	kal_uint16 get_byte = 0;
	char pu_send_cmd[2] = {(char)(addr >> 8), (char)(addr & 0xFF) };

	iReadRegI2C(pu_send_cmd, 2, (u8 *)&get_byte, 1, imgsensor.i2c_write_id);
	return get_byte;
}
static void write_cmos_sensor(kal_uint32 addr, kal_uint32 para)
{
	char pu_send_cmd[3] = {(char)(addr >> 8), (char)(addr & 0xFF), (char)(para & 0xFF)};

	iWriteRegI2C(pu_send_cmd, 3, imgsensor.i2c_write_id);
}
static kal_uint16 read_cmos_sensor_8(kal_uint16 addr)
{
	kal_uint16 get_byte = 0;
	char pu_send_cmd[2] = {(char)(addr >> 8), (char)(addr & 0xFF) };

	iReadRegI2C(pu_send_cmd, 2, (u8 *)&get_byte, 1, imgsensor.i2c_write_id);
	return get_byte;
}
static void write_cmos_sensor_8(kal_uint16 addr, kal_uint8 para)
{
	char pu_send_cmd[4] = {(char)(addr >> 8), (char)(addr & 0xFF), (char)(para & 0xFF)};

	iWriteRegI2C(pu_send_cmd, 3, imgsensor.i2c_write_id);
}
static void set_dummy(void)
{
	LOG_INF("dummyline = %d, dummypixels = %d\n", imgsensor.dummy_line, imgsensor.dummy_pixel);
	write_cmos_sensor_8(0x0104, 0x01);
	write_cmos_sensor(0x0340, (imgsensor.frame_length >> 8) & 0xFF);
	write_cmos_sensor(0x0341, imgsensor.frame_length & 0xFF);
	write_cmos_sensor(0x0342, (imgsensor.line_length >> 8) & 0xFF);
	write_cmos_sensor(0x0343, imgsensor.line_length & 0xFF);
	write_cmos_sensor_8(0x0104, 0x00);
}	/*	set_dummy  */
static void set_max_framerate(UINT16 framerate, kal_bool min_framelength_en)
{
	kal_uint32 frame_length = imgsensor.frame_length;

	LOG_INF("framerate = %d, min framelength should enable?\n", framerate);
	frame_length = imgsensor.pclk / framerate * 10 / imgsensor.line_length;
	spin_lock(&imgsensor_drv_lock);
	imgsensor.frame_length = (frame_length > imgsensor.min_frame_length)?frame_length:imgsensor.min_frame_length;
	imgsensor.dummy_line = imgsensor.frame_length - imgsensor.min_frame_length;
	if (imgsensor.frame_length > imgsensor_info.max_frame_length) {
		imgsensor.frame_length = imgsensor_info.max_frame_length;
		imgsensor.dummy_line = imgsensor.frame_length - imgsensor.min_frame_length;
	}
	if (min_framelength_en)
		imgsensor.min_frame_length = imgsensor.frame_length;
	spin_unlock(&imgsensor_drv_lock);
	LOG_INF("[ylf_set_max_framerate]imgsensor.frame_length =%d\n", imgsensor.frame_length);
	set_dummy();
}	/*	set_max_framerate  */
static void set_shutter(kal_uint16 shutter)
{
	kal_uint16 realtime_fps = 0;
	unsigned long flags;

	spin_lock_irqsave(&imgsensor_drv_lock, flags);
	imgsensor.shutter = shutter;
	spin_unlock_irqrestore(&imgsensor_drv_lock, flags);
	LOG_INF("imgsensor.frame_length =%d, shutter =%d\n", imgsensor.frame_length, shutter);
	spin_lock_irqsave(&imgsensor_drv_lock, flags);
	if (shutter > imgsensor.min_frame_length - imgsensor_info.margin)
		imgsensor.frame_length = shutter + imgsensor_info.margin;
	else
		imgsensor.frame_length = imgsensor.min_frame_length;
	if (imgsensor.frame_length > imgsensor_info.max_frame_length)
		imgsensor.frame_length = imgsensor_info.max_frame_length;
	spin_unlock_irqrestore(&imgsensor_drv_lock, flags);
	shutter = (shutter < imgsensor_info.min_shutter) ? imgsensor_info.min_shutter : shutter;
	shutter = (shutter > (imgsensor_info.max_frame_length - imgsensor_info.margin))
					? (imgsensor_info.max_frame_length - imgsensor_info.margin) : shutter;
	if (imgsensor.autoflicker_en) {
		realtime_fps = imgsensor.pclk / imgsensor.line_length * 10 / imgsensor.frame_length;
		if (realtime_fps >= 297 && realtime_fps <= 305)
			set_max_framerate(296, 0);
		else if (realtime_fps >= 147 && realtime_fps <= 150)
			set_max_framerate(146, 0);
	} else {
		write_cmos_sensor_8(0x0104, 0x01);
		write_cmos_sensor(0x0340, (imgsensor.frame_length >> 8) & 0xFF);
		write_cmos_sensor(0x0341, imgsensor.frame_length & 0xFF);
		write_cmos_sensor_8(0x0104, 0x00);
	}
	write_cmos_sensor_8(0x0104, 0x01);
	write_cmos_sensor(0x0340, (imgsensor.frame_length >> 8) & 0xFF);
	write_cmos_sensor(0x0341, imgsensor.frame_length & 0xFF);
	write_cmos_sensor(0x0350, 0x01); /* Enable auto extend */
	write_cmos_sensor(0x0202, (shutter >> 8) & 0xFF);
	write_cmos_sensor(0x0203, shutter  & 0xFF);
	write_cmos_sensor_8(0x0104, 0x00);
	LOG_INF("Exit! shutter =%d, framelength =%d\n", shutter, imgsensor.frame_length);
}	/*	set_shutter  */
static void set_shutter_frame_length(kal_uint16 shutter, kal_uint16 frame_length)
{
	unsigned long flags;
	kal_uint16 realtime_fps = 0;
	kal_int32 dummy_line = 0;

	spin_lock_irqsave(&imgsensor_drv_lock, flags);
	imgsensor.shutter = shutter;
	spin_unlock_irqrestore(&imgsensor_drv_lock, flags);
	spin_lock(&imgsensor_drv_lock);
	/*Change frame time*/
	if (frame_length > 1)
		dummy_line = frame_length - imgsensor.frame_length;
	imgsensor.frame_length = imgsensor.frame_length + dummy_line;
	/*  */
	if (shutter > imgsensor.frame_length - imgsensor_info.margin)
		imgsensor.frame_length = shutter + imgsensor_info.margin;
	if (imgsensor.frame_length > imgsensor_info.max_frame_length)
		imgsensor.frame_length = imgsensor_info.max_frame_length;
	spin_unlock(&imgsensor_drv_lock);
	shutter = (shutter < imgsensor_info.min_shutter) ? imgsensor_info.min_shutter : shutter;
	shutter = (shutter > (imgsensor_info.max_frame_length - imgsensor_info.margin)) ?
			(imgsensor_info.max_frame_length - imgsensor_info.margin) : shutter;
	if (imgsensor.autoflicker_en) {
		realtime_fps = imgsensor.pclk / imgsensor.line_length * 10 / imgsensor.frame_length;
		if (realtime_fps >= 297 && realtime_fps <= 305)
			set_max_framerate(296, 0);
		else if (realtime_fps >= 147 && realtime_fps <= 150)
			set_max_framerate(146, 0);
		else {
			/* Extend frame length */
			write_cmos_sensor(0x0104, 0x01);
			write_cmos_sensor(0x0340, imgsensor.frame_length >> 8);
			write_cmos_sensor(0x0341, imgsensor.frame_length & 0xFF);
			write_cmos_sensor(0x0104, 0x00);
		}
	} else {
		/* Extend frame length */
		write_cmos_sensor(0x0104, 0x01);
		write_cmos_sensor(0x0340, imgsensor.frame_length >> 8);
		write_cmos_sensor(0x0341, imgsensor.frame_length & 0xFF);
		write_cmos_sensor(0x0104, 0x00);
	}
	/* Update Shutter */
	write_cmos_sensor(0x0104, 0x01);
	write_cmos_sensor(0x0350, 0x00); /* Disable auto extend */
	write_cmos_sensor(0x0202, (shutter >> 8) & 0xFF);
	write_cmos_sensor(0x0203, shutter  & 0xFF);
	write_cmos_sensor(0x0104, 0x00);
	LOG_INF("Exit! shutter =%d, framelength =%d/%d, dummy_line=%d, auto_extend=%d\n",
				shutter, imgsensor.frame_length, frame_length, dummy_line, read_cmos_sensor(0x0350));
}    /*    set_shutter */
static kal_uint16 gain2reg(const kal_uint16 gain)
{
	kal_uint8 iI;

	for (iI = 0; iI < (MaxGainIndex-1); iI++) {
		if (gain <= sensorGainMapping[iI][0])
			break;
	}
	return sensorGainMapping[iI][1];
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
	/* [0:3] = N meams N /16 X	*/
	/* [4:9] = M meams M X		 */
	/* Total gain = M + N /16 X   */
	if (gain < BASEGAIN || gain > 32 * BASEGAIN) {
		LOG_INF("Error gain setting");
		if (gain < BASEGAIN)
			gain = BASEGAIN;
		else if (gain > 32 * BASEGAIN)
			gain = 32 * BASEGAIN;
	}
	reg_gain = gain2reg(gain);
	spin_lock(&imgsensor_drv_lock);
	imgsensor.gain = reg_gain;
	spin_unlock(&imgsensor_drv_lock);
	LOG_INF("gain = %d , reg_gain = 0x%x\n ", gain, reg_gain);
	write_cmos_sensor_8(0x0104, 0x01);
	/*write_cmos_sensor(0x0204, (reg_gain>>8)& 0xFF);*/
	write_cmos_sensor(0x0205, reg_gain & 0xFF);
	write_cmos_sensor_8(0x0104, 0x00);
	return gain;
}	/*	set_gain  */
static void set_mirror_flip(kal_uint8 image_mirror)
{
	LOG_INF("image_mirror = %d", image_mirror);
	spin_lock(&imgsensor_drv_lock);
	imgsensor.mirror = image_mirror;
	spin_unlock(&imgsensor_drv_lock);
	switch (image_mirror) {
	case IMAGE_NORMAL:
		write_cmos_sensor_8(0x0101, 0x00);
		break;
	case IMAGE_H_MIRROR:
		write_cmos_sensor_8(0x0101, 0x01);
		break;
	case IMAGE_V_MIRROR:
		write_cmos_sensor_8(0x0101, 0x02);
		break;
	case IMAGE_HV_MIRROR:
		write_cmos_sensor_8(0x0101, 0x03);
		break;
	default:
		LOG_INF("Error image_mirror setting\n");
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
static void night_mode(kal_bool enable)
{
/*No Need to implement this function*/
}	/*	night_mode	*/
static void sensor_init(void)
{
	LOG_INF("sensor_init() E\n");
	write_cmos_sensor(0x0101, 0x00);
	write_cmos_sensor(0x303C, 0x4B);
	write_cmos_sensor(0x303D, 0x00);
	write_cmos_sensor(0x3041, 0xD7);
	write_cmos_sensor(0x30E0, 0x00);
	write_cmos_sensor(0x30E1, 0x00);
	write_cmos_sensor(0x30F6, 0x00);
	write_cmos_sensor(0x34CE, 0xFF);
	/*write_cmos_sensor(0x3063, 0x04);*/
	/*This register should be deleted*/
	LOG_INF("Exit\n");
}	/*	sensor_init  */
static void preview_setting(void)
{
	LOG_INF("preview_setting() E\n");
	write_cmos_sensor_8(0x0100, 0x00);
	write_cmos_sensor_8(0x0340, 0x08);
	write_cmos_sensor_8(0x0341, 0x70);
	write_cmos_sensor_8(0x0342, 0x05);
	write_cmos_sensor_8(0x0343, 0xB0);
	write_cmos_sensor_8(0x0344, 0x00);
	write_cmos_sensor_8(0x0345, 0x00);
	write_cmos_sensor_8(0x0346, 0x00);
	write_cmos_sensor_8(0x0347, 0x00);
	write_cmos_sensor_8(0x0348, 0x0A);
	write_cmos_sensor_8(0x0349, 0x1F);
	write_cmos_sensor_8(0x034A, 0x07);
	write_cmos_sensor_8(0x034B, 0x97);
	write_cmos_sensor_8(0x0381, 0x01);
	write_cmos_sensor_8(0x0383, 0x01);
	write_cmos_sensor_8(0x0385, 0x01);
	write_cmos_sensor_8(0x0387, 0x01);
	write_cmos_sensor_8(0x3048, 0x20);
	write_cmos_sensor_8(0x30D5, 0x00);
	write_cmos_sensor_8(0x3165, 0x20);
	write_cmos_sensor_8(0x30D1, 0x00);
	write_cmos_sensor_8(0x30D0, 0x2A);
	write_cmos_sensor_8(0x3102, 0x13);
	write_cmos_sensor_8(0x3103, 0x47);
	write_cmos_sensor_8(0x3049, 0x01);
	write_cmos_sensor_8(0x304D, 0x02);
	write_cmos_sensor_8(0x304C, 0xD7);
	write_cmos_sensor_8(0x0112, 0x0A);
	write_cmos_sensor_8(0x0113, 0x0A);
	write_cmos_sensor_8(0x034C, 0x0A);
	write_cmos_sensor_8(0x034D, 0x20);
	write_cmos_sensor_8(0x034E, 0x07);
	write_cmos_sensor_8(0x034F, 0x98);
	write_cmos_sensor_8(0x0305, 0x03);
	write_cmos_sensor_8(0x0307, 0x76);
	write_cmos_sensor_8(0x3037, 0x0A);
	write_cmos_sensor_8(0x3038, 0x01);
	write_cmos_sensor_8(0x303E, 0x01);
	write_cmos_sensor_8(0x30A2, 0x0E);
	write_cmos_sensor_8(0x30A5, 0x60);
	write_cmos_sensor_8(0x30A7, 0x40);
	write_cmos_sensor_8(0x31AA, 0x02);
	write_cmos_sensor_8(0x3301, 0x00);
	write_cmos_sensor_8(0x3318, 0x60);
	write_cmos_sensor_8(0x0202, 0x08);
	write_cmos_sensor_8(0x0203, 0x6B);
	write_cmos_sensor_8(0x0204, 0x00);
	write_cmos_sensor_8(0x0205, 0x00);
	write_cmos_sensor_8(0x020E, 0x01);
	write_cmos_sensor_8(0x020F, 0x00);
	write_cmos_sensor_8(0x0210, 0x01);
	write_cmos_sensor_8(0x0211, 0x00);
	write_cmos_sensor_8(0x0212, 0x01);
	write_cmos_sensor_8(0x0213, 0x00);
	write_cmos_sensor_8(0x0214, 0x01);
	write_cmos_sensor_8(0x0215, 0x00);
	write_cmos_sensor_8(0x0100, 0x01);
}	/*	preview_setting  */
static void capture_setting(kal_uint16 currefps)
{
	LOG_INF("capture_setting() E\n");
	write_cmos_sensor_8(0x0100, 0x00);
	write_cmos_sensor_8(0x0340, 0x08);
	write_cmos_sensor_8(0x0341, 0x70);
	write_cmos_sensor_8(0x0342, 0x05);
	write_cmos_sensor_8(0x0343, 0xB0);
	write_cmos_sensor_8(0x0344, 0x00);
	write_cmos_sensor_8(0x0345, 0x00);
	write_cmos_sensor_8(0x0346, 0x00);
	write_cmos_sensor_8(0x0347, 0x00);
	write_cmos_sensor_8(0x0348, 0x0A);
	write_cmos_sensor_8(0x0349, 0x1F);
	write_cmos_sensor_8(0x034A, 0x07);
	write_cmos_sensor_8(0x034B, 0x97);
	write_cmos_sensor_8(0x0381, 0x01);
	write_cmos_sensor_8(0x0383, 0x01);
	write_cmos_sensor_8(0x0385, 0x01);
	write_cmos_sensor_8(0x0387, 0x01);
	write_cmos_sensor_8(0x3048, 0x20);
	write_cmos_sensor_8(0x30D5, 0x00);
	write_cmos_sensor_8(0x3165, 0x20);
	write_cmos_sensor_8(0x30D1, 0x00);
	write_cmos_sensor_8(0x30D0, 0x2A);
	write_cmos_sensor_8(0x3102, 0x13);
	write_cmos_sensor_8(0x3103, 0x47);
	write_cmos_sensor_8(0x3049, 0x01);
	write_cmos_sensor_8(0x304D, 0x02);
	write_cmos_sensor_8(0x304C, 0xD7);
	write_cmos_sensor_8(0x0112, 0x0A);
	write_cmos_sensor_8(0x0113, 0x0A);
	write_cmos_sensor_8(0x034C, 0x0A);
	write_cmos_sensor_8(0x034D, 0x20);
	write_cmos_sensor_8(0x034E, 0x07);
	write_cmos_sensor_8(0x034F, 0x98);
	write_cmos_sensor_8(0x0305, 0x03);
	write_cmos_sensor_8(0x0307, 0x76);
	write_cmos_sensor_8(0x3037, 0x0A);
	write_cmos_sensor_8(0x3038, 0x01);
	write_cmos_sensor_8(0x303E, 0x01);
	write_cmos_sensor_8(0x30A2, 0x0E);
	write_cmos_sensor_8(0x30A5, 0x60);
	write_cmos_sensor_8(0x30A7, 0x40);
	write_cmos_sensor_8(0x31AA, 0x02);
	write_cmos_sensor_8(0x3301, 0x00);
	write_cmos_sensor_8(0x3318, 0x60);
	write_cmos_sensor_8(0x0202, 0x08);
	write_cmos_sensor_8(0x0203, 0x6B);
	write_cmos_sensor_8(0x0204, 0x00);
	write_cmos_sensor_8(0x0205, 0x00);
	write_cmos_sensor_8(0x020E, 0x01);
	write_cmos_sensor_8(0x020F, 0x00);
	write_cmos_sensor_8(0x0210, 0x01);
	write_cmos_sensor_8(0x0211, 0x00);
	write_cmos_sensor_8(0x0212, 0x01);
	write_cmos_sensor_8(0x0213, 0x00);
	write_cmos_sensor_8(0x0214, 0x01);
	write_cmos_sensor_8(0x0215, 0x00);
	write_cmos_sensor_8(0x0100, 0x01);
}
static void normal_video_setting(kal_uint16 currefps)
{
	LOG_INF("normal_video_setting() E\n");
	write_cmos_sensor_8(0x0100, 0x00);
	write_cmos_sensor_8(0x0340, 0x08);
	write_cmos_sensor_8(0x0341, 0x70);
	write_cmos_sensor_8(0x0342, 0x05);
	write_cmos_sensor_8(0x0343, 0xB0);
	write_cmos_sensor_8(0x0344, 0x00);
	write_cmos_sensor_8(0x0345, 0x00);
	write_cmos_sensor_8(0x0346, 0x00);
	write_cmos_sensor_8(0x0347, 0x00);
	write_cmos_sensor_8(0x0348, 0x0A);
	write_cmos_sensor_8(0x0349, 0x1F);
	write_cmos_sensor_8(0x034A, 0x07);
	write_cmos_sensor_8(0x034B, 0x97);
	write_cmos_sensor_8(0x0381, 0x01);
	write_cmos_sensor_8(0x0383, 0x01);
	write_cmos_sensor_8(0x0385, 0x01);
	write_cmos_sensor_8(0x0387, 0x01);
	write_cmos_sensor_8(0x3048, 0x20);
	write_cmos_sensor_8(0x30D5, 0x00);
	write_cmos_sensor_8(0x3165, 0x20);
	write_cmos_sensor_8(0x30D1, 0x00);
	write_cmos_sensor_8(0x30D0, 0x2A);
	write_cmos_sensor_8(0x3102, 0x13);
	write_cmos_sensor_8(0x3103, 0x47);
	write_cmos_sensor_8(0x3049, 0x01);
	write_cmos_sensor_8(0x304D, 0x02);
	write_cmos_sensor_8(0x304C, 0xD7);
	write_cmos_sensor_8(0x0112, 0x0A);
	write_cmos_sensor_8(0x0113, 0x0A);
	write_cmos_sensor_8(0x034C, 0x0A);
	write_cmos_sensor_8(0x034D, 0x20);
	write_cmos_sensor_8(0x034E, 0x07);
	write_cmos_sensor_8(0x034F, 0x98);
	write_cmos_sensor_8(0x0305, 0x03);
	write_cmos_sensor_8(0x0307, 0x76);
	write_cmos_sensor_8(0x3037, 0x0A);
	write_cmos_sensor_8(0x3038, 0x01);
	write_cmos_sensor_8(0x303E, 0x01);
	write_cmos_sensor_8(0x30A2, 0x0E);
	write_cmos_sensor_8(0x30A5, 0x60);
	write_cmos_sensor_8(0x30A7, 0x40);
	write_cmos_sensor_8(0x31AA, 0x02);
	write_cmos_sensor_8(0x3301, 0x00);
	write_cmos_sensor_8(0x3318, 0x60);
	write_cmos_sensor_8(0x0202, 0x08);
	write_cmos_sensor_8(0x0203, 0x6B);
	write_cmos_sensor_8(0x0204, 0x00);
	write_cmos_sensor_8(0x0205, 0x00);
	write_cmos_sensor_8(0x020E, 0x01);
	write_cmos_sensor_8(0x020F, 0x00);
	write_cmos_sensor_8(0x0210, 0x01);
	write_cmos_sensor_8(0x0211, 0x00);
	write_cmos_sensor_8(0x0212, 0x01);
	write_cmos_sensor_8(0x0213, 0x00);
	write_cmos_sensor_8(0x0214, 0x01);
	write_cmos_sensor_8(0x0215, 0x00);
	write_cmos_sensor_8(0x0100, 0x01);
}
static void hs_video_setting(void)
{
	LOG_INF("Enter video 16:9 setting() E\n");
	write_cmos_sensor_8(0x0100, 0x00);
	write_cmos_sensor_8(0x0340, 0x08);
	write_cmos_sensor_8(0x0341, 0x70);
	write_cmos_sensor_8(0x0342, 0x05);
	write_cmos_sensor_8(0x0343, 0xB0);
	write_cmos_sensor_8(0x0344, 0x00);
	write_cmos_sensor_8(0x0345, 0x00);
	write_cmos_sensor_8(0x0346, 0x00);
	write_cmos_sensor_8(0x0347, 0xF0);
	write_cmos_sensor_8(0x0348, 0x0A);
	write_cmos_sensor_8(0x0349, 0x1F);
	write_cmos_sensor_8(0x034A, 0x06);
	write_cmos_sensor_8(0x034B, 0xA7);
	write_cmos_sensor_8(0x0381, 0x01);
	write_cmos_sensor_8(0x0383, 0x01);
	write_cmos_sensor_8(0x0385, 0x01);
	write_cmos_sensor_8(0x0387, 0x01);
	write_cmos_sensor_8(0x3048, 0x20);
	write_cmos_sensor_8(0x30D5, 0x00);
	write_cmos_sensor_8(0x3165, 0x20);
	write_cmos_sensor_8(0x30D1, 0x00);
	write_cmos_sensor_8(0x30D0, 0x2A);
	write_cmos_sensor_8(0x3102, 0x13);
	write_cmos_sensor_8(0x3103, 0x47);
	write_cmos_sensor_8(0x3049, 0x01);
	write_cmos_sensor_8(0x304D, 0x02);
	write_cmos_sensor_8(0x304C, 0xD7);
	write_cmos_sensor_8(0x0112, 0x0A);
	write_cmos_sensor_8(0x0113, 0x0A);
	write_cmos_sensor_8(0x034C, 0x0A);
	write_cmos_sensor_8(0x034D, 0x20);
	write_cmos_sensor_8(0x034E, 0x05);
	write_cmos_sensor_8(0x034F, 0xB2);
	write_cmos_sensor_8(0x0305, 0x03);
	write_cmos_sensor_8(0x0307, 0x76);
	write_cmos_sensor_8(0x3037, 0x0A);
	write_cmos_sensor_8(0x3038, 0x01);
	write_cmos_sensor_8(0x303E, 0x01);
	write_cmos_sensor_8(0x30A2, 0x0E);
	write_cmos_sensor_8(0x30A5, 0x60);
	write_cmos_sensor_8(0x30A7, 0x40);
	write_cmos_sensor_8(0x31AA, 0x02);
	write_cmos_sensor_8(0x3301, 0x00);
	write_cmos_sensor_8(0x3318, 0x60);
	write_cmos_sensor_8(0x0202, 0x08);
	write_cmos_sensor_8(0x0203, 0x6B);
	write_cmos_sensor_8(0x0204, 0x00);
	write_cmos_sensor_8(0x0205, 0x00);
	write_cmos_sensor_8(0x020E, 0x01);
	write_cmos_sensor_8(0x020F, 0x00);
	write_cmos_sensor_8(0x0210, 0x01);
	write_cmos_sensor_8(0x0211, 0x00);
	write_cmos_sensor_8(0x0212, 0x01);
	write_cmos_sensor_8(0x0213, 0x00);
	write_cmos_sensor_8(0x0214, 0x01);
	write_cmos_sensor_8(0x0215, 0x00);
	write_cmos_sensor_8(0x0100, 0x01);
}
static void slim_video_setting(void)
{
	LOG_INF("slim_video_setting() E\n");
	write_cmos_sensor_8(0x0100, 0x00);
	write_cmos_sensor_8(0x0340, 0x08);
	write_cmos_sensor_8(0x0341, 0x70);
	write_cmos_sensor_8(0x0342, 0x05);
	write_cmos_sensor_8(0x0343, 0xB0);
	write_cmos_sensor_8(0x0344, 0x00);
	write_cmos_sensor_8(0x0345, 0x00);
	write_cmos_sensor_8(0x0346, 0x00);
	write_cmos_sensor_8(0x0347, 0x00);
	write_cmos_sensor_8(0x0348, 0x0A);
	write_cmos_sensor_8(0x0349, 0x1F);
	write_cmos_sensor_8(0x034A, 0x07);
	write_cmos_sensor_8(0x034B, 0x97);
	write_cmos_sensor_8(0x0381, 0x01);
	write_cmos_sensor_8(0x0383, 0x01);
	write_cmos_sensor_8(0x0385, 0x01);
	write_cmos_sensor_8(0x0387, 0x03);
	write_cmos_sensor_8(0x3048, 0x25);
	write_cmos_sensor_8(0x30D5, 0x00);
	write_cmos_sensor_8(0x3165, 0x28);
	write_cmos_sensor_8(0x30D1, 0x00);
	write_cmos_sensor_8(0x30D0, 0x2A);
	write_cmos_sensor_8(0x3102, 0x13);
	write_cmos_sensor_8(0x3103, 0x47);
	write_cmos_sensor_8(0x3049, 0x01);
	write_cmos_sensor_8(0x304D, 0x02);
	write_cmos_sensor_8(0x304C, 0xD7);
	write_cmos_sensor_8(0x0112, 0x0A);
	write_cmos_sensor_8(0x0113, 0x0A);
	write_cmos_sensor_8(0x034C, 0x05);
	write_cmos_sensor_8(0x034D, 0x10);
	write_cmos_sensor_8(0x034E, 0x03);
	write_cmos_sensor_8(0x034F, 0xCC);
	write_cmos_sensor_8(0x0305, 0x03);
	write_cmos_sensor_8(0x0307, 0x76);
	write_cmos_sensor_8(0x3037, 0x0A);
	write_cmos_sensor_8(0x3038, 0x01);
	write_cmos_sensor_8(0x303E, 0x01);
	write_cmos_sensor_8(0x30A2, 0x0E);
	write_cmos_sensor_8(0x30A5, 0x60);
	write_cmos_sensor_8(0x30A7, 0x40);
	write_cmos_sensor_8(0x31AA, 0x02);
	write_cmos_sensor_8(0x3301, 0x00);
	write_cmos_sensor_8(0x3318, 0x60);
	write_cmos_sensor_8(0x0202, 0x08);
	write_cmos_sensor_8(0x0203, 0x6B);
	write_cmos_sensor_8(0x0204, 0x00);
	write_cmos_sensor_8(0x0205, 0x00);
	write_cmos_sensor_8(0x020E, 0x01);
	write_cmos_sensor_8(0x020F, 0x00);
	write_cmos_sensor_8(0x0210, 0x01);
	write_cmos_sensor_8(0x0211, 0x00);
	write_cmos_sensor_8(0x0212, 0x01);
	write_cmos_sensor_8(0x0213, 0x00);
	write_cmos_sensor_8(0x0214, 0x01);
	write_cmos_sensor_8(0x0215, 0x00);
	write_cmos_sensor_8(0x0100, 0x01);
}
static void custom1_setting(void)
{
	/* fullsize 24fps */
	LOG_INF("custom1_setting() E\n");
	write_cmos_sensor_8(0x0100, 0x00);
	write_cmos_sensor_8(0x0340, 0x0A);/*0x0340, 0x08*/
	write_cmos_sensor_8(0x0341, 0x80);/*0x0341, 0x70*/
	write_cmos_sensor_8(0x0342, 0x05);
	write_cmos_sensor_8(0x0343, 0xB0);
	write_cmos_sensor_8(0x0344, 0x00);
	write_cmos_sensor_8(0x0345, 0x00);
	write_cmos_sensor_8(0x0346, 0x00);
	write_cmos_sensor_8(0x0347, 0x00);
	write_cmos_sensor_8(0x0348, 0x0A);
	write_cmos_sensor_8(0x0349, 0x1F);
	write_cmos_sensor_8(0x034A, 0x07);
	write_cmos_sensor_8(0x034B, 0x97);
	write_cmos_sensor_8(0x0381, 0x01);
	write_cmos_sensor_8(0x0383, 0x01);
	write_cmos_sensor_8(0x0385, 0x01);
	write_cmos_sensor_8(0x0387, 0x01);
	write_cmos_sensor_8(0x3048, 0x20);
	write_cmos_sensor_8(0x30D5, 0x00);
	write_cmos_sensor_8(0x3165, 0x20);
	write_cmos_sensor_8(0x30D1, 0x00);
	write_cmos_sensor_8(0x30D0, 0x2A);
	write_cmos_sensor_8(0x3102, 0x13);
	write_cmos_sensor_8(0x3103, 0x47);
	write_cmos_sensor_8(0x3049, 0x01);
	write_cmos_sensor_8(0x304D, 0x02);
	write_cmos_sensor_8(0x304C, 0xD7);
	write_cmos_sensor_8(0x0112, 0x0A);
	write_cmos_sensor_8(0x0113, 0x0A);
	write_cmos_sensor_8(0x034C, 0x0A);
	write_cmos_sensor_8(0x034D, 0x20);
	write_cmos_sensor_8(0x034E, 0x07);
	write_cmos_sensor_8(0x034F, 0x98);
	write_cmos_sensor_8(0x0305, 0x03);
	write_cmos_sensor_8(0x0307, 0x76);
	write_cmos_sensor_8(0x3037, 0x0A);
	write_cmos_sensor_8(0x3038, 0x01);
	write_cmos_sensor_8(0x303E, 0x01);
	write_cmos_sensor_8(0x30A2, 0x0E);
	write_cmos_sensor_8(0x30A5, 0x60);
	write_cmos_sensor_8(0x30A7, 0x40);
	write_cmos_sensor_8(0x31AA, 0x02);
	write_cmos_sensor_8(0x3301, 0x00);
	write_cmos_sensor_8(0x3318, 0x60);
	write_cmos_sensor_8(0x0202, 0x08);
	write_cmos_sensor_8(0x0203, 0x6B);
	write_cmos_sensor_8(0x0204, 0x00);
	write_cmos_sensor_8(0x0205, 0x00);
	write_cmos_sensor_8(0x020E, 0x01);
	write_cmos_sensor_8(0x020F, 0x00);
	write_cmos_sensor_8(0x0210, 0x01);
	write_cmos_sensor_8(0x0211, 0x00);
	write_cmos_sensor_8(0x0212, 0x01);
	write_cmos_sensor_8(0x0213, 0x00);
	write_cmos_sensor_8(0x0214, 0x01);
	write_cmos_sensor_8(0x0215, 0x00);
	write_cmos_sensor_8(0x0100, 0x01);
}
static void custom2_setting(void)
{
	/*binnig size 24fps */
	LOG_INF("custom2_setting() E\n");
	write_cmos_sensor_8(0x0100, 0x00);
	write_cmos_sensor_8(0x0340, 0x0A);
	write_cmos_sensor_8(0x0341, 0x80);
	write_cmos_sensor_8(0x0342, 0x05);
	write_cmos_sensor_8(0x0343, 0xB0);
	write_cmos_sensor_8(0x0344, 0x00);
	write_cmos_sensor_8(0x0345, 0x00);
	write_cmos_sensor_8(0x0346, 0x00);
	write_cmos_sensor_8(0x0347, 0x00);
	write_cmos_sensor_8(0x0348, 0x0A);
	write_cmos_sensor_8(0x0349, 0x1F);
	write_cmos_sensor_8(0x034A, 0x07);
	write_cmos_sensor_8(0x034B, 0x97);
	write_cmos_sensor_8(0x0381, 0x01);
	write_cmos_sensor_8(0x0383, 0x01);
	write_cmos_sensor_8(0x0385, 0x01);
	write_cmos_sensor_8(0x0387, 0x03);
	write_cmos_sensor_8(0x3048, 0x25);
	write_cmos_sensor_8(0x30D5, 0x00);
	write_cmos_sensor_8(0x3165, 0x28);
	write_cmos_sensor_8(0x30D1, 0x00);
	write_cmos_sensor_8(0x30D0, 0x2A);
	write_cmos_sensor_8(0x3102, 0x13);
	write_cmos_sensor_8(0x3103, 0x47);
	write_cmos_sensor_8(0x3049, 0x01);
	write_cmos_sensor_8(0x304D, 0x02);
	write_cmos_sensor_8(0x304C, 0xD7);
	write_cmos_sensor_8(0x0112, 0x0A);
	write_cmos_sensor_8(0x0113, 0x0A);
	write_cmos_sensor_8(0x034C, 0x05);
	write_cmos_sensor_8(0x034D, 0x10);
	write_cmos_sensor_8(0x034E, 0x03);
	write_cmos_sensor_8(0x034F, 0xCC);
	write_cmos_sensor_8(0x0305, 0x03);
	write_cmos_sensor_8(0x0307, 0x76);
	write_cmos_sensor_8(0x3037, 0x0A);
	write_cmos_sensor_8(0x3038, 0x01);
	write_cmos_sensor_8(0x303E, 0x01);
	write_cmos_sensor_8(0x30A2, 0x0E);
	write_cmos_sensor_8(0x30A5, 0x60);
	write_cmos_sensor_8(0x30A7, 0x40);
	write_cmos_sensor_8(0x31AA, 0x02);
	write_cmos_sensor_8(0x3301, 0x00);
	write_cmos_sensor_8(0x3318, 0x60);
	write_cmos_sensor_8(0x0202, 0x08);
	write_cmos_sensor_8(0x0203, 0x6B);
	write_cmos_sensor_8(0x0204, 0x00);
	write_cmos_sensor_8(0x0205, 0x00);
	write_cmos_sensor_8(0x020E, 0x01);
	write_cmos_sensor_8(0x020F, 0x00);
	write_cmos_sensor_8(0x0210, 0x01);
	write_cmos_sensor_8(0x0211, 0x00);
	write_cmos_sensor_8(0x0212, 0x01);
	write_cmos_sensor_8(0x0213, 0x00);
	write_cmos_sensor_8(0x0214, 0x01);
	write_cmos_sensor_8(0x0215, 0x00);
	write_cmos_sensor_8(0x0100, 0x01);
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

	while (imgsensor_info.i2c_addr_table[i] != 0xff) {
		spin_lock(&imgsensor_drv_lock);
		imgsensor.i2c_write_id = imgsensor_info.i2c_addr_table[i];
		spin_unlock(&imgsensor_drv_lock);
		do {
			*sensor_id = ((read_cmos_sensor_8(0x0000) << 8) | read_cmos_sensor_8(0x0001));
			/* *sensor_id = imgsensor_info.sensor_id;*/
			if (*sensor_id == imgsensor_info.sensor_id) {
				LOG_INF("i2c write id: 0x%x, sensor id: 0x%x\n", imgsensor.i2c_write_id, *sensor_id);
				return ERROR_NONE;
			}
			LOG_INF("Read sensor id fail, write id: 0x%x, id: 0x%x\n",
				imgsensor.i2c_write_id, *sensor_id);
			retry--;
		} while (retry > 0);
		i++;
		retry = 2;
	}
	if (*sensor_id != imgsensor_info.sensor_id) {
		/*if Sensor ID is not correct, Must set *sensor_id to 0xFFFFFFFF */
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
	/* const kal_uint8 i2c_addr[] = {IMGSENSOR_WRITE_ID_1, IMGSENSOR_WRITE_ID_2}; */
	kal_uint8 i = 0;
	kal_uint8 retry = 2;
	kal_uint32 sensor_id = 0;

	LOG_1;
	LOG_2;
	while (imgsensor_info.i2c_addr_table[i] != 0xff) {
		spin_lock(&imgsensor_drv_lock);
		imgsensor.i2c_write_id = imgsensor_info.i2c_addr_table[i];
		spin_unlock(&imgsensor_drv_lock);
		do {
			sensor_id = ((read_cmos_sensor_8(0x0000) << 8) | read_cmos_sensor_8(0x0001));
			if (sensor_id == imgsensor_info.sensor_id) {
				LOG_INF("i2c write id: 0x%x, sensor id: 0x%x\n", imgsensor.i2c_write_id, sensor_id);
				break;
			}
			LOG_INF("Read sensor id fail, id: 0x%x\n", sensor_id);
			retry--;
		} while (retry > 0);
		i++;
		if (sensor_id == imgsensor_info.sensor_id)
			break;
		retry = 2;
	}
	if (imgsensor_info.sensor_id != sensor_id)
		return ERROR_SENSOR_CONNECT_FAIL;
	/* initail sequence write in  */
	sensor_init();
	spin_lock(&imgsensor_drv_lock);
	imgsensor.autoflicker_en = KAL_FALSE;
	imgsensor.sensor_mode = IMGSENSOR_MODE_INIT;
	imgsensor.pclk = imgsensor_info.pre.pclk;
	imgsensor.frame_length = imgsensor_info.pre.framelength;
	imgsensor.line_length = imgsensor_info.pre.linelength;
	imgsensor.min_frame_length = imgsensor_info.pre.framelength;
	imgsensor.dummy_pixel = 0;
	imgsensor.dummy_line = 0;
	imgsensor.ihdr_en = KAL_FALSE;
	imgsensor.test_pattern = KAL_FALSE;
	imgsensor.current_fps = imgsensor_info.pre.max_framerate;
	spin_unlock(&imgsensor_drv_lock);
	return ERROR_NONE;
}	/*	open  */
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
	LOG_INF("E\n");
	/*No Need to implement this function*/
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
	LOG_INF("E\n");
	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_PREVIEW;
	imgsensor.pclk = imgsensor_info.pre.pclk;
	/*imgsensor.video_mode = KAL_FALSE;*/
	imgsensor.line_length = imgsensor_info.pre.linelength;
	imgsensor.frame_length = imgsensor_info.pre.framelength;
	imgsensor.min_frame_length = imgsensor_info.pre.framelength;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	preview_setting();
	set_mirror_flip(IMAGE_NORMAL);
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
	LOG_INF("E\n");
	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_CAPTURE;
	imgsensor.pclk = imgsensor_info.cap.pclk;
	imgsensor.line_length = imgsensor_info.cap.linelength;
	imgsensor.frame_length = imgsensor_info.cap.framelength;
	imgsensor.min_frame_length = imgsensor_info.cap.framelength;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	LOG_INF("Caputre fps:%d\n", imgsensor.current_fps);
	capture_setting(imgsensor.current_fps);
	set_mirror_flip(IMAGE_NORMAL);
	return ERROR_NONE;
}	/* capture() */
static kal_uint32 normal_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
					  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("E\n");
	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_VIDEO;
	imgsensor.pclk = imgsensor_info.normal_video.pclk;
	imgsensor.line_length = imgsensor_info.normal_video.linelength;
	imgsensor.frame_length = imgsensor_info.normal_video.framelength;
	imgsensor.min_frame_length = imgsensor_info.normal_video.framelength;
	/*imgsensor.current_fps = 300;*/
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	normal_video_setting(imgsensor.current_fps);
	set_mirror_flip(IMAGE_NORMAL);
	return ERROR_NONE;
}	/*	normal_video   */
static kal_uint32 hs_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
					  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("E\n");
	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_HIGH_SPEED_VIDEO;
	imgsensor.pclk = imgsensor_info.hs_video.pclk;
	/* imgsensor.video_mode = KAL_TRUE; */
	imgsensor.line_length = imgsensor_info.hs_video.linelength;
	imgsensor.frame_length = imgsensor_info.hs_video.framelength;
	imgsensor.min_frame_length = imgsensor_info.hs_video.framelength;
	imgsensor.dummy_line = 0;
	imgsensor.dummy_pixel = 0;
	/* imgsensor.current_fps = 300; */
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	hs_video_setting();
	return ERROR_NONE;
}	/*	hs_video   */
static kal_uint32 slim_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
					  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("E\n");
	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_SLIM_VIDEO;
	imgsensor.pclk = imgsensor_info.slim_video.pclk;
	/*imgsensor.video_mode = KAL_TRUE;*/
	imgsensor.line_length = imgsensor_info.slim_video.linelength;
	imgsensor.frame_length = imgsensor_info.slim_video.framelength;
	imgsensor.min_frame_length = imgsensor_info.slim_video.framelength;
	imgsensor.dummy_line = 0;
	imgsensor.dummy_pixel = 0;
	/*imgsensor.current_fps = 300;*/
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	slim_video_setting();
	set_mirror_flip(IMAGE_NORMAL);
	return ERROR_NONE;
}   /*  slim_video   */
/*************************************************************************
* FUNCTION
* Custom1
*
* DESCRIPTION
*   This function start the sensor Custom1.
*
* PARAMETERS
*   *image_window : address pointer of pixel numbers in one period of HSYNC
*  *sensor_config_data : address pointer of line numbers in one period of VSYNC
*
* RETURNS
*   None
*
* GLOBALS AFFECTED
*
*************************************************************************/
static kal_uint32 Custom1(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
					MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("E\n");
	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_CUSTOM1;
	imgsensor.pclk = imgsensor_info.custom1.pclk;
	/*imgsensor.video_mode = KAL_FALSE;*/
	imgsensor.line_length = imgsensor_info.custom1.linelength;
	imgsensor.frame_length = imgsensor_info.custom1.framelength;
	imgsensor.min_frame_length = imgsensor_info.custom1.framelength;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	custom1_setting();
	set_mirror_flip(IMAGE_NORMAL);
	return ERROR_NONE;
}   /*  Custom1   */
static kal_uint32 Custom2(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
					MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("E\n");
	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_CUSTOM2;
	imgsensor.pclk = imgsensor_info.custom2.pclk;
	/*imgsensor.video_mode = KAL_FALSE;*/
	imgsensor.line_length = imgsensor_info.custom2.linelength;
	imgsensor.frame_length = imgsensor_info.custom2.framelength;
	imgsensor.min_frame_length = imgsensor_info.custom2.framelength;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	custom2_setting();
	set_mirror_flip(IMAGE_NORMAL);
	return ERROR_NONE;
}   /*  Custom2   */
static kal_uint32 get_resolution(MSDK_SENSOR_RESOLUTION_INFO_STRUCT *sensor_resolution)
{
	LOG_INF("E\n");
	sensor_resolution->SensorFullWidth = imgsensor_info.cap.grabwindow_width;
	sensor_resolution->SensorFullHeight = imgsensor_info.cap.grabwindow_height;
	sensor_resolution->SensorPreviewWidth = imgsensor_info.pre.grabwindow_width;
	sensor_resolution->SensorPreviewHeight = imgsensor_info.pre.grabwindow_height;
	sensor_resolution->SensorVideoWidth = imgsensor_info.normal_video.grabwindow_width;
	sensor_resolution->SensorVideoHeight = imgsensor_info.normal_video.grabwindow_height;
	sensor_resolution->SensorHighSpeedVideoWidth	 = imgsensor_info.hs_video.grabwindow_width;
	sensor_resolution->SensorHighSpeedVideoHeight	 = imgsensor_info.hs_video.grabwindow_height;
	sensor_resolution->SensorSlimVideoWidth	 = imgsensor_info.slim_video.grabwindow_width;
	sensor_resolution->SensorSlimVideoHeight	 = imgsensor_info.slim_video.grabwindow_height;
	sensor_resolution->SensorCustom1Width  = imgsensor_info.custom1.grabwindow_width;
	sensor_resolution->SensorCustom1Height     = imgsensor_info.custom1.grabwindow_height;
	sensor_resolution->SensorCustom2Width  = imgsensor_info.custom2.grabwindow_width;
	sensor_resolution->SensorCustom2Height     = imgsensor_info.custom2.grabwindow_height;
	return ERROR_NONE;
}	/*	get_resolution	*/
static kal_uint32 get_info(MSDK_SCENARIO_ID_ENUM scenario_id,
					  MSDK_SENSOR_INFO_STRUCT *sensor_info,
					  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("scenario_id = %d\n", scenario_id);
	sensor_info->SensorClockPolarity = SENSOR_CLOCK_POLARITY_LOW;
	sensor_info->SensorClockFallingPolarity = SENSOR_CLOCK_POLARITY_LOW; /* not use */
	sensor_info->SensorHsyncPolarity = SENSOR_CLOCK_POLARITY_LOW; /* inverse with datasheet */
	sensor_info->SensorVsyncPolarity = SENSOR_CLOCK_POLARITY_LOW;
	sensor_info->SensorInterruptDelayLines = 1; /* not use */
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
	sensor_info->SlimVideoDelayFrame = imgsensor_info.slim_video_delay_frame;
	sensor_info->Custom1DelayFrame = imgsensor_info.custom1_delay_frame;
	sensor_info->Custom2DelayFrame = imgsensor_info.custom2_delay_frame;
	sensor_info->SensorMasterClockSwitch = 0; /* not use */
	sensor_info->SensorDrivingCurrent = imgsensor_info.isp_driving_current;
	sensor_info->AEShutDelayFrame = imgsensor_info.ae_shut_delay_frame;
	/* The frame of setting shutter default 0 for TG int */
	sensor_info->AESensorGainDelayFrame = imgsensor_info.ae_sensor_gain_delay_frame;
	sensor_info->AEISPGainDelayFrame = imgsensor_info.ae_ispGain_delay_frame;
	sensor_info->IHDR_Support = imgsensor_info.ihdr_support;
	sensor_info->IHDR_LE_FirstLine = imgsensor_info.ihdr_le_firstline;
	sensor_info->SensorModeNum = imgsensor_info.sensor_mode_num;
	sensor_info->SensorMIPILaneNumber = imgsensor_info.mipi_lane_num;
	sensor_info->SensorClockFreq = imgsensor_info.mclk;
	sensor_info->SensorClockDividCount = 5; /* not use */
	sensor_info->SensorClockRisingCount = 0;
	sensor_info->SensorClockFallingCount = 2; /* not use */
	sensor_info->SensorPixelClockCount = 3; /* not use */
	sensor_info->SensorDataLatchCount = 2; /* not use */
	sensor_info->MIPIDataLowPwr2HighSpeedTermDelayCount = 0;
	sensor_info->MIPICLKLowPwr2HighSpeedTermDelayCount = 0;
	sensor_info->SensorWidthSampling = 0;  /*0 is default 1x*/
	sensor_info->SensorHightSampling = 0;	/* 0 is default 1x */
	sensor_info->SensorPacketECCOrder = 1;
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
		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
						imgsensor_info.normal_video.mipi_data_lp2hs_settle_dc;
		break;
	case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
		sensor_info->SensorGrabStartX = imgsensor_info.hs_video.startx;
		sensor_info->SensorGrabStartY = imgsensor_info.hs_video.starty;
		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
						imgsensor_info.hs_video.mipi_data_lp2hs_settle_dc;
		break;
	case MSDK_SCENARIO_ID_SLIM_VIDEO:
		sensor_info->SensorGrabStartX = imgsensor_info.slim_video.startx;
		sensor_info->SensorGrabStartY = imgsensor_info.slim_video.starty;
		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
						imgsensor_info.slim_video.mipi_data_lp2hs_settle_dc;
		break;
	case MSDK_SCENARIO_ID_CUSTOM1:
		sensor_info->SensorGrabStartX = imgsensor_info.custom1.startx;
		sensor_info->SensorGrabStartY = imgsensor_info.custom1.starty;
		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
						imgsensor_info.custom1.mipi_data_lp2hs_settle_dc;
		break;
	case MSDK_SCENARIO_ID_CUSTOM2:
		sensor_info->SensorGrabStartX = imgsensor_info.custom2.startx;
		sensor_info->SensorGrabStartY = imgsensor_info.custom2.starty;
		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
						imgsensor_info.custom1.mipi_data_lp2hs_settle_dc;
		break;
	default:
		sensor_info->SensorGrabStartX = imgsensor_info.pre.startx;
		sensor_info->SensorGrabStartY = imgsensor_info.pre.starty;
		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount = imgsensor_info.pre.mipi_data_lp2hs_settle_dc;
		break;
	}
	return ERROR_NONE;
}	/*	get_info  */
static kal_uint32 set_test_pattern_mode(kal_bool enable)
{
	LOG_INF("enable: %d\n", enable);
	if (enable) {
		/* 0x5E00[8]: 1 enable,  0 disable*/
		/* 0x5E00[1:0]; 00 Color bar, 01 Random Data, 10 Square, 11 BLACK */
		write_cmos_sensor(0x3220, 0x0001);
		write_cmos_sensor(0x0601, 0x0002);
	} else {
		/* 0x5E00[8]: 1 enable,  0 disable */
		/* 0x5E00[1:0]; 00 Color bar, 01 Random Data, 10 Square, 11 BLACK */
		write_cmos_sensor(0x3220, 0x0000);
		write_cmos_sensor(0x0601, 0x0000);
	}
	spin_lock(&imgsensor_drv_lock);
	imgsensor.test_pattern = enable;
	spin_unlock(&imgsensor_drv_lock);
	return ERROR_NONE;
}
static kal_uint32 control(MSDK_SCENARIO_ID_ENUM scenario_id, MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
					  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("scenario_id = %d\n", scenario_id);
	spin_lock(&imgsensor_drv_lock);
	imgsensor.current_scenario_id = scenario_id;
	spin_unlock(&imgsensor_drv_lock);
	switch (scenario_id) {
	case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		preview(image_window, sensor_config_data);
		break;
	case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
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
		Custom1(image_window, sensor_config_data);
		break;
	case MSDK_SCENARIO_ID_CUSTOM2:
		Custom2(image_window, sensor_config_data);
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
	if (framerate == 0)
		/* Dynamic frame rate */
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
	LOG_INF("enable = %d, framerate = %d\n", enable, framerate);
	spin_lock(&imgsensor_drv_lock);
	if (enable) /* enable auto flicker */
		imgsensor.autoflicker_en = KAL_TRUE;
	else /* Cancel Auto flick */
		imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	return ERROR_NONE;
}
static kal_uint32 set_max_framerate_by_scenario(MSDK_SCENARIO_ID_ENUM scenario_id, MUINT32 framerate)
{
	kal_uint32 frame_length;

	LOG_INF("scenario_id = %d, framerate = %d\n", scenario_id, framerate);
	switch (scenario_id) {
	case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		frame_length = imgsensor_info.pre.pclk / framerate * 10 / imgsensor_info.pre.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line = (frame_length > imgsensor_info.pre.framelength)
						? (frame_length - imgsensor_info.pre.framelength):0;
		imgsensor.frame_length = imgsensor_info.pre.framelength + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		set_dummy();
		break;
	case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
		if (framerate == 0)
			return ERROR_NONE;
		frame_length = imgsensor_info.normal_video.pclk / framerate * 10
						/ imgsensor_info.normal_video.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line = (frame_length > imgsensor_info.normal_video.framelength)
						? (frame_length - imgsensor_info.normal_video.framelength):0;
		imgsensor.frame_length = imgsensor_info.normal_video.framelength + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		set_dummy();
		break;
	case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		frame_length = imgsensor_info.cap.pclk / framerate * 10 / imgsensor_info.cap.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line = (frame_length > imgsensor_info.cap.framelength)
					? (frame_length - imgsensor_info.cap.framelength) : 0;
		imgsensor.frame_length = imgsensor_info.cap.framelength + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		set_dummy();
		break;
	case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
		frame_length = imgsensor_info.hs_video.pclk / framerate * 10 / imgsensor_info.hs_video.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line = (frame_length > imgsensor_info.hs_video.framelength)
						? (frame_length - imgsensor_info.hs_video.framelength) : 0;
		imgsensor.frame_length = imgsensor_info.hs_video.framelength + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		set_dummy();
		break;
	case MSDK_SCENARIO_ID_SLIM_VIDEO:
		frame_length = imgsensor_info.slim_video.pclk / framerate * 10 / imgsensor_info.slim_video.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line = (frame_length > imgsensor_info.slim_video.framelength)
						? (frame_length - imgsensor_info.slim_video.framelength) : 0;
		imgsensor.frame_length = imgsensor_info.slim_video.framelength + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		set_dummy();
		break;
	case MSDK_SCENARIO_ID_CUSTOM1:
		frame_length = imgsensor_info.custom1.pclk / framerate * 10 / imgsensor_info.custom1.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line = (frame_length > imgsensor_info.custom1.framelength)
						? (frame_length - imgsensor_info.custom1.framelength) : 0;
		if (imgsensor.dummy_line < 0)
			imgsensor.dummy_line = 0;
		imgsensor.frame_length = imgsensor_info.custom1.framelength + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		set_dummy();
		break;
	case MSDK_SCENARIO_ID_CUSTOM2:
		frame_length = imgsensor_info.custom2.pclk / framerate * 10 / imgsensor_info.custom2.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line = (frame_length > imgsensor_info.custom2.framelength)
						? (frame_length - imgsensor_info.custom2.framelength) : 0;
		if (imgsensor.dummy_line < 0)
			imgsensor.dummy_line = 0;
		imgsensor.frame_length = imgsensor_info.custom2.framelength + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		set_dummy();
		break;
	default:  /* coding with  preview scenario by default */
		frame_length = imgsensor_info.pre.pclk / framerate * 10 / imgsensor_info.pre.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line = (frame_length > imgsensor_info.pre.framelength)
						? (frame_length - imgsensor_info.pre.framelength) : 0;
		imgsensor.frame_length = imgsensor_info.pre.framelength + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		set_dummy();
		LOG_INF("error scenario_id = %d, we use preview scenario\n", scenario_id);
		break;
	}
	return ERROR_NONE;
}
static kal_uint32 get_default_framerate_by_scenario(MSDK_SCENARIO_ID_ENUM scenario_id, MUINT32 *framerate)
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
	default:
		break;
	}
	return ERROR_NONE;
}
static kal_uint32 feature_control(MSDK_SENSOR_FEATURE_ENUM feature_id,
							 UINT8 *feature_para, UINT32 *feature_para_len)
{
	UINT16 *feature_return_para_16 = (UINT16 *) feature_para;
	UINT16 *feature_data_16 = (UINT16 *) feature_para;
	UINT32 *feature_return_para_32 = (UINT32 *) feature_para;
	UINT32 *feature_data_32 = (UINT32 *) feature_para;
	SENSOR_WINSIZE_INFO_STRUCT *wininfo;
	MSDK_SENSOR_REG_INFO_STRUCT *sensor_reg_data = (MSDK_SENSOR_REG_INFO_STRUCT *) feature_para;
	unsigned long long *feature_data = (unsigned long long *) feature_para;

	LOG_INF("feature_id = %d\n", feature_id);
	switch (feature_id) {
	case SENSOR_FEATURE_GET_PERIOD:
		*feature_return_para_16++ = imgsensor.line_length;
		*feature_return_para_16 = imgsensor.frame_length;
		*feature_para_len = 4;
		break;
	case SENSOR_FEATURE_GET_PIXEL_CLOCK_FREQ:
		LOG_INF("feature_Control imgsensor.pclk = %d,imgsensor.current_fps = %d\n",
					imgsensor.pclk, imgsensor.current_fps);
		*feature_return_para_32 = imgsensor.pclk;
		*feature_para_len = 4;
		break;
	case SENSOR_FEATURE_SET_ESHUTTER:
		set_shutter(*feature_data_16);
		break;
	case SENSOR_FEATURE_SET_NIGHTMODE:
		night_mode((BOOL) * feature_data_16);
		break;
	case SENSOR_FEATURE_SET_GAIN:
		set_gain((UINT16) *feature_data_16);
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
		/* get the lens driver ID from EEPROM or just return LENS_DRIVER_ID_DO_NOT_CARE */
		/* if EEPROM does not exist in camera module. */
		*feature_return_para_32 = LENS_DRIVER_ID_DO_NOT_CARE;
		*feature_para_len = 4;
		break;
	case SENSOR_FEATURE_SET_VIDEO_MODE:
		set_video_mode(*feature_data_16);
		break;
	case SENSOR_FEATURE_CHECK_SENSOR_ID:
		get_imgsensor_id(feature_return_para_32);
		break;
	case SENSOR_FEATURE_SET_AUTO_FLICKER_MODE:
		set_auto_flicker_mode((BOOL)*feature_data_16, *(feature_data_16+1));
		break;
	case SENSOR_FEATURE_SET_MAX_FRAME_RATE_BY_SCENARIO:
		set_max_framerate_by_scenario((MSDK_SCENARIO_ID_ENUM)*feature_data_32, *(feature_data_32+1));
		break;
	case SENSOR_FEATURE_GET_DEFAULT_FRAME_RATE_BY_SCENARIO:
		get_default_framerate_by_scenario((MSDK_SCENARIO_ID_ENUM)*feature_data,
					(MUINT32 *)(uintptr_t)(*(feature_data+1)));
		break;
	case SENSOR_FEATURE_SET_TEST_PATTERN:
		set_test_pattern_mode((BOOL)*feature_data_16);
		break;
	case SENSOR_FEATURE_GET_TEST_PATTERN_CHECKSUM_VALUE: /* for factory mode auto testing */
		*feature_return_para_32 = imgsensor_info.checksum_value;
		*feature_para_len = 4;
		break;
	case SENSOR_FEATURE_SET_FRAMERATE:
		LOG_INF("current fps :%d\n", *feature_data_16);
		spin_lock(&imgsensor_drv_lock);
		imgsensor.current_fps = *feature_data_16;
		spin_unlock(&imgsensor_drv_lock);
		break;
	case SENSOR_FEATURE_SET_HDR:
		LOG_INF("ihdr enable :%d\n", (BOOL)*feature_data_16);
		spin_lock(&imgsensor_drv_lock);
		imgsensor.ihdr_en = (BOOL)*feature_data_16;
		spin_unlock(&imgsensor_drv_lock);
		break;
	case SENSOR_FEATURE_SET_SHUTTER_FRAME_TIME:
		set_shutter_frame_length((UINT16)*feature_data, (UINT16)*(feature_data+1));
		break;
	case SENSOR_FEATURE_GET_CROP_INFO:
		LOG_INF("SENSOR_FEATURE_GET_CROP_INFO scenarioId:%d\n", (int)*feature_data);
		wininfo = (SENSOR_WINSIZE_INFO_STRUCT *)(uintptr_t)(*(feature_data+1));
		switch (*feature_data_32) {
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			memcpy((void *)wininfo, (void *)&imgsensor_winsize_info[1], sizeof(SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			memcpy((void *)wininfo, (void *)&imgsensor_winsize_info[2], sizeof(SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
			memcpy((void *)wininfo, (void *)&imgsensor_winsize_info[3], sizeof(SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
			memcpy((void *)wininfo, (void *)&imgsensor_winsize_info[4], sizeof(SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_CUSTOM1:
			memcpy((void *)wininfo, (void *)&imgsensor_winsize_info[5], sizeof(SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_CUSTOM2:
			memcpy((void *)wininfo, (void *)&imgsensor_winsize_info[6], sizeof(SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		default:
			memcpy((void *)wininfo, (void *)&imgsensor_winsize_info[0], sizeof(SENSOR_WINSIZE_INFO_STRUCT));
			break;
		}
		break;
	case SENSOR_FEATURE_SET_IHDR_SHUTTER_GAIN:
		LOG_INF("SENSOR_SET_SENSOR_IHDR is no support");
		/* LOG_INF("SENSOR_SET_SENSOR_IHDR LE=%d, SE=%d, Gain=%d\n",*/
			/*(UINT16)*feature_data_32,(UINT16)*(feature_data_32+1),(UINT16)*(feature_data_32+2)); */
		/* ihdr_write_shutter_gain((UINT16)*feature_data_32,*/
			/*(UINT16)*(feature_data_32+1),(UINT16)*(feature_data_32+2)); */
		break;
	default:
		break;
	}
	return ERROR_NONE;
}	/*	feature_control()  */
static SENSOR_FUNCTION_STRUCT sensor_func = {
	open,
	get_info,
	get_resolution,
	feature_control,
	control,
	close
};
UINT32 IMX241_MIPI_MONO_SensorInit(PSENSOR_FUNCTION_STRUCT *pfFunc)
{
	/* To Do : Check Sensor status here */
	if (pfFunc != NULL)
		*pfFunc =  &sensor_func;
	return ERROR_NONE;
}	/*	OV5693_MIPI_RAW_SensorInit	*/


