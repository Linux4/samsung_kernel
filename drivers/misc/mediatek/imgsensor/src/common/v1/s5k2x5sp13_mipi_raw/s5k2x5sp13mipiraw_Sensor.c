/*
 * Copyright (C) 2020 MediaTek Inc.
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
 *	 s5k2x5sp13mipiraw_Sensor.c
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
#define PFX "S5K2X5SP13"
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

#include "s5k2x5sp13mipiraw_Sensor.h"

#define MULTI_WRITE 1
#if MULTI_WRITE
static const int I2C_BUFFER_LEN = 1020;
#else
static const int I2C_BUFFER_LEN = 4;
#endif

/**************************** Modify end *****************************/

#define LOG_DEBUG 1
#if LOG_DEBUG
#define LOG_INF(format, args...) pr_debug(PFX "[%s] " format, __func__, ##args)
#endif

static DEFINE_SPINLOCK(imgsensor_drv_lock);
//Sensor ID Value: 0x08D2//record sensor id defined in Kd_imgsensor.h
static struct imgsensor_info_struct imgsensor_info = {
	.sensor_id = S5K2X5SP13_SENSOR_ID,
	.checksum_value = 0xc98e6b72,
	.pre = {
		.pclk = 959000000,
		.linelength  = 9119,
		.framelength = 3504,
		.startx= 0,
		.starty = 0,
		.grabwindow_width  = 2880,
		.grabwindow_height = 2156,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 300,
		.mipi_pixel_rate = 837200000,
	},
	.cap = {
		.pclk = 959000000,
		.linelength  = 9119,
		.framelength = 3504,
		.startx= 0,
		.starty = 0,
		.grabwindow_width  = 2880,
		.grabwindow_height = 2156,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 300,
		.mipi_pixel_rate = 837200000,
	},
	.normal_video = {
		.pclk = 959000000,
		.linelength  = 12141,
		.framelength = 2632,
		.startx= 0,
		.starty = 0,
		.grabwindow_width  = 2880,
		.grabwindow_height = 1620,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 300,
		.mipi_pixel_rate = 837200000,
	},
	.hs_video = {
		.pclk = 959000000,
		.linelength  = 3760,
		.framelength = 2124,
		.startx= 0,
		.starty = 0,
		.grabwindow_width  = 2880,
		.grabwindow_height = 1620,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 1200,
		.mipi_pixel_rate = 837200000,
	},
	.slim_video = {
		.pclk = 959000000,
		.linelength  = 7128,
		.framelength = 4484,
		.startx= 0,
		.starty = 0,
		.grabwindow_width  = 5760,
		.grabwindow_height = 4312,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 300,
		.mipi_pixel_rate = 837200000,
	},
	.custom1 = {
		.pclk = 959000000,
		.linelength  = 3968,
		.framelength = 2414,
		.startx= 0,
		.starty = 0,
		.grabwindow_width  = 2880,
		.grabwindow_height = 2156,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 1000,
		.mipi_pixel_rate = 837200000,
	},
	.custom2 = {
		.pclk = 959000000,
		.linelength  = 14046,
		.framelength = 2275,
		.startx = 0,
		.starty = 0,
		.grabwindow_width  = 2352,
		.grabwindow_height = 1764,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 300,
		.mipi_pixel_rate = 837200000,
	},
	.custom3 = {
		.pclk = 959000000,
		.linelength  = 14046,
		.framelength = 2275,
		.startx = 0,
		.starty = 0,
		.grabwindow_width  = 2352,
		.grabwindow_height = 1764,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 300,
		.mipi_pixel_rate = 837200000,
	},
	.custom4 = {
		.pclk = 959000000,
		.linelength  = 14855,
		.framelength = 2151,
		.startx = 0,
		.starty = 0,
		.grabwindow_width  = 2352,
		.grabwindow_height = 1324,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 300,
		.mipi_pixel_rate = 837200000,
	},
	.custom5 = {
		.pclk = 959000000,
		.linelength  = 7381,
		.framelength = 4330,
		.startx = 0,
		.starty = 0,
		.grabwindow_width  = 4688,
		.grabwindow_height = 3516,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 300,
		.mipi_pixel_rate = 837200000,
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
	.slim_video_delay_frame = 3,

#ifdef CONFIG_IMPROVE_CAMERA_PERFORMANCE
	.custom1_delay_frame = 1,
	.custom2_delay_frame = 1,
	.custom3_delay_frame = 1,
	.custom4_delay_frame = 1,
	.custom5_delay_frame = 1,
#else
	.custom1_delay_frame = 3,
	.custom2_delay_frame = 3,
	.custom3_delay_frame = 3,
	.custom4_delay_frame = 3,
	.custom5_delay_frame = 3,
#endif

	.isp_driving_current = ISP_DRIVING_8MA,
	.sensor_interface_type = SENSOR_INTERFACE_TYPE_MIPI,
	.mipi_sensor_type = MIPI_OPHY_NCSI2,
	.mipi_settle_delay_mode = 1,
	.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_4CELL_HW_BAYER_Gr,
	.mclk = 26,
	.mipi_lane_num = SENSOR_MIPI_4_LANE,
	.i2c_speed = 1000,

	/* record sensor support all write id addr,
	 * only supprt 4 must end with 0xff
	 */
	.i2c_addr_table = {0x20, 0xff},//see moudle spec
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
	.current_fps = 0,	/* full size current fps : 24fps for PIP,
				 * 30fps for Normal or ZSD */

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
	.i2c_write_id = 0x20,
};

static struct SENSOR_WINSIZE_INFO_STRUCT imgsensor_winsize_info[10] = {
	{ 5776, 4336,    8,    8, 5760, 4320, 2880, 2160,
	     0,    2, 2880, 2156,    0,    0, 2880, 2156 }, /* Preview */
	{ 5776, 4336,    8,    8, 5760, 4320, 2880, 2160,
	     0,    2, 2880, 2156,    0,    0, 2880, 2156 }, /* capture */
	{ 5776, 4336,    8,  544, 5760, 3248, 2880, 1624,
	     0,    2, 2880, 1620,    0,    0, 2880, 1620 }, /* video */
	{ 5776, 4336,    8,  544, 5760, 3248, 2880, 1624,
	     0,    2, 2880, 1620,    0,    0, 2880, 1620 }, /* hs video */
	{ 5776, 4336,    4,    8, 5768, 4320, 5768, 4320,
	     4,    4, 5760, 4312,    0,    0, 5760, 4312 }, /* slim video */
	{ 5776, 4336,    8,    8, 5760, 4320, 2880, 2160,
	     0,    2, 2880, 2156,    0,    0, 2880, 2156 }, /* custom1 */
	{ 5776, 4336,  532,  400, 4712, 3536, 2356, 1768,
	     2,    2, 2352, 1764,    0,    0, 2352, 1764 }, /* custom2 */
	{ 5776, 4336,  532,  400, 4712, 3536, 2356, 1768,
	     2,    2, 2352, 1764,    0,    0, 2352, 1764 }, /* custom3 */
	{ 5776, 4336,  532,  840, 4712, 2656, 2356, 1328,
	     2,    2, 2352, 1324,    0,    0, 2352, 1324 }, /* custom4 */
	{ 5776, 4336,  540,  406, 4696, 3524, 4696, 3524,
	     4,    4, 4688, 3516,    0,    0, 4688, 3516 }, /* custom5 */
};

static void write_cmos_sensor(kal_uint16 addr, kal_uint16 para)
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

static void check_streamoff(void)
{
	unsigned int i = 0;
	int timeout = (10000 / imgsensor.current_fps) + 1;

	mdelay(3);
	for (i = 0; i < timeout; i++) {
		if (read_cmos_sensor_8(0x0005) != 0xFF)
			mdelay(1);
		else
			break;
	}
	pr_debug("%s exit!\n", __func__);
}

static kal_uint32 streaming_control(kal_bool enable)
{
	pr_debug("streaming_enable(0=Sw Standby,1=streaming): %d\n", enable);

	if (enable) {
	LOG_INF("streaming_control() enable = %d\n", enable);
		write_cmos_sensor(0x0100, 0x0100);
	} else {
	LOG_INF("streaming_control() streamoff enable = %d\n", enable);
		write_cmos_sensor(0x0100, 0x0000);
		check_streamoff();
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
		else {

		write_cmos_sensor(0x0340, imgsensor.frame_length & 0xFFFF);
		}
	} else {

		write_cmos_sensor(0x0340, imgsensor.frame_length & 0xFFFF);
	}

	write_cmos_sensor(0x0202, shutter & 0xFFFF);
	pr_debug("Exit! shutter =%d, framelength =%d\n", shutter, imgsensor.frame_length);
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
		pr_debug("Error gain setting");
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
static kal_uint16 addr_data_pair_init_2x5sp13[] = {
	0x6214, 0xE9F0,
	0x6218, 0xE150,
	0x0136, 0x1A00,
	0xFCFC, 0x2000,
	0x117C, 0x0200,
	0x119C, 0x0001,
	0x11A6, 0x28C0,
	0x125A, 0x0700,
	0x1318, 0x0303,
	0x136C, 0x0046,
	0x137C, 0x003A,
	0x13D0, 0x003F,
	0x1458, 0x0316,
	0x14B0, 0x00CC,
	0x15CC, 0x0000,
	0x17F4, 0x8080,
	0x17F8, 0x8091,
	0x17FC, 0x8093,
	0x1800, 0x80A4,
	0x1804, 0x80A6,
	0x1808, 0x80B7,
	0x180C, 0x82F8,
	0x1810, 0x8309,
	0x1814, 0x830B,
	0x1818, 0x831C,
	0x181C, 0x831E,
	0x1820, 0x832F,
	0x1A0E, 0x090F,
	0x1A10, 0x0103,
	0x1A18, 0x0000,
	0x1A1A, 0x0280,
	0x1A1C, 0x4408,
	0x1A20, 0x0096,
	0x1A22, 0x1FE9,
	0x1D00, 0x00AE,
	0x1E6E, 0x0000,
	0x204A, 0x0000,
	0x2312, 0x0201,
	0x23B0, 0x0201,
	0x23B4, 0x0102,
	0x2508, 0x0002,
	0x38DA, 0x0001,
	0x4FB0, 0x0100,
	0xFCFC, 0x4000,
	0x0110, 0x1002,
	0x0114, 0x0300,
	0x021E, 0x0000,
	0x0300, 0x0003,
	0x0302, 0x0001,
	0x0304, 0x0003,
	0x0306, 0x00A6,
	0x0308, 0x0008,
	0x030A, 0x0001,
	0x030C, 0x0001,
	0x030E, 0x0004,
	0x0310, 0x0142,
	0x0312, 0x0000,
	0x0402, 0x1010,
	0x0404, 0x1000,
	0x080A, 0x0100,
	0x080E, 0x2700,
	0x0B00, 0x0100,
	0x0B06, 0x0101,
	0x0D00, 0x0000,
	0x0D02, 0x0001,
	0x0FE8, 0x28C0,
	0x0FEA, 0x2240,
	0xF474, 0x0000,
};

static void sensor_init(void)
{
	pr_debug("%s", __func__);

	write_cmos_sensor(0xFCFC, 0x4000);
	write_cmos_sensor(0x6018, 0x0001);
	write_cmos_sensor(0x6000, 0x0005);
	write_cmos_sensor(0x7002, 0x0008);
	write_cmos_sensor(0x70CA, 0x4CA6);
	write_cmos_sensor(0x7004, 0x1964);
	write_cmos_sensor(0x6014, 0x0001);

	mdelay(10);

	table_write_cmos_sensor(addr_data_pair_init_2x5sp13,
		sizeof(addr_data_pair_init_2x5sp13) / sizeof(kal_uint16));
}

static kal_uint16 addr_data_pair_pre_2x5sp13[] = {
	0xFCFC, 0x2000,
	0x10E0, 0x2ED7,
	0x10E2, 0x2DB4,
	0x11A0, 0x0000,
	0x11C0, 0x0000,
	0x11C2, 0x0200,
	0x11C6, 0x0200,
	0x11CA, 0x0000,
	0x11CC, 0x0000,
	0x11CE, 0x0000,
	0x11D0, 0x0000,
	0x11D2, 0x0000,
	0x11D4, 0x0000,
	0x123C, 0x1212,
	0x129E, 0x0100,
	0x2CF0, 0x0300,
	0x2E9C, 0x0000,
	0x2EB6, 0x0101,
	0x3746, 0x0037,
	0x374A, 0x0001,
	0x3752, 0x0005,
	0x3756, 0x0000,
	0x3762, 0x0000,
	0x376E, 0x0005,
	0x39D6, 0x0064,
	0x39DA, 0x0032,
	0x40E8, 0x0909,
	0x40EA, 0x0909,
	0x40EC, 0x0909,
	0x410C, 0x0404,
	0x410E, 0x0404,
	0x4110, 0x0404,
	0x4130, 0x3131,
	0x4132, 0x3131,
	0x4134, 0x3131,
	0x465A, 0x0812,
	0x465C, 0x1212,
	0x465E, 0x1212,
	0x4660, 0x1200,
	0x46E2, 0x3535,
	0x46E4, 0x3535,
	0x46E6, 0x3535,
	0x46EA, 0x2035,
	0x46EC, 0x3535,
	0x46EE, 0x3535,
	0x46F0, 0x3501,
	0x46F4, 0xC8C8,
	0x46F6, 0xC8C8,
	0x46F8, 0xC8C8,
	0x46FC, 0x20C8,
	0x46FE, 0xC8C8,
	0x4700, 0xC8C8,
	0x4706, 0x8585,
	0x4708, 0x8585,
	0x470A, 0x8585,
	0x470E, 0x2085,
	0x4710, 0x8585,
	0x4712, 0x8585,
	0x4714, 0x8501,
	0x4718, 0x8585,
	0x471A, 0x8585,
	0x471C, 0x8585,
	0x4720, 0x2032,
	0x4722, 0x3232,
	0x4724, 0x3232,
	0x4726, 0x3201,
	0x472A, 0x3232,
	0x472C, 0x3232,
	0x472E, 0x3232,
	0x4732, 0x2021,
	0x4734, 0x2121,
	0x4736, 0x2121,
	0x4738, 0x2101,
	0x473C, 0x2121,
	0x473E, 0x2121,
	0x4740, 0x2121,
	0x4744, 0x2021,
	0x4746, 0x2121,
	0x4748, 0x2121,
	0x474A, 0x21FF,
	0xFCFC, 0x4000,
	0x0340, 0x0DB0,
	0x0342, 0x239F,
	0x0344, 0x0008,
	0x0346, 0x0008,
	0x0348, 0x1687,
	0x034A, 0x10E7,
	0x034C, 0x0B40,
	0x034E, 0x086C,
	0x0350, 0x0000,
	0x0352, 0x0002,
	0x0380, 0x0001,
	0x0382, 0x0001,
	0x0384, 0x0002,
	0x0386, 0x0002,
	0x0400, 0x2010,
	0x0900, 0x0112,
	0x0902, 0x0001,
	0x0B08, 0x0000,
	0xF466, 0x07FB,
	0xF468, 0x2000,
	0x6214, 0xE9F0,
	0x6218, 0xE9F0,
};


static void preview_setting(void)
{
	pr_debug("%s", __func__);

	table_write_cmos_sensor(addr_data_pair_pre_2x5sp13,
			sizeof(addr_data_pair_pre_2x5sp13) / sizeof(kal_uint16));
}

static kal_uint16 addr_data_pair_cap_2x5sp13[] = {
	0xFCFC, 0x2000,
	0x10E0, 0x2ED7,
	0x10E2, 0x2DB4,
	0x11A0, 0x0000,
	0x11C0, 0x0000,
	0x11C2, 0x0200,
	0x11C6, 0x0200,
	0x11CA, 0x0000,
	0x11CC, 0x0000,
	0x11CE, 0x0000,
	0x11D0, 0x0000,
	0x11D2, 0x0000,
	0x11D4, 0x0000,
	0x123C, 0x1212,
	0x129E, 0x0100,
	0x2CF0, 0x0300,
	0x2E9C, 0x0000,
	0x2EB6, 0x0101,
	0x3746, 0x0037,
	0x374A, 0x0001,
	0x3752, 0x0005,
	0x3756, 0x0000,
	0x3762, 0x0000,
	0x376E, 0x0005,
	0x39D6, 0x0064,
	0x39DA, 0x0032,
	0x40E8, 0x0909,
	0x40EA, 0x0909,
	0x40EC, 0x0909,
	0x410C, 0x0404,
	0x410E, 0x0404,
	0x4110, 0x0404,
	0x4130, 0x3131,
	0x4132, 0x3131,
	0x4134, 0x3131,
	0x465A, 0x0812,
	0x465C, 0x1212,
	0x465E, 0x1212,
	0x4660, 0x1200,
	0x46E2, 0x3535,
	0x46E4, 0x3535,
	0x46E6, 0x3535,
	0x46EA, 0x2035,
	0x46EC, 0x3535,
	0x46EE, 0x3535,
	0x46F0, 0x3501,
	0x46F4, 0xC8C8,
	0x46F6, 0xC8C8,
	0x46F8, 0xC8C8,
	0x46FC, 0x20C8,
	0x46FE, 0xC8C8,
	0x4700, 0xC8C8,
	0x4706, 0x8585,
	0x4708, 0x8585,
	0x470A, 0x8585,
	0x470E, 0x2085,
	0x4710, 0x8585,
	0x4712, 0x8585,
	0x4714, 0x8501,
	0x4718, 0x8585,
	0x471A, 0x8585,
	0x471C, 0x8585,
	0x4720, 0x2032,
	0x4722, 0x3232,
	0x4724, 0x3232,
	0x4726, 0x3201,
	0x472A, 0x3232,
	0x472C, 0x3232,
	0x472E, 0x3232,
	0x4732, 0x2021,
	0x4734, 0x2121,
	0x4736, 0x2121,
	0x4738, 0x2101,
	0x473C, 0x2121,
	0x473E, 0x2121,
	0x4740, 0x2121,
	0x4744, 0x2021,
	0x4746, 0x2121,
	0x4748, 0x2121,
	0x474A, 0x21FF,
	0xFCFC, 0x4000,
	0x0340, 0x0DB0,
	0x0342, 0x239F,
	0x0344, 0x0008,
	0x0346, 0x0008,
	0x0348, 0x1687,
	0x034A, 0x10E7,
	0x034C, 0x0B40,
	0x034E, 0x086C,
	0x0350, 0x0000,
	0x0352, 0x0002,
	0x0380, 0x0001,
	0x0382, 0x0001,
	0x0384, 0x0002,
	0x0386, 0x0002,
	0x0400, 0x2010,
	0x0900, 0x0112,
	0x0902, 0x0001,
	0x0B08, 0x0000,
	0xF466, 0x07FB,
	0xF468, 0x2000,
	0x6214, 0xE9F0,
	0x6218, 0xE9F0,
};

static void capture_setting(kal_uint16 currefps)
{
	pr_debug("capture_setting() E! currefps:%d\n", currefps);

	table_write_cmos_sensor(addr_data_pair_cap_2x5sp13,
			sizeof(addr_data_pair_cap_2x5sp13) / sizeof(kal_uint16));
}

static kal_uint16 addr_data_pair_video_2x5sp13[] = {
	0xFCFC, 0x2000,
	0x10E0, 0x2ED7,
	0x10E2, 0x2DB4,
	0x11A0, 0x0000,
	0x11C0, 0x0000,
	0x11C2, 0x0200,
	0x11C6, 0x0200,
	0x11CA, 0x0000,
	0x11CC, 0x0000,
	0x11CE, 0x0000,
	0x11D0, 0x0000,
	0x11D2, 0x0000,
	0x11D4, 0x0000,
	0x123C, 0x1212,
	0x129E, 0x0100,
	0x2CF0, 0x0300,
	0x2E9C, 0x0000,
	0x2EB6, 0x0101,
	0x3746, 0x0037,
	0x374A, 0x0001,
	0x3752, 0x0005,
	0x3756, 0x0000,
	0x3762, 0x0000,
	0x376E, 0x0005,
	0x39D6, 0x0064,
	0x39DA, 0x0032,
	0x40E8, 0x0909,
	0x40EA, 0x0909,
	0x40EC, 0x0909,
	0x410C, 0x0404,
	0x410E, 0x0404,
	0x4110, 0x0404,
	0x4130, 0x3131,
	0x4132, 0x3131,
	0x4134, 0x3131,
	0x465A, 0x0812,
	0x465C, 0x1212,
	0x465E, 0x1212,
	0x4660, 0x1200,
	0x46E2, 0x3535,
	0x46E4, 0x3535,
	0x46E6, 0x3535,
	0x46EA, 0x2035,
	0x46EC, 0x3535,
	0x46EE, 0x3535,
	0x46F0, 0x3501,
	0x46F4, 0xC8C8,
	0x46F6, 0xC8C8,
	0x46F8, 0xC8C8,
	0x46FC, 0x20C8,
	0x46FE, 0xC8C8,
	0x4700, 0xC8C8,
	0x4706, 0x8585,
	0x4708, 0x8585,
	0x470A, 0x8585,
	0x470E, 0x2085,
	0x4710, 0x8585,
	0x4712, 0x8585,
	0x4714, 0x8501,
	0x4718, 0x8585,
	0x471A, 0x8585,
	0x471C, 0x8585,
	0x4720, 0x2032,
	0x4722, 0x3232,
	0x4724, 0x3232,
	0x4726, 0x3201,
	0x472A, 0x3232,
	0x472C, 0x3232,
	0x472E, 0x3232,
	0x4732, 0x2021,
	0x4734, 0x2121,
	0x4736, 0x2121,
	0x4738, 0x2101,
	0x473C, 0x2121,
	0x473E, 0x2121,
	0x4740, 0x2121,
	0x4744, 0x2021,
	0x4746, 0x2121,
	0x4748, 0x2121,
	0x474A, 0x21FF,
	0xFCFC, 0x4000,
	0x0340, 0x0A48,
	0x0342, 0x2F6D,
	0x0344, 0x0008,
	0x0346, 0x0220,
	0x0348, 0x1687,
	0x034A, 0x0ECF,
	0x034C, 0x0B40,
	0x034E, 0x0654,
	0x0350, 0x0000,
	0x0352, 0x0002,
	0x0380, 0x0001,
	0x0382, 0x0001,
	0x0384, 0x0002,
	0x0386, 0x0002,
	0x0400, 0x2010,
	0x0900, 0x0112,
	0x0902, 0x0001,
	0x0B08, 0x0000,
	0xF466, 0x07FB,
	0xF468, 0x2000,
	0x6214, 0xE9F0,
	0x6218, 0xE9F0,
};

static void normal_video_setting(kal_uint16 currefps)
{
	pr_debug("normal_video_setting() E! currefps:%d\n", currefps);

	table_write_cmos_sensor(addr_data_pair_video_2x5sp13,
		sizeof(addr_data_pair_video_2x5sp13) / sizeof(kal_uint16));
}

static kal_uint16 addr_data_pair_hs_2x5sp13[] = {
	0xFCFC, 0x2000,
	0x10E0, 0x2ED7,
	0x10E2, 0x2DB4,
	0x11A0, 0x0000,
	0x11C0, 0x0000,
	0x11C2, 0x0200,
	0x11C6, 0x0200,
	0x11CA, 0x0000,
	0x11CC, 0x0000,
	0x11CE, 0x0000,
	0x11D0, 0x0000,
	0x11D2, 0x0000,
	0x11D4, 0x0000,
	0x123C, 0x1212,
	0x129E, 0x0101,
	0x2CF0, 0x0300,
	0x2E9C, 0x0002,
	0x2EB6, 0x0101,
	0x3746, 0x0037,
	0x374A, 0x0001,
	0x3752, 0x0005,
	0x3756, 0x0000,
	0x3762, 0x0000,
	0x376E, 0x0005,
	0x39D6, 0x0064,
	0x39DA, 0x0032,
	0x40E8, 0x0909,
	0x40EA, 0x0909,
	0x40EC, 0x0909,
	0x410C, 0x0404,
	0x410E, 0x0404,
	0x4110, 0x0404,
	0x4130, 0x3131,
	0x4132, 0x3131,
	0x4134, 0x3131,
	0x465A, 0x0812,
	0x465C, 0x1212,
	0x465E, 0x1212,
	0x4660, 0x1200,
	0x46E2, 0x3535,
	0x46E4, 0x3535,
	0x46E6, 0x3535,
	0x46EA, 0x2035,
	0x46EC, 0x3535,
	0x46EE, 0x3535,
	0x46F0, 0x3501,
	0x46F4, 0xC8C8,
	0x46F6, 0xC8C8,
	0x46F8, 0xC8C8,
	0x46FC, 0x20C8,
	0x46FE, 0xC8C8,
	0x4700, 0xC8C8,
	0x4706, 0x8585,
	0x4708, 0x8585,
	0x470A, 0x8585,
	0x470E, 0x2085,
	0x4710, 0x8585,
	0x4712, 0x8585,
	0x4714, 0x8501,
	0x4718, 0x8585,
	0x471A, 0x8585,
	0x471C, 0x8585,
	0x4720, 0x2032,
	0x4722, 0x3232,
	0x4724, 0x3232,
	0x4726, 0x3201,
	0x472A, 0x3232,
	0x472C, 0x3232,
	0x472E, 0x3232,
	0x4732, 0x2021,
	0x4734, 0x2121,
	0x4736, 0x2121,
	0x4738, 0x2101,
	0x473C, 0x2121,
	0x473E, 0x2121,
	0x4740, 0x2121,
	0x4744, 0x2021,
	0x4746, 0x2121,
	0x4748, 0x2121,
	0x474A, 0x21FF,
	0xFCFC, 0x4000,
	0x0340, 0x084C,
	0x0342, 0x0EB0,
	0x0344, 0x0008,
	0x0346, 0x0220,
	0x0348, 0x1687,
	0x034A, 0x0ECF,
	0x034C, 0x0B40,
	0x034E, 0x0654,
	0x0350, 0x0000,
	0x0352, 0x0002,
	0x0380, 0x0002,
	0x0382, 0x0002,
	0x0384, 0x0002,
	0x0386, 0x0002,
	0x0400, 0x1010,
	0x0900, 0x0122,
	0x0902, 0x0001,
	0x0B08, 0x0000,
	0xF466, 0x07FB,
	0xF468, 0x2000,
	0x6214, 0xE9F0,
	0x6218, 0xE9F0,
};

static void hs_video_setting(void)
{
	pr_debug("hs_video_setting() E\n");

	table_write_cmos_sensor(addr_data_pair_hs_2x5sp13,
			sizeof(addr_data_pair_hs_2x5sp13) / sizeof(kal_uint16));
}

static kal_uint16 addr_data_pair_slim_2x5sp13[] = {
	0xFCFC, 0x2000,
	0x10E0, 0x28D2,
	0x10E2, 0x27F1,
	0x11A0, 0x0001,
	0x11C0, 0x0100,
	0x11C2, 0x007F,
	0x11C6, 0x0080,
	0x11CA, 0x004B,
	0x11CC, 0x066B,
	0x11CE, 0xF466,
	0x11D0, 0xA000,
	0x11D2, 0x4000,
	0x11D4, 0xF468,
	0x123C, 0x1230,
	0x129E, 0x0100,
	0x2CF0, 0x0200,
	0x2E9C, 0x0000,
	0x2EB6, 0x0000,
	0x3746, 0x0038,
	0x374A, 0x0000,
	0x3752, 0x0006,
	0x3756, 0x0000,
	0x3762, 0x0000,
	0x376E, 0x0000,
	0x39D6, 0x0035,
	0x39DA, 0x0028,
	0x40E8, 0x0A0A,
	0x40EA, 0x0A0A,
	0x40EC, 0x0A0A,
	0x410C, 0x0505,
	0x410E, 0x0505,
	0x4110, 0x0505,
	0x4130, 0x3232,
	0x4132, 0x3232,
	0x4134, 0x3232,
	0x465A, 0x0813,
	0x465C, 0x1313,
	0x465E, 0x1313,
	0x4660, 0x1300,
	0x46E2, 0x7878,
	0x46E4, 0x5E42,
	0x46E6, 0x3636,
	0x46EA, 0x2078,
	0x46EC, 0x785E,
	0x46EE, 0x4236,
	0x46F0, 0x3601,
	0x46F4, 0x5A5A,
	0x46F6, 0x7896,
	0x46F8, 0xB4B4,
	0x46FC, 0x2064,
	0x46FE, 0x6485,
	0x4700, 0xA7C8,
	0x4706, 0x5353,
	0x4708, 0x708B,
	0x470A, 0xA7A7,
	0x470E, 0x2053,
	0x4710, 0x5370,
	0x4712, 0x8BA7,
	0x4714, 0xA701,
	0x4718, 0x6464,
	0x471A, 0x86A7,
	0x471C, 0xC8C8,
	0x4720, 0x2014,
	0x4722, 0x141B,
	0x4724, 0x2128,
	0x4726, 0x2801,
	0x472A, 0x1414,
	0x472C, 0x1B21,
	0x472E, 0x2828,
	0x4732, 0x201B,
	0x4734, 0x1B23,
	0x4736, 0x2D36,
	0x4738, 0x3601,
	0x473C, 0x3C3C,
	0x473E, 0x5064,
	0x4740, 0x7878,
	0x4744, 0x203C,
	0x4746, 0x3C50,
	0x4748, 0x6478,
	0x474A, 0x78FF,
	0xFCFC, 0x4000,
	0x0340, 0x1184,
	0x0342, 0x1BD8,
	0x0344, 0x0004,
	0x0346, 0x0008,
	0x0348, 0x168B,
	0x034A, 0x10E7,
	0x034C, 0x1680,
	0x034E, 0x10D8,
	0x0350, 0x0004,
	0x0352, 0x0004,
	0x0380, 0x0001,
	0x0382, 0x0001,
	0x0384, 0x0001,
	0x0386, 0x0001,
	0x0400, 0x1010,
	0x0900, 0x0011,
	0x0902, 0x0000,
	0x0B08, 0x0001,
	0xF466, 0x004B,
	0xF468, 0xA000,
	0x6214, 0xE9F0,
	0x6218, 0xE9F0,
};

static void slim_video_setting(void)
{
	pr_debug("slim_video_setting() E\n");

	table_write_cmos_sensor(addr_data_pair_slim_2x5sp13,
		sizeof(addr_data_pair_slim_2x5sp13) / sizeof(kal_uint16));
}

static kal_uint16 addr_data_pair_custom1_2x5sp13[] = {
	0xFCFC, 0x2000,
	0x10E0, 0x2ED7,
	0x10E2, 0x2DB4,
	0x11A0, 0x0000,
	0x11C0, 0x0000,
	0x11C2, 0x0200,
	0x11C6, 0x0200,
	0x11CA, 0x0000,
	0x11CC, 0x0000,
	0x11CE, 0x0000,
	0x11D0, 0x0000,
	0x11D2, 0x0000,
	0x11D4, 0x0000,
	0x123C, 0x1212,
	0x129E, 0x0101,
	0x2CF0, 0x0300,
	0x2E9C, 0x0002,
	0x2EB6, 0x0101,
	0x3746, 0x0037,
	0x374A, 0x0001,
	0x3752, 0x0005,
	0x3756, 0x0000,
	0x3762, 0x0000,
	0x376E, 0x0005,
	0x39D6, 0x0064,
	0x39DA, 0x0032,
	0x40E8, 0x0909,
	0x40EA, 0x0909,
	0x40EC, 0x0909,
	0x410C, 0x0404,
	0x410E, 0x0404,
	0x4110, 0x0404,
	0x4130, 0x3131,
	0x4132, 0x3131,
	0x4134, 0x3131,
	0x465A, 0x0812,
	0x465C, 0x1212,
	0x465E, 0x1212,
	0x4660, 0x1200,
	0x46E2, 0x3535,
	0x46E4, 0x3535,
	0x46E6, 0x3535,
	0x46EA, 0x2035,
	0x46EC, 0x3535,
	0x46EE, 0x3535,
	0x46F0, 0x3501,
	0x46F4, 0xC8C8,
	0x46F6, 0xC8C8,
	0x46F8, 0xC8C8,
	0x46FC, 0x20C8,
	0x46FE, 0xC8C8,
	0x4700, 0xC8C8,
	0x4706, 0x8585,
	0x4708, 0x8585,
	0x470A, 0x8585,
	0x470E, 0x2085,
	0x4710, 0x8585,
	0x4712, 0x8585,
	0x4714, 0x8501,
	0x4718, 0x8585,
	0x471A, 0x8585,
	0x471C, 0x8585,
	0x4720, 0x2032,
	0x4722, 0x3232,
	0x4724, 0x3232,
	0x4726, 0x3201,
	0x472A, 0x3232,
	0x472C, 0x3232,
	0x472E, 0x3232,
	0x4732, 0x2021,
	0x4734, 0x2121,
	0x4736, 0x2121,
	0x4738, 0x2101,
	0x473C, 0x2121,
	0x473E, 0x2121,
	0x4740, 0x2121,
	0x4744, 0x2021,
	0x4746, 0x2121,
	0x4748, 0x2121,
	0x474A, 0x21FF,
	0xFCFC, 0x4000,
	0x0340, 0x096E,
	0x0342, 0x0F80,
	0x0344, 0x0008,
	0x0346, 0x0008,
	0x0348, 0x1687,
	0x034A, 0x10E7,
	0x034C, 0x0B40,
	0x034E, 0x086C,
	0x0350, 0x0000,
	0x0352, 0x0002,
	0x0380, 0x0002,
	0x0382, 0x0002,
	0x0384, 0x0002,
	0x0386, 0x0002,
	0x0400, 0x1010,
	0x0900, 0x0122,
	0x0902, 0x0001,
	0x0B08, 0x0000,
	0xF466, 0x07FB,
	0xF468, 0x2000,
	0x6214, 0xE9F0,
	0x6218, 0xE9F0,
};

static void custom1_setting(void)
{
	pr_debug("use custom1\n");
	table_write_cmos_sensor(addr_data_pair_custom1_2x5sp13,
			sizeof(addr_data_pair_custom1_2x5sp13) / sizeof(kal_uint16));
}

static kal_uint16 addr_data_pair_custom2_2x5sp13[] = {
	0xFCFC, 0x2000,
	0x10E0, 0x2ED7,
	0x10E2, 0x2DB4,
	0x11A0, 0x0000,
	0x11C0, 0x0000,
	0x11C2, 0x0200,
	0x11C6, 0x0200,
	0x11CA, 0x0000,
	0x11CC, 0x0000,
	0x11CE, 0x0000,
	0x11D0, 0x0000,
	0x11D2, 0x0000,
	0x11D4, 0x0000,
	0x123C, 0x1212,
	0x129E, 0x0100,
	0x2CF0, 0x0300,
	0x2E9C, 0x0000,
	0x2EB6, 0x0101,
	0x3746, 0x0037,
	0x374A, 0x0001,
	0x3752, 0x0005,
	0x3756, 0x0000,
	0x3762, 0x0000,
	0x376E, 0x0005,
	0x39D6, 0x0064,
	0x39DA, 0x0032,
	0x40E8, 0x0909,
	0x40EA, 0x0909,
	0x40EC, 0x0909,
	0x410C, 0x0404,
	0x410E, 0x0404,
	0x4110, 0x0404,
	0x4130, 0x3131,
	0x4132, 0x3131,
	0x4134, 0x3131,
	0x465A, 0x0812,
	0x465C, 0x1212,
	0x465E, 0x1212,
	0x4660, 0x1200,
	0x46E2, 0x3535,
	0x46E4, 0x3535,
	0x46E6, 0x3535,
	0x46EA, 0x2035,
	0x46EC, 0x3535,
	0x46EE, 0x3535,
	0x46F0, 0x3501,
	0x46F4, 0xC8C8,
	0x46F6, 0xC8C8,
	0x46F8, 0xC8C8,
	0x46FC, 0x20C8,
	0x46FE, 0xC8C8,
	0x4700, 0xC8C8,
	0x4706, 0x8585,
	0x4708, 0x8585,
	0x470A, 0x8585,
	0x470E, 0x2085,
	0x4710, 0x8585,
	0x4712, 0x8585,
	0x4714, 0x8501,
	0x4718, 0x8585,
	0x471A, 0x8585,
	0x471C, 0x8585,
	0x4720, 0x2032,
	0x4722, 0x3232,
	0x4724, 0x3232,
	0x4726, 0x3201,
	0x472A, 0x3232,
	0x472C, 0x3232,
	0x472E, 0x3232,
	0x4732, 0x2021,
	0x4734, 0x2121,
	0x4736, 0x2121,
	0x4738, 0x2101,
	0x473C, 0x2121,
	0x473E, 0x2121,
	0x4740, 0x2121,
	0x4744, 0x2021,
	0x4746, 0x2121,
	0x4748, 0x2121,
	0x474A, 0x21FF,
	0xFCFC, 0x4000,
	0x0340, 0x08E3,
	0x0342, 0x36DE,
	0x0344, 0x0214,
	0x0346, 0x0190,
	0x0348, 0x147B,
	0x034A, 0x0F5F,
	0x034C, 0x0930,
	0x034E, 0x06E4,
	0x0350, 0x0002,
	0x0352, 0x0002,
	0x0380, 0x0001,
	0x0382, 0x0001,
	0x0384, 0x0002,
	0x0386, 0x0002,
	0x0400, 0x2010,
	0x0900, 0x0112,
	0x0902, 0x0001,
	0x0B08, 0x0000,
	0xF466, 0x07FB,
	0xF468, 0x2000,
	0x6214, 0xE9F0,
	0x6218, 0xE9F0,
};

static void custom2_setting(void)
{
	pr_debug("use custom2\n");
	table_write_cmos_sensor(addr_data_pair_custom2_2x5sp13,
		sizeof(addr_data_pair_custom2_2x5sp13) / sizeof(kal_uint16));
}

static kal_uint16 addr_data_pair_custom3_2x5sp13[] = {
	0xFCFC, 0x2000,
	0x10E0, 0x2ED7,
	0x10E2, 0x2DB4,
	0x11A0, 0x0000,
	0x11C0, 0x0000,
	0x11C2, 0x0200,
	0x11C6, 0x0200,
	0x11CA, 0x0000,
	0x11CC, 0x0000,
	0x11CE, 0x0000,
	0x11D0, 0x0000,
	0x11D2, 0x0000,
	0x11D4, 0x0000,
	0x123C, 0x1212,
	0x129E, 0x0100,
	0x2CF0, 0x0300,
	0x2E9C, 0x0000,
	0x2EB6, 0x0101,
	0x3746, 0x0037,
	0x374A, 0x0001,
	0x3752, 0x0005,
	0x3756, 0x0000,
	0x3762, 0x0000,
	0x376E, 0x0005,
	0x39D6, 0x0064,
	0x39DA, 0x0032,
	0x40E8, 0x0909,
	0x40EA, 0x0909,
	0x40EC, 0x0909,
	0x410C, 0x0404,
	0x410E, 0x0404,
	0x4110, 0x0404,
	0x4130, 0x3131,
	0x4132, 0x3131,
	0x4134, 0x3131,
	0x465A, 0x0812,
	0x465C, 0x1212,
	0x465E, 0x1212,
	0x4660, 0x1200,
	0x46E2, 0x3535,
	0x46E4, 0x3535,
	0x46E6, 0x3535,
	0x46EA, 0x2035,
	0x46EC, 0x3535,
	0x46EE, 0x3535,
	0x46F0, 0x3501,
	0x46F4, 0xC8C8,
	0x46F6, 0xC8C8,
	0x46F8, 0xC8C8,
	0x46FC, 0x20C8,
	0x46FE, 0xC8C8,
	0x4700, 0xC8C8,
	0x4706, 0x8585,
	0x4708, 0x8585,
	0x470A, 0x8585,
	0x470E, 0x2085,
	0x4710, 0x8585,
	0x4712, 0x8585,
	0x4714, 0x8501,
	0x4718, 0x8585,
	0x471A, 0x8585,
	0x471C, 0x8585,
	0x4720, 0x2032,
	0x4722, 0x3232,
	0x4724, 0x3232,
	0x4726, 0x3201,
	0x472A, 0x3232,
	0x472C, 0x3232,
	0x472E, 0x3232,
	0x4732, 0x2021,
	0x4734, 0x2121,
	0x4736, 0x2121,
	0x4738, 0x2101,
	0x473C, 0x2121,
	0x473E, 0x2121,
	0x4740, 0x2121,
	0x4744, 0x2021,
	0x4746, 0x2121,
	0x4748, 0x2121,
	0x474A, 0x21FF,
	0xFCFC, 0x4000,
	0x0340, 0x08E3,
	0x0342, 0x36DE,
	0x0344, 0x0214,
	0x0346, 0x0190,
	0x0348, 0x147B,
	0x034A, 0x0F5F,
	0x034C, 0x0930,
	0x034E, 0x06E4,
	0x0350, 0x0002,
	0x0352, 0x0002,
	0x0380, 0x0001,
	0x0382, 0x0001,
	0x0384, 0x0002,
	0x0386, 0x0002,
	0x0400, 0x2010,
	0x0900, 0x0112,
	0x0902, 0x0001,
	0x0B08, 0x0000,
	0xF466, 0x07FB,
	0xF468, 0x2000,
	0x6214, 0xE9F0,
	0x6218, 0xE9F0,
};

static void custom3_setting(void)
{
	pr_debug("use custom3\n");
	table_write_cmos_sensor(addr_data_pair_custom3_2x5sp13,
		sizeof(addr_data_pair_custom3_2x5sp13) / sizeof(kal_uint16));
}

static kal_uint16 addr_data_pair_custom4_2x5sp13[] = {
	0xFCFC, 0x2000,
	0x10E0, 0x2ED7,
	0x10E2, 0x2DB4,
	0x11A0, 0x0000,
	0x11C0, 0x0000,
	0x11C2, 0x0200,
	0x11C6, 0x0200,
	0x11CA, 0x0000,
	0x11CC, 0x0000,
	0x11CE, 0x0000,
	0x11D0, 0x0000,
	0x11D2, 0x0000,
	0x11D4, 0x0000,
	0x123C, 0x1212,
	0x129E, 0x0100,
	0x2CF0, 0x0300,
	0x2E9C, 0x0000,
	0x2EB6, 0x0101,
	0x3746, 0x0037,
	0x374A, 0x0001,
	0x3752, 0x0005,
	0x3756, 0x0000,
	0x3762, 0x0000,
	0x376E, 0x0005,
	0x39D6, 0x0064,
	0x39DA, 0x0032,
	0x40E8, 0x0909,
	0x40EA, 0x0909,
	0x40EC, 0x0909,
	0x410C, 0x0404,
	0x410E, 0x0404,
	0x4110, 0x0404,
	0x4130, 0x3131,
	0x4132, 0x3131,
	0x4134, 0x3131,
	0x465A, 0x0812,
	0x465C, 0x1212,
	0x465E, 0x1212,
	0x4660, 0x1200,
	0x46E2, 0x3535,
	0x46E4, 0x3535,
	0x46E6, 0x3535,
	0x46EA, 0x2035,
	0x46EC, 0x3535,
	0x46EE, 0x3535,
	0x46F0, 0x3501,
	0x46F4, 0xC8C8,
	0x46F6, 0xC8C8,
	0x46F8, 0xC8C8,
	0x46FC, 0x20C8,
	0x46FE, 0xC8C8,
	0x4700, 0xC8C8,
	0x4706, 0x8585,
	0x4708, 0x8585,
	0x470A, 0x8585,
	0x470E, 0x2085,
	0x4710, 0x8585,
	0x4712, 0x8585,
	0x4714, 0x8501,
	0x4718, 0x8585,
	0x471A, 0x8585,
	0x471C, 0x8585,
	0x4720, 0x2032,
	0x4722, 0x3232,
	0x4724, 0x3232,
	0x4726, 0x3201,
	0x472A, 0x3232,
	0x472C, 0x3232,
	0x472E, 0x3232,
	0x4732, 0x2021,
	0x4734, 0x2121,
	0x4736, 0x2121,
	0x4738, 0x2101,
	0x473C, 0x2121,
	0x473E, 0x2121,
	0x4740, 0x2121,
	0x4744, 0x2021,
	0x4746, 0x2121,
	0x4748, 0x2121,
	0x474A, 0x21FF,
	0xFCFC, 0x4000,
	0x0340, 0x0867,
	0x0342, 0x3A07,
	0x0344, 0x0214,
	0x0346, 0x0348,
	0x0348, 0x147B,
	0x034A, 0x0DA7,
	0x034C, 0x0930,
	0x034E, 0x052C,
	0x0350, 0x0002,
	0x0352, 0x0002,
	0x0380, 0x0001,
	0x0382, 0x0001,
	0x0384, 0x0002,
	0x0386, 0x0002,
	0x0400, 0x2010,
	0x0900, 0x0112,
	0x0902, 0x0001,
	0x0B08, 0x0000,
	0xF466, 0x07FB,
	0xF468, 0x2000,
	0x6214, 0xE9F0,
	0x6218, 0xE9F0,
};

static void custom4_setting(void)
{
	pr_debug("use custom4\n");
	table_write_cmos_sensor(addr_data_pair_custom4_2x5sp13,
		sizeof(addr_data_pair_custom4_2x5sp13) / sizeof(kal_uint16));
}

static kal_uint16 addr_data_pair_custom5_2x5sp13[] = {
	0xFCFC, 0x2000,
	0x10E0, 0x28D2,
	0x10E2, 0x27F1,
	0x11A0, 0x0001,
	0x11C0, 0x0100,
	0x11C2, 0x007F,
	0x11C6, 0x0080,
	0x11CA, 0x004B,
	0x11CC, 0x066B,
	0x11CE, 0xF466,
	0x11D0, 0xA000,
	0x11D2, 0x4000,
	0x11D4, 0xF468,
	0x123C, 0x1230,
	0x129E, 0x0100,
	0x2CF0, 0x0200,
	0x2E9C, 0x0000,
	0x2EB6, 0x0000,
	0x3746, 0x0038,
	0x374A, 0x0000,
	0x3752, 0x0006,
	0x3756, 0x0000,
	0x3762, 0x0000,
	0x376E, 0x0000,
	0x39D6, 0x0035,
	0x39DA, 0x0028,
	0x40E8, 0x0A0A,
	0x40EA, 0x0A0A,
	0x40EC, 0x0A0A,
	0x410C, 0x0505,
	0x410E, 0x0505,
	0x4110, 0x0505,
	0x4130, 0x3232,
	0x4132, 0x3232,
	0x4134, 0x3232,
	0x465A, 0x0813,
	0x465C, 0x1313,
	0x465E, 0x1313,
	0x4660, 0x1300,
	0x46E2, 0x7878,
	0x46E4, 0x5E42,
	0x46E6, 0x3636,
	0x46EA, 0x2078,
	0x46EC, 0x785E,
	0x46EE, 0x4236,
	0x46F0, 0x3601,
	0x46F4, 0x5A5A,
	0x46F6, 0x7896,
	0x46F8, 0xB4B4,
	0x46FC, 0x2064,
	0x46FE, 0x6485,
	0x4700, 0xA7C8,
	0x4706, 0x5353,
	0x4708, 0x708B,
	0x470A, 0xA7A7,
	0x470E, 0x2053,
	0x4710, 0x5370,
	0x4712, 0x8BA7,
	0x4714, 0xA701,
	0x4718, 0x6464,
	0x471A, 0x86A7,
	0x471C, 0xC8C8,
	0x4720, 0x2014,
	0x4722, 0x141B,
	0x4724, 0x2128,
	0x4726, 0x2801,
	0x472A, 0x1414,
	0x472C, 0x1B21,
	0x472E, 0x2828,
	0x4732, 0x201B,
	0x4734, 0x1B23,
	0x4736, 0x2D36,
	0x4738, 0x3601,
	0x473C, 0x3C3C,
	0x473E, 0x5064,
	0x4740, 0x7878,
	0x4744, 0x203C,
	0x4746, 0x3C50,
	0x4748, 0x6478,
	0x474A, 0x78FF,
	0xFCFC, 0x4000,
	0x0340, 0x10EA,
	0x0342, 0x1CD5,
	0x0344, 0x021C,
	0x0346, 0x0196,
	0x0348, 0x1473,
	0x034A, 0x0F59,
	0x034C, 0x1250,
	0x034E, 0x0DBC,
	0x0350, 0x0004,
	0x0352, 0x0004,
	0x0380, 0x0001,
	0x0382, 0x0001,
	0x0384, 0x0001,
	0x0386, 0x0001,
	0x0400, 0x1010,
	0x0900, 0x0011,
	0x0902, 0x0000,
	0x0B08, 0x0001,
	0xF466, 0x004B,
	0xF468, 0xA000,
	0x6214, 0xE9F0,
	0x6218, 0xE9F0,
};

static void custom5_setting(void)
{
	pr_debug("use custom5\n");
	table_write_cmos_sensor(addr_data_pair_custom5_2x5sp13,
		sizeof(addr_data_pair_custom5_2x5sp13) / sizeof(kal_uint16));
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
				pr_debug("i2c write id: 0x%x, sensor id: 0x%x\n", imgsensor.i2c_write_id, *sensor_id);
			return ERROR_NONE;
			}
			pr_debug("Read sensor id fail, id: 0x%x\n", imgsensor.i2c_write_id);
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

	pr_debug("%s", __func__);

	/* sensor have two i2c address 0x6c 0x6d & 0x21 0x20,
	 * we should detect the module used i2c address
	 */
	while (imgsensor_info.i2c_addr_table[i] != 0xff) {
		spin_lock(&imgsensor_drv_lock);
		imgsensor.i2c_write_id = imgsensor_info.i2c_addr_table[i];
		spin_unlock(&imgsensor_drv_lock);

		do {
			sensor_id = ( (read_cmos_sensor_8(0x0000) << 8) | read_cmos_sensor_8(0x0001) );

			if (sensor_id == imgsensor_info.sensor_id) {
				pr_debug("open():i2c write id: 0x%x, sensor id: 0x%x\n", imgsensor.i2c_write_id, sensor_id);
				break;
			}

			pr_debug("open():Read sensor id fail, id: 0x%x\n", imgsensor.i2c_write_id);
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

	spin_lock(&imgsensor_drv_lock);

	imgsensor.autoflicker_en 	= KAL_FALSE;
	imgsensor.sensor_mode 		= IMGSENSOR_MODE_INIT;
	imgsensor.shutter 		= 0x3D0;
	imgsensor.gain 			= 0x100;
	imgsensor.pclk 			= imgsensor_info.pre.pclk;
	imgsensor.frame_length 		= imgsensor_info.pre.framelength;
	imgsensor.line_length		= imgsensor_info.pre.linelength;
	imgsensor.min_frame_length 	= imgsensor_info.pre.framelength;
	imgsensor.dummy_pixel 		= 0;
	imgsensor.dummy_line 		= 0;
	imgsensor.ihdr_mode 		= 0;
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
	pr_debug("close() E\n");

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
	pr_debug("preview E\n");

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode 			= IMGSENSOR_MODE_PREVIEW;
	imgsensor.pclk 					= imgsensor_info.pre.pclk;
	imgsensor.line_length 			= imgsensor_info.pre.linelength;
	imgsensor.frame_length 			= imgsensor_info.pre.framelength;
	imgsensor.min_frame_length 		= imgsensor_info.pre.framelength;
	imgsensor.autoflicker_en 		= KAL_FALSE;
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
	pr_debug("capture E\n");

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

static kal_uint32 normal_video( MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
	MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	pr_debug("normal_video E\n");

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode 			= IMGSENSOR_MODE_VIDEO;
	imgsensor.pclk 					= imgsensor_info.normal_video.pclk;
	imgsensor.line_length 			= imgsensor_info.normal_video.linelength;
	imgsensor.frame_length 			= imgsensor_info.normal_video.framelength;
	imgsensor.min_frame_length 		= imgsensor_info.normal_video.framelength;
	imgsensor.autoflicker_en 		= KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);

	normal_video_setting(imgsensor.current_fps);

	set_mirror_flip(imgsensor.mirror);

	return ERROR_NONE;
}

static kal_uint32 hs_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			   MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	pr_debug("hs_video E\n");

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

	hs_video_setting();

	set_mirror_flip(imgsensor.mirror);

	return ERROR_NONE;
}

static kal_uint32 slim_video(
	MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
	MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	pr_debug("slim_video E\n");

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode 			= IMGSENSOR_MODE_SLIM_VIDEO;
	imgsensor.pclk 				= imgsensor_info.slim_video.pclk;
	imgsensor.line_length 			= imgsensor_info.slim_video.linelength;
	imgsensor.frame_length 			= imgsensor_info.slim_video.framelength;
	imgsensor.min_frame_length 		= imgsensor_info.slim_video.framelength;

	imgsensor.dummy_line 			= 0;
	imgsensor.dummy_pixel			= 0;
	imgsensor.autoflicker_en 		= KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);

	slim_video_setting();

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

	custom1_setting();

	set_mirror_flip(IMAGE_NORMAL);


    return ERROR_NONE;
}

static kal_uint32 custom2(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
				MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	pr_debug("E cur fps: %d\n", imgsensor.current_fps);

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
	pr_debug("E cur fps: %d\n", imgsensor.current_fps);

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
	pr_debug("E cur fps: %d\n", imgsensor.current_fps);

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
	pr_debug("E cur fps: %d\n", imgsensor.current_fps);

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

	sensor_info->SensorOutputDataFormat = imgsensor_info.sensor_output_dataformat;

	sensor_info->CaptureDelayFrame 	= imgsensor_info.cap_delay_frame;
	sensor_info->PreviewDelayFrame 	= imgsensor_info.pre_delay_frame;
	sensor_info->VideoDelayFrame 	= imgsensor_info.video_delay_frame;

	sensor_info->HighSpeedVideoDelayFrame	= imgsensor_info.hs_video_delay_frame;

	sensor_info->SlimVideoDelayFrame 	= imgsensor_info.slim_video_delay_frame;

	sensor_info->Custom1DelayFrame 		= imgsensor_info.custom1_delay_frame;
	sensor_info->Custom2DelayFrame = imgsensor_info.custom2_delay_frame;
	sensor_info->Custom3DelayFrame = imgsensor_info.custom3_delay_frame;
	sensor_info->Custom4DelayFrame = imgsensor_info.custom4_delay_frame;
	sensor_info->Custom5DelayFrame = imgsensor_info.custom5_delay_frame;
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
	pr_debug("scenario_id = %d\n", scenario_id);

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
		pr_debug("Error ScenarioId setting");
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
		LOG_INF(
		"MSDK_SCENARIO_ID_CUSTOM4 scenario_id= %d\n",
			scenario_id);
		frame_length = imgsensor_info.custom4.pclk
			/ framerate * 10 / imgsensor_info.custom4.linelength;

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
		LOG_INF(
		"MSDK_SCENARIO_ID_CUSTOM5 scenario_id= %d\n",
			scenario_id);
		frame_length = imgsensor_info.custom5.pclk
			/ framerate * 10 / imgsensor_info.custom5.linelength;

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
	case MSDK_SCENARIO_ID_CUSTOM3:
		*framerate = imgsensor_info.custom3.max_framerate;
		pr_debug("custom3 *framerate = %d\n", *framerate);
		break;
	case MSDK_SCENARIO_ID_CUSTOM4:
		*framerate = imgsensor_info.custom4.max_framerate;
		pr_debug("custom4 *framerate = %d\n", *framerate);
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

static kal_uint32 set_test_pattern_mode(kal_bool enable)
{
	pr_debug("enable: %d\n", enable);

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

	if (temperature >= 0x0 && temperature <= 0x78)
		temperature_convert = temperature;
	else
		temperature_convert = -1;

	LOG_INF("temp_c(%d), read_reg(%d), enable %d\n", temperature_convert, temperature, read_cmos_sensor_8(0x0138));

	return temperature_convert;
}

#if defined(CONFIG_TRAN_CAMERA_SYNC_AWB_TO_KERNEL)
static void set_awbgain(kal_uint32 g_gain,kal_uint32 r_gain, kal_uint32 b_gain)
{
	kal_uint32 r_gain_int = 0x0;
	kal_uint32 b_gain_int = 0x0;
	r_gain_int = r_gain / 2;
	b_gain_int = b_gain / 2;

	pr_debug("set_awbgain r_gain=0x%x , b_gain=0x%x\n", r_gain,b_gain);

	write_cmos_sensor(0x0D82, r_gain_int);
	write_cmos_sensor(0x0D86, b_gain_int);
}
#endif

static kal_uint32 feature_control( MSDK_SENSOR_FEATURE_ENUM feature_id,
				  UINT8 *feature_para, UINT32 *feature_para_len)
{
	UINT16 *feature_return_para_16 	= (UINT16 *) feature_para;
	UINT16 *feature_data_16 		= (UINT16 *) feature_para;
	UINT32 *feature_return_para_32 	= (UINT32 *) feature_para;
	UINT32 *feature_data_32 		= (UINT32 *) feature_para;
	INT32 *feature_return_para_i32 	= (INT32 *) feature_para;

	unsigned long long *feature_data = (unsigned long long *)feature_para;
	struct SENSOR_WINSIZE_INFO_STRUCT *wininfo;

	MSDK_SENSOR_REG_INFO_STRUCT *sensor_reg_data =
		( MSDK_SENSOR_REG_INFO_STRUCT *) feature_para;

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
	LOG_INF("SENSOR_FEATURE_SET_ESHUTTER = %d\n", feature_id);
		set_shutter(*feature_data);
		break;
	case SENSOR_FEATURE_SET_NIGHTMODE:
	LOG_INF("SENSOR_FEATURE_SET_NIGHTMODE = %d\n", feature_id);
		break;
	case SENSOR_FEATURE_SET_GAIN:
	LOG_INF("SENSOR_FEATURE_SET_GAIN = %d\n", feature_id);
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
			       sizeof(struct SENSOR_WINSIZE_INFO_STRUCT) );
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
	LOG_INF("SENSOR_FEATURE_SET_AWB_GAIN = %d\n", feature_id);
 #if defined(CONFIG_TRAN_CAMERA_SYNC_AWB_TO_KERNEL)
		set_awbgain((UINT32)(*feature_data_32),
					(UINT32)*(feature_data_32 + 1),
					(UINT32)*(feature_data_32 + 2));
 #endif
		break;
	case SENSOR_FEATURE_SET_HDR_SHUTTER:
	LOG_INF("SENSOR_FEATURE_SET_HDR_SHUTTER = %d\n", feature_id);
		pr_debug("SENSOR_FEATURE_SET_HDR_SHUTTER LE=%d, SE=%d\n",
			(UINT16) *feature_data, (UINT16) *(feature_data + 1));
		break;
	case SENSOR_FEATURE_SET_STREAMING_SUSPEND:
		pr_debug("SENSOR_FEATURE_SET_STREAMING_SUSPEND\n");
		streaming_control(KAL_FALSE);
		break;
	case SENSOR_FEATURE_SET_STREAMING_RESUME:
		pr_debug("SENSOR_FEATURE_SET_STREAMING_RESUME, shutter:%llu\n", *feature_data);
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
			case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
			case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
			case MSDK_SCENARIO_ID_CUSTOM1:
			case MSDK_SCENARIO_ID_CUSTOM2:
			case MSDK_SCENARIO_ID_CUSTOM3:
			case MSDK_SCENARIO_ID_CUSTOM4:
				*feature_return_para_32 = 4;
			break;
			case MSDK_SCENARIO_ID_SLIM_VIDEO:
			case MSDK_SCENARIO_ID_CUSTOM5:
				*feature_return_para_32 = 2;
			break;
			default:
				*feature_return_para_32 = 1;
			break;
			}

		pr_debug(
		"SENSOR_FEATURE_GET_BINNING_TYPE AE_binning_type:%d,\n",
		*feature_return_para_32);
		*feature_para_len = 4;

			break;
	case SENSOR_FEATURE_GET_AWB_REQ_BY_SCENARIO:
		switch (*feature_data) {
#if defined(CONFIG_TRAN_CAMERA_SYNC_AWB_TO_KERNEL)
		case MSDK_SCENARIO_ID_CUSTOM2:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) = 1;
			break;
#endif
		default:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) = 0;
			break;
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

UINT32 S5K2X5SP13_MIPI_RAW_SensorInit(struct SENSOR_FUNCTION_STRUCT **pfFunc)
{
	/* To Do : Check Sensor status here */
	if (pfFunc != NULL)
		*pfFunc = &sensor_func;
	return ERROR_NONE;
}
