/* drivers/rtc/alarm-dev.c
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

#include <linux/time.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/platform_device.h>
#include <linux/sched.h>
#include <linux/spinlock.h>
#include <linux/uaccess.h>
#include <linux/alarmtimer.h>
#include "android_alarm.h"

#define ANDROID_ALARM_PRINT_INFO (1U << 0)
#define ANDROID_ALARM_PRINT_IO (1U << 1)
#define ANDROID_ALARM_PRINT_INT (1U << 2)

static int debug_mask = ANDROID_ALARM_PRINT_INFO;
module_param_named(debug_mask, debug_mask, int, S_IRUGO | S_IWUSR | S_IWGRP);

#define alarm_dbg(debug_level_mask, fmt, ...)				\
do {									\
	if (debug_mask & ANDROID_ALARM_PRINT_##debug_level_mask)	\
		pr_info(fmt, ##__VA_ARGS__);				\
} while (0)

#define ANDROID_ALARM_WAKEUP_MASK ( \
	ANDROID_ALARM_RTC_WAKEUP_MASK | \
	ANDROID_ALARM_ELAPSED_REALTIME_WAKEUP_MASK | \
	ANDROID_ALARM_RTC_POWEROFF_WAKEUP_MASK)

static int alarm_opened;
static DEFINE_SPINLOCK(alarm_slock);
static struct wakeup_source alarm_wake_lock;
static DECLARE_WAIT_QUEUE_HEAD(alarm_wait_queue);
static uint32_t alarm_pending;
static uint32_t alarm_enabled;
static uint32_t wait_pending;

struct devalarm {
	union {
		struct hrtimer hrt;
		struct alarm alrm;
	} u;
	enum android_alarm_type type;
};

static struct devalarm alarms[ANDROID_ALARM_TYPE_COUNT];


static int is_wakeup(enum android_alarm_type type)
{
	return (type == ANDROID_ALARM_RTC_WAKEUP ||
		type == ANDROID_ALARM_ELAPSED_REALTIME_WAKEUP ||
		type == ANDROID_ALARM_RTC_POWEROFF_WAKEUP);
}


static void devalarm_start(struct devalarm *alrm, ktime_t exp)
{
	if (is_wakeup(alrm->type))
		alarm_start(&alrm->u.alrm, exp);
	else
		hrtimer_start(&alrm->u.hrt, exp, HRTIMER_MODE_ABS);
}


static int devalarm_try_to_cancel(struct devalarm *alrm)
{
	if (is_wakeup(alrm->type))
		return alarm_try_to_cancel(&alrm->u.alrm);
	return hrtimer_try_to_cancel(&alrm->u.hrt);
}

static void devalarm_cancel(struct devalarm *alrm)
{
	if (is_wakeup(alrm->type))
		alarm_cancel(&alrm->u.alrm);
	else
		hrtimer_cancel(&alrm->u.hrt);
}

static void alarm_clear(enum android_alarm_type alarm_type, struct timespec *ts)
{
	uint32_t alarm_type_mask = 1U << alarm_type;
	unsigned long flags;

	spin_lock_irqsave(&alarm_slock, flags);
	alarm_dbg(IO, "alarm %d clear\n", alarm_type);
	devalarm_try_to_cancel(&alarms[alarm_type]);
	if (alarm_pending) {
		alarm_pending &= ~alarm_type_mask;
		if (!alarm_pending && !wait_pending)
			__pm_relax(&alarm_wake_lock);
	}
	alarm_enabled &= ~alarm_type_mask;
	spin_unlock_irqrestore(&alarm_slock, flags);

	if (alarm_type == ANDROID_ALARM_RTC_POWEROFF_WAKEUP)
		set_power_on_alarm(ts->tv_sec, 0);
}

static void alarm_set(enum android_alarm_type alarm_type,
							struct timespec *ts)
{
	uint32_t alarm_type_mask = 1U << alarm_type;
	unsigned long flags;

	spin_lock_irqsave(&alarm_slock, flags);
	alarm_dbg(IO, "alarm %d set %ld.%09ld\n",
			alarm_type, ts->tv_sec, ts->tv_nsec);
	alarm_enabled |= alarm_type_mask;
	devalarm_start(&alarms[alarm_type], timespec_to_ktime(*ts));
	spin_unlock_irqrestore(&alarm_slock, flags);

	if (alarm_type == ANDROID_ALARM_RTC_POWEROFF_WAKEUP)
		set_power_on_alarm(ts->tv_sec, 1);
}

static int alarm_wait(void)
{
	unsigned long flags;
	int rv = 0;

	spin_lock_irqsave(&alarm_slock, flags);
	alarm_dbg(IO, "alarm wait\n");
	if (!alarm_pending && wait_pending) {
		__pm_relax(&alarm_wake_lock);
		wait_pending = 0;
	}
	spin_unlock_irqrestore(&alarm_slock, flags);

	rv = wait_event_interruptible(alarm_wait_queue, alarm_pending);
	if (rv)
		return rv;

	spin_lock_irqsave(&alarm_slock, flags);
	rv = alarm_pending;
	wait_pending = 1;
	alarm_pending = 0;
	spin_unlock_irqrestore(&alarm_slock, flags);

	return rv;
}

static int alarm_set_rtc(struct timespec *ts)
{
	struct rtc_time new_rtc_tm;
	struct rtc_device *rtc_dev;
	unsigned long flags;
	int rv = 0;

	rtc_time_to_tm(ts->tv_sec, &new_rtc_tm);

	rtc_dev = alarmtimer_get_rtcdev();
	rv = do_settimeofday(ts);
	if (rv < 0)
		return rv;
	if (rtc_dev)
		rv = rtc_set_time(rtc_dev, &new_rtc_tm);

	spin_lock_irqsave(&alarm_slock, flags);
	alarm_pending |= ANDROID_ALARM_TIME_CHANGE_MASK;
	wake_up(&alarm_wait_queue);
	spin_unlock_irqrestore(&alarm_slock, flags);

	return rv;
}

static int alarm_get_time(enum android_alarm_type alarm_type,
							struct timespec *ts)
{
	int rv = 0;

	switch (alarm_type) {
	case ANDROID_ALARM_RTC_WAKEUP:
	case ANDROID_ALARM_RTC:
	case ANDROID_ALARM_RTC_POWEROFF_WAKEUP:
		getnstimeofday(ts);
		break;
	case ANDROID_ALARM_ELAPSED_REALTIME_WAKEUP:
	case ANDROID_ALARM_ELAPSED_REALTIME:
		get_monotonic_boottime(ts);
		break;
	case ANDROID_ALARM_SYSTEMTIME:
		ktime_get_ts(ts);
		break;
	default:
		rv = -EINVAL;
	}
	return rv;
}

#ifdef CONFIG_RTC_AUTO_PWRON
extern int rtc_set_bootalarm(struct rtc_device *rtc, struct rtc_wkalrm *alarm);
/*
 *  0|1234|56|78|90|12
 *  1|2010|01|01|00|00
 * en yyyy mm dd hh mm
*/
#define BOOTALM_BIT_EN		0
#define BOOTALM_BIT_YEAR	1
#define BOOTALM_BIT_MONTH	5
#define BOOTALM_BIT_DAY		7
#define BOOTALM_BIT_HOUR	9
#define BOOTALM_BIT_MIN		11
#define BOOTALM_BIT_TOTAL	13

