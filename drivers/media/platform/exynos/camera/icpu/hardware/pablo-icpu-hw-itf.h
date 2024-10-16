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

#ifndef PABLO_ICPU_HW_ITF_H
#define PABLO_ICPU_HW_ITF_H

struct icpu_platform_data;
int pablo_icpu_hw_set_base_address(struct icpu_platform_data *pdata, u32 dst_addr);
void pablo_icpu_hw_misc_prepare(struct icpu_platform_data *pdata);
void pablo_icpu_hw_set_sw_reset(struct icpu_platform_data *pdata);
int pablo_icpu_hw_reset(struct icpu_platform_data *pdata, int on);
int pablo_icpu_hw_wait_teardown_timeout(struct icpu_platform_data *pdata, u32 ms);
void pablo_icpu_hw_force_powerdown(struct icpu_platform_data *pdata);
void pablo_icpu_hw_panic_handler(struct icpu_platform_data *pdata);
u32 pablo_icpu_hw_get_num_tx_mbox(struct icpu_platform_data *pdata);
void *pablo_icpu_hw_get_tx_info(struct icpu_platform_data *pdata, u32 index);
u32 pablo_icpu_hw_get_num_rx_mbox(struct icpu_platform_data *pdata);
void *pablo_icpu_hw_get_rx_info(struct icpu_platform_data *pdata, u32 index);
u32 pablo_icpu_hw_get_num_channels(struct icpu_platform_data *pdata);
void pablo_icpu_hw_set_debug_reg(struct icpu_platform_data *pdata, u32 index, u32 val);
int pablo_icpu_hw_probe(void *pdev);
void pablo_icpu_hw_remove(struct icpu_platform_data *pdata);

#endif
