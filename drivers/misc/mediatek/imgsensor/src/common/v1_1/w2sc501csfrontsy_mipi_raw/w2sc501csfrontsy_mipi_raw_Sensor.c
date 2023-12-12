/*
 * Copyright (C) 2018 MediaTek Inc.
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

#include "w2sc501csfrontsy_mipi_raw_Sensor.h"

#define PFX "sc501cs_camera_sensor"
#define LOG_INF(format, args...)    \
    pr_info(PFX "[%s] " format, __func__, ##args)

#define MULTI_WRITE 0
static DEFINE_SPINLOCK(imgsensor_drv_lock);
// +bug727089, liangyiyi.wt, MODIFY, 2022/3/29, update setting for 1st front camera module consistency by fae
static struct imgsensor_info_struct imgsensor_info = {
    .sensor_id = W2SC501CSFRONTSY_SENSOR_ID,
    .checksum_value = 0x55e2a82f,
    .pre = {
        .pclk = 90000000,
        .linelength = 1500,
        .framelength = 2000,
        .startx = 0,
        .starty = 0,
        .grabwindow_width = 1296,
        .grabwindow_height = 972,
        .mipi_data_lp2hs_settle_dc = 14,
        .max_framerate = 300,
    },
    .cap = {
        .pclk = 90000000,
        .linelength = 1500,
        .framelength = 2000,
        .startx = 0,
        .starty = 0,
        .grabwindow_width = 2592,
        .grabwindow_height = 1944,
        .mipi_data_lp2hs_settle_dc = 14,
        .max_framerate = 300,
    },
    .cap1 = {
        .pclk = 90000000,
        .linelength = 1500,
        .framelength = 2000,
        .startx = 0,
        .starty = 0,
        .grabwindow_width = 2592,
        .grabwindow_height = 1944,
        .mipi_data_lp2hs_settle_dc = 14,
        .max_framerate = 300,
    },
    .normal_video = {
        .pclk = 90000000,
        .linelength = 1500,
        .framelength = 2000,
        .startx = 0,
        .starty = 0,
        .grabwindow_width = 2592,
        .grabwindow_height = 1944,
        .mipi_data_lp2hs_settle_dc = 14,
        .max_framerate = 300,
    },
    .hs_video = {
        .pclk = 90000000,
        .linelength = 1500,
        .framelength = 2000,
        .startx = 0,
        .starty = 0,
        .grabwindow_width = 2592,
        .grabwindow_height = 1944,
        .mipi_data_lp2hs_settle_dc = 14,
        .max_framerate = 600,
    },
    .slim_video = {
        .pclk = 90000000,
        .linelength = 1500,
        .framelength = 2000,
        .startx = 0,
        .starty = 0,
        .grabwindow_width = 2592,
        .grabwindow_height = 1944,
        .mipi_data_lp2hs_settle_dc = 14,
        .max_framerate = 300,
    },

     .margin = 5,            //sensor framelength & shutter margin
     .min_shutter = 2,
     .max_frame_length = 0x7FFF,
     .min_gain = 64,
     .max_gain = 2048,
     .min_gain_iso = 100,
     .gain_step = 1,
     .gain_type = 4,
     .ae_shut_delay_frame = 0,
     .ae_sensor_gain_delay_frame = 0,
     .ae_ispGain_delay_frame = 2,
     .ihdr_support = 0,     // //1, support; 0,not support
     .ihdr_le_firstline = 0, // //1,le first ; 0, se first
     .sensor_mode_num = 5,      //support sensor mode num

     .cap_delay_frame = 3,
     .pre_delay_frame = 3,
     .video_delay_frame = 3,
     .hs_video_delay_frame = 3,
     .slim_video_delay_frame = 3,
     .isp_driving_current = ISP_DRIVING_4MA,
     .sensor_interface_type = SENSOR_INTERFACE_TYPE_MIPI,
     .mipi_sensor_type = MIPI_OPHY_NCSI2,
     .mipi_settle_delay_mode =MIPI_SETTLEDELAY_AUTO,//0,MIPI_SETTLEDELAY_AUTO; 1,MIPI_SETTLEDELAY_MANNUAL
     .sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_B,
     .mclk = 24,
     .mipi_lane_num = SENSOR_MIPI_2_LANE,
     .i2c_addr_table = {0x20, 0xff},
     .i2c_speed = 400,
};

static struct imgsensor_struct imgsensor = {
    .mirror = IMAGE_NORMAL,
    .sensor_mode = IMGSENSOR_MODE_INIT,
    .shutter = 0x3d0,                   //current shutter   // Danbo ??
    .gain = 0x40,                      //current gain     // Danbo ??
    .dummy_pixel = 0,
    .dummy_line = 0,
//full size current fps : 24fps for PIP, 30fps for Normal or ZSD
    .current_fps = 300,
    .autoflicker_en = KAL_FALSE,
    .test_pattern = KAL_FALSE,
    .current_scenario_id = MSDK_SCENARIO_ID_CAMERA_PREVIEW,
    .ihdr_en = 0,
    .i2c_write_id = 0x20,
};

/* Sensor output window information */
static struct SENSOR_WINSIZE_INFO_STRUCT imgsensor_winsize_info[5] = {
 { 2592, 1944,      0,    0, 2592, 1944, 1296, 972, 0000, 0000, 1296, 972,    0,	0, 1296, 972}, // Preview
 { 2592, 1944,      0,    0, 2592, 1944, 2592, 1944, 0000, 0000, 2592, 1944,    0,    0, 2592, 1944}, // capture
 { 2592, 1944,      0,    0, 2592, 1944, 2592, 1944, 0000, 0000, 2592, 1944,    0,    0, 2592, 1944}, // video
 { 2592, 1944,      0,    0, 2592, 1944, 2592, 1944, 0000, 0000, 2592, 1944,    0,    0, 2592, 1944}, //hight speed video
 { 2592, 1944,      0,    0, 2592, 1944, 2592, 1944, 0000, 0000, 2592, 1944,    0,    0, 2592, 1944},};// slim video

#if MULTI_WRITE
#define I2C_BUFFER_LEN 765

static kal_uint16 w2sc501csfrontsy_table_write_cmos_sensor(
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
            //puSendCmd[tosend++] = (char)(data >> 8);
            puSendCmd[tosend++] = (char)(data & 0xFF);
            IDX += 2;
            addr_last = addr;
        }

        if ((I2C_BUFFER_LEN - tosend) < 4 ||
            len == IDX ||
            addr != addr_last) {
            iBurstWriteReg_multi(puSendCmd, tosend,
                imgsensor.i2c_write_id,
                3, imgsensor_info.i2c_speed);

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

static void write_cmos_sensor8(kal_uint32 addr, kal_uint32 para)
{
    char pu_send_cmd[3] = {(char)(addr >> 8),
        (char)(addr & 0xFF), (char)(para & 0xFF)};

    iWriteRegI2C(pu_send_cmd, 3, imgsensor.i2c_write_id);
}

static void set_dummy(void)
{
    LOG_INF("dummyline = %d, dummypixels = %d",
        imgsensor.dummy_line, imgsensor.dummy_pixel);
    write_cmos_sensor8(0x320e, imgsensor.frame_length >> 8);
    write_cmos_sensor8(0x320f, imgsensor.frame_length & 0xFF);
    write_cmos_sensor8(0x320c, imgsensor.line_length >> 8);
    write_cmos_sensor8(0x320d, imgsensor.line_length & 0xFF);


}    /*    set_dummy  */

static kal_uint32 return_sensor_id(void)
{
    return ((read_cmos_sensor(0x3107) << 8) | read_cmos_sensor(0x3108)); //0xeb15
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
}    /*    set_max_framerate  */

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
        else{
        write_cmos_sensor8(0x320e, (imgsensor.frame_length >> 8) & 0xff);
        write_cmos_sensor8(0x320f, imgsensor.frame_length & 0xFF);
        }

    } else{
        // Extend frame length

        // ADD ODIN
        realtime_fps = imgsensor.pclk * 10 /
            (imgsensor.line_length * imgsensor.frame_length);
        if (realtime_fps > 300 && realtime_fps < 320)
            set_max_framerate(300, 0);
        // ADD END
        write_cmos_sensor8(0x320e, (imgsensor.frame_length >> 8) & 0xff);
        write_cmos_sensor8(0x320f, imgsensor.frame_length & 0xFF);
    }

    // Update Shutter
    shutter = shutter *2;
    write_cmos_sensor8(0x3e00, (shutter >> 12) & 0xFF);
    write_cmos_sensor8(0x3e01, (shutter >> 4)&0xFF);
    write_cmos_sensor8(0x3e02, (shutter<<4) & 0xF0);

    LOG_INF("shutter =%d, framelength =%d",
        shutter, imgsensor.frame_length);
}    /*    write_shutter  */

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
static void set_shutter(kal_uint32 shutter)
{
    unsigned long flags;

    LOG_INF("set_shutter");
    spin_lock_irqsave(&imgsensor_drv_lock, flags);
    imgsensor.shutter = shutter;
    spin_unlock_irqrestore(&imgsensor_drv_lock, flags);

    write_shutter(shutter);
}    /*    set_shutter */


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
#if 0
#define SC501CS_SENSOR_GAIN_BASE             1024
#define SC501CS_SENSOR_GAIN_MAX              65536 //(64 * SC510CS_SENSOR_GAIN_BASE)
#define SC501CS_SENSOR_GAIN_MAX_VALID_INDEX  6
static kal_uint16 gain2reg(const kal_uint16 gain)
{
    kal_uint16 reg_gain = gain << 4;

    if (reg_gain < SC501CS_SENSOR_GAIN_BASE)
        reg_gain = SC501CS_SENSOR_GAIN_BASE;
    else if (reg_gain > SC501CS_SENSOR_GAIN_MAX)
        reg_gain = SC501CS_SENSOR_GAIN_MAX;

    return (kal_uint16)reg_gain;

}