int alarm_set_alarm(char* alarm_data)
{
	struct rtc_wkalrm alm;
	int ret;
	char buf_ptr[BOOTALM_BIT_TOTAL+1];
	struct rtc_time rtc_tm;
	unsigned long rtc_sec;
	unsigned long rtc_alarm_time;
	struct timespec rtc_delta;
	struct timespec wall_time;
	ktime_t wall_ktm;
	struct rtc_time wall_tm;
	struct rtc_device *rtc_dev = alarmtimer_get_rtcdev();

	if (!rtc_dev) {
		pr_err("%s: no RTC, time will be lost on reboot\n", __func__);
		return -ENXIO;
	}

	strlcpy(buf_ptr, alarm_data, BOOTALM_BIT_TOTAL+1);

	alm.time.tm_sec = 0;
	alm.time.tm_min = (buf_ptr[BOOTALM_BIT_MIN] - '0') * 10
				+ (buf_ptr[BOOTALM_BIT_MIN+1]  - '0');
	alm.time.tm_hour = (buf_ptr[BOOTALM_BIT_HOUR] - '0') * 10
				+ (buf_ptr[BOOTALM_BIT_HOUR+1] - '0');
	alm.time.tm_mday = (buf_ptr[BOOTALM_BIT_DAY] - '0') * 10
				+ (buf_ptr[BOOTALM_BIT_DAY+1] - '0');
	alm.time.tm_mon = (buf_ptr[BOOTALM_BIT_MONTH] - '0') * 10
				+ (buf_ptr[BOOTALM_BIT_MONTH+1] - '0');
	alm.time.tm_year = (buf_ptr[BOOTALM_BIT_YEAR] - '0') * 1000
				+ (buf_ptr[BOOTALM_BIT_YEAR+1] - '0') * 100
				+ (buf_ptr[BOOTALM_BIT_YEAR+2] - '0') * 10
				+ (buf_ptr[BOOTALM_BIT_YEAR+3] - '0');

	alm.enabled = (*buf_ptr == '1');

	pr_info("[SAPA] %s : %s => tm(%d %04d-%02d-%02d %02d:%02d:%02d)\n",
			__func__, buf_ptr, alm.enabled,
			alm.time.tm_year, alm.time.tm_mon, alm.time.tm_mday,
			alm.time.tm_hour, alm.time.tm_min, alm.time.tm_sec);

	if (alm.enabled) {
		/* If time daemon is exist */

		alm.time.tm_mon -= 1;
		alm.time.tm_year -= 1900;

		/* read current time */
		rtc_read_time(rtc_dev, &rtc_tm);
		rtc_tm_to_time(&rtc_tm, &rtc_sec);
		pr_info("[SAPA] rtc  %4d-%02d-%02d %02d:%02d:%02d -> %lu\n",
			rtc_tm.tm_year, rtc_tm.tm_mon, rtc_tm.tm_mday,
			rtc_tm.tm_hour, rtc_tm.tm_min, rtc_tm.tm_sec, rtc_sec);

		/* read kernel time */
		getnstimeofday(&wall_time);
		wall_ktm = timespec_to_ktime(wall_time);
		wall_tm = rtc_ktime_to_tm(wall_ktm);
		pr_info("[SAPA] wall %4d-%02d-%02d %02d:%02d:%02d -> %lu\n",
			wall_tm.tm_year, wall_tm.tm_mon, wall_tm.tm_mday,
			wall_tm.tm_hour, wall_tm.tm_min, wall_tm.tm_sec, wall_time.tv_sec);

		/* calculate offset */
		set_normalized_timespec(&rtc_delta,
					wall_time.tv_sec - rtc_sec,
					wall_time.tv_nsec);

		/* convert user requested SAPA time to second type */
		rtc_tm_to_time(&alm.time, &rtc_alarm_time);

		/* convert to RTC time with user requested SAPA time and offset */
		rtc_alarm_time -= rtc_delta.tv_sec;
		rtc_time_to_tm(rtc_alarm_time, &alm.time);
		pr_info("[SAPA] arlm %4d-%02d-%02d %02d:%02d:%02d -> %lu\n",
			alm.time.tm_year, alm.time.tm_mon, alm.time.tm_mday,
			alm.time.tm_hour, alm.time.tm_min, alm.time.tm_sec, rtc_alarm_time);

	}
	ret = rtc_set_bootalarm(rtc_dev, &alm);
	if (ret < 0) {
		pr_err("%s: Failed to set ALARM, time will be lost on reboot\n", __func__);
		return ret;
	}

	return 0;
}
#endif

