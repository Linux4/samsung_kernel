/*****************************************************************************
 *
 * Filename:
 * ---------
 *     C2515mipi_Sensor.c
 *
 * Project:
 * --------
 *     ALPS
 *
 * Description:
 * ------------
 *     Source code of Sensor driver
 *
 *
 *------------------------------------------------------------------------------
 * Upper this line, this part is controlled by CC/CQ. DO NOT MODIFY!!
 *============================================================================
 ****************************************************************************/
#if 0
#include <linux/videodev2.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <asm/atomic.h>
//#include <linux/xlog.h>
// #include "kd_camera_typedef.h"
#include "kd_camera_hw.h"
#include "kd_imgsensor.h"
#include "kd_imgsensor_define.h"
#include "kd_imgsensor_errcode.h"

//#include "c2515mipi_Sensor.h"
#include <mach/mt_pm_ldo.h>
#endif

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

#include "n26_c2515_dep_cxt_mipi_mono_Sensor.h"



/****************************Modify Following Strings for Debug****************************/
#define PFX "C2515_SensroDriver"
#define LOG_1 LOG_INF("C2515, MIPI 1LANE\n")

//#define C2515MIPIRAW_DEBUG

#define C2515MIPISENSORDB(x,...)


#define LOG_INF(format, args...)    pr_info(PFX "[%s] " format, __func__, ##args)

//+bug604664 liudijin.wt, add, 2021/04/16, distinguish depth camera params for ov64b40 dualcam
//extern kal_uint32 main_sensor_id;
//-bug604664 liudijin.wt, add, 2021/04/16, distinguish depth camera params for ov64b40 dualcam
#if 0
#define EEPROM_BL24SA64E_ID 0xA2

static kal_uint16 read_eeprom(kal_uint32 addr)
{
	kal_uint16 get_byte=0;

	char pu_send_cmd[2] = { (char)(addr >> 8), (char)(addr & 0xFF) };
	iReadRegI2C(pu_send_cmd, 2, (u8*)&get_byte, 1, EEPROM_BL24SA64E_ID);

	return get_byte;
}
#endif


static DEFINE_SPINLOCK(imgsensor_drv_lock);


static struct imgsensor_info_struct imgsensor_info = {
    .sensor_id = N26_C2515_DEP_CXT_SENSOR_ID,

    .checksum_value = 0xf7375923,        //checksum value for Camera Auto Test

    .pre = {
        .pclk = 90000000,            //record different mode's pclk//84
        .linelength = 2432,            //record different mode's linelength
        .framelength = 1232,            //record different mode's framelength
        .startx = 0,                    //record different mode's startx of grabwindow
        .starty = 0,                    //record different mode's starty of grabwindow
        .grabwindow_width = 1600,        //record different mode's width of grabwindow
        .grabwindow_height = 1200,        //record different mode's height of grabwindow
        /*     following for MIPIDataLowPwr2HighSpeedSettleDelayCount by different scenario    */
        .mipi_data_lp2hs_settle_dc = 85,//unit , ns //85
        /*     following for GetDefaultFramerateByScenario()    */
        .max_framerate = 300,
        .mipi_pixel_rate = 72000000,
    },
    .cap = {
        .pclk = 90000000,            //record different mode's pclk//84
        .linelength = 2432,            //record different mode's linelength
        .framelength = 1232,            //record different mode's framelength
        .startx = 0,                    //record different mode's startx of grabwindow
        .starty = 0,                    //record different mode's starty of grabwindow
        .grabwindow_width = 1600,        //record different mode's width of grabwindow
        .grabwindow_height = 1200,        //record different mode's height of grabwindow
        /*     following for MIPIDataLowPwr2HighSpeedSettleDelayCount by different scenario    */
        .mipi_data_lp2hs_settle_dc = 85,//unit , ns //85
        /*     following for GetDefaultFramerateByScenario()    */
        .max_framerate = 300,
        .mipi_pixel_rate = 72000000,
    },
    .cap1 = {
        .pclk = 90000000,            //record different mode's pclk//84
        .linelength = 2432,            //record different mode's linelength
        .framelength = 1232,            //record different mode's framelength
        .startx = 0,                    //record different mode's startx of grabwindow
        .starty = 0,                    //record different mode's starty of grabwindow
        .grabwindow_width = 1600,        //record different mode's width of grabwindow
        .grabwindow_height = 1200,        //record different mode's height of grabwindow
        /*     following for MIPIDataLowPwr2HighSpeedSettleDelayCount by different scenario    */
        .mipi_data_lp2hs_settle_dc = 85,//unit , ns //85
        /*     following for GetDefaultFramerateByScenario()    */
        .max_framerate = 300,
        .mipi_pixel_rate = 72000000,
    },
    .normal_video = {
        .pclk = 90000000,            //record different mode's pclk//84
        .linelength = 2432,            //record different mode's linelength
        .framelength = 1232,            //record different mode's framelength
        .startx = 0,                    //record different mode's startx of grabwindow
        .starty = 0,                    //record different mode's starty of grabwindow
        .grabwindow_width = 1600,        //record different mode's width of grabwindow
        .grabwindow_height = 1200,        //record different mode's height of grabwindow
        /*     following for MIPIDataLowPwr2HighSpeedSettleDelayCount by different scenario    */
        .mipi_data_lp2hs_settle_dc = 85,//unit , ns //85
        /*     following for GetDefaultFramerateByScenario()    */
        .max_framerate = 300,
        .mipi_pixel_rate = 72000000,
    },
    .hs_video = {
        .pclk = 90000000,            //record different mode's pclk//84
        .linelength = 2432,            //record different mode's linelength
        .framelength = 1232,            //record different mode's framelength
        .startx = 0,                    //record different mode's startx of grabwindow
        .starty = 0,                    //record different mode's starty of grabwindow
        .grabwindow_width = 1600,        //record different mode's width of grabwindow
        .grabwindow_height = 1200,        //record different mode's height of grabwindow
        /*     following for MIPIDataLowPwr2HighSpeedSettleDelayCount by different scenario    */
        .mipi_data_lp2hs_settle_dc = 85,//unit , ns //85
        /*     following for GetDefaultFramerateByScenario()    */
        .max_framerate = 300,
        .mipi_pixel_rate = 72000000,
    },
    .slim_video = {
        .pclk = 90000000,            //record different mode's pclk//84
        .linelength = 2432,            //record different mode's linelength
        .framelength = 1232,            //record different mode's framelength
        .startx = 0,                    //record different mode's startx of grabwindow
        .starty = 0,                    //record different mode's starty of grabwindow
        .grabwindow_width = 1600,        //record different mode's width of grabwindow
        .grabwindow_height = 1200,        //record different mode's height of grabwindow
        /*     following for MIPIDataLowPwr2HighSpeedSettleDelayCount by different scenario    */
        .mipi_data_lp2hs_settle_dc = 85,//unit , ns //85
        /*     following for GetDefaultFramerateByScenario()    */
        .max_framerate = 300,
        .mipi_pixel_rate = 72000000,
    },
    //+bug 663265 zhoumin.wt, add, 2021/09/17, modify framerate of dualcam to be 24fps
    .custom1 = {
        .pclk = 90000000,            //record different mode's pclk//84
        .linelength = 2432,            //record different mode's linelength
        .framelength = 1540,            //record different mode's framelength
        .startx = 0,                    //record different mode's startx of grabwindow
        .starty = 0,                    //record different mode's starty of grabwindow
        .grabwindow_width = 1600,        //record different mode's width of grabwindow
        .grabwindow_height = 1200,        //record different mode's height of grabwindow
        /*     following for MIPIDataLowPwr2HighSpeedSettleDelayCount by different scenario    */
        .mipi_data_lp2hs_settle_dc = 85,//unit , ns //85
        /*     following for GetDefaultFramerateByScenario()    */
        .max_framerate = 240,
        .mipi_pixel_rate = 72000000,
    },
    //-bug 663265 zhoumin.wt, add, 2021/09/17, modify framerate of dualcam to be 24fps
    .margin = 4,            //sensor framelength & shutter margin
    //bug626603 liudijin, add, 2021/03/26,fix ITS:test_black_white fail
    .min_shutter = 2,        //min shutter
    .max_frame_length = 0x7fff,//max framelength by sensor register's limitation
    .ae_shut_delay_frame = 0,    //shutter delay frame for AE cycle, 2 frame with ispGain_delay-shut_delay=2-0=2
    .ae_sensor_gain_delay_frame = 1,//sensor gain delay frame for AE cycle,2 frame with ispGain_delay-sensor_gain_delay=2-0=2
    .ae_ispGain_delay_frame = 2,//isp gain delay frame for AE cycle
	//+bug604664 chenbocheng.wt, add, 2020/01/06, modify code for dualcam sync issue
    .frame_time_delay_frame = 1,//The delay frame of setting frame length
    .ihdr_support = 0,      //1, support; 0,not support
    .ihdr_le_firstline = 0,  //1,le first ; 0, se first
    //bug 663265 zhoumin.wt, add, 2021/09/17, modify framerate of dualcam to be 24fps
    .sensor_mode_num = 7,      //support sensor mode num

