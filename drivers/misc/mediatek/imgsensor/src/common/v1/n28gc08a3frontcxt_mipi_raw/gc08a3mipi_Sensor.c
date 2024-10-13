/******************************************************************************
 *
 * Filename:
 * ---------
 *     gc08a3mipi_Sensor.c
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
 *-----------------------------------------------------------------------------
 * Upper this line, this part is controlled by CC/CQ. DO NOT MODIFY!!
 *============================================================================
 *
 * Version:  V20211202143358 by GC-S-TEAM
 *

 ******************************************************************************/



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

#include "gc08a3mipi_Sensor.h"

/************************** Modify Following Strings for Debug **************************/
#define PFX "gc08a3_camera_sensor"
#define LOG_1 LOG_INF("GC08A3, MIPI 4LANE\n")
/****************************   Modify end    *******************************************/
#define GC08A3_DEBUG                0
#if GC08A3_DEBUG
#define LOG_INF(format, args...)    pr_debug(PFX "[%s] " format, __func__, ##args)
#else
#define LOG_INF(format, args...)
#endif

//+S96818AA1-1936,wuwenhao2.wt,ADD,2023/04/27,gc08a3 otp porting
#define GC08A3_OTP_FUNCTION 1

#if GC08A3_OTP_FUNCTION
#define MODULE_INFO_SIZE 7
#define MODULE_INFO_FLAG 0x15A0
#define MODULE_GROUP1_ADDR  0x15A8
#define MODULE_GROUP2_ADDR  0x15E8
#define MODULE_GROUP3_ADDR  0x1628
#define SN_INFO_SIZE 12
#define SN_INFO_FLAG 0x1668
#define SN_GROUP1_ADDR  0x1670
#define SN_GROUP2_ADDR  0x16D8
#define SN_GROUP3_ADDR  0x1740
#define AWB_INFO_SIZE 16
#define AWB_INFO_FLAG 0x17A8
#define AWB_GROUP1_FLAG  0x17B0
#define AWB_GROUP2_FLAG  0x1838
#define AWB_GROUP3_FLAG  0x18C0
#define LSC_INFO_SIZE 1868
#define LSC_INFO_FLAG 0x1948
#define LSC_GROUP1_FLAG  0x1950
#define LSC_GROUP2_FLAG  0x53B8
#define LSC_GROUP3_FLAG  0x8E20
static unsigned char gc08a3_data_lsc[LSC_INFO_SIZE + 1] = {0};/*Add check sum*/
static unsigned char gc08a3_data_awb[AWB_INFO_SIZE + 1] = {0};/*Add check sum*/
static unsigned char gc08a3_data_info[MODULE_INFO_SIZE + 1] = {0};/*Add check sum*/
static unsigned char gc08a3_data_sn[SN_INFO_SIZE + 1] = {0};/*Add check sum*/
#endif
//-S96818AA1-1936,wuwenhao2.wt,ADD,2023/04/27,gc08a3 otp porting

static DEFINE_SPINLOCK(imgsensor_drv_lock);

static struct imgsensor_info_struct imgsensor_info = {
	.sensor_id = N28GC08A3FRONTCXT_SENSOR_ID,
	.checksum_value = 0xe5d32119,
	.pre = {
		.pclk = 280800000,
		.linelength = 3672,
		.framelength = 2548,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 1632,
		.grabwindow_height = 1224,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 137800000,
		.max_framerate = 300,
	},
	.cap = {
		.pclk = 280800000,
		.linelength = 3672,
		.framelength = 2548,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 3264,
		.grabwindow_height = 2448,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 270400000,
		.max_framerate = 300,
	},
	.normal_video = {
		.pclk = 280800000,
		.linelength = 3672,
		.framelength = 2548,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 3264,
		.grabwindow_height = 2448,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 270400000,
		.max_framerate = 300,
	},
	.hs_video = {
		.pclk = 280800000,
		.linelength = 3672,
		.framelength = 2548,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 1632,
		.grabwindow_height = 1224,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 137800000,
		.max_framerate = 300,
	},
	.slim_video = {
		.pclk = 280800000,
		.linelength = 3672,
		.framelength = 2548,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 1280,
		.grabwindow_height = 720,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 137800000,
		.max_framerate = 300,
	},

	.margin = 16,
	.min_shutter = 2,
	.max_frame_length = 0xfffe,
	.ae_shut_delay_frame = 0,
	.ae_sensor_gain_delay_frame = 0,
	.ae_ispGain_delay_frame = 2,
	.ihdr_support = 0,
	.ihdr_le_firstline = 0,
	.sensor_mode_num = 5,

	.cap_delay_frame = 2,
	.pre_delay_frame = 2,
	.video_delay_frame = 2,
	.hs_video_delay_frame = 2,
	.slim_video_delay_frame = 2,

	.isp_driving_current = ISP_DRIVING_6MA,
	.sensor_interface_type = SENSOR_INTERFACE_TYPE_MIPI,
	.mipi_sensor_type = MIPI_OPHY_NCSI2,
	.mipi_settle_delay_mode = MIPI_SETTLEDELAY_AUTO,
#if GC08A3_MIRROR_NORMAL
	.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_R,
#elif GC08A3_MIRROR_H
	.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_Gr,
#elif GC08A3_MIRROR_V
	.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_Gb,
#elif GC08A3_MIRROR_HV
	.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_B,
#else
	.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_R,
#endif
	.mclk = 26,
	.mipi_lane_num = SENSOR_MIPI_4_LANE,
	.i2c_addr_table = {0x62, 0x20, 0x22, 0x24, 0xff},
	.i2c_speed = 400,
};

static struct imgsensor_struct imgsensor = {
	.mirror = IMAGE_HV_MIRROR,
	.sensor_mode = IMGSENSOR_MODE_INIT,
	.shutter = 0x900,
	.gain = 0x40,
	.dummy_pixel = 0,
	.dummy_line = 0,
	.current_fps = 300,
	.autoflicker_en = KAL_FALSE,
	.test_pattern = KAL_FALSE,
	.current_scenario_id = MSDK_SCENARIO_ID_CAMERA_PREVIEW,
	.ihdr_en = 0,
	.i2c_write_id = 0x62,
};

/* Sensor output window information */
static struct SENSOR_WINSIZE_INFO_STRUCT imgsensor_winsize_info[5] = {
	{ 3264, 2448, 0, 0, 3264, 2448, 1632, 1224, 0, 0, 1632, 1224, 0, 0, 1632, 1224}, /* Preview */
	{ 3264, 2448, 0, 0, 3264, 2448, 3264, 2448, 0, 0, 3264, 2448, 0, 0, 3264, 2448}, /* capture */
	{ 3264, 2448, 0, 0, 3264, 2448, 3264, 2448, 0, 0, 3264, 2448, 0, 0, 3264, 2448}, /* video */
	{ 3264, 2448, 0, 0, 3264, 2448, 1632, 1224, 0, 0, 1632, 1224, 0, 0, 1632, 1224}, /* hs video */
	{ 3264, 2448, 0, 0, 3264, 2448, 1632, 1224, 176, 252, 1280, 720, 0, 0, 1280, 720}  /* slim video */
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

