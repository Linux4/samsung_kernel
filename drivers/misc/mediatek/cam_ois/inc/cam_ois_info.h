/*
 * Copyright (C) 2021 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */
#ifndef __CAM_OIS_INFO__
#define __CAM_OIS_INFO__

#include "kd_camera_feature.h"

enum OIS_I2C_DEV_IDX {
	OIS_I2C_DEV_NONE  = -1,
	OIS_I2C_DEV_IDX_1 = 0,
	OIS_I2C_DEV_IDX_2,
	OIS_I2C_DEV_IDX_3,
	OIS_I2C_DEV_IDX_MAX
};

enum OIS_CMD {
	OIS_CMD_MODE  = 0,
	OIS_CMD_DATA_READ,
	OIS_CMD_2,
	OIS_CMD_3,
	OIS_CMD_MAX
};

struct ois_ctrl_info {
	enum OIS_CMD cmd;
	int *param;
	int param_length;
};

struct cam_ois_func {
	int (*ois_open)(void);
	int (*ois_close)(void);
	int (*ois_control)(struct ois_ctrl_info *ctrl_info);
};

struct cam_ois_info {
	enum OIS_I2C_DEV_IDX i2c_chanel;
	struct cam_ois_func *ois_func;
};


struct cam_ois_info *get_cam_ois_info(enum IMGSENSOR_SENSOR_IDX idx);

#endif /* __CAM_OIS_INFO__ */
