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
#ifndef _CAMERA_OIS_H_
#define _CAMERA_OIS_H_

#include "cam_ois_info.h"

#ifdef CONFIG_CAMERA_OIS

int cam_ois_open(enum IMGSENSOR_SENSOR_IDX idx);
int cam_ois_close(enum IMGSENSOR_SENSOR_IDX idx);
int cam_ois_control(enum IMGSENSOR_SENSOR_IDX idx, struct ois_ctrl_info *crtl_info);
#else
int cam_ois_open(enum IMGSENSOR_SENSOR_IDX idx)
{
	return -1;
}

int cam_ois_close(enum IMGSENSOR_SENSOR_IDX idx)
{
	return -1;
}

int cam_ois_control(enum IMGSENSOR_SENSOR_IDX idx, struct ois_ctrl_info *crtl_info)
{
	return -1;
}

#endif
#endif /*_CAMERA_OIS_H_*/
