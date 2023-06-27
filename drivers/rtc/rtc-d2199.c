/*
 *	Real Time Clock driver for Dialog D2199
 *
 *	Copyright (C) 2012 Dialog Semiconductor Ltd.
 *
 *  	Author: D. Chen
 *
 *  	This program is free software; you can redistribute  it and/or modify it
 *  	under  the terms of  the GNU General  Public License as published by the
 *  	Free Software Foundation;  either version 2 of the  License, or (at your
 *  	option) any later version.
 *
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/time.h>
#include <linux/rtc.h>
#include <linux/bcd.h>
#include <linux/interrupt.h>
#include <linux/ioctl.h>
#include <linux/completion.h>
#include <linux/delay.h>
#include <linux/platform_device.h>

#include <linux/d2199/pmic.h>
#include <linux/d2199/d2199_reg.h> 
#include <linux/d2199/hwmon.h>
#include <linux/d2199/core.h>
#include <linux/d2199/rtc.h>

#if defined(CONFIG_RTC_CHN_ALARM_BOOT)
#include <linux/reboot.h>
#include <linux/workqueue.h>
#endif

#define DRIVER_NAME "d2199-rtc"

#define to_d2199_from_rtc_dev(d) container_of(d, struct d2199, rtc.pdev.dev)

#define D2199_YEAR_BASE		100	/*2000 - 1900 */
#define SEC_YEAR_BASE 			12  /* 2012 */

static struct rtc_time init_rtc_time =
{
	.tm_sec   = 0xca,
	.tm_min   = 0xfe,
	.tm_hour  = 0xca,
	.tm_mday  = 0xfe,
	.tm_mon   = 0xca,
	.tm_year  = 0xfe,
	.tm_wday  = 0xca,
	.tm_yday  = 0xfe,
	.tm_isdst = 0xca
};

//static struct rtc_time history_rtc_time[1024];
//static unsigned int history_rtc_time_index = 0;
u8 d2199_alarm_by_battery=0;

static int d2199_rtc_check_param(struct rtc_time *rtc_tm)
{
	if((rtc_tm->tm_sec > D2199_RTC_SECONDS_LIMIT) || (rtc_tm->tm_sec < 0))
		return -D2199_RTC_INVALID_SECONDS;

	if((rtc_tm->tm_min > D2199_RTC_MINUTES_LIMIT) || (rtc_tm->tm_min < 0))
		return -D2199_RTC_INVALID_MINUTES;

	if((rtc_tm->tm_hour > D2199_RTC_HOURS_LIMIT) || (rtc_tm->tm_hour < 0))
		return -D2199_RTC_INVALID_HOURS;

	if(rtc_tm->tm_mday == 0)
		return -D2199_RTC_INVALID_DAYS;

	if((rtc_tm->tm_mon > D2199_RTC_MONTHS_LIMIT) || (rtc_tm->tm_mon <= 0))
		return -D2199_RTC_INVALID_MONTHS;

	if((rtc_tm->tm_year > D2199_RTC_YEARS_LIMIT) || (rtc_tm->tm_year < 0))
		return -D2199_RTC_INVALID_YEARS;

	return 0;
}


/*
 * Read current time and date in RTC
 */
static int d2199_rtc_readtime(struct device *dev, struct rtc_time *tm)
{
	struct d2199 *d2199 = dev_get_drvdata(dev);
	u8 rtc_time1[6] = { 0, };
	int ret = 0;

	ret = d2199_block_read(d2199, D2199_COUNT_S_REG, 6, rtc_time1);

	if (ret < 0)
	{
		dlg_err("%s: read error %d\n", __func__, ret);
		return ret;
	}

	tm->tm_sec  = rtc_time1[0] & D2199_RTC_SECS_MASK;
	tm->tm_min  = rtc_time1[1] & D2199_RTC_MINS_MASK;
	tm->tm_hour = rtc_time1[2] & D2199_RTC_HRS_MASK;
	tm->tm_mday = rtc_time1[3] & D2199_RTC_DAY_MASK;
	tm->tm_mon  = rtc_time1[4] & D2199_RTC_MTH_MASK;
	tm->tm_year = rtc_time1[5] & D2199_RTC_YRS_MASK;

	dlg_info("%s : RTC register %02x-%02x-%02x %02x:%02x:%02x\n", __func__, 
		rtc_time1[5], rtc_time1[4], rtc_time1[3], rtc_time1[2], rtc_time1[1], rtc_time1[0]);
	
	 /* sanity checking */
	ret = d2199_rtc_check_param(tm);
	if (ret)
	{
		dlg_err("%s : check error %d\n", __func__, ret);
		return ret;
	}

	tm->tm_year += D2199_YEAR_BASE;
	tm->tm_mon -= 1;
	tm->tm_yday = rtc_year_days(tm->tm_mday, tm->tm_mon, tm->tm_year);

	printk(KERN_ERR "[WS][%s] y[%d], m[%d], d[%d], h[%d], m[%d], s[%d]\n",
		__func__, tm->tm_year, tm->tm_mon, tm->tm_mday,
		tm->tm_hour, tm->tm_min, tm->tm_sec);
	
	return ret;
}

