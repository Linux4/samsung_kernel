/*****************************************************************************
 *
 * Filename:
 * ---------
 *       a0604xlfrontmt811_mipi_raw.c
 *
 * Project:
 * --------
 *       ALPS
 *
 * Description:
 * ------------
 *       Source code of Sensor driver
 *
 ****************************************************************************/

#include <linux/videodev2.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <asm/atomic.h>
#include <linux/types.h>
#include "kd_camera_typedef.h"
#include "kd_imgsensor.h"
#include "kd_imgsensor_define.h"
#include "kd_imgsensor_errcode.h"
#include "a0604xlfrontmt811_mipi_raw.h"

#define PFX "a0604xlfrontmt811_mipi_raw"
#define LOG_INF(format, args...)    pr_debug(PFX "[%s] " format, __FUNCTION__, ##args)
#define MAX_GAIN 240
#define MIN_GAIN 5

#ifdef MT811_LONG_EXP
#define MAX_CIT_LSHIFT 7
#endif
#define SBL_TH                    6.0
#define REG_SYS_ANA_WR02_LG_VAL   0x1f
#define REG_SYS_ANA_WR02_HG_VAL   0x1b

#define MT811_MIRROR_NORMAL  1
#define MT811_MIRROR_H       0
#define MT811_MIRROR_V       0
#define MT811_MIRROR_HV      0

static DEFINE_SPINLOCK(imgsensor_drv_lock);

static struct imgsensor_info_struct imgsensor_info = {
    .sensor_id = A0604XLFRONTMT811_SENSOR_ID,
    .checksum_value = 0xbf50a4b8,
    /* A06 code for SR-AL7160A-01-501 by zhongbin at 20240422 start */
    .pre = {
        .pclk = 336000000,
        .linelength = 4368,
        .framelength = 2564,
        .mipi_pixel_rate = 269200000,
        .startx = 0,
        .starty = 0,
        .grabwindow_width = 3264,
        .grabwindow_height = 2448,
        .mipi_data_lp2hs_settle_dc = 85,
        .max_framerate = 300,
    /* A06 code for SR-AL7160A-01-501 by zhongbin at 20240422 end */
    },
    .cap = {
        .pclk = 336000000,
        .linelength = 4368,
        .framelength = 2564,
        .mipi_pixel_rate = 269200000,
        .startx = 0,
        .starty = 0,
        .grabwindow_width = 3264,
        .grabwindow_height = 2448,
        .mipi_data_lp2hs_settle_dc = 85,
        .max_framerate = 300,
    },
    .normal_video = {
        .pclk = 336000000,
        .linelength = 4368,
        .framelength = 2564,
        .mipi_pixel_rate = 269200000,
        .startx = 0,
        .starty = 0,
        .grabwindow_width = 3264,
        .grabwindow_height = 2448,
        .mipi_data_lp2hs_settle_dc = 85,
        .max_framerate = 300,
    },
    .hs_video = {
        .pclk = 336000000,
        .linelength = 4368,
        .framelength = 640,
        .mipi_pixel_rate = 270700000,
        .startx = 0,
        .starty = 0,
        .grabwindow_width = 640,
        .grabwindow_height = 480,
        .mipi_data_lp2hs_settle_dc = 85,
        .max_framerate = 1200,
    },
    .slim_video = {
        .pclk = 336000000,
        .linelength = 4368,
        .framelength = 854,
        .mipi_pixel_rate = 269700000,
        .startx = 0,
        .starty = 0,
        .grabwindow_width = 1280,
        .grabwindow_height = 720,
        .mipi_data_lp2hs_settle_dc = 85,
        .max_framerate = 900,
     },
/* A06 code for SR-AL7160A-01-539 by xuyunhui at 20240626 start */
    .min_gain = 84,   // 1.3125x, base 64
    .max_gain = 1024,  // 16x
    .min_gain_iso = 100,
    .gain_step = 1,
    .gain_type = 2, // TODO

    .margin = 8,
    .min_shutter = 1,
    .max_frame_length = 0xffff,
    .ae_shut_delay_frame = 0,
    .ae_sensor_gain_delay_frame = 0,
    .ae_ispGain_delay_frame = 2,
    .frame_time_delay_frame = 3,
    .ihdr_support = 0,                                       // 1:support; 0:not support
    .ihdr_le_firstline = 0,                                  // 1:le first ; 0:se first
    .sensor_mode_num = 5,                                    // support sensor mode num

    .cap_delay_frame = 3,
    .pre_delay_frame = 3,
    .video_delay_frame = 3,
    .hs_video_delay_frame = 3,
    .slim_video_delay_frame = 3,

    .isp_driving_current = ISP_DRIVING_2MA,
    .sensor_interface_type = SENSOR_INTERFACE_TYPE_MIPI,
    .mipi_sensor_type = MIPI_OPHY_NCSI2,                      // 0,MIPI_OPHY_NCSI2;  1,MIPI_OPHY_CSI2
    .mipi_settle_delay_mode = MIPI_SETTLEDELAY_AUTO,          // 0,MIPI_SETTLEDELAY_AUTO; 1,MIPI_SETTLEDELAY_MANNUAL
#if MT811_MIRROR_NORMAL
    .sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_Gb,
#elif MT811_MIRROR_H
    .sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_B,
#elif MT811_MIRROR_V
    .sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_R,
#elif MT811_MIRROR_HV
    .sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_Gr,
#else
    .sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_Gb,
#endif
    .mclk = 24,
    .mipi_lane_num = SENSOR_MIPI_4_LANE,
    .i2c_addr_table = {0x40,0x42,0x44,0x46,0xff},
    /* A06 code for SR-AL7160A-01-501 by zhongbin at 20240422 start */
    .i2c_speed = 400,
    /* A06 code for SR-AL7160A-01-501 by zhongbin at 20240422 start */
};

static  imgsensor_struct imgsensor = {
    .mirror = IMAGE_NORMAL,                                     // mirrorflip information
    .sensor_mode = IMGSENSOR_MODE_INIT,                         // IMGSENSOR_MODE enum value,record current sensor mode,such as: INIT, Preview, Capture, Video,High Speed Video, Slim Video
    .shutter = 0x3D0,                                           // current shutter
    .gain = 0x100,                                              // current gain
    .dummy_pixel = 0,                                           // current dummypixel
    .dummy_line = 0,                                            // current dummyline
    .current_fps = 0,                                           // full size current fps : 24fps for PIP, 30fps for Normal or ZSD
    .autoflicker_en = KAL_FALSE,                                // auto flicker enable: KAL_FALSE for disable auto flicker, KAL_TRUE for enable auto flicker
    .test_pattern = KAL_FALSE,                                  // test pattern mode or not. KAL_FALSE for in test pattern mode, KAL_TRUE for normal output
    .current_scenario_id = MSDK_SCENARIO_ID_CAMERA_PREVIEW,     // current scenario id
    .ihdr_mode = 0,                                             // sensor need support LE, SE with HDR feature
    .i2c_write_id = 0x40,
#ifdef MT811_LONG_EXP
    .current_ae_effective_frame = 2,
#endif
};

/* Sensor output window information */
static  struct SENSOR_WINSIZE_INFO_STRUCT imgsensor_winsize_info[5] =
{
    /* A06 code for SR-AL7160A-01-501 by zhongbin at 20240422 start */
    //{3264, 2448, 0, 0, 3264, 2448, 1632, 1224, 0, 0, 1632, 1224, 0, 0, 1632, 1224}, // Preview
    {3264, 2448, 0, 0, 3264, 2448, 3264, 2448, 0, 0, 3264, 2448, 0, 0, 3264, 2448}, // Preview
    /* A06 code for SR-AL7160A-01-501 by zhongbin at 20240422 end */
    {3264, 2448, 0, 0, 3264, 2448, 3264, 2448, 0, 0, 3264, 2448, 0, 0, 3264, 2448}, // capture
    {3264, 2448, 0, 0, 3264, 2448, 3264, 2448, 0, 0, 3264, 2448, 0, 0, 3264, 2448}, // video
    {3264, 2448, 352, 264, 2560, 1920, 640, 480, 0, 0, 640, 480, 0, 0, 640, 480}, // hight speed video
    {3264, 2448, 352, 504, 2560, 1440, 1280, 720, 0, 0, 1280, 720, 0, 0, 1280, 720} // slim video
};

static kal_uint16 read_cmos_sensor(kal_uint32 addr)
{
    kal_uint16 get_byte=0;
    char pu_send_cmd[2] = {(char)(addr >> 8), (char)(addr & 0xFF) };
    iReadRegI2C(pu_send_cmd, 2, (u8*)&get_byte, 1, imgsensor.i2c_write_id);
    LOG_INF("mt811 read_cmos_sensor addr is 0x%x, val is 0x%x,imgsensor.i2c_write_id=0x%x", addr, get_byte,imgsensor.i2c_write_id);
    return get_byte;
}

static void write_cmos_sensor(kal_uint32 addr, kal_uint32 para)
{
    char pu_send_cmd[3] = {(char)(addr >> 8), (char)(addr & 0xFF), (char)(para & 0xFF)};
    LOG_INF("mt811 write_cmos_sensor addr is 0x%x, val is 0x%x", addr, para);
    iWriteRegI2C(pu_send_cmd, 3, imgsensor.i2c_write_id);
}

