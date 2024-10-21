// SPDX-License-Identifier: GPL-2.0
/*****************************************************************************
 *
 * Filename:
 * ---------
 *	 w3sc1321csfrontlce_txd_front_mipi_raw.c
 *
 * Project:
 * --------
 *	 ALPS
 *
 * Description:
 * ------------
 *	 Source code of Sensor driver
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
#include "w3sc1321csfrontlcemipiraw_Sensor.h"

#define PFX "sc1321_txd_front_mipi_raw"
#define LOG_INFO(format, args...) \
	pr_info(PFX "[%s] " format, __func__, ##args)
#define LOG_ERR(format, args...) \
	pr_err(PFX "[%s] " format, __func__, ##args)

#define sc1321cs_SENSOR_GAIN_MAX_VALID_INDEX  5
#define sc1321cs_SENSOR_GAIN_MAP_SIZE	 5

#define sc1321cs_SENSOR_BASE_GAIN		 0x400
#define sc1321cs_SENSOR_MAX_GAIN		 (16 * sc1321cs_SENSOR_BASE_GAIN)
#define MODE_NUM				 5
//+w3 camera bring up --otp porting.
#define OTP_PORTING              1
#if OTP_PORTING
#include "cam_cal_define.h"
#include <linux/slab.h>
static struct stCAM_CAL_DATAINFO_STRUCT w3sc1321csfrontlce_eeprom_data ={
	.sensorID= W3SC1321CSFRONTLCE_SENSOR_ID,
	.deviceID = 0x02,
	.dataLength = 0x0778,
	.sensorVendorid = 0x13050000,
	.vendorByte = {1,2,3,4},
	.dataBuffer = NULL,
};
static struct stCAM_CAL_CHECKSUM_STRUCT w3sc1321csfrontlce_checksum[6] =
{
	{MODULE_ITEM,0x0000,0x0000,0x0007,0x0008,0x55},
	{SN_DATA,0x0009,0x0009,0x0015,0x0016,0x55},
	{AWB_ITEM,0x0017,0x0017,0x0027,0x0028,0x55},
	{LSC_ITEM,0x0029,0x0029,0x0775,0x0776,0x55},
	{TOTAL_ITEM,0x0000,0x0000,0x0776,0x0777,0x55},
	{MAX_ITEM,0xFFFF,0xFFFF,0xFFFF,0xFFFF,0x55},  // this line must haved
};
extern int imgSensorReadEepromData(struct stCAM_CAL_DATAINFO_STRUCT* pData,
	struct stCAM_CAL_CHECKSUM_STRUCT* checkData);

#define EEPROM_TH24C64UB_ID 0xA0

static kal_uint16 read_eeprom(kal_uint32 addr)
{
	kal_uint16 get_byte=0;

	char pu_send_cmd[2] = { (char)(addr >> 8), (char)(addr & 0xFF) };
	iReadRegI2C(pu_send_cmd, 2, (u8*)&get_byte, 1, EEPROM_TH24C64UB_ID);

	return get_byte;
}
#endif
//-w3 camera bring up --otp porting.

static DEFINE_SPINLOCK(imgsensor_drv_lock);

static struct imgsensor_info_struct imgsensor_info = {
	.sensor_id = W3SC1321CSFRONTLCE_SENSOR_ID,	/* default: 0xc658 */
	.checksum_value = 0xf7375923,			/* checksum value for Camera Auto Test */

	.pre = {
		.pclk = 120000000,
		.linelength  = 1250,
		.framelength = 3200,
		.startx = 0,
		.starty = 0,
		.grabwindow_width  = 2104,
		.grabwindow_height = 1560,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 300,
		.mipi_pixel_rate = 547200000,
	},
	.cap = {
		.pclk = 120000000,
		.linelength = 1250,
		.framelength = 3200,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 4208,
		.grabwindow_height = 3120,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 300,
		.mipi_pixel_rate = 547200000,
	},
	.cap1 = {
		.pclk = 120000000,
		.linelength = 1250,
		.framelength = 3200,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 4208,
		.grabwindow_height = 3120,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 300,
		.mipi_pixel_rate = 547200000,
	},
	.normal_video = {
		.pclk = 120000000,
		.linelength = 1250,
		.framelength = 3200,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 4208,
		.grabwindow_height = 3120,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 300,
		.mipi_pixel_rate = 547200000,
	},
	.hs_video = {
		.pclk = 120000000,
		.linelength = 1250,
		.framelength = 3200,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 4208,
		.grabwindow_height = 3120,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 300,
		.mipi_pixel_rate = 547200000,
	},
	.slim_video = {
		.pclk = 120000000,
		.linelength = 1250,
		.framelength = 3200,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 4208,
		.grabwindow_height = 3120,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 300,
		.mipi_pixel_rate = 547200000,
	},

	.margin = 8,
	.min_shutter = 8,
	.min_gain = 64,
	.max_gain = 1024,
	.min_gain_iso = 50,
	.gain_step = 1,
	.gain_type = 3,
	.max_frame_length = 0x7fffff,
	.ae_shut_delay_frame = 0,
	.ae_sensor_gain_delay_frame = 0,
	.ae_ispGain_delay_frame = 2,
	.ihdr_support = 0,
	.ihdr_le_firstline = 0,
	.sensor_mode_num = MODE_NUM,			/* support sensor mode num */

	.pre_delay_frame = 2,				/* enter preview delay frame num */
	.cap_delay_frame = 3,				/* enter capture delay frame num */
	.video_delay_frame = 2,				/* enter normal_video delay frame num */
	.hs_video_delay_frame = 2,			/* enter high_speed_video delay frame num */
	.slim_video_delay_frame = 2,			/* enter slim_video delay frame num */
	.frame_time_delay_frame = 2,

	.isp_driving_current = ISP_DRIVING_4MA,
	.sensor_interface_type = SENSOR_INTERFACE_TYPE_MIPI,
	.mipi_sensor_type = MIPI_OPHY_NCSI2,		/* 0,MIPI_OPHY_NCSI2;  1,MIPI_OPHY_CSI2 */
	.mipi_settle_delay_mode = 0,
	.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_B,
	.mclk = 24,
	.mipi_lane_num = SENSOR_MIPI_4_LANE,
	.i2c_addr_table = {0x20, 0xff},
	.i2c_speed = 1000,
};

static struct imgsensor_struct imgsensor = {
	.mirror = IMAGE_NORMAL,				/* mirrorflip information */
	.sensor_mode = IMGSENSOR_MODE_INIT,
	.shutter = 0x4de,
	.gain = 0x40,
	.dummy_pixel = 0,
	.dummy_line = 0,
	.current_fps = 300,
	.autoflicker_en = KAL_FALSE,
	.test_pattern = KAL_FALSE,
	.current_scenario_id = MSDK_SCENARIO_ID_CAMERA_PREVIEW,
	.i2c_write_id = 0x6C,
};

