/*****************************************************************************
 *
 * Filename:
 * ---------
 *     n28c8496frontdc_mipiraw_sensor.c
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

#include "n28c8496frontdc_mipiraw_sensor.h"

/****************************Modify Following Strings for Debug****************************/
#define PFX "C8496_SensroDriver"

#define LOG_INF(format, args...)    pr_info(PFX "[%s] " format, __func__, ##args)
#define LOG_ERR(format, args...)    pr_err(PFX "[%s] " format, __func__, ##args)
#define LOG_DBG(format, args...)    pr_debug(PFX "[%s] " format, __func__, ##args)

#define C8496_OTP_FUNCTION 1

#if C8496_OTP_FUNCTION
//+S96818AA1-1936,zhujianjia.wt,ADD,2023/04/28,c8496 otp porting
#define MODULE_INFO_SIZE 7
#define MODULE_INFO_FLAG 0x4800
#define MODULE_GROUP1_ADDR  0x4801
#define MODULE_GROUP2_ADDR  0x4809
#define MODULE_GROUP3_ADDR  0x4811
#define SN_INFO_SIZE 12
#define SN_INFO_FLAG 0x4819
#define SN_GROUP1_ADDR  0x481A
#define SN_GROUP2_ADDR  0x4827
#define SN_GROUP3_ADDR  0x4834
#define AWB_INFO_SIZE 16
#define AWB_INFO_FLAG 0x4841
#define AWB_GROUP1_FLAG  0x4842
#define AWB_GROUP2_FLAG  0x4853
#define AWB_GROUP3_FLAG  0x4864
#define LSC_INFO_SIZE 1868
#define LSC_INFO_FLAG 0x4875
#define LSC_GROUP1_FLAG  0x4876
#define LSC_GROUP2_FLAG  0x4FC3
#define LSC_GROUP3_FLAG  0x5710
static unsigned char c8496_data_lsc[LSC_INFO_SIZE + 1] = {0};/*Add check sum*/
static unsigned char c8496_data_awb[AWB_INFO_SIZE + 1] = {0};/*Add check sum*/
static unsigned char c8496_data_info[MODULE_INFO_SIZE + 1] = {0};/*Add check sum*/
static unsigned char c8496_data_sn[SN_INFO_SIZE + 1] = {0};/*Add check sum*/
//-S96818AA1-1936,zhujianjia.wt,ADD,2023/04/28,c8496 otp porting
#endif

static DEFINE_SPINLOCK(imgsensor_drv_lock);
static kal_uint32 ratio = 64;
static kal_uint32 frame_delay_ratio = 64;



static struct imgsensor_info_struct imgsensor_info = {
    .sensor_id = N28C8496FRONTDC_SENSOR_ID, //0x0803 for C8496
    .checksum_value = 0xf7375923,        //checksum value for Camera Auto Test
    .pre = {
        .pclk = 290000000,            //record different mode's pclk//84
        .linelength = 3856,            //record different mode's linelength
        .framelength = 2500,            //record different mode's framelength
        .startx = 0,                    //record different mode's startx of grabwindow
        .starty = 0,                    //record different mode's starty of grabwindow
        .grabwindow_width = 1632,        //record different mode's width of grabwindow
        .grabwindow_height = 1224,        //record different mode's height of grabwindow
        /*     following for MIPIDataLowPwr2HighSpeedSettleDelayCount by different scenario    */
        .mipi_data_lp2hs_settle_dc = 85,//unit , ns //85
        /*     following for GetDefaultFramerateByScenario()    */
        .max_framerate = 300,
        .mipi_pixel_rate = 294400000,
    },
    .cap = {
        .pclk = 290000000,            //record different mode's pclk//84
        .linelength = 3856,            //record different mode's linelength
        .framelength = 2500,            //record different mode's framelength
        .startx = 0,                    //record different mode's startx of grabwindow
        .starty = 0,                    //record different mode's starty of grabwindow
        .grabwindow_width = 3264,        //record different mode's width of grabwindow
        .grabwindow_height = 2448,        //record different mode's height of grabwindow
        /*     following for MIPIDataLowPwr2HighSpeedSettleDelayCount by different scenario    */
        .mipi_data_lp2hs_settle_dc = 85,//unit , ns //85
        /*     following for GetDefaultFramerateByScenario()    */
        .max_framerate = 300,
        .mipi_pixel_rate = 294400000,
    },
    .normal_video = {
        .pclk = 290000000,            //record different mode's pclk//84
        .linelength = 3856,            //record different mode's linelength
        .framelength = 2500,            //record different mode's framelength
        .startx = 0,                    //record different mode's startx of grabwindow
        .starty = 0,                    //record different mode's starty of grabwindow
        .grabwindow_width = 3264,        //record different mode's width of grabwindow
        .grabwindow_height = 2448,        //record different mode's height of grabwindow
        /*     following for MIPIDataLowPwr2HighSpeedSettleDelayCount by different scenario    */
        .mipi_data_lp2hs_settle_dc = 85,//unit , ns //85
        /*     following for GetDefaultFramerateByScenario()    */
        .max_framerate = 300,
        .mipi_pixel_rate = 294400000,
    },
    .hs_video = {
        .pclk = 290000000,            //record different mode's pclk//84
        .linelength = 3856,            //record different mode's linelength
        .framelength = 2500,            //record different mode's framelength
        .startx = 0,                    //record different mode's startx of grabwindow
        .starty = 0,                    //record different mode's starty of grabwindow
        .grabwindow_width = 3264,        //record different mode's width of grabwindow
        .grabwindow_height = 2448,        //record different mode's height of grabwindow
        /*     following for MIPIDataLowPwr2HighSpeedSettleDelayCount by different scenario    */
        .mipi_data_lp2hs_settle_dc = 85,//unit , ns //85
        /*     following for GetDefaultFramerateByScenario()    */
        .max_framerate = 300,
        .mipi_pixel_rate = 294400000,
    },
    .slim_video = {
        .pclk = 290000000,            //record different mode's pclk//84
        .linelength = 3856,            //record different mode's linelength
        .framelength = 2500,            //record different mode's framelength
        .startx = 0,                    //record different mode's startx of grabwindow
        .starty = 0,                    //record different mode's starty of grabwindow
        .grabwindow_width = 3264,        //record different mode's width of grabwindow
        .grabwindow_height = 2448,        //record different mode's height of grabwindow
        /*     following for MIPIDataLowPwr2HighSpeedSettleDelayCount by different scenario    */
        .mipi_data_lp2hs_settle_dc = 85,//unit , ns //85
        /*     following for GetDefaultFramerateByScenario()    */
        .max_framerate = 300,
        .mipi_pixel_rate = 294400000,
    },
    .margin = 4,            //sensor framelength & shutter margin
    .min_shutter = 8,        //min shutter
    .max_frame_length = 0xffff,//max framelength by sensor register's limitation
    .ae_shut_delay_frame = 0,    //shutter delay frame for AE cycle, 2 frame with ispGain_delay-shut_delay=2-0=2
    .ae_sensor_gain_delay_frame = 1,//sensor gain delay frame for AE cycle,2 frame with ispGain_delay-sensor_gain_delay=2-0=2
    .ae_ispGain_delay_frame = 2,//isp gain delay frame for AE cycle
    .ihdr_support = 0,      //1, support; 0,not support
    .ihdr_le_firstline = 0,  //1,le first ; 0, se first
    .sensor_mode_num = 5,      //support sensor mode num

