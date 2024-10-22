/*
 * Copyright (c) 2020 Samsung Electronics Co., Ltd All Rights Reserved
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef _TZDEV_TESTS_H
#define _TZDEV_TESTS_H

#include <linux/kconfig.h>

#if IS_MODULE(CONFIG_TZDEV)
int efuse_module_init(void);
void efuse_module_exit(void);

int tz_chtest_drv_init(void);
void tz_chtest_drv_exit(void);

int tz_clk_test_helper_init(void);
void tz_clk_test_helper_exit(void);

int tz_memdump_test_init(void);
void tz_memdump_test_exit(void);

int tz_mmap_test_init(void);
void tz_mmap_test_exit(void);

int tz_printk_test_init(void);
void tz_printk_test_exit(void);

int tz_scma_test_init(void);
void tz_scma_test_exit(void);

int tz_tee_kernel_api_init(void);
void tz_tee_kernel_api_exit(void);

int tzasc_test_init(void);
void tzasc_test_exit(void);

int tzpc_test_init(void);
void tzpc_test_exit(void);
#endif

#endif /* _TZDEV_TESTS_H */
