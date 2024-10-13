// SPDX-License-Identifier: GPL-2.0
/*
 * RTC subsystem, gxy power-on-alarm interface
 *
 * Copyright (C) 2005 Tower Technologies
 */

#ifdef pr_fmt
#undef pr_fmt
#endif //pr_fmt
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt
#include <linux/rtc.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/platform_device.h>
#include <linux/sched.h>
#include <linux/uaccess.h>
#include <linux/alarmtimer.h>
#include <linux/string.h>
#include <linux/time.h>
#include <linux/time64.h>
#include <linux/of.h>
#include <linux/of_fdt.h>
#include <linux/of_platform.h>
#include <linux/timekeeping.h>
#include <linux/ktime.h>

/* log debug */
#define GXY_TAG    "[GXY_ALARM]"

#define GXY_DEBUG 1
#ifdef GXY_DEBUG
    #define GXY_INFO(fmt, args...)    pr_info(GXY_TAG fmt, ##args)
#else
    #define GXY_INFO(fmt, args...)
#endif //GXY_DEBUG

#define GXY_ERR(fmt, args...)    pr_err(GXY_TAG fmt, ##args)

struct gxy_tag_bootmode {
    u32 size;
    u32 tag;
    u32 bootmode;
    u32 boottype;
};

enum gxy_boot_mode_t {
    NORMAL_BOOT = 0,
    META_BOOT = 1,
    RECOVERY_BOOT = 2,
    SW_REBOOT = 3,
    FACTORY_BOOT = 4,
    ADVMETA_BOOT = 5,
    ATE_FACTORY_BOOT = 6,
    ALARM_BOOT = 7,
    KERNEL_POWER_OFF_CHARGING_BOOT = 8,
    LOW_POWER_OFF_CHARGING_BOOT = 9,
    DONGLE_BOOT = 10,
    UNKNOWN_BOOT
};

#define ALARM_STR_SIZE                0x0e
#define ANDROID_ALARM_BASE_CMD(cmd)   (cmd & ~(_IOC(0, 0, 0xf0, 0)))
#define ANDROID_ALARM_SET_ALARM_BOOT  _IOW('a', 7, struct timespec64)
#define SAPA_START_POLL_TIME          (10LL * NSEC_PER_SEC) /* 10 sec */
#define SAPA_BOOTING_TIME             (5*60)
#define SAPA_POLL_TIME                (15*60)

static int gs_debug_mask = BIT(0);
module_param_named(gs_debug_mask, gs_debug_mask, int, 0664);
static char gs_pwr_on_alarm[] = "0000000000000";
static char const gs_clear_str[] = "0000000000000";
static struct work_struct gs_check_func_t = {0};
static struct alarm gs_check_poll_t = {0};
static int gs_triggered = 0;

static struct rtc_device *gs_gxy_rtc_t = NULL;

enum {
    SAPA_DISTANT = 0,
    SAPA_NEAR,
    SAPA_EXPIRED,
    SAPA_OVER
};

void gxy_alarm_set_power_on(struct timespec64 new_pwron_time, bool logo)
{
    unsigned long pwron_time = 0;
    struct rtc_wkalrm alm_t = {0};
    struct rtc_device *alarm_rtc_dev_t = NULL;

    GXY_INFO("%s : alarm set power on\n", __func__);

    if (new_pwron_time.tv_sec > 0) {
        pwron_time = new_pwron_time.tv_sec;
        alm_t.enabled = (logo ? 3 : 2);
    } else {
        pwron_time = 0;
        alm_t.enabled = 4;
    }
    alarm_rtc_dev_t = alarmtimer_get_rtcdev();
    rtc_time64_to_tm(pwron_time, &alm_t.time);
    GXY_INFO("%s : rtc time %02d:%02d:%02d %02d/%02d/%04d\n",
             __func__,
             alm_t.time.tm_hour, alm_t.time.tm_min,
             alm_t.time.tm_sec, alm_t.time.tm_mon + 1,
             alm_t.time.tm_mday, alm_t.time.tm_year + 1900);
    rtc_set_alarm(alarm_rtc_dev_t, &alm_t);
}

