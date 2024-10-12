/* * Copyright (C) 2018 MediaTek Inc.
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

#include <linux/videodev2.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/atomic.h>
#include <linux/types.h>

#include <linux/init.h> //Bug 604664 zhanghao2, add, 2021/08/04/, distinguish 801BA1 version to use 865Mhz mipi clock.


#include "w1hi4821qrearsy_mipiraw_Sensor.h"

#define PFX "w1hi4821qrearsy_camera_sensor"
#define LOG_INF(format, args...)    \
	pr_info(PFX "[%s] " format, __func__, ##args)
#define LOG_DBG(format, args...)    \
	pr_debug(PFX "[%s] " format, __func__, ##args)

//+bug604664 zhouyikuan, add, 2020/12/18,w1hi4821qrearsy_qt eeprom bring up
#define W1HI4821QREARSY_OTP_ENABLE 1

#if W1HI4821QREARSY_OTP_ENABLE
//+bug550253 chenbocheng.wt, modify, 2020/11/16,the w1hi4821qrearsy otp porting
#define EEPROM_BL24SA64D_ID 0xA0

//+bug604664 chenbocheng.wt, add, 2021/02/27, add hi4821q seamless code
#define _I2C_BUF_SIZE 4096

//bool hi4821q_sy_is_seamless;
kal_uint16 hi4821q_sy_i2c_data [_I2C_BUF_SIZE];
//unsigned int hi4821q_sy_size_to_write;
//-bug604664 chenbocheng.wt, add, 2021/02/27, add hi4821q seamless code

static kal_uint16 w1hi4821qrearsy_read_eeprom(kal_uint32 addr)
{
	kal_uint16 get_byte=0;

	char pu_send_cmd[2] = { (char)(addr >> 8), (char)(addr & 0xFF) };
	iReadRegI2C(pu_send_cmd, 2, (u8*)&get_byte, 1, EEPROM_BL24SA64D_ID);

	return get_byte;
}
//-bug550253 chenbocheng.wt, modify, 2020/11/16,the w1hi4821qrearsy otp porting
#endif

//PDAF
#define ENABLE_PDAF 1

#define per_frame 1

#define MULTI_WRITE 1



//+bug604664 chenbocheng.wt, add, 2020/12/30,disable bpgc calib feature for evt version test
#define HI4821Q_XGC_QGC_BPGC_CALIB 1

#define W1HI4821QREARSY_OTP_DUMP 0

#if HI4821Q_XGC_QGC_BPGC_CALIB
//  OTP information setting
#define XGC_BLOCK_X  9
#define XGC_BLOCK_Y  7
#define QGC_BLOCK_X  9
#define QGC_BLOCK_Y  7
#define BPGC_BLOCK_X  13
#define BPGC_BLOCK_Y  11

// SRAM Information
#define SRAM_XGC_START_ADDR_48M     0x43F0
#define SRAM_QGC_START_ADDR_48M     0x4000
#define SRAM_BPGC_START_ADDR_48M     0x5068  //0x8980 (2020.9.14)

u8* bpgc_data_buffer_sy = NULL;
u8* qgc_data_buffer_sy = NULL;
u8* xgc_data_buffer_sy = NULL;

#define XGC_DATA_SIZE 630
#define QGC_DATA_SIZE 1008
#define BPGC_DATA_SIZE 572

#if W1HI4821QREARSY_OTP_DUMP
extern void dumpEEPROMData(int u4Length,u8* pu1Params);
#endif

#endif

static DEFINE_SPINLOCK(imgsensor_drv_lock);

static struct imgsensor_info_struct imgsensor_info = {
	.sensor_id = W1HI4821QREARSY_SENSOR_ID,

	.checksum_value = 0x4f1b1d5e,       //0x6d01485c // Auto Test Mode
	.pre = {
		.pclk = 108000000,				//record different mode's pclk
		.linelength =  902, 			//record different mode's linelength
		.framelength = 3991, 			//record different mode's framelength
		.startx = 0,				    //record different mode's startx of grabwindow
		.starty = 0,					//record different mode's starty of grabwindow
		.grabwindow_width = 4000, 		//record different mode's width of grabwindow
		.grabwindow_height = 3000,		//record different mode's height of grabwindow
		/*	 following for MIPIDataLowPwr2HighSpeedSettleDelayCount by different scenario	*/
		.mipi_data_lp2hs_settle_dc = 85,
		/*	 following for GetDefaultFramerateByScenario()	*/
		.max_framerate = 300,
		.mipi_pixel_rate = 713600000,
	},
	.cap = {
		.pclk = 108000000,
		.linelength = 902,
		.framelength = 3991,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 4000,
		.grabwindow_height = 3000,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 300,
		.mipi_pixel_rate = 713600000,
	},
	// need to setting
	.cap1 = {
		.pclk = 108000000,
		.linelength = 902,
		.framelength = 3991,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 4000,
		.grabwindow_height = 3000,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 300,
		.mipi_pixel_rate = 640000000,
	},
	.normal_video = {
		.pclk = 108000000,
		.linelength = 902,
		.framelength = 3991,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 4000,
		.grabwindow_height = 2256,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 300,
		.mipi_pixel_rate = 560000000,
	},
	.hs_video = {
		.pclk = 108000000,
		.linelength = 745,
		.framelength = 1208,
		.startx = 0,
		.starty = 0,
         .grabwindow_width = 2000,
         .grabwindow_height = 1132,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 1200,
		.mipi_pixel_rate = 713600000,
	},
	.slim_video = {
		.pclk = 108000000,
		.linelength = 745,
		.framelength = 4832,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 1920,
		.grabwindow_height = 1080,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 300,
		.mipi_pixel_rate = 713600000,
	},
	.custom1 = {
		.pclk = 108000000,
		.linelength = 902,
		.framelength = 3991,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 4000,
		.grabwindow_height = 3000,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 300,
		.mipi_pixel_rate = 713600000,
	},
	.custom2 = {
		.pclk = 108000000,
		.linelength = 902,
		.framelength = 3991,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 4000,
		.grabwindow_height = 3000,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 300,
		.mipi_pixel_rate = 713600000,
	},
	.custom3 = {
		.pclk = 108000000,
		.linelength = 1752,
		.framelength = 6164,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 8000,
		.grabwindow_height = 6000,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 100,
		.mipi_pixel_rate = 937600000,
	},
	 .custom4 = {
		.pclk = 108000000,
		.linelength = 745,
		.framelength = 604,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 2000,
		.grabwindow_height = 1132,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 2400,
		.mipi_pixel_rate = 937600000,
	},

	.margin = 8,
	.min_shutter = 8,

	//+for factory mode of photo black screen
	.min_gain = 64,
	.max_gain = 4096,
	.min_gain_iso = 100,
	.exp_step = 1,
	.gain_step = 4,
	.gain_type = 3,
	.max_frame_length = 0xFFFFFFFF,
#if per_frame
	.ae_shut_delay_frame = 0,
	.ae_sensor_gain_delay_frame = 0,
	.ae_ispGain_delay_frame = 2,
#else
	.ae_shut_delay_frame = 0,
	.ae_sensor_gain_delay_frame = 1,
	.ae_ispGain_delay_frame = 2,
#endif

	.ihdr_support = 0,      //1, support; 0,not support
	.ihdr_le_firstline = 0,  //1,le first ; 0, se first
	.sensor_mode_num = 9,      //support sensor mode num

	.cap_delay_frame = 2,
	.pre_delay_frame = 1,
	.video_delay_frame = 2,
	.hs_video_delay_frame = 3,
	.slim_video_delay_frame = 3,
	.custom1_delay_frame = 2,
	.custom2_delay_frame = 2,
	.custom3_delay_frame = 2,
	.custom4_delay_frame = 2,
	.isp_driving_current = ISP_DRIVING_6MA,
	.sensor_interface_type = SENSOR_INTERFACE_TYPE_MIPI,
	.mipi_sensor_type = MIPI_OPHY_NCSI2,
	.mipi_settle_delay_mode = MIPI_SETTLEDELAY_AUTO, //0,MIPI_SETTLEDELAY_AUTO; 1,MIPI_SETTLEDELAY_MANNUAL
	.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_4CELL_HW_BAYER_Gr,
	.mclk = 24,
	.mipi_lane_num = SENSOR_MIPI_4_LANE,
	.i2c_addr_table = {0x40, 0x42,0x44,0x46,0xff},
	.i2c_speed = 1000,
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
static struct SENSOR_WINSIZE_INFO_STRUCT imgsensor_winsize_info[9] = {
	{8032, 6032, 0,   12, 8032, 6008, 4016, 3004, 8, 2, 4000, 3000, 0,0, 4000, 3000},  //preview(4000 x 3000)
	{8032, 6032, 0,   12, 8032, 6008, 4016, 3004, 8, 2, 4000, 3000, 0,0, 4000, 3000},  //capture(4000 x 3000)
	{8032, 6032, 0,   12, 8032, 6008, 4016, 3004, 8, 374, 4000, 2256, 0,0, 4000, 2256},  // VIDEO (4000 x 3000)
	{8032, 6032, 0,   744, 8032, 4544, 2008, 1136, 4, 2, 2000, 1132, 0,0, 2000, 1132},       // hight speed video (1280 x 720)
	{8032, 6032, 0,   848, 8032, 4336, 2008, 1084, 44, 2, 1920, 1080,0,  0, 1920, 1080},      // slim video (1280 x 720)
	{8032, 6032, 0,   12, 8032, 6008, 4016, 3004, 8, 2, 4000, 3000, 0,0, 4000, 3000},     // custom1 (4000 x 3000)
	{8032, 6032, 2016,   1516, 4000, 3000, 4000, 3000, 0, 0, 4000, 3000, 0,0, 4000, 3000},     // custom2 (4000 x 3000)
	{8032, 6032, 0,   14, 8032, 6004, 8032, 6004, 16, 2, 8000, 6000,0,  0, 8000, 6000},     // custom3 (8000x6000)
	{8032, 6032, 0,   744, 8032, 4544, 2008, 1136, 4, 2, 2000, 1132, 0,0, 2000, 1132},	   // custom4 (1280 x 720)
};


#if ENABLE_PDAF

static struct SENSOR_VC_INFO_STRUCT SENSOR_VC_INFO[5]=
{
	/* Preview mode setting */
	 {0x02, //VC_Num
	  0x0a, //VC_PixelNum
	  0x00, //ModeSelect	/* 0:auto 1:direct */
	  0x00, //EXPO_Ratio	/* 1/1, 1/2, 1/4, 1/8 */
	  0x00, //0DValue		/* 0D Value */
	  0x00, //RG_STATSMODE	/* STATS divistion mode 0:16x16  1:8x8	2:4x4  3:1x1 */
	  0x00, 0x2B, 0x1070, 0x0C30,	// VC0 Maybe image data?
	  0x00, 0x00, 0x0000, 0x0000,	// VC1 MVHDR
	  0x00, 0x30, 0x026C, 0x05D0,   // VC2 PDAF
	  0x00, 0x00, 0x0000, 0x0000},	// VC3 ??
	/* Capture mode setting */
	 {0x02, //VC_Num
	  0x0a, //VC_PixelNum
	  0x00, //ModeSelect	/* 0:auto 1:direct */
	  0x00, //EXPO_Ratio	/* 1/1, 1/2, 1/4, 1/8 */
	  0x00, //0DValue		/* 0D Value */
	  0x00, //RG_STATSMODE	/* STATS divistion mode 0:16x16  1:8x8	2:4x4  3:1x1 */
	  0x00, 0x2B, 0x0FA0, 0x0BB8,    // VC0 Maybe image data?
	  0x00, 0x00, 0x0000, 0x0000,	// VC1 MVHDR
	  0x00, 0x30, 0x026C, 0x05D0,   // VC2 PDAF
	  0x00, 0x00, 0x0000, 0x0000},	// VC3 ??
	/* Video mode setting */
	 {0x02, //VC_Num
	  0x0a, //VC_PixelNum
	  0x00, //ModeSelect	/* 0:auto 1:direct */
	  0x00, //EXPO_Ratio	/* 1/1, 1/2, 1/4, 1/8 */
	  0x00, //0DValue		/* 0D Value */
	  0x00, //RG_STATSMODE	/* STATS divistion mode 0:16x16  1:8x8	2:4x4  3:1x1 */
	  0x00, 0x2B, 0x1070, 0x0C30,    // VC0 Maybe image data?
	  0x00, 0x00, 0x0000, 0x0000,	// VC1 MVHDR
	  0x00, 0x30, 0x026C, 0x0460,   // VC2 PDAF
	  0x00, 0x00, 0x0000, 0x0000},	// VC3 ??
	  /* Custom1 mode setting */
	 {0x02, //VC_Num
	  0x0a, //VC_PixelNum
	  0x00, //ModeSelect	/* 0:auto 1:direct */
	  0x00, //EXPO_Ratio	/* 1/1, 1/2, 1/4, 1/8 */
	  0x00, //0DValue		/* 0D Value */
	  0x00, //RG_STATSMODE	/* STATS divistion mode 0:16x16  1:8x8	2:4x4  3:1x1 */
	  0x00, 0x2B, 0x0FA0, 0x0BB8,    // VC0 Maybe image data?
	  0x00, 0x00, 0x0000, 0x0000,	// VC1 MVHDR
	  0x00, 0x30, 0x026C, 0x05D0,   // VC2 PDAF
	  0x00, 0x00, 0x0000, 0x0000 },    // VC3 ??
	 /* Custom2 mode setting */
	 {0x02, //VC_Num
	  0x0a, //VC_PixelNum
	  0x00, //ModeSelect    /* 0:auto 1:direct */
	  0x00, //EXPO_Ratio    /* 1/1, 1/2, 1/4, 1/8 */
	  0x00, //0DValue        /* 0D Value */
	  0x00, //RG_STATSMODE    /* STATS divistion mode 0:16x16 1:8x82:4x4  3:1x1 */
	  0x00, 0x2B, 0x0FA0, 0x0BB8,    // VC0 Maybe image data?
	  0x00, 0x00, 0x0000, 0x0000,    // VC1 MVHDR
	  0x00, 0x30, 0x0140, 0x02E0,   // VC2 PDAF  0x01, 0x30, 0x03E0,0x02E8,
	  0x00, 0x00, 0x0000, 0x0000},    // VC3 ??

};

static struct SET_PD_BLOCK_INFO_T imgsensor_pd_info =
{
	.i4OffsetX	= 16,
	.i4OffsetY	= 12,
	.i4PitchX	= 16,
	.i4PitchY	= 16,
	.i4PairNum	= 8,
	.i4SubBlkW	= 8,
	.i4SubBlkH	= 4,
	.i4BlockNumX = 248,
	.i4BlockNumY = 186,
	.iMirrorFlip = 0,
	.i4PosR =	{
						{16,13}, {24,13}, {20,17}, {28,17},
						{16,21}, {24,21}, {20,25}, {28,25},
				},
	.i4PosL =	{
						{17,13}, {25,13}, {21,17}, {29,17},
						{17,21}, {25,21}, {21,25}, {29,25},
				},
	.i4Crop = { {0, 0}, {0, 0}, {0, 368}, {0, 0}, {0, 0},
				{0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0} },
};
#endif

//+Bug 604664 zhanghao2, add, 2021/08/04/, distinguish 801BA1 version to use 865Mhz mipi clock.
#include "w1hi4821qrearsy_mipiraw_Sensor_lamei.h"

static bool board_is_lamei = false;
static bool sar_get_boardid(void)
{
	char board_id[64];
	char *br_ptr;
	char *br_ptr_e;

	memset(board_id, 0x0, 64);
	br_ptr = strstr(saved_command_line, "androidboot.board_id=");
	if (br_ptr != 0) {
		br_ptr_e = strstr(br_ptr, " ");
		/* get board id */
		if (br_ptr_e != 0) {
			strncpy(board_id, br_ptr + 21,
					br_ptr_e - br_ptr - 21);
			board_id[br_ptr_e - br_ptr - 21] = '\0';
		}

		printk("board_id = %s ", board_id);
        /* if it is LA board */
		if ((!strncmp(board_id, "S96801BA1",strlen("S96801BA1"))) || (!strncmp(board_id, "S96801TA1",strlen("S96801TA1")))) {
		     board_is_lamei = true;
		}
	} else
        board_is_lamei = false;

		return board_is_lamei;
}
//-Bug 604664 zhanghao2, add, 2021/08/04/, distinguish 801BA1 version to use 865Mhz mipi clock.
#if MULTI_WRITE
#define I2C_BUFFER_LEN 1020

static kal_uint16 w1hi4821qrearsy_table_write_cmos_sensor(
					kal_uint16 *para, kal_uint32 len)
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

//+ExtB P220329-06776, liudijin.wt, ADD, 2022/5/12, optimization  hi4821 sensor write qgc and bpgc data.
#define HYNIX_SIZEOF_I2C_BUF 254

static bool skhynix_write_burst_mode(kal_uint16 setting_table[], kal_uint32 nSetSize)
{
    kal_int8 Hynix_i2c_buf[HYNIX_SIZEOF_I2C_BUF] = {0,};
    bool err = false;
    kal_uint32 total_set_count = nSetSize;
    kal_uint16 set_count = 0;
    kal_uint16 buf_count = 0;

    for (set_count = 0; set_count < (total_set_count-2); set_count+=2)
    {
            if (!buf_count)
            {
                Hynix_i2c_buf[buf_count] = setting_table[set_count] >> 8;
                buf_count++;
                Hynix_i2c_buf[buf_count] = setting_table[set_count] & 0xFF;
                buf_count++;
            }

            Hynix_i2c_buf[buf_count] = setting_table[set_count+1] >> 8;
            buf_count++;
            Hynix_i2c_buf[buf_count] = setting_table[set_count+1] & 0xFF;
            buf_count++;

            if((setting_table[set_count+2] == setting_table[set_count] + 2) && // check i2c address. if address is add +2 , add data only.
            (buf_count < HYNIX_SIZEOF_I2C_BUF) )
            {
                continue;
            }

            err = iBurstWriteReg(Hynix_i2c_buf, buf_count, imgsensor.i2c_write_id); // i2c write fuction

            if (err)  return true;

            buf_count = 0;

    }
    LOG_INF("skhynix_write_burst_mode buf_count:%d \n", buf_count);
    if (buf_count > 0) // exception handling. (Last buffer command processing)
    {
        err = iBurstWriteReg(Hynix_i2c_buf, buf_count, imgsensor.i2c_write_id); // i2c write fuction
        if (err)  return true;
        buf_count = 0;
    }

    write_cmos_sensor(setting_table[total_set_count-2], setting_table[total_set_count-1]);
    return false;
}
//-ExtB P220329-06776, liudijin.wt, ADD, 2022/5/12, optimization  hi4821 sensor write qgc and bpgc data.
#if HI4821Q_XGC_QGC_BPGC_CALIB
static kal_uint16 XGC_setting_burst[XGC_DATA_SIZE];
static kal_uint16 QGC_setting_burst[QGC_DATA_SIZE];
static kal_uint16 BPGC_setting_burst[BPGC_DATA_SIZE];
//XGC,QGC,PGC Calibration data are applied here
static void apply_sensor_XGC_QGC_Cali(void)
{
	kal_uint16 idx = 0;
#if W1HI4821QREARSY_OTP_DUMP
	kal_uint16 n = 0;
#endif
	kal_uint16 length1 = 0,length2 = 0;
	kal_uint16 sensor_xgc_addr;
	kal_uint16 sensor_qgc_addr;


	kal_uint16 hi4821_xgc_data;
	kal_uint16 hi4821_qgc_data;


	int i;

	sensor_xgc_addr = SRAM_XGC_START_ADDR_48M + 2;

	write_cmos_sensor(0x0b00,0x0000);
	write_cmos_sensor(0x0318,0x0001); // for EEPROM
	//isp_reg_en = read_cmos_sensor(0x0b04);
	//write_cmos_sensor(0x0b04,isp_reg_en|0x000E); //XGC, QGC, PGC enable
	/***********STEP 1: CALIBRAITON ENABLE**************/
	//isp_reg_en = read_cmos_sensor(0x0b04);
	//write_cmos_sensor(0x0b04,isp_reg_en&0xFBF9); //XGC, QGC, Q2B disable
	//write_cmos_sensor(0x0b04,0xd560);
	LOG_INF("[Start]:apply_sensor_Cali finish\n");
	//XGC data apply
	{
		write_cmos_sensor(0x301c,0x0002);
		for(i = 0; i < XGC_BLOCK_X*XGC_BLOCK_Y*10; i += 2) //9(BLOCK_X)* 7 (BLCOK_Y)*10(channel)
		{
			hi4821_xgc_data = ((((xgc_data_buffer_sy[i+1]) & (0x00ff)) << 8) + ((xgc_data_buffer_sy[i]) & (0x00ff)));

			if(idx == XGC_BLOCK_X*XGC_BLOCK_Y){
				sensor_xgc_addr = SRAM_XGC_START_ADDR_48M;
			}

			else if(idx == XGC_BLOCK_X*XGC_BLOCK_Y*2){
				sensor_xgc_addr += 2;
			}

			else if(idx == XGC_BLOCK_X*XGC_BLOCK_Y*3){
				sensor_xgc_addr = SRAM_XGC_START_ADDR_48M + XGC_BLOCK_X * XGC_BLOCK_Y * 4;
			}

			else if(idx == XGC_BLOCK_X*XGC_BLOCK_Y*4){
				sensor_xgc_addr += 2;
			}

			else{
				//LOG_DBG("sensor_xgc_addr:0x%x,[ERROR]:no XGC data need apply\n",sensor_xgc_addr);
			}
			idx++;
			XGC_setting_burst[2*length1]=sensor_xgc_addr;
			XGC_setting_burst[2*length1+1]=hi4821_xgc_data;
			length1++;
			//write_cmos_sensor(sensor_xgc_addr,hi4821_xgc_data);
			#if W1HI4821QREARSY_OTP_DUMP
				pr_info("sensor_xgc_addr:0x%x,xgc_data_buffer[%d]:0x%x,xgc_data_buffer[%d]:0x%x,hi4821_xgc_data:0x%x\n",
					sensor_xgc_addr,i,xgc_data_buffer_sy[i],i+1,xgc_data_buffer_sy[i+1],hi4821_xgc_data);
			#endif

			sensor_xgc_addr += 4;
		}
		if(length1 == (XGC_DATA_SIZE/2)){
			w1hi4821qrearsy_table_write_cmos_sensor(
			XGC_setting_burst,sizeof(XGC_setting_burst) /sizeof(kal_uint16));
		}
#if W1HI4821QREARSY_OTP_DUMP
		pr_info("================hi4821q XGC burst write data.================\n");
		for(n=0; n<XGC_DATA_SIZE; n=n+2){
			pr_info("hi4821q xgc addr:0x%x,data:0x%x",XGC_setting_burst[n],XGC_setting_burst[n+1]);
			if(n%10 == 9){
				pr_info("\n");
			}
		}
#endif
	}
	//QGC data apply
	{
		write_cmos_sensor(0x301c,0x0002);
		sensor_qgc_addr = SRAM_QGC_START_ADDR_48M;
		for(i = 0; i < QGC_BLOCK_X*QGC_BLOCK_Y*16;i += 2) //9(BLOCK_X)* 7 (BLCOK_Y)*16(channel)
		{
			hi4821_qgc_data = ((((qgc_data_buffer_sy[i+1]) & (0x00ff)) << 8) + ((qgc_data_buffer_sy[i]) & (0x00ff)));
			//write_cmos_sensor(sensor_qgc_addr,hi4821_qgc_data);

			QGC_setting_burst[2*length2]=sensor_qgc_addr;
			QGC_setting_burst[2*length2+1]=hi4821_qgc_data;
			length2++;
			#if W1HI4821QREARSY_OTP_DUMP
				pr_info("sensor_qgc_addr:0x%x,qgc_data_buffer_sy[%d]:0x%x,qgc_data_buffer_sy[%d]:0x%x,hi4821_qgc_data:0x%x\n",
					sensor_qgc_addr,i,qgc_data_buffer_sy[i],i+1,qgc_data_buffer_sy[i+1],hi4821_qgc_data);
			#endif
			sensor_qgc_addr += 2;
		}
		//pr_info("length2:%d,QGC_DATA_SIZE:%d\n",length2,QGC_DATA_SIZE);
		if(length2 == (QGC_DATA_SIZE/2)){
                    //+ExtB P220329-06776, liudijin.wt, ADD, 2022/5/12, optimization  hi4821 sensor write qgc and bpgc data.
                    #if 0
                    w1hi4821qrearsy_table_write_cmos_sensor(
                    QGC_setting_burst,sizeof(QGC_setting_burst) /sizeof(kal_uint16));
                    #endif
                    skhynix_write_burst_mode(QGC_setting_burst,
                    sizeof(QGC_setting_burst)/sizeof(kal_uint16));
                    //-ExtB P220329-06776, liudijin.wt, ADD, 2022/5/12, optimization  hi4821 sensor write qgc and bpgc data.
		}
#if W1HI4821QREARSY_OTP_DUMP
		pr_info("================ hi4821q QGC burst write data.================\n");
		for(n=0; n<QGC_DATA_SIZE; n=n+2)
		{
			pr_info("hi4821q qgc addr:0x%x,data:0x%x   ",QGC_setting_burst[n],QGC_setting_burst[n+1]);
			if(n%10 == 9)
			{
				pr_info("\n");
			}
		}
#endif
	}
	/***********STEP 4: CALIBRAITON ENABLE**************/

	//write_cmos_sensor(0x0b04,0xd566); //XGC, QGC, Q2B disable

	//write_cmos_sensor(0x0b00,0x0000);
	mdelay(10);
	/***********STEP 5: STREAMON**************/
	//write_cmos_sensor(0x0b00,0x0100);
	LOG_INF("[End]:apply_sensor_Cali finish\n");
}

