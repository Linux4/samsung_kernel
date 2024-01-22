/*
 * Copyright (C) 2022 MediaTek Inc.
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
 *	 gc08a3mipi_Sensor.c
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
#define PFX "GC08A3 D/D"
#define pr_fmt(fmt) PFX "[%s] " fmt, __func__
/****************************   Modify end    *********************************/

#include <linux/videodev2.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/atomic.h>

#include "kd_imgsensor.h"
#include "kd_imgsensor_define.h"
#include "kd_imgsensor_errcode.h"
#include "kd_camera_typedef.h"

#include "imgsensor_sysfs.h"
#include "gc08a3mipiraw_Sensor.h"
#include "gc08a3mipiraw_setfile.h"

static bool enable_adaptive_mipi;
static int adaptive_mipi_index;

#define LOG_DBG(format, args...) pr_debug(format, ##args)
#define LOG_INF(format, args...) pr_info(format, ##args)
#define LOG_ERR(format, args...) pr_err(format, ##args)

static DEFINE_SPINLOCK(imgsensor_drv_lock);

#define MULTI_WRITE 1
#if MULTI_WRITE
#define I2C_BUFFER_LEN 765 /* trans# max is 255, each 3 bytes */
#else
#define I2C_BUFFER_LEN 3
#endif

#define STREAM_OFF_POLL_TIME_MS 500

/*
 * Image Sensor Scenario
 * pre : FOV 80 4:3
 * cap : FOV 80 Check Active Array
 * normal_video : FOV 80 16:9
 * hs_video: NOT USED
 * slim_video: NOT USED
 * custom1 : FAST AE
 * custom2 : FOV 68 4:3
 * custom3 : FOV 68 Check Active Array
 * custom4 : FOV 68 16:9
 */

static struct imgsensor_info_struct imgsensor_info = {
	.sensor_id = GC08A3_SENSOR_ID,

	.checksum_value = 0x31e3fbe2,
	.pre = {
		.pclk = 280800000,
		.linelength = 3672,
		.framelength = 2548,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 3264,
		.grabwindow_height = 2448,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 267800000,
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
		.mipi_pixel_rate = 267800000,
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
		.mipi_pixel_rate = 267800000,
		.max_framerate = 300,
	},
	.hs_video = { // NOT USED
		.pclk = 280800000,
		.linelength = 3672,
		.framelength = 2548,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 736,
		.grabwindow_height = 552,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 267800000,
		.max_framerate = 1200,
	},
	.slim_video = { // NOT USED
		.pclk = 280800000,
		.linelength = 3672,
		.framelength = 2548,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 1632,
		.grabwindow_height = 1224,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 267800000,
		.max_framerate = 300,
	},
	.custom1 = {
		.pclk = 280800000,
		.linelength = 3672,
		.framelength = 1272,// 636x2 for 60 fps
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 736,
		.grabwindow_height = 552,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 111800000,
		.max_framerate = 600,
	},
	.custom2 = {
		.pclk = 280800000,
		.linelength = 3672,
		.framelength = 2548,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 2880,
		.grabwindow_height = 1980,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 267800000,
		.max_framerate = 300,
	},
	.custom3 = {
		.pclk = 280800000,
		.linelength = 3672,
		.framelength = 2548,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 2880,
		.grabwindow_height = 1980,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 267800000,
		.max_framerate = 300,
	},
	.custom4 = {
		.pclk = 280800000,
		.linelength = 3672,
		.framelength = 2548,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 2880,
		.grabwindow_height = 1980,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 267800000,
		.max_framerate = 300,
	},

	.margin = 16,
	.min_shutter = 4,
	.min_gain = 64, /* 1x gain */
	.max_gain = 1024, /* 16x gain */
	.min_gain_iso = 100,
	.exp_step = 2,
	.gain_step = 1,
	.gain_type = 4,
	.max_frame_length = 0x3fff,
	.ae_shut_delay_frame = 0,
	.ae_sensor_gain_delay_frame = 0,
	.ae_ispGain_delay_frame = 2,
	.ihdr_support = 0,	//1, support; 0,not support
	.ihdr_le_firstline = 0,	//1,le first ; 0, se first
	.sensor_mode_num = 9,	//support sensor mode num

	.cap_delay_frame = 3,
	.pre_delay_frame = 3,
	.video_delay_frame = 3,
	.hs_video_delay_frame = 3,
	.slim_video_delay_frame = 3,
	.custom1_delay_frame = 3,
	.custom2_delay_frame = 3,
	.custom3_delay_frame = 3,
	.custom4_delay_frame = 3,
	.frame_time_delay_frame = 2,

	.isp_driving_current = ISP_DRIVING_4MA,
	.sensor_interface_type = SENSOR_INTERFACE_TYPE_MIPI,
	//0,MIPI_OPHY_NCSI2;  1,MIPI_OPHY_CSI2
	.mipi_sensor_type = MIPI_OPHY_NCSI2,
	//0,MIPI_SETTLEDELAY_AUTO; 1,MIPI_SETTLEDELAY_MANNUAL
	.mipi_settle_delay_mode = 0,
	.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_R,
	.mclk = 26,
	.mipi_lane_num = SENSOR_MIPI_4_LANE,
	.i2c_addr_table = {0x62, 0xff},
	.i2c_speed = 400,
};


static struct imgsensor_struct imgsensor = {
	.mirror = IMAGE_NORMAL,	//mirrorflip information
	// IMGSENSOR_MODE enum value,record current sensor mode,such as:
	// INIT, Preview, Capture, Video,High Speed Video, Slim Video
	.sensor_mode = IMGSENSOR_MODE_INIT,
	.shutter = 0x3D0,	// current shutter
	.gain = 0x100,		// current gain
	.dummy_pixel = 0,	// current dummypixel
	.dummy_line = 0,	// current dummyline
	// full size current fps : 24fps for PIP, 30fps for Normal or ZSD
	.current_fps = 300,
	// auto flicker enable: KAL_FALSE for disable auto flicker,
	// KAL_TRUE for enable auto flicker
	.autoflicker_en = KAL_FALSE,
	// test pattern mode or not. KAL_TRUE for in test pattern mode,
	// KAL_FALSE for normal output
	.test_pattern = KAL_FALSE,
	.enable_secure = KAL_FALSE,
	//current scenario id
	.current_scenario_id = MSDK_SCENARIO_ID_CAMERA_PREVIEW,
	.ihdr_mode = 0,		// sensor need support LE, SE with HDR feature
	.i2c_write_id = 0x62,
};


