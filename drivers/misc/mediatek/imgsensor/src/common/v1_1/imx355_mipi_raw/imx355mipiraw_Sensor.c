/*
* Copyright (C) 2021 MediaTek Inc.
*
* This program is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 as
* published by the Free Software Foundation.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU General Public License for more details.
*/

/*****************************************************************************
*
* Filename:
* ---------
*	 imx355mipi_Sensor.c
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
/********************Modify Following Strings for Debug************************/
#define PFX "IMX355 D/D"
#define pr_fmt(fmt) PFX "[%s] " fmt, __func__
/****************************   Modify end    *********************************/

#define LOG_DBG(format, args...) pr_debug(format, ##args)
#define LOG_INF(format, args...) pr_info(format, ##args)
#define LOG_ERR(format, args...) pr_err(format, ##args)

#include <linux/videodev2.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <asm/atomic.h>
#include <linux/types.h>

#include "kd_imgsensor.h"
#include "kd_imgsensor_define.h"
#include "kd_imgsensor_errcode.h"
#include "kd_camera_typedef.h"
#include "imx355mipiraw_Sensor.h"
#include "imx355mipiraw_setfile.h"

#define MULTI_WRITE 1
#if MULTI_WRITE
static const int I2C_BUFFER_LEN = 1020;
#else
static const int I2C_BUFFER_LEN = 4;
#endif

static bool enable_adaptive_mipi;
static int adaptive_mipi_index;

static DEFINE_SPINLOCK(imgsensor_drv_lock);


static struct imgsensor_info_struct imgsensor_info = {
	.sensor_id = IMX355_SENSOR_ID,
	.checksum_value = 0x9e08861c,
	.pre = {
		.pclk = 280800000,
		.linelength = 3672,
		.framelength = 2548,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 3264,
		.grabwindow_height = 2448,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 280800000,
		.max_framerate = 300,
	},
	.cap = {
		.pclk = 280800000,
		.linelength = 3672,
		.framelength = 2548,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 3264,
		.grabwindow_height = 2448,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 280800000,
		.max_framerate = 300,
	},
	.normal_video = {
		.pclk = 280800000,
		.linelength = 3672,
		.framelength = 2548,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 3264,
		.grabwindow_height = 1836,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 280800000,
		.max_framerate = 300,
	},
	.hs_video = { /* Dummy, not used */
		.pclk = 280800000,
		.linelength = 3672,
		.framelength = 2548,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 3264,
		.grabwindow_height = 1836,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 280800000,
		.max_framerate = 300,
	},
	.slim_video = { /* Dummy, not used */
		.pclk = 280800000,
		.linelength = 3672,
		.framelength = 2548,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 3264,
		.grabwindow_height = 1836,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 280800000,
		.max_framerate = 300,
	},
	.custom1 = {
		.pclk = 280800000,
		.linelength = 3672,
		.framelength = 1272,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 736,
		.grabwindow_height = 552,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 280800000,
		.max_framerate = 600,
	},
	.margin = 5,			//sensor framelength & shutter margin
	.min_shutter = 2,		//min shutter
	.min_gain = 64, 		//1x gain
	.max_gain = 1024, 		//16x gain
	.min_gain_iso = 50,
	.exp_step = 1,
	.gain_step = 1,
	.gain_type = 0,
	.max_frame_length = 0xffff,//max framelength by sensor register's limitation
	.ae_shut_delay_frame = 0,	//shutter delay frame for AE cycle, 2 frame with ispGain_delay-shut_delay=2-0=2
	.ae_sensor_gain_delay_frame = 0,//sensor gain delay frame for AE cycle,2 frame with ispGain_delay-sensor_gain_delay=2-0=2
	.ae_ispGain_delay_frame = 2,//isp gain delay frame for AE cycle
	.ihdr_support = 0,	  //1, support; 0,not support
	.ihdr_le_firstline = 0,  //1,le first ; 0, se first
	.sensor_mode_num = 6,	  //support sensor mode num

	.cap_delay_frame = 2,		//enter capture delay frame num
	.pre_delay_frame = 2, 		//enter preview delay frame num
	.video_delay_frame = 2,		//enter video delay frame num
	.hs_video_delay_frame = 2,		//dummy
	.slim_video_delay_frame = 2,		//dummy
	.custom1_delay_frame = 2,