static void apply_sensor_BPGC_Cali(void)
{
	kal_uint16 idx = 0;
#if W1HI4821QREARSY_OTP_DUMP
	kal_uint16 n = 0;
#endif
	kal_uint16 sensor_bpgc_addr;
	kal_uint16 hi4821_bpgc_data;
	kal_uint16 length3 = 0;

	int i;
	//int isp_reg_en;

	write_cmos_sensor(0x0b00,0x0000);
	write_cmos_sensor(0x0318,0x0001); // for EEPROM
	//isp_reg_en = read_cmos_sensor(0x0b04);
	//write_cmos_sensor(0x0b04,isp_reg_en|0x000E); //XGC, QGC, PGC enable
	LOG_INF("[Start]:apply_sensor_Cali BPGC\n");
	//isp_reg_en = (read_cmos_sensor(0x0b04) << 8) | read_cmos_sensor(0x0b05);
	//write_cmos_sensor(0x0b04,isp_reg_en&0xFFEF); //XGC, QGC, BPGC disable

	//BPGC data apply
	{
		idx = 0;
		write_cmos_sensor(0x301c,0x0000);
		sensor_bpgc_addr = SRAM_BPGC_START_ADDR_48M;
		for(i = 0; i < BPGC_BLOCK_X*BPGC_BLOCK_Y*4;i+=2)//13(BLOCK_X)*11(BLCOK_Y)*2(channel)*2bytes
		{
			hi4821_bpgc_data = ((((bpgc_data_buffer_sy[i+1]) & (0x00ff)) <<8) + ((bpgc_data_buffer_sy[i]) & (0x00ff)));
		BPGC_setting_burst[2*length3]=sensor_bpgc_addr;
		BPGC_setting_burst[2*length3+1]=hi4821_bpgc_data;
		length3++;
		//write_cmos_sensor(sensor_bpgc_addr,hi4821_bpgc_data);

			#if W1HI4821QREARSY_OTP_DUMP
			LOG_INF("sensor_bpgc_addr:0x%x,bpgc_data_buffer_sy[%d]:0x%x,bpgc_data_buffer_sy[%d]:0x%x,hi4821_bpgc_data:0x%x\n",
				sensor_bpgc_addr,i,bpgc_data_buffer_sy[i],i+1,bpgc_data_buffer_sy[i+1],hi4821_bpgc_data);
			#endif
			sensor_bpgc_addr += 2;
			idx++;
		}
		//pr_info("length3:%d,BPGC_DATA_SIZE:%d\n",length3,BPGC_DATA_SIZE);
		if(length3 == (BPGC_DATA_SIZE/2)){
                    //+ExtB P220329-06776, liudijin.wt, ADD, 2022/5/12, optimization  hi4821 sensor write qgc and bpgc data.
                    #if 0
                    w1hi4821qrearsy_table_write_cmos_sensor(
                    BPGC_setting_burst,sizeof(BPGC_setting_burst) /sizeof(kal_uint16));
                    #endif
                    skhynix_write_burst_mode(BPGC_setting_burst,
                    sizeof(BPGC_setting_burst)/sizeof(kal_uint16));
                    //-ExtB P220329-06776, liudijin.wt, ADD, 2022/5/12, optimization  hi4821 sensor write qgc and bpgc data.
		}
#if W1HI4821QREARSY_OTP_DUMP
		pr_info("================hi4821q BPGC burst write data.================\n");
		for(n=0; n<BPGC_DATA_SIZE; n=n+2)
		{
			pr_info("hi4821q bpgc addr:0x%x,data:0x%x   ",BPGC_setting_burst[n],BPGC_setting_burst[n+1]);
			if(n%10 == 9)
			{
				pr_info("\n");
			}
		}
#endif
	}

	/***********STEP 4: CALIBRAITON ENABLE**************/
	 //isp_reg_en = (read_cmos_sensor(0x0b04) << 8) | read_cmos_sensor(0x0b05);
	//write_cmos_sensor(0x0b04,isp_reg_en|0x0010); //XGC, QGC, BPGC enable

	//write_cmos_sensor(0x0b00,0x0000);
	mdelay(10);
	/***********STEP 5: STREAMON**************/
	//write_cmos_sensor(0x0b00,0x0100);
	LOG_INF("[End]:apply_sensor_Cali BPGC\n");
}
#endif

static void set_dummy(void)
{
	LOG_DBG("dummyline = %d, dummypixels = %d\n",
		imgsensor.dummy_line, imgsensor.dummy_pixel);
	write_cmos_sensor(0x0210, imgsensor.frame_length & 0xFFFF);
	write_cmos_sensor(0xC3B2,imgsensor .line_length);

}	/*	set_dummy  */

static kal_uint32 return_sensor_id(void)
{
	return (((read_cmos_sensor(0x0716) << 8) | read_cmos_sensor(0x0717)) + 1);

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

	LOG_DBG("shutter = %d, imgsensor.frame_length = %d, imgsensor.min_frame_length = %d\n",
		shutter, imgsensor.frame_length, imgsensor.min_frame_length);


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
			write_cmos_sensor(0x0210, imgsensor.frame_length);

	} else{
			write_cmos_sensor(0x0210, imgsensor.frame_length);
	}


	write_cmos_sensor(0x020C, shutter);

	LOG_DBG("frame_length = %d , shutter = %d \n", imgsensor.frame_length, shutter);


}	/*	write_shutter  */


static void set_shutter_frame_length(kal_uint16 shutter, kal_uint16 frame_length)
{
	unsigned long flags;
	kal_int32 dummy_line = 0;
	kal_uint16 realtime_fps = 0;
	spin_lock_irqsave(&imgsensor_drv_lock, flags);
	imgsensor.shutter = shutter;
	spin_unlock_irqrestore(&imgsensor_drv_lock, flags);
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
	shutter = (shutter > (imgsensor_info.max_frame_length - imgsensor_info.margin)) ? (imgsensor_info.max_frame_length - imgsensor_info.margin) : shutter;

	if (imgsensor.autoflicker_en) {
		realtime_fps = imgsensor.pclk / imgsensor.line_length * 10 / imgsensor.frame_length;
		if(realtime_fps >= 297 && realtime_fps <= 305)
			set_max_framerate(296, 0);
		else if(realtime_fps >= 147 && realtime_fps <= 150)
			set_max_framerate(146, 0);
	} else {
		// Extend frame length
		write_cmos_sensor(0x0210, imgsensor.frame_length & 0xFFFF);
	}

	// Update Shutter
	write_cmos_sensor(0x020C, shutter);
	LOG_DBG("Exit! shutter =%d, framelength =%d\n", shutter,imgsensor.frame_length);
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

	LOG_DBG("set_shutter");
	spin_lock_irqsave(&imgsensor_drv_lock, flags);
	imgsensor.shutter = shutter;
	spin_unlock_irqrestore(&imgsensor_drv_lock, flags);

	write_shutter(shutter);
}	/*	set_shutter */


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
static kal_uint16 gain2reg(kal_uint16 gain)
{
	kal_uint16 reg_gain = 0x0000;
	reg_gain = gain / 4 - 16;

	return (kal_uint16)reg_gain;

}


static kal_uint16 set_gain(kal_uint16 gain)
{
	kal_uint16 reg_gain;

	/* 0x350A[0:1], 0x350B[0:7] AGC real gain */
	/* [0:3] = N meams N /16 X    */
	/* [4:9] = M meams M X         */
	/* Total gain = M + N /16 X   */

	if (gain < BASEGAIN || gain > 64 * BASEGAIN) {
		LOG_INF("Error gain setting");

		if (gain < BASEGAIN)
			gain = BASEGAIN;
		else if (gain > 64 * BASEGAIN)
			gain = 64 * BASEGAIN;
	}

	reg_gain = gain2reg(gain);
	spin_lock(&imgsensor_drv_lock);
	imgsensor.gain = reg_gain;
	spin_unlock(&imgsensor_drv_lock);
	LOG_DBG("gain = %d , reg_gain = 0x%x\n ", gain, reg_gain);

	write_cmos_sensor(0x020A,reg_gain);
	return gain;

}

#if 0
static void ihdr_write_shutter_gain(kal_uint16 le,
				kal_uint16 se, kal_uint16 gain)
{
	LOG_INF("le:0x%x, se:0x%x, gain:0x%x\n", le, se, gain);
	if (imgsensor.ihdr_en) {
		spin_lock(&imgsensor_drv_lock);
		if (le > imgsensor.min_frame_length - imgsensor_info.margin)
			imgsensor.frame_length = le + imgsensor_info.margin;
		else
			imgsensor.frame_length = imgsensor.min_frame_length;
		if (imgsensor.frame_length > imgsensor_info.max_frame_length)
			imgsensor.frame_length =
				imgsensor_info.max_frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (le < imgsensor_info.min_shutter)
			le = imgsensor_info.min_shutter;
		if (se < imgsensor_info.min_shutter)
			se = imgsensor_info.min_shutter;
		// Extend frame length first
		write_cmos_sensor(0x0006, imgsensor.frame_length);
		write_cmos_sensor(0x3502, (le << 4) & 0xFF);
		write_cmos_sensor(0x3501, (le >> 4) & 0xFF);
		write_cmos_sensor(0x3500, (le >> 12) & 0x0F);
		write_cmos_sensor(0x3508, (se << 4) & 0xFF);
		write_cmos_sensor(0x3507, (se >> 4) & 0xFF);
		write_cmos_sensor(0x3506, (se >> 12) & 0x0F);
		set_gain(gain);
	}
}
#endif

#if 1
static void set_mirror_flip(kal_uint8 image_mirror)
{
	int mirrorflip;
	mirrorflip = (read_cmos_sensor(0x0202) << 8) | read_cmos_sensor(0x0203);
	LOG_INF("image_mirror = %d", image_mirror);
	switch (image_mirror) {
	case IMAGE_NORMAL:
		write_cmos_sensor(0x0202, mirrorflip|0x0000);
		break;
	case IMAGE_H_MIRROR:
		write_cmos_sensor(0x0202, mirrorflip|0x0100);
		break;
	case IMAGE_V_MIRROR:
		write_cmos_sensor(0x0202, mirrorflip|0x0200);
		break;
	case IMAGE_HV_MIRROR:
		write_cmos_sensor(0x0202, mirrorflip|0x0300);
		break;
	default:
		LOG_INF("Error image_mirror setting");
		break;
	}

}
#endif
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

