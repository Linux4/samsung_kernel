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

#include "gc13a0mipiraw_Sensor.h"
#include "gc13a0mipiraw_setfile.h"
#include "gc13a0mipiraw_adaptive_mipi.h"

/****************************Modify Following Strings for Debug***************/
#define PFX "GC13A0 D/D"
/****************************   Modify end    ********************/

#define LOG_DBG(format, args...) pr_debug(PFX "[%s] " format, __func__, ##args)
#define LOG_INF(format, args...) pr_info(PFX "[%s] " format, __func__, ##args)
#define LOG_ERR(format, args...) pr_err(PFX "[%s] " format, __func__, ##args)

static kal_uint32 streaming_control(kal_bool enable);

static DEFINE_SPINLOCK(imgsensor_drv_lock);

#define TRANSFER_UNIT (3)
#define MULTI_WRITE 1
#if MULTI_WRITE
#define I2C_BUFFER_LEN (TRANSFER_UNIT * 255) /* trans# max is 255, (TRANSFER_UNIT) bytes */
#else
#define I2C_BUFFER_LEN (TRANSFER_UNIT)
#endif

static kal_bool enable_adaptive_mipi;
static int adaptive_mipi_index;

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
	/*.sensor_id = GC13A0_SENSOR_ID, sensor_id = 0x13A0*/
	.sensor_id = GC13A0_SENSOR_ID,

	.checksum_value = 0xf7375923,	//checksum value for Camera Auto Test

	.pre = {
		.pclk = 569400000,
		.linelength = 5850,
		.framelength = 3232,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 4128,
		.grabwindow_height = 3096,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 482560000,
		.max_framerate = 300,
	},
	.cap = {
		.pclk = 569400000,
		.linelength = 5850,
		.framelength = 3232,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 4128,
		.grabwindow_height = 3096,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 482560000,
		.max_framerate = 300,
	},
	.normal_video = {
		.pclk = 569400000,
		.linelength = 5850,
		.framelength = 3232,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 4128,
		.grabwindow_height = 2324,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 482560000,
		.max_framerate = 300,
	 },
	.hs_video = { // NOT USED
		.pclk = 569400000,
		.linelength = 5850,
		.framelength = 3232,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 4128,
		.grabwindow_height = 3096,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 482560000,
		.max_framerate = 300,
	},
	.slim_video = { // NOT USED
		.pclk = 569400000,
		.linelength = 5850,
		.framelength = 3232,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 4128,
		.grabwindow_height = 3096,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 482560000,
		.max_framerate = 300,
	},
	.custom1 = {
		.pclk = 569400000,
		.linelength = 5850,
		.framelength = 1616,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 1028,
		.grabwindow_height = 772,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 226720000,
		.max_framerate = 600,
	},
	.custom2 = {
		.pclk = 569400000,
		.linelength = 5850,
		.framelength = 3232,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 3712,
		.grabwindow_height = 2556,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 482560000,
		.max_framerate = 300,
	},
	.custom3 = {
		.pclk = 569400000,
		.linelength = 5850,
		.framelength = 3232,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 3712,
		.grabwindow_height = 2556,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 482560000,
		.max_framerate = 300,
	},
	.custom4 = {
		.pclk = 569400000,
		.linelength = 5850,
		.framelength = 3232,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 3712,
		.grabwindow_height = 2556,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 482560000,
		.max_framerate = 300,
	},
	.custom5 = { // NOT USED
		.pclk = 569400000,
		.linelength = 5850,
		.framelength = 3232,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 4128,
		.grabwindow_height = 3096,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 482560000,
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

	.is_invalid_mode = false,

	.margin = 16,                    //sensor framelength & shutter margin
	.min_shutter = 4,                //min shutter
	.min_gain = 64,                  /*1x gain*/
	.max_gain = 640,                 /*16x gain*/
	.min_gain_iso = 50,
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
	.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_Gr,
	.mclk = 26,
	.mipi_lane_num = SENSOR_MIPI_4_LANE,
	.i2c_addr_table = {0x72, 0xff},
	.i2c_speed = 1000,
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
	.i2c_write_id = 0x72,	//record current sensor's i2c write id
};


