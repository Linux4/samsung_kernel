/*
 * Copyright (C) 2012 Spreadtrum Communications Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/module.h>
#include <linux/err.h>
#include <linux/rtc.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <asm/bitops.h>
#include <linux/clkdev.h>
#include <linux/regulator/consumer.h>
#include <linux/clk.h>
#include <linux/wakelock.h>
#include <linux/of.h>
#include <linux/spinlock.h>

#include <soc/sprd/sci_glb_regs.h>
#include <soc/sprd/arch_lock.h>
#include <soc/sprd/adi.h>

#ifdef CONFIG_64BIT
#define RTC_BASE (SPRD_ADI_BASE + 0x8080)
#else
#define RTC_BASE ANA_RTC_BASE
#endif

#define ANA_RTC_SEC_CNT_VALUE		(RTC_BASE + 0x00)
#define ANA_RTC_MIN_CNT_VALUE		(RTC_BASE + 0x04)
#define ANA_RTC_HOUR_CNT_VALUE		(RTC_BASE + 0x08)
#define ANA_RTC_DAY_CNT_VALUE		(RTC_BASE + 0x0C)

#define ANA_RTC_SEC_CNT_UPD		(RTC_BASE + 0x10)
#define ANA_RTC_MIN_CNT_UPD		(RTC_BASE + 0x14)
#define ANA_RTC_HOUR_CNT_UPD		(RTC_BASE + 0x18)
#define ANA_RTC_DAY_CNT_UPD		(RTC_BASE + 0x1C)

#define ANA_RTC_SEC_ALM_UPD		(RTC_BASE + 0x20)
#define ANA_RTC_MIN_ALM_UPD		(RTC_BASE + 0x24)
#define ANA_RTC_HOUR_ALM_UPD		(RTC_BASE + 0x28)
#define ANA_RTC_DAY_ALM_UPD		(RTC_BASE + 0x2C)

#define ANA_RTC_INT_EN			(RTC_BASE + 0x30)
#define ANA_RTC_INT_RAW_STS		(RTC_BASE + 0x34)
#define ANA_RTC_INT_CLR			(RTC_BASE + 0x38)
#define ANA_RTC_INT_MASK_STS		(RTC_BASE + 0x3C)

#define ANA_RTC_SEC_ALM_VALUE		(RTC_BASE + 0x40)
#define ANA_RTC_MIN_ALM_VALUE		(RTC_BASE + 0x44)
#define ANA_RTC_HOUR_ALM_VALUE		(RTC_BASE + 0x48)
#define ANA_RTC_DAY_ALM_VALUE		(RTC_BASE + 0x4C)

#define ANA_RTC_SPG_VALUE		(RTC_BASE + 0x50)
#define ANA_RTC_SPG_UPD			(RTC_BASE + 0x54)

#define ANA_RTC_SEC_AUXALM_UPD		(RTC_BASE + 0x60)
#define ANA_RTC_MIN_AUXALM_UPD		(RTC_BASE + 0x64)
#define ANA_RTC_HOUR_AUXALM_UPD		(RTC_BASE + 0x68)
#define ANA_RTC_DAY_AUXALM_UPD		(RTC_BASE + 0x6C)

/* The corresponding bit of RTC_CTL register. */
#define RTC_SEC_BIT			BIT(0)        /* Sec int enable */
#define RTC_MIN_BIT			BIT(1)        /* Min int enable */
#define RTC_HOUR_BIT			BIT(2)        /* Hour int enable */
#define RTC_DAY_BIT			BIT(3)        /* Day int enable */
#define RTC_ALARM_BIT			BIT(4)        /* Alarm int enable */
#define RTCCTL_HOUR_FMT_SEL		BIT(5)        /* Hour format select */
#define RTC_AUX_ALARM_BIT		BIT(6)	      /* Auxilible alarm enable */
#define RTC_SPG_BIT			BIT(7)	      /* SPG int enable */
#define RTC_SEC_ACK_BIT			BIT(8)        /* Sec ack int enable */
#define RTC_MIN_ACK_BIT			BIT(9)        /* Min ack int enable */
#define RTC_HOUR_ACK_BIT		BIT(10)       /* Hour ack int enable */
#define RTC_DAY_ACK_BIT			BIT(11)       /* Day ack int enable */
#define RTC_SEC_ALM_ACK_BIT		BIT(12)       /* Sec alm ack int enable */
#define RTC_MIN_ALM_ACK_BIT		BIT(13)       /* Min alm ack int enable */
#define RTC_HOUR_ALM_ACK_BIT		BIT(14)       /* Hour alm ack int enable */
#define RTC_DAY_ALM_ACK_BIT		BIT(15)       /* Day alm ack int enable */

