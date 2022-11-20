/*
 * Samsung Exynos5 SoC series Sensor driver
 *
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/version.h>
#include <linux/gpio.h>
#include <linux/clk.h>
#include <linux/regulator/consumer.h>
#include <linux/videodev2.h>
#include <linux/videodev2_exynos_camera_ext.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/vmalloc.h>
#include <linux/platform_device.h>
#ifdef CONFIG_OF
#include <linux/of_gpio.h>
#endif
#include <linux/workqueue.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-device.h>
#include <media/v4l2-subdev.h>
#include <mach/exynos-fimc-is2-sensor.h>

#include "fimc-is-core.h"
#include "fimc-is-device-sensor.h"

#include "fimc-is-helper-i2c.h"

#include "fimc-is-resourcemgr.h"
#include "fimc-is-hw.h"
#include "fimc-is-dt.h"
#include "fimc-is-device-4ec-soc.h"

#if defined(CONFIG_CAMERA_GTES)
#include "fimc-is-device-4ec-soc-reg-gtes.h" /* 4ECGX */
#elif defined(CONFIG_CAMERA_J1XLTE)
#include "fimc-is-device-4ec-soc-reg.h" /* 4ECGX */
#include "fimc-is-device-4ec-soc-reg_ga.h" /* 4ECGA */
#elif defined(CONFIG_CAMERA_J3XLTE)
#include "fimc-is-device-4ec-soc-reg-j3xlte_ga.h" /* 4ECGA */
#elif defined(CONFIG_CAMERA_ON5LTE)
#include "fimc-is-device-4ec-soc-reg-j3xlte_ga.h" /* 4ECGA : same setfile with j3xlte */
#elif defined(CONFIG_CAMERA_XCOVER3VELTE)
#include "fimc-is-device-4ec-soc-reg_ga.h" /* 4ECGA */
#else /* J2LTE */
#include "fimc-is-device-4ec-soc-reg.h" /* 4ECGX */
#endif

#if defined(CONFIG_FLED_SM5703)
#include <linux/leds/sm5703_fled.h>
extern int sm5703_led_mode_ctrl(int state);
extern bool flash_control_ready;
#elif defined(CONFIG_LEDS_S2MU005)
#include <linux/leds-s2mu005.h>
extern int s2mu005_led_mode_ctrl(int state);
#endif

#define SENSOR_NAME "S5K4EC"
#define SENSOR_NAME_4ECGX04	"S5K4ECGX"
#define SENSOR_NAME_4ECGA04	"S5K4ECGA"
#define SENSOR_VERSION_4ECGX04	0x0404
#define SENSOR_VERSION_4ECGA04	0x1414

#define DEFAULT_SENSOR_WIDTH	640
#define DEFAULT_SENSOR_HEIGHT	480
#define SENSOR_MEMSIZE DEFAULT_SENSOR_WIDTH * DEFAULT_SENSOR_HEIGHT

#define NORMAL_FRAME_DELAY_MS     100
#define FLASH_AE_DELAY_MS     200
#define MAX_SOFTLANDING_DELAY_MS     450
#define CONST_SOFTLANDING_DELAY_AUTO_MS		120
#define CONST_SOFTLANDING_DELAY_MACRO_MS	200
#define POLL_TIME_MS		10
#define CAPTURE_POLL_TIME_MS    1000

#define FIRST_AF_SEARCH_COUNT   80
#define SECOND_AF_SEARCH_COUNT  80
#define AE_STABLE_SEARCH_COUNT  4
#define LOW_LIGHT_LEVEL	0x13

#define EV_MIN_VALUE  EV_MINUS_4
#define CONTRAST_MIN_VALUE  CONTRAST_MINUS_2
#define SATURATION_MIN_VALUE  SATURATION_MINUS_2
#define SHARPNESS_MIN_VALUE  SHARPNESS_MINUS_2
#define GET_CID_INDEX(CID, CID_MIN_VALUE) ((CID) - (CID_MIN_VALUE))

#define IS_LOWLIGHT(x)	((x <= LOW_LIGHT_LEVEL) ? 1 : 0)

#ifdef SUPPORT_MULTIPLE_SENSOR_VERSION
enum s5k4ec_sensor_setfile_index {
	SETFILE_INDEX_4ECGX04 = 0,
	SETFILE_INDEX_4ECGA04 = 1,
	SETFILE_INDEX_MAX,
};
#else
enum s5k4ec_sensor_setfile_index {
	SETFILE_INDEX_4EC_DEFAULT = 0,
	SETFILE_INDEX_MAX,
};
#endif

enum s5k4ecgx_preview_frame_size {
	S5K4ECGX_PREVIEW_144_176 = 0,	/* 144x176 */
	S5K4ECGX_PREVIEW_QCIF,		/* 176x144 */
	S5K4ECGX_PREVIEW_CIF,		/* 352x288 */
	S5K4ECGX_PREVIEW_QVGA,		/* 320x240 */
	S5K4ECGX_PREVIEW_VGA,		/* 640x480 */
	S5K4ECGX_PREVIEW_704_576,	/* 704x576 */
	S5K4ECGX_PREVIEW_D1,		/* 720x480 */
	S5K4ECGX_PREVIEW_720_540,	/* 720x540 */
	S5K4ECGX_PREVIEW_720_720,	/* 720x720 */
	S5K4ECGX_PREVIEW_WVGA,		/* 800x480 */
	S5K4ECGX_PREVIEW_SVGA,		/* 800x600 */
	S5K4ECGX_PREVIEW_880,		/* 880x720 */
	S5K4ECGX_PREVIEW_960_540,	/* 960x540 */
	S5K4ECGX_PREVIEW_960_720,	/* 960x720 */
	S5K4ECGX_PREVIEW_960,		/* 960x960 */
	S5K4ECGX_PREVIEW_964_964,	/* 964x964 */
	S5K4ECGX_PREVIEW_WSVGA,		/* 1024x600*/
	S5K4ECGX_PREVIEW_1024,		/* 1024x768*/
	S5K4ECGX_PREVIEW_W1280,		/* 1280x720*/
	S5K4ECGX_PREVIEW_WXGA,		/* 1280x768*/
	S5K4ECGX_PREVIEW_1280,		/* 1280x960*/
	S5K4ECGX_PREVIEW_1288_772,	/* 1288x772*/
	S5K4ECGX_PREVIEW_1288_966,	/* 1288x966*/
	S5K4ECGX_PREVIEW_1600,		/* 1600x1200*/
	S5K4ECGX_PREVIEW_1920,		/* 1920x1080*/
	S5K4ECGX_PREVIEW_2048,		/* 2048x1536*/
	S5K4ECGX_PREVIEW_2560,		/* 2560x1920*/
	S5K4ECGX_PREVIEW_MAX,
};

enum s5k4ecgx_capture_frame_size {
	S5K4ECGX_CAPTURE_QVGA = 0,	/* 640x480 */
	S5K4ECGX_CAPTURE_VGA,		/* 320x240 */
	S5K4ECGX_CAPTURE_WVGA,		/* 800x480 */
	S5K4ECGX_CAPTURE_SVGA,		/* 800x600 */
	S5K4ECGX_CAPTURE_WSVGA,		/* 1024x600 */
	S5K4ECGX_CAPTURE_XGA,		/* 1024x768 */
	S5K4ECGX_CAPTURE_1MP,		/* 1280x960 */
	S5K4ECGX_CAPTURE_W1MP,		/* 1600x960 */
	S5K4ECGX_CAPTURE_2MP,		/* UXGA  - 1600x1200 */
	S5K4ECGX_CAPTURE_1632_1218,	/* 1632x1218 */
	S5K4ECGX_CAPTURE_1920_1920,	/* 1920x1920 */
	S5K4ECGX_CAPTURE_1928_1928,	/* 1928x1928 */
	S5K4ECGX_CAPTURE_2036_1526,	/* 2036x1526 */
	S5K4ECGX_CAPTURE_2048_1152,	/* 2048x1152 */
	S5K4ECGX_CAPTURE_2048_1232,	/* 2048x1232 */
	S5K4ECGX_CAPTURE_W2MP,		/* 35mm Academy Offset Standard 1.66 */
					/* 2048x1232, 2.4MP */
	S5K4ECGX_CAPTURE_3MP,		/* QXGA  - 2048x1536 */
	S5K4ECGX_CAPTURE_2560_1440,	/* 2560x1440 */
	S5K4ECGX_CAPTURE_W4MP,		/* WQXGA - 2560x1536 */
	S5K4ECGX_CAPTURE_5MP,		/* 2560x1920 */
	S5K4ECGX_CAPTURE_2576_1544,	/* 2576x1544 */
	S5K4ECGX_CAPTURE_2576_1932,	/* 2576x1932 */
	S5K4ECGX_CAPTURE_MAX,
};

enum s5k4ecgx_oprmode {
	S5K4ECGX_OPRMODE_VIDEO = 0,
	S5K4ECGX_OPRMODE_IMAGE = 1,
	S5K4ECGX_OPRMODE_MAX,
};

enum s5k4ecgx_flicker_mode {
	S5K4ECGX_FLICKER_50HZ_AUTO = 0,
	S5K4ECGX_FLICKER_50HZ_FIXED,
	S5K4ECGX_FLICKER_60HZ_AUTO,
	S5K4ECGX_FLICKER_60HZ_FIXED,
	S5K4ECGX_FLICKER_MAX,
};

struct s5k4ecgx_regset_table {
	const u32	*reg[SETFILE_INDEX_MAX];
	const char	*setting_name[SETFILE_INDEX_MAX];
	int		array_size[SETFILE_INDEX_MAX];
};

static const struct s5k4ecgx_framesize preview_size_list[] = {
	{ S5K4ECGX_PREVIEW_144_176,	144,   176  },
	{ S5K4ECGX_PREVIEW_QCIF,	176,   144  },
	{ S5K4ECGX_PREVIEW_QVGA,	320,   240  },
	{ S5K4ECGX_PREVIEW_CIF,		352,   288  },
	{ S5K4ECGX_PREVIEW_VGA,		640,   480  },
	{ S5K4ECGX_PREVIEW_704_576,	704,   576  },
	{ S5K4ECGX_PREVIEW_D1,		720,   480  },
	{ S5K4ECGX_PREVIEW_720_540,	720,   540  },
	{ S5K4ECGX_PREVIEW_720_720,	720,   720  },
	{ S5K4ECGX_PREVIEW_WVGA,	800,   480  },
	{ S5K4ECGX_PREVIEW_960_540,	960,   540  },
	{ S5K4ECGX_PREVIEW_960_720,	960,   720  },
	{ S5K4ECGX_PREVIEW_960,		960,   960  },
	{ S5K4ECGX_PREVIEW_964_964,	964,   964  },
	{ S5K4ECGX_PREVIEW_1024,	1024,  768  },
	{ S5K4ECGX_PREVIEW_W1280,	1280,  720  },
	{ S5K4ECGX_PREVIEW_WXGA,	1280,  768  },
	{ S5K4ECGX_PREVIEW_1280,	1280,  960  },
	{ S5K4ECGX_PREVIEW_1288_772,	1288,  772  },
	{ S5K4ECGX_PREVIEW_1288_966,	1288,  966  },
	{ S5K4ECGX_PREVIEW_1920,	1920,  1080 },
	{ S5K4ECGX_PREVIEW_1600,	1600,  1200 },
	{ S5K4ECGX_PREVIEW_2048,	2048,  1536 },
	{ S5K4ECGX_PREVIEW_2560,	2560,  1920 },
};

static const struct s5k4ecgx_framesize capture_size_list[] = {
	{ S5K4ECGX_CAPTURE_VGA,		640,  480  },
	{ S5K4ECGX_CAPTURE_WVGA,	800,  480  },
	{ S5K4ECGX_CAPTURE_XGA,		1024, 768  },
	{ S5K4ECGX_CAPTURE_1MP,		1280, 960  },
	{ S5K4ECGX_CAPTURE_W1MP,	1600, 960  },
	{ S5K4ECGX_CAPTURE_2MP,		1600, 1200 },
	{ S5K4ECGX_CAPTURE_1632_1218,	1632, 1218 },
	{ S5K4ECGX_CAPTURE_1920_1920,	1920, 1920 },
	{ S5K4ECGX_CAPTURE_1928_1928,	1928, 1928 },
	{ S5K4ECGX_CAPTURE_2036_1526,	2036, 1526 },
	{ S5K4ECGX_CAPTURE_2048_1152,	2048, 1152 },
	{ S5K4ECGX_CAPTURE_2048_1232,	2048, 1232 },
	{ S5K4ECGX_CAPTURE_3MP,		2048, 1536 },
	{ S5K4ECGX_CAPTURE_2560_1440,	2560, 1440 },
	{ S5K4ECGX_CAPTURE_W4MP,	2560, 1536 },
	{ S5K4ECGX_CAPTURE_5MP,		2560, 1920 },
	{ S5K4ECGX_CAPTURE_2576_1544,	2576, 1544 },
	{ S5K4ECGX_CAPTURE_2576_1932,	2576, 1932 },
};