/* Sensor output window information */
static struct SENSOR_WINSIZE_INFO_STRUCT imgsensor_winsize_info[9] = {
	{ 3280, 2464,    8,    8, 3264, 2448, 3264, 2448,
	  0000, 0000, 3264, 2448, 0000, 0000, 3264, 2448 }, // preview
	{ 3280, 2464,    8,    8, 3264, 2448, 3264, 2448,
	  0000, 0000, 3264, 2448, 0000, 0000, 3264, 2448 }, // capture
	{ 3280, 2464,    8,  314, 3264, 1836, 3264, 1836,
	  0000, 0000, 3264, 1836, 0000, 0000, 3264, 1836 }, // video
	{ 3280, 2464,  168,  128, 2944, 2208,  736,  552,
	  0000, 0000,  736,  552, 0000, 0000,  736,  552 }, // high speed video - NOT USED
	{ 3280, 2464,    8,    8, 3264, 2448, 1632, 1224,
	  0000, 0000, 1632, 1224, 0000, 0000, 1632, 1224 }, // slim video - NOT USED
	{ 3280, 2464,  168,  128, 2944, 2208,  736,  552,
	  0000, 0000,  736,  552, 0000, 0000,  736,  552 },  // custom1
	{ 3280, 2464,  200,  242, 2880, 1980, 2880, 1980,
	  0000, 0000, 2880, 1980, 0000, 0000, 2880, 1980 }, // custom2
	{ 3280, 2464,  200,  242, 2880, 1980, 2880, 1980,
	  0000, 0000, 2880, 1980, 0000, 0000, 2880, 1980 }, // custom3
	{ 3280, 2464,  200,  242, 2880, 1980, 2880, 1980,
	  0000, 0000, 2880, 1980, 0000, 0000, 2880, 1980 }  // custom4
};


static kal_uint16 read_cmos_sensor(kal_uint32 addr)
{
	kal_uint16 get_byte = 0;
	char pusendcmd[2] = { (char)(addr >> 8), (char)(addr & 0xFF) };

	iReadRegI2C(pusendcmd, 2, (u8 *) &get_byte, 2, imgsensor.i2c_write_id);
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
	char pusendcmd[2] = { (char)(addr >> 8), (char)(addr & 0xFF) };

	iReadRegI2C(pusendcmd, 2, (u8 *) &get_byte, 1, imgsensor.i2c_write_id);
	return get_byte;
}

static void write_cmos_sensor_8(kal_uint16 addr, kal_uint8 para)
{
	char pusendcmd[3] = {
		(char)(addr >> 8), (char)(addr & 0xFF), (char)(para & 0xFF)
	};

	iWriteRegI2C(pusendcmd, 3, imgsensor.i2c_write_id);
}

static kal_uint16 table_write_cmos_sensor(kal_uint16 *para,
					  kal_uint32 len)
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
		/* Write when remain buffer size is less than 3 bytes
		 * or reach end of data
		 */
		if ((I2C_BUFFER_LEN - tosend) < 3
			|| IDX == len || addr != addr_last) {
			iBurstWriteReg_multi(puSendCmd,
						tosend,
						imgsensor.i2c_write_id,
						3,
						imgsensor_info.i2c_speed);
			tosend = 0;
		}
#else
		iWriteRegI2C(puSendCmd, 3, imgsensor.i2c_write_id);
		tosend = 0;
#endif
	}

	return 0;
}

static void set_dummy(void)
{
	LOG_DBG("dummyline = %d, dummypixels = %d", imgsensor.dummy_line, imgsensor.dummy_pixel);

	write_cmos_sensor(0x0340, imgsensor.frame_length);
	write_cmos_sensor(0x0342, imgsensor.line_length);
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

static void set_max_framerate(UINT16 framerate, kal_bool min_framelength_en)
{
	/*kal_int16 dummy_line;*/
	kal_uint32 frame_length = imgsensor.frame_length;

	LOG_DBG("framerate = %d, min framelength should enable %d\n", framerate, min_framelength_en);

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
} /*      set_max_framerate  */

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

	if (shutter < imgsensor_info.min_shutter)
		shutter = imgsensor_info.min_shutter;

	if (imgsensor.autoflicker_en == KAL_TRUE) {
		realtime_fps = imgsensor.pclk / imgsensor.line_length * 10
			/ imgsensor.frame_length;
		if (realtime_fps >= 297 && realtime_fps <= 305) {
			set_max_framerate(296, 0);
			//set_dummy();
		} else if (realtime_fps >= 147 && realtime_fps <= 150) {
			set_max_framerate(146, 0);
			//set_dummy();
		} else {
			write_cmos_sensor(0x0340, imgsensor.frame_length);
		}
	} else {
		// Extend frame length
		write_cmos_sensor(0x0340, imgsensor.frame_length);
	}

	// Update Shutter
	write_cmos_sensor(0x0340, imgsensor.frame_length);
	write_cmos_sensor(0x0202, shutter);
	LOG_DBG("realtime_fps =%d", realtime_fps);
	LOG_DBG("shutter =%d, framelength =%d", shutter, imgsensor.frame_length);
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

	//x1: sensor->0x400, HAL->0x40
	reg_gain = gain * 16;
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

	if (gain < imgsensor_info.min_gain || gain > imgsensor_info.max_gain) {
		LOG_INF("Error gain setting");

		if (gain < imgsensor_info.min_gain)
			gain = imgsensor_info.min_gain;
		else if (gain > imgsensor_info.max_gain)
			gain = imgsensor_info.max_gain;
	}

	reg_gain = gain2reg(gain);
	spin_lock(&imgsensor_drv_lock);
	imgsensor.gain = reg_gain;
	spin_unlock(&imgsensor_drv_lock);
	LOG_DBG("gain = %d , reg_gain = %#06x", gain, reg_gain);

	write_cmos_sensor(0x0204, reg_gain);

	return gain;
}