    .cap_delay_frame = 2,
    .pre_delay_frame = 2,
    .video_delay_frame = 2,
    .hs_video_delay_frame = 2,
    .slim_video_delay_frame = 2,
    //bug 663265 zhoumin.wt, add, 2021/09/17, modify framerate of dualcam to be 24fps
    .custom1_delay_frame = 2,

    .min_gain = 64, /*1x gain*/
    .max_gain = 1024, /*16x gain*/
    .min_gain_iso = 100,
    .gain_step = 1,
    .gain_type = 0,
    .i2c_speed = 400,

    .isp_driving_current = ISP_DRIVING_4MA, //+bug 720367, wangmingli.wt,MODIFY,2022/6/30, optimize desense of depth camera.
    .sensor_interface_type = SENSOR_INTERFACE_TYPE_MIPI,//sensor_interface_type
    .mipi_sensor_type = MIPI_OPHY_NCSI2, //0,MIPI_OPHY_NCSI2;  1,MIPI_OPHY_CSI2
    .mipi_settle_delay_mode = 0,//0,MIPI_SETTLEDELAY_AUTO; 1,MIPI_SETTLEDELAY_MANNUAL
    .sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_MONO,//sensor output first pixel color  //+bug584789,zhanghengyuan.wt,MODIFY,2021/12/,fix depth c2515 output_dataformat
    .mclk = 24,//mclk value, suggest 24 or 26 for 24Mhz or 26Mhz
    .mipi_lane_num = SENSOR_MIPI_1_LANE,//mipi lane num
    .i2c_addr_table = {0x6c, 0xff},//record sensor support all write id addr, only supprt 4must end with 0xff
};


static struct imgsensor_struct imgsensor = {
    .mirror = IMAGE_H_MIRROR,                //mirrorflip information
    .sensor_mode = IMGSENSOR_MODE_INIT, //IMGSENSOR_MODE enum value,record current sensor mode,such as: INIT, Preview, Capture, Video,High Speed Video, Slim Video
    .shutter = 0x3D0,                    //current shutter
    .gain = 0x100,                        //current gain
    .dummy_pixel = 0,                    //current dummypixel
    .dummy_line = 0,                    //current dummyline
    .current_fps = 300,  //full size current fps : 24fps for PIP, 30fps for Normal or ZSD
    .autoflicker_en = KAL_FALSE,  //auto flicker enable: KAL_FALSE for disable auto flicker, KAL_TRUE for enable auto flicker
    .test_pattern = KAL_FALSE,        //test pattern mode or not. KAL_FALSE for in test pattern mode, KAL_TRUE for normal output
    .current_scenario_id = MSDK_SCENARIO_ID_CAMERA_PREVIEW,//current scenario id
    .ihdr_en = 0, //sensor need support LE, SE with HDR feature
    .i2c_write_id = 0x6c,//record current sensor's i2c write id
};

//+bug 663265 zhoumin.wt, modify, 2021/09/17, modify framerate of dualcam to be 24fps
/* Sensor output window information */
static  struct SENSOR_WINSIZE_INFO_STRUCT imgsensor_winsize_info[6]=
{{ 1600, 1200,      0,    0, 1600, 1200, 1600, 1200,      0,   0, 1600, 1200,  0,    0, 1600,  1200},  // Preview
 { 1600, 1200,	   0,	 0, 1600, 1200, 1600, 1200, 	 0,   0, 1600, 1200,  0,	0, 1600,  1200},   // capture
 { 1600, 1200,	   0,	 0, 1600, 1200, 1600, 1200, 	 0,   0, 1600, 1200,  0,	0, 1600,  1200},   //video
 { 1600, 1200,      0,    0, 1600, 1200, 1600, 1200,      0,   0, 1600, 1200,  0,    0, 1600,  1200},  //high speed video
 { 1600, 1200,      0,    0, 1600, 1200, 1600, 1200,      0,   0, 1600, 1200,  0,    0, 1600,  1200}, //slim video
 { 1600, 1200,      0,    0, 1600, 1200, 1600, 1200,      0,   0, 1600, 1200,  0,    0, 1600,  1200}}; //custom1
 //-bug 663265 zhoumin.wt, modify, 2021/09/17, modify framerate of dualcam to be 24fps
//{ 1920, 1080,      0,    0, 1920, 1080, 1920, 1080,      0,   0, 1920, 1080,  0,    0, 1920,  1080}, // capture
 //{ 1920, 1080,      0,    0, 1920, 1080, 1920, 1080,      0,   0, 1920, 1080,  0,    0, 1920,  1080},  // video
 //{ 1920, 1080,      0,    0, 1920, 1080, 1920, 1080,      0,   0, 1920, 1080,  0,    0, 1920,  1080}, //hight speed video
 //{ 1920, 1080,      0,    0, 1920, 1080, 1920, 1080,      0,   0, 1920, 1080,  0,    0, 1920,  1080}}; // slim video


static kal_uint16 read_cmos_sensor(kal_uint32 addr)
{
	kal_uint16 get_byte=0;
	char pu_send_cmd[2] = {(char)(addr >> 8), (char)(addr & 0xFF) };
	iReadRegI2C(pu_send_cmd, 2, (u8*)&get_byte, 1, imgsensor.i2c_write_id);
	return get_byte;
}

static void write_cmos_sensor(kal_uint32 addr, kal_uint32 para)
{
	char pu_send_cmd[3] = {(char)(addr >> 8), (char)(addr & 0xFF), (char)(para & 0xFF)};
	iWriteRegI2C(pu_send_cmd, 3, imgsensor.i2c_write_id);
}



static void set_max_framerate(UINT16 framerate,kal_bool min_framelength_en)
{
    //C2515MIPISENSORDB("[lj c2515]func= %s \n",__func__);

    kal_uint32 frame_length = imgsensor.frame_length;
	
    C2515MIPISENSORDB("[lj c2515]>>> set_max_framerate():framerate = %d, min framelength should enable = %d\n", framerate,min_framelength_en);

    frame_length = imgsensor.pclk / framerate * 10 / imgsensor.line_length;
	
    spin_lock(&imgsensor_drv_lock);
    imgsensor.frame_length = (frame_length > imgsensor.min_frame_length) ? frame_length : imgsensor.min_frame_length;

    if (imgsensor.frame_length > imgsensor_info.max_frame_length)
        imgsensor.frame_length = imgsensor_info.max_frame_length;

    if (min_framelength_en)
        imgsensor.min_frame_length = imgsensor.frame_length;
    spin_unlock(&imgsensor_drv_lock);


	//update Frame Length
	write_cmos_sensor(0x0340, imgsensor.frame_length >> 8);
	write_cmos_sensor(0x0341, imgsensor.frame_length & 0xFF);

	C2515MIPISENSORDB("[lj c2515]>>> set_max_framerate():framerate = %d, min framelength should enable = %d\n", framerate,min_framelength_en);
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
    //C2515MIPISENSORDB("[lj c2515]func= %s \n",__func__);
    unsigned long flags;
    spin_lock_irqsave(&imgsensor_drv_lock, flags);
    imgsensor.shutter = shutter;
    spin_unlock_irqrestore(&imgsensor_drv_lock, flags);
	//printk("@CST_100:set_shutter(),input_shutter=0x%x\n",shutter);
	
    // if shutter bigger than frame_length, should extend frame length first
    spin_lock(&imgsensor_drv_lock);
    if (shutter > imgsensor.min_frame_length - imgsensor_info.margin)
        imgsensor.frame_length = shutter + imgsensor_info.margin;
    else
        imgsensor.frame_length = imgsensor.min_frame_length;
	
    if (imgsensor.frame_length > imgsensor_info.max_frame_length)
        imgsensor.frame_length = imgsensor_info.max_frame_length;
    spin_unlock(&imgsensor_drv_lock);
	
    C2515MIPISENSORDB("[lj c2515]set_shutter(): shutter =%d\n", shutter);

	
    shutter = (shutter < imgsensor_info.min_shutter) ? imgsensor_info.min_shutter : shutter;
    shutter = (shutter > (imgsensor_info.max_frame_length - imgsensor_info.margin)) ? (imgsensor_info.max_frame_length - imgsensor_info.margin) : shutter;


	// Update Shutter
	write_cmos_sensor(0x0340, imgsensor.frame_length >> 8);
	write_cmos_sensor(0x0341, imgsensor.frame_length & 0xFF);
	//printk("@CST_101:set_shutter(),writer_shutter=0x%x\n",shutter);
	write_cmos_sensor(0x0202, (shutter >> 8) & 0xFF);	
	write_cmos_sensor(0x0203, (shutter) & 0xFF);

	C2515MIPISENSORDB("[lj c2515]set_shutter(): shutter =%d, framelength =%d,mini_framelegth=%d\n", shutter,imgsensor.frame_length,imgsensor.min_frame_length);
}

