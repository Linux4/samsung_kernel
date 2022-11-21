// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/videodev2.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/atomic.h>
#include <linux/types.h>

#include "sr846mipiraw_Sensor.h"

#define PFX "sr846_camera_sensor"
#define LOG_INF(format, args...)    \
	pr_debug(PFX "[%s] " format, __func__, ##args)

#define MULTI_WRITE 1
static DEFINE_SPINLOCK(imgsensor_drv_lock);

#define per_frame 1

static struct imgsensor_info_struct imgsensor_info = {
	.sensor_id = SR846_SENSOR_ID,
	.checksum_value = 0x55e2a82f,
	.pre = {
		.pclk = 287733300,
		.linelength = 3800,
		.framelength = 2523,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 3264,
		.grabwindow_height = 2448,
		.mipi_data_lp2hs_settle_dc = 14,
		.mipi_pixel_rate = 272132000,
		.max_framerate = 300,
	},
	.cap = {
		.pclk = 287733300,
		.linelength = 3800,
		.framelength = 2523,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 3264,
		.grabwindow_height = 2448,
		.mipi_data_lp2hs_settle_dc = 14,
		.mipi_pixel_rate = 272132000,
		.max_framerate = 300,
	},
	.cap1 = {
		.pclk = 176000000,
		.linelength = 2816,
		.framelength = 4166,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 2592,
		.grabwindow_height = 1944,
		.mipi_data_lp2hs_settle_dc = 14,
		.max_framerate = 150,
	},
	.normal_video = {
		.pclk = 287733300,
		.linelength = 3800,
		.framelength = 2523,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 3264,
		.grabwindow_height = 1836,
		.mipi_data_lp2hs_settle_dc = 14,
		.mipi_pixel_rate = 272132000,
		.max_framerate = 300,
	},
	.hs_video = {
	.pclk = 176000000,
	.linelength = 2816,
		.framelength = 520,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 640,
		.grabwindow_height = 480,
		.mipi_data_lp2hs_settle_dc = 14,//unit , ns
		.max_framerate = 1200,
	},
	.slim_video = {
		.pclk = 176000000,
		.linelength = 2816,
		.framelength = 2083,
		.startx = 0,
		.starty = 0,
	.grabwindow_width = 1280,
	.grabwindow_height = 720,
	.mipi_data_lp2hs_settle_dc = 14,//unit , ns
	.max_framerate = 300,
	},

	.margin = 6,
	.min_shutter = 6,
	.max_frame_length = 0x7FFF,
#if per_frame
	.ae_shut_delay_frame = 0,
	.ae_sensor_gain_delay_frame = 0,
	.ae_ispGain_delay_frame = 2,
#else
	.ae_shut_delay_frame = 0,
	.ae_sensor_gain_delay_frame = 1,
	.ae_ispGain_delay_frame = 2,
#endif

	.ae_ispGain_delay_frame = 2,
	.ihdr_support = 0,      //1, support; 0,not support
	.ihdr_le_firstline = 0,  //1,le first ; 0, se first
	.sensor_mode_num = 5,	  //support sensor mode num

	.cap_delay_frame = 3,
	.pre_delay_frame = 3,
	.video_delay_frame = 3,
	.hs_video_delay_frame = 3,
	.slim_video_delay_frame = 3,

	//.cap_delay_frame = 2,
	//.pre_delay_frame = 2,
	//.video_delay_frame = 2,
	//enter high speed video  delay frame num
	//.hs_video_delay_frame = 2,
	//enter slim video delay frame num
	//.slim_video_delay_frame = 2,

	.isp_driving_current = ISP_DRIVING_6MA,
	.sensor_interface_type = SENSOR_INTERFACE_TYPE_MIPI,
	.mipi_sensor_type = MIPI_OPHY_NCSI2,
	.mipi_settle_delay_mode = 1,
	.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_Gr,
	.mclk = 26,
	.mipi_lane_num = SENSOR_MIPI_4_LANE,
	.i2c_addr_table = {0x40, 0xff},
	.i2c_speed = 400,
};

static struct imgsensor_struct imgsensor = {
	.mirror = IMAGE_NORMAL,
	.sensor_mode = IMGSENSOR_MODE_INIT,
	.shutter = 0x0100,
	.gain = 0xe0,
	.dummy_pixel = 0,
	.dummy_line = 0,
//full size current fps : 24fps for PIP, 30fps for Normal or ZSD
	.current_fps = 300,
	.autoflicker_en = KAL_FALSE,
	.test_pattern = KAL_FALSE,
	.current_scenario_id = MSDK_SCENARIO_ID_CAMERA_PREVIEW,
	.ihdr_en = 0,
	.i2c_write_id = 0x40,
};

/* Sensor output window information */
static struct SENSOR_WINSIZE_INFO_STRUCT imgsensor_winsize_info[5] = {
	{ 3280, 2460,    0,    0, 3280, 2460, 3280, 2460,
		 8,    6, 3264, 2448,    0,    0, 3264, 2448 }, /*preview*/
	{ 3280, 2460,    0,    0, 3280, 2460, 3280, 2460,
		 8,    6, 3264, 2448,    0,    0, 3264, 2448 }, /*capture*/
	{ 3280, 2460,    0,    0, 3280, 2460, 3280, 2460,
		 8,  312, 3264, 1836,    0,    0, 3264, 1836 }, /*video*/
	{ 2592, 1944,   16,   12, 2560, 1920,  640,  480,
		 0,   0,   640,  480,    0,    0,  640,  480}, /*high speed video-NOT Arranged*/
	{ 2592, 1944,   16,  252, 2560, 1440, 1280,  720,
		 0,    0, 1280,  720,    0,    0, 1280,  720}, /*slim video-NOT Arranged*/
};

#if MULTI_WRITE
#define I2C_BUFFER_LEN 765

static kal_uint16 sr846_table_write_cmos_sensor(
					kal_uint16 *para, kal_uint32 len)
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

		if ((I2C_BUFFER_LEN - tosend) < 4 ||
			len == IDX ||
			addr != addr_last) {
			iBurstWriteReg_multi(puSendCmd, tosend,
				imgsensor.i2c_write_id,
				4, imgsensor_info.i2c_speed);

			tosend = 0;
		}
	}
	return 0;
}
#endif

static kal_uint16 read_cmos_sensor(kal_uint32 addr)
{
	kal_uint16 get_byte = 0;
	char pu_send_cmd[2] = {(char)(addr >> 8), (char)(addr & 0xFF) };

	iReadRegI2C(pu_send_cmd, 2, (u8 *)&get_byte, 1, imgsensor.i2c_write_id);

	return get_byte;
}

static void write_cmos_sensor(kal_uint32 addr, kal_uint32 para)
{
	char pu_send_cmd[4] = {(char)(addr >> 8),
		(char)(addr & 0xFF), (char)(para >> 8), (char)(para & 0xFF)};

	iWriteRegI2C(pu_send_cmd, 4, imgsensor.i2c_write_id);
}

static void write_cmos_sensor_8(kal_uint32 addr, kal_uint32 para)
{
	char pu_send_cmd[4] = {(char)(addr >> 8),
		(char)(addr & 0xFF), (char)(para & 0xFF)};

	iWriteRegI2C(pu_send_cmd, 3, imgsensor.i2c_write_id);
}

static void set_dummy(void)
{
	LOG_INF("dummyline = %d, dummypixels = %d\n",
		imgsensor.dummy_line, imgsensor.dummy_pixel);
	write_cmos_sensor(0x0006, imgsensor.frame_length & 0xFFFF);
	write_cmos_sensor(0x0008, imgsensor.line_length & 0xFFFF);

}	/*	set_dummy  */

static kal_uint32 return_sensor_id(void)
{
	return ((read_cmos_sensor(0x0F17) << 8) | read_cmos_sensor(0x0F16));
}


static void set_max_framerate(UINT16 framerate, kal_bool min_framelength_en)
{
	kal_uint32 frame_length = imgsensor.frame_length;

	frame_length = imgsensor.pclk / framerate * 10 / imgsensor.line_length;
	spin_lock(&imgsensor_drv_lock);
	imgsensor.frame_length = (frame_length > imgsensor.min_frame_length) ?
			frame_length : imgsensor.min_frame_length;
	imgsensor.dummy_line = imgsensor.frame_length -
		imgsensor.min_frame_length;

	if (imgsensor.frame_length > imgsensor_info.max_frame_length) {
		imgsensor.frame_length = imgsensor_info.max_frame_length;
		imgsensor.dummy_line = imgsensor.frame_length -
			imgsensor.min_frame_length;
	}
	if (min_framelength_en)
		imgsensor.min_frame_length = imgsensor.frame_length;

	spin_unlock(&imgsensor_drv_lock);
	set_dummy();
}	/*	set_max_framerate  */

static void write_shutter(kal_uint32 shutter)
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

	shutter = (shutter < imgsensor_info.min_shutter) ?
		imgsensor_info.min_shutter : shutter;
	shutter = (shutter >
		(imgsensor_info.max_frame_length - imgsensor_info.margin)) ?
		(imgsensor_info.max_frame_length - imgsensor_info.margin) :
		shutter;
	if (imgsensor.autoflicker_en) {
		realtime_fps = imgsensor.pclk * 10 /
			(imgsensor.line_length * imgsensor.frame_length);
		if (realtime_fps >= 297 && realtime_fps <= 305)
			set_max_framerate(296, 0);
		else if (realtime_fps >= 147 && realtime_fps <= 150)
			set_max_framerate(146, 0);
		else
			write_cmos_sensor(0x0006, imgsensor.frame_length);

	} else {
		// Extend frame length

		// ADD ODIN
		realtime_fps = imgsensor.pclk * 10 /
			(imgsensor.line_length * imgsensor.frame_length);
		if (realtime_fps > 300 && realtime_fps < 320)
			set_max_framerate(300, 0);
		// ADD END
		write_cmos_sensor(0x0006, imgsensor.frame_length);
	}

	// Update Shutter
	write_cmos_sensor_8(0x0073, ((shutter & 0xFF0000) >> 16));
	write_cmos_sensor(0x0074, shutter & 0x00FFFF);
	LOG_INF("shutter =%d, framelength =%d",
		shutter, imgsensor.frame_length);
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
static void set_shutter(kal_uint32 shutter)
{
	unsigned long flags;

	LOG_INF("set_shutter");
	spin_lock_irqsave(&imgsensor_drv_lock, flags);
	imgsensor.shutter = shutter;
	spin_unlock_irqrestore(&imgsensor_drv_lock, flags);

	write_shutter(shutter);
}	/*	set_shutter */

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

	if (gain < BASEGAIN)
		gain = BASEGAIN;
	else if (gain > 16 * BASEGAIN)
		gain = 16 * BASEGAIN;

	reg_gain = gain2reg(gain);
	spin_lock(&imgsensor_drv_lock);
	imgsensor.gain = reg_gain;
	spin_unlock(&imgsensor_drv_lock);
	LOG_INF("gain = %d , reg_gain = 0x%x\n ", gain, reg_gain);

	write_cmos_sensor_8(0x0077, reg_gain);

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

