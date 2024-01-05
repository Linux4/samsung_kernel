/* Copyright (c) 2013-2014, 2018, The Linux Foundation. All rights reserved.
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

#ifndef MSM_SENSOR_DRIVER_H
#define MSM_SENSOR_DRIVER_H

#include "msm_sensor.h"

int32_t msm_sensor_driver_probe(void *setting,
	struct msm_sensor_info_t *probed_info, char *entity_name);

//+bug 600732,huangguoyong.wt, add,2020/11/16,86118 project camera kernel code for bring up
enum hi846_sensor_module_D
{
    HI846_SENSOR_MODULE_D_OTHER_SENSOR,
    HI846_SENSOR_MODULE_D_MATCH_NONE,
    HI846_SENSOR_MODULE_D_FRONT_SHINETECH_1,
    HI846_SENSOR_MODULE_D_FRONT_TXD_2,
    HI846_SENSOR_MODULE_D_WIDE_TRULY_1,
    HI846_SENSOR_MODULE_D_WIDE_SHINETECH_2,
};
//-bug 600732,huangguoyong.wt, add,2020/11/16,86118 project camera kernel code for bring up
#endif
