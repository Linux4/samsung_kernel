/*  sound/soc/samsung/samsung_audio_debugfs.c
 *
 *  Copyright (C) 2015 Samsung Electronics Co.Ltd
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 */

#include <stdarg.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/regmap.h>
#include <linux/slab.h>
#include <linux/time.h>
#include <linux/uaccess.h>

#include <asm/stacktrace.h>

#include <sound/samsung_audio_debugfs.h>

static struct dentry *audio_debugfs;
static int debug_enable;

static int regdump_id;
static int stackdump_id;
static struct audio_log_data debug_log_data;
static struct audio_regdump_data regdump_data[MAX_NUM_REGDUMP];
static struct audio_stackdump_data stackdump_data[MAX_NUM_STACKDUMP];

struct snd_soc_codec *dump_codec;
EXPORT_SYMBOL_GPL(dump_codec);

static ssize_t audio_regdump_read_file(struct file *file, char __user *user_buf,
					size_t count, loff_t *ppos)
{
	size_t buf_pos = 0;
	ssize_t ret, size;
	int i, num_msg;
	char *buf;
	unsigned long len = 0;
	struct audio_regdump_data *dump_data = file->private_data;

	if (*ppos < 0 || !count)
		return -EINVAL;

	size = sizeof(dump_data->reg_dump);

	buf = kzalloc(size, GFP_KERNEL);
	if (!buf) {
		pr_err("%s: alloc failed\n", __func__);
		return -ENOMEM;
	}

	num_msg = dump_data->num_regs;

	for (i = 0; i < num_msg; i++) {
		len += snprintf(buf + buf_pos, size - len, "%s",
				dump_data->reg_dump[i]);
		buf_pos = len;
	}
	if (len > size)
		len = size;

	ret = simple_read_from_buffer(user_buf, count, ppos, buf, len);
	kfree(buf);

	return ret;
}

static const struct file_operations audio_regdump_fops = {
	.open = simple_open,
	.read = audio_regdump_read_file,
	.llseek = default_llseek,
};

int audio_debugfs_regdump(int start_reg, int end_reg)
{
	int id, i = 0;
	char name[10];
	struct audio_regdump_data *dump_data;
	struct regmap *dump_regmap;
	unsigned int cache_val, reg_val;

	if (dump_codec == NULL) {
		pr_info("%s: codec is NULL\n", __func__);
		return -1;
	}

	if (!debug_enable) {
		pr_info("%s: not enabled\n", __func__);
		return -1;
	}

	id = regdump_id % MAX_NUM_REGDUMP;

	dump_data = &regdump_data[id];
	if (!dump_data->created) {
		sprintf(name, "dump_%d", id);
		pr_info("%s: create debugfs %s\n", __func__, name);
		debugfs_create_file(name, S_IRUSR,
				audio_debugfs, dump_data, &audio_regdump_fops);
		dump_data->created = 1;
	}

	sprintf(dump_data->reg_dump[0],
		"%02d: 0x%04x-0x%04x\n", regdump_id, start_reg, end_reg);

	sprintf(dump_data->reg_dump[1],
		" addr  : cache  reg\n");

	dump_data->num_regs = (unsigned int) (end_reg - start_reg) + 1;
	if (dump_data->num_regs > MAX_NUM_REGDUMP_MSG - 1)
		dump_data->num_regs = MAX_NUM_REGDUMP_MSG - 1;

	dump_regmap = dump_codec->control_data;

	for (i = 0; i < dump_data->num_regs - 2; i++) {
		cache_val = snd_soc_read(dump_codec, (start_reg + i));

		dump_codec->cache_bypass = 1;
		regcache_cache_bypass(dump_regmap, 1);
		reg_val = snd_soc_read(dump_codec, (start_reg + i));
		regcache_cache_bypass(dump_regmap, 0);
		dump_codec->cache_bypass = 0;

		sprintf(dump_data->reg_dump[i + 2],
				"0x%04x : 0x%04x 0x%04x %s\n",
				(start_reg + i), cache_val, reg_val,
				(cache_val == reg_val) ? "" : "v");
	}

	regdump_id++;
	return regdump_id - 1;
}
EXPORT_SYMBOL_GPL(audio_debugfs_regdump);

