/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2023 MediaTek Inc.
 */

#ifndef _TOOLS_KSYM_H_
#define _TOOLS_KSYM_H_

#include <linux/types.h> // for phys_addr_t
#include <linux/random.h>

#include <linux/err.h>

#include <linux/module.h>
#include <linux/printk.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>

#define KV		(kimage_vaddr+64*1024)
#define S_MAX		SZ_128M
#define SM_SIZE		28
#define TT_SIZE		256
#define NAME_LEN	128

int __init tools_ka_init(void);
unsigned long __init tools_addr_find(const char *name);
#endif /* _TOOLS_KSYM_H */