static int update_mipi_info(enum MSDK_SCENARIO_ID_ENUM scenario_id)
{
	LOG_DBG("- E\n");

	adaptive_mipi_index = imgsensor_select_mipi_by_rf_channel(gc08a3_mipi_channel_full,
						ARRAY_SIZE(gc08a3_mipi_channel_full));
	if (adaptive_mipi_index < 0) {
		LOG_ERR("Not found mipi index");
		return -EIO;
	}

	LOG_DBG("pclk(%d), mipi_pixel_rate(%d), framelength(%d)\n",
		imgsensor_info.pre.pclk, imgsensor_info.pre.mipi_pixel_rate, imgsensor_info.pre.framelength);

	if (adaptive_mipi_index == CAM_GC08A3_SET_A_Full_334p75_MHZ) {
		LOG_INF("Adaptive Mipi : 334.75 Mhz (669.5Mbps), mipi pixel rate = 267.8Mbps");
		if (scenario_id == MSDK_SCENARIO_ID_CAMERA_PREVIEW) {
			imgsensor_info.pre.mipi_pixel_rate = 267800000;
		} else if (scenario_id == MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG) {
			imgsensor_info.cap.mipi_pixel_rate = 267800000;
		} else if (scenario_id == MSDK_SCENARIO_ID_VIDEO_PREVIEW) {
			imgsensor_info.normal_video.mipi_pixel_rate = 267800000;
		} else if (scenario_id == MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO) {
			imgsensor_info.hs_video.mipi_pixel_rate = 267800000;
		} else {
			LOG_ERR("Wrong Scenario id : %d", scenario_id);
			return -EIO;
		}
	} else if (adaptive_mipi_index == CAM_GC08A3_SET_A_Full_341p25_MHZ) {
		LOG_INF("Adaptive Mipi : 341.25 Mhz (682.5Mbps), mipi pixel rate = 273Mbps");
		if (scenario_id == MSDK_SCENARIO_ID_CAMERA_PREVIEW) {
			imgsensor_info.pre.mipi_pixel_rate = 273000000;
		} else if (scenario_id == MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG) {
			imgsensor_info.cap.mipi_pixel_rate = 273000000;
		} else if (scenario_id == MSDK_SCENARIO_ID_VIDEO_PREVIEW) {
			imgsensor_info.normal_video.mipi_pixel_rate = 273000000;
		} else if (scenario_id == MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO) {
			imgsensor_info.hs_video.mipi_pixel_rate = 273000000;
		} else {
			LOG_ERR("Wrong Scenario id : %d", scenario_id);
			return -EIO;
		}
	} else {//CAM_GC08A3_SET_A_Full_344p5_MHZ
		LOG_INF("Adaptive Mipi : 344.5 Mhz (689Mbps), mipi pixel rate = 275.6Mbps");
		if (scenario_id == MSDK_SCENARIO_ID_CAMERA_PREVIEW) {
			imgsensor_info.pre.mipi_pixel_rate = 275600000;
		} else if (scenario_id == MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG) {
			imgsensor_info.cap.mipi_pixel_rate = 275600000;
		} else if (scenario_id == MSDK_SCENARIO_ID_VIDEO_PREVIEW) {
			imgsensor_info.normal_video.mipi_pixel_rate = 275600000;
		} else if (scenario_id == MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO) {
			imgsensor_info.hs_video.mipi_pixel_rate = 275600000;
		} else {
			LOG_ERR("Wrong Scenario id : %d", scenario_id);
			return -EIO;
		}
	}

	return adaptive_mipi_index;
}

static int set_mipi_mode(int mipi_index)
{
	LOG_DBG("- E");
	if (mipi_index == CAM_GC08A3_SET_A_Full_341p25_MHZ) {
		LOG_INF("Set adaptive Mipi as 341.25 MHz");
		table_write_cmos_sensor(gc08a3_mipi_full_341p25mhz,
			ARRAY_SIZE(gc08a3_mipi_full_341p25mhz));
	} else if (mipi_index == CAM_GC08A3_SET_A_Full_344p5_MHZ) {
		LOG_INF("Set adaptive Mipi as 344.5 MHz");
		table_write_cmos_sensor(gc08a3_mipi_full_344p5mhz,
			ARRAY_SIZE(gc08a3_mipi_full_344p5mhz));
	} else {//CAM_GC08A3_SET_A_Full_334p75_MHZ is set as default, so no need to set.
		LOG_INF("Set adaptive Mipi as 334.75 MHz");
	}
	LOG_DBG("- X");
	return ERROR_NONE;
}

static void wait_stream_off(void)
{
	unsigned int frame_cnt_msb = 0x00;
	unsigned int frame_cnt_lsb = 0x00;
	unsigned long poll_time_ms = 0;
	unsigned long wait_delay_ms = 1;

	do {
		frame_cnt_msb = read_cmos_sensor_8(0x0146);
		frame_cnt_lsb = read_cmos_sensor_8(0x0147);

		/* frame count == 0 when stream off */
		if (frame_cnt_msb == 0x00 && frame_cnt_lsb == 0x00)
			break;

		mDELAY(wait_delay_ms);
		poll_time_ms++;
	} while (poll_time_ms < STREAM_OFF_POLL_TIME_MS);

	if (poll_time_ms < STREAM_OFF_POLL_TIME_MS)
		LOG_INF("wait time : %d ms\n", poll_time_ms);
	else
		LOG_ERR("Polling timeout : %d ms\n", poll_time_ms);
}

static kal_uint32 streaming_control(kal_bool enable)
{
	LOG_INF("streaming_enable(0=Sw Standby,1=streaming): %d", enable);

	if (enable) {
		LOG_INF("stream[ON]");
		if (enable_adaptive_mipi && imgsensor.current_scenario_id != MSDK_SCENARIO_ID_CUSTOM1)
			set_mipi_mode(adaptive_mipi_index);
		write_cmos_sensor(0x0100, 0x0100);
	} else {
		LOG_INF("stream[OFF]");
		write_cmos_sensor(0x0100, 0x0000);
		wait_stream_off();
	}
	return ERROR_NONE;
}

