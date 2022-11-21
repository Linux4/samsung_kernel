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
 *	 n21_dongci_main_s5k2p6mipi_Sensor.c
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
#define PFX "N21_DONGCI_MAIN_S5K2P6_camera_sensor"
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

#include "n21_dongci_main_s5k2p6mipiraw_Sensor.h"


#undef ORINGNAL_VERSION

//+Bug 492443, sunhushan.wt, ADD, 2019.10.15, porting the otp of the 12sensor 
#define EEPROM_CN24C64AH_ID 0xB0

static kal_uint16 read_eeprom(kal_uint32 addr)
{
	kal_uint16 get_byte=0;

	char pu_send_cmd[2] = { (char)(addr >> 8), (char)(addr & 0xFF) };
	iReadRegI2C(pu_send_cmd, 2, (u8*)&get_byte, 1, EEPROM_CN24C64AH_ID);

	return get_byte;
}
//+Bug 492443, sunhushan.wt, ADD, 2019.10.15, porting the otp of the 12sensor 

#define MULTI_WRITE 1

#if MULTI_WRITE
#define I2C_BUFFER_LEN 1020	/* trans# max is 255, each 4 bytes */
#else
#define I2C_BUFFER_LEN 4
#endif


static DEFINE_SPINLOCK(imgsensor_drv_lock);


static struct imgsensor_info_struct imgsensor_info = {
	.sensor_id = N21_DONGCI_MAIN_S5K2P6_SENSOR_ID,

	.checksum_value = 0xb1f1b3cc,

	.pre = {
		.pclk = 576000000,              /* record different mode's pclk */
		.linelength = 5152,				/* record different mode's linelength */
		.framelength = 3724,            /* record different mode's framelength */
		.startx = 0,	/*record different mode's startx of grabwindow*/
		.starty = 0,	/*record different mode's starty of grabwindow*/

		/*record different mode's width of grabwindow*/
		.grabwindow_width = 2304,//+bug537596 guojiabin.wt,MODIFY,20200306,modify sensor mode error
		/*record different mode's height of grabwindow*/
		.grabwindow_height = 1728,//+bug537596 guojiabin.wt,MODIFY,20200306,modify sensor mode error

		/* following for MIPIDataLowPwr2HighSpeedSettleDelayCount
		 * by different scenario
		 */
		.mipi_data_lp2hs_settle_dc = 85,
		/*	 following for GetDefaultFramerateByScenario()	*/
		.max_framerate = 300,
		.mipi_pixel_rate = 360000000,//590000000
	},
        //+Bug 524054, wanglei6.wt,MODIFY, 2019.12.23, modify image size and add log to the algorithm
	.cap = {
		.pclk = 576000000,
		.linelength = 5312,
		.framelength = 3613,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 4608,
		.grabwindow_height = 3456,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 300,
		.mipi_pixel_rate = 595200000,//590000000
	},
	.cap1 = {
		.pclk = 576000000,
		.linelength = 6996,
		.framelength = 3558,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 4608,
		.grabwindow_height = 3456,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 240,
		.mipi_pixel_rate = 398400000,
	},
    .cap2 = {
        .pclk = 576000000,
		.linelength = 10624,
		.framelength = 3613,
		.startx = 0,
        .starty = 0,
		.grabwindow_width = 4608,
		.grabwindow_height = 3456,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 150,
		.mipi_pixel_rate = 297600000,
	},
	.normal_video = {
		.pclk = 576000000,
		.linelength = 5312,
		.framelength = 3613,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 4608,
		.grabwindow_height = 3456,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 300,
		.mipi_pixel_rate = 595200000,
	},
        //-Bug 524054, wanglei6.wt,MODIFY, 2019.12.23, modify image size and add log to the algorithm
	.hs_video = {
		.pclk = 576000000,
		.linelength = 3408,
		.framelength = 1408,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 1920,
		.grabwindow_height = 1080,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 1200,
		.mipi_pixel_rate = 192000000,

	},
	.slim_video = {
		.pclk = 576000000,				/* record different mode's pclk */
		.linelength = 5312,			/* record different mode's linelength */
		.framelength = 3613,	        /* record different mode's framelength */
		.startx = 0,					/* record different mode's startx of grabwindow */
		.starty = 0,					/* record different mode's starty of grabwindow */
		.grabwindow_width = 1080,		/* record different mode's width of grabwindow */
		.grabwindow_height = 720,		/* record different mode's height of grabwindow */
		/*	 following for MIPIDataLowPwr2HighSpeedSettleDelayCount by different scenario	*/
		.mipi_data_lp2hs_settle_dc = 85,
		/*	 following for GetDefaultFramerateByScenario()	*/
		.max_framerate = 300,
		.mipi_pixel_rate = 192000000,

	},
	.margin = 4,                    /*sensor framelength & shutter margin*/
	.min_shutter = 4,               /*min shutter*/

	/*max framelength by sensor register's limitation*/
	.max_frame_length = 0xffff,
	/*shutter delay frame for AE cycle, 2 frame*/
	.ae_shut_delay_frame = 0,
	/*sensor gain delay frame for AE cycle,2 frame*/
	.ae_sensor_gain_delay_frame = 0,
        .frame_time_delay_frame = 1,//The delay frame of setting frame length
	.ae_ispGain_delay_frame = 2,    /*isp gain delay frame for AE cycle*/
	.ihdr_support = 0,	            /*1, support; 0,not support*/
	.ihdr_le_firstline = 0,         /*1,le first ; 0, se first*/
	.sensor_mode_num = 5,	        /*support sensor mode num*/

	.cap_delay_frame = 3,           /*enter capture delay frame num*/
	.pre_delay_frame = 3,           /*enter preview delay frame num*/
	.video_delay_frame = 3,         /*enter video delay frame num*/
	.hs_video_delay_frame = 3,   /*enter high speed video  delay frame num*/
	.slim_video_delay_frame = 3, /*enter slim video delay frame num*/

	.isp_driving_current = ISP_DRIVING_6MA,     /*mclk driving current*/

	/*sensor_interface_type*/
	.sensor_interface_type = SENSOR_INTERFACE_TYPE_MIPI,
	/*0,MIPI_OPHY_NCSI2;  1,MIPI_OPHY_CSI2*/
	.mipi_sensor_type = MIPI_OPHY_NCSI2,
	/*0,MIPI_SETTLEDELAY_AUTO; 1,MIPI_SETTLEDELAY_MANNUAL*/
	.mipi_settle_delay_mode = 0,
	.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_Gb,
	.mclk = 24,         /*mclk value, suggest 24 or 26 for 24Mhz or 26Mhz*/
	.mipi_lane_num = SENSOR_MIPI_4_LANE,

	/*record sensor support all write id addr*/
	.i2c_addr_table = {0x5A, 0xff},
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

	.i2c_write_id = 0x20,  /*record current sensor's i2c write id*/

};

/* VC_Num, VC_PixelNum, ModeSelect, EXPO_Ratio, ODValue, RG_STATSMODE*/
/* VC0_ID, VC0_DataType, VC0_SIZEH, VC0_SIZE,
 * VC1_ID, VC1_DataType, VC1_SIZEH, VC1_SIZEV
 */

/* VC2_ID, VC2_DataType, VC2_SIZEH, VC2_SIZE,
 * VC3_ID, VC3_DataType, VC3_SIZEH, VC3_SIZEV
 */

/* Preview mode setting */
//+bug528935,wanglei6.wt,MODIFY,20200110,The rear main camera does not support pdaf
static struct SENSOR_VC_INFO_STRUCT SENSOR_VC_INFO[3] = {
	{0x02, 0x0A,   0x00,   0x08, 0x40, 0x00,
	0x00, 0x2B, 0x0900, 0x06C0, 0x01, 0x00, 0x0000, 0x0000,   //+bug537596 guojiabin.wt,MODIFY,20200306,modify sensor mode error
	0x01, 0x30, 0x0168, 0x06C0, 0x03, 0x00, 0x0000, 0x0000},
	/* Video mode setting */
	{0x02, 0x0A,   0x00,   0x08, 0x40, 0x00,
	0x00, 0x2B, 0x1220, 0x0DA0, 0x01, 0x00, 0x0000, 0x0000,
	0x01, 0x30, 0x0168, 0x06C0, 0x03, 0x00, 0x0000, 0x0000},
	/* Capture mode setting */
	{0x02, 0x0A,   0x00,   0x08, 0x40, 0x00,
	0x00, 0x2B, 0x1200, 0x0D80, 0x01, 0x00, 0x0000, 0x0000,
	0x01, 0x30, 0x0168, 0x06C0, 0x03, 0x00, 0x0000, 0x0000}
	};
//-bug528935,wanglei6.wt,MODIFY,20200110,The rear main camera does not support pdaf

/* Sensor output window information */
static struct SENSOR_WINSIZE_INFO_STRUCT imgsensor_winsize_info[5] = {
 { 4640, 3488,  16,	16, 4608, 3456, 2304,  1728, 0000, 0000, 2304, 1728,	    0,	0, 2304, 1728}, ////+bug537596 guojiabin.wt,MODIFY,20200306,modify sensor mode error
 //+Bug 524054, wanglei6.wt,MODIFY, 2019.12.23, modify image size and add log to the algorithm
 //bug528935,wanglei6.wt,MODIFY,20200110,The rear main camera does not support pdaf
 { 4640, 3488,  16,  16, 4608, 3456, 4608,  3456, 0000, 0000, 4608, 3456,        0,    0, 4608, 3456}, /* capture */
 { 4640, 3488,  16,  16, 4608, 3456, 4608,  3456, 0000, 0000, 4608, 3456,	 0,    0, 4608, 3456}, /* video */
 //-Bug 524054, wanglei6.wt,MODIFY, 2019.12.23, modify image size and add log to the algorithm
 { 4640, 3488,  400, 664, 3840, 2160, 1920,  1080, 0000, 0000, 1920,  1080,     0,  0, 1920,  1080}, /* hight speed video */
 { 4640, 3488,  1240, 1024, 2160, 1440, 1080,   720, 0000, 0000, 1080,  720,     0,  0, 1080,  720}, /* slim video */

};

/*no mirror flip*/
static struct SET_PD_BLOCK_INFO_T imgsensor_pd_info = {
    .i4OffsetX = 16,
    .i4OffsetY = 16,
    .i4PitchX  = 32,
    .i4PitchY  = 32,
    .i4PairNum = 16,
    .i4SubBlkW = 8,
    .i4SubBlkH = 8,
    .i4PosL = {{19, 16}, {27, 16}, {35, 16}, {43, 16}, {23, 24}, {31, 24}, {39, 24}, {47, 24}, {19, 32}, {27, 32}, {35, 32}, {43, 32}, {23, 40}, {31, 40}, {39, 40}, {47, 40} },
    .i4PosR = {{18, 16}, {26, 16}, {34, 16}, {42, 16}, {22, 24}, {30, 24}, {38, 24}, {46, 24}, {18, 32}, {26, 32}, {34, 32}, {42, 32}, {22, 40}, {30, 40}, {38, 40}, {46, 40} },
    .iMirrorFlip = 0,
    .i4BlockNumX = 144,
    .i4BlockNumY = 108,
};


