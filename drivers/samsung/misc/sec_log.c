/*
 * Copyright (c) 2016 Samsung Electronics Co., Ltd.
 *      http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/module.h>
#include <linux/console.h>
#include <linux/kernel.h>
#include <linux/bootmem.h>
#include <linux/slab.h>
#include <linux/stat.h>
#include <linux/module.h>
#include <linux/uaccess.h>
#include <linux/proc_fs.h>
#include <linux/memblock.h>
#include <linux/sched/clock.h>
#include <linux/sec_debug.h>
#include <linux/of.h>
#include <linux/of_reserved_mem.h>
#include <linux/vmalloc.h>
#include <linux/sec_ext.h>

#ifdef CONFIG_SEC_PERF_BUILD_KERNEL
static u32 __initdata seclogger_base = SEC_LOGGER_BASE;
static u32 __initdata seclogger_size = SZ_512K;
static u32 __initdata seclog_base = SEC_LOG_BASE;
static u32 __initdata seclog_size = SZ_512K;
static u32 __initdata seclastklog_base = SEC_LASTKMSG_BASE;
static u32 __initdata seclastklog_size = SZ_512K;
#else
static u32 __initdata seclogger_base = SEC_LOGGER_BASE;
static u32 __initdata seclogger_size = SZ_4M;
static u32 __initdata seclog_base = SEC_LOG_BASE;
static u32 __initdata seclog_size = SZ_2M;
static u32 __initdata seclastklog_base = SEC_LASTKMSG_BASE;
static u32 __initdata seclastklog_size = SZ_2M;
#endif

static char *sec_log_buf;
static unsigned int sec_log_size;
static unsigned int *sec_log_pos;
static unsigned int *sec_log_head;
static char *sec_logger_buf;
static unsigned int sec_logger_size;
static unsigned int *sec_logger_pos;
static unsigned int platform_buffer_size;
static sec_logger logger;

#ifdef CONFIG_SEC_DEBUG_LAST_KMSG
static char *last_kmsg_buffer;
static unsigned int last_kmsg_size;

static void __init sec_log_save_old(unsigned int lastkbase, unsigned int lastksize)
{
	/* provide previous log as last_kmsg */
	unsigned int pos = *sec_log_pos;
	last_kmsg_buffer = phys_to_virt(lastkbase);

	if (last_kmsg_buffer) {
		if (*sec_log_head == 1) {
			last_kmsg_size = sec_log_size;
			memcpy(last_kmsg_buffer, &sec_log_buf[pos], sec_log_size - pos);
			memcpy(&last_kmsg_buffer[sec_log_size - pos], sec_log_buf, pos);
		} else {
			last_kmsg_size = pos;
			memcpy(last_kmsg_buffer, sec_log_buf, lastksize);
		}
		pr_info("%s: saved old log at %u@%p\n",
			__func__, last_kmsg_size, last_kmsg_buffer);
	} else
		pr_err("%s: failed saving old log %u@%p\n",
		       __func__, last_kmsg_size, last_kmsg_buffer);
}

static ssize_t sec_last_kmsg_read(struct file *file, char __user *buf,
				  size_t len, loff_t *offset)
{
	loff_t pos = *offset;
	ssize_t count;

	if (pos >= last_kmsg_size)
		return 0;

	count = min(len, (size_t) (last_kmsg_size - pos));
	if (copy_to_user(buf, last_kmsg_buffer + pos, count))
		return -EFAULT;

	*offset += count;
	return count;
}

static const struct file_operations last_kmsg_file_ops = {
	.owner = THIS_MODULE,
	.read = sec_last_kmsg_read,
};

static int __init sec_last_kmsg_late_init(void)
{
	struct proc_dir_entry *entry;

	if (last_kmsg_buffer == NULL)
		return 0;

	entry = proc_create("last_kmsg", S_IFREG | S_IRUGO,
			NULL, &last_kmsg_file_ops);
	if (!entry) {
		pr_err("%s: failed to create proc entry\n", __func__);
		return 0;
	}

	proc_set_size(entry, last_kmsg_size);
	return 0;
}
late_initcall(sec_last_kmsg_late_init);
#else
static inline void sec_log_save_old(unsigned int lastkbase, unsigned int lastksize)
{
}
#endif /* CONFIG_SEC_DEBUG_LAST_KMSG */