static kal_uint16 read_OTP(UINT32 OTP_address)
{
    kal_uint16 OTP_read_byte = 0;
    kal_uint16 addr_H = (OTP_address >> 8) & 0xff;
    kal_uint16 addr_L = OTP_address & 0xff;
    kal_uint16 read_status = 0;
    int k;
    write_cmos_sensor(0x0c00, 0x01);
    write_cmos_sensor(0x0c19, 0x00);
    write_cmos_sensor(0x0c18, 0x01);
    mdelay(2);
    write_cmos_sensor(0x0c13, addr_H);
    write_cmos_sensor(0x0c14, addr_L);
    write_cmos_sensor(0x0c16, 0x20);
    for(k=0; k<5; k++)
    {
        read_status = read_cmos_sensor(0x0c1a);
        if (read_status == 1)
            break;
        else
            mdelay(1);
    }

    OTP_read_byte = read_cmos_sensor(0x0c1e);
    write_cmos_sensor(0x0c17, 0x01);
    write_cmos_sensor(0x0c00, 0x00);
    LOG_INF("mt811 read_OTP  OTP_read_byte 0x%x", OTP_read_byte);

    return OTP_read_byte;
}

static kal_uint16 read_cmos_sensor_moduleid(void)
{
    kal_uint16 get_moduleid=0;
    kal_uint16 flag = 0;

    flag = read_OTP(0x0604);
    LOG_INF("mt811 read_cmos_sensor_moduleid  flag 2 is 0x%x", flag);

    if(flag == 0x01){
        get_moduleid = read_OTP(0x0606);
    }else if(flag == 0x13){
        get_moduleid = read_OTP(0x0D7A);
    }else if(flag == 0x37){
        get_moduleid = read_OTP(0x14EC);
    }else{
        get_moduleid = 0;
    }
    LOG_INF("mt811 read_cmos_sensor_moduleid  val is 0x%x", get_moduleid);
    return get_moduleid;
}

static void set_dummy(void)
{
    LOG_INF("dummyline = %d, dummypixels = %d\n", imgsensor.dummy_line, imgsensor.dummy_pixel);
    write_cmos_sensor(0x04cc, 1);
    write_cmos_sensor(0x041b, imgsensor.frame_length & 0xFF);
    write_cmos_sensor(0x041c, imgsensor.frame_length >> 8);
    write_cmos_sensor(0x041d, imgsensor.line_length & 0xFF);
    write_cmos_sensor(0x041e, imgsensor.line_length >> 8);
    write_cmos_sensor(0x04cc, 0);
}

static void set_max_framerate(UINT16 framerate, kal_bool min_framelength_en)
{
    kal_uint32 frame_length = imgsensor.frame_length;
    LOG_INF("framerate = %d, min framelength should enable = %d\n", framerate,min_framelength_en);

    frame_length = imgsensor.pclk / framerate * 10 / imgsensor.line_length;
    spin_lock(&imgsensor_drv_lock);
    if(frame_length >= imgsensor.min_frame_length)
        imgsensor.frame_length = frame_length;
    else
        imgsensor.frame_length = imgsensor.min_frame_length;
    imgsensor.dummy_line = imgsensor.frame_length - imgsensor.min_frame_length;
    // imgsensor.dummy_line = dummy_line;
    // imgsensor.frame_length = frame_length + imgsensor.dummy_line;
    if(imgsensor.frame_length > imgsensor_info.max_frame_length)
    {
        imgsensor.frame_length = imgsensor_info.max_frame_length;
        imgsensor.dummy_line = imgsensor.frame_length - imgsensor.min_frame_length;
    }
    if(min_framelength_en)
        imgsensor.min_frame_length = imgsensor.frame_length;
    spin_unlock(&imgsensor_drv_lock);
    set_dummy();
}

static void update_shutter_frame_reg(kal_uint8 exp_mode, kal_uint8 long_exp_time, kal_uint32 shutter, kal_uint32 frame_length)
{
    kal_uint32 coarse_time = shutter;
    coarse_time /= 2;
    write_cmos_sensor(0x04cc, 0x01);
    write_cmos_sensor(0x0401, exp_mode & 0xFF);
    write_cmos_sensor(0x0406, long_exp_time & 0xFF);
    write_cmos_sensor(0x041b, frame_length & 0xFF);
    write_cmos_sensor(0x041c, (frame_length >> 8) & 0xFF);
    write_cmos_sensor(0x0402, coarse_time  & 0xFF);
    write_cmos_sensor(0x0403, (coarse_time >> 8) & 0xFF);
    write_cmos_sensor(0x04cc, 0x00);
    LOG_INF("coarse_time =%d, framelength =%d\n", coarse_time, frame_length);
}

static void write_shutter(kal_uint32 shutter)
{
#ifdef MT811_LONG_EXP
    kal_uint8 exp_mode = 0;
    kal_uint8 l_shift = 0;
    kal_uint8 long_exp_time = 0;
#endif
    kal_uint16 realtime_fps = 0;

    spin_lock(&imgsensor_drv_lock);
    if(shutter > imgsensor.min_frame_length - imgsensor_info.margin)
        imgsensor.frame_length = shutter + imgsensor_info.margin;
    else
        imgsensor.frame_length = imgsensor.min_frame_length;
    if(imgsensor.frame_length > imgsensor_info.max_frame_length)
        imgsensor.frame_length = imgsensor_info.max_frame_length;
    spin_unlock(&imgsensor_drv_lock);
    if(shutter < imgsensor_info.min_shutter)
        shutter = imgsensor_info.min_shutter;

    realtime_fps = imgsensor.pclk / imgsensor.line_length * 10 / imgsensor.frame_length;
    if(imgsensor.autoflicker_en)
    {
        if(realtime_fps >= 297 && realtime_fps <= 305)
            set_max_framerate(296, 0);
        else if(realtime_fps >= 147 && realtime_fps <= 150)
            set_max_framerate(146, 0);
        else
            LOG_INF("autoflicker enable do not need change framerate\n");
    }
#ifdef MT811_LONG_EXP
    /* long expsoure */
    if (shutter > (imgsensor_info.max_frame_length - imgsensor_info.margin))
    {
        for (l_shift = 1; l_shift < MAX_CIT_LSHIFT; l_shift++) {
            if ((shutter >> l_shift)  < (imgsensor_info.max_frame_length - imgsensor_info.margin))
                break;
        }
        if (l_shift > MAX_CIT_LSHIFT) {
            LOG_INF("unable to set such a long exposure %d\n", shutter);
            l_shift = MAX_CIT_LSHIFT;
        }
        exp_mode = 0x01;
        long_exp_time = 0x08 | l_shift;
        shutter = shutter >> l_shift;
        imgsensor.frame_length = shutter + imgsensor_info.margin;
        LOG_INF("enter long exposure mode, time is %d", l_shift);

        // Update frame length and shutter of long exposure mode
        update_shutter_frame_reg(exp_mode, long_exp_time, shutter, imgsensor.frame_length);

        /* Frame exposure mode customization for LE*/
        imgsensor.ae_frm_mode.frame_mode_1 = IMGSENSOR_AE_MODE_SE;
        imgsensor.ae_frm_mode.frame_mode_2 = IMGSENSOR_AE_MODE_SE;
    }
    else
#endif
    {
        // Update frame length and shutter of normal mode
        update_shutter_frame_reg(0, 0, shutter, imgsensor.frame_length);
    }
#ifdef MT811_LONG_EXP
    imgsensor.current_ae_effective_frame = 2;
#endif
    LOG_INF("shutter =%d, framelength =%d, real_fps =%d\n", shutter, imgsensor.frame_length, realtime_fps);
}

/*************************************************************************
* FUNCTION
*       set_shutter
*
* DESCRIPTION
*       This function set e-shutter of sensor to change exposure time.
*
* PARAMETERS
*       iShutter : exposured lines
*
* RETURNS
*       None
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
    kal_uint16 val = MIN_GAIN;

    if (gain < imgsensor_info.min_gain)
        val = MIN_GAIN;
    else if (gain > imgsensor_info.max_gain)
        val = MAX_GAIN;
    else
        val = (kal_uint16)((gain*(MAX_GAIN - MIN_GAIN)) / (imgsensor_info.max_gain - imgsensor_info.min_gain)) - 16;

    LOG_INF("plantform gain=%d, sensor gain= 0x%x\n", gain, val);
    return val;
}

/*************************************************************************
* FUNCTION
*       set_gain
*
* DESCRIPTION
*       This function is to set global gain to sensor.
*
* PARAMETERS
*       iGain : sensor global gain(base: 0x40)
*
* RETURNS
*       the actually gain set to sensor.
*
* GLOBALS AFFECTED
*
*************************************************************************/
static kal_uint16 set_gain(kal_uint16 gain)
{
    kal_uint16 reg_gain;
    
    if (gain < BASEGAIN)
    {
        LOG_INF("error gain setting = %d, base gain = %d", gain, BASEGAIN);
        gain = BASEGAIN;
    }
    if (gain > 16 * BASEGAIN)
    {
        LOG_INF("error gain setting = %d, max gain = %d", gain, 16 * BASEGAIN);
        gain = 16 * BASEGAIN;
    }

    reg_gain = gain2reg(gain);
    spin_lock(&imgsensor_drv_lock);
    imgsensor.gain = 4 * reg_gain + 64;
    spin_unlock(&imgsensor_drv_lock);
    LOG_INF("gain = %d, reg_gain = 0x%x\n", gain, reg_gain);

    write_cmos_sensor(0x04cc, 1);
    write_cmos_sensor(0x04d1, reg_gain);
#ifdef SBL_TH
    if (BASEGAIN * SBL_TH > gain)
        write_cmos_sensor(0x0582, REG_SYS_ANA_WR02_LG_VAL);
    else
        write_cmos_sensor(0x0582, REG_SYS_ANA_WR02_HG_VAL);
#endif
    write_cmos_sensor(0x04cc, 0);

    return gain;
}

/*************************************************************************
* FUNCTION
*       night_mode
*
* DESCRIPTION
*       This function night mode of sensor.
*
* PARAMETERS
*       bEnable: KAL_TRUE -> enable night mode, otherwise, disable night mode
*
* RETURNS
*       None
*
* GLOBALS AFFECTED
*
*************************************************************************/
static void night_mode(kal_bool enable)
{
}

