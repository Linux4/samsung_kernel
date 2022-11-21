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

#include <linux/uaccess.h>

struct mdnie_info;
struct mdnie_table;
#if defined(CONFIG_DEBUG_FS) && !defined(CONFIG_SAMSUNG_PRODUCT_SHIP) && defined(CONFIG_SMCDSD_LCD_DEBUG) && defined(CONFIG_SMCDSD_MDNIE)
extern void mdnie_renew_table(struct mdnie_info *mdnie, struct mdnie_table *org);
extern int init_debugfs_mdnie(struct mdnie_info *md, unsigned int mdnie_no);
extern void mdnie_update(struct mdnie_info *mdnie);
#else
static inline void mdnie_renew_table(struct mdnie_info *mdnie, struct mdnie_table *org) {};
static inline void init_debugfs_mdnie(struct mdnie_info *md, unsigned int mdnie_no) {};
#endif

struct i2c_client;
struct backlight_device;
#if defined(CONFIG_DEBUG_FS) && !defined(CONFIG_SAMSUNG_PRODUCT_SHIP) && defined(CONFIG_SMCDSD_LCD_DEBUG)
extern int init_debugfs_backlight(struct backlight_device *bd, unsigned int *table, struct i2c_client **clients);
extern void init_debugfs_param(const char *name, void *ptr, u32 ptr_type, u32 sum_size, u32 ptr_unit);
#else
static inline void init_debugfs_backlight(struct backlight_device *bd, unsigned int *table, struct i2c_client **clients) {};
static inline void init_debugfs_param(const char *name, void *ptr, u32 ptr_type, u32 sum_size, u32 ptr_unit) {};
#endif

#if defined(CONFIG_DEBUG_FS) && !defined(CONFIG_SAMSUNG_PRODUCT_SHIP) && defined(CONFIG_SMCDSD_LCD_DEBUG)
extern void dsi_write_data_dump(u32 id, unsigned long d0, u32 d1);
extern int run_cmdlist(u32 index);
#else
static inline void dsi_write_data_dump(u32 id, unsigned long d0, u32 d1) {};
static inline int run_cmdlist(u32 index) { return 0; };
#endif

#if defined(CONFIG_DEBUG_FS) && !defined(CONFIG_SAMSUNG_PRODUCT_SHIP) && defined(CONFIG_SMCDSD_LCD_DEBUG)
static inline int dd_simple_write_to_buffer(char *ibuf, size_t sizeof_ibuf,
					loff_t *ppos, const char __user *user_buf, size_t count)
{
	int ret = 0;

	if (*ppos != 0)
		return -EINVAL;

	if (count >= sizeof_ibuf)
		return -ENOMEM;

	if (copy_from_user(ibuf, user_buf, count))
		return -EFAULT;

	ibuf[count] = '\0';

	strim(ibuf);

	return 0;
};
#endif

#endif