    .cap_delay_frame = 1,
    .pre_delay_frame = 1,
    .video_delay_frame = 2,
    .hs_video_delay_frame = 2,
    .slim_video_delay_frame = 2,

    .min_gain = 64, /*1x gain*/
    .max_gain = 1024, /*16x gain*/
    .min_gain_iso = 100,
    .gain_step = 1,
    .gain_type = 0,
    .i2c_speed = 400,

    .isp_driving_current = ISP_DRIVING_4MA, //mclk driving current
    .sensor_interface_type = SENSOR_INTERFACE_TYPE_MIPI,//sensor_interface_type
    .mipi_sensor_type = MIPI_OPHY_NCSI2, //0,MIPI_OPHY_NCSI2;  1,MIPI_OPHY_CSI2
    .mipi_settle_delay_mode = 0,//0,MIPI_SETTLEDELAY_AUTO; 1,MIPI_SETTLEDELAY_MANNUAL
    .sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_R,//sensor output first pixel color
    .mclk = 24,//mclk value, suggest 24 or 26 for 24Mhz or 26Mhz
    .mipi_lane_num = SENSOR_MIPI_4_LANE,//mipi lane num
    .i2c_addr_table = {0x20, 0x6c, 0xff},//record sensor support all write id addr, only supprt 4must end with 0xff
};

static struct imgsensor_struct imgsensor = {
    .mirror = IMAGE_NORMAL,                //mirrorflip information
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
    .i2c_write_id = 0x20,//record current sensor's i2c write id
};

/* Sensor output window information */
static  struct SENSOR_WINSIZE_INFO_STRUCT imgsensor_winsize_info[5]=

{{ 3264, 2448,      0,    0,      3264, 2448,      1632, 1224,      0,   0,      1632, 1224,      0,    0, 1632,  1224},  // Preview
 { 3264, 2448,      0,    0,      3264, 2448,      3264, 2448,      0,   0,      3264, 2448,      0,    0, 3264,  2448},  // capture
 { 3264, 2448,      0,    0,      3264, 2448,      3264, 2448,      0,   0,      3264, 2448,      0,    0, 3264,  2448},  //video
 { 3264, 2448,      0,    0,      3264, 2448,      3264, 2448,      0,   0,      3264, 2448,      0,    0, 3264,  2448},   //high speed video
 { 3264, 2448,      0,    0,      3264, 2448,      3264, 2448,      0,   0,      3264, 2448,      0,    0, 3264,  2448}};  //slim video 

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
    kal_uint32 frame_length = imgsensor.frame_length;

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

    LOG_DBG("C8496 set_max_framerate():framerate = %d, min framelength should enable = %d\n", framerate,min_framelength_en);
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
	kal_uint32 real_shutter;
    spin_lock_irqsave(&imgsensor_drv_lock, flags);
    imgsensor.shutter = shutter;
    spin_unlock_irqrestore(&imgsensor_drv_lock, flags);
    // if shutter bigger than frame_length, should extend frame length first
    spin_lock(&imgsensor_drv_lock);

	frame_delay_ratio = ratio; //ratio delay 1 frame
	
    if (shutter > imgsensor.min_frame_length - imgsensor_info.margin)
        imgsensor.frame_length = shutter + imgsensor_info.margin;
    else
        imgsensor.frame_length = imgsensor.min_frame_length;

    if (imgsensor.frame_length > imgsensor_info.max_frame_length)
        imgsensor.frame_length = imgsensor_info.max_frame_length;
    spin_unlock(&imgsensor_drv_lock);

    //LOG_DBG("C8496 set_shutter(): shutter =%d\n", shutter);

    shutter = (shutter < imgsensor_info.min_shutter) ? imgsensor_info.min_shutter : shutter;
    shutter = (shutter > (imgsensor_info.max_frame_length - imgsensor_info.margin)) ? (imgsensor_info.max_frame_length - imgsensor_info.margin) : shutter;

	shutter = shutter *2; //C8496 support half line exp, shutter should *2
    if(shutter% 4 == 0) //shutter has to be a multiple of 4!!!
    {
        ratio = 64;
    }else{
        ratio = shutter*64/(shutter-(shutter%4));
    }
	real_shutter = shutter-(shutter%4);

    // Update Shutter
    write_cmos_sensor(0x0340, imgsensor.frame_length >> 8);
    write_cmos_sensor(0x0341, imgsensor.frame_length & 0xFF);
    
    write_cmos_sensor(0x0202, (real_shutter>> 8) & 0xFF);
    write_cmos_sensor(0x0203, (real_shutter) & 0xFF);
   

    LOG_DBG("CST_004:C8496 set_shutter(): ratio = %d, real_shutter =%d, framelength =%d,mini_framelegth=%d\n",ratio,real_shutter,imgsensor.frame_length,imgsensor.min_frame_length);
}
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
// +BUGP231202-01894 ,lijiazhen2.wt,MODIFY,20231220, Fixed c8496 video mode HD switch FHD flicker.
static kal_uint32 reg0_bak = 0xff;
static kal_uint32 reg1_bak = 0xff;
static kal_uint32 reg2_bak = 0xff;
static kal_uint32 reg3_bak = 0xff;
static kal_uint32 save_flag = 0xff;
// -BUGP231202-01894 ,lijiazhen2.wt,MODIFY,20231220, Fixed c8496 video mode HD switch FHD flicker.