/*
 * Set current time and date in RTC
 */
static int d2199_rtc_settime(struct device *dev, struct rtc_time *tm)
{

	struct d2199 *d2199 = dev_get_drvdata(dev);
	u8 rtc_time[6];
	int ret = 0;
	unsigned long ticks;
	
	rtc_time[0] = tm->tm_sec;
	rtc_time[1] = tm->tm_min;
	rtc_time[2] = tm->tm_hour;
	rtc_time[3] = tm->tm_mday;
	rtc_time[4] = tm->tm_mon + 1;
	rtc_time[5] = tm->tm_year - D2199_YEAR_BASE;
	rtc_time[5] |= D2199_MONITOR_MASK;

	dlg_info("%s : RTC register %02x-%02x-%02x %02x:%02x:%02x\n", __func__, 
		rtc_time[0], rtc_time[1], rtc_time[2], rtc_time[3], rtc_time[4], rtc_time[5]);

	/* Write time to RTC */
	ret = d2199_block_write(d2199, D2199_COUNT_S_REG, 6, rtc_time);
	if (ret < 0)
		dlg_err("%s: write error %d\n", __func__, ret);

#if 1	// for Marvell AP
	rtc_tm_to_time(tm, &ticks);

	if(d2199->pdata->sync)
	{
		dlg_info("[WS][%s] call rtc sync, ticks[%ld] \n", __func__, ticks);
		d2199->pdata->sync(ticks);
	}
#endif
	return 0;
}

static int d2199_rtc_stop_alarm(struct d2199 *d2199)
{
	int ret;
	u8 time[6] = { 0, };

	dlg_info("%s \n", __func__);

	/* For latching the data before RMW */
	ret = d2199_block_read(d2199, D2199_ALARM_S_REG, 6, time);
	if (ret < 0)
		return ret;

	/* Set RTC_SET to stop the clock */
	ret = d2199_clear_bits(d2199, D2199_ALARM_Y_REG, D2199_ALARM_ON_MASK);
	if (ret < 0)
		return ret;

	return 0;      
}


static int d2199_rtc_start_alarm(struct d2199 *d2199)
{
	int ret;
	u8 time[6] = { 0, };

	dlg_info("%s \n", __func__);

	/* For latching the data before RMW */
	ret = d2199_block_read(d2199, D2199_ALARM_S_REG, 6, time);
	if (ret < 0)
		return ret;

	ret = d2199_set_bits(d2199, D2199_ALARM_Y_REG, D2199_ALARM_ON_MASK);
	if (ret < 0)
		return ret;

	return 0;
}

/*
 * Read alarm time and date in RTC
 */
static int d2199_rtc_readalarm(struct device *dev, struct rtc_wkalrm *alrm)
{
	struct d2199 *d2199 = dev_get_drvdata(dev);
	struct rtc_time *tm = &alrm->time;
	u8 time[6] = { 0, };
	int ret;

	ret = d2199_block_read(d2199, D2199_ALARM_S_REG, 6, time);
	if (ret < 0)
	{
		dlg_err("%s: read error %d\n", __func__, ret);
		return ret;
	}

	tm->tm_sec = time[0] & D2199_RTC_ALMSECS_MASK;	
	tm->tm_min = time[1] & D2199_RTC_ALMMINS_MASK;
	tm->tm_hour = time[2] & D2199_RTC_ALMHRS_MASK;	
	tm->tm_mday = time[3] & D2199_RTC_ALMDAY_MASK;
	tm->tm_mon = time[4] & D2199_RTC_ALMMTH_MASK;	
	tm->tm_year = time[5] & D2199_RTC_ALMYRS_MASK;

	ret = d2199_rtc_check_param(tm);
	if (ret < 0)
	{
		dlg_err("%s : check error %d\n", __func__, ret);
		return ret;
	}
    
	tm->tm_year += D2199_YEAR_BASE;
	tm->tm_mon -= 1;

	dlg_info("%s : RTC register %02x-%02x-%02x %02x:%02x:%02x\n", __func__, 
			time[5], time[4], time[3], time[2], time[1], time[0]);
	
	return 0;
}