#if MULTI_WRITE
kal_uint16 addr_data_pair_init_sr846[] = {
	0x0E16, 0x0100,
	0x5000, 0x120B,
	0x5002, 0x120A,
	0x5004, 0x1209,
	0x5006, 0x1208,
	0x5008, 0x1207,
	0x500A, 0x1206,
	0x500C, 0x1205,
	0x500E, 0x1204,
	0x5010, 0x93C2,
	0x5012, 0x00C1,
	0x5014, 0x24F3,
	0x5016, 0x425E,
	0x5018, 0x00C2,
	0x501A, 0xC35E,
	0x501C, 0x425F,
	0x501E, 0x82A0,
	0x5020, 0xDF4E,
	0x5022, 0x4EC2,
	0x5024, 0x00C2,
	0x5026, 0x934F,
	0x5028, 0x24DF,
	0x502A, 0x4217,
	0x502C, 0x7316,
	0x502E, 0x4218,
	0x5030, 0x7318,
	0x5032, 0x403E,
	0x5034, 0x0AA0,
	0x5036, 0xB3E2,
	0x5038, 0x00C2,
	0x503A, 0x24CB,
	0x503C, 0x9382,
	0x503E, 0x731C,
	0x5040, 0x2003,
	0x5042, 0x93CE,
	0x5044, 0x0000,
	0x5046, 0x23FA,
	0x5048, 0x4E6F,
	0x504A, 0xF21F,
	0x504C, 0x731C,
	0x504E, 0x23FC,
	0x5050, 0x42D2,
	0x5052, 0x0AA0,
	0x5054, 0x0A80,
	0x5056, 0x421A,
	0x5058, 0x7300,
	0x505A, 0x421B,
	0x505C, 0x7302,
	0x505E, 0x421E,
	0x5060, 0x832A,
	0x5062, 0x430F,
	0x5064, 0x421C,
	0x5066, 0x82CE,
	0x5068, 0x421D,
	0x506A, 0x82D0,
	0x506C, 0x8E0C,
	0x506E, 0x7F0D,
	0x5070, 0x4C0E,
	0x5072, 0x4D0F,
	0x5074, 0x4A0C,
	0x5076, 0x4B0D,
	0x5078, 0x8E0C,
	0x507A, 0x7F0D,
	0x507C, 0x2CA6,
	0x507E, 0x4A09,
	0x5080, 0x9339,
	0x5082, 0x3471,
	0x5084, 0x4306,
	0x5086, 0x5039,
	0x5088, 0xFFFD,
	0x508A, 0x43A2,
	0x508C, 0x7316,
	0x508E, 0x4382,
	0x5090, 0x7318,
	0x5092, 0x4214,
	0x5094, 0x7544,
	0x5096, 0x4215,
	0x5098, 0x7546,
	0x509A, 0x4292,
	0x509C, 0x8318,
	0x509E, 0x7544,
	0x50A0, 0x4292,
	0x50A2, 0x831A,
	0x50A4, 0x7546,
	0x50A6, 0x4A0E,
	0x50A8, 0x4B0F,
	0x50AA, 0x821E,
	0x50AC, 0x832C,
	0x50AE, 0x721F,
	0x50B0, 0x832E,
	0x50B2, 0x2C08,
	0x50B4, 0x4382,
	0x50B6, 0x7540,
	0x50B8, 0x4382,
	0x50BA, 0x7542,
	0x50BC, 0x4216,
	0x50BE, 0x82CE,
	0x50C0, 0x8216,
	0x50C2, 0x832C,
	0x50C4, 0x12B0,
	0x50C6, 0xFE78,
	0x50C8, 0x0B00,
	0x50CA, 0x7304,
	0x50CC, 0x03AF,
	0x50CE, 0x4392,
	0x50D0, 0x731C,
	0x50D2, 0x4482,
	0x50D4, 0x7544,
	0x50D6, 0x4582,
	0x50D8, 0x7546,
	0x50DA, 0x490A,
	0x50DC, 0x4A0B,
	0x50DE, 0x5B0B,
	0x50E0, 0x7B0B,
	0x50E2, 0xE33B,
	0x50E4, 0x421E,
	0x50E6, 0x8320,
	0x50E8, 0x421F,
	0x50EA, 0x8322,
	0x50EC, 0x5A0E,
	0x50EE, 0x6B0F,
	0x50F0, 0x4E82,
	0x50F2, 0x8320,
	0x50F4, 0x4F82,
	0x50F6, 0x8322,
	0x50F8, 0x5226,
	0x50FA, 0x460C,
	0x50FC, 0x4C0D,
	0x50FE, 0x5D0D,
	0x5100, 0x7D0D,
	0x5102, 0xE33D,
	0x5104, 0x8226,
	0x5106, 0x8C0E,
	0x5108, 0x7D0F,
	0x510A, 0x2C04,
	0x510C, 0x4C82,
	0x510E, 0x8320,
	0x5110, 0x4D82,
	0x5112, 0x8322,
	0x5114, 0x4292,
	0x5116, 0x8320,
	0x5118, 0x7540,
	0x511A, 0x4292,
	0x511C, 0x8322,
	0x511E, 0x7542,
	0x5120, 0x5A07,
	0x5122, 0x6B08,
	0x5124, 0x421C,
	0x5126, 0x831C,
	0x5128, 0x430D,
	0x512A, 0x470E,
	0x512C, 0x480F,
	0x512E, 0x8C0E,
	0x5130, 0x7D0F,
	0x5132, 0x2C02,
	0x5134, 0x4C07,
	0x5136, 0x4D08,
	0x5138, 0x4782,
	0x513A, 0x7316,
	0x513C, 0x4882,
	0x513E, 0x7318,
	0x5140, 0x460F,
	0x5142, 0x890F,
	0x5144, 0x421E,
	0x5146, 0x7324,
	0x5148, 0x8F0E,
	0x514A, 0x922E,
	0x514C, 0x3401,
	0x514E, 0x422E,
	0x5150, 0x4E82,
	0x5152, 0x7324,
	0x5154, 0x9316,
	0x5156, 0x3804,
	0x5158, 0x4682,
	0x515A, 0x7334,
	0x515C, 0x0F00,
	0x515E, 0x7300,
	0x5160, 0xD3D2,
	0x5162, 0x00C2,
	0x5164, 0x3C4D,
	0x5166, 0x9329,
	0x5168, 0x3422,
	0x516A, 0x490E,
	0x516C, 0x4E0F,
	0x516E, 0x5F0F,
	0x5170, 0x7F0F,
	0x5172, 0xE33F,
	0x5174, 0x5E82,
	0x5176, 0x7316,
	0x5178, 0x6F82,
	0x517A, 0x7318,
	0x517C, 0x521E,
	0x517E, 0x8320,
	0x5180, 0x621F,
	0x5182, 0x8322,
	0x5184, 0x4E82,
	0x5186, 0x7540,
	0x5188, 0x4F82,
	0x518A, 0x7542,
	0x518C, 0x421E,
	0x518E, 0x7300,
	0x5190, 0x421F,
	0x5192, 0x7302,
	0x5194, 0x803E,
	0x5196, 0x0003,
	0x5198, 0x730F,
	0x519A, 0x2FF8,
	0x519C, 0x12B0,
	0x519E, 0xFE78,
	0x51A0, 0x0B00,
	0x51A2, 0x7300,
	0x51A4, 0x0002,
	0x51A6, 0x0B00,
	0x51A8, 0x7304,
	0x51AA, 0x01F4,
	0x51AC, 0x3FD9,
	0x51AE, 0x0B00,
	0x51B0, 0x7304,
	0x51B2, 0x03AF,
	0x51B4, 0x4392,
	0x51B6, 0x731C,
	0x51B8, 0x4292,
	0x51BA, 0x8320,
	0x51BC, 0x7540,
	0x51BE, 0x4292,
	0x51C0, 0x8322,
	0x51C2, 0x7542,
	0x51C4, 0x12B0,
	0x51C6, 0xFE78,
	0x51C8, 0x3FCB,
	0x51CA, 0x4A09,
	0x51CC, 0x8219,
	0x51CE, 0x82CE,
	0x51D0, 0x3F57,
	0x51D2, 0x4E6F,
	0x51D4, 0xF21F,
	0x51D6, 0x731C,
	0x51D8, 0x23FC,
	0x51DA, 0x9382,
	0x51DC, 0x731C,
	0x51DE, 0x2003,
	0x51E0, 0x93CE,
	0x51E2, 0x0000,
	0x51E4, 0x23FA,
	0x51E6, 0x3F34,
	0x51E8, 0x9382,
	0x51EA, 0x730C,
	0x51EC, 0x23B9,
	0x51EE, 0x42D2,
	0x51F0, 0x0AA0,
	0x51F2, 0x0A80,
	0x51F4, 0x9382,
	0x51F6, 0x730C,
	0x51F8, 0x27FA,
	0x51FA, 0x3FB2,
	0x51FC, 0x0900,
	0x51FE, 0x732C,
	0x5200, 0x425F,
	0x5202, 0x0788,
	0x5204, 0x4292,
	0x5206, 0x7544,
	0x5208, 0x8318,
	0x520A, 0x4292,
	0x520C, 0x7546,
	0x520E, 0x831A,
	0x5210, 0x42D2,
	0x5212, 0x0AA0,
	0x5214, 0x0A80,
	0x5216, 0x4134,
	0x5218, 0x4135,
	0x521A, 0x4136,
	0x521C, 0x4137,
	0x521E, 0x4138,
	0x5220, 0x4139,
	0x5222, 0x413A,
	0x5224, 0x413B,
	0x5226, 0x4130,
	0x2000, 0x98EB,
	0x2002, 0x00FF,
	0x2004, 0x0007,
	0x2006, 0x3FFF,
	0x2008, 0x3FFF,
	0x200A, 0xC216,
	0x200C, 0x1292,
	0x200E, 0xC01A,
	0x2010, 0x403D,
	0x2012, 0x000E,
	0x2014, 0x403E,
	0x2016, 0x0B80,
	0x2018, 0x403F,
	0x201A, 0x82AE,
	0x201C, 0x1292,
	0x201E, 0xC00C,
	0x2020, 0x4130,
	0x2022, 0x43E2,
	0x2024, 0x0180,
	0x2026, 0x4130,
	0x2028, 0x7400,
	0x202A, 0x5000,
	0x202C, 0x0253,
	0x202E, 0x0253,
	0x2030, 0x0851,
	0x2032, 0x00D1,
	0x2034, 0x0009,
	0x2036, 0x5020,
	0x2038, 0x000B,
	0x203A, 0x0002,
	0x203C, 0x0044,
	0x203E, 0x0016,
	0x2040, 0x1792,
	0x2042, 0x7002,
	0x2044, 0x154F,
	0x2046, 0x00D5,
	0x2048, 0x000B,
	0x204A, 0x0019,
	0x204C, 0x1698,
	0x204E, 0x000E,
	0x2050, 0x099A,
	0x2052, 0x0058,
	0x2054, 0x7000,
	0x2056, 0x1799,
	0x2058, 0x0310,
	0x205A, 0x03C3,
	0x205C, 0x004C,
	0x205E, 0x064A,
	0x2060, 0x0001,
	0x2062, 0x0007,
	0x2064, 0x0BC7,
	0x2066, 0x0055,
	0x2068, 0x7000,
	0x206A, 0x1550,
	0x206C, 0x158A,
	0x206E, 0x0004,
	0x2070, 0x1488,
	0x2072, 0x7010,
	0x2074, 0x1508,
	0x2076, 0x0004,
	0x2078, 0x0016,
	0x207A, 0x03D5,
	0x207C, 0x0055,
	0x207E, 0x08CA,
	0x2080, 0x2019,
	0x2082, 0x0007,
	0x2084, 0x7057,
	0x2086, 0x0FC7,
	0x2088, 0x5041,
	0x208A, 0x12C8,
	0x208C, 0x5060,
	0x208E, 0x5080,
	0x2090, 0x2084,
	0x2092, 0x12C8,
	0x2094, 0x7800,
	0x2096, 0x0802,
	0x2098, 0x040F,
	0x209A, 0x1007,
	0x209C, 0x0803,
	0x209E, 0x080B,
	0x20A0, 0x3803,
	0x20A2, 0x0807,
	0x20A4, 0x0404,
	0x20A6, 0x0400,
	0x20A8, 0xFFFF,
	0x20AA, 0x1292,
	0x20AC, 0xC006,
	0x20AE, 0x421F,
	0x20B0, 0x0710,
	0x20B2, 0x523F,
	0x20B4, 0x4F82,
	0x20B6, 0x831C,
	0x20B8, 0x93C2,
	0x20BA, 0x829F,
	0x20BC, 0x241E,
	0x20BE, 0x403D,
	0x20C0, 0xFFFE,
	0x20C2, 0x40B2,
	0x20C4, 0xEC78,
	0x20C6, 0x8324,
	0x20C8, 0x40B2,
	0x20CA, 0xEC78,
	0x20CC, 0x8326,
	0x20CE, 0x40B2,
	0x20D0, 0xEC78,
	0x20D2, 0x8328,
	0x20D4, 0x425F,
	0x20D6, 0x008C,
	0x20D8, 0x934F,
	0x20DA, 0x240E,
	0x20DC, 0x4D0E,
	0x20DE, 0x503E,
	0x20E0, 0xFFD8,
	0x20E2, 0x4E82,
	0x20E4, 0x8324,
	0x20E6, 0xF36F,
	0x20E8, 0x2407,
	0x20EA, 0x4E0F,
	0x20EC, 0x5D0F,
	0x20EE, 0x4F82,
	0x20F0, 0x8326,
	0x20F2, 0x5D0F,
	0x20F4, 0x4F82,
	0x20F6, 0x8328,
	0x20F8, 0x4130,
	0x20FA, 0x432D,
	0x20FC, 0x3FE2,
	0x20FE, 0x120B,
	0x2100, 0x120A,
	0x2102, 0x1209,
	0x2104, 0x1208,
	0x2106, 0x1207,
	0x2108, 0x4292,
	0x210A, 0x7540,
	0x210C, 0x832C,
	0x210E, 0x4292,
	0x2110, 0x7542,
	0x2112, 0x832E,
	0x2114, 0x93C2,
	0x2116, 0x00C1,
	0x2118, 0x2405,
	0x211A, 0x42D2,
	0x211C, 0x8330,
	0x211E, 0x82A0,
	0x2120, 0x4392,
	0x2122, 0x82D4,
	0x2124, 0x1292,
	0x2126, 0xC046,
	0x2128, 0x4292,
	0x212A, 0x7324,
	0x212C, 0x832A,
	0x212E, 0x93C2,
	0x2130, 0x00C1,
	0x2132, 0x244E,
	0x2134, 0x4218,
	0x2136, 0x7316,
	0x2138, 0x4219,
	0x213A, 0x7318,
	0x213C, 0x421F,
	0x213E, 0x0710,
	0x2140, 0x4F0E,
	0x2142, 0x430F,
	0x2144, 0x480A,
	0x2146, 0x490B,
	0x2148, 0x8E0A,
	0x214A, 0x7F0B,
	0x214C, 0x4A0E,
	0x214E, 0x4B0F,
	0x2150, 0x803E,
	0x2152, 0x0102,
	0x2154, 0x730F,
	0x2156, 0x283A,
	0x2158, 0x403F,
	0x215A, 0x0101,
	0x215C, 0x832F,
	0x215E, 0x43D2,
	0x2160, 0x01B3,
	0x2162, 0x4F82,
	0x2164, 0x7324,
	0x2166, 0x4292,
	0x2168, 0x7540,
	0x216A, 0x8320,
	0x216C, 0x4292,
	0x216E, 0x7542,
	0x2170, 0x8322,
	0x2172, 0x4347,
	0x2174, 0x823F,
	0x2176, 0x4F0C,
	0x2178, 0x430D,
	0x217A, 0x421E,
	0x217C, 0x8320,
	0x217E, 0x421F,
	0x2180, 0x8322,
	0x2182, 0x5E0C,
	0x2184, 0x6F0D,
	0x2186, 0x8A0C,
	0x2188, 0x7B0D,
	0x218A, 0x2801,
	0x218C, 0x4357,
	0x218E, 0x47C2,
	0x2190, 0x8330,
	0x2192, 0x93C2,
	0x2194, 0x829A,
	0x2196, 0x201C,
	0x2198, 0x93C2,
	0x219A, 0x82A0,
	0x219C, 0x2404,
	0x219E, 0x43B2,
	0x21A0, 0x7540,
	0x21A2, 0x43B2,
	0x21A4, 0x7542,
	0x21A6, 0x93C2,
	0x21A8, 0x8330,
	0x21AA, 0x2412,
	0x21AC, 0x532E,
	0x21AE, 0x630F,
	0x21B0, 0x4E82,
	0x21B2, 0x8320,
	0x21B4, 0x4F82,
	0x21B6, 0x8322,
	0x21B8, 0x480C,
	0x21BA, 0x490D,
	0x21BC, 0x8E0C,
	0x21BE, 0x7F0D,
	0x21C0, 0x2C07,
	0x21C2, 0x4882,
	0x21C4, 0x8320,
	0x21C6, 0x4982,
	0x21C8, 0x8322,
	0x21CA, 0x3C02,
	0x21CC, 0x4A0F,
	0x21CE, 0x3FC6,
	0x21D0, 0x4137,
	0x21D2, 0x4138,
	0x21D4, 0x4139,
	0x21D6, 0x413A,
	0x21D8, 0x413B,
	0x21DA, 0x4130,
	0x21DC, 0x421F,
	0x21DE, 0x7100,
	0x21E0, 0x4F0E,
	0x21E2, 0x503E,
	0x21E4, 0xFFD8,
	0x21E6, 0x4E82,
	0x21E8, 0x7A04,
	0x21EA, 0x421E,
	0x21EC, 0x8324,
	0x21EE, 0x5F0E,
	0x21F0, 0x4E82,
	0x21F2, 0x7A06,
	0x21F4, 0x0B00,
	0x21F6, 0x7304,
	0x21F8, 0x0050,
	0x21FA, 0x40B2,
	0x21FC, 0xD081,
	0x21FE, 0x0B88,
	0x2200, 0x421E,
	0x2202, 0x8326,
	0x2204, 0x5F0E,
	0x2206, 0x4E82,
	0x2208, 0x7A0E,
	0x220A, 0x521F,
	0x220C, 0x8328,
	0x220E, 0x4F82,
	0x2210, 0x7A10,
	0x2212, 0x0B00,
	0x2214, 0x7304,
	0x2216, 0x007A,
	0x2218, 0x40B2,
	0x221A, 0x0081,
	0x221C, 0x0B88,
	0x221E, 0x4392,
	0x2220, 0x7A0A,
	0x2222, 0x0800,
	0x2224, 0x7A0C,
	0x2226, 0x0B00,
	0x2228, 0x7304,
	0x222A, 0x022B,
	0x222C, 0x40B2,
	0x222E, 0xD081,
	0x2230, 0x0B88,
	0x2232, 0x0B00,
	0x2234, 0x7304,
	0x2236, 0x0255,
	0x2238, 0x40B2,
	0x223A, 0x0081,
	0x223C, 0x0B88,
	0x223E, 0x9382,
	0x2240, 0x7112,
	0x2242, 0x2402,
	0x2244, 0x4392,
	0x2246, 0x760E,
	0x2248, 0x93D2,
	0x224A, 0x01B2,
	0x224C, 0x2003,
	0x224E, 0x42D2,
	0x2250, 0x0AA0,
	0x2252, 0x0A80,
	0x2254, 0x4130,
	0x2256, 0xF0B2,
	0x2258, 0xFFBF,
	0x225A, 0x2004,
	0x225C, 0x9382,
	0x225E, 0xC314,
	0x2260, 0x2406,
	0x2262, 0x12B0,
	0x2264, 0xC90A,
	0x2266, 0x40B2,
	0x2268, 0xC6CE,
	0x226A, 0x8246,
	0x226C, 0x3C02,
	0x226E, 0x12B0,
	0x2270, 0xCAB0,
	0x2272, 0x42A2,
	0x2274, 0x7324,
	0x2276, 0x4130,
	0x2278, 0x403E,
	0x227A, 0x00C2,
	0x227C, 0x421F,
	0x227E, 0x7314,
	0x2280, 0xF07F,
	0x2282, 0x000C,
	0x2284, 0x5F4F,
	0x2286, 0x5F4F,
	0x2288, 0xDFCE,
	0x228A, 0x0000,
	0x228C, 0xF0FE,
	0x228E, 0x000F,
	0x2290, 0x0000,
	0x2292, 0x4130,
	0x2294, 0x40B2,
	0x2296, 0x3FFF,
	0x2298, 0x0A8A,
	0x229A, 0x4292,
	0x229C, 0x826E,
	0x229E, 0x0A9C,
	0x22A0, 0x43C2,
	0x22A2, 0x8330,
	0x22A4, 0x1292,
	0x22A6, 0xC018,
	0x22A8, 0x93C2,
	0x22AA, 0x0AA0,
	0x22AC, 0x27FD,
	0x22AE, 0x4292,
	0x22B0, 0x0A9C,
	0x22B2, 0x826E,
	0x22B4, 0x4292,
	0x22B6, 0x0A8A,
	0x22B8, 0x2008,
	0x22BA, 0x43D2,
	0x22BC, 0x0A80,
	0x22BE, 0x4130,
	0x23FE, 0xC056,
	0x3230, 0xFE94,
	0x3232, 0xFC0C,
	0x3236, 0xFC22,
	0x323A, 0xFDDC,
	0x323C, 0xFCAA,
	0x323E, 0xFCFE,
	0x3246, 0xE000,
	0x3248, 0xCDB0,
	0x324E, 0xFE56,
	0x326A, 0x8302,
	0x326C, 0x830A,
	0x326E, 0x0000,
	0x32CA, 0xFC28,
	0x32CC, 0xC3BC,
	0x32CE, 0xC34C,
	0x32D0, 0xC35A,
	0x32D2, 0xC368,
	0x32D4, 0xC376,
	0x32D6, 0xC3C2,
	0x32D8, 0xC3E6,
	0x32DA, 0x0003,
	0x32DC, 0x0003,
	0x32DE, 0x00C7,
	0x32E0, 0x0031,
	0x32E2, 0x0031,
	0x32E4, 0x0031,
	0x32E6, 0xFC28,
	0x32E8, 0xC3BC,
	0x32EA, 0xC384,
	0x32EC, 0xC392,
	0x32EE, 0xC3A0,
	0x32F0, 0xC3AE,
	0x32F2, 0xC3C4,
	0x32F4, 0xC3E6,
	0x32F6, 0x0003,
	0x32F8, 0x0003,
	0x32FA, 0x00C7,
	0x32FC, 0x0031,
	0x32FE, 0x0031,
	0x3300, 0x0031,
	0x3302, 0x82CA,
	0x3304, 0xC164,
	0x3306, 0x82E6,
	0x3308, 0xC19C,
	0x330A, 0x001F,
	0x330C, 0x001A,
	0x330E, 0x0034,
	0x3310, 0x0000,
	0x3312, 0x0000,
	0x3314, 0xFC96,
	0x3316, 0xC3D8,
	0x0A20, 0x0000,
	0x0E04, 0x0012,
	0x002E, 0x1111,
	0x0032, 0x1111,
	0x0022, 0x0008,
	0x0026, 0x0040,
	0x0028, 0x0017,
	0x002C, 0x09CF,
	0x005C, 0x2101,
	0x0006, 0x09BC,
	0x0008, 0x0ED8,
	0x000E, 0x0100,
	0x000C, 0x0022,
	0x0A22, 0x0000,
	0x0A24, 0x0000,
	0x0804, 0x0000,
	0x0A12, 0x0CC0,
	0x0A14, 0x0990,
	0x0074, 0x09B6,
	0x0076, 0x0000,
	0x051E, 0x0000,
	0x0200, 0x0400,
	0x0A1A, 0x0C00,
	0x0A0C, 0x0010,
	0x0A1E, 0x0CCF,
	0x0402, 0x0110,
	0x0404, 0x00F4,
	0x0408, 0x0000,
	0x0410, 0x008D,
	0x0412, 0x011A,
	0x0414, 0x864C,
	0x021C, 0x0001,
	0x0C00, 0x9150,
	0x0C06, 0x0021,
	0x0C10, 0x0040,
	0x0C12, 0x0040,
	0x0C14, 0x0040,
	0x0C16, 0x0040,
	0x0A02, 0x0000,
	0x0A04, 0x014A,
	0x0418, 0x0000,
	0x0128, 0x0025,
	0x012A, 0xFFFF,
	0x0120, 0x0041,
	0x0122, 0x0371,
	0x012C, 0x001E,
	0x012E, 0xFFFF,
	0x0124, 0x003B,
	0x0126, 0x0373,
	0x0746, 0x0050,
	0x0748, 0x01D5,
	0x074A, 0x022B,
	0x074C, 0x03B0,
	0x0756, 0x043F,
	0x0758, 0x3F1D,
	0x0B02, 0xE04D,
	0x0B10, 0x6821,
	0x0B12, 0x0030,
	0x0B14, 0x0001,
	0x0A0A, 0x38FD,
	0x0A1C, 0x0000,
	0x0900, 0x0300,
	0x0902, 0xC319,
	0x0914, 0xC109,
	0x0916, 0x061A,
	0x0918, 0x0407,
	0x091A, 0x0A0B,
	0x091C, 0x0E08,
	0x091E, 0x0A00,
	0x090C, 0x03FA,
	0x090E, 0x0030,
	0x0954, 0x0089,
	0x0956, 0x0000,
	0x0958, 0xC980,
	0x095A, 0x6180,
	0x0710, 0x09B0,
	0x0040, 0x0200,
	0x0042, 0x0100,
	0x0D04, 0x0000,
	0x0F08, 0x2F04,
	0x0F30, 0x001F,
	0x0F36, 0x001F,
	0x0F04, 0x3A00,
	0x0F32, 0x0253,
	0x0F38, 0x059D,
	0x0F2A, 0x4124,
	0x006A, 0x0100,
	0x004C, 0x0100
};
#endif