#define AGAIN_NUM    65
    static    kal_uint16    again_table[AGAIN_NUM] =
    {
     //	0 	 	1 	 	2 	 	3 	 	4 	 	5 	 	6 	 	7 	 	8 	 	9 	 	10 	 	11 	 	12 	 	13 	 	14 	 	15
		64,		68,		72,		76,		80,		84,		88,		92,		96,		100,	104,	108,	112,	116,	120,	124,
		128,	136,	144,	152,	160,	168,	176,	184,	192,	200,	208,	216,	224,	232,	240,	248,
		256,	272,	288,	304,	320,	336,	352,	368,	384,	400,	416,	432,	448,	464,	480,	496,
		512,	544,	576,	608,	640,	672,	704,	736,	768,	800,	832,	864,	896,	928,	960,	992,
		1024,
    };
    static    kal_uint16    again_register_table_0[AGAIN_NUM] =
    {
     //	0		1		2		3		4		5		6		7		8		9		10		11		12		13		14		15
		0x07,	0x07,	0x07,	0x07,   0x07,   0x07,   0x07,   0x07,   0x07,   0x07,   0x07,   0x07,   0x07,   0x07,   0x07,   0x07,
		0x03,	0x03,	0x03,	0x03,	0x03,	0x03,	0x03,	0x03,	0x03,	0x03,	0x03,	0x03,	0x03,	0x03,	0x03,	0x03,
		0x01,	0x01,	0x01,	0x01,	0x01,	0x01,	0x01,	0x01,	0x01,	0x01,	0x01,	0x01,	0x01,	0x01,	0x01,	0x01,
		0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,
		0x00,
    }; //0x3293
    static    kal_uint16  again_register_table_1[AGAIN_NUM] =
    {
     //	0		1		2		3		4		5		6		7		8		9		10		11		12		13		14		15
   		0x10,	0x11,	0x12,	0x13,   0x14,   0x15,   0x16,   0x17,   0x18,   0x19,   0x1a,   0x1b,   0x1c,   0x1d,   0x1e,   0x1f,
		0x10,	0x11,	0x12,	0x13,   0x14,   0x15,   0x16,   0x17,   0x18,   0x19,   0x1a,   0x1b,   0x1c,   0x1d,   0x1e,   0x1f,
		0x10,	0x11,	0x12,	0x13,   0x14,   0x15,   0x16,   0x17,   0x18,   0x19,   0x1a,   0x1b,   0x1c,   0x1d,   0x1e,   0x1f,
		0x10,	0x11,	0x12,	0x13,   0x14,   0x15,   0x16,   0x17,   0x18,   0x19,   0x1a,   0x1b,   0x1c,   0x1d,   0x1e,   0x1f,
		0x20,
    };//0x32a9
    static  kal_uint16  again_register_table_2[AGAIN_NUM] =
	{
	//	0		1		2		3		4		5		6		7		8		9		10		11		12		13		14		15
    	0x18,	0x18,	0x18,	0x18,   0x18,   0x18,   0x18,   0x18,   0x18,   0x18,   0x18,   0x18,   0x18,   0x18,   0x18,   0x18,
		0x18,	0x18,	0x18,	0x18,   0x18,   0x18,   0x18,   0x18,   0x18,   0x18,   0x18,	0x18,	0x18,	0x18,	0x18,	0x18,
		0x68,	0x68,	0x68,	0x68,   0x68,   0x68,   0x68,   0x68,   0x18,   0x18,	0x18,	0x18,	0x18,	0x18,	0x18,	0x18,
		0x58,	0x58,	0x58,	0x58,	0x58,	0x58,	0x58,	0x48,	0x48,	0x48,	0x38,	0x38,	0x38,	0x38,	0x08,	0x08,
		0x08,
	}; //0x3286 change by jingbin 2022_11_10 for fpn
	static	kal_uint16  again_register_table_3[AGAIN_NUM] =
	{
	//	0		1		2		3		4		5		6		7		8		9		10		11		12		13		14		15
		0xff,	0xff,	0xff,	0xff,	0xff,	0xff,	0xff,	0xff,	0xff,	0xff,	0xff,	0xff,	0xff,	0xff,	0xff,	0xff,
		0xfd,	0xfd,	0xfd,	0xfd,	0xfd,	0xfd,	0xfd,	0xfd,	0xfd,	0xfd,	0xfd,	0xfd,	0xfd,	0xfd,	0xfd,	0xfd,
		0xfc,	0xfc,	0xfc,	0xfc,	0xfc,	0xfc,	0xfc,	0xfc,	0xfc,	0xfc,	0xfc,	0xfc,	0xfc,	0xfc,	0xfc,	0xfc,
		0xfb,	0xfb,	0xfb,	0xfb,	0xfb,	0xfb,	0xfb,	0xfb,	0xfb,	0xfb,	0xfb,	0xfb,	0xfb,	0xfb,	0xfb,	0xfb,
		0xfb,
	}; //0x32ac change by jingbin 2022_11_22 for gain linearity

	
static kal_uint16 set_gain(kal_uint16 gain)
{
    kal_uint16 tmp0 = 0, tmp1 = 0, tmp2=0, tmp3=0, i = 0, Again_base = 0;
	//kal_uint16 reg3293=0, reg32a9=0, reg3286=0, reg32ac=0;
    kal_uint16 iGain=gain*frame_delay_ratio/64;
    LOG_DBG("CST_000:c8496_drv_write_gain();frame_delay_ratio=%d, gain=0x%x, iGain=0x%x\n", frame_delay_ratio,gain,iGain);

   
    if(iGain >= again_table[AGAIN_NUM-1]){
        iGain = again_table[AGAIN_NUM-1]; //max gain: 16*64
        Again_base = AGAIN_NUM-1;
    }
    if(iGain <= BASEGAIN){
        iGain = again_table[0];           //iGain <= 0x40(=1x)
    }
    for(i=1; i < AGAIN_NUM; i++){
        if(iGain < again_table[i]){
            Again_base = i - 1;
            break;
        }
    }

    tmp0 = again_register_table_0[Again_base]; //0x3293
    tmp1 = again_register_table_1[Again_base]; //0x32a9
	tmp2 = again_register_table_2[Again_base]; //0x3286
	tmp3 = again_register_table_3[Again_base]; //0x32ac
// +BUGP231202-01894 ,lijiazhen2.wt,MODIFY,20231220, Fixed c8496 video mode HD switch FHD flicker.
#if 1	//bakup first Again value
	reg0_bak = tmp0;
	reg1_bak = tmp1;
	reg2_bak = tmp2;
	reg3_bak = tmp3;
#endif
// -BUGP231202-01894 ,lijiazhen2.wt,MODIFY,20231220, Fixed c8496 video mode HD switch FHD flicker.
    write_cmos_sensor(0xe00c,0x32);
    write_cmos_sensor(0xe00d,0x93);
	write_cmos_sensor(0xe00e,tmp0);

	write_cmos_sensor(0xe00f,0x32);
    write_cmos_sensor(0xe010,0xa9);
	write_cmos_sensor(0xe011,tmp1);

	write_cmos_sensor(0xe012,0x32);
    write_cmos_sensor(0xe013,0x86);
	write_cmos_sensor(0xe014,tmp2);

	write_cmos_sensor(0xe015,0x32);
    write_cmos_sensor(0xe016,0xac);
	write_cmos_sensor(0xe017,tmp3);

	write_cmos_sensor(0x340f,0x11); //sensor group delay 1 frame write sensor gain
	
#if  0  //for debug only
		reg3293 = read_cmos_sensor(0x3293);
		reg32a9 = read_cmos_sensor(0x32a9);
		reg3286 = read_cmos_sensor(0x3286);
		reg32ac = read_cmos_sensor(0x32ac);	
		LOG_DBG("CST_002:c8496_drv_write_gain(): reg3293=0x%x, reg32a9=0x%x, reg3286=0x%x, reg32ac=0x%x\n", reg3293, reg32a9, reg3286, reg32ac);
#endif
	
    return 0;
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
}    /*    night_mode    */

