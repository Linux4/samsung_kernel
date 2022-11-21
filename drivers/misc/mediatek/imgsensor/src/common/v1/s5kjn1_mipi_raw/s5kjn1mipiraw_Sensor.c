/*
 * Copyright (C) 2021 MediaTek Inc.
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
 *	 s5kjn1mipiraw_Sensor.c
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
#define PFX "S5KJN1 D/D"
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
#include <linux/slab.h>

#include "kd_camera_typedef.h"
#include "kd_imgsensor.h"

#include "kd_imgsensor_define.h"
#include "kd_imgsensor_errcode.h"
#include "kd_imgsensor_sysfs_adapter.h"

#include "imgsensor_sysfs.h"

#include "s5kjn1mipiraw_Sensor.h"
#include "s5kjn1mipiraw_setfile.h"

#define WRITE_SENSOR_CAL_FOR_HW_GGC                   (1)
#define XTC_CAL_DEBUG                                 (0)

#define MULTI_WRITE                                   (1)
#if MULTI_WRITE
static const int I2C_BUFFER_LEN = 1020;
#else
static const int I2C_BUFFER_LEN = 4;
#endif

#define INDIRECT_BURST                                (0)
#if INDIRECT_BURST
static const int I2C_BURST_BUFFER_LEN = 1400;
#endif

#define SENSOR_JN1_COARSE_INTEGRATION_TIME_MAX_MARGIN (5)
#define SENSOR_JN1_MAX_COARSE_INTEGRATION_TIME        (65503)

static kal_bool sIsNightHyperlapse = KAL_FALSE;

static bool enable_adaptive_mipi;
static int adaptive_mipi_index;

#define LOG_DBG(format, args...) pr_debug(format, ##args)
#define LOG_INF(format, args...) pr_info(format, ##args)
#define LOG_ERR(format, args...) pr_err(format, ##args)

static DEFINE_SPINLOCK(imgsensor_drv_lock);
//Sensor ID Value: 0x38E1, sensor id defined in Kd_imgsensor.h
static struct imgsensor_info_struct imgsensor_info = {
	.sensor_id = S5KJN1_SENSOR_ID,
	.checksum_value = 0xa4c32546,
	.pre = {
		.pclk = 563330000,
		.linelength  = 5200,
		.framelength = 3565,
		.startx = 0,
		.starty = 0,
		.grabwindow_width  = 4080,
		.grabwindow_height = 3060,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 300,
		.mipi_pixel_rate = 577200000,
	},
	.cap = {
		.pclk = 563330000,
		.linelength  = 5200,
		.framelength = 3565,
		.startx = 0,
		.starty = 0,
		.grabwindow_width  = 4080,
		.grabwindow_height = 3060,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 300,
		.mipi_pixel_rate = 577200000,
	},
	.normal_video = {
		.pclk = 563330000,
		.linelength  = 5200,
		.framelength = 3565,
		.startx = 0,
		.starty = 0,
		.grabwindow_width  = 4080,
		.grabwindow_height = 2296,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 300,
		.mipi_pixel_rate = 577200000,
	},
	.hs_video = {
		.pclk = 598000000,
		.linelength  = 2800,
		.framelength = 1770,
		.startx = 0,
		.starty = 0,
		.grabwindow_width  = 2032,
		.grabwindow_height = 1148,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 1200,
		.mipi_pixel_rate = 577200000,
	},
	.slim_video = {// NOT USED
		.pclk = 563330000,
		.linelength  = 5200,
		.framelength = 3565,
		.startx = 0,
		.starty = 0,
		.grabwindow_width  = 4080,
		.grabwindow_height = 3060,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 300,
		.mipi_pixel_rate = 577200000,
	},
	.custom1 = {
		.pclk = 598000000,
		.linelength  = 2800,
		.framelength = 3540,
		.startx = 0,
		.starty = 0,
		.grabwindow_width  = 2032,
		.grabwindow_height = 1524,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 600,
		.mipi_pixel_rate = 577200000,
	},
	.custom2 = {
		.pclk = 563330000,
		.linelength  = 8688,
		.framelength = 6284,
		.startx = 0,
		.starty = 0,
		.grabwindow_width  = 8160,
		.grabwindow_height = 6120,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 100,
		.mipi_pixel_rate = 577200000,
	},
	.custom3 = { // NOT USED
		.pclk = 563330000,
		.linelength  = 5200,
		.framelength = 3565,
		.startx = 0,
		.starty = 0,
		.grabwindow_width  = 4080,
		.grabwindow_height = 3060,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 300,
		.mipi_pixel_rate = 577200000,
	},
	.custom4 = { // NOT USED
		.pclk = 563330000,
		.linelength  = 5200,
		.framelength = 3565,
		.startx = 0,
		.starty = 0,
		.grabwindow_width  = 4080,
		.grabwindow_height = 3060,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 300,
		.mipi_pixel_rate = 577200000,
	},
	.custom5 = { // NOT USED
		.pclk = 563330000,
		.linelength  = 5200,
		.framelength = 3565,
		.startx = 0,
		.starty = 0,
		.grabwindow_width  = 4080,
		.grabwindow_height = 3060,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 300,
		.mipi_pixel_rate = 577200000,
	},
	.margin = 10,
	.min_shutter = 8,
	.min_gain = 64,
	.max_gain = 2048,
	.min_gain_iso = 20,
	.exp_step = 1,
	.gain_step = 2,
	.gain_type = 2,
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

#ifdef CONFIG_IMPROVE_CAMERA_PERFORMANCE
	.custom1_delay_frame = 1,
	.custom2_delay_frame = 1,
	.custom3_delay_frame = 1,
	.custom4_delay_frame = 1,
	.custom5_delay_frame = 1,
	.slim_video_delay_frame = 1,
#else
	.custom1_delay_frame = 3,
	.custom2_delay_frame = 3,
	.custom3_delay_frame = 3,
	.custom4_delay_frame = 3,
	.custom5_delay_frame = 3,
	.slim_video_delay_frame = 3,
#endif

	.isp_driving_current = ISP_DRIVING_8MA,
	.sensor_interface_type = SENSOR_INTERFACE_TYPE_MIPI,
	.mipi_sensor_type = MIPI_OPHY_NCSI2,
	.mipi_settle_delay_mode = 1,
	.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_4CELL_Gr,
	.mclk = 26,
	.mipi_lane_num = SENSOR_MIPI_4_LANE,
	.i2c_speed = 400,

	/* record sensor support all write id addr,
	 * only supprt 4 must end with 0xff
	 */
	.i2c_addr_table = {0x5A, 0xff},
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
	.current_fps = 0,

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
	.i2c_write_id = 0x5A,
};

static struct SENSOR_VC_INFO_STRUCT SENSOR_VC_INFO[4] = {
	/* Preview mode setting */
	{0x02, 0x0a, 0x00, 0x08, 0x40, 0x00,
	 0x00, 0x2b, 0x0ff0, 0x0bf4,/*VC0, 4080x3060 pixel*/
	 0x00, 0x00, 0x0000, 0x0000,/*VC1*/
	 0x01, 0x2b, 0x01fc, 0x0bf0,/*VC2, 508x3056 pixel -> 635x3056 byte*/
	 0x00, 0x00, 0x0000, 0x0000},/*VC3*/
	/* cap = pre mode setting */
	{0x02, 0x0a, 0x00, 0x08, 0x40, 0x00,
	 0x00, 0x2b, 0x0ff0, 0x0bf4,/*VC0, 4080x3060 pixel*/
	 0x00, 0x00, 0x0000, 0x0000,/*VC1*/
	 0x01, 0x2b, 0x01fc, 0x0bf0,/*VC2, 508x3056 pixel -> 635x3056 byte*/
	 0x00, 0x00, 0x0000, 0x0000},/*VC3*/
	/* Video mode setting */
	{0x02, 0x0a, 0x00, 0x08, 0x40, 0x00,
	 0x00, 0x2b, 0x0ff0, 0x08f8,/*VC0, 4080x2296 pixel*/
	 0x00, 0x00, 0x0000, 0x0000,/*VC1*/
	 0x01, 0x2b, 0x01fc, 0x08f0,/*VC2, 508x2288 pixel -> 635x2288 byte*/
	 0x00, 0x00, 0x0000, 0x0000},/*VC3*/
	/* hs mode setting */
	{0x01, 0x0a, 0x00, 0x08, 0x40, 0x00,
	 0x00, 0x2b, 0x07f0, 0x047c,/*VC0, 2032x1148 pixel*/
	 0x00, 0x00, 0x0000, 0x0000,/*VC1*/
	 0x00, 0x00, 0x0000, 0x0000,/*VC2*/
	 0x00, 0x00, 0x0000, 0x0000},/*VC3*/
};

