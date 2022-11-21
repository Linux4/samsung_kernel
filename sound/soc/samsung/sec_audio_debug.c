/*
 *  sec_audio_debug.c
 *
 *  Copyright (c) 2018 Samsung Electronics
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include <linux/debugfs.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/time.h>
#include <linux/uaccess.h>

#include <linux/sched/clock.h>

#include <sound/soc.h>
#include <sound/samsung/sec_audio_debug.h>

#include <linux/sec_debug.h>
#include <sound/samsung/abox.h>
#include "abox/abox.h"

#define DBG_STR_BUFF_SZ 256

struct sec_audio_debug_data {
	char *dbg_str_buf;
	unsigned long long mode_time;
	unsigned long mode_nanosec_t;
	struct mutex dbg_lock;
};

static struct sec_audio_debug_data *p_debug_data;

struct sec_audio_log_data {
	ssize_t buff_idx;
	int full;
	char *audio_log_buffer;
	ssize_t read_idx;
};

static struct dentry *audio_debugfs;
static size_t debug_enable;
static struct sec_audio_log_data *p_debug_log_data;
static size_t debug_bootenable;
static struct sec_audio_log_data *p_debug_bootlog_data;
static unsigned int debug_buff_switch;

int is_abox_rdma_enabled(int id)
{
	struct abox_data *data = abox_get_abox_data();

	return (readl(data->sfr_base + ABOX_RDMA_BASE + (ABOX_RDMA_INTERVAL * id)
		+ ABOX_RDMA_CTRL0) & ABOX_RDMA_ENABLE_MASK);
}

int is_abox_wdma_enabled(int id)
{
	struct abox_data *data = abox_get_abox_data();

	return (readl(data->sfr_base + ABOX_WDMA_BASE + (ABOX_WDMA_INTERVAL * id)
		+ ABOX_WDMA_CTRL) & ABOX_WDMA_ENABLE_MASK);
}

void abox_debug_string_update(void)
{
	struct abox_data *data = abox_get_abox_data();
	int i;
	unsigned int len = 0;

	if (!p_debug_data)
		return;

	mutex_lock(&p_debug_data->dbg_lock);

	p_debug_data->mode_time = data->audio_mode_time;
	p_debug_data->mode_nanosec_t = do_div(p_debug_data->mode_time, NSEC_PER_SEC);

	p_debug_data->dbg_str_buf = kzalloc(sizeof(char) * DBG_STR_BUFF_SZ, GFP_KERNEL);
	if (!p_debug_data->dbg_str_buf) {
		pr_err("%s: no memory\n", __func__);
		mutex_unlock(&p_debug_data->dbg_lock);
		return;
	}

	if (!abox_is_on()) {
		snprintf(p_debug_data->dbg_str_buf, DBG_STR_BUFF_SZ, "Abox disabled");
		goto buff_done;
	}

	for (i = 0; i <= 14; i++) {
		len += snprintf(p_debug_data->dbg_str_buf + len, DBG_STR_BUFF_SZ - len,
						"%08x ", readl(data->sfr_base + ABOX_CPU_R(i)));
		if (len >= DBG_STR_BUFF_SZ - 1)
			goto buff_done;
	}
	len += snprintf(p_debug_data->dbg_str_buf + len, DBG_STR_BUFF_SZ - len,
						"PC:%08x ", readl(data->sfr_base + ABOX_CPU_PC));
	if (len >= DBG_STR_BUFF_SZ - 1)
		goto buff_done;

	len += snprintf(p_debug_data->dbg_str_buf + len, DBG_STR_BUFF_SZ - len,
						"L2C:%08x ", readl(data->sfr_base + ABOX_CPU_L2C_STATUS));
	if (len >= DBG_STR_BUFF_SZ - 1)
		goto buff_done;

	len += snprintf(p_debug_data->dbg_str_buf + len, DBG_STR_BUFF_SZ - len,
						"m%d:%05lu", data->audio_mode, (unsigned long) p_debug_data->mode_time);

buff_done:
	/* sec_debug_set_extra_info_rvd1(p_debug_data->dbg_str_buf); */

	kfree(p_debug_data->dbg_str_buf);
	p_debug_data->dbg_str_buf = NULL;
	mutex_unlock(&p_debug_data->dbg_lock);
}
EXPORT_SYMBOL_GPL(abox_debug_string_update);