static void sensor_init(void)
{
#if MULTI_WRITE
	sr846_table_write_cmos_sensor(
		addr_data_pair_init_sr846,
		sizeof(addr_data_pair_init_sr846) /
		sizeof(kal_uint16));
#else
	write_cmos_sensor(0x0E16, 0x0100);
	write_cmos_sensor(0x5000, 0x120B);
	write_cmos_sensor(0x5002, 0x120A);
	write_cmos_sensor(0x5004, 0x1209);
	write_cmos_sensor(0x5006, 0x1208);
	write_cmos_sensor(0x5008, 0x1207);
	write_cmos_sensor(0x500A, 0x1206);
	write_cmos_sensor(0x500C, 0x1205);
	write_cmos_sensor(0x500E, 0x1204);
	write_cmos_sensor(0x5010, 0x93C2);
	write_cmos_sensor(0x5012, 0x00C1);
	write_cmos_sensor(0x5014, 0x24F3);
	write_cmos_sensor(0x5016, 0x425E);
	write_cmos_sensor(0x5018, 0x00C2);
	write_cmos_sensor(0x501A, 0xC35E);
	write_cmos_sensor(0x501C, 0x425F);
	write_cmos_sensor(0x501E, 0x82A0);
	write_cmos_sensor(0x5020, 0xDF4E);
	write_cmos_sensor(0x5022, 0x4EC2);
	write_cmos_sensor(0x5024, 0x00C2);
	write_cmos_sensor(0x5026, 0x934F);
	write_cmos_sensor(0x5028, 0x24DF);
	write_cmos_sensor(0x502A, 0x4217);
	write_cmos_sensor(0x502C, 0x7316);
	write_cmos_sensor(0x502E, 0x4218);
	write_cmos_sensor(0x5030, 0x7318);
	write_cmos_sensor(0x5032, 0x403E);
	write_cmos_sensor(0x5034, 0x0AA0);
	write_cmos_sensor(0x5036, 0xB3E2);
	write_cmos_sensor(0x5038, 0x00C2);
	write_cmos_sensor(0x503A, 0x24CB);
	write_cmos_sensor(0x503C, 0x9382);
	write_cmos_sensor(0x503E, 0x731C);
	write_cmos_sensor(0x5040, 0x2003);
	write_cmos_sensor(0x5042, 0x93CE);
	write_cmos_sensor(0x5044, 0x0000);
	write_cmos_sensor(0x5046, 0x23FA);
	write_cmos_sensor(0x5048, 0x4E6F);
	write_cmos_sensor(0x504A, 0xF21F);
	write_cmos_sensor(0x504C, 0x731C);
	write_cmos_sensor(0x504E, 0x23FC);
	write_cmos_sensor(0x5050, 0x42D2);
	write_cmos_sensor(0x5052, 0x0AA0);
	write_cmos_sensor(0x5054, 0x0A80);
	write_cmos_sensor(0x5056, 0x421A);
	write_cmos_sensor(0x5058, 0x7300);
	write_cmos_sensor(0x505A, 0x421B);
	write_cmos_sensor(0x505C, 0x7302);
	write_cmos_sensor(0x505E, 0x421E);
	write_cmos_sensor(0x5060, 0x832A);
	write_cmos_sensor(0x5062, 0x430F);
	write_cmos_sensor(0x5064, 0x421C);
	write_cmos_sensor(0x5066, 0x82CE);
	write_cmos_sensor(0x5068, 0x421D);
	write_cmos_sensor(0x506A, 0x82D0);
	write_cmos_sensor(0x506C, 0x8E0C);
	write_cmos_sensor(0x506E, 0x7F0D);
	write_cmos_sensor(0x5070, 0x4C0E);
	write_cmos_sensor(0x5072, 0x4D0F);
	write_cmos_sensor(0x5074, 0x4A0C);
	write_cmos_sensor(0x5076, 0x4B0D);
	write_cmos_sensor(0x5078, 0x8E0C);
	write_cmos_sensor(0x507A, 0x7F0D);
	write_cmos_sensor(0x507C, 0x2CA6);
	write_cmos_sensor(0x507E, 0x4A09);
	write_cmos_sensor(0x5080, 0x9339);
	write_cmos_sensor(0x5082, 0x3471);
	write_cmos_sensor(0x5084, 0x4306);
	write_cmos_sensor(0x5086, 0x5039);
	write_cmos_sensor(0x5088, 0xFFFD);
	write_cmos_sensor(0x508A, 0x43A2);
	write_cmos_sensor(0x508C, 0x7316);
	write_cmos_sensor(0x508E, 0x4382);
	write_cmos_sensor(0x5090, 0x7318);
	write_cmos_sensor(0x5092, 0x4214);
	write_cmos_sensor(0x5094, 0x7544);
	write_cmos_sensor(0x5096, 0x4215);
	write_cmos_sensor(0x5098, 0x7546);
	write_cmos_sensor(0x509A, 0x4292);
	write_cmos_sensor(0x509C, 0x8318);
	write_cmos_sensor(0x509E, 0x7544);
	write_cmos_sensor(0x50A0, 0x4292);
	write_cmos_sensor(0x50A2, 0x831A);
	write_cmos_sensor(0x50A4, 0x7546);
	write_cmos_sensor(0x50A6, 0x4A0E);
	write_cmos_sensor(0x50A8, 0x4B0F);
	write_cmos_sensor(0x50AA, 0x821E);
	write_cmos_sensor(0x50AC, 0x832C);
	write_cmos_sensor(0x50AE, 0x721F);
	write_cmos_sensor(0x50B0, 0x832E);
	write_cmos_sensor(0x50B2, 0x2C08);
	write_cmos_sensor(0x50B4, 0x4382);
	write_cmos_sensor(0x50B6, 0x7540);
	write_cmos_sensor(0x50B8, 0x4382);
	write_cmos_sensor(0x50BA, 0x7542);
	write_cmos_sensor(0x50BC, 0x4216);
	write_cmos_sensor(0x50BE, 0x82CE);
	write_cmos_sensor(0x50C0, 0x8216);
	write_cmos_sensor(0x50C2, 0x832C);
	write_cmos_sensor(0x50C4, 0x12B0);
	write_cmos_sensor(0x50C6, 0xFE78);
	write_cmos_sensor(0x50C8, 0x0B00);
	write_cmos_sensor(0x50CA, 0x7304);
	write_cmos_sensor(0x50CC, 0x03AF);
	write_cmos_sensor(0x50CE, 0x4392);
	write_cmos_sensor(0x50D0, 0x731C);
	write_cmos_sensor(0x50D2, 0x4482);
	write_cmos_sensor(0x50D4, 0x7544);
	write_cmos_sensor(0x50D6, 0x4582);
	write_cmos_sensor(0x50D8, 0x7546);
	write_cmos_sensor(0x50DA, 0x490A);
	write_cmos_sensor(0x50DC, 0x4A0B);
	write_cmos_sensor(0x50DE, 0x5B0B);
	write_cmos_sensor(0x50E0, 0x7B0B);
	write_cmos_sensor(0x50E2, 0xE33B);
	write_cmos_sensor(0x50E4, 0x421E);
	write_cmos_sensor(0x50E6, 0x8320);
	write_cmos_sensor(0x50E8, 0x421F);
	write_cmos_sensor(0x50EA, 0x8322);
	write_cmos_sensor(0x50EC, 0x5A0E);
	write_cmos_sensor(0x50EE, 0x6B0F);
	write_cmos_sensor(0x50F0, 0x4E82);
	write_cmos_sensor(0x50F2, 0x8320);
	write_cmos_sensor(0x50F4, 0x4F82);
	write_cmos_sensor(0x50F6, 0x8322);
	write_cmos_sensor(0x50F8, 0x5226);
	write_cmos_sensor(0x50FA, 0x460C);
	write_cmos_sensor(0x50FC, 0x4C0D);
	write_cmos_sensor(0x50FE, 0x5D0D);
	write_cmos_sensor(0x5100, 0x7D0D);
	write_cmos_sensor(0x5102, 0xE33D);
	write_cmos_sensor(0x5104, 0x8226);
	write_cmos_sensor(0x5106, 0x8C0E);
	write_cmos_sensor(0x5108, 0x7D0F);
	write_cmos_sensor(0x510A, 0x2C04);
	write_cmos_sensor(0x510C, 0x4C82);
	write_cmos_sensor(0x510E, 0x8320);
	write_cmos_sensor(0x5110, 0x4D82);
	write_cmos_sensor(0x5112, 0x8322);
	write_cmos_sensor(0x5114, 0x4292);
	write_cmos_sensor(0x5116, 0x8320);
	write_cmos_sensor(0x5118, 0x7540);
	write_cmos_sensor(0x511A, 0x4292);
	write_cmos_sensor(0x511C, 0x8322);
	write_cmos_sensor(0x511E, 0x7542);
	write_cmos_sensor(0x5120, 0x5A07);
	write_cmos_sensor(0x5122, 0x6B08);
	write_cmos_sensor(0x5124, 0x421C);
	write_cmos_sensor(0x5126, 0x831C);
	write_cmos_sensor(0x5128, 0x430D);
	write_cmos_sensor(0x512A, 0x470E);
	write_cmos_sensor(0x512C, 0x480F);
	write_cmos_sensor(0x512E, 0x8C0E);
	write_cmos_sensor(0x5130, 0x7D0F);
	write_cmos_sensor(0x5132, 0x2C02);
	write_cmos_sensor(0x5134, 0x4C07);
	write_cmos_sensor(0x5136, 0x4D08);
	write_cmos_sensor(0x5138, 0x4782);
	write_cmos_sensor(0x513A, 0x7316);
	write_cmos_sensor(0x513C, 0x4882);
	write_cmos_sensor(0x513E, 0x7318);
	write_cmos_sensor(0x5140, 0x460F);
	write_cmos_sensor(0x5142, 0x890F);
	write_cmos_sensor(0x5144, 0x421E);
	write_cmos_sensor(0x5146, 0x7324);
	write_cmos_sensor(0x5148, 0x8F0E);
	write_cmos_sensor(0x514A, 0x922E);
	write_cmos_sensor(0x514C, 0x3401);
	write_cmos_sensor(0x514E, 0x422E);
	write_cmos_sensor(0x5150, 0x4E82);
	write_cmos_sensor(0x5152, 0x7324);
	write_cmos_sensor(0x5154, 0x9316);
	write_cmos_sensor(0x5156, 0x3804);
	write_cmos_sensor(0x5158, 0x4682);
	write_cmos_sensor(0x515A, 0x7334);
	write_cmos_sensor(0x515C, 0x0F00);
	write_cmos_sensor(0x515E, 0x7300);
	write_cmos_sensor(0x5160, 0xD3D2);
	write_cmos_sensor(0x5162, 0x00C2);
	write_cmos_sensor(0x5164, 0x3C4D);
	write_cmos_sensor(0x5166, 0x9329);
	write_cmos_sensor(0x5168, 0x3422);
	write_cmos_sensor(0x516A, 0x490E);
	write_cmos_sensor(0x516C, 0x4E0F);
	write_cmos_sensor(0x516E, 0x5F0F);
	write_cmos_sensor(0x5170, 0x7F0F);
	write_cmos_sensor(0x5172, 0xE33F);
	write_cmos_sensor(0x5174, 0x5E82);
	write_cmos_sensor(0x5176, 0x7316);
	write_cmos_sensor(0x5178, 0x6F82);
	write_cmos_sensor(0x517A, 0x7318);
	write_cmos_sensor(0x517C, 0x521E);
	write_cmos_sensor(0x517E, 0x8320);
	write_cmos_sensor(0x5180, 0x621F);
	write_cmos_sensor(0x5182, 0x8322);
	write_cmos_sensor(0x5184, 0x4E82);
	write_cmos_sensor(0x5186, 0x7540);
	write_cmos_sensor(0x5188, 0x4F82);
	write_cmos_sensor(0x518A, 0x7542);
	write_cmos_sensor(0x518C, 0x421E);
	write_cmos_sensor(0x518E, 0x7300);
	write_cmos_sensor(0x5190, 0x421F);
	write_cmos_sensor(0x5192, 0x7302);
	write_cmos_sensor(0x5194, 0x803E);
	write_cmos_sensor(0x5196, 0x0003);
	write_cmos_sensor(0x5198, 0x730F);
	write_cmos_sensor(0x519A, 0x2FF8);
	write_cmos_sensor(0x519C, 0x12B0);
	write_cmos_sensor(0x519E, 0xFE78);
	write_cmos_sensor(0x51A0, 0x0B00);
	write_cmos_sensor(0x51A2, 0x7300);
	write_cmos_sensor(0x51A4, 0x0002);
	write_cmos_sensor(0x51A6, 0x0B00);
	write_cmos_sensor(0x51A8, 0x7304);
	write_cmos_sensor(0x51AA, 0x01F4);
	write_cmos_sensor(0x51AC, 0x3FD9);
	write_cmos_sensor(0x51AE, 0x0B00);
	write_cmos_sensor(0x51B0, 0x7304);
	write_cmos_sensor(0x51B2, 0x03AF);
	write_cmos_sensor(0x51B4, 0x4392);
	write_cmos_sensor(0x51B6, 0x731C);
	write_cmos_sensor(0x51B8, 0x4292);
	write_cmos_sensor(0x51BA, 0x8320);
	write_cmos_sensor(0x51BC, 0x7540);
	write_cmos_sensor(0x51BE, 0x4292);
	write_cmos_sensor(0x51C0, 0x8322);
	write_cmos_sensor(0x51C2, 0x7542);
	write_cmos_sensor(0x51C4, 0x12B0);
	write_cmos_sensor(0x51C6, 0xFE78);
	write_cmos_sensor(0x51C8, 0x3FCB);
	write_cmos_sensor(0x51CA, 0x4A09);
	write_cmos_sensor(0x51CC, 0x8219);
	write_cmos_sensor(0x51CE, 0x82CE);
	write_cmos_sensor(0x51D0, 0x3F57);
	write_cmos_sensor(0x51D2, 0x4E6F);
	write_cmos_sensor(0x51D4, 0xF21F);
	write_cmos_sensor(0x51D6, 0x731C);
	write_cmos_sensor(0x51D8, 0x23FC);
	write_cmos_sensor(0x51DA, 0x9382);
	write_cmos_sensor(0x51DC, 0x731C);
	write_cmos_sensor(0x51DE, 0x2003);
	write_cmos_sensor(0x51E0, 0x93CE);
	write_cmos_sensor(0x51E2, 0x0000);
	write_cmos_sensor(0x51E4, 0x23FA);
	write_cmos_sensor(0x51E6, 0x3F34);
	write_cmos_sensor(0x51E8, 0x9382);
	write_cmos_sensor(0x51EA, 0x730C);
	write_cmos_sensor(0x51EC, 0x23B9);
	write_cmos_sensor(0x51EE, 0x42D2);
	write_cmos_sensor(0x51F0, 0x0AA0);
	write_cmos_sensor(0x51F2, 0x0A80);
	write_cmos_sensor(0x51F4, 0x9382);
	write_cmos_sensor(0x51F6, 0x730C);
	write_cmos_sensor(0x51F8, 0x27FA);
	write_cmos_sensor(0x51FA, 0x3FB2);
	write_cmos_sensor(0x51FC, 0x0900);
	write_cmos_sensor(0x51FE, 0x732C);
	write_cmos_sensor(0x5200, 0x425F);
	write_cmos_sensor(0x5202, 0x0788);
	write_cmos_sensor(0x5204, 0x4292);
	write_cmos_sensor(0x5206, 0x7544);
	write_cmos_sensor(0x5208, 0x8318);
	write_cmos_sensor(0x520A, 0x4292);
	write_cmos_sensor(0x520C, 0x7546);
	write_cmos_sensor(0x520E, 0x831A);
	write_cmos_sensor(0x5210, 0x42D2);
	write_cmos_sensor(0x5212, 0x0AA0);
	write_cmos_sensor(0x5214, 0x0A80);
	write_cmos_sensor(0x5216, 0x4134);
	write_cmos_sensor(0x5218, 0x4135);
	write_cmos_sensor(0x521A, 0x4136);
	write_cmos_sensor(0x521C, 0x4137);
	write_cmos_sensor(0x521E, 0x4138);
	write_cmos_sensor(0x5220, 0x4139);
	write_cmos_sensor(0x5222, 0x413A);
	write_cmos_sensor(0x5224, 0x413B);
	write_cmos_sensor(0x5226, 0x4130);
	write_cmos_sensor(0x2000, 0x98EB);
	write_cmos_sensor(0x2002, 0x00FF);
	write_cmos_sensor(0x2004, 0x0007);
	write_cmos_sensor(0x2006, 0x3FFF);
	write_cmos_sensor(0x2008, 0x3FFF);
	write_cmos_sensor(0x200A, 0xC216);
	write_cmos_sensor(0x200C, 0x1292);
	write_cmos_sensor(0x200E, 0xC01A);
	write_cmos_sensor(0x2010, 0x403D);
	write_cmos_sensor(0x2012, 0x000E);
	write_cmos_sensor(0x2014, 0x403E);
	write_cmos_sensor(0x2016, 0x0B80);
	write_cmos_sensor(0x2018, 0x403F);
	write_cmos_sensor(0x201A, 0x82AE);
	write_cmos_sensor(0x201C, 0x1292);
	write_cmos_sensor(0x201E, 0xC00C);
	write_cmos_sensor(0x2020, 0x4130);
	write_cmos_sensor(0x2022, 0x43E2);
	write_cmos_sensor(0x2024, 0x0180);
	write_cmos_sensor(0x2026, 0x4130);
	write_cmos_sensor(0x2028, 0x7400);
	write_cmos_sensor(0x202A, 0x5000);
	write_cmos_sensor(0x202C, 0x0253);
	write_cmos_sensor(0x202E, 0x0253);
	write_cmos_sensor(0x2030, 0x0851);
	write_cmos_sensor(0x2032, 0x00D1);
	write_cmos_sensor(0x2034, 0x0009);
	write_cmos_sensor(0x2036, 0x5020);
	write_cmos_sensor(0x2038, 0x000B);
	write_cmos_sensor(0x203A, 0x0002);
	write_cmos_sensor(0x203C, 0x0044);
	write_cmos_sensor(0x203E, 0x0016);
	write_cmos_sensor(0x2040, 0x1792);
	write_cmos_sensor(0x2042, 0x7002);
	write_cmos_sensor(0x2044, 0x154F);
	write_cmos_sensor(0x2046, 0x00D5);
	write_cmos_sensor(0x2048, 0x000B);
	write_cmos_sensor(0x204A, 0x0019);
	write_cmos_sensor(0x204C, 0x1698);
	write_cmos_sensor(0x204E, 0x000E);
	write_cmos_sensor(0x2050, 0x099A);
	write_cmos_sensor(0x2052, 0x0058);
	write_cmos_sensor(0x2054, 0x7000);
	write_cmos_sensor(0x2056, 0x1799);
	write_cmos_sensor(0x2058, 0x0310);
	write_cmos_sensor(0x205A, 0x03C3);
	write_cmos_sensor(0x205C, 0x004C);
	write_cmos_sensor(0x205E, 0x064A);
	write_cmos_sensor(0x2060, 0x0001);
	write_cmos_sensor(0x2062, 0x0007);
	write_cmos_sensor(0x2064, 0x0BC7);
	write_cmos_sensor(0x2066, 0x0055);
	write_cmos_sensor(0x2068, 0x7000);
	write_cmos_sensor(0x206A, 0x1550);
	write_cmos_sensor(0x206C, 0x158A);
	write_cmos_sensor(0x206E, 0x0004);
	write_cmos_sensor(0x2070, 0x1488);
	write_cmos_sensor(0x2072, 0x7010);
	write_cmos_sensor(0x2074, 0x1508);
	write_cmos_sensor(0x2076, 0x0004);
	write_cmos_sensor(0x2078, 0x0016);
	write_cmos_sensor(0x207A, 0x03D5);
	write_cmos_sensor(0x207C, 0x0055);
	write_cmos_sensor(0x207E, 0x08CA);
	write_cmos_sensor(0x2080, 0x2019);
	write_cmos_sensor(0x2082, 0x0007);
	write_cmos_sensor(0x2084, 0x7057);
	write_cmos_sensor(0x2086, 0x0FC7);
	write_cmos_sensor(0x2088, 0x5041);
	write_cmos_sensor(0x208A, 0x12C8);
	write_cmos_sensor(0x208C, 0x5060);
	write_cmos_sensor(0x208E, 0x5080);
	write_cmos_sensor(0x2090, 0x2084);
	write_cmos_sensor(0x2092, 0x12C8);
	write_cmos_sensor(0x2094, 0x7800);
	write_cmos_sensor(0x2096, 0x0802);
	write_cmos_sensor(0x2098, 0x040F);
	write_cmos_sensor(0x209A, 0x1007);
	write_cmos_sensor(0x209C, 0x0803);
	write_cmos_sensor(0x209E, 0x080B);
	write_cmos_sensor(0x20A0, 0x3803);
	write_cmos_sensor(0x20A2, 0x0807);
	write_cmos_sensor(0x20A4, 0x0404);
	write_cmos_sensor(0x20A6, 0x0400);
	write_cmos_sensor(0x20A8, 0xFFFF);
	write_cmos_sensor(0x20AA, 0x1292);
	write_cmos_sensor(0x20AC, 0xC006);
	write_cmos_sensor(0x20AE, 0x421F);
	write_cmos_sensor(0x20B0, 0x0710);
	write_cmos_sensor(0x20B2, 0x523F);
	write_cmos_sensor(0x20B4, 0x4F82);
	write_cmos_sensor(0x20B6, 0x831C);
	write_cmos_sensor(0x20B8, 0x93C2);
	write_cmos_sensor(0x20BA, 0x829F);
	write_cmos_sensor(0x20BC, 0x241E);
	write_cmos_sensor(0x20BE, 0x403D);
	write_cmos_sensor(0x20C0, 0xFFFE);
	write_cmos_sensor(0x20C2, 0x40B2);
	write_cmos_sensor(0x20C4, 0xEC78);
	write_cmos_sensor(0x20C6, 0x8324);
	write_cmos_sensor(0x20C8, 0x40B2);
	write_cmos_sensor(0x20CA, 0xEC78);
	write_cmos_sensor(0x20CC, 0x8326);
	write_cmos_sensor(0x20CE, 0x40B2);
	write_cmos_sensor(0x20D0, 0xEC78);
	write_cmos_sensor(0x20D2, 0x8328);
	write_cmos_sensor(0x20D4, 0x425F);
	write_cmos_sensor(0x20D6, 0x008C);
	write_cmos_sensor(0x20D8, 0x934F);
	write_cmos_sensor(0x20DA, 0x240E);
	write_cmos_sensor(0x20DC, 0x4D0E);
	write_cmos_sensor(0x20DE, 0x503E);
	write_cmos_sensor(0x20E0, 0xFFD8);
	write_cmos_sensor(0x20E2, 0x4E82);
	write_cmos_sensor(0x20E4, 0x8324);
	write_cmos_sensor(0x20E6, 0xF36F);
	write_cmos_sensor(0x20E8, 0x2407);
	write_cmos_sensor(0x20EA, 0x4E0F);
	write_cmos_sensor(0x20EC, 0x5D0F);
	write_cmos_sensor(0x20EE, 0x4F82);
	write_cmos_sensor(0x20F0, 0x8326);
	write_cmos_sensor(0x20F2, 0x5D0F);
	write_cmos_sensor(0x20F4, 0x4F82);
	write_cmos_sensor(0x20F6, 0x8328);
	write_cmos_sensor(0x20F8, 0x4130);
	write_cmos_sensor(0x20FA, 0x432D);
	write_cmos_sensor(0x20FC, 0x3FE2);
	write_cmos_sensor(0x20FE, 0x120B);
	write_cmos_sensor(0x2100, 0x120A);
	write_cmos_sensor(0x2102, 0x1209);
	write_cmos_sensor(0x2104, 0x1208);
	write_cmos_sensor(0x2106, 0x1207);
	write_cmos_sensor(0x2108, 0x4292);
	write_cmos_sensor(0x210A, 0x7540);
	write_cmos_sensor(0x210C, 0x832C);
	write_cmos_sensor(0x210E, 0x4292);
	write_cmos_sensor(0x2110, 0x7542);
	write_cmos_sensor(0x2112, 0x832E);
	write_cmos_sensor(0x2114, 0x93C2);
	write_cmos_sensor(0x2116, 0x00C1);
	write_cmos_sensor(0x2118, 0x2405);
	write_cmos_sensor(0x211A, 0x42D2);
	write_cmos_sensor(0x211C, 0x8330);
	write_cmos_sensor(0x211E, 0x82A0);
	write_cmos_sensor(0x2120, 0x4392);
	write_cmos_sensor(0x2122, 0x82D4);
	write_cmos_sensor(0x2124, 0x1292);
	write_cmos_sensor(0x2126, 0xC046);
	write_cmos_sensor(0x2128, 0x4292);
	write_cmos_sensor(0x212A, 0x7324);
	write_cmos_sensor(0x212C, 0x832A);
	write_cmos_sensor(0x212E, 0x93C2);
	write_cmos_sensor(0x2130, 0x00C1);
	write_cmos_sensor(0x2132, 0x244E);
	write_cmos_sensor(0x2134, 0x4218);
	write_cmos_sensor(0x2136, 0x7316);
	write_cmos_sensor(0x2138, 0x4219);
	write_cmos_sensor(0x213A, 0x7318);
	write_cmos_sensor(0x213C, 0x421F);
	write_cmos_sensor(0x213E, 0x0710);
	write_cmos_sensor(0x2140, 0x4F0E);
	write_cmos_sensor(0x2142, 0x430F);
	write_cmos_sensor(0x2144, 0x480A);
	write_cmos_sensor(0x2146, 0x490B);
	write_cmos_sensor(0x2148, 0x8E0A);
	write_cmos_sensor(0x214A, 0x7F0B);
	write_cmos_sensor(0x214C, 0x4A0E);
	write_cmos_sensor(0x214E, 0x4B0F);
	write_cmos_sensor(0x2150, 0x803E);
	write_cmos_sensor(0x2152, 0x0102);
	write_cmos_sensor(0x2154, 0x730F);
	write_cmos_sensor(0x2156, 0x283A);
	write_cmos_sensor(0x2158, 0x403F);
	write_cmos_sensor(0x215A, 0x0101);
	write_cmos_sensor(0x215C, 0x832F);
	write_cmos_sensor(0x215E, 0x43D2);
	write_cmos_sensor(0x2160, 0x01B3);
	write_cmos_sensor(0x2162, 0x4F82);
	write_cmos_sensor(0x2164, 0x7324);
	write_cmos_sensor(0x2166, 0x4292);
	write_cmos_sensor(0x2168, 0x7540);
	write_cmos_sensor(0x216A, 0x8320);
	write_cmos_sensor(0x216C, 0x4292);
	write_cmos_sensor(0x216E, 0x7542);
	write_cmos_sensor(0x2170, 0x8322);
	write_cmos_sensor(0x2172, 0x4347);
	write_cmos_sensor(0x2174, 0x823F);
	write_cmos_sensor(0x2176, 0x4F0C);
	write_cmos_sensor(0x2178, 0x430D);
	write_cmos_sensor(0x217A, 0x421E);
	write_cmos_sensor(0x217C, 0x8320);
	write_cmos_sensor(0x217E, 0x421F);
	write_cmos_sensor(0x2180, 0x8322);
	write_cmos_sensor(0x2182, 0x5E0C);
	write_cmos_sensor(0x2184, 0x6F0D);
	write_cmos_sensor(0x2186, 0x8A0C);
	write_cmos_sensor(0x2188, 0x7B0D);
	write_cmos_sensor(0x218A, 0x2801);
	write_cmos_sensor(0x218C, 0x4357);
	write_cmos_sensor(0x218E, 0x47C2);
	write_cmos_sensor(0x2190, 0x8330);
	write_cmos_sensor(0x2192, 0x93C2);
	write_cmos_sensor(0x2194, 0x829A);
	write_cmos_sensor(0x2196, 0x201C);
	write_cmos_sensor(0x2198, 0x93C2);
	write_cmos_sensor(0x219A, 0x82A0);
	write_cmos_sensor(0x219C, 0x2404);
	write_cmos_sensor(0x219E, 0x43B2);
	write_cmos_sensor(0x21A0, 0x7540);
	write_cmos_sensor(0x21A2, 0x43B2);
	write_cmos_sensor(0x21A4, 0x7542);
	write_cmos_sensor(0x21A6, 0x93C2);
	write_cmos_sensor(0x21A8, 0x8330);
	write_cmos_sensor(0x21AA, 0x2412);
	write_cmos_sensor(0x21AC, 0x532E);
	write_cmos_sensor(0x21AE, 0x630F);
	write_cmos_sensor(0x21B0, 0x4E82);
	write_cmos_sensor(0x21B2, 0x8320);
	write_cmos_sensor(0x21B4, 0x4F82);
	write_cmos_sensor(0x21B6, 0x8322);
	write_cmos_sensor(0x21B8, 0x480C);
	write_cmos_sensor(0x21BA, 0x490D);
	write_cmos_sensor(0x21BC, 0x8E0C);
	write_cmos_sensor(0x21BE, 0x7F0D);
	write_cmos_sensor(0x21C0, 0x2C07);
	write_cmos_sensor(0x21C2, 0x4882);
	write_cmos_sensor(0x21C4, 0x8320);
	write_cmos_sensor(0x21C6, 0x4982);
	write_cmos_sensor(0x21C8, 0x8322);
	write_cmos_sensor(0x21CA, 0x3C02);
	write_cmos_sensor(0x21CC, 0x4A0F);
	write_cmos_sensor(0x21CE, 0x3FC6);
	write_cmos_sensor(0x21D0, 0x4137);
	write_cmos_sensor(0x21D2, 0x4138);
	write_cmos_sensor(0x21D4, 0x4139);
	write_cmos_sensor(0x21D6, 0x413A);
	write_cmos_sensor(0x21D8, 0x413B);
	write_cmos_sensor(0x21DA, 0x4130);
	write_cmos_sensor(0x21DC, 0x421F);
	write_cmos_sensor(0x21DE, 0x7100);
	write_cmos_sensor(0x21E0, 0x4F0E);
	write_cmos_sensor(0x21E2, 0x503E);
	write_cmos_sensor(0x21E4, 0xFFD8);
	write_cmos_sensor(0x21E6, 0x4E82);
	write_cmos_sensor(0x21E8, 0x7A04);
	write_cmos_sensor(0x21EA, 0x421E);
	write_cmos_sensor(0x21EC, 0x8324);
	write_cmos_sensor(0x21EE, 0x5F0E);
	write_cmos_sensor(0x21F0, 0x4E82);
	write_cmos_sensor(0x21F2, 0x7A06);
	write_cmos_sensor(0x21F4, 0x0B00);
	write_cmos_sensor(0x21F6, 0x7304);
	write_cmos_sensor(0x21F8, 0x0050);
	write_cmos_sensor(0x21FA, 0x40B2);
	write_cmos_sensor(0x21FC, 0xD081);
	write_cmos_sensor(0x21FE, 0x0B88);
	write_cmos_sensor(0x2200, 0x421E);
	write_cmos_sensor(0x2202, 0x8326);
	write_cmos_sensor(0x2204, 0x5F0E);
	write_cmos_sensor(0x2206, 0x4E82);
	write_cmos_sensor(0x2208, 0x7A0E);
	write_cmos_sensor(0x220A, 0x521F);
	write_cmos_sensor(0x220C, 0x8328);
	write_cmos_sensor(0x220E, 0x4F82);
	write_cmos_sensor(0x2210, 0x7A10);
	write_cmos_sensor(0x2212, 0x0B00);
	write_cmos_sensor(0x2214, 0x7304);
	write_cmos_sensor(0x2216, 0x007A);
	write_cmos_sensor(0x2218, 0x40B2);
	write_cmos_sensor(0x221A, 0x0081);
	write_cmos_sensor(0x221C, 0x0B88);
	write_cmos_sensor(0x221E, 0x4392);
	write_cmos_sensor(0x2220, 0x7A0A);
	write_cmos_sensor(0x2222, 0x0800);
	write_cmos_sensor(0x2224, 0x7A0C);
	write_cmos_sensor(0x2226, 0x0B00);
	write_cmos_sensor(0x2228, 0x7304);
	write_cmos_sensor(0x222A, 0x022B);
	write_cmos_sensor(0x222C, 0x40B2);
	write_cmos_sensor(0x222E, 0xD081);
	write_cmos_sensor(0x2230, 0x0B88);
	write_cmos_sensor(0x2232, 0x0B00);
	write_cmos_sensor(0x2234, 0x7304);
	write_cmos_sensor(0x2236, 0x0255);
	write_cmos_sensor(0x2238, 0x40B2);
	write_cmos_sensor(0x223A, 0x0081);
	write_cmos_sensor(0x223C, 0x0B88);
	write_cmos_sensor(0x223E, 0x9382);
	write_cmos_sensor(0x2240, 0x7112);
	write_cmos_sensor(0x2242, 0x2402);
	write_cmos_sensor(0x2244, 0x4392);
	write_cmos_sensor(0x2246, 0x760E);
	write_cmos_sensor(0x2248, 0x93D2);
	write_cmos_sensor(0x224A, 0x01B2);
	write_cmos_sensor(0x224C, 0x2003);
	write_cmos_sensor(0x224E, 0x42D2);
	write_cmos_sensor(0x2250, 0x0AA0);
	write_cmos_sensor(0x2252, 0x0A80);
	write_cmos_sensor(0x2254, 0x4130);
	write_cmos_sensor(0x2256, 0xF0B2);
	write_cmos_sensor(0x2258, 0xFFBF);
	write_cmos_sensor(0x225A, 0x2004);
	write_cmos_sensor(0x225C, 0x9382);
	write_cmos_sensor(0x225E, 0xC314);
	write_cmos_sensor(0x2260, 0x2406);
	write_cmos_sensor(0x2262, 0x12B0);
	write_cmos_sensor(0x2264, 0xC90A);
	write_cmos_sensor(0x2266, 0x40B2);
	write_cmos_sensor(0x2268, 0xC6CE);
	write_cmos_sensor(0x226A, 0x8246);
	write_cmos_sensor(0x226C, 0x3C02);
	write_cmos_sensor(0x226E, 0x12B0);
	write_cmos_sensor(0x2270, 0xCAB0);
	write_cmos_sensor(0x2272, 0x42A2);
	write_cmos_sensor(0x2274, 0x7324);
	write_cmos_sensor(0x2276, 0x4130);
	write_cmos_sensor(0x2278, 0x403E);
	write_cmos_sensor(0x227A, 0x00C2);
	write_cmos_sensor(0x227C, 0x421F);
	write_cmos_sensor(0x227E, 0x7314);
	write_cmos_sensor(0x2280, 0xF07F);
	write_cmos_sensor(0x2282, 0x000C);
	write_cmos_sensor(0x2284, 0x5F4F);
	write_cmos_sensor(0x2286, 0x5F4F);
	write_cmos_sensor(0x2288, 0xDFCE);
	write_cmos_sensor(0x228A, 0x0000);
	write_cmos_sensor(0x228C, 0xF0FE);
	write_cmos_sensor(0x228E, 0x000F);
	write_cmos_sensor(0x2290, 0x0000);
	write_cmos_sensor(0x2292, 0x4130);
	write_cmos_sensor(0x2294, 0x40B2);
	write_cmos_sensor(0x2296, 0x3FFF);
	write_cmos_sensor(0x2298, 0x0A8A);
	write_cmos_sensor(0x229A, 0x4292);
	write_cmos_sensor(0x229C, 0x826E);
	write_cmos_sensor(0x229E, 0x0A9C);
	write_cmos_sensor(0x22A0, 0x43C2);
	write_cmos_sensor(0x22A2, 0x8330);
	write_cmos_sensor(0x22A4, 0x1292);
	write_cmos_sensor(0x22A6, 0xC018);
	write_cmos_sensor(0x22A8, 0x93C2);
	write_cmos_sensor(0x22AA, 0x0AA0);
	write_cmos_sensor(0x22AC, 0x27FD);
	write_cmos_sensor(0x22AE, 0x4292);
	write_cmos_sensor(0x22B0, 0x0A9C);
	write_cmos_sensor(0x22B2, 0x826E);
	write_cmos_sensor(0x22B4, 0x4292);
	write_cmos_sensor(0x22B6, 0x0A8A);
	write_cmos_sensor(0x22B8, 0x2008);
	write_cmos_sensor(0x22BA, 0x43D2);
	write_cmos_sensor(0x22BC, 0x0A80);
	write_cmos_sensor(0x22BE, 0x4130);
	write_cmos_sensor(0x23FE, 0xC056);
	write_cmos_sensor(0x3230, 0xFE94);
	write_cmos_sensor(0x3232, 0xFC0C);
	write_cmos_sensor(0x3236, 0xFC22);
	write_cmos_sensor(0x323A, 0xFDDC);
	write_cmos_sensor(0x323C, 0xFCAA);
	write_cmos_sensor(0x323E, 0xFCFE);
	write_cmos_sensor(0x3246, 0xE000);
	write_cmos_sensor(0x3248, 0xCDB0);
	write_cmos_sensor(0x324E, 0xFE56);
	write_cmos_sensor(0x326A, 0x8302);
	write_cmos_sensor(0x326C, 0x830A);
	write_cmos_sensor(0x326E, 0x0000);
	write_cmos_sensor(0x32CA, 0xFC28);
	write_cmos_sensor(0x32CC, 0xC3BC);
	write_cmos_sensor(0x32CE, 0xC34C);
	write_cmos_sensor(0x32D0, 0xC35A);
	write_cmos_sensor(0x32D2, 0xC368);
	write_cmos_sensor(0x32D4, 0xC376);
	write_cmos_sensor(0x32D6, 0xC3C2);
	write_cmos_sensor(0x32D8, 0xC3E6);
	write_cmos_sensor(0x32DA, 0x0003);
	write_cmos_sensor(0x32DC, 0x0003);
	write_cmos_sensor(0x32DE, 0x00C7);
	write_cmos_sensor(0x32E0, 0x0031);
	write_cmos_sensor(0x32E2, 0x0031);
	write_cmos_sensor(0x32E4, 0x0031);
	write_cmos_sensor(0x32E6, 0xFC28);
	write_cmos_sensor(0x32E8, 0xC3BC);
	write_cmos_sensor(0x32EA, 0xC384);
	write_cmos_sensor(0x32EC, 0xC392);
	write_cmos_sensor(0x32EE, 0xC3A0);
	write_cmos_sensor(0x32F0, 0xC3AE);
	write_cmos_sensor(0x32F2, 0xC3C4);
	write_cmos_sensor(0x32F4, 0xC3E6);
	write_cmos_sensor(0x32F6, 0x0003);
	write_cmos_sensor(0x32F8, 0x0003);
	write_cmos_sensor(0x32FA, 0x00C7);
	write_cmos_sensor(0x32FC, 0x0031);
	write_cmos_sensor(0x32FE, 0x0031);
	write_cmos_sensor(0x3300, 0x0031);
	write_cmos_sensor(0x3302, 0x82CA);
	write_cmos_sensor(0x3304, 0xC164);
	write_cmos_sensor(0x3306, 0x82E6);
	write_cmos_sensor(0x3308, 0xC19C);
	write_cmos_sensor(0x330A, 0x001F);
	write_cmos_sensor(0x330C, 0x001A);
	write_cmos_sensor(0x330E, 0x0034);
	write_cmos_sensor(0x3310, 0x0000);
	write_cmos_sensor(0x3312, 0x0000);
	write_cmos_sensor(0x3314, 0xFC96);
	write_cmos_sensor(0x3316, 0xC3D8);
	write_cmos_sensor(0x0A20, 0x0000);
	write_cmos_sensor(0x0E04, 0x0012);
	write_cmos_sensor(0x002E, 0x1111);
	write_cmos_sensor(0x0032, 0x1111);
	write_cmos_sensor(0x0022, 0x0008);
	write_cmos_sensor(0x0026, 0x0040);
	write_cmos_sensor(0x0028, 0x0017);
	write_cmos_sensor(0x002C, 0x09CF);
	write_cmos_sensor(0x005C, 0x2101);
	write_cmos_sensor(0x0006, 0x09BC);
	write_cmos_sensor(0x0008, 0x0ED8);
	write_cmos_sensor(0x000E, 0x0100);
	write_cmos_sensor(0x000C, 0x0022);
	write_cmos_sensor(0x0A22, 0x0000);
	write_cmos_sensor(0x0A24, 0x0000);
	write_cmos_sensor(0x0804, 0x0000);
	write_cmos_sensor(0x0A12, 0x0CC0);
	write_cmos_sensor(0x0A14, 0x0990);
	write_cmos_sensor(0x0074, 0x09B6);
	write_cmos_sensor(0x0076, 0x0000);
	write_cmos_sensor(0x051E, 0x0000);
	write_cmos_sensor(0x0200, 0x0400);
	write_cmos_sensor(0x0A1A, 0x0C00);
	write_cmos_sensor(0x0A0C, 0x0010);
	write_cmos_sensor(0x0A1E, 0x0CCF);
	write_cmos_sensor(0x0402, 0x0110);
	write_cmos_sensor(0x0404, 0x00F4);
	write_cmos_sensor(0x0408, 0x0000);
	write_cmos_sensor(0x0410, 0x008D);
	write_cmos_sensor(0x0412, 0x011A);
	write_cmos_sensor(0x0414, 0x864C);
	write_cmos_sensor(0x021C, 0x0001);
	write_cmos_sensor(0x0C00, 0x9150);
	write_cmos_sensor(0x0C06, 0x0021);
	write_cmos_sensor(0x0C10, 0x0040);
	write_cmos_sensor(0x0C12, 0x0040);
	write_cmos_sensor(0x0C14, 0x0040);
	write_cmos_sensor(0x0C16, 0x0040);
	write_cmos_sensor(0x0A02, 0x0000);
	write_cmos_sensor(0x0A04, 0x014A);
	write_cmos_sensor(0x0418, 0x0000);
	write_cmos_sensor(0x0128, 0x0025);
	write_cmos_sensor(0x012A, 0xFFFF);
	write_cmos_sensor(0x0120, 0x0041);
	write_cmos_sensor(0x0122, 0x0371);
	write_cmos_sensor(0x012C, 0x001E);
	write_cmos_sensor(0x012E, 0xFFFF);
	write_cmos_sensor(0x0124, 0x003B);
	write_cmos_sensor(0x0126, 0x0373);
	write_cmos_sensor(0x0746, 0x0050);
	write_cmos_sensor(0x0748, 0x01D5);
	write_cmos_sensor(0x074A, 0x022B);
	write_cmos_sensor(0x074C, 0x03B0);
	write_cmos_sensor(0x0756, 0x043F);
	write_cmos_sensor(0x0758, 0x3F1D);
	write_cmos_sensor(0x0B02, 0xE04D);
	write_cmos_sensor(0x0B10, 0x6821);
	write_cmos_sensor(0x0B12, 0x0030);
	write_cmos_sensor(0x0B14, 0x0001);
	write_cmos_sensor(0x0A0A, 0x38FD);
	write_cmos_sensor(0x0A1C, 0x0000);
	write_cmos_sensor(0x0900, 0x0300);
	write_cmos_sensor(0x0902, 0xC319);
	write_cmos_sensor(0x0914, 0xC109);
	write_cmos_sensor(0x0916, 0x061A);
	write_cmos_sensor(0x0918, 0x0407);
	write_cmos_sensor(0x091A, 0x0A0B);
	write_cmos_sensor(0x091C, 0x0E08);
	write_cmos_sensor(0x091E, 0x0A00);
	write_cmos_sensor(0x090C, 0x03FA);
	write_cmos_sensor(0x090E, 0x0030);
	write_cmos_sensor(0x0954, 0x0089);
	write_cmos_sensor(0x0956, 0x0000);
	write_cmos_sensor(0x0958, 0xC980);
	write_cmos_sensor(0x095A, 0x6180);
	write_cmos_sensor(0x0710, 0x09B0);
	write_cmos_sensor(0x0040, 0x0200);
	write_cmos_sensor(0x0042, 0x0100);
	write_cmos_sensor(0x0D04, 0x0000);
	write_cmos_sensor(0x0F08, 0x2F04);
	write_cmos_sensor(0x0F30, 0x001F);
	write_cmos_sensor(0x0F36, 0x001F);
	write_cmos_sensor(0x0F04, 0x3A00);
	write_cmos_sensor(0x0F32, 0x0253);
	write_cmos_sensor(0x0F38, 0x059D);
	write_cmos_sensor(0x0F2A, 0x4124);
	write_cmos_sensor(0x006A, 0x0100);
	write_cmos_sensor(0x004C, 0x0100);
#endif
}