static kal_uint16 set_gain(kal_uint16 gain)
{
    kal_uint16 reg_gain;
    kal_uint32 temp_gain;
    kal_int16 gain_index;
    kal_uint16 SC501CS_AGC_Param[SC501CS_SENSOR_GAIN_MAX_VALID_INDEX][2] = {
        {  1024,  0x00 },
        {  2048,  0x01 },
        {  4096,  0x03 },
        {  8192,  0x07 },
        { 16384,  0x0f },
        { 32768,  0x1f },
    };
    LOG_INF("Gain_Debug pass_gain= %d\n",gain);
    reg_gain = gain2reg(gain);

    for (gain_index = SC501CS_SENSOR_GAIN_MAX_VALID_INDEX - 1; gain_index >= 0; gain_index--)
        if (reg_gain >= SC501CS_AGC_Param[gain_index][0])
            break;

    write_cmos_sensor(0x3e09, SC501CS_AGC_Param[gain_index][1]);
    temp_gain = reg_gain * SC501CS_SENSOR_GAIN_BASE / SC501CS_AGC_Param[gain_index][0];
    write_cmos_sensor(0x3e07, (temp_gain >> 3) & 0xff);
    LOG_INF("Gain_Debug again = 0x%x, dgain = 0x%x\n",read_cmos_sensor(0x3e09), read_cmos_sensor(0x3e07));

    return reg_gain;
}
#else
static kal_uint16 set_gain(kal_uint16 gain)
{
    kal_uint16 reg_gain;
    kal_uint16 ana_real_gain;

    if (gain < BASEGAIN)
        gain = BASEGAIN;
    else if (gain > 63 * BASEGAIN)
        gain = 63 * BASEGAIN;

    if((gain>=1*BASEGAIN)&&(gain <2*BASEGAIN))
    {
        reg_gain = 0x00;
        ana_real_gain = 1;
    }
    else if((gain>=2*BASEGAIN)&&(gain <4*BASEGAIN))
    {
        reg_gain = 0x08;
        ana_real_gain = 2;
    }
    else if((gain >= 4*BASEGAIN)&&(gain <8*BASEGAIN))
    {
        reg_gain = 0x09;
        ana_real_gain = 4;
    }
    else if((gain >= 8*BASEGAIN)&&(gain <16*BASEGAIN))
    {
        reg_gain = 0x0b;
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

    write_cmos_sensor8(0x3e09,reg_gain);
    write_cmos_sensor8(0x3e06,0x00);
    write_cmos_sensor8(0x3e07,(gain/ana_real_gain)*128/BASEGAIN);

    spin_lock(&imgsensor_drv_lock);
    imgsensor.gain = reg_gain;
    spin_unlock(&imgsensor_drv_lock);

    LOG_INF("gain = %d ,again = 0x%x, dgain(0x3e07)= 0x%x\n ", gain, read_cmos_sensor(0x3e09),read_cmos_sensor(0x3e07));
    //LOG_INF("gain = %d ,again = 0x%x\n ", gain, reg_gain);

    return gain;
}    /*    set_gain  */
#endif

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
        write_cmos_sensor(0x3501, (le >> 12) & 0x0F);
        write_cmos_sensor(0x3508, (se << 4) & 0xFF);
        write_cmos_sensor(0x3507, (se >> 4) & 0xFF);
        write_cmos_sensor(0x3506, (se >> 12) & 0x0F);
        set_gain(gain);
    }
}
#endif

#if 0
static void set_mirror_flip(kal_uint8 image_mirror)
{
    LOG_INF("image_mirror = %d", image_mirror);

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
        LOG_INF("Error image_mirror setting");
        break;
    }
}
#endif
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

#if MULTI_WRITE
kal_uint16 addr_data_pair_init_w2sc501csfrontsy[] = {
     0x0103,0x01
};
#endif

static void sensor_init(void)
{
#if MULTI_WRITE
     w2sc501csfrontsy_table_write_cmos_sensor(
         addr_data_pair_init_w2sc501csfrontsy,
         sizeof(addr_data_pair_init_w2sc501csfrontsy) /
         sizeof(kal_uint16));
#else
     write_cmos_sensor8(0x0103,0x01); // stream off
#endif
}
// +bug727089, liangyiyi.wt, MODIFY, 2022/3/2, modify 1st front camera read otp by fae
#if MULTI_WRITE
kal_uint16 addr_data_pair_preview_w2sc501csfrontsy[] = {
    0x0103,0x01,
    0x0100,0x00,
    0x36e9,0x80,
    0x37f9,0x80,
    0x36e9,0x23,
    0x37f9,0x23,
    0x301f,0x02,
    0x3208,0x05,
    0x3209,0x10,
    0x320a,0x03,
    0x320b,0xcc,
    0x3211,0x02,
    0x3213,0x02,
    0x3215,0x31,
    0x3220,0x81,
    0x3253,0x10,
    0x3301,0x12,
    0x3306,0x38,
    0x330b,0xa8,
    0x3333,0x10,
    0x3364,0x56,
    0x3390,0x0b,
    0x3391,0x0f,
    0x3392,0x1f,
    0x3393,0x20,
    0x3394,0x40,
    0x3395,0x58,
    0x33b3,0x40,
    0x349f,0x1e,
    0x34a6,0x09,
    0x34a7,0x0f,
    0x34a8,0x38,
    0x34a9,0x28,
    0x34f8,0x1f,
    0x34f9,0x28,
    0x3630,0xa0,
    0x3633,0x43,
    0x3637,0x45,
    0x363c,0xc1,
    0x3670,0x4a,
    0x3674,0xc0,
    0x3675,0xa8,
    0x3676,0xac,
    0x367c,0x08,
    0x367d,0x0b,
    0x3690,0x53,
    0x3691,0x53,
    0x3692,0x63,
    0x3698,0x85,
    0x3699,0x8c,
    0x369a,0x9b,
    0x369b,0xb8,
    0x369c,0x0f,
    0x369d,0x1f,
    0x36a2,0x09,
    0x36a3,0x0b,
    0x36a4,0x0f,
    0x36b0,0x4c,
    0x36b1,0xd8,
    0x36b2,0x01,
    0x3722,0x03,
    0x3724,0xa1,
    0x3903,0xa0,
    0x3905,0x4c,
    0x391d,0x04,
    0x3926,0x21,
    0x393f,0x80,
    0x3940,0x80,
    0x3941,0x00,
    0x3942,0x7f,
    0x3943,0x7f,
    0x3e00,0x00,
    0x3e01,0xf9,
    0x3e02,0x60,
    0x4402,0x02,
    0x4403,0x0a,
    0x4404,0x1c,
    0x4405,0x24,
    0x440c,0x2e,
    0x440d,0x2e,
    0x440e,0x22,
    0x440f,0x39,
    0x4424,0x01,
    0x4509,0x30,
    0x450d,0x18,
    0x5000,0x4e,
    0x5787,0x08,
    0x5788,0x07,
    0x5789,0x02,
    0x5901,0x04,
    0x0100,0x01
};
#endif

static void preview_setting(void)
{
#if MULTI_WRITE
     w2sc501csfrontsy_table_write_cmos_sensor(
         addr_data_pair_preview_w2sc501csfrontsy,
         sizeof(addr_data_pair_preview_w2sc501csfrontsy) /
         sizeof(kal_uint16));
#else
     write_cmos_sensor8(0x0103,0x01);
     mdelay(10);
     write_cmos_sensor8(0x0100,0x00);
     write_cmos_sensor8(0x36e9,0x80);
     write_cmos_sensor8(0x37f9,0x80);
     write_cmos_sensor8(0x36e9,0x23);
     write_cmos_sensor8(0x37f9,0x23);
     write_cmos_sensor8(0x301f,0x02);
     write_cmos_sensor8(0x3208,0x05);
     write_cmos_sensor8(0x3209,0x10);
     write_cmos_sensor8(0x320a,0x03);
     write_cmos_sensor8(0x320b,0xcc);
     write_cmos_sensor8(0x3211,0x02);
     write_cmos_sensor8(0x3213,0x02);
     write_cmos_sensor8(0x3215,0x31);
     write_cmos_sensor8(0x3220,0x81);
     write_cmos_sensor8(0x3253,0x10);
     write_cmos_sensor8(0x3301,0x12);
     write_cmos_sensor8(0x3306,0x38);
     write_cmos_sensor8(0x330b,0xa8);
     write_cmos_sensor8(0x3333,0x10);
     write_cmos_sensor8(0x3364,0x56);
     write_cmos_sensor8(0x3390,0x0b);
     write_cmos_sensor8(0x3391,0x0f);
     write_cmos_sensor8(0x3392,0x1f);
     write_cmos_sensor8(0x3393,0x20);
     write_cmos_sensor8(0x3394,0x40);
     write_cmos_sensor8(0x3395,0x58);
     write_cmos_sensor8(0x33b3,0x40);
     write_cmos_sensor8(0x349f,0x1e);
     write_cmos_sensor8(0x34a6,0x09);
     write_cmos_sensor8(0x34a7,0x0f);
     write_cmos_sensor8(0x34a8,0x38);
     write_cmos_sensor8(0x34a9,0x28);
     write_cmos_sensor8(0x34f8,0x1f);
     write_cmos_sensor8(0x34f9,0x28);
     write_cmos_sensor8(0x3630,0xa0);
     write_cmos_sensor8(0x3633,0x43);
     write_cmos_sensor8(0x3637,0x45);
     write_cmos_sensor8(0x363c,0xc1);
     write_cmos_sensor8(0x3670,0x4a);
     write_cmos_sensor8(0x3674,0xc0);
     write_cmos_sensor8(0x3675,0xa8);
     write_cmos_sensor8(0x3676,0xac);
     write_cmos_sensor8(0x367c,0x08);
     write_cmos_sensor8(0x367d,0x0b);
     write_cmos_sensor8(0x3690,0x53);
     write_cmos_sensor8(0x3691,0x53);
     write_cmos_sensor8(0x3692,0x63);
     write_cmos_sensor8(0x3698,0x85);
     write_cmos_sensor8(0x3699,0x8c);
     write_cmos_sensor8(0x369a,0x9b);
     write_cmos_sensor8(0x369b,0xb8);
     write_cmos_sensor8(0x369c,0x0f);
     write_cmos_sensor8(0x369d,0x1f);
     write_cmos_sensor8(0x36a2,0x09);
     write_cmos_sensor8(0x36a3,0x0b);
     write_cmos_sensor8(0x36a4,0x0f);
     write_cmos_sensor8(0x36b0,0x4c);
     write_cmos_sensor8(0x36b1,0xd8);
     write_cmos_sensor8(0x36b2,0x01);
     write_cmos_sensor8(0x3722,0x03);
     write_cmos_sensor8(0x3724,0xa1);
     write_cmos_sensor8(0x3903,0xa0);
     write_cmos_sensor8(0x3905,0x4c);
     write_cmos_sensor8(0x391d,0x04);
     write_cmos_sensor8(0x3926,0x21);
     write_cmos_sensor8(0x393f,0x80);
     write_cmos_sensor8(0x3940,0x80);
     write_cmos_sensor8(0x3941,0x00);
     write_cmos_sensor8(0x3942,0x7f);
     write_cmos_sensor8(0x3943,0x7f);
     write_cmos_sensor8(0x3e00,0x00);
     write_cmos_sensor8(0x3e01,0xf9);
     write_cmos_sensor8(0x3e02,0x60);
     write_cmos_sensor8(0x4402,0x02);
     write_cmos_sensor8(0x4403,0x0a);
     write_cmos_sensor8(0x4404,0x1c);
     write_cmos_sensor8(0x4405,0x24);
     write_cmos_sensor8(0x440c,0x2e);
     write_cmos_sensor8(0x440d,0x2e);
     write_cmos_sensor8(0x440e,0x22);
     write_cmos_sensor8(0x440f,0x39);
     write_cmos_sensor8(0x4424,0x01);
     write_cmos_sensor8(0x4509,0x30);
     write_cmos_sensor8(0x450d,0x18);
     write_cmos_sensor8(0x5000,0x4e);
     write_cmos_sensor8(0x5787,0x08);
     write_cmos_sensor8(0x5788,0x07);
     write_cmos_sensor8(0x5789,0x02);
     write_cmos_sensor8(0x5901,0x04);
     write_cmos_sensor8(0x0100,0x01);
#endif
}

