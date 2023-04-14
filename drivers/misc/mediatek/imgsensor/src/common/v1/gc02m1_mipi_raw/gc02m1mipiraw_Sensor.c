// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
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

#include "kd_camera_typedef.h"

#include "kd_imgsensor.h"
#include "kd_imgsensor_define.h"
#include "kd_imgsensor_errcode.h"
#include "kd_imgsensor_sysfs_adapter.h"

#include "gc02m1mipiraw_Sensor.h"
#include "gc02m1mipiraw_setfile.h"

/****************************Modify Following Strings for Debug***************/
#define PFX "GC02M1 D/D"
/****************************   Modify end    ********************/

#define LOG_DBG(format, args...) pr_debug(PFX "[%s] " format, __func__, ##args)
#define LOG_INF(format, args...) pr_info(PFX "[%s] " format, __func__, ##args)
#define LOG_ERR(format, args...) pr_err(PFX "[%s] " format, __func__, ##args)

static kal_uint32 streaming_control(kal_bool enable);

static DEFINE_SPINLOCK(imgsensor_drv_lock);
#define MULTI_WRITE 1

#if MULTI_WRITE
static const int I2C_BUFFER_LEN = 1020;
#else
static const int I2C_BUFFER_LEN = 4;
#endif

/*
 * Image Sensor Scenario
 * pre : 4:3
 * cap : 4:3
 * normal_video : 16:9
 * hs_video: NOT USED
 * slim_video: NOT USED
 * custom1 : FAST AE
 */

static struct imgsensor_info_struct imgsensor_info = {
	/*.sensor_id = GC02M1_SENSOR_ID, */
	.sensor_id = GC02M1_SENSOR_ID,

	.checksum_value = 0xf7375923,	//checksum value for Camera Auto Test

	.pre = {
		.pclk = 84500000,	//record different mode's pclk
		.linelength = 2192,	//record different mode's linelength
		.framelength = 1268,	//record different mode's framelength
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 1600,
		.grabwindow_height = 1200,
		.mipi_data_lp2hs_settle_dc = 85, //unit , ns
		/*     following for GetDefaultFramerateByScenario()    */
		.mipi_pixel_rate = 67600000,
		.max_framerate = 300,
	},
	.cap = {
		.pclk = 84500000,	//record different mode's pclk
		.linelength = 2192,	//record different mode's linelength
		.framelength = 1268,	//record different mode's framelength
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 1600,
		.grabwindow_height = 1200,
		.mipi_data_lp2hs_settle_dc = 85, //unit , ns
		/*     following for GetDefaultFramerateByScenario()    */
		.mipi_pixel_rate = 67600000,
		.max_framerate = 300,
	},
	.normal_video = {
		.pclk = 84500000,
		.linelength = 2192,
		.framelength = 1268,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 1600,
		.grabwindow_height = 900,
		.mipi_data_lp2hs_settle_dc = 85, //unit , ns
		.mipi_pixel_rate = 67600000,
		.max_framerate = 300,
	 },
	.hs_video = { // NOT USED
		.pclk = 84500000,
		.linelength = 2192,
		.framelength = 1268,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 1600,
		.grabwindow_height = 1200,
		.mipi_data_lp2hs_settle_dc = 85, //unit , ns
		.mipi_pixel_rate = 67600000,
		.max_framerate = 300,
	},
	.slim_video = { // NOT USED
		.pclk = 84500000,
		.linelength = 2192,
		.framelength = 1268,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 1600,
		.grabwindow_height = 1200,
		.mipi_data_lp2hs_settle_dc = 85, //unit , ns
		.mipi_pixel_rate = 67600000,
		.max_framerate = 300,
	},
	.custom1 = {
		.pclk = 84500000,	//record different mode's pclk
		.linelength = 2192,
		.framelength = 639,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 800,
		.grabwindow_height = 600,
		.mipi_data_lp2hs_settle_dc = 85,	//unit , ns
		/*     following for GetDefaultFramerateByScenario()    */
		.mipi_pixel_rate = 67600000,
		.max_framerate = 600,
	},
	.custom2 = { // NOT USED
		.pclk = 84500000,
		.linelength = 2192,
		.framelength = 1268,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 1600,
		.grabwindow_height = 1200,
		.mipi_data_lp2hs_settle_dc = 85, //unit , ns
		.mipi_pixel_rate = 67600000,
		.max_framerate = 300,
	},
	.custom3 = { // NOT USED
		.pclk = 84500000,
		.linelength = 2192,
		.framelength = 1268,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 1600,
		.grabwindow_height = 1200,
		.mipi_data_lp2hs_settle_dc = 85, //unit , ns
		.mipi_pixel_rate = 67600000,
		.max_framerate = 300,
	},
	.custom4 = { // NOT USED
		.pclk = 84500000,
		.linelength = 2192,
		.framelength = 1268,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 1600,
		.grabwindow_height = 1200,
		.mipi_data_lp2hs_settle_dc = 85, //unit , ns
		.mipi_pixel_rate = 67600000,
		.max_framerate = 300,
	},
	.custom5 = { // NOT USED
		.pclk = 84500000,
		.linelength = 2192,
		.framelength = 1268,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 1600,
		.grabwindow_height = 1200,
		.mipi_data_lp2hs_settle_dc = 85, //unit , ns
		.mipi_pixel_rate = 67600000,
		.max_framerate = 300,
	},
	.margin = 16,                    //sensor framelength & shutter margin
	.min_shutter = 4,                //min shutter
	.min_gain = 64,                  /*1x gain*/
	.max_gain = 640,                 /*16x gain*/
	.min_gain_iso = 100,
	.gain_step = 1,
	.gain_type = 4,                  /*to be modify,no gain table for gc*/
	.exp_step = 1,
	.max_frame_length = 0x3fff,
	.ae_shut_delay_frame = 0,
	.temperature_support = 0,        /* 1, support; 0,not support */
	.ae_sensor_gain_delay_frame = 0,
	.ae_ispGain_delay_frame = 2,     //isp gain delay frame for AE cycle
	.ihdr_support = 0,               // 1 support; 0 not support
	.ihdr_le_firstline = 0,          // 1 le first ; 0, se first
	.sensor_mode_num = 10,           //support sensor mode num

	.cap_delay_frame = 2,            //enter capture delay frame num
	.pre_delay_frame = 2,            //enter preview delay frame num
	.video_delay_frame = 2,          //enter video delay frame num
	.hs_video_delay_frame = 2,
	.slim_video_delay_frame = 2,     //enter slim video delay frame num
	.custom1_delay_frame = 2,
	.custom2_delay_frame = 2,
	.custom3_delay_frame = 2,
	.custom4_delay_frame = 2,
	.custom5_delay_frame = 2,
	.temperature_support = 0,        /* 1, support; 0,not support */