#if MULTI_WRITE
kal_uint16 addr_data_pair_preview_sr846[] = {
	0x0A20, 0x0000,
	0x002E, 0x1111,
	0x0032, 0x1111,
	0x0022, 0x0008,
	0x0026, 0x0040,
	0x0028, 0x0017,
	0x002C, 0x09CF,
	0x005C, 0x2101,
	0x0006, 0x09DB,
	0x0008, 0x0ED8,
	0x000E, 0x0200,
	0x000C, 0x0022,
	0x0A22, 0x0000,
	0x0A24, 0x0000,
	0x0804, 0x0000,
	0x0A12, 0x0CC0,
	0x0A14, 0x0990,
	0x0074, 0x09D5,
	0x0076, 0x0000,
	0x0A0C, 0x0010,
	0x0A1E, 0x0CCF,
	0x0A04, 0x014A,
	0x0418, 0x0000,
	0x0128, 0x0025,
	0x012A, 0xFFFF,
	0x0120, 0x0041,
	0x0122, 0x0371,
	0x012C, 0x001E,
	0x012E, 0xFFFF,
	0x0124, 0x003B,
	0x0126, 0x0373,
	0x0B02, 0xE04D,
	0x0B10, 0x6821,
	0x0B12, 0x0030,
	0x0B14, 0x0001,
	0x0A0A, 0x38FD,
	0x0A1C, 0x0000,
	0x0710, 0x09B0,
	0x0900, 0x0300,
	0x0902, 0xC319,
	0x0914, 0xC109,
	0x0916, 0x061A,
	0x0918, 0x0407,
	0x091A, 0x0A0B,
	0x091C, 0x0E08,
	0x091E, 0x0A00,
	0x090C, 0x03FA,
	0x090E, 0x0030,
	0x0F08, 0x2F04,
	0x0F30, 0x001F,
	0x0F36, 0x001F,
	0x0F04, 0x3A00,
	0x0F32, 0x0253,
	0x0F38, 0x059D,
	0x0F2A, 0x4124,
	0x004C, 0x0100
};
#endif

