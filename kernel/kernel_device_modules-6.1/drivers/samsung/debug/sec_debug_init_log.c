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

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/memblock.h>
#include <linux/of.h>
#include <linux/of_reserved_mem.h>
#include <linux/notifier.h>
#include <linux/sec_debug.h>

/* 
	--------------------------------------------  sec-initlog
	-             INIT_LOG_SIZE 1M             -
	--------------------------------------------  sec-autocomment
	-      AC_BUFFER_SIZE+AC_INFO_SIZE 64K     -  
	--------------------------------------------  debug-history
	-           DEBUG_HIST_SIZE 512K           -
	--------------------------------------------  dbore
	-           DEBUG_BORE_SIZE 512K           -
	--------------------------------------------  sec-extrainfo
    -	           REMAINED_SIZE			   -
    --------------------------------------------  end of reserved sec_debug
*/

static u32 init_log_base;
static u32 init_log_size;

static char *buf_ptr = NULL;
static unsigned long buf_size = 0;
static unsigned long buf_idx = 0;

extern void *secdbg_log_rmem_set_vmap(phys_addr_t base, phys_addr_t size);
extern struct reserved_mem *secdbg_log_get_rmem(const char *compatible);
extern void register_hook_init_log(void (*func)(const char *str, size_t size));

void secdbg_hook_init_log(const char *str, size_t size)
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

static int secdbg_initlog_buf_init(void)
{
	buf_ptr = secdbg_log_rmem_set_vmap(init_log_base, init_log_size);
	buf_size = init_log_size;

	if (!buf_ptr || !buf_size) {
		pr_err("%s: error to get buffer size 0x%lx at addr 0x%px\n", __func__, buf_size, buf_ptr);
		return 0;
	}

	memset(buf_ptr, 0, buf_size);
	pr_err("%s: buffer size 0x%lx at addr 0x%px\n", __func__, buf_size, buf_ptr);

	return 1;
}

/*
 * Log module loading order
 * 
 *  0. sec_debug_init() @ core_initcall
 *  1. secdbg_init_log_init() @ postcore_initcall
 *  2. secdbg_lastkernel_log_init() @ arch_initcall
 *  3. secdbg_kernel_log_init() @ arch_initcall
 *  4. secdbg_pmsg_log_init() @ device_initcall
 */
int secdbg_init_log_init(void)
{
	struct reserved_mem *rmem;

	rmem = secdbg_log_get_rmem("samsung,secdbg-init-log");

	init_log_base = rmem->base;
	init_log_size = rmem->size;

	pr_notice("%s: 0x%llx - 0x%llx (0x%llx)\n",
		"sec-logger", (unsigned long long)rmem->base,
		(unsigned long long)rmem->base + (unsigned long long)rmem->size,
		(unsigned long long)rmem->size);

	secdbg_initlog_buf_init();

	register_hook_init_log(secdbg_hook_init_log);

	return 0;
}
EXPORT_SYMBOL(secdbg_init_log_init);