static struct SENSOR_WINSIZE_INFO_STRUCT imgsensor_winsize_info[] = {
	{ 8192, 6176,    0,   12, 8192, 6152, 4096, 3076,
	     8,    8, 4080, 3060,    0,    0, 4080, 3060 }, /* Preview */
	{ 8192, 6176,    0,   12, 8192, 6152, 4096, 3076,
	     8,    8, 4080, 3060,    0,    0, 4080, 3060 }, /* capture */
	{ 8192, 6176,    0,  776, 8192, 4624, 4096, 2312,
	     8,    8, 4080, 2296,    0,    0, 4080, 2296 }, /* video */
	{ 8192, 6176,   16,  768, 8160, 4640, 2040, 1160,
	     4,    6, 2032, 1148,    0,    0, 2032, 1148 }, /* hs video */
	{ 8192, 6176,    0,   12, 8192, 6152, 4096, 3076,
	     8,    8, 4080, 3060,    0,    0, 4080, 3060 }, /* slim video - NOT USED */
	{ 8192, 6176,   16,   16, 8160, 6144, 2040, 1536,
	     4,    6, 2032, 1524,    0,    0, 2032, 1524 }, /* custom1 */
	{ 8192, 6176,    0,   12, 8192, 6152, 8192, 6152,
	    16,   16, 8160, 6120,    0,    0, 8160, 6120 }, /* custom2 */
	{ 8192, 6176,    0,   12, 8192, 6152, 4096, 3076,
	     8,    8, 4080, 3060,    0,    0, 4080, 3060 }, /* custom3 - NOT USED */
	{ 8192, 6176,    0,   12, 8192, 6152, 4096, 3076,
	     8,    8, 4080, 3060,    0,    0, 4080, 3060 }, /* custom4 - NOT USED */
	{ 8192, 6176,    0,   12, 8192, 6152, 4096, 3076,
	     8,    8, 4080, 3060,    0,    0, 4080, 3060 }, /* custom5 - NOT USED */
};

static struct SET_PD_BLOCK_INFO_T imgsensor_pd_info = {
		.i4OffsetX   = 8,
		.i4OffsetY   = 2,
		.i4PitchX    = 8,
		.i4PitchY    = 8,
		.i4PairNum   = 4,
		.i4SubBlkW   = 8,
		.i4SubBlkH   = 2,
		.i4BlockNumX = 508,
		.i4BlockNumY = 382,
		.iMirrorFlip = 0,
		.i4PosR = {
			{8, 2}, {10, 5}, {14, 6}, {12, 9},
		},
		.i4PosL = {
			{9, 2}, {11, 5}, {15, 6}, {13, 9},
		},
		.i4Crop = {
			{0, 0}, {0, 0}, {0, 382}, {0, 0}, {0, 0},
			{0, 0}, {0, 0}, {0, 0},   {0, 0}, {0, 0}
		}
};

int write_cmos_sensor(kal_uint16 addr, kal_uint16 para)
{
	char pusendcmd[4] = {
		(char)(addr >> 8), (char)(addr & 0xFF),
		(char)(para >> 8), (char)(para & 0xFF) };
	int ret = 0;

	ret = iWriteRegI2CTiming(pusendcmd, 4,
		imgsensor.i2c_write_id, imgsensor_info.i2c_speed);
	return ret;
}

static kal_uint16 read_cmos_sensor_8(kal_uint16 addr)
{
	kal_uint16 get_byte = 0;
	char pusendcmd[2] = { (char)(addr >> 8), (char)(addr & 0xFF) };

	iReadRegI2CTiming(pusendcmd, 2, (u8 *) &get_byte, 1,
		imgsensor.i2c_write_id, imgsensor_info.i2c_speed);
	return get_byte;
}

static kal_uint16 write_cmos_sensor_8(kal_uint16 addr, kal_uint8 para)
{
	int ret = 0;
	char pusendcmd[3] = {
		(char)(addr >> 8), (char)(addr & 0xFF), (char)(para & 0xFF) };

	ret = iWriteRegI2CTiming(pusendcmd, 3,
		imgsensor.i2c_write_id, imgsensor_info.i2c_speed);
	return ret;
}

static void set_dummy(void)
{
	LOG_DBG("dummyline = %d, dummypixels = %d\n",
		imgsensor.dummy_line, imgsensor.dummy_pixel);

	write_cmos_sensor_8(0x702, 0); // FLL Lshift reset
	write_cmos_sensor_8(0x704, 0); // CIT Lshift reset
	write_cmos_sensor(0x0340, imgsensor.frame_length);
	write_cmos_sensor(0x0342, imgsensor.line_length);
}

#if INDIRECT_BURST
static kal_int32 burst_table_write_cmos_sensor(kal_uint16 *para, kal_uint32 len)
{
	char puSendCmd[I2C_BURST_BUFFER_LEN];
	kal_uint32 tosend = 0, data_count = 0;
	kal_int32 ret = -1;
	kal_uint16 data = 0;

	while (len > data_count) {
		data = para[data_count++];
		puSendCmd[tosend++] = (char)(data >> 8);
		puSendCmd[tosend++] = (char)(data & 0xFF);
	}

	ret = iBurstWriteReg_multi(puSendCmd, tosend,
		imgsensor.i2c_write_id, tosend, imgsensor_info.i2c_speed);
	if (ret < 0)
		LOG_ERR("fail!");

	return ret;
}
#endif

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
}

