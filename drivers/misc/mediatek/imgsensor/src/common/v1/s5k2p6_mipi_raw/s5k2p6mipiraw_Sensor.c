// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

/*****************************************************************************
 *
 * Filename:
 * ---------
 *	 s5k2p6mipi_Sensor.c
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
 *----------------------------------------------------------------------------
 * Upper this line, this part is controlled by CC/CQ. DO NOT MODIFY!!
 *============================================================================
 ****************************************************************************/
#define PFX "S5K2P6_camera_sensor"
#define LOG_INF(format, args...) pr_debug(PFX "[%s] " format, __func__, ##args)

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

#include "s5k2p6mipiraw_Sensor.h"


#undef ORINGNAL_VERSION



#define MULTI_WRITE 1

#if MULTI_WRITE
#define I2C_BUFFER_LEN 1020	/* trans# max is 255, each 4 bytes */
#else
#define I2C_BUFFER_LEN 4
#endif


static DEFINE_SPINLOCK(imgsensor_drv_lock);


static struct imgsensor_info_struct imgsensor_info = {
	.sensor_id = S5K2P6_SENSOR_ID,

	.checksum_value = 0xb1f1b3cc,

	.pre = {
		.pclk = 560440000,
		.linelength = 5160,
		.framelength = 3610,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 4608,
		.grabwindow_height = 3456,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 300,
		.mipi_pixel_rate = 577200000,
	},
	.cap = {
		.pclk = 560440000,
		.linelength = 5160,
		.framelength = 3610,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 4608,
		.grabwindow_height = 3456,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 300,
		.mipi_pixel_rate = 577200000,
	},
	.cap1 = {
		.pclk = 560000000,
		.linelength = 6416,
		.framelength = 3610,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 4640,
		.grabwindow_height = 3488,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 240,
		.mipi_pixel_rate = 443000000,
	},
	.cap2 = {
		.pclk = 560000000,
		.linelength = 10240,
		.framelength = 3610,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 4640,
		.grabwindow_height = 3488,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 150,
		.mipi_pixel_rate = 261000000,
	},
	.normal_video = {
		.pclk = 560440000,
		.linelength = 5120,
		.framelength = 3610,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 4608,
		.grabwindow_height = 2592,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 300,
		.mipi_pixel_rate = 577200000,
	},
	.hs_video = {
		.pclk = 560440000,
		.linelength = 5120,
		.framelength = 3610,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 4608,
		.grabwindow_height = 2592,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 300,
		.mipi_pixel_rate = 577200000,
	},
	.slim_video = {
		.pclk = 560440000,
		.linelength = 5120,
		.framelength = 3610,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 4608,
		.grabwindow_height = 2592,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 300,
		.mipi_pixel_rate = 577200000,
	},
	.custom1 = {
		.pclk = 571430400,
		.linelength = 5176,  //26M mode 8 0x1438 =  5176
		.framelength = 920 * 2, //    0x0398 =  920
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 1152,
		.grabwindow_height = 864,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 600,
		.mipi_pixel_rate = 577200000,
	},
	.margin = 4,                    /*sensor framelength & shutter margin*/
	.min_shutter = 4,               /*min shutter*/

	/*max framelength by sensor register's limitation*/
	.max_frame_length = 0xffff,
	/*shutter delay frame for AE cycle, 2 frame*/
	.ae_shut_delay_frame = 0,
	/*sensor gain delay frame for AE cycle,2 frame*/
	.ae_sensor_gain_delay_frame = 0,
	.ae_ispGain_delay_frame = 2,    /*isp gain delay frame for AE cycle*/
	.ihdr_support = 0,	            /*1, support; 0,not support*/
	.ihdr_le_firstline = 0,         /*1,le first ; 0, se first*/
	.sensor_mode_num = 6,	        /*support sensor mode num*/

	.cap_delay_frame = 3,           /*enter capture delay frame num*/
	.pre_delay_frame = 3,           /*enter preview delay frame num*/
	.video_delay_frame = 3,         /*enter video delay frame num*/
	.hs_video_delay_frame = 3,   /*enter high speed video  delay frame num*/
	.slim_video_delay_frame = 3, /*enter slim video delay frame num*/
	.custom1_delay_frame = 3,

	.isp_driving_current = ISP_DRIVING_6MA,     /*mclk driving current*/

	/*sensor_interface_type*/
	.sensor_interface_type = SENSOR_INTERFACE_TYPE_MIPI,
	/*0,MIPI_OPHY_NCSI2;  1,MIPI_OPHY_CSI2*/
	.mipi_sensor_type = MIPI_OPHY_NCSI2,
	/*0,MIPI_SETTLEDELAY_AUTO; 1,MIPI_SETTLEDELAY_MANNUAL*/
	.mipi_settle_delay_mode = 0,
	.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_Gr,
	.mclk = 26,         /*mclk value, suggest 24 or 26 for 24Mhz or 26Mhz*/
	.mipi_lane_num = SENSOR_MIPI_4_LANE,

	/*record sensor support all write id addr*/
	.i2c_addr_table = {0x5a, 0xff},
	.i2c_speed = 300,
};


static struct imgsensor_struct imgsensor = {
	.mirror = IMAGE_NORMAL,		/*mirrorflip information*/

	/*IMGSENSOR_MODE enum value,record current sensor mode*/
	.sensor_mode = IMGSENSOR_MODE_INIT,
	.shutter = 0x3D0,			/*current shutter*/
	.gain = 0x100,				/*current gain*/
	.dummy_pixel = 0,			/*current dummypixel*/
	.dummy_line = 0,			/*current dummyline*/

	/*full size current fps : 24fps for PIP, 30fps for Normal or ZSD*/
	.current_fps = 0,
	/*auto flicker enable: KAL_FALSE for disable auto flicker*/
	.autoflicker_en = KAL_FALSE,
	/*test pattern mode or not. KAL_FALSE for in test pattern mode*/
	.test_pattern = KAL_FALSE,
	/*current scenario id*/
	.current_scenario_id = MSDK_SCENARIO_ID_CAMERA_PREVIEW,
	/*sensor need support LE, SE with HDR feature*/
	.ihdr_mode = KAL_FALSE,

	.i2c_write_id = 0x5A,  /*record current sensor's i2c write id*/

};

/* VC_Num, VC_PixelNum, ModeSelect, EXPO_Ratio, ODValue, RG_STATSMODE*/
/* VC0_ID, VC0_DataType, VC0_SIZEH, VC0_SIZE,
 * VC1_ID, VC1_DataType, VC1_SIZEH, VC1_SIZEV
 */

/* VC2_ID, VC2_DataType, VC2_SIZEH, VC2_SIZE,
 * VC3_ID, VC3_DataType, VC3_SIZEH, VC3_SIZEV
 */

/* Preview mode setting */
static struct SENSOR_VC_INFO_STRUCT SENSOR_VC_INFO[6] = {
	/* Preview mode setting */
	{0x02, 0x0A,   0x00,   0x08, 0x40, 0x00,
	0x00, 0x2B, 0x1200, 0x0D80, 0x01, 0x00, 0x0000, 0x0000,
	0x01, 0x2B, 0x0168, 0x06C0, 0x03, 0x00, 0x0000, 0x0000}, /*288*1728(by pixel) => 360*1728 (by byte)*/
	/* Capture mode setting */
	{0x02, 0x0A,   0x00,   0x08, 0x40, 0x00,
	0x00, 0x2B, 0x1200, 0x0D80, 0x01, 0x00, 0x0000, 0x0000,
	0x02, 0x00, 0x0000, 0x0000, 0x03, 0x00, 0x0000, 0x0000},
	/* Video mode setting */
	{0x02, 0x0A,   0x00,   0x08, 0x40, 0x00,
	0x00, 0x2B, 0x1200, 0x0A20, 0x01, 0x00, 0x0000, 0x0000,
	0x01, 0x2B, 0x0168, 0x0510, 0x03, 0x00, 0x0000, 0x0000},/*288*1296(by pixel) => 360*1296(by byte)*/
	/* Hs mode setting */
	{0x02, 0x0A,   0x00,   0x08, 0x40, 0x00,
	0x00, 0x2B, 0x1200, 0x0A20, 0x01, 0x00, 0x0000, 0x0000,
	0x02, 0x30, 0x0000, 0x0000, 0x03, 0x00, 0x0000, 0x0000},
	/* Slim-video mode setting */
	{0x02, 0x0A,   0x00,   0x08, 0x40, 0x00,
	0x00, 0x2B, 0x1200, 0x0A20, 0x01, 0x00, 0x0000, 0x0000,
	0x02, 0x30, 0x0000, 0x0000, 0x03, 0x00, 0x0000, 0x0000},
	/* Custom1 mode setting not used*/
	{0x02, 0x0A,   0x00,   0x08, 0x40, 0x00,
	0x00, 0x2B, 0x480, 0x0360, 0x01, 0x00, 0x0000, 0x0000,
	0x02, 0x30, 0x0000, 0x0000, 0x03, 0x00, 0x0000, 0x0000},
	};