void gxy_alarm_get_rtctime_sec(struct timespec64 *rtc_time_now_sec)
{
    struct rtc_device *alarm_rtc_dev_t = NULL;
    struct rtc_time tmm_t = {0};

    alarm_rtc_dev_t = alarmtimer_get_rtcdev();
    rtc_read_time(alarm_rtc_dev_t, &tmm_t);
    GXY_INFO("%s : rtc alarm time now %02d:%02d:%02d %02d/%02d/%04d\n",
             __func__,
             tmm_t.tm_hour, tmm_t.tm_min,
             tmm_t.tm_sec, tmm_t.tm_mon + 1,
             tmm_t.tm_mday, tmm_t.tm_year + 1900);
    rtc_time_now_sec->tv_sec = rtc_tm_to_time64(&tmm_t);
    GXY_INFO("%s : rtc alarm time now-> %ld\n", __func__, rtc_time_now_sec->tv_sec);
}

static int gxy_alarm_open(struct inode *inode, struct file *file)
{
    file->private_data = NULL;
    GXY_INFO("%s : (%d:%d)\n", __func__, current->tgid, current->pid);
    return 0;
}

static int gxy_alarm_release(struct inode *inode, struct file *file)
{
    if (file->private_data) {
        GXY_INFO("%s : (%d:%d)(%lu)\n", __func__,
            current->tgid, current->pid, (uintptr_t)file->private_data);
    }
    return 0;
}

static struct rtc_time alarm_str2time(char *str, int length)
{
    int time_int[12] = {0};
    int countNum = 0;
    int year = 0;
    int month = 0;
    int day = 0;
    int hour = 0;
    int minute = 0;
    struct rtc_time rtc_time_t = {0};

    if (strlen(str) > length){
        GXY_INFO("%s : overlength strlen=%d, len=%d\n", __func__, strlen(str), length);
        return rtc_time_t;
    }

