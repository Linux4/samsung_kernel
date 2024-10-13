/*****************************************************************************
 *
 * Filename:
 * ---------
 *	 W2SC1300MCSFRONTSTmipi_Sensor.c
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

#include <linux/videodev2.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <asm/atomic.h>
//#include <asm/system.h>
//#include <linux/xlog.h>
#include "kd_camera_typedef.h"
//#include "kd_camera_hw.h"
#include "kd_imgsensor.h"
#include "kd_imgsensor_define.h"
#include "kd_imgsensor_errcode.h"

#include "w2sc1300mcsfrontst_mipiraw.h"
//#include "w2sc1300mcsfrontst_pdaf_cal.h"
#define PFX "W2SC1300MCSFRONTST_camera_sensor"
#define LOG_INF(format, args...)	pr_debug(PFX "[%s] " format, __FUNCTION__, ##args)

static DEFINE_SPINLOCK(imgsensor_drv_lock);

//+bug 767771,liangyiyi.wt, MODIFY, 2022.10.26, modify sc1300mcs setting for solve RF interference and optimize high-temperature BLC stability by fae
static struct imgsensor_info_struct imgsensor_info = {
	.sensor_id = W2SC1300MCSFRONTST_SENSOR_ID,

	.checksum_value =0xffb1ec31,

	.pre = {
		.pclk = 120000000,
		.linelength = 1250,
		.framelength = 3200,
		.startx =0,
		.starty = 0,
		.grabwindow_width = 2104,
		.grabwindow_height = 1560,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 480000000, /* unit , ns */
		/*	 following for GetDefaultFramerateByScenario()	*/
		.max_framerate = 300,
	},
	.cap = {
		.pclk = 120000000,
		.linelength = 1250,
		.framelength = 3200,
		.startx =0,
		.starty = 0,
		.grabwindow_width = 4208,
		.grabwindow_height = 3120,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 473600000, /* unit , ns */
		.max_framerate = 300,
	},
	.normal_video = {
		.pclk = 120000000,
		.linelength = 1250,
		.framelength = 3200,
		.startx =0,
		.starty = 0,
		.grabwindow_width = 4208,
		.grabwindow_height = 2368,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 473600000, /* unit , ns */
		.max_framerate = 300,
	},
	.hs_video = {
		.pclk = 120000000,
		.linelength = 1250,
		.framelength = 1600,
		.startx =0,
		.starty = 0,
		.grabwindow_width = 1920,
		.grabwindow_height = 1080,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 480000000, /* unit , ns */
		.max_framerate = 600,
	},
	.slim_video = {
		.pclk = 120000000,
		.linelength = 1250,
		.framelength = 1600,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 1280,
		.grabwindow_height = 720,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 480000000, /* unit , ns */
		.max_framerate = 600,
	},

		.margin = 5,
		.min_shutter = 3,
		.max_frame_length = 0xffff,
		//+bug783860,liangyiyi.wt,ADD,2022/07/25,modify for fix w2sc1300mcsfrontst photo is black pictures
		.min_gain = 64,
		.max_gain = 2048,
		.min_gain_iso = 100,
		.exp_step = 1,
		.gain_step = 1,
		.gain_type = 4,
		//-bug783860,liangyiyi.wt,ADD,2022/07/25,modify for fix w2sc1300mcsfrontst photo is black pictures
		.ae_shut_delay_frame = 0,
		.ae_sensor_gain_delay_frame = 0,
		.ae_ispGain_delay_frame = 2,
		.ihdr_support = 0,	  /*1, support; 0,not support*/
		.ihdr_le_firstline = 0,  /*1,le first ; 0, se first*/
		.sensor_mode_num = 5,	  /*support sensor mode num*/

		.cap_delay_frame = 3,/*3 guanjd modify for cts*/
		.pre_delay_frame = 3,/*3 guanjd modify for cts*/
		.video_delay_frame = 3,
		.hs_video_delay_frame = 3,
		.slim_video_delay_frame = 3,


		.isp_driving_current = ISP_DRIVING_2MA,//bug 767771,liangyiyi.wt, MODIFY, 2022.10.31, modify the sc1300mcs mclk drive current to 2MA for solve RF interference
		.sensor_interface_type = SENSOR_INTERFACE_TYPE_MIPI,
		.mipi_sensor_type = MIPI_OPHY_NCSI2, /*0,MIPI_OPHY_NCSI2;  1,MIPI_OPHY_CSI2*/
		.mipi_settle_delay_mode = 0, /*0,MIPI_SETTLEDELAY_AUTO; 1,MIPI_SETTLEDELAY_MANNUAL*/
		.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_B,
		.mclk = 24,
		.mipi_lane_num = SENSOR_MIPI_4_LANE,
		.i2c_addr_table = {0x20, 0xff},
		
};


static struct imgsensor_struct imgsensor = {
	.mirror = IMAGE_NORMAL,				//mirrorflip information
	.sensor_mode = IMGSENSOR_MODE_INIT, /*IMGSENSOR_MODE enum value,record current sensor mode,such as: INIT, Preview, Capture, Video,High Speed Video, Slim Video*/
	.shutter = 0x3D0,					/*current shutter*/
	.gain = 0x100,						/*current gain*/
	.dummy_pixel = 0,					/*current dummypixel*/
	.dummy_line = 0,					/*current dummyline*/
	.current_fps = 0,  /*full size current fps : 24fps for PIP, 30fps for Normal or ZSD*/
	.autoflicker_en = KAL_FALSE,  /*auto flicker enable: KAL_FALSE for disable auto flicker, KAL_TRUE for enable auto flicker*/
	.test_pattern = KAL_FALSE,		/*test pattern mode or not. KAL_FALSE for in test pattern mode, KAL_TRUE for normal output*/
	.current_scenario_id = MSDK_SCENARIO_ID_CAMERA_PREVIEW,/*current scenario id*/
	.ihdr_en = 0, /*sensor need support LE, SE with HDR feature*/
	.i2c_write_id = 0x6c,

};


/* Sensor output window information */
static struct SENSOR_WINSIZE_INFO_STRUCT imgsensor_winsize_info[5] =
{
	{ 4208, 3120, 0,   0, 4208, 3120, 2104, 1560,  0,	0,	 2104, 1560, 0,   0,   2104, 1560}, /*Preview*/
	{ 4208, 3120, 0,   0, 4208, 3120, 4208, 3120,  0,	0,	 4208, 3120, 0,   0,   4208, 3120}, /*capture*/
	{ 4208, 3120, 0,   0, 4208, 3120, 4208, 3120,  0,	376, 4208, 2368, 0,   0,   4208, 2368}, /*video*/
	{ 4208, 3120, 184,   480, 3840, 2160, 1920, 1080,  0,	0,	 1920, 1080, 0,   0,   1920, 1080}, /*hight speed video*/
	{ 4208, 3120, 824,   840, 2560, 1440, 1280, 720,  0,	0,	 1280, 720, 0,   0,   1280, 720}, /* slim video*/
}; /*cpy from preview*/


/*no mirror flip*/

#if 0
 static struct SET_PD_BLOCK_INFO_T imgsensor_pd_info = {
	.i4OffsetX = 59,
	.i4OffsetY = 55,
	.i4PitchX  = 32,
	.i4PitchY  = 32,
	.i4PairNum  = 8,
	.i4SubBlkW  = 16,
	.i4SubBlkH  = 8,
	.i4BlockNumX = 128,
	.i4BlockNumY = 94,
	.iMirrorFlip = 0,
//	.i4PosL = {{58, 62},{74, 62},{66, 66},{82, 66},{58, 78},{74, 78},{66, 82},{82, 82}},
//	.i4PosR = {{58, 58},{74, 58},{66, 70},{82, 70},{58, 74},{74, 74},{66, 86},{82, 86}},
	.i4PosL = {{60, 56},{76, 56},{68, 68},{84, 68},{60, 72},{76, 72},{68, 84},{84, 84}},
	.i4PosR = {{60, 60},{76, 60},{68, 64},{84, 64},{60, 76},{76, 76},{68, 80},{84, 80}},
};

#endif


extern int iReadReg(u16 a_u2Addr , u8 * a_puBuff , u16 i2cId);
extern int iWriteReg(u16 a_u2Addr , u32 a_u4Data , u32 a_u4Bytes , u16 i2cId);
extern void kdSetI2CSpeed(u16 i2cSpeed);

/*extern bool read_2l9_eeprom( kal_uint16 addr, BYTE* data, kal_uint32 size);*/




/*static kal_uint16 read_eeprom(kal_uint16 addr)
{
	kal_uint16 get_byte= 0;
	 char pusendcmd[2] = {(char)(addr >> 8) , (char)(addr & 0xFF) };
	iReadRegI2C(pusendcmd , 2, (u8*)&get_byte,1,0xa0);
        return get_byte;

}*/
static kal_uint16 read_cmos_sensor(kal_uint32 addr)
{
    kal_uint16 get_byte = 0;
    char pu_send_cmd[2] = {(char)(addr >> 8), (char)(addr & 0xFF) };

    iReadRegI2C(pu_send_cmd, 2, (u8 *)&get_byte, 1, imgsensor.i2c_write_id);

    return get_byte;
}
#if 0
static void write_cmos_sensor(kal_uint32 addr, kal_uint32 para)
{
    char pu_send_cmd[4] = {(char)(addr >> 8),
        (char)(addr & 0xFF), (char)(para >> 8), (char)(para & 0xFF)};

    iWriteRegI2C(pu_send_cmd, 4, imgsensor.i2c_write_id);
}
#endif

static void write_cmos_sensor8(kal_uint32 addr, kal_uint32 para)
{
    char pu_send_cmd[3] = {(char)(addr >> 8),
        (char)(addr & 0xFF), (char)(para & 0xFF)};

    iWriteRegI2C(pu_send_cmd, 3, imgsensor.i2c_write_id);
}

static void set_dummy(void)
{
	LOG_INF("dummyline = %d, dummypixels = %d \n", imgsensor.dummy_line, imgsensor.dummy_pixel);
    write_cmos_sensor8(0x320c, imgsensor.line_length >> 8);
    write_cmos_sensor8(0x320d, imgsensor.line_length & 0xFF);
    write_cmos_sensor8(0x320e, imgsensor.frame_length >> 8);
    write_cmos_sensor8(0x320f, imgsensor.frame_length & 0xFF);
}	/*	set_dummy  */