static int update_mipi_info(enum MSDK_SCENARIO_ID_ENUM scenario_id)
{
	LOG_DBG("- E\n");

	adaptive_mipi_index = imgsensor_select_mipi_by_rf_channel(s5kjn1_mipi_channel_full,
						ARRAY_SIZE(s5kjn1_mipi_channel_full));
	if (adaptive_mipi_index < 0) {
		LOG_ERR("Not found mipi index");
		return -EIO;
	}

	LOG_DBG("pclk(%d), mipi_pixel_rate(%d), framelength(%d)\n",
		imgsensor_info.pre.pclk, imgsensor_info.pre.mipi_pixel_rate, imgsensor_info.pre.framelength);

	if (adaptive_mipi_index == CAM_JN1_SET_A_all_721p5_MHZ) {
		LOG_INF("Adaptive Mipi (721.5 MHz) : mipi_pixel_rate (1443 Mbps)");
		if (scenario_id == MSDK_SCENARIO_ID_CAMERA_PREVIEW) {
			imgsensor_info.pre.mipi_pixel_rate = 577200000;
		} else if (scenario_id == MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG) {
			imgsensor_info.cap.mipi_pixel_rate = 577200000;
		} else if (scenario_id == MSDK_SCENARIO_ID_VIDEO_PREVIEW) {
			imgsensor_info.normal_video.mipi_pixel_rate = 577200000;
		} else if (scenario_id == MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO) {
			imgsensor_info.hs_video.mipi_pixel_rate = 577200000;
		} else {
			LOG_ERR("Wrong Scenario id : %d", scenario_id);
			return -EIO;
		}
	} else if (adaptive_mipi_index == CAM_JN1_SET_A_all_773p5_MHZ) {
		LOG_INF("Adaptive Mipi (773.5 MHz) : mipi_pixel_rate (1547 Mbps)");
		if (scenario_id == MSDK_SCENARIO_ID_CAMERA_PREVIEW) {
			imgsensor_info.pre.mipi_pixel_rate = 618800000;
		} else if (scenario_id == MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG) {
			imgsensor_info.cap.mipi_pixel_rate = 618800000;
		} else if (scenario_id == MSDK_SCENARIO_ID_VIDEO_PREVIEW) {
			imgsensor_info.normal_video.mipi_pixel_rate = 618800000;
		} else if (scenario_id == MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO) {
			imgsensor_info.hs_video.mipi_pixel_rate = 618800000;
		} else {
			LOG_ERR("Wrong Scenario id : %d", scenario_id);
			return -EIO;
		}
	} else {//CAM_JN1_SET_A_all_793_MHZ
		LOG_INF("Adaptive Mipi (793 MHz) : mipi_pixel_rate (1586 Mbps)");
		if (scenario_id == MSDK_SCENARIO_ID_CAMERA_PREVIEW) {
			imgsensor_info.pre.mipi_pixel_rate = 634400000;
		} else if (scenario_id == MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG) {
			imgsensor_info.cap.mipi_pixel_rate = 634400000;
		} else if (scenario_id == MSDK_SCENARIO_ID_VIDEO_PREVIEW) {
			imgsensor_info.normal_video.mipi_pixel_rate = 634400000;
		} else if (scenario_id == MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO) {
			imgsensor_info.hs_video.mipi_pixel_rate = 634400000;
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
	if (mipi_index == CAM_JN1_SET_A_all_773p5_MHZ) {
		LOG_INF("Set adaptive Mipi as 773.5 MHz");
		table_write_cmos_sensor(s5kjn1_mipi_full_773p5mhz,
			ARRAY_SIZE(s5kjn1_mipi_full_773p5mhz));
	} else if (mipi_index == CAM_JN1_SET_A_all_793_MHZ) {
		LOG_INF("Set adaptive Mipi as 793 MHz");
		table_write_cmos_sensor(s5kjn1_mipi_full_793mhz,
			ARRAY_SIZE(s5kjn1_mipi_full_793mhz));
	} else {//CAM_JN1_SET_A_all_721p5_MHZ is set as default, so no need to set.
		LOG_INF("Set adaptive Mipi as 721.5 MHz");
	}
	LOG_DBG("- X");
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
	int timeout = (10000 / imgsensor.current_fps) + 1;

	mDELAY(3);
	for (i = 0; i < timeout; i++) {
		if (onoff) {
			if (read_cmos_sensor_8(0x0005) == 0xFF)
				mDELAY(check_delay);
			else
				break;
		} else {
			if (read_cmos_sensor_8(0x0005) != 0xFF)
				mDELAY(check_delay);
			else
				break;
		}
	}
	LOG_INF("[onoff %d], wait_time = %d ms\n", onoff, i * check_delay + 3);
}

static kal_uint32 streaming_control(kal_bool enable)
{
	LOG_INF("streaming_enable(0=Sw Standby,1=streaming): %d\n", enable);

	if (enable) {
		LOG_INF("enable = %d", enable);
		if (enable_adaptive_mipi && imgsensor.current_scenario_id != MSDK_SCENARIO_ID_CUSTOM1)
			set_mipi_mode(adaptive_mipi_index);

		write_cmos_sensor(0x0100, 0x0100);
	} else {
		LOG_INF("streamoff enable = %d", enable);
		write_cmos_sensor(0x0100, 0x0000);

		//only wait stream_off
		wait_stream_onoff(enable);
	}
	return ERROR_NONE;
}

static void write_shutter(kal_uint32 shutter)
{
	kal_uint32 realtime_fps = 0;
	kal_uint32 target_exp = 0;
	kal_uint32 target_frame_duration = 0;
	kal_uint32 frame_length_lines = 0;
	kal_uint32 cit_lshift_val = 0;
	kal_uint32 cit_lshift_count = 0;

	kal_uint32 min_fine_int = 0;
	kal_uint32 vt_pic_clk_freq_mhz = 0;
	kal_uint32 line_length_pck = 0;

	vt_pic_clk_freq_mhz = imgsensor.pclk / (1000 * 1000);
	line_length_pck = imgsensor.line_length;
	min_fine_int = 0x0100;

	target_exp = ((shutter * line_length_pck) + min_fine_int) / vt_pic_clk_freq_mhz;

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

	if (sIsNightHyperlapse) {
		// fps = 1/1.5
		// frame_length = pclk / (line_length * fps)
		imgsensor.frame_length = (((imgsensor.pclk / (1000 * 1000)) * (1500000))/imgsensor.line_length);
	}
	target_frame_duration = ((imgsensor.frame_length * imgsensor.line_length) / (imgsensor.pclk / 1000000));

	if (target_frame_duration > 100000) {
		int max_coarse_integration_time =
			SENSOR_JN1_MAX_COARSE_INTEGRATION_TIME - SENSOR_JN1_COARSE_INTEGRATION_TIME_MAX_MARGIN;

		cit_lshift_val = (kal_uint32)(target_frame_duration / 100000);
		while (cit_lshift_val > 1) {
			cit_lshift_val /= 2;
			target_frame_duration /= 2;
			target_exp /= 2;
			cit_lshift_count++;
		}
		if (cit_lshift_count > 11)
			cit_lshift_count = 11;

		frame_length_lines = (kal_uint32)((vt_pic_clk_freq_mhz * target_frame_duration) / line_length_pck);
		imgsensor.frame_length = frame_length_lines;

		shutter = ((target_exp * vt_pic_clk_freq_mhz) - min_fine_int) / line_length_pck;
		if (shutter > max_coarse_integration_time)
			shutter = max_coarse_integration_time;

		write_cmos_sensor(0x0340, imgsensor.frame_length & 0xFFFF);
		write_cmos_sensor_8(0x702, cit_lshift_count & 0xFF); // FLL Lshift addr
		write_cmos_sensor_8(0x704, cit_lshift_count & 0xFF); // CIT Lshift addr
	} else {
		if (imgsensor.autoflicker_en) {
			realtime_fps = imgsensor.pclk /
			imgsensor.line_length * 10 / imgsensor.frame_length;

			if (realtime_fps >= 297 && realtime_fps <= 305)
				set_max_framerate(296, 0);
			else if (realtime_fps >= 147 && realtime_fps <= 150)
				set_max_framerate(146, 0);
			else {
				write_cmos_sensor(0x0340, imgsensor.frame_length & 0xFFFF);
				write_cmos_sensor_8(0x702, 0); // FLL Lshift reset
				write_cmos_sensor_8(0x704, 0); // CIT Lshift reset
			}
		} else {
			write_cmos_sensor(0x0340, imgsensor.frame_length & 0xFFFF);
			write_cmos_sensor_8(0x702, 0); // FLL Lshift reset
			write_cmos_sensor_8(0x704, 0); // CIT Lshift reset
		}
	}
	write_cmos_sensor(0x0202, shutter & 0xFFFF);
	LOG_DBG("Exit! shutter =%d, framelength =%d\n", shutter, imgsensor.frame_length);
}

static void set_shutter_frame_length(kal_uint32 shutter,
			kal_uint32 frame_length)
{
	unsigned long flags;
	kal_uint16 realtime_fps = 0;
	kal_int32 dummy_line = 0;

	spin_lock_irqsave(&imgsensor_drv_lock, flags);
	imgsensor.shutter = shutter;
	spin_unlock_irqrestore(&imgsensor_drv_lock, flags);

	spin_lock(&imgsensor_drv_lock);
	/*Change frame time*/
	dummy_line = frame_length - imgsensor.frame_length;
	imgsensor.frame_length = imgsensor.frame_length + dummy_line;
	imgsensor.min_frame_length = imgsensor.frame_length;
	//
	if (shutter > imgsensor.min_frame_length - imgsensor_info.margin)
		imgsensor.frame_length = shutter + imgsensor_info.margin;
	else
		imgsensor.frame_length = imgsensor.min_frame_length;

	if (imgsensor.frame_length > imgsensor_info.max_frame_length)
		imgsensor.frame_length = imgsensor_info.max_frame_length;

	spin_unlock(&imgsensor_drv_lock);
	shutter = (shutter < imgsensor_info.min_shutter) ?
		imgsensor_info.min_shutter : shutter;
	shutter = (shutter > (imgsensor_info.max_frame_length -
		imgsensor_info.margin)) ? (imgsensor_info.max_frame_length -
		imgsensor_info.margin) : shutter;

	if (imgsensor.autoflicker_en) {
		realtime_fps = imgsensor.pclk /
			imgsensor.line_length * 10 / imgsensor.frame_length;
		if (realtime_fps >= 297 && realtime_fps <= 305)
			set_max_framerate(296, 0);
		else if (realtime_fps >= 147 && realtime_fps <= 150)
			set_max_framerate(146, 0);
		else {
			// Extend frame length
			write_cmos_sensor(0x0340,
				imgsensor.frame_length & 0xffff);
		}
	} else {
		// Extend frame length
		write_cmos_sensor(0x0340, imgsensor.frame_length & 0xffff);
	}

	// Update Shutter
	write_cmos_sensor(0x0202, shutter & 0xffff);
	LOG_DBG("Add for N3D! shutter =%d, framelength =%d\n",
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
static void set_shutter(kal_uint32 shutter)
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
		LOG_ERR("Error gain setting");
		if (gain < BASEGAIN)
			gain = BASEGAIN;
		else if (gain > 32 * BASEGAIN)
			gain = 32 * BASEGAIN;
	}

	reg_gain = gain2reg(gain);
	spin_lock(&imgsensor_drv_lock);
	imgsensor.gain = reg_gain;
	spin_unlock(&imgsensor_drv_lock);
	LOG_DBG("%s = %d , reg_gain = 0x%x\n ", __func__, gain, reg_gain);

	write_cmos_sensor(0x0204, (reg_gain&0xFFFF));
	return gain;
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

static void set_mode_setfile(enum IMGSENSOR_MODE mode)
{
	if (mode >= IMGSENSOR_MODE_MAX) {
		LOG_ERR("invalid mode: %d\n", mode);
		return;
	}

	LOG_INF("mode: %s\n", s5kjn1_setfile_info[mode].name);

	table_write_cmos_sensor(s5kjn1_setfile_info[mode].setfile, s5kjn1_setfile_info[mode].size);
}


#if INDIRECT_BURST
static kal_uint16 addr_data_burst_init_jn1[] = {
	0x6F12,
	0x1743,
	0x01FC,
	0xE702,
	0xA3F3,
	0xB787,
	0x0024,
	0x1307,
	0xC00E,
	0x9387,
	0xC701,
	0xB785,
	0x0024,
	0x3755,
	0x0020,
	0xBA97,
	0x0146,
	0x3777,
	0x0024,
	0x9385,
	0x4507,
	0x1305,
	0xC504,
	0x2320,
	0xF73C,
	0x9710,
	0x00FC,
	0xE780,
	0x603C,
	0xB787,
	0x0024,
	0x23A0,
	0xA710,
	0x4928,
	0xE177,
	0x3747,
	0x0024,
	0x9387,
	0x5776,
	0x2317,
	0xF782,
	0x1743,
	0x01FC,
	0x6700,
	0xE3F0,
	0x1743,
	0x01FC,
	0xE702,
	0x83EC,
	0xB787,
	0x0024,
	0x83A4,
	0x0710,
	0xAA89,
	0x2E8A,
	0x0146,
	0xA685,
	0x1145,
	0x9780,
	0xFFFB,
	0xE780,
	0xA031,
	0xB787,
	0x0024,
	0x03C7,
	0x4710,
	0x3E84,
	0x0149,
	0x11CF,
	0x3767,
	0x0024,
	0x0357,
	0x2777,
	0xB777,
	0x0024,
	0x9387,
	0x07C7,
	0x0E07,
	0x03D9,
	0x871C,
	0x2394,
	0xE71C,
	0xD285,
	0x4E85,
	0x97D0,
	0xFFFB,
	0xE780,
	0xA0F8,
	0x8347,
	0x4410,
	0x89C7,
	0xB777,
	0x0024,
	0x239C,
	0x27E3,
	0x0546,
	0xA685,
	0x1145,
	0x9780,
	0xFFFB,
	0xE780,
	0xA02C,
	0x1743,
	0x01FC,
	0x6700,
	0xA3E8,
	0xE177,
	0x3747,
	0x0024,
	0x9387,
	0x5776,
	0x2318,
	0xF782,
	0x8280,
	0x0000,
	0x0000,
	0x0000,
	0x0000,
	0x0000
};

static kal_uint16 addr_data_global_jn1[] = {
	0x6000, 0x0005,
	0xFCFC, 0x2400,
	0x0650, 0x0006,
	0x0654, 0x0000,
	0x065A, 0x0000,
	0x0674, 0x0005,
	0x0676, 0x0005,
	0x0678, 0x0005,
	0x067A, 0x0005,
	0x0684, 0x0140,
	0x0688, 0x0140,
	0x06BC, 0x0110,
	0x0A76, 0x0010,
	0x0AEE, 0x0010,
	0x0B66, 0x0010,
	0x0BDE, 0x0010,
	0x0C56, 0x0010,
	0x0CF2, 0x0100,
	0x120E, 0x0010,
	0x1236, 0x0000,
	0x1354, 0x0001,
	0x1356, 0x1770,
	0x1378, 0x3860,
	0x137A, 0x3870,
	0x137C, 0x3880,
	0x1386, 0x000B,
	0x13B2, 0x0000,
	0x1A0A, 0x0A4C,
	0x1A0E, 0x0096,
	0x1A28, 0x004C,
	0x1B26, 0x806F,
	0x2042, 0x001A,
	0x2148, 0x0001,
	0x21E4, 0x0004,
	0x21EC, 0x0000,
	0x2210, 0x0134,
	0x222E, 0x0100,
	0x3570, 0x0000,
	0x7F6C, 0x0001,
	0xFCFC, 0x4000,
	0x0136, 0x001A,
	0x013E, 0x0000,
	0xF44E, 0x0011,
	0xF44C, 0x0B0B,
	0xF44A, 0x0006,
	0x0118, 0x0200,
	0x011A, 0x0100,
	0x6000, 0x0085
};
#endif

static void sensor_init(void)
{
	int ret = 0;
#ifdef IMGSENSOR_HW_PARAM
	struct cam_hw_param *hw_param = NULL;
#endif

	LOG_INF("%s", __func__);
	ret = write_cmos_sensor(0xFCFC, 0x4000);
	ret |= write_cmos_sensor(0x6010, 0x0001);

	mDELAY(5);

#if INDIRECT_BURST
	ret |= write_cmos_sensor(0x6226, 0x0001);
	ret |= write_cmos_sensor(0x6004, 0x0001);
	ret |= write_cmos_sensor(0x6028, 0x2400);
	ret |= write_cmos_sensor(0x602A, 0x801C);

	/* Start burst mode */
	ret |= burst_table_write_cmos_sensor(addr_data_burst_init_jn1,
		sizeof(addr_data_burst_init_jn1) / sizeof(kal_uint16));

	ret |= write_cmos_sensor(0x602A, 0x35CC);
	ret |= write_cmos_sensor(0x6F12, 0x1C80);
	ret |= write_cmos_sensor(0x6F12, 0x0024);
	ret |= table_write_cmos_sensor(addr_data_global_jn1,
		sizeof(addr_data_global_jn1) / sizeof(kal_uint16));
#else
	set_mode_setfile(IMGSENSOR_MODE_INIT);
#endif

#ifdef IMGSENSOR_HW_PARAM
	if (ret != 0) {
		imgsensor_sec_get_hw_param(&hw_param, S5KJN1_CAL_SENSOR_POSITION);
		if (hw_param)
			hw_param->i2c_sensor_err_cnt++;
	}
#endif
}

#if WRITE_SENSOR_CAL_FOR_HW_GGC
static void sensor_HW_GGC_write(void)
{
	int i = 0;
	char *rom_cal_buf = NULL;
	const struct rom_cal_addr *xtc_cal_base_addr = NULL;
	const struct rom_cal_addr *hw_ggc_cal_addr = NULL;

	const int search_cnt = 15; //max cnt of search
	int start_addr = 0;
	int data_size = 0;
	kal_uint16 write_data = 0;

	// TODO : sensor_dev_id needs to be updated according to actual id.
	int sensor_dev_id = IMGSENSOR_SENSOR_IDX_MAIN;

	LOG_INF("-E");

	if (IMGSENSOR_GET_CAL_BUF_BY_SENSOR_IDX(sensor_dev_id, &rom_cal_buf) < 0) {
		LOG_ERR("cal data is NULL, sensor_dev_id: %d", sensor_dev_id);
		return;
	}

	// get start addr of crosstalk cal
	xtc_cal_base_addr = IMGSENSOR_GET_CAL_ADDR_BY_SENSOR_IDX(sensor_dev_id, GET_CAL_CROSSTALK);
	if (xtc_cal_base_addr == NULL) {
		LOG_ERR("[%s] crosstalk cal is NULL", __func__);
		return;
	}

	i = 0;
	//search addr and size of HW GGC
	hw_ggc_cal_addr = xtc_cal_base_addr;
	while (i < search_cnt) {
		if (hw_ggc_cal_addr == NULL) {
			LOG_ERR("Failed to find HW GGC cal");
			return;
		}

		if (hw_ggc_cal_addr->name == CROSSTALK_CAL_HW_GGC) {
			start_addr = hw_ggc_cal_addr->addr;
			data_size  = hw_ggc_cal_addr->size;
			LOG_INF("HW GGC, start addr: %#06x, size: %d, search_cnt: %d", start_addr, data_size, i);
			break;
		}
		hw_ggc_cal_addr = hw_ggc_cal_addr->next;
		i++;
	}
	if (i >= search_cnt) {
		LOG_ERR("Failed to find HW GCC cal, search cnt is over");
		return;
	}

	write_cmos_sensor(0x6028, 0x2400);
	write_cmos_sensor(0x602A, 0x0CFC);

	//write cal data of HW GGC
	rom_cal_buf += start_addr;

	for (i = 0; i < (data_size / 2); i++) {
		//byte swap for writing big endian data
		write_data = ((rom_cal_buf[2*i] << 8) | rom_cal_buf[2*i + 1]);
		write_cmos_sensor(0x6F12, write_data);
		LOG_DBG("write data[%d]: %#06x", i, write_data);
	}
	LOG_INF("done writing %d bytes", data_size);

	write_cmos_sensor(0x6028, 0x2400);
	write_cmos_sensor(0x602A, 0x2138);
	write_cmos_sensor(0x6F12, 0x4000);

	LOG_INF("-X");
}
#endif

int sensor_get_XTC_CAL_data(char *reordered_xtc_cal, unsigned int buf_len)
{
	char *rom_cal_buf = NULL;
	int const MAX_COUNT = 15;
	int const XTC_NUM = 4;
	enum crosstalk_cal_name const XTC_ORDER[XTC_NUM] = {
		CROSSTALK_CAL_TETRA_XTC,
		CROSSTALK_CAL_SENSOR_XTC,
		CROSSTALK_CAL_PD_XTC,
		CROSSTALK_CAL_SW_GGC
	};

	const struct rom_cal_addr *xtc_cal_addr = NULL;
	const int HEADER_SIZE = sizeof(unsigned short);
	int i, timeout, cur_size, cur_addr;

	// TODO : sensor_dev_id needs to be updated according to actual id.
	int sensor_dev_id = IMGSENSOR_SENSOR_IDX_MAIN;

	enum crosstalk_cal_name target_xtc_name;
	const struct rom_cal_addr *cur_xtc_cal;
	int reordered_idx = 0;
	unsigned short total_size = 0;

	if (IMGSENSOR_GET_CAL_BUF_BY_SENSOR_IDX(sensor_dev_id, &rom_cal_buf) < 0) {
		LOG_ERR("[%s] rom_cal_buf is NULL", __func__);
		return -1;
	}

	// get crosstalk cal
	xtc_cal_addr = IMGSENSOR_GET_CAL_ADDR_BY_SENSOR_IDX(sensor_dev_id, GET_CAL_CROSSTALK);

	if (xtc_cal_addr == NULL) {
		LOG_ERR("[%s] crosstalk cal is NULL", __func__);
		return -1;
	}

	//header size
	reordered_idx += HEADER_SIZE;

	// copy crosstalk_cal by XTC_ORDER
	for (i = 0; i < XTC_NUM; i++) {
		target_xtc_name = XTC_ORDER[i];
		cur_xtc_cal = xtc_cal_addr;
		timeout = 0;
		while (1) {
			if (++timeout >= MAX_COUNT || cur_xtc_cal == NULL) {
				LOG_ERR("[%s] Failed to find crosstalk_cal - target_xtc_name: %d", __func__, target_xtc_name);
				return -1;
			}

			if (cur_xtc_cal->name == target_xtc_name) {
				cur_addr = cur_xtc_cal->addr;
				cur_size = cur_xtc_cal->size;
				LOG_INF("[%s] reordered_xtc_cal[%x] = rom_cal_buf[%x], val: %x, size: %x"
					, __func__, reordered_idx, cur_addr, rom_cal_buf[cur_addr], cur_size);
				if (reordered_idx + cur_size > buf_len) {
					LOG_ERR("[%s] xtc cal is bigger than buffer: %d + %d > %d", __func__, reordered_idx, cur_size, buf_len);
					return -1;
				}
				memcpy(&reordered_xtc_cal[reordered_idx], &rom_cal_buf[cur_addr], cur_size);
				reordered_idx += cur_size;
				break;
			}
			cur_xtc_cal = cur_xtc_cal->next;
		}
	}

	//total size
	total_size = reordered_idx - HEADER_SIZE;
	memcpy(reordered_xtc_cal, &total_size, sizeof(total_size));
	return 0;
}

#if XTC_CAL_DEBUG
void test_sensor_get_xtc_cal_data(void)
{
	char *rom_cal_buf = NULL;
	char *ans = NULL;
	char *result = NULL;
	unsigned short before_size = 0x21DF - 0x0140 + 1;
	unsigned short ans_size = before_size - (0x14AB - 0x1352 + 1); //xtc - hw_ggc
	unsigned short result_size = 0;
	int xtc_order[4][2] = {
		{0x14AC, (0x1EDF - 0x14AC + 1)}, // tetra xtc
		{0x1EE0, (0x21DF - 0x1EE0 + 1)}, // sensor xtc
		{0x0140, (0x10DF - 0x0140 + 1)}, // pdxtc
		{0x10E0, (0x1351 - 0x10E0 + 1)}, // sw ggc
	};
	// TODO : sensor_dev_id needs to be updated according to actual id.
	int sensor_dev_id = IMGSENSOR_SENSOR_IDX_MAIN;
	int i, addr, size;
	int idx = 0;

	// get ans
	if (IMGSENSOR_GET_CAL_BUF_BY_SENSOR_IDX(sensor_dev_id, &rom_cal_buf) < 0)
		LOG_ERR("[%s] rom_cal_buf is NULL", __func__);

	ans = kmalloc(ans_size, GFP_KERNEL);
	result = kmalloc(ans_size + sizeof(unsigned short), GFP_KERNEL);

	for (i = 0; i < 4; i++) {
		addr = xtc_order[i][0];
		size = xtc_order[i][1];
		memcpy(&ans[idx], &rom_cal_buf[addr], size);
		LOG_INF("[%s] ans[%x]= rom_cal_buf[%x] size: %x", __func__, idx, addr, size);
		idx += size;
	}

	// test function
	sensor_get_XTC_CAL_data(result, ans_size + sizeof(unsigned short));

	// check data
	if (memcmp(ans, &result[sizeof(unsigned short)], ans_size) != 0)
		LOG_ERR("[%s] ans != result", __func__);
	else
		LOG_INF("[%s] ans == result", __func__);

	// check size
	memcpy(&result_size, result, sizeof(unsigned short));
	if (ans_size != result_size)
		LOG_ERR("[%s] ans_size != result_size", __func__);
	else
		LOG_INF("[%s] ans_size == result_size", __func__);

	LOG_INF("[%s] TEST DONE!! ans_size: %hd, result_size: %hd", __func__, ans_size, result_size);
	LOG_INF("[%s] ans[0]: %x, ans[0xa34]: %x, ans[0xd34]: %x, ans[0x1cd4]: %x, "
			, __func__, ans[0], ans[0xa34], ans[0xd34], ans[0x1cd4]);

	kfree(ans);
	kfree(result);
}
#endif

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
			sensor_id = ((read_cmos_sensor_8(0x0000) << 8) | read_cmos_sensor_8(0x0001));
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

#if WRITE_SENSOR_CAL_FOR_HW_GGC
	sensor_HW_GGC_write();
#endif

	spin_lock(&imgsensor_drv_lock);

	imgsensor.autoflicker_en	= KAL_FALSE;
	imgsensor.sensor_mode		= IMGSENSOR_MODE_INIT;
	imgsensor.shutter			= 0x3D0;
	imgsensor.gain				= 0x100;
	imgsensor.pclk				= imgsensor_info.pre.pclk;
	imgsensor.frame_length		= imgsensor_info.pre.framelength;
	imgsensor.line_length		= imgsensor_info.pre.linelength;
	imgsensor.min_frame_length	= imgsensor_info.pre.framelength;
	imgsensor.dummy_pixel		= 0;
	imgsensor.dummy_line		= 0;
	imgsensor.ihdr_mode			= 0;
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
	LOG_INF("close() E\n");

	streaming_control(false);
	sIsNightHyperlapse = KAL_FALSE;
	enable_adaptive_mipi = false;

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
	LOG_INF("preview E\n");

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode			= IMGSENSOR_MODE_PREVIEW;
	imgsensor.pclk					= imgsensor_info.pre.pclk;
	imgsensor.line_length			= imgsensor_info.pre.linelength;
	imgsensor.frame_length			= imgsensor_info.pre.framelength;
	imgsensor.min_frame_length		= imgsensor_info.pre.framelength;
	imgsensor.autoflicker_en		= KAL_FALSE;
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
	LOG_INF("capture E\n");

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_CAPTURE;
	imgsensor.pclk = imgsensor_info.cap.pclk;
	imgsensor.line_length = imgsensor_info.cap.linelength;
	imgsensor.frame_length = imgsensor_info.cap.framelength;
	imgsensor.min_frame_length = imgsensor_info.cap.framelength;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);

	set_mode_setfile(imgsensor.sensor_mode);

	set_mirror_flip(imgsensor.mirror);

	return ERROR_NONE;
}

static kal_uint32 normal_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
	MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("normal_video E\n");

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode			= IMGSENSOR_MODE_VIDEO;
	imgsensor.pclk					= imgsensor_info.normal_video.pclk;
	imgsensor.line_length			= imgsensor_info.normal_video.linelength;
	imgsensor.frame_length			= imgsensor_info.normal_video.framelength;
	imgsensor.min_frame_length		= imgsensor_info.normal_video.framelength;
	imgsensor.autoflicker_en		= KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);

	set_mode_setfile(imgsensor.sensor_mode);

	set_mirror_flip(imgsensor.mirror);

	return ERROR_NONE;
}