	.isp_driving_current = ISP_DRIVING_6MA, //mclk driving current

	.sensor_interface_type = SENSOR_INTERFACE_TYPE_MIPI,
	.mipi_sensor_type = MIPI_OPHY_NCSI2,
	.mipi_settle_delay_mode = MIPI_SETTLEDELAY_AUTO,
	.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_R,
	.mclk = 26,
	.mipi_lane_num = SENSOR_MIPI_1_LANE, //mipi lane num
	.i2c_addr_table = {0x20, 0x6e, 0xff},
	.i2c_speed = 400,
};


static struct imgsensor_struct imgsensor = {
	.mirror = IMAGE_NORMAL,	//mirrorflip information
	.sensor_mode = IMGSENSOR_MODE_INIT,
	.shutter = 0x3ED,	//current shutter
	.gain = 0x40,		//current gain
	.dummy_pixel = 0,	//current dummypixel
	.dummy_line = 0,	//current dummyline
	.current_fps = 300,
	.autoflicker_en = KAL_FALSE,
	.test_pattern = KAL_FALSE,
	.current_scenario_id = MSDK_SCENARIO_ID_CAMERA_PREVIEW,
	.ihdr_en = 0,		//sensor need support LE, SE with HDR feature
	.i2c_write_id = 0x20,	//record current sensor's i2c write id
};


/* Sensor output window information */
static struct SENSOR_WINSIZE_INFO_STRUCT imgsensor_winsize_info[] = {
	{ 1612, 1214,    0,    2, 1612, 1212, 1612, 1212,
	     6,    6, 1600, 1200,    0,    0, 1600, 1200 }, // Preview
	{ 1612, 1214,    0,    2, 1612, 1212, 1612, 1212,
	     6,    6, 1600, 1200,    0,    0, 1600, 1200 }, // Capture
	{ 1612, 1214,    0,    2, 1612, 1212, 1612, 1212,
	     6,  156, 1600,  900,    0,    0, 1600,  900 }, // Video
	{ 1612, 1214,    0,    2, 1612, 1212, 1612, 1212,
	     6,    6, 1600, 1200,    0,    0, 1600, 1200 }, // high speed video -NOT USED
	{ 1612, 1214,    0,    2, 1612, 1212, 1612, 1212,
	     6,    6, 1600, 1200,    0,    0, 1600, 1200 }, // slim video -NOT USED
	{ 1612, 1214,    0,    2, 1612, 1212,  806,  606,
	     4,    4,  800,  600,    0,    0,  800,  600 }, // Custom1
	{ 1612, 1214,    0,    2, 1612, 1212, 1612, 1212,
	     6,    6, 1600, 1200,    0,    0, 1600, 1200 }, // Custom2 -NOT USED
	{ 1612, 1214,    0,    2, 1612, 1212, 1612, 1212,
	     6,    6, 1600, 1200,    0,    0, 1600, 1200 }, // Custom3 -NOT USED
	{ 1612, 1214,    0,    2, 1612, 1212, 1612, 1212,
	     6,    6, 1600, 1200,    0,    0, 1600, 1200 }, // Custom4 -NOT USED
	{ 1612, 1214,    0,    2, 1612, 1212, 1612, 1212,
	     6,    6, 1600, 1200,    0,    0, 1600, 1200 }, // Custom5 -NOT USED
};

static kal_uint16 read_cmos_sensor(kal_uint32 addr)
{
	kal_uint16 get_byte = 0;
	char pu_send_cmd[1] = { (char)(addr & 0xff) };

	iReadRegI2C(pu_send_cmd, 1, (u8 *) &get_byte, 1,
		    imgsensor.i2c_write_id);

	return get_byte;

}


static int write_cmos_sensor(kal_uint32 addr, kal_uint32 para)
{
	char pu_send_cmd[2] = { (char)(addr & 0xff), (char)(para & 0xff) };

	return iWriteRegI2C(pu_send_cmd, 2, imgsensor.i2c_write_id);
}

static void set_dummy(void)
{
	write_cmos_sensor(0xfe, 0x00);
	write_cmos_sensor(0x41, (imgsensor.frame_length >> 8) & 0xff);
	write_cmos_sensor(0x42, imgsensor.frame_length & 0xff);
}

static kal_uint32 return_sensor_id(void)
{
	return ((read_cmos_sensor(0xf0) << 8) | read_cmos_sensor(0xf1));
}

static void set_max_framerate(UINT16 framerate, kal_bool min_framelength_en)
{
	/* kal_int16 dummy_line; */
	kal_uint32 frame_length = imgsensor.frame_length;
	/* unsigned long flags; */

	frame_length = imgsensor.pclk / framerate * 10 / imgsensor.line_length;

	spin_lock(&imgsensor_drv_lock);
	imgsensor.frame_length = (frame_length > imgsensor.min_frame_length)
	    ? frame_length : imgsensor.min_frame_length;
	imgsensor.dummy_line =
	    imgsensor.frame_length - imgsensor.min_frame_length;
	if (imgsensor.frame_length > imgsensor_info.max_frame_length) {
		imgsensor.frame_length = imgsensor_info.max_frame_length;
		imgsensor.dummy_line = imgsensor.frame_length - imgsensor.min_frame_length;
	}
	if (min_framelength_en)
		imgsensor.min_frame_length = imgsensor.frame_length;
	spin_unlock(&imgsensor_drv_lock);
	set_dummy();
}


/*************************************************************************
 * FUNCTION
 *    set_shutter
 *
 * DESCRIPTION
 *    This function set e-shutter of sensor to change exposure time.
 *
 * PARAMETERS
 *    iShutter : exposured lines
 *
 * RETURNS
 *    None
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/
static void set_shutter(kal_uint16 shutter)
{
	unsigned long flags;
	kal_uint16 realtime_fps = 0;
	/* kal_uint32 frame_length = 0; */
	spin_lock_irqsave(&imgsensor_drv_lock, flags);
	imgsensor.shutter = shutter;
	spin_unlock_irqrestore(&imgsensor_drv_lock, flags);

