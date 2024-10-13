// SPDX-License-Identifier: GPL-2.0
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
#include <linux/slab.h>

#include "kd_camera_typedef.h"
#include "kd_imgsensor.h"
#include "kd_imgsensor_define.h"
#include "kd_imgsensor_errcode.h"

#include "kd_imgsensor_sysfs_adapter.h"
#include "imgsensor_sysfs.h"

#include "kd_imgsensor_adaptive_mipi.h"

#include "hi5022qmipiraw_Sensor.h"
#include "hi5022qmipiraw_setfile.h"

#ifndef HI5022Q_DISABLE_ADAPTIVE_MIPI
#include "hi5022qmipiraw_adaptive_mipi.h"
#endif

#define TRANSFER_UNIT (4)
#define MULTI_WRITE 1
#if MULTI_WRITE
#define I2C_BUFFER_LEN (TRANSFER_UNIT * 255) /* trans# max is 255, (TRANSFER_UNIT) bytes */
#else
#define I2C_BUFFER_LEN (TRANSFER_UNIT)
#endif

static int sNightHyperlapseSpeed;

#ifndef HI5022Q_DISABLE_ADAPTIVE_MIPI
static kal_bool enable_adaptive_mipi;
static int adaptive_mipi_index;
#endif

#define PFX "HI5022Q D/D"

#define LOG_DBG(format, args...) pr_debug(PFX "[%s] " format, __func__, ##args)
#define LOG_INF(format, args...) pr_info(PFX "[%s] " format, __func__, ##args)
#define LOG_ERR(format, args...) pr_err(PFX "[%s] " format, __func__, ##args)

static DEFINE_SPINLOCK(imgsensor_drv_lock);
static enum IMGSENSOR_MODE get_imgsensor_mode(enum MSDK_SCENARIO_ID_ENUM scenario_id);

/*
 * Image Sensor Scenario
 * pre : 4:3 mode
 * cap : Check Active Array
 * normal_video : 16:9 mode
 * hs_video: slow motion
 * slim_video: not used
 * custom1 : FAST AE
 * custom2 : remosaic
 * custom3 : not used
 * custom4 : not used
 * custom5 : not used
 */