#if MULTI_WRITE
kal_uint16 addr_data_pair_init_w1hi4821qrearsy[] = {
	0x0790, 0x0100,       // d2a_analog_en
	0x9000, 0x706F,
	0x9002, 0x0000,
	0x9004, 0x67B1,
	0x9006, 0xA823,
	0x9008, 0x5AA7,
	0x900A, 0x67A5,
	0x900C, 0x8793,
	0x900E, 0x3B87,
	0x9010, 0x2423,
	0x9012, 0x1AF5,
	0x9014, 0x67A5,
	0x9016, 0x8793,
	0x9018, 0x3C47,
	0x901A, 0x2A23,
	0x901C, 0x18F5,
	0x901E, 0x2783,
	0x9020, 0x1805,
	0x9022, 0xD703,
	0x9024, 0x0287,
	0x9026, 0x4789,
	0x9028, 0x0363,
	0x902A, 0x00F7,
	0x902C, 0x8082,
	0x902E, 0x97B7,
	0x9030, 0x0000,
	0x9032, 0x8793,
	0x9034, 0x17A7,
	0x9036, 0x2423,
	0x9038, 0x0AF5,
	0x903A, 0x97B7,
	0x903C, 0x0000,
	0x903E, 0x8793,
	0x9040, 0x0887,
	0x9042, 0x2623,
	0x9044, 0x12F5,
	0x9046, 0x97B7,
	0x9048, 0x0000,
	0x904A, 0x8793,
	0x904C, 0x15E7,
	0x904E, 0xDD7C,
	0x9050, 0x97B7,
	0x9052, 0x0000,
	0x9054, 0x8793,
	0x9056, 0x2907,
	0x9058, 0xCD7C,
	0x905A, 0x97B7,
	0x905C, 0x0000,
	0x905E, 0x8793,
	0x9060, 0x3547,
	0x9062, 0x2A23,
	0x9064, 0x16F5,
	0x9066, 0x2783,
	0x9068, 0x22C5,
	0x906A, 0x4711,
	0x906C, 0x9323,
	0x906E, 0x00E7,
	0x9070, 0x97B7,
	0x9072, 0x0000,
	0x9074, 0x8793,
	0x9076, 0x1CE7,
	0x9078, 0x2023,
	0x907A, 0x16F5,
	0x907C, 0x97B7,
	0x907E, 0x0000,
	0x9080, 0x8793,
	0x9082, 0x1A07,
	0x9084, 0xCD3C,
	0x9086, 0xB75D,
	0x9088, 0x1141,
	0x908A, 0xC606,
	0x908C, 0xC422,
	0x908E, 0x6431,
	0x9090, 0x2783,
	0x9092, 0x5B04,
	0x9094, 0xA783,
	0x9096, 0x22C7,
	0x9098, 0x6709,
	0x909A, 0x5703,
	0x909C, 0x8007,
	0x909E, 0x9023,
	0x90A0, 0x00E7,
	0x90A2, 0x2783,
	0x90A4, 0x5B04,
	0x90A6, 0xA783,
	0x90A8, 0x22C7,
	0x90AA, 0x6705,
	0x90AC, 0x5703,
	0x90AE, 0x3007,
	0x90B0, 0x9123,
	0x90B2, 0x00E7,
	0x90B4, 0x2783,
	0x90B6, 0x5B04,
	0x90B8, 0xA703,
	0x90BA, 0x22C7,
	0x90BC, 0x5683,
	0x90BE, 0x6000,
	0x90C0, 0x1223,
	0x90C2, 0x00D7,
	0x90C4, 0xA783,
	0x90C6, 0x1807,
	0x90C8, 0xA783,
	0x90CA, 0x1547,
	0x90CC, 0x9782,
	0x90CE, 0x2783,
	0x90D0, 0x5B04,
	0x90D2, 0xD683,
	0x90D4, 0x1D07,
	0x90D6, 0x0713,
	0x90D8, 0x3FD0,
	0x90DA, 0x7D63,
	0x90DC, 0x00D7,
	0x90DE, 0xA783,
	0x90E0, 0x22C7,
	0x90E2, 0xD783,
	0x90E4, 0x0027,
	0x90E6, 0xF793,
	0x90E8, 0x0F77,
	0x90EA, 0xE793,
	0x90EC, 0x1007,
	0x90EE, 0x6705,
	0x90F0, 0x1023,
	0x90F2, 0x30F7,
	0x90F4, 0x67B1,
	0x90F6, 0xA703,
	0x90F8, 0x5B07,
	0x90FA, 0x4783,
	0x90FC, 0x18C7,
	0x90FE, 0xE7A9,
	0x9100, 0x0793,
	0x9102, 0x6000,
	0x9104, 0xA683,
	0x9106, 0x0DC7,
	0x9108, 0xA603,
	0x910A, 0x0E07,
	0x910C, 0x6789,
	0x910E, 0xA583,
	0x9110, 0x3047,
	0x9112, 0xA503,
	0x9114, 0x3087,
	0x9116, 0xD7B3,
	0x9118, 0x02C6,
	0x911A, 0x0693,
	0x911C, 0x1010,
	0x911E, 0xE863,
	0x9120, 0x00F6,
	0x9122, 0x87B7,
	0x9124, 0x0040,
	0x9126, 0xF463,
	0x9128, 0x00F5,
	0x912A, 0x6363,
	0x912C, 0x02F5,
	0x912E, 0x2783,
	0x9130, 0x22C7,
	0x9132, 0xD783,
	0x9134, 0x0027,
	0x9136, 0x6685,
	0x9138, 0xF713,
	0x913A, 0x0047,
	0x913C, 0xC783,
	0x913E, 0x3006,
	0x9140, 0x9BED,
	0x9142, 0x8FD9,
	0x9144, 0x8023,
	0x9146, 0x30F6,
	0x9148, 0x40B2,
	0x914A, 0x4422,
	0x914C, 0x0141,
	0x914E, 0x8082,
	0x9150, 0x6705,
	0x9152, 0x4783,
	0x9154, 0x3007,
	0x9156, 0x9BED,
	0x9158, 0x0023,
	0x915A, 0x30F7,
	0x915C, 0xB7F5,
	0x915E, 0x6789,
	0x9160, 0xD703,
	0x9162, 0x2307,
	0x9164, 0x6713,
	0x9166, 0x0017,
	0x9168, 0x9823,
	0x916A, 0x22E7,
	0x916C, 0xD703,
	0x916E, 0x2247,
	0x9170, 0x6713,
	0x9172, 0x0027,
	0x9174, 0x9223,
	0x9176, 0x22E7,
	0x9178, 0x8082,
	0x917A, 0x1141,
	0x917C, 0xC606,
	0x917E, 0x67B1,
	0x9180, 0xA783,
	0x9182, 0x5B07,
	0x9184, 0xA783,
	0x9186, 0x1807,
	0x9188, 0xA783,
	0x918A, 0x0D07,
	0x918C, 0x9782,
	0x918E, 0x6741,
	0x9190, 0x4783,
	0x9192, 0xC007,
	0x9194, 0x9BDD,
	0x9196, 0x0023,
	0x9198, 0xC0F7,
	0x919A, 0x40B2,
	0x919C, 0x0141,
	0x919E, 0x8082,
	0x91A0, 0x1141,
	0x91A2, 0xC606,
	0x91A4, 0x87AA,
	0x91A6, 0x0713,
	0x91A8, 0x0FF0,
	0x91AA, 0x7463,
	0x91AC, 0x00A7,
	0x91AE, 0x0793,
	0x91B0, 0x0FF0,
	0x91B2, 0x6731,
	0x91B4, 0x2703,
	0x91B6, 0x5B07,
	0x91B8, 0x2703,
	0x91BA, 0x1807,
	0x91BC, 0x2703,
	0x91BE, 0x0807,
	0x91C0, 0x9513,
	0x91C2, 0x0107,
	0x91C4, 0x8141,
	0x91C6, 0x9702,
	0x91C8, 0x40B2,
	0x91CA, 0x0141,
	0x91CC, 0x8082,
	0x91CE, 0x1141,
	0x91D0, 0xC606,
	0x91D2, 0x67B1,
	0x91D4, 0xA783,
	0x91D6, 0x5B07,
	0x91D8, 0xA783,
	0x91DA, 0x1807,
	0x91DC, 0xA783,
	0x91DE, 0x1887,
	0x91E0, 0x9782,
	0x91E2, 0x2783,
	0x91E4, 0x3480,
	0x91E6, 0x9713,
	0x91E8, 0x0077,
	0x91EA, 0x4563,
	0x91EC, 0x0007,
	0x91EE, 0x40B2,
	0x91F0, 0x0141,
	0x91F2, 0x8082,
	0x91F4, 0x6521,
	0x91F6, 0x0513,
	0x91F8, 0x0405,
	0x91FA, 0x213D,
	0x91FC, 0xBFCD,
	0x91FE, 0x1141,
	0x9200, 0x0793,
	0x9202, 0x2000,
	0x9204, 0x53D8,
	0x9206, 0xC23A,
	0x9208, 0x5798,
	0x920A, 0xC43A,
	0x920C, 0x57D8,
	0x920E, 0xC63A,
	0x9210, 0x8DA3,
	0x9212, 0x0207,
	0x9214, 0x6741,
	0x9216, 0x0693,
	0x9218, 0xFFF7,
	0x921A, 0x2223,
	0x921C, 0x22D0,
	0x921E, 0x2423,
	0x9220, 0x22D0,
	0x9222, 0x2623,
	0x9224, 0x22D0,
	0x9226, 0x56FD,
	0x9228, 0x2A23,
	0x922A, 0xA2D7,
	0x922C, 0x2E23,
	0x922E, 0xA2D7,
	0x9230, 0x673D,
	0x9232, 0x2603,
	0x9234, 0x6747,
	0x9236, 0x0613,
	0x9238, 0x0C86,
	0x923A, 0x2A23,
	0x923C, 0x60C7,
	0x923E, 0x4605,
	0x9240, 0x1423,
	0x9242, 0x66C7,
	0x9244, 0x4685,
	0x9246, 0xD754,
	0x9248, 0x2023,
	0x924A, 0x0207,
	0x924C, 0x87BE,
	0x924E, 0x2023,
	0x9250, 0x0207,
	0x9252, 0x87BE,
	0x9254, 0x4712,
	0x9256, 0xD3D8,
	0x9258, 0x0713,
	0x925A, 0x2040,
	0x925C, 0x46A2,
	0x925E, 0xD354,
	0x9260, 0x46B2,
	0x9262, 0xD714,
	0x9264, 0x471D,
	0x9266, 0x8DA3,
	0x9268, 0x02E7,
	0x926A, 0x0141,
	0x926C, 0x8082,
	0x926E, 0x4783,
	0x9270, 0x2090,
	0x9272, 0xF793,
	0x9274, 0x0FF7,
	0x9276, 0x4501,
	0x9278, 0xEB99,
	0x927A, 0x67B1,
	0x927C, 0xA783,
	0x927E, 0x5B07,
	0x9280, 0xA783,
	0x9282, 0x22C7,
	0x9284, 0x479C,
	0x9286, 0xC503,
	0x9288, 0x0017,
	0x928A, 0x3533,
	0x928C, 0x00A0,
	0x928E, 0x8082,
	0x9290, 0x1141,
	0x9292, 0xC606,
	0x9294, 0x6799,
	0x9296, 0x8793,
	0x9298, 0x0287,
	0x929A, 0x6719,
	0x929C, 0x0713,
	0x929E, 0x0A87,
	0x92A0, 0xA023,
	0x92A2, 0x0007,
	0x92A4, 0x0791,
	0x92A6, 0x9DE3,
	0x92A8, 0xFEE7,
	0x92AA, 0x67B1,
	0x92AC, 0xA703,
	0x92AE, 0x5B07,
	0x92B0, 0x2703,
	0x92B2, 0x22C7,
	0x92B4, 0x5683,
	0x92B6, 0x0007,
	0x92B8, 0x6709,
	0x92BA, 0x1023,
	0x92BC, 0x80D7,
	0x92BE, 0xA703,
	0x92C0, 0x5B07,
	0x92C2, 0x2703,
	0x92C4, 0x22C7,
	0x92C6, 0x5683,
	0x92C8, 0x0027,
	0x92CA, 0x6705,
	0x92CC, 0x1023,
	0x92CE, 0x30D7,
	0x92D0, 0xA783,
	0x92D2, 0x5B07,
	0x92D4, 0xA783,
	0x92D6, 0x22C7,
	0x92D8, 0xD783,
	0x92DA, 0x0047,
	0x92DC, 0x1023,
	0x92DE, 0x60F0,
	0x92E0, 0x3779,
	0x92E2, 0xC919,
	0x92E4, 0x67B1,
	0x92E6, 0xA783,
	0x92E8, 0x5B07,
	0x92EA, 0xC783,
	0x92EC, 0x18C7,
	0x92EE, 0xE789,
	0x92F0, 0x2783,
	0x92F2, 0x3480,
	0x92F4, 0xCD63,
	0x92F6, 0x0007,
	0x92F8, 0x67B1,
	0x92FA, 0xA783,
	0x92FC, 0x5B07,
	0x92FE, 0xA783,
	0x9300, 0x1807,
	0x9302, 0xA783,
	0x9304, 0x0847,
	0x9306, 0x9782,
	0x9308, 0x40B2,
	0x930A, 0x0141,
	0x930C, 0x8082,
	0x930E, 0x3DC5,
	0x9310, 0x67B1,
	0x9312, 0xA783,
	0x9314, 0x5B07,
	0x9316, 0xA783,
	0x9318, 0x1807,
	0x931A, 0xA783,
	0x931C, 0x0847,
	0x931E, 0x9782,
	0x9320, 0xBFE1,
	0x9322, 0x4218,
	0x9324, 0x429C,
	0x9326, 0x8F99,
	0x9328, 0x0813,
	0x932A, 0x03F0,
	0x932C, 0x6363,
	0x932E, 0x00F8,
	0x9330, 0x8082,
	0x9332, 0x1141,
	0x9334, 0xC606,
	0x9336, 0x8385,
	0x9338, 0x97BA,
	0x933A, 0x9713,
	0x933C, 0x0027,
	0x933E, 0x972A,
	0x9340, 0x4318,
	0x9342, 0x7763,
	0x9344, 0x00B7,
	0x9346, 0xC21C,
	0x9348, 0x3FE9,
	0x934A, 0x40B2,
	0x934C, 0x0141,
	0x934E, 0x8082,
	0x9350, 0xC29C,
	0x9352, 0xBFDD,
	0x9354, 0x1101,
	0x9356, 0xCE06,
	0x9358, 0xCC22,
	0x935A, 0x882E,
	0x935C, 0x85B2,
	0x935E, 0x8436,
	0x9360, 0xC602,
	0x9362, 0xC436,
	0x9364, 0xCE89,
	0x9366, 0x4114,
	0x9368, 0x87AA,
	0x936A, 0x4701,
	0x936C, 0x4605,
	0x936E, 0xEA81,
	0x9370, 0xC390,
	0x9372, 0x0705,
	0x9374, 0x0563,
	0x9376, 0x00E4,
	0x9378, 0x0791,
	0x937A, 0x4394,
	0x937C, 0xDAF5,
	0x937E, 0x67B1,
	0x9380, 0xA783,
	0x9382, 0x5B07,
	0x9384, 0xC783,
	0x9386, 0x1C17,
	0x9388, 0xE391,
	0x938A, 0x85C2,
	0x938C, 0x0034,
	0x938E, 0x0070,
	0x9390, 0x3F49,
	0x9392, 0x67B1,
	0x9394, 0xA783,
	0x9396, 0x5B07,
	0x9398, 0xC783,
	0x939A, 0x1C17,
	0x939C, 0xEF81,
	0x939E, 0xC422,
	0x93A0, 0x5783,
	0x93A2, 0x0081,
	0x93A4, 0x07C2,
	0x93A6, 0x5503,
	0x93A8, 0x00C1,
	0x93AA, 0x8D5D,
	0x93AC, 0x40F2,
	0x93AE, 0x4462,
	0x93B0, 0x6105,
	0x93B2, 0x8082,
	0x93B4, 0xC602,
	0x93B6, 0xB7ED,
	0x93B8, 0x9774,
	0x93BA, 0x0000,
	0x93BC, 0x97CC,
	0x93BE, 0x0000,
	0x93C0, 0x97A0,
	0x93C2, 0x0000,
	0x93C4, 0x9870,
	0x93C6, 0x0000,
	0x93C8, 0x99C8,
	0x93CA, 0x0000,
	0x93CC, 0x9920,
	0x93CE, 0x0000,
	0x93D0, 0x07B3,
	0x93D2, 0x02B5,
	0x93D4, 0x1533,
	0x93D6, 0x02B5,
	0x93D8, 0x83D1,
	0x93DA, 0x0532,
	0x93DC, 0x8D5D,
	0x93DE, 0x8082,
	0x93E0, 0x5713,
	0x93E2, 0x01F5,
	0x93E4, 0xC319,
	0x93E6, 0x0533,
	0x93E8, 0x40A0,
	0x93EA, 0x4785,
	0x93EC, 0x97B3,
	0x93EE, 0x00B7,
	0x93F0, 0x811D,
	0x93F2, 0x17FD,
	0x93F4, 0x7363,
	0x93F6, 0x00F5,
	0x93F8, 0x87AA,
	0x93FA, 0x1533,
	0x93FC, 0x00B7,
	0x93FE, 0x8D5D,
	0x9400, 0x8082,
	0x9402, 0x7159,
	0x9404, 0xCAD6,
	0x9406, 0x0A93,
	0x9408, 0x02C1,
	0x940A, 0xD4A2,
	0x940C, 0xD2A6,
	0x940E, 0xD0CA,
	0x9410, 0xCECE,
	0x9412, 0xCCD2,
	0x9414, 0xC8DA,
	0x9416, 0xC6DE,
	0x9418, 0xD686,
	0x941A, 0x89AA,
	0x941C, 0x84AE,
	0x941E, 0x8432,
	0x9420, 0x8A36,
	0x9422, 0x0913,
	0x9424, 0x0086,
	0x9426, 0x0B93,
	0x9428, 0x0306,
	0x942A, 0x8B56,
	0x942C, 0x2503,
	0x942E, 0x0009,
	0x9430, 0x85D2,
	0x9432, 0x0921,
	0x9434, 0x3F71,
	0x9436, 0x2783,
	0x9438, 0xFFC9,
	0x943A, 0x0B11,
	0x943C, 0x953E,
	0x943E, 0x2E23,
	0x9440, 0xFEAB,
	0x9442, 0x15E3,
	0x9444, 0xFF79,
	0x9446, 0x5808,
	0x9448, 0x85D2,
	0x944A, 0x0B37,
	0x944C, 0xFC00,
	0x944E, 0x3749,
	0x9450, 0x2903,
	0x9452, 0x0344,
	0x9454, 0x85D2,
	0x9456, 0x992A,
	0x9458, 0x5C08,
	0x945A, 0xC64A,
	0x945C, 0x3F95,
	0x945E, 0x2A03,
	0x9460, 0x03C4,
	0x9462, 0x55C2,
	0x9464, 0xCE02,
	0x9466, 0x9A2A,
	0x9468, 0x95DA,
	0x946A, 0x0533,
	0x946C, 0x4149,
	0x946E, 0xC852,
	0x9470, 0x3785,
	0x9472, 0x5952,
	0x9474, 0x842A,
	0x9476, 0xD02A,
	0x9478, 0x05B3,
	0x947A, 0x0169,
	0x947C, 0x8552,
	0x947E, 0x3F89,
	0x9480, 0x55E2,
	0x9482, 0x9522,
	0x9484, 0x0437,
	0x9486, 0x0010,
	0x9488, 0x0933,
	0x948A, 0x40B9,
	0x948C, 0x4433,
	0x948E, 0x0289,
	0x9490, 0x95DA,
	0x9492, 0x4533,
	0x9494, 0x0285,
	0x9496, 0xCA2A,
	0x9498, 0x0533,
	0x949A, 0x40A0,
	0x949C, 0x3F15,
	0x949E, 0xD22A,
	0x94A0, 0xCC02,
	0x94A2, 0xD402,
	0x94A4, 0x85A6,
	0x94A6, 0x8693,
	0x94A8, 0x00A4,
	0x94AA, 0x0737,
	0x94AC, 0x0010,
	0x94AE, 0xA783,
	0x94B0, 0x000A,
	0x94B2, 0x0589,
	0x94B4, 0x0A91,
	0x94B6, 0xC7B3,
	0x94B8, 0x02E7,
	0x94BA, 0x9F23,
	0x94BC, 0xFEF5,
	0x94BE, 0x98E3,
	0x94C0, 0xFED5,
	0x94C2, 0x0064,
	0x94C4, 0x8913,
	0x94C6, 0x0089,
	0x94C8, 0x844E,
	0x94CA, 0x4088,
	0x94CC, 0x45B5,
	0x94CE, 0x0409,
	0x94D0, 0x3F01,
	0x94D2, 0x0542,
	0x94D4, 0x8141,
	0x94D6, 0x1F23,
	0x94D8, 0xFEA4,
	0x94DA, 0x0491,
	0x94DC, 0x17E3,
	0x94DE, 0xFF24,
	0x94E0, 0x0864,
	0x94E2, 0x09E1,
	0x94E4, 0x4088,
	0x94E6, 0x45C5,
	0x94E8, 0x0411,
	0x94EA, 0x3DDD,
	0x94EC, 0x2E23,
	0x94EE, 0xFEA4,
	0x94F0, 0x0491,
	0x94F2, 0x99E3,
	0x94F4, 0xFE89,
	0x94F6, 0x50B6,
	0x94F8, 0x5426,
	0x94FA, 0x5496,
	0x94FC, 0x5906,
	0x94FE, 0x49F6,
	0x9500, 0x4A66,
	0x9502, 0x4AD6,
	0x9504, 0x4B46,
	0x9506, 0x4BB6,
	0x9508, 0x6165,
	0x950A, 0x8082,
	0x950C, 0x7159,
	0x950E, 0xC8DA,
	0x9510, 0x0B13,
	0x9512, 0x02C1,
	0x9514, 0xD4A2,
	0x9516, 0xD2A6,
	0x9518, 0xD0CA,
	0x951A, 0xCECE,
	0x951C, 0xCCD2,
	0x951E, 0xCAD6,
	0x9520, 0xC6DE,
	0x9522, 0xC4E2,
	0x9524, 0xD686,
	0x9526, 0x89AA,
	0x9528, 0x84AE,
	0x952A, 0x8432,
	0x952C, 0x8AB6,
	0x952E, 0x8A3A,
	0x9530, 0x0913,
	0x9532, 0x0086,
	0x9534, 0x0C13,
	0x9536, 0x0306,
	0x9538, 0x8BDA,
	0x953A, 0x2503,
	0x953C, 0x0009,
	0x953E, 0x85D6,
	0x9540, 0x0921,
	0x9542, 0x3579,
	0x9544, 0x2783,
	0x9546, 0xFFC9,
	0x9548, 0x0B91,
	0x954A, 0x953E,
	0x954C, 0xAE23,
	0x954E, 0xFEAB,
	0x9550, 0x15E3,
	0x9552, 0xFF89,
	0x9554, 0x5808,
	0x9556, 0x85D6,
	0x9558, 0x0BB7,
	0x955A, 0xFC00,
	0x955C, 0x3D95,
	0x955E, 0x2903,
	0x9560, 0x0344,
	0x9562, 0x85D6,
	0x9564, 0x992A,
	0x9566, 0x5C08,
	0x9568, 0xC64A,
	0x956A, 0x359D,
	0x956C, 0x2A83,
	0x956E, 0x03C4,
	0x9570, 0x55C2,
	0x9572, 0xCE02,
	0x9574, 0x9AAA,
	0x9576, 0x95DE,
	0x9578, 0x0533,
	0x957A, 0x4159,
	0x957C, 0xC856,
	0x957E, 0x3D89,
	0x9580, 0x5952,
	0x9582, 0x842A,
	0x9584, 0xD02A,
	0x9586, 0x05B3,
	0x9588, 0x0179,
	0x958A, 0x8556,
	0x958C, 0x3591,
	0x958E, 0x55E2,
	0x9590, 0x9522,
	0x9592, 0x0437,
	0x9594, 0x0010,
	0x9596, 0x0933,
	0x9598, 0x40B9,
	0x959A, 0x4433,
	0x959C, 0x0289,
	0x959E, 0x95DE,
	0x95A0, 0x4533,
	0x95A2, 0x0285,
	0x95A4, 0xCA2A,
	0x95A6, 0x0533,
	0x95A8, 0x40A0,
	0x95AA, 0x351D,
	0x95AC, 0xD22A,
	0x95AE, 0xCC02,
	0x95B0, 0xD402,
	0x95B2, 0x85A6,
	0x95B4, 0x8693,
	0x95B6, 0x00A4,
	0x95B8, 0x0737,
	0x95BA, 0x0010,
	0x95BC, 0x2783,
	0x95BE, 0x000B,
	0x95C0, 0x0589,
	0x95C2, 0x0B11,
	0x95C4, 0xC7B3,
	0x95C6, 0x02E7,
	0x95C8, 0x9F23,
	0x95CA, 0xFEF5,
	0x95CC, 0x98E3,
	0x95CE, 0xFED5,
	0x95D0, 0x0064,
	0x95D2, 0x8913,
	0x95D4, 0x0089,
	0x95D6, 0x844E,
	0x95D8, 0x4088,
	0x95DA, 0x85D2,
	0x95DC, 0x0409,
	0x95DE, 0x3BCD,
	0x95E0, 0xC088,
	0x95E2, 0x45B5,
	0x95E4, 0x3BF5,
	0x95E6, 0x0542,
	0x95E8, 0x8141,
	0x95EA, 0x1F23,
	0x95EC, 0xFEA4,
	0x95EE, 0x0491,
	0x95F0, 0x14E3,
	0x95F2, 0xFF24,
	0x95F4, 0x0864,
	0x95F6, 0x09E1,
	0x95F8, 0x4088,
	0x95FA, 0x85D2,
	0x95FC, 0x0411,
	0x95FE, 0x3BC9,
	0x9600, 0xC088,
	0x9602, 0x45C5,
	0x9604, 0x3BF1,
	0x9606, 0x2E23,
	0x9608, 0xFEA4,
	0x960A, 0x0491,
	0x960C, 0x96E3,
	0x960E, 0xFE89,
	0x9610, 0x50B6,
	0x9612, 0x5426,
	0x9614, 0x5496,
	0x9616, 0x5906,
	0x9618, 0x49F6,
	0x961A, 0x4A66,
	0x961C, 0x4AD6,
	0x961E, 0x4B46,
	0x9620, 0x4BB6,
	0x9622, 0x4C26,
	0x9624, 0x6165,
	0x9626, 0x8082,
	0x9628, 0x6789,
	0x962A, 0xD703,
	0x962C, 0xE0E7,
	0x962E, 0x07B7,
	0x9630, 0x4000,
	0x9632, 0x1141,
	0x9634, 0xC7B3,
	0x9636, 0x02E7,
	0x9638, 0xC422,
	0x963A, 0xC606,
	0x963C, 0x0737,
	0x963E, 0x0010,
	0x9640, 0x842A,
	0x9642, 0x8263,
	0x9644, 0x02E7,
	0x9646, 0x4118,
	0x9648, 0x5363,
	0x964A, 0x00F7,
	0x964C, 0x873E,
	0x964E, 0x4054,
	0x9650, 0x5363,
	0x9652, 0x00D7,
	0x9654, 0x86BA,
	0x9656, 0x6585,
	0x9658, 0x6509,
	0x965A, 0x8622,
	0x965C, 0x8593,
	0x965E, 0x4C45,
	0x9660, 0x0513,
	0x9662, 0x9805,
	0x9664, 0x3B79,
	0x9666, 0x6789,
	0x9668, 0xD703,
	0x966A, 0xE107,
	0x966C, 0x07B7,
	0x966E, 0x4000,
	0x9670, 0xC7B3,
	0x9672, 0x02E7,
	0x9674, 0x0737,
	0x9676, 0x0010,
	0x9678, 0x8663,
	0x967A, 0x02E7,
	0x967C, 0x4038,
	0x967E, 0x5363,
	0x9680, 0x00F7,
	0x9682, 0x873E,
	0x9684, 0x4074,
	0x9686, 0x5363,
	0x9688, 0x00D7,
	0x968A, 0x86BA,
	0x968C, 0x0613,
	0x968E, 0x0404,
	0x9690, 0x4422,
	0x9692, 0x40B2,
	0x9694, 0x6585,
	0x9696, 0x6509,
	0x9698, 0x8593,
	0x969A, 0x4CE5,
	0x969C, 0x0513,
	0x969E, 0x9C05,
	0x96A0, 0x0141,
	0x96A2, 0xB385,
	0x96A4, 0x40B2,
	0x96A6, 0x4422,
	0x96A8, 0x0141,
	0x96AA, 0x8082,
	0x96AC, 0x67B1,
	0x96AE, 0xA783,
	0x96B0, 0x5B07,
	0x96B2, 0x1141,
	0x96B4, 0xC422,
	0x96B6, 0xD403,
	0x96B8, 0x2067,
	0x96BA, 0x07B7,
	0x96BC, 0xF900,
	0x96BE, 0xC226,
	0x96C0, 0x0452,
	0x96C2, 0x943E,
	0x96C4, 0x0793,
	0x96C6, 0x0800,
	0x96C8, 0x4433,
	0x96CA, 0x02F4,
	0x96CC, 0x6789,
	0x96CE, 0xD703,
	0x96D0, 0xE0E7,
	0x96D2, 0x07B7,
	0x96D4, 0x4000,
	0x96D6, 0xC606,
	0x96D8, 0x84AA,
	0x96DA, 0xC7B3,
	0x96DC, 0x02E7,
	0x96DE, 0x0737,
	0x96E0, 0x0010,
	0x96E2, 0x8363,
	0x96E4, 0x02E7,
	0x96E6, 0x4118,
	0x96E8, 0x5363,
	0x96EA, 0x00F7,
	0x96EC, 0x873E,
	0x96EE, 0x40D4,
	0x96F0, 0x5363,
	0x96F2, 0x00D7,
	0x96F4, 0x86BA,
	0x96F6, 0x6585,
	0x96F8, 0x6509,
	0x96FA, 0x8722,
	0x96FC, 0x8626,
	0x96FE, 0x8593,
	0x9700, 0x4C45,
	0x9702, 0x0513,
	0x9704, 0x9805,
	0x9706, 0x3519,
	0x9708, 0x6789,
	0x970A, 0xD703,
	0x970C, 0xE107,
	0x970E, 0x07B7,
	0x9710, 0x4000,
	0x9712, 0xC7B3,
	0x9714, 0x02E7,
	0x9716, 0x0737,
	0x9718, 0x0010,
	0x971A, 0x8863,
	0x971C, 0x02E7,
	0x971E, 0x40B8,
	0x9720, 0x5363,
	0x9722, 0x00F7,
	0x9724, 0x873E,
	0x9726, 0x40F4,
	0x9728, 0x5363,
	0x972A, 0x00D7,
	0x972C, 0x86BA,
	0x972E, 0x8722,
	0x9730, 0x4422,
	0x9732, 0x40B2,
	0x9734, 0x8613,
	0x9736, 0x0404,
	0x9738, 0x4492,
	0x973A, 0x6585,
	0x973C, 0x6509,
	0x973E, 0x8593,
	0x9740, 0x4CE5,
	0x9742, 0x0513,
	0x9744, 0x9C05,
	0x9746, 0x0141,
	0x9748, 0xB3D1,
	0x974A, 0x40B2,
	0x974C, 0x4422,
	0x974E, 0x4492,
	0x9750, 0x0141,
	0x9752, 0x8082,
	0x9754, 0x0204,
	0x9756, 0x0104,
	0x9758, 0x0107,
	0x975A, 0x0205,
	0x975C, 0x0104,
	0x975E, 0x0200,
	0x9760, 0xFF00,
	0x9762, 0x7410,
	0x9764, 0x010C,
	0x9766, 0x030C,
	0x9768, 0x010F,
	0x976A, 0x020D,
	0x976C, 0x010C,
	0x976E, 0x0A00,
	0x9770, 0xFFFF,
	0x9772, 0x0000,
	0x9774, 0x9754,
	0x9776, 0x0000,
	0x9778, 0x2C82,
	0x977A, 0x2D8A,
	0x977C, 0x03FF,
	0x977E, 0x0000,
	0x9780, 0x0204,
	0x9782, 0x0104,
	0x9784, 0x0107,
	0x9786, 0x0205,
	0x9788, 0x0104,
	0x978A, 0x0200,
	0x978C, 0x6510,
	0x978E, 0x0100,
	0x9790, 0xCF0C,
	0x9792, 0x030C,
	0x9794, 0x010F,
	0x9796, 0x020D,
	0x9798, 0x010C,
	0x979A, 0x0A00,
	0x979C, 0xFFFF,
	0x979E, 0x0000,
	0x97A0, 0x9780,
	0x97A2, 0x0000,
	0x97A4, 0x2C82,
	0x97A6, 0x2D8A,
	0x97A8, 0x03FF,
	0x97AA, 0x0000,
	0x97AC, 0x0204,
	0x97AE, 0x0104,
	0x97B0, 0x0107,
	0x97B2, 0x0205,
	0x97B4, 0x0104,
	0x97B6, 0x0200,
	0x97B8, 0x5110,
	0x97BA, 0x0100,
	0x97BC, 0xCD0C,
	0x97BE, 0x030C,
	0x97C0, 0x010F,
	0x97C2, 0x020D,
	0x97C4, 0x010C,
	0x97C6, 0x0A00,
	0x97C8, 0xFFFF,
	0x97CA, 0x0000,
	0x97CC, 0x97AC,
	0x97CE, 0x0000,
	0x97D0, 0x2C82,
	0x97D2, 0x2D8A,
	0x97D4, 0x03FF,
	0x97D6, 0x0000,
	0x97D8, 0x30B0,
	0x97DA, 0x0008,
	0x97DC, 0x01E7,
	0x97DE, 0x2267,
	0x97E0, 0x0227,
	0x97E2, 0x0188,
	0x97E4, 0x7001,
	0x97E6, 0x19C6,
	0x97E8, 0x5E00,
	0x97EA, 0x6780,
	0x97EC, 0x01F2,
	0x97EE, 0x209F,
	0x97F0, 0x701A,
	0x97F2, 0x8070,
	0x97F4, 0xF3E6,
	0x97F6, 0x0001,
	0x97F8, 0x0000,
	0x97FA, 0x7025,
	0x97FC, 0x19CF,
	0x97FE, 0x7008,
	0x9800, 0x19CF,
	0x9802, 0x0A16,
	0x9804, 0x0017,
	0x9806, 0x001C,
	0x9808, 0x0000,
	0x980A, 0x03D9,
	0x980C, 0x0B64,
	0x980E, 0x00E2,
	0x9810, 0x00E2,
	0x9812, 0x24CF,
	0x9814, 0x00E4,
	0x9816, 0x00E4,
	0x9818, 0x001D,
	0x981A, 0x0001,
	0x981C, 0x20CC,
	0x981E, 0x7077,
	0x9820, 0x2FCF,
	0x9822, 0x701D,
	0x9824, 0x19CF,
	0x9826, 0x00A4,
	0x9828, 0x0080,
	0x982A, 0x5640,
	0x982C, 0x6701,
	0x982E, 0x03D4,
	0x9830, 0x7001,
	0x9832, 0x19CF,
	0x9834, 0x7000,
	0x9836, 0x4EB2,
	0x9838, 0x0804,
	0x983A, 0x7003,
	0x983C, 0x1289,
	0x983E, 0x004B,
	0x9840, 0x2123,
	0x9842, 0x7011,
	0x9844, 0x0FE4,
	0x9846, 0x00E2,
	0x9848, 0x00E2,
	0x984A, 0x0121,
	0x984C, 0x00E4,
	0x984E, 0x00E4,
	0x9850, 0x051D,
	0x9852, 0x2004,
	0x9854, 0x20CC,
	0x9856, 0x7157,
	0x9858, 0x2FCF,
	0x985A, 0x704B,
	0x985C, 0x0FE4,
	0x985E, 0x0082,
	0x9860, 0xC7A0,
	0x9862, 0xC061,
	0x9864, 0x0301,
	0x9866, 0x0000,
	0x9868, 0x0045,
	0x986A, 0x0003,
	0x986C, 0x4080,
	0x986E, 0x0000,
	0x9870, 0x987C,
	0x9872, 0x0000,
	0x9874, 0x3818,
	0x9876, 0x0001,
	0x9878, 0x0001,
	0x987A, 0x0000,
	0x987C, 0x0000,
	0x987E, 0x004B,
	0x9880, 0x97D8,
	0x9882, 0x0000,
	0x9884, 0x30B0,
	0x9886, 0x0008,
	0x9888, 0x01E7,
	0x988A, 0x2267,
	0x988C, 0x0227,
	0x988E, 0x0188,
	0x9890, 0x7001,
	0x9892, 0x19C6,
	0x9894, 0x5E00,
	0x9896, 0x6780,
	0x9898, 0x01F2,
	0x989A, 0x209F,
	0x989C, 0x701A,
	0x989E, 0x8070,
	0x98A0, 0xF3E6,
	0x98A2, 0x0001,
	0x98A4, 0x0000,
	0x98A6, 0x7025,
	0x98A8, 0x19CF,
	0x98AA, 0x7008,
	0x98AC, 0x19CF,
	0x98AE, 0x05DC,
	0x98B0, 0x0196,
	0x98B2, 0x0017,
	0x98B4, 0x0040,
	0x98B6, 0x0419,
	0x98B8, 0x0664,
	0x98BA, 0x00E2,
	0x98BC, 0x00E2,
	0x98BE, 0x24CF,
	0x98C0, 0x00E4,
	0x98C2, 0x00E4,
	0x98C4, 0x001D,
	0x98C6, 0x0001,
	0x98C8, 0x20CC,
	0x98CA, 0x7083,
	0x98CC, 0x19CF,
	0x98CE, 0x5E62,
	0x98D0, 0x6700,
	0x98D2, 0x0796,
	0x98D4, 0x0080,
	0x98D6, 0x5640,
	0x98D8, 0x6701,
	0x98DA, 0x03D4,
	0x98DC, 0x7001,
	0x98DE, 0x19CF,
	0x98E0, 0x7000,
	0x98E2, 0x4EB2,
	0x98E4, 0x0804,
	0x98E6, 0x7003,
	0x98E8, 0x1289,
	0x98EA, 0x004B,
	0x98EC, 0x2123,
	0x98EE, 0x7003,
	0x98F0, 0x0FE4,
	0x98F2, 0x00E2,
	0x98F4, 0x00E2,
	0x98F6, 0x0121,
	0x98F8, 0x00E4,
	0x98FA, 0x00E4,
	0x98FC, 0x08DD,
	0x98FE, 0x2004,
	0x9900, 0x20CC,
	0x9902, 0x7166,
	0x9904, 0xC064,
	0x9906, 0x9467,
	0x9908, 0x0327,
	0x990A, 0x0000,
	0x990C, 0x0164,
	0x990E, 0x0082,
	0x9910, 0x5400,
	0x9912, 0x618F,
	0x9914, 0x01C0,
	0x9916, 0x0045,
	0x9918, 0x7000,
	0x991A, 0x4088,
	0x991C, 0x0003,
	0x991E, 0x0000,
	0x9920, 0x992C,
	0x9922, 0x0000,
	0x9924, 0x3818,
	0x9926, 0x0001,
	0x9928, 0x0001,
	0x992A, 0x0000,
	0x992C, 0x0000,
	0x992E, 0x004D,
	0x9930, 0x9884,
	0x9932, 0x0000,
	0x9934, 0x30B0,
	0x9936, 0x0008,
	0x9938, 0x01E7,
	0x993A, 0x2267,
	0x993C, 0x0227,
	0x993E, 0x0188,
	0x9940, 0x7001,
	0x9942, 0x19C6,
	0x9944, 0x5E00,
	0x9946, 0x6780,
	0x9948, 0x01F2,
	0x994A, 0x209F,
	0x994C, 0x701A,
	0x994E, 0x8070,
	0x9950, 0xF3E6,
	0x9952, 0x0001,
	0x9954, 0x0000,
	0x9956, 0x7025,
	0x9958, 0x19CF,
	0x995A, 0x7008,
	0x995C, 0x19CF,
	0x995E, 0x0A16,
	0x9960, 0x0017,
	0x9962, 0x001C,
	0x9964, 0x0000,
	0x9966, 0x03D9,
	0x9968, 0x0B64,
	0x996A, 0x00E2,
	0x996C, 0x00E2,
	0x996E, 0x24CF,
	0x9970, 0x00E4,
	0x9972, 0x00E4,
	0x9974, 0x001D,
	0x9976, 0x0001,
	0x9978, 0x20CC,
	0x997A, 0x7050,
	0x997C, 0x19CF,
	0x997E, 0x21CF,
	0x9980, 0x0024,
	0x9982, 0x0080,
	0x9984, 0x5640,
	0x9986, 0x6701,
	0x9988, 0x03D4,
	0x998A, 0x7001,
	0x998C, 0x19CF,
	0x998E, 0x7000,
	0x9990, 0x4EB2,
	0x9992, 0x0804,
	0x9994, 0x7003,
	0x9996, 0x1289,
	0x9998, 0x004B,
	0x999A, 0x2123,
	0x999C, 0x7011,
	0x999E, 0x0FE4,
	0x99A0, 0x00E2,
	0x99A2, 0x00E2,
	0x99A4, 0x0121,
	0x99A6, 0x00E4,
	0x99A8, 0x00E4,
	0x99AA, 0x04DD,
	0x99AC, 0x2004,
	0x99AE, 0x20CC,
	0x99B0, 0x70CF,
	0x99B2, 0x2FCF,
	0x99B4, 0x0064,
	0x99B6, 0x0082,
	0x99B8, 0xC7A0,
	0x99BA, 0xC061,
	0x99BC, 0x0301,
	0x99BE, 0x0000,
	0x99C0, 0x0045,
	0x99C2, 0x0003,
	0x99C4, 0x4080,
	0x99C6, 0x0000,
	0x99C8, 0x99D4,
	0x99CA, 0x0000,
	0x99CC, 0x3818,
	0x99CE, 0x0001,
	0x99D0, 0x0001,
	0x99D2, 0x0000,
	0x99D4, 0x0000,
	0x99D6, 0x0049,
	0x99D8, 0x9934,
	0x99DA, 0x0000,
	0xC290, 0xAA55,
	0xC4B0, 0x0204,
	0xC4B2, 0x0206,
	0xC4B4, 0x0226,
	0xC4B6, 0x022A,
	0xC4B8, 0x022C,
	0xC4BA, 0x022E,
	0xC4BC, 0x0244,
	0xC4BE, 0x0246,
	0xC4C0, 0x0248,
	0xC4C2, 0x024A,
	0xC4C4, 0x024C,
	0xC4C6, 0x024E,
	0xC4C8, 0x0250,
	0xC4CA, 0x0252,
	0xC4CC, 0x0254,
	0xC4CE, 0x0256,
	0xC4D0, 0x0258,
	0xC4D2, 0x025A,
	0xC4D4, 0x025C,
	0xC4D6, 0x025E,
	0xC4D8, 0x0262,
	0xC4DA, 0x0266,
	0xC4DC, 0x026C,
	0xC4DE, 0x0400,
	0xC4E0, 0x0402,
	0xC4E2, 0x040C,
	0xC4E4, 0x040E,
	0xC4E6, 0x0410,
	0xC4E8, 0x041E,
	0xC4EA, 0x0420,
	0xC4EC, 0x0422,
	0xC4EE, 0x0424,
	0xC4F0, 0x0426,
	0xC4F2, 0x042C,
	0xC4F4, 0x042E,
	0xC4F6, 0x0442,
	0xC4F8, 0x044C,
	0xC4FA, 0x0452,
	0xC4FC, 0x0454,
	0xC4FE, 0x0456,
	0xC500, 0x045C,
	0xC502, 0x045E,
	0xC504, 0x046C,
	0xC506, 0x046E,
	0xC508, 0x0470,
	0xC50A, 0x0472,
	0xC50C, 0x0602,
	0xC50E, 0x0604,
	0xC510, 0x0624,
	0xC512, 0x06D0,
	0xC514, 0x06D2,
	0xC516, 0x06E4,
	0xC518, 0x06E6,
	0xC51A, 0x06E8,
	0xC51C, 0x06EA,
	0xC51E, 0x06F6,
	0xC520, 0x0A0A,
	0xC522, 0x0A10,
	0xC524, 0x0B04,
	0xC526, 0x0B20,
	0xC528, 0x0B22,
	0xC52A, 0x0C00,
	0xC52C, 0x0C14,
	0xC52E, 0x0C16,
	0xC530, 0x0C18,
	0xC532, 0x0C1A,
	0xC534, 0x0D00,
	0xC536, 0x0D28,
	0xC538, 0x0D2A,
	0xC53A, 0x0D2C,
	0xC53C, 0x0D50,
	0xC53E, 0x0D52,
	0xC540, 0x0F00,
	0xC542, 0x0F04,
	0xC544, 0x0F06,
	0xC546, 0x0F08,
	0xC548, 0x0F0A,
	0xC54A, 0x0F0C,
	0xC54C, 0x0F0E,
	0xC54E, 0x101C,
	0xC550, 0x103E,
	0xC552, 0x1042,
	0xC554, 0x1044,
	0xC556, 0x1046,
	0xC558, 0x1048,
	0xC55A, 0x1100,
	0xC55C, 0x1130,
	0xC55E, 0x1202,
	0xC560, 0x1206,
	0xC562, 0x1410,
	0xC564, 0x1414,
	0xC566, 0x1416,
	0xC568, 0x1418,
	0xC56A, 0x141A,
	0xC56C, 0x141C,
	0xC56E, 0x141E,
	0xC570, 0x1420,
	0xC572, 0x1422,
	0xC574, 0x1600,
	0xC576, 0x1608,
	0xC578, 0x160A,
	0xC57A, 0x160C,
	0xC57C, 0x160E,
	0xC57E, 0x1614,
	0xC580, 0x1702,
	0xC582, 0x1704,
	0xC584, 0x1706,
	0xC586, 0x1708,
	0xC588, 0x170A,
	0xC58A, 0x170C,
	0xC58C, 0x1718,
	0xC58E, 0x1740,
	0xC590, 0x1C00,
	0xC592, 0x1D06,
	0xC594, 0x1E00,
	0x0368, 0x0073,

	0xC294, 0x0000,
	0xC296, 0x0001,
	0xC2A0, 0x07FF,
	0xC2AA, 0x07FF,
	0xC2B4, 0x07FF,
	0xC2BE, 0x07FF,
	0xC2C0, 0x000E,
	0xC2C2, 0x2024,
	0xC2C4, 0x200B,
	0xC2C6, 0x0022,
	0xC2C8, 0x00B9,
	0xC2CA, 0x0002,
	0xC2CC, 0x00A9,
	0xC2CE, 0x0000,
	0xC2D0, 0x0468,
	0xC2D2, 0x0002,
	0xC2D4, 0x39FF,
	0xC2D6, 0x0002,
	0xC2D8, 0x0019,
	0xC2DA, 0x201B,
	0xC2DC, 0x2008,
	0xC2DE, 0x001E,
	0xC2E0, 0x008A,
	0xC2E2, 0x0002,
	0xC2E4, 0x00E6,
	0xC2E6, 0x0000,
	0xC2E8, 0x02ED,
	0xC2EA, 0x0002,
	0xC2EC, 0x3006,
	0xC2EE, 0x0002,
	0xC2F0, 0x2023,
	0xC2F2, 0x2043,
	0xC2F4, 0x200D,
	0xC2F6, 0x0025,
	0xC2F8, 0x0010,
	0xC2FA, 0x0002,
	0xC2FC, 0x00BB,
	0xC2FE, 0x0000,
	0xC300, 0x0A55,
	0xC302, 0x0002,
	0xC304, 0x4457,
	0xC306, 0x0002,
	0xC308, 0x0014,
	0xC30A, 0x002C,
	0xC30C, 0x2004,
	0xC30E, 0x000B,
	0xC310, 0x000C,
	0xC312, 0x0002,
	0xC314, 0x00AA,
	0xC316, 0x0002,
	0xC318, 0x0898,
	0xC31A, 0x0000,
	0xC31C, 0x0A68,
	0xC31E, 0x0002,
	0xC320, 0x0400,
	0x8000, 0x0040,
	0x8100, 0x0008,
	0x8102, 0x12DE,
	0x8104, 0x25BC,
	0x8106, 0xBCAD,
	0x8108, 0x0000,
	0x810A, 0x2000,
	0x810C, 0x0000,
	0x810E, 0x2000,
	0x8110, 0x0000,
	0x8112, 0x2000,
	0x8114, 0x0000,
	0x8116, 0x2000,
	0x8118, 0x0000,
	0x811A, 0x0030,
	0x811C, 0x0070,
	0x811E, 0x00F0,
	0x8120, 0x0000,
	0x8122, 0x2000,
	0x8124, 0x0000,
	0x8128, 0x0000,
	0x812C, 0x0000,
	0x812E, 0x1B80,
	0x8130, 0x018D,
	0x8132, 0x01B1,
	0x8134, 0x01C9,
	0x8136, 0x01E2,
	0x8138, 0x0000,
	0x813C, 0x0000,
	0x8140, 0x0000,
	0x8144, 0x0000,
	0x8146, 0x2000,
	0x8148, 0x0000,
	0x814A, 0x0030,
	0x814C, 0x0070,
	0x814E, 0x00F0,
	0x8150, 0x0000,
	0x8152, 0x2000,
	0x8154, 0x0000,
	0x8156, 0x2000,
	0x8158, 0x0000,
	0x815A, 0x2000,
	0x815C, 0x0000,
	0x815E, 0x2000,
	0x8160, 0x0000,
	0x8162, 0x1E80,
	0x8164, 0x0000,
	0x8166, 0x0000,
	0x0318, 0x001F,
	0x0328, 0x000B,
	0x0344, 0x0002,
	0x0348, 0x468E,
	0x034A, 0x6AA9,
	0x0364, 0x3200,
	0x0366, 0x0380,
	0x036C, 0x8391,
	0x036E, 0x00C4,
	0x0370, 0x00C4,
	0x1300, 0x000E,
	0x133E, 0x0864,
	0x1340, 0x0100,
	0x139C, 0x0B8C,
	0x0600, 0x1131,
	0x06A6, 0x6700,
	0x06D4, 0x0041,
	0x613C, 0x7070,
	0x613E, 0x5858,
	0x6154, 0x0300,
	0x1102, 0x0008,
	0x11C6, 0x0009,
	0x1742, 0xFFFF,
	0x3006, 0x0003,
	0x0A04, 0xB401,
	0x0A06, 0xC400,
	0x0A08, 0xC88F,
	0x0A0C, 0xF055,
	0x0A0E, 0xEE10,
	0x0A12, 0x0002,
	0x0A18, 0x0014,
	0x0A24, 0x00C4,
	0x070A, 0x1F80,
	0x07A2, 0x3400,
	0x07A4, 0x6800,
	0x07A6, 0x9C00,
	0x07A8, 0xD000,
	0x07C6, 0x1A00,
	0x07C8, 0x3400,
	0x07CA, 0x4E00,
	0x07CC, 0x6800,
	0x07CE, 0x0100,
	0x07D0, 0x0100,
	0x07D2, 0x0100,
	0x07D4, 0x0100,
	0x07D6, 0x0100,
	0x07D8, 0x0100,
	0x1030, 0x1F01,
	0x1032, 0x0008,
	0x1034, 0x0300,
	0x1036, 0x0103,
	0x1064, 0x0300,
	0x027E, 0x0100,

};
#endif