	.isp_driving_current = ISP_DRIVING_6MA,
	.sensor_interface_type = SENSOR_INTERFACE_TYPE_MIPI,
	.mipi_sensor_type = MIPI_OPHY_NCSI2,
	.mipi_settle_delay_mode = MIPI_SETTLEDELAY_MANUAL,
	.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_R,
	.mclk = 26,
	.mipi_lane_num = SENSOR_MIPI_4_LANE,
	.i2c_addr_table = {0x34, 0xff},
	.i2c_speed = 400,
};


static struct imgsensor_struct imgsensor = {
	.mirror = IMAGE_NORMAL,				//mirrorflip information
	.sensor_mode = IMGSENSOR_MODE_INIT, //IMGSENSOR_MODE enum value,record current sensor mode,such as: INIT, Preview, Capture, Video,High Speed Video, Slim Video
	.shutter = 0x3D0,					//current shutter
	.gain = 0x100,						//current gain
	.dummy_pixel = 0,					//current dummypixel
	.dummy_line = 0,					//current dummyline
	.current_fps = 300,  //full size current fps : 24fps for PIP, 30fps for Normal or ZSD
	.autoflicker_en = KAL_FALSE,  //auto flicker enable: KAL_FALSE for disable auto flicker, KAL_TRUE for enable auto flicker
	.test_pattern = KAL_FALSE,		//test pattern mode or not. KAL_FALSE for in test pattern mode, KAL_TRUE for normal output
	.current_scenario_id = MSDK_SCENARIO_ID_CAMERA_PREVIEW,//current scenario id
	.ihdr_mode = KAL_FALSE, //sensor need support LE, SE with HDR feature
	.i2c_write_id = 0x34,//record current sensor's i2c write id
};


/* Sensor output window information */
static struct SENSOR_WINSIZE_INFO_STRUCT imgsensor_winsize_info[6] = {
	{3280, 2464,    8,    8, 3264, 2448, 3264, 2448,
	    0,    0, 3264, 2448,    0,    0, 3264, 2448},	// preview
	{3280, 2464,    8,    8, 3264, 2448, 3264, 2448,
	    0,    0, 3264, 2448,    0,    0, 3264, 2448},	// capture
	{3280, 2464,    8,  312, 3264, 1836, 3264, 1836,
	    0,    0, 3264, 1836,    0,    0, 3264, 1836},	// video
	{3280, 2464,    8,  312, 3264, 1836, 3264, 1836,
	    0,    0, 3264, 1836,    0,    0, 3264, 1836},	// hs_video
	{3280, 2464,    8,  312, 3264, 1836, 3264, 1836,
	    0,    0, 3264, 1836,    0,    0, 3264, 1836},	// slim_video
	{3280, 2464,  168,  128, 2944, 2208,  736,  552,
	    0,    0,  736,  552,    0,    0,  736,  552},	// custom1
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
	char pu_send_cmd[3] = {
		(char)(addr >> 8), (char)(addr & 0xFF), (char)(para & 0xFF) };

	iWriteRegI2C(pu_send_cmd, 3, imgsensor.i2c_write_id);
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
			puSendCmd[tosend++] = (char)(data & 0xFF);
			IDX += 2;
			addr_last = addr;

		}
#if MULTI_WRITE

	if ((I2C_BUFFER_LEN - tosend) < 3 || IDX == len || addr != addr_last) {
		iBurstWriteReg_multi(
		puSendCmd, tosend, imgsensor.i2c_write_id, 3,
				     imgsensor_info.i2c_speed);
		tosend = 0;
	}
#else
		iWriteRegI2CTiming(puSendCmd, 3,
			imgsensor.i2c_write_id, imgsensor_info.i2c_speed);

		tosend = 0;
#endif
	}
	return 0;
}