//+bug604664 chenbocheng.wt, add, 2020/01/06, modify code for dualcam sync issue
static void set_shutter_frame_length(
                kal_uint16 shutter, kal_uint16 frame_length)
{
    unsigned long flags;
    kal_uint16 realtime_fps = 0;
    kal_int32 dummy_line = 0;

    spin_lock_irqsave(&imgsensor_drv_lock, flags);
    imgsensor.shutter = shutter;
    spin_unlock_irqrestore(&imgsensor_drv_lock, flags);
    //LOG_DBG("shutter =%d, frame_time =%d\n", shutter, frame_time);

    /* 0x3500, 0x3501, 0x3502 will increase VBLANK
     * to get exposure larger than frame exposure
     */
    /* AE doesn't update sensor gain at capture mode,
     * thus extra exposure lines must be updated here.
     */

    /* OV Recommend Solution */
    /* if shutter bigger than frame_length,
     * should extend frame length first
     */
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
		if (realtime_fps >= 297 && realtime_fps <= 305)
			set_max_framerate(296, 0);
		else if (realtime_fps >= 147 && realtime_fps <= 150)
			set_max_framerate(146, 0);
		else {
			/* Extend frame length */
		write_cmos_sensor(0x0340, imgsensor.frame_length >> 8);
		write_cmos_sensor(0x0341, imgsensor.frame_length & 0xFF);
		}
	} else {
		/* Extend frame length */
		write_cmos_sensor(0x0340, imgsensor.frame_length >> 8);
		write_cmos_sensor(0x0341, imgsensor.frame_length & 0xFF);
	}
	/* Update Shutter */
	write_cmos_sensor(0x0202, (shutter >> 8) & 0xFF);
	write_cmos_sensor(0x0203, (shutter) & 0xFF);
	LOG_INF("Exit! shutter =%d, framelength =%d/%d, dummy_line=%d, \n", shutter,
		imgsensor.frame_length, frame_length, dummy_line);
}/* set_shutter_frame_length */
//-bug604664 chenbocheng.wt, add, 2020/01/06, modify code for dualcam sync issue

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
#define AGAIN_NUM	49
	static	kal_uint16	again_table[AGAIN_NUM] =
	{
	  64,	68, 72, 76, 80, 84, 88, 92, 96, 100, 104, 108, 112, 116, 120, 124, 128, 136, 144, 152,
	  160, 168, 176, 184, 192, 200, 208, 216, 224, 232, 240, 248, 256, 272, 288, 304, 320, 336, 352, 368,
	  384, 400,	416, 432, 448, 464, 480, 496, 512,
	};
	static	kal_uint16	again_register_table_1[AGAIN_NUM] =
	{
	  0x86, 0x87,	0x86,	0x86,	0x86,	0x86,	0x86,	0x86,	0x86,	0x86,	0xf6,	0xf6,	0xd6,	0xd6,	0xe6,	0xd6,	0xd7,	0xd7,	0xb7,
	  0xc6,	0xc6,	0xc6,	0xe5,	0xb5,	0xd5,	0xd5,	0xd4,	0xd4,	0xd4,	0xd3,	0xc3,	0xd3,	0xd7,	0xd7,	0xd6,	0xd6,	0xd6,	0xe5,
	  0xd5,	0xe5,	0xf4,	0xf4,	0xe4,	0xe4,	0xe3,	0xe3,	0xd3,	0xd3,	0xf3,
	};

	static	kal_uint16  again_register_table_2[AGAIN_NUM] =
	{
	  0x03, 0x03,	0x03,	0x03,	0x03,	0x03,	0x03,	0x03,	0x03,	0x03,	0x03,	0x03,	0x03,	0x03,	0x03,	0x03,	0x03,	0x03,	0x03,
	  0x03,	0x03,	0x03,	0x03,	0x03,	0x03,	0x03,	0x03,	0x03,	0x03,	0x03,	0x03,	0x03,	0x01,	0x01,	0x01,	0x01,	0x01,	0x01,
	  0x01,	0x01,	0x01,	0x01,	0x01,	0x01,	0x01,	0x01,	0x01,	0x01,	0x01,
	};

	static	kal_uint16  again_register_table_3[AGAIN_NUM] =
	{
	  0x10, 0x11,	0x12,	0x13,	0x14,	0x15,	0x16,	0x17,	0x18,	0x19,	0x1a,	0x1b,	0x1c,	0x1d,	0x1e,	0x1f,	0x20,	0x22,	0x24,
	  0x26,	0x28,	0x2a,	0x2c,	0x2e,	0x30,	0x32,	0x34,	0x36,	0x38,	0x3a,	0x3c,	0x3e,	0x20,	0x22,	0x24,	0x26,	0x28,	0x2a,
	  0x2c,	0x2e,	0x30,	0x32,	0x34,	0x36,	0x38,	0x3a,	0x3c,	0x3e,	0x3f,
	};

	static	kal_uint16	again_register_table_4[AGAIN_NUM] =
	{
  	  0x9f, 0x9f,	0x9f,	0x9f,	0x9f,	0x9f,	0x9f,	0x9f,	0xaf,	0xaf,	0xaf,	0xaf,	0xaf,	0xaf,	0xaf,	0xaf,	0xaf,	0xaf,	0xaf,
	  0xaf, 0xaf,	0xaf,	0xaf,	0xaf,	0xaf,	0xaf,	0xaf,	0xaf,	0xbf,	0xbf,	0xb0,	0xb0,	0x80,	0x80,	0x80,	0x80,	0x8a,	0x8a,
	  0x8a, 0x8a,	0x8a,	0x8a,	0x8a,	0x8a,	0x8a,	0x8a,	0x8a,	0x8a,	0x8a,
	};

	static	kal_uint16	again_register_table_5[AGAIN_NUM] =
	{
	  0x2f, 0x2f,	0x2f,	0x2f,	0x2f,	0x2f,	0x2f,	0x2f,	0x2f,	0x2f,	0x10,	0x10,	0x10,	0x10,	0x10,	0x10,	0x10,	0x10,	0x10,
	  0x10, 0x10,	0x10,	0x10,	0x10,	0x10,	0x10,	0x10,	0x10,	0x10,	0x10,	0x10,	0x10,	0x10,	0x10,	0x10,	0x10,	0x10,	0x10,
	  0x10, 0x10,	0x10,	0x10,	0x10,	0x10,	0x10,	0x10,	0x10,	0x10,	0x10,
	};

	static	kal_uint16	again_register_table_6[AGAIN_NUM] =
	{
	  0x16, 0x16,	0x16,	0x16,	0x16,	0x16,	0x16,	0x16,	0x16,	0x16,	0x2f,	0x2f,	0x2f,	0x2f,	0x2f,	0x2f,	0x2f,	0x2f,	0x2f,
	  0x2f, 0x2f,	0x2f,	0x2f,	0x2f,	0x2f,	0x2f,	0x2f,	0x2f,	0x2f,	0x2f,	0x2f,	0x2f,	0x2f,	0x2f,	0x2f,	0x2f,	0x2f,	0x2f,
	  0x2f, 0x2f,	0x2f,	0x2f,	0x2f,	0x2f,	0x2f,	0x2f,	0x2f,	0x2f,	0x2f,
	};

