/*
 * Copyright (C) 2019 MediaTek Inc.
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
 *	 S5K5E9YXmipi_Sensor.h
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
#ifndef _S5K5E9YXMIPI_SENSOR_H
#define _S5K5E9YXMIPI_SENSOR_H

enum IMGSENSOR_MODE {
	IMGSENSOR_MODE_INIT,
	IMGSENSOR_MODE_PREVIEW,
	IMGSENSOR_MODE_CAPTURE,
	IMGSENSOR_MODE_VIDEO,
	IMGSENSOR_MODE_HIGH_SPEED_VIDEO,
	IMGSENSOR_MODE_SLIM_VIDEO,
	IMGSENSOR_MODE_CUSTOM1,
};

struct imgsensor_mode_struct {
	kal_uint32 pclk;	//record different mode's pclk
	kal_uint32 linelength;	//record different mode's linelength
	kal_uint32 framelength;	//record different mode's framelength

	kal_uint8 startx;	//record different mode's startx of grabwindow
	kal_uint8 starty;	//record different mode's startx of grabwindow

	//record different mode's width of grabwindow
	kal_uint16 grabwindow_width;
	//record different mode's height of grabwindow
	kal_uint16 grabwindow_height;

	/*
	 * following for MIPIDataLowPwr2HighSpeedSettleDelayCount
	 * by different scenario
	 */
	kal_uint8 mipi_data_lp2hs_settle_dc;

	/* following for GetDefaultFramerateByScenario() */
	kal_uint16 max_framerate;
	kal_uint32 mipi_pixel_rate;
};

/* SENSOR PRIVATE STRUCT FOR VARIABLES*/
struct imgsensor_struct {
	kal_uint8 mirror;	//mirrorflip information

	kal_uint8 sensor_mode;	//record IMGSENSOR_MODE enum value

	kal_uint32 shutter;	//current shutter
	kal_uint16 gain;	//current gain

	kal_uint32 pclk;	//current pclk

	kal_uint32 frame_length;//current framelength
	kal_uint32 line_length;	//current linelength

	kal_uint32 min_frame_length;//current min framelength to max framerate
	kal_uint16 dummy_pixel;	//current dummypixel
	kal_uint16 dummy_line;	//current dummline
	kal_uint16 current_fps;	//current max fps
	kal_bool autoflicker_en;	//record autoflicker enable or disable
	kal_bool test_pattern;	//record test pattern mode or not
	kal_bool enable_secure;	/* run as secure driver or not */
	enum MSDK_SCENARIO_ID_ENUM current_scenario_id;	//current scenario id
	kal_uint8 ihdr_en;	//ihdr enable or disable

	kal_uint8 i2c_write_id;	//record current sensor's i2c write id
};

/* SENSOR PRIVATE STRUCT FOR CONSTANT*/
struct imgsensor_info_struct {
	kal_uint16 sensor_id;	//record sensor id defined in Kd_imgsensor.h
	//#ifdef VENDOR_EDIT
	/*Zhenagjiang.zhu@camera.drv 2017/10/18,modify for different module */
	kal_uint16 module_id;
	//#endif
	kal_uint32 checksum_value;	//checksum value for Camera Auto Test
	//preview scenario relative information
	struct imgsensor_mode_struct pre;
	//capture scenario relative information
	struct imgsensor_mode_struct cap;
	//normal video scenario relative information
	struct imgsensor_mode_struct normal_video;
	//high speed video scenario relative information
	struct imgsensor_mode_struct hs_video;
	//slim video for VT scenario relative information
	struct imgsensor_mode_struct slim_video;
	struct imgsensor_mode_struct custom1;

	kal_uint8 ae_shut_delay_frame;	//shutter delay frame for AE cycle
	//sensor gain delay frame for AE cycle
	kal_uint8 ae_sensor_gain_delay_frame;
	kal_uint8 ae_ispGain_delay_frame;//isp gain delay frame for AE cycle
	kal_uint8 ihdr_support;	//1, support; 0,not support
	kal_uint8 ihdr_le_firstline;	//1,le first ; 0, se first
	kal_uint8 sensor_mode_num;	//support sensor mode num