/* if shutter bigger than frame_length, should extend frame length first */
	spin_lock(&imgsensor_drv_lock);
	if (shutter > imgsensor.min_frame_length - imgsensor_info.margin)
		imgsensor.frame_length = shutter + imgsensor_info.margin;
	else
		imgsensor.frame_length = imgsensor.min_frame_length;
	if (imgsensor.frame_length > imgsensor_info.max_frame_length)
		imgsensor.frame_length = imgsensor_info.max_frame_length;
	spin_unlock(&imgsensor_drv_lock);
	shutter = (shutter < imgsensor_info.min_shutter) ? imgsensor_info.min_shutter : shutter;
	shutter = (shutter > (imgsensor_info.max_frame_length - imgsensor_info.margin)) ?
				(imgsensor_info.max_frame_length - imgsensor_info.margin) : shutter;

	realtime_fps = imgsensor.pclk / imgsensor.line_length * 10 / imgsensor.frame_length;

	if (imgsensor.autoflicker_en) {
		if (realtime_fps >= 297 && realtime_fps <= 305)
			set_max_framerate(296, 0);
		else if (realtime_fps >= 147 && realtime_fps <= 150)
			set_max_framerate(146, 0);
		else
			set_max_framerate(realtime_fps, 0);
	} else
		set_max_framerate(realtime_fps, 0);

	write_cmos_sensor(0xfe, 0x00);
	write_cmos_sensor(0x03, (shutter >> 8) & 0x3f);
	write_cmos_sensor(0x04, shutter & 0xff);
	LOG_DBG("shutter = %d, framelength = %d\n", shutter, imgsensor.frame_length);
}

static void set_shutter_frame_length(kal_uint16 shutter, kal_uint16 frame_length)
{
	unsigned long flags;
	kal_uint16 realtime_fps = 0;
	kal_int32 dummy_line = 0;

	/* kal_uint32 frame_length = 0; */
	spin_lock_irqsave(&imgsensor_drv_lock, flags);
	imgsensor.shutter = shutter;
	spin_unlock_irqrestore(&imgsensor_drv_lock, flags);

	// if shutter bigger than frame_length, should extend frame length first
	spin_lock(&imgsensor_drv_lock);

	dummy_line = frame_length - imgsensor.frame_length;
	imgsensor.frame_length = imgsensor.frame_length + dummy_line;

	if (shutter > imgsensor.frame_length - imgsensor_info.margin)
		imgsensor.frame_length = shutter + imgsensor_info.margin;


	if (imgsensor.frame_length > imgsensor_info.max_frame_length)
		imgsensor.frame_length = imgsensor_info.max_frame_length;
	spin_unlock(&imgsensor_drv_lock);
	shutter = (shutter < imgsensor_info.min_shutter) ? imgsensor_info.min_shutter : shutter;
	shutter = (shutter > (imgsensor_info.max_frame_length - imgsensor_info.margin)) ?
				(imgsensor_info.max_frame_length - imgsensor_info.margin) : shutter;

	realtime_fps = imgsensor.pclk / imgsensor.line_length * 10 / imgsensor.frame_length;

	if (imgsensor.autoflicker_en) {
		if (realtime_fps >= 297 && realtime_fps <= 305)
			set_max_framerate(296, 0);
		else if (realtime_fps >= 147 && realtime_fps <= 150)
			set_max_framerate(146, 0);
		else
			set_max_framerate(realtime_fps, 0);
	} else
		set_max_framerate(realtime_fps, 0);

	write_cmos_sensor(0xfe, 0x00);
	write_cmos_sensor(0x03, (shutter >> 8) & 0x3f);
	write_cmos_sensor(0x04, shutter  & 0xff);
	LOG_DBG("shutter = %d, framelength = %d Stereo\n", shutter, imgsensor.frame_length);
}