static void sensor_init(void)
{
    //v2p0_setting
    write_cmos_sensor(0x0103,0x01);
    write_cmos_sensor(0x0101,0x03);
	write_cmos_sensor(0x3c06,0x01);
	write_cmos_sensor(0x3c01,0x00);
	write_cmos_sensor(0x3c02,0x00);
	write_cmos_sensor(0x3c04,0xc0);
	write_cmos_sensor(0x0340,0x09);
	write_cmos_sensor(0x0341,0xc4);
	write_cmos_sensor(0x0342,0x0f);
	write_cmos_sensor(0x0343,0x10);
	write_cmos_sensor(0x0348,0x0c);
	write_cmos_sensor(0x0349,0xcf);
	write_cmos_sensor(0x034a,0x09);
	write_cmos_sensor(0x034b,0x9f);
	write_cmos_sensor(0x3295,0x11);
	write_cmos_sensor(0x328B,0x11);
	write_cmos_sensor(0x328C,0x69);
	write_cmos_sensor(0x3284,0x14);
	write_cmos_sensor(0x3212,0x30);
	write_cmos_sensor(0x32aa,0x14);
	write_cmos_sensor(0x3285,0x42);
	write_cmos_sensor(0x3298,0x6c);
	write_cmos_sensor(0x0209,0x80);
	write_cmos_sensor(0x3219,0x54);
	write_cmos_sensor(0x0207,0x08);
	write_cmos_sensor(0x3517,0xb8); //+S96818AA1-1936,wuwenhao2.wt,add,2023/07/12, c8496 modify mipi rate to 736M
	write_cmos_sensor(0x3500,0x10); //group access start
	write_cmos_sensor(0x3584,0x02);
	write_cmos_sensor(0x3401,0x01);
	write_cmos_sensor(0x3405,0x04);
	write_cmos_sensor(0xe00c,0x32);
	write_cmos_sensor(0xe00d,0x93);
	write_cmos_sensor(0xe00e,0x07);
	write_cmos_sensor(0xe00f,0x32);
	write_cmos_sensor(0xe010,0xa9);
	write_cmos_sensor(0xe011,0x10);
	write_cmos_sensor(0xe012,0x32);
	write_cmos_sensor(0xe013,0x86);
	write_cmos_sensor(0xe014,0x18);
	write_cmos_sensor(0xe015,0x32);
	write_cmos_sensor(0xe016,0xac);
	write_cmos_sensor(0xe017,0xff);
	write_cmos_sensor(0x3584,0x22);
	write_cmos_sensor(0x3500,0x00); //group access end
	write_cmos_sensor(0x0202,0x11);
	write_cmos_sensor(0x0203,0xa0);
	write_cmos_sensor(0x3293,0x01);
	write_cmos_sensor(0x32a9,0x12);
	write_cmos_sensor(0x3286,0x68);
	write_cmos_sensor(0x32ac,0xfc);
	write_cmos_sensor(0x3180,0x06); //mirror flip on
	write_cmos_sensor(0x3120,0x22); //blc refine
	write_cmos_sensor(0x3121,0xa6);
	write_cmos_sensor(0x3215,0x10);
	write_cmos_sensor(0x323a,0x1d);
	write_cmos_sensor(0x3287,0x28);
	write_cmos_sensor(0x380e,0x31);
	write_cmos_sensor(0x3c87,0x00);
	write_cmos_sensor(0x32ae,0x44);
	write_cmos_sensor(0x32ad,0x00);
	write_cmos_sensor(0x3904,0x02);
	write_cmos_sensor(0x3122,0x40); //first 2 frame blc on
	save_flag = 0xa5;
}

static void preview_setting(kal_uint16 currefps)
{
    //binning
	write_cmos_sensor(0x034c,0x06);
	write_cmos_sensor(0x034d,0x60);
	write_cmos_sensor(0x034e,0x04);
	write_cmos_sensor(0x034f,0xc8);
	write_cmos_sensor(0x3021,0x22);
	write_cmos_sensor(0x3022,0x01);
	write_cmos_sensor(0x0387,0x03);
	write_cmos_sensor(0x3210,0x1a);
	write_cmos_sensor(0x3514,0x0a);
	write_cmos_sensor(0x3881,0x08);
	write_cmos_sensor(0x3805,0x05);
	write_cmos_sensor(0x3806,0x03);
	write_cmos_sensor(0x3807,0x03);
	write_cmos_sensor(0x3808,0x14);
	write_cmos_sensor(0x3809,0x85);
	write_cmos_sensor(0x380a,0x6d);
	write_cmos_sensor(0x380b,0xaa);
}

static void capture_setting(kal_uint16 currefps)
{
	write_cmos_sensor(0x034c,0x0c);
	write_cmos_sensor(0x034d,0xc0);
	write_cmos_sensor(0x034e,0x09);
	write_cmos_sensor(0x034f,0x90);
	write_cmos_sensor(0x3021,0x1d);
	write_cmos_sensor(0x3022,0x01);
	write_cmos_sensor(0x0387,0x01);
	write_cmos_sensor(0x3210,0x12);
	write_cmos_sensor(0x3514,0x06);
	write_cmos_sensor(0x3881,0x18);
	write_cmos_sensor(0x3805,0x08);
	write_cmos_sensor(0x3806,0x06);
	write_cmos_sensor(0x3807,0x06);
	write_cmos_sensor(0x3808,0x19);
	write_cmos_sensor(0x3809,0x96);
	write_cmos_sensor(0x380a,0x8d);
	write_cmos_sensor(0x380b,0xaa);
}

static void normal_video_setting(kal_uint16 currefps)
{
    capture_setting(currefps);
}

static void hs_video_setting(kal_uint16 currefps)
{
    capture_setting(currefps);
}

static void slim_video_setting(kal_uint16 currefps)
{
   capture_setting(currefps);
}

static kal_uint32 set_test_pattern_mode(kal_bool enable)
{
    spin_lock(&imgsensor_drv_lock);
    spin_unlock(&imgsensor_drv_lock);
    return ERROR_NONE;
}
#if C8496_OTP_FUNCTION
//+S96818AA1-1936,zhujianjia.wt,ADD,2023/04/28,c8496 otp porting
static void c8496_otp_init(void)
{
	write_cmos_sensor(0x0100, 0x00);
	write_cmos_sensor(0x318F, 0x04);
	write_cmos_sensor(0x3584, 0x02);
	write_cmos_sensor(0x3A81, 0x00);
	write_cmos_sensor(0x3A80, 0x00);
	write_cmos_sensor(0x3A8C, 0x04);
	write_cmos_sensor(0x3A8D, 0x02);
	write_cmos_sensor(0x3AA0, 0x03);
	write_cmos_sensor(0x3A80, 0x08);
	write_cmos_sensor(0x3A90, 0x01);
	write_cmos_sensor(0x3A93, 0x01);
	write_cmos_sensor(0x3A94, 0x60);	
}
static void c8496_otp_close(void)
{
	write_cmos_sensor(0x318f, 0x00);//Exit read mode
	write_cmos_sensor(0x3584, 0x22);
	write_cmos_sensor(0x3A81, 0x00);
	write_cmos_sensor(0x3A80, 0x00);
	write_cmos_sensor(0x3A93, 0x00);
	write_cmos_sensor(0x3A94, 0x00);
	write_cmos_sensor(0x3A90, 0x00);
}
static u16 c8496_otp_read_group(u16 addr, u8 *data, u16 length)
{
    u16 i = 0;
	write_cmos_sensor(0x3A80, 0x88);//enter read mode
	write_cmos_sensor(0x3A81, 0x02);
	mdelay(10);
	for (i = 0; i < length; i++) {
		data[i] = read_cmos_sensor(addr+i);
		udelay(500);
	    //LOG_INF("addr = 0x%x, data = 0x%x\n", addr + i * 8, data[i]);
	}
	return 0;
}
static kal_uint16 read_cmos_sensor_otp(u16 addr)
{

	write_cmos_sensor(0x3A80, 0x88);//enter read mode
	write_cmos_sensor(0x3A81, 0x02);
	mdelay(10);
	return read_cmos_sensor(addr);
}