#define RTC_UPD_TIME_MASK		(RTC_SEC_ACK_BIT | RTC_MIN_ACK_BIT | RTC_HOUR_ACK_BIT | RTC_DAY_ACK_BIT)
#define RTC_INT_ALL_MSK			(0xFFFF&(~(BIT(5)|BIT(7))))
#define RTC_INT_ALM_MSK			(RTC_SEC_BIT | RTC_MIN_BIT | RTC_HOUR_BIT | RTC_DAY_BIT | RTC_ALARM_BIT | RTC_AUX_ALARM_BIT)
#define RTC_ALM_TIME_MASK		(RTC_SEC_ALM_ACK_BIT | RTC_MIN_ALM_ACK_BIT | RTC_HOUR_ALM_ACK_BIT | RTC_DAY_ALM_ACK_BIT)

#define RTC_SEC_MASK			(0x3F)
#define RTC_MIN_MASK			(0x3F)
#define RTC_HOUR_MASK			(0x1F)
#define RTC_DAY_MASK			(0xFFFF)

#define SPRD_RTC_GET_MAX		(10)
#define SPRD_RTC_SET_MAX		(150)
#define SPRD_RTC_ALM_UNLOCK		(0xa5)
#define SPRD_RTC_ALM_LOCK		(0xff & (~SPRD_RTC_ALM_UNLOCK))
#define SPRD_RTC_ALMLOCK_MASK		(0xff)

#define SPRD_RTC_POWEROFF_ALARM		(0x1 << 8)
#define SPRD_RTC_POWERDOWN_RESET	(0x1 << 9)

/* check power down reg */
#define RTC_POWERDOWN_VAL		(0xA596)
#define RTC_RST_CLEAN_VAL		(0xFF)
#define ANA_RTC_RST2			(ANA_CTL_GLB_BASE + 0x164)
#define ANA_RTC_RST1			(ANA_CTL_GLB_BASE + 0x160)

struct sprd_rtc{
	struct rtc_device 	*rtc;
	unsigned int 		irq_no;
	struct clk 			*clk;
	struct regulator 	*regulator;
};

enum ALARM_TYPE {
	SET_POWERON_ALARM = 0,
	GET_POWERON_ALARM,
	SET_WAKE_ALARM,
	GET_WAKE_ALARM,
	SET_POWEROFF_ALARM,
	GET_POWEROFF_ALARM,
};

static struct wake_lock rtc_wake_lock;
static struct wake_lock rtc_interrupt_wake_lock;
static unsigned long secs_start_year_to_1970;

static ssize_t sprd_show_caliberate(struct device *dev,
	struct device_attribute *attr, char *buf);

