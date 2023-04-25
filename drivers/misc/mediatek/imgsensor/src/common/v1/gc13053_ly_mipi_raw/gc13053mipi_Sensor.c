 /*
 *
 * Filename:
 * ---------
 *     gc13053mipi_Sensor.c
 *
 * Project:
 * --------
 *     ALPS
 *
 * Description:
 * ------------
 *     Source code of Sensor driver
 *
 * Driver Version:
 * ------------
 *     V0.0001.120
 *
 *-----------------------------------------------------------------------------
 * Upper this line, this part is controlled by CC/CQ. DO NOT MODIFY!!
 *============================================================================
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

#include "kd_camera_typedef.h"
#include "kd_imgsensor.h"
#include "kd_imgsensor_define.h"
#include "kd_imgsensor_errcode.h"

#include "gc13053mipi_Sensor.h"

/************************** Modify Following Strings for Debug **************************/
#define PFX "GC13053MIPI_4Lane"
#define LOG_1 LOG_INF("GC13053MIPI, 4LANE\n")
/****************************   Modify end    *******************************************/
#define GC13053_DEBUG    0
#if GC13053_DEBUG
#define LOG_INF(format, args...)    pr_debug(PFX "[%s] " format, __func__, ##args)
#else
#define LOG_INF(format, args...)
#endif

static DEFINE_SPINLOCK(imgsensor_drv_lock);
/* A03s code for DEVAL5625-221 by xuxianwei at 2021/05/26 start */
/* A03s code for DEVAL5625-973 by xuxianwei at 2021/06/18 start */
static struct imgsensor_info_struct imgsensor_info = {
	.sensor_id = GC13053_LY_SENSOR_ID,
	.checksum_value = 0x1b375588,
	.pre = {
		.pclk = 121200000,
		.linelength = 2448,
		.framelength = 1612,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 2104,
		.grabwindow_height = 1560,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 236160000,
		.max_framerate = 300,
	},
	.cap = {
		.pclk = 489600000,
		.linelength = 5008,
		.framelength = 3224,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 4208,
		.grabwindow_height = 3120,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 472320000,
		.max_framerate = 300,
	},
	.normal_video = {
		.pclk = 489600000,
		.linelength = 5008,
		.framelength = 3224,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 4208,
		.grabwindow_height = 3120,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 472320000,
		.max_framerate = 300,
	},
	.hs_video = {
		.pclk = 489600000,
		.linelength = 5008,
		.framelength = 3224,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 4208,
		.grabwindow_height = 3120,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 472320000,
		.max_framerate = 300,
	},
	.slim_video = {
		.pclk = 121200000,
		.linelength = 2448,
		.framelength = 1612,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 2104,
		.grabwindow_height = 1560,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 236160000,
		.max_framerate = 300,
	},
/* HS03s code for DEVAL5625-990 by chenjun at 2021/05/28 start */
	.custom1 = {
		.pclk = 489600000,
		.linelength = 5008,
		.framelength = 3224,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 4208,
		.grabwindow_height = 3120,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 472320000,
		.max_framerate = 300,
	},
	.custom2 = {
		.pclk = 489600000,
		.linelength = 5008,
		.framelength = 4020,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 4208,
		.grabwindow_height = 3120,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 472320000,
		.max_framerate = 240,
	},

	.margin = 16,
	.min_shutter = 2,
	.max_frame_length = 0xfffe,
	.ae_shut_delay_frame = 0,
	.ae_sensor_gain_delay_frame = 0,
	.ae_ispGain_delay_frame = 2,
	.frame_time_delay_frame = 1,
	.ihdr_support = 0,
	.ihdr_le_firstline = 0,
	.sensor_mode_num = IMGSENSOR_MODE_NUM,

	.cap_delay_frame = 2,
	.pre_delay_frame = 2,
	.video_delay_frame = 2,
	.hs_video_delay_frame = 2,
	.slim_video_delay_frame = 2,
	.custom1_delay_frame = 2,
	.custom2_delay_frame = 2,
/* HS03s code for DEVAL5625-990 by chenjun at 2021/05/28 end */

	.isp_driving_current = ISP_DRIVING_6MA,
	.sensor_interface_type = SENSOR_INTERFACE_TYPE_MIPI,
	.mipi_sensor_type = MIPI_OPHY_NCSI2,
	.mipi_settle_delay_mode = MIPI_SETTLEDELAY_AUTO,
#if GC13053_MIRROR_NORMAL
	.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_Gr,
#elif GC13053_MIRROR_HV
	.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_Gb,
#else
	.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_Gr,
#endif
	.mclk = 24,
	.mipi_lane_num = SENSOR_MIPI_4_LANE,
	.i2c_addr_table = { 0x72, 0xff },
	.i2c_speed = 400,
};

static struct imgsensor_struct imgsensor = {
	.mirror = IMAGE_HV_MIRROR,
	.sensor_mode = IMGSENSOR_MODE_INIT,
	.shutter = 0x0b98,
	.gain = 0x40,
	.dummy_pixel = 0,
	.dummy_line = 0,
	.current_fps = 300,
	.autoflicker_en = KAL_FALSE,
	.test_pattern = KAL_FALSE,
	.current_scenario_id = MSDK_SCENARIO_ID_CAMERA_PREVIEW,
	.ihdr_en = 0,
	.i2c_write_id = 0x72,
};

/* Sensor output window information */
/* HS03s code for DEVAL5625-990 by chenjun at 2021/05/28 start */
static struct SENSOR_WINSIZE_INFO_STRUCT imgsensor_winsize_info[7] = {
	{ 4208, 3120, 0, 0, 4208, 3120, 2104, 1560, 0, 0, 2104, 1560, 0, 0, 2104, 1560 }, /* Preview */
	{ 4208, 3120, 0, 0, 4208, 3120, 4208, 3120, 0, 0, 4208, 3120, 0, 0, 4208, 3120 }, /* capture */
	{ 4208, 3120, 0, 0, 4208, 3120, 4208, 3120, 0, 0, 4208, 3120, 0, 0, 4208, 3120 }, /* video */
	{ 4208, 3120, 0, 0, 4208, 3120, 4208, 3120, 0, 0, 4208, 3120, 0, 0, 4208, 3120 }, /* hs video */
	{ 4208, 3120, 0, 0, 4208, 3120, 2104, 1560, 0, 0, 2104, 1560, 0, 0, 2104, 1560 }, /* slim video */
	{ 4208, 3120, 0, 0, 4208, 3120, 4208, 3120, 0, 0, 4208, 3120, 0, 0, 4208, 3120 }, /* custom1 */
	{ 4208, 3120, 0, 0, 4208, 3120, 4208, 3120, 0, 0, 4208, 3120, 0, 0, 4208, 3120 }, /* custom2 */
};
/* HS03s code for DEVAL5625-990 by chenjun at 2021/05/28 end */

static struct SET_PD_BLOCK_INFO_T imgsensor_pd_info = {
#if GC13053_MIRROR_NORMAL
	.i4OffsetX = 24,
	.i4OffsetY = 20,
	.i4PitchX = 64,
	.i4PitchY = 64,
	.i4PairNum = 16,
	.i4SubBlkW = 16,
	.i4SubBlkH = 16,
	.i4BlockNumX = 65,
	.i4BlockNumY = 48,
	.i4PosR = {
/*hs03s code for DEVAL5625-925 by gaozhenyu at 2021/06/02 start*/
			{28, 27},
			{44, 31},
			{64, 31},
			{80, 27},
			{32, 39},
			{48, 43},
			{60, 43},
			{76, 39},
			{32, 63},
			{48, 59},
			{60, 59},
			{76, 63},
			{28, 75},
			{44, 71},
			{64, 71},
			{80, 75}
		},
		.i4PosL = {
			{28, 23},
			{44, 27},
			{64, 27},
			{80, 23},
			{32, 43},
			{48, 47},
			{60, 47},
			{76, 43},
			{32, 59},
			{48, 55},
			{60, 55},
			{76, 59},
			{28, 79},
			{44, 75},
			{64, 75},
			{80, 79}
	},
/*hs03s code for DEVAL5625-925 by gaozhenyu at 2021/06/02 end*/
#elif GC13053_MIRROR_HV
	.i4OffsetX = 24,
	.i4OffsetY = 28,
	.i4PitchX = 64,
	.i4PitchY = 64,
	.i4PairNum = 16,
	.i4SubBlkW = 16,
	.i4SubBlkH = 16,
	.i4BlockNumX = 65,
	.i4BlockNumY = 48,
	.i4PosR = {
		{30, 31},
		{82, 31},
		{46, 35},
		{66, 35},
		{34, 51},
		{78, 51},
		{50, 55},
		{62, 55},
		{50, 63},
		{62, 63},
		{34, 67},
		{78, 67},
		{46, 83},
		{66, 83},
		{30, 87},
		{82, 87}
	},
	.i4PosL = {
		{30, 35},
		{82, 35},
		{46, 39},
		{66, 39},
		{34, 47},
		{78, 47},
		{50, 51},
		{62, 51},
		{50, 67},
		{62, 67},
		{34, 71},
		{78, 71},
		{46, 79},
		{66, 79},
		{30, 83},
		{82, 83}
	},
#endif
        .i4Crop = {{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0}},
	.iMirrorFlip = 0,	/* 0:Normal; 1:H; 2:V; 3HV (imgsensor.mirror - Normal)*/
};

static kal_uint16 read_cmos_sensor(kal_uint32 addr)
{
	kal_uint16 get_byte = 0;
	char pu_send_cmd[2] = {
		(char)((addr >> 8) & 0xff),
		(char)(addr & 0xff)
	};

	iReadRegI2C(pu_send_cmd, 2, (u8 *)&get_byte, 1, imgsensor.i2c_write_id);

	return get_byte;
}

static void write_cmos_sensor(kal_uint32 addr, kal_uint32 para)
{
	char pu_send_cmd[4] = {
		(char)((addr >> 8) & 0xff),
		(char)(addr & 0xff),
		(char)((para >> 8) & 0xff),
		(char)(para & 0xff)
	};

	iWriteRegI2C(pu_send_cmd, 4, imgsensor.i2c_write_id);
}

static void write_cmos_sensor_8bit(kal_uint32 addr, kal_uint32 para)
{
	char pu_send_cmd[3] = {
		(char)((addr >> 8) & 0xff),
		(char)(addr & 0xff),
		(char)(para & 0xff)
	};

	iWriteRegI2C(pu_send_cmd, 3, imgsensor.i2c_write_id);
}