static void sensor_init(void)
{
	LOG_INF("E\n");
#if MULTI_WRITE
	//+Bug 604664 zhanghao2, add, 2021/08/04/, distinguish 801BA1 version to use 865Mhz mipi clock.
	if (true == board_is_lamei){
	 	w1hi4821qrearsy_table_write_cmos_sensor(
			addr_data_pair_init_w1hi4821qrearsy_lamei,
			sizeof(addr_data_pair_init_w1hi4821qrearsy_lamei) /
			sizeof(kal_uint16));
	} else {
	 	w1hi4821qrearsy_table_write_cmos_sensor(
			addr_data_pair_init_w1hi4821qrearsy,
			sizeof(addr_data_pair_init_w1hi4821qrearsy) /
			sizeof(kal_uint16));
	}
	//-Bug 604664 zhanghao2, add, 2021/08/04/, distinguish 801BA1 version to use 865Mhz mipi clock.
#else


#endif
}

#if MULTI_WRITE
kal_uint16 addr_data_pair_preview_w1hi4821qrearsy[] = {
	0x0208, 0x0100,
	0xC3B0, 0x4408,
	0xC3B2, 0x0386,
	0xC3B4, 0x0046,
	0xC3B6, 0x1B66,
	0xC3B8, 0x0090,
	0xC3BA, 0x1816,
	0xC3BC, 0x0103,
	0xC3BE, 0x0301,
	0xC3C0, 0x8013,
	0xC3C2, 0x0901,
	0xC3C4, 0x0103,
	0xC3C6, 0x0301,
	0xC3C8, 0x8013,
	0xC3CA, 0x0901,
	0xC3CC, 0x0103,
	0xC3CE, 0x0301,
	0xC3D0, 0x8013,
	0xC3D2, 0x0901,
	0xC3D4, 0x0088,
	0xC3D6, 0x0088,
	0xC3D8, 0x0010,
	0xC3DA, 0x0010,
	0xC3DC, 0x0223,
	0xC3DE, 0x3047,
	0xC3E0, 0x1133,
	0xC3E2, 0x0000,
	0xC3E4, 0x0001,
	0xC3E6, 0x01F5,
	0xC3E8, 0x8540,
	0xC3EA, 0x1400,
	0xC3EC, 0x00E1,
	0xC3EE, 0x0028,
	0xC3F0, 0x0404,
	0xC3F2, 0x0000,
	0xC3F4, 0x0BC8,
	0xC3F6, 0x0508,
	0xC3F8, 0x0100,
	0xC3FA, 0x8584,
	0xC3FC, 0x0205,
	0xC3FE, 0xFFFF,
	0xC400, 0x020F,
	0xC402, 0x1FFE,
	0xC404, 0x0002,
	0xC406, 0x03EB,
	0xC408, 0x0002,
	0xC40A, 0x002A,
	0xC40C, 0x7112,
	0xC40E, 0x8C4B,
	0xC410, 0x0004,
	0xC412, 0x0064,
	0xC414, 0x0230,
	0xC416, 0x8040,
	0xC418, 0x0040,
	0xC41A, 0x0010,
	0xC41C, 0x8040,
	0xC41E, 0xFF00,
	0xC420, 0xC372,
	0xC422, 0xD840,
	0xC424, 0x90F4,
	0xC426, 0x0FA0,
	0xC428, 0x0BB8,
	0xC42A, 0x0221,
	0xC42C, 0x0004,
	0xC42E, 0x0008,
	0xC430, 0x0FA0,
	0xC432, 0x0BB8,
	0xC434, 0x4000,
	0xC436, 0x0008,
	0xC438, 0x0FAF,
	0xC43A, 0x0000,
	0xC43C, 0x81F7,
	0xC43E, 0x0010,
	0xC440, 0x0000,
	0xC442, 0x0004,
	0xC444, 0x0008,
	0xC446, 0x0011,
	0xC448, 0x2233,
	0xC44A, 0x4455,
	0xC44C, 0x6677,
	0xC44E, 0x021A,
	0xC450, 0x0001,
	0xC452, 0x0108,
	0xC454, 0x01F0,
	0xC456, 0x0004,
	0xC458, 0x01F0,
	0xC45A, 0x1141,
	0xC45C, 0x0000,
	0xC45E, 0x0E3C,
	0xC460, 0x0860,
	0xC462, 0x00C1,
	0xC464, 0x0400,
	0xC466, 0x0400,
	0xC468, 0x0400,
	0xC46A, 0x0400,
	0xC46C, 0x0400,
	0xC46E, 0x0400,
	0xC470, 0x0400,
	0xC472, 0x0400,
	0xC474, 0xF080,
	0xC476, 0x0014,
	0xC478, 0x0F80,
	0xC47A, 0x0014,
	0xC47C, 0x0BA0,
	0xC47E, 0x0305,
	0xC480, 0x1C9C,
	0xC482, 0x0000,
	0xC484, 0x0000,
	0xC486, 0x0058,
	0xC488, 0x0038,
	0xC48A, 0x8144,
	0xC48C, 0x0000,
	0xC48E, 0x0000,
	0xC490, 0x0140,
	0xC492, 0x0010,
	0xC494, 0x0004,
	0xC298, 0x0042,
	0xC29A, 0x0047,
	0xC29C, 0x0074,
	0xC29E, 0x0167,
	0xC2A2, 0x0042,
	0xC2A4, 0x0047,
	0xC2A6, 0x0074,
	0xC2A8, 0x0168,
	0xC2AC, 0x0042,
	0xC2AE, 0x0046,
	0xC2B0, 0x0074,
	0xC2B2, 0x0164,
	0xC2B6, 0x0042,
	0xC2B8, 0x0046,
	0xC2BA, 0x0070,
	0xC2BC, 0x016A,
	0x8002, 0x004B,
	0x8004, 0x0051,
	0x8006, 0x0058,
	0x8008, 0x0061,
	0x800A, 0x0068,
	0x800C, 0x0070,
	0x800E, 0x0076,
	0x8010, 0x007D,
	0x8012, 0x0081,
	0x8014, 0x008B,
	0x8016, 0x008F,
	0x8018, 0x009D,
	0x801A, 0x00A4,
	0x801C, 0x00AA,
	0x801E, 0x00B5,
	0x8020, 0x00B7,
	0x8022, 0x00C5,
	0x8024, 0x00CB,
	0x8026, 0x00D1,
	0x8028, 0x00DD,
	0x802A, 0x00E4,
	0x802C, 0x00ED,
	0x802E, 0x00F6,
	0x8030, 0x0103,
	0x8032, 0x0104,
	0x8034, 0x0112,
	0x8036, 0x011B,
	0x8038, 0x0131,
	0x803A, 0x013D,
	0x803C, 0x0143,
	0x803E, 0x0143,
	0x8126, 0x1E00,
	0x812A, 0x1C00,
	0x813A, 0x1600,
	0x813E, 0x1700,
	0x8142, 0x1C00,
	0x020C, 0x0F8F,
	0x0210, 0x0F97,
	0x238C, 0x0000,
	0x238E, 0x0000,
	0x2390, 0x0303,
	0x2392, 0x0303,
	0x2394, 0x0303,
	0x2396, 0x0303,
	0x2398, 0x0303,
	0x239A, 0x0303,
	0x239C, 0x0303,
	0x239E, 0x0303,
	0x23A0, 0x0303,
	0x23A2, 0x0303,
	0x23A4, 0x0303,
	0x23A6, 0x0303,
	0x23A8, 0x0303,
	0x23AA, 0x0303,
	0x23AC, 0x0303,
	0x23AE, 0x0303,
	0x0736, 0x0087,
	0x0738, 0x0302,
	0x073C, 0x0900,
	0x0746, 0x01BE,
	0x0748, 0x0005,
	0x0766, 0x0040,
	0x07A0, 0x0000,
	0x07AA, 0x0400,
	0x07AC, 0x0400,
	0x07AE, 0x0400,
	0x07B0, 0x0400,
	0x07B2, 0x0400,
	0x07B6, 0x0080,
	0x07C4, 0x0000,
	0x1200, 0x8922,
	0x1020, 0xC108,
	0x1022, 0x0921,
	0x1024, 0x0508,
	0x1026, 0x0C0B,
	0x1028, 0x150B,
	0x102A, 0x0D0A,
	0x102C, 0x1A0A,
	0x1062, 0x3860,
	0x106C, 0x0008,
	0xC3AC, 0x0100,
	0x0208, 0x0000,
};
#endif



static void preview_setting(void)
{
	LOG_INF("E\n");
	//+Bug 604664 zhanghao2, add, 2021/08/04/, distinguish 801BA1 version to use 865Mhz mipi clock.
	if (true == board_is_lamei){
		w1hi4821qrearsy_table_write_cmos_sensor(
			addr_data_pair_preview_w1hi4821qrearsy_lamei,
			sizeof(addr_data_pair_preview_w1hi4821qrearsy_lamei) /
			sizeof(kal_uint16));
	} else {
		w1hi4821qrearsy_table_write_cmos_sensor(
			addr_data_pair_preview_w1hi4821qrearsy,
			sizeof(addr_data_pair_preview_w1hi4821qrearsy) /
			sizeof(kal_uint16));
	}
	//-Bug 604664 zhanghao2, add, 2021/08/04/, distinguish 801BA1 version to use 865Mhz mipi clock.
}

