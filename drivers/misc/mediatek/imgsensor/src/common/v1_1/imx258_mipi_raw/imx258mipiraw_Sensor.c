/*
 * Copyright (C) 2015 MediaTek Inc.
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
 *	 IMX258mipiraw_Sensor.c
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
#define PFX "IMX258 D/D"
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
#include <linux/atomic.h>
#include <linux/types.h>

#include "kd_camera_typedef.h"
#include "kd_imgsensor.h"
#include "kd_imgsensor_define.h"
#include "kd_imgsensor_errcode.h"

#include "imx258mipiraw_Sensor.h"

#define MULTI_WRITE 1
#if MULTI_WRITE
static const int I2C_BUFFER_LEN = 1020;
#else
static const int I2C_BUFFER_LEN = 4;
#endif

static DEFINE_SPINLOCK(imgsensor_drv_lock);

#define SENSOR_IMX258_COARSE_INTEGRATION_TIME_MAX_MARGIN       (5)
#define SENSOR_IMX258_MAX_COARSE_INTEGRATION_TIME              (65535)

static kal_bool sIsNightHyperlapse = KAL_FALSE;
static kal_uint32 streaming_control(kal_bool enable);
/*
 * Image Sensor Scenario
 * pre : FOV 80 4:3
 * cap : FOV 80 Check Active Array
 * normal_video : FOV 80 16:9
 * hs_video: NOT USED
 * slim_video: NOT USED
 * custom1 : FAST AE
 * custom2 : NOT USED
 * custom3 : NOT USED
 * custom4 : NOT USED
 */

static struct imgsensor_info_struct imgsensor_info = {

	/* IMX258MIPI_SENSOR_ID, sensor_id = 0x2680*/
	.sensor_id = IMX258_SENSOR_ID,

	.checksum_value = 0x38ebe79e, /* checksum value for Camera Auto Test */

	.pre = {
		.pclk = 520000000,
		.linelength = 5352,
		.framelength = 3236,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 4128,
		.grabwindow_height = 3096,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 436800000,
		.max_framerate = 300,
	},
	.cap = {
		.pclk = 520000000,
		.linelength = 5352,
		.framelength = 3236,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 4128,
		.grabwindow_height = 3096,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 436800000,
		.max_framerate = 300,
	},
	.normal_video = {
		.pclk = 520000000,
		.linelength = 5352,
		.framelength = 3236,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 4128,
		.grabwindow_height = 2324,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 436800000,
		.max_framerate = 300,
	},
	.hs_video = {
		.pclk = 520000000,
		.linelength = 5352,
		.framelength = 3236,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 4128,
		.grabwindow_height = 3096,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 436800000,
		.max_framerate = 300,
	},
	.slim_video = {
		.pclk = 520000000,
		.linelength = 5352,
		.framelength = 3236,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 4128,
		.grabwindow_height = 3096,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 436800000,
		.max_framerate = 300,
	},
	.custom1 = {
		.pclk = 520000000,
		.linelength = 5352,
		.framelength = 1616,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 1024,
		.grabwindow_height = 768,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 436800000,
		.max_framerate = 600,
	},
	.custom2 = {
		.pclk = 520000000,
		.linelength = 5352,
		.framelength = 3236,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 3712,
		.grabwindow_height = 2556,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 436800000,
		.max_framerate = 300,
	},
	.custom3 = {
		.pclk = 520000000,
		.linelength = 5352,
		.framelength = 3236,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 3712,
		.grabwindow_height = 2556,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 436800000,
		.max_framerate = 300,
		},
	.custom4 = {
		.pclk = 520000000,
		.linelength = 5352,
		.framelength = 3236,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 3712,
		.grabwindow_height = 2556,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 436800000,
		.max_framerate = 300,
	},
	.margin = 4,		/* sensor framelength & shutter margin */
	.min_shutter = 1,	/* 1,          //min shutter */
	.min_gain = 64,
	.max_gain = 1024,
	.min_gain_iso = 33,	/*A22e-5G Wide Min ISO*/
	.exp_step = 2,
	.gain_step = 1,
	.gain_type = 0,
	/* max framelength by sensor register's limitation */
	.max_frame_length = 0x7fff,
	.ae_shut_delay_frame = 0,

	/* shutter delay frame for AE cycle,
	 * 2 frame with ispGain_delay-shut_delay=2-0=2
	 */
	.ae_sensor_gain_delay_frame = 0,

	/* sensor gain delay frame for AE cycle,
	 * 2 frame with ispGain_delay-sensor_gain_delay=2-0=2
	 */
	.ae_ispGain_delay_frame = 2,	/* isp gain delay frame for AE cycle */

	/* The delay frame of setting frame length	*/
	.frame_time_delay_frame = 2,

	.ihdr_support = 1,	/* 1, support; 0,not support */
	.ihdr_le_firstline = 0,	/* 1,le first ; 0, se first */
	.sensor_mode_num = 7,	/* support sensor mode num */

	.cap_delay_frame = 2,	/* enter capture delay frame num */
	.pre_delay_frame = 2,	/* enter preview delay frame num */
	.video_delay_frame = 2,	/* enter video delay frame num */
	.hs_video_delay_frame = 2, /* enter high speed video  delay frame num */
	.slim_video_delay_frame = 2,	/* enter slim video delay frame num */
	.custom1_delay_frame = 2,	/* add new mode */
	.custom2_delay_frame = 2,
	.custom3_delay_frame = 2,
	.custom4_delay_frame = 2,
	.isp_driving_current = ISP_DRIVING_4MA,	/* mclk driving current */

	/* sensor_interface_type */
	.sensor_interface_type = SENSOR_INTERFACE_TYPE_MIPI,

	/* 0,MIPI_OPHY_NCSI2;  1,MIPI_OPHY_CSI2 */
	.mipi_sensor_type = MIPI_OPHY_NCSI2,

	.mipi_settle_delay_mode = MIPI_SETTLEDELAY_AUTO,
	.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_R,
	/* sensor output first pixel color */
	.mclk = 26,	/* mclk value, suggest 24 or 26 for 24Mhz or 26Mhz */
	.mipi_lane_num = SENSOR_MIPI_4_LANE,	/* mipi lane num */

/* record sensor support all write id addr, only supprt 4must end with 0xff */
	.i2c_speed = 400,
	.i2c_addr_table = {0x34, 0x20, 0xff},

};


static struct imgsensor_struct imgsensor = {
	.mirror = IMAGE_NORMAL,	/* mirrorflip information */
	.sensor_mode = IMGSENSOR_MODE_INIT,
	.shutter = 0x14d,	/* current shutter */
	.gain = 0xe000,		/* current gain */
	.dummy_pixel = 0,	/* current dummypixel */
	.dummy_line = 0,	/* current dummyline */

	/* full size current fps : 24fps for PIP, 30fps for Normal or ZSD */
	.current_fps = 300,
	.autoflicker_en = KAL_FALSE,
	.test_pattern = KAL_FALSE,

	/* current scenario id */
	.current_scenario_id = MSDK_SCENARIO_ID_CAMERA_PREVIEW,

	.ihdr_mode = 0,	/* sensor need support LE, SE with HDR feature */
	.i2c_write_id = 0x34,	/* record current sensor's i2c write id */
};