static void table_write_cmos_sensor(kal_uint16 *para, kal_uint32 len)
{
	char puSendCmd[I2C_BUFFER_LEN];
	kal_uint32 tosend = 0, idx = 0;
	kal_uint16 addr = 0, data = 0;

	while (len > idx) {
		addr = para[idx];
		puSendCmd[tosend++] = (char)((addr >> 8) & 0xff);
		puSendCmd[tosend++] = (char)(addr & 0xff);
		data = para[idx + 1];
		puSendCmd[tosend++] = (char)(data & 0xff);
		idx += 2;
#if MULTI_WRITE
		if (tosend >= I2C_BUFFER_LEN || idx == len) {
			iBurstWriteReg_multi(puSendCmd, tosend, imgsensor.i2c_write_id,
				3, imgsensor_info.i2c_speed);
			tosend = 0;
		}
#else
		iWriteRegI2CTiming(puSendCmd, 3, imgsensor.i2c_write_id, imgsensor_info.i2c_speed);
		tosend = 0;
#endif
	}
}

static kal_uint32 return_sensor_id(void)
{
	kal_uint32 sensor_id = 0;

	sensor_id = (read_cmos_sensor(0x03f0) << 8) | read_cmos_sensor(0x03f1);
	return sensor_id;
}

static void set_dummy(void)
{
	LOG_INF("frame length = %d\n", imgsensor.frame_length);
	write_cmos_sensor(0x0340, imgsensor.frame_length & 0xfffe);
}

static void set_max_framerate(kal_uint16 framerate, kal_bool min_framelength_en)
{
	kal_uint32 frame_length = imgsensor.frame_length;

	frame_length = imgsensor.pclk / framerate * 10 / imgsensor.line_length;
	spin_lock(&imgsensor_drv_lock);
	imgsensor.frame_length = (frame_length > imgsensor.min_frame_length) ?
		frame_length : imgsensor.min_frame_length;
	imgsensor.dummy_line = imgsensor.frame_length - imgsensor.min_frame_length;

	if (imgsensor.frame_length > imgsensor_info.max_frame_length) {
		imgsensor.frame_length = imgsensor_info.max_frame_length;
		imgsensor.dummy_line = imgsensor.frame_length - imgsensor.min_frame_length;
	}
	if (min_framelength_en)
		imgsensor.min_frame_length = imgsensor.frame_length;
	spin_unlock(&imgsensor_drv_lock);
	set_dummy();
}

static void set_shutter(kal_uint16 shutter)
{
	unsigned long flags;
	kal_uint16 realtime_fps = 0;
	/*kal_uint32 frame_length = 0;*/
	spin_lock_irqsave(&imgsensor_drv_lock, flags);
	imgsensor.shutter = shutter;
	spin_unlock_irqrestore(&imgsensor_drv_lock, flags);

	/* if shutter bigger than frame_length, should extend frame length first */
	spin_lock(&imgsensor_drv_lock);
	if (shutter > imgsensor.min_frame_length - imgsensor_info.margin)
		imgsensor.frame_length = shutter + imgsensor_info.margin;
	else
		imgsensor.frame_length = imgsensor.min_frame_length;

	if (imgsensor.frame_length > imgsensor_info.max_frame_length)
		imgsensor.frame_length = imgsensor_info.max_frame_length;
	spin_unlock(&imgsensor_drv_lock);

	shutter = (shutter < imgsensor_info.min_shutter) ? imgsensor_info.min_shutter : shutter;
	shutter = (shutter > (imgsensor_info.max_frame_length - imgsensor_info.margin)) ?
		(imgsensor_info.max_frame_length - imgsensor_info.margin) : shutter;

	realtime_fps = imgsensor.pclk / imgsensor.line_length * 10 / imgsensor.frame_length;
	LOG_INF("realtime_fps = %d\n", realtime_fps);

	if (imgsensor.autoflicker_en) {
		if (realtime_fps >= 297 && realtime_fps <= 305)
			set_max_framerate(296, 0);
		else if (realtime_fps >= 147 && realtime_fps <= 150)
			set_max_framerate(146, 0);
		else
			set_max_framerate(realtime_fps, 0);
	} else {
		set_max_framerate(realtime_fps, 0);
	}

	/* Update Shutter */
	write_cmos_sensor(0x0202, shutter);

	LOG_INF("shutter = %d, framelength = %d\n",
		shutter, imgsensor.frame_length);
}

/* HS03s code for DEVAL5625-990 by chenjun at 2021/05/28 start */
static void set_shutter_frame_length(kal_uint16 shutter, kal_uint16 frame_length)
{
	unsigned long flags;
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

	shutter = (shutter < imgsensor_info.min_shutter) ? imgsensor_info.min_shutter : shutter;

	shutter = (shutter > (imgsensor_info.max_frame_length - imgsensor_info.margin)) ?
		(imgsensor_info.max_frame_length - imgsensor_info.margin) : shutter;

	set_dummy();

	if (shutter > 16383) shutter = 16386;
	if (shutter < 1) shutter = 1;

	/* Update shutter */
	write_cmos_sensor(0x0202, shutter);

	LOG_INF("shutter = %d, framelength = %d\n", shutter, imgsensor.frame_length);
}
/* HS03s code for DEVAL5625-990 by chenjun at 2021/05/28 end */

static kal_uint16 gain2reg(kal_uint16 gain)
{
	kal_uint16 reg_gain = gain << 4;

	reg_gain = (reg_gain < GC13053_SENSOR_BASE_GAIN) ? GC13053_SENSOR_BASE_GAIN : reg_gain;
	reg_gain = (reg_gain > GC13053_SENSOR_MAX_GAIN) ? GC13053_SENSOR_MAX_GAIN : reg_gain;

	return reg_gain;
}

static kal_uint16 set_gain(kal_uint16 gain)
{
	kal_uint32 reg_gain = 0;

	LOG_INF("E\n");
	reg_gain = gain2reg(gain);
	write_cmos_sensor(0x0204, reg_gain & 0xffff);
	LOG_INF("gain = %d, reg_gain = %d\n", gain, reg_gain);
	return gain;
}

static void ihdr_write_shutter_gain(kal_uint16 le, kal_uint16 se, kal_uint16 gain)
{
	LOG_INF("le: 0x%x, se: 0x%x, gain: 0x%x\n", le, se, gain);
}

/*
*static void set_mirror_flip(kal_uint8 image_mirror)
*{
*	LOG_INF("image_mirror = %d\n", image_mirror);
*	write_cmos_sensor(0xfe, 0x00);
*	switch (image_mirror) {
*	case IMAGE_NORMAL:
*		write_cmos_sensor(0x17, 0xd4);
*		break;
*	case IMAGE_H_MIRROR:
*		write_cmos_sensor(0x17, 0xd5);
*		break;
*	case IMAGE_V_MIRROR:
*		write_cmos_sensor(0x17, 0xd6);
*		break;
*	case IMAGE_HV_MIRROR:
*		write_cmos_sensor(0x17, 0xd7);
*		break;
*	}
*}
*/

static void night_mode(kal_bool enable)
{
	/* No Need to implement this function */
}

kal_uint16 gc13053_init_addr_data[] = {
	/*system*/
	0x031c, 0x01,
	0x0320, 0xbb,
	0x0321, 0x10,
	0x0325, 0x40,
	0x0326, 0x66,
	0x0335, 0x51,
	0x0336, 0x7b,
	0x0314, 0x00,
	0x0315, 0xab,
	0x0317, 0x00,
	0x031c, 0x91,
	0x0180, 0x68,
	0x0324, 0x48,
	0x0334, 0x40,
	0x031c, 0x9f,
	0x031c, 0x9f,
	0x031c, 0x9f,
	0x031c, 0x9e,

	/*pre_setting*/
	0x008c, 0xc0,
	0x0087, 0x53,
	0x0220, 0x15,
	0x021f, 0x11,
	0x0234, 0x00,
	0x02ee, 0x03,
	0x009a, 0x0c,
	0x009b, 0x38,
	0x0080, 0x10,
	0x006b, 0x00,
	0x006d, 0x00,
	0x0101, GC13053_MIRROR,
	0x0010, 0x01,
	0x0011, 0x00,

	/*analog*/
	0x0202, 0x0b,
	0x0203, 0xa0,
	0x028d, 0x92,
	0x02e5, 0x03,
	0x0224, 0x00,
	0x0290, 0x00,
	0x0340, 0x0c,
	0x0341, 0xa0,
	0x029d, 0x00,
	0x029e, 0x40,
	0x0256, 0x20,
	0x0342, 0x04,
	0x0343, 0xe4,
	0x0344, 0x00,
	0x0345, 0x10,
	0x0346, 0x00,
	0x0347, 0x10,
	0x0348, 0x10,
	0x0349, 0x80,
	0x034a, 0x0c,
	0x034b, 0x38,
	0x0053, 0x00,
	0x0219, 0x09,
	0x024a, 0x02,
	0x024e, 0x24,
	0x028c, 0x24,
	0x029c, 0x00,
	0x02e1, 0x00,
	0x024b, 0x16,
	0x0255, 0x16,
	0x0221, 0x3c,
	0x0229, 0x80,
	0x025b, 0x12,
	0x0252, 0x63,
	0x02da, 0x05,
	0x02db, 0xd0,
	0x0514, 0x01,
	0x0515, 0x00,
	0x02d9, 0x46,
	0x0217, 0x8b,
	0x0551, 0x50,
	0x02e3, 0x48,
	0x02f3, 0x19,
	0x0530, 0x18,
	0x0531, 0x12,
	0x0532, 0x20,
	0x0533, 0x10,
	0x02cd, 0x26,
	0x022e, 0x66,
	0x0243, 0x06,
	0x0390, 0x78,
	0x039a, 0xc0,
	0x0393, 0xcf,
	0x0397, 0xcc,
	0x0399, 0xb8,
	0x02a8, 0xba,
	0x02d4, 0x40,
	0x0216, 0x04,
	0x022c, 0x2c,
	0x023e, 0x09,
	0x0518, 0x00,
	0x021d, 0x03,
	0x031c, 0x80,
	0x03fe, 0x10,
	0x03fe, 0x00,
	0x031c, 0x9e,
	0x03fe, 0x00,
	0x03fe, 0x00,
	0x03fe, 0x00,
	0x03fe, 0x00,
	0x031c, 0x80,
	0x03fe, 0x10,
	0x03fe, 0x00,
	0x031c, 0x9e,

	/*auto_load*/
	0x0a54, 0x08,
	0x0a59, 0x40,
	0x0a65, 0x10,
	0x0317, 0x08,
	0x0a67, 0x80,
	0x0a70, 0x05,
	0x0a71, 0x12,
	0x0a72, 0x20,
	0x0a76, 0x00,
	0x0a77, 0xa0,
	0x00be, 0x00,
	0x00a9, 0x01,
	0x0313, 0x80,

	/*ISP*/
	0x0089, 0x03,
	0x00c1, 0x80,
	0x00e3, 0x08,
	0x00ef, 0x4b,
	0x05a6, 0x10,
	0x05c4, 0x1c,

	/*ISP_Dither*/
	0x00c3, 0xc0,
	0x00e5, 0x6c,
	0x053f, 0x00,

	/*ISP_BLK*/
	0x0040, 0x22,
	0x0041, 0x20,
	0x0043, 0x10,
	0x0044, 0x00,
	0x0046, 0x08,
	0x0048, 0x03,
	0x0414, 0x80,
	0x0415, 0x80,
	0x0416, 0x80,
	0x0417, 0x80,
	0x0060, 0x40,

	/*OB_dark_ratio*/
	0x0042, 0x20,
	0x0424, 0x04,
	0x0425, 0x04,
	0x0426, 0x04,
	0x0427, 0x04,

	/*darksun*/
	0x00ef, 0x0b,
	0x00f2, 0x00,
	0x00f3, 0x03,
	0x00f4, 0xff,
	0x00f5, 0x05,
	0x00f6, 0x80,
	0x00f7, 0xf0,

	/*ISP_RTC*/
	0x00c0, 0x80,
	0x0470, 0x02,
	0x0471, 0x04,
	0x0472, 0x10,
	0x0473, 0x1a,
	0x0474, 0x2d,
	0x0475, 0x28,
	0x0476, 0x2a,
	0x0477, 0x2c,
	0x0480, 0x01,
	0x0481, 0x01,
	0x0482, 0x03,
	0x0483, 0x03,
	0x0484, 0x03,
	0x0485, 0x03,
	0x0486, 0x03,
	0x0487, 0x03,
	0x0478, 0x04,
	0x0479, 0x08,
	0x047a, 0x0c,
	0x047b, 0x0e,
	0x047c, 0x08,
	0x047d, 0x0a,
	0x047e, 0x0e,
	0x047f, 0x10,
	0x0488, 0x04,
	0x0489, 0x04,
	0x048a, 0x04,
	0x048b, 0x04,
	0x048c, 0x04,
	0x048d, 0x04,
	0x048e, 0x04,
	0x048f, 0x04,

	/*auto_load_off*/
	0x00be, 0x01,
	0x0a70, 0x00,
	0x0317, 0x00,
	0x0a67, 0x00,

};

