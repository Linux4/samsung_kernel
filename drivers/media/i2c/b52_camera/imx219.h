/* Marvell ISP IMX219 Driver
 *
 * Copyright (C) 2009-2010 Marvell International Ltd.
 *
 * Based on mt9v011 -Micron 1/4-Inch VGA Digital Image IMX219
 *
 * Copyright (c) 2009 Mauro Carvalho Chehab (mchehab@redhat.com)
 * This code is placed under the terms of the GNU General Public License v2
 */

#ifndef	B52_IMX219_H
#define	B52_IMX219_H

#include <media/b52_api.h>
#include <media/b52-sensor.h>
#include <media/b52_api.h>
#define OTP_DRV_INDEX_ADDR  0x3202
#define OTP_DRV_GROUP_COUNT 2
#define OTP_DRV_GROUP1_FLAG 0xC0
#define OTP_DRV_GROUP2_FLAG 0x30
#define OTP_DRV_GROUP_FLAG2_VAL 0x10
#define OTP_DRV_GROUP_FLAG1_VAL 0x40

#define OTP_DRV_LSC_SIZE 175

/* raw10,XVCLK=26Mhz, SCLK=280.8Mhz, MIPI 702Mbps*/
struct regval_tab imx219_res_init[] = {
	{0x30EB, 0x05},
	{0x30EB, 0x0C},
	{0x300A, 0xFF},
	{0x300B, 0xFF},
	{0x30EB, 0x05},
	{0x30EB, 0x09},

	{0x0114, 0x03},
	{0x0128, 0x00},
	{0x012A, 0x1A},
	{0x012B, 0x00},
	{0x0160, 0x0A},
	{0x0161, 0x9B},
	{0x0162, 0x0D},
	{0x0163, 0x78},
	{0x0164, 0x00},
	{0x0165, 0x00},
	{0x0166, 0x0C},
	{0x0167, 0xC7},
	{0x0168, 0x00},
	{0x0169, 0x08},
	{0x016A, 0x09},
	{0x016B, 0x97},
	{0x016C, 0x0C},
	{0x016D, 0xC0},
	{0x016E, 0x09},
	{0x016F, 0x90},
	{0x0170, 0x01},
	{0x0171, 0x01},
	{0x0174, 0x00},
	{0x0175, 0x00},
	{0x018C, 0x0A},
	{0x018D, 0x0A},
	{0x0301, 0x05},
	{0x0303, 0x01},
	{0x0304, 0x03},
	{0x0305, 0x03},
	{0x0306, 0x00},
	{0x0307, 0x51},
	{0x0309, 0x0A},
	{0x030B, 0x01},
	{0x030C, 0x00},
	{0x030D, 0x54},
	{0x455E, 0x00},
	{0x471E, 0x4B},
	{0x4767, 0x0F},
	{0x4750, 0x14},
	{0x4540, 0x00},
	{0x47B4, 0x14},
	{0x4713, 0x30},
	{0x478B, 0x10},
	{0x478F, 0x10},
	{0x4793, 0x10},
	{0x4797, 0x0E},
	{0x479B, 0x0E},

	{0x5041, 0x00},

};
struct regval_tab imx219_fmt_raw10[] = {
};

