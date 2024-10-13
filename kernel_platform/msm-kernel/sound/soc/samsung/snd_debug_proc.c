/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 */
/*
 *  snd_debug_proc.c
 *
 *  Copyright (c) 2017 Samsung Electronics
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
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 *
 */
#include <linux/err.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/time.h>
#include <linux/uaccess.h>
#include <linux/wait.h>
#include <linux/timer.h>
#include <linux/proc_fs.h>
#include <linux/rtc.h>
#include <linux/mutex.h>
#include <linux/sched/clock.h>

#include <sound/samsung/snd_debug_proc.h>

#define PROC_SDP_DIR		"snd_debug_proc"
#define SDP_INFO_LOG_NAME	"sdp_info_log"
#define SDP_BOOT_LOG_NAME	"sdp_boot_log"
#define MAX_STR_LEN		256


static struct proc_dir_entry *procfs_sdp_dir;
static struct snd_debug_proc sdp_info;
static struct snd_debug_proc sdp_boot;
static bool is_enabled;

/* Audio Debugging Log */
void sdp_info_print(const char *fmt, ...)
{
	char buf[MAX_STR_LEN] = {0, };
	unsigned int len = 0;
	va_list args;
	u64 time;
	unsigned long nsec;

	if (!is_enabled)
		return;

	mutex_lock(&sdp_info.lock);
	/* timestamp */
	time = local_clock();
	nsec = do_div(time, NSEC_PER_SEC);
	len = snprintf(buf, sizeof(buf), "[%6lu.%06ld] ",
			(unsigned long)time, nsec / NSEC_PER_USEC);

	/* append log */
	va_start(args, fmt);
	len += vsnprintf(buf + len, MAX_STR_LEN - len, fmt, args);
	va_end(args);

	/* buffer full, reset position */
	if (sdp_info.buf_pos + len + 1 > AUD_LOG_BUF_SIZE) {
		sdp_info.buf_pos = 0;
		sdp_info.buf_full++;
	}
	sdp_info.buf_pos += scnprintf(&sdp_info.log_buf[sdp_info.buf_pos],
			len + 1, "%s", buf);
	mutex_unlock(&sdp_info.lock);
}
EXPORT_SYMBOL(sdp_info_print);

static ssize_t sdp_info_log_read(struct file *file, char __user *buf,
						size_t len, loff_t *offset)
{
	loff_t pos = *offset;
	ssize_t count = 0;
	size_t size = (sdp_info.buf_full > 0) ?
			AUD_LOG_BUF_SIZE : (size_t)sdp_info.buf_pos;

	mutex_lock(&sdp_info.lock);
	pr_info("%s: pos(%d), full(%d), size(%d)\n", __func__,
				sdp_info.buf_pos, sdp_info.buf_full, size);

	if (pos >= size) {
		mutex_unlock(&sdp_info.lock);
		return 0;
	}

	count = min(len, size);

	if ((pos + count) > size)
		count = size - pos;

	if (copy_to_user(buf, sdp_info.log_buf + pos, count)) {
		mutex_unlock(&sdp_info.lock);
		return -EFAULT;
	}

	*offset += count;

	pr_info("%s: done\n", __func__);

	mutex_unlock(&sdp_info.lock);

	return count;
}

static const struct proc_ops sdp_info_log_ops = {
	.proc_read = sdp_info_log_read,
};

/* Audio Booting Log */
void sdp_boot_print(const char *fmt, ...)
{
	char buf[MAX_STR_LEN] = {0, };
	unsigned int len = 0;
	va_list args;
	u64 time;
	unsigned long nsec;

	if (!is_enabled)
		return;

	mutex_lock(&sdp_boot.lock);

	/* timestamp */
	time = local_clock();
	nsec = do_div(time, NSEC_PER_SEC);
	len = snprintf(buf, sizeof(buf), "[%6lu.%06ld] ",
			(unsigned long)time, nsec / NSEC_PER_USEC);

	/* append log */
	va_start(args, fmt);
	len += vsnprintf(buf + len, MAX_STR_LEN - len, fmt, args);
	va_end(args);

	/* buffer full, reset position */
	if (sdp_boot.buf_pos + len + 1 > AUD_LOG_BUF_SIZE) {
		pr_info("%s: log buffer is full\n", __func__);
		mutex_unlock(&sdp_boot.lock);
		return;
	}
	sdp_boot.buf_pos += scnprintf(&sdp_boot.log_buf[sdp_boot.buf_pos],
			len + 1, "%s", buf);

	mutex_unlock(&sdp_boot.lock);
}
EXPORT_SYMBOL(sdp_boot_print);