static inline void sec_log_hook_logger(const char *text, size_t size)
{
	unsigned int pos = 0;

	/* Remove flip possibility the upper bit of the 22nd bit*/
	*sec_logger_pos &= (platform_buffer_size - 1);
	pos = *sec_logger_pos;

	if (likely((unsigned int)size + pos <= sec_logger_size))
		memcpy(&sec_logger_buf[pos], text, (unsigned int)size);
	else {
		unsigned int first = sec_logger_size - pos;
		unsigned int second = (unsigned int)size - first;
		memcpy(&sec_logger_buf[pos], text, first);
		memcpy(&sec_logger_buf[0], text + first, second);
	}
	(*sec_logger_pos) += (unsigned int)size;

	/* Check overflow */
	if (unlikely(*sec_logger_pos >= sec_logger_size))
		*sec_logger_pos -= sec_logger_size;
}

static inline void emit_sec_log(char *text, size_t size)
{
	unsigned int pos = *sec_log_pos;

	if (likely((unsigned int)size + pos <= sec_log_size))
		memcpy(&sec_log_buf[pos], text, (unsigned int)size);
	else {
		unsigned int first = sec_log_size - pos;
		unsigned int second = (unsigned int)size - first;
		memcpy(&sec_log_buf[pos], text, first);
		memcpy(&sec_log_buf[0], text + first, second);
		*sec_log_head = 1;
	}
	(*sec_log_pos) += (unsigned int)size;

	/* Check overflow */
	if (unlikely(*sec_log_pos >= sec_log_size))
		*sec_log_pos -= sec_log_size;
}

void register_hook_logger(void (*func)(const char *buf, size_t size))
{
	logger.func_hook_logger = func;
	logger.buffer = vmalloc(PAGE_SIZE * 3);

	if (logger.buffer)
		pr_info("sec_log: logger buffer alloc address: 0x%p\n", logger.buffer);
}
EXPORT_SYMBOL(register_hook_logger);

static int __init sec_log_buf_init(void)
{
	unsigned int *sec_log_mag;

	sec_log_buf = persistent_ram_vmap((phys_addr_t)seclog_base, (phys_addr_t)seclog_size, 0);
	sec_log_size = seclog_size - (sizeof(*sec_log_head) + sizeof(*sec_log_pos) + sizeof(*sec_log_mag));
	sec_log_head = (unsigned int *)(sec_log_buf + sec_log_size);
	sec_log_pos = (unsigned int *)(sec_log_buf + sec_log_size + sizeof(*sec_log_head));
	sec_log_mag = (unsigned int *)(sec_log_buf + sec_log_size + sizeof(*sec_log_head) + sizeof(*sec_log_pos));

	pr_info("%s: *sec_log_head:%u, *sec_log_mag:%x, *sec_log_pos:%u, sec_log_buf:0x%p, sec_log_size:0x%x\n",
		__func__, *sec_log_head, *sec_log_mag, *sec_log_pos, sec_log_buf, sec_log_size);

	if (*sec_log_mag != LOG_MAGIC) {
		pr_info("%s: no old log found\n", __func__);
		*sec_log_head = 0;
		*sec_log_pos = 0;
		*sec_log_mag = LOG_MAGIC;
	} else {
		sec_log_save_old(seclastklog_base, seclastklog_size);
	}

	//register_console(&sec_console);
	//unregister_console(&sec_console);
	register_log_text_hook(emit_sec_log);

	return 0;
}
arch_initcall(sec_log_buf_init);