/* Sensor output window information */
static struct SENSOR_WINSIZE_INFO_STRUCT imgsensor_winsize_info[6] = {
	//Align with PD cali full size (4608,3456) offset (0,0)
	/*preview*/
	{ 4608, 3456, 0, 0, 4608, 3456, 4608, 3456,
	  0000, 0000, 4608, 3456, 0, 0, 4608, 3456},
	/*capture*/
	{ 4608, 3456, 0, 0, 4608, 3456, 4608, 3456,
	  0000, 0000, 4608, 3456, 0, 0, 4608, 3456},
	/*Video*/
	{ 4608, 3456, 0, 432, 4608, 2592, 4608, 2592,
	  0000, 0000, 4608, 2592, 0, 0, 4608, 2592},
	/*hs Video*/
	{ 4608, 3456, 0, 432, 4608, 2592, 4608, 2592,
	  0000, 0000, 4608, 2592, 0, 0, 4608, 2592},
	/*slim Video*/
	{ 4608, 3456, 0, 432, 4608, 2592, 4608, 2592,
	  0000, 0000, 4608, 2592, 0, 0, 4608, 2592},
	/* Custom1*///not use
	{ 4640, 3488, 16, 16, 4208, 3120, 1152, 864,
	  0000, 0000, 1152, 864, 0, 0, 1152, 864},
};

/*no mirror flip*/
static struct SET_PD_BLOCK_INFO_T imgsensor_pd_info = {
	.i4OffsetX = 0,
	.i4OffsetY = 0,
	.i4PitchX  = 32,
	.i4PitchY  = 32,
	.i4PairNum  = 16,
	.i4SubBlkW  = 8,
	.i4SubBlkH  = 8,
	.i4PosR = {{2, 5}, {10, 5}, {18, 5}, {26, 5},
		{6, 9}, {14, 9}, {22, 9}, {30, 9},
		{2, 17}, {10, 17}, {18, 17}, {26, 17},
		{6, 29}, {14, 29}, {22, 29}, {30, 29} },

	.i4PosL = {{2, 1}, {10, 1}, {18, 1}, {26, 1},
		{6, 13}, {14, 13}, {22, 13}, {30, 13},
		{2, 21}, {10, 21}, {18, 21}, {26, 21},
		{6, 25}, {14, 25}, {22, 25}, {30, 25} },
	.iMirrorFlip = 0,
	.i4BlockNumX = 144,
	.i4BlockNumY = 108,
};


static kal_uint16 read_cmos_sensor(kal_uint32 addr)
{
	kal_uint16 get_byte = 0;
	char pusendcmd[2] = {(char)(addr >> 8), (char)(addr & 0xFF) };

	iReadRegI2C(pusendcmd, 2, (u8 *)&get_byte, 2, imgsensor.i2c_write_id);
	LOG_INF("sensor_id = 0x%x\n", ((get_byte << 8) &
					0xff00) | ((get_byte >> 8) & 0x00ff));
	return ((get_byte << 8) & 0xff00) | ((get_byte >> 8) & 0x00ff);
}


static void write_cmos_sensor(kal_uint16 addr, kal_uint16 para)
{
	char pusendcmd[4] = {
		(char)(addr >> 8), (char)(addr & 0xFF),
		(char)(para >> 8), (char)(para & 0xFF)};

	iWriteRegI2C(pusendcmd, 4, imgsensor.i2c_write_id);
}

static kal_uint16 read_cmos_sensor_8(kal_uint16 addr)
{
	kal_uint16 get_byte = 0;
	char pusendcmd[2] = {(char)(addr >> 8), (char)(addr & 0xFF) };

	iReadRegI2C(pusendcmd, 2, (u8 *)&get_byte, 1, imgsensor.i2c_write_id);
	LOG_INF("read_cmos_sensor = 0x%x\n", get_byte);
	return get_byte;
}

static void write_cmos_sensor_8(kal_uint16 addr, kal_uint8 para)
{
	char pusendcmd[3] = {
		(char)(addr >> 8), (char)(addr & 0xFF), (char)(para & 0xFF)};

	iWriteRegI2C(pusendcmd, 3, imgsensor.i2c_write_id);
}

static void set_dummy(void)
{
	LOG_INF("dummyline = %d, dummypixels = %d\n",
		imgsensor.dummy_line, imgsensor.dummy_pixel);
	/*return; //for test*/
	write_cmos_sensor(0x0340, imgsensor.frame_length);
	write_cmos_sensor(0x0342, imgsensor.line_length);
}	/*	set_dummy  */

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

	LOG_INF("framerate = %d, min framelength should enable %d\n",
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
}	/*	set_max_framerate  */

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
	LOG_INF("%s exit!\n", __func__);
}

