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
 *	 s5khm6mipiraw_Sensor.h
 *
 * Project:
 * --------
 *	 ALPS
 *
 * Description:
 * ------------
 *	 CMOS sensor header file
 *
 ****************************************************************************/
#ifndef _S5KHM6MIPI_SENSOR_H
#define _S5KHM6MIPI_SENSOR_H

#include "kd_imgsensor_adaptive_mipi.h"

// if you want to use implant mode without tail data(PDAF data), use below define.
//#define PDAF_DISABLE
#define INDIRECT_BURST                                (1)
#define WRITE_SENSOR_CAL_XTC                          (1)
// #define USE_RAW12 // for iDCG and LN4

enum IMGSENSOR_MODE {
	IMGSENSOR_MODE_INIT,
	IMGSENSOR_MODE_PREVIEW,
	IMGSENSOR_MODE_CAPTURE,
	IMGSENSOR_MODE_VIDEO,
	IMGSENSOR_MODE_HIGH_SPEED_VIDEO,
	IMGSENSOR_MODE_SLIM_VIDEO,
	IMGSENSOR_MODE_CUSTOM1,
	IMGSENSOR_MODE_CUSTOM2,
	IMGSENSOR_MODE_CUSTOM3,
	IMGSENSOR_MODE_CUSTOM4,
	IMGSENSOR_MODE_CUSTOM5,
	IMGSENSOR_MODE_CUSTOM6,
	IMGSENSOR_MODE_MAX
};

enum IMGSENSOR_FC_MODE {
	IMGSENSOR_FC_MODE_4_3_IDCG,
	IMGSENSOR_FC_MODE_4_3_LN4,
	IMGSENSOR_FC_MODE_16_9_IDCG,
	IMGSENSOR_FC_MODE_16_9_LN4,
	IMGSENSOR_FC_MODE_MAX
};

enum IMGSENSOR_FC_SET_MODE {
	IMGSENSOR_FC_SET_MODE_IDCG,
	IMGSENSOR_FC_SET_MODE_LN4,
	IMGSENSOR_FC_SET_MODE_HAL_MAX,
	IMGSENSOR_FC_SET_MODE_DISABLE,
	IMGSENSOR_FC_SET_MODE_MAX
};

enum IMGSENSOR_FCM_SRAM {
	IMGSENSOR_FCM_SRAM_NOT_SET,
	IMGSENSOR_FCM_SRAM_4_3,
	IMGSENSOR_FCM_SRAM_16_9,
	IMGSENSOR_FCM_SRAM_MAX
};

struct setfile_mode_info {
	kal_uint16 *setfile;
	kal_uint32 size;
	char *name;
};

struct imgsensor_mode_struct {
	kal_uint32 pclk;	/* record different mode's pclk */
	kal_uint32 linelength;	/* record different mode's linelength */
	kal_uint32 framelength;	/* record different mode's framelength */

	kal_uint8 startx; /* record different mode's startx of grabwindow */
	kal_uint8 starty; /* record different mode's startx of grabwindow */

	/* record different mode's width of grabwindow */
	kal_uint16 grabwindow_width;

	/* record different mode's height of grabwindow */
	kal_uint16 grabwindow_height;

	/* following for MIPIDataLowPwr2HighSpeedSettleDelayCount
	 * by different scenario
	 */
	kal_uint8 mipi_data_lp2hs_settle_dc;

	/*       following for GetDefaultFramerateByScenario()  */
	kal_uint16 max_framerate;
	kal_uint32 mipi_pixel_rate;

};

/* SENSOR PRIVATE STRUCT FOR VARIABLES*/
struct imgsensor_struct {
	kal_uint8 mirror;	/* mirrorflip information */

	kal_uint8 sensor_mode;	/* record IMGSENSOR_MODE enum value */

	kal_uint32 shutter;	/* current shutter */
	kal_uint16 gain;	/* current gain */

	kal_uint32 pclk;	/* current pclk */

	kal_uint32 frame_length;	/* current framelength */
	kal_uint32 line_length;	/* current linelength */