#define SPRD_CALIBERATE_ATTR_RO(_name)			\
{							\
	.attr = { .name = #_name, .mode = S_IRUGO, },	\
	.show = sprd_show_caliberate,			\
}

static struct device_attribute sprd_caliberate[] = {
	SPRD_CALIBERATE_ATTR_RO(default_time),
};

static inline unsigned int get_sec(void)
{
	return sci_adi_read((unsigned long)ANA_RTC_SEC_CNT_VALUE) & RTC_SEC_MASK;
}

static inline unsigned int get_min(void)
{
	return sci_adi_read((unsigned long)ANA_RTC_MIN_CNT_VALUE) & RTC_MIN_MASK;
}

static inline unsigned int get_hour(void)
{
	return sci_adi_read((unsigned long)ANA_RTC_HOUR_CNT_VALUE) & RTC_HOUR_MASK;
}

static inline unsigned int get_day(void)
{
	return sci_adi_read((unsigned long)ANA_RTC_DAY_CNT_VALUE) & RTC_DAY_MASK;
}

static int clear_rtc_int(unsigned int mask)
{
	return sci_adi_raw_write(ANA_RTC_INT_CLR, mask);
}

static unsigned long sprd_rtc_get_sec(void)
{
	unsigned int sec, min, hour, day;
	unsigned long second;

	sec = get_sec();
	min = get_min();
	hour = get_hour();
	day = get_day();

	second = (((unsigned long)(day*24) + hour)*60 + min)*60 + sec;

	return second;
}

static int sprd_rtc_set_sec(unsigned long secs)
{
	unsigned int sec, min, hour, day;
	unsigned int int_rsts;
	unsigned long temp;
	int cnt = 0, ret = 0;

	sec = secs % 60;
	temp = (secs - sec)/60;
	min = temp%60;
	temp = (temp - min)/60;
	hour = temp%24;
	temp = (temp - hour)/24;
	day = temp;

	sci_adi_set((unsigned long)ANA_RTC_INT_CLR, RTC_UPD_TIME_MASK);
	sci_adi_raw_write((unsigned long)ANA_RTC_SEC_CNT_UPD, sec);
	sci_adi_raw_write((unsigned long)ANA_RTC_MIN_CNT_UPD, min);
	sci_adi_raw_write((unsigned long)ANA_RTC_HOUR_CNT_UPD, hour);
	sci_adi_raw_write((unsigned long)ANA_RTC_DAY_CNT_UPD, day);

	/* wait till all update done */
	do{
		int_rsts = sci_adi_read((unsigned long)ANA_RTC_INT_RAW_STS) & RTC_UPD_TIME_MASK;

		if(RTC_UPD_TIME_MASK == int_rsts)
			break;

		msleep(10);
	}while(cnt++ < SPRD_RTC_SET_MAX);

	if(cnt >= SPRD_RTC_SET_MAX){
		printk(KERN_ERR "Set time values time out!\n");
		ret = -EBUSY;
	}

	sci_adi_set((unsigned long)ANA_RTC_INT_CLR, RTC_UPD_TIME_MASK);
	return ret;
}

static inline unsigned long sprd_rtc_get_alarm_sec(void)
{
	unsigned int sec, min, hour, day;
	unsigned long alarm_sec = 0;

	day = sci_adi_read((unsigned long)ANA_RTC_DAY_ALM_VALUE) & RTC_DAY_MASK;
	hour = sci_adi_read((unsigned long)ANA_RTC_HOUR_ALM_VALUE) & RTC_HOUR_MASK;
	min = sci_adi_read((unsigned long)ANA_RTC_MIN_ALM_VALUE) & RTC_MIN_MASK;
	sec = sci_adi_read((unsigned long)ANA_RTC_SEC_ALM_VALUE) & RTC_SEC_MASK;

	alarm_sec = (((unsigned long)(day*24) + hour)*60 + min)*60 + sec;
	return alarm_sec;
}

static int sprd_rtc_set_alarm_sec(unsigned long secs)
{
	unsigned int sec, min, hour, day;
	unsigned int timeout = 0;
	unsigned long temp;
	static bool rtc_alarm_in_update = false;

	sec = secs % 60;
	temp = (secs - sec)/60;
	min = temp%60;
	temp = (temp - min)/60;
	hour = temp%24;
	temp = (temp - hour)/24;
	day = temp;

	/* Optimize suspend process time */
	if (rtc_alarm_in_update) {
		while ((sci_adi_read((unsigned long)ANA_RTC_INT_RAW_STS) & RTC_ALM_TIME_MASK) !=
			RTC_ALM_TIME_MASK)
		{
			msleep(10);
			if (timeout++ > SPRD_RTC_SET_MAX) {
				printk("RTC set alarm timeout!\n");
				break;
			}
		}
	}
	sci_adi_set((unsigned long)ANA_RTC_INT_CLR, RTC_ALM_TIME_MASK);

	sci_adi_raw_write((unsigned long)ANA_RTC_SEC_ALM_UPD, sec);
	sci_adi_raw_write((unsigned long)ANA_RTC_MIN_ALM_UPD, min);
	sci_adi_raw_write((unsigned long)ANA_RTC_HOUR_ALM_UPD, hour);
	sci_adi_raw_write((unsigned long)ANA_RTC_DAY_ALM_UPD, day);

	rtc_alarm_in_update = true;
	return 0;
}

static int sprd_rtc_read_alarm(struct device *dev,
	struct rtc_wkalrm *alrm)
{
	unsigned long secs = sprd_rtc_get_alarm_sec();

	secs = secs + secs_start_year_to_1970;
	rtc_time_to_tm(secs, &alrm->time);

	alrm->enabled = !!(sci_adi_read((unsigned long)ANA_RTC_INT_EN) & RTC_ALARM_BIT);
	alrm->pending = !!(sci_adi_read((unsigned long)ANA_RTC_INT_RAW_STS) & RTC_ALARM_BIT);

	return 0;
}

static int sprd_rtc_read_poweroff_alarm(struct device *dev,
	struct rtc_wkalrm *alrm)
{
	return sprd_rtc_read_alarm(dev, alrm);
}

static inline unsigned long sprd_rtc_get_aux_alarm_sec(void)
{
	unsigned int sec, min, hour, day;
	unsigned long aux_alrm_sec;

	day = sci_adi_read((unsigned long)ANA_RTC_DAY_AUXALM_UPD) & RTC_DAY_MASK;
	hour = sci_adi_read((unsigned long)ANA_RTC_HOUR_AUXALM_UPD) & RTC_HOUR_MASK;
	min = sci_adi_read((unsigned long)ANA_RTC_MIN_AUXALM_UPD) & RTC_MIN_MASK;
	sec = sci_adi_read((unsigned long)ANA_RTC_SEC_AUXALM_UPD) & RTC_SEC_MASK;

	aux_alrm_sec = (((day*24) + hour)*60 + min)*60 + sec;
	return aux_alrm_sec;
}

static int sprd_rtc_set_aux_alarm_sec(unsigned long secs)
{
	unsigned int sec, min, hour, day;
	unsigned long temp;

	sec = secs % 60;
	temp = (secs - sec)/60;
	min = temp%60;
	temp = (temp - min)/60;
	hour = temp%24;
	temp = (temp - hour)/24;
	day = temp;

	sci_adi_set((unsigned long)ANA_RTC_INT_CLR, RTC_AUX_ALARM_BIT);
	sci_adi_raw_write((unsigned long)ANA_RTC_SEC_AUXALM_UPD, sec);
	sci_adi_raw_write((unsigned long)ANA_RTC_MIN_AUXALM_UPD, min);
	sci_adi_raw_write((unsigned long)ANA_RTC_HOUR_AUXALM_UPD, hour);
	sci_adi_raw_write((unsigned long)ANA_RTC_DAY_AUXALM_UPD, day);

	return 0;
}

static int sprd_rtc_read_aux_alarm(struct device *dev,
	struct rtc_wkalrm *alrm)
{
	unsigned long secs = sprd_rtc_get_aux_alarm_sec();

	secs = secs + secs_start_year_to_1970;
	rtc_time_to_tm(secs, &alrm->time);

	alrm->enabled = !!(sci_adi_read((unsigned long)ANA_RTC_INT_EN) & RTC_AUX_ALARM_BIT);
	alrm->pending = !!(sci_adi_read((unsigned long)ANA_RTC_INT_RAW_STS) & RTC_AUX_ALARM_BIT);

	return 0;
}

static int sprd_rtc_set_aux_alarm(struct device *dev,
	struct rtc_wkalrm *alrm)
{
	unsigned long secs;
	unsigned int int_stat;

	sci_adi_raw_write(ANA_RTC_INT_CLR, RTC_AUX_ALARM_BIT);

	if(alrm->enabled){
		rtc_tm_to_time(&alrm->time, &secs);
		if(secs < secs_start_year_to_1970){
			printk(KERN_ERR "Can't set alarm value,need set the right time!\n");
			return -1;
		}

		/*Notice! need to deal with normal alarm and aux alarm*/
		int_stat = sci_adi_read((unsigned long)ANA_RTC_INT_EN);
		int_stat |= RTC_AUX_ALARM_BIT;
		sci_adi_raw_write((unsigned long)ANA_RTC_INT_EN, int_stat);

		secs = secs - secs_start_year_to_1970;
		wake_lock(&rtc_wake_lock);
		sprd_rtc_set_aux_alarm_sec(secs);
		/*unlock the rtc alrm int*/
		wake_unlock(&rtc_wake_lock);
	}else{
		sci_adi_clr((unsigned long)ANA_RTC_INT_EN, RTC_AUX_ALARM_BIT);
	}

	return 0;
}

static int sprd_rtc_set_alarm(struct device *dev,
	struct rtc_wkalrm *alrm)
{
	unsigned long secs;
	unsigned int int_stat, spg_val;

	sci_adi_raw_write((unsigned long)ANA_RTC_INT_CLR, RTC_ALARM_BIT);

	if(alrm->enabled){
		rtc_tm_to_time(&alrm->time, &secs);
		if(secs < secs_start_year_to_1970){
			printk(KERN_ERR "Can't set alarm value,need set the right time!\n");
			return -1;
		}

		/* Notice! need to deal with normal alarm and aux alarm */
		int_stat = sci_adi_read((unsigned long)ANA_RTC_INT_EN);
		int_stat |= RTC_ALARM_BIT;
		sci_adi_raw_write((unsigned long)ANA_RTC_INT_EN, int_stat);

		secs = secs - secs_start_year_to_1970;
		wake_lock(&rtc_wake_lock);
		sprd_rtc_set_alarm_sec(secs);

		/* unlock the rtc alrm int */
		spg_val = sci_adi_read((unsigned long)ANA_RTC_SPG_VALUE);
		spg_val &= (~(SPRD_RTC_ALMLOCK_MASK | SPRD_RTC_POWEROFF_ALARM));
		spg_val |= SPRD_RTC_ALM_UNLOCK;
		sci_adi_raw_write((unsigned long)ANA_RTC_SPG_UPD, spg_val);
		wake_unlock(&rtc_wake_lock);
	}else{
		sci_adi_clr(ANA_RTC_INT_EN, RTC_ALARM_BIT);

		/* lock the rtc alrm int */
		spg_val = sci_adi_read((unsigned long)ANA_RTC_SPG_VALUE);
		spg_val &= (~(SPRD_RTC_ALMLOCK_MASK | SPRD_RTC_POWEROFF_ALARM));
		spg_val |= SPRD_RTC_ALM_LOCK;
		sci_adi_raw_write((unsigned long)ANA_RTC_SPG_UPD, spg_val);
		msleep(150);
	}

	return 0;
}

static int sprd_rtc_set_poweroff_alarm(struct device *dev,
	struct rtc_wkalrm *alrm)
{
	unsigned int spg_val;

	int ret;

	ret = sprd_rtc_set_alarm(dev, alrm);
	if (ret) { /* set alarm error */
		return ret;
	} else {
		if (alrm->enabled) {
			spg_val = sci_adi_read((unsigned long)ANA_RTC_SPG_VALUE);
			spg_val &= (~SPRD_RTC_ALMLOCK_MASK);
			spg_val |= (SPRD_RTC_ALM_UNLOCK |
				SPRD_RTC_POWEROFF_ALARM);
			sci_adi_raw_write((unsigned long)ANA_RTC_SPG_UPD, spg_val);
			msleep(150);
		}
	}

	return 0;
}

static int sprd_rtc_read_time(struct device *dev,
	struct rtc_time *tm)
{
	unsigned long secs = sprd_rtc_get_sec();

	secs = secs + secs_start_year_to_1970;

	if (secs > 0x7f000000)
		secs = secs_start_year_to_1970;

	rtc_time_to_tm(secs, tm);

	return rtc_valid_tm(tm);
}

static int sprd_rtc_set_time(struct device *dev,
	struct rtc_time *tm)
{
	unsigned long secs;
	int ret = 0;

	rtc_tm_to_time(tm, &secs);
	if(secs < secs_start_year_to_1970){
		printk(KERN_ERR "Can't set time,need set the right time!\n");
		return -1;
	}

	secs = secs - secs_start_year_to_1970;
	ret = sprd_rtc_set_sec(secs);
	return ret;
}

static int sprd_rtc_set_mmss(struct device *dev, unsigned long secs)
{
	int ret = 0;

	if(secs < secs_start_year_to_1970){
		printk(KERN_ERR "Can't set mmss,need set the right time!\n");
		return -1;
	}

	secs = secs - secs_start_year_to_1970;
	ret = sprd_rtc_set_sec(secs);
	return ret;
}

#if defined(CONFIG_ADIE_SC2723) || defined(CONFIG_ADIE_SC2723S)
static int sprd_rtc_check_power_down(struct device *dev)
{
	int rst_value;
	int ret;
	unsigned long secs;
	struct rtc_time tm;

	rst_value = sci_adi_read((unsigned long)ANA_RTC_RST2);
	if(rst_value == RTC_POWERDOWN_VAL){
		/* clear ANA_RTC_RST2 */
		sci_adi_raw_write((unsigned long)ANA_RTC_RST1, RTC_RST_CLEAN_VAL);
		printk("RTC power down and reset RTC time!\n");
		secs = mktime(CONFIG_RTC_START_YEAR, 1, 1, 0, 0, 0);
		rtc_time_to_tm(secs, &tm);
		if((ret = sprd_rtc_set_time(dev,&tm)) < 0)
			return ret;
	}
	return 0;
}
#else
/*
 * check current time
 */
static int sprd_check_current_time(void)
{
	unsigned long secs;
	struct rtc_time tm;

	secs = sprd_rtc_get_sec();
	secs = secs + secs_start_year_to_1970;
	rtc_time_to_tm(secs, &tm);

	rtc_tm_to_time(&tm, &secs);
	if ((secs < secs_start_year_to_1970) || (secs > 0x7f000000))
		return -1;
	else
		return 0;
}

static int sprd_rtc_check_power_down(struct device *dev)
{
	unsigned int spg_val;
	unsigned long secs;
	struct rtc_time tm;
	unsigned int_stat = 0;
	int ret = 0, cnt = 0;

	spg_val = sci_adi_read((unsigned long)ANA_RTC_SPG_VALUE);
	if (((spg_val & SPRD_RTC_POWERDOWN_RESET) == 0) ||
		sprd_check_current_time()) {
		pr_info("RTC power down and reset RTC time!!\n");
		secs = mktime(CONFIG_RTC_START_YEAR, 1, 1, 0, 0, 0);
		rtc_time_to_tm(secs, &tm);
		if ((ret = sprd_rtc_set_time(dev,&tm)) < 0)
			return ret;

		spg_val |= SPRD_RTC_POWERDOWN_RESET;
		sci_adi_raw_write((unsigned long)ANA_RTC_SPG_UPD, spg_val);
		/* wait till SPG value update done */
		do {
			int_stat = sci_adi_read((unsigned long)ANA_RTC_INT_RAW_STS) & RTC_SPG_BIT;
			if (RTC_SPG_BIT == int_stat)
				break;
			msleep(10);
		} while (cnt++ < SPRD_RTC_SET_MAX);

		if (cnt >= SPRD_RTC_SET_MAX){
			pr_err("reset RTC time error.\n");
			ret = -EBUSY;
		}
	}
	return ret;
}
#endif

static int sprd_rtc_proc(struct device *dev, struct seq_file *seq)
{
	struct platform_device *plat_dev = to_platform_device(dev);

	seq_printf(seq, "sprd_rtc\t: yes\n");
	seq_printf(seq, "id\t\t: %d\n", plat_dev->id);

	return 0;
}

void rtc_aie_update_irq(void *private);
static irqreturn_t rtc_interrupt_handler(int irq, void *dev_id)
{
	struct rtc_device *rdev = dev_id;

	pr_info("RTC ***** interrupt happen\n");
	//rtc_update_irq(rdev, 1, RTC_AF | RTC_IRQF);
	wake_lock_timeout(&rtc_interrupt_wake_lock,2*HZ);
	rtc_aie_update_irq(rdev);
	clear_rtc_int(RTC_INT_ALM_MSK);
	return IRQ_HANDLED;
}

static ssize_t sprd_show_caliberate(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	ssize_t retval;
	retval = sprintf(buf, "%lu\n", secs_start_year_to_1970);
	return retval;
}

static int sprd_creat_caliberate_attr(struct device dev)
{
	int i, rc;

	for (i = 0; i < ARRAY_SIZE(sprd_caliberate); i++) {
		rc = device_create_file(&dev, &sprd_caliberate[i]);
		if (rc)
			goto sprd_attrs_failed;
	}
	goto sprd_attrs_succeed;

sprd_attrs_failed:
	while (i--)
		device_remove_file(&dev, &sprd_caliberate[i]);

sprd_attrs_succeed:
	return rc;
}

static int sprd_remove_caliberate_attr(struct device dev)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(sprd_caliberate); i++) {
		device_remove_file(&dev, &sprd_caliberate[i]);
	}
	return 0;
}

