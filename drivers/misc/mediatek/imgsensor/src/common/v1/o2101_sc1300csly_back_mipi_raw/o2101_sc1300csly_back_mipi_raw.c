// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

/* hs04 code for DEAL6398A-1684 by chenjun at 2022/09/24 start */

#define PFX "o2101_sc1300csly_back_mipi_raw"
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

#include "o2101_sc1300csly_back_mipi_raw.h"
#define cam_pr_debug(format, args...)    \
	pr_debug(PFX "[%s] " format, __func__, ##args)

//PDAF
#define ENABLE_PDAF
//extern unsigned char fusion_id_main[48];

#define MULTI_WRITE 1
static DEFINE_SPINLOCK(imgsensor_drv_lock);

static struct imgsensor_info_struct imgsensor_info = {
	.sensor_id = O2101_SC1300CSLY_BACK_SENSOR_ID,

	.checksum_value = 0x44724ea1,

	.pre = {
		.pclk = 120000000,
		.linelength =  1250,
		.framelength = 3200,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 2104,
		.grabwindow_height = 1560,
		.mipi_data_lp2hs_settle_dc = 19,//unit , ns , 85
		.mipi_pixel_rate = 480000000,
		.max_framerate = 300,
	},
	.cap = {
		.pclk = 120000000,
		.linelength = 1250,
		.framelength = 3200,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 4208,
		.grabwindow_height = 3120,
		.mipi_data_lp2hs_settle_dc = 19,
		.mipi_pixel_rate = 480000000,
		.max_framerate = 300,
	},
	/*size@15fps, same as capture*/
	.cap1 = {
		.pclk = 120000000,
		.linelength = 1250,
		.framelength = 3200,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 4208,
		.grabwindow_height = 3120,
		.mipi_data_lp2hs_settle_dc = 19,
		.mipi_pixel_rate = 480000000,
		.max_framerate = 300,
	},
	.normal_video = {
		.pclk = 120000000,
		.linelength = 1250,
		.framelength = 3200,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 4208,
		.grabwindow_height = 3120,
		.mipi_data_lp2hs_settle_dc = 19,
		.mipi_pixel_rate = 480000000,
		.max_framerate = 300,
	},
	.hs_video = {
		.pclk = 120000000,
		.linelength = 1250,
		.framelength = 800,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 1280,
		.grabwindow_height = 720,
		.mipi_data_lp2hs_settle_dc = 19,
		.mipi_pixel_rate = 480000000,
		.max_framerate = 1200,
	},
	.slim_video = {
		.pclk = 120000000,
		.linelength =  1250,
		.framelength = 3200,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 1920,
		.grabwindow_height = 1080,
		.mipi_data_lp2hs_settle_dc = 19,//unit , ns , 85
		.mipi_pixel_rate = 480000000,
		.max_framerate = 300,
	},
	.custom1 = {
		.pclk = 120000000,
		.linelength = 1250,
		.framelength = 4000,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 4208,
		.grabwindow_height = 3120,
		.mipi_data_lp2hs_settle_dc = 19,
		.mipi_pixel_rate = 480000000,
		.max_framerate = 240,
	},
	.margin = 5,
	.min_shutter = 0x04,
	.max_frame_length = 0x7fff,
	.ae_shut_delay_frame = 0,
	.ae_sensor_gain_delay_frame = 0,
	.ae_ispGain_delay_frame = 2,
	.ihdr_support = 0,
	.ihdr_le_firstline = 0,
	.sensor_mode_num = 6,

	.cap_delay_frame = 3,
	.pre_delay_frame = 2,
	.video_delay_frame = 2,
	.hs_video_delay_frame = 2,
	.slim_video_delay_frame = 2,
	.custom1_delay_frame = 2,
	.isp_driving_current = ISP_DRIVING_6MA,
	.sensor_interface_type = SENSOR_INTERFACE_TYPE_MIPI,
	.mipi_sensor_type = MIPI_OPHY_NCSI2,
	.mipi_settle_delay_mode = MIPI_SETTLEDELAY_AUTO,
	.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_B,
	.mclk = 24,
	.mipi_lane_num = SENSOR_MIPI_4_LANE,
	.i2c_addr_table = {0x6c,0x20, 0xff},
	.i2c_speed = 400,
};

static struct imgsensor_struct imgsensor = {
	.mirror = IMAGE_NORMAL,
	.sensor_mode = IMGSENSOR_MODE_INIT,
	.shutter = 0x3D0,
	.gain = 0x100,
	.dummy_pixel = 0,
	.dummy_line = 0,
	.current_fps = 300,
	.autoflicker_en = KAL_FALSE,
	.test_pattern = KAL_FALSE,
	.current_scenario_id = MSDK_SCENARIO_ID_CAMERA_PREVIEW,
	.ihdr_en = 0,
	.i2c_write_id = 0x6c,
};

/* Sensor output window information */
static struct SENSOR_WINSIZE_INFO_STRUCT
	imgsensor_winsize_info[6] = {
	{ 4208, 3120,	  0,  	0, 4208, 3120, 2104, 1560,   0,	0, 2104, 1560, 	 0, 0, 2104, 1560}, // Preview
	{ 4208, 3120,	  0,  	0, 4208, 3120, 4208, 3120,   0,	0, 4208, 3120, 	 0, 0, 4208, 3120}, // capture
	{ 4208, 3120,	  0,  	0, 4208, 3120, 4208, 3120,   0,	0, 4208, 3120, 	 0, 0, 4208, 3120}, // video
	{ 4208, 3120,	824,  600, 2560, 1920, 1280,  720,   0,	0, 1280,  720, 	 0, 0, 1280,  720}, //hight speed video
	{ 4208, 3120,	184,  480, 3840, 2160, 1920, 1080,   0,	0, 1920, 1080, 	 0, 0, 1920, 1080},// slim video
	{ 4208, 3120,	  0,    0, 4208, 3120, 4208, 3120,   0,	0, 4208, 3120, 	 0, 0, 4208, 3120}, // custom1

};

#ifdef ENABLE_PDAF
/*PD information update*/
static struct SET_PD_BLOCK_INFO_T imgsensor_pd_info = {
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

#if MULTI_WRITE
#define I2C_BUFFER_LEN 225
#else
#define I2C_BUFFER_LEN 3
#endif

static kal_uint16 sc1300cs_table_write_cmos_sensor(
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
			puSendCmd[tosend++] = (char)(data & 0xFF);
			IDX += 2;
			addr_last = addr;

		}
#if MULTI_WRITE
		if ((I2C_BUFFER_LEN - tosend) < 3 ||
			len == IDX ||
			addr != addr_last) {
			iBurstWriteReg_multi(puSendCmd, tosend,
				imgsensor.i2c_write_id,
				3, imgsensor_info.i2c_speed);

			tosend = 0;
		}
#else
		iWriteRegI2C(puSendCmd, 3, imgsensor.i2c_write_id);
		tosend = 0;

#endif
	}
	return 0;
}

static kal_uint16 read_cmos_sensor(kal_uint32 addr)
{
	kal_uint16 get_byte = 0;
	char pusendcmd[2] = {(char)(addr >> 8), (char)(addr & 0xFF) };

	iReadRegI2C(pusendcmd, 2, (u8 *)&get_byte, 1, imgsensor.i2c_write_id);

	return get_byte;
}

static void write_cmos_sensor(kal_uint32 addr, kal_uint32 para)
{
	char pusendcmd[4] = {(char)(addr >> 8),
		(char)(addr & 0xFF), (char)(para & 0xFF)};

	iWriteRegI2C(pusendcmd, 3, imgsensor.i2c_write_id);
}