static void sensor_init(void)
{
    LOG_INF("sensor_init E\n");
    write_cmos_sensor(0x0100, 0x01);
    write_cmos_sensor(0x0082, 0x03);
    write_cmos_sensor(0x0581, 0xe0);
    write_cmos_sensor(0x0589, 0xf1);
    write_cmos_sensor(0x0580, 0x80);
    write_cmos_sensor(0x0054, 0x00);
    write_cmos_sensor(0x0055, 0x61);
    write_cmos_sensor(0x0056, 0x07);
    write_cmos_sensor(0x0057, 0x58);
    write_cmos_sensor(0x0058, 0x04);
    write_cmos_sensor(0x0059, 0x3f);
    write_cmos_sensor(0x005a, 0x9e);
    write_cmos_sensor(0x005b, 0x45);
    write_cmos_sensor(0x005c, 0x16);
    write_cmos_sensor(0x005d, 0x25);
    write_cmos_sensor(0x005e, 0x3c);
    write_cmos_sensor(0x005f, 0x58);
    write_cmos_sensor(0x0068, 0x10);
    write_cmos_sensor(0x0069, 0x3f);
    write_cmos_sensor(0x006a, 0x4e);
    write_cmos_sensor(0x006b, 0xb5);
    write_cmos_sensor(0x006c, 0x02);
    write_cmos_sensor(0x006d, 0x08);
    write_cmos_sensor(0x0082, 0x03);
    write_cmos_sensor(0x0080, 0x10);
    msleep(1);
    write_cmos_sensor(0x0081, 0x01);
    write_cmos_sensor(0x0078, 0x04);
    write_cmos_sensor(0x0587, 0x42);
    write_cmos_sensor(0x0558, 0x00);
    write_cmos_sensor(0x0079, 0x70);
    write_cmos_sensor(0x0583, 0x03);
    write_cmos_sensor(0x058c, 0x30);
    write_cmos_sensor(0x041f, 0x00);
    write_cmos_sensor(0x0420, 0x00);
    write_cmos_sensor(0x0421, 0x21);
    write_cmos_sensor(0x0422, 0x02);
    write_cmos_sensor(0x0425, 0x85);
    write_cmos_sensor(0x0426, 0x08);
    write_cmos_sensor(0x0427, 0x01);
    write_cmos_sensor(0x0428, 0x00);
    write_cmos_sensor(0x0429, 0x86);
    write_cmos_sensor(0x042a, 0x08);
    write_cmos_sensor(0x044b, 0x01);
    write_cmos_sensor(0x044c, 0x00);
    write_cmos_sensor(0x044d, 0x02);
    write_cmos_sensor(0x044e, 0x00);
    write_cmos_sensor(0x044f, 0x03);
    write_cmos_sensor(0x0450, 0x00);
    write_cmos_sensor(0x0451, 0x04);
    write_cmos_sensor(0x0452, 0x00);
    write_cmos_sensor(0x0453, 0x25);
    write_cmos_sensor(0x0454, 0x00);
    write_cmos_sensor(0x0455, 0x1d);
    write_cmos_sensor(0x0456, 0x02);
    write_cmos_sensor(0x0457, 0x27);
    write_cmos_sensor(0x0458, 0x00);
    write_cmos_sensor(0x0459, 0x1e);
    write_cmos_sensor(0x045a, 0x02);
    write_cmos_sensor(0x045b, 0x05);
    write_cmos_sensor(0x045c, 0x00);
    write_cmos_sensor(0x045d, 0x48);
    write_cmos_sensor(0x045e, 0x00);
    write_cmos_sensor(0x045f, 0x06);
    write_cmos_sensor(0x0460, 0x00);
    write_cmos_sensor(0x0461, 0x37);
    write_cmos_sensor(0x0462, 0x00);
    write_cmos_sensor(0x0463, 0x50);
    write_cmos_sensor(0x0464, 0x00);
    write_cmos_sensor(0x0465, 0x53);
    write_cmos_sensor(0x0466, 0x00);
    write_cmos_sensor(0x0467, 0x20);
    write_cmos_sensor(0x0468, 0x01);
    write_cmos_sensor(0x0469, 0x23);
    write_cmos_sensor(0x046a, 0x01);
    write_cmos_sensor(0x046b, 0x6b);
    write_cmos_sensor(0x046c, 0x00);
    write_cmos_sensor(0x046d, 0xd3);
    write_cmos_sensor(0x046e, 0x00);
    write_cmos_sensor(0x046f, 0x3b);
    write_cmos_sensor(0x0470, 0x01);
    write_cmos_sensor(0x0471, 0x1c);
    write_cmos_sensor(0x0472, 0x02);
    write_cmos_sensor(0x0473, 0x63);
    write_cmos_sensor(0x0474, 0x00);
    write_cmos_sensor(0x0475, 0xd3);
    write_cmos_sensor(0x0476, 0x00);
    write_cmos_sensor(0x0477, 0x33);
    write_cmos_sensor(0x0478, 0x01);
    write_cmos_sensor(0x0479, 0x1c);
    write_cmos_sensor(0x047a, 0x02);
    write_cmos_sensor(0x047b, 0x4a);
    write_cmos_sensor(0x047c, 0x00);
    write_cmos_sensor(0x047d, 0xd4);
    write_cmos_sensor(0x047e, 0x00);
    write_cmos_sensor(0x047f, 0x1a);
    write_cmos_sensor(0x0480, 0x01);
    write_cmos_sensor(0x0481, 0x1d);
    write_cmos_sensor(0x0482, 0x02);
    write_cmos_sensor(0x0483, 0xfe);
    write_cmos_sensor(0x0484, 0xff);
    write_cmos_sensor(0x0485, 0xff);
    write_cmos_sensor(0x0486, 0xff);
    write_cmos_sensor(0x0487, 0x5c);
    write_cmos_sensor(0x0488, 0x00);
    write_cmos_sensor(0x0489, 0xd4);
    write_cmos_sensor(0x048a, 0x00);
    write_cmos_sensor(0x048b, 0x2c);
    write_cmos_sensor(0x048c, 0x01);
    write_cmos_sensor(0x048d, 0x1d);
    write_cmos_sensor(0x048e, 0x02);
    write_cmos_sensor(0x048f, 0x01);
    write_cmos_sensor(0x0490, 0x00);
    write_cmos_sensor(0x0491, 0x4c);
    write_cmos_sensor(0x0492, 0x00);
    write_cmos_sensor(0x0493, 0x02);
    write_cmos_sensor(0x0494, 0x00);
    write_cmos_sensor(0x0495, 0x1b);
    write_cmos_sensor(0x0496, 0x00);
    write_cmos_sensor(0x0497, 0xd4);
    write_cmos_sensor(0x0498, 0x00);
    write_cmos_sensor(0x0499, 0x2f);
    write_cmos_sensor(0x049a, 0x01);
    write_cmos_sensor(0x042b, 0xf7);
    write_cmos_sensor(0x042c, 0x02);
    write_cmos_sensor(0x042d, 0x29);
    write_cmos_sensor(0x042e, 0x03);
    write_cmos_sensor(0x042f, 0xd5);
    write_cmos_sensor(0x0430, 0x00);
    write_cmos_sensor(0x0431, 0x07);
    write_cmos_sensor(0x0432, 0x01);
    write_cmos_sensor(0x0433, 0x3b);
    write_cmos_sensor(0x0434, 0x07);
    write_cmos_sensor(0x0435, 0x6d);
    write_cmos_sensor(0x0436, 0x07);
    write_cmos_sensor(0x0437, 0x19);
    write_cmos_sensor(0x0438, 0x05);
    write_cmos_sensor(0x0439, 0x4b);
    write_cmos_sensor(0x043a, 0x05);
    write_cmos_sensor(0x049b, 0xd7);
    write_cmos_sensor(0x049c, 0x00);
    write_cmos_sensor(0x049d, 0xdb);
    write_cmos_sensor(0x049e, 0x00);
    write_cmos_sensor(0x049f, 0xd9);
    write_cmos_sensor(0x04a0, 0x00);
    write_cmos_sensor(0x04a1, 0xdd);
    write_cmos_sensor(0x04a2, 0x00);
    write_cmos_sensor(0x04a3, 0x62);
    write_cmos_sensor(0x04a4, 0x00);
    write_cmos_sensor(0x04a5, 0x9a);
    write_cmos_sensor(0x04a6, 0x01);
    write_cmos_sensor(0x04a7, 0x1f);
    write_cmos_sensor(0x04a8, 0x02);
    write_cmos_sensor(0x04a9, 0x21);
    write_cmos_sensor(0x04aa, 0x02);
    write_cmos_sensor(0x04ab, 0x20);
    write_cmos_sensor(0x04ac, 0x02);
    write_cmos_sensor(0x04ad, 0x21);
    write_cmos_sensor(0x04ae, 0x02);
    write_cmos_sensor(0x04b7, 0xd6);
    write_cmos_sensor(0x04b8, 0x00);
    write_cmos_sensor(0x04b9, 0x1f);
    write_cmos_sensor(0x04ba, 0x02);
    write_cmos_sensor(0x04d2, 0x80);
    write_cmos_sensor(0x0412, 0x1a);
    write_cmos_sensor(0x0500, 0x07);
    write_cmos_sensor(0x0582, 0x1f);
    write_cmos_sensor(0x0584, 0xbd);
    write_cmos_sensor(0x0585, 0x37);
    write_cmos_sensor(0x0586, 0x04);
    write_cmos_sensor(0x0588, 0x88);
    write_cmos_sensor(0x058b, 0x40);
    write_cmos_sensor(0x058d, 0x40);
    write_cmos_sensor(0x058e, 0x2f);
    write_cmos_sensor(0x058f, 0x00);
    write_cmos_sensor(0x0590, 0x00);
    write_cmos_sensor(0x0593, 0xc8);
    write_cmos_sensor(0x05c0, 0x44);
    write_cmos_sensor(0x0680, 0x07);
    write_cmos_sensor(0x0681, 0x01);
    msleep(1);
    write_cmos_sensor(0x0600, 0x01);
    write_cmos_sensor(0x0604, 0x0f);
    write_cmos_sensor(0x0622, 0x00);
    write_cmos_sensor(0x0603, 0x10);
    write_cmos_sensor(0x0802, 0x01);
    write_cmos_sensor(0x0808, 0x01);
    write_cmos_sensor(0x0810, 0x0a);
    write_cmos_sensor(0x0811, 0x40);
    write_cmos_sensor(0x0816, 0x20);
}
/*
static void preview_setting(void)
{
    // Sensor Information////////////////////////////
    // Sensor            : mt811
    // Date              : 2023-10-30
    // Image size        : 1632x1224(binning)
    // Frame Length      : 642@2lane/641@4lane
    // Line Length       : 4280@2lane/4368@4lane
    // line Time         : 25939@2lane/26000@4lane
    // Max Fps           : 60.00fps
    // Pixel order       : Green 1st (=GB)
    // X-mirror/Y-flip   : x-mirror/y-flip
    // BLC offset        : 64code
    ////////////////////////////////////////////////
    LOG_INF("preview_setting E\n");
    write_cmos_sensor(0x0400, 0x01);
    write_cmos_sensor(0x0401, 0x00);
    write_cmos_sensor(0x04cc, 0x01);
    write_cmos_sensor(0x0402, 0x00);
    write_cmos_sensor(0x0403, 0x02);
    write_cmos_sensor(0x041b, 0x02);
    write_cmos_sensor(0x041c, 0x05);
    write_cmos_sensor(0x041d, 0x10);
    write_cmos_sensor(0x041e, 0x11);
    write_cmos_sensor(0x04cd, 0x00);
    write_cmos_sensor(0x04ce, 0x01);
    write_cmos_sensor(0x04d1, 0x08);
    write_cmos_sensor(0x04cc, 0x00);
    write_cmos_sensor(0x0408, 0x00);
    write_cmos_sensor(0x0409, 0x00);
    write_cmos_sensor(0x040a, 0x00);
    write_cmos_sensor(0x040b, 0x00);
    write_cmos_sensor(0x040c, 0x00);
#if MT811_MIRROR_NORMAL
    write_cmos_sensor(0x040d, 0x01);
    write_cmos_sensor(0x0640, 0x01);
#elif MT811_MIRROR_H
    write_cmos_sensor(0x040d, 0x01);
    write_cmos_sensor(0x0640, 0x00);
#elif MT811_MIRROR_V
    write_cmos_sensor(0x040d, 0x00);
    write_cmos_sensor(0x0640, 0x01);
#elif MT811_MIRROR_HV
    write_cmos_sensor(0x040d, 0x00);
    write_cmos_sensor(0x0640, 0x00);
#else
    write_cmos_sensor(0x040d, 0x01);
    write_cmos_sensor(0x0640, 0x01);
#endif
    write_cmos_sensor(0x0414, 0xc0);
    write_cmos_sensor(0x0801, 0xd0);
    write_cmos_sensor(0x0c00, 0x01);
    write_cmos_sensor(0x0c18, 0x01);
#if MT811_MIRROR_NORMAL
    write_cmos_sensor(0x0820, 0x11);
#elif MT811_MIRROR_H
    write_cmos_sensor(0x0820, 0x11);
#elif MT811_MIRROR_V
    write_cmos_sensor(0x0820, 0x01);
#elif MT811_MIRROR_HV
    write_cmos_sensor(0x0820, 0x01);
#else
    write_cmos_sensor(0x0820, 0x11);
#endif	
    write_cmos_sensor(0x0821, 0x11);
    write_cmos_sensor(0x0822, 0x00);
    write_cmos_sensor(0x0834, 0x00);
    write_cmos_sensor(0x0835, 0x00);
    write_cmos_sensor(0x0825, 0x01);
    msleep(2);
    write_cmos_sensor(0x0c00, 0x00);
    write_cmos_sensor(0x0860, 0x00);
    write_cmos_sensor(0x0861, 0x00);
    write_cmos_sensor(0x0862, 0x00);
    write_cmos_sensor(0x0863, 0x00);
    write_cmos_sensor(0x0864, 0x60);
    write_cmos_sensor(0x0865, 0x06);
    write_cmos_sensor(0x0866, 0xc8);
    write_cmos_sensor(0x0867, 0x04);
    write_cmos_sensor(0x0800, 0x01);
    write_cmos_sensor(0x0803, 0x01);
}*/