kal_uint16 gc13053_binning_addr_data[] = {
	/*system*/
	0x031c, 0x01,
	0x0320, 0xbb,
	0x0321, 0x10,
	0x0325, 0x41,
	0x0326, 0x65,
	0x0335, 0x55,
	0x0336, 0x7b,
	0x0314, 0x00,
	0x0315, 0xa5,
	0x0317, 0x00,
	0x031c, 0x91,
	0x0180, 0x68,
	0x0324, 0x48,
	0x0334, 0x40,
	0x031c, 0x9f,
	0x031c, 0x9f,
	0x031c, 0x9f,
	0x031c, 0x9e,

	/*pre_setting*/
	0x008c, 0xc0,
	0x0087, 0x53,
	0x0220, 0x15,
	0x021f, 0x19,
	0x0234, 0x20,
	0x006b, 0x00,
	0x006d, 0x00,
	0x0010, 0x11,

	/*analog*/
	0x0202, 0x05,
	0x0203, 0xd0,
	0x028d, 0x93,
	0x02e5, 0x00,
	0x0224, 0x41,
	0x0340, 0x06,
	0x0341, 0x50,
	0x029d, 0x00,
	0x029e, 0x40,
	0x0256, 0x20,
	0x0342, 0x04,
	0x0343, 0xc8,
	0x0344, 0x00,
	0x0345, 0x10,
	0x0346, 0x00,
	0x0347, 0x10,
	0x0348, 0x10,
	0x0349, 0x80,
	0x034a, 0x0c,
	0x034b, 0x38,
	0x024a, 0x04,
	0x024b, 0x16,
	0x0255, 0x16,
	0x0221, 0x2c,
	0x0229, 0x60,
	0x025b, 0x11,
	0x0252, 0x43,
	0x02d9, 0x06,
	0x0217, 0x83,
	0x02e3, 0x28,
	0x0530, 0x18,
	0x0531, 0x0c,
	0x0532, 0x20,
	0x0533, 0x10,
	0x02cd, 0x22,
	0x022e, 0x22,
	0x0390, 0x70,
	0x023e, 0x09,
	0x021d, 0x03,
	0x031c, 0x80,
	0x03fe, 0x10,
	0x03fe, 0x00,
	0x031c, 0x9e,
	0x03fe, 0x00,
	0x03fe, 0x00,
	0x03fe, 0x00,
	0x03fe, 0x00,
	0x031c, 0x80,
	0x03fe, 0x10,
	0x03fe, 0x00,
	0x031c, 0x9e,

	/*GAIN paritition*/
	0x05af, 0x00,
	0x05ae, 0x01,
	0x05ad, 0x00,
	0x05b0, 0x01,
	0x05b0, 0x00,
	0x05b0, 0x00,
	0x05b0, 0x00,
	0x05b0, 0x01,
	0x05b0, 0x2E,
	0x05b0, 0x00,
	0x05b0, 0x20,
	0x05b0, 0x01,
	0x05b0, 0x6B,
	0x05b0, 0x00,
	0x05b0, 0x01,
	0x05b0, 0x01,
	0x05b0, 0xAE,
	0x05b0, 0x00,
	0x05b0, 0x21,
	0x05b0, 0x01,
	0x05b0, 0xF9,
	0x05b0, 0x00,
	0x05b0, 0x02,
	0x05b0, 0x02,
	0x05b0, 0x57,
	0x05b0, 0x00,
	0x05b0, 0x22,
	0x05b0, 0x02,
	0x05b0, 0xCC,
	0x05b0, 0x00,
	0x05b0, 0x03,
	0x05b0, 0x03,
	0x05b0, 0x50,
	0x05b0, 0x00,
	0x05b0, 0x23,
	0x05b0, 0x03,
	0x05b0, 0xDF,
	0x05b0, 0x00,
	0x05b0, 0x04,
	0x05b0, 0x04,
	0x05b0, 0x98,
	0x05b0, 0x00,
	0x05b0, 0x24,
	0x05b0, 0x05,
	0x05b0, 0x91,
	0x05b0, 0x00,
	0x05b0, 0x0b,
	0x05b0, 0x06,
	0x05b0, 0x9B,
	0x05b0, 0x00,
	0x05b0, 0x2b,
	0x05b0, 0x07,
	0x05b0, 0xAC,
	0x05b0, 0x00,
	0x05b0, 0x0c,
	0x05b0, 0x09,
	0x05b0, 0x1C,
	0x05b0, 0x00,
	0x05b0, 0x2c,
	0x05b0, 0x0A,
	0x05b0, 0xF8,
	0x05b0, 0x00,
	0x05b0, 0x1b,
	0x05b0, 0x0D,
	0x05b0, 0x07,
	0x05b0, 0x00,
	0x05b0, 0x3b,
	0x05b0, 0x0F,
	0x05b0, 0x07,
	0x05b0, 0x00,
	0x05b0, 0x1c,
	0x05b0, 0x0F,
	0x05b0, 0x07,
	0x05b0, 0x00,
	0x05b0, 0x1c,
	0x05b0, 0x0F,
	0x05b0, 0x07,
	0x05b0, 0x00,
	0x05b0, 0x1c,
	0x05b0, 0x0F,
	0x05b0, 0x07,
	0x05b0, 0x00,
	0x05b0, 0x1c,
	0x05b0, 0x00,
	0x05b0, 0x80,
	0x05b0, 0x02,
	0x05b0, 0x17,
	0x05b0, 0x02,
	0x05b0, 0xd9,
	0x05b0, 0xd4,
	0x05b0, 0x83,
	0x05b0, 0x06,
	0x05b0, 0xd0,
	0x05b0, 0x83,
	0x05b0, 0x06,
	0x05b0, 0xd0,
	0x05b0, 0x88,
	0x05b0, 0x2e,
	0x05af, 0x01,
	0x05a5, 0x03,
	0x05a7, 0x10,
	0x05a8, 0x04,
	0x05a9, 0x00,
	0x029f, 0x64,

	/*GAIN*/
	0x0204, 0x04,
	0x0205, 0x00,
	0x02b0, 0x68,

	/*BLK*/
	0x0040, 0x22,
	0x0041, 0x22,
#if GC13053_MIRROR_NORMAL
	/*PDAF_Normal*/
	0x0012, 0x18,
	0x0013, 0x10,
	0x0014, 0x00,
	0x0015, 0x18,
	0x0016, 0x30,
	0x0019, 0x0c,
#elif GC13053_MIRROR_HV
	/*PDAF_HV*/
	0x0012, 0x22,
	0x0013, 0x10,
	0x0014, 0x00,
	0x0015, 0x22,
	0x0016, 0x30,
	0x0019, 0x0c,
#endif
	/*window 2104x1560*/
	0x0351, 0x00,
	0x0352, 0x02,
	0x0353, 0x00,
	0x0354, 0x04,
	0x034c, 0x08,
	0x034d, 0x38,
	0x034e, 0x06,
	0x034f, 0x18,

	/*MIPI*/
	0x0107, 0x29,
	0x0108, 0x0c,
	0x0181, 0xf0,
	0x0185, 0x01,
	0x0112, 0x0a,
	0x0113, 0x0a,
	0x0114, 0x03,
	0x0115, 0x20,
	0x0121, 0x05,
	0x0122, 0x04,
	0x0123, 0x13,
	0x0125, 0x0b,
	0x0126, 0x07,
	0x0129, 0x04,
	0x012a, 0x06,
	0x012b, 0x07,
	0x008c, 0x40,

};

