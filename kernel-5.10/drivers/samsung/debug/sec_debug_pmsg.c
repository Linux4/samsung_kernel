/*
 * Copyright (c) 2022 Samsung Electronics Co., Ltd.
 *      http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/vmalloc.h>
#include <linux/of.h>
#include <linux/of_reserved_mem.h>
#include <linux/sched/clock.h>

#define LOGGER_LEVEL_HEADER	(1)
#define LOGGER_LEVEL_PREFIX	(2)
#define LOGGER_LEVEL_TEXT	(3)
#define LOGGER_HEADER_SIZE	(68)

#define LOG_ID_MAIN		(0)
#define LOG_ID_RADIO		(1)
#define LOG_ID_EVENTS		(2)
#define LOG_ID_SYSTEM		(3)
#define LOG_ID_CRASH		(4)
#define LOG_ID_KERNEL		(5)

struct pmsg_log_header {
	uint8_t magic;
	uint16_t len;
	uint16_t uid;
	uint16_t pid;
}__attribute__((__packed__));

struct android_log_header {
	unsigned char id;
	uint16_t tid;
	int32_t tv_sec;
	int32_t tv_nsec;
}__attribute__((__packed__));

struct pmsg_logger {
	uint16_t	len;
	uint16_t	id;
	uint16_t	pid;
	uint16_t	tid;
	uint16_t	uid;
	uint16_t	level;
	int32_t		tv_sec;
	int32_t		tv_nsec;
	char		prio;
	char		*buffer;
};

static DEFINE_MUTEX(pmsg_lock);
static struct pmsg_logger logger;
static const char *kPrioChars = "udVDIWEFs";

static u32 secdbg_pmsg_base;
static u32 secdbg_pmsg_size;

static char *pmsg_buf_ptr = NULL;
static unsigned long pmsg_buf_size = 0;
static unsigned long pmsg_buf_idx = 0;

extern void *secdbg_log_rmem_set_vmap(phys_addr_t base, phys_addr_t size);
extern struct reserved_mem *secdbg_log_get_rmem(const char *compatible);

#if IS_ENABLED(CONFIG_SEC_BOOTSTAT)
static void (*hook_bootstat)(const char *buf);
void register_hook_bootstat(void (*func)(const char *buf))
{
	if (!func) {
		pr_err("No hooking function for bootstat\n");
		return;
	}

	hook_bootstat = func;
	pr_info("%s done!\n", __func__);
}
EXPORT_SYMBOL(register_hook_bootstat);
#endif

static void secdbg_hook_logger(const char *str, size_t size)
{
	int len;

	if (pmsg_buf_idx + size > pmsg_buf_size) {
		len = pmsg_buf_size - pmsg_buf_idx;
		memcpy(pmsg_buf_ptr + pmsg_buf_idx, str, len);
		memcpy(pmsg_buf_ptr, str + len, size - len);
		pmsg_buf_idx = size - len;
	} else {
		memcpy(pmsg_buf_ptr + pmsg_buf_idx, str, size);
		pmsg_buf_idx = (pmsg_buf_idx + size) % pmsg_buf_size;
	}

	if (IS_ENABLED(CONFIG_SEC_BOOTSTAT) && size > 2 &&
			strncmp(str, "!@", 2) == 0) {
		char _buf[SZ_128];
		size_t count = size < SZ_128 ? size : SZ_128 - 1;
		memcpy(_buf, str, count);
		_buf[count] = '\0';

		printk("%s\n", _buf);

		if ((hook_bootstat) && size > 6 &&
				strncmp(_buf, "!@Boot", 6) == 0)
			hook_bootstat(_buf);
	}
}

static void secdbg_combine_pmsg(char *buffer, size_t count, unsigned int level)
{
	char *logbuf = logger.buffer;
	struct tm tmBuf;
	u64 tv_kernel = local_clock();
	unsigned long rem_nsec = do_div(tv_kernel, 1000000000);
	unsigned int logbuf_len;
	unsigned char prio;

	if (logger.id == LOG_ID_EVENTS)
		return;

	switch (level) {
	case LOGGER_LEVEL_HEADER:
		tv_kernel = local_clock();
		rem_nsec = do_div(tv_kernel, 1000000000);
		time64_to_tm(logger.tv_sec, 0, &tmBuf);
		logbuf_len = snprintf(logbuf, LOGGER_HEADER_SIZE,
				"\n[%5llu.%06lu][%d:%16s] %02d-%02d %02d:%02d:%02d.%03d %5d %5d  ",
				tv_kernel, rem_nsec / 1000, raw_smp_processor_id(), current->comm,
				tmBuf.tm_mon + 1, tmBuf.tm_mday,
				tmBuf.tm_hour, tmBuf.tm_min, tmBuf.tm_sec,
				logger.tv_nsec / 1000000, logger.pid, logger.tid);

		secdbg_hook_logger(logbuf, logbuf_len - 1);
		break;
	case LOGGER_LEVEL_PREFIX:
		prio = buffer[0];

		logbuf[0] = prio < strlen(kPrioChars) ? kPrioChars[prio] : '?';
		logbuf[1] = ' ';

		secdbg_hook_logger(logbuf, LOGGER_LEVEL_PREFIX);
		break;
	case LOGGER_LEVEL_TEXT:
		buffer[count - 1] = ' ';
		secdbg_hook_logger(buffer, count);
		break;
	default:
		break;
	}
	return;
}

static int secdbg_hook_pmsg(const char __user *buf, size_t count)
{
	char *buffer = logger.buffer;
	struct android_log_header *header;
	struct pmsg_log_header *pmsg_header;

	if (__copy_from_user(buffer, buf, count))
		return -EFAULT;

	switch (count) {
	case sizeof(struct android_log_header):
		header = (struct android_log_header *)buffer;
		logger.id = header->id;
		logger.tid = header->tid;
		logger.tv_sec = header->tv_sec;
		logger.tv_nsec  = header->tv_nsec;
		secdbg_combine_pmsg(buffer, count,
				logger.id > 7 ? LOGGER_LEVEL_TEXT : LOGGER_LEVEL_HEADER);
		break;
	case sizeof(unsigned char):
		secdbg_combine_pmsg(buffer, count, LOGGER_LEVEL_PREFIX);
		break;
	case sizeof(struct pmsg_log_header):
		pmsg_header = (struct pmsg_log_header *)buffer;
		if (pmsg_header->magic == 'l') {
			logger.pid = pmsg_header->pid;
			logger.uid = pmsg_header->uid;
			logger.len = pmsg_header->len;
			break;
		}
	default:
		secdbg_combine_pmsg(buffer, count, LOGGER_LEVEL_TEXT);
		break;
	}

	return 0;
}

static ssize_t secdbg_write_pmsg(struct file *file, const char __user *buf,
			  	       size_t count, loff_t *ppos)
{
	int ret;

	if (!count)
		return 0;

	/* check outside lock, page in any data. write_user also checks */