static kal_uint32 streaming_control(kal_bool enable)
{
	LOG_INF("streaming_enable(0=Sw Standby,1=streaming): %d\n", enable);

	if (enable) {
		write_cmos_sensor(0x6028, 0x4000);
		write_cmos_sensor(0x0100, 0X0100);
	} else {
		write_cmos_sensor(0x6028, 0x4000);
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
	if (shutter < imgsensor_info.min_shutter)
		shutter = imgsensor_info.min_shutter;

	if (imgsensor.autoflicker_en) {

		realtime_fps =
	   imgsensor.pclk / imgsensor.line_length * 10 / imgsensor.frame_length;

		if (realtime_fps >= 297 && realtime_fps <= 305)
			set_max_framerate(296, 0);
		else if (realtime_fps >= 147 && realtime_fps <= 150)
			set_max_framerate(146, 0);
		else {
			/* Extend frame length*/
			write_cmos_sensor(0x0340, imgsensor.frame_length); }
	} else {
		/* Extend frame length*/
		write_cmos_sensor(0x0340, imgsensor.frame_length);
		LOG_INF("(else)imgsensor.frame_length = %d\n",
				imgsensor.frame_length);

	}
	/* Update Shutter*/
	write_cmos_sensor_8(0x0104, 0x01);
	write_cmos_sensor(0x0202, shutter);
	write_cmos_sensor_8(0x0104, 0x00);
	LOG_INF("shutter =%d, framelength =%d\n",
			shutter, imgsensor.frame_length);

}	/*	write_shutter  */

static void set_shutter_frame_length(
	kal_uint16 shutter, kal_uint16 frame_length)
{
	unsigned long flags;
	kal_uint16 realtime_fps = 0;
#ifdef ORINGNAL_VERSION
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
#else
	 spin_lock_irqsave(&imgsensor_drv_lock, flags);
	imgsensor.shutter = shutter;
	spin_unlock_irqrestore(&imgsensor_drv_lock, flags);

	spin_lock(&imgsensor_drv_lock);
	/*Change frame time*/
	if (frame_length > 1)
		imgsensor.frame_length = frame_length;
/* */
	if (shutter > imgsensor.frame_length - imgsensor_info.margin)
		imgsensor.frame_length = shutter + imgsensor_info.margin;
#endif
	if (imgsensor.frame_length > imgsensor_info.max_frame_length)
		imgsensor.frame_length = imgsensor_info.max_frame_length;
	spin_unlock(&imgsensor_drv_lock);

	shutter = (shutter < imgsensor_info.min_shutter)
			? imgsensor_info.min_shutter : shutter;

	if (shutter > (imgsensor_info.max_frame_length - imgsensor_info.margin))
		shutter =
		(imgsensor_info.max_frame_length - imgsensor_info.margin);

	if (imgsensor.autoflicker_en) {

		realtime_fps =
	   imgsensor.pclk / imgsensor.line_length * 10 / imgsensor.frame_length;

		if (realtime_fps >= 297 && realtime_fps <= 305)
			set_max_framerate(296, 0);
		else if (realtime_fps >= 147 && realtime_fps <= 150)
			set_max_framerate(146, 0);
		else {
			/* Extend frame length*/
			write_cmos_sensor(0x0340, imgsensor.frame_length);
		}
	} else {
		/* Extend frame length*/
		write_cmos_sensor(0x0340, imgsensor.frame_length);
	}
	/* Update Shutter*/
	write_cmos_sensor_8(0x0104, 0x01);
	write_cmos_sensor(0x0202, shutter);
	write_cmos_sensor_8(0x0104, 0x00);

	LOG_INF("Add for N3D! shutterlzl =%d, framelength =%d\n",
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
}	/*	set_shutter */

static kal_uint16 gain2reg(const kal_uint16 gain)
{
	kal_uint16 reg_gain = 0x0;

	reg_gain = gain/2;
	return (kal_uint16)reg_gain;
}

/*************************************************************************
 * FUNCTION
 * set_gain
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
	LOG_INF("gain = %d , reg_gain = 0x%x\n", gain, reg_gain);

	write_cmos_sensor_8(0x0104, 0x01);
	write_cmos_sensor(0x0204, reg_gain);
	write_cmos_sensor_8(0x0104, 0x00);
    /*write_cmos_sensor_8(0x0204,(reg_gain>>8));*/
    /*write_cmos_sensor_8(0x0205,(reg_gain&0xff));*/

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

#define USE_TNP_BURST	1
#if USE_TNP_BURST
const u16 uTnpArrayA[] = {
	0x126F,
	0xB570,
	0x4606,
	0x4827,
	0x2200,
	0x6801,
	0x0C0C,
	0xB28D,
	0x4629,
	0x4620,
	0xF000,
	0xF84B,
	0x4630,
	0xF000,
	0xF84D,
	0xF000,
	0xF850,
	0xF8A6,
	0x0096,
	0x4629,
	0x4620,
	0xE8BD,
	0x4070,
	0x2201,
	0xF000,
	0xB83D,
	0xE92D,
	0x41F0,
	0x4607,
	0x481A,
	0x4688,
	0x2200,
	0x6840,
	0xB285,
	0x0C06,
	0x4629,
	0x4630,
	0xF000,
	0xF830,
	0x4C16,
	0xF8B4,
	0x1366,
	0x78A0,
	0xF894,
	0x2376,
	0xFB00,
	0x1112,
	0xF8A4,
	0x1366,
	0xF8B7,
	0x105E,
	0x4408,
	0x78E1,
	0x4408,
	0xF8A4,
	0x036E,
	0x4641,
	0x4638,
	0xF000,
	0xF82A,
	0xF8B4,
	0x1366,
	0x78A0,
	0xF894,
	0x2376,
	0xFB00,
	0x1102,
	0xF8A4,
	0x1366,
	0xF8B4,
	0x136E,
	0x1A08,
	0x78E1,
	0x1A40,
	0xF8A4,
	0x036E,
	0x4629,
	0x4630,
	0xE8BD,
	0x41F0,
	0x2201,
	0xF000,
	0xB804,
	0x0020,
	0xE043,
	0x0020,
	0x402A,
	0xF640,
	0x0C0B,
	0xF2C0,
	0x0C00,
	0x4760,
	0xF247,
	0x3CFF,
	0xF2C0,
	0x0C00,
	0x4760,
	0xF649,
	0x1CB9,
	0xF2C0,
	0x0C00,
	0x4760,
	0xF640,
	0x0CC9,
	0xF2C0,
	0x0C00,
	0x4760
};

const u16 uTnpArrayB[] = {
	0x126F,
	0x0000,
	0x0000,
	0x0000,
	0x0000,
	0x0000,
	0x0000,
	0x4904,
	0x4803,
	0x4A04,
	0x6008,
	0x1A10,
	0x8088,
	0xF000,
	0xB808,
	0x0020,
	0xE843,
	0x0020,
	0x102A,
	0x0020,
	0x004C,
	0x0000,
	0x0000,
	0xB510,
	0x2200,
	0x4906,
	0x4807,
	0xF000,
	0xF814,
	0x4C06,
	0x2200,
	0x4906,
	0x6020,
	0x4806,
	0xF000,
	0xF80D,
	0x6060,
	0xBD10,
	0x0000,
	0x0020,
	0x113E,
	0x0000,
	0xFF73,
	0x0020,
	0xE043,
	0x0020,
	0x433E,
	0x0000,
	0xC908,
	0xF64D,
	0x0CC9,
	0xF2C0,
	0x0C00,
	0x4760,
	0x0000,
	0x0721,
	0x8D01,
	0x0000,
	0x0100
};

#endif

kal_uint16 addr_data_pair_init_s5k2p6[] = {
	0xFCFC, 0x4000,
	0x6028, 0x2000,
	0x0100, 0x0000,
	0x6214, 0x7971,
	0x6218, 0x7150,
	0xF43A, 0x000C,
	0xF470, 0x000D,
	0x602A, 0x1C50,
	0x6F12, 0x15FF,
	0x3092, 0x0000,
	0x3088, 0x0101,
	0x602A, 0x13CE,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0004,
	0x6F12, 0x0004,
	0x6F12, 0x0004,
	0x6F12, 0x0004,
	0x602A, 0x196C,
	0x6F12, 0x8010,
	0x31D2, 0x0220,
	0x602A, 0x195E,
	0x6F12, 0x0001,
	0x3816, 0x0140,
	0x3818, 0x0140,
	0x3854, 0x0008,
	0x3856, 0x0008,
	0x3858, 0x0008,
	0x385A, 0x0008,
	0xF440, 0x002F,
	0x319C, 0x0130,
	0x602A, 0x1266,
	0x6F12, 0x0001,
	0x6F12, 0x0000,
	0x30D4, 0x0001,
	0x30D6, 0x002B,
	0xB138, 0x0000,
	0x0344, 0x0018,
	0x0348, 0x1217,
	0x0380, 0x0001,
	0x0382, 0x0001,
	0x0384, 0x0001,
	0x0408, 0x0000,
	0x040A, 0x0000,
	0x0136, 0x1A00,
	0x0300, 0x0003,
	0x0302, 0x0001,
	0x0304, 0x0006,
	0x030C, 0x0004,
	0x300A, 0x0000,
	0x0200, 0x0200,
	0x0204, 0x0080,
	0x37C0, 0x0002,
	0x37C2, 0x0103,
	0x3004, 0x0003,
	0x0114, 0x0300,
	0x304C, 0x0000,
	0x3AC4, 0x0100,
	0x3E04, 0x0100,
	0x3B02, 0x000F,
	0x3B34, 0x0000,
	0x3AD0, 0x0066,
	0x3B0C, 0x0000,
	0x3B3E, 0x001E,
	0x3ADA, 0x0085,
	0x3B16, 0x000F,
	0x3B48, 0x0046,
	0x3AE4, 0x00AD,
	0x3B20, 0xFFE2,
	0x3B52, 0x0000,
	0x3AEE, 0x0085,
	0x3E0E, 0x0FEF,
	0x3E18, 0x1010,
	0x3E22, 0x1018,
	0x6214, 0x7970,
};

kal_uint16 addr_data_pair_init2_s5k2p6[] = {
	0x6028, 0x4000,
	0x3190, 0x0008,
	0x3246, 0x0001,
	0x3588, 0x0069,
	0x358E, 0x005E,
	0x3594, 0x004D,
	0x359A, 0x0010,
	0x35A0, 0x0065,
	0x35A6, 0x0062,
	0x35AC, 0x0049,
	0x35B8, 0x004B,
	0x37E6, 0x0001,
	0x37E8, 0x001B,
	0x37FA, 0x007F,
	0x37C0, 0x0907,
	0x37C8, 0x300F,
	0x37CA, 0x1813,
	0x37CC, 0x0F18,
	0x37CE, 0x1305,
	0x37D0, 0x0803,
	0x37D2, 0x0404,
	0x37E4, 0x0064,
	0x37F8, 0x0001,
	0x37FC, 0x0107,
	0x31AA, 0x0001,
	0x31B2, 0x0035,
	0x37EA, 0x004E,
	0x37EC, 0x004D,
	0x37F0, 0x0004,
	0x37F2, 0x0004,
	0x37F6, 0x0001,
	0x3218, 0x01C0,
	0x3252, 0x00BC,
	0x3258, 0x00DF,
	0x325E, 0x00BA,
	0x3264, 0x00E1,
	0x326A, 0x00B8,
	0x3270, 0x01E8,
	0x3276, 0x00BC,
	0x327C, 0x01E6,
	0x3282, 0x00B8,
	0x3288, 0x00FD,
	0x32B8, 0x00B8,
	0x32CA, 0x00DF,
	0x32D0, 0x00F7,
	0x32F4, 0x01E4,
	0x333C, 0x00BA,
	0x3342, 0x0125,
	0x3348, 0x01E6,
	0x3354, 0x00BE,
	0x335A, 0x0121,
	0x3360, 0x01EA,
	0x3390, 0x00B8,
	0x3396, 0x014C,
	0x339C, 0x01E4,
	0x33A2, 0x00C0,
	0x33A8, 0x00D7,
	0x33AE, 0x00C7,
	0x33B4, 0x00DF,
	0x33BA, 0x00CF,
	0x33C0, 0x00DF,
	0x33C6, 0x00C0,
	0x33CC, 0x00C2,
	0x33DE, 0x00C7,
	0x33E4, 0x00DF,
	0x33F6, 0x00C0,
	0x33FC, 0x00C2,
	0x3408, 0x00BA,
	0x340E, 0x014A,
	0x3414, 0x01E6,
	0x3432, 0x00C0,
	0x3438, 0x00DA,
	0x344A, 0x00C0,
	0x3450, 0x00C2,
	0x345C, 0x01EA,
	0x346E, 0x00BA,
	0x3474, 0x00BD,
	0x347A, 0x01E6,
	0x3480, 0x01E9,
	0x3486, 0x00BA,
	0x361E, 0x00ED,
	0x3636, 0x01FE,
	0x3642, 0x0109,
	0x3648, 0x00BB,
	0x3654, 0x01E7,
	0x365A, 0x01FE,
	0x3666, 0x00ED,
	0x366C, 0x000B,
	0x3678, 0x00BB,
	0x367E, 0x01FE,
	0x368A, 0x00F1,
	0x36A2, 0x01FE,
	0x36AE, 0x010D,
	0x36B4, 0x00BB,
	0x36C0, 0x01E7,
	0x36C6, 0x01FE,
	0x36D2, 0x00F1,
	0x36D8, 0x000B,
	0x36E4, 0x00BB,
	0x36EA, 0x01FE,
	0x6028, 0x2000,
	0x602A, 0x2230,
	0x6F12, 0x0100,
	0x6028, 0x4000,
	0x3056, 0x0100,
	0x0B0E, 0x0000,
	0x30D4, 0x0001,
	0x0B04, 0x0101,
	0x0B08, 0x0000,
	0x307C, 0x0340,
	0x6028, 0x2000,
	0x602A, 0x1E10,
	0x6F12, 0x0100,
	0x602A, 0x1E84,
	0x6F12, 0x0101,
	0x0B00, 0x0080,
	0x3060, 0x0742,
	0x6028, 0x2000,
	0x602A, 0x1C7A,
	0x6F12, 0x0100,
	0x602A, 0x1CA2,
	0x6F12, 0x0000,
};

/*Ref S5k2P6SX_EVT0_Ver_0.17_20181204 excel*/
static void sensor_init(void)
{
	table_write_cmos_sensor(addr_data_pair_init_s5k2p6,
		sizeof(addr_data_pair_init_s5k2p6) / sizeof(kal_uint16));
}	/*	sensor_init  */

kal_uint16 addr_data_pair_preview_s5k2p6[] = {
	0x0100, 0x0000,
	0xFCFC, 0x4000,
	0x6028, 0x2000,
	0x6214, 0x7971,
	0x6218, 0x7150,
	0x30CE, 0x0000,
	0x37F6, 0x0021,
	0x3198, 0x0007,
	0x319A, 0x0100,
	0x319C, 0x0130,
	0x3056, 0x0100,
	0x602A, 0x1BB0,
	0x6F12, 0x0000, //tail mode
	//0x6F12, 0x1000,   //normal mode
	0x0B0E, 0x0100,
	0x30D8, 0x0100,
	0x31B0, 0x0008,
	0x0340, 0x0E1A,
	0x0342, 0x1428,
	0x0344, 0x0018,
	0x0346, 0x0010,
	0x0348, 0x1217,
	0x034A, 0x0D8F,
	0x034C, 0x1200,
	0x034E, 0x0D80,
	0x0900, 0x0011,
	0x0386, 0x0001,
	0x0400, 0x0000,
	0x0404, 0x0000,
	0x0306, 0x0061,
	0x1130, 0x440E,
	0x030E, 0x006F,
	0x300A, 0x0000,
	0x0202, 0x0E10,
	0x021E, 0x0E10,
	0x0216, 0x0000,
	0x6214, 0x7970,
	0x0100, 0x0100,
};


static void preview_setting(void)
{
	/*Preview 4608x3456 no bin */
	/*preview 30.01fps*/
	LOG_INF("%s() E\n", __func__);

	table_write_cmos_sensor(addr_data_pair_preview_s5k2p6,
		sizeof(addr_data_pair_preview_s5k2p6) / sizeof(kal_uint16));

}	/*	preview_setting  */

kal_uint16 addr_data_pair_cap30fps_s5k2p6[] = {
	0x0100, 0x0000,
	0xFCFC, 0x4000,
	0x6028, 0x2000,
	0x6214, 0x7971,
	0x6218, 0x7150,
	0x30CE, 0x0000,
	0x37F6, 0x0021,
	0x3198, 0x0007,
	0x319A, 0x0100,
	0x319C, 0x0130,
	0x3056, 0x0100,
	0x602A, 0x1BB0,
	0x6F12, 0x1000,   //normal mode //0x0000 PD
	0x0B0E, 0x0100,
	0x30D8, 0x0100,
	0x31B0, 0x0008,
	0x0340, 0x0E1A,
	0x0342, 0x1428,
	0x0344, 0x0018,
	0x0346, 0x0010,
	0x0348, 0x1217,
	0x034A, 0x0D8F,
	0x034C, 0x1200,
	0x034E, 0x0D80,
	0x0900, 0x0011,
	0x0386, 0x0001,
	0x0400, 0x0000,
	0x0404, 0x0000,
	0x0306, 0x0061,
	0x1130, 0x440E,
	0x030E, 0x006F,
	0x300A, 0x0000,
	0x0202, 0x0E10,
	0x021E, 0x0E10,
	0x0216, 0x0000,
	0x6214, 0x7970,
	0x0100, 0x0100,
};


static void capture_setting(kal_uint16 currefps)
{
	LOG_INF("%s() E! currefps:%d\n", __func__, currefps);

		table_write_cmos_sensor(addr_data_pair_cap30fps_s5k2p6,
					sizeof(addr_data_pair_cap30fps_s5k2p6) /
					sizeof(kal_uint16));

}

kal_uint16 addr_data_pair_normal_video_s5k2p6[] = {
	0xFCFC, 0x4000,
	0x6028, 0x2000,
	0x6214, 0x7971,
	0x6218, 0x7150,
	0x30CE, 0x0000,
	0x37F6, 0x0021,
	0x3198, 0x0007,
	0x319A, 0x0100,
	0x319C, 0x0130,
	0x3056, 0x0100,
	0x602A, 0x1BB0,
	0x6F12, 0x0000, //tail mode
	//0x6F12, 0x1000,   //normal mode
	0x0B0E, 0x0100,
	0x30D8, 0x0100,
	0x31B0, 0x0008,
	0x0340, 0x0E1A,
	0x0342, 0x1428,
	0x0344, 0x0018,
	0x0346, 0x01C0,
	0x0348, 0x1217,
	0x034A, 0x0BDF,
	0x034C, 0x1200,
	0x034E, 0x0A20,
	0x0900, 0x0011,
	0x0386, 0x0001,
	0x0400, 0x0000,
	0x0404, 0x0000,
	0x0306, 0x0061,
	0x1130, 0x440E,
	0x030E, 0x006F,
	0x300A, 0x0000,
	0x0202, 0x0E10,
	0x021E, 0x0E10,
	0x0216, 0x0000,
	0x6214, 0x7970,
};

static void normal_video_setting(kal_uint16 currefps)
{
	LOG_INF("%s() E! currefps:%d\n", __func__, currefps);
	table_write_cmos_sensor(addr_data_pair_normal_video_s5k2p6,
	    sizeof(addr_data_pair_normal_video_s5k2p6) / sizeof(kal_uint16));
}

kal_uint16 addr_data_pair_hs_video_s5k2p6[] = {
	0xFCFC, 0x4000,
	0x6028, 0x2000,
	0x6214, 0x7971,
	0x6218, 0x7150,
	0x30CE, 0x0000,
	0x37F6, 0x0021,
	0x3198, 0x0007,
	0x319A, 0x0100,
	0x319C, 0x0130,
	0x3056, 0x0100,
	0x602A, 0x1BB0,
	//0x6F12, 0x0000, //tail mode
	0x6F12, 0x1000,   //normal mode
	0x0B0E, 0x0100,
	0x30D8, 0x0100,
	0x31B0, 0x0008,
	0x0340, 0x0E1A,
	0x0342, 0x1428,
	0x0344, 0x0018,
	0x0346, 0x01C0,
	0x0348, 0x1217,
	0x034A, 0x0BDF,
	0x034C, 0x1200,
	0x034E, 0x0A20,
	0x0900, 0x0011,
	0x0386, 0x0001,
	0x0400, 0x0000,
	0x0404, 0x0000,
	0x0306, 0x0061,
	0x1130, 0x440E,
	0x030E, 0x006F,
	0x300A, 0x0000,
	0x0202, 0x0E10,
	0x021E, 0x0E10,
	0x0216, 0x0000,
	0x6214, 0x7970,

};



static void hs_video_setting(void)
{
	LOG_INF("%s() E\n", __func__);
	table_write_cmos_sensor(addr_data_pair_hs_video_s5k2p6,
		sizeof(addr_data_pair_hs_video_s5k2p6) / sizeof(kal_uint16));
}

kal_uint16 addr_data_pair_slim_video_s5k2p6[] = {
	0xFCFC, 0x4000,
	0x6028, 0x2000,
	0x6214, 0x7971,
	0x6218, 0x7150,
	0x30CE, 0x0000,
	0x37F6, 0x0021,
	0x3198, 0x0007,
	0x319A, 0x0100,
	0x319C, 0x0130,
	0x3056, 0x0100,
	0x602A, 0x1BB0,
	//0x6F12, 0x0000, //tail mode
	0x6F12, 0x1000,   //normal mode
	0x0B0E, 0x0100,
	0x30D8, 0x0100,
	0x31B0, 0x0008,
	0x0340, 0x0E1A,
	0x0342, 0x1428,
	0x0344, 0x0018,
	0x0346, 0x01C0,
	0x0348, 0x1217,
	0x034A, 0x0BDF,
	0x034C, 0x1200,
	0x034E, 0x0A20,
	0x0900, 0x0011,
	0x0386, 0x0001,
	0x0400, 0x0000,
	0x0404, 0x0000,
	0x0306, 0x0061,
	0x1130, 0x440E,
	0x030E, 0x006F,
	0x300A, 0x0000,
	0x0202, 0x0E10,
	0x021E, 0x0E10,
	0x0216, 0x0000,
	0x6214, 0x7970,
};


static void slim_video_setting(void)
{
	LOG_INF("%s() E\n", __func__);
	table_write_cmos_sensor(addr_data_pair_slim_video_s5k2p6,
		sizeof(addr_data_pair_slim_video_s5k2p6) / sizeof(kal_uint16));
}

kal_uint16 addr_data_pair_custom1_s5k2p6[] = {
	/*26M Mode8*/
0x0100, 0x0000,
0xFCFC, 0x4000,
0x6028, 0x2000,
0x6214, 0x7971,
0x6218, 0x7150,
0x30CE, 0x0040,
0x37F6, 0x0001,
0x3198, 0x00CC,
0x319A, 0x0000,
0x319C, 0x0130,
0x3056, 0x0000,
0x602A, 0x1BB0,
0x6F12, 0x0100,
0x0B0E, 0x0100,
0x30D8, 0x0000,
0x31B0, 0x0002,
0x0340, 0x0730,
0x0342, 0x1438,
0x0344, 0x0018,
0x0346, 0x0010,
0x0348, 0x1217,
0x034A, 0x0D8F,
0x034C, 0x0480,
0x034E, 0x0360,
0x0900, 0x0114,
0x0386, 0x0007,
0x0400, 0x0001,
0x0404, 0x0040,
0x0306, 0x0063,
0x1130, 0x4411,
0x030E, 0x0068,
0x300A, 0x0000,
0x0202, 0x038E,
0x021E, 0x038E,
0x0216, 0x0000,
0x6214, 0x7970,
0x0100, 0x0100,
};


static void custom1_setting(void)
{
	LOG_INF("%s() E\n", __func__);
	/*1080p 60fps*/
	table_write_cmos_sensor(addr_data_pair_custom1_s5k2p6,
		sizeof(addr_data_pair_custom1_s5k2p6) / sizeof(kal_uint16));
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
	 * we should detect the module used i2c address
	 */
	LOG_INF("%s() E\n", __func__);
	while (imgsensor_info.i2c_addr_table[i] != 0xff) {
		spin_lock(&imgsensor_drv_lock);
		imgsensor.i2c_write_id = imgsensor_info.i2c_addr_table[i];
		spin_unlock(&imgsensor_drv_lock);
		LOG_INF("i2c write id: 0x%x\n", imgsensor.i2c_write_id);
		do {
			*sensor_id = (
	      (read_cmos_sensor_8(0x0000) << 8) | read_cmos_sensor_8(0x0001));
				LOG_INF("i2c write id: 0x%x, sensor id: 0x%x\n",
					imgsensor.i2c_write_id, *sensor_id);
			if (*sensor_id == imgsensor_info.sensor_id)
				return ERROR_NONE;

			LOG_INF("Read sensor id fail, id: 0x%x\n",
				imgsensor.i2c_write_id);
			retry--;
		} while (retry > 0);
		i++;
		retry = 2;
	}
	if (*sensor_id != imgsensor_info.sensor_id) {
	/* if Sensor ID is not correct, Must set *sensor_id to 0xFFFFFFFF*/
		*sensor_id = 0xFFFFFFFF;
		LOG_INF("%s() sensor_id = 0xFFFFFFFF X\n", __func__);
		return ERROR_SENSOR_CONNECT_FAIL;
	}
	LOG_INF("%s() X\n", __func__);
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

	LOG_INF(
	  "PLATFORM:VINSON,MIPI 4LANE  preview 1280*960@30fps,864Mbps/lane\n");
	LOG_INF(
	  "video 1280*960@30fps,864Mbps/lane; capture 5M@30fps,864Mbps/lane\n");

	/* sensor have two i2c address 0x6c 0x6d & 0x21 0x20,
	 * we should detect the module used i2c address
	 */

	while (imgsensor_info.i2c_addr_table[i] != 0xff) {
		spin_lock(&imgsensor_drv_lock);
		imgsensor.i2c_write_id = imgsensor_info.i2c_addr_table[i];
		spin_unlock(&imgsensor_drv_lock);
		do {
			sensor_id = (
	      (read_cmos_sensor_8(0x0000) << 8) | read_cmos_sensor_8(0x0001));

			if (sensor_id == imgsensor_info.sensor_id) {

				LOG_INF("i2c write id: 0x%x, sensor id: 0x%x\n",
					imgsensor.i2c_write_id, sensor_id);
				break;
			}
			LOG_INF("Read sensor id fail, id: 0x%x\n",
				imgsensor.i2c_write_id);

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
 *  *image_window : address pointer of pixel numbers in one period of HSYNC
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
	LOG_INF("E\n");
	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_CAPTURE;
	/*PIP capture: 24fps for less than 13M, 20fps for 16M,15fps for 20M*/
	if (imgsensor.current_fps == imgsensor_info.cap1.max_framerate) {
		imgsensor.pclk = imgsensor_info.cap1.pclk;
		imgsensor.line_length = imgsensor_info.cap1.linelength;
		imgsensor.frame_length = imgsensor_info.cap1.framelength;
		imgsensor.min_frame_length = imgsensor_info.cap1.framelength;
		imgsensor.autoflicker_en = KAL_FALSE;
	}  else if (
	    imgsensor.current_fps == imgsensor_info.cap2.max_framerate) {

		imgsensor.pclk = imgsensor_info.cap2.pclk;
		imgsensor.line_length = imgsensor_info.cap2.linelength;
		imgsensor.frame_length = imgsensor_info.cap2.framelength;
		imgsensor.min_frame_length = imgsensor_info.cap2.framelength;
		imgsensor.autoflicker_en = KAL_FALSE;

	} else {
		if (imgsensor.current_fps != imgsensor_info.cap.max_framerate)
			LOG_INF(
		"Warning: current_fps %d fps is not support, so use cap's setting: %d fps!\n",
			imgsensor.current_fps,
			imgsensor_info.cap.max_framerate/10);

		imgsensor.pclk = imgsensor_info.cap.pclk;
		imgsensor.line_length = imgsensor_info.cap.linelength;
		imgsensor.frame_length = imgsensor_info.cap.framelength;
		imgsensor.min_frame_length = imgsensor_info.cap.framelength;
		imgsensor.autoflicker_en = KAL_FALSE;
	}
	spin_unlock(&imgsensor_drv_lock);

	 capture_setting(imgsensor.current_fps);
	set_mirror_flip(imgsensor.mirror);

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
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);

	normal_video_setting(imgsensor.current_fps);
	set_mirror_flip(imgsensor.mirror);

	return ERROR_NONE;
}	/*	normal_video   */

static kal_uint32 hs_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
				  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("E\n");

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_HIGH_SPEED_VIDEO;
	imgsensor.pclk = imgsensor_info.hs_video.pclk;
	/*imgsensor.video_mode = KAL_TRUE;*/
	imgsensor.line_length = imgsensor_info.hs_video.linelength;
	imgsensor.frame_length = imgsensor_info.hs_video.framelength;
	imgsensor.min_frame_length = imgsensor_info.hs_video.framelength;
	imgsensor.dummy_line = 0;
	imgsensor.dummy_pixel = 0;
	/*imgsensor.current_fps = 300;*/
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	hs_video_setting();
	set_mirror_flip(imgsensor.mirror);

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
	set_mirror_flip(imgsensor.mirror);

	return ERROR_NONE;
}	/*	slim_video	 */

/*************************************************************************
 * FUNCTION
 * Custom1
 *
 * DESCRIPTION
 *	This function start the sensor Custom1.
 *
 * PARAMETERS
 *  *image_window : address pointer of pixel numbers in one period of HSYNC
 *  *sensor_config_data : address pointer of line numbers in one period of VSYNC
 *
 * RETURNS
 *	None
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/
static kal_uint32 custom1(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
				  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("E\n");

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_CUSTOM1;
	imgsensor.pclk = imgsensor_info.custom1.pclk;
	imgsensor.line_length = imgsensor_info.custom1.linelength;
	imgsensor.frame_length = imgsensor_info.custom1.framelength;
	imgsensor.min_frame_length = imgsensor_info.custom1.framelength;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);

	custom1_setting();
	set_mirror_flip(imgsensor.mirror);

	return ERROR_NONE;
}	/*	custom1   */



static kal_uint32 get_resolution(
	MSDK_SENSOR_RESOLUTION_INFO_STRUCT(*sensor_resolution))
{
	LOG_INF("E\n");
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

	sensor_resolution->SensorSlimVideoWidth	 =
		imgsensor_info.slim_video.grabwindow_width;
	sensor_resolution->SensorSlimVideoHeight =
		imgsensor_info.slim_video.grabwindow_height;

	sensor_resolution->SensorCustom1Width	 =
		imgsensor_info.custom1.grabwindow_width;
	sensor_resolution->SensorCustom1Height =
		imgsensor_info.custom1.grabwindow_height;

	return ERROR_NONE;
}	/*	get_resolution	*/

static kal_uint32 get_info(enum MSDK_SCENARIO_ID_ENUM scenario_id,
				  MSDK_SENSOR_INFO_STRUCT *sensor_info,
				  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("scenario_id = %d\n", scenario_id);

	sensor_info->SensorClockPolarity = SENSOR_CLOCK_POLARITY_LOW;

	/* not use */
	sensor_info->SensorClockFallingPolarity = SENSOR_CLOCK_POLARITY_LOW;
	/* inverse with datasheet*/
	sensor_info->SensorHsyncPolarity = SENSOR_CLOCK_POLARITY_LOW;
	sensor_info->SensorVsyncPolarity = SENSOR_CLOCK_POLARITY_LOW;
	sensor_info->SensorInterruptDelayLines = 4; /* not use */
	sensor_info->SensorResetActiveHigh = FALSE; /* not use */
	sensor_info->SensorResetDelayCount = 5; /* not use */

	sensor_info->SensroInterfaceType = imgsensor_info.sensor_interface_type;
	sensor_info->MIPIsensorType = imgsensor_info.mipi_sensor_type;
	sensor_info->SettleDelayMode = imgsensor_info.mipi_settle_delay_mode;

	sensor_info->SensorOutputDataFormat =
		imgsensor_info.sensor_output_dataformat;

	sensor_info->CaptureDelayFrame = imgsensor_info.cap_delay_frame;
	sensor_info->PreviewDelayFrame = imgsensor_info.pre_delay_frame;
	sensor_info->VideoDelayFrame = imgsensor_info.video_delay_frame;

	sensor_info->HighSpeedVideoDelayFrame =
		imgsensor_info.hs_video_delay_frame;
	sensor_info->SlimVideoDelayFrame =
		imgsensor_info.slim_video_delay_frame;
	sensor_info->Custom1DelayFrame = imgsensor_info.custom1_delay_frame;

	sensor_info->SensorMasterClockSwitch = 0; /* not use */
	sensor_info->SensorDrivingCurrent = imgsensor_info.isp_driving_current;

	sensor_info->AEShutDelayFrame = imgsensor_info.ae_shut_delay_frame;

	sensor_info->AESensorGainDelayFrame =
		imgsensor_info.ae_sensor_gain_delay_frame;
	sensor_info->AEISPGainDelayFrame =
		imgsensor_info.ae_ispGain_delay_frame;

	sensor_info->IHDR_Support = imgsensor_info.ihdr_support;
	sensor_info->IHDR_LE_FirstLine = imgsensor_info.ihdr_le_firstline;
	sensor_info->SensorModeNum = imgsensor_info.sensor_mode_num;
	sensor_info->PDAF_Support = 2;
	/* 0: NO PDAF, 1: PDAF Raw Data mode,
	 * 2:PDAF VC mode(Full),
	 * 3:PDAF VC mode(Binning)
	 */
	sensor_info->SensorMIPILaneNumber = imgsensor_info.mipi_lane_num;
	sensor_info->SensorClockFreq = imgsensor_info.mclk;
	sensor_info->SensorClockDividCount = 3; /* not use */
	sensor_info->SensorClockRisingCount = 0;
	sensor_info->SensorClockFallingCount = 2; /* not use */
	sensor_info->SensorPixelClockCount = 3; /* not use */
	sensor_info->SensorDataLatchCount = 2; /* not use */

	sensor_info->MIPIDataLowPwr2HighSpeedTermDelayCount = 0;
	sensor_info->MIPICLKLowPwr2HighSpeedTermDelayCount = 0;
	sensor_info->SensorWidthSampling = 0;  /* 0 is default 1x*/
	sensor_info->SensorHightSampling = 0;	/* 0 is default 1x*/
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
		sensor_info->SensorGrabStartX =
			imgsensor_info.normal_video.startx;
		sensor_info->SensorGrabStartY =
			imgsensor_info.normal_video.starty;

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
		sensor_info->SensorGrabStartX =
			imgsensor_info.slim_video.startx;
		sensor_info->SensorGrabStartY =
			imgsensor_info.slim_video.starty;

		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
			imgsensor_info.slim_video.mipi_data_lp2hs_settle_dc;

			break;
	case MSDK_SCENARIO_ID_CUSTOM1:
		sensor_info->SensorGrabStartX = imgsensor_info.custom1.startx;
		sensor_info->SensorGrabStartY = imgsensor_info.custom1.starty;

		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
			imgsensor_info.custom1.mipi_data_lp2hs_settle_dc;
			break;
	default:
		sensor_info->SensorGrabStartX = imgsensor_info.pre.startx;
		sensor_info->SensorGrabStartY = imgsensor_info.pre.starty;

		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
			imgsensor_info.pre.mipi_data_lp2hs_settle_dc;
			break;
	}

	return ERROR_NONE;
}	/* get_info  */


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
			custom1(image_window, sensor_config_data);
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
	/* SetVideoMode Function should fix framerate*/
	if (framerate == 0)
		/* Dynamic frame rate*/
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
	if (enable) /*enable auto flicker*/
		imgsensor.autoflicker_en = KAL_TRUE;
	else /*Cancel Auto flick*/
		imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	return ERROR_NONE;
}