static kal_uint32 return_sensor_id()
{
	return ((read_cmos_sensor(0x3107) << 8) | read_cmos_sensor(0x3108)+1); //0xc628
}
static void set_max_framerate(UINT16 framerate,kal_bool min_framelength_en)
{

	kal_uint32 frame_length = imgsensor.frame_length;

	LOG_INF("framerate = %d, min framelength should enable %d \n", framerate,min_framelength_en);

	frame_length = imgsensor.pclk / framerate * 10 / imgsensor.line_length;
	spin_lock(&imgsensor_drv_lock);
	if (frame_length >= imgsensor.min_frame_length)
		imgsensor.frame_length = frame_length;
	else
		imgsensor.frame_length = imgsensor.min_frame_length;
	imgsensor.dummy_line = imgsensor.frame_length - imgsensor.min_frame_length;

	if (imgsensor.frame_length > imgsensor_info.max_frame_length)
	{
		imgsensor.frame_length = imgsensor_info.max_frame_length;
		imgsensor.dummy_line = imgsensor.frame_length - imgsensor.min_frame_length;
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
	if (shutter < imgsensor_info.min_shutter) shutter = imgsensor_info.min_shutter;

	if (imgsensor.autoflicker_en) {
		realtime_fps = imgsensor.pclk / imgsensor.line_length * 10 / imgsensor.frame_length;
		if(realtime_fps >= 297 && realtime_fps <= 305)
			set_max_framerate(296,0);
		else if(realtime_fps >= 147 && realtime_fps <= 150)
			set_max_framerate(146,0);
		else {
		/* Extend frame length*/
	    write_cmos_sensor8(0x326d, (imgsensor.frame_length >> 16) & 0x7f);
		write_cmos_sensor8(0x320e, (imgsensor.frame_length >> 8) & 0xff);
		write_cmos_sensor8(0x320f, imgsensor.frame_length & 0xFF);

	    }
	} else {
		/* Extend frame length*/
		write_cmos_sensor8(0x326d, (imgsensor.frame_length >> 16) & 0x7f);
		write_cmos_sensor8(0x320e, imgsensor.frame_length >> 8);
		write_cmos_sensor8(0x320f, imgsensor.frame_length & 0xFF);
	}
	/* Update Shutter*/
    shutter = shutter *2;
    //shutter = 3;
     //   write_cmos_sensor8(0x3802,0x01);//group hold on
	write_cmos_sensor8(0x3e20, (shutter >> 20) & 0x0F);
	write_cmos_sensor8(0x3e00, (shutter >> 12) & 0xFF);
	write_cmos_sensor8(0x3e01, (shutter >> 4)&0xFF);
	write_cmos_sensor8(0x3e02, (shutter<<4) & 0xF0);


	LOG_INF("gsl_debug shutter =%d, framelength =%d,ExpReg 0x3e20=%x, 0x3e00=%x, 0x3e01=%x, 0x3e02=%x\n",
	shutter,imgsensor.frame_length,read_cmos_sensor(0x3e20),read_cmos_sensor(0x3e00),read_cmos_sensor(0x3e01), read_cmos_sensor(0x3e02));
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

	spin_lock_irqsave(&imgsensor_drv_lock, flags);
	imgsensor.shutter = shutter;
	spin_unlock_irqrestore(&imgsensor_drv_lock, flags);

	write_shutter(shutter);
}	/*	set_shutter */

/*	write_shutter  */
static void set_shutter_frame_length(kal_uint16 shutter, kal_uint16 frame_length)
{
	unsigned long flags;
	kal_uint16 realtime_fps = 0;
	kal_int32 dummy_line = 0;

	spin_lock_irqsave(&imgsensor_drv_lock, flags);
	imgsensor.shutter = shutter;
	spin_unlock_irqrestore(&imgsensor_drv_lock, flags);

	spin_lock(&imgsensor_drv_lock);
	/* Change frame time */
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
	shutter = (shutter > (imgsensor_info.max_frame_length - imgsensor_info.margin))
		? (imgsensor_info.max_frame_length - imgsensor_info.margin) : shutter;

	if (imgsensor.autoflicker_en) {
		realtime_fps = imgsensor.pclk / imgsensor.line_length * 10 / imgsensor.frame_length;
		if (realtime_fps >= 297 && realtime_fps <= 305)
			set_max_framerate(296, 0);
		else if (realtime_fps >= 147 && realtime_fps <= 150)
			set_max_framerate(146, 0);
		else {
			/* Extend frame length */
				    write_cmos_sensor8(0x326d, (imgsensor.frame_length >> 16) & 0x7f);
		write_cmos_sensor8(0x320e, (imgsensor.frame_length >> 8) & 0xff);
		write_cmos_sensor8(0x320f, imgsensor.frame_length & 0xFF);
		}
	} else {
		/* Extend frame length */
			    write_cmos_sensor8(0x326d, (imgsensor.frame_length >> 16) & 0x7f);
		write_cmos_sensor8(0x320e, (imgsensor.frame_length >> 8) & 0xff);
		write_cmos_sensor8(0x320f, imgsensor.frame_length & 0xFF);
	}

	/* Update Shutter */
	    shutter = shutter *2;
    //shutter = 3;
     //   write_cmos_sensor8(0x3802,0x01);//group hold on
	write_cmos_sensor8(0x3e20, (shutter >> 20) & 0x0F);
	write_cmos_sensor8(0x3e00, (shutter >> 12) & 0xFF);
	write_cmos_sensor8(0x3e01, (shutter >> 4)&0xFF);
	write_cmos_sensor8(0x3e02, (shutter<<4) & 0xF0);

	LOG_INF("shutter = %d, framelength = %d/%d, dummy_line= %d\n", shutter, imgsensor.frame_length,
		frame_length, dummy_line);

}				/*      write_shutter  */



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
	kal_uint16 ana_real_gain;
kal_uint16 dreg;
//kal_uint16 iii = 4;
  if (gain < BASEGAIN)
      gain = BASEGAIN;
  else if (gain > 64 * BASEGAIN)
      gain = 63 * BASEGAIN;


	if((gain>=1*BASEGAIN)&&(gain <2*BASEGAIN))
	{
		reg_gain = 0x00;
		ana_real_gain = 1;
	}
	else if((gain>=2*BASEGAIN)&&(gain <4*BASEGAIN))
	{
		reg_gain = 0x01;
		ana_real_gain = 2;
	}
	else if((gain >= 4*BASEGAIN)&&(gain <8*BASEGAIN))
	{
		reg_gain = 0x03;
		ana_real_gain = 4;
	}
	else if((gain >= 8*BASEGAIN)&&(gain <16*BASEGAIN))
	{
		reg_gain = 0x07;
        ana_real_gain = 8;
	}
	else if((gain >= 16*BASEGAIN)&&(gain <32*BASEGAIN))
	{
		reg_gain = 0x0f;
		ana_real_gain = 16;
	}
	else
	{
		reg_gain = 0x1f;
		ana_real_gain = 32;
	}

	//write_cmos_sensor8(0x3e09,reg_gain);
	write_cmos_sensor8(0x3e06,0x00);
        dreg = gain*128/(ana_real_gain*BASEGAIN);
	LOG_INF("gsldebug dreg: 0x%x\n",dreg);
	write_cmos_sensor8(0x3e07,dreg);//(gain/ana_real_gain)*128/BASEGAIN);
	write_cmos_sensor8(0x3e09,reg_gain);

   // spin_lock(&imgsensor_drv_lock);
   // imgsensor.gain = reg_gain;
   // spin_unlock(&imgsensor_drv_lock);

    LOG_INF("gsldebug gain = %d ,again = 0x%x, dgain(0x3e07)= 0x%x end\n", gain, read_cmos_sensor(0x3e09),read_cmos_sensor(0x3e07));
    //LOG_INF("gain = %d ,again = 0x%x\n ", gain, reg_gain);

	return gain;
}	/*	set_gain  */

#if 1

