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

#ifndef PABLO_V4L2_TEST_H
#define PABLO_V4L2_TEST_H

#if IS_ENABLED(CONFIG_PABLO_UNIT_TEST)
int pablo_v4l2_test_init(void);
void pablo_v4l2_test_pre(struct file *file);
void pablo_v4l2_test_post(struct file *file);
int pablo_v4l2_test_get_result(struct file *file, char __user *buf, size_t count);
int pablo_v4l2_test_run_test(struct file *file, const char __user *buf, size_t count);
#else
#define pablo_v4l2_test_init() 0
#define pablo_v4l2_test_pre(file) do_nothing
#define pablo_v4l2_test_post(file) do_nothing
#define pablo_v4l2_test_get_result(file, buf, count) 0
#define pablo_v4l2_test_run_test(file, buf, count) 0
#endif

#endif /* PABLO_V4L2_TEST_H */
