/*****************************************************************************
 *
 * Filename:
 * ---------
 *     w1c8490fronttxd_mipiraw_sensor.c
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

#include "w1c8490fronttxd_mipiraw_sensor.h"

/****************************Modify Following Strings for Debug****************************/
#define PFX "C8490_SensroDriver"

#define LOG_INF(format, args...)    pr_info(PFX "[%s] " format, __func__, ##args)
#define LOG_ERR(format, args...)    pr_err(PFX "[%s] " format, __func__, ##args)
#define LOG_DBG(format, args...)    pr_debug(PFX "[%s] " format, __func__, ##args)

//+bug604664 qinduilin, add, 2021/11/12,c8490 eeprom bring up
#define EEPROM_GT24P64M_ID 0xA0

static kal_uint16 read_w1_c8490_eeprom(kal_uint32 addr)
{
    kal_uint16 get_byte=0;

    char pu_send_cmd[2] = { (char)(addr >> 8), (char)(addr & 0xFF) };
    iReadRegI2C(pu_send_cmd, 2, (u8*)&get_byte, 1, EEPROM_GT24P64M_ID);

    return get_byte;
}
//-bug604664 qinduilin, add, 2021/11/12,c8490 eeprom bring up

static DEFINE_SPINLOCK(imgsensor_drv_lock);
static float ratio = 1.0;

static struct imgsensor_info_struct imgsensor_info = {
    .sensor_id = W1C8490FRONTTXD_SENSOR_ID,
    .checksum_value = 0xf7375923,        //checksum value for Camera Auto Test
    .pre = {
        .pclk = 280000000,            //record different mode's pclk//84
        .linelength = 4000,            //record different mode's linelength
        .framelength = 2334,            //record different mode's framelength
        .startx = 0,                    //record different mode's startx of grabwindow
        .starty = 0,                    //record different mode's starty of grabwindow
        .grabwindow_width = 1632,        //record different mode's width of grabwindow
        .grabwindow_height = 1224,        //record different mode's height of grabwindow
        /*     following for MIPIDataLowPwr2HighSpeedSettleDelayCount by different scenario    */
        .mipi_data_lp2hs_settle_dc = 85,//unit , ns //85
        /*     following for GetDefaultFramerateByScenario()    */
        .max_framerate = 300,
        .mipi_pixel_rate = 348000000,
    },
    .cap = {
        .pclk = 280000000,            //record different mode's pclk//84
        .linelength = 3732,            //record different mode's linelength
        .framelength = 2500,            //record different mode's framelength
        .startx = 0,                    //record different mode's startx of grabwindow
        .starty = 0,                    //record different mode's starty of grabwindow
        .grabwindow_width = 3264,        //record different mode's width of grabwindow
        .grabwindow_height = 2448,        //record different mode's height of grabwindow
        /*     following for MIPIDataLowPwr2HighSpeedSettleDelayCount by different scenario    */
        .mipi_data_lp2hs_settle_dc = 85,//unit , ns //85
        /*     following for GetDefaultFramerateByScenario()    */
        .max_framerate = 300,
        .mipi_pixel_rate = 664000000,
    },
    .normal_video = {
        .pclk = 280000000,            //record different mode's pclk//84
        .linelength = 3732,            //record different mode's linelength
        .framelength = 2500,            //record different mode's framelength
        .startx = 0,                    //record different mode's startx of grabwindow
        .starty = 0,                    //record different mode's starty of grabwindow
        .grabwindow_width = 3264,        //record different mode's width of grabwindow
        .grabwindow_height = 2448,        //record different mode's height of grabwindow
        /*     following for MIPIDataLowPwr2HighSpeedSettleDelayCount by different scenario    */
        .mipi_data_lp2hs_settle_dc = 85,//unit , ns //85
        /*     following for GetDefaultFramerateByScenario()    */
        .max_framerate = 300,
        .mipi_pixel_rate = 696000000,
    },
    .hs_video = {
        .pclk = 280000000,            //record different mode's pclk//84
        .linelength = 4000,            //record different mode's linelength
        .framelength = 582,            //record different mode's framelength
        .startx = 0,                    //record different mode's startx of grabwindow
        .starty = 0,                    //record different mode's starty of grabwindow
        .grabwindow_width = 640,        //record different mode's width of grabwindow
        .grabwindow_height = 480,        //record different mode's height of grabwindow
        /*     following for MIPIDataLowPwr2HighSpeedSettleDelayCount by different scenario    */
        .mipi_data_lp2hs_settle_dc = 85,//unit , ns //85
        /*     following for GetDefaultFramerateByScenario()    */
        .max_framerate = 1200,
        .mipi_pixel_rate = 174000000,
    },
    .slim_video = {
        .pclk = 280000000,            //record different mode's pclk//84
        .linelength = 4000,            //record different mode's linelength
        .framelength = 776,            //record different mode's framelength
        .startx = 0,                    //record different mode's startx of grabwindow
        .starty = 0,                    //record different mode's starty of grabwindow
        .grabwindow_width = 1280,        //record different mode's width of grabwindow
        .grabwindow_height = 720,        //record different mode's height of grabwindow
        /*     following for MIPIDataLowPwr2HighSpeedSettleDelayCount by different scenario    */
        .mipi_data_lp2hs_settle_dc = 85,//unit , ns //85
        /*     following for GetDefaultFramerateByScenario()    */
        .max_framerate = 900,
        .mipi_pixel_rate = 348000000,
    },
    .margin = 4,            //sensor framelength & shutter margin
    .min_shutter = 8,        //min shutter
    .max_frame_length = 0xffff,//max framelength by sensor register's limitation
    .ae_shut_delay_frame = 0,    //shutter delay frame for AE cycle, 2 frame with ispGain_delay-shut_delay=2-0=2
    .ae_sensor_gain_delay_frame = 0,//sensor gain delay frame for AE cycle,2 frame with ispGain_delay-sensor_gain_delay=2-0=2
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
    .max_gain = 992, /*15.5x gain*/
    .min_gain_iso = 100,
    .gain_step = 1,
    .gain_type = 0,
    .i2c_speed = 400,