static int d2199_rtc_setalarm(struct device *dev, struct rtc_wkalrm *alrm)
{
	struct d2199 *d2199 = dev_get_drvdata(dev);
	//struct rtc_time *tm = &alrm->time;
	struct rtc_time alarm_tm;
	u8 time[6], rtc_ctrl, reg_val;
	int ret;

	/* Set RTC_SET to stop the clock */
	ret = d2199_clear_bits(d2199, D2199_ALARM_Y_REG, D2199_ALARM_ON_MASK);
	if (ret < 0)
	{
		dlg_err("%s: clear bits error %d\n", __func__, ret);
		return ret;
	}

	memcpy(&alarm_tm, &alrm->time, sizeof(struct rtc_time));

	alarm_tm.tm_year -= D2199_YEAR_BASE;
	alarm_tm.tm_mon += 1;

	ret = d2199_rtc_check_param(&alarm_tm);
	if (ret < 0)
	{
		dlg_err("%s : check error %d\n", __func__, ret);
		return ret;
	}
	
	memset(time, 0, sizeof(time));

	time[0] = alarm_tm.tm_sec;
	d2199_reg_read(d2199, D2199_ALARM_MI_REG, &reg_val);
	rtc_ctrl = reg_val & (~D2199_RTC_ALMMINS_MASK);
	time[1] = rtc_ctrl | alarm_tm.tm_min;
	time[2] |= alarm_tm.tm_hour;
	time[3] |= alarm_tm.tm_mday;
	time[4] |= alarm_tm.tm_mon;
	d2199_reg_read(d2199, D2199_ALARM_Y_REG, &reg_val);
	rtc_ctrl = reg_val & (~D2199_RTC_ALMYRS_MASK);
	time[5] = rtc_ctrl | alarm_tm.tm_year;

	/* Write time to RTC */
	ret = d2199_block_write(d2199, D2199_ALARM_S_REG, 6, time);
	if (ret < 0)
	{
		dlg_err("%s: write error %d\n", __func__, ret);
	    	return ret;
	}

	if (alrm->enabled)
		ret = d2199_rtc_start_alarm(d2199);
	else
		ret = d2199_rtc_stop_alarm(d2199);

	dlg_info("%s : RTC register %02x-%02x-%02x %02x:%02x:%02x, enable(%d)\n", __func__, 
				time[5], time[4], time[3], time[2], time[1], time[0], alrm->enabled);

    return ret;
}

static int d2199_rtc_alarm_irq_enable(struct device *dev,
				  unsigned int enabled)
{
	struct d2199 *d2199 = dev_get_drvdata(dev);

	dlg_info("%s \n", __func__);
	
	if (enabled)
		return d2199_rtc_start_alarm(d2199);
	else
		return d2199_rtc_stop_alarm(d2199);
}

#if 0	// block
static int d2199_rtc_update_irq_enable(struct device *dev,
					unsigned int enabled)
{
	int ret;
	struct d2199 *d2199 = dev_get_drvdata(dev);
	u8 time[6];

	dlg_info("%s \n", __func__);
	
	/* For latching the data before RMW */
	ret = d2199_block_read(d2199, BOBCAT_ALARMS_REG, 6, time);
	if (ret < 0)
		return ret;

	if (enabled) {
		ret = d2199_set_bits(d2199, BOBCAT_ALARMY_REG, BOBCAT_ALARMY_TICKON);
		if (ret < 0)
			return ret;
	} else {
		ret = d2199_clear_bits(d2199, BOBCAT_ALARMY_REG, BOBCAT_ALARMY_TICKON);
		if (ret < 0)
			return ret;
	}
	return 0;
}
#endif

#if defined(CONFIG_RTC_CHN_ALARM_BOOT)
#if defined(CONFIG_BATTERY_SAMSUNG)
extern unsigned int lpcharge;
#else
extern int spa_lpm_charging_mode_get(void);
#endif
struct work_struct reboot_work;
static void pm80x_rtc_reboot_work(struct work_struct *work)
{
	kernel_restart("alarm");
}
#endif