static int get_debug_buffer_switch(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = debug_buff_switch;

	return 0;
}

static int set_debug_buffer_switch(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	unsigned int val;
	int ret = 0;

	val = (unsigned int)ucontrol->value.integer.value[0];

	if (val) {
		alloc_sec_audio_log(SZ_1M);
		debug_buff_switch = SZ_1M;
	} else {
		alloc_sec_audio_log(0);
		debug_buff_switch = 0;
	}

	return ret;
}

static const struct snd_kcontrol_new debug_controls[] = {
	SOC_SINGLE_BOOL_EXT("Debug Buffer Switch", 0,
			get_debug_buffer_switch,
			set_debug_buffer_switch),
};

int register_debug_mixer(struct snd_soc_card *card)
{
	return snd_soc_add_card_controls(card, debug_controls,
				ARRAY_SIZE(debug_controls));
}
EXPORT_SYMBOL_GPL(register_debug_mixer);

static int audio_log_open_file(struct inode *inode, struct  file *file)
{
	p_debug_log_data->read_idx = 0;

	return 0;
}

static ssize_t audio_log_read_file(struct file *file, char __user *user_buf,
					size_t count, loff_t *ppos)
{
	size_t num_msg;
	ssize_t ret;
	loff_t pos = *ppos;
	size_t copy_len;

	if (*ppos < 0 || !count)
		return -EINVAL;

	if (p_debug_log_data->full)
		num_msg = debug_enable - p_debug_log_data->read_idx;
	else
		num_msg = (size_t) p_debug_log_data->buff_idx - p_debug_log_data->read_idx;

	if (num_msg < 0) {
		pr_err("%s: buff idx invalid\n", __func__);
		return -EINVAL;
	}

	if (pos > num_msg) {
		pr_err("%s: invalid offset\n", __func__);
		return -EINVAL;
	}

	copy_len = min(count, num_msg);

	ret = copy_to_user(user_buf, p_debug_log_data->audio_log_buffer + p_debug_log_data->read_idx, copy_len);
	if (ret) {
		pr_err("%s: copy fail %d\n", __func__, ret);
		return -EFAULT;
	}
	p_debug_log_data->read_idx += copy_len;

	return copy_len;
}

static const struct file_operations audio_log_fops = {
	.open = audio_log_open_file,
	.read = audio_log_read_file,
	.llseek = default_llseek,
};

void sec_audio_log(int level, const char *fmt, ...)
{
	va_list args;
	unsigned long long time;
	unsigned long nanosec_t;

	if (!debug_enable) {
		return;
	}

	time = local_clock();
	nanosec_t = do_div(time, 1000000000);

	p_debug_log_data->buff_idx +=
			scnprintf(p_debug_log_data->audio_log_buffer + p_debug_log_data->buff_idx,
					debug_enable - p_debug_log_data->buff_idx, "<%d> [%5lu.%06lu] ",
					level, (unsigned long) time, nanosec_t / 1000);

	if (p_debug_log_data->buff_idx >= debug_enable - 1) {
		p_debug_log_data->full = 1;
		p_debug_log_data->buff_idx = 0;
		/* re-write overflowed message */
		p_debug_log_data->buff_idx +=
				scnprintf(p_debug_log_data->audio_log_buffer + p_debug_log_data->buff_idx,
						debug_enable - p_debug_log_data->buff_idx, "<%d> [%5lu.%06lu] ",
						level, (unsigned long) time, nanosec_t / 1000);
	}

	va_start(args, fmt);
	p_debug_log_data->buff_idx +=
		vsnprintf(p_debug_log_data->audio_log_buffer + p_debug_log_data->buff_idx,
				debug_enable - p_debug_log_data->buff_idx, fmt, args);
	va_end(args);

	if (p_debug_log_data->buff_idx >= debug_enable - 1) {
		p_debug_log_data->full = 1;
		p_debug_log_data->buff_idx = 0;
		/* re-write overflowed message */
		p_debug_log_data->buff_idx +=
		scnprintf(p_debug_log_data->audio_log_buffer + p_debug_log_data->buff_idx,
				debug_enable - p_debug_log_data->buff_idx, "<%d> [%5lu.%06lu] ",
				level, (unsigned long) time, nanosec_t / 1000);

		va_start(args, fmt);
		p_debug_log_data->buff_idx +=
			vsnprintf(p_debug_log_data->audio_log_buffer + p_debug_log_data->buff_idx,
					debug_enable - p_debug_log_data->buff_idx, fmt, args);
		va_end(args);
	}
}
EXPORT_SYMBOL_GPL(sec_audio_log);