static int sprd_rtc_open(struct device *dev)
{
	unsigned int_stat = 0;
	int ret = 0;

	/* enable rtc alarm interrupt */
	int_stat = sci_adi_read(ANA_RTC_INT_EN);
	int_stat |= RTC_ALARM_BIT | RTC_AUX_ALARM_BIT;
	sci_adi_raw_write(ANA_RTC_INT_EN, int_stat);

	printk("RTC open is calling!\n");

	return ret;
}

static int sprd_rtc_ioctl(struct device *dev, unsigned int cmd, unsigned long arg)
{
	int ret = 0;

	switch(cmd){
	case SET_POWERON_ALARM:
		ret = sprd_rtc_set_alarm(dev,(struct rtc_wkalrm *)arg);
		break;
	case GET_POWERON_ALARM:
		ret = sprd_rtc_read_alarm(dev,(struct rtc_wkalrm *)arg);
		break;
	case SET_WAKE_ALARM:
		ret = sprd_rtc_set_aux_alarm(dev,(struct rtc_wkalrm *)arg);
		break;
	case GET_WAKE_ALARM:
		ret = sprd_rtc_read_aux_alarm(dev,(struct rtc_wkalrm *)arg);
		break;
	case SET_POWEROFF_ALARM:
		ret = sprd_rtc_set_poweroff_alarm(dev,(struct rtc_wkalrm *)arg);
		break;
	case GET_POWEROFF_ALARM:
		ret = sprd_rtc_read_poweroff_alarm(dev,(struct rtc_wkalrm *)arg);
		break;
	default:
		return -EINVAL;
	}
	return ret;
}

