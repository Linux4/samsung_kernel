/*
 * Samsung Exynos SoC series Sensor driver
 *
 *
 * Copyright (c) 2021 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef IS_VENDER_TEST_SENSOR_H
#define IS_VENDER_TEST_SENSOR_H

#if !defined(CONFIG_USE_SIGNED_BINARY)
#define USE_SENSOR_DEBUG
int is_vender_caminfo_cmd_set_mipi_phy(void __user *user_data);
int is_vender_caminfo_cmd_get_mipi_phy(void __user *user_data);
#endif

#endif /* IS_VENDER_TEST_SENSOR_H */