    .isp_driving_current = ISP_DRIVING_4MA, //mclk driving current
    .sensor_interface_type = SENSOR_INTERFACE_TYPE_MIPI,//sensor_interface_type
    .mipi_sensor_type = MIPI_OPHY_NCSI2, //0,MIPI_OPHY_NCSI2;  1,MIPI_OPHY_CSI2
    .mipi_settle_delay_mode = 0,//0,MIPI_SETTLEDELAY_AUTO; 1,MIPI_SETTLEDELAY_MANNUAL
    .sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_B,//sensor output first pixel color
    .mclk = 24,//mclk value, suggest 24 or 26 for 24Mhz or 26Mhz
    .mipi_lane_num = SENSOR_MIPI_4_LANE,//mipi lane num
    .i2c_addr_table = {0x6c, 0xff},//record sensor support all write id addr, only supprt 4must end with 0xff
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
    .i2c_write_id = 0x6c,//record current sensor's i2c write id
};

/* Sensor output window information */
static  struct SENSOR_WINSIZE_INFO_STRUCT imgsensor_winsize_info[5]=

{{ 3264, 2448,      0,    0,      3264, 2448,      1632, 1224,      0,   0,      1632, 1224,      0,    0, 1632,  1224},  // Preview
 { 3264, 2448,      0,    0,      3264, 2448,      3264, 2448,      0,   0,      3264, 2448,      0,    0, 3264,  2448},  // capture
 { 3264, 2448,      0,    0,      3264, 2448,      3264, 2448,      0,   0,      3264, 2448,      0,    0, 3264,  2448},  //video
 { 3264, 2448,      0,    0,      1280, 960,       1280, 960,       0,   0,      640,  480,      0,    0, 640,   480},   //high speed video
 { 3264, 2448,      252,  504,    2560, 1440,      1280, 720,       0,   0,      1280, 720,      0,    0, 1280,  720}};  //slim video 

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

    LOG_DBG("C8490 set_max_framerate():framerate = %d, min framelength should enable = %d\n", framerate,min_framelength_en);
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
    kal_uint16 reg_3b1b= 0xff;
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

    LOG_DBG("C8490 set_shutter(): shutter =%d\n", shutter);

    shutter = (shutter < imgsensor_info.min_shutter) ? imgsensor_info.min_shutter : shutter;
    shutter = (shutter > (imgsensor_info.max_frame_length - imgsensor_info.margin)) ? (imgsensor_info.max_frame_length - imgsensor_info.margin) : shutter;

    if(shutter% 2 == 0)
    {
        ratio = 1.0;
    }else{
        ratio = (float)shutter/(shutter-1);
    }

    if(shutter < 0x2e0){
        reg_3b1b = 0x5f;
    }else{
        reg_3b1b = 0xff;
    }

    // Update Shutter
    write_cmos_sensor(0x0340, imgsensor.frame_length >> 8);
    write_cmos_sensor(0x0341, imgsensor.frame_length & 0xFF);
    //printk("@CST_101:set_shutter(),writer_shutter=0x%x\n",shutter);
    write_cmos_sensor(0x0202, (shutter >> 8) & 0xFF);
    write_cmos_sensor(0x0203, (shutter) & 0xFF);
    write_cmos_sensor(0x3b1b, reg_3b1b);

    LOG_DBG("C8490 set_shutter(): reg3b1b=0x%x, shutter =%d, framelength =%d,mini_framelegth=%d\n", reg_3b1b,shutter,imgsensor.frame_length,imgsensor.min_frame_length);
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
#define AGAIN_NUM    64
    static    kal_uint16    again_table[AGAIN_NUM] =
    {
      64, 68, 72, 76, 80, 84, 88, 92, 96, 100, 104, 108, 112, 116, 120, 124, 128, 136, 144, 152,
      160, 168, 176, 184, 192, 200, 208, 216, 224, 232, 240, 248, 256, 272, 288, 304, 320, 336, 352, 368,
      384, 400, 416, 432, 448, 464, 480, 496, 512, 544, 576, 608, 640, 672, 704, 736, 768, 800, 832, 864,
      896, 928, 960, 992,
    };
    static    kal_uint16    again_register_table_0[AGAIN_NUM] =
    {
      0x00,    0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10, 0x12, 0x14,
        0x16, 0x18, 0x1a, 0x1c, 0x1e, 0x20, 0x22, 0x24, 0x26, 0x28, 0x2a, 0x2c, 0x2e, 0x30, 0x34, 0x38, 0x3c, 0x40, 0x44,
      0x48, 0x4c, 0x50, 0x54, 0x58, 0x5c, 0x60, 0x64, 0x68, 0x6c, 0x70, 0x78, 0x80, 0x88, 0x90, 0x98, 0xa0, 0xa8, 0xb0,
      0xb8, 0xc0, 0xc8, 0xd0, 0xd8, 0xe0, 0xe8,
    };
    static    kal_uint16  again_register_table_1[AGAIN_NUM] =
    {
      0xff,     0xff,    0xff,    0xff,    0xff,    0xff,    0xff,    0xff,    0xff,    0xff,    0xff,    0xff,    0xff,    0xff,    0xff,    0xff,    0xff,    0xff,    0xff,
      0xff,     0xff,    0xff,    0xff,    0xff,    0xff,    0xff,    0xff,    0xff,    0xff,    0xff,    0xff,    0xff,    0xff,    0xff,    0xff,    0xff,    0xff,    0xff,
      0xff,     0xff,    0xff,    0xff,    0xff,    0xff,    0xff,    0xff,    0xff,    0xff,    0xfb,    0xfc,    0xfd,    0xfe,    0xff,    0xff,    0xff,    0xff,    0xff,
      0xff,     0xff,    0xff,    0xff,    0xff,    0xff,    0xff,
    };//0x32ac