static void set_dummy(void)
{
	LOG_DBG("dummyline = %d, dummypixels = %d \n", imgsensor.dummy_line, imgsensor.dummy_pixel);

	write_cmos_sensor_8(0x0104, 0x01);
	write_cmos_sensor_8(0x0340, imgsensor.frame_length >> 8);
	write_cmos_sensor_8(0x0341, imgsensor.frame_length & 0xFF);
	write_cmos_sensor_8(0x0342, imgsensor.line_length >> 8);
	write_cmos_sensor_8(0x0343, imgsensor.line_length & 0xFF);
	write_cmos_sensor_8(0x0104, 0x00);
}

static void set_max_framerate(UINT16 framerate, kal_bool min_framelength_en)
{
	kal_uint32 frame_length = imgsensor.frame_length;

	LOG_DBG("framerate = %d, min framelength should enable %d\n",
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

static void set_shutter_frame_length(kal_uint32 shutter,
		kal_uint32 frame_length)
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
		else {
			/* Extend frame length*/
			write_cmos_sensor_8(0x0104, 0x01);
			write_cmos_sensor_8(0x0340, imgsensor.frame_length >> 8);
			write_cmos_sensor_8(0x0341, imgsensor.frame_length & 0xFF);
			write_cmos_sensor_8(0x0104, 0x00);
		}
	} else {
			/* Extend frame length*/
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
	LOG_DBG("Exit! shutter =%d, framelength =%d\n", shutter, imgsensor.frame_length);
}	/*	set_shutter_frame_length  */

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
		else {
			write_cmos_sensor_8(0x0104, 0x01);
			write_cmos_sensor_8(0x0340, imgsensor.frame_length >> 8);
			write_cmos_sensor_8(0x0341, imgsensor.frame_length & 0xFF);
			write_cmos_sensor_8(0x0104, 0x00);
		}

	} else  {
		/* Extend frame length*/
		write_cmos_sensor_8(0x0104, 0x01);
		write_cmos_sensor_8(0x0340, imgsensor.frame_length >> 8);
		write_cmos_sensor_8(0x0341, imgsensor.frame_length & 0xFF);
		write_cmos_sensor_8(0x0104, 0x00);

		LOG_DBG("(else)imgsensor.frame_length = %d\n",
			imgsensor.frame_length);
	}

	/* Update Shutter */
	write_cmos_sensor_8(0x0104, 0x01);
	write_cmos_sensor_8(0x0202, (shutter >> 8) & 0xFF);
	write_cmos_sensor_8(0x0203, shutter  & 0xFF);
	write_cmos_sensor_8(0x0104, 0x00);
	LOG_DBG("shutter =%d, framelength =%d\n",
		shutter, imgsensor.frame_length);
}	/*	write_shutter  */

static void set_shutter(kal_uint32 shutter)
{
	unsigned long flags;

	spin_lock_irqsave(&imgsensor_drv_lock, flags);
	imgsensor.shutter = shutter;
	spin_unlock_irqrestore(&imgsensor_drv_lock, flags);

	write_shutter(shutter);
} /* set_shutter */

static kal_uint16 gain2reg(const kal_uint16 gain)
{
	kal_uint16 reg_gain = 0x0;

	reg_gain = 1024 - (1024*64)/gain;
	return (kal_uint16) reg_gain;
}

static kal_uint16 set_gain(kal_uint16 gain)
{
	kal_uint16 reg_gain, max_gain = imgsensor_info.max_gain;

	if (gain < imgsensor_info.min_gain || gain > imgsensor_info.max_gain) {
		LOG_DBG("Error gain setting");

		if (gain < imgsensor_info.min_gain)
			gain = imgsensor_info.min_gain;
		else if (gain > imgsensor_info.max_gain)
			gain = imgsensor_info.max_gain;
	}

	reg_gain = gain2reg(gain);
	spin_lock(&imgsensor_drv_lock);
	imgsensor.gain = reg_gain;
	spin_unlock(&imgsensor_drv_lock);
	LOG_DBG("gain = %d, reg_gain = 0x%x, max_gain: 0x%x\n",
		gain, reg_gain, max_gain);

	write_cmos_sensor_8(0x0104, 0x01);
	write_cmos_sensor_8(0x0204, (reg_gain>>8) & 0xFF);
	write_cmos_sensor_8(0x0205, reg_gain & 0xFF);
	write_cmos_sensor_8(0x0104, 0x00);

	return gain;
}	/*	set_gain  */

