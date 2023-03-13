// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

/*****************************************************************************
 *
 * Filename:
 * ---------
 *	 sc1300csmipiraw_Sensor.c
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


#define PFX "SC1300CS_camera_sensor"
#define pr_fmt(fmt) PFX "[%s] " fmt, __func__


#include <linux/videodev2.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/atomic.h>
#include <linux/types.h>

#include "sc1300csmipiraw_Sensor.h"

int back_camera_find_success;
bool camera_back_probe_ok;//bit2
bool back_camera_otp_status;
char back_camera_msn[64];

/*===FEATURE SWITH===*/
#define FPT_PDAF_SUPPORT   //for pdaf switch
#define MT6761

#define MULTI_WRITE 0

#if MULTI_WRITE
static const int I2C_BUFFER_LEN = 225; /*trans# max is 255, each 3 bytes*/
#else
static const int I2C_BUFFER_LEN = 3;
#endif

/*hs03s_NM code for DEVAL5625-2576 by liluling at 2022/5/5 start*/
#define STEREO_CUSTOM1_30FPS 0
/*hs03s_NM code for DEVAL5625-2576 by liluling at 2022/5/5  end*/
static int islowlight;

/*
 * #define LOG_INF(format, args...) pr_debug(
 * PFX "[%s] " format, __func__, ##args)
 */
static DEFINE_SPINLOCK(imgsensor_drv_lock);


static struct imgsensor_info_struct imgsensor_info = {
	.sensor_id = SC1300CS_LY_SENSOR_ID,

	.checksum_value = 0x44724ea1,


	.pre = {
		.pclk = 120000000,				//record different mode's pclk
		.linelength  = 1250,				//record different mode's linelength
		.framelength = 3200,			//record different mode's framelength
		.startx = 0,					//record different mode's startx of grabwindow
		.starty = 0,					//record different mode's starty of grabwindow
		.grabwindow_width  = 2104,		//record different mode's width of grabwindow
		.grabwindow_height = 1560,		//record different mode's height of grabwindow
		/*	 following for MIPIDataLowPwr2HighSpeedSettleDelayCount by different scenario	*/
		.mipi_data_lp2hs_settle_dc = 85,
		/*	 following for GetDefaultFramerateByScenario()	*/
		.max_framerate = 300,
		.mipi_pixel_rate = 480000000,
	},
	.cap = {
		.pclk = 120000000,				//record different mode's pclk
		.linelength  = 1250,			//record different mode's linelength
		.framelength = 3200,			//record different mode's framelength
		.startx = 0,					//record different mode's startx of grabwindow
		.starty = 0,					//record different mode's starty of grabwindow
		.grabwindow_width  = 4208,		//record different mode's width of grabwindow
		.grabwindow_height = 3120,		//record different mode's height of grabwindow
		/*	 following for MIPIDataLowPwr2HighSpeedSettleDelayCount by different scenario	*/
		.mipi_data_lp2hs_settle_dc = 85,
		/*	 following for GetDefaultFramerateByScenario()	*/
		.max_framerate = 300,
		.mipi_pixel_rate = 480000000,
	},
	.cap1 = {
		.pclk = 120000000,				//record different mode's pclk
		.linelength  = 1250,			//record different mode's linelength
		.framelength = 3200,			//record different mode's framelength
		.startx = 0,					//record different mode's startx of grabwindow
		.starty = 0,					//record different mode's starty of grabwindow
		.grabwindow_width  = 4208,		//record different mode's width of grabwindow
		.grabwindow_height = 3120,		//record different mode's height of grabwindow
		/*	 following for MIPIDataLowPwr2HighSpeedSettleDelayCount by different scenario	*/
		.mipi_data_lp2hs_settle_dc = 85,
		/*	 following for GetDefaultFramerateByScenario()	*/
		.max_framerate = 300,
		.mipi_pixel_rate = 480000000,
	},
	.cap2 = {
		.pclk = 120000000,				//record different mode's pclk
		.linelength  = 1250,			//record different mode's linelength
		.framelength = 3200,			//record different mode's framelength
		.startx = 0,					//record different mode's startx of grabwindow
		.starty = 0,					//record different mode's starty of grabwindow
		.grabwindow_width  = 4208,		//record different mode's width of grabwindow
		.grabwindow_height = 3120,		//record different mode's height of grabwindow
		/*	 following for MIPIDataLowPwr2HighSpeedSettleDelayCount by different scenario	*/
		.mipi_data_lp2hs_settle_dc = 85,
		/*	 following for GetDefaultFramerateByScenario()	*/
		.max_framerate = 300,
		.mipi_pixel_rate = 480000000,
	},
	.normal_video = {
		.pclk = 120000000,				//record different mode's pclk
		.linelength  = 1250,			//record different mode's linelength
		.framelength = 3200,			//record different mode's framelength
		.startx = 0,					//record different mode's startx of grabwindow
		.starty = 0,					//record different mode's starty of grabwindow
		.grabwindow_width  = 4208,		//record different mode's width of grabwindow
		.grabwindow_height = 3120,		//record different mode's height of grabwindow
		/*	 following for MIPIDataLowPwr2HighSpeedSettleDelayCount by different scenario	*/
		.mipi_data_lp2hs_settle_dc = 85,
		/*	 following for GetDefaultFramerateByScenario()	*/
		.max_framerate = 300,
		.mipi_pixel_rate = 480000000,
	},
	.hs_video = {
		.pclk = 120000000,				//record different mode's pclk
		.linelength  = 1250,			//record different mode's linelength
		.framelength = 800,				//record different mode's framelength
		.startx = 0,					//record different mode's startx of grabwindow
		.starty = 0,					//record different mode's starty of grabwindow
		.grabwindow_width  = 1280,		//record different mode's width of grabwindow
		.grabwindow_height = 720,		//record different mode's height of grabwindow
		/*	 following for MIPIDataLowPwr2HighSpeedSettleDelayCount by different scenario	*/
		.mipi_data_lp2hs_settle_dc = 85,
		/*	 following for GetDefaultFramerateByScenario()	*/
		.max_framerate = 1200,
		.mipi_pixel_rate = 480000000,

	},
	.slim_video = {
		.pclk = 120000000,				//record different mode's pclk
		.linelength  = 1250,			//record different mode's linelength
		.framelength = 3200,			//record different mode's framelength
		.startx = 0,					//record different mode's startx of grabwindow
		.starty = 0,					//record different mode's starty of grabwindow
		.grabwindow_width  = 1920,		//record different mode's width of grabwindow
		.grabwindow_height = 1080,		//record different mode's height of grabwindow
		/*	 following for MIPIDataLowPwr2HighSpeedSettleDelayCount by different scenario	*/
		.mipi_data_lp2hs_settle_dc = 85,
		/*	 following for GetDefaultFramerateByScenario()	*/
		.max_framerate = 300,
		.mipi_pixel_rate = 480000000,

	},

#if STEREO_CUSTOM1_30FPS
	#if 0
	// 6 M * 30 fps
	.custom1 = {
		.pclk = 480000000,				//record different mode's pclk
		.linelength  = 4896,//5808,				//record different mode's linelength
		.framelength = 3268,			//record different mode's framelength
		.startx = 0,					//record different mode's startx of grabwindow
		.starty = 0,					//record different mode's starty of grabwindow
		.grabwindow_width  = 2816,		//record different mode's width of grabwindow
		.grabwindow_height = 2112,		//record different mode's height of grabwindow
		/*	 following for MIPIDataLowPwr2HighSpeedSettleDelayCount by different scenario	*/
		.mipi_data_lp2hs_settle_dc = 85,
		/*	 following for GetDefaultFramerateByScenario()	*/
		.max_framerate = 300,
		.mipi_pixel_rate = 480000000,
	},
	#else
	//6 M * 24 fps
	.custom1 = {
		.pclk = 120000000,				//record different mode's pclk
		.linelength  = 1250,			//record different mode's linelength
		.framelength = 3200,			//record different mode's framelength
		.startx = 0,					//record different mode's startx of grabwindow
		.starty = 0,					//record different mode's starty of grabwindow
		.grabwindow_width  = 2104,		//record different mode's width of grabwindow
		.grabwindow_height = 1560,		//record different mode's height of grabwindow
		/*	 following for MIPIDataLowPwr2HighSpeedSettleDelayCount by different scenario	*/
		.mipi_data_lp2hs_settle_dc = 85,
		/*	 following for GetDefaultFramerateByScenario()	*/
		.max_framerate = 300,
		.mipi_pixel_rate = 480000000,
	},
	#endif
#else
	#if 0
	//6 M * 24 fps
	.custom1 = {
		.pclk = 480000000,				//record different mode's pclk
		.linelength  = 4896,//5808,				//record different mode's linelength
		.framelength = 4084,			//record different mode's framelength
		.startx = 0,					//record different mode's startx of grabwindow
		.starty = 0,					//record different mode's starty of grabwindow
		.grabwindow_width  = 2816,		//record different mode's width of grabwindow
		.grabwindow_height = 2112,		//record different mode's height of grabwindow
		/*	 following for MIPIDataLowPwr2HighSpeedSettleDelayCount by different scenario	*/
		.mipi_data_lp2hs_settle_dc = 85,
		/*	 following for GetDefaultFramerateByScenario()	*/
		.max_framerate = 240,
		.mipi_pixel_rate = 480000000,
	},
	#else
/*hs03s_NM code for DEVAL5625-2576 by liluling at 2022/5/5 start*/
	//13 M * 24 fps
	.custom1 = {
		.pclk = 120000000,				//record different mode's pclk
		.linelength  = 1250,			//record different mode's linelength
		.framelength = 4000,			//record different mode's framelength
		.startx = 0,					//record different mode's startx of grabwindow
		.starty = 0,					//record different mode's starty of grabwindow
		.grabwindow_width  = 4208,		//record different mode's width of grabwindow
		.grabwindow_height = 3120,		//record different mode's height of grabwindow
		/*	 following for MIPIDataLowPwr2HighSpeedSettleDelayCount by different scenario	*/
		.mipi_data_lp2hs_settle_dc = 85,
		/*	 following for GetDefaultFramerateByScenario()	*/
		.max_framerate = 240,
		.mipi_pixel_rate = 480000000,
	},
/*hs03s_NM code for DEVAL5625-2576 by liluling at 2022/5/5  end*/
	#endif
#endif
/*hs03s_NM code for DEVAL5625-2576 by liluling at 2022/5/5 start*/
	.margin = 5,                    /*sensor framelength & shutter margin*/
/*hs03s_NM code for DEVAL5625-2576 by liluling at 2022/5/5  end*/
	.min_shutter = 4,               /*min shutter*/

	/* max framelength by sensor register's limitation */
	.max_frame_length = 0x3fff,

	/* shutter delay frame for AE cycle,
	 * 2 frame with ispGain_delay-shut_delay=2-0=2
	 */
	.ae_shut_delay_frame = 0,

	/* sensor gain delay frame for AE cycle,
	 * 2 frame with ispGain_delay-sensor_gain_delay=2-0=2
	 */
	.ae_sensor_gain_delay_frame = 0,