static irqreturn_t d2199_rtc_timer_alarm_handler(int irq, void *data)
{
	struct d2199 *d2199 = data;
	struct rtc_device *rtc = d2199->rtc.rtc;

	rtc_update_irq(rtc, 1, RTC_AF | RTC_IRQF);
        dlg_info("\nRTC: TIMER ALARM\n");

#if defined(CONFIG_RTC_CHN_ALARM_BOOT)
#if defined(CONFIG_BATTERY_SAMSUNG)
	if (lpcharge)
#else
	if(spa_lpm_charging_mode_get())
#endif
		schedule_work(&reboot_work);
#endif

	return IRQ_HANDLED;
}

#if 0
static irqreturn_t d2199_rtc_tick_alarm_handler(int irq, void *data)
{
	struct d2199 *d2199 = data;
	struct rtc_device *rtc = d2199->rtc.rtc;
	
	rtc_update_irq(rtc, 1, RTC_PF | RTC_IRQF);
	dlg_info("RTC: TICK ALARM\n");
	return IRQ_HANDLED;
}
#endif

static const struct rtc_class_ops d2199_rtc_ops = {
	.read_time = d2199_rtc_readtime,
	.set_time = d2199_rtc_settime,
	.read_alarm = d2199_rtc_readalarm,
	.set_alarm = d2199_rtc_setalarm,
	.alarm_irq_enable = d2199_rtc_alarm_irq_enable,
#if 0	// block
	.update_irq_enable = d2199_rtc_update_irq_enable,
#endif
};

#ifdef CONFIG_PM
static int d2199_rtc_suspend(struct device *dev)
{
	dlg_info("%s \n",__FUNCTION__);

	return 0;
}
static int d2199_rtc_resume(struct device *dev)
{
#if 0 //defined(CONFIG_RTC_ANDROID_ALARM_WORKAROUND)
	/* This option selects temporary fix for alarm handling in 'Android'
	 * environment. This option enables code to disable alarm in the
	 * 'resume' handler of RTC driver. In the normal mode,
	 * android handles all alarms in software without using the RTC chip.
	 * Android sets the alarm in the rtc only in the suspend path (by
	 * calling .set_alarm with struct rtc_wkalrm->enabled set to 1).
	 * In the resume path, android tries to disable alarm by calling
	 * .set_alarm with struct rtc_wkalrm->enabled' field set to 0.
	 * But unfortunately, it memsets the rtc_wkalrm struct to 0, which
	 * causes the rtc lib to flag error and control does not reach this
	 * driver. Hence this workaround.
	 */
	d2199_rtc_alarm_irq_enable(dev, 0);
#endif
	dlg_info("%s \n",__FUNCTION__);
	return 0; 
}
#else
#define d2199_rtc_suspend NULL
#define d2199_rtc_resume NULL
#endif

static void d2199_rtc_time_fixup(struct device *dev)
{
	struct rtc_time current_rtc_time;
	memset(&current_rtc_time, 0 , sizeof(struct rtc_time));

	d2199_rtc_readtime(dev, &current_rtc_time);
	current_rtc_time.tm_year += SEC_YEAR_BASE;
	memcpy(&init_rtc_time, &current_rtc_time, sizeof(struct rtc_time));
	d2199_rtc_settime(dev, &current_rtc_time);
}