#if MULTI_WRITE
kal_uint16 addr_data_pair_capture_30fps_w1hi4821qrearsy[] = {
	0x0208, 0x0100,
	0xC3B0, 0x4408,
	0xC3B2, 0x0386,
	0xC3B4, 0x0046,
	0xC3B6, 0x1B66,
	0xC3B8, 0x0090,
	0xC3BA, 0x1816,
	0xC3BC, 0x0103,
	0xC3BE, 0x0301,
	0xC3C0, 0x8013,
	0xC3C2, 0x0901,
	0xC3C4, 0x0103,
	0xC3C6, 0x0301,
	0xC3C8, 0x8013,
	0xC3CA, 0x0901,
	0xC3CC, 0x0103,
	0xC3CE, 0x0301,
	0xC3D0, 0x8013,
	0xC3D2, 0x0901,
	0xC3D4, 0x0088,
	0xC3D6, 0x0088,
	0xC3D8, 0x0010,
	0xC3DA, 0x0010,
	0xC3DC, 0x0223,
	0xC3DE, 0x3047,
	0xC3E0, 0x1133,
	0xC3E2, 0x0000,
	0xC3E4, 0x0001,
	0xC3E6, 0x01F5,
	0xC3E8, 0x8540,
	0xC3EA, 0x1400,
	0xC3EC, 0x00E1,
	0xC3EE, 0x0028,
	0xC3F0, 0x0404,
	0xC3F2, 0x0000,
	0xC3F4, 0x0BC8,
	0xC3F6, 0x0508,
	0xC3F8, 0x0100,
	0xC3FA, 0x8584,
	0xC3FC, 0x0205,
	0xC3FE, 0xFFFF,
	0xC400, 0x020F,
	0xC402, 0x1FFE,
	0xC404, 0x0002,
	0xC406, 0x03EB,
	0xC408, 0x0002,
	0xC40A, 0x002A,
	0xC40C, 0x7112,
	0xC40E, 0x8C4B,
	0xC410, 0x0004,
	0xC412, 0x0064,
	0xC414, 0x0230,
	0xC416, 0x8040,
	0xC418, 0x0040,
	0xC41A, 0x0010,
	0xC41C, 0x8040,
	0xC41E, 0xFF00,
	0xC420, 0xC372,
	0xC422, 0xD840,
	0xC424, 0x90F4,
	0xC426, 0x0FA0,
	0xC428, 0x0BB8,
	0xC42A, 0x0221,
	0xC42C, 0x0004,
	0xC42E, 0x0008,
	0xC430, 0x0FA0,
	0xC432, 0x0BB8,
	0xC434, 0x4000,
	0xC436, 0x0008,
	0xC438, 0x0FAF,
	0xC43A, 0x0000,
	0xC43C, 0x81F7,
	0xC43E, 0x0010,
	0xC440, 0x0000,
	0xC442, 0x0004,
	0xC444, 0x0008,
	0xC446, 0x0011,
	0xC448, 0x2233,
	0xC44A, 0x4455,
	0xC44C, 0x6677,
	0xC44E, 0x021A,
	0xC450, 0x0001,
	0xC452, 0x0108,
	0xC454, 0x01F0,
	0xC456, 0x0004,
	0xC458, 0x01F0,
	0xC45A, 0x1141,
	0xC45C, 0x0000,
	0xC45E, 0x0E3C,
	0xC460, 0x0860,
	0xC462, 0x00C1,
	0xC464, 0x0400,
	0xC466, 0x0400,
	0xC468, 0x0400,
	0xC46A, 0x0400,
	0xC46C, 0x0400,
	0xC46E, 0x0400,
	0xC470, 0x0400,
	0xC472, 0x0400,
	0xC474, 0xF080,
	0xC476, 0x0014,
	0xC478, 0x0F80,
	0xC47A, 0x0014,
	0xC47C, 0x0BA0,
	0xC47E, 0x0305,
	0xC480, 0x1C9C,
	0xC482, 0x0000,
	0xC484, 0x0000,
	0xC486, 0x0058,
	0xC488, 0x0038,
	0xC48A, 0x8144,
	0xC48C, 0x0000,
	0xC48E, 0x0000,
	0xC490, 0x0140,
	0xC492, 0x0010,
	0xC494, 0x0004,
	0xC298, 0x0042,
	0xC29A, 0x0047,
	0xC29C, 0x0074,
	0xC29E, 0x0167,
	0xC2A2, 0x0042,
	0xC2A4, 0x0047,
	0xC2A6, 0x0074,
	0xC2A8, 0x0168,
	0xC2AC, 0x0042,
	0xC2AE, 0x0046,
	0xC2B0, 0x0074,
	0xC2B2, 0x0164,
	0xC2B6, 0x0042,
	0xC2B8, 0x0046,
	0xC2BA, 0x0070,
	0xC2BC, 0x016A,
	0x8002, 0x004B,
	0x8004, 0x0051,
	0x8006, 0x0058,
	0x8008, 0x0061,
	0x800A, 0x0068,
	0x800C, 0x0070,
	0x800E, 0x0076,
	0x8010, 0x007D,
	0x8012, 0x0081,
	0x8014, 0x008B,
	0x8016, 0x008F,
	0x8018, 0x009D,
	0x801A, 0x00A4,
	0x801C, 0x00AA,
	0x801E, 0x00B5,
	0x8020, 0x00B7,
	0x8022, 0x00C5,
	0x8024, 0x00CB,
	0x8026, 0x00D1,
	0x8028, 0x00DD,
	0x802A, 0x00E4,
	0x802C, 0x00ED,
	0x802E, 0x00F6,
	0x8030, 0x0103,
	0x8032, 0x0104,
	0x8034, 0x0112,
	0x8036, 0x011B,
	0x8038, 0x0131,
	0x803A, 0x013D,
	0x803C, 0x0143,
	0x803E, 0x0143,
	0x8126, 0x1E00,
	0x812A, 0x1C00,
	0x813A, 0x1600,
	0x813E, 0x1700,
	0x8142, 0x1C00,
	0x020C, 0x0F8F,
	0x0210, 0x0F97,
	0x238C, 0x0000,
	0x238E, 0x0000,
	0x2390, 0x0303,
	0x2392, 0x0303,
	0x2394, 0x0303,
	0x2396, 0x0303,
	0x2398, 0x0303,
	0x239A, 0x0303,
	0x239C, 0x0303,
	0x239E, 0x0303,
	0x23A0, 0x0303,
	0x23A2, 0x0303,
	0x23A4, 0x0303,
	0x23A6, 0x0303,
	0x23A8, 0x0303,
	0x23AA, 0x0303,
	0x23AC, 0x0303,
	0x23AE, 0x0303,
	0x0736, 0x0087,
	0x0738, 0x0302,
	0x073C, 0x0900,
	0x0746, 0x01BE,
	0x0748, 0x0005,
	0x0766, 0x0040,
	0x07A0, 0x0000,
	0x07AA, 0x0400,
	0x07AC, 0x0400,
	0x07AE, 0x0400,
	0x07B0, 0x0400,
	0x07B2, 0x0400,
	0x07B6, 0x0080,
	0x07C4, 0x0000,
	0x1200, 0x8922,
	0x1020, 0xC108,
	0x1022, 0x0921,
	0x1024, 0x0508,
	0x1026, 0x0C0B,
	0x1028, 0x150B,
	0x102A, 0x0D0A,
	0x102C, 0x1A0A,
	0x1062, 0x3860,
	0x106C, 0x0008,
	0xC3AC, 0x0100,
	0x0208, 0x0000,
};

kal_uint16 addr_data_pair_capture_15fps_w1hi4821qrearsy[] = {

	};
#endif


static void capture_setting(kal_uint16 currefps)
{
	LOG_INF("E\n");
	//+Bug 604664 zhanghao2, add, 2021/08/04/, distinguish 801BA1 version to use 865Mhz mipi clock.
	if (true == board_is_lamei){
		w1hi4821qrearsy_table_write_cmos_sensor(
			addr_data_pair_capture_30fps_w1hi4821qrearsy_lamei,
			sizeof(addr_data_pair_capture_30fps_w1hi4821qrearsy_lamei) /
			sizeof(kal_uint16));
	} else {
		w1hi4821qrearsy_table_write_cmos_sensor(
			addr_data_pair_capture_30fps_w1hi4821qrearsy,
			sizeof(addr_data_pair_capture_30fps_w1hi4821qrearsy) /
			sizeof(kal_uint16));
	}
	//-Bug 604664 zhanghao2, add, 2021/08/04/, distinguish 801BA1 version to use 865Mhz mipi clock.
}

#if MULTI_WRITE
kal_uint16 addr_data_pair_video_w1hi4821qrearsy[] = {
	0x0208, 0x0100,
	0xC3B0, 0x4408,
	0xC3B2, 0x0386,
	0xC3B4, 0x0046,
	0xC3B6, 0x1B66,
	0xC3B8, 0x0370,
	0xC3BA, 0x1536,
	0xC3BC, 0x0103,
	0xC3BE, 0x0301,
	0xC3C0, 0x8013,
	0xC3C2, 0x0901,
	0xC3C4, 0x0103,
	0xC3C6, 0x0301,
	0xC3C8, 0x8013,
	0xC3CA, 0x0901,
	0xC3CC, 0x0103,
	0xC3CE, 0x0301,
	0xC3D0, 0x8013,
	0xC3D2, 0x0901,
	0xC3D4, 0x0088,
	0xC3D6, 0x0088,
	0xC3D8, 0x0010,
	0xC3DA, 0x0010,
	0xC3DC, 0x0223,
	0xC3DE, 0x3047,
	0xC3E0, 0x1133,
	0xC3E2, 0x0000,
	0xC3E4, 0x0001,
	0xC3E6, 0x01F5,
	0xC3E8, 0x8540,
	0xC3EA, 0x1400,
	0xC3EC, 0x00E1,
	0xC3EE, 0x0028,
	0xC3F0, 0x0404,
	0xC3F2, 0x0004,
	0xC3F4, 0x08E0,
	0xC3F6, 0x0508,
	0xC3F8, 0x0100,
	0xC3FA, 0x8584,
	0xC3FC, 0x0205,
	0xC3FE, 0xFFFF,
	0xC400, 0x020F,
	0xC402, 0x1FFE,
	0xC404, 0x0002,
	0xC406, 0x03EB,
	0xC408, 0x0002,
	0xC40A, 0x002A,
	0xC40C, 0x7112,
	0xC40E, 0x8C4B,
	0xC410, 0x0004,
	0xC412, 0x0064,
	0xC414, 0x0230,
	0xC416, 0x8040,
	0xC418, 0x0040,
	0xC41A, 0x0010,
	0xC41C, 0x8040,
	0xC41E, 0xFF00,
	0xC420, 0xC372,
	0xC422, 0xD840,
	0xC424, 0x90F4,
	0xC426, 0x0FA0,
	0xC428, 0x08D0,
	0xC42A, 0x0221,
	0xC42C, 0x0004,
	0xC42E, 0x0008,
	0xC430, 0x0FA0,
	0xC432, 0x08D0,
	0xC434, 0x4000,
	0xC436, 0x0008,
	0xC438, 0x0FAF,
	0xC43A, 0x0000,
	0xC43C, 0x81F7,
	0xC43E, 0x0010,
	0xC440, 0x0000,
	0xC442, 0x0004,
	0xC444, 0x0008,
	0xC446, 0x0011,
	0xC448, 0x2233,
	0xC44A, 0x4455,
	0xC44C, 0x6677,
	0xC44E, 0x021A,
	0xC450, 0x0001,
	0xC452, 0x0108,
	0xC454, 0x01F0,
	0xC456, 0x0004,
	0xC458, 0x01F0,
	0xC45A, 0x1141,
	0xC45C, 0x0000,
	0xC45E, 0x0E3C,
	0xC460, 0x0860,
	0xC462, 0x00C1,
	0xC464, 0x0400,
	0xC466, 0x0400,
	0xC468, 0x0400,
	0xC46A, 0x0400,
	0xC46C, 0x0400,
	0xC46E, 0x0400,
	0xC470, 0x0400,
	0xC472, 0x0400,
	0xC474, 0xF080,
	0xC476, 0x0014,
	0xC478, 0x0F80,
	0xC47A, 0x0010,
	0xC47C, 0x08C0,
	0xC47E, 0x0305,
	0xC480, 0x1C9C,
	0xC482, 0x0000,
	0xC484, 0x0000,
	0xC486, 0x0058,
	0xC488, 0x0320,
	0xC48A, 0x8144,
	0xC48C, 0x0000,
	0xC48E, 0x0000,
	0xC490, 0x0140,
	0xC492, 0x0010,
	0xC494, 0x0004,
	0xC298, 0x0042,
	0xC29A, 0x0047,
	0xC29C, 0x0074,
	0xC29E, 0x0167,
	0xC2A2, 0x0042,
	0xC2A4, 0x0047,
	0xC2A6, 0x0074,
	0xC2A8, 0x0168,
	0xC2AC, 0x0042,
	0xC2AE, 0x0046,
	0xC2B0, 0x0074,
	0xC2B2, 0x0164,
	0xC2B6, 0x0042,
	0xC2B8, 0x0046,
	0xC2BA, 0x0070,
	0xC2BC, 0x016A,
	0x8002, 0x004B,
	0x8004, 0x0051,
	0x8006, 0x0058,
	0x8008, 0x0061,
	0x800A, 0x0068,
	0x800C, 0x0070,
	0x800E, 0x0076,
	0x8010, 0x007D,
	0x8012, 0x0081,
	0x8014, 0x008B,
	0x8016, 0x008F,
	0x8018, 0x009D,
	0x801A, 0x00A4,
	0x801C, 0x00AA,
	0x801E, 0x00B5,
	0x8020, 0x00B7,
	0x8022, 0x00C5,
	0x8024, 0x00CB,
	0x8026, 0x00D1,
	0x8028, 0x00DD,
	0x802A, 0x00E4,
	0x802C, 0x00ED,
	0x802E, 0x00F6,
	0x8030, 0x0103,
	0x8032, 0x0104,
	0x8034, 0x0112,
	0x8036, 0x011B,
	0x8038, 0x0131,
	0x803A, 0x013D,
	0x803C, 0x0143,
	0x803E, 0x0143,
	0x8126, 0x1E00,
	0x812A, 0x1C00,
	0x813A, 0x1600,
	0x813E, 0x1700,
	0x8142, 0x1C00,
	0x020C, 0x0F8F,
	0x0210, 0x0F97,
	0x238C, 0x0000,
	0x238E, 0x0000,
	0x2390, 0x0303,
	0x2392, 0x0303,
	0x2394, 0x0303,
	0x2396, 0x0303,
	0x2398, 0x0303,
	0x239A, 0x0303,
	0x239C, 0x0303,
	0x239E, 0x0303,
	0x23A0, 0x0303,
	0x23A2, 0x0303,
	0x23A4, 0x0303,
	0x23A6, 0x0303,
	0x23A8, 0x0303,
	0x23AA, 0x0303,
	0x23AC, 0x0303,
	0x23AE, 0x0303,
	0x0736, 0x0087,
	0x0738, 0x0302,
	0x073C, 0x0900,
	0x0746, 0x00AF,
	0x0748, 0x0002,
	0x0766, 0x0040,
	0x07A0, 0x0001,
	0x07AA, 0x0200,
	0x07AC, 0x0200,
	0x07AE, 0x0200,
	0x07B0, 0x0200,
	0x07B2, 0x0200,
	0x07B6, 0x0100,
	0x07C4, 0x0001,
	0x1200, 0x8922,
	0x1020, 0xC107,
	0x1022, 0x081A,
	0x1024, 0x0506,
	0x1026, 0x0909,
	0x1028, 0x120A,
	0x102A, 0x0A0A,
	0x102C, 0x110A,
	0x1062, 0x3800,
	0x106C, 0x0018,
	0xC3AC, 0x0100,
	0x0208, 0x0000,

};
#endif

static void video_setting(void)
{
	LOG_INF("E\n");
	//+Bug 604664 zhanghao2, add, 2021/08/04/, distinguish 801BA1 version to use 865Mhz mipi clock.
	if (true == board_is_lamei){
		w1hi4821qrearsy_table_write_cmos_sensor(
			addr_data_pair_video_w1hi4821qrearsy_lamei,
			sizeof(addr_data_pair_video_w1hi4821qrearsy_lamei) /
			sizeof(kal_uint16));
	} else {
		w1hi4821qrearsy_table_write_cmos_sensor(
			addr_data_pair_video_w1hi4821qrearsy,
			sizeof(addr_data_pair_video_w1hi4821qrearsy) /
			sizeof(kal_uint16));
	}
	//-Bug 604664 zhanghao2, add, 2021/08/04/, distinguish 801BA1 version to use 865Mhz mipi clock.
}

#if MULTI_WRITE
kal_uint16 addr_data_pair_hs_video_w1hi4821qrearsy[] = {
	0x0208, 0x0100,
	0xC3B0, 0x0508,
	0xC3B2, 0x02E9,
	0xC3B4, 0x0042,
	0xC3B6, 0x1B62,
	0xC3B8, 0x0360,
	0xC3BA, 0x1542,
	0xC3BC, 0x0301,
	0xC3BE, 0x1B01,
	0xC3C0, 0x0180,
	0xC3C2, 0x0901,
	0xC3C4, 0x0301,
	0xC3C6, 0x1B01,
	0xC3C8, 0x0180,
	0xC3CA, 0x0901,
	0xC3CC, 0x0301,
	0xC3CE, 0x1B01,
	0xC3D0, 0x0180,
	0xC3D2, 0x0901,
	0xC3D4, 0x0088,
	0xC3D6, 0x0044,
	0xC3D8, 0x0010,
	0xC3DA, 0x0010,
	0xC3DC, 0x0111,
	0xC3DE, 0x3807,
	0xC3E0, 0x1316,
	0xC3E2, 0x0000,
	0xC3E4, 0x0001,
	0xC3E6, 0x01F5,
	0xC3E8, 0x2120,
	0xC3EA, 0x1400,
	0xC3EC, 0x02E9,
	0xC3EE, 0x0034,
	0xC3F0, 0x0404,
	0xC3F2, 0x0000,
	0xC3F4, 0x047C,
	0xC3F6, 0x4508,
	0xC3F8, 0x0100,
	0xC3FA, 0x8584,
	0xC3FC, 0x0205,
	0xC3FE, 0xFFFF,
	0xC400, 0x030F,
	0xC402, 0x1FFE,
	0xC404, 0x0002,
	0xC406, 0x03EB,
	0xC408, 0x0002,
	0xC40A, 0x0036,
	0xC40C, 0x7112,
	0xC40E, 0x884B,
	0xC410, 0x0004,
	0xC412, 0x0064,
	0xC414, 0x0230,
	0xC416, 0x8040,
	0xC418, 0x0040,
	0xC41A, 0x0010,
	0xC41C, 0x8040,
	0xC41E, 0x0000,
	0xC420, 0xC772,
	0xC422, 0xD840,
	0xC424, 0xB0E4,
	0xC426, 0x07D0,
	0xC428, 0x046C,
	0xC42A, 0x0221,
	0xC42C, 0x0004,
	0xC42E, 0x0008,
	0xC430, 0x0FA0,
	0xC432, 0x046C,
	0xC434, 0x4000,
	0xC436, 0x0008,
	0xC438, 0x0FAF,
	0xC43A, 0x0000,
	0xC43C, 0x81F7,
	0xC43E, 0x0010,
	0xC440, 0x0400,
	0xC442, 0x0002,
	0xC444, 0x0008,
	0xC446, 0x0011,
	0xC448, 0x2233,
	0xC44A, 0x4455,
	0xC44C, 0x6677,
	0xC44E, 0x0201,
	0xC450, 0x0101,
	0xC452, 0x0008,
	0xC454, 0x01F0,
	0xC456, 0x0174,
	0xC458, 0x00F8,
	0xC45A, 0x1100,
	0xC45C, 0x1000,
	0xC45E, 0x1E3C,
	0xC460, 0x0860,
	0xC462, 0x0001,
	0xC464, 0x0200,
	0xC466, 0x0200,
	0xC468, 0x0200,
	0xC46A, 0x0200,
	0xC46C, 0x0200,
	0xC46E, 0x0200,
	0xC470, 0x0200,
	0xC472, 0x0200,
	0xC474, 0x1080,
	0xC476, 0x0030,
	0xC478, 0x1F00,
	0xC47A, 0x0028,
	0xC47C, 0x1740,
	0xC47E, 0x0305,
	0xC480, 0x1C9C,
	0xC482, 0x0000,
	0xC484, 0x0000,
	0xC486, 0x0058,
	0xC488, 0x0308,
	0xC48A, 0x8144,
	0xC48C, 0x0000,
	0xC48E, 0x0000,
	0xC490, 0x0240,
	0xC492, 0x0010,
	0xC494, 0x0004,
	0xC298, 0x0042,
	0xC29A, 0x0047,
	0xC29C, 0x0074,
	0xC29E, 0x0167,
	0xC2A2, 0x0042,
	0xC2A4, 0x0047,
	0xC2A6, 0x0074,
	0xC2A8, 0x0168,
	0xC2AC, 0x0042,
	0xC2AE, 0x0046,
	0xC2B0, 0x0074,
	0xC2B2, 0x0164,
	0xC2B6, 0x0042,
	0xC2B8, 0x0046,
	0xC2BA, 0x0070,
	0xC2BC, 0x016A,
	0x8002, 0x004B,
	0x8004, 0x0051,
	0x8006, 0x0058,
	0x8008, 0x0061,
	0x800A, 0x0068,
	0x800C, 0x0070,
	0x800E, 0x0076,
	0x8010, 0x007D,
	0x8012, 0x0081,
	0x8014, 0x008B,
	0x8016, 0x008F,
	0x8018, 0x009D,
	0x801A, 0x00A4,
	0x801C, 0x00AA,
	0x801E, 0x00B5,
	0x8020, 0x00B7,
	0x8022, 0x00C5,
	0x8024, 0x00CB,
	0x8026, 0x00D1,
	0x8028, 0x00DD,
	0x802A, 0x00E4,
	0x802C, 0x00ED,
	0x802E, 0x00F6,
	0x8030, 0x0103,
	0x8032, 0x0104,
	0x8034, 0x0112,
	0x8036, 0x011B,
	0x8038, 0x0131,
	0x803A, 0x013D,
	0x803C, 0x0143,
	0x803E, 0x0143,
	0x8126, 0x1E00,
	0x812A, 0x1C00,
	0x813A, 0x1600,
	0x813E, 0x1700,
	0x8142, 0x1C00,
	0x020C, 0x04B0,
	0x0210, 0x04B8,
	0x238C, 0x0000,
	0x238E, 0x0000,
	0x2390, 0x0303,
	0x2392, 0x0303,
	0x2394, 0x0303,
	0x2396, 0x0303,
	0x2398, 0x0303,
	0x239A, 0x0303,
	0x239C, 0x0303,
	0x239E, 0x0303,
	0x23A0, 0x0303,
	0x23A2, 0x0303,
	0x23A4, 0x0303,
	0x23A6, 0x0303,
	0x23A8, 0x0303,
	0x23AA, 0x0303,
	0x23AC, 0x0303,
	0x23AE, 0x0303,
	0x0736, 0x006C,
	0x0738, 0x0102,
	0x073C, 0x0700,
	0x0746, 0x01BE,
	0x0748, 0x0005,
	0x0766, 0x0064,
	0x07A0, 0x0000,
	0x07AA, 0x0400,
	0x07AC, 0x0400,
	0x07AE, 0x0400,
	0x07B0, 0x0400,
	0x07B2, 0x0400,
	0x07B6, 0x0080,
	0x07C4, 0x0000,
	0x1200, 0x8922,
	0x1020, 0xC108,
	0x1022, 0x0921,
	0x1024, 0x0508,
	0x1026, 0x0C0B,
	0x1028, 0x150B,
	0x102A, 0x0D0A,
	0x102C, 0x1A0A,
	0x1062, 0x3860,
	0x106C, 0x0008,
	0xC3AC, 0x0100,
	0x0208, 0x0000,

};
#endif

static void hs_video_setting(void)
{
	LOG_INF("E\n");
	//+Bug 604664 zhanghao2, add, 2021/08/04/, distinguish 801BA1 version to use 865Mhz mipi clock.
	if (true == board_is_lamei){
		w1hi4821qrearsy_table_write_cmos_sensor(
			addr_data_pair_hs_video_w1hi4821qrearsy_lamei,
			sizeof(addr_data_pair_hs_video_w1hi4821qrearsy_lamei) /
			sizeof(kal_uint16));
	} else {
		w1hi4821qrearsy_table_write_cmos_sensor(
			addr_data_pair_hs_video_w1hi4821qrearsy,
			sizeof(addr_data_pair_hs_video_w1hi4821qrearsy) /
			sizeof(kal_uint16));
	}
	//-Bug 604664 zhanghao2, add, 2021/08/04/, distinguish 801BA1 version to use 865Mhz mipi clock.
}