static const struct rtc_class_ops sprd_rtc_ops = {
	.open = sprd_rtc_open,
	.proc = sprd_rtc_proc,
	.read_time = sprd_rtc_read_time,
	.read_alarm = sprd_rtc_read_alarm,
	.set_time = sprd_rtc_set_time,
	.set_alarm = sprd_rtc_set_alarm,
	.set_mmss = sprd_rtc_set_mmss,
	.ioctl = sprd_rtc_ioctl,
};

static int sprd_rtc_probe(struct platform_device *plat_dev)
{
	struct resource *res;
	static struct sprd_rtc *rtc_dev;
	int ret = 0;

	rtc_dev = kzalloc(sizeof(*rtc_dev), GFP_KERNEL);
	if(IS_ERR(rtc_dev)){
		ret = PTR_ERR(rtc_dev);
		return ret;
	};

	/* enable rtc device */
	rtc_dev->clk = clk_get(&plat_dev->dev, "ext_32k");
	if (IS_ERR(rtc_dev->clk)) {
		ret = PTR_ERR(rtc_dev->clk);
		goto kfree_data;
	}

	ret = clk_prepare_enable(rtc_dev->clk);
	if (ret < 0)
		goto put_clk;

	sprd_rtc_check_power_down(&plat_dev->dev);

	/*disable and clean irq*/
	sci_adi_clr(ANA_RTC_INT_EN, 0xffff);
	clear_rtc_int(RTC_INT_ALL_MSK);

	device_init_wakeup(&plat_dev->dev, 1);
	rtc_dev->rtc = rtc_device_register("sprd_rtc", &plat_dev->dev,
		&sprd_rtc_ops, THIS_MODULE);
	if (IS_ERR(rtc_dev->rtc)) {
		ret = PTR_ERR(rtc_dev->rtc);
		goto disable_clk;
	}

	res = platform_get_resource(plat_dev, IORESOURCE_IRQ, 0);
	if(unlikely(!res)) {
		dev_err(&plat_dev->dev, "no irq resource specified\n");
		goto unregister_rtc;
	}
	rtc_dev->irq_no = res->start;

	platform_set_drvdata(plat_dev, rtc_dev);

	ret = request_threaded_irq(rtc_dev->irq_no, rtc_interrupt_handler,
		NULL, 0, "sprd_rtc", rtc_dev->rtc);
	if(ret){
		printk(KERN_ERR "RTC regist irq error\n");
		goto unregister_rtc;
	}
	sprd_creat_caliberate_attr(rtc_dev->rtc->dev);

	return 0;

unregister_rtc:
	rtc_device_unregister(rtc_dev->rtc);
disable_clk:
	clk_disable_unprepare(rtc_dev->clk);
put_clk:
	clk_put(rtc_dev->clk);
kfree_data:
	kfree(rtc_dev);
	return ret;
}