static int read_c8496_module_info(void)
{
	int otp_grp_flag = 0, minfo_start_addr = 0;
	int year = 0, month = 0, day = 0;
	int position = 0,lens_id = 0,vcm_id = 0,c8496_module_id = 0;
	int check_sum = 0, check_sum_cal = 0;
	int i;

	otp_grp_flag = read_cmos_sensor_otp(MODULE_INFO_FLAG);
	mdelay(1);
	LOG_INF("module info otp_grp_flag = 0x%x",otp_grp_flag);

	/* select group */
	if(otp_grp_flag == 0x1f)
		minfo_start_addr = MODULE_GROUP1_ADDR;
	else if(otp_grp_flag == 0x07)
		minfo_start_addr = MODULE_GROUP2_ADDR;
	else if(otp_grp_flag == 0x01)
		minfo_start_addr = MODULE_GROUP3_ADDR;
	else {
		LOG_INF("no module info OTP c8496_data_info\n");
		return 0;
	}

	c8496_otp_read_group(minfo_start_addr,c8496_data_info,MODULE_INFO_SIZE + 1);

	for(i = 0; i <  MODULE_INFO_SIZE ; i++){
		check_sum_cal += c8496_data_info[i];
	}

	check_sum_cal = (check_sum_cal % 255) + 1;
	c8496_module_id = c8496_data_info[0];
	position = c8496_data_info[1];
	lens_id = c8496_data_info[2];
	vcm_id = c8496_data_info[3];
	year = c8496_data_info[4];
	month = c8496_data_info[5];
	day = c8496_data_info[6];
	check_sum = c8496_data_info[MODULE_INFO_SIZE];

	//LOG_INF("module_id=0x%x position=0x%x\n", c8496_module_id, position);
	LOG_INF("=== C8496 INFO module_id=0x%x position=0x%x ===\n", c8496_module_id, position);
	LOG_INF("=== C8496 INFO lens_id=0x%x,vcm_id=0x%x ===\n",lens_id, vcm_id);
	LOG_INF("=== C8496 INFO date is %d-%d-%d ===\n",year,month,day);
	LOG_INF("=== C8496 INFO check_sum=0x%x,check_sum_cal=0x%x ===\n", check_sum, check_sum_cal);
	if(check_sum == check_sum_cal){
		LOG_INF("=== C8496 INFO date sucess!\n");
		return 1;
	}else{
		return 0;
	}
}

static int read_c8496_sn_info(void)
{
	int otp_grp_flag = 0, minfo_start_addr = 0;
	int check_sum = 0, check_sum_cal = 0;
	int i;


	otp_grp_flag = read_cmos_sensor_otp(SN_INFO_FLAG);
	LOG_INF("sn info otp_grp_flag = 0x%x",otp_grp_flag);

	/* select group */
	if(otp_grp_flag == 0x1f)
		minfo_start_addr = SN_GROUP1_ADDR;
	else if(otp_grp_flag == 0x07)
		minfo_start_addr = SN_GROUP2_ADDR;
	else if(otp_grp_flag == 0x01)
		minfo_start_addr = SN_GROUP3_ADDR;
	else {
		LOG_INF("no sn info OTP gc08a3_data_info\n");
		return 0;
	}

	c8496_otp_read_group(minfo_start_addr,c8496_data_sn,SN_INFO_SIZE + 1);

	for(i = 0; i <  SN_INFO_SIZE ; i++){
		check_sum_cal += c8496_data_sn[i];
		LOG_INF("sn info c8496_data_sn[%d] = 0x%x",i,c8496_data_sn[i]);
	}
	check_sum_cal = (check_sum_cal % 255) + 1;
	check_sum=c8496_data_sn[SN_INFO_SIZE];
	LOG_INF("=== C8496 SN check_sum=0x%x,check_sum_cal=0x%x ===\n", check_sum, check_sum_cal);
	if(check_sum == check_sum_cal){
		LOG_INF("=== C8496 SN date sucess!\n");
		return 1;
	}else{
		return 0;
	}
}
static int read_c8496_awb_info(void)
{
	int awb_grp_flag = 0, awb_start_addr=0;
	int check_sum_awb = 0, check_sum_awb_cal = 0;
	int r = 0,b = 0,gr = 0, gb = 0, golden_r = 0, golden_b = 0, golden_gr = 0, golden_gb = 0;
	int i;

	/* read flag */
	awb_grp_flag = read_cmos_sensor_otp(AWB_INFO_FLAG);
	LOG_INF("awb awb_grp_flag = 0x%x",awb_grp_flag);

	/* select group */
	if(awb_grp_flag == 0x1f)
		awb_start_addr = AWB_GROUP1_FLAG;
	else if(awb_grp_flag == 0x07)
		awb_start_addr = AWB_GROUP2_FLAG;
	else if(awb_grp_flag == 0x01)
		awb_start_addr = AWB_GROUP3_FLAG;
	else {
		LOG_INF("no awb OTP c8496_data_info\n");
		return 0;
	}

	/* read & checksum */
	//check_sum_awb_cal += awb_grp_flag;

	c8496_otp_read_group(awb_start_addr,c8496_data_awb,AWB_INFO_SIZE + 1);
	for(i = 0; i < AWB_INFO_SIZE; i++){
		check_sum_awb_cal += c8496_data_awb[i];
	}
	LOG_INF("check_sum_awb_cal =0x%x \n",check_sum_awb_cal);
#if 0
    //old module r gr gb b
	r = ((gc08a3_data_awb[1]<<8)&0xff00)|(gc08a3_data_awb[0]&0xff);
	gr = ((gc08a3_data_awb[3]<<8)&0xff00)|(gc08a3_data_awb[2]&0xff);
	gb = ((gc08a3_data_awb[5]<<8)&0xff00)|(gc08a3_data_awb[4]&0xff);
	b = ((gc08a3_data_awb[7]<<8)&0xff00)|(gc08a3_data_awb[6]&0xff);
	golden_r = ((gc08a3_data_awb[9]<<8)&0xff00)|(gc08a3_data_awb[8]&0xff);
	golden_gr = ((gc08a3_data_awb[11]<<8)&0xff00)|(gc08a3_data_awb[10]&0xff);
	golden_gb = ((gc08a3_data_awb[13]<<8)&0xff00)|(gc08a3_data_awb[12]&0xff);
	golden_b = ((gc08a3_data_awb[15]<<8)&0xff00)|(gc08a3_data_awb[14]&0xff);
	check_sum_awb = gc08a3_data_awb[AWB_INFO_SIZE];
	check_sum_awb_cal = (check_sum_awb_cal % 255) + 1;
#else
   //new module r b gr gb
	r = ((c8496_data_awb[1]<<8)&0xff00)|(c8496_data_awb[0]&0xff);
	b = ((c8496_data_awb[3]<<8)&0xff00)|(c8496_data_awb[2]&0xff);
	gr = ((c8496_data_awb[5]<<8)&0xff00)|(c8496_data_awb[4]&0xff);
	gb = ((c8496_data_awb[7]<<8)&0xff00)|(c8496_data_awb[6]&0xff);
	golden_r = ((c8496_data_awb[9]<<8)&0xff00)|(c8496_data_awb[8]&0xff);
	golden_b = ((c8496_data_awb[11]<<8)&0xff00)|(c8496_data_awb[10]&0xff);
	golden_gr = ((c8496_data_awb[13]<<8)&0xff00)|(c8496_data_awb[12]&0xff);
	golden_gb = ((c8496_data_awb[15]<<8)&0xff00)|(c8496_data_awb[14]&0xff);
	check_sum_awb = c8496_data_awb[AWB_INFO_SIZE];
	check_sum_awb_cal = (check_sum_awb_cal % 255) + 1;

#endif

	LOG_INF("=== c8496 AWB r=0x%x, b=0x%x, gr=%x, gb=0x%x ===\n", r, b,gb, gr);
	LOG_INF("=== c8496 AWB gr=0x%x,gb=0x%x,gGr=%x, gGb=0x%x ===\n", golden_r, golden_b, golden_gr, golden_gb);
	LOG_INF("=== c8496 AWB check_sum_awb=0x%x,check_sum_awb_cal=0x%x ===\n",check_sum_awb,check_sum_awb_cal);
	if(check_sum_awb == check_sum_awb_cal){
		LOG_INF("=== c8496 AWB date sucess!\n");
		return 1;
	}else{
		return 0;
	}
}

