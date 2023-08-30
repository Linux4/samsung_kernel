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

#ifndef FIMC_IS_DEVICE_4E5_H
#define FIMC_IS_DEVICE_4E5_H

/* #define CONFIG_LOAD_FILE */

#if defined(CONFIG_LOAD_FILE)
#include <linux/vmalloc.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/uaccess.h>

#define LOAD_FILE_PATH "/data/media/0/fimc-is-module-4ec-soc-reg.h"
#endif

/* Since anti-banding 60Hz auto setting cause shutter & gain problem
* such as ISO gain and brightness on recording video under low-light condition,
* 4EC sensor have to use 50Hz auto (default) setting only even if CSC setting is 60Hz.
* This feature was decided by IQ and Reliability part on 2015.9.16.
*/
#define USE_ANTIBANDING_50HZ_ONLY

#define SENSOR_S5K4EC_INSTANCE	0
#define SENSOR_S5K4EC_NAME	SENSOR_NAME_S5K4EC
#define SENSOR_S5K4EC_DRIVING

#define TAG_NAME	"[4EC] "
#define cam_err(fmt, ...)	\
	printk(KERN_ERR TAG_NAME fmt, ##__VA_ARGS__)
#define cam_warn(fmt, ...)	\
	printk(KERN_WARNING TAG_NAME fmt, ##__VA_ARGS__)
#define cam_info(fmt, ...)	\
	printk(KERN_INFO TAG_NAME fmt, ##__VA_ARGS__)
#if defined(CONFIG_CAM_DEBUG)
#define cam_dbg(fmt, ...)	\
	printk(KERN_DEBUG TAG_NAME fmt, ##__VA_ARGS__)
#else
#define cam_dbg(fmt, ...)
#endif

enum {
	AUTO_FOCUS_FAILED,
	AUTO_FOCUS_DONE,
	AUTO_FOCUS_CANCELLED,
};

enum af_operation_status {
	AF_NONE = 0,
	AF_START,
	AF_CANCEL,
};

enum capture_flash_status {
	FLASH_STATUS_OFF = 0,
	FLASH_STATUS_PRE_ON,
	FLASH_STATUS_MAIN_ON,
};

enum s5k4ecgx_runmode {
	S5K4ECGX_RUNMODE_NOTREADY,
	S5K4ECGX_RUNMODE_IDLE,
	S5K4ECGX_RUNMODE_RUNNING,
	S5K4ECGX_RUNMODE_CAPTURE,
};

/* result values returned to HAL */
enum af_result_status {
	AF_RESULT_NONE = 0x00,
	AF_RESULT_FAILED = 0x01,
	AF_RESULT_SUCCESS = 0x02,
	AF_RESULT_CANCELLED = 0x04,
	AF_RESULT_DOING = 0x08
};

struct sensor4ec_exif {
	u32 exp_time_num;
	u32 exp_time_den;
	u16 iso;
	u16 flash;
};

/* EXIF - flash filed */
#define EXIF_FLASH_OFF		(0x00)
#define EXIF_FLASH_FIRED		(0x01)
#define EXIF_FLASH_MODE_FIRING		(0x01 << 3)
#define EXIF_FLASH_MODE_SUPPRESSION	(0x02 << 3)
#define EXIF_FLASH_MODE_AUTO		(0x03 << 3)

/* Sensor AF first,second window size.
 * we use constant values instead of reading sensor register */
#define FIRST_WINSIZE_X			512
#define FIRST_WINSIZE_Y			568
#define SCND_WINSIZE_X			116	/* 230 -> 116 */
#define SCND_WINSIZE_Y			306

struct s5k4ecgx_rect {
	s32 x;
	s32 y;
	u32 width;
	u32 height;
};

struct s5k4ecgx_focus {
	u32 pos_x;
	u32 pos_y;
	u32 touch:1;
	u32 reset_done:1;
	u32 window_verified:1;	/* touch window verified */
};

struct s5k4ecgx_framesize {
	u32 index;
	u32 width;
	u32 height;
};

struct fimc_is_module_4ec {
	struct v4l2_subdev	*subdev;
	struct fimc_is_image	image;
	struct mutex		ctrl_lock;
	struct mutex		af_lock;
	struct mutex		i2c_lock;
	struct completion		af_complete;

	u16			sensor_version;
	u32			setfile_index;
	u16			vis_duration;
	u16			frame_length_line;
	u32			line_length_pck;
	u32			system_clock;

	u32			mode;
	u32			contrast;
	u32			effect;
	u32			ev;
	u32			flash_mode;
	u32			focus_mode;
	u32			iso;
	u32			metering;
	u32			saturation;
	u32			scene_mode;
	u32			sharpness;
	u32			white_balance;
	u32			anti_banding;
	u32			fps;
	bool			ae_lock;
	bool			awb_lock;
	bool			user_ae_lock;
	bool			user_awb_lock;
	u32			zoom_ratio;
	bool			sensor_af_in_low_light_mode;
	bool			flash_fire;
	u32			sensor_mode;
	u32			light_level;

	enum s5k4ecgx_runmode	runmode;
	enum af_operation_status	af_status;
	enum capture_flash_status flash_status;
	u32 af_result;
	struct work_struct set_focus_mode_work;
	struct work_struct af_work;
	struct work_struct af_stop_work;
	struct sensor4ec_exif	exif;
	struct s5k4ecgx_focus focus;
	struct s5k4ecgx_framesize preview;
};

#ifndef USE_ANTIBANDING_50HZ_ONLY
static int sensor_4ec_s_anti_banding(struct v4l2_subdev *subdev, int anti_banding);
#endif
static int sensor_4ec_pre_flash_start(struct v4l2_subdev *subdev);
static int sensor_4ec_pre_flash_stop(struct v4l2_subdev *subdev);
static int sensor_4ec_main_flash_start(struct v4l2_subdev *subdev);
static int sensor_4ec_main_flash_stop(struct v4l2_subdev *subdev);
static int sensor_4ec_auto_focus_proc(struct v4l2_subdev *subdev);
static int sensor_4ec_get_exif(struct v4l2_subdev *subdev);
static int sensor_4ec_probe(struct i2c_client *client,
	const struct i2c_device_id *id);

#endif

