/* Marvell ISP S5K4H5 Driver
 *
 * Copyright (C) 2009-2010 Marvell International Ltd.
 *
 * Based on mt9v011 -Micron 1/4-Inch VGA Digital Image S5K3L2
 *
 * Copyright (c) 2009 Mauro Carvalho Chehab (mchehab@redhat.com)
 * This code is placed under the terms of the GNU General Public License v2
 */

#ifndef	B52_S5K4H5_H
#define	B52_S5K4H5_H

#include <media/b52-sensor.h>
#include <media/b52_api.h>
#define OTP_DRV_START_ADDR  0x7220
#define OTP_DRV_INFO_GROUP_COUNT  3
#define OTP_DRV_INFO_SIZE  5
#define OTP_DRV_AWB_GROUP_COUNT 3
#define OTP_DRV_AWB_SIZE  5
#define OTP_DRV_LSC_GROUP_COUNT  3
#define OTP_DRV_LSC_SIZE  62
#define OTP_DRV_LSC_REG_ADDR  0x5200
#define OTP_DRV_VCM_GROUP_COUNT  3
#define OTP_DRV_VCM_SIZE  3
#define bg_ratio_typical 0x400
#define rg_ratio_typical 0x400
struct regval_tab S5K4H5_res_init[] = {
	{0x0100, 0x00},
	{0x3000, 0x07},
	{0x3001, 0x05},
	{0x3002, 0x03},
	{0x3069, 0x00},
	{0x306A, 0x04},
	{0x0200, 0x06},
	{0x0201, 0x44},
	{0x3005, 0x03},
	{0x3006, 0x03},
	{0x3009, 0x0D},
	{0x300A, 0x03},
	{0x300C, 0x65},
	{0x300D, 0x54},
	{0x3010, 0x00},
	{0x3012, 0x14},
	{0x3014, 0x19},
	{0x3017, 0x0F},
	{0x3018, 0x1A},
	{0x3019, 0x6A},
	{0x301A, 0x72},
	{0x306F, 0x00},
	{0x3070, 0x00},
	{0x3071, 0x00},
	{0x3072, 0x00},
	{0x3073, 0x00},
	{0x3074, 0x00},
	{0x3075, 0x00},
	{0x3076, 0x0A},
	{0x3077, 0x03},
	{0x3078, 0x84},
	{0x3079, 0x00},
	{0x307A, 0x00},
	{0x307B, 0x00},
	{0x307C, 0x00},
	{0x3085, 0x00},
	{0x3086, 0x72},
	{0x30A6, 0x01},
	{0x30A7, 0x06},
	{0x3032, 0x01},
	{0x3037, 0x02},
	{0x304A, 0x01},
	{0x3054, 0xF0},
	{0x3044, 0x10},
	{0x3045, 0x20},
	{0x3047, 0x04},
	{0x3048, 0x14},
	{0x303D, 0x08},
	{0x304B, 0x44},
	{0x3063, 0x00},
	{0x302D, 0x7F},
	{0x3039, 0x45},
	{0x3038, 0x10},
	{0x3097, 0x11},
	{0x3096, 0x03},
	{0x3042, 0x01},
	{0x3053, 0x01},
	{0x303A, 0x0B},
	{0x320B, 0x40},
	{0x320C, 0x06},
	{0x320D, 0xC0},
	{0x3202, 0x00},
	{0x3203, 0x3D},
	{0x3204, 0x00},
	{0x3205, 0x3D},
	{0x3206, 0x00},
	{0x3207, 0x3D},
	{0x3208, 0x00},
	{0x3209, 0x3D},
	{0x3211, 0x02},
	{0x3212, 0x21},
	{0x3213, 0x02},
	{0x3214, 0x21},
	{0x3215, 0x02},
	{0x3216, 0x21},
	{0x3217, 0x02},
	{0x3218, 0x21},
	{0x3048, 0x14},
	{0x3244, 0x00},
	{0x3245, 0x00},
	{0x3246, 0x00},
	{0x3247, 0x00},
	{0x323f, 0x01},
	{0x3240, 0x01},
	{0x3241, 0x01},
	{0x3242, 0x01},
	{0x3264, 0x90},
	{0x3265, 0x90},
	{0x3266, 0x90},
	{0x3267, 0x90},
	{0x3269, 0x03},
	{0x3B29, 0x01},
	{0x0301, 0x02},
	{0x0303, 0x01},
	{0x0305, 0x06},
	{0x0306, 0x00},
	{0x0307, 0x81},
	{0x0309, 0x02},
	{0x030B, 0x01},
	{0x3C59, 0x00},
	{0x030D, 0x06},
	{0x030E, 0x00},
	{0x030F, 0xA6},
	{0x3C5A, 0x00},
	{0x3C50, 0x53},
	{0x3C62, 0x02},
	{0x3C63, 0xD0},
	{0x3C64, 0x00},
	{0x3C65, 0x00},
	{0x3C1E, 0x00},
	{0x3901, 0x02},
	{0x3500, 0x0C},
	{0x0310, 0x01},
	{0x3C1A, 0xEC},
	{0x0340, 0x09},
	{0x0341, 0xFF},
	{0x0342, 0x0E},
	{0x0343, 0xAA},
	{0x0344, 0x00},
	{0x0345, 0x08},
	{0x0346, 0x00},
	{0x0347, 0x08},
	{0x0348, 0x0C},
	{0x0349, 0xC7},
	{0x034A, 0x09},
	{0x034B, 0x97},
	{0x034C, 0x0C},
	{0x034D, 0xC0},
	{0x034E, 0x09},
	{0x034F, 0x90},
	{0x0390, 0x00},
	{0x0391, 0x00},
	{0x0940, 0x00},
	{0x0381, 0x01},
	{0x0383, 0x01},
	{0x0385, 0x01},
	{0x0387, 0x01},
	{0x0204, 0x00},
	{0x0205, 0x20},
	{0x3030, 0x1B},
	{0x0202, 0x04},
	{0x0203, 0xE2},
	{0x0200, 0x0C},
	{0x0201, 0x98},
	{0x0101, 0x03},
	{0x0204, 0x00},
	{0x0205, 0xf0},


};
struct regval_tab S5K4H5_fmt_raw10[] = {
};

