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
#include <linux/videodev2_exynos_camera.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/vmalloc.h>
#include <linux/platform_device.h>
#include <linux/firmware.h>
#include <mach/regs-gpio.h>
#include <mach/regs-clock.h>
#include <plat/clock.h>
#include <plat/gpio-cfg.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-device.h>
#include <media/v4l2-subdev.h>
#include <mach/exynos-fimc-is-sensor.h>

#include "../fimc-is-core.h"
#include "../fimc-is-device-sensor.h"
#include "../fimc-is-resourcemgr.h"
#include "../fimc-is-hw.h"
#include "fimc-is-device-m7mu.h"

#include <media/m7mu_platform.h>

#include <linux/time.h>
#include <linux/workqueue.h>


#define SENSOR_NAME "M7MU"

static int boot_irq;
static int status_irq;
static int sound_irq;
static struct mutex m7mu_lock;

#if 0
#define M7MU_BUS_FREQ_LOCK
#define HOLD_LENS_SUPPORT
#define M7MU_LOAD_FW_4MB
#define M7MU_ISP_DEBUG  /* ISP Debug */
#define M7MU_FW_PATH		"/data/RS_M7MU.bin"
#define FW_INFO_PATH		"/data/FW_INFO.bin"
#endif

#define ISP_RESET_SELECT

// for reducing power off time when changing to front camera.
#define ENABLE_FAST_POWER_OFF

/* Read .csv File from ISP directly */
#define READ_CSV_FILE_DIRECT

#define M7MU_FW_PATH		"/sdcard/RS_M7MU.bin"
#define M7MU_FW_MMC_PATH		"/dev/block/mmcblk0p16"
#define FW_BIN_SIZE		(10*1024*1024)

#define M7MU_FW_REQ_PATH	"RS_M7MU.bin"
#define M7MU_FW_REQ_PATH2	"/system/vendor/firmware/RS_M7MU.bin"
#define M7MU_EVT31_FW_REQ_PATH	"RS_M7MU_EVT3.1.bin"

#define FW_INFO_PATH		"/sdcard/FW_INFO.bin"

#define M7MU_FW_DUMP_PATH	"/sdcard/M7MU_dump.bin"

#define M7MU_FACTORY_CSV_PATH "/data/media/0/FACTORY_CSV_RAW.bin"

#define M7MUTB_FW_PATH "RS_M9LS_TB.bin" /* TECHWIN - SONY */
/* #define M7MUON_FW_PATH "RS_M9LS_ON.bin" */ /* FIBEROPTICS - SONY */
/* #define M7MUOM_FW_PATH "RS_M9LS_OM.bin" */ /* FIBEROPTICS - S.LSI */
#if defined(CONFIG_MACH_U1_KOR_LGT)
#define M7MUSB_FW_PATH "RS_M9LS_SB.bin" /* ELECTRO-MECHANICS - SONY */
#endif
/* #define M7MUSC_FW_PATH "RS_M9LS_SC.bin" */ /* ELECTRO-MECHANICS - S.LSI */
/* #define M7MUCB_FW_PATH "RS_M9LS_CB.bin" */ /* CAMSYS - SONY */
#if defined(CONFIG_TARGET_LOCALE_NA)
/* #define M7MUOE_FW_PATH "RS_M9LS_OE.bin" */ /* FIBEROPTICS - SONY */
#endif
#if defined(CONFIG_MACH_Q1_BD)
#define M7MUOO_FW_PATH "RS_M9LS_OO.bin" /* FIBEROPTICS - SONY */
#endif

#define FW_WRITE_SIZE 524288 /*2097152*/

#define M7MU_SHD_DATA_PATH "/sdcard/shading_table_dump.log"	//"/storage/extSdCard/C101_shading_data.bin"
#define FACTORY_RESOL_WIDE 106
#define FACTORY_RESOL_TELE 107
#define FACTORY_RESOL_WIDE_INSIDE 131
#define FACTORY_RESOL_TELE_INSIDE 132
#define FACTORY_TILT_TEST_INSIDE 133
#define FACTORY_DUST_TEST_INSIDE 136
#define FACTORY_LENS_SHADING_TEST_INSIDE 137
#define FACTORY_NOISE_TEST_INSIDE 138
#define FACTORY_REDIMAGE_TEST_INSIDE 165
#define FACTORY_TILT_DIVISION 168

#define M7MU_FLASH_BASE_ADDR	0x00000000

#define M7MU_FLASH_READ_BASE_ADDR	0x000000

#define M7MU_FLASH_BASE_ADDR_1	0x001FF000

extern unsigned int system_rev;

extern struct class *camera_class;
extern struct device *camera_rear_dev; /*sys/class/camera/rear*/

#define M7MU_ENABLE_REGULATOR_CTRL
#if defined(M7MU_ENABLE_REGULATOR_CTRL)
#define M7MU_ENABLE_REGULATOR_CTRL_CORE_3V3
#if defined(M7MU_ENABLE_REGULATOR_CTRL_CORE_3V3)
static struct regulator *main_cam_sensor_3v3_regulator;
#endif /* M7MU_ENABLE_REGULATOR_CTRL_CORE_3V3 */
static struct regulator *main_cam_sensor_a2v8_regulator;
static struct regulator *main_cam_sensor_1v2_regulator;
static struct regulator *main_cam_sensor_1v8_regulator;
#endif /* M7MU_ENABLE_REGULATOR_CTRL */

#define M7MU_ENABLE_ISP_FIRMWARE_UPDATE
#define M7MU_ENABLE_COLD_POWER

#define M7MU_ENABLE_POLLING_WAIT
#define M7MU_ENABLE_BOOT_IRQ_EN_CTRL

u8 buf_port_seting0[] = {
		  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		  0x00, 0x00, 0x00, 0xFF, 0x00, 0x00, 0x00, 0x00,
		  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0F,
		  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x07,
		  0x00, 0x00, 0x00, 0xFF, 0x00, 0x00, 0x00, 0xFF,
		 };
u8 buf_port_seting1[] = {
		  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		 };
u8 buf_port_seting2[] = {
		  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0C,
		  0x00, 0x00, 0x00, 0x20, 0x00, 0x00, 0x00, 0x10,
		  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		 };

/* maximum time for one frame at minimum fps (15fps) in normal mode */
#define NORMAL_MODE_MAX_ONE_FRAME_DELAY_MS     67

#ifdef HOLD_LENS_SUPPORT
static bool leave_power;
#endif
#ifdef M7MU_BUS_FREQ_LOCK
struct device *bus_dev;
#endif

static int m7mu_Lens_close_hold;
static int m7mu_Lens_Off_Needed;

static int m7mu_quickshot_called;
static int m7mu_quickshot_on;
static int m7mu_quickshot_holdlens;

static int esdreset;


int mmc_fw;

enum {
	M7MU_DEBUG_I2C		= 1U << 0,
	M7MU_DEBUG_I2C_BURSTS	= 1U << 1,
};

static uint32_t m7mu_debug_mask = M7MU_DEBUG_I2C_BURSTS;
module_param_named(debug_mask, m7mu_debug_mask, uint, S_IWUSR | S_IRUGO);

#ifdef READ_CSV_FILE_DIRECT
u32 M7MU_FLASH_FACTORY_OIS[] = {0x0000035C, 0x00000373};
u32 M7MU_FLASH_FACTORY_VIB[] = {0x00000374, 0x0000037B};
u32 M7MU_FLASH_FACTORY_GYRO[] = {0x00000384, 0x0000038E};
u32 M7MU_FLASH_FACTORY_TELE_RESOL[] = {0x00000000, 0x00000000}; /* Not Used */
u32 M7MU_FLASH_FACTORY_WIDE_RESOL[] = {0x00000000, 0x00000000}; /* Not Used */
u32 M7MU_FLASH_FACTORY_AF_FCS[] = {0x00000000, 0x00000000}; /* Not used */
u32 M7MU_FLASH_FACTORY_PUNT[] = {0x00000000, 0x00000023};
u32 M7MU_FLASH_FACTORY_DECENTER[] = {0x00000348, 0x0000035B};
u32 M7MU_FLASH_FACTORY_BACKLASH[] = {0x00000024, 0x00000026};

u32 M7MU_FLASH_FACTORY_AF_LED[] = {0x00000000, 0x00000000}; /* Not Used */
u32 M7MU_FLASH_FACTORY_IRIS[] = {0x00000064, 0x0000008D};
u32 M7MU_FLASH_FACTORY_LIVEVIEW[] = {0x000000B8, 0x000000BB};
u32 M7MU_FLASH_FACTORY_GAIN_CAPTURE[] = {0x000000BC, 0x000000BF};
u32 M7MU_FLASH_FACTORY_SH_CLOSE[] = {0x0000008F, 0x000000B7};
u32 M7MU_FLASH_FACTORY_FLASH_CHECK[] = {0x00000000, 0x00000000}; /* Not Used */
u32 M7MU_FLASH_FACTORY_WB_ADJ[] = {0x000000C8, 0x000000CF};
u32 M7MU_FLASH_FACTORY_FLASH_WB[] = {0x00000000, 0x00000000}; /* Not Used */
u32 M7MU_FLASH_FACTORY_ADJ_FLASH_WB[] = {0x00000000, 0x00000000}; /* Not Used */
/* Not Used */
u32 M7MU_FLASH_FACTORY_IR_CHECK[] = {0x00000000, 0x00000000};
/* Not Used */
u32 M7MU_FLASH_FACTORY_BEFORE_SHD_CHECK[] = {0x00000000, 0x00000000};
u32 M7MU_FLASH_FACTORY_RESOLUTION[] = {0x000002A8, 0x00000347};

u32 M7MU_FLASH_FACTORY_RESULT = 0x00000128;
#else
u32 M7MU_FLASH_FACTORY_OIS[] = {0x27E031A2, 0x27E031C7};
u32 M7MU_FLASH_FACTORY_VIB[] = {0x27E031C8, 0x27E031D1};
u32 M7MU_FLASH_FACTORY_GYRO[] = {0x27E031D2, 0x27E031D7};
u32 M7MU_FLASH_FACTORY_TELE_RESOL[] = {0x27E03298, 0x27E0329F};
u32 M7MU_FLASH_FACTORY_WIDE_RESOL[] = {0x27E032A0, 0x27E032A7};
u32 M7MU_FLASH_FACTORY_AF_FCS[] = {0x27E0323A, 0x27E03275};
u32 M7MU_FLASH_FACTORY_PUNT[] = {0x27E031D8, 0x27E03239};
u32 M7MU_FLASH_FACTORY_DECENTER[] = {0x27E032EC, 0x27E03303};

u32 M7MU_FLASH_FACTORY_BACKLASH[] = {0x27E03276, 0x27E03279};

u32 M7MU_FLASH_FACTORY_AF_LED[] = {0x27E032A8, 0x27E032AD};
u32 M7MU_FLASH_FACTORY_IRIS[] = {0x27E030E8, 0x27E03107};
u32 M7MU_FLASH_FACTORY_LIVEVIEW[] = {0x27E03108, 0x27E0310F};
u32 M7MU_FLASH_FACTORY_GAIN_CAPTURE[] = {0x27E03110, 0x27E03117};
u32 M7MU_FLASH_FACTORY_SH_CLOSE[] = {0x27E0327A, 0x27E03297};
u32 M7MU_FLASH_FACTORY_FLASH_CHECK[] = {0x27E032AE, 0x27E032B0};
u32 M7MU_FLASH_FACTORY_WB_ADJ[] = {0x27E03000, 0x27E03059};
u32 M7MU_FLASH_FACTORY_FLASH_WB[] = {0x27E032B4, 0x27E032C3};
u32 M7MU_FLASH_FACTORY_ADJ_FLASH_WB[] = {0x27E032D0, 0x27E032EB};
u32 M7MU_FLASH_FACTORY_IR_CHECK[] = {0x27E032C4, 0x27E032CD};
u32 M7MU_FLASH_FACTORY_BEFORE_SHD_CHECK[] = {0x27E033DD, 0x27E033E1};

u32 M7MU_FLASH_FACTORY_RESOLUTION[] = {0x27E03334, 0x27E033D3};

u32 M7MU_FLASH_FACTORY_RESULT = 0x27E03128;
#endif

static struct i2c_client *tempclient;
static struct v4l2_subdev *tempsubdev;

#define M7MU_INT_RAM_BASE_ADDR	0x01100000

#define M7MU_I2C_RETRY		5
#define M7MU_I2C_VERIFY		100
/* TODO
   Timeout delay is changed to 35 sec to support large shutter speed.
   This value must be set according to shutter speed.
*/
#define M7MU_ISP_TIMEOUT		35000
#define M7MU_ISP_SHORT_TIMEOUT  3000
#define M7MU_ISP_LONG_TIMEOUT	6000
#define M7MU_ISP_CAPTURE_TIMEOUT	35000
#define M7MU_SOUND_TIMEOUT		35000
#define M7MU_ISP_AFB_TIMEOUT	15000 /* FIXME */
#define M7MU_ISP_ESD_TIMEOUT	1000
#define M7MU_ISP_BOOT_TIMEOUT	5000
#define M7MU_PREVIEW_TIMEOUT	5000
#define M7MU_POLLING_TIMEOUT	1000

#define M7MU_JPEG_MAXSIZE	0x17E8000 /* 25M 4K align */
#define M7MU_THUMB_MAXSIZE	0x0
#define M7MU_POST_MAXSIZE	0x0

#define M7MU_DEF_APEX_DEN	100
#if 0
#define EXIF_ONE_THIRD_STOP_STEP
#else
#define EXIF_ONE_HALF_STOP_STEP
#endif

/* #define ISP_LOGWRITE */

#define SIZE_DEFAULT_FFMT	ARRAY_SIZE(default_fmt)
static struct v4l2_mbus_framefmt default_fmt[M7MU_OPRMODE_MAX] = {
	[M7MU_OPRMODE_VIDEO] = {
		.width		= DEFAULT_SENSOR_WIDTH,
		.height		= DEFAULT_SENSOR_HEIGHT,
		.code		= DEFAULT_SENSOR_CODE,
		.field		= V4L2_FIELD_NONE,
		.colorspace	= V4L2_COLORSPACE_JPEG,
	},
	[M7MU_OPRMODE_IMAGE] = {
		.width		= 1920,
		.height		= 1080,
		.code		= V4L2_MBUS_FMT_JPEG_1X8,
		.field		= V4L2_FIELD_NONE,
		.colorspace	= V4L2_COLORSPACE_JPEG,
	},
};

#define m7mu_readb(sd, g, b, v) m7mu_read(__LINE__, sd, 1, g, b, v, true)
#define m7mu_readw(sd, g, b, v) m7mu_read(__LINE__, sd, 2, g, b, v, true)
#define m7mu_readl(sd, g, b, v) m7mu_read(__LINE__, sd, 4, g, b, v, true)
#define m7mu_readw_int(sd, g, b, v) m7mu_read_int(__LINE__, sd, 2, g, b, v, true)

#define m7mu_writeb(sd, g, b, v) m7mu_write(__LINE__, sd, 1, g, b, v, true)
#define m7mu_writew(sd, g, b, v) m7mu_write(__LINE__, sd, 2, g, b, v, true)
#define m7mu_writel(sd, g, b, v) m7mu_write(__LINE__, sd, 4, g, b, v, true)

#define m7mu_readb2(sd, g, b, v) m7mu_read(__LINE__, sd, 1, g, b, v, false)
#define m7mu_readw2(sd, g, b, v) m7mu_read(__LINE__, sd, 2, g, b, v, false)
#define m7mu_readl2(sd, g, b, v) m7mu_read(__LINE__, sd, 4, g, b, v, false)

#define m7mu_writeb2(sd, g, b, v) m7mu_write(__LINE__, sd, 1, g, b, v, false)
#define m7mu_writew2(sd, g, b, v) m7mu_write(__LINE__, sd, 2, g, b, v, false)
#define m7mu_writel2(sd, g, b, v) m7mu_write(__LINE__, sd, 4, g, b, v, false)

#define CHECK_ERR(x)	{ \
	if ((x) <= 0) { \
		cam_err("i2c failed, err %d\n", x); \
		return x; \
	} \
}

#define NELEMS(array) (sizeof(array) / sizeof(array[0]))

struct m7mu_fw_header g_fw_header;

struct m7mu_resolution {
	u8			value;
	enum m7mu_oprmode	type;
	u16			width;
	u16			height;
};

/* DZ : Digital ZOOM is enabled in FCGD08     */
/* Monitor Sizes are changed to support DZoom */
/* From FCGD07(1056x704) to FCGD09~(960x640)  */
/* From FCGD07(1280x720) to FCGD09~(960x540)  */

/* CAUTION!!! : Postview Size must be set to the value same as Monitor Size */
static const struct m7mu_frmsizeenum preview_frmsizes[] = {
	{ M7MU_PREVIEW_QCIF,          176, 144, 176, 144, 0x05 },
	{ M7MU_PREVIEW_QVGA,          320, 240, 320, 240, 0x09 },
	{ M7MU_PREVIEW_VGA,           640, 480, 640, 480, 0x17 },
	/* { M7MU_PREVIEW_800_450,    800, 450, 1280, 720, 0x21 }, */
	{ M7MU_PREVIEW_720P,          1280, 720, 1280, 720, 0x21 },
	{ M7MU_PREVIEW_720P_60FPS,    1280, 720, 1280, 720, 0x25 },
	{ M7MU_PREVIEW_720P_120FPS,    1280, 720, 1280, 720, 0x41 },
	/* { M7MU_PREVIEW_HDR,        3264, 2448, 3264, 2448, 0x27 }, */
	{ M7MU_PREVIEW_1080P,         1920, 1080, 1920, 1080, 0x28 },
	{ M7MU_PREVIEW_1080P_DUAL,    1920, 1080, 1920, 1080, 0x2C },
	{ M7MU_PREVIEW_720P_DUAL,     1280, 720, 1280, 720, 0x2D },
	{ M7MU_PREVIEW_VGA_DUAL,      640, 480, 640, 480, 0x2E },
	{ M7MU_PREVIEW_VGA_60FPS,     640, 480, 640, 480, 0x2F },
	{ M7MU_PREVIEW_D1_120FPS,     768, 512, 768, 512, 0x33 },
	{ M7MU_PREVIEW_704_704,       704, 704, 704, 704, 0x40 },
	/* { M7MU_PREVIEW_704_528_DZ, 704, 528, 960, 720, 0x34 }, */
	{ M7MU_PREVIEW_960_720,       960, 720, 960, 720, 0x34 },
	{ M7MU_PREVIEW_1056_704,      1056, 704, 1056, 704, 0x35 },
	{ M7MU_PREVIEW_QVGA_DUAL,     320, 240, 320, 240, 0x36 },
	{ M7MU_PREVIEW_1440_1080,     1440, 1080, 1440, 1080, 0x37 },
	{ M7MU_PREVIEW_CIF,        704, 576, 704, 576, 0x3A },
	{ M7MU_PREVIEW_640_524,       608, 480, 608, 480, 0x3A },
	{ M7MU_PREVIEW_800_450,       800, 450, 800, 450, 0x3C },
	{ M7MU_PREVIEW_672_448,       672, 448, 672, 448, 0x3D },
	{ M7MU_PREVIEW_FACTORY_TILT,  1920, 1080, 1920, 1080, 0x3F },
};

static const struct m7mu_frmsizeenum capture_frmsizes[] = {
	{ M7MU_CAPTURE_VGA,     640, 480, 640, 480, 0x09 },
	{ M7MU_CAPTURE_HD,      960, 720, 960, 720, 0x34 },
	{ M7MU_CAPTURE_1MP,     1024, 768, 1024, 768, 0x0F },
	{ M7MU_CAPTURE_2MPW,    1920, 1080, 1920, 1080, 0x19 },
	{ M7MU_CAPTURE_3MP,     1984, 1488, 1984, 1488, 0x2F },
	{ M7MU_CAPTURE_4MP,     2304, 1728, 2304, 1728, 0x1E },
	{ M7MU_CAPTURE_5MP,     2592, 1944, 2592, 1944, 0x20 },
	{ M7MU_CAPTURE_6MP,     3264, 1836, 3264, 1836, 0x3E }, /* Magic shot */
	{ M7MU_CAPTURE_8MP,     3264, 2448, 3264, 2448, 0x25 },
	{ M7MU_CAPTURE_8_5MP,   2880, 2880, 2880, 2880, 0x3D },
	{ M7MU_CAPTURE_9MP,     3888, 2592, 3888, 2592, 0x39 },
	{ M7MU_CAPTURE_9MPW,    4096, 2304, 4096, 2304, 0x37 },
	{ M7MU_CAPTURE_10MP,    3648, 2736, 3648, 2736, 0x30 },
	{ M7MU_CAPTURE_11MP,    3960, 2640, 3960, 2640, 0x38 },
	{ M7MU_CAPTURE_12MPW,   4608, 2592, 4608, 2592, 0x31 },
	{ M7MU_CAPTURE_14MP,    4608, 3072, 4608, 3072, 0x32 },
	{ M7MU_CAPTURE_16MP,    4608, 3456, 4608, 3456, 0x33 },
	{ M7MU_CAPTURE_15_1MPW, 5184, 2916, 5184, 2916, 0x3C },
	{ M7MU_CAPTURE_17_9MP,  5184, 3456, 5184, 3456, 0x3B },
	{ M7MU_CAPTURE_20MP,    5184, 3888, 5184, 3888, 0x3A },
	{ M7MU_CAPTURE_9MW,    3968, 2232, 3968, 2232, 0x3F },
};

/* CAUTION!!! : Postview Size must be set to the value same as Monitor Size */
static const struct m7mu_frmsizeenum postview_frmsizes[] = {
	{ M7MU_CAPTURE_POSTQVGA,     320, 240, 320, 240, 0x01 },
	{ M7MU_CAPTURE_POSTVGA,      640, 480, 640, 480, 0x08 },
	{ M7MU_CAPTURE_POSTWVGA,     800, 480, 800, 480, 0x09 },
	{ M7MU_CAPTURE_POST_704_528, 704, 528, 960, 720, 0x13 },
	{ M7MU_CAPTURE_POST_704_704, 704, 704, 704, 704, 0x17 },
	{ M7MU_CAPTURE_POSTHD,       960, 720, 960, 720, 0x13 },
	{ M7MU_CAPTURE_POST_768_512, 768, 512, 960, 640, 0x16 },
	{ M7MU_CAPTURE_POSTP,        1056, 704, 1056, 704, 0x14 },
	{ M7MU_CAPTURE_POST_960_540, 960, 540, 960, 540, 0x15 },
	{ M7MU_CAPTURE_POSTWHD,      1280, 720, 1280, 720, 0x0F },
};

struct m7mu_framesize {
	u32 index;
	u32 width;
	u32 height;
};

static struct m7mu_control m7mu_ctrls[] = {
	{
		.id = V4L2_CID_CAM_ISO,
		.minimum = V4L2_ISO_AUTO,
		.maximum = V4L2_ISO_3200,
		.step = 1,
		.value = V4L2_ISO_AUTO,
		.default_value = V4L2_ISO_AUTO,
	}, {
		.id = V4L2_CID_CAM_BRIGHTNESS,
		.minimum = V4L2_BRIGHTNESS_MINUS_6,
		.maximum = V4L2_BRIGHTNESS_PLUS_6,
		.step = 1,
		.value = V4L2_BRIGHTNESS_DEFAULT,
		.default_value = V4L2_BRIGHTNESS_DEFAULT,
	}, {
		.id = V4L2_CID_CAMERA_SATURATION,
		.minimum = SATURATION_MINUS_2,
		.maximum = SATURATION_MAX - 1,
		.step = 1,
		.value = SATURATION_DEFAULT,
		.default_value = SATURATION_DEFAULT,
	}, {
		.id = V4L2_CID_CAMERA_SHARPNESS,
		.minimum = SHARPNESS_MINUS_2,
		.maximum = SHARPNESS_MAX - 1,
		.step = 1,
		.value = SHARPNESS_DEFAULT,
		.default_value = SHARPNESS_DEFAULT,
	}, {
		.id = V4L2_CID_CAMERA_CONTRAST,
		.minimum = CONTRAST_MINUS_2,
		.maximum = CONTRAST_MAX - 1,
		.step = 1,
		.value = CONTRAST_DEFAULT,
		.default_value = CONTRAST_DEFAULT,
	}, {
		.id = V4L2_CID_CAMERA_ZOOM,
		.minimum = ZOOM_LEVEL_0,
#if defined(CONFIG_MACH_GC2PD)
		.maximum = ZOOM_LEVEL_10,
#else
		.maximum = ZOOM_LEVEL_MAX - 1,
#endif
		.step = 1,
		.value = ZOOM_LEVEL_0,
		.default_value = ZOOM_LEVEL_0,
	}, {
		.id = V4L2_CID_JPEG_QUALITY, /* V4L2_CID_CAM_JPEG_QUALITY, */
		.minimum = 1,
		.maximum = 100,
		.step = 1,
		.value = 100,
		.default_value = 100,
	}, {
		.id = V4L2_CID_CAMERA_ANTI_BANDING,
		.minimum = ANTI_BANDING_AUTO,
		.maximum = ANTI_BANDING_OFF,
		.step = 1,
		.value = ANTI_BANDING_AUTO,
		.default_value = ANTI_BANDING_AUTO,
	},
};

static u8 sysfs_sensor_fw[M7MU_FW_VER_TOKEN + 1] = {0,};
static u8 sysfs_phone_fw[M7MU_FW_VER_TOKEN + 1] = {0,};
static u8 sysfs_sensor_type[M7MU_SENSOR_TYPE_LEN + 1] = {0,};
static int sysfs_check_app = -1;
/* static int s_zoom_speed = 1; */

#ifdef ENABLE_FAST_POWER_OFF
static struct workqueue_struct* m7mu_power_off_wq = NULL;
static struct v4l2_subdev *sd_for_power_off= NULL;
static int fast_power_off_flag = 0;

static void m7mu_power_off_func(struct work_struct *work);
static DECLARE_WORK(m7mu_power_off_work, m7mu_power_off_func);
#endif


static int m7mu_pre_init(struct v4l2_subdev *sd, u32 val);
static int m7mu_init(struct v4l2_subdev *sd, u32 val);
static int m7mu_post_init(struct v4l2_subdev *sd, u32 val);
static int m7mu_wait_boot_interrupt(struct v4l2_subdev *sd,
		unsigned int timeout);
static int m7mu_fw_writing(struct v4l2_subdev *sd);
static int m7mu_fw_writing_vacant(struct v4l2_subdev *sd);
static int m7mu_power(struct v4l2_subdev *sd, int flag, int option);
static int m7mu_s_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl);
static int m7mu_init_formats(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh);
static int m7mu_auto_fw_check(struct v4l2_subdev *sd);
static inline struct fimc_is_module_enum *to_module(struct v4l2_subdev *subdev)
{
	struct fimc_is_module_enum *module;
	module = (struct fimc_is_module_enum *)v4l2_get_subdevdata(subdev);
	return module;
}

static inline struct m7mu_state *to_state(struct v4l2_subdev *subdev)
{
	struct fimc_is_module_enum *module;
	module = (struct fimc_is_module_enum *)v4l2_get_subdevdata(subdev);
	return (struct m7mu_state *)module->private_data;
}

static inline struct i2c_client *to_client(struct v4l2_subdev *subdev)
{
	struct fimc_is_module_enum *module;
	module = (struct fimc_is_module_enum *)v4l2_get_subdevdata(subdev);
	return (struct i2c_client *)module->client;
}

#define CLEAR_ISP_BOOT_INT_STATE(sd)	\
	do {	\
		struct m7mu_state *state = to_state(sd);	\
		state->isp.boot_issued = 0;	\
		cam_info("isp.boot_issued was cleared.");	\
	} while (0)


#define CLEAR_ISP_INT1_STATE(sd)	\
	do {	\
		struct m7mu_state *state = to_state(sd);	\
		state->isp.issued = 0;	\
		cam_info("isp.issued was cleared.");	\
	} while (0)

static int m7mu_read(int _line, struct i2c_client *client,
	u8 len, u8 category, u8 byte, int *val, bool log)
{
	struct i2c_msg msg;
	unsigned char data[5];
	unsigned char recv_data[len + 1];
	int i, err = 0;
	int retry = 3;

	if (!client->adapter)
		return -ENODEV;

	if (len != 0x01 && len != 0x02 && len != 0x04)
		return -EINVAL;

	mutex_lock(&m7mu_lock);
i2c_retry:
	msg.addr = client->addr;
	msg.flags = 0;
	msg.len = sizeof(data);
	msg.buf = data;

	/* high byte goes out first */
	data[0] = msg.len;
	data[1] = 0x01;			/* Read category parameters */
	data[2] = category;
	data[3] = byte;
	data[4] = len;

	for (i = M7MU_I2C_RETRY; i; i--) {
		if (esdreset == 0) {
			err = i2c_transfer(client->adapter, &msg, 1);
			if (err == 1)
				break;
			if (i > 1)
				cam_warn("i2c read req retry %d (cate %#x, byte %#x), err %d\n",
					M7MU_I2C_RETRY-i+1, category, byte, err);
			msleep(20);
		} else {
			err = 0;
		}
	}

	if (err != 1) {
		cam_err("category %#x, byte %#x, err %d\n",
			category, byte, err);
		mutex_unlock(&m7mu_lock);
		return err;
	}

	msg.flags = I2C_M_RD;
	msg.len = sizeof(recv_data);
	msg.buf = recv_data;
	for (i = M7MU_I2C_RETRY; i; i--) {
		if (esdreset == 0) {
			err = i2c_transfer(client->adapter, &msg, 1);
			if (err == 1)
				break;
			if (i > 1)
				cam_warn("i2c read data retry %d (cate %#x, byte %#x), err %d\n",
					M7MU_I2C_RETRY-i+1, category, byte, err);
			msleep(20);
		} else {
			err = 0;
		}
	}

	if (err != 1) {
		cam_err("RD category %#x, byte %#x, err %d\n",
			category, byte, err);
		mutex_unlock(&m7mu_lock);
		return err;
	}

	if (recv_data[0] != sizeof(recv_data)) {
#if 0
		cam_i2c_dbg("expected length %d, but return length %d\n",
				 sizeof(recv_data), recv_data[0]);
#endif
		if (retry > 0) {
			retry--;
			cam_warn("i2c read retry %d (cate %#x, byte %#x), expected len %d, ret len %d\n",
				3-retry, category, byte, sizeof(recv_data), recv_data[0]);
			msleep(20);
			goto i2c_retry;
		} else {
			cam_err("Retry all failed for expected length error.");
			mutex_unlock(&m7mu_lock);
			return -1;
		}
	}

	if (len == 0x01)
		*val = recv_data[1];
	else if (len == 0x02)
		*val = recv_data[1] << 8 | recv_data[2];
	else
		*val = recv_data[1] << 24 | recv_data[2] << 16 |
				recv_data[3] << 8 | recv_data[4];

	if (log)
		cam_info("[ %4d ] Read %s %#02x, byte %#x, value %#x\n",
			_line, (len == 4 ? "L" : (len == 2 ? "W" : "B")),
			category, byte, *val);

	mutex_unlock(&m7mu_lock);
	return err;
}

static int m7mu_read_int(int _line, struct i2c_client *client,
	u8 len, u8 category, u8 byte, int *val, bool log)
{
	struct i2c_msg msg;
	unsigned char data[5];
	unsigned char recv_data[len + 1];
	int i, err = 0;
	int retry = 3;
	int retry_count = 0;

	if (!client->adapter)
		return -ENODEV;

	if (len != 0x01 && len != 0x02 && len != 0x04)
		return -EINVAL;

	mutex_lock(&m7mu_lock);
i2c_retry:
	msg.addr = client->addr;
	msg.flags = 0;
	msg.len = sizeof(data);
	msg.buf = data;

	/* high byte goes out first */
	data[0] = msg.len;
	data[1] = 0x01;			/* Read category parameters */
	data[2] = category;
	data[3] = byte;
	data[4] = len;

	for (i = M7MU_I2C_RETRY; i; i--) {
		if (esdreset == 0) {
			err = i2c_transfer(client->adapter, &msg, 1);
			if (err == 1)
				break;
			if (i > 1)
				cam_warn("i2c read req retry %d (cate %#x, byte %#x), err %d\n",
					M7MU_I2C_RETRY-i+1, category, byte, err);
			retry_count++;
			msleep(20);
		} else {
			err = 0;
		}
	}

	if (err != 1) {
		cam_err("category %#x, byte %#x, err %d\n",
			category, byte, err);
		mutex_unlock(&m7mu_lock);
		return -1;
	}

	msg.flags = I2C_M_RD;
	msg.len = sizeof(recv_data);
	msg.buf = recv_data;
	for (i = M7MU_I2C_RETRY; i; i--) {
		if (esdreset == 0) {
			err = i2c_transfer(client->adapter, &msg, 1);
			if (err == 1)
				break;
			if (i > 1)
				cam_warn("i2c read data retry %d (cate %#x, byte %#x), err %d\n",
					M7MU_I2C_RETRY-i+1, category, byte, err);
			retry_count++;
			msleep(20);
		} else {
			err = 0;
		}
	}

	if (err != 1) {
		cam_err("RD category %#x, byte %#x, err %d\n",
			category, byte, err);
		mutex_unlock(&m7mu_lock);
		return -1;
	}

	if (recv_data[0] != sizeof(recv_data)) {
#if 0
		cam_i2c_dbg("expected length %d, but return length %d\n",
				 sizeof(recv_data), recv_data[0]);
#endif
		if (retry > 0) {
			retry--;
			retry_count++;
			cam_warn("i2c read retry %d (cate %#x, byte %#x), expected len %d, ret len %d\n",
				3-retry, category, byte, sizeof(recv_data), recv_data[0]);
			msleep(20);
			goto i2c_retry;
		} else {
			cam_err("Retry all failed for expected length error.");
			mutex_unlock(&m7mu_lock);
			return -1;
		}
	}

	if (len == 0x01)
		*val = recv_data[1];
	else if (len == 0x02)
		*val = recv_data[1] << 8 | recv_data[2];
	else
		*val = recv_data[1] << 24 | recv_data[2] << 16 |
				recv_data[3] << 8 | recv_data[4];

	if (log)
		cam_info("[ %4d ] Read_Int %s %#02x, byte %#x, value %#x\n",
			_line, (len == 4 ? "L" : (len == 2 ? "W" : "B")),
			category, byte, *val);

	if(retry_count > 0)
		err = -1;
	mutex_unlock(&m7mu_lock);
	return err;
}

static int m7mu_write(int _line, struct i2c_client *client,
	u8 len, u8 category, u8 byte, int val, bool log)
{
	struct i2c_msg msg;
	unsigned char data[len + 4];
	int i, err = 0;

	if (!client->adapter)
		return -ENODEV;

	if (len != 0x01 && len != 0x02 && len != 0x04)
		return -EINVAL;

	mutex_lock(&m7mu_lock);
	msg.addr = client->addr;
	msg.flags = 0;
	msg.len = sizeof(data);
	msg.buf = data;

	data[0] = msg.len;
	data[1] = 0x02;			/* Write category parameters */
	data[2] = category;
	data[3] = byte;
	if (len == 0x01) {
		data[4] = val & 0xFF;
	} else if (len == 0x02) {
		data[4] = (val >> 8) & 0xFF;
		data[5] = val & 0xFF;
	} else {
		data[4] = (val >> 24) & 0xFF;
		data[5] = (val >> 16) & 0xFF;
		data[6] = (val >> 8) & 0xFF;
		data[7] = val & 0xFF;
	}

	if (log)
		cam_info("[ %4d ] Write %s %#x, byte %#x, value %#x\n",
			_line, (len == 4 ? "L" : (len == 2 ? "W" : "B")),
			category, byte, val);

	for (i = M7MU_I2C_RETRY; i; i--) {
		if (esdreset == 0) {
			err = i2c_transfer(client->adapter, &msg, 1);
			if (err == 1)
				break;
			if (i > 1)
				cam_warn("i2c write data retry %d (cate %#x, byte %#x, val %#x), err %d\n",
					M7MU_I2C_RETRY-i+1, category, byte, val, err);
			msleep(20);
		}
	}

	mutex_unlock(&m7mu_lock);
	return err;
}
static int m7mu_mem_dump(struct v4l2_subdev *sd, u16 len, u32 addr, u8 *val)
{
	struct i2c_client *client = to_client(sd);
	struct i2c_msg msg;
	unsigned char data[8];
	unsigned char recv_data[len + 3];
	int i, err = 0;

	if (!client->adapter)
		return -ENODEV;

	if (len <= 0)
		return -EINVAL;

	msg.addr = client->addr;
	msg.flags = 0;
	msg.len = sizeof(data);
	msg.buf = data;

	/* high byte goes out first */
	data[0] = 0x00;
	data[1] = 0x03;
	data[2] = 0x18;
	data[3] = (addr >> 16) & 0xFF;
	data[4] = (addr >> 8) & 0xFF;
	data[5] = addr & 0xFF;
	data[6] = (len >> 8) & 0xFF;
	data[7] = len & 0xFF;

	for (i = M7MU_I2C_RETRY; i; i--) {
		err = i2c_transfer(client->adapter, &msg, 1);
		if (err == 1)
			break;
		msleep(20);
	}

	if (err != 1)
		return err;

	msg.flags = I2C_M_RD;
	msg.len = sizeof(recv_data);
	msg.buf = recv_data;
	for (i = M7MU_I2C_RETRY; i; i--) {
		err = i2c_transfer(client->adapter, &msg, 1);
		if (err == 1)
			break;
		msleep(20);
	}

	if (err != 1)
		return err;

	if (len != (recv_data[1] << 8 | recv_data[2]))
		cam_i2c_dbg("expected length %d, but return length %d\n",
			len, recv_data[1] << 8 | recv_data[2]);

	memcpy(val, recv_data + 3, len);

	cam_i2c_dbg("address %#x, length %d\n", addr, len);
	return err;
}
static int m7mu_mem_read(struct v4l2_subdev *sd, u16 len, u32 addr, u8 *val)
{
	struct i2c_client *client = to_client(sd);
	struct i2c_msg msg;
	unsigned char data[8];
	unsigned char recv_data[len + 3];
	int i, err = 0;

	if (!client->adapter)
		return -ENODEV;

	if (len <= 0)
		return -EINVAL;

	msg.addr = client->addr;
	msg.flags = 0;
	msg.len = sizeof(data);
	msg.buf = data;

	/* high byte goes out first */
	data[0] = 0x00;
	data[1] = 0x03;
	data[2] = (addr >> 24) & 0xFF;
	data[3] = (addr >> 16) & 0xFF;
	data[4] = (addr >> 8) & 0xFF;
	data[5] = addr & 0xFF;
	data[6] = (len >> 8) & 0xFF;
	data[7] = len & 0xFF;

	for (i = M7MU_I2C_RETRY; i; i--) {
		err = i2c_transfer(client->adapter, &msg, 1);
		if (err == 1)
			break;
		msleep(20);
	}

	if (err != 1)
		return err;

	msg.flags = I2C_M_RD;
	msg.len = sizeof(recv_data);
	msg.buf = recv_data;
	for (i = M7MU_I2C_RETRY; i; i--) {
		err = i2c_transfer(client->adapter, &msg, 1);
		if (err == 1)
			break;
		msleep(20);
	}

	if (err != 1)
		return err;

	if (len != (recv_data[1] << 8 | recv_data[2]))
		cam_i2c_dbg("expected length %d, but return length %d\n",
			len, recv_data[1] << 8 | recv_data[2]);

	memcpy(val, recv_data + 3, len);

	/* cam_i2c_dbg("address %#x, length %d\n", addr, len); */
	return err;
}

static int m7mu_mem_write(struct v4l2_subdev *sd, u8 cmd,
		u16 len, u32 addr, u8 *val)
{
	struct i2c_client *client = to_client(sd);
	struct i2c_msg msg;
	unsigned char data[len + 8];
	int i, err = 0;

	if (!client->adapter)
		return -ENODEV;

	msg.addr = client->addr;
	msg.flags = 0;
	msg.len = sizeof(data);
	msg.buf = data;

	/* high byte goes out first */
	data[0] = 0x00;
	data[1] = cmd;
	data[2] = (addr >> 24) & 0xFF;
	data[3] = (addr >> 16) & 0xFF;
	data[4] = (addr >> 8) & 0xFF;
	data[5] = addr & 0xFF;
	data[6] = (len >> 8) & 0xFF;
	data[7] = len & 0xFF;
	memcpy(data + 2 + sizeof(addr) + sizeof(len), val, len);

	cam_i2c_dbg("address %#x, length %d\n", addr, len);

	for (i = M7MU_I2C_RETRY; i; i--) {
		err = i2c_transfer(client->adapter, &msg, 1);
		if (err == 1)
			break;
		msleep(20);
	}

	return err;
}

static int m7mu_makeLog(struct v4l2_subdev *sd,
		char *filename,
		bool dumpLogPath)
{
	struct m7mu_state *state = to_state(sd);
	struct i2c_client *client = to_client(sd);

	int addr = 0, len = 0xff; /* init */
	int err = 0;
	int i = 0, no = 0;
	char buf[256];

	struct file *fp;
	mm_segment_t old_fs;
	char filepath[256];
	state->log_num += 1;

	old_fs = get_fs();
	set_fs(KERNEL_DS);

#ifdef ISP_LOGWRITE
	if (dumpLogPath || !strncmp("_SEC_", filename, 5)) {
		sprintf(filepath, "/data/media/0/log/%s%c", filename, 0);
	} else
#endif
	{
		sprintf(filepath, "/data/media/0/ISPD/%s%c", filename, 0);
	}
	cam_trace(" FilePath = %s\n", filepath);

	fp = filp_open(filepath,
		O_WRONLY|O_CREAT|O_TRUNC, S_IRUGO|S_IWUGO|S_IXUSR);
	if (IS_ERR(fp)) {
		cam_err("failed to open %s, err %ld\n",
			filepath, PTR_ERR(fp));
		return  -1;
	}

#ifdef M7MU_ISP_DEBUG
	cam_dbg("%s\n", filepath);
#endif
	err = m7mu_writeb2(client, 0x0d, 0x06, 0x0);
	CHECK_ERR(err);

	err = m7mu_readl2(client, 0x0d, 0x08, &addr);
	CHECK_ERR(err);

	err = m7mu_writeb2(client, 0x0d, 0x0e, 0x2);
	CHECK_ERR(err);

	while (no < 10000) { /* max log count : 10000 */
		err = m7mu_writew2(client, 0x0d, 0x0c, no);
		CHECK_ERR(err);

		err = m7mu_writeb2(client, 0x0d, 0x0e, 0x3);
		CHECK_ERR(err);

		while (len == 0xff) {
			err = m7mu_readb2(client, 0x0d, 0x07, &len);
			CHECK_ERR(err);

			if (i++ > 3000)  /* only delay code */
				break;
		}

		if (len == 0 || len == 0xff) {
			err = m7mu_writeb2(client, 0x0d, 0x0e, 0x1);
			CHECK_ERR(err);
			break;
		}

		i = 0;
		len += 1;
		if (len > sizeof(buf))
			len = sizeof(buf);
		err = m7mu_mem_read(sd,  len, addr, buf);
		if (err < 0)
			cam_err("ISPD i2c falied, err %d\n", err);

		buf[len-1] = '\n';

		vfs_write(fp, buf, len,  &fp->f_pos);
#if 0
		cam_dbg("ISPD Log : %x[%d], %d, %32s)\n",
					addr, no, len, buf);
#endif
		len = 0xff; /* init */
		no++;
	}

	if (!IS_ERR(fp) && fp != NULL)
		filp_close(fp, current->files);

	set_fs(old_fs);

	return 0;
}

static int m7mu_eep_rw_start(struct v4l2_subdev *sd, u32 addr)
{
	int err = 1; /* 1 means OK in i2c */
	struct m7mu_state *state = to_state(sd);
	struct i2c_client *client = to_client(sd);

	if (state->lens_mem == 0) {   /* 4KB NOR access */
		/* M7Mu needs this cate param before NOR R/W */
		if (addr >= 0 && addr < 0x1000) {
			err = m7mu_writeb(client, M7MU_CATEGORY_ADJST,
				M7MU_ADJST_EEP_RW, 0x01);
			CHECK_ERR(err);
		}
	} else {   /* 1KB EEPROM access */
		/* M7Mu needs this cate param before EEPROM R/W */
		if (addr >= 0 && addr < 0x400) {
			err = m7mu_writeb(client, M7MU_CATEGORY_ADJST,
				M7MU_ADJST_EEP_RW, 0x01);
			CHECK_ERR(err);
		}
	}
	return err;
}


static int m7mu_quickshot_init(int option)
{
	if (option == 0) {
		if (m7mu_quickshot_called == 0)
			m7mu_quickshot_on = 0;
	} else {
		if (m7mu_quickshot_on == 0)
			m7mu_quickshot_holdlens = 0;
		m7mu_quickshot_called = 0;
	}
	return 0;
}

static int m7mu_which_power_on(void)
{
	/*0 : normal, 1:quickshot, 2:normal and don't lens ctl*/\

	if (m7mu_quickshot_on == 1) {
		return 1;
	} else {
		if (m7mu_quickshot_holdlens == 1)
			return 2;
		else
			return 0;
	}

	return 0;
}

static int m7mu_which_power_off(void)
{
	/*0 : normal, 1:don't lens ctl*/
	if (m7mu_quickshot_holdlens == 1)
		return 1;
	else if (m7mu_quickshot_holdlens == 0)
		return 0;

	return 0;
}

static u32 m7mu_wait_sound_interrupt(struct v4l2_subdev *sd,
		unsigned int timeout)
{
	struct m7mu_state *state = to_state(sd);

	if (wait_event_timeout(state->isp.sound_wait,
				state->isp.sound_issued == 1,
				msecs_to_jiffies(timeout)) == 0) {
		return -1;
	}

	state->isp.sound_issued = 0;

	return 1;
}

static u32 m7mu_wait_interrupt(struct v4l2_subdev *sd,
	unsigned int timeout)
{
	int err = 0;
	struct m7mu_state *state = to_state(sd);
	struct i2c_client *client = to_client(sd);
#ifdef M7MU_ENABLE_POLLING_WAIT
	int remain_timeout = timeout;
#endif
#ifdef ISP_LOGWRITE
	char filepath[256];

	sprintf(filepath, "ISP_interrupt_error_%d.log", state->log_num);
#endif
	cam_trace("E\n");

#ifdef M7MU_ENABLE_POLLING_WAIT
	while(remain_timeout > 0) {
		int polling_timeout = (remain_timeout >= M7MU_POLLING_TIMEOUT)? M7MU_POLLING_TIMEOUT:remain_timeout;
		cam_trace("Waiting start (remain_timeout = %d, polling_timeout=%d)\n", remain_timeout, polling_timeout);
		if (wait_event_timeout(state->isp.wait,
					state->isp.issued == 1 || gpio_get_value(GPIO_ISP_INT1_SH),
					msecs_to_jiffies(polling_timeout)) == 0) {
			cam_trace("Polling end (value=%d)\n", gpio_get_value(GPIO_ISP_INT1_SH));
			if(gpio_get_value(GPIO_ISP_INT1_SH) == 1) {
				cam_trace("Waiting end (value=%d) by polling\n", gpio_get_value(GPIO_ISP_INT1_SH));
				break;
			} else {
				remain_timeout -= polling_timeout;
			}
		} else {
			cam_trace("Waiting end (value=%d) by IRQ\n", gpio_get_value(GPIO_ISP_INT1_SH));
			break;
		}
	}

	if(remain_timeout <= 0)
	{
		u32 int_en = 0, int_factor = 0, mode = 0, sys_status = 0;
#ifdef ISP_LOGWRITE
		m7mu_makeLog(sd, filepath, true);
#endif
		cam_err("timeout ~~~~~~~~~~~~~~~~~~~~~~~ (value=%d)\n", gpio_get_value(GPIO_ISP_INT1_SH));
		err = m7mu_readw(client, M7MU_CATEGORY_SYS,
				M7MU_SYS_INT_EN, &int_en);
		CHECK_ERR(err);
		err = m7mu_readw(client, M7MU_CATEGORY_SYS,
			M7MU_SYS_INT_FACTOR, &int_factor);
		CHECK_ERR(err);
		err = m7mu_readb(client, M7MU_CATEGORY_SYS,
				M7MU_SYS_MODE, &mode);
		CHECK_ERR(err);
		err = m7mu_readb(client, M7MU_CATEGORY_SYS,
			0x0c, &sys_status);
		CHECK_ERR(err);
		cam_err("int_en=%d, int_factor=%d, mode_status=%d, sys_status=%d\n",
			int_en, int_factor, mode, sys_status);
		return 0;
	}

#else	/* M7MU_ENABLE_POLLING_WAIT */
	if (wait_event_timeout(state->isp.wait,
				state->isp.issued == 1,
				msecs_to_jiffies(timeout)) == 0) {
		u32 int_en = 0, int_factor = 0, mode = 0, sys_status = 0;
#ifdef ISP_LOGWRITE
		m7mu_makeLog(sd, filepath, true);
#endif
		cam_err("timeout ~~~~~~~~~~~~~~~~~~~~~~~\n");
		err = m7mu_readw(client, M7MU_CATEGORY_SYS,
				M7MU_SYS_INT_EN, &int_en);
		CHECK_ERR(err);
		err = m7mu_readw(client, M7MU_CATEGORY_SYS,
			M7MU_SYS_INT_FACTOR, &int_factor);
		CHECK_ERR(err);
		err = m7mu_readb(client, M7MU_CATEGORY_SYS,
				M7MU_SYS_MODE, &mode);
		CHECK_ERR(err);
		err = m7mu_readb(client, M7MU_CATEGORY_SYS,
			0x0c, &sys_status);
		CHECK_ERR(err);
		cam_err("int_en=%d, int_factor=%d, mode_status=%d, sys_status=%d\n",
			int_en, int_factor, mode, sys_status);
		return 0;
	}
#endif

	state->isp.issued = 0;

	err = m7mu_readw_int(client, M7MU_CATEGORY_SYS,
		M7MU_SYS_INT_FACTOR, &state->isp.int_factor);
	cam_err(": state->isp.int_factor = %x\n",
		state->isp.int_factor);

	if ((err == -1) && 
		((state->isp.int_factor == 0) || (state->isp.int_factor == 0x8000))) {
		/* When interrupt is read twice, the value is reset to 0 by ISP.
		   So it is regarded as OK when I2C retry.
		   After reset, shutter INT(0x8000) can occur. */
		state->isp.int_factor = 0xFFFF;
		cam_err("I2c retry in Interrupt -> all interrupt OK(0xFFFF)\n");
		cam_trace("X\n");
	} else
		cam_trace("X %s\n",
			(state->isp.int_factor == 0xFFFF ? "fail(0xFFFF)" : ""));
	return state->isp.int_factor;
}

static int m7mu_wait_framesync(struct v4l2_subdev *sd)
{
	int i, frame_sync_count = 0;
	u32 int_factor;
	s32 read_val = 0;

	struct m7mu_state *state = to_state(sd);
	struct i2c_client *client = to_client(sd);

	 if (state->running_capture_mode == RUNNING_MODE_AE_BRACKET
		|| state->running_capture_mode == RUNNING_MODE_LOWLIGHT
		|| state->running_capture_mode == RUNNING_MODE_HDR) {
		cam_dbg("Start AE Bracket or HDR capture\n");
		frame_sync_count = 3;
	} else if (state->running_capture_mode == RUNNING_MODE_BLINK) {
		cam_dbg("Start FaceDetect EyeBlink capture\n");
		frame_sync_count = 3;
	}

	/* Clear Interrupt factor */
	for (i = frame_sync_count; i; i--) {
		int_factor = m7mu_wait_interrupt(sd,
				M7MU_SOUND_TIMEOUT);
		if (!(int_factor & M7MU_INT_FRAME_SYNC)) {
			cam_warn("M7MU_INT_FRAME_SYNC isn't issued, %#x\n",
					int_factor);
			return -ETIMEDOUT;
		}
		m7mu_readb(client,
				M7MU_CATEGORY_SYS,
				M7MU_SYS_FRAMESYNC_CNT,
				&read_val);
		cam_dbg("Frame interrupt FRAME_SYNC cnt[%d]\n",
				read_val);
	}

	return 0;
}

static int m7mu_set_smart_auto_default_value(struct v4l2_subdev *sd, int val)
{
	struct m7mu_state *state = to_state(sd);
	struct i2c_client *client = to_client(sd);

	int err, value;

	cam_trace("E %d\n", val);

	if (val == 1) {
		err = m7mu_writeb(client, M7MU_CATEGORY_MON,
			M7MU_MON_EDGE_CTRL, state->sharpness);
		CHECK_ERR(err);

		err = m7mu_writeb(client, M7MU_CATEGORY_MON,
			M7MU_MON_CHROMA_LVL, state->saturation);
		CHECK_ERR(err);

		err = m7mu_writeb(client, M7MU_CATEGORY_MON,
			M7MU_MON_COLOR_EFFECT, state->color_effect);
		CHECK_ERR(err);
	} else {
		err = m7mu_writeb(client, M7MU_CATEGORY_AE,
			M7MU_AE_EP_MODE_CAP, 0x05);
		CHECK_ERR(err);

		err = m7mu_writeb(client, M7MU_CATEGORY_AE,
			M7MU_AE_EV_PRG_MODE_CAP, 0x10);
		CHECK_ERR(err);

#if 0
		err = m7mu_writeb(client, M7MU_CATEGORY_CAPPARM,
			M7MU_CAPPARM_STROBE_EN, state->strobe_en);
		CHECK_ERR(err);
#else
		err = m7mu_readb(client, M7MU_CATEGORY_CAPPARM,
			M7MU_CAPPARM_STROBE_EN, &value);
		CHECK_ERR(err);

		cam_trace("store_en: %d\n", value);

		if (value != state->strobe_en) {
			err = m7mu_writeb(client, M7MU_CATEGORY_CAPPARM,
				M7MU_CAPPARM_STROBE_EN, state->strobe_en);
			CHECK_ERR(err);
		}
#endif

		err = m7mu_writeb(client, M7MU_CATEGORY_AE,
			M7MU_AE_INDEX, 0x1E);
		CHECK_ERR(err);

		err = m7mu_writeb(client, M7MU_CATEGORY_CAPPARM,
			M7MU_CAPPARM_WDR_EN, 0x0);
		CHECK_ERR(err);

		err = m7mu_writeb(client, M7MU_CATEGORY_MON,
			M7MU_MON_EDGE_CTRL, 0x03);
		CHECK_ERR(err);

		err = m7mu_writeb(client, M7MU_CATEGORY_MON,
			M7MU_MON_CHROMA_LVL, 0x03);
		CHECK_ERR(err);

		err = m7mu_readb(client, M7MU_CATEGORY_MON,
			M7MU_MON_COLOR_EFFECT, &value);
		CHECK_ERR(err);

		if (value == 0x11 || value == 0x21) {
			err = m7mu_writeb(client, M7MU_CATEGORY_MON,
				M7MU_MON_COLOR_EFFECT, state->color_effect);
			CHECK_ERR(err);
		}
#if 0
		err = m7mu_writeb(client, M7MU_CATEGORY_MON,
			0xAE, 0x0);
		CHECK_ERR(err);
#endif
	}

	cam_trace("X\n");
	return 0;
}

static int m7mu_set_mode(struct v4l2_subdev *sd, u32 mode)
{
	int i, err;
	u32 old_mode, val;
	u32 int_factor, int_en;
	/* int retry_mode_change = 1; */
	struct m7mu_state *state = to_state(sd);
	struct i2c_client *client = to_client(sd);
	int check_mode_change = 0;

	cam_trace("E\n");

	err = m7mu_readb(client, M7MU_CATEGORY_SYS, M7MU_SYS_MODE, &old_mode);
	CHECK_ERR(err);

	if (state->samsung_app) {
		/* don't change mode when cap -> param */
		if (old_mode == M7MU_STILLCAP_MODE && mode == M7MU_PARMSET_MODE)
			return 10;
	}

	/* Dual Capture */
	if (state->dual_capture_start && mode == M7MU_STILLCAP_MODE)
		mode = M7MU_PARMSET_MODE;

	if (old_mode == mode) {
		cam_dbg("%#x -> %#x\n", old_mode, mode);
		return old_mode;
	}

	cam_dbg("%#x -> %#x\n", old_mode, mode);

	CLEAR_ISP_INT1_STATE(sd);

/* retry_mode_set: */
	switch (old_mode) {
	case M7MU_SYSINIT_MODE:
		cam_warn("sensor is initializing\n");
		err = -EBUSY;
		break;

	case M7MU_PARMSET_MODE:
		if (mode == M7MU_STILLCAP_MODE) {
			err = m7mu_writeb(client, M7MU_CATEGORY_SYS,
				M7MU_SYS_MODE, M7MU_MONITOR_MODE);
			if (err <= 0)
				break;
			for (i = M7MU_I2C_VERIFY; i; i--) {
				err = m7mu_readb(client, M7MU_CATEGORY_SYS,
					M7MU_SYS_MODE, &val);
				if (val == M7MU_MONITOR_MODE)
					break;
				msleep(20);
			}
		}
	case M7MU_MONITOR_MODE:
	case M7MU_STILLCAP_MODE:
		err = m7mu_writeb(client, M7MU_CATEGORY_SYS,
				M7MU_SYS_MODE, mode);
		break;

	default:
		cam_warn("current mode is unknown, %d\n", old_mode);
		err = 1;/* -EINVAL; */
	}

	if (err <= 0)
		return err;

	for (i = M7MU_I2C_VERIFY; i; i--) {
#if 0
		err = m7mu_readb(client, M7MU_CATEGORY_SYS,
				M7MU_SYS_MODE, &val);
		if (val == mode)
			break;
		msleep(20);
#else
		if(check_mode_change++ % 10 == 0) {
			err = m7mu_readb(client, M7MU_CATEGORY_SYS,
				M7MU_SYS_MODE, &val);
			if(val == old_mode && check_mode_change > 10) {
				cam_warn("mode of ISP don't change!!!\n");
				err = m7mu_writeb(client, M7MU_CATEGORY_SYS,
					M7MU_SYS_MODE, mode);
			}
		} else {
			err = m7mu_readb2(client, M7MU_CATEGORY_SYS,
				M7MU_SYS_MODE, &val);
		}

		if (val == mode)
			break;
		msleep(20);
#endif
	}

	if (val != mode) {
#if 0
		if (retry_mode_change) {
			retry_mode_change = 0;
			goto retry_mode_set;
		} else {
			cam_warn("ISP mode not change, %d -> %d\n", val, mode);
			return -ETIMEDOUT;
		}
#else
		cam_warn("ISP mode not change, %d -> %d\n", val, mode);
		return -ETIMEDOUT;
#endif
	}
#if 0 /* Not supported cateParam on M7MU */
	if ((mode == M7MU_PARMSET_MODE) &&
			(state->mode == MODE_ERASER)) {
		err = m7mu_writeb(client, M7MU_CATEGORY_PARM,
			M7MU_PARM_SET_ERASER_MODE, 0x01);
		if (err < 0)
			return err;
	} else if ((mode == M7MU_PARMSET_MODE) &&
			(state->mode != MODE_ERASER)) {
		err = m7mu_writeb(client, M7MU_CATEGORY_PARM,
			M7MU_PARM_SET_ERASER_MODE, 0x00);
		if (err < 0)
			return err;
	}
#endif
	if (mode == M7MU_STILLCAP_MODE
		&& state->running_capture_mode != RUNNING_MODE_AE_BRACKET
		&& state->running_capture_mode != RUNNING_MODE_LOWLIGHT
		&& state->running_capture_mode != RUNNING_MODE_HDR
		&& state->running_capture_mode != RUNNING_MODE_BLINK) {

		m7mu_wait_framesync(sd);

		if (state->running_capture_mode == RUNNING_MODE_WB_BRACKET
			|| state->running_capture_mode == RUNNING_MODE_RAW) {
			err = m7mu_readw(client, M7MU_CATEGORY_SYS,
						M7MU_SYS_INT_EN, &int_en);
			CHECK_ERR(err);

#if 0
			if (int_en & M7MU_INT_SOUND) {
				/* Clear Interrupt factor */
				int_factor = m7mu_wait_interrupt(sd,
						M7MU_SOUND_TIMEOUT);
				if (!(int_factor & M7MU_INT_SOUND)) {
					cam_warn("M7MU_INT_SOUND isn't issued, %#x\n",
							int_factor);
					return -ETIMEDOUT;
				}
			}
#endif
		}
		if (state->mode != MODE_MAGIC) {
			/* Clear Interrupt factor */
			int_factor = m7mu_wait_interrupt(sd,
					M7MU_ISP_CAPTURE_TIMEOUT);
			if (!(int_factor & M7MU_INT_CAPTURE)) {
				cam_warn("M7MU_INT_CAPTURE isn't issued, %#x\n",
						int_factor);
				return -ETIMEDOUT;
			}
		}
	}

	state->isp_mode = mode;

	if (state->factory_test_num != 0x0) {
		cam_trace("X\n");
		return old_mode;
	}

	if (state->mode == MODE_SMART_AUTO
			|| state->mode == MODE_SMART_SUGGEST) {
		if (old_mode == M7MU_STILLCAP_MODE && mode == M7MU_MONITOR_MODE)
			m7mu_set_smart_auto_default_value(sd, 0);
	}

	cam_trace("X\n");
	return old_mode;
}

static int m7mu_set_mode_part1(struct v4l2_subdev *sd, u32 mode)
{
	int i, err;
	u32 old_mode, val;
	u32 int_en;
	int retry_mode_change = 1;
	struct m7mu_state *state = to_state(sd);
	struct i2c_client *client = to_client(sd);
	state->stream_on_part2 = false;

	cam_trace("E\n");

	err = m7mu_readb(client, M7MU_CATEGORY_SYS, M7MU_SYS_MODE, &old_mode);
	CHECK_ERR(err);

	if (state->samsung_app) {
		/* don't change mode when cap -> param */
		if (old_mode == M7MU_STILLCAP_MODE && mode == M7MU_PARMSET_MODE)
			return 10;
	}

	/* Dual Capture */
	if (state->dual_capture_start && mode == M7MU_STILLCAP_MODE)
		mode = M7MU_PARMSET_MODE;

	if (old_mode == mode) {
		cam_dbg("%#x -> %#x\n", old_mode, mode);
		return old_mode;
	}

	cam_dbg("%#x -> %#x\n", old_mode, mode);

retry_mode_set:
	switch (old_mode) {
	case M7MU_SYSINIT_MODE:
		cam_warn("sensor is initializing\n");
		err = -EBUSY;
		break;

	case M7MU_PARMSET_MODE:
		if (mode == M7MU_STILLCAP_MODE) {
			err = m7mu_writeb(client, M7MU_CATEGORY_SYS,
				M7MU_SYS_MODE, M7MU_MONITOR_MODE);
			if (err <= 0)
				break;
			for (i = M7MU_I2C_VERIFY; i; i--) {
				err = m7mu_readb(client, M7MU_CATEGORY_SYS,
					M7MU_SYS_MODE, &val);
				if (val == M7MU_MONITOR_MODE)
					break;
				msleep(20);
			}
		}
	case M7MU_MONITOR_MODE:
	case M7MU_STILLCAP_MODE:
		err = m7mu_writeb(client, M7MU_CATEGORY_SYS,
				M7MU_SYS_MODE, mode);
		break;

	default:
		cam_warn("current mode is unknown, %d\n", old_mode);
		err = 1;/* -EINVAL; */
	}

	if (err <= 0)
		return err;

#if 1  /* check by read mode */
	for (i = M7MU_I2C_VERIFY; i; i--) {
		err = m7mu_readb(client, M7MU_CATEGORY_SYS,
				M7MU_SYS_MODE, &val);
		if (val == mode)
			break;
		msleep(20);
	}
#else /* check by interrupt */
	/* Clear Interrupt factor */
	int_factor = m7mu_wait_interrupt(sd, M7MU_ISP_TIMEOUT);
	if (!(int_factor & M7MU_INT_CAPTURE)) {
		cam_warn("M7MU_INT_CAPTURE isn't issued on mode change, %#x\n",
				int_factor);
		return -ETIMEDOUT;
	}
#endif

	if (val != mode) {
		if (retry_mode_change) {
			retry_mode_change = 0;
			goto retry_mode_set;
		} else {
			cam_warn("ISP mode not change, %d -> %d\n", val, mode);
			return -ETIMEDOUT;
		}
	}

	state->isp_mode = mode;

	if (mode == M7MU_STILLCAP_MODE
		&& state->running_capture_mode != RUNNING_MODE_AE_BRACKET
		&& state->running_capture_mode != RUNNING_MODE_LOWLIGHT) {

		err = m7mu_readw(client, M7MU_CATEGORY_SYS,
						M7MU_SYS_INT_EN, &int_en);
		CHECK_ERR(err);

#if 0
		if (int_en & M7MU_INT_SOUND) {
			/* Clear Interrupt factor */
			int_factor = m7mu_wait_interrupt(sd,
					M7MU_SOUND_TIMEOUT);
			if (!(int_factor & M7MU_INT_SOUND)) {
				cam_warn("M7MU_INT_SOUND isn't issued, %#x\n",
						int_factor);
				return -ETIMEDOUT;
			}
		}
#endif
	}

	state->stream_on_part2 = true;

	cam_trace("X\n");
	return old_mode;
}

static int m7mu_set_mode_part2(struct v4l2_subdev *sd, u32 mode)
{
	u32 int_factor;
	struct m7mu_state *state = to_state(sd);
	struct i2c_client *client = to_client(sd);
	int sys_status, err, cnt = 100;

	if (state->running_capture_mode != RUNNING_MODE_SINGLE)
		return 0;

	if (state->stream_on_part2 == false)
		return 0;

	cam_trace("E, %d\n", mode);

	/* Dual Capture */
	if (state->dual_capture_start && mode == M7MU_STILLCAP_MODE)
		mode = M7MU_PARMSET_MODE;

	if (mode == M7MU_STILLCAP_MODE
		&& state->running_capture_mode != RUNNING_MODE_AE_BRACKET
		&& state->running_capture_mode != RUNNING_MODE_LOWLIGHT) {

		/* m7mu_wait_framesync(sd); */

		/* Clear Interrupt factor */
		int_factor = m7mu_wait_interrupt(sd, M7MU_ISP_CAPTURE_TIMEOUT);
		if (!(int_factor & M7MU_INT_CAPTURE)) {
			cam_warn("M7MU_INT_CAPTURE isn't issued, %#x\n",
					int_factor);
			return -ETIMEDOUT;
		}

		/* Check ISP state */
		err = m7mu_readb(client, M7MU_CATEGORY_SYS,
				0x0c, &sys_status);
		CHECK_ERR(err);

		while (sys_status != 7 && cnt) {
			msleep(20);
			err = m7mu_readb(client, M7MU_CATEGORY_SYS,
					0x0c, &sys_status);
			CHECK_ERR(err);

			if (sys_status == 7)
				break;

			cnt--;
		}
	}

	state->stream_on_part2 = false;

	cam_trace("X\n");
	return 0;
}

static int m7mu_set_cap_rec_end_mode(struct v4l2_subdev *sd, u32 mode)
{
	u32 int_factor, old_mode;

	cam_trace("E, %d\n", mode);

	/* not use stop recording cmd */
	if (mode == 100)
		return 0;

	CLEAR_ISP_INT1_STATE(sd);

	old_mode = m7mu_set_mode(sd, M7MU_MONITOR_MODE);
	if (old_mode <= 0) {
		cam_err("failed to set mode\n");
		return old_mode;
	}

	if (old_mode != M7MU_MONITOR_MODE) {
		int_factor = m7mu_wait_interrupt(sd, M7MU_ISP_TIMEOUT);
		if (!(int_factor & M7MU_INT_MODE)) {
			cam_err("M7MU_INT_MODE isn't issued!!!\n");
			return -ETIMEDOUT;
		}
	}

	cam_trace("X\n");
	return 0;
}

static int m7mu_set_OIS_cap_mode(struct v4l2_subdev *sd)
{
	int err;
	int set_ois_cap_mode, read_ois_cap_mode;
	struct m7mu_state *state = to_state(sd);
	struct i2c_client *client = to_client(sd);

	cam_trace("E\n");

	err = m7mu_readb(client, M7MU_CATEGORY_NEW,
		M7MU_NEW_OIS_CUR_MODE, &read_ois_cap_mode);
	CHECK_ERR(err);

	switch (state->running_capture_mode) {
	case RUNNING_MODE_CONTINUOUS:
	case RUNNING_MODE_BEST:
	case RUNNING_MODE_LOWLIGHT:
	case RUNNING_MODE_AE_BRACKET:
	case RUNNING_MODE_HDR:
	case RUNNING_MODE_BLINK:
	case RUNNING_MODE_BURST:
		set_ois_cap_mode = 0x05;
		break;

	case RUNNING_MODE_SINGLE:
	case RUNNING_MODE_WB_BRACKET:
	default:
		set_ois_cap_mode = 0x04;
		break;
	}

	if (state->recording) {
		if (state->fps <= 30)
			set_ois_cap_mode = 0x01;
		else
			set_ois_cap_mode = 0x02;
	} else if (state->mode == MODE_PANORAMA)
		set_ois_cap_mode = 0x03;

	if (set_ois_cap_mode != read_ois_cap_mode) {
		err = m7mu_writeb(client, M7MU_CATEGORY_NEW,
			M7MU_NEW_OIS_CUR_MODE, set_ois_cap_mode);
		CHECK_ERR(err);
	}

	cam_trace("X set mode : %d\n", set_ois_cap_mode);

	return 0;
}

static void m7mu_init_parameters(struct v4l2_subdev *subdev)
{
	struct m7mu_state *state = to_state(subdev);

	struct sec_cam_parm *parms =
		(struct sec_cam_parm *)&state->strm.parm.raw_data;
	struct sec_cam_parm *stored_parms =
		(struct sec_cam_parm *)&state->stored_parm.parm.raw_data;
	struct i2c_client *client = to_client(subdev);

	dev_info(&client->dev, "%s:\n", __func__);
	state->strm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	state->oprmode = M7MU_OPRMODE_VIDEO;
	parms->capture.capturemode = 0;
	parms->capture.timeperframe.numerator = 1;
	parms->capture.timeperframe.denominator = 30;
	parms->contrast = V4L2_CONTRAST_DEFAULT;
	parms->effects = V4L2_IMAGE_EFFECT_NONE;
	parms->brightness = V4L2_BRIGHTNESS_DEFAULT;
	parms->flash_mode = V4L2_FLASH_MODE_AUTO;
	parms->focus_mode = V4L2_FOCUS_MODE_AUTO;
	parms->iso = V4L2_ISO_AUTO;
	parms->metering = V4L2_METERING_CENTER;
	parms->saturation = V4L2_SATURATION_DEFAULT;
	parms->scene_mode = SCENE_MODE_NONE;
	parms->sharpness = V4L2_SHARPNESS_DEFAULT;
	parms->white_balance = V4L2_WHITE_BALANCE_AUTO;
	parms->fps = V4L2_FRAME_RATE_AUTO;
	parms->aeawb_lockunlock = V4L2_AE_UNLOCK_AWB_UNLOCK;

	stored_parms->effects = V4L2_IMAGE_EFFECT_NONE;
	stored_parms->brightness = V4L2_BRIGHTNESS_DEFAULT;
	stored_parms->iso = V4L2_ISO_AUTO;
	stored_parms->metering = V4L2_METERING_CENTER;
	stored_parms->scene_mode = SCENE_MODE_NONE;
	stored_parms->white_balance = V4L2_WHITE_BALANCE_AUTO;
	stored_parms->fps = V4L2_FRAME_RATE_AUTO;

	state->jpeg.enable = 0;
	state->jpeg.quality = 100;
	state->jpeg.main_offset = 0;
	state->jpeg.main_size = 0;
	state->jpeg.thumb_offset = 0;
	state->jpeg.thumb_size = 0;
	state->jpeg.postview_offset = 0;
	state->start_cap_kind = 0;

	state->fw.major = 1;

	state->one_frame_delay_ms = NORMAL_MODE_MAX_ONE_FRAME_DELAY_MS;

	state->running_capture_mode = RUNNING_MODE_SINGLE;

/*	m7mu_stop_auto_focus(sd); */
}

static int m7mu_set_oprmode_mode(struct v4l2_subdev *subdev, int oprmode)
{
	struct m7mu_state *state = to_state(subdev);

	cam_info("set oprmode to %d\n", oprmode);
	switch (oprmode) {
	case M7MU_OPRMODE_VIDEO:
		state->oprmode = M7MU_OPRMODE_VIDEO;
		break;
	case M7MU_OPRMODE_IMAGE:
		state->oprmode = M7MU_OPRMODE_IMAGE;
		break;
	case M7MU_OPRMODE_HDR:
		state->oprmode = M7MU_OPRMODE_HDR;
		break;
	default:
		cam_err("Oprmode %d is not supported.", oprmode);
		return -1;
	}

	return 0;
}

static int m7mu_set_capture_mode(struct v4l2_subdev *sd, int val)
{
	int err, capture_val;
	int raw_enable, int_en, evp_val, like_pro_mode;
	struct m7mu_state *state = to_state(sd);
	struct i2c_client *client = to_client(sd);

	cam_trace("E capture_mode=%d\n", val);

	state->running_capture_mode = val;

	if (state->running_capture_mode == RUNNING_MODE_DUMP) {
		cam_trace("X\n");
		return state->running_capture_mode;
	}

	err = m7mu_readb(client, M7MU_CATEGORY_CAPCTRL,
			M7MU_CAPCTRL_CAP_MODE, &capture_val);
	CHECK_ERR(err);

	if (state->mode == MODE_PROGRAM) {
		err = m7mu_readb(client, M7MU_CATEGORY_AE,
				M7MU_AE_EP_MODE_CAP, &evp_val);
		CHECK_ERR(err);

		if (evp_val != 0) {
			err = m7mu_writeb(client, M7MU_CATEGORY_AE,
					M7MU_AE_EP_MODE_CAP, 0X00);
			CHECK_ERR(err);
		}
	}

	switch (state->running_capture_mode) {
	case RUNNING_MODE_AE_BRACKET:
	case RUNNING_MODE_LOWLIGHT:
		cam_trace("~~~~~~ AutoBracket AEB ~~~~~~\n");
		if (capture_val != M7MU_CAP_MODE_BRACKET_CAPTURE) {
			err = m7mu_writeb(client, M7MU_CATEGORY_CAPCTRL,
					0x0B, 0x00);
			CHECK_ERR(err);
			err = m7mu_writeb(client, M7MU_CATEGORY_CAPCTRL,
					M7MU_CAPCTRL_CAP_MODE,
					M7MU_CAP_MODE_BRACKET_CAPTURE);
			CHECK_ERR(err);
		}

		if (state->running_capture_mode == RUNNING_MODE_LOWLIGHT) {
			err = m7mu_writeb(client, M7MU_CATEGORY_AE,
				M7MU_AE_AUTO_BRACKET_EV, 0x0); /* EV 0.0 */
			CHECK_ERR(err);

			err = m7mu_writeb(client, M7MU_CATEGORY_AE,
				M7MU_AE_EP_MODE_CAP, 0x05);
			CHECK_ERR(err);

			err = m7mu_writeb(client, M7MU_CATEGORY_CAPPARM,
				M7MU_CAPPARM_YUVOUT_MAIN, 0x00);
			CHECK_ERR(err);


			/*
			   set LIKE_PRO_MODE to Night
			   (CateParam 0x5, 0x1, 0x13)
			 */
			/* Request by ISP(M7Mu) */
			err = m7mu_readb(client, M7MU_CATEGORY_PRO_MODE,
					0x1, &like_pro_mode);
			CHECK_ERR(err);
			if (like_pro_mode != 0x13) {	/* 0x13: Night mode */
				err = m7mu_writeb(client,
					M7MU_CATEGORY_PRO_MODE,	0x1, 0x13);
				CHECK_ERR(err);
			}

			/* set LIKE_PRO_EN to ON (CateParam 0x5, 0x0, 0x1) */
			/* Request by ISP(M7Mu) */
			err = m7mu_readb(client, M7MU_CATEGORY_PRO_MODE,
					0x0, &like_pro_mode);
			CHECK_ERR(err);
			if (like_pro_mode != 0x01) {
				err = m7mu_writeb(client,
					M7MU_CATEGORY_PRO_MODE,	0x0, 0x01);
				CHECK_ERR(err);
			}
		} else {
			err = m7mu_writeb(client, M7MU_CATEGORY_CAPPARM,
				M7MU_CAPPARM_YUVOUT_MAIN, 0x01);
			CHECK_ERR(err);
		}

		err = m7mu_writeb(client, M7MU_CATEGORY_CAPCTRL,
				M7MU_CAPCTRL_CAP_FRM_INTERVAL, 0x00);
		CHECK_ERR(err); /* 0:7.5, 1:5, 2:3fps */
	break;

	case RUNNING_MODE_WB_BRACKET:
		cam_trace("~~~~~~ AutoBracket WBB ~~~~~~\n");
		if (capture_val != M7MU_CAP_MODE_SINGLE_CAPTURE) {
			err = m7mu_writeb(client, M7MU_CATEGORY_CAPCTRL,
					M7MU_CAPCTRL_CAP_MODE,
					M7MU_CAP_MODE_SINGLE_CAPTURE);
			CHECK_ERR(err);
		}
	break;

	case RUNNING_MODE_HDR:
		cam_trace("~~~~~~ HDRmode capture ~~~~~~\n");
		if (capture_val != M7MU_CAP_MODE_BRACKET_CAPTURE) {
			err = m7mu_writeb(client, M7MU_CATEGORY_CAPCTRL,
					M7MU_CAPCTRL_CAP_MODE,
					M7MU_CAP_MODE_BRACKET_CAPTURE);
			CHECK_ERR(err);
		}
		CHECK_ERR(err);
		err = m7mu_writeb(client, M7MU_CATEGORY_CAPCTRL,
				M7MU_CAPCTRL_CAP_FRM_INTERVAL, 0x00);
		CHECK_ERR(err); /* 0:7.5, 1:5, 2:3fps */
		err = m7mu_writeb(client, M7MU_CATEGORY_CAPPARM,
				M7MU_CAPPARM_YUVOUT_MAIN, 0x00);
		CHECK_ERR(err);
		break;

	case RUNNING_MODE_BLINK:
		cam_trace("~~~~~~ EyeBlink capture ~~~~~~\n");
		if (capture_val != M7MU_CAP_MODE_BLINK_CAPTURE) {
			err = m7mu_writeb(client, M7MU_CATEGORY_CAPCTRL,
					M7MU_CAPCTRL_CAP_MODE,
					M7MU_CAP_MODE_BLINK_CAPTURE);
			CHECK_ERR(err);
		}
		err = m7mu_writeb(client, M7MU_CATEGORY_CAPPARM,
				M7MU_CAPPARM_YUVOUT_MAIN, 0x01);
		CHECK_ERR(err);
		break;

	case RUNNING_MODE_RAW:
		cam_trace("~~~~~~ raw capture ~~~~~~\n");
		err = m7mu_readb(client, M7MU_CATEGORY_CAPPARM,
				0x78, &raw_enable);
		CHECK_ERR(err);

		/* if (capture_val != M7MU_CAP_MODE_RAW) always run */
		if (raw_enable != 0x01) {
			err = m7mu_writeb(client, M7MU_CATEGORY_CAPPARM,
					0x78, 0x01);
			CHECK_ERR(err);
		}
		break;

	case RUNNING_MODE_BURST:
		if (state->mode == MODE_MAGIC)
			cam_trace("~~~~~~ Magic shot mode ~~~~~~\n");
		else {
			cam_trace("~~~~~~ burst capture mode ~~~~~~\n");
			err = m7mu_writeb(client, M7MU_CATEGORY_CAPCTRL,
					M7MU_CAPCTRL_CAP_MODE, 0x0D);
			CHECK_ERR(err);
		}

		state->mburst_start = false;
		break;

	case RUNNING_MODE_SINGLE:
	default:
		cam_trace("~~~~~~ Single capture ~~~~~~\n");
		if (capture_val != M7MU_CAP_MODE_SINGLE_CAPTURE) {
			err = m7mu_writeb(client, M7MU_CATEGORY_CAPCTRL,
					M7MU_CAPCTRL_CAP_MODE,
					M7MU_CAP_MODE_SINGLE_CAPTURE);
			CHECK_ERR(err);
			/* Select main image format: jpeg capture */
			err = m7mu_writeb(client, M7MU_CATEGORY_CAPPARM,
					M7MU_CAPPARM_YUVOUT_MAIN, 0x01);
			CHECK_ERR(err);
		}

		if (state->mode == MODE_SMART_AUTO ||
				state->mode == MODE_PANORAMA ||
				state->mode == MODE_PROGRAM ||
				state->mode == MODE_SMART_SELF ||
				state->mode == MODE_BEST_GROUP_POSE ||
				state->mode == MODE_BEAUTY_SHOT ||
				state->mode == MODE_BEST_SHOT ||
				state->mode == MODE_CONTINUOUS_SHOT ||
				state->mode == MODE_ERASER ||
				state->mode == MODE_A ||
				state->mode == MODE_S ||
				state->mode == MODE_M ||
				state->mode == MODE_VIDEO ||
				state->mode == MODE_GOLF_SHOT) {
			/* set LIKE_PRO_EN to OFF (CateParam 0x5, 0x0, 0x0) */
			err = m7mu_readb(client, M7MU_CATEGORY_PRO_MODE,
					0x0, &like_pro_mode);
			CHECK_ERR(err);
			if (like_pro_mode != 0x00) {
				err = m7mu_writeb(client,
						M7MU_CATEGORY_PRO_MODE,
						0x0, 0x00);
				CHECK_ERR(err);
			}

			/* set LIKE_PRO_MODE to OFF (CateParam 0x5, 0x1, 0x0) */
			err = m7mu_readb(client, M7MU_CATEGORY_PRO_MODE,
					0x1, &like_pro_mode);
			CHECK_ERR(err);
			if (like_pro_mode != 0x0) {
				err = m7mu_writeb(client,
						M7MU_CATEGORY_PRO_MODE,
						0x1, 0x0);
				CHECK_ERR(err);
			}
		}
		break;
	}

	/* set low light shot flag for ISP */
	if (state->running_capture_mode == RUNNING_MODE_LOWLIGHT) {
		err = m7mu_writeb(client, M7MU_CATEGORY_LENS,
			0x2E, 0x01);
		CHECK_ERR(err);
	} else {
		err = m7mu_writeb(client, M7MU_CATEGORY_LENS,
			0x2E, 0x00);
		CHECK_ERR(err);
	}

	err = m7mu_readw(client, M7MU_CATEGORY_SYS,
		M7MU_SYS_INT_EN, &int_en);
	CHECK_ERR(err);

	if (state->running_capture_mode == RUNNING_MODE_LOWLIGHT
		|| state->running_capture_mode == RUNNING_MODE_AE_BRACKET
		||  state->running_capture_mode == RUNNING_MODE_HDR
		||  state->running_capture_mode == RUNNING_MODE_BLINK) {
		int_en &= ~M7MU_INT_FRAME_SYNC;
	} else {
		int_en |= M7MU_INT_FRAME_SYNC;
	}

	err = m7mu_writew(client, M7MU_CATEGORY_SYS, M7MU_SYS_INT_EN, int_en);
	CHECK_ERR(err);

	m7mu_set_OIS_cap_mode(sd);

	cam_trace("X\n");
	return state->running_capture_mode;
}


/*
 * v4l2_subdev_core_ops
 */
static int m7mu_queryctrl(struct v4l2_subdev *sd, struct v4l2_queryctrl *qc)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(m7mu_ctrls); i++) {
		if (qc->id == m7mu_ctrls[i].id) {
			qc->maximum = m7mu_ctrls[i].maximum;
			qc->minimum = m7mu_ctrls[i].minimum;
			qc->step = m7mu_ctrls[i].step;
			qc->default_value = m7mu_ctrls[i].default_value;
			return 0;
		}
	}

	return -EINVAL;
}

static int m7mu_set_lock(struct v4l2_subdev *sd, int val)
{
	struct m7mu_state *state = to_state(sd);
	struct i2c_client *client = to_client(sd);
	int err ;
#if 0
	int status;
	int cnt = 100;
#endif

	cam_trace("%s\n", val ? "on" : "off");

#if 1
	if (val == 0) {
		err = m7mu_writeb(client, M7MU_CATEGORY_AE,
				M7MU_AF_AE_LOCK, val);
		CHECK_ERR(err);
#if 0
		err = m7mu_writeb(client, M7MU_CATEGORY_AE, M7MU_AE_LOCK, val);
		CHECK_ERR(err);
		err = m7mu_writeb(client, M7MU_CATEGORY_WB, M7MU_AWB_LOCK, val);
		CHECK_ERR(err);
#endif	/* #if 0 */
		state->sys_status = 0x0;
	}
#else
	err = m7mu_writeb(client, M7MU_CATEGORY_AE, M7MU_AF_AE_LOCK, val);
	CHECK_ERR(err);

	/* check AE stability before AE,AWB lock */
	if (val == 1) {
		err = m7mu_readb(client, M7MU_CATEGORY_AE,
			M7MU_AE_STABILITY, &status);

		while (!status && cnt) {
			msleep(20);
			err = m7mu_readb(client, M7MU_CATEGORY_AE,
				M7MU_AE_STABILITY, &status);
			CHECK_ERR(err);
			cnt--;
		}
	}

	err = m7mu_writeb(client, M7MU_CATEGORY_AE, M7MU_AE_LOCK, val);
	CHECK_ERR(err);
	err = m7mu_writeb(client, M7MU_CATEGORY_WB, M7MU_AWB_LOCK, val);
	CHECK_ERR(err);
#endif

	state->focus.lock = val;

	cam_trace("X\n");
	return 0;
}

static int m7mu_set_CAF(struct v4l2_subdev *sd, int val)
{
	int err, range_status, af_range, zoom_status, mode_status;
	int window_status = -1;
	struct m7mu_state *state = to_state(sd);
	struct i2c_client *client = to_client(sd);

	if (state->fps == 120) {
		cam_info("not supported on 120 fps !!!\n");
		return 0;
	}

	err = m7mu_readb2(client, M7MU_CATEGORY_LENS,
		M7MU_LENS_ZOOM_STATUS, &zoom_status);
	CHECK_ERR(err);

	if (zoom_status == 1 && val == 1) {
		cam_info("zoom moving !!! val : %d\n", val);
		return 0;
	}

	state->caf_state = val;

	if (val == 1) {
		if (state->focus.status != 0x1000) {
			/* Set LOCK OFF */
			if (state->focus.lock && state->focus.status != 0x1000)
				m7mu_set_lock(sd, 0);

			/* Set mode to Continuous */
			err = m7mu_readb(client, M7MU_CATEGORY_LENS,
				M7MU_LENS_AF_MODE, &mode_status);

			if (mode_status != 1) {
				err = m7mu_writeb(client, M7MU_CATEGORY_LENS,
					M7MU_LENS_AF_MODE, 0x01);
			CHECK_ERR(err);
			}

			err = m7mu_readb(client, M7MU_CATEGORY_LENS,
				M7MU_LENS_AF_SCAN_RANGE, &range_status);

			/* Set range to auto-macro */
				af_range = 0x02;

			if (range_status != af_range) {
				err = m7mu_writeb(client, M7MU_CATEGORY_LENS,
					M7MU_LENS_AF_SCAN_RANGE, af_range);
				CHECK_ERR(err);
#if 0
				/* Set Zone REQ */
				err = m7mu_writeb(client, M7MU_CATEGORY_LENS,
						M7MU_LENS_AF_INITIAL, 0x04);
				CHECK_ERR(err);
#endif
			}

			/* Set AF Window Mode to Center*/
			err = m7mu_readb(client, M7MU_CATEGORY_LENS,
				M7MU_LENS_AF_WINDOW_MODE, &window_status);

			if (window_status != 0x0
					&& state->facedetect_mode == V4L2_FACE_DETECTION_OFF) {
				err = m7mu_writeb(client, M7MU_CATEGORY_LENS,
					M7MU_LENS_AF_WINDOW_MODE, 0x0);
				CHECK_ERR(err);
			}

			/* Start Continuous AF */
			err = m7mu_writeb(client, M7MU_CATEGORY_LENS,
				M7MU_LENS_AF_START_STOP, 0x01);
			CHECK_ERR(err);
		}
	} else {
		/* Stop Continuous AF */
		err = m7mu_writeb(client, M7MU_CATEGORY_LENS,
			M7MU_LENS_AF_START_STOP, 0x02);
		CHECK_ERR(err);
	}

	cam_trace("X val : %d %d\n", val, state->focus.mode);
	return 0;
}

static int m7mu_get_af_result(struct v4l2_subdev *sd,
		struct v4l2_control *ctrl)
{
	struct m7mu_state *state = to_state(sd);
	struct i2c_client *client = to_client(sd);
	int status, err, sys_status;
	static int get_cnt;

	cam_trace("E, cnt:%d, focus.status:0x%x, mode:0x%x, samsung_app:%d, sys_status:%d\n",
		get_cnt, state->focus.status, state->focus.mode, state->samsung_app,
		state->sys_status);

	err = m7mu_readb(client, M7MU_CATEGORY_SYS,
			M7MU_SYS_STATUS, &sys_status);
	CHECK_ERR(err);

	if (sys_status == 0x3)
		state->sys_status = 0x3;

	if(state->focus.mode == V4L2_FOCUS_MODE_CONTINUOUS
		|| state->samsung_app == 0
		|| state->sys_status == 0x3) {
		sys_status = 0x3;
	}

	/* 0x3 : 0x03: Auto focus*/
	if(sys_status != 0x3) {
		cam_dbg("sys_status : %d ~~~ focusing !!!~~~\n", sys_status);
		state->af_running = 0;
		ctrl->value = state->focus.status = 0x1000;
	} else if(sys_status == 0x3 ) {

		get_cnt++;

		err = m7mu_readw2(client, M7MU_CATEGORY_LENS,
			M7MU_LENS_AF_RESULT, &status);
		CHECK_ERR(err);

		if ((status != 0x1000) && (status != 0x0)) {
			cam_trace("~~~ success !!!~~~\n");
			state->af_running = 0;
			get_cnt = 0;
		} else if (status == 0x0) {
			cam_trace("~~~ fail !!!~~~\n");
			state->af_running = 0;
			get_cnt = 0;
		} else if (status == 0x1000) {
			cam_dbg("~~~ focusing !!!~~~\n");
			state->af_running = 0;
		}

		ctrl->value = state->focus.status = status;

		cam_trace("X, value 0x%04x\n", ctrl->value);
	}

	return ctrl->value;
}

static int m7mu_get_af_done(struct v4l2_subdev *sd,
		struct v4l2_control *ctrl)
{
	struct m7mu_state *state = to_state(sd);
	struct i2c_client *client = to_client(sd);
	int sys_status, err, aelock_status;

	/* Check ISP state */
	err = m7mu_readb(client, M7MU_CATEGORY_SYS,
					0x0c, &sys_status);
	CHECK_ERR(err);

	err = m7mu_readb(client, M7MU_CATEGORY_AE,
					M7MU_AE_LOCK, &aelock_status);
	CHECK_ERR(err);
	cam_trace("m7mu_get_af_done aelock_status:%d\n", aelock_status);

	if (sys_status == 2 || sys_status == 4 || sys_status == 5)
		ctrl->value = 1; /* Done */
	else
		ctrl->value = 0; /* In progress */

	if (ctrl->value == 1) {
		if (state->focus.status == 0x1000)
			cam_err("Error!! Focus is still in progress\n");
#if 0
		if (state->focus.mode == V4L2_FOCUS_MODE_TOUCH
				&& state->focus.touch && aelock_status == 0)
#else
		if (state->focus.mode == V4L2_FOCUS_MODE_TOUCH
				&& state->focus.touch)
#endif
			m7mu_set_lock(sd, 0);

		if (state->focus.lock && !(state->focus.start))
			m7mu_set_lock(sd, 0);

		if (state->caf_state && !(state->focus.start))
			m7mu_set_CAF(sd, 1);
	}
	return err;
}

static int m7mu_get_ois_decenter(struct v4l2_subdev *sd,
		struct v4l2_control *ctrl)
{
	struct i2c_client *client = to_client(sd);
	int adj_status, err;

	/* Check ISP state */
	err = m7mu_readb(client, M7MU_CATEGORY_LENS,
					0x40, &adj_status);
	CHECK_ERR(err);

	if (adj_status == 1 || adj_status == 2)
		ctrl->value = adj_status; /* NG(1) or OK(2) */
	else
		ctrl->value = 0; /* In progress */
	return err;
}

static int m7mu_get_manual_focus(struct v4l2_subdev *sd,
		struct v4l2_control *ctrl)
{
	int err;
	int default_index, near_index, max_index;
	struct i2c_client *client = to_client(sd);

	cam_dbg("E\n");

	err = m7mu_readb2(client, M7MU_CATEGORY_LENS,
		M7MU_LENS_MF_DEFAULT_INDEX, &default_index);
	CHECK_ERR(err);

	err = m7mu_readb2(client, M7MU_CATEGORY_LENS,
		M7MU_LENS_MF_NEARLIMIT_INDEX, &near_index);
	CHECK_ERR(err);

	err = m7mu_readb2(client, M7MU_CATEGORY_LENS,
		M7MU_LENS_MF_MAX_INDEX, &max_index);
	CHECK_ERR(err);

	ctrl->value = (default_index << 16)
		| (near_index << 8)
		| max_index;

	cam_dbg("X, default_index 0x%x, near_index 0x%x, max_index 0x%x\n",
		default_index, near_index, max_index);

	return err;
}

static int m7mu_get_scene_mode(struct v4l2_subdev *sd,
		struct v4l2_control *ctrl)
{
	int err;
	struct i2c_client *client = to_client(sd);

	err = m7mu_readb2(client, M7MU_CATEGORY_NEW,
			M7MU_NEW_DETECT_SCENE, &ctrl->value);

#if 0
	cam_trace("mode : %d\n", ctrl->value);
#endif

	return ctrl->value;
}

static int m7mu_get_scene_sub_mode(struct v4l2_subdev *sd,
		struct v4l2_control *ctrl)
{
	int err;
	struct i2c_client *client = to_client(sd);

	err = m7mu_readb2(client, M7MU_CATEGORY_NEW, 0x0C, &ctrl->value);

	return ctrl->value;
}

static int m7mu_get_zoom_level(struct v4l2_subdev *sd,
	struct v4l2_control *ctrl)
{
	struct m7mu_state *state = to_state(sd);
	struct i2c_client *client = to_client(sd);
	int err;
	int zoom_level, zoom_status, zoom_lens_status;

	err = m7mu_readb2(client, M7MU_CATEGORY_LENS,
		M7MU_LENS_ZOOM_LEVEL_INFO, &zoom_level);
	CHECK_ERR(err);

	err = m7mu_readb2(client, M7MU_CATEGORY_LENS,
		M7MU_LENS_ZOOM_STATUS, &zoom_status);
	CHECK_ERR(err);

	err = m7mu_readb2(client, M7MU_CATEGORY_LENS,
		M7MU_LENS_ZOOM_LENS_STATUS, &zoom_lens_status);
	CHECK_ERR(err);

	if (state->zoom <= 0xF && (zoom_level & 0xF) < 0xF)
		state->zoom = zoom_level & 0xF;
	ctrl->value = ((0x1 & zoom_status) << 4)
		| ((0x1 & zoom_lens_status) << 5)
		| (0xF & zoom_level);

	return 0;
}

static int m7mu_get_zoom_status(struct v4l2_subdev *sd,
	struct v4l2_control *ctrl)
{
	struct m7mu_state *state = to_state(sd);
	struct i2c_client *client = to_client(sd);
	int err;
	int curr_zoom_info;

	err = m7mu_readb2(client, M7MU_CATEGORY_PRO_MODE,
		M7MU_PRO_SMART_READ3, &curr_zoom_info);
	CHECK_ERR(err);

	if (state->zoom <= 0x3F && (curr_zoom_info & 0x3F) < 0x3F)
		state->zoom = curr_zoom_info & 0x3F;
	ctrl->value = curr_zoom_info & 0xFF;

	return 0;
}

static int m7mu_get_smart_read1(struct v4l2_subdev *sd,
	struct v4l2_control *ctrl)
{
	int value, err;
	struct i2c_client *client = to_client(sd);

	err = m7mu_readl2(client, M7MU_CATEGORY_PRO_MODE,
		M7MU_PRO_SMART_READ1, &value);
	CHECK_ERR(err);

	ctrl->value = value;

	return ctrl->value;
}

static int m7mu_get_smart_read2(struct v4l2_subdev *sd,
	struct v4l2_control *ctrl)
{
	int value, err;
	struct i2c_client *client = to_client(sd);

	err = m7mu_readl2(client, M7MU_CATEGORY_PRO_MODE,
		M7MU_PRO_SMART_READ2, &value);
	CHECK_ERR(err);

	ctrl->value = value;

	return ctrl->value;
}

/* for NSM Mode */
static int m7mu_get_Nsm_fd_info(struct v4l2_subdev *sd,
		struct v4l2_control *ctrl)
{
	int err;
	int fd_first_w, fd_last_b, value;
	struct i2c_client *client = to_client(sd);

	cam_dbg("E\n");

	err = m7mu_readw2(client, M7MU_CATEGORY_FD,
			M7MU_CAMERA_NSM_FD_FIRST_W, &fd_first_w);
	CHECK_ERR(err);

	err = m7mu_readb2(client, M7MU_CATEGORY_FD,
			M7MU_CAMERA_NSM_FD_LAST_B, &fd_last_b);
	CHECK_ERR(err);

	value = (fd_first_w << 8) | fd_last_b;
	cam_trace("E, fd_first_w = 0x%08x\n", (fd_first_w << 8));
	cam_trace("E, fd_last_b = 0x%08x\n", fd_last_b);
	cam_trace("E, value = 0x%08x\n", value);

	ctrl->value = value;

	cam_dbg("X\n");
	return ctrl->value;
}
/* end NSM Mode */

static int m7mu_get_SMART_SELF_SHOT_FD_INFO1(struct v4l2_subdev *sd,
	struct v4l2_control *ctrl)
{
	int value, err;
	struct i2c_client *client = to_client(sd);

	err = m7mu_readl2(client, M7MU_CATEGORY_FD,
		M7MU_FD_X_LOCATION, &value);
	CHECK_ERR(err);

	ctrl->value = value;

	return ctrl->value;
}

static int m7mu_get_SMART_SELF_SHOT_FD_INFO2(struct v4l2_subdev *sd,
	struct v4l2_control *ctrl)
{
	int value, err;
	struct i2c_client *client = to_client(sd);

	err = m7mu_readl2(client, M7MU_CATEGORY_FD,
		M7MU_FD_FRAME_WIDTH, &value);
	CHECK_ERR(err);

	ctrl->value = value;

	return ctrl->value;
}


static int m7mu_get_lens_status(struct v4l2_subdev *sd,
	struct v4l2_control *ctrl)
{
	int value, err;
	struct i2c_client *client = to_client(sd);

	err = m7mu_readb2(client, M7MU_CATEGORY_LENS,
		M7MU_LENS_LENS_STATUS, &value);
	CHECK_ERR(err);

	ctrl->value = value;

	return ctrl->value;
}

static int m7mu_get_flash_status(struct v4l2_subdev *sd,
	struct v4l2_control *ctrl)
{
	int err;
	int strobe_charge;
	struct i2c_client *client = to_client(sd);
#if 0
	int strobe_up_down;
#endif

	err = m7mu_readb2(client, M7MU_CATEGORY_CAPPARM,
		M7MU_CAPPARM_STROBE_CHARGE, &strobe_charge);
	CHECK_ERR(err);

	ctrl->value = strobe_charge;
#if 0
	err = m7mu_readb2(client, M7MU_CATEGORY_CAPPARM,
		M7MU_CAPPARM_STROBE_UP_DOWN, &strobe_up_down);
	CHECK_ERR(err);

	strobe_charge &= 0xFF;
	strobe_up_down &= 0xFF;

	ctrl->value = strobe_charge | (strobe_up_down << 8);

	cam_trace(": strobe_charge %d  up_down %d\n",
		strobe_charge, strobe_up_down);
#endif

	return ctrl->value;
}

static int m7mu_get_object_tracking(struct v4l2_subdev *sd,
	struct v4l2_control *ctrl)
{
	struct m7mu_state *state = to_state(sd);
	struct i2c_client *client = to_client(sd);
	int err;
#if 0
	int ot_ready, cnt = 30;
#endif

	err = m7mu_readb2(client, M7MU_CATEGORY_OT,
		M7MU_OT_TRACKING_STATUS, &state->ot_status);
	CHECK_ERR(err);

#if 0
	if (state->ot_status != OBJECT_TRACKING_STATUS_SUCCESS)
		return 0;

	err = m7mu_readb2(client, M7MU_CATEGORY_OT,
		M7MU_OT_INFO_READY, &ot_ready);
	CHECK_ERR(err);
	while (ot_ready && cnt) {
		msleep(20);
		err = m7mu_readb(client, M7MU_CATEGORY_OT,
			M7MU_OT_INFO_READY, &ot_ready);
		CHECK_ERR(err);
		cnt--;
	}

	err = m7mu_readw(client, M7MU_CATEGORY_OT,
		M7MU_OT_TRACKING_X_LOCATION,
		&state->ot_x_loc);
	CHECK_ERR(err);
	err = m7mu_readw(client, M7MU_CATEGORY_OT,
		M7MU_OT_TRACKING_Y_LOCATION,
		&state->ot_y_loc);
	CHECK_ERR(err);
	err = m7mu_readw(client, M7MU_CATEGORY_OT,
		M7MU_OT_TRACKING_FRAME_WIDTH,
		&state->ot_width);
	CHECK_ERR(err);
	err = m7mu_readw(client, M7MU_CATEGORY_OT,
		M7MU_OT_TRACKING_FRAME_HEIGHT,
		&state->ot_height);
	CHECK_ERR(err);
	cam_dbg("OT pos x: %d, y: %d, w: %d, h: %d\n",
		state->ot_x_loc, state->ot_y_loc,
		state->ot_width, state->ot_height);

	cam_trace("X status : %d\n", state->ot_status);
#endif
	return 0;
}

static int m7mu_get_warning_condition(struct v4l2_subdev *sd,
		struct v4l2_control *ctrl)
{
	int value, err;
	struct i2c_client *client = to_client(sd);

	err = m7mu_readw2(client, M7MU_CATEGORY_PRO_MODE,
		0x03, &value);
	CHECK_ERR(err);

	ctrl->value = value;

	return ctrl->value;
}

static int m7mu_get_av(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	struct m7mu_state *state = to_state(sd);
	struct i2c_client *client = to_client(sd);
	int value, err;

	err = m7mu_readl2(client, M7MU_CATEGORY_AE,
		M7MU_AE_NOW_AV, &value);
	CHECK_ERR(err);

	ctrl->value = value;
	state->AV = value;

	return ctrl->value;
}

static int m7mu_get_tv(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	struct m7mu_state *state = to_state(sd);
	struct i2c_client *client = to_client(sd);
	int value, err;

	err = m7mu_readl2(client, M7MU_CATEGORY_AE,
		M7MU_AE_NOW_TV, &value);
	CHECK_ERR(err);

	ctrl->value = value;
	state->TV = value;

	return ctrl->value;
}

static int m7mu_get_sv(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	struct m7mu_state *state = to_state(sd);
	struct i2c_client *client = to_client(sd);
	int value, err;

	err = m7mu_readl2(client, M7MU_CATEGORY_AE,
		M7MU_AE_NOW_SV, &value);
	CHECK_ERR(err);

	ctrl->value = value;
	state->SV = ctrl->value;

	return ctrl->value;
}

static int m7mu_get_ev(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	struct m7mu_state *state = to_state(sd);

	state->EV = state->AV + state->TV;

	return state->EV;
}

static int m7mu_get_lv(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	struct m7mu_state *state = to_state(sd);
	struct i2c_client *client = to_client(sd);
	int value, err;

	err = m7mu_readb2(client, M7MU_CATEGORY_AE,
		M7MU_AE_NOW_LV, &value);
	CHECK_ERR(err);

	ctrl->value = value;
	state->LV = ctrl->value;

	return ctrl->value;
}


static int m7mu_get_WBcustomX(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	struct m7mu_state *state = to_state(sd);
	struct i2c_client *client = to_client(sd);
	int value, value2, err, int_factor, int_en;
	int changed_capture_mode = false;

	CLEAR_ISP_INT1_STATE(sd);

	if (state->running_capture_mode != RUNNING_MODE_SINGLE) {
		err = m7mu_writeb(client, M7MU_CATEGORY_CAPCTRL,
				M7MU_CAPCTRL_CAP_MODE,
				M7MU_CAP_MODE_SINGLE_CAPTURE);
		CHECK_ERR(err);
		changed_capture_mode = true;
	}

	err = m7mu_writeb(client, M7MU_CATEGORY_WB,
		M7MU_WB_AWB_MODE, 0x02);
	CHECK_ERR(err);

	err = m7mu_writeb(client, M7MU_CATEGORY_WB,
		M7MU_WB_AWB_MANUAL, 0x08);
	CHECK_ERR(err);

	err = m7mu_writeb(client, M7MU_CATEGORY_WB,
		M7MU_WB_CWB_MODE, 0x02);
	CHECK_ERR(err);

	msleep(100);

	err = m7mu_writeb(client, M7MU_CATEGORY_SYS,
		M7MU_SYS_MODE, M7MU_STILLCAP_MODE);
	CHECK_ERR(err);

	err = m7mu_readw(client, M7MU_CATEGORY_SYS,
				M7MU_SYS_INT_EN, &int_en);
	CHECK_ERR(err);

#if 0
	if (int_en & M7MU_INT_SOUND) {
		/* Clear Interrupt factor */
		int_factor = m7mu_wait_interrupt(sd, M7MU_SOUND_TIMEOUT);
		if (!(int_factor & M7MU_INT_SOUND)) {
			cam_warn("M7MU_INT_SOUND isn't issued, %#x\n",
					int_factor);
			return -ETIMEDOUT;
		}
	}
#endif

	int_factor = m7mu_wait_interrupt(sd, M7MU_ISP_TIMEOUT);
	if (!(int_factor & M7MU_INT_CAPTURE)) {
		cam_warn("M7MU_INT_CAPTURE isn't issued, %#x\n",
				int_factor);
		return -ETIMEDOUT;
	}

	CLEAR_ISP_INT1_STATE(sd);

	err = m7mu_set_mode(sd, M7MU_MONITOR_MODE);
	if (err <= 0) {
		cam_err("failed to set mode\n");
		return err;
	}

	err = m7mu_wait_interrupt(sd, M7MU_ISP_TIMEOUT);
	if (!(err & M7MU_INT_MODE)) {
		cam_err("firmware was erased?\n");
		return -ETIMEDOUT;
	}

	err = m7mu_readw(client, M7MU_CATEGORY_WB,
		M7MU_WB_GET_CUSTOM_RG, &value);
	CHECK_ERR(err);

	err = m7mu_readw(client, M7MU_CATEGORY_WB,
		M7MU_WB_GET_CUSTOM_BG, &value2);
	CHECK_ERR(err);

	/* prevent abnormal value, to be fixed by ISP */
	if (value == 0)
		value = 424;
	if (value2 == 0)
		value2 = 452;

	state->wb_custom_rg = value;
	state->wb_custom_bg = value2;

	err = m7mu_writeb(client, M7MU_CATEGORY_WB,
		M7MU_WB_CWB_MODE, 0x02);
	CHECK_ERR(err);

	err = m7mu_writew(client, M7MU_CATEGORY_WB,
		M7MU_WB_SET_CUSTOM_RG, state->wb_custom_rg);
	CHECK_ERR(err);
	err = m7mu_writew(client, M7MU_CATEGORY_WB,
		M7MU_WB_SET_CUSTOM_BG, state->wb_custom_bg);
	CHECK_ERR(err);

	err = m7mu_writeb(client, M7MU_CATEGORY_WB,
		M7MU_WB_CWB_MODE, 0x01);
	CHECK_ERR(err);

	if (changed_capture_mode)
		m7mu_set_capture_mode(sd, state->running_capture_mode);

	cam_trace("X value : %d value2 : %d\n", value, value2);

	return value;
}

static int m7mu_get_WBcustomY(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	struct m7mu_state *state = to_state(sd);

	return state->wb_custom_bg;
}

static int m7mu_get_face_detect_number(struct v4l2_subdev *sd,
	struct v4l2_control *ctrl)
{
	struct m7mu_state *state = to_state(sd);
	struct i2c_client *client = to_client(sd);
	int value, err;

	err = m7mu_readb2(client, M7MU_CATEGORY_FD, 0x0A, &value);
	CHECK_ERR(err);

	state->fd_num = value;

#if 0
	cam_trace("X %d\n", value);
#endif

	return value;
}

static int m7mu_get_factory_FW_info(struct v4l2_subdev *sd,
	struct v4l2_control *ctrl)
{
	int err = 0;
	u32 read_val1, read_val2;
	u32 ver = 0;
	struct i2c_client *client = to_client(sd);

	m7mu_readb(client, M7MU_CATEGORY_SYS,
			0x02, &read_val1);
	CHECK_ERR(err);

	m7mu_readb(client, M7MU_CATEGORY_SYS,
		0x03, &read_val2);
	CHECK_ERR(err);

	ver = 0;
	ver = (read_val1 << 8) | (read_val2);

	cam_trace("m7mu_get_factory_FW : 0x%x\n", ver);
	ctrl->value = ver;

	return 0;
}

static int m7mu_get_factory_OIS_info(struct v4l2_subdev *sd,
	struct v4l2_control *ctrl)
{
	int err;
	u32 ver = 0;
	struct i2c_client *client = to_client(sd);

	err = m7mu_readl(client, M7MU_CATEGORY_NEW,
			0x1B, &ver);
	CHECK_ERR(err);

	cam_trace("m7mu_get_factory_FW : 0x%x\n", ver);
	ctrl->value = ver;

	return 0;
}

static int m7mu_get_factory_flash_charge(struct v4l2_subdev *sd,
	struct v4l2_control *ctrl)
{
	int err, val;
	struct i2c_client *client = to_client(sd);

	cam_trace("E\n");

	err = m7mu_readb(client, M7MU_CATEGORY_CAPPARM,
			M7MU_CAPPARM_STROBE_CHARGE, &val);
	CHECK_ERR(err);

	cam_trace("X : %d\n", val);

	return val;
}

static int m7mu_get_check_sum(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	int err, end_check = 0;
	struct i2c_client *client = to_client(sd);

	err = m7mu_readb(client, M7MU_CATEGORY_LENS,
		M7MU_LENS_CHECK_SUM, &end_check);
	CHECK_ERR(err);
	cam_trace("CHECK_SUM val = 0x%x\n", end_check);
	return end_check;
}

static int m7mu_get_focus_check(struct v4l2_subdev *sd,
		struct v4l2_control *ctrl)
{
	int err, focus_check = 0;
	struct i2c_client *client = to_client(sd);

	err = m7mu_readb(client, M7MU_CATEGORY_LENS,
		M7MU_LENS_AF_FOCUS_CHECK, &focus_check);
	CHECK_ERR(err);
	cam_trace("CHECK_SUM val = 0x%x\n", focus_check);
	return focus_check;
}

static int m7mu_get_factory_mem_compare(struct v4l2_subdev *sd)
{
	int err;
	int result = 0;
	struct i2c_client *client = to_client(sd);

	cam_trace("V4L2_CID_FACTORY_MEM_COMPARE\n");
	err = m7mu_writeb(client, M7MU_CATEGORY_LENS,
		M7MU_PARAM_MEMORY_COMPARE, 0x1);   /* memory compare request */
	CHECK_ERR(err);
	msleep(40);
	result = 0;
	err = m7mu_readb(client, M7MU_CATEGORY_LENS,
		M7MU_PARAM_MEMORY_COMPARE, &result);   /* receive result */
	CHECK_ERR(err);

	return result;
}

static int m7mu_g_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	struct m7mu_state *state = to_state(sd);
	struct i2c_client *client = to_client(sd);
	int err = 0;
	u32 val = 0;
	int grp_type = 0;

	/* V4L2 ID check */
	if ((s32)((ctrl->id & 0xFFFFFFF) - V4L2_CID_PRIVATE_BASE) > 0) {
		int g_cid = (ctrl->id & 0xFFFFFFF) - V4L2_CID_PRIVATE_BASE;
		/*
		   90 = zoom, 544 = fd_info1, 545 = fd_info2, 315 = lens status
		 */
		if (g_cid != 90 && g_cid != 544 &&
				g_cid != 545 && g_cid != 315) {
#if 0
			cam_info("ID : V4L2_CID_PRIVATE_BASE + %d, value = %d",
					ctrl->id - V4L2_CID_PRIVATE_BASE,
					ctrl->value);
#endif
		}
		grp_type = V4L2_CID_PRIVATE_BASE;
	} else if ((s32)((ctrl->id & 0xFFFFFF) - V4L2_CID_SENSOR_BASE) > 0) {
		int g_cid = (ctrl->id & 0xFFFFFF) - V4L2_CID_SENSOR_BASE;
		if (g_cid != 40 && g_cid != 41) { /* Removed smart read log. */
#if 0
			cam_info("ID : V4L2_CID_SENSOR_BASE + %d, value = %d",
				(ctrl->id & 0xFFFFFF) - V4L2_CID_SENSOR_BASE,
				ctrl->value);
#endif
		}
		grp_type = V4L2_CID_SENSOR_BASE;
	} else if ((s32)((ctrl->id & 0xFFFFFF) -
				V4L2_CID_CAMERA_CLASS_BASE) > 0) {
#if 0
		cam_info("ID : V4L2_CID_CAMERA_CLASS_BASE + %d, value = %d",
			ctrl->id - V4L2_CID_CAMERA_CLASS_BASE, ctrl->value);
#endif
		grp_type = V4L2_CID_CAMERA_CLASS_BASE;
	} else if ((u32)((ctrl->id & 0xFFFFFF) -
				V4L2_CTRL_CLASS_CAMERA) > 0) {
		int g_cid2 = ctrl->id - V4L2_CTRL_CLASS_CAMERA;
		if (g_cid2 != 124125274 && g_cid2 != 124125499) {
#if 0
			cam_info("ID : V4L2_CTRL_CLASS_CAMERA + %d, value = %d",
				ctrl->id - V4L2_CTRL_CLASS_CAMERA, ctrl->value);
#endif
		}
		grp_type = V4L2_CTRL_CLASS_CAMERA;
	} else {
		cam_err("no such control id %d, value %d\n",
				ctrl->id, ctrl->value);
	}

	switch (ctrl->id) {
	case V4L2_CID_CAM_AUTO_FOCUS_RESULT:
		m7mu_get_af_result(sd, ctrl);
		break;

	case V4L2_CID_CAMERA_FACTORY_OIS_DECENTER:
		m7mu_get_ois_decenter(sd, ctrl);
		break;

	case V4L2_CID_CAMERA_AUTO_FOCUS_DONE:
		m7mu_get_af_done(sd, ctrl);
		break;
	/*
	case V4L2_CID_CAM_JPEG_MEMSIZE:
		ctrl->value = M7MU_JPEG_MAXSIZE +
			M7MU_THUMB_MAXSIZE + M7MU_POST_MAXSIZE;
		break;
	*/
	case V4L2_CID_CAM_JPEG_MAIN_SIZE:
		ctrl->value = state->jpeg.main_size;
		cam_info("V4L2_CID_CAM_JPEG_MAIN_SIZE-----ctrl->value = %d ",
				ctrl->value);
		break;

	case V4L2_CID_CAM_JPEG_MAIN_OFFSET:
		ctrl->value = state->jpeg.main_offset;
		break;

	case V4L2_CID_CAM_JPEG_THUMB_SIZE:
		ctrl->value = state->jpeg.thumb_size;
		break;

	case V4L2_CID_CAM_JPEG_THUMB_OFFSET:
		ctrl->value = state->jpeg.thumb_offset;
		break;

	case V4L2_CID_CAM_JPEG_POSTVIEW_OFFSET:
		ctrl->value = state->jpeg.postview_offset;
		break;

	case V4L2_CID_CAMERA_EXIF_FLASH:
		ctrl->value = state->exif.flash;
		break;

	case V4L2_CID_CAMERA_EXIF_ISO:
		ctrl->value = state->exif.iso;
		break;

	case V4L2_CID_CAMERA_EXIF_TV:
		ctrl->value = state->exif.tv;
		break;

	case V4L2_CID_CAMERA_EXIF_BV:
		ctrl->value = state->exif.bv;
		break;

	case V4L2_CID_CAMERA_EXIF_AV:
		ctrl->value = state->exif.av;
		break;

	case V4L2_CID_CAMERA_EXIF_EBV:
		ctrl->value = state->exif.ebv;
		break;

	case V4L2_CID_CAMERA_EXIF_FL:
		ctrl->value = state->exif.focal_length;
		break;

	case V4L2_CID_CAMERA_EXIF_FL_35mm:
		ctrl->value = state->exif.focal_35mm_length;
		break;

	case V4L2_CID_CAMERA_FD_EYE_BLINK_RESULT:
		ctrl->value = state->fd_eyeblink_cap;
		break;

	case V4L2_CID_CAMERA_RED_EYE_FIX_RESULT:
		ctrl->value = state->fd_red_eye_status;
		break;

	case V4L2_CID_CAMERA_SCENE_MODE:
		err = m7mu_get_scene_mode(sd, ctrl);
		break;

	case V4L2_CID_CAMERA_SCENE_SUB_MODE:
		err = m7mu_get_scene_sub_mode(sd, ctrl);
		break;

	case V4L2_CID_CAMERA_FACTORY_DOWN_RESULT:
		ctrl->value = state->factory_down_check;
		break;

	case V4L2_CID_CAMERA_FACTORY_AF_INT_RESULT:
		ctrl->value = state->factory_result_check;
		break;

	case V4L2_CID_CAMERA_FACTORY_END_RESULT:
		ctrl->value = state->factory_end_check;
		if (0 != ctrl->value) {
			cam_trace("leesm test ----- factory_end_check %d\n",
				ctrl->value);
		}
		break;

	case V4L2_CID_CAMERA_ZOOM:
		err = m7mu_get_zoom_status(sd, ctrl);
		break;

	case V4L2_CID_CAMERA_OPTICAL_ZOOM_CTRL:
		err = m7mu_get_zoom_level(sd, ctrl);
		break;

	case V4L2_CID_CAMERA_MF:
		err = m7mu_get_manual_focus(sd, ctrl);
		break;

	case V4L2_CID_CAM_FLASH_MODE:
		err = m7mu_get_flash_status(sd, ctrl);
		break;

	case V4L2_CID_CAMERA_OBJ_TRACKING_STATUS:
		err = m7mu_get_object_tracking(sd, ctrl);
		ctrl->value = state->ot_status;
		break;

	case V4L2_CID_CAMERA_AV:
		ctrl->value = m7mu_get_av(sd, ctrl);
		break;

	case V4L2_CID_CAMERA_TV:
		ctrl->value = m7mu_get_tv(sd, ctrl);
		break;

	case V4L2_CID_CAMERA_SV:
		ctrl->value = m7mu_get_sv(sd, ctrl);
		break;

	case V4L2_CID_CAMERA_EV:
		ctrl->value = m7mu_get_ev(sd, ctrl);
		break;

	case V4L2_CID_CAMERA_LV:
		ctrl->value = m7mu_get_lv(sd, ctrl);
		break;

	case V4L2_CID_CAMERA_WB_CUSTOM_X:
		ctrl->value = m7mu_get_WBcustomX(sd, ctrl);
		break;

	case V4L2_CID_CAMERA_WB_CUSTOM_Y:
		ctrl->value = m7mu_get_WBcustomY(sd, ctrl);
		break;

	case V4L2_CID_CAMERA_GET_MODE:
		err = m7mu_readb(client, M7MU_CATEGORY_SYS,
					M7MU_SYS_MODE, &val);
		if (err < 0)
			ctrl->value = -1;
		else
			ctrl->value = val;
		break;

	case V4L2_CID_CAMERA_SMART_READ1:
		m7mu_get_smart_read1(sd, ctrl);
		break;

	case V4L2_CID_CAMERA_SMART_READ2:
		m7mu_get_smart_read2(sd, ctrl);
		break;

	case V4L2_CID_CAMERA_LENS_STATUS:
		m7mu_get_lens_status(sd, ctrl);
		break;

	case V4L2_CID_CAMERA_WARNING_CONDITION:
		ctrl->value = m7mu_get_warning_condition(sd, ctrl);
		break;

    /* for NSM Mode */
	case V4L2_CID_CAMERA_NSM_FD_INFO:
		ctrl->value = m7mu_get_Nsm_fd_info(sd, ctrl);
		break;
    /* end NSM Mode */

	case V4L2_CID_CAMERA_FACTORY_ISP_FW_CHECK:
		err = m7mu_get_factory_FW_info(sd, ctrl);
		break;

	case V4L2_CID_CAMERA_FACTORY_OIS_VER_CHECK:
		err = m7mu_get_factory_OIS_info(sd, ctrl);
		break;

	case V4L2_CID_CAMERA_FACE_DETECT_NUMBER:
		ctrl->value = m7mu_get_face_detect_number(sd, ctrl);
		break;

	case V4L2_CID_CAMERA_FACTORY_FLASH_CHARGE:
		ctrl->value = m7mu_get_factory_flash_charge(sd, ctrl);
		CHECK_ERR(err);
		break;

	case V4L2_CID_CAMERA_FW_CHECKSUM_VAL:
		ctrl->value = state->fw_checksum_val;
		break;

	case V4L2_CID_CAMERA_FACTORY_CHECK_SUM:
		ctrl->value = m7mu_get_check_sum(sd, ctrl);
		break;

	case V4L2_CID_CAMERA_FACTORY_FOCUS_CHECK:
		ctrl->value = m7mu_get_focus_check(sd, ctrl);
		break;

	case V4L2_CID_EXIF_SHUTTER_SPEED_NUM:
		ctrl->value = state->exif.shutter_speed_num;
		break;

	case V4L2_CID_EXIF_SHUTTER_SPEED_DEN:
		ctrl->value = state->exif.shutter_speed_den;
		break;

	case V4L2_CID_EXIF_EXPOSURE_TIME_NUM:
		ctrl->value = state->exif.exptime_num;
		break;

	case V4L2_CID_EXIF_EXPOSURE_TIME_DEN:
		ctrl->value = state->exif.exptime_den;
		break;

	case V4L2_CID_EXIF_F_NUMBER:
		ctrl->value = state->exif.fnumber;
		break;

	case V4L2_CID_CAMERA_SMART_SELF_SHOT_FD_INFO1:
#if 0
		cam_trace("V4L2_CID_CAMERA_SMART_SELF_SHOT_FD_INFO1\n");
#endif
		m7mu_get_SMART_SELF_SHOT_FD_INFO1(sd, ctrl);
		break;

	case V4L2_CID_CAMERA_SMART_SELF_SHOT_FD_INFO2:
#if 0
		cam_trace("V4L2_CID_CAMERA_SMART_SELF_SHOT_FD_INFO2\n");
#endif
		m7mu_get_SMART_SELF_SHOT_FD_INFO2(sd, ctrl);
		break;

	case V4L2_CID_CAMERA_FACTORY_GYRO:
		ctrl->value = state->factory_gyro_value;
		break;

	case V4L2_CID_FACTORY_MEM_COMPARE:
		ctrl->value = m7mu_get_factory_mem_compare(sd);
		break;

	case V4L2_CID_CAMERA_MAIN_FACE_DETECT:
#if 0
		err = m7mu_readb(client, M7MU_CATEGORY_SYS,
					M7MU_SYS_MODE, &val);
		ctrl->value = val;
#else
		ctrl->value = 0;
#endif

		break;

	default:
		break;
	}

	if (err < 0 && err != -ENOIOCTLCMD)
		cam_err("failed, id %d, value %d\n",
				ctrl->id - grp_type, ctrl->value);

	return err;
}

static int m7mu_set_antibanding(struct v4l2_subdev *sd,
		struct v4l2_control *ctrl)
{
	struct v4l2_queryctrl qc = {0,};
	struct m7mu_state *state = to_state(sd);
	int val = ctrl->value, err;
	u32 antibanding[] = {0x00, 0x01, 0x02, 0x03, 0x04};
	struct i2c_client *client = to_client(sd);

	if (state->anti_banding == val)
		return 0;

	cam_dbg("E, value %d\n", val);

	qc.id = ctrl->id;
	m7mu_queryctrl(sd, &qc);

	if (val < qc.minimum || val > qc.maximum) {
		cam_warn("invalied value, %d\n", val);
		val = qc.default_value;
	}

	val -= qc.minimum;

	/* Auto flickering is always used */
	err = m7mu_writeb(client, M7MU_CATEGORY_AE,
					M7MU_AE_FLICKER, antibanding[val]);
	CHECK_ERR(err);

	state->anti_banding = val;

	cam_trace("X\n");
	return 0;
}

static int m7mu_set_lens_off(struct v4l2_subdev *sd)
{
	struct m7mu_state *state = to_state(sd);
	struct i2c_client *client = to_client(sd);
	int err = 0;
	int value;
	int cnt = 3,  cnt2 = 500;

	cam_trace("E\n");

	if (state->factory_no_lens_off) {
		/* 0 : zoom open, 1 : zoom close (default : 1)*/
		m7mu_writeb(client, M7MU_CATEGORY_NEW,
			M7MU_NEW_LENS_OFF, 0x00);
		return 0;
	}

	if (unlikely(state->isp.bad_fw)) {
		cam_err("\"Unknown\" state, please update F/W");
		return -ENOSYS;
	}

	if (m7mu_Lens_Off_Needed == 0) {
		cam_err("Lens close. Already\n");
		return err;
	}

	if (m7mu_which_power_off() != 0) {
		cam_err("don't need lens off\n");
		return err;
	}

	err = m7mu_writeb(client, M7MU_CATEGORY_LENS,
		M7MU_LENS_AF_LENS_CLOSE, 0x00);
	CHECK_ERR(err);

	m7mu_Lens_Off_Needed = 0;

	/* use polling method instead of ISR check */
	msleep(200);

	err = m7mu_readb(client, M7MU_CATEGORY_LENS,
		M7MU_LENS_LENS_STATUS, &value);
	CHECK_ERR(err);

	while (value != 4 && cnt) {
		err = m7mu_writeb(client, M7MU_CATEGORY_LENS,
			M7MU_LENS_AF_LENS_CLOSE, 0x00);
		CHECK_ERR(err);

		msleep(200);

		err = m7mu_readb(client, M7MU_CATEGORY_LENS,
			M7MU_LENS_LENS_STATUS, &value);
		CHECK_ERR(err);

		if (value == 4)
			break;

		cnt--;
	}

	while (value == 4 && cnt2) {
		msleep(20);
		err = m7mu_readb(client, M7MU_CATEGORY_LENS,
			M7MU_LENS_LENS_STATUS, &value);
		CHECK_ERR(err);

		if (value != 4 && value == 0)
			break;

		cnt2--;
	}

	if (value != 0)
		return -1;

	cam_trace("X\n");
	return err;
}

static int m7mu_dump_fw(struct v4l2_subdev *sd)
{
	struct file *fp;
	mm_segment_t old_fs;
	u8 *buf/*, val*/;
	u32 addr, unit, count;
	u32 intram_unit = 0x1000;
	int i, /*j,*/ err;
	struct i2c_client *client = to_client(sd);

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	fp = filp_open(M7MU_FW_DUMP_PATH,
		O_WRONLY|O_CREAT|O_TRUNC, S_IRUGO|S_IWUGO|S_IXUSR);
	if (IS_ERR(fp)) {
		cam_err("failed to open %s, err %ld\n",
			M7MU_FW_DUMP_PATH, PTR_ERR(fp));
		err = -ENOENT;
		goto file_out;
	}

	buf = kmalloc(intram_unit, GFP_KERNEL);
	if (!buf) {
		cam_err("failed to allocate memory\n");
		err = -ENOMEM;
		goto out;
	}

	cam_dbg("start, file path %s\n", M7MU_FW_DUMP_PATH);


/*
	val = 0x7E;
	err = m7mu_mem_write(sd, 0x04, sizeof(val), 0x50000308, &val);
	if (err < 0) {
		cam_err("failed to write memory\n");
		goto out;
	}
*/


	err = m7mu_mem_write(sd, 0x04, SZ_64,
				0x90001200 , buf_port_seting0);
	if (err < 0) {
		cam_err("i2c falied, err %d\n", err);
		goto out;
	}
	mdelay(10);

	err = m7mu_mem_write(sd, 0x04, SZ_64,
				0x90001000 , buf_port_seting1);
	if (err < 0) {
		cam_err("i2c falied, err %d\n", err);
		goto out;
	}
	mdelay(10);

	err = m7mu_mem_write(sd, 0x04, SZ_64,
				0x90001100 , buf_port_seting2);
	if (err < 0) {
		cam_err("i2c falied, err %d\n", err);
		goto out;
	}
	mdelay(10);

	err = m7mu_writel(client, M7MU_CATEGORY_FLASH,
				0x1C, 0x0247036D);

	err = m7mu_writeb(client, M7MU_CATEGORY_FLASH,
				0x57, 01);
	if (err < 0) {
		cam_err("i2c falied, err %d\n", err);
		goto out;
	}


	addr = M7MU_FLASH_READ_BASE_ADDR;
	unit = SZ_4K;
	count = 1024;
	for (i = 0; i < count; i++) {
			err = m7mu_mem_dump(sd,
				unit, addr + (i * unit), buf);
			cam_err("dump ~~ %d\n", i);
			if (err < 0) {
				cam_err("i2c falied, err %d\n", err);
				goto out;
			}
			vfs_write(fp, buf, unit, &fp->f_pos);
	}
/*
	addr = M7MU_FLASH_BASE_ADDR + SZ_64K * count;
	unit = SZ_8K;
	count = 4;
	for (i = 0; i < count; i++) {
		for (j = 0; j < unit; j += intram_unit) {
			err = m7mu_mem_read(sd,
				intram_unit, addr + (i * unit) + j, buf);
			if (err < 0) {
				cam_err("i2c falied, err %d\n", err);
				goto out;
			}
			vfs_write(fp, buf, intram_unit, &fp->f_pos);
		}
	}
*/
	cam_dbg("end\n");

out:
	kfree(buf);
	if (!IS_ERR(fp) && fp != NULL)
		filp_close(fp, current->files);
file_out:
	set_fs(old_fs);

	return err;
}

static int m7mu_get_sensor_fw_version(struct v4l2_subdev *sd)
{
	struct m7mu_state *state = to_state(sd);
	struct i2c_client *client = to_client(sd);
	int err = 0;
	int fw_ver = 0x00;
	int awb_ver = 0x00;
	int af_ver = 0x00;
	int ois_ver = 0x00;
	int parm_ver = 0x00;
	int user_ver_temp;
	char user_ver[M7MU_FW_VER_LEN + 1] = {'\0',};
	char user_tmp[M7MU_FW_VER_LEN + 1] = {'\0',};
	char sensor_ver[M7MU_FW_VER_TOKEN + 1] = {'\0',};
	int i = 0;

	cam_err("E\n");
	memset(user_ver, 0, sizeof(user_ver));
	memset(user_tmp, 0, sizeof(user_tmp));
	memset(sensor_ver, 0, sizeof(sensor_ver));

	memset(sysfs_sensor_type, 0, sizeof(sysfs_sensor_type));
	memset(sysfs_sensor_fw, 0, sizeof(sysfs_sensor_fw));

	/* read F/W version */
	err = m7mu_readw(client, M7MU_CATEGORY_SYS,
		M7MU_SYS_VER_FW, &fw_ver);
	CHECK_ERR(err);

	err = m7mu_readw(client, M7MU_CATEGORY_SYS,
		M7MU_SYS_VER_AWB, &awb_ver);
	CHECK_ERR(err);

	err = m7mu_readw(client, M7MU_CATEGORY_SYS,
		M7MU_SYS_VER_PARAM, &parm_ver);
	CHECK_ERR(err);

	err = m7mu_readl(client, M7MU_CATEGORY_LENS,
		M7MU_LENS_AF_VERSION, &af_ver);
	CHECK_ERR(err);

	err = m7mu_readl(client, M7MU_CATEGORY_NEW,
		M7MU_NEW_OIS_VERSION, &ois_ver);
	CHECK_ERR(err);

	for (i = 0; i < M7MU_FW_VER_LEN; i++) {
		err = m7mu_readb(client, M7MU_CATEGORY_SYS,
				M7MU_SYS_USER_VER, &user_ver_temp);
		CHECK_ERR(err);

		user_tmp[i] = (char)user_ver_temp;

		if ((char)user_ver_temp == '\0')
			break;
	}

	memcpy(user_ver, &user_tmp[4], M7MU_FW_VER_TOKEN);

	/* user_ver[M7MU_FW_VER_LEN] = '\0'; */

/*	if (user_ver[0] == 'F' && user_ver[1] == 'C') {
		for (i = 0; i < M7MU_FW_VER_TOKEN; i++) {
			if (user_ver[i] == 0x20) {
				sensor_ver[M7MU_FW_VER_TOKEN] = '\0';
				break;
			}
			sensor_ver[i] = user_ver[i];
		}
	} else {
		sprintf(sensor_ver, "%s", "Invalid version");
	}	*/

	cam_info("f/w version = %x\n", fw_ver);
	cam_info("awb version = %x\n", awb_ver);
	cam_info("af version = %x\n", af_ver);
	cam_info("ois version = %x\n", ois_ver);
	cam_info("parm version = %x\n", parm_ver);
	cam_info("user version = %s\n", user_ver);
	/*cam_info("sensor version = %s\n", sensor_ver);

	snprintf(state->sensor_ver, M7MU_FW_VER_TOKEN, "%s",
			sensor_ver);
	snprintf(state->sensor_type, M7MU_SENSOR_TYPE_LEN,
			"%d %d %d %x", awb_ver, af_ver, ois_ver, parm_ver);
	memcpy(sysfs_sensor_fw, state->sensor_ver,
			sizeof(state->sensor_ver));
	memcpy(sysfs_sensor_type, state->sensor_type,
			sizeof(state->sensor_type));
	state->isp_fw_ver = fw_ver;	*/

	memset(state->sensor_type, 0, sizeof(state->sensor_type));
	snprintf(state->sensor_type, M7MU_SENSOR_TYPE_LEN,
			"%d %d %d %x", awb_ver, af_ver, ois_ver, parm_ver);

	memset(sysfs_sensor_type, 0, sizeof(sysfs_sensor_type));
	if (sizeof(state->sensor_type) >= M7MU_SENSOR_TYPE_LEN)
		memcpy(sysfs_sensor_type, state->sensor_type,
				sizeof(state->sensor_type));

	memset(state->sensor_ver, 0, sizeof(state->sensor_ver));
	if (sizeof(state->sensor_ver) >= M7MU_FW_VER_TOKEN)
		memcpy(state->sensor_ver, user_ver, M7MU_FW_VER_TOKEN);

	memset(sysfs_sensor_fw, 0, sizeof(sysfs_sensor_fw));
	if (sizeof(sysfs_sensor_fw) >= M7MU_FW_VER_TOKEN)
		memcpy(sysfs_sensor_fw, user_ver, M7MU_FW_VER_TOKEN);

	state->isp_fw_ver = fw_ver;

	cam_info("sensor fw : %s\n", sysfs_sensor_fw);
	cam_info("sensor type : %s\n", sysfs_sensor_type);
	return 0;
}

#if defined(M7MU_ENABLE_ISP_FIRMWARE_UPDATE)
static int m7mu_get_phone_fw_version(struct v4l2_subdev *sd)
{
	struct m7mu_state *state = to_state(sd);
	struct device *dev = sd->v4l2_dev->dev;
	const struct firmware *fw = NULL;
	const struct m7mu_fw_header *hdr = &g_fw_header;
	int err = 0;

	struct file *fp = NULL;
	mm_segment_t old_fs;
	long nread;
	int fw_requested = 0;
	char ver_tmp[M7MU_FW_VER_LEN + 1];
	char phone_ver[M7MU_FW_VER_TOKEN + 1];
	int version_offset = 0;
	int retry_cnt = 2;
	int i = 0;

	memset(ver_tmp, 0, sizeof(ver_tmp));
	memset(phone_ver, 0, sizeof(phone_ver));

	cam_info("E\n");

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	fp = filp_open(M7MU_FW_PATH, O_RDONLY, 0);

	if (IS_ERR(fp)) {
		cam_trace("failed to open %s, err %ld\n",
				M7MU_FW_PATH,  PTR_ERR(fp));
		fw_requested = 1;
fw_retry:
		err = request_firmware(&fw, M7MU_FW_REQ_PATH, dev);
		if (err == 0) {
			cam_info("FW file(/system/vendor/firmware) opened.\n");
			mmc_fw = 0;

			version_offset = (int)&hdr->code.bit.version_2[0] -
				(int)hdr;
			cam_info("Get FW version offt:%d\n",
					(int)&hdr->code.bit.version_2[0] -
					(int)hdr);

			for (i = 0 ; i < M7MU_FW_VER_LEN ; i++)
				ver_tmp[i] = (char)fw->data[version_offset + i];

			ver_tmp[M7MU_FW_VER_LEN] = '\0';

			memcpy(phone_ver, ver_tmp, M7MU_FW_VER_TOKEN);

			cam_info("ver_tmp = %s\n", ver_tmp);
			cam_info("phone_ver = %s\n", phone_ver);

			memset(state->phone_ver, 0, sizeof(state->phone_ver));
			if (sizeof(state->phone_ver) >= sizeof(phone_ver))
				memcpy(state->phone_ver, phone_ver,
						sizeof(phone_ver));

			memset(sysfs_phone_fw, 0, sizeof(sysfs_phone_fw));
			if (sizeof(sysfs_phone_fw) >= sizeof(phone_ver))
				memcpy(sysfs_phone_fw, phone_ver,
						sizeof(phone_ver));

			goto out;

		} else {
			cam_err("request_firmware falied\n");

			if (retry_cnt > 0) {
				retry_cnt--;
				msleep(20);
				cam_err("request_firmware retry %d\n",
						retry_cnt);
				goto fw_retry;
			}

			goto out;
		}
	} else {
		cam_info("FW File(/sdcard) opened.\n");
		mmc_fw = 1;
	}

	version_offset = (int)&hdr->code.bit.version_2[0] - (int)hdr;
	cam_info("Get FW version offt:%d\n",
			(int)&hdr->code.bit.version_2[0] - (int)hdr);

	err = vfs_llseek(fp, version_offset, SEEK_SET);
	if (err < 0) {
		cam_warn("failed to fseek, %d\n", err);
		goto out;
	}

	nread = vfs_read(fp, (char __user *)ver_tmp,
			M7MU_FW_VER_LEN, &fp->f_pos);
	if (nread != M7MU_FW_VER_LEN) {
		cam_err("failed to read firmware file, %ld Bytes\n", nread);
		err = -EIO;
		goto out;
	}

	memcpy(phone_ver, ver_tmp, M7MU_FW_VER_TOKEN);

	cam_info("ver_tmp = %s\n", ver_tmp);
	cam_info("phone_ver = %s\n", phone_ver);

	memset(state->phone_ver, 0, sizeof(state->phone_ver));
	if (sizeof(state->phone_ver) >= sizeof(phone_ver))
		memcpy(state->phone_ver, phone_ver, sizeof(phone_ver));

	memset(sysfs_phone_fw, 0, sizeof(sysfs_phone_fw));
	if (sizeof(sysfs_phone_fw) >= sizeof(phone_ver))
		memcpy(sysfs_phone_fw, phone_ver, sizeof(phone_ver));

	if (mmc_fw == 2) {
		if (strncmp(&ver_tmp[0], "D", 1) ||
				strncmp(&ver_tmp[1], "2", 1))  {
			/* (!IS_ERR(fp) && fp != NULL)
				filp_close(fp, current->files);	*/
			strcpy(sysfs_phone_fw, "Nothing");
			goto out;
		}
	}

out:
	if (!fw_requested) {
		if (!IS_ERR(fp) && fp != NULL)
			filp_close(fp, current->files);
		set_fs(old_fs);
	} else {
		if (!err && (fw != NULL))
			release_firmware(fw);
		else
			cam_err("request_firmware is failed. skip release.\n");
	}

	cam_dbg("phone ver : %s\n", sysfs_phone_fw);
	return 0;
}
#endif /* M7MU_ENABLE_ISP_FIRMWARE_UPDATE */

#if 0
static int m7mu_check_checksum(struct v4l2_subdev *sd)
{
	struct m7mu_state *state = to_state(sd);
	int checksum_value, value, err, init_value;
	int cnt = 100;

	cam_trace("E\n");

	err = m7mu_readl(client, M7MU_CATEGORY_FLASH,
			0x00, &init_value);
	CHECK_ERR(err);

	err = m7mu_writel(client, M7MU_CATEGORY_FLASH,
			0x00, 0x00);
	CHECK_ERR(err);

	err = m7mu_writeb(client, M7MU_CATEGORY_FLASH,
			0x09, 0x04);
	CHECK_ERR(err);

	err = m7mu_readb(client, M7MU_CATEGORY_FLASH,
			0x09, &value);
	CHECK_ERR(err);

	while (value == 4 && cnt) {
		msleep(100);
		err = m7mu_readb(client, M7MU_CATEGORY_FLASH,
				0x09, &value);
		CHECK_ERR(err);

		if (value == 0)
			break;

		cnt--;
	}

	err = m7mu_readw(client, M7MU_CATEGORY_FLASH,
			0x0A, &checksum_value);
	CHECK_ERR(err);

	cam_trace("X %d\n", checksum_value);

	if (checksum_value == 0x0) {
		state->fw_checksum_val = 1;
		return 1;
	} else {
		state->fw_checksum_val = 0;
		return 0;
	}
}
#endif
static int m7mu_set_factory_data_erase(struct v4l2_subdev *sd, int val)
{
	int err;
	/*
	struct m7mu_state *state = to_state(sd);
	 */
	struct i2c_client *client = to_client(sd);

	cam_trace("E val : %d\n", val);

	switch (val) {
	case FACTORY_DATA_ERASE_PUNT:
		cam_trace("~ FACTORY_PUNT_DATA_ERASE ~\n");
		err = m7mu_writeb(client, M7MU_CATEGORY_LENS,
			0x0E, 0xff);
		CHECK_ERR(err);
		break;

	case FACTORY_DATA_ERASE_BACKLASH:
		cam_trace("~ FACTORY_BACKLASH_DATA_REASE ~\n");
		err = m7mu_writeb(client, M7MU_CATEGORY_LENS,
			0xAE, 0xff);
		CHECK_ERR(err);
		break;

	default:
		cam_err("~ m7mu_set_factory_data_erase  ~ val : %d", val);
		break;
	}

	cam_trace("X\n");
	return 0;
}
#if 0
static int m7mu_write_shd_data(struct v4l2_subdev *sd,
	u8 *buf, u32 unit, u32 count)
{
	u32 val;
	int erase = 0x01;
	int test_count = 0;
	int i = 0, err;
	struct i2c_client *client = to_client(sd);

	for (i = 0; i < 45; i++) {
		int retries;
		/* Set Flash ROM memory address */
		err = m7mu_writel(client, M7MU_CATEGORY_FLASH,
			M7MU_FLASH_ADDR, (0x204000+i*0x1000));

		/* Erase FLASH ROM entire memory */
		err = m7mu_writeb(client, M7MU_CATEGORY_FLASH,
			M7MU_FLASH_ERASE, erase);

		/* Response while sector-erase is operating */
		retries = 0;
		do {
			msleep(20);
			err = m7mu_readb(client, M7MU_CATEGORY_FLASH,
				M7MU_FLASH_ERASE, &val);
		} while (val == erase && retries++ < M7MU_I2C_VERIFY);

		if (val != 0) {
			cam_err("failed to erase sector\n");
			return -EFAULT;
		}

		/* Set FLASH ROM programming size */
		err = m7mu_writew(client, M7MU_CATEGORY_FLASH,
				M7MU_FLASH_BYTE, SZ_4K);
		CHECK_ERR(err);

		err = m7mu_mem_write(sd, 0x04, SZ_4K,
				M7MU_INT_RAM_BASE_ADDR, (buf + i*0x1000));
			CHECK_ERR(err);
		cam_err("fw Send = %x count = %d\n", i, test_count++);

		/* Start Programming */
		err = m7mu_writeb(client, M7MU_CATEGORY_FLASH,
				M7MU_FLASH_WR, 0x01);
		CHECK_ERR(err);

		/* Confirm programming has been completed */
		retries = 0;
		do {
			mdelay(30);
			err = m7mu_readb(client, M7MU_CATEGORY_FLASH,
				M7MU_FLASH_WR, &val);
			CHECK_ERR(err);
		} while (val && retries++ < M7MU_I2C_VERIFY);

		cam_err("exit :: retries = %d\n", retries);
		if (val != 0) {
			/* m7mu_dump_SHD(sd); */
			cam_err("failed to program~~~~\n");
			return -1;
		}
	}
	cam_err("err = %d\n", err);
	return err;
}
#endif

#if 0
static int m7mu_read_shd_data(struct v4l2_subdev *sd, int idx)
{
	/*
	struct m7mu_state *state = to_state(sd);
	struct device *dev = sd->v4l2_dev->dev;
	*/
	struct i2c_client *client = to_client(sd);
	u8 *buf_m7mu = NULL;
	unsigned int count = 0;
	struct file *fp;
	mm_segment_t old_fs;
	long fsize, nread;
	int err = 0;
	int data_len = 45*SZ_4K; /* 180*SZ_1K; */

	cam_trace("E idx=%d\n", idx);

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	fp = filp_open(M7MU_SHD_DATA_PATH, O_RDONLY, 0);
	if (fp == NULL || IS_ERR(fp)) {
		cam_trace("failed to open %s, err %ld\n",
			M7MU_SHD_DATA_PATH, PTR_ERR(fp));
		goto file_out;
	}

	fsize = fp->f_path.dentry->d_inode->i_size;
	count = fsize / SZ_4K;

	cam_err("start, file path %s, size %ld Bytes, count %d, index %d\n",
			M7MU_SHD_DATA_PATH, fsize, count, idx);

	cam_err("(fsize/data_len) %d\n", ((int)(fsize/data_len)));

	if (((int)(fsize / data_len)) <= idx) {
		cam_err("wrong index value.\n");
		err = -EINVAL;
		goto out;
	}

	buf_m7mu = vmalloc(data_len);
	if (!buf_m7mu) {
		cam_err("failed to allocate memory\n");
		err = -ENOMEM;
		goto out;
	}
	err = vfs_llseek(fp, data_len*idx, SEEK_SET);
	cam_trace("SEEK_SET err = %d\n", err);
	if (err < 0) {
		cam_warn("failed to fseek, %d\n", err);
		goto out;
	}

	nread = vfs_read(fp, (char __user *)buf_m7mu, data_len, &fp->f_pos);
	if (nread != data_len) {
		cam_err("failed to read SDH data file, %ld Bytes\n", nread);
		err = -EIO;
		goto out;
	}

	err = m7mu_mem_write(sd, 0x04, SZ_64,
			0x90001200 , buf_port_seting0);
	if (err < 0) {
		cam_err("i2c falied, err %d\n", err);
		goto out;
	}
	mdelay(10);

	err = m7mu_mem_write(sd, 0x04, SZ_64,
			0x90001000 , buf_port_seting1);
	if (err < 0) {
		cam_err("i2c falied, err %d\n", err);
		goto out;
	}
	mdelay(10);

	err = m7mu_mem_write(sd, 0x04, SZ_64,
			0x90001100 , buf_port_seting2);
	if (err < 0) {
		cam_err("i2c falied, err %d\n", err);
		goto out;
	}
	mdelay(10);

	/* set clock frequency dividing value */
	err = m7mu_writel(client, M7MU_CATEGORY_FLASH,
			0x1C, 0x0247036D);
	if (err < 0) {
		cam_err("i2c falied, err %d\n", err);
		goto out;
	}

	/* start RAM for write Flash ROM */
	err = m7mu_writeb(client, M7MU_CATEGORY_FLASH,
			0x4A, 0x01);
	if (err < 0) {
		cam_err("i2c falied, err %d\n", err);
		goto out;
	}
	mdelay(10);

	/* program FLASH ROM */
	err = m7mu_write_shd_data(sd, buf_m7mu, SZ_4K, count);
	if (err < 0)
		cam_err("i2c falied, err %d\n", err);

out:
	if (buf_m7mu != NULL)
		vfree(buf_m7mu);
	if (!IS_ERR(fp) && fp != NULL)
		filp_close(fp, current->files);

file_out:
	set_fs(old_fs);
	cam_trace("err = %d\n", err);

	return err;
}
#endif

#if 0
static int m7mu_set_shd_data(struct v4l2_subdev *sd, int idx)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	const struct m7mu_platform_data *pdata = client->dev.platform_data;
	struct m7mu_state *state = to_state(sd);
	int err = 0;

	cam_dbg("E\n");

	pdata->power_on_off(0);
	msleep(20);
	pdata->power_on_off(1);

	err = m7mu_read_shd_data(sd, idx);
	cam_err("err : %d - %s", err, err == 1 ? "success" : "fail");
	err == 1 ? (state->factory_end_check = 0x02) :
		(state->factory_end_check = 0x01);

	pdata->power_on_off(0);
	msleep(100);
	pdata->power_on_off(1);

	pdata->config_sambaz(0);
	msleep(1000);

	cam_dbg("X\n");
	return err;
}
#endif

static int m7mu_set_clip_value(struct v4l2_subdev *sd, int val)
{
	int err;
	struct m7mu_state *state = to_state(sd);
	struct i2c_client *client = to_client(sd);

	cam_trace("E val : %d:%02d:%02d\n", val,
			((val >> 8) & 0xFF), (val & 0xFF));

	err = m7mu_writeb(client, M7MU_CATEGORY_ADJST,
			M7MU_ADJST_CLIP_VALUE1, (val >> 8) & 0xFF);
	CHECK_ERR(err);

	err = m7mu_writeb(client, M7MU_CATEGORY_ADJST,
			M7MU_ADJST_CLIP_VALUE2, val & 0xFF);
	CHECK_ERR(err);

	err == 1 ? (state->factory_end_check = 0x02) :
		(state->factory_end_check = 0x01);

	cam_trace("X\n");
	return err;
}

static int m7mU_set_tilt_location(struct v4l2_subdev *sd, int val)
{
	int err;
	int value = 0;
	int mX = 0;
	int mY = 0;
	int mlocationX = 0;
	int mlocationY = 0;
	struct i2c_client *client = to_client(sd);

	cam_trace("E\n");
	value = (val >> 30 & 0x00000003);
	mX = (val & 0x3FFF0000) >> 16;
	mY = val & 0x0000FFFF;
	cam_trace("E val : %d:%02d:%02d\n", value, mX, mY);

	switch (value) {
	case FACTORY_TILT_LOCATION_LEFT_UP:
		mlocationX = 0xB0;
		mlocationY = 0xB2;
		break;
	case FACTORY_TILT_LOCATION_RIGHT_UP:
		mlocationX = 0xB4;
		mlocationY = 0xB6;
		break;
	case FACTORY_TILT_LOCATION_LEFT_DOWN:
		mlocationX = 0xB8;
		mlocationY = 0xBA;
		break;
	case FACTORY_TILT_LOCATION_RIGHT_DOWN:
		mlocationX = 0xBC;
		mlocationY = 0xBE;
		break;
	default:
		cam_err("~ m7mu_set_tilt_location  ~ error value: %d", value);
		mlocationX = 0xB0;
		mlocationY = 0xB2;
		mX = 0;
		mY = 0;
		break;
	}

	err = m7mu_writew(client, M7MU_CATEGORY_MON,
		mlocationX, mX);
	CHECK_ERR(err);
	err = m7mu_writew(client, M7MU_CATEGORY_MON,
		mlocationY, mY);
	CHECK_ERR(err);
	cam_trace("X\n");
	return err;
}

static int m7mu_check_fw(struct v4l2_subdev *sd)
{
#if 0
	struct m7mu_state *state = to_state(sd);
	int update_count = 0;
		u32 int_factor;
#endif

	int err = 0;

	cam_trace("E\n");

	/* F/W version */
#if 1
	err = m7mu_get_phone_fw_version(sd);
	if (err == -EIO)
		return err;
#endif
#if 0
	if (state->isp.bad_fw)
		goto out;
#endif

	m7mu_get_sensor_fw_version(sd);

	cam_info("phone ver = %s, sensor_ver = %s\n",
			sysfs_phone_fw, sysfs_sensor_fw);

	cam_trace("X\n");
	return err;
}

static int m7mu_wait_make_CSV_rawdata(struct v4l2_subdev *sd)
{
	int err, val = 0x00, cnt = 300;
	struct i2c_client *client = to_client(sd);

	cam_trace("E\n");

	while (cnt) {
		msleep(20);
		err = m7mu_readb(client, M7MU_CATEGORY_LENS,
			M7MU_LENS_AF_ADJ_OK_NG, &val);
		CHECK_ERR(err);

		if (val == 0x01 || val == 0x02) {
			cam_trace(":: write rawdata\n");
			break;
		} else {
			cam_err(":: not yet, cnt = %d\n", cnt);
			cnt--;
		}
	}
	cam_trace("X\n");
	return err;
}

static int m7mu_make_CSV_rawbin(struct v4l2_subdev *sd,
	u32 *address, bool bAddResult)
{
	struct file *fp;
	mm_segment_t old_fs;
	u8 *buf = NULL;
	u32 addr, unit;
	int err;
	u32 intram_unit = 0x1000;

	cam_trace("m7mu_make_CSV_rawbin()\n");

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	fp = filp_open(M7MU_FACTORY_CSV_PATH,
		O_WRONLY|O_CREAT|O_TRUNC, S_IRUGO|S_IWUGO|S_IXUSR);
	if (fp == NULL || IS_ERR(fp)) {
		cam_err("failed to open %s, err %ld\n",
			M7MU_FACTORY_CSV_PATH, PTR_ERR(fp));
		err = -ENOENT;
		goto file_out;
	}

	buf = kmalloc(intram_unit, GFP_KERNEL);
	if (!buf) {
		cam_err("failed to allocate memory\n");
		err = -ENOMEM;
		goto out;
	}

	cam_dbg("start, file path %s\n", M7MU_FACTORY_CSV_PATH);

	addr = address[0];
	unit = address[1]-address[0]+1;

	cam_trace("m7mu_make_CSV_rawbin() addr[0x%0x] size=%d\n",
		addr, unit);

	err = m7mu_eep_rw_start(sd, addr);
	if (err < 0) {
		cam_err("i2c falied, err %d\n", err);
		goto out;
	}
	mdelay(50);
	err = m7mu_mem_read(sd, unit, addr, buf);
	if (err < 0) {
		cam_err("i2c falied, err %d\n", err);
		goto out;
	}

/*"Result27E03128(0:OK, 1:NG)"Bit 0 : IRIS
Bit 1 : Liveview Gain
Bit 2 : ShutterClose
Bit 3 : CaptureGain
Bit 5 : DefectPixel*/
	if (bAddResult) {
		err = m7mu_eep_rw_start(sd, M7MU_FLASH_FACTORY_RESULT);
		if (err < 0) {
			cam_err("i2c falied, err %d\n", err);
			goto out;
		}
		mdelay(50);
		err = m7mu_mem_read(sd, 0x2,
			M7MU_FLASH_FACTORY_RESULT, buf+unit);
		if (err < 0) {
			cam_err("i2c falied, err %d\n", err);
			goto out;
		}
		cam_trace("m7mu_make_CSV_rawbin() size=%d  result=%x\n",
			unit, *(u16 *)(buf+unit));
		unit += 2;
	}

	vfs_write(fp, buf, unit, &fp->f_pos);
	msleep(20);

out:
	if (buf != NULL)
		kfree(buf);
	if (!IS_ERR(fp) && fp != NULL)
		filp_close(fp, current->files);
file_out:
	set_fs(old_fs);

	return err;
}

static int m7mu_make_CSV_rawdata(struct v4l2_subdev *sd,
	u32 *address, bool bAddResult)
{
	struct file *fp;
	mm_segment_t old_fs;
	u8 *buf = NULL;
	u32 addr, unit;
	int err;
	struct i2c_client *client = to_client(sd);
	u32 offHeader, sizheader, offContent, sizcontent, offRes;
	u8 parsingOK = 0;

	cam_trace("m7mu_make_CSV_rawdata() - file direct\n");
	old_fs = get_fs();
	set_fs(KERNEL_DS);

	fp = filp_open(M7MU_FACTORY_CSV_PATH,
		O_WRONLY|O_CREAT|O_TRUNC, S_IRUGO|S_IWUGO|S_IXUSR);
	if (fp == NULL || IS_ERR(fp)) {
		cam_err("failed to open %s, err %ld\n",
			M7MU_FACTORY_CSV_PATH, PTR_ERR(fp));
		err = -ENOENT;
		goto file_out;
	}

	err = m7mu_readl(client, M7MU_CATEGORY_SYS, 0x31, &addr);
	if (err < 0) {
		cam_err("i2c falied, err %d\n", err);
		goto out;
	}
	err = m7mu_readw(client, M7MU_CATEGORY_SYS, 0x35, &unit);
	if (err < 0) {
		cam_err("i2c falied, err %d\n", err);
		goto out;
	}

	buf = kmalloc(unit+3, GFP_KERNEL);
	if (!buf) {
		cam_err("failed to allocate memory\n");
		err = -ENOMEM;
		goto out;
	}

	cam_trace("m7mu_make_CSV_rawdata() addr[0x%0x] size=%d\n",
		addr, unit);

	/* Exception when CSV data can't be read */
	if (addr == 0 || unit == 0) {
		u8 tmp[55];
		cam_err("CSV data addr[0x%X] or size[%d] error\n", addr, unit);
		/* write header */
		sprintf(tmp, "CSV READ FAIL addr:0x%X, size:%d\r\n2",
				addr, unit);
		vfs_write(fp, tmp, strlen(tmp), &fp->f_pos);
		msleep(20);
		goto out;
	}

	err = m7mu_mem_read(sd, unit, addr, buf);
	if (err < 0) {
		cam_err("i2c falied, err %d\n", err);
		goto out;
	}

	/* parsing & append result('0' or '1')
	 * Structure
	 *   filenameSize(1B)+fileName(N)+headerSize(2B)+header(N)+
	 *   ContentsSize(2B)+Contents(N,"\r\n"ended)+Result('0'(OK)or'1'(NG))
	 */
	parsingOK = 0;
	offHeader = 1 + buf[0];
	cam_trace("offsetHeader:%d\n", offHeader);
	if (offHeader+2 <= unit) {
		sizheader = (buf[offHeader]<<8 | buf[offHeader+1]);
		offContent = offHeader + 2 + sizheader;
		cam_trace("offsetContents:%d\n", offContent);
		if (offContent+2 <= unit) {
			sizcontent = (buf[offContent]<<8 | buf[offContent+1]);
			offRes = offContent+2+sizcontent;
			if (offRes < unit)
				parsingOK = 1;
		}
	}

	if (parsingOK) {
		cam_trace("OK: valid CSV data\n");
		/* write header */
		vfs_write(fp, &buf[offHeader + 2], sizheader, &fp->f_pos);
		/* write contents + result for APP */
		vfs_write(fp, &buf[offContent + 2],
			sizcontent + unit-offRes, &fp->f_pos);
	} else {
		cam_trace("NG: Invalid CSV data!!\n");
		vfs_write(fp, buf, unit, &fp->f_pos);
	}
	msleep(20);

out:
	if (buf != NULL)
		kfree(buf);
	if (!IS_ERR(fp) && fp != NULL)
		filp_close(fp, current->files);
file_out:
	set_fs(old_fs);

	return err;
}

#ifndef READ_CSV_FILE_DIRECT
static int m7mu_make_IR_cut_CSV_rawdata(struct v4l2_subdev *sd,
	u32 *address, bool bAddResult)
{
	struct file *fp;
	mm_segment_t old_fs;
	u8 *buf, buf2[2] = {0,};
	u32 unit_movie;
	u32 intram_unit = 0x1000;
	int i, val, err, start, end;
	long nread;
	struct i2c_client *client = to_client(sd);

	cam_trace("m7mu_make_IR_cut_CSV_rawdata()\n");

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	fp = filp_open(M7MU_FACTORY_CSV_PATH,
		O_RDWR|O_CREAT, S_IRUGO|S_IWUGO|S_IXUSR);
	if (IS_ERR(fp)) {
		cam_err("failed to open %s, err %ld\n",
			M7MU_FACTORY_CSV_PATH, PTR_ERR(fp));
		err = -ENOENT;
		goto file_out;
	}

	buf = kmalloc(intram_unit, GFP_KERNEL);
	if (!buf) {
		cam_err("failed to allocate memory\n");
		err = -ENOMEM;
		goto out;
	}

	cam_dbg("start, file path %s\n", M7MU_FACTORY_CSV_PATH);

	start = 0x9d ;
	end = 0x9f ;
	unit_movie = end - start + 1;

	for (i = start; i <= end; i++) {
		err = m7mu_readb(client, M7MU_CATEGORY_LENS, i, &val);
		if (err <= 0) {
			cam_err("i2c failed, err %d\n", err);
			goto out;
		}
		buf[i-start] = (u8)val;
	}

	err = vfs_llseek(fp, -2, SEEK_END);
	if (err < 0) {
		cam_warn("failed to fseek, %d\n", err);
		goto out;
	}

	nread = vfs_read(fp, (char __user *)buf2, 2, &fp->f_pos);
	if (nread != 2) {
		cam_err("failed to read firmware file, %ld Bytes\n", nread);
		err = -EIO;
		goto out;
	}

	err = vfs_llseek(fp, -2, SEEK_END);
	if (err < 0) {
		cam_warn("failed to fseek, %d\n", err);
		goto out;
	}

	vfs_write(fp, buf, unit_movie, &fp->f_pos);
	vfs_write(fp, buf2, 2, &fp->f_pos);

	msleep(20);

out:
	kfree(buf);
	if (!IS_ERR(fp) && fp != NULL)
		filp_close(fp, current->files);
file_out:
	set_fs(old_fs);

	return err;
}
#endif

#ifndef READ_CSV_FILE_DIRECT
static int m7mu_make_CSV_rawdata_direct(struct v4l2_subdev *sd, int nkind)
{
	struct m7mu_state *state = to_state(sd);
	struct i2c_client *client = to_client(sd);
	struct file *fp;
	mm_segment_t old_fs;
	u8 *buf;
	int val;
	u32 unit_default, unit_movie, unit_defect;
	u32 intram_unit = 0x1000;
	int i, err = 0, start, end, buf_size = 0, cnt = 100;

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	fp = filp_open(M7MU_FACTORY_CSV_PATH,
		O_WRONLY|O_CREAT|O_TRUNC, S_IRUGO|S_IWUGO|S_IXUSR);
	if (IS_ERR(fp)) {
		cam_err("failed to open %s, err %ld\n",
			M7MU_FACTORY_CSV_PATH, PTR_ERR(fp));
		err = -ENOENT;
		goto file_out;
	}

	buf = kmalloc((size_t)intram_unit, GFP_KERNEL);
	if (!buf) {
		cam_err("failed to allocate memory\n");
		err = -ENOMEM;
		goto out;
	}

	if (nkind == V4L2_CID_CAMERA_FACTORY_DEFECTPIXEL) {
		cam_dbg("start, file path %s\n", M7MU_FACTORY_CSV_PATH);

		start = 0x69;
		end = 0x8C;
		unit_default = end - start + 1;

		for (i = start; i <= end; i++) {
			err = m7mu_readb(client, M7MU_CATEGORY_MON, i, &val);
			if (err < 0) {
				cam_err("i2c falied, err %d\n", err);
				kfree(buf);
				return err;
			}
			buf[i-start] = (u8)val;
		}

		start = 0xA0;
		end = 0xA5;
		unit_movie = end - start + 1;

		for (i = start; i <= end; i++) {
			err = m7mu_readb(client, M7MU_CATEGORY_MON, i, &val);
			if (err < 0) {
				cam_err("i2c falied, err %d\n", err);
				kfree(buf);
				return err;
			}
			buf[unit_default + (i - start)] = (u8)val;
		}

		start = 0x61;
		end = 0x67;
		unit_defect = end - start + 1;

		for (i = start; i <= end; i++) {
			err = m7mu_readb(client, M7MU_CATEGORY_MON, i, &val);
			if (err < 0) {
				cam_err("i2c falied, err %d\n", err);
				kfree(buf);
				return err;
			}
			buf[unit_default + unit_movie + (i - start)] = (u8)val;
		}

		buf_size = unit_default + unit_movie + unit_defect;
	} else if (nkind == V4L2_CID_CAMERA_FACTORY_LENS_CAP_LOG) {
		cam_dbg("~FACTORY_LENS_CAP_LOG~");
		while (cnt) {
			msleep(20);
			err = m7mu_readb(client, M7MU_CATEGORY_ADJST,
					0x96, &val);
			if (err < 0) {
				cam_err("i2c falied, err %d\n", err);
				kfree(buf);
				return err;
			}

			if (val == 0x00) {
				cam_trace("save is complete\n");
				break;
			} else {
				cam_err("lens cpa G_AVG saving = %d\n", cnt);
				cnt--;
			}
		}

		buf_size = 4;
		err = m7mu_readb(client, M7MU_CATEGORY_ADJST, 0x9b, &val);
		if (err < 0) {
			cam_err("i2c falied, err %d\n", err);
			kfree(buf);
			return err;
		}
		buf[1] = (u8)val;
		/*Check value :: OK/NG 2byte*/
		if (state->lenscap_bright_min <= val &&
				val <= state->lenscap_bright_max) {
			cam_err("result - OK");
			buf[3] = 0x00;
		} else {
			cam_err("result - NG");
			buf[3] = 0x01;
		}
		cam_err("lens cpa G_AVG val = %d\n", val);
	} else {
		cam_err("error!!! no case ->LOG-Write ");
		goto out;
	}

	vfs_write(fp, buf, buf_size, &fp->f_pos);

out:
	kfree(buf);

	if (!IS_ERR(fp) && fp != NULL)
		filp_close(fp, current->files);

file_out:
	set_fs(old_fs);

	return err;
}
#endif

static int m7mu_set_fast_capture(struct v4l2_subdev *sd)
{
	struct m7mu_state *state = to_state(sd);
#if 0
	struct i2c_client *client = to_client(sd);
	u32 int_factor;
#endif
	cam_info("E\n");
	if (state->running_capture_mode == RUNNING_MODE_SINGLE) {
		int err;
		err = m7mu_set_mode(sd, M7MU_STILLCAP_MODE);
		if (err <= 0) {
			cam_err("Mode change is failed to STILLCAP for fast capture\n");
			return err;
		} else {
			cam_info("Fast capture is issued. mode change start.\n");
		}

		state->fast_capture_set = 1;
	} else if (state->running_capture_mode == RUNNING_MODE_DUMP) {
#if 0
		int old_mode, val, i;
		/* Start Dump mode */
		m7mu_readb(client, M7MU_CATEGORY_SYS, M7MU_SYS_MODE, &old_mode);
		err = m7mu_writeb(client, M7MU_CATEGORY_SYS,
				M7MU_SYS_MODE, 0x05);
		if (err <= 0)
			return err;

		for (i = M7MU_I2C_VERIFY; i; i--) {
			err = m7mu_readb(client, M7MU_CATEGORY_SYS,
					M7MU_SYS_MODE, &val);
			if (err <= 0)
				return err;
			if (val == 0x05)
				break;
			msleep(20);
		}

		int_factor = m7mu_wait_interrupt(sd, M7MU_ISP_CAPTURE_TIMEOUT);
		if (!(int_factor & M7MU_INT_CAPTURE)) {
			cam_warn("M7MU_INT_CAPTURE isn't issued, %#x\n",
					int_factor);
			return -ETIMEDOUT;
		}

		/* Start Transfer Custom Data */
		err = m7mu_writeb(client, M7MU_CATEGORY_SYS,
				0x37, 0x01);
		if (err <= 0)
			return err;
		int_factor = m7mu_wait_interrupt(sd, M7MU_ISP_CAPTURE_TIMEOUT);
		if (!(int_factor & M7MU_INT_CAPTURE)) {
			cam_warn("M7MU_INT_CAPTURE isn't issued, %#x\n",
					int_factor);
			return -ETIMEDOUT;
		}

		/* Change to previous mode */
		err = m7mu_writeb(client, M7MU_CATEGORY_SYS,
				M7MU_SYS_MODE, old_mode);
		if (err <= 0)
			return err;

		for (i = M7MU_I2C_VERIFY; i; i--) {
			err = m7mu_readb(client, M7MU_CATEGORY_SYS,
					M7MU_SYS_MODE, &val);
			if (err <= 0)
				return err;
			if (val == 0x05)
				break;
			msleep(20);
		}

		int_factor = m7mu_wait_interrupt(sd, M7MU_ISP_CAPTURE_TIMEOUT);
		if (!(int_factor & M7MU_INT_CAPTURE)) {
			cam_warn("M7MU_INT_CAPTURE isn't issued, %#x\n",
					int_factor);
			return -ETIMEDOUT;
		}
#else
		cam_info("RUNNING_MODE_DUMP\n");
#endif
	}
	return 0;
}

static const struct m7mu_frmsizeenum *m7mu_get_frmsize
	(const struct m7mu_frmsizeenum *frmsizes, int num_entries, int index)
{
	int i;

	for (i = 0; i < num_entries; i++) {
		if (frmsizes[i].index == index)
			return &frmsizes[i];
	}

	return NULL;
}

static int m7mu_set_snapshot_size(struct v4l2_subdev *sd, uint32_t value)
{
	int num_entries;
	int i;
	uint32_t width = value >> 16;
	uint32_t height = value & 0xFFFF;
	struct m7mu_state *state = to_state(sd);
	const struct m7mu_frmsizeenum **frmsize;
	int err;
	struct i2c_client *client = to_client(sd);

	frmsize = &state->capture;
	*frmsize = NULL;
	cam_info("%s: E  width = %d, height = %d", __func__,
			width, height);
	num_entries = ARRAY_SIZE(capture_frmsizes);
	for (i = 0; i < num_entries; i++) {
		if (width == capture_frmsizes[i].target_width &&
				height == capture_frmsizes[i].target_height) {
			*frmsize = &capture_frmsizes[i];
			break;
		}
	}

	cam_info("%s: frmsize index = %d", __func__, i);

	if (*frmsize == NULL) {
		cam_warn("invalid frame size %dx%d, set to default size. 5184X2916\n",
				width, height);
			*frmsize = m7mu_get_frmsize(capture_frmsizes,
				num_entries, M7MU_CAPTURE_15_1MPW);
	}

	err = m7mu_writeb(client, M7MU_CATEGORY_CAPPARM,
			M7MU_CAPPARM_MAIN_IMG_SIZE,
			state->capture->reg_val);
	CHECK_ERR(err);
	cam_info("%s: target capture frame size %dx%d\n", __func__,
			state->capture->target_width,
			state->capture->target_height);
	return 0;
}

static int m7mu_set_sensor_mode(struct v4l2_subdev *sd, int val)
{
	struct m7mu_state *state = to_state(sd);
#if 0
	int err;
	int set_shutter_mode;
#endif
	cam_dbg("E, value %d\n", val);

#if 0
	err = m7mu_set_mode(sd, M7MU_PARMSET_MODE);
	if (err <= 0) {
		cam_err("failed to set mode\n");
		return err;
	}

	if (val == SENSOR_MOVIE)
		set_shutter_mode = 0;  /* Rolling Shutter */
	else
		set_shutter_mode = 1;  /* Mechanical Shutter */
	err = m7mu_writeb(client, M7MU_CATEGORY_ADJST,
		M7MU_ADJST_SHUTTER_MODE, set_shutter_mode);
	CHECK_ERR(err);
#endif

	state->sensor_mode = val;

	cam_trace("X\n");
	return 0;
}

static int m7mu_set_flash_evc_step(struct v4l2_subdev *sd, int val)
{
	int err;
	struct i2c_client *client = to_client(sd);

	cam_trace("E, value %d\n", val);

	err = m7mu_writeb(client, M7MU_CATEGORY_CAPPARM,
			M7MU_CAPPARM_STROBE_EVC, val);
	CHECK_ERR(err);

	cam_trace("X\n");
	return 0;
}

static int m7mu_set_flash(struct v4l2_subdev *sd, int val, int force)
{
	struct m7mu_state *state = to_state(sd);
	struct i2c_client *client = to_client(sd);
	int strobe_en = 0;
	int err;
	cam_trace("E, value %d\n", val);

	if (!force)
		state->flash_mode = val;

retry:
	switch (val) {
	case V4L2_FLASH_MODE_OFF:
		strobe_en = 0;
		break;

	case V4L2_FLASH_MODE_AUTO:
		strobe_en = 0x02;
		break;

	case V4L2_FLASH_MODE_ON:
		strobe_en = 0x01;
		break;

	case V4L2_FLASH_MODE_RED_EYE:
		strobe_en = 0x12;
		break;

	case V4L2_FLASH_MODE_FILL_IN:
		strobe_en = 0x01;
		break;

	case V4L2_FLASH_MODE_SLOW_SYNC:
		strobe_en = 0x03;
		break;

	case V4L2_FLASH_MODE_RED_EYE_FIX:
		strobe_en = 0x02;
		err = m7mu_writeb(client, M7MU_CATEGORY_FD,
				M7MU_FD_RED_EYE, 0x01);
		CHECK_ERR(err);
		break;

	default:
		cam_warn("invalid value, %d\n", val);
		val = V4L2_FLASH_MODE_OFF;
		goto retry;
	}

	state->strobe_en = strobe_en;

	if (val !=  V4L2_FLASH_MODE_RED_EYE_FIX) {
		err = m7mu_writeb(client, M7MU_CATEGORY_FD,
				M7MU_FD_RED_EYE, 0x00);
		CHECK_ERR(err);
	}

	err = m7mu_writeb(client, M7MU_CATEGORY_CAPPARM,
			M7MU_CAPPARM_STROBE_EN, strobe_en);
	CHECK_ERR(err);

	cam_trace("X\n");
	return 0;
}

static int m7mu_set_flash_batt_info(struct v4l2_subdev *sd, int val)
{
	int err;
	int set_strobe_batt;
	struct i2c_client *client = to_client(sd);

	cam_trace("E, value %d\n", val);

	if (val)
		set_strobe_batt = 1;
	else
		set_strobe_batt = 0;

	err = m7mu_writeb(client, M7MU_CATEGORY_CAPPARM,
			M7MU_CAPPARM_STROBE_BATT_INFO, set_strobe_batt);
	CHECK_ERR(err);

	cam_trace("X\n");
	return 0;
}

static int m7mu_set_iso(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	struct m7mu_state *state = to_state(sd);
	struct i2c_client *client = to_client(sd);
	struct v4l2_queryctrl qc = {0,};
	int val = ctrl->value, err, current_state;
	int iso_idx;
	u32 iso[] = {0x00, 0x01, 0x64, 0xC8, 0x190, 0x320, 0x640, 0xC80};

	if (state->scene_mode != SCENE_MODE_NONE) {
		/* sensor will set internally */
		return 0;
	}

	cam_dbg("E, mode %d, value %d\n", state->mode, val);

	qc.id = ctrl->id;
	m7mu_queryctrl(sd, &qc);

	if (val < qc.minimum || val > qc.maximum) {
		cam_warn("invalied value, %d\n", val);
		val = qc.default_value;
	}

	val -= qc.minimum;

	switch (val) {
	case V4L2_ISO_AUTO:
		state->iso = 0;
		iso_idx = 0;
		break;

	case V4L2_ISO_50:
		state->iso = 50;
		iso_idx = 1;
		break;

	case V4L2_ISO_100:
		state->iso = 100;
		iso_idx = 2;
		break;

	case V4L2_ISO_200:
		state->iso = 200;
		iso_idx = 3;
		break;

	case V4L2_ISO_400:
		state->iso = 400;
		iso_idx = 4;
		break;

	case V4L2_ISO_800:
		state->iso = 800;
		iso_idx = 5;
		break;

	case V4L2_ISO_1600:
		state->iso = 1600;
		iso_idx = 6;
		break;

	case V4L2_ISO_3200:
		state->iso = 3200;
		iso_idx = 7;
		break;

	default:
		iso_idx = 0;
		cam_dbg("Error, invalid value %d\n", val);
		break;
	}

	err = m7mu_readb(client, M7MU_CATEGORY_AE,
		M7MU_AE_EV_PRG_MODE_CAP, &current_state);

	/* ISO AUTO */
	if (val == V4L2_ISO_AUTO) {
		switch (state->mode) {
		case MODE_PROGRAM:
		case MODE_BEST_GROUP_POSE:
		case MODE_BEAUTY_SHOT:
		case MODE_BEST_SHOT:
		case MODE_CONTINUOUS_SHOT:
		case MODE_ERASER:
		case MODE_PANORAMA:
		case MODE_SMART_SELF:
			err = m7mu_writeb(client, M7MU_CATEGORY_AE,
				M7MU_AE_EV_PRG_MODE_CAP, 0x00);
			CHECK_ERR(err);
			break;

		case MODE_A:
			err = m7mu_writeb(client, M7MU_CATEGORY_AE,
				M7MU_AE_EV_PRG_MODE_CAP, 0x01);
			CHECK_ERR(err);
			break;

		case MODE_S:
			err = m7mu_writeb(client, M7MU_CATEGORY_AE,
				M7MU_AE_EV_PRG_MODE_CAP, 0x02);
			CHECK_ERR(err);
			break;

		default:
			break;
		}
	} else {
		switch (state->mode) {
		case MODE_PROGRAM:
		case MODE_BEST_GROUP_POSE:
		case MODE_BEAUTY_SHOT:
		case MODE_BEST_SHOT:
		case MODE_CONTINUOUS_SHOT:
		case MODE_ERASER:
		case MODE_PANORAMA:
		case MODE_SMART_SELF:
			if (current_state != 0x04) {
				err = m7mu_writeb(client, M7MU_CATEGORY_AE,
					M7MU_AE_EV_PRG_MODE_CAP, 0x04);
				CHECK_ERR(err);
			}
			break;

		case MODE_A:
			if (current_state != 0x05) {
				err = m7mu_writeb(client, M7MU_CATEGORY_AE,
					M7MU_AE_EV_PRG_MODE_CAP, 0x05);
				CHECK_ERR(err);
			}
			break;

		case MODE_S:
			if (current_state != 0x06) {
				err = m7mu_writeb(client, M7MU_CATEGORY_AE,
					M7MU_AE_EV_PRG_MODE_CAP, 0x06);
				CHECK_ERR(err);
			}
			break;

		default:
			break;
		}
	}
	err = m7mu_writew(client, M7MU_CATEGORY_AE,
			M7MU_AE_EV_PRG_ISO_VALUE, iso[iso_idx]);
	CHECK_ERR(err);
	cam_trace("X\n");
	return 0;
}

static int m7mu_set_metering(struct v4l2_subdev *sd, int val)
{
	int err;
	struct i2c_client *client = to_client(sd);

	cam_dbg("E, value %d\n", val);

retry:
	switch (val) {
	case METERING_CENTER:
		err = m7mu_writeb(client, M7MU_CATEGORY_AE, M7MU_AE_MODE, 0x03);
		CHECK_ERR(err);
		break;
	case METERING_SPOT:
		err = m7mu_writeb(client, M7MU_CATEGORY_AE, M7MU_AE_MODE, 0x05);
		CHECK_ERR(err);
		break;
	case METERING_MATRIX:
		err = m7mu_writeb(client, M7MU_CATEGORY_AE, M7MU_AE_MODE, 0x01);
		CHECK_ERR(err);
		break;
	default:
		cam_warn("invalid value, %d\n", val);
		val = METERING_CENTER;
		goto retry;
	}

	cam_trace("X\n");
	return 0;
}

static int m7mu_set_exposure(struct v4l2_subdev *sd,
	struct v4l2_control *ctrl)
{
	struct i2c_client *client = to_client(sd);
	struct v4l2_queryctrl qc = {0,};
	int val = ctrl->value, err;
	/*
	   -2.0, -1.7, -1.3, -1.0 -0.7 -0.3
	   0
	   +0.3 +0.7 +1.0 +1.3 +1.7 +2.0
	*/
	u32 exposure[] = {0x0A, 0x0D, 0x11, 0x14, 0x17, 0x1B,
		0x1E,
		0x21, 0x25, 0x28, 0x2B, 0x2F, 0x32};
	cam_dbg("E, value %d\n", val);

	qc.id = ctrl->id;
	m7mu_queryctrl(sd, &qc);

	if ((val < qc.minimum || val > qc.maximum) && (val != 50)) {
		cam_warn("invalied value, %d\n", val);
		val = qc.default_value;
	}

	if (val == 50) {
		/* + 0.5 EV */
		err = m7mu_writeb(client, M7MU_CATEGORY_AE,
			M7MU_AE_INDEX, 0x23);
		CHECK_ERR(err);
	} else {
		val -= qc.minimum;

		err = m7mu_writeb(client, M7MU_CATEGORY_AE,
			M7MU_AE_INDEX, exposure[val]);
		CHECK_ERR(err);
	}
	cam_trace("X\n");
	return 0;
}

static int m7mu_set_whitebalance(struct v4l2_subdev *sd, int val)
{
	struct m7mu_state *state = to_state(sd);
	struct i2c_client *client = to_client(sd);

	int err;
	cam_dbg("E, value %d\n", val);

retry:
	switch (val) {
	case V4L2_WHITE_BALANCE_AUTO:
		err = m7mu_writeb(client, M7MU_CATEGORY_WB,
			M7MU_WB_CWB_MODE, 0x00);
		CHECK_ERR(err);
		err = m7mu_writeb(client, M7MU_CATEGORY_WB,
			M7MU_WB_AWB_MODE, 0x01);
		CHECK_ERR(err);
		err = m7mu_writeb(client, M7MU_CATEGORY_WB,
			M7MU_WB_AWB_MANUAL, 0x01);
		CHECK_ERR(err);
		break;
	case V4L2_WHITE_BALANCE_SUNNY:
		err = m7mu_writeb(client, M7MU_CATEGORY_WB,
			M7MU_WB_CWB_MODE, 0x00);
		CHECK_ERR(err);
		err = m7mu_writeb(client, M7MU_CATEGORY_WB,
			M7MU_WB_AWB_MODE, 0x02);
		CHECK_ERR(err);
		err = m7mu_writeb(client, M7MU_CATEGORY_WB,
			M7MU_WB_AWB_MANUAL, 0x04);
		CHECK_ERR(err);
		break;

	case V4L2_WHITE_BALANCE_CLOUDY:
		err = m7mu_writeb(client, M7MU_CATEGORY_WB,
			M7MU_WB_CWB_MODE, 0x00);
		CHECK_ERR(err);
		err = m7mu_writeb(client, M7MU_CATEGORY_WB,
			M7MU_WB_AWB_MODE, 0x02);
		CHECK_ERR(err);
		err = m7mu_writeb(client, M7MU_CATEGORY_WB,
			M7MU_WB_AWB_MANUAL, 0x05);
		CHECK_ERR(err);
		break;

	case V4L2_WHITE_BALANCE_TUNGSTEN:
		err = m7mu_writeb(client, M7MU_CATEGORY_WB,
			M7MU_WB_CWB_MODE, 0x00);
		CHECK_ERR(err);
		err = m7mu_writeb(client, M7MU_CATEGORY_WB,
			M7MU_WB_AWB_MODE, 0x02);
		CHECK_ERR(err);
		err = m7mu_writeb(client, M7MU_CATEGORY_WB,
			M7MU_WB_AWB_MANUAL, 0x01);
		CHECK_ERR(err);
		break;

	case V4L2_WHITE_BALANCE_FLUORESCENT:
	case V4L2_WHITE_BALANCE_FLUORESCENT_H:
		err = m7mu_writeb(client, M7MU_CATEGORY_WB,
			M7MU_WB_CWB_MODE, 0x00);
		CHECK_ERR(err);
		err = m7mu_writeb(client, M7MU_CATEGORY_WB,
			M7MU_WB_AWB_MODE, 0x02);
		CHECK_ERR(err);
		err = m7mu_writeb(client, M7MU_CATEGORY_WB,
			M7MU_WB_AWB_MANUAL, 0x02);
		CHECK_ERR(err);
		break;

	case V4L2_WHITE_BALANCE_FLUORESCENT_L:
		err = m7mu_writeb(client, M7MU_CATEGORY_WB,
			M7MU_WB_CWB_MODE, 0x00);
		CHECK_ERR(err);
		err = m7mu_writeb(client, M7MU_CATEGORY_WB,
			M7MU_WB_AWB_MODE, 0x02);
		CHECK_ERR(err);
		err = m7mu_writeb(client, M7MU_CATEGORY_WB,
			M7MU_WB_AWB_MANUAL, 0x03);
		CHECK_ERR(err);
		break;

	case V4L2_WHITE_BALANCE_K:
		err = m7mu_writeb(client, M7MU_CATEGORY_WB,
			M7MU_WB_CWB_MODE, 0x00);
		CHECK_ERR(err);
		err = m7mu_writeb(client, M7MU_CATEGORY_WB,
			M7MU_WB_AWB_MODE, 0x02);
		CHECK_ERR(err);
		err = m7mu_writeb(client, M7MU_CATEGORY_WB,
			M7MU_WB_AWB_MANUAL, 0x0A);
		CHECK_ERR(err);
		break;

	case V4L2_WHITE_BALANCE_INCANDESCENT:
		err = m7mu_writeb(client, M7MU_CATEGORY_WB,
			M7MU_WB_CWB_MODE, 0x00);
		CHECK_ERR(err);
		err = m7mu_writeb(client, M7MU_CATEGORY_WB,
			M7MU_WB_AWB_MODE, 0x02);
		CHECK_ERR(err);
		err = m7mu_writeb(client, M7MU_CATEGORY_WB,
			M7MU_WB_AWB_MANUAL, 0x0B);
		CHECK_ERR(err);
		break;

	case V4L2_WHITE_BALANCE_PROHIBITION:
		err = m7mu_writeb(client, M7MU_CATEGORY_WB,
			M7MU_WB_CWB_MODE, 0x00);
		CHECK_ERR(err);
		err = m7mu_writeb(client, M7MU_CATEGORY_WB,
			M7MU_WB_AWB_MODE, 0x02);
		CHECK_ERR(err);
		err = m7mu_writeb(client, M7MU_CATEGORY_WB,
			M7MU_WB_AWB_MANUAL, 0x00);
		CHECK_ERR(err);
		break;

	case V4L2_WHITE_BALANCE_HORIZON:
		err = m7mu_writeb(client, M7MU_CATEGORY_WB,
			M7MU_WB_CWB_MODE, 0x00);
		CHECK_ERR(err);
		err = m7mu_writeb(client, M7MU_CATEGORY_WB,
			M7MU_WB_AWB_MODE, 0x02);
		CHECK_ERR(err);
		err = m7mu_writeb(client, M7MU_CATEGORY_WB,
			M7MU_WB_AWB_MANUAL, 0x07);
		CHECK_ERR(err);
		break;

	case V4L2_WHITE_BALANCE_LEDLIGHT:
		err = m7mu_writeb(client, M7MU_CATEGORY_WB,
			M7MU_WB_CWB_MODE, 0x00);
		CHECK_ERR(err);
		err = m7mu_writeb(client, M7MU_CATEGORY_WB,
			M7MU_WB_AWB_MODE, 0x02);
		CHECK_ERR(err);
		err = m7mu_writeb(client, M7MU_CATEGORY_WB,
			M7MU_WB_AWB_MANUAL, 0x09);
		CHECK_ERR(err);
		break;

	case V4L2_WHITE_BALANCE_CUSTOM:
		err = m7mu_writeb(client, M7MU_CATEGORY_WB,
			M7MU_WB_AWB_MODE, 0x02);
		CHECK_ERR(err);

		err = m7mu_writeb(client, M7MU_CATEGORY_WB,
			M7MU_WB_AWB_MANUAL, 0x08);
		CHECK_ERR(err);

		err = m7mu_writeb(client, M7MU_CATEGORY_WB,
			M7MU_WB_CWB_MODE, 0x02);
		CHECK_ERR(err);

		err = m7mu_writew(client, M7MU_CATEGORY_WB,
			M7MU_WB_SET_CUSTOM_RG, state->wb_custom_rg);
		CHECK_ERR(err);

		err = m7mu_writew(client, M7MU_CATEGORY_WB,
			M7MU_WB_SET_CUSTOM_BG, state->wb_custom_bg);
		CHECK_ERR(err);

		err = m7mu_writeb(client, M7MU_CATEGORY_WB,
			M7MU_WB_CWB_MODE, 0x01);
		CHECK_ERR(err);
		break;

	default:
		cam_warn("invalid value, %d\n", val);
		val = V4L2_WHITE_BALANCE_AUTO;
		goto retry;
	}

	cam_trace("X\n");
	return 0;
}

static int m7mu_set_coloradjust(struct v4l2_subdev *sd, int val)
{
/*	struct m7mu_state *state = to_state(sd); */
	struct i2c_client *client = to_client(sd);
	int err;

	cam_dbg("E, value %d\n", val);

	err = m7mu_writeb(client, M7MU_CATEGORY_WB,
		M7MU_WB_COLOR_ADJ, val);
	CHECK_ERR(err);

	cam_trace("X\n");
	return 0;
}

static int m7mu_set_reset_cwb(struct v4l2_subdev *sd, int val)
{
/*	struct m7mu_state *state = to_state(sd);*/
	struct i2c_client *client = to_client(sd);
	int err;

	cam_dbg("E, value %d\n", val);

	err = m7mu_writeb(client, M7MU_CATEGORY_WB,
		M7MU_WB_AWB_MODE, 0x04);
	CHECK_ERR(err);

	cam_trace("X\n");
	return 0;

}

static int m7mu_set_sharpness(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	struct m7mu_state *state = to_state(sd);
	struct i2c_client *client = to_client(sd);
	struct v4l2_queryctrl qc = {0,};
	int val = ctrl->value, err;
	u32 sharpness[] = {0x01, 0x02, 0x03, 0x04, 0x05};
	cam_dbg("E, value %d\n", val);

	qc.id = ctrl->id;
	m7mu_queryctrl(sd, &qc);

	if (val < qc.minimum || val > qc.maximum) {
		cam_warn("invalied value, %d\n", val);
		val = qc.default_value;
	}

	err = m7mu_writeb(client, M7MU_CATEGORY_MON,
		M7MU_MON_EDGE_CTRL, sharpness[val]);
	CHECK_ERR(err);

	state->sharpness = sharpness[val];

	cam_trace("X\n");
	return 0;
}

static int m7mu_set_contrast(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	struct v4l2_queryctrl qc = {0,};
	int val = ctrl->value, err;
	u32 contrast[] = {0x01, 0x02, 0x03, 0x04, 0x05};
	struct i2c_client *client = to_client(sd);

	cam_dbg("E, value %d\n", val);

	qc.id = ctrl->id;
	m7mu_queryctrl(sd, &qc);

	if (val < qc.minimum || val > qc.maximum) {
		cam_warn("invalied value, %d\n", val);
		val = qc.default_value;
	}

	err = m7mu_writeb(client, M7MU_CATEGORY_MON,
		M7MU_MON_TONE_CTRL, contrast[val]);
	CHECK_ERR(err);

	cam_trace("X\n");
	return 0;
}

static int m7mu_set_saturation(struct v4l2_subdev *sd,
	struct v4l2_control *ctrl)
{
	struct m7mu_state *state = to_state(sd);
	struct i2c_client *client = to_client(sd);
	struct v4l2_queryctrl qc = {0,};
	int val = ctrl->value, err;
	u32 saturation[] = {0x01, 0x02, 0x03, 0x04, 0x05};
	cam_dbg("E, value %d\n", val);

	qc.id = ctrl->id;
	m7mu_queryctrl(sd, &qc);

	if (val < qc.minimum || val > qc.maximum) {
		cam_warn("invalied value, %d\n", val);
		val = qc.default_value;
	}

	err = m7mu_writeb(client, M7MU_CATEGORY_MON,
		M7MU_MON_CHROMA_LVL, saturation[val]);
	CHECK_ERR(err);

	state->saturation = saturation[val];

	cam_trace("X\n");
	return 0;
}

static int m7mu_set_scene_mode(struct v4l2_subdev *sd, int val)
{
	struct m7mu_state *state = to_state(sd);
	struct i2c_client *client = to_client(sd);
	struct v4l2_control ctrl;
	int evp, sharpness, saturation;
	int err;
	cam_dbg("E, value %d\n", val);

	sharpness = SHARPNESS_DEFAULT;
	saturation = CONTRAST_DEFAULT;

retry:
	switch (val) {
	case SCENE_MODE_NONE:
		evp = 0x00;
		break;

	case SCENE_MODE_PORTRAIT:
		evp = 0x01;
		sharpness = SHARPNESS_MINUS_1;
		break;

	case SCENE_MODE_LANDSCAPE:
		evp = 0x02;
		sharpness = SHARPNESS_PLUS_1;
		saturation = SATURATION_PLUS_1;
		break;

	case SCENE_MODE_SPORTS:
		evp = 0x03;
		break;

	case SCENE_MODE_PARTY_INDOOR:
		evp = 0x04;
		saturation = SATURATION_PLUS_1;
		break;

	case SCENE_MODE_BEACH_SNOW:
		evp = 0x05;
		saturation = SATURATION_PLUS_1;
		break;

	case SCENE_MODE_SUNSET:
		evp = 0x06;
		break;

	case SCENE_MODE_DUSK_DAWN:
		evp = 0x07;
		break;

	case SCENE_MODE_FALL_COLOR:
		evp = 0x08;
		saturation = SATURATION_PLUS_2;
		break;

	case SCENE_MODE_NIGHTSHOT:
		evp = 0x09;
		break;

	case SCENE_MODE_BACK_LIGHT:
		evp = 0x0A;
		break;

	case SCENE_MODE_FIREWORKS:
		evp = 0x0B;
		break;

	case SCENE_MODE_TEXT:
		evp = 0x0C;
		sharpness = SHARPNESS_PLUS_2;
		break;

	case SCENE_MODE_CANDLE_LIGHT:
		evp = 0x0D;
		break;

	default:
		cam_warn("invalid value, %d\n", val);
		val = SCENE_MODE_NONE;
		goto retry;
	}

	/* EV-P */
	err = m7mu_writeb(client, M7MU_CATEGORY_AE, M7MU_AE_EP_MODE_MON, evp);
	CHECK_ERR(err);
	err = m7mu_writeb(client, M7MU_CATEGORY_AE, M7MU_AE_EP_MODE_CAP, evp);
	CHECK_ERR(err);

	/* Chroma Saturation */
	ctrl.id = V4L2_CID_CAMERA_SATURATION;
	ctrl.value = saturation;
	m7mu_set_saturation(sd, &ctrl);

	/* Sharpness */
	ctrl.id = V4L2_CID_CAMERA_SHARPNESS;
	ctrl.value = sharpness;
	m7mu_set_sharpness(sd, &ctrl);

	/* Emotional Color */
	err = m7mu_writeb(client, M7MU_CATEGORY_CAPPARM,
		M7MU_CAPPARM_MCC_MODE, val == SCENE_MODE_NONE ? 0x01 : 0x00);
	CHECK_ERR(err);

	state->scene_mode = val;

	cam_trace("X\n");
	return 0;
}

static int m7mu_set_effect_color(struct v4l2_subdev *sd, int val)
{
	int cb = 0, cr = 0;
	int err;
	struct i2c_client *client = to_client(sd);

	switch (val) {
	case IMAGE_EFFECT_SEPIA:
		cb = 0xD8;
		cr = 0x18;
		break;

	case IMAGE_EFFECT_BNW:
		cb = 0x00;
		cr = 0x00;
		break;

	case IMAGE_EFFECT_ANTIQUE:
		cb = 0xD0;
		cr = 0x30;
		break;

	default:
		return 0;
	}

	err = m7mu_writeb(client, M7MU_CATEGORY_MON,
			M7MU_MON_COLOR_EFFECT, 0x01);
	CHECK_ERR(err);
	err = m7mu_writeb(client, M7MU_CATEGORY_MON, M7MU_MON_CFIXB, cb);
	CHECK_ERR(err);
	err = m7mu_writeb(client, M7MU_CATEGORY_MON, M7MU_MON_CFIXR, cr);
	CHECK_ERR(err);

	return 0;
}

static int m7mu_set_effect_point(struct v4l2_subdev *sd, int val)
{
	int point = 0;
	int err;
	struct i2c_client *client = to_client(sd);

	switch (val) {
	case IMAGE_EFFECT_POINT_BLUE:
		point = 0;
		break;

	case IMAGE_EFFECT_POINT_RED:
		point = 1;
		break;

	case IMAGE_EFFECT_POINT_YELLOW:
		point = 2;
		break;

	default:
		return 0;
	}

	err = m7mu_writeb(client, M7MU_CATEGORY_MON,
		M7MU_MON_COLOR_EFFECT, 0x03);
	err = m7mu_writeb(client, M7MU_CATEGORY_MON,
		M7MU_MON_POINT_COLOR, point);
	CHECK_ERR(err);

	return 0;
}

static int m7mu_set_effect(struct v4l2_subdev *sd, int val)
{
	int set_effect = 0;
	int err;
	struct m7mu_state *state = to_state(sd);
	struct i2c_client *client = to_client(sd);

	cam_dbg("E, value %d\n", val);

retry:
	switch (val) {
	case IMAGE_EFFECT_NONE:
		set_effect = 0;
		break;

	case IMAGE_EFFECT_NEGATIVE:
		set_effect = 2;
		break;

	case IMAGE_EFFECT_BNW:
	case IMAGE_EFFECT_SEPIA:
	case IMAGE_EFFECT_ANTIQUE:
		err = m7mu_set_effect_color(sd, val);
		CHECK_ERR(err);
		set_effect = 1;
		break;

	case IMAGE_EFFECT_POINT_BLUE:
	case IMAGE_EFFECT_POINT_RED:
	case IMAGE_EFFECT_POINT_YELLOW:
		err = m7mu_set_effect_point(sd, val);
		CHECK_ERR(err);
		set_effect = 3;
		break;

	case IMAGE_EFFECT_VINTAGE_WARM:
		set_effect = 4;
		break;

	case IMAGE_EFFECT_VINTAGE_COLD:
		set_effect = 5;
		break;

	case IMAGE_EFFECT_WASHED:
		set_effect = 6;
		break;

	default:
		cam_warn("invalid value, %d\n", val);
		val = IMAGE_EFFECT_NONE;
		goto retry;
	}

	err = m7mu_writeb(client, M7MU_CATEGORY_MON,
		M7MU_MON_COLOR_EFFECT, set_effect);
	CHECK_ERR(err);

	state->color_effect = set_effect;

	cam_trace("X\n");
	return 0;
}

static int m7mu_set_wdr(struct v4l2_subdev *sd, int val)
{
	int wdr, err;
	struct i2c_client *client = to_client(sd);

	cam_dbg("%s\n", val ? "on" : "off");

	wdr = (val == 1 ? 0x01 : 0x00);

	err = m7mu_writeb(client, M7MU_CATEGORY_CAPPARM,
			M7MU_CAPPARM_WDR_EN, wdr);
		CHECK_ERR(err);

	cam_trace("X\n");
	return 0;
}

static int m7mu_set_antishake(struct v4l2_subdev *sd, int val)
{
	struct m7mu_state *state = to_state(sd);
	struct i2c_client *client = to_client(sd);
	int ahs, err;

	if (state->scene_mode != SCENE_MODE_NONE) {
		cam_warn("Should not be set with scene mode");
		return 0;
	}

	cam_dbg("%s\n", val ? "on" : "off");

	ahs = (val == 1 ? 0x0E : 0x00);

	err = m7mu_writeb(client, M7MU_CATEGORY_AE, M7MU_AE_EP_MODE_MON, ahs);
		CHECK_ERR(err);
	err = m7mu_writeb(client, M7MU_CATEGORY_AE, M7MU_AE_EP_MODE_CAP, ahs);
		CHECK_ERR(err);

	cam_trace("X\n");
	return 0;
}

static int m7mu_set_face_beauty(struct v4l2_subdev *sd, int val)
{
	struct m7mu_state *state = to_state(sd);
	struct i2c_client *client = to_client(sd);
	int err;

	cam_dbg("%s\n", val ? "on" : "off");

	err = m7mu_writeb(client, M7MU_CATEGORY_CAPPARM,
		M7MU_CAPPARM_AFB_CAP_EN, val ? 0x01 : 0x00);
	CHECK_ERR(err);

	state->face_beauty = val;

	cam_trace("X\n");
	return 0;
}

static unsigned int m7mu_set_cal_rect_pos_width(struct v4l2_subdev *sd,
	unsigned int pos_val)
{
	struct m7mu_state *state = to_state(sd);
	unsigned int set_val;

	if (pos_val <= 40)
		set_val = 40;
	else if (pos_val > (state->preview->sensor_width - 40))
		set_val = state->preview->sensor_width - 40;
	else
		set_val = pos_val;

	return set_val;
}

static unsigned int m7mu_set_cal_rect_pos_height(struct v4l2_subdev *sd,
	unsigned int pos_val)
{
	struct m7mu_state *state = to_state(sd);
	unsigned int set_val;

	if (pos_val <= 40)
		set_val = 40;
	else if (pos_val > (state->preview->sensor_height - 40))
		set_val = state->preview->sensor_height - 40;
	else
		set_val = pos_val;

	return set_val;
}


static int m7mu_set_object_tracking(struct v4l2_subdev *sd, int val)
{
	struct m7mu_state *state = to_state(sd);
	struct i2c_client *client = to_client(sd);
	int err = 0;
	unsigned int set_x, set_y;
	int window_status;

	err = m7mu_writeb(client, M7MU_CATEGORY_OT,
		M7MU_OT_TRACKING_CTL, 0x10);
	CHECK_ERR(err);

	cam_trace("E val : %d\n", val);

	if (val == OT_START) {
		set_x = m7mu_set_cal_rect_pos_width(sd, state->focus.pos_x);
		set_y = m7mu_set_cal_rect_pos_height(sd, state->focus.pos_y);

		cam_dbg("idx[%d] sensor_w[%d] sensor_h[%d]",
				state->preview->index,
				state->preview->sensor_width,
				state->preview->sensor_height);
		cam_dbg("pos_x[%d] pos_y[%d] x[%d] y[%d]",
			state->focus.pos_x, state->focus.pos_y,
			set_x, set_y);

		err = m7mu_writeb(client, M7MU_CATEGORY_OT,
			M7MU_OT_FRAME_WIDTH, 0x02);
		CHECK_ERR(err);
		err = m7mu_writew(client, M7MU_CATEGORY_OT,
			M7MU_OT_X_START_LOCATION,
			set_x);
		CHECK_ERR(err);
		err = m7mu_writew(client, M7MU_CATEGORY_OT,
			M7MU_OT_Y_START_LOCATION,
			set_y);
		CHECK_ERR(err);
		err = m7mu_writeb(client, M7MU_CATEGORY_OT,
			M7MU_OT_TRACKING_CTL, 0x11);
		CHECK_ERR(err);
		state->IsStartObjectTraking = true;
	} else {
		cam_dbg("state->focus_area_mode:%d\n", state->focus_area_mode);
		state->IsStartObjectTraking = false;
		if (state->focus_area_mode == V4L2_FOCUS_AREA_TRACKING) {
			err = m7mu_readb(client, M7MU_CATEGORY_LENS,
				M7MU_LENS_AF_WINDOW_MODE, &window_status);

			if (window_status != V4L2_FOCUS_AREA_CENTER
					&& state->facedetect_mode == V4L2_FACE_DETECTION_OFF) {
				err = m7mu_writeb(client, M7MU_CATEGORY_LENS,
					M7MU_LENS_AF_WINDOW_MODE,
					V4L2_FOCUS_AREA_CENTER);
				CHECK_ERR(err);
			}
		}
	}

	return 0;
}

static int m7mu_set_image_stabilizer_OIS(struct v4l2_subdev *sd, int val)
{
	struct m7mu_state *state = to_state(sd);
	struct i2c_client *client = to_client(sd);
	int err, int_factor, set_ois, int_en;
	int wait_int_ois = 0;

	if (state->image_stabilizer_mode != V4L2_IMAGE_STABILIZER_OIS
		|| state->mode == MODE_PANORAMA)
		return 0;

	cam_trace("E: mode %d\n", val);

retry:
	switch (val) {
	case V4L2_IS_OIS_NONE:
		cam_warn("OIS_NONE and OIS End");
		return 0;

	case V4L2_IS_OIS_MOVIE:
		set_ois = 0x01;
		wait_int_ois = 1;
		break;

	case V4L2_IS_OIS_STILL:
		set_ois = 0x02;
		wait_int_ois = 0;
		break;

#if 0
	case V4L2_IS_OIS_MULTI:
		set_ois = 0x03;
		wait_int_ois = 0;
		break;

	case V4L2_IS_OIS_VSS:
		set_ois = 0x04;
		wait_int_ois = 1;
		break;
#endif

	default:
		cam_warn("invalid value, %d", val);
		val = V4L2_IS_OIS_STILL;
		goto retry;
	}

	/* set movie mode when waterfall */
	if (state->mode == MODE_WATERFALL) {
		set_ois = 0x01;
		wait_int_ois = 1;
	}

	if (wait_int_ois) {

		CLEAR_ISP_INT1_STATE(sd);

		err = m7mu_readw(client, M7MU_CATEGORY_SYS,
			M7MU_SYS_INT_EN, &int_en);
		CHECK_ERR(err);

		/* enable OIS_SET interrupt */
		int_en |= M7MU_INT_OIS_SET;

		err = m7mu_writew(client, M7MU_CATEGORY_SYS,
			M7MU_SYS_INT_EN, int_en);
		CHECK_ERR(err);

		err = m7mu_writeb(client, M7MU_CATEGORY_NEW, 0x18, set_ois);
		CHECK_ERR(err);

		int_factor = m7mu_wait_interrupt(sd, M7MU_ISP_TIMEOUT);
		if (!(int_factor & M7MU_INT_OIS_SET)) {
			cam_err("M7MU_INT_OIS_SET isn't issued, %#x\n",
				int_factor);
			return -ETIMEDOUT;
		}

		/* enable OIS_SET interrupt */
		int_en &= ~M7MU_INT_OIS_SET;

		err = m7mu_writew(client, M7MU_CATEGORY_SYS,
			M7MU_SYS_INT_EN, int_en);
		CHECK_ERR(err);
	} else {
		err = m7mu_writeb(client, M7MU_CATEGORY_NEW, 0x18, set_ois);
		CHECK_ERR(err);
	}

	cam_trace("X\n");
	return 0;
}

static int m7mu_set_af_sensor_mode(struct v4l2_subdev *sd, int val)
{
	struct m7mu_state *state = to_state(sd);
	struct i2c_client *client = to_client(sd);
	u32 cancel;
	int err;
	int af_mode, af_window, af_range;
	int range_status, mode_status, window_status;

	cancel = val & V4L2_FOCUS_MODE_DEFAULT;
	val &= 0xFF;
	af_range = state->focus_range;

	cam_dbg("E, value %d\n", val);

retry:
	switch (val) {
	case V4L2_FOCUS_MODE_AUTO:
		af_mode = 0x00;
#if 0
		af_window = state->focus_area_mode;
#else
		if (state->focus_area_mode == V4L2_FOCUS_AREA_TRACKING) {
			if (state->IsStartObjectTraking == true)
				af_window = V4L2_FOCUS_AREA_TRACKING;
			else
				af_window = V4L2_FOCUS_AREA_CENTER;
		} else {
			af_window = state->focus_area_mode;
		}
#endif
		break;

	case V4L2_FOCUS_MODE_MULTI:
		af_mode = 0x00;
		af_window = 0x01;
		break;

	case V4L2_FOCUS_MODE_CONTINUOUS:
		af_mode = 0x01;
		af_range = 0x02;
		af_window = 0x00;
		break;

	case V4L2_FOCUS_MODE_FACEDETECT:
		af_mode = 0x00;
		af_window = 0x02;
		break;

	case V4L2_FOCUS_MODE_TOUCH:
		af_mode = 0x00;
#if 0
		af_window = 0x02;
#else
		if (state->focus_area_mode == V4L2_FOCUS_AREA_TRACKING) {
			if (state->IsStartObjectTraking == true)
				af_window = 0x04;
			else
				af_window = V4L2_FOCUS_AREA_CENTER;
		} else {
			af_window = 0x2;
		}
#endif
		break;

	case V4L2_FOCUS_MODE_MACRO:
		af_mode = 0x00;
		af_range = 0x01;
#if 0
		af_window = state->focus_area_mode;
#else
		if (state->focus_area_mode == V4L2_FOCUS_AREA_TRACKING) {
			if (state->IsStartObjectTraking == true)
				af_window = V4L2_FOCUS_AREA_TRACKING;
			else
				af_window = V4L2_FOCUS_AREA_CENTER;
		} else {
			af_window = state->focus_area_mode;
		}
#endif
		break;

	case V4L2_FOCUS_MODE_MANUAL:
		af_mode = 0x02;
		af_window = state->focus_area_mode;
		af_range = 0x02;
		cancel = 0;
		break;

	case V4L2_FOCUS_MODE_OBJECT_TRACKING:
		af_mode = 0x00;
#if 0
		af_window = state->focus_area_mode;	/* 0x02; */
#else
		if (state->IsStartObjectTraking == true)
			af_window = state->focus_area_mode;
		else
			af_window = V4L2_FOCUS_AREA_CENTER;
#endif
		break;

	default:
		cam_warn("invalid value, %d", val);
		val = V4L2_FOCUS_MODE_AUTO;
		goto retry;
	}

	if (cancel && state->focus.lock)
		m7mu_set_lock(sd, 0);

	state->focus.mode = val;

	/* Set AF Mode */
	err = m7mu_readb(client, M7MU_CATEGORY_LENS,
		M7MU_LENS_AF_MODE, &mode_status);

	if (mode_status != af_mode) {
		if (state->focus.mode != V4L2_FOCUS_MODE_TOUCH) {
			err = m7mu_writeb(client, M7MU_CATEGORY_LENS,
				M7MU_LENS_AF_MODE, af_mode);
			CHECK_ERR(err);
		}
	}

	/* fix range */
	if (state->mode == MODE_SMART_AUTO || state->mode == MODE_VIDEO
	|| state->mode == MODE_CLOSE_UP || state->mode == MODE_BEAUTY_SHOT
	|| state->mode == MODE_FOOD || state->mode == MODE_CANDLE
	|| state->mode == MODE_SMART_SUGGEST) {
		af_range = 0x02;
	} else if (state->mode >= MODE_BEST_GROUP_POSE) {
		af_range = 0x00;
	}

	/* fix window to center */
	if ((state->focus.mode == 0 || state->focus.mode == 1)
		&&  state->focus_area_mode == 2)
		af_window = 0x00;

	/* Set AF Scan Range */
	err = m7mu_readb(client, M7MU_CATEGORY_LENS,
		M7MU_LENS_AF_SCAN_RANGE, &range_status);

	if (range_status != af_range) {
		if ((state->factory_test_num == 131) ||
		   (state->factory_test_num == 132)) {
			/*
			   heechul kim, to solve AF fail
			   in MOT resolution test
			 */
			cam_trace(
				"testno=131/132 -> block(0xa,0x7,val 0x%X->0x%X)\n",
				range_status, af_range);
		} else {
			err = m7mu_writeb(client, M7MU_CATEGORY_LENS,
				M7MU_LENS_AF_SCAN_RANGE, af_range);
			CHECK_ERR(err);
		}
	}
#if 0
	/* Set Zone REQ */
	if (range_status != af_range) {
		err = m7mu_writeb(client, M7MU_CATEGORY_LENS,
			M7MU_LENS_AF_INITIAL, 0x04);
		CHECK_ERR(err);
	}
#endif
	/* Set AF Window Mode */
	err = m7mu_readb(client, M7MU_CATEGORY_LENS,
		M7MU_LENS_AF_WINDOW_MODE, &window_status);

	if (window_status != af_window
			&& state->facedetect_mode == V4L2_FACE_DETECTION_OFF) {
		err = m7mu_writeb(client, M7MU_CATEGORY_LENS,
			M7MU_LENS_AF_WINDOW_MODE, af_window);
		CHECK_ERR(err);
	}
	cam_trace("X\n");
	return 0;
}

static int m7mu_set_af(struct v4l2_subdev *sd, int val)
{
	struct m7mu_state *state = to_state(sd);
	struct i2c_client *client = to_client(sd);
	const struct m7mu_platform_data *pdata = client->dev.platform_data;
	int err = 0;
	/* int int_factor; */

	cam_info("%s, mode %d\n", val ? "start" : "stop", state->focus.mode);

	if (state->focus.lock == 1 && state->samsung_app == 0) {
		cam_info("ISP is lock!!\n");
		m7mu_set_lock(sd, 0);
	}

	state->focus.start = val;

	if (val == 1) {
		/* AF LED regulator on */
		if (pdata != NULL && pdata->af_led_power != NULL)
			pdata->af_led_power(1);

		if (state->facedetect_mode == V4L2_FACE_DETECTION_NORMAL
			&& (state->mode == MODE_SMART_AUTO
			/*|| state->mode == MODE_SMART_SUGGEST*/)) {
			err = m7mu_writeb(client, M7MU_CATEGORY_FD,
				M7MU_FD_CTL, 0x11);
			CHECK_ERR(err);
		}

		m7mu_set_af_sensor_mode(sd, state->focus.mode);

		if (state->focus.mode != V4L2_FOCUS_MODE_CONTINUOUS) {
			m7mu_set_lock(sd, 1);

			/* Single AF Start */
			err = m7mu_writeb(client, M7MU_CATEGORY_LENS,
					M7MU_LENS_AF_START_STOP, 0x00);
			CHECK_ERR(err);

#if 0
			/*
			   14.02.11. heechul kim.
			   the interrupt waiting is useless
			   because AF done is checked by polling
			   in AF sequence
			 */
			/* Clear Interrupt factor */
			int_factor = m7mu_wait_interrupt(sd, M7MU_ISP_TIMEOUT);
			if (!(int_factor & M7MU_INT_AF)) {
				cam_err("M7MU_INT_AF isn't issued, factor = %#x, INT_ZOOM = %#x\n",
					int_factor, M7MU_INT_AF);
				return -ETIMEDOUT;
			}
#endif
		}
	} else {
		if (state->facedetect_mode == V4L2_FACE_DETECTION_NORMAL
			&& (state->mode == MODE_SMART_AUTO
			/*|| state->mode == MODE_SMART_SUGGEST*/)) {
			err = m7mu_writeb(client, M7MU_CATEGORY_FD,
				M7MU_FD_CTL, 0x01);
			CHECK_ERR(err);
		}

		cam_info("lock %d, status %x, running %d\n",
				state->focus.lock,
				state->focus.status,
				state->af_running);

		if (state->focus.lock && state->focus.status != 0x1000
			&& !state->af_running)
			m7mu_set_lock(sd, 0);

		/* AF LED regulator off */
		if (pdata != NULL && pdata->af_led_power != NULL)
			pdata->af_led_power(0);
	}

	cam_dbg("X(%d)\n", err);
	return err;
}

static int m7mu_set_af_mode(struct v4l2_subdev *sd, int val)
{
	struct m7mu_state *state = to_state(sd);
	struct i2c_client *client = to_client(sd);
	int err, af_mode, mode_status;

	if (val == V4L2_FOCUS_MODE_CONTINUOUS)
		af_mode = 0x01;
	else
		af_mode = 0x00;

	/* Set AF Mode */
	err = m7mu_readb(client, M7MU_CATEGORY_LENS,
		M7MU_LENS_AF_MODE, &mode_status);

	if (mode_status != af_mode) {
		if (state->focus.mode != V4L2_FOCUS_MODE_TOUCH) {
			err = m7mu_writeb(client, M7MU_CATEGORY_LENS,
				M7MU_LENS_AF_MODE, af_mode);
			CHECK_ERR(err);
		}
	}

	state->focus.mode = val;

	cam_trace("X val : %d\n", val);
	return 0;
}

static int m7mu_set_focus_range(struct v4l2_subdev *sd, int val)
{
	struct m7mu_state *state = to_state(sd);
	struct i2c_client *client = to_client(sd);
	int err, range_status;

	/* fix range */
	if (state->sensor_mode == SENSOR_CAMERA) {
		if (state->mode == MODE_SMART_AUTO || state->mode == MODE_VIDEO
		|| state->mode >= MODE_BEST_GROUP_POSE
		|| state->mode == MODE_SMART_SUGGEST) {
			cam_trace("don't set !!!\n");
			return 0;
		}
	} else if (state->sensor_mode == SENSOR_MOVIE) {
		if (state->mode == MODE_VIDEO ||
				state->mode >= MODE_BEST_GROUP_POSE ||
				state->mode == MODE_SMART_SUGGEST) {
			cam_trace("don't set !!!\n");
			return 0;
		}
	}

	/* Set AF Scan Range */
	err = m7mu_readb(client, M7MU_CATEGORY_LENS,
		M7MU_LENS_AF_SCAN_RANGE, &range_status);

	if (range_status != val) {
		err = m7mu_writeb(client, M7MU_CATEGORY_LENS,
			M7MU_LENS_AF_SCAN_RANGE, val);
		CHECK_ERR(err);
#if 0
		/* Set Zone REQ */
		err = m7mu_writeb(client, M7MU_CATEGORY_LENS,
			M7MU_LENS_AF_INITIAL, 0x04);
		CHECK_ERR(err);
#endif
	}

	state->focus_range = val;

	cam_trace("X val : %d\n", val);
	return 0;
}

static int m7mu_set_focus_area_mode(struct v4l2_subdev *sd, int val)
{
	struct m7mu_state *state = to_state(sd);
	struct i2c_client *client = to_client(sd);
	int err, window_status, window_mode;

	cam_dbg("E, value %d\n", val);

	if (val != V4L2_FOCUS_AREA_TRACKING) {
		/* Set AF Window Mode */
		err = m7mu_readb(client, M7MU_CATEGORY_LENS,
			M7MU_LENS_AF_WINDOW_MODE, &window_status);

		if (window_status != val
				&& state->facedetect_mode == V4L2_FACE_DETECTION_OFF) {
			err = m7mu_writeb(client, M7MU_CATEGORY_LENS,
				M7MU_LENS_AF_WINDOW_MODE, val);
			CHECK_ERR(err);
		}
	} else if (val == V4L2_FOCUS_AREA_TRACKING) {
		/*
		   And start of object-tracking is
		   in  m7mu_set_object_tracking()
		   -V4L2_CID_CAMERA_OBJ_TRACKING_START_STOP
		 */
		err = m7mu_writeb(client, M7MU_CATEGORY_LENS,
				M7MU_LENS_AF_SCAN_RANGE, 0x00);
			CHECK_ERR(err);

		err = m7mu_writeb(client, M7MU_CATEGORY_LENS,
				M7MU_LENS_AF_MODE, 0x00);
			CHECK_ERR(err);
#if 0
		err = m7mu_writeb(client, M7MU_CATEGORY_LENS,
				M7MU_LENS_AF_WINDOW_MODE, val);
			CHECK_ERR(err);
#else
		if (state->IsStartObjectTraking == true
				&& state->facedetect_mode == V4L2_FACE_DETECTION_OFF) {
			err = m7mu_writeb(client, M7MU_CATEGORY_LENS,
			M7MU_LENS_AF_WINDOW_MODE, val);
			CHECK_ERR(err);
		} else {
			err = m7mu_readb(client, M7MU_CATEGORY_LENS,
				M7MU_LENS_AF_WINDOW_MODE, &window_mode);

			if (window_mode != V4L2_FOCUS_AREA_CENTER
					&& state->facedetect_mode == V4L2_FACE_DETECTION_OFF) {
				err = m7mu_writeb(client, M7MU_CATEGORY_LENS,
				M7MU_LENS_AF_WINDOW_MODE,
				V4L2_FOCUS_AREA_CENTER);
				CHECK_ERR(err);
			}
		}
#endif
	}

	state->focus_area_mode = val;

	cam_trace("X val : %d\n", val);
	return 0;
}

static int m7mu_set_touch_auto_focus(struct v4l2_subdev *sd, int val)
{
	struct m7mu_state *state = to_state(sd);
	struct i2c_client *client = to_client(sd);
	int err = 0;
	cam_info("%s\n", val ? "start" : "stop");

	state->focus.touch = val;

	if (val) {
		err = m7mu_writew(client, M7MU_CATEGORY_LENS,
				M7MU_LENS_AF_TOUCH_POSX, state->focus.pos_x);
		CHECK_ERR(err);
		err = m7mu_writew(client, M7MU_CATEGORY_LENS,
				M7MU_LENS_AF_TOUCH_POSY, state->focus.pos_y);
		CHECK_ERR(err);

		if (state->facedetect_mode == V4L2_FACE_DETECTION_BLINK) {
			err = m7mu_writeb(client, M7MU_CATEGORY_FD,
				M7MU_FD_CTL, 0x01);
			CHECK_ERR(err);
		}
	} else {
		if (state->facedetect_mode == V4L2_FACE_DETECTION_BLINK) {
			err = m7mu_writeb(client, M7MU_CATEGORY_FD,
				M7MU_FD_CTL, 0x11);
			CHECK_ERR(err);
		}
	}

	cam_trace("X\n");
	return err;
}

static int m7mu_set_AF_LED(struct v4l2_subdev *sd, int val)
{
	int err;
	int set_AF_LED_On;
	struct i2c_client *client = to_client(sd);

	cam_trace("E, value %d\n", val);

	if (val)
		set_AF_LED_On = 1;
	else
		set_AF_LED_On = 0;

	err = m7mu_writeb(client, M7MU_CATEGORY_LENS,
		M7MU_LENS_AF_LED, set_AF_LED_On);
	CHECK_ERR(err);

	cam_trace("X\n");
	return 0;
}

static int m7mu_set_Turn_AF_LED(struct v4l2_subdev *sd, int val)
{
	int err;
	struct i2c_client *client = to_client(sd);
	const struct m7mu_platform_data *pdata = client->dev.platform_data;

	cam_trace("E, value %d\n", val);

	if (val) {
		err = m7mu_writeb(client, M7MU_CATEGORY_LENS,
		M7MU_LENS_AF_LED, val);
	}
	if (pdata != NULL && pdata->af_led_power != NULL)
		pdata->af_led_power(val);/* AF LED regulator on :1, off:0 */
	msleep(100);

	err = m7mu_writeb(client, M7MU_CATEGORY_LENS,
		0x4E, val);
	CHECK_ERR(err);

	cam_trace("X\n");
	return 0;
}

static int m7mu_set_timer_Mode(struct v4l2_subdev *sd, int val)
{
	int err;
	int set_OIS_timer;
	struct i2c_client *client = to_client(sd);

	cam_trace("E for OIS, value %d\n", val);

	if (val == 0)
		set_OIS_timer = 0;
	else
		set_OIS_timer = 1;

	err = m7mu_writeb(client, M7MU_CATEGORY_NEW,
		M7MU_NEW_OIS_TIMER, set_OIS_timer);
	CHECK_ERR(err);

	cam_trace("X\n");
	return 0;
}

static int m7mu_set_timer_LED(struct v4l2_subdev *sd, int val)
{
	int err;
	int set_timer_LED_On;
	struct i2c_client *client = to_client(sd);

	cam_trace("E, value %d\n", val);

	switch (val) {
	case V4L2_TIMER_LED_OFF:
		set_timer_LED_On = 0;
		break;

	case V4L2_TIMER_LED_2_SEC:
		set_timer_LED_On = 0x1;
		break;

	case V4L2_TIMER_LED_5_SEC:
		set_timer_LED_On = 0x2;
		break;

	case V4L2_TIMER_LED_10_SEC:
		set_timer_LED_On = 0x3;
		break;

	default:
		cam_warn("invalid value, %d", val);
		return 0;
	}

	err = m7mu_writeb(client, M7MU_CATEGORY_LENS,
		M7MU_LENS_TIMER_LED, set_timer_LED_On);
	CHECK_ERR(err);

	cam_trace("X\n");
	return 0;
}

static int m7mu_set_zoom_action_method(struct v4l2_subdev *sd, int val)
{
/*	struct m7mu_state *state = to_state(sd); */
	struct i2c_client *client = to_client(sd);
	int err = 0;

	if (val < ZOOM_KEY || val > ZOOM_METHOD_MAX)
		return -1;

	cam_trace("E : zoom_action_method = %d\n", val);

	if (val == ZOOM_KEY) {
		err = m7mu_writeb(client, M7MU_CATEGORY_LENS,
			M7MU_LENS_ZOOM_ACTION_METHOD, M7MU_ZOOM_METHOD_KEY);
		CHECK_ERR(err);
	} else {
		err = m7mu_writeb(client, M7MU_CATEGORY_LENS,
			M7MU_LENS_ZOOM_ACTION_METHOD, M7MU_ZOOM_METHOD_RING);
		CHECK_ERR(err);
	}
	cam_trace("X\n");

	return 0;
}
static int m7mu_set_zoom(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	struct m7mu_state *state = to_state(sd);
	struct i2c_client *client = to_client(sd);
	struct v4l2_queryctrl qc = {0,};
	int val = ctrl->value, err;
	int opti_val, digi_val;

	int opti_max = 30;
	int optical_zoom_val[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10,
		11, 12, 13, 14, 15, 16, 17, 18, 19, 20,
		21, 22, 23, 24, 25, 26, 27, 28, 29, 30};

	cam_trace("E, value %d\n", val);

	qc.id = ctrl->id;
	m7mu_queryctrl(sd, &qc);

	if (val < qc.minimum) {
		cam_warn("invalied min value, %d\n", val);
		val = qc.default_value;
	}

	if (val > qc.maximum) {
		cam_warn("invalied max value, %d\n", val);
		val = qc.maximum;
	}

	if (val <= opti_max) {
		opti_val = val;
		digi_val = 0;
	} else {
		opti_val = opti_max;
		digi_val = val - opti_max;
	}

	if (state->recording) {
		err = m7mu_writeb(client, M7MU_CATEGORY_LENS,
			M7MU_LENS_ZOOM_SPEED, 0x00);
		CHECK_ERR(err);
	} else {
		err = m7mu_writeb(client, M7MU_CATEGORY_LENS,
			M7MU_LENS_ZOOM_SPEED, 0x01);
		CHECK_ERR(err);
	}

	/* AF CANCEL */
	err = m7mu_writeb(client, M7MU_CATEGORY_LENS,
		M7MU_LENS_AF_START_STOP, 0x05);
	CHECK_ERR(err);

	err = m7mu_writeb(client, M7MU_CATEGORY_LENS,
		M7MU_LENS_AF_ZOOM_LEVEL, optical_zoom_val[opti_val]);
	CHECK_ERR(err);

	state->zoom = val;

	cam_trace("X\n");
	return 0;
}

static int m7mu_set_dzoom(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	struct i2c_client *client = to_client(sd);
	int val = ctrl->value, err;

	cam_trace("E, value %d\n", val);

	err = m7mu_writeb(client, M7MU_CATEGORY_MON, M7MU_MON_ZOOM, val);
	CHECK_ERR(err);

	cam_trace("X\n");
	return 0;
}

static int m7mu_set_zoom_ctrl(struct v4l2_subdev *sd, int val)
{
	struct m7mu_state *state = to_state(sd);
	struct i2c_client *client = to_client(sd);

	int err, curr_zoom_info;
	int zoom_ctrl, zoom_speed;
	int read_ctrl, read_speed, read_method;

	cam_trace("E, value %d\n", val);

	switch (val) {
	case V4L2_OPTICAL_ZOOM_TELE_START:
	case V4L2_OPTICAL_ZOOM_DIGITAL_TELE_START:
		zoom_ctrl = 0;
#ifdef CONFIG_MACH_GC2PD
		zoom_speed = 2;
#else
		zoom_speed = 1;
#endif
		break;

	case V4L2_OPTICAL_ZOOM_WIDE_START:
	case V4L2_OPTICAL_ZOOM_DIGITAL_WIDE_START:
		zoom_ctrl = 1;
#ifdef CONFIG_MACH_GC2PD
		zoom_speed = 2;
#else
		zoom_speed = 1;
#endif
		break;

	case V4L2_OPTICAL_ZOOM_SLOW_TELE_START:
		zoom_ctrl = 0;
		zoom_speed = 0;
		break;

	case V4L2_OPTICAL_ZOOM_SLOW_WIDE_START:
		zoom_ctrl = 1;
		zoom_speed = 0;
		break;

	case V4L2_OPTICAL_ZOOM_PINCH_START:
		err = m7mu_writeb(client, M7MU_CATEGORY_LENS,
			M7MU_LENS_ZOOM_ACTION_METHOD, 0x2);
		CHECK_ERR(err);
		return 0;

	case V4L2_OPTICAL_ZOOM_PINCH_STOP:
	case V4L2_OPTICAL_ZOOM_STOP:
		err = m7mu_readb(client, M7MU_CATEGORY_LENS,
			M7MU_LENS_ZOOM_ACTION_METHOD, &read_method);
		CHECK_ERR(err);

		if (read_method != 0) {
			err = m7mu_writeb(client, M7MU_CATEGORY_LENS,
				M7MU_LENS_ZOOM_ACTION_METHOD, 0x0);
			CHECK_ERR(err);
		}
		zoom_ctrl = 2;
		zoom_speed = 0x0F;
		break;

	default:
		cam_warn("invalid value, %d", val);
		return 0;
	}

	if (state->recording)
		zoom_speed = 0;

	err = m7mu_readb(client, M7MU_CATEGORY_LENS,
		M7MU_LENS_ZOOM_SPEED, &read_speed);
	CHECK_ERR(err);

	err = m7mu_readb(client, M7MU_CATEGORY_LENS,
		M7MU_LENS_AF_ZOOM_CTRL, &read_ctrl);
	CHECK_ERR(err);

#ifndef CONFIG_MACH_GC2PD
	if (read_speed != zoom_speed && val != V4L2_OPTICAL_ZOOM_STOP) {
		err = m7mu_writeb(client, M7MU_CATEGORY_LENS,
			M7MU_LENS_ZOOM_SPEED, zoom_speed);
		CHECK_ERR(err);
	}
#endif

	err = m7mu_readb(client, M7MU_CATEGORY_PRO_MODE,
		M7MU_PRO_SMART_READ3, &curr_zoom_info);
	CHECK_ERR(err);

	/*if ((read_ctrl != zoom_ctrl) || (curr_zoom_info & 0x40)) {*/
		if (val != V4L2_OPTICAL_ZOOM_STOP) {
			/* AF CANCEL */
			err = m7mu_writeb(client, M7MU_CATEGORY_LENS,
				M7MU_LENS_AF_START_STOP, 0x05);
			CHECK_ERR(err);
		}

		err = m7mu_writeb(client, M7MU_CATEGORY_LENS,
			M7MU_LENS_AF_ZOOM_CTRL, zoom_ctrl);
		CHECK_ERR(err);
	/*}*/

#ifdef CONFIG_MACH_GC2PD
	if (read_speed != zoom_speed && val != V4L2_OPTICAL_ZOOM_STOP) {
		err = m7mu_writeb(client, M7MU_CATEGORY_LENS,
			M7MU_LENS_ZOOM_SPEED, zoom_speed);
		CHECK_ERR(err);
	}
#endif

	cam_trace("X\n");
	return 0;
}

static int m7mu_set_zoom_speed(struct v4l2_subdev *sd, int zoom_speed)
{
	int err;
	int read_speed;
	int read_ctrl;
	struct i2c_client *client = to_client(sd);

	cam_trace("E, zoom_speed %d\n", zoom_speed);

	err = m7mu_readb(client, M7MU_CATEGORY_LENS,
		M7MU_LENS_AF_ZOOM_CTRL, &read_ctrl);
	CHECK_ERR(err);

	if (read_ctrl != 2) {
		err = m7mu_readb(client, M7MU_CATEGORY_LENS,
						M7MU_LENS_ZOOM_SPEED,
						&read_speed);
		CHECK_ERR(err);

		if (read_speed != zoom_speed) {
			err = m7mu_writeb(client, M7MU_CATEGORY_LENS,
							M7MU_LENS_ZOOM_SPEED,
							zoom_speed);
			CHECK_ERR(err);
		}
	} else {
		cam_trace("Zoom has stopped. Abandon zoom speed\n");
	}

	cam_trace("X\n");
	return 0;
}

static int m7mu_set_smart_zoom(struct v4l2_subdev *sd, int val)
{
	int err;
	int smart_zoom;
	struct m7mu_state *state = to_state(sd);
	struct i2c_client *client = to_client(sd);

	cam_trace("E, value %d\n", val);

	if (val)
		smart_zoom = 0x5B;
	else
		smart_zoom = 0;

	/* Off:0x00, On: 0x01 ~ 0x5B */
	err = m7mu_writeb(client, M7MU_CATEGORY_MON,
		M7MU_MON_HR_ZOOM, smart_zoom);
	CHECK_ERR(err);

	state->smart_zoom_mode = val;

	cam_trace("X\n");
	return 0;
}

static int m7mu_set_jpeg_quality(struct v4l2_subdev *sd,
	struct v4l2_control *ctrl)
{
	struct v4l2_queryctrl qc = {0,};
	int val = ctrl->value, ratio, err;
	struct i2c_client *client = to_client(sd);

	cam_dbg("E, value %d\n", val);

	qc.id = ctrl->id;
	m7mu_queryctrl(sd, &qc);

	cam_dbg("qc.maximum : %d, qc.minimum:%d\n", qc.maximum, qc.minimum);

	if (val < qc.minimum || val > qc.maximum) {
		cam_warn("invalied value, %d\n", val);
		val = qc.default_value;
	}

#if 0
	err = m7mu_writeb(client, M7MU_CATEGORY_CAPPARM,
		M7MU_CAPPARM_JPEG_RATIO, 0x62);
	CHECK_ERR(err);
#endif

	/* m7mu */
	if (val <= 65)		/* Normal */
		ratio = 0x14;
	else if (val <= 75)	/* Fine */
		ratio = 0x09;
	else			/* Superfine */
		ratio = 0x02;

	err = m7mu_writeb(client, M7MU_CATEGORY_CAPPARM,
		M7MU_CAPPARM_JPEG_RATIO_OFS, ratio);
	CHECK_ERR(err);

	cam_trace("X\n");
	return 0;
}

static int m7mu_get_exif(struct v4l2_subdev *sd)
{
	struct m7mu_state *state = to_state(sd);
	struct i2c_client *client = to_client(sd);

#if 0 /* legacy */
	/* standard values */
	u16 iso_std_values[] = { 10, 12, 16, 20, 25, 32, 40, 50, 64, 80,
		100, 125, 160, 200, 250, 320, 400, 500, 640, 800,
		1000, 1250, 1600, 2000, 2500, 3200, 4000, 5000, 6400, 8000};
	/* quantization table */
	u16 iso_qtable[] = { 11, 14, 17, 22, 28, 35, 44, 56, 71, 89,
		112, 141, 178, 224, 282, 356, 449, 565, 712, 890,
		1122, 1414, 1782, 2245, 2828, 3564, 4490, 5657, 7127, 8909};
#endif
	/* standard values : M7MU */
	u16 iso_std_values[] = {
		64, 80, 100, 125, 160,
		200, 250, 320, 400, 500,
		640, 800, 1000, 1250, 1600,
		2000, 2500, 3200, 4000, 5000,
		6400
	};
	/* quantization table */
	u16 iso_qtable[] = {
		72, 89, 112, 141, 179,
		224, 283, 358, 447, 566,
		716, 894, 1118, 1414, 1789,
		2236, 2828, 3578, 4472, 5657,
		7155
	};

#if 0
#ifdef EXIF_ONE_HALF_STOP_STEP
	s16 ss_std_values[] = {
		-400, -358, -300, -258, -200,
		-158, -100, -58, 0, 51,
		100, 158, 200, 258, 300,
		332, 391, 432, 491, 549,
		591, 649, 697, 749, 797,
		845, 897, 955, 997, 1055,
	};

	s16 ss_qtable[] = {
		-375, -325, -275, -225, -175,
		-125, -75, -25, 25, 75,
		125, 175, 225, 275, 325,
		375, 425, 475, 525, 575,
		625, 675, 725, 775, 825,
		875, 925, 975, 1025, 1075,
	};
#endif
#ifdef EXIF_ONE_THIRD_STOP_STEP
	s16 ss_std_values[] = {
		-400, -370, -332, -300, -258,
		-232, -200, -168, -132, -100,
		-68, -38, 0, 32, 74,
		100, 132, 158, 200, 232,
		258, 300, 332, 370, 390,
		432, 464, 490, 532, 564,
		590, 632, 664, 697, 732,
		764, 797, 832, 864, 897,
		932, 964, 997, 1029, 1064,
	};

	s16 ss_qtable[] = {
		-383, -350, -317, -283, -250,
		-217, -183, -150, -117, -83,
		-50, -17, 17, 50, 83,
		117, 150, 183, 217, 250,
		283, 317, 350, 383, 417,
		450, 483, 517, 550, 583,
		617, 650, 683, 717,	750,
		783, 817, 850, 883, 917,
		950, 983, 1017, 1050, 1083,
	};
#endif
#endif

	int num, den, i, err;

	cam_trace("E\n");

	/* exposure time */
	err = m7mu_readl(client, M7MU_CATEGORY_EXIF,
		M7MU_EXIF_EXPTIME_NUM, &num);
	CHECK_ERR(err);
	err = m7mu_readl(client, M7MU_CATEGORY_EXIF,
		M7MU_EXIF_EXPTIME_DEN, &den);
	CHECK_ERR(err);
	state->exif.exptime_num = num;
	state->exif.exptime_den = den;

	/* flash */
	err = m7mu_readw(client, M7MU_CATEGORY_EXIF, M7MU_EXIF_FLASH, &num);
	CHECK_ERR(err);
	state->exif.flash = (u16)num;

	/* iso */
	err = m7mu_readw(client, M7MU_CATEGORY_EXIF, M7MU_EXIF_ISO, &num);
	CHECK_ERR(err);
	for (i = 0; i < NELEMS(iso_qtable); i++) {
		if (num <= iso_qtable[i]) {
			state->exif.iso = iso_std_values[i];
			break;
		}
	}
	if (i == NELEMS(iso_qtable))
			state->exif.iso = 8000;

	cam_info("%s: real iso = %d, qtable_iso = %d, stored iso = %d\n",
			__func__, num, (i < NELEMS(iso_qtable)) ?
			iso_qtable[i] : 8000,
			state->exif.iso);

	/* shutter speed */
	err = m7mu_readl(client, M7MU_CATEGORY_EXIF, M7MU_EXIF_TV_NUM, &num);
	CHECK_ERR(err);
	err = m7mu_readl(client, M7MU_CATEGORY_EXIF, M7MU_EXIF_TV_DEN, &den);
	CHECK_ERR(err);
#if 1
	if (den) {
		state->exif.shutter_speed_num = num;
		state->exif.shutter_speed_den = den;
	} else {
		state->exif.shutter_speed_num = 0;
		state->exif.shutter_speed_den = 0;
	}
#else
	if (den) {
		for (i = 0; i < NELEMS(ss_qtable); i++) {
			if (num*M7MU_DEF_APEX_DEN/den <= ss_qtable[i]) {
				state->exif.tv = ss_std_values[i];
				break;
			}
		}
		if (i == NELEMS(ss_qtable))
			state->exif.tv = 1097;
		cam_info("%s: real TV = %d, stored TV = %d\n", __func__,
				num*M7MU_DEF_APEX_DEN/den, state->exif.tv);
	} else
		state->exif.tv = 0;
#endif

	/* brightness */
	err = m7mu_readl(client, M7MU_CATEGORY_EXIF, M7MU_EXIF_BV_NUM, &num);
	CHECK_ERR(err);
	err = m7mu_readl(client, M7MU_CATEGORY_EXIF, M7MU_EXIF_BV_DEN, &den);
	CHECK_ERR(err);
	if (den)
		state->exif.bv = num * M7MU_DEF_APEX_DEN / den;
	else
		state->exif.bv = 0;

	/* exposure bias value */
#if 1
	err = m7mu_readl(client, M7MU_CATEGORY_EXIF, M7MU_EXIF_EBV_NUM, &num);
	CHECK_ERR(err);
	err = m7mu_readl(client, M7MU_CATEGORY_EXIF, M7MU_EXIF_EBV_DEN, &den);
	CHECK_ERR(err);
	if (den)
		state->exif.ebv = num * M7MU_DEF_APEX_DEN / den;
	else
		state->exif.ebv = 0;
#else
	err = m7mu_readb(client, M7MU_CATEGORY_AE, M7MU_AE_INDEX, &num);
	CHECK_ERR(err);
	cam_info("%s: EV index = %d", __func__, num);
	state->exif.ebv = (num - 30) * 10;
#endif

	/* Aperture */
	err = m7mu_readl(client, M7MU_CATEGORY_EXIF, M7MU_EXIF_AV_NUM, &num);
	CHECK_ERR(err);
	err = m7mu_readl(client, M7MU_CATEGORY_EXIF, M7MU_EXIF_AV_DEN, &den);
	CHECK_ERR(err);
	if (den)
		state->exif.av = num * M7MU_DEF_APEX_DEN / den;
	else
		state->exif.av = 0;
	cam_info("%s: AV num = %d, AV den = %d\n", __func__, num, den);

	/* f-number */
	err = m7mu_readw(client, M7MU_CATEGORY_EXIF, M7MU_EXIF_F_NUM, &num);
	CHECK_ERR(err);
	state->exif.fnumber = num * 10;

	/* Focal length */
	err = m7mu_readw(client, M7MU_CATEGORY_LENS, M7MU_EXIF_FL, &num);
	CHECK_ERR(err);
	state->exif.focal_length = num * 10;
	cam_info("%s: FL = %d\n", __func__, num);

	/* Focal length 35m */
	err = m7mu_readw(client, M7MU_CATEGORY_LENS, M7MU_EXIF_FL_35, &num);
	CHECK_ERR(err);
	state->exif.focal_35mm_length = num;
	cam_info("%s: FL_35 = %d\n", __func__, num);

	cam_trace("X\n");

	return err;
}

/* for NSM Mode */
static int m7mu_set_Nsm_system(struct v4l2_subdev *sd, int val)
{
	int err;
	int nsm_system;
	struct i2c_client *client = to_client(sd);

	cam_trace("E, value %d\n", val);

	switch (val) {
	case NSM_SYSTEM_NONE:
		nsm_system = 0x10;
		break;

	case NSM_SYSTEM_FILMSUGGEST:
		nsm_system = 0x11;
		break;

	case NSM_SYSTEM_HOWTOLIBRARY:
		nsm_system = 0x12;
		break;

	case NSM_SYSTEM_FILMLIBRARY:
		nsm_system = 0x13;
		break;

	case NSM_SYSTEM_FILMMAKER:
		nsm_system = 0x14;
		break;

	default:
		nsm_system = 0x10;
		break;
	}
	cam_trace("E, Nsm_system = 0x%x\n", nsm_system);

	err = m7mu_writeb(client, M7MU_CATEGORY_PRO_MODE,
			M7MU_CAMERA_NSM_SYSTEM, nsm_system);
	CHECK_ERR(err);

	cam_trace("X\n");
	return 0;
}

static int m7mu_set_Nsm_state(struct v4l2_subdev *sd, int val)
{
	int err;
	int nsm_state;
	struct i2c_client *client = to_client(sd);

	cam_trace("E, value %d\n", val);

	switch (val) {
	case NSM_STATE_AUTO:
		nsm_state = 0x21;
		break;

	case NSM_STATE_FILM:
		nsm_state = 0x22;
		break;

	case NSM_STATE_SHOW:
		nsm_state = 0x23;
		break;

	default:
		nsm_state = 0x20;
		break;
	}
	cam_trace("E, Nsm_state = 0x%08x\n", nsm_state);

	err = m7mu_writeb(client, M7MU_CATEGORY_PRO_MODE,
			M7MU_CAMERA_NSM_SYSTEM, nsm_state);
	CHECK_ERR(err);

	cam_trace("X\n");
	return 0;
}

static int m7mu_set_Nsm_reset(struct v4l2_subdev *sd, int val)
{
	int err;
	int nsm_reset;
	struct i2c_client *client = to_client(sd);

	cam_trace("E, value %d\n", val);

	switch (val) {
       case NSM_RESET_ALL:
           nsm_reset = 0x31;
           break;

       case NSM_RESET_RGB:
           nsm_reset = 0x32;
           break;

       case NSM_RESET_SHARPNESS:
           nsm_reset = 0x33;
           break;

       case NSM_RESET_CONTRAST:
           nsm_reset = 0x34;
           break;

       case NSM_RESET_HUE:
           nsm_reset = 0x35;
           break;

       case NSM_RESET_SATURATION:
           nsm_reset = 0x36;
           break;

       case NSM_RESET_PRIHUE:
           nsm_reset = 0x37;
           break;

       case NSM_RESET_PRISATURATION:
           nsm_reset = 0x38;
           break;

       default:
           nsm_reset = 0x31;
            break;
	}

	cam_trace("E, Nsm_reset = 0x%08x\n", nsm_reset);

	err = m7mu_writeb(client, M7MU_CATEGORY_PRO_MODE,
			M7MU_CAMERA_NSM_SYSTEM, nsm_reset);
	CHECK_ERR(err);

	cam_trace("X\n");
	return 0;
}

static int m7mu_set_Nsm_command(struct v4l2_subdev *sd, int val)
{
	int err;
	int nsm_command;
	struct i2c_client *client = to_client(sd);

	cam_trace("E, value %d\n", val);

	switch (val) {
	case NSM_COMMAND_CANCEL:
		nsm_command = 0x41;
		break;

	case NSM_COMMAND_SAVE:
		nsm_command = 0x42;
		break;

	case NSM_COMMAND_SYNC:
		nsm_command = 0x43;
		break;

        case NSM_COMMAND_NOSYNC:
		nsm_command = 0x44;
		break;

	default:
		nsm_command = 0x41;
		break;
	}
	cam_trace("E, nsm_command = 0x%02x\n", nsm_command);

	err = m7mu_writeb(client, M7MU_CATEGORY_PRO_MODE,
			M7MU_CAMERA_NSM_SYSTEM, nsm_command);
	CHECK_ERR(err);

	cam_trace("X\n");
	return 0;
}

static int m7mu_set_Nsm_RGB(struct v4l2_subdev *sd, int val)
{
	int err;
	int set_val = (NSM_RGB << 24) | val;
	struct i2c_client *client = to_client(sd);

	cam_trace("E, value = 0x%08x\n", set_val);

	err = m7mu_writel(client, M7MU_CATEGORY_PRO_MODE,
			M7MU_CAMERA_NSM_MODE, set_val);
	CHECK_ERR(err);

	cam_trace("X\n");
	return 0;
}

static int m7mu_set_Nsm_ContSharp(struct v4l2_subdev *sd, int val)
{
	int err;
	int set_val = (NSM_CONTSHARP << 24) | val;
	struct i2c_client *client = to_client(sd);

	cam_trace("E, value = 0x%08x\n", set_val);

	err = m7mu_writel(client, M7MU_CATEGORY_PRO_MODE,
			M7MU_CAMERA_NSM_MODE, set_val);
	CHECK_ERR(err);

	cam_trace("X\n");
	return 0;
}

static int m7mu_set_Nsm_Hue_AllRedOrange(struct v4l2_subdev *sd, int val)
{
	int err;
	int set_val = (NSM_HUE_ALLREDORANGE << 24) | val;
	struct i2c_client *client = to_client(sd);

	cam_trace("E, value = 0x%08x\n", set_val);

	err = m7mu_writel(client, M7MU_CATEGORY_PRO_MODE,
			M7MU_CAMERA_NSM_MODE, set_val);
	CHECK_ERR(err);

	cam_trace("X\n");
	return 0;
}

static int m7mu_set_Nsm_Hue_YellowGreenCyan(struct v4l2_subdev *sd, int val)
{
	int err;
	int set_val = (NSM_HUE_YELLOWGREENCYAN << 24) | val;
	struct i2c_client *client = to_client(sd);

	cam_trace("E, value = 0x%08x\n", set_val);

	err = m7mu_writel(client, M7MU_CATEGORY_PRO_MODE,
			M7MU_CAMERA_NSM_MODE, set_val);
	CHECK_ERR(err);

	cam_trace("X\n");
	return 0;
}

static int m7mu_set_Nsm_BlueVioletPurple(struct v4l2_subdev *sd, int val)
{
	int err;
	int set_val = (NSM_HUE_BLUEVIOLETPURPLE << 24) | val;
	struct i2c_client *client = to_client(sd);

	cam_trace("E, value = 0x%08x\n", set_val);

	err = m7mu_writel(client, M7MU_CATEGORY_PRO_MODE,
			M7MU_CAMERA_NSM_MODE, set_val);
	CHECK_ERR(err);

	cam_trace("X\n");
	return 0;
}

static int m7mu_set_Nsm_Sat_AllRedOrange(struct v4l2_subdev *sd, int val)
{
	int err;
	int set_val = (NSM_SAT_ALLREDORANGE << 24) | val;
	struct i2c_client *client = to_client(sd);

	cam_trace("E, value = 0x%08x\n", set_val);

	err = m7mu_writel(client, M7MU_CATEGORY_PRO_MODE,
			M7MU_CAMERA_NSM_MODE, set_val);
	CHECK_ERR(err);

	cam_trace("X\n");
	return 0;
}

static int m7mu_set_Nsm_Sat_YellowGreenCyan(struct v4l2_subdev *sd, int val)
{
	int err;
	int set_val = (NSM_SAT_YELLOWGREENCYAN << 24) | val;
	struct i2c_client *client = to_client(sd);

	cam_trace("E, value = 0x%08x\n", set_val);

	err = m7mu_writel(client, M7MU_CATEGORY_PRO_MODE,
			M7MU_CAMERA_NSM_MODE, set_val);
	CHECK_ERR(err);

	cam_trace("X\n");
	return 0;
}

static int m7mu_set_Nsm_Sat_BlueVioletPurple(struct v4l2_subdev *sd, int val)
{
	int err;
	int set_val = (NSM_SAT_BLUEVIOLETPURPLE << 24) | val;
	struct i2c_client *client = to_client(sd);

	cam_trace("E, value = 0x%08x\n", set_val);

	err = m7mu_writel(client, M7MU_CATEGORY_PRO_MODE,
			M7MU_CAMERA_NSM_MODE, set_val);
	CHECK_ERR(err);

	cam_trace("X\n");
	return 0;
}

static int m7mu_set_Nsm_r(struct v4l2_subdev *sd, int val)
{
	int err;
	int set_val = (NSM_R << 24) | val;
	struct i2c_client *client = to_client(sd);

	cam_trace("E, value = 0x%08x\n", set_val);

	err = m7mu_writel(client, M7MU_CATEGORY_PRO_MODE,
			M7MU_CAMERA_NSM_MODE, set_val);
	CHECK_ERR(err);

	cam_trace("X\n");
	return 0;
}

static int m7mu_set_Nsm_g(struct v4l2_subdev *sd, int val)
{
	int err;
	int set_val = (NSM_G << 24) | val;
	struct i2c_client *client = to_client(sd);

	cam_trace("E, value = 0x%08x\n", set_val);

	err = m7mu_writel(client, M7MU_CATEGORY_PRO_MODE,
			M7MU_CAMERA_NSM_MODE, set_val);
	CHECK_ERR(err);

	cam_trace("X\n");
	return 0;
}

static int m7mu_set_Nsm_b(struct v4l2_subdev *sd, int val)
{
	int err;
	int set_val = (NSM_B << 24) | val;
	struct i2c_client *client = to_client(sd);

	cam_trace("E, value = 0x%08x\n", set_val);

	err = m7mu_writel(client, M7MU_CATEGORY_PRO_MODE,
			M7MU_CAMERA_NSM_MODE, set_val);
	CHECK_ERR(err);

	cam_trace("X\n");
	return 0;
}

static int m7mu_set_Nsm_global_contrast(struct v4l2_subdev *sd, int val)
{
	int err;
	int set_val = (NSM_GLOBAL_CONTRAST << 24) | val;
	struct i2c_client *client = to_client(sd);

	cam_trace("E, value = 0x%08x\n", set_val);

	err = m7mu_writel(client, M7MU_CATEGORY_PRO_MODE,
			M7MU_CAMERA_NSM_MODE, set_val);
	CHECK_ERR(err);

	cam_trace("X\n");
	return 0;
}

static int m7mu_set_Nsm_sharpness(struct v4l2_subdev *sd, int val)
{
	int err;
	int set_val = (NSM_GLOBAL_SHARPNESS << 24) | val;
	struct i2c_client *client = to_client(sd);

	cam_trace("E, value = 0x%08x\n", set_val);

	err = m7mu_writel(client, M7MU_CATEGORY_PRO_MODE,
			M7MU_CAMERA_NSM_MODE, set_val);
	CHECK_ERR(err);

	cam_trace("X\n");
	return 0;
}

static int m7mu_set_Nsm_hue_all(struct v4l2_subdev *sd, int val)
{
	int err;
	int set_val = (NSM_HUE_ALL << 24) | val;
	struct i2c_client *client = to_client(sd);

	cam_trace("E, value = 0x%08x\n", set_val);

	err = m7mu_writel(client, M7MU_CATEGORY_PRO_MODE,
			M7MU_CAMERA_NSM_MODE, set_val);
	CHECK_ERR(err);

	cam_trace("X\n");
	return 0;
}

static int m7mu_set_Nsm_hue_red(struct v4l2_subdev *sd, int val)
{
	int err;
	int set_val = (NSM_HUE_RED << 24) | val;
	struct i2c_client *client = to_client(sd);

	cam_trace("E, value = 0x%08x\n", set_val);

	err = m7mu_writel(client, M7MU_CATEGORY_PRO_MODE,
			M7MU_CAMERA_NSM_MODE, set_val);
	CHECK_ERR(err);

	cam_trace("X\n");
	return 0;
}

static int m7mu_set_Nsm_hue_orange(struct v4l2_subdev *sd, int val)
{
	int err;
	int set_val = (NSM_HUE_ORANGE << 24) | val;
	struct i2c_client *client = to_client(sd);

	cam_trace("E, value = 0x%08x\n", set_val);

	err = m7mu_writel(client, M7MU_CATEGORY_PRO_MODE,
			M7MU_CAMERA_NSM_MODE, set_val);
	CHECK_ERR(err);

	cam_trace("X\n");
	return 0;
}

static int m7mu_set_Nsm_hue_yellow(struct v4l2_subdev *sd, int val)
{
	int err;
	int set_val = (NSM_HUE_YELLOW << 24) | val;
	struct i2c_client *client = to_client(sd);

	cam_trace("E, value = 0x%08x\n", set_val);

	err = m7mu_writel(client, M7MU_CATEGORY_PRO_MODE,
			M7MU_CAMERA_NSM_MODE, set_val);
	CHECK_ERR(err);

	cam_trace("X\n");
	return 0;
}

static int m7mu_set_Nsm_hue_green(struct v4l2_subdev *sd, int val)
{
	int err;
	int set_val = (NSM_HUE_GREEN << 24) | val;
	struct i2c_client *client = to_client(sd);

	cam_trace("E, value = 0x%08x\n", set_val);

	err = m7mu_writel(client, M7MU_CATEGORY_PRO_MODE,
			M7MU_CAMERA_NSM_MODE, set_val);
	CHECK_ERR(err);

	cam_trace("X\n");
	return 0;
}

static int m7mu_set_Nsm_hue_cyan(struct v4l2_subdev *sd, int val)
{
	int err;
	int set_val = (NSM_HUE_CYAN << 24) | val;
	struct i2c_client *client = to_client(sd);

	cam_trace("E, value = 0x%08x\n", set_val);

	err = m7mu_writel(client, M7MU_CATEGORY_PRO_MODE,
			M7MU_CAMERA_NSM_MODE, set_val);
	CHECK_ERR(err);

	cam_trace("X\n");
	return 0;
}

static int m7mu_set_Nsm_hue_blue(struct v4l2_subdev *sd, int val)
{
	int err;
	int set_val = (NSM_HUE_BLUE << 24) | val;
	struct i2c_client *client = to_client(sd);

	cam_trace("E, value = 0x%08x\n", set_val);

	err = m7mu_writel(client, M7MU_CATEGORY_PRO_MODE,
			M7MU_CAMERA_NSM_MODE, set_val);
	CHECK_ERR(err);

	cam_trace("X\n");
	return 0;
}

static int m7mu_set_Nsm_hue_violet(struct v4l2_subdev *sd, int val)
{
	int err;
	int set_val = (NSM_HUE_VIOLET << 24) | val;
	struct i2c_client *client = to_client(sd);

	cam_trace("E, value = 0x%08x\n", set_val);

	err = m7mu_writel(client, M7MU_CATEGORY_PRO_MODE,
			M7MU_CAMERA_NSM_MODE, set_val);
	CHECK_ERR(err);

	cam_trace("X\n");
	return 0;
}

static int m7mu_set_Nsm_hue_purple(struct v4l2_subdev *sd, int val)
{
	int err;
	int set_val = (NSM_HUE_PURPLE << 24) | val;
	struct i2c_client *client = to_client(sd);

	cam_trace("E, value = 0x%08x\n", set_val);

	err = m7mu_writel(client, M7MU_CATEGORY_PRO_MODE,
			M7MU_CAMERA_NSM_MODE, set_val);
	CHECK_ERR(err);

	cam_trace("X\n");
	return 0;
}

static int m7mu_set_Nsm_sat_all(struct v4l2_subdev *sd, int val)
{
	int err;
	int set_val = (NSM_SAT_ALL << 24) | val;
	struct i2c_client *client = to_client(sd);

	cam_trace("E, value = 0x%08x\n", set_val);

	err = m7mu_writel(client, M7MU_CATEGORY_PRO_MODE,
			M7MU_CAMERA_NSM_MODE, set_val);
	CHECK_ERR(err);

	cam_trace("X\n");
	return 0;
}

static int m7mu_set_Nsm_sat_red(struct v4l2_subdev *sd, int val)
{
	int err;
	int set_val = (NSM_SAT_RED << 24) | val;
	struct i2c_client *client = to_client(sd);

	cam_trace("E, value = 0x%08x\n", set_val);

	err = m7mu_writel(client, M7MU_CATEGORY_PRO_MODE,
			M7MU_CAMERA_NSM_MODE, set_val);
	CHECK_ERR(err);

	cam_trace("X\n");
	return 0;
}

static int m7mu_set_Nsm_sat_orange(struct v4l2_subdev *sd, int val)
{
	int err;
	int set_val = (NSM_SAT_ORANGE << 24) | val;
	struct i2c_client *client = to_client(sd);

	cam_trace("E, value = 0x%08x\n", set_val);

	err = m7mu_writel(client, M7MU_CATEGORY_PRO_MODE,
			M7MU_CAMERA_NSM_MODE, set_val);
	CHECK_ERR(err);

	cam_trace("X\n");

	return 0;
}

static int m7mu_set_Nsm_sat_yellow(struct v4l2_subdev *sd, int val)
{
	int err;
	int set_val = (NSM_SAT_YELLOW << 24) | val;
	struct i2c_client *client = to_client(sd);

	cam_trace("E, value = 0x%08x\n", set_val);

	err = m7mu_writel(client, M7MU_CATEGORY_PRO_MODE,
			M7MU_CAMERA_NSM_MODE, set_val);
	CHECK_ERR(err);

	cam_trace("X\n");
	return 0;
}

static int m7mu_set_Nsm_sat_green(struct v4l2_subdev *sd, int val)
{
	int err;
	int set_val = (NSM_SAT_GREEN << 24) | val;
	struct i2c_client *client = to_client(sd);

	cam_trace("E, value = 0x%08x\n", set_val);

	err = m7mu_writel(client, M7MU_CATEGORY_PRO_MODE,
			M7MU_CAMERA_NSM_MODE, set_val);
	CHECK_ERR(err);

	cam_trace("X\n");
	return 0;
}

static int m7mu_set_Nsm_sat_cyan(struct v4l2_subdev *sd, int val)
{
	int err;
	int set_val = (NSM_SAT_CYAN << 24) | val;
	struct i2c_client *client = to_client(sd);

	cam_trace("E, value = 0x%08x\n", set_val);

	err = m7mu_writel(client, M7MU_CATEGORY_PRO_MODE,
			M7MU_CAMERA_NSM_MODE, set_val);
	CHECK_ERR(err);

	cam_trace("X\n");
	return 0;
}

static int m7mu_set_Nsm_sat_blue(struct v4l2_subdev *sd, int val)
{
	int err;
	int set_val = (NSM_SAT_BLUE << 24) | val;
	struct i2c_client *client = to_client(sd);

	cam_trace("E, value = 0x%08x\n", set_val);

	err = m7mu_writel(client, M7MU_CATEGORY_PRO_MODE,
			M7MU_CAMERA_NSM_MODE, set_val);
	CHECK_ERR(err);

	cam_trace("X\n");
	return 0;
}

static int m7mu_set_Nsm_sat_violet(struct v4l2_subdev *sd, int val)
{
	int err;
	int set_val = (NSM_SAT_VIOLET << 24) | val;
	struct i2c_client *client = to_client(sd);

	cam_trace("E, value = 0x%08x\n", set_val);

	err = m7mu_writel(client, M7MU_CATEGORY_PRO_MODE,
			M7MU_CAMERA_NSM_MODE, set_val);
	CHECK_ERR(err);

	cam_trace("X\n");
	return 0;
}

static int m7mu_set_Nsm_sat_purple(struct v4l2_subdev *sd, int val)
{
	int err;
	int set_val = (NSM_SAT_PURPLE << 24) | val;
	struct i2c_client *client = to_client(sd);

	cam_trace("E, value = 0x%08x\n", set_val);

	err = m7mu_writel(client, M7MU_CATEGORY_PRO_MODE,
			M7MU_CAMERA_NSM_MODE, set_val);
	CHECK_ERR(err);

	cam_trace("X\n");
	return 0;
}

static int m7mu_set_Nsm_fd_write(struct v4l2_subdev *sd, int val)
{
	int err;
	int fd_first_w, fd_last_b;
	struct i2c_client *client = to_client(sd);

	cam_trace("E, value = 0x%08x\n", val);

	if (val != 0xFFFFFF)
		return 0;

	fd_first_w = val >> 8;
	fd_last_b = val & 0xFF;
	cam_trace("E, [fd_first_w = 0x%08x]\n", fd_first_w);
	cam_trace("E, [fd_last_b = 0x%08x]\n", fd_last_b);

	err = m7mu_writew(client, M7MU_CATEGORY_FD,
			M7MU_CAMERA_NSM_FD_FIRST_W, fd_first_w);
	CHECK_ERR(err);

	err = m7mu_writeb(client, M7MU_CATEGORY_FD,
			M7MU_CAMERA_NSM_FD_LAST_B, fd_last_b);
	CHECK_ERR(err);

	cam_trace("X\n");
	return 0;
}

static int m7mu_set_Nsm_FilmID(struct v4l2_subdev *sd, int val)
{
	int err;
	int set_val;
	struct i2c_client *client = to_client(sd);

	cam_trace("E, val = %d\n", val);

	if (val < 0x1000000) {
		set_val =  (0x40000000 | val);
		cam_trace("set_val = %d\n", set_val);

		err = m7mu_writel(client, M7MU_CATEGORY_PRO_MODE,
				M7MU_CAMERA_NSM_MODE, set_val);
		CHECK_ERR(err);
	} else {
		set_val = (0x40000000 | (val & 0xFFFFFF));
		cam_trace("set_val = %d\n", set_val);

		err = m7mu_writel(client, M7MU_CATEGORY_PRO_MODE,
				M7MU_CAMERA_NSM_MODE, set_val);
		CHECK_ERR(err);

		set_val = (0x41000000 | ((val & 0xFF000000)>>24));
		cam_trace("set_val = %d\n", set_val);

		err = m7mu_writel(client, M7MU_CATEGORY_PRO_MODE,
				M7MU_CAMERA_NSM_MODE, set_val);
		CHECK_ERR(err);
	}

	cam_trace("X\n");
	return 0;
}

static int m7mu_set_Nsm_langnum(struct v4l2_subdev *sd, int val)
{
	int err;
	int set_val = (0x42000000 | val);
	struct i2c_client *client = to_client(sd);

	cam_trace("E, value = 0x%08x\n", set_val);

	err = m7mu_writel(client, M7MU_CATEGORY_PRO_MODE,
			M7MU_CAMERA_NSM_MODE, set_val);
	CHECK_ERR(err);

	cam_trace("X\n");
	return 0;
}
/* end NSM Mode */

static int m7mu_get_fd_eye_blink_result(struct v4l2_subdev *sd)
{
	struct m7mu_state *state = to_state(sd);
	struct i2c_client *client = to_client(sd);
	int err;
	s32 val_no = 1, val_level = 0;

	/* EyeBlink error check FRAME No, Level */
	err = m7mu_readb(client, M7MU_CATEGORY_FD,
			M7MU_FD_BLINK_FRAMENO, &val_no);
	CHECK_ERR(err);
	if (val_no < 0 || val_no > 2) {
		val_no = 0;
		cam_warn("Read Error FD_BLINK_FRAMENO [0x%x]\n", val_no);
	}
	err = m7mu_readb(client, M7MU_CATEGORY_FD,
			M7MU_FD_BLINK_LEVEL_1+val_no, &val_level);
	CHECK_ERR(err);

	if ((val_level == 0xFF) || (val_level <= 0x3C))
		state->fd_eyeblink_cap = 1;
	else
		state->fd_eyeblink_cap = 0;
	cam_dbg("blink no[%d] level[0x%x]\n", val_no, val_level);

	return err;
}

static int m7mu_get_red_eye_fix_result(struct v4l2_subdev *sd)
{
	struct m7mu_state *state = to_state(sd);
	struct i2c_client *client = to_client(sd);
	int err;
	s32 red_eye_status;

	if (state->flash_mode != V4L2_FLASH_MODE_RED_EYE_FIX)
		return 0;

	err = m7mu_readb(client, M7MU_CATEGORY_FD,
			M7MU_FD_RED_DET_STATUS, &red_eye_status);
	CHECK_ERR(err);

	state->fd_red_eye_status = red_eye_status;

	cam_dbg("red eye status [0x%x]\n", red_eye_status);

	return err;
}

static int m7mu_start_dual_postview(struct v4l2_subdev *sd, int frame_num)
{
#if 0
	struct m7mu_state *state = to_state(sd);
#endif
	struct i2c_client *client = to_client(sd);
	int err, int_factor;
	cam_trace("E : %d frame\n", frame_num);

	CLEAR_ISP_INT1_STATE(sd);

	/* Select image number of frame Preview image */
	err = m7mu_writeb(client, M7MU_CATEGORY_PARM,
		M7MU_PARM_SEL_FRAME_VIDEO_SNAP, frame_num);
	CHECK_ERR(err);

	/* Select main image format */
	err = m7mu_writeb(client, M7MU_CATEGORY_CAPPARM,
		M7MU_CAPPARM_YUVOUT_PREVIEW, 0x00);
	CHECK_ERR(err);

#if 0
	/* Select preview image size */
#if 0
	err = m7mu_writeb(client, M7MU_CATEGORY_CAPPARM,
	M7MU_CAPPARM_PREVIEW_IMG_SIZE, 0x08);
	CHECK_ERR(err);
#else
	if (FRM_RATIO(state->preview) == CAM_FRMRATIO_VGA) {
		err = m7mu_writeb(client, M7MU_CATEGORY_CAPPARM,
		M7MU_CAPPARM_PREVIEW_IMG_SIZE, 0x08);
		CHECK_ERR(err);
	} else if (FRM_RATIO(state->preview) == CAM_FRMRATIO_HD) {
		err = m7mu_writeb(client, M7MU_CATEGORY_CAPPARM,
		M7MU_CAPPARM_PREVIEW_IMG_SIZE, 0x0F);
		CHECK_ERR(err);
	}
#endif
#endif

	/* Get Video Snap Shot data */
	err = m7mu_writeb(client, M7MU_CATEGORY_PARM,
		M7MU_PARM_VIDEO_SNAP_IMG_TRANSFER_START, 0x02);
	CHECK_ERR(err);

	/* Clear Interrupt factor */
	int_factor = m7mu_wait_interrupt(sd, M7MU_ISP_TIMEOUT);
	if (!(int_factor & M7MU_INT_FRAME_SYNC)) {
		cam_err("M7MU_INT_FRAME_SYNC isn't issued, %#x\n", int_factor);
		return -ETIMEDOUT;
	}

	cam_trace("X\n");
	return err;
}

static int m7mu_start_dual_capture(struct v4l2_subdev *sd, int frame_num)
{
	struct m7mu_state *state = to_state(sd);
	struct i2c_client *client = to_client(sd);
	int err, int_factor;
	cam_trace("E : %d frame\n", frame_num);

	CLEAR_ISP_INT1_STATE(sd);

	/* Select image number of frame For Video Snap Shot image */
	err = m7mu_writeb(client, M7MU_CATEGORY_PARM,
		M7MU_PARM_SEL_FRAME_VIDEO_SNAP, frame_num);
	CHECK_ERR(err);

	/* Select main image format */
	err = m7mu_writeb(client, M7MU_CATEGORY_CAPPARM,
		M7MU_CAPPARM_YUVOUT_MAIN, 0x01);
	CHECK_ERR(err);

	/* Select main image size - 4M */
	err = m7mu_writeb(client, M7MU_CATEGORY_CAPPARM,
		M7MU_CAPPARM_MAIN_IMG_SIZE, 0x1E);
	CHECK_ERR(err);

	/* Get Video Snap Shot data */
	err = m7mu_writeb(client, M7MU_CATEGORY_PARM,
		M7MU_PARM_VIDEO_SNAP_IMG_TRANSFER_START, 0x01);
	CHECK_ERR(err);

	/* Clear Interrupt factor */
	int_factor = m7mu_wait_interrupt(sd, M7MU_ISP_TIMEOUT);
	if (!(int_factor & M7MU_INT_FRAME_SYNC)) {
		cam_err("M7MU_INT_FRAME_SYNC isn't issued, %#x\n", int_factor);
		return -ETIMEDOUT;
	}

	/* Get main image JPEG size */
	err = m7mu_readl(client, M7MU_CATEGORY_CAPCTRL,
			M7MU_CAPCTRL_IMG_SIZE, &state->jpeg.main_size);
	CHECK_ERR(err);
	cam_trace("~~~~~~ main_size : 0x%x ~~~~~~\n", state->jpeg.main_size);
#if 1
	state->jpeg.main_offset = 0;
	state->jpeg.thumb_offset = M7MU_JPEG_MAXSIZE;
	state->jpeg.postview_offset = M7MU_JPEG_MAXSIZE + M7MU_THUMB_MAXSIZE;

	/* Read Exif information */
	m7mu_get_exif(sd);
#endif

	if (frame_num == state->dual_capture_frame)
		state->dual_capture_start = 0;

	cam_trace("X\n");
	return err;
}

static int m7mu_start_postview_capture(struct v4l2_subdev *sd, int frame_num)
{
	struct m7mu_state *state = to_state(sd);
	struct i2c_client *client = to_client(sd);
	int err, int_factor;
	cam_trace("E : %d frame\n", frame_num);

	state->fast_capture_set = 0;

	if (state->dual_capture_start)
		return m7mu_start_dual_postview(sd, frame_num);

	if (state->running_capture_mode == RUNNING_MODE_AE_BRACKET
		|| state->running_capture_mode == RUNNING_MODE_LOWLIGHT) {

		CLEAR_ISP_INT1_STATE(sd);

		/* Select image number of frame */
		err = m7mu_writeb(client, M7MU_CATEGORY_CAPCTRL,
				M7MU_CAPCTRL_FRM_PRV_SEL, frame_num);
		CHECK_ERR(err);

		/* Clear Interrupt factor */
		int_factor = m7mu_wait_interrupt(sd, M7MU_ISP_SHORT_TIMEOUT);
		if (!(int_factor & M7MU_INT_CAPTURE)) {
			cam_warn("M7MU_INT_CAPTURE isn't issued on frame select, %#x\n",
					int_factor);
			return -ETIMEDOUT;
		}
	} else if (state->running_capture_mode == RUNNING_MODE_WB_BRACKET) {
		/* Select image number of frame */
		err = m7mu_writeb(client, M7MU_CATEGORY_CAPCTRL,
				M7MU_CAPCTRL_FRM_PRV_SEL, frame_num);
		CHECK_ERR(err);
	} else if (state->running_capture_mode == RUNNING_MODE_HDR) {
		cam_warn("HDR have no PostView\n");
		return 0;
	} else if (state->running_capture_mode == RUNNING_MODE_BLINK) {

		CLEAR_ISP_INT1_STATE(sd);

		/* Select image number of frame */
		err = m7mu_writeb(client, M7MU_CATEGORY_CAPCTRL,
				M7MU_CAPCTRL_FRM_PRV_SEL, 0xFF);
		CHECK_ERR(err);

		/* Clear Interrupt factor */
		int_factor = m7mu_wait_interrupt(sd, M7MU_ISP_TIMEOUT);
		if (!(int_factor & M7MU_INT_CAPTURE)) {
			cam_warn("M7MU_INT_CAPTURE isn't issued on frame select, %#x\n",
					int_factor);
			return -ETIMEDOUT;
		}
	} else if (state->running_capture_mode == RUNNING_MODE_BURST) {

		CLEAR_ISP_INT1_STATE(sd);

		/* Get Preview data */
		err = m7mu_writeb(client, M7MU_CATEGORY_CAPCTRL,
				M7MU_CAPCTRL_TRANSFER, 0x02);
		CHECK_ERR(err);

#if 0
		int i;
		for (i = 0;  i < 3; i++) { /*wait M7MU_INT_FRAME_SYNC*/
			/* Clear Interrupt factor */
			int_factor = m7mu_wait_interrupt(sd, M7MU_ISP_TIMEOUT);
			if (int_factor & (M7MU_INT_CAPTURE|M7MU_INT_SOUND)) {
				cam_trace("----skip interrupt=%x", int_factor);
				continue;
			}

			if (!(int_factor & M7MU_INT_FRAME_SYNC)) {
				cam_warn("M7MU_INT_FRAME_SYNC isn't issued on transfer, %#x\n",
						int_factor);
				return -ETIMEDOUT;
			}
			break;
		}
#else
		int_factor = m7mu_wait_interrupt(sd, M7MU_ISP_SHORT_TIMEOUT);
		if (!(int_factor & M7MU_INT_FRAME_SYNC)) {
			cam_warn("M7MU_INT_FRAME_SYNC isn't issued on transfer, %#x\n",
					int_factor);
			return -ETIMEDOUT;
		}
#endif
	} else {
		/* Select image number of frame */
		err = m7mu_writeb(client, M7MU_CATEGORY_CAPCTRL,
				M7MU_CAPCTRL_FRM_PRV_SEL, 0x01);
		CHECK_ERR(err);
	}

	if (state->running_capture_mode != RUNNING_MODE_BURST) {

		CLEAR_ISP_INT1_STATE(sd);

		/* Set YUV out for Preview */
		err = m7mu_writeb(client, M7MU_CATEGORY_CAPPARM,
				M7MU_CAPPARM_YUVOUT_PREVIEW, 0x00);
		CHECK_ERR(err);

#if 0
		/* Set Preview(Postview) Image size */
		if (FRM_RATIO(state->capture) == CAM_FRMRATIO_HD) {
			err = m7mu_writeb(client, M7MU_CATEGORY_CAPPARM,
					M7MU_CAPPARM_PREVIEW_IMG_SIZE, 0x0F);
			CHECK_ERR(err);
		} else if (FRM_RATIO(state->capture) == CAM_FRMRATIO_D1) {
			err = m7mu_writeb(client, M7MU_CATEGORY_CAPPARM,
					M7MU_CAPPARM_PREVIEW_IMG_SIZE, 0x14);
			CHECK_ERR(err);
		} else {
			err = m7mu_writeb(client, M7MU_CATEGORY_CAPPARM,
					M7MU_CAPPARM_PREVIEW_IMG_SIZE, 0x13);
			CHECK_ERR(err);
		}
#endif

		/* Get Preview data */
		err = m7mu_writeb(client, M7MU_CATEGORY_CAPCTRL,
				M7MU_CAPCTRL_TRANSFER, 0x02);
		CHECK_ERR(err);

		/* Clear Interrupt factor */
		int_factor = m7mu_wait_interrupt(sd, M7MU_ISP_TIMEOUT);
		if (!(int_factor & M7MU_INT_CAPTURE)) {
			cam_warn("M7MU_INT_CAPTURE isn't issued on transfer, %#x\n",
					int_factor);
			return -ETIMEDOUT;
		}
	}

/*
	err = m7mu_readl(client, M7MU_CATEGORY_CAPCTRL, M7MU_CAPCTRL_IMG_SIZE,
				&state->jpeg.main_size);
	CHECK_ERR(err);

	err = m7mu_readl(client, M7MU_CATEGORY_CAPCTRL, M7MU_CAPCTRL_THUMB_SIZE,
				&state->jpeg.thumb_size);
	CHECK_ERR(err);

	state->jpeg.main_offset = 0;
	state->jpeg.thumb_offset = M7MU_JPEG_MAXSIZE;
	state->jpeg.postview_offset = M7MU_JPEG_MAXSIZE + M7MU_THUMB_MAXSIZE;

	m7mu_get_exif(sd);
*/
	cam_trace("X\n");
	return err;
}

static int m7mu_start_YUV_capture(struct v4l2_subdev *sd, int frame_num)
{
	struct m7mu_state *state = to_state(sd);
	struct i2c_client *client = to_client(sd);
	int err, int_factor;

	cam_trace("E : %d frame\n", frame_num);

	CLEAR_ISP_INT1_STATE(sd);

	/* Select image number of frame */
	err = m7mu_writeb(client, M7MU_CATEGORY_CAPCTRL,
	M7MU_CAPCTRL_FRM_SEL, frame_num);
	CHECK_ERR(err);

	/* Clear Interrupt factor */
	int_factor = m7mu_wait_interrupt(sd, M7MU_ISP_SHORT_TIMEOUT);
	if (!(int_factor & M7MU_INT_CAPTURE)) {
		cam_warn("M7MU_INT_CAPTURE isn't issued on frame select, %#x\n",
				int_factor);
		return -ETIMEDOUT;
	}

	/* TODO : reg_val NULL check */
	err = m7mu_writeb(client, M7MU_CATEGORY_CAPPARM,
		M7MU_CAPPARM_MAIN_IMG_SIZE, state->capture->reg_val);
	CHECK_ERR(err);
	if (state->smart_zoom_mode)
		m7mu_set_smart_zoom(sd, state->smart_zoom_mode);

	cam_trace("Select sensor image size [ w=%d, h=%d ]\n",
			state->capture->sensor_width,
			state->capture->sensor_height);

	CLEAR_ISP_INT1_STATE(sd);

	/* Get main YUV data */
	err = m7mu_writeb(client, M7MU_CATEGORY_CAPCTRL,
			M7MU_CAPCTRL_TRANSFER, 0x01);
	CHECK_ERR(err);

	/* Clear Interrupt factor */
	int_factor = m7mu_wait_interrupt(sd, M7MU_ISP_LONG_TIMEOUT);
	if (!(int_factor & M7MU_INT_CAPTURE)) {
		cam_warn("M7MU_INT_CAPTURE isn't issued on transfer, %#x\n",
				int_factor);
		return -ETIMEDOUT;
	}

	err = m7mu_readl(client, M7MU_CATEGORY_CAPCTRL, M7MU_CAPCTRL_IMG_SIZE,
				&state->jpeg.main_size);
	CHECK_ERR(err);
/*
	err = m7mu_readl(client, M7MU_CATEGORY_CAPCTRL, M7MU_CAPCTRL_THUMB_SIZE,
				&state->jpeg.thumb_size);
	CHECK_ERR(err);

	state->jpeg.main_offset = 0;
	state->jpeg.thumb_offset = M7MU_JPEG_MAXSIZE;
	state->jpeg.postview_offset = M7MU_JPEG_MAXSIZE + M7MU_THUMB_MAXSIZE;

	m7mu_get_exif(sd);
*/
	cam_trace("X\n");
	return err;
}

static int m7mu_start_YUV_one_capture(struct v4l2_subdev *sd, int frame_num)
{
	struct m7mu_state *state = to_state(sd);
	struct i2c_client *client = to_client(sd);
	int err, int_factor;
	cam_trace("E : %d frame\n", frame_num);

	CLEAR_ISP_INT1_STATE(sd);

	/* Select image number of frame */
	err = m7mu_writeb(client, M7MU_CATEGORY_CAPCTRL,
			M7MU_CAPCTRL_FRM_SEL, 0x01);
	CHECK_ERR(err);

	/* Select main image format */
	err = m7mu_writeb(client, M7MU_CATEGORY_CAPPARM,
			M7MU_CAPPARM_YUVOUT_MAIN, 0x00);
	CHECK_ERR(err);

	/* Select main image size */
	err = m7mu_writeb(client, M7MU_CATEGORY_CAPPARM,
		M7MU_CAPPARM_MAIN_IMG_SIZE, state->capture->reg_val);
	CHECK_ERR(err);

	/* Get main YUV data */
	err = m7mu_writeb(client, M7MU_CATEGORY_CAPCTRL,
			M7MU_CAPCTRL_TRANSFER, 0x01);

	/* Clear Interrupt factor */
	int_factor = m7mu_wait_interrupt(sd, M7MU_ISP_TIMEOUT);
	if (!(int_factor & M7MU_INT_CAPTURE)) {
		cam_warn("M7MU_INT_CAPTURE isn't issued on transfer, %#x\n",
				int_factor);
		return -ETIMEDOUT;
	}

	err = m7mu_readl(client, M7MU_CATEGORY_CAPCTRL, M7MU_CAPCTRL_IMG_SIZE,
				&state->jpeg.main_size);
	CHECK_ERR(err);
	cam_dbg("   ==> main image size=%d\n", state->jpeg.main_size);

	state->jpeg.main_offset = 0;
	state->jpeg.thumb_offset = M7MU_JPEG_MAXSIZE;
	state->jpeg.postview_offset = M7MU_JPEG_MAXSIZE + M7MU_THUMB_MAXSIZE;

	m7mu_get_exif(sd);

	cam_trace("X\n");
	return err;
}


static int m7mu_start_capture(struct v4l2_subdev *sd, int frame_num)
{
	struct m7mu_state *state = to_state(sd);
	struct i2c_client *client = to_client(sd);
	int err, int_factor;
	cam_trace("E : %d frame\n", frame_num);

	state->fast_capture_set = 0;

	if (state->dual_capture_start)
		return m7mu_start_dual_capture(sd, frame_num);

	if (state->running_capture_mode == RUNNING_MODE_AE_BRACKET
		|| state->running_capture_mode == RUNNING_MODE_LOWLIGHT) {

		CLEAR_ISP_INT1_STATE(sd);

		/* Select image number of frame */
		err = m7mu_writeb(client, M7MU_CATEGORY_CAPCTRL,
				M7MU_CAPCTRL_FRM_SEL, frame_num);
		CHECK_ERR(err);

		/* Clear Interrupt factor */
		int_factor = m7mu_wait_interrupt(sd, M7MU_ISP_SHORT_TIMEOUT);
		if (!(int_factor & M7MU_INT_CAPTURE)) {
			cam_warn("M7MU_INT_CAPTURE isn't issued on frame select, %#x\n",
					int_factor);
			return -ETIMEDOUT;
		}
	} else if (state->running_capture_mode == RUNNING_MODE_WB_BRACKET) {
		/* Select image number of frame */
		err = m7mu_writeb(client, M7MU_CATEGORY_CAPCTRL,
				M7MU_CAPCTRL_FRM_SEL, frame_num);
		CHECK_ERR(err);
	} else if (state->running_capture_mode == RUNNING_MODE_BLINK) {

		CLEAR_ISP_INT1_STATE(sd);

		/* Select image number of frame */
		err = m7mu_writeb(client, M7MU_CATEGORY_CAPCTRL,
				M7MU_CAPCTRL_FRM_SEL, 0xFF);
		CHECK_ERR(err);

		/* Clear Interrupt factor */
		int_factor = m7mu_wait_interrupt(sd, M7MU_ISP_TIMEOUT);
		if (!(int_factor & M7MU_INT_CAPTURE)) {
			cam_warn("M7MU_INT_CAPTURE isn't issued on frame select, %#x\n",
					int_factor);
			return -ETIMEDOUT;
		}

		err = m7mu_get_fd_eye_blink_result(sd);
		CHECK_ERR(err);
	} else if (state->running_capture_mode == RUNNING_MODE_RAW) {
		/* Select image number of frame */
		err = m7mu_writeb(client, M7MU_CATEGORY_CAPCTRL,
				M7MU_CAPCTRL_FRM_SEL, 0x01);
		CHECK_ERR(err);

		if (frame_num == 1) {
			/* Set Size */
			err = m7mu_writeb(client, M7MU_CATEGORY_CAPPARM,
				M7MU_CAPPARM_MAIN_IMG_SIZE, 0x33);
			CHECK_ERR(err);
		}
	} else if (state->running_capture_mode == RUNNING_MODE_BURST) {
		err = 0;
	} else if (state->running_capture_mode == RUNNING_MODE_DUMP) {
		int old_mode, val, i;
		err = 0;
		cam_trace("state->running_capture_mode is RUNNING_MODE_DUMP\n");

		CLEAR_ISP_INT1_STATE(sd);

		/* Start Dump mode */
		m7mu_readb(client, M7MU_CATEGORY_SYS, M7MU_SYS_MODE, &old_mode);
		err = m7mu_writeb(client, M7MU_CATEGORY_SYS,
				M7MU_SYS_MODE, 0x05);
		if (err <= 0)
			return err;

		for (i = M7MU_I2C_VERIFY; i; i--) {
			err = m7mu_readb(client, M7MU_CATEGORY_SYS,
					M7MU_SYS_MODE, &val);
			if (err <= 0)
				return err;
			if (val == 0x05)
				break;
			msleep(20);
		}

		int_factor = m7mu_wait_interrupt(sd, M7MU_ISP_CAPTURE_TIMEOUT);
		if (!(int_factor & M7MU_INT_CAPTURE)) {
			cam_warn("M7MU_INT_CAPTURE isn't issued, %#x\n",
					int_factor);
			return -ETIMEDOUT;
		}

		CLEAR_ISP_INT1_STATE(sd);

		/* Get main image JPEG size */
		err = m7mu_readl(client, M7MU_CATEGORY_CAPCTRL,
			M7MU_CAPCTRL_IMG_SIZE, &state->jpeg.main_size);
		CHECK_ERR(err);
		cam_trace("~~~~~~ main_size : 0x%x ~~~~~~\n", state->jpeg.main_size);
		if(state->jpeg.main_size == 0) {
			cam_warn("ISP log size : 0 !!\n");
			return 0;
		}

		/* Start Transfer Custom Data */
		err = m7mu_writeb(client, M7MU_CATEGORY_SYS, 0x37, 0x01);
		if (err <= 0)
			return err;
		int_factor = m7mu_wait_interrupt(sd, M7MU_ISP_CAPTURE_TIMEOUT);
		if (!(int_factor & M7MU_INT_CAPTURE)) {
			cam_warn("M7MU_INT_CAPTURE isn't issued, %#x\n",
					int_factor);
			return -ETIMEDOUT;
		}

#if 0
		/* Change to previous mode */
		err = m7mu_writeb(client, M7MU_CATEGORY_SYS,
				M7MU_SYS_MODE, old_mode);
		if (err <= 0)
			return err;

		for (i = M7MU_I2C_VERIFY; i; i--) {
			err = m7mu_readb(client, M7MU_CATEGORY_SYS,
					M7MU_SYS_MODE, &val);
			if (err <= 0)
				return err;
			if (val == old_mode)
				break;
			msleep(20);
		}
#endif
#if 0
		int_factor = m7mu_wait_interrupt(sd, M7MU_ISP_CAPTURE_TIMEOUT);
		if (!(int_factor & M7MU_INT_CAPTURE)) {
			cam_warn("M7MU_INT_CAPTURE isn't issued, %#x\n",
					int_factor);
			return -ETIMEDOUT;
		}
#else
		return 0;
#endif
	} else {
		/* Select image number of frame */
		err = m7mu_writeb(client, M7MU_CATEGORY_CAPCTRL,
				M7MU_CAPCTRL_FRM_SEL, 0x01);
		CHECK_ERR(err);
	}

	m7mu_get_red_eye_fix_result(sd);

#if 0
	/* Set main image JPEG fime max size */
	err = m7mu_writel(client, M7MU_CATEGORY_CAPPARM,
			M7MU_CAPPARM_JPEG_SIZE_MAX, 0x01000000);
	CHECK_ERR(err);

	/* Set main image JPEG fime min size */
	err = m7mu_writel(client, M7MU_CATEGORY_CAPPARM,
			M7MU_CAPPARM_JPEG_SIZE_MIN, 0x00100000);
	CHECK_ERR(err);
#endif

	CLEAR_ISP_INT1_STATE(sd);

	/* Get main JPEG data */
	err = m7mu_writeb(client, M7MU_CATEGORY_CAPCTRL,
			M7MU_CAPCTRL_TRANSFER, 0x01);

	if (state->running_capture_mode == RUNNING_MODE_BURST) {
#if 0
		int i;
		for (i = 0;  i < 3; i++) { /*wait M7MU_INT_CAPTURE*/

			int_factor = m7mu_wait_interrupt(sd, M7MU_ISP_TIMEOUT);
			if (int_factor & (M7MU_INT_FRAME_SYNC|M7MU_INT_SOUND)) {
				cam_trace("----skip interrupt=%x", int_factor);
				continue;
			}

			if (!(int_factor & M7MU_INT_CAPTURE)) {
				cam_warn("M7MU_INT_CAPTURE isn't issued on transfer, %#x\n",
						int_factor);
				return -ETIMEDOUT;
			}
			break;
		}
#else
		int_factor = m7mu_wait_interrupt(sd, M7MU_ISP_SHORT_TIMEOUT);
		if (!(int_factor & M7MU_INT_CAPTURE)) {
			cam_warn("M7MU_INT_CAPTURE isn't issued on transfer, %#x\n",
					int_factor);
			return -ETIMEDOUT;
		}
#endif
	} else {
		/* Clear Interrupt factor */
		int_factor = m7mu_wait_interrupt(sd, M7MU_ISP_CAPTURE_TIMEOUT);
		if (!(int_factor & M7MU_INT_CAPTURE)) {
			cam_warn("M7MU_INT_CAPTURE isn't issued on transfer, %#x\n",
					int_factor);
			return -ETIMEDOUT;
		}
	}

#if 0
	if (state->running_capture_mode == RUNNING_MODE_SINGLE) {
		cam_trace("~V4L2_CID_CAMERA_SET_PRE_LIVEVIEW ~\n");
		err = m7mu_writeb(client, M7MU_CATEGORY_LENS, 0x7C, 0x01);
		CHECK_ERR(err);
	}
#endif

	err = m7mu_readl(client, M7MU_CATEGORY_CAPCTRL, M7MU_CAPCTRL_IMG_SIZE,
				&state->jpeg.main_size);
	CHECK_ERR(err);
	cam_dbg("   ==> jpeg size=%d\n", state->jpeg.main_size);

	state->jpeg.main_offset = 0;
	state->jpeg.thumb_offset = M7MU_JPEG_MAXSIZE;
	state->jpeg.postview_offset = M7MU_JPEG_MAXSIZE + M7MU_THUMB_MAXSIZE;

	if (state->running_capture_mode != RUNNING_MODE_RAW) {
		if (state->running_capture_mode != RUNNING_MODE_LOWLIGHT)
			m7mu_get_exif(sd);
	} else {
		if (frame_num == 1) {
			m7mu_get_exif(sd);

			CLEAR_ISP_INT1_STATE(sd);

			err = m7mu_set_mode(sd, M7MU_MONITOR_MODE);
			if (err <= 0) {
				cam_err("failed to set mode\n");
				return err;
			}

			err = m7mu_wait_interrupt(sd, M7MU_ISP_TIMEOUT);
			if (!(err & M7MU_INT_MODE)) {
				cam_err("m7mu_start_capture() MONITOR_MODE error\n");
				return -ETIMEDOUT;
			}

		err = m7mu_writeb(client, M7MU_CATEGORY_CAPPARM,
				0x78, 0x00);
		CHECK_ERR(err);
		}
	}

	state->focus.touch = 0;

	cam_trace("X\n");
	return err;
}

static int m7mu_start_combined_capture(struct v4l2_subdev *sd, int frame_num)
{
	struct m7mu_state *state = to_state(sd);
	struct i2c_client *client = to_client(sd);
	int err;
	int int_factor;

	CLEAR_ISP_INT1_STATE(sd);

	/* Get combined data */
	err = m7mu_writeb(client, M7MU_CATEGORY_CAPCTRL,
			M7MU_CAPCTRL_TRANSFER, 0x01);
	/* Clear Interrupt factor */
	int_factor = m7mu_wait_interrupt(sd, M7MU_ISP_SHORT_TIMEOUT);
	if (!(int_factor & M7MU_INT_CAPTURE)) {
		cam_warn("M7MU_INT_CAPTURE isn't issued on transfer, %#x\n",
				int_factor);
		return -ETIMEDOUT;
	}

	err = m7mu_readl(client, M7MU_CATEGORY_CAPCTRL, M7MU_CAPCTRL_IMG_SIZE,
				&state->jpeg.main_size);
	CHECK_ERR(err);
	cam_dbg("   ==> jpeg size=%d\n", state->jpeg.main_size);

	state->jpeg.main_offset = 0;
	state->jpeg.thumb_offset = M7MU_JPEG_MAXSIZE;
	state->jpeg.postview_offset = M7MU_JPEG_MAXSIZE + M7MU_THUMB_MAXSIZE;

	if (frame_num == 0)
		m7mu_get_exif(sd);
	return err;
}

/*static int m7mu_set_hdr(struct v4l2_subdev *sd, int val)
{
	cam_trace("E val : %d\n", val);
	cam_trace("X\n");
	return 0;
}*/

static int m7mu_start_capture_thumb(struct v4l2_subdev *sd, int frame_num)
{
	struct m7mu_state *state = to_state(sd);
	struct i2c_client *client = to_client(sd);
	int err, int_factor;
	cam_trace("E : %d frame\n", frame_num);

	cam_dbg("m7mu_start_capture_thumb() num=%d\n", frame_num);

	CLEAR_ISP_INT1_STATE(sd);

	/* Select image number of frame */
	err = m7mu_writeb(client, M7MU_CATEGORY_CAPCTRL,
			M7MU_CAPCTRL_FRM_THUMB_SEL, frame_num);
	CHECK_ERR(err);

	/* Clear Interrupt factor */
	int_factor = m7mu_wait_interrupt(sd, M7MU_ISP_TIMEOUT);
	if (!(int_factor & M7MU_INT_CAPTURE)) {
		cam_warn("M7MU_INT_CAPTURE isn't issued on frame select, %#x\n",
				int_factor);
		return -ETIMEDOUT;
	}

	CLEAR_ISP_INT1_STATE(sd);

	err = m7mu_writeb(client, M7MU_CATEGORY_CAPPARM,
			M7MU_CAPPARM_YUVOUT_THUMB, 0x01);
	CHECK_ERR(err);

	err = m7mu_writeb(client, M7MU_CATEGORY_CAPPARM,
			M7MU_CAPPARM_THUMB_IMG_SIZE, 0x04);  /* 320 x 240 */
	CHECK_ERR(err);

	/* Get main thumb data */
	err = m7mu_writeb(client, M7MU_CATEGORY_CAPCTRL,
			M7MU_CAPCTRL_TRANSFER, 0x03);

	/* Clear Interrupt factor */
	int_factor = m7mu_wait_interrupt(sd, M7MU_ISP_TIMEOUT);
	if (!(int_factor & M7MU_INT_CAPTURE)) {
		cam_warn("M7MU_INT_CAPTURE isn't issued on transfer, %#x\n",
				int_factor);
		return -ETIMEDOUT;
	}

	err = m7mu_readl(client, M7MU_CATEGORY_CAPCTRL, M7MU_CAPCTRL_THUMB_SIZE,
				&state->jpeg.thumb_size);
	CHECK_ERR(err);

	return err;
}

static int m7mu_set_facedetect(struct v4l2_subdev *sd, int val)
{
	int err;
	struct m7mu_state *state = to_state(sd);
	struct i2c_client *client = to_client(sd);

	cam_trace("E val : %d\n", val);

	state->facedetect_mode = val;

	switch (state->facedetect_mode) {
	case V4L2_FACE_DETECTION_NORMAL:
	case V4L2_FACE_DETECTION_BLINK:
		cam_dbg("~~~~~~ face detect on ~~~~~~ val : %d\n", val);
		err = m7mu_writeb(client, M7MU_CATEGORY_FD, M7MU_FD_SIZE, 0x04);
		CHECK_ERR(err);
		err = m7mu_writeb(client, M7MU_CATEGORY_FD, M7MU_FD_MAX, 0x07);
		CHECK_ERR(err);
		if (state->mode == MODE_SMART_AUTO
			|| state->mode == MODE_BEAUTY_SHOT) {
			err = m7mu_writeb(client, M7MU_CATEGORY_FD,
					M7MU_FD_CTL, 0x01);
		} else if (state->mode == MODE_SMART_SUGGEST) {
			cam_trace("@@@ SMART_SUGGEST : set 0x91");
			/* for turning on the FD gender : 0x09 0x00 0x91 */
			err = m7mu_writeb(client, M7MU_CATEGORY_FD,
					M7MU_FD_CTL, 0x91);
		} else {
			err = m7mu_writeb(client, M7MU_CATEGORY_FD,
					M7MU_FD_CTL, 0x11);
		}
		CHECK_ERR(err);

		if (state->isp_mode == M7MU_MONITOR_MODE)
			msleep(30);

		break;

	case V4L2_FACE_DETECTION_SMILE_SHOT:
		cam_dbg("~~~~~~ fd smile shot ~~~~~~ val : %d\n", val);
		/* When Smile-shot mode, FD function of ISP don't need. */
		/* So In Smile-shot mode, FD Off */
		/*break;*/

	case V4L2_FACE_DETECTION_OFF:
	default:
		cam_dbg("~~~~~~ face detect off ~~~~~~ val : %d\n", val);
		err = m7mu_writeb(client, M7MU_CATEGORY_FD, M7MU_FD_CTL, 0x00);
		CHECK_ERR(err);
		break;
	}
	cam_trace("X\n");
	return 0;
}

static int m7mu_set_bracket_aeb(struct v4l2_subdev *sd, int val)
{
	int err;
	struct i2c_client *client = to_client(sd);

	cam_trace("E val : %d\n", val);

	switch (val) {
	case BRACKET_AEB_VALUE0:
		cam_dbg("~~~~~~ AEB value0 ~~~~~~ val : %d\n", val);
		err = m7mu_writeb(client, M7MU_CATEGORY_AE,
				M7MU_AE_AUTO_BRACKET_EV, 0x00); /* EV 0.0 */
		CHECK_ERR(err);
		break;

	case BRACKET_AEB_VALUE1:
		cam_dbg("~~~~~~ AEB value1 ~~~~~~ val : %d\n", val);
		err = m7mu_writeb(client, M7MU_CATEGORY_AE,
				M7MU_AE_AUTO_BRACKET_EV, 0x1E); /* EV 0.3 */
		CHECK_ERR(err);
		break;

	case BRACKET_AEB_VALUE2:
		cam_dbg("~~~~~~ AEB value2 ~~~~~~ val : %d\n", val);
		err = m7mu_writeb(client, M7MU_CATEGORY_AE,
				M7MU_AE_AUTO_BRACKET_EV, 0x3C); /* EV 0.6 */
		CHECK_ERR(err);
		break;

	case BRACKET_AEB_VALUE3:
		cam_dbg("~~~~~~ AEB value3 ~~~~~~ val : %d\n", val);
		err = m7mu_writeb(client, M7MU_CATEGORY_AE,
				M7MU_AE_AUTO_BRACKET_EV, 0x64); /* EV 1.0 */
		CHECK_ERR(err);
		break;

	case BRACKET_AEB_VALUE4:
		cam_dbg("~~~~~~ AEB value4 ~~~~~~ val : %d\n", val);
		err = m7mu_writeb(client, M7MU_CATEGORY_AE,
				M7MU_AE_AUTO_BRACKET_EV, 0x82); /* EV 1.3 */
		CHECK_ERR(err);
		break;

	case BRACKET_AEB_VALUE5:
		cam_dbg("~~~~~~ AEB value5 ~~~~~~ val : %d\n", val);
		err = m7mu_writeb(client, M7MU_CATEGORY_AE,
				M7MU_AE_AUTO_BRACKET_EV, 0xA0); /* EV 1.6 */
		CHECK_ERR(err);
		break;

	case BRACKET_AEB_VALUE6:
		cam_dbg("~~~~~~ AEB value6 ~~~~~~ val : %d\n", val);
		err = m7mu_writeb(client, M7MU_CATEGORY_AE,
				M7MU_AE_AUTO_BRACKET_EV, 0xC8); /* EV 2.0 */
		CHECK_ERR(err);
		break;

	default:
		cam_err("~~~~ TBD ~~~~ val : %d", val);
		err = m7mu_writeb(client, M7MU_CATEGORY_AE,
				M7MU_AE_AUTO_BRACKET_EV, 0x64); /* Ev 1.0 */
		CHECK_ERR(err);
		break;
	}
	cam_trace("X\n");
	return 0;
}

static int m7mu_set_bracket_wbb(struct v4l2_subdev *sd, int val)
{
	struct m7mu_state *state = to_state(sd);
	struct i2c_client *client = to_client(sd);
	int err;
	cam_trace("E val : %d\n", val);

	switch (val) {
	case BRACKET_WBB_VALUE1:
		cam_trace("~~~~~~ WBB value1  AB 3~~~~~~ val : %d\n", val);
		err = m7mu_writeb(client, M7MU_CATEGORY_WB,
				M7MU_WB_WBB_MODE, 0x01);
		CHECK_ERR(err);
		err = m7mu_writeb(client, M7MU_CATEGORY_WB,
				M7MU_WB_WBB_AB, 0x30);
		CHECK_ERR(err);
		break;

	case BRACKET_WBB_VALUE2:
		cam_trace("~~~~~~ WBB value2  AB 2~~~~~~ val : %d\n", val);
		err = m7mu_writeb(client, M7MU_CATEGORY_WB,
				M7MU_WB_WBB_MODE, 0x01);
		CHECK_ERR(err);
		err = m7mu_writeb(client, M7MU_CATEGORY_WB,
				M7MU_WB_WBB_AB, 0x20);
		CHECK_ERR(err);
		break;

	case BRACKET_WBB_VALUE3:
		cam_trace("~~~~~~ WBB value3  AB 1~~~~~~ val : %d\n", val);
		err = m7mu_writeb(client, M7MU_CATEGORY_WB,
				M7MU_WB_WBB_MODE, 0x01);
		CHECK_ERR(err);
		err = m7mu_writeb(client, M7MU_CATEGORY_WB,
				M7MU_WB_WBB_AB, 0x0F);
		CHECK_ERR(err);
		break;

	case BRACKET_WBB_VALUE4:
		cam_trace("~~~~~~ WBB value4  GM 3~~~~~~ val : %d\n", val);
		err = m7mu_writeb(client, M7MU_CATEGORY_WB,
				M7MU_WB_WBB_MODE, 0x02);
		CHECK_ERR(err);
		err = m7mu_writeb(client, M7MU_CATEGORY_WB,
				M7MU_WB_WBB_GM, 0x30);
		CHECK_ERR(err);
		break;

	case BRACKET_WBB_VALUE5:
		cam_trace("~~~~~~ WBB value5  GM 2~~~~~~ val : %d\n", val);
		err = m7mu_writeb(client, M7MU_CATEGORY_WB,
				M7MU_WB_WBB_MODE, 0x02);
		CHECK_ERR(err);
		err = m7mu_writeb(client, M7MU_CATEGORY_WB,
				M7MU_WB_WBB_GM, 0x20);
		CHECK_ERR(err);
		break;

	case BRACKET_WBB_VALUE6:
		cam_trace("~~~~~~ WBB value6  GM 1~~~~~~ val : %d\n", val);
		err = m7mu_writeb(client, M7MU_CATEGORY_WB,
				M7MU_WB_WBB_MODE, 0x02);
		CHECK_ERR(err);
		err = m7mu_writeb(client, M7MU_CATEGORY_WB,
				M7MU_WB_WBB_GM, 0x0F);
		CHECK_ERR(err);
		break;

	case BRACKET_WBB_OFF:
		cam_trace("~~~~~~ WBB Off ~~~~~~\n");
		err = m7mu_writeb(client, M7MU_CATEGORY_WB,
				M7MU_WB_WBB_MODE, 0x00);
		CHECK_ERR(err);
		break;

	default:
		val = 0xFF;
		cam_err("~~~~ TBD ~~~~ val : %d", val);
		break;
	}

	if (val != 0xFF)
		state->bracket_wbb_val = val;

	cam_trace("X\n");
	return 0;
}

static int m7mu_set_bracket(struct v4l2_subdev *sd, int val)
{
	struct m7mu_state *state = to_state(sd);
	cam_trace("E val : %d\n", val);

	switch (val) {
	case BRACKET_MODE_OFF:
	case BRACKET_MODE_AEB:
		cam_dbg("~~~~~~ bracket aeb on ~~~~~~ val : %d\n", val);
		m7mu_set_bracket_wbb(sd, BRACKET_WBB_OFF);
		break;

	case BRACKET_MODE_WBB:
		cam_dbg("~~~~~~ bracket wbb on ~~~~~~ val : %d\n", val);
		if (state->bracket_wbb_val == BRACKET_WBB_OFF)
			state->bracket_wbb_val = BRACKET_WBB_VALUE3;
		m7mu_set_bracket_wbb(sd, state->bracket_wbb_val);
		break;

	default:
		cam_err("~~~~ TBD ~~~~ val : %d", val);
		break;
	}
	cam_trace("X\n");
	return 0;
}

static int m7mu_set_factory_cam_sys_mode(struct v4l2_subdev *sd, int val)
{
	int old_mode;

	cam_trace("E val : %d\n", val);

	switch (val) {
	case FACTORY_SYSMODE_CAPTURE:
		cam_trace("~ FACTORY_SYSMODE_CAPTURE ~\n");
		old_mode = m7mu_set_mode(sd, M7MU_STILLCAP_MODE);
		break;

	case FACTORY_SYSMODE_MONITOR:
		break;

	case FACTORY_SYSMODE_PARAM:
		cam_trace("~ FACTORY_SYSMODE_PARAM ~\n");
		old_mode = m7mu_set_mode(sd, M7MU_PARMSET_MODE);
		break;

	default:
		cam_trace("~ FACTORY_SYSMODE_DEFAULT ~\n");
		break;
	}
	cam_trace("X\n");
	return 0;

}

static int m7mu_set_fps(struct v4l2_subdev *sd, int val)
{
	int err;
	int current_mode;
	bool mode_changed;

	struct m7mu_state *state = to_state(sd);
	struct i2c_client *client = to_client(sd);
	cam_trace("E val : %d\n", val);

	if (val == state->fps) {
		cam_info("same fps. skip\n");
		return 0;
	}
	if (val <= 0 || val > 120) {
		cam_err("invalid frame rate %d\n", val);
		val = 0; /* set to auto(default) */
	}

	cam_info("set AE EP to %d\n", val);
	mode_changed = false;
	if (((val != 24) && (state->fps == 24)) ||
		((val != 30) && (state->fps == 30)) ||
		((val != 15) && (state->fps == 15))) {
		err = m7mu_readb(client, M7MU_CATEGORY_SYS,
				M7MU_SYS_MODE, &current_mode);
		CHECK_ERR(err);
		if (current_mode != M7MU_PARMSET_MODE) {
			err = m7mu_set_mode(sd, M7MU_PARMSET_MODE);
			if (err <= 0) {
				cam_err("failed to set mode\n");
				return err;
			}
			mode_changed = true;
		}

		err = m7mu_writeb(client, M7MU_CATEGORY_PARM,
			M7MU_PARM_FLEX_FPS, 0x00);
		CHECK_ERR(err);
	}

	switch (val) {
	case 120:
		cam_trace("~~~~~~ 120 fps ~~~~~~%s\n",
			state->mode == MODE_GOLF_SHOT ? " Golf Shot mode" : "");
		if (state->mode == MODE_GOLF_SHOT) {
			err = m7mu_writeb(client, M7MU_CATEGORY_AE,
					M7MU_AE_EP_MODE_MON, 0x12);
			CHECK_ERR(err);
			err = m7mu_writeb(client, M7MU_CATEGORY_AE,
					M7MU_AE_EP_MODE_CAP, 0x12);
			CHECK_ERR(err);
		} else {
			err = m7mu_writeb(client, M7MU_CATEGORY_AE,
					M7MU_AE_EP_MODE_MON, 0x1C);
			CHECK_ERR(err);
			err = m7mu_writeb(client, M7MU_CATEGORY_AE,
					M7MU_AE_EP_MODE_CAP, 0x1C);
			CHECK_ERR(err);
		}
		break;

	case 60:
		cam_trace("~~~~~~ 60 fps ~~~~~~\n");
		err = m7mu_writeb(client, M7MU_CATEGORY_AE,
				M7MU_AE_EP_MODE_MON, 0x1A);
		CHECK_ERR(err);
		err = m7mu_writeb(client, M7MU_CATEGORY_AE,
				M7MU_AE_EP_MODE_CAP, 0x1A);
		CHECK_ERR(err);
		break;

	case 30:
		cam_trace("~~~~~~ 30 fps ~~~~~~\n");
		err = m7mu_writeb(client, M7MU_CATEGORY_AE,
				M7MU_AE_EP_MODE_MON, 0x19);
		CHECK_ERR(err);
		err = m7mu_writeb(client, M7MU_CATEGORY_AE,
				M7MU_AE_EP_MODE_CAP, 0x19);
		CHECK_ERR(err);
		if (state->sensor_mode != SENSOR_MOVIE) {
			err = m7mu_writeb(client, M7MU_CATEGORY_PARM,
				M7MU_PARM_FLEX_FPS, 30);
		}
		CHECK_ERR(err);
		break;

	case 24:
		cam_trace("~~~~~~ 24 fps ~~~~~~\n");
		err = m7mu_readb(client, M7MU_CATEGORY_SYS,
				M7MU_SYS_MODE, &current_mode);
		CHECK_ERR(err);
		if (current_mode != M7MU_PARMSET_MODE) {
			err = m7mu_set_mode(sd, M7MU_PARMSET_MODE);
			if (err <= 0) {
				cam_err("failed to set mode\n");
				return err;
			}
			mode_changed = true;
		}
		err = m7mu_writeb(client, M7MU_CATEGORY_PARM,
				M7MU_PARM_FLEX_FPS, 0x18);
		CHECK_ERR(err);
		break;

	case 15:
		cam_trace("~~~~~~ 15 fps ~~~~~~\n");
		err = m7mu_readb(client, M7MU_CATEGORY_SYS,
				M7MU_SYS_MODE, &current_mode);
		CHECK_ERR(err);
		if (current_mode != M7MU_PARMSET_MODE) {
			err = m7mu_set_mode(sd, M7MU_PARMSET_MODE);
			if (err <= 0) {
				cam_err("failed to set mode\n");
				return err;
			}
			mode_changed = true;
		}
		err = m7mu_writeb(client, M7MU_CATEGORY_PARM,
				M7MU_PARM_FLEX_FPS, 0xF);
		CHECK_ERR(err);
		break;

#if 0	/* after ISP update */
	case 15:
		cam_trace("~~~~~~ 15 fps ~~~~~~\n");
		err = m7mu_writeb(client, M7MU_CATEGORY_PARM,
				M7MU_PARM_MON_FPS, 0x03);
		CHECK_ERR(err);
		break;
#endif

	default:
		cam_trace("~~~~~~ default : auto fps ~~~~~~\n");
		err = m7mu_writeb(client, M7MU_CATEGORY_AE,
				M7MU_AE_EP_MODE_MON, 0x09);
		CHECK_ERR(err);
		err = m7mu_writeb(client, M7MU_CATEGORY_AE,
				M7MU_AE_EP_MODE_CAP, 0x09);
		CHECK_ERR(err);
		break;
	}

#if 0	/* after ISP update */
	if (state->fps == 15 && val != 15) {
		err = m7mu_writeb(client, M7MU_CATEGORY_PARM,
				M7MU_PARM_MON_FPS, 0x01);
		CHECK_ERR(err);
	}
#endif

	if (mode_changed) {
		int old_mode = 0;

		CLEAR_ISP_INT1_STATE(sd);

		old_mode = m7mu_set_mode(sd, current_mode);
		if (old_mode <= 0) {
			cam_err("failed to set mode\n");
			return err;
		}

		if (old_mode != M7MU_MONITOR_MODE &&
				current_mode == M7MU_MONITOR_MODE) {
			int int_factor;
			int_factor = m7mu_wait_interrupt(sd, M7MU_ISP_SHORT_TIMEOUT);
			if (!(int_factor & M7MU_INT_MODE)) {
				cam_err("M7MU_INT_MODE isn't issued, %#x\n",
					int_factor);
				return -ETIMEDOUT;
			}
		}
	}

	state->fps = val;

	m7mu_set_OIS_cap_mode(sd);

	cam_trace("X\n");
	return 0;
}

static int m7mu_set_time_info(struct v4l2_subdev *sd, int val)
{
	int err;
	struct i2c_client *client = to_client(sd);
#if 0
	int read_hour, read_min;
#endif

	cam_trace("E val : %02d:%02d\n", ((val >> 8) & 0xFF), (val & 0xFF));

	err = m7mu_writew(client, M7MU_CATEGORY_NEW,
		M7MU_NEW_TIME_INFO, val);
	CHECK_ERR(err);

#if 0	/* for check time */
	err = m7mu_readb(client, M7MU_CATEGORY_NEW,
		M7MU_NEW_TIME_INFO, &read_hour);
	CHECK_ERR(err);

	err = m7mu_readb(client, M7MU_CATEGORY_NEW,
		M7MU_NEW_TIME_INFO+1, &read_min);
	CHECK_ERR(err);

	cam_dbg("time %02d:%02d\n", read_hour, read_min);
#endif

	cam_trace("X\n");
	return 0;
}

static int m7mu_set_lens_off_timer(struct v4l2_subdev *sd, int val)
{
	int err;
	struct i2c_client *client = to_client(sd);

	cam_trace("E val : %d\n", val);
#if 1
	cam_trace("Lens off timer is disabled.\n");
	return 1;
#endif

	if (val > 0xFF) {
		cam_warn("Can not set over 0xFF, but set 0x%x", val);
		val = 0xFF;
	}

	err = m7mu_writeb2(client, M7MU_CATEGORY_SYS,
		M7MU_SYS_LENS_TIMER, val);
	CHECK_ERR(err);

	cam_trace("X\n");
	return 1;
}

static int m7mu_set_widget_mode_level(struct v4l2_subdev *sd, int val)
{
	struct m7mu_state *state = to_state(sd);
	struct i2c_client *client = to_client(sd);
	int err;
	int /*denominator = 500,*/ numerator = 8;
	u32 f_number = 0x45;

	/* 3 step -> 2 step, low level is not used */
	if (val == 1)
		val = 2;

	/* valid values are 0, 2, 4 */
	state->widget_mode_level = val * 2 - 2;

	switch (state->mode) {
	case MODE_HIGH_SPEED:
		state->widget_mode_level = 4;
		break;

	case MODE_LIGHT_TRAIL_SHOT:
		state->widget_mode_level = 4;
		break;

	case MODE_WATERFALL:
		state->widget_mode_level = 4;
		break;

	case MODE_FIREWORKS:
		state->widget_mode_level = 2;
		break;

	case MODE_SILHOUETTE:
		state->widget_mode_level = 2;
		break;

	case MODE_SUNSET:
		state->widget_mode_level = 2;
		break;

	case MODE_NATURAL_GREEN:
		state->widget_mode_level = 4;
		break;

	case MODE_CLOSE_UP:
		state->widget_mode_level = 4;
		break;
	}

	/* LIKE A PRO STEP SET */
	err = m7mu_writeb(client, M7MU_CATEGORY_PRO_MODE,
		0x02, state->widget_mode_level);
	CHECK_ERR(err);

	if (state->mode == MODE_SILHOUETTE) {
		err = m7mu_writeb(client, M7MU_CATEGORY_CAPPARM,
			0x41, 0x0);
		CHECK_ERR(err);
		err = m7mu_writeb(client, M7MU_CATEGORY_CAPPARM,
			0x42, 0x0D + state->widget_mode_level);
		CHECK_ERR(err);
	} else if (state->mode == MODE_NATURAL_GREEN) {
		/* COLOR EFFECT SET */
		err = m7mu_writeb(client, M7MU_CATEGORY_MON,
			M7MU_MON_COLOR_EFFECT, 0x21 + state->widget_mode_level);
		CHECK_ERR(err);
	} else if (state->mode == MODE_FIREWORKS) {
		/* Set Capture Shutter Speed Time */
		err = m7mu_writew(client, M7MU_CATEGORY_AE,
			M7MU_AE_EV_PRG_SS_NUMERATOR, 32);
		CHECK_ERR(err);
		err = m7mu_writew(client, M7MU_CATEGORY_AE,
			M7MU_AE_EV_PRG_SS_DENOMINATOR, 10);
		CHECK_ERR(err);

		/* Set Still Capture F-Number Value */
		err = m7mu_writeb(client, M7MU_CATEGORY_AE,
			M7MU_AE_EV_PRG_F_NUMBER, 0x80);
		CHECK_ERR(err);

	} else if (state->mode == MODE_LIGHT_TRAIL_SHOT) {
		/* Set Capture Shutter Speed Time */
		if (state->widget_mode_level == 0)
			numerator = 3;
		else if (state->widget_mode_level == 2)
			numerator = 5;
		else if (state->widget_mode_level == 4)
			numerator = 10;

		err = m7mu_writew(client, M7MU_CATEGORY_AE,
			M7MU_AE_EV_PRG_SS_NUMERATOR, numerator);
		CHECK_ERR(err);
		err = m7mu_writew(client, M7MU_CATEGORY_AE,
			M7MU_AE_EV_PRG_SS_DENOMINATOR, 1);
		CHECK_ERR(err);
	} else if (state->mode == MODE_HIGH_SPEED) {
		/* Set Still Capture EV program mode */
		if (state->widget_mode_level == 2) {
			err = m7mu_writeb(client, M7MU_CATEGORY_AE,
				M7MU_AE_EP_MODE_CAP, 0x14);
			CHECK_ERR(err);
		} else if (state->widget_mode_level == 4) {
			err = m7mu_writeb(client, M7MU_CATEGORY_AE,
				M7MU_AE_EP_MODE_CAP, 0x13);
			CHECK_ERR(err);
		}
	} else if (state->mode == MODE_CLOSE_UP) {
		/* Set Still Capture F-Number Value */
		if (state->widget_mode_level == 0)
			f_number = 0x80;
		else if (state->widget_mode_level == 2)
			f_number = 0x45;
		else if (state->widget_mode_level == 4)
			f_number = 0x28;

		err = m7mu_writeb(client, M7MU_CATEGORY_AE,
			M7MU_AE_EV_PRG_F_NUMBER, f_number);
		CHECK_ERR(err);
	}

	cam_dbg("X %d %d\n", val, state->mode);
	return 0;
}

static int m7mu_set_LDC(struct v4l2_subdev *sd, int val)
{
	int err;
	struct i2c_client *client = to_client(sd);

	cam_dbg("%s\n", val ? "on" : "off");

	if (val == 1) {
		err = m7mu_writeb(client, M7MU_CATEGORY_PARM,
			0x1B, 0x01);
		CHECK_ERR(err);
	} else {
		err = m7mu_writeb(client, M7MU_CATEGORY_PARM,
			0x1B, 0x00);
		CHECK_ERR(err);
	}

	cam_trace("X\n");
	return 0;
}

static int m7mu_set_LSC(struct v4l2_subdev *sd, int val)
{
	int err;
	struct i2c_client *client = to_client(sd);

	cam_dbg("%s\n", val ? "on" : "off");

	if (val == 1) {
		err = m7mu_writeb(client, M7MU_CATEGORY_PARM,
			0x07, 0x01);
		CHECK_ERR(err);
	} else {
		err = m7mu_writeb(client, M7MU_CATEGORY_PARM,
			0x07, 0x00);
		CHECK_ERR(err);
	}

	cam_trace("X\n");
	return 0;
}

static int m7mu_set_aperture_preview(struct v4l2_subdev *sd, int val)
{
	int err;
	unsigned char convert = 0x00;
	struct i2c_client *client = to_client(sd);

	cam_trace("E val : %d\n", val);

	convert = (unsigned char)val;

	cam_trace("check val : %d\n", convert);
	err = m7mu_writeb(client, M7MU_CATEGORY_AE, 0x3D, convert);

	CHECK_ERR(err);
	cam_trace("X\n");
	return 0;
}

static int m7mu_set_aperture_capture(struct v4l2_subdev *sd, int val)
{
	int err;
	unsigned char convert = 0x00;
	struct i2c_client *client = to_client(sd);

	cam_trace("E val : %d\n", val);

	convert = (unsigned char)val;

	cam_trace("check val : %d\n", convert);
	err = m7mu_writeb(client, M7MU_CATEGORY_AE,
			0x36, convert);
	CHECK_ERR(err);
	cam_trace("X\n");
	return 0;
}

static int m7mu_set_factory_OIS(struct v4l2_subdev *sd, int val)
{
	struct m7mu_state *state = to_state(sd);
	struct i2c_client *client = to_client(sd);
	int err;

	cam_trace("E val : %d\n", val);

	switch (val) {
	case FACTORY_OIS_RETURN_TO_CENTER:
		cam_trace("~ FACTORY_OIS_RETURN_TO_CENTER   ~\n");
		err = m7mu_writeb(client, M7MU_CATEGORY_NEW,
			0x15, 0x30);
		CHECK_ERR(err);
		err = m7mu_writeb(client, M7MU_CATEGORY_NEW,
			0x16, 0x11);
		CHECK_ERR(err);
		break;

	case FACTORY_OIS_RUN:
		cam_trace("~ FACTORY_OIS_RUN ~\n");
		err = m7mu_writeb(client, M7MU_CATEGORY_NEW,
			0x11, 0x01);
		CHECK_ERR(err);
		break;

	case FACTORY_OIS_START:
		cam_trace("~ FACTORY_OIS_START ~\n");
		err = m7mu_writeb(client, M7MU_CATEGORY_NEW,
			0x20, 0x01);
		CHECK_ERR(err);
		break;

	case FACTORY_OIS_STOP:
		cam_trace("~ FACTORY_OIS_STOP ~\n");
		break;

	case FACTORY_OIS_MODE_ON:
		err = m7mu_writeb(client, M7MU_CATEGORY_NEW,
			0x11, 0x02);
		CHECK_ERR(err);
		cam_trace("~ FACTORY_OIS_MODE_ON ~\n");
		break;

	case FACTORY_OIS_MODE_OFF:
		err = m7mu_writeb(client, M7MU_CATEGORY_NEW,
			0x10, 0x00);
		cam_trace("~ FACTORY_OIS_MODE_OFF ~\n");
		break;
	case FACTORY_OIS_LOG:
		if (state->factory_log_write) {
			err = m7mu_writeb(client, M7MU_CATEGORY_NEW,
				0x19, 0x01);
			CHECK_ERR(err);
		}
		msleep(40);
		m7mu_wait_make_CSV_rawdata(sd);
		err = m7mu_make_CSV_rawdata(sd, M7MU_FLASH_FACTORY_OIS, false);
		CHECK_ERR(err);
		cam_trace("~FACTORY_OIS_LOG ~\n");
		break;

	case FACTORY_OIS_ON:
		err = m7mu_writeb(client, M7MU_CATEGORY_NEW,
			0x11, 0x02);
		CHECK_ERR(err);
		break;

	case FACTORY_OIS_DECENTER_LOG:
		err = m7mu_make_CSV_rawdata(sd,
			M7MU_FLASH_FACTORY_DECENTER, false);
		CHECK_ERR(err);
		cam_trace("~FACTORY_OIS_DECENTER_LOG ~\n");
		break;

	default:
		cam_err("~ m7mu_set_factory_OIS ~ val : %d", val);
		break;
	}

	cam_trace("X\n");
	return 0;
}

static int m7mu_set_factory_OIS_shift(struct v4l2_subdev *sd, int val)
{
	int err;
	struct i2c_client *client = to_client(sd);

	cam_trace("E val : 0x%x\n", val);
	err = m7mu_writew(client, M7MU_CATEGORY_AE,
			0x15, val);
	CHECK_ERR(err);
	err = m7mu_writew(client, M7MU_CATEGORY_AE,
			0x14, 0);
	CHECK_ERR(err);
	cam_trace("X\n");
	return 0;
}

static int m7mu_set_factory_punt(struct v4l2_subdev *sd, int val)
{
	int err;
	struct m7mu_state *state = to_state(sd);
	struct i2c_client *client = to_client(sd);

	cam_trace("E val : %d\n", val);

	switch (val) {
	case FACTORY_PUNT_RANGE_START:
		cam_trace("~ FACTORY_PUNT_RANGE_START ~\n");
		err = m7mu_writeb(client, M7MU_CATEGORY_LENS,
			0x0E, 0x00);
		CHECK_ERR(err);
		break;

	case FACTORY_PUNT_RANGE_STOP:
		cam_trace("~ FACTORY_PUNT_RANGE_STOP ~\n");
		break;

	case FACTORY_PUNT_SHORT_SCAN_DATA:
		cam_trace("~ FACTORY_PUNT_SHORT_SCAN_DATA ~\n");
		err = m7mu_writeb(client, M7MU_CATEGORY_LENS,
			0x1B, 0x00);
		CHECK_ERR(err);
		break;

	case FACTORY_PUNT_SHORT_SCAN_START:
		cam_trace("~ FACTORY_PUNT_SHORT_SCAN_START ~\n");
		err = m7mu_writeb(client, M7MU_CATEGORY_LENS,
			0x0E, 0x01);
		CHECK_ERR(err);
		break;

	case FACTORY_PUNT_SHORT_SCAN_STOP:
		cam_trace("~ FACTORY_PUNT_SHORT_SCAN_STOP ~\n");
		break;

	case FACTORY_PUNT_LONG_SCAN_DATA:
		cam_trace("~ FACTORY_PUNT_LONG_SCAN_DATA ~\n");
		err = m7mu_writeb(client, M7MU_CATEGORY_LENS,
			0x1B, 0x00);
		CHECK_ERR(err);
		break;

	case FACTORY_PUNT_LONG_SCAN_START:
		cam_trace("~ FACTORY_PUNT_LONG_SCAN_START ~\n");
		err = m7mu_writeb(client, M7MU_CATEGORY_LENS,
			0x0E, 0x02);
		CHECK_ERR(err);
		break;

	case FACTORY_PUNT_LONG_SCAN_STOP:
		cam_trace("~FACTORY_PUNT_LONG_SCAN_STOP ~\n");
		break;

	case FACTORY_PUNT_LOG:
		if (state->factory_log_write) {
			err = m7mu_writeb(client, M7MU_CATEGORY_LENS,
				0x0E, 0x04);
			CHECK_ERR(err);
		}
		msleep(40);
		m7mu_wait_make_CSV_rawdata(sd);
		err = m7mu_make_CSV_rawdata(sd, M7MU_FLASH_FACTORY_PUNT, false);
		CHECK_ERR(err);
		cam_trace("~FACTORY_PUNT_LOG ~\n");
		break;

	case FACTORY_PUNT_SET_RANGE_DATA:
		cam_trace("~FACTORY_PUNT_SET_RANGE_DATA ~\n");
		err = m7mu_writew(client, M7MU_CATEGORY_LENS,
			0x17, (unsigned short)(state->f_punt_data.min));
		CHECK_ERR(err);

		err = m7mu_writew(client, M7MU_CATEGORY_LENS,
			0x19, (unsigned short)(state->f_punt_data.max));
		CHECK_ERR(err);

		err = m7mu_writeb(client, M7MU_CATEGORY_LENS,
			0x1B, (unsigned char)(state->f_punt_data.num));
		CHECK_ERR(err);

		cam_trace("~ FACTORY_PUNT_RANGE_START ~\n");
		err = m7mu_writeb(client, M7MU_CATEGORY_LENS,
			0x0E, 0x00);
		CHECK_ERR(err);
		break;

	case FACTORY_PUNT_EEP_WRITE:
		err = m7mu_writeb(client, M7MU_CATEGORY_LENS,
			0x0E, 0x05);
		CHECK_ERR(err);
		break;

	case FACTORY_PUNT_INFO_LOG:
		if (state->factory_log_write) {
			err = m7mu_writeb(client, M7MU_CATEGORY_LENS,
				0x0E, 0x06);
			CHECK_ERR(err);
		}
		msleep(40);
		m7mu_wait_make_CSV_rawdata(sd);
		err = m7mu_make_CSV_rawdata(sd, M7MU_FLASH_FACTORY_PUNT, false);
		CHECK_ERR(err);
		cam_trace("~FACTORY_PUNT_LOG ~\n");
		break;

	default:
		cam_err("~ m7mu_set_factory_punt ~ val : %d", val);
		break;
	}

	cam_trace("X\n");
	return 0;
}

static int m7mu_set_factory_zoom(struct v4l2_subdev *sd, int val)
{
	int err;
	int end_check = 0;
	struct m7mu_state *state = to_state(sd);
	struct i2c_client *client = to_client(sd);

	cam_trace("E val : %d\n", val);

	switch (val) {
	case FACTORY_ZOOM_MOVE_STEP:
		cam_trace("~ FACTORY_ZOOM_MOVE_STEP ~\n");
		err = m7mu_writeb(client, M7MU_CATEGORY_LENS,
			0x0F, 0x00);
		CHECK_ERR(err);
		break;

	case FACTORY_ZOOM_RANGE_CHECK_START:
		cam_trace("~ FACTORY_ZOOM_RANGE_CHECK_START ~\n");
		err = m7mu_writeb(client, M7MU_CATEGORY_LENS,
			0x0F, 0x05);
		CHECK_ERR(err);
		break;

	case FACTORY_ZOOM_RANGE_CHECK_STOP:
		cam_trace("~ FACTORY_ZOOM_RANGE_CHECK_STOP ~\n");
		break;

	case FACTORY_ZOOM_SLOPE_CHECK_START:
		cam_trace("~ FACTORY_ZOOM_SLOPE_CHECK_START ~\n");
		err = m7mu_writeb(client, M7MU_CATEGORY_LENS,
			0x0E, 0x03);
		CHECK_ERR(err);
		break;

	case FACTORY_ZOOM_SLOPE_CHECK_STOP:
		cam_trace("~ FACTORY_ZOOM_SLOPE_CHECK_STOP ~\n");
		break;

	case FACTORY_ZOOM_SET_RANGE_CHECK_DATA:
		cam_trace("~ FACTORY_ZOOM_SET_RANGE_CHECK_DATA ~\n");
		err = m7mu_writew(client, M7MU_CATEGORY_LENS,
			0x18, (unsigned short)(state->f_zoom_data.range_min));
		CHECK_ERR(err);

		err = m7mu_writew(client, M7MU_CATEGORY_LENS,
			0x1A, (unsigned short)(state->f_zoom_data.range_max));
		CHECK_ERR(err);
		break;

	case FACTORY_ZOOM_SET_SLOPE_CHECK_DATA:
		cam_trace("~ FACTORY_ZOOM_SET_SLOPE_CHECK_DATA ~\n");
		err = m7mu_writew(client, M7MU_CATEGORY_LENS,
			0x18, (unsigned short)(state->f_zoom_data.slope_min));
		CHECK_ERR(err);

		err = m7mu_writew(client, M7MU_CATEGORY_LENS,
			0x1A, (unsigned short)(state->f_zoom_data.slope_max));
		CHECK_ERR(err);
		break;

	case FACTORY_ZOOM_STEP_TELE:
		cam_trace("~ FACTORY_ZOOM_STEP_TELE ~\n");
		err = m7mu_writew(client, M7MU_CATEGORY_LENS,
			0x1A, 0x0F);
		CHECK_ERR(err);
		break;

	case FACTORY_ZOOM_STEP_WIDE:
		cam_trace("~ FACTORY_ZOOM_STEP_WIDE ~\n");
		err = m7mu_writew(client, M7MU_CATEGORY_LENS,
			0x1A, 0x00);
		CHECK_ERR(err);
		break;

	case FACTORY_ZOOM_MOVE_END_CHECK:
		err = m7mu_readb(client, M7MU_CATEGORY_LENS,
			0x26, &end_check);
		CHECK_ERR(err);
		state->factory_end_check = end_check;
		cam_trace("~ FACTORY_ZOOM_MOVE_CHECK ~\n");
		break;

	default:
		cam_err("~ m7mu_set_factory_zoom ~ val : %d", val);
		break;
	}

	cam_trace("X\n");
	return 0;
}

static int m7mu_set_factory_zoom_step(struct v4l2_subdev *sd, int val)
{
	int err;
	struct i2c_client *client = to_client(sd);

	cam_trace("E val : %d\n", val);
#if 1
	if (val >= 0 && val < 16) {
		err = m7mu_writeb(client, M7MU_CATEGORY_LENS,
			0x1A, val);
		CHECK_ERR(err);
		err = m7mu_writeb(client, M7MU_CATEGORY_LENS,
			0x1B, 0x00);
		CHECK_ERR(err);
	}
	cam_trace("~ FACTORY_ZOOM_MOVE_STEP ~\n");
	err = m7mu_writeb(client, M7MU_CATEGORY_LENS,
		0x0F, 0x00);
	CHECK_ERR(err);
#else
	err = m7mu_writeb(client, M7MU_CATEGORY_LENS,
		0x06, val);
	CHECK_ERR(err);
	msleep(500);
#endif
	return 0;
}

static int m7mu_set_factory_fail_stop(struct v4l2_subdev *sd, int val)
{
	int err;
	struct i2c_client *client = to_client(sd);

	cam_trace("E val : %d\n", val);

	switch (val) {
	case FACTORY_FAIL_STOP_ON:
		cam_trace("~ FACTORY_FAIL_STOP_ON ~\n");
		err = m7mu_writeb(client, M7MU_CATEGORY_LENS,
			0x1B, 0x01);
		CHECK_ERR(err);
		break;

	case FACTORY_FAIL_STOP_OFF:
		cam_trace("~ FACTORY_FAIL_STOP_OFF ~\n");
		break;

	case FACTORY_FAIL_STOP_RUN:
		err = m7mu_writeb(client, M7MU_CATEGORY_LENS,
			0x0D, 0x0C);
		CHECK_ERR(err);

		cam_trace("~ FACTORY_FAIL_STOP_RUN ~\n");
		break;

	case FACTORY_FAIL_STOP_STOP:
		cam_trace("~ FACTORY_FAIL_STOP_STOP ~\n");
		break;

	default:
		cam_err("~ m7mu_set_factory_fail_stop ~ val : %d", val);
		break;
	}

	cam_trace("X\n");
	return 0;
}

static int m7mu_set_factory_nodefocus(struct v4l2_subdev *sd, int val)
{
	int err;
	struct i2c_client *client = to_client(sd);

	cam_trace("E val : %d\n", val);

	switch (val) {
	case FACTORY_NODEFOCUSYES_ON:
		cam_trace("~ FACTORY_NODEFOCUSYES_ON ~\n");
		err = m7mu_writeb(client, M7MU_CATEGORY_LENS,
			0x1B, 0x01);
		CHECK_ERR(err);
		break;

	case FACTORY_NODEFOCUSYES_OFF:
		cam_trace("~ FACTORY_NODEFOCUSYES_OFF ~\n");
		break;

	case FACTORY_NODEFOCUSYES_RUN:
		err = m7mu_writeb(client, M7MU_CATEGORY_LENS,
			0x0D, 0x09);
		CHECK_ERR(err);

		cam_trace("~ FACTORY_NODEFOCUSYES_RUN ~\n");
		break;

	case FACTORY_NODEFOCUSYES_STOP:
		cam_trace("~ FACTORY_NODEFOCUSYES_STOP ~\n");
		break;

	default:
		cam_err("~ m7mu_set_factory_defocus ~ val : %d", val);
		break;
	}

	cam_trace("X\n");
	return 0;
}

static int m7mu_set_factory_interpolation(struct v4l2_subdev *sd, int val)
{
	int err;
	struct i2c_client *client = to_client(sd);

	cam_trace("E val : %d\n", val);

	switch (val) {
	case FACTORY_INTERPOLATION_USE:
		cam_trace("~ FACTORY_INTERPOLATION_USE ~\n");
		err = m7mu_writeb(client, M7MU_CATEGORY_LENS,
			0x0E, 0x0A);
		CHECK_ERR(err);
		break;

	case FACTORY_INTERPOLATION_RELEASE:
		cam_trace("~ FACTORY_INTERPOLATION_RELEASE ~\n");
		break;

	default:
		cam_err("~ m7mu_set_factory_interpolation ~ val : %d", val);
		break;
	}

	cam_trace("X\n");
	return 0;
}

static int m7mu_set_factory_common(struct v4l2_subdev *sd, int val)
{
	int err, down_check = 1, end_check = 0;
	struct m7mu_state *state = to_state(sd);
	struct i2c_client *client = to_client(sd);

	cam_trace("E val : %d\n", val);

	switch (val) {
	case FACTORY_FIRMWARE_DOWNLOAD:
		cam_trace("~ FACTORY_FIRMWARE_DOWNLOAD ~\n");
		err = m7mu_writeb(client, M7MU_CATEGORY_NEW,
			0x11, 0x00);
		CHECK_ERR(err);
		break;

	case FACTORY_DOWNLOAD_CHECK:
		cam_trace("~ FACTORY_DOWNLOAD_CHECK ~\n");
		err = m7mu_readb(client, M7MU_CATEGORY_NEW,
			0x11, &down_check);
		CHECK_ERR(err);
		state->factory_down_check = down_check;
		break;

	case FACTORY_END_CHECK:
		cam_trace("~ FACTORY_END_CHECK ~\n");
		err = m7mu_readb(client, M7MU_CATEGORY_LENS,
			0x40, &end_check);
		CHECK_ERR(err);
		state->factory_end_check = end_check;
		break;

	case FACTORY_COMMON_SET_FOCUS_ZONE_MACRO:
		cam_trace("~ FACTORY_COMMON_SET_FOCUS_ZONE_MACRO ~\n");
		err = m7mu_writeb(client, M7MU_CATEGORY_LENS,
			0x07, 0x02);
		CHECK_ERR(err);
		break;

	case FACTORY_FPS30_ON:
		cam_trace("~ FACTORY_FPS30_ON ~\n");
		err = m7mu_writeb(client, M7MU_CATEGORY_LENS,
			0x3F, 0x01);
		CHECK_ERR(err);
		break;

	case FACTORY_FPS30_OFF:
		cam_trace("~ FACTORY_FPS30_OFF ~\n");
		err = m7mu_writeb(client, M7MU_CATEGORY_LENS,
			0x3F, 0x00);
		CHECK_ERR(err);
		break;

	default:
		cam_err("~ m7mu_set_factory_common ~ val : %d", val);
		break;
	}

	cam_trace("X\n");
	return 0;
}

static int m7mu_set_factory_vib(struct v4l2_subdev *sd, int val)
{
	struct m7mu_state *state = to_state(sd);
	struct i2c_client *client = to_client(sd);
	int err;

	cam_trace("m7mu_set_factory_vib E val : %d\n", val);

	switch (val) {
	case FACTORY_VIB_START:
		cam_trace("~ FACTORY_VIB_START ~\n");
		err = m7mu_writeb(client, M7MU_CATEGORY_NEW,
			0x20, 0x02);
		CHECK_ERR(err);
		break;

	case FACTORY_VIB_STOP:
		cam_trace("~ FACTORY_VIB_STOP ~\n");
		break;

	case FACTORY_VIB_LOG:
		if (state->factory_log_write) {
			err = m7mu_writeb(client, M7MU_CATEGORY_NEW,
				0x19, 0x02);
			CHECK_ERR(err);
		}
		msleep(40);
		err = m7mu_make_CSV_rawdata(sd, M7MU_FLASH_FACTORY_VIB, false);
		CHECK_ERR(err);
		cam_trace("~FACTORY_VIB_LOG ~\n");
		break;

	default:
		cam_err("~m7mu_set_factory_vib~ val : %d", val);
		break;
	}

	cam_trace("X\n");
	return 0;
}

static int m7mu_set_factory_gyro(struct v4l2_subdev *sd, int val)
{
	struct m7mu_state *state = to_state(sd);
	struct i2c_client *client = to_client(sd);
	int err;
	int gyro_value;

	cam_trace("E val : %d\n", val);

	switch (val) {
	case FACTORY_GYRO_START:
		cam_trace("~ FACTORY_GYRO_START ~\n");
		err = m7mu_writeb(client, M7MU_CATEGORY_NEW,
			0x20, 0x03);
		CHECK_ERR(err);
		break;

	case FACTORY_GYRO_STOP:
		cam_trace("~ FACTORY_GYRO_STOP ~\n");
		break;

	case FACTORY_GYRO_LOG:
		if (state->factory_log_write) {
			err = m7mu_writeb(client, M7MU_CATEGORY_NEW,
			0x19, 0x03);
			CHECK_ERR(err);
		}
		msleep(40);
		m7mu_wait_make_CSV_rawdata(sd);
		err = m7mu_make_CSV_rawdata(sd, M7MU_FLASH_FACTORY_GYRO, false);
		CHECK_ERR(err);
		cam_trace("~FACTORY_PUNT_LOG ~\n");
		break;

	case FACTORY_GYRO_VALUE:
		cam_trace("~ FACTORY_GYRO_VALUE ~\n");
		err = m7mu_readl(client, M7MU_CATEGORY_NEW,
			0x21, &gyro_value);
		CHECK_ERR(err);
		state->factory_gyro_value = gyro_value;
		break;

	default:
		cam_err("~ m7mu_set_factory_gyro ~ val : %d", val);
		break;
	}

	cam_trace("X\n");
	return 0;
}

static int m7mu_set_factory_backlash(struct v4l2_subdev *sd, int val)
{
	struct m7mu_state *state = to_state(sd);
	struct i2c_client *client = to_client(sd);
	int err;

	cam_trace("E val : %d\n", val);

	switch (val) {
	case FACTORY_BACKLASH_INPUT:
		cam_trace("~ FACTORY_BACKLASH_INPUT ~\n");
		err = m7mu_writeb(client, M7MU_CATEGORY_LENS,
			0x0A, 0x01);
		CHECK_ERR(err);
		break;

	case FACTORY_BACKLASH_MAX_THR:
		err = m7mu_writeb(client, M7MU_CATEGORY_LENS,
			0x0A, 0x00);
		CHECK_ERR(err);
		cam_trace("~ FACTORY_BACKLASH_MAX_THR ~\n");
		break;

	case FACTORY_BACKLASH_WIDE_RUN:
		err = m7mu_writeb(client, M7MU_CATEGORY_LENS,
			0x0A, 0x03);
		CHECK_ERR(err);
		cam_trace("~ FACTORY_BACKLASH_WIDE_RUN ~\n");
		break;

	case FACTORY_BACKLASH_LOG:
		if (state->factory_log_write) {
			err = m7mu_writeb(client, M7MU_CATEGORY_LENS,
				0x0A, 0x05);
			CHECK_ERR(err);
		}
		msleep(40);
		err = m7mu_make_CSV_rawdata(sd,
			M7MU_FLASH_FACTORY_BACKLASH, false);
		CHECK_ERR(err);
		cam_trace("~FACTORY_BACKLASH_LOG ~\n");
		break;

	default:
		cam_err("~ m7mu_set_factory_backlash ~ val : %d", val);
		break;
	}

	cam_trace("X\n");
	return 0;
}

static int m7mu_set_factory_backlash_count(struct v4l2_subdev *sd, int val)
{
	int err;
	struct i2c_client *client = to_client(sd);

	cam_trace("E val : %d\n", val);

	err = m7mu_writeb(client, M7MU_CATEGORY_LENS,
		0x1B, val);
	CHECK_ERR(err);

	cam_trace("X\n");
	return 0;
}

static int m7mu_set_factory_af(struct v4l2_subdev *sd, int val)
{
	int err, end_check = 0;
	int result_check = 0;
	struct m7mu_state *state = to_state(sd);
	struct i2c_client *client = to_client(sd);

	cam_trace("E val : %d\n", val);

	switch (val) {
	case FACTORY_AF_LOCK_ON_SET:
		cam_trace("~ FACTORY_AF_LOCK_ON_SET ~\n");
		err = m7mu_writeb(client, M7MU_CATEGORY_LENS,
			0x1B, 0x01);
		CHECK_ERR(err);
		break;

	case FACTORY_AF_LOCK_OFF_SET:
		cam_trace("~ FACTORY_AF_LOCK_OFF_SET ~\n");
		err = m7mu_writeb(client, M7MU_CATEGORY_LENS,
			0x1B, 0x00);
		CHECK_ERR(err);
		break;

	case FACTORY_AF_MOVE:
		cam_trace("~ FACTORY_AF_MOVE ~\n");
		break;

	case FACTORY_AF_STEP_LOG:
		if ((state->factory_test_num ==
					FACTORY_RESOL_WIDE) ||
			(state->factory_test_num ==
			 FACTORY_RESOL_WIDE_INSIDE)) {
			cam_trace("~ FACTORY_AF_STEP_LOG WIDE ~\n");
			if (state->factory_log_write) {
				err = m7mu_writeb(client, M7MU_CATEGORY_LENS,
					0x0D, 0x1A);
				CHECK_ERR(err);
			}
			msleep(40);
			m7mu_wait_make_CSV_rawdata(sd);
			err = m7mu_make_CSV_rawdata(sd,
				M7MU_FLASH_FACTORY_WIDE_RESOL, false);
			CHECK_ERR(err);
		} else if ((state->factory_test_num ==
					FACTORY_RESOL_TELE) ||
			(state->factory_test_num ==
			 FACTORY_RESOL_TELE_INSIDE)) {
			cam_trace("~ FACTORY_AF_STEP_LOG TELE ~\n");
			if (state->factory_log_write) {
				err = m7mu_writeb(client, M7MU_CATEGORY_LENS,
					0x0D, 0x19);
				CHECK_ERR(err);
			}
			msleep(40);
			m7mu_wait_make_CSV_rawdata(sd);
			err = m7mu_make_CSV_rawdata(sd,
				M7MU_FLASH_FACTORY_TELE_RESOL, false);
			CHECK_ERR(err);
		} else {
			cam_trace("~ FACTORY NUMBER ERROR ~\n");
		}
		break;

	case FACTORY_AF_LOCK_START:
		cam_trace("~ FACTORY_AF_LOCK_START ~\n");
		err = m7mu_writeb(client, M7MU_CATEGORY_LENS,
			0x0D, 0x01);
		CHECK_ERR(err);
		break;

	case FACTORY_AF_LOCK_STOP:
		cam_trace("~ FACTORY_AF_LOCK_STOP ~\n");
		break;

	case FACTORY_AF_FOCUS_LOG:
		if (state->factory_log_write) {
			err = m7mu_writeb(client, M7MU_CATEGORY_LENS,
				0x0D, 0x0B);
			CHECK_ERR(err);
		}
		msleep(40);
		err = m7mu_make_CSV_rawdata(sd,
			M7MU_FLASH_FACTORY_AF_FCS, false);
		CHECK_ERR(err);

		cam_trace("~ FACTORY_AF_FOCUS_LOG ~\n");
		break;

	case FACTORY_AF_INT_SET:
		cam_trace("~ FACTORY_AF_INT_SET ~\n");
		err = m7mu_readw(client, M7MU_CATEGORY_LENS,
			0x23, &result_check);
		CHECK_ERR(err);
		state->factory_result_check = result_check;
		break;

	case FACTORY_AF_STEP_SAVE:
		err = m7mu_writeb(client, M7MU_CATEGORY_LENS,
			0x0D, 0x0A);
		CHECK_ERR(err);
		cam_trace("~ FACTORY_AF_SETP_SAVE ~\n");
		break;

	case FACTORY_AF_SCAN_LIMIT_START:
		err = m7mu_writeb(client, M7MU_CATEGORY_LENS,
			0x0D, 0x06);
		CHECK_ERR(err);
		cam_trace("~ FACTORY_AF_SCAN_LIMIT_START ~\n");
		break;

	case FACTORY_AF_SCAN_LIMIT_STOP:
		cam_trace("~ FACTORY_AF_SCAN_LIMIT_STOP ~\n");
		break;

	case FACTORY_AF_SCAN_RANGE_START:
		err = m7mu_writeb(client, M7MU_CATEGORY_LENS,
			0x0D, 0x07);
		CHECK_ERR(err);
		cam_trace("~ FACTORY_AF_SCAN_RANGE_START ~\n");
		break;

	case FACTORY_AF_SCAN_RANGE_STOP:
		cam_trace("~ FACTORY_AF_SCAN_RANGE_STOP ~\n");
		break;

	case FACTORY_AF_LED_END_CHECK:
		cam_trace("~ FACTORY_AF_LED_END_CHECK ~\n");
		err = m7mu_readb(client, M7MU_CATEGORY_LENS,
			0x40, &end_check);
		CHECK_ERR(err);
		state->factory_end_check = end_check;
		break;

	case FACTORY_AF_LED_LOG:
		if (state->factory_log_write) {
			err = m7mu_writeb(client, M7MU_CATEGORY_LENS,
				0x4D, 0x02);
			CHECK_ERR(err);
		}
		msleep(40);
		err = m7mu_make_CSV_rawdata(sd,
			M7MU_FLASH_FACTORY_AF_LED, false);
		CHECK_ERR(err);

		cam_trace("~ FACTORY_AF_LED_LOG ~\n");
		break;

	case FACTORY_AF_MOVE_END_CHECK:
		cam_trace("~ FACTORY_AF_MOVE_END_CHECK ~\n");
		err = m7mu_readb(client, M7MU_CATEGORY_LENS,
			0x29, &end_check);
		CHECK_ERR(err);
		state->factory_end_check = end_check;
		break;

	case FACTORY_AF_SCAN_END_CHECK:
		cam_trace("~ FACTORY_AF_SCAN_END_CHECK ~\n");
		err = m7mu_readw(client, M7MU_CATEGORY_LENS,
			0x20, &end_check);
		CHECK_ERR(err);
		state->factory_end_check = end_check;
		break;

	default:
		cam_err("~ m7mu_set_factory_af ~ val : %d", val);
		break;
	}

	cam_trace("X\n");
	return 0;
}

static int m7mu_set_factory_af_step(struct v4l2_subdev *sd, int val)
{
	int err;
	struct i2c_client *client = to_client(sd);

	cam_trace("E val : %d\n", val);

	err = m7mu_writew(client, M7MU_CATEGORY_LENS,
		0x1A, val);
	CHECK_ERR(err);

	cam_trace("X\n");
	return 0;
}

static int m7mu_set_factory_af_position(struct v4l2_subdev *sd, int val)
{
	int err;
	struct i2c_client *client = to_client(sd);

	cam_trace("E val : %d\n", val);

	err = m7mu_writeb(client, M7MU_CATEGORY_LENS,
			0x0B, val);
	CHECK_ERR(err);

	cam_trace("X\n");
	return 0;
}

static int m7mu_set_factory_defocus(struct v4l2_subdev *sd, int val)
{
	int err;
	struct i2c_client *client = to_client(sd);

	cam_trace("E val : %d\n", val);

	switch (val) {
	case FACTORY_DEFOCUS_RUN:
		cam_trace("~ FACTORY_DEFOCUS_RUN ~\n");
		err = m7mu_writeb(client, M7MU_CATEGORY_LENS,
				0x0D, 0x18);
		CHECK_ERR(err);
		break;

	case FACTORY_DEFOCUS_STOP:
		cam_trace("~ FACTORY_DEFOCUS_STOP ~\n");
		break;

	default:
		cam_err("~ m7mu_set_factory_defocus ~ val : %d", val);
		break;
	}

	cam_trace("X\n");
	return 0;
}

static int m7mu_set_factory_defocus_wide(struct v4l2_subdev *sd, int val)
{
	int err;
	struct i2c_client *client = to_client(sd);

	cam_trace("E val : %d\n", val);

	err = m7mu_writeb(client, M7MU_CATEGORY_LENS,
			0x1A, val);
	CHECK_ERR(err);

	cam_trace("X\n");
	return 0;
}

static int m7mu_set_factory_defocus_tele(struct v4l2_subdev *sd, int val)
{
	int err;
	struct i2c_client *client = to_client(sd);

	cam_trace("E val : %d\n", val);

	err = m7mu_writeb(client, M7MU_CATEGORY_LENS,
			0x1B, val);
	CHECK_ERR(err);

	cam_trace("X\n");
	return 0;
}

static int m7mu_set_factory_resol_cap(struct v4l2_subdev *sd, int val)
{
	int err;
	struct i2c_client *client = to_client(sd);

	cam_trace("E val : %d\n", val);

	switch (val) {
	case FACTORY_CAP_COMP_ON:
		err = m7mu_writeb(client, M7MU_CATEGORY_LENS,
			0x1B, 0x01);
		CHECK_ERR(err);
		cam_trace("~ FACTORY_CAP_COMP_ON ~\n");
		break;

	case FACTORY_CAP_COMP_OFF:
		err = m7mu_writeb(client, M7MU_CATEGORY_LENS,
			0x1B, 0x00);
		CHECK_ERR(err);
		cam_trace("~ FACTORY_CAP_COMP_OFF ~\n");
		break;

	case FACTORY_CAP_COMP_START:
		err = m7mu_writeb(client, M7MU_CATEGORY_LENS,
			0x0D, 0x04);
		CHECK_ERR(err);
		cam_trace("~ FACTORY_CAP_COMP_START ~\n");
		break;

	case FACTORY_CAP_COMP_STOP:
		cam_trace("~ FACTORY_CAP_COMP_STOP ~\n");
		break;

	case FACTORY_CAP_BARREL_ON:
		err = m7mu_writeb(client, M7MU_CATEGORY_LENS,
			0x1B, 0x01);
		CHECK_ERR(err);
		cam_trace("~ FACTORY_CAP_BARREL_ON ~\n");
		break;

	case FACTORY_CAP_BARREL_OFF:
		err = m7mu_writeb(client, M7MU_CATEGORY_LENS,
			0x1B, 0x00);
		CHECK_ERR(err);
		cam_trace("~ FACTORY_CAP_BARREL_OFF ~\n");
		break;

	case FACTORY_CAP_BARREL_START:
		err = m7mu_writeb(client, M7MU_CATEGORY_LENS,
			0x0D, 0x05);
		CHECK_ERR(err);
		cam_trace("~ FACTORY_CAP_BARREL_START ~\n");
		break;

	case FACTORY_CAP_BARREL_STOP:
		cam_trace("~ FACTORY_CAP_BARREL_STOP ~\n");
		break;

	default:
		cam_err("~ m7mu_set_factory_resol_cap ~ val : %d", val);
		break;
	}

	cam_trace("X\n");
	return 0;
}

static int m7mu_set_factory_af_zone(struct v4l2_subdev *sd, int val)
{
	int err;
	struct i2c_client *client = to_client(sd);

	cam_trace("E val : %d\n", val);

	switch (val) {
	case FACTORY_AFZONE_NORMAL:
		err = m7mu_writeb(client, M7MU_CATEGORY_LENS,
			0x07, 0x00);
		CHECK_ERR(err);
		cam_trace("~ FACTORY_AFZONE_NORMAL ~\n");
		break;

	case FACTORY_AFZONE_MACRO:
		err = m7mu_writeb(client, M7MU_CATEGORY_LENS,
			0x07, 0x01);
		CHECK_ERR(err);
		cam_trace("~ FACTORY_AFZONE_MACRO ~\n");
		break;

	case FACTORY_AFZONE_AUTO:
		err = m7mu_writeb(client, M7MU_CATEGORY_LENS,
			0x07, 0x02);
		CHECK_ERR(err);
		cam_trace("~ FACTORY_AFZONE_AUTO ~\n");
		break;

	default:
		cam_err("~ m7mu_set_factory_resol_cap ~ val : %d", val);
		break;
	}

	cam_trace("X\n");
	return 0;
}

static int m7mu_set_factory_af_lens(struct v4l2_subdev *sd, int val)
{
	int err;
	struct i2c_client *client = to_client(sd);
#if 0
	u32 int_factor;
#endif

	cam_trace("E val : %d\n", val);

	switch (val) {
	case FACTORY_AFLENS_OPEN:
		err = m7mu_writeb(client, M7MU_CATEGORY_LENS,
			0x00, 0x02);
		CHECK_ERR(err);

#if 0
		int_factor = m7mu_wait_interrupt(sd, M7MU_ISP_TIMEOUT);

		if (!(int_factor & M7MU_INT_LENS_INIT)) {
			cam_err("M7MU_INT_LENS_INIT isn't issued, %#x\n",
					int_factor);
			return -ETIMEDOUT;
		}
#endif

		cam_trace("~ FACTORY_AFLENS_OPEN ~\n");
		break;

	case FACTORY_AFLENS_CLOSE:
		err = m7mu_writeb(client, M7MU_CATEGORY_LENS,
			0x01, 0x00);
		CHECK_ERR(err);
		cam_trace("~ FACTORY_AFLENS_CLOSE ~\n");
		break;

	default:
		cam_err("~ m7mu_set_factory_af_lens ~ val : %d", val);
		break;
	}

	cam_trace("X\n");
	return 0;
}

static int m7mu_set_factory_adj_iris(struct v4l2_subdev *sd, int val)
{
	int err, end_check = 0;
	struct m7mu_state *state = to_state(sd);
	struct i2c_client *client = to_client(sd);

	cam_trace("E val : %d\n", val);

	switch (val) {
	case FACTORY_ADJ_IRIS_RUN:
		cam_trace("~ FACTORY_ADJ_IRIS_RUN ~\n");
		err = m7mu_writeb(client, M7MU_CATEGORY_ADJST,
			0x53, 0x01);
		CHECK_ERR(err);
		msleep(40);
		m7mu_wait_make_CSV_rawdata(sd);
		break;

	case FACTORY_ADJ_IRIS_STOP:
		cam_trace("~ FACTORY_ADJ_IRIS_STOP ~\n");
		break;

	case FACTORY_ADJ_IRIS_END_CHECK:
		err = m7mu_readb(client, M7MU_CATEGORY_LENS,
			0x40, &end_check);
		CHECK_ERR(err);
		state->factory_end_check = end_check;
		cam_trace("FACTORY_ADJ_IRIS_END_CHECK=%d\n",
			end_check);
		break;

	case FACTORY_ADJ_IRIS_LOG:
		if (state->factory_log_write) {
			err = m7mu_writeb(client, M7MU_CATEGORY_ADJST,
				M7MU_ADJST_LOG_REQUEST, 0x2);
			CHECK_ERR(err);
		}
		msleep(40);
		m7mu_wait_make_CSV_rawdata(sd);
		err = m7mu_make_CSV_rawdata(sd,
			M7MU_FLASH_FACTORY_IRIS, true);
		CHECK_ERR(err);
		break;

	case FACTORY_ADJ_IRIS_CHECK_RUN:
		if (state->factory_log_write) {
			err = m7mu_writeb(client, M7MU_CATEGORY_ADJST,
				M7MU_ADJST_LOG_REQUEST, 0x9);
			CHECK_ERR(err);
		}
		msleep(40);
		m7mu_wait_make_CSV_rawdata(sd);
		err = m7mu_make_CSV_rawdata(sd,
			M7MU_FLASH_FACTORY_IRIS, true);
		CHECK_ERR(err);
		break;

	default:
		cam_err("~ m7mu_set_factory_adj_iris ~ val : %d", val);
		break;
	}

	cam_trace("X\n");
	return 0;
}

static int m7mu_set_factory_sh_close(struct v4l2_subdev *sd, int val)
{
	int err, end_check = 0;
	struct m7mu_state *state = to_state(sd);
	struct i2c_client *client = to_client(sd);

	cam_trace("E val : %d\n", val);

	switch (val) {
	case FACTORY_SH_CLOSE_RUN:
		cam_trace("~ FACTORY_SH_CLOSE_RUN ~\n");
		err = m7mu_writeb(client, M7MU_CATEGORY_ADJST,
			0x53, 0x05);
		CHECK_ERR(err);
		msleep(40);
		m7mu_wait_make_CSV_rawdata(sd);
		break;

	case FACTORY_SH_CLOSE_STOP:
		cam_trace("~ FACTORY_SH_CLOSE_STOP ~\n");
		break;

	case FACTORY_SH_CLOSE_END_CHECK:
		cam_trace("~ FACTORY_SH_CLOSE_END_CHECK ~\n");

		err = m7mu_readb(client, M7MU_CATEGORY_LENS,
			0x40, &end_check);
		CHECK_ERR(err);
		state->factory_end_check = end_check;

		break;

	case FACTORY_SH_CLOSE_LOG:
		cam_trace("~ FACTORY_SH_CLOSE_LOG ~\n");
		if (state->factory_log_write) {
				err = m7mu_writeb(client, M7MU_CATEGORY_ADJST,
					M7MU_ADJST_LOG_REQUEST, 0x6);
				CHECK_ERR(err);
		}
		msleep(40);
		m7mu_wait_make_CSV_rawdata(sd);
		err = m7mu_make_CSV_rawdata(sd,
			M7MU_FLASH_FACTORY_SH_CLOSE, true);
		CHECK_ERR(err);

		break;

	default:
		cam_err("~ m7mu_set_factory_adj_iris ~ val : %d", val);
		break;
	}

	cam_trace("X\n");
	return 0;
}

static int m7mu_set_factory_adj_gain_liveview(struct v4l2_subdev *sd, int val)
{
	int err, end_check = 0;
	struct m7mu_state *state = to_state(sd);
	struct i2c_client *client = to_client(sd);

	cam_trace("E val : %d\n", val);

	switch (val) {
	case FACTORY_ADJ_GAIN_LIVEVIEW_RUN:
		cam_trace("~ FACTORY_ADJ_GAIN_LIVEVIEW_RUN ~\n");
		err = m7mu_writeb(client, M7MU_CATEGORY_ADJST,
			0x53, 0x03);
		CHECK_ERR(err);
		msleep(40);
		m7mu_wait_make_CSV_rawdata(sd);
		break;

	case FACTORY_ADJ_GAIN_LIVEVIEW_STOP:
		cam_trace("~ FACTORY_ADJ_GAIN_LIVEVIEW_STOP ~\n");
		break;

	case FACTORY_ADJ_GAIN_LIVEVIEW_END_CHECK:
		cam_trace("~ FACTORY_ADJ_GAIN_LIVEVIEW_END_CHECK ~\n");
		err = m7mu_readb(client, M7MU_CATEGORY_LENS,
			0x40, &end_check);
		CHECK_ERR(err);
		state->factory_end_check = end_check;
		break;

	case FACTORY_ADJ_GAIN_LIVEVIEW_LOG:
		cam_trace("~ FACTORY_ADJ_GAIN_LIVEVIEW_END_CHECK ~\n");
		if (state->factory_log_write) {
			err = m7mu_writeb(client, M7MU_CATEGORY_ADJST,
				M7MU_ADJST_LOG_REQUEST, 0x4);
			CHECK_ERR(err);
		}
		msleep(40);
		m7mu_wait_make_CSV_rawdata(sd);
		err = m7mu_make_CSV_rawdata(sd,
			M7MU_FLASH_FACTORY_LIVEVIEW, true);
		CHECK_ERR(err);
		break;

	default:
		cam_err("~ m7mu_set_factory_adj_iris ~ val : %d", val);
		break;
	}

	cam_trace("X\n");
	return 0;
}

static int m7mu_set_factory_flicker(struct v4l2_subdev *sd, int val)
{
	int err;
	struct i2c_client *client = to_client(sd);
	cam_trace("E val : %d\n", val);

	switch (val) {
	case FACTORY_FLICKER_AUTO:
		cam_trace("~ FACTORY_FLICKER_AUTO ~\n");
		err = m7mu_writeb(client, M7MU_CATEGORY_AE,
			M7MU_AE_FLICKER, 0x00);
		CHECK_ERR(err);
		break;

	case FACTORY_FLICKER_50HZ:
		cam_trace("~ FACTORY_FLICKER_50HZ ~\n");
		err = m7mu_writeb(client, M7MU_CATEGORY_AE,
			M7MU_AE_FLICKER, 0x01);
		CHECK_ERR(err);
		break;

	case FACTORY_FLICKER_60HZ:
		cam_trace("~ FACTORY_FLICKER_60HZ ~\n");
		err = m7mu_writeb(client, M7MU_CATEGORY_AE,
			M7MU_AE_FLICKER, 0x02);
		break;

	case FACTORY_FLICKER_50_60:
		cam_trace("~ FACTORY_FLICKER_50_60 ~\n");
		err = m7mu_writeb(client, M7MU_CATEGORY_AE,
			M7MU_AE_FLICKER, 0x03);
		break;

	case FACTORY_FLICKER_OFF:
		cam_trace("~ FACTORY_FLICKER_OFF ~\n");
		err = m7mu_writeb(client, M7MU_CATEGORY_AE,
			M7MU_AE_FLICKER, 0x04);
		CHECK_ERR(err);
		break;

	default:
		cam_err("~ m7mu_set_factory_adj_iris ~ val : %d", val);
		break;
	}

	cam_trace("X\n");
	return 0;
}

static int m7mu_set_factory_capture_gain(struct v4l2_subdev *sd, int val)
{
	int err, end_check = 0;
	struct m7mu_state *state = to_state(sd);
	struct i2c_client *client = to_client(sd);
	cam_trace("E val : %d\n", val);

	switch (val) {
	case FACTORY_CAPTURE_GAIN_RUN:
		cam_trace("~ FACTORY_CAPTURE_GAIN_RUN ~\n");
		err = m7mu_writeb(client, M7MU_CATEGORY_ADJST,
			0x53, 0x07);
		CHECK_ERR(err);
		msleep(40);
		m7mu_wait_make_CSV_rawdata(sd);
		break;

	case FACTORY_CAPTURE_GAIN_STOP:
		cam_trace("~ FACTORY_CAPTURE_GAIN_STOP ~\n");
		break;

	case FACTORY_CAPTURE_GAIN_END_CHECK:
		cam_trace("~ FACTORY_CAPTURE_GAIN_END_CHECK ~\n");
		err = m7mu_readb(client, M7MU_CATEGORY_LENS,
			0x40, &end_check);
		CHECK_ERR(err);
		state->factory_end_check = end_check;
		break;

	case FACTORY_CAPTURE_GAIN_LOG:
		if (state->factory_log_write) {
			err = m7mu_writeb(client, M7MU_CATEGORY_ADJST,
				M7MU_ADJST_LOG_REQUEST, 0x8);
			CHECK_ERR(err);
		}
		msleep(40);
		m7mu_wait_make_CSV_rawdata(sd);
		err = m7mu_make_CSV_rawdata(sd,
			M7MU_FLASH_FACTORY_GAIN_CAPTURE, true);
		CHECK_ERR(err);
		cam_trace("~FACTORY_CAPTURE_GAIN_LOG ~\n");
		break;

	default:
		cam_err("~ m7mu_set_factory_capture_gain ~ val : %d", val);
		break;
	}

	cam_trace("X\n");
	return 0;
}

static int m7mu_set_image_stabilizer_mode(struct v4l2_subdev *sd, int val)
{
	struct m7mu_state *state = to_state(sd);
	struct i2c_client *client = to_client(sd);
	int err;
	int cnt = 30;
	s32 ois_stability = 1;
	cam_trace("E: mode %d\n", val);

retry:
	switch (val) {
	case V4L2_IMAGE_STABILIZER_OFF:
		if (state->mode == MODE_PROGRAM || state->mode == MODE_M) {
			err = m7mu_writeb(client, M7MU_CATEGORY_AE,
				M7MU_AE_EP_MODE_CAP, 0x00);
			CHECK_ERR(err);
		}
		err = m7mu_writeb(client, M7MU_CATEGORY_NEW, 0x1A, 0x01);
		CHECK_ERR(err);
		err = m7mu_writeb(client, M7MU_CATEGORY_NEW, 0x11, 0x01);
		CHECK_ERR(err);

		err = m7mu_readb(client, M7MU_CATEGORY_NEW,
				0x1A, &ois_stability);
		CHECK_ERR(err);
		while (ois_stability && cnt) {
			msleep(20);
			err = m7mu_readb(client, M7MU_CATEGORY_NEW,
					0x1A, &ois_stability);
			CHECK_ERR(err);
			cnt--;
		}
		break;

	case V4L2_IMAGE_STABILIZER_OIS:
		if (state->mode == MODE_PROGRAM || state->mode == MODE_M) {
			err = m7mu_writeb(client, M7MU_CATEGORY_AE,
				M7MU_AE_EP_MODE_CAP, 0x18);
			CHECK_ERR(err);
		}
		err = m7mu_writeb(client, M7MU_CATEGORY_NEW, 0x1A, 0x01);
		CHECK_ERR(err);
		err = m7mu_writeb(client, M7MU_CATEGORY_NEW, 0x11, 0x02);
		CHECK_ERR(err);

		err = m7mu_readb(client, M7MU_CATEGORY_NEW,
				0x1A, &ois_stability);
		CHECK_ERR(err);
		while (ois_stability && cnt) {
			msleep(20);
			err = m7mu_readb(client, M7MU_CATEGORY_NEW,
					0x1A, &ois_stability);
			CHECK_ERR(err);
			cnt--;
		}
		break;

	case V4L2_IMAGE_STABILIZER_DUALIS:
		/*break;*/

	default:
		val = V4L2_IMAGE_STABILIZER_OFF;
		goto retry;
		break;
	}

	state->image_stabilizer_mode = val;

	cam_trace("X\n");
	return 0;
}

static int m7mu_set_factory_capture_ctrl(struct v4l2_subdev *sd, int val)
{
	int err = 0;
	int int_factor;
	struct i2c_client *client = to_client(sd);

	cam_trace("E val : %d\n", val);

	switch (val) {
	case FACTORY_STILL_CAP_NORMAL:
		cam_trace("~ FACTORY_STILL_CAP_NORMAL ~\n");
		err = m7mu_writeb(client, M7MU_CATEGORY_CAPCTRL,
			M7MU_CAPCTRL_CAP_MODE, 0x00);
		CHECK_ERR(err);
		break;

	case FACTORY_STILL_CAP_DUALCAP:
		cam_trace("~ FACTORY_STILL_CAP_DUALCAP ~\n");
		err = m7mu_writeb(client, M7MU_CATEGORY_CAPCTRL,
			M7MU_CAPCTRL_CAP_MODE, 0x05);
		CHECK_ERR(err);
		break;

	case FACTORY_DUAL_CAP_ON:
		cam_trace("~ FACTORY_DUAL_CAP_ON ~\n");
#if 0
		err = m7mu_readw(client, M7MU_CATEGORY_SYS,
				M7MU_SYS_INT_EN, &int_factor);
		CHECK_ERR(err);
		int_factor &= ~M7MU_INT_FRAME_SYNC;
		err = m7mu_writew(client, M7MU_CATEGORY_SYS,
				M7MU_SYS_INT_EN, int_factor);
		CHECK_ERR(err);
#endif

		err = m7mu_writeb(client, M7MU_CATEGORY_CAPCTRL,
			M7MU_CAPCTRL_START_DUALCAP, 0x01);
		CHECK_ERR(err);
		break;

	case FACTORY_DUAL_CAP_OFF:
		cam_trace("~ FACTORY_DUAL_CAP_OFF ~\n");
#if 1
		int_factor = m7mu_wait_interrupt(sd, M7MU_ISP_TIMEOUT);
		if (!(int_factor/* & M7MU_INT_SCENARIO_FIN*/)) {
			cam_warn(
				"M7MU_INT_SCENARIO_FIN isn't issued, %#x\n",
					int_factor);
			return -ETIMEDOUT;
		}
#endif
		CLEAR_ISP_INT1_STATE(sd);

		err = m7mu_writeb(client, M7MU_CATEGORY_CAPCTRL,
			M7MU_CAPCTRL_START_DUALCAP, 0x02);
		CHECK_ERR(err);
#if 1
		int_factor = m7mu_wait_interrupt(sd, M7MU_ISP_TIMEOUT);
		if (!(int_factor/* & M7MU_INT_SCENARIO_FIN*/)) {
			cam_warn(
				"M7MU_INT_SCENARIO_FIN isn't issued, %#x\n",
					int_factor);
			return -ETIMEDOUT;
		}
#endif

		break;

	default:
		cam_err("~ m7mu_set_factory_capture_ctrl ~ val : %d", val);
		break;
	}

	cam_trace("X\n");
	return 0;
}

static int m7mu_set_factory_flash(struct v4l2_subdev *sd, int val)
{
	int err, end_check = 0;
	struct m7mu_state *state = to_state(sd);
	struct i2c_client *client = to_client(sd);

	cam_trace("E val : %d\n", val);

	switch (val) {
	case FACTORY_FLASH_STROBE_CHECK_ON:
		cam_trace("~ FACTORY_FLASH_STROBE_CHECK_ON ~\n");
		err = m7mu_writeb(client, M7MU_CATEGORY_ADJST,
			0x70, 0x01);
		CHECK_ERR(err);
		break;

	case FACTORY_FLASH_STROBE_CHECK_OFF:
		cam_trace("~ FACTORY_FLASH_STROBE_CHECK_OFF ~\n");
		err = m7mu_writeb(client, M7MU_CATEGORY_ADJST,
			0x70, 0x00);
		CHECK_ERR(err);
		break;

	case FACTORY_FLASH_CHARGE:
		cam_trace("~ FACTORY_FLASH_CHARGE ~\n");
		err = m7mu_writeb(client, M7MU_CATEGORY_CAPPARM,
			0x2A, 0x01);
		CHECK_ERR(err);
		break;

	case FACTORY_FLASH_LOG:
		err = m7mu_make_CSV_rawdata(sd,
				M7MU_FLASH_FACTORY_FLASH_CHECK, false);
		cam_trace("~ FACTORY_FLASH_LOG ~\n");
		break;

	case FACTORY_FLASH_CHARGE_END_CHECK:
		cam_trace("~ FACTORY_FLASH_CHARGE_END_CHECK ~\n");
		err = m7mu_readb(client, M7MU_CATEGORY_CAPPARM,
			0x27, &end_check);
		CHECK_ERR(err);
		state->factory_end_check = end_check;
		break;

	case FACTORY_FLASH_STROBE_CHARGE_END_CHECK:
		cam_trace("~ FLASH_STROBE_CHARGE_END_CHECK ~\n");
		err = m7mu_readb(client, M7MU_CATEGORY_CAPPARM,
			0x27, &end_check);
		CHECK_ERR(err);
		state->factory_end_check = end_check;
		break;

	case FACTORY_ADJ_FLASH_WB_END_CHECK:
		cam_trace("~ ADJ_FLASH_WB_END_CHECK ~\n");
		err = m7mu_readb(client, M7MU_CATEGORY_ADJST,
			0x14, &end_check);
		CHECK_ERR(err);
		state->factory_end_check = end_check;
		break;

	case FACTORY_FLASH_WB_LOG:
		if (state->factory_log_write) {
			err = m7mu_writeb(client, M7MU_CATEGORY_NEW,
				0x31, 0x01);
		}
		msleep(40);
		err = m7mu_make_CSV_rawdata(sd,
				M7MU_FLASH_FACTORY_FLASH_WB, false);
		cam_trace("~ FACTORY_FLASH_WB_LOG ~\n");
		break;

	case FACTORY_ADJ_FLASH_WB_LOG:
		err = m7mu_make_CSV_rawdata(sd,
				M7MU_FLASH_FACTORY_ADJ_FLASH_WB, false);
		cam_trace("~ FACTORY_ADJ_FLASH_WB_LOG ~\n");
		break;

	default:
		break;
	}

	cam_trace("X\n");
	return 0;
}

static int m7mu_set_factory_wb(struct v4l2_subdev *sd, int val)
{
	int err, end_check;
	struct m7mu_state *state = to_state(sd);
	struct i2c_client *client = to_client(sd);

	cam_trace("E val : %d\n", val);

	switch (val) {
	case FACTORY_WB_INDOOR_RUN:
		cam_trace("~ FACTORY_WB_INDOOR_RUN ~\n");
		CLEAR_ISP_INT1_STATE(sd);
		err = m7mu_writeb(client, M7MU_CATEGORY_ADJST,
			0x14, 0x01);
		CHECK_ERR(err);
		m7mu_wait_interrupt(sd, M7MU_ISP_TIMEOUT);
	break;

	case FACTORY_WB_OUTDOOR_RUN:
		cam_trace("~ FACTORY_WB_OUTDOOR_RUN ~\n");
		err = m7mu_writeb(client, M7MU_CATEGORY_ADJST,
			0x14, 0x02);
		CHECK_ERR(err);
		break;

	case FACTORY_WB_INDOOR_END_CHECK:
	case FACTORY_WB_OUTDOOR_END_CHECK:
		cam_trace("~ FACTORY_WB_END_CHECK ~\n");
		err = m7mu_readb(client, M7MU_CATEGORY_ADJST,
			0x14, &end_check);
		CHECK_ERR(err);
		state->factory_end_check = end_check;
		break;

	case FACTORY_WB_LOG:
		if (state->factory_log_write) {
			err = m7mu_writeb(client, M7MU_CATEGORY_ADJST,
				0x14, 0x3);
			CHECK_ERR(err);
		}
		msleep(40);
		m7mu_wait_make_CSV_rawdata(sd);
		err = m7mu_make_CSV_rawdata(sd,
				M7MU_FLASH_FACTORY_WB_ADJ, false);
		CHECK_ERR(err);
		cam_trace("~ FACTORY_WB_LOG ~\n");
		break;

	default:
		break;
	}

	cam_trace("X\n");
	return 0;
}

static int m7mu_set_factory_defectpixel(struct v4l2_subdev *sd, int val)
{
	int err;
	int int_factor;
	int end_check = 0;
#if 0
	int i;
#endif
	struct m7mu_state *state = to_state(sd);
	struct i2c_client *client = to_client(sd);

	cam_trace("E val : %d\n", val);

	switch (val) {
	case FACTORY_DEFECTPIXEL_SCENARIO_6:
		cam_trace("~ FACTORY_DEFECTPIXEL_SCENARIO_6 ~\n");

		/*Interrupt Enable*/
		err = m7mu_readw(client, M7MU_CATEGORY_SYS,
			M7MU_SYS_INT_EN, &int_factor);
		CHECK_ERR(err);
		int_factor |= M7MU_INT_SCENARIO_FIN;
		err = m7mu_writew(client, M7MU_CATEGORY_SYS,
				M7MU_SYS_INT_EN, int_factor);
		CHECK_ERR(err);

		err = m7mu_writeb(client, M7MU_CATEGORY_LENS,
			0x40, 0x00);
		CHECK_ERR(err);

		err = m7mu_writeb(client, M7MU_CATEGORY_MON,
			0x5C, 0x06);
		CHECK_ERR(err);
		break;

	case FACTORY_DEFECTPIXEL_RUN:
		cam_trace("~ FACTORY_DEFECTPIXEL_RUN ~\n");
		err = m7mu_writeb(client, M7MU_CATEGORY_LENS,
			0x40, 0x00);
		CHECK_ERR(err);
		state->factory_end_interrupt = 0x0;
		/*Interrrupt Disable*/
#if 0
		err = m7mu_readw(client, M7MU_CATEGORY_SYS,
				M7MU_SYS_INT_EN, &int_factor);
		CHECK_ERR(err);
		int_factor &= ~M7MU_INT_STNW_DETECT;
		int_factor &= ~M7MU_INT_SCENARIO_FIN;
		err = m7mu_writew(client, M7MU_CATEGORY_SYS,
				M7MU_SYS_INT_EN, int_factor);
		CHECK_ERR(err);
#endif

		err = m7mu_writeb(client, M7MU_CATEGORY_MON,
			0x5C, 0x07);
		CHECK_ERR(err);
#if 0
		for (i = 0; i < 5; i++) {
			int_factor = m7mu_wait_interrupt(sd, M7MU_ISP_TIMEOUT);
			if (!(int_factor & M7MU_INT_STNW_DETECT)) {
				cam_warn(
					"M7MU_INT_STNW_DETECT isn't issued, %#x\n",
						int_factor);
				return -ETIMEDOUT;
			}
		}

		int_factor = m7mu_wait_interrupt(sd, M7MU_ISP_TIMEOUT);
		if (!(int_factor & M7MU_INT_SCENARIO_FIN)) {
			cam_warn(
				"M7MU_INT_SCENARIO_FIN isn't issued, %#x\n",
					int_factor);
			return -ETIMEDOUT;
		}
#endif
		break;

	case FACTORY_DEFECTPIXEL_END_CHECK:
		cam_trace("~ FACTORY_DEFECTPIXEL_END_CHECK ~\n");
		err = m7mu_readb(client, M7MU_CATEGORY_LENS,
			0x40, &end_check);
		CHECK_ERR(err);
		state->factory_end_check = end_check;
#if 0
		if (0) { /*end_check != 0) */
			msleep(100);
			int_factor = m7mu_wait_interrupt(sd, M7MU_ISP_TIMEOUT);
			if (!(int_factor & M7MU_INT_SCENARIO_FIN)) {
				cam_warn(
					"M7MU_INT_SCENARIO_FIN isn't issued, %#x\n",
						int_factor);
				return -ETIMEDOUT;
			}
		}
#endif
		break;

	case FACTORY_DEFECTPIXEL_CID_WRITE:
		cam_trace("~ FACTORY_DEFECTPIXEL_CID_WRITE ~\n");
		m7mu_writeb(client, M7MU_CATEGORY_SYS,
			0x29, 0x01);
		cam_trace("X\n");
		break;

	case FACTORY_DEFECTPIXEL_CID_1:
		cam_trace("~ FACTORY_DEFECTPIXEL_CID_1 ~\n");
		m7mu_readw(client, M7MU_CATEGORY_SYS,
			0x2A, &end_check);
		cam_err("CID_1 : %x\n", end_check);
		state->factory_end_check = end_check;
		cam_trace("X\n");
		break;

	case FACTORY_DEFECTPIXEL_CID_2:
		cam_trace("~ FACTORY_DEFECTPIXEL_CID_2 ~\n");
		m7mu_readw(client, M7MU_CATEGORY_SYS,
			0x2C, &end_check);
		cam_err("CID_2 : %x\n", end_check);
		state->factory_end_check = end_check;
		cam_trace("X\n");
		break;

	case FACTORY_DEFECTPIXEL_CID_3:
		cam_trace("~ FACTORY_DEFECTPIXEL_CID_3 ~\n");
		m7mu_readw(client, M7MU_CATEGORY_SYS,
			0x2E, &end_check);
		cam_err("CID_3 : %x\n", end_check);
		state->factory_end_check = end_check;
		cam_trace("X\n");
		break;

	case FACTORY_DEFECTPIXEL_LOG:
		cam_trace("~ FACTORY_DEFECTPIXEL_LOG ~\n");
		if (state->factory_log_write) {
			err = m7mu_writeb(client, M7MU_CATEGORY_MON,
					0x5C, 0x8);
		}
		msleep(40);
		m7mu_wait_make_CSV_rawdata(sd);
		err = m7mu_make_CSV_rawdata(sd,
				M7MU_FLASH_FACTORY_WB_ADJ, false);
#if 0
		msleep(300);
		int_factor = m7mu_wait_interrupt(sd, M7MU_ISP_TIMEOUT);
		if (!(int_factor & M7MU_INT_SCENARIO_FIN)) {
			cam_warn(
				"M7MU_INT_SCENARIO_FIN isn't issued, %#x\n",
					int_factor);
			return -ETIMEDOUT;
		}
#endif
		break;

	case FACTORY_DEFECTPIXEL_DOT_WRITE_CHECK:
	case FACTORY_DEFECTPIXEL_FLASH_MERGE:
		err = m7mu_readb(client, M7MU_CATEGORY_ADJST,
			0x90, &end_check);
		CHECK_ERR(err);
		cam_trace("DOT DATA END CHECK : %d\n", end_check);
		state->factory_end_check = end_check;
		break;

	case FACTORY_DEFECTPIXEL_RAW_DUMP:
		cam_trace("~ FACTORY_DEFECTPIXEL_RAW_DUMP ~\n");
		break;

	default:
		break;
	}

	cam_trace("X\n");
	return 0;
}

static int m7mu_set_factory_tilt(struct v4l2_subdev *sd, int val)
{
	int err, end_check = 0;
	struct m7mu_state *state = to_state(sd);
	struct i2c_client *client = to_client(sd);

	cam_trace("E val : %d\n", val);

	switch (val) {
	case FACTORY_TILT_ONE_SCRIPT_RUN:
		err = m7mu_writeb(client, M7MU_CATEGORY_LENS,
			0x0C, 0x06);
		CHECK_ERR(err);
		cam_trace("FACTORY_TILT_ONE_SCRIPT_RUN\n");
		break;

	case FACTORY_TILT_ONE_SCRIPT_DISP1:
		err = m7mu_readw(client, M7MU_CATEGORY_LENS,
			0x34, &end_check);
		CHECK_ERR(err);
		state->factory_end_check = end_check;
		cam_trace("FACTORY_TILT_ONE_SCRIPT_DISP1 %d\n", end_check);
		break;

	case FACTORY_TILT_ONE_SCRIPT_DISP2:
		err = m7mu_readw(client, M7MU_CATEGORY_LENS,
			0x36, &end_check);
		CHECK_ERR(err);
		state->factory_end_check = end_check;
		cam_trace("FACTORY_TILT_ONE_SCRIPT_DISP2 %d\n", end_check);
		break;

	case FACTORY_TILT_ONE_SCRIPT_DISP3:
		err = m7mu_readw(client, M7MU_CATEGORY_LENS,
			0x38, &end_check);
		CHECK_ERR(err);
		state->factory_end_check = end_check;
		cam_trace("FACTORY_TILT_ONE_SCRIPT_DISP3 %d\n", end_check);
		break;

	case FACTORY_TILT_ONE_SCRIPT_DISP4:
		err = m7mu_readw(client, M7MU_CATEGORY_LENS,
			0x3A, &end_check);
		CHECK_ERR(err);
		state->factory_end_check = end_check;
		cam_trace("FACTORY_TILT_ONE_SCRIPT_DISP4 %d\n", end_check);
		break;

	case FACTORY_TILT_ONE_SCRIPT_DISP5:
		err = m7mu_readw(client, M7MU_CATEGORY_LENS,
			0x3C, &end_check);
		CHECK_ERR(err);
		state->factory_end_check = end_check;
		cam_trace("FACTORY_TILT_ONE_SCRIPT_DISP5 %d\n", end_check);
		break;

	default:
		cam_err("~ m7mu_set_factory_common ~ val : %d", val);
		break;
	}

	cam_trace("X\n");
	return 0;
}

static int m7mu_set_factory_IR_Check(struct v4l2_subdev *sd, int val)
{
	int err = 0;
	struct m7mu_state *state = to_state(sd);
	struct i2c_client *client = to_client(sd);

	cam_trace("E val : %d\n", val);

	switch (val) {
	case FACTORY_IR_CHECK_LOG:
		cam_trace("~ FACTORY_IR_CHECK_LOG ~\n");
		msleep(40);
		m7mu_wait_make_CSV_rawdata(sd);
		err = m7mu_make_CSV_rawdata(sd,
			M7MU_FLASH_FACTORY_IR_CHECK, false);
		CHECK_ERR(err);
#ifndef READ_CSV_FILE_DIRECT
		err = m7mu_make_IR_cut_CSV_rawdata(sd,
			M7MU_FLASH_FACTORY_IR_CHECK, false);
		CHECK_ERR(err);
#endif
		break;

	case FACTORY_IR_CHECK_FLASH_LOG:
		cam_trace("~ FACTORY_IR_CHECK_FLASH_LOG ~\n");
		if (state->factory_log_write) {
			err = m7mu_writeb(client, M7MU_CATEGORY_ADJST,
				M7MU_ADJST_IR_CHECK, 0x02);
		}
		CHECK_ERR(err);
		m7mu_wait_make_CSV_rawdata(sd);
		err = m7mu_make_CSV_rawdata(sd,
			M7MU_FLASH_FACTORY_IR_CHECK, false);
		CHECK_ERR(err);
		break;

	default:
		cam_err("~ m7mu_set_factory_common ~ val : %d", val);
		break;
	}

	cam_trace("X\n");
	return 0;
}

static int m7mu_set_factory_IR_Check_GAvg(struct v4l2_subdev *sd, int val)
{
	int err, retval = 0;
	int min = 0, max = 0;
	struct m7mu_state *state = to_state(sd);
	struct i2c_client *client = to_client(sd);

	min = val >> 16;
	max =  val & 0xFFFF;
	err = m7mu_readb(client, M7MU_CATEGORY_LENS, 0x9e, &retval);
	CHECK_ERR(err);
	cam_trace("min = %d, max = %d, retval = %d\n", min, max, retval);

	state->factory_end_check = (min <= retval && retval <= max) ?
		0x02 : 0x01;
	cam_trace("X state->factory_end_check = %d\n",
			state->factory_end_check);
	return 0;
}

static int m7mu_shd_table_write(struct v4l2_subdev *sd, int idx)
{
	int err = 0;
	int image_size = 0;
	struct file *fp = NULL;
	mm_segment_t old_fs;
	
	struct i2c_client *client = to_client(sd);
	int nread = 0;
	int addr = 0;
	char *image_buf = NULL;
	int tx_size = 8*1024;

	u32 int_factor;
	char path[40] = {0, };
	
	old_fs = get_fs();
	set_fs(KERNEL_DS);

	if( idx > 0)
		sprintf( path ,"/storage/extSdCard/M2_shading_data%d%s", idx, ".log");
	else 
		sprintf( path , M7MU_SHD_DATA_PATH );
	
	fp = filp_open( path , O_RDONLY , 0);
	if(IS_ERR(fp)) {
		cam_trace("failed to open %s\n", path );
		return -1;
	}
	image_size = fp->f_path.dentry->d_inode->i_size;
	cam_trace("file path %s, image_size %d\n", path , image_size);

	err = m7mu_writew(client, M7MU_CATEGORY_SYS,
				M7MU_SYS_MEM_SIZE, image_size);
	CHECK_ERR(err);

	err = m7mu_writeb(client, M7MU_CATEGORY_ADJST,
				M7MU_ADJST_SHADING_DUMP, 1);
	CHECK_ERR(err);

	msleep(20);
	err = m7mu_readl(client, M7MU_CATEGORY_SYS,
					M7MU_SYS_MEM_ADDR, &addr);
	CHECK_ERR(err);
	
	cam_trace("addr = %#x\n", addr);

	//call data memory write function
	image_buf = vmalloc(image_size);

	if( image_buf == NULL) {
		cam_err("failed to alloc memory %d Bytes\n", image_size);
		err = -1;
		goto out;
	}

	nread = vfs_read(fp,
					image_buf,
					image_size, &fp->f_pos);

	if (nread != image_size) {
		cam_err("failed to read shading file, %d Bytes\n", nread);
		err = -1;
		goto out;
	}

	/* send image :NSS is High Control */
	if (!gpio_request(GPIO_ISP_SPI1_EN, "GPA2_5")) {
			gpio_direction_output(GPIO_ISP_SPI1_EN, 1);
			gpio_free(GPIO_ISP_SPI1_EN);
			cam_dbg("CS is setting to 1.\n");
	}

	/* send image :MISO is High Control */
	if (!gpio_request(GPIO_ISP_SPI1_MISO, "GPA2_6")) {
			gpio_direction_output(GPIO_ISP_SPI1_MISO, 1);
			gpio_free(GPIO_ISP_SPI1_MISO);
			cam_dbg("MISO is setting to 1.\n");
	}

	mdelay(30);

	tx_size = 8*1024;
	cam_dbg("trasnfer ready::image size = %d txSize:%d\n",
				image_size, tx_size);

	/* send image :spi transfer */
	err = m7mu_spi_write((unsigned char *)image_buf,
			image_size, tx_size);

	if (err < 0) {
		cam_err("m7mu_spi_write falied\n");
		err = -1;
		goto out;
	}

	/* send image :NSS is Low Control */
	if (!gpio_request(GPIO_ISP_SPI1_EN, "GPA2_5")) {
		gpio_direction_output(GPIO_ISP_SPI1_EN, 0);
		gpio_free(GPIO_ISP_SPI1_EN);
		cam_dbg("CS is setting to 0.\n");
	}

	/* send image :MISO is Low Control */
	if (!gpio_request(GPIO_ISP_SPI1_MISO, "GPA2_6")) {
		gpio_direction_output(GPIO_ISP_SPI1_MISO, 0);
		gpio_free(GPIO_ISP_SPI1_MISO);
		cam_dbg("MISO is setting to 0.\n");
	}

	err = m7mu_writeb(client, M7MU_CATEGORY_ADJST,
				M7MU_ADJST_SHADING_DUMP, 0x2);
	CHECK_ERR(err);

	mdelay(20);

	int_factor = m7mu_wait_interrupt(sd,M7MU_ISP_CAPTURE_TIMEOUT);

out:
	
	filp_close(fp, current->files);
	fp = NULL;
	set_fs(old_fs);
	vfree(image_buf);
	image_buf = NULL;

	return err;
}

static int m7mu_set_factory_shading(struct v4l2_subdev *sd, int val)
{
	int err = 0;
	struct i2c_client *client = to_client(sd);

	cam_trace("E val : %d\n", val);

	switch (val) {
	case V4L2_FACTORY_SHD_LOG:
		cam_trace("~V4L2_FACTORY_SHD_LOG ~\n");
		err = m7mu_writeb(client, M7MU_CATEGORY_ADJST,
			M7MU_ADJST_LOG_REQUEST2, 1);
		CHECK_ERR(err);
		msleep(40);
		m7mu_wait_make_CSV_rawdata(sd);
		err = m7mu_make_CSV_rawdata(sd,
			M7MU_FLASH_FACTORY_BEFORE_SHD_CHECK, false);
		CHECK_ERR(err);
		break;

	case V4L2_FACTORY_SHD_TABLE_DUMP_READ:
		cam_trace("~V4L2_FACTORY_SHD_TABLE_DUMP_READ ~\n");
		err = m7mu_writeb(client, M7MU_CATEGORY_ADJST,
				M7MU_ADJST_SHADING_DUMP, 0);
		CHECK_ERR(err);
		break;

	case V4L2_FACTORY_SHD_TABLE_DUMP_WRITE:
		cam_trace("~V4L2_FACTORY_SHD_TABLE_DUMP_WRITE ~\n");
		err = m7mu_shd_table_write(sd, 0);
		CHECK_ERR(err);
		break;
		
	}
	cam_trace("X\n");

	return 0;
}

static int m7mu_set_factory_adjust_log(struct v4l2_subdev *sd, int val)
{
	int err = 0;
	struct i2c_client *client = to_client(sd);
	int index = 0;

	cam_trace("E val : %d\n", val);

	switch (val) {
	case V4L2_FACTORY_ADJ_LOG_0:
	case V4L2_FACTORY_ADJ_LOG_1:
	case V4L2_FACTORY_ADJ_LOG_2:
	case V4L2_FACTORY_ADJ_LOG_3:
	case V4L2_FACTORY_ADJ_LOG_4:
	case V4L2_FACTORY_ADJ_LOG_5:
	case V4L2_FACTORY_ADJ_LOG_6:
	case V4L2_FACTORY_ADJ_LOG_7:
	case V4L2_FACTORY_ADJ_LOG_8:
	case V4L2_FACTORY_ADJ_LOG_9:
	case V4L2_FACTORY_ADJ_LOG_10:
	case V4L2_FACTORY_ADJ_LOG_11:
	case V4L2_FACTORY_ADJ_LOG_12:
	case V4L2_FACTORY_ADJ_LOG_13:
	case V4L2_FACTORY_ADJ_LOG_14:
	case V4L2_FACTORY_ADJ_LOG_15:
	case V4L2_FACTORY_ADJ_LOG_16:
		index = (int)val;
		cam_trace("~V4L2_FACTORY_ADJUST_TABLE_DUMP_READ %d~\n", index);
		err = m7mu_writeb(client, M7MU_CATEGORY_ADJST,
			M7MU_ADJST_ADJUST_TBL_DUMP, index);
		CHECK_ERR(err);
		break;

	case V4L2_FACTORY_ADJ_WRITE:
		cam_trace("~V4L2_FACTORY_ADJUST_TABLE_DUMP_WRITE~\n");
		break;
	}
	cam_trace("X\n");

	return 0;
}

static int m7mu_set_factory_backup_mem_data(struct v4l2_subdev *sd, int val)
{
	int err = 0;
	struct i2c_client *client = to_client(sd);

	cam_trace("E val : %d\n", val);

	switch (val) {
	case FACTORY_BACKUP_MAKE_SHADOW:
		err = m7mu_writeb(client, M7MU_CATEGORY_LENS,
			M7MU_LENS_DATA_BACKUP, 2);
			CHECK_ERR(err);
		break;
	case FACTORY_BACKUP_CHECK_SHADOW:
		err = m7mu_writeb(client, M7MU_CATEGORY_LENS,
			M7MU_LENS_DATA_BACKUP, 3);
		CHECK_ERR(err);
		break;
	case FACTORY_BACKUP_MAKE_ADJUST:
		err = m7mu_writeb(client, M7MU_CATEGORY_LENS,
				M7MU_LENS_DATA_BACKUP, 4);
		CHECK_ERR(err);
		break;
	}
	cam_trace("X\n");

	return 0;
}

static int m7mu_send_factory_command_value(struct v4l2_subdev *sd)
{
	int err;
	int category = 0, byte = 0, value = 0, size = 0;
	struct m7mu_state *state = to_state(sd);
	struct i2c_client *client = to_client(sd);

	category = state->factory_category;
	byte = state->factory_byte;
	value = state->factory_value;
	size = state->factory_value_size;

	cam_trace("category : 0x%x, byte : 0x%x, value : 0x%x\n",
		category, byte, value);

	if ((size == 4) || (value > 0xFFFF)) {
		cam_trace("write long");
		err = m7mu_writel(client, category, byte, value);
		CHECK_ERR(err);
		return err;
	}

	if ((size == 2) || (value > 0xFF)) {
		cam_trace("write word");
		err = m7mu_writew(client, category, byte, value);
		CHECK_ERR(err);
		return err;
	}

	cam_trace("write byte");
	err = m7mu_writeb(client, category, byte, value);
	CHECK_ERR(err);
	return err;
}

static int m7mu_set_factory_eepwrite_mark(struct v4l2_subdev *sd, int val)
{
	int err;
	int param1, param2;

	struct i2c_client *client = to_client(sd);

	param1 = (u8)val >> 4;
	param2 = (u8)val & 0x0F;
	cam_trace("E val : %d=0x%x / param1=%d/ param2=0x%x\n",
				val, val, param1, param2);

	switch (param1) {
	case FACTORY_EEPWRITE_CHECK:
		err = m7mu_writeb(client, M7MU_CATEGORY_LENS,
			0x75, param2);
		CHECK_ERR(err);
		cam_trace("FACTORY_EEPWRITE_CHECK\n");
		break;
	case FACTORY_EEPWRITE_REPAIR:
		err = m7mu_writeb(client, M7MU_CATEGORY_LENS,
			0x76, param2);
		CHECK_ERR(err);
		cam_trace("FACTORY_EEPWRITE_REPAIR\n");
		break;
	case FACTORY_EEPWRITE_STEP0:
		err = m7mu_writeb(client, M7MU_CATEGORY_LENS,
			0x70, param2);
		CHECK_ERR(err);
		cam_trace("FACTORY_EEPWRITE_STEP0\n");
		break;
	case FACTORY_EEPWRITE_STEP1:
		err = m7mu_writeb(client, M7MU_CATEGORY_LENS,
			0x71, param2);
		CHECK_ERR(err);
		cam_trace("FACTORY_EEPWRITE_STEP1\n");
		break;
	case FACTORY_EEPWRITE_STEP2:
		err = m7mu_writeb(client, M7MU_CATEGORY_LENS,
			0x72, param2);
		CHECK_ERR(err);
		cam_trace("FACTORY_EEPWRITE_STEP2\n");
		break;
	case FACTORY_EEPWRITE_STEP3:
		err = m7mu_writeb(client, M7MU_CATEGORY_LENS,
			0x73, param2);
		CHECK_ERR(err);
		cam_trace("FACTORY_EEPWRITE_STEP3\n");
		break;
	case FACTORY_EEPWRITE_STEP4:
		err = m7mu_writeb(client, M7MU_CATEGORY_LENS,
			0x74, param2);
		CHECK_ERR(err);
		cam_trace("FACTORY_EEPWRITE_STEP4\n");
		break;
	case FACTORY_EEPWRITE_STEP5:
		err = m7mu_writeb(client, M7MU_CATEGORY_LENS,
			0x75, param2);
		CHECK_ERR(err);
		cam_trace("FACTORY_EEPWRITE_STEP5\n");
		break;
	case FACTORY_EEPWRITE_STEP6:
		err = m7mu_writeb(client, M7MU_CATEGORY_LENS,
			0x76, param2);
		CHECK_ERR(err);
		cam_trace("FACTORY_EEPWRITE_STEP6\n");
		break;
	case FACTORY_EEPWRITE_STEP7:
		err = m7mu_writeb(client, M7MU_CATEGORY_LENS,
			0x77, param2);
		CHECK_ERR(err);
		cam_trace("FACTORY_EEPWRITE_STEP7\n");
		break;
	case FACTORY_EEPWRITE_STEP8:
		err = m7mu_writeb(client, M7MU_CATEGORY_LENS,
			0x78, param2);
		CHECK_ERR(err);
		cam_trace("FACTORY_EEPWRITE_STEP8\n");
		break;
	case FACTORY_EEPWRITE_STEP9:
		err = m7mu_writeb(client, M7MU_CATEGORY_LENS,
			0x79, param2);
		CHECK_ERR(err);
		cam_trace("FACTORY_EEPWRITE_STEP9\n");
		break;
	default:
		cam_err("~ m7mu_set_factory_eepwrite_mark ~ val : %d", val);
		break;
	}

	cam_trace("X\n");
	return 0;
}


static int m7mu_set_aeawblock(struct v4l2_subdev *sd, int val)
{
	int err;
	struct i2c_client *client = to_client(sd);

	cam_err("%d\n", val);
	switch (val) {
	case AE_UNLOCK_AWB_UNLOCK:
		err = m7mu_writeb(client, M7MU_CATEGORY_AE, M7MU_AE_LOCK, 0);
		CHECK_ERR(err);
		err = m7mu_writeb(client, M7MU_CATEGORY_WB, M7MU_AWB_LOCK, 0);
		CHECK_ERR(err);
		break;

	case AE_LOCK_AWB_UNLOCK:
		err = m7mu_writeb(client, M7MU_CATEGORY_AE, M7MU_AE_LOCK, 1);
		CHECK_ERR(err);
		err = m7mu_writeb(client, M7MU_CATEGORY_WB, M7MU_AWB_LOCK, 0);
		CHECK_ERR(err);
		break;

	case AE_UNLOCK_AWB_LOCK:
		err = m7mu_writeb(client, M7MU_CATEGORY_AE, M7MU_AE_LOCK, 0);
		CHECK_ERR(err);
		err = m7mu_writeb(client, M7MU_CATEGORY_WB, M7MU_AWB_LOCK, 1);
		CHECK_ERR(err);
		break;

	case AE_LOCK_AWB_LOCK:
		err = m7mu_writeb(client, M7MU_CATEGORY_AE, M7MU_AE_LOCK, 1);
		CHECK_ERR(err);
		err = m7mu_writeb(client, M7MU_CATEGORY_WB, M7MU_AWB_LOCK, 1);
		CHECK_ERR(err);
		break;
	}
	cam_err("X\n");
	return 0;
}

static int m7mu_set_aelock(struct v4l2_subdev *sd, int val)
{
	int err, AE_lock_status = 0;
	struct i2c_client *client = to_client(sd);

	cam_err("%d\n", val);

	err = m7mu_readb(client, M7MU_CATEGORY_AE,
		M7MU_AE_LOCK, &AE_lock_status);

	if (AE_lock_status != val) {
		err = m7mu_writeb(client, M7MU_CATEGORY_AE,
			M7MU_AE_LOCK, val);
		CHECK_ERR(err);
	}

	cam_err("X\n");
	return 0;
}


static int m7mu_set_GBAM(struct v4l2_subdev *sd, int val)
{
	struct m7mu_state *state = to_state(sd);
	struct i2c_client *client = to_client(sd);
	int err = 0;

	if (state->wb_g_value == 0 && state->wb_b_value == 0
		&& state->wb_a_value == 0 && state->wb_m_value == 0)
		val = 0;

	cam_trace("E, val = %d\n", val);

	if (val) {
		err = m7mu_writeb(client, M7MU_CATEGORY_WB,
				M7MU_WB_GBAM_MODE, 0x01);
		CHECK_ERR(err);

		err = m7mu_writeb(client, M7MU_CATEGORY_WB,
				M7MU_WB_G_VALUE, state->wb_g_value);
		CHECK_ERR(err);

		err = m7mu_writeb(client, M7MU_CATEGORY_WB,
				M7MU_WB_B_VALUE, state->wb_b_value);
		CHECK_ERR(err);

		err = m7mu_writeb(client, M7MU_CATEGORY_WB,
				M7MU_WB_A_VALUE, state->wb_a_value);
		CHECK_ERR(err);

		err = m7mu_writeb(client, M7MU_CATEGORY_WB,
				M7MU_WB_M_VALUE, state->wb_m_value);
		CHECK_ERR(err);
	} else {
		err = m7mu_writeb(client, M7MU_CATEGORY_WB,
				M7MU_WB_GBAM_MODE, 0x00);
		CHECK_ERR(err);
	}

	cam_trace("X\n");
	return 0;
}

static int m7mu_set_K(struct v4l2_subdev *sd, int val)
{
	int err = 0;
	struct i2c_client *client = to_client(sd);

	cam_trace("E %02X\n", val);

	err = m7mu_writeb(client, M7MU_CATEGORY_WB,
		M7MU_WB_K_VALUE, val);
	CHECK_ERR(err);

	cam_trace("X\n");
	return 0;
}

static int m7mu_set_dual_capture_mode(struct v4l2_subdev *sd, int val)
{
	int err = 0;
	int old_mode;
	struct m7mu_state *state = to_state(sd);
	struct i2c_client *client = to_client(sd);

	cam_trace("E, val = %d\n", val);

	if (val == state->vss_mode) {
		cam_err("same vss_mode: %d\n", state->vss_mode);
		return err;
	}

	old_mode = m7mu_set_mode(sd, M7MU_PARMSET_MODE);
	if (old_mode <= 0) {
		cam_err("failed to set mode\n");
		return old_mode;
	}

	switch (val) {
	case 0:
		/* Normal Video Snap Shot */
		err = m7mu_writeb(client, M7MU_CATEGORY_PARM,
				M7MU_PARM_VSS_MODE, 0x00);
		CHECK_ERR(err);
		break;

	case 1:
		/* 4M Video Snap Shot */
		err = m7mu_writeb(client, M7MU_CATEGORY_PARM,
				M7MU_PARM_VSS_MODE, 0x01);
		CHECK_ERR(err);
		break;
	default:
		val = 0;
		break;
	}

	state->vss_mode = val;

	cam_trace("X\n");
	return err;
}

static int m7mu_start_set_dual_capture(struct v4l2_subdev *sd, int frame_num)
{
	struct m7mu_state *state = to_state(sd);
	struct i2c_client *client = to_client(sd);
	int err, int_factor;

	cam_trace("E, vss mode: %d, frm[%d]\n", state->vss_mode, frame_num);

	if (!state->vss_mode)
		return 0;

	CLEAR_ISP_INT1_STATE(sd);

	/* Start video snap shot */
	err = m7mu_writeb(client, M7MU_CATEGORY_MON,
		M7MU_MON_START_VIDEO_SNAP_SHOT, 0x01);
	CHECK_ERR(err);

	/* Clear Interrupt factor */
	int_factor = m7mu_wait_interrupt(sd, M7MU_ISP_TIMEOUT);
	if (!(int_factor & M7MU_INT_FRAME_SYNC)) {
		cam_err("M7MU_INT_FRAME_SYNC isn't issued, %#x\n", int_factor);
		return -ETIMEDOUT;
	}

	state->dual_capture_start = 1;
	state->dual_capture_frame = frame_num;

	cam_trace("X\n");
	return err;
}

static int m7mu_set_smart_moving_recording(struct v4l2_subdev *sd, int val)
{
	int err = 0;
	struct m7mu_state *state = to_state(sd);
	struct i2c_client *client = to_client(sd);

	cam_dbg("E val=%d\n", val);

	/* add recording check for zoom move */
	if (val == 1) {  /* recording start */
		if (state->smart_scene_detect_mode == 1) {
			err = m7mu_writeb(client, M7MU_CATEGORY_NEW,
				0x0A, 0x02);
			CHECK_ERR(err);
		}

		state->recording = 1;
		err = m7mu_writeb(client, M7MU_CATEGORY_ADJST,
			M7MU_ADJST_SHUTTER_MODE, 0); /* Rolling Shutter */
		CHECK_ERR(err);

		err = m7mu_writeb(client, M7MU_CATEGORY_LENS, 0x27, 0x01);
		CHECK_ERR(err);
	} else if (val == 2) {  /* record end */
		if (state->smart_scene_detect_mode == 1) {
			err = m7mu_writeb(client, M7MU_CATEGORY_NEW,
				0x0A, 0x01);
			CHECK_ERR(err);

			m7mu_set_smart_auto_default_value(sd, 1);
		}

		state->recording = 0;
		err = m7mu_writeb(client, M7MU_CATEGORY_ADJST,
			M7MU_ADJST_SHUTTER_MODE, 1); /* Mechanical Shutter */
		CHECK_ERR(err);
		err = m7mu_writeb(client, M7MU_CATEGORY_LENS, 0x27, 0x00);
		CHECK_ERR(err);
	}
	m7mu_set_OIS_cap_mode(sd);

	return err;
}

static int m7mu_continue_proc(struct v4l2_subdev *sd, int val)
{
	int err = 1, int_factor;
	struct i2c_client *client = to_client(sd);

	cam_trace("E\n");

	switch (val) {
	case V4L2_INT_STATE_FRAME_SYNC:
#if 0
		int_factor = m7mu_wait_interrupt(sd, M7MU_SOUND_TIMEOUT);
		if (!(int_factor & M7MU_INT_SOUND)) {
			cam_dbg("m7mu_continue_proc() INT_FRAME_SOUND error%#x\n",
					int_factor);
			return -ETIMEDOUT;
		}
#endif
		break;

	case V4L2_INT_STATE_CAPTURE_SYNC:
		/* continue : cancel CAPTURE or postview end CAPTURE*/
		int_factor = m7mu_wait_interrupt(sd, M7MU_ISP_CAPTURE_TIMEOUT);
		if (!(int_factor & M7MU_INT_CAPTURE)) {
			cam_dbg("m7mu_continue_proc() INT_STATE_CAPTURE_SYNC error%#x\n",
					int_factor);
			return -ETIMEDOUT;
		}
		break;

	case V4L2_INT_STATE_CONTINUE_CANCEL:
		/* continue cancel */
		err = m7mu_writeb(client, M7MU_CATEGORY_CAPCTRL,
				M7MU_CAPCTRL_START_DUALCAP, 0x02);
			CHECK_ERR(err);
		cam_dbg("-------------V4L2_INT_STATE_CONTINUE_CANCEL-------------\n");
		/* CAPTURE wait interrupt -> V4L2_INT_STATE_CAPTURE_SYNC */
		break;

	case V4L2_INT_STATE_CONTINUE_END:
		CLEAR_ISP_INT1_STATE(sd);

		err = m7mu_set_mode(sd, M7MU_MONITOR_MODE);
		if (err <= 0) {
			cam_err("failed to set mode\n");
			return err;
		}

		err = m7mu_wait_interrupt(sd, M7MU_ISP_SHORT_TIMEOUT);
		if (!(err & M7MU_INT_MODE)) {
			cam_err("m7mu_continue_proc() INT_STATE_CONTINUE_END error\n");
			return -ETIMEDOUT;
		}
		break;

	case V4L2_INT_STATE_START_CAPTURE:
		err = m7mu_set_mode(sd, M7MU_STILLCAP_MODE);
		if (err <= 0) {
			cam_err("failed to set mode\n");
			return err;
		}
		break;
	}

	cam_dbg("m7mu_continue_proc : 0x%x  err=%d\n",
		val, err);

	cam_trace("X\n");
	return err;
}

static int m7mu_burst_set_postview_size(struct v4l2_subdev *sd, int val)
{
	int err = 1, i, num_entries;
	struct m7mu_state *state = to_state(sd);
	struct i2c_client *client = to_client(sd);
	const struct m7mu_frmsizeenum **frmsize;

	int width = val >> 16;
	int height = val & 0xFFFF;

	cam_trace("E\n");
	cam_trace("size = (%d x %d)\n", width, height);

	frmsize = &state->postview;

	num_entries = ARRAY_SIZE(postview_frmsizes);
	*frmsize = &postview_frmsizes[num_entries-1];

	for (i = 0; i < num_entries; i++) {
		if (width == postview_frmsizes[i].target_width &&
			height == postview_frmsizes[i].target_height) {
			*frmsize = &postview_frmsizes[i];
			break;
		}
	}

	err = m7mu_writeb(client, M7MU_CATEGORY_CAPPARM,
			M7MU_CAPPARM_PREVIEW_IMG_SIZE,
			state->postview->reg_val);

	cam_trace("X\n");

	return err;
}

static int m7mu_burst_set_snapshot_size(struct v4l2_subdev *sd, int val)
{
	int err = 1, i, num_entries;
	struct m7mu_state *state = to_state(sd);
	const struct m7mu_frmsizeenum **frmsize;
	struct i2c_client *client = to_client(sd);

	int width = val >> 16;
	int height = val & 0xFFFF;

	cam_trace("E\n");
	cam_trace("size = (%d x %d)\n", width, height);

	frmsize = &state->capture;

	num_entries = ARRAY_SIZE(capture_frmsizes);
	*frmsize = &capture_frmsizes[num_entries-1];

	for (i = 0; i < num_entries; i++) {
		if (width == capture_frmsizes[i].target_width &&
			height == capture_frmsizes[i].target_height) {
			*frmsize = &capture_frmsizes[i];
			break;
		}
	}

	err = m7mu_writeb(client, M7MU_CATEGORY_CAPPARM,
			M7MU_CAPPARM_MAIN_IMG_SIZE,
			state->capture->reg_val);

	cam_trace("X\n");

	return err;
}

static int m7mu_set_capture_cnt(struct v4l2_subdev *sd, int val)
{
	int err;
	struct i2c_client *client = to_client(sd);

	cam_trace("%s: E, val = %d", __func__, val);

	err = m7mu_writeb(client, M7MU_CATEGORY_CAPCTRL,
			M7MU_CAPCTRL_CAP_FRM_COUNT, val);
	return err;
}

static int m7mu_burst_proc(struct v4l2_subdev *sd, int val)
{
	int err = 1;
	int int_factor;
	struct m7mu_state *state = to_state(sd);
	struct i2c_client *client = to_client(sd);
	int read_val;
	int i;

	cam_trace("E\n");

	switch (val) {
	case V4L2_INT_STATE_BURST_START:
		cam_trace("Burstshot  Capture  START ~~~~~~\n");

		state->mburst_start = true;

		if (state->mode == MODE_MAGIC) {
			err = m7mu_writeb(client, M7MU_CATEGORY_CAPCTRL,
					M7MU_CAPCTRL_CAP_MODE, 0x0E);
			CHECK_ERR(err);
		} else {
			err = m7mu_writeb(client, M7MU_CATEGORY_CAPCTRL,
					M7MU_CAPCTRL_CAP_MODE, 0x0D);
			CHECK_ERR(err);
		}

		err = m7mu_writeb(client, M7MU_CATEGORY_CAPPARM,
				M7MU_CAPPARM_YUVOUT_MAIN, 0x01);
		CHECK_ERR(err);

		err = m7mu_writeb(client, M7MU_CATEGORY_CAPCTRL,
			M7MU_CAPCTRL_START_DUALCAP,
			M7MU_CAP_MODE_MULTI_CAPTURE);
		CHECK_ERR(err);

		for (i = M7MU_I2C_VERIFY; i; i--) {
			err = m7mu_readb(client, M7MU_CATEGORY_SYS,
					M7MU_SYS_MODE, &read_val);
			if (read_val == M7MU_STILLCAP_MODE)
				break;
			msleep(20);
		}
		break;

	case V4L2_INT_STATE_BURST_SYNC:
		cam_trace("Burstshot  Page SYNC~~~\n");
		int_factor = m7mu_wait_interrupt(sd, M7MU_ISP_SHORT_TIMEOUT);
		if (!(int_factor & M7MU_INT_SOUND)) {
			cam_dbg("m7mu_continue_proc() INT_FRAME_SYNC error%#x\n",
					int_factor);
			return -ETIMEDOUT;
		}
		break;

	case V4L2_INT_STATE_BURST_SOUND:
		cam_trace("Burstshot  Page SOUND~~~\n");
		int_factor = m7mu_wait_interrupt(sd, M7MU_ISP_SHORT_TIMEOUT);
		if (!(int_factor & M7MU_INT_SOUND)) {
			cam_dbg("m7mu_continue_proc() INT_FRAME_SOUND error%#x\n",
					int_factor);
			return -ETIMEDOUT;
		}
		break;

	case V4L2_INT_STATE_BURST_STOP_REQ:
		/* continue cancel */
		err = m7mu_writeb(client, M7MU_CATEGORY_CAPCTRL,
			M7MU_CAPCTRL_START_DUALCAP, 0x03);
		CHECK_ERR(err);

		cam_trace("Burstshot  Capture  Shot Stop ~~~~~~\n");
		break;


	case V4L2_INT_STATE_BURST_STOP:
		state->mburst_start = false;

		CLEAR_ISP_INT1_STATE(sd);

		/* continue cancel */
		err = m7mu_writeb(client, M7MU_CATEGORY_CAPCTRL,
			M7MU_CAPCTRL_START_DUALCAP, 0x02);
		CHECK_ERR(err);

		/* CAPTURE wait interrupt -> V4L2_INT_STATE_CAPTURE_SYNC */
		err = m7mu_wait_interrupt(sd, M7MU_ISP_SHORT_TIMEOUT);
		if (!(err & M7MU_INT_MODE)) {
			cam_err("m7mu_burst_proc() INT_STATE_CONTINUE_END error\n");
			return -ETIMEDOUT;
		}

		cam_trace("Burstshot  Capture  STOP ~~~~~~\n");
		break;
	}

	cam_trace("X\n");
	return err;
}

static int m7mu_set_iqgrp(struct v4l2_subdev *sd, int val)
{
	struct m7mu_state *state = to_state(sd);
	struct i2c_client *client = to_client(sd);
	int err, current_val, current_mode;
	u32 iqgrp_val = 0x01;

	cam_trace("E\n");

	err = m7mu_readb(client, M7MU_CATEGORY_SYS,
			M7MU_SYS_MODE, &current_mode);

	if (current_mode != M7MU_PARMSET_MODE) {
		cam_trace("~ return !!! %d\n", current_mode);
		return 0;
	}

	if (state->fps == 60) {
		if (state->preview_height == 480)
			iqgrp_val = 0x68;
		else if (state->preview_height == 720)
			iqgrp_val = 0x65;
	} else if (state->sensor_mode == SENSOR_MOVIE
		&& (state->fps == 30 || state->fps == 0)) {
		if (state->preview_height == 1080)
			iqgrp_val = 0x64;
		else if (state->preview_height == 720)
			iqgrp_val = 0x66;
		else if (state->preview_height == 480)
			iqgrp_val = 0x69;
		else if (state->preview_height == 240)
			iqgrp_val = 0x69;
	} else {
		if (state->preview_width == 768
			|| state->mode == MODE_GOLF_SHOT)
			iqgrp_val = 0x67;
		else
			iqgrp_val = 0x01;
	}

	if (val == 1080)
		iqgrp_val = 0x64;

	err = m7mu_readb(client, M7MU_CATEGORY_MON,
		0x59, &current_val);
	CHECK_ERR(err);

	if (current_val != iqgrp_val) {
		err = m7mu_writeb(client, M7MU_CATEGORY_MON,
			0x59, iqgrp_val);
		CHECK_ERR(err);
	}

	cam_trace("X\n");
	return 0;
}

static int m7mu_set_gamma(struct v4l2_subdev *sd)
{
	struct m7mu_state *state = to_state(sd);
	struct i2c_client *client = to_client(sd);
	int err = 0;
	int cap_gamma, gamma_rgb_cap;
	int current_mode;

	cam_trace("E, mode %d\n", state->mode);

	err = m7mu_readb(client, M7MU_CATEGORY_SYS,
			M7MU_SYS_MODE, &current_mode);

	if (current_mode != M7MU_PARMSET_MODE) {
		cam_trace("~ return !!! %d\n", current_mode);
		return 0;
	}

	/* Set Gamma value */
	err = m7mu_readb(client, M7MU_CATEGORY_CAPPARM, 0x41, &cap_gamma);
	CHECK_ERR(err);
	err = m7mu_readb(client, M7MU_CATEGORY_CAPPARM, 0x42, &gamma_rgb_cap);
	CHECK_ERR(err);

	if (gamma_rgb_cap < 0xD) {
		state->gamma_rgb_cap = cap_gamma;
		state->gamma_tbl_rgb_cap = gamma_rgb_cap;
	}

	if (state->mode == MODE_SILHOUETTE) {
		if (cap_gamma != 0x00) {
			err = m7mu_writeb(client, M7MU_CATEGORY_CAPPARM,
				0x41, 0x00);
			CHECK_ERR(err);
		}
#ifdef CONFIG_MACH_GC2PD
		if (gamma_rgb_cap != 0x0F + state->widget_mode_level) {
			err = m7mu_writeb(client, M7MU_CATEGORY_CAPPARM,
				0x42, 0x0F);
			CHECK_ERR(err);
		}
#else
		if (gamma_rgb_cap != 0x0D + state->widget_mode_level) {
			err = m7mu_writeb(client, M7MU_CATEGORY_CAPPARM,
				0x42, 0x0D + state->widget_mode_level);
			CHECK_ERR(err);
		}
#endif
	} else if (state->mode == MODE_DAWN) {
#ifdef CONFIG_MACH_GC2PD
		if (cap_gamma != 0x00) {
			err = m7mu_writeb(client, M7MU_CATEGORY_CAPPARM,
				0x41, 0x00);
			CHECK_ERR(err);
		}
		if (gamma_rgb_cap != 0x0D + state->widget_mode_level) {
			err = m7mu_writeb(client, M7MU_CATEGORY_CAPPARM,
				0x42, 0x19);
			CHECK_ERR(err);
		}
#endif
	} else {
		if (cap_gamma != state->gamma_rgb_cap) {
			err = m7mu_writeb(client, M7MU_CATEGORY_CAPPARM,
				0x41, state->gamma_rgb_cap);
			CHECK_ERR(err);
		}
		if (gamma_rgb_cap != state->gamma_tbl_rgb_cap) {
			err = m7mu_writeb(client, M7MU_CATEGORY_CAPPARM,
				0x42, state->gamma_tbl_rgb_cap);
			CHECK_ERR(err);
		}
	}

	cam_trace("X\n");
	return 0;
}

static int m7mu_set_PASM_mode(struct v4l2_subdev *sd, int val)
{
	struct m7mu_state *state = to_state(sd);
	struct i2c_client *client = to_client(sd);
	int err = 0;
	int color_effect, current_mode;
	/*int denominator = 500, numerator = 8;*/
	/*u32 f_number = 0x45;*/

	cam_dbg("E, value %d\n", val);

	state->mode = val;

	err = m7mu_readb(client, M7MU_CATEGORY_SYS,
			M7MU_SYS_MODE, &current_mode);
	CHECK_ERR(err);

	switch (val) {
	case MODE_SMART_AUTO:
	case MODE_SMART_SUGGEST:
		err = m7mu_writeb(client, M7MU_CATEGORY_AE, 0x53, 0x01);
		CHECK_ERR(err);

		err = m7mu_writeb(client, M7MU_CATEGORY_MON,
				M7MU_MON_COLOR_EFFECT, state->color_effect);
		CHECK_ERR(err);

		/* Set LIKE_PRO_EN Disable */
		err = m7mu_writeb(client,
				M7MU_CATEGORY_PRO_MODE, 0x00, 0x00);
		CHECK_ERR(err);

		/* LIKE PRO DISPLAY SET */
		err = m7mu_writeb(client, M7MU_CATEGORY_PRO_MODE, 0x07, 0x00);
		CHECK_ERR(err);

		/* Set Still Mode */
		err = m7mu_writeb(client, M7MU_CATEGORY_NEW, 0x08, 0x01);
		CHECK_ERR(err);

		/* Set HISTOGRAM ON */
		err = m7mu_writeb(client, M7MU_CATEGORY_MON, 0x58, 0x01);
		CHECK_ERR(err);

		/* Set Monitor EV program mode */
/*		err = m7mu_writeb(client, M7MU_CATEGORY_AE,
			M7MU_AE_EP_MODE_MON, 0x05);
		CHECK_ERR(err);
*/
		/* Set Still Capture EV program mode */
/*		err = m7mu_writeb(client, M7MU_CATEGORY_AE,
			M7MU_AE_EP_MODE_CAP, 0x05);
		CHECK_ERR(err);
*/
		/* SMART AUTO CAP */
		err = m7mu_writeb(client, M7MU_CATEGORY_AE,
			M7MU_AE_EV_PRG_MODE_CAP, 0x10);
		CHECK_ERR(err);

		/* Set AF range to AUTO-MACRO */
		err = m7mu_writeb(client, M7MU_CATEGORY_LENS,
			M7MU_LENS_AF_SCAN_RANGE, 0x02);
			CHECK_ERR(err);
		break;

	case MODE_PANORAMA:
		err = m7mu_writeb(client, M7MU_CATEGORY_AE, 0x53, 0x01);
		CHECK_ERR(err);

		err = m7mu_writeb(client, M7MU_CATEGORY_MON,
			M7MU_MON_COLOR_EFFECT, state->color_effect);
		CHECK_ERR(err);

		/* Set LIKE_PRO_EN Disable */
        err = m7mu_writeb(client, M7MU_CATEGORY_PRO_MODE, 0x00, 0x01);
		CHECK_ERR(err);

        /* Set LIKE_PRO_MODE Panorama */
        err = m7mu_writeb(client, M7MU_CATEGORY_PRO_MODE, 0x01, 0x15);
        CHECK_ERR(err);

		/* LIKE PRO DISPLAY SET */
		err = m7mu_writeb(client, M7MU_CATEGORY_PRO_MODE, 0x07, 0x00);
		CHECK_ERR(err);

		/* Set CATE_408 to None */
		err = m7mu_writeb(client, M7MU_CATEGORY_NEW, 0x08, 0x00);
		CHECK_ERR(err);

		/* Set HISTOGRAM OFF */
		err = m7mu_writeb(client, M7MU_CATEGORY_MON, 0x58, 0x00);
		CHECK_ERR(err);

		/* Set Monitor EV program mode */
/*		err = m7mu_writeb(client, M7MU_CATEGORY_AE,
			M7MU_AE_EP_MODE_MON, 0x10);
		CHECK_ERR(err);
*/
		/* Set Still Capture EV program mode */
/*		err = m7mu_writeb(client, M7MU_CATEGORY_AE,
			M7MU_AE_EP_MODE_CAP, 0x10);
		CHECK_ERR(err);
*/
		/* Still Capture EVP Set Parameter Mode */
		if (state->iso == 0) {
			err = m7mu_writeb(client, M7MU_CATEGORY_AE,
				M7MU_AE_EV_PRG_MODE_CAP, 0x00);
			CHECK_ERR(err);
		} else {
			err = m7mu_writeb(client, M7MU_CATEGORY_AE,
				M7MU_AE_EV_PRG_MODE_CAP, 0x04);
			CHECK_ERR(err);
		}

		/* Set AF range to auto */
		err = m7mu_writeb(client, M7MU_CATEGORY_LENS,
			M7MU_LENS_AF_SCAN_RANGE, 0x00);
		CHECK_ERR(err);
		break;

	case MODE_PROGRAM:
	case MODE_SMART_SELF:
		/* Set Monitor EV program mode */
		err = m7mu_writeb(client, M7MU_CATEGORY_AE,
			M7MU_AE_EP_MODE_MON, 0x00);
		CHECK_ERR(err);

		/* Set Still Capture EV program mode */
		if (val == MODE_PROGRAM) {
			if (state->image_stabilizer_mode ==
					V4L2_IMAGE_STABILIZER_OIS) {
				err = m7mu_writeb(client, M7MU_CATEGORY_AE,
					M7MU_AE_EP_MODE_CAP, 0x18);
				CHECK_ERR(err);
			} else {
				err = m7mu_writeb(client, M7MU_CATEGORY_AE,
					M7MU_AE_EP_MODE_CAP, 0x00);
				CHECK_ERR(err);
			}
		} else {
			err = m7mu_writeb(client, M7MU_CATEGORY_AE,
				M7MU_AE_EP_MODE_CAP, 0x00);
			CHECK_ERR(err);
		}

	case MODE_BEST_GROUP_POSE:
	case MODE_BEAUTY_SHOT:
	case MODE_BEST_SHOT:
	case MODE_CONTINUOUS_SHOT:
	case MODE_ERASER:
		err = m7mu_writeb(client, M7MU_CATEGORY_AE, 0x53, 0x01);
		CHECK_ERR(err);

		err = m7mu_writeb(client, M7MU_CATEGORY_MON,
			M7MU_MON_COLOR_EFFECT, state->color_effect);
		CHECK_ERR(err);

		/* Set LIKE_PRO_EN Disable */
		err = m7mu_writeb(client, M7MU_CATEGORY_PRO_MODE, 0x00, 0x00);
		CHECK_ERR(err);

		/* LIKE PRO DISPLAY SET */
		err = m7mu_writeb(client, M7MU_CATEGORY_PRO_MODE, 0x07, 0x00);
		CHECK_ERR(err);

		/* Set CATE_408 to None */
		err = m7mu_writeb(client, M7MU_CATEGORY_NEW, 0x08, 0x00);
		CHECK_ERR(err);

		/* Set HISTOGRAM OFF */
		err = m7mu_writeb(client, M7MU_CATEGORY_MON, 0x58, 0x00);
		CHECK_ERR(err);

		/* Set Monitor EV program mode */
/*		err = m7mu_writeb(client, M7MU_CATEGORY_AE,
			M7MU_AE_EP_MODE_MON, 0x00);
		CHECK_ERR(err);
*/
		/* Set Still Capture EV program mode */
/*		if (val == MODE_PROGRAM) {
			if (state->image_stabilizer_mode ==
			V4L2_IMAGE_STABILIZER_OIS) {
				err = m7mu_writeb(client, M7MU_CATEGORY_AE,
					M7MU_AE_EP_MODE_CAP, 0x18);
				CHECK_ERR(err);
			} else {
				err = m7mu_writeb(client, M7MU_CATEGORY_AE,
					M7MU_AE_EP_MODE_CAP, 0x00);
				CHECK_ERR(err);
			}
		} else {
			err = m7mu_writeb(client, M7MU_CATEGORY_AE,
				M7MU_AE_EP_MODE_CAP, 0x00);
			CHECK_ERR(err);
		}
*/
		/* Still Capture EVP Set Parameter Mode */
		if (state->iso == 0) {
			err = m7mu_writeb(client, M7MU_CATEGORY_AE,
				M7MU_AE_EV_PRG_MODE_CAP, 0x00);
			CHECK_ERR(err);
		} else {
			err = m7mu_writeb(client, M7MU_CATEGORY_AE,
				M7MU_AE_EV_PRG_MODE_CAP, 0x04);
			CHECK_ERR(err);
		}

		if (state->mode == MODE_BEAUTY_SHOT) {
			/* Set AF range to auto-macro */
			err = m7mu_writeb(client, M7MU_CATEGORY_LENS,
				M7MU_LENS_AF_SCAN_RANGE, 0x02);
			CHECK_ERR(err);
		} else if (state->mode == MODE_BEST_GROUP_POSE
		|| state->mode == MODE_BEST_SHOT
		|| state->mode == MODE_CONTINUOUS_SHOT) {
			/* Set AF range to auto */
			err = m7mu_writeb(client, M7MU_CATEGORY_LENS,
				M7MU_LENS_AF_SCAN_RANGE, 0x00);
			CHECK_ERR(err);
		}
		break;

	case MODE_A:
		err = m7mu_writeb(client, M7MU_CATEGORY_AE, 0x53, 0x01);
		CHECK_ERR(err);

		err = m7mu_writeb(client, M7MU_CATEGORY_MON,
			M7MU_MON_COLOR_EFFECT, state->color_effect);
		CHECK_ERR(err);

		/* Set LIKE_PRO_EN Disable */
		err = m7mu_writeb(client, M7MU_CATEGORY_PRO_MODE, 0x00, 0x00);
		CHECK_ERR(err);

		/* LIKE PRO DISPLAY SET */
		err = m7mu_writeb(client, M7MU_CATEGORY_PRO_MODE, 0x07, 0x00);
		CHECK_ERR(err);

		/* Set CATE_408 to None */
		err = m7mu_writeb(client, M7MU_CATEGORY_NEW, 0x08, 0x00);
		CHECK_ERR(err);

		/* Set HISTOGRAM OFF */
		err = m7mu_writeb(client, M7MU_CATEGORY_MON, 0x58, 0x00);
		CHECK_ERR(err);

		/* Set Monitor EV program mode */
		err = m7mu_writeb(client, M7MU_CATEGORY_AE,
			M7MU_AE_EP_MODE_MON, 0x02);
		CHECK_ERR(err);

		/* Set Still Capture EV program mode */
		err = m7mu_writeb(client, M7MU_CATEGORY_AE,
			M7MU_AE_EP_MODE_CAP, 0x02);
		CHECK_ERR(err);

		/* Still Capture EVP Set Parameter Mode */
		if (state->iso == 0) {
			err = m7mu_writeb(client, M7MU_CATEGORY_AE,
				M7MU_AE_EV_PRG_MODE_CAP, 0x01);
			CHECK_ERR(err);
		} else {
		err = m7mu_writeb(client, M7MU_CATEGORY_AE,
				M7MU_AE_EV_PRG_MODE_CAP, 0x05);
		CHECK_ERR(err);
		}

		/* Set Still Capture F-Number Value */
		err = m7mu_writeb(client, M7MU_CATEGORY_AE,
			M7MU_AE_EV_PRG_F_NUMBER, state->f_number);
		CHECK_ERR(err);
		break;

	case MODE_S:
		err = m7mu_writeb(client, M7MU_CATEGORY_AE, 0x53, 0x01);
		CHECK_ERR(err);

		err = m7mu_writeb(client, M7MU_CATEGORY_MON,
			M7MU_MON_COLOR_EFFECT, state->color_effect);
		CHECK_ERR(err);

		/* Set LIKE_PRO_EN Disable */
		err = m7mu_writeb(client, M7MU_CATEGORY_PRO_MODE, 0x00, 0x00);
		CHECK_ERR(err);

		/* LIKE PRO DISPLAY SET */
		err = m7mu_writeb(client, M7MU_CATEGORY_PRO_MODE, 0x07, 0x00);
		CHECK_ERR(err);

		/* Set CATE_408 to None */
		err = m7mu_writeb(client, M7MU_CATEGORY_NEW, 0x08, 0x00);
		CHECK_ERR(err);

		/* Set HISTOGRAM OFF */
		err = m7mu_writeb(client, M7MU_CATEGORY_MON, 0x58, 0x00);
		CHECK_ERR(err);

		/* Set Monitor EV program mode */
/*		err = m7mu_writeb(client, M7MU_CATEGORY_AE,
			M7MU_AE_EP_MODE_MON, 0x04);
		CHECK_ERR(err);
*/
		/* Set Still Capture EV program mode */
/*		err = m7mu_writeb(client, M7MU_CATEGORY_AE,
			M7MU_AE_EP_MODE_CAP, 0x04);
		CHECK_ERR(err);
*/
		/* Still Capture EVP Set Parameter Mode */
		if (state->iso == 0) {
			err = m7mu_writeb(client, M7MU_CATEGORY_AE,
				M7MU_AE_EV_PRG_MODE_CAP, 0x02);
			CHECK_ERR(err);
		} else {
			err = m7mu_writeb(client, M7MU_CATEGORY_AE,
				M7MU_AE_EV_PRG_MODE_CAP, 0x06);
			CHECK_ERR(err);
		}

		/* Set Capture Shutter Speed Time */
/*		err = m7mu_writew(client, M7MU_CATEGORY_AE,
			M7MU_AE_EV_PRG_SS_NUMERATOR, state->numerator);
		CHECK_ERR(err);
		err = m7mu_writew(client, M7MU_CATEGORY_AE,
			M7MU_AE_EV_PRG_SS_DENOMINATOR, state->denominator);
		CHECK_ERR(err);
*/
		break;

	case MODE_M:
		err = m7mu_writeb(client, M7MU_CATEGORY_AE, 0x53, 0x01);
		CHECK_ERR(err);

		err = m7mu_writeb(client, M7MU_CATEGORY_MON,
			M7MU_MON_COLOR_EFFECT, state->color_effect);
		CHECK_ERR(err);

		/* Set LIKE_PRO_EN Disable */
		err = m7mu_writeb(client, M7MU_CATEGORY_PRO_MODE, 0x00, 0x00);
		CHECK_ERR(err);

		/* LIKE PRO DISPLAY SET */
		err = m7mu_writeb(client, M7MU_CATEGORY_PRO_MODE, 0x07, 0x00);
		CHECK_ERR(err);

		/* Set CATE_408 to None */
		err = m7mu_writeb(client, M7MU_CATEGORY_NEW, 0x08, 0x00);
		CHECK_ERR(err);

		/* Set HISTOGRAM OFF */
		err = m7mu_writeb(client, M7MU_CATEGORY_MON, 0x58, 0x00);
		CHECK_ERR(err);

		/* Set Monitor EV program mode */
/*		err = m7mu_writeb(client, M7MU_CATEGORY_AE,
			M7MU_AE_EP_MODE_MON, 0x00);
		CHECK_ERR(err);
*/
		/* Set Still Capture EV program mode */
/*		if (state->image_stabilizer_mode == V4L2_IMAGE_STABILIZER_OIS) {
			err = m7mu_writeb(client, M7MU_CATEGORY_AE,
				M7MU_AE_EP_MODE_CAP, 0x18);
			CHECK_ERR(err);
		} else {
			err = m7mu_writeb(client, M7MU_CATEGORY_AE,
				M7MU_AE_EP_MODE_CAP, 0x00);
			CHECK_ERR(err);
		}
*/
		/* Still Capture EVP Set Parameter Mode */
		err = m7mu_writeb(client, M7MU_CATEGORY_AE,
			M7MU_AE_EV_PRG_MODE_CAP, 0x07);
		CHECK_ERR(err);

		/* Set Still Capture F-Number Value */
/*		err = m7mu_writeb(client, M7MU_CATEGORY_AE,
			M7MU_AE_EV_PRG_F_NUMBER, state->f_number);
		CHECK_ERR(err);
*/
		/* Set Capture Shutter Speed Time */
/*		err = m7mu_writew(client, M7MU_CATEGORY_AE,
			M7MU_AE_EV_PRG_SS_NUMERATOR, state->numerator);
		CHECK_ERR(err);
		err = m7mu_writew(client, M7MU_CATEGORY_AE,
			M7MU_AE_EV_PRG_SS_DENOMINATOR, state->denominator);
		CHECK_ERR(err);
*/
		/* Set Still Capture ISO Value */
/*		err = m7mu_writew(client, M7MU_CATEGORY_AE,
			M7MU_AE_EV_PRG_ISO_VALUE, state->iso);
		CHECK_ERR(err);
*/
		break;

	case MODE_VIDEO:
		err = m7mu_writeb(client, M7MU_CATEGORY_AE, 0x53, 0x01);
		CHECK_ERR(err);

		err = m7mu_writeb(client, M7MU_CATEGORY_MON,
			M7MU_MON_COLOR_EFFECT, state->color_effect);
		CHECK_ERR(err);

		if (state->smart_scene_detect_mode) {
			err = m7mu_writeb(client,
					M7MU_CATEGORY_NEW, 0x08, 0x02);
			CHECK_ERR(err);
		} else {
			err = m7mu_writeb(client,
					M7MU_CATEGORY_NEW, 0x08, 0x00);
			CHECK_ERR(err);
		}

		/* Set LIKE_PRO_EN Disable */
		err = m7mu_writeb(client,
				M7MU_CATEGORY_PRO_MODE, 0x00, 0x00);
		CHECK_ERR(err);

		/* LIKE PRO DISPLAY SET */
		err = m7mu_writeb(client,
				M7MU_CATEGORY_PRO_MODE, 0x07, 0x00);
		CHECK_ERR(err);

		/* Set HISTOGRAM OFF */
		err = m7mu_writeb(client,
				M7MU_CATEGORY_MON, 0x58, 0x00);
		CHECK_ERR(err);

		/* Still Capture EVP Set Parameter Mode */
		err = m7mu_writeb(client, M7MU_CATEGORY_AE,
			M7MU_AE_EV_PRG_MODE_CAP, 0x00);
		CHECK_ERR(err);

		/* Set AF range to AUTO-MACRO */
		err = m7mu_writeb(client, M7MU_CATEGORY_LENS,
			M7MU_LENS_AF_SCAN_RANGE, 0x02);
		CHECK_ERR(err);
#if 0
		if (state->fps == 120) {
			/* Set Monitor EV program mode : 120 fps */
/*			err = m7mu_writeb(client, M7MU_CATEGORY_AE,
				M7MU_AE_EP_MODE_MON, 0x1C);
			CHECK_ERR(err);
*/
			/* Set Still Capture EV program mode : 120 fps */
/*			err = m7mu_writeb(client, M7MU_CATEGORY_AE,
				M7MU_AE_EP_MODE_CAP, 0x1C);
			CHECK_ERR(err);
			*/
		}
#endif

		break;

	case MODE_HIGH_SPEED:
		err = m7mu_writeb(client, M7MU_CATEGORY_AE, 0x53, 0x00);
		CHECK_ERR(err);

		/* Set CATE_408 to None */
		err = m7mu_writeb(client, M7MU_CATEGORY_NEW, 0x08, 0x00);
		CHECK_ERR(err);

		/* Set HISTOGRAM ON */
		err = m7mu_writeb(client, M7MU_CATEGORY_MON, 0x58, 0x01);
		CHECK_ERR(err);

		/* LIKE A PRO MODE SET */
		err = m7mu_writeb(client, M7MU_CATEGORY_PRO_MODE, 0x01, 0x01);
		CHECK_ERR(err);

		/* LIKE PRO DISPLAY SET */
		err = m7mu_writeb(client, M7MU_CATEGORY_PRO_MODE, 0x07, 0x00);
		CHECK_ERR(err);

		/* Still Capture EVP Set Parameter Mode */
		err = m7mu_writeb(client, M7MU_CATEGORY_AE,
			M7MU_AE_EV_PRG_MODE_CAP, 0x00);
		CHECK_ERR(err);

		/* Set Monitor EV program mode */
/*		err = m7mu_writeb(client, M7MU_CATEGORY_AE,
			M7MU_AE_EP_MODE_MON, 0x04);
		CHECK_ERR(err);
*/
		/* Set Still Capture EV program mode */
/*		err = m7mu_writeb(client, M7MU_CATEGORY_AE,
			M7MU_AE_EP_MODE_CAP, 0x13);
		CHECK_ERR(err);
*/
		/* LIKE A PRO STEP SET */
		err = m7mu_writeb(client, M7MU_CATEGORY_PRO_MODE,
			0x02, 0x04);
		CHECK_ERR(err);

		err = m7mu_writeb(client, M7MU_CATEGORY_MON,
			M7MU_MON_COLOR_EFFECT, state->color_effect);
		CHECK_ERR(err);

		/* Set LIKE_PRO_EN Enable */
		err = m7mu_writeb(client, M7MU_CATEGORY_PRO_MODE, 0x00, 0x01);
		CHECK_ERR(err);

		/* Set CATE_409 to 1(PREVIEW) */
		err = m7mu_writeb(client, M7MU_CATEGORY_NEW, 0x09, 0x01);
		CHECK_ERR(err);

		/* Set AF range to auto */
		err = m7mu_writeb(client, M7MU_CATEGORY_LENS,
			M7MU_LENS_AF_SCAN_RANGE, 0x00);
		CHECK_ERR(err);
		break;

	case MODE_LIGHT_TRAIL_SHOT:
		err = m7mu_writeb(client, M7MU_CATEGORY_AE, 0x53, 0x00);
		CHECK_ERR(err);

		/* Set CATE_408 to None */
		err = m7mu_writeb(client, M7MU_CATEGORY_NEW, 0x08, 0x00);
		CHECK_ERR(err);

		/* Set HISTOGRAM ON */
		err = m7mu_writeb(client, M7MU_CATEGORY_MON, 0x58, 0x01);
		CHECK_ERR(err);

		/* LIKE A PRO MODE SET */
		err = m7mu_writeb(client, M7MU_CATEGORY_PRO_MODE, 0x01, 0x02);
		CHECK_ERR(err);

		/* LIKE PRO DISPLAY SET */
		err = m7mu_writeb(client, M7MU_CATEGORY_PRO_MODE, 0x07, 0x00);
		CHECK_ERR(err);
#ifdef CONFIG_MACH_GC2PD
		/* Still Capture EVP Set Parameter Mode */
		err = m7mu_writeb(client, M7MU_CATEGORY_AE,
			M7MU_AE_EV_PRG_MODE_CAP, 0x00);
		CHECK_ERR(err);

		/* Set Monitor EV program mode */
/*		err = m7mu_writeb(client, M7MU_CATEGORY_AE,
			M7MU_AE_EP_MODE_MON, 0x00);
		CHECK_ERR(err);
*/
		/* Set Still Capture EV program mode */
/*		err = m7mu_writeb(client, M7MU_CATEGORY_AE,
			M7MU_AE_EP_MODE_CAP, 0x16);
		CHECK_ERR(err);
*/
#else
		/* Still Capture EVP Set Parameter Mode */
		err = m7mu_writeb(client, M7MU_CATEGORY_AE,
			M7MU_AE_EV_PRG_MODE_CAP, 0x06);
		CHECK_ERR(err);

		/* Set Monitor EV program mode */
/*		err = m7mu_writeb(client, M7MU_CATEGORY_AE,
			M7MU_AE_EP_MODE_MON, 0x04);
		CHECK_ERR(err);
*/
		/* Set Still Capture EV program mode */
/*		err = m7mu_writeb(client, M7MU_CATEGORY_AE,
			M7MU_AE_EP_MODE_CAP, 0x04);
		CHECK_ERR(err);
*/
#endif
		/* LIKE A PRO STEP SET */
		err = m7mu_writeb(client, M7MU_CATEGORY_PRO_MODE,
			0x02, 0x4);
		CHECK_ERR(err);

		err = m7mu_writeb(client, M7MU_CATEGORY_MON,
			M7MU_MON_COLOR_EFFECT, state->color_effect);
		CHECK_ERR(err);
#ifndef CONFIG_MACH_GC2PD
		/* Set Capture Shutter Speed Time - 10s */
/*		err = m7mu_writew(client, M7MU_CATEGORY_AE,
			M7MU_AE_EV_PRG_SS_NUMERATOR, 0x0A);
		CHECK_ERR(err);
		err = m7mu_writew(client, M7MU_CATEGORY_AE,
			M7MU_AE_EV_PRG_SS_DENOMINATOR, 1);
		CHECK_ERR(err);
*/
		/* Set Still Capture ISO Value */
/*		err = m7mu_writew(client, M7MU_CATEGORY_AE,
			M7MU_AE_EV_PRG_ISO_VALUE, 0x64);
		CHECK_ERR(err);
*/
#endif
		/* Set LIKE_PRO_EN Enable */
		err = m7mu_writeb(client, M7MU_CATEGORY_PRO_MODE, 0x00, 0x01);
		CHECK_ERR(err);

		/* Set CATE_409 to 1(PREVIEW) */
		err = m7mu_writeb(client, M7MU_CATEGORY_NEW, 0x09, 0x01);
		CHECK_ERR(err);

		/* Set AF range to auto */
		err = m7mu_writeb(client, M7MU_CATEGORY_LENS,
			M7MU_LENS_AF_SCAN_RANGE, 0x00);
		CHECK_ERR(err);
		break;

	case MODE_WATERFALL:
		err = m7mu_writeb(client, M7MU_CATEGORY_AE, 0x53, 0x01);
		CHECK_ERR(err);

		/* Set CATE_408 to None */
		err = m7mu_writeb(client, M7MU_CATEGORY_NEW, 0x08, 0x00);
		CHECK_ERR(err);

		/* Set HISTOGRAM ON */
		err = m7mu_writeb(client, M7MU_CATEGORY_MON, 0x58, 0x01);
		CHECK_ERR(err);

		/* LIKE A PRO MODE SET */
		err = m7mu_writeb(client, M7MU_CATEGORY_PRO_MODE, 0x01, 0x03);
		CHECK_ERR(err);

		/* LIKE PRO DISPLAY SET */
		err = m7mu_writeb(client, M7MU_CATEGORY_PRO_MODE, 0x07, 0x00);
		CHECK_ERR(err);

		/* Still Capture EVP Set Parameter Mode */
		err = m7mu_writeb(client, M7MU_CATEGORY_AE,
			M7MU_AE_EV_PRG_MODE_CAP, 0x00);
		CHECK_ERR(err);

		/* LIKE A PRO STEP SET */
		err = m7mu_writeb(client, M7MU_CATEGORY_PRO_MODE,
			0x02, 0x04);
		CHECK_ERR(err);

		err = m7mu_writeb(client, M7MU_CATEGORY_MON,
			M7MU_MON_COLOR_EFFECT, state->color_effect);
		CHECK_ERR(err);

		/* Set Monitor EV program mode */
/*		err = m7mu_writeb(client, M7MU_CATEGORY_AE,
			M7MU_AE_EP_MODE_MON, 0x00);
		CHECK_ERR(err);
*/
		/* Set Still Capture EV program mode */
/*		err = m7mu_writeb(client, M7MU_CATEGORY_AE,
			M7MU_AE_EP_MODE_CAP, 0x0E);
		CHECK_ERR(err);
*/
		/* Set LIKE_PRO_EN Enable */
		err = m7mu_writeb(client, M7MU_CATEGORY_PRO_MODE, 0x00, 0x01);
		CHECK_ERR(err);

		/* Set CATE_409 to 1(PREVIEW) */
		err = m7mu_writeb(client, M7MU_CATEGORY_NEW, 0x09, 0x01);
		CHECK_ERR(err);

		/* Set AF range to auto */
		err = m7mu_writeb(client, M7MU_CATEGORY_LENS,
			M7MU_LENS_AF_SCAN_RANGE, 0x00);
		CHECK_ERR(err);
		break;

	case MODE_SILHOUETTE:
		err = m7mu_writeb(client, M7MU_CATEGORY_AE, 0x53, 0x01);
		CHECK_ERR(err);

		/* Set CATE_408 to None */
		err = m7mu_writeb(client, M7MU_CATEGORY_NEW, 0x08, 0x00);
		CHECK_ERR(err);

		/* Set HISTOGRAM ON */
		err = m7mu_writeb(client, M7MU_CATEGORY_MON, 0x58, 0x01);
		CHECK_ERR(err);

		/* LIKE A PRO MODE SET */
		err = m7mu_writeb(client, M7MU_CATEGORY_PRO_MODE, 0x01, 0x04);
		CHECK_ERR(err);

		/* LIKE PRO DISPLAY SET */
		err = m7mu_writeb(client, M7MU_CATEGORY_PRO_MODE, 0x07, 0x00);
		CHECK_ERR(err);

		/* Set Monitor EV program mode */
/*		err = m7mu_writeb(client, M7MU_CATEGORY_AE,
			M7MU_AE_EP_MODE_MON, 0x00);
		CHECK_ERR(err);
*/
		/* Set Still Capture EV program mode */
/*		err = m7mu_writeb(client, M7MU_CATEGORY_AE,
			M7MU_AE_EP_MODE_CAP, 0x00);
		CHECK_ERR(err);
*/
		/* Still Capture EVP Set Parameter Mode */
		err = m7mu_writeb(client, M7MU_CATEGORY_AE,
			M7MU_AE_EV_PRG_MODE_CAP, 0x00);
		CHECK_ERR(err);

		/* LIKE A PRO STEP SET */
		err = m7mu_writeb(client, M7MU_CATEGORY_PRO_MODE,
			0x02, 0x02);
		CHECK_ERR(err);

		/* Set Color effect */
		err = m7mu_writeb(client, M7MU_CATEGORY_MON,
			M7MU_MON_COLOR_EFFECT, state->color_effect);
		CHECK_ERR(err);

		/* Set LIKE_PRO_EN Enable */
		err = m7mu_writeb(client, M7MU_CATEGORY_PRO_MODE, 0x00, 0x01);
		CHECK_ERR(err);

		/* Set CATE_409 to 1(PREVIEW) */
		err = m7mu_writeb(client, M7MU_CATEGORY_NEW, 0x09, 0x01);
		CHECK_ERR(err);

		/* Set AF range to auto */
		err = m7mu_writeb(client, M7MU_CATEGORY_LENS,
			M7MU_LENS_AF_SCAN_RANGE, 0x00);
		CHECK_ERR(err);
		break;

	case MODE_SUNSET:
		err = m7mu_writeb(client, M7MU_CATEGORY_AE, 0x53, 0x01);
		CHECK_ERR(err);

		/* Set CATE_408 to None */
		err = m7mu_writeb(client, M7MU_CATEGORY_NEW, 0x08, 0x00);
		CHECK_ERR(err);

		/* Set HISTOGRAM ON */
		err = m7mu_writeb(client, M7MU_CATEGORY_MON, 0x58, 0x01);
		CHECK_ERR(err);

		/* LIKE A PRO MODE SET */
		err = m7mu_writeb(client, M7MU_CATEGORY_PRO_MODE, 0x01, 0x05);
		CHECK_ERR(err);

		/* LIKE PRO DISPLAY SET */
		err = m7mu_writeb(client, M7MU_CATEGORY_PRO_MODE, 0x07, 0x00);
		CHECK_ERR(err);

		/* Still Capture EVP Set Parameter Mode */
		err = m7mu_writeb(client, M7MU_CATEGORY_AE,
			M7MU_AE_EV_PRG_MODE_CAP, 0x00);
		CHECK_ERR(err);

		/* LIKE A PRO STEP SET */
		err = m7mu_writeb(client, M7MU_CATEGORY_PRO_MODE,
			0x02, 0x02);
		CHECK_ERR(err);

		err = m7mu_writeb(client, M7MU_CATEGORY_MON,
			M7MU_MON_COLOR_EFFECT, state->color_effect);
		CHECK_ERR(err);

		/* Set Monitor EV program mode */
/*		err = m7mu_writeb(client, M7MU_CATEGORY_AE,
			M7MU_AE_EP_MODE_MON, 0x00);
		CHECK_ERR(err);
*/
		/* Set Still Capture EV program mode */
/*		err = m7mu_writeb(client, M7MU_CATEGORY_AE,
			M7MU_AE_EP_MODE_CAP, 0x00);
		CHECK_ERR(err);
*/
		/* Set LIKE_PRO_EN Enable */
		err = m7mu_writeb(client, M7MU_CATEGORY_PRO_MODE, 0x00, 0x01);
		CHECK_ERR(err);

		/* Set CATE_409 to 1(PREVIEW) */
		err = m7mu_writeb(client, M7MU_CATEGORY_NEW, 0x09, 0x01);
		CHECK_ERR(err);

		/* Set AF range to auto */
		err = m7mu_writeb(client, M7MU_CATEGORY_LENS,
			M7MU_LENS_AF_SCAN_RANGE, 0x00);
		CHECK_ERR(err);
		break;

	case MODE_CLOSE_UP:
		err = m7mu_writeb(client, M7MU_CATEGORY_AE, 0x53, 0x01);
		CHECK_ERR(err);

		/* Set CATE_408 to None */
		err = m7mu_writeb(client, M7MU_CATEGORY_NEW, 0x08, 0x00);
		CHECK_ERR(err);

		/* Set HISTOGRAM ON */
		err = m7mu_writeb(client, M7MU_CATEGORY_MON, 0x58, 0x01);
		CHECK_ERR(err);

		/* LIKE A PRO MODE SET */
		err = m7mu_writeb(client, M7MU_CATEGORY_PRO_MODE, 0x01, 0x06);
		CHECK_ERR(err);

		/* LIKE PRO DISPLAY SET */
		err = m7mu_writeb(client, M7MU_CATEGORY_PRO_MODE, 0x07, 0x00);
		CHECK_ERR(err);

		/* Still Capture EVP Set Parameter Mode */
		err = m7mu_writeb(client, M7MU_CATEGORY_AE,
			M7MU_AE_EV_PRG_MODE_CAP, 0x01);
		CHECK_ERR(err);

		/* Set Monitor EV program mode */
/*		err = m7mu_writeb(client, M7MU_CATEGORY_AE,
			M7MU_AE_EP_MODE_MON, 0x02);
		CHECK_ERR(err);
*/
		/* Set Still Capture EV program mode */
/*		err = m7mu_writeb(client, M7MU_CATEGORY_AE,
			M7MU_AE_EP_MODE_CAP, 0x02);
		CHECK_ERR(err);
*/
		/* LIKE A PRO STEP SET */
		err = m7mu_writeb(client, M7MU_CATEGORY_PRO_MODE,
			0x02, 0x04);
		CHECK_ERR(err);

		err = m7mu_writeb(client, M7MU_CATEGORY_MON,
			M7MU_MON_COLOR_EFFECT, state->color_effect);
		CHECK_ERR(err);

		/* Set Still Capture F-Number Value - 2.8 or 3.1*/
/*
#ifdef CONFIG_MACH_GC2PD
		err = m7mu_writeb(client, M7MU_CATEGORY_AE,
			M7MU_AE_EV_PRG_F_NUMBER, 0x31);
#else
		err = m7mu_writeb(client, M7MU_CATEGORY_AE,
			M7MU_AE_EV_PRG_F_NUMBER, 0x28);
#endif
		CHECK_ERR(err);
*/
		/* Set LIKE_PRO_EN Enable */
		err = m7mu_writeb(client, M7MU_CATEGORY_PRO_MODE, 0x00, 0x01);
		CHECK_ERR(err);

		/* Set CATE_409 to 1(PREVIEW) */
		err = m7mu_writeb(client, M7MU_CATEGORY_NEW, 0x09, 0x01);
		CHECK_ERR(err);

		/* Set AF range to auto-macro */
#ifdef CONFIG_MACH_GC2PD
		err = m7mu_writeb(client, M7MU_CATEGORY_LENS,
			M7MU_LENS_AF_SCAN_RANGE, 0x01);
#else
		err = m7mu_writeb(client, M7MU_CATEGORY_LENS,
			M7MU_LENS_AF_SCAN_RANGE, 0x02);
#endif
		CHECK_ERR(err);

		break;

	case MODE_FIREWORKS:
		err = m7mu_writeb(client, M7MU_CATEGORY_AE, 0x53, 0x00);
		CHECK_ERR(err);

		/* Set CATE_408 to None */
		err = m7mu_writeb(client, M7MU_CATEGORY_NEW, 0x08, 0x00);
		CHECK_ERR(err);

		/* Set HISTOGRAM ON */
		err = m7mu_writeb(client, M7MU_CATEGORY_MON, 0x58, 0x01);
		CHECK_ERR(err);

		/* LIKE A PRO MODE SET */
		err = m7mu_writeb(client, M7MU_CATEGORY_PRO_MODE, 0x01, 0x07);
		CHECK_ERR(err);

		/* LIKE PRO DISPLAY SET */
		err = m7mu_writeb(client, M7MU_CATEGORY_PRO_MODE, 0x07, 0x00);
		CHECK_ERR(err);

		/* Still Capture EVP Set Parameter Mode */
		err = m7mu_writeb(client, M7MU_CATEGORY_AE,
			M7MU_AE_EV_PRG_MODE_CAP, 0x07);
		CHECK_ERR(err);

		/* LIKE A PRO STEP SET */
		err = m7mu_writeb(client, M7MU_CATEGORY_PRO_MODE,
			0x02, 0x02);
		CHECK_ERR(err);

		err = m7mu_writeb(client, M7MU_CATEGORY_MON,
			M7MU_MON_COLOR_EFFECT, state->color_effect);
		CHECK_ERR(err);

		/* Set Capture Shutter Speed Time - 3.2s*/
/*		err = m7mu_writew(client, M7MU_CATEGORY_AE,
			M7MU_AE_EV_PRG_SS_NUMERATOR, 32);
		CHECK_ERR(err);
		err = m7mu_writew(client, M7MU_CATEGORY_AE,
			M7MU_AE_EV_PRG_SS_DENOMINATOR, 10);
		CHECK_ERR(err);
*/
		/* Set Still Capture F-Number Value  - 8.0 */
/*
#ifndef CONFIG_MACH_GC2PD
		err = m7mu_writeb(client, M7MU_CATEGORY_AE,
			M7MU_AE_EV_PRG_F_NUMBER, 0x80);
		CHECK_ERR(err);
#endif
*/

		/* Set Still Capture ISO Value - 100 */
/*		err = m7mu_writew(client, M7MU_CATEGORY_AE,
			M7MU_AE_EV_PRG_ISO_VALUE, 0x64);
		CHECK_ERR(err);
*/
		/* Set Monitor EV program mode */
/*		err = m7mu_writeb(client, M7MU_CATEGORY_AE,
			M7MU_AE_EP_MODE_MON, 0x00);
		CHECK_ERR(err);
*/
		/* Set Still Capture EV program mode */
/*		err = m7mu_writeb(client, M7MU_CATEGORY_AE,
			M7MU_AE_EP_MODE_CAP, 0x04);
		CHECK_ERR(err);
*/
		/* Set LIKE_PRO_EN Enable */
		err = m7mu_writeb(client, M7MU_CATEGORY_PRO_MODE, 0x00, 0x01);
		CHECK_ERR(err);

		/* Set CATE_409 to 1(PREVIEW) */
		err = m7mu_writeb(client, M7MU_CATEGORY_NEW, 0x09, 0x01);
		CHECK_ERR(err);

		/* Set AF range to auto */
		err = m7mu_writeb(client, M7MU_CATEGORY_LENS,
			M7MU_LENS_AF_SCAN_RANGE, 0x00);
		CHECK_ERR(err);
		break;

	case MODE_NATURAL_GREEN:
		err = m7mu_writeb(client, M7MU_CATEGORY_AE, 0x53, 0x01);
		CHECK_ERR(err);

		/* Set CATE_408 to None */
		err = m7mu_writeb(client, M7MU_CATEGORY_NEW, 0x08, 0x00);
		CHECK_ERR(err);

		/* Set HISTOGRAM ON */
		err = m7mu_writeb(client, M7MU_CATEGORY_MON, 0x58, 0x01);
		CHECK_ERR(err);

		/* Set Monitor EV program mode */
/*		err = m7mu_writeb(client, M7MU_CATEGORY_AE,
			M7MU_AE_EP_MODE_MON, 0x00);
		CHECK_ERR(err);
*/
		/* Set Still Capture EV program mode */
/*		err = m7mu_writeb(client, M7MU_CATEGORY_AE,
			M7MU_AE_EP_MODE_CAP, 0x00);
		CHECK_ERR(err);
*/
		/* LIKE A PRO MODE SET */
		err = m7mu_writeb(client, M7MU_CATEGORY_PRO_MODE, 0x01, 0x09);
		CHECK_ERR(err);

		/* LIKE PRO DISPLAY SET */
		err = m7mu_writeb(client, M7MU_CATEGORY_PRO_MODE, 0x07, 0x00);
		CHECK_ERR(err);

		/* Still Capture EVP Set Parameter Mode */
		err = m7mu_writeb(client, M7MU_CATEGORY_AE,
			M7MU_AE_EV_PRG_MODE_CAP, 0x00);
		CHECK_ERR(err);

		/* LIKE A PRO STEP SET */
		err = m7mu_writeb(client, M7MU_CATEGORY_PRO_MODE,
			0x02, 0x04);
		CHECK_ERR(err);

		/* COLOR EFFECT SET */
		err = m7mu_readb(client, M7MU_CATEGORY_MON,
			M7MU_MON_COLOR_EFFECT, &color_effect);
		CHECK_ERR(err);

		if (color_effect < 0x11)
			state->color_effect = color_effect;

		err = m7mu_writeb(client, M7MU_CATEGORY_MON,
			M7MU_MON_COLOR_EFFECT, 0x21 + 0x04);
		CHECK_ERR(err);

		/* Set LIKE_PRO_EN Enable */
		err = m7mu_writeb(client, M7MU_CATEGORY_PRO_MODE, 0x00, 0x01);
		CHECK_ERR(err);

		/* Set CATE_409 to 1(PREVIEW) */
		err = m7mu_writeb(client, M7MU_CATEGORY_NEW, 0x09, 0x01);
		CHECK_ERR(err);

		/* Set AF range to auto */
		err = m7mu_writeb(client, M7MU_CATEGORY_LENS,
			M7MU_LENS_AF_SCAN_RANGE, 0x00);
		CHECK_ERR(err);
		break;

	case MODE_DAWN:
		err = m7mu_writeb(client, M7MU_CATEGORY_AE, 0x53, 0x01);
		CHECK_ERR(err);

		err = m7mu_writeb(client, M7MU_CATEGORY_MON,
			M7MU_MON_COLOR_EFFECT, state->color_effect);
		CHECK_ERR(err);

		/* Set CATE_408 to None */
		err = m7mu_writeb(client, M7MU_CATEGORY_NEW, 0x08, 0x00);
		CHECK_ERR(err);

		/* Set HISTOGRAM ON */
		err = m7mu_writeb(client, M7MU_CATEGORY_MON, 0x58, 0x01);
		CHECK_ERR(err);

		/* LIKE A PRO MODE SET */
		err = m7mu_writeb(client, M7MU_CATEGORY_PRO_MODE, 0x01, 0x0B);
		CHECK_ERR(err);

		/* Set Monitor EV program mode */
/*		err = m7mu_writeb(client, M7MU_CATEGORY_AE,
			M7MU_AE_EP_MODE_MON, 0x00);
		CHECK_ERR(err);
*/
		/* Set Still Capture EV program mode */
/*		err = m7mu_writeb(client, M7MU_CATEGORY_AE,
			M7MU_AE_EP_MODE_CAP, 0x00);
		CHECK_ERR(err);
*/
		/* LIKE PRO DISPLAY SET */
#ifdef CONFIG_MACH_GC2PD
		err = m7mu_writeb(client, M7MU_CATEGORY_PRO_MODE, 0x07, 0x00);
#else
		err = m7mu_writeb(client, M7MU_CATEGORY_PRO_MODE, 0x07, 0x01);
#endif
		CHECK_ERR(err);

		/* Still Capture EVP Set Parameter Mode */
		err = m7mu_writeb(client, M7MU_CATEGORY_AE,
			M7MU_AE_EV_PRG_MODE_CAP, 0x00);
		CHECK_ERR(err);

		/* Set LIKE_PRO_EN Enable */
		err = m7mu_writeb(client, M7MU_CATEGORY_PRO_MODE, 0x00, 0x01);
		CHECK_ERR(err);

		/* Set CATE_409 to 1(PREVIEW) */
		err = m7mu_writeb(client, M7MU_CATEGORY_NEW, 0x09, 0x01);
		CHECK_ERR(err);

		/* Set AF range to auto */
		err = m7mu_writeb(client, M7MU_CATEGORY_LENS,
			M7MU_LENS_AF_SCAN_RANGE, 0x00);
		CHECK_ERR(err);
		break;


	case MODE_SNOW:
		err = m7mu_writeb(client, M7MU_CATEGORY_AE, 0x53, 0x01);
		CHECK_ERR(err);

		err = m7mu_writeb(client, M7MU_CATEGORY_MON,
			M7MU_MON_COLOR_EFFECT, state->color_effect);
		CHECK_ERR(err);

		/* Set CATE_408 to None */
		err = m7mu_writeb(client, M7MU_CATEGORY_NEW, 0x08, 0x00);
		CHECK_ERR(err);

		/* Set HISTOGRAM ON */
		err = m7mu_writeb(client, M7MU_CATEGORY_MON, 0x58, 0x01);
		CHECK_ERR(err);

		/* LIKE A PRO MODE SET */
		err = m7mu_writeb(client, M7MU_CATEGORY_PRO_MODE, 0x01, 0x0C);
		CHECK_ERR(err);

		/* Set Monitor EV program mode */
/*		err = m7mu_writeb(client, M7MU_CATEGORY_AE,
			M7MU_AE_EP_MODE_MON, 0x00);
		CHECK_ERR(err);
*/
		/* Set Still Capture EV program mode */
/*		err = m7mu_writeb(client, M7MU_CATEGORY_AE,
			M7MU_AE_EP_MODE_CAP, 0x00);
		CHECK_ERR(err);
*/
		/* LIKE PRO DISPLAY SET */
		err = m7mu_writeb(client, M7MU_CATEGORY_PRO_MODE, 0x07, 0x04);
		CHECK_ERR(err);

		/* LIKE PRO EV VIAS SET */
		err = m7mu_writeb(client, M7MU_CATEGORY_PRO_MODE, 0x08, 0x28);
		CHECK_ERR(err);

		/* Still Capture EVP Set Parameter Mode */
		err = m7mu_writeb(client, M7MU_CATEGORY_AE,
			M7MU_AE_EV_PRG_MODE_CAP, 0x00);
		CHECK_ERR(err);

		/* Set LIKE_PRO_EN Enable */
		err = m7mu_writeb(client, M7MU_CATEGORY_PRO_MODE, 0x00, 0x01);
		CHECK_ERR(err);

		/* Set CATE_409 to 1(PREVIEW) */
		err = m7mu_writeb(client, M7MU_CATEGORY_NEW, 0x09, 0x01);
		CHECK_ERR(err);

		/* Set AF range to auto */
		err = m7mu_writeb(client, M7MU_CATEGORY_LENS,
			M7MU_LENS_AF_SCAN_RANGE, 0x00);
		CHECK_ERR(err);
		break;

	case MODE_BEACH:
		err = m7mu_writeb(client, M7MU_CATEGORY_AE, 0x53, 0x01);
		CHECK_ERR(err);

		/* Set CATE_408 to None */
		err = m7mu_writeb(client, M7MU_CATEGORY_NEW, 0x08, 0x00);
		CHECK_ERR(err);

		/* Set HISTOGRAM ON */
		err = m7mu_writeb(client, M7MU_CATEGORY_MON, 0x58, 0x01);
		CHECK_ERR(err);

		/* LIKE A PRO MODE SET */
		err = m7mu_writeb(client, M7MU_CATEGORY_PRO_MODE, 0x01, 0x0D);
		CHECK_ERR(err);

		/* Set Monitor EV program mode */
/*		err = m7mu_writeb(client, M7MU_CATEGORY_AE,
			M7MU_AE_EP_MODE_MON, 0x00);
		CHECK_ERR(err);
*/
		/* Set Still Capture EV program mode */
/*		err = m7mu_writeb(client, M7MU_CATEGORY_AE,
			M7MU_AE_EP_MODE_CAP, 0x00);
		CHECK_ERR(err);
*/
		/* LIKE PRO DISPLAY SET */
		err = m7mu_writeb(client, M7MU_CATEGORY_PRO_MODE, 0x07, 0x04);
		CHECK_ERR(err);

		/* LIKE PRO EV VIAS SET */
		err = m7mu_writeb(client, M7MU_CATEGORY_PRO_MODE, 0x08, 0x25);
		CHECK_ERR(err);

		/* Still Capture EVP Set Parameter Mode */
		err = m7mu_writeb(client, M7MU_CATEGORY_AE,
			M7MU_AE_EV_PRG_MODE_CAP, 0x00);
		CHECK_ERR(err);

		/* SET GAMMA TBL RGB CAP */
		err = m7mu_writeb(client, M7MU_CATEGORY_MON,
			M7MU_MON_COLOR_EFFECT, 0x23);
		CHECK_ERR(err);

		/* Set LIKE_PRO_EN Enable */
		err = m7mu_writeb(client, M7MU_CATEGORY_PRO_MODE, 0x00, 0x01);
		CHECK_ERR(err);

		/* Set CATE_409 to 1(PREVIEW) */
		err = m7mu_writeb(client, M7MU_CATEGORY_NEW, 0x09, 0x01);
		CHECK_ERR(err);

		/* Set AF range to auto */
		err = m7mu_writeb(client, M7MU_CATEGORY_LENS,
			M7MU_LENS_AF_SCAN_RANGE, 0x00);
		CHECK_ERR(err);
		break;

	case MODE_FOOD:
		err = m7mu_writeb(client, M7MU_CATEGORY_AE, 0x53, 0x01);
		CHECK_ERR(err);

		/* Set CATE_408 to None */
		err = m7mu_writeb(client, M7MU_CATEGORY_NEW, 0x08, 0x00);
		CHECK_ERR(err);

		/* Set HISTOGRAM ON */
		err = m7mu_writeb(client, M7MU_CATEGORY_MON, 0x58, 0x01);
		CHECK_ERR(err);

		/* LIKE A PRO MODE SET */
		err = m7mu_writeb(client, M7MU_CATEGORY_PRO_MODE, 0x01, 0x0E);
		CHECK_ERR(err);

		/* Set Monitor EV program mode */
/*		err = m7mu_writeb(client, M7MU_CATEGORY_AE,
			M7MU_AE_EP_MODE_MON, 0x00);
		CHECK_ERR(err);
*/
		/* Set Still Capture EV program mode */
/*		err = m7mu_writeb(client, M7MU_CATEGORY_AE,
			M7MU_AE_EP_MODE_CAP, 0x00);
		CHECK_ERR(err);
*/
		/* LIKE PRO DISPLAY SET */
		err = m7mu_writeb(client, M7MU_CATEGORY_PRO_MODE, 0x07, 0x02);
		CHECK_ERR(err);

		/* Still Capture EVP Set Parameter Mode */
		err = m7mu_writeb(client, M7MU_CATEGORY_AE,
			M7MU_AE_EV_PRG_MODE_CAP, 0x00);
		CHECK_ERR(err);

		/* SET GAMMA TBL RGB CAP */
		err = m7mu_writeb(client, M7MU_CATEGORY_MON,
			M7MU_MON_COLOR_EFFECT, 0x15);
		CHECK_ERR(err);

		/* Set LIKE_PRO_EN Enable */
		err = m7mu_writeb(client, M7MU_CATEGORY_PRO_MODE, 0x00, 0x01);
		CHECK_ERR(err);

		/* Set CATE_409 to 1(PREVIEW) */
		err = m7mu_writeb(client, M7MU_CATEGORY_NEW, 0x09, 0x01);
		CHECK_ERR(err);

		/* Set AF range to auto-macro */
		err = m7mu_writeb(client, M7MU_CATEGORY_LENS,
			M7MU_LENS_AF_SCAN_RANGE, 0x02);
		CHECK_ERR(err);
		break;

	case MODE_CANDLE:
		err = m7mu_writeb(client, M7MU_CATEGORY_AE, 0x53, 0x01);
		CHECK_ERR(err);

		err = m7mu_writeb(client, M7MU_CATEGORY_MON,
			M7MU_MON_COLOR_EFFECT, state->color_effect);
		CHECK_ERR(err);

		/* Set CATE_408 to None */
		err = m7mu_writeb(client, M7MU_CATEGORY_NEW, 0x08, 0x00);
		CHECK_ERR(err);

		/* Set HISTOGRAM ON */
		err = m7mu_writeb(client, M7MU_CATEGORY_MON, 0x58, 0x01);
		CHECK_ERR(err);

		/* LIKE A PRO MODE SET */
		err = m7mu_writeb(client, M7MU_CATEGORY_PRO_MODE, 0x01, 0x0F);
		CHECK_ERR(err);

		/* Set Monitor EV program mode */
/*		err = m7mu_writeb(client, M7MU_CATEGORY_AE,
			M7MU_AE_EP_MODE_MON, 0x00);
		CHECK_ERR(err);
*/
		/* Set Still Capture EV program mode */
/*		err = m7mu_writeb(client, M7MU_CATEGORY_AE,
			M7MU_AE_EP_MODE_CAP, 0x00);
		CHECK_ERR(err);
*/
		/* LIKE PRO DISPLAY SET */
		err = m7mu_writeb(client, M7MU_CATEGORY_PRO_MODE, 0x07, 0x06);
		CHECK_ERR(err);

		/* LIKE PRO EV VIAS SET */
		err = m7mu_writeb(client, M7MU_CATEGORY_PRO_MODE, 0x08, 0x14);
		CHECK_ERR(err);

		/* Still Capture EVP Set Parameter Mode */
		err = m7mu_writeb(client, M7MU_CATEGORY_AE,
			M7MU_AE_EV_PRG_MODE_CAP, 0x00);
		CHECK_ERR(err);

		/* Set LIKE_PRO_EN Enable */
		err = m7mu_writeb(client, M7MU_CATEGORY_PRO_MODE, 0x00, 0x01);
		CHECK_ERR(err);

		/* Set CATE_409 to 1(PREVIEW) */
		err = m7mu_writeb(client, M7MU_CATEGORY_NEW, 0x09, 0x01);
		CHECK_ERR(err);

		/* Set AF range to auto-macro */
		err = m7mu_writeb(client, M7MU_CATEGORY_LENS,
			M7MU_LENS_AF_SCAN_RANGE, 0x02);
		CHECK_ERR(err);
		break;

	case MODE_PARTY_INDOOR:
		err = m7mu_writeb(client, M7MU_CATEGORY_AE, 0x53, 0x01);
		CHECK_ERR(err);

		err = m7mu_writeb(client, M7MU_CATEGORY_MON,
			M7MU_MON_COLOR_EFFECT, state->color_effect);
		CHECK_ERR(err);

		/* Set CATE_408 to None */
		err = m7mu_writeb(client, M7MU_CATEGORY_NEW, 0x08, 0x00);
		CHECK_ERR(err);

		/* Set HISTOGRAM ON */
		err = m7mu_writeb(client, M7MU_CATEGORY_MON, 0x58, 0x01);
		CHECK_ERR(err);

		/* LIKE A PRO MODE SET */
		err = m7mu_writeb(client, M7MU_CATEGORY_PRO_MODE, 0x01, 0x10);
		CHECK_ERR(err);

		/* Set Monitor EV program mode */
/*		err = m7mu_writeb(client, M7MU_CATEGORY_AE,
			M7MU_AE_EP_MODE_MON, 0x15);
		CHECK_ERR(err);
*/
		/* Set Still Capture EV program mode */
/*		err = m7mu_writeb(client, M7MU_CATEGORY_AE,
			M7MU_AE_EP_MODE_CAP, 0x15);
		CHECK_ERR(err);
*/
		/* LIKE PRO DISPLAY SET */
		err = m7mu_writeb(client, M7MU_CATEGORY_PRO_MODE, 0x07, 0x01);
		CHECK_ERR(err);

		/* Still Capture EVP Set Parameter Mode */
		err = m7mu_writeb(client, M7MU_CATEGORY_AE,
			M7MU_AE_EV_PRG_MODE_CAP, 0x00);
		CHECK_ERR(err);

		/* Set LIKE_PRO_EN Enable */
		err = m7mu_writeb(client, M7MU_CATEGORY_PRO_MODE, 0x00, 0x01);
		CHECK_ERR(err);

		/* Set CATE_409 to 1(PREVIEW) */
		err = m7mu_writeb(client, M7MU_CATEGORY_NEW, 0x09, 0x01);
		CHECK_ERR(err);

		/* Set AF range to auto */
		err = m7mu_writeb(client, M7MU_CATEGORY_LENS,
			M7MU_LENS_AF_SCAN_RANGE, 0x00);
		CHECK_ERR(err);
		break;

	case MODE_GOLF_SHOT:
		/* Set Monitor EV program mode : GolfShot */
/*		err = m7mu_writeb(client, M7MU_CATEGORY_AE,
			M7MU_AE_EP_MODE_MON, 0x12);
		CHECK_ERR(err);
*/
		/* Set Still Capture EV program mode : GolfShot */
/*		err = m7mu_writeb(client, M7MU_CATEGORY_AE,
			M7MU_AE_EP_MODE_CAP, 0x12);
		CHECK_ERR(err);
*/
		err = m7mu_writeb(client, M7MU_CATEGORY_AE, 0x53, 0x00);
		CHECK_ERR(err);

		/* Set CATE_408 to None */
		err = m7mu_writeb(client, M7MU_CATEGORY_NEW, 0x08, 0x00);
		CHECK_ERR(err);

		/* Set HISTOGRAM OFF */
		err = m7mu_writeb(client, M7MU_CATEGORY_MON, 0x58, 0x00);
		CHECK_ERR(err);

		/* LIKE PRO DISPLAY SET */
		err = m7mu_writeb(client, M7MU_CATEGORY_PRO_MODE, 0x07, 0x00);
		CHECK_ERR(err);

		/* Set LIKE_PRO_EN Disable */
		err = m7mu_writeb(client, M7MU_CATEGORY_PRO_MODE, 0x00, 0x00);
		CHECK_ERR(err);

		/* Still Capture EVP Set Parameter Mode */
		err = m7mu_writeb(client, M7MU_CATEGORY_AE,
			M7MU_AE_EV_PRG_MODE_CAP, 0x00);
		CHECK_ERR(err);

		/* Set AF range to auto */
		err = m7mu_writeb(client, M7MU_CATEGORY_LENS,
			M7MU_LENS_AF_SCAN_RANGE, 0x00);
		CHECK_ERR(err);
		break;

	case MODE_MAGIC:
		err = m7mu_writeb(client, M7MU_CATEGORY_PRO_MODE, 0x1, 0x12);
		CHECK_ERR(err);

		err = m7mu_writeb(client, M7MU_CATEGORY_PRO_MODE, 0x0, 0x1);
		CHECK_ERR(err);

		break;

	case MODE_SOCCER:
		/* LIKE A PRO MODE SET to SOCCER mode */
		err = m7mu_writeb(client, M7MU_CATEGORY_PRO_MODE, 0x01, 0x14);
		CHECK_ERR(err);

		/* Set LIKE_PRO_EN Enable */
		err = m7mu_writeb(client, M7MU_CATEGORY_PRO_MODE, 0x00, 0x01);
		CHECK_ERR(err);
		break;

	case MODE_VIRTUAL_TOUR:
		/* LIKE A PRO MODE SET to virtual-tour mode */
		err = m7mu_writeb(client, M7MU_CATEGORY_PRO_MODE, 0x01, 0x16);
		CHECK_ERR(err);

		/* Set LIKE_PRO_EN Enable */
		err = m7mu_writeb(client, M7MU_CATEGORY_PRO_MODE, 0x00, 0x01);
		CHECK_ERR(err);
		break;

	default:
		break;
	}

	if (state->facedetect_mode == V4L2_FACE_DETECTION_NORMAL
		|| state->facedetect_mode == V4L2_FACE_DETECTION_BLINK) {
		if (state->mode == MODE_SMART_AUTO
			|| state->mode == MODE_BEAUTY_SHOT) {
			err = m7mu_writeb(client, M7MU_CATEGORY_FD,
				M7MU_FD_CTL, 0x01);
		} else if (state->mode == MODE_SMART_SUGGEST) {
			err = m7mu_writeb(client, M7MU_CATEGORY_FD,
					M7MU_FD_CTL, 0x91);
		} else {
			err = m7mu_writeb(client, M7MU_CATEGORY_FD,
				M7MU_FD_CTL, 0x11);
		}
		CHECK_ERR(err);
	}

	m7mu_set_gamma(sd);
	m7mu_set_iqgrp(sd, 0);

	m7mu_set_OIS_cap_mode(sd);

	cam_trace("X\n");

	return 0;
}

static int m7mu_set_shutter_speed(struct v4l2_subdev *sd, int val)
{
	struct m7mu_state *state = to_state(sd);
	struct i2c_client *client = to_client(sd);
	int err = 0;
	int numerator = 1, denominator = 30;
	cam_dbg("E, value %d\n", val);

	if (state->mode > MODE_VIDEO) {
		cam_trace("Don't set when like a pro !!!\n");
		return 0;
	}

	switch (val) {
	case 0:	/* default */
		numerator = 1;
		denominator = 30;
		break;

	case 1:
		numerator = 16;
		denominator = 1;
		break;

	case 2:
		numerator = 15;
		denominator = 1;
		break;

	case 3:
		numerator = 14;
		denominator = 1;
		break;

	case 4:
		numerator = 13;
		denominator = 1;
		break;

	case 5:
		numerator = 12;
		denominator = 1;
		break;

	case 6:
		numerator = 11;
		denominator = 1;
		break;

	case 7:
		numerator = 10;
		denominator = 1;
		break;

	case 8:
		numerator = 9;
		denominator = 1;
		break;

	case 9:
		numerator = 8;
		denominator = 1;
		break;

	case 10:
		numerator = 6;
		denominator = 1;
		break;

	case 11:
		numerator = 4;
		denominator = 1;
		break;

	case 12:
		numerator = 3;
		denominator = 1;
		break;

	case 13:
		numerator = 2;
		denominator = 1;
		break;

	case 14:
		numerator = 3;
		denominator = 2;
		break;

	case 15:
		numerator = 1;
		denominator = 1;
		break;

	case 16:
		numerator = 7;
		denominator = 10;
		break;

	case 17:
		numerator = 5;
		denominator = 10;
		break;

	case 18:
		numerator = 1;
		denominator = 3;
		break;

	case 19:
		numerator = 1;
		denominator = 4;
		break;

	case 20:
		numerator = 1;
		denominator = 6;
		break;

	case 21:
		numerator = 1;
		denominator = 8;
		break;

	case 22:
		numerator = 1;
		denominator = 10;
		break;

	case 23:
		numerator = 1;
		denominator = 15;
		break;

	case 24:
		numerator = 1;
		denominator = 20;
		break;

	case 25:
		numerator = 1;
		denominator = 30;
		break;

	case 26:
		numerator = 1;
		denominator = 45;
		break;

	case 27:
		numerator = 1;
		denominator = 60;
		break;

	case 28:
		numerator = 1;
		denominator = 90;
		break;

	case 29:
		numerator = 1;
		denominator = 125;
		break;

	case 30:
		numerator = 1;
		denominator = 180;
		break;

	case 31:
		numerator = 1;
		denominator = 250;
		break;

	case 32:
		numerator = 1;
		denominator = 350;
		break;

	case 33:
		numerator = 1;
		denominator = 500;
		break;

	case 34:
		numerator = 1;
		denominator = 750;
		break;

	case 35:
		numerator = 1;
		denominator = 1000;
		break;

	case 36:
		numerator = 1;
		denominator = 1500;
		break;

	case 37:
		numerator = 1;
		denominator = 2000;
		break;

#if 0
	case 38:
		numerator = 1;
		denominator = 320;
		break;

	case 39:
		numerator = 1;
		denominator = 400;
		break;

	case 40:
		numerator = 1;
		denominator = 500;
		break;

	case 41:
		numerator = 1;
		denominator = 640;
		break;

	case 42:
		numerator = 1;
		denominator = 800;
		break;

	case 43:
		numerator = 1;
		denominator = 1000;
		break;

	case 44:
		numerator = 1;
		denominator = 1250;
		break;

	case 45:
		numerator = 1;
		denominator = 1600;
		break;

	case 46:
		numerator = 1;
		denominator = 2000;
		break;
#endif
	default:
		break;
	}

	state->numerator = numerator;
	state->denominator = denominator;

	/* Set Capture Shutter Speed Time */
	err = m7mu_writew(client, M7MU_CATEGORY_AE,
		M7MU_AE_EV_PRG_SS_NUMERATOR, numerator);
	CHECK_ERR(err);
	err = m7mu_writew(client, M7MU_CATEGORY_AE,
		M7MU_AE_EV_PRG_SS_DENOMINATOR, denominator);
	CHECK_ERR(err);

	cam_trace("X\n");
	return 0;
}

static int m7mu_set_f_number(struct v4l2_subdev *sd, int val)
{
	struct m7mu_state *state = to_state(sd);
	struct i2c_client *client = to_client(sd);
	int err = 0;

	cam_dbg("E, value %d\n", val);

	/* Max value : 18.4 */
	if (val > 184)
		val = 184;

	state->f_number = val;

	/* Set Still Capture F-Number Value */
	err = m7mu_writeb(client, M7MU_CATEGORY_AE,
		M7MU_AE_EV_PRG_F_NUMBER, val);
	CHECK_ERR(err);

	cam_trace("X\n");
	return 0;
}

static int m7mu_set_wb_custom(struct v4l2_subdev *sd, int val)
{
	struct m7mu_state *state = to_state(sd);
	struct i2c_client *client = to_client(sd);
	int err = 0;

	cam_dbg("E\n");

	err = m7mu_writeb(client, M7MU_CATEGORY_WB,
		M7MU_WB_AWB_MODE, 0x02);
	CHECK_ERR(err);

	err = m7mu_writeb(client, M7MU_CATEGORY_WB,
		M7MU_WB_AWB_MANUAL, 0x08);
	CHECK_ERR(err);

	err = m7mu_writeb(client, M7MU_CATEGORY_WB,
		M7MU_WB_CWB_MODE, 0x02);
	CHECK_ERR(err);

	err = m7mu_writew(client, M7MU_CATEGORY_WB,
		M7MU_WB_SET_CUSTOM_RG, state->wb_custom_rg);
	CHECK_ERR(err);

	err = m7mu_writew(client, M7MU_CATEGORY_WB,
		M7MU_WB_SET_CUSTOM_BG, state->wb_custom_bg);
	CHECK_ERR(err);

	err = m7mu_writeb(client, M7MU_CATEGORY_WB,
		M7MU_WB_CWB_MODE, 0x01);
	CHECK_ERR(err);

	cam_trace("X\n");
	return 0;
}

static int m7mu_set_smart_auto_s1_push(struct v4l2_subdev *sd, int val)
{
	struct m7mu_state *state = to_state(sd);
	struct i2c_client *client = to_client(sd);
	int int_factor, err;

	cam_dbg("E val : %d\n", val);

	if ((state->mode == MODE_SMART_AUTO
		|| state->mode == MODE_SMART_SUGGEST)
		|| (state->mode >= MODE_BACKGROUND_BLUR &&
		state->mode <= MODE_PARTY_INDOOR)) {
		if (val == 1) {
			err = m7mu_writeb(client,
					M7MU_CATEGORY_NEW, 0x09, 0x02);
			CHECK_ERR(err);
		} else if (val == 2) {
			CLEAR_ISP_INT1_STATE(sd);

			if (state->facedetect_mode == V4L2_FACE_DETECTION_NORMAL
				&& (state->mode == MODE_SMART_AUTO
				/*|| state->mode == MODE_SMART_SUGGEST*/)) {
				err = m7mu_writeb(client, M7MU_CATEGORY_FD,
					M7MU_FD_CTL, 0x01);
				CHECK_ERR(err);
			}

			err = m7mu_writeb(client,
					M7MU_CATEGORY_NEW, 0x09, 0x04);
			CHECK_ERR(err);

			int_factor = m7mu_wait_interrupt(sd, M7MU_ISP_TIMEOUT);
			if (!(int_factor & M7MU_INT_ATSCENE_UPDATE)) {
				cam_err("M7MU_INT_ATSCENE_UPDATE isn't issued, %#x\n",
					int_factor);
				return -ETIMEDOUT;
			}
			CHECK_ERR(err);
		} else {
			if (state->mode == MODE_SMART_AUTO
					|| state->mode == MODE_SMART_SUGGEST)
				m7mu_set_smart_auto_default_value(sd, 0);

			err = m7mu_writeb(client,
					M7MU_CATEGORY_NEW, 0x09, 0x03);
			CHECK_ERR(err);
		}
	}
	cam_trace("X\n");
	return 0;
}

static int m7mu_set_mon_size(struct v4l2_subdev *sd, int val)
{
	struct m7mu_state *state = to_state(sd);
	struct i2c_client *client = to_client(sd);
	int err, vss_val, current_mode;
	u32 size_val = 0;

	if (state->isp_fw_ver < 0xA02B) {
		cam_dbg("%x firmware cannot working quick monitor mode\n",
			state->isp_fw_ver);
		return 0;
	}

	err = m7mu_readb(client, M7MU_CATEGORY_SYS,
			M7MU_SYS_MODE, &current_mode);
	CHECK_ERR(err);

	if (current_mode != M7MU_PARMSET_MODE) {
		cam_trace("only param mode !!!\n");
		return 0;
	}

	cam_dbg("E\n");

	if (state->fps == 60) {
		if (state->preview_height == 480)
			size_val = 0x2F;
		else if (state->preview_height == 720)
			size_val = 0x25;
		vss_val = 0;
	} else if (state->fps == 30) {
		if (state->preview_height == 1080)
			size_val = 0x28;
		else if (state->preview_height == 720)
			size_val = 0x21;
		else if (state->preview_height == 480)
			size_val = 0x17;
		else if (state->preview_height == 240)
			size_val = 0x09;
		vss_val = 0;
	} else {
		if (state->preview_width == 640)
			size_val = 0x17;
		else if (state->preview_width == 768)
			size_val = 0x33;
		else if (state->preview_width == 960)
			size_val = 0x34;
		else if (state->preview_width == 1056)
			size_val = 0x35;
		else if (state->preview_width == 1280)
			size_val = 0x21;
		else if (state->preview_width == 1920)
			size_val = 0x28;
		vss_val = 0;
	}

	if (val == 1080) {
		if (state->factory_test_num)
			size_val = 0x37;
	}

	err = m7mu_writeb(client, M7MU_CATEGORY_PARM,
		M7MU_PARM_MON_SIZE, size_val);
	CHECK_ERR(err);

	m7mu_set_iqgrp(sd, val);

	m7mu_set_dual_capture_mode(sd, vss_val);

	err = m7mu_writeb(client, M7MU_CATEGORY_CAPCTRL,
			M7MU_CAPCTRL_AUTOMATIC_SHIFT_EN, 0x01);
	CHECK_ERR(err);

	cam_trace("start quick monitor mode !!!\n");
	err = m7mu_writeb(client, M7MU_CATEGORY_PARM, 0x7C, 0x01);
	CHECK_ERR(err);

	cam_trace("X\n");
	return 0;
}

#if 0
static int m7mu_check_dataline(struct v4l2_subdev *sd, int val)
{
	int err = 0;

	cam_dbg("E, value %d\n", val);

	err = m7mu_writeb(client, M7MU_CATEGORY_TEST,
		M7MU_TEST_OUTPUT_YCO_TEST_DATA, val ? 0x01 : 0x00);
	CHECK_ERR(err);

	cam_trace("X\n");
	return 0;
}
#endif

static int m7mu_check_esd(struct v4l2_subdev *sd)
{
	s32 val = 0;
	int err = 0;

	struct m7mu_state *state = to_state(sd);
	struct i2c_client *client = to_client(sd);

	if (state->factory_test_num != 0) {
		cam_dbg("factory test mode !!! ignore esd check\n");
		return 0;
	}

	/* check ISP */
#if 0	/* TO DO */
	err = m7mu_readb(client, M7MU_CATEGORY_TEST,
			M7MU_TEST_ISP_PROCESS, &val);
	CHECK_ERR(err);
	cam_dbg("progress %#x\n", val);
#else
	val = 0x80;
#endif

	if (val != 0x80) {
		goto esd_occur;
	} else {
		m7mu_wait_interrupt(sd, M7MU_ISP_ESD_TIMEOUT);

		err = m7mu_readb(client, M7MU_CATEGORY_SYS,
				M7MU_SYS_ESD_INT, &val);
		CHECK_ERR(err);

		if (val & M7MU_INT_ESD)
			goto esd_occur;
	}

	cam_warn("ESD is not detected\n");
	return 0;

esd_occur:
	cam_warn("ESD shock is detected\n");
	return -EIO;
}

static int m7mu_g_ext_ctrl(struct v4l2_subdev *sd,
		struct v4l2_ext_control *ctrl)
{
	int err = 0, i = 0;
	struct m7mu_state *state = to_state(sd);
	struct i2c_client *client = to_client(sd);
	char *buf = NULL;

	int size = 0, rtn = 0, cmd_size = 0;
	u8 category = 0, sub = 0;
	u32 addr = 0;

	size = ctrl->size;

	cam_dbg("ISPD m7mu_g_ext_ctrl()  id=%d, size=%d\n",
				ctrl->id, ctrl->size);

	if (size > 4096)
		return -1;

	switch (ctrl->id) {
	case V4L2_CID_ISP_DEBUG_READ:

		cmd_size = 2;
		if (size < cmd_size+1)  /* category:1, sub:1, data:>1 */
			return -2;

		buf = kmalloc((size_t)ctrl->size, GFP_KERNEL);
		if (buf == NULL) {
			cam_err("failed to allocate memory\n");
			err = -ENOMEM;
			break;
		}
		if (copy_from_user((void *)buf,
				(void __user *)ctrl->string, size) != 0) {
			err = -1;
			kfree(buf);
			break;
		}

		category = buf[0];
		sub = buf[1];

		memset(buf, 0, size-cmd_size);
		for (i = 0; i < size-cmd_size; i++) {
			err = m7mu_readb(client, category, sub+i, &rtn);
			if (err < 0) {
				cam_err("i2c falied, err %d\n", err);
				kfree(buf);
				return err;
			}
			buf[i] = rtn;
			cam_dbg("ISPD m7mu_readb(client, %x, %x, %x\n",
						category, sub+i, buf[i]);
		}

		if (copy_to_user((void __user *)ctrl->string,
					buf, size-cmd_size) != 0)
			err = -1;

		kfree(buf);
		break;

	case V4L2_CID_ISP_DEBUG_READ_MEM:

		cmd_size = 4;
		if (size < cmd_size+1)  /* cmd size : 4, data : >1 */
			return -2;

		buf = kmalloc(ctrl->size, GFP_KERNEL);
		if (buf == NULL) {
			cam_err("failed to allocate memory\n");
			err = -ENOMEM;
			break;
		}
		if (copy_from_user(buf,
				(void __user *)ctrl->string, size) != 0) {
			err = -1;
			kfree(buf);
			break;
		}

		addr = buf[0]<<24|buf[1]<<16|buf[2]<<8|buf[3];

		memset(buf, 0, size-cmd_size);
		err = m7mu_eep_rw_start(sd, addr);
		if (err < 0) {
			cam_err("i2c falied, err %d\n", err);
			kfree(buf);
			break;
		}
		mdelay(50);
		cam_dbg("ISPD m7mu_mem_read7(sd, %x, %d)\n",
					addr, size-cmd_size);
		err = m7mu_mem_read(sd, size-cmd_size, addr, buf);
		if (err < 0) {
			cam_err("i2c falied, err %d\n", err);
			kfree(buf);
			break;
		}

		if (copy_to_user((void __user *)ctrl->string,
					buf, size-cmd_size) != 0)
			err = -1;

		kfree(buf);
		break;

	case V4L2_CID_CAM_SENSOR_FW_VER:
		strcpy(ctrl->string, state->sensor_ver);
		break;

	default:
		cam_err("no such control id %d\n",
				ctrl->id - V4L2_CID_CAMERA_CLASS_BASE);
		/*err = -ENOIOCTLCMD*/
		/*err = 0;*/
		break;
	}

	/* FIXME
	 * if (err < 0 && err != -ENOIOCTLCMD)
	 *	cam_err("failed, id %d\n",
		ctrl->id - V4L2_CID_CAMERA_CLASS_BASE);
	 */

	return ((err > 0) ? 0 : err);
}

static int m7mu_g_ext_ctrls(struct v4l2_subdev *sd,
		struct v4l2_ext_controls *ctrls)
{
	struct v4l2_ext_control *ctrl = ctrls->controls;
	int i, err = 0;

	for (i = 0; i < ctrls->count; i++, ctrl++) {
		err = m7mu_g_ext_ctrl(sd, ctrl);
		if (err) {
			ctrls->error_idx = i;
			break;
		}
	}
	return err;
}

static int m7mu_s_ext_ctrl(struct v4l2_subdev *sd,
		struct v4l2_ext_control *ctrl)
{
	int err = 0, i = 0;
	char *buf = NULL;

	int size = 0, cmd_size = 0;
	u8 category = 0, sub = 0;
	u32 addr = 0;

	struct i2c_client *client = to_client(sd);

	size = ctrl->size;

	if (size > 1024)
		return -1;

	switch (ctrl->id) {
	case V4L2_CID_ISP_DEBUG_WRITE:

		cmd_size = 2;
		if (size < cmd_size+1) {
			cam_dbg(" return -2 /ISP_DBG write() cmd_size=%d, size=%d\n",
					cmd_size, size);
			return -2;
		}

		buf = kmalloc(ctrl->size, GFP_KERNEL);
		if (buf == NULL) {
			cam_err("failed to allocate memory\n");
			err = -ENOMEM;
			break;
		}
		if (copy_from_user(buf,
				(void __user *)ctrl->string, size) != 0) {
			err = -1;
			kfree(buf);
			break;
		}

		category = buf[0];
		sub = buf[1];

		cam_dbg("ISP_DBG write() %x,%x,%x\n", buf[0], buf[1], buf[2]);

		for (i = 0; i < size-cmd_size; i++) {
			cam_dbg("ISPD m7mu_writeb(client, %x, %x, %x\n",
				category, sub+i, buf[i+cmd_size]);
			err = m7mu_writeb(client, category,
					sub+i, buf[i+cmd_size]);
			if (err < 0) {
				cam_err("i2c falied, err %d\n", err);
				kfree(buf);
				return err;
			}
		}

		kfree(buf);
		break;

	case V4L2_CID_ISP_DEBUG_WRITE_MEM:

		cmd_size = 4;
		if (size < cmd_size+1)
			return -2;

		buf = kmalloc(ctrl->size, GFP_KERNEL);
		if (buf == NULL) {
			cam_err("failed to allocate memory\n");
			err = -ENOMEM;
			break;
		}
		if (copy_from_user(buf,
				(void __user *)ctrl->string, size) != 0) {
			err = -1;
			kfree(buf);
			break;
		}

		addr = buf[0]<<24|buf[1]<<16|buf[2]<<8|buf[3];

		cam_dbg("ISP_DBG write_mem() 0x%08x, size=%d\n",
					addr, size);
		err = m7mu_eep_rw_start(sd, addr);
		if (err < 0) {
			cam_err("i2c falied, err %d\n", err);
			kfree(buf);
			break;
		}
		mdelay(50);
		cam_dbg("ISPD m7mu_mem_write(sd, %x, %d)\n",
				addr, size-cmd_size);
		err = m7mu_mem_write(sd, 0x04,
				size-cmd_size, addr, buf+cmd_size);
		if (err < 0)
			cam_err("i2c falied, err %d\n", err);

		kfree(buf);
		break;

	case V4L2_CID_ISP_DEBUG_LOGV:

		if (size > 0) {
			buf = kmalloc(ctrl->size+1, GFP_KERNEL);
			if (buf == NULL) {
				cam_err("failed to allocate memory\n");
				err = -ENOMEM;
				break;
			}
			if (copy_from_user(buf,
						(void __user *)ctrl->string,
						size) != 0) {
				err = -1;
				kfree(buf);
				break;
			}
			buf[size] = 0;

			m7mu_makeLog(sd, buf, false);

			kfree(buf);
		} else {
			m7mu_makeLog(sd, "default.log", false);
		}

		break;

	case V4L2_CID_CAMERA_FACTORY_EEP_WRITE_VERSION:
	case V4L2_CID_CAMERA_FACTORY_EEP_WRITE_SN:

		buf = kmalloc(ctrl->size, GFP_KERNEL);
		if (buf == NULL) {
			cam_err("failed to allocate memory\n");
			err = -ENOMEM;
			break;
		}
		if (copy_from_user(buf,
					(void __user *)ctrl->string,
					size) != 0) {
			err = -1;
			kfree(buf);
			break;
		}

		cmd_size = (V4L2_CID_CAMERA_FACTORY_EEP_WRITE_VERSION
				== ctrl->id) ? 12 : 19;
		sub = (V4L2_CID_CAMERA_FACTORY_EEP_WRITE_VERSION
				== ctrl->id) ? 0xA0 : 0xA1;

		for (i = 0; i < cmd_size; i++) {
			cam_dbg("EEP_WRITE_VERSION m7mu_writeb(client, %x, %x, %x)\n",
				M7MU_CATEGORY_LENS, 0xB0+i,
				(size > i) ? buf[i] : 0x00);
			if (size > i)
				err = m7mu_writeb(client,
						M7MU_CATEGORY_LENS,
						0xB0+i, buf[i]);
			else
				err = m7mu_writeb(client,
						M7MU_CATEGORY_LENS,
						0xB0+i, 0x00);
			if (err < 0) {
				cam_err("i2c falied, err %d\n", err);
				kfree(buf);
				return err;
			}
		}

		err = m7mu_writeb(client, M7MU_CATEGORY_LENS, sub, 0x00);
		if (err < 0)
			cam_err("i2c falied, err %d\n", err);

		kfree(buf);
		break;

	default:
		cam_err("no such control id %d\n",
				ctrl->id - V4L2_CID_CAMERA_CLASS_BASE);
		break;
	}

	return ((err > 0) ? 0 : err);
}


static int m7mu_s_ext_ctrls(struct v4l2_subdev *sd,
		struct v4l2_ext_controls *ctrls)
{
	struct v4l2_ext_control *ctrl = ctrls->controls;
	int i, err = 0;

	for (i = 0; i < ctrls->count; i++, ctrl++) {
		err = m7mu_s_ext_ctrl(sd, ctrl);
		if (err) {
			ctrls->error_idx = i;
			break;
		}
	}
	return err;
}

#if 0
static int m7mu_check_manufacturer_id(struct v4l2_subdev *sd)
{
	int i, err;
	u8 id;
	u32 addr[] = {0x1000AAAA, 0x10005554, 0x1000AAAA};
	u8 val[3][2] = {
		[0] = {0x00, 0xAA},
		[1] = {0x00, 0x55},
		[2] = {0x00, 0x90},
	};
	u8 reset[] = {0x00, 0xF0};

	/* set manufacturer's ID read-mode */
	for (i = 0; i < 3; i++) {
		err = m7mu_mem_write(sd, 0x06, 2, addr[i], val[i]);
		CHECK_ERR(err);
	}

	/* read manufacturer's ID */
	err = m7mu_mem_read(sd, sizeof(id), 0x10000001, &id);
	CHECK_ERR(err);

	/* reset manufacturer's ID read-mode */
	err = m7mu_mem_write(sd, 0x06, sizeof(reset), 0x10000000, reset);
	CHECK_ERR(err);

	cam_dbg("%#x\n", id);

	return id;
}
#endif

#if defined(M7MU_ENABLE_ISP_FIRMWARE_UPDATE)
#ifndef CONFIG_VIDEO_M7MU_SPI
#define MAX_TX_BUF_SZ	(4*1024)
#define BASE_ADDRESS	0x40000000
char tx_buf[MAX_TX_BUF_SZ];

static int m7mu_i2c_write(struct v4l2_subdev *sd,
		const u8 *addr, const int len, const int txSize)
{
	int i, j = 0, k = 0;
	char swap_tmp = 0;
	int ret = 0;
	u32 count = len/txSize;
	u32 remain = len%txSize;

	int err = 0;

	cam_err("Start!!\n");
	cam_err("remain : %d\n", remain);

	for (i = 0 ; i < count ; i++) {
		memcpy(tx_buf, &addr[j] , txSize);

		for (k = 0 ; k < (txSize / 4) ; k++) {

			swap_tmp = tx_buf[k * 4];
			tx_buf[k * 4] = tx_buf[k * 4 + 3];
			tx_buf[k * 4 + 3] = swap_tmp;

			swap_tmp = tx_buf[k * 4 + 1];
			tx_buf[k * 4 + 1] = tx_buf[k * 4 + 2];
			tx_buf[k * 4 + 2] = swap_tmp;
		}

		err = m7mu_mem_write(sd, 0x08, txSize,
			BASE_ADDRESS+j , tx_buf);

		cam_err("data : %#x %#x %#x %#x, count : %d\n",
				tx_buf[0], tx_buf[1], tx_buf[2], tx_buf[3], i);

		j += txSize;

		if (err <= 0) {
			cam_err("i2c failed, err %d\n", err);
			err = -1;
			goto program_err;
		}
	}

	if (remain) {
		memcpy(tx_buf, &addr[j], remain);

		for (k = 0; k < (remain / 4); k++) {

			swap_tmp = tx_buf[k * 4];
			tx_buf[k * 4] = tx_buf[k * 4 + 3];
			tx_buf[k * 4 + 3] = swap_tmp;

			swap_tmp = tx_buf[k * 4 + 1];
			tx_buf[k * 4 + 1] = tx_buf[k * 4 + 2];
			tx_buf[k * 4 + 2] = swap_tmp;
		}

		err = m7mu_mem_write(sd, 0x08, remain,
			BASE_ADDRESS+j , tx_buf);

		cam_err("FW send : %#x , addr : %#x, count : %d\n",
				j + remain, BASE_ADDRESS + j, i);

		if (err <= 0) {
			cam_err("i2c failed, err %d\n", err);
			err = -1;
			goto program_err;
		}
	}

program_err:
	return ret;
}
#endif

static int m7mu_program_fw(struct v4l2_subdev *sd,
		char *image, int image_size)
{
	int err = 0;
	int i, j;
	int start, end;
	int cmd_num, cmd_param, sdram_param;
	int retries = 0;
	int val = 0;
	int tx_size = 0;
	struct m7mu_fw_header *hdr = &g_fw_header;
	struct i2c_client *client = to_client(sd);

	cam_dbg("E\n");

	i = j = 0;
	start = end = 0;
	cmd_num = cmd_param = sdram_param = 0;

	if (image == NULL) {
		cam_err("image is null\n");
		err = -1;
		goto program_error;
	}

	/* configurate isp : sdram */
	start = M7MU_FLASH_SDITOPSCNF2;
	end = M7MU_FLASH_SDIDDRZQ1;
	cmd_num = ((end - start) / 4 + 1);

	if (sizeof(struct m7mu_fw_header) > image_size) {
		cam_err("hdr size error : %d\n", image_size);
		err = -1;
		goto program_error;
	}

	memcpy((char *)hdr, image, sizeof(struct m7mu_fw_header));

	for (i = 0; i < cmd_num; i++) {
		cmd_param = start + i*4;
		sdram_param = (int)(*(&hdr->sdram.bit.sditopscnf2 + i));

		err = m7mu_writel(client,
				M7MU_CATEGORY_FLASH,
				cmd_param,
				sdram_param);
		if (err <= 0) {
			cam_err("i2c failed, err %d\n", err);
			err = -1;
			goto program_error;
		}
	}

	/* DPL value of PLLCNTL register */
	cmd_param = M7MU_FLASH_DPLSEL;
	sdram_param = hdr->sdram.bit.sdram_clk;

	err = m7mu_writeb(client,
			M7MU_CATEGORY_FLASH,
			cmd_param,
			(int)(sdram_param & (0xff)));

	if (err <= 0) {
		cam_err("i2c failed, err %d\n", err);
		err = -1;
		goto program_error;
	}

	/* configurate isp: sio */
	err = m7mu_writel(client,
			M7MU_CATEGORY_FLASH,
			M7MU_FLASH_DATA_RAM_ADDR,
			0x40000000); /* 0x40000000 ISP SDRAM Base*/
	if (err <= 0) {
		cam_err("i2c failed, err %d\n", err);
		err = -1;
		goto program_error;
	}
#ifdef CONFIG_VIDEO_M7MU_SPI
	err = m7mu_writeb(client,
			M7MU_CATEGORY_FLASH,
			M7MU_FLASH_SIO_CS_POL,
			0x01); /* 0x00 ISP CS POL */

	if (err <= 0) {
		cam_err("i2c failed, err %d\n", err);
		err = -1;
		goto program_error;
	}

	err = m7mu_writeb(client,
			M7MU_CATEGORY_FLASH,
			M7MU_FLASH_SMR_VALUE,
			0x44); /* 0x00 ISP SMR */

	if (err <= 0) {
		cam_err("i2c failed, err %d\n", err);
		err = -1;
		goto program_error;
	}
#endif
	err = m7mu_writeb(client,
			M7MU_CATEGORY_FLASH,
			M7MU_FLASH_RAM_START,
			0x02);

	if (err <= 0) {
		cam_err("i2c failed, err %d\n", err);
		err = -1;
		goto program_error;
	}

	/* configurate isp: check ready for ISP CSIO */
	retries = 50;
	val = 0x02;

	do {
		msleep(300);
		err = m7mu_readb(client,
				M7MU_CATEGORY_FLASH,
				M7MU_FLASH_RAM_START,
				&val);
		if (err <= 0) {
			cam_err("fail to ISP CSIO setup err:%d\n", err);
			err = -1;
			break;
		}
	} while (val == 0x02 && retries++ < M7MU_I2C_VERIFY);

	if (val == 0x02)
		cam_err("Check Start SIO complete flag:%d retry:%d\n",
				val, retries);

#ifndef CONFIG_VIDEO_M7MU_SPI
	tx_size = 4*1024; /* 8*1024;*/

	cam_dbg("trasnfer ready::image size = %d txSize:%d\n",
				image_size, tx_size);

	err = m7mu_i2c_write(sd,
			(unsigned char *)image, image_size, tx_size);

	if (err < 0) {
		cam_err("m7mu_temp_i2c_write falied\n");
		err = -1;
		goto program_error;
	}

#else /*  SPI */
	/* send image :NSS is High Control */
	if (!gpio_request(GPIO_ISP_SPI1_EN, "GPA2_5")) {
			gpio_direction_output(GPIO_ISP_SPI1_EN, 1);
			gpio_free(GPIO_ISP_SPI1_EN);
			cam_dbg("CS is setting to 1.\n");
	}

	/* send image :MISO is High Control */
	if (!gpio_request(GPIO_ISP_SPI1_MISO, "GPA2_6")) {
			gpio_direction_output(GPIO_ISP_SPI1_MISO, 1);
			gpio_free(GPIO_ISP_SPI1_MISO);
			cam_dbg("MISO is setting to 1.\n");
	}

	mdelay(30);

	tx_size = 8*1024;
	cam_dbg("trasnfer ready::image size = %d txSize:%d\n",
				image_size, tx_size);

	/* send image :spi transfer */
	err = m7mu_spi_write((unsigned char *)image,
			image_size, tx_size);

	if (err < 0) {
		cam_err("m7mu_spi_write falied\n");
		err = -1;
		goto program_error;
	}

	/* send image :NSS is Low Control */
	if (!gpio_request(GPIO_ISP_SPI1_EN, "GPA2_5")) {
		gpio_direction_output(GPIO_ISP_SPI1_EN, 0);
		gpio_free(GPIO_ISP_SPI1_EN);
		cam_dbg("CS is setting to 0.\n");
	}

	/* send image :MISO is Low Control */
	if (!gpio_request(GPIO_ISP_SPI1_MISO, "GPA2_6")) {
		gpio_direction_output(GPIO_ISP_SPI1_MISO, 0);
		gpio_free(GPIO_ISP_SPI1_MISO);
		cam_dbg("MISO is setting to 0.\n");
	}

#endif

	sdram_param = hdr->top.bit.write_code_entry;
	/* address of the writer execution 0x40000400*/
	err = m7mu_writel(client,
						M7MU_CATEGORY_FLASH,
						M7MU_FLASH_CAM_START_AD,
						sdram_param);

	if (err <= 0) {
		cam_err("i2c failed, err %d\n", err);
		err = -1;
		goto program_error;
	}

	err = m7mu_writeb(client,
						M7MU_CATEGORY_FLASH,
						M7MU_FLASH_CAM_START,
						0x02);/* programming mode */

	if (err <= 0) {
		cam_err("i2c failed, err %d\n", err);
		err = -1;
		goto program_error;
	}

	CLEAR_ISP_BOOT_INT_STATE(sd);
	/*	programming ISP: waiting completed	*/
	if (m7mu_wait_boot_interrupt(sd, 120000)) {
		cam_err("boot time out\n");
		err = -1;
		goto program_error;
	}

program_error:

	cam_dbg("X\n");

	if (err < 0)
		err = -1;
	else
		err = 0;

	return err;
}

static int m7mu_load_fw_main(struct v4l2_subdev *sd)
{
	int err = 0;
	struct file *fp = NULL;
	struct device *dev = sd->v4l2_dev->dev;
	const struct firmware *fw = NULL;
	char *image_buf = NULL;
	int image_size = 0;
	mm_segment_t old_fs;
	/* char filename[128]; */
	int nread = 0;
	int fw_requested = 1;

	cam_dbg("E\n");

	/* memset(filename, 0, 128); */

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	cam_trace("mmc_fw = %d\n", mmc_fw);

	if (mmc_fw == 1)
		fp = filp_open(M7MU_FW_PATH, O_RDONLY, 0);
	else
		fp = filp_open(M7MU_FW_MMC_PATH, O_RDONLY, 0);

	if (mmc_fw == 0) {
		if (!IS_ERR(fp) && fp != NULL)
			filp_close(fp, current->files);
		goto request_fw;
	}

	if (IS_ERR(fp) || fp == NULL) {
		cam_trace("failed to open %s/%s, err %ld\n",
				M7MU_FW_PATH, M7MU_FW_MMC_PATH , PTR_ERR(fp));
		if (PTR_ERR(fp) == -4) {
			cam_err("%s: file open I/O is interrupted\n", __func__);
			return -EIO;
		}
		goto request_fw;
	}

	fw_requested = 0;
	if (mmc_fw == 1) {	/* /sdcard/RS_M7MU.bin */
		image_size = fp->f_path.dentry->d_inode->i_size;
		cam_err("file path %s, image_size %d\n",
				M7MU_FW_PATH, image_size);
	} else {
		image_size = FW_BIN_SIZE;
		cam_err("file path %s, image_size %d\n",
				M7MU_FW_MMC_PATH, image_size);
	}

	image_buf = vmalloc(image_size);

	if (image_buf == NULL) {
		cam_err("failed to alloc memory %d Bytes\n", image_size);
		err = -1;
		goto out;
	}

	nread = vfs_read(fp,
					image_buf,
					image_size, &fp->f_pos);

	if (nread != image_size) {
		cam_err("failed to read firmware file, %d Bytes\n", nread);
		err = -1;
		goto out;
	}

	if (!IS_ERR(fp) && fp != NULL)
			filp_close(fp, current->files);

request_fw:
	if (fw_requested) {
		set_fs(old_fs);

		cam_info("Firmware Path = %s\n", M7MU_FW_REQ_PATH);
		err = request_firmware(&fw, M7MU_FW_REQ_PATH, dev);

		if (err != 0) {
			cam_err("request_firmware failed\n");
			err = -EINVAL;
			goto out;
		}

		image_size = fw->size;
		image_buf = (u8 *)fw->data;
	}

	err = m7mu_program_fw(sd,
							image_buf,
							image_size);
	if (err < 0) {
		cam_err("failed to program\n");
		err = -1;
		goto out;
	}

out:

	if (!fw_requested) {
		filp_close(fp, current->files);
		fp = NULL;

		set_fs(old_fs);

		vfree(image_buf);
		image_buf = NULL;
	} else {
		release_firmware(fw);
	}

	cam_dbg("X\n");

	return err;
}
#endif /* M7MU_ENABLE_ISP_FIRMWARE_UPDATE */

#if 0
static int m7mu_load_fw_info(struct v4l2_subdev *sd)
{
	u8 *buf_m7mu = NULL;
	int err = 0;

	struct file *fp;
	mm_segment_t old_fs;
	long fsize, nread;
	int fw_requested = 1;

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	fp = filp_open(FW_INFO_PATH, O_RDONLY, 0);
	if (IS_ERR(fp)) {
		cam_trace("failed to open %s, err %ld\n",
			M7MU_FW_PATH, PTR_ERR(fp));
		if (PTR_ERR(fp) == -4) {
			cam_err("%s: file open I/O is interrupted\n", __func__);
			return -EIO;
		}
	}
	fsize = fp->f_path.dentry->d_inode->i_size;

	cam_err("start, file path %s, size %ld Bytes\n", FW_INFO_PATH, fsize);

	buf_m7mu = vmalloc(fsize);
	if (!buf_m7mu) {
		cam_err("failed to allocate memory\n");
		err = -ENOMEM;
		goto out;
	}

	nread = vfs_read(fp, (char __user *)buf_m7mu, fsize, &fp->f_pos);
	if (nread != fsize) {
		cam_err("failed to read firmware file, %ld Bytes\n", nread);
		err = -EIO;
		goto out;
	}

	err = m7mu_mem_write(sd, 0x04, SZ_64,
				0x90001200 , buf_port_seting0);
			CHECK_ERR(err);
			mdelay(10);

	err = m7mu_mem_write(sd, 0x04, SZ_64,
				0x90001000 , buf_port_seting1);
			CHECK_ERR(err);
			mdelay(10);

	err = m7mu_mem_write(sd, 0x04, SZ_64,
				0x90001100 , buf_port_seting2);
			CHECK_ERR(err);
			mdelay(10);

	err = m7mu_writel(client, M7MU_CATEGORY_FLASH,
				0x1C, 0x0247036D);
	CHECK_ERR(err);

	err = m7mu_writeb(client, M7MU_CATEGORY_FLASH,
				0x4A, 0x01);
	CHECK_ERR(err);
			mdelay(10);

	/* program FLSH ROM */
	err = m7mu_program_fw(sd, buf_m7mu, M7MU_FLASH_BASE_ADDR_1, SZ_4K, 1);
	if (err < 0)
		goto out;


#if 0
	offset = SZ_64K * 31;
	if (id == 0x01) {
		err = m7mu_program_fw(sd, buf + offset,
				M7MU_FLASH_BASE_ADDR + offset, SZ_8K, 4, id);
	} else {
		err = m7mu_program_fw(sd, buf + offset,
				M7MU_FLASH_BASE_ADDR + offset, SZ_4K, 8, id);
	}
#endif
	cam_err("end\n");

out:
	vfree(buf_m7mu);
	if (!IS_ERR(fp) && fp != NULL)
		filp_close(fp, current->files);
	set_fs(old_fs);

	return err;
}

static int m7mu_load_fw(struct v4l2_subdev *sd)
{
	struct device *dev = sd->v4l2_dev->dev;
	const struct firmware *fw = NULL;
	u8 sensor_ver[M7MU_FW_VER_LEN] = {0, };
	u8 *buf_m7mu = NULL, *buf_fw_info = NULL;
	/*int offset;*/
	int err = 0;

	struct file *fp;
	mm_segment_t old_fs;
	long fsize, nread;
	int fw_requested = 1;

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	fp = filp_open(M7MU_FW_PATH, O_RDONLY, 0);
	if (IS_ERR(fp)) {
		cam_err("failed to open %s, err %ld\n",
			M7MU_FW_PATH, PTR_ERR(fp));
		if (PTR_ERR(fp) == -4) {
			cam_err("%s: file open I/O is interrupted\n", __func__);
			return -EIO;
		}
		goto request_fw;
	}

	fw_requested = 0;
	fsize = fp->f_path.dentry->d_inode->i_size;

	cam_err("start, file path %s, size %ld Bytes\n", M7MU_FW_PATH, fsize);

	buf_m7mu = vmalloc(fsize);
	if (!buf_m7mu) {
		cam_err("failed to allocate memory\n");
		err = -ENOMEM;
		goto out;
	}

	nread = vfs_read(fp, (char __user *)buf_m7mu, fsize, &fp->f_pos);
	if (nread != fsize) {
		cam_err("failed to read firmware file, %ld Bytes\n", nread);
		err = -EIO;
		goto out;
	}

	if (!IS_ERR(fp) && fp != NULL)
		filp_close(fp, current->files);

	fp = filp_open(FW_INFO_PATH, O_RDONLY, 0);
	if (IS_ERR(fp)) {
		cam_trace("failed to open %s, err %ld\n",
			FW_INFO_PATH, PTR_ERR(fp));
		if (PTR_ERR(fp) == -4) {
			cam_err("%s: file open I/O is interrupted\n", __func__);
			return -EIO;
		}
		goto request_fw;
	}

	fw_requested = 0;
	fsize = fp->f_path.dentry->d_inode->i_size;

	cam_err("start, file path %s, size %ld Bytes\n", FW_INFO_PATH, fsize);

	buf_fw_info = vmalloc(fsize);
	if (!buf_fw_info) {
		cam_err("failed to allocate memory\n");
		err = -ENOMEM;
		goto out;
	}

	nread = vfs_read(fp, (char __user *)buf_fw_info, fsize, &fp->f_pos);
	if (nread != fsize) {
		cam_err("failed to read firmware file, %ld Bytes\n", nread);
		err = -EIO;
		goto out;
	}


request_fw:
	if (fw_requested) {
		set_fs(old_fs);

	m7mu_get_sensor_fw_version(sd);

	if (sensor_ver[0] == 'T' && sensor_ver[1] == 'B') {
		err = request_firmware(&fw, M7MUTB_FW_PATH, dev);
#if defined(CONFIG_MACH_Q1_BD)
	} else if (sensor_ver[0] == 'O' && sensor_ver[1] == 'O') {
		err = request_firmware(&fw, M7MUOO_FW_PATH, dev);
#endif
#if defined(CONFIG_MACH_U1_KOR_LGT)
	} else if (sensor_ver[0] == 'S' && sensor_ver[1] == 'B') {
		err = request_firmware(&fw, M7MUSB_FW_PATH, dev);
#endif
	} else {
		cam_err("cannot find the matched F/W file\n");
		err = -EINVAL;
	}

	if (err != 0) {
		cam_err("request_firmware falied\n");
			err = -EINVAL;
			goto out;
	}
		cam_dbg("start, size %d Bytes\n", fw->size);
		buf_m7mu = (u8 *)fw->data;
	}


	err = m7mu_mem_write(sd, 0x04, SZ_64,
				0x90001200 , buf_port_seting0);
			CHECK_ERR(err);
			mdelay(10);

	err = m7mu_mem_write(sd, 0x04, SZ_64,
				0x90001000 , buf_port_seting1);
			CHECK_ERR(err);
			mdelay(10);

	err = m7mu_mem_write(sd, 0x04, SZ_64,
				0x90001100 , buf_port_seting2);
			CHECK_ERR(err);
			mdelay(10);

	err = m7mu_writel(client, M7MU_CATEGORY_FLASH,
				0x1C, 0x0247036D);
	CHECK_ERR(err);

	err = m7mu_writeb(client, M7MU_CATEGORY_FLASH,
				0x4A, 0x01);
	CHECK_ERR(err);
			mdelay(10);

	/* program FLSH ROM */
	err = m7mu_program_fw(sd, buf_m7mu, M7MU_FLASH_BASE_ADDR, SZ_4K, 504);
	if (err < 0)
		goto out;

	if (buf_fw_info > 0) {
		err = m7mu_program_fw(sd, buf_fw_info,
				M7MU_FLASH_BASE_ADDR_1, SZ_4K, 1);
		if (err < 0)
			goto out;
	}

#if 0
	offset = SZ_64K * 31;
	if (id == 0x01) {
		err = m7mu_program_fw(sd, buf + offset,
				M7MU_FLASH_BASE_ADDR + offset, SZ_8K, 4, id);
	} else {
		err = m7mu_program_fw(sd, buf + offset,
				M7MU_FLASH_BASE_ADDR + offset, SZ_4K, 8, id);
	}
#endif
	cam_err("end\n");

out:
	if (!fw_requested) {
		vfree(buf_m7mu);
		vfree(buf_fw_info);

		if (!IS_ERR(fp) && fp != NULL)
			filp_close(fp, current->files);
		set_fs(old_fs);
	} else {
		release_firmware(fw);
	}

	return err;
}
#endif

static int m7mu_set_factory_af_led_onoff(struct v4l2_subdev *sd, bool on)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	const struct m7mu_platform_data *pdata = client->dev.platform_data;

	if (on == true) {
		/* AF LED regulator on */
		if (pdata != NULL && pdata->af_led_power != NULL)
			pdata->af_led_power(1);
	} else {
		/* AF LED regulator off */
		if (pdata != NULL && pdata->af_led_power != NULL)
			pdata->af_led_power(0);
	}
	return 0;
}

static int m7mu_get_noti(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	int err = 0;
	cam_info("%s E\n", __func__);
	ctrl->value = 200;

	err = m7mu_wait_sound_interrupt(sd, M7MU_SOUND_TIMEOUT);
	if (err < 0) {
		cam_err("%s: sound interrupt is not occured\n", __func__);
		return err;
	}

	cam_info("%s X %d\n", __func__, ctrl->value);
	return ctrl->value;
}

static int m7mu_noti_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	int err = 0;
	cam_info("noti_ctrl is called %d\n",
			ctrl->id - V4L2_CID_PRIVATE_BASE);

	switch (ctrl->id) {
	case V4L2_CID_CAMERA_NOTI:
		err = m7mu_get_noti(sd, ctrl);
		break;

	default:
		cam_err("no such control id %d\n",
				ctrl->id - V4L2_CID_PRIVATE_BASE);
		/*err = -ENOIOCTLCMD*/
		err = 0;
		break;
	}

	if (err < 0 && err != -ENOIOCTLCMD)
		cam_err("failed, id %d\n", ctrl->id - V4L2_CID_PRIVATE_BASE);

	return err;
}

/* --9 */
static int m7mu_s_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	struct m7mu_state *state = to_state(sd);
	struct i2c_client *client = to_client(sd);
	int err = 0;
	s16 temp;
	int grp_type = 0;
	/*int value = 0;*/

	struct timespec curr_tm;

	getnstimeofday(&curr_tm);

	cam_info("TIME: %.2lu:%.2lu:%.2lu:%.6lu \r\n",
			(curr_tm.tv_sec / 3600) % (24),
			(curr_tm.tv_sec / 60) % (60),
			curr_tm.tv_sec % 60,
			curr_tm.tv_nsec / 1000);

	/* V4L2 ID check */
	if ((s32)((ctrl->id & 0xFFFFFFF) - V4L2_CID_PRIVATE_BASE) > 0) {
		cam_info("ID : V4L2_CID_PRIVATE_BASE + %d, value = %d",
				ctrl->id - V4L2_CID_PRIVATE_BASE, ctrl->value);
		grp_type = V4L2_CID_PRIVATE_BASE;
	} else if ((s32)((ctrl->id & 0xFFFFFF) - V4L2_CID_SENSOR_BASE) > 0) {
		cam_info("ID : V4L2_CID_SENSOR_BASE + %d, value = %d",
				(ctrl->id & 0xFFFFFF) - V4L2_CID_SENSOR_BASE,
				ctrl->value);
		grp_type = V4L2_CID_SENSOR_BASE;
	} else if ((s32)((ctrl->id & 0xFFFFFF) -
				V4L2_CID_CAMERA_CLASS_BASE) > 0) {
		cam_info("ID : V4L2_CID_CAMERA_CLASS_BASE + %d, value = %d",
				ctrl->id - V4L2_CID_CAMERA_CLASS_BASE,
				ctrl->value);
		grp_type = V4L2_CID_CAMERA_CLASS_BASE;
	} else if ((u32)((ctrl->id & 0xFFFFFF) -
				V4L2_CTRL_CLASS_CAMERA) > 0) {
		cam_info("ID : V4L2_CTRL_CLASS_CAMERA + %d, value = %d",
				ctrl->id - V4L2_CTRL_CLASS_CAMERA, ctrl->value);
		grp_type = V4L2_CTRL_CLASS_CAMERA;
	} else {
		cam_err("no such control id %d, value %d\n",
				ctrl->id, ctrl->value);
	}

	if (unlikely(state->isp.bad_fw &&
				ctrl->id != V4L2_CID_CAM_UPDATE_FW)) {
		cam_err("\"Unknown\" state, please update F/W");
		return 0;
	}

	switch (ctrl->id) {
#ifdef HOLD_LENS_SUPPORT
	case V4L2_CID_CAMERA_HOLD_LENS:
		leave_power = true;
		break;
#endif
	case V4L2_CID_CAM_UPDATE_FW:
		switch (ctrl->value) {
		case FW_MODE_VERSION:
			err = m7mu_check_fw(sd);
			break;
		case FW_MODE_UPDATE:
			err = m7mu_fw_writing(sd);
			if (err == 0)
				m7mu_power(sd, M7MU_HW_POWER_ON, 1);
			break;
		case FW_MODE_DUMP:
			err = m7mu_dump_fw(sd);
			break;
		case FW_MODE_AUTO:
			err = m7mu_auto_fw_check(sd);
			break;
		}
		break;

	case V4L2_CID_CAMERA_SENSOR_MODE:
		err = m7mu_set_sensor_mode(sd, ctrl->value);
		break;

	case V4L2_CID_CAM_FLASH_MODE:
		err = m7mu_set_flash(sd, ctrl->value, 0);
		break;

	case V4L2_CID_CAM_ISO:
		err = m7mu_set_iso(sd, ctrl);
		break;

	case V4L2_CID_CAMERA_METERING:
		err = m7mu_set_metering(sd, ctrl->value);
		break;

	case V4L2_CID_CAM_BRIGHTNESS:
		err = m7mu_set_exposure(sd, ctrl);
		break;

	case V4L2_CID_CAMERA_SHARPNESS:
		err = m7mu_set_sharpness(sd, ctrl);
		break;

	case V4L2_CID_CAMERA_CONTRAST:
		err = m7mu_set_contrast(sd, ctrl);
		break;

	case V4L2_CID_CAMERA_SATURATION:
		err = m7mu_set_saturation(sd, ctrl);
		break;

	case V4L2_CID_WHITE_BALANCE_PRESET:
		err = m7mu_set_whitebalance(sd, ctrl->value);
		break;

	case V4L2_CID_CAMERA_COLOR_ADJUST:
		err = m7mu_set_coloradjust(sd, ctrl->value);
		break;

	case V4L2_CID_CAMERA_SCENE_MODE:
		err = m7mu_set_scene_mode(sd, ctrl->value);
		break;

	case V4L2_CID_CAMERA_EFFECT:
		err = m7mu_set_effect(sd, ctrl->value);
		break;

	case V4L2_CID_CAMERA_WDR:
		err = m7mu_set_wdr(sd, ctrl->value);
		break;

	case V4L2_CID_CAMERA_ANTI_SHAKE:
		err = m7mu_set_antishake(sd, ctrl->value);
		break;

	case V4L2_CID_CAMERA_BEAUTY_SHOT:
		err = m7mu_set_face_beauty(sd, ctrl->value);
		break;

	case V4L2_CID_FOCUS_MODE:
		err = m7mu_set_af_mode(sd, ctrl->value);
		break;

	case V4L2_CID_CAMERA_SET_AUTO_FOCUS:
		err = m7mu_set_af(sd, ctrl->value);
		break;

	case V4L2_CID_CAMERA_MF:
		cam_trace("V4L2_CID_CAMERA_MF : 0x%x\n", ctrl->value);
		err = m7mu_writeb(client, M7MU_CATEGORY_LENS,
			M7MU_LENS_MF_RUN, ctrl->value);
		CHECK_ERR(err);
		break;

	case V4L2_CID_CAMERA_OBJECT_POSITION_X:
		state->focus.pos_x = ctrl->value;
		#if 0
		/* FIXME - It should be fixed on F/W (touch AF offset) */
		if (state->preview != NULL) {
			if (state->exif.unique_id[0] == 'T') {
				if (state->preview->index == M7MU_PREVIEW_VGA)
					state->focus.pos_x -= 40;
				else if (state->preview->index ==
						M7MU_PREVIEW_WVGA)
					state->focus.pos_x -= 50;
			}
		}
		#endif
		break;

	case V4L2_CID_CAMERA_OBJECT_POSITION_Y:
		state->focus.pos_y = ctrl->value;
		#if 0
		/* FIXME - It should be fixed on F/W (touch AF offset) */
		if (state->preview != NULL) {
			if (state->preview->index == M7MU_PREVIEW_VGA) {
				if (state->exif.unique_id[0] == 'T')
					state->focus.pos_y -= 50;
			} else if (state->preview->index == M7MU_PREVIEW_WVGA) {
				if (state->exif.unique_id[0] == 'T')
					state->focus.pos_y -= 2;
				else
					state->focus.pos_y += 60;
			}
		}
		#endif
		break;

	case V4L2_CID_CAMERA_TOUCH_AF_START_STOP:
		err = m7mu_set_touch_auto_focus(sd, ctrl->value);
		break;

	case V4L2_CID_CAMERA_AF_LED:
		err = m7mu_set_AF_LED(sd, ctrl->value);
		break;

	case V4L2_CID_CAMERA_TURN_AF_LED:
		err = m7mu_set_Turn_AF_LED(sd, ctrl->value);
		break;

	case V4L2_CID_CAMERA_TIMER_LED:
		err = m7mu_set_timer_LED(sd, ctrl->value);
		break;

	case V4L2_CID_CAMERA_TIMER_MODE:
		err = m7mu_set_timer_Mode(sd, ctrl->value);
		break;

	case V4L2_CID_ZOOM_ACTION_METHOD:
		err = m7mu_set_zoom_action_method(sd, ctrl->value);
		break;

	case V4L2_CID_CAMERA_ZOOM:
		err = m7mu_set_zoom(sd, ctrl);
		break;

	case V4L2_CID_CAMERA_DZOOM:
		err = m7mu_set_dzoom(sd, ctrl);
		break;

	case V4L2_CID_ZOOM_SPEED:
		err = m7mu_set_zoom_speed(sd, ctrl->value);
		break;

	case V4L2_CID_CAMERA_OPTICAL_ZOOM_CTRL:
		err = m7mu_set_zoom_ctrl(sd, ctrl->value);
		break;

	case V4L2_CID_JPEG_QUALITY:
		err = m7mu_set_jpeg_quality(sd, ctrl);
		break;

	case V4L2_CID_CAPTURE:
		err = m7mu_set_oprmode_mode(sd, ctrl->value);
		break;

	case V4L2_CID_CAMERA_TRANSFER:
		if (state->start_cap_kind == START_CAPTURE_JPEG_MAIN) {
			err = m7mu_start_capture(sd, ctrl->value);
		} else if (state->start_cap_kind == START_CAPTURE_YUV_MAIN) {
			cam_dbg("@@@ YUV_CAPTURE :: state->factory_test_num %d\n",
					state->factory_test_num);
			if ((state->factory_test_num == FACTORY_RESOL_WIDE_INSIDE)
				|| (state->factory_test_num == FACTORY_RESOL_TELE_INSIDE)
				|| (state->factory_test_num == FACTORY_TILT_TEST_INSIDE)
				|| (state->factory_test_num == FACTORY_DUST_TEST_INSIDE)
				|| (state->factory_test_num == FACTORY_LENS_SHADING_TEST_INSIDE)
				|| (state->factory_test_num == FACTORY_NOISE_TEST_INSIDE)
				|| (state->factory_test_num == FACTORY_REDIMAGE_TEST_INSIDE)
				) {
				err = m7mu_start_YUV_one_capture(sd,
						ctrl->value);
			} else {
				err = m7mu_start_YUV_capture(sd, ctrl->value);
			}
		} else if (state->start_cap_kind == START_CAPTURE_COMBINED) {
			cam_info("Transfer combined data.");
			err = m7mu_start_combined_capture(sd, ctrl->value);
		}
		break;

	case V4L2_CID_CAMERA_CAPTURE_THUMB:
		err = m7mu_start_capture_thumb(sd, ctrl->value);
		break;

	case V4L2_CID_CAMERA_POSTVIEW_TRANSFER:
		err = m7mu_start_postview_capture(sd, ctrl->value);
		break;

	case V4L2_CID_CAMERA_CAPTURE_MODE:
		err = m7mu_set_capture_mode(sd, ctrl->value);
		break;

	/*case V4L2_CID_CAMERA_HDR:
		err = m7mu_set_hdr(sd, ctrl->value);
		break;*/

	case V4L2_CID_CAMERA_VT_MODE:
		state->vt_mode = ctrl->value;
		break;

	case V4L2_CID_CAMERA_SAMSUNG_APP:
		state->samsung_app = ctrl->value;
		cam_dbg("samsung_app %d\n", state->samsung_app);
		break;

	case V4L2_CID_CAMERA_CHECK_DATALINE:
		state->check_dataline = ctrl->value;
		break;

	case V4L2_CID_CAMERA_ANTI_BANDING:
		err = m7mu_set_antibanding(sd, ctrl);
		break;

	case V4L2_CID_CAMERA_CHECK_ESD:
		err = m7mu_check_esd(sd);
		break;

	case V4L2_CID_CAMERA_AEAWB_LOCK_UNLOCK:
		err = m7mu_set_aeawblock(sd, ctrl->value);
		break;

	case V4L2_CID_CAMERA_AE_LOCK_UNLOCK:
		err = m7mu_set_aelock(sd, ctrl->value);
		break;

	case V4L2_CID_CAM_FACE_DETECTION:
		err = m7mu_set_facedetect(sd, ctrl->value);
		break;

	case V4L2_CID_CAMERA_BRACKET:
		err = m7mu_set_bracket(sd, ctrl->value);
		break;

	case V4L2_CID_CAMERA_BRACKET_AEB:
		err = m7mu_set_bracket_aeb(sd, ctrl->value);
		break;

	case V4L2_CID_CAMERA_BRACKET_WBB:
		err = m7mu_set_bracket_wbb(sd, ctrl->value);
		break;

	case V4L2_CID_CAMERA_IMAGE_STABILIZER:
		err = m7mu_set_image_stabilizer_mode(sd, ctrl->value);
		break;

	case V4L2_CID_CAMERA_IS_OIS_MODE:
		err = m7mu_set_image_stabilizer_OIS(sd, ctrl->value);
		break;

	case V4L2_CID_CAMERA_FOCUS_AREA_MODE:
		err = m7mu_set_focus_area_mode(sd, ctrl->value);
		break;

	case V4L2_CID_CAMERA_OBJ_TRACKING_START_STOP:
		err = m7mu_set_object_tracking(sd, ctrl->value);
		break;

	case V4L2_CID_CAM_FRAME_RATE:
		err = m7mu_set_fps(sd, ctrl->value);
		break;

	case V4L2_CID_CAMERA_SMART_ZOOM:
		err = m7mu_set_smart_zoom(sd, ctrl->value);
		break;

	case V4L2_CID_CAMERA_LDC:
		err = m7mu_set_LDC(sd, ctrl->value);
		break;

	case V4L2_CID_CAMERA_LSC:
		err = m7mu_set_LSC(sd, ctrl->value);
		break;

	case V4L2_CID_CAMERA_WIDGET_MODE_LEVEL:
		err = m7mu_set_widget_mode_level(sd, ctrl->value);
		break;

	case V4L2_CID_CAMERA_PREVIEW_WIDTH:
		state->preview_width = ctrl->value;
		break;

	case V4L2_CID_CAMERA_PREVIEW_HEIGHT:
		state->preview_height = ctrl->value;
		break;

	case V4L2_CID_CAMERA_PREVIEW_SIZE:
		err = m7mu_set_mon_size(sd, ctrl->value);
		break;

	case V4L2_CID_CAM_APERTURE_PREVIEW:
		err = m7mu_set_aperture_preview(sd, ctrl->value);
		break;

	case V4L2_CID_CAM_APERTURE_CAPTURE:
		err = m7mu_set_aperture_capture(sd, ctrl->value);
		break;

	case V4L2_CID_CAM_SINGLE_AUTO_FOCUS:
		err = m7mu_set_af(sd, ctrl->value);
		break;

	case V4L2_CID_CAMERA_SYSTEM_RESET_CWB:
		err = m7mu_set_reset_cwb(sd, ctrl->value);
		break;

	case V4L2_CID_CAMERA_POWER_OFF:
		cam_info("Forced wakeup sound interrupt for power off. ctrl->value = %d",
				ctrl->value);
		if (ctrl->value == 1) {
			state->isp.sound_issued = 1;
			wake_up(&state->isp.sound_wait);
		} else if (ctrl->value == 2) {
			esdreset = 1;
		}
		break;

	/* for NSM Mode */
	case V4L2_CID_CAMERA_NSM_SYSTEM:
		err = m7mu_set_Nsm_system(sd, ctrl->value);
		break;

	case V4L2_CID_CAMERA_NSM_STATE:
		err = m7mu_set_Nsm_state(sd, ctrl->value);
		break;

	case V4L2_CID_CAMERA_NSM_RESET:
		err = m7mu_set_Nsm_reset(sd, ctrl->value);
		break;

	case V4l2_CID_CAMERA_NSM_COMMAND:
		err = m7mu_set_Nsm_command(sd, ctrl->value);
		break;

	case V4L2_CID_CAMERA_NSM_RGB:
		err = m7mu_set_Nsm_RGB(sd, ctrl->value);
		break;

	case V4L2_CID_CAMERA_NSM_CONTSHARP:
		err = m7mu_set_Nsm_ContSharp(sd, ctrl->value);
		break;

	case V4L2_CID_CAMERA_NSM_HUE_ALLREDORANGE:
		err = m7mu_set_Nsm_Hue_AllRedOrange(sd, ctrl->value);
		break;

	case V4L2_CID_CAMERA_NSM_HUE_YELLOWGREENCYAN:
		err = m7mu_set_Nsm_Hue_YellowGreenCyan(sd, ctrl->value);
		break;

	case V4L2_CID_CAMERA_NSM_HUE_BLUEVIOLETPURPLE:
		err = m7mu_set_Nsm_BlueVioletPurple(sd, ctrl->value);
		break;

	case V4L2_CID_CAMERA_NSM_SAT_ALLREDORANGE:
		err = m7mu_set_Nsm_Sat_AllRedOrange(sd, ctrl->value);
		break;

	case V4L2_CID_CAMERA_NSM_SAT_YELLOWGREENCYAN:
		err = m7mu_set_Nsm_Sat_YellowGreenCyan(sd, ctrl->value);
		break;

	case V4L2_CID_CAMERA_NSM_SAT_BLUEVIOLETPURPLE:
		err = m7mu_set_Nsm_Sat_BlueVioletPurple(sd, ctrl->value);
		break;

	case V4L2_CID_CAMERA_NSM_R:
		err = m7mu_set_Nsm_r(sd, ctrl->value);
		break;

	case V4L2_CID_CAMERA_NSM_G:
		err = m7mu_set_Nsm_g(sd, ctrl->value);
		break;

	case V4L2_CID_CAMERA_NSM_B:
		err = m7mu_set_Nsm_b(sd, ctrl->value);
		break;

	case V4L2_CID_CAMERA_NSM_GLOBAL_CONTRAST:
		err = m7mu_set_Nsm_global_contrast(sd, ctrl->value);
		break;

	case V4L2_CID_CAMERA_NSM_GLOBAL_SHARPNESS:
		err = m7mu_set_Nsm_sharpness(sd, ctrl->value);
		break;

	case V4L2_CID_CAMERA_NSM_HUE_ALL:
		err = m7mu_set_Nsm_hue_all(sd, ctrl->value);
		break;

	case V4L2_CID_CAMERA_NSM_HUE_RED:
		err = m7mu_set_Nsm_hue_red(sd, ctrl->value);
		break;

	case V4L2_CID_CAMERA_NSM_HUE_ORANGE:
		err = m7mu_set_Nsm_hue_orange(sd, ctrl->value);
		break;

	case V4L2_CID_CAMERA_NSM_HUE_YELLOW:
		err = m7mu_set_Nsm_hue_yellow(sd, ctrl->value);
		break;

	case V4L2_CID_CAMERA_NSM_HUE_GREEN:
		err = m7mu_set_Nsm_hue_green(sd, ctrl->value);
		break;

	case V4L2_CID_CAMERA_NSM_HUE_CYAN:
		err = m7mu_set_Nsm_hue_cyan(sd, ctrl->value);
		break;

	case V4L2_CID_CAMERA_NSM_HUE_BLUE:
		err = m7mu_set_Nsm_hue_blue(sd, ctrl->value);
		break;

	case V4L2_CID_CAMERA_NSM_HUE_VIOLET:
		err = m7mu_set_Nsm_hue_violet(sd, ctrl->value);
		break;

	case V4L2_CID_CAMERA_NSM_HUE_PURPLE:
		err = m7mu_set_Nsm_hue_purple(sd, ctrl->value);
		break;

	case V4L2_CID_CAMERA_NSM_SAT_ALL:
		err = m7mu_set_Nsm_sat_all(sd, ctrl->value);
		break;

	case V4L2_CID_CAMERA_NSM_SAT_RED:
		err = m7mu_set_Nsm_sat_red(sd, ctrl->value);
		break;

	case V4L2_CID_CAMERA_NSM_SAT_ORANGE:
		err = m7mu_set_Nsm_sat_orange(sd, ctrl->value);
		break;

	case V4L2_CID_CAMERA_NSM_SAT_YELLOW:
		err = m7mu_set_Nsm_sat_yellow(sd, ctrl->value);
		break;

	case V4L2_CID_CAMERA_NSM_SAT_GREEN:
		err = m7mu_set_Nsm_sat_green(sd, ctrl->value);
		break;

	case V4L2_CID_CAMERA_NSM_SAT_CYAN:
		err = m7mu_set_Nsm_sat_cyan(sd, ctrl->value);
		break;

	case V4L2_CID_CAMERA_NSM_SAT_BLUE:
		err = m7mu_set_Nsm_sat_blue(sd, ctrl->value);
		break;

	case V4L2_CID_CAMERA_NSM_SAT_VIOLET:
		err = m7mu_set_Nsm_sat_violet(sd, ctrl->value);
		break;

	case V4L2_CID_CAMERA_NSM_SAT_PURPLE:
		err = m7mu_set_Nsm_sat_purple(sd, ctrl->value);
		break;

	case V4L2_CID_CAMERA_NSM_FD_WRITE:
		err = m7mu_set_Nsm_fd_write(sd, ctrl->value);
		break;

	case V4l2_CID_CAMERA_NSM_FILMID:
		err = m7mu_set_Nsm_FilmID(sd, ctrl->value);
		break;

	case V4l2_CID_CAMERA_NSM_LANGNUM:
		err = m7mu_set_Nsm_langnum(sd, ctrl->value);
		break;
	/* end NSM Mode */

	case V4L2_CID_CAMERA_FACTORY_OIS:
		err = m7mu_set_factory_OIS(sd, ctrl->value);
		break;

	case V4L2_CID_CAMERA_FACTORY_OIS_SHIFT:
		err = m7mu_set_factory_OIS_shift(sd, ctrl->value);
		break;

	case V4L2_CID_CAMERA_FACTORY_PUNT:
		err = m7mu_set_factory_punt(sd, ctrl->value);
		break;

	case V4L2_CID_CAMERA_FACTORY_PUNT_SHORT_SCAN_DATA:
		cam_trace("FACTORY_PUNT_SHORT_SCAN_DATA : 0x%x\n", ctrl->value);
		err = m7mu_writeb(client, M7MU_CATEGORY_LENS,
			0x1B, (unsigned char)(ctrl->value));
		CHECK_ERR(err);

		cam_trace("~ FACTORY_PUNT_SHORT_SCAN_START ~\n");
		err = m7mu_writeb(client, M7MU_CATEGORY_LENS,
			0x0E, 0x01);
		CHECK_ERR(err);
		break;

	case V4L2_CID_CAMERA_FACTORY_PUNT_LONG_SCAN_DATA:
		cam_trace("FACTORY_PUNT_LONG_SCAN_DATA : 0x%x\n", ctrl->value);
		err = m7mu_writeb(client, M7MU_CATEGORY_LENS,
			0x1B, (unsigned char)(ctrl->value));
		CHECK_ERR(err);

		cam_trace("~ FACTORY_PUNT_LONG_SCAN_START ~\n");
		err = m7mu_writeb(client, M7MU_CATEGORY_LENS,
			0x0E, 0x02);
		CHECK_ERR(err);
		break;

	case V4L2_CID_CAMERA_FACTORY_PUNT_INTERPOLATION:
		cam_trace("~ FACTORY_PUNT_INTERPOLATION ~\n");
		err = m7mu_writeb(client, M7MU_CATEGORY_LENS,
			0x1B, ctrl->value);
		CHECK_ERR(err);
		err = m7mu_writeb(client, M7MU_CATEGORY_LENS,
			0x0E, 0x0A);
		CHECK_ERR(err);
		err = m7mu_writeb(client, M7MU_CATEGORY_LENS,
			0x0E, 0x0B);
		CHECK_ERR(err);
		break;

	case V4L2_CID_CAMERA_FACTORY_ZOOM:
		err = m7mu_set_factory_zoom(sd, ctrl->value);
		break;

	case V4L2_CID_CAMERA_FACTORY_ZOOM_STEP:
		err = m7mu_set_factory_zoom_step(sd, ctrl->value);
		break;

	case V4L2_CID_CAMERA_FACTORY_PUNT_RANGE_DATA_MIN:
		state->f_punt_data.min = ctrl->value;
		break;

	case V4L2_CID_CAMERA_FACTORY_PUNT_RANGE_DATA_MAX:
		state->f_punt_data.max = ctrl->value;
		break;

	case V4L2_CID_CAMERA_FACTORY_PUNT_RANGE_DATA_NUM:
		state->f_punt_data.num = ctrl->value;
		break;

	case V4L2_CID_CAMERA_FACTORY_FAIL_STOP:
		err = m7mu_set_factory_fail_stop(sd, ctrl->value);
		break;

	case V4L2_CID_CAMERA_FACTORY_NODEFOCUS:
		err = m7mu_set_factory_nodefocus(sd, ctrl->value);
		break;

	case V4L2_CID_CAMERA_FACTORY_INTERPOLATION:
		err = m7mu_set_factory_interpolation(sd, ctrl->value);
		break;

	case V4L2_CID_CAMERA_FACTORY_COMMON:
		err = m7mu_set_factory_common(sd, ctrl->value);
		break;

	case V4L2_CID_CAMERA_FACTORY_VIB:
		err = m7mu_set_factory_vib(sd, ctrl->value);
		break;

	case V4L2_CID_CAMERA_FACTORY_GYRO:
		err = m7mu_set_factory_gyro(sd, ctrl->value);
		break;

	case V4L2_CID_CAMERA_FACTORY_BACKLASH:
		err = m7mu_set_factory_backlash(sd, ctrl->value);
		break;

	case V4L2_CID_CAMERA_FACTORY_BACKLASH_COUNT:
		err = m7mu_set_factory_backlash_count(sd, ctrl->value);
		break;

	case V4L2_CID_CAMERA_FACTORY_BACKLASH_MAXTHRESHOLD:
		err = m7mu_writew(client, M7MU_CATEGORY_LENS,
			0x1A, ctrl->value);
		CHECK_ERR(err);
		break;

	case V4L2_CID_CAMERA_FACTORY_ZOOM_RANGE_CHECK_DATA_MIN:
		state->f_zoom_data.range_min = ctrl->value;
		break;

	case V4L2_CID_CAMERA_FACTORY_ZOOM_RANGE_CHECK_DATA_MAX:
		state->f_zoom_data.range_max = ctrl->value;
		break;

	case V4L2_CID_CAMERA_FACTORY_ZOOM_SLOPE_CHECK_DATA_MIN:
		state->f_zoom_data.slope_min = ctrl->value;
		break;

	case V4L2_CID_CAMERA_FACTORY_ZOOM_SLOPE_CHECK_DATA_MAX:
		state->f_zoom_data.slope_max = ctrl->value;
		break;

	case V4L2_CID_CAMERA_FACTORY_AF:
		err = m7mu_set_factory_af(sd, ctrl->value);
		break;

	case V4L2_CID_CAMERA_FACTORY_AF_STEP_SET:
		err = m7mu_set_factory_af_step(sd, ctrl->value);
		break;

	case V4L2_CID_CAMERA_FACTORY_AF_POSITION:
		err = m7mu_set_factory_af_position(sd, ctrl->value);
		break;

	case V4L2_CID_CAMERA_FACTORY_DEFOCUS:
		err = m7mu_set_factory_defocus(sd, ctrl->value);
		break;

	case V4L2_CID_CAMERA_FACTORY_DEFOCUS_WIDE:
		err = m7mu_set_factory_defocus_wide(sd, ctrl->value);
		break;

	case V4L2_CID_CAMERA_FACTORY_DEFOCUS_TELE:
		err = m7mu_set_factory_defocus_tele(sd, ctrl->value);
		break;

	case V4L2_CID_CAMERA_FACTORY_RESOL_CAP:
		err = m7mu_set_factory_resol_cap(sd, ctrl->value);
		break;
	case V4L2_CID_CAMERA_SET_G_VALUE:
		state->wb_g_value = ctrl->value;
		break;

	case V4L2_CID_CAMERA_SET_B_VALUE:
		state->wb_b_value = ctrl->value;
		break;

	case V4L2_CID_CAMERA_SET_A_VALUE:
		state->wb_a_value = ctrl->value;
		break;

	case V4L2_CID_CAMERA_SET_M_VALUE:
		state->wb_m_value = ctrl->value;
		break;

	case V4L2_CID_CAMERA_SET_GBAM:
		err = m7mu_set_GBAM(sd, ctrl->value);
		break;

	case V4L2_CID_CAMERA_SET_K_VALUE:
		err = m7mu_set_K(sd, ctrl->value);
		break;

	case V4L2_CID_CAMERA_SET_FLASH_EVC_STEP:
		err = m7mu_set_flash_evc_step(sd, ctrl->value);
		break;

	case V4L2_CID_CAMERA_FLASH_BATT_INFO:
		err = m7mu_set_flash_batt_info(sd, ctrl->value);
		break;

	case V4L2_CID_CAMERA_FACTORY_OIS_RANGE_DATA_X_MIN:
		cam_trace("==========Range X min Data : 0x%x, %d\n",
			(short)ctrl->value, (short)ctrl->value);
		err = m7mu_writew(client, M7MU_CATEGORY_NEW,
			0x21, (short)(ctrl->value));
		CHECK_ERR(err);
		break;

	case V4L2_CID_CAMERA_FACTORY_VIB_RANGE_DATA_X_MIN:
	case V4L2_CID_CAMERA_FACTORY_GYRO_RANGE_DATA_X_MIN:
		cam_trace("==========Range X min Data : 0x%x, %d\n",
			(u16)ctrl->value, (u16)ctrl->value);
		err = m7mu_writew(client, M7MU_CATEGORY_NEW,
			0x21, (short)(ctrl->value));
		CHECK_ERR(err);
		break;

	case V4L2_CID_CAMERA_FACTORY_OIS_RANGE_DATA_X_MAX:
		cam_trace("==========Range X max Data : 0x%x, %d\n",
			(short)ctrl->value, (short)ctrl->value);
		err = m7mu_writew(client, M7MU_CATEGORY_NEW,
			0x23, (short)(ctrl->value));
		CHECK_ERR(err);
		break;

	case V4L2_CID_CAMERA_FACTORY_VIB_RANGE_DATA_X_MAX:
	case V4L2_CID_CAMERA_FACTORY_GYRO_RANGE_DATA_X_MAX:
		cam_trace("==========Range X max Data : 0x%x, %d\n",
			(u16)ctrl->value, (u16)ctrl->value);
		err = m7mu_writew(client, M7MU_CATEGORY_NEW,
			0x23, (u16)(ctrl->value));
		CHECK_ERR(err);
		break;

	case V4L2_CID_CAMERA_FACTORY_OIS_RANGE_DATA_Y_MIN:
		err = m7mu_writew(client, M7MU_CATEGORY_NEW,
			0x25, (short)(ctrl->value));
		CHECK_ERR(err);
		break;

	case V4L2_CID_CAMERA_FACTORY_VIB_RANGE_DATA_Y_MIN:
	case V4L2_CID_CAMERA_FACTORY_GYRO_RANGE_DATA_Y_MIN:
		err = m7mu_writew(client, M7MU_CATEGORY_NEW,
			0x25, (u16)(ctrl->value));
		CHECK_ERR(err);
		break;

	case V4L2_CID_CAMERA_FACTORY_OIS_RANGE_DATA_Y_MAX:
		err = m7mu_writew(client, M7MU_CATEGORY_NEW,
			0x27, (short)(ctrl->value));
		CHECK_ERR(err);
		break;

	case V4L2_CID_CAMERA_FACTORY_VIB_RANGE_DATA_Y_MAX:
	case V4L2_CID_CAMERA_FACTORY_GYRO_RANGE_DATA_Y_MAX:
		err = m7mu_writew(client, M7MU_CATEGORY_NEW,
			0x27, (u16)(ctrl->value));
		CHECK_ERR(err);
		break;

	case V4L2_CID_CAMERA_FACTORY_OIS_RANGE_DATA_X_GAIN:
		err = m7mu_writew(client, M7MU_CATEGORY_NEW,
			0x29, (short)(ctrl->value));
		CHECK_ERR(err);
		break;

	case V4L2_CID_CAMERA_FACTORY_VIB_RANGE_DATA_PEAK_X:
		err = m7mu_writew(client, M7MU_CATEGORY_NEW,
			0x29, (u16)(ctrl->value));
		CHECK_ERR(err);
		break;

	case V4L2_CID_CAMERA_FACTORY_OIS_RANGE_DATA_PEAK_X:
		err = m7mu_writew(client, M7MU_CATEGORY_NEW,
			0x2B, (short)(ctrl->value));
		CHECK_ERR(err);
		break;

	case V4L2_CID_CAMERA_FACTORY_VIB_RANGE_DATA_PEAK_Y:
		err = m7mu_writew(client, M7MU_CATEGORY_NEW,
			0x2B, (u16)(ctrl->value));
		CHECK_ERR(err);
		break;

	case V4L2_CID_CAMERA_FACTORY_OIS_RANGE_DATA_PEAK_Y:
		err = m7mu_writew(client, M7MU_CATEGORY_NEW,
			0x2D, (u16)(ctrl->value));
		CHECK_ERR(err);
		break;

	case V4L2_CID_CAMERA_FACTORY_TEST_NUMBER:
		cam_trace("==========FACTORY_TEST_NUMBER : 0x%x\n",
			ctrl->value);

		err = m7mu_writeb(client, M7MU_CATEGORY_CAPCTRL,
				M7MU_CAPCTRL_AUTOMATIC_SHIFT_EN,
				(ctrl->value ? 0x00 : 0x01));
		CHECK_ERR(err);

		err = m7mu_writeb(client, M7MU_CATEGORY_LENS,
			0x41, ctrl->value);
		state->factory_test_num = ctrl->value;
		CHECK_ERR(err);

		/* AF LED on/off */
		if (ctrl->value == 120)
			m7mu_set_factory_af_led_onoff(sd, true);
		else
			m7mu_set_factory_af_led_onoff(sd, false);

		break;
	case V4L2_CID_CAMERA_FACTORY_LOG_WRITE_ALL:
		cam_trace("FACTORY_LOG_WRITE_ALL : 0x%x\n",
			ctrl->value);
		state->factory_log_write = ctrl->value;
		break;

	case V4L2_CID_CAMERA_FACTORY_NO_LENS_OFF:
		cam_trace("FACTORY_NO_LENS_OFF : 0x%x\n",
			ctrl->value);
		state->factory_no_lens_off = ctrl->value;
		break;

	case V4L2_CID_CAMERA_FACTORY_FOCUS_CLOSEOPEN:
		if (ctrl->value == 0) {
			err = m7mu_writeb(client, M7MU_CATEGORY_LENS,
					M7MU_LENS_AF_FOCUS_ADJ, 0x02);
			CHECK_ERR(err);
		} else if (ctrl->value == 1) {
			err = m7mu_writeb(client, M7MU_CATEGORY_LENS,
					M7MU_LENS_AF_FOCUS_ADJ, 0x00);
			CHECK_ERR(err);
		} else
			cam_trace(
				"error/ FACTORY_FOCUS_CLOSEOPENCHECK : 0x%x\n",
				ctrl->value);
		break;

	case V4L2_CID_SET_CONTINUE_FPS:
		state->continueFps = ctrl->value;
		break;

	case V4L2_CID_CONTINUESHOT_PROC:
		err = m7mu_continue_proc(sd, ctrl->value);
		break;

	case V4L2_CID_BURSTSHOT_PROC:
		err = m7mu_burst_proc(sd, ctrl->value);
		break;

	case V4L2_CID_CAMERA_CAPTURE_CNT:
		err = m7mu_set_capture_cnt(sd, ctrl->value);
		break;

	case V4L2_CID_CAMERA_SET_POSTVIEW_SIZE:
		err = m7mu_burst_set_postview_size(sd, ctrl->value);
		break;

	case V4L2_CID_BURSTSHOT_SET_SNAPSHOT_SIZE:
		err = m7mu_burst_set_snapshot_size(sd, ctrl->value);
		break;

#if 0
	case V4L2_CID_CAMERA_DUAL_POSTVIEW:
		err = m7mu_start_dual_postview(sd, ctrl->value);
		break;

	case V4L2_CID_CAMERA_DUAL_CAPTURE:
		err = m7mu_start_dual_capture(sd, ctrl->value);
		break;
#endif

	case V4L2_CID_CAMERA_SET_DUAL_CAPTURE:
		err = m7mu_start_set_dual_capture(sd, ctrl->value);
		break;

	case V4L2_CID_CAMERA_DUAL_CAPTURE_MODE:
		/*err = m7mu_set_dual_capture_mode(sd, ctrl->value);*/
		break;

	case V4L2_CID_CAMERA_FACTORY_AF_SCAN_LIMIT_MIN:
	case V4L2_CID_CAMERA_FACTORY_AF_SCAN_RANGE_MIN:
		temp = (short)((ctrl->value) & 0x0000FFFF);
		cam_trace("==========Range    min Data : 0x%x, %d\n",
			temp, temp);
		err = m7mu_writew(client, M7MU_CATEGORY_LENS,
			0x18, temp);
		CHECK_ERR(err);
		break;

	case V4L2_CID_CAMERA_FACTORY_AF_SCAN_LIMIT_MAX:
	case V4L2_CID_CAMERA_FACTORY_AF_SCAN_RANGE_MAX:
		err = m7mu_writew(client, M7MU_CATEGORY_LENS,
			0x1A, (u16)(ctrl->value));
		CHECK_ERR(err);
		break;

	case V4L2_CID_CAMERA_FACTORY_AF_LENS:
		err = m7mu_set_factory_af_lens(sd, ctrl->value);
		break;

	case V4L2_CID_CAMERA_FACTORY_AF_ZONE:
		err = m7mu_set_factory_af_zone(sd, ctrl->value);
		break;

	case V4L2_CID_CAMERA_FACTORY_LV_TARGET:
		cam_trace("FACTORY_LV_TARGET : 0x%x\n", ctrl->value);
		err = m7mu_writeb(client, M7MU_CATEGORY_ADJST,
			0x52, ctrl->value);
		CHECK_ERR(err);
		break;

	case V4L2_CID_CAMERA_FACTORY_ADJ_IRIS:
		err = m7mu_set_factory_adj_iris(sd, ctrl->value);
		break;

	case V4L2_CID_CAMERA_FACTORY_ADJ_IRIS_RANGE_MIN:
		cam_trace("FACTORY_ADJ_IRIS_RANGE_MIN : 0x%x\n", ctrl->value);
		err = m7mu_writeb(client, M7MU_CATEGORY_ADJST,
			0x54, (unsigned char)(ctrl->value));
		CHECK_ERR(err);
		break;

	case V4L2_CID_CAMERA_FACTORY_ADJ_IRIS_RANGE_MAX:
		cam_trace("FACTORY_ADJ_IRIS_RANGE_MAX : 0x%x\n", ctrl->value);
		err = m7mu_writeb(client, M7MU_CATEGORY_ADJST,
			0x55, (unsigned char)(ctrl->value));
		CHECK_ERR(err);
		break;

	case V4L2_CID_CAMERA_FACTORY_ADJ_GAIN_LIVEVIEW:
		err = m7mu_set_factory_adj_gain_liveview(sd, ctrl->value);
		break;

	case V4L2_CID_CAMERA_FACTORY_LIVEVIEW_OFFSET_MARK:
		cam_trace("FACTORY_LIVEVIEW_OFFSET_MARK : 0x%x\n",
				ctrl->value);
		err = m7mu_writel(client, M7MU_CATEGORY_ADJST,
			0x3A, (u32)ctrl->value);
		CHECK_ERR(err);
		break;

	case V4L2_CID_CAMERA_FACTORY_LIVEVIEW_OFFSET_VAL:
		cam_trace("FACTORY_LIVEVIEW_OFFSET_VAL : 0x%x\n",
				ctrl->value);
		err = m7mu_writeb(client, M7MU_CATEGORY_ADJST,
			0x3E, (u16)ctrl->value);
		CHECK_ERR(err);
		break;

	case V4L2_CID_CAMERA_FACTORY_ADJ_GAIN_LIVEVIEW_RANGE_MIN:
		cam_trace("FACTORY_ADJ_GAIN_LIVEVIEW_RANGE_MIN : 0x%x\n",
				ctrl->value);
		err = m7mu_writew(client, M7MU_CATEGORY_ADJST,
			0x56, (u16)(ctrl->value));
		CHECK_ERR(err);
		break;

	case V4L2_CID_CAMERA_FACTORY_ADJ_GAIN_LIVEVIEW_RANGE_MAX:
		cam_trace("FACTORY_ADJ_GAIN_LIVEVIEW_RANGE_MAX : 0x%x\n",
				ctrl->value);
		err = m7mu_writew(client, M7MU_CATEGORY_ADJST,
			0x58, (u16)(ctrl->value));
		CHECK_ERR(err);
		break;

	case V4L2_CID_CAMERA_FACTORY_SH_CLOSE:
		err = m7mu_set_factory_sh_close(sd, ctrl->value);
		break;

	case V4L2_CID_CAMERA_FACTORY_SH_CLOSE_IRIS_NUM:
		cam_trace("FACTORY_SH_CLOSE_IRIS_NUM : 0x%x\n", ctrl->value);
		err = m7mu_writeb(client, M7MU_CATEGORY_ADJST,
			0x51, ctrl->value);
		CHECK_ERR(err);
		break;

	case V4L2_CID_CAMERA_FACTORY_SH_CLOSE_SET_IRIS:
		cam_trace("FACTORY_SH_CLOSE_SET_IRIS : 0x%x\n", ctrl->value);
		err = m7mu_writeb(client, M7MU_CATEGORY_ADJST,
			0x6E, ctrl->value);
		CHECK_ERR(err);
		break;

	case V4L2_CID_CAMERA_FACTORY_SH_CLOSE_RANGE:
		cam_trace("FACTORY_SH_CLOSE_RANGE : 0x%x\n", ctrl->value);
		err = m7mu_writew(client, M7MU_CATEGORY_ADJST,
			0x5A, (u16)(ctrl->value));
		CHECK_ERR(err);
		break;

	case V4L2_CID_CAMERA_FACTORY_SH_CLOSE_ISO:
		cam_trace("FACTORY_SH_CLOSE_ISO : 0x%x\n", ctrl->value);
		err = m7mu_writew(client, M7MU_CATEGORY_AE,
			0x3B, ctrl->value);
		CHECK_ERR(err);
		break;

	case V4L2_CID_CAMERA_FACTORY_SH_CLOSE_SPEEDTIME_X:
		cam_trace("FACTORY_SH_CLOSE_SPEEDTIME_X : 0x%x\n", ctrl->value);
		err = m7mu_writew(client, M7MU_CATEGORY_AE,
			0x37, (u16)(ctrl->value));
		CHECK_ERR(err);
		break;

	case V4L2_CID_CAMERA_FACTORY_SH_CLOSE_SPEEDTIME_Y:
		cam_trace("FACTORY_SH_CLOSE_SPEEDTIME_Y : 0x%x\n", ctrl->value);
		err = m7mu_writew(client, M7MU_CATEGORY_AE,
			0x39, (u16)(ctrl->value));
		CHECK_ERR(err);
		break;

	case V4L2_CID_CAMERA_FACTORY_FLICKER:
		err = m7mu_set_factory_flicker(sd, ctrl->value);
		break;

	case V4L2_CID_CAMERA_FACTORY_CAPTURE_GAIN:
		err = m7mu_set_factory_capture_gain(sd, ctrl->value);
		break;

	case V4L2_CID_CAMERA_FACTORY_CAPTURE_GAIN_OFFSET_MARK:
		cam_trace("FACTORY_CAPTURE_GAIN_OFFSET_MARK : 0x%x\n",
				ctrl->value);
		err = m7mu_writel(client, M7MU_CATEGORY_ADJST,
			0x3A, (u32)ctrl->value);
		CHECK_ERR(err);
		break;

	case V4L2_CID_CAMERA_FACTORY_CAPTURE_GAIN_OFFSET_VAL:
		cam_trace("FACTORY_CAPTURE_GAIN_OFFSET_VAL : 0x%x\n",
				ctrl->value);
		err = m7mu_writeb(client, M7MU_CATEGORY_ADJST,
			0x3E, (u16)ctrl->value);
		CHECK_ERR(err);
		break;

	case V4L2_CID_CAMERA_FACTORY_CAPTURE_GAIN_RANGE_MIN:
		cam_trace("FACTORY_CAPTURE_GAIN_RANGE_MIN : 0x%x\n",
				ctrl->value);
		err = m7mu_writew(client, M7MU_CATEGORY_ADJST,
			0x56, (u16)ctrl->value);
		CHECK_ERR(err);
		break;

	case V4L2_CID_CAMERA_FACTORY_CAPTURE_GAIN_RANGE_MAX:
		cam_trace("FACTORY_CAPTURE_GAIN_RANGE_MAX : 0x%x\n",
				ctrl->value);
		err = m7mu_writew(client, M7MU_CATEGORY_ADJST,
			0x58, (u16)ctrl->value);
		CHECK_ERR(err);
		break;

	case V4L2_CID_CAMERA_FACTORY_LSC_TABLE:
		cam_trace("FACTORY_LSC_TABLE : 0x%x\n", ctrl->value);
		err = m7mu_writeb(client, M7MU_CATEGORY_ADJST,
			0x30, ctrl->value);
		CHECK_ERR(err);
		m7mu_wait_make_CSV_rawdata(sd);
		break;

	case V4L2_CID_CAMERA_FACTORY_LSC_REFERENCE:
		cam_trace("FACTORY_LSC_REFERENCE : 0x%x\n", ctrl->value);
		err = m7mu_writeb(client, M7MU_CATEGORY_ADJST,
			0x31, ctrl->value);
		CHECK_ERR(err);
		break;

	case V4L2_CID_CAMERA_FACTORY_CAPTURE_CTRL:
		err = m7mu_set_factory_capture_ctrl(sd, ctrl->value);
		break;

	case V4L2_CID_CAMERA_FACTORY_AE_TARGET:
		cam_trace("V4L2_CID_CAMERA_FACTORY_AE_TARGET : 0x%x\n",
				ctrl->value);
		err = m7mu_writeb(client, M7MU_CATEGORY_AE,
			0x02, (unsigned char)ctrl->value);
		CHECK_ERR(err);
		break;

	case V4L2_CID_CAMERA_FACTORY_FLASH:
		err = m7mu_set_factory_flash(sd, ctrl->value);
		break;

	case V4L2_CID_CAMERA_FACTORY_FLASH_CHR_CHK_TM:
		cam_trace("FLASH_CHR_CHK_TM : 0x%x\n", ctrl->value);
		err = m7mu_writew(client, M7MU_CATEGORY_CAPPARM,
			0x3B, (u16)ctrl->value);
		CHECK_ERR(err);
		break;

	case V4L2_CID_CAMERA_FACTORY_FLASH_RANGE_X:
		cam_trace("FLASH_RANGE_X : 0x%x\n", ctrl->value);
		err = m7mu_writew(client, M7MU_CATEGORY_ADJST,
			0x71, (u16)ctrl->value);
		CHECK_ERR(err);
		break;

	case V4L2_CID_CAMERA_FACTORY_FLASH_RANGE_Y:
		cam_trace("FLASH_RANGE_Y : 0x%x\n", ctrl->value);
		err = m7mu_writew(client, M7MU_CATEGORY_ADJST,
			0x73, (u16)ctrl->value);
		CHECK_ERR(err);
		break;

	case V4L2_CID_CAMERA_FACTORY_WB:
		err = m7mu_set_factory_wb(sd, ctrl->value);
		break;

	case V4L2_CID_CAMERA_FACTORY_WB_IN_RG_VALUE:
		cam_trace("WB_IN_RG_VALUE : 0x%x\n",
				ctrl->value);
		err = m7mu_writew(client, M7MU_CATEGORY_ADJST,
			0x27, (u16)ctrl->value);
		CHECK_ERR(err);
		break;

	case V4L2_CID_CAMERA_FACTORY_WB_IN_BG_VALUE:
		cam_trace("WB_IN_BG_VALUE : 0x%x\n",
				ctrl->value);
		err = m7mu_writew(client, M7MU_CATEGORY_ADJST,
			0x29, (u16)ctrl->value);
		CHECK_ERR(err);
		break;

	case V4L2_CID_CAMERA_FACTORY_WB_OUT_RG_VALUE:
		cam_trace("WB_OUT_RG_VALUE : 0x%x\n",
				ctrl->value);
		err = m7mu_writew(client, M7MU_CATEGORY_ADJST,
			0x2B, (u16)ctrl->value);
		CHECK_ERR(err);
		break;

	case V4L2_CID_CAMERA_FACTORY_WB_OUT_BG_VALUE:
		cam_trace("WB_OUT_RG_VALUE : 0x%x\n",
				ctrl->value);
		err = m7mu_writew(client, M7MU_CATEGORY_ADJST,
			0x2D, (u16)ctrl->value);
		CHECK_ERR(err);
		break;

	case V4L2_CID_CAMERA_FACTORY_WB_RANGE:
		cam_trace("WB_RANGE_PERCENT : 0x%x\n",
				ctrl->value);
		err = m7mu_writeb(client, M7MU_CATEGORY_ADJST,
			0x1F, (u8)ctrl->value);
		CHECK_ERR(err);
		break;

	case V4L2_CID_CAMERA_FACTORY_WB_RANGE_FLASH_WRITE:
		err = m7mu_writeb(client, M7MU_CATEGORY_ADJST,
			0x26, 0x01);
		CHECK_ERR(err);
		break;

	case V4L2_CID_CAMERA_FACTORY_AFLED_RANGE_DATA_START_X:
		cam_trace("AFLED_RANGE_DATA_START_X : 0x%x\n",
				ctrl->value);
		err = m7mu_writeb(client, M7MU_CATEGORY_LENS,
			0x43, (unsigned char)ctrl->value);
		CHECK_ERR(err);
		break;

	case V4L2_CID_CAMERA_FACTORY_AFLED_RANGE_DATA_END_X:
		cam_trace("AFLED_RANGE_DATA_END_X : 0x%x\n",
				ctrl->value);
		err = m7mu_writeb(client, M7MU_CATEGORY_LENS,
			0x44, (unsigned char)ctrl->value);
		CHECK_ERR(err);
		break;

	case V4L2_CID_CAMERA_FACTORY_AFLED_RANGE_DATA_START_Y:
		cam_trace("AFLED_RANGE_DATA_START_Y : 0x%x\n",
				ctrl->value);
		err = m7mu_writeb(client, M7MU_CATEGORY_LENS,
			0x45, (unsigned char)ctrl->value);
		CHECK_ERR(err);
		break;

	case V4L2_CID_CAMERA_FACTORY_AFLED_RANGE_DATA_END_Y:
		cam_trace("AFLED_RANGE_DATA_END_Y : 0x%x\n",
				ctrl->value);
		err = m7mu_writeb(client, M7MU_CATEGORY_LENS,
			0x46, (unsigned char)ctrl->value);
		CHECK_ERR(err);
		break;

	case V4L2_CID_CAMERA_FACTORY_AF_LED_TIME:
		cam_trace("AF_LED_TIME : 0x%x\n", ctrl->value);
		err = m7mu_writew(client, M7MU_CATEGORY_LENS,
			0x4B, ctrl->value);
		CHECK_ERR(err);

		err = m7mu_writeb(client, M7MU_CATEGORY_LENS,
			0x4D, 0x01);
		CHECK_ERR(err);
		break;

	case V4L2_CID_CAMERA_FACTORY_AF_DIFF_CHECK_MIN:
		cam_trace("AF_DIFF_CHECK_MIN : 0x%x\n", ctrl->value);
		err = m7mu_writew(client, M7MU_CATEGORY_LENS,
			0x18, (u16)ctrl->value);
		CHECK_ERR(err);
		break;

	case V4L2_CID_CAMERA_FACTORY_AF_DIFF_CHECK_MAX:
		cam_trace("AF_DIFF_CHECK_MAX : 0x%x\n", ctrl->value);
		err = m7mu_writew(client, M7MU_CATEGORY_LENS,
			0x1A, (u16)ctrl->value);
		CHECK_ERR(err);
		err = m7mu_writeb(client, M7MU_CATEGORY_LENS,
			0xD, 0x11);
		CHECK_ERR(err);
		break;

	case V4L2_CID_CAMERA_FACTORY_DEFECTPIXEL:
		err = m7mu_set_factory_defectpixel(sd, ctrl->value);
		break;

	case V4L2_CID_CAMERA_FACTORY_DFPX_NLV_CAP:
		cam_trace("DFPX_NLV_CAP : 0x%x\n", ctrl->value);
		err = m7mu_writew(client, M7MU_CATEGORY_PARM,
			0x70, (u16) ctrl->value);
		CHECK_ERR(err);
		break;

	case V4L2_CID_CAMERA_FACTORY_DFPX_NLV_DR1_HD:
		cam_trace("DFPX_NLV_DR1_HD : 0x%x\n", ctrl->value);
		err = m7mu_writew(client, M7MU_CATEGORY_PARM,
			0x7A, (u16)ctrl->value);
		CHECK_ERR(err);
		break;

	case V4L2_CID_CAMERA_FACTORY_DFPX_NLV_DR0:
		cam_trace("DFPX_NLV_DR0 : 0x%x\n", ctrl->value);
		err = m7mu_writew(client, M7MU_CATEGORY_PARM,
			0x76, (u16)ctrl->value);
		CHECK_ERR(err);
		break;

	case V4L2_CID_CAMERA_FACTORY_DFPX_NLV_DR1:
		cam_trace("DFPX_NLV_D1 : 0x%x\n", ctrl->value);
		err = m7mu_writew(client, M7MU_CATEGORY_PARM,
			0x72, (u16)ctrl->value);
		CHECK_ERR(err);
		break;

	case V4L2_CID_CAMERA_FACTORY_DFPX_NLV_DR2:
		cam_trace("DFPX_NLV_DR2 : 0x%x\n", ctrl->value);
		err = m7mu_writew(client, M7MU_CATEGORY_PARM,
			0x78, (u16)ctrl->value);
		CHECK_ERR(err);
		break;

	case V4L2_CID_CAMERA_FACTORY_DFPX_NLV_DR_HS:
		cam_trace("DFPX_NLV_DR_HS : 0x%x\n", ctrl->value);
		err = m7mu_writew(client, M7MU_CATEGORY_PARM,
			0x74, (u16)ctrl->value);
		CHECK_ERR(err);
		break;

	case V4L2_CID_CAMERA_FACTORY_AF_LED_LV_MIN:
		cam_trace("AF_LED_LV_MIN : 0x%x\n", ctrl->value);
		err = m7mu_writew(client, M7MU_CATEGORY_LENS,
			0x47, (u16)ctrl->value);
		CHECK_ERR(err);
		break;

	case V4L2_CID_CAMERA_FACTORY_AF_LED_LV_MAX:
		cam_trace("AF_LED_LV_MIN : 0x%x\n", ctrl->value);
		err = m7mu_writew(client, M7MU_CATEGORY_LENS,
			0x49, (u16)ctrl->value);
		CHECK_ERR(err);
		break;

	case V4L2_CID_CAMERA_FACTORY_CAM_SYS_MODE:
		err = m7mu_set_factory_cam_sys_mode(sd, ctrl->value);
		break;

	case V4L2_CID_CAMERA_PASM_MODE:
		err = m7mu_set_PASM_mode(sd, ctrl->value);
		break;

	case V4L2_CID_CAMERA_SHUTTER_SPEED:
		err = m7mu_set_shutter_speed(sd, ctrl->value);
		break;

	case V4L2_CID_CAMERA_F_NUMBER:
		err = m7mu_set_f_number(sd, ctrl->value);
		break;

	case V4L2_CID_CAMERA_WB_CUSTOM_X:
		state->wb_custom_rg = ctrl->value;
		break;

	case V4L2_CID_CAMERA_WB_CUSTOM_Y:
		state->wb_custom_bg = ctrl->value;
		break;

	case V4L2_CID_CAMERA_WB_CUSTOM_VALUE:
		err = m7mu_set_wb_custom(sd, ctrl->value);
		break;

	case V4L2_CID_CAMERA_SMART_SCENE_DETECT:
		if (ctrl->value == 1) {
			state->smart_scene_detect_mode = 1;
			err = m7mu_writeb(client,
					M7MU_CATEGORY_NEW, 0x08, 0x02);
			CHECK_ERR(err);
			err = m7mu_writeb(client,
					M7MU_CATEGORY_NEW, 0x0A, 0x01);
			CHECK_ERR(err);
		} else {
			state->smart_scene_detect_mode = 0;
			err = m7mu_writeb(client,
					M7MU_CATEGORY_NEW, 0x08, 0x00);
			CHECK_ERR(err);
			err = m7mu_writeb(client,
					M7MU_CATEGORY_NEW, 0x0A, 0x00);
			CHECK_ERR(err);
		}
		break;

	case V4L2_CID_CAMERA_SMART_MOVIE_RECORDING:
		err = m7mu_set_smart_moving_recording(sd, ctrl->value);
		break;

#if 0
	case V4L2_CID_CAMERA_SMART_AUTO_S1_PUSH:
#else
	case V4L2_CID_CAMERA_S1_PUSH:
#endif
		err = m7mu_set_smart_auto_s1_push(sd, ctrl->value);
		break;

	case V4L2_CID_CAMERA_CAF:
		err = m7mu_set_CAF(sd, ctrl->value);
		break;

	case V4L2_CID_CAMERA_FOCUS_RANGE:
		err = m7mu_set_focus_range(sd, ctrl->value);
		break;

	case V4L2_CID_CAMERA_TIME_INFO:
		err = m7mu_set_time_info(sd, ctrl->value);
		break;

	case V4L2_CID_CAMERA_LENS_TIMER:
		err = m7mu_set_lens_off_timer(sd, ctrl->value);
		break;

	case V4L2_CID_CAMERA_STREAM_PART2: /* for shutter sound */
		err = m7mu_set_mode_part2(sd, M7MU_STILLCAP_MODE);
		break;

	case V4L2_CID_CAMERA_CAPTURE_END:
		err = m7mu_set_cap_rec_end_mode(sd, ctrl->value);
		break;

	case V4L2_CID_CAMERA_FACTORY_SEND_SETTING:
		state->factory_category = (ctrl->value) / 1000;
		state->factory_byte = (ctrl->value) % 1000;
		break;

	case V4L2_CID_CAMERA_FACTORY_SEND_VALUE:
		state->factory_value_size = 1;
		state->factory_value = ctrl->value;
		m7mu_send_factory_command_value(sd);
		break;

	case V4L2_CID_CAMERA_FACTORY_SEND_WORD_VALUE:
		state->factory_value_size = 2;
		state->factory_value = ctrl->value;
		m7mu_send_factory_command_value(sd);
		break;

	case V4L2_CID_CAMERA_FACTORY_SEND_LONG_VALUE:
		state->factory_value_size = 4;
		state->factory_value = ctrl->value;
		m7mu_send_factory_command_value(sd);
		break;

	case V4L2_CID_CAMERA_FACTORY_TILT:
		cam_trace("TILT_ONE_SCRIPT_RUN : 0x%x\n", ctrl->value);
		m7mu_set_factory_tilt(sd, ctrl->value);
		break;

	case V4L2_CID_CAMERA_FACTORY_IR_CHECK:
		cam_trace("IR_CHECK : 0x%x\n", ctrl->value);
		m7mu_set_factory_IR_Check(sd, ctrl->value);
		break;

	case V4L2_CID_CAMERA_FACTORY_IR_CHECK_G_AVG:
		cam_trace("IR_CHECK_G_AVG : 0x%x\n", ctrl->value);
		m7mu_set_factory_IR_Check_GAvg(sd, ctrl->value);
		break;

	case V4L2_CID_CAMERA_FACTORY_TILT_SCAN_MIN:
		cam_trace("TILT_SCAN_MIN : 0x%x\n", ctrl->value);
		err = m7mu_writew(client, M7MU_CATEGORY_LENS,
			0x18, (u16)ctrl->value);
		CHECK_ERR(err);
		break;

	case V4L2_CID_CAMERA_FACTORY_TILT_SCAN_MAX:
		cam_trace("TILT_SCAN_MAX : 0x%x\n", ctrl->value);
		err = m7mu_writew(client, M7MU_CATEGORY_LENS,
			0x1A, (u16)ctrl->value);
		CHECK_ERR(err);
		err = m7mu_writeb(client, M7MU_CATEGORY_LENS,
			0x0D, 0x06);
		CHECK_ERR(err);
		break;

	case V4L2_CID_CAMERA_FACTORY_TILT_FIELD:
		cam_trace("TILT_FIELD : 0x%x\n", ctrl->value);
		err = m7mu_writeb(client, M7MU_CATEGORY_LENS,
			0x1B, (u8)ctrl->value);
		CHECK_ERR(err);
		err = m7mu_writeb(client, M7MU_CATEGORY_LENS,
			0x0C, 0x00);
		CHECK_ERR(err);
		break;

	case V4L2_CID_CAMERA_FACTORY_TILT_AF_RANGE_MIN:
		cam_trace("TILT_AF_RANGE_MIN : 0x%x\n", ctrl->value);
		err = m7mu_writew(client, M7MU_CATEGORY_LENS,
			0x18, (u16)ctrl->value);
		CHECK_ERR(err);
		break;

	case V4L2_CID_CAMERA_FACTORY_TILT_AF_RANGE_MAX:
		cam_trace("TILT_AF_RANGE_MAX : 0x%x\n", ctrl->value);
		err = m7mu_writew(client, M7MU_CATEGORY_LENS,
			0x1A, (u16)ctrl->value);
		CHECK_ERR(err);
		err = m7mu_writeb(client, M7MU_CATEGORY_LENS,
			0x0C, 0x01);
		CHECK_ERR(err);
		break;

	case V4L2_CID_CAMERA_FACTORY_TILT_DIFF_RANGE_MIN:
		cam_trace("TILT_DIFF_MIN : 0x%x\n", ctrl->value);
		err = m7mu_writeb(client, M7MU_CATEGORY_LENS,
			0x1A, (u8)ctrl->value);
		CHECK_ERR(err);
		break;

	case V4L2_CID_CAMERA_FACTORY_TILT_DIFF_RANGE_MAX:
		cam_trace("TILT_DIFF_MAX : 0x%x\n", ctrl->value);
		err = m7mu_writeb(client, M7MU_CATEGORY_LENS,
			0x1B, (u8)ctrl->value);
		CHECK_ERR(err);
		err = m7mu_writeb(client, M7MU_CATEGORY_LENS,
			0x0C, 0x02);
		CHECK_ERR(err);
		break;

	case V4L2_CID_CAMERA_FACTORY_IR_B_GAIN_MIN:
		cam_trace("IR_B_GAIN_MIN : 0x%x\n", ctrl->value);
		break;

	case V4L2_CID_CAMERA_FACTORY_IR_B_GAIN_MAX:
		cam_trace("IR_B_GAIN_MAX : 0x%x\n", ctrl->value);
		break;

	case V4L2_CID_CAMERA_FACTORY_IR_R_GAIN_MIN:
		cam_trace("IR_R_GAIN_MIN : 0x%x\n", ctrl->value);
		break;

	case V4L2_CID_CAMERA_FACTORY_IR_R_GAIN_MAX:
		cam_trace("IR_R_GAIN_MAX : 0x%x\n", ctrl->value);
		break;

	case V4L2_CID_CAMERA_FACTORY_FLASH_MAN_CHARGE:
		cam_trace("FLASH_MAN_CHARGE : 0x%x\n", ctrl->value);
		err = m7mu_writeb(client, M7MU_CATEGORY_CAPPARM,
			0x2A, (u8)ctrl->value);
		CHECK_ERR(err);
		break;

	case V4L2_CID_CAMERA_FACTORY_FLASH_MAN_EN:
		cam_trace("FLASH_MAN_EN : 0x%x\n", ctrl->value);
		err = m7mu_writeb(client, M7MU_CATEGORY_CAPPARM,
			0x3D, (u8)ctrl->value);
		CHECK_ERR(err);
		break;
	case V4L2_CID_CAMERA_FACTORY_MEM_COPY:
		cam_trace("V4L2_CID_CAMERA_FACTORY_MEM_COPY : 0x%x, 0x%x\n",
			(u8)ctrl->value >> 4, (u8)ctrl->value & 0x0F);
		err = m7mu_writeb(client, M7MU_CATEGORY_LENS,
			0x9B, (u8)ctrl->value >> 4);
		CHECK_ERR(err);
		err = m7mu_writeb(client, M7MU_CATEGORY_LENS,
			0x9C, (u8)ctrl->value & 0x0F);
		CHECK_ERR(err);
		err = m7mu_writeb(client, M7MU_CATEGORY_LENS,
			0x97, 0x00);
		CHECK_ERR(err);
		break;
	case V4L2_CID_CAMERA_FACTORY_MEM_MODE:
		cam_trace("V4L2_CID_CAMERA_FACTORY_MEM_MODE : 0x%x\n",
				ctrl->value);
		err = m7mu_writeb(client, M7MU_CATEGORY_LENS,
			0x98, (u8)ctrl->value);
		CHECK_ERR(err);
		break;
	case V4L2_CID_CAMERA_FACTORY_EEP_WRITE_MARK:
		cam_trace("V4L2_CID_CAMERA_FACTORY_EEP_WRITE_MARK : 0x%x\n",
				ctrl->value);
		m7mu_set_factory_eepwrite_mark(sd, ctrl->value);
		break;
	case V4L2_CID_CAMERA_FACTORY_EEP_WRITE_OIS_SHIFT:
		cam_trace(
			"FACTORY_EEP_WRITE_OIS_SHIFT : 0x%x, 0x%x, 0x%x, 0x%x\n",
			(ctrl->value & 0xff000000) >> 24,
			(ctrl->value & 0x00ff0000)>>16,
			(ctrl->value & 0x0000ff00) >> 8,
			(ctrl->value & 0x000000ff));
		err = m7mu_writeb(client, M7MU_CATEGORY_LENS,
			0xB0, (ctrl->value & 0xff000000) >> 24);
		CHECK_ERR(err);
		err = m7mu_writeb(client, M7MU_CATEGORY_LENS,
			0xB1, (ctrl->value & 0x00ff0000) >> 16);
		CHECK_ERR(err);
		err = m7mu_writeb(client, M7MU_CATEGORY_LENS,
			0xB2, (ctrl->value & 0x0000ff00) >> 8);
		CHECK_ERR(err);
		err = m7mu_writeb(client, M7MU_CATEGORY_LENS,
			0xB3, (ctrl->value & 0x000000ff));
		CHECK_ERR(err);
		err = m7mu_writeb(client, M7MU_CATEGORY_LENS,
			0xA3, 0x00);
		CHECK_ERR(err);
		break;
	case V4L2_CID_CAMERA_FACTORY_LENS_CAP:
		cam_trace("V4L2_CID_CAMERA_FACTORY_LENS_CAP : 0x%x /%d\n",
				ctrl->value, ctrl->value >> 30 & 0x00000003);
		if ((ctrl->value >> 30 & 0x00000003) == 1) {
			err = m7mu_writeb(client, M7MU_CATEGORY_ADJST,
				0x97, (ctrl->value & 0x3FFF0000) >> 16);
			CHECK_ERR(err);
			err = m7mu_writeb(client, M7MU_CATEGORY_ADJST,
				0x99, (ctrl->value & 0x0000FFFF));
			CHECK_ERR(err);
		}
		if ((ctrl->value >> 30 & 0x00000003) == 2) {
			err = m7mu_writeb(client, M7MU_CATEGORY_ADJST,
				0x98, (ctrl->value & 0x3FFF0000) >> 16);
			CHECK_ERR(err);
			err = m7mu_writeb(client, M7MU_CATEGORY_ADJST,
				0x9A, (ctrl->value & 0x0000FFFF));
			CHECK_ERR(err);
		}
		if ((ctrl->value >> 30 & 0x00000003) == 3) {
			state->lenscap_bright_min =
				((ctrl->value & 0x3FFF0000) >> 16);
			state->lenscap_bright_max = (ctrl->value & 0x0000FFFF);
			err = m7mu_writeb(client,
					M7MU_CATEGORY_ADJST, 0x96, 0x01);
			CHECK_ERR(err);
		}
		break;

	case V4L2_CID_CAMERA_FACTORY_LENS_CAP_LOG:
		cam_trace("~FACTORY_LENS_CAP_LOG~");
#ifndef READ_CSV_FILE_DIRECT
		m7mu_make_CSV_rawdata_direct(sd,
			V4L2_CID_CAMERA_FACTORY_LENS_CAP_LOG);
#endif
		break;

	case V4L2_CID_START_CAPTURE_KIND:
		cam_trace("START_CAP_KIND : 0x%x\n", ctrl->value);
		state->start_cap_kind = ctrl->value;
		break;

	case V4L2_CID_CAMERA_INIT:
		cam_trace("MANUAL INIT launched.");

		m7mu_quickshot_init(0);

		if ((m7mu_which_power_on() == 0) ||
				(m7mu_which_power_on() == 1)) {
			err = m7mu_power(sd, M7MU_HW_POWER_ON, ctrl->value);
			if (err == -EIO) {
				m7mu_power(sd, M7MU_HW_POWER_OFF, ctrl->value);
				return err;
			}
		}

		state->power_on = M7MU_HW_POWER_ON;

		err = m7mu_init(sd, ctrl->value);

		m7mu_quickshot_init(1);


		break;

	case V4L2_CID_CAMERA_POST_INIT:
		cam_trace("MANUAL OIS INIT launched.");
		err = m7mu_post_init(sd, ctrl->value);
		break;

	case V4L2_CID_CAMERA_FACTORY_DATA_ERASE:
		err = m7mu_set_factory_data_erase(sd, ctrl->value);
		break;

	case V4L2_CID_CAMERA_FACTORY_RESOLUTION_LOG:
		cam_trace("~FACTORY_RESOLUTION_LOG ~\n");
		err = m7mu_make_CSV_rawbin(sd,
				M7MU_FLASH_FACTORY_RESOLUTION, false);
		CHECK_ERR(err);
		break;

	case V4L2_CID_CAMERA_FACTORY_SHD_LOG:
		cam_trace("~V4L2_CID_CAMERA_FACTORY_SHD_LOG\n");
		err = m7mu_set_factory_shading(sd, ctrl->value);
		CHECK_ERR(err);
		break;

	case V4L2_CID_CAMERA_FACTORY_ADJUST_LOG:
		cam_trace("~FACTORY_ADJUST_LOG~\n");
		err = m7mu_set_factory_adjust_log(sd, ctrl->value);
		CHECK_ERR(err);
		break;

	case V4L2_CID_CAMERA_FACTORY_WRITE_SHD_DATA:
		cam_trace("~V4L2_CID_CAMERA_FACTORY_WRITE_SHD_DATA~\n");
		err = m7mu_shd_table_write(sd, ctrl->value);
		CHECK_ERR(err);
		break;

	case V4L2_CID_CAMERA_FACTORY_CLIP_VALUE:
		cam_trace("~V4L2_CID_CAMERA_FACTORY_CLIP_VALUE~\n");
		err = m7mu_set_clip_value(sd, ctrl->value);
		CHECK_ERR(err);
		break;

	case V4L2_CID_CAMERA_FACTORY_TILT_LOCATION:
		cam_trace("~V4L2_CID_CAMERA_FACTORY_TILT_LOCATION\n");
		err = m7mU_set_tilt_location(sd, ctrl->value);
		CHECK_ERR(err);
		break;

	case V4L2_CID_CAMERA_FAST_CAPTURE:
		cam_trace("~V4L2_CID_CAMERA_FAST_CAPTURE~\n");
		err = m7mu_set_fast_capture(sd);
		break;

	case V4L2_CID_CAMERA_SET_SNAPSHOT_SIZE:
		cam_info("V4L2_CID_CAMERA_SET_SNAPSHOT_SIZE");
		err = m7mu_set_snapshot_size(sd, (uint32_t)ctrl->value);
		break;

	case V4L2_CID_ISP_MAIN_FORMAT:
		err = m7mu_writeb(client, M7MU_CATEGORY_CAPPARM,
				M7MU_CAPPARM_YUVOUT_MAIN, ctrl->value);
		break;


	case V4L2_CID_CAMERA_SET_ROI_BOX:
	{
		int roi_x_pos = 0, roi_y_pos = 0;
		roi_x_pos = (u32)ctrl->value >> 16;
		roi_y_pos = (u32)ctrl->value & 0x0FFFF;
		cam_trace("SET_ROI_BOX, roi_x_pos[%d], roi_y_pos[%d]\n",
				roi_x_pos, roi_y_pos);
		err = m7mu_writew(client, M7MU_CATEGORY_FD,
				M7MU_FD_ROI_X_LOCATION, roi_x_pos);
		CHECK_ERR(err);

		err = m7mu_writew(client, M7MU_CATEGORY_FD,
				M7MU_FD_ROI_Y_LOCATION, roi_y_pos);
		CHECK_ERR(err);

		break;
	}

	case V4L2_CID_CAMERA_SET_ROI_BOX_WIDTH_HEIGHT:
	{
		int roi_width = 0, roi_height = 0;
		roi_width = (u32)ctrl->value >> 16;
		roi_height = (u32)ctrl->value & 0x0FFFF;
		cam_trace(
			"SET_ROI_BOX_WIDTH_HEIGHT, roi_width[%d], roi_height[%d]\n",
			roi_width, roi_height);
		err = m7mu_writew(client, M7MU_CATEGORY_FD,
				M7MU_FD_ROI_FRAME_WIDTH, roi_width);
		CHECK_ERR(err);

		err = m7mu_writew(client, M7MU_CATEGORY_FD,
				M7MU_FD_ROI_FRAME_HEIGHT, roi_height);
		CHECK_ERR(err);

		break;
	}

	case V4L2_CID_CAMERA_SET_FD_FOCUS_SELECT:
	{
		int fd_focus_select = 0;
		fd_focus_select = ctrl->value;
		cam_trace("SET_FD_FOCUS_SELECT, fd_focus_select[%d]\n",
				fd_focus_select);
		err = m7mu_writeb(client, M7MU_CATEGORY_FD,
				M7MU_FD_FOCUS_SELECT, fd_focus_select);
		CHECK_ERR(err);
		break;
	}

	case V4L2_CID_CAMERA_AE_POSITION_X:
		state->ae_touch.pos_x = ctrl->value;
		cam_trace("V4L2_CID_CAMERA_AE_POSITION_X X:%d\n",
				state->ae_touch.pos_x);
		err = m7mu_writew(client, M7MU_CATEGORY_MON,
				M7MU_MON_TOUCH_AE_WINDOW_POSITION_X,
				state->ae_touch.pos_x);
		CHECK_ERR(err);
		break;

	case V4L2_CID_CAMERA_AE_POSITION_Y:
		state->ae_touch.pos_y = ctrl->value;
		cam_trace("V4L2_CID_CAMERA_AE_POSITION_Y Y:%d\n",
				state->ae_touch.pos_y);
		err = m7mu_writew(client, M7MU_CATEGORY_MON,
				M7MU_MON_TOUCH_AE_WINDOW_POSITION_Y,
				state->ae_touch.pos_y);
		CHECK_ERR(err);
		break;

	case V4L2_CID_CAMERA_AE_WINDOW_SIZE_WIDTH:
		state->ae_touch.window_size_width = ctrl->value;
		cam_trace("V4L2_CID_CAMERA_AE_WINDOW_SIZE_WIDTH X:%d\n",
				state->ae_touch.window_size_width);
		err = m7mu_writew(client, M7MU_CATEGORY_MON,
				M7MU_MON_TOUCH_AE_WINDOW_WIDTH,
				state->ae_touch.window_size_width);
		CHECK_ERR(err);
		break;

	case V4L2_CID_CAMERA_AE_WINDOW_SIZE_HEIGHT:
		state->ae_touch.window_size_height = ctrl->value;
		cam_trace("V4L2_CID_CAMERA_AE_WINDOW_SIZE_HEIGHT Y:%d\n",
				state->ae_touch.window_size_height);
		err = m7mu_writew(client, M7MU_CATEGORY_MON,
				M7MU_MON_TOUCH_AE_WINDOW_HEIGHT,
				state->ae_touch.window_size_width);
		CHECK_ERR(err);
		break;

	case V4L2_CID_CAMERA_AE_PREVIEW_TOUCH_CTRL:
		cam_trace("V4L2_CID_CAMERA_AE_PREVIEW_TOUCH_CTRL status:%d\n",
				state->ae_touch.status);
#if 1
		if (state->ae_touch.status == ctrl->value) {
			cam_trace("1 AE_PREVIEW_TOUCH_CTRL status:%d\n",
					state->ae_touch.status);
		} else {
			state->ae_touch.status = ctrl->value;
			cam_trace("2 PREVIEW_TOUCH_CTRL status:%d\n",
					state->ae_touch.status);
			/* Start Touch AE */
			err = m7mu_writeb(client, M7MU_CATEGORY_MON,
				M7MU_MON_TOUCH_AE_START,
				state->ae_touch.status);
			CHECK_ERR(err);
		}
#else
		state->ae_touch.status = ctrl->value;
		/* Start Touch AE */
		err = m7mu_readb(client, M7MU_CATEGORY_MON,
			M7MU_MON_TOUCH_AE_START, &value);
		CHECK_ERR(err);

		cam_trace("0x2, byte 0xAE: %d\n", value);

		if (value != state->ae_touch.status) {
			err = m7mu_writeb(client, M7MU_CATEGORY_MON,
				M7MU_MON_TOUCH_AE_START, state->ae_touch.status);
			CHECK_ERR(err);
		}
#endif
		break;

	case V4L2_CID_FACTORY_DEFECT_NOISE_LEVEL_TABLE:
		cam_trace("DEFECT_NOISE_LEVEL_MODE : 0x%x\n",
				ctrl->value);
		err = m7mu_writeb(client, M7MU_CATEGORY_PARM,
				M7MU_PARAM_DEFECT_NOISE_LEVEL_TABLE,
				ctrl->value);
		CHECK_ERR(err);
		break;

	case V4L2_CID_FACTORY_DEFECT_NOISE_LEVEL_MODE:
		cam_trace("DEFECT_NOISE_LEVEL_MODE : 0x%x\n",
				ctrl->value);
		err = m7mu_writeb(client, M7MU_CATEGORY_PARM,
				M7MU_PARAM_DEFECT_NOISE_LEVEL_MODE,
				ctrl->value);
		CHECK_ERR(err);
		break;

	case V4L2_CID_FACTORY_DEFECT_NOISE_LEVEL_MIN_NUM:
		cam_trace("DEFECT_NOISE_LEVEL_MIN_NUM : 0x%x\n",
				ctrl->value);
		err = m7mu_writeb(client, M7MU_CATEGORY_PARM,
				M7MU_PARAM_DEFECT_NOISE_LEVEL_MIN_NUM,
				ctrl->value);
		CHECK_ERR(err);
		break;

	case V4L2_CID_FACTORY_DEFECT_NOISE_LEVEL_MAX_NUM:
		cam_trace("DEFECT_NOISE_LEVEL_MAX_NUM : 0x%x\n",
				ctrl->value);
		err = m7mu_writeb(client, M7MU_CATEGORY_PARM,
				M7MU_PARAM_DEFECT_NOISE_LEVEL_MAX_NUM,
				ctrl->value);
		CHECK_ERR(err);
		break;

	case V4L2_CID_FACTORY_DEFECT_NOISE_LEVEL_ISO_NUM:
		cam_trace("DEFECT_NOISE_LEVEL_ISO_NUM : 0x%x\n",
				ctrl->value);
		err = m7mu_writeb(client, M7MU_CATEGORY_PARM,
				M7MU_PARAM_DEFECT_NOISE_LEVEL_ISO_NUM,
				ctrl->value);
		CHECK_ERR(err);
		break;

	case V4L2_CID_FACTORY_DEFECT_NOISE_LEVEL_EXP_TIME:
		cam_trace("DEFECT_NOISE_LEVEL_EXP_TIME : 0x%x\n",
				ctrl->value);
		err = m7mu_writeb(client, M7MU_CATEGORY_PARM,
				M7MU_PARAM_DEFECT_NOISE_LEVEL_EXP_TIME,
				ctrl->value);
		CHECK_ERR(err);
		break;

	case V4L2_CID_FACTORY_DEFECT_NOISE_LEVEL_REPEAT_NUM:
		cam_trace("DEFECT_NOISE_LEVEL_REPEAT_NUM : 0x%x\n",
				ctrl->value);
		err = m7mu_writeb(client, M7MU_CATEGORY_PARM,
				M7MU_PARAM_DEFECT_NOISE_LEVEL_REPEAT_NUM,
				ctrl->value);
		CHECK_ERR(err);
		break;

	case V4L2_CID_FACTORY_BACKUP_MEM_DATA:
		cam_trace("V4L2_CID_FACTORY_BACKUP_MEM_DATA ~");
		err = m7mu_set_factory_backup_mem_data(sd, ctrl->value);
		CHECK_ERR(err);
		break;

	case V4L2_CID_CAMERA_SET_FAST_POWER_OFF:
		cam_trace("V4L2_CID_CAMERA_SET_FAST_POWER_OFF : %d", ctrl->value);
#ifdef ENABLE_FAST_POWER_OFF
		fast_power_off_flag = ctrl->value;
#endif
		break;

	case V4L2_CID_CAMERA_RECORDING_STATE:
		cam_trace("V4L2_CID_CAMERA_RECORDING_STATE : %d", ctrl->value);
		err = m7mu_writeb(client, M7MU_CATEGORY_MON,
				M7MU_MON_RECORDING_STATE,
				ctrl->value);
		break;

	case V4L2_CID_CAMERA_SET_PREVIEW_MIPI:
		cam_trace("V4L2_CID_CAMERA_STOP_PREVIEW_MIPI : %d", ctrl->value);
		err = m7mu_writeb(client, M7MU_CATEGORY_MON,
				M7MU_MON_MIPI_CNT, ctrl->value);
		break;
	case V4L2_CID_CAMERA_FACE_DETECT_READ_SEL:
	{
		int read_sel = 0xff;
		cam_trace("V4L2_CID_CAMERA_FACE_DETECT_READ_SEL : %d", ctrl->value);
		while(read_sel != ctrl->value) {
			err = m7mu_writeb(client, M7MU_CATEGORY_FD,
					M7MU_FD_READ_SEL, ctrl->value);
			CHECK_ERR(err);
			msleep(1);
			err = m7mu_readb(client, M7MU_CATEGORY_FD,
				M7MU_FD_READ_SEL, &read_sel);
			CHECK_ERR(err);

			if(read_sel != ctrl->value)	msleep(2);
		}
		break;
	}
	default:
		break;
	}

	if (err < 0 && err != -ENOIOCTLCMD)
		cam_err("failed, id %d, value %d\n",
				ctrl->id - grp_type, ctrl->value);
	return ((err > 0) ? 0 : err);
}

static bool m7mu_check_postview(struct v4l2_subdev *sd)
{
	struct m7mu_state *state = to_state(sd);

	if (state->start_cap_kind == START_CAPTURE_JPEG_MAIN
		|| state->running_capture_mode == START_CAPTURE_COMBINED
		|| state->running_capture_mode == RUNNING_MODE_LOWLIGHT
		|| state->running_capture_mode == RUNNING_MODE_HDR) {
		/* capture */
		cam_info("%s: JPEG or LLS or HDR capture", __func__);
		return false;
	} else {
		/* New capture condition for resolution factory test.
		   This condition is necessary if you captured
		   YUV postview + YUV main image. */
		if (state->start_cap_kind != START_CAPTURE_POSTVIEW) {
			cam_info("%s: not postview mode", __func__);
			/* capture */
			/* yuv capture */
			return false;
		}
	}
	/* postview capture*/
	cam_info("%s: postview capture mode", __func__);

	return true;
}

static int m7mu_set_size_val(struct v4l2_subdev *sd, int default_size)
{
	struct m7mu_state *state = to_state(sd);
	struct i2c_client *client = to_client(sd);
	int err;
	u32 size_val;

	cam_info("frameRate : %d", state->fps);
	cam_info("sensor_mode : %d", state->sensor_mode);
	cam_info("sensor_width : %d", state->preview->sensor_width);
	cam_info("sensor_height : %d", state->preview->sensor_height);
	cam_info("reg_val : %d", state->preview->reg_val);

	if (state->fps == 60) {
		if (state->sensor_mode == SENSOR_MOVIE)
			state->vss_mode = 1;
		if (state->preview->sensor_height == 1080)
			size_val = 0x42;
		else if (state->preview->sensor_height == 480)
			size_val = 0x2F;
		else if ((state->preview->sensor_height == 720) &&
				(state->preview->sensor_width == 960))
			size_val = 0x2F;
		else if (state->preview->sensor_height == 720)
			size_val = 0x25;
		else if ((state->preview->sensor_height == 240) &&
				(state->preview->sensor_width == 320))
			size_val = 0x36;
		else
			size_val = default_size;

		err = m7mu_writeb(client, M7MU_CATEGORY_PARM,
				M7MU_PARM_MON_SIZE, size_val);
		CHECK_ERR(err);
	} else if ((state->fps == 30) || (state->fps == 24) || (state->fps == 15)) {
		if (state->sensor_mode == SENSOR_MOVIE) {
			state->vss_mode = 1;
			if (state->preview->sensor_height == 1080)
				size_val = 0x2C;
			else if ((state->preview->sensor_height == 720) &&
					(state->preview->sensor_width == 960))
				size_val = 0x2E;
			else if (state->preview->sensor_height == 720)
				size_val = 0x2D;
			else if (state->preview->sensor_width == 320
					&& state->preview->sensor_height == 240)
				size_val = 0x36;
			else if (state->preview->sensor_width == 704 &&
					state->preview->sensor_height == 576) {
				size_val = 0x3A;
				/* VSS is not supported in 11:9 ratio */
				state->vss_mode = 0;
			} else if (state->preview->sensor_width == 1920
					&& state->preview->sensor_height == 1080
					&& state->factory_test_num ==
					FACTORY_TILT_DIVISION) {
				size_val = 0x3F;
			} else
				size_val = state->preview->reg_val;
		} else {
			state->vss_mode = 0;
			if ((state->preview->sensor_height == 1080) &&
					(state->preview->sensor_width == 1440))
				size_val = 0x37;
			else if (state->preview->sensor_height == 1080)
				size_val = 0x28;
			else if ((state->preview->sensor_height == 720) &&
					(state->preview->sensor_width == 960))
				size_val = 0x34;
			else if (state->preview->sensor_height == 720)
				size_val = 0x21;
			else if (state->preview->sensor_height == 480)
				size_val = 0x17;
			else if (state->preview->sensor_height == 240)
				size_val = 0x36;
			else if (state->preview->sensor_width == 704 &&
					state->preview->sensor_height == 576) {
				size_val = 0x3A;
			} else
				size_val = state->preview->reg_val;
		}
#if 1
		if (state->mode == MODE_PANORAMA) {
			size_val = 0x2C;
			err = m7mu_writeb(client,
					M7MU_CATEGORY_PARM,
					M7MU_PARM_MON_SIZE, size_val);
			CHECK_ERR(err);
		} else
#endif
		{
			err = m7mu_writeb(client,
					M7MU_CATEGORY_PARM,
					M7MU_PARM_MON_SIZE,
					size_val);
			CHECK_ERR(err);

			err = m7mu_writeb(client,
					M7MU_CATEGORY_PARM,
					M7MU_PARM_VSS_MODE,
					state->vss_mode);
			CHECK_ERR(err);
		}
	} else if (state->fps == 120) {
		if (state->preview->sensor_width == 1280
				&& state->preview->sensor_height == 720)
			size_val = 0x41;
		else if (state->preview->sensor_width == 768
				&& state->preview->sensor_height == 512)
			size_val = 0x33;
		else
			size_val = default_size;

		err = m7mu_writeb(client, M7MU_CATEGORY_PARM,
				M7MU_PARM_MON_SIZE, size_val);
		CHECK_ERR(err);
	} else {
#if 1
		if (state->mode == MODE_PANORAMA) {
			size_val = 0x2C;
			err = m7mu_writeb(client,
					M7MU_CATEGORY_PARM,
					M7MU_PARM_MON_SIZE, size_val);
			CHECK_ERR(err);
		} else if ((state->sensor_mode == SENSOR_MOVIE)
				&& (state->preview->sensor_width == 1280
				&& state->preview->sensor_height == 720)){
			size_val = 0x2D;
			err = m7mu_writeb(client, M7MU_CATEGORY_PARM,
					M7MU_PARM_MON_SIZE,
					size_val);
			CHECK_ERR(err);
		}else
#endif
		{
			err = m7mu_writeb(client, M7MU_CATEGORY_PARM,
					M7MU_PARM_MON_SIZE,
					state->preview->reg_val);
			CHECK_ERR(err);
		}
	}

	return err;
}

/*
 * v4l2_subdev_video_ops
 */
static int m7mu_set_frmsize(struct v4l2_subdev *sd)
{
	struct m7mu_state *state = to_state(sd);
	struct i2c_client *client = to_client(sd);
	int err;
	int value = 0;
	int read_mon_size;

	cam_trace("E\n");

	if (state->oprmode == M7MU_OPRMODE_VIDEO) {
		err = m7mu_set_mode(sd, M7MU_PARMSET_MODE);
		if (err <= 0) {
			cam_err("failed to set mode\n");
			return err;
		}

		/* don't set frmsize when returning preivew after capture */
		if (err == 10)
			cam_trace("~~~~ return when CAP->PAR ~~~~\n");
		else {
		err = m7mu_readb(client, M7MU_CATEGORY_PARM,
			M7MU_PARM_MON_SIZE, &read_mon_size);
		CHECK_ERR(err);

		err = m7mu_set_size_val(sd, read_mon_size);
		CHECK_ERR(err);

		err = m7mu_readb(client, M7MU_CATEGORY_PARM,
			M7MU_PARM_MON_MOVIE_SELECT, &value);
		CHECK_ERR(err);
		if (state->sensor_mode == SENSOR_MOVIE) {
			if (value != SENSOR_MOVIE) {
				err = m7mu_writeb(client, M7MU_CATEGORY_PARM,
					M7MU_PARM_MON_MOVIE_SELECT, 0x01);
				CHECK_ERR(err);
			}
		} else {
#if 1
			if (state->mode == MODE_PANORAMA) {
				err = m7mu_writeb(client,
						M7MU_CATEGORY_PARM,
						M7MU_PARM_MON_MOVIE_SELECT,
						0x01);
				CHECK_ERR(err);
			} else
#endif
			{
				if (value != SENSOR_CAMERA) {
					err = m7mu_writeb(client,
						M7MU_CATEGORY_PARM,
						M7MU_PARM_MON_MOVIE_SELECT,
						0x00);
					CHECK_ERR(err);
				}
			}
		}

		m7mu_set_gamma(sd);
		m7mu_set_iqgrp(sd, 0);

		if (state->sensor_mode != SENSOR_MOVIE)
			m7mu_set_dual_capture_mode(sd, 0);

		}
		cam_err("fps %d, target preview frame size %dx%d\n",
			state->fps, state->preview->target_width,
			state->preview->target_height);
	} else {
		if (!m7mu_check_postview(sd)) {
			if (!state->dual_capture_start) {
				/*
				   Capture size must be
				   set before capture command.
				 */
				err = m7mu_writeb(client, M7MU_CATEGORY_CAPPARM,
						M7MU_CAPPARM_MAIN_IMG_SIZE,
						state->capture->reg_val);
				CHECK_ERR(err);
				if (state->smart_zoom_mode)
					m7mu_set_smart_zoom(sd,
						state->smart_zoom_mode);
			}
			cam_info("target capture frame size %dx%d\n",
					state->capture->target_width,
					state->capture->target_height);
		} else {
			if (!state->fast_capture_set) {
				/*
				   Capture size must be
				   set before capture command.
				 */
				err = m7mu_writeb(client, M7MU_CATEGORY_CAPPARM,
						M7MU_CAPPARM_PREVIEW_IMG_SIZE,
						state->postview->reg_val);
				CHECK_ERR(err);
				cam_info("target postview frame size %dx%d\n",
						state->postview->target_width,
						state->postview->target_height);
			}
		}
	}
	cam_trace("X\n");
	return 0;
}

static int m7mu_s_fmt(struct v4l2_subdev *sd, struct v4l2_mbus_framefmt *ffmt)
{
	struct m7mu_state *state = to_state(sd);
	const struct m7mu_frmsizeenum **frmsize;

	u32 width = ffmt->width;
	u32 height = ffmt->height;
	u32 old_index;
	int i, num_entries;

	cam_trace("E\n");

	if (unlikely(state->isp.bad_fw)) {
		cam_err("\"Unknown\" state, please update F/W");
		return -ENOSYS;
	}

	cam_info("%s: ffmt->width = %d, ffmt->height = %d", __func__,
			ffmt->width, ffmt->height);
	/* state->format_mode = ffmt->field; */
	state->pixelformat = ffmt->colorspace;

	if (state->oprmode == M7MU_OPRMODE_VIDEO)
		frmsize = &state->preview;
	else if (!m7mu_check_postview(sd))
		frmsize = &state->capture;
	else
		frmsize = &state->postview;

	old_index = *frmsize ? (*frmsize)->index : -1;
	*frmsize = NULL;

	if (state->oprmode == M7MU_OPRMODE_VIDEO) {
		cam_info("%s: set preview size", __func__);
		num_entries = ARRAY_SIZE(preview_frmsizes);
		for (i = 0; i < num_entries; i++) {
			if (state->sensor_mode == SENSOR_MOVIE &&
				state->fps == 120 && width == 768 &&
				height == 512) {
				*frmsize = m7mu_get_frmsize(preview_frmsizes,
					num_entries, M7MU_PREVIEW_D1_120FPS);
				break;
			} else if (state->sensor_mode == SENSOR_MOVIE &&
				state->fps == 120 && width == 1280 &&
				height == 720) {
				*frmsize = m7mu_get_frmsize(preview_frmsizes,
					num_entries, M7MU_PREVIEW_720P_120FPS);
				break;
			} else if (state->sensor_mode == SENSOR_MOVIE &&
			/*
			   Workaround for 768x512 60fps 1/2x.
			   Should be Removed later.
			 */
				state->fps == 60 && width == 768 &&
				height == 512) {
				*frmsize = m7mu_get_frmsize(preview_frmsizes,
					num_entries, M7MU_PREVIEW_720P_60FPS);
				break;
			} else if (state->sensor_mode == SENSOR_MOVIE &&
			/*
			   Workaround for 960x540 30fps for Factory.
			   Should be Removed later.
			 */
				state->fps == 30 && width == 960 &&
				height == 540) {
				*frmsize = m7mu_get_frmsize(preview_frmsizes,
					num_entries, M7MU_PREVIEW_720P);
				break;
			} else if (state->sensor_mode == SENSOR_CAMERA &&
			/*
			   Workaround for target size 1920x1080
			   for Factory Tilt test.
			 */
				state->factory_test_num ==
				FACTORY_TILT_DIVISION &&
				width == 1920 && height == 1080) {
				*frmsize = m7mu_get_frmsize(preview_frmsizes,
					num_entries, M7MU_PREVIEW_FACTORY_TILT);
				break;
			}
			if (width == preview_frmsizes[i].target_width &&
				height == preview_frmsizes[i].target_height) {
				*frmsize = &preview_frmsizes[i];
				break;
			}
		}
	} else {
		if (!m7mu_check_postview(sd)) {
			cam_info("%s: set capture size", __func__);
			num_entries = ARRAY_SIZE(capture_frmsizes);
			for (i = 0; i < num_entries; i++) {
				if (width == capture_frmsizes[i].target_width &&
					height == capture_frmsizes[i].target_height) {
					*frmsize = &capture_frmsizes[i];
					break;
				}
			}
		} else {
			cam_info("%s: set postview size", __func__);
			num_entries = ARRAY_SIZE(postview_frmsizes);
			for (i = 0; i < num_entries; i++) {
				if (width == postview_frmsizes[i].target_width &&
					height == postview_frmsizes[i].target_height) {
					*frmsize = &postview_frmsizes[i];
					break;
				}
			}
		}
	}

	cam_info("%s: frmsize index = %d", __func__, i);

	if (*frmsize == NULL) {
		cam_warn("invalid frame size %dx%d\n", width, height);
		if (state->oprmode == M7MU_OPRMODE_VIDEO)
			*frmsize = m7mu_get_frmsize(preview_frmsizes,
				num_entries, M7MU_PREVIEW_720P);
		else if (!m7mu_check_postview(sd))
			*frmsize = m7mu_get_frmsize(capture_frmsizes,
				num_entries, M7MU_CAPTURE_15_1MPW);
		else
			*frmsize = m7mu_get_frmsize(postview_frmsizes,
				num_entries, M7MU_CAPTURE_POSTWHD);
	}

	cam_err("target size %dx%d\n",
			(*frmsize)->target_width, (*frmsize)->target_height);
	m7mu_set_frmsize(sd);

	cam_trace("X\n");
	return 0;
}

static int m7mu_g_parm(struct v4l2_subdev *sd, struct v4l2_streamparm *a)
{
	struct m7mu_state *state = to_state(sd);

	a->parm.capture.timeperframe.numerator = 1;
	a->parm.capture.timeperframe.denominator = state->fps;

	return 0;
}

/* --8 */
static int m7mu_s_parm(struct v4l2_subdev *sd, struct v4l2_streamparm *a)
{
	struct m7mu_state *state = to_state(sd);
	u32 fps = 0;
	/*int err;*/

	if (a->parm.capture.timeperframe.numerator != 0) {
		fps = a->parm.capture.timeperframe.denominator /
			a->parm.capture.timeperframe.numerator;
	} else
		fps = 0;

	if (unlikely(state->isp.bad_fw)) {
		cam_err("\"Unknown\" state, please update F/W");
		return -ENOSYS;
	}

	if (fps != state->fps) {
		if (fps <= 0 || fps > 120) {
			cam_err("invalid frame rate %d\n", fps);
			fps = 0; /* set to auto(default) */
		}
	}

	cam_info("%s: X, fps = %d\n", __func__, fps);
	return 0;
}

static int m7mu_enum_framesizes(struct v4l2_subdev *sd,
	struct v4l2_frmsizeenum *fsize)
{
	struct m7mu_state *state = to_state(sd);

	/*
	* The camera interface should read this value, this is the resolution
	* at which the sensor would provide framedata to the camera i/f
	* In case of image capture,
	* this returns the default camera resolution (VGA)
	*/
	if (state->oprmode == M7MU_OPRMODE_VIDEO) {
		if (state->preview == NULL
				/* FIXME || state->preview->index < 0 */)
			return -EINVAL;

		fsize->type = V4L2_FRMSIZE_TYPE_DISCRETE;
		fsize->discrete.width = state->preview->sensor_width;
		fsize->discrete.height = state->preview->sensor_height;
	} else if (!m7mu_check_postview(sd)) {
		if (state->capture == NULL
				/* FIXME || state->capture->index < 0 */)
			return -EINVAL;

		fsize->type = V4L2_FRMSIZE_TYPE_DISCRETE;
		fsize->discrete.width = state->capture->sensor_width;
		fsize->discrete.height = state->capture->sensor_height;
	} else {
		if (state->postview == NULL
				/* FIXME || state->postview->index < 0 */)
			return -EINVAL;

		fsize->type = V4L2_FRMSIZE_TYPE_DISCRETE;
		fsize->discrete.width = state->postview->sensor_width;
		fsize->discrete.height = state->postview->sensor_height;
	}

	cam_info("%s: discrete width = %d, height = %d", __func__,
			fsize->discrete.width, fsize->discrete.height);

	return 0;
}

static int m7mu_s_stream_preview(struct v4l2_subdev *sd, int enable)
{
	struct m7mu_state *state = to_state(sd);
	struct i2c_client *client = to_client(sd);

	u32 old_mode;
	u32 int_factor;
	int err;

	if (enable) {
		if (state->vt_mode) {
			err = m7mu_writeb(client, M7MU_CATEGORY_AE,
					M7MU_AE_EP_MODE_MON, 0x11);
			CHECK_ERR(err);
		}

		CLEAR_ISP_INT1_STATE(sd);

		old_mode = m7mu_set_mode(sd, M7MU_MONITOR_MODE);
		if (old_mode <= 0) {
			cam_err("failed to set mode\n");
			return old_mode;
		}

		if (old_mode != M7MU_MONITOR_MODE) {
			int_factor = m7mu_wait_interrupt(sd, M7MU_PREVIEW_TIMEOUT);
			if (!(int_factor & M7MU_INT_MODE)) {
				cam_err("M7MU_INT_MODE isn't issued, %#x\n",
					int_factor);
				/* return -ETIMEDOUT; */
			}
		}

#if 0 /* test */
		if (state->zoom >= 0x0F) {
			/* Zoom position returns to 1
			   when the monitor size is changed. */
			ctrl.id = V4L2_CID_CAMERA_ZOOM;
			ctrl.value = state->zoom;
			m7mu_set_zoom(sd, &ctrl);
		}
		if (state->smart_zoom_mode)
			m7mu_set_smart_zoom(sd, state->smart_zoom_mode);

		m7mu_set_lock(sd, 0);
#endif
	} else {
	}

	return 0;
}

static int m7mu_s_stream_capture(struct v4l2_subdev *sd, int enable)
{
	struct m7mu_state *state = to_state(sd);

	if (enable) {
		if (state->running_capture_mode == RUNNING_MODE_SINGLE) {
			if (state->factory_test_num != 0)
				m7mu_set_mode_part1(sd, M7MU_STILLCAP_MODE);
			state->fast_capture_set = 0;
		} else {
			int err;
			err = m7mu_set_mode(sd, M7MU_STILLCAP_MODE);
			if (err <= 0) {
				cam_err("failed to set mode\n");
				return err;
			}
		}
	}
	return 0;
}

static int m7mu_s_stream(struct v4l2_subdev *sd, int enable)
{
	struct m7mu_state *state = to_state(sd);
	int err = 0;

	cam_trace("E\n");

	if (unlikely(state->isp.bad_fw)) {
		cam_err("\"Unknown\" state, please update F/W");
		return -ENOSYS;
	}

	cam_info("state->oprmode=%d\n", state->oprmode);

	if ((state->running_capture_mode == RUNNING_MODE_BURST
			&& state->mburst_start)
		|| state->running_capture_mode == RUNNING_MODE_DUMP) {
		cam_trace("X\n");
		return 0;
	}

	switch (enable) {
	case STREAM_MODE_CAM_ON:
	case STREAM_MODE_CAM_OFF:
		switch (state->oprmode) {
		case M7MU_OPRMODE_IMAGE:
			cam_info("capture %s",
				enable == STREAM_MODE_CAM_ON ? "on" : "off");
			err = m7mu_s_stream_capture(sd,
					enable == STREAM_MODE_CAM_ON);
			break;
		default:
			cam_err("preview %s",
				enable == STREAM_MODE_CAM_ON ? "on" : "off");

			err = m7mu_s_stream_preview(sd,
					enable == STREAM_MODE_CAM_ON);
			break;
		}
		break;

	case STREAM_MODE_MOVIE_ON:
		state->recording = 1;
#if 0	/* Not use S project */
		if (state->flash_mode != FLASH_MODE_OFF)
			err = m7mu_set_flash(sd, state->flash_mode, 1);
#endif

		if (state->preview->index == M7MU_PREVIEW_720P ||
				state->preview->index == M7MU_PREVIEW_1080P)
			err = m7mu_set_af(sd, 1);
		break;

	case STREAM_MODE_MOVIE_OFF:
		if (state->preview->index == M7MU_PREVIEW_720P ||
				state->preview->index == M7MU_PREVIEW_1080P)
			err = m7mu_set_af(sd, 0);

#if 0	/* Not use S project */
		m7mu_set_flash(sd, FLASH_MODE_OFF, 1);
#endif
		state->recording = 0;
		break;

	default:
		cam_err("invalid stream option, %d\n", enable);
		break;
	}
	cam_trace("X\n");
	return err;
}

#if 0
static int m7mu_check_version(struct v4l2_subdev *sd)
{
	struct m7mu_state *state = to_state(sd);
	int i, val;

	for (i = 0; i < 6; i++) {
		m7mu_readb(client, M7MU_CATEGORY_SYS, M7MU_SYS_USER_VER, &val);
		state->exif.unique_id[i] = (char)val;
	}
	state->exif.unique_id[i] = '\0';

	cam_info("*************************************\n");
	cam_info("F/W Version: %s\n", state->exif.unique_id);
	cam_dbg("Binary Released: %s %s\n", __DATE__, __TIME__);
	cam_info("*************************************\n");

	return 0;
}
#endif

static int m7mu_init_param(struct v4l2_subdev *sd)
{
	struct m7mu_state *state = to_state(sd);
	struct i2c_client *client = to_client(sd);
	int err;
	cam_trace("E\n");

	err = m7mu_writew(client, M7MU_CATEGORY_SYS, M7MU_SYS_INT_EN,
			M7MU_INT_MODE | M7MU_INT_CAPTURE | M7MU_INT_FRAME_SYNC
			| M7MU_INT_ATSCENE_UPDATE /* | M7MU_INT_AF */
			| M7MU_INT_SOUND | M7MU_INT_ZOOM);
	CHECK_ERR(err);

	err = m7mu_writeb(client, M7MU_CATEGORY_PARM,
			M7MU_PARM_OUT_SEL, 0x02);
	CHECK_ERR(err);

	/* Capture */
	err = m7mu_writeb(client, M7MU_CATEGORY_CAPPARM,
			M7MU_CAPPARM_YUVOUT_MAIN, 0x01);
	CHECK_ERR(err);

	m7mu_set_sensor_mode(sd, state->sensor_mode);

	cam_trace("X\n");
	return 0;
}

#if defined(M7MU_ENABLE_ISP_FIRMWARE_UPDATE)
static int m7mu_fw_writing(struct v4l2_subdev *sd)
{
	int err = 0;
	int loading_err = 0;

	cam_trace("E\n");

	/* request gpio : ISP BOOT OPT , ISP RESET , POWER KEY */
	err = gpio_request(GPIO_ISP_INT2_SH, "ISP INT2");
	if (err < 0)
		cam_err("failed gpio_request(GPK0_0) for camera control\n");

	err = gpio_request(GPIO_ISP_BOOT_OPT, "ISP BOOT OPT");
	if (err < 0)
		cam_err("failed gpio_request(GPD0_4) for camera control\n");

	err = gpio_request(GPIO_AP_ISP_RESET, "ISP RESET");
	if (err < 0)
		cam_err("failed gpio_request(GPC4_0) for camera control\n");

	err = gpio_request(GPIO_AP_ISP_POWER_KEY, "GPB2_1");
	if (err < 0)
		cam_err("failed gpio_request(GPB2_1) for camera control\n");

	/* ISP Power off */
	err = gpio_direction_output(GPIO_ISP_INT2_SH, 1);
	if (err < 0)
		cam_err("failed gpio_direction_output(GPK0_0) value 1\n");
	/*	cam_err("GPIO_ISP_INT2_SH : 1\n");	*/

	err = gpio_direction_output(GPIO_ISP_BOOT_OPT, 0);
	if (err < 0)
		cam_err("failed gpio_direction_output(GPD0_4) value 0\n");
	/*	cam_err("GPIO_ISP_BOOT_OPT : 0\n");	*/

	mdelay(10);

	CLEAR_ISP_BOOT_INT_STATE(sd);

	err = gpio_direction_output(GPIO_AP_ISP_POWER_KEY, 1);
	if (err < 0)
		cam_err("failed gpio_direction_output(GPM2_3) value 1\n");
	/*	cam_err("GPIO_AP_ISP_POWER_KEY : 1\n");	*/

	mdelay(100);

	err = gpio_direction_output(GPIO_AP_ISP_POWER_KEY, 0);
	if (err < 0)
		cam_err("failed gpio_direction_output(GPM2_3) value 0\n");
	/*	cam_err("GPIO_AP_ISP_POWER_KEY : 0\n");	*/

	cam_trace("wait_boot_interrupt for Power Off on fw writing\n");
	if (m7mu_wait_boot_interrupt(sd, M7MU_ISP_BOOT_TIMEOUT)) {
		cam_err("ISP power off boot interrupt err\n");

		if(regulator_is_enabled(main_cam_sensor_3v3_regulator)) {
			err = regulator_disable(main_cam_sensor_3v3_regulator);
			if (err < 0)
				cam_err("failed to disable main_cam_sensor_3v3_regulator\n");
		} else {
			cam_err("main_cam_sensor_3v3_regulator is already disabled\n");
		}

		msleep(300);

		if(!regulator_is_enabled(main_cam_sensor_3v3_regulator)) {
			err = regulator_enable(main_cam_sensor_3v3_regulator);
			if (err < 0)
				cam_err("failed to disable main_cam_sensor_3v3_regulator\n");
		} else {
			cam_err("main_cam_sensor_3v3_regulator is already enabled\n");
		}

		msleep(300);
	}

	mdelay(10);

	err = gpio_direction_output(GPIO_AP_ISP_RESET, 0);
	if (err < 0)
		cam_err("failed gpio_direction_output(GPC4_0) value 0\n");
	/*	cam_err("GPIO_AP_ISP_RESET : 0\n");	*/

	msleep(200);

	/* Start ISP update mode (I2C mode)	*/
	err = gpio_direction_output(GPIO_ISP_BOOT_OPT, 1);
	if (err < 0)
		cam_err("failed gpio_direction_output(GPD0_4) value 1\n");
	/*	cam_err("GPIO_ISP_BOOT_OPT : 1\n");	*/

	mdelay(50);

	CLEAR_ISP_BOOT_INT_STATE(sd);

	err = gpio_direction_output(GPIO_AP_ISP_POWER_KEY, 1);
	if (err < 0)
		cam_err("failed gpio_direction_output(GPM2_3) value 1\n");
	/*	cam_err("GPIO_AP_ISP_POWER_KEY : 1\n");	*/

	mdelay(40);

	err = gpio_direction_output(GPIO_AP_ISP_RESET, 1);
	if (err < 0)
		cam_err("failed gpio_direction_output(GPC4_0) value 1\n");
	/*	cam_err("GPIO_AP_ISP_RESET : 1\n");	*/

	mdelay(60);

	err = gpio_direction_output(GPIO_AP_ISP_POWER_KEY, 0);
	if (err < 0)
		cam_err("failed gpio_direction_output(GPM2_3) value 0\n");
	/*	cam_err("GPIO_AP_ISP_POWER_KEY : 0\n");	*/

	cam_trace("wait_boot_interrupt for Power On on fw wiring\n");
	if (m7mu_wait_boot_interrupt(sd, M7MU_ISP_BOOT_TIMEOUT)) {
		cam_err("ISP update mode boot interrupt err\n");
		return -1;
	}

	/* load and update ISP fw */
	loading_err = m7mu_load_fw_main(sd);
	if (loading_err < 0)
		cam_err("failed load_fw_main!!\n");

	/* ISP Power off */
	msleep(200);

	err = gpio_direction_output(GPIO_ISP_BOOT_OPT, 0);
	if (err < 0)
		cam_err("failed gpio_direction_output(GPD0_4) value 0\n");
	/*	cam_err("GPIO_ISP_BOOT_OPT : 0\n");	*/

	err = gpio_direction_output(GPIO_AP_ISP_RESET, 0);
	if (err < 0)
		cam_err("failed gpio_direction_output(GPC4_0) value 0\n");
	/*	cam_err("GPIO_AP_ISP_RESET : 0\n");	*/

	err = gpio_direction_output(GPIO_AP_ISP_POWER_KEY, 0);
	if (err < 0)
		cam_err("failed gpio_direction_output(GPM2_3) value 0\n");
	/*	cam_err("GPIO_AP_ISP_POWER_KEY : 0\n");	*/

	err = gpio_direction_output(GPIO_ISP_INT2_SH, 0);
	if (err < 0)
		cam_err("failed gpio_direction_output(GPK0_0) value 0\n");
	/*	cam_err("GPIO_ISP_INT2_SH : 0\n");	*/

	mdelay(100);

	/*	cam_err("3.3v regulator off\n");	*/
	if(regulator_is_enabled(main_cam_sensor_3v3_regulator)) {
		err = regulator_disable(main_cam_sensor_3v3_regulator);
		if (err < 0)
			cam_err("failed to disable main_cam_sensor_3v3_regulator\n");
	} else {
		cam_err("main_cam_sensor_3v3_regulator is already disabled\n");
	}

	mdelay(200);

	/* free gpio */
	gpio_free(GPIO_AP_ISP_POWER_KEY);
	gpio_free(GPIO_ISP_BOOT_OPT);
	gpio_free(GPIO_AP_ISP_RESET);
	gpio_free(GPIO_ISP_INT2_SH);

	if (loading_err < 0)
		return loading_err;
	else
		return err;

}

static int m7mu_fw_writing_vacant(struct v4l2_subdev *sd)
{
	int err = 0;
	int loading_err = 0;

	/* struct m7mu_state *state = to_state(sd); */

	cam_trace("E\n");


	cam_info("3.3v regulator off\n");
	if(regulator_is_enabled(main_cam_sensor_3v3_regulator)) {
		err = regulator_disable(main_cam_sensor_3v3_regulator);
		if (err < 0)
			cam_err("failed to disable main_cam_sensor_3v3_regulator\n");
	} else {
		cam_err("main_cam_sensor_3v3_regulator is already disabled\n");
	}

	msleep(300);

	cam_info("3.3v regulator on\n");
	if(!regulator_is_enabled(main_cam_sensor_3v3_regulator)) {
		err = regulator_enable(main_cam_sensor_3v3_regulator);
		if (err < 0)
			cam_err("failed to disable main_cam_sensor_3v3_regulator\n");
	} else {
		cam_err("main_cam_sensor_3v3_regulator is already enabled\n");
	}

	msleep(300);

	/* request for gpio  */
	err = gpio_request(GPIO_ISP_BOOT_OPT, "ISP BOOT OPT");
	if (err < 0)
		cam_err("failed gpio_request(GPD0_4) for camera control\n");

	err = gpio_request(GPIO_AP_ISP_RESET, "ISP RESET");
	if (err < 0)
		cam_err("failed gpio_request(GPC4_0) for camera control\n");

	err = gpio_request(GPIO_ISP_INT2_SH, "ISP INT2");
	if (err < 0)
		cam_err("failed gpio_request(GPK0_0) for camera control\n");

	err = gpio_request(GPIO_AP_ISP_POWER_KEY, "GPB2_1");
	if (err < 0)
		cam_err("failed gpio_request(GPB2_1) for camera control\n");

	/* Start ISP update mode (I2C mode)	*/
	err = gpio_direction_output(GPIO_ISP_BOOT_OPT, 1);
	if (err < 0)
		cam_err("failed gpio_direction_output(GPD0_4) value 1\n");
	/*	cam_err("GPIO_ISP_BOOT_OPT : 1\n");	*/

	err = gpio_direction_output(GPIO_ISP_INT2_SH, 1);
	if (err < 0)
		cam_err("failed gpio_direction_output(GPK0_0) value 1\n");
	/*	cam_err("GPIO_ISP_INT2_SH : 1\n");	*/


	mdelay(50);

	CLEAR_ISP_BOOT_INT_STATE(sd);
	err = gpio_direction_output(GPIO_AP_ISP_POWER_KEY, 1);
	if (err < 0)
		cam_err("failed gpio_direction_output(GPM2_3) value 1\n");
	/*	cam_err("GPIO_AP_ISP_POWER_KEY : 1\n");	*/

	mdelay(40);

	err = gpio_direction_output(GPIO_AP_ISP_RESET, 1);
	if (err < 0)
		cam_err("failed gpio_direction_output(GPC4_0) value 1\n");
	/*	cam_err("GPIO_AP_ISP_RESET : 1\n");	*/

	mdelay(60);

	err = gpio_direction_output(GPIO_AP_ISP_POWER_KEY, 0);
	if (err < 0)
		cam_err("failed gpio_direction_output(GPM2_3) value 0\n");
	/*	cam_err("GPIO_AP_ISP_POWER_KEY : 0\n");	*/

	if (m7mu_wait_boot_interrupt(sd, M7MU_ISP_BOOT_TIMEOUT)) {
		cam_err("ISP update mode boot interrupt err\n");
		return -1;
	}

	/* load and update ISP fw */
	cam_err("load_fw_main\n");
	loading_err = m7mu_load_fw_main(sd);
	if (loading_err < 0)
		cam_err("failed load_fw_main!!\n");

	/* isp reboot */
	err = gpio_direction_output(GPIO_ISP_BOOT_OPT, 0);
	if (err < 0)
		cam_err("failed gpio_direction_output(GPD0_4) value 0\n");
	/*	cam_err("GPIO_ISP_BOOT_OPT : 0\n");	*/

	err = gpio_direction_output(GPIO_AP_ISP_RESET, 0);
	if (err < 0)
		cam_err("failed gpio_direction_output(GPC4_0) value 0\n");
	/*	cam_err("GPIO_AP_ISP_RESET : 0\n");	*/

	err = gpio_direction_output(GPIO_AP_ISP_POWER_KEY, 0);
	if (err < 0)
		cam_err("failed gpio_direction_output(GPM2_3) value 0\n");
	/*	cam_err("GPIO_AP_ISP_POWER_KEY : 0\n");	*/

	err = gpio_direction_output(GPIO_ISP_INT2_SH, 0);
	if (err < 0)
		cam_err("failed gpio_direction_output(GPK0_0) value 0\n");
	/*	cam_err("GPIO_ISP_INT2_SH : 0\n");	*/

	mdelay(100);

	/*	cam_err("3.3v regulator off\n");	*/
	if(regulator_is_enabled(main_cam_sensor_3v3_regulator)) {
		err = regulator_disable(main_cam_sensor_3v3_regulator);
		if (err < 0)
			cam_err("failed to disable main_cam_sensor_3v3_regulator\n");
	} else {
		cam_err("main_cam_sensor_3v3_regulator is already disabled\n");
	}

	mdelay(200);

	gpio_free(GPIO_AP_ISP_POWER_KEY);
	gpio_free(GPIO_ISP_BOOT_OPT);
	gpio_free(GPIO_AP_ISP_RESET);
	gpio_free(GPIO_ISP_INT2_SH);

	if (loading_err < 0 )
		return loading_err;
	else
		return err;
}
#endif /* M7MU_ENABLE_ISP_FIRMWARE_UPDATE */

static int m7mu_ois_init(struct v4l2_subdev *sd)
{
	struct i2c_client *client = to_client(sd);
	const struct m7mu_platform_data *pdata = client->dev.platform_data;
	struct m7mu_state *state = to_state(sd);
	u32 int_factor, int_en, err, ois_result;
	int try_cnt = 2;

	cam_dbg("E\n");

	err = m7mu_readw(client, M7MU_CATEGORY_SYS, M7MU_SYS_INT_EN, &int_en);
	CHECK_ERR(err);

	/* enable OIS_INIT interrupt */
	int_en |= M7MU_INT_OIS_INIT;
	/* enable LENS_INIT interrupt */
	int_en |= M7MU_INT_LENS_INIT;

	err = m7mu_writew(client, M7MU_CATEGORY_SYS, M7MU_SYS_INT_EN, int_en);
	CHECK_ERR(err);

	do {
		CLEAR_ISP_INT1_STATE(sd);

		/* OIS on set */
		err = m7mu_writeb(client, M7MU_CATEGORY_NEW,
				0x10, 0x01);
		CHECK_ERR(err);

		/* OIS F/W download, boot */
		err = m7mu_writeb(client, M7MU_CATEGORY_NEW,
				0x11, 0x00);

		int_factor = m7mu_wait_interrupt(sd, M7MU_ISP_TIMEOUT);
		if (!(int_factor & M7MU_INT_OIS_INIT)) {
			cam_err("OIS interrupt not issued\n");
			if (try_cnt > 1) {
				try_cnt--;
				pdata->config_sambaz(0);
				msleep(20);
				if (try_cnt == 1)
					pdata->config_sambaz(1);
				continue;
			}
			state->isp.bad_fw = 1;
			return -ENOSYS;
		}
		cam_info("OIS init complete\n");

		/* Read OIS result */
		m7mu_readb(client, M7MU_CATEGORY_NEW, 0x17, &ois_result);
		cam_info("ois result = %d", ois_result);
		if (ois_result != 0x02) {
			try_cnt--;
			pdata->config_sambaz(0);
			msleep(20);
			if (try_cnt == 1)
				pdata->config_sambaz(1);
		} else
			try_cnt = 0;
	} while (try_cnt);

	/* Lens boot */
	if (!m7mu_Lens_close_hold) {
		CLEAR_ISP_INT1_STATE(sd);
		err = m7mu_writeb(client, M7MU_CATEGORY_LENS,
				M7MU_LENS_AF_INITIAL, 0x00);
		int_factor = m7mu_wait_interrupt(sd, M7MU_ISP_TIMEOUT);
		if (!(int_factor & M7MU_INT_LENS_INIT)) {
			cam_err("M7MU_INT_LENS_INIT isn't issued, %#x\n",
					int_factor);
			return -ETIMEDOUT;
		}
		m7mu_Lens_Off_Needed = 1;
	}
	cam_dbg("m7mu_Lens_Off_Needed = %d\n", m7mu_Lens_Off_Needed);
	cam_dbg("X\n");

	return err;
}

#if 1
static int m7mu_auto_fw_check(struct v4l2_subdev *sd)
{
	int err = 0;
	int retry_cnt = 2;
	
	if (strcmp(sysfs_phone_fw, sysfs_sensor_fw) == 0)
		return 1;
fw_retry:
	cam_dbg("Auto Firmware update start! Try No. %d\n", 3-retry_cnt);

	err = m7mu_fw_writing(sd);
	if ( err < 0 ) {
		cam_err("failed Auto FW update!!\n");
		if (retry_cnt > 0 ) {
			retry_cnt--;
			m7mu_power(sd, M7MU_HW_POWER_ON, 0);
			msleep(200);
			goto fw_retry;
		}
		return err;
	}

	cam_dbg("X\n");

	return err;
}
#endif

static int m7mu_pre_init(struct v4l2_subdev *sd, u32 val)
{
	cam_dbg("PRE init called. val=%d\n", val);
	return 0;
}

static irqreturn_t m7mu_isp_sound_isr(int irq, void *dev_id)
{
	struct v4l2_subdev *sd = (struct v4l2_subdev *)dev_id;
	struct m7mu_state *state = to_state(sd);

	cam_trace("################ interrupt ################\n");
	state->isp.sound_issued = 1;
	wake_up(&state->isp.sound_wait);

	return IRQ_HANDLED;
}

static ssize_t m7mu_register_sound_irq(struct v4l2_subdev *sd)
{
	struct m7mu_state *state = to_state(sd);
	int err = 0;
	int ret = 0;

	printk(KERN_INFO "m7mu_config_isp_sound_irq\n");
	gpio_request(GPIO_ISP_INT2_SH, "M7MU SOUND IRQ");
	s3c_gpio_cfgpin(GPIO_ISP_INT2_SH, S3C_GPIO_SFN(0xF));
	s3c_gpio_setpull(GPIO_ISP_INT2_SH, S3C_GPIO_PULL_DOWN);

	ret = s5p_register_gpio_interrupt(GPIO_ISP_INT2_SH);
	if (ret < 0) {
		printk(KERN_ERR "%s: s5p_register_gpio_interrupt is failed. %d",
				__func__, ret);
	}
	gpio_free(GPIO_ISP_INT2_SH);

	sound_irq = gpio_to_irq(GPIO_ISP_INT2_SH);
	state->isp.sound_issued = 0;

	/* wait queue initialize */
	init_waitqueue_head(&state->isp.sound_wait);

	err = request_irq(sound_irq,
		m7mu_isp_sound_isr, IRQF_TRIGGER_RISING, "m7mu sound isp", sd);
	if (err) {
		cam_err("failed to request sound irq ~~~~~~~~~~~~~\n");
		return err;
	}

	state->isp.sound_irq = sound_irq;

	return err;
}

/* --4 */
static int m7mu_init(struct v4l2_subdev *sd, u32 val)
{
	/*
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct m7mu_platform_data *pdata = client->dev.platform_data;
	*/
	struct m7mu_state *state = to_state(sd);
	struct i2c_client *client = to_client(sd);
	/* u32 int_factor; */
	/* u32 value; */
	int err = 0;
	int retry_cnt = 2;

	cam_dbg("E : val = %d\n", val);
	cam_dbg("system_rev : %d\n", system_rev);
	if (val == 2) {
		cam_trace("################ interrupt ################\n");
		state->isp.sound_issued = 1;
		wake_up(&state->isp.sound_wait);

		m7mu_power(sd, M7MU_HW_POWER_OFF, 0);
		msleep(300);
		m7mu_power(sd, M7MU_HW_POWER_ON, 0);
		cam_dbg("val : 2 , esd test\n");
	}
re_init:
	/* Default state values */
	state->preview = NULL;
	state->capture = NULL;
	state->postview = NULL;

	state->format_mode = V4L2_PIX_FMT_MODE_PREVIEW;
	state->sensor_mode = SENSOR_CAMERA;
	state->flash_mode = V4L2_FLASH_MODE_OFF;
	state->scene_mode = SCENE_MODE_NONE;

	state->face_beauty = 0;

	state->fps = 0;			/* auto */

	state->isp.bad_fw = 0;
	state->isp.issued = 0;

	state->zoom = 0;
	state->smart_zoom_mode = 0;

	state->fast_capture_set = 0;

	state->IsStartObjectTraking = false;

	state->vss_mode = 0;
	state->dual_capture_start = 0;
	state->dual_capture_frame = 1;
	state->focus_area_mode = V4L2_FOCUS_AREA_CENTER;

	state->bracket_wbb_val = BRACKET_WBB_VALUE3;  /* AB -+1 */
	state->wb_custom_rg = 424; /* 1A8 */
	state->wb_custom_bg = 452; /* 1C4 */

	state->color_effect = 0;
	state->gamma_rgb_mon = 2;
	state->gamma_rgb_cap = 2;
	state->gamma_tbl_rgb_cap = 1;
	state->gamma_tbl_rgb_mon = 1;

	state->mburst_start = false;
	state->factory_log_write = true;
	state->factory_no_lens_off = false;

	state->isp.boot_lens = 0;
	state->mode = 0;
	state->strobe_en = 0;

	state->samsung_app = 0;
	state->sys_status = 0x0;

	memset(&state->focus, 0, sizeof(state->focus));

	esdreset = 0;

	m7mu_Lens_close_hold = (val & 0x00FF);
	state->lens_mem = (val >> 8) & 0xFF;
	m7mu_Lens_Off_Needed = 0;

	if (state->isp.boot_issued)
		state->isp.boot_issued = 0;
	if (state->isp.issued)
		state->isp.issued = 0;

	m7mu_get_phone_fw_version(sd);

	if ((m7mu_which_power_on() == 0) || (m7mu_which_power_on() == 1)) {
		err = m7mu_wait_boot_interrupt(sd, M7MU_ISP_BOOT_TIMEOUT);
		if (err < 0) {
			if ( val == 3 || val == 1 ) {
fw_retry:
				cam_err("Try No.%d\n",3-retry_cnt);
				err = m7mu_fw_writing_vacant(sd);
				if ( err < 0 ) {
					cam_err("failed vacant ISP fw writing!!, err %d\n", err );
					if (retry_cnt > 0) {
						retry_cnt--;
						goto fw_retry;
					}
					return err;
				}
			} else if( val == 0 ) {
				cam_err("User abnormal case : vacant ISP!!\n");
				err = m7mu_fw_writing_vacant(sd);
				if ( err == 0) {
					m7mu_power(sd, M7MU_HW_POWER_ON, 0);
					goto re_init;
				} else if( err < 0 ) {
					cam_err("failed vacant ISP fw writing!!, err %d\n", err );
					return err;
				}
			} else {
				cam_err("boot interrupt timeout~~, err %d\n", err);
				return err;
			}
		}
	}

	cam_info("Register sound irq");
	err = m7mu_register_sound_irq(sd);
	if (err < 0)
		cam_err("%s: Failed to register sound irq", __func__);
	/* Choose EEPROM or NOR using ISP_RESET command */
	cam_info("M7MU set init target memory(%d)\n", state->lens_mem);
	if (state->lens_mem != 0) {   /* default state is nor(lens_mem == 0) */
		err = m7mu_writeb(client, M7MU_CATEGORY_LENS,
				M7MU_LENS_MEM_INIT_TARGET, state->lens_mem);
		CHECK_ERR(err);
		err = m7mu_writeb(client, M7MU_CATEGORY_SYS,
				M7MU_SYS_ISP_RESET, 0x01);
		CHECK_ERR(err);
		msleep(20);	/* Wait until previous interrupt pulse (10ms) has been complete */
		CLEAR_ISP_BOOT_INT_STATE(sd);
		err = m7mu_wait_boot_interrupt(sd, M7MU_ISP_BOOT_TIMEOUT);
		if ( err < 0) {
			cam_err("reset time out err %d\n", err);
			return err;
		}
	}

	m7mu_init_parameters(sd);

	/* TODO init_param : move to post_init */
	m7mu_init_param(sd);

	if (m7mu_init_formats(sd, NULL))
		cam_err("Failed init formats.\n");

#ifdef HOLD_LENS_SUPPORT
	if (!leave_power) {
		/* SambaZ PLL enable */
		cam_dbg("SambaZ On start ~~~\n");
#if 0
		pdata->config_sambaz(1);
#endif
		cam_dbg("SambaZ On finish ~~~\n");

		CLEAR_ISP_INT1_STATE(sd);

		if (system_rev > 0) {
			err = m7mu_writel(client, M7MU_CATEGORY_FLASH,
					0x0C, 0x27c00020);
		}

		/* start camera program(parallel FLASH ROM) */
		cam_info("write 0x0f, 0x12~~~\n");
		err = m7mu_writeb(client, M7MU_CATEGORY_FLASH,
				M7MU_FLASH_CAM_START, 0x01);
		CHECK_ERR(err);

		int_factor = m7mu_wait_interrupt(sd, M7MU_ISP_SHORT_TIMEOUT);
		if (!(int_factor & M7MU_INT_MODE)) {
			cam_err("firmware was erased?\n");
			state->isp.bad_fw = 1;
			return -ENOSYS;
		}
		cam_info("ISP boot complete\n");
	}
#else
	/* SambaZ PLL enable */
	cam_dbg("SambaZ On start ~~~\n");
#if 0
	pdata->config_sambaz(1);
#endif
	cam_dbg("SambaZ On finish ~~~\n");

#if 0
#ifdef CONFIG_MACH_GC2PD
	err = m7mu_writel(client, M7MU_CATEGORY_FLASH,
			0x0C, 0x27c00020);
#else
	if (system_rev > 0) {
		err = m7mu_writel(client, M7MU_CATEGORY_FLASH,
				0x0C, 0x27c00020);
	}
#endif
#endif
#if 0
	/* start camera program(parallel FLASH ROM) */
	cam_info("write 0x0f, 0x12~~~\n");
	err = m7mu_writeb(client, M7MU_CATEGORY_FLASH,
			M7MU_FLASH_CAM_START, 0x01);
	CHECK_ERR(err);

	int_factor = m7mu_wait_interrupt(sd, M7MU_ISP_TIMEOUT);
	if (!(int_factor & M7MU_INT_MODE)) {
		cam_err("firmware was erased?\n");
		state->isp.bad_fw++;
		return -ENOSYS;
	}
#endif
	cam_info("ISP boot complete\n");
#endif

	/* check up F/W version */
#if 0
	m7mu_check_fw(sd);
#else
	m7mu_get_sensor_fw_version(sd);
	cam_info("phone ver = %s, sensor_ver = %s\n",
			sysfs_phone_fw, sysfs_sensor_fw);
#endif

	if (val == 0) {
		if (m7mu_quickshot_on == 0) {
			err = m7mu_auto_fw_check(sd);
			if (err == 0) {
				m7mu_power(sd, M7MU_HW_POWER_ON, 0);
				goto re_init;
			}
			else if ( err < 0) {
				cam_err("auto_fw_update err!!\n");
				return err;
			}
		}
	}
	cam_info("M7MU init complete\n");

#if 0
	cam_info("M7MU set init target memory(%d)\n", state->lens_mem);
	err = m7mu_writeb(client, M7MU_CATEGORY_LENS,
			M7MU_LENS_MEM_INIT_TARGET, state->lens_mem);
#endif

	return 0;
}

static int m7mu_deinit(struct v4l2_subdev *sd)
{
	struct m7mu_state *state = to_state(sd);
	struct i2c_client *client = to_client(sd);
	const struct m7mu_platform_data *pdata = client->dev.platform_data;

	int err = 0;

	cam_trace("E\n");

	/* AF LED regulator off */
	if (pdata != NULL && pdata->af_led_power != NULL)
		pdata->af_led_power(0);

#ifdef HOLD_LENS_SUPPORT
	if (!leave_power) {
#ifdef M7MU_ISP_DEBUG
		char filename[32];
		sprintf(filename, "_ISP_%06d.LOG%c", ++m7mu_LogNo, 0);
		m7mu_makeLog(sd, filename, false);
#endif
		if (m7mu_set_lens_off(sd) < 0)
			cam_err("failed to set m7mu_set_lens_off~~~~~\n");
	} else {
		m7mu_set_capture_mode(sd, RUNNING_MODE_SINGLE);
	}
#else
#ifdef M7MU_ISP_DEBUG
	char filename[32];
	sprintf(filename, "_ISP_%06d.LOG%c", ++m7mu_LogNo, 0);
	m7mu_makeLog(sd, filename, false);
#endif
	if (m7mu_set_lens_off(sd) < 0) {
		cam_err("failed to set m7mu_set_lens_off~~~~~\n");
		tempclient = NULL;
	}
#endif

#ifdef HOLD_LENS_SUPPORT
	if (leave_power) {
		err = m7mu_set_lens_off_timer(sd, 0);
		CHECK_ERR(err);

		/*err = m7mu_set_mode(sd, M7MU_PARMSET_MODE);
		CHECK_ERR(err);*/
	}
#else
	err = m7mu_set_lens_off_timer(sd, 0);
	if (err < 1) {
		cam_err("failed to set m7mu_set_lens_off_timer~~~~~\n");
		tempclient = NULL;
	}
	CHECK_ERR(err);
#endif

	if (state->isp.sound_irq > 0)
		free_irq(state->isp.sound_irq, sd);

	return err;
}

static int m7mu_post_init(struct v4l2_subdev *sd, u32 val)
{
	int err;
	struct i2c_client *client = to_client(sd);

	cam_info("post init E");
	cam_info("Thermistor val: True(0~40C) or False = %d\n", val);

	err = m7mu_writeb(client, M7MU_CATEGORY_LENS,
			M7MU_LENS_AF_TEMP_INDICATE, val);
	CHECK_ERR(err);

#ifdef HOLD_LENS_SUPPORT
	if (!leave_power) {
		m7mu_init_param(sd);
		m7mu_ois_init(sd);
	}
#else
/* m7mu_init_param(sd); */
	m7mu_ois_init(sd);
#endif

#ifdef HOLD_LENS_SUPPORT
	leave_power = false;
#endif

	cam_info("Lens boot complete - M7MU post init complete\n");

	return 0;
}

#if defined(M7MU_ENABLE_COLD_POWER)
static int m7mu_cold_power(struct v4l2_subdev *sd, int flag)
{
	int ret;
#ifdef M7MU_ENABLE_BOOT_IRQ_EN_CTRL
	struct m7mu_state *state = to_state(sd);
#endif

	cam_trace("Sensor is power %s\n",
			flag == M7MU_HW_POWER_ON ? "on" : "off");

	ret = gpio_request(GPIO_ISP_INT2_SH, "ISP INT2");
	if (ret < 0)
		cam_err("failed gpio_request(GPK0_0) for camera control\n");
	ret = gpio_request(GPIO_ISP_BOOT_OPT, "ISP BOOT OPT");
	if (ret < 0)
		cam_err("failed gpio_request(GPD0_4) for camera control\n");
	ret = gpio_request(GPIO_AP_ISP_POWER_KEY, "ISP POWER KEY");
	if (ret < 0)
		cam_err("failed gpio_request(GPB2_1) for camera control\n");

#if defined(ISP_RESET_SELECT)
	ret = gpio_request(GPIO_AP_ISP_RESET, "ISP RESET");
	if (ret < 0)
		cam_err("failed gpio_request(GPC4_0) for camera control\n");
#endif

	if (flag == M7MU_HW_POWER_ON) {

#ifdef M7MU_ENABLE_BOOT_IRQ_EN_CTRL
		if(!state->isp.boot_irq_enabled) {
			enable_irq(state->isp.boot_irq);
			state->isp.boot_irq_enabled = 1;
		}
#endif

#if defined(M7MU_ENABLE_REGULATOR_CTRL_CORE_3V3)
		/*
		* Since sensor 3.3V was already was enabled on bootloader, we have to explicitly
		* enable the regulator again in order to increase use_count from 0 to 1.
		*/
		ret = regulator_enable(main_cam_sensor_3v3_regulator);
		if (ret < 0)
			cam_err("failed to enable main_cam_sensor_3v3_regulator\n");

		if(!regulator_is_enabled(main_cam_sensor_1v2_regulator)) {
			ret = regulator_enable(main_cam_sensor_1v2_regulator);
			if (ret < 0)
				cam_err("failed to enable main_cam_sensor_1v2_regulator\n");
		} else {
			cam_err("main_cam_sensor_1v2_regulator is already enabled\n");
		}

		if(!regulator_is_enabled(main_cam_sensor_1v8_regulator)) {
			ret = regulator_enable(main_cam_sensor_1v8_regulator);
			if (ret < 0)
				cam_err("failed to enable main_cam_sensor_1v8_regulator\n");
		} else {
			cam_err("main_cam_sensor_1v8_regulator is already enabled\n");
		}

		if(!regulator_is_enabled(main_cam_sensor_a2v8_regulator)) {
			ret = regulator_enable(main_cam_sensor_a2v8_regulator);
			if (ret < 0)
				cam_err("failed to enable main_cam_sensor_a2v8_regulator\n");
		} else {
			cam_err("main_cam_sensor_a2v8_regulator is already enabled\n");
		}

#endif /* M7MU_ENABLE_REGULATOR_CTRL_CORE_3V3 */
	} else {
		ret = gpio_direction_output(GPIO_ISP_INT2_SH, 0);
		if (ret < 0)
			cam_err("failed gpio_direction_output(GPD0_4) value 0\n");
	}

	/* Control ISP Boot Option */
	ret = gpio_direction_output(GPIO_ISP_BOOT_OPT, 0);
	if (ret < 0)
		cam_err("failed gpio_direction_output(GPD0_4) value 0\n");

	/* Control ISP Power Key */
	CLEAR_ISP_BOOT_INT_STATE(sd);
	ret = gpio_direction_output(GPIO_AP_ISP_POWER_KEY, 1);
	cam_err("AP_ISP_POWER_KEY value : 1\n");
	if (ret < 0)
		cam_err("failed gpio_direction_output(GPM2_3) value 1\n");

	mdelay(40);

#if defined(ISP_RESET_SELECT)
	/* Control Reset */
	if (flag == M7MU_HW_POWER_ON) {
		ret = gpio_direction_output(GPIO_AP_ISP_RESET, 1);
		cam_err("AP_ISP_RESET value : 1\n");
		if (ret < 0)
			cam_err("failed gpio_direction_output(GPC4_0) value 1\n");
		
		ret = gpio_direction_output(GPIO_ISP_INT2_SH, 1);
		if (ret < 0)
			cam_err("failed gpio_direction_output(GPD0_4) value 0\n");
	}
#endif

	mdelay(60);

	ret = gpio_direction_output(GPIO_AP_ISP_POWER_KEY, 0);
	cam_err("AP_ISP_POWER_KEY value : 0\n");
	if (ret < 0)
		cam_err("failed gpio_direction_output(GPB2_1) value 0\n");

	if (flag == M7MU_HW_POWER_OFF) {
		mdelay(20);	/* Wait until previous interrupt pulse (10ms) has been complete */
		CLEAR_ISP_BOOT_INT_STATE(sd);
		m7mu_wait_boot_interrupt(sd, M7MU_ISP_BOOT_TIMEOUT);

		mdelay(10);
#if defined(ISP_RESET_SELECT)
		ret = gpio_direction_output(GPIO_AP_ISP_RESET, 0);
		if (ret < 0)
			cam_err("failed gpio_direction_output(GPC4_0) value 0\n");
#endif	/* ISP_RESET_SELECT */

#if defined(M7MU_ENABLE_REGULATOR_CTRL_CORE_3V3)
		if (regulator_is_enabled(main_cam_sensor_3v3_regulator)) {
			ret = regulator_disable(main_cam_sensor_3v3_regulator);
			if (ret < 0)
				cam_err("failed to disable main_cam_sensor_3v3_regulator\n");
		} else {
			cam_err("main_cam_sensor_3v3_regulator is already disabled\n");
		}

		if(regulator_is_enabled(main_cam_sensor_a2v8_regulator)) {
			regulator_disable(main_cam_sensor_a2v8_regulator);
			if (ret < 0)
				cam_err("failed to disable main_cam_sensor_a2v8_regulator\n");
		} else {
			cam_err("main_cam_sensor_a2v8_regulator is already disabled\n");
		}

		if(regulator_is_enabled(main_cam_sensor_1v8_regulator)) {
			regulator_disable(main_cam_sensor_1v8_regulator);
			if (ret < 0)
				cam_err("failed to disable main_cam_sensor_1v8_regulator\n");
		} else {
			cam_err("main_cam_sensor_1v8_regulator is already disabled\n");
		}

		if(regulator_is_enabled(main_cam_sensor_1v2_regulator)) {
			regulator_disable(main_cam_sensor_1v2_regulator);
			if (ret < 0)
				cam_err("failed to disable main_cam_sensor_1v2_regulator\n");
		} else {
			cam_err("main_cam_sensor_1v2_regulator is already disabled\n");
		}

#endif /* M7MU_ENABLE_REGULATOR_CTRL */

#ifdef M7MU_ENABLE_BOOT_IRQ_EN_CTRL
		if(state->isp.boot_irq_enabled) {
			disable_irq(state->isp.boot_irq);
			state->isp.boot_irq_enabled = 0;
		}
#endif
	}

	gpio_free(GPIO_ISP_INT2_SH);
	gpio_free(GPIO_AP_ISP_POWER_KEY);
	gpio_free(GPIO_ISP_BOOT_OPT);

#if defined(ISP_RESET_SELECT)
	gpio_free(GPIO_AP_ISP_RESET);
#endif

	return ret;

}
#endif /* M7MU_ENABLE_COLD_POWER */


#ifdef ENABLE_FAST_POWER_OFF
static void m7mu_power_off_func(struct work_struct *work)
{
	int ret;
#ifdef CONFIG_VIDEO_M7MU_SPI
	unsigned int gpio;
#endif
#ifdef M7MU_ENABLE_BOOT_IRQ_EN_CTRL
	struct m7mu_state *state;
#endif

	if(!sd_for_power_off) {
		printk(KERN_ERR "sd_for_power_off is null\n");
		return;
	}
	cam_info("waiting booting interrupt!\n");
	m7mu_wait_boot_interrupt(sd_for_power_off, M7MU_ISP_BOOT_TIMEOUT);
	cam_info("get booting interrupt!\n");
	mdelay(10);
#if defined(ISP_RESET_SELECT)
	ret = gpio_direction_output(GPIO_AP_ISP_RESET, 0);
	if (ret < 0)
		cam_err("failed gpio_direction_output(GPC4_0) value 0\n");
#endif	/* ISP_RESET_SELECT */

#if defined(M7MU_ENABLE_REGULATOR_CTRL_CORE_3V3)
	if(regulator_is_enabled(main_cam_sensor_a2v8_regulator)) {
		regulator_disable(main_cam_sensor_a2v8_regulator);
		if (ret < 0)
			cam_err("failed to disable main_cam_sensor_a2v8_regulator\n");
	} else {
		cam_err("main_cam_sensor_a2v8_regulator is already disabled\n");
	}
	mdelay(2);
#endif /* M7MU_ENABLE_REGULATOR_CTRL */

	if(regulator_is_enabled(main_cam_sensor_1v8_regulator)) {
		regulator_disable(main_cam_sensor_1v8_regulator);
		if (ret < 0)
			cam_err("failed to disable main_cam_sensor_1v8_regulator\n");
	} else {
		cam_err("main_cam_sensor_1v8_regulator is already disabled\n");
	}
	mdelay(2);

	if(regulator_is_enabled(main_cam_sensor_1v2_regulator)) {
		regulator_disable(main_cam_sensor_1v2_regulator);
		if (ret < 0)
			cam_err("failed to disable main_cam_sensor_1v2_regulator\n");
	} else {
		cam_err("main_cam_sensor_1v2_regulator is already disabled\n");
	}
	mdelay(2);

	if(regulator_is_enabled(main_cam_sensor_3v3_regulator)) {
		ret = regulator_disable(main_cam_sensor_3v3_regulator);
		if (ret < 0)
			cam_err("failed to disable main_cam_sensor_3v3_regulator\n");
	} else {
		cam_err("main_cam_sensor_3v3_regulator is already disabled\n");
	}
	cam_err("3.3v regulator off\n");

#ifdef M7MU_ENABLE_BOOT_IRQ_EN_CTRL
	state = to_state(sd_for_power_off);

	if(state->isp.boot_irq_enabled) {
		disable_irq(state->isp.boot_irq);
		state->isp.boot_irq_enabled = 0;
	}
#endif

#ifdef CONFIG_VIDEO_M7MU_SPI
	ret = gpio_request(GPIO_ISP_SPI1_SCK, "ISP_SPI1_SCK");
	if (ret < 0)
		cam_err("failed gpio_request GPIO_ISP_SPI1_SCK\n");
	ret = gpio_request(GPIO_ISP_SPI1_EN, "ISP_SPI1_EN");
	if (ret < 0)
		cam_err("failed gpio_request GPIO_ISP_SPI1_EN\n");
	ret = gpio_request(GPIO_ISP_SPI1_MISO, "ISP_SPI1_MISO");
	if (ret < 0)
		cam_err("failed gpio_request GPIO_ISP_SPI1_MISO\n");
	ret = gpio_request(GPIO_ISP_SPI1_MOSI, "ISP_SPI1_MOSI");
	if (ret < 0)
		cam_err("failed gpio_request GPIO_ISP_SPI1_MOSI\n");

	for (gpio = EXYNOS5260_GPA2(4);
			gpio < EXYNOS5260_GPA2(8); gpio++) {
		s3c_gpio_cfgpin(gpio, S3C_GPIO_INPUT);
		s3c_gpio_setpull(gpio, S3C_GPIO_PULL_NONE);
		s5p_gpio_set_drvstr(gpio, S5P_GPIO_DRVSTR_LV1);
	}

	gpio_free(GPIO_ISP_SPI1_SCK);
	gpio_free(GPIO_ISP_SPI1_EN);
	gpio_free(GPIO_ISP_SPI1_MISO);
	gpio_free(GPIO_ISP_SPI1_MOSI);
#endif
	gpio_free(GPIO_ISP_INT2_SH);
	gpio_free(GPIO_AP_ISP_POWER_KEY);
	gpio_free(GPIO_ISP_BOOT_OPT);

#if defined(ISP_RESET_SELECT)
	gpio_free(GPIO_AP_ISP_RESET);
#endif
	cam_info("Complete power_off function!\n");
}
#endif

/* --2 */
#if 1
static int m7mu_power(struct v4l2_subdev *sd, int flag, int option)
{
	int ret;
#ifdef CONFIG_VIDEO_M7MU_SPI
	unsigned int gpio;
#endif
#ifdef M7MU_ENABLE_BOOT_IRQ_EN_CTRL
	struct m7mu_state *state = to_state(sd);
#endif

#ifdef ENABLE_FAST_POWER_OFF
	if(flag == M7MU_HW_POWER_ON) {
		int timeout = M7MU_ISP_SHORT_TIMEOUT;
		int interval=10;
		int busy = work_busy(&m7mu_power_off_work);
		cam_info("m7mu_power_off_work work_busy : %x\n", busy);
		while(busy) {
			msleep(interval);
			busy = work_busy(&m7mu_power_off_work);
			cam_warn("m7mu_power_off_work work_busy : %x\n", busy);
			timeout -= interval;
			if(timeout < 0) {
				cam_err("waiting m7mu_power_off_work work_busy timeout!\n");
				break;
			}
		}
	}
#endif

	cam_trace("Sensor is power %s\n",
			flag == M7MU_HW_POWER_ON ? "on" : "off");
	cam_trace("power option is %s\n", option == 1 ? "fw_update" : "Normal");

	ret = gpio_request(GPIO_ISP_INT2_SH, "ISP INT2");
	if (ret < 0)
		cam_err("failed gpio_request(GPK0_0) for camera control\n");
	ret = gpio_request(GPIO_ISP_BOOT_OPT, "ISP BOOT OPT");
	if (ret < 0)
		cam_err("failed gpio_request(GPD0_4) for camera control\n");
	ret = gpio_request(GPIO_AP_ISP_POWER_KEY, "ISP POWER KEY");
	if (ret < 0)
		cam_err("failed gpio_request(GPB2_1) for camera control\n");

#if defined(ISP_RESET_SELECT)
	ret = gpio_request(GPIO_AP_ISP_RESET, "ISP RESET");
	if (ret < 0)
		cam_err("failed gpio_request(GPC4_0) for camera control\n");
#endif

	/*
	   Control ISP_INT2 for lens zoom check
	   (0 : lens open, 1 : lens freeze)
	 */
	if (option == 0) {
		/* Normal */
		ret = gpio_direction_output(GPIO_ISP_INT2_SH, 0);
		if (ret < 0)
			cam_err("failed gpio_direction_output(GPD0_4) value 0\n");
	} else {
		/* fw_updata or factory mode */
		ret = gpio_direction_output(GPIO_ISP_INT2_SH, 1);
		if (ret < 0)
			cam_err("failed gpio_direction_output(GPD0_4) value 0\n");
	}

	/* Control ISP Boot Option */
	ret = gpio_direction_output(GPIO_ISP_BOOT_OPT, 0);
	if (ret < 0)
		cam_err("failed gpio_direction_output(GPD0_4) value 0\n");

#ifdef M7MU_ENABLE_BOOT_IRQ_EN_CTRL
	if (flag == M7MU_HW_POWER_ON) {
		if(!state->isp.boot_irq_enabled) {
			enable_irq(state->isp.boot_irq);
			state->isp.boot_irq_enabled = 1;
		}
	}
#endif

#if defined(M7MU_ENABLE_REGULATOR_CTRL)
	/* Control Sensor Power */
	if (flag == M7MU_HW_POWER_ON) {

#if defined(M7MU_ENABLE_REGULATOR_CTRL_CORE_3V3)
		if(!regulator_is_enabled(main_cam_sensor_3v3_regulator)) {
			ret = regulator_enable(main_cam_sensor_3v3_regulator);
			if (ret < 0)
				cam_err("failed to enable main_cam_sensor_3v3_regulator\n");
		} else {
			cam_err("main_cam_sensor_3v3_regulator is already enabled\n");
		}
		mdelay(70);
#endif /* M7MU_ENABLE_REGULATOR_CTRL_CORE_3V3 */
		if(!regulator_is_enabled(main_cam_sensor_1v2_regulator)) {
			ret = regulator_enable(main_cam_sensor_1v2_regulator);
			if (ret < 0)
				cam_err("failed to enable main_cam_sensor_1v2_regulator\n");
		} else {
			cam_err("main_cam_sensor_1v2_regulator is already enabled\n");
		}
		mdelay(5);
		if(!regulator_is_enabled(main_cam_sensor_1v8_regulator)) {
			ret = regulator_enable(main_cam_sensor_1v8_regulator);
			if (ret < 0)
				cam_err("failed to enable main_cam_sensor_1v8_regulator\n");
		} else {
			cam_err("main_cam_sensor_1v8_regulator is already enabled\n");
		}
		mdelay(5);
		if(!regulator_is_enabled(main_cam_sensor_a2v8_regulator)) {
			ret = regulator_enable(main_cam_sensor_a2v8_regulator);
			if (ret < 0)
				cam_err("failed to enable main_cam_sensor_a2v8_regulator\n");
		} else {
			cam_err("main_cam_sensor_a2v8_regulator is already enabled\n");
		}
	} else {
		/*regulator_disable(main_cam_sensor_a2v8_regulator);
		if (ret < 0)
			cam_err("failed to disable main_cam_sensor_a2v8_regulator\n");
		mdelay(5);
		regulator_disable(main_cam_sensor_1v8_regulator);
		if (ret < 0)
			cam_err("failed to disable main_cam_sensor_1v8_regulator\n");
		mdelay(5);
		regulator_disable(main_cam_sensor_1v2_regulator);
		if (ret < 0)
			cam_err("failed to disable main_cam_sensor_1v2_regulator\n");*/

		ret = gpio_direction_output(GPIO_ISP_INT2_SH, 0);
		if (ret < 0)
			cam_err("failed gpio_direction_output(GPD0_4) value 0\n");
	}
#endif /* M7MU_ENABLE_REGULATOR_CTRL */

	/* Control ISP Power Key */
	CLEAR_ISP_BOOT_INT_STATE(sd);
	ret = gpio_direction_output(GPIO_AP_ISP_POWER_KEY, 1);
	cam_err("AP_ISP_POWER_KEY valeu : 1 \n");
	if (ret < 0)
		cam_err("failed gpio_direction_output(GPM2_3) value 1\n");

	mdelay(40);

#if defined(ISP_RESET_SELECT)
	if (flag == M7MU_HW_POWER_ON) {
		ret = gpio_direction_output(GPIO_AP_ISP_RESET, 1);
		cam_err("AP_ISP_RESET valeu : 1 \n");
		if (ret < 0)
			cam_err("failed gpio_direction_output(GPC4_0) value 1\n");
	}
#endif

	mdelay(60);

	ret = gpio_direction_output(GPIO_AP_ISP_POWER_KEY, 0);
	cam_err("AP_ISP_POWER_KEY valeu : 0 \n");
	if (ret < 0)
		cam_err("failed gpio_direction_output(GPB2_1) value 0\n");

#ifdef ENABLE_FAST_POWER_OFF
	if(flag == M7MU_HW_POWER_OFF && fast_power_off_flag) {
		cam_info("power_off workque is triggered\n");
		sd_for_power_off = sd;
		fast_power_off_flag = 0;
		if(queue_work(m7mu_power_off_wq, &m7mu_power_off_work) == 0) {
			cam_err("queue_work m7mu_power_off_wq fail. Do normal power off\n");
		} else {
			return ret;
		}
	}
#endif

	if (flag == M7MU_HW_POWER_OFF) {
		m7mu_wait_boot_interrupt(sd, M7MU_ISP_BOOT_TIMEOUT);

		mdelay(10);
#if defined(ISP_RESET_SELECT)
		ret = gpio_direction_output(GPIO_AP_ISP_RESET, 0);
		cam_err("AP_ISP_RESET valeu : 0 \n");
		if (ret < 0)
			cam_err("failed gpio_direction_output(GPC4_0) value 0\n");
#endif	/* ISP_RESET_SELECT */

#if defined(M7MU_ENABLE_REGULATOR_CTRL_CORE_3V3)
		if(regulator_is_enabled(main_cam_sensor_a2v8_regulator)) {
			regulator_disable(main_cam_sensor_a2v8_regulator);
			if (ret < 0)
				cam_err("failed to disable main_cam_sensor_a2v8_regulator\n");
		} else {
			cam_err("main_cam_sensor_a2v8_regulator is already disabled\n");
		}
		mdelay(2);
#endif /* M7MU_ENABLE_REGULATOR_CTRL */

		if(regulator_is_enabled(main_cam_sensor_1v8_regulator)) {
			regulator_disable(main_cam_sensor_1v8_regulator);
			if (ret < 0)
				cam_err("failed to disable main_cam_sensor_1v8_regulator\n");
		} else {
			cam_err("main_cam_sensor_1v8_regulator is already disabled\n");
		}
		mdelay(2);

		if(regulator_is_enabled(main_cam_sensor_1v2_regulator)) {
			regulator_disable(main_cam_sensor_1v2_regulator);
			if (ret < 0)
				cam_err("failed to disable main_cam_sensor_1v2_regulator\n");
		} else {
			cam_err("main_cam_sensor_1v2_regulator is already disabled\n");
		}
		mdelay(2);

		if(regulator_is_enabled(main_cam_sensor_3v3_regulator)) {
			ret = regulator_disable(main_cam_sensor_3v3_regulator);
			if (ret < 0)
				cam_err("failed to disable main_cam_sensor_3v3_regulator\n");
		} else {
			cam_err("main_cam_sensor_3v3_regulator is already disabled\n");
		}
		cam_err("3.3v regulator off\n");

#ifdef M7MU_ENABLE_BOOT_IRQ_EN_CTRL
		if(state->isp.boot_irq_enabled) {
			disable_irq(state->isp.boot_irq);
			state->isp.boot_irq_enabled = 0;
		}
#endif
	}

#ifdef CONFIG_VIDEO_M7MU_SPI
	ret = gpio_request(GPIO_ISP_SPI1_SCK, "ISP_SPI1_SCK");
	if (ret < 0)
		cam_err("failed gpio_request GPIO_ISP_SPI1_SCK\n");
	ret = gpio_request(GPIO_ISP_SPI1_EN, "ISP_SPI1_EN");
	if (ret < 0)
		cam_err("failed gpio_request GPIO_ISP_SPI1_EN\n");
	ret = gpio_request(GPIO_ISP_SPI1_MISO, "ISP_SPI1_MISO");
	if (ret < 0)
		cam_err("failed gpio_request GPIO_ISP_SPI1_MISO\n");
	ret = gpio_request(GPIO_ISP_SPI1_MOSI, "ISP_SPI1_MOSI");
	if (ret < 0)
		cam_err("failed gpio_request GPIO_ISP_SPI1_MOSI\n");

	if (flag == M7MU_HW_POWER_ON) {
		gpio_direction_output(GPIO_ISP_SPI1_EN, 1);
		s3c_gpio_cfgpin(GPIO_ISP_SPI1_EN, S3C_GPIO_SFN(1));
		s3c_gpio_setpull(GPIO_ISP_SPI1_EN, S3C_GPIO_PULL_UP);

		s3c_gpio_cfgpin(EXYNOS5260_GPA2(4), S3C_GPIO_SFN(2));
		s3c_gpio_setpull(EXYNOS5260_GPA2(4), S3C_GPIO_PULL_UP);
		s3c_gpio_cfgall_range(EXYNOS5260_GPA2(6), 2,
				      S3C_GPIO_SFN(2), S3C_GPIO_PULL_UP);

		for (gpio = EXYNOS5260_GPA2(4);
				gpio < EXYNOS5260_GPA2(8); gpio++)
			s5p_gpio_set_drvstr(gpio, S5P_GPIO_DRVSTR_LV3);
	} else {
		for (gpio = EXYNOS5260_GPA2(4);
				gpio < EXYNOS5260_GPA2(8); gpio++) {
			s3c_gpio_cfgpin(gpio, S3C_GPIO_INPUT);
			s3c_gpio_setpull(gpio, S3C_GPIO_PULL_NONE);
			s5p_gpio_set_drvstr(gpio, S5P_GPIO_DRVSTR_LV1);
		}
	}
	gpio_free(GPIO_ISP_SPI1_SCK);
	gpio_free(GPIO_ISP_SPI1_EN);
	gpio_free(GPIO_ISP_SPI1_MISO);
	gpio_free(GPIO_ISP_SPI1_MOSI);
#endif
	gpio_free(GPIO_ISP_INT2_SH);
	gpio_free(GPIO_AP_ISP_POWER_KEY);
	gpio_free(GPIO_ISP_BOOT_OPT);

#if defined(ISP_RESET_SELECT)
	gpio_free(GPIO_AP_ISP_RESET);
#endif

	return ret;

}
#else /* foreced ISP fwup (vacant ISP) */
static int m7mu_power(struct v4l2_subdev *sd, int flag, int option)
{
	int ret = 0;

	cam_trace("Sensor is power %s\n",
			flag == M7MU_HW_POWER_ON ? "on" : "off");

	if (gpio_request(GPIO_ISP_BOOT_OPT, "ISP BOOT OPT") < 0)
		cam_err("failed gpio_request(GPC0_2) for camera control\n");

	if (gpio_direction_output(GPIO_ISP_BOOT_OPT, 1) < 0)
		cam_err("failed gpio_direction_output(GPC0_2) value 0\n");

	mdelay(200);

	if (gpio_request(GPIO_AP_ISP_POWER_KEY, "GPM2_3") < 0)
		cam_err("failed gpio_request(GPM2_3) for camera control\n");

	if (gpio_direction_output(GPIO_AP_ISP_POWER_KEY, 1) < 0)
		cam_err("failed gpio_direction_output(GPM2_3) value 1\n");

	if (flag == M7MU_HW_POWER_ON) {
		ret = regulator_enable(main_cam_core_1v2_regulator);
		if (ret < 0)
			cam_err("failed to enable main_cam_core_1v2_regulator\n");
		mdelay(5);
		ret = regulator_enable(main_cam_io_1v8_regulator);
		if (ret < 0)
			cam_err("failed to enable main_cam_io_1v8_regulator\n");
		mdelay(5);
		ret = regulator_enable(main_cam_sensor_a2v8_regulator);
		if (ret < 0)
			cam_err("failed to enable main_cam_sensor_a2v8_regulator\n");
		mdelay(200);
	}

	if (gpio_direction_output(GPIO_AP_ISP_POWER_KEY, 0) < 0)
		cam_err("failed gpio_direction_output(GPM2_3) value 1\n");

	if (m7mu_wait_boot_interrupt(sd, M7MU_ISP_TIMEOUT)) {
		cam_err("boot time out  err\n");
		return -1;
	}

	/* FIXME error check */
	m7mu_load_fw_main(sd);

	if (gpio_direction_output(GPIO_ISP_BOOT_OPT, 0) < 0)
		cam_err("failed gpio_direction_output(GPC0_2) value 0\n");

	mdelay(100);

	if (gpio_direction_output(GPIO_AP_ISP_POWER_KEY, 1) < 0)
		cam_err("failed gpio_direction_output(GPM2_3) value 1\n");

	mdelay(100);

	if (gpio_direction_output(GPIO_AP_ISP_POWER_KEY, 0) < 0)
		cam_err("failed gpio_direction_output(GPM2_3) value 1\n");

	gpio_free(GPIO_ISP_BOOT_OPT);
	gpio_free(GPIO_AP_ISP_POWER_KEY);

	return 0;
}
#endif
static int m7mu_init_formats(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh);

/* --1 */
static int m7mu_s_power(struct v4l2_subdev *sd, int on)
{
	struct i2c_client *client = to_client(sd);
	struct m7mu_state *state = to_state(sd);
	int ret = 0;

	cam_info(" E");

	dev_info(&client->dev, "%s: %d\n", __func__, __LINE__);
	if (on) {

		/*
		   Power On after setting V4L2_CID_CAMERA_INIT
		   For decreasing ISP booting time
		 */
#if 0
		ret = m7mu_power(sd, M7MU_HW_POWER_ON, 0);

		if (ret == -EIO) {
			m7mu_power(sd, M7MU_HW_POWER_OFF, 0);
			return ret;
		}

		state->power_on = M7MU_HW_POWER_ON;
#endif
	} else {
		if (m7mu_deinit(sd)) {
			dev_err(&client->dev,
				"%s: Failed deinit.\n", __func__);
			/* return -EIO; */
		}
		if (m7mu_which_power_off() == 0)
			ret = m7mu_power(sd, M7MU_HW_POWER_OFF, 0);

		state->power_on = M7MU_HW_POWER_OFF;
	}
	return ret;
}

static const struct v4l2_subdev_core_ops m7mu_core_ops = {
	.s_power	= m7mu_s_power,
	.init		= m7mu_pre_init,		/* initializing API */
	.load_fw	= m7mu_fw_writing,
	.queryctrl	= m7mu_queryctrl,
	.g_ctrl		= m7mu_g_ctrl,
	.s_ctrl		= m7mu_s_ctrl,
	.noti_ctrl  = m7mu_noti_ctrl,
	.g_ext_ctrls	= m7mu_g_ext_ctrls,
	.s_ext_ctrls	= m7mu_s_ext_ctrls,
};

static const struct v4l2_subdev_video_ops m7mu_video_ops = {
	.s_mbus_fmt = m7mu_s_fmt,
	.g_parm = m7mu_g_parm,
	.s_parm = m7mu_s_parm,
	.enum_framesizes = m7mu_enum_framesizes,
	.s_stream = m7mu_s_stream,
};

/* get format by flite video device command */
static int m7mu_get_fmt(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh,
			  struct v4l2_subdev_format *fmt)
{
	struct m7mu_state *state = to_state(sd);

	if (fmt->pad != 0)
		return -EINVAL;

	if (state->oprmode == M7MU_OPRMODE_VIDEO) {
		fmt->format.width = state->preview->target_width;
		fmt->format.height = state->preview->target_height;
	} else if (state->oprmode == M7MU_OPRMODE_IMAGE) {
		if (!m7mu_check_postview(sd)) {
			fmt->format.width = state->capture->target_width;
			fmt->format.height = state->capture->target_height;
		} else {
			fmt->format.width = state->postview->target_width;
			fmt->format.height = state->postview->target_height;
		}
	} else { /* TODO:  HDR */
		fmt->format.width = state->capture->target_width;
		fmt->format.height = state->capture->target_height;
	}

	/* fmt->format = *format; */

	return 0;
}

/* set format by flite video device command */
/* --6 */
static int m7mu_set_fmt(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh,
			  struct v4l2_subdev_format *fmt)
{
	struct i2c_client *client = to_client(sd);
	struct m7mu_state *state = to_state(sd);
	struct v4l2_mbus_framefmt *format = &fmt->format;
	struct v4l2_mbus_framefmt *sfmt;
	enum m7mu_oprmode type;
	u32 resolution = 0;
/*	int ret; */

	dev_info(&client->dev, "%s: E", __func__);
	if (fmt->pad != 0)
		return -EINVAL;

	cam_info("%s: fmt->format.width = %d, fmt->format.height = %d",
			__func__, fmt->format.width, fmt->format.height);

	type         = state->oprmode;
	sfmt         = &default_fmt[type];
	sfmt->width  = format->width;
	sfmt->height = format->height;
	cam_info("%s: sfmt->width = %d, sfmt->height = %d", __func__,
			sfmt->width, sfmt->height);

	if (fmt->which == V4L2_SUBDEV_FORMAT_ACTIVE) {
		/* for enum size of entity by flite */
		state->ffmt[type].width  = format->width;
		state->ffmt[type].height = format->height;
#ifndef CONFIG_VIDEO_M7MU_SENSOR_JPEG
		state->ffmt[type].code   = V4L2_MBUS_FMT_YUYV8_2X8;
#else
		state->ffmt[type].code   = format->code;
#endif

		/* find adaptable resolution */
		state->resolution       = resolution;
#ifndef CONFIG_VIDEO_M7MU_SENSOR_JPEG
		state->code             = V4L2_MBUS_FMT_YUYV8_2X8;
#else
		state->code             = format->code;
#endif
		state->res_type         = type;

		/* for set foramat */
		state->pix.width        = format->width;
		state->pix.height       = format->height;

		if ((state->power_on == M7MU_HW_POWER_ON)
		    && (state->runmode != M7MU_RUNMODE_CAPTURE))
			m7mu_s_fmt(sd, sfmt);  /* set format */
	}

	dev_info(&client->dev, "%s: X", __func__);
	return 0;
}

/* enum code by flite video device command */
static int m7mu_enum_mbus_code(struct v4l2_subdev *sd,
				 struct v4l2_subdev_fh *fh,
				 struct v4l2_subdev_mbus_code_enum *code)
{
	if (!code || code->index >= SIZE_DEFAULT_FFMT)
		return -EINVAL;

	code->code = default_fmt[code->index].code;

	return 0;
}

static struct v4l2_subdev_pad_ops m7mu_pad_ops = {
	.enum_mbus_code	= m7mu_enum_mbus_code,
	.get_fmt	= m7mu_get_fmt,
	.set_fmt	= m7mu_set_fmt,
};

static const struct v4l2_subdev_ops m7mu_ops = {
	.core	= &m7mu_core_ops,
	.pad	= &m7mu_pad_ops,
	.video	= &m7mu_video_ops,
};

static int m7mu_link_setup(struct media_entity *entity,
			    const struct media_pad *local,
			    const struct media_pad *remote, u32 flags)
{
	pr_debug("%s\n", __func__);
	return 0;
}

static const struct media_entity_operations m7mu_media_ops = {
	.link_setup = m7mu_link_setup,
};

/* internal ops for media controller */
/* --5 */
static int m7mu_init_formats(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh)
{
	struct v4l2_subdev_format format;
	struct i2c_client *client = to_client(sd);
	struct m7mu_state *state = to_state(sd);

	dev_err(&client->dev, "%s:\n", __func__);
	memset(&format, 0, sizeof(format));
	format.pad = 0;
	format.which = fh ? V4L2_SUBDEV_FORMAT_TRY : V4L2_SUBDEV_FORMAT_ACTIVE;
	format.format.code = DEFAULT_SENSOR_CODE;
	format.format.width = DEFAULT_SENSOR_WIDTH;
	format.format.height = DEFAULT_SENSOR_HEIGHT;

	m7mu_set_fmt(sd, fh, &format);
	m7mu_s_parm(sd, &state->strm);

	return 0;
}

int sensor_m7m_stream_on(struct v4l2_subdev *subdev)
{
	int ret = 0;
	cam_info("stream on\n");

	return ret;
}

int sensor_m7m_stream_off(struct v4l2_subdev *subdev)
{
	int ret = 0;
	cam_info("stream off\n");
	return ret;
}

/*
 * @ brief
 * frame duration time
 * @ unit
 * nano second
 * @ remarks
 */
int sensor_m7mu_ops_set(struct v4l2_subdev *subdev, u64 value)
{
	int ret = 0;
	return ret;
}
int sensor_m7mu_ops_get(struct v4l2_subdev *subdev)
{
	int ret = 0;
	return ret;
}

static irqreturn_t m7mu_isp_isr(int irq, void *dev_id)
{
	struct v4l2_subdev *sd = (struct v4l2_subdev *)dev_id;
	struct m7mu_state *state = to_state(sd);

	cam_trace("**************** interrupt ****************\n");
	state->isp.issued = 1;
	wake_up(&state->isp.wait);

	return IRQ_HANDLED;
}

static int m7mu_wait_boot_interrupt(struct v4l2_subdev *sd,
		unsigned int timeout)
{
	struct m7mu_state *state = to_state(sd);

	cam_info("Wait ISP Boot Complete ~~~ : wait start\n");
	if (wait_event_timeout(state->isp.boot_wait,
			state->isp.boot_issued == 1,
			msecs_to_jiffies(timeout)) == 0) {
		cam_info("~~~ waiting boot timeout ~~~ %d\n", gpio_get_value(GPIO_ISP_STATUS_INT));
		return -ENOSYS;
	}
	cam_info("Wait ISP Boot Complete ~~~ : wait end\n");
	state->isp.boot_issued = 0;

	return 0;
}

static irqreturn_t m7mu_isp_boot_isr(int irq, void *dev_id)
{
	struct v4l2_subdev *sd = (struct v4l2_subdev *)dev_id;
	struct m7mu_state *state = to_state(sd);
	bool b_rising = gpio_get_value(GPIO_ISP_STATUS_INT);
	bool b_ignore = false;

	if(state->isp.boot_issued_rising && b_rising == false) {
		b_ignore = true;
	}
	state->isp.boot_issued_rising = b_rising;

	cam_trace("**************** isp boot interrupt **************** %s %s\n", b_rising ? "rising":"falling", b_ignore? "ignored":"");

	if(b_ignore == false) {
		state->isp.boot_issued = 1;
		wake_up(&state->isp.boot_wait);
	}

	return IRQ_HANDLED;
}

static ssize_t m7mu_register_irq(struct v4l2_subdev *sd)
{
	struct m7mu_state *state = to_state(sd);
	int err = 0;
	int ret = 0;

	printk(KERN_INFO "m7mu_config_isp_irq\n");
	gpio_request(GPIO_ISP_INT1_SH, "M7MU IRQ");
	s3c_gpio_setpull(GPIO_ISP_INT1_SH, S3C_GPIO_PULL_DOWN);

	ret = s5p_register_gpio_interrupt(GPIO_ISP_INT1_SH);
	if (ret < 0) {
		printk(KERN_ERR "%s: s5p_register_gpio_interrupt is failed. %d",
				__func__, ret);
	}

	status_irq = gpio_to_irq(GPIO_ISP_INT1_SH);
	state->isp.issued = 0;

	/* wait queue initialize */
	init_waitqueue_head(&state->isp.wait);

	err = request_irq(status_irq,
		m7mu_isp_isr, IRQF_TRIGGER_RISING, "m7mu isp", sd);
	if (err) {
		cam_err("failed to request irq ~~~~~~~~~~~~~\n");
		return err;
	}

	s3c_gpio_cfgpin(GPIO_ISP_INT1_SH, S3C_GPIO_SFN(0xF));
	gpio_free(GPIO_ISP_INT1_SH);

	state->isp.irq = status_irq;

	printk(KERN_INFO "m7mu_config_boot_isp_irq\n");
	gpio_request(GPIO_ISP_STATUS_INT, "M7MU BOOT IRQ");
	s3c_gpio_setpull(GPIO_ISP_STATUS_INT, S3C_GPIO_PULL_DOWN);

	ret = s5p_register_gpio_interrupt(GPIO_ISP_STATUS_INT);
	if (ret < 0) {
		printk(KERN_ERR "%s: s5p_register_gpio_interrupt is failed. %d",
				__func__, ret);
	}

	/* set boot_int to irq */
	boot_irq = gpio_to_irq(GPIO_ISP_STATUS_INT);
	state->isp.boot_issued = 0;

	/* wait queue initialize */
	init_waitqueue_head(&state->isp.boot_wait);

#ifdef M7MU_ENABLE_BOOT_IRQ_EN_CTRL
	irq_set_status_flags(boot_irq, IRQ_NOAUTOEN);
	state->isp.boot_irq_enabled = 0;
#endif
	err = request_irq(boot_irq,
		m7mu_isp_boot_isr, IRQF_TRIGGER_RISING |IRQF_TRIGGER_FALLING, "m7mu boot isp", sd);
	if (err) {
		cam_err("failed to request boot irq ~~~~~~~~~~~~~\n");
		return err;
	}

	s3c_gpio_cfgpin(GPIO_ISP_STATUS_INT, S3C_GPIO_SFN(0xF));
	gpio_free(GPIO_ISP_STATUS_INT);

	state->isp.boot_irq = boot_irq;

	return err;
}

static ssize_t m7mu_camera_type_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%s\n", sysfs_sensor_type);
}

static ssize_t m7mu_camera_fw_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%s %s\n", sysfs_sensor_fw, sysfs_phone_fw);
}

static ssize_t m7mu_check_app_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	/* FIXME : simple_strtoul is obsolete, use kstrtoul instead */
	int v = simple_strtoul(buf, NULL, 10);
	sysfs_check_app = v;
	return count;
}

static ssize_t m7mu_camera_close(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct v4l2_subdev *sd;
	const struct m7mu_platform_data *pdata;

	if (tempclient == NULL) {
		cam_err("i2c client is NULL!!!!\n");
		return count;
	}

	if (m7mu_Lens_Off_Needed == 0) {
		cam_err("Lens close. Already\n");
		return count;
	}

	sd = i2c_get_clientdata(tempclient);
	pdata = tempclient->dev.platform_data;

	cam_trace("E\n");

	/* AF LED regulator off */
	if (pdata != NULL && pdata->af_led_power != NULL)
		pdata->af_led_power(0);

	/* Lens Close */
	if (m7mu_set_lens_off(sd) < 0)
		cam_err("failed to set m7mu_set_lens_off~~~~~\n");
	cam_trace("x\n");
	return count;
}

static ssize_t m7mu_cancelwait_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct m7mu_state *state;

	if (tempsubdev == NULL) {
		printk(KERN_ERR "m7mu_cancelwait_store tempsubdev is NULL!!\n");
		return count;
	}

	state = to_state(tempsubdev);

	if (state == NULL) {
		printk(KERN_ERR "m7mu_cancelwait_store state is NULL!!\n");
		return count;
	}

	printk(KERN_ERR "m7mu_cancelwait_store E %s\n", buf);
	/* cancel capture of Long_AE */
	printk(KERN_ERR "m7mu_cancelwait_store Wake-up\n");
	state->isp.issued = 1;
	wake_up(&state->isp.wait);

	state->isp.sound_issued = 1;
	wake_up(&state->isp.sound_wait);
	printk(KERN_ERR "m7mu_cancelwait_store X\n");
	return count;
}

static ssize_t m7mu_quickshot(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t count)
{
	printk(KERN_DEBUG "%s:%d: %s\n", __func__, __LINE__, buf);

	m7mu_quickshot_called = 1;

	if (!strcmp(buf, "quickshotlcok")) {
		m7mu_quickshot_on = 1;
		m7mu_quickshot_holdlens = 0;
	} else if (!strcmp(buf, "quickshotunlock")) {
		m7mu_quickshot_on = 1;
		m7mu_quickshot_holdlens = 1;
	} else {
		m7mu_quickshot_on = 0;
	}

	return count;
}

static DEVICE_ATTR(rear_camtype, S_IRUGO, m7mu_camera_type_show, NULL);
static DEVICE_ATTR(rear_camfw, S_IRUGO, m7mu_camera_fw_show, NULL);
static DEVICE_ATTR(rear_camclose, S_IRUGO|S_IWUSR, NULL, m7mu_camera_close);
static DEVICE_ATTR(rear_checkApp, S_IRUGO|S_IWUSR, NULL, m7mu_check_app_store);
static DEVICE_ATTR(rear_cancelwait,
		S_IRUGO|S_IWUSR, NULL, m7mu_cancelwait_store);
static DEVICE_ATTR(rear_quickshot, S_IRUGO|S_IWUSR, NULL, m7mu_quickshot);


struct fimc_is_sensor_ops module_m7m_ops = {
	.stream_on	= sensor_m7m_stream_on,
	.stream_off	= sensor_m7m_stream_off,
	.s_duration	= sensor_m7mu_ops_set,
	.g_min_duration	= sensor_m7mu_ops_get,
	.g_max_duration	= sensor_m7mu_ops_get,
	.s_exposure	= sensor_m7mu_ops_set,
	.g_min_exposure	= sensor_m7mu_ops_get,
	.g_max_exposure	= sensor_m7mu_ops_get,
	.s_again	= sensor_m7mu_ops_set,
	.g_min_again	= sensor_m7mu_ops_get,
	.g_max_again	= sensor_m7mu_ops_get,
	.s_dgain	= sensor_m7mu_ops_get,
	.g_min_dgain	= sensor_m7mu_ops_get,
	.g_max_dgain	= sensor_m7mu_ops_get
};

static struct fimc_is_sensor_cfg config_m7mu[] = {
	/* 1456x1090@24fps */
	FIMC_IS_SENSOR_CFG(1456, 1090, 24, 13, 0),
	/* 1936x1090@24fps */
	FIMC_IS_SENSOR_CFG(1936, 1090, 24, 13, 1),
	/* 1456x1090@30fps */
	FIMC_IS_SENSOR_CFG(1456, 1090, 30, 16, 2),
	/* 1936x1090@30fps */
	FIMC_IS_SENSOR_CFG(1456, 1090, 30, 16, 3),
	/* 1456x1090@24fps */
	FIMC_IS_SENSOR_CFG(1456, 1090, 24, 13, 4),
	/* 1456x1090@15fps */
	FIMC_IS_SENSOR_CFG(1456, 1090, 15, 13, 5),
	/* 1456x1090@15fps */
	FIMC_IS_SENSOR_CFG(960, 720, 0, 12, 5)
};

static struct fimc_is_vci vci_m7mu[] = {
	{
		.pixelformat = V4L2_PIX_FMT_YUYV,
		.vc_map = {2, 1, 0, 3}
	}, {
		.pixelformat = V4L2_PIX_FMT_JPEG,
		.vc_map = {0, 2, 1, 3}
	}
};

int m7mu_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	int ret = 0;
	struct fimc_is_core *core;
	struct v4l2_subdev *subdev_module;
	struct fimc_is_module_enum *module;
	struct fimc_is_device_sensor *device;
	struct m7mu_state *state;

	cam_info("(%d)\n", ret);

	BUG_ON(!fimc_is_dev);

	core = (struct fimc_is_core *)dev_get_drvdata(fimc_is_dev);
	if (!core) {
		err("core device is not yet probed");
		return -EPROBE_DEFER;
	}

	device = &core->sensor[SENSOR_M7MU_INSTANCE];

	subdev_module = kzalloc(sizeof(struct v4l2_subdev), GFP_KERNEL);
	if (!subdev_module) {
		err("subdev_module is NULL");
		ret = -ENOMEM;
		goto p_err;
	}

	module = &device->module_enum[atomic_read(
			&core->resourcemgr.rsccount_module)];
	atomic_inc(&core->resourcemgr.rsccount_module);
	module->id = SENSOR_M7MU_NAME;
	module->subdev = subdev_module;
	module->device = SENSOR_M7MU_INSTANCE;
	module->ops = &module_m7m_ops;
	module->client = client;
	module->pixel_width = 1920 + 16;
	module->pixel_height = 1080 + 10;
	module->active_width = 1920;
	module->active_height = 1080;
	module->max_framerate = 120;
	module->position = SENSOR_POSITION_REAR;
	module->mode = CSI_MODE_VC_ONLY;
	module->lanes = CSI_DATA_LANES_4;
	module->vcis = ARRAY_SIZE(vci_m7mu);
	module->vci = vci_m7mu;
	module->setfile_name = "setfile_m7m.bin";
	module->cfgs = ARRAY_SIZE(config_m7mu);
	module->cfg = config_m7mu;
	module->private_data = kzalloc(sizeof(struct m7mu_state), GFP_KERNEL);
	if (!module->private_data) {
		err("private_data is NULL");
		ret = -ENOMEM;
		kfree(subdev_module);
		goto p_err;
	}

#if defined(M7MU_ENABLE_REGULATOR_CTRL)
	/* Getting regulators about ISP power */
#if defined(M7MU_ENABLE_REGULATOR_CTRL_CORE_3V3)
	main_cam_sensor_3v3_regulator = regulator_get(NULL,
			"main_cam_pwr_3v3");
	if (IS_ERR(main_cam_sensor_3v3_regulator)) {
		cam_err("%s: failed to get %s\n", __func__, "main_cam_3v3");
		ret = -ENODEV;
		goto err_regulator;
	}
#endif /* M7MU_ENABLE_REGULATOR_CTRL_CORE_3V3 */
	main_cam_sensor_1v8_regulator = regulator_get(NULL,
			"VT Camera Core (1.8V)");
	if (IS_ERR(main_cam_sensor_1v8_regulator)) {
		cam_err("%s: failed to get %s\n", __func__, "main_cam_1v8");
		ret = -ENODEV;
		goto err_regulator;
	}
	main_cam_sensor_1v2_regulator = regulator_get(NULL,
			"main_cam_core_1v2");
	if (IS_ERR(main_cam_sensor_1v2_regulator)) {
		cam_err("%s: failed to get %s\n", __func__, "main_cam_1v2");
		ret = -ENODEV;
		goto err_regulator;
	}
	main_cam_sensor_a2v8_regulator = regulator_get(NULL,
			"Camera Sensor (2.8V)");
	if (IS_ERR(main_cam_sensor_a2v8_regulator)) {
		cam_err("%s: failed to get %s\n", __func__, "main_cam_2v8");
		ret = -ENODEV;
		goto err_regulator;
	}
#endif /* M7MU_ENABLE_REGULATOR_CTRL */

	state = (struct m7mu_state *)module->private_data;

	mutex_init(&m7mu_lock);
	mutex_init(&state->ctrl_lock);
	init_completion(&state->af_complete);

#ifdef ENABLE_FAST_POWER_OFF
	if(!m7mu_power_off_wq){
		m7mu_power_off_wq = create_workqueue("m7mu_power_off_wq");
		if(m7mu_power_off_wq == NULL) {
			cam_err("create_workqueue m7mu_power_off_wq fail\n");
			ret = -ENOMEM;
			kfree(subdev_module);
			goto p_err;
		} else {
			cam_info("m7mu_power_off_wq create\n");
		}
	}
#endif

	state->runmode = M7MU_RUNMODE_NOTREADY;
	state->dbg_level =  CAM_DEBUG | CAM_TRACE | CAM_I2C;

#ifdef DEFAULT_S5K4EC_DRIVING
	v4l2_i2c_subdev_init(subdev_module, client, &m7mu_ops);
	cam_info("DEFAULT_S5K4EC_DRIVING");
#else
	v4l2_subdev_init(subdev_module, &m7mu_ops);
	cam_info("DEFAULT_M7MU_DRIVING");
#endif
	v4l2_set_subdevdata(subdev_module, module);
	v4l2_set_subdev_hostdata(subdev_module, device);
	snprintf(subdev_module->name, V4L2_SUBDEV_NAME_SIZE,
			"sensor-subdev.%d", module->id);
	cam_info("name : %s", subdev_module->name);

	m7mu_register_irq(subdev_module);

	tempclient = client;
	tempsubdev = subdev_module;

	/* For closing invalid lens open */
#if defined(M7MU_ENABLE_COLD_POWER)
	m7mu_cold_power(subdev_module, M7MU_HW_POWER_ON);
#else
	m7mu_power(subdev_module, M7MU_HW_POWER_ON, 0);
#endif

	/* wait queue initialize */
	init_waitqueue_head(&state->isp.boot_wait);
	state->isp.boot_issued = 0;
	if (m7mu_wait_boot_interrupt(subdev_module, M7MU_ISP_BOOT_TIMEOUT) < 0) {
		cam_err("boot time out err\n");
		/* return -1; */
	}

#if defined(M7MU_ENABLE_COLD_POWER)
	m7mu_cold_power(subdev_module, M7MU_HW_POWER_OFF);
#else
	m7mu_power(subdev_module, M7MU_HW_POWER_OFF, 0);
#endif

p_err:
	cam_info("(%d)\n", ret);
	return ret;

#if defined(M7MU_ENABLE_REGULATOR_CTRL)
err_regulator:
	kfree(subdev_module);
	cam_err("regulator");
#if defined(M7MU_ENABLE_REGULATOR_CTRL_CORE_3V3)
	regulator_put(main_cam_sensor_3v3_regulator);
#endif /* M7MU_ENABLE_REGULATOR_CTRL_CORE_3V3 */
	regulator_put(main_cam_sensor_a2v8_regulator);
	regulator_put(main_cam_sensor_1v2_regulator);
	regulator_put(main_cam_sensor_1v8_regulator);
	return ret;
#endif /* M7MU_ENABLE_REGULATOR_CTRL */
}

static int m7mu_remove(struct i2c_client *client)
{
	int ret = 0;
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct m7mu_state *state = to_state(sd);

	cam_trace("regulator put\n");
#if defined(M7MU_ENABLE_REGULATOR_CTRL_CORE_3V3)
	regulator_put(main_cam_sensor_3v3_regulator);
#endif /* M7MU_ENABLE_REGULATOR_CTRL_CORE_3V3 */
	regulator_put(main_cam_sensor_a2v8_regulator);
	regulator_put(main_cam_sensor_1v2_regulator);
	regulator_put(main_cam_sensor_1v8_regulator);

	if (state->isp.irq > 0)
		free_irq(state->isp.irq, sd);

	if (state->isp.boot_irq > 0)
		free_irq(state->isp.boot_irq, sd);

	mutex_destroy(&m7mu_lock);
	v4l2_device_unregister_subdev(sd);

	kfree(state);
	tempclient = NULL;

#ifdef ENABLE_FAST_POWER_OFF
	if(m7mu_power_off_wq) {
		destroy_workqueue(m7mu_power_off_wq);
		m7mu_power_off_wq = NULL;
	}
#endif


	return ret;
}

#ifdef CONFIG_OF
static const struct of_device_id exynos_fimc_is_sensor_m7mu_match[] = {
	{
		.compatible = "samsung,exynos5-fimc-is-sensor-m7mu",
	},
	{},
};
#endif

static const struct i2c_device_id sensor_m7mu_idt[] = {
	{ SENSOR_NAME, 0 },
};

static struct i2c_driver sensor_m7mu_driver = {
	.driver = {
		.name	= SENSOR_NAME,
		.owner	= THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = exynos_fimc_is_sensor_m7mu_match
#endif
	},
	.probe	= m7mu_probe,
	.remove	= m7mu_remove,
	.id_table = sensor_m7mu_idt
};

static int __init m7mu_load(void)
{
	if (!camera_rear_dev) {
		camera_rear_dev = device_create(camera_class,
				NULL, 0, NULL, "rear");

		if (device_create_file
		(camera_rear_dev, &dev_attr_rear_camtype) < 0) {
			cam_err("failed to create device file, %s\n",
			dev_attr_rear_camtype.attr.name);
		}
		if (device_create_file
		(camera_rear_dev, &dev_attr_rear_camfw) < 0) {
			cam_err("failed to create device file, %s\n",
			dev_attr_rear_camfw.attr.name);
		}
		if (device_create_file
		(camera_rear_dev, &dev_attr_rear_checkApp) < 0) {
			cam_err("failed to create device file, %s\n",
			dev_attr_rear_checkApp.attr.name);
		}
		if (device_create_file
		(camera_rear_dev, &dev_attr_rear_camclose) < 0) {
			cam_err("failed to create device file, %s\n",
			dev_attr_rear_camclose.attr.name);
		}
		if (device_create_file
		(camera_rear_dev, &dev_attr_rear_cancelwait) < 0) {
			cam_err("failed to create device file, %s\n",
			dev_attr_rear_cancelwait.attr.name);
		}
		if (device_create_file
		(camera_rear_dev, &dev_attr_rear_quickshot) < 0) {
			cam_err("failed to create device file, %s\n",
			dev_attr_rear_quickshot.attr.name);
		}
	}

	return i2c_add_driver(&sensor_m7mu_driver);
}

static void __exit m7mu_unload(void)
{
	i2c_del_driver(&sensor_m7mu_driver);
}

module_init(m7mu_load);
module_exit(m7mu_unload);

MODULE_AUTHOR("Camera System");
MODULE_DESCRIPTION("Sensor m7mu driver");
MODULE_LICENSE("GPL v2");