#ifdef CONFIG_SEC_LOG_HOOK_PMSG
static int sec_log_combine_pmsg(char *buffer, size_t count, unsigned int level)
{
	char *logbuf = logger.buffer;
	if (!logbuf)
		return -ENOMEM;

	switch(level) {
	case SEC_LOGGER_LEVEL_HEADER:
		{
			struct tm tmBuf;
			u64 tv_kernel;
			unsigned int logbuf_len;
			unsigned long rem_nsec;

			if (logger.id == SEC_LOG_ID_EVENTS)
				break;

			tv_kernel = local_clock();
			rem_nsec = do_div(tv_kernel, 1000000000);
			time_to_tm(logger.tv_sec, 0, &tmBuf);

			logbuf_len = snprintf(logbuf, SEC_LOGGER_HEADER_SIZE,
					"\n[%5lu.%06lu][%d:%16s] %02d-%02d %02d:%02d:%02d.%03d %5d %5d  ",
					(unsigned long)tv_kernel, rem_nsec / 1000,
					raw_smp_processor_id(), current->comm,
					tmBuf.tm_mon + 1, tmBuf.tm_mday,
					tmBuf.tm_hour, tmBuf.tm_min, tmBuf.tm_sec,
					logger.tv_nsec / 1000000, logger.pid, logger.tid);

			logger.func_hook_logger(logbuf, logbuf_len - 1);
		}
		break;
	case SEC_LOGGER_LEVEL_PREFIX:
		{
			static const char* kPrioChars = "!.VDIWEFS";
			unsigned char prio = logger.msg[0];

			if (logger.id == SEC_LOG_ID_EVENTS)
				break;

			logbuf[0] = prio < strlen(kPrioChars) ? kPrioChars[prio] : '?';
			logbuf[1] = ' ';

			logger.func_hook_logger(logbuf, SEC_LOGGER_LEVEL_PREFIX);
		}
		break;
	case SEC_LOGGER_LEVEL_TEXT:
		{
			char *eatnl = buffer + count - SEC_LOGGER_STRING_PAD;

			if (logger.id == SEC_LOG_ID_EVENTS)
				break;
			else {
				if (count == SEC_LOGGER_SKIP_COUNT && *eatnl != '\0')
					break;

				logger.func_hook_logger(buffer, count - 1);
				if (count > 1 && strncmp(buffer, "!@", 2) == 0) {
					/* To prevent potential buffer overrun
					 * put a null at the end of the buffer if required
					 */
					buffer[count - 1] = '\0';					
					pr_info("%s\n", buffer);
#ifdef CONFIG_SEC_BOOTSTAT
					if (count > 5 && strncmp(buffer, "!@Boot", 6) == 0)
					sec_boot_stat_add(buffer);
#endif /* CONFIG_SEC_BOOTSTAT */
				}
			}
		}
		break;
	default:
		break;
	}
	return 0;
}

int sec_log_hook_pmsg(char *buffer, size_t count)
{
	sec_android_log_header_t header;
	sec_pmsg_log_header_t pmsg_header;

	if (!logger.buffer)
		return -ENOMEM;

	switch(count) {
	case sizeof(pmsg_header):
		memcpy((void *)&pmsg_header, buffer, count);
		if (pmsg_header.magic != 'l') {
			sec_log_combine_pmsg(buffer, count, SEC_LOGGER_LEVEL_TEXT);
		} else {
			/* save logger data */
			logger.pid = pmsg_header.pid;
			logger.uid = pmsg_header.uid;
			logger.len = pmsg_header.len;
		}
		break;
	case sizeof(header):
		/* save logger data */
		memcpy((void *)&header, buffer, count);
		logger.id = header.id;
		logger.tid = header.tid;
		logger.tv_sec = header.tv_sec;
		logger.tv_nsec  = header.tv_nsec;
		if (logger.id > 7) {
			/* write string */
			sec_log_combine_pmsg(buffer, count, SEC_LOGGER_LEVEL_TEXT);
		} else {
			/* write header */
			sec_log_combine_pmsg(buffer, count, SEC_LOGGER_LEVEL_HEADER);
		}
		break;
	case sizeof(unsigned char):
		logger.msg[0] = buffer[0];
		/* write char for prefix */
		sec_log_combine_pmsg(buffer, count, SEC_LOGGER_LEVEL_PREFIX);
		break;
	default:
		/* write string */
		sec_log_combine_pmsg(buffer, count, SEC_LOGGER_LEVEL_TEXT);
		break;
	}

	return 0;
}
EXPORT_SYMBOL(sec_log_hook_pmsg);
#endif

static int __init sec_logger_init(void)
{
	unsigned int *sec_logger_mag;
	unsigned int size_512K = 512*1024;

	platform_buffer_size = seclogger_size;

	if (SEC_DEBUG_LEVEL(kernel) == 0) {
		memblock_free_late(seclogger_base+size_512K, seclogger_size-size_512K);
		pr_info("freed sec-logger memory : %uMB at %#.8x\n",
			(unsigned)(seclogger_size-size_512K), (unsigned)seclogger_base);
		platform_buffer_size = size_512K;
//		return 0;
	}

	sec_logger_buf = persistent_ram_vmap((phys_addr_t)seclogger_base, (phys_addr_t)platform_buffer_size, 0);
	sec_logger_size = platform_buffer_size - (sizeof(*sec_logger_pos) + sizeof(*sec_logger_mag));
	sec_logger_pos = (unsigned int *)(sec_logger_buf + sec_logger_size);
	sec_logger_mag = (unsigned int *)(sec_logger_buf + sec_logger_size + sizeof(*sec_logger_pos));

	pr_info("%s: platform_buffer_size:%x\n", __func__, platform_buffer_size);
	pr_info("%s: *sec_logger_mag:%x, *sec_logger_pos:%u, sec_logger_buf:0x%p, sec_logger_size:0x%x\n",
		__func__, *sec_logger_mag, *sec_logger_pos, sec_logger_buf, sec_logger_size);

	if (*sec_logger_mag != LOG_MAGIC) {
		pr_info("%s: no old logger found\n", __func__);
		*sec_logger_pos = 0;
		*sec_logger_mag = LOG_MAGIC;
	}

	register_hook_logger(sec_log_hook_logger);

	return 0;
}
device_initcall(sec_logger_init);

