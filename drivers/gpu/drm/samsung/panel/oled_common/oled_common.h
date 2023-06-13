/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) Samsung Electronics Co., Ltd.
 * Gwanghui Lee <gwanghui.lee@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __OLED_COMMON_H__
#define __OLED_COMMON_H__

#include <linux/types.h>
#include <linux/kernel.h>

int init_common_table(struct maptbl *tbl);
int getidx_common_maptbl(struct maptbl *tbl);
void copy_dummy_maptbl(struct maptbl *tbl, u8 *dst);
void copy_common_maptbl(struct maptbl *tbl, u8 *dst);
#ifdef CONFIG_EXYNOS_DECON_LCD_COPR
void copy_copr_maptbl(struct maptbl *tbl, u8 *dst);
#endif

#endif /* __OLED_COMMON_H__ */