static kal_uint16 set_gain(kal_uint16 gain)
{
	//kal_uint32 index = 0;
	//kal_uint16 regCount = 0;
	kal_uint16 tmp0 = 0, tmp1 = 0, tmp2 = 0, tmp3 = 0, tmp4 = 0, tmp5 = 0, tmp6 = 0, i = 0, Again_base = 0;
	kal_uint16 iGain=gain;
    C2515MIPISENSORDB("[lj c2515] C2515 isp again = %x\n",(gain & 0xFF));
	//printk("@CST_000:set_gain(),input_gain=0x%x\n",gain);


	#if 0
	//for debug
		{
			kal_uint16 reg_32ac = 0;
			kal_uint16 reg_3293 = 0;
			kal_uint16 reg_32a9 = 0;
			kal_uint16 reg_3290 = 0;
			kal_uint16 reg_32ad = 0;
			
			reg_32ac = read_cmos_sensor(0x32ac);
			reg_3293 = read_cmos_sensor(0x3293);
			reg_32a9 = read_cmos_sensor(0x32a9);
			reg_3290 = read_cmos_sensor(0x3290);
			reg_32ad = read_cmos_sensor(0x32ad);
	
			printk("@CST_002:set_gain(),R,32ac=0x%x,3293=0x%x,32a9=0x%x,3290=0x%x,32ad=0x%x\n",reg_32ac,reg_3293,reg_32a9,reg_3290,reg_32ad);
		}
	#endif
	if(iGain >= again_table[AGAIN_NUM-1]){
		iGain = again_table[AGAIN_NUM-1]; //max gain: 8*64
		Again_base = AGAIN_NUM-1;
	}
	if(iGain <= BASEGAIN){
		iGain = again_table[0];           //iGain <= 0x40(=1x)
		//Again_base = 0;
	}
	for(i=1; i < AGAIN_NUM; i++){
			if(iGain < again_table[i]){
				Again_base = i - 1;
				break;
			}
	}

	tmp0 = again_register_table_2[Again_base];                 //0x3293 0xe00e
	tmp1 = again_register_table_3[Again_base];                 //0x32a9 0xe011
	tmp2 = (again_register_table_1[Again_base]>>4) | 0xf0;     //0x32ac 0xe014
	tmp3 = (again_register_table_1[Again_base] & 0x0f) | 0xb0; //0x3290 0xe017
	tmp4 = again_register_table_4[Again_base];                 //0x32ad 0xe01a
	tmp5 = again_register_table_5[Again_base];				   //0x3211	0xe01d
	tmp6 = again_register_table_6[Again_base];				   //0x3216	0xe020

	write_cmos_sensor(0xe01a,tmp0);
	write_cmos_sensor(0xe01d,tmp1);
	write_cmos_sensor(0xe020,tmp2);
	write_cmos_sensor(0xe023,tmp3);
	write_cmos_sensor(0xe026,tmp4);
	write_cmos_sensor(0xe029,tmp5);
	write_cmos_sensor(0xe02c,tmp6);
	write_cmos_sensor(0x340f,0x13); //group delay 1frame write gain
	//printk("@CST_001:set_gain(),W,32ac=0x%x,3293=0x%x,32a9=0x%x,3290=0x%x,32ad=0x%x,3211=0x%x,3216=0x%x\n",tmp2,tmp0,tmp1,tmp3,tmp4,tmp5,tmp6);

	return 0;
}
static void ihdr_write_shutter_gain(kal_uint16 le, kal_uint16 se, kal_uint16 gain)
{
	LOG_INF("le: 0x%x, se: 0x%x, gain: 0x%x\n", le, se, gain);
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
	    C2515MIPISENSORDB("[lj c2515]func= %s \n",__func__);
/*No Need to implement this function*/
}    /*    night_mode    */

static void sensor_init(void)
{
    C2515MIPISENSORDB("[lj c2515]func= %s \n",__func__);
	C2515MIPISENSORDB("[lj c2515]sensor_init begin\n");

	write_cmos_sensor(0x0100,0x00);
	write_cmos_sensor(0x0103,0x01);
	
	write_cmos_sensor(0x3288,0x50);
	write_cmos_sensor(0x0304,0x00);
	write_cmos_sensor(0x0400,0x41);
	write_cmos_sensor(0x0401,0xa5);
	write_cmos_sensor(0x0403,0x36);
	write_cmos_sensor(0x3885,0x22);
	write_cmos_sensor(0x32af,0x88);
	write_cmos_sensor(0x0202,0x03);
	write_cmos_sensor(0x3280,0x28);
	write_cmos_sensor(0x3284,0xA3);
	write_cmos_sensor(0x3286,0x35);
	write_cmos_sensor(0x3287,0x0B);
	write_cmos_sensor(0x3288,0x40);
	//write_cmos_sensor(0x328b,0x61);  //+bug584789,zhanghengyuan.wt,MODIFY,2020/12/30,depth c2515  mclk driving current
	write_cmos_sensor(0x328C,0x48);
	write_cmos_sensor(0x32aa,0x05); 
	write_cmos_sensor(0x32ab,0x08);
	write_cmos_sensor(0x3C00,0x43);
	write_cmos_sensor(0x3C01,0x03);
	write_cmos_sensor(0x3211,0x10);
	write_cmos_sensor(0x3218,0x28);
	write_cmos_sensor(0x3805,0x08);
	write_cmos_sensor(0x3808,0x16);
	write_cmos_sensor(0x3809,0x96);
	write_cmos_sensor(0x380a,0x7d);
	write_cmos_sensor(0x380e,0x0d);
	write_cmos_sensor(0x380c,0x01);
	write_cmos_sensor(0x0202,0x04);
	write_cmos_sensor(0x0203,0x56);
	write_cmos_sensor(0x3108,0xcf);
	write_cmos_sensor(0x3115,0x30);
//+bug584789,zhanghengyuan.wt,MODIFY,2020/12/30,depth c2515  mclk driving current
	write_cmos_sensor(0x328b,0xa1);
	write_cmos_sensor(0x3295,0x05);
//-bug584789,zhanghengyuan.wt,MODIFY,2020/12/30,depth c2515  mclk driving current
	write_cmos_sensor(0x3212,0x2f);
	write_cmos_sensor(0x3293,0x01);
	write_cmos_sensor(0x32a9,0x3f);
	write_cmos_sensor(0x3290,0xb3);
	write_cmos_sensor(0x32ad,0x8a);
	write_cmos_sensor(0x3216,0x2f);
	write_cmos_sensor(0x3217,0x08);
	//write_cmos_sensor(0x3298,0x44);
	write_cmos_sensor(0x3298,0x48);
	write_cmos_sensor(0x3211,0x10);
	write_cmos_sensor(0x3216,0x2f);
	write_cmos_sensor(0x32af,0x80);
	//write_cmos_sensor(0x3286,0x03);
	//write_cmos_sensor(0x3287,0x62);
	write_cmos_sensor(0x3212,0x4a);
	write_cmos_sensor(0x3286,0x05); //remove H line
	write_cmos_sensor(0x3287,0x46); //improve H line

	write_cmos_sensor(0x3881,0x00); //FIFO
	write_cmos_sensor(0x0400,0x47);
	write_cmos_sensor(0x0404,0x08);
	write_cmos_sensor(0x0405,0x80);
	write_cmos_sensor(0x0406,0x02);
	write_cmos_sensor(0x0407,0x80);
	write_cmos_sensor(0x3403,0x02);
	write_cmos_sensor(0x3407,0x07);
	write_cmos_sensor(0x3411,0x00);
	write_cmos_sensor(0x3412,0x01);
	write_cmos_sensor(0x3415,0x01);
	write_cmos_sensor(0x3416,0x01);
	write_cmos_sensor(0x3500,0x10);
	write_cmos_sensor(0x3584,0x02);
	write_cmos_sensor(0xe000,0x31);
	write_cmos_sensor(0xe001,0x08);
	write_cmos_sensor(0xe002,0x4f);
	write_cmos_sensor(0xe00c,0x31);
	write_cmos_sensor(0xe00d,0x08);
	write_cmos_sensor(0xe00e,0xef);
	write_cmos_sensor(0xe018,0x32);
	write_cmos_sensor(0xe019,0x93);
	write_cmos_sensor(0xe01a,0x03);
	write_cmos_sensor(0xe01b,0x32);
	write_cmos_sensor(0xe01c,0xa9);
	write_cmos_sensor(0xe01d,0x10);
	write_cmos_sensor(0xe01e,0x32);
	write_cmos_sensor(0xe01f,0xac);
	write_cmos_sensor(0xe020,0xff);
	write_cmos_sensor(0xe021,0x32);
	write_cmos_sensor(0xe022,0x90);
	write_cmos_sensor(0xe023,0xb6);
	write_cmos_sensor(0xe024,0x32);
	write_cmos_sensor(0xe025,0xad);
	write_cmos_sensor(0xe026,0x9f);
	write_cmos_sensor(0xe027,0x32);
	write_cmos_sensor(0xe028,0x11);
	write_cmos_sensor(0xe029,0x10);
	write_cmos_sensor(0xe02a,0x32);
	write_cmos_sensor(0xe02b,0x16);
	write_cmos_sensor(0xe02c,0x2f);
	write_cmos_sensor(0x3500,0x00);
	write_cmos_sensor(0x3584,0x22);

	write_cmos_sensor(0x0101,0x03); //+bug584789,zhanghengyuan.wt,MODIFY,2020/12/30,depth c2515 mirror
	///write_cmos_sensor(0x0100,0x01);

	C2515MIPISENSORDB("[lj c2515]sensor_init end\n");
}