int alloc_sec_audio_log(size_t buffer_len)
{
	if (debug_enable) {
		debug_enable = 0;
		free_sec_audio_log();
	}

	p_debug_log_data->buff_idx = 0;
	p_debug_log_data->full = 0;

	if (buffer_len <= 0) {
		pr_err("%s: Invalid buffer_len %d\n", __func__, buffer_len);
		debug_enable = 0;
		return 0;
	}

	p_debug_log_data->audio_log_buffer = vzalloc(buffer_len);
	if (p_debug_log_data->audio_log_buffer == NULL) {
		pr_err("%s: Failed to alloc audio_log_buffer\n", __func__);
		debug_enable = 0;
		return -ENOMEM;
	}

	debug_enable = buffer_len;

	return debug_enable;
}
EXPORT_SYMBOL_GPL(alloc_sec_audio_log);

void free_sec_audio_log(void)
{
	debug_enable = 0;
	vfree(p_debug_log_data->audio_log_buffer);
	p_debug_log_data->audio_log_buffer = NULL;
}

static ssize_t log_enable_read_file(struct file *file, char __user *user_buf,
					size_t count, loff_t *ppos)
{
	char buf[16];
	int len;

	len = snprintf(buf, 16, "%d\n", debug_enable);
	return simple_read_from_buffer(user_buf, count, ppos, buf, len);
}

static ssize_t log_enable_write_file(struct file *file,
				     const char __user *user_buf,
				     size_t count, loff_t *ppos)
{
	char buf[16];
	size_t size;
	u64 value;

	size = min(count, (sizeof(buf)-1));
	if (copy_from_user(buf, user_buf, size)) {
		pr_err("%s: copy_from_user err\n", __func__);
		return -EFAULT;
	}
	buf[size] = 0;

	if (kstrtou64(buf, 10, &value)) {
		pr_err("%s: Invalid value\n", __func__);
		return -EINVAL;
	}

	/* do not alloc over 2MB */
	if (value > SZ_2M)
		value = SZ_2M;

	alloc_sec_audio_log((size_t) value);

	return size;
}

static const struct file_operations log_enable_fops = {
	.open = simple_open,
	.read = log_enable_read_file,
	.write = log_enable_write_file,
	.llseek = default_llseek,
};

static int audio_bootlog_open_file(struct inode *inode, struct  file *file)
{
	p_debug_bootlog_data->read_idx = 0;

	return 0;
}

static ssize_t audio_bootlog_read_file(struct file *file, char __user *user_buf,
					size_t count, loff_t *ppos)
{
	size_t num_msg;
	ssize_t ret;
	loff_t pos = *ppos;
	size_t copy_len;

	if (*ppos < 0 || !count)
		return -EINVAL;

	if (p_debug_bootlog_data->full)
		num_msg = debug_bootenable - p_debug_bootlog_data->read_idx;
	else
		num_msg = (size_t) p_debug_bootlog_data->buff_idx - p_debug_bootlog_data->read_idx;

	if (num_msg < 0) {
		pr_err("%s: buff idx invalid\n", __func__);
		return -EINVAL;
	}

	if (pos > num_msg) {
		pr_err("%s: invalid offset\n", __func__);
		return -EINVAL;
	}

	copy_len = min(count, num_msg);

	ret = copy_to_user(user_buf, p_debug_bootlog_data->audio_log_buffer + p_debug_bootlog_data->read_idx, copy_len);
	if (ret) {
		pr_err("%s: copy fail %d\n", __func__, ret);
		return -EFAULT;
	}
	p_debug_bootlog_data->read_idx += copy_len;

	return copy_len;
}