/* Sensor output window information */
static struct SENSOR_WINSIZE_INFO_STRUCT imgsensor_winsize_info[MODE_NUM] = {
	{4208, 3120, 0, 0, 4208, 3120, 2104, 1560, 0, 0, 2104, 1560, 0, 0, 2104, 1560}, /* Preview */
	{4208, 3120, 0, 0, 4208, 3120, 4208, 3120, 0, 0, 4208, 3120, 0, 0, 4208, 3120}, /* capture */
	{4208, 3120, 0, 0, 4208, 3120, 4208, 3120, 0, 0, 4208, 3120, 0, 0, 4208, 3120}, /* video */
	{4208, 3120, 0, 0, 4208, 3120, 4208, 3120, 0, 0, 4208, 3120, 0, 0, 4208, 3120}, /* hs video */
	{4208, 3120, 0, 0, 4208, 3120, 4208, 3120, 0, 0, 4208, 3120, 0, 0, 4208, 3120}, /* slim */
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
	char pu_send_cmd[3] = {(char)(addr >> 8), (char)(addr & 0xFF), (char)(para & 0xFF)};

	iWriteRegI2C(pu_send_cmd, 3, imgsensor.i2c_write_id);
}

static void set_dummy(void)
{
	LOG_INFO("Smartsens_SC1321CS frame length = %d", imgsensor.frame_length);
	write_cmos_sensor(0x320e, (imgsensor.frame_length >> 8) & 0xff);
	write_cmos_sensor(0x320f, imgsensor.frame_length & 0xff);
}

static kal_uint32 return_sensor_id(void)
{
	LOG_INFO("Smartsens_SC1321CS sensor_id 0x3107 =0x%x, 0x3108 = 0x%x \n",read_cmos_sensor(0x3107),read_cmos_sensor(0x3108));
	return ((read_cmos_sensor(0x3107) << 8) | read_cmos_sensor(0x3108));
}

static void set_max_framerate(UINT16 framerate, kal_bool min_framelength_en)
{
	kal_uint32 frame_length = imgsensor.frame_length;

	LOG_INFO("framerate = %d, min framelength enable: %d", framerate, min_framelength_en);
	frame_length = imgsensor.pclk / framerate * 10 / imgsensor.line_length;
	spin_lock(&imgsensor_drv_lock);
	imgsensor.frame_length = (frame_length > imgsensor.min_frame_length) ?
		frame_length : imgsensor.min_frame_length;
	imgsensor.dummy_line = imgsensor.frame_length - imgsensor.min_frame_length;

	if (imgsensor.frame_length > imgsensor_info.max_frame_length)
		imgsensor.frame_length = imgsensor_info.max_frame_length;
	imgsensor.dummy_line = imgsensor.frame_length - imgsensor.min_frame_length;
	if (min_framelength_en)
		imgsensor.min_frame_length = imgsensor.frame_length;
	spin_unlock(&imgsensor_drv_lock);
	set_dummy();
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
	kal_uint16 realtime_fps = 0;

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

	if (imgsensor.autoflicker_en) {
		realtime_fps = imgsensor.pclk / imgsensor.line_length * 10 / imgsensor.frame_length;
		if (realtime_fps >= 297 && realtime_fps <= 305)
			set_max_framerate(296, 0);
		else if (realtime_fps >= 147 && realtime_fps <= 150)
			set_max_framerate(146, 0);
		else {
			write_cmos_sensor(0x326d, (imgsensor.frame_length >> 16) & 0x7f);
			write_cmos_sensor(0x320e, (imgsensor.frame_length >> 8) & 0xff);
			write_cmos_sensor(0x320f, imgsensor.frame_length & 0xFF);
		}
	} else {
		/* Extend frame length */
		write_cmos_sensor(0x326d, (imgsensor.frame_length >> 16) & 0x7f);
		write_cmos_sensor(0x320e, (imgsensor.frame_length >> 8) & 0xff);
		write_cmos_sensor(0x320f, imgsensor.frame_length & 0xFF);
	}
	/* Update Shutter */
	shutter = shutter * 2;
	write_cmos_sensor(0x3e20, (shutter >> 20) & 0x0F);
	write_cmos_sensor(0x3e00, (shutter >> 12) & 0xFF);
	write_cmos_sensor(0x3e01, (shutter >>  4) & 0xFF);
	write_cmos_sensor(0x3e02, (shutter <<  4) & 0xF0);
	LOG_INFO("Smartsens_SC1321CS shutter = %d, framelength = %d", shutter, imgsensor.frame_length);
}

static kal_uint16 gain2reg(const kal_uint16 gain)
{
	kal_uint16 reg_gain = gain << 4;

	if (reg_gain < sc1321cs_SENSOR_BASE_GAIN)
		reg_gain = sc1321cs_SENSOR_BASE_GAIN;
	else if (reg_gain > sc1321cs_SENSOR_MAX_GAIN)
		reg_gain = sc1321cs_SENSOR_MAX_GAIN;

	return (kal_uint16)reg_gain;
}

/*
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
 */
static kal_uint16 set_gain(kal_uint16 gain)
{
	kal_uint16 reg_gain;
	kal_uint32 temp_gain;
	kal_int16 gain_index;
	kal_uint16 sc1321cs_LY_AGC_Param[sc1321cs_SENSOR_GAIN_MAP_SIZE][2] = {
		{  1024,  0x00 },
		{  2048,  0x08 },
		{  4096,  0x09 },
		{  8192,  0x0B },
		{ 16384,  0x0f },
	};

	reg_gain = gain2reg(gain);

	for (gain_index = sc1321cs_SENSOR_GAIN_MAX_VALID_INDEX - 1; gain_index >= 0; gain_index--)
		if (reg_gain >= sc1321cs_LY_AGC_Param[gain_index][0])
			break;

	if (gain_index < 0) {
		gain_index = 0;
		LOG_ERR("there is a error change gain_index = 0");
	}

	write_cmos_sensor(0x3e09, sc1321cs_LY_AGC_Param[gain_index][1]);
	temp_gain = reg_gain * sc1321cs_SENSOR_BASE_GAIN / sc1321cs_LY_AGC_Param[gain_index][0];
	write_cmos_sensor(0x3e07, (temp_gain >> 3) & 0xff);
	LOG_INFO("Smartsens_SC1321CS LY_AGC_Param[gain_index][1] = 0x%x, temp_gain = 0x%x, reg_gain = %d, gain = %d",
		sc1321cs_LY_AGC_Param[gain_index][1], temp_gain, reg_gain, gain);

	return reg_gain;
}

static void ihdr_write_shutter_gain(kal_uint16 le, kal_uint16 se, kal_uint16 gain)
{
	LOG_INFO("le: 0x%x, se: 0x%x, gain: 0x%x", le, se, gain);
}

/*
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
 */
static void night_mode(kal_bool enable)
{
	/* No Need to implement this function */
}