//HI5022Q_SENSOR_ID is defined in Kd_imgsensor.h
static struct imgsensor_info_struct imgsensor_info = {
	.sensor_id = HI5022Q_SENSOR_ID,
	.checksum_value = 0xa4c32546,
	.pre = {
		.pclk = 123500000,
		.linelength  = 1224,
		.framelength = 3362,
		.startx = 0,
		.starty = 0,
		.grabwindow_width  = 4080,
		.grabwindow_height = 3060,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 300,
		.mipi_pixel_rate = 538200000,
	},
	.cap = {
		.pclk = 123500000,
		.linelength  = 1224,
		.framelength = 3362,
		.startx = 0,
		.starty = 0,
		.grabwindow_width  = 4080,
		.grabwindow_height = 3060,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 300,
		.mipi_pixel_rate = 538200000,
	},
	.normal_video = {
		.pclk = 123500000,
		.linelength  = 1224,
		.framelength = 3362,
		.startx = 0,
		.starty = 0,
		.grabwindow_width  = 4080,
		.grabwindow_height = 2296,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 300,
		.mipi_pixel_rate = 538200000,
	},
	.hs_video = {
		.pclk = 123500000,
		.linelength  = 594,
		.framelength = 1732,
		.startx = 0,
		.starty = 0,
		.grabwindow_width  = 2032,
		.grabwindow_height = 1148,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 1200,
		.mipi_pixel_rate = 538200000,
	},
	.slim_video = { // NOT USED
		.pclk = 123500000,
		.linelength  = 1291,
		.framelength = 3186,
		.startx = 0,
		.starty = 0,
		.grabwindow_width  = 4080,
		.grabwindow_height = 3060,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 300,
		.mipi_pixel_rate = 538200000,
	},
	.custom1 = {
		.pclk = 123500000,
		.linelength  = 594,
		.framelength = 3464,
		.startx = 0,
		.starty = 0,
		.grabwindow_width  = 2032,
		.grabwindow_height = 1524,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 600,
		.mipi_pixel_rate = 538200000,
	},
	.custom2 = {
		.pclk = 123500000,
		.linelength  = 1266,
		.framelength = 6499,
		.startx = 0,
		.starty = 0,
		.grabwindow_width  = 8160,
		.grabwindow_height = 6120,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 150,
		.mipi_pixel_rate = 920400000,
	},
	.custom3 = { // NOT USED
		.pclk = 123500000,
		.linelength  = 1291,
		.framelength = 3186,
		.startx = 0,
		.starty = 0,
		.grabwindow_width  = 4080,
		.grabwindow_height = 3060,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 300,
		.mipi_pixel_rate = 538200000,
	},
	.custom4 = { // NOT USED
		.pclk = 123500000,
		.linelength  = 1291,
		.framelength = 3186,
		.startx = 0,
		.starty = 0,
		.grabwindow_width  = 4080,
		.grabwindow_height = 3060,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 300,
		.mipi_pixel_rate = 538200000,
	},
	.custom5 = { // NOT USED
		.pclk = 123500000,
		.linelength  = 1291,
		.framelength = 3186,
		.startx = 0,
		.starty = 0,
		.grabwindow_width  = 4080,
		.grabwindow_height = 3060,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 300,
		.mipi_pixel_rate = 538200000,
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

	.margin = 8,
	.min_shutter = 8,
	.min_gain = 64,
	.max_gain = 4096, // maximum analog gain 16x
	.min_gain_iso = 20,
	.exp_step = 1,
	.gain_step = 2,
	.gain_type = 2,
	.max_frame_length = 0xFFFFFF,

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

	.isp_driving_current = ISP_DRIVING_8MA,
	.sensor_interface_type = SENSOR_INTERFACE_TYPE_MIPI,
	.mipi_sensor_type = MIPI_OPHY_NCSI2,
	.mipi_settle_delay_mode = MIPI_SETTLEDELAY_AUTO,
	.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_4CELL_Gr,
	.mclk = 26,
	.mipi_lane_num = SENSOR_MIPI_4_LANE,
	.i2c_speed = 1000,

	/* record sensor support all write id addr,
	 * only supprt 4 must end with 0xff
	 */
	.i2c_addr_table = {0x44, 0x46, 0xff},
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
	.ihdr_mode = 0,
	.i2c_write_id = 0x44,
};

static struct SENSOR_VC_INFO_STRUCT SENSOR_VC_INFO[4] = {
	/* Preview mode setting */
	{0x02, 0x0a, 0x00, 0x08, 0x40, 0x00,
	 0x00, 0x2b, 0x0ff0, 0x0bf4,/*VC0, 4080x3060 pixel*/
	 0x00, 0x00, 0x0000, 0x0000,/*VC1*/
	 0x01, 0x2b, 0x03fc, 0x05f8,/*VC2, 1020x1528 pixel*/
	 0x00, 0x00, 0x0000, 0x0000},/*VC3*/
	/* cap = pre mode setting */
	{0x02, 0x0a, 0x00, 0x08, 0x40, 0x00,
	 0x00, 0x2b, 0x0ff0, 0x0bf4,/*VC0, 4080x3060 pixel*/
	 0x00, 0x00, 0x0000, 0x0000,/*VC1*/
	 0x01, 0x2b, 0x03fc, 0x05f8,/*VC2, 1020x1528 pixel*/
	 0x00, 0x00, 0x0000, 0x0000},/*VC3*/
	/* Video mode setting */
	{0x02, 0x0a, 0x00, 0x08, 0x40, 0x00,
	 0x00, 0x2b, 0x0ff0, 0x08f8,/*VC0, 4080x2296 pixel*/
	 0x00, 0x00, 0x0000, 0x0000,/*VC1*/
	 0x01, 0x2b, 0x03fc, 0x0478,/*VC2, 1020x1144 pixel*/
	 0x00, 0x00, 0x0000, 0x0000},/*VC3*/
	/* hs mode setting */
	{0x01, 0x0a, 0x00, 0x08, 0x40, 0x00,
	 0x00, 0x2b, 0x07f0, 0x047c,/*VC0, 2032x1148 pixel*/
	 0x00, 0x00, 0x0000, 0x0000,/*VC1*/
	 0x00, 0x00, 0x0000, 0x0000,/*VC2*/
	 0x00, 0x00, 0x0000, 0x0000},/*VC3*/
};

static struct SENSOR_WINSIZE_INFO_STRUCT imgsensor_winsize_info[] = {
	{   0,    0,    0,    0,    0,    0,    0,    0,
	    0,    0,    0,    0,    0,    0,    0,    0}, /* init - NOT USED*/
	{8224, 6176,    0,    0, 8224, 6176, 4112, 3088, // setfile reg addr readout_size: 0x0B1C, 0x0B1E
	   16,   14, 4080, 3060,    0,    0, 4080, 3060}, /* Preview */
	{8224, 6176,    0,    0, 8224, 6176, 4112, 3088,
	   16,   14, 4080, 3060,    0,    0, 4080, 3060}, /* capture */
	{8224, 6176,    0,  768, 8224, 4640, 4112, 2320,
	   16,   12, 4080, 2296,    0,    0, 4080, 2296}, /* video */
	{8224, 6176,    8,  752, 8208, 4672, 2052, 1168,
	   10,   10, 2032, 1148,    0,    0, 2032, 1148}, /* hs video */
	{8224, 6176,    0,    0, 8224, 6176, 4112, 3088,
	   16,   14, 4080, 3060,    0,    0, 4080, 3060}, /* slim video - NOT USED */
	{8224, 6176,    8,    0, 8208, 6176, 2052, 1544,
	   10,   10, 2032, 1524,    0,    0, 2032, 1524}, /* custom1 */
	{8224, 6176,    0,   12, 8224, 6152, 8224, 6152,
	   32,   16, 8160, 6120,    0,    0, 8160, 6120}, /* custom2 */
	{8224, 6176,    0,    0, 8224, 6176, 4112, 3088,
	   16,   14, 4080, 3060,    0,    0, 4080, 3060}, /* custom3 - NOT USED */
	{8224, 6176,    0,    0, 8224, 6176, 4112, 3088,
	   16,   14, 4080, 3060,    0,    0, 4080, 3060}, /* custom4 - NOT USED */
	{8224, 6176,    0,    0, 8224, 6176, 4112, 3088,
	   16,   14, 4080, 3060,    0,    0, 4080, 3060}, /* custom5 - NOT USED */
};


static struct SET_PD_BLOCK_INFO_T imgsensor_pd_info = {
		.i4OffsetX   = 0,
		.i4OffsetY   = 2,
		.i4PitchX    = 8,
		.i4PitchY    = 8,
		.i4PairNum   = 4,
		.i4SubBlkW   = 8,
		.i4SubBlkH   = 2,
		.i4BlockNumX = 510,
		.i4BlockNumY = 382,
		.iMirrorFlip = 0,
		.i4PosR = {
			{0, 2}, {2, 5}, {6, 6}, {4, 9},
		},
		.i4PosL = {
			{1, 2}, {3, 5}, {7, 6}, {5, 9},
		},
		.i4Crop = {
			{0, 0}, {0, 0}, {0, 382}, {0, 0}, {0, 0},
			{0, 0}, {0, 0}, {0, 0},   {0, 0}, {0, 0}
		}
};

/*****************************************************************************
 *
 * Filename:
 * ---------
 *	 hi5022qmipiraw_Sensor.c
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
static int write_cmos_sensor(kal_uint16 addr, kal_uint16 para)
{
	char pusendcmd[4] = {
		(char)(addr >> 8), (char)(addr & 0xFF),
		(char)(para >> 8), (char)(para & 0xFF) };
	int ret = 0;

	ret = iWriteRegI2CTiming(pusendcmd, 4,
		imgsensor.i2c_write_id, imgsensor_info.i2c_speed);
	return ret;
}

static kal_uint16 read_cmos_sensor(kal_uint16 addr)
{
	kal_uint16 get_byte = 0;
	char pusendcmd[2] = { (char)(addr >> 8), (char)(addr & 0xFF) };

	iReadRegI2CTiming(pusendcmd, 2, (u8 *) &get_byte, 2,
		imgsensor.i2c_write_id, imgsensor_info.i2c_speed);
	return ((get_byte << 8) & 0xFF00) | ((get_byte >> 8) & 0x00FF);
}

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
		puSendCmd[tosend++] = (char)(data >> 8);
		puSendCmd[tosend++] = (char)(data & 0xFF);

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

	write_cmos_sensor(HI5022Q_FLL_ADDR_HW, (imgsensor.frame_length >> 16) & 0xFFFF);
	write_cmos_sensor(HI5022Q_FLL_ADDR, imgsensor.frame_length & 0xFFFF);
	write_cmos_sensor(HI5022Q_LINE_LENGTH_PCK_ADDR, imgsensor.line_length & 0xFFFF);
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
	enum IMGSENSOR_MODE sensor_mode;
	unsigned int mipi_pixel_rate_full[CAM_HI5022Q_SET_FULL_MAX_NUM] = {
		920400000, 943800000, 956800000,
	};
	unsigned int mipi_pixel_rate_binning[CAM_HI5022Q_SET_BINNING_MAX_NUM] = {
		538200000, 668200000, 819000000, 837200000,
	};

	LOG_INF("- E\n");
	sensor_mode = get_imgsensor_mode(scenario_id);
	if (sensor_mode == IMGSENSOR_MODE_CUSTOM1) {
		LOG_INF("NOT supported sensor mode : %d", sensor_mode);
		return ERROR_NONE;
	} else if (sensor_mode == IMGSENSOR_MODE_CUSTOM2) {
		adaptive_mipi_index = imgsensor_select_mipi_by_rf_channel(hi5022q_mipi_channel_full,
							ARRAY_SIZE(hi5022q_mipi_channel_full));
		if (adaptive_mipi_index < 0 || adaptive_mipi_index >= CAM_HI5022Q_SET_FULL_MAX_NUM) {
			LOG_ERR("Not found mipi index: %d. mipi index is set to defaults value, 1150.5MHz", adaptive_mipi_index);
			adaptive_mipi_index = CAM_HI5022Q_SET_A_Full_1150p5_MHZ;
		}
		imgsensor_info.mode_info[sensor_mode]->mipi_pixel_rate = mipi_pixel_rate_full[adaptive_mipi_index];

	} else {
		adaptive_mipi_index = imgsensor_select_mipi_by_rf_channel(hi5022q_mipi_channel_binning,
							ARRAY_SIZE(hi5022q_mipi_channel_binning));
		if (adaptive_mipi_index < 0 || adaptive_mipi_index >= CAM_HI5022Q_SET_BINNING_MAX_NUM) {
			LOG_ERR("Not found mipi index: %d. mipi index is set to defaults value, 672.75MHz", adaptive_mipi_index);
			adaptive_mipi_index = CAM_HI5022Q_SET_B_binning_672p75_MHZ;
		}
		imgsensor_info.mode_info[sensor_mode]->mipi_pixel_rate = mipi_pixel_rate_binning[adaptive_mipi_index];

	}

	return adaptive_mipi_index;
}

static int set_mipi_mode(int mipi_index)
{
	enum IMGSENSOR_MODE sensor_mode;

	LOG_DBG("- E : adaptive mipi index %d", mipi_index);

	sensor_mode = get_imgsensor_mode(imgsensor.current_scenario_id);
	if (mipi_index >= CAM_HI5022Q_SET_MAX_NUM) {
		LOG_INF("adaptive Mipi setting(%s) - default",  hi5022q_adaptive_mipi_setfile[sensor_mode][0].name);
	} else {
		LOG_INF("adaptive Mipi setting(%s)", hi5022q_adaptive_mipi_setfile[sensor_mode][mipi_index].name);
		table_write_cmos_sensor(hi5022q_adaptive_mipi_setfile[sensor_mode][mipi_index].setfile,
			hi5022q_adaptive_mipi_setfile[sensor_mode][mipi_index].size);
	}
	LOG_DBG("- X");
	return ERROR_NONE;
}

/*
 * parameter[onoff]: KAL_TRUE[on], KAL_FALSE[off]
 *
 */
static void wait_stream_onoff(kal_bool onoff)
{
	unsigned int i = 0;
	unsigned int check_delay = onoff ? 2 : 1;
	int timeout = (10000 / imgsensor.current_fps) + 1;
	unsigned short readVal = 0x0;

	timeout = 400;

	mDELAY(3);
	for (i = 0; i < timeout; i++) {
		readVal = read_cmos_sensor(HI5022Q_STREAM_CHECK_ADDR) & 0x0F00;
		if (onoff) {
			if (readVal == 0x0F00)
				break;

			mDELAY(check_delay);
		} else {
			if (readVal == 0x0)
				break;

			mDELAY(check_delay);
		}
	}

	LOG_INF("onoff %d, pll_cfg(0x%04x): 0x%04x\n", onoff, HI5022Q_STREAM_CHECK_ADDR, read_cmos_sensor(HI5022Q_STREAM_CHECK_ADDR));
	LOG_INF("[onoff %d], wait_time = %d ms\n", onoff, i * check_delay + 3);
}

static kal_uint32 streaming_control(kal_bool enable)
{
	LOG_INF("streaming_enable(0=Sw Standby,1=streaming): %d\n", enable);

	if (enable) {
		LOG_INF("enable = %d", enable);

		if (enable_adaptive_mipi && imgsensor.current_scenario_id != MSDK_SCENARIO_ID_CUSTOM1)
			set_mipi_mode(adaptive_mipi_index);

		write_cmos_sensor(HI5022Q_STREAM_CONTROL_ADDR, 0x0100);
		// wait_stream_onoff(enable);
	} else {
		LOG_INF("streamoff enable = %d", enable);
		write_cmos_sensor(HI5022Q_STREAM_CONTROL_ADDR, 0x0000);

		//only wait stream_off
		wait_stream_onoff(enable);
	}
	return ERROR_NONE;
}

static void write_shutter(kal_uint32 shutter)
{
	kal_uint32 realtime_fps = 0;

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

	if (sNightHyperlapseSpeed)
		imgsensor.frame_length = ((imgsensor.pclk / 30) * sNightHyperlapseSpeed) / imgsensor.line_length;

	if (imgsensor.autoflicker_en) {
		realtime_fps = imgsensor.pclk /
		imgsensor.line_length * 10 / imgsensor.frame_length;

		if (realtime_fps >= 297 && realtime_fps <= 305)
			set_max_framerate(296, 0);
		else if (realtime_fps >= 147 && realtime_fps <= 150)
			set_max_framerate(146, 0);
		else {
			write_cmos_sensor(HI5022Q_FLL_ADDR_HW, (imgsensor.frame_length >> 16) & 0xFFFF);
			write_cmos_sensor(HI5022Q_FLL_ADDR, imgsensor.frame_length & 0xFFFF);
		}

	} else {
		write_cmos_sensor(HI5022Q_FLL_ADDR_HW, (imgsensor.frame_length >> 16) & 0xFFFF);
		write_cmos_sensor(HI5022Q_FLL_ADDR, imgsensor.frame_length & 0xFFFF);
	}

	write_cmos_sensor(HI5022Q_CIT_ADDR_HW, (shutter >> 16) & 0xFFFF);
	write_cmos_sensor(HI5022Q_CIT_ADDR, shutter & 0xFFFF);
	LOG_DBG("Exit! shutter =%d, framelength =%d\n", shutter, imgsensor.frame_length);
}

static void set_shutter_frame_length(kal_uint32 shutter, kal_uint32 frame_length)
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
	shutter = (shutter < imgsensor_info.min_shutter) ? imgsensor_info.min_shutter : shutter;
	shutter = (shutter > (imgsensor_info.max_frame_length - imgsensor_info.margin)) ? (imgsensor_info.max_frame_length - imgsensor_info.margin) : shutter;