static const struct file_operations audio_bootlog_fops = {
	.open = audio_bootlog_open_file,
	.read = audio_bootlog_read_file,
	.llseek = default_llseek,
};

void sec_audio_bootlog(int level, const char *fmt, ...)
{
	va_list args;
	unsigned long long time;
	unsigned long nanosec_t;

	if (!debug_bootenable) {
		return;
	}

	time = local_clock();
	nanosec_t = do_div(time, 1000000000);

	p_debug_bootlog_data->buff_idx +=
			scnprintf(p_debug_bootlog_data->audio_log_buffer + p_debug_bootlog_data->buff_idx,
					debug_bootenable - p_debug_bootlog_data->buff_idx, "<%d> [%5lu.%06lu] ",
					level, (unsigned long) time, nanosec_t / 1000);

	if (p_debug_bootlog_data->buff_idx >= debug_bootenable - 1) {
		p_debug_bootlog_data->full = 1;
		p_debug_bootlog_data->buff_idx = 0;
		/* re-write overflowed message */
		p_debug_bootlog_data->buff_idx +=
				scnprintf(p_debug_bootlog_data->audio_log_buffer + p_debug_bootlog_data->buff_idx,
						debug_bootenable - p_debug_bootlog_data->buff_idx, "<%d> [%5lu.%06lu] ",
						level, (unsigned long) time, nanosec_t / 1000);
	}

	va_start(args, fmt);
	p_debug_bootlog_data->buff_idx +=
		vsnprintf(p_debug_bootlog_data->audio_log_buffer + p_debug_bootlog_data->buff_idx,
				debug_bootenable - p_debug_bootlog_data->buff_idx, fmt, args);
	va_end(args);

	if (p_debug_bootlog_data->buff_idx >= debug_bootenable - 1) {
		p_debug_bootlog_data->full = 1;
		p_debug_bootlog_data->buff_idx = 0;
		/* re-write overflowed message */
		p_debug_bootlog_data->buff_idx +=
		scnprintf(p_debug_bootlog_data->audio_log_buffer + p_debug_bootlog_data->buff_idx,
				debug_bootenable - p_debug_bootlog_data->buff_idx, "<%d> [%5lu.%06lu] ",
				level, (unsigned long) time, nanosec_t / 1000);

		va_start(args, fmt);
		p_debug_bootlog_data->buff_idx +=
			vsnprintf(p_debug_bootlog_data->audio_log_buffer + p_debug_bootlog_data->buff_idx,
					debug_bootenable - p_debug_bootlog_data->buff_idx, fmt, args);
		va_end(args);
	}
}
EXPORT_SYMBOL_GPL(sec_audio_bootlog);

int alloc_sec_audio_bootlog(size_t buffer_len)
{
	if (debug_bootenable) {
		debug_bootenable = 0;
		free_sec_audio_bootlog();
	}

	p_debug_bootlog_data->buff_idx = 0;
	p_debug_bootlog_data->full = 0;

	if (buffer_len <= 0) {
		pr_err("%s: Invalid buffer_len %d\n", __func__, buffer_len);
		debug_bootenable = 0;
		return 0;
	}

	p_debug_bootlog_data->audio_log_buffer = kzalloc(buffer_len, GFP_KERNEL);
	if (p_debug_bootlog_data->audio_log_buffer == NULL) {
		pr_err("%s: Failed to alloc audio_log_buffer\n", __func__);
		debug_bootenable = 0;
		return -ENOMEM;
	}

	debug_bootenable = buffer_len;

	return debug_bootenable;
}
EXPORT_SYMBOL_GPL(alloc_sec_audio_bootlog);

void free_sec_audio_bootlog(void)
{
	debug_bootenable = 0;
	kfree(p_debug_bootlog_data->audio_log_buffer);
	p_debug_bootlog_data->audio_log_buffer = NULL;
}

static ssize_t bootlog_enable_read_file(struct file *file, char __user *user_buf,
					size_t count, loff_t *ppos)
{
	char buf[16];
	int len;

	len = snprintf(buf, 16, "%d\n", debug_bootenable);
	return simple_read_from_buffer(user_buf, count, ppos, buf, len);
}