    for (countNum = 1; countNum < 13; countNum++) {
        if ((str[countNum] > '0') && (str[countNum] <='9')) {
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

    rtc_time_t.tm_year = year-1900;
    rtc_time_t.tm_mon  = month-1;
    rtc_time_t.tm_mday = day;
    rtc_time_t.tm_hour = hour;
    rtc_time_t.tm_min  = minute;
    rtc_time_t.tm_sec  = 0;
    GXY_INFO("%s : year = %d,month = %d,day = %d,hour = %d,minute = %d\n",
             __func__,
             rtc_time_t.tm_year, rtc_time_t.tm_mon, rtc_time_t.tm_mday,
             rtc_time_t.tm_hour, rtc_time_t.tm_min);
    return rtc_time_t;
}

static int sapa_check_state(unsigned long *data)
{
    unsigned long rtc_secs = 0;
    unsigned long secs_pwron = 0;
    struct rtc_device *alarm_rtc_dev_t = NULL;
    struct rtc_wkalrm alm_t = {0};
    struct timespec64 alarm_base_time_sec_t = {0};
    int res = SAPA_NEAR;

    gxy_alarm_get_rtctime_sec(&alarm_base_time_sec_t);
    rtc_secs = alarm_base_time_sec_t.tv_sec;
    alarm_rtc_dev_t = alarmtimer_get_rtcdev();
    rtc_read_alarm(alarm_rtc_dev_t, &alm_t);
    secs_pwron = rtc_tm_to_time64(&alm_t.time);
    GXY_INFO("%s : year = %ld",__func__, secs_pwron);

    if (rtc_secs < secs_pwron) {
        if (secs_pwron - rtc_secs > SAPA_POLL_TIME) {
            res = SAPA_DISTANT;
        }
        if (data) {
            *data = secs_pwron - rtc_secs;
        }
    } else if (rtc_secs <= secs_pwron + SAPA_BOOTING_TIME) {
        res = SAPA_EXPIRED;
        if (data) {
            *data = rtc_secs + 10;
        }
    } else {
        res = SAPA_OVER;
    }

    GXY_INFO("%s : rtc:%lu, alrm:%lu[%d]\n", __func__, rtc_secs, secs_pwron, res);
    return res;
}

static void sapa_check_func(struct work_struct *work)
{
    int res = 0;
    unsigned long remain = 0;

    res = sapa_check_state(&remain);
    if (res <= SAPA_NEAR) {
        ktime_t kt;

        if (res == SAPA_DISTANT) {
            remain = SAPA_POLL_TIME;
        }
        kt = ns_to_ktime((u64)remain * NSEC_PER_SEC);
        alarm_start_relative(&gs_check_poll_t, kt);
        GXY_INFO("%s : next %lu s\n", __func__, remain);
    } else if (res == SAPA_EXPIRED) {
        gs_triggered = 1;
    }
}

static enum alarmtimer_restart sapa_check_callback(struct alarm *alarm, ktime_t now)
{
    schedule_work(&gs_check_func_t);
    return ALARMTIMER_NORESTART;
}

static int gxy_alarm_get_alarm(struct rtc_wkalrm *alm)
{
    alm->enabled = gs_triggered;
    return 1;
}

int gxy_alarm_set_alarm(char *alarm_data)
{
    struct timespec64 alarm_delta_sec_t = {0};
    struct timespec64 set_time_sec_t = {0};
    struct timespec64 base_time_sec_t = {0};
    struct timespec64 alarm_base_time_sec_t = {0};
    char cmd_input[ALARM_STR_SIZE] = {0};
    struct rtc_time rtc_tm_set_t = {0};
    struct rtc_time rtc_tm_base_t = {0};
    int ret = 0;

    GXY_INFO("%s : alarm write!\n", __func__);
    strlcpy(cmd_input, alarm_data, ALARM_STR_SIZE);
    cmd_input[13] = '\0';
    switch ((int)cmd_input[0]) {
        case '0':
            set_time_sec_t.tv_sec = 0;
            gxy_alarm_set_power_on(set_time_sec_t, false);
            GXY_INFO("%s : Disable power on alarm\n", __func__);
            strncpy(gs_pwr_on_alarm, gs_clear_str, strlen(gs_pwr_on_alarm));
            break;
        case '1':
            //get alarm
            rtc_tm_set_t = alarm_str2time(cmd_input, ALARM_STR_SIZE);
            GXY_INFO("%s : set_value %02d:%02d:%02d %02d/%02d/%04d\n",
                     __func__,
                     rtc_tm_set_t.tm_hour, rtc_tm_set_t.tm_min,
                     rtc_tm_set_t.tm_sec, rtc_tm_set_t.tm_mon + 1,
                     rtc_tm_set_t.tm_mday, rtc_tm_set_t.tm_year + 1900);
            set_time_sec_t.tv_sec = rtc_tm_to_time64(&rtc_tm_set_t);
            //get UTC time
            base_time_sec_t.tv_sec = ktime_get_real_seconds();
            // SEC has already add timezone, avoid this function
            rtc_time64_to_tm(base_time_sec_t.tv_sec, &rtc_tm_base_t);
            GXY_INFO("%s : rtc_base+timezone %02d:%02d:%02d %02d/%02d/%04d\n",
                     __func__,
                     rtc_tm_base_t.tm_hour, rtc_tm_base_t.tm_min,
                     rtc_tm_base_t.tm_sec, rtc_tm_base_t.tm_mon + 1,
                     rtc_tm_base_t.tm_mday, rtc_tm_base_t.tm_year + 1900);
            //get delta
            alarm_delta_sec_t.tv_sec = set_time_sec_t.tv_sec - base_time_sec_t.tv_sec;
            if (alarm_delta_sec_t.tv_sec < 0) {
                strncpy(gs_pwr_on_alarm, gs_clear_str, strlen(gs_pwr_on_alarm));
                GXY_INFO("%s : alarm set less than ktime.\n", __func__);
                break;
            }
            //get kernel time
            gxy_alarm_get_rtctime_sec(&alarm_base_time_sec_t);
            alarm_delta_sec_t.tv_sec += alarm_base_time_sec_t.tv_sec;
            //set alarm
            gxy_alarm_set_power_on(alarm_delta_sec_t, false);
            GXY_INFO("%s : alarm set-> %s\n", __func__, cmd_input);
            break;

        default:
            set_time_sec_t.tv_sec = 0;
            gxy_alarm_set_power_on(set_time_sec_t, false);
            GXY_INFO("%s : No such cmd\n", __func__);
            strncpy(gs_pwr_on_alarm, gs_clear_str, strlen(gs_pwr_on_alarm));
            break;
    }
    strncpy(gs_pwr_on_alarm, cmd_input, strlen(gs_pwr_on_alarm));
    return ret;
}
EXPORT_SYMBOL(gxy_alarm_set_alarm);

static long gxy_power_on_alarm_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    int rv = 0;
    char bootalarm_data[ALARM_STR_SIZE] = {0};

    GXY_INFO("%s : cmd = %u\n", __func__, cmd);

    switch (ANDROID_ALARM_BASE_CMD(cmd)) {
        case ANDROID_ALARM_SET_ALARM_BOOT:
            if (copy_from_user(bootalarm_data, (void __user *)arg, 14)) {
                rv = -EFAULT;
                return rv;
            }
            rv = gxy_alarm_set_alarm(bootalarm_data);
            break;
        default:
            GXY_INFO("%s : No such cmd\n", __func__);
            break;
    }

    return rv;
}

enum gxy_boot_mode_t gxy_get_boot_mode(void) {
    struct device_node *np_chosen_t = NULL;
    struct gxy_tag_bootmode *tag_t = NULL;
    enum gxy_boot_mode_t boot_mode = UNKNOWN_BOOT;