static kal_uint16 read_cmos_sensor(kal_uint32 addr)
{
	kal_uint16 get_byte = 0;
	char pusendcmd[2] = {(char)(addr >> 8), (char)(addr & 0xFF) };

	iReadRegI2C(pusendcmd, 2, (u8 *)&get_byte, 2, imgsensor.i2c_write_id);
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
	pr_debug("dummyline = %d, dummypixels = %d\n",
		imgsensor.dummy_line, imgsensor.dummy_pixel);
	/*return; //for test*/
	write_cmos_sensor(0x0340, imgsensor.frame_length);
	write_cmos_sensor(0x0342, imgsensor.line_length);
}	/*	set_dummy  */

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
	pr_debug("%s exit!\n", __func__);
}

static kal_uint32 streaming_control(kal_bool enable)
{
	pr_debug("streaming_enable(0=Sw Standby,1=streaming): %d\n", enable);

	if (enable) {
		write_cmos_sensor_8(0x0100, 0X01);
	} else {
		write_cmos_sensor(0x6028, 0x4000);
		write_cmos_sensor_8(0x0100, 0x00);
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
		pr_debug("(else)imgsensor.frame_length = %d\n",
				imgsensor.frame_length);

	}
	/* Update Shutter*/
	write_cmos_sensor_8(0x0104, 0x01);
	write_cmos_sensor(0x0202, shutter);
	write_cmos_sensor_8(0x0104, 0x00);
	pr_debug("shutter =%d, framelength =%d\n",
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

	pr_debug("Add for N3D! shutterlzl =%d, framelength =%d\n",
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
	pr_debug("gain = %d , reg_gain = 0x%x\n", gain, reg_gain);

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
#if 0
static void night_mode(kal_bool enable)
{
/*No Need to implement this function*/
}	/*	night_mode	*/
#endif


#define USE_TNP_BURST	1
static void check_output_stream_off(void)
{
	kal_uint16 read_count = 0, read_register0005_value = 0;

	for (read_count = 0; read_count <= 4; read_count++)
	{
		read_register0005_value = read_cmos_sensor_8(0x0005);

		if (read_register0005_value == 0xff)
			break;
		mdelay(50);

		if (read_count == 4)
			pr_debug("stream off error\n");
	}
}
static void sensor_init(void)
{
pr_debug("sensor_init() E\n");
write_cmos_sensor(0x6028, 0x4000); 
write_cmos_sensor(0x0000, 0x0130); 
write_cmos_sensor(0x0000, 0x2106); 
write_cmos_sensor(0x6214, 0x7971); 
write_cmos_sensor(0x6218, 0x7150); 
write_cmos_sensor(0x6230, 0x0000); 
write_cmos_sensor(0x0A02, 0x5F00); 
write_cmos_sensor(0x6028, 0x2000); 
write_cmos_sensor(0x602A, 0x3684); 
write_cmos_sensor(0x6F12, 0x0000); 
write_cmos_sensor(0x6F12, 0x0000); 
write_cmos_sensor(0x6F12, 0x0000); 
write_cmos_sensor(0x6F12, 0x0000); 
write_cmos_sensor(0x6F12, 0x0000); 
write_cmos_sensor(0x6F12, 0x0000); 
write_cmos_sensor(0x6F12, 0x0449); 
write_cmos_sensor(0x6F12, 0x0348); 
write_cmos_sensor(0x6F12, 0x044A); 
write_cmos_sensor(0x6F12, 0x0860); 
write_cmos_sensor(0x6F12, 0x101A); 
write_cmos_sensor(0x6F12, 0x8880); 
write_cmos_sensor(0x6F12, 0x00F0); 
write_cmos_sensor(0x6F12, 0xA4B8); 
write_cmos_sensor(0x6F12, 0x2000); 
write_cmos_sensor(0x6F12, 0x3924); 
write_cmos_sensor(0x6F12, 0x2000); 
write_cmos_sensor(0x6F12, 0x29A0); 
write_cmos_sensor(0x6F12, 0x2000); 
write_cmos_sensor(0x6F12, 0x6FD4); 
write_cmos_sensor(0x6F12, 0x0000); 
write_cmos_sensor(0x6F12, 0x0000); 
write_cmos_sensor(0x6F12, 0x70B5); 
write_cmos_sensor(0x6F12, 0x6948); 
write_cmos_sensor(0x6F12, 0x0022); 
write_cmos_sensor(0x6F12, 0x0168); 
write_cmos_sensor(0x6F12, 0x0C0C); 
write_cmos_sensor(0x6F12, 0x8DB2); 
write_cmos_sensor(0x6F12, 0x2946); 
write_cmos_sensor(0x6F12, 0x2046); 
write_cmos_sensor(0x6F12, 0x00F0); 
write_cmos_sensor(0x6F12, 0xE8F8); 
write_cmos_sensor(0x6F12, 0x00F0); 
write_cmos_sensor(0x6F12, 0xEBF8); 
write_cmos_sensor(0x6F12, 0x0122); 
write_cmos_sensor(0x6F12, 0x2946); 
write_cmos_sensor(0x6F12, 0x2046); 
write_cmos_sensor(0x6F12, 0x00F0); 
write_cmos_sensor(0x6F12, 0xE1F8); 
write_cmos_sensor(0x6F12, 0x6248); 
write_cmos_sensor(0x6F12, 0x417A); 
write_cmos_sensor(0x6F12, 0x21F0); 
write_cmos_sensor(0x6F12, 0x0801); 
write_cmos_sensor(0x6F12, 0x4172); 
write_cmos_sensor(0x6F12, 0x70BD); 
write_cmos_sensor(0x6F12, 0x70B5); 
write_cmos_sensor(0x6F12, 0x5D48); 
write_cmos_sensor(0x6F12, 0x0022); 
write_cmos_sensor(0x6F12, 0x4168); 
write_cmos_sensor(0x6F12, 0x0C0C); 
write_cmos_sensor(0x6F12, 0x8DB2); 
write_cmos_sensor(0x6F12, 0x2946); 
write_cmos_sensor(0x6F12, 0x2046); 
write_cmos_sensor(0x6F12, 0x00F0); 
write_cmos_sensor(0x6F12, 0xD1F8); 
write_cmos_sensor(0x6F12, 0x00F0); 
write_cmos_sensor(0x6F12, 0xD9F8); 
write_cmos_sensor(0x6F12, 0x5A4A); 
write_cmos_sensor(0x6F12, 0x0220); 
write_cmos_sensor(0x6F12, 0x5A49); 
write_cmos_sensor(0x6F12, 0x9080); 
write_cmos_sensor(0x6F12, 0xD080); 
write_cmos_sensor(0x6F12, 0x0020); 
write_cmos_sensor(0x6F12, 0x1080); 
write_cmos_sensor(0x6F12, 0x0120); 
write_cmos_sensor(0x6F12, 0x8882); 
write_cmos_sensor(0x6F12, 0x0246); 
write_cmos_sensor(0x6F12, 0x2946); 
write_cmos_sensor(0x6F12, 0x2046); 
write_cmos_sensor(0x6F12, 0xBDE8); 
write_cmos_sensor(0x6F12, 0x7040); 
write_cmos_sensor(0x6F12, 0x00F0); 
write_cmos_sensor(0x6F12, 0xBFB8); 
write_cmos_sensor(0x6F12, 0x70B5); 
write_cmos_sensor(0x6F12, 0x4F48); 
write_cmos_sensor(0x6F12, 0x0022); 
write_cmos_sensor(0x6F12, 0x8168); 
write_cmos_sensor(0x6F12, 0x0C0C); 
write_cmos_sensor(0x6F12, 0x8DB2); 
write_cmos_sensor(0x6F12, 0x2946); 
write_cmos_sensor(0x6F12, 0x2046); 
write_cmos_sensor(0x6F12, 0x00F0); 
write_cmos_sensor(0x6F12, 0xB5F8); 
write_cmos_sensor(0x6F12, 0x4F48); 
write_cmos_sensor(0x6F12, 0x30F8); 
write_cmos_sensor(0x6F12, 0x6C1F); 
write_cmos_sensor(0x6F12, 0x01F0); 
write_cmos_sensor(0x6F12, 0x8002); 
write_cmos_sensor(0x6F12, 0x0280); 
write_cmos_sensor(0x6F12, 0x1006); 
write_cmos_sensor(0x6F12, 0x01D5); 
write_cmos_sensor(0x6F12, 0x00F0); 
write_cmos_sensor(0x6F12, 0xBAF8); 
write_cmos_sensor(0x6F12, 0x2946); 
write_cmos_sensor(0x6F12, 0x2046); 
write_cmos_sensor(0x6F12, 0xBDE8); 
write_cmos_sensor(0x6F12, 0x7040); 
write_cmos_sensor(0x6F12, 0x0122); 
write_cmos_sensor(0x6F12, 0x00F0); 
write_cmos_sensor(0x6F12, 0xA4B8); 
write_cmos_sensor(0x6F12, 0x2DE9); 
write_cmos_sensor(0x6F12, 0xF041); 
write_cmos_sensor(0x6F12, 0x434E); 
write_cmos_sensor(0x6F12, 0x0F46); 
write_cmos_sensor(0x6F12, 0x0024); 
write_cmos_sensor(0x6F12, 0xB188); 
write_cmos_sensor(0x6F12, 0x444D); 
write_cmos_sensor(0x6F12, 0x8142); 
write_cmos_sensor(0x6F12, 0x0BD0); 
write_cmos_sensor(0x6F12, 0x2846); 
write_cmos_sensor(0x6F12, 0x90F8); 
write_cmos_sensor(0x6F12, 0x8B00); 
write_cmos_sensor(0x6F12, 0x29B1); 
write_cmos_sensor(0x6F12, 0x0D21); 
write_cmos_sensor(0x6F12, 0x00F0); 
write_cmos_sensor(0x6F12, 0xA8F8); 
write_cmos_sensor(0x6F12, 0x95F8); 
write_cmos_sensor(0x6F12, 0x8B40); 
write_cmos_sensor(0x6F12, 0x01E0); 
write_cmos_sensor(0x6F12, 0x00F0); 
write_cmos_sensor(0x6F12, 0xA8F8); 
write_cmos_sensor(0x6F12, 0xF088); 
write_cmos_sensor(0x6F12, 0xB842); 
write_cmos_sensor(0x6F12, 0x12D0); 
write_cmos_sensor(0x6F12, 0x48B1); 
write_cmos_sensor(0x6F12, 0x15F8); 
write_cmos_sensor(0x6F12, 0x8A0F); 
write_cmos_sensor(0x6F12, 0x00F0); 
write_cmos_sensor(0x6F12, 0xA0F8); 
write_cmos_sensor(0x6F12, 0x2878); 
write_cmos_sensor(0x6F12, 0xBDE8); 
write_cmos_sensor(0x6F12, 0xF041); 
write_cmos_sensor(0x6F12, 0x0821); 
write_cmos_sensor(0x6F12, 0x00F0); 
write_cmos_sensor(0x6F12, 0x95B8); 
write_cmos_sensor(0x6F12, 0x95F8); 
write_cmos_sensor(0x6F12, 0x8A00); 
write_cmos_sensor(0x6F12, 0xA042); 
write_cmos_sensor(0x6F12, 0x03D0); 
write_cmos_sensor(0x6F12, 0xBDE8); 
write_cmos_sensor(0x6F12, 0xF041); 
write_cmos_sensor(0x6F12, 0x00F0); 
write_cmos_sensor(0x6F12, 0x92B8); 
write_cmos_sensor(0x6F12, 0xBDE8); 
write_cmos_sensor(0x6F12, 0xF081); 
write_cmos_sensor(0x6F12, 0x7047); 
write_cmos_sensor(0x6F12, 0x70B5); 
write_cmos_sensor(0x6F12, 0x3148); 
write_cmos_sensor(0x6F12, 0x018C); 
write_cmos_sensor(0x6F12, 0x49B1); 
write_cmos_sensor(0x6F12, 0x2E49); 
write_cmos_sensor(0x6F12, 0x408C); 
write_cmos_sensor(0x6F12, 0x496E); 
write_cmos_sensor(0x6F12, 0x4843); 
write_cmos_sensor(0x6F12, 0x4FF4); 
write_cmos_sensor(0x6F12, 0x7A71); 
write_cmos_sensor(0x6F12, 0xB0FB); 
write_cmos_sensor(0x6F12, 0xF1F0); 
write_cmos_sensor(0x6F12, 0x00F0); 
write_cmos_sensor(0x6F12, 0x86F8); 
write_cmos_sensor(0x6F12, 0x2448); 
write_cmos_sensor(0x6F12, 0x0022); 
write_cmos_sensor(0x6F12, 0x0169); 
write_cmos_sensor(0x6F12, 0x0C0C); 
write_cmos_sensor(0x6F12, 0x8DB2); 
write_cmos_sensor(0x6F12, 0x2946); 
write_cmos_sensor(0x6F12, 0x2046); 
write_cmos_sensor(0x6F12, 0x00F0); 
write_cmos_sensor(0x6F12, 0x5FF8); 
write_cmos_sensor(0x6F12, 0x00F0); 
write_cmos_sensor(0x6F12, 0x80F8); 
write_cmos_sensor(0x6F12, 0x2946); 
write_cmos_sensor(0x6F12, 0x2046); 
write_cmos_sensor(0x6F12, 0xBDE8); 
write_cmos_sensor(0x6F12, 0x7040); 
write_cmos_sensor(0x6F12, 0x0122); 
write_cmos_sensor(0x6F12, 0x00F0); 
write_cmos_sensor(0x6F12, 0x56B8); 
write_cmos_sensor(0x6F12, 0x10B5); 
write_cmos_sensor(0x6F12, 0x0022); 
write_cmos_sensor(0x6F12, 0xAFF2); 
write_cmos_sensor(0x6F12, 0x3F11); 
write_cmos_sensor(0x6F12, 0x2048); 
write_cmos_sensor(0x6F12, 0x00F0); 
write_cmos_sensor(0x6F12, 0x77F8); 
write_cmos_sensor(0x6F12, 0x184C); 
write_cmos_sensor(0x6F12, 0x0022); 
write_cmos_sensor(0x6F12, 0xAFF2); 
write_cmos_sensor(0x6F12, 0x1D11); 
write_cmos_sensor(0x6F12, 0x2060); 
write_cmos_sensor(0x6F12, 0x1D48); 
write_cmos_sensor(0x6F12, 0x00F0); 
write_cmos_sensor(0x6F12, 0x6FF8); 
write_cmos_sensor(0x6F12, 0x0022); 
write_cmos_sensor(0x6F12, 0xAFF2); 
write_cmos_sensor(0x6F12, 0xF501); 
write_cmos_sensor(0x6F12, 0x6060); 
write_cmos_sensor(0x6F12, 0x1B48); 
write_cmos_sensor(0x6F12, 0x00F0); 
write_cmos_sensor(0x6F12, 0x68F8); 
write_cmos_sensor(0x6F12, 0x0022); 
write_cmos_sensor(0x6F12, 0xAFF2); 
write_cmos_sensor(0x6F12, 0xCB01); 
write_cmos_sensor(0x6F12, 0xA060); 
write_cmos_sensor(0x6F12, 0x1848); 
write_cmos_sensor(0x6F12, 0x00F0); 
write_cmos_sensor(0x6F12, 0x61F8); 
write_cmos_sensor(0x6F12, 0xE060); 
write_cmos_sensor(0x6F12, 0x0D48); 
write_cmos_sensor(0x6F12, 0x184B); 
write_cmos_sensor(0x6F12, 0x164A); 
write_cmos_sensor(0x6F12, 0xC18C); 
write_cmos_sensor(0x6F12, 0x43F8); 
write_cmos_sensor(0x6F12, 0x2120); 
write_cmos_sensor(0x6F12, 0x4A1C); 
write_cmos_sensor(0x6F12, 0xC284); 
write_cmos_sensor(0x6F12, 0x4FF4); 
write_cmos_sensor(0x6F12, 0xFE70); 
write_cmos_sensor(0x6F12, 0x00F0); 
write_cmos_sensor(0x6F12, 0x59F8); 
write_cmos_sensor(0x6F12, 0x0022); 
write_cmos_sensor(0x6F12, 0xAFF2); 
write_cmos_sensor(0x6F12, 0x9901); 
write_cmos_sensor(0x6F12, 0x1248); 
write_cmos_sensor(0x6F12, 0x00F0); 
write_cmos_sensor(0x6F12, 0x4EF8); 
write_cmos_sensor(0x6F12, 0x0022); 
write_cmos_sensor(0x6F12, 0xAFF2); 
write_cmos_sensor(0x6F12, 0xA301); 
write_cmos_sensor(0x6F12, 0x1048); 
write_cmos_sensor(0x6F12, 0x00F0); 
write_cmos_sensor(0x6F12, 0x48F8); 
write_cmos_sensor(0x6F12, 0x2061); 
write_cmos_sensor(0x6F12, 0x10BD); 
write_cmos_sensor(0x6F12, 0x2000); 
write_cmos_sensor(0x6F12, 0x3910); 
write_cmos_sensor(0x6F12, 0x2000); 
write_cmos_sensor(0x6F12, 0x29A0); 
write_cmos_sensor(0x6F12, 0x2000); 
write_cmos_sensor(0x6F12, 0x26F0); 
write_cmos_sensor(0x6F12, 0x2000); 
write_cmos_sensor(0x6F12, 0x0420); 
write_cmos_sensor(0x6F12, 0x4000); 
write_cmos_sensor(0x6F12, 0x7000); 
write_cmos_sensor(0x6F12, 0x2000); 
write_cmos_sensor(0x6F12, 0x29E0); 
write_cmos_sensor(0x6F12, 0x2000); 
write_cmos_sensor(0x6F12, 0x6FD4); 
write_cmos_sensor(0x6F12, 0x0000); 
write_cmos_sensor(0x6F12, 0x992F); 
write_cmos_sensor(0x6F12, 0x0000); 
write_cmos_sensor(0x6F12, 0x4287); 
write_cmos_sensor(0x6F12, 0x0000); 
write_cmos_sensor(0x6F12, 0x06B3); 
write_cmos_sensor(0x6F12, 0x0000); 
write_cmos_sensor(0x6F12, 0x42ED); 
write_cmos_sensor(0x6F12, 0x03E0); 
write_cmos_sensor(0x6F12, 0x0146); 
write_cmos_sensor(0x6F12, 0x2000); 
write_cmos_sensor(0x6F12, 0x3580); 
write_cmos_sensor(0x6F12, 0x0000); 
write_cmos_sensor(0x6F12, 0x077B); 
write_cmos_sensor(0x6F12, 0x0000); 
write_cmos_sensor(0x6F12, 0x464F); 
write_cmos_sensor(0x6F12, 0x40F6); 
write_cmos_sensor(0x6F12, 0x4F2C); 
write_cmos_sensor(0x6F12, 0xC0F2); 
write_cmos_sensor(0x6F12, 0x000C); 
write_cmos_sensor(0x6F12, 0x6047); 
write_cmos_sensor(0x6F12, 0x49F6); 
write_cmos_sensor(0x6F12, 0x2F1C); 
write_cmos_sensor(0x6F12, 0xC0F2); 
write_cmos_sensor(0x6F12, 0x000C); 
write_cmos_sensor(0x6F12, 0x6047); 
write_cmos_sensor(0x6F12, 0x44F2); 
write_cmos_sensor(0x6F12, 0x872C); 
write_cmos_sensor(0x6F12, 0xC0F2); 
write_cmos_sensor(0x6F12, 0x000C); 
write_cmos_sensor(0x6F12, 0x6047); 
write_cmos_sensor(0x6F12, 0x40F2); 
write_cmos_sensor(0x6F12, 0xB36C); 
write_cmos_sensor(0x6F12, 0xC0F2); 
write_cmos_sensor(0x6F12, 0x000C); 
write_cmos_sensor(0x6F12, 0x6047); 
write_cmos_sensor(0x6F12, 0x4BF6); 
write_cmos_sensor(0x6F12, 0x295C); 
write_cmos_sensor(0x6F12, 0xC0F2); 
write_cmos_sensor(0x6F12, 0x000C); 
write_cmos_sensor(0x6F12, 0x6047); 
write_cmos_sensor(0x6F12, 0x4BF6); 
write_cmos_sensor(0x6F12, 0x035C); 
write_cmos_sensor(0x6F12, 0xC0F2); 
write_cmos_sensor(0x6F12, 0x000C); 
write_cmos_sensor(0x6F12, 0x6047); 
write_cmos_sensor(0x6F12, 0x4AF2); 
write_cmos_sensor(0x6F12, 0xF12C); 
write_cmos_sensor(0x6F12, 0xC0F2); 
write_cmos_sensor(0x6F12, 0x000C); 
write_cmos_sensor(0x6F12, 0x6047); 
write_cmos_sensor(0x6F12, 0x44F2); 
write_cmos_sensor(0x6F12, 0x4F6C); 
write_cmos_sensor(0x6F12, 0xC0F2); 
write_cmos_sensor(0x6F12, 0x000C); 
write_cmos_sensor(0x6F12, 0x6047); 
write_cmos_sensor(0x6F12, 0x4FF2); 
write_cmos_sensor(0x6F12, 0xC15C); 
write_cmos_sensor(0x6F12, 0xC0F2); 
write_cmos_sensor(0x6F12, 0x000C); 
write_cmos_sensor(0x6F12, 0x6047); 
write_cmos_sensor(0x6F12, 0x4FF2); 
write_cmos_sensor(0x6F12, 0x615C); 
write_cmos_sensor(0x6F12, 0xC0F2); 
write_cmos_sensor(0x6F12, 0x000C); 
write_cmos_sensor(0x6F12, 0x6047); 
write_cmos_sensor(0x6F12, 0x0000); 
write_cmos_sensor(0x6F12, 0x0000); 
write_cmos_sensor(0x6F12, 0x0000); 
write_cmos_sensor(0x6F12, 0x0000); 
write_cmos_sensor(0x6028, 0x4000); 
write_cmos_sensor(0x31E0, 0x0001); 

}	/*	sensor_init  */


static void preview_setting(void)
{
    pr_debug("preview_setting() E\n");
    write_cmos_sensor_8(0x0100, 0x00);
    check_output_stream_off();
//+bug537596 guojiabin.wt,MODIFY,20200306,modify sensor mode error
	write_cmos_sensor(0x3874, 0x0000);    
write_cmos_sensor(0x3876, 0x0102);    
write_cmos_sensor(0x3190, 0x0004);    
write_cmos_sensor(0x389A, 0x0011);    
write_cmos_sensor(0xF406, 0xFFF7);    
write_cmos_sensor(0x389C, 0x000B);    
write_cmos_sensor(0xF438, 0x000F);    
write_cmos_sensor(0xF46C, 0x000E);    
write_cmos_sensor(0xF43C, 0x0088);    
write_cmos_sensor(0xF496, 0x0010);    
write_cmos_sensor(0x38B4, 0x0050);    
write_cmos_sensor(0x31B2, 0x0200);    
write_cmos_sensor(0x31D4, 0x0001);    
write_cmos_sensor(0x31D6, 0x0103);    
write_cmos_sensor(0xF48A, 0x0060);    
write_cmos_sensor(0xF48E, 0x003F);    
write_cmos_sensor(0xF490, 0x0035);    
write_cmos_sensor(0x31B8, 0x0040);    
write_cmos_sensor(0x3220, 0x0004);    
write_cmos_sensor(0x3228, 0x0002);    
write_cmos_sensor(0x316C, 0x0071);    
write_cmos_sensor(0xF47C, 0x000C);    
write_cmos_sensor(0xF4BA, 0x0005);    
write_cmos_sensor(0x388C, 0x0507);    
write_cmos_sensor(0x3888, 0x0F17);    
write_cmos_sensor(0x388A, 0x0F17);    
write_cmos_sensor(0x3786, 0x0257);    
write_cmos_sensor(0x324A, 0x0095);    
write_cmos_sensor(0x3222, 0x0004);    
write_cmos_sensor(0x322A, 0x0002);    
write_cmos_sensor(0x6028, 0x2000);    
write_cmos_sensor(0x602A, 0x1A12);    
write_cmos_sensor(0x6F12, 0x0002);    
write_cmos_sensor(0x602A, 0x1A14);    
write_cmos_sensor(0x6F12, 0x0100);    
write_cmos_sensor(0x602A, 0x1A16);    
write_cmos_sensor(0x6F12, 0x0040);    
write_cmos_sensor(0x602A, 0x1A94);    
write_cmos_sensor(0x6F12, 0x0000);    
write_cmos_sensor(0x602A, 0x0760);    
write_cmos_sensor(0x6F12, 0x0000);    
write_cmos_sensor(0x602A, 0x0764);    
write_cmos_sensor(0x6F12, 0x0200);    
write_cmos_sensor(0x602A, 0x076A);    
write_cmos_sensor(0x6F12, 0x0200);    
write_cmos_sensor(0x602A, 0x0770);    
write_cmos_sensor(0x6F12, 0x0000);    
write_cmos_sensor(0x602A, 0x0772);    
write_cmos_sensor(0x6F12, 0x0000);    
write_cmos_sensor(0x602A, 0x0774);    
write_cmos_sensor(0x6F12, 0x0000);    
write_cmos_sensor(0x602A, 0x1EA8);    
write_cmos_sensor(0x6F12, 0x0001);    
write_cmos_sensor(0x30BC, 0x0000);    
write_cmos_sensor(0x30C8, 0x0100);    
write_cmos_sensor(0x0B0E, 0x0100);    
write_cmos_sensor(0x0340, 0x0EBC);    
write_cmos_sensor(0x0342, 0x1420);    
write_cmos_sensor(0x0344, 0x0018);    
write_cmos_sensor(0x0346, 0x0010);    
write_cmos_sensor(0x0348, 0x1217);    
write_cmos_sensor(0x034A, 0x0D8F);    
write_cmos_sensor(0x034C, 0x0900);    
write_cmos_sensor(0x034E, 0x06C0);    
write_cmos_sensor(0x0900, 0x0112);    
write_cmos_sensor(0x0380, 0x0001);    
write_cmos_sensor(0x0382, 0x0001);    
write_cmos_sensor(0x0384, 0x0001);    
write_cmos_sensor(0x0386, 0x0003);    
write_cmos_sensor(0x0400, 0x0001);    
write_cmos_sensor(0x0404, 0x0020);    
write_cmos_sensor(0x0136, 0x1800);    
write_cmos_sensor(0x0300, 0x0003);    
write_cmos_sensor(0x0302, 0x0001);    
write_cmos_sensor(0x0304, 0x0008);    
write_cmos_sensor(0x0306, 0x0090);    
write_cmos_sensor(0x0308, 0x0008);    
write_cmos_sensor(0x030A, 0x0001);    
write_cmos_sensor(0x030C, 0x0004);    
write_cmos_sensor(0x030E, 0x0096);    
write_cmos_sensor(0x300A, 0x0001);    
write_cmos_sensor(0x3004, 0x0006);    
write_cmos_sensor(0x0202, 0x0740);    
write_cmos_sensor(0x0204, 0x0020);    
write_cmos_sensor(0x6028, 0x2000);    
write_cmos_sensor(0x602A, 0x184E);    
write_cmos_sensor(0x6F12, 0x0100);    
write_cmos_sensor(0x602A, 0x1F72);    
write_cmos_sensor(0x6F12, 0x0F80);    
write_cmos_sensor(0x0114, 0x0301);
write_cmos_sensor(0x6028, 0x2000);    
write_cmos_sensor(0x602A, 0x0086);    
write_cmos_sensor(0x6F12, 0x0101);    
write_cmos_sensor(0x602A, 0x0088);    
write_cmos_sensor(0x6F12, 0x0100);    
write_cmos_sensor(0x0100, 0x0100);    
//+bug537596 guojiabin.wt,MODIFY,20200306,modify sensor mode error
}	/*	preview_setting  */

//+Bug 524054, wanglei6.wt,MODIFY, 2019.12.23, modify image size and add log to the algorithm
static void capture_setting(kal_uint16 currefps)
{
	pr_debug("capture_setting() E! currefps:%d\n", currefps);
 //   write_cmos_sensor_8(0x0100, 0x00);
//    check_output_stream_off();
    if (currefps == 300) {
write_cmos_sensor(0x3874, 0x0000);
write_cmos_sensor(0x3876, 0x0102);
write_cmos_sensor(0x3190, 0x0008);
write_cmos_sensor(0x389A, 0x0011);
write_cmos_sensor(0xF406, 0xFFF7);
write_cmos_sensor(0x389C, 0x000B);
write_cmos_sensor(0xF438, 0x000F);
write_cmos_sensor(0xF46C, 0x000E);
write_cmos_sensor(0xF43C, 0x0080);
write_cmos_sensor(0xF496, 0x0010);
write_cmos_sensor(0x38B4, 0x0050);
write_cmos_sensor(0x31B2, 0x0200);
write_cmos_sensor(0x31D4, 0x0001);
write_cmos_sensor(0x31D6, 0x0103);
write_cmos_sensor(0xF48A, 0x0060);
write_cmos_sensor(0xF48E, 0x003F);
write_cmos_sensor(0xF490, 0x0035);
write_cmos_sensor(0x31B8, 0x0038);
write_cmos_sensor(0x3220, 0x0004);
write_cmos_sensor(0x3228, 0x0002);
write_cmos_sensor(0x316C, 0xFFFF);
write_cmos_sensor(0xF47C, 0x000C);
write_cmos_sensor(0xF4BA, 0x0005);
write_cmos_sensor(0x388C, 0x0507);
write_cmos_sensor(0x3888, 0x0F17);
write_cmos_sensor(0x388A, 0x0F17);
write_cmos_sensor(0x3786, 0x0257);
write_cmos_sensor(0x324A, 0x0095);
write_cmos_sensor(0x3222, 0x0004);
write_cmos_sensor(0x322A, 0x0002);
write_cmos_sensor(0x6028, 0x2000);
write_cmos_sensor(0x602A, 0x1A12);
write_cmos_sensor(0x6F12, 0x0002);
write_cmos_sensor(0x602A, 0x1A14);
write_cmos_sensor(0x6F12, 0x0100);
write_cmos_sensor(0x602A, 0x1A16);
write_cmos_sensor(0x6F12, 0x0040);
write_cmos_sensor(0x602A, 0x1A94);
write_cmos_sensor(0x6F12, 0x0F50);
write_cmos_sensor(0x602A, 0x0760);
write_cmos_sensor(0x6F12, 0x0100);
write_cmos_sensor(0x602A, 0x0764);
write_cmos_sensor(0x6F12, 0x01F0);
write_cmos_sensor(0x602A, 0x076A);
write_cmos_sensor(0x6F12, 0x01FF);
write_cmos_sensor(0x602A, 0x0770);
write_cmos_sensor(0x6F12, 0x0080);
write_cmos_sensor(0x602A, 0x0772);
write_cmos_sensor(0x6F12, 0x00A0);
write_cmos_sensor(0x602A, 0x0774);
write_cmos_sensor(0x6F12, 0xF43C);
write_cmos_sensor(0x602A, 0x1EA8);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x30BC, 0x0001);
write_cmos_sensor(0x30C8, 0x0100);
write_cmos_sensor(0x0B0E, 0x0100);
write_cmos_sensor(0x0340, 0x0E1D);
write_cmos_sensor(0x0342, 0x14C0);
write_cmos_sensor(0x0344, 0x0018);
write_cmos_sensor(0x0346, 0x0010);
write_cmos_sensor(0x0348, 0x1217);
write_cmos_sensor(0x034A, 0x0D8F);
write_cmos_sensor(0x034C, 0x1200);
write_cmos_sensor(0x034E, 0x0D80);
write_cmos_sensor(0x0900, 0x0011);
write_cmos_sensor(0x0380, 0x0001);
write_cmos_sensor(0x0382, 0x0001);
write_cmos_sensor(0x0384, 0x0001);
write_cmos_sensor(0x0386, 0x0001);
write_cmos_sensor(0x0400, 0x0001);
write_cmos_sensor(0x0404, 0x0010);
write_cmos_sensor(0x0136, 0x1800);
write_cmos_sensor(0x0300, 0x0003);
write_cmos_sensor(0x0302, 0x0001);
write_cmos_sensor(0x0304, 0x0008);
write_cmos_sensor(0x0306, 0x0090);
write_cmos_sensor(0x0308, 0x0008);
write_cmos_sensor(0x030A, 0x0001);
write_cmos_sensor(0x030C, 0x0004);
write_cmos_sensor(0x030E, 0x007C);
write_cmos_sensor(0x300A, 0x0000);
write_cmos_sensor(0x3004, 0x0006);
write_cmos_sensor(0x0202, 0x0E10);
write_cmos_sensor(0x0204, 0x0020);
write_cmos_sensor(0x6028, 0x2000);
write_cmos_sensor(0x602A, 0x184E);
write_cmos_sensor(0x6F12, 0x0100);
write_cmos_sensor(0x602A, 0x1F72);
write_cmos_sensor(0x6F12, 0x0F80);
write_cmos_sensor(0x0114, 0x0301);
write_cmos_sensor(0x6028, 0x2000);
write_cmos_sensor(0x602A, 0x0086);
write_cmos_sensor(0x6F12, 0x0101);
write_cmos_sensor(0x602A, 0x0088);
write_cmos_sensor(0x6F12, 0x0100);
//write_cmos_sensor(0x0100, 0x0100);


    } else if(currefps==240){
 write_cmos_sensor(0x3874, 0x0000);
write_cmos_sensor(0x3876, 0x0102);
write_cmos_sensor(0x3190, 0x0008);
write_cmos_sensor(0x389A, 0x0011);
write_cmos_sensor(0xF406, 0xFFF7);
write_cmos_sensor(0x389C, 0x000B);
write_cmos_sensor(0xF438, 0x000F);
write_cmos_sensor(0xF46C, 0x000E);
write_cmos_sensor(0xF43C, 0x0080);
write_cmos_sensor(0xF496, 0x0010);
write_cmos_sensor(0x38B4, 0x0050);
write_cmos_sensor(0x31B2, 0x0200);
write_cmos_sensor(0x31D4, 0x0001);
write_cmos_sensor(0x31D6, 0x0103);
write_cmos_sensor(0xF48A, 0x0060);
write_cmos_sensor(0xF48E, 0x003F);
write_cmos_sensor(0xF490, 0x0035);
write_cmos_sensor(0x31B8, 0x0038);
write_cmos_sensor(0x3220, 0x0004);
write_cmos_sensor(0x3228, 0x0002);
write_cmos_sensor(0x316C, 0xFFFF);
write_cmos_sensor(0xF47C, 0x000C);
write_cmos_sensor(0xF4BA, 0x0005);
write_cmos_sensor(0x388C, 0x0507);
write_cmos_sensor(0x3888, 0x0F17);
write_cmos_sensor(0x388A, 0x0F17);
write_cmos_sensor(0x3786, 0x0257);
write_cmos_sensor(0x324A, 0x0095);
write_cmos_sensor(0x3222, 0x0004);
write_cmos_sensor(0x322A, 0x0002);
write_cmos_sensor(0x6028, 0x2000);
write_cmos_sensor(0x602A, 0x1A12);
write_cmos_sensor(0x6F12, 0x0002);
write_cmos_sensor(0x602A, 0x1A14);
write_cmos_sensor(0x6F12, 0x0100);
write_cmos_sensor(0x602A, 0x1A16);
write_cmos_sensor(0x6F12, 0x0040);
write_cmos_sensor(0x602A, 0x1A94);
write_cmos_sensor(0x6F12, 0x0F50);
write_cmos_sensor(0x602A, 0x0760);
write_cmos_sensor(0x6F12, 0x0100);
write_cmos_sensor(0x602A, 0x0764);
write_cmos_sensor(0x6F12, 0x01F0);
write_cmos_sensor(0x602A, 0x076A);
write_cmos_sensor(0x6F12, 0x01FF);
write_cmos_sensor(0x602A, 0x0770);
write_cmos_sensor(0x6F12, 0x0080);
write_cmos_sensor(0x602A, 0x0772);
write_cmos_sensor(0x6F12, 0x00A0);
write_cmos_sensor(0x602A, 0x0774);
write_cmos_sensor(0x6F12, 0xF43C);
write_cmos_sensor(0x602A, 0x1EA8);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x30BC, 0x0001);
write_cmos_sensor(0x30C8, 0x0100);
write_cmos_sensor(0x0B0E, 0x0100);
write_cmos_sensor(0x0340, 0x0DE6);
write_cmos_sensor(0x0342, 0x1B54);
write_cmos_sensor(0x0344, 0x0018);
write_cmos_sensor(0x0346, 0x0010);
write_cmos_sensor(0x0348, 0x1217);
write_cmos_sensor(0x034A, 0x0D8F);
write_cmos_sensor(0x034C, 0x1200);
write_cmos_sensor(0x034E, 0x0D80);
write_cmos_sensor(0x0900, 0x0011);
write_cmos_sensor(0x0380, 0x0001);
write_cmos_sensor(0x0382, 0x0001);
write_cmos_sensor(0x0384, 0x0001);
write_cmos_sensor(0x0386, 0x0001);
write_cmos_sensor(0x0400, 0x0001);
write_cmos_sensor(0x0404, 0x0010);
write_cmos_sensor(0x0136, 0x1800);
write_cmos_sensor(0x0300, 0x0003);
write_cmos_sensor(0x0302, 0x0001);
write_cmos_sensor(0x0304, 0x0008);
write_cmos_sensor(0x0306, 0x0090);
write_cmos_sensor(0x0308, 0x0008);
write_cmos_sensor(0x030A, 0x0001);
write_cmos_sensor(0x030C, 0x0004);
write_cmos_sensor(0x030E, 0x00A6);
write_cmos_sensor(0x300A, 0x0001);
write_cmos_sensor(0x3004, 0x0006);
write_cmos_sensor(0x0202, 0x0E10);
write_cmos_sensor(0x0204, 0x0020);
write_cmos_sensor(0x6028, 0x2000);
write_cmos_sensor(0x602A, 0x184E);
write_cmos_sensor(0x6F12, 0x0100);
write_cmos_sensor(0x602A, 0x1F72);
write_cmos_sensor(0x6F12, 0x0F80);
write_cmos_sensor(0x0114, 0x0301);
write_cmos_sensor(0x6028, 0x2000);
write_cmos_sensor(0x602A, 0x0086);
write_cmos_sensor(0x6F12, 0x0101);
write_cmos_sensor(0x602A, 0x0088);
write_cmos_sensor(0x6F12, 0x0100);
    } else{ //15fps
  write_cmos_sensor(0x3874, 0x0000);
write_cmos_sensor(0x3876, 0x0102);
write_cmos_sensor(0x3190, 0x0008);
write_cmos_sensor(0x389A, 0x0011);
write_cmos_sensor(0xF406, 0xFFF7);
write_cmos_sensor(0x389C, 0x000B);
write_cmos_sensor(0xF438, 0x000F);
write_cmos_sensor(0xF46C, 0x000E);
write_cmos_sensor(0xF43C, 0x0080);
write_cmos_sensor(0xF496, 0x0010);
write_cmos_sensor(0x38B4, 0x0050);
write_cmos_sensor(0x31B2, 0x0200);
write_cmos_sensor(0x31D4, 0x0001);
write_cmos_sensor(0x31D6, 0x0103);
write_cmos_sensor(0xF48A, 0x0060);
write_cmos_sensor(0xF48E, 0x003F);
write_cmos_sensor(0xF490, 0x0035);
write_cmos_sensor(0x31B8, 0x0038);
write_cmos_sensor(0x3220, 0x0004);
write_cmos_sensor(0x3228, 0x0002);
write_cmos_sensor(0x316C, 0xFFFF);
write_cmos_sensor(0xF47C, 0x000C);
write_cmos_sensor(0xF4BA, 0x0005);
write_cmos_sensor(0x388C, 0x0507);
write_cmos_sensor(0x3888, 0x0F17);
write_cmos_sensor(0x388A, 0x0F17);
write_cmos_sensor(0x3786, 0x0257);
write_cmos_sensor(0x324A, 0x0095);
write_cmos_sensor(0x3222, 0x0004);
write_cmos_sensor(0x322A, 0x0002);
write_cmos_sensor(0x6028, 0x2000);
write_cmos_sensor(0x602A, 0x1A12);
write_cmos_sensor(0x6F12, 0x0002);
write_cmos_sensor(0x602A, 0x1A14);
write_cmos_sensor(0x6F12, 0x0100);
write_cmos_sensor(0x602A, 0x1A16);
write_cmos_sensor(0x6F12, 0x0040);
write_cmos_sensor(0x602A, 0x1A94);
write_cmos_sensor(0x6F12, 0x0F50);
write_cmos_sensor(0x602A, 0x0760);
write_cmos_sensor(0x6F12, 0x0100);
write_cmos_sensor(0x602A, 0x0764);
write_cmos_sensor(0x6F12, 0x01F0);
write_cmos_sensor(0x602A, 0x076A);
write_cmos_sensor(0x6F12, 0x01FF);
write_cmos_sensor(0x602A, 0x0770);
write_cmos_sensor(0x6F12, 0x0080);
write_cmos_sensor(0x602A, 0x0772);
write_cmos_sensor(0x6F12, 0x00A0);
write_cmos_sensor(0x602A, 0x0774);
write_cmos_sensor(0x6F12, 0xF43C);
write_cmos_sensor(0x602A, 0x1EA8);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x30BC, 0x0001);
write_cmos_sensor(0x30C8, 0x0100);
write_cmos_sensor(0x0B0E, 0x0100);
write_cmos_sensor(0x0340, 0x0E1D);
write_cmos_sensor(0x0342, 0x2980);
write_cmos_sensor(0x0344, 0x0018);
write_cmos_sensor(0x0346, 0x0010);
write_cmos_sensor(0x0348, 0x1217);
write_cmos_sensor(0x034A, 0x0D8F);
write_cmos_sensor(0x034C, 0x1200);
write_cmos_sensor(0x034E, 0x0D80);
write_cmos_sensor(0x0900, 0x0011);
write_cmos_sensor(0x0380, 0x0001);
write_cmos_sensor(0x0382, 0x0001);
write_cmos_sensor(0x0384, 0x0001);
write_cmos_sensor(0x0386, 0x0001);
write_cmos_sensor(0x0400, 0x0001);
write_cmos_sensor(0x0404, 0x0010);
write_cmos_sensor(0x0136, 0x1800);
write_cmos_sensor(0x0300, 0x0003);
write_cmos_sensor(0x0302, 0x0001);
write_cmos_sensor(0x0304, 0x0008);
write_cmos_sensor(0x0306, 0x0090);
write_cmos_sensor(0x0308, 0x0008);
write_cmos_sensor(0x030A, 0x0001);
write_cmos_sensor(0x030C, 0x0004);
write_cmos_sensor(0x030E, 0x007C);
write_cmos_sensor(0x300A, 0x0001);
write_cmos_sensor(0x3004, 0x0006);
write_cmos_sensor(0x0202, 0x0E10);
write_cmos_sensor(0x0204, 0x0020);
write_cmos_sensor(0x6028, 0x2000);
write_cmos_sensor(0x602A, 0x184E);
write_cmos_sensor(0x6F12, 0x0100);
write_cmos_sensor(0x602A, 0x1F72);
write_cmos_sensor(0x6F12, 0x0F80);
write_cmos_sensor(0x0114, 0x0301);
write_cmos_sensor(0x6028, 0x2000);
write_cmos_sensor(0x602A, 0x0086);
write_cmos_sensor(0x6F12, 0x0101);
write_cmos_sensor(0x602A, 0x0088);
write_cmos_sensor(0x6F12, 0x0100);
    }
	//write_cmos_sensor(0x6214, 0x7970);
	//write_cmos_sensor_8(0x0100, 0x01);
}