	/* current min	framelength to max framerate */
	kal_uint32 min_frame_length;
	kal_uint16 dummy_pixel;	/* current dummypixel */
	kal_uint16 dummy_line;	/* current dummline */

	kal_uint16 current_fps;	/* current max fps */
	kal_bool autoflicker_en; /* record autoflicker enable or disable */
	kal_bool test_pattern;	/* record test pattern mode or not */

	kal_uint16 frame_length_shifter;

	/* current scenario id */
	enum MSDK_SCENARIO_ID_ENUM current_scenario_id;

	/* ihdr mode 0: disable, 1: ihdr, 2:mVHDR, 9:zigzag */
	kal_uint8 ihdr_mode;

	kal_uint8 i2c_write_id;	/* record current sensor's i2c write id */
};

/* SENSOR PRIVATE STRUCT FOR CONSTANT*/
struct imgsensor_info_struct {
	kal_uint16 sensor_id;	/* record sensor id defined in Kd_imgsensor.h */
	kal_uint32 checksum_value; /* checksum value for Camera Auto Test */

	/* preview scenario relative information */
	struct imgsensor_mode_struct pre;

	/* capture scenario relative information */
	struct imgsensor_mode_struct cap;

	/* capture for PIP 24fps relative information */
	struct imgsensor_mode_struct cap1;

	/* capture for PIP 24fps relative information */
	struct imgsensor_mode_struct cap2;

	/* normal video  scenario relative information */
	struct imgsensor_mode_struct normal_video;

	/* high speed video scenario relative information */
	struct imgsensor_mode_struct hs_video;

	/* slim video for VT scenario relative information */
	struct imgsensor_mode_struct slim_video;
     /* custom1 for stereo relative information */
	struct imgsensor_mode_struct custom1;
	struct imgsensor_mode_struct custom2;
	struct imgsensor_mode_struct custom3;
	struct imgsensor_mode_struct custom4;
	struct imgsensor_mode_struct custom5;
	struct imgsensor_mode_struct custom6;
	kal_uint8 ae_shut_delay_frame;	/* shutter delay frame for AE cycle */

	/* sensor gain delay frame for AE cycle */
	kal_uint8 ae_sensor_gain_delay_frame;

	/* isp gain delay frame for AE cycle */
	kal_uint8 ae_ispGain_delay_frame;

	kal_uint8  frame_time_delay_frame;

	kal_uint8 ihdr_support;	/* 1, support; 0,not support */
	kal_uint8 ihdr_le_firstline;	/* 1,le first ; 0, se first */
	kal_uint8 temperature_support;	/* 1, support; 0,not support */
	kal_uint8 sensor_mode_num;	/* support sensor mode num */

	kal_uint8 cap_delay_frame;	/* enter capture delay frame num */
	kal_uint8 pre_delay_frame;	/* enter preview delay frame num */
	kal_uint8 video_delay_frame;	/* enter video delay frame num */

	/* enter high speed video  delay frame num */
	kal_uint8 hs_video_delay_frame;

	kal_uint8 slim_video_delay_frame; /* enter slim video delay frame num */

	kal_uint8 custom1_delay_frame; /* enter custom1 delay frame num */
	kal_uint8 custom2_delay_frame;
	kal_uint8 custom3_delay_frame;
	kal_uint8 custom4_delay_frame;
	kal_uint8 custom5_delay_frame;
	kal_uint8 custom6_delay_frame;
	kal_uint8 margin;	/* sensor framelength & shutter margin */
	kal_uint32 min_shutter;	/* min shutter */

	/* max framelength by sensor register's limitation */
	kal_uint32 max_frame_length;
	kal_uint32 min_gain;
	kal_uint32 max_gain;
	kal_uint32 min_gain_iso;
	kal_uint32 gain_step;
	kal_uint32 exp_step;
	kal_uint32 gain_type;

	kal_uint8 isp_driving_current;	/* mclk driving current */
	kal_uint8 sensor_interface_type;	/* sensor_interface_type */

	kal_uint8 mipi_sensor_type;
	/* 0,MIPI_OPHY_NCSI2; 1,MIPI_OPHY_CSI2,
	 * default is NCSI2, don't modify this para
	 */