static kal_uint32 set_max_framerate_by_scenario(
	enum MSDK_SCENARIO_ID_ENUM scenario_id, MUINT32 framerate)
{
	kal_uint32 frame_length;

	LOG_INF("scenario_id = %d, framerate = %d\n", scenario_id, framerate);

	switch (scenario_id) {
	case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		frame_length = imgsensor_info.pre.pclk /
			framerate * 10 / imgsensor_info.pre.linelength;

		spin_lock(&imgsensor_drv_lock);
		if (frame_length > imgsensor_info.pre.framelength)
			imgsensor.dummy_line =
			(frame_length - imgsensor_info.pre.framelength);
		else
			imgsensor.dummy_line = 0;
		imgsensor.frame_length =
		  imgsensor_info.pre.framelength + imgsensor.dummy_line;

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
		if (frame_length > imgsensor_info.normal_video.framelength)
			imgsensor.dummy_line =
		      (frame_length - imgsensor_info.normal_video.framelength);

		else
			imgsensor.dummy_line = 0;
		imgsensor.frame_length =
		 imgsensor_info.normal_video.framelength + imgsensor.dummy_line;

		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);

		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
		break;
	case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		if (imgsensor.current_fps ==
			imgsensor_info.cap1.max_framerate) {

			frame_length = imgsensor_info.cap1.pclk
			    / framerate * 10 / imgsensor_info.cap1.linelength;

			spin_lock(&imgsensor_drv_lock);
			if (frame_length > imgsensor_info.cap1.framelength)
				imgsensor.dummy_line =
				 frame_length - imgsensor_info.cap1.framelength;

			else
				imgsensor.dummy_line = 0;

			imgsensor.frame_length =
			 imgsensor_info.cap1.framelength + imgsensor.dummy_line;

			imgsensor.min_frame_length = imgsensor.frame_length;
			spin_unlock(&imgsensor_drv_lock);

		} else if (
		  imgsensor.current_fps == imgsensor_info.cap2.max_framerate) {

			frame_length = imgsensor_info.cap2.pclk /
				framerate * 10 / imgsensor_info.cap2.linelength;

			spin_lock(&imgsensor_drv_lock);
			if (frame_length > imgsensor_info.cap2.framelength)
				imgsensor.dummy_line =
			      (frame_length - imgsensor_info.cap2.framelength);

			else
				imgsensor.dummy_line = 0;

			imgsensor.frame_length =
			 imgsensor_info.cap2.framelength + imgsensor.dummy_line;

			 imgsensor.min_frame_length = imgsensor.frame_length;
			spin_unlock(&imgsensor_drv_lock);
		} else {
			if (imgsensor.current_fps !=
				imgsensor_info.cap.max_framerate)
				LOG_INF(
					"current_fps %d is not support, so use cap's setting: %d fps!\n",
					framerate,
					imgsensor_info.cap.max_framerate/10);

			frame_length = imgsensor_info.cap.pclk /
			framerate * 10 / imgsensor_info.cap.linelength;
			spin_lock(&imgsensor_drv_lock);

			if (frame_length > imgsensor_info.cap.framelength)
				imgsensor.dummy_line =
				  frame_length - imgsensor_info.cap.framelength;
			else
				imgsensor.dummy_line = 0;

			imgsensor.frame_length =
			 imgsensor_info.cap.framelength + imgsensor.dummy_line;

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
		if (frame_length > imgsensor_info.hs_video.framelength)
			imgsensor.dummy_line =
		  (frame_length - imgsensor_info.hs_video.framelength);
		else
			imgsensor.dummy_line = 0;

		imgsensor.frame_length =
		    imgsensor_info.hs_video.framelength + imgsensor.dummy_line;

		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
		break;
	case MSDK_SCENARIO_ID_SLIM_VIDEO:
		frame_length = imgsensor_info.slim_video.pclk /
			framerate * 10 / imgsensor_info.slim_video.linelength;

		spin_lock(&imgsensor_drv_lock);
		if (frame_length > imgsensor_info.slim_video.framelength)
			imgsensor.dummy_line =
			(frame_length - imgsensor_info.slim_video.framelength);

		else
			imgsensor.dummy_line = 0;

		imgsensor.frame_length =
		   imgsensor_info.slim_video.framelength + imgsensor.dummy_line;

		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
		break;
	case MSDK_SCENARIO_ID_CUSTOM1:
		frame_length = imgsensor_info.custom1.pclk /
			framerate * 10 / imgsensor_info.custom1.linelength;

		spin_lock(&imgsensor_drv_lock);
		if (frame_length > imgsensor_info.custom1.framelength)
			imgsensor.dummy_line =
			(frame_length - imgsensor_info.custom1.framelength);
		else
			imgsensor.dummy_line = 0;
		imgsensor.frame_length =
		  imgsensor_info.custom1.framelength + imgsensor.dummy_line;

		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
		break;
	default:  /*coding with  preview scenario by default*/

		frame_length = imgsensor_info.pre.pclk /
			framerate * 10 / imgsensor_info.pre.linelength;

		spin_lock(&imgsensor_drv_lock);
		if (frame_length > imgsensor_info.pre.framelength)
			imgsensor.dummy_line =
			(frame_length - imgsensor_info.pre.framelength);

		else
			imgsensor.dummy_line = 0;

		imgsensor.frame_length =
		  imgsensor_info.pre.framelength + imgsensor.dummy_line;

		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
		LOG_INF(
		    "error scenario_id = %d, we use preview scenario\n",
			scenario_id);
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
	    /* 0x5E00[8]: 1 enable,  0 disable*/
	    /* 0x5E00[1:0]; 00 Color bar, 01 Random Data, 10 Square, 11 BLACK*/
		write_cmos_sensor(0x0600, 0x0002);
	} else {
	    /* 0x5E00[8]: 1 enable,  0 disable*/
	    /* 0x5E00[1:0]; 00 Color bar, 01 Random Data, 10 Square, 11 BLACK*/
		write_cmos_sensor(0x0600, 0x0000);
	}
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
	UINT32 fps = 0;