static int read_c8496_lsc_info(void)
{
	int lsc_grp_flag = 0, lsc_start_addr = 0;
	int check_sum_lsc = 0, check_sum_lsc_cal = 0;
	int i;

	/* read flag */
	lsc_grp_flag = read_cmos_sensor_otp(LSC_INFO_FLAG);//1948
	LOG_INF("lsc lsc_grp_flag = 0x%x",lsc_grp_flag);

	/* select group */
	if(lsc_grp_flag == 0x1f)
		lsc_start_addr = LSC_GROUP1_FLAG;//1950
	else if(lsc_grp_flag == 0x07)
		lsc_start_addr = LSC_GROUP2_FLAG;//53b8
	else if(lsc_grp_flag == 0x01)
		lsc_start_addr = LSC_GROUP3_FLAG;//8E20
	else {
		LOG_INF("no lsc OTP c8496_data_info\n");
		return 0;
	}

	/* read & checksum */
	//check_sum_lsc_cal += lsc_grp_flag;

	c8496_otp_read_group(lsc_start_addr,c8496_data_lsc,LSC_INFO_SIZE + 1);

	for(i = 0; i < LSC_INFO_SIZE; i++){
		check_sum_lsc_cal += c8496_data_lsc[i];
	}
	LOG_INF("check_sum_lsc_cal =0x%x \n",check_sum_lsc_cal);
	check_sum_lsc = c8496_data_lsc[LSC_INFO_SIZE];
	check_sum_lsc_cal = (check_sum_lsc_cal % 255) + 1;

	LOG_INF("=== c8496 LSC check_sum_lsc=0x%x, check_sum_lsc_cal=0x%x ===\n", check_sum_lsc, check_sum_lsc_cal);
	if(check_sum_lsc == check_sum_lsc_cal){
		LOG_INF("=== c8496 LSC date sucess!\n");
		return 1;
	}else{
		return 0;
	}
}
static int c8496_sensor_otp_info(){
	int ret = 0, c8496_module_valid = 0, c8496_sn_valid = 0, c8496_awb_valid = 0,c8496_lsc_valid = 0;
	LOG_INF("come to %s:%d E!\n", __func__, __LINE__);

	c8496_otp_init();
	ret = read_c8496_module_info();
	if(ret != 1){
		c8496_module_valid = 0;
		LOG_INF("=== c8496_data_info invalid ===\n");
	}else{
		c8496_module_valid = 1;
	}
	ret = read_c8496_sn_info();
	if(ret != 1){
		c8496_sn_valid = 0;
		LOG_INF("=== c8496_data_sn invalid ===\n");
	}else{
		c8496_sn_valid = 1;
	}
	ret = read_c8496_awb_info();
	if(ret != 1){
		c8496_awb_valid = 0;
		LOG_INF("=== c8496_data_awb invalid ===\n");
	}else{
		c8496_awb_valid = 1;
	}
	ret = read_c8496_lsc_info();
	if(ret != 1){
		c8496_lsc_valid = 0;
		LOG_INF("=== c8496_data_lsc invalid ===\n");
	}else{
		c8496_lsc_valid = 1;
	}

	c8496_otp_close();
	if(c8496_module_valid == 0 || c8496_awb_valid == 0 || c8496_lsc_valid == 0){
		return -1;
	}else{
		return 0;
	}
}


#include "cam_cal_define.h"
#include <linux/slab.h>
static struct stCAM_CAL_DATAINFO_STRUCT n28c8496frontdc_eeprom_data ={
    .sensorID= N28C8496FRONTDC_SENSOR_ID,
    .deviceID = 0x02,
    .dataLength = 0x076F,//1912
    .sensorVendorid = 0x13050001,
    .vendorByte = {1,2,3,4},
    .dataBuffer = NULL,
};

extern int imgSensorSetEepromData(struct stCAM_CAL_DATAINFO_STRUCT* pData);

//-S96818AA1-1936,zhujianjia.wt,ADD,2023/04/28,c8496 otp porting
#endif
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
#if C8496_OTP_FUNCTION
    kal_uint8 ret = 0;
