/* Copyright (c) 2011-2013, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#ifndef MSM_OIS_H
#define MSM_OIS_H

#include <linux/i2c.h>
#include <linux/gpio.h>
#include <soc/qcom/camera2.h>
#include <media/v4l2-subdev.h>
#include <media/msmb_camera.h>
#include <media/msm_cam_sensor.h>
#include "msm_camera_i2c.h"
#include "msm_camera_dt_util.h"
#include "msm_camera_io_util.h"

#define DEFINE_MSM_MUTEX(mutexname) \
    static struct mutex mutexname = __MUTEX_INITIALIZER(mutexname)

#define MSM_OIS_MAX_VREGS (10)
#define NUM_AF_POSITION (512)

#define SCALE (10000)
#define Coef_angle_max (3500)	// unit : 1/SCALE, OIS Maximum compensation angle, 0.35*SCALE
#define SH_THRES (798000)		// unit : 1/SCALE, 39.9*SCALE
#define SWAP32(x) ((((x) & 0xff000000) >> 24) | (((x) & 0x00ff0000) >> 8) | (((x) & 0x0000ff00) << 8) | (((x) & 0x000000ff) << 24))
#define RND_DIV(num, den) ((num > 0) ? (num+(den>>1))/den : (num-(den>>1))/den)
#define Gyrocode (1000) // Gyro input code for 1 angle degree
#define IMAGE_SHIFT_VALUE_SIZE (8)
#define ABS(a)				((a) > 0 ? (a) : -(a))

enum {
	eBIG_ENDIAN = 0, // big endian
	eLIT_ENDIAN = 1  // little endian
};


enum msm_ois_state_t {
	OIS_POWER_UP,
	OIS_POWER_DOWN,
};

enum msm_ois_module_t {
	OIS_MODULE_NONE,
	OIS_MODULE_1 = 1, //wide
	OIS_MODULE_2 = 2, //tele
	OIS_MODULE_DUAL = 3, // dual
	OIS_MODULE_4 = 7, //seperate 2 cameras
};

struct msm_ois_vreg {
	struct camera_vreg_t *cam_vreg;
	void *data[MSM_OIS_MAX_VREGS];
	int num_vreg;
};

struct msm_ois_ver_t {
	uint8_t  core_ver;
	uint8_t  gyro_sensor;
	uint8_t  driver_ic;
#if 1//defined(SAMSUNG_OIS_RUMBA_S4)
	uint8_t  year;
#endif
	uint8_t  month;
	uint8_t  iteration_0;
	uint8_t  iteration_1;
};

struct msm_ois_debug_t {
    int   err_reg;
    int   status_reg;
    char  phone_ver[MSM_OIS_VER_SIZE+1];
    char  module_ver[MSM_OIS_VER_SIZE+1];
    char  cal_ver[MSM_OIS_VER_SIZE+1];
};

struct msm_ois_shift_table_t {
    bool ois_shift_used;
    int16_t ois_shift_x[NUM_AF_POSITION];
    int16_t ois_shift_y[NUM_AF_POSITION];
};

struct msm_ois_sinewave_t {
    int sin_x;
    int sin_y;
};

struct msm_ois_ctrl_t {
    struct i2c_driver *i2c_driver;
    struct platform_driver *pdriver;
    struct platform_device *pdev;
    struct msm_camera_i2c_client i2c_client;
    enum msm_camera_device_type_t ois_device_type;
    struct msm_sd_subdev msm_sd;
    enum af_camera_name cam_name;
    struct mutex *ois_mutex;
    struct v4l2_subdev sdev;
    struct v4l2_subdev_ops *ois_v4l2_subdev_ops;
    enum cci_i2c_master_t cci_master;
    uint32_t subdev_id;
    enum msm_ois_state_t ois_state;
    enum msm_camera_i2c_data_type i2c_data_type;
    struct msm_ois_vreg vreg_cfg;
    struct msm_camera_gpio_conf *gpio_conf;
    struct msm_ois_ver_t phone_ver;
    struct msm_ois_ver_t module_ver;
    struct msm_ois_cal_info_t cal_info;
    char load_fw_name[256];
    bool is_camera_run;
    bool is_set_debug_info;
    struct msm_ois_debug_t debug_info;
    struct msm_ois_shift_table_t shift_tbl[2];
    bool is_shift_enabled;
    bool is_init_done;
    bool is_force_update;
    bool is_servo_on;
    bool is_image_shift_cal_set;
    bool is_image_shift_cal_done;
    int16_t wide_x_shift;
    int16_t wide_y_shift;
    int16_t tele_x_shift;
    int16_t tele_y_shift;
    uint8_t *image_shift_cal;
    uint16_t module;
};

enum msm_ois_modes {
    OIS_MODE_OFF       = 1,
    OIS_MODE_ON        = 2,
    OIS_MODE_ON_STILL  = 3,
    OIS_MODE_ON_ZOOM   = 4,
    OIS_MODE_ON_VIDEO  = 5,
    OIS_MODE_SINE_X    = 6,
    OIS_MODE_SINE_Y    = 7,
    OIS_MODE_CENTERING = 8,
    OIS_MODE_ON_VDIS   = 9,
    OIS_MODE_MAX,
};

int msm_ois_i2c_byte_read(struct msm_ois_ctrl_t *a_ctrl, uint32_t addr, uint16_t *data);
int msm_ois_i2c_byte_write(struct msm_ois_ctrl_t *a_ctrl, uint32_t addr, uint16_t data);
int msm_ois_i2c_seq_read(struct msm_ois_ctrl_t *a_ctrl, uint32_t addr, uint8_t *data, uint32_t size);
int msm_ois_i2c_seq_write(struct msm_ois_ctrl_t *a_ctrl, uint32_t addr, uint8_t *data, uint32_t size);

void msm_is_ois_offset_test(long *raw_data_x, long *raw_data_y, bool is_need_cal);
u8 msm_is_ois_self_test(void);
int msm_ois_create_shift_table(struct msm_ois_ctrl_t *a_ctrl, uint8_t *shift_data);
int msm_ois_shift_calibration(uint16_t af_position, uint16_t subdev_id);
int msm_ois_wait_idle(struct msm_ois_ctrl_t *a_ctrl, int retries);

#endif