static long alarm_do_ioctl(struct file *file, unsigned int cmd,
							struct timespec *ts)
{
	int rv = 0;
	unsigned long flags;
	enum android_alarm_type alarm_type = ANDROID_ALARM_IOCTL_TO_TYPE(cmd);

	if (alarm_type >= ANDROID_ALARM_TYPE_COUNT)
		return -EINVAL;

	if (ANDROID_ALARM_BASE_CMD(cmd) != ANDROID_ALARM_GET_TIME(0)) {
		if ((file->f_flags & O_ACCMODE) == O_RDONLY)
			return -EPERM;
		if (file->private_data == NULL &&
		    cmd != ANDROID_ALARM_SET_RTC) {
			spin_lock_irqsave(&alarm_slock, flags);
			if (alarm_opened) {
				spin_unlock_irqrestore(&alarm_slock, flags);
				return -EBUSY;
			}
			alarm_opened = 1;
			file->private_data = (void *)1;
			spin_unlock_irqrestore(&alarm_slock, flags);
		}
	}

	switch (ANDROID_ALARM_BASE_CMD(cmd)) {
	case ANDROID_ALARM_CLEAR(0):
		alarm_clear(alarm_type, ts);
		break;
	case ANDROID_ALARM_SET(0):
		alarm_set(alarm_type, ts);
		break;
	case ANDROID_ALARM_SET_AND_WAIT(0):
		alarm_set(alarm_type, ts);
		/* fall though */
	case ANDROID_ALARM_WAIT:
		rv = alarm_wait();
		break;
	case ANDROID_ALARM_SET_RTC:
		rv = alarm_set_rtc(ts);
		break;
	case ANDROID_ALARM_GET_TIME(0):
		rv = alarm_get_time(alarm_type, ts);
		break;

	default:
		rv = -EINVAL;
	}
	return rv;
}