static int d2199_rtc_probe(struct platform_device *pdev)
{
	struct d2199 *d2199 = platform_get_drvdata(pdev);
	struct d2199_rtc *dlg_rtc = &d2199->rtc;
	int ret = 0;
	u8 reg_val;

#if 1	// for pxa1088
	unsigned long ticks = 0;
	unsigned long now_ticks = 0, default_ticks = 0;
	struct rtc_time tm;
	struct rtc_time default_time; 
#endif

	dlg_info("Starting RTC\n");
	device_init_wakeup(&pdev->dev, 1);

	dlg_rtc->rtc = rtc_device_register("d2199", &pdev->dev,
					  &d2199_rtc_ops, THIS_MODULE);
	if (IS_ERR(dlg_rtc->rtc)) {
		ret = PTR_ERR(dlg_rtc->rtc);
		dlg_err("failed to register RTC: %d\n", ret);
		return ret;
	}

	ret = d2199_reg_read(d2199, D2199_COUNT_Y_REG,&reg_val);
	if(ret < 0)
		reg_val = 0;	

	if (!(reg_val & D2199_MONITOR_MASK))
	{
		dlg_info("Fixup RTC\n");
		d2199_set_bits(d2199, D2199_COUNT_Y_REG, D2199_MONITOR_MASK);
		d2199_rtc_time_fixup(&pdev->dev);
	}
        


#if 1	// for pxa1088
	{
		//printk("====> RTC resetting <====\n");

		d2199_rtc_readtime(&pdev->dev,&tm);
	
		rtc_tm_to_time(&tm, &now_ticks);
		default_time.tm_year = 100;
		default_time.tm_mon = 11;
		default_time.tm_mday = 31;
		default_time.tm_hour = 0;
		default_time.tm_min = 0;
		default_time.tm_sec = 0;
		rtc_tm_to_time(&default_time, &default_ticks);
		
		if((tm.tm_year < 70) || (tm.tm_year > 138) || (now_ticks <= default_ticks)) {
			printk("%s now : %ld default : %ld\n", __func__, now_ticks, default_ticks);
#if defined(CONFIG_MACH_CS05)
			tm.tm_year = 114;
#else
			tm.tm_year = 112;
#endif
			tm.tm_mon = 0;
			tm.tm_mday = 1;
			tm.tm_hour = 0;
			tm.tm_min = 0;
			tm.tm_sec = 0;
			ret = d2199_rtc_settime(&pdev->dev,&tm);
			if (ret < 0) {
				dev_err(&pdev->dev, "Failed to set initial time.\n");
				return -1;
			}
		}
		else
		{
			printk("ELSE %s now : %ld default : %ld\n", __func__, now_ticks, default_ticks);
		}

		rtc_tm_to_time(&tm, &ticks);
		if (d2199->pdata->sync) {
			d2199->pdata->sync(ticks);
		}
	}
#endif

#if defined(CONFIG_RTC_CHN_ALARM_BOOT)
#if defined(CONFIG_BATTERY_SAMSUNG)
	if (lpcharge)
#else
	if(spa_lpm_charging_mode_get())
#endif
	{
		INIT_WORK(&reboot_work, pm80x_rtc_reboot_work);
	}
#endif

	d2199_register_irq(d2199, D2199_IRQ_EALARM, d2199_rtc_timer_alarm_handler,
                            0, "RTC Timer Alarm", d2199);
#if 0
	d2199_register_irq(d2199, BOBCAT_IRQ_ETICK, d2199_rtc_tick_alarm_handler,
                            0, "RTC Tick Alarm", d2199);
#endif
	dev_info(d2199->dev, "\nRTC registered\n");

	return 0;
}

static int __devexit d2199_rtc_remove(struct platform_device *pdev)
{
	struct d2199 *d2199 = platform_get_drvdata(pdev);
	struct d2199_rtc *dlg_rtc = &d2199->rtc;
	d2199_free_irq(d2199, D2199_IRQ_EALARM);
	//d2199_free_irq(d2199, BOBCAT_IRQ_ETICK);
	rtc_device_unregister(dlg_rtc->rtc);

	return 0;
}

static struct dev_pm_ops d2199_rtc_pm_ops = {
	.suspend = d2199_rtc_suspend,
	.resume = d2199_rtc_resume,
	.thaw = d2199_rtc_resume,
	.restore = d2199_rtc_resume,
	.poweroff = d2199_rtc_suspend,
};

static struct platform_driver d2199_rtc_driver = {
	.probe = d2199_rtc_probe,
	.remove = __devexit_p(d2199_rtc_remove),
	.driver = {
		.name = DRIVER_NAME,
		.owner = THIS_MODULE,
		.pm = &d2199_rtc_pm_ops,
	},
};

static int __init d2199_rtc_init(void)
{
	return platform_driver_register(&d2199_rtc_driver);
}
module_init(d2199_rtc_init);

static void __exit d2199_rtc_exit(void)
{
	platform_driver_unregister(&d2199_rtc_driver);
}
module_exit(d2199_rtc_exit);

MODULE_AUTHOR("Dialog Semiconductor Ltd < william.seo@diasemi.com >");
MODULE_DESCRIPTION("RTC driver for the Dialog d2199 PMIC");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:" DRIVER_NAME);