	if (imgsensor.autoflicker_en) {
		if (realtime_fps >= 297 && realtime_fps <= 305)
			set_max_framerate(296, 0);
		else if (realtime_fps >= 147 && realtime_fps <= 150)
			set_max_framerate(146, 0);
		else
			write_cmos_sensor(0x0340, imgsensor.frame_length & 0xfffe);
	} else
		write_cmos_sensor(0x0340, imgsensor.frame_length & 0xfffe);

	/* Update Shutter */
	write_cmos_sensor(0x0202, shutter);

	LOG_INF("shutter = %d, framelength = %d\n",
		shutter, imgsensor.frame_length);
}

static kal_uint16 gain2reg(kal_uint16 gain)
{
	kal_uint16 reg_gain = gain << 4;

	reg_gain = (reg_gain < SENSOR_BASE_GAIN) ? SENSOR_BASE_GAIN : reg_gain;
	reg_gain = (reg_gain > SENSOR_MAX_GAIN) ? SENSOR_MAX_GAIN : reg_gain;

	return reg_gain;
}

static kal_uint16 set_gain(kal_uint16 gain)
{
	kal_uint32 reg_gain = 0;

	reg_gain = gain2reg(gain);
	LOG_INF("gain = %d, reg_gain = %d\n", gain, reg_gain);
	write_cmos_sensor(0x0204, reg_gain & 0xffff);

	return gain;
}

static void ihdr_write_shutter_gain(kal_uint16 le, kal_uint16 se, kal_uint16 gain)
{
	LOG_INF("le: 0x%x, se: 0x%x, gain: 0x%x\n", le, se, gain);
}

static void night_mode(kal_bool enable)
{
	/* No Need to implement this function */
}

kal_uint16 gc08a3_init_addr_data[] = {
/*system*/
	0x031c, 0x60,
	0x0337, 0x04,
	0x0335, 0x51,
	0x0336, 0x67,
	0x0383, 0xbb,
	0x031a, 0x00,
	0x0321, 0x10,
	0x0327, 0x05,
	0x0325, 0x40,
	0x0326, 0x36,
	0x0314, 0x11,
	0x0315, 0xd6,
	0x0316, 0x01,
	0x0334, 0x40,
	0x0324, 0x42,
	0x031c, 0x00,
	0x031c, 0x9f,
	0x039a, 0x13,
	0x0084, 0x30,
	0x02b3, 0x08,
	0x0057, 0x0c,
	0x05c3, 0x50,
	0x0311, 0x90,
	0x05a0, 0x02,
	0x0074, 0x0a,
	0x0059, 0x11,
	0x0070, 0x05,
	0x0101, GC08A3_MIRROR,
/*analog*/
	0x0344, 0x00,
	0x0345, 0x06,
	0x0346, 0x00,
	0x0347, 0x04,
	0x0348, 0x0c,
	0x0349, 0xd0,
	0x034a, 0x09,
	0x034b, 0x9c,
	0x0202, 0x08,
	0x0203, 0xf6,
	0x0340, 0x09,
	0x0341, 0xf4,
	0x0342, 0x07,
	0x0343, 0x2c,
	0x0219, 0x05,
	0x0226, 0x00,
	0x0227, 0x28,
	0x0e0a, 0x00,
	0x0e0b, 0x00,
	0x0e24, 0x04,
	0x0e25, 0x04,
	0x0e26, 0x00,
	0x0e27, 0x10,
	0x0e01, 0x74,
	0x0e03, 0x47,
	0x0e04, 0x33,
	0x0e05, 0x44,
	0x0e06, 0x44,
	0x0e0c, 0x1e,
	0x0e17, 0x3a,
	0x0e18, 0x3c,
	0x0e19, 0x40,
	0x0e1a, 0x42,
	0x0e28, 0x21,
	0x0e2b, 0x68,
	0x0e2c, 0x0d,
	0x0e2d, 0x08,
	0x0e34, 0xf4,
	0x0e35, 0x44,
	0x0e36, 0x07,
	0x0e38, 0x49,
	0x0210, 0x13,
	0x0218, 0x00,
	0x0241, 0x88,
	0x0e32, 0x00,
	0x0e33, 0x18,
	0x0e42, 0x03,
	0x0e43, 0x80,
	0x0e44, 0x04,
	0x0e45, 0x00,
	0x0e4f, 0x04,
	0x057a, 0x20,
	0x0381, 0x7c,
	0x0382, 0x9b,
	0x0384, 0xfb,
	0x0389, 0x38,
	0x038a, 0x03,
	0x0390, 0x6a,
	0x0391, 0x0b,
	0x0392, 0x60,
	0x0393, 0xc1,
	0x0396, 0xff,
	0x0398, 0x62,
/*cisctl reset*/
	0x031c, 0x80,
	0x03fe, 0x10,
	0x03fe, 0x00,
	0x031c, 0x9f,
	0x03fe, 0x00,
	0x03fe, 0x00,
	0x03fe, 0x00,
	0x03fe, 0x00,
	0x031c, 0x80,
	0x03fe, 0x10,
	0x03fe, 0x00,
	0x031c, 0x9f,
	0x0360, 0x01,
	0x0360, 0x00,
	0x0316, 0x09,
	0x0a67, 0x80,
	0x0313, 0x00,
	0x0a53, 0x0e,
	0x0a65, 0x17,
	0x0a68, 0xa1,
	0x0a58, 0x00,
	0x0ace, 0x0c,
	0x00a4, 0x00,
	0x00a5, 0x01,
	0x00a7, 0x09,
	0x00a8, 0x9c,
	0x00a9, 0x0c,
	0x00aa, 0xd0,
	0x0a8a, 0x00,
	0x0a8b, 0xe0,
	0x0a8c, 0x13,
	0x0a8d, 0xe8,
	0x0a90, 0x0a,
	0x0a91, 0x10,
	0x0a92, 0xf8,
	0x0a71, 0xf2,
	0x0a72, 0x12,
	0x0a73, 0x64,
	0x0a75, 0x41,
	0x0a70, 0x07,
	0x0313, 0x80,
/*ISP*/
	0x00a0, 0x01,
	0x0080, 0xd2,
	0x0081, 0x3f,
	0x0087, 0x51,
	0x0089, 0x03,
	0x009b, 0x40,
	0x05a0, 0x82,
	0x05ac, 0x00,
	0x05ad, 0x01,
	0x05ae, 0x00,
	0x0800, 0x0a,
	0x0801, 0x14,
	0x0802, 0x28,
	0x0803, 0x34,
	0x0804, 0x0e,
	0x0805, 0x33,
	0x0806, 0x03,
	0x0807, 0x8a,
	0x0808, 0x50,
	0x0809, 0x00,
	0x080a, 0x34,
	0x080b, 0x03,
	0x080c, 0x26,
	0x080d, 0x03,
	0x080e, 0x18,
	0x080f, 0x03,
	0x0810, 0x10,
	0x0811, 0x03,
	0x0812, 0x00,
	0x0813, 0x00,
	0x0814, 0x01,
	0x0815, 0x00,
	0x0816, 0x01,
	0x0817, 0x00,
	0x0818, 0x00,
	0x0819, 0x0a,
	0x081a, 0x01,
	0x081b, 0x6c,
	0x081c, 0x00,
	0x081d, 0x0b,
	0x081e, 0x02,
	0x081f, 0x00,
	0x0820, 0x00,
	0x0821, 0x0c,
	0x0822, 0x02,
	0x0823, 0xd9,
	0x0824, 0x00,
	0x0825, 0x0d,
	0x0826, 0x03,
	0x0827, 0xf0,
	0x0828, 0x00,
	0x0829, 0x0e,
	0x082a, 0x05,
	0x082b, 0x94,
	0x082c, 0x09,
	0x082d, 0x6e,
	0x082e, 0x07,
	0x082f, 0xe6,
	0x0830, 0x10,
	0x0831, 0x0e,
	0x0832, 0x0b,
	0x0833, 0x2c,
	0x0834, 0x14,
	0x0835, 0xae,
	0x0836, 0x0f,
	0x0837, 0xc4,
	0x0838, 0x18,
	0x0839, 0x0e,
	0x05ac, 0x01,
	0x059a, 0x00,
	0x059b, 0x00,
	0x059c, 0x01,
	0x0598, 0x00,
	0x0597, 0x14,
	0x05ab, 0x09,
	0x05a4, 0x02,
	0x05a3, 0x05,
	0x05a0, 0xc2,
	0x0207, 0xc4,
/*GAIN*/
	0x0208, 0x01,
	0x0209, 0x72,
	0x0204, 0x04,
	0x0205, 0x00,
	0x0040, 0x22,
	0x0041, 0x20,
	0x0043, 0x10,
	0x0044, 0x00,
	0x0046, 0x08,
	0x0047, 0xf0,
	0x0048, 0x0f,
	0x004b, 0x0f,
	0x004c, 0x00,
	0x0050, 0x5c,
	0x0051, 0x44,
	0x005b, 0x03,
	0x00c0, 0x00,
	0x00c1, 0x80,
	0x00c2, 0x31,
	0x00c3, 0x00,
	0x0460, 0x04,
	0x0462, 0x08,
	0x0464, 0x0e,
	0x0466, 0x0a,
	0x0468, 0x12,
	0x046a, 0x12,
	0x046c, 0x10,
	0x046e, 0x0c,
	0x0461, 0x03,
	0x0463, 0x03,
	0x0465, 0x03,
	0x0467, 0x03,
	0x0469, 0x04,
	0x046b, 0x04,
	0x046d, 0x04,
	0x046f, 0x04,
	0x0470, 0x04,
	0x0472, 0x10,
	0x0474, 0x26,
	0x0476, 0x38,
	0x0478, 0x20,
	0x047a, 0x30,
	0x047c, 0x38,
	0x047e, 0x60,
	0x0471, 0x05,
	0x0473, 0x05,
	0x0475, 0x05,
	0x0477, 0x05,
	0x0479, 0x04,
	0x047b, 0x04,
	0x047d, 0x04,
	0x047f, 0x04,
};