#ifdef SUPPORT_MULTIPLE_SENSOR_VERSION
#define S5K4ECGX_REGSET_TABLE(y)			\
	{						\
		.reg		= { s5k4ecgx_##y,		\
				s5k4ecga_##y },			\
		.setting_name	= { "s5k4ecgx_"#y,	\
					"s5k4ecga_"#y },		\
		.array_size	= { ARRAY_SIZE(s5k4ecgx_##y),	\
					ARRAY_SIZE(s5k4ecga_##y) },	\
	}
#else
#ifdef HAVE_SETFILE_VERSION_4ECGA04
#define S5K4ECGX_REGSET_TABLE(y)			\
	{						\
		.reg		= { s5k4ecga_##y, },	\
		.setting_name	= { "s5k4ecga_"#y, }, 	\
		.array_size	= { ARRAY_SIZE(s5k4ecga_##y), },	\
	}
#else
#define S5K4ECGX_REGSET_TABLE(y)			\
	{						\
		.reg		= { s5k4ecgx_##y, },	\
		.setting_name	= { "s5k4ecgx_"#y, }, 	\
		.array_size	= { ARRAY_SIZE(s5k4ecgx_##y), },	\
	}
#endif
#endif

#define S5K4ECGX_REGSET(x, y)				\
	[(x)] = S5K4ECGX_REGSET_TABLE(y)

struct s5k4ecgx_regs {
	struct s5k4ecgx_regset_table ev[GET_CID_INDEX(EV_MAX, EV_MIN_VALUE)];
	struct s5k4ecgx_regset_table metering[METERING_MAX];
	struct s5k4ecgx_regset_table iso[ISO_MAX];
	struct s5k4ecgx_regset_table effect[IMAGE_EFFECT_MAX];
	struct s5k4ecgx_regset_table white_balance[WHITE_BALANCE_MAX];
	struct s5k4ecgx_regset_table preview_size[S5K4ECGX_PREVIEW_MAX];
	struct s5k4ecgx_regset_table capture_size[S5K4ECGX_CAPTURE_MAX];
	struct s5k4ecgx_regset_table scene_mode[SCENE_MODE_MAX];
	struct s5k4ecgx_regset_table saturation[SATURATION_MAX];
	struct s5k4ecgx_regset_table contrast[CONTRAST_MAX];
	struct s5k4ecgx_regset_table sharpness[SHARPNESS_MAX];
	struct s5k4ecgx_regset_table anti_banding[S5K4ECGX_FLICKER_MAX];
	struct s5k4ecgx_regset_table fps[FRAME_RATE_MAX];
	struct s5k4ecgx_regset_table preview_return;
	struct s5k4ecgx_regset_table jpeg_quality_high;
	struct s5k4ecgx_regset_table jpeg_quality_normal;
	struct s5k4ecgx_regset_table jpeg_quality_low;
	struct s5k4ecgx_regset_table flash_start;
	struct s5k4ecgx_regset_table flash_end;
	struct s5k4ecgx_regset_table af_assist_flash_start;
	struct s5k4ecgx_regset_table af_assist_flash_end;
	struct s5k4ecgx_regset_table af_low_light_mode_on;
	struct s5k4ecgx_regset_table af_low_light_mode_off;
	struct s5k4ecgx_regset_table aeawb_lockunlock[AE_AWB_MAX];
	struct s5k4ecgx_regset_table ae_lockunlock[AE_LOCK_MAX];
	struct s5k4ecgx_regset_table awb_lockunlock[AWB_LOCK_MAX];
	struct s5k4ecgx_regset_table wdr_on;
	struct s5k4ecgx_regset_table wdr_off;
	struct s5k4ecgx_regset_table face_detection_on;
	struct s5k4ecgx_regset_table face_detection_off;
	struct s5k4ecgx_regset_table capture_start;
	struct s5k4ecgx_regset_table normal_snapshot;
	struct s5k4ecgx_regset_table camcorder;
	struct s5k4ecgx_regset_table camcorder_disable;
	struct s5k4ecgx_regset_table af_macro_mode_1;
	struct s5k4ecgx_regset_table af_macro_mode_2;
	struct s5k4ecgx_regset_table af_macro_mode_3;
	struct s5k4ecgx_regset_table af_normal_mode_1;
	struct s5k4ecgx_regset_table af_normal_mode_2;
	struct s5k4ecgx_regset_table af_normal_mode_3;
	struct s5k4ecgx_regset_table af_return_macro_position;
	struct s5k4ecgx_regset_table single_af_start;
	struct s5k4ecgx_regset_table single_af_off_1;
	struct s5k4ecgx_regset_table single_af_off_2;
	struct s5k4ecgx_regset_table continuous_af_on;
	struct s5k4ecgx_regset_table continuous_af_off;
	struct s5k4ecgx_regset_table dtp_start;
	struct s5k4ecgx_regset_table dtp_stop;
	struct s5k4ecgx_regset_table init_reg_1;
	struct s5k4ecgx_regset_table init_reg_2;
	struct s5k4ecgx_regset_table init_reg_3; //many add
	struct s5k4ecgx_regset_table init_reg_4; //many add
	struct s5k4ecgx_regset_table flash_init;
	struct s5k4ecgx_regset_table reset_crop;
	struct s5k4ecgx_regset_table fast_ae_on;
	struct s5k4ecgx_regset_table fast_ae_off;
	struct s5k4ecgx_regset_table get_ae_stable_status;
	struct s5k4ecgx_regset_table get_light_level;
	struct s5k4ecgx_regset_table get_frame_duration;
	struct s5k4ecgx_regset_table get_1st_af_search_status;
	struct s5k4ecgx_regset_table get_2nd_af_search_status;
	struct s5k4ecgx_regset_table get_capture_status;
	struct s5k4ecgx_regset_table get_esd_status;
	struct s5k4ecgx_regset_table get_iso;
	struct s5k4ecgx_regset_table get_shutterspeed;
	struct s5k4ecgx_regset_table get_exptime;
	struct s5k4ecgx_regset_table get_frame_count;
	struct s5k4ecgx_regset_table get_preview_status;
	struct s5k4ecgx_regset_table get_pid;
	struct s5k4ecgx_regset_table get_revision;
	struct s5k4ecgx_regset_table get_modechange_check;
	struct s5k4ecgx_regset_table set_vendor_id_read;
	struct s5k4ecgx_regset_table get_vendor_id_read;
};

static const struct s5k4ecgx_regs regs_set =
{
	.init_reg_1 = S5K4ECGX_REGSET_TABLE(init_reg1),
	.init_reg_2 = S5K4ECGX_REGSET_TABLE(init_reg2),
	.init_reg_3 = S5K4ECGX_REGSET_TABLE(init_reg3),
	.init_reg_4 = S5K4ECGX_REGSET_TABLE(init_reg4), //many add

	.dtp_start = S5K4ECGX_REGSET_TABLE(DTP_init),
	.dtp_stop = S5K4ECGX_REGSET_TABLE(DTP_stop),

	.capture_start = S5K4ECGX_REGSET_TABLE(Capture_Start),
	.normal_snapshot = S5K4ECGX_REGSET_TABLE(normal_snapshot),
	.camcorder = S5K4ECGX_REGSET_TABLE(camcorder),
	.camcorder_disable = S5K4ECGX_REGSET_TABLE(camcorder_disable),

	.preview_size = {
			S5K4ECGX_REGSET(S5K4ECGX_PREVIEW_144_176, 144_176_Preview),
			S5K4ECGX_REGSET(S5K4ECGX_PREVIEW_QCIF, 176_Preview),
			S5K4ECGX_REGSET(S5K4ECGX_PREVIEW_QVGA, QVGA_Preview),
			S5K4ECGX_REGSET(S5K4ECGX_PREVIEW_CIF, 352_Preview),
			S5K4ECGX_REGSET(S5K4ECGX_PREVIEW_VGA, 640_Preview),
			S5K4ECGX_REGSET(S5K4ECGX_PREVIEW_704_576, 704_576_Preview),
			S5K4ECGX_REGSET(S5K4ECGX_PREVIEW_D1, 720_Preview),
			S5K4ECGX_REGSET(S5K4ECGX_PREVIEW_720_540, 720_540_Preview),
			S5K4ECGX_REGSET(S5K4ECGX_PREVIEW_720_720, 720_720_Preview),
			S5K4ECGX_REGSET(S5K4ECGX_PREVIEW_WVGA, WVGA_Preview),
			S5K4ECGX_REGSET(S5K4ECGX_PREVIEW_960_540, 960_540_Preview),
			S5K4ECGX_REGSET(S5K4ECGX_PREVIEW_960_720, 960_720_Preview),
			S5K4ECGX_REGSET(S5K4ECGX_PREVIEW_960, 960_Preview),
			S5K4ECGX_REGSET(S5K4ECGX_PREVIEW_964_964, 964_964_Preview),
			S5K4ECGX_REGSET(S5K4ECGX_PREVIEW_1024, 1024_Preview),
			S5K4ECGX_REGSET(S5K4ECGX_PREVIEW_W1280, W1280_Preview),
			S5K4ECGX_REGSET(S5K4ECGX_PREVIEW_WXGA, 1280_WXGA_Preview),
			S5K4ECGX_REGSET(S5K4ECGX_PREVIEW_1288_772, 1288_772_Preview),
			S5K4ECGX_REGSET(S5K4ECGX_PREVIEW_1288_966, 1288_966_Preview),
			S5K4ECGX_REGSET(S5K4ECGX_PREVIEW_1280, 1280_Preview),
			S5K4ECGX_REGSET(S5K4ECGX_PREVIEW_1600, 1600_Preview),
			S5K4ECGX_REGSET(S5K4ECGX_PREVIEW_1920, 1920_Preview),
			S5K4ECGX_REGSET(S5K4ECGX_PREVIEW_2048, 2048_Preview),
			S5K4ECGX_REGSET(S5K4ECGX_PREVIEW_2560, max_Preview),
	},

	.capture_size = {
			S5K4ECGX_REGSET(S5K4ECGX_CAPTURE_VGA, VGA_Capture),
			S5K4ECGX_REGSET(S5K4ECGX_CAPTURE_XGA, XGA_Capture),
			S5K4ECGX_REGSET(S5K4ECGX_CAPTURE_WVGA, WVGA_Capture),
			S5K4ECGX_REGSET(S5K4ECGX_CAPTURE_1MP, 1M_Capture),
			S5K4ECGX_REGSET(S5K4ECGX_CAPTURE_W1MP, W1MP_Capture),
			S5K4ECGX_REGSET(S5K4ECGX_CAPTURE_2MP, 2M_Capture),
			S5K4ECGX_REGSET(S5K4ECGX_CAPTURE_1632_1218, 1632_1218_Capture),
			S5K4ECGX_REGSET(S5K4ECGX_CAPTURE_3MP, 3M_Capture),
			S5K4ECGX_REGSET(S5K4ECGX_CAPTURE_1920_1920, 1920_1920_Capture),
			S5K4ECGX_REGSET(S5K4ECGX_CAPTURE_1928_1928, 1928_1928_Capture),
			S5K4ECGX_REGSET(S5K4ECGX_CAPTURE_2036_1526, 2036_1526_Capture),
			S5K4ECGX_REGSET(S5K4ECGX_CAPTURE_2048_1152, 2048_1152_Capture),
			S5K4ECGX_REGSET(S5K4ECGX_CAPTURE_2048_1232, 2048_1232_Capture),
			S5K4ECGX_REGSET(S5K4ECGX_CAPTURE_W4MP, W4MP_Capture),
			S5K4ECGX_REGSET(S5K4ECGX_CAPTURE_2560_1440, 2560_1440_Capture),
			S5K4ECGX_REGSET(S5K4ECGX_CAPTURE_5MP, 5M_Capture),
			S5K4ECGX_REGSET(S5K4ECGX_CAPTURE_2576_1544, 2576_1544_Capture),
			S5K4ECGX_REGSET(S5K4ECGX_CAPTURE_2576_1932, 2576_1932_Capture),
	},

	.reset_crop = S5K4ECGX_REGSET_TABLE(Reset_Crop),
	.get_capture_status =
		S5K4ECGX_REGSET_TABLE(get_capture_status),
	.ev = {
		S5K4ECGX_REGSET(GET_CID_INDEX(EV_MINUS_4, EV_MIN_VALUE), EV_Minus_4),
		S5K4ECGX_REGSET(GET_CID_INDEX(EV_MINUS_3, EV_MIN_VALUE), EV_Minus_3),
		S5K4ECGX_REGSET(GET_CID_INDEX(EV_MINUS_2, EV_MIN_VALUE), EV_Minus_2),
		S5K4ECGX_REGSET(GET_CID_INDEX(EV_MINUS_1, EV_MIN_VALUE), EV_Minus_1),
		S5K4ECGX_REGSET(GET_CID_INDEX(EV_DEFAULT, EV_MIN_VALUE), EV_Default),
		S5K4ECGX_REGSET(GET_CID_INDEX(EV_PLUS_1, EV_MIN_VALUE), EV_Plus_1),
		S5K4ECGX_REGSET(GET_CID_INDEX(EV_PLUS_2, EV_MIN_VALUE), EV_Plus_2),
		S5K4ECGX_REGSET(GET_CID_INDEX(EV_PLUS_3, EV_MIN_VALUE), EV_Plus_3),
		S5K4ECGX_REGSET(GET_CID_INDEX(EV_PLUS_4, EV_MIN_VALUE), EV_Plus_4),
	},
	.metering = {
		S5K4ECGX_REGSET(METERING_MATRIX, Metering_Matrix),
		S5K4ECGX_REGSET(METERING_CENTER, Metering_Center),
		S5K4ECGX_REGSET(METERING_SPOT, Metering_Spot),
	},
	.iso = {
		S5K4ECGX_REGSET(ISO_AUTO, ISO_Auto),
		S5K4ECGX_REGSET(ISO_50, ISO_100),     /* map to 100 */
		S5K4ECGX_REGSET(ISO_100, ISO_100),
		S5K4ECGX_REGSET(ISO_200, ISO_200),
		S5K4ECGX_REGSET(ISO_400, ISO_400),
		S5K4ECGX_REGSET(ISO_800, ISO_400),    /* map to 400 */
		S5K4ECGX_REGSET(ISO_1600, ISO_400),   /* map to 400 */
		S5K4ECGX_REGSET(ISO_SPORTS, ISO_Auto),/* map to auto */
		S5K4ECGX_REGSET(ISO_NIGHT, ISO_Auto), /* map to auto */
		S5K4ECGX_REGSET(ISO_MOVIE, ISO_Auto), /* map to auto */
	},
	.effect = {
		S5K4ECGX_REGSET(IMAGE_EFFECT_NONE, Effect_Normal),
		S5K4ECGX_REGSET(IMAGE_EFFECT_BNW, Effect_Black_White),
		S5K4ECGX_REGSET(IMAGE_EFFECT_SEPIA, Effect_Sepia),
		S5K4ECGX_REGSET(IMAGE_EFFECT_NEGATIVE, Effect_Negative),
		S5K4ECGX_REGSET(IMAGE_EFFECT_SOLARIZE, Effect_Solarization),
		S5K4ECGX_REGSET(IMAGE_EFFECT_SOLARIZE, Effect_Aqua),
		S5K4ECGX_REGSET(IMAGE_EFFECT_SOLARIZE, Effect_Sketch),
	},
	.white_balance = {
		S5K4ECGX_REGSET(WHITE_BALANCE_AUTO, WB_Auto),
		S5K4ECGX_REGSET(WHITE_BALANCE_SUNNY, WB_Sunny),
		S5K4ECGX_REGSET(WHITE_BALANCE_CLOUDY, WB_Cloudy),
		S5K4ECGX_REGSET(WHITE_BALANCE_TUNGSTEN, WB_Tungsten),
		S5K4ECGX_REGSET(WHITE_BALANCE_FLUORESCENT, WB_Fluorescent),
	},
	.scene_mode = {
		S5K4ECGX_REGSET(SCENE_MODE_NONE, Scene_Default),
		S5K4ECGX_REGSET(SCENE_MODE_PORTRAIT, Scene_Portrait),
		S5K4ECGX_REGSET(SCENE_MODE_NIGHTSHOT, Scene_Nightshot),
		S5K4ECGX_REGSET(SCENE_MODE_BACK_LIGHT, Scene_Backlight),
		S5K4ECGX_REGSET(SCENE_MODE_LANDSCAPE, Scene_Landscape),
		S5K4ECGX_REGSET(SCENE_MODE_SPORTS, Scene_Sports),
		S5K4ECGX_REGSET(SCENE_MODE_PARTY_INDOOR, Scene_Party_Indoor),
		S5K4ECGX_REGSET(SCENE_MODE_BEACH_SNOW, Scene_Beach_Snow),
		S5K4ECGX_REGSET(SCENE_MODE_SUNSET, Scene_Sunset),
		S5K4ECGX_REGSET(SCENE_MODE_DUSK_DAWN, Scene_Duskdawn),
		S5K4ECGX_REGSET(SCENE_MODE_FALL_COLOR, Scene_Fall_Color),
		S5K4ECGX_REGSET(SCENE_MODE_FIREWORKS, Scene_Fireworks),
		S5K4ECGX_REGSET(SCENE_MODE_TEXT, Scene_Text),
		S5K4ECGX_REGSET(SCENE_MODE_CANDLE_LIGHT, Scene_Candle_Light),
	},
	.saturation = {
		S5K4ECGX_REGSET(SATURATION_MINUS_2, Saturation_Minus_2),
		S5K4ECGX_REGSET(SATURATION_MINUS_1, Saturation_Minus_1),
		S5K4ECGX_REGSET(SATURATION_DEFAULT, Saturation_Default),
		S5K4ECGX_REGSET(SATURATION_PLUS_1, Saturation_Plus_1),
		S5K4ECGX_REGSET(SATURATION_PLUS_2, Saturation_Plus_2),
	},
	.contrast = {
		S5K4ECGX_REGSET(CONTRAST_MINUS_2, Contrast_Minus_2),
		S5K4ECGX_REGSET(CONTRAST_MINUS_1, Contrast_Minus_1),
		S5K4ECGX_REGSET(CONTRAST_DEFAULT, Contrast_Default),
		S5K4ECGX_REGSET(CONTRAST_PLUS_1, Contrast_Plus_1),
		S5K4ECGX_REGSET(CONTRAST_PLUS_2, Contrast_Plus_2),
	},
	.sharpness = {
		S5K4ECGX_REGSET(SHARPNESS_MINUS_2, Sharpness_Minus_2),
		S5K4ECGX_REGSET(SHARPNESS_MINUS_1, Sharpness_Minus_1),
		S5K4ECGX_REGSET(SHARPNESS_DEFAULT, Sharpness_Default),
		S5K4ECGX_REGSET(SHARPNESS_PLUS_1, Sharpness_Plus_1),
		S5K4ECGX_REGSET(SHARPNESS_PLUS_2, Sharpness_Plus_2),
	},
	.anti_banding = {
		S5K4ECGX_REGSET(S5K4ECGX_FLICKER_50HZ_AUTO, 50hz_auto),
		S5K4ECGX_REGSET(S5K4ECGX_FLICKER_50HZ_FIXED, 50hz_fixed),
		S5K4ECGX_REGSET(S5K4ECGX_FLICKER_60HZ_AUTO, 60hz_auto),
		S5K4ECGX_REGSET(S5K4ECGX_FLICKER_60HZ_FIXED, 60hz_fixed),
	},
	.fps = {
		S5K4ECGX_REGSET(FRAME_RATE_AUTO, FPS_Auto),
		S5K4ECGX_REGSET(FRAME_RATE_7, FPS_7),
		S5K4ECGX_REGSET(FRAME_RATE_15, FPS_15),
		S5K4ECGX_REGSET(FRAME_RATE_30, FPS_30),
	},
	.preview_return = S5K4ECGX_REGSET_TABLE(Preview_Return),
	.jpeg_quality_high = S5K4ECGX_REGSET_TABLE(Jpeg_Quality_High),
	.jpeg_quality_normal = S5K4ECGX_REGSET_TABLE(Jpeg_Quality_Normal),
	.jpeg_quality_low = S5K4ECGX_REGSET_TABLE(Jpeg_Quality_Low),
	.flash_start = S5K4ECGX_REGSET_TABLE(Flash_Start),
	.flash_end = S5K4ECGX_REGSET_TABLE(Flash_End),
	.af_assist_flash_start = S5K4ECGX_REGSET_TABLE(Pre_Flash_Start),
	.af_assist_flash_end = S5K4ECGX_REGSET_TABLE(Pre_Flash_End),
	.af_low_light_mode_on = S5K4ECGX_REGSET_TABLE(AF_Low_Light_Mode_On),
	.af_low_light_mode_off = S5K4ECGX_REGSET_TABLE(AF_Low_Light_Mode_Off),
	.aeawb_lockunlock = {
		S5K4ECGX_REGSET(AE_UNLOCK_AWB_UNLOCK, AE_AWB_Lock_Off),
		S5K4ECGX_REGSET(AE_LOCK_AWB_UNLOCK, AE_Lock_On_AWB_Lock_Off),
		S5K4ECGX_REGSET(AE_UNLOCK_AWB_LOCK, AE_Lock_Off_AWB_Lock_On),
		S5K4ECGX_REGSET(AE_LOCK_AWB_LOCK, AE_AWB_Lock_On),
	},
	.ae_lockunlock = {
		S5K4ECGX_REGSET(AE_UNLOCK, AE_Lock_Off),
		S5K4ECGX_REGSET(AE_LOCK, AE_Lock_On),
	},
	.awb_lockunlock = {
		S5K4ECGX_REGSET(AWB_UNLOCK, AWB_Lock_Off),
		S5K4ECGX_REGSET(AWB_LOCK, AWB_Lock_On),
	},
	.wdr_on = S5K4ECGX_REGSET_TABLE(WDR_on),
	.wdr_off = S5K4ECGX_REGSET_TABLE(WDR_off),
	.face_detection_on = S5K4ECGX_REGSET_TABLE(Face_Detection_On),
	.face_detection_off = S5K4ECGX_REGSET_TABLE(Face_Detection_Off),
	.af_macro_mode_1 = S5K4ECGX_REGSET_TABLE(AF_Macro_mode_1),
	.af_macro_mode_2 = S5K4ECGX_REGSET_TABLE(AF_Macro_mode_2),
	.af_macro_mode_3 = S5K4ECGX_REGSET_TABLE(AF_Macro_mode_3),
	.af_normal_mode_1 = S5K4ECGX_REGSET_TABLE(AF_Normal_mode_1),
	.af_normal_mode_2 = S5K4ECGX_REGSET_TABLE(AF_Normal_mode_2),
	.af_normal_mode_3 = S5K4ECGX_REGSET_TABLE(AF_Normal_mode_3),
	.af_return_macro_position =
		S5K4ECGX_REGSET_TABLE(AF_Return_Macro_pos),
	.single_af_start = S5K4ECGX_REGSET_TABLE(Single_AF_Start),
	.single_af_off_1 = S5K4ECGX_REGSET_TABLE(Single_AF_Off_1),
	.single_af_off_2 = S5K4ECGX_REGSET_TABLE(Single_AF_Off_2),
	.continuous_af_on = S5K4ECGX_REGSET_TABLE(Continuous_AF_On),
	.continuous_af_off = S5K4ECGX_REGSET_TABLE(Continuous_AF_Off),
	.dtp_start = S5K4ECGX_REGSET_TABLE(DTP_init),
	.dtp_stop = S5K4ECGX_REGSET_TABLE(DTP_stop),

	.flash_init = S5K4ECGX_REGSET_TABLE(Flash_init),
	.reset_crop = S5K4ECGX_REGSET_TABLE(Reset_Crop),
	.fast_ae_on =  S5K4ECGX_REGSET_TABLE(Fast_Ae_On),
	.fast_ae_off =  S5K4ECGX_REGSET_TABLE(Fast_Ae_Off),
	.get_ae_stable_status =
		S5K4ECGX_REGSET_TABLE(Get_AE_Stable_Status),
	.get_light_level = S5K4ECGX_REGSET_TABLE(Get_Light_Level),
	.get_frame_duration = S5K4ECGX_REGSET_TABLE(get_frame_duration_reg),
	.get_1st_af_search_status =
		S5K4ECGX_REGSET_TABLE(get_1st_af_search_status),
	.get_2nd_af_search_status =
		S5K4ECGX_REGSET_TABLE(get_2nd_af_search_status),
	.get_capture_status =
		S5K4ECGX_REGSET_TABLE(get_capture_status),
	.get_esd_status = S5K4ECGX_REGSET_TABLE(get_esd_status),
	.get_iso = S5K4ECGX_REGSET_TABLE(get_iso_reg),
	.get_shutterspeed =
		S5K4ECGX_REGSET_TABLE(get_shutterspeed_reg),
	.get_exptime =
		S5K4ECGX_REGSET_TABLE(get_exptime_reg),
	.get_frame_count =
		S5K4ECGX_REGSET_TABLE(get_frame_count_reg),
	.get_preview_status =
		S5K4ECGX_REGSET_TABLE(get_preview_status_reg),
	.get_pid =
		S5K4ECGX_REGSET_TABLE(get_pid_reg),
	.get_revision =
		S5K4ECGX_REGSET_TABLE(get_revision_reg),
	.get_modechange_check =
		S5K4ECGX_REGSET_TABLE(get_modechange_check_reg),
	.set_vendor_id_read =
		S5K4ECGX_REGSET_TABLE(set_vendor_id_read_reg),
	.get_vendor_id_read =
		S5K4ECGX_REGSET_TABLE(get_vendor_id_read_reg),
};

struct s5k4ecgx_regset {
	u32 size;
	u8 *data;
};

static struct fimc_is_sensor_cfg settle_4ec[] = {
	/* 3264x2448@30fps */
	FIMC_IS_SENSOR_CFG(3264, 2448, 30, 7, 0),
};

static struct fimc_is_vci vci_4ec[] = {
#if 0
	{
		.pixelformat = V4L2_PIX_FMT_YUYV,
		.vc_map = {2, 1, 0, 3}
	}, {
		.pixelformat = V4L2_PIX_FMT_JPEG,
		.vc_map = {0, 2, 1, 3}
	}
#else
	{
		.pixelformat = V4L2_PIX_FMT_YUYV,
		.config = {{0, HW_FORMAT_YUV422_8BIT}, {1, HW_FORMAT_UNKNOWN}, {2, HW_FORMAT_USER}, {3, 0}}
	}
#endif
};

static inline struct fimc_is_module_enum *to_module(struct v4l2_subdev *subdev)
{
	struct fimc_is_module_enum *module;
	module = (struct fimc_is_module_enum *)v4l2_get_subdevdata(subdev);
	return module;
}

static inline struct fimc_is_module_4ec *to_state(struct v4l2_subdev *subdev)
{
	struct fimc_is_module_enum *module;
	module = (struct fimc_is_module_enum *)v4l2_get_subdevdata(subdev);
	return (struct fimc_is_module_4ec *)module->private_data;
}

static inline struct i2c_client *to_client(struct v4l2_subdev *subdev)
{
	struct fimc_is_module_enum *module;
	module = (struct fimc_is_module_enum *)v4l2_get_subdevdata(subdev);
	return (struct i2c_client *)module->client;
}

#ifdef CONFIG_LOAD_FILE
static char *sensor_4ec_regs_table;
static int sensor_4ec_regs_table_size;

int sensor_4ec_regs_table_init(void)
{
	struct file *filp;
	char *dp;
	long size;
	loff_t pos;
	int ret;
	mm_segment_t fs = get_fs();

	cam_info("%s : E\n", __func__);

	set_fs(get_ds());

	filp = filp_open(LOAD_FILE_PATH, O_RDONLY, 0);

	if (IS_ERR_OR_NULL(filp)) {
		cam_err("file open error\n");
		return PTR_ERR(filp);
	}

	size = filp->f_path.dentry->d_inode->i_size;
	cam_dbg("size = %ld\n", size);
	//dp = kmalloc(size, GFP_KERNEL);
	dp = vmalloc(size);
	if (unlikely(!dp)) {
		cam_err("Out of Memory\n");
		filp_close(filp, current->files);
		return -ENOMEM;
	}

	pos = 0;
	memset(dp, 0, size);
	ret = vfs_read(filp, (char __user *)dp, size, &pos);

	if (unlikely(ret != size)) {
		cam_err("Failed to read file ret = %d\n", ret);
		/*kfree(dp);*/
		vfree(dp);
		filp_close(filp, current->files);
		return -EINVAL;
	}

	filp_close(filp, current->files);

	set_fs(fs);

	sensor_4ec_regs_table = dp;

	sensor_4ec_regs_table_size = size;

	*((sensor_4ec_regs_table + sensor_4ec_regs_table_size) - 1) = '\0';

	cam_info("%s : X\n", __func__);
	return 0;
}

void sensor_4ec_regs_table_exit(void)
{
	printk(KERN_DEBUG "%s %d\n", __func__, __LINE__);

	if (sensor_4ec_regs_table) {
		vfree(sensor_4ec_regs_table);
		sensor_4ec_regs_table = NULL;
	}
}

static int sensor_4ec_write_regs_from_sd(struct v4l2_subdev *sd, const char *name)
{
	char *start = NULL, *end = NULL, *reg = NULL, *temp_start = NULL;
	u16 addr = 0, value = 0;
	u16 data = 0;
	char data_buf[7] = {0, };
	u32 len = 0;
	int err = 0;
	int i;
	int cnt = 0;
	struct i2c_client *client = to_client(sd);

	cam_info("%s : E, sensor_4ec_regs_table_size - %d\n", __func__, sensor_4ec_regs_table_size);

	addr = value = 0;

	*(data_buf + 6) = '\0';

	start = strnstr(sensor_4ec_regs_table, name, sensor_4ec_regs_table_size);

	do {
		if (*(start + strlen(name)) == '[') {
			cam_info("break to search (%s) \n", name);
			break;
		} else {
			cam_info("searching (%s) \n", name);
			start = start + strlen(name);
		}
		start = strnstr(start, name, strlen(start));
	} while(start != NULL);

	if (unlikely(start == NULL)) {
		cam_err("start is NULL\n");
		return -ENODATA;
	}

	end = strnstr(start, "};", sensor_4ec_regs_table_size);
	if (unlikely(end == NULL)) {
		cam_err("end is NULL\n");
		return -ENODATA;
	}

	while (1) {
		if (cnt < 1) {
			len = end - start;
			temp_start = strnstr(start, "{", len);
			if (!temp_start || (temp_start > end)) {
				cam_info("write end of %s\n", name);
				break;
			}
			start = temp_start;
		}

		len = end - start;
		/* Find Address */
		reg = strnstr(start, "0x", len);
		if (!reg || (reg > end)) {
			cam_info("write end of %s\n", name);
			break;
		}

		start = (reg + 6);

		/* Write Value to Address */
		memcpy(data_buf, reg, 6);

		err = kstrtou16(data_buf, 16, &data);
		if (unlikely(err < 0)) {
			cam_err("kstrtou16 failed\n");
			return err;
		}

		addr = data;
		len = end - start;

		/* Write Value to Address */
		memset(data_buf, 0, sizeof(char) * 7);
		memcpy(data_buf, "0x", 2);
		for (i = 0; i < 4; i++) {
			data_buf[i+2] = *(reg + 6 + i);
		}

		err = kstrtou16(data_buf, 16, &data);
		if (unlikely(err < 0)) {
			cam_err("kstrtou16 failed\n");
			return err;
		}

		value = data;

		if(addr == 0xFFFF) {
			cam_info("%s : use delay (%d ms) in I2C Write \n", __func__, value);
			msleep(value);
		} else {
			err = fimc_is_sensor_write16(client, addr, value);
			cam_info("(%s)addr(0x%x)value(0x%x)\n", name, addr, value);
			if (unlikely(err < 0)) {
				cam_err("register set failed\n");
				return err;
			}
		}
		cnt++;
	}

	cam_info("%s : X\n", __func__);

	return err;
}
#endif

static int sensor_4ec_apply_set(struct v4l2_subdev *subdev,
	const struct s5k4ecgx_regset_table *regset)
{
	int ret = 0;
#ifndef CONFIG_LOAD_FILE
	u16 addr, val;
	u32 i;
#endif
	struct i2c_client *client = to_client(subdev);
	struct fimc_is_module_enum *module = to_module(subdev);
	struct fimc_is_module_4ec *module_4ec = to_state(subdev);
	u32 set_idx = module_4ec->setfile_index;

	BUG_ON(!client);
	BUG_ON(!regset);
	BUG_ON(!module);
	BUG_ON(!module_4ec);

	mutex_lock(&module_4ec->i2c_lock);

	cam_info("%s : E, setting_name : %s\n", __func__, regset->setting_name[set_idx]);

#ifdef CONFIG_LOAD_FILE
	cam_info("COFIG_LOAD_FILE feature is enabled\n");
	ret = sensor_4ec_write_regs_from_sd(subdev, regset->setting_name[set_idx]);
	if (unlikely(ret < 0)) {
		err("regs set(%s)apply is fail(%d)\n", regset->setting_name[set_idx], ret);
		goto p_err;
	}
#else
	for (i = 0; i < regset->array_size[set_idx]; i++) {
		addr = (regset->reg[set_idx][i] & 0xFFFF0000) >> 16;
		val = regset->reg[set_idx][i] & 0x0000FFFF;
		if(addr == 0xFFFF) {
			cam_info("%s : use delay (%d ms) in I2C Write \n", __func__, val);
			msleep(val);
		} else {
			ret = fimc_is_sensor_write16(client, addr, val);
			if (unlikely(ret < 0)) {
				err("regs set(addr 0x%08X, size %d) apply is fail(%d)",
						(u32)regset->reg[set_idx], regset->array_size[set_idx], ret);
				goto p_err;
			}
		}
	}
#endif

p_err:
	mutex_unlock(&module_4ec->i2c_lock);
	return ret;
}

static int sensor_4ec_read_reg(struct v4l2_subdev *subdev,
	const struct s5k4ecgx_regset_table *regset, u16 *data, u32 count)
{
	int ret = 0;
	u16 addr, val;
	u32 i;
	struct i2c_client *client = to_client(subdev);
	struct fimc_is_module_enum *module = to_module(subdev);
	struct fimc_is_module_4ec *module_4ec = to_state(subdev);
	u32 set_idx = module_4ec->setfile_index;

	BUG_ON(!client);
	BUG_ON(!module);
	BUG_ON(!module_4ec);

	mutex_lock(&module_4ec->i2c_lock);

	/* Enter read mode */
	fimc_is_sensor_write16(client, 0x002C, 0x7000);

	for (i = 0; i < regset->array_size[set_idx]; i++) {
		addr = (regset->reg[set_idx][i] & 0xFFFF0000) >> 16;
		val = regset->reg[set_idx][i] & 0x0000FFFF;
		ret = fimc_is_sensor_write16(client, addr, val);
		if (unlikely(ret < 0)) {
			err("regs set(addr 0x%08X, size %d) apply is fail(%d)",
				(u32)regset->reg[set_idx], regset->array_size[set_idx], ret);
			goto p_err;
		}
	}

	for (i = 0; i < count; i++, data++) {
		ret = fimc_is_sensor_read16(client, 0x0F12, data);
		if (unlikely(ret < 0)) {
			err("read reg fail(%d)", ret);
			goto p_err;
		}
	}

p_err:
	/* restore write mode */
	fimc_is_sensor_write16(client, 0x0028, 0x7000);
	mutex_unlock(&module_4ec->i2c_lock);
	return ret;
}

static int sensor_4ec_read_addr(struct v4l2_subdev *subdev,
	u16 addr, u16 *data, u32 count)
{
	int ret = 0;
	u32 i;
	struct i2c_client *client = to_client(subdev);
	struct fimc_is_module_enum *module = to_module(subdev);
	struct fimc_is_module_4ec *module_4ec = to_state(subdev);

	BUG_ON(!client);
	BUG_ON(!module);
	BUG_ON(!module_4ec);

	mutex_lock(&module_4ec->i2c_lock);

	/* Enter read mode */
	fimc_is_sensor_write16(client, 0x002C, 0x7000);
	fimc_is_sensor_write16(client, 0x002e, addr);

	for (i = 0; i < count; i++, data++) {
		ret = fimc_is_sensor_read16(client, 0x0F12, data);
		if (unlikely(ret < 0)) {
			err("read reg fail(%d)", ret);
			goto p_err;
		}
	}

p_err:
	/* restore write mode */
	fimc_is_sensor_write16(client, 0x0028, 0x7000);
	mutex_unlock(&module_4ec->i2c_lock);
	return ret;
}

static int sensor_4ec_s_flash(struct v4l2_subdev *subdev, int value)
{
	int ret = 0;
	struct fimc_is_module_enum *module;
	struct fimc_is_module_4ec *module_4ec;

	BUG_ON(!subdev);

	cam_info("%s: %d\n", __func__, value);

	module = (struct fimc_is_module_enum *)v4l2_get_subdevdata(subdev);
	module_4ec = module->private_data;

	if (module_4ec->flash_mode == value) {
		cam_warn("already flash mode(%d) is set \n", value);
		goto p_err;
	}

	if ((value >= FLASH_MODE_OFF) && (value <= FLASH_MODE_TORCH)) {
		module_4ec->flash_mode = value;
		if (module_4ec->flash_mode == FLASH_MODE_TORCH) {
			ret = sensor_4ec_apply_set(subdev, &regs_set.flash_start);
#if defined(CONFIG_FLED_SM5703)
			sm5703_led_mode_ctrl(SM5703_FLED_MODE_MOVIE);
			if (flash_control_ready == false) {
				sm5703_led_mode_ctrl(SM5703_FLED_MODE_EXT_GPIO_ENABLE);
				flash_control_ready = true;
			}
#elif defined(CONFIG_LEDS_S2MU005)
			s2mu005_led_mode_ctrl(S2MU005_FLED_MODE_MOVIE);
#endif
		} else {
			ret = sensor_4ec_apply_set(subdev, &regs_set.flash_end);
#if defined(CONFIG_FLED_SM5703)
			sm5703_led_mode_ctrl(SM5703_FLED_MODE_OFF);
#elif defined(CONFIG_LEDS_S2MU005)
			s2mu005_led_mode_ctrl(S2MU005_FLED_MODE_OFF);
#endif
		}
	}

p_err:
	return ret;
}

static int sensor_4ec_s_white_balance(struct v4l2_subdev *subdev, int wb)
{
	int ret = 0;
	struct fimc_is_module_enum *module;
	struct fimc_is_module_4ec *module_4ec;

	module = (struct fimc_is_module_enum *)v4l2_get_subdevdata(subdev);
	if (unlikely(!module)) {
		err("module is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	module_4ec = module->private_data;
	if (unlikely(!module_4ec)) {
		err("module_4ec is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	cam_info("%s( %d ) : E\n",__func__, wb);

	switch (wb) {
	case WHITE_BALANCE_BASE :
	case WHITE_BALANCE_AUTO :
		module_4ec->white_balance = WHITE_BALANCE_AUTO;
		break;
	case WHITE_BALANCE_SUNNY :
		module_4ec->white_balance = WHITE_BALANCE_SUNNY;
		break;
	case WHITE_BALANCE_CLOUDY :
		module_4ec->white_balance = WHITE_BALANCE_CLOUDY;
		break;
	case WHITE_BALANCE_TUNGSTEN :
		module_4ec->white_balance = WHITE_BALANCE_TUNGSTEN;
		break;
	case WHITE_BALANCE_FLUORESCENT :
		module_4ec->white_balance = WHITE_BALANCE_FLUORESCENT;
		break;
	default:
		err("%s: Not support.(%d)\n", __func__, wb);
		ret = -EINVAL;
		goto p_err;
		break;
	}

	ret = sensor_4ec_apply_set(subdev, &regs_set.white_balance[module_4ec->white_balance]);
	if (ret) {
		err("%s: write cmd failed\n", __func__);
		goto p_err;
	}

p_err:

	return ret;
}

static int sensor_4ec_s_effect(struct v4l2_subdev *subdev, int effect)
{
	int ret = 0;
	struct fimc_is_module_enum *module;
	struct fimc_is_module_4ec *module_4ec;

	module = (struct fimc_is_module_enum *)v4l2_get_subdevdata(subdev);
	if (unlikely(!module)) {
		err("module is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	module_4ec = module->private_data;
	if (unlikely(!module_4ec)) {
		err("module_4ec is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	cam_info("%s( %d ) : E\n",__func__, effect);

	switch (effect) {
	case IMAGE_EFFECT_BASE :
	case IMAGE_EFFECT_NONE :
		module_4ec->effect = IMAGE_EFFECT_NONE;
		break;
	case IMAGE_EFFECT_BNW :
		module_4ec->effect = IMAGE_EFFECT_BNW;
		break;
	case IMAGE_EFFECT_SEPIA :
		module_4ec->effect = IMAGE_EFFECT_SEPIA;
		break;
	case IMAGE_EFFECT_NEGATIVE :
		module_4ec->effect = IMAGE_EFFECT_NEGATIVE;
		break;
	default:
		err("%s: Not support.(%d)\n", __func__, effect);
		ret = -EINVAL;
		goto p_err;
		break;
	}

	ret = sensor_4ec_apply_set(subdev, &regs_set.effect[module_4ec->effect]);
	if (ret) {
		err("%s: write cmd failed\n", __func__);
		goto p_err;
	}

p_err:

	return ret;
}

static int sensor_4ec_s_iso(struct v4l2_subdev *subdev, int iso)
{
	int ret = 0;
	struct fimc_is_module_enum *module;
	struct fimc_is_module_4ec *module_4ec;

	module = (struct fimc_is_module_enum *)v4l2_get_subdevdata(subdev);
	if (unlikely(!module)) {
		err("module is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	module_4ec = module->private_data;
	if (unlikely(!module_4ec)) {
		err("module_4ec is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	cam_info("%s( %d ) : E\n",__func__, iso);

	switch (iso) {
	case ISO_AUTO :
		module_4ec->iso = ISO_AUTO;
		break;
	case ISO_50 :
		module_4ec->iso = ISO_50;
		break;
	case ISO_100 :
		module_4ec->iso = ISO_100;
		break;
	case ISO_200 :
		module_4ec->iso = ISO_200;
		break;
	case ISO_400 :
		module_4ec->iso = ISO_400;
		break;
	default:
		err("%s: Not support.(%d)\n", __func__, iso);
		ret = -EINVAL;
		goto p_err;
		break;
	}

	ret = sensor_4ec_apply_set(subdev, &regs_set.iso[module_4ec->iso]);
	if (ret) {
		err("%s: write cmd failed\n", __func__);
		goto p_err;
	}

p_err:

	return ret;
}

static int sensor_4ec_s_contrast(struct v4l2_subdev *subdev, int contrast)
{
	int ret = 0;
	struct fimc_is_module_enum *module;
	struct fimc_is_module_4ec *module_4ec;

	module = (struct fimc_is_module_enum *)v4l2_get_subdevdata(subdev);
	if (unlikely(!module)) {
		err("module is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	module_4ec = module->private_data;
	if (unlikely(!module_4ec)) {
		err("module_4ec is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	cam_info("%s( %d ) : E\n",__func__, contrast);

	if (contrast < CONTRAST_MINUS_2)
		contrast = CONTRAST_MINUS_2;
	else if (contrast > CONTRAST_PLUS_2)
		contrast = CONTRAST_PLUS_2;

	module_4ec->contrast = GET_CID_INDEX(contrast, CONTRAST_MIN_VALUE);

	ret = sensor_4ec_apply_set(subdev, &regs_set.contrast[module_4ec->contrast]);
	if (ret) {
		err("%s: write cmd failed\n", __func__);
		goto p_err;
	}

p_err:

	return ret;
}

static int sensor_4ec_s_saturation(struct v4l2_subdev *subdev, int saturation)
{
	int ret = 0;
	struct fimc_is_module_enum *module;
	struct fimc_is_module_4ec *module_4ec;

	module = (struct fimc_is_module_enum *)v4l2_get_subdevdata(subdev);
	if (unlikely(!module)) {
		err("module is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	module_4ec = module->private_data;
	if (unlikely(!module_4ec)) {
		err("module_4ec is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	cam_info("%s( %d ) : E\n",__func__, saturation);

	if (saturation < SATURATION_MINUS_2)
		saturation = SATURATION_MINUS_2;
	else if (saturation > SATURATION_PLUS_2)
		saturation = SATURATION_PLUS_2;

	module_4ec->saturation = GET_CID_INDEX(saturation, SATURATION_MIN_VALUE);

	ret = sensor_4ec_apply_set(subdev, &regs_set.saturation[module_4ec->saturation]);
	if (ret) {
		err("%s: write cmd failed\n", __func__);
		goto p_err;
	}

p_err:

	return ret;
}

static int sensor_4ec_s_sharpness(struct v4l2_subdev *subdev, int sharpness)
{
	int ret = 0;
	struct fimc_is_module_enum *module;
	struct fimc_is_module_4ec *module_4ec;

	module = (struct fimc_is_module_enum *)v4l2_get_subdevdata(subdev);
	if (unlikely(!module)) {
		err("module is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	module_4ec = module->private_data;
	if (unlikely(!module_4ec)) {
		err("module_4ec is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	cam_info("%s( %d ) : E\n",__func__, sharpness);

	if (sharpness < SHARPNESS_MINUS_2)
		sharpness = SHARPNESS_MINUS_2;
	else if (sharpness > SHARPNESS_PLUS_2)
		sharpness = SHARPNESS_PLUS_2;

	module_4ec->sharpness = GET_CID_INDEX(sharpness, SHARPNESS_MIN_VALUE);

	ret = sensor_4ec_apply_set(subdev, &regs_set.sharpness[module_4ec->sharpness]);
	if (ret) {
		err("%s: write cmd failed\n", __func__);
		goto p_err;
	}

p_err:

	return ret;
}

static int sensor_4ec_s_brightness(struct v4l2_subdev *subdev, int brightness)
{
	int ret = 0;
	struct fimc_is_module_enum *module;
	struct fimc_is_module_4ec *module_4ec;

	module = (struct fimc_is_module_enum *)v4l2_get_subdevdata(subdev);
	if (unlikely(!module)) {
		err("module is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	module_4ec = module->private_data;
	if (unlikely(!module_4ec)) {
		err("module_4ec is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	cam_info("%s( %d ) : E\n",__func__, brightness);

	if (brightness < EV_MINUS_4)
		brightness = EV_MINUS_4;
	else if (brightness > EV_PLUS_4)
		brightness = EV_PLUS_4;

	module_4ec->ev = GET_CID_INDEX(brightness, EV_MIN_VALUE);

	ret = sensor_4ec_apply_set(subdev, &regs_set.ev[module_4ec->ev]);
	if (ret) {
		err("%s: write cmd failed\n", __func__);
		goto p_err;
	}

p_err:

	return ret;
}

static int sensor_4ec_s_metering(struct v4l2_subdev *subdev, int metering)
{
	int ret = 0;
	struct fimc_is_module_enum *module;
	struct fimc_is_module_4ec *module_4ec;

	module = (struct fimc_is_module_enum *)v4l2_get_subdevdata(subdev);
	if (unlikely(!module)) {
		err("module is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	module_4ec = module->private_data;
	if (unlikely(!module_4ec)) {
		err("module_4ec is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	cam_info("%s( %d ) : E\n",__func__, metering);

	switch (metering) {
	case METERING_MATRIX :
	case METERING_CENTER :
	case METERING_SPOT :
		module_4ec->metering = metering;
		break;
	default:
		err("%s: Not support.(%d)\n", __func__, metering);
		ret = -EINVAL;
		goto p_err;
		break;
	}

	ret = sensor_4ec_apply_set(subdev, &regs_set.metering[module_4ec->metering]);
	if (ret) {
		err("%s: write cmd failed\n", __func__);
		goto p_err;
	}

p_err:

	return ret;
}

static int sensor_4ec_s_scene_mode(struct v4l2_subdev *subdev, int scene_mode)
{
	int ret = 0;
	struct fimc_is_module_enum *module;
	struct fimc_is_module_4ec *module_4ec;

	module = (struct fimc_is_module_enum *)v4l2_get_subdevdata(subdev);
	if (unlikely(!module)) {
		err("module is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	module_4ec = module->private_data;
	if (unlikely(!module_4ec)) {
		err("module_4ec is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	cam_info("%s( %d ) : E\n",__func__, scene_mode);

	switch (scene_mode) {
	case SCENE_MODE_BASE :
	case SCENE_MODE_NONE :
		module_4ec->scene_mode = SCENE_MODE_NONE;
		break;
	case SCENE_MODE_PORTRAIT :
		module_4ec->scene_mode = SCENE_MODE_PORTRAIT;
		break;
	case SCENE_MODE_NIGHTSHOT :
		module_4ec->scene_mode = SCENE_MODE_NIGHTSHOT;
		break;
	case SCENE_MODE_BACK_LIGHT :
		module_4ec->scene_mode = SCENE_MODE_BACK_LIGHT;
		break;
	case SCENE_MODE_LANDSCAPE :
		module_4ec->scene_mode = SCENE_MODE_LANDSCAPE;
		break;
	case SCENE_MODE_SPORTS :
		module_4ec->scene_mode = SCENE_MODE_SPORTS;
		break;
	case SCENE_MODE_PARTY_INDOOR :
		module_4ec->scene_mode = SCENE_MODE_PARTY_INDOOR;
		break;
	case SCENE_MODE_BEACH_SNOW :
		module_4ec->scene_mode = SCENE_MODE_BEACH_SNOW;
		break;
	case SCENE_MODE_SUNSET :
		module_4ec->scene_mode = SCENE_MODE_SUNSET;
		break;
	case SCENE_MODE_DUSK_DAWN :
		module_4ec->scene_mode = SCENE_MODE_DUSK_DAWN;
		break;
	case SCENE_MODE_FALL_COLOR :
		module_4ec->scene_mode = SCENE_MODE_FALL_COLOR;
		break;
	case SCENE_MODE_FIREWORKS :
		module_4ec->scene_mode = SCENE_MODE_FIREWORKS;
		break;
	case SCENE_MODE_TEXT :
		module_4ec->scene_mode = SCENE_MODE_TEXT;
		break;
	case SCENE_MODE_CANDLE_LIGHT :
		module_4ec->scene_mode = SCENE_MODE_CANDLE_LIGHT;
		break;
	default:
		err("%s: Not support.(%d)\n", __func__, scene_mode);
		ret = -EINVAL;
		goto p_err;
		break;
	}

	ret = sensor_4ec_apply_set(subdev, &regs_set.scene_mode[module_4ec->scene_mode]);
	if (ret) {
		err("%s: write cmd failed\n", __func__);
		goto p_err;
	}

p_err:

	return ret;
}

#ifndef USE_ANTIBANDING_50HZ_ONLY
static int sensor_4ec_s_anti_banding(struct v4l2_subdev *subdev, int anti_banding)
{
	int ret = 0;
	struct fimc_is_module_enum *module;
	struct fimc_is_module_4ec *module_4ec;

	module = (struct fimc_is_module_enum *)v4l2_get_subdevdata(subdev);
	if (unlikely(!module)) {
		err("module is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	module_4ec = module->private_data;
	if (unlikely(!module_4ec)) {
		err("module_4ec is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	cam_info("%s( %d ) : E\n",__func__, anti_banding);

	switch (anti_banding) {
	case ANTI_BANDING_AUTO :
	case ANTI_BANDING_OFF :
	case ANTI_BANDING_50HZ :
		module_4ec->anti_banding = S5K4ECGX_FLICKER_50HZ_AUTO;
		break;
	case ANTI_BANDING_60HZ :
		module_4ec->anti_banding = S5K4ECGX_FLICKER_60HZ_AUTO;
		break;
	default:
		err("%s: Not support.(%d)\n", __func__, anti_banding);
		ret = -EINVAL;
		goto p_err;
		break;
	}

	ret = sensor_4ec_apply_set(subdev, &regs_set.anti_banding[module_4ec->anti_banding]);
	if (ret) {
		err("%s: write cmd failed\n", __func__);
		goto p_err;
	}

p_err:

	return ret;
}
#endif

static int sensor_4ec_s_frame_rate(struct v4l2_subdev *subdev, int frame_rate)
{
	int ret = 0;
	struct fimc_is_module_enum *module;
	struct fimc_is_module_4ec *module_4ec;

	module = (struct fimc_is_module_enum *)v4l2_get_subdevdata(subdev);
	if (unlikely(!module)) {
		err("module is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	module_4ec = module->private_data;
	if (unlikely(!module_4ec)) {
		err("module_4ec is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	cam_info("%s( %d ) : E\n",__func__, frame_rate);

	switch (frame_rate) {
	case FRAME_RATE_AUTO:
	case FRAME_RATE_7:
	case FRAME_RATE_15:
	case FRAME_RATE_30:
		module_4ec->fps = frame_rate;
		break;
	default:
		err("%s: Not support.(%d)\n", __func__, frame_rate);
		ret = -EINVAL;
		goto p_err;
		break;
	}

	ret = sensor_4ec_apply_set(subdev, &regs_set.fps[frame_rate]);
	if (ret) {
		err("%s: write cmd failed\n", __func__);
		goto p_err;
	}

p_err:

	return ret;
}

static bool sensor_4ec_check_focus_mode(struct v4l2_subdev *subdev, s32 focus_mode)
{
	bool cancelAF = false;
	struct fimc_is_module_enum *module;
	struct fimc_is_module_4ec *module_4ec;

	BUG_ON(!subdev);

	module = (struct fimc_is_module_enum *)v4l2_get_subdevdata(subdev);
	module_4ec = module->private_data;

	cancelAF = (focus_mode & V4L2_FOCUS_MODE_DEFAULT);
	focus_mode = (focus_mode & ~V4L2_FOCUS_MODE_DEFAULT);

	cam_info ("(%s)cancelAF(%d) focus_mode(%d) \n", __func__, cancelAF, focus_mode);

	if ((module_4ec->focus_mode != focus_mode) || cancelAF) {
		return true;
	} else {
		return false;
	}
}

static int sensor_4ec_s_focus_mode(struct v4l2_subdev *subdev, s32 focus_mode)
{
	int ret = 0;
	struct fimc_is_module_enum *module;
	struct fimc_is_module_4ec *module_4ec;

	BUG_ON(!subdev);

	module = (struct fimc_is_module_enum *)v4l2_get_subdevdata(subdev);
	module_4ec = module->private_data;

	cam_info("%s( %d ) E\n",__func__, focus_mode);

	mutex_lock(&module_4ec->af_lock);

	if (focus_mode == V4L2_FOCUS_MODE_AUTO) {
		ret = sensor_4ec_apply_set(subdev, &regs_set.af_normal_mode_1);
		msleep(100);
		ret |= sensor_4ec_apply_set(subdev, &regs_set.af_normal_mode_2);
		msleep(100);
		ret |= sensor_4ec_apply_set(subdev, &regs_set.af_normal_mode_3);
		msleep(100);
	} else if (focus_mode == V4L2_FOCUS_MODE_MACRO) {
		ret = sensor_4ec_apply_set(subdev, &regs_set.af_macro_mode_1);
		msleep(100);
		ret |= sensor_4ec_apply_set(subdev, &regs_set.af_macro_mode_2);
		msleep(100);
		ret |= sensor_4ec_apply_set(subdev, &regs_set.af_macro_mode_3);
		msleep(100);
	} else {
		cam_warn("%s: Not support.(%d)\n", __func__, focus_mode);
		ret = -EINVAL;
		goto p_err;
	}

p_err :
	mutex_unlock(&module_4ec->af_lock);

	return ret;
}

static int sensor_4ec_ae_lock(struct v4l2_subdev *subdev, int ctrl)
{
	int ret = 0;
	struct fimc_is_module_enum *module;
	struct fimc_is_module_4ec *module_4ec;

	module = (struct fimc_is_module_enum *)v4l2_get_subdevdata(subdev);
	if (unlikely(!module)) {
		err("module is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	module_4ec = module->private_data;
	if (unlikely(!module_4ec)) {
		err("module_4ec is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	cam_info("%s( %d ) : E\n",__func__, ctrl);

	if (module_4ec->ae_lock == ctrl) {
		cam_info("%s: same ae_lock mode( %d ). skip to set ae_lock mode.\n", __FUNCTION__, ctrl);
		return 0;
	}

	switch (ctrl) {
	case AE_UNLOCK :
	case AE_LOCK :
		module_4ec->ae_lock = ctrl;
		break;
	default:
		err("%s: Not support.(%d)\n", __func__, ctrl);
		ret = -EINVAL;
		goto p_err;
		break;
	}

	ret = sensor_4ec_apply_set(subdev, &regs_set.ae_lockunlock[module_4ec->ae_lock]);
	if (ret) {
		err("%s: write cmd failed\n", __func__);
		goto p_err;
	}

p_err:
	return ret;
}

static int sensor_4ec_awb_lock(struct v4l2_subdev *subdev, int ctrl)
{
	int ret = 0;
	struct fimc_is_module_enum *module;
	struct fimc_is_module_4ec *module_4ec;

	module = (struct fimc_is_module_enum *)v4l2_get_subdevdata(subdev);
	if (unlikely(!module)) {
		err("module is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	module_4ec = module->private_data;
	if (unlikely(!module_4ec)) {
		err("module_4ec is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	cam_info("%s( %d ) : E\n",__func__, ctrl);

	if (module_4ec->awb_lock == ctrl) {
		cam_info("%s: same awb_lock mode( %d ). skip to set awb_lock mode.\n", __FUNCTION__, ctrl);
		return 0;
	}

	if (module_4ec->white_balance != WHITE_BALANCE_AUTO) {
		cam_info("%s cannot lock/unlock awb on MWB mode\n",__func__);
		return 0;
	}

	switch (ctrl) {
	case AWB_UNLOCK :
	case AWB_LOCK :
		module_4ec->awb_lock = ctrl;
		break;
	default:
		err("%s: Not support.(%d)\n", __func__, ctrl);
		ret = -EINVAL;
		goto p_err;
		break;
	}

	ret = sensor_4ec_apply_set(subdev, &regs_set.awb_lockunlock[module_4ec->awb_lock]);
	if (ret) {
		err("%s: write cmd failed\n", __func__);
		goto p_err;
	}

p_err:
	return ret;
}

static int sensor_4ec_ae_awb_lock(struct v4l2_subdev *subdev, int ae_lock, int awb_lock)
{
	int ret = 0;
	struct fimc_is_module_enum *module;
	struct fimc_is_module_4ec *module_4ec;

	module = (struct fimc_is_module_enum *)v4l2_get_subdevdata(subdev);
	if (unlikely(!module)) {
		err("module is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	module_4ec = module->private_data;
	if (unlikely(!module_4ec)) {
		err("module_4ec is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	cam_info("%s( %d, %d ) : E\n",__func__, ae_lock, awb_lock);

	ret = sensor_4ec_ae_lock(subdev, ae_lock);
	if (ret) {
		err("%s: ae_lock(%d) failed\n", __func__, ae_lock);
		goto p_err;
	}

	ret = sensor_4ec_awb_lock(subdev, awb_lock);
	if (ret) {
		err("%s: awbe_lock(%d) failed\n", __func__, awb_lock);
		goto p_err;
	}

p_err:
	return ret;
}

static int sensor_4ec_read_sensor_version(struct v4l2_subdev *subdev, u16* sensor_version)
{
	int ret = 0;
	struct i2c_client *client = to_client(subdev);
	struct fimc_is_module_4ec *module_4ec = to_state(subdev);

	BUG_ON(!client);
	BUG_ON(!module_4ec);

	cam_info("%s : E\n", __func__);

	mutex_lock(&module_4ec->i2c_lock);

	ret = fimc_is_sensor_write16(client, 0xFCFC, 0xD000);
	ret |= fimc_is_sensor_write16(client, 0x0028, 0xD000);
	ret |= fimc_is_sensor_write16(client, 0x002A, 0x0012);
	ret |= fimc_is_sensor_write16(client, 0x0F12, 0x0001);
	ret |= fimc_is_sensor_write16(client, 0x002A, 0x007A);
	ret |= fimc_is_sensor_write16(client, 0x0F12, 0x0000);
	ret |= fimc_is_sensor_write16(client, 0x002A, 0xA000);
	ret |= fimc_is_sensor_write16(client, 0x0F12, 0x0004);
	ret |= fimc_is_sensor_write16(client, 0x002A, 0xA002);
	ret |= fimc_is_sensor_write16(client, 0x0F12, 0x000F); // F page_select
	ret |= fimc_is_sensor_write16(client, 0x002A, 0xA000);
	ret |= fimc_is_sensor_write16(client, 0x0F12, 0x0001);

	usleep_range(200, 200);

	ret |= fimc_is_sensor_write16(client, 0x002C, 0xD000);
	ret |= fimc_is_sensor_write16(client, 0x002E, 0xA044);
	ret |= fimc_is_sensor_read16(client, 0x0F12, sensor_version);

	mutex_unlock(&module_4ec->i2c_lock);

	cam_info("%s : X\n", __func__);
	return ret;
}

static int sensor_4ec_init(struct v4l2_subdev *subdev, u32 val)
{
	int ret = 0;
	struct fimc_is_module_enum *module;
	struct fimc_is_module_4ec *module_4ec;
	struct i2c_client *client;
	u16 pid = 0;
	u16 revision = 0;
#ifndef CONFIG_LOAD_FILE
	u16 vendor_id = 0;
#endif

	BUG_ON(!subdev);

	cam_info("%s start\n", __func__);

	module = (struct fimc_is_module_enum *)v4l2_get_subdevdata(subdev);
	module_4ec = module->private_data;
	client = module->client;

	module_4ec->system_clock = 146 * 1000 * 1000;
	module_4ec->line_length_pck = 146 * 1000 * 1000;

	if(module_4ec->sensor_version == 0) {
		ret = sensor_4ec_read_sensor_version(subdev, &module_4ec->sensor_version);
		if (ret < 0) {
			err("%s: read_sensor_version failed\n", __func__);
			goto p_err;
		}

#ifdef SUPPORT_MULTIPLE_SENSOR_VERSION
		if(module_4ec->sensor_version == SENSOR_VERSION_4ECGA04) {
			module_4ec->setfile_index = SETFILE_INDEX_4ECGA04;
			strcpy(module->sensor_name, SENSOR_NAME_4ECGA04);
		} else {
			module_4ec->setfile_index = SETFILE_INDEX_4ECGX04;
			strcpy(module->sensor_name, SENSOR_NAME_4ECGX04);
		}
#else
		module_4ec->setfile_index = SETFILE_INDEX_4EC_DEFAULT;
#endif
	}

	cam_info("%s : Sensor Version 0x%04X\n", __func__, module_4ec->sensor_version);
	cam_info("%s : setfile_index %d\n", __func__, module_4ec->setfile_index);

#ifdef CONFIG_LOAD_FILE
	ret = sensor_4ec_regs_table_init();
	if (ret < 0) {
		err("%s: [CONFIG_LOAD_FILE] init fail \n", __func__);
		goto p_err;
	}
#endif

	/* init member */
	module_4ec->flash_mode = FLASH_MODE_OFF;
	module_4ec->flash_status = FLASH_STATUS_OFF;
	module_4ec->af_status = AF_NONE;
	module_4ec->af_result = AF_RESULT_NONE;
	module_4ec->sensor_af_in_low_light_mode = false;
	module_4ec->preview.width = 0;
	module_4ec->preview.height = 0;

	ret = sensor_4ec_read_reg(subdev, &regs_set.get_pid, &pid, 1);
	if (ret < 0) {
		err("%s: read PID failed\n", __func__);
		goto p_err;
	}

	cam_info("%s : pid 0x%04X\n", __func__, pid);

	ret = sensor_4ec_read_reg(subdev, &regs_set.get_revision, &revision, 1);
	if (ret < 0) {
		err("%s: read Version failed\n", __func__);
		goto p_err;
	}

	cam_info("%s : revision 0x%04X\n", __func__, revision);

#ifndef CONFIG_LOAD_FILE
	if(strcmp(module->sensor_vendorid, "NULL") == 0) {
		ret = sensor_4ec_apply_set(subdev, &regs_set.set_vendor_id_read);
		if (ret < 0) {
			err("%s: set_vendor_id_read failed\n", __func__);
			goto p_err;
		}

		usleep_range(200, 200);

		ret = sensor_4ec_read_reg(subdev, &regs_set.get_vendor_id_read, &vendor_id, 1);
		if (ret < 0) {
			err("%s: read Module vendor ID failed\n", __func__);
			goto p_err;
		}

		sprintf(module->sensor_vendorid, "0x%04X", vendor_id);
	}
	cam_info("%s : Module vendor ID %s\n", __func__, module->sensor_vendorid);
#endif

	msleep(10);

	ret = sensor_4ec_apply_set(subdev, &regs_set.init_reg_1);
	if (ret < 0) {
		err("%s: init_reg_1 failed\n", __func__);
		goto p_err;
	}

#ifdef CONFIG_LOAD_FILE
	ret = sensor_4ec_apply_set(subdev, &regs_set.init_reg_2);
	if (ret < 0) {
		err("%s: init_reg_2 failed\n", __func__);
		goto p_err;
	}
#else
	{
		struct s5k4ecgx_regset *regset_table;
		struct s5k4ecgx_regset *regset;
		struct s5k4ecgx_regset *end_regset;
		u8 *regset_data;
		u8 *dst_ptr;
		const u32 *end_src_ptr;
		bool flag_copied;
		u32 set_idx = module_4ec->setfile_index;
		int init_reg_2_array_size = regs_set.init_reg_2.array_size[set_idx];
		int init_reg_2_size = init_reg_2_array_size * sizeof(u32);
		const u32 *src_ptr = regs_set.init_reg_2.reg[set_idx];
		u32 src_value;
		int err;

		cam_info("%s : start reg 2 bursts\n", __func__);

		regset_data = vmalloc(init_reg_2_size);
		if (regset_data == NULL)
			return -ENOMEM;
		regset_table = vmalloc((u32)sizeof(struct s5k4ecgx_regset) * init_reg_2_size);
		if (regset_table == NULL) {
			kfree(regset_data);
			return -ENOMEM;
		}

		dst_ptr = regset_data;
		regset = regset_table;
		end_src_ptr = &regs_set.init_reg_2.reg[set_idx][init_reg_2_array_size];

		src_value = *src_ptr++;
		while (src_ptr <= end_src_ptr) {
			/* initial value for a regset */
			regset->data = dst_ptr;
			flag_copied = false;
			*dst_ptr++ = src_value >> 24;
			*dst_ptr++ = src_value >> 16;
			*dst_ptr++ = src_value >> 8;
			*dst_ptr++ = src_value;

			/* check subsequent values for a data flag (starts with
			   0x0F12) or something else */
			do {
				src_value = *src_ptr++;
				if ((src_value & 0xFFFF0000) != 0x0F120000) {
					/* src_value is start of next regset */
					regset->size = dst_ptr - regset->data;
					regset++;
					break;
				}
				/* copy the 0x0F12 flag if not done already */
				if (!flag_copied) {
					*dst_ptr++ = src_value >> 24;
					*dst_ptr++ = src_value >> 16;
					flag_copied = true;
				}
				/* copy the data part */
				*dst_ptr++ = src_value >> 8;
				*dst_ptr++ = src_value;
			} while (src_ptr < end_src_ptr);
		}
		cam_info("%s : finished creating table\n", __func__);

		end_regset = regset;
		cam_info("%s : first regset = %p, last regset = %p, count = %d\n",
			__func__, regset_table, regset, end_regset - regset_table);
		cam_info("%s : regset_data = %p, end = %p, dst_ptr = %p\n", __func__,
			regset_data, regset_data + (init_reg_2_size * sizeof(u32)),
			dst_ptr);

		regset = regset_table;
		cam_info("%s : start writing init reg 2 bursts\n", __func__);
		do {
			if (regset->size > 4) {
				/* write the address packet */
				err = fimc_is_sensor_write(client, regset->data, 4);
				if (err)
					break;
				/* write the data in a burst */
				err = fimc_is_sensor_write(client, regset->data+4,
							regset->size-4);

			} else
				err = fimc_is_sensor_write(client, regset->data,
							regset->size);
			if (err)
				break;
			regset++;
		} while (regset < end_regset);

		cam_info("%s : finished writing init reg 2 bursts\n", __func__);

		vfree(regset_data);
		vfree(regset_table);

		if (err) {
			err("%s: write cmd failed\n", __func__);
			goto p_err;
		}
	}
#endif

	module_4ec->mode = S5K4ECGX_OPRMODE_VIDEO;
	module_4ec->sensor_mode = SENSOR_CAMERA;
	module_4ec->contrast = CONTRAST_DEFAULT;
	module_4ec->effect = IMAGE_EFFECT_NONE;
	module_4ec->ev = GET_CID_INDEX(EV_DEFAULT, EV_MIN_VALUE);
	module_4ec->flash_mode = FLASH_MODE_OFF;
	module_4ec->focus_mode = V4L2_FOCUS_MODE_AUTO;
	module_4ec->iso = ISO_AUTO;
	module_4ec->metering = METERING_CENTER;
	module_4ec->saturation = SATURATION_DEFAULT;
	module_4ec->scene_mode = SCENE_MODE_NONE;
	module_4ec->sharpness = SHARPNESS_DEFAULT;
	module_4ec->white_balance = WHITE_BALANCE_AUTO;
	module_4ec->fps = FRAME_RATE_AUTO;
	module_4ec->ae_lock = AE_UNLOCK;
	module_4ec->awb_lock = AWB_UNLOCK;
	module_4ec->user_ae_lock = AE_UNLOCK;
	module_4ec->user_awb_lock = AWB_UNLOCK;
	module_4ec->anti_banding = S5K4ECGX_FLICKER_50HZ_AUTO;
	module_4ec->light_level = 0;

	ret = sensor_4ec_apply_set(subdev, &regs_set.effect[module_4ec->effect]);
	ret |= sensor_4ec_apply_set(subdev, &regs_set.ev[module_4ec->ev]);
	ret |= sensor_4ec_apply_set(subdev, &regs_set.iso[module_4ec->iso]);
	ret |= sensor_4ec_apply_set(subdev, &regs_set.metering[module_4ec->metering]);
	ret |= sensor_4ec_apply_set(subdev, &regs_set.scene_mode[module_4ec->scene_mode]);
	ret |= sensor_4ec_apply_set(subdev, &regs_set.ae_lockunlock[module_4ec->ae_lock]);
	ret |= sensor_4ec_apply_set(subdev, &regs_set.awb_lockunlock[module_4ec->awb_lock]);
	ret |= sensor_4ec_apply_set(subdev, &regs_set.white_balance[module_4ec->white_balance]);
	ret |= sensor_4ec_apply_set(subdev, &regs_set.fps[module_4ec->fps]);
	ret |= sensor_4ec_apply_set(subdev, &regs_set.flash_init);
	if (ret) {
		err("%s: write cmd failed\n", __func__);
		goto p_err;
	}

	ret = (schedule_work(&module_4ec->set_focus_mode_work) == 1) ? 0 : -EINVAL;
	if (ret < 0) {
		err("%s: set focus mode fail.\n", __func__);
	}

	cam_info("[MOD:D:%d] %s(%d)\n", module->sensor_id, __func__, val);

p_err:
	return ret;
}

static int sensor_4ec_s_format(struct v4l2_subdev *subdev, struct v4l2_mbus_framefmt *fmt)
{
	int ret = 0;
	struct s5k4ecgx_framesize const *size;
	struct fimc_is_module_enum *module;
	struct fimc_is_module_4ec *module_4ec;
	u32 index;

	BUG_ON(!subdev);
	BUG_ON(!fmt);

	cam_info("%s\n", __func__);

	module = (struct fimc_is_module_enum *)v4l2_get_subdevdata(subdev);
	if (!module) {
		err("module is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	module_4ec = module->private_data;
	if (unlikely(!module_4ec)) {
		err("module_4ec is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	module_4ec->image.window.offs_h = 0;
	module_4ec->image.window.offs_v = 0;
	module_4ec->image.window.width = fmt->width;
	module_4ec->image.window.height = fmt->height;
	module_4ec->image.window.o_width = fmt->width;
	module_4ec->image.window.o_height = fmt->height;
	module_4ec->image.format.pixelformat = fmt->code;

	size = NULL;

	if(module_4ec->mode == S5K4ECGX_OPRMODE_IMAGE) {
		cam_info("%s for capture \n", __func__);
		for (index = 0; index < ARRAY_SIZE(capture_size_list); ++index) {
			if ((capture_size_list[index].width == fmt->width) &&
				(capture_size_list[index].height == fmt->height)) {
				size = &capture_size_list[index];
				break;
			}
		}

		if (!size) {
			err("the capture size(%d x %d) is not supported", fmt->width, fmt->height);
			ret = -EINVAL;
			goto p_err;
		}

		ret = sensor_4ec_apply_set(subdev, &regs_set.capture_size[size->index]);
		if (ret) {
			err("%s: write cmd failed\n", __func__);
			goto p_err;
		}
	} else {
		cam_info("%s for preview \n", __func__);
		for (index = 0; index < ARRAY_SIZE(preview_size_list); ++index) {
			if ((preview_size_list[index].width == fmt->width) &&
				(preview_size_list[index].height == fmt->height)) {
				size = &preview_size_list[index];
				break;
			}
		}

		if (!size) {
			err("the preview size(%d x %d) is not supported", fmt->width, fmt->height);
			ret = -EINVAL;
			goto p_err;
		}

		ret = sensor_4ec_apply_set(subdev, &regs_set.preview_size[size->index]);
		if (ret) {
			err("%s: write cmd failed\n", __func__);
			goto p_err;
		}
		module_4ec->preview.width = size->width;
		module_4ec->preview.height = size->height;
	}

	cam_info("%s: stream size(%d) %d x %d\n", __func__, size->index, size->width, size->height);

p_err:
	return ret;
}

static int sensor_4ec_s_stream(struct v4l2_subdev *subdev, int enable)
{
	int ret = 0;
	struct fimc_is_module_enum *module;

	cam_info("%s: %d\n", __func__, enable);

	module = (struct fimc_is_module_enum *)v4l2_get_subdevdata(subdev);
	if (!module) {
		err("module is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	if (enable) {
		ret = CALL_MOPS(module, stream_on, subdev);
		if (ret) {
			err("stream_on is fail(%d)", ret);
			goto p_err;
		}
	} else {
		ret = CALL_MOPS(module, stream_off, subdev);
		if (ret) {
			err("stream_off is fail(%d)", ret);
			goto p_err;
		}
	}

p_err:
	return 0;
}

static int sensor_4ec_s_param(struct v4l2_subdev *subdev, struct v4l2_streamparm *param)
{
	int ret = 0;
	struct fimc_is_module_enum *module;
	struct v4l2_captureparm *cp;
	struct v4l2_fract *tpf;
	u64 framerate;

	BUG_ON(!subdev);
	BUG_ON(!param);

	cp = &param->parm.capture;
	tpf = &cp->timeperframe;

	if (!tpf->numerator) {
		err("numerator is 0");
		ret = -EINVAL;
		goto p_err;
	}

	framerate = tpf->denominator;

	module = (struct fimc_is_module_enum *)v4l2_get_subdevdata(subdev);
	if (!module) {
		err("module is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	ret = CALL_MOPS(module, s_duration, subdev, framerate);
	if (ret) {
		err("s_duration is fail(%d)", ret);
		goto p_err;
	}

p_err:
	return ret;
}

static int sensor_4ec_stream_on(struct v4l2_subdev *subdev)
{
	int ret = 0;
	u16 read_value = 0;
	u32 poll_time_ms = 0;
	struct fimc_is_module_enum *module;
	struct fimc_is_module_4ec *module_4ec;

	BUG_ON(!subdev);

	module = (struct fimc_is_module_enum *)v4l2_get_subdevdata(subdev);
	if (unlikely(!module)) {
		err("module is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	module_4ec = module->private_data;
	if (unlikely(!module_4ec)) {
		err("module_4ec is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	if(module_4ec->mode == S5K4ECGX_OPRMODE_IMAGE) {
		/* Do Flash work */
		cam_dbg("%s: light_level = 0x%08x\n", __func__, module_4ec->light_level);
		module_4ec->flash_fire = false;

		switch (module_4ec->flash_mode) {
		case FLASH_MODE_AUTO:
			if (!IS_LOWLIGHT(module_4ec->light_level)) {
				/* light level bright enough
				 * that we don't need flash
				 */
				break;
			}
			/* fall through to flash start */
		case FLASH_MODE_ON:
			ret = sensor_4ec_main_flash_start(subdev);
			if (ret) {
				err("%s: main_flash_start failed\n", __func__);
				goto p_err;
			}

			module_4ec->flash_fire = true;
			break;
		default:
			break;
		}

		/* Start Capture */
		ret = sensor_4ec_apply_set(subdev, &regs_set.capture_start);
		if (ret) {
			err("%s: write cmd failed\n", __func__);
			goto p_err;
		}

		msleep(50);
		poll_time_ms = 50;
		do {
			sensor_4ec_read_reg(subdev, &regs_set.get_modechange_check, &read_value, 1);
			if (read_value == 0x0100)
				break;
			msleep(POLL_TIME_MS);
			poll_time_ms += POLL_TIME_MS;
		} while (poll_time_ms < CAPTURE_POLL_TIME_MS);

		cam_info("%s: capture done check finished after %d ms\n", __func__, poll_time_ms);
	} else {
		ret = sensor_4ec_apply_set(subdev, &regs_set.init_reg_3);
		if (ret) {
			err("%s: write cmd failed\n", __func__);
			goto p_err;
		}

		/* Checking preveiw start (handshaking) */
		do {
			sensor_4ec_read_reg(subdev, &regs_set.get_preview_status, &read_value, 1);
			if (read_value == 0x00)
				break;
			cam_dbg("%s: value = %d\n",  __func__, read_value);
			msleep(POLL_TIME_MS);
			poll_time_ms += POLL_TIME_MS;
		} while (poll_time_ms < CAPTURE_POLL_TIME_MS);

		cam_info("%s: preview on done check finished after %d ms\n", __func__, poll_time_ms);
	}

	cam_info("%s: stream on end\n", __func__);

p_err:
	return ret;
}

static int sensor_4ec_stream_off(struct v4l2_subdev *subdev)
{
	int ret = 0;
	u16 read_value = 0;
	u32 poll_time_ms = 0;
	struct fimc_is_module_enum *module;
	struct fimc_is_module_4ec *module_4ec;

	BUG_ON(!subdev);

	module = (struct fimc_is_module_enum *)v4l2_get_subdevdata(subdev);
	if (unlikely(!module)) {
		err("module is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	module_4ec = module->private_data;
	if (unlikely(!module_4ec)) {
		err("module_4ec is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	flush_work(&module_4ec->set_focus_mode_work);
	flush_work(&module_4ec->af_work);
	flush_work(&module_4ec->af_stop_work);

	if(module_4ec->mode == S5K4ECGX_OPRMODE_IMAGE) {
		u16 size[2] = {0, };

		if(module_4ec->flash_status == FLASH_STATUS_MAIN_ON) {
			ret = sensor_4ec_main_flash_stop(subdev);
			if (ret) {
				err("%s: main_flash_stop failed\n", __func__);
				goto p_err;
			}
		}

		sensor_4ec_read_addr(subdev, 0x1d02, size, 2);
		cam_info("%s: Captured image width = %d, height = %d\n",  __func__, size[0], size[1]);

		/* get exif information */
		ret = sensor_4ec_get_exif(subdev);
		if (ret) {
			err("%s: get exif failed\n", __func__);
			goto p_err;
		}
	}

	ret = sensor_4ec_apply_set(subdev, &regs_set.init_reg_4);
	if (ret) {
		err("%s: write cmd failed\n", __func__);
		goto p_err;
	}

	/* Checking preveiw start (handshaking) */
	do {
		sensor_4ec_read_reg(subdev, &regs_set.get_preview_status, &read_value, 1);
		if (read_value == 0x00)
			break;
		cam_dbg("%s: value = %d\n",  __func__, read_value);
		msleep(POLL_TIME_MS);
		poll_time_ms += POLL_TIME_MS;
	} while (poll_time_ms < CAPTURE_POLL_TIME_MS);

	cam_info("%s: preview off done check finished after %d ms\n", __func__, poll_time_ms);
/*
	if (module_4ec->runmode == S5K4ECGX_RUNMODE_CAPTURE) {
		dev_dbg(&client->dev, "%s: sending Preview_Return cmd\n",
			__func__);
		sensor_4ec_apply_set(subdev, &regs_set->preview_return);
	}

	if (module_4ec->runmode == S5K4ECGX_RUNMODE_RUNNING
	    || module_4ec->runmode == S5K4ECGX_RUNMODE_CAPTURE)
		module_4ec->runmode = S5K4ECGX_RUNMODE_IDLE;
*/

	cam_info("%s: stream off\n", __func__);

p_err:
	return ret;
}

static int sensor_4ec_get_exif_exptime(struct v4l2_subdev *subdev, u32 *exp_time)
{
	int err = 0;
	u16 exptime[2] = {0, };
	u32 val;

	err = sensor_4ec_read_reg(subdev, &regs_set.get_exptime, exptime, 2);
	if (err) {
		err("%s: read exptime failed\n", __func__);
		goto out;
	}

	cam_info("%s: exp_time_den value(LSB) = %08X, (MSB) = %08X\n",
		__func__, exptime[0], exptime[1]);

	val = ((exptime[1] << 16) | (exptime[0] & 0xffff));
	*exp_time = (val * 1000) / 400;

	cam_info("%s: exp_time_den value = %08X\n",
		__func__, *exp_time);

out:
	return err;
}

static int sensor_4ec_get_exif_iso(struct v4l2_subdev *subdev, u16 *iso)
{
	int err = 0;
	u16 gain_value = 0;
	u16 gain[2] = {0, };

	struct fimc_is_module_enum *module;
	struct fimc_is_module_4ec *module_4ec;

	BUG_ON(!subdev);

	module = (struct fimc_is_module_enum *)v4l2_get_subdevdata(subdev);
	if (unlikely(!module)) {
		err("module is NULL");
		goto out;
	}

	module_4ec = module->private_data;
	if (unlikely(!module_4ec)) {
		err("module_4ec is NULL");
		goto out;
	}

	err = sensor_4ec_read_reg(subdev, &regs_set.get_iso, gain, 2);
	if (err) {
		err("%s: read iso failed\n", __func__);
		goto out;
	}

	cam_info("%s: a_gain value = %08X, d_gain value = %08X\n",
		__func__, gain[0], gain[1]);

	gain_value = ((gain[0] * gain[1]) / 0x100 ) / 2;
	cam_info("%s: iso : gain_value=%08X\n", __func__, gain_value);

	switch (module_4ec->iso) {
	case ISO_AUTO :
		if (gain_value < 256) { /*0x100*/
			*iso = 50;
		} else if (gain_value < 512) { /*0x1ff*/
			*iso = 100;
		} else if (gain_value < 896) { /*0x37f*/
			*iso = 200;
		} else {
			*iso = 400;
		}
		break;
	case ISO_50 :
		*iso = 100; /* map to 100 */
		break;
	case ISO_100 :
		*iso = 100;
		break;
	case ISO_200 :
		*iso = 200;
		break;
	case ISO_400 :
		*iso = 400;
		break;
	default:
		err("%s: Not support. id(%d)\n", __func__, module_4ec->iso);
		break;
	}

	cam_info("%s: ISO=%d\n", __func__, *iso);
out:
	return err;
}

static void sensor_4ec_get_exif_flash(struct v4l2_subdev *subdev,
					u16 *flash)
{
	struct fimc_is_module_enum *module;
	struct fimc_is_module_4ec *module_4ec;

	BUG_ON(!subdev);

	module = (struct fimc_is_module_enum *)v4l2_get_subdevdata(subdev);
	module_4ec = module->private_data;

	if (module_4ec->flash_fire) {
		*flash = EXIF_FLASH_FIRED;
		module_4ec->flash_fire = false;
	} else {
		*flash = 0;
	}

	cam_info("%s: flash(%d)\n", __func__, *flash);
}

static int sensor_4ec_get_exif(struct v4l2_subdev *subdev)
{
	int ret = 0;
	u32 exp_time = 0;
	struct fimc_is_module_enum *module;
	struct fimc_is_module_4ec *module_4ec;

	BUG_ON(!subdev);

	module = (struct fimc_is_module_enum *)v4l2_get_subdevdata(subdev);
	module_4ec = module->private_data;

	/* exposure time */
	module_4ec->exif.exp_time_num = 1;
	module_4ec->exif.exp_time_den = 0;
	ret = sensor_4ec_get_exif_exptime(subdev, &exp_time);
	if (exp_time) {
		module_4ec->exif.exp_time_den = 1000 * 1000 / exp_time;
	} else {
		cam_err("EXIF: error, exposure time 0. %d\n", ret);
	}

	/* iso */
	module_4ec->exif.iso = 0;
	ret |= sensor_4ec_get_exif_iso(subdev, &module_4ec->exif.iso);

	/* flash */
	sensor_4ec_get_exif_flash(subdev, &module_4ec->exif.flash);

	return ret;
}

static u32 sensor_4ec_get_light_level(struct v4l2_subdev *subdev)
{
	int ret = 0;
	u16 light[2] = {0, };
	u32 light_sum = 0;
	struct fimc_is_module_enum *module;
	struct fimc_is_module_4ec *module_4ec;

	module = (struct fimc_is_module_enum *)v4l2_get_subdevdata(subdev);
	if (unlikely(!module)) {
		err("module is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	module_4ec = module->private_data;
	if (unlikely(!module_4ec)) {
		err("module_4ec is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	ret = sensor_4ec_read_reg(subdev, &regs_set.get_light_level, light, 2);
	if (ret) {
		err("%s: read cmd failed\n", __func__);
		goto p_err;
	}

	light_sum = (light[1] << 16) | light[0];

	cam_info("%s: light value = 0x%08x\n",
		__func__, light_sum);

p_err:
	return light_sum;
}

static u32 sensor_4ec_get_frame_duration(struct v4l2_subdev *subdev)
{
	int ret = 0;
	u16 frame_duration = 0;
	u16 frame_duration_ms = 0;
	struct fimc_is_module_enum *module;
	struct fimc_is_module_4ec *module_4ec;

	module = (struct fimc_is_module_enum *)v4l2_get_subdevdata(subdev);
	if (unlikely(!module)) {
		err("module is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	module_4ec = module->private_data;
	if (unlikely(!module_4ec)) {
		err("module_4ec is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	ret = sensor_4ec_read_reg(subdev, &regs_set.get_frame_duration, &frame_duration, 1);
	if (ret) {
		err("%s: read cmd failed\n", __func__);
		goto p_err;
	}

	frame_duration_ms = frame_duration / 400;

	cam_info("%s: frame_duration_ms = %d ms\n", __func__, frame_duration_ms);

p_err:
	return frame_duration_ms;
}

static int sensor_4ec_pre_flash_start(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct fimc_is_module_enum *module;
	struct fimc_is_module_4ec *module_4ec;
	int count;
	u16 read_value;

	module = (struct fimc_is_module_enum *)v4l2_get_subdevdata(subdev);
	if (unlikely(!module)) {
		err("module is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	module_4ec = module->private_data;
	if (unlikely(!module_4ec)) {
		err("module_4ec is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	/* AE / AWB Unlock */
	ret = sensor_4ec_ae_awb_lock(subdev, AE_UNLOCK, AWB_UNLOCK);
	if (ret) {
		err("%s: AE/AWB unlock failed\n", __func__);
		goto p_err;
	}

	/* Fast AE On */
	ret = sensor_4ec_apply_set(subdev, &regs_set.fast_ae_on);
	if (ret) {
		err("%s: write cmd failed\n", __func__);
		goto p_err;
	}

	/* Pre Flash Start */
	ret = sensor_4ec_apply_set(subdev, &regs_set.af_assist_flash_start);
	if (ret) {
		err("%s: write cmd failed\n", __func__);
		goto p_err;
	}

	/* Pre Flash On */
	cam_info("%s: Preflash On\n", __func__);
#if defined(CONFIG_FLED_SM5703)
	sm5703_led_mode_ctrl(SM5703_FLED_MODE_PREFLASH);
	if (flash_control_ready == false) {
		sm5703_led_mode_ctrl(SM5703_FLED_MODE_EXT_GPIO_ENABLE);
		flash_control_ready = true;
	}
#elif defined(CONFIG_LEDS_S2MU005)
	s2mu005_led_mode_ctrl(S2MU005_FLED_MODE_PREFLASH);
#endif
	module_4ec->flash_status = FLASH_STATUS_PRE_ON;

	/* delay 400ms (SLSI value) and then poll to see if AE is stable.
	 * once it is stable, lock it and then return to do AF
	 */
	msleep(400);

	/* enter read mode */
	for (count = 0; count < AE_STABLE_SEARCH_COUNT; count++) {
		if (module_4ec->af_status == AF_CANCEL)
			break;
		ret = sensor_4ec_read_reg(subdev, &regs_set.get_ae_stable_status, &read_value, 1);
		if (ret) {
			err("%s: write cmd failed\n", __func__);
			goto p_err;
		}

		cam_info("%s: ae stable status = 0x%04x\n",
			__func__, read_value);
		if (read_value == 0x1)
			break;
		msleep(NORMAL_FRAME_DELAY_MS);
	}

p_err:
	return ret;
}

static int sensor_4ec_pre_flash_stop(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct fimc_is_module_enum *module;
	struct fimc_is_module_4ec *module_4ec;

	module = (struct fimc_is_module_enum *)v4l2_get_subdevdata(subdev);
	if (unlikely(!module)) {
		err("module is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	module_4ec = module->private_data;
	if (unlikely(!module_4ec)) {
		err("module_4ec is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	/* Fast AE Off */
	ret = sensor_4ec_apply_set(subdev, &regs_set.fast_ae_off);
	if (ret) {
		err("%s: write cmd failed\n", __func__);
		goto p_err;
	}

	/* AE / AWB Unlock */
	ret = sensor_4ec_ae_lock(subdev, AE_UNLOCK);
	if (ret) {
		err("%s: AE unlock failed\n", __func__);
		goto p_err;
	}

	ret = sensor_4ec_awb_lock(subdev, AWB_UNLOCK);
	if (ret) {
		err("%s: AWB unlock failed\n", __func__);
		goto p_err;
	}

	/* Pre Flash End */
	ret = sensor_4ec_apply_set(subdev, &regs_set.af_assist_flash_end);
	if (ret) {
		err("%s: write cmd failed\n", __func__);
		goto p_err;
	}

	/* Pre Flash Off */
	cam_info("%s: Preflash Off\n", __func__);
#if defined(CONFIG_FLED_SM5703)
	sm5703_led_mode_ctrl(SM5703_FLED_MODE_OFF);
#elif defined(CONFIG_LEDS_S2MU005)
	s2mu005_led_mode_ctrl(S2MU005_FLED_MODE_OFF);
#endif
	module_4ec->flash_status = FLASH_STATUS_OFF;

p_err:
	return ret;
}

static int sensor_4ec_main_flash_start(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct fimc_is_module_enum *module;
	struct fimc_is_module_4ec *module_4ec;

	module = (struct fimc_is_module_enum *)v4l2_get_subdevdata(subdev);
	if (unlikely(!module)) {
		err("module is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	module_4ec = module->private_data;
	if (unlikely(!module_4ec)) {
		err("module_4ec is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	/* Main Flash On */
	cam_info("%s: Main Flash On\n", __func__);
#if defined(CONFIG_FLED_SM5703)
	sm5703_led_mode_ctrl(SM5703_FLED_MODE_FLASH);
#elif defined(CONFIG_LEDS_S2MU005)
	s2mu005_led_mode_ctrl(S2MU005_FLED_MODE_FLASH);
#endif

	/* Main Flash Start */
	ret = sensor_4ec_apply_set(subdev, &regs_set.flash_start);
	if (ret) {
		err("%s: write cmd failed\n", __func__);
		goto p_err;
	}

	module_4ec->flash_status = FLASH_STATUS_MAIN_ON;

p_err:
	return ret;
}

static int sensor_4ec_main_flash_stop(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct fimc_is_module_enum *module;
	struct fimc_is_module_4ec *module_4ec;

	module = (struct fimc_is_module_enum *)v4l2_get_subdevdata(subdev);
	if (unlikely(!module)) {
		err("module is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	module_4ec = module->private_data;
	if (unlikely(!module_4ec)) {
		err("module_4ec is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	/* Main Flash End */
	ret = sensor_4ec_apply_set(subdev, &regs_set.flash_end);
	if (ret) {
		err("%s: write cmd failed\n", __func__);
		goto p_err;
	}

	/* Main Flash Off */
	cam_info("%s: Main Flash Off\n", __func__);
#if defined(CONFIG_FLED_SM5703)
	sm5703_led_mode_ctrl(SM5703_FLED_MODE_OFF);
#elif defined(CONFIG_LEDS_S2MU005)
	s2mu005_led_mode_ctrl(S2MU005_FLED_MODE_OFF);
#endif
	module_4ec->flash_status = FLASH_STATUS_OFF;

p_err:
	return ret;
}

static int sensor_4ec_auto_focus_start(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct fimc_is_module_enum *module;
	struct fimc_is_module_4ec *module_4ec;

	BUG_ON(!subdev);

	module = (struct fimc_is_module_enum *)v4l2_get_subdevdata(subdev);
	if (unlikely(!module)) {
		err("module is NULL");
		return -EINVAL;
	}

	module_4ec = module->private_data;
	if (unlikely(!module_4ec)) {
		err("module_4ec is NULL");
		return -EINVAL;
	}

	mutex_lock(&module_4ec->af_lock);

	/* Check low light*/
	module_4ec->light_level = sensor_4ec_get_light_level(subdev);

	switch (module_4ec->flash_mode) {
	case FLASH_MODE_AUTO:
		if (!IS_LOWLIGHT(module_4ec->light_level)) {
			/* flash not needed */
			break;
		}
		/* fall through to turn on flash for AF assist */
	case FLASH_MODE_ON:
		ret = sensor_4ec_pre_flash_start(subdev);
		if (ret) {
			err("%s: pre flash start failed\n", __func__);
			goto p_err;
		}

		if (module_4ec->af_status == AF_CANCEL) {
			ret = sensor_4ec_pre_flash_stop(subdev);
			if (ret) {
				err("%s: pre flash stop failed\n", __func__);
				goto p_err;
			}
			mutex_unlock(&module_4ec->af_lock);
			return 0;
		}
		break;
	case FLASH_MODE_OFF:
		break;
	default:
		cam_info("%s: Unknown Flash mode 0x%x\n",
			__func__, module_4ec->flash_mode);
		break;
	}

	/* AE / AWB Lock */
	ret = sensor_4ec_ae_awb_lock(subdev, AE_LOCK, AWB_LOCK);
	if (ret) {
		err("%s: AE/AWB lock failed\n", __func__);
		goto p_err;
	}

	if (!IS_LOWLIGHT(module_4ec->light_level)) {
		if (module_4ec->sensor_af_in_low_light_mode) {
			module_4ec->sensor_af_in_low_light_mode = false;
			ret = sensor_4ec_apply_set(subdev, &regs_set.af_low_light_mode_off);
			if (ret) {
				err("%s: write cmd failed\n", __func__);
				goto p_err;
			}
		}
	} else {
		if (!module_4ec->sensor_af_in_low_light_mode) {
			module_4ec->sensor_af_in_low_light_mode = true;
			ret = sensor_4ec_apply_set(subdev, &regs_set.af_low_light_mode_on);
			if (ret) {
				err("%s: write cmd failed\n", __func__);
				goto p_err;
			}
		}
	}

	/* Start AF */
	module_4ec->af_status = AF_START;
	ret = sensor_4ec_apply_set(subdev, &regs_set.single_af_start);
	if (ret) {
		err("%s: write cmd failed\n", __func__);
		goto p_err;
	}

	INIT_COMPLETION(module_4ec->af_complete);

	cam_info("%s: AF started.\n", __func__);

	ret = sensor_4ec_auto_focus_proc(subdev);
	if (ret) {
		err("%s: auto_focus_proc failed\n", __func__);
	}

p_err:
	mutex_unlock(&module_4ec->af_lock);
	return ret;
}

static int sensor_4ec_auto_focus_stop(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct fimc_is_module_enum *module;
	struct fimc_is_module_4ec *module_4ec;

	BUG_ON(!subdev);

	module = (struct fimc_is_module_enum *)v4l2_get_subdevdata(subdev);
	if (unlikely(!module)) {
		err("module is NULL");
		return -EINVAL;
	}

	module_4ec = module->private_data;
	if (unlikely(!module_4ec)) {
		err("module_4ec is NULL");
		return -EINVAL;
	}

	if (module_4ec->af_status != AF_START) {
		/* To do */
		/* restore focus mode */
		module_4ec->af_status = AF_NONE;
		module_4ec->af_result = AF_RESULT_CANCELLED;
		return 0;
	}

	/* Stop AF */
	module_4ec->af_status = AF_CANCEL;
	module_4ec->af_result = AF_RESULT_CANCELLED;

	/* AE / AWB Unock */
	ret = sensor_4ec_ae_awb_lock(subdev, AE_UNLOCK, AWB_UNLOCK);
	if (ret) {
		err("%s: AE / AWB Unock failed\n", __func__);
		goto p_err;
	}

	ret = sensor_4ec_apply_set(subdev, &regs_set.single_af_off_1);
	if (ret) {
		err("%s: write cmd failed\n", __func__);
		goto p_err;
	}

	/* wait until the other thread has completed
	 * aborting the auto focus and restored state
	 */
	cam_info("%s: wait AF cancel done start\n", __func__);
	wait_for_completion(&module_4ec->af_complete);
	cam_info("%s: wait AF cancel done finished\n", __func__);

	cam_info("%s: AF stop.\n", __func__);

p_err:
	return ret;
}

static int sensor_4ec_auto_focus_proc(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct fimc_is_module_enum *module;
	struct fimc_is_module_4ec *module_4ec;
	u16 read_value;
	int count = 0;

	BUG_ON(!subdev);

	module = (struct fimc_is_module_enum *)v4l2_get_subdevdata(subdev);
	if (unlikely(!module)) {
		err("module is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	module_4ec = module->private_data;
	if (unlikely(!module_4ec)) {
		err("module_4ec is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	/* Get 1st AF result*/
	msleep(NORMAL_FRAME_DELAY_MS * 2);

	for (count = 0; count < FIRST_AF_SEARCH_COUNT; count++) {
		if (module_4ec->af_status == AF_CANCEL) {
			cam_info("%s: AF is cancelled while doing\n", __FUNCTION__);
			module_4ec->af_result = AF_RESULT_FAILED;
			goto check_flash;
		}
		sensor_4ec_read_reg(subdev, &regs_set.get_1st_af_search_status, &read_value, 1);
		cam_dbg("%s: 1st i2c_read --- read_value == 0x%x\n",
			__FUNCTION__, read_value);

		/* check for success and failure cases.  0x1 is
		 * auto focus still in progress.  0x2 is success.
		 * 0x0,0x3,0x4,0x6 are all failures cases
		 * 0x8 is to receive new AF command during AF execution
		 */
		if ((read_value != 0x01) && (read_value != 0x08))
			break;
		msleep(50);
	}

	if ((count >= FIRST_AF_SEARCH_COUNT) || (read_value != 0x02)) {
		cam_info("%s: 1st scan timed out or failed, read_value=%d\n",
			__FUNCTION__, read_value);

		/* we need a time to move the lens to default after af is failed*/
		msleep(350);

		module_4ec->af_result  = AF_RESULT_FAILED;
		goto check_flash;
	}

	cam_info("%s: 2nd AF search\n", __FUNCTION__);

	/* delay 1 frame time before checking for 2nd AF completion */
	msleep(NORMAL_FRAME_DELAY_MS);

	for (count = 0; count < SECOND_AF_SEARCH_COUNT; count++) {
		if (module_4ec->af_status == AF_CANCEL) {
			cam_info("%s: AF is cancelled while doing\n", __FUNCTION__);
			module_4ec->af_result  = AF_RESULT_FAILED;
			goto check_flash;
		}
		sensor_4ec_read_reg(subdev, &regs_set.get_2nd_af_search_status, &read_value, 1);
		cam_dbg("%s: 2nd i2c_read --- read_value == 0x%x\n",
			__FUNCTION__, read_value);

		/* Check result value. 0x0 means finished. */
		if (read_value == 0x0)
			break;

		msleep(50);
	}

	if (count >= SECOND_AF_SEARCH_COUNT) {
		cam_info("%s: 2nd scan timed out\n", __FUNCTION__);
		module_4ec->af_result  = AF_RESULT_FAILED;
		goto check_flash;
	}

	cam_info("%s: AF is success\n", __FUNCTION__);
	module_4ec->af_result  = AF_RESULT_SUCCESS;

check_flash:

	if (module_4ec->flash_status == FLASH_STATUS_PRE_ON) {
		/* Delay 1 frame time before turning off preflash
		* to prevent black screen of preview on capturing image.
		 */
		msleep(NORMAL_FRAME_DELAY_MS);
		ret = sensor_4ec_pre_flash_stop(subdev);
		if (ret) {
			err("%s: pre flash stop failed\n", __func__);
		}
	} else {
		/* AE / AWB Unlock */
		if (module_4ec->user_ae_lock == AE_UNLOCK) {
			ret = sensor_4ec_ae_lock(subdev, AE_UNLOCK);
			if (ret) {
				err("%s: AE unlock failed\n", __func__);
			}
		}

		if (module_4ec->user_awb_lock == AWB_UNLOCK) {
			ret = sensor_4ec_awb_lock(subdev, AWB_UNLOCK);
			if (ret) {
				err("%s: AWB unlock failed\n", __func__);
			}
		}
	}

	cam_info("%s: single AF finished\n", __func__);

	module_4ec->af_status = AF_NONE;
	complete(&module_4ec->af_complete);

p_err:
	return ret;
}

static int sensor_4ec_auto_focus_softlanding(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct fimc_is_module_enum *module;
	struct fimc_is_module_4ec *module_4ec;
	u32 frame_delay, delay = 0;

	BUG_ON(!subdev);

	module = (struct fimc_is_module_enum *)v4l2_get_subdevdata(subdev);
	if (unlikely(!module)) {
		err("module is NULL");
		return -EINVAL;
	}

	module_4ec = module->private_data;
	if (unlikely(!module_4ec)) {
		err("module_4ec is NULL");
		return -EINVAL;
	}

	/* Do soft landing */
	ret = sensor_4ec_apply_set(subdev, &regs_set.af_normal_mode_1);
	if (ret) {
		err("%s: write cmd failed\n", __func__);
		goto p_err;
	}

	ret = sensor_4ec_apply_set(subdev, &regs_set.af_normal_mode_2);
	if (ret) {
		err("%s: write cmd failed\n", __func__);
		goto p_err;
	}

	frame_delay = sensor_4ec_get_frame_duration(subdev);

	if (module_4ec->focus_mode == V4L2_FOCUS_MODE_AUTO) {
		delay = CONST_SOFTLANDING_DELAY_AUTO_MS + frame_delay;
	} else if (module_4ec->focus_mode == V4L2_FOCUS_MODE_MACRO) {
		delay = CONST_SOFTLANDING_DELAY_MACRO_MS + frame_delay * 3;
	}

	if (delay > MAX_SOFTLANDING_DELAY_MS)
		delay = MAX_SOFTLANDING_DELAY_MS;

	cam_info("%s: softlanding delay = %d ms\n", __func__, delay);

	/* Delay for moving lens */
	msleep(delay);

p_err:
	return ret;
}

static void  sensor_4ec_set_focus_work(struct work_struct *work)
{
	struct fimc_is_module_4ec *module_4ec = container_of(work, struct fimc_is_module_4ec, set_focus_mode_work);

	BUG_ON(!module_4ec);

	sensor_4ec_s_focus_mode(module_4ec->subdev, module_4ec->focus_mode);
}

static void  sensor_4ec_af_worker(struct work_struct *work)
{
	struct fimc_is_module_4ec *module_4ec = container_of(work, struct fimc_is_module_4ec, af_work);

	BUG_ON(!module_4ec);

	sensor_4ec_auto_focus_start(module_4ec->subdev);
}

static void  sensor_4ec_af_stop_worker(struct work_struct *work)
{
	struct fimc_is_module_4ec *module_4ec = container_of(work, struct fimc_is_module_4ec, af_stop_work);

	BUG_ON(!module_4ec);

	sensor_4ec_auto_focus_stop(module_4ec->subdev);
}

static int sensor_4ec_verify_window(struct v4l2_subdev *subdev)
{
	int err = 0;
	u16 outter_window_width = 0, outter_window_height = 0;
	u16 inner_window_width= 0, inner_window_height = 0;
	u16 read_value[2] = {0, };
	struct fimc_is_module_enum *module;
	struct fimc_is_module_4ec *module_4ec;

	BUG_ON(!subdev);

	module = (struct fimc_is_module_enum *)v4l2_get_subdevdata(subdev);
	module_4ec = module->private_data;

	/* Set first widnow x width, y height */
	err |= sensor_4ec_read_addr(subdev, 0x0298, read_value, 2);
	outter_window_width = read_value[0];
	outter_window_height = read_value[1];

	memset(read_value, 0, sizeof(read_value));

	/* Set second widnow x width, y height */
	err |= sensor_4ec_read_addr(subdev, 0x02A0, read_value, 2);
	inner_window_width = read_value[0];
	inner_window_height = read_value[1];

	if ((outter_window_width != FIRST_WINSIZE_X)
		|| (outter_window_height != FIRST_WINSIZE_Y)
		|| (inner_window_width != SCND_WINSIZE_X)
		|| (inner_window_height != SCND_WINSIZE_Y)) {
		cam_err("verify_window: error, invalid window size"
			" outter_window(0x%X, 0x%X) inner_window(0x%X, 0x%X)\n",
			outter_window_width, outter_window_height,
			inner_window_width, inner_window_height);
	} else {
		module_4ec->focus.window_verified = 1;
		cam_info("verify_sindow: outter(0x%X, 0x%X) inner(0x%X, 0x%X)\n",
			outter_window_width, outter_window_height,
			inner_window_width, inner_window_height);
	}

	return err;
}

static int sensor_4ec_set_af_window(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct i2c_client *client = to_client(subdev);
	struct fimc_is_module_enum *module;
	struct fimc_is_module_4ec *module_4ec;

	struct s5k4ecgx_rect inner_window, outter_window;
	struct s5k4ecgx_rect first_window, second_window;

	s32 mapped_x = 0, mapped_y = 0;
	u32 preview_width = 0, preview_height = 0;

	u32 inner_half_width = 0, inner_half_height = 0;
	u32 outter_half_width = 0, outter_half_height = 0;

	BUG_ON(!subdev);

	module = (struct fimc_is_module_enum *)v4l2_get_subdevdata(subdev);
	module_4ec = module->private_data;

	inner_window.x = 0;
	inner_window.y = 0;
	inner_window.width = 0;
	inner_window.height = 0;

	outter_window.x = 0;
	outter_window.y = 0;
	outter_window.width = 0;
	outter_window.height = 0;

	first_window.x = 0;
	first_window.y = 0;
	first_window.width = 0;
	first_window.height = 0;

	second_window.x = 0;
	second_window.y = 0;
	second_window.width = 0;
	second_window.height = 0;

	mapped_x = module_4ec->focus.pos_x;
	mapped_y = module_4ec->focus.pos_y;

	preview_width = module_4ec->preview.width;
	preview_height = module_4ec->preview.height;
	if (unlikely(preview_width <= 0 || preview_height <= 0)) {
		cam_warn("invalid preview_width(%d) or preview_height(%d)\n", preview_width, preview_height);
		goto p_err;
	}

	cam_info("(%s)E\n", __func__);

	mutex_lock(&module_4ec->af_lock);

	/* Verify 1st, 2nd widnwo size */
	if (!module_4ec->focus.window_verified)
		sensor_4ec_verify_window(subdev);

	inner_window.width = SCND_WINSIZE_X * preview_width / 1024;
	inner_window.height = SCND_WINSIZE_Y * preview_height / 1024;
	outter_window.width = FIRST_WINSIZE_X * preview_width / 1024;
	outter_window.height = FIRST_WINSIZE_Y * preview_height / 1024;
	inner_half_width = inner_window.width / 2;
	inner_half_height = inner_window.height / 2;
	outter_half_width = outter_window.width / 2;
	outter_half_height = outter_window.height / 2;

	cam_info("Preview width=%d, height=%d\n", preview_width, preview_height);
	cam_info("inner_window_width=%d, inner_window_height=%d, " \
		"outter_window_width=%d, outter_window_height=%d\n ",
		inner_window.width, inner_window.height,
		outter_window.width, outter_window.height);

	/* Get X */
	if (mapped_x <= inner_half_width) {
		inner_window.x = outter_window.x = 0;
		cam_info("inner & outter window over sensor left."
				"in_x=%d, out=%d\n", inner_window.x, outter_window.x);
	} else if (mapped_x <= outter_half_width) {
		inner_window.x = mapped_x - inner_half_width;
		outter_window.x = 0;
		cam_info("outter window over sensor left.in_x=%d, out_x=%d\n",
			inner_window.x, outter_window.x);
	} else if (mapped_x >= ((preview_width - 1) - inner_half_width)) {
		inner_window.x = (preview_width - 1) - inner_window.width;
		outter_window.x = (preview_width - 1) - outter_window.width;
		cam_info("inner & outter window over sensor right." \
			"in_x=%d, out_x=%d\n", inner_window.x, outter_window.x);
	} else if (mapped_x >= ((preview_width - 1) - outter_half_width)) {
		inner_window.x = mapped_x - inner_half_width;
		outter_window.x = (preview_width - 1) - outter_window.width;
		cam_info("outter window over sensor right. in_x=%d, out_x=%d\n",
			inner_window.x, outter_window.x);
	} else {
		inner_window.x = mapped_x - inner_half_width;
		outter_window.x = mapped_x - outter_half_width;
		cam_info("inner & outter window within snesor area." \
			"in_x=%d, out_x=%d\n", inner_window.x, outter_window.x);
	}

	/* Get Y */
	if (mapped_y <= inner_half_height) {
		inner_window.y = outter_window.y = 0;
		cam_info("inner &outter window over sensor top." \
			"in_y=%d, out_y=%d\n", inner_window.y, outter_window.y);
	} else if (mapped_y <= outter_half_height) {
		inner_window.y = mapped_y - inner_half_height;
		outter_window.y = 0;
		cam_info("outter window over sensor top. in_y=%d, out_y=%d\n",
			inner_window.y, outter_window.y);
	} else if (mapped_y >= ((preview_height - 1) - inner_half_height)) {
		inner_window.y = (preview_height - 1) - inner_window.height;
		outter_window.y = (preview_height - 1) - outter_window.height;
		cam_info("inner & outter window over sensor bottom." \
			"in_y=%d, out_y=%d\n", inner_window.y, outter_window.y);
	} else if (mapped_y >= ((preview_height - 1) - outter_half_height)) {
		inner_window.y = mapped_y - inner_half_height;
		outter_window.y = (preview_height - 1) - outter_window.height;
		cam_info("outter window over sensor bottom. in_y=%d, out_y=%d\n",
			inner_window.y, outter_window.y);
	} else {
		inner_window.y = mapped_y - inner_half_height;
		outter_window.y = mapped_y - outter_half_height;
		cam_info("inner & outter window within sensor area." \
			"in_y=%d, out_y=%d\n", inner_window.y, outter_window.y);
	}

	cam_info("==> inner_window top=(%d,%d), bottom=(%d,%d)\n",
		inner_window.x, inner_window.y,
		inner_window.x + inner_window.width,
		inner_window.y + inner_window.height);
	cam_info("==> outter window top=(%d,%d), bottom(%d,%d)\n",
		outter_window.x, outter_window.y,
		outter_window.x + outter_window.width,
		outter_window.y + outter_window.height);

	second_window.x = inner_window.x * 1024 / preview_width;
	second_window.y = inner_window.y * 1024 / preview_height;
	first_window.x = outter_window.x * 1024 / preview_width;
	first_window.y = outter_window.y * 1024 / preview_height;

	cam_info("=> second_window top=(%d,%d)\n",
		second_window.x, second_window.y);
	cam_info("=> first_window top=(%d,%d)\n",
		first_window.x, first_window.y);

	mutex_lock(&module_4ec->i2c_lock);

	/* restore write mode */
	fimc_is_sensor_write16(client, 0x0028, 0x7000);

	/* Set first widnow x, y */
	fimc_is_sensor_write16(client, 0x002A, 0x0294);
	fimc_is_sensor_write16(client, 0x0F12, (u16)(first_window.x));

	fimc_is_sensor_write16(client, 0x002A, 0x0296);
	fimc_is_sensor_write16(client, 0x0F12, (u16)(first_window.y));

	/* Set second widnow x, y */
	fimc_is_sensor_write16(client, 0x002A, 0x029C);
	fimc_is_sensor_write16(client, 0x0F12, (u16)(second_window.x));

	fimc_is_sensor_write16(client, 0x002A, 0x029E);
	fimc_is_sensor_write16(client, 0x0F12, (u16)(second_window.y));

	/* Update AF window */
	fimc_is_sensor_write16(client, 0x002A, 0x02A4);
	fimc_is_sensor_write16(client, 0x0F12, 0x0001);

	mutex_unlock(&module_4ec->i2c_lock);

	mutex_unlock(&module_4ec->af_lock);

p_err :
	return ret;
}

static int sensor_4ec_set_touch_af(struct v4l2_subdev *subdev, s32 val)
{
	int ret = 0;
	struct fimc_is_module_enum *module;
	struct fimc_is_module_4ec *module_4ec;

	BUG_ON(!subdev);

	module = (struct fimc_is_module_enum *)v4l2_get_subdevdata(subdev);
	if (unlikely(!module)) {
		err("module is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	module_4ec = module->private_data;
	if (unlikely(!module_4ec)) {
		err("module_4ec is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	if (val) {
		if (mutex_is_locked(&module_4ec->af_lock)) {
			cam_warn("%s: AF is still operating!\n", __func__);
			return 0;
		}
		ret = sensor_4ec_set_af_window(subdev);
	} else {
		cam_info("set_touch_af: invalid value %d\n", val);
	}

p_err:
	return ret;
}

static int sensor_4ec_s_capture_mode(struct v4l2_subdev *subdev, int value)
{
	int ret = 0;
	struct fimc_is_module_enum *module;
	struct fimc_is_module_4ec *module_4ec;

	BUG_ON(!subdev);

	cam_info("%s: %d\n", __func__, value);

	module = (struct fimc_is_module_enum *)v4l2_get_subdevdata(subdev);
	if (unlikely(!module)) {
		err("module is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	module_4ec = module->private_data;
	if (unlikely(!module_4ec)) {
		err("module_4ec is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	if (value)
		module_4ec->mode = S5K4ECGX_OPRMODE_IMAGE;
	else
		module_4ec->mode = S5K4ECGX_OPRMODE_VIDEO;

p_err:
	return ret;
}

static int sensor_4ec_s_sensor_mode(struct v4l2_subdev *subdev, int value)
{
	int ret = 0;
	struct fimc_is_module_enum *module;
	struct fimc_is_module_4ec *module_4ec;

	BUG_ON(!subdev);

	cam_info("%s: %d\n", __func__, value);

	module = (struct fimc_is_module_enum *)v4l2_get_subdevdata(subdev);
	if (unlikely(!module)) {
		err("module is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	module_4ec = module->private_data;
	if (unlikely(!module_4ec)) {
		err("module_4ec is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	switch (value) {
	case SENSOR_CAMERA:
		cam_info("%s: SENSOR_CAMERA mode\n", __func__);

		if (module_4ec->sensor_mode == SENSOR_MOVIE) {
			ret = sensor_4ec_apply_set(subdev, &regs_set.camcorder_disable);
			if (ret) {
				err("%s: write cmd failed\n", __func__);
				goto p_err;
			}
		}

		ret = sensor_4ec_apply_set(subdev, &regs_set.preview_return);
		if (ret) {
			err("%s: write cmd failed\n", __func__);
			goto p_err;
		}

		module_4ec->sensor_mode = SENSOR_CAMERA;
		break;
	case SENSOR_MOVIE:
		cam_info("%s: SENSOR_MOVIE mode\n", __func__);

		ret = sensor_4ec_apply_set(subdev, &regs_set.camcorder);
		if (ret) {
			err("%s: write cmd failed\n", __func__);
			goto p_err;
		}

		module_4ec->sensor_mode = SENSOR_MOVIE;
		break;
	default:
		err("invalid mode %d\n", value);
		ret = -EINVAL;
		break;
	}

p_err:
	return ret;
}

/*
 * @ brief
 * frame duration time
 * @ unit
 * nano second
 * @ remarks
 */
static int sensor_4ec_s_duration(struct v4l2_subdev *subdev, u64 duration)
{
	int ret = 0;
	u32 framerate;
	struct fimc_is_module_enum *module;
	struct fimc_is_module_4ec *module_4ec;

	BUG_ON(!subdev);

	cam_info("%s: %lld\n", __func__, duration);

	module = (struct fimc_is_module_enum *)v4l2_get_subdevdata(subdev);
	if (unlikely(!module)) {
		err("module is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	module_4ec = module->private_data;

	framerate = duration;
	if (framerate > FRAME_RATE_MAX) {
		err("framerate is invalid(%d)", framerate);
		ret = -EINVAL;
		goto p_err;
	}

#if 0
	info("sene mode set (%p, %d)\n", &regs_set.scene_mode[0], regs_set.scene_mode[0].array_size);
	sensor_4ec_apply_set(subdev, &regs_set.scene_mode[1]);
#endif

	cam_info("%s: frame rate set\n", __func__);
	module_4ec->fps = framerate;
	ret = sensor_4ec_apply_set(subdev, &regs_set.fps[framerate]);
	if (ret) {
		err("%s: write cmd failed\n", __func__);
	}

p_err:
	return ret;
}

static int sensor_4ec_s_power_off(struct v4l2_subdev *subdev, u32 val)
{
	int ret = 0;
	struct fimc_is_module_enum *module;
	struct fimc_is_module_4ec *module_4ec;

	BUG_ON(!subdev);

	module = (struct fimc_is_module_enum *)v4l2_get_subdevdata(subdev);
	if (unlikely(!module)) {
		err("module is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	module_4ec = module->private_data;
	if (unlikely(!module_4ec)) {
		err("module_4ec is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	flush_work(&module_4ec->set_focus_mode_work);
	flush_work(&module_4ec->af_work);
	flush_work(&module_4ec->af_stop_work);

	cam_info("%s: %d\n", __func__, val);

	ret = sensor_4ec_auto_focus_softlanding(subdev);
	if (ret) {
		err("%s: write cmd failed\n", __func__);
		goto p_err;
	}

#if defined(CONFIG_FLED_SM5703)
	if (flash_control_ready == true) {
		sm5703_led_mode_ctrl(SM5703_FLED_MODE_EXT_GPIO_DISABLE);
		flash_control_ready = false;
	}
#elif defined(CONFIG_LEDS_S2MU005)
    s2mu005_led_mode_ctrl(S2MU005_FLED_MODE_OFF);
#endif

p_err:
	return ret;
}

static int sensor_4ec_s_ctrl(struct v4l2_subdev *subdev, struct v4l2_control *ctrl)
{
	int ret = 0;
	struct fimc_is_module_enum *module;
	struct fimc_is_module_4ec *module_4ec = NULL;

	module = (struct fimc_is_module_enum *)v4l2_get_subdevdata(subdev);
	if (unlikely(!module)) {
		err("module is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	module_4ec = module->private_data;
	if (unlikely(!module_4ec)) {
		err("module_4ec is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	mutex_lock(&module_4ec->ctrl_lock);

	switch (ctrl->id) {
	case V4L2_CID_CAMERA_INIT:
	case V4L2_CID_CAMERA_SET_SNAPSHOT_SIZE:
	case V4L2_CID_CAMERA_SET_POSTVIEW_SIZE:
	case V4L2_CID_JPEG_QUALITY:
	case V4L2_CID_EXIF_F_NUMBER:
		break;
	case V4L2_CID_CAM_FRAME_RATE:
		ret = sensor_4ec_s_frame_rate(subdev, ctrl->value);
		if (ret < 0) {
			err("%s: sensor_4ec_s_frame_rate fail.\n", __FUNCTION__);
		}
		break;
	case V4L2_CID_CAMERA_ANTI_BANDING:
#ifndef USE_ANTIBANDING_50HZ_ONLY
		ret = sensor_4ec_s_anti_banding(subdev, ctrl->value);
		if (ret < 0) {
			err("%s: sensor_4ec_s_anti_banding fail.\n", __FUNCTION__);
		}
#endif
		break;
	case V4L2_CID_CAMERA_TOUCH_AF_START_STOP:
		cam_dbg("V4L2_CID_CAMERA_TOUCH_AF_START_STOP E \n");
		ret = sensor_4ec_set_touch_af(subdev, 1);
		cam_dbg("V4L2_CID_CAMERA_TOUCH_AF_START_STOP X \n");
		if (ret < 0) {
			err("%s: sensor_4ec_set_touch_af fail.\n", __FUNCTION__);
		}
		break;
	case V4L2_CID_CAMERA_SCENE_MODE:
	case V4L2_CID_SCENEMODE:
		ret = sensor_4ec_s_scene_mode(subdev, ctrl->value);
		if (ret < 0) {
			err("%s: sensor_4ec_s_scene_mode fail.\n", __FUNCTION__);
		}
		break;
	case V4L2_CID_CAMERA_FOCUS_MODE:
		if (sensor_4ec_check_focus_mode(subdev, ctrl->value)) {
			int busy = work_busy(&module_4ec->set_focus_mode_work);

			module_4ec->focus_mode = (ctrl->value & ~V4L2_FOCUS_MODE_DEFAULT);
			if(busy) {
				cam_info("%s: set_focus_mode_work is busy(%d)\n", __FUNCTION__, busy);
				if(!(ctrl->value & V4L2_FOCUS_MODE_DEFAULT)) {
					flush_work(&module_4ec->set_focus_mode_work);
					ret = (schedule_work(&module_4ec->set_focus_mode_work) == 1) ? 0 : -EINVAL;
					if (ret < 0) {
						err("%s: set focus mode fail.\n", __FUNCTION__);
					}
				}
			} else {
				ret = (schedule_work(&module_4ec->set_focus_mode_work) == 1) ? 0 : -EINVAL;
				if (ret < 0) {
					err("%s: set focus mode fail.\n", __FUNCTION__);
				}
			}
		} else {
			cam_info("%s: same focus mode( %d ). skip to set focus mode.\n", __FUNCTION__, ctrl->value);
		}
		break;
	case V4L2_CID_WHITE_BALANCE_PRESET:
		ret = sensor_4ec_s_white_balance(subdev, ctrl->value);
		if (ret < 0) {
			err("%s: sensor_4ec_s_white_balance fail.\n", __FUNCTION__);
		}
		break;
	case V4L2_CID_CAMERA_EFFECT:
	case V4L2_CID_IMAGE_EFFECT:
		ret = sensor_4ec_s_effect(subdev, ctrl->value);
		if (ret < 0) {
			err("%s: sensor_4ec_s_effect fail.\n", __FUNCTION__);
		}
		break;
	case V4L2_CID_CAMERA_ISO:
	case V4L2_CID_CAM_ISO:
		ret = sensor_4ec_s_iso(subdev, ctrl->value);
		if (ret < 0) {
			err("%s: sensor_4ec_s_iso fail.\n", __FUNCTION__);
		}
		break;
	case V4L2_CID_CAMERA_CONTRAST:
		ret = sensor_4ec_s_contrast(subdev, ctrl->value);
		if (ret < 0) {
			err("%s: sensor_4ec_s_contrast fail.\n", __FUNCTION__);
		}
		break;
	case V4L2_CID_CAMERA_SATURATION:
		ret = sensor_4ec_s_saturation(subdev, ctrl->value);
		if (ret < 0) {
			err("%s: sensor_4ec_s_saturation fail.\n", __FUNCTION__);
		}
		break;
	case V4L2_CID_CAMERA_SHARPNESS:
		ret = sensor_4ec_s_sharpness(subdev, ctrl->value);
		if (ret < 0) {
			err("%s: sensor_4ec_s_sharpness fail.\n", __FUNCTION__);
		}
		break;
	case V4L2_CID_CAMERA_BRIGHTNESS:
	case V4L2_CID_CAM_BRIGHTNESS:
		ret = sensor_4ec_s_brightness(subdev, ctrl->value);
		if (ret < 0) {
			err("%s: sensor_4ec_s_brightness fail.\n", __FUNCTION__);
		}
		break;
	case V4L2_CID_CAMERA_METERING:
	case V4L2_CID_CAM_METERING:
		ret = sensor_4ec_s_metering(subdev, ctrl->value);
		if (ret < 0) {
			err("%s: sensor_4ec_s_metering fail.\n", __FUNCTION__);
		}
		break;
	case V4L2_CID_CAMERA_POWER_OFF:
		sensor_4ec_s_power_off(subdev, ctrl->value);
		break;
	case V4L2_CID_CAM_SINGLE_AUTO_FOCUS:
		module_4ec->af_result = AF_RESULT_DOING;
		ret = (schedule_work(&module_4ec->af_work) == 1) ? 0 : -EINVAL;
		if (ret < 0) {
			err("%s: Start auto focus fail.\n", __FUNCTION__);
		}
		break;
	case V4L2_CID_CAMERA_CANCEL_AUTO_FOCUS:
		ret = (schedule_work(&module_4ec->af_stop_work) == 1) ? 0 : -EINVAL;
		if (ret < 0) {
			err("%s: Stop auto focus fail.\n", __FUNCTION__);
		}
		break;
	case V4L2_CID_CAMERA_OBJECT_POSITION_X:
		module_4ec->focus.pos_x = ctrl->value;
		break;
	case V4L2_CID_CAMERA_OBJECT_POSITION_Y:
		module_4ec->focus.pos_y = ctrl->value;
		break;
	case V4L2_CID_CAMERA_AE_LOCK_UNLOCK:
		module_4ec->user_ae_lock = ctrl->value;
		ret = sensor_4ec_ae_lock(subdev, ctrl->value);
		if (ret < 0) {
			err("%s: sensor_4ec_s_ae_lock_unlock fail.\n", __FUNCTION__);
		}
		break;
	case V4L2_CID_CAMERA_AWB_LOCK_UNLOCK:
		module_4ec->user_awb_lock = ctrl->value;
		ret = sensor_4ec_awb_lock(subdev, ctrl->value);
		if (ret < 0) {
			err("%s: sensor_4ec_s_awb_lock_unlock fail.\n", __FUNCTION__);
		}
		break;
	case V4L2_CID_CAMERA_FACE_DETECTION:
	case V4L2_CID_CAMERA_WDR:
	case V4L2_CID_CAMERA_AUTO_FOCUS_RESULT:
	case V4L2_CID_CAMERA_JPEG_QUALITY:
	case V4L2_CID_CAMERA_AEAWB_LOCK_UNLOCK:
	case V4L2_CID_CAMERA_CAF_START_STOP:
	case V4L2_CID_CAMERA_ZOOM:
		err("Not supported ioctl(0x%08X) is requested", ctrl->id);
		break;
	case V4L2_CID_CAPTURE:
	case V4L2_CID_CAMERA_CAPTURE:
		ret = sensor_4ec_s_capture_mode(subdev, ctrl->value);
		if (ret) {
			err("sensor_4ec_s_capture_mode is fail(%d)", ret);
			goto p_err;
		}
		break;
	case V4L2_CID_CAM_FLASH_MODE:
	case V4L2_CID_CAMERA_FLASH_MODE:
		ret = sensor_4ec_s_flash(subdev, ctrl->value);
		if (ret) {
			err("sensor_4ec_s_flash is fail(%d)", ret);
			goto p_err;
		}
		break;
	case V4L2_CID_CAMERA_SENSOR_MODE:
		ret = sensor_4ec_s_sensor_mode(subdev, ctrl->value);
		if (ret) {
			err("sensor_4ec_s_sensor_mode is fail(%d)", ret);
			goto p_err;
		}
		break;
	default:
		err("invalid ioctl(0x%08X) is requested", ctrl->id);
		//ret = -EINVAL;
		break;
	}

p_err:
	mutex_unlock(&module_4ec->ctrl_lock);

	return ret;
}

static int sensor_4ec_g_ctrl(struct v4l2_subdev *subdev, struct v4l2_control *ctrl)
{
	int ret = 0;
	struct fimc_is_module_enum *module;
	struct fimc_is_module_4ec *module_4ec = NULL;

	module = (struct fimc_is_module_enum *)v4l2_get_subdevdata(subdev);
	if (unlikely(!module)) {
		err("module is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	module_4ec = module->private_data;
	if (unlikely(!module_4ec)) {
		err("module_4ec is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	mutex_lock(&module_4ec->ctrl_lock);

	switch (ctrl->id) {
	case V4L2_CID_EXIF_EXPOSURE_TIME_DEN:
		ctrl->value = module_4ec->exif.exp_time_den;
		cam_dbg("V4L2_CID_CAMERA_EXIF_EXPTIME :%d\n", ctrl->value);
		break;
	case V4L2_CID_EXIF_EXPOSURE_TIME_NUM:
		ctrl->value = 1; // module_4ec->exif.exp_time_num
		cam_dbg("V4L2_CID_EXIF_EXPOSURE_TIME_NUM :%d\n", ctrl->value);
		break;
	case V4L2_CID_EXIF_SHUTTER_SPEED_NUM:
		/* To Do : Implement */
		ctrl->value = 1;
		cam_dbg("V4L2_CID_EXIF_SHUTTER_SPEED_NUM :%d\n", ctrl->value);
		break;
	case V4L2_CID_CAMERA_EXIF_FLASH:
		ctrl->value = module_4ec->exif.flash;
		cam_dbg("V4L2_CID_CAMERA_EXIF_FLASH :%d\n", ctrl->value);
		break;
	case V4L2_CID_CAMERA_EXIF_ISO:
		ctrl->value = module_4ec->exif.iso;
		cam_dbg("V4L2_CID_CAMERA_EXIF_ISO :%d\n", ctrl->value);
		break;
	case V4L2_CID_CAMERA_EXIF_TV:
		ctrl->value = 0;
		break;
	case V4L2_CID_CAMERA_EXIF_BV:
		ctrl->value = 0;
		break;
	case V4L2_CID_CAMERA_AUTO_FOCUS_DONE:
		ctrl->value = module_4ec->af_result ;
		cam_dbg("%s: af_result = %d\n", __func__, ctrl->value);
		break;
	default:
		err("invalid ioctl(0x%08X) is requested", ctrl->id);
		ret = -EINVAL;
		break;
	}

p_err:
	mutex_unlock(&module_4ec->ctrl_lock);

	return ret;
}

static const struct v4l2_subdev_core_ops core_ops = {
	.init		= sensor_4ec_init,
	.s_ctrl		= sensor_4ec_s_ctrl,
	.g_ctrl		= sensor_4ec_g_ctrl
};

static const struct v4l2_subdev_video_ops video_ops = {
	.s_stream = sensor_4ec_s_stream,
	.s_parm = sensor_4ec_s_param,
	.s_mbus_fmt = sensor_4ec_s_format
};

static const struct v4l2_subdev_ops subdev_ops = {
	.core = &core_ops,
	.video = &video_ops
};

struct fimc_is_sensor_ops module_4ec_ops = {
	.stream_on	= sensor_4ec_stream_on,
	.stream_off	= sensor_4ec_stream_off,
	.s_duration	= sensor_4ec_s_duration,
};

#ifdef CONFIG_OF
static int sensor_4ec_power_setpin(struct i2c_client *client,
		struct exynos_platform_fimc_is_module *pdata)
{
	struct device_node *dnode;
	int gpio_reset = 0;
	int gpio_standby = 0;
	int gpio_core_en = 0;
	int gpio_none = 0;
	int gpio_mclk = 0;
#if defined (VDD_CAM_SENSOR_A2P8_GPIO_CONTROL)
	int gpio_cam_avdd_en = 0;
#endif
#if defined (VDDAF_2P8V_CAM_GPIO_CONTROL)
	int gpio_cam_af_en = 0;
#endif
//	u8 id;

	BUG_ON(!client);
	BUG_ON(!client->dev.of_node);

	dnode = client->dev.of_node;

	cam_info("%s E\n", __func__);

	gpio_reset = of_get_named_gpio(dnode, "gpio_reset", 0);
	if (!gpio_is_valid(gpio_reset)) {
		err("%s failed to get gpio_reset\n",__func__);
		return -EINVAL;
	} else {
		gpio_request_one(gpio_reset, GPIOF_OUT_INIT_LOW, "CAM_GPIO_OUTPUT_LOW");
		gpio_free(gpio_reset);
	}

	gpio_standby = of_get_named_gpio(dnode, "gpio_standby", 0);
	if (!gpio_is_valid(gpio_standby)) {
		err("%s failed to get gpio_standby\n",__func__);
		return -EINVAL;
	} else {
		gpio_request_one(gpio_standby, GPIOF_OUT_INIT_LOW, "CAM_GPIO_OUTPUT_LOW");
		gpio_free(gpio_standby);
	}

	gpio_core_en = of_get_named_gpio(dnode, "gpio_core_en", 0);
	if (!gpio_is_valid(gpio_core_en)) {
		err("%s failed to get gpio_core_en\n",__func__);
		return -EINVAL;
	} else {
		gpio_request_one(gpio_core_en, GPIOF_OUT_INIT_LOW, "CAM_GPIO_OUTPUT_LOW");
		gpio_free(gpio_core_en);
	}

	gpio_mclk = of_get_named_gpio(dnode, "gpio_mclk", 0);
	if (!gpio_is_valid(gpio_mclk)) {
		cam_err("%s failed to get PIN_RESET\n", __func__);
		return -EINVAL;
	} else {
		gpio_request_one(gpio_mclk, GPIOF_OUT_INIT_LOW, "CAM_MCLK_OUTPUT_LOW");
		gpio_free(gpio_mclk);
	}

#if defined (VDD_CAM_SENSOR_A2P8_GPIO_CONTROL)
	gpio_cam_avdd_en = of_get_named_gpio(dnode, "gpio_cam_avdd_en", 0);
	if (!gpio_is_valid(gpio_cam_avdd_en)) {
		err("%s failed to get gpio_cam_avdd_en\n",__func__);
		return -EINVAL;
	} else {
		gpio_request_one(gpio_cam_avdd_en, GPIOF_OUT_INIT_LOW, "CAM_GPIO_OUTPUT_LOW");
		gpio_free(gpio_cam_avdd_en);
	}
#endif

#if defined (VDDAF_2P8V_CAM_GPIO_CONTROL)
	gpio_cam_af_en = of_get_named_gpio(dnode, "gpio_cam_af_en", 0);
	if (!gpio_is_valid(gpio_cam_af_en)) {
		err("%s failed to get gpio_cam_af_en\n",__func__);
		return -EINVAL;
	} else {
		gpio_request_one(gpio_cam_af_en, GPIOF_OUT_INIT_LOW, "CAM_GPIO_OUTPUT_LOW");
		gpio_free(gpio_cam_af_en);
	}
#endif

	SET_PIN_INIT(pdata, SENSOR_SCENARIO_EXTERNAL, GPIO_SCENARIO_ON);
	SET_PIN_INIT(pdata, SENSOR_SCENARIO_EXTERNAL, GPIO_SCENARIO_OFF);

	/* BACK CAMERA - POWER ON */
	SET_PIN(pdata, SENSOR_SCENARIO_EXTERNAL, GPIO_SCENARIO_ON, gpio_reset, NULL, PIN_OUTPUT, 0, 10);
	SET_PIN(pdata, SENSOR_SCENARIO_EXTERNAL, GPIO_SCENARIO_ON, gpio_none, "VDDIO_1.8V_CAM", PIN_REGULATOR, 1, 1);
#if defined (VDD_CAM_SENSOR_A2P8_GPIO_CONTROL)
	SET_PIN(pdata, SENSOR_SCENARIO_EXTERNAL, GPIO_SCENARIO_ON, gpio_cam_avdd_en, NULL, PIN_OUTPUT, 1, 1);
#else
	SET_PIN(pdata, SENSOR_SCENARIO_EXTERNAL, GPIO_SCENARIO_ON, gpio_none, "VDD_CAM_SENSOR_A2P8", PIN_REGULATOR, 1, 1);
#endif
#if defined (VDDAF_2P8V_CAM_GPIO_CONTROL)
	SET_PIN(pdata, SENSOR_SCENARIO_EXTERNAL, GPIO_SCENARIO_ON, gpio_cam_af_en, NULL, PIN_OUTPUT, 1, 2000);
#else
	SET_PIN(pdata, SENSOR_SCENARIO_EXTERNAL, GPIO_SCENARIO_ON, gpio_none, "VDDAF_2.8V_CAM", PIN_REGULATOR, 1, 2000);
#endif
	SET_PIN(pdata, SENSOR_SCENARIO_EXTERNAL, GPIO_SCENARIO_ON, gpio_none, "pin", PIN_FUNCTION, 0, 30);
#if defined (VDDD_1P2_CAM_GPIO_CONTROL)
	SET_PIN(pdata, SENSOR_SCENARIO_EXTERNAL, GPIO_SCENARIO_ON, gpio_core_en, NULL, PIN_OUTPUT, 1, 1000);
#else
	SET_PIN(pdata, SENSOR_SCENARIO_EXTERNAL, GPIO_SCENARIO_ON, gpio_none, "VDDD_1.2V_CAM", PIN_REGULATOR, 1, 1000);
#endif
	SET_PIN(pdata, SENSOR_SCENARIO_EXTERNAL, GPIO_SCENARIO_ON, gpio_standby, NULL, PIN_OUTPUT, 1, 1000);
	SET_PIN(pdata, SENSOR_SCENARIO_EXTERNAL, GPIO_SCENARIO_ON, gpio_reset, NULL, PIN_OUTPUT, 1, 1000);

	/* BACK CAMERA - POWER OFF */
	SET_PIN(pdata, SENSOR_SCENARIO_EXTERNAL, GPIO_SCENARIO_OFF, gpio_reset, NULL, PIN_OUTPUT, 0, 10);
	SET_PIN(pdata, SENSOR_SCENARIO_EXTERNAL, GPIO_SCENARIO_OFF, gpio_none, "pin", PIN_FUNCTION, 1, 0);
	SET_PIN(pdata, SENSOR_SCENARIO_EXTERNAL, GPIO_SCENARIO_OFF, gpio_standby, NULL, PIN_OUTPUT, 0, 1);
	SET_PIN(pdata, SENSOR_SCENARIO_EXTERNAL, GPIO_SCENARIO_OFF, gpio_none, "VDDIO_1.8V_CAM", PIN_REGULATOR, 0, 150);
#if defined (VDD_CAM_SENSOR_A2P8_GPIO_CONTROL)
	SET_PIN(pdata, SENSOR_SCENARIO_EXTERNAL, GPIO_SCENARIO_OFF, gpio_cam_avdd_en, NULL, PIN_OUTPUT, 0, 0);
#else
	SET_PIN(pdata, SENSOR_SCENARIO_EXTERNAL, GPIO_SCENARIO_OFF, gpio_none, "VDD_CAM_SENSOR_A2P8", PIN_REGULATOR, 0, 0);
#endif
#if defined (VDDAF_2P8V_CAM_GPIO_CONTROL)
	SET_PIN(pdata, SENSOR_SCENARIO_EXTERNAL, GPIO_SCENARIO_OFF, gpio_cam_af_en, NULL, PIN_OUTPUT, 0, 0);
#else
	SET_PIN(pdata, SENSOR_SCENARIO_EXTERNAL, GPIO_SCENARIO_OFF, gpio_none, "VDDAF_2.8V_CAM", PIN_REGULATOR, 0, 0);
#endif
#if defined (VDDD_1P2_CAM_GPIO_CONTROL)
	SET_PIN(pdata, SENSOR_SCENARIO_EXTERNAL, GPIO_SCENARIO_OFF, gpio_core_en, NULL, PIN_OUTPUT, 0, 0);
#else
	SET_PIN(pdata, SENSOR_SCENARIO_EXTERNAL, GPIO_SCENARIO_OFF, gpio_none, "VDDD_1.2V_CAM", PIN_REGULATOR, 0, 0);
#endif

	cam_info("%s X\n", __func__);
	return 0;
}
#endif /* CONFIG_OF */

static int sensor_4ec_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	int ret = 0;
	struct fimc_is_core *core;
	struct v4l2_subdev *subdev_module;
	struct fimc_is_module_enum *module;
	struct fimc_is_device_sensor *device;
	struct exynos_platform_fimc_is_module *pdata;
	struct fimc_is_module_4ec *module_4ec;

	BUG_ON(!fimc_is_dev);

	core = (struct fimc_is_core *)dev_get_drvdata(fimc_is_dev);
	if (!core) {
		err("core device is not yet probed");
		return -EPROBE_DEFER;
	}

	device = &core->sensor[SENSOR_S5K4EC_INSTANCE];

	pdata = kzalloc(sizeof(struct exynos_platform_fimc_is_module), GFP_KERNEL);
	if (!pdata) {
		err("%s: no memory for platform data\n", __func__);
		return -ENOMEM;
	}

#ifdef CONFIG_OF
	fimc_is_sensor_module_soc_parse_dt(client, pdata, sensor_4ec_power_setpin);
#endif

	subdev_module = kzalloc(sizeof(struct v4l2_subdev), GFP_KERNEL);
	if (!subdev_module) {
		err("subdev_module is NULL");
		ret = -ENOMEM;
		goto p_err;
	}

	/* S5K4EC */
	module = &device->module_enum[atomic_read(&core->resourcemgr.rsccount_module)];
	atomic_inc(&core->resourcemgr.rsccount_module);
	clear_bit(FIMC_IS_MODULE_GPIO_ON, &module->state);
	module->pdata = pdata;
	module->sensor_id = SENSOR_S5K4EC_NAME;
	module->subdev = subdev_module;
	module->device = SENSOR_S5K4EC_INSTANCE;
	module->ops = &module_4ec_ops;
	module->client = client;
	module->pixel_width = 1920 + 16;
	module->pixel_height = 1080 + 10;
	module->active_width = 1920;
	module->active_height = 1080;
	module->max_framerate = 120;
	module->position = SENSOR_POSITION_REAR;
	module->mode = CSI_MODE_VC_ONLY;
	module->lanes = CSI_DATA_LANES_2;
	module->vcis = ARRAY_SIZE(vci_4ec);
	module->vci = vci_4ec;
	module->sensor_maker = "SLSI";
	module->sensor_vendorid = kzalloc(8 * sizeof(char), GFP_KERNEL);
	if (!module->sensor_vendorid) {
		err("sensor_vendorid alloc failed");
		ret = -ENOMEM;
		goto p_err;
	}
	strcpy(module->sensor_vendorid, "NULL");
	module->sensor_name = kzalloc(10 * sizeof(char), GFP_KERNEL);
	if (!module->sensor_name) {
		err("sensor_name alloc failed");
		ret = -ENOMEM;
		goto p_err;
	}
	strcpy(module->sensor_name, SENSOR_NAME);
	module->setfile_name = "setfile_4ec.bin";
	module->cfgs = ARRAY_SIZE(settle_4ec);
	module->cfg = settle_4ec;
	module->private_data = kzalloc(sizeof(struct fimc_is_module_4ec), GFP_KERNEL);
	if (!module->private_data) {
		err("private_data is NULL");
		ret = -ENOMEM;
		goto p_err;
	}

#ifdef SENSOR_S5K4EC_DRIVING
	v4l2_i2c_subdev_init(subdev_module, client, &subdev_ops);
#else
	v4l2_subdev_init(subdev_module, &subdev_ops);
#endif
	v4l2_set_subdevdata(subdev_module, module);
	v4l2_set_subdev_hostdata(subdev_module, device);
	snprintf(subdev_module->name, V4L2_SUBDEV_NAME_SIZE, "sensor-subdev.%d", module->sensor_id);

	module_4ec = module->private_data;
	module_4ec->subdev = subdev_module;
	mutex_init(&module_4ec->ctrl_lock);
	mutex_init(&module_4ec->af_lock);
	mutex_init(&module_4ec->i2c_lock);
	init_completion(&module_4ec->af_complete);

	INIT_WORK(&module_4ec->set_focus_mode_work, sensor_4ec_set_focus_work);
	INIT_WORK(&module_4ec->af_work, sensor_4ec_af_worker);
	INIT_WORK(&module_4ec->af_stop_work, sensor_4ec_af_stop_worker);

p_err:
	cam_info("%s(%d)\n", __func__, ret);
	return ret;
}

static int sensor_4ec_remove(struct i2c_client *client)
{
	int ret = 0;
	return ret;
}

#ifdef CONFIG_OF
static const struct of_device_id exynos_fimc_is_sensor_4ec_match[] = {
	{
		.compatible = "samsung,exynos5-fimc-is-sensor-4ec",
	},
	{},
};
#endif

static const struct i2c_device_id sensor_4ec_idt[] = {
	{ SENSOR_NAME, 0 },
};

static struct i2c_driver sensor_4ec_driver = {
	.driver = {
		.name	= SENSOR_NAME,
		.owner	= THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = exynos_fimc_is_sensor_4ec_match
#endif
	},
	.probe	= sensor_4ec_probe,
	.remove	= sensor_4ec_remove,
	.id_table = sensor_4ec_idt
};

static int __init sensor_4ec_load(void)
{
        return i2c_add_driver(&sensor_4ec_driver);
}

static void __exit sensor_4ec_unload(void)
{
        i2c_del_driver(&sensor_4ec_driver);
}

module_init(sensor_4ec_load);
module_exit(sensor_4ec_unload);

MODULE_AUTHOR("Gilyeon lim");
MODULE_DESCRIPTION("Sensor 4ec driver");
MODULE_LICENSE("GPL v2");