static void sensor_init(void)
{
/*
Sensor name:	SC1321CS
Resolution:	4208(H)x3120(V) @30fps, Linear, /
Port:	MCLK=24M, MIPI 4lane, RAW 10, 1368Mbps
 */
	LOG_INFO("%s()", __func__);
	write_cmos_sensor(0x0103,0x01);
	write_cmos_sensor(0x0100,0x00);
	write_cmos_sensor(0x36e9,0x80);
	write_cmos_sensor(0x37f9,0x80);
	write_cmos_sensor(0x36ea,0x13);
	write_cmos_sensor(0x36eb,0x0c);
	write_cmos_sensor(0x36ec,0x4b);
	write_cmos_sensor(0x36ed,0x18);
	write_cmos_sensor(0x36e9,0x23);
	write_cmos_sensor(0x37f9,0x24);
	write_cmos_sensor(0x301f,0x17);
	write_cmos_sensor(0x320c,0x04);
	write_cmos_sensor(0x320d,0xe2);
	write_cmos_sensor(0x320e,0x0c);
	write_cmos_sensor(0x320f,0x80);
	write_cmos_sensor(0x3279,0x10);
	write_cmos_sensor(0x3301,0x07);
	write_cmos_sensor(0x3302,0x18);
	write_cmos_sensor(0x3304,0x38);
	write_cmos_sensor(0x3306,0x58);
	write_cmos_sensor(0x3308,0x0c);
	write_cmos_sensor(0x3309,0x80);
	write_cmos_sensor(0x330b,0xb0);
	write_cmos_sensor(0x330d,0x10);
	write_cmos_sensor(0x330e,0x2b);
	write_cmos_sensor(0x3314,0x15);
	write_cmos_sensor(0x331e,0x29);
	write_cmos_sensor(0x331f,0x71);
	write_cmos_sensor(0x3333,0x10);
	write_cmos_sensor(0x3334,0x40);
	write_cmos_sensor(0x335d,0x60);
	write_cmos_sensor(0x3364,0x56);
	write_cmos_sensor(0x337f,0x13);
	write_cmos_sensor(0x3390,0x09);
	write_cmos_sensor(0x3391,0x0f);
	write_cmos_sensor(0x3393,0x10);
	write_cmos_sensor(0x3394,0x1c);
	write_cmos_sensor(0x33ad,0x14);
	write_cmos_sensor(0x33af,0x60);
	write_cmos_sensor(0x33b0,0x0f);
	write_cmos_sensor(0x33b3,0x10);
	write_cmos_sensor(0x349f,0x1e);
	write_cmos_sensor(0x34a6,0x08);
	write_cmos_sensor(0x34a7,0x09);
	write_cmos_sensor(0x34a8,0x18);
	write_cmos_sensor(0x34a9,0x18);
	write_cmos_sensor(0x34f8,0x0f);
	write_cmos_sensor(0x34f9,0x18);
	write_cmos_sensor(0x3630,0xcd);
	write_cmos_sensor(0x3637,0x45);
	write_cmos_sensor(0x363c,0x8d);
	write_cmos_sensor(0x3670,0x0c);
	write_cmos_sensor(0x367b,0x56);
	write_cmos_sensor(0x367c,0x66);
	write_cmos_sensor(0x367d,0x66);
	write_cmos_sensor(0x367e,0x09);
	write_cmos_sensor(0x367f,0x0f);
	write_cmos_sensor(0x3690,0x83);
	write_cmos_sensor(0x3691,0x89);
	write_cmos_sensor(0x3692,0x8c);
	write_cmos_sensor(0x3693,0x9c);
	write_cmos_sensor(0x3694,0x09);
	write_cmos_sensor(0x3695,0x0b);
	write_cmos_sensor(0x3696,0x0f);
	write_cmos_sensor(0x370f,0x01);
	write_cmos_sensor(0x3724,0x41);
	write_cmos_sensor(0x3771,0x03);
	write_cmos_sensor(0x3772,0x03);
	write_cmos_sensor(0x3773,0x63);
	write_cmos_sensor(0x377a,0x08);
	write_cmos_sensor(0x377b,0x0f);
	write_cmos_sensor(0x3903,0x20);
	write_cmos_sensor(0x3905,0x0c);
	write_cmos_sensor(0x3908,0x40);
	write_cmos_sensor(0x391f,0x41);
	write_cmos_sensor(0x393f,0x80);
	write_cmos_sensor(0x3940,0x00);
	write_cmos_sensor(0x3941,0x00);
	write_cmos_sensor(0x3942,0x00);
	write_cmos_sensor(0x3943,0x6a);
	write_cmos_sensor(0x3944,0x69);
	write_cmos_sensor(0x3945,0x6e);
	write_cmos_sensor(0x3946,0x68);
	write_cmos_sensor(0x3e00,0x01);
	write_cmos_sensor(0x3e01,0x8f);
	write_cmos_sensor(0x3e02,0x90);
	write_cmos_sensor(0x3f08,0x0a);
	write_cmos_sensor(0x4401,0x13);
	write_cmos_sensor(0x4402,0x03);
	write_cmos_sensor(0x4403,0x0c);
	write_cmos_sensor(0x4404,0x24);
	write_cmos_sensor(0x4405,0x2f);
	write_cmos_sensor(0x440c,0x3c);
	write_cmos_sensor(0x440d,0x3c);
	write_cmos_sensor(0x440e,0x2d);
	write_cmos_sensor(0x440f,0x4b);
	write_cmos_sensor(0x4413,0x01);
	write_cmos_sensor(0x441b,0x18);
	write_cmos_sensor(0x4509,0x20);
	write_cmos_sensor(0x4800,0x24);
	write_cmos_sensor(0x4837,0x17);
	write_cmos_sensor(0x5000,0x1e);
	write_cmos_sensor(0x5002,0x00);
	write_cmos_sensor(0x550e,0x00);
	write_cmos_sensor(0x550f,0x7a);
	write_cmos_sensor(0x5780,0x76);
	write_cmos_sensor(0x5784,0x10);
	write_cmos_sensor(0x5785,0x04);
	write_cmos_sensor(0x5787,0x0a);
	write_cmos_sensor(0x5788,0x0a);
	write_cmos_sensor(0x5789,0x08);
	write_cmos_sensor(0x578a,0x0a);
	write_cmos_sensor(0x578b,0x0a);
	write_cmos_sensor(0x578c,0x08);
	write_cmos_sensor(0x578d,0x40);
	write_cmos_sensor(0x5799,0x46);
	write_cmos_sensor(0x579a,0x77);
	write_cmos_sensor(0x57a1,0x04);
	write_cmos_sensor(0x57a8,0xd2);
	write_cmos_sensor(0x57aa,0x2a);
	write_cmos_sensor(0x57ab,0x7f);
	write_cmos_sensor(0x57ac,0x00);
	write_cmos_sensor(0x57ad,0x00);
	write_cmos_sensor(0x58e2,0x08);
	write_cmos_sensor(0x58e3,0x04);
	write_cmos_sensor(0x58e4,0x04);
	write_cmos_sensor(0x58e8,0x08);
	write_cmos_sensor(0x58e9,0x04);
	write_cmos_sensor(0x58ea,0x04);
LOG_INFO("Smartsens_SC1321CS initial setting  0X301f = 0x%x \n", read_cmos_sensor(0x301F));
}