	.ae_ispGain_delay_frame = 2,	/* isp gain delay frame for AE cycle */
	.frame_time_delay_frame = 1,    /*The delay frame of setting frame length*/
	.ihdr_support = 0,	/* 1, support; 0,not support */
	.ihdr_le_firstline = 0,	/* 1,le first ; 0, se first */
	.sensor_mode_num = 6,	/* support sensor mode num */


	.cap_delay_frame = 3,	/* enter capture delay frame num */
	.pre_delay_frame = 3,	/* enter preview delay frame num */
	.video_delay_frame = 3,	/* enter video delay frame num */

	/* enter high speed video  delay frame num */
	.hs_video_delay_frame = 3,

	.slim_video_delay_frame = 3,	/* enter slim video delay frame num */

#ifdef CONFIG_IMPROVE_CAMERA_PERFORMANCE
	.custom1_delay_frame = 1,
#else
	.custom1_delay_frame = 3,
#endif


	.isp_driving_current = ISP_DRIVING_6MA,	/* mclk driving current */

	/* sensor_interface_type */
	.sensor_interface_type = SENSOR_INTERFACE_TYPE_MIPI,

	/* 0,MIPI_OPHY_NCSI2;  1,MIPI_OPHY_CSI2 */
	.mipi_sensor_type = MIPI_OPHY_NCSI2,

	/* 0,MIPI_SETTLEDELAY_AUTO; 1,MIPI_SETTLEDELAY_MANNUAL */
	.mipi_settle_delay_mode = 0,
	.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_B,
	.mclk = 24,         /*mclk value, suggest 24 or 26 for 24Mhz or 26Mhz*/
	.mipi_lane_num = SENSOR_MIPI_4_LANE,
	/* record sensor support all write id addr,
	 * only supprt 4 must end with 0xff
	 */
	.i2c_addr_table = {0x6c,0x20, 0xff},
	.i2c_speed = 400, /*support 1MHz write*/
};


static struct imgsensor_struct imgsensor = {
	.mirror = IMAGE_NORMAL,	/* mirrorflip information */

	/* IMGSENSOR_MODE enum value,record current sensor mode,such as:
	 * INIT, Preview, Capture, Video,High Speed Video, Slim Video
	 */
	.sensor_mode = IMGSENSOR_MODE_INIT,

	.shutter = 0x3D0,	/* current shutter */
	.gain = 0x40,		/* current gain */
	.dummy_pixel = 0,	/* current dummypixel */
	.dummy_line = 0,	/* current dummyline */
	.current_fps = 0,	/* full size current fps : 24fps for PIP,
				 * 30fps for Normal or ZSD
				 */

	/* auto flicker enable: KAL_FALSE for disable auto flicker,
	 * KAL_TRUE for enable auto flicker
	 */
	.autoflicker_en = KAL_FALSE,

	/* test pattern mode or not.
	 * KAL_FALSE for in test pattern mode,
	 * KAL_TRUE for normal output
	 */
	.test_pattern = KAL_FALSE,

	/* current scenario id */
	.current_scenario_id = MSDK_SCENARIO_ID_CAMERA_PREVIEW,

	/* sensor need support LE, SE with HDR feature */
	.ihdr_mode = KAL_FALSE,
	.i2c_write_id = 0x6c,	/* record current sensor's i2c write id */

};

/* Sensor output window information */
/*hs03s_NM code for DEVAL5625-2576 by liluling at 2022/5/5 start*/
static struct SENSOR_WINSIZE_INFO_STRUCT
	imgsensor_winsize_info[6] = {
	{ 4208, 3120,	  0,  	0, 4208, 3120, 2104, 1560,   0,	0, 2104, 1560, 	 0, 0, 2104, 1560}, // Preview
	{ 4208, 3120,	  0,  	0, 4208, 3120, 4208, 3120,   0,	0, 4208, 3120, 	 0, 0, 4208, 3120}, // capture
	{ 4208, 3120,	  0,  	0, 4208, 3120, 4208, 3120,   0,	0, 4208, 3120, 	 0, 0, 4208, 3120}, // video
	{ 4208, 3120,	824,  600, 2560, 1920, 1280,  720,   0,	0, 1280,  720, 	 0, 0, 1280,  720}, //hight speed video
	{ 4208, 3120,	184,  480, 3840, 2160, 1920, 1080,   0,	0, 1920, 1080, 	 0, 0, 1920, 1080},// slim video
#if STEREO_CUSTOM1_30FPS
	{ 4208, 3120,	  0,    0, 4208, 3120, 2104, 1560,   0,	0, 2104, 1560, 	 0, 0, 2104, 1560}, // custom1
#else
	{ 4208, 3120,	  0,    0, 4208, 3120, 4208, 3120,   0,	0, 4208, 3120, 	 0, 0, 4208, 3120}, // custom1
#endif
};
/*hs03s_NM code for DEVAL5625-2576 by liluling at 2022/5/5  end*/
#ifdef FPT_PDAF_SUPPORT
static struct SET_PD_BLOCK_INFO_T imgsensor_pd_info =
{
	.i4OffsetX = 57,
	.i4OffsetY = 57,
	.i4PitchX  = 32,
	.i4PitchY  = 32,
	.i4PairNum  = 8,
	.i4SubBlkW  = 16,
	.i4SubBlkH  = 8,
	.i4BlockNumX = 128,
	.i4BlockNumY = 94,
	.iMirrorFlip = 0,
	.i4PosL = {{58, 62},{74, 62},{66, 66},{82, 66},{58, 78},{74, 78},{66, 82},{82, 82}},
	.i4PosR = {{58, 58},{74, 58},{66, 70},{82, 70},{58, 74},{74, 74},{66, 86},{82, 86}},
};
#endif



static kal_uint16 table_write_cmos_sensor(kal_uint16 *para,
  kal_uint32 len)
{
	char puSendCmd[I2C_BUFFER_LEN];
	kal_uint32 IDX = 0;
	kal_uint16 addr = 0, data = 0;
#if MULTI_WRITE
	kal_uint32 tosend = 0;
	kal_uint16 addr_last = 0;
#endif

	while (len > IDX) {
#if MULTI_WRITE
		addr = para[IDX];

		puSendCmd[tosend++] = (char)((addr >> 8) & 0xFF);
		puSendCmd[tosend++] = (char)(addr & 0xFF);
		data = para[IDX + 1];
		puSendCmd[tosend++] = (char)(data & 0xFF);
		IDX += 2;
  		addr_last = addr;


		if ((I2C_BUFFER_LEN - tosend) <3  || IDX == len || addr != addr_last) {
			iBurstWriteReg_multi(puSendCmd, tosend,
				imgsensor.i2c_write_id,
				3, imgsensor_info.i2c_speed);

			tosend = 0;
		}
#else
  		// iWriteRegI2CTiming(puSendCmd, 3,
       		// imgsensor.i2c_write_id, imgsensor_info.i2c_speed);

  		// tosend = 0;
		addr = para[IDX];
		data = para[IDX + 1];

		puSendCmd[0] = (char)((addr >> 8) & 0xFF);
		puSendCmd[1] = (char)(addr & 0xFF);
		puSendCmd[2] = (char)(data & 0xFF);
		IDX += 2;

		iWriteRegI2C(puSendCmd, 3, imgsensor.i2c_write_id);

#endif
	//pr_debug("I2C_BUFFER_LEN IDX = %d, len = %d",IDX,len);
 }
 return 0;
}

static kal_uint16 read_cmos_sensor(kal_uint16 addr)
{
	kal_uint16 get_byte = 0;
	char pusendcmd[2] = { (char)(addr >> 8), (char)(addr & 0xFF) };

	iReadRegI2C(pusendcmd, 2, (u8 *) &get_byte, 1,
		    imgsensor.i2c_write_id);
	return get_byte;
}

static void write_cmos_sensor(kal_uint16 addr, kal_uint8 para)
{
	char pusendcmd[3] = {
		(char)(addr >> 8), (char)(addr & 0xFF), (char)(para & 0xFF)
	};

	iWriteRegI2C(pusendcmd, 3, imgsensor.i2c_write_id);
}

static void set_dummy(void)
{
	pr_debug("dummyline = %d, dummypixels = %d, frame_length = %d\n",
		 imgsensor.dummy_line, imgsensor.dummy_pixel,imgsensor.frame_length);

	/* return; //for test */
	write_cmos_sensor(0x320e, (imgsensor.frame_length>>8) & 0xFF);
	write_cmos_sensor(0x320f, imgsensor.frame_length & 0xFF);
}				/*      set_dummy  */

static kal_uint32 return_sensor_id(void)
{
	return ((read_cmos_sensor(0x3107) << 8) | read_cmos_sensor(0x3108));
}

static void set_max_framerate(UINT16 framerate, kal_bool min_framelength_en)
{
	kal_uint32 frame_length = imgsensor.frame_length;

	pr_debug("framerate = %d, min framelength should enable %d\n",
		 framerate, min_framelength_en);

	frame_length = imgsensor.pclk / framerate * 10 / imgsensor.line_length;
	spin_lock(&imgsensor_drv_lock);
	if (frame_length >= imgsensor.min_frame_length)
		imgsensor.frame_length = frame_length;
	else
		imgsensor.frame_length = imgsensor.min_frame_length;

	if (imgsensor.frame_length > imgsensor_info.max_frame_length) {
		imgsensor.frame_length = imgsensor_info.max_frame_length;

	}
		imgsensor.dummy_line =
			imgsensor.frame_length - imgsensor.min_frame_length;
	if (min_framelength_en)
		imgsensor.min_frame_length = imgsensor.frame_length;
	spin_unlock(&imgsensor_drv_lock);
	set_dummy();
}				/*      set_max_framerate  */

static kal_uint32 streaming_control(kal_bool enable)
{
	pr_debug("streaming_enable(0=Sw Standby,1=streaming): %d\n", enable);

	if (enable) {
		write_cmos_sensor(0x0100, 0x01); // stream on
		write_cmos_sensor(0x302d, 0x00);
	} else
		write_cmos_sensor(0x0100, 0x00); // stream off

	mdelay(10);
	return ERROR_NONE;
}