static kal_uint16 gc08a3_1632x1224_addr_data[] = {
/*system*/
	0x031c, 0x60,
	0x0337, 0x04,
	0x0335, 0x55,
	0x0336, 0x6a,
	0x0383, 0xbb,
	0x031a, 0x00,
	0x0321, 0x10,
	0x0327, 0x05,
	0x0325, 0x40,
	0x0326, 0x36,
	0x0314, 0x11,
	0x0315, 0xd6,
	0x0316, 0x01,
	0x0334, 0x40,
	0x0324, 0x42,
	0x031c, 0x00,
	0x031c, 0x9f,
	0x0344, 0x00,
	0x0345, 0x06,
	0x0346, 0x00,
	0x0347, 0x04,
	0x0348, 0x0c,
	0x0349, 0xd0,
	0x034a, 0x09,
	0x034b, 0x9c,
	0x0202, 0x02,
	0x0203, 0xfe,
	0x0340, 0x09,
	0x0341, 0xec,
	0x0342, 0x07,
	0x0343, 0x2c,
	0x0226, 0x04,
	0x0227, 0xf6,
	0x0e38, 0x49,
	0x0210, 0x53,
	0x0218, 0x80,
	0x0241, 0x8c,
	0x0392, 0x3b,
/*ISP*/
	0x031c, 0x80,
	0x03fe, 0x10,
	0x03fe, 0x00,
	0x031c, 0x9f,
	0x03fe, 0x00,
	0x03fe, 0x00,
	0x03fe, 0x00,
	0x03fe, 0x00,
	0x031c, 0x80,
	0x03fe, 0x10,
	0x03fe, 0x00,
	0x031c, 0x9f,
	0x00a2, 0x00,
	0x00a3, 0x00,
	0x00ab, 0x00,
	0x00ac, 0x00,
	0x05a0, 0x82,
	0x05ac, 0x00,
	0x05ad, 0x01,
	0x05ae, 0x00,
	0x0800, 0x0a,
	0x0801, 0x14,
	0x0802, 0x28,
	0x0803, 0x34,
	0x0804, 0x0e,
	0x0805, 0x33,
	0x0806, 0x03,
	0x0807, 0x8a,
	0x0808, 0x50,
	0x0809, 0x00,
	0x080a, 0x34,
	0x080b, 0x03,
	0x080c, 0x26,
	0x080d, 0x03,
	0x080e, 0x18,
	0x080f, 0x03,
	0x0810, 0x10,
	0x0811, 0x03,
	0x0812, 0x00,
	0x0813, 0x00,
	0x0814, 0x01,
	0x0815, 0x00,
	0x0816, 0x01,
	0x0817, 0x00,
	0x0818, 0x00,
	0x0819, 0x0a,
	0x081a, 0x01,
	0x081b, 0x6c,
	0x081c, 0x00,
	0x081d, 0x0b,
	0x081e, 0x02,
	0x081f, 0x00,
	0x0820, 0x00,
	0x0821, 0x0c,
	0x0822, 0x02,
	0x0823, 0xd9,
	0x0824, 0x00,
	0x0825, 0x0d,
	0x0826, 0x03,
	0x0827, 0xf0,
	0x0828, 0x00,
	0x0829, 0x0e,
	0x082a, 0x05,
	0x082b, 0x94,
	0x082c, 0x09,
	0x082d, 0x6e,
	0x082e, 0x07,
	0x082f, 0xe6,
	0x0830, 0x10,
	0x0831, 0x0e,
	0x0832, 0x0b,
	0x0833, 0x2c,
	0x0834, 0x14,
	0x0835, 0xae,
	0x0836, 0x0f,
	0x0837, 0xc4,
	0x0838, 0x18,
	0x0839, 0x0e,
	0x05ac, 0x01,
	0x059a, 0x00,
	0x059b, 0x00,
	0x059c, 0x01,
	0x0598, 0x00,
	0x0597, 0x14,
	0x05ab, 0x09,
	0x05a4, 0x02,
	0x05a3, 0x05,
	0x05a0, 0xc2,
	0x0207, 0xc4,
/*GAIN*/
	0x0204, 0x04,
	0x0205, 0x00,
	0x0050, 0x5c,
	0x0051, 0x44,
/*out window*/
	0x009a, 0x00,
	0x0351, 0x00,
	0x0352, 0x04,
	0x0353, 0x00,
	0x0354, 0x04,
	0x034c, 0x06,
	0x034d, 0x60,
	0x034e, 0x04,
	0x034f, 0xc8,
/*MIPI*/
	0x0114, 0x03,
	0x0180, 0x64,//+S96818AA1-1936,wuwenhao2.wt,Modify,2023/05/11, Improve mipi transmission ability
	0x0181, 0xf0,
	0x0185, 0x01,
	0x0115, 0x30,
	0x011b, 0x12,
	0x011c, 0x12,
	0x0121, 0x02,
	0x0122, 0x03,
	0x0123, 0x09,
	0x0124, 0x00,
	0x0125, 0x08,
	0x0126, 0x05,
	0x0129, 0x03,
	0x012a, 0x02,
	0x012b, 0x05,
	0x0a73, 0x60,
	0x0a70, 0x11,
	0x0313, 0x80,
	0x0aff, 0x00,
	0x0aff, 0x00,
	0x0aff, 0x00,
	0x0aff, 0x00,
	0x0aff, 0x00,
	0x0aff, 0x00,
	0x0aff, 0x00,
	0x0aff, 0x00,
	0x0a70, 0x00,
	0x00a4, 0x80,
	0x0316, 0x01,
	0x0a67, 0x00,
	0x0084, 0x10,
	0x0102, 0x09,
};

