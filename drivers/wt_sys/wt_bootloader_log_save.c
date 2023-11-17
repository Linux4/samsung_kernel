/**
 * @file wt_bootloader_log_save.c
 *
 * Add for wt bootloader log feature
 *
 * Copyright (C) 2020 Wingtech.Co.Ltd. All rights reserved.
 *
 * =============================================================================
 *                             EDIT HISTORY
 *
 * when         who          what, where, why
 * ----------   --------     ---------------------------------------------------
 * 2020/03/10   wanghui      Initial Revision
 *
 * =============================================================================
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/highmem.h>
#include <linux/vmalloc.h>
#include <linux/mm.h>
#include <linux/of.h>
#include <linux/io.h>
#include <linux/uaccess.h>
#include <linux/init.h>
#include <linux/of_address.h>
#include <linux/workqueue.h>
#include <linux/kallsyms.h>
#include <wt_sys/wt_sys_config.h>

#define WT_BOOTLOADER_DELAY          20000
#define WT_BOOTLOADER_LOG_MAGIC      0x474C4C42  /* B L L G */

struct wt_bllog_header {
	uint32_t magic;
	uint32_t initial;
	uint64_t log_addr;
	uint32_t log_size;
	uint32_t log_crc;
	uint8_t  reserved[8];
};

static struct wt_bllog_header *bllog_head = NULL;

#ifndef WT_FINAL_RELEASE
struct wt_bllog_workqueue {
	char *log_addr;
	unsigned int log_size;
	struct delayed_work wt_bllog_handle;
};

static struct wt_bllog_workqueue wt_bllog_wq;

static void wt_bootloader_log_print(struct work_struct *work)
{
	char *buf = NULL;
	char *p = NULL;
	unsigned int i = 0;
	unsigned int len = 0;
	struct wt_bllog_workqueue *bllog_wq = NULL;

	if (work == NULL) {
		printk(KERN_ERR"wt_bootloader_log_print work == NULL!");
		return;
	}
	bllog_wq = container_of(( struct delayed_work *)work, struct wt_bllog_workqueue, wt_bllog_handle);
	buf = bllog_wq->log_addr;
	len = bllog_wq->log_size;
	p = buf;
	if (p == NULL) {
		printk(KERN_ERR"wt_bootloader_log_print p == NULL!");
		return;
	}
	if (len >= WT_BLLOG_ONE_SIZE) {
		printk(KERN_ERR"len > log store size");
		return;
	}
	printk(KERN_ERR"Bootloader log start:%lx, len:%d\n", (long unsigned int)buf, len);
	for (i = 0; i < len; i++) {
		if (buf[i] == '\0')
			buf[i] = ' ';
		if (buf[i] == '\r')
			buf[i] = ' ';
		if (buf[i] == '\n') {
			buf[i] = '\0';
			printk(KERN_ERR"Bootloader log:%s\n",p);
			buf[i] = '\n';
			p = &buf[i+1];
		}
	}
	printk(KERN_ERR"Bootloader log end!\n");
}
#endif

static int wt_bootloader_log_init_memory(void)
{
	void *head_addr = NULL;
	unsigned long wt_log_addr = 0;
	unsigned long wt_log_size = 0;
	struct device_node *wt_bll_mem_dts_node = NULL;
	const u32 *wt_bll_mem_dts_basep = NULL;

	wt_bll_mem_dts_node = of_find_compatible_node(NULL, NULL, "wt_bllog_mem");
	if (wt_bll_mem_dts_node == 0) {
		printk(KERN_ERR"wt_of_find_compatible_node error!\n");
		return -1;
	}
	wt_bll_mem_dts_basep = of_get_address(wt_bll_mem_dts_node, 0, (u64 *)&wt_log_size, NULL);
	wt_log_addr = (unsigned long)of_translate_address(wt_bll_mem_dts_node, wt_bll_mem_dts_basep);
	//printk(KERN_ERR"wt_log_addr:0x%lx wt_log_size:0x%lx\n", wt_log_addr, wt_log_size);

	if (wt_log_addr == 0 || wt_log_size > WT_BOOTLOADER_LOG_SIZE) {
		printk(KERN_ERR"wt_log_addr error!\n");
		return -1;
	}

	#ifdef CONFIG_ARM
		head_addr = (void *)ioremap_nocache(wt_log_addr, wt_log_size);
	#else
		head_addr = (void *)ioremap_wc(wt_log_addr, wt_log_size);
	#endif

	if (head_addr == NULL) {
		printk(KERN_ERR"wt_log_addr ioremap error!\n");
		return -1;
	}
	bllog_head = (struct wt_bllog_header *)head_addr;

	return 0;
}

#ifndef WT_FINAL_RELEASE
static int wt_bootloader_log_handle(void)
{
	unsigned long delay_time = 0;

	if (bllog_head->magic != WT_BOOTLOADER_LOG_MAGIC) {
		printk(KERN_ERR"wt bootloader log magic not match.\n");
		return -1;
	}
	if (bllog_head->log_crc != (uint32_t)((bllog_head->magic | bllog_head->log_addr | bllog_head->log_addr >> 32) ^ bllog_head->log_size)) {
		printk(KERN_ERR"wt bootloader log size error,\n");
		return -1;
	}
	wt_bllog_wq.log_addr = (char *)(unsigned long)bllog_head + sizeof(struct wt_bllog_header);
	wt_bllog_wq.log_size = (unsigned int)(bllog_head->log_size);
	delay_time = msecs_to_jiffies(WT_BOOTLOADER_DELAY);
	INIT_DELAYED_WORK(&(wt_bllog_wq.wt_bllog_handle), wt_bootloader_log_print);
	schedule_delayed_work(&(wt_bllog_wq.wt_bllog_handle), delay_time);
	return 0;
}
#endif

static int __init wt_bootloader_log_init(void)
{
	int ret = 0;

	ret = wt_bootloader_log_init_memory();
	if (ret) {
		printk(KERN_ERR"wt_bootloader_log_init_memory error!\n");
		return -1;
	}

#ifndef WT_FINAL_RELEASE
	ret = wt_bootloader_log_handle();
	if (ret) {
		printk(KERN_ERR"wt_bootloader_log_handle error!\n");
		return -1;
	}
#endif
	return ret;
}

module_init(wt_bootloader_log_init);
MODULE_LICENSE("GPL");

