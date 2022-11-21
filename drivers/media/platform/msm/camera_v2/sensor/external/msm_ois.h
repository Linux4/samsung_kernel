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
#define NUM_OIS_VERSION_STR (6)

enum msm_ois_state_t {
    OIS_POWER_UP,
    OIS_POWER_DOWN,
};


struct msm_ois_vreg {
    struct camera_vreg_t *cam_vreg;
    void *data[MSM_OIS_MAX_VREGS];
    int num_vreg;
};

struct msm_ois_ver_t {
    uint8_t  gyro_sensor;
    uint8_t  driver_ic;
    uint8_t  core_ver;
    uint8_t  month;
    uint8_t  iteration_0;
    uint8_t  iteration_1;
};

struct msm_ois_debug_t {
    int   err_reg;
    int   status_reg;
    char  phone_ver[NUM_OIS_VERSION_STR+1];
    char  module_ver[NUM_OIS_VERSION_STR+1];
    char  cal_ver[NUM_OIS_VERSION_STR+1];
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
};

enum msm_ois_modes{
    OIS_MODE_OFF       = 1,
    OIS_MODE_ON        = 2,
    OIS_MODE_ON_STILL  = 3,
    OIS_MODE_ON_ZOOM   = 4,
    OIS_MODE_ON_VIDEO  = 5,
    OIS_MODE_SINE_X    = 6,
    OIS_MODE_SINE_Y    = 7,
    OIS_MODE_CENTERING = 8,
    OIS_MODE_MAX,
};

void msm_is_ois_offset_test(long *raw_data_x, long *raw_data_y, bool is_need_cal);
u8 msm_is_ois_self_test(void);

#endif