	kal_uint8 cap_delay_frame;	//enter capture delay frame num
	kal_uint8 pre_delay_frame;	//enter preview delay frame num
	kal_uint8 video_delay_frame;	//enter video delay frame num
	kal_uint8 hs_video_delay_frame;//enter high speed video  delay frame num
	kal_uint8 slim_video_delay_frame;//enter slim video delay frame num
	kal_uint8 custom1_delay_frame;

	kal_uint8 margin;	//sensor framelength & shutter margin
	kal_uint32 min_shutter;	//min shutter
	kal_uint32 min_gain;
	kal_uint32 max_gain;
	kal_uint32 min_gain_iso;
	kal_uint32 gain_step;
	kal_uint32 gain_type;
	//max framelength by sensor register's limitation
	kal_uint32 max_frame_length;

	kal_uint8 isp_driving_current;	//mclk driving current
	kal_uint8 sensor_interface_type;	//sensor_interface_type
	//0,MIPI_OPHY_NCSI2; 1,MIPI_OPHY_CSI2, default is NCSI2,
	//don't modify this para
	kal_uint8 mipi_sensor_type;
	//0, high speed signal auto detect; 1, use settle delay,unit is ns,
	//default is auto detect, don't modify this para
	kal_uint8 mipi_settle_delay_mode;
	kal_uint8 sensor_output_dataformat;
	kal_uint8 mclk;	//mclk value, suggest 24 or 26 for 24Mhz or 26Mhz

	kal_uint8 mipi_lane_num;	//mipi lane num
	//record sensor support all write id addr, only supprt 4,
	//must end with 0xff
	kal_uint8 i2c_addr_table[5];
	kal_uint32 i2c_speed;	//khz
};

/* SENSOR READ/WRITE ID */
//#define IMGSENSOR_WRITE_ID_1 (0x6c)
//#define IMGSENSOR_READ_ID_1  (0x6d)
//#define IMGSENSOR_WRITE_ID_2 (0x20)
//#define IMGSENSOR_READ_ID_2  (0x21)

/* OTP */
#define S5K5E9_OTP_BANK1_MARK       0x01
#define S5K5E9_OTP_BANK2_MARK       0x03
#define S5K5E9_OTP_BANK3_MARK       0x07
#define S5K5E9_OTP_BANK4_MARK       0x0F
#define S5K5E9_OTP_BANK5_MARK       0x1F

#define S5K5E9_OTP_BANK1_START_PAGE 17
#define S5K5E9_OTP_BANK2_START_PAGE 21
#define S5K5E9_OTP_BANK3_START_PAGE 25
#define S5K5E9_OTP_BANK4_START_PAGE 29
#define S5K5E9_OTP_BANK5_START_PAGE 33

// control register
#define S5K5E9_OTP_CONTROL          0x0A00
#define S5K5E9YX_OTP_ERR_FLAG       0x0A01
#define S5K5E9YX_OTP_PAGE_SEL       0x0A02