static kal_uint32 hs_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			   MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("hs_video E\n");

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode			= IMGSENSOR_MODE_HIGH_SPEED_VIDEO;
	imgsensor.pclk					= imgsensor_info.hs_video.pclk;
	imgsensor.line_length			= imgsensor_info.hs_video.linelength;
	imgsensor.frame_length			= imgsensor_info.hs_video.framelength;
	imgsensor.min_frame_length		= imgsensor_info.hs_video.framelength;

	imgsensor.dummy_line			= 0;
	imgsensor.dummy_pixel			= 0;
	imgsensor.autoflicker_en		= KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);

	set_mode_setfile(imgsensor.sensor_mode);

	set_mirror_flip(imgsensor.mirror);

	return ERROR_NONE;
}

static kal_uint32 slim_video(
	MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
	MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("%s - E\n", __func__);

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode			= IMGSENSOR_MODE_SLIM_VIDEO;
	imgsensor.pclk					= imgsensor_info.slim_video.pclk;
	imgsensor.line_length			= imgsensor_info.slim_video.linelength;
	imgsensor.frame_length			= imgsensor_info.slim_video.framelength;
	imgsensor.min_frame_length		= imgsensor_info.slim_video.framelength;

	imgsensor.dummy_line			= 0;
	imgsensor.dummy_pixel			= 0;
	imgsensor.autoflicker_en		= KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);

	set_mode_setfile(imgsensor.sensor_mode);

	set_mirror_flip(imgsensor.mirror);

	return ERROR_NONE;
}