static void preview_setting(void)
{
	
	// 	Sensor name:	SC1321CS
    //	Resolution:	2104(H)x1560(V) @30fps, Linear, /
	//	Port:	MCLK=24M, MIPI 4lane, RAW 10, 1368Mbps

	write_cmos_sensor(0x0103,0x01);
	write_cmos_sensor(0x0100,0x00);
	write_cmos_sensor(0x36e9,0x80);
	write_cmos_sensor(0x37f9,0x80);
	write_cmos_sensor(0x36ea,0x13);
	write_cmos_sensor(0x36eb,0x0c);
	write_cmos_sensor(0x36ec,0x5b);
	write_cmos_sensor(0x36ed,0x08);
	write_cmos_sensor(0x36e9,0x27);
	write_cmos_sensor(0x37f9,0x24);
	write_cmos_sensor(0x301f,0x1e);
	write_cmos_sensor(0x3208,0x08);
	write_cmos_sensor(0x3209,0x38);
	write_cmos_sensor(0x320a,0x06);
	write_cmos_sensor(0x320b,0x18);
	write_cmos_sensor(0x320c,0x04);
	write_cmos_sensor(0x320d,0xe2);
	write_cmos_sensor(0x320e,0x0c);
	write_cmos_sensor(0x320f,0x80);
	write_cmos_sensor(0x3211,0x04);
	write_cmos_sensor(0x3213,0x04);
	write_cmos_sensor(0x3215,0x31);
	write_cmos_sensor(0x3220,0x01);
	write_cmos_sensor(0x3279,0x10);
	write_cmos_sensor(0x3301,0x07);
	write_cmos_sensor(0x3302,0x18);
	write_cmos_sensor(0x3304,0x38);
	write_cmos_sensor(0x3306,0x58);
	write_cmos_sensor(0x3308,0x0c);
	write_cmos_sensor(0x3309,0x80);
	write_cmos_sensor(0x330b,0xb0);
	write_cmos_sensor(0x330d,0x10);
	write_cmos_sensor(0x330e,0x2b);
	write_cmos_sensor(0x3314,0x15);
	write_cmos_sensor(0x331e,0x29);
	write_cmos_sensor(0x331f,0x71);
	write_cmos_sensor(0x3333,0x10);
	write_cmos_sensor(0x3334,0x40);
	write_cmos_sensor(0x335d,0x60);
	write_cmos_sensor(0x3364,0x56);
	write_cmos_sensor(0x337f,0x13);
	write_cmos_sensor(0x3390,0x09);
	write_cmos_sensor(0x3391,0x0f);
	write_cmos_sensor(0x3393,0x10);
	write_cmos_sensor(0x3394,0x1c);
	write_cmos_sensor(0x33ad,0x14);
	write_cmos_sensor(0x33af,0x60);
	write_cmos_sensor(0x33b0,0x0f);
	write_cmos_sensor(0x33b3,0x10);
	write_cmos_sensor(0x349f,0x1e);
	write_cmos_sensor(0x34a6,0x08);
	write_cmos_sensor(0x34a7,0x09);
	write_cmos_sensor(0x34a8,0x18);
	write_cmos_sensor(0x34a9,0x18);
	write_cmos_sensor(0x34f8,0x0f);
	write_cmos_sensor(0x34f9,0x18);
	write_cmos_sensor(0x3630,0xcd);
	write_cmos_sensor(0x3637,0x45);
	write_cmos_sensor(0x363c,0x8d);
	write_cmos_sensor(0x3670,0x0c);
	write_cmos_sensor(0x367b,0x56);
	write_cmos_sensor(0x367c,0x66);
	write_cmos_sensor(0x367d,0x66);
	write_cmos_sensor(0x367e,0x09);
	write_cmos_sensor(0x367f,0x0f);
	write_cmos_sensor(0x3690,0x83);
	write_cmos_sensor(0x3691,0x89);
	write_cmos_sensor(0x3692,0x8c);
	write_cmos_sensor(0x3693,0x9c);
	write_cmos_sensor(0x3694,0x09);
	write_cmos_sensor(0x3695,0x0b);
	write_cmos_sensor(0x3696,0x0f);
	write_cmos_sensor(0x370f,0x01);
	write_cmos_sensor(0x3724,0x41);
	write_cmos_sensor(0x3771,0x03);
	write_cmos_sensor(0x3772,0x03);
	write_cmos_sensor(0x3773,0x63);
	write_cmos_sensor(0x377a,0x08);
	write_cmos_sensor(0x377b,0x0f);
	write_cmos_sensor(0x3903,0x20);
	write_cmos_sensor(0x3905,0x0c);
	write_cmos_sensor(0x3908,0x40);
	write_cmos_sensor(0x391f,0x41);
	write_cmos_sensor(0x393f,0x80);
	write_cmos_sensor(0x3940,0x00);
	write_cmos_sensor(0x3941,0x00);
	write_cmos_sensor(0x3942,0x00);
	write_cmos_sensor(0x3943,0x6a);
	write_cmos_sensor(0x3944,0x69);
	write_cmos_sensor(0x3945,0x6e);
	write_cmos_sensor(0x3946,0x68);
	write_cmos_sensor(0x3e00,0x01);
	write_cmos_sensor(0x3e01,0x8f);
	write_cmos_sensor(0x3e02,0x90);
	write_cmos_sensor(0x3f08,0x0a);
	write_cmos_sensor(0x4401,0x13);
	write_cmos_sensor(0x4402,0x03);
	write_cmos_sensor(0x4403,0x0c);
	write_cmos_sensor(0x4404,0x24);
	write_cmos_sensor(0x4405,0x2f);
	write_cmos_sensor(0x440c,0x3c);
	write_cmos_sensor(0x440d,0x3c);
	write_cmos_sensor(0x440e,0x2d);
	write_cmos_sensor(0x440f,0x4b);
	write_cmos_sensor(0x4413,0x01);
	write_cmos_sensor(0x441b,0x18);
	write_cmos_sensor(0x4509,0x20);
	write_cmos_sensor(0x4800,0x24);
	write_cmos_sensor(0x4837,0x17);
	write_cmos_sensor(0x5000,0x5e);
	write_cmos_sensor(0x5002,0x00);
	write_cmos_sensor(0x550e,0x00);
	write_cmos_sensor(0x550f,0x7a);
	write_cmos_sensor(0x5780,0x76);
	write_cmos_sensor(0x5784,0x10);
	write_cmos_sensor(0x5785,0x04);
	write_cmos_sensor(0x5787,0x0a);
	write_cmos_sensor(0x5788,0x0a);
	write_cmos_sensor(0x5789,0x08);
	write_cmos_sensor(0x578a,0x0a);
	write_cmos_sensor(0x578b,0x0a);
	write_cmos_sensor(0x578c,0x08);
	write_cmos_sensor(0x578d,0x40);
	write_cmos_sensor(0x5799,0x46);
	write_cmos_sensor(0x579a,0x77);
	write_cmos_sensor(0x57a1,0x04);
	write_cmos_sensor(0x57a8,0xd2);
	write_cmos_sensor(0x57aa,0x2a);
	write_cmos_sensor(0x57ab,0x7f);
	write_cmos_sensor(0x57ac,0x00);
	write_cmos_sensor(0x57ad,0x00);
	write_cmos_sensor(0x58e2,0x08);
	write_cmos_sensor(0x58e3,0x04);
	write_cmos_sensor(0x58e4,0x04);
	write_cmos_sensor(0x58e8,0x08);
	write_cmos_sensor(0x58e9,0x04);
	write_cmos_sensor(0x58ea,0x04);
	write_cmos_sensor(0x5900,0x01);
	write_cmos_sensor(0x5901,0x04);
	LOG_INFO("Smartsens_SC1321CS initial setting  0X301f = 0x%x \n", read_cmos_sensor(0x301F));
}