static void write_shutter(kal_uint16 shutter)
{

	kal_uint16 realtime_fps = 0;

	spin_lock(&imgsensor_drv_lock);

	if (shutter > imgsensor.min_frame_length * 2 - imgsensor_info.margin)
		imgsensor.frame_length = (shutter + imgsensor_info.margin + 1) / 2;
	else
		imgsensor.frame_length = imgsensor.min_frame_length;

	if (imgsensor.frame_length > imgsensor_info.max_frame_length)
		imgsensor.frame_length = imgsensor_info.max_frame_length;
	spin_unlock(&imgsensor_drv_lock);

	shutter = (shutter < imgsensor_info.min_shutter) ?
		imgsensor_info.min_shutter : shutter;
	shutter = (shutter >
		(imgsensor_info.max_frame_length*2 - imgsensor_info.margin)) ?
		(imgsensor_info.max_frame_length*2 - imgsensor_info.margin) :
		shutter;
	realtime_fps = imgsensor.pclk * 10 /
		(imgsensor.line_length * imgsensor.frame_length);
	if (imgsensor.autoflicker_en) {
		if (realtime_fps >= 297 && realtime_fps <= 305)
			set_max_framerate(296, 0);
		else if (realtime_fps >= 147 && realtime_fps <= 150)
			set_max_framerate(146, 0);
		else
			set_max_framerate(realtime_fps, 0);
	} else {
		if (realtime_fps > 300 && realtime_fps < 320)
			set_max_framerate(300, 0);
	}

	/* Update Shutter*/
	write_cmos_sensor(0x3e20, (shutter >> 20) & 0x0F);
	write_cmos_sensor(0x3e00, (shutter >> 12) & 0xFF);
	write_cmos_sensor(0x3e01, (shutter >>  4) & 0xFF);
	write_cmos_sensor(0x3e02, (shutter <<  4) & 0xF0);
	pr_debug("shutter =%d, framelength =%d\n",
			shutter, imgsensor.frame_length);
}				/*      write_shutter  */


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
	unsigned long flags;

	spin_lock_irqsave(&imgsensor_drv_lock, flags);
	imgsensor.shutter = shutter;
	shutter = shutter*2;
	spin_unlock_irqrestore(&imgsensor_drv_lock, flags);

	write_shutter(shutter);
}				/*      set_shutter */
/*hs03s_NM code for DEVAL5625-2576 by liluling at 2022/5/5 start*/
static void set_shutter_frame_length(kal_uint16 shutter, kal_uint16 frame_length)
{
	unsigned long flags;
	kal_uint16 realtime_fps = 0;
	kal_int32 dummy_line = 0;

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

	shutter = (shutter < imgsensor_info.min_shutter) ?
			imgsensor_info.min_shutter : shutter;
	shutter = (shutter >
			(imgsensor_info.max_frame_length*2 - imgsensor_info.margin)) ?
			(imgsensor_info.max_frame_length*2 - imgsensor_info.margin) :
			shutter;

	if (imgsensor.autoflicker_en) {
		realtime_fps = imgsensor.pclk * 10 /
			(imgsensor.line_length * imgsensor.frame_length);
		if (realtime_fps >= 297 && realtime_fps <= 305)
			set_max_framerate(296, 0);
		else if (realtime_fps >= 147 && realtime_fps <= 150)
			set_max_framerate(146, 0);
		else{
			write_cmos_sensor(0x320e, (imgsensor.frame_length>>8) & 0xFF);
			write_cmos_sensor(0x320f, imgsensor.frame_length & 0xFF);
		}
	} else {
			write_cmos_sensor(0x320e, (imgsensor.frame_length>>8) & 0xFF);
			write_cmos_sensor(0x320f, imgsensor.frame_length & 0xFF);
	}

	/* Update Shutter*/
	write_cmos_sensor(0x3e20, (shutter >> 20) & 0x0F);
	write_cmos_sensor(0x3e00, (shutter >> 12) & 0xFF);
	write_cmos_sensor(0x3e01, (shutter >>  4) & 0xFF);
	write_cmos_sensor(0x3e02, (shutter <<  4) & 0xF0);
	pr_info("shutter =%d, framelength =%d\n",
				shutter, imgsensor.frame_length);

}
/*hs03s_NM code for DEVAL5625-2576 by liluling at 2022/5/5  end*/
static kal_uint16 gain2reg(const kal_uint16 gain)
{
	kal_uint16 reg_gain = gain << 4;

	if (reg_gain < SC1300CS_SENSOR_GAIN_BASE)
		reg_gain = SC1300CS_SENSOR_GAIN_BASE;
	else if (reg_gain > SC1300CS_SENSOR_GAIN_MAX)
		reg_gain = SC1300CS_SENSOR_GAIN_MAX;

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
	kal_uint32 temp_gain;
	kal_int16 gain_index;
	kal_uint16 SC1300CS_AGC_Param[SC1300CS_SENSOR_GAIN_MAP_SIZE][2] = {
		{  1024,  0x00 },
		{  2048,  0x01 },
		{  4096,  0x03 },
		{  8192,  0x07 },
		{ 16384,  0x0f },
		{ 32768,  0x1f },
	};

	reg_gain = gain2reg(gain);

	for (gain_index = SC1300CS_SENSOR_GAIN_MAP_SIZE - 1; gain_index >= 0; gain_index--)
		if (reg_gain >= SC1300CS_AGC_Param[gain_index][0])
			break;

	write_cmos_sensor(0x3e09, SC1300CS_AGC_Param[gain_index][1]);
	temp_gain = reg_gain * SC1300CS_SENSOR_GAIN_BASE / SC1300CS_AGC_Param[gain_index][0];
	write_cmos_sensor(0x3e07, (temp_gain >> 3) & 0xff);

	pr_debug("Exit! SC1300CS_AGC_Param[gain_index][1] = 0x%x, temp_gain = 0x%x, gain = 0x%x, reg_gain = %d\n",
		SC1300CS_AGC_Param[gain_index][1], temp_gain, gain, reg_gain);

	return reg_gain;
}				/*      set_gain  */