/* Sensor output window information */
/*according toIMX258 datasheet p53 image cropping*/
static struct SENSOR_WINSIZE_INFO_STRUCT imgsensor_winsize_info[7] = {
	{4208, 3120,    0,    0, 4208, 3120, 4208, 3120,
	   40,   12, 4128, 3096,    0,    0, 4128, 3096}, /* pre */
	{4208, 3120,    0,    0, 4208, 3120, 4208, 3120,
	   40,   12, 4128, 3096,    0,    0, 4128, 3096}, /* cap */
	{4208, 3120,    0,    0, 4208, 3120, 4208, 3120,
	   40,  398, 4128, 2324,    0,    0, 4128, 2324}, /* normal video */
	{4208, 3120,   56,   24, 4096, 3072, 1024,  768,
	 0000,    0, 1024,  768,    0,    0, 1024,  768}, /* custom1 */
	{4208, 3120,    0,  282, 4208, 2556, 4208, 2556,
	  248,    0, 3712, 2556,    0,    0, 3712, 2556}, /* custom2 : NOT USED*/
	{4208, 3120,    0,  282, 4208, 2556, 4208, 2556,
	  248,    0, 3712, 2556,    0,    0, 3712, 2556}, /* custom3 : NOT USED*/
	{4208, 3120,    0,  282, 4208, 2556, 4208, 2556,
	  248,    0, 3712, 2556,    0,    0, 3712, 2556}, /* custom4 : NOT USED*/
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
	/*
	 * LOG_DBG("dummyline = %d, dummypixels = %d\n",
	 * imgsensor.dummy_line, imgsensor.dummy_pixel);
	 */

	write_cmos_sensor_8(0x0104, 0x01);

	write_cmos_sensor_8(0x3002, 0x00);
	write_cmos_sensor_8(0x0340, imgsensor.frame_length >> 8);
	write_cmos_sensor_8(0x0341, imgsensor.frame_length & 0xFF);
	write_cmos_sensor_8(0x0342, imgsensor.line_length >> 8);
	write_cmos_sensor_8(0x0343, imgsensor.line_length & 0xFF);

	write_cmos_sensor_8(0x0104, 0x00);
}

static void set_max_framerate(UINT16 framerate, kal_bool min_framelength_en)
{
	/* kal_int16 dummy_line; */
	kal_uint32 frame_length = imgsensor.frame_length;
	/* unsigned long flags; */

	LOG_DBG("framerate = %d, min framelength should enable %d\n",
		framerate, min_framelength_en);

	frame_length = imgsensor.pclk / framerate * 10 / imgsensor.line_length;
	spin_lock(&imgsensor_drv_lock);
	imgsensor.frame_length =
		(frame_length > imgsensor.min_frame_length)
		? frame_length : imgsensor.min_frame_length;

	imgsensor.dummy_line =
		imgsensor.frame_length - imgsensor.min_frame_length;

	/* dummy_line = frame_length - imgsensor.min_frame_length; */
	/* if (dummy_line < 0) */
	/* imgsensor.dummy_line = 0; */
	/* else */
	/* imgsensor.dummy_line = dummy_line; */
	/* imgsensor.frame_length = frame_length + imgsensor.dummy_line; */
	if (imgsensor.frame_length > imgsensor_info.max_frame_length) {
		imgsensor.frame_length = imgsensor_info.max_frame_length;

		imgsensor.dummy_line =
			imgsensor.frame_length - imgsensor.min_frame_length;
	}
	if (min_framelength_en)
		imgsensor.min_frame_length = imgsensor.frame_length;
	spin_unlock(&imgsensor_drv_lock);
	set_dummy();
}				/*      set_max_framerate  */

static void write_shutter(kal_uint32 shutter)
{
	kal_uint16 realtime_fps = 0;
	kal_uint32 target_exp = 0;
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
	if (shutter < imgsensor_info.min_shutter)
		shutter = imgsensor_info.min_shutter;

	if (sIsNightHyperlapse) {
		// fps = 1/1.5 (Frame Duration : 1.5sec)
		// frame_length = pclk / (line_length * fps)
		imgsensor.frame_length = (((imgsensor.pclk / (1000 * 1000)) * (1500000))/imgsensor.line_length);
	}

	if (imgsensor.frame_length > SENSOR_IMX258_MAX_COARSE_INTEGRATION_TIME) {
		int max_coarse_integration_time =
			SENSOR_IMX258_MAX_COARSE_INTEGRATION_TIME - SENSOR_IMX258_COARSE_INTEGRATION_TIME_MAX_MARGIN;

		cit_lshift_val = (kal_uint32)(imgsensor.frame_length / SENSOR_IMX258_MAX_COARSE_INTEGRATION_TIME);
		while (cit_lshift_val >= 1) {
			cit_lshift_val /= 2;
			imgsensor.frame_length /= 2;
			target_exp /= 2;
			cit_lshift_count++;
		}
		if (cit_lshift_count > 7) //Max. CIT_LSHIFT = 7(128times)
			cit_lshift_count = 7;

		shutter = ((target_exp * vt_pic_clk_freq_mhz) - min_fine_int) / line_length_pck;
		if (shutter > max_coarse_integration_time)
			shutter = max_coarse_integration_time;

		write_cmos_sensor_8(0x0104, 0x01);
		write_cmos_sensor_8(0x3002, cit_lshift_count & 0x07);
		write_cmos_sensor_8(0x0340, imgsensor.frame_length >> 8);
		write_cmos_sensor_8(0x0341, imgsensor.frame_length & 0xFF);
		write_cmos_sensor_8(0x0104, 0x00);

	} else {
		if (imgsensor.autoflicker_en) {
			realtime_fps = imgsensor.pclk / imgsensor.line_length * 10
					/ imgsensor.frame_length;
			if (realtime_fps >= 297 && realtime_fps <= 305)
				set_max_framerate(296, 0);
			else if (realtime_fps >= 147 && realtime_fps <= 150)
				set_max_framerate(146, 0);
			else {
				/* Extend frame length*/
				write_cmos_sensor_8(0x0104, 0x01);
				write_cmos_sensor_8(0x3002, 0x00);
				write_cmos_sensor_8(0x0340, imgsensor.frame_length >> 8);
				write_cmos_sensor_8(0x0341, imgsensor.frame_length & 0xFF);
				write_cmos_sensor_8(0x0104, 0x00);
			}
		} else {
			/* Extend frame length*/
			write_cmos_sensor_8(0x0104, 0x01);
			write_cmos_sensor_8(0x3002, 0x00);
			write_cmos_sensor_8(0x0340, imgsensor.frame_length >> 8);
			write_cmos_sensor_8(0x0341, imgsensor.frame_length & 0xFF);
			write_cmos_sensor_8(0x0104, 0x00);

			LOG_DBG("(else)imgsensor.frame_length = %d\n", imgsensor.frame_length);
		}
	}
	/* Update Shutter */
	write_cmos_sensor_8(0x0104, 0x01);
	//write_cmos_sensor_8(0x0350, 0x00);	/* Disable auto extend */
	write_cmos_sensor_8(0x0202, (shutter >> 8) & 0xFF);
	write_cmos_sensor_8(0x0203, shutter  & 0xFF);
	write_cmos_sensor_8(0x0104, 0x00);
	LOG_DBG("shutter =%d, framelength =%d\n", shutter, imgsensor.frame_length);

}	/*	write_shutter  */


static void set_shutter(kal_uint32 shutter)
{
	unsigned long flags;

	spin_lock_irqsave(&imgsensor_drv_lock, flags);
	imgsensor.shutter = shutter;
	spin_unlock_irqrestore(&imgsensor_drv_lock, flags);

	write_shutter(shutter);

}	/*    set_shutter */