kal_uint16 gc13053_fullsize_addr_data[] = {
	/*system*/
	0x031c, 0x01,
	0x0320, 0xbb,
	0x0321, 0x10,
	0x0325, 0x40,
	0x0326, 0x66,
	0x0335, 0x51,
	0x0336, 0x7b,
	0x0314, 0x00,
	0x0315, 0xab,
	0x0317, 0x00,
	0x031c, 0x91,
	0x0180, 0x68,
	0x0324, 0x48,
	0x0334, 0x40,
	0x031c, 0x9f,
	0x031c, 0x9f,
	0x031c, 0x9f,
	0x031c, 0x9e,

	/*pre_setting*/
	0x008c, 0xc0,
	0x0087, 0x53,
	0x0220, 0x15,
	0x021f, 0x11,
	0x0234, 0x00,
	0x006b, 0x00,
	0x006d, 0x00,
	0x0010, 0x01,

	/*analog*/
	0x0202, 0x0b,
	0x0203, 0xa0,
	0x028d, 0x92,
	0x02e5, 0x03,
	0x0224, 0x00,
	0x0340, 0x0c,
	0x0341, 0xa0,
	0x029d, 0x00,
	0x029e, 0x40,
	0x0256, 0x20,
	0x0342, 0x04,
	0x0343, 0xe4,
	0x0344, 0x00,
	0x0345, 0x10,
	0x0346, 0x00,
	0x0347, 0x10,
	0x0348, 0x10,
	0x0349, 0x80,
	0x034a, 0x0c,
	0x034b, 0x38,
	0x024a, 0x02,
	0x024b, 0x16,
	0x0255, 0x16,
	0x0221, 0x3c,
	0x0229, 0x80,
	0x025b, 0x12,
	0x0252, 0x63,
	0x02d9, 0x46,
	0x0217, 0x8b,
	0x02e3, 0x48,
	0x0530, 0x18,
	0x0531, 0x12,
	0x0532, 0x20,
	0x0533, 0x10,
	0x02cd, 0x26,
	0x022e, 0x66,
	0x0390, 0x78,
	0x023e, 0x09,
	0x021d, 0x03,
	0x031c, 0x80,
	0x03fe, 0x10,
	0x03fe, 0x00,
	0x031c, 0x9e,
	0x03fe, 0x00,
	0x03fe, 0x00,
	0x03fe, 0x00,
	0x03fe, 0x00,
	0x031c, 0x80,
	0x03fe, 0x10,
	0x03fe, 0x00,
	0x031c, 0x9e,

	/*GAIN paritition*/
	0x05af, 0x00,
	0x05ae, 0x01,
	0x05ad, 0x00,
	0x05b0, 0x01,
	0x05b0, 0x00,
	0x05b0, 0x00,
	0x05b0, 0x00,
	0x05b0, 0x01,
	0x05b0, 0x2C,
	0x05b0, 0x00,
	0x05b0, 0x20,
	0x05b0, 0x01,
	0x05b0, 0x66,
	0x05b0, 0x00,
	0x05b0, 0x01,
	0x05b0, 0x01,
	0x05b0, 0xA8,
	0x05b0, 0x00,
	0x05b0, 0x21,
	0x05b0, 0x01,
	0x05b0, 0xF3,
	0x05b0, 0x00,
	0x05b0, 0x02,
	0x05b0, 0x02,
	0x05b0, 0x4F,
	0x05b0, 0x00,
	0x05b0, 0x22,
	0x05b0, 0x02,
	0x05b0, 0xBE,
	0x05b0, 0x00,
	0x05b0, 0x03,
	0x05b0, 0x03,
	0x05b0, 0x3E,
	0x05b0, 0x00,
	0x05b0, 0x23,
	0x05b0, 0x03,
	0x05b0, 0xC8,
	0x05b0, 0x00,
	0x05b0, 0x04,
	0x05b0, 0x04,
	0x05b0, 0x7A,
	0x05b0, 0x00,
	0x05b0, 0x24,
	0x05b0, 0x05,
	0x05b0, 0x69,
	0x05b0, 0x00,
	0x05b0, 0x0b,
	0x05b0, 0x06,
	0x05b0, 0x6A,
	0x05b0, 0x00,
	0x05b0, 0x2b,
	0x05b0, 0x07,
	0x05b0, 0x72,
	0x05b0, 0x00,
	0x05b0, 0x0c,
	0x05b0, 0x08,
	0x05b0, 0xCF,
	0x05b0, 0x00,
	0x05b0, 0x2c,
	0x05b0, 0x0A,
	0x05b0, 0xA0,
	0x05b0, 0x00,
	0x05b0, 0x1b,
	0x05b0, 0x0C,
	0x05b0, 0xA0,
	0x05b0, 0x00,
	0x05b0, 0x3b,
	0x05b0, 0x0E,
	0x05b0, 0x7B,
	0x05b0, 0x00,
	0x05b0, 0x1C,
	0x05b0, 0x0E,
	0x05b0, 0x7B,
	0x05b0, 0x00,
	0x05b0, 0x1C,
	0x05b0, 0x0E,
	0x05b0, 0x7B,
	0x05b0, 0x00,
	0x05b0, 0x1C,
	0x05b0, 0x0E,
	0x05b0, 0x7B,
	0x05b0, 0x00,
	0x05b0, 0x1C,
	0x05b0, 0x03,
	0x05b0, 0x1d,
	0x05b0, 0x03,
	0x05b0, 0x90,
	0x05b0, 0x03,
	0x05b0, 0x1d,
	0x05b0, 0x00,
	0x05b0, 0x80,
	0x05b0, 0x02,
	0x05b0, 0x17,
	0x05b0, 0x02,
	0x05b0, 0xd9,
	0x05b0, 0x29,
	0x05b0, 0x70,
	0x05b0, 0x28,
	0x05b0, 0xd4,
	0x05b0, 0x88,
	0x05b0, 0x2e,
	0x05b0, 0x29,
	0x05b0, 0x78,
	0x05b0, 0x28,
	0x05b0, 0xd0,
	0x05b0, 0x88,
	0x05b0, 0x2e,
	0x05b0, 0x29,
	0x05b0, 0x78,
	0x05b0, 0x28,
	0x05b0, 0xd0,
	0x05b0, 0x8f,
	0x05b0, 0x66,
	0x05af, 0x01,
	0x05a5, 0x06,
	0x05a7, 0x10,
	0x05a8, 0x08,
	0x05a9, 0x00,
	0x029f, 0x64,

	/*GAIN*/
	0x0204, 0x04,
	0x0205, 0x00,
	0x02b0, 0x68,

	/*BLK*/
	0x0040, 0x22,
	0x0041, 0x20,

#if GC13053_MIRROR_NORMAL
	/*PDAF_Normal*/
	0x0012, 0x18,
	0x0013, 0x10,
	0x0014, 0x00,
	0x0015, 0x18,
	0x0016, 0x30,
	0x0019, 0x04,
#elif GC13053_MIRROR_HV
	/*PDAF_HV*/
	0x0012, 0x20,
	0x0013, 0x12,
	0x0014, 0x00,
	0x0015, 0x20,
	0x0016, 0x32,
	0x0019, 0x04,
#endif
	/*window 4208x3120*/
	0x0351, 0x00,
	0x0352, 0x04,
	0x0353, 0x00,
	0x0354, 0x08,
	0x034c, 0x10,
	0x034d, 0x70,
	0x034e, 0x0c,
	0x034f, 0x30,

	/*MIPI*/
	0x0107, 0x29,
	0x0108, 0x0c,
	0x0181, 0xf0,
	0x0185, 0x01,
	0x0112, 0x0a,
	0x0113, 0x0a,
	0x0114, 0x03,
	0x0115, 0x20,
	0x0121, 0x0c,
	0x0122, 0x09,
	0x0123, 0x28,
	0x0125, 0x12,
	0x0126, 0x0c,
	0x0129, 0x08,
	0x012a, 0x10,
	0x012b, 0x0c,
	0x008c, 0x40,

};

