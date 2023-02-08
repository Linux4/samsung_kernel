/* drivers/media/video/s5k4ecgx.h
 *
 * Driver for s5k4ecgx (3MP Camera) from SEC(LSI), firmware EVT1.1
 *
 * Copyright (C) 2010, SAMSUNG ELECTRONICS
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef __S5K4ECGX_H__
#define __S5K4ECGX_H__
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <media/v4l2-device.h>
#include <media/v4l2-subdev.h>
#include <linux/workqueue.h>
#include <linux/vmalloc.h>
#ifdef CONFIG_VIDEO_SAMSUNG_EXT_CAMERA
#include <linux/videodev2_exynos_camera_ext.h>
#else
#include <linux/videodev2_exynos_camera.h>
#endif
#include <media/s5k4ecgx_platform.h>
#include <linux/regulator/consumer.h>
#include <plat/gpio-cfg.h>
#include <linux/gpio.h>
#include <linux/clk.h>
#include <linux/spinlock.h>
#include <linux/kthread.h>

#ifndef false
#define false 0
#endif
#ifndef true
#define true 1
#endif

#define S5K4ECGX_DRIVER_NAME	S5K4ECGX_DEVICE_NAME
static const char driver_name[] = S5K4ECGX_DRIVER_NAME;

/************************************
 * FEATURE DEFINITIONS
 ************************************/
#define CONFIG_SUPPORT_AF			true
#define CONFIG_SUPPORT_TOUCH_AF			true
#define CONFIG_SUPPORT_FLASH			true
#define CONFIG_SUPPORT_CAMSYSFS			true
#define CONFIG_SETFILE_POWERLOGICS		true
#define CONFIG_POWER_STANDBY_SUPPORT		true
#define CONFIG_ENABLE_PREVIEW_UPDATE		true
#define CONFIG_CAM_EARYLY_PROBE

/* for tuning */
#define CONFIG_LOAD_FILE		false

/** Debuging Feature **/
#define CONFIG_CAM_DEBUG		true
#define CONFIG_CAM_TRACE		false /* Enable me with CONFIG_CAM_DEBUG */
#define CONFIG_CAM_AF_DEBUG		false /* Enable me with CONFIG_CAM_DEBUG */
#define DEBUG_WRITE_REGS		true

#ifdef CONFIG_VIDEO_FAST_MODECHANGE
#define CONFIG_DEBUG_STREAMOFF		false
#endif

#define CONFIG_DEBUG_FLASH_SET_INTERVAL

#if defined CONFIG_VIDEO_FAST_MODECHANGE \
    && defined CONFIG_VIDEO_FAST_MODECHANGE_V2
#undef CONFIG_VIDEO_FAST_MODECHANGE
#endif
/***********************************/

#define I2C_RETRY_COUNT		5

#ifdef CONFIG_VIDEO_S5K4ECGX_DEBUG
enum {
	S5K4ECGX_DEBUG_I2C		= 1U << 0,
	S5K4ECGX_DEBUG_I2C_BURSTS	= 1U << 1,
};
static uint32_t s5k4ecgx_debug_mask = S5K4ECGX_DEBUG_I2C_BURSTS;
module_param_named(debug_mask, s5k4ecgx_debug_mask, uint, S_IWUSR | S_IRUGO);

#define s5k4ecgx_debug(mask, x...) \
	do { \
		if (s5k4ecgx_debug_mask & mask) \
			pr_info(x);	\
	} while (0)
#else
#define s5k4ecgx_debug(mask, x...)
#endif