#if MULTI_WRITE
kal_uint16 addr_data_pair_capture_fps_w2sc501csfrontsy[] = {
    0x0103,0x01,
    0x0100,0x00,
    0x36e9,0x80,
    0x37f9,0x80,
    0x36e9,0x23,
    0x37f9,0x23,
    0x301f,0x01,
    0x3253,0x10,
    0x3301,0x12,
    0x3306,0x38,
    0x330b,0xa8,
    0x3333,0x10,
    0x3364,0x56,
    0x3390,0x0b,
    0x3391,0x0f,
    0x3392,0x1f,
    0x3393,0x20,
    0x3394,0x40,
    0x3395,0x58,
    0x33b3,0x40,
    0x349f,0x1e,
    0x34a6,0x09,
    0x34a7,0x0f,
    0x34a8,0x38,
    0x34a9,0x28,
    0x34f8,0x1f,
    0x34f9,0x28,
    0x3630,0xa0,
    0x3633,0x43,
    0x3637,0x45,
    0x363c,0xc1,
    0x3670,0x4a,
    0x3674,0xc0,
    0x3675,0xa8,
    0x3676,0xac,
    0x367c,0x08,
    0x367d,0x0b,
    0x3690,0x53,
    0x3691,0x53,
    0x3692,0x63,
    0x3698,0x85,
    0x3699,0x8c,
    0x369a,0x9b,
    0x369b,0xb8,
    0x369c,0x0f,
    0x369d,0x1f,
    0x36a2,0x09,
    0x36a3,0x0b,
    0x36a4,0x0f,
    0x36b0,0x4c,
    0x36b1,0xd8,
    0x36b2,0x01,
    0x3722,0x03,
    0x3724,0xa1,
    0x3903,0xa0,
    0x3905,0x4c,
    0x391d,0x04,
    0x3926,0x21,
    0x393f,0x80,
    0x3940,0x80,
    0x3941,0x00,
    0x3942,0x7f,
    0x3943,0x7f,
    0x3e00,0x00,
    0x3e01,0xf9,
    0x3e02,0x60,
    0x4402,0x02,
    0x4403,0x0a,
    0x4404,0x1c,
    0x4405,0x24,
    0x440c,0x2e,
    0x440d,0x2e,
    0x440e,0x22,
    0x440f,0x39,
    0x4424,0x01,
    0x4509,0x30,
    0x450d,0x18,
    0x5000,0x0e,
    0x5787,0x08,
    0x5788,0x07,
    0x5789,0x02,
    0x0100,0x01
};

kal_uint16 addr_data_pair_capture_30fps_w2sc501csfrontsy[] = {
    0x0103,0x01,
    0x0100,0x00,
    0x36e9,0x80,
    0x37f9,0x80,
    0x36e9,0x23,
    0x37f9,0x23,
    0x301f,0x01,
    0x3253,0x10,
    0x3301,0x12,
    0x3306,0x38,
    0x330b,0xa8,
    0x3333,0x10,
    0x3364,0x56,
    0x3390,0x0b,
    0x3391,0x0f,
    0x3392,0x1f,
    0x3393,0x20,
    0x3394,0x40,
    0x3395,0x58,
    0x33b3,0x40,
    0x349f,0x1e,
    0x34a6,0x09,
    0x34a7,0x0f,
    0x34a8,0x38,
    0x34a9,0x28,
    0x34f8,0x1f,
    0x34f9,0x28,
    0x3630,0xa0,
    0x3633,0x43,
    0x3637,0x45,
    0x363c,0xc1,
    0x3670,0x4a,
    0x3674,0xc0,
    0x3675,0xa8,
    0x3676,0xac,
    0x367c,0x08,
    0x367d,0x0b,
    0x3690,0x53,
    0x3691,0x53,
    0x3692,0x63,
    0x3698,0x85,
    0x3699,0x8c,
    0x369a,0x9b,
    0x369b,0xb8,
    0x369c,0x0f,
    0x369d,0x1f,
    0x36a2,0x09,
    0x36a3,0x0b,
    0x36a4,0x0f,
    0x36b0,0x4c,
    0x36b1,0xd8,
    0x36b2,0x01,
    0x3722,0x03,
    0x3724,0xa1,
    0x3903,0xa0,
    0x3905,0x4c,
    0x391d,0x04,
    0x3926,0x21,
    0x393f,0x80,
    0x3940,0x80,
    0x3941,0x00,
    0x3942,0x7f,
    0x3943,0x7f,
    0x3e00,0x00,
    0x3e01,0xf9,
    0x3e02,0x60,
    0x4402,0x02,
    0x4403,0x0a,
    0x4404,0x1c,
    0x4405,0x24,
    0x440c,0x2e,
    0x440d,0x2e,
    0x440e,0x22,
    0x440f,0x39,
    0x4424,0x01,
    0x4509,0x30,
    0x450d,0x18,
    0x5000,0x0e,
    0x5787,0x08,
    0x5788,0x07,
    0x5789,0x02,
    0x0100,0x01
};
#endif