static void set_mirror_flip(kal_uint8 image_mirror)
{

	kal_uint8 itemp;

	pr_debug("image_mirror = %d\n", image_mirror);
	itemp = read_cmos_sensor(0x3221);
	itemp &= ~0x66;

	switch (image_mirror) {

	case IMAGE_NORMAL:
		write_cmos_sensor(0x3221, itemp);
		break;

	case IMAGE_V_MIRROR:
		write_cmos_sensor(0x3221, itemp | 0x06);
		break;

	case IMAGE_H_MIRROR:
		write_cmos_sensor(0x3221, itemp | 0x60);
		break;

	case IMAGE_HV_MIRROR:
		write_cmos_sensor(0x3221, itemp | 0x66);
		break;
	}
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
#if 0
static void night_mode(kal_bool enable)
{
	/*No Need to implement this function*/
}	/*	night_mode	*/
#endif


static kal_uint16 addr_data_pair_init_sc1300cs[] = {
	0x0103,0x01,
};

static void sensor_init(void)
{
	pr_debug("sensor_init() E\n");
	/* initial sequence */

	table_write_cmos_sensor(addr_data_pair_init_sc1300cs,
				sizeof(addr_data_pair_init_sc1300cs) / sizeof(kal_uint16));
}				/*      sensor_init  */



static kal_uint16 addr_data_pair_pre_sc1300cs[] = {
	0x0103,0x01,
	0x0100,0x00,
	0x36e9,0x80,
	0x36f9,0x80,
	0x36ea,0x05,
	0x36eb,0x0a,
	0x36ec,0x0b,
	0x36ed,0x24,
	0x36fa,0x06,
	0x36fd,0x24,
	0x36e9,0x04,
	0x36f9,0x04,
	0x301f,0x07,
	0x302d,0x20,
	0x3106,0x01,
	0x3208,0x08,
	0x3209,0x38,
	0x320a,0x06,
	0x320b,0x18,
	0x320e,0x0c,
	0x320f,0x80,
	0x3211,0x04,
	0x3213,0x04,
	0x3215,0x31,
	0x3220,0x01,
	0x325f,0x42,
	0x3301,0x08,
	0x3306,0x28,
	0x330b,0x88,
	0x330e,0x40,
	0x3314,0x15,
	0x331e,0x45,
	0x331f,0x65,
	0x3333,0x10,
	0x3347,0x07,
	0x335d,0x60,
	0x3364,0x57,
	0x3367,0x04,
	0x3390,0x01,
	0x3391,0x03,
	0x3392,0x07,
	0x3393,0x07,
	0x3394,0x0e,
	0x3395,0x15,
	0x33ad,0x30,
	0x33b1,0x80,
	0x33b3,0x44,
	0x341e,0x00,
	0x34a8,0x40,
	0x34a9,0x28,
	0x34ab,0x84,
	0x34ad,0x84,
	0x360f,0x01,
	0x3621,0x68,
	0x3622,0xe8,
	0x3635,0x20,
	0x3637,0x26,
	0x3638,0xc7,
	0x3639,0xf4,
	0x363a,0x82,
	0x363c,0xc0,
	0x363d,0x40,
	0x363e,0x28,
	0x3670,0x4a,
	0x3671,0xe5,
	0x3672,0x83,
	0x3673,0x83,
	0x3674,0x10,
	0x3675,0x36,
	0x3676,0x3a,
	0x367a,0x03,
	0x367b,0x0f,
	0x367c,0x03,
	0x367d,0x07,
	0x3690,0x55,
	0x3691,0x65,
	0x3692,0x65,
	0x3699,0x82,
	0x369a,0x9f,
	0x369b,0xc0,
	0x369c,0x03,
	0x369d,0x07,
	0x36a2,0x03,
	0x36a3,0x07,
	0x36b1,0xd8,
	0x36b2,0x01,
	0x3900,0x1d,
	0x3902,0xc5,
	0x391b,0x81,
	0x391c,0x30,
	0x391d,0x19,
	0x3948,0x06,
	0x394a,0x08,
	0x3952,0x02,
	0x396a,0x07,
	0x396c,0x0e,
	0x3e00,0x00,
	0x3e01,0xc7,
	0x3e02,0x60,
	0x3e20,0x00,
	0x4000,0x00,
	0x4001,0x05,
	0x4002,0x00,
	0x4003,0x00,
	0x4004,0x00,
	0x4005,0x00,
	0x4006,0x08,
	0x4007,0x31,
	0x4008,0x00,
	0x4009,0xc8,
	0x4424,0x02,
	0x4506,0x3c,
	0x4509,0x30,
	0x4800,0x24,
	0x4816,0x21,
	0x5000,0x4e,
	0x5015,0x41,
	0x5016,0x88,
	0x5017,0x22,
	0x5018,0x00,
	0x5019,0x41,
	0x501a,0x10,
	0x501b,0x41,
	0x501c,0x00,
	0x501d,0x41,
	0x501e,0x0c,
	0x501f,0x01,
	0x5799,0x06,
	0x5901,0x04,
	0x59e0,0xd8,
	0x59e1,0x08,
	0x59e2,0x10,
	0x59e3,0x08,
	0x59e4,0x00,
	0x59e5,0x10,
	0x59e6,0x08,
	0x59e7,0x00,
	0x59e8,0x18,
	0x59e9,0x0c,
	0x59ea,0x04,
	0x59eb,0x18,
	0x59ec,0x0c,
	0x59ed,0x04,
};


static void preview_setting(void)
{
	pr_debug("preview_setting() E\n");

	table_write_cmos_sensor(addr_data_pair_pre_sc1300cs,
				sizeof(addr_data_pair_pre_sc1300cs) / sizeof(kal_uint16));



}				/*      preview_setting  */



static kal_uint16 addr_data_pair_cap_sc1300cs[] = {
	0x0103,0x01,
	0x0100,0x00,
	0x36e9,0x80,
	0x36f9,0x80,
	0x36ea,0x05,
	0x36eb,0x0a,
	0x36ec,0x0b,
	0x36ed,0x24,
	0x36fa,0x06,
	0x36fd,0x24,
	0x36f9,0x04,
	0x36e9,0x04,
	0x301f,0x01,
	0x302d,0x20,
	0x3106,0x01,
	0x325f,0x42,
	0x3301,0x08,
	0x3306,0x28,
	0x330b,0x88,
	0x330e,0x40,
	0x3314,0x15,
	0x331e,0x45,
	0x331f,0x65,
	0x3333,0x10,
	0x3347,0x07,
	0x335d,0x60,
	0x3364,0x57,
	0x3367,0x04,
	0x3390,0x01,
	0x3391,0x03,
	0x3392,0x07,
	0x3393,0x07,
	0x3394,0x0e,
	0x3395,0x15,
	0x33ad,0x30,
	0x33b1,0x80,
	0x33b3,0x44,
	0x341e,0x00,
	0x34a8,0x40,
	0x34a9,0x28,
	0x34ab,0x84,
	0x34ad,0x84,
	0x360f,0x01,
	0x3621,0x68,
	0x3622,0xe8,
	0x3635,0x20,
	0x3637,0x26,
	0x3638,0xc7,
	0x3639,0xf4,
	0x363a,0x82,
	0x363c,0xc0,
	0x363d,0x40,
	0x363e,0x28,
	0x3670,0x4a,
	0x3671,0xe5,
	0x3672,0x83,
	0x3673,0x83,
	0x3674,0x10,
	0x3675,0x36,
	0x3676,0x3a,
	0x367a,0x03,
	0x367b,0x0f,
	0x367c,0x03,
	0x367d,0x07,
	0x3690,0x55,
	0x3691,0x65,
	0x3692,0x65,
	0x3699,0x82,
	0x369a,0x9f,
	0x369b,0xc0,
	0x369c,0x03,
	0x369d,0x07,
	0x36a2,0x03,
	0x36a3,0x07,
	0x36b1,0xd8,
	0x36b2,0x01,
	0x3900,0x1d,
	0x3902,0xc5,
	0x391b,0x81,
	0x391c,0x30,
	0x391d,0x19,
	0x3948,0x06,
	0x394a,0x08,
	0x3952,0x02,
	0x396a,0x07,
	0x396c,0x0e,
	0x3e00,0x01,
	0x3e01,0x8f,
	0x3e02,0x60,
	0x3e20,0x00,
	0x4000,0x00,
	0x4001,0x05,
	0x4002,0x00,
	0x4003,0x00,
	0x4004,0x00,
	0x4005,0x00,
	0x4006,0x08,
	0x4007,0x31,
	0x4008,0x00,
	0x4009,0xc8,
	0x4424,0x02,
	0x4506,0x3c,
	0x4509,0x30,
	0x5000,0x0e,
	0x5015,0x41,
	0x5016,0x88,
	0x5017,0x22,
	0x5018,0x00,
	0x5019,0x41,
	0x501a,0x10,
	0x501b,0x41,
	0x501c,0x00,
	0x501d,0x41,
	0x501e,0x0c,
	0x501f,0x01,
	0x5799,0x06,
	0x59e0,0xd8,
	0x59e1,0x08,
	0x59e2,0x10,
	0x59e3,0x08,
	0x59e4,0x00,
	0x59e5,0x10,
	0x59e6,0x08,
	0x59e7,0x00,
	0x59e8,0x18,
	0x59e9,0x0c,
	0x59ea,0x04,
	0x59eb,0x18,
	0x59ec,0x0c,
	0x59ed,0x04,
};



static void capture_setting(kal_uint16 currefps)
{
	pr_debug("capture_setting() E! currefps:%d\n", currefps);

	table_write_cmos_sensor(addr_data_pair_cap_sc1300cs,
				sizeof(addr_data_pair_cap_sc1300cs) / sizeof(kal_uint16));


}

static kal_uint16 addr_data_pair_video_sc1300cs[] = {
	0x0103,0x01,
	0x0100,0x00,
	0x36e9,0x80,
	0x36f9,0x80,
	0x36ea,0x05,
	0x36eb,0x0a,
	0x36ec,0x0b,
	0x36ed,0x24,
	0x36fa,0x06,
	0x36fd,0x24,
	0x36f9,0x04,
	0x36e9,0x04,
	0x301f,0x01,
	0x302d,0x20,
	0x3106,0x01,
	0x325f,0x42,
	0x3301,0x08,
	0x3306,0x28,
	0x330b,0x88,
	0x330e,0x40,
	0x3314,0x15,
	0x331e,0x45,
	0x331f,0x65,
	0x3333,0x10,
	0x3347,0x07,
	0x335d,0x60,
	0x3364,0x57,
	0x3367,0x04,
	0x3390,0x01,
	0x3391,0x03,
	0x3392,0x07,
	0x3393,0x07,
	0x3394,0x0e,
	0x3395,0x15,
	0x33ad,0x30,
	0x33b1,0x80,
	0x33b3,0x44,
	0x341e,0x00,
	0x34a8,0x40,
	0x34a9,0x28,
	0x34ab,0x84,
	0x34ad,0x84,
	0x360f,0x01,
	0x3621,0x68,
	0x3622,0xe8,
	0x3635,0x20,
	0x3637,0x26,
	0x3638,0xc7,
	0x3639,0xf4,
	0x363a,0x82,
	0x363c,0xc0,
	0x363d,0x40,
	0x363e,0x28,
	0x3670,0x4a,
	0x3671,0xe5,
	0x3672,0x83,
	0x3673,0x83,
	0x3674,0x10,
	0x3675,0x36,
	0x3676,0x3a,
	0x367a,0x03,
	0x367b,0x0f,
	0x367c,0x03,
	0x367d,0x07,
	0x3690,0x55,
	0x3691,0x65,
	0x3692,0x65,
	0x3699,0x82,
	0x369a,0x9f,
	0x369b,0xc0,
	0x369c,0x03,
	0x369d,0x07,
	0x36a2,0x03,
	0x36a3,0x07,
	0x36b1,0xd8,
	0x36b2,0x01,
	0x3900,0x1d,
	0x3902,0xc5,
	0x391b,0x81,
	0x391c,0x30,
	0x391d,0x19,
	0x3948,0x06,
	0x394a,0x08,
	0x3952,0x02,
	0x396a,0x07,
	0x396c,0x0e,
	0x3e00,0x01,
	0x3e01,0x8f,
	0x3e02,0x60,
	0x3e20,0x00,
	0x4000,0x00,
	0x4001,0x05,
	0x4002,0x00,
	0x4003,0x00,
	0x4004,0x00,
	0x4005,0x00,
	0x4006,0x08,
	0x4007,0x31,
	0x4008,0x00,
	0x4009,0xc8,
	0x4424,0x02,
	0x4506,0x3c,
	0x4509,0x30,
	0x5000,0x0e,
	0x5015,0x41,
	0x5016,0x88,
	0x5017,0x22,
	0x5018,0x00,
	0x5019,0x41,
	0x501a,0x10,
	0x501b,0x41,
	0x501c,0x00,
	0x501d,0x41,
	0x501e,0x0c,
	0x501f,0x01,
	0x5799,0x06,
	0x59e0,0xd8,
	0x59e1,0x08,
	0x59e2,0x10,
	0x59e3,0x08,
	0x59e4,0x00,
	0x59e5,0x10,
	0x59e6,0x08,
	0x59e7,0x00,
	0x59e8,0x18,
	0x59e9,0x0c,
	0x59ea,0x04,
	0x59eb,0x18,
	0x59ec,0x0c,
	0x59ed,0x04,
};

static void normal_video_setting(kal_uint16 currefps)
{
	pr_debug("normal_video_setting() E! currefps:%d\n", currefps);


	table_write_cmos_sensor(addr_data_pair_video_sc1300cs,
				sizeof(addr_data_pair_video_sc1300cs) / sizeof(kal_uint16));

}

static kal_uint16 addr_data_pair_hs_sc1300cs[] = {
	0x0103,0x01,
	0x0100,0x00,
	0x36e9,0x80,
	0x36f9,0x80,
	0x36ea,0x05,
	0x36eb,0x0a,
	0x36ec,0x0b,
	0x36ed,0x24,
	0x36fa,0x06,
	0x36fd,0x24,
	0x36e9,0x04,
	0x36f9,0x04,
	0x301f,0x09,
	0x302d,0x20,
	0x3106,0x01,
	0x3200,0x03,
	0x3201,0x38,
	0x3202,0x03,
	0x3203,0x48,
	0x3204,0x0d,
	0x3205,0x47,
	0x3206,0x08,
	0x3207,0xf7,
	0x3208,0x05,
	0x3209,0x00,
	0x320a,0x02,
	0x320b,0xd0,
	0x320e,0x03,
	0x320f,0x20,
	0x3210,0x00,
	0x3211,0x04,
	0x3212,0x00,
	0x3213,0x04,
	0x3215,0x31,
	0x3220,0x01,
	0x325f,0x42,
	0x3301,0x08,
	0x3306,0x28,
	0x330b,0x88,
	0x330e,0x40,
	0x3314,0x15,
	0x331e,0x45,
	0x331f,0x65,
	0x3333,0x10,
	0x3347,0x07,
	0x335d,0x60,
	0x3364,0x57,
	0x3367,0x04,
	0x3390,0x01,
	0x3391,0x03,
	0x3392,0x07,
	0x3393,0x07,
	0x3394,0x0e,
	0x3395,0x15,
	0x33ad,0x30,
	0x33b1,0x80,
	0x33b3,0x44,
	0x341e,0x00,
	0x34a8,0x40,
	0x34a9,0x28,
	0x34ab,0x84,
	0x34ad,0x84,
	0x360f,0x01,
	0x3621,0x68,
	0x3622,0xe8,
	0x3635,0x20,
	0x3637,0x26,
	0x3638,0xc7,
	0x3639,0xf4,
	0x363a,0x82,
	0x363c,0xc0,
	0x363d,0x40,
	0x363e,0x28,
	0x3670,0x4a,
	0x3671,0xe5,
	0x3672,0x83,
	0x3673,0x83,
	0x3674,0x10,
	0x3675,0x36,
	0x3676,0x3a,
	0x367a,0x03,
	0x367b,0x0f,
	0x367c,0x03,
	0x367d,0x07,
	0x3690,0x55,
	0x3691,0x65,
	0x3692,0x65,
	0x3699,0x82,
	0x369a,0x9f,
	0x369b,0xc0,
	0x369c,0x03,
	0x369d,0x07,
	0x36a2,0x03,
	0x36a3,0x07,
	0x36b1,0xd8,
	0x36b2,0x01,
	0x3900,0x1d,
	0x3902,0xc5,
	0x391b,0x81,
	0x391c,0x30,
	0x391d,0x19,
	0x3948,0x06,
	0x394a,0x08,
	0x3952,0x02,
	0x396a,0x07,
	0x396c,0x0e,
	0x3e00,0x00,
	0x3e01,0x63,
	0x3e02,0x60,
	0x3e20,0x00,
	0x4000,0x00,
	0x4001,0x05,
	0x4002,0x00,
	0x4003,0x00,
	0x4004,0x00,
	0x4005,0x00,
	0x4006,0x08,
	0x4007,0x31,
	0x4008,0x00,
	0x4009,0xc8,
	0x4424,0x02,
	0x4506,0x3c,
	0x4509,0x30,
	0x4800,0x24,
	0x4816,0x21,
	0x5000,0x4e,
	0x5015,0x41,
	0x5016,0x88,
	0x5017,0x22,
	0x5018,0x00,
	0x5019,0x41,
	0x501a,0x10,
	0x501b,0x41,
	0x501c,0x00,
	0x501d,0x41,
	0x501e,0x0c,
	0x501f,0x01,
	0x5799,0x06,
	0x5901,0x04,
	0x59e0,0xd8,
	0x59e1,0x08,
	0x59e2,0x10,
	0x59e3,0x08,
	0x59e4,0x00,
	0x59e5,0x10,
	0x59e6,0x08,
	0x59e7,0x00,
	0x59e8,0x18,
	0x59e9,0x0c,
	0x59ea,0x04,
	0x59eb,0x18,
	0x59ec,0x0c,
	0x59ed,0x04,
};

static void hs_video_setting(void)
{
	pr_debug("hs_video_setting() E\n");
	/*//720P 120fps*/
	table_write_cmos_sensor(addr_data_pair_hs_sc1300cs,
				sizeof(addr_data_pair_hs_sc1300cs) / sizeof(kal_uint16));
}

static kal_uint16 addr_data_pair_slim_sc1300cs[] = {
	0x0103,0x01,
	0x0100,0x00,
	0x36e9,0x80,
	0x36f9,0x80,
	0x36ea,0x05,
	0x36eb,0x0a,
	0x36ec,0x0b,
	0x36ed,0x24,
	0x36fa,0x06,
	0x36fd,0x24,
	0x36e9,0x04,
	0x36f9,0x04,
	0x301f,0x08,
	0x302d,0x20,
	0x3106,0x01,
	0x3200,0x00,
	0x3201,0xb8,
	0x3202,0x01,
	0x3203,0xe0,
	0x3204,0x0f,
	0x3205,0xc7,
	0x3206,0x0a,
	0x3207,0x5f,
	0x3208,0x07,
	0x3209,0x80,
	0x320a,0x04,
	0x320b,0x38,
	0x320e,0x0c,
	0x320f,0x80,
	0x3210,0x00,
	0x3211,0x04,
	0x3212,0x00,
	0x3213,0x04,
	0x3215,0x31,
	0x3220,0x01,
	0x325f,0x42,
	0x3301,0x08,
	0x3306,0x28,
	0x330b,0x88,
	0x330e,0x40,
	0x3314,0x15,
	0x331e,0x45,
	0x331f,0x65,
	0x3333,0x10,
	0x3347,0x07,
	0x335d,0x60,
	0x3364,0x57,
	0x3367,0x04,
	0x3390,0x01,
	0x3391,0x03,
	0x3392,0x07,
	0x3393,0x07,
	0x3394,0x0e,
	0x3395,0x15,
	0x33ad,0x30,
	0x33b1,0x80,
	0x33b3,0x44,
	0x341e,0x00,
	0x34a8,0x40,
	0x34a9,0x28,
	0x34ab,0x84,
	0x34ad,0x84,
	0x360f,0x01,
	0x3621,0x68,
	0x3622,0xe8,
	0x3635,0x20,
	0x3637,0x26,
	0x3638,0xc7,
	0x3639,0xf4,
	0x363a,0x82,
	0x363c,0xc0,
	0x363d,0x40,
	0x363e,0x28,
	0x3670,0x4a,
	0x3671,0xe5,
	0x3672,0x83,
	0x3673,0x83,
	0x3674,0x10,
	0x3675,0x36,
	0x3676,0x3a,
	0x367a,0x03,
	0x367b,0x0f,
	0x367c,0x03,
	0x367d,0x07,
	0x3690,0x55,
	0x3691,0x65,
	0x3692,0x65,
	0x3699,0x82,
	0x369a,0x9f,
	0x369b,0xc0,
	0x369c,0x03,
	0x369d,0x07,
	0x36a2,0x03,
	0x36a3,0x07,
	0x36b1,0xd8,
	0x36b2,0x01,
	0x3900,0x1d,
	0x3902,0xc5,
	0x391b,0x81,
	0x391c,0x30,
	0x391d,0x19,
	0x3948,0x06,
	0x394a,0x08,
	0x3952,0x02,
	0x396a,0x07,
	0x396c,0x0e,
	0x3e00,0x00,
	0x3e01,0xc7,
	0x3e02,0x60,
	0x3e20,0x00,
	0x4000,0x00,
	0x4001,0x05,
	0x4002,0x00,
	0x4003,0x00,
	0x4004,0x00,
	0x4005,0x00,
	0x4006,0x08,
	0x4007,0x31,
	0x4008,0x00,
	0x4009,0xc8,
	0x4424,0x02,
	0x4506,0x3c,
	0x4509,0x30,
	0x4800,0x24,
	0x4816,0x21,
	0x5000,0x4e,
	0x5015,0x41,
	0x5016,0x88,
	0x5017,0x22,
	0x5018,0x00,
	0x5019,0x41,
	0x501a,0x10,
	0x501b,0x41,
	0x501c,0x00,
	0x501d,0x41,
	0x501e,0x0c,
	0x501f,0x01,
	0x5799,0x06,
	0x5901,0x04,
	0x59e0,0xd8,
	0x59e1,0x08,
	0x59e2,0x10,
	0x59e3,0x08,
	0x59e4,0x00,
	0x59e5,0x10,
	0x59e6,0x08,
	0x59e7,0x00,
	0x59e8,0x18,
	0x59e9,0x0c,
	0x59ea,0x04,
	0x59eb,0x18,
	0x59ec,0x0c,
	0x59ed,0x04,
};

static void slim_video_setting(void)
{
	pr_debug("slim_video_setting() E\n");
	/* 1080p 60fps */
	table_write_cmos_sensor(addr_data_pair_slim_sc1300cs,
				sizeof(addr_data_pair_slim_sc1300cs) / sizeof(kal_uint16));
}
#if STEREO_CUSTOM1_30FPS
//6 M * 30 fps the same with cap
static kal_uint16 addr_data_pair_custom1_sc1300cs[] = {
	0x0103,0x01,
	0x0100,0x00,
	0x36e9,0x80,
	0x36f9,0x80,
	0x36ea,0x05,
	0x36eb,0x0a,
	0x36ec,0x0b,
	0x36ed,0x24,
	0x36fa,0x06,
	0x36fd,0x24,
	0x36e9,0x04,
	0x36f9,0x04,
	0x301f,0x03,
	0x302d,0x20,
	0x3106,0x01,
	0x3208,0x08,
	0x3209,0x38,
	0x320a,0x06,
	0x320b,0x18,
	0x320e,0x0c,
	0x320f,0x80,
	0x3211,0x04,
	0x3213,0x04,
	0x3215,0x31,
	0x3220,0x01,
	0x325f,0x42,
	0x3301,0x08,
	0x3306,0x28,
	0x330b,0x88,
	0x330e,0x40,
	0x3314,0x15,
	0x331e,0x45,
	0x331f,0x65,
	0x3333,0x10,
	0x3347,0x07,
	0x335d,0x60,
	0x3364,0x57,
	0x3367,0x04,
	0x3390,0x01,
	0x3391,0x03,
	0x3392,0x07,
	0x3393,0x07,
	0x3394,0x0e,
	0x3395,0x15,
	0x33ad,0x30,
	0x33b1,0x80,
	0x33b3,0x44,
	0x341e,0x00,
	0x34a8,0x40,
	0x34a9,0x28,
	0x34ab,0x84,
	0x34ad,0x84,
	0x360f,0x01,
	0x3621,0x68,
	0x3622,0xe8,
	0x3635,0x20,
	0x3637,0x26,
	0x3638,0xc7,
	0x3639,0xf4,
	0x363a,0x82,
	0x363c,0xc0,
	0x363d,0x40,
	0x363e,0x28,
	0x3670,0x4a,
	0x3671,0xe5,
	0x3672,0x83,
	0x3673,0x83,
	0x3674,0x10,
	0x3675,0x36,
	0x3676,0x3a,
	0x367a,0x03,
	0x367b,0x0f,
	0x367c,0x03,
	0x367d,0x07,
	0x3690,0x55,
	0x3691,0x65,
	0x3692,0x65,
	0x3699,0x82,
	0x369a,0x9f,
	0x369b,0xc0,
	0x369c,0x03,
	0x369d,0x07,
	0x36a2,0x03,
	0x36a3,0x07,
	0x36b1,0xd8,
	0x36b2,0x01,
	0x3900,0x1d,
	0x3902,0xc5,
	0x391b,0x81,
	0x391c,0x30,
	0x391d,0x19,
	0x3948,0x06,
	0x394a,0x08,
	0x3952,0x02,
	0x396a,0x07,
	0x396c,0x0e,
	0x3e00,0x00,
	0x3e01,0xc7,
	0x3e02,0x60,
	0x3e20,0x00,
	0x4000,0x00,
	0x4001,0x05,
	0x4002,0x00,
	0x4003,0x00,
	0x4004,0x00,
	0x4005,0x00,
	0x4006,0x08,
	0x4007,0x31,
	0x4008,0x00,
	0x4009,0xc8,
	0x4424,0x02,
	0x4506,0x3c,
	0x4509,0x30,
	0x5000,0x4e,
	0x5015,0x41,
	0x5016,0x88,
	0x5017,0x22,
	0x5018,0x00,
	0x5019,0x41,
	0x501a,0x10,
	0x501b,0x41,
	0x501c,0x00,
	0x501d,0x41,
	0x501e,0x0c,
	0x501f,0x01,
	0x5799,0x06,
	0x5901,0x04,
	0x59e0,0xd8,
	0x59e1,0x08,
	0x59e2,0x10,
	0x59e3,0x08,
	0x59e4,0x00,
	0x59e5,0x10,
	0x59e6,0x08,
	0x59e7,0x00,
	0x59e8,0x18,
	0x59e9,0x0c,
	0x59ea,0x04,
	0x59eb,0x18,
	0x59ec,0x0c,
	0x59ed,0x04,
};
#else
/*hs03s_NM code for DEVAL5625-2576 by liluling at 2022/5/5 start*/
//13 M * 24fps
static kal_uint16 addr_data_pair_custom1_sc1300cs[] = {
	0x0103,0x01,
	0x0100,0x00,
	0x36e9,0x80,
	0x36f9,0x80,
	0x36ea,0x05,
	0x36eb,0x0a,
	0x36ec,0x0b,
	0x36ed,0x24,
	0x36fa,0x06,
	0x36fd,0x24,
	0x36f9,0x04,
	0x36e9,0x04,
	0x301f,0x01,
	0x302d,0x20,
	0x3106,0x01,
	0x325f,0x42,
	0x3301,0x08,
	0x3306,0x28,
	0x330b,0x88,
	0x330e,0x40,
	0x3314,0x15,
	0x331e,0x45,
	0x331f,0x65,
	0x3333,0x10,
	0x3347,0x07,
	0x335d,0x60,
	0x3364,0x57,
	0x3367,0x04,
	0x3390,0x01,
	0x3391,0x03,
	0x3392,0x07,
	0x3393,0x07,
	0x3394,0x0e,
	0x3395,0x15,
	0x33ad,0x30,
	0x33b1,0x80,
	0x33b3,0x44,
	0x341e,0x00,
	0x34a8,0x40,
	0x34a9,0x28,
	0x34ab,0x84,
	0x34ad,0x84,
	0x360f,0x01,
	0x3621,0x68,
	0x3622,0xe8,
	0x3635,0x20,
	0x3637,0x26,
	0x3638,0xc7,
	0x3639,0xf4,
	0x363a,0x82,
	0x363c,0xc0,
	0x363d,0x40,
	0x363e,0x28,
	0x3670,0x4a,
	0x3671,0xe5,
	0x3672,0x83,
	0x3673,0x83,
	0x3674,0x10,
	0x3675,0x36,
	0x3676,0x3a,
	0x367a,0x03,
	0x367b,0x0f,
	0x367c,0x03,
	0x367d,0x07,
	0x3690,0x55,
	0x3691,0x65,
	0x3692,0x65,
	0x3699,0x82,
	0x369a,0x9f,
	0x369b,0xc0,
	0x369c,0x03,
	0x369d,0x07,
	0x36a2,0x03,
	0x36a3,0x07,
	0x36b1,0xd8,
	0x36b2,0x01,
	0x3900,0x1d,
	0x3902,0xc5,
	0x391b,0x81,
	0x391c,0x30,
	0x391d,0x19,
	0x3948,0x06,
	0x394a,0x08,
	0x3952,0x02,
	0x396a,0x07,
	0x396c,0x0e,
	0x3e00,0x01,
	0x3e01,0x8f,
	0x3e02,0x60,
	0x3e20,0x00,
	0x4000,0x00,
	0x4001,0x05,
	0x4002,0x00,
	0x4003,0x00,
	0x4004,0x00,
	0x4005,0x00,
	0x4006,0x08,
	0x4007,0x31,
	0x4008,0x00,
	0x4009,0xc8,
	0x4424,0x02,
	0x4506,0x3c,
	0x4509,0x30,
	0x5000,0x0e,
	0x5015,0x41,
	0x5016,0x88,
	0x5017,0x22,
	0x5018,0x00,
	0x5019,0x41,
	0x501a,0x10,
	0x501b,0x41,
	0x501c,0x00,
	0x501d,0x41,
	0x501e,0x0c,
	0x501f,0x01,
	0x5799,0x06,
	0x59e0,0xd8,
	0x59e1,0x08,
	0x59e2,0x10,
	0x59e3,0x08,
	0x59e4,0x00,
	0x59e5,0x10,
	0x59e6,0x08,
	0x59e7,0x00,
	0x59e8,0x18,
	0x59e9,0x0c,
	0x59ea,0x04,
	0x59eb,0x18,
	0x59ec,0x0c,
	0x59ed,0x04,

	0x320e,0x0F, //.framelength =4000,//24fpsset FOR 1300CS
	0x320f,0xA0, //.framelength =4000,//24fpsset FOR 1300CS

	0x0100,0x01,
	0x302d,0x00,
};
/*hs03s_NM code for DEVAL5625-2576 by liluling at 2022/5/5  end*/
#endif

/*hs03s_NM code for DEVAL5625-2576 by liluling at 2022/5/5 start*/
static void custom1_setting(void)
{
#if STEREO_CUSTOM1_30FPS
	pr_debug("custom1_setting() 6 M*30 fps E!\n");
#else
	pr_debug("custom1_setting() 13 M*24 fps E!\n");
#endif
	pr_debug("custom1_setting() use custom1\n");
	table_write_cmos_sensor(addr_data_pair_custom1_sc1300cs,
				sizeof(addr_data_pair_custom1_sc1300cs) / sizeof(kal_uint16));
}
/*hs03s_NM code for DEVAL5625-2576 by liluling at 2022/5/5  end*/
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
	//	kal_uint16 sp8spFlag = 0;

	/* sensor have two i2c address 0x6c 0x6d & 0x21 0x20,
	 *we should detect the module used i2c address
	 */
	while (imgsensor_info.i2c_addr_table[i] != 0xff) {
		spin_lock(&imgsensor_drv_lock);
		imgsensor.i2c_write_id = imgsensor_info.i2c_addr_table[i];
		spin_unlock(&imgsensor_drv_lock);
		do {
			*sensor_id =  return_sensor_id();
			//pr_debug("i2c write id: 0x%x, sensor id: 0x%x, imgsensor_info.sensor_id: 0x%x\n",
			//		 imgsensor.i2c_write_id, *sensor_id, imgsensor_info.sensor_id);
			if (*sensor_id == imgsensor_info.sensor_id) {
				pr_info("mjf0_i2c write id: 0x%x, sensor id: 0x%x\n",
					 imgsensor.i2c_write_id, *sensor_id);
				back_camera_find_success = 3;
				camera_back_probe_ok = 1;
				return ERROR_NONE;
			}
			pr_info("mjf1_Read sensor id fail, id: 0x%x\n",
				 imgsensor.i2c_write_id);
			retry--;
		} while (retry > 0);
		i++;
		retry = 2;
	}
	if (*sensor_id != imgsensor_info.sensor_id) {
		/* if Sensor ID is not correct, Must set *sensor_id to 0xFFFFFFFF */
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

	pr_debug("%s", __func__);

	/* sensor have two i2c address 0x6c 0x6d & 0x21 0x20,
	 * we should detect the module used i2c address
	 */
	while (imgsensor_info.i2c_addr_table[i] != 0xff) {
		spin_lock(&imgsensor_drv_lock);
		imgsensor.i2c_write_id = imgsensor_info.i2c_addr_table[i];
		spin_unlock(&imgsensor_drv_lock);
		do {
			sensor_id = return_sensor_id();

			if (sensor_id == imgsensor_info.sensor_id) {
				pr_info("mjf2_i2c write id: 0x%x, sensor id: 0x%x\n",
					 imgsensor.i2c_write_id, sensor_id);
				break;
			}

			pr_info("mjf_3Read sensor id fail, id: 0x%x\n",
				 imgsensor.i2c_write_id);
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
	imgsensor.shutter = 0x3D0;
	imgsensor.gain = 0x40;
	imgsensor.pclk = imgsensor_info.pre.pclk;
	imgsensor.frame_length = imgsensor_info.pre.framelength;
	imgsensor.line_length = imgsensor_info.pre.linelength;
	imgsensor.min_frame_length = imgsensor_info.pre.framelength;
	imgsensor.dummy_pixel = 0;
	imgsensor.dummy_line = 0;
	imgsensor.ihdr_mode = 0;
	imgsensor.test_pattern = KAL_FALSE;
	imgsensor.current_fps = imgsensor_info.pre.max_framerate;
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
	pr_debug("E\n");

	/*No Need to implement this function */

	return ERROR_NONE;
}				/*      close  */


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
static kal_uint32 preview(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT
			  *image_window,
			  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	pr_debug("preview E\n");

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_PREVIEW;
	imgsensor.pclk = imgsensor_info.pre.pclk;
	imgsensor.line_length = imgsensor_info.pre.linelength;
	imgsensor.frame_length = imgsensor_info.pre.framelength;
	imgsensor.min_frame_length = imgsensor_info.pre.framelength;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);

	preview_setting();
	set_mirror_flip(imgsensor.mirror);

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
static kal_uint32 capture(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT
			  *image_window,
			  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	pr_debug("capture E\n");
	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_CAPTURE;
	if (imgsensor.current_fps == imgsensor_info.cap1.max_framerate) {
		/* PIP capture: 24fps for less than 13M, 20fps for 16M,15fps for 20M */
		imgsensor.pclk = imgsensor_info.cap1.pclk;
		imgsensor.line_length = imgsensor_info.cap1.linelength;
		imgsensor.frame_length = imgsensor_info.cap1.framelength;
		imgsensor.min_frame_length = imgsensor_info.cap1.framelength;
		imgsensor.autoflicker_en = KAL_FALSE;
	} else if (imgsensor.current_fps ==
		   imgsensor_info.cap2.max_framerate) {
		imgsensor.pclk = imgsensor_info.cap2.pclk;
		imgsensor.line_length = imgsensor_info.cap2.linelength;
		imgsensor.frame_length = imgsensor_info.cap2.framelength;
		imgsensor.min_frame_length = imgsensor_info.cap2.framelength;
		imgsensor.autoflicker_en = KAL_FALSE;
	} else {

		if (imgsensor.current_fps != imgsensor_info.cap.max_framerate) {
			pr_debug("Warning: current_fps %d fps is not support, so use cap's setting: %d fps!\n",
				 imgsensor.current_fps,
				 imgsensor_info.cap.max_framerate / 10);
		}

		imgsensor.pclk = imgsensor_info.cap.pclk;
		imgsensor.line_length = imgsensor_info.cap.linelength;
		imgsensor.frame_length = imgsensor_info.cap.framelength;
		imgsensor.min_frame_length = imgsensor_info.cap.framelength;
		imgsensor.autoflicker_en = KAL_FALSE;
	}
	spin_unlock(&imgsensor_drv_lock);

	capture_setting(imgsensor.current_fps);
	set_mirror_flip(imgsensor.mirror);

	return ERROR_NONE;
}				/* capture() */

static kal_uint32 normal_video(
	MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
	MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	pr_debug("normal_video E\n");

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_VIDEO;
	imgsensor.pclk = imgsensor_info.normal_video.pclk;
	imgsensor.line_length = imgsensor_info.normal_video.linelength;
	imgsensor.frame_length = imgsensor_info.normal_video.framelength;
	imgsensor.min_frame_length =
		imgsensor_info.normal_video.framelength;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);

	normal_video_setting(imgsensor.current_fps);
	set_mirror_flip(imgsensor.mirror);

	return ERROR_NONE;
}				/*      normal_video   */

static kal_uint32 hs_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT
			   *image_window,
			   MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	pr_debug("hs_video E\n");

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_HIGH_SPEED_VIDEO;
	imgsensor.pclk = imgsensor_info.hs_video.pclk;
	/* imgsensor.video_mode = KAL_TRUE; */
	imgsensor.line_length = imgsensor_info.hs_video.linelength;
	imgsensor.frame_length = imgsensor_info.hs_video.framelength;
	imgsensor.min_frame_length = imgsensor_info.hs_video.framelength;
	imgsensor.dummy_line = 0;
	imgsensor.dummy_pixel = 0;
	/* imgsensor.current_fps = 300; */
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	hs_video_setting();
	set_mirror_flip(imgsensor.mirror);

	return ERROR_NONE;
}				/*      hs_video   */

static kal_uint32 slim_video(
	MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
	MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	pr_debug("slim_video E\n");

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_SLIM_VIDEO;
	imgsensor.pclk = imgsensor_info.slim_video.pclk;
	/* imgsensor.video_mode = KAL_TRUE; */
	imgsensor.line_length = imgsensor_info.slim_video.linelength;
	imgsensor.frame_length = imgsensor_info.slim_video.framelength;
	imgsensor.min_frame_length =
		imgsensor_info.slim_video.framelength;
	imgsensor.dummy_line = 0;
	imgsensor.dummy_pixel = 0;
	/* imgsensor.current_fps = 300; */
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	slim_video_setting();
	set_mirror_flip(imgsensor.mirror);

	return ERROR_NONE;
}				/*      slim_video       */

static kal_uint32 custom1(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT
			  *image_window,
			  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	pr_debug("E cur fps: %d\n", imgsensor.current_fps);
	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_CUSTOM1;
	imgsensor.pclk = imgsensor_info.custom1.pclk;
	imgsensor.line_length = imgsensor_info.custom1.linelength;
	imgsensor.frame_length = imgsensor_info.custom1.framelength;
	imgsensor.min_frame_length = imgsensor_info.custom1.framelength;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	custom1_setting();
	set_mirror_flip(imgsensor.mirror);
	//mdelay(10);

	return ERROR_NONE;
}   /*  custom1   */

static kal_uint32 get_resolution(
	MSDK_SENSOR_RESOLUTION_INFO_STRUCT(*sensor_resolution))
{
	pr_debug("get_resolution E\n");
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

	sensor_resolution->SensorCustom1Width  =
		imgsensor_info.custom1.grabwindow_width;
	sensor_resolution->SensorCustom1Height     =
		imgsensor_info.custom1.grabwindow_height;

	return ERROR_NONE;
}				/*      get_resolution  */

static kal_uint32 get_info(enum MSDK_SCENARIO_ID_ENUM
			   scenario_id,
			   MSDK_SENSOR_INFO_STRUCT *sensor_info,
			   MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	pr_debug("get_info -> scenario_id = %d\n", scenario_id);

	sensor_info->SensorClockPolarity = SENSOR_CLOCK_POLARITY_LOW;

	/* not use */
	sensor_info->SensorClockFallingPolarity =
		SENSOR_CLOCK_POLARITY_LOW;

	/* inverse with datasheet */
	sensor_info->SensorHsyncPolarity = SENSOR_CLOCK_POLARITY_LOW;

	sensor_info->SensorVsyncPolarity = SENSOR_CLOCK_POLARITY_LOW;
	sensor_info->SensorInterruptDelayLines = 4;	/* not use */
	sensor_info->SensorResetActiveHigh = FALSE;	/* not use */
	sensor_info->SensorResetDelayCount = 5;	/* not use */

	sensor_info->SensroInterfaceType =
		imgsensor_info.sensor_interface_type;
	sensor_info->MIPIsensorType = imgsensor_info.mipi_sensor_type;
	sensor_info->SettleDelayMode =
		imgsensor_info.mipi_settle_delay_mode;

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

	sensor_info->SensorMasterClockSwitch = 0;	/* not use */
	sensor_info->SensorDrivingCurrent =
		imgsensor_info.isp_driving_current;

	/* The frame of setting shutter default 0 for TG int */
	sensor_info->AEShutDelayFrame =
		imgsensor_info.ae_shut_delay_frame;

	/* The frame of setting sensor gain*/
	sensor_info->AESensorGainDelayFrame =
		imgsensor_info.ae_sensor_gain_delay_frame;

	sensor_info->AEISPGainDelayFrame =
		imgsensor_info.ae_ispGain_delay_frame;

	/*  The delay frame of setting frame length   */
	sensor_info->FrameTimeDelayFrame =
		imgsensor_info.frame_time_delay_frame;

	sensor_info->IHDR_Support = imgsensor_info.ihdr_support;
	sensor_info->IHDR_LE_FirstLine =
		imgsensor_info.ihdr_le_firstline;
	sensor_info->SensorModeNum = imgsensor_info.sensor_mode_num;
	/* A03s code for SR-AL5625-01-324 by wuwenjie at  2022/04/06 start */
	sensor_info->PDAF_Support =
		PDAF_SUPPORT_RAW;//mt6762 mt6763 mt6765 use pdo
    /* A03s code for SR-AL5625-01-324 by wuwenjie at  2022/04/06 end */
	/* 0: NO PDAF, 1: PDAF Raw Data mode,
	 * 2:PDAF VC mode(Full),
	 * 3:PDAF VC mode(Binning)
	 */
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
		sensor_info->SensorGrabStartX = imgsensor_info.custom1.startx;
		sensor_info->SensorGrabStartY = imgsensor_info.custom1.starty;
		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
			imgsensor_info.custom1.mipi_data_lp2hs_settle_dc;

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
	pr_debug("scenario_id = %d\n", scenario_id);
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
		custom1(image_window, sensor_config_data); // custom1
		break;
	default:
		pr_debug("Error ScenarioId setting");
		preview(image_window, sensor_config_data);
		return ERROR_INVALID_SCENARIO_ID;
	}
	return ERROR_NONE;
}				/* control() */