/* Sensor output window information */
static struct SENSOR_WINSIZE_INFO_STRUCT imgsensor_winsize_info[] = {
	{   0,    0,    0,    0,    0,    0,    0,    0,
	    0,    0,    0,    0,    0,    0,    0,    0}, /* init - NOT USED */
	{4208, 3120,   40,   12, 4128, 3096, 4128, 3096,
	    0,    0, 4128, 3096,    0,    0, 4128, 3096}, /* Preview */
	{4208, 3120,   40,   12, 4128, 3096, 4128, 3096,
	    0,    0, 4128, 3096,    0,    0, 4128, 3096}, /* capture */
	{4208, 3120,   40,  398, 4128, 2324, 4128, 2324,
	    0,    0, 4128, 2324,    0,    0, 4128, 2324}, /* video */
	{4208, 3120,   40,   12, 4128, 3096, 4128, 3096,
	    0,    0, 4128, 3096,    0,    0, 4128, 3096}, /* high speed video -NOT USED */
	{4208, 3120,   40,   12, 4128, 3096, 4128, 3096,
	    0,    0, 4128, 3096,    0,    0, 4128, 3096}, /* slim video -NOT USED */
	{4208, 3120,   48,   16, 4112, 3088, 1028,  772,
	    0,    0, 1028,  772,    0,    0, 1028,  772}, /* custom1 */
	{4208, 3120,  248,  282, 3712, 2556, 3712, 2556,
	    0,    0, 3712, 2556,    0,    0, 3712, 2556}, /* custom2 */
	{4208, 3120,  248,  282, 3712, 2556, 3712, 2556,
	    0,    0, 3712, 2556,    0,    0, 3712, 2556}, /* custom3 */
	{4208, 3120,  248,  282, 3712, 2556, 3712, 2556,
	    0,    0, 3712, 2556,    0,    0, 3712, 2556}, /* custom4 */
	{4208, 3120,   40,   12, 4128, 3096, 4128, 3096,
	    0,    0, 4128, 3096,    0,    0, 4128, 3096}, /* Custom5 -NOT USED */
};

static kal_uint16 read_cmos_sensor_8(kal_uint16 addr)
{
	kal_uint16 get_byte = 0;
	char pusendcmd[2] = { (char)(addr >> 8), (char)(addr & 0xFF) };

	iReadRegI2CTiming(pusendcmd, 2, (u8 *) &get_byte, 1,
		imgsensor.i2c_write_id, imgsensor_info.i2c_speed);
	return get_byte;
}

static int write_cmos_sensor_8(kal_uint16 addr, kal_uint8 para)
{
	int ret = 0;
	char pusendcmd[3] = {
		(char)(addr >> 8), (char)(addr & 0xFF), (char)(para & 0xFF) };

	ret = iWriteRegI2CTiming(pusendcmd, 3,
		imgsensor.i2c_write_id, imgsensor_info.i2c_speed);

	return ret;
}

static int table_write_cmos_sensor(kal_uint16 *para, kal_uint32 len)
{
	int ret = 0;
	char puSendCmd[I2C_BUFFER_LEN];
	kal_uint32 tosend, IDX;
	kal_uint16 addr = 0, addr_last = 0, data = 0;

	tosend = 0;
	IDX = 0;

	while (len > IDX) {
		addr = para[IDX];
		puSendCmd[tosend++] = (char)(addr >> 8);
		puSendCmd[tosend++] = (char)(addr & 0xFF);

		data = para[IDX + 1];
		puSendCmd[tosend++] = (char)data;

		IDX += 2;
		addr_last = addr;

		if ((I2C_BUFFER_LEN - tosend) < TRANSFER_UNIT || IDX == len || addr != addr_last) {
			ret |= iBurstWriteReg_multi(puSendCmd, tosend, imgsensor.i2c_write_id,
					TRANSFER_UNIT, imgsensor_info.i2c_speed);
			tosend = 0;
		}
	}
	return ret;
}