static void capture_setting(kal_uint16 currefps)
{
#if MULTI_WRITE
     if (currefps == 300) {
     w2sc501csfrontsy_table_write_cmos_sensor(
         addr_data_pair_capture_30fps_w2sc501csfrontsy,
         sizeof(addr_data_pair_capture_30fps_w2sc501csfrontsy) /
         sizeof(kal_uint16));
     } else {
     w2sc501csfrontsy_table_write_cmos_sensor(
         addr_data_pair_capture_fps_w2sc501csfrontsy,
         sizeof(addr_data_pair_capture_fps_w2sc501csfrontsy) /
         sizeof(kal_uint16));
     }
#else
     if (currefps == 300) {
     LOG_INF("capture_setting fps = 300");
     write_cmos_sensor8(0x0103,0x01);
     mdelay(10);
     write_cmos_sensor8(0x0100,0x00);
     write_cmos_sensor8(0x36e9,0x80);
     write_cmos_sensor8(0x37f9,0x80);
     write_cmos_sensor8(0x36e9,0x23);
     write_cmos_sensor8(0x37f9,0x23);
     write_cmos_sensor8(0x301f,0x01);
     write_cmos_sensor8(0x3253,0x10);
     write_cmos_sensor8(0x3301,0x12);
     write_cmos_sensor8(0x3306,0x38);
     write_cmos_sensor8(0x330b,0xa8);
     write_cmos_sensor8(0x3333,0x10);
     write_cmos_sensor8(0x3364,0x56);
     write_cmos_sensor8(0x3390,0x0b);
     write_cmos_sensor8(0x3391,0x0f);
     write_cmos_sensor8(0x3392,0x1f);
     write_cmos_sensor8(0x3393,0x20);
     write_cmos_sensor8(0x3394,0x40);
     write_cmos_sensor8(0x3395,0x58);
     write_cmos_sensor8(0x33b3,0x40);
     write_cmos_sensor8(0x349f,0x1e);
     write_cmos_sensor8(0x34a6,0x09);
     write_cmos_sensor8(0x34a7,0x0f);
     write_cmos_sensor8(0x34a8,0x38);
     write_cmos_sensor8(0x34a9,0x28);
     write_cmos_sensor8(0x34f8,0x1f);
     write_cmos_sensor8(0x34f9,0x28);
     write_cmos_sensor8(0x3630,0xa0);
     write_cmos_sensor8(0x3633,0x43);
     write_cmos_sensor8(0x3637,0x45);
     write_cmos_sensor8(0x363c,0xc1);
     write_cmos_sensor8(0x3670,0x4a);
     write_cmos_sensor8(0x3674,0xc0);
     write_cmos_sensor8(0x3675,0xa8);
     write_cmos_sensor8(0x3676,0xac);
     write_cmos_sensor8(0x367c,0x08);
     write_cmos_sensor8(0x367d,0x0b);
     write_cmos_sensor8(0x3690,0x53);
     write_cmos_sensor8(0x3691,0x53);
     write_cmos_sensor8(0x3692,0x63);
     write_cmos_sensor8(0x3698,0x85);
     write_cmos_sensor8(0x3699,0x8c);
     write_cmos_sensor8(0x369a,0x9b);
     write_cmos_sensor8(0x369b,0xb8);
     write_cmos_sensor8(0x369c,0x0f);
     write_cmos_sensor8(0x369d,0x1f);
     write_cmos_sensor8(0x36a2,0x09);
     write_cmos_sensor8(0x36a3,0x0b);
     write_cmos_sensor8(0x36a4,0x0f);
     write_cmos_sensor8(0x36b0,0x4c);
     write_cmos_sensor8(0x36b1,0xd8);
     write_cmos_sensor8(0x36b2,0x01);
     write_cmos_sensor8(0x3722,0x03);
     write_cmos_sensor8(0x3724,0xa1);
     write_cmos_sensor8(0x3903,0xa0);
     write_cmos_sensor8(0x3905,0x4c);
     write_cmos_sensor8(0x391d,0x04);
     write_cmos_sensor8(0x3926,0x21);
     write_cmos_sensor8(0x393f,0x80);
     write_cmos_sensor8(0x3940,0x80);
     write_cmos_sensor8(0x3941,0x00);
     write_cmos_sensor8(0x3942,0x7f);
     write_cmos_sensor8(0x3943,0x7f);
     write_cmos_sensor8(0x3e00,0x00);
     write_cmos_sensor8(0x3e01,0xf9);
     write_cmos_sensor8(0x3e02,0x60);
     write_cmos_sensor8(0x4402,0x02);
     write_cmos_sensor8(0x4403,0x0a);
     write_cmos_sensor8(0x4404,0x1c);
     write_cmos_sensor8(0x4405,0x24);
     write_cmos_sensor8(0x440c,0x2e);
     write_cmos_sensor8(0x440d,0x2e);
     write_cmos_sensor8(0x440e,0x22);
     write_cmos_sensor8(0x440f,0x39);
     write_cmos_sensor8(0x4424,0x01);
     write_cmos_sensor8(0x4509,0x30);
     write_cmos_sensor8(0x450d,0x18);
     write_cmos_sensor8(0x5000,0x0e);
     write_cmos_sensor8(0x5787,0x08);
     write_cmos_sensor8(0x5788,0x07);
     write_cmos_sensor8(0x5789,0x02);
     write_cmos_sensor8(0x0100,0x01);
     } else {
     LOG_INF("capture_setting fps not 300");
     write_cmos_sensor8(0x0103,0x01);
     mdelay(10);
     write_cmos_sensor8(0x0100,0x00);
     write_cmos_sensor8(0x36e9,0x80);
     write_cmos_sensor8(0x37f9,0x80);
     write_cmos_sensor8(0x36e9,0x23);
     write_cmos_sensor8(0x37f9,0x23);
     write_cmos_sensor8(0x301f,0x01);
     write_cmos_sensor8(0x3253,0x10);
     write_cmos_sensor8(0x3301,0x12);
     write_cmos_sensor8(0x3306,0x38);
     write_cmos_sensor8(0x330b,0xa8);
     write_cmos_sensor8(0x3333,0x10);
     write_cmos_sensor8(0x3364,0x56);
     write_cmos_sensor8(0x3390,0x0b);
     write_cmos_sensor8(0x3391,0x0f);
     write_cmos_sensor8(0x3392,0x1f);
     write_cmos_sensor8(0x3393,0x20);
     write_cmos_sensor8(0x3394,0x40);
     write_cmos_sensor8(0x3395,0x58);
     write_cmos_sensor8(0x33b3,0x40);
     write_cmos_sensor8(0x349f,0x1e);
     write_cmos_sensor8(0x34a6,0x09);
     write_cmos_sensor8(0x34a7,0x0f);
     write_cmos_sensor8(0x34a8,0x38);
     write_cmos_sensor8(0x34a9,0x28);
     write_cmos_sensor8(0x34f8,0x1f);
     write_cmos_sensor8(0x34f9,0x28);
     write_cmos_sensor8(0x3630,0xa0);
     write_cmos_sensor8(0x3633,0x43);
     write_cmos_sensor8(0x3637,0x45);
     write_cmos_sensor8(0x363c,0xc1);
     write_cmos_sensor8(0x3670,0x4a);
     write_cmos_sensor8(0x3674,0xc0);
     write_cmos_sensor8(0x3675,0xa8);
     write_cmos_sensor8(0x3676,0xac);
     write_cmos_sensor8(0x367c,0x08);
     write_cmos_sensor8(0x367d,0x0b);
     write_cmos_sensor8(0x3690,0x53);
     write_cmos_sensor8(0x3691,0x53);
     write_cmos_sensor8(0x3692,0x63);
     write_cmos_sensor8(0x3698,0x85);
     write_cmos_sensor8(0x3699,0x8c);
     write_cmos_sensor8(0x369a,0x9b);
     write_cmos_sensor8(0x369b,0xb8);
     write_cmos_sensor8(0x369c,0x0f);
     write_cmos_sensor8(0x369d,0x1f);
     write_cmos_sensor8(0x36a2,0x09);
     write_cmos_sensor8(0x36a3,0x0b);
     write_cmos_sensor8(0x36a4,0x0f);
     write_cmos_sensor8(0x36b0,0x4c);
     write_cmos_sensor8(0x36b1,0xd8);
     write_cmos_sensor8(0x36b2,0x01);
     write_cmos_sensor8(0x3722,0x03);
     write_cmos_sensor8(0x3724,0xa1);
     write_cmos_sensor8(0x3903,0xa0);
     write_cmos_sensor8(0x3905,0x4c);
     write_cmos_sensor8(0x391d,0x04);
     write_cmos_sensor8(0x3926,0x21);
     write_cmos_sensor8(0x393f,0x80);
     write_cmos_sensor8(0x3940,0x80);
     write_cmos_sensor8(0x3941,0x00);
     write_cmos_sensor8(0x3942,0x7f);
     write_cmos_sensor8(0x3943,0x7f);
     write_cmos_sensor8(0x3e00,0x00);
     write_cmos_sensor8(0x3e01,0xf9);
     write_cmos_sensor8(0x3e02,0x60);
     write_cmos_sensor8(0x4402,0x02);
     write_cmos_sensor8(0x4403,0x0a);
     write_cmos_sensor8(0x4404,0x1c);
     write_cmos_sensor8(0x4405,0x24);
     write_cmos_sensor8(0x440c,0x2e);
     write_cmos_sensor8(0x440d,0x2e);
     write_cmos_sensor8(0x440e,0x22);
     write_cmos_sensor8(0x440f,0x39);
     write_cmos_sensor8(0x4424,0x01);
     write_cmos_sensor8(0x4509,0x30);
     write_cmos_sensor8(0x450d,0x18);
     write_cmos_sensor8(0x5000,0x0e);
     write_cmos_sensor8(0x5787,0x08);
     write_cmos_sensor8(0x5788,0x07);
     write_cmos_sensor8(0x5789,0x02);
     write_cmos_sensor8(0x0100,0x01);
     }
#endif
}

#if MULTI_WRITE
kal_uint16 addr_data_pair_hs_video_w2sc501csfrontsy[] = {
    0x0103,0x01,
    0x0100,0x00,
    0x36e9,0x80,
    0x37f9,0x80,
    0x36e9,0x23,
    0x37f9,0x23,
    0x301f,0x01,
    0x3253,0x10,
    0x3301,0x12,
    0x3306,0x38,
    0x330b,0xa8,
    0x3333,0x10,
    0x3364,0x56,
    0x3390,0x0b,
    0x3391,0x0f,
    0x3392,0x1f,
    0x3393,0x20,
    0x3394,0x40,
    0x3395,0x58,
    0x33b3,0x40,
    0x349f,0x1e,
    0x34a6,0x09,
    0x34a7,0x0f,
    0x34a8,0x38,
    0x34a9,0x28,
    0x34f8,0x1f,
    0x34f9,0x28,
    0x3630,0xa0,
    0x3633,0x43,
    0x3637,0x45,
    0x363c,0xc1,
    0x3670,0x4a,
    0x3674,0xc0,
    0x3675,0xa8,
    0x3676,0xac,
    0x367c,0x08,
    0x367d,0x0b,
    0x3690,0x53,
    0x3691,0x53,
    0x3692,0x63,
    0x3698,0x85,
    0x3699,0x8c,
    0x369a,0x9b,
    0x369b,0xb8,
    0x369c,0x0f,
    0x369d,0x1f,
    0x36a2,0x09,
    0x36a3,0x0b,
    0x36a4,0x0f,
    0x36b0,0x4c,
    0x36b1,0xd8,
    0x36b2,0x01,
    0x3722,0x03,
    0x3724,0xa1,
    0x3903,0xa0,
    0x3905,0x4c,
    0x391d,0x04,
    0x3926,0x21,
    0x393f,0x80,
    0x3940,0x80,
    0x3941,0x00,
    0x3942,0x7f,
    0x3943,0x7f,
    0x3e00,0x00,
    0x3e01,0xf9,
    0x3e02,0x60,
    0x4402,0x02,
    0x4403,0x0a,
    0x4404,0x1c,
    0x4405,0x24,
    0x440c,0x2e,
    0x440d,0x2e,
    0x440e,0x22,
    0x440f,0x39,
    0x4424,0x01,
    0x4509,0x30,
    0x450d,0x18,
    0x5000,0x0e,
    0x5787,0x08,
    0x5788,0x07,
    0x5789,0x02,
    0x0100,0x01
};
#endif