static void set_mode_setfile(enum IMGSENSOR_MODE mode)
{
	if (mode >= IMGSENSOR_MODE_MAX) {
		LOG_ERR("invalid mode: %d", mode);
		return;
	}
	LOG_INF(" - E");
	LOG_INF("mode: %s", gc08a3_setfile_info[mode].name);

	if ((gc08a3_setfile_info[mode].setfile == NULL) || (gc08a3_setfile_info[mode].size == 0))
		LOG_ERR("failed, mode: %d", mode);
	else
		table_write_cmos_sensor(gc08a3_setfile_info[mode].setfile, gc08a3_setfile_info[mode].size);

	LOG_INF(" - X");
}

static void sensor_init(void)
{
	LOG_INF(" - E");
	set_mode_setfile(IMGSENSOR_MODE_INIT);
	LOG_INF(" - X");
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
			*sensor_id = ((read_cmos_sensor_8(0x03f0) << 8) | read_cmos_sensor_8(0x03f1));
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
			sensor_id = ((read_cmos_sensor_8(0x03f0) << 8) | read_cmos_sensor_8(0x03f1));
			if (sensor_id == imgsensor_info.sensor_id) {
				LOG_INF("i2c write id: 0x%x, sensor id: 0x%x\n",
						imgsensor.i2c_write_id, sensor_id);
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

	spin_lock(&imgsensor_drv_lock);

	imgsensor.autoflicker_en    = KAL_FALSE;
	imgsensor.sensor_mode       = IMGSENSOR_MODE_INIT;
	imgsensor.shutter           = 0x3D0;
	imgsensor.gain              = 0x100;
	imgsensor.pclk              = imgsensor_info.pre.pclk;
	imgsensor.frame_length      = imgsensor_info.pre.framelength;
	imgsensor.line_length       = imgsensor_info.pre.linelength;
	imgsensor.min_frame_length  = imgsensor_info.pre.framelength;
	imgsensor.dummy_pixel       = 0;
	imgsensor.dummy_line        = 0;
	imgsensor.ihdr_mode         = 0;
	imgsensor.test_pattern      = KAL_FALSE;
	imgsensor.current_fps       = imgsensor_info.pre.max_framerate;
	spin_unlock(&imgsensor_drv_lock);

	return ERROR_NONE;
}		/* open */

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

	streaming_control(false);
	enable_adaptive_mipi = false;

	return ERROR_NONE;
}		/* close */

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
	imgsensor.sensor_mode           = IMGSENSOR_MODE_PREVIEW;
	imgsensor.pclk                  = imgsensor_info.pre.pclk;
	imgsensor.line_length           = imgsensor_info.pre.linelength;
	imgsensor.frame_length          = imgsensor_info.pre.framelength;
	imgsensor.min_frame_length      = imgsensor_info.pre.framelength;
	imgsensor.autoflicker_en        = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);

	set_mode_setfile(imgsensor.sensor_mode);

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
	LOG_INF("E");

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode           = IMGSENSOR_MODE_CAPTURE;
	imgsensor.pclk                  = imgsensor_info.cap.pclk;
	imgsensor.line_length           = imgsensor_info.cap.linelength;
	imgsensor.frame_length          = imgsensor_info.cap.framelength;
	imgsensor.min_frame_length      = imgsensor_info.cap.framelength;
	imgsensor.autoflicker_en        = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);

	set_mode_setfile(imgsensor.sensor_mode);

	set_mirror_flip(imgsensor.mirror);

	return ERROR_NONE;
}

static kal_uint32 normal_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			       MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("E");

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode           = IMGSENSOR_MODE_VIDEO;
	imgsensor.pclk                  = imgsensor_info.normal_video.pclk;
	imgsensor.line_length           = imgsensor_info.normal_video.linelength;
	imgsensor.frame_length          = imgsensor_info.normal_video.framelength;
	imgsensor.min_frame_length      = imgsensor_info.normal_video.framelength;
	imgsensor.autoflicker_en        = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);

	set_mode_setfile(imgsensor.sensor_mode);

	set_mirror_flip(imgsensor.mirror);

	return ERROR_NONE;
}

static kal_uint32 hs_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			   MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("E");

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode           = IMGSENSOR_MODE_HIGH_SPEED_VIDEO;
	imgsensor.pclk                  = imgsensor_info.hs_video.pclk;
	imgsensor.line_length           = imgsensor_info.hs_video.linelength;
	imgsensor.frame_length          = imgsensor_info.hs_video.framelength;
	imgsensor.min_frame_length      = imgsensor_info.hs_video.framelength;

	imgsensor.dummy_line            = 0;
	imgsensor.dummy_pixel           = 0;
	imgsensor.autoflicker_en        = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);

	set_mode_setfile(imgsensor.sensor_mode);

	set_mirror_flip(imgsensor.mirror);

	return ERROR_NONE;
}

static kal_uint32 slim_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			     MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("E");

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode           = IMGSENSOR_MODE_SLIM_VIDEO;
	imgsensor.pclk                  = imgsensor_info.slim_video.pclk;
	imgsensor.line_length           = imgsensor_info.slim_video.linelength;
	imgsensor.frame_length          = imgsensor_info.slim_video.framelength;
	imgsensor.min_frame_length      = imgsensor_info.slim_video.framelength;

	imgsensor.dummy_line            = 0;
	imgsensor.dummy_pixel           = 0;
	imgsensor.autoflicker_en        = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);

	set_mode_setfile(imgsensor.sensor_mode);

	set_mirror_flip(imgsensor.mirror);

	return ERROR_NONE;
}

static kal_uint32 custom1(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			     MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("E");

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode           = IMGSENSOR_MODE_CUSTOM1;
	imgsensor.pclk                  = imgsensor_info.custom1.pclk;
	imgsensor.line_length           = imgsensor_info.custom1.linelength;
	imgsensor.frame_length          = imgsensor_info.custom1.framelength;
	imgsensor.min_frame_length      = imgsensor_info.custom1.framelength;
	imgsensor.autoflicker_en        = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);

	set_mode_setfile(imgsensor.sensor_mode);

	set_mirror_flip(IMAGE_NORMAL);

	return ERROR_NONE;
}

