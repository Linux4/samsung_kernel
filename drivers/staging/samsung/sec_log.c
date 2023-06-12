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
#include <linux/sec_debug.h>

#define SEC_LOG_BASE		0x43700000
#define SEC_LASTKMSG_BASE	0x43900000

static u32 __initdata seclog_base = SEC_LOG_BASE;
static u32 __initdata seclog_size = SZ_2M;
static u32 __initdata seclastklog_base = SEC_LASTKMSG_BASE;
static u32 __initdata seclastklog_size = SZ_2M;
static char *sec_log_buf;
static size_t sec_log_size;
static unsigned int *sec_log_pos;
static unsigned int *sec_log_head;

#ifdef CONFIG_SEC_DEBUG_LAST_KMSG
static char *last_kmsg_buffer;
static size_t last_kmsg_size;
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
#endif /* CONFIG_SEC_DEBUG_LAST_KMSG */

static void sec_log_buf_write(struct console *console, const char *text,
			      unsigned int size)
{
	unsigned int pos = *sec_log_pos;

	if (likely(size + pos <= sec_log_size))
		memcpy(&sec_log_buf[pos], text, size);
	else {
		unsigned int first = sec_log_size - pos;
		unsigned int second = size - first;
		memcpy(&sec_log_buf[pos], text, first);
		memcpy(&sec_log_buf[0], text + first, second);
		*sec_log_head = 1;
	}
	(*sec_log_pos) += size;

	/* Check overflow */
	if (unlikely(*sec_log_pos >= sec_log_size))
		*sec_log_pos -= sec_log_size;
}

static struct console sec_console = {
	.name	= "sec_log_buf",
	.write	= sec_log_buf_write,
	.flags	= CON_PRINTBUFFER | CON_ENABLED | CON_ANYTIME,
	.index	= -1,
};

static inline void emit_sec_log(char *text, unsigned int size)
{
	unsigned int pos = *sec_log_pos;

	if (likely(size + pos <= sec_log_size))
		memcpy(&sec_log_buf[pos], text, size);
	else {
		unsigned int first = sec_log_size - pos;
		unsigned int second = size - first;
		memcpy(&sec_log_buf[pos], text, first);
		memcpy(&sec_log_buf[0], text + first, second);
		*sec_log_head = 1;
	}
	(*sec_log_pos) += size;

	/* Check overflow */
	if (unlikely(*sec_log_pos >= sec_log_size))
		*sec_log_pos -= sec_log_size;
}

static int __init sec_log_buf_init(void)
{
	unsigned int *sec_log_mag;

	if (sec_debug_level.en.kernel_fault == 0) {
		memblock_free_late(seclog_base, seclog_size);
		pr_info("freed sec-log memory : %uMB at %#.8x\n",
			(unsigned)seclog_size / SZ_1M, (unsigned)seclog_base);
		memblock_free_late(seclastklog_base, seclastklog_size);
		pr_info("freed sec-lastklog memory : %uMB at %#.8x\n",
			(unsigned)seclastklog_size / SZ_1M, (unsigned)seclastklog_base);
		return 0;
	}

	sec_log_buf = persistent_ram_vmap(seclog_base, seclog_size, 1);
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

	register_console(&sec_console);
	unregister_console(&sec_console);
	register_log_text_hook(emit_sec_log, sec_log_buf, sec_log_pos, sec_log_size);

	sec_getlog_supply_kernel(phys_to_virt(seclog_base));

	return 0;
}
arch_initcall(sec_log_buf_init);

#ifdef CONFIG_SEC_AVC_LOG
static unsigned *sec_avc_log_ptr;
static char *sec_avc_log_buf;
static unsigned sec_avc_log_size;

static int __init sec_avc_log_setup(char *str)
{
	unsigned size = memparse(str, &str);
	unsigned long base = 0;
	unsigned *sec_avc_log_mag;

	if (sec_debug_level.en.kernel_fault == 0)
		return 0;

	/* If we encounter any problem parsing str ... */
	if (!size || size != roundup_pow_of_two(size) || *str != '@'
		|| kstrtoul(str + 1, 0, &base))
			goto out;

	if (memblock_is_region_reserved(base, size) ||
			memblock_reserve(base, size)) {
		pr_err("%s: failed reserving size %d " \
					"at base 0x%lx\n", __func__, size, base);
		goto out;
	}

	sec_avc_log_buf = persistent_ram_vmap(base, size, 1);
	sec_avc_log_size = size - (sizeof(*sec_avc_log_ptr) + sizeof(*sec_avc_log_mag));
	sec_avc_log_ptr = (unsigned int *)(sec_avc_log_buf + sec_avc_log_size);
	sec_avc_log_mag = (unsigned int *)(sec_avc_log_buf + sec_avc_log_size + sizeof(*sec_avc_log_ptr));

	pr_info("%s: *sec_avc_log_ptr:%x " \
		"sec_avc_log_buf:%p sec_log_size:0x%x\n",
		__func__, *sec_avc_log_ptr, sec_avc_log_buf,
		sec_avc_log_size);

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

	if (sec_debug_level.en.kernel_fault == 0)
		return 0;

	/* If we encounter any problem parsing str ... */
	if (!size || size != roundup_pow_of_two(size) || *str != '@'
		|| kstrtoul(str + 1, 0, &base))
			goto out;

	if (memblock_is_region_reserved(base, size) ||
			memblock_reserve(base, size)) {
		pr_err("%s: failed reserving size %d " \
					"at base 0x%lx\n", __func__, size, base);
		goto out;
	}

	sec_tsp_log_buf = persistent_ram_vmap(base, size, 1);
	sec_tsp_log_size = size - (sizeof(*sec_tsp_log_ptr) + sizeof(*sec_tsp_log_mag));
	sec_tsp_log_ptr = (unsigned int *)(sec_tsp_log_buf + sec_tsp_log_size);
	sec_tsp_log_mag = (unsigned int *)(sec_tsp_log_buf + sec_tsp_log_size + sizeof(*sec_tsp_log_ptr));

	pr_info("%s: *sec_tsp_log_ptr:%x " \
		"sec_tsp_log_buf:%p sec_tsp_log_size:0x%x\n",
		__func__, *sec_tsp_log_ptr, sec_tsp_log_buf,
		sec_tsp_log_size);

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