static void night_mode(kal_bool enable)
{
/*No Need to implement this function*/
}	/*	night_mode	*/

static void set_mirror_flip(kal_uint8 image_mirror)
{
	kal_uint8 itemp;

	LOG_DBG("image_mirror = %d\n", image_mirror);
	itemp = read_cmos_sensor_8(0x0101);
	itemp &= ~0x03;

	switch (image_mirror) {
	case IMAGE_NORMAL:
		write_cmos_sensor_8(0x0101, itemp | 0x00);
		break;
	case IMAGE_H_MIRROR:
		write_cmos_sensor_8(0x0101, itemp | 0x01);
		break;
	case IMAGE_V_MIRROR:
		write_cmos_sensor_8(0x0101, itemp | 0x02);
		break;
	case IMAGE_HV_MIRROR:
		write_cmos_sensor_8(0x0101, itemp | 0x03);
		break;
	default:
		LOG_DBG("Error image_mirror setting\n");
	}
}

static int update_mipi_info(enum MSDK_SCENARIO_ID_ENUM scenario_id)
{
	LOG_DBG("E\n");

 	adaptive_mipi_index = imgsensor_select_mipi_by_rf_channel(imx355_mipi_channel_full,
 						ARRAY_SIZE(imx355_mipi_channel_full));
 	if (adaptive_mipi_index < 0) {
 		LOG_ERR("Not found mipi index");
 		return -EIO;
 	}

 	LOG_DBG("pclk(%d), mipi_pixel_rate(%d), framelength(%d) \n",
 		imgsensor_info.pre.pclk, imgsensor_info.pre.mipi_pixel_rate, imgsensor_info.pre.framelength);

 	if (adaptive_mipi_index == CAM_IMX355_SET_A_fulLA2I_357p5_MHZ) {
 		LOG_INF("Adaptive Mipi (357.5 MHz) : mipi_pixel_rate (275.60 Mpps)");
 		if (scenario_id == MSDK_SCENARIO_ID_CAMERA_PREVIEW) {
 			imgsensor_info.pre.pclk = 286000000;
 			imgsensor_info.pre.mipi_pixel_rate = 286000000;
 			imgsensor_info.pre.framelength = 2594;
 		} else if (scenario_id == MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG) {
 			imgsensor_info.cap.pclk = 286000000;
 			imgsensor_info.cap.mipi_pixel_rate = 286000000;
 			imgsensor_info.cap.framelength = 2594;
 		} else if (scenario_id == MSDK_SCENARIO_ID_VIDEO_PREVIEW) {
 			imgsensor_info.normal_video.pclk = 286000000;
 			imgsensor_info.normal_video.mipi_pixel_rate = 286000000;
 			imgsensor_info.normal_video.framelength = 2594;
 		} else {
 			LOG_ERR("Wrong Scenario id : %d", scenario_id);
 			return -EIO;
 		}
 	} else {//CAM_IMX355_SET_A_fulLA2I_351_MHZ
 		LOG_INF("Adaptive Mipi (351 MHz) : mipi_pixel_rate (280.80 Mpps)");
 		if (scenario_id == MSDK_SCENARIO_ID_CAMERA_PREVIEW) {
 			imgsensor_info.pre.pclk = 280800000;
 			imgsensor_info.pre.mipi_pixel_rate = 280800000;
 			imgsensor_info.pre.framelength = 2548;
 		} else if (scenario_id == MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG) {
 			imgsensor_info.cap.pclk = 280800000;
 			imgsensor_info.cap.mipi_pixel_rate = 280800000;
 			imgsensor_info.cap.framelength = 2548;
 		} else if (scenario_id == MSDK_SCENARIO_ID_VIDEO_PREVIEW) {
 			imgsensor_info.normal_video.pclk = 280800000;
 			imgsensor_info.normal_video.mipi_pixel_rate = 280800000;
 			imgsensor_info.normal_video.framelength = 2548;
 		} else {
 			LOG_ERR("Wrong Scenario id : %d", scenario_id);
 			return -EIO;
 		}
 	}

 	return adaptive_mipi_index;
}