static void set_mirror_flip(kal_uint8 image_mirror)
{
    switch (image_mirror) {
				case IMAGE_NORMAL:
					write_cmos_sensor8(0x3221,0x00);
					break;
				case IMAGE_H_MIRROR:
					write_cmos_sensor8(0x3221,0x06);
					break;
				case IMAGE_V_MIRROR:
					write_cmos_sensor8(0x3221,0x60);
					break;
				case IMAGE_HV_MIRROR:
					write_cmos_sensor8(0x3221,0x66);
					break;
				default:
					LOG_INF("Error image_mirror setting\n");
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
#if 0
static void night_mode(kal_bool enable)
{
/*No Need to implement this function*/
}	/*	night_mode	*/
#endif
//+bug 767771,liangyiyi.wt, MODIFY, 2022.10.5,Modify the setting of sc1300 stream on by fae
static kal_uint32 streaming_control(kal_bool enable)
{
	int timeout = (10000 / imgsensor.current_fps) + 1;
	int i = 0;
	int framecnt = 0;

	LOG_INF("streaming_enable(0= Sw Standby,1= streaming): %d\n", enable);
	if (enable) {
		//+bug783860,liangyiyi.wt,ADD,2022/10/9,modify for fix i2c will report error when the sc1300mcs stream on
		write_cmos_sensor8(0x0100, 0X01);
		mDELAY(10);
		write_cmos_sensor8(0x302d, 0X00);
		//-bug783860,liangyiyi.wt,ADD,2022/10/9,modify for fix i2c will report error when the sc1300mcs stream on
	} else {
		write_cmos_sensor8(0x0100, 0x00);
		for (i = 0; i < timeout; i++) {
			mDELAY(5);
			framecnt = read_cmos_sensor(0x0100);
			if ( framecnt == 0x00) {
				LOG_INF(" Stream Off OK at i=%d.\n", i);
				return ERROR_NONE;
			}
		}
		LOG_INF("Stream Off Fail! framecnt= %d.\n", framecnt);
	}
	return ERROR_NONE;
}
//+bug 767771,liangyiyi.wt, MODIFY, 2022.8.15, update the 2st and 3st front camera setting by fae
static void sensor_init(void)
{
	//Global setting
	pr_info("start \n");
write_cmos_sensor8(0x0103,0x01);
//mDELAY(10);
write_cmos_sensor8(0x0100,0x00);
	pr_info("end remove mDELAY\n");
}	/*	sensor_init  */


static void preview_setting(void)
{
LOG_INF("E!\n");
write_cmos_sensor8(0x0103,0x01);
write_cmos_sensor8(0x0100,0x00);
 //mDELAY(10);
write_cmos_sensor8(0x36e9,0x80);
write_cmos_sensor8(0x36f9,0x80);
write_cmos_sensor8(0x36ea,0x05);
write_cmos_sensor8(0x36eb,0x0a);
write_cmos_sensor8(0x36ec,0x0b);
write_cmos_sensor8(0x36ed,0x24);
write_cmos_sensor8(0x36fa,0x06);
write_cmos_sensor8(0x36fd,0x24);
write_cmos_sensor8(0x36e9,0x04);
write_cmos_sensor8(0x36f9,0x04);
write_cmos_sensor8(0x301f,0x85);
write_cmos_sensor8(0x302d,0x20);
write_cmos_sensor8(0x3106,0x01);
write_cmos_sensor8(0x3208,0x08);
write_cmos_sensor8(0x3209,0x38);
write_cmos_sensor8(0x320a,0x06);
write_cmos_sensor8(0x320b,0x18);
write_cmos_sensor8(0x3211,0x04);
write_cmos_sensor8(0x3213,0x04);
write_cmos_sensor8(0x3215,0x31);
write_cmos_sensor8(0x3220,0x01);
write_cmos_sensor8(0x3250,0x40);
write_cmos_sensor8(0x325f,0x42);
write_cmos_sensor8(0x3301,0x09);
write_cmos_sensor8(0x3306,0x28);
write_cmos_sensor8(0x330b,0x88);
write_cmos_sensor8(0x330e,0x40);
write_cmos_sensor8(0x331e,0x45);
write_cmos_sensor8(0x331f,0x65);
write_cmos_sensor8(0x3333,0x10);
write_cmos_sensor8(0x3347,0x07);
write_cmos_sensor8(0x335d,0x60);
write_cmos_sensor8(0x3364,0x5f);
write_cmos_sensor8(0x3367,0x04);
write_cmos_sensor8(0x3390,0x01);
write_cmos_sensor8(0x3391,0x03);
write_cmos_sensor8(0x3392,0x07);
write_cmos_sensor8(0x3393,0x08);
write_cmos_sensor8(0x3394,0x0e);
write_cmos_sensor8(0x3395,0x1a);
write_cmos_sensor8(0x33ad,0x30);
write_cmos_sensor8(0x33b1,0x80);
write_cmos_sensor8(0x33b3,0x44);
write_cmos_sensor8(0x33f9,0x28);
write_cmos_sensor8(0x33fb,0x2c);
write_cmos_sensor8(0x33fc,0x07);
write_cmos_sensor8(0x33fd,0x1f);
write_cmos_sensor8(0x341e,0x00);
write_cmos_sensor8(0x34a8,0x38);
write_cmos_sensor8(0x34a9,0x20);
write_cmos_sensor8(0x34ab,0x88);
write_cmos_sensor8(0x34ad,0x8c);
write_cmos_sensor8(0x360f,0x01);
write_cmos_sensor8(0x3621,0x68);
write_cmos_sensor8(0x3622,0xe8);
write_cmos_sensor8(0x3635,0x20);
write_cmos_sensor8(0x3637,0x23);
write_cmos_sensor8(0x3638,0xc7);
write_cmos_sensor8(0x3639,0xf4);
write_cmos_sensor8(0x363a,0x82);
write_cmos_sensor8(0x363c,0xc0);
write_cmos_sensor8(0x363d,0x40);
write_cmos_sensor8(0x363e,0x28);
write_cmos_sensor8(0x3650,0x51);  //mipi test
write_cmos_sensor8(0x3651,0x9f);  //mipi test
write_cmos_sensor8(0x3670,0x4a);
write_cmos_sensor8(0x3671,0xe5);
write_cmos_sensor8(0x3672,0x83);
write_cmos_sensor8(0x3673,0x83);
write_cmos_sensor8(0x3674,0x10);
write_cmos_sensor8(0x3675,0x36);
write_cmos_sensor8(0x3676,0x3a);
write_cmos_sensor8(0x367a,0x03);
write_cmos_sensor8(0x367b,0x0f);
write_cmos_sensor8(0x367c,0x03);
write_cmos_sensor8(0x367d,0x07);
write_cmos_sensor8(0x3690,0x55);
write_cmos_sensor8(0x3691,0x65);
write_cmos_sensor8(0x3692,0x85);
write_cmos_sensor8(0x3699,0x82);
write_cmos_sensor8(0x369a,0x9f);
write_cmos_sensor8(0x369b,0xc0);
write_cmos_sensor8(0x369c,0x03);
write_cmos_sensor8(0x369d,0x07);
write_cmos_sensor8(0x36a2,0x03);
write_cmos_sensor8(0x36a3,0x07);
write_cmos_sensor8(0x36b0,0x4c);
write_cmos_sensor8(0x36b1,0xd8);
write_cmos_sensor8(0x36b2,0x01);
write_cmos_sensor8(0x3900,0x1d);
write_cmos_sensor8(0x3901,0x00);
write_cmos_sensor8(0x3902,0xc5);
write_cmos_sensor8(0x3905,0x98);
write_cmos_sensor8(0x391b,0x80);
write_cmos_sensor8(0x391c,0x32);
write_cmos_sensor8(0x391d,0x19);
write_cmos_sensor8(0x3931,0x01);
write_cmos_sensor8(0x3932,0x87);
write_cmos_sensor8(0x3933,0xc0);
write_cmos_sensor8(0x3934,0x07);
write_cmos_sensor8(0x3940,0x74);
write_cmos_sensor8(0x3942,0x00);
write_cmos_sensor8(0x3943,0x08);
write_cmos_sensor8(0x3948,0x06);
write_cmos_sensor8(0x394a,0x08);
write_cmos_sensor8(0x3954,0x86);
write_cmos_sensor8(0x396a,0x07);
write_cmos_sensor8(0x396c,0x0e);
write_cmos_sensor8(0x3e00,0x01);
write_cmos_sensor8(0x3e01,0x8f);
write_cmos_sensor8(0x3e02,0x60);
write_cmos_sensor8(0x3e20,0x00);
write_cmos_sensor8(0x4000,0x00);
write_cmos_sensor8(0x4001,0x05);
write_cmos_sensor8(0x4002,0x00);
write_cmos_sensor8(0x4003,0x00);
write_cmos_sensor8(0x4004,0x00);
write_cmos_sensor8(0x4005,0x00);
write_cmos_sensor8(0x4006,0x08);
write_cmos_sensor8(0x4007,0x31);
write_cmos_sensor8(0x4008,0x00);
write_cmos_sensor8(0x4009,0xc8);
write_cmos_sensor8(0x4402,0x03);
write_cmos_sensor8(0x4403,0x0c);
write_cmos_sensor8(0x4404,0x24);
write_cmos_sensor8(0x4405,0x2f);
write_cmos_sensor8(0x440c,0x3c);
write_cmos_sensor8(0x440d,0x3c);
write_cmos_sensor8(0x440e,0x2d);
write_cmos_sensor8(0x440f,0x4b);
write_cmos_sensor8(0x441b,0x18);
write_cmos_sensor8(0x4424,0x01);
write_cmos_sensor8(0x4506,0x3c);
write_cmos_sensor8(0x4509,0x30);
write_cmos_sensor8(0x5000,0x4e);
write_cmos_sensor8(0x5780,0x66);
write_cmos_sensor8(0x578d,0x40);
write_cmos_sensor8(0x5799,0x06);
write_cmos_sensor8(0x5901,0x04);
write_cmos_sensor8(0x59e0,0xd8);
write_cmos_sensor8(0x59e1,0x08);
write_cmos_sensor8(0x59e2,0x10);
write_cmos_sensor8(0x59e3,0x08);
write_cmos_sensor8(0x59e4,0x00);
write_cmos_sensor8(0x59e5,0x10);
write_cmos_sensor8(0x59e6,0x08);
write_cmos_sensor8(0x59e7,0x00);
write_cmos_sensor8(0x59e8,0x18);
write_cmos_sensor8(0x59e9,0x0c);
write_cmos_sensor8(0x59ea,0x04);
write_cmos_sensor8(0x59eb,0x18);
write_cmos_sensor8(0x59ec,0x0c);
write_cmos_sensor8(0x59ed,0x04);
	LOG_INF("end \n");
}	/*	preview_setting  */

// Pll Setting - VCO = 280Mhz
static void capture_setting(kal_uint16 currefps)
{
	LOG_INF("E! currefps:%d\n",currefps);
write_cmos_sensor8(0x0103,0x01);
write_cmos_sensor8(0x0100,0x00);
 //mDELAY(10);
write_cmos_sensor8(0x36e9,0x80);
write_cmos_sensor8(0x36f9,0x80);
write_cmos_sensor8(0x36ea,0x25);
write_cmos_sensor8(0x36eb,0x0a);
write_cmos_sensor8(0x36ec,0x4b);
write_cmos_sensor8(0x36ed,0x14);
write_cmos_sensor8(0x36fa,0x06);
write_cmos_sensor8(0x36fd,0x24);
write_cmos_sensor8(0x36e9,0x40);
write_cmos_sensor8(0x36f9,0x04);
write_cmos_sensor8(0x301f,0x92);
write_cmos_sensor8(0x302d,0x00);
write_cmos_sensor8(0x3106,0x01);
write_cmos_sensor8(0x3250,0x40);
write_cmos_sensor8(0x325f,0x42);
write_cmos_sensor8(0x3301,0x09);
write_cmos_sensor8(0x3306,0x28);
write_cmos_sensor8(0x330b,0x88);
write_cmos_sensor8(0x330e,0x40);
write_cmos_sensor8(0x331e,0x45);
write_cmos_sensor8(0x331f,0x65);
write_cmos_sensor8(0x3333,0x10);
write_cmos_sensor8(0x3347,0x07);
write_cmos_sensor8(0x335d,0x60);
write_cmos_sensor8(0x3364,0x5f);
write_cmos_sensor8(0x3367,0x04);
write_cmos_sensor8(0x3390,0x01);
write_cmos_sensor8(0x3391,0x03);
write_cmos_sensor8(0x3392,0x07);
write_cmos_sensor8(0x3393,0x08);
write_cmos_sensor8(0x3394,0x0e);
write_cmos_sensor8(0x3395,0x1a);
write_cmos_sensor8(0x33ad,0x30);
write_cmos_sensor8(0x33b1,0x80);
write_cmos_sensor8(0x33b3,0x44);
write_cmos_sensor8(0x33f9,0x28);
write_cmos_sensor8(0x33fb,0x2c);
write_cmos_sensor8(0x33fc,0x07);
write_cmos_sensor8(0x33fd,0x1f);
write_cmos_sensor8(0x341e,0x00);
write_cmos_sensor8(0x34a8,0x38);
write_cmos_sensor8(0x34a9,0x20);
write_cmos_sensor8(0x34ab,0x88);
write_cmos_sensor8(0x34ad,0x8c);
write_cmos_sensor8(0x360f,0x01);
write_cmos_sensor8(0x3621,0x68);
write_cmos_sensor8(0x3622,0xe8);
write_cmos_sensor8(0x3635,0x20);
write_cmos_sensor8(0x3637,0x23);
write_cmos_sensor8(0x3638,0xc7);
write_cmos_sensor8(0x3639,0xf4);
write_cmos_sensor8(0x363a,0x82);
write_cmos_sensor8(0x363c,0xc0);
write_cmos_sensor8(0x363d,0x40);
write_cmos_sensor8(0x363e,0x28);
write_cmos_sensor8(0x3650,0x51);  //mipi test
write_cmos_sensor8(0x3651,0x9f);  //mipi test
write_cmos_sensor8(0x3670,0x4a);
write_cmos_sensor8(0x3671,0xe5);
write_cmos_sensor8(0x3672,0x83);
write_cmos_sensor8(0x3673,0x83);
write_cmos_sensor8(0x3674,0x10);
write_cmos_sensor8(0x3675,0x36);
write_cmos_sensor8(0x3676,0x3a);
write_cmos_sensor8(0x367a,0x03);
write_cmos_sensor8(0x367b,0x0f);
write_cmos_sensor8(0x367c,0x03);
write_cmos_sensor8(0x367d,0x07);
write_cmos_sensor8(0x3690,0x55);
write_cmos_sensor8(0x3691,0x65);
write_cmos_sensor8(0x3692,0x85);
write_cmos_sensor8(0x3699,0x82);
write_cmos_sensor8(0x369a,0x9f);
write_cmos_sensor8(0x369b,0xc0);
write_cmos_sensor8(0x369c,0x03);
write_cmos_sensor8(0x369d,0x07);
write_cmos_sensor8(0x36a2,0x03);
write_cmos_sensor8(0x36a3,0x07);
write_cmos_sensor8(0x36b0,0x4c);
write_cmos_sensor8(0x36b1,0xd8);
write_cmos_sensor8(0x36b2,0x01);
write_cmos_sensor8(0x3900,0x1d);
write_cmos_sensor8(0x3901,0x00);
write_cmos_sensor8(0x3902,0xc5);
write_cmos_sensor8(0x3905,0x98);
write_cmos_sensor8(0x391b,0x80);
write_cmos_sensor8(0x391c,0x32);
write_cmos_sensor8(0x391d,0x19);
write_cmos_sensor8(0x3931,0x01);
write_cmos_sensor8(0x3932,0x87);
write_cmos_sensor8(0x3933,0xc0);
write_cmos_sensor8(0x3934,0x07);
write_cmos_sensor8(0x3940,0x74);
write_cmos_sensor8(0x3942,0x00);
write_cmos_sensor8(0x3943,0x08);
write_cmos_sensor8(0x3948,0x06);
write_cmos_sensor8(0x394a,0x08);
write_cmos_sensor8(0x3954,0x86);
write_cmos_sensor8(0x396a,0x07);
write_cmos_sensor8(0x396c,0x0e);
write_cmos_sensor8(0x3e00,0x01);
write_cmos_sensor8(0x3e01,0x8f);
write_cmos_sensor8(0x3e02,0x60);
write_cmos_sensor8(0x3e20,0x00);
write_cmos_sensor8(0x4000,0x20);
write_cmos_sensor8(0x4402,0x03);
write_cmos_sensor8(0x4403,0x0c);
write_cmos_sensor8(0x4404,0x24);
write_cmos_sensor8(0x4405,0x2f);
write_cmos_sensor8(0x440c,0x3c);
write_cmos_sensor8(0x440d,0x3c);
write_cmos_sensor8(0x440e,0x2d);
write_cmos_sensor8(0x440f,0x4b);
write_cmos_sensor8(0x441b,0x18);
write_cmos_sensor8(0x4424,0x01);
write_cmos_sensor8(0x4506,0x3c);
write_cmos_sensor8(0x4509,0x30);
write_cmos_sensor8(0x4837,0x1b);
write_cmos_sensor8(0x5000,0x0e);
write_cmos_sensor8(0x5780,0x66);
write_cmos_sensor8(0x578d,0x40);
write_cmos_sensor8(0x5799,0x06);
write_cmos_sensor8(0x59e0,0xd8);
write_cmos_sensor8(0x59e1,0x08);
write_cmos_sensor8(0x59e2,0x10);
write_cmos_sensor8(0x59e3,0x08);
write_cmos_sensor8(0x59e4,0x00);
write_cmos_sensor8(0x59e5,0x10);
write_cmos_sensor8(0x59e6,0x08);
write_cmos_sensor8(0x59e7,0x00);
write_cmos_sensor8(0x59e8,0x18);
write_cmos_sensor8(0x59e9,0x0c);
write_cmos_sensor8(0x59ea,0x04);
write_cmos_sensor8(0x59eb,0x18);
write_cmos_sensor8(0x59ec,0x0c);
write_cmos_sensor8(0x59ed,0x04);
LOG_INF("end \n");
}

static void normal_video_setting(kal_uint16 currefps)
{
	LOG_INF("E! currefps:%d\n",currefps);
write_cmos_sensor8(0x0103,0x01);
write_cmos_sensor8(0x0100,0x00);
 //mDELAY(10);
write_cmos_sensor8(0x36e9,0x80);
write_cmos_sensor8(0x36f9,0x80);
write_cmos_sensor8(0x36ea,0x25);
write_cmos_sensor8(0x36eb,0x0a);
write_cmos_sensor8(0x36ec,0x4b);
write_cmos_sensor8(0x36ed,0x14);
write_cmos_sensor8(0x36fa,0x06);
write_cmos_sensor8(0x36fd,0x24);
write_cmos_sensor8(0x36e9,0x40);
write_cmos_sensor8(0x36f9,0x04);
write_cmos_sensor8(0x301f,0x92);
write_cmos_sensor8(0x302d,0x00);
write_cmos_sensor8(0x3106,0x01);
write_cmos_sensor8(0x3200,0x00);
write_cmos_sensor8(0x3201,0x00);
write_cmos_sensor8(0x3202,0x01);
write_cmos_sensor8(0x3203,0x78);
write_cmos_sensor8(0x3204,0x10);
write_cmos_sensor8(0x3205,0x7f);
write_cmos_sensor8(0x3206,0x0a);
write_cmos_sensor8(0x3207,0xc7);
write_cmos_sensor8(0x3208,0x10);
write_cmos_sensor8(0x3209,0x70);
write_cmos_sensor8(0x320a,0x09);
write_cmos_sensor8(0x320b,0x40);
write_cmos_sensor8(0x3210,0x00);
write_cmos_sensor8(0x3211,0x08);
write_cmos_sensor8(0x3212,0x00);
write_cmos_sensor8(0x3213,0x08);
write_cmos_sensor8(0x3250,0x40);
write_cmos_sensor8(0x325f,0x42);
write_cmos_sensor8(0x3301,0x09);
write_cmos_sensor8(0x3306,0x28);
write_cmos_sensor8(0x330b,0x88);
write_cmos_sensor8(0x330e,0x40);
write_cmos_sensor8(0x331e,0x45);
write_cmos_sensor8(0x331f,0x65);
write_cmos_sensor8(0x3333,0x10);
write_cmos_sensor8(0x3347,0x07);
write_cmos_sensor8(0x335d,0x60);
write_cmos_sensor8(0x3364,0x5f);
write_cmos_sensor8(0x3367,0x04);
write_cmos_sensor8(0x3390,0x01);
write_cmos_sensor8(0x3391,0x03);
write_cmos_sensor8(0x3392,0x07);
write_cmos_sensor8(0x3393,0x08);
write_cmos_sensor8(0x3394,0x0e);
write_cmos_sensor8(0x3395,0x1a);
write_cmos_sensor8(0x33ad,0x30);
write_cmos_sensor8(0x33b1,0x80);
write_cmos_sensor8(0x33b3,0x44);
write_cmos_sensor8(0x33f9,0x28);
write_cmos_sensor8(0x33fb,0x2c);
write_cmos_sensor8(0x33fc,0x07);
write_cmos_sensor8(0x33fd,0x1f);
write_cmos_sensor8(0x341e,0x00);
write_cmos_sensor8(0x34a8,0x38);
write_cmos_sensor8(0x34a9,0x20);
write_cmos_sensor8(0x34ab,0x88);
write_cmos_sensor8(0x34ad,0x8c);
write_cmos_sensor8(0x360f,0x01);
write_cmos_sensor8(0x3621,0x68);
write_cmos_sensor8(0x3622,0xe8);
write_cmos_sensor8(0x3635,0x20);
write_cmos_sensor8(0x3637,0x23);
write_cmos_sensor8(0x3638,0xc7);
write_cmos_sensor8(0x3639,0xf4);
write_cmos_sensor8(0x363a,0x82);
write_cmos_sensor8(0x363c,0xc0);
write_cmos_sensor8(0x363d,0x40);
write_cmos_sensor8(0x363e,0x28);
write_cmos_sensor8(0x3650,0x51);  //mipi test
write_cmos_sensor8(0x3651,0x9f);  //mipi test
write_cmos_sensor8(0x3670,0x4a);
write_cmos_sensor8(0x3671,0xe5);
write_cmos_sensor8(0x3672,0x83);
write_cmos_sensor8(0x3673,0x83);
write_cmos_sensor8(0x3674,0x10);
write_cmos_sensor8(0x3675,0x36);
write_cmos_sensor8(0x3676,0x3a);
write_cmos_sensor8(0x367a,0x03);
write_cmos_sensor8(0x367b,0x0f);
write_cmos_sensor8(0x367c,0x03);
write_cmos_sensor8(0x367d,0x07);
write_cmos_sensor8(0x3690,0x55);
write_cmos_sensor8(0x3691,0x65);
write_cmos_sensor8(0x3692,0x85);
write_cmos_sensor8(0x3699,0x82);
write_cmos_sensor8(0x369a,0x9f);
write_cmos_sensor8(0x369b,0xc0);
write_cmos_sensor8(0x369c,0x03);
write_cmos_sensor8(0x369d,0x07);
write_cmos_sensor8(0x36a2,0x03);
write_cmos_sensor8(0x36a3,0x07);
write_cmos_sensor8(0x36b0,0x4c);
write_cmos_sensor8(0x36b1,0xd8);
write_cmos_sensor8(0x36b2,0x01);
write_cmos_sensor8(0x3900,0x1d);
write_cmos_sensor8(0x3901,0x00);
write_cmos_sensor8(0x3902,0xc5);
write_cmos_sensor8(0x3905,0x98);
write_cmos_sensor8(0x391b,0x80);
write_cmos_sensor8(0x391c,0x32);
write_cmos_sensor8(0x391d,0x19);
write_cmos_sensor8(0x3931,0x01);
write_cmos_sensor8(0x3932,0x87);
write_cmos_sensor8(0x3933,0xc0);
write_cmos_sensor8(0x3934,0x07);
write_cmos_sensor8(0x3940,0x74);
write_cmos_sensor8(0x3942,0x00);
write_cmos_sensor8(0x3943,0x08);
write_cmos_sensor8(0x3948,0x06);
write_cmos_sensor8(0x394a,0x08);
write_cmos_sensor8(0x3954,0x86);
write_cmos_sensor8(0x396a,0x07);
write_cmos_sensor8(0x396c,0x0e);
write_cmos_sensor8(0x3e00,0x01);
write_cmos_sensor8(0x3e01,0x8f);
write_cmos_sensor8(0x3e02,0x60);
write_cmos_sensor8(0x3e20,0x00);
write_cmos_sensor8(0x4000,0x20);
write_cmos_sensor8(0x4402,0x03);
write_cmos_sensor8(0x4403,0x0c);
write_cmos_sensor8(0x4404,0x24);
write_cmos_sensor8(0x4405,0x2f);
write_cmos_sensor8(0x440c,0x3c);
write_cmos_sensor8(0x440d,0x3c);
write_cmos_sensor8(0x440e,0x2d);
write_cmos_sensor8(0x440f,0x4b);
write_cmos_sensor8(0x441b,0x18);
write_cmos_sensor8(0x4424,0x01);
write_cmos_sensor8(0x4506,0x3c);
write_cmos_sensor8(0x4509,0x30);
write_cmos_sensor8(0x4837,0x1b);
write_cmos_sensor8(0x5000,0x0e);
write_cmos_sensor8(0x5780,0x66);
write_cmos_sensor8(0x578d,0x40);
write_cmos_sensor8(0x5799,0x06);
write_cmos_sensor8(0x59e0,0xd8);
write_cmos_sensor8(0x59e1,0x08);
write_cmos_sensor8(0x59e2,0x10);
write_cmos_sensor8(0x59e3,0x08);
write_cmos_sensor8(0x59e4,0x00);
write_cmos_sensor8(0x59e5,0x10);
write_cmos_sensor8(0x59e6,0x08);
write_cmos_sensor8(0x59e7,0x00);
write_cmos_sensor8(0x59e8,0x18);
write_cmos_sensor8(0x59e9,0x0c);
write_cmos_sensor8(0x59ea,0x04);
write_cmos_sensor8(0x59eb,0x18);
write_cmos_sensor8(0x59ec,0x0c);
write_cmos_sensor8(0x59ed,0x04);
LOG_INF("end \n");
}
static void hs_video_setting(void)
{
	LOG_INF("start \n");
write_cmos_sensor8(0x0103,0x01);
write_cmos_sensor8(0x0100,0x00);
 //mDELAY(10);
write_cmos_sensor8(0x36e9,0x80);
write_cmos_sensor8(0x36f9,0x80);
write_cmos_sensor8(0x36ea,0x05);
write_cmos_sensor8(0x36eb,0x0a);
write_cmos_sensor8(0x36ec,0x0b);
write_cmos_sensor8(0x36ed,0x24);
write_cmos_sensor8(0x36fa,0x06);
write_cmos_sensor8(0x36fd,0x24);
write_cmos_sensor8(0x36e9,0x04);
write_cmos_sensor8(0x36f9,0x04);
write_cmos_sensor8(0x301f,0x86);
write_cmos_sensor8(0x302d,0x20);
write_cmos_sensor8(0x3106,0x01);
write_cmos_sensor8(0x3200,0x00);
write_cmos_sensor8(0x3201,0xb8);
write_cmos_sensor8(0x3202,0x01);
write_cmos_sensor8(0x3203,0xe0);
write_cmos_sensor8(0x3204,0x0f);
write_cmos_sensor8(0x3205,0xc7);
write_cmos_sensor8(0x3206,0x0a);
write_cmos_sensor8(0x3207,0x5f);
write_cmos_sensor8(0x3208,0x07);
write_cmos_sensor8(0x3209,0x80);
write_cmos_sensor8(0x320a,0x04);
write_cmos_sensor8(0x320b,0x38);
write_cmos_sensor8(0x320e,0x06);
write_cmos_sensor8(0x320f,0x40);
write_cmos_sensor8(0x3210,0x00);
write_cmos_sensor8(0x3211,0x04);
write_cmos_sensor8(0x3212,0x00);
write_cmos_sensor8(0x3213,0x04);
write_cmos_sensor8(0x3215,0x31);
write_cmos_sensor8(0x3220,0x01);
write_cmos_sensor8(0x3250,0x40);
write_cmos_sensor8(0x325f,0x42);
write_cmos_sensor8(0x3301,0x09);
write_cmos_sensor8(0x3306,0x28);
write_cmos_sensor8(0x330b,0x88);
write_cmos_sensor8(0x330e,0x40);
write_cmos_sensor8(0x331e,0x45);
write_cmos_sensor8(0x331f,0x65);
write_cmos_sensor8(0x3333,0x10);
write_cmos_sensor8(0x3347,0x07);
write_cmos_sensor8(0x335d,0x60);
write_cmos_sensor8(0x3364,0x5f);
write_cmos_sensor8(0x3367,0x04);
write_cmos_sensor8(0x3390,0x01);
write_cmos_sensor8(0x3391,0x03);
write_cmos_sensor8(0x3392,0x07);
write_cmos_sensor8(0x3393,0x08);
write_cmos_sensor8(0x3394,0x0e);
write_cmos_sensor8(0x3395,0x1a);
write_cmos_sensor8(0x33ad,0x30);
write_cmos_sensor8(0x33b1,0x80);
write_cmos_sensor8(0x33b3,0x44);
write_cmos_sensor8(0x33f9,0x28);
write_cmos_sensor8(0x33fb,0x2c);
write_cmos_sensor8(0x33fc,0x07);
write_cmos_sensor8(0x33fd,0x1f);
write_cmos_sensor8(0x341e,0x00);
write_cmos_sensor8(0x34a8,0x38);
write_cmos_sensor8(0x34a9,0x20);
write_cmos_sensor8(0x34ab,0x88);
write_cmos_sensor8(0x34ad,0x8c);
write_cmos_sensor8(0x360f,0x01);
write_cmos_sensor8(0x3621,0x68);
write_cmos_sensor8(0x3622,0xe8);
write_cmos_sensor8(0x3635,0x20);
write_cmos_sensor8(0x3637,0x23);
write_cmos_sensor8(0x3638,0xc7);
write_cmos_sensor8(0x3639,0xf4);
write_cmos_sensor8(0x363a,0x82);
write_cmos_sensor8(0x363c,0xc0);
write_cmos_sensor8(0x363d,0x40);
write_cmos_sensor8(0x363e,0x28);
write_cmos_sensor8(0x3650,0x51);  //mipi test
write_cmos_sensor8(0x3651,0x9f);  //mipi test
write_cmos_sensor8(0x3670,0x4a);
write_cmos_sensor8(0x3671,0xe5);
write_cmos_sensor8(0x3672,0x83);
write_cmos_sensor8(0x3673,0x83);
write_cmos_sensor8(0x3674,0x10);
write_cmos_sensor8(0x3675,0x36);
write_cmos_sensor8(0x3676,0x3a);
write_cmos_sensor8(0x367a,0x03);
write_cmos_sensor8(0x367b,0x0f);
write_cmos_sensor8(0x367c,0x03);
write_cmos_sensor8(0x367d,0x07);
write_cmos_sensor8(0x3690,0x55);
write_cmos_sensor8(0x3691,0x65);
write_cmos_sensor8(0x3692,0x85);
write_cmos_sensor8(0x3699,0x82);
write_cmos_sensor8(0x369a,0x9f);
write_cmos_sensor8(0x369b,0xc0);
write_cmos_sensor8(0x369c,0x03);
write_cmos_sensor8(0x369d,0x07);
write_cmos_sensor8(0x36a2,0x03);
write_cmos_sensor8(0x36a3,0x07);
write_cmos_sensor8(0x36b0,0x4c);
write_cmos_sensor8(0x36b1,0xd8);
write_cmos_sensor8(0x36b2,0x01);
write_cmos_sensor8(0x3900,0x1d);
write_cmos_sensor8(0x3901,0x00);
write_cmos_sensor8(0x3902,0xc5);
write_cmos_sensor8(0x3905,0x98);
write_cmos_sensor8(0x391b,0x80);
write_cmos_sensor8(0x391c,0x32);
write_cmos_sensor8(0x391d,0x19);
write_cmos_sensor8(0x3931,0x01);
write_cmos_sensor8(0x3932,0x87);
write_cmos_sensor8(0x3933,0xc0);
write_cmos_sensor8(0x3934,0x07);
write_cmos_sensor8(0x3940,0x74);
write_cmos_sensor8(0x3942,0x00);
write_cmos_sensor8(0x3943,0x08);
write_cmos_sensor8(0x3948,0x06);
write_cmos_sensor8(0x394a,0x08);
write_cmos_sensor8(0x3954,0x86);
write_cmos_sensor8(0x396a,0x07);
write_cmos_sensor8(0x396c,0x0e);
write_cmos_sensor8(0x3e00,0x00);
write_cmos_sensor8(0x3e01,0xc7);
write_cmos_sensor8(0x3e02,0x60);
write_cmos_sensor8(0x3e20,0x00);
write_cmos_sensor8(0x4000,0x00);
write_cmos_sensor8(0x4001,0x05);
write_cmos_sensor8(0x4002,0x00);
write_cmos_sensor8(0x4003,0x00);
write_cmos_sensor8(0x4004,0x00);
write_cmos_sensor8(0x4005,0x00);
write_cmos_sensor8(0x4006,0x08);
write_cmos_sensor8(0x4007,0x31);
write_cmos_sensor8(0x4008,0x00);
write_cmos_sensor8(0x4009,0xc8);
write_cmos_sensor8(0x4402,0x03);
write_cmos_sensor8(0x4403,0x0c);
write_cmos_sensor8(0x4404,0x24);
write_cmos_sensor8(0x4405,0x2f);
write_cmos_sensor8(0x440c,0x3c);
write_cmos_sensor8(0x440d,0x3c);
write_cmos_sensor8(0x440e,0x2d);
write_cmos_sensor8(0x440f,0x4b);
write_cmos_sensor8(0x441b,0x18);
write_cmos_sensor8(0x4424,0x01);
write_cmos_sensor8(0x4506,0x3c);
write_cmos_sensor8(0x4509,0x30);
write_cmos_sensor8(0x5000,0x4e);
write_cmos_sensor8(0x5780,0x66);
write_cmos_sensor8(0x578d,0x40);
write_cmos_sensor8(0x5799,0x06);
write_cmos_sensor8(0x5901,0x04);
write_cmos_sensor8(0x59e0,0xd8);
write_cmos_sensor8(0x59e1,0x08);
write_cmos_sensor8(0x59e2,0x10);
write_cmos_sensor8(0x59e3,0x08);
write_cmos_sensor8(0x59e4,0x00);
write_cmos_sensor8(0x59e5,0x10);
write_cmos_sensor8(0x59e6,0x08);
write_cmos_sensor8(0x59e7,0x00);
write_cmos_sensor8(0x59e8,0x18);
write_cmos_sensor8(0x59e9,0x0c);
write_cmos_sensor8(0x59ea,0x04);
write_cmos_sensor8(0x59eb,0x18);
write_cmos_sensor8(0x59ec,0x0c);
write_cmos_sensor8(0x59ed,0x04);
	LOG_INF("end \n");
}

static void slim_video_setting(void)
{
	LOG_INF("E\n");
/*	preview_setting();*/
write_cmos_sensor8(0x0103,0x01);
write_cmos_sensor8(0x0100,0x00);
 //mDELAY(10);
write_cmos_sensor8(0x36e9,0x80);
write_cmos_sensor8(0x36f9,0x80);
write_cmos_sensor8(0x36ea,0x05);
write_cmos_sensor8(0x36eb,0x0a);
write_cmos_sensor8(0x36ec,0x0b);
write_cmos_sensor8(0x36ed,0x24);
write_cmos_sensor8(0x36fa,0x06);
write_cmos_sensor8(0x36fd,0x24);
write_cmos_sensor8(0x36e9,0x04);
write_cmos_sensor8(0x36f9,0x04);
write_cmos_sensor8(0x301f,0x87);
write_cmos_sensor8(0x302d,0x20);
write_cmos_sensor8(0x3106,0x01);
write_cmos_sensor8(0x3200,0x03);
write_cmos_sensor8(0x3201,0x38);
write_cmos_sensor8(0x3202,0x03);
write_cmos_sensor8(0x3203,0x48);
write_cmos_sensor8(0x3204,0x0d);
write_cmos_sensor8(0x3205,0x47);
write_cmos_sensor8(0x3206,0x08);
write_cmos_sensor8(0x3207,0xf7);
write_cmos_sensor8(0x3208,0x05);
write_cmos_sensor8(0x3209,0x00);
write_cmos_sensor8(0x320a,0x02);
write_cmos_sensor8(0x320b,0xd0);
write_cmos_sensor8(0x320e,0x06);
write_cmos_sensor8(0x320f,0x40);
write_cmos_sensor8(0x3210,0x00);
write_cmos_sensor8(0x3211,0x04);
write_cmos_sensor8(0x3212,0x00);
write_cmos_sensor8(0x3213,0x04);
write_cmos_sensor8(0x3215,0x31);
write_cmos_sensor8(0x3220,0x01);
write_cmos_sensor8(0x3250,0x40);
write_cmos_sensor8(0x325f,0x42);
write_cmos_sensor8(0x3301,0x09);
write_cmos_sensor8(0x3306,0x28);
write_cmos_sensor8(0x330b,0x88);
write_cmos_sensor8(0x330e,0x40);
write_cmos_sensor8(0x331e,0x45);
write_cmos_sensor8(0x331f,0x65);
write_cmos_sensor8(0x3333,0x10);
write_cmos_sensor8(0x3347,0x07);
write_cmos_sensor8(0x335d,0x60);
write_cmos_sensor8(0x3364,0x5f);
write_cmos_sensor8(0x3367,0x04);
write_cmos_sensor8(0x3390,0x01);
write_cmos_sensor8(0x3391,0x03);
write_cmos_sensor8(0x3392,0x07);
write_cmos_sensor8(0x3393,0x08);
write_cmos_sensor8(0x3394,0x0e);
write_cmos_sensor8(0x3395,0x1a);
write_cmos_sensor8(0x33ad,0x30);
write_cmos_sensor8(0x33b1,0x80);
write_cmos_sensor8(0x33b3,0x44);
write_cmos_sensor8(0x33f9,0x28);
write_cmos_sensor8(0x33fb,0x2c);
write_cmos_sensor8(0x33fc,0x07);
write_cmos_sensor8(0x33fd,0x1f);
write_cmos_sensor8(0x341e,0x00);
write_cmos_sensor8(0x34a8,0x38);
write_cmos_sensor8(0x34a9,0x20);
write_cmos_sensor8(0x34ab,0x88);
write_cmos_sensor8(0x34ad,0x8c);
write_cmos_sensor8(0x360f,0x01);
write_cmos_sensor8(0x3621,0x68);
write_cmos_sensor8(0x3622,0xe8);
write_cmos_sensor8(0x3635,0x20);
write_cmos_sensor8(0x3637,0x23);
write_cmos_sensor8(0x3638,0xc7);
write_cmos_sensor8(0x3639,0xf4);
write_cmos_sensor8(0x363a,0x82);
write_cmos_sensor8(0x363c,0xc0);
write_cmos_sensor8(0x363d,0x40);
write_cmos_sensor8(0x363e,0x28);
write_cmos_sensor8(0x3650,0x51);  //mipi test
write_cmos_sensor8(0x3651,0x9f);  //mipi test
write_cmos_sensor8(0x3670,0x4a);
write_cmos_sensor8(0x3671,0xe5);
write_cmos_sensor8(0x3672,0x83);
write_cmos_sensor8(0x3673,0x83);
write_cmos_sensor8(0x3674,0x10);
write_cmos_sensor8(0x3675,0x36);
write_cmos_sensor8(0x3676,0x3a);
write_cmos_sensor8(0x367a,0x03);
write_cmos_sensor8(0x367b,0x0f);
write_cmos_sensor8(0x367c,0x03);
write_cmos_sensor8(0x367d,0x07);
write_cmos_sensor8(0x3690,0x55);
write_cmos_sensor8(0x3691,0x65);
write_cmos_sensor8(0x3692,0x85);
write_cmos_sensor8(0x3699,0x82);
write_cmos_sensor8(0x369a,0x9f);
write_cmos_sensor8(0x369b,0xc0);
write_cmos_sensor8(0x369c,0x03);
write_cmos_sensor8(0x369d,0x07);
write_cmos_sensor8(0x36a2,0x03);
write_cmos_sensor8(0x36a3,0x07);
write_cmos_sensor8(0x36b0,0x4c);
write_cmos_sensor8(0x36b1,0xd8);
write_cmos_sensor8(0x36b2,0x01);
write_cmos_sensor8(0x3900,0x1d);
write_cmos_sensor8(0x3901,0x00);
write_cmos_sensor8(0x3902,0xc5);
write_cmos_sensor8(0x3905,0x98);
write_cmos_sensor8(0x391b,0x80);
write_cmos_sensor8(0x391c,0x32);
write_cmos_sensor8(0x391d,0x19);
write_cmos_sensor8(0x3931,0x01);
write_cmos_sensor8(0x3932,0x87);
write_cmos_sensor8(0x3933,0xc0);
write_cmos_sensor8(0x3934,0x07);
write_cmos_sensor8(0x3940,0x74);
write_cmos_sensor8(0x3942,0x00);
write_cmos_sensor8(0x3943,0x08);
write_cmos_sensor8(0x3948,0x06);
write_cmos_sensor8(0x394a,0x08);
write_cmos_sensor8(0x3954,0x86);
write_cmos_sensor8(0x396a,0x07);
write_cmos_sensor8(0x396c,0x0e);
write_cmos_sensor8(0x3e00,0x00);
write_cmos_sensor8(0x3e01,0xc7);
write_cmos_sensor8(0x3e02,0x60);
write_cmos_sensor8(0x3e20,0x00);
write_cmos_sensor8(0x4000,0x00);
write_cmos_sensor8(0x4001,0x05);
write_cmos_sensor8(0x4002,0x00);
write_cmos_sensor8(0x4003,0x00);
write_cmos_sensor8(0x4004,0x00);
write_cmos_sensor8(0x4005,0x00);
write_cmos_sensor8(0x4006,0x08);
write_cmos_sensor8(0x4007,0x31);
write_cmos_sensor8(0x4008,0x00);
write_cmos_sensor8(0x4009,0xc8);
write_cmos_sensor8(0x4402,0x03);
write_cmos_sensor8(0x4403,0x0c);
write_cmos_sensor8(0x4404,0x24);
write_cmos_sensor8(0x4405,0x2f);
write_cmos_sensor8(0x440c,0x3c);
write_cmos_sensor8(0x440d,0x3c);
write_cmos_sensor8(0x440e,0x2d);
write_cmos_sensor8(0x440f,0x4b);
write_cmos_sensor8(0x441b,0x18);
write_cmos_sensor8(0x4424,0x01);
write_cmos_sensor8(0x4506,0x3c);
write_cmos_sensor8(0x4509,0x30);
write_cmos_sensor8(0x5000,0x4e);
write_cmos_sensor8(0x5780,0x66);
write_cmos_sensor8(0x578d,0x40);
write_cmos_sensor8(0x5799,0x06);
write_cmos_sensor8(0x5901,0x04);
write_cmos_sensor8(0x59e0,0xd8);
write_cmos_sensor8(0x59e1,0x08);
write_cmos_sensor8(0x59e2,0x10);
write_cmos_sensor8(0x59e3,0x08);
write_cmos_sensor8(0x59e4,0x00);
write_cmos_sensor8(0x59e5,0x10);
write_cmos_sensor8(0x59e6,0x08);
write_cmos_sensor8(0x59e7,0x00);
write_cmos_sensor8(0x59e8,0x18);
write_cmos_sensor8(0x59e9,0x0c);
write_cmos_sensor8(0x59ea,0x04);
write_cmos_sensor8(0x59eb,0x18);
write_cmos_sensor8(0x59ec,0x0c);
write_cmos_sensor8(0x59ed,0x04);
LOG_INF("end \n");
}
//+songshiyang.wt, ADD, 2022/02/25, add main camera w2sc1300mcsfrontst eeprom bring up
//-bug 767771,liangyiyi.wt, MODIFY, 2022.8.15, update the 2st and 3st front camera setting by fae
//-bug 767771,liangyiyi.wt, MODIFY, 2022.10.5,Modify the setting of sc1300 stream on by fae
//-bug 767771,liangyiyi.wt, MODIFY, 2022.10.26, modify sc1300mcs setting for solve RF interference and optimize high-temperature BLC stability by fae
#define OTP_PORTING 1

#if OTP_PORTING
#define EEPROM_W2SC1300MCSFRONTST_ID 0xA0

#include "cam_cal_define.h"
#include <linux/slab.h>


static struct stCAM_CAL_DATAINFO_STRUCT w2sc1300mcsfrontst_eeprom_data ={
	.sensorID= W2SC1300MCSFRONTST_SENSOR_ID,
	.deviceID = 0x02,
	.dataLength = 0x0778,
	.sensorVendorid = 0x17050100,
	.vendorByte = {1,2,3,4},
	.dataBuffer = NULL,
};

static struct stCAM_CAL_CHECKSUM_STRUCT w2sc1300mcsfrontst_Checksum[5] =
{
//  item         flagAdrees; startAdress;   endAdress;  checksumAdress;  validFlag;
	{MODULE_ITEM,0x0000,     0x0000,        0x0007,     0x0008,        0x55},
	{AWB_ITEM,   0x0017,     0x0017,        0x0027,     0x0028,        0x55},
	//{AF_ITEM,    0x0029,     0x0029,        0x002E,     0x002F,        0x55},
	{LSC_ITEM,   0x0029,     0x0029,        0x0775,     0x0776,        0x55},
	//{PDAF_ITEM,  0x077E,     0x077E,        0x096E,     0x096F,        0x55},
	//{PDAF_ITEM,  0x0970,     0x0970,        0x0D5C,     0x0D5D,        0x55},
	{TOTAL_ITEM, 0x0000,     0x0000,        0x0776,     0x0777,        0x55},
	{MAX_ITEM,   0xFFFF,     0xFFFF,        0xFFFF,     0xFFFF,        0x55},  // this line must haved
};

extern int imgSensorReadEepromData(struct stCAM_CAL_DATAINFO_STRUCT* pData,
	struct stCAM_CAL_CHECKSUM_STRUCT* checkData);
extern int imgSensorSetEepromData(struct stCAM_CAL_DATAINFO_STRUCT* pData);
static kal_uint16 w2sc1300mcsfrontst_read_eeprom(kal_uint32 addr)
{
     kal_uint16 get_byte=0;

     char pu_send_cmd[2] = { (char)(addr >> 8), (char)(addr & 0xFF) };
     iReadRegI2C(pu_send_cmd, 2, (u8*)&get_byte, 1, EEPROM_W2SC1300MCSFRONTST_ID);

     return get_byte;
}

#endif
//-songshiyang.wt, ADD, 2022/02/25, add main camera w2sc1300mcsfrontst eeprom bring up







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
	//int I2C_BUS = -1 ;
	//I2C_BUS = i2c_adapter_id(pgi2c_cfg_legacy->pinst->pi2c_client->adapter);
	//LOG_INF(" I2C_BUS = %d\n",I2C_BUS);
	//if(I2C_BUS != 2){
	//	*sensor_id = 0xFFFFFFFF;
	//	return ERROR_SENSOR_CONNECT_FAIL;
	//}
	/*sensor have two i2c address 0x6c 0x6d & 0x21 0x20, we should detect the module used i2c address*/
//+songshiyang.wt, ADD, 2022/02/25, add main camera w2sc1300mcsfrontst eeprom bring up
#if OTP_PORTING
	kal_int32 size = 0;
#endif
//-songshiyang.wt, ADD, 2022/02/25, add main camera w2sc1300mcsfrontst eeprom bring up
	while (imgsensor_info.i2c_addr_table[i] != 0xff) {
		spin_lock(&imgsensor_drv_lock);
		imgsensor.i2c_write_id = imgsensor_info.i2c_addr_table[i];
		spin_unlock(&imgsensor_drv_lock);
		do {
			*sensor_id = return_sensor_id();
			if (*sensor_id == imgsensor_info.sensor_id) {
//+songshiyang.wt, ADD, 2022/02/25, add main camera w2sc1300mcsfrontst eeprom bring up
#if OTP_PORTING
                LOG_INF("w2sc1300mcsfrontst i2c write id: 0x%x, sensor id: 0x%x module id: 0x%x\n",imgsensor.i2c_write_id, *sensor_id, w2sc1300mcsfrontst_read_eeprom(0x0001));
				size = imgSensorReadEepromData(&w2sc1300mcsfrontst_eeprom_data,w2sc1300mcsfrontst_Checksum);
				if(size != w2sc1300mcsfrontst_eeprom_data.dataLength || 
                  (w2sc1300mcsfrontst_eeprom_data.sensorVendorid >> 24) != w2sc1300mcsfrontst_read_eeprom(0x0001)) {
					if(w2sc1300mcsfrontst_eeprom_data.dataBuffer != NULL) {
						kfree(w2sc1300mcsfrontst_eeprom_data.dataBuffer);
						w2sc1300mcsfrontst_eeprom_data.dataBuffer = NULL;
					}
					printk("w2sc1300mcsfrontst get eeprom data failed %d\n",size);
					*sensor_id = 0xFFFFFFFF;
					return ERROR_SENSOR_CONNECT_FAIL;
				} else {
					LOG_INF("w2sc1300mcsfrontst get eeprom data success\n");
					imgSensorSetEepromData(&w2sc1300mcsfrontst_eeprom_data);
				}
#endif
//-songshiyang.wt, ADD, 2022/02/25, add main camera w2sc1300mcsfrontst eeprom bring up
				pr_err("w2sc1300mcsfrontst i2c write id : 0x%x, sensor id: 0x%x\n",
				imgsensor.i2c_write_id, *sensor_id);
				return ERROR_NONE;
			}
			retry--;
		} while (retry > 0);
		i++;
		retry = 2;
	}
	if (*sensor_id != imgsensor_info.sensor_id) {
		pr_err("w2sc1300mcsfrontst Read id fail,sensor id: 0x%x\n", *sensor_id);
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
	//LOG_INF("PLATFORM:MT6750,MIPI 4LANE\n");

	/*sensor have two i2c address 0x6c 0x6d & 0x21 0x20, we should detect the module used i2c address*/
	while (imgsensor_info.i2c_addr_table[i] != 0xff) {
		spin_lock(&imgsensor_drv_lock);
		imgsensor.i2c_write_id = imgsensor_info.i2c_addr_table[i];
		spin_unlock(&imgsensor_drv_lock);
		do {

			sensor_id =return_sensor_id();
			if (sensor_id == imgsensor_info.sensor_id) {
				LOG_INF("i2c write id: 0x%x, sensor id: 0x%x\n", imgsensor.i2c_write_id,sensor_id);
				break;
			}
			LOG_INF("Read sensor id fail, id: 0x%x\n", imgsensor.i2c_write_id);
			retry--;
		} while(retry > 0);
		i++;
		if (sensor_id == imgsensor_info.sensor_id)
			break;
		retry = 2;
	}
	if (imgsensor_info.sensor_id != sensor_id)
		return ERROR_SENSOR_CONNECT_FAIL;

	/* initial sequence write in  */
	sensor_init();

	spin_lock(&imgsensor_drv_lock);

	imgsensor.autoflicker_en= KAL_FALSE;
	imgsensor.sensor_mode = IMGSENSOR_MODE_INIT;
	imgsensor.shutter = 0x3D0;
	imgsensor.gain = 0x100;
	imgsensor.pclk = imgsensor_info.pre.pclk;
	imgsensor.frame_length = imgsensor_info.pre.framelength;
	imgsensor.line_length = imgsensor_info.pre.linelength;
	imgsensor.min_frame_length = imgsensor_info.pre.framelength;
	imgsensor.dummy_pixel = 0;
	imgsensor.dummy_line = 0;
	//imgsensor.pdaf_mode = 4;
	imgsensor.ihdr_en = 0;
	imgsensor.test_pattern = KAL_FALSE;
	imgsensor.current_fps = imgsensor_info.pre.max_framerate;
	spin_unlock(&imgsensor_drv_lock);

	return ERROR_NONE;
}	/*	open  */



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
	LOG_INF("E\n");

	/*No Need to implement this function*/

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
	LOG_INF("E\n");

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
	LOG_INF("E\n");
	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_CAPTURE;
	if (imgsensor.current_fps == imgsensor_info.cap.max_framerate) {/*PIP capture: 24fps for less than 13M, 20fps for 16M,15fps for 20M*/
		imgsensor.pclk = imgsensor_info.cap.pclk;
		imgsensor.line_length = imgsensor_info.cap.linelength;
		imgsensor.frame_length = imgsensor_info.cap.framelength;
		imgsensor.min_frame_length = imgsensor_info.cap.framelength;
		imgsensor.autoflicker_en = KAL_FALSE;
	}  else if(imgsensor.current_fps == imgsensor_info.cap.max_framerate){
		imgsensor.pclk = imgsensor_info.cap.pclk;
		imgsensor.line_length = imgsensor_info.cap.linelength;
		imgsensor.frame_length = imgsensor_info.cap.framelength;
		imgsensor.min_frame_length = imgsensor_info.cap.framelength;
		imgsensor.autoflicker_en = KAL_FALSE;
	}else {
		if (imgsensor.current_fps != imgsensor_info.cap.max_framerate)
			LOG_INF("Warning: current_fps %d fps is not support, so use cap's setting: %d fps!\n",imgsensor.current_fps,imgsensor_info.cap.max_framerate/10);
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
}	/* capture() */
static kal_uint32 normal_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
					  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("E\n");

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_VIDEO;
	imgsensor.pclk = imgsensor_info.normal_video.pclk;
	imgsensor.line_length = imgsensor_info.normal_video.linelength;
	imgsensor.frame_length = imgsensor_info.normal_video.framelength;
	imgsensor.min_frame_length = imgsensor_info.normal_video.framelength;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);

	normal_video_setting(imgsensor.current_fps);
	set_mirror_flip(imgsensor.mirror);

	return ERROR_NONE;
}	/*	normal_video   */

