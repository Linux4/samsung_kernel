// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * Copyright (c) 2023 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef PABLO_PHY_TUNE_H
#define PABLO_PHY_TUNE_H

#include <linux/module.h>

int pablo_test_set_sensor_run(const char *val, const struct kernel_param *kp);

int pablo_test_set_phy_tune(const char *val, const struct kernel_param *kp);
u32 pablo_test_get_open_window(void);
u32 pablo_test_get_total_err(void);
u32 pablo_test_get_first_open(void);

extern bool fst_phy_check_step;

#endif
