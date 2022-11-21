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

#include "kd_imgsensor.h"
#include "kd_imgsensor_define.h"
#include "kd_imgsensor_errcode.h"
#include "kd_camera_typedef.h"
#include "sr846dmipiraw_Sensor.h"
#include "imgsensor_sysfs.h"

#define I2C_BURST_WRITE_SUPPORT

#define PFX "sr846d_camera_sensor"
#define LOG_INF(format, args...)    \
	pr_info(PFX "[%s] " format, __func__, ##args)

static DEFINE_SPINLOCK(imgsensor_drv_lock);

#define per_frame 1

static struct imgsensor_info_struct imgsensor_info = {
	.sensor_id = SR846D_SENSOR_ID,
	.checksum_value = 0x55e2a82f,
	.pre = {
		.pclk = 287733300,
		.linelength = 3800,
		.framelength = 2524,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 3264,
		.grabwindow_height = 2448,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 272132000,
		.max_framerate = 300,
	},
	.cap = {
		.pclk = 287733300,
		.linelength = 3800,
		.framelength = 2524,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 3264,
		.grabwindow_height = 2448,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 272132000,
		.max_framerate = 300,
	},
	.normal_video = {
		.pclk = 287733300,
		.linelength = 3800,
		.framelength = 2523,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 3264,
		.grabwindow_height = 1836,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 272132000,
		.max_framerate = 300,
	},
	.hs_video = { // NOT USED
		.pclk = 287733300,
		.linelength = 3800,
		.framelength = 2523,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 3264,
		.grabwindow_height = 2448,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 272132000,
		.max_framerate = 300,
	},
	.slim_video = { // NOT USED
		.pclk = 287733300,
		.linelength = 3800,
		.framelength = 2523,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 3264,
		.grabwindow_height = 2448,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 272132000,
		.max_framerate = 300,
	},
	.custom1 = {
		.pclk = 287733300,
		.linelength = 3800,
		.framelength = 1261,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 1576,
		.grabwindow_height = 1182,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 136064000,
		.max_framerate = 600,
	},
	.custom2 = { // NOT USED
		.pclk = 287733300,
		.linelength = 3800,
		.framelength = 2523,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 2640,
		.grabwindow_height = 1980,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 272132000,
		.max_framerate = 300,
	},
	.custom3 = { // NOT USED
		.pclk = 287733300,
		.linelength = 3800,
		.framelength = 2523,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 2640,
		.grabwindow_height = 1980,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 272132000,
		.max_framerate = 300,
	},
	.custom4 = { // NOT USED
		.pclk = 287733300,
		.linelength = 3800,
		.framelength = 2523,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 2640,
		.grabwindow_height = 1488,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 272132000,
		.max_framerate = 300,
	},
	.custom5 = { // NOT USED
		.pclk = 287733300,
		.linelength = 3800,
		.framelength = 2523,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 3264,
		.grabwindow_height = 2448,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 272132000,
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
	.sensor_mode_num = 10,	  //support sensor mode num

	.cap_delay_frame = 2,
	.pre_delay_frame = 2,
	.video_delay_frame = 2,
	.hs_video_delay_frame = 2,
	.slim_video_delay_frame = 2,
	.custom1_delay_frame = 2,
	.custom2_delay_frame = 2,
	.custom3_delay_frame = 2,
	.custom4_delay_frame = 2,
	.custom5_delay_frame = 2,
	.frame_time_delay_frame = 2,

	.min_gain = 64, /*1x gain*/
	.max_gain = 1024, /*16x gain*/
	.min_gain_iso = 40,
	.gain_step = 1,
	.gain_type = 4,
	.exp_step = 1,
	.isp_driving_current = ISP_DRIVING_6MA,
	.sensor_interface_type = SENSOR_INTERFACE_TYPE_MIPI,
	.mipi_sensor_type = MIPI_OPHY_NCSI2,
	.mipi_settle_delay_mode = MIPI_SETTLEDELAY_AUTO,
	.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_Gr,
	.mclk = 26,
	.mipi_lane_num = SENSOR_MIPI_4_LANE,
	.i2c_addr_table = {0x42, 0xff},
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
	.i2c_write_id = 0x42,
};

/* Sensor output window information */
static struct SENSOR_WINSIZE_INFO_STRUCT imgsensor_winsize_info[] = {
	{ 3280, 2464,    0,    0, 3280, 2464, 3280, 2464,
	     8,    8, 3264, 2448,    0,    0, 3264, 2448 }, /*preview*/
	{ 3280, 2464,    0,    0, 3280, 2464, 3280, 2464,
	     8,    8, 3264, 2448,    0,    0, 3264, 2448 }, /*capture*/
	{ 3280, 2464,    0,    0, 3280, 2464, 3280, 2464,
	     8,  314, 3264, 1836,    0,    0, 3264, 1836 }, /*video*/
	{ 3280, 2464,    0,    0, 3280, 2464, 3280, 2464,
	     8,    8, 3264, 2448,    0,    0, 3264, 2448 }, /*high speed-NOT USED*/
	{ 3280, 2464,    0,    0, 3280, 2464, 3280, 2464,
	     8,    8, 3264, 2448,    0,    0, 3264, 2448 }, /*slim video-NOT USED*/
	{ 3280, 2464,    0,    0, 3280, 2464, 1640, 1232,
	    32,   24, 1576, 1182,    0,    0, 1576, 1182 }, /*custom1*/
	{ 3280, 2464,    0,    0, 3280, 2464, 3280, 2464,
	   320,  242, 2640, 1980,    0,    0, 2640, 1980 }, /*custom2-NOT USED*/
	{ 3280, 2464,    0,    0, 3280, 2464, 3280, 2464,
	   320,  242, 2640, 1980,    0,    0, 2640, 1980 }, /*custom3-NOT USED*/
	{ 3280, 2464,    0,    0, 3280, 2464, 3280, 2464,
	   320,  488, 2640, 1488,    0,    0, 2640, 1488 }, /*custom4-NOT USED*/
	{ 3280, 2464,    0,    0, 3280, 2464, 3280, 2464,
	     8,    8, 3264, 2448,    0,    0, 3264, 2448 }, /*custom5-NOT USED*/
};

#define I2C_BUFFER_LEN 770
#define I2C_BURST_BUFFER_LEN (I2C_BUFFER_LEN * 2)


#if defined I2C_BURST_WRITE_SUPPORT
static kal_uint16 sr846d_table_write_cmos_sensor(kal_uint16 *para, kal_uint32 len)
{
	char puSendCmd[I2C_BURST_BUFFER_LEN + 2];
	kal_uint32 tosend = 0, IDX = 0;
	kal_uint16 addr = 0, data = 0;

	while (len > IDX) {
		addr = para[IDX];
		if (tosend == 0) {
			puSendCmd[tosend++] = (char)(addr >> 8);
			puSendCmd[tosend++] = (char)(addr & 0xFF);
		}

		data = para[IDX + 1];
		puSendCmd[tosend++] = (char)(data >> 8);
		puSendCmd[tosend++] = (char)(data & 0xFF);
		IDX += 2;

		if ((len == IDX) ||
			((len > IDX) && (para[IDX] != (addr + 2)))) {
			iBurstWriteReg_multi(puSendCmd, tosend,
								imgsensor.i2c_write_id,
								tosend, imgsensor_info.i2c_speed);

			tosend = 0;
		}
	}
	return 0;
}

#else
static kal_uint16 sr846d_table_write_cmos_sensor(kal_uint16 *para, kal_uint32 len)
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

static kal_uint8 sr846d_otp_read_byte(kal_uint16 addr)
{
	write_cmos_sensor_8(0x070A, (addr >> 8) & 0xFF); //High address
	write_cmos_sensor_8(0x070B, addr & 0xFF); //Low address
	write_cmos_sensor_8(0x0702, 0x01);

	return (kal_uint8)read_cmos_sensor(0x0708);
}

static void set_dummy(void)
{
	pr_debug("dummyline = %d, dummypixels = %d\n",
		imgsensor.dummy_line, imgsensor.dummy_pixel);
	write_cmos_sensor(0x0006, imgsensor.frame_length & 0xFFFF);
	write_cmos_sensor(0x0008, imgsensor.line_length & 0xFFFF);

}	/*	set_dummy  */

static kal_uint32 return_sensor_id(void)
{
	return ((read_cmos_sensor(0x0F17) << 8) | read_cmos_sensor(0x0F16));
}

#define MAX_WAIT_POLL_CNT 10
#define POLL_TIME_USEC  5000
#define STREAM_OFF_CHECK_REGISTER_ADDR 0x0F02
static void wait_stream_off(void)
{
	kal_uint16 read_value = 0;
	kal_uint16 wait_poll_cnt = 0;

	do {
		read_value = read_cmos_sensor(STREAM_OFF_CHECK_REGISTER_ADDR);
		read_value = (read_value & 0x01);
		if (read_value != 0x01)
			break;
		wait_poll_cnt++;
		usleep_range(POLL_TIME_USEC, POLL_TIME_USEC + 10);
	} while (wait_poll_cnt < MAX_WAIT_POLL_CNT);

	LOG_INF("wait time : %d msec", wait_poll_cnt * (POLL_TIME_USEC / 1000));
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

	// Update Shutter - 20bits (0x0073[3:0], 0x0074[7:0], 0x0075[7:0])
	write_cmos_sensor_8(0x0073, ((shutter & 0x0F0000) >> 16));
	write_cmos_sensor(0x0074, shutter & 0x00FFFF);
	pr_debug("shutter =%d, framelength =%d",
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

	pr_debug("%s", __func__);
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
	pr_debug("gain = %d , reg_gain = 0x%x\n ", gain, reg_gain);

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

kal_uint16 addr_data_pair_init_sr846d[] = {
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

static void sensor_init(void)
{
	int ret = 0;
#ifdef IMGSENSOR_HW_PARAM
	struct cam_hw_param *hw_param = NULL;
#endif

	ret = sr846d_table_write_cmos_sensor(
		addr_data_pair_init_sr846d,
		sizeof(addr_data_pair_init_sr846d) /
		sizeof(kal_uint16));
#ifdef IMGSENSOR_HW_PARAM
	if (ret != 0) {
		imgsensor_sec_get_hw_param(&hw_param, SR846D_CAL_SENSOR_POSITION);
		if (hw_param)
			hw_param->i2c_sensor_err_cnt++;
	}
#endif
}

kal_uint16 addr_data_pair_preview_capture_sr846d[] = {
	0x0A20, 0x0000,
	0x002E, 0x1111,
	0x0032, 0x1111,
	0x0022, 0x0008,
	0x0026, 0x0040,
	0x0028, 0x0017,
	0x002C, 0x09CF,
	0x005C, 0x2101,
	0x0006, 0x09DC,
	0x0008, 0x0ED8,
	0x000E, 0x0200,
	0x000C, 0x0022,
	0x0A22, 0x0000,
	0x0A24, 0x0000,
	0x0804, 0x0000,
	0x0A12, 0x0CC0,
	0x0A14, 0x0990,
	0x0074, 0x09D6,
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

static void preview_setting(void)
{
	LOG_INF("E\n");
	sr846d_table_write_cmos_sensor(
		addr_data_pair_preview_capture_sr846d,
		sizeof(addr_data_pair_preview_capture_sr846d) /
		sizeof(kal_uint16));
}

static void capture_setting(kal_uint16 currefps)
{
	LOG_INF("E\n");
	sr846d_table_write_cmos_sensor(
		addr_data_pair_preview_capture_sr846d,
		sizeof(addr_data_pair_preview_capture_sr846d) /
		sizeof(kal_uint16));
}

kal_uint16 addr_data_pair_normal_video_sr846d[] = {
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

static void normal_video_setting(void)
{
	LOG_INF("E\n");
	sr846d_table_write_cmos_sensor(
		addr_data_pair_normal_video_sr846d,
		sizeof(addr_data_pair_normal_video_sr846d) /
		sizeof(kal_uint16));
}

static void hs_video_setting(void)
{
	LOG_INF("E\n");
	// NOT USED
}

static void slim_video_setting(void)
{
	LOG_INF("E\n");
	// NOT USED
}

kal_uint16 addr_data_pair_custom1_sr846d[] = {
	0x0A20, 0x0000,
	0x002E, 0x3311,
	0x0032, 0x3311,
	0x0022, 0x0008,
	0x0026, 0x0068,
	0x0028, 0x0017,
	0x002C, 0x09A3,
	0x005C, 0x4202,
	0x0006, 0x04ED,
	0x0008, 0x0ED8,
	0x000E, 0x0200,
	0x000C, 0x0122,
	0x0A22, 0x0100,
	0x0A24, 0x0000,
	0x0804, 0x001C,
	0x0A12, 0x0628,
	0x0A14, 0x049E,
	0x0074, 0x04E7,
	0x0076, 0x0000,
	0x0A0C, 0x0010,
	0x0A1E, 0x0CCF,
	0x0A04, 0x016A,
	0x0418, 0x002A,
	0x0128, 0x0025,
	0x012A, 0xFFFF,
	0x0120, 0x0041,
	0x0122, 0x0371,
	0x012C, 0x001E,
	0x012E, 0xFFFF,
	0x0124, 0x003B,
	0x0126, 0x0373,
	0x0B02, 0xE04D,
	0x0B10, 0x6C21,
	0x0B12, 0x0030,
	0x0B14, 0x0005,
	0x0A0A, 0x38FD,
	0x0A1C, 0x0000,
	0x0710, 0x04B6,
	0x0900, 0x0300,
	0x0902, 0xC319,
	0x0914, 0xC105,
	0x0916, 0x030C,
	0x0918, 0x0304,
	0x091A, 0x0708,
	0x091C, 0x0B04,
	0x091E, 0x0600,
	0x090C, 0x01EC,
	0x090E, 0x000B,
	0x0F08, 0x2F04,
	0x0F30, 0x001F,
	0x0F36, 0x001F,
	0x0F04, 0x3A00,
	0x0F32, 0x0253,
	0x0F38, 0x059D,
	0x0F2A, 0x4924,
	0x004C, 0x0100
};

static void custom1_setting(void)
{
	LOG_INF("E\n");
	sr846d_table_write_cmos_sensor(
		addr_data_pair_custom1_sr846d,
		sizeof(addr_data_pair_custom1_sr846d) /
		sizeof(kal_uint16));
}

kal_uint16 addr_data_pair_custom2_3_sr846d[] = {
	0x0A20, 0x0000,
	0x002E, 0x1111,
	0x0032, 0x1111,
	0x0022, 0x0008,
	0x0026, 0x012A,
	0x0028, 0x0017,
	0x002C, 0x08E5,
	0x005C, 0x2101,
	0x0006, 0x09DB,
	0x0008, 0x0ED8,
	0x000E, 0x0200,
	0x000C, 0x0022,
	0x0A22, 0x0000,
	0x0A24, 0x0000,
	0x0804, 0x0138,
	0x0A12, 0x0A50,
	0x0A14, 0x07BC,
	0x0074, 0x09D5,
	0x0076, 0x0000,
	0x0A0C, 0x0010,
	0x0A1E, 0x0CCF,
	0x0A04, 0x014A,
	0x0418, 0x00EA,
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
	0x0710, 0x07DC,
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

static void custom2_setting(void)
{
	// NOT USED
}

static void custom3_setting(void)
{
	// NOT USED
}

kal_uint16 addr_data_pair_custom4_sr846d[] = {
	0x0A20, 0x0000,
	0x002E, 0x1111,
	0x0032, 0x1111,
	0x0022, 0x0008,
	0x0026, 0x0220,
	0x0028, 0x0017,
	0x002C, 0x07EF,
	0x005C, 0x2101,
	0x0006, 0x09DB,
	0x0008, 0x0ED8,
	0x000E, 0x0200,
	0x000C, 0x0022,
	0x0A22, 0x0000,
	0x0A24, 0x0000,
	0x0804, 0x0138,
	0x0A12, 0x0A50,
	0x0A14, 0x05D0,
	0x0074, 0x09D5,
	0x0076, 0x0000,
	0x0A0C, 0x0010,
	0x0A1E, 0x0CCF,
	0x0A04, 0x014A,
	0x0418, 0x02EC,
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
	0x0710, 0x05F0,
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

static void custom4_setting(void)
{
	// NOT USED
}

static void custom5_setting(void)
{
	// NOT USED
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

	imgsensor.pclk = imgsensor_info.cap.pclk;
	imgsensor.line_length = imgsensor_info.cap.linelength;
	imgsensor.frame_length = imgsensor_info.cap.framelength;
	imgsensor.min_frame_length = imgsensor_info.cap.framelength;
	imgsensor.autoflicker_en = KAL_FALSE;

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
	normal_video_setting();
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

static kal_uint32 custom1(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
						  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_CUSTOM1;

	imgsensor.pclk = imgsensor_info.custom1.pclk;
	imgsensor.line_length = imgsensor_info.custom1.linelength;
	imgsensor.frame_length = imgsensor_info.custom1.framelength;
	imgsensor.min_frame_length = imgsensor_info.custom1.framelength;
	imgsensor.dummy_line = 0;
	imgsensor.dummy_pixel = 0;
	imgsensor.autoflicker_en = KAL_FALSE;

	spin_unlock(&imgsensor_drv_lock);
	LOG_INF("%s fps:%d\n", __func__, imgsensor.current_fps);
	custom1_setting();

	return ERROR_NONE;
}	/* custom1() */

static kal_uint32 custom2(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
						  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_CUSTOM2;

	imgsensor.pclk = imgsensor_info.custom2.pclk;
	imgsensor.line_length = imgsensor_info.custom2.linelength;
	imgsensor.frame_length = imgsensor_info.custom2.framelength;
	imgsensor.min_frame_length = imgsensor_info.custom2.framelength;
	imgsensor.dummy_line = 0;
	imgsensor.dummy_pixel = 0;
	imgsensor.autoflicker_en = KAL_FALSE;

	spin_unlock(&imgsensor_drv_lock);
	LOG_INF("%s fps:%d\n", __func__, imgsensor.current_fps);
	custom2_setting();

	return ERROR_NONE;
}	/* custom2() */

static kal_uint32 custom3(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
						  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_CUSTOM3;

	imgsensor.pclk = imgsensor_info.custom3.pclk;
	imgsensor.line_length = imgsensor_info.custom3.linelength;
	imgsensor.frame_length = imgsensor_info.custom3.framelength;
	imgsensor.min_frame_length = imgsensor_info.custom3.framelength;
	imgsensor.dummy_line = 0;
	imgsensor.dummy_pixel = 0;
	imgsensor.autoflicker_en = KAL_FALSE;

	spin_unlock(&imgsensor_drv_lock);
	LOG_INF("%s fps:%d\n", __func__, imgsensor.current_fps);
	custom3_setting();

	return ERROR_NONE;
}	/* custom3() */

static kal_uint32 custom4(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_CUSTOM4;

	imgsensor.pclk = imgsensor_info.custom4.pclk;
	imgsensor.line_length = imgsensor_info.custom4.linelength;
	imgsensor.frame_length = imgsensor_info.custom4.framelength;
	imgsensor.min_frame_length = imgsensor_info.custom4.framelength;
	imgsensor.dummy_line = 0;
	imgsensor.dummy_pixel = 0;
	imgsensor.autoflicker_en = KAL_FALSE;

	spin_unlock(&imgsensor_drv_lock);
	LOG_INF("%s fps:%d\n", __func__, imgsensor.current_fps);
	custom4_setting();

	return ERROR_NONE;
}	/* custom4() */

static kal_uint32 custom5(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_CUSTOM5;

	imgsensor.pclk = imgsensor_info.custom5.pclk;
	imgsensor.line_length = imgsensor_info.custom5.linelength;
	imgsensor.frame_length = imgsensor_info.custom5.framelength;
	imgsensor.min_frame_length = imgsensor_info.custom5.framelength;
	imgsensor.dummy_line = 0;
	imgsensor.dummy_pixel = 0;
	imgsensor.autoflicker_en = KAL_FALSE;

	spin_unlock(&imgsensor_drv_lock);
	LOG_INF("%s fps:%d\n", __func__, imgsensor.current_fps);
	custom5_setting();

	return ERROR_NONE;
}	/* custom5() */

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

	sensor_resolution->SensorCustom5Width =
		imgsensor_info.custom5.grabwindow_width;
	sensor_resolution->SensorCustom5Height =
		imgsensor_info.custom5.grabwindow_height;

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
	sensor_info->VideoDelayFrame = imgsensor_info.video_delay_frame;
	sensor_info->HighSpeedVideoDelayFrame = imgsensor_info.hs_video_delay_frame;
	sensor_info->SlimVideoDelayFrame = imgsensor_info.slim_video_delay_frame;
	sensor_info->Custom1DelayFrame = imgsensor_info.custom1_delay_frame;
	sensor_info->Custom2DelayFrame = imgsensor_info.custom2_delay_frame;
	sensor_info->Custom3DelayFrame = imgsensor_info.custom3_delay_frame;
	sensor_info->Custom4DelayFrame = imgsensor_info.custom4_delay_frame;
	sensor_info->Custom5DelayFrame = imgsensor_info.custom5_delay_frame;

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
	case MSDK_SCENARIO_ID_CUSTOM5:
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
	case MSDK_SCENARIO_ID_CUSTOM5:
		custom5(image_window, sensor_config_data);
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
	pr_debug("enable = %d, framerate = %d ", enable, framerate);
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

	pr_debug("scenario_id = %d, framerate = %d\n",
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
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
		break;
	case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
		frame_length = imgsensor_info.hs_video.pclk /
			framerate * 10 / imgsensor_info.hs_video.linelength;
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
		frame_length = imgsensor_info.slim_video.pclk /
			framerate * 10 / imgsensor_info.slim_video.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line = (frame_length > imgsensor_info.slim_video.framelength) ?
			(frame_length - imgsensor_info.slim_video.framelength) : 0;
		imgsensor.frame_length = imgsensor_info.slim_video.framelength + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
		break;
	case MSDK_SCENARIO_ID_CUSTOM1:
		frame_length = imgsensor_info.custom1.pclk /
			framerate * 10 / imgsensor_info.custom1.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line = (frame_length > imgsensor_info.custom1.framelength) ?
			(frame_length - imgsensor_info.custom1.framelength) : 0;
		imgsensor.frame_length = imgsensor_info.custom1.framelength + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
		break;
	case MSDK_SCENARIO_ID_CUSTOM2:
		frame_length = imgsensor_info.custom2.pclk /
			framerate * 10 / imgsensor_info.custom2.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line = (frame_length > imgsensor_info.custom2.framelength) ?
			(frame_length - imgsensor_info.custom2.framelength) : 0;
		imgsensor.frame_length = imgsensor_info.custom2.framelength + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
		break;
	case MSDK_SCENARIO_ID_CUSTOM3:
		frame_length = imgsensor_info.custom3.pclk /
			framerate * 10 / imgsensor_info.custom3.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line = (frame_length > imgsensor_info.custom3.framelength) ?
			(frame_length - imgsensor_info.custom3.framelength) : 0;
		imgsensor.frame_length = imgsensor_info.custom3.framelength + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
		break;
	case MSDK_SCENARIO_ID_CUSTOM4:
		frame_length = imgsensor_info.custom4.pclk /
			framerate * 10 / imgsensor_info.custom4.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line = (frame_length > imgsensor_info.custom4.framelength) ?
			(frame_length - imgsensor_info.custom4.framelength) : 0;
		imgsensor.frame_length = imgsensor_info.custom4.framelength + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
		break;
	case MSDK_SCENARIO_ID_CUSTOM5:
		frame_length = imgsensor_info.custom5.pclk /
			framerate * 10 / imgsensor_info.custom5.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line = (frame_length > imgsensor_info.custom5.framelength) ?
			(frame_length - imgsensor_info.custom5.framelength) : 0;
		imgsensor.frame_length = imgsensor_info.custom5.framelength + imgsensor.dummy_line;
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

	if (enable) {
		write_cmos_sensor(0x0A20, 0x0100);
		usleep_range(10000, 10100);
	} else {
		write_cmos_sensor(0x0F10, 0x0A00);
		write_cmos_sensor(0x0A20, 0x0000);
		wait_stream_off();
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

	pr_debug("feature_id = %d\n", feature_id);
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
		case MSDK_SCENARIO_ID_CUSTOM5:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= (imgsensor_info.custom5.framelength << 16)
					+ imgsensor_info.custom5.linelength;
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
			case MSDK_SCENARIO_ID_CUSTOM5:
				rate = imgsensor_info.custom5.mipi_pixel_rate;
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
		case MSDK_SCENARIO_ID_CUSTOM1:
			memcpy((void *)wininfo,
				(void *)&imgsensor_winsize_info[5],
				sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_CUSTOM2:
			memcpy((void *)wininfo,
				(void *)&imgsensor_winsize_info[6],
				sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_CUSTOM3:
			memcpy((void *)wininfo,
				(void *)&imgsensor_winsize_info[7],
				sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_CUSTOM4:
			memcpy((void *)wininfo,
				(void *)&imgsensor_winsize_info[8],
				sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_CUSTOM5:
			memcpy((void *)wininfo,
				(void *)&imgsensor_winsize_info[9],
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
}    /*    feature_control()  */

static struct SENSOR_FUNCTION_STRUCT sensor_func = {
	open,
	get_info,
	get_resolution,
	feature_control,
	control,
	close
};

int sr846d_read_otp_cal(unsigned int addr, unsigned char *data, unsigned int size)
{
	kal_uint32 i = 0;
	kal_uint8 otp_bank = 0;
	kal_uint16 read_addr = 0;

	pr_debug("%s - E\n", __func__);

	if (data == NULL) {
		pr_err("%s: fail, data is NULL\n", __func__);
		return -1;
	}

	streaming_control(true);

	// OTP initial setting
	write_cmos_sensor_8(0x0A20, 0x00);
	write_cmos_sensor_8(0x0A00, 0x00);
	usleep_range(10000, 10100);
	write_cmos_sensor_8(0x0F02, 0x00);
	write_cmos_sensor_8(0x071A, 0x01);
	write_cmos_sensor_8(0x071B, 0x09);
	write_cmos_sensor_8(0x0D04, 0x01);
	write_cmos_sensor_8(0x0D00, 0x07);

	write_cmos_sensor_8(0x003E, 0x10);
	write_cmos_sensor_8(0x0A20, 0x01);
	write_cmos_sensor_8(0x0A00, 0x01);
	usleep_range(1000, 1100);

	// Select OTP Bank
	otp_bank = sr846d_otp_read_byte(SR846D_OTP_CHECK_BANK);
	pr_info("%s, check OTP bank:0x%x, size: %d\n", __func__, otp_bank, size);

	switch (otp_bank) {
	case SR846D_OTP_BANK1_MARK:
		read_addr = SR846D_OTP_BANK1_START_ADDR;
		break;
	case SR846D_OTP_BANK2_MARK:
		read_addr = SR846D_OTP_BANK2_START_ADDR;
		break;
	default:
		pr_err("%s, check bank: fail\n", __func__);
		return -1;
	}

	for (i = 0; i < size; i++) {
		*(data + i) = sr846d_otp_read_byte(read_addr);
		pr_debug("read data addr:0x%x, value:0x%x, i:%d\n", read_addr, *(data + i), i);
		read_addr += 1;
	}
	// Streaming mode change
	write_cmos_sensor_8(0x0A20, 0x00);
	write_cmos_sensor_8(0x0A00, 0x00);
	msleep(100);

	write_cmos_sensor_8(0x003E, 0x00);
	write_cmos_sensor_8(0x0A20, 0x01);
	write_cmos_sensor_8(0x0A00, 0x01);

	// SW reset
	write_cmos_sensor_8(0x0F00, 0x01);
	usleep_range(1000, 1100);
	write_cmos_sensor_8(0x0F00, 0x00);
	sensor_init();
	pr_debug("%s - X\n", __func__);
	return (int)size;
}

UINT32 SR846D_MIPI_RAW_SensorInit(struct SENSOR_FUNCTION_STRUCT **pfFunc)
{
	/* To Do : Check Sensor status here */
	if (pfFunc != NULL)
		*pfFunc =  &sensor_func;
	return ERROR_NONE;
}	/*	SR846D_MIPI_RAW_SensorInit	*/