static kal_uint16 set_gain(kal_uint16 gain)
{
    kal_uint16 tmp0 = 0, tmp1 = 0,  i = 0, Again_base = 0;
    kal_uint16 iGain=gain*ratio;
    LOG_DBG("C8490 isp again = 0x%x, iGain=0x%x\n",gain,iGain);

    #if 0
    //for debug
        {
            kal_uint16 reg_32ac = 0;
            kal_uint16 reg_0205 = 0;

            reg_32ac = read_cmos_sensor(0x32ac);
            reg_3293 = read_cmos_sensor(0x0205);
        }
    #endif
    if(iGain >= again_table[AGAIN_NUM-1]){
        iGain = again_table[AGAIN_NUM-1]; //max gain: 8*64
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

    tmp0 = again_register_table_0[Again_base];
    tmp1 = again_register_table_1[Again_base];                 //0x32ac

    write_cmos_sensor(0x32ac,tmp1);
    write_cmos_sensor(0x0205,tmp0);
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
    //modify from spreadtrum initial param by cista yjp,20211110
    write_cmos_sensor(0x0103,0x01);
    write_cmos_sensor(0x32aa,0x19); //changed from 0a to 19, adc range 0.75V
    write_cmos_sensor(0x32ab,0x08);
    write_cmos_sensor(0x3280,0x86);
    write_cmos_sensor(0x3284,0x04); //hvdd=VRFX
    write_cmos_sensor(0x3286,0x44);    // 54 to 44 reduce the VFPN
    write_cmos_sensor(0x3287,0x51); //  for better snr
    write_cmos_sensor(0x328f,0x32);
    write_cmos_sensor(0x3290,0x44); //nv1=nv2=-1.36
    write_cmos_sensor(0x3297,0x03);
    write_cmos_sensor(0x3298,0x4b);
    write_cmos_sensor(0x3216,0x36);
    write_cmos_sensor(0x0217,0x00); //
    write_cmos_sensor(0x328d,0x06); //
    write_cmos_sensor(0x3217,0x10); // remove VFPN
    write_cmos_sensor(0x3296,0x82);
    write_cmos_sensor(0x0403,0x50);
    write_cmos_sensor(0x3182,0x30);
    write_cmos_sensor(0x3183,0x00); //orc_en off
    write_cmos_sensor(0x0202,0x09);
    write_cmos_sensor(0x0203,0xc4);
    write_cmos_sensor(0x0101,0x00);
    write_cmos_sensor(0x300b,0x00);
    write_cmos_sensor(0x3281,0x81);
    write_cmos_sensor(0x3293,0x03);
    write_cmos_sensor(0x3211,0x54);
    write_cmos_sensor(0x3215,0x01);
    write_cmos_sensor(0x3087,0xf0);
    write_cmos_sensor(0x3022,0x01);    //0x02
    write_cmos_sensor(0x3180,0x09);
    write_cmos_sensor(0x022d,0x4c);
    write_cmos_sensor(0x3600,0xc8);
    write_cmos_sensor(0x3602,0x02);    //01 org by lujie 2020 12 09
    write_cmos_sensor(0x3583,0x10);
    write_cmos_sensor(0x3584,0x02);
    write_cmos_sensor(0xa000,0x8b);
    write_cmos_sensor(0xa001,0xd0);
    write_cmos_sensor(0xa002,0x00);
    write_cmos_sensor(0xa003,0x00);
    write_cmos_sensor(0xa004,0x00);
    write_cmos_sensor(0xa005,0x00);
    write_cmos_sensor(0xa006,0x80);
    write_cmos_sensor(0xa007,0x80);
    write_cmos_sensor(0xa008,0x4e);
    write_cmos_sensor(0xa009,0x01);
    write_cmos_sensor(0xa00a,0x83);
    write_cmos_sensor(0xa00b,0x1c);
    write_cmos_sensor(0xa00c,0x03);
    write_cmos_sensor(0xa00d,0xa5);
    write_cmos_sensor(0xa00e,0x8d);
    write_cmos_sensor(0xa00f,0x01);
    write_cmos_sensor(0xa010,0x8a);
    write_cmos_sensor(0xa011,0x90);
    write_cmos_sensor(0xa012,0x8e);
    write_cmos_sensor(0xa013,0x81);
    write_cmos_sensor(0xa014,0x8c);
    write_cmos_sensor(0xa015,0x50);
    write_cmos_sensor(0xa016,0x05);
    write_cmos_sensor(0xa017,0x51);
    write_cmos_sensor(0xa018,0x29);
    write_cmos_sensor(0xa019,0x61);
    write_cmos_sensor(0xa01a,0x85);
    write_cmos_sensor(0xa01b,0x81);
    write_cmos_sensor(0xa01c,0x30);
    write_cmos_sensor(0xa01d,0xe0);
    write_cmos_sensor(0xa01e,0x86);
    write_cmos_sensor(0xa01f,0x81);
    write_cmos_sensor(0xa020,0x08);
    write_cmos_sensor(0xa021,0x10);
    write_cmos_sensor(0xa022,0x9d);
    write_cmos_sensor(0xa023,0x01);
    write_cmos_sensor(0xa024,0x88);
    write_cmos_sensor(0xa025,0x10);
    write_cmos_sensor(0xa026,0x1d);
    write_cmos_sensor(0xa027,0x01);
    write_cmos_sensor(0xa028,0x9d);
    write_cmos_sensor(0xa029,0x10);
    write_cmos_sensor(0xa02a,0x90);
    write_cmos_sensor(0xa02b,0x7c);
    write_cmos_sensor(0xa02c,0x9e);
    write_cmos_sensor(0xa02d,0x81);
    write_cmos_sensor(0xa02e,0x00);
    write_cmos_sensor(0xa02f,0xe1);
    write_cmos_sensor(0xa030,0x1e);
    write_cmos_sensor(0xa031,0x84);
    write_cmos_sensor(0xa032,0x03);
    write_cmos_sensor(0xa033,0x30);
    write_cmos_sensor(0xa034,0xb5);
    write_cmos_sensor(0xa035,0xd0);
    write_cmos_sensor(0xa036,0x40);
    write_cmos_sensor(0xa037,0xe0);
    write_cmos_sensor(0xa038,0x1e);
    write_cmos_sensor(0xa039,0x84);
    write_cmos_sensor(0xa03a,0x03);
    write_cmos_sensor(0xa03b,0x30);
    write_cmos_sensor(0xa03c,0xad);
    write_cmos_sensor(0xa03d,0x50);
    write_cmos_sensor(0xa03e,0x20);
    write_cmos_sensor(0xa03f,0x60);
    write_cmos_sensor(0xa040,0x1e);
    write_cmos_sensor(0xa041,0x84);
    write_cmos_sensor(0xa042,0x03);
    write_cmos_sensor(0xa043,0x30);
    write_cmos_sensor(0xa044,0x25);
    write_cmos_sensor(0xa045,0x50);
    write_cmos_sensor(0xa046,0x87);
    write_cmos_sensor(0xa047,0xe0);
    write_cmos_sensor(0xa048,0x26);
    write_cmos_sensor(0xa049,0x50);
    write_cmos_sensor(0xa04a,0x03);
    write_cmos_sensor(0xa04b,0x60);
    write_cmos_sensor(0xa04c,0x93);
    write_cmos_sensor(0xa04d,0x01);
    write_cmos_sensor(0xa04e,0x14);
    write_cmos_sensor(0xa04f,0x83);
    write_cmos_sensor(0xa050,0x94);
    write_cmos_sensor(0xa051,0x10);
    write_cmos_sensor(0xa052,0x92);
    write_cmos_sensor(0xa053,0x01);
    write_cmos_sensor(0xa054,0x93);
    write_cmos_sensor(0xa055,0x90);
    write_cmos_sensor(0xa056,0x91);
    write_cmos_sensor(0xa057,0x81);
    write_cmos_sensor(0xa058,0x30);
    write_cmos_sensor(0xa059,0x50);
    write_cmos_sensor(0xa05a,0x11);
    write_cmos_sensor(0xa05b,0x83);
    write_cmos_sensor(0xa05c,0x11);
    write_cmos_sensor(0xa05d,0x95);
    write_cmos_sensor(0xa05e,0x92);
    write_cmos_sensor(0xa05f,0x03);
    write_cmos_sensor(0xa060,0x92);
    write_cmos_sensor(0xa061,0x90);
    write_cmos_sensor(0xa062,0x90);
    write_cmos_sensor(0xa063,0x81);
    write_cmos_sensor(0xa064,0x91);
    write_cmos_sensor(0xa065,0x10);
    write_cmos_sensor(0xa066,0x8f);
    write_cmos_sensor(0xa067,0x01);
    write_cmos_sensor(0xa068,0x37);
    write_cmos_sensor(0xa069,0x50);
    write_cmos_sensor(0xa06a,0x0f);
    write_cmos_sensor(0xa06b,0x83);
    write_cmos_sensor(0xa06c,0x90);
    write_cmos_sensor(0xa06d,0x03);
    write_cmos_sensor(0xa06e,0x8f);
    write_cmos_sensor(0xa06f,0x90);
    write_cmos_sensor(0xa070,0x1b);
    write_cmos_sensor(0xa071,0x81);
    write_cmos_sensor(0xa072,0x00);
    write_cmos_sensor(0xa073,0x61);
    write_cmos_sensor(0xa074,0x1e);
    write_cmos_sensor(0xa075,0x04);
    write_cmos_sensor(0xa076,0x83);
    write_cmos_sensor(0xa077,0xb0);
    write_cmos_sensor(0xa078,0xd9);
    write_cmos_sensor(0xa079,0xd0);
    write_cmos_sensor(0xa07a,0xc0);
    write_cmos_sensor(0xa07b,0x60);
    write_cmos_sensor(0xa07c,0x1e);
    write_cmos_sensor(0xa07d,0x04);
    write_cmos_sensor(0xa07e,0x83);
    write_cmos_sensor(0xa07f,0x30);
    write_cmos_sensor(0xa080,0xd0);
    write_cmos_sensor(0xa081,0x50);
    write_cmos_sensor(0xa082,0x20);
    write_cmos_sensor(0xa083,0xe0);
    write_cmos_sensor(0xa084,0x9e);
    write_cmos_sensor(0xa085,0x04);
    write_cmos_sensor(0xa086,0x83);
    write_cmos_sensor(0xa087,0x30);
    write_cmos_sensor(0xa088,0x47);
    write_cmos_sensor(0xa089,0x50);
    write_cmos_sensor(0xa08a,0x1e);
    write_cmos_sensor(0xa08b,0x90);
    write_cmos_sensor(0xa08c,0x49);
    write_cmos_sensor(0xa08d,0x50);
    write_cmos_sensor(0xa08e,0x03);
    write_cmos_sensor(0xa08f,0x20);
    write_cmos_sensor(0xa090,0x1e);
    write_cmos_sensor(0xa091,0x98);
    write_cmos_sensor(0xa092,0x99);
    write_cmos_sensor(0xa093,0x81);
    write_cmos_sensor(0xa094,0x1a);
    write_cmos_sensor(0xa095,0x83);
    write_cmos_sensor(0xa096,0x9a);
    write_cmos_sensor(0xa097,0x10);
    write_cmos_sensor(0xa098,0x18);
    write_cmos_sensor(0xa099,0x01);
    write_cmos_sensor(0xa09a,0x19);
    write_cmos_sensor(0xa09b,0x10);
    write_cmos_sensor(0xa09c,0x97);
    write_cmos_sensor(0xa09d,0x81);
    write_cmos_sensor(0xa09e,0x54);
    write_cmos_sensor(0xa09f,0xd0);
    write_cmos_sensor(0xa0a0,0xf9);
    write_cmos_sensor(0xa0a1,0x40);
    write_cmos_sensor(0xa0a2,0x0c);
    write_cmos_sensor(0xa0a3,0x90);
    write_cmos_sensor(0xa0a4,0x97);
    write_cmos_sensor(0xa0a5,0x81);
    write_cmos_sensor(0xa0a6,0x18);
    write_cmos_sensor(0xa0a7,0x03);
    write_cmos_sensor(0xa0a8,0x18);
    write_cmos_sensor(0xa0a9,0x90);
    write_cmos_sensor(0xa0aa,0x96);
    write_cmos_sensor(0xa0ab,0x01);
    write_cmos_sensor(0xa0ac,0x97);
    write_cmos_sensor(0xa0ad,0x10);
    write_cmos_sensor(0xa0ae,0x95);
    write_cmos_sensor(0xa0af,0x01);
    write_cmos_sensor(0xa0b0,0xdf);
    write_cmos_sensor(0xa0b1,0xd0);
    write_cmos_sensor(0xa0b2,0x79);
    write_cmos_sensor(0xa0b3,0xc0);
    write_cmos_sensor(0xa0b4,0x03);
    write_cmos_sensor(0xa0b5,0x20);
    write_cmos_sensor(0xa0b6,0x8c);
    write_cmos_sensor(0xa0b7,0x99);
    write_cmos_sensor(0xa0b8,0x0c);
    write_cmos_sensor(0xa0b9,0x90);
    write_cmos_sensor(0xa0ba,0x95);
    write_cmos_sensor(0xa0bb,0x81);
    write_cmos_sensor(0xa0bc,0x16);
    write_cmos_sensor(0xa0bd,0x03);
    write_cmos_sensor(0xa0be,0x95);
    write_cmos_sensor(0xa0bf,0x10);
    write_cmos_sensor(0xa0c0,0x9c);
    write_cmos_sensor(0xa0c1,0x01);
    write_cmos_sensor(0xa0c2,0x13);
    write_cmos_sensor(0xa0c3,0x61);
    write_cmos_sensor(0xa0c4,0x85);
    write_cmos_sensor(0xa0c5,0x81);
    write_cmos_sensor(0xa0c6,0x32);
    write_cmos_sensor(0xa0c7,0x60);
    write_cmos_sensor(0xa0c8,0x86);
    write_cmos_sensor(0xa0c9,0x01);
    write_cmos_sensor(0xa0ca,0x1b);
    write_cmos_sensor(0xa0cb,0x10);
    write_cmos_sensor(0xa0cc,0x08);
    write_cmos_sensor(0xa0cd,0x01);
    write_cmos_sensor(0xa0ce,0x29);
    write_cmos_sensor(0xa0cf,0x61);
    write_cmos_sensor(0xa0d0,0x85);
    write_cmos_sensor(0xa0d1,0x81);
    write_cmos_sensor(0xa0d2,0x32);
    write_cmos_sensor(0xa0d3,0x60);
    write_cmos_sensor(0xa0d4,0x86);
    write_cmos_sensor(0xa0d5,0x81);
    write_cmos_sensor(0xa0d6,0x9c);
    write_cmos_sensor(0xa0d7,0x90);
    write_cmos_sensor(0xa0d8,0x88);
    write_cmos_sensor(0xa0d9,0x81);
    write_cmos_sensor(0xa0da,0x90);
    write_cmos_sensor(0xa0db,0x60);
    write_cmos_sensor(0xa0dc,0x85);
    write_cmos_sensor(0xa0dd,0x01);
    write_cmos_sensor(0xa0de,0xb6);
    write_cmos_sensor(0xa0df,0xe0);
    write_cmos_sensor(0xa0e0,0x86);
    write_cmos_sensor(0xa0e1,0x81);
    write_cmos_sensor(0xa0e2,0x08);
    write_cmos_sensor(0xa0e3,0x83);
    write_cmos_sensor(0xa0e4,0x8e);
    write_cmos_sensor(0xa0e5,0x10);
    write_cmos_sensor(0xa0e6,0x8a);
    write_cmos_sensor(0xa0e7,0x01);
    write_cmos_sensor(0xa0e8,0x8d);
    write_cmos_sensor(0xa0e9,0x9c);
    write_cmos_sensor(0xa0ea,0x03);
    write_cmos_sensor(0xa0eb,0x81);
    write_cmos_sensor(0xa0ec,0x4e);
    write_cmos_sensor(0xa0ed,0x9d);
    write_cmos_sensor(0xa0ee,0xce);
    write_cmos_sensor(0xa0ef,0x9c);
    write_cmos_sensor(0xa0f0,0x89);
    write_cmos_sensor(0xa0f1,0x00);
    write_cmos_sensor(0xa0f2,0x1e);
    write_cmos_sensor(0xa0f3,0x90);
    write_cmos_sensor(0xa0f4,0x0c);
    write_cmos_sensor(0xa0f5,0x81);
    write_cmos_sensor(0xa0f6,0x03);
    write_cmos_sensor(0xa0f7,0xa0);
    write_cmos_sensor(0xa0f8,0x0c);
    write_cmos_sensor(0xa0f9,0x19);
    write_cmos_sensor(0xa0fa,0x83);
    write_cmos_sensor(0xa0fb,0xa0);
    write_cmos_sensor(0xa0fc,0x0c);
    write_cmos_sensor(0xa0fd,0x19);
    write_cmos_sensor(0xa0fe,0x88);
    write_cmos_sensor(0xa0ff,0x00);
    write_cmos_sensor(0xa100,0x03);
    write_cmos_sensor(0xa101,0x2d);
    write_cmos_sensor(0xa102,0x01);
    write_cmos_sensor(0xa103,0xae);
    write_cmos_sensor(0xa104,0x0b);
    write_cmos_sensor(0xa105,0xac);
    write_cmos_sensor(0xa106,0x8b);
    write_cmos_sensor(0xa107,0x2f);
    write_cmos_sensor(0xa108,0x84);
    write_cmos_sensor(0xa109,0x51);
    write_cmos_sensor(0xa10a,0x03);
    write_cmos_sensor(0xa10b,0x83);
    write_cmos_sensor(0xa10c,0x00);
    write_cmos_sensor(0xa10d,0xd1);
    write_cmos_sensor(0xa10e,0x80);
    write_cmos_sensor(0xa10f,0x80);
    write_cmos_sensor(0x3583,0x00);
    write_cmos_sensor(0x3584,0x22);
    write_cmos_sensor(0x3b00,0x61); //manel blc dc on
    write_cmos_sensor(0x3b01,0x40);
    write_cmos_sensor(0x3b02,0x00);
    write_cmos_sensor(0x3b03,0xf4);
    write_cmos_sensor(0x3b04,0x03);
    write_cmos_sensor(0x3b05,0x40);
    write_cmos_sensor(0x3b06,0x00);
    write_cmos_sensor(0x3b07,0xf4);
    write_cmos_sensor(0x3b08,0x01);
    write_cmos_sensor(0x3b09,0xe8);
    write_cmos_sensor(0x3b0a,0x03);
    write_cmos_sensor(0x3b0b,0xdc);
    write_cmos_sensor(0x3b0c,0x05);
    write_cmos_sensor(0x3b0d,0xcf);
    write_cmos_sensor(0x3b0e,0xf7);
    write_cmos_sensor(0x3b0f,0x91);
    write_cmos_sensor(0x3b10,0x0f);
    write_cmos_sensor(0x3b11,0x82);
    write_cmos_sensor(0x3b12,0x0b);
    write_cmos_sensor(0x3b13,0x81);
    write_cmos_sensor(0x3b14,0x03);
    write_cmos_sensor(0x3b15,0x10);
    write_cmos_sensor(0x3b16,0x30);
    write_cmos_sensor(0x3b17,0x10);
    write_cmos_sensor(0x3b18,0x00);
    write_cmos_sensor(0x3b19,0x30);
    write_cmos_sensor(0x3b1a,0x00);
    write_cmos_sensor(0x3b1b,0x5f);
    write_cmos_sensor(0x3b1c,0x00);
    write_cmos_sensor(0x3b1d,0x10);
    write_cmos_sensor(0x3b1e,0x10);
    write_cmos_sensor(0x3b1f,0x10);
    write_cmos_sensor(0x3b20,0x08);
    write_cmos_sensor(0x3b21,0x00);
    write_cmos_sensor(0x3b22,0x07); //manual dc
    write_cmos_sensor(0x3b23,0x00);
    write_cmos_sensor(0x3b24,0x14);
    write_cmos_sensor(0x3122,0x10);//Bug 794716 liuxaingyin, add, 2022/10/17,fix for c8490 preview screen flashes red when switch between capture and video mode
}

static void preview_setting(kal_uint16 currefps)
{
    //binning
    write_cmos_sensor(0x3216,0x20);
    write_cmos_sensor(0x3c01,0x17); //BIN SIZE
    write_cmos_sensor(0x3c04,0xe0); //demosic on
    write_cmos_sensor(0x034c,0x06);
    write_cmos_sensor(0x034d,0x60);
    write_cmos_sensor(0x034e,0x04);
    write_cmos_sensor(0x034f,0xc8);
    write_cmos_sensor(0x3009,0x00);
    write_cmos_sensor(0x300b,0x00);
    write_cmos_sensor(0x0305,0x02);
    write_cmos_sensor(0x3515,0x02);
    write_cmos_sensor(0x0307,0x8c);     //sys 150.4MHz
    write_cmos_sensor(0x3517,0x57);     //mp 188MHz
    write_cmos_sensor(0x3509,0x3c);
    write_cmos_sensor(0x3519,0x3c);     // mp pll_vco hi=low
    write_cmos_sensor(0x3514,0x04);     //mp postdiv =1
    write_cmos_sensor(0x0303,0x00);     //sys_post div=8
    write_cmos_sensor(0x0304,0x00);
    write_cmos_sensor(0x3021,0x22);
    write_cmos_sensor(0x0385,0x01);
    write_cmos_sensor(0x0387,0x03);
    write_cmos_sensor(0x3210,0x1a);
    write_cmos_sensor(0x3805,0x07);
    write_cmos_sensor(0x3806,0x03);
    write_cmos_sensor(0x3807,0x03);
    write_cmos_sensor(0x3808,0x0b);
    write_cmos_sensor(0x3809,0x54);
    write_cmos_sensor(0x380a,0x4b);
    write_cmos_sensor(0x380b,0x26);
    write_cmos_sensor(0x380c,0x28);
    write_cmos_sensor(0x380d,0x00);
    write_cmos_sensor(0x380e,0x31);
    write_cmos_sensor(0x0340,0x09);
    write_cmos_sensor(0x0341,0x1e);
    write_cmos_sensor(0x0342,0x0f);
    write_cmos_sensor(0x0343,0xa0);
    write_cmos_sensor(0x3882,0x31);
    write_cmos_sensor(0x3881,0x18);
}

static void capture_setting(kal_uint16 currefps)
{
    write_cmos_sensor(0x3216,0x36);
    write_cmos_sensor(0x3c01,0x17); //FULL SIZE
    write_cmos_sensor(0x3c04,0xe0); //demosic on
    write_cmos_sensor(0x034c,0x0c);
    write_cmos_sensor(0x034d,0xc0);
    write_cmos_sensor(0x034e,0x09);
    write_cmos_sensor(0x034f,0x90);
    write_cmos_sensor(0x3009,0x00);
    write_cmos_sensor(0x300b,0x00);
    write_cmos_sensor(0x0305,0x02);
    write_cmos_sensor(0x3515,0x02);
    write_cmos_sensor(0x0307,0x8C);     //sys 280MHz
    write_cmos_sensor(0x3517,0x53);     //mp 664MHz
    write_cmos_sensor(0x3509,0x3c);
    write_cmos_sensor(0x3519,0x3c);     // mp pll_vco hi=low
    write_cmos_sensor(0x3514,0x00);     //mp postdiv =1
    write_cmos_sensor(0x0303,0x00);     //sys_post div=8
    write_cmos_sensor(0x0304,0x00);
    write_cmos_sensor(0x3805,0x0a);
    write_cmos_sensor(0x3806,0x07);
    write_cmos_sensor(0x3807,0x07);
    write_cmos_sensor(0x3808,0x16);
    write_cmos_sensor(0x3809,0xa8);
    write_cmos_sensor(0x380a,0xcd);
    write_cmos_sensor(0x380b,0xab);
    write_cmos_sensor(0x380c,0x28);
    write_cmos_sensor(0x380d,0x00);
    write_cmos_sensor(0x380e,0x31);
    write_cmos_sensor(0x0340,0x09);
    write_cmos_sensor(0x0341,0xc4);
    write_cmos_sensor(0x0342,0x0e);
    write_cmos_sensor(0x0343,0x94);
    write_cmos_sensor(0x3882,0x31);
    write_cmos_sensor(0x3881,0x04);
}

static void normal_video_setting(kal_uint16 currefps)
{
    capture_setting(currefps);
}

static void hs_video_setting(kal_uint16 currefps)
{
    //vga
    write_cmos_sensor(0x3216,0x36);
    write_cmos_sensor(0x3c01,0x17);
    write_cmos_sensor(0x3c04,0xe0);
    write_cmos_sensor(0x034c,0x02);
    write_cmos_sensor(0x034d,0x80);
    write_cmos_sensor(0x034e,0x01);
    write_cmos_sensor(0x034f,0xe0);
    write_cmos_sensor(0x3009,0x58);
    write_cmos_sensor(0x300b,0x00);
    //pll
    write_cmos_sensor(0x0305,0x02);
    write_cmos_sensor(0x3515,0x02);
    write_cmos_sensor(0x0307,0x8c);   //sys 280MHz
    write_cmos_sensor(0x3517,0x57);   //mp 174MHz
    write_cmos_sensor(0x3509,0x3c);
    write_cmos_sensor(0x3519,0x3c);   // mp pll_vco hi=low
    write_cmos_sensor(0x3514,0x08);   //mp postdiv =1
    write_cmos_sensor(0x0303,0x00);   //sys_post div=8
    write_cmos_sensor(0x0304,0x00);
    write_cmos_sensor(0x3021,0x44);
    write_cmos_sensor(0x0385,0x01);
    write_cmos_sensor(0x0387,0x07);
    write_cmos_sensor(0x3210,0x1a);
    //MIPI
    write_cmos_sensor(0x3805,0x05);
    write_cmos_sensor(0x3806,0x02);
    write_cmos_sensor(0x3807,0x02);
    write_cmos_sensor(0x3808,0x06);
    write_cmos_sensor(0x3809,0x32);
    write_cmos_sensor(0x380a,0x3a);
    write_cmos_sensor(0x380b,0x24);
    write_cmos_sensor(0x380c,0x28);
    write_cmos_sensor(0x380d,0x00);
    write_cmos_sensor(0x380e,0x31);
    write_cmos_sensor(0x0340,0x02);
    write_cmos_sensor(0x0341,0x44);
    write_cmos_sensor(0x0342,0x0f);
    write_cmos_sensor(0x0343,0xa0);
    write_cmos_sensor(0x0346,0x01);
    write_cmos_sensor(0x0347,0x00);
    write_cmos_sensor(0x034a,0x08);
    write_cmos_sensor(0x034b,0x7f);
    write_cmos_sensor(0x3882,0x31);
    write_cmos_sensor(0x3881,0x18);
}

static void slim_video_setting(kal_uint16 currefps)
{
    //720p
    write_cmos_sensor(0x3216,0x20);
    //output size
    write_cmos_sensor(0x3c01,0x17);
    write_cmos_sensor(0x3c04,0xe0); //demosic on
    write_cmos_sensor(0x034c,0x05);
    write_cmos_sensor(0x034d,0x00);
    write_cmos_sensor(0x034e,0x02);
    write_cmos_sensor(0x034f,0xd0);
    write_cmos_sensor(0x3009,0xaf);
    write_cmos_sensor(0x300b,0x09);
    //pll
    write_cmos_sensor(0x0305,0x02);
    write_cmos_sensor(0x3515,0x02);
    write_cmos_sensor(0x0307,0x8c);   //sys 280MHz
    write_cmos_sensor(0x3517,0x57);   //mp 348MHz
    write_cmos_sensor(0x3509,0x3c);
    write_cmos_sensor(0x3519,0x3c);   // mp pll_vco hi=low
    write_cmos_sensor(0x3514,0x04);   //mp postdiv =1
    write_cmos_sensor(0x0303,0x00);   //sys_post div=8
    write_cmos_sensor(0x0304,0x00);
    write_cmos_sensor(0x3021,0x22);
    ///write_cmos_sensor(0x3022,0x02);  //bin sum
    write_cmos_sensor(0x0385,0x01);
    write_cmos_sensor(0x0387,0x03);
    write_cmos_sensor(0x3210,0x1a);
    //MIPI
    write_cmos_sensor(0x3805,0x08);
    write_cmos_sensor(0x3806,0x03);
    write_cmos_sensor(0x3807,0x03);
    write_cmos_sensor(0x3808,0x0b);
    write_cmos_sensor(0x3809,0x54);
    write_cmos_sensor(0x380a,0x4b);
    write_cmos_sensor(0x380b,0x26);
    write_cmos_sensor(0x380c,0x21);
    write_cmos_sensor(0x380d,0x00);
    write_cmos_sensor(0x380e,0x31);
    write_cmos_sensor(0x0340,0x03);
    write_cmos_sensor(0x0341,0x08);
    write_cmos_sensor(0x0342,0x0f);
    write_cmos_sensor(0x0343,0xa0);
    write_cmos_sensor(0x0346,0x03);
    write_cmos_sensor(0x0347,0x83);
    write_cmos_sensor(0x034a,0x09);
    write_cmos_sensor(0x034b,0x4b);
    write_cmos_sensor(0x3882,0x31);
    write_cmos_sensor(0x3881,0x18);
}

static kal_uint32 set_test_pattern_mode(kal_bool enable)
{
    spin_lock(&imgsensor_drv_lock);
    spin_unlock(&imgsensor_drv_lock);
    return ERROR_NONE;
}

//+bug604664 qinduilin, add, 2021/11/12,c8490 eeprom bring up
#include "cam_cal_define.h"
#include <linux/slab.h>
static struct stCAM_CAL_DATAINFO_STRUCT w1c8490fronttxd_eeprom_data ={
    .sensorID= W1C8490FRONTTXD_SENSOR_ID,
    .deviceID = 0x02,
    .dataLength = 0x076A,
    .sensorVendorid = 0x19050100,
    .vendorByte = {1,2,3,4},
    .dataBuffer = NULL,
};
static struct stCAM_CAL_CHECKSUM_STRUCT w1c8490fronttxd_checksum[8] =
{
    {MODULE_ITEM,0x0000,0x0000,0x0007,0x0008,0x55},
    {AWB_ITEM,0x0009,0x0009,0x0019,0x001A,0x55},
    {LSC_ITEM,0x001B,0x001B,0x0767,0x0768,0x55},
    {TOTAL_ITEM,0x0000,0x0000,0x0768,0x0769,0x55},
    {MAX_ITEM,0xFFFF,0xFFFF,0xFFFF,0xFFFF,0x55},  // this line must haved
};
extern int imgSensorReadEepromData(struct stCAM_CAL_DATAINFO_STRUCT* pData,
    struct stCAM_CAL_CHECKSUM_STRUCT* checkData);
extern int imgSensorSetEepromData(struct stCAM_CAL_DATAINFO_STRUCT* pData);
//-bug604664 qinduilin, add, 2021/11/12,c8490 eeprom bring up
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
    kal_int32 size = 0;

    while (imgsensor_info.i2c_addr_table[i] != 0xff) {
        spin_lock(&imgsensor_drv_lock);
        imgsensor.i2c_write_id = imgsensor_info.i2c_addr_table[i];
        spin_unlock(&imgsensor_drv_lock);
        do {
            *sensor_id = ((read_cmos_sensor(0x0000) << 8) | read_cmos_sensor(0x0001));
            if (*sensor_id == imgsensor_info.sensor_id) {
                LOG_INF("C8490 get_imgsensor_id OK,i2c write id: 0x%X, sensor id: 0x%X\n", imgsensor.i2c_write_id,*sensor_id);
                //+bug604664 qinduilin, add, 2021/11/12,c8490 eeprom bring up
                size = imgSensorReadEepromData(&w1c8490fronttxd_eeprom_data,w1c8490fronttxd_checksum);
                if(size != w1c8490fronttxd_eeprom_data.dataLength || (w1c8490fronttxd_eeprom_data.sensorVendorid >> 24)!= read_w1_c8490_eeprom(0x0001)) {
                    LOG_ERR("get eeprom data failed size:0x%X,dataLength:0x%X,vendorid:0x%X\n",size,w1c8490fronttxd_eeprom_data.dataLength,read_w1_c8490_eeprom(0x0001));
                    if(w1c8490fronttxd_eeprom_data.dataBuffer != NULL) {
                        kfree(w1c8490fronttxd_eeprom_data.dataBuffer);
                        w1c8490fronttxd_eeprom_data.dataBuffer = NULL;
                    }
                    *sensor_id = 0xFFFFFFFF;
                    return ERROR_SENSOR_CONNECT_FAIL;
                } else {
                    LOG_INF("get eeprom data success\n");
                    imgSensorSetEepromData(&w1c8490fronttxd_eeprom_data);
                }
                //-bug604664 qinduilin, add, 2021/11/12,c8490 eeprom bring up

                return ERROR_NONE;
            }
            LOG_ERR("C8490 get_imgsensor_id fail, write id: 0x%X, id: 0x%X\n", imgsensor.i2c_write_id,*sensor_id);
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
                LOG_DBG("C8490 open:Read sensor id OK,i2c write id: 0x%X, sensor id: 0x%X\n", imgsensor.i2c_write_id,sensor_id);
                break;
            }
            LOG_DBG("C8490 Read sensor id fail, write id: 0x%X, id: 0x%X\n", imgsensor.i2c_write_id,sensor_id);
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
            //+bug604664 yaoguizhen, add, 2021/12/10,fix c8490 p1 iso is double for algo
            *feature_return_para_32 = 1; /*BINNING_NONE*/
            break;
            //-bug604664 yaoguizhen, add, 2021/12/10,fix c8490 p1 iso is double for algo
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

UINT32 W1C8490FRONTTXD_MIPI_RAW_SensorInit(struct SENSOR_FUNCTION_STRUCT **pfFunc)
{
    /* To Do : Check Sensor status here */
    if (pfFunc != NULL)
        *pfFunc = &sensor_func;
    return ERROR_NONE;
}

