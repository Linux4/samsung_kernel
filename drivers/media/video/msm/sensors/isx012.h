/* Copyright (c) 2011, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __ISX012_H__
#define __ISX012_H__

#include "msm_sensor.h"
#define SENSOR_NAME "isx012"
#define PLATFORM_DRIVER_NAME "msm_camera_isx012"
#define isx012_obj isx012_##obj

#include <linux/types.h>
#include <mach/board.h>

#undef CONFIG_LOAD_FILE
/*#define CONFIG_LOAD_FILE*/

#undef DEBUG_LEVEL_HIGH
#undef DEBUG_LEVEL_MID
#define DEBUG_LEVEL_HIGH
/*#define DEBUG_LEVEL_MID */

#if defined(DEBUG_LEVEL_HIGH)
#define CAM_DEBUG(fmt, arg...)	\
	do {					\
		printk(KERN_DEBUG "[%s:%d] " fmt,	\
			__func__, __LINE__, ##arg);	\
	}						\
	while (0)

#define cam_info(fmt, arg...)			\
	do {					\
		printk(KERN_INFO "[%s:%d] " fmt,	\
			__func__, __LINE__, ##arg);	\
	}						\
	while (0)
#elif defined(DEBUG_LEVEL_MID)
#define CAM_DEBUG(fmt, arg...)
#define cam_info(fmt, arg...)			\
	do {					\
		printk(KERN_INFO "[%s:%d] " fmt,	\
			__func__, __LINE__, ##arg);	\
	}						\
	while (0)
#else
#define CAM_DEBUG(fmt, arg...)
#define cam_info(fmt, arg...)
#endif

#undef DEBUG_CAM_I2C
#define DEBUG_CAM_I2C

#if defined(DEBUG_CAM_I2C)
#define cam_i2c_dbg(fmt, arg...)		\
	do {					\
		printk(KERN_DEBUG "[%s:%d] " fmt,	\
			__func__, __LINE__, ##arg);	\
	}						\
	while (0)
#else
#define cam_i2c_dbg(fmt, arg...)
#endif


#define cam_err(fmt, arg...)	\
	do {					\
		printk(KERN_ERR "[%s:%d] " fmt,		\
			__func__, __LINE__, ##arg);	\
	}						\
	while (0)

/* level at or below which we need to enable flash when in auto mode */
#define LOW_LIGHT_LEVEL		0x20

#define LOWLIGHT_DEFAULT	0x002B	/*for tuning*/
#define LOWLIGHT_SCENE_NIGHT	0x003D	/*for night mode*/
#define LOWLIGHT_ISO50		0xB52A	/*for tuning*/
#define LOWLIGHT_ISO100		0x9DBA	/*for tuning*/
#define LOWLIGHT_ISO200		0x864A	/*for tuning*/
#define LOWLIGHT_ISO400		0x738A	/*for tuning*/

#define LOWLIGHT_EV_P4		0x003B
#define LOWLIGHT_EV_P3		0x0037
#define LOWLIGHT_EV_P2		0x0033
#define LOWLIGHT_EV_P1		0x002F
#define LOWLIGHT_EV_M1		0x0026
#define LOWLIGHT_EV_M2		0x0021
#define LOWLIGHT_EV_M3		0x001C
#define LOWLIGHT_EV_M4		0x0014

/* for flash */
#define ERRSCL_AUTO			0x01CA	/*for tuning*/
#define USER_AESCL_AUTO		0x01CE	/*for tuning*/
#define ERRSCL_NOW			0x01CC	/*for tuning*/
#define USER_AESCL_NOW		0x01D0	/*for tuning*/

#define CAP_GAINOFFSET		0x0186	/*for tuning*/

#define AE_OFSETVAL			3350	/*for tuning max 5.1times*/
#define AE_MAXDIFF			5000	/*for tuning max =< 5000*/

#define ISX012_DELAY_RETRIES 100/*for modesel_fix, awbsts, half_move_sts*/

#define FLASH_PULSE_CNT 2

#define MOVIEMODE_FLASH	17
#define FLASHMODE_FLASH	18

#define ERROR 1

#define IN_AUTO_MODE 1
#define IN_MACRO_MODE 2

#define ISX012_BURST_DATA_LENGTH 2700

#define WAIT_1MS 1000
#define WAIT_10MS 10000
#define I2C_RETRY_CNT 5

#define ISX012_WRITE_LIST(A)	\
	isx012_i2c_write_list(A, (sizeof(A) / sizeof(A[0])), #A)

struct isx012_userset {
	unsigned int	focus_mode;
	unsigned int	focus_status;
	unsigned int	continuous_af;

	unsigned int	metering;
	unsigned int	exposure;
	unsigned int	wb;
	unsigned int	iso;
	int				contrast;
	int				saturation;
	int				sharpness;
	int				brightness;
	int				ev;
	int				scene;
	unsigned int	zoom;
	unsigned int	effect;		/* Color FX (AKA Color tone) */
	unsigned int	scenemode;
	unsigned int	detectmode;
	unsigned int	antishake;
	unsigned int	fps;
	unsigned int	flash_mode;
	unsigned int	flash_state;
	unsigned int	stabilize;	/* IS */
	unsigned int	strobe;
	unsigned int	jpeg_quality;
	/*unsigned int preview_size;*/
	unsigned int	preview_size_idx;
	unsigned int	capture_size;
	unsigned int	thumbnail_size;
};


/*extern struct isx012_reg isx012_regs;*/
struct reg_struct_init {
    /* PLL setting */
	uint8_t pre_pll_clk_div;	/* 0x0305 */
	uint8_t plstatim;			/* 0x302b */
	uint8_t reg_3024;			/* 0x3024 */
	uint8_t image_orientation;	/* 0x0101 */
	uint8_t vndmy_ablmgshlmt;	/* 0x300a */
	uint8_t y_opbaddr_start_di; /* 0x3014 */
	uint8_t reg_0x3015;			/* 0x3015 */
	uint8_t reg_0x301c;			/* 0x301c */
	uint8_t reg_0x302c;			/* 0x302c */
	uint8_t reg_0x3031;			/* 0x3031 */
	uint8_t reg_0x3041;			/* 0x3041 */
	uint8_t reg_0x3051;			/* 0x3051 */
	uint8_t reg_0x3053;			/* 0x3053 */
	uint8_t reg_0x3057;			/* 0x3057 */
	uint8_t reg_0x305c;			/* 0x305c */
	uint8_t reg_0x305d;			/* 0x305d */
	uint8_t reg_0x3060;			/* 0x3060 */
	uint8_t reg_0x3065;			/* 0x3065 */
	uint8_t reg_0x30aa;			/* 0x30aa */
	uint8_t reg_0x30ab;
	uint8_t reg_0x30b0;
	uint8_t reg_0x30b2;
	uint8_t reg_0x30d3;
	uint8_t reg_0x3106;
	uint8_t reg_0x310c;
	uint8_t reg_0x3304;
	uint8_t reg_0x3305;
	uint8_t reg_0x3306;
	uint8_t reg_0x3307;
	uint8_t reg_0x3308;
	uint8_t reg_0x3309;
	uint8_t reg_0x330a;
	uint8_t reg_0x330b;
	uint8_t reg_0x330c;
	uint8_t reg_0x330d;
	uint8_t reg_0x330f;
	uint8_t reg_0x3381;
};

struct reg_struct {
	uint8_t pll_multiplier;			/* 0x0307 */
	uint8_t frame_length_lines_hi;	/* 0x0340 */
	uint8_t frame_length_lines_lo;	/* 0x0341 */
	uint8_t y_addr_start;			/* 0x0347 */
	uint8_t y_add_end;				/* 0x034b */
	uint8_t x_output_size_msb;		/* 0x034c */
	uint8_t x_output_size_lsb;		/* 0x034d */
	uint8_t y_output_size_msb;		/* 0x034e */
	uint8_t y_output_size_lsb;		/* 0x034f */
	uint8_t x_even_inc;				/* 0x0381 */
	uint8_t x_odd_inc;				/* 0x0383 */
	uint8_t y_even_inc;				/* 0x0385 */
	uint8_t y_odd_inc;				/* 0x0387 */
	uint8_t hmodeadd;				/* 0x3001 */
	uint8_t vmodeadd;				/* 0x3016 */
	uint8_t vapplinepos_start;		/* 0x3069 */
	uint8_t vapplinepos_end;		/* 0x306b */
	uint8_t shutter;				/* 0x3086 */
	uint8_t haddave;				/* 0x30e8 */
	uint8_t lanesel;				/* 0x3301 */
};

struct isx012_work {
	struct work_struct work;
};

struct isx012_exif_data {
	unsigned short iso;
	unsigned short shutterspeed;
};

struct isx012_ctrl {
	const struct msm_camera_sensor_info *sensordata;
	struct isx012_userset settings;
	struct msm_camera_i2c_client *sensor_i2c_client;
	struct msm_sensor_ctrl_t *s_ctrl;
	struct v4l2_subdev *sensor_dev;
	struct v4l2_subdev sensor_v4l2_subdev;
	struct v4l2_subdev_info *sensor_v4l2_subdev_info;
	uint8_t sensor_v4l2_subdev_info_size;
	struct v4l2_subdev_ops *sensor_v4l2_subdev_ops;

	int op_mode;
	int dtp_mode;
	int cam_mode;
	int vtcall_mode;
	int started;
	int flash_mode;
	int lowLight;
	int dtpTest;
	int af_mode;
	int af_status;
	unsigned int lux;
	int awb_mode;
	int samsungapp;
};

struct isx012_format {
	enum v4l2_mbus_pixelcode code;
	enum v4l2_colorspace colorspace;
	u16 fmt;
	u16 order;
};

struct isx012_i2c_reg_conf {
	unsigned short waddr;
	unsigned short wdata;
};

static struct v4l2_subdev_info isx012_subdev_info[] = {
	{
	 .code = V4L2_MBUS_FMT_YUYV8_2X8,
	 .colorspace = V4L2_COLORSPACE_JPEG,
	 .fmt = 1,
	 .order = 0,
	 },
	/* more can be supported, to be added later */
};

/* preview size idx*/
enum isx012_preview_size_t {
	PREVIEW_SIZE_VGA = 0,	/* 640x480*/
	PREVIEW_SIZE_D1,		/* 720x480*/
	PREVIEW_SIZE_WVGA,	/* 800x480*/
	PREVIEW_SIZE_XGA,		/* 1024x768*/
	PREVIEW_SIZE_HD,		/* 1280x720*/
	PREVIEW_SIZE_FHD,		/* 1920x1080*/
	PREVIEW_SIZE_MMS,		/* 528x432 */
	PREVIEW_SIZE_QCIF = 8	/* 176x144 */
};

/* DTP */
enum isx012_dtp_t {
	DTP_OFF = 0,
	DTP_ON,
	DTP_OFF_ACK,
	DTP_ON_ACK,
};

enum isx012_camera_mode_t {
	PREVIEW_MODE = 0,
	MOVIE_MODE
};

enum isx02_AF_mode_t {
	SHUTTER_AF_MODE = 0,
	TOUCH_AF_MODE
};

enum isx012_flash_mode_t {
	FLASH_OFF = 0,
	CAPTURE_FLASH,
	MOVIE_FLASH,
};

enum isx012_test_mode_t {
	TEST_OFF,
	TEST_1,
	TEST_2,
	TEST_3
};

enum isx012_resolution_t {
	QTR_SIZE,
	FULL_SIZE,
	INVALID_SIZE
};

enum isx012_setting {
	RES_PREVIEW,
	RES_CAPTURE
};

#define FRAMESIZE_RATIO(w, h)	((w) * 10 / (h))
#define DEFAULT_WINDOW_WIDTH		80
#define DEFAULT_WINDOW_HEIGHT		80
#define AF_PRECISION	100

enum frame_ratio {
	FRMRATIO_QCIF   = 12,   /* 11 : 9 */
	FRMRATIO_VGA    = 13,   /* 4 : 3 */
	FRMRATIO_D1     = 15,   /* 3 : 2 */
	FRMRATIO_WVGA   = 16,   /* 5 : 3 */
	FRMRATIO_HD     = 17,   /* 16 : 9 */
};
struct isx012_rect {
	s32 x;
	s32 y;
	u32 width;
	u32 height;
};

enum isx012_reg_update {
	/* Sensor egisters that need to be updated during initialization */
	REG_INIT,
	/* Sensor egisters that needs periodic I2C writes */
	UPDATE_PERIODIC,
	/* All the sensor Registers will be updated */
	UPDATE_ALL,
	/* Not valid update */
	UPDATE_INVALID
};
/*
struct isx012_reg {
	const struct reg_struct_init  *reg_pat_init;
	const struct reg_struct  *reg_pat;
};
*/

#ifdef CONFIG_LOAD_FILE

#include <linux/vmalloc.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/uaccess.h>

#define TABLE_MAX_NUM 500

int gtable_buf[TABLE_MAX_NUM];
int gAE_OFSETVAL = AE_OFSETVAL, gAE_MAXDIFF = AE_MAXDIFF;

static char *isx012_regs_table;
static int isx012_regs_table_size;
static int isx012_write_regs_from_sd(char *name);

#define ISX012_BURST_WRITE_LIST(A)	\
	isx012_i2c_write_list(A, (sizeof(A) / sizeof(A[0])), #A)
#else
#define ISX012_BURST_WRITE_LIST(A)	\
	isx012_i2c_burst_write_list(A, (sizeof(A) / sizeof(A[0])), #A)
#endif

extern u8 torchonoff;

static bool g_bCameraRunning;
static unsigned int config_csi2;
int initFlag;
unsigned char isx012_buf[ISX012_BURST_DATA_LENGTH];

uint16_t g_ae_auto = 0, g_ae_now = 0;
int16_t g_ersc_auto = 0, g_ersc_now = 0;

static struct isx012_ctrl *isx012_ctrl;
static struct i2c_client *isx012_client;
static struct isx012_exif_data *isx012_exif;
static struct msm_sensor_ctrl_t isx012_s_ctrl;
static struct device isx012_dev;

static void isx012_set_metering(int mode);
static int32_t isx012_sensor_setting(int update_type, int rt);
static DECLARE_WAIT_QUEUE_HEAD(isx012_wait_queue);
static void isx012_set_flash(int mode);
static int isx012_sensor_config(struct msm_sensor_ctrl_t *s_ctrl,
	void __user *argp);
/*static int  isx012_sensor_get_csi_params(struct msm_sensor_ctrl_t *s_ctrl,
	void __user *argp);*/

DEFINE_MUTEX(isx012_mut);

#endif /* __ISX012_H__ */