static void set_shutter_frame_length(
				UINT32 shutter, UINT32 frame_length)
{
	unsigned long flags;
	kal_uint16 realtime_fps = 0;
	kal_int32 dummy_line = 0;

	spin_lock_irqsave(&imgsensor_drv_lock, flags);
	imgsensor.shutter = shutter;
	spin_unlock_irqrestore(&imgsensor_drv_lock, flags);
	/* LOG_DBG("shutter =%d, frame_time =%d\n", shutter, frame_time); */

	/* 0x3500, 0x3501, 0x3502 will increase VBLANK
	 * to get exposure larger than frame exposure
	 */
	/* AE doesn't update sensor gain at capture mode,
	 * thus extra exposure lines must be updated here.
	 */

	/* OV Recommend Solution */
	/* if shutter bigger than frame_length,
	 * should extend frame length first
	 */
	spin_lock(&imgsensor_drv_lock);
	/*Change frame time */
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

	shutter = (shutter > (imgsensor_info.max_frame_length - imgsensor_info.margin))
			? (imgsensor_info.max_frame_length - imgsensor_info.margin) : shutter;

	if (imgsensor.autoflicker_en) {
		realtime_fps = imgsensor.pclk / imgsensor.line_length * 10 / imgsensor.frame_length;

		if (realtime_fps >= 297 && realtime_fps <= 305)
			set_max_framerate(296, 0);
		else if (realtime_fps >= 147 && realtime_fps <= 150)
			set_max_framerate(146, 0);
		else {
			/* Extend frame length */
			write_cmos_sensor_8(0x0104, 0x01);
			write_cmos_sensor_8(0x0340, imgsensor.frame_length >> 8);
			write_cmos_sensor_8(0x0341, imgsensor.frame_length & 0xFF);
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
	write_cmos_sensor_8(0x0202, (shutter >> 8) & 0xFF);
	write_cmos_sensor_8(0x0203, shutter & 0xFF);
	write_cmos_sensor_8(0x0104, 0x00);

	LOG_DBG("Exit! shutter =%d, framelength =%d/%d, dummy_line=%d, auto_extend=%d\n",
		shutter, imgsensor.frame_length, frame_length,
		dummy_line, read_cmos_sensor_8(0x0350));
}				/* set_shutter_frame_length */


static kal_uint16 gain2reg(const kal_uint16 gain)
{
	kal_uint16 reg_gain = 0x0;

	reg_gain = (1024 - (1024*64)/gain)/2;
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
}

static void ihdr_write_shutter_gain(
				kal_uint16 le, kal_uint16 se, kal_uint16 gain)
{

	kal_uint16 realtime_fps = 0;
	kal_uint16 reg_gain;

	spin_lock(&imgsensor_drv_lock);
	if (le > imgsensor.min_frame_length - imgsensor_info.margin)
		imgsensor.frame_length = le + imgsensor_info.margin;
	else
		imgsensor.frame_length = imgsensor.min_frame_length;
	if (imgsensor.frame_length > imgsensor_info.max_frame_length)
		imgsensor.frame_length = imgsensor_info.max_frame_length;
	spin_unlock(&imgsensor_drv_lock);
	if (le < imgsensor_info.min_shutter)
		le = imgsensor_info.min_shutter;
	if (imgsensor.autoflicker_en) {
		realtime_fps = imgsensor.pclk
			/ imgsensor.line_length * 10 / imgsensor.frame_length;

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
	} else {
		write_cmos_sensor_8(0x0104, 0x01);
		write_cmos_sensor_8(0x0340, imgsensor.frame_length >> 8);
		write_cmos_sensor_8(0x0341, imgsensor.frame_length & 0xFF);
		write_cmos_sensor_8(0x0104, 0x00);
	}
	write_cmos_sensor_8(0x0104, 0x01);
	write_cmos_sensor_8(0x0350, 0x01);	/* Enable auto extend */
	/* Long exposure */
	write_cmos_sensor_8(0x0202, (le >> 8) & 0xFF);
	write_cmos_sensor_8(0x0203, le & 0xFF);
	/* Short exposure */
	write_cmos_sensor_8(0x0224, (se >> 8) & 0xFF);
	write_cmos_sensor_8(0x0225, se & 0xFF);
	reg_gain = gain2reg(gain);
	spin_lock(&imgsensor_drv_lock);
	imgsensor.gain = reg_gain;
	spin_unlock(&imgsensor_drv_lock);
	/* Global analog Gain for Long expo */
	write_cmos_sensor_8(0x0204, (reg_gain >> 8) & 0xFF);
	write_cmos_sensor_8(0x0205, reg_gain & 0xFF);
	/* Global analog Gain for Short expo */
	/* write_cmos_sensor(0x0216, (reg_gain>>8)& 0xFF); */
	/* write_cmos_sensor(0x0217, reg_gain & 0xFF); */
	write_cmos_sensor_8(0x0104, 0x00);
	LOG_DBG("le:0x%x, se:0x%x, gain:0x%x, auto_extend=%d\n", le, se, gain, read_cmos_sensor_8(0x0350));
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
#if 0 // Not used
static void night_mode(kal_bool enable)
{
/*No Need to implement this function*/
}				/*      night_mode      */
#endif

kal_uint16 addr_data_init[] = {
	/* External Clock Setting */
	0x0136, 0x1A,
	0x0137, 0x00,

	/* Flip Mirror Setting */
	//0x0101, 0x03,

	/* Global Setting */
	0x3051, 0x00,
	0x3052, 0x00,
	0x4E21, 0x14,
	0x6B11, 0xCF,
	0x7FF0, 0x08,
	0x7FF1, 0x0F,
	0x7FF2, 0x08,
	0x7FF3, 0x1B,
	0x7FF4, 0x23,
	0x7FF5, 0x60,
	0x7FF6, 0x00,
	0x7FF7, 0x01,
	0x7FF8, 0x00,
	0x7FF9, 0x78,
	0x7FFA, 0x00,
	0x7FFB, 0x00,
	0x7FFC, 0x00,
	0x7FFD, 0x00,
	0x7FFE, 0x00,
	0x7FFF, 0x03,
	0x7F76, 0x03,
	0x7F77, 0xFE,
	0x7FA8, 0x03,
	0x7FA9, 0xFE,
	0x7B24, 0x81,
	0x7B25, 0x00,
	0x6564, 0x07,
	0x6B0D, 0x41,
	0x653D, 0x04,
	0x6B05, 0x8C,
	0x6B06, 0xF9,
	0x6B08, 0x65,
	0x6B09, 0xFC,
	0x6B0A, 0xCF,
	0x6B0B, 0xD2,
	0x6700, 0x0E,
	0x6707, 0x0E,
	0x5F04, 0x00,
	0x5F05, 0xED,

	//for Pedestal problem
	0x792F, 0x01,
	0x7930, 0x01,
	0x7937, 0x01,
	0x793C, 0x01,
};

kal_uint16 addr_data_preview[] = {
	/* Output format setting */
	0x0112, 0x0A,
	0x0113, 0x0A,
	0x0114, 0x03,

	/* Clock setting */
	0x0301, 0x05,
	0x0303, 0x02,
	0x0305, 0x04,
	0x0306, 0x00,
	0x0307, 0xC8,
	0x0309, 0x0A,
	0x030B, 0x01,
	0x030D, 0x04,
	0x030E, 0x00,
	0x030F, 0xA8,
	0x0310, 0x01,
	0x0820, 0x11,
	0x0821, 0x10,
	0x0822, 0x00,
	0x0823, 0x00,

	/* Clock Adjustment Setting */
	0x4648, 0x7F,
	0x7420, 0x00,
	0x7421, 0x5C,
	0x7422, 0x00,
	0x7423, 0xD7,
	0x9104, 0x04,

	/* Line Length Setting */
	0x0342, 0x14,
	0x0343, 0xE8,

	/* Frame Length Setting */
	0x0340, 0x0C,
	0x0341, 0xA4,

	/* ROI setting */
	0x0344, 0x00,
	0x0345, 0x00,
	0x0346, 0x00,
	0x0347, 0x00,
	0x0348, 0x10,
	0x0349, 0x6F,
	0x034A, 0x0C,
	0x034B, 0x2F,

	/* Analog Image Size Setting */
	0x0381, 0x01,
	0x0383, 0x01,
	0x0385, 0x01,
	0x0387, 0x01,
	0x0900, 0x00,
	0x0901, 0x11,

	/* Digital Image Size Setting */
	0x0401, 0x00,
	0x0404, 0x00,
	0x0405, 0x10,
	0x0408, 0x00,
	0x0409, 0x28,
	0x040A, 0x00,
	0x040B, 0x0C,
	0x040C, 0x10,
	0x040D, 0x20,
	0x040E, 0x0C,
	0x040F, 0x18,
	0x3038, 0x00,
	0x303A, 0x00,
	0x303B, 0x10,
	0x300D, 0x00,

	/* Output Size Setting */
	0x034C, 0x10,
	0x034D, 0x20,
	0x034E, 0x0C,
	0x034F, 0x18,

	/* Integration Time Setting */
	0x0202, 0x0C,
	0x0203, 0x9A,

	/* Gain Setting */
	0x0204, 0x00,
	0x0205, 0x00,
	0x020E, 0x01,
	0x020F, 0x00,
	0x0210, 0x01,
	0x0211, 0x00,
	0x0212, 0x01,
	0x0213, 0x00,
	0x0214, 0x01,
	0x0215, 0x00,

	/* Added Setting(AF) */
	0x7BCD, 0x00,

	/* Added Setting(IQ) */
	0x94DC, 0x20,
	0x94DD, 0x20,
	0x94DE, 0x20,
	0x95DC, 0x20,
	0x95DD, 0x20,
	0x95DE, 0x20,
	0x7FB0, 0x00,
	0x9010, 0x3E,
	0x9419, 0x50,
	0x941B, 0x50,
	0x9519, 0x50,
	0x951B, 0x50,

	/* Added Setting(mode) */
	0x3030, 0x00,
	0x3032, 0x00,
	0x0220, 0x00,
	0x0B06, 0x00,
};

kal_uint16 addr_data_video[] = {
	/* Output format setting */
	0x0112, 0x0A,
	0x0113, 0x0A,
	0x0114, 0x03,

	/* Clock setting */
	0x0301, 0x05,
	0x0303, 0x02,
	0x0305, 0x04,
	0x0306, 0x00,
	0x0307, 0xC8,
	0x0309, 0x0A,
	0x030B, 0x01,
	0x030D, 0x04,
	0x030E, 0x00,
	0x030F, 0xA8,
	0x0310, 0x01,
	0x0820, 0x11,
	0x0821, 0x10,
	0x0822, 0x00,
	0x0823, 0x00,

	/* Clock Adjustment Setting */
	0x4648, 0x7F,
	0x7420, 0x00,
	0x7421, 0x5C,
	0x7422, 0x00,
	0x7423, 0xD7,
	0x9104, 0x04,

	/* Line Length Setting */
	0x0342, 0x14,
	0x0343, 0xE8,

	/* Frame Length Setting */
	0x0340, 0x0C,
	0x0341, 0xA4,

	/* ROI setting */
	0x0344, 0x00,
	0x0345, 0x00,
	0x0346, 0x00,
	0x0347, 0x00,
	0x0348, 0x10,
	0x0349, 0x6F,
	0x034A, 0x0C,
	0x034B, 0x2F,

	/* Analog Image Size Setting */
	0x0381, 0x01,
	0x0383, 0x01,
	0x0385, 0x01,
	0x0387, 0x01,
	0x0900, 0x00,
	0x0901, 0x11,

	/* Digital Image Size Setting */
	0x0401, 0x00,
	0x0404, 0x00,
	0x0405, 0x10,
	0x0408, 0x00,
	0x0409, 0x28,
	0x040A, 0x01,
	0x040B, 0x8E,
	0x040C, 0x10,
	0x040D, 0x20,
	0x040E, 0x09,
	0x040F, 0x14,
	0x3038, 0x00,
	0x303A, 0x00,
	0x303B, 0x10,
	0x300D, 0x00,

	/* Output Size Setting */
	0x034C, 0x10,
	0x034D, 0x20,
	0x034E, 0x09,
	0x034F, 0x14,

	/* Integration Time Setting */
	0x0202, 0x0C,
	0x0203, 0x9A,

	/* Gain Setting */
	0x0204, 0x00,
	0x0205, 0x00,
	0x020E, 0x01,
	0x020F, 0x00,
	0x0210, 0x01,
	0x0211, 0x00,
	0x0212, 0x01,
	0x0213, 0x00,
	0x0214, 0x01,
	0x0215, 0x00,

	/* Added Setting(AF) */
	0x7BCD, 0x00,

	/* Added Setting(IQ) */
	0x94DC, 0x20,
	0x94DD, 0x20,
	0x94DE, 0x20,
	0x95DC, 0x20,
	0x95DD, 0x20,
	0x95DE, 0x20,
	0x7FB0, 0x00,
	0x9010, 0x3E,
	0x9419, 0x50,
	0x941B, 0x50,
	0x9519, 0x50,
	0x951B, 0x50,

	/* Added Setting(mode) */
	0x3030, 0x00,
	0x3032, 0x00,
	0x0220, 0x00,
	0x0B06, 0x00,
};

kal_uint16 addr_data_custom1[] = {
	/* Output format setting */
	0x0112, 0x0A,
	0x0113, 0x0A,
	0x0114, 0x03,

	/* Clock setting */
	0x0301, 0x05,
	0x0303, 0x02,
	0x0305, 0x04,
	0x0306, 0x00,
	0x0307, 0xC8,
	0x0309, 0x0A,
	0x030B, 0x01,
	0x030D, 0x04,
	0x030E, 0x00,
	0x030F, 0xA8,
	0x0310, 0x01,
	0x0820, 0x11,
	0x0821, 0x10,
	0x0822, 0x00,
	0x0823, 0x00,

	/* Clock Adjustment Setting */
	0x4648, 0x7F,
	0x7420, 0x00,
	0x7421, 0x5C,
	0x7422, 0x00,
	0x7423, 0xD7,
	0x9104, 0x04,

	/* Line Length Setting */
	0x0342, 0x14,
	0x0343, 0xE8,

	/* Frame Length Setting */
	0x0340, 0x06,
	0x0341, 0x50,

	/* ROI setting */
	0x0344, 0x00,
	0x0345, 0x00,
	0x0346, 0x00,
	0x0347, 0x00,
	0x0348, 0x10,
	0x0349, 0x6F,
	0x034A, 0x0C,
	0x034B, 0x2F,

	/* Analog Image Size Setting */
	0x0381, 0x01,
	0x0383, 0x01,
	0x0385, 0x01,
	0x0387, 0x01,
	0x0900, 0x01,
	0x0901, 0x14,

	/* Digital Image Size Setting */
	0x0401, 0x01,
	0x0404, 0x00,
	0x0405, 0x40,
	0x0408, 0x00,
	0x0409, 0x36,
	0x040A, 0x00,
	0x040B, 0x06,
	0x040C, 0x10,
	0x040D, 0x04,
	0x040E, 0x03,
	0x040F, 0x00,
	0x3038, 0x00,
	0x303A, 0x00,
	0x303B, 0x10,
	0x300D, 0x00,

	/* Output Size Setting */
	0x034C, 0x04,
	0x034D, 0x00,
	0x034E, 0x03,
	0x034F, 0x00,

	/* Integration Time Setting */
	0x0202, 0x06,
	0x0203, 0x46,

	/* Gain Setting */
	0x0204, 0x00,
	0x0205, 0x00,
	0x020E, 0x01,
	0x020F, 0x00,
	0x0210, 0x01,
	0x0211, 0x00,
	0x0212, 0x01,
	0x0213, 0x00,
	0x0214, 0x01,
	0x0215, 0x00,

	/* Added Setting(AF) */
	0x7BCD, 0x00,

	/* Added Setting(IQ) */
	0x94DC, 0x20,
	0x94DD, 0x20,
	0x94DE, 0x20,
	0x95DC, 0x20,
	0x95DD, 0x20,
	0x95DE, 0x20,
	0x7FB0, 0x00,
	0x9010, 0x3E,
	0x9419, 0x50,
	0x941B, 0x50,
	0x9519, 0x50,
	0x951B, 0x50,

	/* Added Setting(mode) */
	0x3030, 0x00,
	0x3032, 0x00,
	0x0220, 0x00,
	0x0B06, 0x00,
};

kal_uint16 addr_data_custom2[] = {
	/* Output format setting */
	0x0112, 0x0A,
	0x0113, 0x0A,
	0x0114, 0x03,

	/* Clock setting */
	0x0301, 0x05,
	0x0303, 0x02,
	0x0305, 0x04,
	0x0306, 0x00,
	0x0307, 0xC8,
	0x0309, 0x0A,
	0x030B, 0x01,
	0x030D, 0x04,
	0x030E, 0x00,
	0x030F, 0xA8,
	0x0310, 0x01,
	0x0820, 0x11,
	0x0821, 0x10,
	0x0822, 0x00,
	0x0823, 0x00,

	/* Clock Adjustment Setting */
	0x4648, 0x7F,
	0x7420, 0x00,
	0x7421, 0x5C,
	0x7422, 0x00,
	0x7423, 0xD7,
	0x9104, 0x04,

	/* Line Length Setting */
	0x0342, 0x14,
	0x0343, 0xE8,

	/* Frame Length Setting */
	0x0340, 0x0C,
	0x0340, 0xA4,

	/* ROI setting */
	0x0344, 0x00,
	0x0345, 0x00,
	0x0346, 0x01,
	0x0347, 0x1A,
	0x0348, 0x10,
	0x0349, 0x6F,
	0x034A, 0x0B,
	0x034B, 0x15,

	/* Analog Image Size Setting */
	0x0381, 0x01,
	0x0383, 0x01,
	0x0385, 0x01,
	0x0387, 0x01,
	0x0900, 0x00,
	0x0901, 0x11,

	/* Digital Image Size Setting */
	0x0401, 0x00,
	0x0404, 0x00,
	0x0405, 0x10,
	0x0408, 0x00,
	0x0409, 0xF8,
	0x040A, 0x00,
	0x040B, 0x00,
	0x040C, 0x0E,
	0x040D, 0x80,
	0x040E, 0x09,
	0x040F, 0xFC,
	0x3038, 0x00,
	0x303A, 0x00,
	0x303B, 0x10,
	0x300D, 0x00,

	/* Output Size Setting */
	0x034C, 0x0E,
	0x034D, 0x80,
	0x034E, 0x09,
	0x034F, 0xFC,

	/* Integration Time Setting */
	0x0202, 0x0C,
	0x0203, 0x9A,

	/* Gain Setting */
	0x0204, 0x00,
	0x0205, 0x00,
	0x020E, 0x01,
	0x020F, 0x00,
	0x0210, 0x01,
	0x0211, 0x00,
	0x0212, 0x01,
	0x0213, 0x00,
	0x0214, 0x01,
	0x0215, 0x00,

	/* Added Setting(AF) */
	0x7BCD, 0x00,

	/* Added Setting(IQ) */
	0x94DC, 0x20,
	0x94DD, 0x20,
	0x94DE, 0x20,
	0x95DC, 0x20,
	0x95DD, 0x20,
	0x95DE, 0x20,
	0x7FB0, 0x00,
	0x9010, 0x3E,
	0x9419, 0x50,
	0x941B, 0x50,
	0x9519, 0x50,
	0x951B, 0x50,

	/* Added Setting(mode) */
	0x3030, 0x00,
	0x3032, 0x00,
	0x0220, 0x00,
	0x0B06, 0x00,
};

static void preview_setting(void)
{
	LOG_INF("- E");
	table_write_cmos_sensor(addr_data_preview,
		sizeof(addr_data_preview) / sizeof(kal_uint16));
	LOG_INF("- X");
}

static void capture_setting(void)
{
	LOG_INF("- E");
	table_write_cmos_sensor(addr_data_preview,
		sizeof(addr_data_preview) / sizeof(kal_uint16));
	LOG_INF("- X");
}

static void normal_video_setting(void)
{
	LOG_INF("- E");
	table_write_cmos_sensor(addr_data_video,
		sizeof(addr_data_video) / sizeof(kal_uint16));
	LOG_INF("- X");
}

static void custom1_setting(void)
{
	LOG_INF("- E");
	table_write_cmos_sensor(addr_data_custom1,
		sizeof(addr_data_custom1) / sizeof(kal_uint16));
	LOG_INF("- X");
}

static void custom2_setting(void)
{
	LOG_INF("- E");
	table_write_cmos_sensor(addr_data_custom2,
		sizeof(addr_data_custom2) / sizeof(kal_uint16));
	LOG_INF("- X");
}

static void custom3_setting(void)
{
	LOG_INF("- E");
	table_write_cmos_sensor(addr_data_custom2,
		sizeof(addr_data_custom2) / sizeof(kal_uint16));
	LOG_INF("- X");
}

static void custom4_setting(void)
{
	LOG_INF("- E");
	table_write_cmos_sensor(addr_data_custom2,
		sizeof(addr_data_custom2) / sizeof(kal_uint16));
	LOG_INF("- X");
}

static void sensor_init(void)
{
	int ret = 0;

	LOG_INF("- E");
	ret = table_write_cmos_sensor(addr_data_init,
		sizeof(addr_data_init) / sizeof(kal_uint16));
	LOG_INF("- X");
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

	LOG_DBG("%s E\n", __func__);

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
	/* const kal_uint8 i2c_addr[] = {
	 * IMGSENSOR_WRITE_ID_1, IMGSENSOR_WRITE_ID_2};
	 */
	kal_uint8 i = 0;
	kal_uint8 retry = 2;
	kal_uint32 sensor_id = 0;

	LOG_DBG("%s() - E\n", __func__);

	while (imgsensor_info.i2c_addr_table[i] != 0xff) {
		spin_lock(&imgsensor_drv_lock);
		imgsensor.i2c_write_id = imgsensor_info.i2c_addr_table[i];
		spin_unlock(&imgsensor_drv_lock);

		do {
			sensor_id = ((read_cmos_sensor_8(0x0016) << 8) | read_cmos_sensor_8(0x0017));

			if (sensor_id == imgsensor_info.sensor_id) {
				LOG_DBG("i2c write id: 0x%x, sensor id: 0x%x\n", imgsensor.i2c_write_id, sensor_id);
				break;
			}

			LOG_DBG("%s:Read sensor id fail, id: 0x%x\n", __func__, imgsensor.i2c_write_id);
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
	imgsensor.autoflicker_en		= KAL_FALSE;
	imgsensor.sensor_mode		= IMGSENSOR_MODE_INIT;
	imgsensor.shutter				= 0x3D0;
	imgsensor.gain				= 0xe000;
	imgsensor.pclk				= imgsensor_info.pre.pclk;
	imgsensor.frame_length			= imgsensor_info.pre.framelength;
	imgsensor.line_length			= imgsensor_info.pre.linelength;
	imgsensor.min_frame_length		= imgsensor_info.pre.framelength;
	imgsensor.dummy_pixel			= 0;
	imgsensor.dummy_line			= 0;
	imgsensor.ihdr_mode			= 0;
	imgsensor.test_pattern			= KAL_FALSE;
	imgsensor.current_fps			= imgsensor_info.pre.max_framerate;
	spin_unlock(&imgsensor_drv_lock);

	return ERROR_NONE;
}				/*      open  */



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
	LOG_DBG("E\n");

	/*No Need to implement this function */
	streaming_control(false);
	return ERROR_NONE;
}				/*      close  */

static void set_imgsensor(enum IMGSENSOR_MODE sensor_mode_type, struct imgsensor_mode_struct imgsensor_mode)
{
	imgsensor.sensor_mode = sensor_mode_type;
	imgsensor.pclk = imgsensor_mode.pclk;
	imgsensor.line_length = imgsensor_mode.linelength;
	imgsensor.frame_length = imgsensor_mode.framelength;
	imgsensor.min_frame_length = imgsensor_mode.framelength;
	imgsensor.autoflicker_en = KAL_FALSE;
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
	LOG_INF("- E");
	spin_lock(&imgsensor_drv_lock);
	set_imgsensor(IMGSENSOR_MODE_PREVIEW, imgsensor_info.pre);
	spin_unlock(&imgsensor_drv_lock);
	preview_setting();
	LOG_INF("- X");
	return ERROR_NONE;
}				/*      preview   */

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
	LOG_INF("- E");
	spin_lock(&imgsensor_drv_lock);
	set_imgsensor(IMGSENSOR_MODE_CAPTURE, imgsensor_info.cap);
	spin_unlock(&imgsensor_drv_lock);
	capture_setting();
	msleep(100);
	LOG_INF("- X");
	return ERROR_NONE;
}				/* capture() */

static kal_uint32 normal_video(
			MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("- E");
	spin_lock(&imgsensor_drv_lock);
	set_imgsensor(IMGSENSOR_MODE_VIDEO, imgsensor_info.normal_video);
	spin_unlock(&imgsensor_drv_lock);
	normal_video_setting();
	LOG_INF("- X");
	return ERROR_NONE;
}				/*      normal_video   */

static kal_uint32 custom1(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("- E");
	spin_lock(&imgsensor_drv_lock);
	set_imgsensor(IMGSENSOR_MODE_CUSTOM1, imgsensor_info.custom1);
	spin_unlock(&imgsensor_drv_lock);
	custom1_setting();
	LOG_INF("- X");
	return ERROR_NONE;
}

static kal_uint32 custom2(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("- E");
	spin_lock(&imgsensor_drv_lock);
	set_imgsensor(IMGSENSOR_MODE_CUSTOM2, imgsensor_info.custom2);
	spin_unlock(&imgsensor_drv_lock);
	custom2_setting();
	LOG_INF("- X");
	return ERROR_NONE;
}

static kal_uint32 custom3(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("- E");
	spin_lock(&imgsensor_drv_lock);
	set_imgsensor(IMGSENSOR_MODE_CUSTOM3, imgsensor_info.custom3);
	spin_unlock(&imgsensor_drv_lock);
	custom3_setting();
	LOG_INF("- X");
	return ERROR_NONE;
}

static kal_uint32 custom4(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("- E");
	spin_lock(&imgsensor_drv_lock);
	set_imgsensor(IMGSENSOR_MODE_CUSTOM4, imgsensor_info.custom4);
	spin_unlock(&imgsensor_drv_lock);
	custom4_setting();
	LOG_INF("- X");
	return ERROR_NONE;
}

static kal_uint32 get_resolution(
	MSDK_SENSOR_RESOLUTION_INFO_STRUCT(*sensor_resolution))
{
	LOG_DBG("- E");
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

	sensor_resolution->SensorCustom2Width =
		imgsensor_info.custom2.grabwindow_width;
	sensor_resolution->SensorCustom2Height =
		imgsensor_info.custom2.grabwindow_height;

	sensor_resolution->SensorCustom3Width =
		imgsensor_info.custom3.grabwindow_width;
	sensor_resolution->SensorCustom3Height =
		imgsensor_info.custom3.grabwindow_height;

	sensor_resolution->SensorCustom4Width =
		imgsensor_info.custom4.grabwindow_width;
	sensor_resolution->SensorCustom4Height =
		imgsensor_info.custom4.grabwindow_height;
	return ERROR_NONE;
}				/*      get_resolution  */

static kal_uint32 get_info(enum MSDK_SCENARIO_ID_ENUM scenario_id,
			   MSDK_SENSOR_INFO_STRUCT *sensor_info,
			   MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("scenario_id = %d\n", scenario_id);

	sensor_info->SensorClockPolarity = SENSOR_CLOCK_POLARITY_LOW;
	sensor_info->SensorClockFallingPolarity = SENSOR_CLOCK_POLARITY_LOW;
	sensor_info->SensorHsyncPolarity = SENSOR_CLOCK_POLARITY_LOW;
	sensor_info->SensorVsyncPolarity = SENSOR_CLOCK_POLARITY_LOW;
	sensor_info->SensorInterruptDelayLines = 4;	/* not use */
	sensor_info->SensorResetActiveHigh = FALSE;	/* not use */
	sensor_info->SensorResetDelayCount = 5;	/* not use */

	sensor_info->SensroInterfaceType = imgsensor_info.sensor_interface_type;
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
	sensor_info->Custom2DelayFrame = imgsensor_info.custom2_delay_frame;
	sensor_info->Custom3DelayFrame = imgsensor_info.custom3_delay_frame;
	sensor_info->Custom4DelayFrame = imgsensor_info.custom4_delay_frame;
	sensor_info->SensorMasterClockSwitch = 0;	/* not use */
	sensor_info->SensorDrivingCurrent = imgsensor_info.isp_driving_current;

	sensor_info->AEShutDelayFrame = imgsensor_info.ae_shut_delay_frame;
	sensor_info->AESensorGainDelayFrame =
		imgsensor_info.ae_sensor_gain_delay_frame;
	sensor_info->AEISPGainDelayFrame =
		imgsensor_info.ae_ispGain_delay_frame;
	sensor_info->IHDR_Support = imgsensor_info.ihdr_support;
	sensor_info->IHDR_LE_FirstLine = imgsensor_info.ihdr_le_firstline;
	sensor_info->SensorModeNum = imgsensor_info.sensor_mode_num;
	sensor_info->TEMPERATURE_SUPPORT = 1;

	sensor_info->SensorMIPILaneNumber = imgsensor_info.mipi_lane_num;
	sensor_info->SensorClockFreq = imgsensor_info.mclk;
	sensor_info->SensorClockDividCount = 3;	/* not use */
	sensor_info->SensorClockRisingCount = 0;
	sensor_info->SensorClockFallingCount = 2;	/* not use */
	sensor_info->SensorPixelClockCount = 3;	/* not use */
	sensor_info->SensorDataLatchCount = 2;	/* not use */

	sensor_info->MIPIDataLowPwr2HighSpeedTermDelayCount = 0;
	sensor_info->MIPICLKLowPwr2HighSpeedTermDelayCount = 0;
	sensor_info->SensorWidthSampling = 0;	/* 0 is default 1x */
	sensor_info->SensorHightSampling = 0;	/* 0 is default 1x */
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
	case MSDK_SCENARIO_ID_CUSTOM2:
		sensor_info->SensorGrabStartX = imgsensor_info.custom2.startx;
		sensor_info->SensorGrabStartY = imgsensor_info.custom2.starty;
		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
			imgsensor_info.custom2.mipi_data_lp2hs_settle_dc;
		break;
	case MSDK_SCENARIO_ID_CUSTOM3:
		sensor_info->SensorGrabStartX = imgsensor_info.custom3.startx;
		sensor_info->SensorGrabStartY = imgsensor_info.custom3.starty;
		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
			imgsensor_info.custom3.mipi_data_lp2hs_settle_dc;
		break;
	case MSDK_SCENARIO_ID_CUSTOM4:
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
}				/*      get_info  */


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
		LOG_ERR("Error ScenarioId setting");
		preview(image_window, sensor_config_data);
		return ERROR_INVALID_SCENARIO_ID;
	}
	return ERROR_NONE;
}				/* control() */



static kal_uint32 set_video_mode(UINT16 framerate)
{
	LOG_DBG("framerate = %d\n ", framerate);
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
	LOG_DBG("enable = %d, framerate = %d\n", enable, framerate);
	spin_lock(&imgsensor_drv_lock);
	if (enable)		/* enable auto flicker */
		imgsensor.autoflicker_en = KAL_TRUE;
	else			/* Cancel Auto flick */
		imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	return ERROR_NONE;
}

static kal_uint32 set_max_framerate_by_scenario(
		enum MSDK_SCENARIO_ID_ENUM scenario_id, MUINT32 framerate)
{
	kal_uint32 frame_length;

	LOG_DBG("scenario_id = %d, framerate = %d\n", scenario_id, framerate);

	switch (scenario_id) {
	case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		frame_length = imgsensor_info.pre.pclk
			/ framerate * 10 / imgsensor_info.pre.linelength;

		spin_lock(&imgsensor_drv_lock);

		imgsensor.dummy_line =
		  (frame_length > imgsensor_info.pre.framelength)
		? (frame_length - imgsensor_info.pre.framelength) : 0;

		imgsensor.frame_length =
			imgsensor_info.pre.framelength + imgsensor.dummy_line;

		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		set_dummy();
		break;
	case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
		if (framerate == 0)
			return ERROR_NONE;

		frame_length = imgsensor_info.normal_video.pclk
			/ framerate * 10 / imgsensor_info.normal_video.linelength;

		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line =
		  (frame_length > imgsensor_info.normal_video.framelength)
		? (frame_length - imgsensor_info.normal_video.framelength) : 0;

		imgsensor.frame_length =
		 imgsensor_info.normal_video.framelength + imgsensor.dummy_line;

		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		set_dummy();
		break;
	case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		if (imgsensor.current_fps != imgsensor_info.cap.max_framerate)
			LOG_DBG(
				"current_fps %d fps is not support, so use cap's setting: %d fps!\n",
				framerate,
				imgsensor_info.cap.max_framerate / 10);

		frame_length = imgsensor_info.cap.pclk /
			framerate * 10 / imgsensor_info.cap.linelength;

		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line =
		  (frame_length > imgsensor_info.cap.framelength)
		? (frame_length - imgsensor_info.cap.framelength) : 0;

		imgsensor.frame_length =
			imgsensor_info.cap.framelength + imgsensor.dummy_line;

		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		set_dummy();
		break;
	case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
		frame_length = imgsensor_info.hs_video.pclk
			/ framerate * 10 / imgsensor_info.hs_video.linelength;

		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line =
			(frame_length > imgsensor_info.hs_video.framelength)
			? (frame_length - imgsensor_info.hs_video.framelength) : 0;

		imgsensor.frame_length =
			imgsensor_info.hs_video.framelength + imgsensor.dummy_line;

		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		set_dummy();
		break;
	case MSDK_SCENARIO_ID_SLIM_VIDEO:
		frame_length = imgsensor_info.slim_video.pclk
			/ framerate * 10 / imgsensor_info.slim_video.linelength;

		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line =
		  (frame_length > imgsensor_info.slim_video.framelength)
		? (frame_length - imgsensor_info.slim_video.framelength) : 0;

		imgsensor.frame_length =
		  imgsensor_info.slim_video.framelength + imgsensor.dummy_line;

		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		set_dummy();
		break;
	case MSDK_SCENARIO_ID_CUSTOM1:
		frame_length = imgsensor_info.custom1.pclk
			/ framerate * 10 / imgsensor_info.custom1.linelength;

		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line =
		  (frame_length > imgsensor_info.custom1.framelength)
		? (frame_length - imgsensor_info.custom1.framelength) : 0;

		imgsensor.frame_length =
			imgsensor_info.custom1.framelength + imgsensor.dummy_line;

		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		set_dummy();
		break;
	case MSDK_SCENARIO_ID_CUSTOM2:
	case MSDK_SCENARIO_ID_CUSTOM3:
	case MSDK_SCENARIO_ID_CUSTOM4:
		frame_length = imgsensor_info.custom2.pclk
			/ framerate * 10 / imgsensor_info.custom2.linelength;

		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line =
		  (frame_length > imgsensor_info.custom2.framelength)
		? (frame_length - imgsensor_info.custom2.framelength) : 0;

		imgsensor.frame_length =
			imgsensor_info.custom2.framelength + imgsensor.dummy_line;

		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		set_dummy();
		break;
	default:		/* coding with  preview scenario by default */
		frame_length = imgsensor_info.pre.pclk
			/ framerate * 10 / imgsensor_info.pre.linelength;

		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line =
		  (frame_length > imgsensor_info.pre.framelength)
		? (frame_length - imgsensor_info.pre.framelength) : 0;

		imgsensor.frame_length =
			imgsensor_info.pre.framelength + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		set_dummy();
		LOG_DBG("error scenario_id = %d, we use preview scenario\n",
			scenario_id);
		break;
	}
	return ERROR_NONE;
}


static kal_uint32 get_default_framerate_by_scenario(
		enum MSDK_SCENARIO_ID_ENUM scenario_id, MUINT32 *framerate)
{
	LOG_DBG("scenario_id = %d\n", scenario_id);

	switch (scenario_id) {
	case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
	case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
	case MSDK_SCENARIO_ID_SLIM_VIDEO:
		*framerate = imgsensor_info.pre.max_framerate;
		break;
	case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
		*framerate = imgsensor_info.normal_video.max_framerate;
		break;
	case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		*framerate = imgsensor_info.cap.max_framerate;
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
	LOG_DBG("enable: %d\n", enable);

	if (enable)
		write_cmos_sensor(0x0601, 0x02);
	else
		write_cmos_sensor(0x0601, 0x00);

	spin_lock(&imgsensor_drv_lock);
	imgsensor.test_pattern = enable;
	spin_unlock(&imgsensor_drv_lock);
	return ERROR_NONE;
}

static void wait_stream_onoff_range(kal_uint32 addr, kal_uint32 min_val, kal_uint32 max_val)
{
	kal_uint8 read_val = 0;
	kal_uint16 read_addr = 0;
	kal_uint32 max_cnt = 0, cur_cnt = 0;
	unsigned long wait_delay_ms = 0;

	read_addr = addr;
	wait_delay_ms = 2;
	max_cnt = 100;
	cur_cnt = 0;

	read_val = read_cmos_sensor_8(read_addr);

	while (read_val < min_val || read_val > max_val) {
		mDELAY(wait_delay_ms);
		read_val = read_cmos_sensor_8(read_addr);
		cur_cnt++;

		if (cur_cnt >= max_cnt) {
			LOG_ERR("stream timeout: %d ms\n", (max_cnt * wait_delay_ms));
			break;
		}
	}
	LOG_INF("wait time: %d ms\n", (cur_cnt * wait_delay_ms));
}

/*
 * Read sensor frame counter (sensor_fcount address = 0x0005)
 * stream on (0x01 ~ 0xFE), stream standby (0xFF)
 */

static kal_uint32 streaming_control(kal_bool enable)
{
	LOG_INF("streaming_enable(0=Sw Standby,1=streaming): %d\n", enable);
	if (enable) {
		write_cmos_sensor(0x0100, 0X01);
		wait_stream_onoff_range(0x0005, 0x01, 0xFE);
	} else {
		write_cmos_sensor(0x0100, 0x00);
		wait_stream_onoff_range(0x0005, 0xFF, 0xFF);
	}

	return ERROR_NONE;
}

static kal_uint32 feature_control(MSDK_SENSOR_FEATURE_ENUM feature_id,
	UINT8 *feature_para, UINT32 *feature_para_len)
{
	UINT16 *feature_return_para_16 = (UINT16 *)feature_para;
	UINT16 *feature_data_16 = (UINT16 *)feature_para;
	UINT32 *feature_return_para_32 = (UINT32 *)feature_para;
	UINT32 *feature_data_32 = (UINT32 *)feature_para;
	unsigned long long *feature_data = (unsigned long long *) feature_para;
	/* unsigned long long *feature_return_para=(unsigned long long *) feature_para; */

	struct SENSOR_WINSIZE_INFO_STRUCT *wininfo;
	MSDK_SENSOR_REG_INFO_STRUCT *sensor_reg_data = (MSDK_SENSOR_REG_INFO_STRUCT *)feature_para;

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
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
					= imgsensor_info.slim_video.pclk;
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
			case MSDK_SCENARIO_ID_CUSTOM2:
				rate = imgsensor_info.custom2.mipi_pixel_rate;
				break;
			case MSDK_SCENARIO_ID_CUSTOM3:
				rate = imgsensor_info.custom3.mipi_pixel_rate;
				break;
			case MSDK_SCENARIO_ID_CUSTOM4:
				rate = imgsensor_info.custom4.mipi_pixel_rate;
				break;
			case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
			default:
				rate = imgsensor_info.pre.mipi_pixel_rate;
				break;
			}
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) = rate;
		}
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
	case SENSOR_FEATURE_SET_ESHUTTER:
		set_shutter(*feature_data);
		break;
	case SENSOR_FEATURE_SET_NIGHTMODE:
		//night_mode((BOOL)*feature_data);
		LOG_INF("SENSOR_FEATURE_SET_NIGHTMODE = %d, data=%d\n", feature_id, *feature_data);
		if (*feature_data)
			sIsNightHyperlapse = KAL_TRUE;
		else
			sIsNightHyperlapse = KAL_FALSE;
		break;
	case SENSOR_FEATURE_SET_GAIN:
		set_gain((UINT16)*feature_data);
		break;
	case SENSOR_FEATURE_SET_FLASHLIGHT:
		break;
	case SENSOR_FEATURE_SET_ISP_MASTER_CLOCK_FREQ:
		break;
	case SENSOR_FEATURE_SET_SHUTTER_FRAME_TIME:
		set_shutter_frame_length((UINT32) (*feature_data), (UINT32) (*(feature_data + 1)));
		break;
	case SENSOR_FEATURE_SET_REGISTER:
		write_cmos_sensor(sensor_reg_data->RegAddr, sensor_reg_data->RegData);
		break;
	case SENSOR_FEATURE_GET_REGISTER:
		sensor_reg_data->RegData = read_cmos_sensor(sensor_reg_data->RegAddr);
		LOG_INF("adb_i2c_read 0x%x = 0x%x\n", sensor_reg_data->RegAddr, sensor_reg_data->RegData);
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
		set_auto_flicker_mode((BOOL)*feature_data_16, *(feature_data_16 + 1));
		break;
	case SENSOR_FEATURE_SET_MAX_FRAME_RATE_BY_SCENARIO:
		 set_max_framerate_by_scenario(
				(enum MSDK_SCENARIO_ID_ENUM)*feature_data,
				*(feature_data+1));
		break;
	case SENSOR_FEATURE_GET_DEFAULT_FRAME_RATE_BY_SCENARIO:
		get_default_framerate_by_scenario((enum MSDK_SCENARIO_ID_ENUM)*(feature_data),
			(MUINT32 *)(uintptr_t)(*(feature_data + 1)));
		break;
	case SENSOR_FEATURE_SET_TEST_PATTERN:
		set_test_pattern_mode((BOOL)*feature_data);
		break;
	case SENSOR_FEATURE_GET_TEST_PATTERN_CHECKSUM_VALUE: /*for factory mode auto testing */
		*feature_return_para_32 = imgsensor_info.checksum_value;
		*feature_para_len = 4;
		break;
	case SENSOR_FEATURE_SET_FRAMERATE:
		LOG_INF("set current fps :%d\n", (UINT32)*feature_data);
		spin_lock(&imgsensor_drv_lock);
		imgsensor.current_fps = *feature_data;
		spin_unlock(&imgsensor_drv_lock);
		break;
	case SENSOR_FEATURE_SET_HDR:
		LOG_INF("ihdr enable :%d\n", (BOOL)*feature_data);
		spin_lock(&imgsensor_drv_lock);
		imgsensor.ihdr_mode = (BOOL)*feature_data;
		spin_unlock(&imgsensor_drv_lock);
		break;
	case SENSOR_FEATURE_GET_CROP_INFO:
		LOG_INF("SENSOR_FEATURE_GET_CROP_INFO, scenario id = %d\n",
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
		case MSDK_SCENARIO_ID_CUSTOM1:
			memcpy((void *)wininfo,
				(void *)&imgsensor_winsize_info[3],
				sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_CUSTOM2:
			memcpy((void *)wininfo,
				(void *)&imgsensor_winsize_info[4],
				sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_CUSTOM3:
			memcpy((void *)wininfo,
				(void *)&imgsensor_winsize_info[5],
				sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_CUSTOM4:
			memcpy(
				(void *)wininfo,
				(void *)&imgsensor_winsize_info[6],
				sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
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
			(UINT16)*feature_data, (UINT16)*(feature_data + 1), (UINT16)*(feature_data + 2));
		ihdr_write_shutter_gain((UINT16)*feature_data, (UINT16)*(feature_data + 1),
				(UINT16)*(feature_data + 2));
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
	default:
		break;
	}

	return ERROR_NONE;
}

static struct SENSOR_FUNCTION_STRUCT sensor_func = {
	open,
	get_info,
	get_resolution,
	feature_control,
	control,
	close
};

UINT32 IMX258_MIPI_RAW_SensorInit(struct SENSOR_FUNCTION_STRUCT **pfFunc)
/*UINT32 IMX258_MIPI_SensorInit(struct SENSOR_FUNCTION_STRUCT **pfFunc)*/
{
	/* To Do : Check Sensor status here */
	if (pfFunc != NULL)
		*pfFunc = &sensor_func;
	return ERROR_NONE;
}				/*      IMX258_MIPI_RAW_SensorInit      */