static int set_mipi_mode(int mipi_index)
{
	LOG_DBG("E\n");

	if (mipi_index == CAM_IMX355_SET_A_fulLA2I_357p5_MHZ) {
		LOG_INF("Set adaptive Mipi as 357.5 MHz");
		table_write_cmos_sensor(imx355_mipi_full_357p5mhz,
			ARRAY_SIZE(imx355_mipi_full_357p5mhz));
	} else {//CAM_IMX355_SET_A_fulLA2I_351_MHZ is set as defualt, so no need to set.
		LOG_INF("Set adaptive Mipi as 351 MHz");
	}

	return ERROR_NONE;
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
	kal_uint16 frameCnt = 0;
	int timeout = (10000 / imgsensor.current_fps) + 1;

	if (onoff)
		mDELAY(2);

	for (i = 0; i < timeout; i++) {
		frameCnt = read_cmos_sensor_8(0x0005);
		if (onoff) {
			if (frameCnt == 0xFF)
				mDELAY(check_delay);
			else
				break;
		} else {
			if (frameCnt != 0xFF)
				mDELAY(check_delay);
			else
				break;
		}
	}

	if (i >= timeout)
		LOG_ERR("[onoff:%d] timeout, wait_time:%d , frameCnt:0x%x\n", onoff, onoff ? i*check_delay + 2 : i*check_delay, frameCnt);
	else
		LOG_INF("[onoff:%d], wait_time:%d, frameCnt:0x%x\n", onoff, onoff ? i*check_delay + 2 : i*check_delay, frameCnt);
}

static kal_uint32 streaming_control(kal_bool enable)
{
	LOG_DBG("streaming_enable(0=Sw Standby,1=streaming): %d\n", enable);
	if (enable) {
		if (enable_adaptive_mipi && imgsensor.current_scenario_id != MSDK_SCENARIO_ID_CUSTOM1) {
			set_mipi_mode(adaptive_mipi_index);
		}
		write_cmos_sensor_8(0x0100, 0X01);
	} else
		write_cmos_sensor_8(0x0100, 0x00);
	wait_stream_onoff(enable);
	return ERROR_NONE;
}

static void set_mode_setfile(enum IMGSENSOR_MODE mode)
{
	if (mode >= IMGSENSOR_MODE_MAX) {
		LOG_ERR("invalid mode: %d\n", mode);
		return;
	}

	LOG_INF("mode: %s\n", imx355_setfile_info[mode].name);

	table_write_cmos_sensor(imx355_setfile_info[mode].setfile, imx355_setfile_info[mode].size);
}

static void sensor_init(void)
{
	LOG_DBG("E\n");

	set_mode_setfile(IMGSENSOR_MODE_INIT);
}	/*	sensor_init  */