static void hs_video_setting(void)
{
#if MULTI_WRITE
     w2sc501csfrontsy_table_write_cmos_sensor(
          addr_data_pair_hs_video_w2sc501csfrontsy,
          sizeof(addr_data_pair_hs_video_w2sc501csfrontsy) /
          sizeof(kal_uint16));
#else
     write_cmos_sensor8(0x0103,0x01);
     mdelay(10);
     write_cmos_sensor8(0x0100,0x00);
     write_cmos_sensor8(0x36e9,0x80);
     write_cmos_sensor8(0x37f9,0x80);
     write_cmos_sensor8(0x36e9,0x23);
     write_cmos_sensor8(0x37f9,0x23);
     write_cmos_sensor8(0x301f,0x01);
     write_cmos_sensor8(0x3253,0x10);
     write_cmos_sensor8(0x3301,0x12);
     write_cmos_sensor8(0x3306,0x38);
     write_cmos_sensor8(0x330b,0xa8);
     write_cmos_sensor8(0x3333,0x10);
     write_cmos_sensor8(0x3364,0x56);
     write_cmos_sensor8(0x3390,0x0b);
     write_cmos_sensor8(0x3391,0x0f);
     write_cmos_sensor8(0x3392,0x1f);
     write_cmos_sensor8(0x3393,0x20);
     write_cmos_sensor8(0x3394,0x40);
     write_cmos_sensor8(0x3395,0x58);
     write_cmos_sensor8(0x33b3,0x40);
     write_cmos_sensor8(0x349f,0x1e);
     write_cmos_sensor8(0x34a6,0x09);
     write_cmos_sensor8(0x34a7,0x0f);
     write_cmos_sensor8(0x34a8,0x38);
     write_cmos_sensor8(0x34a9,0x28);
     write_cmos_sensor8(0x34f8,0x1f);
     write_cmos_sensor8(0x34f9,0x28);
     write_cmos_sensor8(0x3630,0xa0);
     write_cmos_sensor8(0x3633,0x43);
     write_cmos_sensor8(0x3637,0x45);
     write_cmos_sensor8(0x363c,0xc1);
     write_cmos_sensor8(0x3670,0x4a);
     write_cmos_sensor8(0x3674,0xc0);
     write_cmos_sensor8(0x3675,0xa8);
     write_cmos_sensor8(0x3676,0xac);
     write_cmos_sensor8(0x367c,0x08);
     write_cmos_sensor8(0x367d,0x0b);
     write_cmos_sensor8(0x3690,0x53);
     write_cmos_sensor8(0x3691,0x53);
     write_cmos_sensor8(0x3692,0x63);
     write_cmos_sensor8(0x3698,0x85);
     write_cmos_sensor8(0x3699,0x8c);
     write_cmos_sensor8(0x369a,0x9b);
     write_cmos_sensor8(0x369b,0xb8);
     write_cmos_sensor8(0x369c,0x0f);
     write_cmos_sensor8(0x369d,0x1f);
     write_cmos_sensor8(0x36a2,0x09);
     write_cmos_sensor8(0x36a3,0x0b);
     write_cmos_sensor8(0x36a4,0x0f);
     write_cmos_sensor8(0x36b0,0x4c);
     write_cmos_sensor8(0x36b1,0xd8);
     write_cmos_sensor8(0x36b2,0x01);
     write_cmos_sensor8(0x3722,0x03);
     write_cmos_sensor8(0x3724,0xa1);
     write_cmos_sensor8(0x3903,0xa0);
     write_cmos_sensor8(0x3905,0x4c);
     write_cmos_sensor8(0x391d,0x04);
     write_cmos_sensor8(0x3926,0x21);
     write_cmos_sensor8(0x393f,0x80);
     write_cmos_sensor8(0x3940,0x80);
     write_cmos_sensor8(0x3941,0x00);
     write_cmos_sensor8(0x3942,0x7f);
     write_cmos_sensor8(0x3943,0x7f);
     write_cmos_sensor8(0x3e00,0x00);
     write_cmos_sensor8(0x3e01,0xf9);
     write_cmos_sensor8(0x3e02,0x60);
     write_cmos_sensor8(0x4402,0x02);
     write_cmos_sensor8(0x4403,0x0a);
     write_cmos_sensor8(0x4404,0x1c);
     write_cmos_sensor8(0x4405,0x24);
     write_cmos_sensor8(0x440c,0x2e);
     write_cmos_sensor8(0x440d,0x2e);
     write_cmos_sensor8(0x440e,0x22);
     write_cmos_sensor8(0x440f,0x39);
     write_cmos_sensor8(0x4424,0x01);
     write_cmos_sensor8(0x4509,0x30);
     write_cmos_sensor8(0x450d,0x18);
     write_cmos_sensor8(0x5000,0x0e);
     write_cmos_sensor8(0x5787,0x08);
     write_cmos_sensor8(0x5788,0x07);
     write_cmos_sensor8(0x5789,0x02);
     write_cmos_sensor8(0x0100,0x01);
#endif
}

#if MULTI_WRITE
kal_uint16 addr_data_pair_slim_video_w2sc501csfrontsy[] = {
    0x0103,0x01,
    0x0100,0x00,
    0x36e9,0x80,
    0x37f9,0x80,
    0x36e9,0x23,
    0x37f9,0x23,
    0x301f,0x01,
    0x3253,0x10,
    0x3301,0x12,
    0x3306,0x38,
    0x330b,0xa8,
    0x3333,0x10,
    0x3364,0x56,
    0x3390,0x0b,
    0x3391,0x0f,
    0x3392,0x1f,
    0x3393,0x20,
    0x3394,0x40,
    0x3395,0x58,
    0x33b3,0x40,
    0x349f,0x1e,
    0x34a6,0x09,
    0x34a7,0x0f,
    0x34a8,0x38,
    0x34a9,0x28,
    0x34f8,0x1f,
    0x34f9,0x28,
    0x3630,0xa0,
    0x3633,0x43,
    0x3637,0x45,
    0x363c,0xc1,
    0x3670,0x4a,
    0x3674,0xc0,
    0x3675,0xa8,
    0x3676,0xac,
    0x367c,0x08,
    0x367d,0x0b,
    0x3690,0x53,
    0x3691,0x53,
    0x3692,0x63,
    0x3698,0x85,
    0x3699,0x8c,
    0x369a,0x9b,
    0x369b,0xb8,
    0x369c,0x0f,
    0x369d,0x1f,
    0x36a2,0x09,
    0x36a3,0x0b,
    0x36a4,0x0f,
    0x36b0,0x4c,
    0x36b1,0xd8,
    0x36b2,0x01,
    0x3722,0x03,
    0x3724,0xa1,
    0x3903,0xa0,
    0x3905,0x4c,
    0x391d,0x04,
    0x3926,0x21,
    0x393f,0x80,
    0x3940,0x80,
    0x3941,0x00,
    0x3942,0x7f,
    0x3943,0x7f,
    0x3e00,0x00,
    0x3e01,0xf9,
    0x3e02,0x60,
    0x4402,0x02,
    0x4403,0x0a,
    0x4404,0x1c,
    0x4405,0x24,
    0x440c,0x2e,
    0x440d,0x2e,
    0x440e,0x22,
    0x440f,0x39,
    0x4424,0x01,
    0x4509,0x30,
    0x450d,0x18,
    0x5000,0x0e,
    0x5787,0x08,
    0x5788,0x07,
    0x5789,0x02,
    0x0100,0x01
};
#endif

static void slim_video_setting(void)
{
#if MULTI_WRITE
    w2sc501csfrontsy_table_write_cmos_sensor(
        addr_data_pair_slim_video_w2sc501csfrontsy,
        sizeof(addr_data_pair_slim_video_w2sc501csfrontsy) /
        sizeof(kal_uint16));
#else
     write_cmos_sensor8(0x0103,0x01);
     mdelay(10);
     write_cmos_sensor8(0x0100,0x00);
     write_cmos_sensor8(0x36e9,0x80);
     write_cmos_sensor8(0x37f9,0x80);
     write_cmos_sensor8(0x36e9,0x23);
     write_cmos_sensor8(0x37f9,0x23);
     write_cmos_sensor8(0x301f,0x01);
     write_cmos_sensor8(0x3253,0x10);
     write_cmos_sensor8(0x3301,0x12);
     write_cmos_sensor8(0x3306,0x38);
     write_cmos_sensor8(0x330b,0xa8);
     write_cmos_sensor8(0x3333,0x10);
     write_cmos_sensor8(0x3364,0x56);
     write_cmos_sensor8(0x3390,0x0b);
     write_cmos_sensor8(0x3391,0x0f);
     write_cmos_sensor8(0x3392,0x1f);
     write_cmos_sensor8(0x3393,0x20);
     write_cmos_sensor8(0x3394,0x40);
     write_cmos_sensor8(0x3395,0x58);
     write_cmos_sensor8(0x33b3,0x40);
     write_cmos_sensor8(0x349f,0x1e);
     write_cmos_sensor8(0x34a6,0x09);
     write_cmos_sensor8(0x34a7,0x0f);
     write_cmos_sensor8(0x34a8,0x38);
     write_cmos_sensor8(0x34a9,0x28);
     write_cmos_sensor8(0x34f8,0x1f);
     write_cmos_sensor8(0x34f9,0x28);
     write_cmos_sensor8(0x3630,0xa0);
     write_cmos_sensor8(0x3633,0x43);
     write_cmos_sensor8(0x3637,0x45);
     write_cmos_sensor8(0x363c,0xc1);
     write_cmos_sensor8(0x3670,0x4a);
     write_cmos_sensor8(0x3674,0xc0);
     write_cmos_sensor8(0x3675,0xa8);
     write_cmos_sensor8(0x3676,0xac);
     write_cmos_sensor8(0x367c,0x08);
     write_cmos_sensor8(0x367d,0x0b);
     write_cmos_sensor8(0x3690,0x53);
     write_cmos_sensor8(0x3691,0x53);
     write_cmos_sensor8(0x3692,0x63);
     write_cmos_sensor8(0x3698,0x85);
     write_cmos_sensor8(0x3699,0x8c);
     write_cmos_sensor8(0x369a,0x9b);
     write_cmos_sensor8(0x369b,0xb8);
     write_cmos_sensor8(0x369c,0x0f);
     write_cmos_sensor8(0x369d,0x1f);
     write_cmos_sensor8(0x36a2,0x09);
     write_cmos_sensor8(0x36a3,0x0b);
     write_cmos_sensor8(0x36a4,0x0f);
     write_cmos_sensor8(0x36b0,0x4c);
     write_cmos_sensor8(0x36b1,0xd8);
     write_cmos_sensor8(0x36b2,0x01);
     write_cmos_sensor8(0x3722,0x03);
     write_cmos_sensor8(0x3724,0xa1);
     write_cmos_sensor8(0x3903,0xa0);
     write_cmos_sensor8(0x3905,0x4c);
     write_cmos_sensor8(0x391d,0x04);
     write_cmos_sensor8(0x3926,0x21);
     write_cmos_sensor8(0x393f,0x80);
     write_cmos_sensor8(0x3940,0x80);
     write_cmos_sensor8(0x3941,0x00);
     write_cmos_sensor8(0x3942,0x7f);
     write_cmos_sensor8(0x3943,0x7f);
     write_cmos_sensor8(0x3e00,0x00);
     write_cmos_sensor8(0x3e01,0xf9);
     write_cmos_sensor8(0x3e02,0x60);
     write_cmos_sensor8(0x4402,0x02);
     write_cmos_sensor8(0x4403,0x0a);
     write_cmos_sensor8(0x4404,0x1c);
     write_cmos_sensor8(0x4405,0x24);
     write_cmos_sensor8(0x440c,0x2e);
     write_cmos_sensor8(0x440d,0x2e);
     write_cmos_sensor8(0x440e,0x22);
     write_cmos_sensor8(0x440f,0x39);
     write_cmos_sensor8(0x4424,0x01);
     write_cmos_sensor8(0x4509,0x30);
     write_cmos_sensor8(0x450d,0x18);
     write_cmos_sensor8(0x5000,0x0e);
     write_cmos_sensor8(0x5787,0x08);
     write_cmos_sensor8(0x5788,0x07);
     write_cmos_sensor8(0x5789,0x02);
     write_cmos_sensor8(0x0100,0x01);
#endif
}