static void capture_setting(kal_uint16 currefps)
{
    // Sensor Information////////////////////////////
    // Sensor           : mt811
    // Date             : 2023-10-30
    // Image size       : 3264x2448(full_size)
    // Frame Length     : 1285@2lane/1282@4lane
    // Line Length      : 4280@2lane/4368@4lane
    // line Time        : 25939@2lane/26000@4lane
    // Pixel order      : Green 1st (=GB)
    // Max Fps          : 30.00fps
    // Pixel order      : Green 1st (=GB)
    // X-mirror/Y-flip  : x-mirror/y-flip
    // BLC offset       : 64code
    ////////////////////////////////////////////////
    LOG_INF("capture_setting E\n");
    write_cmos_sensor(0x0400, 0x00);
    write_cmos_sensor(0x0401, 0x00);
    write_cmos_sensor(0x04cc, 0x01);
    write_cmos_sensor(0x0402, 0x00);
    write_cmos_sensor(0x0403, 0x02);
    write_cmos_sensor(0x041b, 0x04);
    write_cmos_sensor(0x041c, 0x0a);
    write_cmos_sensor(0x041d, 0x10);
    write_cmos_sensor(0x041e, 0x11);
    write_cmos_sensor(0x04cd, 0x00);
    write_cmos_sensor(0x04ce, 0x01);
    write_cmos_sensor(0x04d1, 0x08);
    write_cmos_sensor(0x04cc, 0x00);
    write_cmos_sensor(0x0408, 0x00);
    write_cmos_sensor(0x0409, 0x00);
    write_cmos_sensor(0x040a, 0x00);
    write_cmos_sensor(0x040b, 0x00);
    write_cmos_sensor(0x040c, 0x00);
#if MT811_MIRROR_NORMAL
    write_cmos_sensor(0x040d, 0x01);
    write_cmos_sensor(0x0640, 0x01);
#elif MT811_MIRROR_H
    write_cmos_sensor(0x040d, 0x01);
    write_cmos_sensor(0x0640, 0x00);
#elif MT811_MIRROR_V
    write_cmos_sensor(0x040d, 0x00);
    write_cmos_sensor(0x0640, 0x01);
#elif MT811_MIRROR_HV
    write_cmos_sensor(0x040d, 0x00);
    write_cmos_sensor(0x0640, 0x00);
#else
    write_cmos_sensor(0x040d, 0x01);
    write_cmos_sensor(0x0640, 0x01);
#endif
    write_cmos_sensor(0x0414, 0xe0);
    write_cmos_sensor(0x0801, 0xc1);
    write_cmos_sensor(0x0c00, 0x01);
    write_cmos_sensor(0x0c18, 0x01);
#if MT811_MIRROR_NORMAL
    write_cmos_sensor(0x0820, 0x11);
#elif MT811_MIRROR_H
    write_cmos_sensor(0x0820, 0x11);
#elif MT811_MIRROR_V
    write_cmos_sensor(0x0820, 0x01);
#elif MT811_MIRROR_HV
    write_cmos_sensor(0x0820, 0x01);
#else
    write_cmos_sensor(0x0820, 0x11);
#endif	
    write_cmos_sensor(0x0821, 0x11);
    write_cmos_sensor(0x0822, 0x00);
    write_cmos_sensor(0x0834, 0x00);
    write_cmos_sensor(0x0835, 0x00);
    write_cmos_sensor(0x0825, 0x01);
    msleep(2);
    write_cmos_sensor(0x0c00, 0x00);
    write_cmos_sensor(0x0860, 0x00);
    write_cmos_sensor(0x0861, 0x00);
    write_cmos_sensor(0x0862, 0x00);
    write_cmos_sensor(0x0863, 0x00);
    write_cmos_sensor(0x0864, 0xc0);
    write_cmos_sensor(0x0865, 0x0c);
    write_cmos_sensor(0x0866, 0x90);
    write_cmos_sensor(0x0867, 0x09);
    write_cmos_sensor(0x0800, 0x01);
    write_cmos_sensor(0x0803, 0x01);
}

static void normal_video_setting(kal_uint16 currefps)
{
    LOG_INF("normal_video_setting E\n");
    capture_setting(currefps);
}