static kal_uint32 get_imgsensor_id(UINT32 *sensor_id)
{
	kal_uint8 i = 0;
	kal_uint8 retry = 2;

	LOG_DBG("E\n");

	while (imgsensor_info.i2c_addr_table[i] != 0xff) {
		spin_lock(&imgsensor_drv_lock);
		imgsensor.i2c_write_id = imgsensor_info.i2c_addr_table[i];
		spin_unlock(&imgsensor_drv_lock);

		do {
			*sensor_id = ((read_cmos_sensor_8(0x0016) << 8) | read_cmos_sensor_8(0x0017));
			if (*sensor_id == imgsensor_info.sensor_id) {
				LOG_INF("i2c write id: 0x%x, sensor id: 0x%x\n", imgsensor.i2c_write_id, *sensor_id);

				return ERROR_NONE;
			}
			LOG_ERR("Read sensor id fail, id: 0x%x\n", imgsensor.i2c_write_id);
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


static kal_uint32 open(void)
{
	kal_uint8 i = 0;
	kal_uint8 retry = 2;
	kal_uint16 sensor_id = 0;

	LOG_DBG("E\n");

	while (imgsensor_info.i2c_addr_table[i] != 0xff) {
		spin_lock(&imgsensor_drv_lock);
		imgsensor.i2c_write_id = imgsensor_info.i2c_addr_table[i];
		spin_unlock(&imgsensor_drv_lock);

		do {
			sensor_id = ((read_cmos_sensor_8(0x0016) << 8) | read_cmos_sensor_8(0x0017));

			if (sensor_id == imgsensor_info.sensor_id) {
				LOG_DBG("open():i2c write id: 0x%x, sensor id: 0x%x\n", imgsensor.i2c_write_id, sensor_id);
				break;
			}

			LOG_DBG("Read sensor id fail, id: 0x%x\n", imgsensor.i2c_write_id);
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
	imgsensor.autoflicker_en		= KAL_FALSE;
	imgsensor.sensor_mode		= IMGSENSOR_MODE_INIT;
	imgsensor.shutter				= 0x3D0;
	imgsensor.gain				= 0x100;
	imgsensor.pclk				= imgsensor_info.pre.pclk;
	imgsensor.frame_length			= imgsensor_info.pre.framelength;
	imgsensor.line_length			= imgsensor_info.pre.linelength;
	imgsensor.min_frame_length		= imgsensor_info.pre.framelength;
	imgsensor.dummy_pixel			= 0;
	imgsensor.dummy_line			= 0;
	imgsensor.ihdr_mode			= 0;
	imgsensor.test_pattern			= KAL_FALSE;
	imgsensor.current_fps			= imgsensor_info.pre.max_framerate;
	imgsensor.frame_length_shifter	= 0;
	spin_unlock(&imgsensor_drv_lock);

	LOG_DBG("X\n");

	return ERROR_NONE;
}	/*	open  */

static kal_uint32 close(void)
{
	LOG_DBG("E\n");
	streaming_control(false);
	enable_adaptive_mipi = false;

	return ERROR_NONE;
}	/*	close  */

static kal_uint32 preview(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
		MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_DBG("E\n");

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode		= IMGSENSOR_MODE_PREVIEW;
	imgsensor.pclk				= imgsensor_info.pre.pclk;
	imgsensor.line_length			= imgsensor_info.pre.linelength;
	imgsensor.frame_length			= imgsensor_info.pre.framelength;
	imgsensor.min_frame_length		= imgsensor_info.pre.framelength;
	imgsensor.autoflicker_en		= KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);

	set_mode_setfile(imgsensor.sensor_mode);

	set_mirror_flip(imgsensor.mirror);

	return ERROR_NONE;
}

static kal_uint32 capture(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
		MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_DBG("E\n");

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode		= IMGSENSOR_MODE_CAPTURE;
	imgsensor.pclk				= imgsensor_info.cap.pclk;
	imgsensor.line_length			= imgsensor_info.cap.linelength;
	imgsensor.frame_length			= imgsensor_info.cap.framelength;
	imgsensor.min_frame_length		= imgsensor_info.cap.framelength;
	imgsensor.autoflicker_en		= KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);

	set_mode_setfile(imgsensor.sensor_mode);

	set_mirror_flip(imgsensor.mirror);

	return ERROR_NONE;
}	/* capture() */


static kal_uint32 normal_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
		  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_DBG("E\n");

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode		= IMGSENSOR_MODE_VIDEO;
	imgsensor.pclk				= imgsensor_info.normal_video.pclk;
	imgsensor.line_length			= imgsensor_info.normal_video.linelength;
	imgsensor.frame_length			= imgsensor_info.normal_video.framelength;
	imgsensor.min_frame_length		= imgsensor_info.normal_video.framelength;
	imgsensor.autoflicker_en		= KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);

	set_mode_setfile(imgsensor.sensor_mode);

	set_mirror_flip(imgsensor.mirror);

	return ERROR_NONE;
}	/*	normal_video   */

static kal_uint32 hs_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
		MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_DBG("E\n");

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_HIGH_SPEED_VIDEO;
	imgsensor.pclk = imgsensor_info.hs_video.pclk;
	imgsensor.line_length = imgsensor_info.hs_video.linelength;
	imgsensor.frame_length = imgsensor_info.hs_video.framelength;
	imgsensor.min_frame_length = imgsensor_info.hs_video.framelength;
	imgsensor.dummy_line = 0;
	imgsensor.dummy_pixel = 0;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	//hs_video_setting();
	return ERROR_NONE;
}	/*	hs_video	*/

static kal_uint32 slim_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
		MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_DBG("E\n");
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
	//slim_video_setting();
	return ERROR_NONE;
}	/*	slim_video	*/