    np_chosen_t = of_find_node_by_path("/chosen");
    if (!np_chosen_t) {
        np_chosen_t = of_find_node_by_path("/chosen@0");
    }

    if (np_chosen_t) {
        tag_t = (struct gxy_tag_bootmode *)of_get_property(
            np_chosen_t, "atag,boot", NULL);
        if (!tag_t) {
            GXY_ERR("%s : failed to get atag,boot\n", __func__);
        } else {
            GXY_INFO("%s : bootmode:%d\n", __func__, tag_t->bootmode);
            boot_mode = tag_t->bootmode;
        }
    } else {
        GXY_ERR("%s : failed to get /chosen and /chosen@0\n", __func__);
    }

    return boot_mode;
}

static int gs_alarm_enable = SAPA_OVER;
static void sapa_load_alarm(void)
{
    unsigned long alarm_secs = 0;

    gs_alarm_enable = sapa_check_state(&alarm_secs);
}

static const struct file_operations alarm_fops = {
    .owner = THIS_MODULE,
    .unlocked_ioctl = gxy_power_on_alarm_ioctl,
    .open = gxy_alarm_open,
    .release = gxy_alarm_release,
};

static struct miscdevice alarm_device = {
    .minor = MISC_DYNAMIC_MINOR,
    .name = "power_on_alarm",
    .fops = &alarm_fops,
};

static void sapa_init(void)
{
    ktime_t kt = {0};
    enum gxy_boot_mode_t boot_mode = gxy_get_boot_mode();

    gs_triggered = 0;
    if ((boot_mode == KERNEL_POWER_OFF_CHARGING_BOOT ||
            boot_mode == LOW_POWER_OFF_CHARGING_BOOT) &&
            gs_alarm_enable != SAPA_OVER) {
        alarm_init(&gs_check_poll_t, ALARM_REALTIME, sapa_check_callback);
        INIT_WORK(&gs_check_func_t, sapa_check_func);
        kt = ns_to_ktime(SAPA_START_POLL_TIME);
        alarm_start_relative(&gs_check_poll_t, kt);
    }
}

static void sapa_exit(void)
{
    enum gxy_boot_mode_t boot_mode = gxy_get_boot_mode();

    if ((boot_mode == KERNEL_POWER_OFF_CHARGING_BOOT ||
            boot_mode == LOW_POWER_OFF_CHARGING_BOOT) &&
            gs_alarm_enable != SAPA_OVER) {
        cancel_work_sync(&gs_check_func_t);
        alarm_cancel(&gs_check_poll_t);
    }
}

/**********************************************************
 *
 *   /sys/class/rtc/rtc0/alarm_boot (sysfs)
 *
 *********************************************************/
static ssize_t alarm_boot_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    ssize_t retval = 0;
    struct rtc_wkalrm alm_t = {0};