static kal_uint32 set_video_mode(UINT16 framerate)
{
	pr_debug("framerate = %d\n", framerate);
	/* SetVideoMode Function should fix framerate */
	if (framerate == 0)
		/* Dynamic frame rate */
		return ERROR_NONE;
	spin_lock(&imgsensor_drv_lock);

	if ((framerate == 300) && (imgsensor.autoflicker_en == KAL_TRUE))
		imgsensor.current_fps = 296;
	else if ((framerate == 150)
		 && (imgsensor.autoflicker_en == KAL_TRUE))
		imgsensor.current_fps = 146;
	else
		imgsensor.current_fps = framerate;
	spin_unlock(&imgsensor_drv_lock);
	set_max_framerate(imgsensor.current_fps, 1);

	return ERROR_NONE;
}

static kal_uint32 set_auto_flicker_mode(
	kal_bool enable, UINT16 framerate)
{
	pr_debug("enable = %d, framerate = %d\n", enable, framerate);
	spin_lock(&imgsensor_drv_lock);
	if (enable)		/* enable auto flicker */
		imgsensor.autoflicker_en = KAL_TRUE;
	else			/* Cancel Auto flick */
		imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	return ERROR_NONE;
}


static kal_uint32 set_max_framerate_by_scenario(
	enum MSDK_SCENARIO_ID_ENUM scenario_id,	MUINT32 framerate)
{
	kal_uint32 frame_length;

