/* Copyright (c) 2011-2016, The Linux Foundation. All rights reserved.
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
#ifndef MSM_APERTURE_H
#define MSM_APERTURE_H

#include <linux/i2c.h>
#include <linux/gpio.h>
#include <soc/qcom/camera2.h>
#include <media/v4l2-subdev.h>
#include <media/msmb_camera.h>
#include "msm_camera_i2c.h"
#include "msm_camera_dt_util.h"
#include "msm_camera_io_util.h"

#define DEFINE_MSM_MUTEX(mutexname) \
	static struct mutex mutexname = __MUTEX_INITIALIZER(mutexname)

#define	MSM_APERTURE_MAX_VREGS (10)

struct msm_aperture_ctrl_t;

enum msm_aperture_state_t {
	ACT_ENABLE_STATE,
	ACT_OPS_ACTIVE,
	ACT_OPS_INACTIVE,
	ACT_DISABLE_STATE,
};

struct msm_aperture_func_tbl {
	int32_t (*aperture_power_up)(struct msm_aperture_ctrl_t *a_ctrl);
	int32_t (*aperture_power_down)(struct msm_aperture_ctrl_t *a_ctrl);
	int (*aperture_iris_init)(struct msm_aperture_ctrl_t *a_ctrl);
	int (*aperture_iris_set_value)(struct msm_aperture_ctrl_t *a_ctrl, int value);
};

struct msm_aperture_vreg {
	struct camera_vreg_t *cam_vreg;
	void *data[MSM_APERTURE_MAX_VREGS];
	int num_vreg;
};

struct msm_aperture_func_tbl aperture_func_tbl;

struct msm_aperture_ctrl_t {
	struct i2c_driver *i2c_driver;
	struct platform_driver *pdriver;
	struct platform_device *pdev;
	struct msm_camera_i2c_client i2c_client;
	enum msm_camera_device_type_t act_device_type;
	struct msm_sd_subdev msm_sd;
	struct mutex *aperture_mutex;
	struct msm_aperture_func_tbl *func_tbl;
	enum msm_camera_i2c_data_type i2c_data_type;
	struct v4l2_subdev sdev;
	struct v4l2_subdev_ops *act_v4l2_subdev_ops;
	uint16_t f_number;
	unsigned short slave_addr;	

	void *user_data;
	struct msm_camera_i2c_reg_array *i2c_reg_tbl;
	uint16_t i2c_tbl_index;
	enum cci_i2c_master_t cci_master;
	uint32_t subdev_id;
	enum msm_aperture_state_t aperture_state;
	struct msm_aperture_vreg vreg_cfg;

	struct msm_camera_gpio_conf *gconf;
	struct msm_pinctrl_info pinctrl_info;
	uint8_t cam_pinctrl_status;
};

#ifdef CONFIG_COMPAT
static long msm_aperture_subdev_fops_ioctl32(struct file *file, unsigned int cmd,
	unsigned long arg);
#endif

#endif