static void capture_setting(void)
{	
	/*
Sensor name:	SC1321CS
Resolution:	4208(H)x3120(V) @30fps, Linear, /
Port:	MCLK=24M, MIPI 4lane, RAW 10, 1368Mbps
 */
	write_cmos_sensor(0x0103,0x01);
	write_cmos_sensor(0x0100,0x00);
	write_cmos_sensor(0x36e9,0x80);
	write_cmos_sensor(0x37f9,0x80);
	write_cmos_sensor(0x36ea,0x13);
	write_cmos_sensor(0x36eb,0x0c);
	write_cmos_sensor(0x36ec,0x4b);
	write_cmos_sensor(0x36ed,0x18);
	write_cmos_sensor(0x36e9,0x23);
	write_cmos_sensor(0x37f9,0x24);
	write_cmos_sensor(0x301f,0x17);
	write_cmos_sensor(0x320c,0x04);
	write_cmos_sensor(0x320d,0xe2);
	write_cmos_sensor(0x320e,0x0c);
	write_cmos_sensor(0x320f,0x80);
	write_cmos_sensor(0x3279,0x10);
	write_cmos_sensor(0x3301,0x07);
	write_cmos_sensor(0x3302,0x18);
	write_cmos_sensor(0x3304,0x38);
	write_cmos_sensor(0x3306,0x58);
	write_cmos_sensor(0x3308,0x0c);
	write_cmos_sensor(0x3309,0x80);
	write_cmos_sensor(0x330b,0xb0);
	write_cmos_sensor(0x330d,0x10);
	write_cmos_sensor(0x330e,0x2b);
	write_cmos_sensor(0x3314,0x15);
	write_cmos_sensor(0x331e,0x29);
	write_cmos_sensor(0x331f,0x71);
	write_cmos_sensor(0x3333,0x10);
	write_cmos_sensor(0x3334,0x40);
	write_cmos_sensor(0x335d,0x60);
	write_cmos_sensor(0x3364,0x56);
	write_cmos_sensor(0x337f,0x13);
	write_cmos_sensor(0x3390,0x09);
	write_cmos_sensor(0x3391,0x0f);
	write_cmos_sensor(0x3393,0x10);
	write_cmos_sensor(0x3394,0x1c);
	write_cmos_sensor(0x33ad,0x14);
	write_cmos_sensor(0x33af,0x60);
	write_cmos_sensor(0x33b0,0x0f);
	write_cmos_sensor(0x33b3,0x10);
	write_cmos_sensor(0x349f,0x1e);
	write_cmos_sensor(0x34a6,0x08);
	write_cmos_sensor(0x34a7,0x09);
	write_cmos_sensor(0x34a8,0x18);
	write_cmos_sensor(0x34a9,0x18);
	write_cmos_sensor(0x34f8,0x0f);
	write_cmos_sensor(0x34f9,0x18);
	write_cmos_sensor(0x3630,0xcd);
	write_cmos_sensor(0x3637,0x45);
	write_cmos_sensor(0x363c,0x8d);
	write_cmos_sensor(0x3670,0x0c);
	write_cmos_sensor(0x367b,0x56);
	write_cmos_sensor(0x367c,0x66);
	write_cmos_sensor(0x367d,0x66);
	write_cmos_sensor(0x367e,0x09);
	write_cmos_sensor(0x367f,0x0f);
	write_cmos_sensor(0x3690,0x83);
	write_cmos_sensor(0x3691,0x89);
	write_cmos_sensor(0x3692,0x8c);
	write_cmos_sensor(0x3693,0x9c);
	write_cmos_sensor(0x3694,0x09);
	write_cmos_sensor(0x3695,0x0b);
	write_cmos_sensor(0x3696,0x0f);
	write_cmos_sensor(0x370f,0x01);
	write_cmos_sensor(0x3724,0x41);
	write_cmos_sensor(0x3771,0x03);
	write_cmos_sensor(0x3772,0x03);
	write_cmos_sensor(0x3773,0x63);
	write_cmos_sensor(0x377a,0x08);
	write_cmos_sensor(0x377b,0x0f);
	write_cmos_sensor(0x3903,0x20);
	write_cmos_sensor(0x3905,0x0c);
	write_cmos_sensor(0x3908,0x40);
	write_cmos_sensor(0x391f,0x41);
	write_cmos_sensor(0x393f,0x80);
	write_cmos_sensor(0x3940,0x00);
	write_cmos_sensor(0x3941,0x00);
	write_cmos_sensor(0x3942,0x00);
	write_cmos_sensor(0x3943,0x6a);
	write_cmos_sensor(0x3944,0x69);
	write_cmos_sensor(0x3945,0x6e);
	write_cmos_sensor(0x3946,0x68);
	write_cmos_sensor(0x3e00,0x01);
	write_cmos_sensor(0x3e01,0x8f);
	write_cmos_sensor(0x3e02,0x90);
	write_cmos_sensor(0x3f08,0x0a);
	write_cmos_sensor(0x4401,0x13);
	write_cmos_sensor(0x4402,0x03);
	write_cmos_sensor(0x4403,0x0c);
	write_cmos_sensor(0x4404,0x24);
	write_cmos_sensor(0x4405,0x2f);
	write_cmos_sensor(0x440c,0x3c);
	write_cmos_sensor(0x440d,0x3c);
	write_cmos_sensor(0x440e,0x2d);
	write_cmos_sensor(0x440f,0x4b);
	write_cmos_sensor(0x4413,0x01);
	write_cmos_sensor(0x441b,0x18);
	write_cmos_sensor(0x4509,0x20);
	write_cmos_sensor(0x4800,0x24);
	write_cmos_sensor(0x4837,0x17);
	write_cmos_sensor(0x5000,0x1e);
	write_cmos_sensor(0x5002,0x00);
	write_cmos_sensor(0x550e,0x00);
	write_cmos_sensor(0x550f,0x7a);
	write_cmos_sensor(0x5780,0x76);
	write_cmos_sensor(0x5784,0x10);
	write_cmos_sensor(0x5785,0x04);
	write_cmos_sensor(0x5787,0x0a);
	write_cmos_sensor(0x5788,0x0a);
	write_cmos_sensor(0x5789,0x08);
	write_cmos_sensor(0x578a,0x0a);
	write_cmos_sensor(0x578b,0x0a);
	write_cmos_sensor(0x578c,0x08);
	write_cmos_sensor(0x578d,0x40);
	write_cmos_sensor(0x5799,0x46);
	write_cmos_sensor(0x579a,0x77);
	write_cmos_sensor(0x57a1,0x04);
	write_cmos_sensor(0x57a8,0xd2);
	write_cmos_sensor(0x57aa,0x2a);
	write_cmos_sensor(0x57ab,0x7f);
	write_cmos_sensor(0x57ac,0x00);
	write_cmos_sensor(0x57ad,0x00);
	write_cmos_sensor(0x58e2,0x08);
	write_cmos_sensor(0x58e3,0x04);
	write_cmos_sensor(0x58e4,0x04);
	write_cmos_sensor(0x58e8,0x08);
	write_cmos_sensor(0x58e9,0x04);
	write_cmos_sensor(0x58ea,0x04);
}

