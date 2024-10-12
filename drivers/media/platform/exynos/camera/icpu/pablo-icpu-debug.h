/* SPDX-License-Identifier: GPL */
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * Exynos Pablo image subsystem functions
 *
 * Copyright (c) 2021 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef PABLO_ICPU_DEBUG_H
#define PABLO_ICPU_DEBUG_H

int pablo_icpu_debug_probe(void);
void pablo_icpu_debug_remove(void);
int pablo_icpu_debug_config(void *kva, size_t fw_mem_size);
void pablo_icpu_debug_reset(void);
void pablo_icpu_debug_fw_log_dump(void);

typedef unsigned long (*copy_to_buf_func_t)(void *to, const void *from, unsigned long n);

#if IS_ENABLED(CONFIG_PABLO_KUNIT_TEST)
int pablo_icpu_test_get_file_ops(struct file_operations *fops);
int pablo_icpu_debug_test_config(void *kva, size_t log_size, size_t margin, copy_to_buf_func_t func);
int pablo_icpu_debug_test_loglevel(u32 lv);
#endif
#endif