static void preview_setting(void)
{
#if MULTI_WRITE
	sr846_table_write_cmos_sensor(
		addr_data_pair_preview_sr846,
		sizeof(addr_data_pair_preview_sr846) /
		sizeof(kal_uint16));
#else
	write_cmos_sensor(0x0A20, 0x0000);
	write_cmos_sensor(0x002E, 0x1111);
	write_cmos_sensor(0x0032, 0x1111);
	write_cmos_sensor(0x0022, 0x0008);
	write_cmos_sensor(0x0026, 0x0040);
	write_cmos_sensor(0x0028, 0x0017);
	write_cmos_sensor(0x002C, 0x09CF);
	write_cmos_sensor(0x005C, 0x2101);
	write_cmos_sensor(0x0006, 0x09DB);
	write_cmos_sensor(0x0008, 0x0ED8);
	write_cmos_sensor(0x000E, 0x0200);
	write_cmos_sensor(0x000C, 0x0022);
	write_cmos_sensor(0x0A22, 0x0000);
	write_cmos_sensor(0x0A24, 0x0000);
	write_cmos_sensor(0x0804, 0x0000);
	write_cmos_sensor(0x0A12, 0x0CC0);
	write_cmos_sensor(0x0A14, 0x0990);
	write_cmos_sensor(0x0074, 0x09D5);
	write_cmos_sensor(0x0076, 0x0000);
	write_cmos_sensor(0x0A0C, 0x0010);
	write_cmos_sensor(0x0A1E, 0x0CCF);
	write_cmos_sensor(0x0A04, 0x014A);
	write_cmos_sensor(0x0418, 0x0000);
	write_cmos_sensor(0x0128, 0x0025);
	write_cmos_sensor(0x012A, 0xFFFF);
	write_cmos_sensor(0x0120, 0x0041);
	write_cmos_sensor(0x0122, 0x0371);
	write_cmos_sensor(0x012C, 0x001E);
	write_cmos_sensor(0x012E, 0xFFFF);
	write_cmos_sensor(0x0124, 0x003B);
	write_cmos_sensor(0x0126, 0x0373);
	write_cmos_sensor(0x0B02, 0xE04D);
	write_cmos_sensor(0x0B10, 0x6821);
	write_cmos_sensor(0x0B12, 0x0030);
	write_cmos_sensor(0x0B14, 0x0001);
	write_cmos_sensor(0x0A0A, 0x38FD);
	write_cmos_sensor(0x0A1C, 0x0000);
	write_cmos_sensor(0x0710, 0x09B0);
	write_cmos_sensor(0x0900, 0x0300);
	write_cmos_sensor(0x0902, 0xC319);
	write_cmos_sensor(0x0914, 0xC109);
	write_cmos_sensor(0x0916, 0x061A);
	write_cmos_sensor(0x0918, 0x0407);
	write_cmos_sensor(0x091A, 0x0A0B);
	write_cmos_sensor(0x091C, 0x0E08);
	write_cmos_sensor(0x091E, 0x0A00);
	write_cmos_sensor(0x090C, 0x03FA);
	write_cmos_sensor(0x090E, 0x0030);
	write_cmos_sensor(0x0F08, 0x2F04);
	write_cmos_sensor(0x0F30, 0x001F);
	write_cmos_sensor(0x0F36, 0x001F);
	write_cmos_sensor(0x0F04, 0x3A00);
	write_cmos_sensor(0x0F32, 0x0253);
	write_cmos_sensor(0x0F38, 0x059D);
	write_cmos_sensor(0x0F2A, 0x4124);
	write_cmos_sensor(0x004C, 0x0100);
#endif
}

