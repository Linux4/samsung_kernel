/* drivers/rtc/alarm-poweroff.c
 *
 * Copyright (C) 2007-2009 Google, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <asm/mach/time.h>
#include <linux/device.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/platform_device.h>
#include <linux/sched.h>
#include <linux/spinlock.h>
#include <linux/uaccess.h>
#include <linux/wakelock.h>
#include <linux/module.h>
#include "android_alarm.h"

static unsigned long power_up_alarm_time;
static int rtc_power_up_flag;

static ssize_t alarm_read(struct file *file, char __user *buf, size_t count,
			loff_t *ppos)
{
	int len = 0;

	if (rtc_power_up_flag) {
		len += sprintf(buf+len, "%lu\n", power_up_alarm_time);
		printk(KERN_INFO"%s,%s,%d\n", __func__, buf, len);
	}
	return len;

}

static int alarm_open(struct inode *inode, struct file *file)
{
	file->private_data = NULL;
	return 0;
}

static int alarm_release(struct inode *inode, struct file *file)
{
	return 0;
}

static const struct file_operations alarm_fops = {
	.owner = THIS_MODULE,
	.open = alarm_open,
	.read = alarm_read,
	.release = alarm_release,
};

static struct miscdevice alarm_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "alarm-poweroff",
	.fops = &alarm_fops,
};

static int __init alarm_dev_init(void)
{
	int err;

	err = misc_register(&alarm_device);
	if (err)
		return err;
	alarm_read_rtc_ring(&rtc_power_up_flag, &power_up_alarm_time);
	printk(KERN_INFO" %s, rtc power up : %d, alarm time: %lu\n", __func__,
		rtc_power_up_flag, power_up_alarm_time);
	return 0;
}

static void  __exit alarm_dev_exit(void)
{
	misc_deregister(&alarm_device);
}

late_initcall(alarm_dev_init);
module_exit(alarm_dev_exit);
