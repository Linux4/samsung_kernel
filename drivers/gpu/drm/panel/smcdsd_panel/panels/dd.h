/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __DD_H__
#define __DD_H__

#if !defined(CONFIG_DEBUG_FS) && !defined(CONFIG_SAMSUNG_PRODUCT_SHIP) && defined(CONFIG_SMCDSD_LCD_DEBUG)
#error CONFIG_SMCDSD_LCD_DEBUG must be enabled with CONFIG_DEBUG_FS
#endif

#if defined(CONFIG_DEBUG_FS) && !defined(CONFIG_SAMSUNG_PRODUCT_SHIP) && defined(CONFIG_SMCDSD_LCD_DEBUG)
#include <linux/ctype.h>
#endif

#if defined(CONFIG_DEBUG_FS) && !defined(CONFIG_SAMSUNG_PRODUCT_SHIP) && defined(CONFIG_SMCDSD_LCD_DEBUG) && defined(CONFIG_SMCDSD_MDNIE)
#define mdnie_renew_table
#define init_debugfs_mdnie
#else
#define mdnie_renew_table
#define init_debugfs_mdnie
#endif

#if defined(CONFIG_DEBUG_FS) && !defined(CONFIG_SAMSUNG_PRODUCT_SHIP) && defined(CONFIG_SMCDSD_LCD_DEBUG)
struct i2c_client;
struct backlight_device;
extern int init_debugfs_backlight(struct backlight_device *bd, unsigned int *table, struct i2c_client **clients);
extern void init_debugfs_param(const char *name, void *ptr, u32 ptr_type, u32 sum_size, u32 ptr_unit);
#else
#define init_debugfs_backlight(...)
#define init_debugfs_param(...)
#endif

#if defined(CONFIG_DEBUG_FS) && !defined(CONFIG_SAMSUNG_PRODUCT_SHIP) && defined(CONFIG_SMCDSD_LCD_DEBUG)
extern void dsi_write_data_dump(u32 id, unsigned long d0, u32 d1);
extern int run_cmdlist(u32 index);
#else
#define dsi_write_data_dump(...)
#define run_cmdlist(...)
#endif

#if defined(CONFIG_DEBUG_FS) && !defined(CONFIG_SAMSUNG_PRODUCT_SHIP) && defined(CONFIG_SMCDSD_LCD_DEBUG)
static inline int dd_simple_write_to_buffer(char *ibuf, size_t sizeof_ibuf,
					loff_t *ppos, const char __user *user_buf, size_t count)
{
	int ret = 0;

	if (*ppos != 0)
		return -EINVAL;

	if (count == 0)
		return -EINVAL;

	if (count >= sizeof_ibuf)
		return -ENOMEM;

	ret = simple_write_to_buffer(ibuf, sizeof_ibuf, ppos, user_buf, count);
	if (ret < 0)
		return ret;

	ibuf[ret] = '\0';

	ibuf = strim(ibuf);

	if (ibuf[0] && !isalnum(ibuf[0]))
		return -EFAULT;

	return 0;
};
#endif

#endif

