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

#ifndef IS_DEVICE_MODULE_BASE_H
#define IS_DEVICE_MODULE_BASE_H

#define PERI_SET_MODULE(mod) (((struct is_device_sensor_peri*)mod->private_data)->module = mod)

struct platform_driver *sensor_module_get_platform_driver(void);
#endif