static void normal_video_setting(void)
{
	capture_setting();
}

static void hs_video_setting(void)
{
	capture_setting();
}

static void slim_video_setting(void)
{
	capture_setting();
}

static kal_uint32 set_test_pattern_mode(kal_bool enable)
{
	LOG_INFO("test pattern enable: %d", enable);

	if (enable) {
		write_cmos_sensor(0x4501, 0xbc);
	LOG_INFO("Smartsens_SC1321CS test_pattern enable");
	} else {
		write_cmos_sensor(0x4501, 0xb4);
	LOG_INFO("Smartsens_SC1321CS test_pattern disable");
	}
	spin_lock(&imgsensor_drv_lock);
	imgsensor.test_pattern = enable;
	spin_unlock(&imgsensor_drv_lock);

	return ERROR_NONE;
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
				//+w3 camera bring up --otp porting.
#if OTP_PORTING
				imgSensorReadEepromData(&w3sc1321csfrontlce_eeprom_data,w3sc1321csfrontlce_checksum);
				if((w3sc1321csfrontlce_eeprom_data.sensorVendorid >> 24)!= read_eeprom(0x0001)) {
					pr_err("cwd w3sc1321csfrontlce get eeprom data failed\n");
					if(w3sc1321csfrontlce_eeprom_data.dataBuffer != NULL) {
						kfree(w3sc1321csfrontlce_eeprom_data.dataBuffer);
						w3sc1321csfrontlce_eeprom_data.dataBuffer = NULL;
					}
					//*sensor_id = 0xFFFFFFFF;
					//return ERROR_SENSOR_CONNECT_FAIL;
				} else {
					LOG_ERR("cwd w3sc1321csfrontlce get eeprom data success\n");
				}
#endif
				//-w3 camera bring up --otp porting.
                LOG_INFO("i2c write id  : 0x%x, sensor id: 0x%x",
					 imgsensor.i2c_write_id, *sensor_id);
				return ERROR_NONE;
			}
			LOG_ERR("Read sensor id fail, write id: 0x%x,sensor id: 0x%x\n",
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

/*
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
 */
static kal_uint32 open(void)
{
	kal_uint8 i = 0;
	kal_uint8 retry = 2;
	kal_uint32 sensor_id = 0;

	LOG_INFO("preview 1632*1224@30fps,360Mbps/lane; capture 3264*2448@30fps,880Mbps/lane");
	while (imgsensor_info.i2c_addr_table[i] != 0xff) {
		spin_lock(&imgsensor_drv_lock);
		imgsensor.i2c_write_id = imgsensor_info.i2c_addr_table[i];
		spin_unlock(&imgsensor_drv_lock);
		do {
			sensor_id = return_sensor_id();
			if (sensor_id == imgsensor_info.sensor_id) {
				LOG_INFO("i2c write id: 0x%x, sensor id: 0x%x",
					 imgsensor.i2c_write_id, sensor_id);
				break;
			}
			LOG_ERR("open:Read sensor id fail open i2c write id: 0x%x, id: 0x%x",
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
}

static kal_uint32 close(void)
{
	LOG_INFO("close E");

	return ERROR_NONE;
}

/*
 * FUNCTION
 *	preview
 *
 * DESCRIPTION
 *	This function start the sensor preview.
 *
 * PARAMETERS
 *	*image_window : address pointer of pixel numbers in one period of HSYNC
 *	*sensor_config_data : address pointer of line numbers in one period of VSYNC
 *
 * RETURNS
 *	None
 *
 * GLOBALS AFFECTED
 *
 */
static kal_uint32 preview(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
					  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INFO("Enter %s", __func__);
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
	LOG_INFO("Enter %s", __func__);
	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_CAPTURE;
	if (imgsensor.current_fps == imgsensor_info.cap1.max_framerate) {
		imgsensor.pclk = imgsensor_info.cap1.pclk;
		imgsensor.line_length = imgsensor_info.cap1.linelength;
		imgsensor.frame_length = imgsensor_info.cap1.framelength;
		imgsensor.min_frame_length = imgsensor_info.cap1.framelength;
		imgsensor.autoflicker_en = KAL_FALSE;
	} else {
		if (imgsensor.current_fps != imgsensor_info.cap.max_framerate)
			LOG_ERR("current_fps %d fps is not support, so use cap's setting: %d fps",
				 imgsensor.current_fps, imgsensor_info.cap.max_framerate / 10);
		imgsensor.pclk = imgsensor_info.cap.pclk;
		imgsensor.line_length = imgsensor_info.cap.linelength;
		imgsensor.frame_length = imgsensor_info.cap.framelength;
		imgsensor.min_frame_length = imgsensor_info.cap.framelength;
		imgsensor.autoflicker_en = KAL_FALSE;
	}
	spin_unlock(&imgsensor_drv_lock);
	capture_setting();

	return ERROR_NONE;
}

static kal_uint32 normal_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
				MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INFO("Enter %s", __func__);
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
}

static kal_uint32 hs_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
				MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INFO("Enter %s", __func__);
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
	hs_video_setting();

	return ERROR_NONE;
}

static kal_uint32 slim_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INFO("Enter %s", __func__);
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
}

static kal_uint32 get_resolution(MSDK_SENSOR_RESOLUTION_INFO_STRUCT *sensor_resolution)
{
	LOG_INFO("Enter %s", __func__);
	sensor_resolution->SensorFullWidth = imgsensor_info.cap.grabwindow_width;
	sensor_resolution->SensorFullHeight = imgsensor_info.cap.grabwindow_height;
	sensor_resolution->SensorPreviewWidth = imgsensor_info.pre.grabwindow_width;
	sensor_resolution->SensorPreviewHeight = imgsensor_info.pre.grabwindow_height;
	sensor_resolution->SensorVideoWidth = imgsensor_info.normal_video.grabwindow_width;
	sensor_resolution->SensorVideoHeight = imgsensor_info.normal_video.grabwindow_height;
	sensor_resolution->SensorHighSpeedVideoWidth = imgsensor_info.hs_video.grabwindow_width;
	sensor_resolution->SensorHighSpeedVideoHeight = imgsensor_info.hs_video.grabwindow_height;
	sensor_resolution->SensorSlimVideoWidth = imgsensor_info.slim_video.grabwindow_width;
	sensor_resolution->SensorSlimVideoHeight = imgsensor_info.slim_video.grabwindow_height;

	return ERROR_NONE;
}

static kal_uint32 get_info(enum MSDK_SCENARIO_ID_ENUM scenario_id,
			  MSDK_SENSOR_INFO_STRUCT *sensor_info,
			  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INFO("scenario_id = %d", scenario_id);
	sensor_info->SensorClockPolarity = SENSOR_CLOCK_POLARITY_LOW;
	sensor_info->SensorClockFallingPolarity = SENSOR_CLOCK_POLARITY_LOW;
	sensor_info->SensorHsyncPolarity = SENSOR_CLOCK_POLARITY_LOW;
	sensor_info->SensorVsyncPolarity = SENSOR_CLOCK_POLARITY_LOW;
	sensor_info->SensorInterruptDelayLines = 4;
	sensor_info->SensorResetActiveHigh = FALSE;
	sensor_info->SensorResetDelayCount = 5;
	sensor_info->SensroInterfaceType = imgsensor_info.sensor_interface_type;
	sensor_info->MIPIsensorType = imgsensor_info.mipi_sensor_type;
	sensor_info->SettleDelayMode = imgsensor_info.mipi_settle_delay_mode;
	sensor_info->SensorOutputDataFormat = imgsensor_info.sensor_output_dataformat;
	sensor_info->CaptureDelayFrame = imgsensor_info.cap_delay_frame;
	sensor_info->PreviewDelayFrame = imgsensor_info.pre_delay_frame;
	sensor_info->VideoDelayFrame = imgsensor_info.video_delay_frame;
	sensor_info->HighSpeedVideoDelayFrame = imgsensor_info.hs_video_delay_frame;
	sensor_info->SlimVideoDelayFrame = imgsensor_info.slim_video_delay_frame;
	sensor_info->SensorMasterClockSwitch = 0;
	sensor_info->SensorDrivingCurrent = imgsensor_info.isp_driving_current;
	sensor_info->AEShutDelayFrame = imgsensor_info.ae_shut_delay_frame;
	sensor_info->AESensorGainDelayFrame = imgsensor_info.ae_sensor_gain_delay_frame;
	sensor_info->AEISPGainDelayFrame = imgsensor_info.ae_ispGain_delay_frame;
	sensor_info->IHDR_Support = imgsensor_info.ihdr_support;
	sensor_info->IHDR_LE_FirstLine = imgsensor_info.ihdr_le_firstline;
	sensor_info->SensorModeNum = imgsensor_info.sensor_mode_num;
	sensor_info->SensorMIPILaneNumber = imgsensor_info.mipi_lane_num;
	sensor_info->SensorClockFreq = imgsensor_info.mclk;
	sensor_info->SensorClockDividCount = 3;
	sensor_info->SensorClockRisingCount = 0;
	sensor_info->SensorClockFallingCount = 2;
	sensor_info->SensorPixelClockCount = 3;
	sensor_info->SensorDataLatchCount = 2;
	sensor_info->MIPIDataLowPwr2HighSpeedTermDelayCount = 0;
	sensor_info->MIPICLKLowPwr2HighSpeedTermDelayCount = 0;
	sensor_info->SensorWidthSampling = 0;
	sensor_info->SensorHightSampling = 0;
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
}


static kal_uint32 control(enum MSDK_SCENARIO_ID_ENUM scenario_id,
			  MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INFO("scenario_id = %d", scenario_id);
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
		LOG_ERR("Error ScenarioId setting");
		preview(image_window, sensor_config_data);
		return ERROR_INVALID_SCENARIO_ID;
	}

	return ERROR_NONE;
}