#if MULTI_WRITE
kal_uint16 addr_data_pair_capture_30fps_sr846[] = {
	0x0A20, 0x0000,
	0x002E, 0x1111,
	0x0032, 0x1111,
	0x0022, 0x0008,
	0x0026, 0x0040,
	0x0028, 0x0017,
	0x002C, 0x09CF,
	0x005C, 0x2101,
	0x0006, 0x09DB,
	0x0008, 0x0ED8,
	0x000E, 0x0200,
	0x000C, 0x0022,
	0x0A22, 0x0000,
	0x0A24, 0x0000,
	0x0804, 0x0000,
	0x0A12, 0x0CC0,
	0x0A14, 0x0990,
	0x0074, 0x09D5,
	0x0076, 0x0000,
	0x0A0C, 0x0010,
	0x0A1E, 0x0CCF,
	0x0A04, 0x014A,
	0x0418, 0x0000,
	0x0128, 0x0025,
	0x012A, 0xFFFF,
	0x0120, 0x0041,
	0x0122, 0x0371,
	0x012C, 0x001E,
	0x012E, 0xFFFF,
	0x0124, 0x003B,
	0x0126, 0x0373,
	0x0B02, 0xE04D,
	0x0B10, 0x6821,
	0x0B12, 0x0030,
	0x0B14, 0x0001,
	0x0A0A, 0x38FD,
	0x0A1C, 0x0000,
	0x0710, 0x09B0,
	0x0900, 0x0300,
	0x0902, 0xC319,
	0x0914, 0xC109,
	0x0916, 0x061A,
	0x0918, 0x0407,
	0x091A, 0x0A0B,
	0x091C, 0x0E08,
	0x091E, 0x0A00,
	0x090C, 0x03FA,
	0x090E, 0x0030,
	0x0F08, 0x2F04,
	0x0F30, 0x001F,
	0x0F36, 0x001F,
	0x0F04, 0x3A00,
	0x0F32, 0x0253,
	0x0F38, 0x059D,
	0x0F2A, 0x4124,
	0x004C, 0x0100
};
#endif


static void capture_setting(kal_uint16 currefps)
{
#if MULTI_WRITE
	sr846_table_write_cmos_sensor(
		addr_data_pair_capture_30fps_sr846,
		sizeof(addr_data_pair_capture_30fps_sr846) /
		sizeof(kal_uint16));
#else
	write_cmos_sensor(0x0A20, 0x0000);
	write_cmos_sensor(0x002E, 0x1111);
	write_cmos_sensor(0x0032, 0x1111);
	write_cmos_sensor(0x0022, 0x0008);
	write_cmos_sensor(0x0026, 0x0040);
	write_cmos_sensor(0x0028, 0x0017);
	write_cmos_sensor(0x002C, 0x09CF);
	write_cmos_sensor(0x005C, 0x2101);
	write_cmos_sensor(0x0006, 0x09DB);
	write_cmos_sensor(0x0008, 0x0ED8);
	write_cmos_sensor(0x000E, 0x0200);
	write_cmos_sensor(0x000C, 0x0022);
	write_cmos_sensor(0x0A22, 0x0000);
	write_cmos_sensor(0x0A24, 0x0000);
	write_cmos_sensor(0x0804, 0x0000);
	write_cmos_sensor(0x0A12, 0x0CC0);
	write_cmos_sensor(0x0A14, 0x0990);
	write_cmos_sensor(0x0074, 0x09D5);
	write_cmos_sensor(0x0076, 0x0000);
	write_cmos_sensor(0x0A0C, 0x0010);
	write_cmos_sensor(0x0A1E, 0x0CCF);
	write_cmos_sensor(0x0A04, 0x014A);
	write_cmos_sensor(0x0418, 0x0000);
	write_cmos_sensor(0x0128, 0x0025);
	write_cmos_sensor(0x012A, 0xFFFF);
	write_cmos_sensor(0x0120, 0x0041);
	write_cmos_sensor(0x0122, 0x0371);
	write_cmos_sensor(0x012C, 0x001E);
	write_cmos_sensor(0x012E, 0xFFFF);
	write_cmos_sensor(0x0124, 0x003B);
	write_cmos_sensor(0x0126, 0x0373);
	write_cmos_sensor(0x0B02, 0xE04D);
	write_cmos_sensor(0x0B10, 0x6821);
	write_cmos_sensor(0x0B12, 0x0030);
	write_cmos_sensor(0x0B14, 0x0001);
	write_cmos_sensor(0x0A0A, 0x38FD);
	write_cmos_sensor(0x0A1C, 0x0000);
	write_cmos_sensor(0x0710, 0x09B0);
	write_cmos_sensor(0x0900, 0x0300);
	write_cmos_sensor(0x0902, 0xC319);
	write_cmos_sensor(0x0914, 0xC109);
	write_cmos_sensor(0x0916, 0x061A);
	write_cmos_sensor(0x0918, 0x0407);
	write_cmos_sensor(0x091A, 0x0A0B);
	write_cmos_sensor(0x091C, 0x0E08);
	write_cmos_sensor(0x091E, 0x0A00);
	write_cmos_sensor(0x090C, 0x03FA);
	write_cmos_sensor(0x090E, 0x0030);
	write_cmos_sensor(0x0F08, 0x2F04);
	write_cmos_sensor(0x0F30, 0x001F);
	write_cmos_sensor(0x0F36, 0x001F);
	write_cmos_sensor(0x0F04, 0x3A00);
	write_cmos_sensor(0x0F32, 0x0253);
	write_cmos_sensor(0x0F38, 0x059D);
	write_cmos_sensor(0x0F2A, 0x4124);
	write_cmos_sensor(0x004C, 0x0100);
#endif
}