#ifdef CONFIG_SEC_AVC_LOG
static unsigned *sec_avc_log_ptr;
static char *sec_avc_log_buf;
static unsigned sec_avc_log_size;

static int __init sec_avc_log_setup(char *str)
{
	unsigned size = memparse(str, &str);
	unsigned long base = 0;
	unsigned *sec_avc_log_mag;

	if (SEC_DEBUG_LEVEL(kernel) == 0)
		return 0;

	/* If we encounter any problem parsing str ... */
	if (!size || size != roundup_pow_of_two(size) || *str != '@'
		|| kstrtoul(str + 1, 0, &base))
			goto out;

	if (memblock_is_region_reserved(base, size) || memblock_reserve(base, size)) {
			pr_err("%s: failed reserving size %d " \
				"at base 0x%lx\n", __func__, size, base);
			goto out;
	}

	sec_avc_log_buf = persistent_ram_vmap(base, size, 0);
	sec_avc_log_size = size - (sizeof(*sec_avc_log_ptr) + sizeof(*sec_avc_log_mag));
	sec_avc_log_ptr = (unsigned int *)(sec_avc_log_buf + sec_avc_log_size);
	sec_avc_log_mag = (unsigned int *)(sec_avc_log_buf + sec_avc_log_size + sizeof(*sec_avc_log_ptr));

	pr_info("%s: *sec_avc_log_ptr:%x " \
		"sec_avc_log_buf:%p sec_log_size:0x%x\n",
		__func__, *sec_avc_log_ptr, sec_avc_log_buf, sec_avc_log_size);

	if (*sec_avc_log_mag != LOG_MAGIC) {
		pr_info("%s: no old log found\n", __func__);
		*sec_avc_log_ptr = 0;
		*sec_avc_log_mag = LOG_MAGIC;
	}
	return 1;
out:
	return 0;
}
__setup("sec_avc_log=", sec_avc_log_setup);

#define BUF_SIZE 512
void sec_debug_avc_log(char *fmt, ...)
{
	va_list args;
	char buf[BUF_SIZE];
	int len = 0;
	unsigned long idx;
	unsigned long size;

	/* In case of sec_avc_log_setup is failed */
	if (!sec_avc_log_size)
		return;

	va_start(args, fmt);
	vsnprintf(buf, sizeof(buf), fmt, args);
	va_end(args);

	idx = *sec_avc_log_ptr;
	size = strlen(buf);

	if (idx + size > sec_avc_log_size - 1) {
		len = scnprintf(&sec_avc_log_buf[0], size + 1, "%s\n", buf);
		*sec_avc_log_ptr = len;
	} else {
		len = scnprintf(&sec_avc_log_buf[idx], size + 1, "%s\n", buf);
		*sec_avc_log_ptr += len;
	}
}
EXPORT_SYMBOL(sec_debug_avc_log);

static ssize_t sec_avc_log_write(struct file *file,
		const char __user *buf,
		size_t count, loff_t *ppos)

{
	char *page = NULL;
	ssize_t ret;
	int new_value;

	if (!sec_avc_log_buf)
		return 0;

	ret = -ENOMEM;
	if (count >= PAGE_SIZE)
		return ret;

	ret = -ENOMEM;
	page = (char *)get_zeroed_page(GFP_KERNEL);
	if (!page)
		return ret;;

	ret = -EFAULT;
	if (copy_from_user(page, buf, count))
		goto out;

	ret = -EINVAL;
	if (sscanf(page, "%u", &new_value) != 1) {
		pr_info("%s\n", page);
		/* print avc_log to sec_avc_log_buf */
		sec_debug_avc_log("%s", page);
	}
	ret = count;
out:
	free_page((unsigned long)page);
	return ret;
}