	kal_uint8 mipi_settle_delay_mode;
	/* 0, high speed signal auto detect;
	 * 1, use settle delay,unit is ns,
	 * default is auto detect, don't modify this para
	 */

	kal_uint8 sensor_output_dataformat;
	kal_uint8 mclk;	/* mclk value, suggest 24 or 26 for 24Mhz or 26Mhz */

	kal_uint8 mipi_lane_num;	/* mipi lane num */

	/* record sensor support all write id addr,
	 * only supprt 4must end with 0xff
	 */
	kal_uint32 i2c_speed;
	kal_uint8 i2c_addr_table[5];

};

	/* MIPI table */
enum {
	CAM_HM6_SET_A_all_DEFAULT_MIPI_CLOCK = 0,  /* Default - mipi clock = 2021.5 Mhz (4043Mbps), mipi rate = Mhz */
	CAM_HM6_SET_A_AtoD_2021p5_MHZ = 0,         /* mipi clock = 2021.5 Mhz (4043Mbps), mipi rate = Mhz */
	CAM_HM6_SET_A_AtoD_2073p5_MHZ = 1,         /* mipi clock = 2073.5 Mhz (4147Mbps), mipi rate = Mhz */
	CAM_HM6_SET_A_AtoD_2151p5_MHZ = 2,         /* mipi clock = 2151.5 Mhz (4303Mbps), mipi rate = Mhz */
};

static kal_uint16 s5khm6_mipi_full_2021p5mhz[] = {
	0xFCFC, 0x4000,
	0x0310, 0x0137,
};

static kal_uint16 s5khm6_mipi_full_2073p5mhz[] = {
	0xFCFC, 0x4000,
	0x0310, 0x013F,
};

static kal_uint16 s5khm6_mipi_full_2151p5mhz[] = {
	0xFCFC, 0x4000,
	0x0310, 0x014B,
};