static kal_uint32 hs_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
					  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("E\n");

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_HIGH_SPEED_VIDEO;
	imgsensor.pclk = imgsensor_info.hs_video.pclk;
	/*imgsensor.video_mode = KAL_TRUE;*/
	imgsensor.line_length = imgsensor_info.hs_video.linelength;
	imgsensor.frame_length = imgsensor_info.hs_video.framelength;
	imgsensor.min_frame_length = imgsensor_info.hs_video.framelength;
	imgsensor.dummy_line = 0;
	imgsensor.dummy_pixel = 0;
	/*imgsensor.current_fps = 300;*/
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	hs_video_setting();
	set_mirror_flip(imgsensor.mirror);

	return ERROR_NONE;
}	/*	hs_video   */

static kal_uint32 slim_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
					  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("E\n");

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_SLIM_VIDEO;
	imgsensor.pclk = imgsensor_info.slim_video.pclk;
	/*imgsensor.video_mode = KAL_TRUE;*/
	imgsensor.line_length = imgsensor_info.slim_video.linelength;
	imgsensor.frame_length = imgsensor_info.slim_video.framelength;
	imgsensor.min_frame_length = imgsensor_info.slim_video.framelength;
	imgsensor.dummy_line = 0;
	imgsensor.dummy_pixel = 0;
	/*imgsensor.current_fps = 300;*/
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	slim_video_setting();
	set_mirror_flip(imgsensor.mirror);

	return ERROR_NONE;
}	/*	slim_video	 */



