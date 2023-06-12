/*****************************************************************************
 *
 * Filename:
 * ---------
 *	 S5K4H5YCmipi_Sensor.c
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
#include <linux/xlog.h>

#include "kd_camera_hw.h"
#include "kd_imgsensor.h"
#include "kd_imgsensor_define.h"
#include "kd_imgsensor_errcode.h"

#include "s5k4h5ycmipiraw_Sensor.h"

#define PFX "S5K4H5YC_camera_sensor"
#define LOG_INF(format, args...)	xlog_printk(ANDROID_LOG_INFO   , PFX, "[%s] " format, __FUNCTION__, ##args)
extern int iReadReg(u16 a_u2Addr , u8 * a_puBuff , u16 i2cId);//add by hhl
extern int iWriteReg(u16 a_u2Addr , u32 a_u4Data , u32 a_u4Bytes , u16 i2cId);//add by hhl
#define write_cmos_sensor(addr, para) iWriteReg((u16) addr , (u32) para , 1, imgsensor.i2c_write_id)//add by hhl
static DEFINE_SPINLOCK(imgsensor_drv_lock);


static imgsensor_info_struct imgsensor_info = { 
	.sensor_id = S5K4H5YC_SENSOR_ID,
	
	.checksum_value = 0x82256eb5,
	
	.pre = {
		.pclk = 280000000,
		.linelength = 3688,
		.framelength = 2512,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 1600,		//record different mode's width of grabwindow
		.grabwindow_height = 1200,		//record different mode's height of grabwindow
		/*	 following for MIPIDataLowPwr2HighSpeedSettleDelayCount by different scenario	*/
		.mipi_data_lp2hs_settle_dc = 14,
		/*	 following for GetDefaultFramerateByScenario()	*/
		.max_framerate = 300,	
	},
	.cap = {
		.pclk = 280000000,
		.linelength = 3688,
		.framelength = 2512,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 3200,
		.grabwindow_height = 2400,
		.mipi_data_lp2hs_settle_dc = 14,
		.max_framerate = 300,
	},
	.cap1 = {
		.pclk = 140000000,
		.linelength = 3688,
		.framelength = 2530,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 3200,
		.grabwindow_height = 2400,
		.mipi_data_lp2hs_settle_dc = 14,
		.max_framerate = 150,
	},
	.normal_video = {
		.pclk = 280000000,
		.linelength = 3688,
		.framelength = 2512,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 3200,
		.grabwindow_height = 2400,
		.mipi_data_lp2hs_settle_dc = 14,
		.max_framerate = 300,
	},
	.hs_video = {
		.pclk = 280000000,
		.linelength = 3688,
		.framelength = 640,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 820,
		.grabwindow_height = 616,
		.mipi_data_lp2hs_settle_dc = 14,
		.max_framerate = 1200,
	},
	.slim_video = {
		.pclk = 280000000,
		.linelength = 3688,
		.framelength = 2512,
		.startx = 0,
		.starty = 0,
		//.grabwindow_width = 1600,		//record different mode's width of grabwindow
		//.grabwindow_height = 1200,		//record different mode's height of grabwindow
		.grabwindow_width = 1280,
		.grabwindow_height = 720,
		.mipi_data_lp2hs_settle_dc = 14,
		.max_framerate = 300,
	},
	.margin = 16,
	.min_shutter = 3,
	.max_frame_length = 0xffff,
	.ae_shut_delay_frame = 0,
	.ae_sensor_gain_delay_frame = 1,
	.ae_ispGain_delay_frame = 2,
	.ihdr_support = 0,	  //1, support; 0,not support
	.ihdr_le_firstline = 0,  //1,le first ; 0, se first
	.sensor_mode_num = 5,	  //support sensor mode num
	
	.cap_delay_frame = 3, 
	.pre_delay_frame = 3, 
	.video_delay_frame = 3,
	.hs_video_delay_frame = 3,
	.slim_video_delay_frame = 3,
	
	.isp_driving_current = ISP_DRIVING_8MA,
	.sensor_interface_type = SENSOR_INTERFACE_TYPE_MIPI,
    .mipi_sensor_type = MIPI_OPHY_NCSI2, //0,MIPI_OPHY_NCSI2;  1,MIPI_OPHY_CSI2
    .mipi_settle_delay_mode = MIPI_SETTLEDELAY_AUTO,//0,MIPI_SETTLEDELAY_AUTO; 1,MIPI_SETTLEDELAY_MANNUAL
	.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_Gr,
	.mclk = 24,
	.mipi_lane_num = SENSOR_MIPI_4_LANE,
	.i2c_addr_table = {0x6E},
    .i2c_speed = 300, 
};


static imgsensor_struct imgsensor = {
	.mirror = IMAGE_NORMAL,				//mirrorflip information
	.sensor_mode = IMGSENSOR_MODE_INIT, //IMGSENSOR_MODE enum value,record current sensor mode,such as: INIT, Preview, Capture, Video,High Speed Video, Slim Video
	.shutter = 0x3D0,					//current shutter
	.gain = 0x100,						//current gain
	.dummy_pixel = 0,					//current dummypixel
	.dummy_line = 0,					//current dummyline
    .current_fps = 30,  //full size current fps : 24fps for PIP, 30fps for Normal or ZSD
    .autoflicker_en = KAL_FALSE,  //auto flicker enable: KAL_FALSE for disable auto flicker, KAL_TRUE for enable auto flicker
	.test_pattern = KAL_FALSE,		//test pattern mode or not. KAL_TRUE for in test pattern mode, KAL_FALSE for normal output
	.current_scenario_id = MSDK_SCENARIO_ID_CAMERA_PREVIEW,//current scenario id
	.ihdr_en = 0, //sensor need support LE, SE with HDR feature
	.i2c_write_id = 0x20,
};


/* Sensor output window information */
static SENSOR_WINSIZE_INFO_STRUCT imgsensor_winsize_info[5] =	 
{{ 3280, 2464,	  0,	  0,   3280, 2464, 1640, 1232, 0000, 0000, 1640, 1232,	0,	0, 1600, 1200}, // Preview 
 { 3280, 2464,	  0,	  0,   3280, 2464, 3280, 2464, 0000, 0000, 3280, 2464,	0,	0, 3200, 2400}, // capture 
 { 3280, 2464,	  0,	  0,   3280, 2464, 3280, 2464, 0000, 0000, 3280, 2464,	0,	0, 3200, 2400}, // video 
 { 3280, 2464,	  0,    0,   3280, 2464, 820,  616,  0000, 0000, 820,  616,	  0,	0, 820,  616}, //hight speed video 
 { 3280, 2464,	  360,  512, 2562, 1440, 1280, 720,  0000, 0000, 1280, 720,	  0,	0, 1280, 720}};// slim video 


static kal_uint16 read_cmos_sensor(kal_uint32 addr)
/*{
	kal_uint16 get_byte=0;

	char pu_send_cmd[2] = {(char)(addr >> 8), (char)(addr & 0xFF) };
	iReadRegI2C(pu_send_cmd, 2, (u8*)&get_byte, 1, imgsensor.i2c_write_id);

	return get_byte;
}*/
{
	kal_uint16 get_byte=0;
    iReadReg((u16) addr ,(u8*)&get_byte,imgsensor.i2c_write_id);
    return get_byte;
}

/*
static void write_cmos_sensor(kal_uint32 addr, kal_uint32 para)
{
	char pu_send_cmd[3] = {(char)(addr >> 8), (char)(addr & 0xFF), (char)(para & 0xFF)};
	iWriteRegI2C(pu_send_cmd, 3, imgsensor.i2c_write_id);
}
*/

static void set_dummy()
{
	LOG_INF("dummyline = %d, dummypixels = %d \n", imgsensor.dummy_line, imgsensor.dummy_pixel);

	write_cmos_sensor(0x0340, imgsensor.frame_length >> 8);
	write_cmos_sensor(0x0341, imgsensor.frame_length & 0xFF);	  
	write_cmos_sensor(0x0342, imgsensor.line_length >> 8);
	write_cmos_sensor(0x0343, imgsensor.line_length & 0xFF);
}
		