static kal_uint32 custom1(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
					  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_DBG("E cur fps: %d\n", imgsensor.current_fps);

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode		= IMGSENSOR_MODE_CUSTOM1;
	imgsensor.pclk				= imgsensor_info.custom1.pclk;
	imgsensor.line_length			= imgsensor_info.custom1.linelength;
	imgsensor.frame_length			= imgsensor_info.custom1.framelength;
	imgsensor.min_frame_length		= imgsensor_info.custom1.framelength;
	imgsensor.autoflicker_en		= KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);

	set_mode_setfile(imgsensor.sensor_mode);

	set_mirror_flip(IMAGE_NORMAL);

	return ERROR_NONE;
}	/* custom1() */

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

	sensor_resolution->SensorCustom1Width =
		imgsensor_info.custom1.grabwindow_width;
	sensor_resolution->SensorCustom1Height =
		imgsensor_info.custom1.grabwindow_height;

	return ERROR_NONE;
}    /*    get_resolution    */

static kal_uint32 get_info(enum MSDK_SCENARIO_ID_ENUM scenario_id,
				  MSDK_SENSOR_INFO_STRUCT *sensor_info,
				  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_DBG("scenario_id = %d\n", scenario_id);

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
	sensor_info->VideoDelayFrame = imgsensor_info.video_delay_frame;
	sensor_info->HighSpeedVideoDelayFrame = imgsensor_info.hs_video_delay_frame;
	sensor_info->SlimVideoDelayFrame = imgsensor_info.slim_video_delay_frame;
	sensor_info->Custom1DelayFrame = imgsensor_info.custom1_delay_frame;

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
	default:
		sensor_info->SensorGrabStartX = imgsensor_info.pre.startx;
		sensor_info->SensorGrabStartY = imgsensor_info.pre.starty;

		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
			imgsensor_info.pre.mipi_data_lp2hs_settle_dc;
		break;
	}

	return ERROR_NONE;
}	/*	get_info  */


static kal_uint32 set_video_mode(UINT16 framerate)
{
	LOG_DBG("framerate = %d\n ", framerate);

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

static kal_uint32 set_auto_flicker_mode(kal_bool enable, UINT16 framerate)
{
	LOG_DBG("enable = %d, framerate = %d\n", enable, framerate);

	spin_lock(&imgsensor_drv_lock);
	if (enable)
		imgsensor.autoflicker_en = KAL_TRUE;
	else
		imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);

	return ERROR_NONE;
}

static kal_uint32 set_max_framerate_by_scenario(enum MSDK_SCENARIO_ID_ENUM scenario_id,	MUINT32 framerate)
{
	kal_uint32 frame_length;

	LOG_DBG("scenario_id = %d, framerate = %d\n", scenario_id, framerate);

	switch (scenario_id) {
	case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		LOG_DBG("set_max_framerate_by_scenario():MSDK_SCENARIO_ID_CAMERA_PREVIEW scenario_id= %d\n", scenario_id);
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
		LOG_DBG("set_max_framerate_by_scenario():MSDK_SCENARIO_ID_VIDEO_PREVIEW scenario_id= %d\n", scenario_id);
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
		LOG_DBG("set_max_framerate_by_scenario():MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG scenario_id= %d\n", scenario_id);

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
		frame_length = imgsensor_info.hs_video.pclk / framerate * 10 / imgsensor_info.hs_video.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line = (frame_length > imgsensor_info.hs_video.framelength)
			? (frame_length - imgsensor_info.hs_video.framelength) : 0;
		imgsensor.frame_length = imgsensor_info.hs_video.framelength + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter)
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
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
		break;

	case MSDK_SCENARIO_ID_CUSTOM1:
		LOG_DBG("set_max_framerate_by_scenario():MSDK_SCENARIO_ID_CUSTOM1 scenario_id= %d\n", scenario_id);
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

	default:
		frame_length = imgsensor_info.pre.pclk / framerate * 10
		/ imgsensor_info.pre.linelength;

		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line = (frame_length > imgsensor_info.pre.framelength)
			? (frame_length - imgsensor_info.pre.framelength) : 0;

		imgsensor.frame_length = imgsensor_info.pre.framelength + imgsensor.dummy_line;

		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);

		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();

		LOG_DBG("error scenario_id = %d, we use preview scenario\n", scenario_id);
		break;
	}
	return ERROR_NONE;
}

