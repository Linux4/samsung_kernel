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
 *	 IMX616mipi_Sensor.c
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

#include "imx616mipiraw_Sensor.h"
#include "imx616_eeprom.h"
//#include "imgsensor_common.h"

/***************Modify Following Strings for Debug**********************/
#define PFX "IMX616_camera_sensor"
#define LOG_1 LOG_INF("IMX616,MIPI 4LANE\n")
/****************************   Modify end	**************************/

#define LOG_INF(format, args...) pr_debug(PFX "[%s] " format, __func__, ##args)

#undef VENDOR_EDIT
#ifdef VENDOR_EDIT
/*Caohua.Lin@Camera.Driver add for 18011/18311  board 20180723*/
#define DEVICE_VERSION_IMX616     "imx616"
extern void register_imgsensor_deviceinfo(char *name, char *version, u8 module_id);
static kal_uint8 deviceInfo_register_value = 0x00;
static kal_uint32 streaming_control(kal_bool enable);
#define MODULE_ID_OFFSET 0x0000
static kal_uint8 qsc_flag = 0;
#endif

static kal_uint16 imx616_table_write_cmos_sensor(kal_uint16 *para, kal_uint32 len);

static DEFINE_SPINLOCK(imgsensor_drv_lock);


static struct imgsensor_info_struct imgsensor_info = {
	.sensor_id = IMX616_SENSOR_ID,
	.checksum_value = 0x8ac2d94a,

	.pre = { /* reg_B-1 3280x2464 @30fps*/
		.pclk = 863200000,
		.linelength = 3768,
		.framelength = 7636,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 2448,
		.grabwindow_height = 2448,
		.mipi_data_lp2hs_settle_dc = 85,
		/* following for GetDefaultFramerateByScenario() */
		.mipi_pixel_rate = 821600000,
		.max_framerate = 300, /* 30fps */
	},

	.cap = { /*reg_A-1 32M@30fps*/
		.pclk = 847600000,
		.linelength = 3768,
		.framelength = 7498,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 3264,
		.grabwindow_height = 2448,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 821600000,
		.max_framerate = 300,
	},

	.normal_video = { /* reg_B-1 3280x2464 @30fps*/
		.pclk = 847600000,
		.linelength = 3768,
		.framelength = 7498,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 3264,
		.grabwindow_height = 1836,
		.mipi_data_lp2hs_settle_dc = 85,
		/* following for GetDefaultFramerateByScenario() */
		.mipi_pixel_rate = 821600000,
		.max_framerate = 300, /* 30fps */
	},


	.hs_video = { /* reg_B-1 3280x2464 @30fps*/
		.pclk = 863200000,
		.linelength = 2248,
		.framelength = 3198,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 1632,
		.grabwindow_height = 1224,
		.mipi_data_lp2hs_settle_dc = 85,
		/* following for GetDefaultFramerateByScenario() */
		.mipi_pixel_rate = 821600000,
		.max_framerate = 1200, /* 30fps */
	},


	.slim_video = { /* reg_B-1 3280x2464 @30fps*/
		.pclk = 289200000,
		.linelength = 3768,
		.framelength = 2558,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 3280,
		.grabwindow_height = 2464,
		.mipi_data_lp2hs_settle_dc = 85,
		/* following for GetDefaultFramerateByScenario() */
		.mipi_pixel_rate = 273600000,
		.max_framerate = 300, /* 30fps */
	},


	.custom1 = { /*reg_B-2 3280x1856 (16:9) @30FPS */
		.pclk = 228000000,
		.linelength = 3768,
		.framelength = 2016,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 3280,
		.grabwindow_height = 1856,
		.mipi_data_lp2hs_settle_dc = 85,
		/* following for GetDefaultFramerateByScenario() */
		.mipi_pixel_rate = 210000000,
		.max_framerate = 300, /*30fps */
	},

	.margin = 48,		/* sensor framelength & shutter margin */
	.min_shutter = 16,	/* min shutter */
	.min_gain = 64, /*1x gain*/
	.max_gain = 4096, /*64x gain*/
	.min_gain_iso = 100,
	.gain_step = 16,
	.gain_type = 0,
	.max_frame_length = 0xffff,
	.ae_shut_delay_frame = 0,
	.ae_sensor_gain_delay_frame = 0,
	.ae_ispGain_delay_frame = 2,	/* isp gain delay frame for AE cycle */
	.ihdr_support = 0,	/* 1, support; 0,not support */
	.ihdr_le_firstline = 0,	/* 1,le first ; 0, se first */
	.sensor_mode_num = 10,	/* support sensor mode num */

	.cap_delay_frame = 2,	/* enter capture delay frame num */
	.pre_delay_frame = 2,	/* enter preview delay frame num */
	.video_delay_frame = 2,	/* enter video delay frame num */
	.hs_video_delay_frame = 2,
	.slim_video_delay_frame = 2,	/* enter slim video delay frame num */
	.custom1_delay_frame = 2,	/* enter custom1 delay frame num */
	.custom2_delay_frame = 2,	/* enter custom2 delay frame num */
	.custom3_delay_frame = 2,	/* enter custom3 delay frame num */
	.custom4_delay_frame = 2,	/* enter custom4 delay frame num */
	.custom5_delay_frame = 2,	/* enter custom5 delay frame num */
	.frame_time_delay_frame = 3,

	.isp_driving_current = ISP_DRIVING_8MA,
	.sensor_interface_type = SENSOR_INTERFACE_TYPE_MIPI,
	.mipi_sensor_type = MIPI_OPHY_NCSI2, /*only concern if it's cphy*/
	.mipi_settle_delay_mode = 0,
	.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_4CELL_HW_BAYER_R,
	.mclk = 26, /* mclk value, suggest 24 or 26 for 24Mhz or 26Mhz */
	/*.mipi_lane_num = SENSOR_MIPI_4_LANE,*/
	.mipi_lane_num = SENSOR_MIPI_4_LANE,
	.i2c_addr_table = {0x20,0x34,0xff},
	/* record sensor support all write id addr,
	 * only supprt 4 must end with 0xff
	 */
	.i2c_speed = 400, /* i2c read/write speed */
};

static struct imgsensor_struct imgsensor = {
	.mirror = IMAGE_NORMAL,	/* NORMAL information */
	.sensor_mode = IMGSENSOR_MODE_INIT,
	/* IMGSENSOR_MODE enum value,record current sensor mode,such as:
	 * INIT, Preview, Capture, Video,High Speed Video, Slim Video
	 */
	.shutter = 0x3D0,	/* current shutter */
	.gain = 0x100,		/* current gain */
	.dummy_pixel = 0,	/* current dummypixel */
	.dummy_line = 0,	/* current dummyline */
	.current_fps = 300,
	.autoflicker_en = KAL_FALSE,
	.test_pattern = KAL_FALSE,
	.current_scenario_id = MSDK_SCENARIO_ID_CAMERA_PREVIEW,
	.ihdr_mode = 0, /* sensor need support LE, SE with HDR feature */
	.i2c_write_id = 0x20, /* record current sensor's i2c write id */
};


/* Sensor output window information */
static struct SENSOR_WINSIZE_INFO_STRUCT imgsensor_winsize_info[10] = {
	{6560, 4928,    0,    0, 6560, 4928, 3280, 2464,
	  416,    8, 2448, 2448,    0,    0, 2448, 2448}, /* Preview */
	{6560, 4928,    0,    0, 6560, 4928, 3280, 2464,
	    8,    8, 3264, 2448,    0,    0, 3264, 2448}, /* capture */
	{6560, 4928,    0,    0, 6560, 4928, 3280, 2464,
	    8,  314, 3264, 1836,    0,    0, 3264, 1836}, /* normal video */
	{6560, 4928,    0,   16, 6560, 4928, 3280, 2464,
	    4,    0, 1632, 1224,    0,    0, 1632, 1224}, /* hs_video */
	{6560, 4928,    0,    0, 6560, 4928, 3280, 2464,
	    0,    0, 3280, 2464,    0,    0, 3280, 2464}, /* slim video */
	{6560, 4928,    0,  608, 6560, 3712, 3280, 1856,
	    0,    0, 3280, 1856,    0,    0, 3280, 1856} /* custom1 */
};

#ifdef VENDOR_EDIT
/*Caohua.Lin@Camera.Driver add for 18011/18311  board 20180723*/
static kal_uint16 read_module_id(void)
{
	kal_uint16 get_byte=0;
	char pusendcmd[2] = {(char)(MODULE_ID_OFFSET >> 8) , (char)(MODULE_ID_OFFSET & 0xFF) };
	iReadRegI2C(pusendcmd , 2, (u8*)&get_byte,1,0xA8/*EEPROM_READ_ID*/);
	if (get_byte == 0) {
		iReadRegI2C(pusendcmd, 2, (u8 *)&get_byte, 1, 0xA8/*EEPROM_READ_ID*/);
	}
	return get_byte;

}
#endif

static kal_uint16 read_cmos_sensor(kal_uint32 addr)
{
	kal_uint16 get_byte = 0;
	char pusendcmd[2] = {(char)(addr >> 8), (char)(addr & 0xFF)};

	iReadRegI2C(pusendcmd, 2, (u8 *)&get_byte, 2, imgsensor.i2c_write_id);
	return ((get_byte<<8)&0xff00) | ((get_byte>>8)&0x00ff);
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
	char pusendcmd[3] = {(char)(addr >> 8), (char)(addr & 0xFF),
			(char)(para & 0xFF)};

	iWriteRegI2C(pusendcmd, 3, imgsensor.i2c_write_id);
}

#ifdef VENDOR_EDIT
/*Henry.Chang@Camera.Driver add for 18073 ModuleSN*/
#define  CAMERA_MODULE_INFO_LENGTH  (8)
static kal_uint8 gImx616_SN[CAMERA_MODULE_SN_LENGTH];
static kal_uint8 gImx616_CamInfo[CAMERA_MODULE_INFO_LENGTH];
/*Henry.Chang@Camera.Driver add for google ARCode Feature verify 20190531*/
static void read_eeprom_CamInfo(void)
{
	kal_uint16 idx = 0;
	kal_uint8 get_byte[12];
	for (idx = 0; idx <12; idx++) {
		char pusendcmd[2] = {0x00 , (char)((0x00 + idx) & 0xFF) };
		iReadRegI2C(pusendcmd , 2, (u8*)&get_byte[idx],1, 0xA0);
		LOG_INF("imx616_info[%d]: 0x%x\n", idx, get_byte[idx]);
	}

	gImx616_CamInfo[0] = get_byte[0];
	gImx616_CamInfo[1] = get_byte[1];
	gImx616_CamInfo[2] = get_byte[6];
	gImx616_CamInfo[3] = get_byte[7];
	gImx616_CamInfo[4] = get_byte[8];
	gImx616_CamInfo[5] = get_byte[9];
	gImx616_CamInfo[6] = get_byte[10];
	gImx616_CamInfo[7] = get_byte[11];
}