static void normal_video_setting(kal_uint16 currefps)
{
	pr_debug("normal_video_setting() E! currefps:%d\n", currefps);
    write_cmos_sensor_8(0x0100, 0x00);
    check_output_stream_off();
 write_cmos_sensor(0x3874, 0x0000);
write_cmos_sensor(0x3876, 0x0102);
write_cmos_sensor(0x3190, 0x0008);
write_cmos_sensor(0x389A, 0x0011);
write_cmos_sensor(0xF406, 0xFFF7);
write_cmos_sensor(0x389C, 0x000B);
write_cmos_sensor(0xF438, 0x000F);
write_cmos_sensor(0xF46C, 0x000E);
write_cmos_sensor(0xF43C, 0x0080);
write_cmos_sensor(0xF496, 0x0010);
write_cmos_sensor(0x38B4, 0x0050);
write_cmos_sensor(0x31B2, 0x0200);
write_cmos_sensor(0x31D4, 0x0001);
write_cmos_sensor(0x31D6, 0x0103);
write_cmos_sensor(0xF48A, 0x0060);
write_cmos_sensor(0xF48E, 0x003F);
write_cmos_sensor(0xF490, 0x0035);
write_cmos_sensor(0x31B8, 0x0038);
write_cmos_sensor(0x3220, 0x0004);
write_cmos_sensor(0x3228, 0x0002);
write_cmos_sensor(0x316C, 0xFFFF);
write_cmos_sensor(0xF47C, 0x000C);
write_cmos_sensor(0xF4BA, 0x0005);
write_cmos_sensor(0x388C, 0x0507);
write_cmos_sensor(0x3888, 0x0F17);
write_cmos_sensor(0x388A, 0x0F17);
write_cmos_sensor(0x3786, 0x0257);
write_cmos_sensor(0x324A, 0x0095);
write_cmos_sensor(0x3222, 0x0004);
write_cmos_sensor(0x322A, 0x0002);
write_cmos_sensor(0x6028, 0x2000);
write_cmos_sensor(0x602A, 0x1A12);
write_cmos_sensor(0x6F12, 0x0002);
write_cmos_sensor(0x602A, 0x1A14);
write_cmos_sensor(0x6F12, 0x0100);
write_cmos_sensor(0x602A, 0x1A16);
write_cmos_sensor(0x6F12, 0x0040);
write_cmos_sensor(0x602A, 0x1A94);
write_cmos_sensor(0x6F12, 0x0F50);
write_cmos_sensor(0x602A, 0x0760);
write_cmos_sensor(0x6F12, 0x0100);
write_cmos_sensor(0x602A, 0x0764);
write_cmos_sensor(0x6F12, 0x01F0);
write_cmos_sensor(0x602A, 0x076A);
write_cmos_sensor(0x6F12, 0x01FF);
write_cmos_sensor(0x602A, 0x0770);
write_cmos_sensor(0x6F12, 0x0080);
write_cmos_sensor(0x602A, 0x0772);
write_cmos_sensor(0x6F12, 0x00A0);
write_cmos_sensor(0x602A, 0x0774);
write_cmos_sensor(0x6F12, 0xF43C);
write_cmos_sensor(0x602A, 0x1EA8);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x30BC, 0x0001);
write_cmos_sensor(0x30C8, 0x0100);
write_cmos_sensor(0x0B0E, 0x0100);
write_cmos_sensor(0x0340, 0x0E1D);
write_cmos_sensor(0x0342, 0x14C0);
write_cmos_sensor(0x0344, 0x0018);
write_cmos_sensor(0x0346, 0x0010);
write_cmos_sensor(0x0348, 0x1217);
write_cmos_sensor(0x034A, 0x0D8F);
write_cmos_sensor(0x034C, 0x1200);
write_cmos_sensor(0x034E, 0x0D80);
write_cmos_sensor(0x0900, 0x0011);
write_cmos_sensor(0x0380, 0x0001);
write_cmos_sensor(0x0382, 0x0001);
write_cmos_sensor(0x0384, 0x0001);
write_cmos_sensor(0x0386, 0x0001);
write_cmos_sensor(0x0400, 0x0001);
write_cmos_sensor(0x0404, 0x0010);
write_cmos_sensor(0x0136, 0x1800);
write_cmos_sensor(0x0300, 0x0003);
write_cmos_sensor(0x0302, 0x0001);
write_cmos_sensor(0x0304, 0x0008);
write_cmos_sensor(0x0306, 0x0090);
write_cmos_sensor(0x0308, 0x0008);
write_cmos_sensor(0x030A, 0x0001);
write_cmos_sensor(0x030C, 0x0004);
write_cmos_sensor(0x030E, 0x007C);
write_cmos_sensor(0x300A, 0x0000);
write_cmos_sensor(0x3004, 0x0006);
write_cmos_sensor(0x0202, 0x0E10);
write_cmos_sensor(0x0204, 0x0020);
write_cmos_sensor(0x6028, 0x2000);
write_cmos_sensor(0x602A, 0x184E);
write_cmos_sensor(0x6F12, 0x0100);
write_cmos_sensor(0x602A, 0x1F72);
write_cmos_sensor(0x6F12, 0x0F80);
write_cmos_sensor(0x0114, 0x0301);
write_cmos_sensor(0x6028, 0x2000);
write_cmos_sensor(0x602A, 0x0086);
write_cmos_sensor(0x6F12, 0x0101);
write_cmos_sensor(0x602A, 0x0088);
write_cmos_sensor(0x6F12, 0x0100);
//write_cmos_sensor(0x0100, 0x0100);

}
//-Bug 524054, wanglei6.wt,MODIFY, 2019.12.23, modify image size and add log to the algorithm