static kal_uint32 get_resolution(MSDK_SENSOR_RESOLUTION_INFO_STRUCT *sensor_resolution)
{
	LOG_INF("E\n");
	sensor_resolution->SensorFullWidth = imgsensor_info.cap.grabwindow_width;
	sensor_resolution->SensorFullHeight = imgsensor_info.cap.grabwindow_height;

	sensor_resolution->SensorPreviewWidth = imgsensor_info.pre.grabwindow_width;
	sensor_resolution->SensorPreviewHeight = imgsensor_info.pre.grabwindow_height;

	sensor_resolution->SensorVideoWidth = imgsensor_info.normal_video.grabwindow_width;
	sensor_resolution->SensorVideoHeight = imgsensor_info.normal_video.grabwindow_height;


	sensor_resolution->SensorHighSpeedVideoWidth	 = imgsensor_info.hs_video.grabwindow_width;
	sensor_resolution->SensorHighSpeedVideoHeight	 = imgsensor_info.hs_video.grabwindow_height;

	sensor_resolution->SensorSlimVideoWidth	 = imgsensor_info.slim_video.grabwindow_width;
	sensor_resolution->SensorSlimVideoHeight	 = imgsensor_info.slim_video.grabwindow_height;

	return ERROR_NONE;
}	/*	get_resolution	*/