struct regval_tab imx219_res_4M[] = {
};
struct regval_tab imx219_res_8M[] = {
};
struct regval_tab imx219_res_13M[] = {
};
struct regval_tab imx219_id[] = {
	{0x0000, 0x02, 0xff},
	{0x0001, 0x19, 0xff},
};
struct regval_tab imx219_vts[] = {
	{0x0160, 0x0A, 0x7f},
	{0x0161, 0x9B, 0xff},
};
struct regval_tab imx219_stream_on[] = {
	{0x0100, 0x01, 0xff},
};
struct regval_tab imx219_stream_off[] = {
	{0x0100, 0x00, 0xff},
};
struct regval_tab imx219_expo[] = {
	{0x015A, 0x00, 0xff},
	{0x015B, 0x00, 0xff},
};
struct regval_tab imx219_ag[] = {
	{0x0157, 0x00, 0xff},
};
struct regval_tab imx219_dg[] = {
	{0x0158, 0x00, 0xff},
	{0x0159, 0x00, 0xff},
};
struct regval_tab imx219_vcm_id[] = {
	{0x00, 0xF1, 0xff},
};
struct regval_tab imx219_vcm_init[] = {
	/* control register */
	{0x02, 0x02, 0x03},
	/* mode register */
	{0x06, 0x00, 0xC0},
	/* period register */
	{0x07, 0x7C, 0xFF},
};
struct regval_tab imx219_af[] = {
};
struct regval_tab imx219_vflip[] = {
	{0x0172, 0x02, 0x2},
};
struct regval_tab imx219_hflip[] = {
	{0x0172, 0x01, 0x1},
};
struct b52_sensor_i2c_attr imx219_i2c_attr[] = {
	[0] = {
		.reg_len = I2C_16BIT,
		.val_len = I2C_8BIT,
		.addr = 0x1A,
	},
};
#define N_IMX219_I2C_ATTR ARRAY_SIZE(imx219_i2c_attr)
#define N_IMX219_INIT ARRAY_SIZE(imx219_res_init)
#define N_IMX219_ID ARRAY_SIZE(imx219_id)
#define N_IMX219_VCM_ID ARRAY_SIZE(imx219_vcm_id)
#define N_IMX219_FMT_RAW10 ARRAY_SIZE(imx219_fmt_raw10)
#define N_IMX219_13M ARRAY_SIZE(imx219_res_13M)
#define N_IMX219_4M ARRAY_SIZE(imx219_res_4M)
#define N_IMX219_8M ARRAY_SIZE(imx219_res_8M)
#define N_IMX219_VTS ARRAY_SIZE(imx219_vts)
#define N_IMX219_EXPO ARRAY_SIZE(imx219_expo)
#define N_IMX219_AG ARRAY_SIZE(imx219_ag)
#define N_IMX219_DG ARRAY_SIZE(imx219_dg)
#define N_IMX219_AF ARRAY_SIZE(imx219_af)
#define N_IMX219_STREAM_ON ARRAY_SIZE(imx219_stream_on)
#define N_IMX219_STREAM_OFF ARRAY_SIZE(imx219_stream_off)
#define N_IMX219_VCM_INIT ARRAY_SIZE(imx219_vcm_init)
#define N_IMX219_VFLIP ARRAY_SIZE(imx219_vflip)
#define N_IMX219_HFLIP ARRAY_SIZE(imx219_hflip)
struct b52_sensor_mbus_fmt imx219_fmt = {
	.mbus_code	= V4L2_MBUS_FMT_SBGGR10_1X10,
	.colorspace	= V4L2_COLORSPACE_SRGB,
	.regs = {
		.tab = imx219_fmt_raw10,
		.num = N_IMX219_FMT_RAW10,
	}
};
struct b52_sensor_resolution imx219_res[] = {
	[0] = {
		 .width = 3264,
		 .height = 2448,
		 .hts = 0x0D78,
		 .min_vts = 0x0A9B,
		 .prop = SENSOR_RES_BINING1,
		 .regs = {
			.tab = imx219_res_8M,
			.num = N_IMX219_8M,
		},
	},
#if 0
	[1] = {
		 .width = 2112,
		 .height = 1568,
		 .hts = 0x1580,
		 .min_vts = 0x0ba0,
		 .prop = SENSOR_RES_BINING2,
		 .regs = {
			.tab = imx219_res_4M,
			.num = N_IMX219_4M,
		},
	},
#endif
};
static struct b52_sensor_i2c_attr vcm_attr = {
	.reg_len = I2C_8BIT,
	.val_len = I2C_8BIT,
	.addr = 0x0c,
};
static struct b52_sensor_vcm vcm_dw9804 = {
	.name = "dw9804",
	.type = VCM_DW9804,
	.attr = &vcm_attr,
	.pos_reg_msb = 0x03,
	.pos_reg_lsb = 0x04,
	.id = {
		.tab = imx219_vcm_id,
		.num = N_IMX219_VCM_ID,
	},
	.init = {
		.tab = imx219_vcm_init,
		.num = N_IMX219_VCM_INIT,
	},
};
static struct b52_sensor_module imx219_SSG = {
	.vcm = &vcm_dw9804,
};
static int IMX219_get_pixelclock(struct v4l2_subdev *sd, u32 *rate, u32 mclk);
static int IMX219_get_dphy_desc(struct v4l2_subdev *sd,
			struct csi_dphy_desc *dphy_desc, u32 mclk);
static int IMX219_update_otp(struct v4l2_subdev *sd,
				struct b52_sensor_otp *opt);

struct b52_sensor_spec_ops imx219_ops = {
	.get_pixel_rate = IMX219_get_pixelclock,
	.get_dphy_desc = IMX219_get_dphy_desc,
	.update_otp = IMX219_update_otp,
	.s_power = NULL,
};
struct b52_sensor_data b52_imx219 = {
	.name = "imx219",
	.type = SONY_SENSOR,
	.i2c_attr = imx219_i2c_attr,
	.num_i2c_attr = N_IMX219_I2C_ATTR,
	.id = {
		.tab = imx219_id,
		.num = N_IMX219_ID,
	},
	.global_setting = {
		.tab = imx219_res_init,
		.num = N_IMX219_INIT,
	},
	.mbus_fmt = &imx219_fmt,
	.num_mbus_fmt = 1,
	.res = imx219_res,
	.num_res = 1,
	.streamon = {
		.tab = imx219_stream_on,
		.num = N_IMX219_STREAM_ON,
	},
	.streamoff = {
		.tab = imx219_stream_off,
		.num = N_IMX219_STREAM_OFF,
	},
	.gain2iso_ratio = {
		.numerator = 100,
		.denominator = 0x10,
	},
	.vts_range = {0X0100, 0xfffe},
	.gain_range = {
		[B52_SENSOR_AG] = {0x0010, 0x80},
		[B52_SENSOR_DG] = {0x0010, 0x20},
	},
	.expo_range = {0x0001, 0x0a99},
	.focus_range = {0x0010, 0x03ff},
	.vts_reg = {
		.tab = imx219_vts,
		.num = N_IMX219_VTS,
	},
	.expo_reg = {
		.tab = imx219_expo,
		.num = N_IMX219_EXPO,
	},
	.gain_reg = {
		[B52_SENSOR_AG] = {
			.tab = imx219_ag,
			.num = N_IMX219_AG,
		},
		[B52_SENSOR_DG] = {
			.tab = imx219_dg,
			.num = N_IMX219_DG,
		},
	},
	.af_reg = {
		.tab = imx219_af,
		.num = N_IMX219_AF,
	},
	.hflip = {
		.tab = imx219_hflip,
		.num = N_IMX219_HFLIP,
	},
	.vflip = {
		.tab = imx219_vflip,
		.num = N_IMX219_VFLIP,
	},

	.flip_change_phase = 1,
	.gain_shift = 0x00,
	.expo_shift = 0x08,
	.calc_dphy = 1,
	.nr_lane = 4,
	.mipi_clk_bps = 728000000,
	.ops = &imx219_ops,
	.module = &imx219_SSG,
	.num_module =  1,
};

#endif