static void hs_video_setting(void)
{
	pr_debug("hs_video_setting() E\n");
	/* 1920 1080 120fps */
    write_cmos_sensor_8(0x0100, 0x00);
    check_output_stream_off();
      write_cmos_sensor(0x3874, 0x0001);
write_cmos_sensor(0x3876, 0x0102);
write_cmos_sensor(0x3190, 0x0004);
write_cmos_sensor(0x389A, 0x0011);
write_cmos_sensor(0xF406, 0xFFF7);
write_cmos_sensor(0x389C, 0x000B);
write_cmos_sensor(0xF438, 0x000F);
write_cmos_sensor(0xF46C, 0x000E);
write_cmos_sensor(0xF43C, 0x0088);
write_cmos_sensor(0xF496, 0x0010);
write_cmos_sensor(0x38B4, 0x0050);
write_cmos_sensor(0x31B2, 0x0200);
write_cmos_sensor(0x31D4, 0x0001);
write_cmos_sensor(0x31D6, 0x0103);
write_cmos_sensor(0xF48A, 0x0060);
write_cmos_sensor(0xF48E, 0x003F);
write_cmos_sensor(0xF490, 0x0035);
write_cmos_sensor(0x31B8, 0x0040);
write_cmos_sensor(0x3220, 0x0004);
write_cmos_sensor(0x3228, 0x0002);
write_cmos_sensor(0x316C, 0xFFFF);
write_cmos_sensor(0xF47C, 0x0000);
write_cmos_sensor(0xF4BA, 0x0007);
write_cmos_sensor(0x388C, 0x0507);
write_cmos_sensor(0x3888, 0x0F17);
write_cmos_sensor(0x388A, 0x0F17);
write_cmos_sensor(0x3786, 0x0223);
write_cmos_sensor(0x324A, 0x0095);
write_cmos_sensor(0x3222, 0x0004);
write_cmos_sensor(0x322A, 0x0002);
write_cmos_sensor(0x6028, 0x2000);
write_cmos_sensor(0x602A, 0x1A12);
write_cmos_sensor(0x6F12, 0x0002);
write_cmos_sensor(0x602A, 0x1A14);
write_cmos_sensor(0x6F12, 0x0100);
write_cmos_sensor(0x602A, 0x1A16);
write_cmos_sensor(0x6F12, 0x0040);
write_cmos_sensor(0x602A, 0x1A94);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x602A, 0x0760);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x602A, 0x0764);
write_cmos_sensor(0x6F12, 0x0200);
write_cmos_sensor(0x602A, 0x076A);
write_cmos_sensor(0x6F12, 0x0200);
write_cmos_sensor(0x602A, 0x0770);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x602A, 0x0772);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x602A, 0x0774);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x602A, 0x1EA8);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x30BC, 0x0000);
write_cmos_sensor(0x30C8, 0x0100);
write_cmos_sensor(0x0B0E, 0x0100);
write_cmos_sensor(0x0340, 0x0580);
write_cmos_sensor(0x0342, 0x0D50);
write_cmos_sensor(0x0344, 0x0198);
write_cmos_sensor(0x0346, 0x0298);
write_cmos_sensor(0x0348, 0x1097);
write_cmos_sensor(0x034A, 0x0B07);
write_cmos_sensor(0x034C, 0x0780);
write_cmos_sensor(0x034E, 0x0438);
write_cmos_sensor(0x0900, 0x0122);
write_cmos_sensor(0x0380, 0x0001);
write_cmos_sensor(0x0382, 0x0003);
write_cmos_sensor(0x0384, 0x0001);
write_cmos_sensor(0x0386, 0x0003);
write_cmos_sensor(0x0400, 0x0001);
write_cmos_sensor(0x0404, 0x0010);
write_cmos_sensor(0x0136, 0x1800);
write_cmos_sensor(0x0300, 0x0005);
write_cmos_sensor(0x0302, 0x0001);
write_cmos_sensor(0x0304, 0x0006);
write_cmos_sensor(0x0306, 0x00B4);
write_cmos_sensor(0x0308, 0x0008);
write_cmos_sensor(0x030A, 0x0001);
write_cmos_sensor(0x030C, 0x0004);
write_cmos_sensor(0x030E, 0x007C);
write_cmos_sensor(0x300A, 0x0000);
write_cmos_sensor(0x3004, 0x000A);
write_cmos_sensor(0x0202, 0x0570);
write_cmos_sensor(0x0204, 0x0020);
write_cmos_sensor(0x6028, 0x2000);
write_cmos_sensor(0x602A, 0x184E);
write_cmos_sensor(0x6F12, 0x0100);
write_cmos_sensor(0x602A, 0x1F72);
write_cmos_sensor(0x6F12, 0x0F80);
write_cmos_sensor(0x0114, 0x0301);
write_cmos_sensor(0x6028, 0x2000);
write_cmos_sensor(0x602A, 0x0086);
write_cmos_sensor(0x6F12, 0x0100);
write_cmos_sensor(0x602A, 0x0088);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x0100, 0x0100);

}

