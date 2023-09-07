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
/*****************************************************************************
 *
 * Filename:
 * ---------
 *	 s5k3p8snmipiraw_Sensor.c
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
#include "s5k3p8snmipiraw_Sensor.h"
#define PFX "S5K3P8SN_camera_sensor"
#define LOG_INF(format, args...)	pr_debug(PFX "[%s] " format, __func__, ##args)
static DEFINE_SPINLOCK(imgsensor_drv_lock);
static struct imgsensor_info_struct imgsensor_info = {
	.sensor_id = S5K3P8SN_SENSOR_ID,
	.checksum_value = 0xB1F1B3CC,
	.pre = {
	.pclk = 560000000,              /* record different mode's pclk */
		.linelength = 5120,				/* record different mode's linelength */
		.framelength = 3643,            /* record different mode's framelength */
		.startx = 0,					/* record different mode's startx of grabwindow */
		.starty = 0,					/* record different mode's starty of grabwindow */
		.grabwindow_width = 4640,		/* record different mode's width of grabwindow */
		.grabwindow_height = 3480,		/* record different mode's height of grabwindow */
		/*	 following for MIPIDataLowPwr2HighSpeedSettleDelayCount by different scenario	*/
		.mipi_data_lp2hs_settle_dc = 85,
		/*	 following for GetDefaultFramerateByScenario()	*/
		.max_framerate = 300,           /* 30fps */
	},
	.cap = {
		.pclk = 560000000,
		.linelength = 5120,
		.framelength = 3643,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 4640,
		.grabwindow_height = 3480,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 300,
	},
	.cap1 = {
		.pclk = 560000000,
		.linelength = 6400,
		.framelength = 3643,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 4640,
		.grabwindow_height = 3480,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 240,
	},
	.cap2 = {
		.pclk = 560000000,
		.linelength = 10240,
		.framelength = 3643,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 4640,
		.grabwindow_height = 3480,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 150,
	},
	.normal_video = {
		.pclk = 560000000,
		.linelength = 5120,
		.framelength = 3643,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 4640,
		.grabwindow_height = 3480,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 300,
	},
	.hs_video = {/*slow motion*/
	/* now used for video 16:9 */
		.pclk = 560000000,
		.linelength = 5120,
		.framelength = 3643,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 4640,
		.grabwindow_height = 2612,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 300,
	},
	.slim_video = {/*VT Call*/
		.pclk = 560000000,				/* record different mode's pclk */
		.linelength = 5120,			/* record different mode's linelength */
		.framelength = 3643,	        /* record different mode's framelength */
		/* .framelength = 1822,	 */
		.startx = 0,					/* record different mode's startx of grabwindow */
		.starty = 0,					/* record different mode's starty of grabwindow */
		.grabwindow_width = 2320,		/* record different mode's width of grabwindow */
		.grabwindow_height = 1740,		/* record different mode's height of grabwindow */
		/*	 following for MIPIDataLowPwr2HighSpeedSettleDelayCount by different scenario	*/
		.mipi_data_lp2hs_settle_dc = 85,
		/*	 following for GetDefaultFramerateByScenario()	*/
		.max_framerate = 300,
		/* .max_framerate = 600, */
	},
	.margin = 4,                    /* sensor framelength & shutter margin */
	.min_shutter = 4,               /* min shutter */
	.max_frame_length = 0xffff,     /* max framelength by sensor register's limitation */
	.ae_shut_delay_frame = 0,
	/* shutter delay frame for AE cycle, 2 frame with ispGain_delay-shut_delay=2-0=2 */
	.ae_sensor_gain_delay_frame = 0,
	.ae_ispGain_delay_frame = 2,    /* isp gain delay frame for AE cycle */
	.ihdr_support = 0,	            /* 1, support; 0,not support */
	.ihdr_le_firstline = 0,         /* 1,le first ; 0, se first */
	.sensor_mode_num = 5,	        /* support sensor mode num */
	.cap_delay_frame = 2,           /* enter capture delay frame num */
	.pre_delay_frame = 2,           /* enter preview delay frame num */
	.video_delay_frame = 2,         /* enter video delay frame num */
	.hs_video_delay_frame = 2,      /* enter high speed video  delay frame num */
	.slim_video_delay_frame = 2,    /* enter slim video delay frame num */
	.isp_driving_current = ISP_DRIVING_2MA,     /* mclk driving current */
	.sensor_interface_type = SENSOR_INTERFACE_TYPE_MIPI,    /* sensor_interface_type */
	.mipi_sensor_type = MIPI_OPHY_NCSI2,        /* 0,MIPI_OPHY_NCSI2;  1,MIPI_OPHY_CSI2 */
	.mipi_settle_delay_mode = 0,                /* 0,MIPI_SETTLEDELAY_AUTO; 1,MIPI_SETTLEDELAY_MANNUAL */
	.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_Gr,
	.mclk = 24,                                 /* mclk value, suggest 24 or 26 for 24Mhz or 26Mhz */
	.mipi_lane_num = SENSOR_MIPI_4_LANE,
	.i2c_addr_table = {0x20, 0x5a, 0xff},
};
static struct imgsensor_struct imgsensor = {
	.mirror = IMAGE_NORMAL,				/* mirrorflip information */
	.sensor_mode = IMGSENSOR_MODE_INIT, /* record current sensor mode,such as: INIT, Preview, Capture */
	.shutter = 0x3D0,					/* current shutter */
	.gain = 0x100,						/* current gain */
	.dummy_pixel = 0,					/* current dummypixel */
	.dummy_line = 0,					/* current dummyline */
	.current_fps = 0,                   /* full size current fps : 24fps for PIP */
	.autoflicker_en = KAL_FALSE,        /* auto flicker enable */
	.test_pattern = KAL_FALSE,          /* test pattern mode or not. */
	.current_scenario_id = MSDK_SCENARIO_ID_CAMERA_PREVIEW, /* current scenario id */
	.ihdr_mode = KAL_FALSE,             /* sensor need support LE, SE with HDR feature */
	.i2c_write_id = 0x20,               /* record current sensor's i2c write id */
};
/* Sensor output window information */
static SENSOR_WINSIZE_INFO_STRUCT imgsensor_winsize_info[5] = {
	{ 4656, 3504,   8, 12,  4640, 3480, 4640,  3480, 0000, 0000, 4640, 3480, 0, 0, 4640, 3480}, /* preview */
	{ 4656, 3504,   8, 12,  4640, 3480, 4640,  3480, 0000, 0000, 4640, 3480, 0, 0, 4640, 3480}, /* capture*/
	{ 4656, 3504,   8, 12,  4640, 3480, 4640,  3480, 0000, 0000, 4640, 3480, 0, 0, 4640, 3480}, /* video-4:3*/
	{ 4656, 3504,   8, 446, 4640, 2612, 4640,  2612, 0000, 0000, 4640, 2612, 0, 0, 4640, 2612}, /* video-16:9*/
	{ 4656, 3504,   8, 12,  4640, 3480, 2320,  1740, 0000, 0000, 2320, 1740, 0, 0, 2320, 1740}, /* slim video*/
};
#if 0
static kal_uint16 read_cmos_sensor(kal_uint32 addr)
{
	kal_uint16 get_byte = 0;

	iReadReg((u16) addr, (u8 *)&get_byte, imgsensor.i2c_write_id);
	return get_byte;
}
#define write_cmos_sensor(addr, para) iWriteReg((u16) addr, (u32) para, 1,  imgsensor.i2c_write_id)
#endif
#define RWB_ID_OFFSET 0x0F73
#define EEPROM_READ_ID  0xA0
#define EEPROM_WRITE_ID   0xA1
#if 0
static kal_uint16 is_RWB_sensor(void)
{
	kal_uint16 get_byte = 0;
	char pusendcmd[2] = {(char)(RWB_ID_OFFSET >> 8), (char)(RWB_ID_OFFSET & 0xFF) };

	iReadRegI2C(pusendcmd, 2, (u8 *)&get_byte, 1, EEPROM_READ_ID);
	return get_byte;
}
#endif
static kal_uint16 read_cmos_sensor(kal_uint32 addr)
{
	kal_uint16 get_byte = 0;
	char pusendcmd[2] = {(char)(addr >> 8), (char)(addr & 0xFF) };

	iReadRegI2C(pusendcmd, 2, (u8 *)&get_byte, 2, imgsensor.i2c_write_id);
	return ((get_byte<<8)&0xff00)|((get_byte>>8)&0x00ff);
}
static void write_cmos_sensor(kal_uint16 addr, kal_uint16 para)
{
	char pusendcmd[4] = {(char)(addr >> 8), (char)(addr & 0xFF), (char)(para >> 8), (char)(para & 0xFF)};
		iWriteRegI2C(pusendcmd, 4, imgsensor.i2c_write_id);
}
static kal_uint16 read_cmos_sensor_8(kal_uint16 addr)
{
	kal_uint16 get_byte = 0;
	char pusendcmd[2] = {(char)(addr >> 8), (char)(addr & 0xFF) };

	iReadRegI2C(pusendcmd, 2, (u8 *)&get_byte, 1, imgsensor.i2c_write_id);
	return get_byte;
}
static void write_cmos_sensor_8(kal_uint16 addr, kal_uint8 para)
{
	char pusendcmd[4] = {(char)(addr >> 8), (char)(addr & 0xFF), (char)(para & 0xFF)};

	iWriteRegI2C(pusendcmd, 3, imgsensor.i2c_write_id);
}
static void set_dummy(void)
{
	LOG_INF("dummyline = %d, dummypixels = %d\n", imgsensor.dummy_line, imgsensor.dummy_pixel);
	/* return; //for test */
	write_cmos_sensor(0x0340, imgsensor.frame_length);
	write_cmos_sensor(0x0342, imgsensor.line_length);
}	/*	set_dummy  */
static void set_max_framerate(UINT16 framerate, kal_bool min_framelength_en)
{
	kal_uint32 frame_length = imgsensor.frame_length;

	LOG_INF("framerate = %d, min framelength should enable %d\n", framerate, min_framelength_en);
	frame_length = imgsensor.pclk / framerate * 10 / imgsensor.line_length;
	spin_lock(&imgsensor_drv_lock);
	if (frame_length >= imgsensor.min_frame_length)
		imgsensor.frame_length = frame_length;
	else
		imgsensor.frame_length = imgsensor.min_frame_length;
	imgsensor.dummy_line = imgsensor.frame_length - imgsensor.min_frame_length;
	if (imgsensor.frame_length > imgsensor_info.max_frame_length) {
		imgsensor.frame_length = imgsensor_info.max_frame_length;
		imgsensor.dummy_line = imgsensor.frame_length - imgsensor.min_frame_length;
	}
	if (min_framelength_en)
		imgsensor.min_frame_length = imgsensor.frame_length;
	spin_unlock(&imgsensor_drv_lock);
	set_dummy();
}	/*	set_max_framerate  */
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
	if (shutter < imgsensor_info.min_shutter)
		shutter = imgsensor_info.min_shutter;
	if (imgsensor.autoflicker_en) {
		realtime_fps = imgsensor.pclk / imgsensor.line_length * 10 / imgsensor.frame_length;
		if (realtime_fps >= 297 && realtime_fps <= 305)
			set_max_framerate(296, 0);
		else if (realtime_fps >= 147 && realtime_fps <= 150)
			set_max_framerate(146, 0);
		else {
		/* Extend frame length */
		write_cmos_sensor(0x0340, imgsensor.frame_length);
		}
	} else {
		/* Extend frame length */
		write_cmos_sensor(0x0340, imgsensor.frame_length);
	LOG_INF("(else)imgsensor.frame_length = %d\n", imgsensor.frame_length);
	}
	/* Update Shutter */
	write_cmos_sensor(0x0202, shutter);
	LOG_INF("shutter =%d, framelength =%d\n", shutter, imgsensor.frame_length);
}	/*	write_shutter  */
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
}	/*	set_shutter */
static kal_uint16 gain2reg(const kal_uint16 gain)
{
	 kal_uint16 reg_gain = 0x0;
	reg_gain = gain/2;
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
	/* gain=1024;//for test */
	/* return; //for test */
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
	write_cmos_sensor(0x0204, reg_gain);
	/* write_cmos_sensor_8(0x0204,(reg_gain>>8)); */
	/* write_cmos_sensor_8(0x0205,(reg_gain&0xff)); */
	return gain;
}	/*	set_gain  */
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
#if 0
static void night_mode(kal_bool enable)
{
/*No Need to implement this function*/
}	/*	night_mode	*/
#endif
/*************************************************************************
* FUNCTION
*	check_stremoff
*
* DESCRIPTION
*	waiting function until sensor streaming finish.
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
static void check_stremoff(void)
{
	unsigned int i = 0, framecnt = 0;

	LOG_INF(" Stream Off+\n");
	for (i = 0; i < 100; i++) {
		framecnt = read_cmos_sensor_8(0x0005);
		if (framecnt == 0xFF) {
			LOG_INF(" Stream Off OK at i=%d.\n", i);
			return;
		}
		mdelay(30);
	}
	LOG_INF(" Stream Off Fail! framecnt=%d.\n", framecnt);
	/*	return; */
}
static void sensor_init(void)
{
	LOG_INF("sensor_init() E\n");
	write_cmos_sensor(0xFCFC, 0x4000);
	write_cmos_sensor(0x6010, 0x0001);
	mdelay(3);
	write_cmos_sensor(0xFCFC, 0x4000);
	write_cmos_sensor(0x6214, 0x7971);
	write_cmos_sensor(0x6218, 0x7150);
	write_cmos_sensor(0x6028, 0x2000);
	write_cmos_sensor(0x602A, 0x3074);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x6F12, 0x0449);
	write_cmos_sensor(0x6F12, 0x0348);
	write_cmos_sensor(0x6F12, 0x0860);
	write_cmos_sensor(0x6F12, 0xCA8D);
	write_cmos_sensor(0x6F12, 0x101A);
	write_cmos_sensor(0x6F12, 0x8880);
	write_cmos_sensor(0x6F12, 0x00F0);
	write_cmos_sensor(0x6F12, 0x52B8);
	write_cmos_sensor(0x6F12, 0x2000);
	write_cmos_sensor(0x6F12, 0x31B8);
	write_cmos_sensor(0x6F12, 0x2000);
	write_cmos_sensor(0x6F12, 0x1E80);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x6F12, 0x70B5);
	write_cmos_sensor(0x6F12, 0x0646);
	write_cmos_sensor(0x6F12, 0x2C48);
	write_cmos_sensor(0x6F12, 0x0022);
	write_cmos_sensor(0x6F12, 0x0168);
	write_cmos_sensor(0x6F12, 0x0C0C);
	write_cmos_sensor(0x6F12, 0x8DB2);
	write_cmos_sensor(0x6F12, 0x2946);
	write_cmos_sensor(0x6F12, 0x2046);
	write_cmos_sensor(0x6F12, 0x00F0);
	write_cmos_sensor(0x6F12, 0x61F8);
	write_cmos_sensor(0x6F12, 0x3046);
	write_cmos_sensor(0x6F12, 0x00F0);
	write_cmos_sensor(0x6F12, 0x63F8);
	write_cmos_sensor(0x6F12, 0x2748);
	write_cmos_sensor(0x6F12, 0x284A);
	write_cmos_sensor(0x6F12, 0x0188);
	write_cmos_sensor(0x6F12, 0x1180);
	write_cmos_sensor(0x6F12, 0x911C);
	write_cmos_sensor(0x6F12, 0x4088);
	write_cmos_sensor(0x6F12, 0x0880);
	write_cmos_sensor(0x6F12, 0x2946);
	write_cmos_sensor(0x6F12, 0x2046);
	write_cmos_sensor(0x6F12, 0xBDE8);
	write_cmos_sensor(0x6F12, 0x7040);
	write_cmos_sensor(0x6F12, 0x0122);
	write_cmos_sensor(0x6F12, 0x00F0);
	write_cmos_sensor(0x6F12, 0x50B8);
	write_cmos_sensor(0x6F12, 0x2DE9);
	write_cmos_sensor(0x6F12, 0xF041);
	write_cmos_sensor(0x6F12, 0x8046);
	write_cmos_sensor(0x6F12, 0x1E48);
	write_cmos_sensor(0x6F12, 0x1446);
	write_cmos_sensor(0x6F12, 0x0F46);
	write_cmos_sensor(0x6F12, 0x4068);
	write_cmos_sensor(0x6F12, 0x0022);
	write_cmos_sensor(0x6F12, 0x85B2);
	write_cmos_sensor(0x6F12, 0x060C);
	write_cmos_sensor(0x6F12, 0x2946);
	write_cmos_sensor(0x6F12, 0x3046);
	write_cmos_sensor(0x6F12, 0x00F0);
	write_cmos_sensor(0x6F12, 0x42F8);
	write_cmos_sensor(0x6F12, 0x1B48);
	write_cmos_sensor(0x6F12, 0x408C);
	write_cmos_sensor(0x6F12, 0x70B1);
	write_cmos_sensor(0x6F12, 0x94F8);
	write_cmos_sensor(0x6F12, 0x6B00);
	write_cmos_sensor(0x6F12, 0x4021);
	write_cmos_sensor(0x6F12, 0xB1FB);
	write_cmos_sensor(0x6F12, 0xF0F0);
	write_cmos_sensor(0x6F12, 0x1849);
	write_cmos_sensor(0x6F12, 0x194A);
	write_cmos_sensor(0x6F12, 0x098B);
	write_cmos_sensor(0x6F12, 0x5288);
	write_cmos_sensor(0x6F12, 0x891A);
	write_cmos_sensor(0x6F12, 0x0901);
	write_cmos_sensor(0x6F12, 0x91FB);
	write_cmos_sensor(0x6F12, 0xF0F0);
	write_cmos_sensor(0x6F12, 0x84B2);
	write_cmos_sensor(0x6F12, 0x05E0);
	write_cmos_sensor(0x6F12, 0x2246);
	write_cmos_sensor(0x6F12, 0x3946);
	write_cmos_sensor(0x6F12, 0x4046);
	write_cmos_sensor(0x6F12, 0x00F0);
	write_cmos_sensor(0x6F12, 0x35F8);
	write_cmos_sensor(0x6F12, 0x0446);
	write_cmos_sensor(0x6F12, 0x0122);
	write_cmos_sensor(0x6F12, 0x2946);
	write_cmos_sensor(0x6F12, 0x3046);
	write_cmos_sensor(0x6F12, 0x00F0);
	write_cmos_sensor(0x6F12, 0x25F8);
	write_cmos_sensor(0x6F12, 0x2046);
	write_cmos_sensor(0x6F12, 0xBDE8);
	write_cmos_sensor(0x6F12, 0xF081);
	write_cmos_sensor(0x6F12, 0x10B5);
	write_cmos_sensor(0x6F12, 0x0022);
	write_cmos_sensor(0x6F12, 0xAFF2);
	write_cmos_sensor(0x6F12, 0x9B01);
	write_cmos_sensor(0x6F12, 0x0C48);
	write_cmos_sensor(0x6F12, 0x00F0);
	write_cmos_sensor(0x6F12, 0x2AF8);
	write_cmos_sensor(0x6F12, 0x054C);
	write_cmos_sensor(0x6F12, 0x0022);
	write_cmos_sensor(0x6F12, 0xAFF2);
	write_cmos_sensor(0x6F12, 0x6F01);
	write_cmos_sensor(0x6F12, 0x2060);
	write_cmos_sensor(0x6F12, 0x0948);
	write_cmos_sensor(0x6F12, 0x00F0);
	write_cmos_sensor(0x6F12, 0x22F8);
	write_cmos_sensor(0x6F12, 0x6060);
	write_cmos_sensor(0x6F12, 0x10BD);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x6F12, 0x2000);
	write_cmos_sensor(0x6F12, 0x31B0);
	write_cmos_sensor(0x6F12, 0x2000);
	write_cmos_sensor(0x6F12, 0x4A00);
	write_cmos_sensor(0x6F12, 0x4000);
	write_cmos_sensor(0x6F12, 0x950C);
	write_cmos_sensor(0x6F12, 0x2000);
	write_cmos_sensor(0x6F12, 0x2F10);
	write_cmos_sensor(0x6F12, 0x2000);
	write_cmos_sensor(0x6F12, 0x1B10);
	write_cmos_sensor(0x6F12, 0x2000);
	write_cmos_sensor(0x6F12, 0x2F40);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x6F12, 0x15E9);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x6F12, 0x9ECD);
	write_cmos_sensor(0x6F12, 0x40F2);
	write_cmos_sensor(0x6F12, 0xA37C);
	write_cmos_sensor(0x6F12, 0xC0F2);
	write_cmos_sensor(0x6F12, 0x000C);
	write_cmos_sensor(0x6F12, 0x6047);
	write_cmos_sensor(0x6F12, 0x41F2);
	write_cmos_sensor(0x6F12, 0xE95C);
	write_cmos_sensor(0x6F12, 0xC0F2);
	write_cmos_sensor(0x6F12, 0x000C);
	write_cmos_sensor(0x6F12, 0x6047);
	write_cmos_sensor(0x6F12, 0x49F6);
	write_cmos_sensor(0x6F12, 0xCD6C);
	write_cmos_sensor(0x6F12, 0xC0F2);
	write_cmos_sensor(0x6F12, 0x000C);
	write_cmos_sensor(0x6F12, 0x6047);
	write_cmos_sensor(0x6F12, 0x4DF6);
	write_cmos_sensor(0x6F12, 0x1B0C);
	write_cmos_sensor(0x6F12, 0xC0F2);
	write_cmos_sensor(0x6F12, 0x000C);
	write_cmos_sensor(0x6F12, 0x6047);
	write_cmos_sensor(0x6F12, 0x3108);
	write_cmos_sensor(0x6F12, 0x021C);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x6F12, 0x0005);
	write_cmos_sensor(0x602A, 0x4A00);
	write_cmos_sensor(0x6F12, 0x0088);
	write_cmos_sensor(0x6F12, 0x0D70);
	write_cmos_sensor(0x6028, 0x2000);
	write_cmos_sensor(0x602A, 0x162C);
	write_cmos_sensor(0x6F12, 0x8080);
	write_cmos_sensor(0x602A, 0x164C);
	write_cmos_sensor(0x6F12, 0x5555);
	write_cmos_sensor(0x602A, 0x164E);
	write_cmos_sensor(0x6F12, 0x5500);
	write_cmos_sensor(0x602A, 0x166C);
	write_cmos_sensor(0x6F12, 0x4040);
	write_cmos_sensor(0x602A, 0x166E);
	write_cmos_sensor(0x6F12, 0x4040);
	write_cmos_sensor(0xFCFC, 0x4000);
	write_cmos_sensor(0xF496, 0x0048);
	write_cmos_sensor(0xF470, 0x0008);
	write_cmos_sensor(0xF43A, 0x0015);
	write_cmos_sensor(0xF484, 0x0006);
	write_cmos_sensor(0xF442, 0x44C6);
	write_cmos_sensor(0xF408, 0xFFF7);
	write_cmos_sensor(0xF494, 0x0010);
	write_cmos_sensor(0xF4D4, 0x0038);
	write_cmos_sensor(0xF4D6, 0x0039);
	write_cmos_sensor(0xF4D8, 0x0034);
	write_cmos_sensor(0xF4DA, 0x0035);
	write_cmos_sensor(0xF4DC, 0x0038);
	write_cmos_sensor(0xF4DE, 0x0039);
	write_cmos_sensor(0xF47C, 0x001F);
	write_cmos_sensor(0xF62E, 0x00C5);
	write_cmos_sensor(0xF630, 0x00CD);
	write_cmos_sensor(0xF632, 0x00DD);
	write_cmos_sensor(0xF634, 0x00E5);
	write_cmos_sensor(0xF636, 0x00F5);
	write_cmos_sensor(0xF638, 0x00FD);
	write_cmos_sensor(0xF63A, 0x010D);
	write_cmos_sensor(0xF63C, 0x0115);
	write_cmos_sensor(0xF63E, 0x0125);
	write_cmos_sensor(0xF640, 0x012D);
	write_cmos_sensor(0x3070, 0x0000);
	write_cmos_sensor(0x31C0, 0x00C8);
	write_cmos_sensor(0x31A4, 0x0102);
	LOG_INF("sensor_init() End\n");
}	/*	sensor_init  */
static void preview_setting(void)
{
	LOG_INF("preview_setting() E\n");
	write_cmos_sensor(0x0100, 0x0000);
	check_stremoff();
	write_cmos_sensor(0xFCFC, 0x4000);
	write_cmos_sensor(0x6214, 0x7971);
	write_cmos_sensor(0x6218, 0x7150);
	write_cmos_sensor(0x0300, 0x0003);
	write_cmos_sensor(0x0306, 0x0069);
	write_cmos_sensor(0x030E, 0x0079);
	write_cmos_sensor(0x3004, 0x0003);
	write_cmos_sensor(0x0344, 0x0008);
	write_cmos_sensor(0x0348, 0x1227);
	write_cmos_sensor(0x0346, 0x0010);
	write_cmos_sensor(0x034A, 0x0DA7);
	write_cmos_sensor(0x034C, 0x1220);
	write_cmos_sensor(0x034E, 0x0D98);
	write_cmos_sensor(0x0342, 0x1400);
	write_cmos_sensor(0x0340, 0x0E3B);
	write_cmos_sensor(0x0202, 0x0200);
	write_cmos_sensor(0x0200, 0x0618);
	write_cmos_sensor(0x021E, 0x0300);
	write_cmos_sensor(0x021C, 0x0000);
	write_cmos_sensor(0x0900, 0x0011);
	write_cmos_sensor(0x0380, 0x0001);
	write_cmos_sensor(0x0382, 0x0001);
	write_cmos_sensor(0x0384, 0x0001);
	write_cmos_sensor(0x0386, 0x0001);
	write_cmos_sensor(0x0400, 0x0000);
	write_cmos_sensor(0x0404, 0x0010);
	write_cmos_sensor(0x3604, 0x0002);
	write_cmos_sensor(0x3606, 0x0103);
	write_cmos_sensor(0x0216, 0x0000);
	write_cmos_sensor(0x3000, 0x0001);
	write_cmos_sensor(0x1006, 0x0002);
	write_cmos_sensor(0xF440, 0x002F);
	write_cmos_sensor(0x3664, 0x0019);
	write_cmos_sensor(0x3058, 0x0000);
	write_cmos_sensor(0x317A, 0x0130);
	write_cmos_sensor(0x6028, 0x2000);
	write_cmos_sensor(0x602A, 0x162C);
	write_cmos_sensor(0x6F12, 0x8080);
	write_cmos_sensor(0x0100, 0x0100);
	LOG_INF("preview_setting() End\n");
}	/*	preview_setting  */
static void capture_setting(kal_uint16 currefps)
{
	LOG_INF("capture_setting() E! currefps:%d\n", currefps);
	write_cmos_sensor(0x0100, 0x0000);
	check_stremoff();
	write_cmos_sensor(0xFCFC, 0x4000);
	write_cmos_sensor(0x6214, 0x7971);
	write_cmos_sensor(0x6218, 0x7150);
	write_cmos_sensor(0x0300, 0x0003);
	write_cmos_sensor(0x0306, 0x0069);
	write_cmos_sensor(0x030E, 0x0079);
	write_cmos_sensor(0x3004, 0x0003);
	write_cmos_sensor(0x0344, 0x0008);
	write_cmos_sensor(0x0348, 0x1227);
	write_cmos_sensor(0x0346, 0x0010);
	write_cmos_sensor(0x034A, 0x0DA7);
	write_cmos_sensor(0x034C, 0x1220);
	write_cmos_sensor(0x034E, 0x0D98);
	write_cmos_sensor(0x0342, 0x1400);
	write_cmos_sensor(0x0340, 0x0E3B);
	write_cmos_sensor(0x0202, 0x0200);
	write_cmos_sensor(0x0200, 0x0618);
	write_cmos_sensor(0x021E, 0x0300);
	write_cmos_sensor(0x021C, 0x0000);
	write_cmos_sensor(0x0900, 0x0011);
	write_cmos_sensor(0x0380, 0x0001);
	write_cmos_sensor(0x0382, 0x0001);
	write_cmos_sensor(0x0384, 0x0001);
	write_cmos_sensor(0x0386, 0x0001);
	write_cmos_sensor(0x0400, 0x0000);
	write_cmos_sensor(0x0404, 0x0010);
	write_cmos_sensor(0x3604, 0x0002);
	write_cmos_sensor(0x3606, 0x0103);
	write_cmos_sensor(0x0216, 0x0000);
	write_cmos_sensor(0x3000, 0x0001);
	write_cmos_sensor(0x1006, 0x0002);
	write_cmos_sensor(0xF440, 0x002F);
	write_cmos_sensor(0x3664, 0x0019);
	write_cmos_sensor(0x3058, 0x0000);
	write_cmos_sensor(0x317A, 0x0130);
	write_cmos_sensor(0x6028, 0x2000);
	write_cmos_sensor(0x602A, 0x162C);
	write_cmos_sensor(0x6F12, 0x8080);
	write_cmos_sensor(0x0100, 0x0100);
}
static void normal_video_setting(kal_uint16 currefps)
{
	LOG_INF("normal_video_setting() E! currefps:%d\n", currefps);
	write_cmos_sensor(0x0100, 0x0000);
	check_stremoff();
	write_cmos_sensor(0xFCFC, 0x4000);
	write_cmos_sensor(0x6214, 0x7971);
	write_cmos_sensor(0x6218, 0x7150);
	write_cmos_sensor(0x0300, 0x0003);
	write_cmos_sensor(0x0306, 0x0069);
	write_cmos_sensor(0x030E, 0x0079);
	write_cmos_sensor(0x3004, 0x0003);
	write_cmos_sensor(0x0344, 0x0008);
	write_cmos_sensor(0x0348, 0x1227);
	write_cmos_sensor(0x0346, 0x0010);
	write_cmos_sensor(0x034A, 0x0DA7);
	write_cmos_sensor(0x034C, 0x1220);
	write_cmos_sensor(0x034E, 0x0D98);
	write_cmos_sensor(0x0342, 0x1400);
	write_cmos_sensor(0x0340, 0x0E3B);
	write_cmos_sensor(0x0202, 0x0200);
	write_cmos_sensor(0x0200, 0x0618);
	write_cmos_sensor(0x021E, 0x0300);
	write_cmos_sensor(0x021C, 0x0000);
	write_cmos_sensor(0x0900, 0x0011);
	write_cmos_sensor(0x0380, 0x0001);
	write_cmos_sensor(0x0382, 0x0001);
	write_cmos_sensor(0x0384, 0x0001);
	write_cmos_sensor(0x0386, 0x0001);
	write_cmos_sensor(0x0400, 0x0000);
	write_cmos_sensor(0x0404, 0x0010);
	write_cmos_sensor(0x3604, 0x0002);
	write_cmos_sensor(0x3606, 0x0103);
	write_cmos_sensor(0x0216, 0x0000);
	write_cmos_sensor(0x3000, 0x0001);
	write_cmos_sensor(0x1006, 0x0002);
	write_cmos_sensor(0xF440, 0x002F);
	write_cmos_sensor(0x3664, 0x0019);
	write_cmos_sensor(0x3058, 0x0000);
	write_cmos_sensor(0x317A, 0x0130);
	write_cmos_sensor(0x6028, 0x2000);
	write_cmos_sensor(0x602A, 0x162C);
	write_cmos_sensor(0x6F12, 0x8080);
	write_cmos_sensor(0x0100, 0x0100);
}
static void hs_video_setting(void)
{
	LOG_INF("video setting for 16:9 E\n");
	write_cmos_sensor(0x0100, 0x0000);
	check_stremoff();
	write_cmos_sensor(0xFCFC, 0x4000);
	write_cmos_sensor(0x6214, 0x7971);
	write_cmos_sensor(0x6218, 0x7150);
	write_cmos_sensor(0x0300, 0x0003);
	write_cmos_sensor(0x0306, 0x0069);
	write_cmos_sensor(0x030E, 0x0079);
	write_cmos_sensor(0x3004, 0x0003);
	write_cmos_sensor(0x0344, 0x0008);
	write_cmos_sensor(0x0348, 0x1227);
	write_cmos_sensor(0x0346, 0x01C0);
	write_cmos_sensor(0x034A, 0x0BF1);
	write_cmos_sensor(0x034C, 0x1220);
	write_cmos_sensor(0x034E, 0x0A34);/*0x0A32*/
	write_cmos_sensor(0x0342, 0x1400);
	write_cmos_sensor(0x0340, 0x0E3B);
	write_cmos_sensor(0x0202, 0x0200);
	write_cmos_sensor(0x0200, 0x0618);
	write_cmos_sensor(0x021E, 0x0300);
	write_cmos_sensor(0x021C, 0x0000);
	write_cmos_sensor(0x0900, 0x0011);
	write_cmos_sensor(0x0380, 0x0001);
	write_cmos_sensor(0x0382, 0x0001);
	write_cmos_sensor(0x0384, 0x0001);
	write_cmos_sensor(0x0386, 0x0001);
	write_cmos_sensor(0x0400, 0x0000);
	write_cmos_sensor(0x0404, 0x0010);
	write_cmos_sensor(0x3604, 0x0002);
	write_cmos_sensor(0x3606, 0x0103);
	write_cmos_sensor(0x0216, 0x0000);
	write_cmos_sensor(0x3000, 0x0001);
	write_cmos_sensor(0x1006, 0x0002);
	write_cmos_sensor(0xF440, 0x002F);
	write_cmos_sensor(0x3664, 0x0019);
	write_cmos_sensor(0x3058, 0x0000);
	write_cmos_sensor(0x317A, 0x0130);
	write_cmos_sensor(0x6028, 0x2000);
	write_cmos_sensor(0x602A, 0x162C);
	write_cmos_sensor(0x6F12, 0x8080);
	write_cmos_sensor(0x0100, 0x0100);
	LOG_INF("video setting for 16:9 End\n");
}
static void slim_video_setting(void)
{
	LOG_INF("slim_video_setting() E\n");
	write_cmos_sensor(0x0100, 0x0000);
	check_stremoff();
	write_cmos_sensor(0xFCFC, 0x4000);
	write_cmos_sensor(0x6214, 0x7971);
	write_cmos_sensor(0x6218, 0x7150);
	write_cmos_sensor(0x0300, 0x0003);
	write_cmos_sensor(0x0306, 0x0069);
	write_cmos_sensor(0x030E, 0x0079);
	write_cmos_sensor(0x3004, 0x0003);
	write_cmos_sensor(0x0344, 0x0008);
	write_cmos_sensor(0x0348, 0x1227);
	write_cmos_sensor(0x0346, 0x0010);
	write_cmos_sensor(0x034A, 0x0DA7);
	write_cmos_sensor(0x034C, 0x0910);
	write_cmos_sensor(0x034E, 0x06CC);
	write_cmos_sensor(0x0342, 0x1400);
	write_cmos_sensor(0x0340, 0x0E3B);
	write_cmos_sensor(0x0202, 0x0200);
	write_cmos_sensor(0x0200, 0x0618);
	write_cmos_sensor(0x021E, 0x0300);
	write_cmos_sensor(0x021C, 0x0000);
	write_cmos_sensor(0x0900, 0x0122);
	write_cmos_sensor(0x0380, 0x0001);
	write_cmos_sensor(0x0382, 0x0003);
	write_cmos_sensor(0x0384, 0x0001);
	write_cmos_sensor(0x0386, 0x0003);
	write_cmos_sensor(0x0400, 0x0000);
	write_cmos_sensor(0x0404, 0x0010);
	write_cmos_sensor(0x3604, 0x0002);
	write_cmos_sensor(0x3606, 0x0103);
	write_cmos_sensor(0x0216, 0x0000);
	write_cmos_sensor(0x3000, 0x0001);
	write_cmos_sensor(0x1006, 0x0002);
	write_cmos_sensor(0xF440, 0x002F);
	write_cmos_sensor(0x3664, 0x0019);
	write_cmos_sensor(0x3058, 0x0000);
	write_cmos_sensor(0x317A, 0x00A0);
	write_cmos_sensor(0x6028, 0x2000);
	write_cmos_sensor(0x602A, 0x162C);
	write_cmos_sensor(0x6F12, 0x8080);
	write_cmos_sensor(0x0100, 0x0100);
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
	/* sensor have two i2c address 0x6c 0x6d & 0x21 0x20, we should detect the module used i2c address */
	while (imgsensor_info.i2c_addr_table[i] != 0xff) {
		spin_lock(&imgsensor_drv_lock);
		imgsensor.i2c_write_id = imgsensor_info.i2c_addr_table[i];
		spin_unlock(&imgsensor_drv_lock);
		do {
			*sensor_id = ((read_cmos_sensor_8(0x0000) << 8) | read_cmos_sensor_8(0x0001));
			LOG_INF("read_0x0000=0x%x, 0x0001=0x%x,0x0000_0001=0x%x\n", read_cmos_sensor_8(0x0000),
					read_cmos_sensor_8(0x0001), read_cmos_sensor(0x0000));
			if (*sensor_id == imgsensor_info.sensor_id) {
				LOG_INF("i2c write id: 0x%x, sensor id: 0x%x\n", imgsensor.i2c_write_id, *sensor_id);
#if 0
				if (is_RWB_sensor() == 0x1) {
					imgsensor_info.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_RWB_Wr;
					LOG_INF("RWB sensor of S5k3p8\n");
				}
#endif
				return ERROR_NONE;
			}
			LOG_INF("Read sensor id fail, id: 0x%x\n", imgsensor.i2c_write_id);
			retry--;
		} while (retry > 0);
		i++;
		retry = 2;
	}
	if (*sensor_id != imgsensor_info.sensor_id) {
		/* if Sensor ID is not correct, Must set *sensor_id to 0xFFFFFFFF */
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
	LOG_INF("PLATFORM:MT6797,MIPI 4LANE\n");
	LOG_INF("preview 2320*1744@30fps; video 4640*3488@30fps;capture 4640*3488@30fps,1464Mbps/lane\n");
	/* sensor have two i2c address 0x6c 0x6d & 0x21 0x20, we should detect the module used i2c address */
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
			LOG_INF("Read sensor id fail, id: 0x%x\n", imgsensor.i2c_write_id);
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
	imgsensor.shutter = 0x3D0;
	imgsensor.gain = 0x100;
	imgsensor.pclk = imgsensor_info.pre.pclk;
	imgsensor.frame_length = imgsensor_info.pre.framelength;
	imgsensor.line_length = imgsensor_info.pre.linelength;
	imgsensor.min_frame_length = imgsensor_info.pre.framelength;
	imgsensor.dummy_pixel = 0;
	imgsensor.dummy_line = 0;
	imgsensor.ihdr_mode = 0;
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
	LOG_INF("preview E\n");
	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_PREVIEW;
	imgsensor.pclk = imgsensor_info.pre.pclk;
	imgsensor.line_length = imgsensor_info.pre.linelength;
	imgsensor.frame_length = imgsensor_info.pre.framelength;
	imgsensor.min_frame_length = imgsensor_info.pre.framelength;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	preview_setting();
	set_mirror_flip(imgsensor.mirror);
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
	LOG_INF("capture E\n");
	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_CAPTURE;
	if (imgsensor.current_fps != imgsensor_info.cap.max_framerate)
		LOG_INF("Warning: current_fps %d fps is not support, so use cap's setting: %d fps!\n",
					imgsensor.current_fps, imgsensor_info.cap.max_framerate/10);
	imgsensor.pclk = imgsensor_info.cap.pclk;
	imgsensor.line_length = imgsensor_info.cap.linelength;
	imgsensor.frame_length = imgsensor_info.cap.framelength;
	imgsensor.min_frame_length = imgsensor_info.cap.framelength;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	capture_setting(imgsensor.current_fps);
	set_mirror_flip(imgsensor.mirror);
	return ERROR_NONE;
}	/* capture() */
static kal_uint32 normal_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
					  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("normal_video E\n");
	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_VIDEO;
	imgsensor.pclk = imgsensor_info.normal_video.pclk;
	imgsensor.line_length = imgsensor_info.normal_video.linelength;
	imgsensor.frame_length = imgsensor_info.normal_video.framelength;
	imgsensor.min_frame_length = imgsensor_info.normal_video.framelength;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	normal_video_setting(imgsensor.current_fps);
	set_mirror_flip(imgsensor.mirror);
	return ERROR_NONE;
}	/*	normal_video   */
static kal_uint32 hs_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
					  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("hs_video E\n");
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
	set_mirror_flip(imgsensor.mirror);
	return ERROR_NONE;
}	/*	hs_video   */
static kal_uint32 slim_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
					  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("slim_video E\n");
	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_SLIM_VIDEO;
	imgsensor.pclk = imgsensor_info.slim_video.pclk;
	/* imgsensor.video_mode = KAL_TRUE; */
	imgsensor.line_length = imgsensor_info.slim_video.linelength;
	imgsensor.frame_length = imgsensor_info.slim_video.framelength;
	imgsensor.min_frame_length = imgsensor_info.slim_video.framelength;
	imgsensor.dummy_line = 0;
	imgsensor.dummy_pixel = 0;
	/* imgsensor.current_fps = 300; */
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	slim_video_setting();
	set_mirror_flip(imgsensor.mirror);
	return ERROR_NONE;
}	/*	slim_video	 */
static kal_uint32 get_resolution(MSDK_SENSOR_RESOLUTION_INFO_STRUCT *sensor_resolution)
{
	LOG_INF("get_resolution E\n");
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
	return ERROR_NONE;
}	/*	get_resolution	*/
static kal_uint32 get_info(MSDK_SCENARIO_ID_ENUM scenario_id,
					  MSDK_SENSOR_INFO_STRUCT *sensor_info,
					  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("get_info -> scenario_id = %d\n", scenario_id);
	sensor_info->SensorClockPolarity = SENSOR_CLOCK_POLARITY_LOW;
	sensor_info->SensorClockFallingPolarity = SENSOR_CLOCK_POLARITY_LOW; /* not use */
	sensor_info->SensorHsyncPolarity = SENSOR_CLOCK_POLARITY_LOW; /* inverse with datasheet */
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
	sensor_info->SensorMasterClockSwitch = 0; /* not use */
	sensor_info->SensorDrivingCurrent = imgsensor_info.isp_driving_current;
	sensor_info->AEShutDelayFrame = imgsensor_info.ae_shut_delay_frame;
	sensor_info->AESensorGainDelayFrame = imgsensor_info.ae_sensor_gain_delay_frame;
	sensor_info->AEISPGainDelayFrame = imgsensor_info.ae_ispGain_delay_frame;
	sensor_info->IHDR_Support = imgsensor_info.ihdr_support;
	sensor_info->IHDR_LE_FirstLine = imgsensor_info.ihdr_le_firstline;
	sensor_info->SensorModeNum = imgsensor_info.sensor_mode_num;
	sensor_info->PDAF_Support = 0; /* change pdaf support mode to pdaf VC mode */
	sensor_info->SensorMIPILaneNumber = imgsensor_info.mipi_lane_num;
	sensor_info->SensorClockFreq = imgsensor_info.mclk;
	sensor_info->SensorClockDividCount = 3; /* not use */
	sensor_info->SensorClockRisingCount = 0;
	sensor_info->SensorClockFallingCount = 2; /* not use */
	sensor_info->SensorPixelClockCount = 3; /* not use */
	sensor_info->SensorDataLatchCount = 2; /* not use */
	sensor_info->MIPIDataLowPwr2HighSpeedTermDelayCount = 0;
	sensor_info->MIPICLKLowPwr2HighSpeedTermDelayCount = 0;
	sensor_info->SensorWidthSampling = 0;  /* 0 is default 1x */
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
	default:
		sensor_info->SensorGrabStartX = imgsensor_info.pre.startx;
		sensor_info->SensorGrabStartY = imgsensor_info.pre.starty;
		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount = imgsensor_info.pre.mipi_data_lp2hs_settle_dc;
		break;
		}
	return ERROR_NONE;
}	/*	get_info  */
static kal_uint32 control(MSDK_SCENARIO_ID_ENUM scenario_id, MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
					  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
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
	default:
		LOG_INF("Error ScenarioId setting");
		preview(image_window, sensor_config_data);
		return ERROR_INVALID_SCENARIO_ID;
	}
	return ERROR_NONE;
}	/* control() */
static kal_uint32 set_video_mode(UINT16 framerate)
{
	/* //LOG_INF("framerate = %d\n ", framerate); */
	/* SetVideoMode Function should fix framerate */
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
		imgsensor.dummy_line = (frame_length > imgsensor_info.pre.framelength) ?
				(frame_length - imgsensor_info.pre.framelength) : 0;
		imgsensor.frame_length = imgsensor_info.pre.framelength + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
		break;
	case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
		if (framerate == 0)
			return ERROR_NONE;
		frame_length = imgsensor_info.normal_video.pclk / framerate * 10
						/ imgsensor_info.normal_video.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line = (frame_length > imgsensor_info.normal_video.framelength) ?
						(frame_length - imgsensor_info.normal_video.framelength) : 0;
		imgsensor.frame_length = imgsensor_info.normal_video.framelength + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
		break;
	case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		frame_length = imgsensor_info.cap.pclk / framerate * 10 / imgsensor_info.cap.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line = (frame_length > imgsensor_info.cap.framelength) ?
						(frame_length - imgsensor_info.cap.framelength) : 0;
		imgsensor.frame_length = imgsensor_info.cap.framelength + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
		break;
	case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
		frame_length = imgsensor_info.hs_video.pclk / framerate * 10 / imgsensor_info.hs_video.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line = (frame_length > imgsensor_info.hs_video.framelength) ?
					(frame_length - imgsensor_info.hs_video.framelength) : 0;
		imgsensor.frame_length = imgsensor_info.hs_video.framelength + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
		break;
	case MSDK_SCENARIO_ID_SLIM_VIDEO:
		frame_length = imgsensor_info.slim_video.pclk / framerate * 10 / imgsensor_info.slim_video.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line = (frame_length > imgsensor_info.slim_video.framelength) ?
					(frame_length - imgsensor_info.slim_video.framelength) : 0;
		imgsensor.frame_length = imgsensor_info.slim_video.framelength + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
		break;
	default:  /* coding with  preview scenario by default */
		frame_length = imgsensor_info.pre.pclk / framerate * 10 / imgsensor_info.pre.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line = (frame_length > imgsensor_info.pre.framelength) ?
					(frame_length - imgsensor_info.pre.framelength) : 0;
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
	default:
		break;
	}
	return ERROR_NONE;
}
static kal_uint32 set_test_pattern_mode(kal_bool enable)
{
	LOG_INF("enable: %d\n", enable);
	if (enable)
		write_cmos_sensor(0x0600, 0x0002);
		/* 0 : Normal, 1 : Solid Color, 2 : Color Bar, 3 : Shade Color Bar, 4 : PN9 */
	else
		write_cmos_sensor(0x0600, 0x0000);
	spin_lock(&imgsensor_drv_lock);
	imgsensor.test_pattern = enable;
	spin_unlock(&imgsensor_drv_lock);
	return ERROR_NONE;
}
static kal_uint32 feature_control(MSDK_SENSOR_FEATURE_ENUM feature_id,
							 UINT8 *feature_para, UINT32 *feature_para_len)
{
	UINT16 *feature_return_para_16 = (UINT16 *) feature_para;
	UINT16 *feature_data_16 = (UINT16 *) feature_para;
	UINT32 *feature_return_para_32 = (UINT32 *) feature_para;
	UINT32 *feature_data_32 = (UINT32 *) feature_para;
	unsigned long long *feature_data = (unsigned long long *) feature_para;
	SENSOR_WINSIZE_INFO_STRUCT *wininfo;
	MSDK_SENSOR_REG_INFO_STRUCT *sensor_reg_data = (MSDK_SENSOR_REG_INFO_STRUCT *) feature_para;

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
		set_shutter(*feature_data);
		break;
	case SENSOR_FEATURE_SET_NIGHTMODE:
		/* night_mode((BOOL) *feature_data); no need to implement this mode */
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
		set_auto_flicker_mode((BOOL)*feature_data_16, *(feature_data_16+1));
		break;
	case SENSOR_FEATURE_SET_MAX_FRAME_RATE_BY_SCENARIO:
		set_max_framerate_by_scenario((MSDK_SCENARIO_ID_ENUM)*feature_data, *(feature_data+1));
		break;
	case SENSOR_FEATURE_GET_DEFAULT_FRAME_RATE_BY_SCENARIO:
		get_default_framerate_by_scenario((MSDK_SCENARIO_ID_ENUM)*(feature_data),
					(MUINT32 *)(uintptr_t)(*(feature_data+1)));
		break;
	case SENSOR_FEATURE_SET_TEST_PATTERN:
		set_test_pattern_mode((BOOL)*feature_data);
		break;
	case SENSOR_FEATURE_GET_TEST_PATTERN_CHECKSUM_VALUE: /* for factory mode auto testing */
		*feature_return_para_32 = imgsensor_info.checksum_value;
		*feature_para_len = 4;
		break;
	case SENSOR_FEATURE_SET_FRAMERATE:
		LOG_INF("current fps :%d\n", (UINT32)*feature_data);
		spin_lock(&imgsensor_drv_lock);
	     imgsensor.current_fps = *feature_data;
		spin_unlock(&imgsensor_drv_lock);
		break;
	case SENSOR_FEATURE_SET_HDR:
		LOG_INF("ihdr enable :%d\n", (BOOL)*feature_data);
		spin_lock(&imgsensor_drv_lock);
		imgsensor.ihdr_mode = *feature_data;
		spin_unlock(&imgsensor_drv_lock);
		break;
	case SENSOR_FEATURE_GET_CROP_INFO:
		LOG_INF("SENSOR_FEATURE_GET_CROP_INFO scenarioId:%d\n", (UINT32)*feature_data);
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
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		default:
			memcpy((void *)wininfo, (void *)&imgsensor_winsize_info[0], sizeof(SENSOR_WINSIZE_INFO_STRUCT));
			break;
		}
		break;
	case SENSOR_FEATURE_SET_IHDR_SHUTTER_GAIN:
		LOG_INF("SENSOR_SET_SENSOR_IHDR LE=%d, SE=%d, Gain=%d\n",
					(UINT16)*feature_data, (UINT16)*(feature_data+1), (UINT16)*(feature_data+2));
		/* ihdr_write_shutter_gain((UINT16)*feature_data,(UINT16)*(feature_data+1), */
		/*							(UINT16)*(feature_data+2)); */
		break;
	case SENSOR_FEATURE_SET_AWB_GAIN:
		break;
	case SENSOR_FEATURE_SET_HDR_SHUTTER:
		LOG_INF("SENSOR_FEATURE_SET_HDR_SHUTTER LE=%d, SE=%d\n",
						(UINT16)*feature_data, (UINT16)*(feature_data+1));
		/* ihdr_write_shutter((UINT16)*feature_data,(UINT16)*(feature_data+1)); */
		break;
	case SENSOR_FEATURE_GET_TWO_PIXEL_SETTING:
		switch (*feature_data) {
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
			*(MUINT32 *) (uintptr_t) (*(feature_data + 1)) = 1; /*force to set 2pixel mode*/
			LOG_INF("slim video: two pixel mode enable\n");
			break;
		default:
			*(MUINT32 *) (uintptr_t) (*(feature_data + 1)) = 0;
			break;
		}
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
UINT32 S5K3P8SN_MIPI_RAW_SensorInit(PSENSOR_FUNCTION_STRUCT *pfFunc)
{
	/* To Do : Check Sensor status here */
	if (pfFunc != NULL)
		*pfFunc =  &sensor_func;
	return ERROR_NONE;
}