static void preview_setting(kal_uint16 currefps)
{
    C2515MIPISENSORDB("[lj c2515]func= %s \n",__func__);
	C2515MIPISENSORDB("[lj c2515]E! preview_setting currefps:%d\n",currefps);
	//sensor_init();
	write_cmos_sensor(0x0100,0x01);  //streaming starts
}

static void capture_setting(kal_uint16 currefps)
{
    C2515MIPISENSORDB("[lj c2515]func= %s \n",__func__);
    C2515MIPISENSORDB("[lj c2515]E! capture_setting currefps:%d\n",currefps);
	//sensor_init();
	write_cmos_sensor(0x0100,0x01);  //streaming starts

}


static void normal_video_setting(kal_uint16 currefps)
{
    C2515MIPISENSORDB("[lj c2515]func= %s \n",__func__);
	write_cmos_sensor(0x0100,0x01); //streaming starts

}

static void hs_video_setting(kal_uint16 currefps)
{
    C2515MIPISENSORDB("[lj c2515]func= %s \n",__func__);
	//preview_setting(300);
	write_cmos_sensor(0x0100,0x01);  //streaming starts
}

static void slim_video_setting(kal_uint16 currefps)
{
    C2515MIPISENSORDB("[lj c2515]func= %s \n",__func__);
	//preview_setting(300);
   write_cmos_sensor(0x0100,0x01);  //streaming starts
}

//+bug 663265 zhoumin.wt, add, 2021/09/17, modify framerate of dualcam to be 24fps
static void custom1_setting(kal_uint16 currefps)
{
    C2515MIPISENSORDB("[lj c2515]func= %s \n",__func__);
    write_cmos_sensor(0x0340,0x06);
    write_cmos_sensor(0x0341,0x04);
    write_cmos_sensor(0x0100,0x01);  //streaming starts
}
//-bug 663265 zhoumin.wt, add, 2021/09/17, modify framerate of dualcam to be 24fps

static kal_uint32 set_test_pattern_mode(kal_bool enable)
{
    C2515MIPISENSORDB("[lj c2515]func= %s \n",__func__);
    C2515MIPISENSORDB("[lj c2515]enable: %d\n", enable);
    spin_lock(&imgsensor_drv_lock);
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
//extern u32 pinSetIdx;
static kal_uint32 get_imgsensor_id(UINT32 *sensor_id)
{
    kal_uint8 i = 0;
    kal_uint8 retry = 2;
	printk("[lj c2515]func= %s \n",__func__);
	//printk("c2515 get_imgsensor_id pinSetIdx = %d\n ",pinSetIdx);
	//if(pinSetIdx)
		//return ERROR_SENSOR_CONNECT_FAIL;
    //+bug604664 liudijin.wt, add, 2021/04/16, distinguish depth camera params for ov64b40 dualcam
    while (imgsensor_info.i2c_addr_table[i] != 0xff) {
        spin_lock(&imgsensor_drv_lock);
        imgsensor.i2c_write_id = imgsensor_info.i2c_addr_table[i];
        spin_unlock(&imgsensor_drv_lock);
        do {
            *sensor_id = ((read_cmos_sensor(0x0000) << 8) | read_cmos_sensor(0x0001));
            //if ((*sensor_id == imgsensor_info.sensor_id) && (W1HI4821QREARQT_SENSOR_ID == main_sensor_id)) {
            if (*sensor_id == imgsensor_info.sensor_id) {
                printk("[dualcam] c2515 get_imgsensor_id OK,i2c write id: 0x%x, sensor id: 0x%x\n", imgsensor.i2c_write_id,*sensor_id);
                return ERROR_NONE;
            }
            printk("[dualcam] c2515 get_imgsensor_id fail, write id: 0x%x, id: 0x%x\n", imgsensor.i2c_write_id,*sensor_id);
            retry--;
        } while(retry > 0);
        i++;
        retry = 2;
    }

    //if ((*sensor_id != imgsensor_info.sensor_id) || (W1HI4821QREARQT_SENSOR_ID != main_sensor_id)) {
    if (*sensor_id != imgsensor_info.sensor_id){
        /* if Sensor ID is not correct, Must set *sensor_id to 0xFFFFFFFF */
        *sensor_id = 0xFFFFFFFF;
        //pr_err("3rd dualcam c2515 sensor id error,main sensor id:%x,depth sensor id:%x\n",main_sensor_id,*sensor_id);
        pr_err("3rd dualcam c2515 sensor id error,depth sensor id:%x\n",*sensor_id);
        return ERROR_SENSOR_CONNECT_FAIL;
    }
    //-bug604664 liudijin.wt, add, 2021/04/16, distinguish depth camera params for ov64b40 dualcam

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
    printk("[lj c2515]func= %s \n",__func__);
	//hwPowerOn(CAMERA_POWER_VCAM_D, VOL_1800,"C2515");
	//printk("xyc DVDD 1.8V\n");
    while (imgsensor_info.i2c_addr_table[i] != 0xff) {
        spin_lock(&imgsensor_drv_lock);
        imgsensor.i2c_write_id = imgsensor_info.i2c_addr_table[i];
        spin_unlock(&imgsensor_drv_lock);
        do {
            sensor_id = ((read_cmos_sensor(0x0000) << 8) | read_cmos_sensor(0x0001));
		pr_err("zhh11 read id success,depth sensor id:%x\n", sensor_id);
            if (sensor_id == imgsensor_info.sensor_id) {
                printk("[lj c2515]open:Read sensor id OK,i2c write id: 0x%x, sensor id: 0x%x\n", imgsensor.i2c_write_id,sensor_id);
                break;
            }
            printk("[lj c2515]Read sensor id fail, write id: 0x%x, id: 0x%x\n", imgsensor.i2c_write_id,sensor_id);
            retry--;
        } while(retry > 0);
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
    imgsensor.autoflicker_en= KAL_FALSE;
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
}    /*    open  */



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
    C2515MIPISENSORDB("[lj c2515]func= %s \n",__func__);

    /*No Need to implement this function*/

    return ERROR_NONE;
}    /*    close  */


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
*    None
*
* GLOBALS AFFECTED
*
*************************************************************************/
static kal_uint32 preview(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
                      MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
    C2515MIPISENSORDB("[lj c2515]func= %s \n",__func__);
    C2515MIPISENSORDB("[lj c2515]>>> preview()\n");

    spin_lock(&imgsensor_drv_lock);
    imgsensor.sensor_mode = IMGSENSOR_MODE_PREVIEW;
    imgsensor.autoflicker_en = KAL_FALSE;

	imgsensor.pclk= imgsensor_info.pre.pclk;
	imgsensor.line_length=imgsensor_info.pre.linelength;
	imgsensor.frame_length=imgsensor_info.pre.framelength;

	imgsensor.min_frame_length = imgsensor_info.pre.framelength;

    spin_unlock(&imgsensor_drv_lock);

    preview_setting(imgsensor.current_fps);

    C2515MIPISENSORDB("[lj c2515]<<< preview()\n");
    return ERROR_NONE;
}    /*    preview   */

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
    C2515MIPISENSORDB("[lj c2515]func= %s \n",__func__);

    spin_lock(&imgsensor_drv_lock);
    imgsensor.sensor_mode = IMGSENSOR_MODE_CAPTURE;

	imgsensor.pclk= imgsensor_info.cap.pclk;
	imgsensor.line_length=imgsensor_info.cap.linelength;
	imgsensor.frame_length=imgsensor_info.cap.framelength;

	imgsensor.min_frame_length = imgsensor_info.cap.framelength;
    spin_unlock(&imgsensor_drv_lock);

    capture_setting(imgsensor.current_fps);

    C2515MIPISENSORDB("[lj c2515]<<< capture()\n");

    return ERROR_NONE;
}    /* capture() */
static kal_uint32 normal_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
                      MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
    C2515MIPISENSORDB("[lj c2515]<<< normal_video()\n");
    C2515MIPISENSORDB("[lj c2515]func= %s \n",__func__);
    spin_lock(&imgsensor_drv_lock);
    imgsensor.sensor_mode = IMGSENSOR_MODE_VIDEO;
    imgsensor.autoflicker_en = KAL_FALSE;

	imgsensor.pclk= imgsensor_info.normal_video.pclk;
	imgsensor.line_length=imgsensor_info.normal_video.linelength;
	imgsensor.frame_length=imgsensor_info.normal_video.framelength;

	imgsensor.min_frame_length = imgsensor_info.normal_video.framelength;
    spin_unlock(&imgsensor_drv_lock);

    normal_video_setting(imgsensor.current_fps);

    C2515MIPISENSORDB("[lj c2515]<<< normal_video()\n");
    return ERROR_NONE;
}    /*    normal_video   */