	if (imgsensor.autoflicker_en) {
		realtime_fps = imgsensor.pclk / imgsensor.line_length * 10 / imgsensor.frame_length;
		if (realtime_fps >= 297 && realtime_fps <= 305)
			set_max_framerate(296, 0);
		else if (realtime_fps >= 147 && realtime_fps <= 150)
			set_max_framerate(146, 0);
		else {
			// Extend frame length
			write_cmos_sensor(HI5022Q_FLL_ADDR_HW, (imgsensor.frame_length >> 16) & 0xFFFF);
			write_cmos_sensor(HI5022Q_FLL_ADDR, imgsensor.frame_length & 0xFFFF);
		}
	} else {
		// Extend frame length
		write_cmos_sensor(HI5022Q_FLL_ADDR_HW, (imgsensor.frame_length >> 16) & 0xFFFF);
		write_cmos_sensor(HI5022Q_FLL_ADDR, imgsensor.frame_length & 0xFFFF);
	}

	// Update Shutter
	write_cmos_sensor(HI5022Q_CIT_ADDR_HW, (shutter >> 16) & 0xFFFF);
	write_cmos_sensor(HI5022Q_CIT_ADDR, shutter & 0xFFFF);
	LOG_DBG("Add for N3D! shutter =%d, framelength =%d\n", shutter, imgsensor.frame_length);
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
	kal_uint16 in_gain = gain;