// data register
#define S5K5E9YX_OTP_DATA_0         0x0A04
#define S5K5E9YX_OTP_DATA_1         0x0A05
#define S5K5E9YX_OTP_DATA_2         0x0A06
#define S5K5E9YX_OTP_DATA_3         0x0A07
#define S5K5E9YX_OTP_DATA_4         0x0A08
#define S5K5E9YX_OTP_DATA_5         0x0A09
#define S5K5E9YX_OTP_DATA_6         0x0A0A
#define S5K5E9YX_OTP_DATA_7         0x0A0B
#define S5K5E9YX_OTP_DATA_8         0x0A0C
#define S5K5E9YX_OTP_DATA_9         0x0A0D
#define S5K5E9YX_OTP_DATA_10        0x0A0E
#define S5K5E9YX_OTP_DATA_11        0x0A0F
#define S5K5E9YX_OTP_DATA_12        0x0A10
#define S5K5E9YX_OTP_DATA_13        0x0A11
#define S5K5E9YX_OTP_DATA_14        0x0A12
#define S5K5E9YX_OTP_DATA_15        0x0A13
#define S5K5E9YX_OTP_DATA_16        0x0A14
#define S5K5E9YX_OTP_DATA_17        0x0A15
#define S5K5E9YX_OTP_DATA_18        0x0A16
#define S5K5E9YX_OTP_DATA_19        0x0A17
#define S5K5E9YX_OTP_DATA_20        0x0A18
#define S5K5E9YX_OTP_DATA_21        0x0A19
#define S5K5E9YX_OTP_DATA_22        0x0A1A
#define S5K5E9YX_OTP_DATA_23        0x0A1B
#define S5K5E9YX_OTP_DATA_24        0x0A1C
#define S5K5E9YX_OTP_DATA_25        0x0A1D
#define S5K5E9YX_OTP_DATA_26        0x0A1E
#define S5K5E9YX_OTP_DATA_27        0x0A1F
#define S5K5E9YX_OTP_DATA_28        0x0A20
#define S5K5E9YX_OTP_DATA_29        0x0A21
#define S5K5E9YX_OTP_DATA_30        0x0A22
#define S5K5E9YX_OTP_DATA_31        0x0A23
#define S5K5E9YX_OTP_DATA_32        0x0A24
#define S5K5E9YX_OTP_DATA_33        0x0A25
#define S5K5E9YX_OTP_DATA_34        0x0A26
#define S5K5E9YX_OTP_DATA_35        0x0A27
#define S5K5E9YX_OTP_DATA_36        0x0A28
#define S5K5E9YX_OTP_DATA_37        0x0A29
#define S5K5E9YX_OTP_DATA_38        0x0A2A
#define S5K5E9YX_OTP_DATA_39        0x0A2B
#define S5K5E9YX_OTP_DATA_40        0x0A2C
#define S5K5E9YX_OTP_DATA_41        0x0A2D
#define S5K5E9YX_OTP_DATA_42        0x0A2E
#define S5K5E9YX_OTP_DATA_43        0x0A2F
#define S5K5E9YX_OTP_DATA_44        0x0A30
#define S5K5E9YX_OTP_DATA_45        0x0A31
#define S5K5E9YX_OTP_DATA_46        0x0A32
#define S5K5E9YX_OTP_DATA_47        0x0A33
#define S5K5E9YX_OTP_DATA_48        0x0A34
#define S5K5E9YX_OTP_DATA_49        0x0A35
#define S5K5E9YX_OTP_DATA_50        0x0A36
#define S5K5E9YX_OTP_DATA_51        0x0A37
#define S5K5E9YX_OTP_DATA_52        0x0A38
#define S5K5E9YX_OTP_DATA_53        0x0A39
#define S5K5E9YX_OTP_DATA_54        0x0A3A
#define S5K5E9YX_OTP_DATA_55        0x0A3B
#define S5K5E9YX_OTP_DATA_56        0x0A3C
#define S5K5E9YX_OTP_DATA_57        0x0A3D
#define S5K5E9YX_OTP_DATA_58        0x0A3E
#define S5K5E9YX_OTP_DATA_59        0x0A3F
#define S5K5E9YX_OTP_DATA_60        0x0A40
#define S5K5E9YX_OTP_DATA_61        0x0A41
#define S5K5E9YX_OTP_DATA_62        0x0A42
#define S5K5E9YX_OTP_DATA_63        0x0A43


extern int iReadRegI2C(u8 *a_pSendData, u16 a_sizeSendData,
		       u8 *a_pRecvData, u16 a_sizeRecvData,
		       u16 i2cId);
extern int iWriteRegI2C(u8 *a_pSendData, u16 a_sizeSendData, u16 i2cId);
extern int iBurstWriteReg_multi(u8 *pData, u32 bytes, u16 i2cId,
				u16 transfer_length, u16 timing);

#ifdef VENDOR_EDIT
extern void register_imgsensor_deviceinfo(char *name, char *version,
					  u8 module_id);
#endif

#endif
