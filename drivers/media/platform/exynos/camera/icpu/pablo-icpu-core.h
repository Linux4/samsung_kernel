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

#ifndef PABLO_ICPU_CORE_H
#define PABLO_ICPU_CORE_H

#include "pablo-icpu.h"

struct icpu_core {
	struct platform_device *pdev;

	void *icpu_mem;

	struct icpu_mbox_controller **tx_mboxes;
	struct icpu_mbox_controller **rx_mboxes;
	struct pablo_icpu_mbox_chan *chans;
};

int pablo_icpu_boot(void);
void pablo_icpu_power_down(void);
int pablo_icpu_preload_fw(unsigned long flag);

struct icpu_mbox_client;
enum icpu_mbox_chan_type;
struct pablo_icpu_mbox_chan *pablo_icpu_request_mbox_chan(struct icpu_mbox_client *cl,
		enum icpu_mbox_chan_type type);
void pablo_icpu_free_mbox_chan(struct pablo_icpu_mbox_chan *chan);
void pablo_icpu_handle_msg(u32 *rx_data, u32 len);

typedef void (*icpu_panic_handler_t)(void);
void pablo_icpu_register_panic_handler(icpu_panic_handler_t hnd);

struct icpu_core *get_icpu_core(void);
void *get_icpu_dev(struct icpu_core *core);
struct platform_driver *pablo_icpu_get_platform_driver(void);

#if IS_ENABLED(CONFIG_PABLO_KUNIT_TEST)
void *pablo_icpu_test_set_platform_data(void *pdata);
const struct dev_pm_ops *pablo_icpu_test_get_pm_ops(void);
struct notifier_block *pablo_icpu_test_get_panic_handler(void);
struct notifier_block *pablo_icpu_test_get_reboot_handler(void);
void *pablo_icpu_test_get_iommu_fault_handler(void);
#endif
#endif