static ssize_t sec_avc_log_read(struct file *file, char __user *buf,
		size_t len, loff_t *offset)
{
	loff_t pos = *offset;
	ssize_t count;

	if (sec_avc_log_buf == NULL)
		return 0;

	if (pos >= *sec_avc_log_ptr)
		return 0;

	count = min(len, (size_t)(*sec_avc_log_ptr - pos));
	if (copy_to_user(buf, sec_avc_log_buf + pos, count))
		return -EFAULT;
	*offset += count;
	return count;
}

static const struct file_operations avc_msg_file_ops = {
	.owner = THIS_MODULE,
	.read = sec_avc_log_read,
	.write = sec_avc_log_write,
	.llseek = generic_file_llseek,
};

static int __init sec_avc_log_late_init(void)
{
	struct proc_dir_entry *entry;
	if (sec_avc_log_buf == NULL)
		return 0;

	entry = proc_create("avc_msg", S_IFREG | S_IRUGO, NULL,
			&avc_msg_file_ops);
	if (!entry) {
		pr_err("%s: failed to create proc entry\n", __func__);
		return 0;
	}

	proc_set_size(entry, sec_avc_log_size);
	return 0;
}
late_initcall(sec_avc_log_late_init);
#endif /* CONFIG_SEC_AVC_LOG */

#ifdef CONFIG_SEC_DEBUG_TSP_LOG
static unsigned *sec_tsp_log_ptr;
static char *sec_tsp_log_buf;
static unsigned sec_tsp_log_size;

static int __init sec_tsp_log_setup(char *str)
{
	unsigned size = memparse(str, &str);
	unsigned long base = 0;
	unsigned *sec_tsp_log_mag;

	if (SEC_DEBUG_LEVEL(kernel) == 0)
		return 0;

	/* If we encounter any problem parsing str ... */
	if (!size || size != roundup_pow_of_two(size) || *str != '@'
		|| kstrtoul(str + 1, 0, &base))
			goto out;

	if (memblock_is_region_reserved(base, size) || memblock_reserve(base, size)) {
			pr_err("%s: failed reserving size %d " \
				"at base 0x%lx\n", __func__, size, base);
			goto out;
	}

	sec_tsp_log_buf = persistent_ram_vmap(base, size, 0);
	sec_tsp_log_size = size - (sizeof(*sec_tsp_log_ptr) + sizeof(*sec_tsp_log_mag));
	sec_tsp_log_ptr = (unsigned int *)(sec_tsp_log_buf + sec_tsp_log_size);
	sec_tsp_log_mag = (unsigned int *)(sec_tsp_log_buf + sec_tsp_log_size + sizeof(*sec_tsp_log_ptr));

	pr_info("%s: *sec_tsp_log_ptr:%x " \
		"sec_tsp_log_buf:%p sec_tsp_log_size:0x%x\n",
		__func__, *sec_tsp_log_ptr, sec_tsp_log_buf, sec_tsp_log_size);

	if (*sec_tsp_log_mag != LOG_MAGIC) {
		pr_info("%s: no old log found\n", __func__);
		*sec_tsp_log_ptr = 0;
		*sec_tsp_log_mag = LOG_MAGIC;
	}
	return 1;
out:
	return 0;
}
__setup("sec_tsp_log=", sec_tsp_log_setup);

static int sec_tsp_log_timestamp(unsigned int idx)
{
	/* Add the current time stamp */
	char tbuf[50];
	unsigned tlen;
	unsigned long long t;
	unsigned long nanosec_rem;

	t = local_clock();
	nanosec_rem = do_div(t, 1000000000);
	tlen = sprintf(tbuf, "[%5lu.%06lu] ",
			(unsigned long) t,
			nanosec_rem / 1000);

	/* Overflow buffer size */
	if (idx + tlen > sec_tsp_log_size - 1) {
		tlen = scnprintf(&sec_tsp_log_buf[0],
						tlen + 1, "%s", tbuf);
		*sec_tsp_log_ptr = tlen;
	} else {
		tlen = scnprintf(&sec_tsp_log_buf[idx], tlen + 1, "%s", tbuf);
		*sec_tsp_log_ptr += tlen;
	}

	return *sec_tsp_log_ptr;
}