/*************************************************************************
 * FUNCTION
 *    set_gain
 *
 * DESCRIPTION
 *    This function is to set global gain to sensor.
 *
 * PARAMETERS
 *    iGain : sensor global gain(base: 0x40)
 *
 * RETURNS
 *    the actually gain set to sensor.
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/

#define ANALOG_GAIN_1 1024	// 1.00x
#define ANALOG_GAIN_2 1536	// 1.5x
#define ANALOG_GAIN_3 2035	// 1.98x
#define ANALOG_GAIN_4 2519	// 2.460x
#define ANALOG_GAIN_5 3165	// 3.09x
#define ANALOG_GAIN_6 3626	// 3.541
#define ANALOG_GAIN_7 4148	// 4.05x
#define ANALOG_GAIN_8 4593	// 4.485x
#define ANALOG_GAIN_9 5095	// 4.9759
#define ANALOG_GAIN_10 5696	// 5.563x
#define ANALOG_GAIN_11 6270	// 6.123x
#define ANALOG_GAIN_12 6714	// 6.556x
#define ANALOG_GAIN_13 7210	// 7.041x
#define ANALOG_GAIN_14 7686	// 7.506xx
#define ANALOG_GAIN_15 8214	// 8.022x
#define ANALOG_GAIN_16 10337	// 10.095x
#define ANALOG_GAIN_17 16199	// 15.819x

static kal_uint16 set_gain(kal_uint16 gain)
{
	kal_uint16 iReg, temp;

	if (gain < imgsensor_info.min_gain || gain > imgsensor_info.max_gain) {
		LOG_ERR("Error gain setting");

		if (gain < imgsensor_info.min_gain)
			gain = imgsensor_info.min_gain;
		else if (gain > imgsensor_info.max_gain)
			gain = imgsensor_info.max_gain;
	}

	LOG_DBG("celery--gain = %d\n", gain);
	iReg = gain << 4;

	if (iReg < 0x400)
		iReg = 0x400;

	if ((iReg >= ANALOG_GAIN_1) && (iReg < ANALOG_GAIN_2)) {
		write_cmos_sensor(0xfe, 0x00);
		write_cmos_sensor(0xb6, 0x00);
		temp = 1024 * iReg / ANALOG_GAIN_1;
		write_cmos_sensor(0xb1, (temp >> 8) & 0x3f);
		write_cmos_sensor(0xb2, (temp & 0xff));
		LOG_DBG("GC02M1MIPI analogic gain 1x, GC02M1MIPI add pregain = %d\n", temp);
	} else if ((iReg >= ANALOG_GAIN_2) && (iReg < ANALOG_GAIN_3)) {
		write_cmos_sensor(0xfe, 0x00);
		write_cmos_sensor(0xb6, 0x01);
		temp = 1024 * iReg / ANALOG_GAIN_2;
		write_cmos_sensor(0xb1, (temp >> 8) & 0x3f);
		write_cmos_sensor(0xb2, (temp & 0xff));
		LOG_DBG("GC02M1MIPI analogic gain 1.185x, GC02M1MIPI add pregain = %d\n", temp);
	} else if ((iReg >= ANALOG_GAIN_3) && (iReg < ANALOG_GAIN_4)) {
		write_cmos_sensor(0xfe, 0x00);
		write_cmos_sensor(0xb6, 0x02);
		temp = 1024 * iReg / ANALOG_GAIN_3;
		write_cmos_sensor(0xb1, (temp >> 8) & 0x3f);
		write_cmos_sensor(0xb2, (temp & 0xff));
		LOG_DBG("GC02M1MIPI analogic gain 1.4x, GC02M1MIPI add pregain = %d\n", temp);
	} else if ((iReg >= ANALOG_GAIN_4) && (iReg < ANALOG_GAIN_5)) {
		write_cmos_sensor(0xfe, 0x00);
		write_cmos_sensor(0xb6, 0x03);
		temp = 1024 * iReg / ANALOG_GAIN_4;
		write_cmos_sensor(0xb1, (temp >> 8) & 0x3f);
		write_cmos_sensor(0xb2, (temp & 0xff));
		LOG_DBG("GC02M1MIPI analogic gain 1.659x, GC02M1MIPI add pregain = %d\n", temp);
	} else if ((iReg >= ANALOG_GAIN_5) && (iReg < ANALOG_GAIN_6)) {
		write_cmos_sensor(0xfe, 0x00);
		write_cmos_sensor(0xb6, 0x04);
		temp = 1024 * iReg / ANALOG_GAIN_5;
		write_cmos_sensor(0xb1, (temp >> 8) & 0x3f);
		write_cmos_sensor(0xb2, (temp & 0xff));
		LOG_DBG("GC02M1MIPI analogic gain 2.0x, GC02M1MIPI add pregain = %d\n", temp);
	} else if ((iReg >= ANALOG_GAIN_6) && (iReg < ANALOG_GAIN_7)) {
		write_cmos_sensor(0xfe, 0x00);
		write_cmos_sensor(0xb6, 0x05);
		temp = 1024 * iReg / ANALOG_GAIN_6;
		write_cmos_sensor(0xb1, (temp >> 8) & 0x3f);
		write_cmos_sensor(0xb2, (temp & 0xff));
		LOG_DBG("GC02M1MIPI analogic gain 2.37x, GC02M1MIPI add pregain = %d\n", temp);
	} else if ((iReg >= ANALOG_GAIN_7) && (iReg < ANALOG_GAIN_8)) {
		write_cmos_sensor(0xfe, 0x00);
		write_cmos_sensor(0xb6, 0x06);
		temp = 1024 * iReg / ANALOG_GAIN_7;
		write_cmos_sensor(0xb1, (temp >> 8) & 0x3f);
		write_cmos_sensor(0xb2, (temp & 0xff));
		LOG_DBG("GC02M1MIPI analogic gain 2.8x, GC02M1MIPI add pregain = %d\n", temp);
	} else if ((iReg >= ANALOG_GAIN_8) && (iReg < ANALOG_GAIN_9)) {
		write_cmos_sensor(0xfe, 0x00);
		write_cmos_sensor(0xb6, 0x07);
		temp = 1024 * iReg / ANALOG_GAIN_8;
		write_cmos_sensor(0xb1, (temp >> 8) & 0x3f);
		write_cmos_sensor(0xb2, (temp & 0xff));
		LOG_DBG("GC02M1MIPI analogic gain 3.318x, GC02M1MIPI add pregain = %d\n", temp);
	} else if ((iReg >= ANALOG_GAIN_9) && (iReg < ANALOG_GAIN_10)) {
		write_cmos_sensor(0xfe, 0x00);
		write_cmos_sensor(0xb6, 0x08);
		temp = 1024 * iReg / ANALOG_GAIN_9;
		write_cmos_sensor(0xb1, (temp >> 8) & 0x3f);
		write_cmos_sensor(0xb2, (temp & 0xff));
		LOG_DBG("GC02M1MIPI analogic gain 4.0x, GC02M1MIPI add pregain = %d\n", temp);
	} else if ((iReg >= ANALOG_GAIN_10) && (iReg < ANALOG_GAIN_11)) {
		write_cmos_sensor(0xfe, 0x00);
		write_cmos_sensor(0xb6, 0x09);
		temp = 1024 * iReg / ANALOG_GAIN_10;
		write_cmos_sensor(0xb1, (temp >> 8) & 0x3f);
		write_cmos_sensor(0xb2, (temp & 0xff));
		LOG_DBG("GC02M1MIPI analogic gain 4.74x, GC02M1MIPI add pregain = %d\n", temp);
	} else if ((iReg >= ANALOG_GAIN_11) && (iReg < ANALOG_GAIN_12)) {
		write_cmos_sensor(0xfe, 0x00);
		write_cmos_sensor(0xb6, 0x0a);
		temp = 1024 * iReg / ANALOG_GAIN_11;
		write_cmos_sensor(0xb1, (temp >> 8) & 0x3f);
		write_cmos_sensor(0xb2, (temp & 0xff));
		LOG_DBG("GC02M1MIPI analogic gain 5.6x, GC02M1MIPI add pregain = %d\n", temp);
	} else if ((iReg >= ANALOG_GAIN_12) && (iReg < ANALOG_GAIN_13)) {
		write_cmos_sensor(0xfe, 0x00);
		write_cmos_sensor(0xb6, 0x0b);
		temp = 1024 * iReg / ANALOG_GAIN_12;
		write_cmos_sensor(0xb1, (temp >> 8) & 0x3f);
		write_cmos_sensor(0xb2, (temp & 0xff));
		LOG_DBG("GC02M1MIPI analogic gain 6.636x, GC02M1MIPI add pregain = %d\n", temp);
	} else if ((iReg >= ANALOG_GAIN_13) && (iReg < ANALOG_GAIN_14)) {
		write_cmos_sensor(0xfe, 0x00);
		write_cmos_sensor(0xb6, 0x0c);
		temp = 1024 * iReg / ANALOG_GAIN_13;
		write_cmos_sensor(0xb1, (temp >> 8) & 0x3f);
		write_cmos_sensor(0xb2, (temp & 0xff));
		LOG_DBG("GC02M1MIPI analogic gain 8.0x, GC02M1MIPI add pregain = %d\n", temp);
	} else if ((iReg >= ANALOG_GAIN_14) && (iReg < ANALOG_GAIN_15)) {
		write_cmos_sensor(0xfe, 0x00);
		write_cmos_sensor(0xb6, 0x0d);
		temp = 1024 * iReg / ANALOG_GAIN_14;
		write_cmos_sensor(0xb1, (temp >> 8) & 0x3f);
		write_cmos_sensor(0xb2, (temp & 0xff));
		LOG_DBG("GC02M1MIPI analogic gain 9.48x, GC02M1MIPI add pregain = %d\n", temp);
	} else if ((iReg >= ANALOG_GAIN_15) && (iReg < ANALOG_GAIN_16)) {
		write_cmos_sensor(0xfe, 0x00);
		write_cmos_sensor(0xb6, 0x0e);
		temp = 1024 * iReg / ANALOG_GAIN_15;
		write_cmos_sensor(0xb1, (temp >> 8) & 0x3f);
		write_cmos_sensor(0xb2, (temp & 0xff));
		LOG_DBG("GC02M1MIPI analogic gain 11.2x, GC02M1MIPI add pregain = %d\n", temp);
	} else if ((iReg >= ANALOG_GAIN_16) && (iReg < ANALOG_GAIN_17)) {
		write_cmos_sensor(0xfe, 0x00);
		write_cmos_sensor(0xb6, 0x0f);
		temp = 1024 * iReg / ANALOG_GAIN_16;
		write_cmos_sensor(0xb1, (temp >> 8) & 0x3f);
		write_cmos_sensor(0xb2, (temp & 0xff));
		LOG_DBG("GC02M1MIPI analogic gain 13.2725x, GC02M1MIPI add pregain = %d\n", temp);
	} else {
		write_cmos_sensor(0xfe, 0x00);
		write_cmos_sensor(0xb6, 0x0f);
		temp = 1024 * iReg / ANALOG_GAIN_16;
		write_cmos_sensor(0xb1, (temp >> 8) & 0x3f);
		write_cmos_sensor(0xb2, (temp & 0xff));
		LOG_DBG("GC02M1MIPI analogic gain 16x, GC02M1MIPI add pregain = %d\n", temp);
	}
	LOG_DBG("celery--0xb6 =0x%x, 0xb1 =0x%x, 0xb2 =0x%x\n",
			read_cmos_sensor(0xb6), read_cmos_sensor(0xb1), read_cmos_sensor(0xb2));
	return gain;

}				/*    set_gain  */