	if (in_gain < imgsensor_info.min_gain)
		in_gain = imgsensor_info.min_gain;
	else if (in_gain > imgsensor_info.max_gain)
		in_gain = imgsensor_info.max_gain;

	reg_gain = (in_gain - imgsensor_info.min_gain) / 4;

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

	write_cmos_sensor(HI5022Q_AGAIN_ADDR, (reg_gain & 0x0FFF));
	return gain;
}

static unsigned int set_mode_setfile(enum IMGSENSOR_MODE mode)
{
	unsigned int ret = ERROR_NONE;
	int sensor_ret = 0;

	if (mode >= IMGSENSOR_MODE_MAX) {
		LOG_ERR("invalid mode: %d\n", mode);
		return ERROR_INVALID_SCENARIO_ID;
	}
	LOG_INF(" - E mode: %s\n", hi5022q_setfile_info[mode].name);

	if ((hi5022q_setfile_info[mode].setfile == NULL) || (hi5022q_setfile_info[mode].size == 0)) {
		LOG_ERR("failed, mode: %d", mode);
		ret = ERROR_INVALID_SCENARIO_ID;
	} else {
		sensor_ret = table_write_cmos_sensor(hi5022q_setfile_info[mode].setfile, hi5022q_setfile_info[mode].size);
		if (sensor_ret == -1)
			ret = ERROR_SENSOR_CONNECT_FAIL;
	}

	LOG_INF(" - X");
	return ret;
}

