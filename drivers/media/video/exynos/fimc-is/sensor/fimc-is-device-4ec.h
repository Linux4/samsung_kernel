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
//#include <linux/videodev2_exynos_camera_ext.h>

#define SENSOR_M7MU_INSTANCE	1
#define SENSOR_M7MU_NAME	SENSOR_NAME_M7MU
#define SENSOR_M7MU_DRIVING

#if 0
#define M7MU_FW_VER_LEN		22
#define M7MU_FW_VER_FILE_CUR	0x16FF00
#define M7MU_FW_VER_NUM		0x000018
#else
#define M7MU_FW_VER_LEN	20
#define M7MU_FW_VER_TOKEN 7
#define M7MU_SENSOR_TYPE_LEN 25
#define M7MU_SEN_FW_VER_LEN	30
#define M7MU_FW_VER_FILE_CUR	0x1FF080
#define M7MU_FW_VER_NUM		0x1FF080
#endif


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

struct m7mu_frmsizeenum {
	unsigned int index;
	unsigned int target_width;	/* FIMC output width */
	unsigned int target_height;	/* FIMC output height */
	unsigned int sensor_width;	/* CIS Input width */
	unsigned int sensor_height;	/* CIS Input height */
	u8 reg_val;		/* a value for category parameter */
};

struct m7mu_version {
	u32 major;
	u32 minor;
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

struct m7mu_jpeg {
	u32 enable;
	int quality;
	unsigned int main_size;	/* Main JPEG file size */
	unsigned int thumb_size;	/* Thumbnail file size */
	unsigned int main_offset;
	unsigned int thumb_offset;
	unsigned int postview_offset;
};

struct m7mu_exif {
	char unique_id[7];
	u32 exptime;		/* us */
	u16 flash;
	u16 iso;
	int tv;			/* shutter speed */
	int bv;			/* brightness */
	int ebv;		/* exposure bias */
	int av;			/* Aperture */
	int focal_length;
	int focal_35mm_length;
};

struct m7mu_isp {
	wait_queue_head_t wait;
	wait_queue_head_t boot_wait;
	unsigned int irq;	/* irq issued by ISP */
	unsigned int issued;
	unsigned int boot_irq;
	unsigned int boot_issued;
	unsigned int int_factor;
	unsigned int boot_lens;
	unsigned int bad_fw:1;
};

struct m7mu_state {
	struct m7mu_platform_data *pdata;
	struct device *m7mu_dev;
	struct v4l2_subdev sd;
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
	//enum v4l2_sensor_mode sensor_mode;
	//enum v4l2_cam_flash_mode flash_mode;
	//enum v4l2_camera_scene_mode scene_mode;
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

	struct m7mu_focus focus;
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

	int lenscap_bright_min;
	int lenscap_bright_max;
	int log_num;
};


struct fimc_is_module_4ec {
	struct fimc_is_image	image;

	u16			vis_duration;
	u16			frame_length_line;
	u32			line_length_pck;
	u32			system_clock;

	struct m7mu_isp		isp;

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

	int vt_mode;
	int samsung_app;
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

	int lenscap_bright_min;
	int lenscap_bright_max;
	int log_num;
};

int sensor_4ec_probe(struct i2c_client *client,
	const struct i2c_device_id *id);

#endif