/* HS03s code for DEVAL5625-990 by chenjun at 2021/05/28 start */
kal_uint16 gc13053_custom1_addr_data[] = {
	/*system*/
	0x031c, 0x01,
	0x0320, 0xbb,
	0x0321, 0x10,
	0x0325, 0x40,
	0x0326, 0x66,
	0x0335, 0x51,
	0x0336, 0x7b,
	0x0314, 0x00,
	0x0315, 0xab,
	0x0317, 0x00,
	0x031c, 0x91,
	0x0180, 0x68,
	0x0324, 0x48,
	0x0334, 0x40,
	0x031c, 0x9f,
	0x031c, 0x9f,
	0x031c, 0x9f,
	0x031c, 0x9e,

	/*pre_setting*/
	0x008c, 0xc0,
	0x0087, 0x53,
	0x0220, 0x15,
	0x021f, 0x11,
	0x0234, 0x00,
	0x006b, 0x00,
	0x006d, 0x00,
	0x0010, 0x01,

	/*analog*/
	0x0202, 0x0b,
	0x0203, 0xa0,
	0x028d, 0x92,
	0x02e5, 0x03,
	0x0224, 0x00,
	0x0340, 0x0c,
	0x0341, 0xa0,
	0x029d, 0x00,
	0x029e, 0x40,
	0x0256, 0x20,
	0x0342, 0x04,
	0x0343, 0xe4,
	0x0344, 0x00,
	0x0345, 0x10,
	0x0346, 0x00,
	0x0347, 0x10,
	0x0348, 0x10,
	0x0349, 0x80,
	0x034a, 0x0c,
	0x034b, 0x38,
	0x024a, 0x02,
	0x024b, 0x16,
	0x0255, 0x16,
	0x0221, 0x3c,
	0x0229, 0x80,
	0x025b, 0x12,
	0x0252, 0x63,
	0x02d9, 0x46,
	0x0217, 0x8b,
	0x02e3, 0x48,
	0x0530, 0x18,
	0x0531, 0x12,
	0x0532, 0x20,
	0x0533, 0x10,
	0x02cd, 0x26,
	0x022e, 0x66,
	0x0390, 0x78,
	0x023e, 0x09,
	0x021d, 0x03,
	0x031c, 0x80,
	0x03fe, 0x10,
	0x03fe, 0x00,
	0x031c, 0x9e,
	0x03fe, 0x00,
	0x03fe, 0x00,
	0x03fe, 0x00,
	0x03fe, 0x00,
	0x031c, 0x80,
	0x03fe, 0x10,
	0x03fe, 0x00,
	0x031c, 0x9e,

	/*GAIN paritition*/
	0x05af, 0x00,
	0x05ae, 0x01,
	0x05ad, 0x00,
	0x05b0, 0x01,
	0x05b0, 0x00,
	0x05b0, 0x00,
	0x05b0, 0x00,
	0x05b0, 0x01,
	0x05b0, 0x2C,
	0x05b0, 0x00,
	0x05b0, 0x20,
	0x05b0, 0x01,
	0x05b0, 0x66,
	0x05b0, 0x00,
	0x05b0, 0x01,
	0x05b0, 0x01,
	0x05b0, 0xA8,
	0x05b0, 0x00,
	0x05b0, 0x21,
	0x05b0, 0x01,
	0x05b0, 0xF3,
	0x05b0, 0x00,
	0x05b0, 0x02,
	0x05b0, 0x02,
	0x05b0, 0x4F,
	0x05b0, 0x00,
	0x05b0, 0x22,
	0x05b0, 0x02,
	0x05b0, 0xBE,
	0x05b0, 0x00,
	0x05b0, 0x03,
	0x05b0, 0x03,
	0x05b0, 0x3E,
	0x05b0, 0x00,
	0x05b0, 0x23,
	0x05b0, 0x03,
	0x05b0, 0xC8,
	0x05b0, 0x00,
	0x05b0, 0x04,
	0x05b0, 0x04,
	0x05b0, 0x7A,
	0x05b0, 0x00,
	0x05b0, 0x24,
	0x05b0, 0x05,
	0x05b0, 0x69,
	0x05b0, 0x00,
	0x05b0, 0x0b,
	0x05b0, 0x06,
	0x05b0, 0x6A,
	0x05b0, 0x00,
	0x05b0, 0x2b,
	0x05b0, 0x07,
	0x05b0, 0x72,
	0x05b0, 0x00,
	0x05b0, 0x0c,
	0x05b0, 0x08,
	0x05b0, 0xCF,
	0x05b0, 0x00,
	0x05b0, 0x2c,
	0x05b0, 0x0A,
	0x05b0, 0xA0,
	0x05b0, 0x00,
	0x05b0, 0x1b,
	0x05b0, 0x0C,
	0x05b0, 0xA0,
	0x05b0, 0x00,
	0x05b0, 0x3b,
	0x05b0, 0x0E,
	0x05b0, 0x7B,
	0x05b0, 0x00,
	0x05b0, 0x1C,
	0x05b0, 0x0E,
	0x05b0, 0x7B,
	0x05b0, 0x00,
	0x05b0, 0x1C,
	0x05b0, 0x0E,
	0x05b0, 0x7B,
	0x05b0, 0x00,
	0x05b0, 0x1C,
	0x05b0, 0x0E,
	0x05b0, 0x7B,
	0x05b0, 0x00,
	0x05b0, 0x1C,
	0x05b0, 0x03,
	0x05b0, 0x1d,
	0x05b0, 0x03,
	0x05b0, 0x90,
	0x05b0, 0x03,
	0x05b0, 0x1d,
	0x05b0, 0x00,
	0x05b0, 0x80,
	0x05b0, 0x02,
	0x05b0, 0x17,
	0x05b0, 0x02,
	0x05b0, 0xd9,
	0x05b0, 0x29,
	0x05b0, 0x70,
	0x05b0, 0x28,
	0x05b0, 0xd4,
	0x05b0, 0x88,
	0x05b0, 0x2e,
	0x05b0, 0x29,
	0x05b0, 0x78,
	0x05b0, 0x28,
	0x05b0, 0xd0,
	0x05b0, 0x88,
	0x05b0, 0x2e,
	0x05b0, 0x29,
	0x05b0, 0x78,
	0x05b0, 0x28,
	0x05b0, 0xd0,
	0x05b0, 0x8f,
	0x05b0, 0x66,
	0x05af, 0x01,
	0x05a5, 0x06,
	0x05a7, 0x10,
	0x05a8, 0x08,
	0x05a9, 0x00,
	0x029f, 0x64,

	/*GAIN*/
	0x0204, 0x04,
	0x0205, 0x00,
	0x02b0, 0x68,

	/*BLK*/
	0x0040, 0x22,
	0x0041, 0x20,

#if GC13053_MIRROR_NORMAL
	/*PDAF_Normal*/
	0x0012, 0x18,
	0x0013, 0x10,
	0x0014, 0x00,
	0x0015, 0x18,
	0x0016, 0x30,
	0x0019, 0x04,
#elif GC13053_MIRROR_HV
	/*PDAF_HV*/
	0x0012, 0x20,
	0x0013, 0x12,
	0x0014, 0x00,
	0x0015, 0x20,
	0x0016, 0x32,
	0x0019, 0x04,
#endif
	/*window 4208x3120*/
	0x0351, 0x00,
	0x0352, 0x04,
	0x0353, 0x00,
	0x0354, 0x08,
	0x034c, 0x10,
	0x034d, 0x70,
	0x034e, 0x0c,
	0x034f, 0x30,

	/*MIPI*/
	0x0107, 0x29,
	0x0108, 0x0c,
	0x0181, 0xf0,
	0x0185, 0x01,
	0x0112, 0x0a,
	0x0113, 0x0a,
	0x0114, 0x03,
	0x0115, 0x20,
	0x0121, 0x0c,
	0x0122, 0x09,
	0x0123, 0x28,
	0x0125, 0x12,
	0x0126, 0x0c,
	0x0129, 0x08,
	0x012a, 0x10,
	0x012b, 0x0c,
	0x008c, 0x40,

};

kal_uint16 gc13053_custom2_addr_data[] = {
	/*system*/
	0x031c, 0x01,
	0x0320, 0xbb,
	0x0321, 0x10,
	0x0325, 0x40,
	0x0326, 0x66,
	0x0335, 0x51,
	0x0336, 0x7b,
	0x0314, 0x00,
	0x0315, 0xab,
	0x0317, 0x00,
	0x031c, 0x91,
	0x0180, 0x68,
	0x0324, 0x48,
	0x0334, 0x40,
	0x031c, 0x9f,
	0x031c, 0x9f,
	0x031c, 0x9f,
	0x031c, 0x9e,

	/*pre_setting*/
	0x008c, 0xc0,
	0x0087, 0x53,
	0x0220, 0x15,
	0x021f, 0x11,
	0x0234, 0x00,
	0x006b, 0x00,
	0x006d, 0x00,
	0x0010, 0x01,

	/*analog*/
	0x0202, 0x0b,
	0x0203, 0xa0,
	0x028d, 0x92,
	0x02e5, 0x03,
	0x0224, 0x00,
	0x0340, 0x0f,
	0x0341, 0xb4,
	0x029d, 0x00,
	0x029e, 0x40,
	0x0256, 0x20,
	0x0342, 0x04,
	0x0343, 0xe4,
	0x0344, 0x00,
	0x0345, 0x10,
	0x0346, 0x00,
	0x0347, 0x10,
	0x0348, 0x10,
	0x0349, 0x80,
	0x034a, 0x0c,
	0x034b, 0x38,
	0x024a, 0x02,
	0x024b, 0x16,
	0x0255, 0x16,
	0x0221, 0x3c,
	0x0229, 0x80,
	0x025b, 0x12,
	0x0252, 0x63,
	0x02d9, 0x46,
	0x0217, 0x8b,
	0x02e3, 0x48,
	0x0530, 0x18,
	0x0531, 0x12,
	0x0532, 0x20,
	0x0533, 0x10,
	0x02cd, 0x26,
	0x022e, 0x66,
	0x0390, 0x78,
	0x023e, 0x09,
	0x021d, 0x03,
	0x031c, 0x80,
	0x03fe, 0x10,
	0x03fe, 0x00,
	0x031c, 0x9e,
	0x03fe, 0x00,
	0x03fe, 0x00,
	0x03fe, 0x00,
	0x03fe, 0x00,
	0x031c, 0x80,
	0x03fe, 0x10,
	0x03fe, 0x00,
	0x031c, 0x9e,

	/*GAIN paritition*/
	0x05af, 0x00,
	0x05ae, 0x01,
	0x05ad, 0x00,
	0x05b0, 0x01,
	0x05b0, 0x00,
	0x05b0, 0x00,
	0x05b0, 0x00,
	0x05b0, 0x01,
	0x05b0, 0x2C,
	0x05b0, 0x00,
	0x05b0, 0x20,
	0x05b0, 0x01,
	0x05b0, 0x66,
	0x05b0, 0x00,
	0x05b0, 0x01,
	0x05b0, 0x01,
	0x05b0, 0xA8,
	0x05b0, 0x00,
	0x05b0, 0x21,
	0x05b0, 0x01,
	0x05b0, 0xF3,
	0x05b0, 0x00,
	0x05b0, 0x02,
	0x05b0, 0x02,
	0x05b0, 0x4F,
	0x05b0, 0x00,
	0x05b0, 0x22,
	0x05b0, 0x02,
	0x05b0, 0xBE,
	0x05b0, 0x00,
	0x05b0, 0x03,
	0x05b0, 0x03,
	0x05b0, 0x3E,
	0x05b0, 0x00,
	0x05b0, 0x23,
	0x05b0, 0x03,
	0x05b0, 0xC8,
	0x05b0, 0x00,
	0x05b0, 0x04,
	0x05b0, 0x04,
	0x05b0, 0x7A,
	0x05b0, 0x00,
	0x05b0, 0x24,
	0x05b0, 0x05,
	0x05b0, 0x69,
	0x05b0, 0x00,
	0x05b0, 0x0b,
	0x05b0, 0x06,
	0x05b0, 0x6A,
	0x05b0, 0x00,
	0x05b0, 0x2b,
	0x05b0, 0x07,
	0x05b0, 0x72,
	0x05b0, 0x00,
	0x05b0, 0x0c,
	0x05b0, 0x08,
	0x05b0, 0xCF,
	0x05b0, 0x00,
	0x05b0, 0x2c,
	0x05b0, 0x0A,
	0x05b0, 0xA0,
	0x05b0, 0x00,
	0x05b0, 0x1b,
	0x05b0, 0x0C,
	0x05b0, 0xA0,
	0x05b0, 0x00,
	0x05b0, 0x3b,
	0x05b0, 0x0E,
	0x05b0, 0x7B,
	0x05b0, 0x00,
	0x05b0, 0x1C,
	0x05b0, 0x0E,
	0x05b0, 0x7B,
	0x05b0, 0x00,
	0x05b0, 0x1C,
	0x05b0, 0x0E,
	0x05b0, 0x7B,
	0x05b0, 0x00,
	0x05b0, 0x1C,
	0x05b0, 0x0E,
	0x05b0, 0x7B,
	0x05b0, 0x00,
	0x05b0, 0x1C,
	0x05b0, 0x03,
	0x05b0, 0x1d,
	0x05b0, 0x03,
	0x05b0, 0x90,
	0x05b0, 0x03,
	0x05b0, 0x1d,
	0x05b0, 0x00,
	0x05b0, 0x80,
	0x05b0, 0x02,
	0x05b0, 0x17,
	0x05b0, 0x02,
	0x05b0, 0xd9,
	0x05b0, 0x29,
	0x05b0, 0x70,
	0x05b0, 0x28,
	0x05b0, 0xd4,
	0x05b0, 0x88,
	0x05b0, 0x2e,
	0x05b0, 0x29,
	0x05b0, 0x78,
	0x05b0, 0x28,
	0x05b0, 0xd0,
	0x05b0, 0x88,
	0x05b0, 0x2e,
	0x05b0, 0x29,
	0x05b0, 0x78,
	0x05b0, 0x28,
	0x05b0, 0xd0,
	0x05b0, 0x8f,
	0x05b0, 0x66,
	0x05af, 0x01,
	0x05a5, 0x06,
	0x05a7, 0x10,
	0x05a8, 0x08,
	0x05a9, 0x00,
	0x029f, 0x64,

	/*GAIN*/
	0x0204, 0x04,
	0x0205, 0x00,
	0x02b0, 0x68,

	/*BLK*/
	0x0040, 0x22,
	0x0041, 0x20,

#if GC13053_MIRROR_NORMAL
	/*PDAF_Normal*/
	0x0012, 0x18,
	0x0013, 0x10,
	0x0014, 0x00,
	0x0015, 0x18,
	0x0016, 0x30,
	0x0019, 0x04,
#elif GC13053_MIRROR_HV
	/*PDAF_HV*/
	0x0012, 0x20,
	0x0013, 0x12,
	0x0014, 0x00,
	0x0015, 0x20,
	0x0016, 0x32,
	0x0019, 0x04,
#endif
	/*window 4208x3120*/
	0x0351, 0x00,
	0x0352, 0x04,
	0x0353, 0x00,
	0x0354, 0x08,
	0x034c, 0x10,
	0x034d, 0x70,
	0x034e, 0x0c,
	0x034f, 0x30,

	/*MIPI*/
	0x0107, 0x29,
	0x0108, 0x0c,
	0x0181, 0xf0,
	0x0185, 0x01,
	0x0112, 0x0a,
	0x0113, 0x0a,
	0x0114, 0x03,
	0x0115, 0x20,
	0x0121, 0x0c,
	0x0122, 0x09,
	0x0123, 0x28,
	0x0125, 0x12,
	0x0126, 0x0c,
	0x0129, 0x08,
	0x012a, 0x10,
	0x012b, 0x0c,
	0x008c, 0x40,

};
/* HS03s code for DEVAL5625-990 by chenjun at 2021/05/28 end */
/* A03s code for DEVAL5625-973 by xuxianwei at 2021/06/18 end */
/* HS03s code for DEVAL5625-990 by chenjun at 2021/05/28 start */
kal_uint16 gc13053_streamOn_addr_data[] = {
	// 0x0100, 0x09,
};