static void hs_video_setting(void)
{
    // Sensor Information////////////////////////////
    // Sensor            : mt811
    // Date              : 2023-10-30
    // Customer          : SPRD_validation
    // Image size        : 640x480(bining+crop)
    // Frame Length     : 321@2lane/320@4lane
    // Line Length      : 4280@2lane/4368@4lane
    // line Time        : 25939@2lane/26000@4lane
    // Max Fps           : 120.00fps
    // Pixel order       : Green 1st (=GB)
    // X-mirror/Y-flip   : x-mirror/y-flip
    // BLC offset        : 64code
    ////////////////////////////////////////////////
    LOG_INF("hs_video_setting E\n");
    write_cmos_sensor(0x0400, 0x02);
    write_cmos_sensor(0x0401, 0x00);
    write_cmos_sensor(0x04cc, 0x01);
    write_cmos_sensor(0x0402, 0x00);
    write_cmos_sensor(0x0403, 0x02);
    write_cmos_sensor(0x041b, 0x80);
    write_cmos_sensor(0x041c, 0x02);
    write_cmos_sensor(0x041d, 0x10);
    write_cmos_sensor(0x041e, 0x11);
    write_cmos_sensor(0x04cd, 0x00);
    write_cmos_sensor(0x04ce, 0x01);
    write_cmos_sensor(0x04d1, 0x08);
    write_cmos_sensor(0x04cc, 0x00);
    write_cmos_sensor(0x0408, 0x01);
    write_cmos_sensor(0x0409, 0x08);
    write_cmos_sensor(0x040a, 0x01);
    write_cmos_sensor(0x040b, 0x80);
    write_cmos_sensor(0x040c, 0x08);
#if MT811_MIRROR_NORMAL
    write_cmos_sensor(0x040d, 0x01);
    write_cmos_sensor(0x0640, 0x01);
#elif MT811_MIRROR_H
    write_cmos_sensor(0x040d, 0x01);
    write_cmos_sensor(0x0640, 0x00);
#elif MT811_MIRROR_V
    write_cmos_sensor(0x040d, 0x00);
    write_cmos_sensor(0x0640, 0x01);
#elif MT811_MIRROR_HV
    write_cmos_sensor(0x040d, 0x00);
    write_cmos_sensor(0x0640, 0x00);
#else
    write_cmos_sensor(0x040d, 0x01);
    write_cmos_sensor(0x0640, 0x01);
#endif
    write_cmos_sensor(0x0414, 0x80);
    write_cmos_sensor(0x0801, 0xd0);
    write_cmos_sensor(0x0c00, 0x01);
    write_cmos_sensor(0x0c18, 0x01);
#if MT811_MIRROR_NORMAL
    write_cmos_sensor(0x0820, 0x11);
#elif MT811_MIRROR_H
    write_cmos_sensor(0x0820, 0x11);
#elif MT811_MIRROR_V
    write_cmos_sensor(0x0820, 0x01);
#elif MT811_MIRROR_HV
    write_cmos_sensor(0x0820, 0x01);
#else
    write_cmos_sensor(0x0820, 0x11);
#endif	
    write_cmos_sensor(0x0821, 0x11);
    write_cmos_sensor(0x0822, 0x00);
    write_cmos_sensor(0x0834, 0x08);
    write_cmos_sensor(0x0835, 0x01);
    write_cmos_sensor(0x0825, 0x01);
    msleep(2);
    write_cmos_sensor(0x0c00, 0x00);
    write_cmos_sensor(0x0860, 0x58);
    write_cmos_sensor(0x0861, 0x00);
    write_cmos_sensor(0x0862, 0x00);
    write_cmos_sensor(0x0863, 0x00);
    write_cmos_sensor(0x0864, 0x80);
    write_cmos_sensor(0x0865, 0x02);
    write_cmos_sensor(0x0866, 0xe0);
    write_cmos_sensor(0x0867, 0x01);
    write_cmos_sensor(0x0800, 0x01);
    write_cmos_sensor(0x0803, 0x01);
}

static void slim_video_setting(void)
{
    // Sensor Information////////////////////////////
    // Sensor            : mt811
    // Date              : 2023-10-30
    // Customer          : SPRD_validation
    // Image size        : 1280x720(bining+crop)
    // Frame Length     : 428@2lane/427@4lane
    // Line Length      : 4280@2lane/4368@4lane
    // line Time        : 25939@2lane/26000@4lane
    // Max Fps           : 90.00fps
    // Pixel order       : Green 1st (=GB)
    // X-mirror/Y-flip   : x-mirror/y-flip
    // BLC offset        : 64code
    ////////////////////////////////////////////////
    LOG_INF("slim_video_setting E\n");
    write_cmos_sensor(0x0400, 0x01);
    write_cmos_sensor(0x0401, 0x00);
    write_cmos_sensor(0x04cc, 0x01);
    write_cmos_sensor(0x0402, 0x00);
    write_cmos_sensor(0x0403, 0x02);
    write_cmos_sensor(0x041b, 0x56);
    write_cmos_sensor(0x041c, 0x03);
    write_cmos_sensor(0x041d, 0x10);
    write_cmos_sensor(0x041e, 0x11);
    write_cmos_sensor(0x04cd, 0x00);
    write_cmos_sensor(0x04ce, 0x01);
    write_cmos_sensor(0x04d1, 0x08);
    write_cmos_sensor(0x04cc, 0x00);
    write_cmos_sensor(0x0408, 0x01);
    write_cmos_sensor(0x0409, 0xf8);
    write_cmos_sensor(0x040a, 0x01);
    write_cmos_sensor(0x040b, 0x94);
    write_cmos_sensor(0x040c, 0x07);
#if MT811_MIRROR_NORMAL
    write_cmos_sensor(0x040d, 0x01);
    write_cmos_sensor(0x0640, 0x01);
#elif MT811_MIRROR_H
    write_cmos_sensor(0x040d, 0x01);
    write_cmos_sensor(0x0640, 0x00);
#elif MT811_MIRROR_V
    write_cmos_sensor(0x040d, 0x00);
    write_cmos_sensor(0x0640, 0x01);
#elif MT811_MIRROR_HV
    write_cmos_sensor(0x040d, 0x00);
    write_cmos_sensor(0x0640, 0x00);
#else
    write_cmos_sensor(0x040d, 0x01);
    write_cmos_sensor(0x0640, 0x01);
#endif
    write_cmos_sensor(0x0414, 0xc0);
    write_cmos_sensor(0x0801, 0xd0);
    write_cmos_sensor(0x0c00, 0x01);
    write_cmos_sensor(0x0c18, 0x01);
#if MT811_MIRROR_NORMAL
    write_cmos_sensor(0x0820, 0x11);
#elif MT811_MIRROR_H
    write_cmos_sensor(0x0820, 0x11);
#elif MT811_MIRROR_V
    write_cmos_sensor(0x0820, 0x01);
#elif MT811_MIRROR_HV
    write_cmos_sensor(0x0820, 0x01);
#else	
    write_cmos_sensor(0x0820, 0x11);
#endif
    write_cmos_sensor(0x0821, 0x11);
    write_cmos_sensor(0x0822, 0x00);
    write_cmos_sensor(0x0834, 0xf8);
    write_cmos_sensor(0x0835, 0x01);
    write_cmos_sensor(0x0825, 0x01);
    msleep(2);
    write_cmos_sensor(0x0c00, 0x00);
    write_cmos_sensor(0x0860, 0xb0);
    write_cmos_sensor(0x0861, 0x00);
    write_cmos_sensor(0x0862, 0x00);
    write_cmos_sensor(0x0863, 0x00);
    write_cmos_sensor(0x0864, 0x00);
    write_cmos_sensor(0x0865, 0x05);
    write_cmos_sensor(0x0866, 0xd0);
    write_cmos_sensor(0x0867, 0x02);
    write_cmos_sensor(0x0800, 0x01);
    write_cmos_sensor(0x0803, 0x01);
}
/*************************************************************************
* FUNCTION
*       get_imgsensor_id
*
* DESCRIPTION
*       This function get the sensor ID
*
* PARAMETERS
*       *sensorID : return the sensor ID
*
* RETURNS
*       None
*
* GLOBALS AFFECTED
*
*************************************************************************/
static kal_uint32 get_imgsensor_id(UINT32 *sensor_id)
{
    kal_uint8 i = 0;
    kal_uint8 retry = 2;
    LOG_INF("mt811 get_imgsensor_id imgsensor_info.sensor_id=0x%x\n", imgsensor_info.sensor_id);
    while(imgsensor_info.i2c_addr_table[i] != 0xff)
    {
        spin_lock(&imgsensor_drv_lock);
        imgsensor.i2c_write_id = imgsensor_info.i2c_addr_table[i];
        spin_unlock(&imgsensor_drv_lock);
        do
        {
            msleep(10);
            *sensor_id = ((read_cmos_sensor(0x0000) << 8) | read_cmos_sensor(0x0001)) + 1;
            if (*sensor_id == imgsensor_info.sensor_id)
            {
                kal_uint32 mt811VendorId = 0;
                mt811VendorId = read_cmos_sensor_moduleid();
                if(mt811VendorId != 0x00 && mt811VendorId != 0x84){
                    LOG_INF("[cameradebug-compatible] return vendorId error,vendorId=0x%x",mt811VendorId);
                    *sensor_id = 0xFFFFFFFF;
                    return ERROR_SENSOR_CONNECT_FAIL;
                }
                LOG_INF("mt811 i2c config id: 0x%x, sensor id: 0x%x\n", imgsensor.i2c_write_id, *sensor_id);
                return ERROR_NONE;
            }
            LOG_INF("mt811 Read sensor id fail, config id:0x%x id: 0x%x\n", imgsensor.i2c_write_id, *sensor_id);
            retry--;
        } while(retry > 0);
        i++;
        retry = 3;
    }
    if (*sensor_id != imgsensor_info.sensor_id) {
        *sensor_id = 0xFFFFFFFF;
        return ERROR_SENSOR_CONNECT_FAIL;
    }
    return ERROR_NONE;
}