static kal_uint32 custom1(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("E cur fps: %d\n", imgsensor.current_fps);

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode			= IMGSENSOR_MODE_CUSTOM1;
	imgsensor.pclk					= imgsensor_info.custom1.pclk;
	imgsensor.line_length			= imgsensor_info.custom1.linelength;
	imgsensor.frame_length			= imgsensor_info.custom1.framelength;
	imgsensor.min_frame_length		= imgsensor_info.custom1.framelength;
	imgsensor.autoflicker_en		= KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);

	set_mode_setfile(imgsensor.sensor_mode);

	set_mirror_flip(IMAGE_NORMAL);

	return ERROR_NONE;
}

static kal_uint32 custom2(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
				MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("E cur fps: %d\n", imgsensor.current_fps);

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_CUSTOM2;
	imgsensor.pclk = imgsensor_info.custom2.pclk;
	imgsensor.line_length = imgsensor_info.custom2.linelength;
	imgsensor.frame_length = imgsensor_info.custom2.framelength;
	imgsensor.min_frame_length = imgsensor_info.custom2.framelength;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);

	set_mode_setfile(imgsensor.sensor_mode);

	set_mirror_flip(IMAGE_NORMAL);

	return ERROR_NONE;
}

static kal_uint32 custom3(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
				MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("E cur fps: %d\n", imgsensor.current_fps);

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_CUSTOM3;
	imgsensor.pclk = imgsensor_info.custom3.pclk;
	imgsensor.line_length = imgsensor_info.custom3.linelength;
	imgsensor.frame_length = imgsensor_info.custom3.framelength;
	imgsensor.min_frame_length = imgsensor_info.custom3.framelength;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);

	set_mode_setfile(imgsensor.sensor_mode);

	set_mirror_flip(IMAGE_NORMAL);

	return ERROR_NONE;
}