#define cam_err(fmt, ...)	\
	printk(KERN_ERR "[""%s""] " fmt, driver_name, ##__VA_ARGS__)
#define cam_warn(fmt, ...)	\
	printk(KERN_WARNING "[""%s""] " fmt, driver_name, ##__VA_ARGS__)
#define cam_info(fmt, ...)	\
	printk(KERN_INFO "[""%s""] " fmt, driver_name, ##__VA_ARGS__)

#if CONFIG_CAM_DEBUG
#define cam_dbg(fmt, ...)	\
	printk(KERN_DEBUG "[""%s""] " fmt, driver_name, ##__VA_ARGS__)

#else
#define cam_dbg(fmt, ...)	\
	do { \
		if (dbg_level & CAMDBG_LEVEL_DEBUG) \
			printk(KERN_DEBUG "[""%s""] " fmt, driver_name, ##__VA_ARGS__); \
	} while (0)
#endif

#if CONFIG_CAM_DEBUG && CONFIG_CAM_TRACE
#define cam_trace(fmt, ...)	cam_dbg("%s: " fmt, __func__, ##__VA_ARGS__);
#else
#define cam_trace(fmt, ...)	\
	do { \
		if (dbg_level & CAMDBG_LEVEL_TRACE) \
			printk(KERN_DEBUG "[""%s""] " fmt, \
				driver_name, ##__VA_ARGS__); \
	} while (0)
#endif

#if CONFIG_CAM_DEBUG && CONFIG_CAM_AF_DEBUG
#define af_dbg(fmt, ...)	cam_dbg(fmt, ##__VA_ARGS__);
#else
#define af_dbg(fmt, ...)
#endif

#if CONFIG_CAM_DEBUG
#define cam_bug_on(arg...)	\
	do { cam_err(arg); BUG(); } while (0)
#define cam_warn_on(arg...)	\
	do { cam_err(arg); WARN_ON(true); } while (0)
#else
#define cam_bug_on(arg...)	cam_err(arg)
#define cam_warn_on(arg...)	cam_err(arg)
#endif

#define CHECK_ERR_COND(condition, ret)	\
		do { if (unlikely(condition)) return ret; } while (0)
#define CHECK_ERR_COND_MSG(condition, ret, fmt, ...) \
		if (unlikely(condition)) { \
			cam_err("%s: error, " fmt, __func__, ##__VA_ARGS__); \
			return ret; \
		}

#define CHECK_ERR_GOTO_COND(condition, label)	\
		do { if (unlikely(condition)) goto label; } while (0)

#define CHECK_ERR_GOTO_COND_MSG(condition, label, fmt, ...) \
		if (unlikely(condition)) { \
			cam_err("%s: error, " fmt, __func__, ##__VA_ARGS__); \
			goto label; \
		}

#define CHECK_ERR(x)	CHECK_ERR_COND(((x) < 0), (x))
#define CHECK_ERR_MSG(x, fmt, ...) \
		CHECK_ERR_COND_MSG(((x) < 0), (x), fmt, ##__VA_ARGS__)

#define CHECK_ERR_GOTO(x, label)	CHECK_ERR_GOTO_COND(((x) < 0), label)
#define CHECK_ERR_GOTO_MSG(x, label, fmt, ...) \
		CHECK_ERR_GOTO_COND_MSG(((x) < 0), label, fmt, ##__VA_ARGS__)

#if CONFIG_LOAD_FILE
#define S5K4ECGX_BURST_WRITE_REGS(sd, A) \
	({ \
		int ret; \
		cam_info("BURST_WRITE_REGS: reg_name=%s from setfile\n", #A); \
		ret = s5k4ecgx_write_regs_from_sd(sd, #A); \
		ret; \
	})
#else
#define S5K4ECGX_BURST_WRITE_REGS(sd, A) \
	s5k4ecgx_burst_write_regs(sd, A, (sizeof(A) / sizeof(A[0])), #A)
#endif

enum s5k4ecgx_i2c_flag {
	S5K_I2C_PAGE = 0x01,
};

#ifndef CONFIG_MACH_WICHITA
/* DSLIM.0515 Fix me later */
enum s5k4ecgx_hw_power {
	S5K4ECGX_HW_POWER_NOT_READY,
	S5K4ECGX_HW_POWER_READY,
	S5K4ECGX_HW_POWER_OFF,
	S5K4ECGX_HW_POWER_ON,
};
#endif

/* result values returned to HAL */
enum af_result_status {
	AF_RESULT_NONE = 0x00,
	AF_RESULT_FAILED = 0x01,
	AF_RESULT_SUCCESS = 0x02,
	AF_RESULT_CANCELLED = 0x04,
	AF_RESULT_DOING = 0x08
};

enum af_operation_status {
	AF_NONE = 0,
	AF_START,
	AF_CANCEL,
};

enum preflash_status {
	PREFLASH_NONE = 0,
	PREFLASH_OFF,
	PREFLASH_ON,
};

enum s5k4ecgx_oprmode {
	S5K4ECGX_OPRMODE_VIDEO = 0,
	S5K4ECGX_OPRMODE_IMAGE = 1,
	S5K4ECGX_OPRMODE_MAX,
};

enum stream_cmd {
	STREAM_STOP,
	STREAM_START,
};

enum wide_req_cmd {
	WIDE_REQ_NONE,
	WIDE_REQ_CHANGE,
	WIDE_REQ_RESTORE,
};

enum s5k4ecgx_preview_frame_size {
	PREVIEW_SZ_QCIF = 0,	/* 176x144 */
	PREVIEW_SZ_QVGA,	/* 320x240 */
	PREVIEW_SZ_CIF,		/* 352x288 */
	PREVIEW_SZ_528x432,	/* 528x432 */
	PREVIEW_SZ_VGA,		/* 640x480 */
	PREVIEW_SZ_D1,		/* 720x480 */
	PREVIEW_SZ_SVGA,	/* 800x600 */
	PREVIEW_SZ_1024x576,	/* 1024x576, 16:9 */
	PREVIEW_SZ_1024x616,	/* 1024x616, ? */
	PREVIEW_SZ_XGA,		/* 1024x768*/
	PREVIEW_SZ_PVGA,	/* 1280x720*/
	PREVIEW_SZ_SXGA,	/* 1280x1024 */
	PREVIEW_SZ_5M,		/* 2560x1920 */
	PREVIEW_SZ_MAX,
};

/* Capture Size List: Capture size is defined as below.
 *
 *	CAPTURE_SZ_VGA:		640x480
 *	CAPTURE_SZ_WVGA:		800x480
 *	CAPTURE_SZ_SVGA:		800x600
 *	CAPTURE_SZ_WSVGA:		1024x600
 *	CAPTURE_SZ_1MP:		1280x960
 *	CAPTURE_SZ_W1MP:		1600x960
 *	CAPTURE_SZ_2MP:		UXGA - 1600x1200
 *	CAPTURE_SZ_W2MP:		35mm Academy Offset Standard 1.66
 *					2048x1232, 2.4MP
 *	CAPTURE_SZ_3MP:		QXGA  - 2048x1536
 *	CAPTURE_SZ_W4MP:		WQXGA - 2560x1536
 *	CAPTURE_SZ_5MP:		2560x1920
 */

enum s5k4ecgx_capture_frame_size {
	CAPTURE_SZ_VGA = 0,	/* 640x480 */
	CAPTURE_SZ_W2MP,	/* 2048x1152, 2.4MP */
	CAPTURE_SZ_3MP,		/* QXGA  - 2048x1536 */
	CAPTRUE_SZ_W3MP,	/* 2560x1440 */
	CAPTURE_SZ_W4MP,	/* W4MP  - 2560x1536 */
	CAPTURE_SZ_5MP,		/* 2560x1920 */
	CAPTURE_SZ_MAX,
};

#ifdef CONFIG_MACH_P2
#define PREVIEW_WIDE_SIZE	PREVIEW_SZ_1024x552
#else
#define PREVIEW_WIDE_SIZE	PREVIEW_SZ_1024x576
#endif
#define CAPTURE_WIDE_SIZE	CAPTURE_SZ_W2MP

enum s5k4ecgx_fps_index {
	I_FPS_0,
	I_FPS_7,
	I_FPS_10,
	I_FPS_12,
	I_FPS_15,
	I_FPS_25,
	I_FPS_30,
	I_FPS_MAX,
};

enum ae_awb_lock {
	AEAWB_UNLOCK = 0,
	AEAWB_LOCK,
	AEAWB_LOCK_MAX,
};

struct s5k4ecgx_control {
	u32 id;
	s32 value;
	s32 default_value;
};

#define S5K4ECGX_INIT_CONTROL(ctrl_id, default_val) \
	{					\
		.id = ctrl_id,			\
		.value = default_val,		\
		.default_value = default_val,	\
	}

struct s5k4ecgx_framesize {
	s32 index;
	u32 width;
	u32 height;
};

/* DSLIM.0515 Fix me later */
struct s5k4ecgx_resolution {
	u8			value;
	enum s5k4ecgx_oprmode	type;
	u16			width;
	u16			height;
};

enum {
    FRMRATIO_QCIF   = 12,   /* 11 : 9 */
    FRMRATIO_VGA    = 13,   /* 4 : 3 */
    FRMRATIO_D1     = 15,   /* 3 : 2 */
    FRMRATIO_WVGA   = 16,   /* 5 : 3 */
    FRMRATIO_HD     = 17,   /* 16 : 9 */
    FRMRATIO_SQUARE     = 10,   /* 1 : 1 */
};

#define FRM_RATIO(w, h)	((w) * 10 / (h))

#define FRAMESIZE_RATIO(framesize) \
	FRM_RATIO((framesize)->width, (framesize)->height)

struct s5k4ecgx_time_interval {
	struct timeval start;
	struct timeval end;
};

#define GET_ELAPSED_TIME(before, cur) \
		(((cur).tv_sec - (before).tv_sec) * USEC_PER_SEC \
		+ ((cur).tv_usec - (before).tv_usec))

struct s5k4ecgx_fps {
	u32 index;
	u32 fps;
};

struct s5k4ecgx_version {
	u32 major;
	u32 minor;
};

struct s5k4ecgx_date_info {
	u32 year;
	u32 month;
	u32 date;
};

enum runmode {
	RUNMODE_NOTREADY,
	RUNMODE_INIT,
	/*RUNMODE_IDLE,*/
	RUNMODE_RUNNING, /* previewing */
	RUNMODE_RUNNING_STOP,
	RUNMODE_CAPTURING,
	RUNMODE_CAPTURING_STOP,
	RUNMODE_RECORDING,	/* camcorder mode */
	RUNMODE_RECORDING_STOP,
};

struct s5k4ecgx_firmware {
	u32 addr;
	u32 size;
};

struct s5k4ecgx_jpeg_param {
	u32 enable;
	u32 quality;
	u32 main_size;		/* Main JPEG file size */
	u32 thumb_size;		/* Thumbnail file size */
	u32 main_offset;
	u32 thumb_offset;
	/* u32 postview_offset; */
};

struct s5k4ecgx_position {
	s32 x;
	s32 y;
};

struct s5k4ecgx_rect {
	s32 x;
	s32 y;
	u32 width;
	u32 height;
};

struct gps_info_common {
	u32 direction;
	u32 dgree;
	u32 minute;
	u32 second;
};

struct s5k4ecgx_gps_info {
	u8 gps_buf[8];
	u8 altitude_buf[4];
	s32 gps_timeStamp;
};

struct s5k4ecgx_mode {
	enum v4l2_sensor_mode sensor;
	enum runmode runmode;

	u32 hd_video:1;
} ;

struct s5k4ecgx_preview {
	const struct s5k4ecgx_framesize *frmsize;
	u32 update_frmsize:1;
	u32 fast_ae:1;
};

struct s5k4ecgx_capture {
	const struct s5k4ecgx_framesize *frmsize;
	u32 pre_req;		/* for fast capture */
	u32 ae_manual_mode:1;
	u32 lowlux:1;		/* low lux without night scene */
	u32 lowlux_night:1;	/* low lux with night scene*/
	u32 ready:1;		/* for fast capture */
};

struct s5k4ecgx_focus {
	enum v4l2_focusmode mode;
	enum af_result_status status;
	struct s5k4ecgx_time_interval win_stable;

	u32 pos_x;
	u32 pos_y;

	u32 support:1;
	u32 start:1;		/* enum v4l2_auto_focus*/
	u32 touch:1;

	/* It means that cancel has been done and then each AF regs-table
	 * has been written. */
	u32 reset_done:1;
	u32 window_verified:1;	/* touch window verified */
};

/* struct for sensor specific data */
struct s5k4ecgx_ae_gain_offset {
	u32	ae_auto;
	u32	ae_now;
	u32	ersc_auto;
	u32	ersc_now;

	u32	ae_ofsetval;
	u32	ae_maxdiff;
};

/* Flash struct */
struct s5k4ecgx_flash {
	struct s5k4ecgx_ae_gain_offset ae_offset;
	enum v4l2_flash_mode mode;
	enum preflash_status preflash;
	u32 awb_delay;
	u32 ae_scl;	/* for back-up */
	u32 on:1;	/* flash on/off */
	u32 ignore_flash:1;
	u32 ae_flash_lock:1;
	u32 support:1;	/* to support flash */
};

/* Exposure struct */
struct s5k4ecgx_exposure {
	s32 val;	/* exposure value */
	u32 ae_lock:1;
};

/* White Balance struct */
struct s5k4ecgx_whitebalance {
	enum v4l2_wb_mode mode; /* wb mode */
	u32 awb_lock:1;
};

struct s5k4ecgx_exif {
	u32 exp_time_den;
	u16 iso;
	u16 flash;

	/*int bv;*/		/* brightness */
	/*int ebv;*/		/* exposure bias */
};

/* EXIF - flash filed */
#define EXIF_FLASH_FIRED		(0x01)
#define EXIF_FLASH_MODE_FIRING		(0x01 << 3)
#define EXIF_FLASH_MODE_SUPPRESSION	(0x02 << 3)
#define EXIF_FLASH_MODE_AUTO		(0x03 << 3)

struct s5k4ecgx_regset {
	u32 size;
	u8 *data;
};

#if CONFIG_LOAD_FILE
#if !(DEBUG_WRITE_REGS)
#undef DEBUG_WRITE_REGS
#define DEBUG_WRITE_REGS	true
#endif

struct regset_table {
	const char	*const name;
};

#define REGSET(x, y)		\
	[(x)] = {			\
		.name		= #y,	\
}

#define REGSET_TABLE(y)	\
	{				\
		.name		= #y,	\
}
#else
struct regset_table {
	const u32	*const reg;
	const u32	array_size;
#if DEBUG_WRITE_REGS
	const char	*const name;
#endif
};

#if DEBUG_WRITE_REGS
#define REGSET(x, y)		\
	[(x)] = {					\
		.reg		= (y),			\
		.array_size	= ARRAY_SIZE((y)),	\
		.name		= #y,			\
}

#define REGSET_TABLE(y)		\
	{					\
		.reg		= (y),			\
		.array_size	= ARRAY_SIZE((y)),	\
		.name		= #y,			\
}
#else
#define REGSET(x, y)		\
	[(x)] = {					\
		.reg		= (y),			\
		.array_size	= ARRAY_SIZE((y)),	\
}

#define REGSET_TABLE(y)		\
	{					\
		.reg		= (y),			\
		.array_size	= ARRAY_SIZE((y)),	\
}
#endif /* DEBUG_WRITE_REGS */
#endif /* CONFIG_LOAD_FILE */

#define EV_MIN_VLAUE		EV_MINUS_4
#define GET_EV_INDEX(EV)	((EV) - (EV_MIN_VLAUE))

struct s5k4ecgx_regs {
	struct regset_table ev[GET_EV_INDEX(EV_MAX /*EV_MAX_V4L2*/)];
	struct regset_table metering[METERING_MAX];
	struct regset_table iso[ISO_MAX];
	struct regset_table effect[IMAGE_EFFECT_MAX];
	struct regset_table white_balance[WHITE_BALANCE_MAX];
	struct regset_table preview_size[PREVIEW_SZ_MAX];
	struct regset_table scene_mode[SCENE_MODE_MAX];
	struct regset_table saturation[SATURATION_MAX];
	struct regset_table contrast[CONTRAST_MAX];
	struct regset_table sharpness[SHARPNESS_MAX];
	struct regset_table fps[I_FPS_MAX];
	struct regset_table flash_start;
	struct regset_table flash_end;
	struct regset_table preflash_start;
	struct regset_table preflash_end;
	struct regset_table fast_ae_on;
	struct regset_table fast_ae_off;
	struct regset_table ae_lock;
	struct regset_table ae_unlock;
	struct regset_table awb_lock;
	struct regset_table awb_unlock;
	struct regset_table restore_cap;
	struct regset_table change_wide_cap;
	struct regset_table lowlight_cap_on;
	struct regset_table lowlight_cap_off;

	/* AF */
	struct regset_table af_macro_mode;
	struct regset_table af_normal_mode;
#if !defined(CONFIG_MACH_P2)
	struct regset_table af_night_normal_mode;
#endif
	struct regset_table af_off;
	struct regset_table hd_af_start;
	struct regset_table hd_first_af_start;
	struct regset_table single_af_start;
	struct regset_table single_af_pre_start;
	struct regset_table single_af_pre_start_macro;

	/* Init */
	struct regset_table init;
#if 1 /* DSLIM fix me later */
	struct regset_table init_reg_1;
	struct regset_table init_reg_2;
	struct regset_table init_reg_3;
	struct regset_table init_reg_4;
#endif

	struct regset_table get_light_level;
	struct regset_table get_esd_status;
	struct regset_table get_iso;
	struct regset_table get_ae_stable;
	struct regset_table get_shutterspeed;

	/* Mode */
	struct regset_table preview_mode;
	struct regset_table preview_hd_mode;
	struct regset_table update_preview;
	struct regset_table return_preview_mode;
	struct regset_table capture_mode[CAPTURE_SZ_MAX];
	struct regset_table camcorder_on[PREVIEW_SZ_MAX];
	struct regset_table camcorder_off;
	struct regset_table stream_stop;

	/* Misc */
	struct regset_table antibanding;
	struct regset_table softlanding;
	struct regset_table softlanding_cap;
};

#ifdef CONFIG_CAM_EARYLY_PROBE
struct s5k4ecgx_core_state {
	struct v4l2_subdev sd;
	struct media_pad pad; /* for media deivce pad */
	struct workqueue_struct *wq;
	char c_name[128];
	u32 data;
};
#endif

struct s5k4ecgx_state {
#ifdef CONFIG_CAM_EARYLY_PROBE
	struct s5k4ecgx_core_state *c_state;
	struct v4l2_subdev *sd;
#else
	struct v4l2_subdev sd;
	struct media_pad pad; /* for media deivce pad */
#endif
	struct s5k4ecgx_platform_data *pdata;

	struct v4l2_pix_format req_fmt;
	struct v4l2_mbus_framefmt ffmt;
	struct s5k4ecgx_preview preview;
	struct s5k4ecgx_capture capture;
	struct s5k4ecgx_focus focus;
	struct s5k4ecgx_flash flash;
	struct s5k4ecgx_exposure exposure;
	struct s5k4ecgx_whitebalance wb;
	struct s5k4ecgx_exif exif;
	struct s5k4ecgx_time_interval stream_time;
	const struct s5k4ecgx_regs *regs;
	struct mutex ctrl_lock;
	struct mutex af_lock;
	struct mutex i2c_lock;
#if defined(CONFIG_VIDEO_FAST_CAPTURE) || defined(CONFIG_VIDEO_FAST_MODECHANGE_V2)
	struct completion streamoff_complete;
#endif
	struct workqueue_struct *wq;
	struct work_struct af_work;
	struct work_struct af_win_work;
#if defined(CONFIG_VIDEO_FAST_MODECHANGE) \
    || defined(CONFIG_VIDEO_FAST_MODECHANGE_V2)
	struct work_struct streamoff_work;
#endif
#ifdef CONFIG_VIDEO_FAST_CAPTURE
	struct work_struct capmode_work;
#endif
#ifdef CONFIG_DEBUG_FLASH_AE
	struct task_struct *print_ae_thread;
#endif

#ifndef CONFIG_MACH_WICHITA
	struct clk *mclk;
#endif
	char s_name[128]; //dslim
	enum runmode runmode;
	enum s5k4ecgx_oprmode oprmode; /* DSLIM.0515 Fix me later */
	enum v4l2_sensor_mode sensor_mode;
	enum v4l2_pix_format_mode format_mode;
	enum v4l2_scene_mode scene_mode;

	/* To switch from nornal ratio to wide ratio.*/
	enum wide_req_cmd wide_cmd;
	enum v4l2_mbus_pixelcode code; /* for media deivce code *//* DSLIM.0515 Fix me later */

	int res_type; /* DSLIM.0515 Fix me later */
	u8 resolution; /* DSLIM.0515 Fix me later */
	s32 vt_mode;
	s32 req_fps;
	s32 fps;
	s32 freq;		/* MCLK in Hz */
	u32 one_frame_delay_ms;
	u32 light_level;	/* light level */
	/* u32 streamoff_delay;*/
	u32 *dbg_level;
#if defined(CONFIG_VIDEO_FAST_MODECHANGE) \
    || defined(CONFIG_VIDEO_FAST_MODECHANGE_V2)
	atomic_t streamoff_check;
#endif
#ifdef CONFIG_VIDEO_FAST_CAPTURE
	atomic_t capmode_check;
#endif

	pid_t af_pid;
#ifndef CONFIG_MACH_WICHITA
	int power_on; /* DSLIM.0515 Fix me later */
#endif
	int iso;

	u32 recording:1;
	u32 hd_videomode:1;
	u32 need_wait_streamoff:1;
	u32 reset_done:1;	/* reset is done */
	u32 initialized:1;
	u32 need_preview_update:1;
};

#ifdef CONFIG_CAM_EARYLY_PROBE
#define TO_C_STATE(p, m)	(container_of(p, struct s5k4ecgx_core_state, m))

#define SD_TO_STATE(p, m)		({ \
		struct s5k4ecgx_core_state *c_state = \
			container_of(p, struct s5k4ecgx_core_state, m); \
		struct s5k4ecgx_state *state = \
			(struct s5k4ecgx_state *)(c_state->data); \
		state; \
	})
#endif
#define TO_STATE(p, m)		(container_of(p, struct s5k4ecgx_state, m))

#define GET_ONE_FRAME_DELAY(fps)	(1000 / fps + 1)
#define IS_FLASH_SUPPORTED()		(CONFIG_SUPPORT_FLASH)
#define IS_AF_SUPPORTED()		(CONFIG_SUPPORT_AF)
#define IS_TOUCH_AF_SUPPORTED()	(CONFIG_SUPPORT_TOUCH_AF && CONFIG_SUPPORT_AF)
#define IS_PREVIEW_UPDATE_SUPPORTED()	(CONFIG_ENABLE_PREVIEW_UPDATE)

extern struct class *camera_class;

static inline struct  s5k4ecgx_state *to_state(struct v4l2_subdev *sd)
{
#ifdef CONFIG_CAM_EARYLY_PROBE
	return SD_TO_STATE(sd, sd);
#else
	return TO_STATE(sd, sd);
#endif
}

#ifdef CONFIG_CAM_EARYLY_PROBE
static inline struct  s5k4ecgx_core_state *to_c_state(struct v4l2_subdev *sd)
{
	return TO_C_STATE(sd, sd);
}
#endif

static inline int check_af_pid(struct v4l2_subdev *sd)
{
	struct s5k4ecgx_state *state = to_state(sd);

	if (state->af_pid && (task_pid_nr(current) == state->af_pid))
		return -EPERM;
	else
		return 0;
}

#ifdef CONFIG_CAM_EARYLY_PROBE
static int s5k4ecgx_late_probe(struct v4l2_subdev *sd);
static int s5k4ecgx_early_remove(struct v4l2_subdev *sd);
#endif
static int s5k4ecgx_init(struct v4l2_subdev *, u32);
static int s5k4ecgx_reset(struct v4l2_subdev *, u32);
/*static int s5k4ecgx_post_init(struct v4l2_subdev *, u32);*/
static int s5k4ecgx_s_ctrl(struct v4l2_subdev *, struct v4l2_control *);
static int s5k4ecgx_restore_ctrl(struct v4l2_subdev *);
static int s5k4ecgx_start_fast_capture(struct v4l2_subdev *sd);
static int s5k4ecgx_get_exif(struct v4l2_subdev *sd);

/*********** Sensor specific ************/
#define S5K4ECGX_DELAY		0xFFFF0000

#define S5K4ECGX_CHIP_ID	0x4EC0
#define S5K4ECGX_CHIP_REV	0x0011 /* S5K4ECGX_VERSION_1_1 */

#define FORMAT_FLAGS_COMPRESSED		0x3

#define POLL_TIME_MS		10
#define CAPTURE_POLL_TIME_MS    1000

#define PREVIEW_FPS_HIGH		30
#define PREVIEW_FPS_NORMAL		15
#define PREVIEW_FPS_LOW			10	/* 10 fps in low lux */
#define PREVIEW_FPS_NIGHT		6	/* 6 fps */

/* maximum time for one frame in norma light */
#define ONE_FRAME_DELAY_MS_NORMAL \
			GET_ONE_FRAME_DELAY(PREVIEW_FPS_NORMAL)
/* maximum time for one frame in low light: minimum 10fps. */
#define ONE_FRAME_DELAY_MS_LOW \
			GET_ONE_FRAME_DELAY(PREVIEW_FPS_LOW)
/* maximum time for one frame in night mode: 6fps */
#define ONE_FRAME_DELAY_MS_NIGHTMODE \
			GET_ONE_FRAME_DELAY(PREVIEW_FPS_NIGHT)

/* level at or below which we need to enable flash when in auto mode */
#define FLASH_LOW_LIGHT_LEVEL		0x32

#define LUX_LEVEL_MAX			0xFFFFFFFF	/* the brightest */

/* low light. 32h -> 51h. if 50h, it's low light. */
#define LUX_LEVEL_LOW			0x51
/*#define LUX_LEVEL_FLASH_ON		0x2B*/

#define LENS_POS_CHECK_COUNT		10	/* 33ms x 10 */
#define FIRST_AF_SEARCH_COUNT		220	/* 33ms x 220 */
#define SECOND_AF_SEARCH_COUNT		220	/* 33ms x 220 */
#define AE_STABLE_SEARCH_COUNT		22	/* 33ms x 22 */
#define STREAMOFF_CHK_COUNT		150

#define AF_SEARCH_DELAY			GET_ONE_FRAME_DELAY(PREVIEW_FPS_HIGH)
#define AE_STABLE_SEARCH_DELAY		AF_SEARCH_DELAY
#define LENS_MOVE_CHECK_DELAY		GET_ONE_FRAME_DELAY(PREVIEW_FPS_HIGH)

/* Sensor AF first,second window size.
 * we use constant values intead of reading sensor register */
#define FIRST_WINSIZE_X			512
#define FIRST_WINSIZE_Y			568
#define SCND_WINSIZE_X			116	/* 230 -> 116 */
#define SCND_WINSIZE_Y			306


/*
 * Register Address Definition
 */
 
 /** HW ID registers **/
#define REG_FW_VER_SENSOR_ID		0x700001A4
#define REG_FW_VER_HW_REV		0x700001A6

/** REG_TC_GP_EnablePreviewChanged **/
#define REG_ENABLE_PREVIEW_CHANGED	0x70000240
#define REG_ENABLE_CAPTURE_CHANGED	0x70000244

/** AF window registers **/
#define REG_AF_FSTWIN_STARTX		0x70000294
#define REG_AF_FSTWIN_STARTY		0x70000296
#define REG_AF_FSTWIN_SIZEX		0x70000298
#define REG_AF_FSTWIN_SIZEY		0x7000029A
#define REG_AF_SCNDWIN_STARTX		0x7000029C
#define REG_AF_SCNDWIN_STARTY		0x7000029E
#define REG_AF_SCNDWIN_SIZEX		0x700002A0
#define REG_AF_SCNDWIN_SIZEY		0x700002A2

#define REG_AF_WINSIZES_UPDATED		0x700002A4

#define REG_MON_AAIO_ME_LEI_EXP		0x70002BC0
#define REG_MON_AAIO_ME_AGAIN		0x70002BC4
#define REG_MON_AAIO_ME_DGAIN		0x70002BC6
#define REG_MON_AE_STABLE		0x70002C74
#define REG_LIGHT_STATUS		0x70002C18

/** AF registers **/
#define REG_MON_AF_1ST_SEARCH		0x70002EEE
#define REG_MON_AF_2ND_SEARCH		0x70002207

/* Mon_afd_usCurrentDriverPosition */
#define REG_MON_AFD_CUR_DRV_POS		0x70003048

/** FLASH IC Register **/
#define FEN_FCUR_REG		1
#define TEN_TCUR_REG		2
#define LB_TH_REG		4

/* The Path of Setfile */
#if CONFIG_LOAD_FILE
struct test {
	u8 data;
	struct test *nextBuf;
};
static struct test *testBuf;
static s32 large_file;

#define TEST_INIT	\
{			\
	.data = 0;	\
	.nextBuf = NULL;	\
}

#define TUNING_FILE_PATH "/data/media/0/s5k4ecgx_regs.h"
#endif /* CONFIG_LOAD_FILE*/

#if defined(CONFIG_MACH_DELOSLTE_KOR_SKT) || defined(CONFIG_MACH_DELOSLTE_KOR_LGT)
#include "s5k4ecgx_regs_deloslte_kor.h"
#elif defined(CONFIG_MACH_WHCHITA)
#include "s5k4ecgx_regs_wichita.h"
#else
#include "s5k4ecgx_regs.h"
#endif

#endif /* __S5K4ECGX_H__ */