static long alarm_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{

	struct timespec ts;
	int rv;
#ifdef CONFIG_RTC_AUTO_PWRON
	char bootalarm_data[14];
#endif

	switch (ANDROID_ALARM_BASE_CMD(cmd)) {
	case ANDROID_ALARM_SET_AND_WAIT(0):
	case ANDROID_ALARM_SET(0):
	case ANDROID_ALARM_SET_RTC:
	case ANDROID_ALARM_CLEAR(0):
		if (copy_from_user(&ts, (void __user *)arg, sizeof(ts)))
			return -EFAULT;
		break;
#ifdef CONFIG_RTC_AUTO_PWRON
	case ANDROID_ALARM_SET_ALARM:
		pr_info("[SAPA] %s\n", __func__);
		if (copy_from_user(bootalarm_data, (void __user *)arg, 14))
			rv = -EFAULT;
		rv = alarm_set_alarm(bootalarm_data);
		break;
#endif
	}

	rv = alarm_do_ioctl(file, cmd, &ts);
	if (rv)
		return rv;

	switch (ANDROID_ALARM_BASE_CMD(cmd)) {
	case ANDROID_ALARM_GET_TIME(0):
		if (copy_to_user((void __user *)arg, &ts, sizeof(ts)))
			return -EFAULT;
		break;
	}

	return 0;
}
#ifdef CONFIG_COMPAT
static long alarm_compat_ioctl(struct file *file, unsigned int cmd,
							unsigned long arg)
{

	struct timespec ts;
	int rv;

	switch (ANDROID_ALARM_BASE_CMD(cmd)) {
	case ANDROID_ALARM_SET_AND_WAIT_COMPAT(0):
	case ANDROID_ALARM_SET_COMPAT(0):
	case ANDROID_ALARM_SET_RTC_COMPAT:
		if (compat_get_timespec(&ts, (void __user *)arg))
			return -EFAULT;
		/* fall through */
	case ANDROID_ALARM_GET_TIME_COMPAT(0):
		cmd = ANDROID_ALARM_COMPAT_TO_NORM(cmd);
		break;
	}

	rv = alarm_do_ioctl(file, cmd, &ts);
	if (rv)
		return rv;

	switch (ANDROID_ALARM_BASE_CMD(cmd)) {
	case ANDROID_ALARM_GET_TIME(0): /* NOTE: we modified cmd above */
		if (compat_put_timespec(&ts, (void __user *)arg))
			return -EFAULT;
		break;
	}

	return 0;
}
#endif

static int alarm_open(struct inode *inode, struct file *file)
{
	file->private_data = NULL;
	return 0;
}