	pr_debug("scenario_id = %d, framerate = %d\n", scenario_id,
		 framerate);

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
		if (imgsensor.frame_length > imgsensor.shutter)
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
			? (frame_length - imgsensor_info.normal_video.  framelength) : 0;

		imgsensor.frame_length =
			imgsensor_info.normal_video.framelength + imgsensor.dummy_line;

		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);

		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
		break;

	case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		if (imgsensor.current_fps == imgsensor_info.cap1.max_framerate) {

			frame_length = imgsensor_info.cap1.pclk
				       / framerate * 10 / imgsensor_info.cap1.linelength;

			spin_lock(&imgsensor_drv_lock);
			imgsensor.dummy_line =
				(frame_length > imgsensor_info.cap1.framelength)
				? (frame_length - imgsensor_info.cap1.  framelength) : 0;

			imgsensor.frame_length =
				imgsensor_info.cap1.framelength + imgsensor.dummy_line;
			imgsensor.min_frame_length = imgsensor.frame_length;
			spin_unlock(&imgsensor_drv_lock);
		} else if (imgsensor.current_fps ==
			   imgsensor_info.cap2.max_framerate) {
			frame_length = imgsensor_info.cap2.pclk
				       / framerate * 10 / imgsensor_info.cap2.linelength;
			spin_lock(&imgsensor_drv_lock);
			imgsensor.dummy_line =
				(frame_length > imgsensor_info.cap2.framelength)
				? (frame_length - imgsensor_info.cap2.  framelength) : 0;

			imgsensor.frame_length =
				imgsensor_info.cap2.framelength + imgsensor.dummy_line;

			imgsensor.min_frame_length = imgsensor.frame_length;
			spin_unlock(&imgsensor_drv_lock);
		} else {
			if (imgsensor.current_fps != imgsensor_info.cap.max_framerate)
				pr_debug("Warning: current_fps %d fps is not support, so use cap's setting: %d fps!\n",
					 framerate,
					 imgsensor_info.cap.max_framerate / 10);

			frame_length = imgsensor_info.cap.pclk
				       / framerate * 10 / imgsensor_info.cap.linelength;
			spin_lock(&imgsensor_drv_lock);
			imgsensor.dummy_line =
				(frame_length > imgsensor_info.cap.framelength)
				? (frame_length - imgsensor_info.cap.framelength) : 0;
			imgsensor.frame_length =
				imgsensor_info.cap.framelength + imgsensor.dummy_line;
			imgsensor.min_frame_length = imgsensor.frame_length;
			spin_unlock(&imgsensor_drv_lock);
		}
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
		break;

	case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
		frame_length = imgsensor_info.hs_video.pclk
			       / framerate * 10 / imgsensor_info.hs_video.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line =
			(frame_length > imgsensor_info.hs_video.framelength)
			? (frame_length - imgsensor_info.hs_video.  framelength) : 0;

		imgsensor.frame_length =
			imgsensor_info.hs_video.framelength + imgsensor.dummy_line;

		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
		break;
	case MSDK_SCENARIO_ID_SLIM_VIDEO:
		frame_length = imgsensor_info.slim_video.pclk
			       / framerate * 10 / imgsensor_info.slim_video.linelength;

		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line =
			(frame_length > imgsensor_info.slim_video.framelength)
			? (frame_length - imgsensor_info.slim_video.  framelength) : 0;

		imgsensor.frame_length =
			imgsensor_info.slim_video.framelength + imgsensor.dummy_line;

		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
		break;

	case MSDK_SCENARIO_ID_CUSTOM1:
		frame_length = imgsensor_info.custom1.pclk
			       / framerate * 10 / imgsensor_info.custom1.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line =
			(frame_length > imgsensor_info.custom1.framelength)
			? (frame_length - imgsensor_info.custom1.framelength) : 0;
		if (imgsensor.dummy_line < 0)
			imgsensor.dummy_line = 0;			
		imgsensor.frame_length = imgsensor_info.custom1.framelength +
					imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter)
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
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
		pr_debug("error scenario_id = %d, we use preview scenario\n",
			 scenario_id);
		break;
	}
	return ERROR_NONE;
}