static void read_eeprom_SN(void)
{
	kal_uint16 idx = 0;
	kal_uint8 *get_byte= &gImx616_SN[0];
	for (idx = 0; idx <CAMERA_MODULE_SN_LENGTH; idx++) {
		char pusendcmd[2] = {0x00 , (char)((0xB0 + idx) & 0xFF) };
		iReadRegI2C(pusendcmd , 2, (u8*)&get_byte[idx],1, 0xA0);
		LOG_INF("imx616_SN[%d]: 0x%x  0x%x\n", idx, get_byte[idx], gImx616_SN[idx]);
	}
}
#endif

static void set_dummy(void)
{
	LOG_INF("dummyline = %d, dummypixels = %d\n",
		imgsensor.dummy_line, imgsensor.dummy_pixel);
	/* return;*/ /* for test */
	write_cmos_sensor_8(0x0104, 0x01);

	write_cmos_sensor_8(0x0340, imgsensor.frame_length >> 8);
	write_cmos_sensor_8(0x0341, imgsensor.frame_length & 0xFF);
	write_cmos_sensor_8(0x0342, imgsensor.line_length >> 8);
	write_cmos_sensor_8(0x0343, imgsensor.line_length & 0xFF);

	write_cmos_sensor_8(0x0104, 0x00);

}	/*	set_dummy  */

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

static void set_max_framerate(UINT16 framerate, kal_bool min_framelength_en)
{
	/*kal_int16 dummy_line;*/
	kal_uint32 frame_length = imgsensor.frame_length;

	LOG_INF("framerate = %d, min framelength should enable %d\n", framerate,
		min_framelength_en);

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

static void write_shutter(kal_uint32 shutter)
{
	kal_uint16 realtime_fps = 0;
	#ifdef VENDOR_EDIT
	/*Yijun.Tan@camera.driver,20180116,add for slow shutter */
	int longexposure_times = 0;
	static int long_exposure_status;
	#endif

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
		realtime_fps = imgsensor.pclk / imgsensor.line_length * 10
				/ imgsensor.frame_length;
		if (realtime_fps >= 297 && realtime_fps <= 305)
			set_max_framerate(296, 0);
		else if (realtime_fps >= 147 && realtime_fps <= 150)
			set_max_framerate(146, 0);
		else {
		/* Extend frame length*/
		#ifndef VENDOR_EDIT
		/*An.Wu@Cam 20190603 apply MTK'patch ALPS04523649 to solve supernight frame length issue*/
		write_cmos_sensor_8(0x0104, 0x01);
		write_cmos_sensor_8(0x0340, imgsensor.frame_length >> 8);
		write_cmos_sensor_8(0x0341, imgsensor.frame_length & 0xFF);
		write_cmos_sensor_8(0x0104, 0x00);
		#endif
		}
	} else {
		#ifndef VENDOR_EDIT
		/*An.Wu@Cam 20190603 apply MTK'patch ALPS04523649 to solve supernight frame length issue*/
		/* Extend frame length*/
		write_cmos_sensor_8(0x0104, 0x01);
		write_cmos_sensor_8(0x0340, imgsensor.frame_length >> 8);
		write_cmos_sensor_8(0x0341, imgsensor.frame_length & 0xFF);
		write_cmos_sensor_8(0x0104, 0x00);

		LOG_INF("(else)imgsensor.frame_length = %d\n",
			imgsensor.frame_length);
		#endif
	}
	#ifdef VENDOR_EDIT
	/*Yijun.Tan@camera.driver,20180116,add for slow shutter */
	while (shutter >= 65535) {
		shutter = shutter / 2;
		longexposure_times += 1;
	}

	if (longexposure_times > 0) {
		LOG_INF("enter long exposure mode, time is %d",
			longexposure_times);
		long_exposure_status = 1;
		imgsensor.frame_length = shutter + 32;
		#ifndef VENDOR_EDIT
		/*An.Wu@Cam 20190603 apply MTK'patch ALPS04523649 to solve supernight frame length issue*/
		write_cmos_sensor_8(0x0104, 0x01);
		#endif
		write_cmos_sensor_8(0x3100, longexposure_times & 0x07);
		#ifndef VENDOR_EDIT
		/*An.Wu@Cam 20190603 apply MTK'patch ALPS04523649 to solve supernight frame length issue*/
		write_cmos_sensor_8(0x0340, imgsensor.frame_length >> 8);
		write_cmos_sensor_8(0x0341, imgsensor.frame_length & 0xFF);
		write_cmos_sensor_8(0x0104, 0x00);
		#endif
	} else if (long_exposure_status == 1) {
		long_exposure_status = 0;
		write_cmos_sensor_8(0x0104, 0x01);
		write_cmos_sensor_8(0x3100, 0x00);
		write_cmos_sensor_8(0x0340, imgsensor.frame_length >> 8);
		write_cmos_sensor_8(0x0341, imgsensor.frame_length & 0xFF);
		write_cmos_sensor_8(0x0104, 0x00);

		LOG_INF("exit long exposure mode");
	}
	#endif
	/* Update Shutter */
	write_cmos_sensor_8(0x0104, 0x01);
	write_cmos_sensor_8(0x0202, (shutter >> 8) & 0xFF);
	write_cmos_sensor_8(0x0203, shutter  & 0xFF);
	write_cmos_sensor_8(0x0104, 0x00);
	LOG_INF("shutter =%d, framelength =%d\n",
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

	spin_lock_irqsave(&imgsensor_drv_lock, flags);
	imgsensor.shutter = shutter;
	spin_unlock_irqrestore(&imgsensor_drv_lock, flags);

	write_shutter(shutter);
} /* set_shutter */


/*************************************************************************
 * FUNCTION
 *	set_shutter_frame_length
 *
 * DESCRIPTION
 *	for frame & 3A sync
 *
 *************************************************************************/
static void set_shutter_frame_length(kal_uint16 shutter,
				     kal_uint16 frame_length)
{
	unsigned long flags;
	kal_uint16 realtime_fps = 0;
	kal_int32 dummy_line = 0;

	spin_lock_irqsave(&imgsensor_drv_lock, flags);
	imgsensor.shutter = shutter;
	spin_unlock_irqrestore(&imgsensor_drv_lock, flags);

	/*0x3500, 0x3501, 0x3502 will increase VBLANK to
	 *get exposure larger than frame exposure
	 *AE doesn't update sensor gain at capture mode,
	 *thus extra exposure lines must be updated here.
	 */

	/* OV Recommend Solution */
	/*if shutter bigger than frame_length,
	 *should extend frame length first
	 */
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
		realtime_fps = imgsensor.pclk / imgsensor.line_length * 10 /
				imgsensor.frame_length;
		if (realtime_fps >= 297 && realtime_fps <= 305)
			set_max_framerate(296, 0);
		else if (realtime_fps >= 147 && realtime_fps <= 150)
			set_max_framerate(146, 0);
		else {
			/* Extend frame length */
			write_cmos_sensor_8(0x0104, 0x01);
			write_cmos_sensor_8(0x0340,
					imgsensor.frame_length >> 8);
			write_cmos_sensor_8(0x0341,
					imgsensor.frame_length & 0xFF);
			write_cmos_sensor_8(0x0104, 0x00);
		}
	} else {
		/* Extend frame length */
		write_cmos_sensor_8(0x0104, 0x01);
		write_cmos_sensor_8(0x0340, imgsensor.frame_length >> 8);
		write_cmos_sensor_8(0x0341, imgsensor.frame_length & 0xFF);
		write_cmos_sensor_8(0x0104, 0x00);
	}

	/* Update Shutter */
	write_cmos_sensor_8(0x0104, 0x01);
	write_cmos_sensor_8(0x0350, 0x00); /* Disable auto extend */
	write_cmos_sensor_8(0x0202, (shutter >> 8) & 0xFF);
	write_cmos_sensor_8(0x0203, shutter  & 0xFF);
	write_cmos_sensor_8(0x0104, 0x00);
	LOG_INF(
		"Exit! shutter =%d, framelength =%d/%d, dummy_line=%d, auto_extend=%d\n",
		shutter, imgsensor.frame_length, frame_length,
		dummy_line, read_cmos_sensor_8(0x0350));

}	/* set_shutter_frame_length */

static kal_uint16 gain2reg(const kal_uint16 gain)
{
	 kal_uint16 reg_gain = 0x0;

	reg_gain = 1024 - (1024*64)/gain;
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
	kal_uint16 reg_gain, max_gain = imgsensor_info.max_gain;

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
	LOG_INF("gain = %d, reg_gain = 0x%x, max_gain:0x%x\n ", gain, reg_gain, max_gain);

	write_cmos_sensor_8(0x0104, 0x01);
	write_cmos_sensor_8(0x0204, (reg_gain>>8) & 0xFF);
	write_cmos_sensor_8(0x0205, reg_gain & 0xFF);
	write_cmos_sensor_8(0x0104, 0x00);

	return gain;
} /* set_gain */

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
static kal_uint32 streaming_control(kal_bool enable)
{
	LOG_INF("streaming_enable(0=Sw Standby,1=streaming): %d\n",
		enable);
	if (enable)
		write_cmos_sensor_8(0x0100, 0X01);
	else
		write_cmos_sensor_8(0x0100, 0x00);
	return ERROR_NONE;
}