static kal_uint32 get_info(enum MSDK_SCENARIO_ID_ENUM scenario_id,
					  MSDK_SENSOR_INFO_STRUCT *sensor_info,
					  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("scenario_id = %d\n", scenario_id);

	sensor_info->SensorClockPolarity = SENSOR_CLOCK_POLARITY_LOW;
	sensor_info->SensorClockFallingPolarity = SENSOR_CLOCK_POLARITY_LOW; /* not use */
	sensor_info->SensorHsyncPolarity = SENSOR_CLOCK_POLARITY_LOW; /* inverse with datasheet*/
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


	sensor_info->SensorMasterClockSwitch = 0; /* not use */
	sensor_info->SensorDrivingCurrent = imgsensor_info.isp_driving_current;

	sensor_info->AEShutDelayFrame = imgsensor_info.ae_shut_delay_frame; 		 /* The frame of setting shutter default 0 for TG int */
	sensor_info->AESensorGainDelayFrame = imgsensor_info.ae_sensor_gain_delay_frame;	/* The frame of setting sensor gain */
	sensor_info->AEISPGainDelayFrame = imgsensor_info.ae_ispGain_delay_frame;
	sensor_info->IHDR_Support = imgsensor_info.ihdr_support;
	sensor_info->IHDR_LE_FirstLine = imgsensor_info.ihdr_le_firstline;
	sensor_info->SensorModeNum = imgsensor_info.sensor_mode_num;
	//sensor_info->PDAF_Support = 6;
	sensor_info->SensorMIPILaneNumber = imgsensor_info.mipi_lane_num;
	sensor_info->SensorClockFreq = imgsensor_info.mclk;
	sensor_info->SensorClockDividCount = 3; /* not use */
	sensor_info->SensorClockRisingCount = 0;
	sensor_info->SensorClockFallingCount = 2; /* not use */
	sensor_info->SensorPixelClockCount = 3; /* not use */
	sensor_info->SensorDataLatchCount = 2; /* not use */

	sensor_info->MIPIDataLowPwr2HighSpeedTermDelayCount = 0;
	sensor_info->MIPICLKLowPwr2HighSpeedTermDelayCount = 0;
	sensor_info->SensorWidthSampling = 0;  /* 0 is default 1x*/
	sensor_info->SensorHightSampling = 0;	/* 0 is default 1x*/
	sensor_info->SensorPacketECCOrder = 1;
	switch (scenario_id) {
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
			sensor_info->SensorGrabStartX = imgsensor_info.pre.startx;
			sensor_info->SensorGrabStartY = imgsensor_info.pre.starty;

			sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount = imgsensor_info.pre.mipi_data_lp2hs_settle_dc;

			break;
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			sensor_info->SensorGrabStartX = imgsensor_info.cap.startx;
			sensor_info->SensorGrabStartY = imgsensor_info.cap.starty;

			sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount = imgsensor_info.cap.mipi_data_lp2hs_settle_dc;

			break;
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:

			sensor_info->SensorGrabStartX = imgsensor_info.normal_video.startx;
			sensor_info->SensorGrabStartY = imgsensor_info.normal_video.starty;

			sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount = imgsensor_info.normal_video.mipi_data_lp2hs_settle_dc;

			break;
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
			sensor_info->SensorGrabStartX = imgsensor_info.hs_video.startx;
			sensor_info->SensorGrabStartY = imgsensor_info.hs_video.starty;

			sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount = imgsensor_info.hs_video.mipi_data_lp2hs_settle_dc;

			break;
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
			sensor_info->SensorGrabStartX = imgsensor_info.slim_video.startx;
			sensor_info->SensorGrabStartY = imgsensor_info.slim_video.starty;

			sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount = imgsensor_info.slim_video.mipi_data_lp2hs_settle_dc;

			break;

		default:
			sensor_info->SensorGrabStartX = imgsensor_info.pre.startx;
			sensor_info->SensorGrabStartY = imgsensor_info.pre.starty;

			sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount = imgsensor_info.pre.mipi_data_lp2hs_settle_dc;
			break;
	}

	return ERROR_NONE;
}	/*	get_info  */


