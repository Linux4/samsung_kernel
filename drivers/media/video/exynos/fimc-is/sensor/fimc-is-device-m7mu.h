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

#include <linux/wakelock.h>

#define SENSOR_M7MU_INSTANCE	0
#define SENSOR_M7MU_NAME	SENSOR_NAME_M7MU
#define SENSOR_M7MU_DRIVING

#define CONFIG_CAM_DEBUG

#define cam_warn(fmt, ...)	\
	do { \
		printk(KERN_WARNING "%s:%d: " fmt, __func__, __LINE__, ##__VA_ARGS__); \
	} while (0)

#define cam_err(fmt, ...)	\
	do { \
		printk(KERN_ERR "%s:%d: " fmt, __func__, __LINE__, ##__VA_ARGS__); \
	} while (0)

#define cam_info(fmt, ...)	\
	do { \
		printk(KERN_INFO "%s:%d " fmt, __func__, __LINE__, ##__VA_ARGS__); \
	} while (0)

#ifdef CONFIG_CAM_DEBUG
#define CAM_DEBUG   (1 << 0)
#define CAM_TRACE   (1 << 1)
#define CAM_I2C     (1 << 2)

#define cam_dbg(fmt, ...)	\
	do { \
		if (to_state(sd)->dbg_level & CAM_DEBUG) \
			printk(KERN_DEBUG "%s:%d: " fmt, __func__, __LINE__, ##__VA_ARGS__); \
	} while (0)

#define cam_trace(fmt, ...)	\
	do { \
		if (to_state(sd)->dbg_level & CAM_TRACE) \
			printk(KERN_DEBUG "%s:%d: " fmt, __func__, __LINE__, ##__VA_ARGS__); \
	} while (0)

#define cam_i2c_dbg(fmt, ...)	\
	do { \
		if (to_state(sd)->dbg_level & CAM_I2C) \
			printk(KERN_DEBUG "%s:%d " fmt, __func__, __LINE__, ##__VA_ARGS__); \
	} while (0)
#else
#define cam_dbg(fmt, ...)
#define cam_trace(fmt, ...)
#define cam_i2c_dbg(fmt, ...)
#endif
#define FRM_RATIO(x)    ((x)->width*10/(x)->height)

#if 0
#define M7MU_FW_VER_LEN		22
#define M7MU_FW_VER_FILE_CUR	0x16FF00
#define M7MU_FW_VER_NUM		0x000018
#else
#define M7MU_FW_VER_LEN	20
#define M7MU_FW_VER_TOKEN 11
#define M7MU_SENSOR_TYPE_LEN 25
#define M7MU_SEN_FW_VER_LEN	30
#define M7MU_FW_VER_FILE_CUR	0x1FF080
#define M7MU_FW_VER_NUM		0x1FF080
#endif

#define DEFAULT_SENSOR_WIDTH		1280
#define DEFAULT_SENSOR_HEIGHT		720
#define DEFAULT_SENSOR_CODE		(V4L2_MBUS_FMT_YUYV8_2X8)


enum m7mu_hw_power {
	M7MU_HW_POWER_OFF,
	M7MU_HW_POWER_ON,
};

enum m7mu_runmode {
	M7MU_RUNMODE_NOTREADY,
	M7MU_RUNMODE_IDLE,
	M7MU_RUNMODE_RUNNING,
	M7MU_RUNMODE_CAPTURE,
};

enum m7mu_oprmode {
	M7MU_OPRMODE_VIDEO = 0,
	M7MU_OPRMODE_IMAGE = 1,
	M7MU_OPRMODE_HDR = 2,
	M7MU_OPRMODE_MAX,
};

enum m7mu_prev_frmsize {
	M7MU_PREVIEW_QCIF,
	M7MU_PREVIEW_QCIF2,
	M7MU_PREVIEW_QVGA,
	M7MU_PREVIEW_CIF,
	M7MU_PREVIEW_VGA,
	M7MU_PREVIEW_640_524,
	M7MU_PREVIEW_800_450,
	M7MU_PREVIEW_1024,
	M7MU_PREVIEW_D1_120FPS,
	M7MU_PREVIEW_704_704,
	M7MU_PREVIEW_D1_DZ,
	M7MU_PREVIEW_WVGA,
	M7MU_PREVIEW_704_528_DZ,
	M7MU_PREVIEW_960_720,
	M7MU_PREVIEW_960_540_DZ,
	M7MU_PREVIEW_1056_704,
	M7MU_PREVIEW_720P,
	M7MU_PREVIEW_1080P,
	M7MU_PREVIEW_HDR,
	M7MU_PREVIEW_720P_60FPS,
	M7MU_PREVIEW_720P_120FPS,
	M7MU_PREVIEW_VGA_60FPS,
	M7MU_PREVIEW_1080P_DUAL,
	M7MU_PREVIEW_720P_DUAL,
	M7MU_PREVIEW_VGA_DUAL,
	M7MU_PREVIEW_QVGA_DUAL,
	M7MU_PREVIEW_1440_1080,
	M7MU_PREVIEW_672_448,
	M7MU_PREVIEW_FACTORY_TILT,
};

enum m7mu_cap_frmsize {
	M7MU_CAPTURE_VGA,		/* 640 x 480 */
	M7MU_CAPTURE_HD,		/* 960 x 720 */
	M7MU_CAPTURE_1MP,		/* 1024 x 768 */
	M7MU_CAPTURE_2MPW,		/* 1920 x 1080 */
	M7MU_CAPTURE_3MP,		/* 1984 x 1488 */
	M7MU_CAPTURE_4MP,		/* 2304 x 1728 */
	M7MU_CAPTURE_5MP,		/* 2592 x 1944 */
	M7MU_CAPTURE_6MP,		/* 3328 x 1872 MAGIC SHOT */
	M7MU_CAPTURE_8MP,		/* 3264 x 2448 */
	M7MU_CAPTURE_8_5MP,		/* 2880 x 2880 */
	M7MU_CAPTURE_9MP,		/* 3888 x 2592 */
	M7MU_CAPTURE_9MW,		/* 3968 x 2232 */
	M7MU_CAPTURE_9MPW,		/* 4096 x 2304 */
	M7MU_CAPTURE_10MP,		/* 3648 x 2736 */
	M7MU_CAPTURE_11MP,		/* 3960 x 2640 */
	M7MU_CAPTURE_12MPW,		/* 4608 x 2592 */
	M7MU_CAPTURE_14MP,		/* 4608 x 3072 */
	M7MU_CAPTURE_16MP,		/* 4608 x 3456 */
	M7MU_CAPTURE_RAW,		/* 4088 x 2500 */
	M7MU_CAPTURE_15_1MPW,	/* 5184 x 2916 */
	M7MU_CAPTURE_17_9MP,	/* 5184 x 3456 */
	M7MU_CAPTURE_20MP,		/* 5184 x 3888 */
};

enum m7mu_post_frmsize {
	M7MU_CAPTURE_POSTQVGA,		/* 320 x 240 */
	M7MU_CAPTURE_POSTVGA,		/* 640 x 480 */
	M7MU_CAPTURE_POST_704_528,  /* 704 x 528 */
	M7MU_CAPTURE_POST_704_704,  /* 704 x 704 */
	M7MU_CAPTURE_POSTHD,		/* 960 x 720 */
	M7MU_CAPTURE_POST_768_512,	/* 768 x 512 */
	M7MU_CAPTURE_POSTP,			/* 1056 x 704 */
	M7MU_CAPTURE_POSTWVGA,		/* 800 x 480 */
	M7MU_CAPTURE_POST_960_540,	/* 960 x 540 */
	M7MU_CAPTURE_POSTWHD,		/* 1280 x 720 */
};

enum cam_frmratio {
	CAM_FRMRATIO_QCIF   = 12,   /* 11 : 9 */
	CAM_FRMRATIO_VGA    = 13,   /* 4 : 3 */
	CAM_FRMRATIO_D1     = 15,   /* 3 : 2 */
	CAM_FRMRATIO_WVGA   = 16,   /* 5 : 3 */
	CAM_FRMRATIO_HD     = 17,   /* 16 : 9 */
};

enum m7mu_zoom_action_method {
	M7MU_ZOOM_METHOD_NONE = 0,
	M7MU_ZOOM_METHOD_KEY = 1,
	M7MU_ZOOM_METHOD_RING = 2,
};

struct m7mu_control {
	u32 id;
	s32 value;
	s32 minimum;		/* Note signedness */
	s32 maximum;
	s32 step;
	s32 default_value;
};

struct m7mu_frmsizeenum {
	unsigned int index;
	unsigned int target_width;	/* FIMC output width */
	unsigned int target_height;	/* FIMC output height */
	unsigned int sensor_width;	/* CIS Input width */
	unsigned int sensor_height;	/* CIS Input height */
	u8 reg_val;		/* a value for category parameter */
};

struct m7mu_isp {
	wait_queue_head_t wait;
	wait_queue_head_t boot_wait;
	unsigned int irq;	/* irq issued by ISP */
	unsigned int issued;
	unsigned int boot_irq;
	unsigned int boot_irq_enabled;
	unsigned int boot_issued;
	unsigned int boot_issued_rising;
	unsigned int int_factor;
	unsigned int boot_lens;
	unsigned int bad_fw:1;

	wait_queue_head_t sound_wait;
	unsigned int sound_irq;	/* sound irq issued by ISP */
	unsigned int sound_issued;
};

struct m7mu_jpeg {
	u32 enable;
	int quality;
	unsigned int main_size;	/* Main JPEG file size */
	unsigned int thumb_size;	/* Thumbnail file size */
	unsigned int main_offset;
	unsigned int thumb_offset;
	unsigned int postview_offset;
};

struct m7mu_focus {
	unsigned int start:1;
	unsigned int lock:1;
	unsigned int touch:1;

	unsigned int mode;
#if defined(CONFIG_TARGET_LOCALE_NA)
	unsigned int ui_mode;
	unsigned int mode_select;
#endif
	unsigned int status;

	unsigned int pos_x;
	unsigned int pos_y;
};

/* AE touch information for AF/AE Division Function in Galaxy-Camera */
struct m7mu_ae_touch {
	unsigned int start:1;
	unsigned int lock:1;
	unsigned int touch:1;

	unsigned int mode;
	unsigned int status;

	unsigned int pos_x;
	unsigned int pos_y;

	unsigned int window_size_width;
	unsigned int window_size_height;
};

struct m7mu_factory_punt_data {
	unsigned int min;
	unsigned int max;
	unsigned int num;
};

struct m7mu_factory_zoom_data {
	unsigned int range_min;
	unsigned int range_max;
	unsigned int slope_min;
	unsigned int slope_max;
};

struct m7mu_factory_zoom_slope_data {
	unsigned int min;
	unsigned int max;
};

struct m7mu_exif {
	char unique_id[7];
	u32 exptime;		/* us */
	u16 flash;
	u16 iso;
	int tv;			/* shutter speed */
	int shutter_speed_num;
	int shutter_speed_den;
	int bv;			/* brightness */
	int ebv;		/* exposure bias */
	int av;			/* Aperture */
	int focal_length;
	int focal_35mm_length;
	int fnumber;
	int exptime_num;
	int exptime_den;
};

struct m7mu_version {
	u32 major;
	u32 minor;
};

struct m7mu_state {
	struct m7mu_platform_data *pdata;
	struct device *m7mu_dev;
	//struct v4l2_subdev sd;
	struct v4l2_mbus_framefmt	ffmt[2]; /* for media deivce fmt */
	struct v4l2_pix_format		pix;

	struct wake_lock wake_lock;

	struct m7mu_isp isp;
	struct clk			*mclk;
	struct v4l2_streamparm		strm;
	struct v4l2_streamparm		stored_parm;
	struct m7mu_version		fw;
	struct media_pad	 	pad; /* for media deivce pad */
	struct mutex			ctrl_lock;
	struct completion		af_complete;

	const struct m7mu_frmsizeenum *preview;
	const struct m7mu_frmsizeenum *capture;
	const struct m7mu_frmsizeenum *postview;

	enum v4l2_pix_format_mode format_mode;
	enum v4l2_sensor_mode sensor_mode;
	enum v4l2_cam_flash_mode flash_mode;
	enum v4l2_scene_mode scene_mode;
	enum m7mu_runmode		runmode;
	enum m7mu_oprmode		oprmode;
	int vt_mode;
	int samsung_app;
	int zoom;
	int smart_zoom_mode;
	int 				one_frame_delay_ms;
	int				preview_framesize_index;
	int				capture_framesize_index;

	int m7mu_fw_done;
	int fw_info_done;

	int start_cap_kind;
	unsigned int fps;
	
	enum v4l2_mbus_pixelcode	code; /* for media deivce code */
	int 				res_type;
	u8 				resolution;
	bool 				power_on;

	int factory_down_check;
	int factory_result_check;
	int factory_end_check;
	int factory_category;
	int factory_byte;
	int factory_value;
	int factory_value_size;
	int factory_end_interrupt;
	int factory_gyro_value;

	bool IsStartObjectTraking;

	struct m7mu_focus focus;
	struct m7mu_ae_touch ae_touch;
	struct m7mu_factory_punt_data f_punt_data;
	struct m7mu_factory_zoom_data f_zoom_data;

	unsigned int factory_log_addr;
	u16 factory_log_size;
	int factory_test_num;
	int factory_log_write;
	int factory_no_lens_off;

	struct m7mu_jpeg jpeg;
	struct m7mu_exif exif;

	int isp_fw_ver;
	u8 sensor_ver[M7MU_FW_VER_TOKEN + 1];
	u8 phone_ver[M7MU_FW_VER_TOKEN + 1];
	u8 sensor_type[M7MU_SENSOR_TYPE_LEN + 1];

	int fw_checksum_val;

#ifdef CONFIG_CAM_DEBUG
	u8 dbg_level;
#endif

	int facedetect_mode;
	int running_capture_mode;
	int fd_eyeblink_cap;
	int fd_red_eye_status;
	int image_stabilizer_mode;
	int ot_status;
	int ot_x_loc;
	int ot_y_loc;
	int ot_width;
	int ot_height;
	int bracket_wbb_val;

	unsigned int face_beauty:1;
	unsigned int recording:1;
	unsigned int check_dataline:1;
	int anti_banding;
	int pixelformat;

	int wb_g_value;
	int wb_b_value;
	int wb_a_value;
	int wb_m_value;
	int wb_custom_rg;
	int wb_custom_bg;

	int fast_capture_set;

	int vss_mode;
	int dual_capture_start;
	int dual_capture_frame;

	int focus_mode;
	int focus_range;
	int focus_area_mode;

	int f_number;
	int iso;
	int numerator;
	int denominator;

	int AV;
	int TV;
	int SV;
	int EV;
	int LV;

	int smart_scene_detect_mode;

	int continueFps;

	int fd_num;

	int caf_state;

	int mode;

	bool stream_on_part2;

	int widget_mode_level;
	int gamma_rgb_mon;
	int gamma_rgb_cap;
	int gamma_tbl_rgb_mon;
	int gamma_tbl_rgb_cap;
	int color_effect;

	int preview_width;
	int preview_height;

	int mburst_start;

	int strobe_en;
	int sharpness;
	int saturation;

	int isp_mode;

	int af_running;
	int sys_status;

	int lenscap_bright_min;
	int lenscap_bright_max;
	int log_num;

	int lens_mem;
};

#define TOP_INFO_SIZE		( 20)
#define SDRAM_PARAM_SIZE	(144)
#define NAND_PARAM_SIZE		(225)
#define	CODE_INFO_SIZE		( 45)
#define	SECTION_PARAM		(100)
#define	USER_CODE_INFO		(490)

union top_info {
	char data [TOP_INFO_SIZE];
	struct {
		unsigned int block_size;
		unsigned int writer_load_size;		
		unsigned int write_code_entry;
		unsigned int sdram_param_size;
		unsigned int nand_param_size;	
	} __attribute__((packed)) bit;
};

union sdram_param {
	char data [SDRAM_PARAM_SIZE];
	struct {
		unsigned int reserved;		/* offset   0B ~   3B */
		unsigned int sdram_clk;		/* offset   4B ~   7B */
		unsigned int sditopscnf1;	/* offset   8B ~  11B */
		unsigned int sditopscnf2;	/* offset  12B ~  15B */	
		unsigned int param_1[25]; 	/* offset  16B ~ 115B */
		unsigned int sdiddrzq1;		/* offset 116B ~ 119B */
		unsigned int param_2[6]; 	/* offset 120B ~ 143B */
	} __attribute__((packed)) bit;
};

union nand_param {
	char data [NAND_PARAM_SIZE];
 } __attribute__((packed));

union code_info {
	char data [CODE_INFO_SIZE];
	struct {
		unsigned int code_size;
		unsigned int offset_code;
		char version_1[5];
		char log[12];
		char version_2[7];
		char model[13];
	} __attribute__((packed)) bit;
};

union section_info {
	char data [SECTION_PARAM];
} __attribute__((packed));

union user_code_info {
	char data [USER_CODE_INFO];
	struct {
		char pdr[18];
		char ddr[18];
		char epcr[18];
		char resv[436];
	} __attribute__((packed)) bit;
};

struct m7mu_fw_header {
	union top_info			top; 
	union sdram_param		sdram;
	union nand_param		nand;
	union code_info			code;
	union section_info		section;
	union user_code_info	user_code;
} __attribute__((packed));

/* Category */
#define M7MU_CATEGORY_SYS	0x00
#define M7MU_CATEGORY_PARM	0x01
#define M7MU_CATEGORY_MON	0x02
#define M7MU_CATEGORY_AE	0x03
#define M7MU_CATEGORY_NEW	0x04
#define M7MU_CATEGORY_PRO_MODE	0x05
#define M7MU_CATEGORY_WB	0x06
#define M7MU_CATEGORY_EXIF	0x07
#define M7MU_CATEGORY_OT    0x08
#define M7MU_CATEGORY_FD	0x09
#define M7MU_CATEGORY_LENS	0x0A
#define M7MU_CATEGORY_CAPPARM	0x0B
#define M7MU_CATEGORY_CAPCTRL	0x0C
#define M7MU_CATEGORY_TEST	0x0D
#define M7MU_CATEGORY_ADJST	0x0E
#define M7MU_CATEGORY_FLASH	0x0F    /* F/W update */

/* M7MU_CATEGORY_SYS: 0x00 */
#define M7MU_SYS_PJT_CODE	0x01
#define M7MU_SYS_VER_FW		0x02
#define M7MU_SYS_VER_HW		0x04
#define M7MU_SYS_VER_PARAM	0x06
#define M7MU_SYS_VER_AWB	0x08
#define M7MU_SYS_USER_VER	0x0A
#define M7MU_SYS_MODE		0x0B
#define M7MU_SYS_STATUS		0x0C
#define M7MU_SYS_ESD_INT	0x0E

#define M7MU_SYS_INT_EN        0x10
#define M7MU_SYS_INT_FACTOR    0x1C
#define M7MU_SYS_FRAMESYNC_CNT 0x14
#define M7MU_SYS_LENS_TIMER    0x28
#define M7MU_SYS_ISP_RESET     0x30

#define M7MU_SYS_MEM_ADDR      0x31
#define M7MU_SYS_MEM_SIZE      0x35

/* M7MU_CATEGORY_PARAM: 0x01 */
#define M7MU_PARM_OUT_SEL	0x00
#define M7MU_PARM_MON_SIZE	0x01
#define M7MU_PARM_MON_FPS	0x02
#define M7MU_PARM_EFFECT	0x0B
#define M7MU_PARM_FLEX_FPS	0x67
#define M7MU_PARM_HDMOVIE	0x32
#define M7MU_PARM_VIDEO_SNAP_IMG_TRANSFER_START 0x3A
#define M7MU_PARM_SEL_FRAME_VIDEO_SNAP 0x3B
#define M7MU_PARM_MON_MOVIE_SELECT	0x3C
#define M7MU_PARM_VSS_MODE 0x6E
#define M7MU_PARM_SET_PRE_LIVEVIEW 0x7C
#define M7MU_PARM_SET_ERASER_MODE 0x10
#define M7MU_PARAM_DEFECT_NOISE_LEVEL_TABLE  0x90
#define M7MU_PARAM_DEFECT_NOISE_LEVEL_MODE  0x91
#define M7MU_PARAM_DEFECT_NOISE_LEVEL_MIN_NUM  0x92
#define M7MU_PARAM_DEFECT_NOISE_LEVEL_MAX_NUM  0x93
#define M7MU_PARAM_DEFECT_NOISE_LEVEL_ISO_NUM  0x94
#define M7MU_PARAM_DEFECT_NOISE_LEVEL_EXP_TIME  0x95
#define M7MU_PARAM_DEFECT_NOISE_LEVEL_REPEAT_NUM  0x96
#define M7MU_PARAM_MEMORY_COMPARE  0x97

/* M7MU_CATEGORY_MON: 0x02 */
#define M7MU_MON_ZOOM		0x01
#define M7MU_MON_HR_ZOOM    0x04
#define M7MU_MON_MON_REVERSE	0x05
#define M7MU_MON_MON_MIRROR	0x06
#define M7MU_MON_SHOT_REVERSE	0x07
#define M7MU_MON_SHOT_MIRROR	0x08
#define M7MU_MON_CFIXB		0x09
#define M7MU_MON_CFIXR		0x0A
#define M7MU_MON_COLOR_EFFECT	0x0B
#define M7MU_MON_CHROMA_LVL	0x0F
#define M7MU_MON_EDGE_LVL	0x11
#define M7MU_MON_ATSCENE_DETECT_START	0x1C
#define M7MU_MON_RECORDING_STATE	0x1E
#define M7MU_MON_EDGE_CTRL	0x20
#define M7MU_MON_POINT_COLOR	0x22
#define M7MU_MON_TONE_CTRL	0x25
#define M7MU_MON_AF_SET_TOUCH		0x46
#define M7MU_MON_START_VIDEO_SNAP_SHOT 0x56
#define M7MU_MON_VIDEO_SNAP_SHOT_FRAME_COUNT 0x57
#define M7MU_MON_TOUCH_AE_WINDOW_POSITION_X 0xA6
#define M7MU_MON_TOUCH_AE_WINDOW_POSITION_Y 0xA8
#define M7MU_MON_TOUCH_AE_WINDOW_WIDTH 0xAA
#define M7MU_MON_TOUCH_AE_WINDOW_HEIGHT 0xAC
#define M7MU_MON_TOUCH_AE_START 0xAE
#define M7MU_MON_AF_FPS_CHECK 0xAF
#define M7MU_MON_MIPI_CNT 0xC0

/* M7MU_CATEGORY_AE: 0x03 */
#define M7MU_AE_LOCK		0x00
#define M7MU_AE_MODE		0x01
#define M7MU_AE_ISOSEL		0x05
#define M7MU_AE_FLICKER		0x06
#define M7MU_AE_INDEX		0x09
#define M7MU_AE_EP_MODE_MON	0x0A
#define M7MU_AE_EP_MODE_CAP	0x0B
#define M7MU_AF_AE_LOCK		0x0D
#define M7MU_AE_AUTO_BRACKET_EV	0x20
#define M7MU_AE_STABILITY	0x21
#define M7MU_AE_EV_PRG_MODE_CAP	0x34
#define M7MU_AE_EV_PRG_MODE_MON	0x35
#define M7MU_AE_EV_PRG_F_NUMBER	0x36
#define M7MU_AE_EV_PRG_SS_NUMERATOR		0x37
#define M7MU_AE_EV_PRG_SS_DENOMINATOR	0x39
#define M7MU_AE_EV_PRG_ISO_VALUE	0x3B
#define M7MU_AE_EV_PRG_F_NUMBER_MON				0x3D
#define M7MU_AE_EV_PRG_SS_NUMERATOR_MON		0x3E
#define M7MU_AE_EV_PRG_SS_DENOMINATOR_MON		0x40
#define M7MU_AE_EV_PRG_ISO_VALUE_MON			0x42
#define M7MU_AE_NOW_AV	0x54
#define M7MU_AE_NOW_TV	0x58
#define M7MU_AE_NOW_SV	0x5C
#define M7MU_AE_NOW_LV	0x52

/* M7MU_CATEGORY_NEW: 0x04 */
#define M7MU_NEW_TIME_INFO		0x02
#define M7MU_NEW_OIS_CUR_MODE	0x06
#define M7MU_NEW_OIS_TIMER		0x07
#define M7MU_NEW_DETECT_SCENE	0x0B
#define M7MU_NEW_OIS_VERSION	0x1B
#define M7MU_NEW_LENS_OFF		0x40

/* M7MU_CATEGORY_PRO_MODE: 0x05 */
#define M7MU_PRO_SMART_READ1		0x20
#define M7MU_PRO_SMART_READ2		0x24
#define M7MU_PRO_SMART_READ3		0x28
#define M7MU_PRO_MAGIC_SHOT         0x01

/* for NSM Mode */
#define M7MU_CAMERA_NSM_SYSTEM	0x30	//NSM Mode & Data : system, state, reset
#define M7MU_CAMERA_NSM_MODE	0X31	// Color Mode : at once(1~9) seperaded(a~21)
#define M7MU_CAMERA_NSM_DATA1	0X32	// Color Data1
#define M7MU_CAMERA_NSM_DATA2	0X33	// Color Data2
#define M7MU_CAMERA_NSM_DATA3	0X34	// Color Data3

#define M7MU_CAMERA_NSM_FD_FIRST_W	0X5D	// FD_INFO : 0x5D~0x5E
#define M7MU_CAMERA_NSM_FD_LAST_B	0X5F	// FD_INFO : 0x5F
/* end NSM Mode */

/* M7MU_CATEGORY_WB: 0x06 */
#define M7MU_AWB_LOCK		0x00
#define M7MU_WB_AWB_MODE	0x02
#define M7MU_WB_AWB_MANUAL	0x03
#define M7MU_WB_GBAM_MODE	0x8D
#define M7MU_WB_G_VALUE		0x8E
#define M7MU_WB_B_VALUE		0x8F
#define M7MU_WB_A_VALUE		0x90
#define M7MU_WB_M_VALUE		0x91
#define M7MU_WB_K_VALUE		0x92
#define M7MU_WB_CWB_MODE		0x93
#define M7MU_WB_SET_CUSTOM_RG	0x94
#define M7MU_WB_SET_CUSTOM_BG	0x96
#define M7MU_WB_GET_CUSTOM_RG	0x98
#define M7MU_WB_GET_CUSTOM_BG	0x9A
#define M7MU_WB_WBB_MODE		0x9C
#define M7MU_WB_WBB_AB		0x9D
#define M7MU_WB_WBB_GM		0x9E
#define M7MU_WB_COLOR_ADJ	0x9F

/* M7MU_CATEGORY_EXIF: 0x07 */
#define M7MU_EXIF_EXPTIME_NUM	0x00
#define M7MU_EXIF_EXPTIME_DEN	0x04
#define M7MU_EXIF_TV_NUM	0x08
#define M7MU_EXIF_TV_DEN	0x0C
#define M7MU_EXIF_BV_NUM	0x18
#define M7MU_EXIF_BV_DEN	0x1C
#define M7MU_EXIF_EBV_NUM	0x20
#define M7MU_EXIF_EBV_DEN	0x24
#define M7MU_EXIF_ISO		0x28
#define M7MU_EXIF_FLASH		0x2A
#define M7MU_EXIF_AV_NUM	0x10
#define M7MU_EXIF_AV_DEN	0x14
#define M7MU_EXIF_FL		0x11
#define M7MU_EXIF_FL_35		0x13
#define M7MU_EXIF_F_NUM		0x2E

/* M7MU_CATEGORY_OT: 0x08 */
#define M7MU_OT_TRACKING_CTL		0x00
#define M7MU_OT_INFO_READY			0x01
#define M7MU_OT_X_START_LOCATION	0x05
#define M7MU_OT_Y_START_LOCATION	0x07
#define M7MU_OT_X_END_LOCATION		0x09
#define M7MU_OT_Y_END_LOCATION		0x0B
#define M7MU_OT_TRACKING_X_LOCATION	0x10
#define M7MU_OT_TRACKING_Y_LOCATION	0x12
#define M7MU_OT_TRACKING_FRAME_WIDTH	0x14
#define M7MU_OT_TRACKING_FRAME_HEIGHT	0x16
#define M7MU_OT_TRACKING_STATUS		0x18
#define M7MU_OT_FRAME_WIDTH			0x30

/* M7MU_CATEGORY_FD: 0x09 */
#define M7MU_FD_CTL					0x00
#define M7MU_FD_SIZE				0x01
#define M7MU_FD_MAX					0x02
#define M7MU_FD_FOCUS_SELECT			0x03
#define M7MU_FD_READ_SEL			0x0B
#define M7MU_FD_X_LOCATION			0x0E
#define M7MU_FD_FRAME_WIDTH			0x12
#define M7MU_FD_RED_EYE				0x55
#define M7MU_FD_RED_DET_STATUS		0x56
#define M7MU_FD_BLINK_FRAMENO		0x59
#define M7MU_FD_BLINK_LEVEL_1		0x5A
#define M7MU_FD_BLINK_LEVEL_2		0x5B
#define M7MU_FD_BLINK_LEVEL_3		0x5C
#define M7MU_FD_ROI_X_LOCATION		0x60
#define M7MU_FD_ROI_Y_LOCATION		0x62
#define M7MU_FD_ROI_FRAME_WIDTH		0x64
#define M7MU_FD_ROI_FRAME_HEIGHT		0x66

/* M7MU_CATEGORY_LENS: 0x0A */
#define M7MU_LENS_AF_INITIAL		0x00
#define M7MU_LENS_AF_LENS_CLOSE		0x01
#define M7MU_LENS_AF_ZOOM_CTRL		0x02
#define M7MU_LENS_AF_START_STOP		0x03
#define M7MU_LENS_AF_IRIS_STEP		0x05
#define M7MU_LENS_AF_ZOOM_LEVEL		0x06
#define M7MU_LENS_AF_SCAN_RANGE		0x07
#define M7MU_LENS_AF_MODE			0x08
#define M7MU_LENS_AF_WINDOW_MODE	0x09
#define M7MU_LENS_AF_BACKLASH_ADJ	0x0A
#define M7MU_LENS_AF_FOCUS_ADJ		0x0B
#define M7MU_LENS_AF_TILT_ADJ		0x0C
#define M7MU_LENS_AF_AF_ADJ			0x0D
#define M7MU_LENS_AF_PUNT_ADJ		0x0E
#define M7MU_LENS_AF_ZOOM_ADJ		0x0F
#define M7MU_LENS_AF_ADJ_TEMP_VALUE	0x0C
#define M7MU_LENS_AF_ALGORITHM		0x0D
#define M7MU_LENS_ZOOM_LEVEL_INFO	0x10
#define M7MU_LENS_AF_LED			0x1C
#define M7MU_LENS_AF_CAL			0x1D
#define M7MU_LENS_AF_RESULT			0x20
#define M7MU_LENS_ZOOM_SET_INFO		0x22
#define M7MU_LENS_ZOOM_SPEED		0x25
#define M7MU_LENS_ZOOM_STATUS		0x26
#define M7MU_LENS_LENS_STATUS		0x28
#define M7MU_LENS_ZOOM_LENS_STATUS	0x2A
#define M7MU_LENS_AF_TEMP_INDICATE	0x2B
#define M7MU_LENS_TIMER_LED			0x2D
#define M7MU_LENS_AF_TOUCH_POSX		0x30
#define M7MU_LENS_AF_TOUCH_POSY		0x32
#define M7MU_LENS_AF_ADJ_OK_NG		0x40
#define M7MU_LENS_AF_FOCUS_CHECK		0x42
#define M7MU_LENS_MF_DEFAULT_INDEX	0x50
#define M7MU_LENS_MF_NEARLIMIT_INDEX	0x51
#define M7MU_LENS_MF_MAX_INDEX		0x52
#define M7MU_LENS_MF_RUN			0x53
#define M7MU_LENS_ZOOM_ACTION_METHOD	0x54
#define M7MU_LENS_AF_VERSION		0x60
#define M7MU_LENS_MEM_INIT_TARGET	0x96
#define M7MU_LENS_DATA_BACKUP		0x97
#define M7MU_LENS_CHECK_SUM			0xC3

/* M7MU_CATEGORY_CAPPARM: 0x0B */
#define M7MU_CAPPARM_YUVOUT_MAIN	0x00
#define M7MU_CAPPARM_MAIN_IMG_SIZE	0x01
#define M7MU_CAPPARM_YUVOUT_PREVIEW	0x05
#define M7MU_CAPPARM_PREVIEW_IMG_SIZE	0x06
#define M7MU_CAPPARM_YUVOUT_THUMB	0x0A
#define M7MU_CAPPARM_THUMB_IMG_SIZE	0x0B
#define M7MU_CAPPARM_JPEG_SIZE_MAX	0x0F
#define M7MU_CAPPARM_JPEG_SIZE_MIN	0x13
#define M7MU_CAPPARM_JPEG_RATIO		0x17
#define M7MU_CAPPARM_MCC_MODE		0x1D
#define M7MU_CAPPARM_STROBE_EN		0x22
#define M7MU_CAPPARM_STROBE_CHARGE	0x27
#define M7MU_CAPPARM_STROBE_EVC		0x28
#define M7MU_CAPPARM_STROBE_UP_DOWN	0x29
#define M7MU_CAPPARM_WDR_EN			0x2C
#define M7MU_CAPPARM_JPEG_RATIO_OFS	0x1B
#define M7MU_CAPPARM_THUMB_JPEG_MAX	0x3C
#define M7MU_CAPPARM_STROBE_BATT_INFO	0x3F
#define M7MU_CAPPARM_AFB_CAP_EN		0x53

/* M7MU_CATEGORY_CAPCTRL: 0x0C */
#define M7MU_CAPCTRL_CAP_MODE	0x00
#define M7MU_CAPCTRL_CAP_FRM_INTERVAL 0x01
#define M7MU_CAPCTRL_CAP_FRM_COUNT 0x02
#define M7MU_CAPCTRL_START_DUALCAP 0x05
#define M7MU_CAPCTRL_FRM_SEL	0x06
#define M7MU_CAPCTRL_FRM_PRV_SEL	0x07
#define M7MU_CAPCTRL_FRM_THUMB_SEL	0x08
#define M7MU_CAPCTRL_TRANSFER	0x09
#define M7MU_CAPCTRL_IMG_SIZE	0x0D
#define M7MU_CAPCTRL_THUMB_SIZE	0x11
#define M7MU_CAPCTRL_AUTOMATIC_SHIFT_EN	0x25

/* M7MU_CATEGORY_CAPCTRL: 0x0C  M7MU_CAPCTRL_CAP_MODE: 0x00 */
#define M7MU_CAP_MODE_SINGLE_CAPTURE		(0x00)
#define M7MU_CAP_MODE_MULTI_CAPTURE			(0x01)
#define M7MU_CAP_MODE_DUAL_CAPTURE			(0x05)
#define M7MU_CAP_MODE_BRACKET_CAPTURE		(0x06)
#define M7MU_CAP_MODE_ADDPIXEL_CAPTURE		(0x08)
#define M7MU_CAP_MODE_PANORAMA_CAPTURE		(0x0B)
#define M7MU_CAP_MODE_BLINK_CAPTURE			(0x0C)
#define M7MU_CAP_MODE_RAW			(0x0D)

/* M7MU_CATEGORY_ADJST: 0x0E */
#define M7MU_ADJST_SHUTTER_MODE	0x33
#define M7MU_ADJST_CLIP_VALUE1	0x34
#define M7MU_ADJST_CLIP_VALUE2	0x35
#define M7MU_ADJST_AWB_RG_H 0x3C
#define M7MU_ADJST_AWB_RG_L 0x3D
#define M7MU_ADJST_AWB_BG_H 0x3E
#define M7MU_ADJST_AWB_BG_L 0x3F
#define M7MU_ADJST_EEP_RW   0x9C
#define M7MU_ADJST_IR_CHECK 0x88
#define M7MU_ADJST_LOG_REQUEST 0x53
#define M7MU_ADJST_LOG_REQUEST2 0xA0
#define M7MU_ADJST_SHADING_DUMP 0xA1
#define M7MU_ADJST_ADJUST_TBL_DUMP 0xA2

/* M7MU_CATEGORY_FLASH: 0x0F */
#define M7MU_FLASH_ADDR		0x00
#define M7MU_FLASH_BYTE		0x04
#define M7MU_FLASH_ERASE	0x06
#define M7MU_FLASH_WR		0x07
#define M7MU_FLASH_RAM_CLEAR	0x08
#define M7MU_FLASH_CAM_START_AD		0x0C
#define M7MU_FLASH_CAM_START	0x12
#define M7MU_FLASH_SEL		0x13
#define M7MU_FLASH_DATA_RAM_ADDR 	0x14
#define M7MU_FLASH_SIO_CS_POL		0x49
#define M7MU_FLASH_RAM_START		0x4A
#define M7MU_FLASH_SMR_VALUE		0x4B
#define M7MU_FLASH_SDITOPSCNF2		0x4c
#define M7MU_FLASH_SDITOPSCNF3		0x50
#define M7MU_FLASH_SDITOPSCNF4		0x54
#define M7MU_FLASH_SDIPHYIMP1		0x58
#define M7MU_FLASH_SDIPHYIMP2		0x5c
#define M7MU_FLASH_SDIPHYIOD1		0x60
#define M7MU_FLASH_SDIPHYIOD2		0x64
#define M7MU_FLASH_SDIPHYIOD3		0x68
#define M7MU_FLASH_SDIPHYIOD4		0x6c
#define M7MU_FLASH_SDIDDRMR0		0x70
#define M7MU_FLASH_SDIDDRMR1		0x74
#define M7MU_FLASH_SDIDDRMR2		0x78
#define M7MU_FLASH_SDIDDRMR3		0x7c
#define M7MU_FLASH_SDIDDRCNF1		0x80
#define M7MU_FLASH_SDIDDRAC1		0x84
#define M7MU_FLASH_SDIDDRAC2		0x88
#define M7MU_FLASH_SDIDDRAC5		0x8c
#define M7MU_FLASH_SDIDDRAC6		0x90
#define M7MU_FLASH_SDIDDRAC7		0x94
#define M7MU_FLASH_SDIDDRAC8		0x98
#define M7MU_FLASH_SDIDDRAC9		0x9c
#define M7MU_FLASH_SDIDDRACA		0xA0
#define M7MU_FLASH_SDIDDRACC		0xA4
#define M7MU_FLASH_SDIDDRACD		0xA8
#define M7MU_FLASH_SDIDDRPHY1		0xAC
#define M7MU_FLASH_SDIDDRAR1		0xB0
#define M7MU_FLASH_SDIDDRZQ1		0xB4
#define M7MU_FLASH_DPLSEL			0xB8

/* M7MU_CATEGORY_TEST:	0x0D */
#define M7MU_TEST_OUTPUT_YCO_TEST_DATA	0x1B
#define M7MU_TEST_ISP_PROCESS		0x59

/* M7MU Sensor Mode */
#define M7MU_SYSINIT_MODE	0x0
#define M7MU_PARMSET_MODE	0x1
#define M7MU_MONITOR_MODE	0x2
#define M7MU_STILLCAP_MODE	0x3

/* Interrupt Factor */
#define M7MU_INT_SOUND		(1 << 15)
#define M7MU_INT_LENS_INIT	(1 << 14)
#define M7MU_INT_FD		(1 << 13)
#define M7MU_INT_FRAME_SYNC	(1 << 12)
#define M7MU_INT_CAPTURE	(1 << 11)
#define M7MU_INT_ZOOM		(1 << 10)
#define M7MU_INT_AF		(1 << 9)
#define M7MU_INT_MODE		(1 << 8)
#define M7MU_INT_ATSCENE	(1 << 7)
#define M7MU_INT_ATSCENE_UPDATE	(1 << 6)
#define M7MU_INT_AF_STATUS		(1 << 5)
#define M7MU_INT_OIS_SET	(1 << 4)
#define M7MU_INT_OIS_INIT	(1 << 3)
#define M7MU_INT_STNW_DETECT	(1 << 2)
#define M7MU_INT_SCENARIO_FIN	(1 << 1)
#define M7MU_INT_PRINT	(1 << 0)

/* ESD Interrupt */
#define M7MU_INT_ESD		(1 << 0)

extern int m7mu_spi_read(u8 *buf, size_t len, const int rxSize);
extern int m7mu_spi_write(const u8 *addr, const int len, const int txSize);
extern int m7mu_spi_init(void);

#endif /* __M7MU_H */

