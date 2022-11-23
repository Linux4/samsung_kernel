// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 Google, Inc.
 */
/* HS03 code for SR-SL6215-01-558 by shixuanxuan at 20210814 start */
#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt
#include <linux/time.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/platform_device.h>
#include <linux/sched.h>
#include <linux/uaccess.h>
#include <linux/rtc.h>
#include <linux/alarmtimer.h>
#include <linux/string.h>

#define ALARM_STR_SIZE 0x0e
static int debug_mask = BIT(0);
module_param_named(debug_mask, debug_mask, int, 0664);
static char pwr_on_alarm[] = "0000000000000";
static char const clear_str[] = "0000000000000";
void alarm_set_power_on(struct timespec new_pwron_time, bool logo)
{
	unsigned long pwron_time;
	struct rtc_wkalrm alm;
	struct rtc_device *alarm_rtc_dev;
	printk(KERN_DEBUG"PWR_ON_ALARM: alarm set power on\n");

#ifdef RTC_PWRON_SEC
	/* round down the second */
	new_pwron_time.tv_sec = (new_pwron_time.tv_sec / 60) * 60;
#endif
	if (new_pwron_time.tv_sec > 0) {
		pwron_time = new_pwron_time.tv_sec;
#ifdef RTC_PWRON_SEC
		pwron_time += RTC_PWRON_SEC;
#endif
		alm.enabled = (logo ? 3 : 2);
	} else {
		pwron_time = 0;
		alm.enabled = 4;
	}
	alarm_rtc_dev = alarmtimer_get_rtcdev();
	rtc_time_to_tm(pwron_time, &alm.time);
	printk(KERN_DEBUG"PWR_ON_ALARM: rtc time %02d:%02d:%02d %02d/%02d/%04d\n",
		  alm.time.tm_hour, alm.time.tm_min,
		  alm.time.tm_sec, alm.time.tm_mon + 1,
		  alm.time.tm_mday, alm.time.tm_year + 1900);
	rtc_set_alarm(alarm_rtc_dev, &alm);
}

void alarm_get_rtctime_sec(struct timespec *rtc_time_now_sec)
{
	struct rtc_device *alarm_rtc_dev;
	struct rtc_time tmm = {0};

	alarm_rtc_dev = alarmtimer_get_rtcdev();
	rtc_read_time(alarm_rtc_dev,&tmm);
	printk(KERN_DEBUG"PWR_ON_ALARM: rtc alarm time now %02d:%02d:%02d %02d/%02d/%04d\n",
		  tmm.tm_hour, tmm.tm_min,
		  tmm.tm_sec, tmm.tm_mon + 1,
		  tmm.tm_mday, tmm.tm_year + 1900);
	rtc_tm_to_time(&tmm, &(rtc_time_now_sec->tv_sec));
	printk(KERN_DEBUG"PWR_ON_ALARM: rtc alarm time now-> %ld\n",rtc_time_now_sec->tv_sec);
}

static int alarm_open(struct inode *inode, struct file *file)
{
	file->private_data = NULL;
	printk(KERN_DEBUG, "%s (%d:%d)\n", __func__, current->tgid, current->pid);
	return 0;
}

static int alarm_release(struct inode *inode, struct file *file)
{
	if (file->private_data) {
	printk(KERN_DEBUG,"PWR_ON_ALARM:%s (%d:%d)(%lu)\n", __func__,
		  current->tgid, current->pid, (uintptr_t)file->private_data);
	}
	return 0;
}

static struct rtc_time alarm_str2time(char *str, int length)
{
	int time_int[12];
	int countNum = 0;
	int year = 0;
	int month = 0;
	int day = 0;
	int hour = 0;
	int minute = 0;
	struct rtc_time rtc_time_struct = {0};

	if (strlen(str) > length){
		printk(KERN_DEBUG"PWR_ON_ALARM: overlength strlen=%d,len=%d\n",strlen(str),length);
		return rtc_time_struct;
	}

	for (countNum = 1; countNum < 13; countNum++){
		if ((str[countNum] > '0') && (str[countNum] <='9')){
			time_int[countNum-1] = (int)(str[countNum] -'0');
		} else {
			time_int[countNum-1] = 0;
		}
	}
	// Get Year
	year += time_int[0] * 1000;
	year += time_int[1] * 100;
	year += time_int[2] * 10;
	year += time_int[3];
	// Get month
	month += time_int[4] * 10;
	month += time_int[5];
	// Get day
	day += time_int[6] * 10;
	day += time_int[7];
	// Get hour
	hour += time_int[8] * 10;
	hour += time_int[9];
	// Get minute
	minute += time_int[10] * 10;
	minute += time_int[11];

	rtc_time_struct.tm_year = year-1900;
	rtc_time_struct.tm_mon  = month-1;
	rtc_time_struct.tm_mday = day;
	rtc_time_struct.tm_hour = hour;
	rtc_time_struct.tm_min  = minute;
	rtc_time_struct.tm_sec  = 0;
	printk(KERN_DEBUG"PWR_ON_ALARM: ,year = %d,month = %d,day = %d,hour = %d,minute = %d\n",\
			rtc_time_struct.tm_year, rtc_time_struct.tm_mon, rtc_time_struct.tm_mday, rtc_time_struct.tm_hour, rtc_time_struct.tm_min);
	return rtc_time_struct;
}