/*************************************************************************
* FUNCTION
*       open
*
* DESCRIPTION
*       This function initialize the registers of CMOS sensor
*
* PARAMETERS
*       None
*
* RETURNS
*       None
*
* GLOBALS AFFECTED
*
*************************************************************************/
static kal_uint32 open(void)
{
    kal_uint8 i = 0;
    kal_uint8 retry = 2;
    kal_uint16 sensor_id = 0;
    LOG_INF("[O8U Enter] mt811_mipi_raw open\n");
    LOG_INF("PLATFORM:MT6877, MIPI 4LANE\n");

    while (imgsensor_info.i2c_addr_table[i] != 0xff)
    {
        LOG_INF("[O8U Enter] mt811_mipi_raw imgsensor_info.i2c_addr_table[i]=%d\n",imgsensor_info.i2c_addr_table[i]);
        spin_lock(&imgsensor_drv_lock);
        imgsensor.i2c_write_id = imgsensor_info.i2c_addr_table[i];
        spin_unlock(&imgsensor_drv_lock);
        do
        {
            msleep(10);
            sensor_id = ((read_cmos_sensor(0x0000) << 8) | read_cmos_sensor(0x0001)) + 1;
            if (sensor_id == imgsensor_info.sensor_id)
            {
                LOG_INF("i2c write id: 0x%x, sensor id: 0x%x\n", imgsensor.i2c_write_id, sensor_id);
                break;
            }
            LOG_INF("Read sensor id fail, write id:0x%x id: 0x%x\n", imgsensor.i2c_write_id, sensor_id);
            retry--;
        } while(retry > 0);
        i++;
        if (sensor_id == imgsensor_info.sensor_id)
            break;
        retry = 3;
    }
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
    imgsensor.ihdr_mode = 0;
    imgsensor.test_pattern = KAL_FALSE;
    imgsensor.current_fps = imgsensor_info.pre.max_framerate;
    spin_unlock(&imgsensor_drv_lock);

    return ERROR_NONE;
}


/*************************************************************************
* FUNCTION
*       close
*
* DESCRIPTION
*
*
* PARAMETERS
*       None
*
* RETURNS
*       None
*
* GLOBALS AFFECTED
*
*************************************************************************/
static kal_uint32 close(void)
{
    LOG_INF("E\n");

    return ERROR_NONE;
}


/*************************************************************************
* FUNCTION
* preview
*
* DESCRIPTION
*       This function start the sensor preview.
*
* PARAMETERS
*       *image_window : address pointer of pixel numbers in one period of HSYNC
*  *sensor_config_data : address pointer of line numbers in one period of VSYNC
*
* RETURNS
*       None
*
* GLOBALS AFFECTED
*
*************************************************************************/
static kal_uint32 preview(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
                              MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
    LOG_INF("preview E\n");

    spin_lock(&imgsensor_drv_lock);
    imgsensor.sensor_mode = IMGSENSOR_MODE_PREVIEW;
    imgsensor.pclk = imgsensor_info.pre.pclk;
    // imgsensor.video_mode = KAL_FALSE;
    imgsensor.line_length = imgsensor_info.pre.linelength;
    imgsensor.frame_length = imgsensor_info.pre.framelength;
    imgsensor.min_frame_length = imgsensor_info.pre.framelength;
    imgsensor.autoflicker_en = KAL_FALSE;
    spin_unlock(&imgsensor_drv_lock);
    /* A06 code for SR-AL7160A-01-501 by zhongbin at 20240422 start */
    //preview_setting();
    capture_setting(imgsensor.current_fps);
    /* A06 code for SR-AL7160A-01-501 by zhongbin at 20240422 end */

    return ERROR_NONE;
}       /*      preview   */

/*************************************************************************
* FUNCTION
*       capture
*
* DESCRIPTION
*       This function setup the CMOS sensor in capture MY_OUTPUT mode
*
* PARAMETERS
*
* RETURNS
*       None
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
    if (imgsensor.current_fps == imgsensor_info.cap1.max_framerate)
    {
        imgsensor.pclk = imgsensor_info.cap1.pclk;
        imgsensor.line_length = imgsensor_info.cap1.linelength;
        imgsensor.frame_length = imgsensor_info.cap1.framelength;
        imgsensor.min_frame_length = imgsensor_info.cap1.framelength;
        imgsensor.autoflicker_en = KAL_FALSE;
    }
    else if(imgsensor.current_fps == imgsensor_info.cap2.max_framerate)
    {
        if (imgsensor.current_fps != imgsensor_info.cap.max_framerate)
        {
            LOG_INF("Warning: current_fps %d fps is not support, so use cap1's setting: %d fps!\n",
                imgsensor.current_fps,imgsensor_info.cap1.max_framerate/10);
        }
        imgsensor.pclk = imgsensor_info.cap2.pclk;
        imgsensor.line_length = imgsensor_info.cap2.linelength;
        imgsensor.frame_length = imgsensor_info.cap2.framelength;
        imgsensor.min_frame_length = imgsensor_info.cap2.framelength;
        imgsensor.autoflicker_en = KAL_FALSE;
    }
    else
    {
        if (imgsensor.current_fps != imgsensor_info.cap.max_framerate)
        {
            LOG_INF("Warning: current_fps %d fps is not support, so use cap1's setting: %d fps!\n",
                imgsensor.current_fps,imgsensor_info.cap1.max_framerate/10);
        }
        imgsensor.pclk = imgsensor_info.cap.pclk;
        imgsensor.line_length = imgsensor_info.cap.linelength;
        imgsensor.frame_length = imgsensor_info.cap.framelength;
        imgsensor.min_frame_length = imgsensor_info.cap.framelength;
        imgsensor.autoflicker_en = KAL_FALSE;
    }
    spin_unlock(&imgsensor_drv_lock);

    capture_setting(imgsensor.current_fps);

    return ERROR_NONE;
}

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
    // imgsensor.current_fps = 300;
    imgsensor.autoflicker_en = KAL_FALSE;
    spin_unlock(&imgsensor_drv_lock);

    normal_video_setting(imgsensor.current_fps);

    return ERROR_NONE;
}

static kal_uint32 hs_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
                              MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
    LOG_INF("E\n");

    spin_lock(&imgsensor_drv_lock);
    imgsensor.sensor_mode = IMGSENSOR_MODE_HIGH_SPEED_VIDEO;
    imgsensor.pclk = imgsensor_info.hs_video.pclk;
    // imgsensor.video_mode = KAL_TRUE;
    imgsensor.line_length = imgsensor_info.hs_video.linelength;
    imgsensor.frame_length = imgsensor_info.hs_video.framelength;
    imgsensor.min_frame_length = imgsensor_info.hs_video.framelength;
    imgsensor.dummy_line = 0;
    imgsensor.dummy_pixel = 0;
    // imgsensor.current_fps = 300;
    imgsensor.autoflicker_en = KAL_FALSE;
    spin_unlock(&imgsensor_drv_lock);
    hs_video_setting();

    return ERROR_NONE;
}

static kal_uint32 slim_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
                              MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
    LOG_INF("E\n");

    spin_lock(&imgsensor_drv_lock);
    imgsensor.sensor_mode = IMGSENSOR_MODE_SLIM_VIDEO;
    imgsensor.pclk = imgsensor_info.slim_video.pclk;
    // imgsensor.video_mode = KAL_TRUE;
    imgsensor.line_length = imgsensor_info.slim_video.linelength;
    imgsensor.frame_length = imgsensor_info.slim_video.framelength;
    imgsensor.min_frame_length = imgsensor_info.slim_video.framelength;
    imgsensor.dummy_line = 0;
    imgsensor.dummy_pixel = 0;
    // imgsensor.current_fps = 300;
    imgsensor.autoflicker_en = KAL_FALSE;
    spin_unlock(&imgsensor_drv_lock);
    slim_video_setting();

    return ERROR_NONE;
}

static kal_uint32 get_resolution(MSDK_SENSOR_RESOLUTION_INFO_STRUCT *sensor_resolution)
{
    LOG_INF("E\n");
    sensor_resolution->SensorFullWidth = imgsensor_info.cap.grabwindow_width;
    sensor_resolution->SensorFullHeight = imgsensor_info.cap.grabwindow_height;

    sensor_resolution->SensorPreviewWidth = imgsensor_info.pre.grabwindow_width;
    sensor_resolution->SensorPreviewHeight = imgsensor_info.pre.grabwindow_height;

    sensor_resolution->SensorVideoWidth = imgsensor_info.normal_video.grabwindow_width;
    sensor_resolution->SensorVideoHeight = imgsensor_info.normal_video.grabwindow_height;


    sensor_resolution->SensorHighSpeedVideoWidth     = imgsensor_info.hs_video.grabwindow_width;
    sensor_resolution->SensorHighSpeedVideoHeight    = imgsensor_info.hs_video.grabwindow_height;

    sensor_resolution->SensorSlimVideoWidth  = imgsensor_info.slim_video.grabwindow_width;
    sensor_resolution->SensorSlimVideoHeight     = imgsensor_info.slim_video.grabwindow_height;
    return ERROR_NONE;
}

static kal_uint32 get_info(enum MSDK_SCENARIO_ID_ENUM scenario_id,
                              MSDK_SENSOR_INFO_STRUCT *sensor_info,
                              MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
    LOG_INF("scenario_id = %d\n", scenario_id);

    sensor_info->SensorClockPolarity = SENSOR_CLOCK_POLARITY_LOW;
    sensor_info->SensorClockFallingPolarity = SENSOR_CLOCK_POLARITY_LOW; /* not use */
    sensor_info->SensorHsyncPolarity = SENSOR_CLOCK_POLARITY_LOW; // inverse with datasheet
    sensor_info->SensorVsyncPolarity = SENSOR_CLOCK_POLARITY_LOW;
    sensor_info->SensorInterruptDelayLines = 4; /* not use */
    sensor_info->SensorResetActiveHigh = FALSE; /* not use */
    sensor_info->SensorResetDelayCount = 5; /* not use */

    sensor_info->SensroInterfaceType = imgsensor_info.sensor_interface_type;
    // sensor_info->MIPIsensorType = imgsensor_info.mipi_sensor_type;
    // sensor_info->SettleDelayMode = imgsensor_info.mipi_settle_delay_mode;
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
    sensor_info->SensorHightSampling = 0;   // 0 is default 1x
    sensor_info->SensorPacketECCOrder = 1;

    switch (scenario_id)
    {
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
}