static kal_uint32 set_video_mode(UINT16 framerate)
{
	LOG_INFO("framerate = %d ", framerate);
	if (framerate == 0)
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

static kal_uint32 set_auto_flicker_mode(kal_bool enable, UINT16 framerate)
{
	LOG_INFO("enable = %d, framerate = %d ", enable, framerate);
	spin_lock(&imgsensor_drv_lock);
	if (enable)
		imgsensor.autoflicker_en = KAL_TRUE;
	else
		imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);

	return ERROR_NONE;
}

static kal_uint32 set_max_framerate_by_scenario(enum MSDK_SCENARIO_ID_ENUM scenario_id,
						MUINT32 framerate)
{
	kal_uint32 frame_length;

	LOG_INFO("scenario_id = %d, framerate = %d", scenario_id, framerate);
	switch (scenario_id) {
	case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		frame_length = imgsensor_info.pre.pclk / framerate * 10 /
			imgsensor_info.pre.linelength;
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
		frame_length = imgsensor_info.normal_video.pclk / framerate * 10 /
			imgsensor_info.normal_video.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line = (frame_length > imgsensor_info.normal_video.framelength) ?
			(frame_length - imgsensor_info.normal_video.framelength) : 0;
		imgsensor.frame_length = imgsensor_info.normal_video.framelength +
			imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
		break;
	case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		if (imgsensor.current_fps == imgsensor_info.cap1.max_framerate) {
			frame_length = imgsensor_info.cap1.pclk / framerate * 10 /
					imgsensor_info.cap1.linelength;
			spin_lock(&imgsensor_drv_lock);
			imgsensor.dummy_line = (frame_length > imgsensor_info.cap1.framelength) ?
				(frame_length - imgsensor_info.cap1.framelength) : 0;
			imgsensor.frame_length = imgsensor_info.cap1.framelength +
				imgsensor.dummy_line;
			imgsensor.min_frame_length = imgsensor.frame_length;
			spin_unlock(&imgsensor_drv_lock);
		} else {
			if (imgsensor.current_fps != imgsensor_info.cap.max_framerate)
				LOG_ERR("current_fps %d fps not support, use cap's: %d fps",
					 framerate, imgsensor_info.cap.max_framerate/10);
			frame_length = imgsensor_info.cap.pclk / framerate * 10 /
				imgsensor_info.cap.linelength;
			spin_lock(&imgsensor_drv_lock);
			imgsensor.dummy_line = (frame_length > imgsensor_info.cap.framelength) ?
				(frame_length - imgsensor_info.cap.framelength) : 0;
			imgsensor.frame_length = imgsensor_info.cap.framelength +
				imgsensor.dummy_line;
			imgsensor.min_frame_length = imgsensor.frame_length;
			spin_unlock(&imgsensor_drv_lock);
		}
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
		break;
	case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
		frame_length = imgsensor_info.hs_video.pclk / framerate * 10 /
			imgsensor_info.hs_video.linelength;
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
		frame_length = imgsensor_info.slim_video.pclk / framerate * 10 /
			imgsensor_info.slim_video.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line = (frame_length > imgsensor_info.slim_video.framelength) ?
			(frame_length - imgsensor_info.slim_video.framelength) : 0;
		imgsensor.frame_length = imgsensor_info.slim_video.framelength +
			imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
		break;
	default:
		frame_length = imgsensor_info.pre.pclk / framerate * 10 /
			imgsensor_info.pre.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line = (frame_length > imgsensor_info.pre.framelength) ?
			(frame_length - imgsensor_info.pre.framelength) : 0;
		imgsensor.frame_length = imgsensor_info.pre.framelength + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
		LOG_ERR("error scenario_id = %d, we use preview scenario", scenario_id);
		break;
	}

	return ERROR_NONE;
}