#if MULTI_WRITE
kal_uint16 addr_data_pair_hs_video_sr846[] = {
	0x0A20, 0x0000,
	0x002E, 0x1111,
	0x0032, 0x1111,
	0x0022, 0x0008,
	0x0026, 0x0172,
	0x0028, 0x0017,
	0x002C, 0x089D,
	0x005C, 0x2101,
	0x0006, 0x09DB,
	0x0008, 0x0ED8,
	0x000E, 0x0200,
	0x000C, 0x0022,
	0x0A22, 0x0000,
	0x0A24, 0x0000,
	0x0804, 0x0000,
	0x0A12, 0x0CC0,
	0x0A14, 0x072C,
	0x0074, 0x09D5,
	0x0076, 0x0000,
	0x0A0C, 0x0010,
	0x0A1E, 0x0CCF,
	0x0A04, 0x014A,
	0x0418, 0x023E,
	0x0128, 0x0025,
	0x012A, 0xFFFF,
	0x0120, 0x0041,
	0x0122, 0x0371,
	0x012C, 0x001E,
	0x012E, 0xFFFF,
	0x0124, 0x003B,
	0x0126, 0x0373,
	0x0B02, 0xE04D,
	0x0B10, 0x6821,
	0x0B12, 0x0030,
	0x0B14, 0x0001,
	0x0A0A, 0x38FD,
	0x0A1C, 0x0000,
	0x0710, 0x074C,
	0x0900, 0x0300,
	0x0902, 0xC319,
	0x0914, 0xC109,
	0x0916, 0x061A,
	0x0918, 0x0407,
	0x091A, 0x0A0B,
	0x091C, 0x0E08,
	0x091E, 0x0A00,
	0x090C, 0x03FA,
	0x090E, 0x0030,
	0x0F08, 0x2F04,
	0x0F30, 0x001F,
	0x0F36, 0x001F,
	0x0F04, 0x3A00,
	0x0F32, 0x0253,
	0x0F38, 0x059D,
	0x0F2A, 0x4124,
	0x004C, 0x0100
};
#endif

static void hs_video_setting(void)
{
#if MULTI_WRITE
	sr846_table_write_cmos_sensor(
		addr_data_pair_hs_video_sr846,
		sizeof(addr_data_pair_hs_video_sr846) /
		sizeof(kal_uint16));
#else
	write_cmos_sensor(0x0A20, 0x0000);
	write_cmos_sensor(0x002E, 0x1111);
	write_cmos_sensor(0x0032, 0x1111);
	write_cmos_sensor(0x0022, 0x0008);
	write_cmos_sensor(0x0026, 0x0172);
	write_cmos_sensor(0x0028, 0x0017);
	write_cmos_sensor(0x002C, 0x089D);
	write_cmos_sensor(0x005C, 0x2101);
	write_cmos_sensor(0x0006, 0x09DB);
	write_cmos_sensor(0x0008, 0x0ED8);
	write_cmos_sensor(0x000E, 0x0200);
	write_cmos_sensor(0x000C, 0x0022);
	write_cmos_sensor(0x0A22, 0x0000);
	write_cmos_sensor(0x0A24, 0x0000);
	write_cmos_sensor(0x0804, 0x0000);
	write_cmos_sensor(0x0A12, 0x0CC0);
	write_cmos_sensor(0x0A14, 0x072C);
	write_cmos_sensor(0x0074, 0x09D5);
	write_cmos_sensor(0x0076, 0x0000);
	write_cmos_sensor(0x0A0C, 0x0010);
	write_cmos_sensor(0x0A1E, 0x0CCF);
	write_cmos_sensor(0x0A04, 0x014A);
	write_cmos_sensor(0x0418, 0x023E);
	write_cmos_sensor(0x0128, 0x0025);
	write_cmos_sensor(0x012A, 0xFFFF);
	write_cmos_sensor(0x0120, 0x0041);
	write_cmos_sensor(0x0122, 0x0371);
	write_cmos_sensor(0x012C, 0x001E);
	write_cmos_sensor(0x012E, 0xFFFF);
	write_cmos_sensor(0x0124, 0x003B);
	write_cmos_sensor(0x0126, 0x0373);
	write_cmos_sensor(0x0B02, 0xE04D);
	write_cmos_sensor(0x0B10, 0x6821);
	write_cmos_sensor(0x0B12, 0x0030);
	write_cmos_sensor(0x0B14, 0x0001);
	write_cmos_sensor(0x0A0A, 0x38FD);
	write_cmos_sensor(0x0A1C, 0x0000);
	write_cmos_sensor(0x0710, 0x074C);
	write_cmos_sensor(0x0900, 0x0300);
	write_cmos_sensor(0x0902, 0xC319);
	write_cmos_sensor(0x0914, 0xC109);
	write_cmos_sensor(0x0916, 0x061A);
	write_cmos_sensor(0x0918, 0x0407);
	write_cmos_sensor(0x091A, 0x0A0B);
	write_cmos_sensor(0x091C, 0x0E08);
	write_cmos_sensor(0x091E, 0x0A00);
	write_cmos_sensor(0x090C, 0x03FA);
	write_cmos_sensor(0x090E, 0x0030);
	write_cmos_sensor(0x0F08, 0x2F04);
	write_cmos_sensor(0x0F30, 0x001F);
	write_cmos_sensor(0x0F36, 0x001F);
	write_cmos_sensor(0x0F04, 0x3A00);
	write_cmos_sensor(0x0F32, 0x0253);
	write_cmos_sensor(0x0F38, 0x059D);
	write_cmos_sensor(0x0F2A, 0x4124);
	write_cmos_sensor(0x004C, 0x0100);
#endif
}

#if MULTI_WRITE
kal_uint16 addr_data_pair_slim_video_sr846[] = {
	0x0b0a, 0x8252,
	0x0f30, 0x6e25,
	0x0f32, 0x7167,
	0x004a, 0x0100,
	0x004c, 0x0000,
	0x004e, 0x0100,
	0x000c, 0x0022,
	0x0008, 0x0b00,
	0x005a, 0x0204,
	0x0012, 0x001c,
	0x0018, 0x0a23,
	0x0022, 0x0008,
	0x0028, 0x0017,
	0x0024, 0x0122,
	0x002a, 0x0127,
	0x0026, 0x012c,
	0x002c, 0x06cb,
	0x002e, 0x1111,
	0x0030, 0x1111,
	0x0032, 0x3311,
	0x0006, 0x0823,
	0x0a22, 0x0000,
	0x0a12, 0x0500,
	0x0a14, 0x02d0,
	0x003e, 0x0000,
	0x0074, 0x0821,
	0x0070, 0x0411,
	0x0804, 0x0200,
	0x0a04, 0x016a,
	0x090e, 0x0010,
	0x090c, 0x09c0,
	0x0902, 0x4319,
	0x0914, 0xc106,
	0x0916, 0x040e,
	0x0918, 0x0304,
	0x091c, 0x0e06,
	0x091a, 0x0709,
	0x091e, 0x0300
};
#endif


static void slim_video_setting(void)
{
	//Sensor Information////////////////////////////
	//Sensor	  : hi-556
	//Date		  : 2016-10-19
	//Customer        : MTK_validation
	//Image size	  : 1280x720
	//MCLK		  : 24MHz
	//MIPI speed(Mbps): 440Mbps x 2Lane
	//Frame Length	  : 2083
	//Line Length	  : 2816
	//Max Fps	  : 30.0fps
	//Pixel order	  : Green 1st (=GB)
	//X/Y-flip	  : X-flip
	//BLC offset	  : 64code
	////////////////////////////////////////////////
#if MULTI_WRITE
	sr846_table_write_cmos_sensor(
		addr_data_pair_slim_video_sr846,
		sizeof(addr_data_pair_slim_video_sr846) /
		sizeof(kal_uint16));
#else
	write_cmos_sensor(0x0b0a, 0x8252);
	write_cmos_sensor(0x0f30, 0x6e25);
	write_cmos_sensor(0x0f32, 0x7167);
	write_cmos_sensor(0x004a, 0x0100);
	write_cmos_sensor(0x004c, 0x0000);
	write_cmos_sensor(0x004e, 0x0100); //perframe enable
	write_cmos_sensor(0x000c, 0x0022);
	write_cmos_sensor(0x0008, 0x0b00);
	write_cmos_sensor(0x005a, 0x0204);
	write_cmos_sensor(0x0012, 0x001c);
	write_cmos_sensor(0x0018, 0x0a23);
	write_cmos_sensor(0x0022, 0x0008);
	write_cmos_sensor(0x0028, 0x0017);
	write_cmos_sensor(0x0024, 0x0122);
	write_cmos_sensor(0x002a, 0x0127);
	write_cmos_sensor(0x0026, 0x012c);
	write_cmos_sensor(0x002c, 0x06cb);
	write_cmos_sensor(0x002e, 0x1111);
	write_cmos_sensor(0x0030, 0x1111);
	write_cmos_sensor(0x0032, 0x3311);
	write_cmos_sensor(0x0006, 0x0823);
	write_cmos_sensor(0x0a22, 0x0000);
	write_cmos_sensor(0x0a12, 0x0500);
	write_cmos_sensor(0x0a14, 0x02d0);
	write_cmos_sensor(0x003e, 0x0000);
	write_cmos_sensor(0x0074, 0x0821);
	write_cmos_sensor(0x0070, 0x0411);
	write_cmos_sensor(0x0804, 0x0200);
	write_cmos_sensor(0x0a04, 0x016a);
	write_cmos_sensor(0x090e, 0x0010);
	write_cmos_sensor(0x090c, 0x09c0);
	//===============================================
	//             mipi 2 lane 440Mbps
	//===============================================
	write_cmos_sensor(0x0902, 0x4319);
	write_cmos_sensor(0x0914, 0xc106);
	write_cmos_sensor(0x0916, 0x040e);
	write_cmos_sensor(0x0918, 0x0304);
	write_cmos_sensor(0x091c, 0x0e06);
	write_cmos_sensor(0x091a, 0x0709);
	write_cmos_sensor(0x091e, 0x0300);
#endif
}

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
			if (*sensor_id == imgsensor_info.sensor_id) {
				LOG_INF(
					"i2c write id : 0x%x, sensor id: 0x%x\n",
					imgsensor.i2c_write_id, *sensor_id);
				return ERROR_NONE;
			}

			retry--;
		} while (retry > 0);
		i++;
		retry = 2;
	}
	if (*sensor_id != imgsensor_info.sensor_id) {
		LOG_INF("Read id fail,sensor id: 0x%x\n", *sensor_id);
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

	LOG_INF("[%s]: PLATFORM:MT6737,MIPI 24LANE\n", __func__);
	LOG_INF("preview 1296*972@30fps,360Mbps/lane;"
		"capture 2592*1944@30fps,880Mbps/lane\n");
	while (imgsensor_info.i2c_addr_table[i] != 0xff) {
		spin_lock(&imgsensor_drv_lock);
		imgsensor.i2c_write_id = imgsensor_info.i2c_addr_table[i];
		spin_unlock(&imgsensor_drv_lock);
		do {
			sensor_id = return_sensor_id();
			if (sensor_id == imgsensor_info.sensor_id) {
				LOG_INF("i2c write id: 0x%x, sensor id: 0x%x\n",
					imgsensor.i2c_write_id, sensor_id);
				break;
			}

			retry--;
		} while (retry > 0);
		i++;
		if (sensor_id == imgsensor_info.sensor_id)
			break;
		retry = 2;
	}
	if (imgsensor_info.sensor_id != sensor_id) {
		LOG_INF("%s sensor id fail: 0x%x\n", __func__, sensor_id);
		return ERROR_SENSOR_CONNECT_FAIL;
	}
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
	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_CAPTURE;

	if (imgsensor.current_fps == imgsensor_info.cap.max_framerate)	{
		imgsensor.pclk = imgsensor_info.cap.pclk;
		imgsensor.line_length = imgsensor_info.cap.linelength;
		imgsensor.frame_length = imgsensor_info.cap.framelength;
		imgsensor.min_frame_length = imgsensor_info.cap.framelength;
		imgsensor.autoflicker_en = KAL_FALSE;
	} else {
	 //PIP capture: 24fps for less than 13M, 20fps for 16M,15fps for 20M
		imgsensor.pclk = imgsensor_info.cap1.pclk;
		imgsensor.line_length = imgsensor_info.cap1.linelength;
		imgsensor.frame_length = imgsensor_info.cap1.framelength;
		imgsensor.min_frame_length = imgsensor_info.cap1.framelength;
		imgsensor.autoflicker_en = KAL_FALSE;
	}

	spin_unlock(&imgsensor_drv_lock);
	LOG_INF("Caputre fps:%d\n", imgsensor.current_fps);
	capture_setting(imgsensor.current_fps);

	return ERROR_NONE;

}	/* capture() */
static kal_uint32 normal_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_VIDEO;
	imgsensor.pclk = imgsensor_info.normal_video.pclk;
	imgsensor.line_length = imgsensor_info.normal_video.linelength;
	imgsensor.frame_length = imgsensor_info.normal_video.framelength;
	imgsensor.min_frame_length = imgsensor_info.normal_video.framelength;
	imgsensor.current_fps = 300;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	capture_setting(imgsensor.current_fps);
	return ERROR_NONE;
}	/*	normal_video   */

static kal_uint32 hs_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
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

static kal_uint32 get_resolution(
		MSDK_SENSOR_RESOLUTION_INFO_STRUCT * sensor_resolution)
{
	sensor_resolution->SensorFullWidth =
		imgsensor_info.cap.grabwindow_width;
	sensor_resolution->SensorFullHeight =
		imgsensor_info.cap.grabwindow_height;

	sensor_resolution->SensorPreviewWidth =
		imgsensor_info.pre.grabwindow_width;
	sensor_resolution->SensorPreviewHeight =
		imgsensor_info.pre.grabwindow_height;

	sensor_resolution->SensorVideoWidth =
		imgsensor_info.normal_video.grabwindow_width;
	sensor_resolution->SensorVideoHeight =
		imgsensor_info.normal_video.grabwindow_height;


	sensor_resolution->SensorHighSpeedVideoWidth =
		imgsensor_info.hs_video.grabwindow_width;
	sensor_resolution->SensorHighSpeedVideoHeight =
		imgsensor_info.hs_video.grabwindow_height;

	sensor_resolution->SensorSlimVideoWidth =
		imgsensor_info.slim_video.grabwindow_width;
	sensor_resolution->SensorSlimVideoHeight =
		imgsensor_info.slim_video.grabwindow_height;
	return ERROR_NONE;
}    /*    get_resolution    */