static kal_uint32 hs_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
                      MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
    C2515MIPISENSORDB("[lj c2515]<<< hs_video()\n");
    C2515MIPISENSORDB("[lj c2515]func= %s \n",__func__);
    spin_lock(&imgsensor_drv_lock);
    imgsensor.sensor_mode = IMGSENSOR_MODE_HIGH_SPEED_VIDEO;
    imgsensor.autoflicker_en = KAL_FALSE;

	imgsensor.pclk= imgsensor_info.hs_video.pclk;
	imgsensor.line_length=imgsensor_info.hs_video.linelength;
	imgsensor.frame_length=imgsensor_info.hs_video.framelength;

	imgsensor.min_frame_length = imgsensor_info.hs_video.framelength;
    spin_unlock(&imgsensor_drv_lock);
    hs_video_setting(imgsensor.current_fps);



    C2515MIPISENSORDB("[lj c2515]<<< hs_video()\n");
    return ERROR_NONE;
}    /*    hs_video   */

static kal_uint32 slim_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
                      MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
    C2515MIPISENSORDB("[lj c2515]<<< slim_video()\n");
    C2515MIPISENSORDB("[lj c2515]func= %s \n",__func__);
    spin_lock(&imgsensor_drv_lock);
    imgsensor.sensor_mode = IMGSENSOR_MODE_SLIM_VIDEO;
    imgsensor.autoflicker_en = KAL_FALSE;

	imgsensor.pclk= imgsensor_info.slim_video.pclk;
	imgsensor.line_length=imgsensor_info.slim_video.linelength;
	imgsensor.frame_length=imgsensor_info.slim_video.framelength;

	imgsensor.min_frame_length = imgsensor_info.slim_video.framelength;
    spin_unlock(&imgsensor_drv_lock);
    slim_video_setting(imgsensor.current_fps);


    C2515MIPISENSORDB("[lj c2515]<<< slim_video()\n");
    return ERROR_NONE;
}    /*    slim_video     */

//+bug 663265 zhoumin.wt, add, 2021/09/17, modify framerate of dualcam to be 24fps
static kal_uint32 custom1(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
                      MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
    C2515MIPISENSORDB("[lj c2515]<<< custom()\n");
    C2515MIPISENSORDB("[lj c2515]func= %s \n",__func__);
    spin_lock(&imgsensor_drv_lock);
    imgsensor.sensor_mode = IMGSENSOR_MODE_CUSTOM1;
    imgsensor.autoflicker_en = KAL_FALSE;

    imgsensor.pclk= imgsensor_info.custom1.pclk;
    imgsensor.line_length=imgsensor_info.custom1.linelength;
    imgsensor.frame_length=imgsensor_info.custom1.framelength;

    imgsensor.min_frame_length = imgsensor_info.custom1.framelength;
    spin_unlock(&imgsensor_drv_lock);
    custom1_setting(imgsensor.current_fps);

    C2515MIPISENSORDB("[lj c2515]<<< custom()\n");
    return ERROR_NONE;
}    /*    custom     */
//-bug 663265 zhoumin.wt, add, 2021/09/17, modify framerate of dualcam to be 24fps

static kal_uint32 get_resolution(MSDK_SENSOR_RESOLUTION_INFO_STRUCT *sensor_resolution)
{
    C2515MIPISENSORDB("[lj c2515]func= %s \n",__func__);
    C2515MIPISENSORDB("[lj c2515]>>> get_resolution()\n");
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

    //+bug 663265 zhoumin.wt, add, 2021/09/17, modify framerate of dualcam to be 24fps
    sensor_resolution->SensorCustom1Width = imgsensor_info.custom1.grabwindow_width;
    sensor_resolution->SensorCustom1Height = imgsensor_info.custom1.grabwindow_height;
    //-bug 663265 zhoumin.wt, add, 2021/09/17, modify framerate of dualcam to be 24fps

    C2515MIPISENSORDB("[lj c2515]>>> feature_control()\n");
    return ERROR_NONE;
}    /*    get_resolution    */

static kal_uint32 get_info(enum MSDK_SCENARIO_ID_ENUM scenario_id,
                      MSDK_SENSOR_INFO_STRUCT *sensor_info,
                      MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
    C2515MIPISENSORDB("[lj c2515]>>> get_info():scenario_id = %d\n", scenario_id);
    C2515MIPISENSORDB("[lj c2515]func= %s \n",__func__);
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
    sensor_info->Custom1DelayFrame = imgsensor_info.custom1_delay_frame;//bug 663265 zhoumin.wt, add, 2021/09/17, modify framerate of dualcam to be 24fps

    sensor_info->SensorMasterClockSwitch = 0; /* not use */
    sensor_info->SensorDrivingCurrent = imgsensor_info.isp_driving_current;

    sensor_info->AEShutDelayFrame = imgsensor_info.ae_shut_delay_frame;          /* The frame of setting shutter default 0 for TG int */
    sensor_info->AESensorGainDelayFrame = imgsensor_info.ae_sensor_gain_delay_frame;    /* The frame of setting sensor gain */
    sensor_info->AEISPGainDelayFrame = imgsensor_info.ae_ispGain_delay_frame;
	//+bug604664 chenbocheng.wt, add, 2020/01/06, modify code for dualcam sync issue
    sensor_info->FrameTimeDelayFrame = imgsensor_info.frame_time_delay_frame;

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
    sensor_info->SensorHightSampling = 0;    // 0 is default 1x
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
        //+bug 663265 zhoumin.wt, add, 2021/09/17, modify framerate of dualcam to be 24fps
        case MSDK_SCENARIO_ID_CUSTOM1:
            sensor_info->SensorGrabStartX = imgsensor_info.custom1.startx;
            sensor_info->SensorGrabStartY = imgsensor_info.custom1.starty;
            sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount = imgsensor_info.custom1.mipi_data_lp2hs_settle_dc;
            break;
        //-bug 663265 zhoumin.wt, add, 2021/09/17, modify framerate of dualcam to be 24fps
        default:
            sensor_info->SensorGrabStartX = imgsensor_info.pre.startx;
            sensor_info->SensorGrabStartY = imgsensor_info.pre.starty;
            sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount = imgsensor_info.pre.mipi_data_lp2hs_settle_dc;
            break;
    }

	C2515MIPISENSORDB("[lj c2515]<<< get_info()\n");

    return ERROR_NONE;
}    /*    get_info  */


static kal_uint32 control(enum MSDK_SCENARIO_ID_ENUM scenario_id, MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
                      MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
    C2515MIPISENSORDB("[lj c2515]func= %s \n",__func__);
    C2515MIPISENSORDB("[lj c2515]>>> control() scenario_id = %d\n", scenario_id);
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
	//+bug 663265 zhoumin.wt, add, 2021/09/17, modify framerate of dualcam to be 24fps
        case MSDK_SCENARIO_ID_CUSTOM1:
            custom1(image_window, sensor_config_data);
            break;
	//-bug 663265 zhoumin.wt, add, 2021/09/17, modify framerate of dualcam to be 24fps
        default:
            C2515MIPISENSORDB("[lj c2515]Error ScenarioId setting\n");
            preview(image_window, sensor_config_data);
            return ERROR_INVALID_SCENARIO_ID;
    }


	C2515MIPISENSORDB("[lj c2515]<<< control()\n");
    return ERROR_NONE;
}    /* control() */