static unsigned int sensor_init(void)
{
	unsigned int ret = ERROR_NONE;

	LOG_INF(" - E");

	ret = set_mode_setfile(IMGSENSOR_MODE_INIT);

	return ret;
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
			*sensor_id = read_cmos_sensor(HI5022Q_SENSOR_ID_ADDR);
			LOG_INF("i2c write id: 0x%x, sensor id: 0x%x\n", imgsensor.i2c_write_id, *sensor_id);
			if (*sensor_id == imgsensor_info.sensor_id)
				return ERROR_NONE;

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

#if HI5022Q_XGC_ENABLE
static void write_sensor_hw_xgc(void)
{
	int i = 0;
	char *rom_cal_buf = NULL;
	const struct rom_cal_addr *xtc_cal_base_addr = NULL;
	const struct rom_cal_addr *hw_xgc_cal_addr = NULL;

	const int search_cnt = 15; //max cnt of search
	int start_addr = 0;
	int data_size = 0;

	kal_uint16 write_data[HI5022Q_XGC_HALF_SIZE] = {0x0, };
	int write_cnt = 0;
	kal_uint16 read_data = 0x0;

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
	//search addr and size of HW XGC
	hw_xgc_cal_addr = xtc_cal_base_addr;
	while (i < search_cnt) {
		if (hw_xgc_cal_addr == NULL) {
			LOG_ERR("Failed to find HW XGC cal");
			return;
		}

		if (hw_xgc_cal_addr->name == CROSSTALK_CAL_XGC) {
			start_addr = hw_xgc_cal_addr->addr;
			data_size  = hw_xgc_cal_addr->size;
			LOG_INF("HW XGC, start addr: %#06x, size: %d, search_cnt: %d", start_addr, data_size, i);
			break;
		}
		hw_xgc_cal_addr = hw_xgc_cal_addr->next;
		i++;
	}
	if (i >= search_cnt) {
		LOG_ERR("Failed to find HW GCC cal, search cnt is over");
		return;
	}

	if (data_size != HI5022Q_XGC_SIZE) {
		LOG_ERR("XGC Cal data size(%d) HI5022Q XGC size(%d)\n", data_size, HI5022Q_XGC_SIZE);
		return;
	}

	// OTP loading disable
	read_data = read_cmos_sensor(0x0318);
	write_cmos_sensor(0x0318, read_data & 0xFFEF); // XGC data loading disable B[4]

	// XGC(Gb line) SRAM memory access enable
	write_cmos_sensor(0xFFFF, 0x0A40); // XGC(Gb line) enable

	//write cal data of HW XGC
	rom_cal_buf += start_addr;

	//data size is always even. 480
	write_cnt = data_size / 4;

	for (i = 0; i < write_cnt; i++) {
		write_data[2 * i] = i * 2; //address
		//byte swap for writing big endian data
		write_data[2 * i + 1] = ((rom_cal_buf[2 * i] << 8) | rom_cal_buf[2 * i + 1]);
		LOG_DBG("write data[%d]: %#06x", 2 * i + 1, write_data[2 * i + 1]);
	}
	table_write_cmos_sensor(write_data, data_size / 2);
	LOG_INF("Gb done writing %d bytes", data_size / 2);

	// XGC(Gb line) SRAM memory access disable
	write_cmos_sensor(0xFFFF, 0x0); // XGC(Gb line) disable

	// XGC(Gr line) SRAM memory access enable
	write_cmos_sensor(0xFFFF, 0x0B40); // XGC(Gr line) enable

	rom_cal_buf += HI5022Q_XGC_HALF_SIZE;
	//data size is always even.
	write_cnt = data_size / 4;

	for (i = 0; i < write_cnt; i++) {
		write_data[2 * i] = i * 2; //address
		//byte swap for writing big endian data
		write_data[2 * i + 1] = ((rom_cal_buf[2 * i] << 8) | rom_cal_buf[2 * i + 1]);
		LOG_DBG("write data[%d]: %#06x", 2 * i + 1, write_data[2 * i + 1]);
	}
	table_write_cmos_sensor(write_data, data_size / 2);
	LOG_INF("Gr done writing %d bytes", data_size / 2);

	// XGC(Gr line) SRAM memory access disable
	write_cmos_sensor(0xFFFF, 0x0); // XGC(Gr line) disable

	//F/W run and data copy
	write_cmos_sensor(0x0B32, 0xAAAA);

	LOG_INF("-X");
}

static void write_sensor_hw_qbgc(void)
{
	int i = 0;
	char *rom_cal_buf = NULL;
	const struct rom_cal_addr *xtc_cal_base_addr = NULL;
	const struct rom_cal_addr *hw_qbgc_cal_addr = NULL;

	const int search_cnt = 15; //max cnt of search
	int start_addr = 0;
	int data_size = 0;

	kal_uint16 write_data[HI5022Q_QBGC_SIZE] = {0x0, };
	int write_cnt = 0;
	kal_uint16 read_data = 0x0;

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
	//search addr and size of HW XGC
	hw_qbgc_cal_addr = xtc_cal_base_addr;
	while (i < search_cnt) {
		if (hw_qbgc_cal_addr == NULL) {
			LOG_ERR("Failed to find HW XGC cal");
			return;
		}

		if (hw_qbgc_cal_addr->name == CROSSTALK_CAL_QBGC) {
			start_addr = hw_qbgc_cal_addr->addr;
			data_size  = hw_qbgc_cal_addr->size;
			LOG_INF("HW XGC, start addr: %#06x, size: %d, search_cnt: %d", start_addr, data_size, i);
			break;
		}
		hw_qbgc_cal_addr = hw_qbgc_cal_addr->next;
		i++;
	}
	if (i >= search_cnt) {
		LOG_ERR("Failed to find QBGC cal, search cnt is over");
		return;
	}

	if (data_size != HI5022Q_QBGC_SIZE) {
		LOG_ERR("QBGC Cal data size(%d), HI5022Q QBGC size (%d)\n", data_size, HI5022Q_QBGC_SIZE);
		return;
	}

	// OTP loading disable
	read_data = read_cmos_sensor(0x0318);

	write_cmos_sensor(0x0318, read_data & 0xFFF7); // QBGC data loading disable B[3]

	// QBGC SRAM memory access enable
	write_cmos_sensor(0xFFFF, 0x0E40); // QBGC enable

	//write cal data of HW QBGC
	rom_cal_buf += start_addr;

	//data size is always even.
	write_cnt = data_size / 2;

	for (i = 0; i < write_cnt; i++) {
		write_data[2 * i] = i * 2; //address
		//byte swap for writing big endian data
		write_data[2 * i + 1] = ((rom_cal_buf[2 * i] << 8) | rom_cal_buf[2 * i + 1]);
		LOG_DBG("write data[%d]: %#06x", 2 * i + 1, write_data[2 * i + 1]);
	}
	table_write_cmos_sensor(write_data, data_size);
	LOG_INF("done writing %d bytes", data_size);

	// QBGC SRAM memory access disable
	write_cmos_sensor(0xFFFF, 0x0000);  // QBGC disable

	//F/W run and data copy
	write_cmos_sensor(0x0B32, 0xAAAA);

	LOG_INF("-X");
}
#endif

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
	kal_uint32 sensor_id = 0;
	kal_uint32 ret = ERROR_NONE;

	LOG_INF(" - E");

	ret = get_imgsensor_id(&sensor_id);
	if (ret != ERROR_NONE)
		return ERROR_SENSOR_CONNECT_FAIL;

	ret = sensor_init();

#if HI5022Q_XGC_ENABLE
	write_sensor_hw_xgc();
	write_sensor_hw_qbgc();
#endif

	spin_lock(&imgsensor_drv_lock);
	imgsensor.autoflicker_en	= KAL_FALSE;
	imgsensor.sensor_mode		= IMGSENSOR_MODE_INIT;
	imgsensor.shutter			= 0x3D0;
	imgsensor.gain				= 0x100;
	imgsensor.pclk				= imgsensor_info.mode_info[IMGSENSOR_MODE_PREVIEW]->pclk;
	imgsensor.frame_length		= imgsensor_info.mode_info[IMGSENSOR_MODE_PREVIEW]->framelength;
	imgsensor.line_length		= imgsensor_info.mode_info[IMGSENSOR_MODE_PREVIEW]->linelength;
	imgsensor.min_frame_length	= imgsensor_info.mode_info[IMGSENSOR_MODE_PREVIEW]->framelength;
	imgsensor.dummy_pixel		= 0;
	imgsensor.dummy_line		= 0;
	imgsensor.ihdr_mode			= 0;
	imgsensor.test_pattern		= KAL_FALSE;
	imgsensor.current_fps		= imgsensor_info.mode_info[IMGSENSOR_MODE_PREVIEW]->max_framerate;
	spin_unlock(&imgsensor_drv_lock);

	LOG_INF(" - X");
	return ret;
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
	LOG_INF("- E\n");

	streaming_control(KAL_FALSE);
	sNightHyperlapseSpeed = 0;
#ifndef HI5022Q_DISABLE_ADAPTIVE_MIPI
	enable_adaptive_mipi = KAL_FALSE;
#endif
	return ERROR_NONE;
}

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

	sensor_info->SensorClockPolarity		= SENSOR_CLOCK_POLARITY_LOW;
	sensor_info->SensorClockFallingPolarity = SENSOR_CLOCK_POLARITY_LOW;

	sensor_info->SensorHsyncPolarity = SENSOR_CLOCK_POLARITY_LOW;
	LOG_INF("sensor_info->SensorHsyncPolarity = %d\n", sensor_info->SensorHsyncPolarity);
	sensor_info->SensorVsyncPolarity = SENSOR_CLOCK_POLARITY_LOW;
	LOG_INF("sensor_info->SensorVsyncPolarity = %d\n", sensor_info->SensorVsyncPolarity);

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

	sensor_info->PDAF_Support = PDAF_SUPPORT_CAMSV;

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
}