static kal_uint32 get_info(enum MSDK_SCENARIO_ID_ENUM scenario_id,
			MSDK_SENSOR_INFO_STRUCT *sensor_info,
			MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("scenario_id = %d\n", scenario_id);

	sensor_info->SensorClockPolarity = SENSOR_CLOCK_POLARITY_LOW;
	sensor_info->SensorClockFallingPolarity = SENSOR_CLOCK_POLARITY_LOW;
	sensor_info->SensorHsyncPolarity = SENSOR_CLOCK_POLARITY_LOW;
	sensor_info->SensorVsyncPolarity = SENSOR_CLOCK_POLARITY_LOW;
	sensor_info->SensorInterruptDelayLines = 4; /* not use */
	sensor_info->SensorResetActiveHigh = FALSE; /* not use */
	sensor_info->SensorResetDelayCount = 5; /* not use */

	sensor_info->SensroInterfaceType =
	imgsensor_info.sensor_interface_type;
	sensor_info->MIPIsensorType = imgsensor_info.mipi_sensor_type;
	sensor_info->SettleDelayMode = imgsensor_info.mipi_settle_delay_mode;
	sensor_info->SensorOutputDataFormat =
		imgsensor_info.sensor_output_dataformat;

	sensor_info->CaptureDelayFrame = imgsensor_info.cap_delay_frame;
	sensor_info->PreviewDelayFrame = imgsensor_info.pre_delay_frame;
	sensor_info->VideoDelayFrame =
		imgsensor_info.video_delay_frame;
	sensor_info->HighSpeedVideoDelayFrame =
		imgsensor_info.hs_video_delay_frame;
	sensor_info->SlimVideoDelayFrame =
		imgsensor_info.slim_video_delay_frame;

	sensor_info->SensorMasterClockSwitch = 0; /* not use */
	sensor_info->SensorDrivingCurrent =
		imgsensor_info.isp_driving_current;
/* The frame of setting shutter default 0 for TG int */
	sensor_info->AEShutDelayFrame =
		imgsensor_info.ae_shut_delay_frame;
/* The frame of setting sensor gain */
	sensor_info->AESensorGainDelayFrame =
		imgsensor_info.ae_sensor_gain_delay_frame;
	sensor_info->AEISPGainDelayFrame =
		imgsensor_info.ae_ispGain_delay_frame;
	sensor_info->IHDR_Support = imgsensor_info.ihdr_support;
	sensor_info->IHDR_LE_FirstLine =
		imgsensor_info.ihdr_le_firstline;
	sensor_info->SensorModeNum =
		imgsensor_info.sensor_mode_num;

	sensor_info->SensorMIPILaneNumber =
		imgsensor_info.mipi_lane_num;
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

	switch (scenario_id) {
	case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		sensor_info->SensorGrabStartX = imgsensor_info.pre.startx;
		sensor_info->SensorGrabStartY = imgsensor_info.pre.starty;

		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
				imgsensor_info.pre.mipi_data_lp2hs_settle_dc;
		break;
	case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		sensor_info->SensorGrabStartX = imgsensor_info.cap.startx;
		sensor_info->SensorGrabStartY = imgsensor_info.cap.starty;

		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
			imgsensor_info.cap.mipi_data_lp2hs_settle_dc;
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

		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
			imgsensor_info.pre.mipi_data_lp2hs_settle_dc;
		break;
	}

	return ERROR_NONE;
}    /*    get_info  */


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
	default:
		preview(image_window, sensor_config_data);
		return ERROR_INVALID_SCENARIO_ID;
	}
	return ERROR_NONE;
}	/* control() */



static kal_uint32 set_video_mode(UINT16 framerate)
{
	LOG_INF("framerate = %d ", framerate);
	// SetVideoMode Function should fix framerate
	if (framerate == 0)
		// Dynamic frame rate
		return ERROR_NONE;
	spin_lock(&imgsensor_drv_lock);

	if ((framerate == 30) && (imgsensor.autoflicker_en == KAL_TRUE))
		imgsensor.current_fps = 296;
	else if ((framerate == 15) && (imgsensor.autoflicker_en == KAL_TRUE))
		imgsensor.current_fps = 146;
	else
		imgsensor.current_fps = 10 * framerate;
	spin_unlock(&imgsensor_drv_lock);
	set_max_framerate(imgsensor.current_fps, 1);
	set_dummy();
	return ERROR_NONE;
}


static kal_uint32 set_auto_flicker_mode(kal_bool enable,
			UINT16 framerate)
{
	LOG_INF("enable = %d, framerate = %d ", enable, framerate);
	spin_lock(&imgsensor_drv_lock);
	if (enable)
		imgsensor.autoflicker_en = KAL_TRUE;
	else //Cancel Auto flick
		imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	return ERROR_NONE;
}


static kal_uint32 set_max_framerate_by_scenario(
			enum MSDK_SCENARIO_ID_ENUM scenario_id,
			MUINT32 framerate)
{
	kal_uint32 frame_length;

	LOG_INF("scenario_id = %d, framerate = %d\n",
				scenario_id, framerate);

	switch (scenario_id) {
	case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		frame_length = imgsensor_info.pre.pclk / framerate * 10 /
			imgsensor_info.pre.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line = (frame_length >
			imgsensor_info.pre.framelength) ?
			(frame_length - imgsensor_info.pre.framelength) : 0;
		imgsensor.frame_length = imgsensor_info.pre.framelength +
			imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
		break;
	case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
		if (framerate == 0)
			return ERROR_NONE;
		frame_length = imgsensor_info.normal_video.pclk /
			framerate * 10 / imgsensor_info.normal_video.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line = (frame_length >
			imgsensor_info.normal_video.framelength) ?
		(frame_length - imgsensor_info.normal_video.framelength) : 0;
		imgsensor.frame_length = imgsensor_info.normal_video.framelength +
			imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
		break;
	case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		if (imgsensor.current_fps ==
			imgsensor_info.cap1.max_framerate) {
			frame_length = imgsensor_info.cap1.pclk /
				framerate * 10 /
				imgsensor_info.cap1.linelength;
			spin_lock(&imgsensor_drv_lock);
			imgsensor.dummy_line = (frame_length >
				imgsensor_info.cap1.framelength) ?
				(frame_length - imgsensor_info.cap1.framelength)
				: 0;
			imgsensor.frame_length = imgsensor_info.cap1.framelength
				+ imgsensor.dummy_line;
			imgsensor.min_frame_length = imgsensor.frame_length;
			spin_unlock(&imgsensor_drv_lock);
		} else {
			if (imgsensor.current_fps !=
				imgsensor_info.cap.max_framerate)
				LOG_INF(
					"fps %d fps not support,use cap: %d fps!\n",
					framerate,
					imgsensor_info.cap.max_framerate / 10);
			frame_length = imgsensor_info.cap.pclk /
				framerate * 10 / imgsensor_info.cap.linelength;
			spin_lock(&imgsensor_drv_lock);
			imgsensor.dummy_line = (frame_length >
				imgsensor_info.cap.framelength) ?
			(frame_length - imgsensor_info.cap.framelength) : 0;
			imgsensor.frame_length =
				imgsensor_info.cap.framelength +
				imgsensor.dummy_line;
			imgsensor.min_frame_length = imgsensor.frame_length;
			spin_unlock(&imgsensor_drv_lock);
		}
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
		break;
	case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
		frame_length = imgsensor_info.hs_video.pclk /
			framerate * 10 / imgsensor_info.hs_video.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line = (frame_length >
			imgsensor_info.hs_video.framelength) ? (frame_length -
			imgsensor_info.hs_video.framelength) : 0;
		imgsensor.frame_length = imgsensor_info.hs_video.framelength +
			imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
		break;
	case MSDK_SCENARIO_ID_SLIM_VIDEO:
		frame_length = imgsensor_info.slim_video.pclk /
			framerate * 10 / imgsensor_info.slim_video.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line = (frame_length >
			imgsensor_info.slim_video.framelength) ? (frame_length -
			imgsensor_info.slim_video.framelength) : 0;
		imgsensor.frame_length =
			imgsensor_info.slim_video.framelength +
			imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
		break;
	default:  //coding with  preview scenario by default
		frame_length = imgsensor_info.pre.pclk / framerate * 10 /
						imgsensor_info.pre.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line = (frame_length >
			imgsensor_info.pre.framelength) ?
			(frame_length - imgsensor_info.pre.framelength) : 0;
		imgsensor.frame_length = imgsensor_info.pre.framelength +
				imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
		LOG_INF("error scenario_id = %d, we use preview scenario\n",
				scenario_id);
		break;
	}
	return ERROR_NONE;
}


static kal_uint32 get_default_framerate_by_scenario(
				enum MSDK_SCENARIO_ID_ENUM scenario_id,
				MUINT32 *framerate)
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
	LOG_INF("%s enable: %d", __func__, enable);

	if (enable) {
// 0x5E00[8]: 1 enable,  0 disable
// 0x5E00[1:0]; 00 Color bar, 01 Random Data, 10 Square, 11 BLACK
		write_cmos_sensor(0x0a04, 0x0143);
		write_cmos_sensor(0x0200, 0x0002);
	} else {
// 0x5E00[8]: 1 enable,  0 disable
// 0x5E00[1:0]; 00 Color bar, 01 Random Data, 10 Square, 11 BLACK
		write_cmos_sensor(0x0a04, 0x0142);
		write_cmos_sensor(0x0200, 0x0000);
	}
	spin_lock(&imgsensor_drv_lock);
	imgsensor.test_pattern = enable;
	spin_unlock(&imgsensor_drv_lock);
	return ERROR_NONE;
}

static kal_uint32 streaming_control(kal_bool enable)
{
	LOG_INF("streaming_enable(0=Sw Standby,1=streaming): %d\n", enable);

	if (enable)
		write_cmos_sensor(0x0A20, 0x0100);
	else {
		write_cmos_sensor(0x0F10, 0x0A00);
		write_cmos_sensor(0x0A20, 0x0000);
	}

	usleep_range(10000, 10100);
	return ERROR_NONE;
}

static kal_uint32 feature_control(
			MSDK_SENSOR_FEATURE_ENUM feature_id,
			UINT8 *feature_para, UINT32 *feature_para_len)
{
	UINT16 *feature_return_para_16 = (UINT16 *) feature_para;
	UINT16 *feature_data_16 = (UINT16 *) feature_para;
	UINT32 *feature_return_para_32 = (UINT32 *) feature_para;
	UINT32 *feature_data_32 = (UINT32 *) feature_para;
	INT32 *feature_return_para_i32 = (INT32 *) feature_para;
	unsigned long long *feature_data =
		(unsigned long long *) feature_para;

	struct SENSOR_WINSIZE_INFO_STRUCT *wininfo;
	MSDK_SENSOR_REG_INFO_STRUCT *sensor_reg_data =
		(MSDK_SENSOR_REG_INFO_STRUCT *) feature_para;

	LOG_INF("feature_id = %d\n", feature_id);
	switch (feature_id) {
	case SENSOR_FEATURE_GET_PERIOD:
		*feature_return_para_16++ = imgsensor.line_length;
		*feature_return_para_16 = imgsensor.frame_length;
		*feature_para_len = 4;
		break;
	case SENSOR_FEATURE_GET_PIXEL_CLOCK_FREQ:
		*feature_return_para_32 = imgsensor.pclk;
		*feature_para_len = 4;
		break;
	case SENSOR_FEATURE_SET_ESHUTTER:
		set_shutter(*feature_data);
		break;
	case SENSOR_FEATURE_SET_NIGHTMODE:
		night_mode((BOOL) * feature_data);
		break;
	case SENSOR_FEATURE_SET_GAIN:
		set_gain((UINT16) *feature_data);
		break;
	case SENSOR_FEATURE_SET_FLASHLIGHT:
		break;
	case SENSOR_FEATURE_SET_ISP_MASTER_CLOCK_FREQ:
		break;
	case SENSOR_FEATURE_SET_REGISTER:
		write_cmos_sensor(sensor_reg_data->RegAddr,
						sensor_reg_data->RegData);
		break;
	case SENSOR_FEATURE_GET_REGISTER:
		sensor_reg_data->RegData =
				read_cmos_sensor(sensor_reg_data->RegAddr);
		break;
	case SENSOR_FEATURE_GET_LENS_DRIVER_ID:
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
		set_auto_flicker_mode((BOOL)*feature_data_16,
			*(feature_data_16+1));
		break;
	case SENSOR_FEATURE_SET_MAX_FRAME_RATE_BY_SCENARIO:
		set_max_framerate_by_scenario(
			(enum MSDK_SCENARIO_ID_ENUM)*feature_data,
			*(feature_data+1));
		break;
	case SENSOR_FEATURE_GET_DEFAULT_FRAME_RATE_BY_SCENARIO:
		get_default_framerate_by_scenario(
			(enum MSDK_SCENARIO_ID_ENUM)*(feature_data),
			(MUINT32 *)(uintptr_t)(*(feature_data+1)));
		break;
	case SENSOR_FEATURE_SET_TEST_PATTERN:
		set_test_pattern_mode((BOOL)*feature_data);
		break;
	case SENSOR_FEATURE_GET_TEST_PATTERN_CHECKSUM_VALUE:
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
		imgsensor.ihdr_en = (BOOL)*feature_data;
		spin_unlock(&imgsensor_drv_lock);
		break;
	case SENSOR_FEATURE_GET_CROP_INFO:
		LOG_INF("SENSOR_FEATURE_GET_CROP_INFO scenarioId:%d\n",
				(UINT32)*feature_data);

		wininfo = (struct SENSOR_WINSIZE_INFO_STRUCT *)
			(uintptr_t)(*(feature_data+1));

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
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		default:
			memcpy((void *)wininfo,
				(void *)&imgsensor_winsize_info[0],
				sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		}
		break;
	case SENSOR_FEATURE_SET_IHDR_SHUTTER_GAIN:
		LOG_INF("SENSOR_SET_SENSOR_IHDR LE=%d, SE=%d, Gain=%d\n",
			(UINT16)*feature_data, (UINT16)*(feature_data+1),
			(UINT16)*(feature_data+2));
		/* ihdr_write_shutter_gain((UINT16)*feature_data,
		 *	(UINT16)*(feature_data+1), (UINT16)*(feature_data+2));
		 */
		break;
	case SENSOR_FEATURE_GET_TEMPERATURE_VALUE:
		*feature_return_para_i32 = 0;
		*feature_para_len = 4;
		break;
	case SENSOR_FEATURE_SET_STREAMING_SUSPEND:
		streaming_control(KAL_FALSE);
		break;
	case SENSOR_FEATURE_SET_STREAMING_RESUME:
		if (*feature_data != 0)
			set_shutter(*feature_data);
		streaming_control(KAL_TRUE);
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

UINT32 SR846_MIPI_RAW_SensorInit(struct SENSOR_FUNCTION_STRUCT **pfFunc)
{
	/* To Do : Check Sensor status here */
	if (pfFunc != NULL)
		*pfFunc =  &sensor_func;
	return ERROR_NONE;
}	/*	SR846_MIPI_RAW_SensorInit	*/