#if (LINUX_VERSION_CODE < KERNEL_VERSION(5, 4, 0))
	if (!access_ok(VERIFY_READ, buf, count))
#else	
	if (!access_ok(buf, count))
#endif		
		return -EFAULT;

	mutex_lock(&pmsg_lock);
	ret = secdbg_hook_pmsg(buf, count);
	mutex_unlock(&pmsg_lock);
	return ret ? ret : count;
}

static const struct file_operations secdbg_pmsg_fops = {
	.owner		= THIS_MODULE,
	.llseek		= noop_llseek,
	.write		= secdbg_write_pmsg,
};

static struct class *secdbg_pmsg_class;
static int secdbg_pmsg_major;
#define PMSG_NAME "pmsg"

static char *secdbg_pmsg_devnode(struct device *dev, umode_t *mode)
{
	if (mode)
		*mode = 0220;
	return NULL;
}

void secdbg_init_pmsg(void)
{
	struct device *pmsg_device;

	logger.buffer = vmalloc(PAGE_SIZE);
	if (!logger.buffer)
		goto err;

	secdbg_pmsg_major = register_chrdev(0, PMSG_NAME, &secdbg_pmsg_fops);
	if (secdbg_pmsg_major < 0) {
		pr_err("register_chrdev failed\n");
		goto err;
	}

	secdbg_pmsg_class = class_create(THIS_MODULE, PMSG_NAME);
	if (IS_ERR(secdbg_pmsg_class)) {
		pr_err("device class file already in use\n");
		 goto err_class;
	}
	secdbg_pmsg_class->devnode = secdbg_pmsg_devnode;
	pmsg_device = device_create(secdbg_pmsg_class, NULL, MKDEV(secdbg_pmsg_major, 0),
					NULL, "%s%d", PMSG_NAME, 0);
	if (IS_ERR(pmsg_device)) {
		pr_err("failed to create device\n");
		goto err_device;
	}

	pr_info("secdbg_init_pmsg: Register pmsg0 successful\n");
	return;

err_device:
	class_destroy(secdbg_pmsg_class);
err_class:
	unregister_chrdev(secdbg_pmsg_major, PMSG_NAME);
err:
	pr_info("secdbg_init_pmsg: Register pmsg0 failed\n");
	return;
}

static int secdbg_pmsg_buf_init(void)
{
	pmsg_buf_ptr = secdbg_log_rmem_set_vmap(secdbg_pmsg_base, secdbg_pmsg_size);
	pmsg_buf_size = secdbg_pmsg_size;

	if (!pmsg_buf_ptr || !pmsg_buf_size) {
		pr_err("%s: error to get buffer size 0x%lx at addr 0x%px\n", __func__, pmsg_buf_size, pmsg_buf_ptr);
		return 0;
	}

	memset(pmsg_buf_ptr, 0, pmsg_buf_size);
	pr_err("%s: buffer size 0x%lx at addr 0x%px\n", __func__, pmsg_buf_size, pmsg_buf_ptr);

	return 0;
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
int secdbg_pmsg_log_init(void)
{
	struct reserved_mem *rmem;

	rmem = secdbg_log_get_rmem("samsung,secdbg-pmsg-log");

	secdbg_pmsg_base = rmem->base;
	secdbg_pmsg_size = rmem->size;

	pr_notice("%s: 0x%llx - 0x%llx (0x%llx)\n",
		"sec-logger", (unsigned long long)rmem->base,
		(unsigned long long)rmem->base + (unsigned long long)rmem->size,
		(unsigned long long)rmem->size);

	secdbg_pmsg_buf_init();
	
	secdbg_init_pmsg();
	
	return 0;
}
EXPORT_SYMBOL(secdbg_pmsg_log_init);