static void set_dummy(void)
{
	LOG_DBG("dummyline = %d, dummypixels = %d\n",
		imgsensor.dummy_line, imgsensor.dummy_pixel);

	write_cmos_sensor_8(0x0340, (u8)(imgsensor.frame_length >> 8) & 0xFF);
	write_cmos_sensor_8(0x0341, (u8)(imgsensor.frame_length & 0xFF));
}

static kal_uint32 return_sensor_id(void)
{
	return (read_cmos_sensor_8(0x03f0) << 8) | read_cmos_sensor_8(0x03f1);
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

	write_cmos_sensor_8(0x0202, (shutter >> 8) & 0xff);
	write_cmos_sensor_8(0x0203, shutter & 0xff);
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

	write_cmos_sensor_8(0x0202, (shutter >> 8) & 0xff);
	write_cmos_sensor_8(0x0203, shutter & 0xff);

	LOG_DBG("shutter = %d, framelength = %d Stereo\n", shutter, imgsensor.frame_length);
}

static kal_uint16 gain2reg(const kal_uint16 gain)
{
	kal_uint16 reg_gain = 0x0;
	kal_uint16 in_gain = gain;

	if (in_gain < imgsensor_info.min_gain)
		in_gain = imgsensor_info.min_gain;
	else if (in_gain > imgsensor_info.max_gain)
		in_gain = imgsensor_info.max_gain;

	reg_gain = in_gain * 16;

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

	reg_gain = gain2reg(gain);

	spin_lock(&imgsensor_drv_lock);
	imgsensor.gain = reg_gain;
	spin_unlock(&imgsensor_drv_lock);

	LOG_DBG("in_gain = 0x%04x, reg_gain = 0x%04x\n ", gain, reg_gain);

	write_cmos_sensor_8(0x0204, (reg_gain >> 8) & 0xff);
	write_cmos_sensor_8(0x0205, reg_gain & 0xff);
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

static int set_mode_setfile(enum IMGSENSOR_MODE mode)
{
	int ret = 0;

	if (mode >= IMGSENSOR_MODE_MAX) {
		LOG_ERR("invalid mode: %d", mode);
		return -1;
	}
	LOG_INF(" - E");
	LOG_INF("mode: %s", gc13a0_setfile_info[mode].name);

	if ((gc13a0_setfile_info[mode].setfile == NULL) || (gc13a0_setfile_info[mode].size == 0)) {
		LOG_ERR("failed, mode: %d", mode);
		ret = -1;
	} else
		ret = table_write_cmos_sensor(gc13a0_setfile_info[mode].setfile, gc13a0_setfile_info[mode].size);

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

	//test pattern is always disable
	spin_lock(&imgsensor_drv_lock);
	imgsensor.test_pattern = false;
	spin_unlock(&imgsensor_drv_lock);
	LOG_INF("test pattern not supported");

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
			LOG_INF("i2c write id: 0x%x, sensor id: 0x%x\n",
				imgsensor.i2c_write_id, *sensor_id);
			if (*sensor_id == imgsensor_info.sensor_id)
				return ERROR_NONE;

			retry--;
		} while (retry > 0);
		i++;
		retry = 2;
	}
	if (*sensor_id != imgsensor_info.sensor_id) {
		LOG_ERR("Read sensor id fail, write id: 0x%x, id: 0x%x\n",
			imgsensor.i2c_write_id, *sensor_id);
		/* if Sensor ID is not correct, Must set *sensor_id to 0xFFFFFFFF */
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
	LOG_INF("GC13A0,MIPI 4LANE\n");

	while (imgsensor_info.i2c_addr_table[i] != 0xff) {
		spin_lock(&imgsensor_drv_lock);
		imgsensor.i2c_write_id = imgsensor_info.i2c_addr_table[i];
		spin_unlock(&imgsensor_drv_lock);
		do {
			sensor_id = return_sensor_id();
			LOG_INF("i2c write id: 0x%x, sensor id: 0x%x\n",
				imgsensor.i2c_write_id, sensor_id);
			if (sensor_id == imgsensor_info.sensor_id)
				break;

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
	imgsensor.pclk				= imgsensor_info.mode_info[IMGSENSOR_MODE_PREVIEW]->pclk;
	imgsensor.frame_length		= imgsensor_info.mode_info[IMGSENSOR_MODE_PREVIEW]->framelength;
	imgsensor.line_length		= imgsensor_info.mode_info[IMGSENSOR_MODE_PREVIEW]->linelength;
	imgsensor.min_frame_length	= imgsensor_info.mode_info[IMGSENSOR_MODE_PREVIEW]->framelength;
	imgsensor.dummy_pixel = 0;
	imgsensor.dummy_line = 0;
	imgsensor.ihdr_en = 0;
	imgsensor.test_pattern = KAL_FALSE;
	imgsensor.current_fps		= imgsensor_info.mode_info[IMGSENSOR_MODE_PREVIEW]->max_framerate;
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
	enable_adaptive_mipi = KAL_FALSE;

	LOG_INF("- X");

	return ERROR_NONE;
}				/*    close  */

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

static enum IMGSENSOR_MODE get_imgsensor_mode(enum MSDK_SCENARIO_ID_ENUM scenario_id)
{
	enum IMGSENSOR_MODE sensor_mode = IMGSENSOR_MODE_PREVIEW;

	imgsensor_info.is_invalid_mode = false;
	switch (scenario_id) {
	case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		sensor_mode = IMGSENSOR_MODE_PREVIEW;
		break;

	case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		sensor_mode = IMGSENSOR_MODE_CAPTURE;
		break;

	case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
		sensor_mode = IMGSENSOR_MODE_VIDEO;
		break;

	case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
		sensor_mode = IMGSENSOR_MODE_HIGH_SPEED_VIDEO;
		break;

	case MSDK_SCENARIO_ID_SLIM_VIDEO:
		sensor_mode = IMGSENSOR_MODE_SLIM_VIDEO;
		break;

	case MSDK_SCENARIO_ID_CUSTOM1:
		sensor_mode = IMGSENSOR_MODE_CUSTOM1;
		break;

	case MSDK_SCENARIO_ID_CUSTOM2:
		sensor_mode = IMGSENSOR_MODE_CUSTOM2;
		break;

	case MSDK_SCENARIO_ID_CUSTOM3:
		sensor_mode = IMGSENSOR_MODE_CUSTOM3;
		break;

	case MSDK_SCENARIO_ID_CUSTOM4:
		sensor_mode = IMGSENSOR_MODE_CUSTOM4;
		break;

	case MSDK_SCENARIO_ID_CUSTOM5:
		sensor_mode = IMGSENSOR_MODE_CUSTOM5;
		break;

	default:
		LOG_DBG("error scenario_id = %d, we use preview scenario", scenario_id);
		imgsensor_info.is_invalid_mode = true;
		sensor_mode = IMGSENSOR_MODE_PREVIEW;
		break;
	}
	return sensor_mode;
}

static kal_uint32 get_resolution(
	MSDK_SENSOR_RESOLUTION_INFO_STRUCT(*sensor_resolution))
{
	LOG_INF("- E\n");
	sensor_resolution->SensorFullWidth =
		imgsensor_info.mode_info[IMGSENSOR_MODE_CAPTURE]->grabwindow_width;
	sensor_resolution->SensorFullHeight =
		imgsensor_info.mode_info[IMGSENSOR_MODE_CAPTURE]->grabwindow_height;

	sensor_resolution->SensorPreviewWidth =
		imgsensor_info.mode_info[IMGSENSOR_MODE_PREVIEW]->grabwindow_width;
	sensor_resolution->SensorPreviewHeight =
		imgsensor_info.mode_info[IMGSENSOR_MODE_PREVIEW]->grabwindow_height;

	sensor_resolution->SensorVideoWidth =
		imgsensor_info.mode_info[IMGSENSOR_MODE_VIDEO]->grabwindow_width;
	sensor_resolution->SensorVideoHeight =
		imgsensor_info.mode_info[IMGSENSOR_MODE_VIDEO]->grabwindow_height;

	sensor_resolution->SensorHighSpeedVideoWidth =
		imgsensor_info.mode_info[IMGSENSOR_MODE_HIGH_SPEED_VIDEO]->grabwindow_width;
	sensor_resolution->SensorHighSpeedVideoHeight =
		imgsensor_info.mode_info[IMGSENSOR_MODE_HIGH_SPEED_VIDEO]->grabwindow_height;

	sensor_resolution->SensorSlimVideoWidth =
		imgsensor_info.mode_info[IMGSENSOR_MODE_SLIM_VIDEO]->grabwindow_width;
	sensor_resolution->SensorSlimVideoHeight =
		imgsensor_info.mode_info[IMGSENSOR_MODE_SLIM_VIDEO]->grabwindow_height;

	sensor_resolution->SensorCustom1Width =
		imgsensor_info.mode_info[IMGSENSOR_MODE_CUSTOM1]->grabwindow_width;
	sensor_resolution->SensorCustom1Height =
		imgsensor_info.mode_info[IMGSENSOR_MODE_CUSTOM1]->grabwindow_height;

	sensor_resolution->SensorCustom2Width =
		imgsensor_info.mode_info[IMGSENSOR_MODE_CUSTOM2]->grabwindow_width;
	sensor_resolution->SensorCustom2Height =
		imgsensor_info.mode_info[IMGSENSOR_MODE_CUSTOM2]->grabwindow_height;

	sensor_resolution->SensorCustom3Width =
		imgsensor_info.mode_info[IMGSENSOR_MODE_CUSTOM3]->grabwindow_width;
	sensor_resolution->SensorCustom3Height =
		imgsensor_info.mode_info[IMGSENSOR_MODE_CUSTOM3]->grabwindow_height;

	sensor_resolution->SensorCustom4Width =
		imgsensor_info.mode_info[IMGSENSOR_MODE_CUSTOM4]->grabwindow_width;
	sensor_resolution->SensorCustom4Height =
		imgsensor_info.mode_info[IMGSENSOR_MODE_CUSTOM4]->grabwindow_height;

	sensor_resolution->SensorCustom5Width =
		imgsensor_info.mode_info[IMGSENSOR_MODE_CUSTOM5]->grabwindow_width;
	sensor_resolution->SensorCustom5Height =
		imgsensor_info.mode_info[IMGSENSOR_MODE_CUSTOM5]->grabwindow_height;

	return ERROR_NONE;
}


static kal_uint32 get_info(enum MSDK_SCENARIO_ID_ENUM scenario_id,
			   MSDK_SENSOR_INFO_STRUCT *sensor_info,
			   MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	enum IMGSENSOR_MODE sensor_mode;

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

	LOG_INF("scenario_id = %d\n", scenario_id);
	sensor_mode = get_imgsensor_mode(scenario_id);

	sensor_info->SensorGrabStartX = imgsensor_info.mode_info[sensor_mode]->startx;
	sensor_info->SensorGrabStartY = imgsensor_info.mode_info[sensor_mode]->starty;
	sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
		imgsensor_info.mode_info[sensor_mode]->mipi_data_lp2hs_settle_dc;

	return ERROR_NONE;
}

static kal_uint32 control(enum MSDK_SCENARIO_ID_ENUM scenario_id,
			  MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{

	enum IMGSENSOR_MODE sensor_mode;

	LOG_INF("scenario_id = %d\n", scenario_id);

	spin_lock(&imgsensor_drv_lock);
	imgsensor.current_scenario_id = scenario_id;
	spin_unlock(&imgsensor_drv_lock);

	sensor_mode = get_imgsensor_mode(scenario_id);

	set_imgsensor_mode_info(sensor_mode);
	set_mode_setfile(sensor_mode);
	if (imgsensor_info.is_invalid_mode) {
		LOG_ERR("Invalid scenario id - use preview setting");
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

static void set_frame_length(enum IMGSENSOR_MODE sensor_mode, MUINT32 framerate)
{
	kal_uint32 frame_length;

	frame_length = imgsensor_info.mode_info[sensor_mode]->pclk / framerate * 10 /
		imgsensor_info.mode_info[sensor_mode]->linelength;

	spin_lock(&imgsensor_drv_lock);

	imgsensor.dummy_line = (frame_length > imgsensor_info.mode_info[sensor_mode]->framelength)
		? (frame_length - imgsensor_info.mode_info[sensor_mode]->framelength) : 0;

	imgsensor.frame_length = imgsensor_info.mode_info[sensor_mode]->framelength + imgsensor.dummy_line;

	imgsensor.min_frame_length = imgsensor.frame_length;

	spin_unlock(&imgsensor_drv_lock);

	if (imgsensor.frame_length > imgsensor.shutter)
		set_dummy();
}

static kal_uint32 set_max_framerate_by_scenario(enum MSDK_SCENARIO_ID_ENUM
						scenario_id, MUINT32 framerate)
{

	enum IMGSENSOR_MODE sensor_mode;

	LOG_INF("scenario_id = %d, framerate = %d\n", scenario_id, framerate);

	sensor_mode = get_imgsensor_mode(scenario_id);
	set_frame_length(sensor_mode, framerate);

	return ERROR_NONE;
}



static kal_uint32 get_default_framerate_by_scenario(enum MSDK_SCENARIO_ID_ENUM
						    scenario_id,
						    MUINT32 *framerate)
{
	enum IMGSENSOR_MODE sensor_mode;

	sensor_mode = get_imgsensor_mode(scenario_id);
	if (!imgsensor_info.is_invalid_mode)
		*framerate = imgsensor_info.mode_info[sensor_mode]->max_framerate;

	LOG_INF("scenario_id = %d, framerate = %d", scenario_id, *framerate);
	return ERROR_NONE;
}

static int update_mipi_info(enum MSDK_SCENARIO_ID_ENUM scenario_id)
{
	enum IMGSENSOR_MODE sensor_mode;
	unsigned int mipi_pixel_rate[CAM_GC13A0_SET_MAX_NUM] = {
		478400000, 470080000, 490880000, 495040000,
	};

	LOG_INF("- E\n");
	sensor_mode = get_imgsensor_mode(scenario_id);
	if (sensor_mode == IMGSENSOR_MODE_CUSTOM1) {
		LOG_INF("NOT supported sensor mode : %d", sensor_mode);
		return ERROR_NONE;
	}

	adaptive_mipi_index = imgsensor_select_mipi_by_rf_channel(gc13a0_mipi_channel_full,
						ARRAY_SIZE(gc13a0_mipi_channel_full));
	if (adaptive_mipi_index < 0 || adaptive_mipi_index >= CAM_GC13A0_SET_MAX_NUM) {
		LOG_ERR("Not found mipi index: %d. mipi index is set to defaults value, 598Mhz", adaptive_mipi_index);
		adaptive_mipi_index = CAM_GC13A0_SET_A_All_598_MHZ;
	}

	imgsensor_info.mode_info[sensor_mode]->mipi_pixel_rate = mipi_pixel_rate[adaptive_mipi_index];
	LOG_INF("- X\n");
	return adaptive_mipi_index;
}

static int set_mipi_mode(int mipi_index)
{
	enum IMGSENSOR_MODE sensor_mode;

	LOG_DBG("- E : adaptive mipi index %d", mipi_index);

	if (mipi_index <= CAM_GC13A0_SET_A_All_598_MHZ || mipi_index >= CAM_GC13A0_SET_MAX_NUM) {
		LOG_INF("adaptive Mipi is set to the default values of 598Mhz");
	} else {
		sensor_mode = get_imgsensor_mode(imgsensor.current_scenario_id);
		LOG_INF("adaptive Mipi setting(%s)", gc13a0_adaptive_mipi_setfile[sensor_mode][mipi_index].name);
		table_write_cmos_sensor(gc13a0_adaptive_mipi_setfile[sensor_mode][mipi_index].setfile,
			gc13a0_adaptive_mipi_setfile[sensor_mode][mipi_index].size);
	}
	LOG_DBG("- X");
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

	read_val = read_cmos_sensor_8(read_addr) & 0x40;

	while (read_val != val) {
		mDELAY(wait_delay_ms);
		read_val = read_cmos_sensor_8(read_addr) & 0x40;

		LOG_DBG("read addr = 0x%.2x, val = 0x%.2x", addr, read_val);
		cur_cnt++;

		if (cur_cnt >= max_cnt) {
			LOG_ERR("stream timeout: %d ms", (max_cnt * wait_delay_ms));
			break;
		}
	}
	LOG_INF("wait time: %d ms\n", (cur_cnt * wait_delay_ms));
	return 0;
}

static kal_uint32 streaming_control(kal_bool enable)
{
	LOG_INF("streaming_enable(0= Sw Standby,1= streaming): %d\n", enable);
	if (enable) {
		if (enable_adaptive_mipi && imgsensor.current_scenario_id != MSDK_SCENARIO_ID_CUSTOM1)
			set_mipi_mode(adaptive_mipi_index);

		write_cmos_sensor_8(0x0100, 0X01);
		wait_stream_changes(0x0180, 0x40);
	} else {
		write_cmos_sensor_8(0x0100, 0x00);
		wait_stream_changes(0x0180, 0x00);
	}
	return ERROR_NONE;
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
enum IMGSENSOR_MODE sensor_mode;

	struct SENSOR_WINSIZE_INFO_STRUCT *wininfo;
	MSDK_SENSOR_REG_INFO_STRUCT *sensor_reg_data =
	    (MSDK_SENSOR_REG_INFO_STRUCT *) feature_para;

	LOG_DBG("feature_id = %d\n", feature_id);
	switch (feature_id) {
	case SENSOR_FEATURE_GET_PIXEL_CLOCK_FREQ_BY_SCENARIO:
		sensor_mode = get_imgsensor_mode(*feature_data);
		*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) = imgsensor_info.mode_info[sensor_mode]->pclk;
		break;
	case SENSOR_FEATURE_GET_PERIOD_BY_SCENARIO:
		sensor_mode = get_imgsensor_mode(*feature_data);
		*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) = (imgsensor_info.mode_info[sensor_mode]->framelength << 16)
			+ imgsensor_info.mode_info[sensor_mode]->linelength;
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
		LOG_DBG("SENSOR_FEATURE_GET_MIPI_PIXEL_RATE = %d, MSDK_SCENARIO_ID = %d\n",
			feature_id, *feature_data);
		if (enable_adaptive_mipi)
			update_mipi_info(*feature_data);

		sensor_mode = get_imgsensor_mode(*feature_data);
		*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) = imgsensor_info.mode_info[sensor_mode]->mipi_pixel_rate;
		break;

	case SENSOR_FEATURE_SET_ADAPTIVE_MIPI:
		if (*feature_para == 1)
			enable_adaptive_mipi = KAL_TRUE;
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
		write_cmos_sensor_8(sensor_reg_data->RegAddr,
				  sensor_reg_data->RegData);
		break;
	case SENSOR_FEATURE_GET_REGISTER:
		sensor_reg_data->RegData =
		    read_cmos_sensor_8(sensor_reg_data->RegAddr);
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
		sensor_mode = get_imgsensor_mode(*feature_data);
		memcpy((void *)wininfo, (void *)&imgsensor_winsize_info[sensor_mode],
				sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
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

UINT32 GC13A0_MIPI_RAW_SensorInit(struct SENSOR_FUNCTION_STRUCT **pfFunc)
{
	/* To Do : Check Sensor status here */
	if (pfFunc != NULL)
		*pfFunc = &sensor_func;
	return ERROR_NONE;
}				/*    GC13A0_MIPI_RAW_SensorInit    */