kal_uint16 gc13053_streamOff_addr_data[] = {
	// 0x0100, 0x00,
};
/* HS03s code for DEVAL5625-990 by chenjun at 2021/05/28 end */

static void sensor_init(void)
{
	LOG_INF("E\n");
	table_write_cmos_sensor(gc13053_init_addr_data,
		sizeof(gc13053_init_addr_data)/sizeof(kal_uint16));
}

static void preview_setting(void)
{
	table_write_cmos_sensor(gc13053_streamOff_addr_data,
		sizeof(gc13053_streamOn_addr_data)/sizeof(kal_uint16));
	table_write_cmos_sensor(gc13053_binning_addr_data,
		sizeof(gc13053_binning_addr_data)/sizeof(kal_uint16));
	table_write_cmos_sensor(gc13053_streamOn_addr_data,
		sizeof(gc13053_streamOn_addr_data)/sizeof(kal_uint16));
}

static void capture_setting(void)
{
	table_write_cmos_sensor(gc13053_streamOff_addr_data,
		sizeof(gc13053_streamOn_addr_data)/sizeof(kal_uint16));
	table_write_cmos_sensor(gc13053_fullsize_addr_data,
		sizeof(gc13053_fullsize_addr_data)/sizeof(kal_uint16));
	table_write_cmos_sensor(gc13053_streamOn_addr_data,
		sizeof(gc13053_streamOn_addr_data)/sizeof(kal_uint16));
}

static void normal_video_setting(void)
{
	table_write_cmos_sensor(gc13053_streamOff_addr_data,
		sizeof(gc13053_streamOn_addr_data)/sizeof(kal_uint16));
	table_write_cmos_sensor(gc13053_fullsize_addr_data,
		sizeof(gc13053_fullsize_addr_data)/sizeof(kal_uint16));
	table_write_cmos_sensor(gc13053_streamOn_addr_data,
		sizeof(gc13053_streamOn_addr_data)/sizeof(kal_uint16));
}

static void hs_video_setting(void)
{
	table_write_cmos_sensor(gc13053_streamOff_addr_data,
		sizeof(gc13053_streamOn_addr_data)/sizeof(kal_uint16));
	table_write_cmos_sensor(gc13053_fullsize_addr_data,
		sizeof(gc13053_fullsize_addr_data)/sizeof(kal_uint16));
	table_write_cmos_sensor(gc13053_streamOn_addr_data,
		sizeof(gc13053_streamOn_addr_data)/sizeof(kal_uint16));
}

static void slim_video_setting(void)
{
	table_write_cmos_sensor(gc13053_streamOff_addr_data,
		sizeof(gc13053_streamOn_addr_data)/sizeof(kal_uint16));
	table_write_cmos_sensor(gc13053_binning_addr_data,
		sizeof(gc13053_binning_addr_data)/sizeof(kal_uint16));
	table_write_cmos_sensor(gc13053_streamOn_addr_data,
		sizeof(gc13053_streamOn_addr_data)/sizeof(kal_uint16));
}

/* HS03s code for DEVAL5625-990 by chenjun at 2021/05/28 start */
static void custom1_setting(void)
{
	table_write_cmos_sensor(gc13053_streamOff_addr_data,
		sizeof(gc13053_streamOn_addr_data)/sizeof(kal_uint16));
	table_write_cmos_sensor(gc13053_custom1_addr_data,
		sizeof(gc13053_custom1_addr_data)/sizeof(kal_uint16));
	table_write_cmos_sensor(gc13053_streamOn_addr_data,
		sizeof(gc13053_streamOn_addr_data)/sizeof(kal_uint16));
}

static void custom2_setting(void)
{
	table_write_cmos_sensor(gc13053_streamOff_addr_data,
		sizeof(gc13053_streamOn_addr_data)/sizeof(kal_uint16));
	table_write_cmos_sensor(gc13053_custom2_addr_data,
		sizeof(gc13053_custom2_addr_data)/sizeof(kal_uint16));
	table_write_cmos_sensor(gc13053_streamOn_addr_data,
		sizeof(gc13053_streamOn_addr_data)/sizeof(kal_uint16));
}
/* HS03s code for DEVAL5625-990 by chenjun at 2021/05/28 end */

static kal_uint32 set_test_pattern_mode(kal_bool enable)
{
	LOG_INF("enable: %d\n", enable);

	if (enable)
		write_cmos_sensor_8bit(0x008c, 0x41);
	else
		write_cmos_sensor_8bit(0x008c, 0x40);

	spin_lock(&imgsensor_drv_lock);
	imgsensor.test_pattern = enable;
	spin_unlock(&imgsensor_drv_lock);
	return ERROR_NONE;
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
				pr_debug("[GC13053MIPI]get_imgsensor_id: i2c write id: 0x%x, sensor id: 0x%x\n",
					imgsensor.i2c_write_id, *sensor_id);

				return ERROR_NONE;
			}
			pr_debug("[GC13053MIPI]get_imgsensor_id: Read sensor id fail, write id: 0x%x, id: 0x%x\n",
			imgsensor.i2c_write_id, *sensor_id);
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