static void slim_video_setting(void)
{
	pr_debug("slim_video_setting() E\n");
    /* 1080p 30fps */
write_cmos_sensor_8(0x0100, 0x00);
    check_output_stream_off();
write_cmos_sensor(0x3874, 0x0000);
write_cmos_sensor(0x3876, 0x0102);
write_cmos_sensor(0x3190, 0x0004);
write_cmos_sensor(0x389A, 0x0011);
write_cmos_sensor(0xF406, 0xFFF7);
write_cmos_sensor(0x389C, 0x000B);
write_cmos_sensor(0xF438, 0x000F);
write_cmos_sensor(0xF46C, 0x000E);
write_cmos_sensor(0xF43C, 0x0088);
write_cmos_sensor(0xF496, 0x0010);
write_cmos_sensor(0x38B4, 0x0050);
write_cmos_sensor(0x31B2, 0x0200);
write_cmos_sensor(0x31D4, 0x0001);
write_cmos_sensor(0x31D6, 0x0103);
write_cmos_sensor(0xF48A, 0x0060);
write_cmos_sensor(0xF48E, 0x003F);
write_cmos_sensor(0xF490, 0x0035);
write_cmos_sensor(0x31B8, 0x0040);
write_cmos_sensor(0x3220, 0x0003);
write_cmos_sensor(0x3228, 0x0006);
write_cmos_sensor(0x316C, 0x0071);
write_cmos_sensor(0xF47C, 0x000C);
write_cmos_sensor(0xF4BA, 0x0005);
write_cmos_sensor(0x388C, 0x0507);
write_cmos_sensor(0x3888, 0x0F17);
write_cmos_sensor(0x388A, 0x0F17);
write_cmos_sensor(0x3786, 0x0257);
write_cmos_sensor(0x324A, 0x0095);
write_cmos_sensor(0x3222, 0x0003);
write_cmos_sensor(0x322A, 0x0006);
write_cmos_sensor(0x6028, 0x2000);
write_cmos_sensor(0x602A, 0x1A94);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x602A, 0x0760);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x602A, 0x0764);
write_cmos_sensor(0x6F12, 0x0200);
write_cmos_sensor(0x602A, 0x076A);
write_cmos_sensor(0x6F12, 0x0200);
write_cmos_sensor(0x602A, 0x0770);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x602A, 0x0772);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x602A, 0x0774);
write_cmos_sensor(0x6F12, 0x0000);
write_cmos_sensor(0x602A, 0x1EA8);
write_cmos_sensor(0x6F12, 0x0001);
write_cmos_sensor(0x30BC, 0x0000);
write_cmos_sensor(0x30C8, 0x0100);
write_cmos_sensor(0x0B0E, 0x0000);
write_cmos_sensor(0x0340, 0x0E1D);
write_cmos_sensor(0x0342, 0x14C0);
write_cmos_sensor(0x0344, 0x04E0);
write_cmos_sensor(0x0346, 0x0400);
write_cmos_sensor(0x0348, 0x0D4F);
write_cmos_sensor(0x034A, 0x099F);
write_cmos_sensor(0x034C, 0x0438);
write_cmos_sensor(0x034E, 0x02D0);
write_cmos_sensor(0x0900, 0x0112);
write_cmos_sensor(0x0380, 0x0001);
write_cmos_sensor(0x0382, 0x0001);
write_cmos_sensor(0x0384, 0x0001);
write_cmos_sensor(0x0386, 0x0003);
write_cmos_sensor(0x0400, 0x0001);
write_cmos_sensor(0x0404, 0x0020);
write_cmos_sensor(0x0136, 0x1800);
write_cmos_sensor(0x0300, 0x0003);
write_cmos_sensor(0x0302, 0x0001);
write_cmos_sensor(0x0304, 0x0008);
write_cmos_sensor(0x0306, 0x0090);
write_cmos_sensor(0x0308, 0x0008);
write_cmos_sensor(0x030A, 0x0001);
write_cmos_sensor(0x030C, 0x0004);
write_cmos_sensor(0x030E, 0x007C);
write_cmos_sensor(0x300A, 0x0001);
write_cmos_sensor(0x3004, 0x0006);
write_cmos_sensor(0x0202, 0x017E);
write_cmos_sensor(0x0204, 0x0020);
write_cmos_sensor(0x6028, 0x2000);
write_cmos_sensor(0x602A, 0x184E);
write_cmos_sensor(0x6F12, 0x0100);
write_cmos_sensor(0x602A, 0x1F72);
write_cmos_sensor(0x6F12, 0x0F80);
write_cmos_sensor(0x0114, 0x0301);
write_cmos_sensor(0x0100, 0x0100);
}
//+Bug 492443, sunhushan.wt, ADD, 2019.10.15, porting the otp of the 12sensor 
#include "cam_cal_define.h"
#include <linux/slab.h>
static struct stCAM_CAL_DATAINFO_STRUCT dongci_main_s5k2p6_eeprom_data ={
	.sensorID= N21_DONGCI_MAIN_S5K2P6_SENSOR_ID,
	.deviceID = 0x01,
	.dataLength = 0x0D5D,
	.sensorVendorid = 0x13000101,
	.vendorByte = {1,2,3,4},
	.dataBuffer = NULL,
};
static struct stCAM_CAL_CHECKSUM_STRUCT dongci_main_s5k2p6_Checksum[8] =
{
	{MODULE_ITEM,0x0000,0x0000,0x0007,0x0008,0x55},
	{AWB_ITEM,0x0009,0x0009,0x0019,0x001A,0x55},
	{AF_ITEM,0x001B,0x001B,0x0020,0x0021,0x55},
	{LSC_ITEM,0x0022,0x0022,0x076E,0x076F,0x55},
	{PDAF_ITEM,0x0770,0x0770,0x0CF6,0x0CF7,0x55},
	{TOTAL_ITEM,0x0000,0x0000,0x0CF7,0x0CF8,0x55},
	{MAX_ITEM,0xFFFF,0xFFFF,0xFFFF,0xFFFF,0x55},  // this line must haved
};
extern int imgSensorReadEepromData(struct stCAM_CAL_DATAINFO_STRUCT* pData,
	struct stCAM_CAL_CHECKSUM_STRUCT* checkData);