static ssize_t bootlog_enable_write_file(struct file *file,
				     const char __user *user_buf,
				     size_t count, loff_t *ppos)
{
	char buf[16];
	size_t size;
	u64 value;

	size = min(count, (sizeof(buf)-1));
	if (copy_from_user(buf, user_buf, size)) {
		pr_err("%s: copy_from_user err\n", __func__);
		return -EFAULT;
	}
	buf[size] = 0;

	if (kstrtou64(buf, 10, &value)) {
		pr_err("%s: Invalid value\n", __func__);
		return -EINVAL;
	}

	/* do not alloc over 4KB */
	if (value > SZ_4K)
		value = SZ_4K;

	alloc_sec_audio_bootlog((size_t) value);

	return size;
}

static const struct file_operations bootlog_enable_fops = {
	.open = simple_open,
	.read = bootlog_enable_read_file,
	.write = bootlog_enable_write_file,
	.llseek = default_llseek,
};

static int __init sec_audio_debug_init(void)
{
	struct sec_audio_debug_data *data;
	struct sec_audio_log_data *log_data;
	struct sec_audio_log_data *bootlog_data;
	int ret = 0;

	data = kzalloc(sizeof(*data), GFP_KERNEL);
	if (!data) {
		pr_err("%s: failed to alloc data\n", __func__);
		return -ENOMEM;
	}

	p_debug_data = data;
	mutex_init(&p_debug_data->dbg_lock);

	audio_debugfs = debugfs_create_dir("audio", NULL);
	if (!audio_debugfs) {
		pr_err("Failed to create audio debugfs\n");
		return -EPERM;
	}

	debugfs_create_file("log_enable",
			S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP,
			audio_debugfs, NULL, &log_enable_fops);

	debugfs_create_file("log",
			S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP,
			audio_debugfs, NULL, &audio_log_fops);

	debugfs_create_file("bootlog_enable",
			S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP,
			audio_debugfs, NULL, &bootlog_enable_fops);

	debugfs_create_file("bootlog",
			S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP,
			audio_debugfs, NULL, &audio_bootlog_fops);

	log_data = kzalloc(sizeof(*log_data), GFP_KERNEL);
	if (!log_data) {
		pr_err("%s: failed to alloc log_data\n", __func__);
		ret = -ENOMEM;
		goto err_debugfs;
	}

	p_debug_log_data = log_data;

	p_debug_log_data->audio_log_buffer = NULL;
	p_debug_log_data->buff_idx = 0;
	p_debug_log_data->full = 0;
	p_debug_log_data->read_idx = 0;
	debug_enable = 0;

	bootlog_data = kzalloc(sizeof(*bootlog_data), GFP_KERNEL);
	if (!log_data) {
		pr_err("%s: failed to alloc bootlog_data\n", __func__);
		ret = -ENOMEM;
		goto err_log_data;
	}

	p_debug_bootlog_data = bootlog_data;

	p_debug_bootlog_data->audio_log_buffer = NULL;
	p_debug_bootlog_data->buff_idx = 0;
	p_debug_bootlog_data->full = 0;
	p_debug_bootlog_data->read_idx = 0;
	debug_bootenable = 0;

	alloc_sec_audio_bootlog(SZ_1K);

	return 0;

err_log_data:
	kfree(p_debug_log_data);
	p_debug_log_data = NULL;

err_debugfs:
	debugfs_remove_recursive(audio_debugfs);

	kfree(p_debug_data);
	p_debug_data = NULL;

	return ret;
}
early_initcall(sec_audio_debug_init);

static void __exit sec_audio_debug_exit(void)
{
	if (debug_enable)
		free_sec_audio_log();

	if (debug_bootenable)
		free_sec_audio_bootlog();

	if (p_debug_bootlog_data)
		kfree(p_debug_bootlog_data);
	p_debug_bootlog_data = NULL;

	if (p_debug_log_data)
		kfree(p_debug_log_data);
	p_debug_log_data = NULL;

	debugfs_remove_recursive(audio_debugfs);

	kfree(p_debug_data);
	p_debug_data = NULL;
}
module_exit(sec_audio_debug_exit);
MODULE_DESCRIPTION("Samsung Electronics Audio Debug driver");
MODULE_LICENSE("GPL");