#if MULTI_WRITE
kal_uint16 addr_data_pair_slim_video_w1hi4821qrearsy[] = {
	0x0208, 0x0100,
	0xC3B0, 0x0508,
	0xC3B2, 0x02E9,
	0xC3B4, 0x0042,
	0xC3B6, 0x1B62,
	0xC3B8, 0x03C0,
	0xC3BA, 0x14E2,
	0xC3BC, 0x0301,
	0xC3BE, 0x1B01,
	0xC3C0, 0x0180,
	0xC3C2, 0x0901,
	0xC3C4, 0x0301,
	0xC3C6, 0x1B01,
	0xC3C8, 0x0180,
	0xC3CA, 0x0901,
	0xC3CC, 0x0301,
	0xC3CE, 0x1B01,
	0xC3D0, 0x0180,
	0xC3D2, 0x0901,
	0xC3D4, 0x0088,
	0xC3D6, 0x0044,
	0xC3D8, 0x0010,
	0xC3DA, 0x0010,
	0xC3DC, 0x0111,
	0xC3DE, 0x3807,
	0xC3E0, 0x1316,
	0xC3E2, 0x0000,
	0xC3E4, 0x0001,
	0xC3E6, 0x01F5,
	0xC3E8, 0x2120,
	0xC3EA, 0x1400,
	0xC3EC, 0x02E9,
	0xC3EE, 0x004A,
	0xC3F0, 0x0404,
	0xC3F2, 0x0002,
	0xC3F4, 0x0448,
	0xC3F6, 0x4508,
	0xC3F8, 0x0100,
	0xC3FA, 0x8584,
	0xC3FC, 0x0205,
	0xC3FE, 0xFFFF,
	0xC400, 0x030F,
	0xC402, 0x1FFE,
	0xC404, 0x0002,
	0xC406, 0x03EB,
	0xC408, 0x0002,
	0xC40A, 0x004C,
	0xC40C, 0x7112,
	0xC40E, 0x884B,
	0xC410, 0x0004,
	0xC412, 0x0064,
	0xC414, 0x0230,
	0xC416, 0x8040,
	0xC418, 0x0040,
	0xC41A, 0x0010,
	0xC41C, 0x8040,
	0xC41E, 0x0000,
	0xC420, 0xC772,
	0xC422, 0xD840,
	0xC424, 0xB0E4,
	0xC426, 0x0780,
	0xC428, 0x0438,
	0xC42A, 0x0221,
	0xC42C, 0x0054,
	0xC42E, 0x0008,
	0xC430, 0x0F00,
	0xC432, 0x0438,
	0xC434, 0x4000,
	0xC436, 0x0008,
	0xC438, 0x0FAF,
	0xC43A, 0x0000,
	0xC43C, 0x81F7,
	0xC43E, 0x0010,
	0xC440, 0x0400,
	0xC442, 0x002A,
	0xC444, 0x0008,
	0xC446, 0x0011,
	0xC448, 0x2233,
	0xC44A, 0x4455,
	0xC44C, 0x6677,
	0xC44E, 0x0201,
	0xC450, 0x0101,
	0xC452, 0x0008,
	0xC454, 0x01F0,
	0xC456, 0x0174,
	0xC458, 0x00F8,
	0xC45A, 0x1100,
	0xC45C, 0x1000,
	0xC45E, 0x1E3C,
	0xC460, 0x0860,
	0xC462, 0x0001,
	0xC464, 0x0200,
	0xC466, 0x0200,
	0xC468, 0x0200,
	0xC46A, 0x0200,
	0xC46C, 0x0200,
	0xC46E, 0x0200,
	0xC470, 0x0200,
	0xC472, 0x0200,
	0xC474, 0x1080,
	0xC476, 0x0030,
	0xC478, 0x1F00,
	0xC47A, 0x0028,
	0xC47C, 0x1740,
	0xC47E, 0x0305,
	0xC480, 0x1C9C,
	0xC482, 0x0000,
	0xC484, 0x0000,
	0xC486, 0x0058,
	0xC488, 0x0370,
	0xC48A, 0x8144,
	0xC48C, 0x0000,
	0xC48E, 0x0000,
	0xC490, 0x0240,
	0xC492, 0x0010,
	0xC494, 0x0004,
	0xC298, 0x0042,
	0xC29A, 0x0047,
	0xC29C, 0x0074,
	0xC29E, 0x0167,
	0xC2A2, 0x0042,
	0xC2A4, 0x0047,
	0xC2A6, 0x0074,
	0xC2A8, 0x0168,
	0xC2AC, 0x0042,
	0xC2AE, 0x0046,
	0xC2B0, 0x0074,
	0xC2B2, 0x0164,
	0xC2B6, 0x0042,
	0xC2B8, 0x0046,
	0xC2BA, 0x0070,
	0xC2BC, 0x016A,
	0x8002, 0x004B,
	0x8004, 0x0051,
	0x8006, 0x0058,
	0x8008, 0x0061,
	0x800A, 0x0068,
	0x800C, 0x0070,
	0x800E, 0x0076,
	0x8010, 0x007D,
	0x8012, 0x0081,
	0x8014, 0x008B,
	0x8016, 0x008F,
	0x8018, 0x009D,
	0x801A, 0x00A4,
	0x801C, 0x00AA,
	0x801E, 0x00B5,
	0x8020, 0x00B7,
	0x8022, 0x00C5,
	0x8024, 0x00CB,
	0x8026, 0x00D1,
	0x8028, 0x00DD,
	0x802A, 0x00E4,
	0x802C, 0x00ED,
	0x802E, 0x00F6,
	0x8030, 0x0103,
	0x8032, 0x0104,
	0x8034, 0x0112,
	0x8036, 0x011B,
	0x8038, 0x0131,
	0x803A, 0x013D,
	0x803C, 0x0143,
	0x803E, 0x0143,
	0x8126, 0x1E00,
	0x812A, 0x1C00,
	0x813A, 0x1600,
	0x813E, 0x1700,
	0x8142, 0x1C00,
	0x020C, 0x12D8,
	0x0210, 0x12E0,
	0x238C, 0x0000,
	0x238E, 0x0000,
	0x2390, 0x0303,
	0x2392, 0x0303,
	0x2394, 0x0303,
	0x2396, 0x0303,
	0x2398, 0x0303,
	0x239A, 0x0303,
	0x239C, 0x0303,
	0x239E, 0x0303,
	0x23A0, 0x0303,
	0x23A2, 0x0303,
	0x23A4, 0x0303,
	0x23A6, 0x0303,
	0x23A8, 0x0303,
	0x23AA, 0x0303,
	0x23AC, 0x0303,
	0x23AE, 0x0303,
	0x0736, 0x006C,
	0x0738, 0x0102,
	0x073C, 0x0700,
	0x0746, 0x01BE,
	0x0748, 0x0005,
	0x0766, 0x0064,
	0x07A0, 0x0000,
	0x07AA, 0x0400,
	0x07AC, 0x0400,
	0x07AE, 0x0400,
	0x07B0, 0x0400,
	0x07B2, 0x0400,
	0x07B6, 0x0080,
	0x07C4, 0x0000,
	0x1200, 0x8922,
	0x1020, 0xC108,
	0x1022, 0x0921,
	0x1024, 0x0508,
	0x1026, 0x0C0B,
	0x1028, 0x150B,
	0x102A, 0x0D0A,
	0x102C, 0x1A0A,
	0x1062, 0x3860,
	0x106C, 0x0008,
	0xC3AC, 0x0100,
	0x0208, 0x0000,

};
#endif


static void slim_video_setting(void)
{
	LOG_INF("E\n");
	//+Bug 604664 zhanghao2, add, 2021/08/04/, distinguish 801BA1 version to use 865Mhz mipi clock.
	if (true == board_is_lamei){
		w1hi4821qrearsy_table_write_cmos_sensor(
			addr_data_pair_slim_video_w1hi4821qrearsy_lamei,
			sizeof(addr_data_pair_slim_video_w1hi4821qrearsy_lamei) /
			sizeof(kal_uint16));
	} else {
		w1hi4821qrearsy_table_write_cmos_sensor(
			addr_data_pair_slim_video_w1hi4821qrearsy,
			sizeof(addr_data_pair_slim_video_w1hi4821qrearsy) /
			sizeof(kal_uint16));
	}
	//-Bug 604664 zhanghao2, add, 2021/08/04/, distinguish 801BA1 version to use 865Mhz mipi clock.
}

#if MULTI_WRITE
kal_uint16 addr_data_pair_custom1_w1hi4821qrearsy[] = {
	0x0208, 0x0100,
	0xC3B0, 0x4408,
	0xC3B2, 0x0386,
	0xC3B4, 0x0046,
	0xC3B6, 0x1B66,
	0xC3B8, 0x0090,
	0xC3BA, 0x1816,
	0xC3BC, 0x0103,
	0xC3BE, 0x0301,
	0xC3C0, 0x8013,
	0xC3C2, 0x0901,
	0xC3C4, 0x0103,
	0xC3C6, 0x0301,
	0xC3C8, 0x8013,
	0xC3CA, 0x0901,
	0xC3CC, 0x0103,
	0xC3CE, 0x0301,
	0xC3D0, 0x8013,
	0xC3D2, 0x0901,
	0xC3D4, 0x0088,
	0xC3D6, 0x0088,
	0xC3D8, 0x0010,
	0xC3DA, 0x0010,
	0xC3DC, 0x0223,
	0xC3DE, 0x3047,
	0xC3E0, 0x1133,
	0xC3E2, 0x0000,
	0xC3E4, 0x0001,
	0xC3E6, 0x01F5,
	0xC3E8, 0x8540,
	0xC3EA, 0x1400,
	0xC3EC, 0x00E1,
	0xC3EE, 0x0028,
	0xC3F0, 0x0404,
	0xC3F2, 0x0000,
	0xC3F4, 0x0BC8,
	0xC3F6, 0x0508,
	0xC3F8, 0x0100,
	0xC3FA, 0x8584,
	0xC3FC, 0x0205,
	0xC3FE, 0xFFFF,
	0xC400, 0x020F,
	0xC402, 0x1FFE,
	0xC404, 0x0002,
	0xC406, 0x03EB,
	0xC408, 0x0002,
	0xC40A, 0x002A,
	0xC40C, 0x7112,
	0xC40E, 0x8C4B,
	0xC410, 0x0004,
	0xC412, 0x0064,
	0xC414, 0x0230,
	0xC416, 0x8040,
	0xC418, 0x0040,
	0xC41A, 0x0010,
	0xC41C, 0x8040,
	0xC41E, 0xFF00,
	0xC420, 0xC372,
	0xC422, 0xD840,
	0xC424, 0x90F4,
	0xC426, 0x0FA0,
	0xC428, 0x0BB8,
	0xC42A, 0x0221,
	0xC42C, 0x0004,
	0xC42E, 0x0008,
	0xC430, 0x0FA0,
	0xC432, 0x0BB8,
	0xC434, 0x4000,
	0xC436, 0x0008,
	0xC438, 0x0FAF,
	0xC43A, 0x0000,
	0xC43C, 0x81F7,
	0xC43E, 0x0010,
	0xC440, 0x0000,
	0xC442, 0x0004,
	0xC444, 0x0008,
	0xC446, 0x0011,
	0xC448, 0x2233,
	0xC44A, 0x4455,
	0xC44C, 0x6677,
	0xC44E, 0x021A,
	0xC450, 0x0001,
	0xC452, 0x0108,
	0xC454, 0x01F0,
	0xC456, 0x0004,
	0xC458, 0x01F0,
	0xC45A, 0x1141,
	0xC45C, 0x0000,
	0xC45E, 0x0E3C,
	0xC460, 0x0860,
	0xC462, 0x00C1,
	0xC464, 0x0400,
	0xC466, 0x0400,
	0xC468, 0x0400,
	0xC46A, 0x0400,
	0xC46C, 0x0400,
	0xC46E, 0x0400,
	0xC470, 0x0400,
	0xC472, 0x0400,
	0xC474, 0xF080,
	0xC476, 0x0014,
	0xC478, 0x0F80,
	0xC47A, 0x0014,
	0xC47C, 0x0BA0,
	0xC47E, 0x0305,
	0xC480, 0x1C9C,
	0xC482, 0x0000,
	0xC484, 0x0000,
	0xC486, 0x0058,
	0xC488, 0x0038,
	0xC48A, 0x8144,
	0xC48C, 0x0000,
	0xC48E, 0x0000,
	0xC490, 0x0140,
	0xC492, 0x0010,
	0xC494, 0x0004,
	0xC298, 0x0042,
	0xC29A, 0x0047,
	0xC29C, 0x0074,
	0xC29E, 0x0167,
	0xC2A2, 0x0042,
	0xC2A4, 0x0047,
	0xC2A6, 0x0074,
	0xC2A8, 0x0168,
	0xC2AC, 0x0042,
	0xC2AE, 0x0046,
	0xC2B0, 0x0074,
	0xC2B2, 0x0164,
	0xC2B6, 0x0042,
	0xC2B8, 0x0046,
	0xC2BA, 0x0070,
	0xC2BC, 0x016A,
	0x8002, 0x004B,
	0x8004, 0x0051,
	0x8006, 0x0058,
	0x8008, 0x0061,
	0x800A, 0x0068,
	0x800C, 0x0070,
	0x800E, 0x0076,
	0x8010, 0x007D,
	0x8012, 0x0081,
	0x8014, 0x008B,
	0x8016, 0x008F,
	0x8018, 0x009D,
	0x801A, 0x00A4,
	0x801C, 0x00AA,
	0x801E, 0x00B5,
	0x8020, 0x00B7,
	0x8022, 0x00C5,
	0x8024, 0x00CB,
	0x8026, 0x00D1,
	0x8028, 0x00DD,
	0x802A, 0x00E4,
	0x802C, 0x00ED,
	0x802E, 0x00F6,
	0x8030, 0x0103,
	0x8032, 0x0104,
	0x8034, 0x0112,
	0x8036, 0x011B,
	0x8038, 0x0131,
	0x803A, 0x013D,
	0x803C, 0x0143,
	0x803E, 0x0143,
	0x8126, 0x1E00,
	0x812A, 0x1C00,
	0x813A, 0x1600,
	0x813E, 0x1700,
	0x8142, 0x1C00,
	0x020C, 0x0F8F,
	0x0210, 0x0F97,
	0x238C, 0x0000,
	0x238E, 0x0000,
	0x2390, 0x0303,
	0x2392, 0x0303,
	0x2394, 0x0303,
	0x2396, 0x0303,
	0x2398, 0x0303,
	0x239A, 0x0303,
	0x239C, 0x0303,
	0x239E, 0x0303,
	0x23A0, 0x0303,
	0x23A2, 0x0303,
	0x23A4, 0x0303,
	0x23A6, 0x0303,
	0x23A8, 0x0303,
	0x23AA, 0x0303,
	0x23AC, 0x0303,
	0x23AE, 0x0303,
	0x0736, 0x0087,
	0x0738, 0x0302,
	0x073C, 0x0900,
	0x0746, 0x01BE,
	0x0748, 0x0005,
	0x0766, 0x0040,
	0x07A0, 0x0000,
	0x07AA, 0x0400,
	0x07AC, 0x0400,
	0x07AE, 0x0400,
	0x07B0, 0x0400,
	0x07B2, 0x0400,
	0x07B6, 0x0080,
	0x07C4, 0x0000,
	0x1200, 0x8922,
	0x1020, 0xC108,
	0x1022, 0x0921,
	0x1024, 0x0508,
	0x1026, 0x0C0B,
	0x1028, 0x150B,
	0x102A, 0x0D0A,
	0x102C, 0x1A0A,
	0x1062, 0x3860,
	0x106C, 0x0008,
	0xC3AC, 0x0100,
	0x0208, 0x0000,
};
#endif

static void custom1_setting(void)
{
	LOG_INF("E\n");

	//+Bug 604664 zhanghao2, add, 2021/08/04/, distinguish 801BA1 version to use 865Mhz mipi clock.
	if (true == board_is_lamei){
		w1hi4821qrearsy_table_write_cmos_sensor(
			addr_data_pair_custom1_w1hi4821qrearsy_lamei,
			sizeof(addr_data_pair_custom1_w1hi4821qrearsy_lamei) /
			sizeof(kal_uint16));
	} else {
		w1hi4821qrearsy_table_write_cmos_sensor(
			addr_data_pair_custom1_w1hi4821qrearsy,
			sizeof(addr_data_pair_custom1_w1hi4821qrearsy) /
			sizeof(kal_uint16));
	}
	//-Bug 604664 zhanghao2, add, 2021/08/04/, distinguish 801BA1 version to use 865Mhz mipi clock.
}

#if MULTI_WRITE
kal_uint16 addr_data_pair_custom2_w1hi4821qrearsy[] = {
	0x0208, 0x0100,
	0xC3B0, 0x4008,
	0xC3B2, 0x0386,
	0xC3B4, 0x004B,
	0xC3B6, 0x1B6B,
	0xC3B8, 0x0670,
	0xC3BA, 0x1243,
	0xC3BC, 0x0101,
	0xC3BE, 0x0101,
	0xC3C0, 0x0101,
	0xC3C2, 0x0901,
	0xC3C4, 0x0101,
	0xC3C6, 0x0101,
	0xC3C8, 0x0101,
	0xC3CA, 0x0901,
	0xC3CC, 0x0101,
	0xC3CE, 0x0101,
	0xC3D0, 0x0101,
	0xC3D2, 0x0901,
	0xC3D4, 0x0044,
	0xC3D6, 0x0044,
	0xC3D8, 0x0008,
	0xC3DA, 0x0808,
	0xC3DC, 0x0220,
	0xC3DE, 0x3003,
	0xC3E0, 0x0000,
	0xC3E2, 0x0100,
	0xC3E4, 0x00FB,
	0xC3E6, 0x02F2,
	0xC3E8, 0x2131,
	0xC3EA, 0x1400,
	0xC3EC, 0x0386,
	0xC3EE, 0x0000,
	0xC3F0, 0x0000,
	0xC3F2, 0x0000,
	0xC3F4, 0x0BD8,
	0xC3F6, 0x0508,
	0xC3F8, 0x0000,
	0xC3FA, 0x8500,
	0xC3FC, 0x0005,
	0xC3FE, 0xFFFF,
	0xC400, 0x000F,
	0xC402, 0x1FFF,
	0xC404, 0x00FB,
	0xC406, 0x02F2,
	0xC408, 0x0001,
	0xC40A, 0x0003,
	0xC40C, 0x7712,
	0xC40E, 0x884B,
	0xC410, 0x0000,
	0xC412, 0x0300,
	0xC414, 0x0100,
	0xC416, 0x8018,
	0xC418, 0x8018,
	0xC41A, 0x8018,
	0xC41C, 0x8018,
	0xC41E, 0x0000,
	0xC420, 0xC372,
	0xC422, 0x8470,
	0xC424, 0xD566,
	0xC426, 0x0FA0,
	0xC428, 0x0BB8,
	0xC42A, 0x0223,
	0xC42C, 0x0010,
	0xC42E, 0x0010,
	0xC430, 0x0FA0,
	0xC432, 0x0BB8,
	0xC434, 0x400B,
	0xC436, 0x07D8,
	0xC438, 0x1797,
	0xC43A, 0x0000,
	0xC43C, 0x83EE,
	0xC43E, 0x0000,
	0xC440, 0x0000,
	0xC442, 0x0010,
	0xC444, 0x0010,
	0xC446, 0x0011,
	0xC448, 0x2233,
	0xC44A, 0x4455,
	0xC44C, 0x6677,
	0xC44E, 0x021A,
	0xC450, 0x0001,
	0xC452, 0x0108,
	0xC454, 0x0100,
	0xC456, 0x0004,
	0xC458, 0x0100,
	0xC45A, 0x1100,
	0xC45C, 0x1000,
	0xC45E, 0x0E3C,
	0xC460, 0xC860,
	0xC462, 0x0001,
	0xC464, 0x0200,
	0xC466, 0x0200,
	0xC468, 0x0200,
	0xC46A, 0x0200,
	0xC46C, 0x0200,
	0xC46E, 0x0200,
	0xC470, 0x0200,
	0xC472, 0x0200,
	0xC474, 0xF080,
	0xC476, 0x0010,
	0xC478, 0x0FA0,
	0xC47A, 0x001C,
	0xC47C, 0x0BA0,
	0xC47E, 0x0D11,
	0xC480, 0x1C3C,
	0xC482, 0x0001,
	0xC484, 0x0201,
	0xC486, 0x0020,
	0xC488, 0x0214,
	0xC48A, 0x8141,
	0xC48C, 0x0014,
	0xC48E, 0x0001,
	0xC490, 0x0140,
	0xC492, 0x0030,
	0xC494, 0x0006,
	0xC298, 0x07FF,
	0xC29A, 0x07FF,
	0xC29C, 0x07FF,
	0xC29E, 0x07FF,
	0xC2A2, 0x07FF,
	0xC2A4, 0x07FF,
	0xC2A6, 0x07FF,
	0xC2A8, 0x07FF,
	0xC2AC, 0x07FF,
	0xC2AE, 0x07FF,
	0xC2B0, 0x07FF,
	0xC2B2, 0x07FF,
	0xC2B6, 0x07FF,
	0xC2B8, 0x07FF,
	0xC2BA, 0x07FF,
	0xC2BC, 0x07FF,
	0x8002, 0x0044,
	0x8004, 0x0047,
	0x8006, 0x004A,
	0x8008, 0x004D,
	0x800A, 0x0052,
	0x800C, 0x0056,
	0x800E, 0x005A,
	0x8010, 0x005F,
	0x8012, 0x0060,
	0x8014, 0x0065,
	0x8016, 0x006B,
	0x8018, 0x006E,
	0x801A, 0x0071,
	0x801C, 0x0076,
	0x801E, 0x007C,
	0x8020, 0x007C,
	0x8022, 0x007C,
	0x8024, 0x0083,
	0x8026, 0x008D,
	0x8028, 0x008F,
	0x802A, 0x0099,
	0x802C, 0x009A,
	0x802E, 0x009B,
	0x8030, 0x00A3,
	0x8032, 0x00A8,
	0x8034, 0x00AC,
	0x8036, 0x00B0,
	0x8038, 0x00B7,
	0x803A, 0x00B8,
	0x803C, 0x00AF,
	0x803E, 0x00AF,
	0x8126, 0x1E00,
	0x812A, 0x1C00,
	0x813A, 0x1500,
	0x813E, 0x1680,
	0x8142, 0x1700,
	0x020C, 0x0F8F,
	0x0210, 0x0F97,
	0x238C, 0x0108,
	0x238E, 0x0108,
	0x2390, 0x0000,
	0x2392, 0x0080,
	0x2394, 0x0000,
	0x2396, 0x0000,
	0x2398, 0x8000,
	0x239A, 0x0000,
	0x239C, 0x0000,
	0x239E, 0x0000,
	0x23A0, 0x0000,
	0x23A2, 0x0080,
	0x23A4, 0x0000,
	0x23A6, 0x0000,
	0x23A8, 0x8000,
	0x23AA, 0x0000,
	0x23AC, 0x0000,
	0x23AE, 0x0000,
	0x0736, 0x0087,
	0x0738, 0x0302,
	0x073C, 0x0900,
	0x0746, 0x01BE,
	0x0748, 0x0005,
	0x0766, 0x0040,
	0x07A0, 0x0000,
	0x07AA, 0x0400,
	0x07AC, 0x0400,
	0x07AE, 0x0400,
	0x07B0, 0x0400,
	0x07B2, 0x0400,
	0x07B6, 0x0080,
	0x07C4, 0x0000,
	0x1200, 0x8902,
	0x1020, 0xC108,
	0x1022, 0x0921,
	0x1024, 0x0508,
	0x1026, 0x0C0B,
	0x1028, 0x150B,
	0x102A, 0x0D0A,
	0x102C, 0x1A0A,
	0x1062, 0x3860,
	0x106C, 0x0008,
	0xC3AC, 0x0100,
	0x0208, 0x0000,

};
#endif

static void custom2_setting(void)
{
	LOG_INF("E\n");

	//+Bug 604664 zhanghao2, add, 2021/08/04/, distinguish 801BA1 version to use 865Mhz mipi clock.
	if (true == board_is_lamei){
		w1hi4821qrearsy_table_write_cmos_sensor(
			addr_data_pair_custom2_w1hi4821qrearsy_lamei,
			sizeof(addr_data_pair_custom2_w1hi4821qrearsy_lamei) /
			sizeof(kal_uint16));
	} else {
		w1hi4821qrearsy_table_write_cmos_sensor(
			addr_data_pair_custom2_w1hi4821qrearsy,
			sizeof(addr_data_pair_custom2_w1hi4821qrearsy) /
			sizeof(kal_uint16));
	}
	//-Bug 604664 zhanghao2, add, 2021/08/04/, distinguish 801BA1 version to use 865Mhz mipi clock.
}