static void set_max_framerate(UINT16 framerate,kal_bool min_framelength_en)
{
	kal_int16 dummy_line;
	kal_uint32 frame_length = imgsensor.frame_length;
	//unsigned long flags;

	LOG_INF("framerate = %d, min_framelength_en=%d\n", framerate,min_framelength_en);
	frame_length = imgsensor.pclk / framerate * 10 / imgsensor.line_length;
	LOG_INF("frame_length =%d\n", frame_length);
	spin_lock(&imgsensor_drv_lock);
	imgsensor.frame_length = (frame_length > imgsensor.min_frame_length) ? frame_length : imgsensor.min_frame_length; 
	imgsensor.dummy_line = imgsensor.frame_length - imgsensor.min_frame_length;
	//dummy_line = frame_length - imgsensor.min_frame_length;
	//if (dummy_line < 0)
		//imgsensor.dummy_line = 0;
	//else
		//imgsensor.dummy_line = dummy_line;
	//imgsensor.frame_length = frame_length + imgsensor.dummy_line;
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


static void write_shutter(kal_uint16 shutter)
	{
		kal_uint16 realtime_fps = 0;
		kal_uint32 frame_length = 0;
		// shutter=2512;//add for debug capture framerate  
		/* 0x3500, 0x3501, 0x3502 will increase VBLANK to get exposure larger than frame exposure */
		/* AE doesn't update sensor gain at capture mode, thus extra exposure lines must be updated here. */
		
		// OV Recommend Solution
		// if shutter bigger than frame_length, should extend frame length first
	
		if(imgsensor.sensor_mode == IMGSENSOR_MODE_HIGH_SPEED_VIDEO)
		{
			if(shutter > imgsensor.min_frame_length - imgsensor_info.margin)
				shutter = imgsensor.min_frame_length - imgsensor_info.margin;
			write_cmos_sensor(0x0340, imgsensor.frame_length >> 8);
			write_cmos_sensor(0x0341, imgsensor.frame_length & 0xFF);
			write_cmos_sensor(0x0202, (shutter >> 8) & 0xFF); 
 			write_cmos_sensor(0x0203, shutter  & 0xFF);
			LOG_INF("shutter =%d, framelength =%d\n", shutter,imgsensor.frame_length);
			return;
		}
		spin_lock(&imgsensor_drv_lock);
		if (shutter > imgsensor.min_frame_length - imgsensor_info.margin)
			{
			imgsensor.frame_length = shutter + imgsensor_info.margin;
			}
		else
			{
			imgsensor.frame_length = imgsensor.min_frame_length;
			}
		if (imgsensor.frame_length > imgsensor_info.max_frame_length)
			{
			imgsensor.frame_length = imgsensor_info.max_frame_length;
			}
		spin_unlock(&imgsensor_drv_lock);
		if (shutter < imgsensor_info.min_shutter) 
			shutter = imgsensor_info.min_shutter;
		
		if (imgsensor.autoflicker_en == KAL_TRUE) { 
			realtime_fps = imgsensor.pclk / imgsensor.line_length * 10 / imgsensor.frame_length;
			if(realtime_fps >= 297 && realtime_fps <= 305)
			{
				set_max_framerate(296,0);
				//set_dummy();
			}
			else if(realtime_fps >= 147 && realtime_fps <= 150)
			{
				set_max_framerate(146,0);	
				//set_dummy();
			}
			else{
			write_cmos_sensor(0x0340, imgsensor.frame_length >> 8);
			write_cmos_sensor(0x0341, imgsensor.frame_length & 0xFF);
			}
		} else {
			// Extend frame length
			write_cmos_sensor(0x0340, imgsensor.frame_length >> 8);
			write_cmos_sensor(0x0341, imgsensor.frame_length & 0xFF);
		}
	
		// Update Shutter
		//write_cmos_sensor(0x0340, imgsensor.frame_length >> 8);
		//write_cmos_sensor(0x0341, imgsensor.frame_length & 0xFF);
		write_cmos_sensor(0x0202, (shutter >> 8) & 0xFF); 
 		write_cmos_sensor(0x0203, shutter  & 0xFF);
 		LOG_INF("realtime_fps =%d\n", realtime_fps);
		LOG_INF("shutter =%d, framelength =%d\n", shutter,imgsensor.frame_length);
	
		//LOG_INF("frame_length = %d ", frame_length);
		
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
	unsigned long flags;
	spin_lock_irqsave(&imgsensor_drv_lock, flags);
	imgsensor.shutter = shutter;
	spin_unlock_irqrestore(&imgsensor_drv_lock, flags);
	write_shutter(shutter);
}	/*	set_shutter */



static kal_uint16 gain2reg(const kal_uint16 gain)
{
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
	gain = gain / 2;
	
	//write_cmos_sensor(0x0104, 0x01);	
	write_cmos_sensor(0x0204,(gain>>8));
	write_cmos_sensor(0x0205,(gain&0xff));
	//write_cmos_sensor(0x0104, 0x00);
}   /*  S5K4H5YCMIPI_SetGain  */

static void ihdr_write_shutter_gain(kal_uint16 le, kal_uint16 se, kal_uint16 gain)
{
	LOG_INF("le:0x%x, se:0x%x, gain:0x%x\n",le,se,gain);

}



static void set_mirror_flip(kal_uint8 image_mirror)
{
	LOG_INF("image_mirror = %d\n", image_mirror);

	/********************************************************
	   *
	   *   0x3820[2] ISP Vertical flip
	   *   0x3820[1] Sensor Vertical flip
	   *
	   *   0x3821[2] ISP Horizontal mirror
	   *   0x3821[1] Sensor Horizontal mirror
	   *
	   *   ISP and Sensor flip or mirror register bit should be the same!!
	   *
	   ********************************************************/
	
	switch (image_mirror)
    {
        case IMAGE_NORMAL: //B
            write_cmos_sensor(0x0101, 0x03);	//Set normal
            break;
        case IMAGE_V_MIRROR: //Gr X
            write_cmos_sensor(0x0101, 0x01);	//Set flip
            break;
        case IMAGE_H_MIRROR: //Gb
            write_cmos_sensor(0x0101, 0x02);	//Set mirror
            break;
        case IMAGE_HV_MIRROR: //R
            write_cmos_sensor(0x0101, 0x00);	//Set mirror and flip
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
static void night_mode(kal_bool enable)
{
/*No Need to implement this function*/ 
}	/*	night_mode	*/

static void sensor_init(void)
{
}   /*  S5K4H5YCMIPI_Sensor_Init  */


static void preview_setting(void)
{ 
		write_cmos_sensor(0x0100,0x00);
		write_cmos_sensor(0x0101,0x00);
		write_cmos_sensor(0x0204,0x00);
		write_cmos_sensor(0x0205,0x20);
		write_cmos_sensor(0x0200,0x0D);//Caval 140613
  	write_cmos_sensor(0x0201,0x78);//Caval 140613
		write_cmos_sensor(0x0202,0x04);
		write_cmos_sensor(0x0203,0xE2);
		write_cmos_sensor(0x0340,0x09);
		write_cmos_sensor(0x0341,0xD0);
		write_cmos_sensor(0x0342,0x0E);
		write_cmos_sensor(0x0343,0x68);
		write_cmos_sensor(0x0344,0x00);
		write_cmos_sensor(0x0345,0x00);
		write_cmos_sensor(0x0346,0x00);
		write_cmos_sensor(0x0347,0x00);
		write_cmos_sensor(0x0348,0x0C);
		write_cmos_sensor(0x0349,0xD1);//Caval 140613
		write_cmos_sensor(0x034A,0x09);
		write_cmos_sensor(0x034B,0x9F);
		write_cmos_sensor(0x034C,0x06);
		write_cmos_sensor(0x034D,0x68);
		write_cmos_sensor(0x034E,0x04);
		write_cmos_sensor(0x034F,0xD0);
		write_cmos_sensor(0x0390,0x01);
		write_cmos_sensor(0x0391,0x22);
		write_cmos_sensor(0x0940,0x00);
		write_cmos_sensor(0x0381,0x01);
		write_cmos_sensor(0x0383,0x03);
		write_cmos_sensor(0x0385,0x01);
		write_cmos_sensor(0x0387,0x03);
		write_cmos_sensor(0x0301,0x02);
		write_cmos_sensor(0x0303,0x01);
		write_cmos_sensor(0x0305,0x06);
		write_cmos_sensor(0x0306,0x00);
		write_cmos_sensor(0x0307,0x8C);
		write_cmos_sensor(0x0309,0x02);
		write_cmos_sensor(0x030B,0x01);
		write_cmos_sensor(0x3C59,0x00);
		write_cmos_sensor(0x030D,0x06);
		write_cmos_sensor(0x030E,0x00);
		write_cmos_sensor(0x030F,0xAF);
		write_cmos_sensor(0x3C5A,0x00);
		write_cmos_sensor(0x0310,0x01);
		write_cmos_sensor(0x3C50,0x53);
		write_cmos_sensor(0x3C62,0x02);
		write_cmos_sensor(0x3C63,0xBC);
		write_cmos_sensor(0x3C64,0x00);
		write_cmos_sensor(0x3C65,0x00);
		write_cmos_sensor(0x0114,0x03);
		write_cmos_sensor(0x3C1E,0x0F);//Caval 140613
		write_cmos_sensor(0x3500,0x0C);
		write_cmos_sensor(0x3C1A,0xA8);
		write_cmos_sensor(0x3B29,0x01);//Caval 140613
		write_cmos_sensor(0x3300,0x00);//Caval 140613			//20150309
		write_cmos_sensor(0x3000,0x07);
		write_cmos_sensor(0x3001,0x05);
		write_cmos_sensor(0x3002,0x03);
		write_cmos_sensor(0x0200,0x0C);
		write_cmos_sensor(0x0201,0xB4);
		write_cmos_sensor(0x300A,0x03);
		write_cmos_sensor(0x300C,0x65);
		write_cmos_sensor(0x300D,0x54);
		write_cmos_sensor(0x3010,0x00);
		write_cmos_sensor(0x3012,0x14);
		write_cmos_sensor(0x3014,0x19);
		write_cmos_sensor(0x3017,0x0F);
		write_cmos_sensor(0x3018,0x1A);
		write_cmos_sensor(0x3019,0x6C);
		write_cmos_sensor(0x301A,0x78);
		write_cmos_sensor(0x306F,0x00);
		write_cmos_sensor(0x3070,0x00);
		write_cmos_sensor(0x3071,0x00);
		write_cmos_sensor(0x3072,0x00);
		write_cmos_sensor(0x3073,0x00);
		write_cmos_sensor(0x3074,0x00);
		write_cmos_sensor(0x3075,0x00);
		write_cmos_sensor(0x3076,0x0A);
		write_cmos_sensor(0x3077,0x03);
		write_cmos_sensor(0x3078,0x84);
		write_cmos_sensor(0x3079,0x00);
		write_cmos_sensor(0x307A,0x00);
		write_cmos_sensor(0x307B,0x00);
		write_cmos_sensor(0x307C,0x00);
		write_cmos_sensor(0x3085,0x00);
		write_cmos_sensor(0x3086,0x72);
		write_cmos_sensor(0x30A6,0x01);
		write_cmos_sensor(0x30A7,0x0E);
		write_cmos_sensor(0x3032,0x01);
		write_cmos_sensor(0x3037,0x02);
		write_cmos_sensor(0x304A,0x01);
		write_cmos_sensor(0x3054,0xF0);
		write_cmos_sensor(0x3044,0x20);
		write_cmos_sensor(0x3045,0x20);
		write_cmos_sensor(0x3047,0x04);
		write_cmos_sensor(0x3048,0x11);
		write_cmos_sensor(0x303D,0x08);
		write_cmos_sensor(0x304B,0x31);
		write_cmos_sensor(0x3063,0x00);
		write_cmos_sensor(0x303A,0x0B);
		write_cmos_sensor(0x302D,0x7F);
		write_cmos_sensor(0x3039,0x45);
		write_cmos_sensor(0x3038,0x10);
		write_cmos_sensor(0x3097,0x11);
		write_cmos_sensor(0x3096,0x01);
		write_cmos_sensor(0x3042,0x01);
		write_cmos_sensor(0x3053,0x01);
		write_cmos_sensor(0x320B,0x40);
		write_cmos_sensor(0x320C,0x06);
		write_cmos_sensor(0x320D,0xC0);
		write_cmos_sensor(0x3202,0x00);
		write_cmos_sensor(0x3203,0x3D);
		write_cmos_sensor(0x3204,0x00);
		write_cmos_sensor(0x3205,0x3D);
		write_cmos_sensor(0x3206,0x00);
		write_cmos_sensor(0x3207,0x3D);
		write_cmos_sensor(0x3208,0x00);
		write_cmos_sensor(0x3209,0x3D);
		write_cmos_sensor(0x3211,0x02);
		write_cmos_sensor(0x3212,0x21);
		write_cmos_sensor(0x3213,0x02);
		write_cmos_sensor(0x3214,0x21);
		write_cmos_sensor(0x3215,0x02);
		write_cmos_sensor(0x3216,0x21);
		write_cmos_sensor(0x3217,0x02);
		write_cmos_sensor(0x3218,0x21);
		write_cmos_sensor(0x0100,0x01);
	}   /*  S5K4H5YCMIPI_Capture_Setting  */

static void capture_setting(kal_uint16 currefps)
{
    if (currefps == 150) 
	{
			write_cmos_sensor(0x0100, 0x00);
			write_cmos_sensor(0x0101, 0x00);
			write_cmos_sensor(0x0204, 0x00);
			write_cmos_sensor(0x0205, 0x20);
			write_cmos_sensor(0x0200, 0x0C);
			write_cmos_sensor(0x0201, 0xB4);
			write_cmos_sensor(0x0202, 0x04);
			write_cmos_sensor(0x0203, 0xE2);
			write_cmos_sensor(0x0340, 0x09);
			write_cmos_sensor(0x0341, 0xE2);
			write_cmos_sensor(0x0342, 0x0E);
			write_cmos_sensor(0x0343, 0x68);
			write_cmos_sensor(0x0344, 0x00);
			write_cmos_sensor(0x0345, 0x00);
			write_cmos_sensor(0x0346, 0x00);
			write_cmos_sensor(0x0347, 0x00);
			write_cmos_sensor(0x0348, 0x0C);
			write_cmos_sensor(0x0349, 0xCF);
			write_cmos_sensor(0x034A, 0x09);
			write_cmos_sensor(0x034B, 0x9F);
			write_cmos_sensor(0x034C, 0x0C);
			write_cmos_sensor(0x034D, 0xD0);
			write_cmos_sensor(0x034E, 0x09);
			write_cmos_sensor(0x034F, 0xA0);
			write_cmos_sensor(0x0390, 0x00);
			write_cmos_sensor(0x0391, 0x00);
			write_cmos_sensor(0x0940, 0x00);
			write_cmos_sensor(0x0381, 0x01);
			write_cmos_sensor(0x0383, 0x01);
			write_cmos_sensor(0x0385, 0x01);
			write_cmos_sensor(0x0387, 0x01);
			write_cmos_sensor(0x0114, 0x03);
			write_cmos_sensor(0x0301, 0x04);
			write_cmos_sensor(0x0303, 0x01);
			write_cmos_sensor(0x0305, 0x06);
			write_cmos_sensor(0x0306, 0x00);
			write_cmos_sensor(0x0307, 0x8C);
			write_cmos_sensor(0x0309, 0x02);
			write_cmos_sensor(0x030B, 0x01);
			write_cmos_sensor(0x3C59, 0x00);
			write_cmos_sensor(0x030D, 0x06);
			write_cmos_sensor(0x030E, 0x00);
			write_cmos_sensor(0x030F, 0xAF);
			write_cmos_sensor(0x3C5A, 0x01);
			write_cmos_sensor(0x0310, 0x01);
			write_cmos_sensor(0x3C50, 0x53);
			write_cmos_sensor(0x3C62, 0x01);
			write_cmos_sensor(0x3C63, 0x5E);
			write_cmos_sensor(0x3C64, 0x00);
			write_cmos_sensor(0x3C65, 0x00);
			write_cmos_sensor(0x3C1E, 0x0F);
			write_cmos_sensor(0x3000, 0x07);
			write_cmos_sensor(0x3001, 0x05);
			write_cmos_sensor(0x3002, 0x03);
			write_cmos_sensor(0x0200, 0x0C);
			write_cmos_sensor(0x0201, 0xB4);
			write_cmos_sensor(0x300A, 0x03);
			write_cmos_sensor(0x300C, 0x65);
			write_cmos_sensor(0x300D, 0x54);
			write_cmos_sensor(0x3010, 0x00);
			write_cmos_sensor(0x3012, 0x14);
			write_cmos_sensor(0x3014, 0x19);
			write_cmos_sensor(0x3017, 0x0F);
			write_cmos_sensor(0x3018, 0x1A);
			write_cmos_sensor(0x3019, 0x6C);
			write_cmos_sensor(0x301A, 0x78);
			write_cmos_sensor(0x306F, 0x00);
			write_cmos_sensor(0x3070, 0x00);
			write_cmos_sensor(0x3071, 0x00);
			write_cmos_sensor(0x3072, 0x00);
			write_cmos_sensor(0x3073, 0x00);
			write_cmos_sensor(0x3074, 0x00);
			write_cmos_sensor(0x3075, 0x00);
			write_cmos_sensor(0x3076, 0x0A);
			write_cmos_sensor(0x3077, 0x03);
			write_cmos_sensor(0x3078, 0x84);
			write_cmos_sensor(0x3079, 0x00);
			write_cmos_sensor(0x307A, 0x00);
			write_cmos_sensor(0x307B, 0x00);
			write_cmos_sensor(0x307C, 0x00);
			write_cmos_sensor(0x3085, 0x00);
			write_cmos_sensor(0x3086, 0x72);
			write_cmos_sensor(0x30A6, 0x01);
			write_cmos_sensor(0x30A7, 0x0E);
			write_cmos_sensor(0x3032, 0x01);
			write_cmos_sensor(0x3037, 0x02);
			write_cmos_sensor(0x304A, 0x01);
			write_cmos_sensor(0x3054, 0xF0);
			write_cmos_sensor(0x3044, 0x20);
			write_cmos_sensor(0x3045, 0x20);
			write_cmos_sensor(0x3047, 0x04);
			write_cmos_sensor(0x3048, 0x11);
			write_cmos_sensor(0x303D, 0x08);
			write_cmos_sensor(0x304B, 0x31);
			write_cmos_sensor(0x3063, 0x00);
			write_cmos_sensor(0x303A, 0x0B);
			write_cmos_sensor(0x302D, 0x7F);
			write_cmos_sensor(0x3039, 0x45);
			write_cmos_sensor(0x3038, 0x10);
			write_cmos_sensor(0x3097, 0x11);
			write_cmos_sensor(0x3096, 0x01);
			write_cmos_sensor(0x3042, 0x01);
			write_cmos_sensor(0x3053, 0x01);
			write_cmos_sensor(0x320B, 0x40);
			write_cmos_sensor(0x320C, 0x06);
			write_cmos_sensor(0x320D, 0xC0);
			write_cmos_sensor(0x3202, 0x00);
			write_cmos_sensor(0x3203, 0x3D);
			write_cmos_sensor(0x3204, 0x00);
			write_cmos_sensor(0x3205, 0x3D);
			write_cmos_sensor(0x3206, 0x00);
			write_cmos_sensor(0x3207, 0x3D);
			write_cmos_sensor(0x3208, 0x00);
			write_cmos_sensor(0x3209, 0x3D);
			write_cmos_sensor(0x3211, 0x02);
			write_cmos_sensor(0x3212, 0x21);
			write_cmos_sensor(0x3213, 0x02);
			write_cmos_sensor(0x3214, 0x21);
			write_cmos_sensor(0x3215, 0x02);
			write_cmos_sensor(0x3216, 0x21);
			write_cmos_sensor(0x3217, 0x02);
			write_cmos_sensor(0x3218, 0x21);
			write_cmos_sensor(0x0100, 0x01);
	}
	else
	{ 
		write_cmos_sensor(0x0100,0x00);
		write_cmos_sensor(0x0101,0x00);
		write_cmos_sensor(0x0204,0x00);
		write_cmos_sensor(0x0205,0x20);
		//write_cmos_sensor(0x0200,0x0C);//Caval 140613
		//write_cmos_sensor(0x0201,0xB4);//Caval 140613
		write_cmos_sensor(0x0202,0x04);
		write_cmos_sensor(0x0203,0xE2);
		write_cmos_sensor(0x0340,0x09);
		write_cmos_sensor(0x0341,0xD0);
		write_cmos_sensor(0x0342,0x0E);
		write_cmos_sensor(0x0343,0x68);
		write_cmos_sensor(0x0344,0x00);
		write_cmos_sensor(0x0345,0x00);
		write_cmos_sensor(0x0346,0x00);
		write_cmos_sensor(0x0347,0x00);
		write_cmos_sensor(0x0348,0x0C);
		write_cmos_sensor(0x0349,0xCF);
		write_cmos_sensor(0x034A,0x09);
		write_cmos_sensor(0x034B,0x9F);
		write_cmos_sensor(0x034C,0x0C);
		write_cmos_sensor(0x034D,0xD0);
		write_cmos_sensor(0x034E,0x09);
		write_cmos_sensor(0x034F,0xA0);
		write_cmos_sensor(0x0390,0x00);
		write_cmos_sensor(0x0391,0x00);
		write_cmos_sensor(0x0940,0x00);
		write_cmos_sensor(0x0381,0x01);
		write_cmos_sensor(0x0383,0x01);
		write_cmos_sensor(0x0385,0x01);
		write_cmos_sensor(0x0387,0x01);
		write_cmos_sensor(0x0301,0x02);
		write_cmos_sensor(0x0303,0x01);
		write_cmos_sensor(0x0305,0x06);
		write_cmos_sensor(0x0306,0x00);
		write_cmos_sensor(0x0307,0x8C);
		write_cmos_sensor(0x0309,0x02);
		write_cmos_sensor(0x030B,0x01);
		write_cmos_sensor(0x3C59,0x00);
		write_cmos_sensor(0x030D,0x06);
		write_cmos_sensor(0x030E,0x00);
		write_cmos_sensor(0x030F,0xAF);
		write_cmos_sensor(0x3C5A,0x00);
		write_cmos_sensor(0x0310,0x01);
		write_cmos_sensor(0x3C50,0x53);
		write_cmos_sensor(0x3C62,0x02);
		write_cmos_sensor(0x3C63,0xBC);
		write_cmos_sensor(0x3C64,0x00);
		write_cmos_sensor(0x3C65,0x00);


        //below only need to set after reset sensor.
			//write_cmos_sensor(0x0202,0x04);
			//write_cmos_sensor(0x0203,0xE2);
			//write_cmos_sensor(0x0340,0x09);
			//write_cmos_sensor(0x0341,0xD0);
			write_cmos_sensor(0x0114,0x03);
	
	////////////////////////////////////////////////////
	//	name:  S5K4H5YX EVT3 setfile
	//	vR10a Temporary version
	//	Image size : Full size - 1280x720, 100 fps
	//	EXTCLK : 24 MHz
	//	System PLL output : 560 MHz
	//	Output PLL output : 700 Mbps
	//	GCLK (ATOP) : 280 MHz
	//	DCLK_4 (ATOP) : 70 MHz
	////////////////////////////////////////////////////
	
	//$MIPI[Width:1280,Height:720,Format:Raw10,Lane:4,ErrorCheck:0,PolarityData:0,PolarityClock:0,Buffer:4]
	
	
	// Streaming off -----------------------------------
	
	// Image Orientation -------------------------------
	
	// Analog Gain -------------------------------------
	
	// Exposure Time -----------------------------------
	
	// Frame Rate --------------------------------------
	
	// Image Size --------------------------------------
	
	// Analog Binning ----------------------------------
	
	// Digital Binning ---------------------------------
	
	// Sub-Sampling Ratio ------------------------------
	
	// PLL control -------------------------------------
			//	[3:0]	reg_DIV_G
	
	// Embedded line on/off ----------------------------
	write_cmos_sensor(0x3C1E,0x0F); 	//	[3] reg_isp_fe_TN_SMIA_sync_sel
			//	[2] reg_pat_TN_SMIA_sync_sel
			//	[1] reg_bpc_TN_SMIA_sync_sel
			//	[0] reg_outif_TN_SMIA_sync_sel
	
	
	// BPC on/off --------------------------------------
	write_cmos_sensor(0x3500,0x0C); 	//	[3] bpcms_rate_auto
		write_cmos_sensor(0x3C1A,0xA8);
		write_cmos_sensor(0x3B29,0x01);//Caval 140613
		write_cmos_sensor(0x3300,0x00);//Caval 140613		//20150309	//0x00 is open, 0x01 is close
	
	// Analog Tuning for 3280x2464 30fps R10 131115
	write_cmos_sensor(0x3000,0x07);    // ct_ld_start
	write_cmos_sensor(0x3001,0x05);    // ct_sl_start
	write_cmos_sensor(0x3002,0x03);    // ct_rx_start
		write_cmos_sensor(0x0200,0x0C);
	write_cmos_sensor(0x0201,0xB4);    // (fine_integ_time) (LSB)
	write_cmos_sensor(0x300A,0x03);    // ct_cds_start
	write_cmos_sensor(0x300C,0x65);    // ct_s3_width
	write_cmos_sensor(0x300D,0x54);    // ct_s4_width
	write_cmos_sensor(0x3010,0x00);    // ct_pbr_width
	write_cmos_sensor(0x3012,0x14);    // ct_pbs_width
	write_cmos_sensor(0x3014,0x19);    // ct_pbr_ob_width
	write_cmos_sensor(0x3017,0x0F);    // ct_cds_lim_start
	write_cmos_sensor(0x3018,0x1A);    // ct_rmp_off_start
	write_cmos_sensor(0x3019,0x6C);    // ct_rmp_rst_start
	write_cmos_sensor(0x301A,0x78);    // ct_rmp_sig_start
	write_cmos_sensor(0x306F,0x00);    // ct_opt_l0_width1 (MSB)
	write_cmos_sensor(0x3070,0x00);    // ct_opt_l0_width1 (LSB)
	write_cmos_sensor(0x3071,0x00);    // ct_opt_l0_start2 (MSB)
	write_cmos_sensor(0x3072,0x00);    // ct_opt_l0_start2 (LSB)
	write_cmos_sensor(0x3073,0x00);    // ct_opt_l0_width2 (MSB)
	write_cmos_sensor(0x3074,0x00);    // ct_opt_l0_width2 (LSB)
	write_cmos_sensor(0x3075,0x00);    // ct_opt_l1_start1 (MSB)
	write_cmos_sensor(0x3076,0x0A);    // ct_opt_l1_start1 (LSB)
	write_cmos_sensor(0x3077,0x03);    // ct_opt_l1_width1 (MSB)
	write_cmos_sensor(0x3078,0x84);    // ct_opt_l1_width1 (LSB)
	write_cmos_sensor(0x3079,0x00);    // ct_opt_l1_start2 (MSB)
	write_cmos_sensor(0x307A,0x00);    // ct_opt_l1_start2 (LSB)
	write_cmos_sensor(0x307B,0x00);    // ct_opt_l1_width2 (MSB)
	write_cmos_sensor(0x307C,0x00);    // ct_opt_l1_width2 (LSB)
	write_cmos_sensor(0x3085,0x00);    // ct_opt_h0_start1 (MSB)
	write_cmos_sensor(0x3086,0x72);    // ct_opt_h0_start1 (LSB)
	write_cmos_sensor(0x30A6,0x01);    // cds_option 2
	write_cmos_sensor(0x30A7,0x0E);    // cds_option 1
	write_cmos_sensor(0x3032,0x01);    // rmp_option
	write_cmos_sensor(0x3037,0x02);    // dbs_option
	write_cmos_sensor(0x304A,0x01);    // dbr_option
	write_cmos_sensor(0x3054,0xF0);    // rdv_option
	write_cmos_sensor(0x3044,0x20);    // dbr_tune_tg
	write_cmos_sensor(0x3045,0x20);    // dbr_tune_rgsl
	write_cmos_sensor(0x3047,0x04);    // dbr_tune_ntg
	write_cmos_sensor(0x3048,0x11);    // dbr_tune_rd
	write_cmos_sensor(0x303D,0x08);    // off_rst
	write_cmos_sensor(0x304B,0x31);    // adc_sat (530mV)
	write_cmos_sensor(0x3063,0x00);    // ldb_ctrl
	write_cmos_sensor(0x303A,0x0B);    // clp_lvl
	write_cmos_sensor(0x302D,0x7F);    // (aig_main2)
	write_cmos_sensor(0x3039,0x45);    // pix/pxbst_bias
	write_cmos_sensor(0x3038,0x10);    // comp1/2_bias comp1_bias @AG<=x16
	write_cmos_sensor(0x3097,0x11);    // comp1_bias @AG<x8, @AG<x4, respectively
	write_cmos_sensor(0x3096,0x01);    // comp1_bias @AG<x2
	write_cmos_sensor(0x3042,0x01);    // # of L-OB
	write_cmos_sensor(0x3053,0x01);    // lp_vblk_en								  
	write_cmos_sensor(0x320B,0x40);    // adc_default (LSB) 						   
	write_cmos_sensor(0x320C,0x06);    // adc_max (MSB) 							   
	write_cmos_sensor(0x320D,0xC0);    // adc_max (LSB) 							   
	write_cmos_sensor(0x3202,0x00);    // adc_offset_even0 (MSB)					   
	write_cmos_sensor(0x3203,0x3D);    // adc_offset_even0 (LSB)					   
	write_cmos_sensor(0x3204,0x00);    // adc_offset_odd0 (MSB) 					   
	write_cmos_sensor(0x3205,0x3D);    // adc_offset_odd0 (LSB) 					   
	write_cmos_sensor(0x3206,0x00);    // adc_offset_even1 (MSB)					   
	write_cmos_sensor(0x3207,0x3D);    // adc_offset_even1 (LSB)					   
	write_cmos_sensor(0x3208,0x00);    // adc_offset_odd1 (MSB) 					   
	write_cmos_sensor(0x3209,0x3D);    // adc_offset_odd1 (LSB) 					   
	write_cmos_sensor(0x3211,0x02);    // adc_offset_ms_even0 (MSB) 				   
	write_cmos_sensor(0x3212,0x21);    // adc_offset_ms_even0 (LSB) 				   
	write_cmos_sensor(0x3213,0x02);    // adc_offset_ms_odd0 (MSB)					   
	write_cmos_sensor(0x3214,0x21);    // adc_offset_ms_odd0 (LSB)					   
	write_cmos_sensor(0x3215,0x02);    // adc_offset_ms_even1 (MSB) 				   
	write_cmos_sensor(0x3216,0x21);    // adc_offset_ms_even1 (LSB) 				   
	write_cmos_sensor(0x3217,0x02);    // adc_offset_ms_odd1 (MSB)					   
	write_cmos_sensor(0x3218,0x21);    // adc_offset_ms_odd1 (LSB)
		write_cmos_sensor(0x0100,0x01);
	}
}
static void normal_video_setting(kal_uint16 currefps)
	{ 
			LOG_INF("E! currefps:%d\n",currefps);
			write_cmos_sensor(0x0100,0x00);
			write_cmos_sensor(0x0101,0x00);
			write_cmos_sensor(0x0204,0x00);
			write_cmos_sensor(0x0205,0x20);
			write_cmos_sensor(0x0342,0x0E);
			write_cmos_sensor(0x0343,0x68);
			write_cmos_sensor(0x0344,0x00);
			write_cmos_sensor(0x0345,0x00);
			write_cmos_sensor(0x0346,0x00);
			write_cmos_sensor(0x0347,0x00);
			write_cmos_sensor(0x0348,0x0C);
			write_cmos_sensor(0x0349,0xCF);
			write_cmos_sensor(0x034A,0x09);
			write_cmos_sensor(0x034B,0x9F);
			write_cmos_sensor(0x034C,0x0C);
			write_cmos_sensor(0x034D,0xD0);
			write_cmos_sensor(0x034E,0x09);
			write_cmos_sensor(0x034F,0xA0);
			write_cmos_sensor(0x0390,0x00);
			write_cmos_sensor(0x0391,0x00);
			write_cmos_sensor(0x0940,0x00);
			write_cmos_sensor(0x0381,0x01);
			write_cmos_sensor(0x0383,0x01);
			write_cmos_sensor(0x0385,0x01);
			write_cmos_sensor(0x0387,0x01);
			write_cmos_sensor(0x0301,0x02);
			write_cmos_sensor(0x0303,0x01);
			write_cmos_sensor(0x0305,0x06);
			write_cmos_sensor(0x0306,0x00);
			write_cmos_sensor(0x0307,0x8C);
			write_cmos_sensor(0x0309,0x02);
			write_cmos_sensor(0x030B,0x01);
			write_cmos_sensor(0x3C59,0x00);
			write_cmos_sensor(0x030D,0x06);
			write_cmos_sensor(0x030E,0x00);
			write_cmos_sensor(0x030F,0xAF);
			write_cmos_sensor(0x3C5A,0x00);
			write_cmos_sensor(0x0310,0x01);
			write_cmos_sensor(0x3C50,0x53);
			write_cmos_sensor(0x3C62,0x02);
			write_cmos_sensor(0x3C63,0xBC);
			write_cmos_sensor(0x3C64,0x00);
			write_cmos_sensor(0x3C65,0x00);
		write_cmos_sensor(0x0114,0x03);
		write_cmos_sensor(0x3C1E,0x0F);//Caval 140613
		write_cmos_sensor(0x3500,0x0C);
		write_cmos_sensor(0x3C1A,0xA8);
	write_cmos_sensor(0x3B29,0x01); 	 // OTP enable
		write_cmos_sensor(0x3300,0x00);//Caval 140613			//20150309	
		write_cmos_sensor(0x3000,0x07);
		write_cmos_sensor(0x3001,0x05);
		write_cmos_sensor(0x3002,0x03);
		write_cmos_sensor(0x0200,0x0C);
		write_cmos_sensor(0x0201,0xB4);
		write_cmos_sensor(0x300A,0x03);
		write_cmos_sensor(0x300C,0x65);
		write_cmos_sensor(0x300D,0x54);
		write_cmos_sensor(0x3010,0x00);
		write_cmos_sensor(0x3012,0x14);
		write_cmos_sensor(0x3014,0x19);
		write_cmos_sensor(0x3017,0x0F);
		write_cmos_sensor(0x3018,0x1A);
		write_cmos_sensor(0x3019,0x6C);
		write_cmos_sensor(0x301A,0x78);
		write_cmos_sensor(0x306F,0x00);
		write_cmos_sensor(0x3070,0x00);
		write_cmos_sensor(0x3071,0x00);
		write_cmos_sensor(0x3072,0x00);
		write_cmos_sensor(0x3073,0x00);
		write_cmos_sensor(0x3074,0x00);
		write_cmos_sensor(0x3075,0x00);
		write_cmos_sensor(0x3076,0x0A);
		write_cmos_sensor(0x3077,0x03);
		write_cmos_sensor(0x3078,0x84);
		write_cmos_sensor(0x3079,0x00);
		write_cmos_sensor(0x307A,0x00);
		write_cmos_sensor(0x307B,0x00);
		write_cmos_sensor(0x307C,0x00);
		write_cmos_sensor(0x3085,0x00);
		write_cmos_sensor(0x3086,0x72);
		write_cmos_sensor(0x30A6,0x01);
		write_cmos_sensor(0x30A7,0x0E);
		write_cmos_sensor(0x3032,0x01);
		write_cmos_sensor(0x3037,0x02);
		write_cmos_sensor(0x304A,0x01);
		write_cmos_sensor(0x3054,0xF0);
		write_cmos_sensor(0x3044,0x20);
		write_cmos_sensor(0x3045,0x20);
		write_cmos_sensor(0x3047,0x04);
		write_cmos_sensor(0x3048,0x11);
		write_cmos_sensor(0x303D,0x08);
		write_cmos_sensor(0x304B,0x31);
		write_cmos_sensor(0x3063,0x00);
		write_cmos_sensor(0x303A,0x0B);
		write_cmos_sensor(0x302D,0x7F);
		write_cmos_sensor(0x3039,0x45);
		write_cmos_sensor(0x3038,0x10);
		write_cmos_sensor(0x3097,0x11);
		write_cmos_sensor(0x3096,0x01);
		write_cmos_sensor(0x3042,0x01);
		write_cmos_sensor(0x3053,0x01);
		write_cmos_sensor(0x320B,0x40);
		write_cmos_sensor(0x320C,0x06);
		write_cmos_sensor(0x320D,0xC0);
		write_cmos_sensor(0x3202,0x00);
		write_cmos_sensor(0x3203,0x3D);
		write_cmos_sensor(0x3204,0x00);
		write_cmos_sensor(0x3205,0x3D);
		write_cmos_sensor(0x3206,0x00);
		write_cmos_sensor(0x3207,0x3D);
		write_cmos_sensor(0x3208,0x00);
		write_cmos_sensor(0x3209,0x3D);
		write_cmos_sensor(0x3211,0x02);
		write_cmos_sensor(0x3212,0x21);
		write_cmos_sensor(0x3213,0x02);
		write_cmos_sensor(0x3214,0x21);
		write_cmos_sensor(0x3215,0x02);
		write_cmos_sensor(0x3216,0x21);
		write_cmos_sensor(0x3217,0x02);
		write_cmos_sensor(0x3218,0x21);
			write_cmos_sensor(0x0100,0x01);
		}
static void hs_video_setting() 
{              LOG_INF("E//VGA 120fps");
				////////////////////////////////////////////////////                                                  
			//  name:  S5K4H5YX EVT3 setfile                                                                      
			//  v0.00 Temporary version                                                                           
			//  Image size : Full size - 3280x2464, 30 fps    --> 4x4 binning 120fps                                                   
			//  EXTCLK : 24 MHz                                                                                   
			//  System PLL output : 560 MHz                                                                       
			//  Output PLL output : 700 Mbps                                                                      
			//  GCLK (ATOP) : 280 MHz                                                                             
			//  DCLK_4 (ATOP) : 70 MHz                                                                            
			////////////////////////////////////////////////////                                                  
			                                                                                                      
			//$MIPI[Width:820,Height:616,Format:Raw10,Lane:4,ErrorCheck:0,PolarityData:0,PolarityClock:0,Buffer:4]

			write_cmos_sensor(0x0100,0x00);       //Stream Off	[0]mode_select                                        
			write_cmos_sensor(0x0101,0x00);       //Image  orientation	image_orientation ([0] mirror en, [1] flip en)
			                       
			write_cmos_sensor(0x0204,0x00);       //Analog Gain	analogue_gain_code_global H                         
			write_cmos_sensor(0x0205,0x20);     	//analogue_gain_code_global L                                     
			                       
			write_cmos_sensor(0x0200,0x0C);       //Exposure Time	fine_integration_time H                           
			write_cmos_sensor(0x0201,0x98);     	//fine_integration_time L                                         
			write_cmos_sensor(0x0202,0x04);     	//coarse_integration_time H                                       
			write_cmos_sensor(0x0203,0xE2);     	//coarse_integration_time L                                       
			                  
			write_cmos_sensor(0x0340,0x02);       //Frame Rate 	frame_length_lines H                                
			write_cmos_sensor(0x0341,0x80);     	//frame_length_lines L                                            
			write_cmos_sensor(0x0342,0x0E);     	//line_length_pck H                                               
			write_cmos_sensor(0x0343,0x68);     	//line_length_pck L                                               
			                  
			write_cmos_sensor(0x0344,0x00);       //Image Size 	x_addr_start H                                      
			write_cmos_sensor(0x0345,0x00);     	//x_addr_start L                                                  
			write_cmos_sensor(0x0346,0x00);     	//y_addr_start H                                                  
			write_cmos_sensor(0x0347,0x00);     	//y_addr_start L                                                  
			write_cmos_sensor(0x0348,0x0C);     	//x_addr_end H                                                    
			write_cmos_sensor(0x0349,0xCF);     	//x_addr_end L                                                    
			write_cmos_sensor(0x034A,0x09);     	//y_addr_end H                                                    
			write_cmos_sensor(0x034B,0x9F);     	//y_addr_end L                                                    
			                  
			write_cmos_sensor(0x034C,0x03);     	//x_output_size H                                                 
			write_cmos_sensor(0x034D,0x34);     	//x_output_size L                                                 
			write_cmos_sensor(0x034E,0x02);     	//y_output_size H                                                 
			write_cmos_sensor(0x034F,0x68);     	//y_output_size L                                                 
			                  
			write_cmos_sensor(0x0390,0x01);       //Analog Binning 	binning_mode ([0] binning enable)               
			write_cmos_sensor(0x0391,0x44);     	//binning_type (22h : 2x2 binning, 44h : 4x4 binning)             
			write_cmos_sensor(0x0940,0x00);       //Digital Binning	digital_binning_mode (0:disable, 1:enable)      
			                  
			write_cmos_sensor(0x0381,0x01);       //Sub-Sampling Ratio 	x_even_inc                                  
			write_cmos_sensor(0x0383,0x07);     	//x_odd_inc                                                       
			write_cmos_sensor(0x0385,0x01);     	//y_even_inc                                                      
			write_cmos_sensor(0x0387,0x07);     	//y_odd_inc                                                       
			                  
			write_cmos_sensor(0x0114,0x03);       //MIPI Lains	Mipi lanes                                            
			                  
			write_cmos_sensor(0x0301,0x02);       //PLL control	vt_pix_clk_div                                      
			write_cmos_sensor(0x0303,0x01);     	//vt_sys_clk_div                                                  
			write_cmos_sensor(0x0305,0x06);     	//pre_pll_clk_div                                                 
			write_cmos_sensor(0x0306,0x00);     	//pll_multiplier H                                                
			write_cmos_sensor(0x0307,0x8C);     	//pll_multiplier L                                                
			write_cmos_sensor(0x0309,0x02);     	//op_pix_clk_div                                                  
			write_cmos_sensor(0x030B,0x01);     	//op_sys_clk_div                                                  
			write_cmos_sensor(0x3C59,0x00);     	//write_cmos_sensor_PLL_S                                                       
			write_cmos_sensor(0x030D,0x06);     	//out_pre_pll_clk_div                                             
			write_cmos_sensor(0x030E,0x00);     	//out_pll_multiplier H                                            
			write_cmos_sensor(0x030F,0xAF);     	//out_pll_multiplier L                                            
			write_cmos_sensor(0x3C5A,0x00);     	//write_cmos_sensor_out_PLL_S                                                   
			write_cmos_sensor(0x0310,0x01);     	//pll_mode (01h : 2-PLL, 00h : 1-PLL)                             
			write_cmos_sensor(0x3C50,0x53);     	//write_cmos_sensor_DIV_G /write_cmos_sensor_DIV_DBR                                          
			write_cmos_sensor(0x3C62,0x02);       //Bandwith control	requested_link_bit_rate_mbps HH[31:24]          
			write_cmos_sensor(0x3C63,0xBC);     	//requested_link_bit_rate_mbps HL[23:16]                          
			write_cmos_sensor(0x3C64,0x00);     	//requested_link_bit_rate_mbps LH[15:8]                           
			write_cmos_sensor(0x3C65,0x00);     	//requested_link_bit_rate_mbps LL[7:0]                            
			                  
			write_cmos_sensor(0x3C1E,0x0F);       //Embedded line on/off 	reg_fe_TN_SMIA_sync_sel[3]            
			    	//Reg_pat_TN0x_SMIA0x_sync_sel[2]                                     
			    	//reg_bpc_TN0x_SMIA0x_sync_sel[1]                                     
			    	//Reg_outif_0xTN_SM0xIA_sync_sel[0]                                   
			                  
			write_cmos_sensor(0x3500,0x0C);       //BPC on/off	[3] bpcms_rate_auto                                   
			                  
			write_cmos_sensor(0x3C1A,0xEC);       //write_cmos_sensor update sel	ECh : SMIA, A8h : TN                              
			                  
			write_cmos_sensor(0x3B29,0x01);       //OTP enable write_cmos_sensor.	                                                
			//write_cmos_sensor(0x/s33,0x00);00       //LSC enable write_cmos_sensor.	01 = LSC disable, 00 = LSC enable           
			                  
			                  
			//write_cmos_sensor(0x/ana,0xlo);g tuning
			write_cmos_sensor(0x3000,0x07);
			write_cmos_sensor(0x3001,0x05);
			write_cmos_sensor(0x3002,0x03);
			write_cmos_sensor(0x0200,0x0C);
			write_cmos_sensor(0x0201,0xB4);
			write_cmos_sensor(0x300A,0x03);
			write_cmos_sensor(0x300C,0x65);
			write_cmos_sensor(0x300D,0x54);
			write_cmos_sensor(0x3010,0x00);
			write_cmos_sensor(0x3012,0x14);
			write_cmos_sensor(0x3014,0x19);
			write_cmos_sensor(0x3017,0x0F);
			write_cmos_sensor(0x3018,0x1A);
			write_cmos_sensor(0x3019,0x6C);
			write_cmos_sensor(0x301A,0x78);
			write_cmos_sensor(0x306F,0x00);
			write_cmos_sensor(0x3070,0x00);
			write_cmos_sensor(0x3071,0x00);
			write_cmos_sensor(0x3072,0x00);
			write_cmos_sensor(0x3073,0x00);
			write_cmos_sensor(0x3074,0x00);
			write_cmos_sensor(0x3075,0x00);
			write_cmos_sensor(0x3076,0x0A);
			write_cmos_sensor(0x3077,0x03);
			write_cmos_sensor(0x3078,0x84);
			write_cmos_sensor(0x3079,0x00);
			write_cmos_sensor(0x307A,0x00);
			write_cmos_sensor(0x307B,0x00);
			write_cmos_sensor(0x307C,0x00);
			write_cmos_sensor(0x3085,0x00);
			write_cmos_sensor(0x3086,0x72);
			write_cmos_sensor(0x30A6,0x01);
			write_cmos_sensor(0x30A7,0x0E);
			write_cmos_sensor(0x3032,0x01);
			write_cmos_sensor(0x3037,0x02);
			write_cmos_sensor(0x304A,0x01);
			write_cmos_sensor(0x3054,0xF0);
			write_cmos_sensor(0x3044,0x20);
			write_cmos_sensor(0x3045,0x20);
			write_cmos_sensor(0x3047,0x04);
			write_cmos_sensor(0x3048,0x11);
			write_cmos_sensor(0x303D,0x08);
			write_cmos_sensor(0x304B,0x31);
			write_cmos_sensor(0x3063,0x00);
			write_cmos_sensor(0x303A,0x0B);
			write_cmos_sensor(0x302D,0x7F);
			write_cmos_sensor(0x3039,0x45);
			write_cmos_sensor(0x3038,0x10);
			write_cmos_sensor(0x3097,0x11);
			write_cmos_sensor(0x3096,0x01);
			write_cmos_sensor(0x3042,0x01);
			write_cmos_sensor(0x3053,0x01);
			write_cmos_sensor(0x320B,0x40);
			write_cmos_sensor(0x320C,0x06);
			write_cmos_sensor(0x320D,0xC0);
			write_cmos_sensor(0x3202,0x00);
			write_cmos_sensor(0x3203,0x3D);
			write_cmos_sensor(0x3204,0x00);
			write_cmos_sensor(0x3205,0x3D);
			write_cmos_sensor(0x3206,0x00);
			write_cmos_sensor(0x3207,0x3D);
			write_cmos_sensor(0x3208,0x00);
			write_cmos_sensor(0x3209,0x3D);
			write_cmos_sensor(0x3211,0x02);
			write_cmos_sensor(0x3212,0x21);
			write_cmos_sensor(0x3213,0x02);
			write_cmos_sensor(0x3214,0x21);
			write_cmos_sensor(0x3215,0x02);
			write_cmos_sensor(0x3216,0x21);
			write_cmos_sensor(0x3217,0x02);
			write_cmos_sensor(0x3218,0x21);
			                  
			write_cmos_sensor(0x0100,0x01);
	}

static void slim_video_setting()
	{
		LOG_INF("E");
		preview_setting();
		return;
		////////////////////////////////////////////////////
		//	name:  S5K4H5YX EVT3 setfile
		//	vR10a Temporary version
		//	Image size : Full size - 3280x2464, 30 fps
		//	EXTCLK : 24 MHz
		//	System PLL output : 560 MHz
		//	Output PLL output : 700 Mbps
		//	GCLK (ATOP) : 280 MHz
		//	DCLK_4 (ATOP) : 70 MHz
		////////////////////////////////////////////////////
		
		//$MIPI[Width:1280,Height:720,Format:Raw10,Lane:4,ErrorCheck:0,PolarityData:0,PolarityClock:0,Buffer:4]
		
		
		// Streaming off -----------------------------------
		write_cmos_sensor(0x0100,0x00); 	//	[0] mode_select
		
		//Image  Orientation -------------------------------
		write_cmos_sensor(0x0101,0x00); 	//	[1:0]	image_orientation ([0] mirror en, [1] flip en)
		
		//Analog Gain -------------------------------------
		write_cmos_sensor(0x0204,0x00); 	//	[15:8]	analogue_gain_code_global H
		write_cmos_sensor(0x0205,0x20); 	//	[7:0]	analogue_gain_code_global L
		
		//Exposure Time -----------------------------------
		write_cmos_sensor(0x0202,0x04); 	//	[15:8]	coarse_integration_time H
		write_cmos_sensor(0x0203,0xE2); 	//	[7:0]	coarse_integration_time L
		
		//Framee Rate --------------------------------------
		write_cmos_sensor(0x0340,0x09); 	//	[15:8]	frame_length_lines H
		write_cmos_sensor(0x0341,0xD0); 	//	[7:0]	frame_length_lines L
		write_cmos_sensor(0x0342,0x0E); 	//	[15:8]	line_length_pck H
		write_cmos_sensor(0x0343,0x68); 	//	[7:0]	line_length_pck L
		
		//Image Size --------------------------------------
		write_cmos_sensor(0x0344,0x01); 	//	[11:8]	x_addr_start H
		write_cmos_sensor(0x0345,0x68); 	//	[7:0]	x_addr_start L
		write_cmos_sensor(0x0346,0x02); 	//	[11:8]	y_addr_start H
		write_cmos_sensor(0x0347,0x00); 	//	[7:0]	y_addr_start L
		write_cmos_sensor(0x0348,0x0B); 	//	[11:8]	x_addr_end H
		write_cmos_sensor(0x0349,0x69); 	//	[7:0]	x_addr_end L
		write_cmos_sensor(0x034A,0x07); 	//	[11:8]	y_addr_end H
		write_cmos_sensor(0x034B,0x9F); 	//	[7:0]	y_addr_end L
		write_cmos_sensor(0x034C,0x05); 	//	[11:8]	x_output_size H
		write_cmos_sensor(0x034D,0x00); 	//	[7:0]	x_output_size L
		write_cmos_sensor(0x034E,0x02); 	//	[11:8]	y_output_size H
		write_cmos_sensor(0x034F,0xD0); 	//	[7:0]	y_output_size L
		
		//analog Binning ----------------------------------
		write_cmos_sensor(0x0390,0x01); 	//	[7:0]	binning_mode ([0] binning enable)
		write_cmos_sensor(0x0391,0x22); 	//	[7:0]	binning_type (22h : 2x2 binning, 44h : 4x4 binning)
		
		//Digital Binning ---------------------------------
		write_cmos_sensor(0x0940,0x00); 	//	[7:0]	digital_binning_mode (0:disable, 1:enable)
		
		//SubSampling Ratio ------------------------------
		write_cmos_sensor(0x0381,0x01); 	//	[4:0]	x_even_inc
		write_cmos_sensor(0x0383,0x03); 	//	[4:0]	x_odd_inc
		write_cmos_sensor(0x0385,0x01); 	//	[4:0]	y_even_inc
		write_cmos_sensor(0x0387,0x03); 	//	[4:0]	y_odd_inc
		
		//PLL control -------------------------------------
		write_cmos_sensor(0x0301,0x02); 	//	[3:0]	vt_pix_clk_div
		write_cmos_sensor(0x0303,0x01); 	//	[3:0]	vt_sys_clk_div
		write_cmos_sensor(0x0305,0x06); 	//	[5:0]	pre_pll_clk_div
		write_cmos_sensor(0x0306,0x00); 	//	[9:8]	pll_multiplier H
		write_cmos_sensor(0x0307,0x8C); 	//	[7:0]	pll_multiplier L
		write_cmos_sensor(0x0309,0x02); 	//	[3:0]	op_pix_clk_div
		write_cmos_sensor(0x030B,0x01); 	//	[3:0]	op_sys_clk_div
		write_cmos_sensor(0x3C59,0x00); 	//	[2:0]	reg_PLL_S
		write_cmos_sensor(0x030D,0x06); 	//	[5:0]	out_pre_pll_clk_div
		write_cmos_sensor(0x030E,0x00); 	//	[9:8]	out_pll_multiplier H
		write_cmos_sensor(0x030F,0xAF); 	//	[7:0]	out_pll_multiplier L
		write_cmos_sensor(0x3C5A,0x00); 	//	[2:0]	reg_out_PLL_S
		write_cmos_sensor(0x0310,0x01); 	//	[0] pll_mode (01h : 2-PLL, 00h : 1-PLL)
		write_cmos_sensor(0x3C50,0x53); 	//	[7:4]	reg_DIV_DBR
		//reg_DIV_G
		write_cmos_sensor(0x3C62,0x02); 	//	[31:24] requested_link_bit_rate_mbps HH
		write_cmos_sensor(0x3C63,0xBC); 	//	[23:16] requested_link_bit_rate_mbps HL
		write_cmos_sensor(0x3C64,0x00); 	//	[15:8]	requested_link_bit_rate_mbps LH
		write_cmos_sensor(0x3C65,0x00); 	//	[7:0]	requested_link_bit_rate_mbps LL
		
		//Embedded line on/off ----------------------------
		write_cmos_sensor(0x3C1E,0x0F); 	//	[3] reg_isp_fe_TN_SMIA_sync_sel
		//[);2]	reg_pat_TN_SMIA_sync_sel
		//[);1]	reg_bpc_TN_SMIA_sync_sel
		//[);0]	reg_outif_TN_SMIA_sync_sel
		
		
		//BPC on/off --------------------------------------
		write_cmos_sensor(0x3500,0x0C); 	//	[3] bpcms_rate_auto
		
		// Reg update sel ----------------------------------
		write_cmos_sensor(0x3C1A,0xEC); 	//	ECh : SMIA, A8h : TN
		
		//Analog Tuning for 3280x2464 30fps R10 131115
		write_cmos_sensor(0x3000,0x07);    // ct_ld_start
		write_cmos_sensor(0x3001,0x05);    // ct_sl_start
		write_cmos_sensor(0x3002,0x03);    // ct_rx_start
		write_cmos_sensor(0x0200,0x0c);    // (fine_integ_time) (MSB)
		write_cmos_sensor(0x0201,0xB4);    // (fine_integ_time) (LSB)
		write_cmos_sensor(0x300A,0x03);    // ct_cds_start
		write_cmos_sensor(0x300C,0x65);    // ct_s3_width
		write_cmos_sensor(0x300D,0x54);    // ct_s4_width
		write_cmos_sensor(0x3010,0x00);    // ct_pbr_width
		write_cmos_sensor(0x3012,0x14);    // ct_pbs_width
		write_cmos_sensor(0x3014,0x19);    // ct_pbr_ob_width
		write_cmos_sensor(0x3017,0x0F);    // ct_cds_lim_start
		write_cmos_sensor(0x3018,0x1A);    // ct_rmp_off_start
		write_cmos_sensor(0x3019,0x6C);    // ct_rmp_rst_start
		write_cmos_sensor(0x301A,0x78);    // ct_rmp_sig_start
		write_cmos_sensor(0x306F,0x00);    // ct_opt_l0_width1 (MSB)
		write_cmos_sensor(0x3070,0x00);    // ct_opt_l0_width1 (LSB)
		write_cmos_sensor(0x3071,0x00);    // ct_opt_l0_start2 (MSB)
		write_cmos_sensor(0x3072,0x00);    // ct_opt_l0_start2 (LSB)
		write_cmos_sensor(0x3073,0x00);    // ct_opt_l0_width2 (MSB)
		write_cmos_sensor(0x3074,0x00);    // ct_opt_l0_width2 (LSB)
		write_cmos_sensor(0x3075,0x00);    // ct_opt_l1_start1 (MSB)
		write_cmos_sensor(0x3076,0x0A);    // ct_opt_l1_start1 (LSB)
		write_cmos_sensor(0x3077,0x03);    // ct_opt_l1_width1 (MSB)
		write_cmos_sensor(0x3078,0x84);    // ct_opt_l1_width1 (LSB)
		write_cmos_sensor(0x3079,0x00);    // ct_opt_l1_start2 (MSB)
		write_cmos_sensor(0x307A,0x00);    // ct_opt_l1_start2 (LSB)
		write_cmos_sensor(0x307B,0x00);    // ct_opt_l1_width2 (MSB)
		write_cmos_sensor(0x307C,0x00);    // ct_opt_l1_width2 (LSB)
		write_cmos_sensor(0x3085,0x00);    // ct_opt_h0_start1 (MSB)
		write_cmos_sensor(0x3086,0x72);    // ct_opt_h0_start1 (LSB)
		write_cmos_sensor(0x30A6,0x01);    // cds_option 2
		write_cmos_sensor(0x30A7,0x0E);    // cds_option 1
		write_cmos_sensor(0x3032,0x01);    // rmp_option
		write_cmos_sensor(0x3037,0x02);    // dbs_option
		write_cmos_sensor(0x304A,0x01);    // dbr_option
		write_cmos_sensor(0x3054,0xF0);    // rdv_option
		write_cmos_sensor(0x3044,0x20);    // dbr_tune_tg
		write_cmos_sensor(0x3045,0x20);    // dbr_tune_rgsl
		write_cmos_sensor(0x3047,0x04);    // dbr_tune_ntg
		write_cmos_sensor(0x3048,0x11);    // dbr_tune_rd
		write_cmos_sensor(0x303D,0x08);    // off_rst
		write_cmos_sensor(0x304B,0x31);    // adc_sat (530mV)
		write_cmos_sensor(0x3063,0x00);    // ldb_ctrl
		write_cmos_sensor(0x303A,0x0B);    // clp_lvl
		write_cmos_sensor(0x302D,0x7F);    // (aig_main2)
		write_cmos_sensor(0x3039,0x45);    // pix/pxbst_bias
		write_cmos_sensor(0x3038,0x10);    // comp1/2_bias comp1_bias @AG<=x16
		write_cmos_sensor(0x3097,0x11);    // comp1_bias @AG<x8, @AG<x4, respectively
		write_cmos_sensor(0x3096,0x01);    // comp1_bias @AG<x2
		write_cmos_sensor(0x3042,0x01);    // # of L-OB
		write_cmos_sensor(0x3053,0x01);    // lp_vblk_en								  
		write_cmos_sensor(0x320B,0x40);    // adc_default (LSB) 						   
		write_cmos_sensor(0x320C,0x06);    // adc_max (MSB) 							   
		write_cmos_sensor(0x320D,0xC0);    // adc_max (LSB) 							   
		write_cmos_sensor(0x3202,0x00);    // adc_offset_even0 (MSB)					   
		write_cmos_sensor(0x3203,0x3D);    // adc_offset_even0 (LSB)					   
		write_cmos_sensor(0x3204,0x00);    // adc_offset_odd0 (MSB) 					   
		write_cmos_sensor(0x3205,0x3D);    // adc_offset_odd0 (LSB) 					   
		write_cmos_sensor(0x3206,0x00);    // adc_offset_even1 (MSB)					   
		write_cmos_sensor(0x3207,0x3D);    // adc_offset_even1 (LSB)					   
		write_cmos_sensor(0x3208,0x00);    // adc_offset_odd1 (MSB) 					   
		write_cmos_sensor(0x3209,0x3D);    // adc_offset_odd1 (LSB) 					   
		write_cmos_sensor(0x3211,0x02);    // adc_offset_ms_even0 (MSB) 				   
		write_cmos_sensor(0x3212,0x21);    // adc_offset_ms_even0 (LSB) 				   
		write_cmos_sensor(0x3213,0x02);    // adc_offset_ms_odd0 (MSB)					   
		write_cmos_sensor(0x3214,0x21);    // adc_offset_ms_odd0 (LSB)					   
		write_cmos_sensor(0x3215,0x02);    // adc_offset_ms_even1 (MSB) 				   
		write_cmos_sensor(0x3216,0x21);    // adc_offset_ms_even1 (LSB) 				   
		write_cmos_sensor(0x3217,0x02);    // adc_offset_ms_odd1 (MSB)					   
		write_cmos_sensor(0x3218,0x21);    // adc_offset_ms_odd1 (LSB)
		write_cmos_sensor(0x3B29,0x01); 	 // OTP enable
		
		
		//Straming On -----------------------------------
		write_cmos_sensor(0x0100,0x01);    // Streaming On
		
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
    int  retry = 1;
	//sensor have two i2c address 0x6c 0x6d & 0x21 0x20, we should detect the module used i2c address
	//while (imgsensor_info.i2c_addr_table[i] != 0xff) {
		spin_lock(&imgsensor_drv_lock);
		imgsensor.i2c_write_id = 0x20;//imgsensor_info.i2c_addr_table[i];
		spin_unlock(&imgsensor_drv_lock);
		do {
			*sensor_id = ((read_cmos_sensor(0x0000) << 8) | read_cmos_sensor(0x0001));
			if (*sensor_id == imgsensor_info.sensor_id) {				
				LOG_INF("i2c write id: 0x%x, sensor id: 0x%x\n", imgsensor.i2c_write_id,*sensor_id);	  
				break;
			}	
			LOG_INF("Read sensor id fail, id: 0x%x,0x%x\n", imgsensor.i2c_write_id,*sensor_id);
			retry--;
		} while(retry > 0);
		//i++;
		//retry = 2;
//	}
	if (*sensor_id != imgsensor_info.sensor_id) {
		// if Sensor ID is not correct, Must set *sensor_id to 0xFFFFFFFF 
		*sensor_id = imgsensor_info.sensor_id;
		return ERROR_NONE;
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
	//const kal_uint8 i2c_addr[] = {IMGSENSOR_WRITE_ID_1, IMGSENSOR_WRITE_ID_2};
	kal_uint8 i = 0;
	kal_uint8 retry = 1;
	kal_uint16 sensor_id = 0; 
	LOG_INF("PLATFORM:MT6595,MIPI 2LANE\n");
	LOG_INF("preview 1280*960@30fps,864Mbps/lane; video 1280*960@30fps,864Mbps/lane; capture 5M@30fps,864Mbps/lane\n");
	
	//sensor have two i2c address 0x6c 0x6d & 0x21 0x20, we should detect the module used i2c address
	//while (imgsensor_info.i2c_addr_table[i] != 0xff) {
		spin_lock(&imgsensor_drv_lock);
		imgsensor.i2c_write_id = 0x20;//imgsensor_info.i2c_addr_table[i];
		spin_unlock(&imgsensor_drv_lock);
		do {
			sensor_id = ((read_cmos_sensor(0x0000) << 8) | read_cmos_sensor(0x0001));
			if (sensor_id == imgsensor_info.sensor_id) {				
				LOG_INF("i2c write id: 0x%x, sensor id: 0x%x,0x%x\n", imgsensor.i2c_write_id,sensor_id);	  
				break;
			}	
			LOG_INF("Read sensor id fail, id: 0x%x,0x%x\n", imgsensor.i2c_write_id,sensor_id);
			retry--;
		} while(retry > 0);
		//i++;
		//if (sensor_id == imgsensor_info.sensor_id)
		//	break;
		//retry = 2;
	//}		 
	if (imgsensor_info.sensor_id != sensor_id)
		return ERROR_SENSOR_CONNECT_FAIL;

	


	
	/* initail sequence write in  */
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
	//imgsensor.video_mode = KAL_FALSE;
	imgsensor.line_length = imgsensor_info.pre.linelength;
	imgsensor.frame_length = imgsensor_info.pre.framelength; 
	imgsensor.min_frame_length = imgsensor_info.pre.framelength;
	imgsensor.current_fps = imgsensor.current_fps;
	//imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	preview_setting();
	return ERROR_NONE;
}   /*  S5K4H5YCMIPIPreview   */

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
	if (imgsensor.current_fps == imgsensor_info.cap1.max_framerate) {//PIP capture: 24fps for less than 13M, 20fps for 16M,15fps for 20M
		imgsensor.pclk = imgsensor_info.cap1.pclk;
		imgsensor.line_length = imgsensor_info.cap1.linelength;
		imgsensor.frame_length = imgsensor_info.cap1.framelength;  
		imgsensor.min_frame_length = imgsensor_info.cap1.framelength;
		//imgsensor.autoflicker_en = KAL_FALSE;
	} else {
		if (imgsensor.current_fps != imgsensor_info.cap.max_framerate)
			LOG_INF("Warning: current_fps %d fps is not support, so use cap1's setting: %d fps!\n",imgsensor_info.cap1.max_framerate/10);
		imgsensor.pclk = imgsensor_info.cap.pclk;
		imgsensor.line_length = imgsensor_info.cap.linelength;
		imgsensor.frame_length = imgsensor_info.cap.framelength;  
		imgsensor.min_frame_length = imgsensor_info.cap.framelength;
		//imgsensor.autoflicker_en = KAL_FALSE;
	}

	spin_unlock(&imgsensor_drv_lock);

	capture_setting(imgsensor.current_fps); 
	
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
	//imgsensor.current_fps = 300;
	//imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	normal_video_setting(imgsensor.current_fps);
	return ERROR_NONE;
}   /*  S5K4H5YCMIPIPreview   */

static kal_uint32 hs_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
					  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("E\n");
	
	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_HIGH_SPEED_VIDEO;
	imgsensor.pclk = imgsensor_info.hs_video.pclk;
	//imgsensor.video_mode = KAL_TRUE;
	imgsensor.line_length = imgsensor_info.hs_video.linelength;
	imgsensor.frame_length = imgsensor_info.hs_video.framelength; 
	imgsensor.min_frame_length = imgsensor_info.hs_video.framelength;
	imgsensor.dummy_line = 0;
	imgsensor.dummy_pixel = 0;
	//imgsensor.current_fps = 300;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	hs_video_setting();
	return ERROR_NONE;
}   /*  S5K4H5YCMIPIPreview   */

static kal_uint32 slim_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
					  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("E\n");
	
	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_SLIM_VIDEO;
	imgsensor.pclk = imgsensor_info.slim_video.pclk;
	//imgsensor.video_mode = KAL_TRUE;
	imgsensor.line_length = imgsensor_info.slim_video.linelength;
	imgsensor.frame_length = imgsensor_info.slim_video.framelength; 
	imgsensor.min_frame_length = imgsensor_info.slim_video.framelength;
	imgsensor.dummy_line = 0;
	imgsensor.dummy_pixel = 0;
	//imgsensor.current_fps = 300;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	slim_video_setting();
	return ERROR_NONE;
}   /*  S5K4H5YCMIPIPreview   */



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

static kal_uint32 get_info(MSDK_SCENARIO_ID_ENUM scenario_id,
					  MSDK_SENSOR_INFO_STRUCT *sensor_info,
					  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("scenario_id = %d\n", scenario_id);

	
	//sensor_info->SensorVideoFrameRate = imgsensor_info.normal_video.max_framerate/10; /* not use */
	//sensor_info->SensorStillCaptureFrameRate= imgsensor_info.cap.max_framerate/10; /* not use */
	//imgsensor_info->SensorWebCamCaptureFrameRate= imgsensor_info.v.max_framerate; /* not use */

	sensor_info->SensorClockPolarity = SENSOR_CLOCK_POLARITY_LOW;
	sensor_info->SensorClockFallingPolarity = SENSOR_CLOCK_POLARITY_LOW; /* not use */
	sensor_info->SensorHsyncPolarity = SENSOR_CLOCK_POLARITY_LOW; // inverse with datasheet
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
	sensor_info->SensorHightSampling = 0;	// 0 is default 1x 
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


static kal_uint32 control(MSDK_SCENARIO_ID_ENUM scenario_id, MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
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
		// SetVideoMode Function should fix framerate
		if (framerate == 0)
			// Dynamic frame rate
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
	if (enable) //enable auto flicker	  
		imgsensor.autoflicker_en = KAL_TRUE;
	else //Cancel Auto flick
		imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	return ERROR_NONE;
}


static kal_uint32 set_max_framerate_by_scenario(MSDK_SCENARIO_ID_ENUM scenario_id, MUINT32 framerate) 
{
	kal_int16 dummyLine;
	kal_uint32 lineLength,frameHeight;
  
	LOG_INF("scenario_id = %d, framerate = %d\n", scenario_id, framerate);
				if(framerate == 0)
				return ERROR_NONE;
	switch (scenario_id) {
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
			frameHeight = imgsensor_info.pre.pclk / framerate * 10 / imgsensor_info.pre.linelength;
			spin_lock(&imgsensor_drv_lock);
			imgsensor.dummy_line = frameHeight - imgsensor_info.pre.framelength;
			if (imgsensor.dummy_line < 0)
				imgsensor.dummy_line = 0;
			imgsensor.frame_length =imgsensor_info.pre.framelength + imgsensor.dummy_line;
			imgsensor.min_frame_length = imgsensor.frame_length;
			spin_unlock(&imgsensor_drv_lock);
			//set_dummy();			
			break;			
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			if(framerate == 0)
				return ERROR_NONE;
			frameHeight = imgsensor_info.normal_video.pclk / framerate * 10 / imgsensor_info.normal_video.linelength;
			spin_lock(&imgsensor_drv_lock);
			imgsensor.dummy_line = frameHeight - imgsensor_info.normal_video.framelength;
			if (imgsensor.dummy_line < 0)
				imgsensor.dummy_line = 0;			
			imgsensor.frame_length = imgsensor_info.normal_video.framelength + imgsensor.dummy_line;
			imgsensor.min_frame_length = imgsensor.frame_length;
			spin_unlock(&imgsensor_drv_lock);

			//set_dummy();			
			break;
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		//case MSDK_SCENARIO_ID_CAMERA_ZSD:			
			frameHeight = imgsensor_info.cap.pclk / framerate * 10 / imgsensor_info.cap.linelength;
			spin_lock(&imgsensor_drv_lock);
			imgsensor.dummy_line = frameHeight - imgsensor_info.cap.framelength;
			if (imgsensor.dummy_line < 0)
				imgsensor.dummy_line = 0;
			imgsensor.frame_length =imgsensor_info.cap.framelength + imgsensor.dummy_line;
			imgsensor.min_frame_length = imgsensor.frame_length;
			spin_unlock(&imgsensor_drv_lock);
			//set_dummy();			
			break;	
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
			frameHeight = imgsensor_info.hs_video.pclk / framerate * 10 / imgsensor_info.hs_video.linelength;
			spin_lock(&imgsensor_drv_lock);
			imgsensor.dummy_line = frameHeight - imgsensor_info.hs_video.framelength;
			if (imgsensor.dummy_line < 0)
				imgsensor.dummy_line = 0;
			imgsensor.min_frame_length = imgsensor.frame_length;
			spin_unlock(&imgsensor_drv_lock);
			//set_dummy();			
			break;
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
			frameHeight = imgsensor_info.slim_video.pclk / framerate * 10 / imgsensor_info.slim_video.linelength;
			spin_lock(&imgsensor_drv_lock);
			imgsensor.dummy_line = frameHeight - imgsensor_info.slim_video.framelength;
			if (imgsensor.dummy_line < 0)
				imgsensor.dummy_line = 0;			
			imgsensor.frame_length =imgsensor_info.hs_video.framelength + imgsensor.dummy_line;
			imgsensor.min_frame_length = imgsensor.frame_length;
			spin_unlock(&imgsensor_drv_lock);
			//set_dummy();			
		default:  //coding with  preview scenario by default
			frameHeight = imgsensor_info.pre.pclk / framerate * 10 / imgsensor_info.pre.linelength;
			spin_lock(&imgsensor_drv_lock);
			imgsensor.dummy_line = frameHeight - imgsensor_info.pre.framelength;
			if (imgsensor.dummy_line < 0)
				imgsensor.dummy_line = 0;
			imgsensor.frame_length =imgsensor_info.pre.framelength + imgsensor.dummy_line;
			imgsensor.min_frame_length = imgsensor.frame_length;
			spin_unlock(&imgsensor_drv_lock);
			//set_dummy();	
			LOG_INF("error scenario_id = %d, we use preview scenario \n", scenario_id);
			break;
	}	
	return ERROR_NONE;
}


static kal_uint32 get_default_framerate_by_scenario(MSDK_SCENARIO_ID_ENUM scenario_id, MUINT32 *framerate) 
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

	if(enable) 	 
		write_cmos_sensor(0x0601, 0x02);
	else		  
		write_cmos_sensor(0x0601, 0x00);  

	spin_lock(&imgsensor_drv_lock);
	imgsensor.test_pattern = enable;
	spin_unlock(&imgsensor_drv_lock);
	return ERROR_NONE;
}

static kal_uint32 feature_control(MSDK_SENSOR_FEATURE_ENUM feature_id,
							 UINT8 *feature_para,UINT32 *feature_para_len)
{
	UINT16 *feature_return_para_16=(UINT16 *) feature_para;
	UINT16 *feature_data_16=(UINT16 *) feature_para;
	UINT32 *feature_return_para_32=(UINT32 *) feature_para;
	UINT32 *feature_data_32=(UINT32 *) feature_para;
	unsigned long long *feature_data=(unsigned long long *) feature_para;
	unsigned long long *feature_return_para=(unsigned long long *) feature_para;
	SENSOR_WINSIZE_INFO_STRUCT *wininfo;	
	MSDK_SENSOR_REG_INFO_STRUCT *sensor_reg_data=(MSDK_SENSOR_REG_INFO_STRUCT *) feature_para;
 
	//LOG_INF("feature_id = %d\n", feature_id);
	switch (feature_id) {
		case SENSOR_FEATURE_GET_PERIOD:
			*feature_return_para_16++ = imgsensor.line_length;
			*feature_return_para_16 = imgsensor.frame_length;
			*feature_para_len=4;
			break;
		case SENSOR_FEATURE_GET_PIXEL_CLOCK_FREQ:	 
			*feature_return_para_32 = imgsensor.pclk;
			*feature_para_len=4;
			break;		   
		case SENSOR_FEATURE_SET_ESHUTTER:
			set_shutter(*feature_data);
			break;
		case SENSOR_FEATURE_SET_NIGHTMODE:
			night_mode((BOOL) *feature_data);
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
			// get the lens driver ID from EEPROM or just return LENS_DRIVER_ID_DO_NOT_CARE
			// if EEPROM does not exist in camera module.
			*feature_return_para_32=LENS_DRIVER_ID_DO_NOT_CARE;
			*feature_para_len=4;
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
            set_max_framerate_by_scenario((MSDK_SCENARIO_ID_ENUM)*feature_data, *(feature_data+1));
			break;
		case SENSOR_FEATURE_GET_DEFAULT_FRAME_RATE_BY_SCENARIO:
			get_default_framerate_by_scenario((MSDK_SCENARIO_ID_ENUM)*(feature_data), (MUINT32 *)(uintptr_t)(*(feature_data+1)));
			//get_default_framerate_by_scenario((MSDK_SCENARIO_ID_ENUM)*feature_data_32, (MUINT32 *)(*(feature_data_32+1)));
			break;
		case SENSOR_FEATURE_SET_TEST_PATTERN:
			set_test_pattern_mode((BOOL)*feature_data);
			break;
		case SENSOR_FEATURE_GET_TEST_PATTERN_CHECKSUM_VALUE: //for factory mode auto testing			 
			*feature_return_para_32 = imgsensor_info.checksum_value;
			*feature_para_len=4;							 
			break;				
		case SENSOR_FEATURE_SET_FRAMERATE:
            LOG_INF("current fps :%d\n", (UINT32)*feature_data);
			spin_lock(&imgsensor_drv_lock);
			imgsensor.current_fps = *feature_data;
			spin_unlock(&imgsensor_drv_lock);		
			break;
//		case SENSOR_FEATURE_SET_HDR:
	//		LOG_INF("ihdr enable :%d\n", *feature_data_16);
	//		spin_lock(&imgsensor_drv_lock);
//			imgsensor.ihdr_en = *feature_data_16;
	//		spin_unlock(&imgsensor_drv_lock);		
//			break;
		case SENSOR_FEATURE_GET_CROP_INFO:
			//LOG_INF("SENSOR_FEATURE_GET_CROP_INFO scenarioId:%d\n", *feature_data_32);
			//wininfo = (SENSOR_WINSIZE_INFO_STRUCT *)(*(feature_data_32+1));
			wininfo = (SENSOR_WINSIZE_INFO_STRUCT *)(uintptr_t)(*(feature_data+1));
			switch (*feature_data_32) {
				case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
					memcpy((void *)wininfo,(void *)&imgsensor_winsize_info[1],sizeof(SENSOR_WINSIZE_INFO_STRUCT));
					break;	  
				case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
					memcpy((void *)wininfo,(void *)&imgsensor_winsize_info[2],sizeof(SENSOR_WINSIZE_INFO_STRUCT));
					break;
				case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
					memcpy((void *)wininfo,(void *)&imgsensor_winsize_info[3],sizeof(SENSOR_WINSIZE_INFO_STRUCT));
					break;
				case MSDK_SCENARIO_ID_SLIM_VIDEO:
					memcpy((void *)wininfo,(void *)&imgsensor_winsize_info[4],sizeof(SENSOR_WINSIZE_INFO_STRUCT));
					break;
				case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
				default:
					memcpy((void *)wininfo,(void *)&imgsensor_winsize_info[0],sizeof(SENSOR_WINSIZE_INFO_STRUCT));
					break;
			}
		case SENSOR_FEATURE_SET_IHDR_SHUTTER_GAIN:
            LOG_INF("SENSOR_SET_SENSOR_IHDR LE=%d, SE=%d, Gain=%d\n",(UINT16)*feature_data,(UINT16)*(feature_data+1),(UINT16)*(feature_data+2));
            ihdr_write_shutter_gain((UINT16)*feature_data,(UINT16)*(feature_data+1),(UINT16)*(feature_data+2));
			break;
		default:
			break;
	}
  
	return ERROR_NONE;
}	/*	feature_control()  */

static SENSOR_FUNCTION_STRUCT sensor_func = {
	open,
	get_info,
	get_resolution,
	feature_control,
	control,
	close
};

UINT32 S5K4H5YC_MIPI_RAW_SensorInit(PSENSOR_FUNCTION_STRUCT *pfFunc)
{
	/* To Do : Check Sensor status here */
	if (pfFunc!=NULL)
		*pfFunc=&sensor_func;
	return ERROR_NONE;
}	/*	S5K4H5YC_MIPI_RAW_SensorInit	*/