static kal_uint32 control(enum MSDK_SCENARIO_ID_ENUM scenario_id, MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
                              MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
    LOG_INF("control scenario_id = %d\n", scenario_id);
    spin_lock(&imgsensor_drv_lock);
    imgsensor.current_scenario_id = scenario_id;
    spin_unlock(&imgsensor_drv_lock);
    switch (scenario_id)
    {
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
}

static kal_uint32 set_video_mode(UINT16 framerate)
{
    LOG_INF("framerate = %d\n ", framerate);
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
    set_max_framerate(imgsensor.current_fps,1);

    return ERROR_NONE;
}

static kal_uint32 set_auto_flicker_mode(kal_bool enable, UINT16 framerate)
{
    LOG_INF("enable = %d, framerate = %d\n", enable, framerate);
    spin_lock(&imgsensor_drv_lock);
    if (enable)
        imgsensor.autoflicker_en = KAL_TRUE;
    else
        // Cancel Auto flick
        imgsensor.autoflicker_en = KAL_FALSE;
    spin_unlock(&imgsensor_drv_lock);
    return ERROR_NONE;
}


static kal_uint32 set_max_framerate_by_scenario(enum MSDK_SCENARIO_ID_ENUM scenario_id, MUINT32 framerate)
{
    kal_uint32 frame_length;

    LOG_INF("scenario_id = %d, framerate = %d\n", scenario_id, framerate);

    switch (scenario_id)
    {
       case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
            frame_length = imgsensor_info.pre.pclk / framerate * 10 / imgsensor_info.pre.linelength;
            spin_lock(&imgsensor_drv_lock);
            imgsensor.dummy_line = (frame_length > imgsensor_info.pre.framelength) ? (frame_length - imgsensor_info.pre.framelength) : 0;
            imgsensor.frame_length = imgsensor_info.pre.framelength + imgsensor.dummy_line;
            imgsensor.min_frame_length = imgsensor.frame_length;
            spin_unlock(&imgsensor_drv_lock);
            // set_dummy();
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
            // set_dummy();
            break;
        case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
            if (imgsensor.current_fps == imgsensor_info.cap1.max_framerate)
            {
                frame_length = imgsensor_info.cap1.pclk / framerate * 10 / imgsensor_info.cap1.linelength;
                spin_lock(&imgsensor_drv_lock);
                imgsensor.dummy_line = (frame_length > imgsensor_info.cap1.framelength) ? (frame_length - imgsensor_info.cap1.framelength) : 0;
                imgsensor.frame_length = imgsensor_info.cap1.framelength + imgsensor.dummy_line;
                imgsensor.min_frame_length = imgsensor.frame_length;
                spin_unlock(&imgsensor_drv_lock);
            }
            else if (imgsensor.current_fps == imgsensor_info.cap2.max_framerate)
            {
                frame_length = imgsensor_info.cap2.pclk / framerate * 10 / imgsensor_info.cap2.linelength;
                spin_lock(&imgsensor_drv_lock);
                imgsensor.dummy_line = (frame_length > imgsensor_info.cap2.framelength) ? (frame_length - imgsensor_info.cap2.framelength) : 0;
                imgsensor.frame_length = imgsensor_info.cap2.framelength + imgsensor.dummy_line;
                imgsensor.min_frame_length = imgsensor.frame_length;
                spin_unlock(&imgsensor_drv_lock);
            }
            else
            {
                if (imgsensor.current_fps != imgsensor_info.cap.max_framerate)
                {
                    LOG_INF("Warning: current_fps %d fps is not support, so use cap's setting: %d fps!\n",
                        framerate,imgsensor_info.cap.max_framerate/10);
                }
                frame_length = imgsensor_info.cap.pclk / framerate * 10 / imgsensor_info.cap.linelength;
                spin_lock(&imgsensor_drv_lock);
                imgsensor.dummy_line = (frame_length > imgsensor_info.cap.framelength) ? (frame_length - imgsensor_info.cap.framelength) : 0;
                imgsensor.frame_length = imgsensor_info.cap.framelength + imgsensor.dummy_line;
                imgsensor.min_frame_length = imgsensor.frame_length;
                spin_unlock(&imgsensor_drv_lock);
            }
            // set_dummy();
            break;
        case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
            frame_length = imgsensor_info.hs_video.pclk / framerate * 10 / imgsensor_info.hs_video.linelength;
            spin_lock(&imgsensor_drv_lock);
            imgsensor.dummy_line = (frame_length > imgsensor_info.hs_video.framelength) ? (frame_length - imgsensor_info.hs_video.framelength) : 0;
            imgsensor.frame_length = imgsensor_info.hs_video.framelength + imgsensor.dummy_line;
            imgsensor.min_frame_length = imgsensor.frame_length;
            spin_unlock(&imgsensor_drv_lock);
            // set_dummy();
            break;
        case MSDK_SCENARIO_ID_SLIM_VIDEO:
            frame_length = imgsensor_info.slim_video.pclk / framerate * 10 / imgsensor_info.slim_video.linelength;
            spin_lock(&imgsensor_drv_lock);
            imgsensor.dummy_line = (frame_length > imgsensor_info.slim_video.framelength) ? (frame_length - imgsensor_info.slim_video.framelength): 0;
            imgsensor.frame_length = imgsensor_info.slim_video.framelength + imgsensor.dummy_line;
            imgsensor.min_frame_length = imgsensor.frame_length;
            spin_unlock(&imgsensor_drv_lock);
            // set_dummy();
            break;
        default:  // coding with  preview scenario by default
            frame_length = imgsensor_info.pre.pclk / framerate * 10 / imgsensor_info.pre.linelength;
            spin_lock(&imgsensor_drv_lock);
            imgsensor.dummy_line = (frame_length > imgsensor_info.pre.framelength) ? (frame_length - imgsensor_info.pre.framelength) : 0;
            imgsensor.frame_length = imgsensor_info.pre.framelength + imgsensor.dummy_line;
            imgsensor.min_frame_length = imgsensor.frame_length;
            spin_unlock(&imgsensor_drv_lock);
            // set_dummy();
            LOG_INF("error scenario_id = %d, we use preview scenario \n", scenario_id);
            break;
    }
    return ERROR_NONE;
}

static kal_uint32 get_default_framerate_by_scenario(enum MSDK_SCENARIO_ID_ENUM scenario_id, MUINT32 *framerate)
{
    LOG_INF("scenario_id = %d\n", scenario_id);

    switch (scenario_id)
    {
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

    if (enable)
    {
        write_cmos_sensor(0x0641, 0x01);
        write_cmos_sensor(0x0804, 0x01);
        write_cmos_sensor(0x090b, 0x05); // check board
        write_cmos_sensor(0x0902, 0xc0);
        write_cmos_sensor(0x0903, 0x0c); // w
        write_cmos_sensor(0x0904, 0x90);
        write_cmos_sensor(0x0905, 0x09); // h
        write_cmos_sensor(0x0906, 0x80);
        write_cmos_sensor(0x0907, 0x05); // h blank
        write_cmos_sensor(0x0908, 0x78);
        write_cmos_sensor(0x0909, 0x00); // vblank
        write_cmos_sensor(0x0900, 0x01);
        write_cmos_sensor(0x0800, 0x01);
        write_cmos_sensor(0x0901, 0x01);
        write_cmos_sensor(0x0803, 0x01);
    }
    else
    {
        write_cmos_sensor(0x0641, 0x00);
        write_cmos_sensor(0x0804, 0x00);
        write_cmos_sensor(0x0901, 0x00);
        write_cmos_sensor(0x0800, 0x01);
        write_cmos_sensor(0x0803, 0x01);
    }
    spin_lock(&imgsensor_drv_lock);
    imgsensor.test_pattern = enable;
    spin_unlock(&imgsensor_drv_lock);
    return ERROR_NONE;
}
/* A06 code for SR-AL7160A-01-537 by dingling at 20240513 start */
static kal_uint32 streaming_control(kal_bool enable)
{
    pr_info("streaming_enable(0=Sw tandby,1=streaming): %d\n", enable);
    if(enable)
    {
        write_cmos_sensor(0x0600, 0X01);
        msleep(1);
        write_cmos_sensor(0x040e, 0X01);
    }
    else
    {
        write_cmos_sensor(0x040e, 0X02);
        mdelay(35);
        write_cmos_sensor(0x0600, 0X00);
    }
    return ERROR_NONE;
}
/* A06 code for SR-AL7160A-01-537 by dingling at 20240513 end */
static void set_shutter_frame_length(
                    kal_uint16 shutter, kal_uint16 frame_length,
                    kal_bool auto_extend_en)
{
    unsigned long flags;
    kal_uint16 realtime_fps = 0;
    kal_int32 dummy_line = 0;

    spin_lock_irqsave(&imgsensor_drv_lock, flags);
    imgsensor.shutter = shutter;
    spin_unlock_irqrestore(&imgsensor_drv_lock, flags);

    spin_lock(&imgsensor_drv_lock);
    /* Change frame time */
    dummy_line = frame_length - imgsensor.frame_length;
    imgsensor.frame_length = imgsensor.frame_length + dummy_line;
    imgsensor.min_frame_length = imgsensor.frame_length;

    if (shutter > imgsensor.min_frame_length - imgsensor_info.margin)
        imgsensor.frame_length = shutter + imgsensor_info.margin;
    else
        imgsensor.frame_length = imgsensor.min_frame_length;

    if (imgsensor.frame_length > imgsensor_info.max_frame_length)
        imgsensor.frame_length = imgsensor_info.max_frame_length;
    spin_unlock(&imgsensor_drv_lock);

    shutter = (shutter < imgsensor_info.min_shutter) ? imgsensor_info.min_shutter : shutter;
    shutter = (shutter > (imgsensor_info.max_frame_length - imgsensor_info.margin))
              ? (imgsensor_info.max_frame_length - imgsensor_info.margin) : shutter;

    if(imgsensor.autoflicker_en)
    {
        realtime_fps = imgsensor.pclk / imgsensor.line_length * 10 / imgsensor.frame_length;
        if(realtime_fps >= 297 && realtime_fps <= 305)
            set_max_framerate(296, 0);
        else if(realtime_fps >= 147 && realtime_fps <= 150)
            set_max_framerate(146, 0);
        else
            LOG_INF("autoflicker enable\n");
    }

     // Update frame length and shutter of normal mode
     update_shutter_frame_reg(0, 0, shutter, imgsensor.frame_length);

    LOG_INF("Exit! shutter =%d, imgsensor.framelength =%d, frame_length=%d, dummy_line=%d\n",
            shutter, imgsensor.frame_length, frame_length, dummy_line);
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

    LOG_INF("mt811  feature_control feature_id = %d,paramlength=%d\n", feature_id,*feature_para_len);
    switch (feature_id)
    {
        case SENSOR_FEATURE_GET_PERIOD:
            *feature_return_para_16++ = imgsensor.line_length;
            *feature_return_para_16 = imgsensor.frame_length;
            *feature_para_len=4;
            break;
        case SENSOR_FEATURE_GET_PIXEL_CLOCK_FREQ:
            LOG_INF("feature_Control imgsensor.pclk = %d,imgsensor.current_fps = %d\n", imgsensor.pclk,imgsensor.current_fps);
            *feature_return_para_32 = imgsensor.pclk;
            *feature_para_len=4;
            break;
        case SENSOR_FEATURE_SET_ESHUTTER:
            set_shutter(*feature_data);
            break;
        case SENSOR_FEATURE_SET_NIGHTMODE:
            night_mode((kal_bool) *feature_data);
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
            set_auto_flicker_mode((kal_bool)*feature_data_16,*(feature_data_16+1));
            break;
        case SENSOR_FEATURE_SET_MAX_FRAME_RATE_BY_SCENARIO:
            set_max_framerate_by_scenario((enum MSDK_SCENARIO_ID_ENUM)*feature_data, *(feature_data+1));
            break;
        case SENSOR_FEATURE_GET_DEFAULT_FRAME_RATE_BY_SCENARIO:
            get_default_framerate_by_scenario((enum MSDK_SCENARIO_ID_ENUM)*(feature_data), (MUINT32 *)(uintptr_t)(*(feature_data+1)));
            break;
        case SENSOR_FEATURE_SET_TEST_PATTERN:
            set_test_pattern_mode((kal_bool)*feature_data);
            break;
        case SENSOR_FEATURE_GET_TEST_PATTERN_CHECKSUM_VALUE: 
            // for factory mode auto testing
            *feature_return_para_32 = imgsensor_info.checksum_value;
            *feature_para_len=4;
            break;
        //A06 code for SR-AL7160A-01-501 by zhongbin at 20240416 start
        case SENSOR_FEATURE_SET_FRAMERATE:
            LOG_INF("current fps :%d\n", *feature_data_32);
            spin_lock(&imgsensor_drv_lock);
            imgsensor.current_fps = (UINT16)*feature_data_32;
            spin_unlock(&imgsensor_drv_lock);
            break;
        case SENSOR_FEATURE_SET_HDR:
            LOG_INF("ihdr enable :%d\n", *feature_data_32);
            spin_lock(&imgsensor_drv_lock);
            //imgsensor.ihdr_mode = (BOOL)*feature_data_32;
            spin_unlock(&imgsensor_drv_lock);
            break;
        //A06 code for SR-AL7160A-01-501 by zhongbin at 20240416 end
        case SENSOR_FEATURE_GET_CROP_INFO:
            LOG_INF("SENSOR_FEATURE_GET_CROP_INFO scenarioId:%d\n", (UINT32)*feature_data);
            wininfo = (struct SENSOR_WINSIZE_INFO_STRUCT *)(uintptr_t)(*(feature_data+1));
            switch(*feature_data_32)
            {
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
        case SENSOR_FEATURE_SET_IHDR_SHUTTER_GAIN:
            LOG_INF("SENSOR_SET_SENSOR_IHDR LE=%d, SE=%d, Gain=%d\n",(UINT16)*feature_data,(UINT16)*(feature_data+1),(UINT16)*(feature_data+2));
            // ihdr_write_shutter_gain((UINT16)*feature_data,(UINT16)*(feature_data+1),(UINT16)*(feature_data+2));
            break;
        case SENSOR_FEATURE_GET_VC_INFO:
            LOG_INF("SENSOR_FEATURE_GET_VC_INFO not implement%d\n", (UINT16)*feature_data);
            break;
        case SENSOR_FEATURE_SET_AWB_GAIN:
            break;
        case SENSOR_FEATURE_SET_HDR_SHUTTER:
            LOG_INF("SENSOR_FEATURE_SET_HDR_SHUTTER LE=%d, SE=%d\n",(UINT16)*feature_data,(UINT16)*(feature_data+1));
            // ihdr_write_shutter((UINT16)*feature_data,(UINT16)*(feature_data+1));
            break;
        case SENSOR_FEATURE_GET_TEMPERATURE_VALUE:
            break;
        case SENSOR_FEATURE_SET_SHUTTER_FRAME_TIME:
            set_shutter_frame_length((UINT16) (*feature_data),
                        (UINT16) (*(feature_data + 1)),
                        (BOOL) (*(feature_data + 2)));
            break;
        case SENSOR_FEATURE_SET_STREAMING_SUSPEND:
            pr_debug("SENSOR_FEATURE_SET_STREAMING_SUSPEND\n");
            streaming_control(KAL_FALSE);
            break;
        case SENSOR_FEATURE_SET_STREAMING_RESUME:
            pr_info("SENSOR_FEATURE_SET_STREAMING_RESUME, shutter:%llu\n",
                    *feature_data);
            if (*feature_data != 0)
                set_shutter(*feature_data);
            streaming_control(KAL_TRUE);
            break;
        case SENSOR_FEATURE_GET_BINNING_TYPE:
            switch (*(feature_data + 1)) {
            case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
            case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
                *feature_return_para_32 = 1; /*BINNING_NONE*/
                break;
            case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
            case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
            case MSDK_SCENARIO_ID_SLIM_VIDEO:
            default:
                *feature_return_para_32 = 2; /*BINNING_AVERAGED*/
                break;
            }
            pr_debug("SENSOR_FEATURE_GET_BINNING_TYPE AE_binning_type:%d,\n",
                *feature_return_para_32);
            *feature_para_len = 4;
            break;
#ifdef MT811_LONG_EXP
        case SENSOR_FEATURE_GET_AE_EFFECTIVE_FRAME_FOR_LE:
            *feature_return_para_32 = imgsensor.current_ae_effective_frame;
            break;
        case SENSOR_FEATURE_GET_AE_FRAME_MODE_FOR_LE:
            memcpy(feature_return_para_32,
            &imgsensor.ae_frm_mode, sizeof(struct IMGSENSOR_AE_FRM_MODE));
            break;
#endif
        case SENSOR_FEATURE_GET_MIPI_PIXEL_RATE:
        {
            switch(*feature_data)
            {
                case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
                    *(MUINT32 *)(uintptr_t)(*(feature_data + 1))
                        = imgsensor_info.cap.mipi_pixel_rate;
                    break;
                case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
                    *(MUINT32 *)(uintptr_t)(*(feature_data + 1))
                        = imgsensor_info.normal_video.mipi_pixel_rate;
                    break;
                case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
                    *(MUINT32 *)(uintptr_t)(*(feature_data + 1))
                        = imgsensor_info.hs_video.mipi_pixel_rate;
                    break;
                case MSDK_SCENARIO_ID_SLIM_VIDEO:
                    *(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
                        imgsensor_info.slim_video.mipi_pixel_rate;
                    break;
                case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
                default:
                    *(MUINT32 *)(uintptr_t)(*(feature_data + 1))
                        = imgsensor_info.pre.mipi_pixel_rate;
                    break;
            }
            break;
        }
        case SENSOR_FEATURE_GET_PIXEL_CLOCK_FREQ_BY_SCENARIO:
            switch(*feature_data)
            {
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
        case SENSOR_FEATURE_GET_PERIOD_BY_SCENARIO:
            switch (*feature_data)
            {
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
       case SENSOR_FEATURE_GET_OFFSET_TO_START_OF_EXPOSURE:
       LOG_INF("hqdebug mt811  feature_control feature_id = %d,paramlength=%d,feature_data=%p,feature_data+1=%p\n", 
        feature_id,*feature_para_len,feature_data,feature_data+1);
        //    *(MUINT32 *)(uintptr_t)(*(feature_data + 1)) = 3000000;
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
        default:
            break;
    }
/* A06 code for SR-AL7160A-01-539 by xuyunhui at 20240626 end */
    return ERROR_NONE;
}/*      feature_control()  */

static struct  SENSOR_FUNCTION_STRUCT sensor_func = {
    open,
    get_info,
    get_resolution,
    feature_control,
    control,
    close
};

UINT32 A0604XLFRONTMT811_MIPI_RAW_SensorInit(struct SENSOR_FUNCTION_STRUCT  **pfFunc)
{
    LOG_INF("A0604XLFRONTMT811_MIPI_RAW_SensorInit\n");
    if (pfFunc!=NULL)
            *pfFunc=&sensor_func;
    return ERROR_NONE;
}