	struct SET_PD_BLOCK_INFO_T *PDAFinfo;
	struct SENSOR_VC_INFO_STRUCT *pvcinfo;
	struct SENSOR_WINSIZE_INFO_STRUCT *wininfo;


	MSDK_SENSOR_REG_INFO_STRUCT *sensor_reg_data =
		(MSDK_SENSOR_REG_INFO_STRUCT *) feature_para;

	LOG_INF("feature_id = %d\n", feature_id);
	switch (feature_id) {
	case SENSOR_FEATURE_GET_PERIOD:
			LOG_INF("SENSOR_FEATURE_GET_PERIOD\n");
			*feature_return_para_16++ = imgsensor.line_length;
			*feature_return_para_16 = imgsensor.frame_length;
			*feature_para_len = 4;
			break;
	case SENSOR_FEATURE_GET_PIXEL_CLOCK_FREQ:
		LOG_INF("imgsensor.pclk = %d,current_fps = %d\n",
			imgsensor.pclk, imgsensor.current_fps);
			*feature_return_para_32 = imgsensor.pclk;
			*feature_para_len = 4;
			break;
	case SENSOR_FEATURE_SET_ESHUTTER:
		LOG_INF("SENSOR_FEATURE_SET_ESHUTTER\n");
		set_shutter(*feature_data);
			break;
	case SENSOR_FEATURE_SET_NIGHTMODE:
		LOG_INF("SENSOR_FEATURE_SET_NIGHTMODE\n");
	    /*night_mode((BOOL) *feature_data); no need to implement this mode*/
			break;
	case SENSOR_FEATURE_SET_GAIN:
		LOG_INF("SENSOR_FEATURE_SET_GAIN\n");
		set_gain((UINT16) *feature_data);
			break;
	case SENSOR_FEATURE_SET_FLASHLIGHT:
			LOG_INF("SENSOR_FEATURE_SET_FLASHLIGHT\n");
			break;
	case SENSOR_FEATURE_SET_ISP_MASTER_CLOCK_FREQ:
			LOG_INF("SENSOR_FEATURE_SET_ISP_MASTER_CLOCK_FREQ\n");
			break;
	case SENSOR_FEATURE_SET_REGISTER:
			LOG_INF("SENSOR_FEATURE_SET_REGISTER\n");
			write_cmos_sensor(
			    sensor_reg_data->RegAddr, sensor_reg_data->RegData);
			break;
	case SENSOR_FEATURE_GET_REGISTER:
			LOG_INF("SENSOR_FEATURE_GET_REGISTER\n");
			sensor_reg_data->RegData =
				read_cmos_sensor(sensor_reg_data->RegAddr);
			break;
	case SENSOR_FEATURE_GET_LENS_DRIVER_ID:
			LOG_INF("SENSOR_FEATURE_GET_LENS_DRIVER_ID\n");
		/* get the lens driver ID from EEPROM or
		 * just return LENS_DRIVER_ID_DO_NOT_CARE
		 */

		/* if EEPROM does not exist in camera module.*/
		*feature_return_para_32 = LENS_DRIVER_ID_DO_NOT_CARE;
		*feature_para_len = 4;
		break;
	case SENSOR_FEATURE_SET_VIDEO_MODE:
		LOG_INF("SENSOR_FEATURE_SET_VIDEO_MODE\n");
		set_video_mode(*feature_data);
		break;
	case SENSOR_FEATURE_CHECK_SENSOR_ID:
			LOG_INF("SENSOR_FEATURE_CHECK_SENSOR_ID\n");
			get_imgsensor_id(feature_return_para_32);
			break;
	case SENSOR_FEATURE_SET_AUTO_FLICKER_MODE:
		LOG_INF("SENSOR_FEATURE_SET_AUTO_FLICKER_MODE\n");
		set_auto_flicker_mode(
			(BOOL)*feature_data_16, *(feature_data_16+1));
		break;
	case SENSOR_FEATURE_SET_MAX_FRAME_RATE_BY_SCENARIO:
		LOG_INF("SENSOR_FEATURE_SET_MAX_FRAME_RATE_BY_SCENARIO\n");
		set_max_framerate_by_scenario(
		  (enum MSDK_SCENARIO_ID_ENUM)*feature_data, *(feature_data+1));
		break;
	case SENSOR_FEATURE_GET_DEFAULT_FRAME_RATE_BY_SCENARIO:
		LOG_INF("SENSOR_FEATURE_GET_DEFAULT_FRAME_RATE_BY_SCENARIO\n");
		get_default_framerate_by_scenario(
			(enum MSDK_SCENARIO_ID_ENUM)*(feature_data),
			(MUINT32 *)(uintptr_t)(*(feature_data+1)));
		break;
	case SENSOR_FEATURE_SET_TEST_PATTERN:
		LOG_INF("SENSOR_FEATURE_SET_TEST_PATTERN\n");
		set_test_pattern_mode((BOOL)*feature_data);
		break;

		/*for factory mode auto testing*/
	case SENSOR_FEATURE_GET_TEST_PATTERN_CHECKSUM_VALUE:
			LOG_INF("%s\n", __func__);
			*feature_return_para_32 = imgsensor_info.checksum_value;
			*feature_para_len = 4;
			break;

	case SENSOR_FEATURE_SET_FRAMERATE:
		LOG_INF("current fps :%d\n", *feature_data_32);
		spin_lock(&imgsensor_drv_lock);
		imgsensor.current_fps = (UINT16)*feature_data_32;
		spin_unlock(&imgsensor_drv_lock);
		break;

	case SENSOR_FEATURE_SET_HDR:
		LOG_INF("hdr enable :%d\n", *feature_data_32);
		spin_lock(&imgsensor_drv_lock);
		imgsensor.ihdr_mode = (UINT8)*feature_data_32;
		spin_unlock(&imgsensor_drv_lock);
		break;

	case SENSOR_FEATURE_GET_CROP_INFO:
		LOG_INF("SENSOR_FEATURE_GET_CROP_INFO scenarioId:%d\n",
				(UINT32)*feature_data);

		wininfo =
	    (struct SENSOR_WINSIZE_INFO_STRUCT *)(uintptr_t)(*(feature_data+1));

		switch (*feature_data_32) {
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			memcpy(
				(void *)wininfo,
				(void *)&imgsensor_winsize_info[1],
				sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			memcpy(
				(void *)wininfo,
				(void *)&imgsensor_winsize_info[2],
				sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_CUSTOM1:
			memcpy(
				(void *)wininfo,
				(void *)&imgsensor_winsize_info[5],
				sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
			memcpy(
				(void *)wininfo,
				(void *)&imgsensor_winsize_info[3],
				sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
			memcpy(
				(void *)wininfo,
				(void *)&imgsensor_winsize_info[4],
				sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		default:
			memcpy(
				(void *)wininfo,
				(void *)&imgsensor_winsize_info[0],
				sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		}
		break;
	case SENSOR_FEATURE_SET_IHDR_SHUTTER_GAIN:
		LOG_INF("SENSOR_SET_SENSOR_IHDR LE=%d, SE=%d, Gain=%d\n",
			(UINT16)*feature_data,
			(UINT16)*(feature_data+1), (UINT16)*(feature_data+2));

		/* ihdr_write_shutter_gain((UINT16)*feature_data,
		 * (UINT16)*(feature_data+1),(UINT16)*(feature_data+2));
		 */
		break;
	case SENSOR_FEATURE_SET_AWB_GAIN:
		LOG_INF("SENSOR_FEATURE_SET_AWB_GAIN\n");
		break;
	case SENSOR_FEATURE_SET_HDR_SHUTTER:
		LOG_INF("SENSOR_FEATURE_SET_HDR_SHUTTER LE=%d, SE=%d\n",
			(UINT16)*feature_data, (UINT16)*(feature_data+1));
		/* ihdr_write_shutter(
		 * (UINT16)*feature_data,(UINT16)*(feature_data+1));
		 */
		break;
		/******************** PDAF START >>> *********/
	case SENSOR_FEATURE_GET_PDAF_INFO:
		LOG_INF("SENSOR_FEATURE_GET_PDAF_INFO scenarioId:%d\n",
			(UINT16)*feature_data);
		PDAFinfo =
		  (struct SET_PD_BLOCK_INFO_T *)(uintptr_t)(*(feature_data+1));
		switch (*feature_data) {
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
			imgsensor_pd_info.i4BlockNumX = 144;  //4608 / 32
			imgsensor_pd_info.i4BlockNumY = 108;  //3456 / 32
			memcpy(
				(void *)PDAFinfo,
				(void *)&imgsensor_pd_info,
				sizeof(struct SET_PD_BLOCK_INFO_T));
			break;
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			imgsensor_pd_info.i4BlockNumX = 144;//4608 / 32
			imgsensor_pd_info.i4BlockNumY = 81;// 2592 / 32
			memcpy(
				(void *)PDAFinfo,
				(void *)&imgsensor_pd_info,
				sizeof(struct SET_PD_BLOCK_INFO_T));
			break;
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
		default:
				break;
		}
		break;
	case SENSOR_FEATURE_GET_VC_INFO:
		LOG_INF("SENSOR_FEATURE_GET_VC_INFO %d\n",
			(UINT16)*feature_data);
		pvcinfo =
		 (struct SENSOR_VC_INFO_STRUCT *)(uintptr_t)(*(feature_data+1));
		switch (*feature_data_32) {
		case MSDK_SCENARIO_ID_CUSTOM1:
			memcpy(
				(void *)pvcinfo,
				(void *)&SENSOR_VC_INFO[5],
				sizeof(struct SENSOR_VC_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			memcpy(
				(void *)pvcinfo,
				(void *)&SENSOR_VC_INFO[1],
				sizeof(struct SENSOR_VC_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			memcpy(
				(void *)pvcinfo,
				(void *)&SENSOR_VC_INFO[2],
				sizeof(struct SENSOR_VC_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
			memcpy(
				(void *)pvcinfo,
				(void *)&SENSOR_VC_INFO[3],
				sizeof(struct SENSOR_VC_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
			memcpy(
				(void *)pvcinfo,
				(void *)&SENSOR_VC_INFO[4],
				sizeof(struct SENSOR_VC_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		default:
			memcpy(
				(void *)pvcinfo,
				(void *)&SENSOR_VC_INFO[0],
				sizeof(struct SENSOR_VC_INFO_STRUCT));
			break;
		}
		break;
	case SENSOR_FEATURE_GET_SENSOR_PDAF_CAPACITY:
		LOG_INF(
		  "SENSOR_FEATURE_GET_SENSOR_PDAF_CAPACITY scenarioId:%d\n",
			(UINT16)*feature_data);
		/*PDAF capacity enable or not*/
		switch (*feature_data) {
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			*(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 0;
			break;
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			/* video & capture use same setting*/
			*(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 1;
			break;
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
			*(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 0;
			break;
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
			/*need to check*/
			*(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 0;
			break;
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
			*(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 1;
			break;
		case MSDK_SCENARIO_ID_CUSTOM1:
			*(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 0;
			break;
		default:
			*(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 0;
			break;
		}
		break;
	case SENSOR_FEATURE_GET_PDAF_DATA:	/*get cal data from eeprom*/
		LOG_INF("SENSOR_FEATURE_GET_PDAF_DATA\n");
		if (0)
			read_2P6_eeprom((kal_uint16)(*feature_data),
				(char *)(uintptr_t)(*(feature_data+1)),
					(kal_uint32)(*(feature_data+2)));
		LOG_INF("SENSOR_FEATURE_GET_PDAF_DATA success\n");
		break;
	case SENSOR_FEATURE_SET_PDAF:
			LOG_INF("PDAF mode :%d\n", *feature_data_16);
			imgsensor.pdaf_mode = *feature_data_16;
			break;
	case SENSOR_FEATURE_SET_SHUTTER_FRAME_TIME:/*lzl*/
			LOG_INF("SENSOR_FEATURE_SET_SHUTTER_FRAME_TIME\n");
			set_shutter_frame_length((UINT16)*feature_data,
					(UINT16)*(feature_data+1));
			break;
	/******************** STREAMING RESUME/SUSPEND *********/
	case SENSOR_FEATURE_SET_STREAMING_SUSPEND:
		LOG_INF("SENSOR_FEATURE_SET_STREAMING_SUSPEND\n");
		streaming_control(KAL_FALSE);
		break;
	case SENSOR_FEATURE_SET_STREAMING_RESUME:
		LOG_INF("SENSOR_FEATURE_SET_STREAMING_RESUME, shutter:%llu\n",
			*feature_data);
		if (*feature_data != 0)
			set_shutter(*feature_data);
		streaming_control(KAL_TRUE);
		break;
	case SENSOR_FEATURE_GET_PIXEL_RATE:
			LOG_INF("SENSOR_FEATURE_GET_PIXEL_RATE\n");
		switch (*feature_data) {
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
			(imgsensor_info.cap.pclk /
			(imgsensor_info.cap.linelength - 80))*
			imgsensor_info.cap.grabwindow_width;

			break;
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
			(imgsensor_info.normal_video.pclk /
			(imgsensor_info.normal_video.linelength - 80))*
			imgsensor_info.normal_video.grabwindow_width;

			break;
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
			(imgsensor_info.hs_video.pclk /
			(imgsensor_info.hs_video.linelength - 80))*
			imgsensor_info.hs_video.grabwindow_width;

			break;
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
			(imgsensor_info.slim_video.pclk /
			(imgsensor_info.slim_video.linelength - 80))*
			imgsensor_info.slim_video.grabwindow_width;

			break;
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		default:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
			(imgsensor_info.pre.pclk /
			(imgsensor_info.pre.linelength - 80))*
			imgsensor_info.pre.grabwindow_width;
			break;
		}
		break;

	case SENSOR_FEATURE_GET_MIPI_PIXEL_RATE:
		fps = (MUINT32)(*(feature_data + 2));

		switch (*feature_data) {
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			if (fps == 240)
				*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
					imgsensor_info.cap1.mipi_pixel_rate;
			else if (fps == 150)
				*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
					imgsensor_info.cap2.mipi_pixel_rate;
			else
				*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
					imgsensor_info.cap.mipi_pixel_rate;
			break;
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
				imgsensor_info.normal_video.mipi_pixel_rate;
			break;
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
				imgsensor_info.hs_video.mipi_pixel_rate;
			break;
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
				imgsensor_info.slim_video.mipi_pixel_rate;
			break;
		case MSDK_SCENARIO_ID_CUSTOM1:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
				imgsensor_info.custom1.mipi_pixel_rate;
			break;
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		default:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
				imgsensor_info.pre.mipi_pixel_rate;
			break;
		}

		break;

	default:
			break;
	}

	return ERROR_NONE;
}	/*	feature_control()  */

static struct SENSOR_FUNCTION_STRUCT sensor_func = {
	open,
	get_info,
	get_resolution,
	feature_control,
	control,
	close
};

UINT32 S5K2P6_MIPI_RAW_SensorInit(struct SENSOR_FUNCTION_STRUCT **pfFunc)
{
	/* To Do : Check Sensor status here */
	if (pfFunc != NULL)
		*pfFunc = &sensor_func;
	return ERROR_NONE;
}