static void ihdr_write_shutter_gain(kal_uint16 le, kal_uint16 se,
				    kal_uint16 gain)
{
	LOG_INF("le:0x%x, se:0x%x, gain:0x%x\n", le, se, gain);

}

/*************************************************************************
 * FUNCTION
 *    night_mode
 *
 * DESCRIPTION
 *    This function night mode of sensor.
 *
 * PARAMETERS
 *    bEnable: KAL_TRUE -> enable night mode, otherwise, disable night mode
 *
 * RETURNS
 *    None
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/
static void night_mode(kal_bool enable)
{
/*No Need to implement this function*/
}				/*    night_mode    */

#define I2C_BUFFER_LEN 360	/* trans# max is 255, each 3 bytes */
extern int iBurstWriteReg_multi
	(u8 *pData, u32 bytes, u16 i2cId, u16 transfer_length, u16 timing);

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
			puSendCmd[tosend++] = (char)addr;
			data = para[IDX + 1];
			puSendCmd[tosend++] = (char)data;
			IDX += 2;
			addr_last = addr;

		}

	if ((I2C_BUFFER_LEN - tosend) < 2 || IDX == len ||
						addr != addr_last) {
		iBurstWriteReg_multi(puSendCmd, tosend, imgsensor.i2c_write_id,
			2, imgsensor_info.i2c_speed);
			tosend = 0;
		}
	}
	return 0;
}

static int set_mode_setfile(enum IMGSENSOR_MODE mode)
{
	int ret = 0;

	if (mode >= IMGSENSOR_MODE_MAX) {
		LOG_ERR("invalid mode: %d", mode);
		return -1;
	}
	LOG_INF(" - E");
	LOG_INF("mode: %s", gc02m1_setfile_info[mode].name);

	if ((gc02m1_setfile_info[mode].setfile == NULL) || (gc02m1_setfile_info[mode].size == 0)) {
		LOG_ERR("failed, mode: %d", mode);
		ret = -1;
	}
	else
		ret = table_write_cmos_sensor(gc02m1_setfile_info[mode].setfile, gc02m1_setfile_info[mode].size);

	LOG_INF(" - X");

	return ret;
}

static int sensor_init(void)
{
	int ret = ERROR_NONE;

	LOG_INF("- E");
	ret = set_mode_setfile(IMGSENSOR_MODE_INIT);

	LOG_INF("- X");

	return ret;
}				/*    sensor_init  */

static kal_uint32 set_test_pattern_mode(kal_bool enable)
{
	LOG_INF("enable: %d\n", enable);

	if (enable) {
		write_cmos_sensor(0xfe, 0x01);
		write_cmos_sensor(0x8c, 0x11);
		write_cmos_sensor(0xfe, 0x00);
	} else {
		write_cmos_sensor(0xfe, 0x01);
		write_cmos_sensor(0x8c, 0x10);
		write_cmos_sensor(0xfe, 0x00);
	}
	spin_lock(&imgsensor_drv_lock);
	imgsensor.test_pattern = enable;
	spin_unlock(&imgsensor_drv_lock);
	return ERROR_NONE;
}

/*************************************************************************
 * FUNCTION
 *    get_imgsensor_id
 *
 * DESCRIPTION
 *    This function get the sensor ID
 *
 * PARAMETERS
 *    *sensorID : return the sensor ID
 *
 * RETURNS
 *    None
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/
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
				LOG_INF("i2c write id: 0x%x, sensor id: 0x%x\n",
					imgsensor.i2c_write_id, *sensor_id);
				return ERROR_NONE;
			}
			LOG_INF
			    ("Read sensor id fail, write id: 0x%x, id: 0x%x\n",
			     imgsensor.i2c_write_id, *sensor_id);
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
 *    open
 *
 * DESCRIPTION
 *    This function initialize the registers of CMOS sensor
 *
 * PARAMETERS
 *    None
 *
 * RETURNS
 *    None
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/
static kal_uint32 open(void)
{
	kal_uint8 i = 0;
	kal_uint8 retry = 2;
	kal_uint32 sensor_id = 0;
	kal_uint32 ret = ERROR_NONE;

	LOG_INF("- E");
	LOG_INF("GC02M1,MIPI 1LANE\n");

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
			LOG_INF
			    ("Read sensor id fail, write id: 0x%x, id: 0x%x\n",
			     imgsensor.i2c_write_id, sensor_id);
			retry--;
		} while (retry > 0);
		i++;
		if (sensor_id == imgsensor_info.sensor_id)
			break;
		retry = 2;
	}
	if (imgsensor_info.sensor_id != sensor_id)
		return ERROR_SENSOR_CONNECT_FAIL;

	/*Don't Remove!! */
	//gc02m1_gcore_identify_otp();

	/* initail sequence write in  */
	ret = sensor_init();

	/*write registers from sram */
	//gc02m1_gcore_update_otp();

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
	LOG_INF("- X");

	return ret;
}				/*    open  */


/*************************************************************************
 * FUNCTION
 *    close
 *
 * DESCRIPTION
 *
 *
 * PARAMETERS
 *    None
 *
 * RETURNS
 *    None
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/
static kal_uint32 close(void)
{
	LOG_INF("- E");
	streaming_control(KAL_FALSE);
	/*No Need to implement this function */
	LOG_INF("- X");

	return ERROR_NONE;
}				/*    close  */


