/******************************************************************************
 *
 * Filename:
 * ---------
 *     gc13a0mipi_Sensor.c
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
 * Version:  V20220728232527 by GC-S-TEAM
 *

 ******************************************************************************/


/*hs14 code for SR-AL6528A-01-88 by renxinglin at 2022-11-03 start*/
/*hs14 code for AL6528ADEU-1828 by jianghongyan at 2022-11-11 end*/
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

#include "a1402frontgc13a0xl_mipi_raw.h"

/************************** Modify Following Strings for Debug **************************/
#define PFX "a1402frontgc13a0xl_mipi_raw"
#define LOG_1 LOG_INF("GC13A0, MIPI 4LANE\n")
/****************************   Modify end    *******************************************/
#define GC13A0_DEBUG                1
#if GC13A0_DEBUG
#define LOG_INF(format, args...)    pr_info(PFX "[%s] " format, __func__, ##args)
#else
#define LOG_INF(format, args...)
#endif

static DEFINE_SPINLOCK(imgsensor_drv_lock);

static struct imgsensor_info_struct imgsensor_info = {
	.sensor_id = A1402FRONTGC13A0XL_SENSOR_ID,
	.checksum_value = 0xe5d32119,
	.pre = {
		.pclk = 379200000,
		.linelength = 3900,
		.framelength = 3240,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 4128,
		.grabwindow_height = 3096,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 480000000,
		.max_framerate = 300,
	},
	.cap = {
		.pclk = 379200000,
		.linelength = 3900,
		.framelength = 3240,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 4128,
		.grabwindow_height = 3096,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 480000000,
		.max_framerate = 300,
	},
	.normal_video = {
		.pclk = 379200000,
		.linelength = 3900,
		.framelength = 3240,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 4128,
		.grabwindow_height = 3096,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 480000000,
		.max_framerate = 300,
	},
	.hs_video = {
		.pclk = 379200000,
		.linelength = 3900,
		.framelength = 3232,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 3712,
		.grabwindow_height = 2556,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 480000000,
		.max_framerate = 300,
	},
	.slim_video = {
		.pclk = 379200000,
		.linelength = 3900,
		.framelength = 3232,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 3408,
		.grabwindow_height = 2556,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 480000000,
		.max_framerate = 300,
	},
	.custom1 = {
		.pclk = 379200000,
		.linelength = 3900,
		.framelength = 3232,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 1696,
		.grabwindow_height = 1272,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 240000000,
		.max_framerate = 300,
	},
	.custom2 = {
		.pclk = 379200000,
		.linelength = 3900,
		.framelength = 3232,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 1856,
		.grabwindow_height = 1044,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 240000000,
		.max_framerate = 300,
	},
	.custom3 = {
		.pclk = 379200000,
		.linelength = 3900,
		.framelength = 3232,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 2064,
		.grabwindow_height = 1160,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 240000000,
		.max_framerate = 300,
	},
	.custom4 = {
		.pclk = 379200000,
		.linelength = 3900,
		.framelength = 3232,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 2064,
		.grabwindow_height = 1548,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 240000000,
		.max_framerate = 300,
	},
	.custom5 = {
		.pclk = 379200000,
		.linelength = 3900,
		.framelength = 3232,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 4128,
		.grabwindow_height = 2324,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 480000000,
		.max_framerate = 300,
	},
	.margin = 16,
	.min_shutter = 4,
	.max_frame_length = 0xfffe,
	.ae_shut_delay_frame = 0,
	.ae_sensor_gain_delay_frame = 0,
	.ae_ispGain_delay_frame = 2,
	.ihdr_support = 0,
	.ihdr_le_firstline = 0,
	.sensor_mode_num = 10,

	.cap_delay_frame = 2,
	.pre_delay_frame = 2,
	.video_delay_frame = 2,
	.hs_video_delay_frame = 2,
	.slim_video_delay_frame = 2,
	.custom1_delay_frame = 2,
	.custom2_delay_frame = 2,
	.custom3_delay_frame = 2,
	.custom4_delay_frame = 2,
	.custom5_delay_frame = 2,

	.isp_driving_current = ISP_DRIVING_4MA,
	.sensor_interface_type = SENSOR_INTERFACE_TYPE_MIPI,
	.mipi_sensor_type = MIPI_OPHY_NCSI2,
	.mipi_settle_delay_mode = MIPI_SETTLEDELAY_AUTO,
#if GC13A0_MIRROR_NORMAL
	.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_Gr,
#elif GC13A0_MIRROR_H
	.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_R,
#elif GC13A0_MIRROR_V
	.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_B,
#elif GC13A0_MIRROR_HV
	.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_Gb,
#else
	.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_Gr,
#endif
	.mclk = 24,
	.mipi_lane_num = SENSOR_MIPI_4_LANE,
	.i2c_addr_table = {0x72, 0x20, 0x22, 0x24, 0xff},
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
	.i2c_write_id = 0x20,
};

