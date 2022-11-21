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
#include "stddef.h"
#include "cam_ois_info.h"

// OIS list
#include "ois_drv1.h"

static struct cam_ois_info g_ois_info[IMGSENSOR_SENSOR_IDX_MAX_NUM] = {
	{OIS_I2C_DEV_IDX_1, &ois_drv1_func},/* main */
	{OIS_I2C_DEV_NONE,  NULL},/* sub */
	{OIS_I2C_DEV_NONE,  NULL},/* main2 */
	{OIS_I2C_DEV_NONE,  NULL},/* sub2 */
	{OIS_I2C_DEV_NONE,  NULL},/* main3 */
};

struct cam_ois_info *get_cam_ois_info(enum IMGSENSOR_SENSOR_IDX idx)
{
	if (idx >= IMGSENSOR_SENSOR_IDX_MIN_NUM &&
		idx < IMGSENSOR_SENSOR_IDX_MAX_NUM)
		return &g_ois_info[idx];

	return &g_ois_info[IMGSENSOR_SENSOR_IDX_MAIN];
}