struct regval_tab S5K4H5_res_720P[] = {

};

struct regval_tab S5K4H5_res_8M[] = {

};

struct regval_tab S5K4H5_res_video[] = {

};

struct regval_tab S5K4H5_id[] = {
	{0x0000, 0x48, 0xff},
	{0x0001, 0x5b, 0xff},
	{0x0002, 0x03, 0xff},
	{0x0003, 0x00, 0xff},
};
struct regval_tab  S5K4H5_vts[] = {
	{0x0340, 0x09, 0xff},
	{0x0341, 0xDB, 0xff},
};
struct regval_tab  S5K4H5_stream_on[] = {
	{0x0100, 0x01, 0xff},
};
struct regval_tab  S5K4H5_stream_off[] = {
	{0x0100, 0x00, 0xff},
};
struct regval_tab  S5K4H5_expo[] = {
	{0x0202, 0x09, 0xff},
	{0x0203, 0xDB, 0xff},
};
struct regval_tab S5K4H5_frationalexp[] = {
	{0x0200, 0x0E, 0xff},
	{0x0201, 0xAA, 0xff},
};
struct regval_tab  S5K4H5_ag[] = {
	{0x0204, 0x00, 0xff},
	{0x0205, 0x20, 0xff},
};
struct regval_tab  S5K4H5_dg[] = {
	{0x020e, 0x01, 0xff},
	{0x020f, 0x00, 0xff},
};
struct regval_tab S5K4H5_vflip[] = {
	{0x0101, 0x00, 0x02},

};
struct regval_tab S5K4H5_hflip[] = {
	{0x0101, 0x00, 0x01},
};
struct b52_sensor_i2c_attr S5K4H5_i2c_attr[] = {
	[0] = {
		.reg_len = I2C_16BIT,
		.val_len = I2C_8BIT,
		.addr = 0x37,
	},
};
static int ev_bias_offset[] = {
	-0x3C, -0x33, -0x2f, -0x29, -0x21, -0x11, 0,
	0x16, 0x2d, 0x47, 0x61, 0x4B, 0x5A
};
#define N_S5K4H5_I2C_ATTR ARRAY_SIZE(S5K4H5_i2c_attr)
#define N_S5K4H5_INIT ARRAY_SIZE(S5K4H5_res_init)
#define N_S5K4H5_ID ARRAY_SIZE(S5K4H5_id)
#define N_S5K4H5_FMT_RAW10 ARRAY_SIZE(S5K4H5_fmt_raw10)
#define N_S5K4H5_720P ARRAY_SIZE(S5K4H5_res_720P)
#define N_S5K4H5_8M ARRAY_SIZE(S5K4H5_res_8M)
#define N_S5K4H5_VTS ARRAY_SIZE(S5K4H5_vts)
#define N_S5K4H5_EXPO ARRAY_SIZE(S5K4H5_expo)
#define N_S5K4H5_FRATIONALEXPO ARRAY_SIZE(S5K4H5_frationalexp)
#define N_S5K4H5_AG ARRAY_SIZE(S5K4H5_ag)
#define N_S5K4H5_DG ARRAY_SIZE(S5K4H5_dg)
#define N_S5K4H5_VFLIP ARRAY_SIZE(S5K4H5_vflip)
#define N_S5K4H5_HFLIP ARRAY_SIZE(S5K4H5_hflip)
#define N_S5K4H5_STREAM_ON ARRAY_SIZE(S5K4H5_stream_on)
#define N_S5K4H5_STREAM_OFF ARRAY_SIZE(S5K4H5_stream_off)
#define N_S5K4H5_video ARRAY_SIZE(S5K4H5_res_video)