static kal_uint32 custom4(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
				MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("E cur fps: %d\n", imgsensor.current_fps);

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_CUSTOM4;
	imgsensor.pclk = imgsensor_info.custom4.pclk;
	imgsensor.line_length = imgsensor_info.custom4.linelength;
	imgsensor.frame_length = imgsensor_info.custom4.framelength;
	imgsensor.min_frame_length = imgsensor_info.custom4.framelength;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);

	set_mode_setfile(imgsensor.sensor_mode);

	set_mirror_flip(IMAGE_NORMAL);

	return ERROR_NONE;
}

static kal_uint32 custom5(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
				MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("E cur fps: %d\n", imgsensor.current_fps);

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_CUSTOM5;
	imgsensor.pclk = imgsensor_info.custom5.pclk;
	imgsensor.line_length = imgsensor_info.custom5.linelength;
	imgsensor.frame_length = imgsensor_info.custom5.framelength;
	imgsensor.min_frame_length = imgsensor_info.custom5.framelength;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);

	set_mode_setfile(imgsensor.sensor_mode);

	set_mirror_flip(IMAGE_NORMAL);

	return ERROR_NONE;
}

static kal_uint32 get_resolution(
	MSDK_SENSOR_RESOLUTION_INFO_STRUCT(*sensor_resolution))
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

	sensor_resolution->SensorCustom5Width         = imgsensor_info.custom5.grabwindow_width;
	sensor_resolution->SensorCustom5Height        = imgsensor_info.custom5.grabwindow_height;

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
	sensor_info->Custom5DelayFrame         = imgsensor_info.custom5_delay_frame;
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
	sensor_info->TEMPERATURE_SUPPORT       = imgsensor_info.temperature_support;
	sensor_info->MIPIDataLowPwr2HighSpeedTermDelayCount = 0;
	sensor_info->MIPICLKLowPwr2HighSpeedTermDelayCount  = 0;
	sensor_info->SensorWidthSampling       = 0;
	sensor_info->SensorHightSampling       = 0;
	sensor_info->SensorPacketECCOrder      = 1;