extern int imgSensorSetEepromData(struct stCAM_CAL_DATAINFO_STRUCT* pData);
//-Bug 492443, sunhushan.wt, ADD, 2019.10.15, porting the otp of the 12sensor 
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
	kal_int32 size = 0; //Bug 492443, sunhushan.wt, ADD, 2019.10.15, porting the otp of the 12sensor 

	/* sensor have two i2c address 0x6c 0x6d & 0x21 0x20,
	 * we should detect the module used i2c address
	 */

	while (imgsensor_info.i2c_addr_table[i] != 0xff) {
		spin_lock(&imgsensor_drv_lock);
		imgsensor.i2c_write_id = imgsensor_info.i2c_addr_table[i];
		spin_unlock(&imgsensor_drv_lock);
		do {
			*sensor_id = (
	      (read_cmos_sensor_8(0x0000) << 8) | read_cmos_sensor_8(0x0001)) - 1;

			if (*sensor_id == imgsensor_info.sensor_id) {
				//+Bug 492443, sunhushan.wt, ADD, 2019.10.15, porting the otp of the 12sensor 
				size = imgSensorReadEepromData(&dongci_main_s5k2p6_eeprom_data,dongci_main_s5k2p6_Checksum);
				if(size != dongci_main_s5k2p6_eeprom_data.dataLength || (dongci_main_s5k2p6_eeprom_data.sensorVendorid >> 24)!= read_eeprom(0x0001)) {
					printk("get eeprom data failed\n");
					if(dongci_main_s5k2p6_eeprom_data.dataBuffer != NULL) {
						kfree(dongci_main_s5k2p6_eeprom_data.dataBuffer);
						dongci_main_s5k2p6_eeprom_data.dataBuffer = NULL;
					}
					*sensor_id = 0xFFFFFFFF;
					return ERROR_SENSOR_CONNECT_FAIL;
				} else {
					pr_debug("get eeprom data success\n");
					imgSensorSetEepromData(&dongci_main_s5k2p6_eeprom_data);
				}
				//-Bug 492443, sunhushan.wt, ADD, 2019.10.15, porting the otp of the 12sensor 
				pr_debug("i2c write id: 0x%x, sensor id: 0x%x\n",
					imgsensor.i2c_write_id, *sensor_id);

				return ERROR_NONE;
			}
			pr_debug("Read sensor id fail, id: 0x%x\n",
				imgsensor.i2c_write_id);
			retry--;
		} while (retry > 0);
		i++;
		retry = 2;
	}
	if (*sensor_id != imgsensor_info.sensor_id) {
	/* if Sensor ID is not correct, Must set *sensor_id to 0xFFFFFFFF*/
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

	pr_debug(
	  "PLATFORM:VINSON,MIPI 4LANE  preview 1280*960@30fps,864Mbps/lane\n");
	pr_debug(
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
	      (read_cmos_sensor_8(0x0000) << 8) | read_cmos_sensor_8(0x0001))  - 1;

			if (sensor_id == imgsensor_info.sensor_id) {

				pr_debug("i2c write id: 0x%x, sensor id: 0x%x\n",
					imgsensor.i2c_write_id, sensor_id);
				break;
			}
			pr_debug("Read sensor id fail, id: 0x%x\n",
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
	pr_debug("E\n");

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
	pr_debug("E\n");

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
	pr_debug("E\n");
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
			pr_debug(
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
	pr_debug("E\n");

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
	pr_debug("E\n");

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
	pr_debug("E\n");

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



static kal_uint32 get_resolution(
	MSDK_SENSOR_RESOLUTION_INFO_STRUCT(*sensor_resolution))
{
	pr_debug("E\n");
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

	return ERROR_NONE;
}	/*	get_resolution	*/

static kal_uint32 get_info(enum MSDK_SCENARIO_ID_ENUM scenario_id,
				  MSDK_SENSOR_INFO_STRUCT *sensor_info,
				  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	pr_debug("scenario_id = %d\n", scenario_id);

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

	sensor_info->SensorMasterClockSwitch = 0; /* not use */
	sensor_info->SensorDrivingCurrent = imgsensor_info.isp_driving_current;

	sensor_info->AEShutDelayFrame = imgsensor_info.ae_shut_delay_frame;

	sensor_info->AESensorGainDelayFrame =
		imgsensor_info.ae_sensor_gain_delay_frame;
       sensor_info->FrameTimeDelayFrame = imgsensor_info.frame_time_delay_frame;
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
}	/* get_info  */


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
			pr_debug("Error ScenarioId setting");
			preview(image_window, sensor_config_data);
			return ERROR_INVALID_SCENARIO_ID;
	}
	return ERROR_NONE;
}	/* control() */



static kal_uint32 set_video_mode(UINT16 framerate)
{
	pr_debug("framerate = %d\n ", framerate);
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
	pr_debug("enable = %d, framerate = %d\n", enable, framerate);
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

	pr_debug("scenario_id = %d, framerate = %d\n", scenario_id, framerate);

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
				pr_debug(
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
		pr_debug(
		    "error scenario_id = %d, we use preview scenario\n",
			scenario_id);
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
	default:
			break;
	}

	return ERROR_NONE;
}

static kal_uint32 set_test_pattern_mode(kal_bool enable)
{
	pr_debug("enable: %d\n", enable);

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

	pr_debug("feature_id = %d\n", feature_id);
	switch (feature_id) {
	case SENSOR_FEATURE_GET_PERIOD:
			*feature_return_para_16++ = imgsensor.line_length;
			*feature_return_para_16 = imgsensor.frame_length;
			*feature_para_len = 8;
			break;
	case SENSOR_FEATURE_GET_PIXEL_CLOCK_FREQ:
		pr_debug("imgsensor.pclk = %d,current_fps = %d\n",
			imgsensor.pclk, imgsensor.current_fps);
			*feature_return_para_32 = imgsensor.pclk;
			*feature_para_len = 8;
			break;
	case SENSOR_FEATURE_SET_ESHUTTER:
		set_shutter(*feature_data);
			break;
	case SENSOR_FEATURE_SET_NIGHTMODE:
	    /*night_mode((BOOL) *feature_data); no need to implement this mode*/
			break;
	case SENSOR_FEATURE_SET_GAIN:
		set_gain((UINT16) *feature_data);
			break;
	case SENSOR_FEATURE_SET_FLASHLIGHT:
			break;
	case SENSOR_FEATURE_SET_ISP_MASTER_CLOCK_FREQ:
			break;
	case SENSOR_FEATURE_SET_REGISTER:
			write_cmos_sensor(
			    sensor_reg_data->RegAddr, sensor_reg_data->RegData);
			break;
	case SENSOR_FEATURE_GET_REGISTER:
			sensor_reg_data->RegData =
				read_cmos_sensor(sensor_reg_data->RegAddr);
			break;
	case SENSOR_FEATURE_GET_LENS_DRIVER_ID:
		/* get the lens driver ID from EEPROM or
		 * just return LENS_DRIVER_ID_DO_NOT_CARE
		 */

		/* if EEPROM does not exist in camera module.*/
		*feature_return_para_32 = LENS_DRIVER_ID_DO_NOT_CARE;
		*feature_para_len = 8;
		break;
	case SENSOR_FEATURE_SET_VIDEO_MODE:
		set_video_mode(*feature_data);
		break;
	case SENSOR_FEATURE_CHECK_SENSOR_ID:
			get_imgsensor_id(feature_return_para_32);
			break;
	case SENSOR_FEATURE_SET_AUTO_FLICKER_MODE:
		set_auto_flicker_mode(
			(BOOL)*feature_data_16, *(feature_data_16+1));
		break;
	case SENSOR_FEATURE_SET_MAX_FRAME_RATE_BY_SCENARIO:
		set_max_framerate_by_scenario(
		  (enum MSDK_SCENARIO_ID_ENUM)*feature_data, *(feature_data+1));
		break;
	case SENSOR_FEATURE_GET_DEFAULT_FRAME_RATE_BY_SCENARIO:
		get_default_framerate_by_scenario(
			(enum MSDK_SCENARIO_ID_ENUM)*(feature_data),
			(MUINT32 *)(uintptr_t)(*(feature_data+1)));
		break;
	case SENSOR_FEATURE_SET_TEST_PATTERN:
		set_test_pattern_mode((BOOL)*feature_data);
		break;

		/*for factory mode auto testing*/
	case SENSOR_FEATURE_GET_TEST_PATTERN_CHECKSUM_VALUE:
			*feature_return_para_32 = imgsensor_info.checksum_value;
			*feature_para_len = 8;
			break;

	case SENSOR_FEATURE_SET_FRAMERATE:
		pr_debug("current fps :%d\n", *feature_data_32);
		spin_lock(&imgsensor_drv_lock);
		imgsensor.current_fps = (UINT16)*feature_data_32;
		spin_unlock(&imgsensor_drv_lock);
		break;

	case SENSOR_FEATURE_SET_HDR:
		pr_debug("hdr enable :%d\n", *feature_data_32);
		spin_lock(&imgsensor_drv_lock);
		imgsensor.ihdr_mode = (UINT8)*feature_data_32;
		spin_unlock(&imgsensor_drv_lock);
		break;

	case SENSOR_FEATURE_GET_CROP_INFO:
		pr_debug("SENSOR_FEATURE_GET_CROP_INFO scenarioId:%d\n",
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
		pr_debug("SENSOR_SET_SENSOR_IHDR LE=%d, SE=%d, Gain=%d\n",
			(UINT16)*feature_data,
			(UINT16)*(feature_data+1), (UINT16)*(feature_data+2));

		/* ihdr_write_shutter_gain((UINT16)*feature_data,
		 * (UINT16)*(feature_data+1),(UINT16)*(feature_data+2));
		 */
		break;
	case SENSOR_FEATURE_SET_AWB_GAIN:
		break;
	case SENSOR_FEATURE_SET_HDR_SHUTTER:
		pr_debug("SENSOR_FEATURE_SET_HDR_SHUTTER LE=%d, SE=%d\n",
			(UINT16)*feature_data, (UINT16)*(feature_data+1));
		/* ihdr_write_shutter(
		 * (UINT16)*feature_data,(UINT16)*(feature_data+1));
		 */
		break;
		/******************** PDAF START >>> *********/
	case SENSOR_FEATURE_GET_PDAF_INFO:
		pr_debug("SENSOR_FEATURE_GET_PDAF_INFO scenarioId:%d\n",
			(UINT16)*feature_data);
		PDAFinfo =
		  (struct SET_PD_BLOCK_INFO_T *)(uintptr_t)(*(feature_data+1));
		switch (*feature_data) {
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG: /*full*/
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW: /*2x2 binning*/
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
		pr_debug("SENSOR_FEATURE_GET_VC_INFO %d\n",
			(UINT16)*feature_data);
		pvcinfo =
		 (struct SENSOR_VC_INFO_STRUCT *)(uintptr_t)(*(feature_data+1));
		switch (*feature_data_32) {
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			memcpy(
				(void *)pvcinfo,
				(void *)&SENSOR_VC_INFO[2],
				sizeof(struct SENSOR_VC_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			memcpy(
				(void *)pvcinfo,
				(void *)&SENSOR_VC_INFO[1],
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
		pr_debug(
		  "SENSOR_FEATURE_GET_SENSOR_PDAF_CAPACITY scenarioId:%d\n",
			(UINT16)*feature_data);
		/*PDAF capacity enable or not*/
		switch (*feature_data) {
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			*(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 1;
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
		default:
			*(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 0;
			break;
		}
		break;
	case SENSOR_FEATURE_SET_PDAF:
			pr_debug("PDAF mode :%d\n", *feature_data_16);
			imgsensor.pdaf_mode = *feature_data_16;
			break;
	case SENSOR_FEATURE_SET_SHUTTER_FRAME_TIME:/*lzl*/
			set_shutter_frame_length((UINT16)*feature_data,
					(UINT16)*(feature_data+1));
			break;
	/******************** STREAMING RESUME/SUSPEND *********/
	case SENSOR_FEATURE_SET_STREAMING_SUSPEND:
		pr_debug("SENSOR_FEATURE_SET_STREAMING_SUSPEND\n");
		streaming_control(KAL_FALSE);
		break;
	case SENSOR_FEATURE_SET_STREAMING_RESUME:
		pr_debug("SENSOR_FEATURE_SET_STREAMING_RESUME, shutter:%llu\n",
			*feature_data);
		if (*feature_data != 0)
			set_shutter(*feature_data);
		streaming_control(KAL_TRUE);
		break;
	case SENSOR_FEATURE_GET_PIXEL_RATE:
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

UINT32 N21_DONGCI_MAIN_S5K2P6_MIPI_RAW_SensorInit(struct SENSOR_FUNCTION_STRUCT **pfFunc)
{
	/* To Do : Check Sensor status here */
	if (pfFunc != NULL)
		*pfFunc = &sensor_func;
	return ERROR_NONE;
}