static void set_dummy(void)
{
	write_cmos_sensor(0x320c, imgsensor.line_length >> 8);
	write_cmos_sensor(0x320d, imgsensor.line_length & 0xFF);
	write_cmos_sensor(0x320e, (imgsensor.frame_length >> 8) & 0x7f);
	write_cmos_sensor(0x320f, imgsensor.frame_length & 0xFF);
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
}

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

	/* frame_length and shutter should be an even number. */
	shutter = (shutter >> 1) << 1;
	imgsensor.frame_length = (imgsensor.frame_length >> 1) << 1;

	if (imgsensor.autoflicker_en == KAL_TRUE) {
		realtime_fps = imgsensor.pclk / imgsensor.line_length * 10 /
			imgsensor.frame_length;
		if (realtime_fps >= 297 && realtime_fps <= 305) {
			realtime_fps = 296;
			set_max_framerate(realtime_fps, 0);
		} else if (realtime_fps >= 147 && realtime_fps <= 150) {
			realtime_fps = 146;
			set_max_framerate(realtime_fps, 0);
		} else	{
			/* Extend frame length */
			write_cmos_sensor(0x320e,
				(imgsensor.frame_length >> 8) & 0x7f);
			write_cmos_sensor(0x320f,
				imgsensor.frame_length & 0xFF);
		}
	} else	{
		write_cmos_sensor(0x320e, (imgsensor.frame_length >> 8) & 0x7f);
		write_cmos_sensor(0x320f, imgsensor.frame_length & 0xFF);
	}

	/* Update Shutter */
    shutter = shutter *2;
	write_cmos_sensor(0x3e20, (shutter >> 20) & 0x0F);
	write_cmos_sensor(0x3e00, (shutter >> 12) & 0xFF);
	write_cmos_sensor(0x3e01, (shutter >>  4) & 0xFF);
	write_cmos_sensor(0x3e02, (shutter <<  4) & 0xF0);

	cam_pr_debug("shutter =%d, framelength =%d, realtime_fps =%d\n",
		shutter, imgsensor.frame_length, realtime_fps);
}

static void set_shutter(kal_uint32 shutter)
{
	unsigned long flags;

	spin_lock_irqsave(&imgsensor_drv_lock, flags);
	imgsensor.shutter = shutter;
	spin_unlock_irqrestore(&imgsensor_drv_lock, flags);
	write_shutter(shutter);
}

static void set_shutter_frame_length(
				kal_uint16 shutter, kal_uint16 frame_length)
{
	unsigned long flags;
	kal_uint16 realtime_fps = 0;
	kal_int32 dummy_line = 0;
	spin_lock_irqsave(&imgsensor_drv_lock, flags);
	imgsensor.shutter = shutter;
	spin_unlock_irqrestore(&imgsensor_drv_lock, flags);
	pr_err("sc1300cs %s %d\n", __func__, __LINE__);

	spin_lock(&imgsensor_drv_lock);
	if (frame_length > 1)
		dummy_line = frame_length - imgsensor.frame_length;
	imgsensor.frame_length = imgsensor.frame_length + dummy_line;

	if (shutter > imgsensor.frame_length - imgsensor_info.margin)
		imgsensor.frame_length = shutter + imgsensor_info.margin;

	if (imgsensor.frame_length > imgsensor_info.max_frame_length)
		imgsensor.frame_length = imgsensor_info.max_frame_length;
	spin_unlock(&imgsensor_drv_lock);

	shutter = (shutter < imgsensor_info.min_shutter) ? imgsensor_info.min_shutter : shutter;

	shutter = (shutter > (imgsensor_info.max_frame_length - imgsensor_info.margin))
	? (imgsensor_info.max_frame_length - imgsensor_info.margin) : shutter;

	if (imgsensor.autoflicker_en) {
		realtime_fps = imgsensor.pclk
			/ imgsensor.line_length * 10 / imgsensor.frame_length;

		if (realtime_fps >= 297 && realtime_fps <= 305)
			set_max_framerate(296, 0);
		else if (realtime_fps >= 147 && realtime_fps <= 150)
			set_max_framerate(146, 0);
		else {
			write_cmos_sensor(0x320e,(imgsensor.frame_length >> 8) & 0x7f);
			write_cmos_sensor(0x320f,imgsensor.frame_length & 0xFF);
		}
	} else {
			write_cmos_sensor(0x320e,(imgsensor.frame_length >> 8) & 0x7f);
			write_cmos_sensor(0x320f,imgsensor.frame_length & 0xFF);
	}

	/* Update Shutter */
    shutter = shutter *2;
	write_cmos_sensor(0x3e20, (shutter >> 20) & 0x0F);
	write_cmos_sensor(0x3e00, (shutter >> 12) & 0xFF);
	write_cmos_sensor(0x3e01, (shutter >>  4) & 0xFF);
	write_cmos_sensor(0x3e02, (shutter <<  4) & 0xF0);

	pr_debug("Exit! shutter =%d, framelength =%d/%d, dummy_line=%d, auto_extend=%d\n",
		shutter, imgsensor.frame_length,
		frame_length, dummy_line);

}

static kal_uint16 gain2reg(const kal_uint16 gain)
{
	kal_uint16 reg_gain = gain << 4;

	if (reg_gain < SC1300CS_SENSOR_GAIN_BASE)
		reg_gain = SC1300CS_SENSOR_GAIN_BASE;
	else if (reg_gain > SC1300CS_SENSOR_GAIN_MAX)
		reg_gain = SC1300CS_SENSOR_GAIN_MAX;

	return (kal_uint16) reg_gain;
}

static kal_uint16 set_gain(kal_uint16 gain)
{
	kal_uint16 reg_gain;
	kal_uint32 temp_gain;
	kal_int16 gain_index;
	unsigned long flags;
	kal_uint16 SC1300CS_AGC_Param[SC1300CS_SENSOR_GAIN_MAP_SIZE][2] = {
		{  1024,  0x00 },
		{  2048,  0x01 },
		{  4096,  0x03 },
		{  8192,  0x07 },
		{ 16384,  0x0f },
		{ 32768,  0x1f },
	};

	reg_gain = gain2reg(gain);
	for (gain_index = SC1300CS_SENSOR_GAIN_MAP_SIZE - 1; gain_index > 0; gain_index--){
		if (reg_gain >= SC1300CS_AGC_Param[gain_index][0]) {
			break;
		}
	}
	spin_lock_irqsave(&imgsensor_drv_lock, flags);
	imgsensor.gain = reg_gain;
	spin_unlock_irqrestore(&imgsensor_drv_lock, flags);

	cam_pr_debug("gain = %d , reg_gain = 0x%x\n ", gain, reg_gain);
	write_cmos_sensor(0x3e09, SC1300CS_AGC_Param[gain_index][1]);
	temp_gain = reg_gain * SC1300CS_SENSOR_GAIN_BASE / SC1300CS_AGC_Param[gain_index][0];
	write_cmos_sensor(0x3e07, (temp_gain >> 3) & 0xff);

	return gain;
}

static void ihdr_write_shutter_gain(kal_uint16 le,
				kal_uint16 se, kal_uint16 gain)
{
}

static void night_mode(kal_bool enable)
{
}