struct b52_sensor_mbus_fmt S5K4H5_fmt = {
	.mbus_code	= V4L2_MBUS_FMT_SGRBG10_1X10,
	.colorspace	= V4L2_COLORSPACE_SRGB,
	.regs = {
		.tab = S5K4H5_fmt_raw10,
		.num = N_S5K4H5_FMT_RAW10,
	}
};
struct b52_sensor_resolution S5K4H5_res[] = {
	[0] = {
		 .width = 3264,
		 .height = 2448,
		 .hts = 0x0EAA,
		 .min_vts = 0x09FF,
		 .sensor_mode = SENSOR_PREVIEW_MODE,
		 .prop = SENSOR_RES_BINING1,
		 .regs = {
			.tab = S5K4H5_res_8M,
			.num = N_S5K4H5_8M,
		},
	},

	[1] = {
		 .width = 3264,
		 .height = 2448,
		 .hts = 0x0EAA,
		 .min_vts = 0x09DB,
		 .sensor_mode = SENSOR_VIDEO_MODE,
		 .prop = SENSOR_RES_BINING1,
		 .regs = {
			.tab = S5K4H5_res_video,
			.num = N_S5K4H5_video,
		},
	},

};
static struct b52_sensor_i2c_attr vcm_attr = {
	.reg_len = I2C_8BIT,
	.val_len = I2C_8BIT,
	.addr = 0x0c,
};
static struct b52_sensor_vcm vcm_dw9807 = {
	.type = VCM_DW9807,
	.attr = &vcm_attr,
};
static struct b52_sensor_module S5K4H5_SSG = {
	.vcm = &vcm_dw9807,
	.id = 0,
};
static int S5K4H5_get_pixelclock(struct v4l2_subdev *sd, u32 *rate, u32 mclk);
static int S5K4H5_get_dphy_desc(struct v4l2_subdev *sd,
			struct csi_dphy_desc *dphy_desc, u32 mclk);
static int S5K4H5_update_otp(struct v4l2_subdev *sd,
				struct b52_sensor_otp *opt);
static int S5K4H5_s_power(struct v4l2_subdev *sd, int on);

struct b52_sensor_spec_ops S5K4H5_ops = {
	.get_pixel_rate = S5K4H5_get_pixelclock,
	.get_dphy_desc = S5K4H5_get_dphy_desc,
	.update_otp = S5K4H5_update_otp,
	.s_power = S5K4H5_s_power,
};
struct b52_sensor_data b52_s5k4h5 = {
	.name = "samsung.s5k4h5",
	.type = SAMSUNG_ASNY_SENSOR,
	.i2c_attr = S5K4H5_i2c_attr,
	.num_i2c_attr = N_S5K4H5_I2C_ATTR,
	.id = {
		.tab = S5K4H5_id,
		.num = N_S5K4H5_ID,
	},
	.global_setting = {
		.tab = S5K4H5_res_init,
		.num = N_S5K4H5_INIT,
	},
	.mbus_fmt = &S5K4H5_fmt,
	.num_mbus_fmt = 1,
	.res = S5K4H5_res,
	.num_res = 2,
	.streamon = {
		.tab = S5K4H5_stream_on,
		.num = N_S5K4H5_STREAM_ON,
	},
	.streamoff = {
		.tab = S5K4H5_stream_off,
		.num = N_S5K4H5_STREAM_OFF,
	},
	.gain2iso_ratio = {
		.numerator = 100,
		.denominator = 0x10,
	},
	.vts_range = {0x09DB, 0x7fff},
	.gain_range = {
		[B52_SENSOR_AG] = {0x0010, 0x00ff},
		[B52_SENSOR_DG] = {0x0010, 0x0020},
	},
	.expo_range = {0x0004, 0x09FF},
	.frationalexp_range = {0x00000, 0x11a0},
	.focus_range = {0x0010, 0x03ff},
	.vts_reg = {
		.tab = S5K4H5_vts,
		.num = N_S5K4H5_VTS,
	},
	.expo_reg = {
		.tab = S5K4H5_expo,
		.num = N_S5K4H5_EXPO,
	},
	.frationalexp_reg = {
		.tab = S5K4H5_frationalexp,
		.num = N_S5K4H5_FRATIONALEXPO,
	},
	.gain_reg = {
		[B52_SENSOR_AG] = {
			.tab = S5K4H5_ag,
			.num = N_S5K4H5_AG,
		},
		[B52_SENSOR_DG] = {
			.tab = S5K4H5_dg,
			.num = N_S5K4H5_DG,
		},
	},
	.hflip = {
		.tab = S5K4H5_hflip,
		.num = N_S5K4H5_HFLIP,
	},
	.vflip = {
		.tab = S5K4H5_vflip,
		.num = N_S5K4H5_VFLIP,
	},
	.ev_bias_offset = ev_bias_offset,
	.flip_change_phase =  0,
	.dgain_channel = 4,
	/* A gain format is 8.5 */
	.gain_shift = 0x00,
	/* A expo format is 2 byte */
	.expo_shift = 0x08,
	.calc_dphy = 1,
	.nr_lane = 4,
	.mipi_clk_bps = 719942000,
	.ops = &S5K4H5_ops,
	.module = &S5K4H5_SSG,
	.num_module =  1,
	.reset_delay = 10000,
};

#endif