static ssize_t alarm_read(struct file *filp, char __user *buf, size_t count, loff_t *ppos)
{
	unsigned long p = *ppos;
	int ret = 0;
	struct rtc_wkalrm alm;
	struct rtc_device *alarm_rtc_dev;

	if (p >= ALARM_STR_SIZE){
		return 0;
	}

	if (count > (ALARM_STR_SIZE - p)){
		count = ALARM_STR_SIZE - p;
	}

    if (copy_to_user(buf, pwr_on_alarm, count)){
        ret = -EFAULT;
    } else {
		*ppos += count;
		ret = count;
	}
	alarm_rtc_dev = alarmtimer_get_rtcdev();
	rtc_read_alarm(alarm_rtc_dev,&alm);
	printk(KERN_DEBUG"PWR_ON_ALARM: alarm is set as %02d:%02d:%02d %02d/%02d/%04d\n",
		  alm.time.tm_hour, alm.time.tm_min,
		  alm.time.tm_sec, alm.time.tm_mon + 1,
		  alm.time.tm_mday, alm.time.tm_year + 1900);
    printk(KERN_DEBUG"PWR_ON_ALARM:alarm read!\n");
	return ret;
}

static ssize_t alarm_write(struct file *filp, const char __user *buf, size_t count, loff_t *ppos)
{
	unsigned long p = *ppos;
	struct timespec alarm_delta_sec;
	struct timespec set_time_sec;
	struct timespec base_time_sec;
	struct timespec alarm_base_time_sec;
    char cmd_input[ALARM_STR_SIZE];
	struct rtc_time rtc_tm_set;
	struct rtc_time rtc_tm_base;
	int ret = 0;

	printk(KERN_DEBUG"PWR_ON_ALARM: alarm write!\n");
	if (p >= ALARM_STR_SIZE){
		return 0;
	}

	if (count > (ALARM_STR_SIZE - p)){
		count = ALARM_STR_SIZE - p;
	}

	if (copy_from_user(cmd_input, buf, count)){
        ret = -EFAULT;
    } else {
		*ppos += count;
		ret = count;
	}
	cmd_input[13] = '\0';
	switch ((int)cmd_input[0])
	{
	case '0':
		set_time_sec.tv_sec = 0;
		alarm_set_power_on(set_time_sec, false);
		printk(KERN_DEBUG"PWR_ON_ALARM: Disable power on alarm\n");
		strncpy(pwr_on_alarm,clear_str,strlen(pwr_on_alarm));
		break;
	case '1':
		//get alarm
		rtc_tm_set = alarm_str2time(cmd_input, ALARM_STR_SIZE);
		printk(KERN_DEBUG"PWR_ON_ALARM: set_value %02d:%02d:%02d %02d/%02d/%04d\n",
		  rtc_tm_set.tm_hour, rtc_tm_set.tm_min,
		  rtc_tm_set.tm_sec, rtc_tm_set.tm_mon + 1,
		  rtc_tm_set.tm_mday, rtc_tm_set.tm_year + 1900);
		rtc_tm_to_time(&rtc_tm_set, &set_time_sec.tv_sec);
		//get ktime
		base_time_sec.tv_sec = ktime_get_real_seconds();
		base_time_sec.tv_sec -= sys_tz.tz_minuteswest * 60; //add timezone
		rtc_time_to_tm(base_time_sec.tv_sec, &rtc_tm_base);
		printk(KERN_DEBUG"PWR_ON_ALARM: rtc_base+timezone %02d:%02d:%02d %02d/%02d/%04d\n",
		  rtc_tm_base.tm_hour, rtc_tm_base.tm_min,
		  rtc_tm_base.tm_sec, rtc_tm_base.tm_mon + 1,
		  rtc_tm_base.tm_mday, rtc_tm_base.tm_year + 1900);
		//get delta
		alarm_delta_sec.tv_sec = set_time_sec.tv_sec - base_time_sec.tv_sec;
		if (alarm_delta_sec.tv_sec < 0){
			strncpy(pwr_on_alarm,clear_str,strlen(pwr_on_alarm));
			printk(KERN_ERR"PWR_ON_ALARM: alarm set less than ktime.\n");
			break;
		}
		//get rtc now
		alarm_get_rtctime_sec(&alarm_base_time_sec);
		alarm_delta_sec.tv_sec += alarm_base_time_sec.tv_sec;
		//set alarm
		alarm_set_power_on(alarm_delta_sec,false);
		printk(KERN_DEBUG"PWR_ON_ALARM: alarm set-> %s\n",cmd_input);
		break;

	default:
		set_time_sec.tv_sec = 0;
		alarm_set_power_on(set_time_sec, false);
		printk(KERN_DEBUG"PWR_ON_ALARM: No such cmd\n");
		strncpy(pwr_on_alarm,clear_str,strlen(pwr_on_alarm));
		break;
	}
	strncpy(pwr_on_alarm,cmd_input,strlen(pwr_on_alarm));
	return ret;
}

static const struct file_operations alarm_fops = {
	.owner = THIS_MODULE,
	.read = alarm_read,
	.write = alarm_write,
	.open = alarm_open,
	.release = alarm_release,
};

static struct miscdevice alarm_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "power_on_alarm",
	.fops = &alarm_fops,
};

static int __init alarm_dev_init(void)
{
	int err;
	err = misc_register(&alarm_device);
	if (err)
		return err;
	return 0;
}

static void __exit alarm_dev_exit(void)
{
	misc_deregister(&alarm_device);
}

module_init(alarm_dev_init);
module_exit(alarm_dev_exit);
MODULE_LICENSE("GPL v2");

/* HS03 code for SR-SL6215-01-558 by shixuanxuan at 20210814 end */