static kal_uint16 gc08a3_3264x2448_addr_data[] = {
/*system*/
	0x031c, 0x60,
	0x0337, 0x04,
	0x0335, 0x51,
	0x0336, 0x68,
	0x0383, 0xbb,
	0x031a, 0x00,
	0x0321, 0x10,
	0x0327, 0x05,
	0x0325, 0x40,
	0x0326, 0x36,
	0x0314, 0x11,
	0x0315, 0xd6,
	0x0316, 0x01,
	0x0334, 0x40,
	0x0324, 0x42,
	0x031c, 0x00,
	0x031c, 0x9f,
	0x0344, 0x00,
	0x0345, 0x06,
	0x0346, 0x00,
	0x0347, 0x04,
	0x0348, 0x0c,
	0x0349, 0xd0,
	0x034a, 0x09,
	0x034b, 0x9c,
	0x0202, 0x08,
	0x0203, 0xf6,
	0x0340, 0x09,
	0x0341, 0xf4,
	0x0342, 0x07,
	0x0343, 0x2c,
	0x0226, 0x00,
	0x0227, 0x28,
	0x0e38, 0x49,
	0x0210, 0x13,
	0x0218, 0x00,
	0x0241, 0x88,
	0x0392, 0x60,
/*ISP*/
	0x031c, 0x80,
	0x03fe, 0x10,
	0x03fe, 0x00,
	0x031c, 0x9f,
	0x03fe, 0x00,
	0x03fe, 0x00,
	0x03fe, 0x00,
	0x03fe, 0x00,
	0x031c, 0x80,
	0x03fe, 0x10,
	0x03fe, 0x00,
	0x031c, 0x9f,
	0x00a2, 0x00,
	0x00a3, 0x00,
	0x00ab, 0x00,
	0x00ac, 0x00,
	0x05a0, 0x82,
	0x05ac, 0x00,
	0x05ad, 0x01,
	0x05ae, 0x00,
	0x0800, 0x0a,
	0x0801, 0x14,
	0x0802, 0x28,
	0x0803, 0x34,
	0x0804, 0x0e,
	0x0805, 0x33,
	0x0806, 0x03,
	0x0807, 0x8a,
	0x0808, 0x50,
	0x0809, 0x00,
	0x080a, 0x34,
	0x080b, 0x03,
	0x080c, 0x26,
	0x080d, 0x03,
	0x080e, 0x18,
	0x080f, 0x03,
	0x0810, 0x10,
	0x0811, 0x03,
	0x0812, 0x00,
	0x0813, 0x00,
	0x0814, 0x01,
	0x0815, 0x00,
	0x0816, 0x01,
	0x0817, 0x00,
	0x0818, 0x00,
	0x0819, 0x0a,
	0x081a, 0x01,
	0x081b, 0x6c,
	0x081c, 0x00,
	0x081d, 0x0b,
	0x081e, 0x02,
	0x081f, 0x00,
	0x0820, 0x00,
	0x0821, 0x0c,
	0x0822, 0x02,
	0x0823, 0xd9,
	0x0824, 0x00,
	0x0825, 0x0d,
	0x0826, 0x03,
	0x0827, 0xf0,
	0x0828, 0x00,
	0x0829, 0x0e,
	0x082a, 0x05,
	0x082b, 0x94,
	0x082c, 0x09,
	0x082d, 0x6e,
	0x082e, 0x07,
	0x082f, 0xe6,
	0x0830, 0x10,
	0x0831, 0x0e,
	0x0832, 0x0b,
	0x0833, 0x2c,
	0x0834, 0x14,
	0x0835, 0xae,
	0x0836, 0x0f,
	0x0837, 0xc4,
	0x0838, 0x18,
	0x0839, 0x0e,
	0x05ac, 0x01,
	0x059a, 0x00,
	0x059b, 0x00,
	0x059c, 0x01,
	0x0598, 0x00,
	0x0597, 0x14,
	0x05ab, 0x09,
	0x05a4, 0x02,
	0x05a3, 0x05,
	0x05a0, 0xc2,
	0x0207, 0xc4,
/*GAIN*/
	0x0204, 0x04,
	0x0205, 0x00,
	0x0050, 0x5c,
	0x0051, 0x44,
/*out window*/
	0x009a, 0x00,
	0x0351, 0x00,
	0x0352, 0x06,
	0x0353, 0x00,
	0x0354, 0x08,
	0x034c, 0x0c,
	0x034d, 0xc0,
	0x034e, 0x09,
	0x034f, 0x90,
/*MIPI*/
	0x0114, 0x03,
	0x0180, 0x64,//+S96818AA1-1936,wuwenhao2.wt,Modify,2023/05/11, Improve mipi transmission ability
	0x0181, 0xf0,
	0x0185, 0x01,
	0x0115, 0x30,
	0x011b, 0x12,
	0x011c, 0x12,
	0x0121, 0x06,
	0x0122, 0x06,
	0x0123, 0x15,
	0x0124, 0x01,
	0x0125, 0x0b,
	0x0126, 0x08,
	0x0129, 0x06,
	0x012a, 0x08,
	0x012b, 0x08,
	0x0a73, 0x60,
	0x0a70, 0x11,
	0x0313, 0x80,
	0x0aff, 0x00,
	0x0aff, 0x00,
	0x0aff, 0x00,
	0x0aff, 0x00,
	0x0aff, 0x00,
	0x0aff, 0x00,
	0x0aff, 0x00,
	0x0aff, 0x00,
	0x0a70, 0x00,
	0x00a4, 0x80,
	0x0316, 0x01,
	0x0a67, 0x00,
	0x0084, 0x10,
	0x0102, 0x09,
};

