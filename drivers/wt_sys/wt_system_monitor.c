/**
 * @file wt_system_monitor.c
 *
 * Add for wt system monitor feature
 *
 * Copyright (C) 2020 Wingtech.Co.Ltd. All rights reserved.
 *
 * =============================================================================
 *                             EDIT HISTORY
 *
 * when         who          what, where, why
 * ----------   --------     ---------------------------------------------------
 * 2020/03/13   wanghui      Initial Revision
 *
 * =============================================================================
 *
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/string.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/kallsyms.h>
#include <linux/uaccess.h>
#include <linux/init.h>
#include "wt_sys/wt_sys_config.h"

#define WT_SYSTEM_MONITOR_MAGIC     0x4D535953  /* S Y S M */

struct wt_sysm_header {
	uint32_t magic;
	uint32_t initial;
	uint64_t log_addr;
	uint32_t log_size;
	uint32_t log_checksum;
	uint8_t  reserved[8];
};

static struct wt_sysm_header *wt_sysm_head = NULL;

static uint32_t wt_cal_checksum(void *buf, int len)
{
	uint32_t *_buf = buf;
	int _len = len / sizeof(uint32_t);
	uint32_t checksum = 0x55aa55aa;
	int i;

	for (i = 0; i < _len; i++) {
		checksum += *_buf++;
	}
	return checksum;
}

static int wt_system_monitor_init_memory(void)
{
	void *head_addr = NULL;
	unsigned long wt_sysm_addr = 0;
	unsigned long wt_sysm_size = 0;
	struct device_node *wt_sysm_mem_dts_node = NULL;
	const uint32_t *wt_sysm_mem_dts_basep = NULL;

	wt_sysm_mem_dts_node = of_find_compatible_node(NULL, NULL, "wt_sysm_mem");
	if (wt_sysm_mem_dts_node == 0) {
		printk(KERN_ERR"of_find_compatible_node error!\n");
		return -1;
	}
	wt_sysm_mem_dts_basep = of_get_address(wt_sysm_mem_dts_node, 0, (u64 *)&wt_sysm_size, NULL);
	wt_sysm_addr = (unsigned long)of_translate_address(wt_sysm_mem_dts_node, wt_sysm_mem_dts_basep);
	//printk(KERN_ERR"wt_sysm_addr:0x%lx wt_sysm_size:0x%lx\n", wt_sysm_addr, wt_sysm_size);

	if (wt_sysm_addr == 0 || wt_sysm_size > WT_SYSTEM_MONITOR_SIZE) {
		printk(KERN_ERR"wt_sysm_addr error!\n");
		return -1;
	}

	#ifdef CONFIG_ARM
		head_addr = (void *)ioremap_nocache(wt_sysm_addr, wt_sysm_size);
	#else
		head_addr = (void *)ioremap_wc(wt_sysm_addr, wt_sysm_size);
	#endif

	if (head_addr == NULL) {
		printk(KERN_ERR"wt_sysm_addr ioremap error!\n");
		return -1;
	}
	wt_sysm_head = (struct wt_sysm_header *)head_addr;

	return 0;
}

static int __init wt_system_monitor_init(void)
{
	int ret = 0;

	ret = wt_system_monitor_init_memory();
	if (ret) {
		printk(KERN_ERR"wt_system_monitor_init_memory error!\n");
		return -1;
	}

	wt_sysm_head->magic = WT_SYSTEM_MONITOR_MAGIC;
#ifdef CONFIG_KALLSYMS
	wt_sysm_head->log_addr = virt_to_phys(*(void **)kallsyms_lookup_name("log_buf"));
#else
	wt_sysm_head->log_addr = 0;
#endif
#ifdef CONFIG_LOG_BUF_SHIFT
	wt_sysm_head->log_size = *(unsigned long *)kallsyms_lookup_name("log_buf_len");
#else
	wt_sysm_head->log_size = 0;
#endif
	wt_sysm_head->log_checksum = wt_cal_checksum(&wt_sysm_head->log_addr, sizeof(uint64_t) + sizeof(uint32_t));
	memset(wt_sysm_head->reserved, 0, sizeof(wt_sysm_head->reserved));

	return ret;
}

module_init(wt_system_monitor_init);
MODULE_LICENSE("GPL");