static kal_uint32 control(enum MSDK_SCENARIO_ID_ENUM scenario_id, MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
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

		default:
			LOG_INF("Error ScenarioId setting");
			preview(image_window, sensor_config_data);
			return ERROR_INVALID_SCENARIO_ID;
	}
	return ERROR_NONE;
}	/* control() */



static kal_uint32 set_video_mode(UINT16 framerate)
{
	LOG_INF("framerate = %d\n ", framerate);
	/* SetVideoMode Function should fix framerate*/
	if (framerate == 0)
		/* Dynamic frame rate*/
		return ERROR_NONE;
	spin_lock(&imgsensor_drv_lock);
	if ((framerate == 300) && (imgsensor.autoflicker_en == KAL_TRUE))
		imgsensor.current_fps = 296;
	else if ((framerate == 150) && (imgsensor.autoflicker_en == KAL_TRUE))
		imgsensor.current_fps = 146;
	else
		imgsensor.current_fps = framerate;
	spin_unlock(&imgsensor_drv_lock);
	set_max_framerate(imgsensor.current_fps,1);

	return ERROR_NONE;
}

static kal_uint32 set_auto_flicker_mode(kal_bool enable, UINT16 framerate)
{
	LOG_INF("enable = %d, framerate = %d \n", enable, framerate);
	spin_lock(&imgsensor_drv_lock);
	if (enable) /*enable auto flicker*/
		imgsensor.autoflicker_en = KAL_TRUE;
	else /*Cancel Auto flick*/
		imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	return ERROR_NONE;
}


static kal_uint32 set_max_framerate_by_scenario(enum MSDK_SCENARIO_ID_ENUM scenario_id, MUINT32 framerate)
{
	kal_uint32 frame_length;

	LOG_INF("scenario_id = %d, framerate = %d\n", scenario_id, framerate);

	switch (scenario_id) {
	case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		frame_length = imgsensor_info.pre.pclk / framerate * 10 / imgsensor_info.pre.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line = (frame_length > imgsensor_info.pre.framelength) ? (frame_length - imgsensor_info.pre.framelength) : 0;
		imgsensor.frame_length = imgsensor_info.pre.framelength + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		/*set_dummy();*/
		break;
	case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
		if(framerate == 0)
			return ERROR_NONE;
		frame_length = imgsensor_info.normal_video.pclk / framerate * 10 / imgsensor_info.normal_video.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line = (frame_length > imgsensor_info.normal_video.framelength) ? (frame_length - imgsensor_info.normal_video.framelength) : 0;
		imgsensor.frame_length = imgsensor_info.normal_video.framelength + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		/*set_dummy();*/
		break;
	case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		  if (imgsensor.current_fps == imgsensor_info.cap.max_framerate) {
	        frame_length = imgsensor_info.cap.pclk / framerate * 10 / imgsensor_info.cap.linelength;
	        spin_lock(&imgsensor_drv_lock);
	            imgsensor.dummy_line = (frame_length > imgsensor_info.cap.framelength) ? (frame_length - imgsensor_info.cap.framelength) : 0;
	            imgsensor.frame_length = imgsensor_info.cap.framelength + imgsensor.dummy_line;
	            imgsensor.min_frame_length = imgsensor.frame_length;
	            spin_unlock(&imgsensor_drv_lock);
	    } else if (imgsensor.current_fps == imgsensor_info.cap.max_framerate) {
	        frame_length = imgsensor_info.cap.pclk / framerate * 10 / imgsensor_info.cap.linelength;
	        spin_lock(&imgsensor_drv_lock);
	            imgsensor.dummy_line = (frame_length > imgsensor_info.cap.framelength) ? (frame_length - imgsensor_info.cap.framelength) : 0;
	            imgsensor.frame_length = imgsensor_info.cap.framelength + imgsensor.dummy_line;
	            imgsensor.min_frame_length = imgsensor.frame_length;
	            spin_unlock(&imgsensor_drv_lock);
	    } else {
			    if (imgsensor.current_fps != imgsensor_info.cap.max_framerate)
	            LOG_INF("Warning: current_fps %d fps is not support, so use cap's setting: %d fps!\n",framerate,imgsensor_info.cap.max_framerate/10);
	        frame_length = imgsensor_info.cap.pclk / framerate * 10 / imgsensor_info.cap.linelength;
	        spin_lock(&imgsensor_drv_lock);
	            imgsensor.dummy_line = (frame_length > imgsensor_info.cap.framelength) ? (frame_length - imgsensor_info.cap.framelength) : 0;
	            imgsensor.frame_length = imgsensor_info.cap.framelength + imgsensor.dummy_line;
	            imgsensor.min_frame_length = imgsensor.frame_length;
	            spin_unlock(&imgsensor_drv_lock);
	    }
	    /*set_dummy();*/
	    break;
	case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
		frame_length = imgsensor_info.hs_video.pclk / framerate * 10 / imgsensor_info.hs_video.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line = (frame_length > imgsensor_info.hs_video.framelength) ? (frame_length - imgsensor_info.hs_video.framelength) : 0;
		imgsensor.frame_length = imgsensor_info.hs_video.framelength + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		/*set_dummy();*/
		break;
	case MSDK_SCENARIO_ID_SLIM_VIDEO:
		frame_length = imgsensor_info.slim_video.pclk / framerate * 10 / imgsensor_info.slim_video.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line = (frame_length > imgsensor_info.slim_video.framelength) ? (frame_length - imgsensor_info.slim_video.framelength): 0;
		imgsensor.frame_length = imgsensor_info.slim_video.framelength + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		/*set_dummy();*/
		break;

	default:  /*coding with  preview scenario by default*/
		frame_length = imgsensor_info.pre.pclk / framerate * 10 / imgsensor_info.pre.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line = (frame_length > imgsensor_info.pre.framelength) ? (frame_length - imgsensor_info.pre.framelength) : 0;
		imgsensor.frame_length = imgsensor_info.pre.framelength + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		/*set_dummy();*/
		LOG_INF("error scenario_id = %d, we use preview scenario \n", scenario_id);
		break;
	}
	return ERROR_NONE;
}


static kal_uint32 get_default_framerate_by_scenario(enum MSDK_SCENARIO_ID_ENUM scenario_id, MUINT32 *framerate)
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

	default:
		break;
	}

	return ERROR_NONE;
}