static kal_uint16 gc08a3_1280x720_addr_data[] = {
/*system*/
	0x031c, 0x60,
	0x0337, 0x04,
	0x0335, 0x55,
	0x0336, 0x6a,
	0x0383, 0xbb,
	0x031a, 0x00,
	0x0321, 0x10,
	0x0327, 0x05,
	0x0325, 0x40,
	0x0326, 0x36,
	0x0314, 0x11,
	0x0315, 0xd6,
	0x0316, 0x01,
	0x0334, 0x40,
	0x0324, 0x42,
	0x031c, 0x00,
	0x031c, 0x9f,
	0x0344, 0x01,
	0x0345, 0x66,
	0x0346, 0x01,
	0x0347, 0xfc,
	0x0348, 0x0a,
	0x0349, 0x10,
	0x034a, 0x05,
	0x034b, 0xac,
	0x0202, 0x02,
	0x0203, 0xfe,
	0x0340, 0x09,
	0x0341, 0xf4,
	0x0342, 0x07,
	0x0343, 0x2c,
	0x0226, 0x00,
	0x0227, 0x56,
	0x0e38, 0x49,
	0x0210, 0x53,
	0x0218, 0x80,
	0x0241, 0x8c,
	0x0392, 0x3b,
/*ISP*/
	0x031c, 0x80,
	0x03fe, 0x10,
	0x03fe, 0x00,
	0x031c, 0x9f,
	0x03fe, 0x00,
	0x03fe, 0x00,
	0x03fe, 0x00,
	0x03fe, 0x00,
	0x031c, 0x80,
	0x03fe, 0x10,
	0x03fe, 0x00,
	0x031c, 0x9f,
	0x00a2, 0xf8,
	0x00a3, 0x01,
	0x00ab, 0x60,
	0x00ac, 0x01,
	0x05a0, 0x82,
	0x05ac, 0x00,
	0x05ad, 0x01,
	0x05ae, 0x00,
	0x0800, 0x0a,
	0x0801, 0x14,
	0x0802, 0x28,
	0x0803, 0x34,
	0x0804, 0x0e,
	0x0805, 0x33,
	0x0806, 0x03,
	0x0807, 0x8a,
	0x0808, 0x50,
	0x0809, 0x00,
	0x080a, 0x34,
	0x080b, 0x03,
	0x080c, 0x26,
	0x080d, 0x03,
	0x080e, 0x18,
	0x080f, 0x03,
	0x0810, 0x10,
	0x0811, 0x03,
	0x0812, 0x00,
	0x0813, 0x00,
	0x0814, 0x01,
	0x0815, 0x00,
	0x0816, 0x01,
	0x0817, 0x00,
	0x0818, 0x00,
	0x0819, 0x0a,
	0x081a, 0x01,
	0x081b, 0x6c,
	0x081c, 0x00,
	0x081d, 0x0b,
	0x081e, 0x02,
	0x081f, 0x00,
	0x0820, 0x00,
	0x0821, 0x0c,
	0x0822, 0x02,
	0x0823, 0xd9,
	0x0824, 0x00,
	0x0825, 0x0d,
	0x0826, 0x03,
	0x0827, 0xf0,
	0x0828, 0x00,
	0x0829, 0x0e,
	0x082a, 0x05,
	0x082b, 0x94,
	0x082c, 0x09,
	0x082d, 0x6e,
	0x082e, 0x07,
	0x082f, 0xe6,
	0x0830, 0x10,
	0x0831, 0x0e,
	0x0832, 0x0b,
	0x0833, 0x2c,
	0x0834, 0x14,
	0x0835, 0xae,
	0x0836, 0x0f,
	0x0837, 0xc4,
	0x0838, 0x18,
	0x0839, 0x0e,
	0x05ac, 0x01,
	0x059a, 0x00,
	0x059b, 0x00,
	0x059c, 0x01,
	0x0598, 0x00,
	0x0597, 0x14,
	0x05ab, 0x09,
	0x05a4, 0x02,
	0x05a3, 0x05,
	0x05a0, 0xc2,
	0x0207, 0xc4,
/*GAIN*/
	0x0204, 0x04,
	0x0205, 0x00,
	0x0050, 0x48,
	0x0051, 0x30,
/*out window*/
	0x009a, 0x00,
	0x0351, 0x00,
	0x0352, 0x04,
	0x0353, 0x00,
	0x0354, 0x04,
	0x034c, 0x05,
	0x034d, 0x00,
	0x034e, 0x02,
	0x034f, 0xd0,
/*MIPI*/
	0x0114, 0x03,
	0x0180, 0x64,//+S96818AA1-1936,wuwenhao2.wt,Modify,2023/05/11, Improve mipi transmission ability
	0x0181, 0xf0,
	0x0185, 0x01,
	0x0115, 0x30,
	0x011b, 0x12,
	0x011c, 0x12,
	0x0121, 0x01,
	0x0122, 0x02,
	0x0123, 0x07,
	0x0124, 0x00,
	0x0125, 0x07,
	0x0126, 0x04,
	0x0129, 0x02,
	0x012a, 0x01,
	0x012b, 0x04,
	0x0a73, 0x60,
	0x0a70, 0x11,
	0x0313, 0x80,
	0x0aff, 0x00,
	0x0aff, 0x00,
	0x0aff, 0x00,
	0x0aff, 0x00,
	0x0aff, 0x00,
	0x0aff, 0x00,
	0x0aff, 0x00,
	0x0aff, 0x00,
	0x0a70, 0x00,
	0x00a4, 0x80,
	0x0316, 0x01,
	0x0a67, 0x00,
	0x0084, 0x10,
	0x0102, 0x09,
};


static void gc08a3_stream_on(void)
{
	write_cmos_sensor_8bit(0x0100, 0x01);
}

static void gc08a3_stream_off(void)
{
	write_cmos_sensor_8bit(0x0100, 0x00);
}


static void sensor_init(void)
{
	table_write_cmos_sensor(gc08a3_init_addr_data,
		sizeof(gc08a3_init_addr_data)/sizeof(kal_uint16));
}

static void preview_setting(void)
{
	gc08a3_stream_off();
	table_write_cmos_sensor(gc08a3_1632x1224_addr_data,
		sizeof(gc08a3_1632x1224_addr_data)/sizeof(kal_uint16));
	gc08a3_stream_on();
}

static void capture_setting(void)
{
	gc08a3_stream_off();
	table_write_cmos_sensor(gc08a3_3264x2448_addr_data,
		sizeof(gc08a3_3264x2448_addr_data)/sizeof(kal_uint16));
	gc08a3_stream_on();
}