static kal_uint32 get_default_framerate_by_scenario(enum MSDK_SCENARIO_ID_ENUM scenario_id,
						    MUINT32 *framerate)
{
	LOG_INFO("scenario_id = %d", scenario_id);
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

static kal_uint32 streaming_control(kal_bool enable)
{
	LOG_INFO("streaming %s", enable ? "enabled" : "disabled");
	if (enable) {
		write_cmos_sensor(0x0100, 0x01);
		mdelay(10);
		write_cmos_sensor(0x302d, 0x00);
	} else {
		write_cmos_sensor(0x0100, 0x00);
	}
	mdelay(10);

	return ERROR_NONE;
}

static kal_uint32 feature_control(MSDK_SENSOR_FEATURE_ENUM feature_id,
				 UINT8 *feature_para, UINT32 *feature_para_len)
{
	kal_uint32 rate;
	UINT16 *feature_return_para_16 = (UINT16 *) feature_para;
	UINT16 *feature_data_16 = (UINT16 *) feature_para;
	UINT32 *feature_return_para_32 = (UINT32 *) feature_para;
	UINT32 *feature_data_32 = (UINT32 *) feature_para;
	unsigned long long *feature_data = (unsigned long long *) feature_para;
	struct SENSOR_WINSIZE_INFO_STRUCT *wininfo;
	MSDK_SENSOR_REG_INFO_STRUCT *sensor_reg_data = (MSDK_SENSOR_REG_INFO_STRUCT *) feature_para;

	LOG_INFO("feature_id = %d", feature_id);
	switch (feature_id) {
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
		switch (*feature_data) {
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
		case MSDK_SCENARIO_ID_CUSTOM1:
			*(feature_data + 2) = 2;
			break;
		default:
			*(feature_data + 2) = 1;
			break;
		}
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
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		default:
			rate = imgsensor_info.pre.mipi_pixel_rate;
			break;
		}
		*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) = rate;
		break;
	case SENSOR_FEATURE_SET_ESHUTTER:
		set_shutter((kal_uint16)(*feature_data));
		break;
	case SENSOR_FEATURE_SET_NIGHTMODE:
		night_mode((BOOL)(*feature_data));
		break;
	case SENSOR_FEATURE_SET_GAIN:
		set_gain((UINT16)(*feature_data));
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
		LOG_INFO("adb_i2c_read 0x%x = 0x%x", sensor_reg_data->RegAddr,
			sensor_reg_data->RegData);
		break;
	case SENSOR_FEATURE_GET_LENS_DRIVER_ID:
		*feature_return_para_32 = LENS_DRIVER_ID_DO_NOT_CARE;
		*feature_para_len = 4;
		break;
	case SENSOR_FEATURE_SET_VIDEO_MODE:
		set_video_mode((UINT16)(*feature_data));
		break;
	case SENSOR_FEATURE_CHECK_SENSOR_ID:
		get_imgsensor_id(feature_return_para_32);
		break;
	case SENSOR_FEATURE_SET_AUTO_FLICKER_MODE:
		set_auto_flicker_mode((BOOL)(*feature_data_16), (UINT16)(*(feature_data_16 + 1)));
		break;
	case SENSOR_FEATURE_SET_MAX_FRAME_RATE_BY_SCENARIO:
		set_max_framerate_by_scenario((enum MSDK_SCENARIO_ID_ENUM)(*feature_data),
					      (MUINT32)(*(feature_data + 1)));
		break;
	case SENSOR_FEATURE_GET_DEFAULT_FRAME_RATE_BY_SCENARIO:
		get_default_framerate_by_scenario((enum MSDK_SCENARIO_ID_ENUM)(*feature_data),
						  (MUINT32 *)(uintptr_t)(*(feature_data + 1)));
		break;
	case SENSOR_FEATURE_SET_TEST_PATTERN:
		set_test_pattern_mode((BOOL)(*feature_data));
		break;
	case SENSOR_FEATURE_GET_TEST_PATTERN_CHECKSUM_VALUE:
		*feature_return_para_32 = imgsensor_info.checksum_value;
		*feature_para_len = 4;
		break;
	case SENSOR_FEATURE_SET_FRAMERATE:
		LOG_INFO("current fps: %d", (UINT32)(*feature_data));
		spin_lock(&imgsensor_drv_lock);
		imgsensor.current_fps = (kal_uint16)(*feature_data);
		spin_unlock(&imgsensor_drv_lock);
		break;
	case SENSOR_FEATURE_SET_HDR:
		LOG_INFO("ihdr enable: %d", (BOOL)(*feature_data));
		spin_lock(&imgsensor_drv_lock);
		imgsensor.ihdr_en = (BOOL)(*feature_data);
		spin_unlock(&imgsensor_drv_lock);
		break;
	case SENSOR_FEATURE_GET_CROP_INFO:
		LOG_INFO("SENSOR_FEATURE_GET_CROP_INFO scenarioId: %d", (UINT32)(*feature_data));
		wininfo = (struct SENSOR_WINSIZE_INFO_STRUCT *)(uintptr_t)(*(feature_data + 1));
		switch (*feature_data_32) {
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			memcpy((void *)wininfo, (void *)&imgsensor_winsize_info[1],
				sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			memcpy((void *)wininfo, (void *)&imgsensor_winsize_info[2],
				sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
			memcpy((void *)wininfo, (void *)&imgsensor_winsize_info[3],
				sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
			memcpy((void *)wininfo, (void *)&imgsensor_winsize_info[4],
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
		LOG_INFO("SENSOR_SET_SENSOR_IHDR LE = %d, SE = %d, Gain = %d",
			(UINT16)(*feature_data), (UINT16)(*(feature_data + 1)),
			(UINT16)(*(feature_data + 2)));
		ihdr_write_shutter_gain((UINT16)*feature_data,
			(UINT16)(*(feature_data + 1)), (UINT16)(*(feature_data + 2)));
		break;
	case SENSOR_FEATURE_SET_STREAMING_SUSPEND:
		LOG_INFO("SENSOR_FEATURE_SET_STREAMING_SUSPEND");
		streaming_control(KAL_FALSE);
		break;
	case SENSOR_FEATURE_SET_STREAMING_RESUME:
		LOG_INFO("SENSOR_FEATURE_SET_STREAMING_RESUME, shutter:%hu",
			 (kal_uint16)(*feature_data));
		if ((*feature_data) != 0)
			set_shutter((kal_uint16)(*feature_data));
		streaming_control(KAL_TRUE);
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

UINT32 W3SC1321CSFRONTLCE_MIPI_RAW_SensorInit(struct SENSOR_FUNCTION_STRUCT **pfFunc)
{
	if (pfFunc != NULL)
		*pfFunc = &sensor_func;

	return ERROR_NONE;
}
