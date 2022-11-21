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

static struct cam_ois_info g_ois_info[IMGSENSOR_SENSOR_IDX_MAX_NUM] = {
	NULL,/* main */
	NULL,/* sub */
	NULL,/* main2 */
	NULL,/* sub2 */
	NULL,/* main3 */
};

struct cam_ois_info *get_cam_ois_info(enum IMGSENSOR_SENSOR_IDX idx)
{
	if (idx >= IMGSENSOR_SENSOR_IDX_MIN_NUM &&
		idx < IMGSENSOR_SENSOR_IDX_MAX_NUM)
		return &g_ois_info[idx];

	return &g_ois_info[IMGSENSOR_SENSOR_IDX_MAIN];
}