/* Sensor output window information */
static struct SENSOR_WINSIZE_INFO_STRUCT imgsensor_winsize_info[10] = {
	{ 4128, 3096, 0, 0, 4128, 3096, 4128, 3096, 0, 0, 4128, 3096, 0, 0, 4128, 3096}, /* Preview */
	{ 4128, 3096, 0, 0, 4128, 3096, 4128, 3096, 0, 0, 4128, 3096, 0, 0, 4128, 3096}, /* capture */
	{ 4128, 3096, 0, 0, 4128, 3096, 4128, 3096, 0, 0, 4128, 3096, 0, 0, 4128, 3096}, /* video */
	{ 4128, 3096, 0, 0, 4128, 3096, 4128, 3096, 208, 270, 3712, 2556, 0, 0, 3712, 2556}, /* hs video */
	{ 4128, 3096, 0, 0, 4128, 3096, 4128, 3096, 360, 270, 3408, 2556, 0, 0, 3408, 2556},  /* slim video */
	{ 4128, 3096, 0, 0, 4128, 3096, 2064, 1548, 184, 138, 1696, 1272, 0, 0, 1696, 1272}, /* custom1 */
	{ 4128, 3096, 0, 0, 4128, 3096, 2064, 1548, 104, 252, 1856, 1044, 0, 0, 1856, 1044}, /* custom2 */
	{ 4128, 3096, 0, 0, 4128, 3096, 2064, 1548, 0, 194, 2064, 1160, 0, 0, 2064, 1160}, /* custom3 */
	{ 4128, 3096, 0, 0, 4128, 3096, 2064, 1548, 0, 0, 2064, 1548, 0, 0, 2064, 1548}, /* custom4 */
	{ 4128, 3096, 0, 0, 4128, 3096, 4128, 3096, 0, 386, 4128, 2324, 0, 0, 4128, 2324} /* custom5 */
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
/*hs14 code for AL6528A-163|AL6528ADEU-2823 by pengxutao at 2022/11/23 start*/
kal_uint16 gc13a0_init_addr_data[] = {
/*system*/
	0x031c, 0x20,
	0x0337, 0x05,
	0x0335, 0x01,
	0x0253, 0x04,
	0x0254, 0x84,
	0x0336, 0x7d,
	0x0321, 0x10,
	0x031a, 0x00,
	0x0315, 0xd7,
	0x0c0d, 0x34,
	0x0c0c, 0x4f,
	0x0c0e, 0x20,
	0x0314, 0x11,
	0x03a2, 0x0f,
	0x0334, 0x40,
	0x0c0d, 0xb4,
	0x031c, 0x9f,
/*Pre_set*/
	0x0c08, 0x18,
	0x0059, 0x11,
	0x0057, 0x08,
	0x0084, 0x30,
	0x0087, 0x51,
	0x05a0, 0x02,
	0x0074, 0x0b,
	0x0218, 0x00,
	0x0241, 0x88,
	0x0101, GC13A0_MIRROR,
	0x00a1, 0x0d,
/*analog*/
	0x0c17, 0x12,
	0x0c16, 0x24,
	0x0c15, 0x04,
	0x0c57, 0x12,
	0x0c56, 0x24,
	0x0c55, 0x04,
	0x0346, 0x00,
	0x0347, 0x04,
	0x0348, 0x10,
	0x0349, 0x80,
	0x034a, 0x0c,
	0x034b, 0x40,
	0x0244, 0x17,
	0x0245, 0x17,
	0x0202, 0x0b,
	0x0203, 0x64,
	0x0342, 0x07,
	0x0343, 0x9e,
	0x0340, 0x0c,
	0x0341, 0xa0,
	0x0210, 0x53,
	0x0225, 0x04,
	0x0226, 0x00,
	0x0227, 0x30,
	0x0219, 0x05,
	0x0e0a, 0x01,
	0x0e0b, 0x01,
	0x0220, 0x08,
	0x0e22, 0x04,
	0x0e23, 0x08,
	0x0e01, 0x64,
	0x0e15, 0x58,
	0x0e16, 0x84,
	0x0e0c, 0x38,
	0x0e28, 0x38,
	0x0e49, 0x6a,
	0x0e18, 0x68,
	0x0e17, 0x66,
	0x0e1a, 0x48,
	0x0e19, 0x46,
	0x0e2b, 0x64,
	0x0e35, 0x80,
	0x0e36, 0x06,
	0x0e33, 0x4c,
	0x025c, 0x40,
	0x0e37, 0x30,
	0x0e38, 0x30,
	0x0c47, 0x3e,
	0x02b8, 0x04,
	0x02b5, 0x74,
	0x02b6, 0x87,
	0x0e4f, 0x00,
	0x0e44, 0x00,
	0x0e42, 0x0f,
	0x0e43, 0x80,
	0x0c41, 0x0b,
	0x0c45, 0x9f,
	0x0c46, 0xfd,
	0x0c09, 0x24,
	0x0c07, 0x14,
	0x0c05, 0xff,
	0x02b7, 0x09,
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
/*auto_load_DD/channel gain*/
	0x0360, 0x01,
	0x0360, 0x00,
	0x0316, 0x01,
	0x0a67, 0x80,
	0x0313, 0x00,
	0x0a65, 0x05,
	0x0a68, 0x11,
	0x0a58, 0x00,
	0x0a4f, 0x00,
	0x00a4, 0x00,
	0x00a5, 0x01,
	0x00a2, 0x00,
	0x00a3, 0x00,
	0x00ab, 0x00,
	0x00ac, 0x00,
	0x00a7, 0x0c,
	0x00a8, 0x40,
	0x00a9, 0x10,
	0x00aa, 0x80,
	0x0a8a, 0x00,
	0x0a8b, 0xe0,
	0x0a8c, 0x1b,
	0x0a8d, 0xf0,
	0x0a90, 0x0a,
	0x0a91, 0x19,
	0x0a92, 0xf8,
	0x0a71, 0x52,
	0x0a72, 0x22,
	0x0a73, 0x64,
	0x0a75, 0x41,
	0x0a70, 0x07,
	0x0313, 0x80,
/*standby*/
	0x0311, 0xb0,
	0x02db, 0x01,
/*standby->active*/
	0x0b00, 0x0f,
	0x0b01, 0xa2,
	0x0b02, 0x03,
	0x0b03, 0x07,
	0x0b04, 0x11,
	0x0b05, 0x14,
	0x0b06, 0x03,
	0x0b07, 0x07,
	0x0b08, 0xb4,
	0x0b09, 0x0d,
	0x0b0a, 0x0c,
	0x0b0b, 0x07,
	0x0b0c, 0x40,
	0x0b0d, 0x34,
	0x0b0e, 0x03,
	0x0b0f, 0x07,
	0x0b10, 0x80,
	0x0b11, 0x1c,
	0x0b12, 0x03,
	0x0b13, 0x07,
	0x0b14, 0x10,
	0x0b15, 0xfe,
	0x0b16, 0x03,
	0x0b17, 0x07,
	0x0b18, 0x00,
	0x0b19, 0xfe,
	0x0b1a, 0x03,
	0x0b1b, 0x07,
	0x0b1c, 0x9f,
	0x0b1d, 0x1c,
	0x0b1e, 0x03,
	0x0b1f, 0x07,
	0x0b20, 0x00,
	0x0b21, 0xfe,
	0x0b22, 0x03,
	0x0b23, 0x07,
	0x0b24, 0x00,
	0x0b25, 0xfe,
	0x0b26, 0x03,
	0x0b27, 0x07,
	0x0b28, 0x80,
	0x0b29, 0x1c,
	0x0b2a, 0x03,
	0x0b2b, 0x07,
	0x0b2c, 0x10,
	0x0b2d, 0xfe,
	0x0b2e, 0x03,
	0x0b2f, 0x07,
	0x0b30, 0x00,
	0x0b31, 0xfe,
	0x0b32, 0x03,
	0x0b33, 0x07,
	0x0b34, 0x9f,
	0x0b35, 0x1c,
	0x0b36, 0x03,
	0x0b37, 0x07,
	0x0b38, 0x44,
	0x0b39, 0x80,
	0x0b3a, 0x01,
	0x0b3b, 0x07,
	0x0b3c, 0x99,
	0x0b3d, 0x02,
	0x0b3e, 0x01,
	0x0b3f, 0x07,
/*active->standby*/
	0x0b40, 0xd9,
	0x0b41, 0x02,
	0x0b42, 0x01,
	0x0b43, 0x07,
	0x0b44, 0x00,
	0x0b45, 0xfe,
	0x0b46, 0x03,
	0x0b47, 0x07,
	0x0b48, 0x06,
	0x0b49, 0x14,
	0x0b4a, 0x03,
	0x0b4b, 0x07,
	0x0b4c, 0x34,
	0x0b4d, 0x0d,
	0x0b4e, 0x0c,
	0x0b4f, 0x07,
	0x0b50, 0x00,
	0x0b51, 0x34,
	0x0b52, 0x03,
	0x0b53, 0x07,
	0x0b54, 0xe0,
	0x0b55, 0x1c,
	0x0b56, 0x03,
	0x0b57, 0x07,
	0x0b58, 0x04,
	0x0b59, 0x80,
	0x0b5a, 0x01,
	0x0b5b, 0x07,
	0x0b5c, 0x07,
	0x0b5d, 0xa2,
	0x0b5e, 0x03,
	0x0b5f, 0x07,
	0x0aab, 0x09,
	0x0aa8, 0xb0,
	0x0264, 0x00,
	0x0265, 0x04,
	0x0266, 0x1c,
	0x0267, 0x80,
	0x0aa9, 0x10,
	0x0aaa, 0x18,
/*gain partition*/
	0x05a0, 0x82,
	0x05ac, 0x00,
	0x05ad, 0x01,
	0x05ae, 0x00,
	0x059a, 0x00,
	0x059b, 0x00,
	0x059c, 0x01,
	0x0597, 0x0d,
	0x0598, 0x00,
	0x05ab, 0x09,
	0x05a4, 0x01,
	0x05a3, 0x05,
	0x0800, 0x0b,
	0x0801, 0x16,
	0x0802, 0x2c,
	0x0803, 0x3e,
	0x0804, 0x0e,
	0x0805, 0x33,
	0x0806, 0x31,
	0x0807, 0x25,
	0x0808, 0x1b,
	0x0809, 0x14,
	0x080a, 0x10,
	0x080b, 0x00,
	0x080c, 0x00,
	0x080d, 0x01,
	0x080e, 0x00,
	0x080f, 0x01,
	0x0810, 0x00,
	0x0811, 0x00,
	0x0812, 0x02,
	0x0813, 0x01,
	0x0814, 0x6b,
	0x0815, 0x00,
	0x0816, 0x03,
	0x0817, 0x01,
	0x0818, 0xfa,
	0x0819, 0x00,
	0x081a, 0x04,
	0x081b, 0x02,
	0x081c, 0xd2,
	0x081d, 0x00,
	0x081e, 0x05,
	0x081f, 0x03,
	0x0820, 0xf1,
	0x0821, 0x00,
	0x0822, 0x06,
	0x0823, 0x05,
	0x0824, 0x8a,
	0x0825, 0x09,
	0x0826, 0x36,
	0x0827, 0x07,
	0x0828, 0xe4,
	0x0829, 0x10,
	0x082a, 0x06,
	0x082b, 0x0b,
	0x082c, 0x10,
	0x082d, 0x14,
	0x082e, 0xa6,
	0x082f, 0x0f,
	0x0830, 0x80,
	0x0831, 0x17,
	0x0832, 0xf6,
	0x05ac, 0x01,
	0x05a0, 0xc2,
	0x0207, 0xc4,
/*ISP*/
	0x0089, 0x03,
	0x0080, 0xd8,
	0x009a, 0x00,
/*BLK*/
	0x0040, 0x22,
	0x0047, 0xf0,
	0x0048, 0x0f,
	0x004b, 0x0f,
	0x004c, 0x00,
	0x0046, 0x0a,
	0x0041, 0x20,
	0x0043, 0x50,
	0x0044, 0x00,
	0x005b, 0x02,
	0x0082, 0x00,
	0x009b, 0x40,
/*gain*/
	0x0208, 0x01,
	0x0209, 0x5a,
	0x0204, 0x04,
	0x0205, 0x00,
/*dither*/
	0x0096, 0x81,
	0x0097, 0x08,
	0x0098, 0x87,
/*RTC*/
	0x00c0, 0x00,
	0x00c1, 0x80,
	0x00c2, 0x11,
	0x0460, 0x04,
	0x0462, 0x08,
	0x0464, 0x0a,
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
	0x0474, 0x38,
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
/*out window*/
	0x0351, 0x00,
	0x0352, 0x08,
	0x0353, 0x00,
	0x0354, 0x08,
	0x034c, 0x10,
	0x034d, 0x70,
	0x034e, 0x0c,
	0x034f, 0x30,
/*PDAF_1/128*/
	0x00e6, 0x11,
	0x00e0, 0x20,
	0x00e1, 0x10,
	0x00e2, 0x20,
	0x00e3, 0x0c,
	0x00e4, 0x30,
	0x00e5, 0x08,
	0x00a0, 0x11,
	0x00c3, 0x00,
/*MIPI*/
	0x0114, 0x03,
	0x0115, 0x30,
	0x0180, 0x44,
	0x0181, 0xf0,
	0x0184, 0x5d,
	0x0185, 0x40,
/*auto_load off*/
	0x00a4, 0x80,
	0x0a70, 0x00,
	0x0316, 0x00,
	0x0a67, 0x00,
/*OUT*/
	0x0084, 0x10,
	0x0102, 0x09,
	0x0103, 0x04,
	0x011b, 0x12,
	0x011c, 0x12,
};
/*hs14 code for AL6528A-163|AL6528ADEU-2823 by pengxutao at 2022/11/23 end*/
static kal_uint16 gc13a0_4128x3096_addr_data[] = {
	/*system*/
	0x031c, 0x20,
	0x0337, 0x05,
	0x0335, 0x01,
	0x0253, 0x04,
	0x0254, 0x84,
	0x0336, 0x7d,
	0x0321, 0x10,
	0x031a, 0x00,
	0x0315, 0xd7,
	0x0c0d, 0x34,
	0x0c0c, 0x4f,
	0x0c0e, 0x20,
	0x0314, 0x11,
	0x03a2, 0x0f,
	0x0334, 0x40,
	0x0c0d, 0xb4,
	0x031c, 0x9f,

	/*Pre_set*/
	0x0218, 0x00,
	0x0241, 0x88,

	/*analog*/
	0x0c17, 0x12,
	0x0c16, 0x24,
	0x0c15, 0x04,
	0x0c57, 0x12,
	0x0c56, 0x24,
	0x0c55, 0x04,
	0x0346, 0x00,
	0x0347, 0x04,
	0x0348, 0x10,
	0x0349, 0x80,
	0x034a, 0x0c,
	0x034b, 0x40,

	0x0202, 0x0b,
	0x0203, 0x64,
	0x0342, 0x07,
	0x0343, 0x9e,
	0x0340, 0x0c,
	0x0341, 0xa8,
	0x0210, 0x53,
	0x0225, 0x04,
	0x0226, 0x00,
	0x0227, 0x38,

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

	/*auto_load_DD/channel gain*/
	0x00a2, 0x00,
	0x00a3, 0x00,
	0x00ab, 0x00,
	0x00ac, 0x00,

	/*ISP*/
	0x0089, 0x03,
	0x0080, 0xd0,

	/*BLK*/
	0x004f, 0x0a,
	0x0050, 0x40,
	0x0051, 0x20,
	0x0052, 0x18,

	/*gain*/
	0x0204, 0x04,
	0x0205, 0x00,

	/*RTC*/
	0x00c0, 0x00,

	/*out window*/
	0x0351, 0x00,
	0x0352, 0x14,
	0x0353, 0x00,
	0x0354, 0x30,
	0x034c, 0x10,
	0x034d, 0x20,
	0x034e, 0x0c,
	0x034f, 0x18,

	/*auto_load_register*/
	0x0316, 0x01,
	0x0a67, 0x80,
	0x0a70, 0x11,
	0x0313, 0x80,

	/*PDAF_1/128*/
	0x00e6, 0x11,
	0x00e0, 0x20,
	0x00e1, 0x10,
	0x00e2, 0x20,
	0x00e3, 0x0c,
	0x00e4, 0x30,
	0x00e5, 0x08,
	0x00a0, 0x31,
	0x00c3, 0x10,

	/*MIPI*/
	0x0121, 0x0e,
	0x0122, 0x0c,
	0x0123, 0x28,
	0x0124, 0x02,
	0x0125, 0x10,
	0x0126, 0x0d,
	0x0129, 0x0c,
	0x012a, 0x12,
	0x012b, 0x0e,

	/*auto_load off*/
	0x0a70, 0x00,
	0x0316, 0x00,
	0x0a67, 0x00,

	/*OUT*/
	0x0084, 0x10,
	0x0102, 0x09,
};

static kal_uint16 gc13a0_2064x1548_addr_data[] = {
	/*system*/
	0x031c, 0x20,
	0x0337, 0x05,
	0x0335, 0x05,
	0x0253, 0x04,
	0x0254, 0x84,
	0x0336, 0x7d,
	0x0321, 0x10,
	0x031a, 0x00,
	0x0315, 0xd7,
	0x0c0d, 0x34,
	0x0c0c, 0x4f,
	0x0c0e, 0x20,
	0x0314, 0x11,
	0x03a2, 0x0f,
	0x0334, 0x40,
	0x0c0d, 0xb4,
	0x031c, 0x9f,
/*Pre_set*/
	0x0218, 0x80,
	0x0241, 0xdc,
	/*analog*/
	0x0c17, 0x12,
	0x0c16, 0x24,
	0x0c15, 0x04,
	0x0c57, 0x12,
	0x0c56, 0x24,
	0x0c55, 0x04,
	0x0346, 0x00,
	0x0347, 0x04,
	0x0348, 0x10,
	0x0349, 0x80,
	0x034a, 0x0c,
	0x034b, 0x40,
	0x0202, 0x0b,
	0x0203, 0x64,
	0x0342, 0x07,
	0x0343, 0x9e,
	0x0340, 0x0c,
	0x0341, 0xa0,
	0x0210, 0x53,
	0x0225, 0x04,
	0x0226, 0x06,
	0x0227, 0x58,
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
/*auto_load_DD/channel gain*/
	0x00a2, 0x00,
	0x00a3, 0x00,
	0x00ab, 0x00,
	0x00ac, 0x00,
/*ISP*/
	0x0089, 0x03,
	0x0080, 0xd0,
/*BLK*/
	0x004f, 0x0a,
	0x0050, 0x40,
	0x0051, 0x20,
	0x0052, 0x18,
/*gain*/
	0x0204, 0x04,
	0x0205, 0x00,
/*RTC*/
	0x00c0, 0x02,
/*out window*/
	0x0351, 0x00,
	0x0352, 0x0a,
	0x0353, 0x00,
	0x0354, 0x18,
	0x034c, 0x08,
	0x034d, 0x10,
	0x034e, 0x06,
	0x034f, 0x0c,
/*auto_load_register*/
	0x0316, 0x01,
	0x0a67, 0x80,
	0x0a70, 0x11,
	0x0313, 0x80,
/*PDAF_1/128*/
	0x00e6, 0x11,
	0x00e0, 0x20,
	0x00e1, 0x10,
	0x00e2, 0x20,
	0x00e3, 0x0c,
	0x00e4, 0x30,
	0x00e5, 0x08,
	0x00a0, 0x31,
	0x00c3, 0x10,
/*MIPI*/
	0x0121, 0x06,
	0x0122, 0x05,
	0x0123, 0x18,
	0x0124, 0x02,
	0x0125, 0x0b,
	0x0126, 0x08,
	0x0129, 0x06,
	0x012a, 0x06,
	0x012b, 0x07,
/*auto_load off*/
	0x0a70, 0x00,
	0x0316, 0x00,
	0x0a67, 0x00,
/*OUT*/
	0x0084, 0x10,
	0x0102, 0x09,
};

static kal_uint16 gc13a0_2064x1160_addr_data[] = {
/*system*/
	0x031c, 0x20,
	0x0337, 0x05,
	0x0335, 0x05,
	0x0253, 0x04,
	0x0254, 0x84,
	0x0336, 0x7d,
	0x0321, 0x10,
	0x031a, 0x00,
	0x0315, 0xd7,
	0x0c0d, 0x34,
	0x0c0c, 0x4f,
	0x0c0e, 0x20,
	0x0314, 0x11,
	0x03a2, 0x0f,
	0x0334, 0x40,
	0x0c0d, 0xb4,
	0x031c, 0x9f,
/*Pre_set*/
	0x0218, 0x80,
	0x0241, 0xdc,
/*analog*/
	0x0c17, 0x12,
	0x0c16, 0x24,
	0x0c15, 0x04,
	0x0c57, 0x12,
	0x0c56, 0x24,
	0x0c55, 0x04,
	0x0346, 0x00,
	0x0347, 0x04,
	0x0348, 0x10,
	0x0349, 0x80,
	0x034a, 0x0c,
	0x034b, 0x40,
	0x0202, 0x0b,
	0x0203, 0x64,
	0x0342, 0x07,
	0x0343, 0x9e,
	0x0340, 0x0c,
	0x0341, 0xa0,
	0x0210, 0x53,
	0x0225, 0x04,
	0x0226, 0x06,
	0x0227, 0x58,
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
/*auto_load_DD/channel gain*/
	0x00a2, 0x00,
	0x00a3, 0x00,
	0x00ab, 0x00,
	0x00ac, 0x00,
/*ISP*/
	0x0089, 0x03,
	0x0080, 0xd0,
/*BLK*/
	0x004f, 0x0a,
	0x0050, 0x40,
	0x0051, 0x20,
	0x0052, 0x18,
/*gain*/
	0x0204, 0x04,
	0x0205, 0x00,
/*RTC*/
	0x00c0, 0x02,
/*out window*/
	0x0351, 0x00,
	0x0352, 0xcc,
	0x0353, 0x00,
	0x0354, 0x18,
	0x034c, 0x08,
	0x034d, 0x10,
	0x034e, 0x04,
	0x034f, 0x88,
/*auto_load_register*/
	0x0316, 0x01,
	0x0a67, 0x80,
	0x0a70, 0x11,
	0x0313, 0x80,
/*PDAF_1/128*/
	0x00e6, 0x11,
	0x00e0, 0x20,
	0x00e1, 0x10,
	0x00e2, 0x20,
	0x00e3, 0x0c,
	0x00e4, 0x30,
	0x00e5, 0x08,
	0x00a0, 0x31,
	0x00c3, 0x10,
/*MIPI*/
	0x0121, 0x06,
	0x0122, 0x05,
	0x0123, 0x18,
	0x0124, 0x02,
	0x0125, 0x0b,
	0x0126, 0x08,
	0x0129, 0x06,
	0x012a, 0x06,
	0x012b, 0x07,
/*auto_load off*/
	0x0a70, 0x00,
	0x0316, 0x00,
	0x0a67, 0x00,
/*OUT*/
	0x0084, 0x10,
	0x0102, 0x09,
};

static kal_uint16 gc13a0_1696x1272_addr_data[] = {
/*system*/
	0x031c, 0x20,
	0x0337, 0x05,
	0x0335, 0x05,
	0x0253, 0x04,
	0x0254, 0x84,
	0x0336, 0x7d,
	0x0321, 0x10,
	0x031a, 0x00,
	0x0315, 0xd7,
	0x0c0d, 0x34,
	0x0c0c, 0x4f,
	0x0c0e, 0x20,
	0x0314, 0x11,
	0x03a2, 0x0f,
	0x0334, 0x40,
	0x0c0d, 0xb4,
	0x031c, 0x9f,
/*Pre_set*/
	0x0218, 0x80,
	0x0241, 0xdc,
/*analog*/
	0x0c17, 0x12,
	0x0c16, 0x24,
	0x0c15, 0x04,
	0x0c57, 0x12,
	0x0c56, 0x24,
	0x0c55, 0x04,
	0x0346, 0x00,
	0x0347, 0x04,
	0x0348, 0x10,
	0x0349, 0x80,
	0x034a, 0x0c,
	0x034b, 0x40,
	0x0202, 0x0b,
	0x0203, 0x64,
	0x0342, 0x07,
	0x0343, 0x9e,
	0x0340, 0x0c,
	0x0341, 0xa0,
	0x0210, 0x53,
	0x0225, 0x04,
	0x0226, 0x06,
	0x0227, 0x58,
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
/*auto_load_DD/channel gain*/
	0x00a2, 0x00,
	0x00a3, 0x00,
	0x00ab, 0x00,
	0x00ac, 0x00,
/*ISP*/
	0x0089, 0x03,
	0x0080, 0xd0,
/*BLK*/
	0x004f, 0x0a,
	0x0050, 0x40,
	0x0051, 0x20,
	0x0052, 0x18,
/*gain*/
	0x0204, 0x04,
	0x0205, 0x00,
/*RTC*/
	0x00c0, 0x02,
/*out window*/
	0x0351, 0x00,
	0x0352, 0x94,
	0x0353, 0x00,
	0x0354, 0xd0,
	0x034c, 0x06,
	0x034d, 0xa0,
	0x034e, 0x04,
	0x034f, 0xf8,
/*auto_load_register*/
	0x0316, 0x01,
	0x0a67, 0x80,
	0x0a70, 0x11,
	0x0313, 0x80,
/*PDAF_1/128*/
	0x00e6, 0x11,
	0x00e0, 0x20,
	0x00e1, 0x10,
	0x00e2, 0x20,
	0x00e3, 0x0c,
	0x00e4, 0x30,
	0x00e5, 0x08,
	0x00a0, 0x31,
	0x00c3, 0x10,
/*MIPI*/
	0x0121, 0x06,
	0x0122, 0x05,
	0x0123, 0x18,
	0x0124, 0x02,
	0x0125, 0x0b,
	0x0126, 0x08,
	0x0129, 0x06,
	0x012a, 0x06,
	0x012b, 0x07,
/*auto_load off*/
	0x0a70, 0x00,
	0x0316, 0x00,
	0x0a67, 0x00,
/*OUT*/
	0x0084, 0x10,
	0x0102, 0x09,
};

static kal_uint16 gc13a0_1856x1044_addr_data[] = {
	/*system*/
	0x031c, 0x20,
	0x0337, 0x05,
	0x0335, 0x05,
	0x0253, 0x04,
	0x0254, 0x84,
	0x0336, 0x7d,
	0x0321, 0x10,
	0x031a, 0x00,
	0x0315, 0xd7,
	0x0c0d, 0x34,
	0x0c0c, 0x4f,
	0x0c0e, 0x20,
	0x0314, 0x11,
	0x03a2, 0x0f,
	0x0334, 0x40,
	0x0c0d, 0xb4,
	0x031c, 0x9f,

	/*Pre_set*/
	0x0218, 0x80,
	0x0241, 0xdc,

	/*analog*/
	0x0c17, 0x12,
	0x0c16, 0x24,
	0x0c15, 0x04,
	0x0c57, 0x12,
	0x0c56, 0x24,
	0x0c55, 0x04,
	0x0346, 0x00,
	0x0347, 0x04,
	0x0348, 0x10,
	0x0349, 0x80,
	0x034a, 0x0c,
	0x034b, 0x40,

	0x0202, 0x0b,
	0x0203, 0x64,
	0x0342, 0x07,
	0x0343, 0x9e,
	0x0340, 0x0c,
	0x0341, 0xa0,
	0x0210, 0x53,
	0x0225, 0x04,
	0x0226, 0x06,
	0x0227, 0x58,

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

	/*auto_load_DD/channel gain*/
	0x00a2, 0x00,
	0x00a3, 0x00,
	0x00ab, 0x00,
	0x00ac, 0x00,

	/*ISP*/
	0x0089, 0x03,
	0x0080, 0xd0,

	/*BLK*/
	0x004f, 0x0a,
	0x0050, 0x40,
	0x0051, 0x20,
	0x0052, 0x18,

	/*gain*/
	0x0204, 0x04,
	0x0205, 0x00,

	/*RTC*/
	0x00c0, 0x02,

	/*out window*/
	0x0351, 0x01,
	0x0352, 0x06,
	0x0353, 0x00,
	0x0354, 0x80,
	0x034c, 0x07,
	0x034d, 0x40,
	0x034e, 0x04,
	0x034f, 0x14,

	/*auto_load_register*/
	0x0316, 0x01,
	0x0a67, 0x80,
	0x0a70, 0x11,
	0x0313, 0x80,

	/*PDAF_1/128*/
	0x00e6, 0x11,
	0x00e0, 0x20,
	0x00e1, 0x10,
	0x00e2, 0x20,
	0x00e3, 0x0c,
	0x00e4, 0x30,
	0x00e5, 0x08,
	0x00a0, 0x31,
	0x00c3, 0x10,

	/*MIPI*/
	0x0121, 0x06,
	0x0122, 0x05,
	0x0123, 0x18,
	0x0124, 0x02,
	0x0125, 0x0b,
	0x0126, 0x08,
	0x0129, 0x06,
	0x012a, 0x06,
	0x012b, 0x07,

	/*auto_load off*/
	0x0a70, 0x00,
	0x0316, 0x00,
	0x0a67, 0x00,

	/*OUT*/
	0x0084, 0x10,
	0x0102, 0x09,
};

static kal_uint16 gc13a0_4128x2324_addr_data[] = {
	/*system*/
	0x031c, 0x20,
	0x0337, 0x05,
	0x0335, 0x01,
	0x0253, 0x04,
	0x0254, 0x84,
	0x0336, 0x7d,
	0x0321, 0x10,
	0x031a, 0x00,
	0x0315, 0xd7,
	0x0c0d, 0x34,
	0x0c0c, 0x4f,
	0x0c0e, 0x20,
	0x0314, 0x11,
	0x03a2, 0x0f,
	0x0334, 0x40,
	0x0c0d, 0xb4,
	0x031c, 0x9f,

	/*Pre_set*/
	0x0218, 0x00,
	0x0241, 0x88,

	/*analog*/
	0x0c17, 0x12,
	0x0c16, 0x24,
	0x0c15, 0x04,
	0x0c57, 0x12,
	0x0c56, 0x24,
	0x0c55, 0x04,
	0x0346, 0x00,
	0x0347, 0x04,
	0x0348, 0x10,
	0x0349, 0x80,
	0x034a, 0x0c,
	0x034b, 0x40,

	0x0202, 0x0b,
	0x0203, 0x64,
	0x0342, 0x07,
	0x0343, 0x9e,
	0x0340, 0x0c,
	0x0341, 0xa0,
	0x0210, 0x53,
	0x0225, 0x04,
	0x0226, 0x00,
	0x0227, 0x30,

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

	/*auto_load_DD/channel gain*/
	0x00a2, 0x00,
	0x00a3, 0x00,
	0x00ab, 0x00,
	0x00ac, 0x00,

	/*ISP*/
	0x0089, 0x03,
	0x0080, 0xd0,

	/*BLK*/
	0x004f, 0x0a,
	0x0050, 0x40,
	0x0051, 0x20,
	0x0052, 0x18,

	/*gain*/
	0x0204, 0x04,
	0x0205, 0x00,

	/*RTC*/
	0x00c0, 0x00,

	/*out window*/
	0x0351, 0x01,
	0x0352, 0x96,
	0x0353, 0x00,
	0x0354, 0x30,
	0x034c, 0x10,
	0x034d, 0x20,
	0x034e, 0x09,
	0x034f, 0x14,

	/*auto_load_register*/
	0x0316, 0x01,
	0x0a67, 0x80,
	0x0a70, 0x11,
	0x0313, 0x80,

	/*PDAF_1/128*/
	0x00e6, 0x11,
	0x00e0, 0x20,
	0x00e1, 0x10,
	0x00e2, 0x20,
	0x00e3, 0x0c,
	0x00e4, 0x30,
	0x00e5, 0x08,
	0x00a0, 0x31,
	0x00c3, 0x10,

	/*MIPI*/
	0x0121, 0x0e,
	0x0122, 0x0c,
	0x0123, 0x28,
	0x0124, 0x02,
	0x0125, 0x10,
	0x0126, 0x0d,
	0x0129, 0x0c,
	0x012a, 0x12,
	0x012b, 0x0e,

	/*auto_load off*/
	0x0a70, 0x00,
	0x0316, 0x00,
	0x0a67, 0x00,

	/*OUT*/
	0x0084, 0x10,
	0x0102, 0x09,
};

static kal_uint16 gc13a0_3712x2556_addr_data[] = {
	/*system*/
	0x031c, 0x20,
	0x0337, 0x05,
	0x0335, 0x01,
	0x0253, 0x04,
	0x0254, 0x84,
	0x0336, 0x7d,
	0x0321, 0x10,
	0x031a, 0x00,
	0x0315, 0xd7,
	0x0c0d, 0x34,
	0x0c0c, 0x4f,
	0x0c0e, 0x20,
	0x0314, 0x11,
	0x03a2, 0x0f,
	0x0334, 0x40,
	0x0c0d, 0xb4,
	0x031c, 0x9f,

	/*Pre_set*/
	0x0218, 0x00,
	0x0241, 0x88,

	/*analog*/
	0x0c17, 0x12,
	0x0c16, 0x24,
	0x0c15, 0x04,
	0x0c57, 0x12,
	0x0c56, 0x24,
	0x0c55, 0x04,
	0x0346, 0x00,
	0x0347, 0x04,
	0x0348, 0x10,
	0x0349, 0x80,
	0x034a, 0x0c,
	0x034b, 0x40,

	0x0202, 0x0b,
	0x0203, 0x64,
	0x0342, 0x07,
	0x0343, 0x9e,
	0x0340, 0x0c,
	0x0341, 0xa0,
	0x0210, 0x53,
	0x0225, 0x04,
	0x0226, 0x00,
	0x0227, 0x30,

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

	/*auto_load_DD/channel gain*/
	0x00a2, 0x00,
	0x00a3, 0x00,
	0x00ab, 0x00,
	0x00ac, 0x00,

	/*ISP*/
	0x0089, 0x03,
	0x0080, 0xd0,

	/*BLK*/
	0x004f, 0x0a,
	0x0050, 0x40,
	0x0051, 0x20,
	0x0052, 0x18,

	/*gain*/
	0x0204, 0x04,
	0x0205, 0x00,

	/*RTC*/
	0x00c0, 0x00,

	/*out window*/
	0x0351, 0x01,
	0x0352, 0x22,
	0x0353, 0x01,
	0x0354, 0x00,
	0x034c, 0x0e,
	0x034d, 0x80,
	0x034e, 0x09,
	0x034f, 0xfc,

	/*auto_load_register*/
	0x0316, 0x01,
	0x0a67, 0x80,
	0x0a70, 0x11,
	0x0313, 0x80,

	/*PDAF_1/128*/
	0x00e6, 0x11,
	0x00e0, 0x20,
	0x00e1, 0x10,
	0x00e2, 0x20,
	0x00e3, 0x0c,
	0x00e4, 0x30,
	0x00e5, 0x08,
	0x00a0, 0x31,
	0x00c3, 0x10,

	/*MIPI*/
	0x0121, 0x0e,
	0x0122, 0x0c,
	0x0123, 0x28,
	0x0124, 0x02,
	0x0125, 0x10,
	0x0126, 0x0d,
	0x0129, 0x0c,
	0x012a, 0x12,
	0x012b, 0x0e,

	/*auto_load off*/
	0x0a70, 0x00,
	0x0316, 0x00,
	0x0a67, 0x00,

	/*OUT*/
	0x0084, 0x10,
	0x0102, 0x09,
};

static kal_uint16 gc13a0_3408x2556_addr_data[] = {
	/*system*/
	0x031c, 0x20,
	0x0337, 0x05,
	0x0335, 0x01,
	0x0253, 0x04,
	0x0254, 0x84,
	0x0336, 0x7d,
	0x0321, 0x10,
	0x031a, 0x00,
	0x0315, 0xd7,
	0x0c0d, 0x34,
	0x0c0c, 0x4f,
	0x0c0e, 0x20,
	0x0314, 0x11,
	0x03a2, 0x0f,
	0x0334, 0x40,
	0x0c0d, 0xb4,
	0x031c, 0x9f,

	/*Pre_set*/
	0x0218, 0x00,
	0x0241, 0x88,

	/*analog*/
	0x0c17, 0x12,
	0x0c16, 0x24,
	0x0c15, 0x04,
	0x0c57, 0x12,
	0x0c56, 0x24,
	0x0c55, 0x04,
	0x0346, 0x00,
	0x0347, 0x04,
	0x0348, 0x10,
	0x0349, 0x80,
	0x034a, 0x0c,
	0x034b, 0x40,

	0x0202, 0x0b,
	0x0203, 0x64,
	0x0342, 0x07,
	0x0343, 0x9e,
	0x0340, 0x0c,
	0x0341, 0xa0,
	0x0210, 0x53,
	0x0225, 0x04,
	0x0226, 0x00,
	0x0227, 0x30,

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

	/*auto_load_DD/channel gain*/
	0x00a2, 0x00,
	0x00a3, 0x00,
	0x00ab, 0x00,
	0x00ac, 0x00,

	/*ISP*/
	0x0089, 0x03,
	0x0080, 0xd0,

	/*BLK*/
	0x004f, 0x0a,
	0x0050, 0x40,
	0x0051, 0x20,
	0x0052, 0x18,

	/*gain*/
	0x0204, 0x04,
	0x0205, 0x00,

	/*RTC*/
	0x00c0, 0x00,

	/*out window*/
	0x0351, 0x01,
	0x0352, 0x22,
	0x0353, 0x01,
	0x0354, 0x98,
	0x034c, 0x0d,
	0x034d, 0x50,
	0x034e, 0x09,
	0x034f, 0xfc,

	/*auto_load_register*/
	0x0316, 0x01,
	0x0a67, 0x80,
	0x0a70, 0x11,
	0x0313, 0x80,

	/*PDAF_1/128*/
	0x00e6, 0x11,
	0x00e0, 0x20,
	0x00e1, 0x10,
	0x00e2, 0x20,
	0x00e3, 0x0c,
	0x00e4, 0x30,
	0x00e5, 0x08,
	0x00a0, 0x31,
	0x00c3, 0x10,

	/*MIPI*/
	0x0121, 0x0e,
	0x0122, 0x0c,
	0x0123, 0x28,
	0x0124, 0x02,
	0x0125, 0x10,
	0x0126, 0x0d,
	0x0129, 0x0c,
	0x012a, 0x12,
	0x012b, 0x0e,

	/*auto_load off*/
	0x0a70, 0x00,
	0x0316, 0x00,
	0x0a67, 0x00,

	/*OUT*/
	0x0084, 0x10,
	0x0102, 0x09,
};

static void gc13a0_stream_on(void)
{
	write_cmos_sensor_8bit(0x0100, 0x01);
}

static void gc13a0_stream_off(void)
{
	write_cmos_sensor_8bit(0x0100, 0x00);
}


static void sensor_init(void)
{
	table_write_cmos_sensor(gc13a0_init_addr_data,
		sizeof(gc13a0_init_addr_data)/sizeof(kal_uint16));
}

static void preview_setting(void)
{
	gc13a0_stream_off();
	table_write_cmos_sensor(gc13a0_4128x3096_addr_data,
		sizeof(gc13a0_4128x3096_addr_data)/sizeof(kal_uint16));
	gc13a0_stream_on();
}

static void capture_setting(void)
{
	gc13a0_stream_off();
	table_write_cmos_sensor(gc13a0_4128x3096_addr_data,
		sizeof(gc13a0_4128x3096_addr_data)/sizeof(kal_uint16));
	gc13a0_stream_on();
}

static void normal_video_setting(void)
{
	gc13a0_stream_off();
	table_write_cmos_sensor(gc13a0_4128x3096_addr_data,
		sizeof(gc13a0_4128x3096_addr_data)/sizeof(kal_uint16));
	gc13a0_stream_on();
}

static void hs_video_setting(void)
{
	gc13a0_stream_off();
	table_write_cmos_sensor(gc13a0_3712x2556_addr_data,
		sizeof(gc13a0_3712x2556_addr_data)/sizeof(kal_uint16));
	gc13a0_stream_on();
}

static void slim_video_setting(void)
{
	gc13a0_stream_off();
	table_write_cmos_sensor(gc13a0_3408x2556_addr_data,
		sizeof(gc13a0_3408x2556_addr_data)/sizeof(kal_uint16));
	gc13a0_stream_on();
}

static void custom1_setting(void)
{
	gc13a0_stream_off();
	table_write_cmos_sensor(gc13a0_1696x1272_addr_data,
				sizeof(gc13a0_1696x1272_addr_data) /
					sizeof(kal_uint16));
	gc13a0_stream_on();
}

static void custom2_setting(void)
{
	gc13a0_stream_off();
	table_write_cmos_sensor(gc13a0_1856x1044_addr_data,
				sizeof(gc13a0_1856x1044_addr_data) /
					sizeof(kal_uint16));
	gc13a0_stream_on();
}

static void custom3_setting(void)
{
	gc13a0_stream_off();
	table_write_cmos_sensor(gc13a0_2064x1160_addr_data,
				sizeof(gc13a0_2064x1160_addr_data) /
					sizeof(kal_uint16));
	gc13a0_stream_on();
}

static void custom4_setting(void)
{
	gc13a0_stream_off();
	table_write_cmos_sensor(gc13a0_2064x1548_addr_data,
				sizeof(gc13a0_2064x1548_addr_data) /
					sizeof(kal_uint16));
	gc13a0_stream_on();
}

static void custom5_setting(void)
{
	gc13a0_stream_off();
	table_write_cmos_sensor(gc13a0_4128x2324_addr_data,
				sizeof(gc13a0_4128x2324_addr_data) /
					sizeof(kal_uint16));
	gc13a0_stream_on();
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
				pr_debug("[gc13a0_camera_sensor]get_imgsensor_id:i2c write id: 0x%x, sensor id: 0x%x\n",
					imgsensor.i2c_write_id, *sensor_id);
				return ERROR_NONE;
			}
			pr_debug("[gc13a0_camera_sensor]get_imgsensor_id:Read sensor id fail, write id: 0x%x, id: 0x%x\n",
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
				pr_debug("[gc13a0_camera_sensor]open:i2c write id: 0x%x, sensor id: 0x%x\n",
					imgsensor.i2c_write_id, sensor_id);
				break;
			}
			pr_debug("[gc13a0_camera_sensor]open:Read sensor id fail, write id: 0x%x, id: 0x%x\n",
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
static kal_uint32 custom1(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("E\n");

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_CUSTOM1;
	imgsensor.pclk = imgsensor_info.custom1.pclk;
	imgsensor.line_length = imgsensor_info.custom1.linelength;
	imgsensor.frame_length = imgsensor_info.custom1.framelength;
	imgsensor.min_frame_length = imgsensor_info.custom1.framelength;
	imgsensor.dummy_line = 0;
	imgsensor.dummy_pixel = 0;
	imgsensor.autoflicker_en = KAL_FALSE;
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
	imgsensor.pclk = imgsensor_info.custom2.pclk;
	imgsensor.line_length = imgsensor_info.custom2.linelength;
	imgsensor.frame_length = imgsensor_info.custom2.framelength;
	imgsensor.min_frame_length = imgsensor_info.custom2.framelength;
	imgsensor.dummy_line = 0;
	imgsensor.dummy_pixel = 0;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	custom2_setting();
	return ERROR_NONE;
}

static kal_uint32 custom3(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("E\n");

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_CUSTOM3;
	imgsensor.pclk = imgsensor_info.custom3.pclk;
	imgsensor.line_length = imgsensor_info.custom3.linelength;
	imgsensor.frame_length = imgsensor_info.custom3.framelength;
	imgsensor.min_frame_length = imgsensor_info.custom3.framelength;
	imgsensor.dummy_line = 0;
	imgsensor.dummy_pixel = 0;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	custom3_setting();
	return ERROR_NONE;
}

static kal_uint32 custom4(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("E\n");

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_CUSTOM4;
	imgsensor.pclk = imgsensor_info.custom4.pclk;
	imgsensor.line_length = imgsensor_info.custom4.linelength;
	imgsensor.frame_length = imgsensor_info.custom4.framelength;
	imgsensor.min_frame_length = imgsensor_info.custom4.framelength;
	imgsensor.dummy_line = 0;
	imgsensor.dummy_pixel = 0;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	custom4_setting();
	return ERROR_NONE;
}
static kal_uint32 custom5(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("E\n");

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_CUSTOM5;
	imgsensor.pclk = imgsensor_info.custom5.pclk;
	imgsensor.line_length = imgsensor_info.custom5.linelength;
	imgsensor.frame_length = imgsensor_info.custom5.framelength;
	imgsensor.min_frame_length = imgsensor_info.custom5.framelength;
	imgsensor.dummy_line = 0;
	imgsensor.dummy_pixel = 0;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	custom5_setting();
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
	sensor_resolution->SensorCustom1Width = imgsensor_info.custom1.grabwindow_width;
	sensor_resolution->SensorCustom1Height = imgsensor_info.custom1.grabwindow_height;
	sensor_resolution->SensorCustom2Width = imgsensor_info.custom2.grabwindow_width;
	sensor_resolution->SensorCustom2Height = imgsensor_info.custom2.grabwindow_height;
	sensor_resolution->SensorCustom3Width = imgsensor_info.custom3.grabwindow_width;
	sensor_resolution->SensorCustom3Height = imgsensor_info.custom3.grabwindow_height;
	sensor_resolution->SensorCustom4Width = imgsensor_info.custom4.grabwindow_width;
	sensor_resolution->SensorCustom4Height = imgsensor_info.custom4.grabwindow_height;
	sensor_resolution->SensorCustom5Width = imgsensor_info.custom5.grabwindow_width;
	sensor_resolution->SensorCustom5Height = imgsensor_info.custom5.grabwindow_height;
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
	sensor_info->Custom1DelayFrame = imgsensor_info.custom1_delay_frame;
	sensor_info->Custom2DelayFrame = imgsensor_info.custom2_delay_frame;
	sensor_info->Custom3DelayFrame = imgsensor_info.custom3_delay_frame;
	sensor_info->Custom4DelayFrame = imgsensor_info.custom4_delay_frame;
	sensor_info->Custom5DelayFrame = imgsensor_info.custom5_delay_frame;

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
	case MSDK_SCENARIO_ID_CUSTOM3:
		sensor_info->SensorGrabStartX = imgsensor_info.custom3.startx;
		sensor_info->SensorGrabStartY = imgsensor_info.custom3.starty;
		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
			imgsensor_info.custom3.mipi_data_lp2hs_settle_dc;
		break;
	case MSDK_SCENARIO_ID_CUSTOM4:
		sensor_info->SensorGrabStartX = imgsensor_info.custom4.startx;
		sensor_info->SensorGrabStartY = imgsensor_info.custom4.starty;
		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
			imgsensor_info.custom4.mipi_data_lp2hs_settle_dc;
		break;
	case MSDK_SCENARIO_ID_CUSTOM5:
		sensor_info->SensorGrabStartX = imgsensor_info.custom5.startx;
		sensor_info->SensorGrabStartY = imgsensor_info.custom5.starty;
		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
			imgsensor_info.custom5.mipi_data_lp2hs_settle_dc;
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
	case MSDK_SCENARIO_ID_CUSTOM1:
		custom1(image_window, sensor_config_data);
		break;
	case MSDK_SCENARIO_ID_CUSTOM2:
		custom2(image_window, sensor_config_data);
		break;
	case MSDK_SCENARIO_ID_CUSTOM3:
		custom3(image_window, sensor_config_data);
		break;
	case MSDK_SCENARIO_ID_CUSTOM4:
		custom4(image_window, sensor_config_data);
		break;
	case MSDK_SCENARIO_ID_CUSTOM5:
		custom5(image_window, sensor_config_data);
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
	case MSDK_SCENARIO_ID_CUSTOM1:
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
		frame_length = imgsensor_info.custom2.pclk / framerate * 10 / imgsensor_info.custom2.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line = (frame_length > imgsensor_info.custom2.framelength) ?
			(frame_length - imgsensor_info.custom2.framelength) : 0;
		imgsensor.frame_length = imgsensor_info.custom2.framelength + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		set_dummy();
		break;
	case MSDK_SCENARIO_ID_CUSTOM3:
		frame_length = imgsensor_info.custom3.pclk / framerate * 10 / imgsensor_info.custom3.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line = (frame_length > imgsensor_info.custom3.framelength) ?
			(frame_length - imgsensor_info.custom3.framelength) : 0;
		imgsensor.frame_length = imgsensor_info.custom3.framelength + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		set_dummy();
		break;
	case MSDK_SCENARIO_ID_CUSTOM4:
		frame_length = imgsensor_info.custom4.pclk / framerate * 10 / imgsensor_info.custom4.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line = (frame_length > imgsensor_info.custom4.framelength) ?
			(frame_length - imgsensor_info.custom4.framelength) : 0;
		imgsensor.frame_length = imgsensor_info.custom4.framelength + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		set_dummy();
		break;
	case MSDK_SCENARIO_ID_CUSTOM5:
		frame_length = imgsensor_info.custom5.pclk / framerate * 10 / imgsensor_info.custom5.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line = (frame_length > imgsensor_info.custom5.framelength) ?
			(frame_length - imgsensor_info.custom5.framelength) : 0;
		imgsensor.frame_length = imgsensor_info.custom5.framelength + imgsensor.dummy_line;
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
	case MSDK_SCENARIO_ID_CUSTOM1:
		*framerate = imgsensor_info.custom1.max_framerate;
		break;
	case MSDK_SCENARIO_ID_CUSTOM2:
		*framerate = imgsensor_info.custom2.max_framerate;
		break;
	case MSDK_SCENARIO_ID_CUSTOM3:
		*framerate = imgsensor_info.custom3.max_framerate;
		break;
	case MSDK_SCENARIO_ID_CUSTOM4:
		*framerate = imgsensor_info.custom4.max_framerate;
		break;
	case MSDK_SCENARIO_ID_CUSTOM5:
		*framerate = imgsensor_info.custom5.max_framerate;
		break;
	default:
		break;
	}

	return ERROR_NONE;
}
/*hs14 code for SR-AL6528A-01-60 by jianghongyan at 2022-11-16 start*/
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
			case MSDK_SCENARIO_ID_CUSTOM1:
				rate = imgsensor_info.custom1.mipi_pixel_rate;
				break;
			case MSDK_SCENARIO_ID_CUSTOM2:
				rate = imgsensor_info.custom2.mipi_pixel_rate;
				break;
			case MSDK_SCENARIO_ID_CUSTOM3:
				rate = imgsensor_info.custom3.mipi_pixel_rate;
				break;
			case MSDK_SCENARIO_ID_CUSTOM4:
				rate = imgsensor_info.custom4.mipi_pixel_rate;
				break;
			case MSDK_SCENARIO_ID_CUSTOM5:
				rate = imgsensor_info.custom5.mipi_pixel_rate;
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
		LOG_INF("current fps :%d\n", *feature_data_32);
		spin_lock(&imgsensor_drv_lock);
		imgsensor.current_fps = (UINT16)*feature_data_32;
		spin_unlock(&imgsensor_drv_lock);
		break;
	case SENSOR_FEATURE_SET_HDR:
		LOG_INF("hdr enable :%d\n", *feature_data_32);
		spin_lock(&imgsensor_drv_lock);
		imgsensor.ihdr_en = (BOOL)*feature_data_32;
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
		case MSDK_SCENARIO_ID_CUSTOM1:
			memcpy((void *)wininfo, (void *)&imgsensor_winsize_info[5], sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_CUSTOM2:
			memcpy((void *)wininfo, (void *)&imgsensor_winsize_info[6], sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_CUSTOM3:
			memcpy((void *)wininfo, (void *)&imgsensor_winsize_info[7], sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_CUSTOM4:
			memcpy((void *)wininfo, (void *)&imgsensor_winsize_info[8], sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_CUSTOM5:
			memcpy((void *)wininfo, (void *)&imgsensor_winsize_info[9], sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
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
/*hs14 code for SR-AL6528A-01-60 by jianghongyan at 2022-11-16 end*/
static struct SENSOR_FUNCTION_STRUCT sensor_func = {
	open,
	get_info,
	get_resolution,
	feature_control,
	control,
	close
};

UINT32 A1402FRONTGC13A0XL_MIPI_RAW_SensorInit(struct SENSOR_FUNCTION_STRUCT **pfFunc)
{
	/* Check Sensor status here */
	if (pfFunc != NULL)
		*pfFunc = &sensor_func;
	return ERROR_NONE;
}
/*hs14 code for AL6528ADEU-1828 by jianghongyan at 2022-11-11 end*/
/*hs14 code for SR-AL6528A-01-88 by renxinglin at 2022-11-03 end*/