struct cam_mipi_channel s5khm6_mipi_channel_full[] = {	
	{ CAM_RAT_BAND(CAM_RAT_1_GSM, CAM_BAND_001_GSM_GSM850), 0, 0, CAM_HM6_SET_A_AtoD_2021p5_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_1_GSM, CAM_BAND_002_GSM_EGSM900), 0, 0, CAM_HM6_SET_A_AtoD_2021p5_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_1_GSM, CAM_BAND_003_GSM_DCS1800), 0, 0, CAM_HM6_SET_A_AtoD_2073p5_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_1_GSM, CAM_BAND_004_GSM_PCS1900), 0, 0, CAM_HM6_SET_A_AtoD_2021p5_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_2_WCDMA, CAM_BAND_011_WCDMA_WB01), 10562, 10576, CAM_HM6_SET_A_AtoD_2073p5_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_2_WCDMA, CAM_BAND_011_WCDMA_WB01), 10577, 10626, CAM_HM6_SET_A_AtoD_2151p5_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_2_WCDMA, CAM_BAND_011_WCDMA_WB01), 10627, 10791, CAM_HM6_SET_A_AtoD_2021p5_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_2_WCDMA, CAM_BAND_011_WCDMA_WB01), 10792, 10823, CAM_HM6_SET_A_AtoD_2073p5_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_2_WCDMA, CAM_BAND_011_WCDMA_WB01), 10824, 10838, CAM_HM6_SET_A_AtoD_2151p5_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_2_WCDMA, CAM_BAND_012_WCDMA_WB02), 9662, 9829, CAM_HM6_SET_A_AtoD_2021p5_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_2_WCDMA, CAM_BAND_012_WCDMA_WB02), 9830, 9836, CAM_HM6_SET_A_AtoD_2073p5_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_2_WCDMA, CAM_BAND_012_WCDMA_WB02), 9837, 9904, CAM_HM6_SET_A_AtoD_2151p5_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_2_WCDMA, CAM_BAND_012_WCDMA_WB02), 9905, 9938, CAM_HM6_SET_A_AtoD_2021p5_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_2_WCDMA, CAM_BAND_013_WCDMA_WB03), 1162, 1232, CAM_HM6_SET_A_AtoD_2021p5_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_2_WCDMA, CAM_BAND_013_WCDMA_WB03), 1233, 1295, CAM_HM6_SET_A_AtoD_2151p5_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_2_WCDMA, CAM_BAND_013_WCDMA_WB03), 1296, 1307, CAM_HM6_SET_A_AtoD_2073p5_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_2_WCDMA, CAM_BAND_013_WCDMA_WB03), 1308, 1473, CAM_HM6_SET_A_AtoD_2021p5_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_2_WCDMA, CAM_BAND_013_WCDMA_WB03), 1474, 1513, CAM_HM6_SET_A_AtoD_2151p5_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_2_WCDMA, CAM_BAND_014_WCDMA_WB04), 1537, 1552, CAM_HM6_SET_A_AtoD_2073p5_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_2_WCDMA, CAM_BAND_014_WCDMA_WB04), 1553, 1601, CAM_HM6_SET_A_AtoD_2151p5_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_2_WCDMA, CAM_BAND_014_WCDMA_WB04), 1602, 1738, CAM_HM6_SET_A_AtoD_2021p5_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_2_WCDMA, CAM_BAND_015_WCDMA_WB05), 4357, 4369, CAM_HM6_SET_A_AtoD_2073p5_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_2_WCDMA, CAM_BAND_015_WCDMA_WB05), 4370, 4458, CAM_HM6_SET_A_AtoD_2021p5_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_2_WCDMA, CAM_BAND_016_WCDMA_WB06), 4387, 4413, CAM_HM6_SET_A_AtoD_2021p5_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_2_WCDMA, CAM_BAND_017_WCDMA_WB07), 2237, 2323, CAM_HM6_SET_A_AtoD_2021p5_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_2_WCDMA, CAM_BAND_017_WCDMA_WB07), 2324, 2398, CAM_HM6_SET_A_AtoD_2073p5_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_2_WCDMA, CAM_BAND_017_WCDMA_WB07), 2399, 2563, CAM_HM6_SET_A_AtoD_2021p5_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_2_WCDMA, CAM_BAND_018_WCDMA_WB08), 2937, 3076, CAM_HM6_SET_A_AtoD_2021p5_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_2_WCDMA, CAM_BAND_018_WCDMA_WB08), 3077, 3088, CAM_HM6_SET_A_AtoD_2073p5_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_2_WCDMA, CAM_BAND_029_WCDMA_WB19), 712, 763, CAM_HM6_SET_A_AtoD_2021p5_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_091_LTE_LB01), 0, 53, CAM_HM6_SET_A_AtoD_2073p5_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_091_LTE_LB01), 54, 152, CAM_HM6_SET_A_AtoD_2151p5_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_091_LTE_LB01), 153, 483, CAM_HM6_SET_A_AtoD_2021p5_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_091_LTE_LB01), 484, 547, CAM_HM6_SET_A_AtoD_2073p5_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_091_LTE_LB01), 548, 599, CAM_HM6_SET_A_AtoD_2151p5_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_092_LTE_LB02), 600, 627, CAM_HM6_SET_A_AtoD_2151p5_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_092_LTE_LB02), 628, 958, CAM_HM6_SET_A_AtoD_2021p5_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_092_LTE_LB02), 959, 972, CAM_HM6_SET_A_AtoD_2073p5_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_092_LTE_LB02), 973, 1108, CAM_HM6_SET_A_AtoD_2151p5_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_092_LTE_LB02), 1109, 1199, CAM_HM6_SET_A_AtoD_2021p5_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_093_LTE_LB03), 1200, 1364, CAM_HM6_SET_A_AtoD_2021p5_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_093_LTE_LB03), 1365, 1491, CAM_HM6_SET_A_AtoD_2151p5_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_093_LTE_LB03), 1492, 1514, CAM_HM6_SET_A_AtoD_2073p5_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_093_LTE_LB03), 1515, 1846, CAM_HM6_SET_A_AtoD_2021p5_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_093_LTE_LB03), 1847, 1949, CAM_HM6_SET_A_AtoD_2151p5_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_094_LTE_LB04), 1950, 1952, CAM_HM6_SET_A_AtoD_2021p5_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_094_LTE_LB04), 1953, 2003, CAM_HM6_SET_A_AtoD_2073p5_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_094_LTE_LB04), 2004, 2102, CAM_HM6_SET_A_AtoD_2151p5_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_094_LTE_LB04), 2103, 2399, CAM_HM6_SET_A_AtoD_2021p5_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_095_LTE_LB05), 2400, 2448, CAM_HM6_SET_A_AtoD_2073p5_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_095_LTE_LB05), 2449, 2649, CAM_HM6_SET_A_AtoD_2021p5_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_097_LTE_LB07), 2750, 2947, CAM_HM6_SET_A_AtoD_2021p5_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_097_LTE_LB07), 2948, 3097, CAM_HM6_SET_A_AtoD_2073p5_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_097_LTE_LB07), 3098, 3428, CAM_HM6_SET_A_AtoD_2021p5_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_097_LTE_LB07), 3429, 3449, CAM_HM6_SET_A_AtoD_2073p5_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_098_LTE_LB08), 3450, 3751, CAM_HM6_SET_A_AtoD_2021p5_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_098_LTE_LB08), 3752, 3799, CAM_HM6_SET_A_AtoD_2073p5_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_102_LTE_LB12), 5010, 5014, CAM_HM6_SET_A_AtoD_2073p5_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_102_LTE_LB12), 5015, 5179, CAM_HM6_SET_A_AtoD_2021p5_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_103_LTE_LB13), 5180, 5279, CAM_HM6_SET_A_AtoD_2021p5_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_104_LTE_LB14), 5280, 5325, CAM_HM6_SET_A_AtoD_2021p5_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_104_LTE_LB14), 5326, 5379, CAM_HM6_SET_A_AtoD_2073p5_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_107_LTE_LB17), 5730, 5849, CAM_HM6_SET_A_AtoD_2021p5_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_108_LTE_LB18), 5850, 5988, CAM_HM6_SET_A_AtoD_2073p5_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_108_LTE_LB18), 5989, 5999, CAM_HM6_SET_A_AtoD_2021p5_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_109_LTE_LB19), 6000, 6149, CAM_HM6_SET_A_AtoD_2021p5_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_110_LTE_LB20), 6150, 6347, CAM_HM6_SET_A_AtoD_2021p5_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_110_LTE_LB20), 6348, 6449, CAM_HM6_SET_A_AtoD_2073p5_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_111_LTE_LB21), 6450, 6486, CAM_HM6_SET_A_AtoD_2073p5_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_111_LTE_LB21), 6487, 6599, CAM_HM6_SET_A_AtoD_2021p5_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_115_LTE_LB25), 8040, 8067, CAM_HM6_SET_A_AtoD_2151p5_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_115_LTE_LB25), 8068, 8398, CAM_HM6_SET_A_AtoD_2021p5_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_115_LTE_LB25), 8399, 8412, CAM_HM6_SET_A_AtoD_2073p5_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_115_LTE_LB25), 8413, 8548, CAM_HM6_SET_A_AtoD_2151p5_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_115_LTE_LB25), 8549, 8689, CAM_HM6_SET_A_AtoD_2021p5_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_116_LTE_LB26), 8690, 8838, CAM_HM6_SET_A_AtoD_2073p5_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_116_LTE_LB26), 8839, 9039, CAM_HM6_SET_A_AtoD_2021p5_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_118_LTE_LB28), 9210, 9255, CAM_HM6_SET_A_AtoD_2021p5_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_118_LTE_LB28), 9256, 9405, CAM_HM6_SET_A_AtoD_2073p5_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_118_LTE_LB28), 9406, 9659, CAM_HM6_SET_A_AtoD_2021p5_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_119_LTE_LB29), 9660, 9769, CAM_HM6_SET_A_AtoD_2073p5_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_120_LTE_LB30), 9770, 9779, CAM_HM6_SET_A_AtoD_2021p5_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_120_LTE_LB30), 9780, 9869, CAM_HM6_SET_A_AtoD_2073p5_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_122_LTE_LB32), 9920, 10265, CAM_HM6_SET_A_AtoD_2021p5_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_122_LTE_LB32), 10266, 10359, CAM_HM6_SET_A_AtoD_2073p5_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_124_LTE_LB34), 36200, 36240, CAM_HM6_SET_A_AtoD_2021p5_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_124_LTE_LB34), 36241, 36266, CAM_HM6_SET_A_AtoD_2073p5_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_124_LTE_LB34), 36267, 36349, CAM_HM6_SET_A_AtoD_2151p5_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_128_LTE_LB38), 37750, 37965, CAM_HM6_SET_A_AtoD_2021p5_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_128_LTE_LB38), 37966, 38115, CAM_HM6_SET_A_AtoD_2073p5_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_128_LTE_LB38), 38116, 38249, CAM_HM6_SET_A_AtoD_2021p5_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_129_LTE_LB39), 38250, 38285, CAM_HM6_SET_A_AtoD_2151p5_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_129_LTE_LB39), 38286, 38296, CAM_HM6_SET_A_AtoD_2073p5_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_129_LTE_LB39), 38297, 38627, CAM_HM6_SET_A_AtoD_2021p5_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_129_LTE_LB39), 38628, 38628, CAM_HM6_SET_A_AtoD_2073p5_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_129_LTE_LB39), 38629, 38649, CAM_HM6_SET_A_AtoD_2151p5_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_130_LTE_LB40), 38650, 38677, CAM_HM6_SET_A_AtoD_2021p5_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_130_LTE_LB40), 38678, 38778, CAM_HM6_SET_A_AtoD_2073p5_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_130_LTE_LB40), 38779, 38827, CAM_HM6_SET_A_AtoD_2151p5_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_130_LTE_LB40), 38828, 39159, CAM_HM6_SET_A_AtoD_2021p5_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_130_LTE_LB40), 39160, 39281, CAM_HM6_SET_A_AtoD_2073p5_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_130_LTE_LB40), 39282, 39309, CAM_HM6_SET_A_AtoD_2151p5_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_130_LTE_LB40), 39310, 39640, CAM_HM6_SET_A_AtoD_2021p5_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_130_LTE_LB40), 39641, 39649, CAM_HM6_SET_A_AtoD_2073p5_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_131_LTE_LB41), 39650, 39793, CAM_HM6_SET_A_AtoD_2073p5_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_131_LTE_LB41), 39794, 40124, CAM_HM6_SET_A_AtoD_2021p5_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_131_LTE_LB41), 40125, 40274, CAM_HM6_SET_A_AtoD_2073p5_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_131_LTE_LB41), 40275, 40605, CAM_HM6_SET_A_AtoD_2021p5_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_131_LTE_LB41), 40606, 40755, CAM_HM6_SET_A_AtoD_2073p5_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_131_LTE_LB41), 40756, 41087, CAM_HM6_SET_A_AtoD_2021p5_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_131_LTE_LB41), 41088, 41237, CAM_HM6_SET_A_AtoD_2073p5_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_131_LTE_LB41), 41238, 41568, CAM_HM6_SET_A_AtoD_2021p5_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_131_LTE_LB41), 41569, 41589, CAM_HM6_SET_A_AtoD_2073p5_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_132_LTE_LB42), 41590, 41687, CAM_HM6_SET_A_AtoD_2021p5_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_132_LTE_LB42), 41688, 41729, CAM_HM6_SET_A_AtoD_2151p5_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_132_LTE_LB42), 41730, 41837, CAM_HM6_SET_A_AtoD_2073p5_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_132_LTE_LB42), 41838, 42169, CAM_HM6_SET_A_AtoD_2021p5_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_132_LTE_LB42), 42170, 42223, CAM_HM6_SET_A_AtoD_2151p5_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_132_LTE_LB42), 42224, 42319, CAM_HM6_SET_A_AtoD_2073p5_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_132_LTE_LB42), 42320, 42650, CAM_HM6_SET_A_AtoD_2021p5_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_132_LTE_LB42), 42651, 42717, CAM_HM6_SET_A_AtoD_2151p5_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_132_LTE_LB42), 42718, 42800, CAM_HM6_SET_A_AtoD_2073p5_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_132_LTE_LB42), 42801, 43131, CAM_HM6_SET_A_AtoD_2021p5_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_132_LTE_LB42), 43132, 43210, CAM_HM6_SET_A_AtoD_2151p5_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_132_LTE_LB42), 43211, 43281, CAM_HM6_SET_A_AtoD_2073p5_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_132_LTE_LB42), 43282, 43589, CAM_HM6_SET_A_AtoD_2021p5_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_138_LTE_LB48), 55240, 55281, CAM_HM6_SET_A_AtoD_2021p5_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_138_LTE_LB48), 55282, 55360, CAM_HM6_SET_A_AtoD_2151p5_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_138_LTE_LB48), 55361, 55431, CAM_HM6_SET_A_AtoD_2073p5_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_138_LTE_LB48), 55432, 55763, CAM_HM6_SET_A_AtoD_2021p5_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_138_LTE_LB48), 55764, 55854, CAM_HM6_SET_A_AtoD_2151p5_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_138_LTE_LB48), 55855, 55913, CAM_HM6_SET_A_AtoD_2073p5_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_138_LTE_LB48), 55914, 56244, CAM_HM6_SET_A_AtoD_2021p5_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_138_LTE_LB48), 56245, 56348, CAM_HM6_SET_A_AtoD_2151p5_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_138_LTE_LB48), 56349, 56394, CAM_HM6_SET_A_AtoD_2073p5_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_138_LTE_LB48), 56395, 56725, CAM_HM6_SET_A_AtoD_2021p5_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_138_LTE_LB48), 56726, 56739, CAM_HM6_SET_A_AtoD_2151p5_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_156_LTE_LB66), 66436, 66438, CAM_HM6_SET_A_AtoD_2021p5_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_156_LTE_LB66), 66439, 66489, CAM_HM6_SET_A_AtoD_2073p5_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_156_LTE_LB66), 66490, 66588, CAM_HM6_SET_A_AtoD_2151p5_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_156_LTE_LB66), 66589, 66919, CAM_HM6_SET_A_AtoD_2021p5_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_156_LTE_LB66), 66920, 66983, CAM_HM6_SET_A_AtoD_2073p5_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_156_LTE_LB66), 66984, 67069, CAM_HM6_SET_A_AtoD_2151p5_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_156_LTE_LB66), 67070, 67335, CAM_HM6_SET_A_AtoD_2021p5_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_161_LTE_LB71), 68586, 68598, CAM_HM6_SET_A_AtoD_2021p5_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_161_LTE_LB71), 68599, 68748, CAM_HM6_SET_A_AtoD_2073p5_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_3_LTE, CAM_BAND_161_LTE_LB71), 68749, 68935, CAM_HM6_SET_A_AtoD_2021p5_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_4_TDSCDMA, CAM_BAND_051_TDSCDMA_TD1), 0, 0, CAM_HM6_SET_A_AtoD_2151p5_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_4_TDSCDMA, CAM_BAND_052_TDSCDMA_TD2), 0, 0, CAM_HM6_SET_A_AtoD_2021p5_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_4_TDSCDMA, CAM_BAND_053_TDSCDMA_TD3), 0, 0, CAM_HM6_SET_A_AtoD_2151p5_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_4_TDSCDMA, CAM_BAND_054_TDSCDMA_TD4), 0, 0, CAM_HM6_SET_A_AtoD_2021p5_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_4_TDSCDMA, CAM_BAND_055_TDSCDMA_TD5), 0, 0, CAM_HM6_SET_A_AtoD_2021p5_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_4_TDSCDMA, CAM_BAND_056_TDSCDMA_TD6), 0, 0, CAM_HM6_SET_A_AtoD_2021p5_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_5_CDMA, CAM_BAND_061_CDMA_BC0), 0, 0, CAM_HM6_SET_A_AtoD_2073p5_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_5_CDMA, CAM_BAND_062_CDMA_BC1), 0, 0, CAM_HM6_SET_A_AtoD_2021p5_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_5_CDMA, CAM_BAND_071_CDMA_BC10), 0, 0, CAM_HM6_SET_A_AtoD_2151p5_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_7_NR5G, CAM_BAND_260_NR5G_N005), 173800, 175480, CAM_HM6_SET_A_AtoD_2073p5_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_7_NR5G, CAM_BAND_260_NR5G_N005), 175481, 178780, CAM_HM6_SET_A_AtoD_2021p5_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_7_NR5G, CAM_BAND_263_NR5G_N008), 185000, 185240, CAM_HM6_SET_A_AtoD_2073p5_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_7_NR5G, CAM_BAND_263_NR5G_N008), 185241, 190120, CAM_HM6_SET_A_AtoD_2021p5_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_7_NR5G, CAM_BAND_263_NR5G_N008), 190121, 191980, CAM_HM6_SET_A_AtoD_2073p5_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_7_NR5G, CAM_BAND_267_NR5G_N012), 145800, 146780, CAM_HM6_SET_A_AtoD_2151p5_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_7_NR5G, CAM_BAND_267_NR5G_N012), 146781, 149200, CAM_HM6_SET_A_AtoD_2021p5_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_7_NR5G, CAM_BAND_275_NR5G_N020), 158200, 161240, CAM_HM6_SET_A_AtoD_2021p5_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_7_NR5G, CAM_BAND_275_NR5G_N020), 161241, 164180, CAM_HM6_SET_A_AtoD_2073p5_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_7_NR5G, CAM_BAND_283_NR5G_N028), 151600, 151600, CAM_HM6_SET_A_AtoD_2021p5_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_7_NR5G, CAM_BAND_283_NR5G_N028), 151601, 155820, CAM_HM6_SET_A_AtoD_2073p5_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_7_NR5G, CAM_BAND_283_NR5G_N028), 155821, 156400, CAM_HM6_SET_A_AtoD_2151p5_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_7_NR5G, CAM_BAND_283_NR5G_N028), 156401, 160580, CAM_HM6_SET_A_AtoD_2021p5_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_7_NR5G, CAM_BAND_326_NR5G_N071), 123400, 125940, CAM_HM6_SET_A_AtoD_2073p5_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_7_NR5G, CAM_BAND_326_NR5G_N071), 125941, 127540, CAM_HM6_SET_A_AtoD_2151p5_MHZ },
	{ CAM_RAT_BAND(CAM_RAT_7_NR5G, CAM_BAND_326_NR5G_N071), 127541, 130380, CAM_HM6_SET_A_AtoD_2021p5_MHZ },
};