static kal_uint32 get_default_framerate_by_scenario(
	enum MSDK_SCENARIO_ID_ENUM scenario_id, MUINT32 *framerate)
{
	pr_debug("scenario_id = %d\n", scenario_id);

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
		pr_debug("custom1 *framerate = %d\n", *framerate);
		break;

	default:
		break;
	}

	return ERROR_NONE;
}

static kal_uint32 set_test_pattern_mode(kal_bool enable)
{
	kal_uint8 itemp;
	pr_debug("enable: %d\n", enable);

	itemp = read_cmos_sensor(0x4501);
	itemp &= ~0x80;
	if (enable) {
		write_cmos_sensor(0x4501, itemp);
		//write_cmos_sensor(0x0200, 0x0002);
	} else {
		write_cmos_sensor(0x4501, itemp | 0x80);
		//write_cmos_sensor(0x0200, 0x0000);
	}
	spin_lock(&imgsensor_drv_lock);
	imgsensor.test_pattern = enable;
	spin_unlock(&imgsensor_drv_lock);
	return ERROR_NONE;
}

static kal_uint32 feature_control(MSDK_SENSOR_FEATURE_ENUM
				  feature_id,
				  UINT8 *feature_para, UINT32 *feature_para_len)
{
	UINT16 *feature_return_para_16 = (UINT16 *) feature_para;
	UINT16 *feature_data_16 = (UINT16 *) feature_para;
	UINT32 *feature_return_para_32 = (UINT32 *) feature_para;
	UINT32 *feature_data_32 = (UINT32 *) feature_para;
	//INT32 *feature_return_para_i32 = (INT32 *) feature_para;
	unsigned long long *feature_data =
		(unsigned long long *) feature_para;

#ifdef FPT_PDAF_SUPPORT
	struct SET_PD_BLOCK_INFO_T *PDAFinfo;
#endif

	struct SENSOR_WINSIZE_INFO_STRUCT *wininfo;

	MSDK_SENSOR_REG_INFO_STRUCT *sensor_reg_data =
		(MSDK_SENSOR_REG_INFO_STRUCT *) feature_para;

	pr_debug("feature_id = %d\n", feature_id);
	switch (feature_id) {
	case SENSOR_FEATURE_GET_PERIOD:
		*feature_return_para_16++ = imgsensor.line_length;
		*feature_return_para_16 = imgsensor.frame_length;
		*feature_para_len = 4;
		break;
	case SENSOR_FEATURE_GET_PIXEL_CLOCK_FREQ:
#if 0
		pr_debug(
			"feature_Control imgsensor.pclk = %d,imgsensor.current_fps = %d\n",
			imgsensor.pclk, imgsensor.current_fps);
#endif
		*feature_return_para_32 = imgsensor.pclk;
		*feature_para_len = 4;
		break;
	case SENSOR_FEATURE_SET_ESHUTTER:
		set_shutter(*feature_data);
		break;
	case SENSOR_FEATURE_SET_NIGHTMODE:
		/* night_mode((BOOL) *feature_data); no need to implement this mode */
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
		/* get the lens driver ID from EEPROM or
		 * just return LENS_DRIVER_ID_DO_NOT_CARE
		 */
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
		set_auto_flicker_mode((BOOL) (*feature_data_16),
				      *(feature_data_16 + 1));
		break;
	case SENSOR_FEATURE_SET_MAX_FRAME_RATE_BY_SCENARIO:
		set_max_framerate_by_scenario(
			(enum MSDK_SCENARIO_ID_ENUM) *feature_data, *(feature_data + 1));
		break;

	case SENSOR_FEATURE_GET_DEFAULT_FRAME_RATE_BY_SCENARIO:
		get_default_framerate_by_scenario(
			(enum MSDK_SCENARIO_ID_ENUM) *(feature_data),
			(MUINT32 *) (uintptr_t) (*(feature_data + 1)));
		break;
	case SENSOR_FEATURE_SET_TEST_PATTERN:
		set_test_pattern_mode((BOOL) (*feature_data));
		break;

	/* for factory mode auto testing */
	case SENSOR_FEATURE_GET_TEST_PATTERN_CHECKSUM_VALUE:
		*feature_return_para_32 = imgsensor_info.checksum_value;
		*feature_para_len = 4;
		break;
	case SENSOR_FEATURE_SET_FRAMERATE:
		pr_debug("current fps :%d\n", *feature_data_32);
		spin_lock(&imgsensor_drv_lock);
		imgsensor.current_fps = (UINT16) *feature_data_32;
		spin_unlock(&imgsensor_drv_lock);
		break;
	case SENSOR_FEATURE_SET_HDR:
		pr_debug("ihdr enable :%d\n", *feature_data_32);
		spin_lock(&imgsensor_drv_lock);
		imgsensor.ihdr_mode = (UINT8) *feature_data_32;
		spin_unlock(&imgsensor_drv_lock);
		break;

	case SENSOR_FEATURE_GET_CROP_INFO:
		pr_debug("SENSOR_FEATURE_GET_CROP_INFO scenarioId:%d\n",
			 (UINT32) *feature_data);

		wininfo =
			(struct SENSOR_WINSIZE_INFO_STRUCT *) (uintptr_t)
				(*(feature_data + 1));

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
			memcpy((void *)wininfo, (void *)&imgsensor_winsize_info[5],
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
		pr_debug("SENSOR_SET_SENSOR_IHDR LE=%d, SE=%d, Gain=%d\n",
			 (UINT16) *feature_data,
			 (UINT16) *(feature_data + 1),
			 (UINT16) *(feature_data + 2));

		/* ihdr_write_shutter_gain((UINT16)*feature_data,(UINT16)*(feature_data+1),
		 * (UINT16)*(feature_data+2));
		 */
		break;
	case SENSOR_FEATURE_SET_AWB_GAIN:
		break;
	case SENSOR_FEATURE_SET_HDR_SHUTTER:
		pr_debug("SENSOR_FEATURE_SET_HDR_SHUTTER LE=%d, SE=%d\n",
			 (UINT16) *feature_data,
			 (UINT16) *(feature_data + 1));
		/* ihdr_write_shutter((UINT16)*feature_data,(UINT16)*(feature_data+1)); */
		break;


#ifdef FPT_PDAF_SUPPORT
	/******************** PDAF START >>> *********/
	case SENSOR_FEATURE_GET_PDAF_INFO:
		pr_debug("SENSOR_FEATURE_GET_PDAF_INFO scenarioId:%d\n",
			 (UINT16)*feature_data);
		PDAFinfo =
			(struct SET_PD_BLOCK_INFO_T *)(uintptr_t)(*(feature_data + 1));
		switch (*feature_data) {
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG: /*full*/
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW: /*2x2 binning*/
			memcpy(
				(void *)PDAFinfo,
				(void *)&imgsensor_pd_info,
				sizeof(struct SET_PD_BLOCK_INFO_T));
			break;
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
		default:
			break;
		}
		break;
	case SENSOR_FEATURE_GET_SENSOR_PDAF_CAPACITY:
		pr_debug(
			"SENSOR_FEATURE_GET_SENSOR_PDAF_CAPACITY scenarioId:%d\n",
			(UINT16)*feature_data);
		/*PDAF capacity enable or not*/
		switch (*feature_data) {
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) = 1;
			break;
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			/* video & capture use same setting*/
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) = 1;
			break;
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) = 0;
			break;
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
			/*need to check*/
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) = 0;
			break;
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) = 1;
			break;
		default:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) = 0;
			break;
		}
		break;
	case SENSOR_FEATURE_GET_PDAF_DATA:	/*get cal data from eeprom*/
		pr_debug("SENSOR_FEATURE_GET_PDAF_DATA\n");
		//read_SC1300CS_eeprom((kal_uint16)(*feature_data),(char *)(uintptr_t)(*(feature_data+1)),(kal_uint32)(*(feature_data+2)));
		//pr_debug("SENSOR_FEATURE_GET_PDAF_DATA success\n");
		break;
	case SENSOR_FEATURE_SET_PDAF:
		pr_debug("PDAF mode :%d\n", *feature_data_16);
		imgsensor.pdaf_mode = *feature_data_16;
		break;

		/******************** PDAF END   <<< *********/