static kal_uint32 open(void)
{
	kal_uint8 i = 0;
	kal_uint8 retry = 2;
	kal_uint32 sensor_id = 0;

	LOG_1;

	while (imgsensor_info.i2c_addr_table[i] != 0xff) {
		spin_lock(&imgsensor_drv_lock);
		imgsensor.i2c_write_id = imgsensor_info.i2c_addr_table[i];
		spin_unlock(&imgsensor_drv_lock);
		do {
			sensor_id = return_sensor_id();
			if (sensor_id == imgsensor_info.sensor_id) {
				pr_debug("[GC13053MIPI]open: i2c write id: 0x%x, sensor id: 0x%x\n",
					imgsensor.i2c_write_id, sensor_id);
				break;
			}
			pr_debug("[GC13053MIPI]open: Read sensor id fail, write id: 0x%x, id: 0x%x\n",
			imgsensor.i2c_write_id, sensor_id);
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
	gc13053_otp_function(imgsensor.i2c_write_id);

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
}

static kal_uint32 close(void)
{
	LOG_INF("E\n");
	/* No Need to implement this function */
	return ERROR_NONE;
}


static kal_uint32 preview(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
	MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("E\n");

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_PREVIEW;
	imgsensor.pclk = imgsensor_info.pre.pclk;
	/* imgsensor.video_mode = KAL_FALSE; */
	imgsensor.line_length = imgsensor_info.pre.linelength;
	imgsensor.frame_length = imgsensor_info.pre.framelength;
	imgsensor.min_frame_length = imgsensor_info.pre.framelength;
	imgsensor.autoflicker_en = KAL_TRUE;
	spin_unlock(&imgsensor_drv_lock);

	preview_setting();
	return ERROR_NONE;
}


static kal_uint32 capture(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
	MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("E\n");
	
	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_CAPTURE;
		if (imgsensor.current_fps != imgsensor_info.cap.max_framerate)
			LOG_INF("Warning: current_fps %d fps is not support, so use cap's setting: %d fps!\n",
				imgsensor.current_fps, imgsensor_info.cap.max_framerate / 10);
		imgsensor.pclk = imgsensor_info.cap.pclk;
		imgsensor.line_length = imgsensor_info.cap.linelength;
		imgsensor.frame_length = imgsensor_info.cap.framelength;
		imgsensor.min_frame_length = imgsensor_info.cap.framelength;
	imgsensor.autoflicker_en = KAL_TRUE;
	spin_unlock(&imgsensor_drv_lock);

	capture_setting();

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
	/* imgsensor.current_fps = 300; */
	imgsensor.autoflicker_en = KAL_TRUE;
	spin_unlock(&imgsensor_drv_lock);

	normal_video_setting();

	return ERROR_NONE;
}

static kal_uint32 hs_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
	MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("E\n");

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_HIGH_SPEED_VIDEO;
	imgsensor.pclk = imgsensor_info.hs_video.pclk;
	/* imgsensor.video_mode = KAL_TRUE; */
	imgsensor.line_length = imgsensor_info.hs_video.linelength;
	imgsensor.frame_length = imgsensor_info.hs_video.framelength;
	imgsensor.min_frame_length = imgsensor_info.hs_video.framelength;
	imgsensor.dummy_line = 0;
	imgsensor.dummy_pixel = 0;
	imgsensor.autoflicker_en = KAL_TRUE;
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
	imgsensor.line_length = imgsensor_info.slim_video.linelength;
	imgsensor.frame_length = imgsensor_info.slim_video.framelength;
	imgsensor.min_frame_length = imgsensor_info.slim_video.framelength;
	imgsensor.dummy_line = 0;
	imgsensor.dummy_pixel = 0;
	imgsensor.autoflicker_en = KAL_TRUE;
	spin_unlock(&imgsensor_drv_lock);

	slim_video_setting();

	return ERROR_NONE;
}

/* HS03s code for DEVAL5625-990 by chenjun at 2021/05/28 start */
static kal_uint32 custom1(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
	MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("E\n");

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_CUSTOM1;
		if (imgsensor.current_fps != imgsensor_info.custom1.max_framerate)
			LOG_INF("Warning: current_fps %d fps is not support, so use cap's setting: %d fps!\n",
				imgsensor.current_fps, imgsensor_info.custom1.max_framerate / 10);
		imgsensor.pclk = imgsensor_info.custom1.pclk;
		imgsensor.line_length = imgsensor_info.custom1.linelength;
		imgsensor.frame_length = imgsensor_info.custom1.framelength;
		imgsensor.min_frame_length = imgsensor_info.custom1.framelength;
	imgsensor.autoflicker_en = KAL_TRUE;
	spin_unlock(&imgsensor_drv_lock);

	custom1_setting();

	return ERROR_NONE;
}

static kal_uint32 custom2(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
	MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("E\n");

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_CUSTOM2;
		if (imgsensor.current_fps != imgsensor_info.custom2.max_framerate)
			LOG_INF("Warning: current_fps %d fps is not support, so use cap's setting: %d fps!\n",
				imgsensor.current_fps, imgsensor_info.custom2.max_framerate / 10);
		imgsensor.pclk = imgsensor_info.custom2.pclk;
		imgsensor.line_length = imgsensor_info.custom2.linelength;
		imgsensor.frame_length = imgsensor_info.custom2.framelength;
		imgsensor.min_frame_length = imgsensor_info.custom2.framelength;
	imgsensor.autoflicker_en = KAL_TRUE;
	spin_unlock(&imgsensor_drv_lock);

	custom2_setting();

	return ERROR_NONE;
}
/* HS03s code for DEVAL5625-990 by chenjun at 2021/05/28 end */

static kal_uint32 get_resolution(MSDK_SENSOR_RESOLUTION_INFO_STRUCT *sensor_resolution)
{
	LOG_INF("E\n");
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
/* HS03s code for DEVAL5625-990 by chenjun at 2021/05/28 start */
	sensor_resolution->SensorCustom1Width = imgsensor_info.custom1.grabwindow_width;
	sensor_resolution->SensorCustom1Height = imgsensor_info.custom1.grabwindow_height;
	sensor_resolution->SensorCustom2Width = imgsensor_info.custom2.grabwindow_width;
	sensor_resolution->SensorCustom2Height = imgsensor_info.custom2.grabwindow_height;
/* HS03s code for DEVAL5625-990 by chenjun at 2021/05/28 end */
	return ERROR_NONE;
}


static kal_uint32 get_info(enum MSDK_SCENARIO_ID_ENUM scenario_id,
	MSDK_SENSOR_INFO_STRUCT *sensor_info,
	MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("scenario_id = %d\n", scenario_id);

	sensor_info->SensorClockPolarity = SENSOR_CLOCK_POLARITY_LOW;
	sensor_info->SensorClockFallingPolarity = SENSOR_CLOCK_POLARITY_LOW;
	sensor_info->SensorHsyncPolarity = SENSOR_CLOCK_POLARITY_LOW;
	sensor_info->SensorVsyncPolarity = SENSOR_CLOCK_POLARITY_LOW;
	sensor_info->SensorInterruptDelayLines = 4;
	sensor_info->SensorResetActiveHigh = FALSE;
	sensor_info->SensorResetDelayCount = 5;

	sensor_info->SensroInterfaceType = imgsensor_info.sensor_interface_type;
	sensor_info->MIPIsensorType = imgsensor_info.mipi_sensor_type;
	sensor_info->SettleDelayMode = imgsensor_info.mipi_settle_delay_mode;
	sensor_info->SensorOutputDataFormat = imgsensor_info.sensor_output_dataformat;

	sensor_info->CaptureDelayFrame = imgsensor_info.cap_delay_frame;
	sensor_info->PreviewDelayFrame = imgsensor_info.pre_delay_frame;
	sensor_info->VideoDelayFrame = imgsensor_info.video_delay_frame;
	sensor_info->HighSpeedVideoDelayFrame = imgsensor_info.hs_video_delay_frame;
	sensor_info->SlimVideoDelayFrame = imgsensor_info.slim_video_delay_frame;
/* HS03s code for DEVAL5625-990 by chenjun at 2021/05/28 start */
	sensor_info->Custom1DelayFrame = imgsensor_info.custom1_delay_frame;
	sensor_info->Custom2DelayFrame = imgsensor_info.custom2_delay_frame;

	sensor_info->SensorMasterClockSwitch = 0; /* not use */
	sensor_info->SensorDrivingCurrent = imgsensor_info.isp_driving_current;

	sensor_info->AEShutDelayFrame = imgsensor_info.ae_shut_delay_frame;
	/* The frame of setting shutter default 0 for TG int */
	sensor_info->AESensorGainDelayFrame = imgsensor_info.ae_sensor_gain_delay_frame;
	/* The frame of setting sensor gain */
	sensor_info->AEISPGainDelayFrame = imgsensor_info.ae_ispGain_delay_frame;
	sensor_info->FrameTimeDelayFrame = imgsensor_info.frame_time_delay_frame;
	sensor_info->IHDR_Support = imgsensor_info.ihdr_support;
	sensor_info->IHDR_LE_FirstLine = imgsensor_info.ihdr_le_firstline;
	sensor_info->SensorModeNum = imgsensor_info.sensor_mode_num;
/* HS03s code for DEVAL5625-990 by chenjun at 2021/05/28 end */

	sensor_info->SensorMIPILaneNumber = imgsensor_info.mipi_lane_num;
	sensor_info->SensorClockFreq = imgsensor_info.mclk;
	sensor_info->SensorClockDividCount = 3; /* not use */
	sensor_info->SensorClockRisingCount = 0;
	sensor_info->SensorClockFallingCount = 2; /* not use */
	sensor_info->SensorPixelClockCount = 3; /* not use */
	sensor_info->SensorDataLatchCount = 2; /* not use */

	sensor_info->MIPIDataLowPwr2HighSpeedTermDelayCount = 0;
	sensor_info->MIPICLKLowPwr2HighSpeedTermDelayCount = 0;
	sensor_info->SensorWidthSampling = 0;  /* 0 is default 1x */
	sensor_info->SensorHightSampling = 0;  /* 0 is default 1x */
	sensor_info->SensorPacketECCOrder = 1;
	sensor_info->PDAF_Support = 1;
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
/* HS03s code for DEVAL5625-990 by chenjun at 2021/05/28 start */
	case MSDK_SCENARIO_ID_CUSTOM1:
		sensor_info->SensorGrabStartX = imgsensor_info.custom1.startx;
		sensor_info->SensorGrabStartY = imgsensor_info.custom1.starty;
		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
			imgsensor_info.custom1.mipi_data_lp2hs_settle_dc;
		break;
	case MSDK_SCENARIO_ID_CUSTOM2:
		sensor_info->SensorGrabStartX = imgsensor_info.custom2.startx;
		sensor_info->SensorGrabStartY = imgsensor_info.custom2.starty;
		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
			imgsensor_info.custom2.mipi_data_lp2hs_settle_dc;
		break;
/* HS03s code for DEVAL5625-990 by chenjun at 2021/05/28 end */
	default:
		sensor_info->SensorGrabStartX = imgsensor_info.pre.startx;
		sensor_info->SensorGrabStartY = imgsensor_info.pre.starty;
		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
			imgsensor_info.pre.mipi_data_lp2hs_settle_dc;
		break;
	}

	return ERROR_NONE;
}

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
/* HS03s code for DEVAL5625-990 by chenjun at 2021/05/28 start */
	case MSDK_SCENARIO_ID_CUSTOM1:
		custom1(image_window, sensor_config_data);
		break;
	case MSDK_SCENARIO_ID_CUSTOM2:
		custom2(image_window, sensor_config_data);
		break;
/* HS03s code for DEVAL5625-990 by chenjun at 2021/05/28 end */
	default:
		LOG_INF("Error ScenarioId setting");
		preview(image_window, sensor_config_data);
		return ERROR_INVALID_SCENARIO_ID;
	}

	return ERROR_NONE;
}


static kal_uint32 set_video_mode(UINT16 framerate)
{	/*This Function not used after ROME*/
	LOG_INF("framerate = %d\n ", framerate);
	/* SetVideoMode Function should fix framerate */
	/*
	 *if (framerate == 0)	 //Dynamic frame rate
	 *	return ERROR_NONE;
	 *spin_lock(&imgsensor_drv_lock);
	 *if ((framerate == 300) && (imgsensor.autoflicker_en == KAL_TRUE))
	 *	imgsensor.current_fps = 296;
	 *else if ((framerate == 150) && (imgsensor.autoflicker_en == KAL_TRUE))
	 *	imgsensor.current_fps = 146;
	 *else
	 *	imgsensor.current_fps = framerate;
	 *spin_unlock(&imgsensor_drv_lock);
	 *set_max_framerate(imgsensor.current_fps, 1);
	*/
	return ERROR_NONE;
}