#endif
    while (imgsensor_info.i2c_addr_table[i] != 0xff) {
        spin_lock(&imgsensor_drv_lock);
        imgsensor.i2c_write_id = imgsensor_info.i2c_addr_table[i];
        spin_unlock(&imgsensor_drv_lock);
        do {
            *sensor_id = ((read_cmos_sensor(0x0000) << 8) | read_cmos_sensor(0x0001));
            if (*sensor_id == imgsensor_info.sensor_id) {
                LOG_ERR("C8496 get_imgsensor_id OK,i2c write id: 0x%X, sensor id: 0x%X\n", imgsensor.i2c_write_id,*sensor_id);
#if C8496_OTP_FUNCTION
//+S96818AA1-1936,zhujianjia.wt,ADD,2023/04/28,c8496 otp porting
            ret = c8496_sensor_otp_info();
            if(ret != 0){
                *sensor_id = 0xFFFFFFFF;
                LOG_INF("n28c8496frontdc read OTP failed ret:%d, \n",ret);
                return ERROR_SENSOR_CONNECT_FAIL;
                }else{
                LOG_INF("n28c8496frontdc read OTP successed: ret: %d \n",ret);
                n28c8496frontdc_eeprom_data.dataBuffer = kmalloc(n28c8496frontdc_eeprom_data.dataLength, GFP_KERNEL);
                 if (n28c8496frontdc_eeprom_data.dataBuffer == NULL) {
                     LOG_INF("n28c8496frontdc_eeprom_data->dataBuffer is malloc fail\n");
                     return -EFAULT;
                }
                memcpy(n28c8496frontdc_eeprom_data.dataBuffer, (u8 *)&c8496_data_info, MODULE_INFO_SIZE);
                memcpy(n28c8496frontdc_eeprom_data.dataBuffer+MODULE_INFO_SIZE, (u8 *)&c8496_data_sn, SN_INFO_SIZE);
                memcpy(n28c8496frontdc_eeprom_data.dataBuffer+SN_INFO_SIZE+MODULE_INFO_SIZE, (u8 *)&c8496_data_awb, AWB_INFO_SIZE);
                memcpy(n28c8496frontdc_eeprom_data.dataBuffer+SN_INFO_SIZE+MODULE_INFO_SIZE+AWB_INFO_SIZE, (u8 *)&c8496_data_lsc, LSC_INFO_SIZE);
                imgSensorSetEepromData(&n28c8496frontdc_eeprom_data);
             }
//-S96818AA1-1936,zhujianjia.wt,ADD,2023/04/28,c8496 otp porting
#endif
                return ERROR_NONE;
            }
            LOG_ERR("C8496 get_imgsensor_id fail, write id: 0x%X, id: 0x%X\n", imgsensor.i2c_write_id,*sensor_id);
            retry--;
        } while(retry > 0);
        i++;
        retry = 2;
    }
    if (*sensor_id != imgsensor_info.sensor_id) {
        // if Sensor ID is not correct, Must set *sensor_id to 0xFFFFFFFF
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
    while (imgsensor_info.i2c_addr_table[i] != 0xff) {
        spin_lock(&imgsensor_drv_lock);
        imgsensor.i2c_write_id = imgsensor_info.i2c_addr_table[i];
        spin_unlock(&imgsensor_drv_lock);
        do {
            sensor_id = ((read_cmos_sensor(0x0000) << 8) | read_cmos_sensor(0x0001));
            if (sensor_id == imgsensor_info.sensor_id) {
                LOG_DBG("C8496 open:Read sensor id OK,i2c write id: 0x%X, sensor id: 0x%X\n", imgsensor.i2c_write_id,sensor_id);
                break;
            }
            LOG_DBG("C8496 Read sensor id fail, write id: 0x%X, id: 0x%X\n", imgsensor.i2c_write_id,sensor_id);
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
    spin_lock(&imgsensor_drv_lock);
    imgsensor.sensor_mode = IMGSENSOR_MODE_PREVIEW;
    imgsensor.autoflicker_en = KAL_FALSE;

    imgsensor.pclk= imgsensor_info.pre.pclk;
    imgsensor.line_length=imgsensor_info.pre.linelength;
    imgsensor.frame_length=imgsensor_info.pre.framelength;

    imgsensor.min_frame_length = imgsensor_info.pre.framelength;

    spin_unlock(&imgsensor_drv_lock);

    preview_setting(imgsensor.current_fps);
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
    spin_lock(&imgsensor_drv_lock);
    imgsensor.sensor_mode = IMGSENSOR_MODE_CAPTURE;

    imgsensor.pclk= imgsensor_info.cap.pclk;
    imgsensor.line_length=imgsensor_info.cap.linelength;
    imgsensor.frame_length=imgsensor_info.cap.framelength;

    imgsensor.min_frame_length = imgsensor_info.cap.framelength;
    spin_unlock(&imgsensor_drv_lock);

    capture_setting(imgsensor.current_fps);
    return ERROR_NONE;
}    /* capture() */
static kal_uint32 normal_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
                      MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
    spin_lock(&imgsensor_drv_lock);
    imgsensor.sensor_mode = IMGSENSOR_MODE_VIDEO;
    imgsensor.autoflicker_en = KAL_FALSE;

    imgsensor.pclk= imgsensor_info.normal_video.pclk;
    imgsensor.line_length=imgsensor_info.normal_video.linelength;
    imgsensor.frame_length=imgsensor_info.normal_video.framelength;

    imgsensor.min_frame_length = imgsensor_info.normal_video.framelength;
    spin_unlock(&imgsensor_drv_lock);

    normal_video_setting(imgsensor.current_fps);
    return ERROR_NONE;
}    /*    normal_video   */

static kal_uint32 hs_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
                      MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
    spin_lock(&imgsensor_drv_lock);
    imgsensor.sensor_mode = IMGSENSOR_MODE_HIGH_SPEED_VIDEO;
    imgsensor.autoflicker_en = KAL_FALSE;
    imgsensor.pclk= imgsensor_info.hs_video.pclk;
    imgsensor.line_length=imgsensor_info.hs_video.linelength;
    imgsensor.frame_length=imgsensor_info.hs_video.framelength;
    imgsensor.min_frame_length = imgsensor_info.hs_video.framelength;
    spin_unlock(&imgsensor_drv_lock);
    hs_video_setting(imgsensor.current_fps);

    return ERROR_NONE;
}    /*    hs_video   */

static kal_uint32 slim_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
                      MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
    spin_lock(&imgsensor_drv_lock);
    imgsensor.sensor_mode = IMGSENSOR_MODE_SLIM_VIDEO;
    imgsensor.autoflicker_en = KAL_FALSE;

    imgsensor.pclk= imgsensor_info.slim_video.pclk;
    imgsensor.line_length=imgsensor_info.slim_video.linelength;
    imgsensor.frame_length=imgsensor_info.slim_video.framelength;

    imgsensor.min_frame_length = imgsensor_info.slim_video.framelength;
    spin_unlock(&imgsensor_drv_lock);
    slim_video_setting(imgsensor.current_fps);

    return ERROR_NONE;
}    /*    slim_video     */

static kal_uint32 get_resolution(MSDK_SENSOR_RESOLUTION_INFO_STRUCT *sensor_resolution)
{
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
}    /*    get_resolution    */

static kal_uint32 get_info(enum MSDK_SCENARIO_ID_ENUM scenario_id,
                      MSDK_SENSOR_INFO_STRUCT *sensor_info,
                      MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
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

    sensor_info->AEShutDelayFrame = imgsensor_info.ae_shut_delay_frame;          /* The frame of setting shutter default 0 for TG int */
    sensor_info->AESensorGainDelayFrame = imgsensor_info.ae_sensor_gain_delay_frame;    /* The frame of setting sensor gain */
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
        default:
            sensor_info->SensorGrabStartX = imgsensor_info.pre.startx;
            sensor_info->SensorGrabStartY = imgsensor_info.pre.starty;
            sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount = imgsensor_info.pre.mipi_data_lp2hs_settle_dc;
            break;
    }

    return ERROR_NONE;
}    /*    get_info  */

static kal_uint32 control(enum MSDK_SCENARIO_ID_ENUM scenario_id, MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
                      MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
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
            preview(image_window, sensor_config_data);
            return ERROR_INVALID_SCENARIO_ID;
    }

    return ERROR_NONE;
}    /* control() */

static kal_uint32 set_video_mode(UINT16 framerate)
{
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
    spin_lock(&imgsensor_drv_lock);
    if (enable) //enable auto flicker
        imgsensor.autoflicker_en = KAL_TRUE;
    else //Cancel Auto flick
        imgsensor.autoflicker_en = KAL_FALSE;
    spin_unlock(&imgsensor_drv_lock);

    return ERROR_NONE;
}

static kal_uint32 set_max_framerate_by_scenario(enum MSDK_SCENARIO_ID_ENUM scenario_id, MUINT32 framerate)
{
    //kal_uint32 frame_length;
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

        default:  //coding with  preview scenario by default
            imgsensor.pclk= imgsensor_info.pre.pclk;
            imgsensor.line_length=imgsensor_info.pre.linelength;
            imgsensor.frame_length=imgsensor_info.pre.framelength;
            imgsensor.current_fps=framerate;
            imgsensor.min_frame_length = imgsensor_info.pre.framelength;
            break;
    }

    set_max_framerate(imgsensor.current_fps,1);

    return ERROR_NONE;
}