static ssize_t sdp_boot_log_read(struct file *file, char __user *buf,
						size_t len, loff_t *offset)
{
	loff_t pos = *offset;
	ssize_t count = 0;
	size_t size = (size_t)sdp_boot.buf_pos;

	mutex_lock(&sdp_boot.lock);

	pr_info("%s: pos(%d),size(%d)\n", __func__,
				sdp_boot.buf_pos, size);

	if (pos >= size) {
		mutex_unlock(&sdp_boot.lock);
		return 0;
	}

	count = min(len, size);

	if ((pos + count) > size)
		count = size - pos;

	if (copy_to_user(buf, sdp_boot.log_buf + pos, count)) {
		mutex_unlock(&sdp_boot.lock);
		return -EFAULT;
	}

	*offset += count;

	pr_info("%s: done\n", __func__);

	mutex_unlock(&sdp_boot.lock);

	return count;
}

static const struct proc_ops sdp_boot_log_ops = {
	.proc_read = sdp_boot_log_read,
};

static int snd_debug_proc_probe(struct platform_device *pdev)
{
	struct proc_dir_entry *entry;

	procfs_sdp_dir = proc_mkdir(PROC_SDP_DIR, NULL);
	if (unlikely(!procfs_sdp_dir)) {
		pr_err("%s: failed to make %s\n", __func__, PROC_SDP_DIR);
		return 0;
	}

	/* proc sdp_info */
	entry = proc_create(SDP_INFO_LOG_NAME, 0444,
			procfs_sdp_dir, &sdp_info_log_ops);
	if (unlikely(!entry))
		pr_err("%s: proc sdp_info log fail\n", __func__);
	else
		proc_set_size(entry, AUD_LOG_BUF_SIZE);

	mutex_init(&sdp_info.lock);

	/* proc sdp_boot */
	entry = proc_create(SDP_BOOT_LOG_NAME, 0444,
			procfs_sdp_dir, &sdp_boot_log_ops);
	if (unlikely(!entry))
		pr_err("%s: proc sdp_boot log fail\n", __func__);
	else
		proc_set_size(entry, AUD_LOG_BUF_SIZE);

	mutex_init(&sdp_boot.lock);

	is_enabled = true;

	pr_info("%s: Enabling samsung snd debug proc\n", __func__);

	return 0;
}

static int snd_debug_proc_remove(struct platform_device *pdev)
{
	is_enabled = false;

	remove_proc_subtree(SDP_INFO_LOG_NAME, procfs_sdp_dir);
	mutex_destroy(&sdp_info.lock);
	remove_proc_subtree(SDP_BOOT_LOG_NAME, procfs_sdp_dir);
	mutex_destroy(&sdp_boot.lock);
	remove_proc_subtree(PROC_SDP_DIR, NULL);

	return 0;
}

#if IS_ENABLED(CONFIG_OF)
static const struct of_device_id snd_debug_proc_of_match[] = {
	{ .compatible = "samsung,snd-debug-proc", },
	{},
};
MODULE_DEVICE_TABLE(of, snd_debug_proc_of_match);
#endif /* CONFIG_OF */

static struct platform_driver snd_debug_proc_driver = {
	.driver		= {
		.name	= "snd-debug-proc",
		.owner	= THIS_MODULE,
		.of_match_table = of_match_ptr(snd_debug_proc_of_match),
	},

	.probe		= snd_debug_proc_probe,
	.remove		= snd_debug_proc_remove,
};

static int __init snd_dedug_proc_init(void)
{
	int ret = 0;

	is_enabled = false;
	ret = platform_driver_register(&snd_debug_proc_driver);
	if (ret)
		pr_err("%s: fail to register driver\n", __func__);

	return ret;
}
module_init(snd_dedug_proc_init);

static void __exit snd_dedug_proc_exit(void)
{
	is_enabled = false;
	platform_driver_unregister(&snd_debug_proc_driver);
}
module_exit(snd_dedug_proc_exit);

MODULE_DESCRIPTION("Samsung Electronics Sound Debug Proc driver");
MODULE_LICENSE("GPL");