    retval = gxy_alarm_get_alarm(&alm_t);
    if (retval) {
        retval = sprintf(buf, "%d", alm_t.enabled);
        pr_info("sapa_%s:%d\n",__func__, alm_t.enabled);
        return retval;
    }

    return retval;
}
static DEVICE_ATTR_RO(alarm_boot);

static struct attribute *gxy_alarm_attrs[] = {
    &dev_attr_alarm_boot.attr,
    NULL,
};

static const struct attribute_group gxy_alarm_attr_group = {
    .attrs        = gxy_alarm_attrs,
};

static int gxy_alarm_probe(struct platform_device *pdev)
{
    int rc = 0;

    GXY_INFO("%s : enter\n", __func__);

    gs_gxy_rtc_t = rtc_class_open("rtc0");
    if (!gs_gxy_rtc_t) {
        GXY_ERR("%s : rtc0 not available\n", __func__);
        return -EPROBE_DEFER;
    }

    /* create sysfs debug files */
    rc = sysfs_create_group(&gs_gxy_rtc_t->dev.kobj,
            &gxy_alarm_attr_group);
    if (rc < 0) {
        GXY_ERR("%s: Fail to create sysfs files!\n", __func__);
        return rc;
    }

    rc = misc_register(&alarm_device);
    if (rc) {
        GXY_ERR("%s: alarm_device register fail !!!\n", __func__);
        return rc;
    }

    sapa_load_alarm();
    sapa_init();

    GXY_INFO("%s : alarm dev Install Success !!\n", __func__);

    return 0;
}

static int gxy_alarm_remove(struct platform_device *pdev)
{
    sapa_exit();
    misc_deregister(&alarm_device);
    sysfs_remove_group(&gs_gxy_rtc_t->dev.kobj,
        &gxy_alarm_attr_group);

    return 0;
}

/*M55 code for SR-QN6887A-01-535 by xiongxiaoliang at 20231110 start*/
static const struct of_device_id gxy_alarm_of_match[] = {
    {.compatible = "gxy, gxy_alarm",},
    {},
};

MODULE_DEVICE_TABLE(of, gxy_alarm_of_match);

static struct platform_driver gxy_alarm_driver = {
    .probe = gxy_alarm_probe,
    .remove = gxy_alarm_remove,
    .driver = {
        .name = "gxy_alarm",
        .of_match_table = of_match_ptr(gxy_alarm_of_match),
    },
};
/*M55 code for SR-QN6887A-01-535 by xiongxiaoliang at 20231110 end*/

static int __init gxy_alarm_dev_init(void)
{
    GXY_INFO("%s : start\n", __func__);

    return platform_driver_register(&gxy_alarm_driver);
}
module_init(gxy_alarm_dev_init);

static void __exit gxy_alarm_dev_exit(void)
{
    platform_driver_unregister(&gxy_alarm_driver);
}
module_exit(gxy_alarm_dev_exit);

MODULE_AUTHOR("SHIXUANXUAN");
MODULE_DESCRIPTION("power_on_alarm Driver based on QC-SM7450");
MODULE_LICENSE("GPL v2");