#if MULTI_WRITE
kal_uint16 addr_data_pair_init_sc1300cs_hlt[] = {

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
#endif

static void sensor_init(void)
{
	write_cmos_sensor(0x0103, 0x01);
	mdelay(10);
#if MULTI_WRITE
	cam_pr_debug("MULTI_WRITE\n");
	sc1300cs_table_write_cmos_sensor(
		addr_data_pair_init_sc1300cs_hlt,
		sizeof(addr_data_pair_init_sc1300cs_hlt) / sizeof(kal_uint16));
#else
	cam_pr_debug("E\n");

	write_cmos_sensor(0x0100,0x00);
	write_cmos_sensor(0x36e9,0x80);
	write_cmos_sensor(0x36f9,0x80);
	write_cmos_sensor(0x36ea,0x05);
	write_cmos_sensor(0x36eb,0x0a);
	write_cmos_sensor(0x36ec,0x0b);
	write_cmos_sensor(0x36ed,0x24);
	write_cmos_sensor(0x36fa,0x06);
	write_cmos_sensor(0x36fd,0x24);
	write_cmos_sensor(0x36f9,0x04);
	write_cmos_sensor(0x36e9,0x04);
	write_cmos_sensor(0x301f,0x01);
	write_cmos_sensor(0x302d,0x20);
	write_cmos_sensor(0x3106,0x01);
	write_cmos_sensor(0x325f,0x42);
	write_cmos_sensor(0x3301,0x08);
	write_cmos_sensor(0x3306,0x28);
	write_cmos_sensor(0x330b,0x88);
	write_cmos_sensor(0x330e,0x40);
	write_cmos_sensor(0x3314,0x15);
	write_cmos_sensor(0x331e,0x45);
	write_cmos_sensor(0x331f,0x65);
	write_cmos_sensor(0x3333,0x10);
	write_cmos_sensor(0x3347,0x07);
	write_cmos_sensor(0x335d,0x60);
	write_cmos_sensor(0x3364,0x57);
	write_cmos_sensor(0x3367,0x04);
	write_cmos_sensor(0x3390,0x01);
	write_cmos_sensor(0x3391,0x03);
	write_cmos_sensor(0x3392,0x07);
	write_cmos_sensor(0x3393,0x07);
	write_cmos_sensor(0x3394,0x0e);
	write_cmos_sensor(0x3395,0x15);
	write_cmos_sensor(0x33ad,0x30);
	write_cmos_sensor(0x33b1,0x80);
	write_cmos_sensor(0x33b3,0x44);
	write_cmos_sensor(0x341e,0x00);
	write_cmos_sensor(0x34a8,0x40);
	write_cmos_sensor(0x34a9,0x28);
	write_cmos_sensor(0x34ab,0x84);
	write_cmos_sensor(0x34ad,0x84);
	write_cmos_sensor(0x360f,0x01);
	write_cmos_sensor(0x3621,0x68);
	write_cmos_sensor(0x3622,0xe8);
	write_cmos_sensor(0x3635,0x20);
	write_cmos_sensor(0x3637,0x26);
	write_cmos_sensor(0x3638,0xc7);
	write_cmos_sensor(0x3639,0xf4);
	write_cmos_sensor(0x363a,0x82);
	write_cmos_sensor(0x363c,0xc0);
	write_cmos_sensor(0x363d,0x40);
	write_cmos_sensor(0x363e,0x28);
	write_cmos_sensor(0x3670,0x4a);
	write_cmos_sensor(0x3671,0xe5);
	write_cmos_sensor(0x3672,0x83);
	write_cmos_sensor(0x3673,0x83);
	write_cmos_sensor(0x3674,0x10);
	write_cmos_sensor(0x3675,0x36);
	write_cmos_sensor(0x3676,0x3a);
	write_cmos_sensor(0x367a,0x03);
	write_cmos_sensor(0x367b,0x0f);
	write_cmos_sensor(0x367c,0x03);
	write_cmos_sensor(0x367d,0x07);
	write_cmos_sensor(0x3690,0x55);
	write_cmos_sensor(0x3691,0x65);
	write_cmos_sensor(0x3692,0x65);
	write_cmos_sensor(0x3699,0x82);
	write_cmos_sensor(0x369a,0x9f);
	write_cmos_sensor(0x369b,0xc0);
	write_cmos_sensor(0x369c,0x03);
	write_cmos_sensor(0x369d,0x07);
	write_cmos_sensor(0x36a2,0x03);
	write_cmos_sensor(0x36a3,0x07);
	write_cmos_sensor(0x36b1,0xd8);
	write_cmos_sensor(0x36b2,0x01);
	write_cmos_sensor(0x3900,0x1d);
	write_cmos_sensor(0x3902,0xc5);
	write_cmos_sensor(0x391b,0x81);
	write_cmos_sensor(0x391c,0x30);
	write_cmos_sensor(0x391d,0x19);
	write_cmos_sensor(0x3948,0x06);
	write_cmos_sensor(0x394a,0x08);
	write_cmos_sensor(0x3952,0x02);
	write_cmos_sensor(0x396a,0x07);
	write_cmos_sensor(0x396c,0x0e);
	write_cmos_sensor(0x3e00,0x01);
	write_cmos_sensor(0x3e01,0x8f);
	write_cmos_sensor(0x3e02,0x60);
	write_cmos_sensor(0x3e20,0x00);
	write_cmos_sensor(0x4000,0x00);
	write_cmos_sensor(0x4001,0x05);
	write_cmos_sensor(0x4002,0x00);
	write_cmos_sensor(0x4003,0x00);
	write_cmos_sensor(0x4004,0x00);
	write_cmos_sensor(0x4005,0x00);
	write_cmos_sensor(0x4006,0x08);
	write_cmos_sensor(0x4007,0x31);
	write_cmos_sensor(0x4008,0x00);
	write_cmos_sensor(0x4009,0xc8);
	write_cmos_sensor(0x4424,0x02);
	write_cmos_sensor(0x4506,0x3c);
	write_cmos_sensor(0x4509,0x30);
	write_cmos_sensor(0x5000,0x0e);
	write_cmos_sensor(0x5015,0x41);
	write_cmos_sensor(0x5016,0x88);
	write_cmos_sensor(0x5017,0x22);
	write_cmos_sensor(0x5018,0x00);
	write_cmos_sensor(0x5019,0x41);
	write_cmos_sensor(0x501a,0x10);
	write_cmos_sensor(0x501b,0x41);
	write_cmos_sensor(0x501c,0x00);
	write_cmos_sensor(0x501d,0x41);
	write_cmos_sensor(0x501e,0x0c);
	write_cmos_sensor(0x501f,0x01);
	write_cmos_sensor(0x5799,0x06);
	write_cmos_sensor(0x59e0,0xd8);
	write_cmos_sensor(0x59e1,0x08);
	write_cmos_sensor(0x59e2,0x10);
	write_cmos_sensor(0x59e3,0x08);
	write_cmos_sensor(0x59e4,0x00);
	write_cmos_sensor(0x59e5,0x10);
	write_cmos_sensor(0x59e6,0x08);
	write_cmos_sensor(0x59e7,0x00);
	write_cmos_sensor(0x59e8,0x18);
	write_cmos_sensor(0x59e9,0x0c);
	write_cmos_sensor(0x59ea,0x04);
	write_cmos_sensor(0x59eb,0x18);
	write_cmos_sensor(0x59ec,0x0c);
	write_cmos_sensor(0x59ed,0x04);
#endif
}

#if MULTI_WRITE
kal_uint16 addr_data_pair_preview_sc1300cs_hlt[] = {
	0x0100,0x00,
	0x3220,0x01,
	0x3200,0x00,
	0x3201,0x00,
	0x3202,0x00,
	0x3203,0x00,
	0x3204,0x10,
	0x3205,0x7f,
	0x3206,0x0c,
	0x3207,0x3f,
	0x3208,0x08,
	0x3209,0x38,
	0x320a,0x06,
	0x320b,0x18,
	0x320c,0x04,
	0x320d,0xe2,
	0x320e,0x06,
	0x320f,0x40,
	0x3210,0x00,
	0x3211,0x04,
	0x3212,0x00,
	0x3213,0x04,
	0x3214,0x11,
	0x3215,0x31,
	0x5000,0x4e,
	0x5901,0x04,
};
#endif

static void preview_setting(void)
{
	cam_pr_debug("E RES_2112x1568_30fps\n");
#if MULTI_WRITE
	sc1300cs_table_write_cmos_sensor(
		addr_data_pair_preview_sc1300cs_hlt,
		sizeof(addr_data_pair_preview_sc1300cs_hlt) / sizeof(kal_uint16));
#else
	write_cmos_sensor(0x0100,0x00);
	write_cmos_sensor(0x3220,0x01);
	write_cmos_sensor(0x3200,0x00);
	write_cmos_sensor(0x3201,0x00);
	write_cmos_sensor(0x3202,0x00);
	write_cmos_sensor(0x3203,0x00);
	write_cmos_sensor(0x3204,0x10);
	write_cmos_sensor(0x3205,0x7f);
	write_cmos_sensor(0x3206,0x0c);
	write_cmos_sensor(0x3207,0x3f);
	write_cmos_sensor(0x3208,0x08);
	write_cmos_sensor(0x3209,0x38);
	write_cmos_sensor(0x320a,0x06);
	write_cmos_sensor(0x320b,0x18);
	write_cmos_sensor(0x320c,0x04);
	write_cmos_sensor(0x320d,0xe2);
	write_cmos_sensor(0x320e,0x06);
	write_cmos_sensor(0x320f,0x40);
	write_cmos_sensor(0x3210,0x00);
	write_cmos_sensor(0x3211,0x04);
	write_cmos_sensor(0x3212,0x00);
	write_cmos_sensor(0x3213,0x04);
	write_cmos_sensor(0x3214,0x11);
	write_cmos_sensor(0x3215,0x31);
	write_cmos_sensor(0x5000,0x4e);
	write_cmos_sensor(0x5901,0x04);
#endif
}

#if MULTI_WRITE
kal_uint16 addr_data_pair_capture_15fps_sc1300cs_hlt[] = {
	0x0100,0x00,
	0x3220,0x00,
	0x3200,0x00,
	0x3201,0x00,
	0x3202,0x00,
	0x3203,0x00,
	0x3204,0x10,
	0x3205,0x7f,
	0x3206,0x0c,
	0x3207,0x3f,
	0x3208,0x10,
	0x3209,0x70,
	0x320a,0x0c,
	0x320b,0x30,
	0x320c,0x04,
	0x320d,0xe2,
	0x320e,0x0c,
	0x320f,0x80,
	0x3210,0x00,
	0x3211,0x08,
	0x3212,0x00,
	0x3213,0x08,
	0x3214,0x11,
	0x3215,0x11,
	0x5000,0x0e,
	0x5901,0x00,
	0x320e,0x0c, //.framelength =3200,//30fpsset FOR 1300CS
	0x320f,0x80, //.framelength =3200,//30fpsset FOR 1300CS

};

kal_uint16 addr_data_pair_capture_30fps_sc1300cs_hlt[] = {
	0x0100,0x00,
	0x3220,0x00,
	0x3200,0x00,
	0x3201,0x00,
	0x3202,0x00,
	0x3203,0x00,
	0x3204,0x10,
	0x3205,0x7f,
	0x3206,0x0c,
	0x3207,0x3f,
	0x3208,0x10,
	0x3209,0x70,
	0x320a,0x0c,
	0x320b,0x30,
	0x320c,0x04,
	0x320d,0xe2,
	0x320e,0x0c,
	0x320f,0x80,
	0x3210,0x00,
	0x3211,0x08,
	0x3212,0x00,
	0x3213,0x08,
	0x3214,0x11,
	0x3215,0x11,
	0x5000,0x0e,
	0x5901,0x00,
	0x320e,0x0c, //.framelength =3200,//30fpsset FOR 1300CS
	0x320f,0x80, //.framelength =3200,//30fpsset FOR 1300CS
};
#endif

static void capture_setting(kal_uint16 currefps)
{

	cam_pr_debug("E 4224x3136_zsl_30fps currefps = %d\n",
		currefps);
#if MULTI_WRITE
	if (currefps == 150) {
		sc1300cs_table_write_cmos_sensor(
			addr_data_pair_capture_15fps_sc1300cs_hlt,
			sizeof(addr_data_pair_capture_15fps_sc1300cs_hlt) /
			sizeof(kal_uint16));
	} else {
		sc1300cs_table_write_cmos_sensor(
			addr_data_pair_capture_30fps_sc1300cs_hlt,
			sizeof(addr_data_pair_capture_30fps_sc1300cs_hlt) /
			sizeof(kal_uint16));
	}
#else
	write_cmos_sensor(0x0100,0x00);
	write_cmos_sensor(0x3220,0x00);
	write_cmos_sensor(0x3200,0x00);
	write_cmos_sensor(0x3201,0x00);
	write_cmos_sensor(0x3202,0x00);
	write_cmos_sensor(0x3203,0x00);
	write_cmos_sensor(0x3204,0x10);
	write_cmos_sensor(0x3205,0x7f);
	write_cmos_sensor(0x3206,0x0c);
	write_cmos_sensor(0x3207,0x3f);
	write_cmos_sensor(0x3208,0x10);
	write_cmos_sensor(0x3209,0x70);
	write_cmos_sensor(0x320a,0x0c);
	write_cmos_sensor(0x320b,0x30);
	write_cmos_sensor(0x320c,0x04);
	write_cmos_sensor(0x320d,0xe2);
	write_cmos_sensor(0x320e,0x0c);
	write_cmos_sensor(0x320f,0x80);
	write_cmos_sensor(0x3210,0x00);
	write_cmos_sensor(0x3211,0x08);
	write_cmos_sensor(0x3212,0x00);
	write_cmos_sensor(0x3213,0x08);
	write_cmos_sensor(0x3214,0x11);
	write_cmos_sensor(0x3215,0x11);
	write_cmos_sensor(0x5000,0x0e);
	write_cmos_sensor(0x5901,0x00);
	write_cmos_sensor(0x320e,0x0c);
	write_cmos_sensor(0x320f,0x80);
#endif
}

#if MULTI_WRITE
kal_uint16 addr_data_pair_video_sc1300cs_dd[] = {
	0x0100,0x00,
	0x3220,0x00,
	0x3200,0x00,
	0x3201,0x00,
	0x3202,0x00,
	0x3203,0x00,
	0x3204,0x10,
	0x3205,0x7f,
	0x3206,0x0c,
	0x3207,0x3f,
	0x3208,0x10,
	0x3209,0x70,
	0x320a,0x0c,
	0x320b,0x30,
	0x320c,0x04,
	0x320d,0xe2,
	0x320e,0x0c,
	0x320f,0x80,
	0x3210,0x00,
	0x3211,0x08,
	0x3212,0x00,
	0x3213,0x08,
	0x3214,0x11,
	0x3215,0x11,
	0x5000,0x0e,
	0x5901,0x00,
};
#endif

static void normal_video_setting(kal_uint16 currefps)
{
	cam_pr_debug("E RES_4224x3136_zsl_30fps\n");
#if MULTI_WRITE
	sc1300cs_table_write_cmos_sensor(
		addr_data_pair_video_sc1300cs_dd,
		sizeof(addr_data_pair_video_sc1300cs_dd) /
		sizeof(kal_uint16));
#else
	write_cmos_sensor(0x0100,0x00);
	write_cmos_sensor(0x3220,0x00);
	write_cmos_sensor(0x3200,0x00);
	write_cmos_sensor(0x3201,0x00);
	write_cmos_sensor(0x3202,0x00);
	write_cmos_sensor(0x3203,0x00);
	write_cmos_sensor(0x3204,0x10);
	write_cmos_sensor(0x3205,0x7f);
	write_cmos_sensor(0x3206,0x0c);
	write_cmos_sensor(0x3207,0x3f);
	write_cmos_sensor(0x3208,0x10);
	write_cmos_sensor(0x3209,0x70);
	write_cmos_sensor(0x320a,0x0c);
	write_cmos_sensor(0x320b,0x30);
	write_cmos_sensor(0x320c,0x04);
	write_cmos_sensor(0x320d,0xe2);
	write_cmos_sensor(0x320e,0x0c);
	write_cmos_sensor(0x320f,0x80);
	write_cmos_sensor(0x3210,0x00);
	write_cmos_sensor(0x3211,0x08);
	write_cmos_sensor(0x3212,0x00);
	write_cmos_sensor(0x3213,0x08);
	write_cmos_sensor(0x3214,0x11);
	write_cmos_sensor(0x3215,0x11);
	write_cmos_sensor(0x5000,0x0e);
	write_cmos_sensor(0x5901,0x00);
#endif
}

#if MULTI_WRITE
kal_uint16 addr_data_pair_hs_video_sc1300cs_hlt[] = {
	0x0100,0x00,
	0x3220,0x01,
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
	0x320c,0x04,
	0x320d,0xe2,
	0x320e,0x03,
	0x320f,0x20,
	0x3210,0x00,
	0x3211,0x04,
	0x3212,0x00,
	0x3213,0x04,
	0x3214,0x11,
	0x3215,0x31,
	0x5000,0x4e,
	0x5901,0x04,

};
#endif

static void hs_video_setting(void)
{
	cam_pr_debug("E RES_640x480_120fps\n");
#if MULTI_WRITE
	sc1300cs_table_write_cmos_sensor(
		addr_data_pair_hs_video_sc1300cs_hlt,
		sizeof(addr_data_pair_hs_video_sc1300cs_hlt) /
		sizeof(kal_uint16));
#else
	write_cmos_sensor(0x0100,0x00);
	write_cmos_sensor(0x3220,0x01);
	write_cmos_sensor(0x3200,0x03);
	write_cmos_sensor(0x3201,0x38);
	write_cmos_sensor(0x3202,0x03);
	write_cmos_sensor(0x3203,0x48);
	write_cmos_sensor(0x3204,0x0d);
	write_cmos_sensor(0x3205,0x47);
	write_cmos_sensor(0x3206,0x08);
	write_cmos_sensor(0x3207,0xf7);
	write_cmos_sensor(0x3208,0x05);
	write_cmos_sensor(0x3209,0x00);
	write_cmos_sensor(0x320a,0x02);
	write_cmos_sensor(0x320b,0xd0);
	write_cmos_sensor(0x320c,0x04);
	write_cmos_sensor(0x320d,0xe2);
	write_cmos_sensor(0x320e,0x03);
	write_cmos_sensor(0x320f,0x20);
	write_cmos_sensor(0x3210,0x00);
	write_cmos_sensor(0x3211,0x04);
	write_cmos_sensor(0x3212,0x00);
	write_cmos_sensor(0x3213,0x04);
	write_cmos_sensor(0x3214,0x11);
	write_cmos_sensor(0x3215,0x31);
	write_cmos_sensor(0x5000,0x4e);
	write_cmos_sensor(0x5901,0x04);
#endif
}

#if MULTI_WRITE
kal_uint16 addr_data_pair_slim_video_sc1300cs_hlt[] = {
	0x0100,0x00,
	0x3220,0x01,
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
	0x320c,0x04,
	0x320d,0xe2,
	0x320e,0x06,
	0x320f,0x40,
	0x3210,0x00,
	0x3211,0x04,
	0x3212,0x00,
	0x3213,0x04,
	0x3214,0x11,
	0x3215,0x31,
	0x5000,0x4e,
	0x5901,0x04,

};
#endif

static void slim_video_setting(void)
{
	cam_pr_debug("E\n");
#if MULTI_WRITE
	sc1300cs_table_write_cmos_sensor(
		addr_data_pair_slim_video_sc1300cs_hlt,
		sizeof(addr_data_pair_slim_video_sc1300cs_hlt) /
		sizeof(kal_uint16));
#else
	write_cmos_sensor(0x0100,0x00);
	write_cmos_sensor(0x3220,0x01);
	write_cmos_sensor(0x3200,0x00);
	write_cmos_sensor(0x3201,0xb8);
	write_cmos_sensor(0x3202,0x01);
	write_cmos_sensor(0x3203,0xe0);
	write_cmos_sensor(0x3204,0x0f);
	write_cmos_sensor(0x3205,0xc7);
	write_cmos_sensor(0x3206,0x0a);
	write_cmos_sensor(0x3207,0x5f);
	write_cmos_sensor(0x3208,0x07);
	write_cmos_sensor(0x3209,0x80);
	write_cmos_sensor(0x320a,0x04);
	write_cmos_sensor(0x320b,0x38);
	write_cmos_sensor(0x320c,0x04);
	write_cmos_sensor(0x320d,0xe2);
	write_cmos_sensor(0x320e,0x06);
	write_cmos_sensor(0x320f,0x40);
	write_cmos_sensor(0x3210,0x00);
	write_cmos_sensor(0x3211,0x04);
	write_cmos_sensor(0x3212,0x00);
	write_cmos_sensor(0x3213,0x04);
	write_cmos_sensor(0x3214,0x11);
	write_cmos_sensor(0x3215,0x31);
	write_cmos_sensor(0x5000,0x4e);
	write_cmos_sensor(0x5901,0x04);
#endif
}

#if MULTI_WRITE
kal_uint16 addr_data_pair_custom1_sc1300cs_hlt[] = {
	0x0100,0x00,
	0x3220,0x00,
	0x3200,0x00,
	0x3201,0x00,
	0x3202,0x00,
	0x3203,0x00,
	0x3204,0x10,
	0x3205,0x7f,
	0x3206,0x0c,
	0x3207,0x3f,
	0x3208,0x10,
	0x3209,0x70,
	0x320a,0x0c,
	0x320b,0x30,
	0x320c,0x04,
	0x320d,0xe2,
	0x320e,0x0c,
	0x320f,0x80,
	0x3210,0x00,
	0x3211,0x08,
	0x3212,0x00,
	0x3213,0x08,
	0x3214,0x11,
	0x3215,0x11,
	0x5000,0x0e,
	0x5901,0x00,
	0x320e,0x0F, //.framelength =4000,//24fpsset FOR 1300CS
	0x320f,0xA0, //.framelength =4000,//24fpsset FOR 1300CS
};
#endif

static void custom1_setting(void)
{
#if MULTI_WRITE
	sc1300cs_table_write_cmos_sensor(
		addr_data_pair_custom1_sc1300cs_hlt,
		sizeof(addr_data_pair_custom1_sc1300cs_hlt) /
		sizeof(kal_uint16));
#else
	write_cmos_sensor(0x0100,0x00);
	write_cmos_sensor(0x3220,0x00);
	write_cmos_sensor(0x3200,0x00);
	write_cmos_sensor(0x3201,0x00);
	write_cmos_sensor(0x3202,0x00);
	write_cmos_sensor(0x3203,0x00);
	write_cmos_sensor(0x3204,0x10);
	write_cmos_sensor(0x3205,0x7f);
	write_cmos_sensor(0x3206,0x0c);
	write_cmos_sensor(0x3207,0x3f);
	write_cmos_sensor(0x3208,0x10);
	write_cmos_sensor(0x3209,0x70);
	write_cmos_sensor(0x320a,0x0c);
	write_cmos_sensor(0x320b,0x30);
	write_cmos_sensor(0x320c,0x04);
	write_cmos_sensor(0x320d,0xe2);
	write_cmos_sensor(0x320e,0x0c);
	write_cmos_sensor(0x320f,0x80);
	write_cmos_sensor(0x3210,0x00);
	write_cmos_sensor(0x3211,0x08);
	write_cmos_sensor(0x3212,0x00);
	write_cmos_sensor(0x3213,0x08);
	write_cmos_sensor(0x3214,0x11);
	write_cmos_sensor(0x3215,0x11);
	write_cmos_sensor(0x5000,0x0e);
	write_cmos_sensor(0x5901,0x00);
	write_cmos_sensor(0x320e,0x0f);
	write_cmos_sensor(0x320f,0xa0);
#endif
}

static kal_uint32 return_sensor_id(void)
{
	return ((read_cmos_sensor(0x3107) << 8) | read_cmos_sensor(0x3108))+1;
}

static kal_uint32 get_imgsensor_id(UINT32 *sensor_id)
{
	kal_uint8 i = 0;
	kal_uint8 retry = 2;

	while (imgsensor_info.i2c_addr_table[i] != 0xff) {
		spin_lock(&imgsensor_drv_lock);
		imgsensor.i2c_write_id = imgsensor_info.i2c_addr_table[i];
		spin_unlock(&imgsensor_drv_lock);
		do {
			*sensor_id = return_sensor_id();
			if (*sensor_id == imgsensor_info.sensor_id) {
				cam_pr_debug("i2c write id: 0x%x, sensor id: 0x%x\n",
				imgsensor.i2c_write_id, *sensor_id);
				return ERROR_NONE;
			}
			cam_pr_debug("Read sensor id fail, write id: 0x%x, id: 0x%x\n",
				imgsensor.i2c_write_id, *sensor_id);
			retry--;
		} while (retry > 0);
		i++;
		retry = 1;
	}
	if (*sensor_id != imgsensor_info.sensor_id) {
		cam_pr_debug("imgsensor id: 0x%x fail\n", *sensor_id);
		*sensor_id = 0xFFFFFFFF;
		return ERROR_SENSOR_CONNECT_FAIL;
	}

	return ERROR_NONE;
}

static kal_uint32 open(void)
{
	kal_uint8 i = 0;
	kal_uint8 retry = 1;
	kal_uint32 sensor_id = 0;

	while (imgsensor_info.i2c_addr_table[i] != 0xff) {
		spin_lock(&imgsensor_drv_lock);
		imgsensor.i2c_write_id = imgsensor_info.i2c_addr_table[i];
		spin_unlock(&imgsensor_drv_lock);
		do {
			sensor_id = return_sensor_id();
			if (sensor_id == imgsensor_info.sensor_id) {
				cam_pr_debug("i2c write id: 0x%x, sensor id: 0x%x\n",
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
		cam_pr_debug("Open sensor id: 0x%x fail\n", sensor_id);
		return ERROR_SENSOR_CONNECT_FAIL;
	}

	sensor_init();

	spin_lock(&imgsensor_drv_lock);
	imgsensor.autoflicker_en = KAL_FALSE;
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
	imgsensor.pdaf_mode = 0;
	spin_unlock(&imgsensor_drv_lock);

	return ERROR_NONE;
}

static kal_uint32 close(void)
{
	printk("[sc1300cs] close sensor\n");
	return ERROR_NONE;
}   /*  close  */

static kal_uint32 preview(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
		      MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_PREVIEW;
	imgsensor.pclk = imgsensor_info.pre.pclk;
	imgsensor.line_length = imgsensor_info.pre.linelength;
	imgsensor.frame_length = imgsensor_info.pre.framelength;
	imgsensor.min_frame_length = imgsensor_info.pre.framelength;
	imgsensor.current_fps = imgsensor.current_fps;
	spin_unlock(&imgsensor_drv_lock);
	preview_setting();
	return ERROR_NONE;
}

static kal_uint32 capture(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_CAPTURE;
	if (imgsensor.current_fps == imgsensor_info.cap1.max_framerate) {
		imgsensor.pclk = imgsensor_info.cap1.pclk;
		imgsensor.line_length = imgsensor_info.cap1.linelength;
		imgsensor.frame_length = imgsensor_info.cap1.framelength;
		imgsensor.min_frame_length = imgsensor_info.cap1.framelength;
	} else {
		if (imgsensor.current_fps != imgsensor_info.cap.max_framerate)
			cam_pr_debug("current_fps %d fps is not support,use cap1: %d fps!\n",
				imgsensor.current_fps,
				imgsensor_info.cap1.max_framerate/10);
		imgsensor.pclk = imgsensor_info.cap.pclk;
		imgsensor.line_length = imgsensor_info.cap.linelength;
		imgsensor.frame_length = imgsensor_info.cap.framelength;
		imgsensor.min_frame_length = imgsensor_info.cap.framelength;
	}

	spin_unlock(&imgsensor_drv_lock);

	capture_setting(imgsensor.current_fps);

	return ERROR_NONE;
}   /* capture() */

static kal_uint32 normal_video(
			MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_VIDEO;
	imgsensor.pclk = imgsensor_info.normal_video.pclk;
	imgsensor.line_length = imgsensor_info.normal_video.linelength;
	imgsensor.frame_length = imgsensor_info.normal_video.framelength;
	imgsensor.min_frame_length = imgsensor_info.normal_video.framelength;
	spin_unlock(&imgsensor_drv_lock);
	normal_video_setting(imgsensor.current_fps);
	return ERROR_NONE;
}

static kal_uint32 hs_video(
			MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_HIGH_SPEED_VIDEO;
	imgsensor.pclk = imgsensor_info.hs_video.pclk;
	imgsensor.line_length = imgsensor_info.hs_video.linelength;
	imgsensor.frame_length = imgsensor_info.hs_video.framelength;
	imgsensor.min_frame_length = imgsensor_info.hs_video.framelength;
	imgsensor.dummy_line = 0;
	imgsensor.dummy_pixel = 0;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	hs_video_setting();
	return ERROR_NONE;
}

static kal_uint32 slim_video(
			MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
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
}

static kal_uint32 custom1(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
       printk("[sc1300cs] custom1 mode start\n");
	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_CUSTOM1;
	imgsensor.pclk = imgsensor_info.custom1.pclk;
	imgsensor.line_length = imgsensor_info.custom1.linelength;
	imgsensor.frame_length = imgsensor_info.custom1.framelength;
	imgsensor.min_frame_length = imgsensor_info.custom1.framelength;
	imgsensor.current_fps = imgsensor_info.custom1.max_framerate;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	custom1_setting();
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
	sensor_resolution->SensorCustom1Width =
		imgsensor_info.custom1.grabwindow_width;
	sensor_resolution->SensorCustom1Height =
		imgsensor_info.custom1.grabwindow_height;

	return ERROR_NONE;
}   /*  get_resolution  */

static kal_uint32 get_info(enum MSDK_SCENARIO_ID_ENUM scenario_id,
		      MSDK_SENSOR_INFO_STRUCT *sensor_info,
		      MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	if (scenario_id == 0)
	cam_pr_debug("scenario_id = %d\n", scenario_id);

	sensor_info->SensorClockPolarity = SENSOR_CLOCK_POLARITY_LOW;
	sensor_info->SensorClockFallingPolarity = SENSOR_CLOCK_POLARITY_LOW;
	sensor_info->SensorHsyncPolarity = SENSOR_CLOCK_POLARITY_LOW;
	sensor_info->SensorVsyncPolarity = SENSOR_CLOCK_POLARITY_LOW;
	sensor_info->SensorInterruptDelayLines = 4; /* not use */
	sensor_info->SensorResetActiveHigh = FALSE; /* not use */
	sensor_info->SensorResetDelayCount = 5; /* not use */

	sensor_info->SensroInterfaceType = imgsensor_info.sensor_interface_type;
	sensor_info->MIPIsensorType = imgsensor_info.mipi_sensor_type;
	sensor_info->SettleDelayMode = imgsensor_info.mipi_settle_delay_mode;
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

	sensor_info->SensorMasterClockSwitch = 0; /* not use */
	sensor_info->SensorDrivingCurrent = imgsensor_info.isp_driving_current;
	/* The frame of setting shutter default 0 for TG int */
	sensor_info->AEShutDelayFrame = imgsensor_info.ae_shut_delay_frame;
	/* The frame of setting sensor gain */
	sensor_info->AESensorGainDelayFrame =
		imgsensor_info.ae_sensor_gain_delay_frame;
	sensor_info->AEISPGainDelayFrame =
		imgsensor_info.ae_ispGain_delay_frame;
	sensor_info->IHDR_Support = imgsensor_info.ihdr_support;
	sensor_info->IHDR_LE_FirstLine = imgsensor_info.ihdr_le_firstline;
	sensor_info->SensorModeNum = imgsensor_info.sensor_mode_num;
	/*0: NO PDAF, 1: PDAF Raw Data mode, 2:PDAF VC mode*/
	//sensor_info->PDAF_Support = 1;

	//sensor_info->HDR_Support = 0; /*0: NO HDR, 1: iHDR, 2:mvHDR, 3:zHDR*/
	sensor_info->SensorMIPILaneNumber = imgsensor_info.mipi_lane_num;
	sensor_info->SensorClockFreq = imgsensor_info.mclk;
	sensor_info->SensorClockDividCount = 3; /* not use */
	sensor_info->SensorClockRisingCount = 0;
	sensor_info->SensorClockFallingCount = 2; /* not use */
	sensor_info->SensorPixelClockCount = 3; /* not use */
	sensor_info->SensorDataLatchCount = 2; /* not use */

	sensor_info->MIPIDataLowPwr2HighSpeedTermDelayCount = 0;
	sensor_info->MIPICLKLowPwr2HighSpeedTermDelayCount = 0;
	sensor_info->SensorWidthSampling = 0;
	sensor_info->SensorHightSampling = 0;
	sensor_info->SensorPacketECCOrder = 1;

#ifdef  ENABLE_PDAF
	/*0: NO PDAF, 1: PDAF Raw Data mode, 2:PDAF VC mode*/
	sensor_info->PDAF_Support = PDAF_SUPPORT_RAW;
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
}   /*  get_info  */


static kal_uint32 control(enum MSDK_SCENARIO_ID_ENUM scenario_id,
			MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	spin_lock(&imgsensor_drv_lock);
	imgsensor.current_scenario_id = scenario_id;
	cam_pr_debug("scenario_id =%d\n",scenario_id);
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
			custom1(image_window, sensor_config_data);
			break;

		default:
			cam_pr_debug("Error ScenarioId setting");
			preview(image_window, sensor_config_data);
			return ERROR_INVALID_SCENARIO_ID;
	}
	return ERROR_NONE;
}   /* control() */

static kal_uint32 set_video_mode(UINT16 framerate)
{
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

	set_max_framerate(imgsensor.current_fps, 1);

	return ERROR_NONE;
}

static kal_uint32 set_auto_flicker_mode(kal_bool enable,
			UINT16 framerate)
{
	cam_pr_debug("enable = %d, framerate = %d\n",
		enable, framerate);

	spin_lock(&imgsensor_drv_lock);
	if (enable)
		imgsensor.autoflicker_en = KAL_TRUE;
	else
		imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);

	return ERROR_NONE;
}

static kal_uint32 set_max_framerate_by_scenario(
			enum MSDK_SCENARIO_ID_ENUM scenario_id,
			MUINT32 framerate)
{
	kal_uint32 frameHeight;

	cam_pr_debug("scenario_id = %d, framerate = %d\n",
			scenario_id, framerate);

	if (framerate == 0)
		return ERROR_NONE;

	switch (scenario_id) {
	case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		frameHeight = imgsensor_info.pre.pclk / framerate * 10 /
			imgsensor_info.pre.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line =
			(frameHeight > imgsensor_info.pre.framelength) ?
			(frameHeight - imgsensor_info.pre.framelength) : 0;
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
		frameHeight = imgsensor_info.normal_video.pclk /
			framerate * 10 /
			imgsensor_info.normal_video.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line = (frameHeight >
			imgsensor_info.normal_video.framelength) ?
			(frameHeight -
			 imgsensor_info.normal_video.framelength) : 0;
		imgsensor.frame_length =
			imgsensor_info.normal_video.framelength +
			imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
		break;
	case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		frameHeight = imgsensor_info.cap.pclk / framerate * 10 /
			imgsensor_info.cap.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line =
			(frameHeight > imgsensor_info.cap.framelength) ?
			(frameHeight - imgsensor_info.cap.framelength) : 0;
		imgsensor.frame_length = imgsensor_info.cap.framelength +
			imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
		break;
	case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
		frameHeight = imgsensor_info.hs_video.pclk / framerate * 10 /
			imgsensor_info.hs_video.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line =
			(frameHeight > imgsensor_info.hs_video.framelength) ?
			(frameHeight - imgsensor_info.hs_video.framelength) : 0;
		imgsensor.frame_length = imgsensor_info.hs_video.framelength +
			imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
		break;
	case MSDK_SCENARIO_ID_SLIM_VIDEO:
		frameHeight = imgsensor_info.slim_video.pclk / framerate * 10 /
			imgsensor_info.slim_video.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line = (frameHeight >
			imgsensor_info.slim_video.framelength) ?
			(frameHeight -
			imgsensor_info.slim_video.framelength) : 0;
		imgsensor.frame_length = imgsensor_info.slim_video.framelength +
			imgsensor.dummy_line;
	    imgsensor.min_frame_length = imgsensor.frame_length;
	    spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
	break;
	case MSDK_SCENARIO_ID_CUSTOM1:
		frameHeight = imgsensor_info.custom1.pclk / framerate * 10 / imgsensor_info.custom1.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line = (frameHeight > imgsensor_info.custom1.framelength) ?
			(frameHeight - imgsensor_info.custom1.framelength) : 0;
		imgsensor.frame_length = imgsensor_info.custom1.framelength + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
		break;
	default:
		frameHeight = imgsensor_info.pre.pclk / framerate * 10 /
			imgsensor_info.pre.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line = (frameHeight >
			imgsensor_info.pre.framelength) ?
			(frameHeight - imgsensor_info.pre.framelength) : 0;
		imgsensor.frame_length = imgsensor_info.pre.framelength +
			imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
		break;
	}
	return ERROR_NONE;
}

static kal_uint32 get_default_framerate_by_scenario(
			enum MSDK_SCENARIO_ID_ENUM scenario_id,
			MUINT32 *framerate)
{
	cam_pr_debug("[3058]scenario_id = %d\n", scenario_id);

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

	default:
		break;
	}

	return ERROR_NONE;
}

static kal_uint32 set_test_pattern_mode(kal_bool enable)
{
	// if (enable) {
	// 	//write_cmos_sensor(0x5000, 0x81);
	// //	write_cmos_sensor(0x5080, 0x80);
	// } else {
	// //	write_cmos_sensor(0x5000, 0xff);
	// //	write_cmos_sensor(0x5080, 0x00);
	// }

	spin_lock(&imgsensor_drv_lock);
	imgsensor.test_pattern = enable;
	spin_unlock(&imgsensor_drv_lock);

	return ERROR_NONE;
}

static kal_uint32 get_sensor_temperature(void)
{
	// UINT32 temperature = 0;
	// INT32 temperature_convert = 0;

	// TEMP_SEN_CTL
	// write_cmos_sensor(0x4d12, 0x01);
	// temperature = (read_cmos_sensor(0x4d13) << 8) |
	// 	read_cmos_sensor(0x4d13);
	// if (temperature < 0xc000)
	// 	temperature_convert = temperature / 256;
	// else
	// 	temperature_convert = 192 - temperature / 256;

	// if (temperature_convert > 192)
	// 	temperature_convert = 192;
	// else if (temperature_convert < -64)
	// 	temperature_convert = -64;

	return 0;
}

static kal_uint32 streaming_control(kal_bool enable)
{
	cam_pr_debug("streaming_enable(0=Sw Standby,1=streaming): %d\n", enable);
	if (enable) {
		write_cmos_sensor(0x0100, 0x01);
		write_cmos_sensor(0x302d, 0x00);
	}
	else {
		write_cmos_sensor(0x0100, 0x00);
	}

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
	INT32 *feature_return_para_i32 = (INT32 *) feature_para;

#ifdef  ENABLE_PDAF
	struct SET_PD_BLOCK_INFO_T *PDAFinfo;
#endif
	unsigned long long *feature_data = (unsigned long long *) feature_para;

	struct SENSOR_WINSIZE_INFO_STRUCT *wininfo;
	MSDK_SENSOR_REG_INFO_STRUCT *sensor_reg_data =
		(MSDK_SENSOR_REG_INFO_STRUCT *) feature_para;

        cam_pr_debug("xinj :%d; feature_id =%d\n", *feature_data_32,feature_id);
//	UINT32 fps = 0;

	switch (feature_id) {
	case SENSOR_FEATURE_GET_PIXEL_RATE:
		switch (*feature_data) {
			case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
				*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) = (imgsensor_info.cap.pclk /
				(imgsensor_info.cap.linelength - 80)) * imgsensor_info.cap.grabwindow_width;
				break;
			case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
				*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) = (imgsensor_info.normal_video.pclk /
				(imgsensor_info.normal_video.linelength - 80)) * imgsensor_info.normal_video.grabwindow_width;
				break;
			case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
				*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) = (imgsensor_info.hs_video.pclk /
				(imgsensor_info.hs_video.linelength - 80)) * imgsensor_info.hs_video.grabwindow_width;
				break;
			case MSDK_SCENARIO_ID_SLIM_VIDEO:
				*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) = (imgsensor_info.slim_video.pclk /
				(imgsensor_info.slim_video.linelength - 80)) * imgsensor_info.slim_video.grabwindow_width;
				break;
			case MSDK_SCENARIO_ID_CUSTOM1:
				*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) = (imgsensor_info.custom1.pclk /
				(imgsensor_info.custom1.linelength - 80)) * imgsensor_info.custom1.grabwindow_width;
				break;
			case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
			default:
				*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) = (imgsensor_info.pre.pclk /
				(imgsensor_info.pre.linelength - 80)) * imgsensor_info.pre.grabwindow_width;
				break;
			}
		break;
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
		cam_pr_debug("current fps :%d\n", imgsensor.current_fps);
		break;
	case SENSOR_FEATURE_GET_CROP_INFO:
		cam_pr_debug("GET_CROP_INFO scenarioId:%d\n",
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
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		default:
			memcpy((void *)wininfo,
				(void *)&imgsensor_winsize_info[0],
				sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		}
		break;
	case SENSOR_FEATURE_SET_IHDR_SHUTTER_GAIN:
		cam_pr_debug("SENSOR_SET_SENSOR_IHDR LE=%d, SE=%d, Gain=%d\n",
			(UINT16)*feature_data, (UINT16)*(feature_data+1),
			(UINT16)*(feature_data+2));
		ihdr_write_shutter_gain((UINT16)*feature_data,
			(UINT16)*(feature_data+1),
				(UINT16)*(feature_data+2));
		break;
	case SENSOR_FEATURE_GET_TEMPERATURE_VALUE:
		*feature_return_para_i32 = get_sensor_temperature();
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
#ifdef  ENABLE_PDAF
/*
		case SENSOR_FEATURE_GET_VC_INFO:
				cam_pr_debug("SENSOR_FEATURE_GET_VC_INFO %d\n", (UINT16)*feature_data);
				pvcinfo = (struct SENSOR_VC_INFO_STRUCT *)(uintptr_t)(*(feature_data+1));
				switch (*feature_data_32)
				{
					case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
					case MSDK_SCENARIO_ID_CUSTOM1:
						cam_pr_debug("SENSOR_FEATURE_GET_VC_INFO CAPTURE_JPEG\n");
						memcpy((void *)pvcinfo,(void *)&SENSOR_VC_INFO[1],sizeof(struct SENSOR_VC_INFO_STRUCT));
						break;
					case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
						cam_pr_debug("SENSOR_FEATURE_GET_VC_INFO VIDEO PREVIEW\n");
						memcpy((void *)pvcinfo,(void *)&SENSOR_VC_INFO[2],sizeof(struct SENSOR_VC_INFO_STRUCT));
						break;
					case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
					default:
						cam_pr_debug("SENSOR_FEATURE_GET_VC_INFO DEFAULT_PREVIEW\n");
						memcpy((void *)pvcinfo,(void *)&SENSOR_VC_INFO[0],sizeof(struct SENSOR_VC_INFO_STRUCT));
						break;
				}
				break;*/
	case SENSOR_FEATURE_GET_PDAF_DATA:
	        cam_pr_debug(" GET_PDAF_DATA EEPROM\n");
		break;
	case SENSOR_FEATURE_GET_PDAF_INFO:
		PDAFinfo = (struct SET_PD_BLOCK_INFO_T *)
			(uintptr_t)(*(feature_data+1));

		switch (*feature_data) {
			case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			case MSDK_SCENARIO_ID_CUSTOM1:
				memcpy((void *)PDAFinfo, (void *)&imgsensor_pd_info,
					sizeof(struct SET_PD_BLOCK_INFO_T));
				break;
			case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
			case MSDK_SCENARIO_ID_SLIM_VIDEO:
			case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
			default:
				break;
		}
		break;

	case SENSOR_FEATURE_GET_SENSOR_PDAF_CAPACITY:
		cam_pr_debug("SENSOR_FEATURE_GET_SENSOR_PDAF_CAPACITY scenarioId:%lld\n", *feature_data);
		//PDAF capacity enable or not, 2p8 only full size support PDAF
		switch (*feature_data) {
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			*(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 1;
			break;
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			*(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 1;
			break;
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
			*(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 0;
			break;
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
			*(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 0;
			break;
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
			*(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 0;
			break;
		case MSDK_SCENARIO_ID_CUSTOM1:
			*(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 1;
			break;
		default:
			*(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 0;
			break;
		}
		cam_pr_debug("SENSOR_FEATURE_GET_SENSOR_PDAF_CAPACITY end scenarioId:%lld\n", *feature_data);
		break;
	case SENSOR_FEATURE_SET_PDAF:
		imgsensor.pdaf_mode = *feature_data_16;
	        cam_pr_debug(" pdaf mode : %d \n", imgsensor.pdaf_mode);
		break;
#endif
/*	case SENSOR_FEATURE_GET_MIPI_PIXEL_RATE:
		fps = (MUINT32)(*(feature_data + 2));

		switch (*feature_data) {
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			if (fps == 150)
				*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
				imgsensor_info.cap1.mipi_pixel_rate;
			else
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

		break;*/
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

UINT32 O2101_SC1300CSLY_BACK_MIPI_RAW_SensorInit(struct SENSOR_FUNCTION_STRUCT **pfFunc)
{
	/* To Do : Check Sensor status here */
	if (pfFunc != NULL)
		*pfFunc =  &sensor_func;
	return ERROR_NONE;
}
/* hs04 code for DEAL6398A-1684 by chenjun at 2022/09/24 end */