static kal_uint32 custom2(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			     MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("E");

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode           = IMGSENSOR_MODE_CUSTOM2;
	imgsensor.pclk                  = imgsensor_info.custom2.pclk;
	imgsensor.line_length           = imgsensor_info.custom2.linelength;
	imgsensor.frame_length          = imgsensor_info.custom2.framelength;
	imgsensor.min_frame_length      = imgsensor_info.custom2.framelength;
	imgsensor.autoflicker_en        = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);

	set_mode_setfile(imgsensor.sensor_mode);

	set_mirror_flip(IMAGE_NORMAL);

	return ERROR_NONE;
}


static kal_uint32 custom3(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			     MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("E");

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode           = IMGSENSOR_MODE_CUSTOM3;
	imgsensor.pclk                  = imgsensor_info.custom3.pclk;
	imgsensor.line_length           = imgsensor_info.custom3.linelength;
	imgsensor.frame_length          = imgsensor_info.custom3.framelength;
	imgsensor.min_frame_length      = imgsensor_info.custom3.framelength;
	imgsensor.autoflicker_en        = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);

	set_mode_setfile(imgsensor.sensor_mode);

	set_mirror_flip(IMAGE_NORMAL);

	return ERROR_NONE;
}


static kal_uint32 custom4(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			     MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("E");

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode           = IMGSENSOR_MODE_CUSTOM4;
	imgsensor.pclk                  = imgsensor_info.custom4.pclk;
	imgsensor.line_length           = imgsensor_info.custom4.linelength;
	imgsensor.frame_length          = imgsensor_info.custom4.framelength;
	imgsensor.min_frame_length      = imgsensor_info.custom4.framelength;
	imgsensor.autoflicker_en        = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);

	set_mode_setfile(imgsensor.sensor_mode);

	set_mirror_flip(IMAGE_NORMAL);

	return ERROR_NONE;
}



static kal_uint32 get_resolution(
		MSDK_SENSOR_RESOLUTION_INFO_STRUCT * sensor_resolution)
{
	LOG_INF("%s E\n", __func__);
	sensor_resolution->SensorFullWidth            = imgsensor_info.cap.grabwindow_width;
	sensor_resolution->SensorFullHeight           = imgsensor_info.cap.grabwindow_height;

	sensor_resolution->SensorPreviewWidth         = imgsensor_info.pre.grabwindow_width;
	sensor_resolution->SensorPreviewHeight        = imgsensor_info.pre.grabwindow_height;

	sensor_resolution->SensorVideoWidth           = imgsensor_info.normal_video.grabwindow_width;
	sensor_resolution->SensorVideoHeight          = imgsensor_info.normal_video.grabwindow_height;


	sensor_resolution->SensorHighSpeedVideoWidth  = imgsensor_info.hs_video.grabwindow_width;
	sensor_resolution->SensorHighSpeedVideoHeight = imgsensor_info.hs_video.grabwindow_height;

	sensor_resolution->SensorSlimVideoWidth       = imgsensor_info.slim_video.grabwindow_width;
	sensor_resolution->SensorSlimVideoHeight      = imgsensor_info.slim_video.grabwindow_height;

	sensor_resolution->SensorCustom1Width         = imgsensor_info.custom1.grabwindow_width;
	sensor_resolution->SensorCustom1Height        = imgsensor_info.custom1.grabwindow_height;

	sensor_resolution->SensorCustom2Width         = imgsensor_info.custom2.grabwindow_width;
	sensor_resolution->SensorCustom2Height        = imgsensor_info.custom2.grabwindow_height;

	sensor_resolution->SensorCustom3Width         = imgsensor_info.custom3.grabwindow_width;
	sensor_resolution->SensorCustom3Height        = imgsensor_info.custom3.grabwindow_height;

	sensor_resolution->SensorCustom4Width         = imgsensor_info.custom4.grabwindow_width;
	sensor_resolution->SensorCustom4Height        = imgsensor_info.custom4.grabwindow_height;


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
	LOG_INF("%s:sensor_info->SensorHsyncPolarity = %d\n", __func__, sensor_info->SensorHsyncPolarity);
	sensor_info->SensorVsyncPolarity = SENSOR_CLOCK_POLARITY_LOW;
	LOG_INF("%s:sensor_info->SensorVsyncPolarity = %d\n", __func__, sensor_info->SensorVsyncPolarity);

	sensor_info->SensorInterruptDelayLines = 4;
	sensor_info->SensorResetActiveHigh     = FALSE;
	sensor_info->SensorResetDelayCount     = 5;

	sensor_info->SensroInterfaceType       = imgsensor_info.sensor_interface_type;
	sensor_info->MIPIsensorType            = imgsensor_info.mipi_sensor_type;
	sensor_info->SettleDelayMode           = imgsensor_info.mipi_settle_delay_mode;

	sensor_info->SensorOutputDataFormat    = imgsensor_info.sensor_output_dataformat;

	sensor_info->CaptureDelayFrame         = imgsensor_info.cap_delay_frame;
	sensor_info->PreviewDelayFrame         = imgsensor_info.pre_delay_frame;
	sensor_info->VideoDelayFrame           = imgsensor_info.video_delay_frame;

	sensor_info->HighSpeedVideoDelayFrame  = imgsensor_info.hs_video_delay_frame;

	sensor_info->SlimVideoDelayFrame       = imgsensor_info.slim_video_delay_frame;

	sensor_info->Custom1DelayFrame         = imgsensor_info.custom1_delay_frame;
	sensor_info->Custom2DelayFrame         = imgsensor_info.custom2_delay_frame;
	sensor_info->Custom3DelayFrame         = imgsensor_info.custom3_delay_frame;
	sensor_info->Custom4DelayFrame         = imgsensor_info.custom4_delay_frame;
	sensor_info->SensorMasterClockSwitch   = 0;
	sensor_info->SensorDrivingCurrent      = imgsensor_info.isp_driving_current;

	sensor_info->AEShutDelayFrame          = imgsensor_info.ae_shut_delay_frame;

	sensor_info->AESensorGainDelayFrame    = imgsensor_info.ae_sensor_gain_delay_frame;

	sensor_info->AEISPGainDelayFrame       = imgsensor_info.ae_ispGain_delay_frame;

	sensor_info->FrameTimeDelayFrame       = imgsensor_info.frame_time_delay_frame;

	sensor_info->IHDR_Support              = imgsensor_info.ihdr_support;
	sensor_info->IHDR_LE_FirstLine         = imgsensor_info.ihdr_le_firstline;
	sensor_info->SensorModeNum             = imgsensor_info.sensor_mode_num;
	sensor_info->SensorMIPILaneNumber      = imgsensor_info.mipi_lane_num;
	sensor_info->SensorClockFreq           = imgsensor_info.mclk;
	sensor_info->SensorClockDividCount     = 3;
	sensor_info->SensorClockRisingCount    = 0;
	sensor_info->SensorClockFallingCount   = 2;
	sensor_info->SensorPixelClockCount     = 3;
	sensor_info->SensorDataLatchCount      = 2;
	sensor_info->MIPIDataLowPwr2HighSpeedTermDelayCount = 0;
	sensor_info->MIPICLKLowPwr2HighSpeedTermDelayCount  = 0;
	sensor_info->SensorWidthSampling       = 0;
	sensor_info->SensorHightSampling       = 0;
	sensor_info->SensorPacketECCOrder      = 1;

	LOG_INF("%s scenario_id = %d\n", __func__, scenario_id);
	switch (scenario_id) {
	case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
	LOG_INF("%s:MSDK_SCENARIO_ID_CAMERA_PREVIEW scenario_id= %d\n", __func__, scenario_id);
		sensor_info->SensorGrabStartX = imgsensor_info.pre.startx;
		sensor_info->SensorGrabStartY = imgsensor_info.pre.starty;

		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
			imgsensor_info.pre.mipi_data_lp2hs_settle_dc;

		break;
	case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
	LOG_INF("%s:MSDK_SCENARIO_ID_CAMERA_PREVIEW scenario_id= %d\n", __func__, scenario_id);
		sensor_info->SensorGrabStartX = imgsensor_info.cap.startx;
		sensor_info->SensorGrabStartY = imgsensor_info.cap.starty;

		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
			imgsensor_info.cap.mipi_data_lp2hs_settle_dc;

		break;
	case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
	LOG_INF("%s:MSDK_SCENARIO_ID_CAMERA_PREVIEW scenario_id= %d\n", __func__, scenario_id);
		sensor_info->SensorGrabStartX = imgsensor_info.normal_video.startx;
		sensor_info->SensorGrabStartY = imgsensor_info.normal_video.starty;

		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
			imgsensor_info.normal_video.mipi_data_lp2hs_settle_dc;

		break;
	case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
	LOG_INF("%s:MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO scenario_id= %d\n", __func__, scenario_id);
		sensor_info->SensorGrabStartX = imgsensor_info.hs_video.startx;
		sensor_info->SensorGrabStartY = imgsensor_info.hs_video.starty;

		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
			imgsensor_info.hs_video.mipi_data_lp2hs_settle_dc;

		break;
	case MSDK_SCENARIO_ID_SLIM_VIDEO:
	LOG_INF("%s:MSDK_SCENARIO_ID_SLIM_VIDEO scenario_id= %d\n", __func__, scenario_id);
		sensor_info->SensorGrabStartX = imgsensor_info.slim_video.startx;
		sensor_info->SensorGrabStartY = imgsensor_info.slim_video.starty;

		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
			imgsensor_info.slim_video.mipi_data_lp2hs_settle_dc;

		break;

	case MSDK_SCENARIO_ID_CUSTOM1:
	LOG_INF("%s:MSDK_SCENARIO_ID_CUSTOM1 scenario_id= %d\n", __func__, scenario_id);
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
	default:
		sensor_info->SensorGrabStartX = imgsensor_info.pre.startx;
		sensor_info->SensorGrabStartY = imgsensor_info.pre.starty;

		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
			imgsensor_info.pre.mipi_data_lp2hs_settle_dc;
		break;
	}

	return ERROR_NONE;
}	/* get_info */