#define TSP_BUF_SIZE 512
void sec_debug_tsp_log(char *fmt, ...)
{
	va_list args;
	char buf[TSP_BUF_SIZE];
	int len = 0;
	unsigned int idx;
	unsigned int size;

	/* In case of sec_tsp_log_setup is failed */
	if (!sec_tsp_log_size)
		return;
	va_start(args, fmt);
	vsnprintf(buf, sizeof(buf), fmt, args);
	va_end(args);

	idx = *sec_tsp_log_ptr;
	size = strlen(buf);

	idx = sec_tsp_log_timestamp(idx);

	/* Overflow buffer size */
	if (idx + size > sec_tsp_log_size - 1) {
		len = scnprintf(&sec_tsp_log_buf[0],
						size + 1, "%s", buf);
		*sec_tsp_log_ptr = len;
	} else {
		len = scnprintf(&sec_tsp_log_buf[idx], size + 1, "%s", buf);
		*sec_tsp_log_ptr += len;
	}
}
EXPORT_SYMBOL(sec_debug_tsp_log);

void sec_debug_tsp_log_msg(char *msg, char *fmt, ...)
{
	va_list args;
	char buf[TSP_BUF_SIZE];
	int len = 0;
	unsigned int idx;
	unsigned int size;
	unsigned int size_dev_name;

	/* In case of sec_tsp_log_setup is failed */
	if (!sec_tsp_log_size)
		return;
	va_start(args, fmt);
	vsnprintf(buf, sizeof(buf), fmt, args);
	va_end(args);

	idx = *sec_tsp_log_ptr;
	size = strlen(buf);
	size_dev_name = strlen(msg);

	idx = sec_tsp_log_timestamp(idx);

	/* Overflow buffer size */
	if (idx + size + size_dev_name + 3 + 1 > sec_tsp_log_size) {
		len = scnprintf(&sec_tsp_log_buf[0],
						size + size_dev_name + 3 + 1, "%s : %s", msg, buf);
		*sec_tsp_log_ptr = len;
	} else {
		len = scnprintf(&sec_tsp_log_buf[idx], size + size_dev_name + 3 + 1, "%s : %s", msg, buf);
		*sec_tsp_log_ptr += len;
	}
}
EXPORT_SYMBOL(sec_debug_tsp_log_msg);

static ssize_t sec_tsp_log_write(struct file *file,
					     const char __user *buf,
					     size_t count, loff_t *ppos)
{
	char *page = NULL;
	ssize_t ret;
	int new_value;

	if (!sec_tsp_log_buf)
		return 0;

	ret = -ENOMEM;
	if (count >= PAGE_SIZE)
		return ret;

	ret = -ENOMEM;
	page = (char *)get_zeroed_page(GFP_KERNEL);
	if (!page)
		return ret;;

	ret = -EFAULT;
	if (copy_from_user(page, buf, count))
		goto out;

	ret = -EINVAL;
	if (sscanf(page, "%u", &new_value) != 1) {
		pr_info("%s\n", page);
		/* print tsp_log to sec_tsp_log_buf */
		sec_debug_tsp_log(page);
	}
	ret = count;
out:
	free_page((unsigned long)page);
	return ret;
}

static ssize_t sec_tsp_log_read(struct file *file, char __user *buf,
								size_t len, loff_t *offset)
{
	loff_t pos = *offset;
	ssize_t count;

	if (sec_tsp_log_buf == NULL)
		return 0;

	if (pos >= *sec_tsp_log_ptr)
		return 0;

	count = min(len, (size_t)(*sec_tsp_log_ptr - pos));
	if (copy_to_user(buf, sec_tsp_log_buf + pos, count))
		return -EFAULT;
	*offset += count;
	return count;
}

static const struct file_operations tsp_msg_file_ops = {
	.owner = THIS_MODULE,
	.read = sec_tsp_log_read,
	.write = sec_tsp_log_write,
	.llseek = generic_file_llseek,
};

static int __init sec_tsp_log_late_init(void)
{
	struct proc_dir_entry *entry;
	if (sec_tsp_log_buf == NULL)
		return 0;

	entry = proc_create("tsp_msg", S_IFREG | S_IRUGO,
			NULL, &tsp_msg_file_ops);
	if (!entry) {
		pr_err("%s: failed to create proc entry\n", __func__);
		return 0;
	}

	proc_set_size(entry, sec_tsp_log_size);

	return 0;
}
late_initcall(sec_tsp_log_late_init);
#endif /* CONFIG_SEC_DEBUG_TSP_LOG */