static kal_uint32 set_video_mode(UINT16 framerate)
{
    C2515MIPISENSORDB("[lj c2515]func= %s \n",__func__);
    C2515MIPISENSORDB("[lj c2515]framerate = %d\n ", framerate);
    // SetVideoMode Function should fix framerate
    if (framerate == 0)
        // Dynamic frame rate
        return ERROR_NONE;
    spin_lock(&imgsensor_drv_lock);
        imgsensor.current_fps = framerate;
    spin_unlock(&imgsensor_drv_lock);

    set_max_framerate(imgsensor.current_fps,1);

    return ERROR_NONE;
}

static kal_uint32 set_auto_flicker_mode(kal_bool enable, UINT16 framerate)
{
    C2515MIPISENSORDB("[lj c2515]func= %s \n",__func__);
    C2515MIPISENSORDB("[lj c2515]>>>set_auto_flicker_mode():enable = %d, framerate = %d \n", enable, framerate);
    spin_lock(&imgsensor_drv_lock);
    if (enable) //enable auto flicker
        imgsensor.autoflicker_en = KAL_TRUE;
    else //Cancel Auto flick
        imgsensor.autoflicker_en = KAL_FALSE;
    spin_unlock(&imgsensor_drv_lock);

    C2515MIPISENSORDB("[lj c2515]<<< set_auto_flicker_mode():enable = %d, framerate = %d \n", enable, framerate);

    return ERROR_NONE;
}


static kal_uint32 set_max_framerate_by_scenario(enum MSDK_SCENARIO_ID_ENUM scenario_id, MUINT32 framerate)
{
    //kal_uint32 frame_length;
    C2515MIPISENSORDB("[lj c2515]func= %s \n",__func__);
    C2515MIPISENSORDB("[lj c2515]>>>>set_max_framerate_by_scenario():scenario_id = %d, framerate = %d\n", scenario_id, framerate);

    switch (scenario_id) {
        case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
			imgsensor.pclk= imgsensor_info.pre.pclk;
			imgsensor.line_length=imgsensor_info.pre.linelength;
			imgsensor.frame_length=imgsensor_info.pre.framelength;
			imgsensor.current_fps=framerate;


			imgsensor.min_frame_length = imgsensor_info.pre.framelength;
            break;
        case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			imgsensor.pclk= imgsensor_info.normal_video.pclk;
			imgsensor.line_length=imgsensor_info.normal_video.linelength;
			imgsensor.frame_length=imgsensor_info.normal_video.framelength;
			imgsensor.current_fps=framerate;

			imgsensor.min_frame_length = imgsensor_info.normal_video.framelength;
            break;
        case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			imgsensor.pclk= imgsensor_info.cap.pclk;
			imgsensor.line_length=imgsensor_info.cap.linelength;
			imgsensor.frame_length=imgsensor_info.cap.framelength;
			imgsensor.current_fps=framerate;


			imgsensor.min_frame_length = imgsensor_info.cap.framelength;
            break;
        case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
			imgsensor.pclk= imgsensor_info.hs_video.pclk;
			imgsensor.line_length=imgsensor_info.hs_video.linelength;
			imgsensor.frame_length=imgsensor_info.hs_video.framelength;
			imgsensor.current_fps=framerate;


			imgsensor.min_frame_length = imgsensor_info.hs_video.framelength;
            break;
        case MSDK_SCENARIO_ID_SLIM_VIDEO:
			imgsensor.pclk= imgsensor_info.slim_video.pclk;
			imgsensor.line_length=imgsensor_info.slim_video.linelength;
			imgsensor.frame_length=imgsensor_info.slim_video.framelength;
			imgsensor.current_fps=framerate;


			imgsensor.min_frame_length = imgsensor.frame_length;
            break;
        //+bug 663265 zhoumin.wt, add, 2021/09/17, modify framerate of dualcam to be 24fps
        case MSDK_SCENARIO_ID_CUSTOM1:
			imgsensor.pclk= imgsensor_info.custom1.pclk;
			imgsensor.line_length=imgsensor_info.custom1.linelength;
			imgsensor.frame_length=imgsensor_info.custom1.framelength;
			imgsensor.current_fps=framerate;
			imgsensor.min_frame_length = imgsensor.frame_length;
            break;
        //-bug 663265 zhoumin.wt, add, 2021/09/17, modify framerate of dualcam to be 24fps
        default:  //coding with  preview scenario by default
			imgsensor.pclk= imgsensor_info.pre.pclk;
			imgsensor.line_length=imgsensor_info.pre.linelength;
			imgsensor.frame_length=imgsensor_info.pre.framelength;
			imgsensor.current_fps=framerate;


			imgsensor.min_frame_length = imgsensor_info.pre.framelength;
            break;
    }

	set_max_framerate(imgsensor.current_fps,1);

    C2515MIPISENSORDB("[lj c2515]<<<set_max_framerate_by_scenario()\n");

	return ERROR_NONE;
}


static kal_uint32 get_default_framerate_by_scenario(enum MSDK_SCENARIO_ID_ENUM scenario_id, MUINT32 *framerate)
{
    C2515MIPISENSORDB("[lj c2515]func= %s \n",__func__);
    C2515MIPISENSORDB("[lj c2515]scenario_id = %d\n", scenario_id);

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
        //+bug 663265 zhoumin.wt, add, 2021/09/17, modify framerate of dualcam to be 24fps
        case MSDK_SCENARIO_ID_CUSTOM1:
            *framerate = imgsensor_info.custom1.max_framerate;
            break;
        //-bug 663265 zhoumin.wt, add, 2021/09/17, modify framerate of dualcam to be 24fps
        default:
            break;
    }

    return ERROR_NONE;
}


static kal_uint32 streaming_control(kal_bool enable)
{/*
	if (enable) {
		write_cmos_sensor(0x0100, 0X01); // stream on
	} else {
		write_cmos_sensor(0x0100, 0X00); // stream off
	}
	mdelay(2);*/
	return ERROR_NONE;
}