/*************************************************************************
 * FUNCTION
 * preview
 *
 * DESCRIPTION
 *    This function start the sensor preview.
 *
 * PARAMETERS
 *    *image_window : address pointer of pixel numbers in one period of HSYNC
 *  *sensor_config_data : address pointer of line numbers in one period of VSYNC
 *
 * RETURNS
 *     None
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/
static kal_uint32 preview(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("- E");

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_PREVIEW;
	imgsensor.pclk = imgsensor_info.pre.pclk;
	//imgsensor.video_mode = KAL_FALSE;
	imgsensor.line_length = imgsensor_info.pre.linelength;
	imgsensor.frame_length = imgsensor_info.pre.framelength;
	imgsensor.min_frame_length = imgsensor_info.pre.framelength;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);

	set_mode_setfile(imgsensor.sensor_mode);
	LOG_INF("- X");

	return ERROR_NONE;
}				/*    preview   */


/*************************************************************************
 * FUNCTION
 *    capture
 *
 * DESCRIPTION
 *    This function setup the CMOS sensor in capture MY_OUTPUT mode
 *
 * PARAMETERS
 *
 * RETURNS
 *    None
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/
static kal_uint32 capture(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("- E");

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_CAPTURE;
	imgsensor.pclk = imgsensor_info.cap.pclk;
	imgsensor.line_length = imgsensor_info.cap.linelength;
	imgsensor.frame_length = imgsensor_info.cap.framelength;
	imgsensor.min_frame_length = imgsensor_info.cap.framelength;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);

	set_mode_setfile(imgsensor.sensor_mode);
	LOG_INF("- X");

	return ERROR_NONE;
}

static kal_uint32 normal_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *
			       image_window,
			       MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("- E");

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_VIDEO;
	imgsensor.pclk = imgsensor_info.normal_video.pclk;
	imgsensor.line_length = imgsensor_info.normal_video.linelength;
	imgsensor.frame_length = imgsensor_info.normal_video.framelength;
	imgsensor.min_frame_length = imgsensor_info.normal_video.framelength;
	/* imgsensor.current_fps = 300; */
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);

	set_mode_setfile(imgsensor.sensor_mode);
	LOG_INF("- X");

	return ERROR_NONE;
}


static kal_uint32 hs_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			   MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("- E");

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

	set_mode_setfile(imgsensor.sensor_mode);
	LOG_INF("- X");

	return ERROR_NONE;
}				/*    hs_video   */


static kal_uint32 slim_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			     MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("- E");

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

	set_mode_setfile(imgsensor.sensor_mode);
	LOG_INF("- X");

	return ERROR_NONE;
}				/*    slim_video     */

static kal_uint32 custom1(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			     MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("- E");

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

	set_mode_setfile(imgsensor.sensor_mode);
	LOG_INF("- X");

	return ERROR_NONE;
}				/*    custom1     */

static kal_uint32 custom2(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			     MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("- E");

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

	set_mode_setfile(imgsensor.sensor_mode);
	LOG_INF("- X");

	return ERROR_NONE;
}				/*    custom2     */
static kal_uint32 custom3(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			     MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("- E");

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

	set_mode_setfile(imgsensor.sensor_mode);
	LOG_INF("- X");

	return ERROR_NONE;
}				/*    custom3     */
static kal_uint32 custom4(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			     MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("- E");

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

	set_mode_setfile(imgsensor.sensor_mode);
	LOG_INF("- X");

	return ERROR_NONE;
}				/*    custom4     */
static kal_uint32 custom5(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			     MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("- E");

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

	set_mode_setfile(imgsensor.sensor_mode);
	LOG_INF("- X");

	return ERROR_NONE;
}				/*    custom5     */

static kal_uint32 get_resolution(MSDK_SENSOR_RESOLUTION_INFO_STRUCT *
				 sensor_resolution)
{
	LOG_INF("- E");
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
}				/*    get_resolution    */


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
	sensor_info->HighSpeedVideoDelayFrame =
	    imgsensor_info.hs_video_delay_frame;
	sensor_info->SlimVideoDelayFrame =
	    imgsensor_info.slim_video_delay_frame;
	sensor_info->Custom1DelayFrame =
	    imgsensor_info.custom1_delay_frame;
	sensor_info->Custom2DelayFrame =
	    imgsensor_info.custom2_delay_frame;
	sensor_info->Custom3DelayFrame =
	    imgsensor_info.custom3_delay_frame;
	sensor_info->Custom4DelayFrame =
	    imgsensor_info.custom4_delay_frame;
	sensor_info->Custom5DelayFrame =
	    imgsensor_info.custom5_delay_frame;

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

	sensor_info->SensorMIPILaneNumber = imgsensor_info.mipi_lane_num;
	sensor_info->SensorClockFreq = imgsensor_info.mclk;
	sensor_info->SensorClockDividCount = 3;	/* not use */
	sensor_info->SensorClockRisingCount = 0;
	sensor_info->SensorClockFallingCount = 2;	/* not use */
	sensor_info->SensorPixelClockCount = 3;	/* not use */
	sensor_info->SensorDataLatchCount = 2;	/* not use */

	sensor_info->MIPIDataLowPwr2HighSpeedTermDelayCount = 0;
	sensor_info->MIPICLKLowPwr2HighSpeedTermDelayCount = 0;
	sensor_info->SensorWidthSampling = 0;	// 0 is default 1x
	sensor_info->SensorHightSampling = 0;	// 0 is default 1x
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
		sensor_info->SensorGrabStartX =
		    imgsensor_info.custom1.startx;
		sensor_info->SensorGrabStartY =
		    imgsensor_info.custom1.starty;
		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
		    imgsensor_info.custom1.mipi_data_lp2hs_settle_dc;
		break;
	case MSDK_SCENARIO_ID_CUSTOM2:
		sensor_info->SensorGrabStartX =
			imgsensor_info.custom2.startx;
		sensor_info->SensorGrabStartY =
			imgsensor_info.custom2.starty;
		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
			imgsensor_info.custom2.mipi_data_lp2hs_settle_dc;
		break;
	case MSDK_SCENARIO_ID_CUSTOM3:
		sensor_info->SensorGrabStartX =
			imgsensor_info.custom3.startx;
		sensor_info->SensorGrabStartY =
			imgsensor_info.custom3.starty;
		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
			imgsensor_info.custom3.mipi_data_lp2hs_settle_dc;
		break;
	case MSDK_SCENARIO_ID_CUSTOM4:
		sensor_info->SensorGrabStartX =
			imgsensor_info.custom4.startx;
		sensor_info->SensorGrabStartY =
			imgsensor_info.custom4.starty;
		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
			imgsensor_info.custom4.mipi_data_lp2hs_settle_dc;
		break;
	case MSDK_SCENARIO_ID_CUSTOM5:
		sensor_info->SensorGrabStartX =
			imgsensor_info.custom5.startx;
		sensor_info->SensorGrabStartY =
			imgsensor_info.custom5.starty;
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
	case MSDK_SCENARIO_ID_CUSTOM5:
		custom5(image_window, sensor_config_data);
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
	LOG_INF("framerate = %d\n", framerate);
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


