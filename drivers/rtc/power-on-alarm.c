// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 Google, Inc.
 */
/* Tab A8 code for SR-AX6300-01-433 by wenyaqi at 20211108 start */
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
#include <linux/wakelock.h>

struct alarm_timespec {
	char alarm[14];
};

#define ALARM_STR_SIZE                0x0e
#define ANDROID_ALARM_BASE_CMD(cmd)   (cmd & ~(_IOC(0, 0, 0xf0, 0)))
#define ANDROID_ALARM_SET_ALARM_BOOT  _IOW('a', 7, struct timespec)
#define ANDROID_ALARM_SET_ALARM_BOOT_NEW    _IOW('a', 7, struct alarm_timespec)
#define SAPA_START_POLL_TIME          (10LL * NSEC_PER_SEC) /* 10 sec */
#define SAPA_BOOTING_TIME             (5*60)
#define SAPA_POLL_TIME                (15*60)

static int debug_mask = BIT(0);
module_param_named(debug_mask, debug_mask, int, 0664);
static char pwr_on_alarm[] = "0000000000000";
static char const clear_str[] = "0000000000000";
static struct work_struct check_func;
static struct alarm check_poll;
static struct wake_lock wakelock;
static int triggered;

enum {
    SAPA_DISTANT = 0,
    SAPA_NEAR,
    SAPA_EXPIRED,
    SAPA_OVER
};

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
        printk(KERN_DEBUG, "PWR_ON_ALARM:%s (%d:%d)(%lu)\n", __func__,
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
            rtc_time_struct.tm_year, rtc_time_struct.tm_mon, rtc_time_struct.tm_mday,
            rtc_time_struct.tm_hour, rtc_time_struct.tm_min);
    return rtc_time_struct;
}

static int sapa_check_state(unsigned long *data)
{
    unsigned long rtc_secs;
    unsigned long secs_pwron;
    struct rtc_device *alarm_rtc_dev;
    struct rtc_wkalrm alm;
    struct timespec alarm_base_time_sec;
    int res = SAPA_NEAR;

    alarm_get_rtctime_sec(&alarm_base_time_sec);
    rtc_secs = alarm_base_time_sec.tv_sec;
    alarm_rtc_dev = alarmtimer_get_rtcdev();
    rtc_read_alarm(alarm_rtc_dev,&alm);
    rtc_tm_to_time(&alm.time, &secs_pwron);

    if (rtc_secs < secs_pwron) {
        if (secs_pwron - rtc_secs > SAPA_POLL_TIME) {
            res = SAPA_DISTANT;
        }
        if (data) {
            *data = secs_pwron - rtc_secs;
        }
    } else if (rtc_secs <= secs_pwron+SAPA_BOOTING_TIME) {
        res = SAPA_EXPIRED;
        if (data) {
            *data = rtc_secs + 10;
        }
    } else {
        res = SAPA_OVER;
    }

    pr_info("%s: rtc:%lu, alrm:%lu[%d]\n", __func__, rtc_secs, secs_pwron, res);
    return res;
}

static void sapa_check_func(struct work_struct *work)
{
    int res;
    unsigned long remain;

    res = sapa_check_state(&remain);
    if (res <= SAPA_NEAR) {
        ktime_t kt;

        if (res == SAPA_DISTANT) {
            remain = SAPA_POLL_TIME;
        }
        kt = ns_to_ktime((u64)remain * NSEC_PER_SEC);
        alarm_start_relative(&check_poll, kt);
        pr_info("%s: next %lu s\n", __func__, remain);
    } else if (res == SAPA_EXPIRED) {
        wake_lock(&wakelock);
        triggered = 1;
    }
}

static enum alarmtimer_restart
sapa_check_callback(struct alarm *alarm, ktime_t now)
{
    schedule_work(&check_func);
    return ALARMTIMER_NORESTART;
}

int alarm_get_alarm(struct rtc_wkalrm *alm)
{
    alm->enabled = triggered;
    return 1;
}
EXPORT_SYMBOL_GPL(alarm_get_alarm);

static int alarm_set_alarm(char *alarm_data)
{
    struct timespec alarm_delta_sec;
    struct timespec set_time_sec;
    struct timespec base_time_sec;
    struct timespec alarm_base_time_sec;
    char cmd_input[ALARM_STR_SIZE];
    struct rtc_time rtc_tm_set;
    struct rtc_time rtc_tm_base;
    int ret = 0;

    printk(KERN_DEBUG"PWR_ON_ALARM: alarm write!\n");
    strlcpy(cmd_input, alarm_data, ALARM_STR_SIZE);
    cmd_input[13] = '\0';
    switch ((int)cmd_input[0]) {
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
            // SEC has already add timezone, avoid this function
            // base_time_sec.tv_sec -= sys_tz.tz_minuteswest * 60; //add timezone
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

static long alarm_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    int rv = 0;
    char bootalarm_data[14];

    pr_info("sapa: power_on_alarm cmd = %u\n", cmd);

    switch (ANDROID_ALARM_BASE_CMD(cmd)) {
        case ANDROID_ALARM_SET_ALARM_BOOT:
        case ANDROID_ALARM_SET_ALARM_BOOT_NEW:
            if (copy_from_user(bootalarm_data, (void __user *)arg, 14)) {
                rv = -EFAULT;
                return rv;
            }
            rv = alarm_set_alarm(bootalarm_data);
            break;
        default:
            printk(KERN_DEBUG"alarm_ioctl: No such cmd\n");
            break;
    }

    return rv;
}

static bool is_charger_mode;
static int __init get_boot_mode(char *str)
{
    if (!str) {
        return 0;
    }

    if (!strncmp(str, "charger", strlen("charger"))) {
        is_charger_mode = true;
    }

    return 0;
}
__setup("androidboot.mode=", get_boot_mode);

static int alarm_enable = SAPA_OVER;
static void sapa_load_alarm(void)
{
    unsigned long alarm_secs;

    alarm_enable = sapa_check_state(&alarm_secs);
}

static const struct file_operations alarm_fops = {
    .owner = THIS_MODULE,
    .unlocked_ioctl = alarm_ioctl,
    .open = alarm_open,
    .release = alarm_release,
};

static struct miscdevice alarm_device = {
    .minor = MISC_DYNAMIC_MINOR,
    .name = "power_on_alarm",
    .fops = &alarm_fops,
};

static void sapa_init(void)
{
    ktime_t kt;

    triggered = 0;
    if (is_charger_mode && alarm_enable != SAPA_OVER) {
        wake_lock_init(&wakelock, WAKE_LOCK_SUSPEND, "SAPA");
        alarm_init(&check_poll, ALARM_REALTIME, sapa_check_callback);
        INIT_WORK(&check_func, sapa_check_func);
        kt = ns_to_ktime(SAPA_START_POLL_TIME);
        alarm_start_relative(&check_poll, kt);
    }
}

static void sapa_exit(void)
{
    if (is_charger_mode && alarm_enable != SAPA_OVER) {
        cancel_work_sync(&check_func);
        alarm_cancel(&check_poll);
        wake_lock_destroy(&wakelock);
    }
}

static int __init alarm_dev_init(void)
{
    int err;
    err = misc_register(&alarm_device);
    if (err) {
        return err;
    }
    sapa_load_alarm();
    sapa_init();
    return 0;
}

static void __exit alarm_dev_exit(void)
{
    sapa_exit();
    misc_deregister(&alarm_device);
}

module_init(alarm_dev_init);
module_exit(alarm_dev_exit);
MODULE_LICENSE("GPL v2");

/* Tab A8 code for SR-AX6300-01-433 by wenyaqi at 20211108 end */
