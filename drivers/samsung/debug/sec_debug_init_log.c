/*
 * sec_debug_init_log.c
 *
 * Copyright (c) 2016 Samsung Electronics Co., Ltd
 *              http://www.samsung.com
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 */

#include <linux/kernel.h>
#include <linux/sec_debug.h>

static u32 __initdata init_log_base = SEC_INIT_LOG_BASE;
static u32 __initdata init_log_size = SZ_2M;

extern void register_init_log_hook_func(void (*func)(const char *buf, size_t size));
static char *buf_ptr;
static unsigned long buf_size;
static unsigned long buf_idx;

static void secdbg_hook_init_log(const char *str, size_t size)
{
	int len;

	if (buf_idx + size > buf_size) {
		len = buf_size - buf_idx;
		memcpy(buf_ptr + buf_idx, str, len);
		memcpy(buf_ptr, str + len, size - len);
		buf_idx = size - len;
	} else {
		memcpy(buf_ptr + buf_idx, str, size);
		buf_idx = (buf_idx + size) % buf_size;
	}
}

static int __init secdbg_hook_init_log_init(void)
{
	pr_err("%s: start\n", __func__);

	buf_ptr = persistent_ram_vmap((phys_addr_t)init_log_base, (phys_addr_t)init_log_size, 0);
	buf_size = init_log_size;
	pr_err("%s: buffer size 0x%lx at addr 0x%px\n", __func__, buf_size, buf_ptr);

	if (!buf_ptr || !buf_size)
		return 0;

	memset(buf_ptr, 0, buf_size);
	register_init_log_hook_func(secdbg_hook_init_log);
	pr_err("%s: done\n", __func__);

	return 0;
}
late_initcall(secdbg_hook_init_log_init);