static kal_uint32 get_default_framerate_by_scenario(enum MSDK_SCENARIO_ID_ENUM scenario_id, MUINT32 *framerate)
{
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
    if (enable) {
        write_cmos_sensor(0x0100, 0X01); // stream on
// +BUGP231202-01894 ,lijiazhen2.wt,MODIFY,20231220, Fixed c8496 video mode HD switch FHD flicker.
#if 1	//20221219
	if(save_flag == 0xa5)
	{
		save_flag = 0;
		write_cmos_sensor(0xe00c,0x32);
		write_cmos_sensor(0xe00d,0x93);
		write_cmos_sensor(0xe00e,reg0_bak);

		write_cmos_sensor(0xe00f,0x32);
		write_cmos_sensor(0xe010,0xa9);
		write_cmos_sensor(0xe011,reg1_bak);

		write_cmos_sensor(0xe012,0x32);
		write_cmos_sensor(0xe013,0x86);
		write_cmos_sensor(0xe014,reg2_bak);

		write_cmos_sensor(0xe015,0x32);
		write_cmos_sensor(0xe016,0xac);
		write_cmos_sensor(0xe017,reg3_bak);

		write_cmos_sensor(0x340f,0x11);
	}
#endif
// -BUGP231202-01894 ,lijiazhen2.wt,MODIFY,20231220, Fixed c8496 video mode HD switch FHD flicker.
    } else {
        write_cmos_sensor(0x0100, 0X00); // stream off
    }
    mdelay(2);
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

    struct SENSOR_WINSIZE_INFO_STRUCT *wininfo;
    MSDK_SENSOR_REG_INFO_STRUCT *sensor_reg_data=(MSDK_SENSOR_REG_INFO_STRUCT *) feature_para;
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
        //*(feature_data + 2) = imgsensor_info.exp_step;
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
    case SENSOR_FEATURE_SET_SHUTTER_FRAME_TIME:
        //set_shutter_frame_length(
            //(UINT16) *feature_data, (UINT16) *(feature_data + 1));
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
        case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
        default:
            memcpy((void *)wininfo,
                (void *)&imgsensor_winsize_info[0],
                sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
        break;
        }
    break;
    case SENSOR_FEATURE_SET_IHDR_SHUTTER_GAIN:
        LOG_INF("SENSOR_SET_SENSOR_IHDR LE=%d, SE=%d, Gain=%d\n",
            (UINT16)*feature_data, (UINT16)*(feature_data+1),
            (UINT16)*(feature_data+2));
        //   ihdr_write_shutter_gain((UINT16)*feature_data,
            //    (UINT16)*(feature_data+1),
            //    (UINT16)*(feature_data+2));
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
            case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
            default:
                *(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
                    imgsensor_info.pre.mipi_pixel_rate;
                break;
            }
    break;
#ifdef FPT_PDAF_SUPPORT
/******************** PDAF START ********************/
    case SENSOR_FEATURE_GET_SENSOR_PDAF_CAPACITY:
        switch (*feature_data) {
        case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
            *(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 0;
            break;
        case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
        case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
        case MSDK_SCENARIO_ID_SLIM_VIDEO:
        case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
            *(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 1;
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
        case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
        case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
        case MSDK_SCENARIO_ID_SLIM_VIDEO:
        case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
            memcpy((void *)PDAFinfo, (void *)&imgsensor_pd_info,
                sizeof(struct SET_PD_BLOCK_INFO_T));
            break;
        default:
            break;
        }
        break;
    case SENSOR_FEATURE_GET_VC_INFO:
        LOG_DBG("SENSOR_FEATURE_GET_VC_INFO %d\n",
            (UINT16) *feature_data);
        pvcinfo =
        (struct SENSOR_VC_INFO_STRUCT *) (uintptr_t) (*(feature_data + 1));

        switch (*feature_data_32) {
        case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
            LOG_DBG("Jesse+ CAPTURE_JPEG \n");
            memcpy((void *)pvcinfo, (void *)&SENSOR_VC_INFO[1],
                   sizeof(struct SENSOR_VC_INFO_STRUCT));
            break;
        case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
            LOG_DBG("Jesse+ VIDEO_PREVIEW \n");
            memcpy((void *)pvcinfo, (void *)&SENSOR_VC_INFO[2],
                   sizeof(struct SENSOR_VC_INFO_STRUCT));
            break;
        case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
        default:
            LOG_DBG("Jesse+ CAMERA_PREVIEW \n");
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
    //+bug 558061,zhanglinfeng.wt, modify, 2020/07/02, modify codes for factory mode of photo black screen
    case SENSOR_FEATURE_GET_FRAME_CTRL_INFO_BY_SCENARIO:
        /*
        * 1, if driver support new sw frame sync
        * set_shutter_frame_length() support third para auto_extend_en
        */
        *(feature_data + 1) = 1;
        /* margin info by scenario */
        *(feature_data + 2) = imgsensor_info.margin;
        break;
    //-bug 558061,zhanglinfeng.wt, modify, 2020/07/02, modify codes for factory mode of photo black screen
#endif
    case SENSOR_FEATURE_SET_STREAMING_SUSPEND:
        streaming_control(KAL_FALSE);
        break;

    case SENSOR_FEATURE_SET_STREAMING_RESUME:
        if (*feature_data != 0)
            set_shutter(*feature_data);
        streaming_control(KAL_TRUE);
        break;
    //+bug 558061, zhanglinfeng.wt, modify, 2020/07/02, modify codes for factory mode of photo black screen
    case SENSOR_FEATURE_GET_BINNING_TYPE:
        switch (*(feature_data + 1)) {
        case MSDK_SCENARIO_ID_CUSTOM3:
            *feature_return_para_32 = 1; /*BINNING_NONE*/
            break;
        case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
            //+bug604664 yaoguizhen, add, 2021/12/10,fix c8496 p1 iso is double for algo
            *feature_return_para_32 = 1; /*BINNING_NONE*/
            break;
            //-bug604664 yaoguizhen, add, 2021/12/10,fix c8496 p1 iso is double for algo
        case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
        case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
        case MSDK_SCENARIO_ID_SLIM_VIDEO:
        case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
        case MSDK_SCENARIO_ID_CUSTOM4:
        default:
            //+bug 558061, shaozhuchao.wt, modify, 2020/07/23, modify codes for main camera hw remosaic
            *feature_return_para_32 = 2; /*BINNING_AVERAGED*/
            //-bug 558061, shaozhuchao.wt, modify, 2020/07/23, modify codes for main camera hw remosaic
            break;
        }
        LOG_DBG("SENSOR_FEATURE_GET_BINNING_TYPE AE_binning_type:%d,\n",
            *feature_return_para_32);
            *feature_para_len = 4;
        break;
    //-bug 558061, zhanglinfeng.wt, modify, 2020/07/02, modify codes for factory mode of photo black screen
    default:
    break;
    }

    return ERROR_NONE;
}    /*    feature_control()  */

static struct SENSOR_FUNCTION_STRUCT sensor_func = {
    open,
    get_info,
    get_resolution,
    feature_control,
    control,
    close
};

UINT32 N28C8496FRONTDC_MIPI_RAW_SensorInit(struct SENSOR_FUNCTION_STRUCT **pfFunc)
{
    /* To Do : Check Sensor status here */
    if (pfFunc != NULL)
        *pfFunc = &sensor_func;
    return ERROR_NONE;
}