static kal_uint32 set_test_pattern_mode(kal_bool enable)
{
	LOG_INF("enable: %d\n", enable);
#if 0
	if (enable) {
		/* 0x5E00[8]: 1 enable,  0 disable*/
		/* 0x5E00[1:0]; 00 Color bar, 01 Random Data, 10 Square, 11 BLACK*/
		write_cmos_sensor8(0x0600, 0x0002);
	} else {
		/* 0x5E00[8]: 1 enable,  0 disable*/
		/* 0x5E00[1:0]; 00 Color bar, 01 Random Data, 10 Square, 11 BLACK*/
		write_cmos_sensor8(0x0600,0x0000);
	}
	spin_lock(&imgsensor_drv_lock);
	imgsensor.test_pattern = enable;
	spin_unlock(&imgsensor_drv_lock);
#endif
	return ERROR_NONE;
}
static kal_uint32 feature_control(MSDK_SENSOR_FEATURE_ENUM feature_id,
							 UINT8 *feature_para,UINT32 *feature_para_len)
{
	UINT16 *feature_return_para_16= (UINT16 *) feature_para;
	UINT16 *feature_data_16= (UINT16 *) feature_para;
	UINT32 *feature_return_para_32= (UINT32 *) feature_para;
	UINT32 *feature_data_32= (UINT32 *) feature_para;
	unsigned long long *feature_data= (unsigned long long *) feature_para;

	//struct SET_PD_BLOCK_INFO_T *PDAFinfo;
	struct SENSOR_WINSIZE_INFO_STRUCT *wininfo;


	MSDK_SENSOR_REG_INFO_STRUCT *sensor_reg_data= (MSDK_SENSOR_REG_INFO_STRUCT *) feature_para;

	LOG_INF("feature_id = %d\n", feature_id);
	switch (feature_id) {
	case SENSOR_FEATURE_GET_PERIOD:
		*feature_return_para_16++ = imgsensor.line_length;
		*feature_return_para_16 = imgsensor.frame_length;
		*feature_para_len= 4;
		break;
	case SENSOR_FEATURE_GET_PIXEL_CLOCK_FREQ:
		*feature_return_para_32 = imgsensor.pclk;
		*feature_para_len= 4;
		break;
	//+bug783860,liangyiyi.wt,ADD,2022/07/25,modify for fix w2sc1300mcsfrontst photo is black pictures
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
		break;
	case SENSOR_FEATURE_GET_BINNING_TYPE:
		switch (*(feature_data + 1)) {
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			*feature_return_para_32 = 1; /*BINNING_NONE*/
			break;
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			if (*(feature_data + 2))/* HDR on */
				*feature_return_para_32 = 1;/*BINNING_NONE*/
			else
				*feature_return_para_32 = 1;/*BINNING_AVERAGED*/
			break;
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
		default:
			*feature_return_para_32 = 1; /*BINNING_AVERAGED*/
			break;
		}
		pr_debug("SENSOR_FEATURE_GET_BINNING_TYPE AE_binning_type:%d\n",
			*feature_return_para_32);

		*feature_para_len = 4;
		break;
	case SENSOR_FEATURE_GET_FRAME_CTRL_INFO_BY_SCENARIO:
		/*
		* 1, if driver support new sw frame sync
		* set_shutter_frame_length() support third para auto_extend_en
		*/
		*(feature_data + 1) = 1;
		/* margin info by scenario */
		*(feature_data + 2) = imgsensor_info.margin;
		break;
	//-bug783860,liangyiyi.wt,ADD,2022/07/25,modify for fix w2sc1300mcsfrontst photo is black pictures
	case SENSOR_FEATURE_SET_ESHUTTER:
	             set_shutter(*feature_data);
		break;
	case SENSOR_FEATURE_SET_NIGHTMODE:
	             /*night_mode((BOOL) *feature_data);*/
		break;
	case SENSOR_FEATURE_SET_GAIN:
	             set_gain((UINT16) *feature_data);
		break;
	case SENSOR_FEATURE_SET_FLASHLIGHT:
		break;
	case SENSOR_FEATURE_SET_ISP_MASTER_CLOCK_FREQ:
		break;
	case SENSOR_FEATURE_SET_REGISTER:
		write_cmos_sensor8(sensor_reg_data->RegAddr, sensor_reg_data->RegData);
		break;
	case SENSOR_FEATURE_GET_REGISTER:
		sensor_reg_data->RegData = read_cmos_sensor(sensor_reg_data->RegAddr);
		break;
	case SENSOR_FEATURE_GET_LENS_DRIVER_ID:
		/* get the lens driver ID from EEPROM or just return LENS_DRIVER_ID_DO_NOT_CARE*/
		/* if EEPROM does not exist in camera module.*/
		*feature_return_para_32= LENS_DRIVER_ID_DO_NOT_CARE;
		*feature_para_len= 4;
		break;
	case SENSOR_FEATURE_SET_VIDEO_MODE:
	            set_video_mode(*feature_data);
		break;
	case SENSOR_FEATURE_CHECK_SENSOR_ID:
		get_imgsensor_id(feature_return_para_32);
		break;
	case SENSOR_FEATURE_SET_AUTO_FLICKER_MODE:
		set_auto_flicker_mode((BOOL)*feature_data_16,*(feature_data_16+1));
		break;
	case SENSOR_FEATURE_SET_MAX_FRAME_RATE_BY_SCENARIO:
	             set_max_framerate_by_scenario((enum MSDK_SCENARIO_ID_ENUM)*feature_data, *(feature_data+1));
		break;
	case SENSOR_FEATURE_GET_DEFAULT_FRAME_RATE_BY_SCENARIO:
	             get_default_framerate_by_scenario((enum MSDK_SCENARIO_ID_ENUM)*(feature_data), (MUINT32 *)(uintptr_t)(*(feature_data+1)));
		break;
#if 0
	case SENSOR_FEATURE_GET_PDAF_DATA:
		LOG_INF("SENSOR_FEATURE_GET_PDAF_DATA\n");
		w2sc1300mcsfrontst_read_eeprom((kal_uint16 )(*feature_data),(char*)(uintptr_t)(*(feature_data+1)),(kal_uint32)(*(feature_data+2)));
		break;
#endif
	case SENSOR_FEATURE_SET_TEST_PATTERN:
	               set_test_pattern_mode((BOOL)*feature_data);
		break;
	case SENSOR_FEATURE_GET_TEST_PATTERN_CHECKSUM_VALUE: /*for factory mode auto testing*/
		*feature_return_para_32 = imgsensor_info.checksum_value;
		*feature_para_len= 4;
		break;
	//+bug727089, liangyiyi.wt,MODIFY, 2022.10.27, fix KASAN opened to Monitor memory, camera  memory access error.
	case SENSOR_FEATURE_SET_FRAMERATE:
		spin_lock(&imgsensor_drv_lock);
	             imgsensor.current_fps = (UINT16)*feature_data_32;
		spin_unlock(&imgsensor_drv_lock);
		break;
	//-bug727089, liangyiyi.wt,MODIFY, 2022.10.27, fix KASAN opened to Monitor memory, camera  memory access error.
	case SENSOR_FEATURE_GET_CROP_INFO:
	    LOG_INF("SENSOR_FEATURE_GET_CROP_INFO scenarioId:%d\n", *feature_data_32);
	    wininfo = (struct SENSOR_WINSIZE_INFO_STRUCT *)(uintptr_t)(*(feature_data+1));

		switch (*feature_data_32) {
			case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
				memcpy((void *)wininfo,(void *)&imgsensor_winsize_info[1],sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
				break;
			case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
				memcpy((void *)wininfo,(void *)&imgsensor_winsize_info[2],sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
				break;
			case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
				memcpy((void *)wininfo,(void *)&imgsensor_winsize_info[3],sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
				break;
			case MSDK_SCENARIO_ID_SLIM_VIDEO:
				memcpy((void *)wininfo,(void *)&imgsensor_winsize_info[4],sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
				break;

			case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
			default:
				memcpy((void *)wininfo,(void *)&imgsensor_winsize_info[0],sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
				break;
		}
		break;
#if 0
	case SENSOR_FEATURE_GET_PDAF_INFO:
		LOG_INF("SENSOR_FEATURE_GET_PDAF_INFO scenarioId:%lld\n", *feature_data);
		PDAFinfo= (struct SET_PD_BLOCK_INFO_T *)(uintptr_t)(*(feature_data+1));

		switch (*feature_data) {
			case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
			case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
				//memcpy((void *)PDAFinfo,(void *)&imgsensor_pd_info,sizeof(struct SET_PD_BLOCK_INFO_T));
				break;
			case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
			case MSDK_SCENARIO_ID_SLIM_VIDEO:
			default:
				break;
		}
		break;
	case SENSOR_FEATURE_SET_PDAF:
		LOG_INF("PDAF mode :%d\n", imgsensor.pdaf_mode);//*feature_data);
		imgsensor.pdaf_mode = *feature_data_16;
		break;
	case SENSOR_FEATURE_GET_SENSOR_PDAF_CAPACITY:
		LOG_INF("SENSOR_FEATURE_GET_SENSOR_PDAF_CAPACITY scenarioId:%lld\n", *feature_data);
		/*PDAF capacity enable or not, 2p8 only full size support PDAF*/
		switch (*feature_data) {
			case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
				*(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 1;
				break;
			case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
				*(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 0; /* video & capture use same setting*/
				break;
			case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
				*(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 0;
				break;
			case MSDK_SCENARIO_ID_SLIM_VIDEO:
				*(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 0;
				break;
			case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
				*(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 1;
				break;
			default:
				*(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 0;
				break;
		}
		break;
#endif
#if 0
        case SENSOR_FEATURE_GET_CUSTOM_INFO:
		    printk("SENSOR_FEATURE_GET_CUSTOM_INFO information type:%lld  S5k2l8SX_OTP_ERROR_CODE:%d \n", *feature_data,S5k2l8SX_OTP_ERROR_CODE);
			switch (*feature_data) {
				case 0:    //info type: otp state
				    *(MUINT32 *)(uintptr_t)(*(feature_data+1)) = S5k2l8sx_OTP_ERROR_CODE;//otp_state
					break;
			}
			break;
#endif

	case SENSOR_FEATURE_SET_SHUTTER_FRAME_TIME:
		set_shutter_frame_length((UINT16) *feature_data, (UINT16) *(feature_data + 1));
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
        case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
        default:
            *(MUINT32 *)(uintptr_t)(*(feature_data + 1))
            = (imgsensor_info.pre.framelength << 16)
                + imgsensor_info.pre.linelength;
            break;
        }
        break;
	case SENSOR_FEATURE_GET_PIXEL_RATE:

		switch (*feature_data) {
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
			(imgsensor_info.cap.pclk /
			(imgsensor_info.cap.linelength - 80))*
			imgsensor_info.cap.grabwindow_width;

			break;
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
			(imgsensor_info.normal_video.pclk /
			(imgsensor_info.normal_video.linelength - 80))*
			imgsensor_info.normal_video.grabwindow_width;

			break;
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
			(imgsensor_info.hs_video.pclk /
			(imgsensor_info.hs_video.linelength - 80))*
			imgsensor_info.hs_video.grabwindow_width;

			break;
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
			(imgsensor_info.slim_video.pclk /
			(imgsensor_info.slim_video.linelength - 80))*
			imgsensor_info.slim_video.grabwindow_width;

			break;
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		default:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
			(imgsensor_info.pre.pclk /
			(imgsensor_info.pre.linelength - 80))*
			imgsensor_info.pre.grabwindow_width;
			break;
		}
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
        case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
        default:
            rate = imgsensor_info.pre.mipi_pixel_rate;
            break;
        }
        LOG_INF("sc1300mcs SENSOR_FEATURE_GET_MIPI_PIXEL_RATE");
        *(MUINT32 *) (uintptr_t) (*(feature_data + 1)) = rate;
    }
    break;

	default:
	    break;
	}

	return ERROR_NONE;
}	/*	feature_control()  */

static struct SENSOR_FUNCTION_STRUCT sensor_func = {
	open,
	get_info,
	get_resolution,
	feature_control,
	control,
	close
};


UINT32 W2SC1300MCSFRONTST_MIPI_RAW_SensorInit(struct SENSOR_FUNCTION_STRUCT **pfFunc)
{
	/* To Do : Check Sensor status here */
	if (pfFunc!= NULL)
		*pfFunc= &sensor_func;
	return ERROR_NONE;
}	/*	OV5693_MIPI_RAW_SensorInit	*/