static ssize_t audio_stackdump_read_file(struct file *file,
			char __user *user_buf, size_t count, loff_t *ppos)
{
	size_t buf_pos = 0;
	ssize_t ret, size;
	int i, num_msg;
	char *buf;
	unsigned long len = 0;
	struct audio_stackdump_data *dump_data = file->private_data;

	if (*ppos < 0 || !count)
		return -EINVAL;

	size = sizeof(dump_data->stack_dump);

	buf = kzalloc(size, GFP_KERNEL);
	if (!buf) {
		pr_err("%s: alloc failed\n", __func__);
		return -ENOMEM;
	}

	num_msg = dump_data->num_stacks;

	for (i = 0; i < num_msg; i++) {
		len += snprintf(buf + buf_pos, size - len, "%s",
				dump_data->stack_dump[i]);
		buf_pos = len;
	}
	if (len > size)
		len = size;

	ret = simple_read_from_buffer(user_buf, count, ppos, buf, len);
	kfree(buf);

	return ret;
}

static const struct file_operations audio_stackdump_fops = {
	.open = simple_open,
	.read = audio_stackdump_read_file,
	.llseek = default_llseek,
};

int audio_debugfs_stackdump(void)
{
	int id, i = 0;
	char name[10];
	struct audio_stackdump_data *dump_data;
	struct stackframe frame;
	const register unsigned long current_sp asm ("sp");

	if (!debug_enable) {
		pr_info("%s: not enabled\n", __func__);
		return -1;
	}

	id = stackdump_id % MAX_NUM_STACKDUMP;

	dump_data = &stackdump_data[id];
	if (!dump_data->created) {
		sprintf(name, "stack_%d", id);
		pr_info("%s: create debugfs %s\n", __func__, name);
		debugfs_create_file(name, S_IRUSR,
			audio_debugfs, dump_data, &audio_stackdump_fops);
		dump_data->created = 1;
	}

	sprintf(dump_data->stack_dump[i],
			"dumpstack id : %d\n", stackdump_id);

	frame.fp = (unsigned long)__builtin_frame_address(0);
	frame.sp = current_sp;
	frame.pc = (unsigned long) audio_debugfs_stackdump;

	while (1) {
		unsigned long where = frame.pc;
		int ret;

		ret = unwind_frame(&frame);
		if (ret < 0)
			break;

		i++;
		if (i > MAX_NUM_STACKDUMP_MSG - 1) {
			i = MAX_NUM_STACKDUMP_MSG - 1;
			break;
		}
		sprintf(dump_data->stack_dump[i], "%pS\n", (void *) where);
	}

	dump_data->num_stacks = i;

	stackdump_id++;
	return stackdump_id - 1;
}
EXPORT_SYMBOL_GPL(audio_debugfs_stackdump);

static ssize_t audio_log_read_file(struct file *file, char __user *user_buf,
					size_t count, loff_t *ppos)
{
	size_t buf_pos = 0;
	ssize_t ret, size;
	int i, num_msg;
	char *buf;
	unsigned long len = 0;
	struct audio_log_data *log_data = &debug_log_data;

	if (*ppos < 0 || !count)
		return -EINVAL;

	size = sizeof(log_data->audio_log);

	buf = kzalloc(size, GFP_KERNEL);
	if (!buf) {
		pr_err("%s: alloc failed\n", __func__);
		return -ENOMEM;
	}

	if (log_data->full)
		num_msg = MAX_NUM_DEBUG_MSG;
	else
		num_msg = log_data->count;

	for (i = 0; i < num_msg; i++) {
		len += snprintf(buf + buf_pos, size - len, "%s",
				log_data->audio_log[i]);
		buf_pos = len;
	}
	if (len > size)
		len = size;

	ret = simple_read_from_buffer(user_buf, count, ppos, buf, len);
	kfree(buf);

	return ret;
}