static void normal_video_setting(void)
{
	gc08a3_stream_off();
	table_write_cmos_sensor(gc08a3_3264x2448_addr_data,
		sizeof(gc08a3_3264x2448_addr_data)/sizeof(kal_uint16));
	gc08a3_stream_on();
}

static void hs_video_setting(void)
{
	gc08a3_stream_off();
	table_write_cmos_sensor(gc08a3_1632x1224_addr_data,
		sizeof(gc08a3_1632x1224_addr_data)/sizeof(kal_uint16));
	gc08a3_stream_on();
}

static void slim_video_setting(void)
{
	gc08a3_stream_off();
	table_write_cmos_sensor(gc08a3_1280x720_addr_data,
		sizeof(gc08a3_1280x720_addr_data)/sizeof(kal_uint16));
	gc08a3_stream_on();
}

static kal_uint32 set_test_pattern_mode(kal_bool enable)
{
	LOG_INF("enable: %d\n", enable);

	if (enable)
		write_cmos_sensor_8bit(0x008c, 0x01);
	else
		write_cmos_sensor_8bit(0x008c, 0x00);

	spin_lock(&imgsensor_drv_lock);
	imgsensor.test_pattern = enable;
	spin_unlock(&imgsensor_drv_lock);
	return ERROR_NONE;
}

//+S96818AA1-1936,wuwenhao2.wt,ADD,2023/04/27,gc08a3 otp porting
#if GC08A3_OTP_FUNCTION
static void gc08a3_otp_init(void)
{
	write_cmos_sensor_8bit(0x0315, 0x80);
	write_cmos_sensor_8bit(0x031c, 0x60);

	write_cmos_sensor_8bit(0x0324, 0x42);
	write_cmos_sensor_8bit(0x0316, 0x09);
	write_cmos_sensor_8bit(0x0a67, 0x80);
	write_cmos_sensor_8bit(0x0313, 0x00);
	write_cmos_sensor_8bit(0x0a53, 0x0e);
	write_cmos_sensor_8bit(0x0a65, 0x17);
	write_cmos_sensor_8bit(0x0a68, 0xa1);
	write_cmos_sensor_8bit(0x0a47, 0x00);
	write_cmos_sensor_8bit(0x0a58, 0x00);
	write_cmos_sensor_8bit(0x0ace, 0x0c);
}
static void gc08a3_otp_close(void)
{
	write_cmos_sensor_8bit(0x0316, 0x01);
	write_cmos_sensor_8bit(0x0a67, 0x00);
}
static u16 gc08a3_otp_read_group(u16 addr, u8 *data, u16 length)
{
	u16 i = 0;

	write_cmos_sensor_8bit(0x0313, 0x00);
	write_cmos_sensor_8bit(0x0a69, (addr >> 8) & 0xff);
	write_cmos_sensor_8bit(0x0a6a, addr & 0xff);
	write_cmos_sensor_8bit(0x0313, 0x20);
	write_cmos_sensor_8bit(0x0313, 0x12);

	for (i = 0; i < length; i++) {
		data[i] = read_cmos_sensor(0x0a6c);
	    //LOG_INF("addr = 0x%x, data = 0x%x\n", addr + i * 8, data[i]);
	}
	return 0;
}
static kal_uint16 read_cmos_sensor_otp(u16 addr)
{
	write_cmos_sensor_8bit(0x0313, 0x00);
	write_cmos_sensor_8bit(0x0a69, (addr >> 8) & 0xff);
	write_cmos_sensor_8bit(0x0a6a, addr & 0xff);
	write_cmos_sensor_8bit(0x0313, 0x20);
	write_cmos_sensor_8bit(0x0313, 0x12);
	return read_cmos_sensor(0x0a6c);
}

static int read_gc08a3_module_info(void)
{
	int otp_grp_flag = 0, minfo_start_addr = 0;
	int year = 0, month = 0, day = 0;
	int position = 0,lens_id = 0,vcm_id = 0,gc08a3_module_id = 0;
	int check_sum = 0, check_sum_cal = 0;
	int i;

	otp_grp_flag = read_cmos_sensor_otp(MODULE_INFO_FLAG);
	LOG_INF("module info otp_grp_flag = 0x%x",otp_grp_flag);

	/* select group */
	if(otp_grp_flag == 0x01)
		minfo_start_addr = MODULE_GROUP1_ADDR;
	else if(otp_grp_flag == 0x07)
		minfo_start_addr = MODULE_GROUP2_ADDR;
	else if(otp_grp_flag == 0x1f)
		minfo_start_addr = MODULE_GROUP3_ADDR;
	else {
		LOG_INF("no module info OTP gc08a3_data_info\n");
		return 0;
	}

	gc08a3_otp_read_group(minfo_start_addr,gc08a3_data_info,MODULE_INFO_SIZE + 1);

	for(i = 0; i <  MODULE_INFO_SIZE ; i++){
		check_sum_cal += gc08a3_data_info[i];
	}

	check_sum_cal = (check_sum_cal % 255) + 1;
	gc08a3_module_id = gc08a3_data_info[0];
	position = gc08a3_data_info[1];
	lens_id = gc08a3_data_info[2];
	vcm_id = gc08a3_data_info[3];
	year = gc08a3_data_info[4];
	month = gc08a3_data_info[5];
	day = gc08a3_data_info[6];
	check_sum = gc08a3_data_info[MODULE_INFO_SIZE];

	//LOG_INF("module_id=0x%x position=0x%x\n", gc08a3_module_id, position);
	LOG_INF("=== GC08A3 INFO module_id=0x%x position=0x%x ===\n", gc08a3_module_id, position);
	LOG_INF("=== GC08A3 INFO lens_id=0x%x,vcm_id=0x%x ===\n",lens_id, vcm_id);
	LOG_INF("=== GC08A3 INFO date is %d-%d-%d ===\n",year,month,day);
	LOG_INF("=== GC08A3 INFO check_sum=0x%x,check_sum_cal=0x%x ===\n", check_sum, check_sum_cal);
	if(check_sum == check_sum_cal){
		LOG_INF("=== GC08A3 INFO date sucess!\n");
		return 1;
	}else{
		return 0;
	}
}