#define I2C_BUFFER_LEN 255 /* trans# max is 255, each 3 bytes */
static kal_uint16 imx616_table_write_cmos_sensor(kal_uint16 *para,
						 kal_uint32 len)
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
			puSendCmd[tosend++] = (char)(data & 0xFF);
			IDX += 2;
			addr_last = addr;

		}
		/* Write when remain buffer size is less than 3 bytes
		 * or reach end of data
		 */
		if ((I2C_BUFFER_LEN - tosend) < 3
			|| IDX == len || addr != addr_last) {
			for (i = 0; i < 3; i++) {
			LOG_INF("i = %d\n", i);
			ret = iBurstWriteReg_multi(puSendCmd,
						tosend,
						imgsensor.i2c_write_id,
						3,
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

static kal_uint16 imx616_init_setting[] = {
	/* External Clock Setting */
	0x0136,	0x1A,
	0x0137,	0x00,


	/* Register version */
	0x3C7E,	0x06,
	0x3C7F,	0x08,


	/* Signaling mode setting */
	0x0111,	0x02,


	/* Global Setting */
	0x380C,	0x00,
	0x3C00,	0x01,
	0x3C01,	0x00,
	0x3C02,	0x00,
	0x3C03,	0x03,
	0x3C04,	0xFF,
	0x3C05,	0x01,
	0x3C06,	0x00,
	0x3C07,	0x00,
	0x3C08,	0x03,
	0x3C09,	0xFF,
	0x3C0A,	0x00,
	0x3C0B,	0x00,
	0x3C0C,	0x10,
	0x3C0D,	0x10,
	0x3C0E,	0x10,
	0x3C0F,	0x10,
	0x3C10,	0x10,
	0x3C11,	0x20,
	0x3C15,	0x00,
	0x3C16,	0x00,
	0x3C17,	0x00,
	0x3C18,	0x00,
	0x3C19,	0x01,
	0x3C1A,	0x00,
	0x3C1B,	0x01,
	0x3C1C,	0x00,
	0x3C1D,	0x01,
	0x3C1E,	0x00,
	0x3C1F,	0x00,
	0x3F89,	0x01,
	0x3F8F,	0x01,
	0x53B9,	0x01,
	0x571C,	0x00,
	0x571D,	0x72,
	0x5732,	0x00,
	0x5733,	0xAD,
	0x5736,	0x00,
	0x5737,	0x2F,
	0x5748,	0x00,
	0x5749,	0x72,
	0x5754,	0x00,
	0x5755,	0xAD,
	0x5756,	0x00,
	0x5757,	0x59,
	0x5768,	0x00,
	0x5769,	0x8B,
	0x577A,	0x00,
	0x577B,	0x72,
	0x577C,	0x00,
	0x577D,	0x1D,
	0x5784,	0x02,
	0x5785,	0x4E,
	0x5786,	0x00,
	0x5787,	0xAD,
	0x5788,	0x00,
	0x5789,	0x66,
	0x5794,	0x06,
	0x5795,	0xB3,
	0x579C,	0x00,
	0x579D,	0xAD,
	0x579E,	0x00,
	0x579F,	0x48,
	0x57A8,	0x02,
	0x57A9,	0x4E,
	0x57AC,	0x00,
	0x57AD,	0x4F,
	0x57B0,	0x00,
	0x57B1,	0x98,
	0x57B4,	0x06,
	0x57B5,	0xB3,
	0x57B6,	0x00,
	0x57B7,	0x7A,
	0x57BA,	0x02,
	0x57BB,	0x4E,
	0x57C2,	0x00,
	0x57C3,	0x72,
	0x57C4,	0x00,
	0x57C5,	0x18,
	0x57CE,	0x00,
	0x57CF,	0xAD,
	0x57D0,	0x00,
	0x57D1,	0x66,
	0x57DE,	0x00,
	0x57DF,	0x4A,
	0x57E2,	0x00,
	0x57E3,	0x98,
	0x57EC,	0x00,
	0x57ED,	0x72,
	0x57F8,	0x00,
	0x57F9,	0xAD,
	0x57FA,	0x00,
	0x57FB,	0x59,
	0x580C,	0x00,
	0x580D,	0x72,
	0x5818,	0x00,
	0x5819,	0xAD,
	0x581A,	0x00,
	0x581B,	0x59,
	0x582C,	0x00,
	0x582D,	0x72,
	0x5838,	0x00,
	0x5839,	0xAD,
	0x583A,	0x00,
	0x583B,	0x59,
	0x6076,	0x01,
	0x6077,	0x01,
	0x6078,	0x01,
	0x6079,	0x01,
	0x60D5,	0x0F,
	0x60D6,	0x0F,
	0x60D7,	0x0F,
	0x60D8,	0x0F,
	0x60D9,	0x0F,
	0x60DA,	0x0F,
	0x60DB,	0x0F,
	0x60DC,	0x0F,
	0x60DD,	0x0F,
	0x60DE,	0x0F,
	0x60DF,	0x0F,
	0x60E0,	0x0F,
	0x60E1,	0x0F,
	0x60E2,	0x0F,
	0x60E3,	0x0F,
	0x62A2,	0x08,
	0x62A3,	0x08,
	0x62A5,	0x08,
	0x62A7,	0x08,
	0x62A9,	0x08,
	0x62AB,	0x08,
	0x62AD,	0x08,
	0x62B1,	0x08,
	0x62B2,	0x08,
	0x62B3,	0x08,
	0x62B4,	0x08,
	0x62B5,	0x08,
	0x62B6,	0x08,
	0x62B7,	0x10,
	0x62B8,	0x10,
	0x62B9,	0x10,
	0x62BA,	0x10,
	0x62BB,	0x10,
	0x62BC,	0x10,
	0x62BD,	0x10,
	0x62BE,	0x10,
	0x62BF,	0x10,
	0x62C0,	0x10,
	0x62C1,	0x10,
	0x62C2,	0x10,
	0x62C3,	0x10,
	0x62C4,	0x04,
	0x62C5,	0x04,
	0x62C6,	0x04,
	0x62C7,	0x04,
	0x62C8,	0x04,
	0x62C9,	0x04,
	0x62CA,	0x04,
	0x62CB,	0x04,
	0x62CC,	0x04,
	0x62CD,	0x04,
	0x62CE,	0x04,
	0x62CF,	0x04,
	0x62D0,	0x04,
	0x62D1,	0x04,
	0x62D2,	0x04,
	0x62D3,	0x04,
	0x62D4,	0x04,
	0x62D5,	0x04,
	0x62D6,	0x04,
	0x62D7,	0x04,
	0x62D8,	0x04,
	0x62D9,	0x04,
	0x62DA,	0x04,
	0x62DB,	0x04,
	0x62DC,	0x04,
	0x62DD,	0x04,
	0x62DE,	0x04,
	0x62DF,	0x04,
	0x62E0,	0x04,
	0x62E1,	0x04,
	0x62E2,	0x04,
	0x62E3,	0x04,
	0x62E4,	0x04,
	0x62E5,	0x04,
	0x62E6,	0x05,
	0x62E7,	0x05,
	0x62E8,	0x05,
	0x62E9,	0x05,
	0x62EA,	0x05,
	0x62EB,	0x05,
	0x62EC,	0x05,
	0x62ED,	0x05,
	0x62EE,	0x05,
	0x62EF,	0x05,
	0x62F0,	0x05,
	0x62F1,	0x05,
	0x62F2,	0x05,
	0x62F3,	0x01,
	0x62F4,	0x01,
	0x62F5,	0x01,
	0x62F6,	0x01,
	0x62F7,	0x01,
	0x62F8,	0x01,
	0x62F9,	0x01,
	0x62FA,	0x01,
	0x62FB,	0x01,
	0x62FC,	0x01,
	0x62FD,	0x01,
	0x62FE,	0x01,
	0x62FF,	0x01,
	0x6300,	0x01,
	0x6301,	0x01,
	0x6302,	0x01,
	0x6303,	0x01,
	0x6304,	0x01,
	0x6305,	0x01,
	0x6306,	0x01,
	0x6307,	0x01,
	0x6308,	0x01,
	0x6309,	0x01,
	0x630A,	0x01,
	0x630B,	0x01,
	0x630C,	0x01,
	0x630D,	0x01,
	0x630E,	0x01,
	0x630F,	0x01,
	0x6310,	0x01,
	0x6311,	0x01,
	0x6312,	0x01,
	0x6313,	0x01,
	0x6314,	0x01,
	0x6315,	0x01,
	0x6316,	0x01,
	0x6317,	0x01,
	0x6318,	0x01,
	0x6319,	0x01,
	0x631A,	0x01,
	0x631B,	0x01,
	0x631C,	0x01,
	0x631D,	0x01,
	0x631E,	0x01,
	0x631F,	0x01,
	0x6320,	0x01,
	0x6321,	0x01,
	0x6366,	0x00,
	0x6367,	0x55,
	0x6368,	0x00,
	0x6369,	0x55,
	0x636A,	0x00,
	0x636B,	0x55,
	0x636C,	0x00,
	0x636D,	0x55,
	0x636E,	0x00,
	0x636F,	0x55,
	0x6370,	0x00,
	0x6371,	0x55,
	0x6372,	0x00,
	0x6373,	0x55,
	0x6374,	0x00,
	0x6375,	0x55,
	0x6376,	0x00,
	0x6377,	0x55,
	0x6378,	0x00,
	0x6379,	0x55,
	0x637A,	0x00,
	0x637B,	0x55,
	0x637C,	0x00,
	0x637D,	0x55,
	0x637E,	0x00,
	0x637F,	0x55,
	0x63C4,	0x00,
	0x63C5,	0xBC,
	0x63C6,	0x00,
	0x63C7,	0xBC,
	0x63C8,	0x00,
	0x63C9,	0xBC,
	0x63CA,	0x00,
	0x63CB,	0xBC,
	0x63CC,	0x00,
	0x63CD,	0xBC,
	0x63CE,	0x00,
	0x63CF,	0xBC,
	0x63D0,	0x00,
	0x63D1,	0xBC,
	0x63D2,	0x00,
	0x63D3,	0xBC,
	0x63D4,	0x00,
	0x63D5,	0xBC,
	0x63D6,	0x00,
	0x63D7,	0xBC,
	0x63D8,	0x00,
	0x63D9,	0xBC,
	0x63DA,	0x00,
	0x63DB,	0xBC,
	0x63DC,	0x00,
	0x63DD,	0xBC,
	0x63EA,	0x00,
	0x63EB,	0xBC,
	0x63EC,	0x00,
	0x63ED,	0xBC,
	0x63EE,	0x19,
	0x63EF,	0x19,
	0x63F0,	0x19,
	0x63F1,	0x19,
	0x63F2,	0x19,
	0x63F3,	0x19,
	0x63F4,	0x19,
	0x63F5,	0x19,
	0x63F6,	0x19,
	0x63F7,	0x19,
	0x63F8,	0x19,
	0x63F9,	0x19,
	0x63FA,	0x19,
	0x63FB,	0x19,
	0x63FC,	0x19,
	0x63FD,	0x19,
	0x63FE,	0x19,
	0x63FF,	0x19,
	0x6400,	0x19,
	0x6401,	0x19,
	0x6402,	0x19,
	0x6403,	0x19,
	0x6404,	0x19,
	0x6405,	0x19,
	0x6406,	0x19,
	0x6407,	0x19,
	0x6408,	0x19,
	0x6409,	0x19,
	0x640A,	0x19,
	0x640B,	0x19,
	0x640C,	0x19,
	0x640D,	0x19,
	0x640E,	0x19,
	0x640F,	0x19,
	0x6410,	0x19,
	0x6411,	0x19,
	0x6412,	0x19,
	0x6413,	0x19,
	0x6414,	0x19,
	0x6415,	0x19,
	0x6416,	0x19,
	0x6417,	0x19,
	0x6418,	0x19,
	0x6419,	0x19,
	0x641A,	0x19,
	0x641B,	0x19,
	0x641C,	0x19,
	0x644A,	0x00,
	0x644B,	0x21,
	0x644C,	0x00,
	0x644D,	0x1D,
	0x644E,	0x00,
	0x644F,	0x09,
	0x6450,	0x00,
	0x6451,	0x18,
	0x6452,	0x00,
	0x6453,	0x06,
	0x6454,	0x00,
	0x6455,	0x18,
	0x6456,	0x00,
	0x6457,	0x06,
	0x6462,	0x00,
	0x6463,	0x21,
	0x6464,	0x00,
	0x6465,	0x1E,
	0x6466,	0x00,
	0x6467,	0x1C,
	0x6468,	0x00,
	0x6469,	0x18,
	0x646A,	0x00,
	0x646B,	0x17,
	0x646C,	0x00,
	0x646D,	0x18,
	0x646E,	0x00,
	0x646F,	0x17,
	0x647A,	0x00,
	0x647B,	0x1D,
	0x647C,	0x00,
	0x647D,	0x18,
	0x647E,	0x00,
	0x647F,	0x18,
	0x6486,	0x00,
	0x6487,	0x1E,
	0x6488,	0x00,
	0x6489,	0x18,
	0x648A,	0x00,
	0x648B,	0x18,
	0x648C,	0x00,
	0x648D,	0x38,
	0x648E,	0x00,
	0x648F,	0x32,
	0x6490,	0x00,
	0x6491,	0x31,
	0x6492,	0x00,
	0x6493,	0x29,
	0x6494,	0x00,
	0x6495,	0x29,
	0x6496,	0x00,
	0x6497,	0x2A,
	0x6498,	0x00,
	0x6499,	0x29,
	0x64A4,	0x08,
	0x64A5,	0x08,
	0x64A6,	0x01,
	0x64A7,	0x07,
	0x64A8,	0x00,
	0x64A9,	0x05,
	0x64AA,	0x00,
	0x64B0,	0x08,
	0x64B1,	0x07,
	0x64B2,	0x07,
	0x64B3,	0x07,
	0x64B4,	0x07,
	0x64B5,	0x05,
	0x64B6,	0x05,
	0x64BC,	0x07,
	0x64BD,	0x07,
	0x64BE,	0x05,
	0x64C2,	0x07,
	0x64C3,	0x07,
	0x64C4,	0x05,
	0x64C5,	0x0D,
	0x64C6,	0x0C,
	0x64C7,	0x0C,
	0x64C8,	0x0C,
	0x64C9,	0x0C,
	0x64CA,	0x08,
	0x64CB,	0x08,
	0x6558,	0x01,
	0x6559,	0x01,
	0x655A,	0x01,
	0x655B,	0x01,
	0x655C,	0x01,
	0x655D,	0x01,
	0x655E,	0x01,
	0x655F,	0x01,
	0x6560,	0x01,
	0x6561,	0x01,
	0x6562,	0x01,
	0x6563,	0x01,
	0x6564,	0x01,
	0x6565,	0x01,
	0x6566,	0x01,
	0x6567,	0x01,
	0x6568,	0x01,
	0x6569,	0x01,
	0x656A,	0x01,
	0x656B,	0x01,
	0x656C,	0x01,
	0x656D,	0x01,
	0x656E,	0x01,
	0x656F,	0x01,
	0x6570,	0x01,
	0x6571,	0x01,
	0x6572,	0x01,
	0x6573,	0x01,
	0x6574,	0x01,
	0x6575,	0x01,
	0x6576,	0x01,
	0x6577,	0x01,
	0x6578,	0x01,
	0x6579,	0x01,
	0x657A,	0x01,
	0x657B,	0x01,
	0x657C,	0x01,
	0x657D,	0x01,
	0x657E,	0x01,
	0x657F,	0x01,
	0x658F,	0x07,
	0x6590,	0x05,
	0x6591,	0x07,
	0x6592,	0x05,
	0x6593,	0x07,
	0x6594,	0x05,
	0x6595,	0x07,
	0x6596,	0x05,
	0x6597,	0x05,
	0x6598,	0x05,
	0x6599,	0x05,
	0x659A,	0x05,
	0x659B,	0x05,
	0x659C,	0x05,
	0x659D,	0x05,
	0x659E,	0x07,
	0x659F,	0x05,
	0x65A0,	0x07,
	0x65A1,	0x05,
	0x65A2,	0x07,
	0x65A3,	0x05,
	0x65A4,	0x07,
	0x65A5,	0x05,
	0x65A6,	0x05,
	0x65A7,	0x05,
	0x65A8,	0x05,
	0x65A9,	0x05,
	0x65AA,	0x05,
	0x65AB,	0x05,
	0x65AC,	0x05,
	0x65AD,	0x07,
	0x65AE,	0x07,
	0x65AF,	0x07,
	0x65B0,	0x05,
	0x65B1,	0x05,
	0x65B2,	0x05,
	0x65B3,	0x05,
	0x65B4,	0x07,
	0x65B5,	0x07,
	0x65B6,	0x07,
	0x65B7,	0x07,
	0x65B8,	0x05,
	0x65B9,	0x05,
	0x65BA,	0x05,
	0x65BB,	0x05,
	0x65BC,	0x05,
	0x65BD,	0x05,
	0x65BE,	0x05,
	0x65BF,	0x05,
	0x65C0,	0x05,
	0x65C1,	0x05,
	0x65C2,	0x05,
	0x65C3,	0x05,
	0x65C4,	0x05,
	0x65C5,	0x05,
	0x65C6,	0x00,
	0x65C7,	0x34,
	0x65C8,	0x1E,
	0x65C9,	0x28,
	0x65CA,	0x14,
	0x65CB,	0x18,
	0x65CC,	0x0F,
	0x65CE,	0x2C,
	0x65D0,	0x1A,
	0x65D2,	0x30,
	0x65D4,	0x34,
	0x65D5,	0x00,
	0x65D6,	0x46,
	0x65D7,	0x37,
	0x65D8,	0x3C,
	0x65D9,	0x28,
	0x65DA,	0x28,
	0x65DB,	0x23,
	0x65DD,	0x41,
	0x65DF,	0x2D,
	0x65E1,	0x46,
	0x65E3,	0x4B,
	0x65E4,	0x00,
	0x65E5,	0x4B,
	0x65E6,	0x4B,
	0x65E7,	0x41,
	0x65E8,	0x41,
	0x65E9,	0x2D,
	0x65EA,	0x2D,
	0x65EC,	0x3C,
	0x65EE,	0x28,
	0x65F0,	0x41,
	0x65F2,	0x46,
	0x65F3,	0x00,
	0x65F4,	0x00,
	0x65F5,	0x00,
	0x65F6,	0x00,
	0x65F7,	0x00,
	0x65F8,	0x00,
	0x65F9,	0x00,
	0x65FB,	0x00,
	0x65FD,	0x00,
	0x65FF,	0x00,
	0x6601,	0x00,
	0x6E1C,	0x00,
	0x6E1D,	0x00,
	0x6E25,	0x00,
	0x6E38,	0x03,
	0x7529,	0x07,
	0x752A,	0x08,
	0x895C,	0x01,
	0x895D,	0x00,
	0x8966,	0x00,
	0x8967,	0x4E,
	0x896A,	0x00,
	0x896B,	0x24,
	0x896F,	0x34,
	0x8976,	0x00,
	0x8977,	0x00,
	0x9004,	0x15,
	0x9200,	0xB7,
	0x9201,	0x34,
	0x9202,	0xB7,
	0x9203,	0x36,
	0x9204,	0xB7,
	0x9205,	0x37,
	0x9206,	0xB7,
	0x9207,	0x38,
	0x9208,	0xB7,
	0x9209,	0x39,
	0x920A,	0xB7,
	0x920B,	0x3A,
	0x920C,	0xB7,
	0x920D,	0x3C,
	0x920E,	0xB7,
	0x920F,	0x3D,
	0x9210,	0xB7,
	0x9211,	0x3E,
	0x9212,	0xB7,
	0x9213,	0x3F,
	0x9214,	0xF6,
	0x9215,	0x13,
	0x9216,	0xF6,
	0x9217,	0x34,
	0x9218,	0xF4,
	0x9219,	0xA7,
	0x921A,	0xF4,
	0x921B,	0xAA,
	0x921C,	0xF4,
	0x921D,	0xAD,
	0x921E,	0xF4,
	0x921F,	0xB0,
	0x9220,	0xF4,
	0x9221,	0xB3,
	0x9222,	0x85,
	0x9223,	0x77,
	0x9224,	0xC4,
	0x9225,	0x4B,
	0x9226,	0xC4,
	0x9227,	0x4C,
	0x9228,	0xC4,
	0x9229,	0x4D,
	0x922A,	0xF5,
	0x922B,	0x5E,
	0x922C,	0xF5,
	0x922D,	0x5F,
	0x922E,	0xF5,
	0x922F,	0x64,
	0x9230,	0xF5,
	0x9231,	0x65,
	0x9232,	0xF5,
	0x9233,	0x6A,
	0x9234,	0xF5,
	0x9235,	0x6B,
	0x9236,	0xF5,
	0x9237,	0x70,
	0x9238,	0xF5,
	0x9239,	0x71,
	0x923A,	0xF5,
	0x923B,	0x76,
	0x923C,	0xF5,
	0x923D,	0x77,
	0x9360,	0x00,
	0x9361,	0x00,
	0x9362,	0xD8,
	0x9363,	0x00,
	0x9810,	0x14,
	0x9814,	0x14,
	0xB074,	0xF0,
	0xBC7C,	0x11,
	0xBC7D,	0x04,
	0xBC7E,	0x07,
	0xBC7F,	0xB8,
	0xC020,	0x00,
	0xC026,	0x00,
	0xC027,	0x00,
	0xC448,	0x01,
	0xC44F,	0x01,
	0xC450,	0x00,
	0xC451,	0x00,
	0xC452,	0x01,
	0xC455,	0x00,
	0xE206,	0x35,
	0xE226,	0x33,
	0xE266,	0x34,
	0xE2A6,	0x31,
	0xE2C6,	0x37,
	0xE2E6,	0x32,
	0xEC00,	0x01,
	0x3E14,	0x01,
	0xE013,	0x01,
	0xE186,	0x2B,


	/* Image Quality adjustment setting */
	0x88D6,	0x60,
	0x9852,	0x00,
	0xA569,	0x06,
	0xA56A,	0x13,
	0xA56B,	0x13,
	0xA56C,	0x01,
	0xA678,	0x00,
	0xA679,	0x20,
	0xA812,	0x00,
	0xA813,	0x3F,
	0xA814,	0x3F,
	0xA830,	0x68,
	0xA831,	0x56,
	0xA832,	0x2B,
	0xA833,	0x55,
	0xA834,	0x55,
	0xA835,	0x16,
	0xA837,	0x51,
	0xA838,	0x34,
	0xA854,	0x4F,
	0xA855,	0x48,
	0xA856,	0x45,
	0xA857,	0x02,
	0xA85A,	0x23,
	0xA85B,	0x16,
	0xA85C,	0x12,
	0xA85D,	0x02,
	0xAA55,	0x00,
	0xAA56,	0x01,
	0xAA57,	0x30,
	0xAA58,	0x01,
	0xAA59,	0x30,
	0xAC72,	0x01,
	0xAC73,	0x26,
	0xAC74,	0x01,
	0xAC75,	0x26,
	0xAC76,	0x00,
	0xAC77,	0xC4,
	0xAE09,	0x04,
	0xAE0A,	0x16,
	0xAE12,	0x58,
	0xAE13,	0x58,
	0xAE15,	0x10,
	0xAE16,	0x10,
	0xAF05,	0x48,
	0xB069,	0x02,
	0xEA4B,	0x00,
	0xEA4C,	0x00,
	0xEA4D,	0x00,
	0xEA4E,	0x00,
};

static kal_uint16 imx616_capture_setting[] = {
	/* MIPI output setting */
	0x0112,	0x0A,
	0x0113,	0x0A,
	0x0114,	0x03,

	/* Line Length PCK Setting */
	0x0342,	0x0E,
	0x0343,	0xB8,

	/* Frame Length Lines Setting */
	0x0340,	0x1D,
	0x0341,	0x4A,

	/* ROI Setting */
	0x0344,	0x00,
	0x0345,	0x00,
	0x0346,	0x00,
	0x0347,	0x00,
	0x0348,	0x19,
	0x0349,	0x9F,
	0x034A,	0x13,
	0x034B,	0x3F,

	/* Mode Setting */
	0x0220,	0x62,
	0x0222,	0x01,
	0x0900,	0x01,
	0x0901,	0x22,
	0x0902,	0x08,
	0x3140,	0x00,
	0x3246,	0x81,
	0x3247,	0x81,

	/* Digital Crop & Scaling */
	0x0401,	0x00,
	0x0404,	0x00,
	0x0405,	0x10,
	0x0408,	0x00,
	0x0409,	0x08,
	0x040A,	0x00,
	0x040B,	0x08,
	0x040C,	0x0C,
	0x040D,	0xC0,
	0x040E,	0x09,
	0x040F,	0x90,

	/* Output Size Setting */
	0x034C,	0x0C,
	0x034D,	0xC0,
	0x034E,	0x09,
	0x034F,	0x90,

	/* Clock Setting */
	0x0301,	0x05,
	0x0303,	0x02,
	0x0305,	0x04,
	0x0306,	0x01,
	0x0307,	0x46,
	0x030B,	0x01,
	0x030D,	0x03,
	0x030E,	0x00,
	0x030F,	0xED,
	0x0310,	0x01,

	/* Other Setting */
	0x3620,	0x00,
	0x3621,	0x00,
	0x3C12,	0x56,
	0x3C13,	0x52,
	0x3C14,	0x3E,
	0x3F0C,	0x00,
	0x3F14,	0x01,
	0x3F80,	0x04,
	0x3F81,	0x2C,
	0x3F8C,	0x00,
	0x3F8D,	0x00,
	0x3FFC,	0x01,
	0x3FFD,	0x86,
	0x3FFE,	0x00,
	0x3FFF,	0xDC,
	0xBCF1,	0x00,

	/* Integration Setting */
	0x0202,	0x1D,
	0x0203,	0x1A,
	0x0224,	0x01,
	0x0225,	0xF4,
	0x3FE0,	0x01,
	0x3FE1,	0xF4,

	/* Gain Setting */
	0x0204,	0x00,
	0x0205,	0x70,
	0x0216,	0x00,
	0x0217,	0x70,
	0x0218,	0x01,
	0x0219,	0x00,
	0x020E,	0x01,
	0x020F,	0x00,
	0x0210,	0x01,
	0x0211,	0x00,
	0x0212,	0x01,
	0x0213,	0x00,
	0x0214,	0x01,
	0x0215,	0x00,
	0x3FE2,	0x00,
	0x3FE3,	0x70,
	0x3FE4,	0x01,
	0x3FE5,	0x00,

	/* LSC Setting */
	0x0B00,	0x00,

	/* AE-Hist Setting */
	0x323B,	0x00,

	/* AE-Hist Ave Setting */
	0x323D,	0x00,

	/* Flicker Setting */
	0x323C,	0x00,

	/* FACE AE-Hist Setting */
	0x323E,	0x00,

	/* FACE AE-Hist Ave Setting */
	0x323F,	0x00,

	/* DPC output ctrl Setting */
	0x0B06,	0x00,
};

static kal_uint16 imx616_preview_setting[] = {
	/* MIPI output setting */
	0x0112,	0x0A,
	0x0113,	0x0A,
	0x0114,	0x03,

	/* Line Length PCK Setting */
	0x0342,	0x0E,
	0x0343,	0xB8,

	/* Frame Length Lines Setting */
	0x0340,	0x1D,
	0x0341,	0xD4,

	/* ROI Setting */
	0x0344,	0x00,
	0x0345,	0x00,
	0x0346,	0x00,
	0x0347,	0x00,
	0x0348,	0x19,
	0x0349,	0x9F,
	0x034A,	0x13,
	0x034B,	0x3F,

	/* Mode Setting */
	0x0220,	0x62,
	0x0222,	0x01,
	0x0900,	0x01,
	0x0901,	0x22,
	0x0902,	0x08,
	0x3140,	0x00,
	0x3246,	0x81,
	0x3247,	0x81,

	/* Digital Crop & Scaling */
	0x0401,	0x00,
	0x0404,	0x00,
	0x0405,	0x10,
	0x0408,	0x01,
	0x0409,	0xA0,
	0x040A,	0x00,
	0x040B,	0x08,
	0x040C,	0x09,
	0x040D,	0x90,
	0x040E,	0x09,
	0x040F,	0x90,

	/* Output Size Setting */
	0x034C,	0x09,
	0x034D,	0x90,
	0x034E,	0x09,
	0x034F,	0x90,

	/* Clock Setting */
	0x0301,	0x05,
	0x0303,	0x02,
	0x0305,	0x04,
	0x0306,	0x01,
	0x0307,	0x4C,
	0x030B,	0x01,
	0x030D,	0x03,
	0x030E,	0x00,
	0x030F,	0xED,
	0x0310,	0x01,

	/* Other Setting */
	0x3620,	0x00,
	0x3621,	0x00,
	0x3C12,	0x56,
	0x3C13,	0x52,
	0x3C14,	0x3E,
	0x3F0C,	0x00,
	0x3F14,	0x01,
	0x3F80,	0x04,
	0x3F81,	0x2C,
	0x3F8C,	0x00,
	0x3F8D,	0x00,
	0x3FFC,	0x01,
	0x3FFD,	0x86,
	0x3FFE,	0x00,
	0x3FFF,	0xDC,
	0xBCF1,	0x00,

	/* Integration Setting */
	0x0202,	0x1D,
	0x0203,	0xA4,
	0x0224,	0x01,
	0x0225,	0xF4,
	0x3FE0,	0x01,
	0x3FE1,	0xF4,

	/* Gain Setting */
	0x0204,	0x00,
	0x0205,	0x70,
	0x0216,	0x00,
	0x0217,	0x70,
	0x0218,	0x01,
	0x0219,	0x00,
	0x020E,	0x01,
	0x020F,	0x00,
	0x0210,	0x01,
	0x0211,	0x00,
	0x0212,	0x01,
	0x0213,	0x00,
	0x0214,	0x01,
	0x0215,	0x00,
	0x3FE2,	0x00,
	0x3FE3,	0x70,
	0x3FE4,	0x01,
	0x3FE5,	0x00,

	/* LSC Setting */
	0x0B00,	0x00,

	/* AE-Hist Setting */
	0x323B,	0x00,

	/* AE-Hist Ave Setting */
	0x323D,	0x00,

	/* Flicker Setting */
	0x323C,	0x00,

	/* FACE AE-Hist Setting */
	0x323E,	0x00,

	/* FACE AE-Hist Ave Setting */
	0x323F,	0x00,

	/* DPC output ctrl Setting */
	0x0B06,	0x00,
};

static kal_uint16 imx616_normal_video_setting[] = {
	/* MIPI output setting */
	0x0112,	0x0A,
	0x0113,	0x0A,
	0x0114,	0x03,

	/* Line Length PCK Setting */
	0x0342,	0x0E,
	0x0343,	0xB8,

	/* Frame Length Lines Setting */
	0x0340,	0x1D,
	0x0341,	0x4A,

	/* ROI Setting */
	0x0344,	0x00,
	0x0345,	0x00,
	0x0346,	0x00,
	0x0347,	0x00,
	0x0348,	0x19,
	0x0349,	0x9F,
	0x034A,	0x13,
	0x034B,	0x3F,

	/* Mode Setting */
	0x0220,	0x62,
	0x0222,	0x01,
	0x0900,	0x01,
	0x0901,	0x22,
	0x0902,	0x08,
	0x3140,	0x00,
	0x3246,	0x81,
	0x3247,	0x81,

	/* Digital Crop & Scaling */
	0x0401,	0x00,
	0x0404,	0x00,
	0x0405,	0x10,
	0x0408,	0x00,
	0x0409,	0x08,
	0x040A,	0x01,
	0x040B,	0x3A,
	0x040C,	0x0C,
	0x040D,	0xC0,
	0x040E,	0x07,
	0x040F,	0x2C,

	/* Output Size Setting */
	0x034C,	0x0C,
	0x034D,	0xC0,
	0x034E,	0x07,
	0x034F,	0x2C,

	/* Clock Setting */
	0x0301,	0x05,
	0x0303,	0x02,
	0x0305,	0x04,
	0x0306,	0x01,
	0x0307,	0x46,
	0x030B,	0x01,
	0x030D,	0x03,
	0x030E,	0x00,
	0x030F,	0xED,
	0x0310,	0x01,

	/* Other Setting */
	0x3620,	0x00,
	0x3621,	0x00,
	0x3C12,	0x56,
	0x3C13,	0x52,
	0x3C14,	0x3E,
	0x3F0C,	0x00,
	0x3F14,	0x01,
	0x3F80,	0x04,
	0x3F81,	0x2C,
	0x3F8C,	0x00,
	0x3F8D,	0x00,
	0x3FFC,	0x01,
	0x3FFD,	0x86,
	0x3FFE,	0x00,
	0x3FFF,	0xDC,
	0xBCF1,	0x00,

	/* Integration Setting */
	0x0202,	0x1D,
	0x0203,	0x1A,
	0x0224,	0x01,
	0x0225,	0xF4,
	0x3FE0,	0x01,
	0x3FE1,	0xF4,

	/* Gain Setting */
	0x0204,	0x00,
	0x0205,	0x70,
	0x0216,	0x00,
	0x0217,	0x70,
	0x0218,	0x01,
	0x0219,	0x00,
	0x020E,	0x01,
	0x020F,	0x00,
	0x0210,	0x01,
	0x0211,	0x00,
	0x0212,	0x01,
	0x0213,	0x00,
	0x0214,	0x01,
	0x0215,	0x00,
	0x3FE2,	0x00,
	0x3FE3,	0x70,
	0x3FE4,	0x01,
	0x3FE5,	0x00,

	/* LSC Setting */
	0x0B00,	0x00,

	/* AE-Hist Setting */
	0x323B,	0x00,

	/* AE-Hist Ave Setting */
	0x323D,	0x00,

	/* Flicker Setting */
	0x323C,	0x00,

	/* FACE AE-Hist Setting */
	0x323E,	0x00,

	/* FACE AE-Hist Ave Setting */
	0x323F,	0x00,

	/* DPC output ctrl Setting */
	0x0B06,	0x00,
};

static kal_uint16 imx616_hs_video_setting[] = {
	/* MIPI output setting */
	0x0112,	0x0A,
	0x0113,	0x0A,
	0x0114,	0x03,

	/* Line Length PCK Setting */
	0x0342,	0x08,
	0x0343,	0xC8,

	/* Frame Length Lines Setting */
	0x0340,	0x0C,
	0x0341,	0x7E,

	/* ROI Setting */
	0x0344,	0x00,
	0x0345,	0x00,
	0x0346,	0x00,
	0x0347,	0x10,
	0x0348,	0x19,
	0x0349,	0x9F,
	0x034A,	0x13,
	0x034B,	0x2F,

	/* Mode Setting */
	0x0220,	0x62,
	0x0222,	0x01,
	0x0900,	0x01,
	0x0901,	0x44,
	0x0902,	0x08,
	0x3140,	0x00,
	0x3246,	0x89,
	0x3247,	0x89,

	/* Digital Crop & Scaling */
	0x0401,	0x00,
	0x0404,	0x00,
	0x0405,	0x10,
	0x0408,	0x00,
	0x0409,	0x04,
	0x040A,	0x00,
	0x040B,	0x00,
	0x040C,	0x06,
	0x040D,	0x60,
	0x040E,	0x04,
	0x040F,	0xC8,

	/* Output Size Setting */
	0x034C,	0x06,
	0x034D,	0x60,
	0x034E,	0x04,
	0x034F,	0xC8,

	/* Clock Setting */
	0x0301,	0x05,
	0x0303,	0x02,
	0x0305,	0x04,
	0x0306,	0x01,
	0x0307,	0x4C,
	0x030B,	0x01,
	0x030D,	0x03,
	0x030E,	0x00,
	0x030F,	0xED,
	0x0310,	0x01,

	/* Other Setting */
	0x3620,	0x00,
	0x3621,	0x00,
	0x3C12,	0x3E,
	0x3C13,	0x3A,
	0x3C14,	0x22,
	0x3F0C,	0x00,
	0x3F14,	0x00,
	0x3F80,	0x00,
	0x3F81,	0x00,
	0x3F8C,	0x00,
	0x3F8D,	0x00,
	0x3FFC,	0x00,
	0x3FFD,	0x55,
	0x3FFE,	0x00,
	0x3FFF,	0x78,
	0xBCF1,	0x00,

	/* Integration Setting */
	0x0202,	0x0C,
	0x0203,	0x4E,
	0x0224,	0x01,
	0x0225,	0xF4,
	0x3FE0,	0x01,
	0x3FE1,	0xF4,

	/* Gain Setting */
	0x0204,	0x00,
	0x0205,	0x70,
	0x0216,	0x00,
	0x0217,	0x70,
	0x0218,	0x01,
	0x0219,	0x00,
	0x020E,	0x01,
	0x020F,	0x00,
	0x0210,	0x01,
	0x0211,	0x00,
	0x0212,	0x01,
	0x0213,	0x00,
	0x0214,	0x01,
	0x0215,	0x00,
	0x3FE2,	0x00,
	0x3FE3,	0x70,
	0x3FE4,	0x01,
	0x3FE5,	0x00,

	/* LSC Setting */
	0x0B00,	0x00,

	/* AE-Hist Setting */
	0x323B,	0x00,

	/* AE-Hist Ave Setting */
	0x323D,	0x00,

	/* Flicker Setting */
	0x323C,	0x00,

	/* FACE AE-Hist Setting */
	0x323E,	0x00,

	/* FACE AE-Hist Ave Setting */
	0x323F,	0x00,

	/* DPC output ctrl Setting */
	0x0B06,	0x00,
};

static kal_uint16 imx616_slim_video_setting[] = {
	//MIPI output setting
	0x0112,0x0A,
	0x0113,0x0A,
	0x0114,0x03,
	//Line Length PCK Setting
	0x0342,0x0E,
	0x0343,0xB8,
	//Frame Length Lines Setting
	0x0340,0x09,
	0x0341,0xFE,
	//ROI Setting
	0x0344,0x00,
	0x0345,0x00,
	0x0346,0x00,
	0x0347,0x00,
	0x0348,0x19,
	0x0349,0x9F,
	0x034A,0x13,
	0x034B,0x3F,
	//Mode Setting
	0x0220,0x62,
	0x0222,0x01,
	0x0900,0x01,
	0x0901,0x22,
	0x0902,0x08,
	0x3140,0x00,
	0x3246,0x81,
	0x3247,0x81,
	//Digital Crop & Scaling
	0x0401,0x00,
	0x0404,0x00,
	0x0405,0x10,
	0x0408,0x00,
	0x0409,0x00,
	0x040A,0x00,
	0x040B,0x00,
	0x040C,0x0C,
	0x040D,0xD0,
	0x040E,0x09,
	0x040F,0xA0,
	//Output Size Setting
	0x034C,0x0C,
	0x034D,0xD0,
	0x034E,0x09,
	0x034F,0xA0,
	//Clock Setting
	0x0301,0x05,
	0x0303,0x04,
	0x0305,0x04,
	0x0306,0x00,
	0x0307,0xF1,
	0x030B,0x02,
	0x030D,0x09,
	0x030E,0x01,
	0x030F,0xF6,
	0x0310,0x01,
	//Other Setting
	0x3620,0x00,
	0x3621,0x00,
	0x3C12,0x56,
	0x3C13,0x52,
	0x3C14,0x3E,
	0x3F0C,0x00,
	0x3F14,0x01,
	0x3F80,0x00,
	0x3F81,0xA0,
	0x3F8C,0x00,
	0x3F8D,0x00,
	0x3FFC,0x00,
	0x3FFD,0x1E,
	0x3FFE,0x00,
	0x3FFF,0xDC,
	//Integration Setting
	0x0202,0x09,
	0x0203,0xCE,
	0x0224,0x01,
	0x0225,0xF4,
	0x3FE0,0x01,
	0x3FE1,0xF4,
	//Gain Setting
	0x0204,0x00,
	0x0205,0x70,
	0x0216,0x00,
	0x0217,0x70,
	0x0218,0x01,
	0x0219,0x00,
	0x020E,0x01,
	0x020F,0x00,
	0x0210,0x01,
	0x0211,0x00,
	0x0212,0x01,
	0x0213,0x00,
	0x0214,0x01,
	0x0215,0x00,
	0x3FE2,0x00,
	0x3FE3,0x70,
	0x3FE4,0x01,
	0x3FE5,0x00,
	//PDAF TYPE1 Setting
	0x3E20,0x01,
	0x3E37,0x00,
};

static kal_uint16 imx616_custom1_setting[] = {
	//MIPI output setting
	0x0112,0x0A,
	0x0113,0x0A,
	0x0114,0x03,
	//Line Length PCK Setting
	0x0342,0x0E,
	0x0343,0xB8,
	//Frame Length Lines Setting
	0x0340,0x07,
	0x0341,0xE0,
	//ROI Setting
	0x0344,0x00,
	0x0345,0x00,
	0x0346,0x02,
	0x0347,0x60,
	0x0348,0x19,
	0x0349,0x9F,
	0x034A,0x10,
	0x034B,0xDF,
	//Mode Setting
	0x0220,0x62,
	0x0222,0x01,
	0x0900,0x01,
	0x0901,0x22,
	0x0902,0x08,
	0x3140,0x00,
	0x3246,0x81,
	0x3247,0x81,
	//Digital Crop & Scaling
	0x0401,0x00,
	0x0404,0x00,
	0x0405,0x10,
	0x0408,0x00,
	0x0409,0x00,
	0x040A,0x00,
	0x040B,0x00,
	0x040C,0x0C,
	0x040D,0xD0,
	0x040E,0x07,
	0x040F,0x40,
	//Output Size Setting
	0x034C,0x0C,
	0x034D,0xD0,
	0x034E,0x07,
	0x034F,0x40,
	//Clock Setting
	0x0301,0x05,
	0x0303,0x04,
	0x0305,0x04,
	0x0306,0x00,
	0x0307,0xBE,
	0x030B,0x04,
	0x030D,0x0C,
	0x030E,0x04,
	0x030F,0x1A,
	0x0310,0x01,
	//Other Setting
	0x3620,0x00,
	0x3621,0x00,
	0x3C12,0x56,
	0x3C13,0x52,
	0x3C14,0x3E,
	0x3F0C,0x00,
	0x3F14,0x01,
	0x3F80,0x00,
	0x3F81,0xA0,
	0x3F8C,0x00,
	0x3F8D,0x00,
	0x3FFC,0x00,
	0x3FFD,0x1E,
	0x3FFE,0x00,
	0x3FFF,0xDC,
	//Integration Setting
	0x0202,0x07,
	0x0203,0xB0,
	0x0224,0x01,
	0x0225,0xF4,
	0x3FE0,0x01,
	0x3FE1,0xF4,
	//Gain Setting
	0x0204,0x00,
	0x0205,0x70,
	0x0216,0x00,
	0x0217,0x70,
	0x0218,0x01,
	0x0219,0x00,
	0x020E,0x01,
	0x020F,0x00,
	0x0210,0x01,
	0x0211,0x00,
	0x0212,0x01,
	0x0213,0x00,
	0x0214,0x01,
	0x0215,0x00,
	0x3FE2,0x00,
	0x3FE3,0x70,
	0x3FE4,0x01,
	0x3FE5,0x00,
	//PDAF TYPE1 Setting
	0x3E20,0x01,
	0x3E37,0x00,

};

static void sensor_init(void)
{
	LOG_INF("sensor_init start\n");

	imx616_table_write_cmos_sensor(imx616_init_setting,
		sizeof(imx616_init_setting)/sizeof(kal_uint16));

	set_mirror_flip(imgsensor.mirror);
	LOG_INF("sensor_init End\n");
}	/*	  sensor_init  */

static void preview_setting(void)
{
	LOG_INF("preview_setting Start\n");

	imx616_table_write_cmos_sensor(imx616_preview_setting,
		sizeof(imx616_preview_setting)/sizeof(kal_uint16));

	LOG_INF("preview_setting End\n");
} /* preview_setting */


/*full size 30fps*/
static void capture_setting(kal_uint16 currefps)
{
	LOG_INF("%s 24 fps E! currefps:%d\n", __func__, currefps);
	/*************MIPI output setting************/

	imx616_table_write_cmos_sensor(imx616_capture_setting,
		sizeof(imx616_capture_setting)/sizeof(kal_uint16));

	LOG_INF("%s 30 fpsX\n", __func__);
}

static void normal_video_setting(kal_uint16 currefps)
{
	LOG_INF("%s E! currefps:%d\n", __func__, currefps);

	imx616_table_write_cmos_sensor(imx616_normal_video_setting,
		sizeof(imx616_normal_video_setting)/sizeof(kal_uint16));

	LOG_INF("X\n");
}

static void hs_video_setting(void)
{
	LOG_INF("%s E! currefps 30\n", __func__);

	imx616_table_write_cmos_sensor(imx616_hs_video_setting,
		sizeof(imx616_hs_video_setting)/sizeof(kal_uint16));

	LOG_INF("X\n");
}

static void slim_video_setting(void)
{
	LOG_INF("%s E!\n", __func__);

	imx616_table_write_cmos_sensor(imx616_slim_video_setting,
		sizeof(imx616_slim_video_setting)/sizeof(kal_uint16));

	LOG_INF("X\n");
}

static void custom1_setting(void)
{
	LOG_INF("%s 16:9 E! currefps\n", __func__);
	/*************MIPI output setting************/

	imx616_table_write_cmos_sensor(imx616_custom1_setting,
		sizeof(imx616_custom1_setting)/sizeof(kal_uint16));

	LOG_INF("X");
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
	/*sensor have two i2c address 0x34 & 0x20,
	 *we should detect the module used i2c address
	 */
	while (imgsensor_info.i2c_addr_table[i] != 0xff) {
		spin_lock(&imgsensor_drv_lock);
		imgsensor.i2c_write_id = imgsensor_info.i2c_addr_table[i];
		spin_unlock(&imgsensor_drv_lock);
		do {
			LOG_INF("read sensor id\n");
			*sensor_id = ((read_cmos_sensor_8(0x0016) << 8)
					| read_cmos_sensor_8(0x0017));
			LOG_INF(
				"read_0x0000=0x%x, 0x0001=0x%x,0x0000_0001=0x%x\n",
				read_cmos_sensor_8(0x0016),
				read_cmos_sensor_8(0x0017),
				read_cmos_sensor(0x0000));
			if (*sensor_id == imgsensor_info.sensor_id) {
				LOG_INF("i2c write id: 0x%x, sensor id: 0x%x\n",
					imgsensor.i2c_write_id, *sensor_id);
				#ifdef VENDOR_EDIT
				/*Caohua.Lin@Camera.Driver add for 18011/18311  board 20180723*/
				imgsensor_info.module_id = read_module_id();
				read_eeprom_SN();
				/*Henry.Chang@Camera.Driver add for google ARCode Feature verify 20190531*/
				read_eeprom_CamInfo();
				LOG_INF("IMX616_module_id=%d\n",imgsensor_info.module_id);
				if(deviceInfo_register_value == 0x00){
					register_imgsensor_deviceinfo("Cam_r0", DEVICE_VERSION_IMX616,
									imgsensor_info.module_id);
					deviceInfo_register_value = 0x01;
				}
				#endif
				/*if (is_project(OPPO_18151) || is_project(OPPO_19011) || is_project(OPPO_19301)) {
					pr_info("18151/19011/19301: imx616 set mirrorflip\n");
					imgsensor.mirror = IMAGE_HV_MIRROR;
					imgsensor_info.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_4CELL_HW_BAYER_B;
				}*/
				return ERROR_NONE;
			}

			LOG_INF("Read sensor id fail, id: 0x%x\n",
				imgsensor.i2c_write_id);
			retry--;
		} while (retry > 0);
		i++;
		retry = 2;
	}
	if (*sensor_id != imgsensor_info.sensor_id) {
		/*if Sensor ID is not correct,
		 *Must set *sensor_id to 0xFFFFFFFF
		 */
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

	LOG_INF("IMX616 open start\n");
	/*sensor have two i2c address 0x6c 0x6d & 0x21 0x20,
	 *we should detect the module used i2c address
	 */
	while (imgsensor_info.i2c_addr_table[i] != 0xff) {
		spin_lock(&imgsensor_drv_lock);
		imgsensor.i2c_write_id = imgsensor_info.i2c_addr_table[i];
		spin_unlock(&imgsensor_drv_lock);
		do {
			sensor_id = ((read_cmos_sensor_8(0x0016) << 8)
					| read_cmos_sensor_8(0x0017));
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
	LOG_INF("IMX616 open End\n");
	return ERROR_NONE;
} /* open */

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
	/* No Need to implement this function */
	streaming_control(KAL_FALSE);
	#ifdef VENDOR_EDIT
	qsc_flag = 0;
	#endif
	return ERROR_NONE;
} /* close */


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
	LOG_INF("%s E\n", __func__);

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_PREVIEW;
	imgsensor.pclk = imgsensor_info.pre.pclk;
	imgsensor.line_length = imgsensor_info.pre.linelength;
	imgsensor.frame_length = imgsensor_info.pre.framelength;
	imgsensor.min_frame_length = imgsensor_info.pre.framelength;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);

	preview_setting();

	return ERROR_NONE;
} /* preview */

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
#if 0
	if (imgsensor.current_fps == imgsensor_info.cap1.max_framerate) {
	/* PIP capture: 24fps for less than 13M, 20fps for 16M,15fps for 20M */
		imgsensor.pclk = imgsensor_info.cap1.pclk;
		imgsensor.line_length = imgsensor_info.cap1.linelength;
		imgsensor.frame_length = imgsensor_info.cap1.framelength;
		imgsensor.min_frame_length = imgsensor_info.cap1.framelength;
		imgsensor.autoflicker_en = KAL_FALSE;
	} else if (imgsensor.current_fps == imgsensor_info.cap2.max_framerate) {
		if (imgsensor.current_fps != imgsensor_info.cap.max_framerate)
			LOG_INF(
			"Warning: current_fps %d fps is not support, so use cap1's setting: %d fps!\n",
		imgsensor.current_fps, imgsensor_info.cap1.max_framerate/10);
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
			imgsensor_info.cap.max_framerate / 10);
		imgsensor.pclk = imgsensor_info.cap.pclk;
		imgsensor.line_length = imgsensor_info.cap.linelength;
		imgsensor.frame_length = imgsensor_info.cap.framelength;
		imgsensor.min_frame_length = imgsensor_info.cap.framelength;
		imgsensor.autoflicker_en = KAL_FALSE;
	}
#else
	if (imgsensor.current_fps != imgsensor_info.cap.max_framerate)
		LOG_INF(
			"Warning: current_fps %d fps is not support, so use cap's setting: %d fps!\n",
			imgsensor.current_fps,
			imgsensor_info.cap.max_framerate / 10);
	imgsensor.pclk = imgsensor_info.cap.pclk;
	imgsensor.line_length = imgsensor_info.cap.linelength;
	imgsensor.frame_length = imgsensor_info.cap.framelength;
	imgsensor.min_frame_length = imgsensor_info.cap.framelength;
	imgsensor.autoflicker_en = KAL_FALSE;
#endif

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
	LOG_INF("%s. 720P@240FPS\n", __func__);

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
}	/* slim_video */


static kal_uint32 custom1(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("%s.\n", __func__);

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
}	/* custom1 */

static kal_uint32
get_resolution(MSDK_SENSOR_RESOLUTION_INFO_STRUCT *sensor_resolution)
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

	sensor_resolution->SensorSlimVideoWidth =
		imgsensor_info.slim_video.grabwindow_width;
	sensor_resolution->SensorSlimVideoHeight =
		imgsensor_info.slim_video.grabwindow_height;

	sensor_resolution->SensorCustom1Width =
		imgsensor_info.custom1.grabwindow_width;
	sensor_resolution->SensorCustom1Height =
		imgsensor_info.custom1.grabwindow_height;

		return ERROR_NONE;
} /* get_resolution */

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
	sensor_info->PDAF_Support = 0;
	sensor_info->SensorMIPILaneNumber = imgsensor_info.mipi_lane_num;
	sensor_info->SensorClockFreq = imgsensor_info.mclk;
	sensor_info->SensorClockDividCount = 3; /* not use */
	sensor_info->SensorClockRisingCount = 0;
	sensor_info->SensorClockFallingCount = 2; /* not use */
	sensor_info->SensorPixelClockCount = 3; /* not use */
	sensor_info->SensorDataLatchCount = 2; /* not use */

	sensor_info->MIPIDataLowPwr2HighSpeedTermDelayCount = 0;
	sensor_info->MIPICLKLowPwr2HighSpeedTermDelayCount = 0;
	sensor_info->SensorWidthSampling = 0; /* 0 is default 1x */
	sensor_info->SensorHightSampling = 0; /* 0 is default 1x */
	sensor_info->SensorPacketECCOrder = 1;

	sensor_info->FrameTimeDelayFrame = imgsensor_info.frame_time_delay_frame;

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
}	/*	get_info  */


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
		frame_length = imgsensor_info.pre.pclk / framerate * 10
				/ imgsensor_info.pre.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line =
			(frame_length > imgsensor_info.pre.framelength)
		? (frame_length - imgsensor_info.pre.framelength) : 0;
		imgsensor.frame_length =
			imgsensor_info.pre.framelength
			+ imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
		break;
	case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
		if (framerate == 0)
			return ERROR_NONE;
		frame_length = imgsensor_info.normal_video.pclk /
				framerate * 10 /
				imgsensor_info.normal_video.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line =
			(frame_length > imgsensor_info.normal_video.framelength)
		? (frame_length - imgsensor_info.normal_video.framelength)
		: 0;
		imgsensor.frame_length =
			imgsensor_info.normal_video.framelength
			+ imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
		break;
	case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		if (imgsensor.current_fps != imgsensor_info.cap.max_framerate)
			LOG_INF(
				"Warning: current_fps %d fps is not support, so use cap's setting: %d fps!\n"
				, framerate, imgsensor_info.cap.max_framerate/10);
			frame_length = imgsensor_info.cap.pclk / framerate * 10
					/ imgsensor_info.cap.linelength;
			spin_lock(&imgsensor_drv_lock);
				imgsensor.dummy_line =
				(frame_length > imgsensor_info.cap.framelength)
				  ? (frame_length - imgsensor_info.cap.framelength) : 0;
				imgsensor.frame_length =
					imgsensor_info.cap.framelength
					+ imgsensor.dummy_line;
				imgsensor.min_frame_length = imgsensor.frame_length;
				spin_unlock(&imgsensor_drv_lock);
			if (imgsensor.frame_length > imgsensor.shutter)
				set_dummy();
		break;
	case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
		frame_length = imgsensor_info.hs_video.pclk / framerate * 10
				/ imgsensor_info.hs_video.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line =
			(frame_length > imgsensor_info.hs_video.framelength)
			  ? (frame_length - imgsensor_info.hs_video.framelength)
			  : 0;
		imgsensor.frame_length =
			imgsensor_info.hs_video.framelength
				+ imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
		break;
	case MSDK_SCENARIO_ID_SLIM_VIDEO:
		frame_length = imgsensor_info.slim_video.pclk / framerate * 10
			/ imgsensor_info.slim_video.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line =
			(frame_length > imgsensor_info.slim_video.framelength)
			? (frame_length - imgsensor_info.slim_video.framelength)
			: 0;
		imgsensor.frame_length =
			imgsensor_info.slim_video.framelength
			+ imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
		break;
	case MSDK_SCENARIO_ID_CUSTOM1:
		frame_length = imgsensor_info.custom1.pclk / framerate * 10
				/ imgsensor_info.custom1.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line =
			(frame_length > imgsensor_info.custom1.framelength)
			? (frame_length - imgsensor_info.custom1.framelength)
			: 0;
		imgsensor.frame_length =
			imgsensor_info.custom1.framelength
			+ imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
		break;
	default:  /*coding with  preview scenario by default*/
		frame_length = imgsensor_info.pre.pclk / framerate * 10
			/ imgsensor_info.pre.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line =
			(frame_length > imgsensor_info.pre.framelength)
			? (frame_length - imgsensor_info.pre.framelength) : 0;
		imgsensor.frame_length =
			imgsensor_info.pre.framelength + imgsensor.dummy_line;
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

	if (enable)
		write_cmos_sensor_8(0x0601, 0x0002); /*100% Color bar*/
	else
		write_cmos_sensor_8(0x0601, 0x0000); /*No pattern*/

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
	/* unsigned long long *feature_return_para
	 *  = (unsigned long long *) feature_para;
	 */
	struct SET_PD_BLOCK_INFO_T *PDAFinfo;
	struct SENSOR_WINSIZE_INFO_STRUCT *wininfo;
	struct SENSOR_VC_INFO_STRUCT *pvcinfo;
	/* SET_SENSOR_AWB_GAIN *pSetSensorAWB
	 *  = (SET_SENSOR_AWB_GAIN *)feature_para;
	 */
	MSDK_SENSOR_REG_INFO_STRUCT *sensor_reg_data
		= (MSDK_SENSOR_REG_INFO_STRUCT *) feature_para;

	/*LOG_INF("feature_id = %d\n", feature_id);*/
	switch (feature_id) {
	#ifdef VENDOR_EDIT
	/*Henry.Chang@Camera.Driver add for google ARCode Feature verify 20190531*/
	case SENSOR_FEATURE_GET_MODULE_INFO:
		LOG_INF("imx616 GET_MODULE_CamInfo:%d %d\n", *feature_para_len, *feature_data_32);
		*(feature_data_32 + 1) = (gImx616_CamInfo[1] << 24)
					| (gImx616_CamInfo[0] << 16)
					| (gImx616_CamInfo[3] << 8)
					| (gImx616_CamInfo[2] & 0xFF);
		*(feature_data_32 + 2) = (gImx616_CamInfo[5] << 24)
					| (gImx616_CamInfo[4] << 16)
					| (gImx616_CamInfo[7] << 8)
					| (gImx616_CamInfo[6] & 0xFF);
		break;
	/*Henry.Chang@Camera.Driver add for 18531 ModuleSN*/
	case SENSOR_FEATURE_GET_MODULE_SN:
		LOG_INF("imx616 GET_MODULE_SN:%d %d\n", *feature_para_len, *feature_data_32);
		if (*feature_data_32 < CAMERA_MODULE_SN_LENGTH/4)
			*(feature_data_32 + 1) = (gImx616_SN[4*(*feature_data_32) + 3] << 24)
						| (gImx616_SN[4*(*feature_data_32) + 2] << 16)
						| (gImx616_SN[4*(*feature_data_32) + 1] << 8)
						| (gImx616_SN[4*(*feature_data_32)] & 0xFF);
		break;
	/*Henry.Chang@camera.driver 20181129, add for sensor Module SET*/
	case SENSOR_FEATURE_SET_SENSOR_OTP:
	/*{
		kal_int32 ret = IMGSENSOR_RETURN_SUCCESS;
		LOG_INF("SENSOR_FEATURE_SET_SENSOR_OTP length :%d\n", (UINT32)*feature_para_len);
		ret = write_Module_data((ACDK_SENSOR_ENGMODE_STEREO_STRUCT *)(feature_para));
		if (ret == ERROR_NONE)
			return ERROR_NONE;
		else
			return ERROR_MSDK_IS_ACTIVED;
	}*/
	break;
	/*Caohua.Lin@Camera.Driver , 20190318, add for ITS--sensor_fusion*/
	case SENSOR_FEATURE_GET_OFFSET_TO_START_OF_EXPOSURE:
		LOG_INF("exporsure");
	break;
	#endif
	/*Longyuan.Yang@camera.driver add for video crash */
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
        case MSDK_SCENARIO_ID_SLIM_VIDEO:
                *(MUINT32 *)(uintptr_t)(*(feature_data + 1))
                        = imgsensor_info.slim_video.pclk;
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
        case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
        default:
                *(MUINT32 *)(uintptr_t)(*(feature_data + 1))
                        = (imgsensor_info.pre.framelength << 16)
                                 + imgsensor_info.pre.linelength;
                break;
        }
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
	case SENSOR_FEATURE_SET_ESHUTTER:
		 set_shutter(*feature_data);
		break;
	case SENSOR_FEATURE_SET_NIGHTMODE:
		 /* night_mode((BOOL) *feature_data); */
		break;
	#ifdef VENDOR_EDIT
	case SENSOR_FEATURE_CHECK_MODULE_ID:
		*feature_return_para_32 = imgsensor_info.module_id;
		break;
	#endif
	case SENSOR_FEATURE_SET_GAIN:
		set_gain((UINT16) *feature_data);
		break;
	case SENSOR_FEATURE_SET_FLASHLIGHT:
		break;
	case SENSOR_FEATURE_SET_ISP_MASTER_CLOCK_FREQ:
		break;
	case SENSOR_FEATURE_SET_REGISTER:
		write_cmos_sensor_8(sensor_reg_data->RegAddr,
				    sensor_reg_data->RegData);
		break;
	case SENSOR_FEATURE_GET_REGISTER:
		sensor_reg_data->RegData =
			read_cmos_sensor_8(sensor_reg_data->RegAddr);
		break;
	case SENSOR_FEATURE_GET_LENS_DRIVER_ID:
		/*get the lens driver ID from EEPROM
		 * or just return LENS_DRIVER_ID_DO_NOT_CARE
		 * if EEPROM does not exist in camera module.
		 */
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
	case SENSOR_FEATURE_GET_PDAF_DATA:
		LOG_INF("SENSOR_FEATURE_GET_PDAF_DATA\n");
		break;
	case SENSOR_FEATURE_SET_TEST_PATTERN:
		set_test_pattern_mode((BOOL)*feature_data);
		break;
	case SENSOR_FEATURE_GET_TEST_PATTERN_CHECKSUM_VALUE:
		/* for factory mode auto testing */
		*feature_return_para_32 = imgsensor_info.checksum_value;
		*feature_para_len = 4;
		break;
	case SENSOR_FEATURE_SET_FRAMERATE:
		LOG_INF("current fps :%d\n", (UINT32)*feature_data_32);
		spin_lock(&imgsensor_drv_lock);
		imgsensor.current_fps = *feature_data_32;
		spin_unlock(&imgsensor_drv_lock);
		break;
	case SENSOR_FEATURE_SET_HDR:
		LOG_INF("ihdr enable :%d\n", (BOOL)*feature_data_32);
		spin_lock(&imgsensor_drv_lock);
		imgsensor.ihdr_mode = *feature_data_32;
		spin_unlock(&imgsensor_drv_lock);
		break;
	case SENSOR_FEATURE_GET_CROP_INFO:
		LOG_INF("SENSOR_FEATURE_GET_CROP_INFO scenarioId:%d\n",
			(UINT32)*feature_data);
		wininfo =
	(struct SENSOR_WINSIZE_INFO_STRUCT *)(uintptr_t)(*(feature_data+1));

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
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		default:
			memcpy((void *)wininfo,
			(void *)&imgsensor_winsize_info[0],
			sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		}
		break;
	case SENSOR_FEATURE_GET_PDAF_INFO:
		LOG_INF("SENSOR_FEATURE_GET_PDAF_INFO scenarioId:%d\n",
			(UINT16) *feature_data);
		PDAFinfo =
		  (struct SET_PD_BLOCK_INFO_T *)(uintptr_t)(*(feature_data+1));
		break;
	case SENSOR_FEATURE_SET_PDAF:
		LOG_INF("PDAF mode :%d\n", *feature_data_16);
		imgsensor.pdaf_mode = *feature_data_16;
		break;
	case SENSOR_FEATURE_SET_IHDR_SHUTTER_GAIN:
		LOG_INF("SENSOR_SET_SENSOR_IHDR LE=%d, SE=%d, Gain=%d\n",
			(UINT16)*feature_data,
			(UINT16)*(feature_data+1),
			(UINT16)*(feature_data+2));
		break;
	case SENSOR_FEATURE_SET_SHUTTER_FRAME_TIME:
		set_shutter_frame_length((UINT16) (*feature_data),
					(UINT16) (*(feature_data + 1)));
		break;
	case SENSOR_FEATURE_SET_HDR_SHUTTER:
		LOG_INF("SENSOR_FEATURE_SET_HDR_SHUTTER LE=%d, SE=%d\n",
			(UINT16)*feature_data, (UINT16)*(feature_data+1));
		#if 0
		ihdr_write_shutter((UINT16)*feature_data,
				   (UINT16)*(feature_data+1));
		#endif
		break;
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
	case SENSOR_FEATURE_GET_BINNING_TYPE:
		switch (*(feature_data + 1)) {
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		default:
			*feature_return_para_32 = 1; /*BINNING_AVERAGED*/
			break;
		}
		pr_debug("SENSOR_FEATURE_GET_BINNING_TYPE AE_binning_type:%d,\n",
			*feature_return_para_32);
		*feature_para_len = 4;

		break;
	case SENSOR_FEATURE_GET_MIPI_PIXEL_RATE:
	{
		switch (*feature_data) {
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.cap.mipi_pixel_rate;
			break;
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.normal_video.mipi_pixel_rate;
			break;
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.hs_video.mipi_pixel_rate;
			break;
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.slim_video.mipi_pixel_rate;
			break;
		case MSDK_SCENARIO_ID_CUSTOM1:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.custom1.mipi_pixel_rate;
			break;
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		default:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.pre.mipi_pixel_rate;
			break;
		}
	}
break;

	case SENSOR_FEATURE_GET_VC_INFO:
		LOG_INF("SENSOR_FEATURE_GET_VC_INFO %d\n",
			(UINT16)*feature_data);
		pvcinfo =
		 (struct SENSOR_VC_INFO_STRUCT *)(uintptr_t)(*(feature_data+1));
		break;
	case SENSOR_FEATURE_SET_AWB_GAIN:
		break;
	case SENSOR_FEATURE_SET_LSC_TBL:
		break;
	default:
		break;
	}

	return ERROR_NONE;
} /* feature_control() */

static struct SENSOR_FUNCTION_STRUCT sensor_func = {
	open,
	get_info,
	get_resolution,
	feature_control,
	control,
	close
};

UINT32 IMX616_MIPI_RAW_SensorInit(struct SENSOR_FUNCTION_STRUCT **pfFunc)
{
	/* To Do : Check Sensor status here */
	if (pfFunc != NULL)
		*pfFunc = &sensor_func;
	return ERROR_NONE;
} /* IMX616_MIPI_RAW_SensorInit */