#if 0
//+bug682590,zhanghengyuan.wt,ADD,2022/2/18, n26 sc501cs front xl porting otp
#define SC501CS_OTP_DUMP

#ifdef SC501CS_OTP_DUMP
static void dumpEEPROMData1(int u4Length,u8* pu1Params)
{
	int i = 0;
	for(i = 0; i < u4Length; i += 16){
		if(u4Length - i  >= 16){
			LOG_INF("eeprom[%d-%d]:0x%2x 0x%2x 0x%2x 0x%2x 0x%2x 0x%2x 0x%2x 0x%2x 0x%2x 0x%2x 0x%2x 0x%2x 0x%2x 0x%2x 0x%2x 0x%2x ",
			i,i+15,pu1Params[i],pu1Params[i+1],pu1Params[i+2],pu1Params[i+3],pu1Params[i+4],pu1Params[i+5],pu1Params[i+6]
			,pu1Params[i+7],pu1Params[i+8],pu1Params[i+9],pu1Params[i+10],pu1Params[i+11],pu1Params[i+12],pu1Params[i+13],pu1Params[i+14]
			,pu1Params[i+15]);
		}else{
			int j = i;
			for(;j < u4Length;j++)
			LOG_INF("eeprom[%d] = 0x%2x ",j,pu1Params[j]);
		}
	}
	LOG_INF("\n");
}

#endif

#define OTP_SECTION1_START_ADDR 0x80EC  //modifiable;the first section with 1800 Bytes OTP;
#define OTP_SECTION1_END_ADDR 0x87f3    //modifiable;
#define OTP_SECTION2_START_ADDR 0x8800  //modifiable;the second section with 2036 Bytes OTP;
#define OTP_SECTION2_END_ADDR 0x8FE7    //modifiable;don't set over 0x8FF3

#define MODULE_INFO_SIZE 7
#define AWB_DATA_SIZE 16
#define LSC_DATA_SIZE 1868

#define MODULE_GROUP1_ADDR 0x80EC
#define MODULE_GROUP2_ADDR 0x8870

#define AWB_GROUP1_FLAG 0x8103
#define AWB_GROUP2_FLAG 0x8887

#define LSC_GROUP1_FLAG 0x8115
#define LSC_GROUP2_FLAG 0x8899

#define VALID_OTP_FLAG 0x55

static unsigned char sc501cs_data_lsc[LSC_DATA_SIZE + 1] = {0};/*Add check sum*/
static unsigned char sc501cs_data_awb[AWB_DATA_SIZE + 1] = {0};/*Add check sum*/
static unsigned char sc501cs_data_info[MODULE_INFO_SIZE + 1] = {0};/*Add check sum*/
static unsigned char sc501cs_module_id = 0;

static int read_sc501cs_module_info(kal_uint32 addr)
{
	int year = 0, month = 0, day = 0;
	int position = 0,lens_id = 0,vcm_id = 0;
	int check_sum = 0, check_sum_cal = 0;
	int i;

	for(i = 0; i < MODULE_INFO_SIZE + 1; i++){
		sc501cs_data_info[i]=read_cmos_sensor(addr+i+1);
		check_sum_cal += sc501cs_data_info[i];
	}

	check_sum_cal = check_sum_cal + VALID_OTP_FLAG - sc501cs_data_info[MODULE_INFO_SIZE];
	check_sum_cal = (check_sum_cal % 255) + 1;
	sc501cs_module_id = sc501cs_data_info[0];
	position = sc501cs_data_info[1];
	lens_id = sc501cs_data_info[2];
	vcm_id = sc501cs_data_info[3];
	year = sc501cs_data_info[4];
	month = sc501cs_data_info[5];
	day = sc501cs_data_info[6];
	check_sum = sc501cs_data_info[MODULE_INFO_SIZE];

#ifdef SC501CS_OTP_DUMP
	dumpEEPROMData1(MODULE_INFO_SIZE,&sc501cs_data_info[0]);
#endif
	LOG_INF("=== SC501CS INFO module_id=0x%x position=0x%x ===\n", sc501cs_module_id, position);
	LOG_INF("=== SC501CS INFO lens_id=0x%x,vcm_id=0x%x ===\n",lens_id, vcm_id);
	LOG_INF("=== SC501CS INFO date is %d-%d-%d ===\n",year,month,day);
	LOG_INF("=== SC501CS INFO check_sum=0x%x,check_sum_cal=0x%x ===\n", check_sum, check_sum_cal);
	if(check_sum == check_sum_cal){
		return 1;
	}else{
		return 0;
	}
}

static int read_sc501cs_awb_info(kal_uint32 addr)
{
	int check_sum_awb = 0, check_sum_awb_cal = 0;
	int r = 0,b = 0,gr = 0, gb = 0, golden_r = 0, golden_b = 0, golden_gr = 0, golden_gb = 0;
	int i;

	for(i = 0; i < AWB_DATA_SIZE + 1; i++){
		sc501cs_data_awb[i]=read_cmos_sensor(addr+i+1);
		check_sum_awb_cal += sc501cs_data_awb[i];
	}

	check_sum_awb_cal = check_sum_awb_cal + VALID_OTP_FLAG - sc501cs_data_awb[AWB_DATA_SIZE];

	LOG_INF("check_sum_awb_cal =0x%x \n",check_sum_awb_cal);
	r = ((sc501cs_data_awb[1]<<8)&0xff00)|(sc501cs_data_awb[0]&0xff);
	b = ((sc501cs_data_awb[3]<<8)&0xff00)|(sc501cs_data_awb[2]&0xff);
	gr = ((sc501cs_data_awb[5]<<8)&0xff00)|(sc501cs_data_awb[4]&0xff);
	gb = ((sc501cs_data_awb[7]<<8)&0xff00)|(sc501cs_data_awb[6]&0xff);
	golden_r = ((sc501cs_data_awb[9]<<8)&0xff00)|(sc501cs_data_awb[8]&0xff);
	golden_b = ((sc501cs_data_awb[11]<<8)&0xff00)|(sc501cs_data_awb[10]&0xff);
	golden_gr = ((sc501cs_data_awb[13]<<8)&0xff00)|(sc501cs_data_awb[12]&0xff);
	golden_gb = ((sc501cs_data_awb[15]<<8)&0xff00)|(sc501cs_data_awb[14]&0xff);
	check_sum_awb = sc501cs_data_awb[AWB_DATA_SIZE];
	check_sum_awb_cal = (check_sum_awb_cal % 255) + 1;

#ifdef SC501CS_OTP_DUMP
	dumpEEPROMData1(AWB_DATA_SIZE,&sc501cs_data_awb[0]);
#endif
	LOG_INF("=== SC501CS AWB r=0x%x, b=0x%x, gr=%x, gb=0x%x ===\n", r, b,gb, gr);
	LOG_INF("=== SC501CS AWB gr=0x%x,gb=0x%x,gGr=%x, gGb=0x%x ===\n", golden_r, golden_b, golden_gr, golden_gb);
	LOG_INF("=== SC501CS AWB check_sum_awb=0x%x,check_sum_awb_cal=0x%x ===\n",check_sum_awb,check_sum_awb_cal);
	if(check_sum_awb == check_sum_awb_cal){
		return 1;
	}else{
		return 0;
	}
}