static kal_uint32 set_auto_flicker_mode(kal_bool enable, UINT16 framerate)
{
	LOG_INF("enable = %d, framerate = %d\n", enable, framerate);
	spin_lock(&imgsensor_drv_lock);
	if (enable) /* enable auto flicker */
		imgsensor.autoflicker_en = KAL_TRUE;
	else /* Cancel Auto flick */
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
		imgsensor.dummy_line = (frame_length > imgsensor_info.pre.framelength) ?
			(frame_length - imgsensor_info.pre.framelength) : 0;
		imgsensor.frame_length = imgsensor_info.pre.framelength + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		set_dummy();
		break;
	case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
		if (framerate == 0)
			return ERROR_NONE;
		frame_length = imgsensor_info.normal_video.pclk / framerate * 10 /
			imgsensor_info.normal_video.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line = (frame_length > imgsensor_info.normal_video.framelength) ?
			(frame_length - imgsensor_info.normal_video.framelength) : 0;
		imgsensor.frame_length = imgsensor_info.normal_video.framelength + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		set_dummy();
		break;
	case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			if (imgsensor.current_fps != imgsensor_info.cap.max_framerate)
				LOG_INF("Warning: current_fps %d fps is not support, so use cap's setting: %d fps!\n",
					framerate, imgsensor_info.cap.max_framerate / 10);
			frame_length = imgsensor_info.cap.pclk / framerate * 10 / imgsensor_info.cap.linelength;
			spin_lock(&imgsensor_drv_lock);
			imgsensor.dummy_line = (frame_length > imgsensor_info.cap.framelength) ?
				(frame_length - imgsensor_info.cap.framelength) : 0;
			imgsensor.frame_length = imgsensor_info.cap.framelength + imgsensor.dummy_line;
			imgsensor.min_frame_length = imgsensor.frame_length;
			spin_unlock(&imgsensor_drv_lock);

		set_dummy();
		break;
	case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
		frame_length = imgsensor_info.hs_video.pclk / framerate * 10 / imgsensor_info.hs_video.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line = (frame_length > imgsensor_info.hs_video.framelength) ?
			(frame_length - imgsensor_info.hs_video.framelength) : 0;
		imgsensor.frame_length = imgsensor_info.hs_video.framelength + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		set_dummy();
		break;
	case MSDK_SCENARIO_ID_SLIM_VIDEO:
		frame_length = imgsensor_info.slim_video.pclk / framerate * 10 / imgsensor_info.slim_video.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line = (frame_length > imgsensor_info.slim_video.framelength) ?
			(frame_length - imgsensor_info.slim_video.framelength) : 0;
		imgsensor.frame_length = imgsensor_info.slim_video.framelength + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		set_dummy();
		break;
/* HS03s code for DEVAL5625-990 by chenjun at 2021/05/28 start */
	case MSDK_SCENARIO_ID_CUSTOM1:
		if (imgsensor.current_fps != imgsensor_info.custom1.max_framerate)
			LOG_INF("Warning: current_fps %d fps is not support, so use cap's setting: %d fps!\n",
				framerate, imgsensor_info.custom1.max_framerate / 10);
		frame_length = imgsensor_info.custom1.pclk / framerate * 10 / imgsensor_info.custom1.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line = (frame_length > imgsensor_info.custom1.framelength) ?
			(frame_length - imgsensor_info.custom1.framelength) : 0;
		imgsensor.frame_length = imgsensor_info.custom1.framelength + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		set_dummy();
		break;
	case MSDK_SCENARIO_ID_CUSTOM2:
		if (imgsensor.current_fps != imgsensor_info.custom2.max_framerate)
			LOG_INF("Warning: current_fps %d fps is not support, so use cap's setting: %d fps!\n",
				framerate, imgsensor_info.custom2.max_framerate / 10);
		frame_length = imgsensor_info.custom2.pclk / framerate * 10 / imgsensor_info.custom2.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line = (frame_length > imgsensor_info.custom2.framelength) ?
			(frame_length - imgsensor_info.custom2.framelength) : 0;
		imgsensor.frame_length = imgsensor_info.custom2.framelength + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		set_dummy();
		break;
/* HS03s code for DEVAL5625-990 by chenjun at 2021/05/28 end */
	default:  /* coding with  preview scenario by default */
		frame_length = imgsensor_info.pre.pclk / framerate * 10 / imgsensor_info.pre.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line = (frame_length > imgsensor_info.pre.framelength) ?
			(frame_length - imgsensor_info.pre.framelength) : 0;
		imgsensor.frame_length = imgsensor_info.pre.framelength + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		set_dummy();
		LOG_INF("error scenario_id = %d, we use preview scenario\n", scenario_id);
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
/* HS03s code for DEVAL5625-990 by chenjun at 2021/05/28 start */
	case MSDK_SCENARIO_ID_CUSTOM1:
		*framerate = imgsensor_info.custom1.max_framerate;
		break;
	case MSDK_SCENARIO_ID_CUSTOM2:
		*framerate = imgsensor_info.custom2.max_framerate;
		break;
/* HS03s code for DEVAL5625-990 by chenjun at 2021/05/28 end */
	default:
		break;
	}

	return ERROR_NONE;
}

/* HS03s code for DEVAL5625-990 by chenjun at 2021/05/28 start */
static kal_uint32 streaming_control(kal_bool enable)
{
	pr_debug("streaming_enable(0=Sw Standby,1=streaming): %d\n", enable);
	if (enable){
		write_cmos_sensor_8bit(0x0100, 0x09);
		pr_debug("stream on");
	}else{
		write_cmos_sensor_8bit(0x0100, 0x00);
		pr_debug("stream off");
	}
	mdelay(10);
	return ERROR_NONE;
}
/* HS03s code for DEVAL5625-990 by chenjun at 2021/05/28 end */

static kal_uint32 feature_control(MSDK_SENSOR_FEATURE_ENUM feature_id,
	UINT8 *feature_para, UINT32 *feature_para_len)
{
	UINT16 *feature_return_para_16 = (UINT16 *)feature_para;
	UINT16 *feature_data_16 = (UINT16 *)feature_para;
	UINT32 *feature_return_para_32 = (UINT32 *)feature_para;
	UINT32 *feature_data_32 = (UINT32 *)feature_para;
	unsigned long long *feature_data = (unsigned long long *) feature_para;
	/* unsigned long long *feature_return_para=(unsigned long long *) feature_para; */

	struct SENSOR_WINSIZE_INFO_STRUCT *wininfo;
	MSDK_SENSOR_REG_INFO_STRUCT *sensor_reg_data = (MSDK_SENSOR_REG_INFO_STRUCT *)feature_para;
	struct SET_PD_BLOCK_INFO_T *PDAFinfo;
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
/* HS03s code for DEVAL5625-990 by chenjun at 2021/05/28 start */
			case MSDK_SCENARIO_ID_CUSTOM1:
				rate = imgsensor_info.custom1.mipi_pixel_rate;
				break;
			case MSDK_SCENARIO_ID_CUSTOM2:
				rate = imgsensor_info.custom2.mipi_pixel_rate;
				break;
/* HS03s code for DEVAL5625-990 by chenjun at 2021/05/28 end */
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
		set_gain((UINT16)*feature_data);
		break;
	case SENSOR_FEATURE_SET_FLASHLIGHT:
		break;
	case SENSOR_FEATURE_SET_ISP_MASTER_CLOCK_FREQ:
		break;
	case SENSOR_FEATURE_SET_REGISTER:
		write_cmos_sensor_8bit(sensor_reg_data->RegAddr, sensor_reg_data->RegData);
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
		set_max_framerate_by_scenario((enum MSDK_SCENARIO_ID_ENUM)*feature_data, *(feature_data + 1));
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
		LOG_INF("current fps: %d\n", (UINT32)*feature_data);
		spin_lock(&imgsensor_drv_lock);
		imgsensor.current_fps = *feature_data;
		spin_unlock(&imgsensor_drv_lock);
		break;
	case SENSOR_FEATURE_SET_HDR:
		LOG_INF("ihdr enable: %d\n", (BOOL)*feature_data);
		spin_lock(&imgsensor_drv_lock);
		imgsensor.ihdr_en = (BOOL)*feature_data;
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
/* HS03s code for DEVAL5625-990 by chenjun at 2021/05/28 start */
		case MSDK_SCENARIO_ID_CUSTOM1:
			memcpy((void *)wininfo, (void *)&imgsensor_winsize_info[5], sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_CUSTOM2:
			memcpy((void *)wininfo, (void *)&imgsensor_winsize_info[6], sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
/* HS03s code for DEVAL5625-990 by chenjun at 2021/05/28 end */
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		default:
			memcpy((void *)wininfo, (void *)&imgsensor_winsize_info[0], sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		}
		break;
	case SENSOR_FEATURE_SET_IHDR_SHUTTER_GAIN:
		LOG_INF("SENSOR_SET_SENSOR_IHDR LE = %d, SE = %d, Gain = %d\n",
			(UINT16)*feature_data, (UINT16)*(feature_data + 1), (UINT16)*(feature_data + 2));
		ihdr_write_shutter_gain((UINT16)*feature_data,
			(UINT16)*(feature_data + 1), (UINT16)*(feature_data + 2));
		break;
		/******************** PDAF START **********************/
	case SENSOR_FEATURE_GET_PDAF_INFO:
		LOG_INF("SENSOR_FEATURE_GET_PDAF_INFO scenarioId:%lld\n", *feature_data);
		PDAFinfo = (struct SET_PD_BLOCK_INFO_T *)(uintptr_t)(*(feature_data + 1));
		switch (*feature_data) {
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
			memcpy((void *)PDAFinfo, (void *)&imgsensor_pd_info, sizeof(struct SET_PD_BLOCK_INFO_T));
			break;
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
/* HS03s code for DEVAL5625-990 by chenjun at 2021/05/28 start */
		case MSDK_SCENARIO_ID_CUSTOM1:
		case MSDK_SCENARIO_ID_CUSTOM2:
/* HS03s code for DEVAL5625-990 by chenjun at 2021/05/28 end */
		default:
			break;
		}
		break;
	case SENSOR_FEATURE_GET_SENSOR_PDAF_CAPACITY:
		LOG_INF("SENSOR_FEATURE_GET_SENSOR_PDAF_CAPACITY scenarioId:%llu\n", *feature_data);
		switch (*feature_data) {
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) = 1;
			break;
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) = 1;
			break;
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) = 0; /* video & capture use same setting */
			break;
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) = 0;
			break;
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) = 0;
			break;
/* HS03s code for DEVAL5625-990 by chenjun at 2021/05/28 start */
		case MSDK_SCENARIO_ID_CUSTOM1:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) = 1;
			break;
		case MSDK_SCENARIO_ID_CUSTOM2:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) = 1;
			break;
/* HS03s code for DEVAL5625-990 by chenjun at 2021/05/28 end */
		default:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) = 0;
			break;
		}
		break;
	/*
	*case SENSOR_FEATURE_SET_PDAF:
	*	LOG_INF("SENSOR_FEATURE_SET_PDAF PDAF mode : %d\n", *feature_data_16);
	*	imgsensor.pdaf_mode = *feature_data_16;
	*	break;
	*/
	/******************** PDAF END *********************/
/* HS03s code for DEVAL5625-990 by chenjun at 2021/05/28 start */
	case SENSOR_FEATURE_SET_SHUTTER_FRAME_TIME:
		set_shutter_frame_length((UINT16) *feature_data, (UINT16) *(feature_data + 1));
		break;
	case SENSOR_FEATURE_SET_STREAMING_SUSPEND:
		streaming_control(KAL_FALSE);
		break;
	case SENSOR_FEATURE_SET_STREAMING_RESUME:
		if (*feature_data != 0) {
			set_shutter(*feature_data);
		}
		streaming_control(KAL_TRUE);
		break;
/* HS03s code for DEVAL5625-990 by chenjun at 2021/05/28 end */
	default:
		break;
	}
	return ERROR_NONE;
}
/* A03s code for DEVAL5625-221 by xuxianwei at 2021/05/26 end */
static struct SENSOR_FUNCTION_STRUCT sensor_func = {
	open,
	get_info,
	get_resolution,
	feature_control,
	control,
	close
};

UINT32  GC13053_LY_MIPI_RAW_SensorInit(struct SENSOR_FUNCTION_STRUCT **pfFunc)
{
	/* To Do : Check Sensor status here */
	if (pfFunc != NULL)
		*pfFunc = &sensor_func;
	return ERROR_NONE;
}