//+bug604664 chenbocheng.wt, add, 2020/01/06, modify code for dualcam sync issue
static kal_uint32 feature_control(MSDK_SENSOR_FEATURE_ENUM feature_id,
    UINT8 *feature_para, UINT32 *feature_para_len)
{
    UINT16 *feature_return_para_16 = (UINT16 *)feature_para;
    UINT16 *feature_data_16 = (UINT16 *)feature_para;
    UINT32 *feature_return_para_32 = (UINT32 *)feature_para;
    UINT32 *feature_data_32 = (UINT32 *)feature_para;
    unsigned long long *feature_data = (unsigned long long *)feature_para;

    struct SENSOR_WINSIZE_INFO_STRUCT *wininfo;
    MSDK_SENSOR_REG_INFO_STRUCT *sensor_reg_data = (MSDK_SENSOR_REG_INFO_STRUCT *)feature_para;
    C2515MIPISENSORDB("[lj c2515]func= %s \n",__func__);
    C2515MIPISENSORDB("[lj c2515]>>> feature_control():feature_id = %d\n", feature_id);
    switch (feature_id) {
    //modify codes for factory mode of photo black screen
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
    //modify codes for factory mode of photo black screen
    //modify codes for mipi rate is 0
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
        //+bug 663265 zhoumin.wt, add, 2021/09/17, modify framerate of dualcam to be 24fps
        case MSDK_SCENARIO_ID_CUSTOM1:
            *(MUINT32 *)(uintptr_t)(*(feature_data + 1))
                = imgsensor_info.custom1.pclk;
            break;
        //-bug 663265 zhoumin.wt, add, 2021/09/17, modify framerate of dualcam to be 24fps
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
        //+bug 663265 zhoumin.wt, add, 2021/09/17, modify framerate of dualcam to be 24fps
        case MSDK_SCENARIO_ID_CUSTOM1:
            *(MUINT32 *)(uintptr_t)(*(feature_data + 1))
            = (imgsensor_info.custom1.framelength << 16)
                + imgsensor_info.custom1.linelength;
            break;
        //-bug 663265 zhoumin.wt, add, 2021/09/17, modify framerate of dualcam to be 24fps
        case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
        default:
            *(MUINT32 *)(uintptr_t)(*(feature_data + 1))
            = (imgsensor_info.pre.framelength << 16)
                + imgsensor_info.pre.linelength;
            break;
        }
        break;
    //modify codes for mipi rate is 0
    case SENSOR_FEATURE_GET_PERIOD:
        *feature_return_para_16++ = imgsensor.line_length;
        *feature_return_para_16 = imgsensor.frame_length;
        *feature_para_len = 4;
        break;
    case SENSOR_FEATURE_GET_PIXEL_CLOCK_FREQ:
        *feature_return_para_32 = imgsensor.pclk;
        *feature_para_len = 4;
        break;
    //modify codes for factory mode of photo black screen
    case SENSOR_FEATURE_GET_BINNING_TYPE:
        switch (*(feature_data + 1)) {
        case MSDK_SCENARIO_ID_CUSTOM3:
            *feature_return_para_32 = 1; /*BINNING_NONE*/
            break;
        case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
        case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
        case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
        case MSDK_SCENARIO_ID_SLIM_VIDEO:
        //bug 663265 zhoumin.wt, add, 2021/09/17, modify framerate of dualcam to be 24fps
        case MSDK_SCENARIO_ID_CUSTOM1:
        case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
        case MSDK_SCENARIO_ID_CUSTOM4:
        default:
            *feature_return_para_32 = 1; /*BINNING_AVERAGED*/
            break;
        }
        pr_debug("SENSOR_FEATURE_GET_BINNING_TYPE AE_binning_type:%d,\n",
            *feature_return_para_32);
            *feature_para_len = 4;
        break;
    //modify codes for factory mode of photo black screen
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
            //+bug 663265 zhoumin.wt, add, 2021/09/17, modify framerate of dualcam to be 24fps
            case MSDK_SCENARIO_ID_CUSTOM1:
                rate = imgsensor_info.custom1.mipi_pixel_rate;
                break;
            //-bug 663265 zhoumin.wt, add, 2021/09/17, modify framerate of dualcam to be 24fps
            case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
            default:
                rate = imgsensor_info.pre.mipi_pixel_rate;
                break;
            }
            *(MUINT32 *)(uintptr_t)(*(feature_data + 1)) = rate;
        }
        break;
    case SENSOR_FEATURE_SET_ESHUTTER:
        set_shutter(*feature_data);
        break;
    case SENSOR_FEATURE_SET_NIGHTMODE:
        night_mode((BOOL)*feature_data);
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
        LOG_INF("adb_i2c_read 0x%x = 0x%x\n", sensor_reg_data->RegAddr, sensor_reg_data->RegData);
        break;
    case SENSOR_FEATURE_GET_LENS_DRIVER_ID:
        /* get the lens driver ID from EEPROM or just return LENS_DRIVER_ID_DO_NOT_CARE */
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
        set_auto_flicker_mode((BOOL)*feature_data_16, *(feature_data_16 + 1));
        break;
    case SENSOR_FEATURE_SET_MAX_FRAME_RATE_BY_SCENARIO:
        set_max_framerate_by_scenario((enum MSDK_SCENARIO_ID_ENUM)*feature_data, *(feature_data+1));
        break;
    case SENSOR_FEATURE_GET_DEFAULT_FRAME_RATE_BY_SCENARIO:
        get_default_framerate_by_scenario((enum MSDK_SCENARIO_ID_ENUM)*(feature_data),
            (MUINT32 *)(uintptr_t)(*(feature_data + 1)));
        break;
    case SENSOR_FEATURE_SET_TEST_PATTERN:
        set_test_pattern_mode((BOOL)*feature_data);
        break;
    case SENSOR_FEATURE_GET_TEST_PATTERN_CHECKSUM_VALUE:
        *feature_return_para_32 = imgsensor_info.checksum_value;
        *feature_para_len = 4;
        break;
    case SENSOR_FEATURE_SET_FRAMERATE:
        //+bug682590, liuxiangyin.wt,modif, 2021.09.17,fix KASAN opened to Monitor memory, camera  memory access error.
        LOG_INF("current fps: %d\n", *feature_data_32);
        spin_lock(&imgsensor_drv_lock);
        imgsensor.current_fps = (UINT16)*feature_data_32;
        //-bug682590, liuxiangyin.wt,modif, 2021.09.17, fix KASAN opened to Monitor memory, camera  memory access error.
        spin_unlock(&imgsensor_drv_lock);
        break;
    case SENSOR_FEATURE_SET_SHUTTER_FRAME_TIME:
            set_shutter_frame_length((UINT16) *feature_data,
            (UINT16) *(feature_data + 1));
        break;
    case SENSOR_FEATURE_SET_STREAMING_SUSPEND:
            streaming_control(KAL_FALSE);
        break;
    case SENSOR_FEATURE_SET_STREAMING_RESUME:
            if (*feature_data != 0)
                set_shutter(*feature_data);
            streaming_control(KAL_TRUE);
        break;
    case SENSOR_FEATURE_SET_HDR:
        //+bug682590, liuxiangyin.wt,modif, 2021.09.17, fix KASAN opened to Monitor memory, camera  memory access error.
        LOG_INF("ihdr enable: %d\n", (BOOL)*feature_data_32);
        spin_lock(&imgsensor_drv_lock);
        imgsensor.ihdr_en = (BOOL)*feature_data_32;
        //-bug682590, liuxiangyin.wt,modif, 2021.09.17, fix KASAN opened to Monitor memory, camera  memory access error.
        spin_unlock(&imgsensor_drv_lock);
        break;
    case SENSOR_FEATURE_GET_CROP_INFO:
        LOG_INF("SENSOR_FEATURE_GET_CROP_INFO scenarioId: %d\n", (UINT32)*feature_data);
        wininfo = (struct SENSOR_WINSIZE_INFO_STRUCT *)(uintptr_t)(*(feature_data + 1));
        switch (*feature_data_32) {
        case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
            memcpy((void *)wininfo, (void *)&imgsensor_winsize_info[1], sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
            break;
        case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
            memcpy((void *)wininfo, (void *)&imgsensor_winsize_info[2], sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
            break;
        case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
            memcpy((void *)wininfo, (void *)&imgsensor_winsize_info[3], sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
            break;
        case MSDK_SCENARIO_ID_SLIM_VIDEO:
            memcpy((void *)wininfo, (void *)&imgsensor_winsize_info[4], sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
            break;
        //+bug 663265 zhoumin.wt, add, 2021/09/17, modify framerate of dualcam to be 24fps
        case MSDK_SCENARIO_ID_CUSTOM1:
            memcpy((void *)wininfo, (void *)&imgsensor_winsize_info[5], sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
            break;
        //-bug 663265 zhoumin.wt, add, 2021/09/17, modify framerate of dualcam to be 24fps
        case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
        default:
            memcpy((void *)wininfo, (void *)&imgsensor_winsize_info[0], sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
            break;
        }
        break;
    case SENSOR_FEATURE_SET_IHDR_SHUTTER_GAIN:
        LOG_INF("SENSOR_SET_SENSOR_IHDR LE = %d, SE = %d, Gain = %d\n",
            (UINT16)*feature_data, (UINT16)*(feature_data + 1), (UINT16)*(feature_data + 2));
        ihdr_write_shutter_gain((UINT16)*feature_data, (UINT16)*(feature_data + 1),
            (UINT16)*(feature_data + 2));
        break;
    //modify codes for factory mode of photo black screen
    case SENSOR_FEATURE_GET_FRAME_CTRL_INFO_BY_SCENARIO:
        /*
        * 1, if driver support new sw frame sync
        * set_shutter_frame_length() support third para auto_extend_en
        */
        *(feature_data + 1) = 1;
        /* margin info by scenario */
        *(feature_data + 2) = imgsensor_info.margin;
        break;
    //modify codes for factory mode of photo black screen
    default:
        break;
    }
    return ERROR_NONE;
}
//-bug604664 chenbocheng.wt, add, 2020/01/06, modify code for dualcam sync issue

static struct SENSOR_FUNCTION_STRUCT sensor_func = {
    open,
    get_info,
    get_resolution,
    feature_control,
    control,
    close
};

UINT32 N26_C2515_DEP_CXT_MIPI_MONO_SensorInit(struct SENSOR_FUNCTION_STRUCT **pfFunc)
{
	/* To Do : Check Sensor status here */
	if (pfFunc != NULL)
		*pfFunc = &sensor_func;
	return ERROR_NONE;
}