static kal_uint32 set_max_framerate_by_scenario(enum MSDK_SCENARIO_ID_ENUM
						scenario_id, MUINT32 framerate)
{
	kal_uint32 frame_length;

	LOG_INF("scenario_id = %d, framerate = %d\n", scenario_id, framerate);

	switch (scenario_id) {
	case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		frame_length = imgsensor_info.pre.pclk / framerate * 10 / imgsensor_info.pre.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line = (frame_length > imgsensor_info.pre.framelength) ?
			(frame_length - imgsensor_info.pre.framelength) : 0;
		imgsensor.frame_length = imgsensor_info.pre.framelength + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
		break;
	case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
		if (framerate == 0)
			return ERROR_NONE;
		frame_length =
			imgsensor_info.normal_video.pclk / framerate * 10 / imgsensor_info.normal_video.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line = (frame_length > imgsensor_info.normal_video.framelength) ?
			(frame_length - imgsensor_info.normal_video.framelength) : 0;
		imgsensor.frame_length = imgsensor_info.normal_video.framelength + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
		break;
	case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		if (imgsensor.current_fps != imgsensor_info.cap.max_framerate) {
			LOG_INF("Warning: current_fps %d fps is not support, so use cap's setting: %d fps!\n",
				framerate, imgsensor_info.cap.max_framerate / 10);
		}
		frame_length = imgsensor_info.cap.pclk / framerate * 10 / imgsensor_info.cap.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line = (frame_length > imgsensor_info.cap.framelength) ?
			(frame_length - imgsensor_info.cap.framelength) : 0;
		imgsensor.frame_length = imgsensor_info.cap.framelength + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
		break;
	case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
		frame_length = imgsensor_info.hs_video.pclk / framerate * 10 / imgsensor_info.hs_video.linelength;
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
		frame_length = imgsensor_info.slim_video.pclk / framerate * 10 / imgsensor_info.slim_video.linelength;
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
		frame_length = imgsensor_info.custom1.pclk / framerate * 10 / imgsensor_info.custom1.linelength;
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
		frame_length = imgsensor_info.custom2.pclk / framerate * 10 / imgsensor_info.custom2.linelength;
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
		frame_length = imgsensor_info.custom3.pclk / framerate * 10 / imgsensor_info.custom3.linelength;
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
		frame_length = imgsensor_info.custom4.pclk / framerate * 10 / imgsensor_info.custom4.linelength;
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
		frame_length = imgsensor_info.custom5.pclk / framerate * 10 / imgsensor_info.custom5.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line = (frame_length > imgsensor_info.custom5.framelength) ?
			(frame_length - imgsensor_info.custom5.framelength) : 0;
		imgsensor.frame_length = imgsensor_info.custom5.framelength + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
		break;
	default:  /*coding with  preview scenario by default*/
		frame_length = imgsensor_info.pre.pclk / framerate * 10 / imgsensor_info.pre.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line = (frame_length > imgsensor_info.pre.framelength) ?
			(frame_length - imgsensor_info.pre.framelength) : 0;
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



static kal_uint32 get_default_framerate_by_scenario(enum MSDK_SCENARIO_ID_ENUM
						    scenario_id,
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

static int wait_stream_changes(kal_uint32 addr, kal_uint32 val)
{
	kal_uint32 read_val = 0;
	kal_uint16 read_addr = 0;
	kal_uint32 max_cnt = 0, cur_cnt = 0;
	unsigned long wait_delay_ms = 0;

	read_addr = addr;
	wait_delay_ms = 2;
	max_cnt = 100;
	cur_cnt = 0;

	read_val = read_cmos_sensor(read_addr);

	while (read_val != val) {
		mDELAY(wait_delay_ms);
		read_val = read_cmos_sensor(read_addr);

		LOG_DBG("read addr = 0x%.2x, val = 0x%.2x", addr, read_val);
		cur_cnt++;

		if (cur_cnt >= max_cnt) {
			LOG_ERR("stream timeout: %d ms", (max_cnt * wait_delay_ms));
			break;
		}
	}
	LOG_INF("wait time: %d ms \n", (cur_cnt * wait_delay_ms));
	return 0;
}

static kal_uint32 streaming_control(kal_bool enable)
{
	LOG_INF("streaming_enable(0=Sw Standby,1=streaming): %d\n",
		 enable);

	if (enable) {
		write_cmos_sensor(0xfe, 0x00);
		write_cmos_sensor(0x3e, 0x90);
		wait_stream_changes(0x3e, 0x90);
	} else {
		write_cmos_sensor(0xfe, 0x00);
		write_cmos_sensor(0x3e, 0x00);
		wait_stream_changes(0x3e, 0x00);
	}

	return ERROR_NONE;
}

static void set_imgsensor_info_by_sensor_id(unsigned int sensor_id)
{
	switch (sensor_id) {
	case GC02M1_SENSOR_ID: //for macro sensor
		LOG_INF("set imgsensor info for GC02M1");
		imgsensor_info.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_R;
#if defined(CONFIG_CAMERA_AAT_V12) || defined(CONFIG_CAMERA_AAU_V22) ||\
	defined(CONFIG_CAMERA_MMU_V22) || defined(CONFIG_CAMERA_MMU_V32)
		imgsensor_info.min_gain_iso = 100; //old models
		LOG_INF("Min Gain ISO: 100");
#else
		imgsensor_info.min_gain_iso = 40;
		LOG_INF("Min Gain ISO: 40");
#endif

#if defined(CONFIG_CAMERA_AAU_V22) || defined(CONFIG_CAMERA_AAV_V13VE) || defined(CONFIG_CAMERA_MMV_V13VE)
		imgsensor_info.isp_driving_current = ISP_DRIVING_8MA; //mclk driving current
#elif defined(CONFIG_CAMERA_MMU_V32)
		imgsensor_info.isp_driving_current = ISP_DRIVING_4MA; //mclk driving current
#else
		imgsensor_info.isp_driving_current = ISP_DRIVING_6MA; //mclk driving current
#endif
		break;
	case GC02M1B_SENSOR_ID: //for bokeh sensor
		LOG_INF("set imgsensor info for GC02M1B");
		imgsensor_info.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_MONO;

#if defined(CONFIG_CAMERA_AAT_V12) || defined(CONFIG_CAMERA_AAT_V32X) ||\
	defined(CONFIG_CAMERA_AAU_V22) || defined(CONFIG_CAMERA_MMU_V22)
		imgsensor_info.min_gain_iso = 100;  //old models
		LOG_INF("Min Gain ISO: 100");
#else
		imgsensor_info.min_gain_iso = 33;
		LOG_INF("Min ISO: 33");
#endif

#if defined(CONFIG_CAMERA_AAT_V12) || defined(CONFIG_CAMERA_MMU_V32) || defined(CONFIG_CAMERA_MMU_V22)
		imgsensor_info.isp_driving_current = ISP_DRIVING_4MA; //mclk driving current
#else
		imgsensor_info.isp_driving_current = ISP_DRIVING_6MA; //mclk driving current
#endif
		break;
	default:
		LOG_INF("invalid sensor_id[%#x]", sensor_id);
		break;
	}
}

static kal_uint32 feature_control(MSDK_SENSOR_FEATURE_ENUM feature_id,
				  UINT8 *feature_para,
				  UINT32 *feature_para_len)
{
	UINT16 *feature_return_para_16 = (UINT16 *) feature_para;
	UINT16 *feature_data_16 = (UINT16 *) feature_para;
	UINT32 *feature_return_para_32 = (UINT32 *) feature_para;
	UINT32 *feature_data_32 = (UINT32 *) feature_para;
	unsigned long long *feature_data = (unsigned long long *)feature_para;
//unsigned long long *feature_return_para=(unsigned long long *) feature_para;

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
			*(MUINT32 *) (uintptr_t) (*(feature_data + 1)) = rate;
		}
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
	case SENSOR_FEATURE_SET_SHUTTER_FRAME_TIME:
		set_shutter_frame_length((UINT16) (*feature_data), (UINT16) (*(feature_data + 1)));
		break;
	case SENSOR_FEATURE_SET_REGISTER:
		write_cmos_sensor(sensor_reg_data->RegAddr,
				  sensor_reg_data->RegData);
		break;
	case SENSOR_FEATURE_GET_REGISTER:
		sensor_reg_data->RegData =
		    read_cmos_sensor(sensor_reg_data->RegAddr);
		LOG_INF("adb_i2c_read 0x%x = 0x%x\n", sensor_reg_data->RegAddr,
			sensor_reg_data->RegData);
		break;
	case SENSOR_FEATURE_GET_LENS_DRIVER_ID:
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
		set_auto_flicker_mode((BOOL) * feature_data_16,
				      *(feature_data_16 + 1));
		break;
	case SENSOR_FEATURE_SET_MAX_FRAME_RATE_BY_SCENARIO:
		set_max_framerate_by_scenario((enum MSDK_SCENARIO_ID_ENUM)
					      *feature_data,
					      *(feature_data + 1));
		break;
	case SENSOR_FEATURE_GET_DEFAULT_FRAME_RATE_BY_SCENARIO:
		get_default_framerate_by_scenario((enum MSDK_SCENARIO_ID_ENUM)
						  *(feature_data),
						  (MUINT32
						   *) (uintptr_t) (*
								   (feature_data
								    + 1)));
		break;
	case SENSOR_FEATURE_SET_TEST_PATTERN:
		set_test_pattern_mode((BOOL) * feature_data);
		break;
	case SENSOR_FEATURE_GET_TEST_PATTERN_CHECKSUM_VALUE:
		*feature_return_para_32 = imgsensor_info.checksum_value;
		*feature_para_len = 4;
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
		imgsensor.ihdr_en = (UINT8)*feature_data_32;
		spin_unlock(&imgsensor_drv_lock);
		break;
	case SENSOR_FEATURE_GET_CROP_INFO:
		LOG_INF("SENSOR_FEATURE_GET_CROP_INFO scenarioId: %d\n",
			(UINT32) *feature_data);
		wininfo =
		    (struct SENSOR_WINSIZE_INFO_STRUCT
		     *)(uintptr_t) (*(feature_data + 1));
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
		LOG_INF("SENSOR_SET_SENSOR_IHDR LE = %d, SE = %d, Gain = %d\n",
			(UINT16) *feature_data, (UINT16) *(feature_data + 1),
			(UINT16) *(feature_data + 2));
		ihdr_write_shutter_gain((UINT16) *feature_data,
					(UINT16) *(feature_data + 1),
					(UINT16) *(feature_data + 2));
		break;
	case SENSOR_FEATURE_GET_FRAME_CTRL_INFO_BY_SCENARIO:
		/*
		 * 1, if driver support new sw frame sync
		 * set_shutter_frame_length() support third para auto_extend_en
		 */
		*(feature_data + 1) = 1; /* margin info by scenario */
		*(feature_data + 2) = imgsensor_info.margin;
		break;
	case SENSOR_FEATURE_GET_AWB_REQ_BY_SCENARIO:
		*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) = 0;
		break;
	case SENSOR_FEATURE_GET_MIN_SHUTTER_BY_SCENARIO:
		*(feature_data + 1) = imgsensor_info.min_shutter;
		*(feature_data + 2) = imgsensor_info.exp_step;
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
	case SENSOR_FEATURE_SET_SENSOR_ID:
		set_imgsensor_info_by_sensor_id(*feature_data_32);
		break;
	default:
		break;
	}
	return ERROR_NONE;
}

