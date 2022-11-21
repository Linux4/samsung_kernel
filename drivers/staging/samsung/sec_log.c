/*
 * Copyright (c) 2016 Samsung Electronics Co., Ltd.
 *      http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/bootmem.h>
#include <linux/slab.h>
#include <linux/stat.h>
#include <linux/module.h>
#include <linux/uaccess.h>
#include <linux/proc_fs.h>
#include <linux/memblock.h>
#include <linux/sec_debug.h>

#define LOG_MAGIC 0x4d474f4c	/* "LOGM" */

#ifdef CONFIG_SEC_AVC_LOG
static unsigned *sec_avc_log_ptr;
static char *sec_avc_log_buf;
static unsigned sec_avc_log_size;

static int __init sec_avc_log_setup(char *str)
{
	unsigned size = memparse(str, &str);
	unsigned long base = 0;
	unsigned *sec_avc_log_mag;

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

	sec_avc_log_buf = (char *)__phys_to_virt(base);
	sec_avc_log_size = size - (unsigned)(sizeof(*sec_avc_log_ptr) + sizeof(*sec_avc_log_mag));
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

	sec_tsp_log_buf = (char *)__phys_to_virt(base);
	sec_tsp_log_size = size - (unsigned)(sizeof(*sec_tsp_log_ptr) + sizeof(*sec_tsp_log_mag));
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
	size_t size;

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
	size_t size;
	size_t size_dev_name;

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