#endif

	case SENSOR_FEATURE_SET_SHUTTER_FRAME_TIME:/*lzl*/
		pr_debug("SENSOR_FEATURE_SET_SHUTTER_FRAME_TIME\n");
		set_shutter_frame_length((UINT16)*feature_data,
					 (UINT16) *(feature_data + 1));
		break;
	case SENSOR_FEATURE_SET_STREAMING_SUSPEND:
		pr_debug("SENSOR_FEATURE_SET_STREAMING_SUSPEND\n");
		streaming_control(KAL_FALSE);
		break;
	case SENSOR_FEATURE_SET_STREAMING_RESUME:
		pr_debug("SENSOR_FEATURE_SET_STREAMING_RESUME, shutter:%llu\n",
			 *feature_data);
		if (*feature_data != 0)
			set_shutter(*feature_data);
		streaming_control(KAL_TRUE);
		break;
//	case SENSOR_FEATURE_GET_TEMPERATURE_VALUE:
//		*feature_return_para_i32 = get_sensor_temperature();
//		*feature_para_len = 4;
//		break;

	case SENSOR_FEATURE_GET_PIXEL_RATE:
		switch (*feature_data) {
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
				(imgsensor_info.cap.pclk /
				 (imgsensor_info.cap.linelength)) *
				imgsensor_info.cap.grabwindow_width;

			break;
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
				(imgsensor_info.normal_video.pclk /
				 (imgsensor_info.normal_video.linelength)) *
				imgsensor_info.normal_video.grabwindow_width;

			break;
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
				(imgsensor_info.hs_video.pclk /
				 (imgsensor_info.hs_video.linelength)) *
				imgsensor_info.hs_video.grabwindow_width;

			break;
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
				(imgsensor_info.slim_video.pclk /
				 (imgsensor_info.slim_video.linelength)) *
				imgsensor_info.slim_video.grabwindow_width;

			break;
		case MSDK_SCENARIO_ID_CUSTOM1:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
				(imgsensor_info.custom1.pclk /
				 (imgsensor_info.custom1.linelength)) *
				imgsensor_info.custom1.grabwindow_width;

			break;
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		default:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
				(imgsensor_info.pre.pclk /
				 (imgsensor_info.pre.linelength)) *
				imgsensor_info.pre.grabwindow_width;
			break;
		}
		break;

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
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		default:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
				imgsensor_info.pre.mipi_pixel_rate;
			break;
		}
		break;
	case SENSOR_FEATURE_GET_Y_AVERAGE:
		*feature_return_para_32 = islowlight;
		break;
	default:
		break;
	}

	return ERROR_NONE;
}				/*      feature_control()  */

static struct SENSOR_FUNCTION_STRUCT sensor_func = {
	open,
	get_info,
	get_resolution,
	feature_control,
	control,
	close
};

UINT32 SC1300CS_LY_MIPI_RAW_SensorInit(
	struct SENSOR_FUNCTION_STRUCT **pfFunc)
{
	/* To Do : Check Sensor status here */
	if (pfFunc != NULL)
		*pfFunc = &sensor_func;
	return ERROR_NONE;
}