static kal_uint8 gc02m1_otp_read_byte(kal_uint16 addr)
{
	write_cmos_sensor(0x17, addr);
	write_cmos_sensor(0xf3, 0x34);
	return (kal_uint8)read_cmos_sensor(0x19);
}

static void gc02m1_otp_off_setting(void)
{
	write_cmos_sensor(0xf3, 0x00);
	write_cmos_sensor(0xfe, 0x00);
}

int gc02m1_read_otp_cal(unsigned int addr, unsigned char *data, unsigned int size)
{
	kal_uint32 i = 0;
	kal_uint8 read_addr = GC02M1_OTP_START_ADDR;

	LOG_INF("- E, size %d", size);

	if (data == NULL) {
		LOG_ERR("fail, data is NULL");
		return -1;
	}

	// OTP initial setting
	if (write_cmos_sensor(0xf3, 0x30) < 0) {
		LOG_ERR("Failed to write_cmos_sensor");
		gc02m1_otp_off_setting();
		return -1;
	}
	write_cmos_sensor(0xfe, 0x02); //Page_2_select

	for (i = 0; i < size; i++) {
		*(data + i) = gc02m1_otp_read_byte(read_addr);
		LOG_INF("read data read_addr: %x, data: %x", read_addr, *(data + i));
		read_addr += 8;
	}

	// OTP off setting
	gc02m1_otp_off_setting();
	LOG_INF("%s - X\n", __func__);

	return (int)size;
}

static struct SENSOR_FUNCTION_STRUCT sensor_func = {
	open,
	get_info,
	get_resolution,
	feature_control,
	control,
	close
};

UINT32 GC02M1_MIPI_RAW_SensorInit(struct SENSOR_FUNCTION_STRUCT **pfFunc)
{
	/* To Do : Check Sensor status here */
	if (pfFunc != NULL)
		*pfFunc = &sensor_func;
	return ERROR_NONE;
}				/*    GC02M1_MIPI_RAW_SensorInit    */