#ifdef PDAF_DISABLE
	sensor_info->PDAF_Support = PDAF_SUPPORT_NA;
#else
	sensor_info->PDAF_Support = PDAF_SUPPORT_CAMSV;
#endif

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
	LOG_INF("scenario_id = %d\n", scenario_id);

	spin_lock(&imgsensor_drv_lock);
	imgsensor.current_scenario_id = scenario_id;
	spin_unlock(&imgsensor_drv_lock);

	switch (scenario_id) {
	case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		LOG_INF("%s:MSDK_SCENARIO_ID_CAMERA_PREVIEW scenario_id= %d\n", __func__, scenario_id);
		preview(image_window, sensor_config_data);
		break;
	case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		LOG_INF("%s:MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG scenario_id= %d\n", __func__, scenario_id);
		capture(image_window, sensor_config_data);
		break;
	case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
		LOG_INF("%s:MSDK_SCENARIO_ID_VIDEO_PREVIEW scenario_id= %d\n", __func__, scenario_id);
		normal_video(image_window, sensor_config_data);
		break;
	case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
		LOG_INF("%s:MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO scenario_id= %d\n", __func__, scenario_id);
		hs_video(image_window, sensor_config_data);
		break;
	case MSDK_SCENARIO_ID_SLIM_VIDEO:
		LOG_INF("%s:MSDK_SCENARIO_ID_SLIM_VIDEO scenario_id= %d\n", __func__, scenario_id);
		slim_video(image_window, sensor_config_data);
		break;

	case MSDK_SCENARIO_ID_CUSTOM1:
		LOG_INF("%s:MSDK_SCENARIO_ID_CUSTOM1 scenario_id= %d\n", __func__, scenario_id);
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
		LOG_ERR("Error ScenarioId setting");
		preview(image_window, sensor_config_data);
		return ERROR_INVALID_SCENARIO_ID;
	}
	return ERROR_NONE;
}

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

static kal_uint32 set_auto_flicker_mode(
	kal_bool enable, UINT16 framerate)
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
		? (frame_length - imgsensor_info.hs_video.  framelength) : 0;

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
	case MSDK_SCENARIO_ID_CUSTOM5:
		LOG_DBG("MSDK_SCENARIO_ID_CUSTOM5 scenario_id= %d\n", scenario_id);
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
		LOG_INF("error scenario_id = %d, we use preview scenario\n", scenario_id);
		break;
	}
	return ERROR_NONE;
}


static kal_uint32 get_default_framerate_by_scenario(
	enum MSDK_SCENARIO_ID_ENUM scenario_id, MUINT32 *framerate)
{
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
	LOG_INF("scenario_id = %d, framerate = %d", scenario_id, *framerate);
	return ERROR_NONE;
}

static kal_uint32 set_test_pattern_mode(kal_bool enable)
{
	LOG_INF("enable: %d\n", enable);

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

	LOG_DBG("temp_c(%d), read_reg(%d), enable %d\n", temperature_convert, temperature, read_cmos_sensor_8(0x0138));

	return temperature_convert;
}

#if defined(CONFIG_TRAN_CAMERA_SYNC_AWB_TO_KERNEL)
static void set_awbgain(kal_uint32 g_gain, kal_uint32 r_gain, kal_uint32 b_gain)
{
	kal_uint32 r_gain_int = 0x0;
	kal_uint32 b_gain_int = 0x0;

	r_gain_int = r_gain / 2;
	b_gain_int = b_gain / 2;

	LOG_INF("%s r_gain=0x%x , b_gain=0x%x\n", __func__, r_gain, b_gain);

	write_cmos_sensor(0x0D82, r_gain_int);
	write_cmos_sensor(0x0D86, b_gain_int);
}
#endif