static const struct file_operations audio_log_fops = {
	.open = simple_open,
	.read = audio_log_read_file,
	.llseek = default_llseek,
};

void audio_debugfs_log(const char *fmt, ...)
{
	struct timeval tv;
	struct tm log_tm;
	u32 len = 0;
	va_list args;
	struct audio_log_data *log_data = &debug_log_data;

	if (!debug_enable) {
		pr_info("%s: not enabled\n", __func__);
		return;
	}

	do_gettimeofday(&tv);
	time_to_tm(tv.tv_sec, 0, &log_tm);

	len += sprintf(log_data->audio_log[log_data->count] + len,
				"%02d ", log_data->count + 1);

	len += sprintf(log_data->audio_log[log_data->count] + len,
		"[%02d:%02d:%02d] ",
		((int) log_tm.tm_hour + 9) >= 24 ?
			((int) log_tm.tm_hour + 9 - 24) :
			((int) log_tm.tm_hour + 9),
		(int) log_tm.tm_min,
		(int) log_tm.tm_sec);

	va_start(args, fmt);
	len += vsnprintf(log_data->audio_log[log_data->count] + len,
				MAX_DEBUG_MSG_SIZE - len, fmt, args);
	va_end(args);

	if (len >= MAX_DEBUG_MSG_SIZE) {
		len = MAX_DEBUG_MSG_SIZE - 1;
		len += sprintf(log_data->audio_log[log_data->count] + len,
				"\n");
	}

	log_data->count++;
	if (log_data->count > MAX_NUM_DEBUG_MSG - 1) {
		log_data->full = 1;
		log_data->count = 0;
	}
}
EXPORT_SYMBOL_GPL(audio_debugfs_log);

static ssize_t enable_read_file(struct file *file, char __user *user_buf,
					size_t count, loff_t *ppos)
{
	char buf[4];
	int len;

	len = sprintf(buf, "%d\n", debug_enable);
	return simple_read_from_buffer(user_buf, count, ppos, buf, len);
}

static ssize_t enable_write_file(struct file *file,
				     const char __user *user_buf,
				     size_t count, loff_t *ppos)
{
	char buf[4];
	size_t size;
	int value;

	size = min(count, (sizeof(buf)-1));
	if (copy_from_user(buf, user_buf, size)) {
		pr_err("%s: copy_from_user err\n", __func__);
		return -EFAULT;
	}
	buf[size] = 0;

	if (kstrtoint(buf, 10, &value)) {
		pr_err("%s: Invalid value\n", __func__);
		return -EINVAL;
	}

	debug_enable = value;

	return size;
}

static const struct file_operations enable_fops = {
	.open = simple_open,
	.read = enable_read_file,
	.write = enable_write_file,
	.llseek = default_llseek,
};

static int __init audio_debugfs_init(void)
{
	audio_debugfs = debugfs_create_dir("audio", NULL);
	if (!audio_debugfs) {
		pr_err("Failed to create audio_debugfs\n");
		return -EPERM;
	}

	debugfs_create_file("enable", S_IRUSR, audio_debugfs,
				NULL, &enable_fops);

	debugfs_create_file("log", S_IRUSR, audio_debugfs,
				NULL, &audio_log_fops);

	return 0;
}

static void __exit audio_debugfs_exit(void)
{
	debugfs_remove_recursive(audio_debugfs);
}

module_init(audio_debugfs_init);
module_exit(audio_debugfs_exit);

MODULE_DESCRIPTION("Samsung Electronics Audio Debug driver");
MODULE_LICENSE("GPL");