static int alarm_release(struct inode *inode, struct file *file)
{
	int i;
	unsigned long flags;

	spin_lock_irqsave(&alarm_slock, flags);
	if (file->private_data) {
		for (i = 0; i < ANDROID_ALARM_TYPE_COUNT; i++) {
			uint32_t alarm_type_mask = 1U << i;
			if (alarm_enabled & alarm_type_mask) {
				alarm_dbg(INFO,
					  "%s: clear alarm, pending %d\n",
					  __func__,
					  !!(alarm_pending & alarm_type_mask));
				alarm_enabled &= ~alarm_type_mask;
			}
			spin_unlock_irqrestore(&alarm_slock, flags);
			devalarm_cancel(&alarms[i]);
			spin_lock_irqsave(&alarm_slock, flags);
		}
		if (alarm_pending | wait_pending) {
			if (alarm_pending)
				alarm_dbg(INFO, "%s: clear pending alarms %x\n",
					  __func__, alarm_pending);
			__pm_relax(&alarm_wake_lock);
			wait_pending = 0;
			alarm_pending = 0;
		}
		alarm_opened = 0;
	}
	spin_unlock_irqrestore(&alarm_slock, flags);
	return 0;
}

static void devalarm_triggered(struct devalarm *alarm)
{
	unsigned long flags;
	uint32_t alarm_type_mask = 1U << alarm->type;

	alarm_dbg(INT, "%s: type %d\n", __func__, alarm->type);
	spin_lock_irqsave(&alarm_slock, flags);
	if (alarm_enabled & alarm_type_mask) {
		__pm_wakeup_event(&alarm_wake_lock, 5000); /* 5secs */
		alarm_enabled &= ~alarm_type_mask;
		alarm_pending |= alarm_type_mask;
		wake_up(&alarm_wait_queue);
	}
	spin_unlock_irqrestore(&alarm_slock, flags);
}


static enum hrtimer_restart devalarm_hrthandler(struct hrtimer *hrt)
{
	struct devalarm *devalrm = container_of(hrt, struct devalarm, u.hrt);

	devalarm_triggered(devalrm);
	return HRTIMER_NORESTART;
}

static enum alarmtimer_restart devalarm_alarmhandler(struct alarm *alrm,
							ktime_t now)
{
	struct devalarm *devalrm = container_of(alrm, struct devalarm, u.alrm);

	devalarm_triggered(devalrm);
	return ALARMTIMER_NORESTART;
}


static const struct file_operations alarm_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = alarm_ioctl,
	.open = alarm_open,
	.release = alarm_release,
#ifdef CONFIG_COMPAT
	.compat_ioctl = alarm_compat_ioctl,
#endif
};

static struct miscdevice alarm_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "alarm",
	.fops = &alarm_fops,
};

static int __init alarm_dev_init(void)
{
	int err;
	int i;

	err = misc_register(&alarm_device);
	if (err)
		return err;

	alarm_init(&alarms[ANDROID_ALARM_RTC_WAKEUP].u.alrm,
			ALARM_REALTIME, devalarm_alarmhandler);
	hrtimer_init(&alarms[ANDROID_ALARM_RTC].u.hrt,
			CLOCK_REALTIME, HRTIMER_MODE_ABS);
	alarm_init(&alarms[ANDROID_ALARM_ELAPSED_REALTIME_WAKEUP].u.alrm,
			ALARM_BOOTTIME, devalarm_alarmhandler);
	hrtimer_init(&alarms[ANDROID_ALARM_ELAPSED_REALTIME].u.hrt,
			CLOCK_BOOTTIME, HRTIMER_MODE_ABS);
	hrtimer_init(&alarms[ANDROID_ALARM_SYSTEMTIME].u.hrt,
			CLOCK_MONOTONIC, HRTIMER_MODE_ABS);
	alarm_init(&alarms[ANDROID_ALARM_RTC_POWEROFF_WAKEUP].u.alrm,
			ALARM_REALTIME, devalarm_alarmhandler);

	for (i = 0; i < ANDROID_ALARM_TYPE_COUNT; i++) {
		alarms[i].type = i;
		if (!is_wakeup(i))
			alarms[i].u.hrt.function = devalarm_hrthandler;
	}

	wakeup_source_init(&alarm_wake_lock, "alarm");
	return 0;
}

static void  __exit alarm_dev_exit(void)
{
	misc_deregister(&alarm_device);
	wakeup_source_trash(&alarm_wake_lock);
}

module_init(alarm_dev_init);
module_exit(alarm_dev_exit);