static kal_uint32 control(enum MSDK_SCENARIO_ID_ENUM scenario_id,
			  MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("scenario_id = %d", scenario_id);
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
	case MSDK_SCENARIO_ID_CUSTOM2:
		custom2(image_window, sensor_config_data);
		break;
	case MSDK_SCENARIO_ID_CUSTOM3:
		custom3(image_window, sensor_config_data);
		break;
	case MSDK_SCENARIO_ID_CUSTOM4:
		custom4(image_window, sensor_config_data);
		break;
	default:
		LOG_INF("Error ScenarioId setting");
		preview(image_window, sensor_config_data);
		return ERROR_INVALID_SCENARIO_ID;
	}
	return ERROR_NONE;
}				/* control() */

static kal_uint32 set_video_mode(UINT16 framerate)
{

	LOG_INF("%s framerate = %d\n", __func__, framerate);

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

static kal_uint32 set_max_framerate_by_scenario(
		enum MSDK_SCENARIO_ID_ENUM scenario_id,	MUINT32 framerate)
{
	kal_uint32 frame_length;

	LOG_INF("scenario_id = %d, framerate = %d\n", scenario_id, framerate);

	switch (scenario_id) {
	case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		LOG_DBG("MSDK_SCENARIO_ID_CAMERA_PREVIEW scenario_id= %d\n", scenario_id);
		frame_length = imgsensor_info.pre.pclk / framerate * 10 / imgsensor_info.pre.linelength;

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
		LOG_DBG("MSDK_SCENARIO_ID_VIDEO_PREVIEW scenario_id= %d\n", scenario_id);
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
		LOG_DBG("MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG scenario_id= %d\n", scenario_id);

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
		LOG_DBG("MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO scenario_id= %d\n", scenario_id);
		frame_length = imgsensor_info.hs_video.pclk /
		framerate * 10 / imgsensor_info.hs_video.linelength;

		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line = (frame_length > imgsensor_info.hs_video.framelength)
		? (frame_length - imgsensor_info.hs_video.	framelength) : 0;

		imgsensor.frame_length = imgsensor_info.hs_video.framelength + imgsensor.dummy_line;

		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);

		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
		break;
	case MSDK_SCENARIO_ID_SLIM_VIDEO:
		LOG_DBG("MSDK_SCENARIO_ID_SLIM_VIDEO scenario_id= %d\n", scenario_id);

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
		LOG_DBG("MSDK_SCENARIO_ID_CUSTOM1 scenario_id= %d\n", scenario_id);
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
		LOG_DBG("MSDK_SCENARIO_ID_CUSTOM2 scenario_id= %d\n", scenario_id);
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
		LOG_DBG("MSDK_SCENARIO_ID_CUSTOM3 scenario_id= %d\n", scenario_id);
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
		LOG_DBG("MSDK_SCENARIO_ID_CUSTOM4 scenario_id= %d\n", scenario_id);
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
		LOG_INF("error scenario_id = %d, we use preview scenario\n", scenario_id);
		break;
	}
	return ERROR_NONE;
}