static int read_gc08a3_sn_info(void)
{
	int otp_grp_flag = 0, minfo_start_addr = 0;
	int check_sum = 0, check_sum_cal = 0;
	int i;


	otp_grp_flag = read_cmos_sensor_otp(SN_INFO_FLAG);
	LOG_INF("sn info otp_grp_flag = 0x%x",otp_grp_flag);

	/* select group */
	if(otp_grp_flag == 0x01)
		minfo_start_addr = SN_GROUP1_ADDR;
	else if(otp_grp_flag == 0x07)
		minfo_start_addr = SN_GROUP2_ADDR;
	else if(otp_grp_flag == 0x1f)
		minfo_start_addr = SN_GROUP3_ADDR;
	else {
		LOG_INF("no sn info OTP gc08a3_data_info\n");
		return 0;
	}

	gc08a3_otp_read_group(minfo_start_addr,gc08a3_data_sn,SN_INFO_SIZE + 1);

	for(i = 0; i <  SN_INFO_SIZE ; i++){
		check_sum_cal += gc08a3_data_sn[i];
		LOG_INF("sn info gc08a3_data_sn[%d] = 0x%x",i,gc08a3_data_sn[i]);
	}
	check_sum_cal = (check_sum_cal % 255) + 1;
	check_sum=gc08a3_data_sn[SN_INFO_SIZE];
	LOG_INF("=== GC08A3 SN check_sum=0x%x,check_sum_cal=0x%x ===\n", check_sum, check_sum_cal);
	if(check_sum == check_sum_cal){
		LOG_INF("=== GC08A3 SN date sucess!\n");
		return 1;
	}else{
		return 0;
	}
}
static int read_gc08a3_awb_info(void)
{
	int awb_grp_flag = 0, awb_start_addr=0;
	int check_sum_awb = 0, check_sum_awb_cal = 0;
	int r = 0,b = 0,gr = 0, gb = 0, golden_r = 0, golden_b = 0, golden_gr = 0, golden_gb = 0;
	int i;

	/* read flag */
	awb_grp_flag = read_cmos_sensor_otp(AWB_INFO_FLAG);
	LOG_INF("awb awb_grp_flag = 0x%x",awb_grp_flag);

	/* select group */
	if(awb_grp_flag == 0x01)
		awb_start_addr = AWB_GROUP1_FLAG;
	else if(awb_grp_flag == 0x07)
		awb_start_addr = AWB_GROUP2_FLAG;
	else if(awb_grp_flag == 0x1f)
		awb_start_addr = AWB_GROUP3_FLAG;
	else {
		LOG_INF("no awb OTP gc08a3_data_info\n");
		return 0;
	}

	/* read & checksum */
	//check_sum_awb_cal += awb_grp_flag;

	gc08a3_otp_read_group(awb_start_addr,gc08a3_data_awb,AWB_INFO_SIZE + 1);
	for(i = 0; i < AWB_INFO_SIZE; i++){
		check_sum_awb_cal += gc08a3_data_awb[i];
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
	r = ((gc08a3_data_awb[1]<<8)&0xff00)|(gc08a3_data_awb[0]&0xff);
	b = ((gc08a3_data_awb[3]<<8)&0xff00)|(gc08a3_data_awb[2]&0xff);
	gr = ((gc08a3_data_awb[5]<<8)&0xff00)|(gc08a3_data_awb[4]&0xff);
	gb = ((gc08a3_data_awb[7]<<8)&0xff00)|(gc08a3_data_awb[6]&0xff);
	golden_r = ((gc08a3_data_awb[9]<<8)&0xff00)|(gc08a3_data_awb[8]&0xff);
	golden_b = ((gc08a3_data_awb[11]<<8)&0xff00)|(gc08a3_data_awb[10]&0xff);
	golden_gr = ((gc08a3_data_awb[13]<<8)&0xff00)|(gc08a3_data_awb[12]&0xff);
	golden_gb = ((gc08a3_data_awb[15]<<8)&0xff00)|(gc08a3_data_awb[14]&0xff);
	check_sum_awb = gc08a3_data_awb[AWB_INFO_SIZE];
	check_sum_awb_cal = (check_sum_awb_cal % 255) + 1;

#endif

	LOG_INF("=== GC08A3 AWB r=0x%x, b=0x%x, gr=%x, gb=0x%x ===\n", r, b,gb, gr);
	LOG_INF("=== GC08A3 AWB gr=0x%x,gb=0x%x,gGr=%x, gGb=0x%x ===\n", golden_r, golden_b, golden_gr, golden_gb);
	LOG_INF("=== GC08A3 AWB check_sum_awb=0x%x,check_sum_awb_cal=0x%x ===\n",check_sum_awb,check_sum_awb_cal);
	if(check_sum_awb == check_sum_awb_cal){
		LOG_INF("=== GC08A3 AWB date sucess!\n");
		return 1;
	}else{
		return 0;
	}
}

static int read_gc08a3_lsc_info(void)
{
	int lsc_grp_flag = 0, lsc_start_addr = 0;
	int check_sum_lsc = 0, check_sum_lsc_cal = 0;
	int i;

	/* read flag */
	lsc_grp_flag = read_cmos_sensor_otp(LSC_INFO_FLAG);//1948
	LOG_INF("lsc lsc_grp_flag = 0x%x",lsc_grp_flag);

	/* select group */
	if(lsc_grp_flag == 0x01)
		lsc_start_addr = LSC_GROUP1_FLAG;//1950
	else if(lsc_grp_flag == 0x07)
		lsc_start_addr = LSC_GROUP2_FLAG;//53b8
	else if(lsc_grp_flag == 0x1f)
		lsc_start_addr = LSC_GROUP3_FLAG;//8E20
	else {
		LOG_INF("no lsc OTP gc08a3_data_info\n");
		return 0;
	}

	/* read & checksum */
	//check_sum_lsc_cal += lsc_grp_flag;

	gc08a3_otp_read_group(lsc_start_addr,gc08a3_data_lsc,LSC_INFO_SIZE + 1);

	for(i = 0; i < LSC_INFO_SIZE; i++){
		check_sum_lsc_cal += gc08a3_data_lsc[i];
	}
	LOG_INF("check_sum_lsc_cal =0x%x \n",check_sum_lsc_cal);
	check_sum_lsc = gc08a3_data_lsc[LSC_INFO_SIZE];
	check_sum_lsc_cal = (check_sum_lsc_cal % 255) + 1;

	LOG_INF("=== GC08A3 LSC check_sum_lsc=0x%x, check_sum_lsc_cal=0x%x ===\n", check_sum_lsc, check_sum_lsc_cal);
	if(check_sum_lsc == check_sum_lsc_cal){
		LOG_INF("=== GC08A3 LSC date sucess!\n");
		return 1;
	}else{
		return 0;
	}
}
static int gc0a83_sensor_otp_info(){
	int ret = 0, gc08a3_module_valid = 0, gc08a3_sn_valid = 0, gc08a3_awb_valid = 0,gc08a3_lsc_valid = 0;
	LOG_INF("come to %s:%d E!\n", __func__, __LINE__);

	gc08a3_otp_init();
	mdelay(10);

	ret = read_gc08a3_module_info();
	if(ret != 1){
		gc08a3_module_valid = 0;
		LOG_INF("=== gc08a3_data_info invalid ===\n");
	}else{
		gc08a3_module_valid = 1;
	}
	ret = read_gc08a3_sn_info();
	if(ret != 1){
		gc08a3_sn_valid = 0;
		LOG_INF("=== gc08a3_data_sn invalid ===\n");
	}else{
		gc08a3_sn_valid = 1;
	}
	ret = read_gc08a3_awb_info();
	if(ret != 1){
		gc08a3_awb_valid = 0;
		LOG_INF("=== gc08a3_data_awb invalid ===\n");
	}else{
		gc08a3_awb_valid = 1;
	}
	ret = read_gc08a3_lsc_info();
	if(ret != 1){
		gc08a3_lsc_valid = 0;
		LOG_INF("=== gc08a3_data_lsc invalid ===\n");
	}else{
		gc08a3_lsc_valid = 1;
	}

	gc08a3_otp_close();
	if(gc08a3_module_valid == 0 || gc08a3_awb_valid == 0 || gc08a3_lsc_valid == 0){
		return -1;
	}else{
		return 0;
	}
}


#include "cam_cal_define.h"
#include <linux/slab.h>
static struct stCAM_CAL_DATAINFO_STRUCT n26gc0a83frontly_eeprom_data ={
    .sensorID= N28GC08A3FRONTCXT_SENSOR_ID,
    .deviceID = 0x02,
    .dataLength = 0x076F,//1903
    .sensorVendorid = 0x16050000,
    .vendorByte = {1,2,3,4},
    .dataBuffer = NULL,
};

extern int imgSensorSetEepromData(struct stCAM_CAL_DATAINFO_STRUCT* pData);
#endif
//-S96818AA1-1936,wuwenhao2.wt,ADD,2023/04/27,gc08a3 otp porting

static kal_uint32 get_imgsensor_id(UINT32 *sensor_id)
{
	kal_uint8 i = 0;
	kal_uint8 retry = 2;
#if GC08A3_OTP_FUNCTION
	kal_uint8 ret = 0;
#endif
	while (imgsensor_info.i2c_addr_table[i] != 0xff) {
		spin_lock(&imgsensor_drv_lock);
		imgsensor.i2c_write_id = imgsensor_info.i2c_addr_table[i];
		spin_unlock(&imgsensor_drv_lock);
		do {
			*sensor_id = return_sensor_id();
			if (*sensor_id == imgsensor_info.sensor_id) {
//+S96818AA1-1936,wuwenhao2.wt,ADD,2023/04/27,gc08a3 otp porting
#if GC08A3_OTP_FUNCTION
			ret = gc0a83_sensor_otp_info();
			if(ret != 0){
				//*sensor_id = 0xFFFFFFFF;
				LOG_INF("gc0a83 read OTP failed ret:%d, \n",ret);
				//return ERROR_SENSOR_CONNECT_FAIL;
			}else{
				LOG_INF("gc0a83 read OTP successed: ret: %d \n",ret);
				n26gc0a83frontly_eeprom_data.dataBuffer = kmalloc(n26gc0a83frontly_eeprom_data.dataLength, GFP_KERNEL);
				if (n26gc0a83frontly_eeprom_data.dataBuffer == NULL) {
					LOG_INF("n26gc0a83frontly_eeprom_data->dataBuffer is malloc fail\n");
					return -EFAULT;
				}
				memcpy(n26gc0a83frontly_eeprom_data.dataBuffer, (u8 *)&gc08a3_data_info, MODULE_INFO_SIZE);
				memcpy(n26gc0a83frontly_eeprom_data.dataBuffer+MODULE_INFO_SIZE, (u8 *)&gc08a3_data_sn, SN_INFO_SIZE);
				memcpy(n26gc0a83frontly_eeprom_data.dataBuffer+SN_INFO_SIZE+MODULE_INFO_SIZE, (u8 *)&gc08a3_data_awb, AWB_INFO_SIZE);
				memcpy(n26gc0a83frontly_eeprom_data.dataBuffer+SN_INFO_SIZE+MODULE_INFO_SIZE+AWB_INFO_SIZE, (u8 *)&gc08a3_data_lsc, LSC_INFO_SIZE);
				imgSensorSetEepromData(&n26gc0a83frontly_eeprom_data);
			}
#endif
//-S96818AA1-1936,wuwenhao2.wt,ADD,2023/04/27,gc08a3 otp porting
				pr_err("[gc08a3_camera_sensor]get_imgsensor_id:i2c write id: 0x%x, sensor id: 0x%x\n",
					imgsensor.i2c_write_id, *sensor_id);
				return ERROR_NONE;
			}
			pr_err("[gc08a3_camera_sensor]get_imgsensor_id:Read sensor id fail, write id: 0x%x, id: 0x%x\n",
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
	LOG_INF("imgsensor_open\n");
	while (imgsensor_info.i2c_addr_table[i] != 0xff) {
		spin_lock(&imgsensor_drv_lock);
		imgsensor.i2c_write_id = imgsensor_info.i2c_addr_table[i];
		spin_unlock(&imgsensor_drv_lock);
		do {
			sensor_id = return_sensor_id();
			if (sensor_id == imgsensor_info.sensor_id) {
				pr_err("[gc08a3_camera_sensor]open:i2c write id: 0x%x, sensor id: 0x%x\n",
					imgsensor.i2c_write_id, sensor_id);
				break;
			}
			pr_err("[gc08a3_camera_sensor]open:Read sensor id fail, write id: 0x%x, id: 0x%x\n",
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
	/*imgsensor.current_fps = 300*/
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
	return ERROR_NONE;
}

static kal_uint32 get_info(enum MSDK_SCENARIO_ID_ENUM scenario_id,
	MSDK_SENSOR_INFO_STRUCT *sensor_info,
	MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("scenario_id = %d\n", scenario_id);

	sensor_info->SensorClockPolarity = SENSOR_CLOCK_POLARITY_LOW;
	sensor_info->SensorClockFallingPolarity = SENSOR_CLOCK_POLARITY_LOW; /* not use */
	sensor_info->SensorHsyncPolarity = SENSOR_CLOCK_POLARITY_LOW; /* inverse with datasheet */
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

	sensor_info->AEShutDelayFrame = imgsensor_info.ae_shut_delay_frame;
	sensor_info->AESensorGainDelayFrame = imgsensor_info.ae_sensor_gain_delay_frame;
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
	sensor_info->SensorWidthSampling = 0;  /* 0 is default 1x */
	sensor_info->SensorHightSampling = 0;  /* 0 is default 1x */
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
	default:
		LOG_INF("Error ScenarioId setting");
		preview(image_window, sensor_config_data);
		return ERROR_INVALID_SCENARIO_ID;
	}
	return ERROR_NONE;
}

static kal_uint32 set_video_mode(UINT16 framerate)
{
	/*This Function not used after ROME*/
	LOG_INF("framerate = %d\n ", framerate);
	/* SetVideoMode Function should fix framerate */
	/***********
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
	 ********/
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
	default:
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
	default:
		break;
	}

	return ERROR_NONE;
}

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
	default:
		break;
	}
	return ERROR_NONE;
}

static struct SENSOR_FUNCTION_STRUCT sensor_func = {
	open,
	get_info,
	get_resolution,
	feature_control,
	control,
	close
};

UINT32 N28GC08A3FRONTCXT_MIPI_RAW_SensorInit(struct SENSOR_FUNCTION_STRUCT **pfFunc)
{
	/* Check Sensor status here */
	if (pfFunc != NULL)
		*pfFunc = &sensor_func;
	return ERROR_NONE;
}
