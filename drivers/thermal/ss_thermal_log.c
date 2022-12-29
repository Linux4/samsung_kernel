// SPDX-License-Identifier: GPL-2.0
/*
 *  ss_thermal_log.c - SAMSUNG Thermal logging.
 *
 */

#include <linux/module.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/thermal.h>
#include <linux/proc_fs.h>
#include <linux/time.h>
#include <linux/sched/clock.h>

/* TODO: move to general 'sec_pm_log' driver?? */
#define PROC_PM_DIR		"sec_pm_log"
static struct proc_dir_entry *procfs_power_dir;

/* thermal log */
#define THERMAL_LOG_NAME	"thermal_log"
#define	THERM_BUF_SIZE	SZ_128K
#define MAX_STR_LEN	120

static bool init_done __read_mostly = false;
static char thermal_log_buf[THERM_BUF_SIZE];
static unsigned int buf_pos;
static unsigned int buf_full;

void ss_thermal_print(const char *fmt, ...)
{
	char buf[MAX_STR_LEN] = {0, };
	unsigned int len = 0;
	va_list args;
	u64 time;
	unsigned long nsec;

	if (!init_done)
		return;

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
	if (buf_pos + len + 1 > THERM_BUF_SIZE) {
		buf_pos = 0;
		buf_full++;
	}

	buf_pos += scnprintf(&thermal_log_buf[buf_pos], len + 1, "%s", buf);
}

static ssize_t ss_thermal_log_read(struct file *file, char __user *buf,
						size_t len, loff_t *offset)
{
	loff_t pos = *offset;
	ssize_t count = 0;
	size_t size = (buf_full > 0) ? THERM_BUF_SIZE : (size_t)buf_pos;

	pr_info("%s: pos(%d), full(%d), size(%d)\n", __func__,
					buf_pos, buf_full, size);

	if (pos >= size)
		return 0;

	count = min(len, size);

	if ((pos + count) > size)
		count = size - pos;

	if (copy_to_user(buf, thermal_log_buf + pos, count))
		return -EFAULT;

	*offset += count;

	pr_info("%s: done\n", __func__);

	return count;
}


static const struct file_operations proc_ss_thermal_log_ops = {
	.owner 		= THIS_MODULE,
	.read		= ss_thermal_log_read,
};

void ss_thermal_log_init(void)
{
	struct proc_dir_entry *entry;
	pr_info("%s\n", __func__);

	procfs_power_dir = proc_mkdir(PROC_PM_DIR, NULL);
	if (unlikely(!procfs_power_dir))
		return;

	entry = proc_create(THERMAL_LOG_NAME, 0444,
				procfs_power_dir,
				&proc_ss_thermal_log_ops);
	if (unlikely(!entry))
		return;

	proc_set_size(entry, THERM_BUF_SIZE);

	init_done = true;

	ss_thermal_print("%s done\n", __func__);

	pr_info("%s: done\n", __func__);
}