static kal_uint32 feature_control(MSDK_SENSOR_FEATURE_ENUM feature_id,
				  UINT8 *feature_para, UINT32 *feature_para_len)
{
	UINT16 *feature_return_para_16	= (UINT16 *) feature_para;
	UINT16 *feature_data_16			= (UINT16 *) feature_para;
	UINT32 *feature_return_para_32	= (UINT32 *) feature_para;
	UINT32 *feature_data_32			= (UINT32 *) feature_para;
	INT32 *feature_return_para_i32	= (INT32 *) feature_para;

	unsigned long long *feature_data = (unsigned long long *)feature_para;
	struct SENSOR_WINSIZE_INFO_STRUCT *wininfo;
	struct SET_PD_BLOCK_INFO_T *PDAFinfo;
	struct SENSOR_VC_INFO_STRUCT *pvcinfo;
	char *XTC_CAL;
	unsigned int xtc_buf_size;

	MSDK_SENSOR_REG_INFO_STRUCT *sensor_reg_data =
		(MSDK_SENSOR_REG_INFO_STRUCT *) feature_para;

	LOG_DBG("feature_id = %d\n", feature_id);

	switch (feature_id) {
	case SENSOR_FEATURE_GET_PERIOD:
		LOG_DBG("SENSOR_FEATURE_GET_PERIOD = %d\n", feature_id);
		*feature_return_para_16++	= imgsensor.line_length;
		*feature_return_para_16		= imgsensor.frame_length;
		*feature_para_len			= 4;
		break;
	case SENSOR_FEATURE_GET_PIXEL_CLOCK_FREQ:
		LOG_DBG("SENSOR_FEATURE_GET_PIXEL_CLOCK_FREQ = %d\n", feature_id);
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
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		default:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
			= (imgsensor_info.pre.framelength << 16)
				+ imgsensor_info.pre.linelength;
			break;
		}
		break;
	case SENSOR_FEATURE_SET_ESHUTTER:
		LOG_DBG("SENSOR_FEATURE_SET_ESHUTTER = %d\n", feature_id);
		set_shutter(*feature_data);
		break;
	case SENSOR_FEATURE_SET_NIGHTMODE:
		LOG_INF("SENSOR_FEATURE_SET_NIGHTMODE = %d, data=%d\n", feature_id, *feature_data);
		if (*feature_data)
			sIsNightHyperlapse = KAL_TRUE;
		else
			sIsNightHyperlapse = KAL_FALSE;
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
		break;
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
		case MSDK_SCENARIO_ID_CUSTOM5:
			LOG_DBG("MSDK_SCENARIO_ID_CUSTOM5 = %d\n", feature_id);
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
	case SENSOR_FEATURE_GET_PDAF_INFO:
		LOG_INF("SENSOR_FEATURE_GET_PDAF_INFO, scenario id:%d\n", (UINT16)*feature_data);
		PDAFinfo = (struct SET_PD_BLOCK_INFO_T *)(uintptr_t)(*(feature_data+1));

		switch (*feature_data) {
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		case MSDK_SCENARIO_ID_CUSTOM1:
		case MSDK_SCENARIO_ID_CUSTOM2:
		case MSDK_SCENARIO_ID_CUSTOM3:
		case MSDK_SCENARIO_ID_CUSTOM4:
		case MSDK_SCENARIO_ID_CUSTOM5:
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
			LOG_INF("MSDK_SCENARIO_ID_CAMERA_PREVIEW:%d\n", MSDK_SCENARIO_ID_CAMERA_PREVIEW);
			/* TODO */
			imgsensor_pd_info.i4BlockNumX = 508;
			imgsensor_pd_info.i4BlockNumY = 382;
			memcpy((void *)PDAFinfo,
			(void *)&imgsensor_pd_info,
			sizeof(struct SET_PD_BLOCK_INFO_T));
			break;
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			LOG_INF("MSDK_SCENARIO_ID_VIDEO_PREVIEW:%d\n", MSDK_SCENARIO_ID_VIDEO_PREVIEW);
			/* TODO */
			imgsensor_pd_info.i4BlockNumX = 508;
			imgsensor_pd_info.i4BlockNumY = 286;
			memcpy((void *)PDAFinfo,
			(void *)&imgsensor_pd_info,
			sizeof(struct SET_PD_BLOCK_INFO_T));
			break;
		default:
			break;
		}
		break;
	case SENSOR_FEATURE_GET_SENSOR_PDAF_CAPACITY:
		LOG_INF("SENSOR_FEATURE_GET_SENSOR_PDAF_CAPACITY scenarioId:%d\n",
			(UINT16) *feature_data);
		/* PDAF capacity enable or not */
#ifdef PDAF_DISABLE
		*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) = 0;
#else
		switch (*feature_data) {
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) = 1;
			break;
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			/* video & capture use same setting */
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) = 1;
			break;
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) = 0;
			break;
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) = 0;
			break;
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) = 1;
			break;
		case MSDK_SCENARIO_ID_CUSTOM1:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) = 0;
			break;
		case MSDK_SCENARIO_ID_CUSTOM2:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) = 0;
			break;
		case MSDK_SCENARIO_ID_CUSTOM3:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) = 0;
			break;
		case MSDK_SCENARIO_ID_CUSTOM4:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) = 0;
			break;
		case MSDK_SCENARIO_ID_CUSTOM5:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) = 0;
			break;
		default:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) = 0;
			break;
		}
#endif
		break;
	case SENSOR_FEATURE_GET_VC_INFO:
		LOG_INF("SENSOR_FEATURE_GET_VC_INFO, scenario id = %d\n", (UINT16) *feature_data);
		pvcinfo = (struct SENSOR_VC_INFO_STRUCT *) (uintptr_t) (*(feature_data + 1));

		switch (*feature_data_32) {
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			memcpy((void *)pvcinfo, (void *)&SENSOR_VC_INFO[1],
				   sizeof(struct SENSOR_VC_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			memcpy((void *)pvcinfo, (void *)&SENSOR_VC_INFO[2],
				   sizeof(struct SENSOR_VC_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
			memcpy((void *)pvcinfo, (void *)&SENSOR_VC_INFO[3],
				   sizeof(struct SENSOR_VC_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
			memcpy((void *)pvcinfo, (void *)&SENSOR_VC_INFO[0],
				   sizeof(struct SENSOR_VC_INFO_STRUCT));
			break;
		default:
			break;
		}
		break;
	case SENSOR_FEATURE_SET_IHDR_SHUTTER_GAIN:
		LOG_DBG("SENSOR_FEATURE_SET_IHDR_SHUTTER_GAIN = %d\n", feature_id);
		LOG_DBG("SENSOR_SET_SENSOR_IHDR LE=%d, SE=%d, Gain=%d\n",
			(UINT16) *feature_data,
			(UINT16) *(feature_data + 1),
			(UINT16) *(feature_data + 2));

/* ihdr_write_shutter_gain(	(UINT16)*feature_data,
 *				(UINT16)*(feature_data+1),
 *				(UINT16)*(feature_data+2));
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
		LOG_INF("SENSOR_FEATURE_SET_HDR_SHUTTER LE=%d, SE=%d\n",
			(UINT16) *feature_data, (UINT16) *(feature_data + 1));
		break;

	case SENSOR_FEATURE_SET_SHUTTER_FRAME_TIME:/*lzl*/
			set_shutter_frame_length((UINT32)*feature_data,
					(UINT32)*(feature_data+1));
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
	case SENSOR_FEATURE_GET_TEMPERATURE_VALUE:
		LOG_DBG("SENSOR_FEATURE_GET_TEMPERATURE_VALUE = %d\n", feature_id);
		*feature_return_para_i32	= get_sensor_temperature();
		*feature_para_len			= 4;
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
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) = (imgsensor_info.custom2.pclk /
			(imgsensor_info.custom2.linelength - 80)) * imgsensor_info.custom2.grabwindow_width;
			break;
		case MSDK_SCENARIO_ID_CUSTOM3:
			LOG_INF("MSDK_SCENARIO_ID_CUSTOM3 = %d\n", *feature_data);
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) = (imgsensor_info.custom3.pclk /
			(imgsensor_info.custom3.linelength - 80)) * imgsensor_info.custom3.grabwindow_width;
			break;
		case MSDK_SCENARIO_ID_CUSTOM4:
			LOG_INF("MSDK_SCENARIO_ID_CUSTOM4 = %d\n", *feature_data);
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) = (imgsensor_info.custom4.pclk /
			(imgsensor_info.custom4.linelength - 80)) * imgsensor_info.custom4.grabwindow_width;
			break;
		case MSDK_SCENARIO_ID_CUSTOM5:
			LOG_INF("MSDK_SCENARIO_ID_CUSTOM5 = %d\n", *feature_data);
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) = (imgsensor_info.custom5.pclk /
			(imgsensor_info.custom5.linelength - 80)) * imgsensor_info.custom5.grabwindow_width;
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
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
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
		case MSDK_SCENARIO_ID_CUSTOM5:
		default:
			*feature_return_para_32 = 2;
			break;
		}
		LOG_INF("scenario_id %d, binning_type %d,\n", *(feature_data + 1), *feature_return_para_32);
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
	case SENSOR_FEATURE_GET_4CELL_DATA:
		LOG_INF("SENSOR_FEATURE_GET_4CELL_DATA\n");
		XTC_CAL = (char *)(*(feature_data + 1));
		xtc_buf_size = *(feature_data + 2);
#if XTC_CAL_DEBUG
		test_sensor_get_xtc_cal_data();
#endif
		if (sensor_get_XTC_CAL_data(XTC_CAL, xtc_buf_size) < 0) {
			LOG_ERR("Failed to get XTC cal");
			return ERROR_SENSOR_FEATURE_NOT_IMPLEMENT;
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

UINT32 S5KJN1_MIPI_RAW_SensorInit(struct SENSOR_FUNCTION_STRUCT **pfFunc)
{
	/* To Do : Check Sensor status here */
	if (pfFunc != NULL)
		*pfFunc = &sensor_func;
	return ERROR_NONE;
}
