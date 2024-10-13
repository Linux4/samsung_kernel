/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2022 Samsung Electronics Co., Ltd
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

#include "kd_imgsensor_sysfs_adapter.h"

#include "sr846dmipiraw_Sensor.h"
#include "sr846dmipiraw_setfile.h"
#include "imgsensor_sysfs.h"

#define I2C_BURST_WRITE_SUPPORT

#define PFX "SR846D D/D"
#define LOG_DBG(format, args...) pr_debug(PFX "[%s] " format, __func__, ##args)
#define LOG_INF(format, args...) pr_info(PFX "[%s] " format, __func__, ##args)
#define LOG_ERR(format, args...) pr_err(PFX "[%s] " format, __func__, ##args)

static DEFINE_SPINLOCK(imgsensor_drv_lock);

#define per_frame 1

static kal_uint32 streaming_control(kal_bool enable);

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
	.custom2 = {
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
	.custom3 = {
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
	.custom4 = {
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

	// set sensor_mode
	.mode_info[IMGSENSOR_MODE_PREVIEW]          = &imgsensor_info.pre,
	.mode_info[IMGSENSOR_MODE_CAPTURE]          = &imgsensor_info.cap,
	.mode_info[IMGSENSOR_MODE_VIDEO]            = &imgsensor_info.normal_video,
	.mode_info[IMGSENSOR_MODE_HIGH_SPEED_VIDEO] = &imgsensor_info.hs_video,
	.mode_info[IMGSENSOR_MODE_SLIM_VIDEO]       = &imgsensor_info.slim_video,
	.mode_info[IMGSENSOR_MODE_CUSTOM1]          = &imgsensor_info.custom1,
	.mode_info[IMGSENSOR_MODE_CUSTOM2]          = &imgsensor_info.custom2,
	.mode_info[IMGSENSOR_MODE_CUSTOM3]          = &imgsensor_info.custom3,
	.mode_info[IMGSENSOR_MODE_CUSTOM4]          = &imgsensor_info.custom4,
	.mode_info[IMGSENSOR_MODE_CUSTOM5]          = &imgsensor_info.custom5,

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

#if IS_ENABLED(CONFIG_CAMERA_AAU_V32)
	.min_gain_iso = 100,
#elif IS_ENABLED(CONFIG_CAMERA_AAW_V34X)
	.min_gain_iso = 40,
#else
	.min_gain_iso = 50,
#endif

	.gain_step = 1,
	.gain_type = 4,
	.exp_step = 1,
	.isp_driving_current = ISP_DRIVING_6MA,
	.sensor_interface_type = SENSOR_INTERFACE_TYPE_MIPI,
	.mipi_sensor_type = MIPI_OPHY_NCSI2,
	.mipi_settle_delay_mode = MIPI_SETTLEDELAY_AUTO, //0: Auto, 1: Manual
	.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_Gr,
	.mclk = 26,
	.mipi_lane_num = SENSOR_MIPI_4_LANE,
	.i2c_addr_table = {0x40, 0x42, 0xff},
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
static struct SENSOR_WINSIZE_INFO_STRUCT imgsensor_winsize_info[] = {
	{    0,    0,    0,    0,    0,    0,    0,    0,
	     0,    0,    0,    0,    0,    0,    0,    0 }, /*init-NOT USED*/
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
	   320,  242, 2640, 1980,    0,    0, 2640, 1980 }, /*custom2*/
	{ 3280, 2464,    0,    0, 3280, 2464, 3280, 2464,
	   320,  242, 2640, 1980,    0,    0, 2640, 1980 }, /*custom3*/
	{ 3280, 2464,    0,    0, 3280, 2464, 3280, 2464,
	   320,  488, 2640, 1488,    0,    0, 2640, 1488 }, /*custom4*/
	{ 3280, 2464,    0,    0, 3280, 2464, 3280, 2464,
	     8,    8, 3264, 2448,    0,    0, 3264, 2448 }, /*custom5-NOT USED*/
};

#define I2C_BUFFER_LEN 512
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

		if ((len == IDX) || ((len > IDX) && (para[IDX] != (addr + 2))) || tosend >= (I2C_BURST_BUFFER_LEN - 4)) {
			iBurstWriteReg_multi(puSendCmd, tosend, imgsensor.i2c_write_id, tosend, imgsensor_info.i2c_speed);
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
	kal_uint16 addr = 0, addr_last = 0, data = 0;

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
				imgsensor.i2c_write_id, 4, imgsensor_info.i2c_speed);
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

static enum IMGSENSOR_MODE get_imgsensor_mode(enum MSDK_SCENARIO_ID_ENUM scenario_id)
{
	enum IMGSENSOR_MODE mode = IMGSENSOR_MODE_PREVIEW;

	imgsensor_info.is_invalid_mode = false;
	switch (scenario_id) {
	case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		mode = IMGSENSOR_MODE_PREVIEW;
		break;
	case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		mode = IMGSENSOR_MODE_CAPTURE;
		break;
	case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
		mode = IMGSENSOR_MODE_VIDEO;
		break;
	case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
		mode = IMGSENSOR_MODE_HIGH_SPEED_VIDEO;
		break;
	case MSDK_SCENARIO_ID_SLIM_VIDEO:
		mode = IMGSENSOR_MODE_SLIM_VIDEO;
		break;
	case MSDK_SCENARIO_ID_CUSTOM1:
		mode = IMGSENSOR_MODE_CUSTOM1;
		break;
	case MSDK_SCENARIO_ID_CUSTOM2:
		mode = IMGSENSOR_MODE_CUSTOM2;
		break;
	case MSDK_SCENARIO_ID_CUSTOM3:
		mode = IMGSENSOR_MODE_CUSTOM3;
		break;
	case MSDK_SCENARIO_ID_CUSTOM4:
		mode = IMGSENSOR_MODE_CUSTOM4;
		break;
	case MSDK_SCENARIO_ID_CUSTOM5:
		mode = IMGSENSOR_MODE_CUSTOM5;
		break;
	default:
		LOG_DBG("error scenario_id = %d, we use preview scenario", scenario_id);
		imgsensor_info.is_invalid_mode = true;
		mode = IMGSENSOR_MODE_PREVIEW;
		break;
	}
	LOG_DBG("sensor_mode = %d\n", mode);
	return mode;
}

static void set_dummy(void)
{
	LOG_DBG("dummyline = %d, dummypixels = %d\n",
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
	imgsensor.dummy_line = imgsensor.frame_length - imgsensor.min_frame_length;

	if (imgsensor.frame_length > imgsensor_info.max_frame_length) {
		imgsensor.frame_length = imgsensor_info.max_frame_length;
		imgsensor.dummy_line = imgsensor.frame_length - imgsensor.min_frame_length;
	}
	if (min_framelength_en)
		imgsensor.min_frame_length = imgsensor.frame_length;

	spin_unlock(&imgsensor_drv_lock);
	set_dummy();
}	/*	set_max_framerate  */

/*************************************************************************
 * FUNCTION
 *	set_shutter_frame_length
 *
 * DESCRIPTION
 *	for frame & 3A sync
 *
 *************************************************************************/

static void set_shutter_frame_length(UINT32 shutter, UINT32 frame_length)
{
	unsigned long flags;
	kal_uint16 realtime_fps = 0;
	kal_int32 dummy_line = 0;

	spin_lock_irqsave(&imgsensor_drv_lock, flags);
	imgsensor.shutter = shutter;
	spin_unlock_irqrestore(&imgsensor_drv_lock, flags);

	spin_lock(&imgsensor_drv_lock);

	dummy_line = frame_length - imgsensor.frame_length;
	imgsensor.frame_length = imgsensor.frame_length + dummy_line;
	imgsensor.min_frame_length = imgsensor.frame_length;

	if (shutter > imgsensor.frame_length - imgsensor_info.margin)
		imgsensor.frame_length = shutter + imgsensor_info.margin;

	if (imgsensor.frame_length > imgsensor_info.max_frame_length)
		imgsensor.frame_length = imgsensor_info.max_frame_length;

	spin_unlock(&imgsensor_drv_lock);

	shutter = (shutter < imgsensor_info.min_shutter) ? imgsensor_info.min_shutter : shutter;
	shutter = (shutter > (imgsensor_info.max_frame_length - imgsensor_info.margin)) ?
				(imgsensor_info.max_frame_length - imgsensor_info.margin) : shutter;

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
			write_cmos_sensor(0x0006, imgsensor.frame_length);
		}
	} else {
		// Extend frame length
		write_cmos_sensor(0x0006, imgsensor.frame_length);
	}

	// Update Shutter - 20bits (0x0073[3:0], 0x0074[7:0], 0x0075[7:0])
	write_cmos_sensor_8(0x0073, ((shutter & 0x0F0000) >> 16));
	write_cmos_sensor(0x0074, shutter & 0x00FFFF);

	LOG_DBG("realtime_fps =%d\n", realtime_fps);
	LOG_DBG("shutter =%d, framelength =%d\n", shutter, imgsensor.frame_length);
}

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
		realtime_fps = imgsensor.pclk * 10 / (imgsensor.line_length * imgsensor.frame_length);
		if (realtime_fps >= 297 && realtime_fps <= 305)
			set_max_framerate(296, 0);
		else if (realtime_fps >= 147 && realtime_fps <= 150)
			set_max_framerate(146, 0);
		else
			write_cmos_sensor(0x0006, imgsensor.frame_length);

	} else {
		// Extend frame length

		// ADD ODIN
		realtime_fps = imgsensor.pclk * 10 / (imgsensor.line_length * imgsensor.frame_length);
		if (realtime_fps > 300 && realtime_fps < 320)
			set_max_framerate(300, 0);
		// ADD END
		write_cmos_sensor(0x0006, imgsensor.frame_length);
	}

	// Update Shutter - 20bits (0x0073[3:0], 0x0074[7:0], 0x0075[7:0])
	write_cmos_sensor_8(0x0073, ((shutter & 0x0F0000) >> 16));
	write_cmos_sensor(0x0074, shutter & 0x00FFFF);
	LOG_DBG("shutter =%d, framelength =%d",
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

	LOG_DBG(" - E");
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
	LOG_DBG("gain = %d , reg_gain = 0x%x\n ", gain, reg_gain);

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

static int set_mode_setfile(enum IMGSENSOR_MODE mode)
{
	int ret = -1;

	if (mode >= IMGSENSOR_MODE_MAX) {
		LOG_ERR("invalid mode: %d", mode);
		return -1;
	}
	LOG_INF(" - E");
	LOG_INF("mode: %s", sr846d_setfile_info[mode].name);

	if ((sr846d_setfile_info[mode].setfile == NULL) || (sr846d_setfile_info[mode].size == 0)) {
		LOG_ERR("failed, mode: %d", mode);
		ret = -1;
	} else
		ret = sr846d_table_write_cmos_sensor(sr846d_setfile_info[mode].setfile, sr846d_setfile_info[mode].size);

	LOG_INF(" - X");
	return ret;
}

static int sensor_init(void)
{
	int ret = ERROR_NONE;

	LOG_INF(" - E");
	ret = set_mode_setfile(IMGSENSOR_MODE_INIT);

	LOG_INF(" - X");

	return ret;
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
			LOG_INF("i2c write id : 0x%x, sensor id: 0x%x\n",	imgsensor.i2c_write_id, *sensor_id);
			if (*sensor_id == imgsensor_info.sensor_id) {
				return ERROR_NONE;
			}

			retry--;
		} while (retry > 0);
		i++;
		retry = 2;
	}
	if (*sensor_id != imgsensor_info.sensor_id) {
		LOG_ERR("Read sensor id fail, write id: 0x%x, id: 0x%x\n", imgsensor.i2c_write_id, *sensor_id);

		/*if Sensor ID is not correct, Must set *sensor_id to 0xFFFFFFFF*/
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
	kal_uint32 ret = ERROR_NONE;

	LOG_INF(" - E");

	while (imgsensor_info.i2c_addr_table[i] != 0xff) {
		spin_lock(&imgsensor_drv_lock);
		imgsensor.i2c_write_id = imgsensor_info.i2c_addr_table[i];
		spin_unlock(&imgsensor_drv_lock);
		do {
			sensor_id = return_sensor_id();
			LOG_INF("i2c write id: 0x%x, sensor id: 0x%x\n", imgsensor.i2c_write_id, sensor_id);
			if (sensor_id == imgsensor_info.sensor_id) {
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
		LOG_ERR("Read sensor id fail, write id: 0x%x, id: 0x%x\n", imgsensor.i2c_write_id, sensor_id);
		return ERROR_SENSOR_CONNECT_FAIL;
	}
	/* initail sequence write in  */
	ret = sensor_init();

	spin_lock(&imgsensor_drv_lock);
	imgsensor.autoflicker_en = KAL_FALSE;
	imgsensor.sensor_mode = IMGSENSOR_MODE_INIT;
	imgsensor.pclk = imgsensor_info.mode_info[IMGSENSOR_MODE_PREVIEW]->pclk;
	imgsensor.frame_length = imgsensor_info.mode_info[IMGSENSOR_MODE_PREVIEW]->framelength;
	imgsensor.line_length = imgsensor_info.mode_info[IMGSENSOR_MODE_PREVIEW]->linelength;
	imgsensor.min_frame_length = imgsensor_info.mode_info[IMGSENSOR_MODE_PREVIEW]->framelength;
	imgsensor.dummy_pixel = 0;
	imgsensor.dummy_line = 0;
	imgsensor.ihdr_en = 0;
	imgsensor.test_pattern = KAL_FALSE;
	imgsensor.current_fps = imgsensor_info.mode_info[IMGSENSOR_MODE_PREVIEW]->max_framerate;
	spin_unlock(&imgsensor_drv_lock);

	LOG_INF(" - X");
	return ret;
}	/*	open  */

static kal_uint32 close(void)
{
	LOG_INF(" - E");
	streaming_control(false);
	LOG_INF(" - X");
	return ERROR_NONE;
}	/*	close  */

static kal_uint32 get_resolution(
		MSDK_SENSOR_RESOLUTION_INFO_STRUCT *
			sensor_resolution)
{
	sensor_resolution->SensorFullWidth = imgsensor_info.mode_info[IMGSENSOR_MODE_CAPTURE]->grabwindow_width;
	sensor_resolution->SensorFullHeight = imgsensor_info.mode_info[IMGSENSOR_MODE_CAPTURE]->grabwindow_height;

	sensor_resolution->SensorPreviewWidth = imgsensor_info.mode_info[IMGSENSOR_MODE_PREVIEW]->grabwindow_width;
	sensor_resolution->SensorPreviewHeight = imgsensor_info.mode_info[IMGSENSOR_MODE_PREVIEW]->grabwindow_height;

	sensor_resolution->SensorVideoWidth = imgsensor_info.mode_info[IMGSENSOR_MODE_VIDEO]->grabwindow_width;
	sensor_resolution->SensorVideoHeight = imgsensor_info.mode_info[IMGSENSOR_MODE_VIDEO]->grabwindow_height;

	sensor_resolution->SensorHighSpeedVideoWidth = imgsensor_info.mode_info[IMGSENSOR_MODE_HIGH_SPEED_VIDEO]->grabwindow_width;
	sensor_resolution->SensorHighSpeedVideoHeight = imgsensor_info.mode_info[IMGSENSOR_MODE_HIGH_SPEED_VIDEO]->grabwindow_height;

	sensor_resolution->SensorSlimVideoWidth = imgsensor_info.mode_info[IMGSENSOR_MODE_SLIM_VIDEO]->grabwindow_width;
	sensor_resolution->SensorSlimVideoHeight = imgsensor_info.mode_info[IMGSENSOR_MODE_SLIM_VIDEO]->grabwindow_height;

	sensor_resolution->SensorCustom1Width = imgsensor_info.mode_info[IMGSENSOR_MODE_CUSTOM1]->grabwindow_width;
	sensor_resolution->SensorCustom1Height = imgsensor_info.mode_info[IMGSENSOR_MODE_CUSTOM1]->grabwindow_height;

	sensor_resolution->SensorCustom2Width = imgsensor_info.mode_info[IMGSENSOR_MODE_CUSTOM2]->grabwindow_width;
	sensor_resolution->SensorCustom2Height = imgsensor_info.mode_info[IMGSENSOR_MODE_CUSTOM2]->grabwindow_height;

	sensor_resolution->SensorCustom3Width = imgsensor_info.mode_info[IMGSENSOR_MODE_CUSTOM3]->grabwindow_width;
	sensor_resolution->SensorCustom3Height = imgsensor_info.mode_info[IMGSENSOR_MODE_CUSTOM3]->grabwindow_height;

	sensor_resolution->SensorCustom4Width = imgsensor_info.mode_info[IMGSENSOR_MODE_CUSTOM4]->grabwindow_width;
	sensor_resolution->SensorCustom4Height = imgsensor_info.mode_info[IMGSENSOR_MODE_CUSTOM4]->grabwindow_height;

	sensor_resolution->SensorCustom5Width = imgsensor_info.mode_info[IMGSENSOR_MODE_CUSTOM5]->grabwindow_width;
	sensor_resolution->SensorCustom5Height = imgsensor_info.mode_info[IMGSENSOR_MODE_CUSTOM5]->grabwindow_height;

	return ERROR_NONE;
}    /*    get_resolution    */


static kal_uint32 get_info(enum MSDK_SCENARIO_ID_ENUM scenario_id,
			MSDK_SENSOR_INFO_STRUCT *sensor_info,
			MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	enum IMGSENSOR_MODE sensor_mode = IMGSENSOR_MODE_PREVIEW;
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
	sensor_info->SensorOutputDataFormat = imgsensor_info.sensor_output_dataformat;

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
	sensor_info->SensorDrivingCurrent = imgsensor_info.isp_driving_current;
/* The frame of setting shutter default 0 for TG int */
	sensor_info->AEShutDelayFrame = imgsensor_info.ae_shut_delay_frame;
/* The frame of setting sensor gain */
	sensor_info->AESensorGainDelayFrame = imgsensor_info.ae_sensor_gain_delay_frame;
	sensor_info->AEISPGainDelayFrame = imgsensor_info.ae_ispGain_delay_frame;
	sensor_info->IHDR_Support = imgsensor_info.ihdr_support;
	sensor_info->IHDR_LE_FirstLine = imgsensor_info.ihdr_le_firstline;
	sensor_info->SensorModeNum = imgsensor_info.sensor_mode_num;

	sensor_info->SensorMIPILaneNumber = imgsensor_info.mipi_lane_num;
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

	sensor_mode = get_imgsensor_mode(scenario_id);
	sensor_info->SensorGrabStartX = imgsensor_info.mode_info[sensor_mode]->startx;
	sensor_info->SensorGrabStartY = imgsensor_info.mode_info[sensor_mode]->starty;
	sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
			imgsensor_info.mode_info[sensor_mode]->mipi_data_lp2hs_settle_dc;

	return ERROR_NONE;
}    /*    get_info  */

static void set_imgsensor_mode_info(enum IMGSENSOR_MODE sensor_mode)
{
	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode		= sensor_mode;
	imgsensor.pclk				= imgsensor_info.mode_info[sensor_mode]->pclk;
	imgsensor.line_length		= imgsensor_info.mode_info[sensor_mode]->linelength;
	imgsensor.frame_length		= imgsensor_info.mode_info[sensor_mode]->framelength;
	imgsensor.min_frame_length	= imgsensor_info.mode_info[sensor_mode]->framelength;
	imgsensor.autoflicker_en	= KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
}

static kal_uint32 control(enum MSDK_SCENARIO_ID_ENUM scenario_id,
			MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	enum IMGSENSOR_MODE sensor_mode = IMGSENSOR_MODE_PREVIEW;

	LOG_INF(" - E");
	LOG_INF("scenario_id = %d", scenario_id);
	spin_lock(&imgsensor_drv_lock);
	imgsensor.current_scenario_id = scenario_id;
	spin_unlock(&imgsensor_drv_lock);

	sensor_mode = get_imgsensor_mode(scenario_id);
	set_imgsensor_mode_info(sensor_mode);
	set_mode_setfile(sensor_mode);
	LOG_INF(" - X");

	if (imgsensor_info.is_invalid_mode) {
		LOG_ERR("selected invalid mode");
		return ERROR_INVALID_SCENARIO_ID;
	}
	return ERROR_NONE;
}	/* control() */

static kal_uint32 set_video_mode(UINT16 framerate)
{
	LOG_INF("framerate = %d ", framerate);
	// SetVideoMode Function should fix framerate
	if (framerate == 0) // Dynamic frame rate
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
	LOG_DBG("enable = %d, framerate = %d ", enable, framerate);
	spin_lock(&imgsensor_drv_lock);
	if (enable)
		imgsensor.autoflicker_en = KAL_TRUE;
	else //Cancel Auto flick
		imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	return ERROR_NONE;
}

static kal_uint32 set_max_framerate_by_scenario(enum MSDK_SCENARIO_ID_ENUM scenario_id, MUINT32 framerate)
{
	kal_uint32 frame_length;
	enum IMGSENSOR_MODE sensor_mode = IMGSENSOR_MODE_PREVIEW;

	LOG_DBG("scenario_id = %d, framerate = %d\n", scenario_id, framerate);
	sensor_mode = get_imgsensor_mode(scenario_id);

	frame_length = imgsensor_info.mode_info[sensor_mode]->pclk /
			framerate * 10 / imgsensor_info.mode_info[sensor_mode]->linelength;

	spin_lock(&imgsensor_drv_lock);
	imgsensor.dummy_line = (frame_length > imgsensor_info.mode_info[sensor_mode]->framelength) ?
			(frame_length - imgsensor_info.mode_info[sensor_mode]->framelength) : 0;

	imgsensor.frame_length = imgsensor_info.mode_info[sensor_mode]->framelength + imgsensor.dummy_line;
	imgsensor.min_frame_length = imgsensor.frame_length;
	spin_unlock(&imgsensor_drv_lock);

	if (imgsensor.frame_length > imgsensor.shutter)
		set_dummy();

	return ERROR_NONE;
}

static kal_uint32 get_default_framerate_by_scenario(enum MSDK_SCENARIO_ID_ENUM scenario_id, MUINT32 *framerate)
{
	enum IMGSENSOR_MODE sensor_mode = IMGSENSOR_MODE_PREVIEW;
	LOG_INF("scenario_id = %d\n", scenario_id);

	sensor_mode = get_imgsensor_mode(scenario_id);

	if (!imgsensor_info.is_invalid_mode)
		*framerate = imgsensor_info.mode_info[sensor_mode]->max_framerate;

	return ERROR_NONE;
}

static kal_uint32 set_test_pattern_mode(kal_bool enable)
{
#if 0
	if (enable) {
// 0x0A05[0]: 1 enable,  0 disable
// 0x020A[3:0]; 0 No Pattern, 1 Solid Color, 2 100% Color Bar
		write_cmos_sensor(0x0a04, 0x0143);
		write_cmos_sensor_8(0x020A, 0x0002);
	} else {
// 0x0A05[0]: 1 enable,  0 disable
		write_cmos_sensor(0x0a04, 0x0142);
		write_cmos_sensor_8(0x020A, 0x0000);
	}
	spin_lock(&imgsensor_drv_lock);
	imgsensor.test_pattern = enable;
	spin_unlock(&imgsensor_drv_lock);

	LOG_INF("test_pattern: %d", imgsensor.test_pattern);
#else
	//test pattern is always disable
	spin_lock(&imgsensor_drv_lock);
	imgsensor.test_pattern = false;
	spin_unlock(&imgsensor_drv_lock);
	LOG_INF("test pattern not supported");
#endif
	return ERROR_NONE;
}

static kal_uint32 streaming_control(kal_bool enable)
{
	LOG_INF("streaming_enable(0=Sw Standby,1=streaming): %d\n", enable);

	if (enable) {
		write_cmos_sensor(0x0A20, 0x0100);
		write_cmos_sensor(0x004C, 0x0100);
		usleep_range(10000, 10100);
	} else {
		write_cmos_sensor(0x0F10, 0x0A00);
		write_cmos_sensor(0x0A20, 0x0000);
		wait_stream_off();
	}

	return ERROR_NONE;
}

static kal_uint32 feature_control(MSDK_SENSOR_FEATURE_ENUM feature_id,
			UINT8 *feature_para, UINT32 *feature_para_len)
{
	UINT16 *feature_return_para_16 = (UINT16 *) feature_para;
	UINT16 *feature_data_16 = (UINT16 *) feature_para;
	UINT32 *feature_return_para_32 = (UINT32 *) feature_para;
	UINT32 *feature_data_32 = (UINT32 *) feature_para;
	INT32 *feature_return_para_i32 = (INT32 *) feature_para;
	unsigned long long *feature_data =
		(unsigned long long *) feature_para;

	enum IMGSENSOR_MODE sensor_mode = IMGSENSOR_MODE_PREVIEW;

	struct SENSOR_WINSIZE_INFO_STRUCT *wininfo;
	MSDK_SENSOR_REG_INFO_STRUCT *sensor_reg_data = (MSDK_SENSOR_REG_INFO_STRUCT *) feature_para;
	LOG_DBG("feature_id = %d\n", feature_id);

	switch (feature_id) {
	case SENSOR_FEATURE_GET_PIXEL_CLOCK_FREQ_BY_SCENARIO:
		sensor_mode = get_imgsensor_mode((enum MSDK_SCENARIO_ID_ENUM)(*feature_data));
		*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) = imgsensor_info.mode_info[sensor_mode]->pclk;
		break;
	case SENSOR_FEATURE_GET_PERIOD_BY_SCENARIO:
		sensor_mode = get_imgsensor_mode((enum MSDK_SCENARIO_ID_ENUM)(*feature_data));
		*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
			(imgsensor_info.mode_info[sensor_mode]->framelength << 16) +
			imgsensor_info.mode_info[sensor_mode]->linelength;
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
		sensor_mode = get_imgsensor_mode((enum MSDK_SCENARIO_ID_ENUM)(*feature_data));
		*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) = imgsensor_info.mode_info[sensor_mode]->mipi_pixel_rate;
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
		write_cmos_sensor(sensor_reg_data->RegAddr, sensor_reg_data->RegData);
		break;
	case SENSOR_FEATURE_GET_REGISTER:
		sensor_reg_data->RegData = read_cmos_sensor(sensor_reg_data->RegAddr);
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
		set_auto_flicker_mode((BOOL)*feature_data_16, *(feature_data_16+1));
		break;
	case SENSOR_FEATURE_SET_MAX_FRAME_RATE_BY_SCENARIO:
		set_max_framerate_by_scenario(
			(enum MSDK_SCENARIO_ID_ENUM)*feature_data,
			*(feature_data+1));
		break;
	case SENSOR_FEATURE_GET_DEFAULT_FRAME_RATE_BY_SCENARIO:
		get_default_framerate_by_scenario(
			(enum MSDK_SCENARIO_ID_ENUM)*(feature_data), (MUINT32 *)(uintptr_t)(*(feature_data+1)));
		break;
	case SENSOR_FEATURE_SET_TEST_PATTERN:
		set_test_pattern_mode((BOOL)*feature_data);
		break;
	case SENSOR_FEATURE_GET_TEST_PATTERN_CHECKSUM_VALUE:
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
		LOG_INF("ihdr enable :%d\n", *feature_data_32);
		spin_lock(&imgsensor_drv_lock);
		imgsensor.ihdr_en = (BOOL)*feature_data_32;
		spin_unlock(&imgsensor_drv_lock);
		break;
	case SENSOR_FEATURE_GET_CROP_INFO:
		LOG_INF("SENSOR_FEATURE_GET_CROP_INFO scenarioId:%d\n", (UINT32)*feature_data);
		wininfo = (struct SENSOR_WINSIZE_INFO_STRUCT *)(uintptr_t)(*(feature_data+1));

		sensor_mode = get_imgsensor_mode((enum MSDK_SCENARIO_ID_ENUM)(*feature_data_32));
		memcpy((void *)wininfo, (void *)&imgsensor_winsize_info[sensor_mode], sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
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
	case SENSOR_FEATURE_GET_FRAME_CTRL_INFO_BY_SCENARIO:
		/*
		 * 1, if driver support new sw frame sync
		 * set_shutter_frame_length() support third para auto_extend_en
		 */
		*(feature_data + 1) = 1; /* margin info by scenario */
		*(feature_data + 2) = imgsensor_info.margin;
		break;
	case SENSOR_FEATURE_SET_SHUTTER_FRAME_TIME:
		set_shutter_frame_length((UINT32) (*feature_data), (UINT32) (*(feature_data + 1)));
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
	int ret = -1;

	LOG_INF(" - E");

	if (data == NULL) {
		LOG_ERR("fail, data is NULL");
		return -1;
	}

	// OTP Read only initial setting
	LOG_INF("OTP Read only initial setting");
	sr846d_table_write_cmos_sensor(otp_readonly_init_sr846d,
			sizeof(otp_readonly_init_sr846d) / sizeof(kal_uint16));

	// OTP mode select register control
	write_cmos_sensor_8(0x071A, 0x01); // CP TRIM_H
	write_cmos_sensor_8(0x071B, 0x09); // IPGM TRIM_H
	write_cmos_sensor_8(0x0D04, 0x00); // OTP busy output enable
	write_cmos_sensor_8(0x0D00, 0x07); // OTP busy output drivability
	write_cmos_sensor_8(0x003E, 0x10); // OTP mode enable
	write_cmos_sensor_8(0x004C, 0x01); // TG enable
	write_cmos_sensor_8(0x0A00, 0x01); // stream on
	usleep_range(10000, 10100);        // sleep 10msec

	// Select OTP Bank
	otp_bank = sr846d_otp_read_byte(SR846D_OTP_CHECK_BANK);
	LOG_INF("check OTP bank:0x%x, size: %d\n", otp_bank, size);

	switch (otp_bank) {
	case SR846D_OTP_BANK1_MARK:
		read_addr = SR846D_OTP_BANK1_START_ADDR;
		break;
	case SR846D_OTP_BANK2_MARK:
		read_addr = SR846D_OTP_BANK2_START_ADDR;
		break;
	default:
		LOG_ERR("check bank: fail");
		break;
	}

	if (read_addr) {
		for (i = 0; i < size; i++) {
			*(data + i) = sr846d_otp_read_byte(read_addr);
			LOG_DBG("read data addr:0x%x, value:0x%x, i:%d\n", read_addr, *(data + i), i);
			read_addr += 1;
		}
		ret = (int)size;
	}

	// OTP mode off
	write_cmos_sensor_8(0x0F10, 0x0A); // stream off
	write_cmos_sensor_8(0x004C, 0x00); // TG disable
	write_cmos_sensor_8(0x0A00, 0x00); // stream off
	usleep_range(10000, 10100);        // sleep 10msec

	write_cmos_sensor_8(0x0702, 0x00); // OTP r/w disable
	write_cmos_sensor_8(0x003E, 0x00); // OTP mode disable
	usleep_range(1000, 1100);          // sleep 1msec

	// SW reset
	write_cmos_sensor_8(0x0F00, 0x01);
	usleep_range(1000, 1100);
	write_cmos_sensor_8(0x0F00, 0x00);
	sensor_init();

	LOG_INF(" - X");
	return ret;
}

UINT32 SR846D_MIPI_RAW_SensorInit(struct SENSOR_FUNCTION_STRUCT **pfFunc)
{
	LOG_INF(" - E");
	/* To Do : Check Sensor status here */
	if (pfFunc != NULL)
		*pfFunc =  &sensor_func;

	LOG_INF(" - X");
	return ERROR_NONE;
}	/*	SR846D_MIPI_RAW_SensorInit	*/