static kal_uint32 get_default_framerate_by_scenario(
		enum MSDK_SCENARIO_ID_ENUM scenario_id, MUINT32 *framerate)
{
	LOG_INF("scenario_id = %d", scenario_id);

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
	default:
		break;
	}

	return ERROR_NONE;
}

static kal_uint32 set_test_pattern_mode(kal_bool enable)
{
	LOG_INF("enable: %d", enable);

	//Test Pattern : Address(0x008C) / Defualt(0x00)

	return ERROR_NONE;
}

static kal_uint32 feature_control(MSDK_SENSOR_FEATURE_ENUM feature_id,
				  UINT8 *feature_para, UINT32 *feature_para_len)
{
	UINT16 *feature_return_para_16 = (UINT16 *) feature_para;
	UINT16 *feature_data_16 = (UINT16 *) feature_para;
	UINT32 *feature_return_para_32 = (UINT32 *) feature_para;
	UINT32 *feature_data_32 = (UINT32 *) feature_para;

	unsigned long long *feature_data = (unsigned long long *)feature_para;
	struct SENSOR_WINSIZE_INFO_STRUCT *wininfo;

	MSDK_SENSOR_REG_INFO_STRUCT *sensor_reg_data =
		(MSDK_SENSOR_REG_INFO_STRUCT *) feature_para;

	LOG_DBG("feature_id = %d", feature_id);
	switch (feature_id) {
	case SENSOR_FEATURE_GET_PERIOD:
		*feature_return_para_16++ = imgsensor.line_length;
		*feature_return_para_16   = imgsensor.frame_length;
		*feature_para_len         = 4;
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
			= (imgsensor_info.hs_video.framelength << 16)
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
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		default:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
			= (imgsensor_info.pre.framelength << 16)
				+ imgsensor_info.pre.linelength;
			break;
		}
		break;
	case SENSOR_FEATURE_GET_PIXEL_CLOCK_FREQ:
		*feature_return_para_32 = imgsensor.pclk;
		*feature_para_len = 4;
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
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		default:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.pre.pclk;
			break;
		}
		break;
	case SENSOR_FEATURE_SET_ESHUTTER:
		LOG_DBG("SENSOR_FEATURE_SET_ESHUTTER = %d\n", feature_id);
		set_shutter(*feature_data);
		break;
	case SENSOR_FEATURE_SET_NIGHTMODE:
		LOG_INF("SENSOR_FEATURE_SET_NIGHTMODE = %d, data=%d\n", feature_id, *feature_data);
		break;
	case SENSOR_FEATURE_SET_GAIN:
		LOG_DBG("SENSOR_FEATURE_SET_GAIN = %d\n", feature_id);
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
		write_cmos_sensor(sensor_reg_data->RegAddr, sensor_reg_data->RegData);
		break;
	case SENSOR_FEATURE_GET_REGISTER:
		LOG_INF("SENSOR_FEATURE_GET_REGISTER = %d\n", feature_id);
		sensor_reg_data->RegData = read_cmos_sensor(sensor_reg_data->RegAddr);
		break;
	case SENSOR_FEATURE_GET_LENS_DRIVER_ID:
		// get the lens driver ID from EEPROM or just
		// return LENS_DRIVER_ID_DO_NOT_CARE
		// if EEPROM does not exist in camera module.
		LOG_INF("SENSOR_FEATURE_GET_LENS_DRIVER_ID = %d\n", feature_id);
		*feature_return_para_32 = LENS_DRIVER_ID_DO_NOT_CARE;
		*feature_para_len       = 4;
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
		LOG_DBG("SENSOR_FEATURE_SET_AUTO_FLICKER_MODE = %d\n", feature_id);
		set_auto_flicker_mode((BOOL) (*feature_data_16), *(feature_data_16 + 1));
		break;
	case SENSOR_FEATURE_SET_MAX_FRAME_RATE_BY_SCENARIO:
		LOG_DBG("SENSOR_FEATURE_SET_MAX_FRAME_RATE_BY_SCENARIO = %d\n", feature_id);
		set_max_framerate_by_scenario(
		(enum MSDK_SCENARIO_ID_ENUM) *feature_data, *(feature_data + 1));
		break;
	case SENSOR_FEATURE_GET_DEFAULT_FRAME_RATE_BY_SCENARIO:
		LOG_DBG("SENSOR_FEATURE_GET_DEFAULT_FRAME_RATE_BY_SCENARIO = %d\n", feature_id);
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
		LOG_DBG("SENSOR_FEATURE_SET_FRAMERATE = %d\n", feature_id);
		LOG_INF("current fps :%d\n", *feature_data_32);
		spin_lock(&imgsensor_drv_lock);
		imgsensor.current_fps = (UINT16)*feature_data_32;
		spin_unlock(&imgsensor_drv_lock);
	case SENSOR_FEATURE_SET_HDR:
		LOG_DBG("SENSOR_FEATURE_SET_HDR = %d\n", feature_id);
		LOG_INF("ihdr enable :%d\n", *feature_data_32);
		spin_lock(&imgsensor_drv_lock);
		imgsensor.ihdr_mode = (UINT8)*feature_data_32;
		spin_unlock(&imgsensor_drv_lock);
		break;
	case SENSOR_FEATURE_GET_CROP_INFO:
		LOG_DBG("SENSOR_FEATURE_GET_CROP_INFO = %d\n", feature_id);
		LOG_INF("SENSOR_FEATURE_GET_CROP_INFO scenarioId:%d\n", (UINT32) *feature_data);
		wininfo = (struct SENSOR_WINSIZE_INFO_STRUCT *) (uintptr_t) (*(feature_data + 1));

		switch (*feature_data_32) {
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			LOG_DBG("MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG = %d\n", *feature_data_32);
			memcpy((void *)wininfo, (void *)&imgsensor_winsize_info[1],
				   sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			LOG_DBG("MSDK_SCENARIO_ID_VIDEO_PREVIEW = %d\n", *feature_data_32);
			memcpy((void *)wininfo, (void *)&imgsensor_winsize_info[2],
				   sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
			LOG_DBG("MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO = %d\n", *feature_data_32);
			memcpy((void *)wininfo, (void *)&imgsensor_winsize_info[3],
				   sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
			LOG_DBG("MSDK_SCENARIO_ID_SLIM_VIDEO = %d\n", *feature_data_32);
			memcpy((void *)wininfo, (void *)&imgsensor_winsize_info[4],
				   sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;

		case MSDK_SCENARIO_ID_CUSTOM1:
			LOG_DBG("MSDK_SCENARIO_ID_CUSTOM1 = %d\n", feature_id);
			memcpy((void *)wininfo, (void *)&imgsensor_winsize_info[5],
					sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_CUSTOM2:
			LOG_DBG("MSDK_SCENARIO_ID_CUSTOM2 = %d\n", feature_id);
			memcpy((void *)wininfo,
			(void *)&imgsensor_winsize_info[6],
			sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_CUSTOM3:
			LOG_DBG("MSDK_SCENARIO_ID_CUSTOM3 = %d\n", feature_id);
			memcpy((void *)wininfo,
			(void *)&imgsensor_winsize_info[7],
			sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_CUSTOM4:
			LOG_DBG("MSDK_SCENARIO_ID_CUSTOM4 = %d\n", feature_id);
			memcpy((void *)wininfo,
			(void *)&imgsensor_winsize_info[8],
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
		LOG_DBG("SENSOR_FEATURE_SET_IHDR_SHUTTER_GAIN = %d\n", feature_id);
		LOG_DBG("SENSOR_SET_SENSOR_IHDR LE=%d, SE=%d, Gain=%d\n",
			(UINT16) *feature_data,
			(UINT16) *(feature_data + 1),
			(UINT16) *(feature_data + 2));
		//ihdr_write_shutter_gain((UINT16)*feature_data,
		//(UINT16)*(feature_data+1),(UINT16)*(feature_data+2));
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

	case SENSOR_FEATURE_GET_MIPI_PIXEL_RATE:
	{
		LOG_DBG("SENSOR_FEATURE_GET_MIPI_PIXEL_RATE = %d\n", feature_id);
		if (enable_adaptive_mipi) {
			LOG_INF("Update mipi info for adaptive mipi");
			update_mipi_info(*feature_data);
		}

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
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
			LOG_INF("MSDK_SCENARIO_ID_CAMERA_PREVIEW = %d\n", *feature_data);
		default:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) = imgsensor_info.pre.mipi_pixel_rate;
			break;
		}
	}
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
}				/*      feature_control()  */

static struct SENSOR_FUNCTION_STRUCT sensor_func = {
	open,
	get_info,
	get_resolution,
	feature_control,
	control,
	close
};

static kal_uint16 gc08a3_otp_init_setting[] = {
	0x0324,	0x42,
	0x0316,	0x09,
	0x0A67,	0x80,
	0x0313,	0x00,
	0x0A53,	0x0E,
	0x0A65,	0x17,
	0x0A68,	0xa1,
	0x0A47,	0x00,
	0x0A58,	0x00,
	0x0ACE,	0x0C,
};

static void gc08a3_otp_off_setting(void)
{
	write_cmos_sensor_8(0x0316, 0x00);
	write_cmos_sensor_8(0x0A67, 0x00);
}

static unsigned char gc08a3_read_otp_byte(unsigned int addr)
{
	unsigned short read_data = 0;
	write_cmos_sensor_8(0x0A69, ((addr>>8) & 0xFF));
	write_cmos_sensor_8(0x0A6A, (addr & 0xFF));
	read_data = read_cmos_sensor_8(0x0313);
	write_cmos_sensor_8(0x0313, (read_data | (0x1 << 5)));

	return (unsigned char)read_cmos_sensor_8(0x0A6C);
}

#define GC08A3_BANK_SELECT_ADDR			0x15A0
#define GC08A3_OTP_START_ADDR_BANK1		0x15C0
#define GC08A3_OTP_START_ADDR_BANK2		0x38E0
#define GC08A3_OTP_START_ADDR_BANK3		0x5C00
#define GC08A3_OTP_START_ADDR_BANK4		0x7F20
#define GC08A3_OTP_START_ADDR_BANK5		0xA240
int gc08a3_read_otp_cal(unsigned int addr, unsigned char *data, unsigned int size)
{
	int i;
	unsigned int otp_addr;
	unsigned char bank;


	table_write_cmos_sensor(gc08a3_otp_init_setting,
		sizeof(gc08a3_otp_init_setting) / sizeof(kal_uint16));

	mDELAY(10);

	bank = gc08a3_read_otp_byte(GC08A3_BANK_SELECT_ADDR);

	/* select start address */
	switch (bank) {
	case 0x01:
		otp_addr = GC08A3_OTP_START_ADDR_BANK1;
		break;
	case 0x03:
		otp_addr = GC08A3_OTP_START_ADDR_BANK2;
		break;
	case 0x07:
		otp_addr = GC08A3_OTP_START_ADDR_BANK3;
		break;
	case 0x0F:
		otp_addr = GC08A3_OTP_START_ADDR_BANK4;
		break;
	case 0x1F:
		otp_addr = GC08A3_OTP_START_ADDR_BANK5;
		break;
	default:
		LOG_ERR(" Invalid bank data %d", bank);
		gc08a3_otp_off_setting();
		return -1;
	}

	LOG_INF(" otp bank: %d", bank);

	for (i = 0; i < size; i++) {
		*(data + i) = gc08a3_read_otp_byte(otp_addr);
		otp_addr += 8;
	}

	gc08a3_otp_off_setting();

	return size;
}

UINT32 GC08A3_MIPI_RAW_SensorInit(struct SENSOR_FUNCTION_STRUCT **pfFunc)
{
	/* To Do : Check Sensor status here */
	if (pfFunc != NULL)
		*pfFunc = &sensor_func;
	return ERROR_NONE;
}				/*      s5k4hayx_MIPI_RAW_SensorInit      */