static int sc501cs_sensor_otp_init(int section)
{
	int delay=0;
	kal_uint16 otp_start_add_h,otp_start_add_l,otp_end_add_h,otp_end_add_l;
	
	if(section == 1) {
		otp_start_add_h = (OTP_SECTION1_START_ADDR&0x0f00)>>8;
		otp_start_add_l = OTP_SECTION1_START_ADDR&0x00ff;
		otp_end_add_h   = (OTP_SECTION1_END_ADDR&0x0f00)>>8;
		otp_end_add_l   = OTP_SECTION1_END_ADDR&0x00ff;
		
		write_cmos_sensor8(0x36b0,0x4c);
		write_cmos_sensor8(0x36b1,0xd8);
		write_cmos_sensor8(0x36b2,0x01);
		write_cmos_sensor8(0x4408,otp_start_add_h);
		write_cmos_sensor8(0x4409,otp_start_add_l);
		write_cmos_sensor8(0x440a,otp_end_add_h);
		write_cmos_sensor8(0x440b,otp_end_add_l);
		write_cmos_sensor8(0x4401,0x1f);
		write_cmos_sensor8(0x4400,0x11);

		//msleep(5);
		while((read_cmos_sensor(0x4420)&0x01) == 0x01){
			delay++;
			if(delay == 1000) {
				LOG_INF("1st section OTP is still busy for reading. R0x4420[0]!=0 \n");
				return 0;
			}
		}
	} else if(section == 2) {
		otp_start_add_h = (OTP_SECTION2_START_ADDR&0x0f00)>>8;
		otp_start_add_l = OTP_SECTION2_START_ADDR&0x00ff;
		otp_end_add_h   = (OTP_SECTION2_END_ADDR&0x0f00)>>8;
		otp_end_add_l   = OTP_SECTION2_END_ADDR&0x00ff;
		
		write_cmos_sensor8(0x4408,otp_start_add_h);
		write_cmos_sensor8(0x4409,otp_start_add_l);
		write_cmos_sensor8(0x440a,otp_end_add_h);
		write_cmos_sensor8(0x440b,otp_end_add_l);
		write_cmos_sensor8(0x4401,0x1e);
		write_cmos_sensor8(0x4400,0x11);

		while((read_cmos_sensor(0x4420)&0x01) == 0x01){
			delay++;
			if(delay == 1000) {
				LOG_INF("2nd section OTP is still busy for reading. R0x4420[0]!=0 \n");
				return 0;
			}
		}
	} else {
		LOG_INF("section input error \n");
		return 0;
	}
	#if 0
	otp_start_add_h = (OTP_SECTION1_START_ADDR&0x0f00)>>8;
	otp_start_add_l = OTP_SECTION1_START_ADDR&0x00ff;
	otp_end_add_h   = (OTP_SECTION1_END_ADDR&0x0f00)>>8;
	otp_end_add_l   = OTP_SECTION1_END_ADDR&0x00ff;
	write_cmos_sensor8(0x4424,0x03);
	write_cmos_sensor8(0x4408,0x00);
	write_cmos_sensor8(0x4409,0x00);
	write_cmos_sensor8(0x440a,0x07);
	write_cmos_sensor8(0x440b,0xff);
	write_cmos_sensor8(0x4402,0x02);
	write_cmos_sensor8(0x4403,0x0a);
	write_cmos_sensor8(0x4404,0x1c);
	write_cmos_sensor8(0x4405,0x24);
	write_cmos_sensor8(0x440c,0x2e);
	write_cmos_sensor8(0x440d,0x2e);
	write_cmos_sensor8(0x440e,0x22);
	write_cmos_sensor8(0x440f,0x39);
	write_cmos_sensor8(0x4401,0x1f);
	#endif
	
	return 1;
}


static int read_sc501cs_lsc_info(kal_uint32 addr)
{
	int check_sum_lsc = 0, check_sum_lsc_cal = 0;
	int i;

	if(LSC_GROUP1_FLAG == addr){
		for(i = 0; i < OTP_SECTION1_END_ADDR - addr; i++){
			sc501cs_data_lsc[i]=read_cmos_sensor(addr+i+1);
		}
		//LSC_GROUP1 need jump 0x87F4-0x87FF addr, and start the 2nd otp option.
		sc501cs_sensor_otp_init(2);

		for(i =OTP_SECTION1_END_ADDR - addr ; i < LSC_DATA_SIZE + 1; i++){
			sc501cs_data_lsc[i]=read_cmos_sensor(addr+i+12+1);
		}
	}else if(LSC_GROUP2_FLAG == addr){
		for(i = 0; i < LSC_DATA_SIZE + 1; i++){
			sc501cs_data_lsc[i]=read_cmos_sensor(addr+i+1);
		}
	}

	check_sum_lsc_cal += VALID_OTP_FLAG;

	for(i = 0; i < LSC_DATA_SIZE; i++){
		check_sum_lsc_cal += sc501cs_data_lsc[i];
	}
	LOG_INF("check_sum_lsc_cal =0x%x \n",check_sum_lsc_cal);
	check_sum_lsc = sc501cs_data_lsc[LSC_DATA_SIZE];
	check_sum_lsc_cal = (check_sum_lsc_cal % 255) + 1;

#ifdef SC501CS_OTP_DUMP
	dumpEEPROMData1(LSC_DATA_SIZE,&sc501cs_data_lsc[0]);
#endif
	LOG_INF("=== SC501CS LSC check_sum_lsc=0x%x, check_sum_lsc_cal=0x%x ===\n", check_sum_lsc, check_sum_lsc_cal);
	if(check_sum_lsc == check_sum_lsc_cal){
		return 1;
	}else{
		return 0;
	}
}

static int sc501cs_sensor_otp_info(void)
{
	int ret = 0;
	kal_uint16 total_ret = 0x1f;  //bit0:module info, bit1:AWB data, bit2:LSC data, bit3:otp init-1, bit4:otp init-2;

	LOG_INF("come to %s:%d E!\n", __func__, __LINE__);

	//read first section, R0x80ec~R0x87f3;
	ret = sc501cs_sensor_otp_init(1);
	if(ret != 1){
		LOG_INF("=== sc501cs_sensor_otp_init(1) failed ===\n");
		total_ret = total_ret & 0xf7;
	}
	//Get Module Info
	if(VALID_OTP_FLAG == read_cmos_sensor(MODULE_GROUP1_ADDR)) {
		ret = read_sc501cs_module_info(MODULE_GROUP1_ADDR);
		if(ret != 1){
			LOG_INF("=== read_sc501cs_module_info(MODULE_GROUP1_ADDR) checksum error ===\n");
			total_ret = total_ret & 0xfe;
		}
	} else {
		LOG_INF("=== sc501cs_data_info group1 invalid ===\n");
	}
	//Get AWB data
	if(VALID_OTP_FLAG == read_cmos_sensor(AWB_GROUP1_FLAG)) {
		ret = read_sc501cs_awb_info(AWB_GROUP1_FLAG);
		if(ret != 1){
			LOG_INF("=== read_sc501cs_awb_info(AWB_GROUP1_FLAG) checksum error ===\n");
			total_ret = total_ret & 0xfd;
		}
	} else {
		LOG_INF("=== sc501cs_data_awb group1 invalid ===\n");
	}
	//Get LSC data
	if(VALID_OTP_FLAG == read_cmos_sensor(LSC_GROUP1_FLAG)) {
		ret = read_sc501cs_lsc_info(LSC_GROUP1_FLAG);
		if(ret != 1){
			LOG_INF("=== read_sc501cs_lsc_info(LSC_GROUP1_FLAG) checksum error ===\n");
			total_ret = total_ret & 0xfb;
		}
	} else {
		LOG_INF("=== sc501cs_data_lsc group1 invalid ===\n");
		ret = sc501cs_sensor_otp_init(2);
		if(ret != 1){
			LOG_INF("=== sc501cs_sensor_otp_init(2) failed ===\n");
			total_ret = total_ret & 0xef;
		}		
	}

	//Get Module Info
	if(VALID_OTP_FLAG == read_cmos_sensor(MODULE_GROUP2_ADDR)) {
		ret = read_sc501cs_module_info(MODULE_GROUP2_ADDR);
		if(ret != 1){
			LOG_INF("=== read_sc501cs_module_info(MODULE_GROUP2_ADDR) checksum error ===\n");
			total_ret = total_ret & 0xfe;
		}
	} else {
		LOG_INF("=== sc501cs_data_info group2 invalid ===\n");
	}
	//Get AWB data
	if(VALID_OTP_FLAG == read_cmos_sensor(AWB_GROUP2_FLAG)) {
		ret = read_sc501cs_awb_info(AWB_GROUP2_FLAG);
		if(ret != 1){
			LOG_INF("=== read_sc501cs_awb_info(AWB_GROUP2_FLAG) checksum error ===\n");
			total_ret = total_ret & 0xfd;
		}
	} else {
		LOG_INF("=== sc501cs_data_awb group2 invalid ===\n");
	}
	//Get LSC data
	if(VALID_OTP_FLAG == read_cmos_sensor(LSC_GROUP2_FLAG)) {
		ret = read_sc501cs_lsc_info(LSC_GROUP2_FLAG);
		if(ret != 1){
			LOG_INF("=== read_sc501cs_lsc_info(LSC_GROUP2_FLAG) checksum error ===\n");
			total_ret = total_ret & 0xfb;
		}
	} else {
		LOG_INF("=== sc501cs_data_lsc group2 invalid ===\n");
	}

	LOG_INF("=== sc501cs_sensor_otp_info option done, total_ret = 0x%2x ===\n",total_ret);
	return total_ret;
}
//-bug682590,zhanghengyuan.wt,ADD,2022/2/18, n26 sc501cs front xl porting otp


#include "cam_cal_define.h"
#include <linux/slab.h>
static struct stCAM_CAL_DATAINFO_STRUCT w2sc501csfrontsy_eeprom_data ={
    .sensorID= W2SC501CSFRONTSY_SENSOR_ID,
    .deviceID = 0x02,
    .dataLength = 0x0766,
    .sensorVendorid = 0x18050000,
    .vendorByte = {1,2,3,4},
    .dataBuffer = NULL,
};


extern int imgSensorSetEepromData(struct stCAM_CAL_DATAINFO_STRUCT* pData);
#endif