/* SENSOR READ/WRITE ID */
/* #define IMGSENSOR_WRITE_ID_1 (0x6c) */
/* #define IMGSENSOR_READ_ID_1  (0x6d) */
/* #define IMGSENSOR_WRITE_ID_2 (0x20) */
/* #define IMGSENSOR_READ_ID_2  (0x21) */

extern int iReadRegI2C(u8 *a_pSendData, u16 a_sizeSendData,
				u8 *a_pRecvData, u16 a_sizeRecvData, u16 i2cId);

extern int iWriteRegI2C(u8 *a_pSendData, u16 a_sizeSendData, u16 i2cId);

extern int iReadReg(u16 a_u2Addr, u8 *a_puBuff, u16 i2cId);
extern int iWriteReg(u16 a_u2Addr, u32 a_u4Data, u32 a_u4Bytes, u16 i2cId);
extern void kdSetI2CSpeed(u16 i2cSpeed);


extern int iReadRegI2CTiming(u8 *a_pSendData, u16 a_sizeSendData,
				    u8 *a_pRecvData, u16 a_sizeRecvData,
				    u16 i2cId, u16 timing);


extern int iWriteRegI2CTiming(u8 *a_pSendData, u16 a_sizeSendData,
				     u16 i2cId, u16 timing);

extern int iBurstWriteReg_multi(u8 *pData, u32 bytes, u16 i2cId,
					u16 transfer_length, u16 timing);

#endif				/* _S5KHM6MIPI_SENSOR_H */