#if MULTI_WRITE
kal_uint16 addr_data_pair_custom3_w1hi4821qrearsy[] = {
	0x0208, 0x0100,
	0xC3B0, 0x4008,
	0xC3B2, 0x06D8,
	0xC3B4, 0x004B,
	0xC3B6, 0x1B6B,
	0xC3B8, 0x0090,
	0xC3BA, 0x1823,
	0xC3BC, 0x0101,
	0xC3BE, 0x0101,
	0xC3C0, 0x0101,
	0xC3C2, 0x0901,
	0xC3C4, 0x0101,
	0xC3C6, 0x0101,
	0xC3C8, 0x0101,
	0xC3CA, 0x0901,
	0xC3CC, 0x0101,
	0xC3CE, 0x0101,
	0xC3D0, 0x0101,
	0xC3D2, 0x0901,
	0xC3D4, 0x0044,
	0xC3D6, 0x0044,
	0xC3D8, 0x0008,
	0xC3DA, 0x0808,
	0xC3DC, 0x0220,
	0xC3DE, 0x3003,
	0xC3E0, 0x0000,
	0xC3E2, 0x0100,
	0xC3E4, 0x0001,
	0xC3E6, 0x03EC,
	0xC3E8, 0x2131,
	0xC3EA, 0x1400,
	0xC3EC, 0x06D8,
	0xC3EE, 0x0258,
	0xC3F0, 0x0000,
	0xC3F2, 0x0004,
	0xC3F4, 0x1790,
	0xC3F6, 0x0508,
	0xC3F8, 0x0000,
	0xC3FA, 0x8500,
	0xC3FC, 0x0005,
	0xC3FE, 0xFFFF,
	0xC400, 0x000F,
	0xC402, 0x1FFF,
	0xC404, 0x0000,
	0xC406, 0x03ED,
	0xC408, 0x0001,
	0xC40A, 0x025B,
	0xC40C, 0x7712,
	0xC40E, 0x884B,
	0xC410, 0x0000,
	0xC412, 0x0300,
	0xC414, 0x0100,
	0xC416, 0x8018,
	0xC418, 0x8018,
	0xC41A, 0x8018,
	0xC41C, 0x8018,
	0xC41E, 0x0000,
	0xC420, 0xC372,
	0xC422, 0x8470,
	0xC424, 0xD566,
	0xC426, 0x1F40,
	0xC428, 0x1770,
	0xC42A, 0x0223,
	0xC42C, 0x0010,
	0xC42E, 0x0010,
	0xC430, 0x1F40,
	0xC432, 0x1770,
	0xC434, 0x400B,
	0xC436, 0x0008,
	0xC438, 0x1F67,
	0xC43A, 0x0000,
	0xC43C, 0x83EE,
	0xC43E, 0x0000,
	0xC440, 0x0000,
	0xC442, 0x0010,
	0xC444, 0x0010,
	0xC446, 0x0011,
	0xC448, 0x2233,
	0xC44A, 0x4455,
	0xC44C, 0x6677,
	0xC44E, 0x021A,
	0xC450, 0x0001,
	0xC452, 0x0108,
	0xC454, 0x01F0,
	0xC456, 0x0004,
	0xC458, 0x01F0,
	0xC45A, 0x1100,
	0xC45C, 0x1000,
	0xC45E, 0x0E3C,
	0xC460, 0xC860,
	0xC462, 0x0001,
	0xC464, 0x0200,
	0xC466, 0x0200,
	0xC468, 0x0200,
	0xC46A, 0x0200,
	0xC46C, 0x0200,
	0xC46E, 0x0200,
	0xC470, 0x0200,
	0xC472, 0x0200,
	0xC474, 0xF080,
	0xC476, 0x0030,
	0xC478, 0x1F00,
	0xC47A, 0x0028,
	0xC47C, 0x1740,
	0xC47E, 0x0D11,
	0xC480, 0x1C3C,
	0xC482, 0x0001,
	0xC484, 0x0000,
	0xC486, 0x0050,
	0xC488, 0x0038,
	0xC48A, 0x8141,
	0xC48C, 0x0014,
	0xC48E, 0x0001,
	0xC490, 0x0140,
	0xC492, 0x0030,
	0xC494, 0x0006,
	0xC298, 0x07FF,
	0xC29A, 0x07FF,
	0xC29C, 0x07FF,
	0xC29E, 0x07FF,
	0xC2A2, 0x07FF,
	0xC2A4, 0x07FF,
	0xC2A6, 0x07FF,
	0xC2A8, 0x07FF,
	0xC2AC, 0x07FF,
	0xC2AE, 0x07FF,
	0xC2B0, 0x07FF,
	0xC2B2, 0x07FF,
	0xC2B6, 0x07FF,
	0xC2B8, 0x07FF,
	0xC2BA, 0x07FF,
	0xC2BC, 0x07FF,
	0x8002, 0x0044,
	0x8004, 0x0047,
	0x8006, 0x004A,
	0x8008, 0x004D,
	0x800A, 0x0052,
	0x800C, 0x0056,
	0x800E, 0x005A,
	0x8010, 0x005F,
	0x8012, 0x0060,
	0x8014, 0x0065,
	0x8016, 0x006B,
	0x8018, 0x006E,
	0x801A, 0x0071,
	0x801C, 0x0076,
	0x801E, 0x007C,
	0x8020, 0x007C,
	0x8022, 0x007C,
	0x8024, 0x0083,
	0x8026, 0x008D,
	0x8028, 0x008F,
	0x802A, 0x0099,
	0x802C, 0x009A,
	0x802E, 0x009B,
	0x8030, 0x00A3,
	0x8032, 0x00A8,
	0x8034, 0x00AC,
	0x8036, 0x00B0,
	0x8038, 0x00B7,
	0x803A, 0x00B8,
	0x803C, 0x00AF,
	0x803E, 0x00AF,
	0x8126, 0x1E00,
	0x812A, 0x1C00,
	0x813A, 0x1500,
	0x813E, 0x1680,
	0x8142, 0x1700,
	0x020C, 0x180C,
	0x0210, 0x1814,
	0x238C, 0x0108,
	0x238E, 0x0108,
	0x2390, 0x0000,
	0x2392, 0x0080,
	0x2394, 0x0000,
	0x2396, 0x0000,
	0x2398, 0x8000,
	0x239A, 0x0000,
	0x239C, 0x0000,
	0x239E, 0x0000,
	0x23A0, 0x0000,
	0x23A2, 0x0080,
	0x23A4, 0x0000,
	0x23A6, 0x0000,
	0x23A8, 0x8000,
	0x23AA, 0x0000,
	0x23AC, 0x0000,
	0x23AE, 0x0000,
	0x0736, 0x0087,
	0x0738, 0x0302,
	0x073C, 0x0900,
	0x0746, 0x0125,
	0x0748, 0x0002,
	0x0766, 0x0064,
	0x07A0, 0x0001,
	0x07AA, 0x0200,
	0x07AC, 0x0200,
	0x07AE, 0x0200,
	0x07B0, 0x0200,
	0x07B2, 0x0200,
	0x07B6, 0x0100,
	0x07C4, 0x0001,
	0x1200, 0x8902,
	0x1020, 0xC10A,
	0x1022, 0x0A2B,
	0x1024, 0x050B,
	0x1026, 0x0F0F,
	0x1028, 0x180E,
	0x102A, 0x100A,
	0x102C, 0x1B0A,
	0x1062, 0x3800,
	0x106C, 0x0018,
	0xC3AC, 0x0100,
	0x0208, 0x0000,

};
#endif


static void custom3_setting(void)
{
	LOG_INF("E\n");
	//+Bug 604664 zhanghao2, add, 2021/08/04/, distinguish 801BA1 version to use 865Mhz mipi clock.
	if (true == board_is_lamei){
		w1hi4821qrearsy_table_write_cmos_sensor(
			addr_data_pair_custom3_w1hi4821qrearsy_lamei,
			sizeof(addr_data_pair_custom3_w1hi4821qrearsy_lamei) /
			sizeof(kal_uint16));
	} else {
		w1hi4821qrearsy_table_write_cmos_sensor(
			addr_data_pair_custom3_w1hi4821qrearsy,
			sizeof(addr_data_pair_custom3_w1hi4821qrearsy) /
			sizeof(kal_uint16));
	}
	//-Bug 604664 zhanghao2, add, 2021/08/04/, distinguish 801BA1 version to use 865Mhz mipi clock.


}

kal_uint16 addr_data_pair_custom4_w1hi4821qrearsy[] = {
	0x0208, 0x0100,
	0xC3B0, 0x0908,
	0xC3B2, 0x02E9,
	0xC3B4, 0x0042,
	0xC3B6, 0x1B62,
	0xC3B8, 0x0370,
	0xC3BA, 0x1532,
	0xC3BC, 0x1C04,
	0xC3BE, 0x0180,
	0xC3C0, 0x0101,
	0xC3C2, 0x0901,
	0xC3C4, 0x1C04,
	0xC3C6, 0x0180,
	0xC3C8, 0x0101,
	0xC3CA, 0x0901,
	0xC3CC, 0x1C04,
	0xC3CE, 0x0180,
	0xC3D0, 0x0101,
	0xC3D2, 0x0901,
	0xC3D4, 0x0044,
	0xC3D6, 0x0044,
	0xC3D8, 0x0010,
	0xC3DA, 0x0020,
	0xC3DC, 0x0112,
	0xC3DE, 0x380B,
	0xC3E0, 0x1455,
	0xC3E2, 0x0000,
	0xC3E4, 0x0000,
	0xC3E6, 0x00FA,
	0xC3E8, 0x2130,
	0xC3EA, 0x0400,
	0xC3EC, 0x0174,
	0xC3EE, 0x0028,
	0xC3F0, 0x0404,
	0xC3F2, 0x0002,
	0xC3F4, 0x0470,
	0xC3F6, 0x4508,
	0xC3F8, 0x0000,
	0xC3FA, 0x8557,
	0xC3FC, 0x1405,
	0xC3FE, 0xC03F,
	0xC400, 0x030F,
	0xC402, 0x1FFE,
	0xC404, 0x0002,
	0xC406, 0x03EB,
	0xC408, 0x0004,
	0xC40A, 0x002A,
	0xC40C, 0x7112,
	0xC40E, 0x884B,
	0xC410, 0x0004,
	0xC412, 0x0300,
	0xC414, 0x0100,
	0xC416, 0x8080,
	0xC418, 0x0080,
	0xC41A, 0x8040,
	0xC41C, 0x8160,
	0xC41E, 0x0000,
	0xC420, 0xC772,
	0xC422, 0xD840,
	0xC424, 0x90E4,
	0xC426, 0x07D0,
	0xC428, 0x046C,
	0xC42A, 0x0221,
	0xC42C, 0x0004,
	0xC42E, 0x0002,
	0xC430, 0x07D0,
	0xC432, 0x046C,
	0xC434, 0x4000,
	0xC436, 0x0000,
	0xC438, 0x07D7,
	0xC43A, 0x6004,
	0xC43C, 0x81F7,
	0xC43E, 0x0010,
	0xC440, 0x0000,
	0xC442, 0x0004,
	0xC444, 0x0002,
	0xC446, 0x0011,
	0xC448, 0x2233,
	0xC44A, 0x4455,
	0xC44C, 0x6677,
	0xC44E, 0x0201,
	0xC450, 0x0001,
	0xC452, 0x0008,
	0xC454, 0x01F0,
	0xC456, 0x0174,
	0xC458, 0x00F8,
	0xC45A, 0x1100,
	0xC45C, 0x1000,
	0xC45E, 0x0E3C,
	0xC460, 0x0860,
	0xC462, 0x0001,
	0xC464, 0x0200,
	0xC466, 0x0200,
	0xC468, 0x0200,
	0xC46A, 0x0200,
	0xC46C, 0x0200,
	0xC46E, 0x0200,
	0xC470, 0x0200,
	0xC472, 0x0200,
	0xC474, 0x1080,
	0xC476, 0x0030,
	0xC478, 0x1F00,
	0xC47A, 0x0028,
	0xC47C, 0x1740,
	0xC47E, 0x0305,
	0xC480, 0x1C9C,
	0xC482, 0x0000,
	0xC484, 0x0000,
	0xC486, 0x0050,
	0xC488, 0x0320,
	0xC48A, 0x8144,
	0xC48C, 0x0000,
	0xC48E, 0x0000,
	0xC490, 0x0140,
	0xC492, 0x0010,
	0xC494, 0x0004,
	0xC298, 0x0042,
	0xC29A, 0x0047,
	0xC29C, 0x0074,
	0xC29E, 0x0167,
	0xC2A2, 0x0042,
	0xC2A4, 0x0047,
	0xC2A6, 0x0074,
	0xC2A8, 0x0168,
	0xC2AC, 0x0042,
	0xC2AE, 0x0046,
	0xC2B0, 0x0074,
	0xC2B2, 0x0164,
	0xC2B6, 0x0042,
	0xC2B8, 0x0046,
	0xC2BA, 0x0070,
	0xC2BC, 0x016A,
	0x8002, 0x004C,
	0x8004, 0x0053,
	0x8006, 0x0056,
	0x8008, 0x0060,
	0x800A, 0x0069,
	0x800C, 0x006A,
	0x800E, 0x0072,
	0x8010, 0x007A,
	0x8012, 0x007E,
	0x8014, 0x0090,
	0x8016, 0x0093,
	0x8018, 0x009B,
	0x801A, 0x00A5,
	0x801C, 0x00A6,
	0x801E, 0x00AC,
	0x8020, 0x00B4,
	0x8022, 0x00BF,
	0x8024, 0x00C3,
	0x8026, 0x00C3,
	0x8028, 0x00D2,
	0x802A, 0x00DC,
	0x802C, 0x00DB,
	0x802E, 0x00EF,
	0x8030, 0x00EC,
	0x8032, 0x00F6,
	0x8034, 0x0108,
	0x8036, 0x0109,
	0x8038, 0x0109,
	0x803A, 0x0119,
	0x803C, 0x011B,
	0x803E, 0x011B,
	0x8126, 0x1C00,
	0x812A, 0x1BC0,
	0x813A, 0x1600,
	0x813E, 0x1700,
	0x8142, 0x1C00,
	0x020C, 0x0254,
	0x0210, 0x025C,
	0x238C, 0x0000,
	0x238E, 0x0000,
	0x2390, 0x0D0D,
	0x2392, 0x0D0D,
	0x2394, 0x0D0D,
	0x2396, 0x0D0D,
	0x2398, 0x0D0D,
	0x239A, 0x0D0D,
	0x239C, 0x0D0D,
	0x239E, 0x0D0D,
	0x23A0, 0x0D0D,
	0x23A2, 0x0D0D,
	0x23A4, 0x0D0D,
	0x23A6, 0x0D0D,
	0x23A8, 0x0D0D,
	0x23AA, 0x0D0D,
	0x23AC, 0x0D0D,
	0x23AE, 0x0D0D,
	0x0736, 0x006C,
	0x0738, 0x0102,
	0x073C, 0x0700,
	0x0746, 0x0125,
	0x0748, 0x0002,
	0x0766, 0x0064,
	0x07A0, 0x0001,
	0x07AA, 0x0200,
	0x07AC, 0x0200,
	0x07AE, 0x0200,
	0x07B0, 0x0200,
	0x07B2, 0x0200,
	0x07B6, 0x0100,
	0x07C4, 0x0001,
	0x1200, 0x8122,
	0x1020, 0xC10A,
	0x1022, 0x0A2B,
	0x1024, 0x050B,
	0x1026, 0x0F0F,
	0x1028, 0x180E,
	0x102A, 0x100A,
	0x102C, 0x1B0A,
	0x1062, 0x3800,
	0x106C, 0x0018,
	0xC3AC, 0x0100,
	0x0208, 0x0000,

};

static void custom4_setting(void)
{
	LOG_INF("E\n");
	//+Bug 604664 zhanghao2, add, 2021/08/04/, distinguish 801BA1 version to use 865Mhz mipi clock.
	if (true == board_is_lamei){
		w1hi4821qrearsy_table_write_cmos_sensor(
			addr_data_pair_custom4_w1hi4821qrearsy_lamei,
			sizeof(addr_data_pair_custom4_w1hi4821qrearsy_lamei) /
			sizeof(kal_uint16));
	} else {
		w1hi4821qrearsy_table_write_cmos_sensor(
			addr_data_pair_custom4_w1hi4821qrearsy,
			sizeof(addr_data_pair_custom4_w1hi4821qrearsy) /
			sizeof(kal_uint16));
	}
	//-Bug 604664 zhanghao2, add, 2021/08/04/, distinguish 801BA1 version to use 865Mhz mipi clock.

}

#if W1HI4821QREARSY_OTP_ENABLE
//+bug584789 chenbocheng.wt, add, 2020/11/16, add main camera w1hi4821qrearsy otp code
#include "cam_cal_define.h"
#include <linux/slab.h>
//+bug604664 zhouyikuan, add, 2020/12/18,w1hi4821qrearsy_qt eeprom bring up
static struct stCAM_CAL_DATAINFO_STRUCT st_rear_w1hi4821qrearsy_eeprom_data ={
	.sensorID= W1HI4821QREARSY_SENSOR_ID,
	.deviceID = 0x01,
	.dataLength = 0x1638,
	.sensorVendorid = 0x01000000,
	.vendorByte = {1,2,3,4},
	.dataBuffer = NULL,
};

static struct stCAM_CAL_CHECKSUM_STRUCT st_rear_w1hi4821qrearsy_Checksum[11] =
{
	{MODULE_ITEM,0x0000,0x0000,0x0007,0x0008,0x55},
	{AWB_ITEM,0x0009,0x0009,0x0019,0x001A,0x55},
	{AF_ITEM,0x001B,0x001B,0x0020,0x0021,0x55},
	{LSC_ITEM,0x0022,0x0022,0x076E,0x076F,0x55},
	{PDAF_ITEM,0x0770,0x0770,0x0960,0x0961,0x55},
	{PDAF_PROC2_ITEM,0x0962,0x0962,0x0D4E,0x0D4F,0x55},
	{HI4821Q_XGC,0x0D50,0x0D50,0x1005,0x1006,0x55},
	{HI4821Q_QGC,0x1007,0x1007,0x13F7,0x13F8,0x55},
	{HI4821Q_BPGC,0x13F9,0x13F9,0x1635,0x1636,0x55},
	{TOTAL_ITEM,0x0000,0x0000,0x1636,0x1637,0x55},
	{MAX_ITEM,0xFFFF,0xFFFF,0xFFFF,0xFFFF,0x55},  // this line must haved
};
//-bug604664 zhouyikuan, add, 2020/12/18,w1hi4821qrearsy_qt eeprom bring up
extern int imgSensorReadEepromData(struct stCAM_CAL_DATAINFO_STRUCT* pData,
	struct stCAM_CAL_CHECKSUM_STRUCT* checkData);
extern int imgSensorSetEepromData(struct stCAM_CAL_DATAINFO_STRUCT* pData);

//-bug584789 chenbocheng.wt, add, 2020/11/16, add main camera w1hi4821qrearsy otp code
#endif