static kal_uint32 set_video_mode(UINT16 framerate)
{

	LOG_INF("framerate = %d\n", framerate);

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

static kal_uint32 set_max_framerate_by_scenario(
	enum MSDK_SCENARIO_ID_ENUM scenario_id, MUINT32 framerate)
{
	enum IMGSENSOR_MODE sensor_mode;

	LOG_INF("scenario_id = %d, framerate = %d\n", scenario_id, framerate);

	sensor_mode = get_imgsensor_mode(scenario_id);
	set_frame_length(sensor_mode, framerate);

	return ERROR_NONE;
}


static kal_uint32 get_default_framerate_by_scenario(
	enum MSDK_SCENARIO_ID_ENUM scenario_id, MUINT32 *framerate)
{
	enum IMGSENSOR_MODE sensor_mode;

	sensor_mode = get_imgsensor_mode(scenario_id);
	if (!imgsensor_info.is_invalid_mode)
		*framerate = imgsensor_info.mode_info[sensor_mode]->max_framerate;

	LOG_INF("scenario_id = %d, framerate = %d", scenario_id, *framerate);
	return ERROR_NONE;
}

static kal_uint32 set_test_pattern_mode(kal_bool enable)
{
	//test pattern is always disable
	spin_lock(&imgsensor_drv_lock);
	imgsensor.test_pattern = false;
	spin_unlock(&imgsensor_drv_lock);
	LOG_INF("test pattern not supported");

	return ERROR_NONE;
}
static kal_uint32 get_sensor_temperature(void)
{
	LOG_DBG("Temperature information is not valid");
	return 0;
}

#if defined(CONFIG_TRAN_CAMERA_SYNC_AWB_TO_KERNEL)
static void set_awbgain(kal_uint32 gr_gain, kal_uint32 r_gain, kal_uint32 b_gain, kal_uint32 gb_gain)
{
	kal_uint32 gain_max = 0x1FFF
	kal_uint32 gr_gain_int = 0x0;
	kal_uint32 r_gain_int = 0x0;
	kal_uint32 b_gain_int = 0x0;
	kal_uint32 gb_gain_int = 0x0;
	kal_uint16 read_val = 0x0;
	kal_uint16 isp_en_val = 0x100;

	gr_gain_int = gr_gain;
	if (gr_gain_int > gain_max)
		gr_gain_int = gain_max;

	r_gain_int = r_gain;
	if (r_gain_int > gain_max)
		r_gain_int = gain_max;

	b_gain_int = b_gain;
	if (b_gain_int > gain_max)
		b_gain_int = gain_max;

	gb_gain_int = gb_gain;
	if (gb_gain_int > gain_max)
		gb_gain_int = gain_max;

	LOG_INF("gr_gain=0x%x, r_gain=0x%x, b_gain=0x%x, gb_gain=0x%x\n", gr_gain, r_gain, b_gain, gb_gain);

	write_cmos_sensor(0x0514, gr_gain_int & 0xFFFF);
	write_cmos_sensor(0x0516, gb_gain_int & 0xFFFF);
	write_cmos_sensor(0x0518, r_gain_int & 0xFFFF);
	write_cmos_sensor(0x051A, b_gain_int & 0xFFFF);

	read_val = read_cmos_sensor(HI5022Q_ISP_ADDR);
	write_cmos_sensor(HI5022Q_ISP_ADDR, (read_val | isp_en_val) & 0xFFFF);
}
#endif

#define XTC_NUM 2
static int sensor_get_XTC_CAL_data(char *reordered_xtc_cal, unsigned int buf_len)
{
	char *rom_cal_buf = NULL;
	int const MAX_COUNT = 15;

	enum crosstalk_cal_name const XTC_ORDER[XTC_NUM] = {
		CROSSTALK_CAL_XGC,
		CROSSTALK_CAL_OPC
	};

	const struct rom_cal_addr *xtc_cal_addr = NULL;
	const int HEADER_SIZE = sizeof(unsigned short);
	int i, timeout, cur_size, cur_addr;

	enum crosstalk_cal_name target_xtc_name;
	const struct rom_cal_addr *cur_xtc_cal;
	int reordered_idx = 0;
	unsigned short total_size = 0;

	// TODO : sensor_dev_id needs to be updated according to actual id.
	int sensor_dev_id = IMGSENSOR_SENSOR_IDX_MAIN;

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
				LOG_INF("[%s] reordered_xtc_cal[0x%x] = rom_cal_buf[0x%x], val: 0x%x, size: 0x%x"
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
	struct SENSOR_VC_INFO_STRUCT *pvcinfo;
	struct SET_PD_BLOCK_INFO_T *PDAFinfo;

	char *XTC_CAL;
	unsigned int xtc_buf_size;

	MSDK_SENSOR_REG_INFO_STRUCT *sensor_reg_data =
		(MSDK_SENSOR_REG_INFO_STRUCT *) feature_para;

	enum IMGSENSOR_MODE sensor_mode;

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
		sensor_mode = get_imgsensor_mode(*feature_data);
		*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) = imgsensor_info.mode_info[sensor_mode]->pclk;
		break;

	case SENSOR_FEATURE_GET_PERIOD_BY_SCENARIO:
		sensor_mode = get_imgsensor_mode(*feature_data);
		*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) = (imgsensor_info.mode_info[sensor_mode]->framelength << 16)
			+ imgsensor_info.mode_info[sensor_mode]->linelength;
		break;

	case SENSOR_FEATURE_SET_ESHUTTER:
		LOG_DBG("SENSOR_FEATURE_SET_ESHUTTER = %d\n", feature_id);
		set_shutter(*feature_data);
		break;

	case SENSOR_FEATURE_SET_NIGHTMODE:
		LOG_INF("SENSOR_FEATURE_SET_NIGHTMODE = %d, data=%d\n", feature_id, *feature_data);
		sNightHyperlapseSpeed = *feature_data;
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

		sensor_mode = get_imgsensor_mode(*feature_data);
		memcpy((void *)wininfo, (void *)&imgsensor_winsize_info[sensor_mode],
				sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
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
			imgsensor_pd_info.i4BlockNumX = 510;
			imgsensor_pd_info.i4BlockNumY = 382;
			memcpy((void *)PDAFinfo,
			(void *)&imgsensor_pd_info,
			sizeof(struct SET_PD_BLOCK_INFO_T));
			break;
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			LOG_INF("MSDK_SCENARIO_ID_VIDEO_PREVIEW:%d\n", MSDK_SCENARIO_ID_VIDEO_PREVIEW);
			imgsensor_pd_info.i4BlockNumX = 510;
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
		LOG_INF("SENSOR_FEATURE_GET_SENSOR_PDAF_CAPACITY scenarioId:%d\n", (UINT16) *feature_data);
		/* PDAF capacity enable or not */
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
		set_awbgain((UINT32)(*feature_data_32), (UINT32)*(feature_data_32 + 1), (UINT32)*(feature_data_32 + 2), (UINT32)*(feature_data_32 + 3));
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
		*feature_return_para_i32 = get_sensor_temperature();
		*feature_para_len = 4;
		break;

	case SENSOR_FEATURE_GET_PIXEL_RATE:
		LOG_INF("SENSOR_FEATURE_GET_PIXEL_RATE = %d\n", feature_id);
		LOG_INF("MSDK_SCENARIO_ID = %d\n", *feature_data);
		sensor_mode = get_imgsensor_mode(*feature_data);
		*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) = (imgsensor_info.mode_info[sensor_mode]->pclk /
			(imgsensor_info.mode_info[sensor_mode]->linelength - 80)) * imgsensor_info.mode_info[sensor_mode]->grabwindow_width;
		break;

	case SENSOR_FEATURE_GET_BINNING_TYPE:
		switch (*(feature_data + 1)) {
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
			*feature_return_para_32 = 4;
			break;
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		case MSDK_SCENARIO_ID_CUSTOM1:
		case MSDK_SCENARIO_ID_CUSTOM2:
		case MSDK_SCENARIO_ID_CUSTOM3:
		case MSDK_SCENARIO_ID_CUSTOM4:
		case MSDK_SCENARIO_ID_CUSTOM5:
		default:
			*feature_return_para_32 = 1;
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

	case SENSOR_FEATURE_GET_MIPI_PIXEL_RATE:
		LOG_DBG("SENSOR_FEATURE_GET_MIPI_PIXEL_RATE = %d, MSDK_SCENARIO_ID = %d\n",
			feature_id, *feature_data);

		if (enable_adaptive_mipi)
			update_mipi_info(*feature_data);

		sensor_mode = get_imgsensor_mode(*feature_data);
		*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) = imgsensor_info.mode_info[sensor_mode]->mipi_pixel_rate;
		break;

#ifndef HI5022Q_DISABLE_ADAPTIVE_MIPI
	case SENSOR_FEATURE_SET_ADAPTIVE_MIPI:
		if (*feature_para == 1)
			enable_adaptive_mipi = KAL_TRUE;
		break;
#endif

	case SENSOR_FEATURE_GET_4CELL_DATA:
		LOG_INF("SENSOR_FEATURE_GET_4CELL_DATA\n");
		XTC_CAL = (char *)(*(feature_data + 1));
		xtc_buf_size = *(feature_data + 2);

		if (sensor_get_XTC_CAL_data(XTC_CAL, xtc_buf_size) < 0) {
			LOG_ERR("Failed to get XTC cal");
			return ERROR_SENSOR_FEATURE_NOT_IMPLEMENT;
		}
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

UINT32 HI5022Q_MIPI_RAW_SensorInit(struct SENSOR_FUNCTION_STRUCT **pfFunc)
{
	/* To Do : Check Sensor status here */
	if (pfFunc != NULL)
		*pfFunc = &sensor_func;
	return ERROR_NONE;
}