static kal_uint32 get_imgsensor_id(UINT32 *sensor_id)
{
    kal_uint8 i = 0;
    kal_uint8 retry = 2;
    //int rc = 0;

    while (imgsensor_info.i2c_addr_table[i] != 0xff) {
        spin_lock(&imgsensor_drv_lock);
        imgsensor.i2c_write_id = imgsensor_info.i2c_addr_table[i];
        spin_unlock(&imgsensor_drv_lock);
        do {
            *sensor_id = return_sensor_id();
            if (*sensor_id == imgsensor_info.sensor_id) {

#if 0
			//+bug682590,zhanghengyuan.wt,ADD,2022/2/18, n26 sc501cs front xl porting otp
			rc = sc501cs_sensor_otp_info();
			if(rc != 0x1f){
				*sensor_id = 0xFFFFFFFF;
				LOG_INF("sc501cs read OTP failed: 0x%2x \n",rc);
				retry--;
				continue;
				//return ERROR_SENSOR_CONNECT_FAIL;
			}else{
				LOG_INF("sc501cs read OTP successed: 0x%2x \n",rc);

			w2sc501csfrontsy_eeprom_data.dataBuffer = kmalloc(w2sc501csfrontsy_eeprom_data.dataLength, GFP_KERNEL);
			if (w2sc501csfrontsy_eeprom_data.dataBuffer == NULL) {
				LOG_INF("w2sc501csfrontsy_eeprom_data->dataBuffer is malloc fail\n");
				return -EFAULT;
			}

			memcpy(w2sc501csfrontsy_eeprom_data.dataBuffer, (u8 *)&sc501cs_data_info, MODULE_INFO_SIZE);
			memcpy(w2sc501csfrontsy_eeprom_data.dataBuffer+MODULE_INFO_SIZE, (u8 *)&sc501cs_data_awb, AWB_DATA_SIZE);
			memcpy(w2sc501csfrontsy_eeprom_data.dataBuffer+MODULE_INFO_SIZE+AWB_DATA_SIZE, (u8 *)&sc501cs_data_lsc, LSC_DATA_SIZE);

			imgSensorSetEepromData(&w2sc501csfrontsy_eeprom_data);
		}
		//+bug682590,zhanghengyuan.wt,ADD,2022/2/18, n26 sc501cs front xl porting otp
#endif

                return ERROR_NONE;
            }

            retry--;
        } while (retry > 0);
        i++;
        retry = 2;
    }
    if (*sensor_id != imgsensor_info.sensor_id) {
        pr_err("w2sc501csfrontsy Read id fail,sensor id: 0x%x", *sensor_id);
        *sensor_id = 0xFFFFFFFF;
        return ERROR_SENSOR_CONNECT_FAIL;
    }
    return ERROR_NONE;
}
// -bug727089, liangyiyi.wt, MODIFY, 2022/3/2, modify 1st front camera read otp by fae
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
    kal_uint16 sensor_id = 0;

    LOG_INF("[open]: PLATFORM:MT6765,MIPI 2LANE");
    LOG_INF("preview 2592*1944@30fps,360Mbps/lane;"
        "capture 2592*1944@30fps,880Mbps/lane\n");
    while (imgsensor_info.i2c_addr_table[i] != 0xff) {
        spin_lock(&imgsensor_drv_lock);
        imgsensor.i2c_write_id = imgsensor_info.i2c_addr_table[i];
        spin_unlock(&imgsensor_drv_lock);
        do {
            sensor_id = return_sensor_id();
            if (sensor_id == imgsensor_info.sensor_id) {
                LOG_INF("i2c write id: 0x%x, sensor id: 0x%x",
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
        LOG_INF("open sensor id fail: 0x%x\n", sensor_id);
        return ERROR_SENSOR_CONNECT_FAIL;
    }
    /* initail sequence write in  */
    sensor_init();

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
    spin_unlock(&imgsensor_drv_lock);
    return ERROR_NONE;
}    /*    open  */
static kal_uint32 close(void)
{
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

    if (imgsensor.current_fps == imgsensor_info.cap.max_framerate)    {
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

    return ERROR_NONE;

}    /* capture() */
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
    capture_setting(imgsensor.current_fps);
    return ERROR_NONE;
}    /*    normal_video   */

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

    return ERROR_NONE;
}    /*    slim_video     */

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
    case MSDK_SCENARIO_ID_CAMERA_ZSD:
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
    LOG_INF("enable = %d, framerate = %d", enable, framerate);
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

    LOG_INF("scenario_id = %d, framerate = %d",
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
    default:
    break;
    }

    return ERROR_NONE;
}

static kal_uint32 set_test_pattern_mode(kal_bool enable)
{
    LOG_INF("set_test_pattern_mode enable: %d", enable);

    if (enable) {
        write_cmos_sensor8(0x4501, 0xbc);
    } else {
        write_cmos_sensor8(0x4501, 0xb4);
    }
    spin_lock(&imgsensor_drv_lock);
    imgsensor.test_pattern = enable;
    spin_unlock(&imgsensor_drv_lock);
    return ERROR_NONE;
}

static kal_uint32 streaming_control(kal_bool enable)
{
    pr_debug("streaming_enable(0=Sw Standby,1=streaming): %d\n", enable);

    if (enable) {
        write_cmos_sensor8(0x0100, 0x01); // stream on
		if ((read_cmos_sensor(0x8000) == 0x36) &&
			(read_cmos_sensor(0x8001) == 0x37) &&
			(read_cmos_sensor(0x8002) == 0x5a) &&
			(read_cmos_sensor(0x8003) == 0x50) &&
			(read_cmos_sensor(0x8004) == 0x46) &&
			(read_cmos_sensor(0x8005) == 0x37)) {
			switch (read_cmos_sensor(0x8007)) {
				case 0x04:
				case 0x05:
				case 0x07:
					write_cmos_sensor8(0x3670,0x4b);
					write_cmos_sensor8(0x36a6,0x03);
					write_cmos_sensor8(0x36a7,0x07);
					write_cmos_sensor8(0x36ab,0x8b);
					write_cmos_sensor8(0x36ac,0x8d);
					write_cmos_sensor8(0x36ad,0x8d);
					LOG_INF("Logic 1: R0x36ab = 0x%2x \n", read_cmos_sensor(0x36ab));
				break;
				default:
				break;
			}
		} else {
			write_cmos_sensor8(0x3670,0x4b);
			write_cmos_sensor8(0x36a6,0x03);
			write_cmos_sensor8(0x36a7,0x07);
			write_cmos_sensor8(0x36ab,0xc1);
			write_cmos_sensor8(0x36ac,0xc1);
			write_cmos_sensor8(0x36ad,0xc1);
			LOG_INF("Logic 2: R0x36ab = 0x%2x \n", read_cmos_sensor(0x36ab));
		}
    } else {
        write_cmos_sensor8(0x0100, 0x00); // stream off
	}

    mdelay(10);
    return ERROR_NONE;
}
// -bug727089, liangyiyi.wt, MODIFY, 2022/3/29, update setting for 1st front camera module consistency by fae

static kal_uint32 feature_control(
            MSDK_SENSOR_FEATURE_ENUM feature_id,
            UINT8 *feature_para, UINT32 *feature_para_len)
{
    UINT16 *feature_return_para_16 = (UINT16 *) feature_para;
    UINT16 *feature_data_16 = (UINT16 *) feature_para;
    UINT32 *feature_return_para_32 = (UINT32 *) feature_para;
    UINT32 *feature_data_32 = (UINT32 *) feature_para;
    INT32 *feature_return_para_i32 = (INT32 *) feature_para;
    unsigned long long *feature_data =
        (unsigned long long *) feature_para;

    struct SENSOR_WINSIZE_INFO_STRUCT *wininfo;
    MSDK_SENSOR_REG_INFO_STRUCT *sensor_reg_data =
        (MSDK_SENSOR_REG_INFO_STRUCT *) feature_para;

    LOG_INF("feature_id = %d\n", feature_id);
    switch (feature_id) {
    case SENSOR_FEATURE_GET_PERIOD:
        *feature_return_para_16++ = imgsensor.line_length;
        *feature_return_para_16 = imgsensor.frame_length;
        *feature_para_len = 4;
    break;
    case SENSOR_FEATURE_GET_PIXEL_CLOCK_FREQ:
        *feature_return_para_32 = imgsensor.pclk;
        *feature_para_len = 4;
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
    //+bug727089, liangyiyi.wt,MODIFY, 2022.03.09, fix KASAN opened to Monitor memory, camera  memory access error.
    case SENSOR_FEATURE_SET_FRAMERATE:
        LOG_INF("current fps :%d\n", *feature_data_32);
        spin_lock(&imgsensor_drv_lock);
        imgsensor.current_fps = (UINT16)*feature_data_32;
        spin_unlock(&imgsensor_drv_lock);
    break;

    case SENSOR_FEATURE_SET_HDR:
        LOG_INF("ihdr enable: %d\n", (BOOL)*feature_data_32);
        spin_lock(&imgsensor_drv_lock);
        imgsensor.ihdr_en = (BOOL)*feature_data_32;
        spin_unlock(&imgsensor_drv_lock);
    break;
    //-bug727089, liangyiyi.wt,MODIFY, 2022.03.09, fix KASAN opened to Monitor memory, camera  memory access error.
    case SENSOR_FEATURE_GET_CROP_INFO:
        LOG_INF("SENSOR_FEATURE_GET_CROP_INFO scenarioId:%d\n",
                (UINT32)*feature_data);

        wininfo = (struct SENSOR_WINSIZE_INFO_STRUCT *)
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
    #if 0
        ihdr_write_shutter_gain((UINT16)*feature_data,
            (UINT16)*(feature_data+1), (UINT16)*(feature_data+2));
    #endif
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
        LOG_INF("sc501sc SENSOR_FEATURE_GET_MIPI_PIXEL_RATE");
        *(MUINT32 *) (uintptr_t) (*(feature_data + 1)) = rate;
    }
    break;
    case SENSOR_FEATURE_GET_TEMPERATURE_VALUE:
        *feature_return_para_i32 = 0;
        *feature_para_len = 4;
    break;
    case SENSOR_FEATURE_SET_STREAMING_SUSPEND:
        streaming_control(KAL_FALSE);
    break;
    case SENSOR_FEATURE_SET_STREAMING_RESUME:
        if (*feature_data != 0)
            set_shutter(*feature_data);
        streaming_control(KAL_TRUE);
    break;
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

UINT32 W2SC501CSFRONTSY_MIPI_RAW_SensorInit(struct SENSOR_FUNCTION_STRUCT **pfFunc)
{
    /* To Do : Check Sensor status here */
    if (pfFunc != NULL)
        *pfFunc =  &sensor_func;
    return ERROR_NONE;
}    /*    N26_SC501CS_FRONT_LY_MIPI_RAW_SensorInit    */