static kal_uint32 get_imgsensor_id(UINT32 *sensor_id)
{
	kal_uint8 i = 0;
	kal_uint8 retry = 2;
	//+bug604664 zhouyikuan, add, 2020/12/18,w1hi4821qrearsy_qt eeprom bring up
#if W1HI4821QREARSY_OTP_ENABLE
	kal_int32 size = 0;
#endif
	//+Bug 604664 zhanghao2, add, 2021/08/04/, distinguish 801BA1 version to use 865Mhz mipi clock.
	sar_get_boardid();
	pr_info("board_is_lamei=%d\n", board_is_lamei);
	//-Bug 604664 zhanghao2, add, 2021/08/04/, distinguish 801BA1 version to use 865Mhz mipi clock.

	while (imgsensor_info.i2c_addr_table[i] != 0xff) {
		spin_lock(&imgsensor_drv_lock);
		imgsensor.i2c_write_id = imgsensor_info.i2c_addr_table[i];
		spin_unlock(&imgsensor_drv_lock);
		do {
			*sensor_id = return_sensor_id();
			if (*sensor_id == imgsensor_info.sensor_id) {
			pr_info("i2c write id : 0x%x, sensor id: 0x%x\n",
			imgsensor.i2c_write_id, *sensor_id);

#if W1HI4821QREARSY_OTP_ENABLE
			//+bug584789 chenbocheng.wt, add, 2020/11/16, add main camera w1hi4821qrearsy otp code
			size = imgSensorReadEepromData(&st_rear_w1hi4821qrearsy_eeprom_data,st_rear_w1hi4821qrearsy_Checksum);
			if(size != st_rear_w1hi4821qrearsy_eeprom_data.dataLength || (st_rear_w1hi4821qrearsy_eeprom_data.sensorVendorid >> 24)!= w1hi4821qrearsy_read_eeprom(0x0001)) {
				pr_err("get eeprom data failed\n");
				if(st_rear_w1hi4821qrearsy_eeprom_data.dataBuffer != NULL) {
					kfree(st_rear_w1hi4821qrearsy_eeprom_data.dataBuffer);
					st_rear_w1hi4821qrearsy_eeprom_data.dataBuffer = NULL;
				}
				*sensor_id = 0xFFFFFFFF;
				return ERROR_SENSOR_CONNECT_FAIL;
			} else {
				pr_info("get eeprom data success\n");
				imgSensorSetEepromData(&st_rear_w1hi4821qrearsy_eeprom_data);

				#if HI4821Q_XGC_QGC_BPGC_CALIB
					//get the xgc qgc bpgc buffer
					xgc_data_buffer_sy = &(st_rear_w1hi4821qrearsy_eeprom_data.dataBuffer[st_rear_w1hi4821qrearsy_Checksum[HI4821Q_XGC - 1].startAdress + 1]);
					qgc_data_buffer_sy = &(st_rear_w1hi4821qrearsy_eeprom_data.dataBuffer[st_rear_w1hi4821qrearsy_Checksum[HI4821Q_QGC - 1].startAdress + 1]);
					bpgc_data_buffer_sy = &(st_rear_w1hi4821qrearsy_eeprom_data.dataBuffer[st_rear_w1hi4821qrearsy_Checksum[HI4821Q_BPGC - 1].startAdress + 1]);


					#if W1HI4821QREARSY_OTP_DUMP
						pr_info("w1hi4821qrearsy_eeprom:xgc_addr:0x%x\n",st_rear_w1hi4821qrearsy_Checksum[HI4821Q_XGC - 1].startAdress + 1);
						pr_info("w1hi4821qrearsy_eeprom:qgc_addr:0x%x\n",st_rear_w1hi4821qrearsy_Checksum[HI4821Q_QGC - 1].startAdress + 1);
						pr_info("w1hi4821qrearsy_eeprom:bpgc_addr:0x%x\n",st_rear_w1hi4821qrearsy_Checksum[HI4821Q_BPGC - 1].startAdress + 1);

						pr_info("=====================w1hi4821qrearsy dump xgc eeprom data start====================\n");
						dumpEEPROMData(XGC_DATA_SIZE,xgc_data_buffer_sy);
						pr_info("=====================w1hi4821qrearsy dump xgc eeprom data end======================\n");

						pr_info("=====================w1hi4821qrearsy dump qgc eeprom data start====================\n");
						dumpEEPROMData(QGC_DATA_SIZE,qgc_data_buffer_sy);
						pr_info("=====================w1hi4821qrearsy dump qgc eeprom data end======================\n");

						pr_info("=====================w1hi4821qrearsy dump bpgc eeprom data start====================\n");
						dumpEEPROMData(BPGC_DATA_SIZE,bpgc_data_buffer_sy);
						pr_info("=====================w1hi4821qrearsy dump bpgc eeprom data end======================\n");
					#endif
				#endif
			}
			//-bug584789 chenbocheng.wt, add, 2020/11/16, add main camera w1hi4821qrearsy otp code
#endif
			return ERROR_NONE;
			}

			retry--;
		} while (retry > 0);
		i++;
		retry = 2;
	}
	if (*sensor_id != imgsensor_info.sensor_id) {
		pr_err("Read id fail,sensor id: 0x%x\n", *sensor_id);
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

	LOG_INF("0420 update [open]: PLATFORM:MT6833,MIPI 24LANE\n");
	while (imgsensor_info.i2c_addr_table[i] != 0xff) {
		spin_lock(&imgsensor_drv_lock);
		imgsensor.i2c_write_id = imgsensor_info.i2c_addr_table[i];
		spin_unlock(&imgsensor_drv_lock);
		do {
			sensor_id = return_sensor_id();
			if (sensor_id == imgsensor_info.sensor_id) {
				pr_info("i2c write id: 0x%x, sensor id: 0x%x\n",
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
		pr_err("open sensor id fail: 0x%x\n", sensor_id);
		return ERROR_SENSOR_CONNECT_FAIL;
	}
	/* initail sequence write in  */
	sensor_init();
	#if HI4821Q_XGC_QGC_BPGC_CALIB
		apply_sensor_BPGC_Cali();
		apply_sensor_XGC_QGC_Cali();
	#endif

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
	//imgsensor.pdaf_mode = 1;
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
	set_mirror_flip(imgsensor.mirror);

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

	if (imgsensor.current_fps == imgsensor_info.cap.max_framerate)	{
		imgsensor.pclk = imgsensor_info.cap.pclk;
		imgsensor.line_length = imgsensor_info.cap.linelength;
		imgsensor.frame_length = imgsensor_info.cap.framelength;
		imgsensor.min_frame_length = imgsensor_info.cap.framelength;
		imgsensor.autoflicker_en = KAL_FALSE;
	} else {
	 //PIP capture: 24fps for less than 13M, 20fps for 16M,15fps for 20M
		imgsensor.pclk = imgsensor_info.cap1.pclk;
		imgsensor.line_length = imgsensor_info.cap1.linelength;
		imgsensor.frame_length = imgsensor_info.cap1.framelength;
		imgsensor.min_frame_length = imgsensor_info.cap1.framelength;
		imgsensor.autoflicker_en = KAL_FALSE;
	}

	spin_unlock(&imgsensor_drv_lock);
	LOG_INF("Caputre fps:%d\n", imgsensor.current_fps);
	capture_setting(imgsensor.current_fps);
	set_mirror_flip(imgsensor.mirror);

	return ERROR_NONE;

}	/* capture() */
//	#if 0 //normal video, hs video, slim video to be added

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
	video_setting();
	set_mirror_flip(imgsensor.mirror);

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
	set_mirror_flip(imgsensor.mirror);

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
	set_mirror_flip(imgsensor.mirror);

	return ERROR_NONE;
}    /*    slim_video     */
//#endif

static kal_uint32 Custom1(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
								MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("E\n");

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
	custom1_setting();
	set_mirror_flip(imgsensor.mirror);

     return ERROR_NONE;
}

static kal_uint32 Custom2(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT
*image_window,
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
	custom2_setting();
	set_mirror_flip(imgsensor.mirror);

     return ERROR_NONE;
}

static kal_uint32 Custom3(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT
*image_window,
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
	custom3_setting();
	set_mirror_flip(imgsensor.mirror);

	return ERROR_NONE;
}   /*  Custom1	*/

static kal_uint32 Custom4(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT
*image_window,
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
	custom4_setting();
	set_mirror_flip(imgsensor.mirror);

	return ERROR_NONE;
}
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

//#if 0
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

	sensor_resolution->SensorCustom1Width  =
		imgsensor_info.custom1.grabwindow_width;
	sensor_resolution->SensorCustom1Height  =
		imgsensor_info.custom1.grabwindow_height;
//#endif
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
	sensor_info->VideoDelayFrame =
		imgsensor_info.video_delay_frame;
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

#if ENABLE_PDAF
	sensor_info->PDAF_Support = PDAF_SUPPORT_CAMSV;
#endif

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
		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount = imgsensor_info.custom1.mipi_data_lp2hs_settle_dc;
		break;
	case MSDK_SCENARIO_ID_CUSTOM2:
		sensor_info->SensorGrabStartX = imgsensor_info.custom2.startx;
		sensor_info->SensorGrabStartY = imgsensor_info.custom2.starty;

		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount = imgsensor_info.custom2.mipi_data_lp2hs_settle_dc;

		break;
	case MSDK_SCENARIO_ID_CUSTOM3:
		sensor_info->SensorGrabStartX = imgsensor_info.custom3.startx;
		sensor_info->SensorGrabStartY = imgsensor_info.custom3.starty;

		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount = imgsensor_info.custom3.mipi_data_lp2hs_settle_dc;
	case MSDK_SCENARIO_ID_CUSTOM4:
		sensor_info->SensorGrabStartX = imgsensor_info.custom4.startx;
		sensor_info->SensorGrabStartY = imgsensor_info.custom4.starty;

		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount = imgsensor_info.custom4.mipi_data_lp2hs_settle_dc;
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
		LOG_INF("capture\n");
		capture(image_window, sensor_config_data);
		break;
	case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
		LOG_INF("video preview\n");
		normal_video(image_window, sensor_config_data);
		break;
	case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
		hs_video(image_window, sensor_config_data);
		break;
	case MSDK_SCENARIO_ID_SLIM_VIDEO:
		slim_video(image_window, sensor_config_data);
		break;
	case MSDK_SCENARIO_ID_CUSTOM1:
		Custom1(image_window, sensor_config_data); // Custom1
		break;
	case MSDK_SCENARIO_ID_CUSTOM2:
		Custom2(image_window, sensor_config_data);
		break;
	case MSDK_SCENARIO_ID_CUSTOM3:
		Custom3(image_window, sensor_config_data);
		break;
	case MSDK_SCENARIO_ID_CUSTOM4:
		Custom4(image_window, sensor_config_data);
		break;
	default:
		LOG_INF("default mode\n");
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
	LOG_DBG("enable = %d, framerate = %d ", enable, framerate);
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

	LOG_DBG("scenario_id = %d, framerate = %d\n",
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
		if (imgsensor.current_fps ==
				imgsensor_info.cap1.max_framerate) {
		frame_length = imgsensor_info.cap1.pclk / framerate * 10 /
				imgsensor_info.cap1.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line = (frame_length >
			imgsensor_info.cap1.framelength) ?
			(frame_length - imgsensor_info.cap1.framelength) : 0;
		imgsensor.frame_length = imgsensor_info.cap1.framelength +
				imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		} else {
			if (imgsensor.current_fps !=
				imgsensor_info.cap.max_framerate)
			LOG_INF("fps %d fps not support,use cap: %d fps!\n",
			framerate, imgsensor_info.cap.max_framerate/10);
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
		}
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
	break;
	case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
		frame_length = imgsensor_info.hs_video.pclk /
			framerate * 10 / imgsensor_info.hs_video.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line = (frame_length >
			imgsensor_info.hs_video.framelength) ? (frame_length -
			imgsensor_info.hs_video.framelength) : 0;
		imgsensor.frame_length = imgsensor_info.hs_video.framelength +
			imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
	break;
	case MSDK_SCENARIO_ID_SLIM_VIDEO:
		frame_length = imgsensor_info.slim_video.pclk /
			framerate * 10 / imgsensor_info.slim_video.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line = (frame_length >
			imgsensor_info.slim_video.framelength) ? (frame_length -
			imgsensor_info.slim_video.framelength) : 0;
		imgsensor.frame_length =
			imgsensor_info.slim_video.framelength +
			imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
	break;
	case MSDK_SCENARIO_ID_CUSTOM1:
		frame_length = imgsensor_info.custom1.pclk / framerate * 10 / imgsensor_info.custom1.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line = (frame_length > imgsensor_info.custom1.framelength) ? (frame_length - imgsensor_info.custom1.framelength) : 0;
		imgsensor.frame_length = imgsensor_info.custom1.framelength + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
	break;
	case MSDK_SCENARIO_ID_CUSTOM2:
		frame_length = imgsensor_info.custom2.pclk / framerate * 10 / imgsensor_info.custom2.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line = (frame_length > imgsensor_info.custom2.framelength) ? (frame_length - imgsensor_info.custom2.framelength) : 0;
		imgsensor.frame_length = imgsensor_info.custom2.framelength + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
		break;
	case MSDK_SCENARIO_ID_CUSTOM3:
		frame_length = imgsensor_info.custom3.pclk / framerate * 10 / imgsensor_info.custom3.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line = (frame_length > imgsensor_info.custom3.framelength) ? (frame_length - imgsensor_info.custom3.framelength) : 0;
		imgsensor.frame_length = imgsensor_info.custom3.framelength + imgsensor.dummy_line;

		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
		break;
	case MSDK_SCENARIO_ID_CUSTOM4:
		frame_length = imgsensor_info.custom4.pclk / framerate * 10 / imgsensor_info.custom4.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line = (frame_length > imgsensor_info.custom4.framelength) ? (frame_length - imgsensor_info.custom4.framelength) : 0;
		imgsensor.frame_length = imgsensor_info.custom4.framelength + imgsensor.dummy_line;

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
	default:
		break;
	}

	return ERROR_NONE;
}

static kal_uint32 set_test_pattern_mode(kal_bool enable)
{
	LOG_INF("set_test_pattern_mode enable: %d", enable);

	if (enable) {
		//write_cmos_sensor(0x1038, 0x0000); //mipi_virtual_channel_ctrl
		//write_cmos_sensor(0x1042, 0x0008); //mipi_pd_sep_ctrl_h, mipi_pd_sep_ctrl_l
		write_cmos_sensor(0x0b04, 0x8001);
		write_cmos_sensor(0x0C0A, 0x0202);
	} else {
		//write_cmos_sensor(0x1038, 0x4100); //mipi_virtual_channel_ctrl
		//write_cmos_sensor(0x1042, 0x0108); //mipi_pd_sep_ctrl_h, mipi_pd_sep_ctrl_l
		//write_cmos_sensor(0x0b04, 0x8000);
		//write_cmos_sensor(0x0C0A, 0x0000);
		write_cmos_sensor(0x0b04, ((read_cmos_sensor(0x0b04) << 8) | (read_cmos_sensor(0x0b05)& 0xFE)));//bIT0 TEST PATTERN CONTROL
		write_cmos_sensor(0x0C0A, 0x0000);
	}

	spin_lock(&imgsensor_drv_lock);
	imgsensor.test_pattern = enable;
	spin_unlock(&imgsensor_drv_lock);

	return ERROR_NONE;
}
//+bug604664 chenbocheng.wt, add, 2021/02/27, add hi4821q seamless code
static kal_uint32 seamless_switch(enum MSDK_SCENARIO_ID_ENUM scenario_id,
     kal_uint32 shutter, kal_uint32 gain,
     kal_uint32 shutter_2ndframe, kal_uint32 gain_2ndframe)
{
	//int hi4821q_length = 0;
	//_is_seamless = true;
	memset(hi4821q_sy_i2c_data, 0x0, sizeof(hi4821q_sy_i2c_data));
	//hi4821q_sy_size_to_write = 0;

	LOG_INF("seamless_switch %d, %d, %d, %d, %d, sizeof(hi4821q_sy_i2c_data) %d\n",scenario_id, shutter, gain, shutter_2ndframe, gain_2ndframe,sizeof(hi4821q_sy_i2c_data));

	control(scenario_id, NULL, NULL);

	if((shutter != 0)&& (shutter==shutter_2ndframe))
		set_shutter(shutter);
	if((gain != 0)&&(gain==gain_2ndframe))
		set_gain(gain);

	//w1hi4821qrearsy_table_write_cmos_sensor(
		//_i2c_data,
		//_size_to_write);
	//if(shutter_2ndframe != 0)
		//set_shutter(shutter_2ndframe);
	//if(gain_2ndframe != 0)
		//set_gain(gain_2ndframe);

	//_is_seamless = false;
	pr_err("exit\n");
	return ERROR_NONE;
}
//-bug604664 chenbocheng.wt, add, 2021/02/27, add hi4821q seamless code

static kal_uint32 streaming_control(kal_bool enable)
{
	pr_debug("streaming_enable(0=Sw Standby,1=streaming): %d\n", enable);

	if (enable)
		write_cmos_sensor(0x0b00, 0x0100); // stream on
	else
		write_cmos_sensor(0x0b00, 0x0000); // stream off

	mdelay(10);
	return ERROR_NONE;
}

static kal_uint32 feature_control(MSDK_SENSOR_FEATURE_ENUM feature_id,
			UINT8 *feature_para, UINT32 *feature_para_len)
{
	UINT16 *feature_return_para_16 = (UINT16 *) feature_para;
	UINT16 *feature_data_16 = (UINT16 *) feature_para;
	UINT32 *feature_return_para_32 = (UINT32 *) feature_para;
	UINT32 *feature_data_32 = (UINT32 *) feature_para;
	UINT32 *pScenarios = NULL;
	UINT32 *pAeCtrls = NULL;
	unsigned long long *feature_data = (unsigned long long *) feature_para;

	//+bug604664,huangguoyong.wt, add, 2021/04/03, add hi4821q seamless code
	UINT32 fpsHynix;
	UINT32 TimeHynix;
	//-bug604664,huangguoyong.wt, add, 2021/04/03, add hi4821q seamless code

	struct SENSOR_WINSIZE_INFO_STRUCT *wininfo;
	MSDK_SENSOR_REG_INFO_STRUCT *sensor_reg_data =
		(MSDK_SENSOR_REG_INFO_STRUCT *) feature_para;
#ifdef ENABLE_PDAF
	struct SET_PD_BLOCK_INFO_T *PDAFinfo;
	struct SENSOR_VC_INFO_STRUCT *pvcinfo;
#endif
	if (!((feature_id == 3040) || (feature_id == 3058)))
		LOG_DBG("feature_id = %d\n", feature_id);

	switch (feature_id) {
		//+bug604664 chenbocheng.wt, add, 2021/02/27, add hi4821q seamless code
		case SENSOR_FEATURE_GET_SEAMLESS_SCENARIOS:
			LOG_DBG("seamless feature_id = %d\n", feature_id);
			pScenarios = (MUINT32 *)((uintptr_t)(*(feature_data+1)));
			LOG_INF("seamless SENSOR_FEATURE_GET_SEAMLESS_SCENARIOS %d,%d\n", *feature_data, *pScenarios);
			*pScenarios = MSDK_SCENARIO_ID_CAMERA_PREVIEW;
			LOG_INF("seamless SENSOR_FEATURE_GET_SEAMLESS_SCENARIOS %d,%d\n", *feature_data, *pScenarios);
			switch (*feature_data) {
				case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
					*pScenarios = MSDK_SCENARIO_ID_CUSTOM2;
					break;
				case MSDK_SCENARIO_ID_CUSTOM2:
					*pScenarios = MSDK_SCENARIO_ID_CAMERA_PREVIEW;
					break;
				case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
				case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
				case MSDK_SCENARIO_ID_SLIM_VIDEO:
				case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
				//case MSDK_SCENARIO_ID_CUSTOM2:
				//case MSDK_SCENARIO_ID_CUSTOM4:
				case MSDK_SCENARIO_ID_CUSTOM1:
				case MSDK_SCENARIO_ID_CUSTOM3:
				case MSDK_SCENARIO_ID_CUSTOM4:
				default:
					*pScenarios = 0xff;
					break;
			}
		LOG_DBG("SENSOR_FEATURE_GET_SEAMLESS_SCENARIOS %d %d\n",*feature_data, *pScenarios);
		break;

		case SENSOR_FEATURE_SEAMLESS_SWITCH:
			//+bug604664,huangguoyong.wt, add, 2021/04/08, add hi4821q seamless code
			fpsHynix = imgsensor_info.custom2.pclk/imgsensor.frame_length/imgsensor_info.custom2.linelength;
			TimeHynix = 1000/fpsHynix;
			mdelay(2*TimeHynix);
			//-bug604664,huangguoyong.wt, add, 2021/04/08, add hi4821q seamless code
			pAeCtrls = (MUINT32 *)((uintptr_t)(*(feature_data+1)));
			if (pAeCtrls)
				seamless_switch((*feature_data),*pAeCtrls,*(pAeCtrls+1),*(pAeCtrls+4),*(pAeCtrls+5));
			else
				seamless_switch((*feature_data), 0, 0, 0, 0);
		break;
		//+bug604664 chenbocheng.wt, add, 2021/02/27, add hi4821q seamless code
	//+for factory mode of photo black screen
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
	//-for factory mode of photo black screen
	//+ for mipi rate is 0
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
		//+for main camera hw remosaic
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
		//-for main camera hw remosaic
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
		//+for main camera hw remosaic
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
		//-for main camera hw remosaic
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		default:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
			= (imgsensor_info.pre.framelength << 16)
				+ imgsensor_info.pre.linelength;
			break;
		}
		break;
	//-for mipi rate is 0
	case SENSOR_FEATURE_SET_SHUTTER_FRAME_TIME:
		set_shutter_frame_length(
			(UINT16) *feature_data, (UINT16) *(feature_data + 1));
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
	    spin_lock(&imgsensor_drv_lock);
	    imgsensor.current_fps = *feature_data_32;
		spin_unlock(&imgsensor_drv_lock);
		LOG_INF("current fps :%d\n", imgsensor.current_fps);
	break;
	case SENSOR_FEATURE_GET_CROP_INFO:
	    LOG_INF("GET_CROP_INFO scenarioId:%d\n",
			*feature_data_32);

	    wininfo = (struct  SENSOR_WINSIZE_INFO_STRUCT *)
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
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		default:
			memcpy((void *)wininfo,
				(void *)&imgsensor_winsize_info[0],
				sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
		break;
		}
	break;
	#if 0
	case SENSOR_FEATURE_SET_IHDR_SHUTTER_GAIN:
	    LOG_INF("SENSOR_SET_SENSOR_IHDR LE=%d, SE=%d, Gain=%d\n",
			(UINT16)*feature_data, (UINT16)*(feature_data+1),
			(UINT16)*(feature_data+2));
	    ihdr_write_shutter_gain((UINT16)*feature_data,
			(UINT16)*(feature_data+1),
				(UINT16)*(feature_data+2));
	break;
	#endif
	case SENSOR_FEATURE_GET_MIPI_PIXEL_RATE:
			switch (*feature_data) {
				case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
				*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
					imgsensor_info.cap.mipi_pixel_rate;
				break;
			case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
				*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
					imgsensor_info.normal_video.mipi_pixel_rate;
				break;
			case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
				*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
					imgsensor_info.hs_video.mipi_pixel_rate;
				break;
			case MSDK_SCENARIO_ID_SLIM_VIDEO:
				*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
					imgsensor_info.slim_video.mipi_pixel_rate;
				break;
			case MSDK_SCENARIO_ID_CUSTOM1:
				*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
					imgsensor_info.custom1.mipi_pixel_rate;
				break;
			case MSDK_SCENARIO_ID_CUSTOM2:
				*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
					imgsensor_info.custom2.mipi_pixel_rate;
				break;
			case MSDK_SCENARIO_ID_CUSTOM3:
				*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
					imgsensor_info.custom3.mipi_pixel_rate;
				break;
			case MSDK_SCENARIO_ID_CUSTOM4:
				*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
					imgsensor_info.custom4.mipi_pixel_rate;
				break;
			case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
			default:
				*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
					imgsensor_info.pre.mipi_pixel_rate;
				break;
			}
	break;
#ifdef ENABLE_PDAF
/******************** PDAF START ********************/
	case SENSOR_FEATURE_GET_SENSOR_PDAF_CAPACITY:
		switch (*feature_data) {
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
		case MSDK_SCENARIO_ID_CUSTOM1:
			*(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 1;
			break;
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
		case MSDK_SCENARIO_ID_CUSTOM2:
		case MSDK_SCENARIO_ID_CUSTOM3:
		case MSDK_SCENARIO_ID_CUSTOM4:
			*(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 0;
			break;
		default:
			*(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 0;
			break;
		}
		break;
	case SENSOR_FEATURE_GET_PDAF_INFO:
		PDAFinfo = (struct SET_PD_BLOCK_INFO_T *)
			(uintptr_t)(*(feature_data+1));

		switch (*feature_data) {
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		case MSDK_SCENARIO_ID_CUSTOM1:
			imgsensor_pd_info.i4BlockNumX = 248; //4000*3000
			imgsensor_pd_info.i4BlockNumY = 186;
			memcpy((void *)PDAFinfo, (void *)&imgsensor_pd_info,
				sizeof(struct SET_PD_BLOCK_INFO_T));
			break;
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			imgsensor_pd_info.i4BlockNumX = 248; //4000*2256
			imgsensor_pd_info.i4BlockNumY = 140; //bug604664,liudiijin.wt,ADD,2021/11/12,fixed hi4821 sensor video size pd slowly issue.
			memcpy((void *)PDAFinfo, (void *)&imgsensor_pd_info,
				sizeof(struct SET_PD_BLOCK_INFO_T));
			break;
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
		case MSDK_SCENARIO_ID_CUSTOM2:
		case MSDK_SCENARIO_ID_CUSTOM3:
		case MSDK_SCENARIO_ID_CUSTOM4:
			memcpy((void *)PDAFinfo, (void *)&imgsensor_pd_info,
				sizeof(struct SET_PD_BLOCK_INFO_T));
			break;
		default:
			break;
		}
		break;
	case SENSOR_FEATURE_GET_VC_INFO:
		pr_debug("SENSOR_FEATURE_GET_VC_INFO %d\n",
			(UINT16) *feature_data);
		pvcinfo =
	    (struct SENSOR_VC_INFO_STRUCT *) (uintptr_t) (*(feature_data + 1));

		switch (*feature_data_32) {
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		case MSDK_SCENARIO_ID_CUSTOM1:
		case MSDK_SCENARIO_ID_CUSTOM2:
		case MSDK_SCENARIO_ID_CUSTOM3:
		case MSDK_SCENARIO_ID_CUSTOM4:
			pr_debug("Jesse+ CAPTURE_JPEG \n");
			memcpy((void *)pvcinfo, (void *)&SENSOR_VC_INFO[1],
			       sizeof(struct SENSOR_VC_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			pr_debug("Jesse+ VIDEO_PREVIEW \n");
			memcpy((void *)pvcinfo, (void *)&SENSOR_VC_INFO[2],
			       sizeof(struct SENSOR_VC_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		default:
			pr_debug("Jesse+ CAMERA_PREVIEW \n");
			memcpy((void *)pvcinfo, (void *)&SENSOR_VC_INFO[0],
			       sizeof(struct SENSOR_VC_INFO_STRUCT));
			break;
		}
		break;
	case SENSOR_FEATURE_GET_PDAF_DATA:
		break;
	case SENSOR_FEATURE_SET_PDAF:
			imgsensor.pdaf_mode = *feature_data_16;
		break;
/******************** PDAF END ********************/
	//+for factory mode of photo black screen
	case SENSOR_FEATURE_GET_FRAME_CTRL_INFO_BY_SCENARIO:
		/*
		* 1, if driver support new sw frame sync
		* set_shutter_frame_length() support third para auto_extend_en
		*/
		*(feature_data + 1) = 1;
		/* margin info by scenario */
		*(feature_data + 2) = imgsensor_info.margin;
		break;
	//-for factory mode of photo black screen
#endif
    //+bug 646787 zhanghao2, add, 2021/04/25,fix ITS:test_sensor_fusion fail
	case SENSOR_FEATURE_GET_OFFSET_TO_START_OF_EXPOSURE:
		*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) = 6720000;
		break;
	//-bug 646787 zhanghao2, add, 2021/04/25,fix ITS:test_sensor_fusion fail
	case SENSOR_FEATURE_SET_STREAMING_SUSPEND:
		streaming_control(KAL_FALSE);
		break;

	case SENSOR_FEATURE_SET_STREAMING_RESUME:
		if (*feature_data != 0)
			set_shutter(*feature_data);
		streaming_control(KAL_TRUE);
		break;
	//+for factory mode of photo black screen
	case SENSOR_FEATURE_GET_BINNING_TYPE:
		switch (*(feature_data + 1)) {
		//+bug604664 chenbocheng.wt, add, 2021/02/25, modify for skip dualcam mode ae transform
		case MSDK_SCENARIO_ID_CUSTOM1:
			*feature_return_para_32 = 999; /*DUALCAM_MODE*/
			break;
		//-bug604664 chenbocheng.wt, add, 2021/02/25, modify for skip dualcam mode ae transform
		case MSDK_SCENARIO_ID_CUSTOM2:
		case MSDK_SCENARIO_ID_CUSTOM3:
			*feature_return_para_32 = 1000; /*BINNING_NONE*/
			break;
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		case MSDK_SCENARIO_ID_CUSTOM4:
		default:
			//+for main camera hw remosaic
			*feature_return_para_32 = 1466; /*BINNING_AVERAGED*/
			//-for main camera hw remosaic
			break;
		}
		pr_debug("SENSOR_FEATURE_GET_BINNING_TYPE AE_binning_type:%d,\n",
			*feature_return_para_32);
			*feature_para_len = 4;
		break;
	//-for factory mode of photo black screen
	default:
	break;
	}

	return ERROR_NONE;
}   /*  feature_control()  */


static struct SENSOR_FUNCTION_STRUCT sensor_func = {
	open,
	get_info,
	get_resolution,
	feature_control,
	control,
	close
};

UINT32 W1HI4821QREARSY_MIPI_RAW_SensorInit(struct SENSOR_FUNCTION_STRUCT **pfFunc)
{
	/* To Do : Check Sensor status here */
	if (pfFunc != NULL)
		*pfFunc =  &sensor_func;
	return ERROR_NONE;
}	/*	w1hi4821qrearsy_MIPI_RAW_SensorInit	*/