static int sprd_rtc_remove(struct platform_device *plat_dev)
{
	struct sprd_rtc *rtc_dev = platform_get_drvdata(plat_dev);
	sprd_remove_caliberate_attr(rtc_dev->rtc->dev);
	rtc_device_unregister(rtc_dev->rtc);
	clk_disable_unprepare(rtc_dev->clk);
	clk_put(rtc_dev->clk);
	kfree(rtc_dev);

	return 0;
}

static struct of_device_id sprd_rtc_match_table[] = {
	{ .compatible = "sprd,rtc", },
	{ },
};

static struct platform_driver sprd_rtc_driver = {
	.probe	= sprd_rtc_probe,
	.remove = sprd_rtc_remove,
	.driver = {
		.name = "sprd_rtc",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(sprd_rtc_match_table),
	},
};

static int __init sprd_rtc_init(void)
{
	int err;
	struct rtc_time read_tm;

	wake_lock_init(&rtc_wake_lock, WAKE_LOCK_SUSPEND, "rtc");
	wake_lock_init(&rtc_interrupt_wake_lock, WAKE_LOCK_SUSPEND, "rtc_interrupt");

	if(CONFIG_RTC_START_YEAR > 1970)
		secs_start_year_to_1970 = mktime(CONFIG_RTC_START_YEAR, 1, 1, 0, 0, 0);
	else
		secs_start_year_to_1970 = mktime(1970, 1, 1, 0, 0, 0);

	if ((err = platform_driver_register(&sprd_rtc_driver))) {
		wake_lock_destroy(&rtc_wake_lock);
		wake_lock_destroy(&rtc_interrupt_wake_lock);
		return err;
	}

	sprd_rtc_read_time(NULL,&read_tm);
	printk("After init RTC time: %d-%d-%d %d:%d:%d\n",read_tm.tm_year+1900,
		read_tm.tm_mon+1,read_tm.tm_mday,read_tm.tm_hour,read_tm.tm_min,read_tm.tm_sec);
	return 0;
}

static void __exit sprd_rtc_exit(void)
{
	platform_driver_unregister(&sprd_rtc_driver);
	wake_lock_destroy(&rtc_interrupt_wake_lock);
	wake_lock_destroy(&rtc_wake_lock);
}

MODULE_AUTHOR("Baolin.wang@spreadtrum.com");
MODULE_DESCRIPTION("RTC driver/device");
MODULE_LICENSE("GPL");

module_init(sprd_rtc_init);
module_exit(sprd_rtc_exit);