static kal_uint32 get_default_framerate_by_scenario(enum MSDK_SCENARIO_ID_ENUM scenario_id, MUINT32 *framerate)
{
	LOG_DBG("scenario_id = %d\n", scenario_id);

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
		LOG_DBG("custom1 *framerate = %d\n", *framerate);
		break;
	default:
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
		LOG_DBG("MSDK_SCENARIO_ID_CAMERA_PREVIEW scenario_id= %d\n", scenario_id);
		preview(image_window, sensor_config_data);
		break;
	case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		LOG_DBG("MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG scenario_id= %d\n", scenario_id);
		capture(image_window, sensor_config_data);
		break;
	case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
		LOG_DBG("MSDK_SCENARIO_ID_VIDEO_PREVIEW scenario_id= %d\n", scenario_id);
		normal_video(image_window, sensor_config_data);
		break;
	case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
		hs_video(image_window, sensor_config_data);
		break;
	case MSDK_SCENARIO_ID_SLIM_VIDEO:
		slim_video(image_window, sensor_config_data);
		break;
	case MSDK_SCENARIO_ID_CUSTOM1:
		LOG_DBG("MSDK_SCENARIO_ID_CUSTOM1 scenario_id= %d\n", scenario_id);
		custom1(image_window, sensor_config_data);
		break;
	default:
		LOG_DBG("Error ScenarioId setting");
		preview(image_window, sensor_config_data);
		return ERROR_INVALID_SCENARIO_ID;
	}
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

	LOG_DBG("feature_id = %d\n", feature_id);
	switch (feature_id) {
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
	case SENSOR_FEATURE_GET_MIPI_PIXEL_RATE:
		{
			kal_uint32 rate;

			if (enable_adaptive_mipi) {
				update_mipi_info(*feature_data);
			}

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
		LOG_DBG("SENSOR_FEATURE_SET_TEST_PATTERN not implemented yet = %d\n", feature_id);
		break;
	case SENSOR_FEATURE_GET_TEST_PATTERN_CHECKSUM_VALUE:
		*feature_return_para_32 = imgsensor_info.checksum_value;
		*feature_para_len = 4;
		break;
	case SENSOR_FEATURE_SET_FRAMERATE:
		LOG_DBG("current fps :%d\n", (UINT32)*feature_data);
		spin_lock(&imgsensor_drv_lock);
		imgsensor.current_fps = *feature_data;
		spin_unlock(&imgsensor_drv_lock);
		break;
	case SENSOR_FEATURE_SET_HDR:
		LOG_DBG("SENSOR_FEATURE_SET_HDR = %d\n", feature_id);
		LOG_DBG("ihdr enable :%d\n", *feature_data_32);
		spin_lock(&imgsensor_drv_lock);
		imgsensor.ihdr_mode = (UINT8)*feature_data_32;
		spin_unlock(&imgsensor_drv_lock);
		break;
	case SENSOR_FEATURE_SET_SHUTTER_FRAME_TIME:/*lzl*/
		set_shutter_frame_length((UINT32)*feature_data,
				(UINT32)*(feature_data+1));
		break;
	case SENSOR_FEATURE_GET_CROP_INFO:
		LOG_DBG("SENSOR_FEATURE_GET_CROP_INFO scenarioId:%d\n",
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
	case SENSOR_FEATURE_SET_IHDR_SHUTTER_GAIN:
		LOG_DBG("SENSOR_SET_SENSOR_IHDR LE=%d, SE=%d, Gain=%d\n",
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
	case SENSOR_FEATURE_SET_ADAPTIVE_MIPI:
		if (*feature_para == 1)
	 		enable_adaptive_mipi = true;
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

UINT32 IMX355_MIPI_RAW_SensorInit(struct SENSOR_FUNCTION_STRUCT **pfFunc)
{
	/* To Do : Check Sensor status here */
	if (pfFunc != NULL)
		*pfFunc = &sensor_func;
	return ERROR_NONE;
}	/*	IMX355_MIPI_RAW_SensorInit	*/
