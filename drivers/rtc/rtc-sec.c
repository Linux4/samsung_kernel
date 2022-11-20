/*
 * rtc-sec.c
 *
 * Copyright (c) 2012 Samsung Electronics Co., Ltd
 *              http://www.samsung.com
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 */
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/bcd.h>
#include <linux/rtc.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/mfd/samsung/core.h>
#include <linux/mfd/samsung/rtc.h>
#include <linux/mfd/samsung/s2mps11.h>
#include <linux/mfd/samsung/s2mpa01.h>
#include <linux/mfd/samsung/s2mpu01.h>
#include <linux/mfd/samsung/s2mpu01a.h>
#include <linux/mfd/samsung/irq.h>
#include <linux/regmap.h>
#if defined(CONFIG_RTC_ALARM_BOOT)
#include <linux/reboot.h>
#include <linux/wakelock.h>
#endif

struct s2m_rtc_info {
	struct device *dev;
	struct sec_pmic_dev *iodev;
	struct rtc_device *rtc_dev;
	int irq;
#if defined(CONFIG_RTC_ALARM_BOOT)
	int irq1;
	bool lpm_mode;
	bool alarm_irq_flag;
	struct wake_lock alarm_wake_lock;
	struct workqueue_struct*  sapa_workq;
	struct delayed_work       sapa_reboot_work;
#endif
	int rtc_24hr_mode;
	bool wtsr_smpl;
};

static inline int s2m_rtc_calculate_wday(u8 shifted)
{
	int counter = -1;

	while (shifted) {
		shifted >>= 1;
		counter++;
	}
	return counter;
}

static void s2m_data_to_tm(u8 *data, struct rtc_time *tm,
				int rtc_24hr_mode)
{
	tm->tm_sec = data[RTC_SEC] & 0x7f;
	tm->tm_min = data[RTC_MIN] & 0x7f;

	if (rtc_24hr_mode) {
		tm->tm_hour = data[RTC_HOUR] & 0x1f;
	} else {
		tm->tm_hour = data[RTC_HOUR] & 0x0f;
		if (data[RTC_HOUR] & HOUR_PM_MASK)
			tm->tm_hour += 12;
	}

	tm->tm_wday = s2m_rtc_calculate_wday(data[RTC_WEEKDAY] & 0x7f);
	tm->tm_mday = data[RTC_DATE] & 0x1f;
	tm->tm_mon = (data[RTC_MONTH] & 0x0f) - 1;
	tm->tm_year = (data[RTC_YEAR1] & 0x7f)+100;
	tm->tm_yday = 0;
	tm->tm_isdst = 0;
}

static void s2m_tm_to_data(struct rtc_time *tm, u8 *data)
{
	data[RTC_SEC] = tm->tm_sec;
	data[RTC_MIN] = tm->tm_min;

	if (tm->tm_hour >= 12)
		data[RTC_HOUR] = tm->tm_hour | HOUR_PM_MASK;
	else
		data[RTC_HOUR] = tm->tm_hour & ~HOUR_PM_MASK;

	data[RTC_WEEKDAY] = 1 << tm->tm_wday;
	data[RTC_DATE] = tm->tm_mday;
	data[RTC_MONTH] = tm->tm_mon + 1;
	data[RTC_YEAR1] = tm->tm_year > 100 ? (tm->tm_year - 100) : 0 ;
}

static inline int s2m_rtc_set_time_reg(struct s2m_rtc_info *info,
					   int alarm_enable)
{
	int ret;
	unsigned int data;

	ret = sec_rtc_read(info->iodev, S2M_RTC_UPDATE, &data);
	if (ret < 0)
		return ret;

	data |= RTC_WUDR_MASK;

	if (alarm_enable)
		data |= RTC_RUDR_MASK;
	else
		data &= ~RTC_RUDR_MASK;

	ret = sec_rtc_write(info->iodev, S2M_RTC_UPDATE, (char)data);

	if (ret < 0) {
		dev_err(info->dev, "%s: fail to write update reg(%d)\n",
				__func__, ret);
	} else {
		usleep_range(1000, 1000);
	}

	return ret;
}

static inline int s2m_rtc_read_time_reg(struct s2m_rtc_info *info)
{
	int ret;
	unsigned int data;

	ret = sec_rtc_read(info->iodev, S2M_RTC_UPDATE, &data);
	if (ret < 0)
		return ret;

	data |= RTC_RUDR_MASK;
	data &= ~RTC_WUDR_MASK;

	ret = sec_rtc_write(info->iodev, S2M_RTC_UPDATE, (char)data);
	if (ret < 0) {
		dev_err(info->dev, "%s: fail to write update reg(%d)\n",
			__func__, ret);
	} else {
		usleep_range(1000, 1000);
	}
	return ret;
}

static int s2m_rtc_read_time(struct device *dev, struct rtc_time *tm)
{
	struct s2m_rtc_info *info = dev_get_drvdata(dev);
	u8 data[7];
	int ret;

	ret = s2m_rtc_read_time_reg(info);
	if (ret < 0)
		return ret;

	ret = sec_rtc_bulk_read(info->iodev, S2M_RTC_SEC, 7, data);
	if (ret < 0)
		return ret;

	s2m_data_to_tm(data, tm, info->rtc_24hr_mode);

	printk(KERN_INFO "%s: %d/%d/%d %d:%d:%d(%d)\n", __func__,
		1900 + tm->tm_year, 1 + tm->tm_mon, tm->tm_mday,
		tm->tm_hour, tm->tm_min, tm->tm_sec, tm->tm_wday);

	return rtc_valid_tm(tm);
}

static int s2m_rtc_set_time(struct device *dev, struct rtc_time *tm)
{
	struct s2m_rtc_info *info = dev_get_drvdata(dev);
	u8 data[7];
	int ret;

	s2m_tm_to_data(tm, data);

	printk(KERN_INFO "%s: %d/%d/%d %d:%d:%d(%d)\n", __func__,
		1900 + tm->tm_year, 1 + tm->tm_mon, tm->tm_mday,
		tm->tm_hour, tm->tm_min, tm->tm_sec, tm->tm_wday);

	ret = sec_rtc_bulk_write(info->iodev, S2M_RTC_SEC, 7, data);
	if (ret < 0)
		return ret;

	ret = s2m_rtc_set_time_reg(info, 0);

	return ret;
}

static int s2m_rtc_read_alarm(struct device *dev, struct rtc_wkalrm *alrm)
{
	struct s2m_rtc_info *info = dev_get_drvdata(dev);
	u8 data[7];
	unsigned int val;
	int ret, i, id;

	ret = sec_rtc_bulk_read(info->iodev, S2M_ALARM0_SEC, 7, data);
	if (ret < 0)
		return ret;

	s2m_data_to_tm(data, &alrm->time, info->rtc_24hr_mode);

	printk(KERN_INFO "%s: %d/%d/%d %d:%d:%d(%d)\n", __func__,
		1900 + alrm->time.tm_year, 1 + alrm->time.tm_mon,
		alrm->time.tm_mday, alrm->time.tm_hour,
		alrm->time.tm_min, alrm->time.tm_sec,
		alrm->time.tm_wday);

	alrm->enabled = 0;

	for (i = 0; i < 7; i++) {
		if (data[i] & ALARM_ENABLE_MASK) {
			alrm->enabled = 1;
			break;
		}
	}

	alrm->pending = 0;
	switch (info->iodev->device_type) {
	case S2MPS11X:
		ret = sec_reg_read(info->iodev, S2MPS11_REG_ST2, &val);
		break;

	case S2MPA01:
		ret = sec_reg_read(info->iodev, S2MPA01_REG_ST2, &val);
		break;

	case S2MPU01X:
		/* A problem of read operation, needs workaround */
		ret = sec_reg_read(info->iodev, S2MPU01_REG_ID, &id);
		if (ret < 0)
			return ret;
		if (id == 0)
			ret = sec_reg_read(info->iodev, S2MPU01_REG_ST2 -1 , &val);
		else
			ret = sec_reg_read(info->iodev, S2MPU01_REG_ST2, &val);
		break;
	case S2MPU01A:
		/* A problem of read operation, needs workaround */
		ret = sec_reg_read(info->iodev, S2MPU01A_REG_ID, &id);
		if (ret < 0)
			return ret;
		if (id == 0)
			ret = sec_reg_read(info->iodev, S2MPU01A_REG_ST2 - 1 , &val);
		else
			ret = sec_reg_read(info->iodev, S2MPU01A_REG_ST2, &val);
		break;
	default:
		/* If this happens the core funtion has a problem */
		BUG();
	}
	if (ret < 0)
		return ret;

	if (val & RTCA0E)
		alrm->pending = 1;
	else
		alrm->pending = 0;

	return 0;
}

static int s2m_rtc_workaround_operation(struct s2m_rtc_info *info, u8* data)
{
	int ret;

	/* RUDR */
	ret = s2m_rtc_read_time_reg(info);
	if (ret < 0)
		return ret;

	/* Read time */
	ret = sec_rtc_bulk_read(info->iodev, S2M_RTC_SEC, 7, data);
	if (ret < 0)
		return ret;

	/* Write time */
	ret = sec_rtc_bulk_write(info->iodev, S2M_RTC_SEC, 7, data);
	return ret;
}

static int s2m_rtc_stop_alarm(struct s2m_rtc_info *info)
{
	u8 data[7];
	int ret, i;
	struct rtc_time tm;

	switch (info->iodev->device_type) {
	case S2MPS11X:
		if (info->iodev->rev_num <= S2MPS11_REV_82) {
			ret = s2m_rtc_workaround_operation(info, data);
		if (ret < 0)
			return ret;
		}
		break;
	case S2MPA01:
		ret = s2m_rtc_workaround_operation(info, data);
		if (ret < 0)
			return ret;
		break;
	case S2MPU01X:
		ret = s2m_rtc_workaround_operation(info, data);
		if (ret < 0)
			return ret;
		break;
	case S2MPU01A:
		if (info->iodev->rev_num <= S2MPU01A_REV_01) {
			ret = s2m_rtc_workaround_operation(info, data);
		if (ret < 0)
			return ret;
	}
		break;
	default:
		/* If this happens the core funtion has a problem */
		BUG();
	}

	ret = sec_rtc_bulk_read(info->iodev, S2M_ALARM0_SEC, 7, data);
	if (ret < 0)
		return ret;

	s2m_data_to_tm(data, &tm, info->rtc_24hr_mode);

	printk(KERN_INFO "%s: %d/%d/%d %d:%d:%d(%d)\n", __func__,
		1900 + tm.tm_year, 1 + tm.tm_mon, tm.tm_mday,
		tm.tm_hour, tm.tm_min, tm.tm_sec, tm.tm_wday);

	for (i = 0; i < 7; i++)
		data[i] &= ~ALARM_ENABLE_MASK;

	ret = sec_rtc_bulk_write(info->iodev, S2M_ALARM0_SEC, 7, data);
	if (ret < 0)
		return ret;

	ret = s2m_rtc_set_time_reg(info, 1);
	return ret;
}

static int s2m_rtc_start_alarm(struct s2m_rtc_info *info)
{
	int ret;
	u8 data[7];
	struct rtc_time tm;

	ret = sec_rtc_bulk_read(info->iodev, S2M_ALARM0_SEC, 7, data);
	if (ret < 0)
		return ret;

	s2m_data_to_tm(data, &tm, info->rtc_24hr_mode);

	printk(KERN_INFO "%s: %d/%d/%d %d:%d:%d(%d)\n", __func__,
		1900 + tm.tm_year, 1 + tm.tm_mon, tm.tm_mday,
		tm.tm_hour, tm.tm_min, tm.tm_sec, tm.tm_wday);

	data[RTC_SEC] |= ALARM_ENABLE_MASK;
	data[RTC_MIN] |= ALARM_ENABLE_MASK;
	data[RTC_HOUR] |= ALARM_ENABLE_MASK;
	data[RTC_WEEKDAY] |= ALARM_ENABLE_MASK;
	if (data[RTC_DATE] & 0x1f)
		data[RTC_DATE] |= ALARM_ENABLE_MASK;
	if (data[RTC_MONTH] & 0xf)
		data[RTC_MONTH] |= ALARM_ENABLE_MASK;
	if (data[RTC_YEAR1] & 0x7f)
		data[RTC_YEAR1] |= ALARM_ENABLE_MASK;

	ret = sec_rtc_bulk_write(info->iodev, S2M_ALARM0_SEC, 7, data);
	if (ret < 0)
		return ret;

	ret = s2m_rtc_set_time_reg(info, 1);

	return ret;
}

static int s2m_rtc_set_alarm(struct device *dev, struct rtc_wkalrm *alrm)
{
	struct s2m_rtc_info *info = dev_get_drvdata(dev);
	u8 data[7];
	int ret;

	s2m_tm_to_data(&alrm->time, data);

	printk(KERN_INFO "%s: %d/%d/%d %d:%d:%d(%d)\n", __func__,
		1900 + alrm->time.tm_year, 1 + alrm->time.tm_mon,
		alrm->time.tm_mday, alrm->time.tm_hour, alrm->time.tm_min,
		alrm->time.tm_sec, alrm->time.tm_wday);

	ret = s2m_rtc_stop_alarm(info);
	if (ret < 0)
		return ret;

	ret = sec_rtc_bulk_write(info->iodev, S2M_ALARM0_SEC, 7, data);
	if (ret < 0)
		return ret;

	ret = s2m_rtc_set_time_reg(info, 1);
	if (ret < 0)
		return ret;

	if (alrm->enabled)
		ret = s2m_rtc_start_alarm(info);

	return ret;
}

#if defined(CONFIG_RTC_ALARM_BOOT)
static int s2mpa01_rtc_stop_alarm_boot(struct s2m_rtc_info *info)
{
	u8 data[7];
	int ret, i;
	struct rtc_time tm;

	ret = s2m_rtc_read_time_reg(info);
	if (ret < 0)
		return ret;

	ret = sec_rtc_bulk_read(info->iodev, S2M_RTC_SEC, 7, data);
	if (ret < 0)
		return ret;

	ret = sec_rtc_bulk_write(info->iodev, S2M_RTC_SEC, 7, data);
	if (ret < 0)
		return ret;

	ret = sec_rtc_bulk_read(info->iodev, S2M_ALARM1_SEC, 7, data);
	if (ret < 0)
		return ret;

	s2m_data_to_tm(data, &tm, info->rtc_24hr_mode);

	printk(KERN_INFO "%s: %d/%d/%d %d:%d:%d(%d)\n", __func__,
		1900 + tm.tm_year, 1 + tm.tm_mon, tm.tm_mday,
		tm.tm_hour, tm.tm_min, tm.tm_sec, tm.tm_wday);

	for (i = 0; i < 7; i++)
		data[i] &= ~ALARM_ENABLE_MASK;

	ret = sec_rtc_bulk_write(info->iodev, S2M_ALARM1_SEC, 7, data);
	if (ret < 0)
		return ret;

	ret = s2m_rtc_set_time_reg(info, 1);
	return ret;
}

static int s2mpa01_rtc_start_alarm_boot(struct s2m_rtc_info *info)
{
	int ret;
	u8 data[7];
	struct rtc_time tm;

	ret = sec_rtc_bulk_read(info->iodev, S2M_ALARM1_SEC, 7, data);
	if (ret < 0)
		return ret;

	s2m_data_to_tm(data, &tm, info->rtc_24hr_mode);

	printk(KERN_INFO "%s: %d/%d/%d %d:%d:%d(%d)\n", __func__,
		1900 + tm.tm_year, 1 + tm.tm_mon, tm.tm_mday,
		tm.tm_hour, tm.tm_min, tm.tm_sec, tm.tm_wday);

	data[RTC_SEC] |= ALARM_ENABLE_MASK;
	data[RTC_MIN] |= ALARM_ENABLE_MASK;
	data[RTC_HOUR] |= ALARM_ENABLE_MASK;
	data[RTC_WEEKDAY] &= 0x00;
	if (data[RTC_DATE] & 0x1f)
		data[RTC_DATE] |= ALARM_ENABLE_MASK;
	if (data[RTC_MONTH] & 0xf)
		data[RTC_MONTH] |= ALARM_ENABLE_MASK;
	if (data[RTC_YEAR1] & 0x7f)
		data[RTC_YEAR1] |= ALARM_ENABLE_MASK;

	ret = sec_rtc_bulk_write(info->iodev, S2M_ALARM1_SEC, 7, data);
	if (ret < 0)
		return ret;

	ret = s2m_rtc_set_time_reg(info, 1);

	return ret;
}

static int s2mpa01_rtc_set_alarm_boot(struct device *dev,
					struct rtc_wkalrm *alrm)
{
	struct s2m_rtc_info *info = dev_get_drvdata(dev);
	u8 data[7];
	unsigned int rtcwake;
	int ret;

	s2m_tm_to_data(&alrm->time, data);

	printk(KERN_INFO "%s: %d/%d/%d %d:%d:%d(%d)\n", __func__,
		1900 + alrm->time.tm_year, 1 + alrm->time.tm_mon,
		alrm->time.tm_mday, alrm->time.tm_hour, alrm->time.tm_min,
		alrm->time.tm_sec, alrm->time.tm_wday);

	ret = s2mpa01_rtc_stop_alarm_boot(info);
	if (ret < 0)
		return ret;

	ret = sec_rtc_read(info->iodev, S2M_RTC_UPDATE, &rtcwake);
	if (ret < 0)
		return ret;

	if (alrm->enabled)
		rtcwake |= RTC_WAKE_MASK;
	else
		rtcwake &= ~RTC_WAKE_MASK;

	ret = sec_rtc_write(info->iodev, S2M_RTC_UPDATE, (char)rtcwake);

	if (ret < 0) {
		dev_err(info->dev, "%s: fail to write update reg(%d)\n",
				__func__, ret);
	} else {
		usleep_range(1000, 1000);
	}

	ret = sec_rtc_bulk_write(info->iodev, S2M_ALARM1_SEC, 7, data);
	if (ret < 0)
		return ret;

	ret = s2m_rtc_set_time_reg(info, 1);
	if (ret < 0)
		return ret;

	if (alrm->enabled)
		ret = s2mpa01_rtc_start_alarm_boot(info);

	return ret;
}

static int s2mpa01_rtc_get_alarm_boot(struct device *dev,
				      struct rtc_wkalrm *alrm)
{
	struct s2m_rtc_info *info = dev_get_drvdata(dev);

	if( info->alarm_irq_flag)
		alrm->enabled = 0x1;
	else
		alrm->enabled = 0x0;
	printk("max77686_rtc_get_alarm_boot : %d, %d\n",
						info->lpm_mode, alrm->enabled);
	return info->lpm_mode;
}
#endif

static int s2m_rtc_alarm_irq_enable(struct device *dev,
					unsigned int enabled)
{
	struct s2m_rtc_info *info = dev_get_drvdata(dev);

	if (enabled)
		return s2m_rtc_start_alarm(info);
	else
		return s2m_rtc_stop_alarm(info);
}

static irqreturn_t s2m_rtc_alarm_irq(int irq, void *data)
{
	struct s2m_rtc_info *info = data;

	rtc_update_irq(info->rtc_dev, 1, RTC_IRQF | RTC_AF);

	return IRQ_HANDLED;
}

#if defined(CONFIG_RTC_ALARM_BOOT)
static void sapa_reboot(struct work_struct *work)
{
	//machine_restart(NULL);
	kernel_restart(NULL);
	//panic("Test panic");
}

static irqreturn_t s2m_rtc_alarm1_irq(int irq, void *data)
{
	struct s2m_rtc_info *info = data;

	dev_info(info->dev, "%s:irq(%d), lpm_mode:(%d)\n",
						__func__, irq, info->lpm_mode);

	if (info->lpm_mode) {
		wake_lock(&info->alarm_wake_lock);
		info->alarm_irq_flag = true;
		if (info->sapa_workq) {
			queue_delayed_work(info->sapa_workq, &info->sapa_reboot_work, (1*HZ));
		}
	}

	rtc_update_irq(info->rtc_dev, 1, RTC_IRQF | RTC_AF);

	return IRQ_HANDLED;
}
#endif

static const struct rtc_class_ops s2m_rtc_ops = {
	.read_time = s2m_rtc_read_time,
	.set_time = s2m_rtc_set_time,
	.read_alarm = s2m_rtc_read_alarm,
	.set_alarm = s2m_rtc_set_alarm,
#if defined(CONFIG_RTC_ALARM_BOOT)
	.set_alarm_boot = s2mpa01_rtc_set_alarm_boot,
	.get_alarm_boot = s2mpa01_rtc_get_alarm_boot,
#endif
	.alarm_irq_enable = s2m_rtc_alarm_irq_enable,
};

static void s2m_rtc_enable_wtsr(struct s2m_rtc_info *info, bool enable)
{
	int ret;
	unsigned int val, mask;

	if (enable)
		val = WTSR_ENABLE_MASK;
	else
		val = 0;

	mask = WTSR_ENABLE_MASK;

	dev_info(info->dev, "%s: %s WTSR\n", __func__,
		 enable ? "enable" : "disable");

	ret = sec_rtc_update(info->iodev, S2M_RTC_WTSR_SMPL, val, mask);
	if (ret < 0) {
		dev_err(info->dev, "%s: fail to update WTSR reg(%d)\n",
			__func__, ret);
		return;
	}
}

static void s2m_rtc_enable_smpl(struct s2m_rtc_info *info, bool enable)
{
	int ret;
	unsigned int val, mask;

	if (enable)
		val = SMPL_ENABLE_MASK;
	else
		val = 0;

	mask = SMPL_ENABLE_MASK;

	dev_info(info->dev, "%s: %s SMPL\n", __func__,
			enable ? "enable" : "disable");

	ret = sec_rtc_update(info->iodev, S2M_RTC_WTSR_SMPL, val, mask);
	if (ret < 0) {
		dev_err(info->dev, "%s: fail to update SMPL reg(%d)\n",
				__func__, ret);
		return;
	}

	val = 0;
	sec_rtc_read(info->iodev, S2M_RTC_WTSR_SMPL, &val);
	pr_info("%s: WTSR_SMPL(0x%02x)\n", __func__, val);
}

static int s2m_rtc_init_reg(struct s2m_rtc_info *info)
{
	unsigned int data, tp_read, tp_read1;
	int ret;
	struct rtc_time tm;
#if defined(CONFIG_RTC_ALARM_BOOT)
	u8 data_alm1[7];
	struct rtc_time alrm;

	info->rtc_24hr_mode = 1;

	ret = sec_rtc_bulk_read(info->iodev, S2M_ALARM1_SEC, 7, data_alm1);
	if (ret < 0)
		return ret;

	s2m_data_to_tm(data_alm1, &alrm, info->rtc_24hr_mode);

	printk(KERN_INFO "%s: %d/%d/%d %d:%d:%d(%d)\n", __func__,
		1900 + alrm.tm_year, 1 + alrm.tm_mon,
		alrm.tm_mday, alrm.tm_hour,
		alrm.tm_min, alrm.tm_sec,
		alrm.tm_wday);
	printk(KERN_INFO "%s: %d/%d/%d %d:%d:%d(%d)\n", __func__,
		data_alm1[6], data_alm1[5],
		data_alm1[4], data_alm1[2],
		data_alm1[1], data_alm1[0],
		data_alm1[3]);

	ret = sec_rtc_read(info->iodev, S2M_RTC_UPDATE, &data);
	if (ret < 0)
		return ret;
	printk(KERN_INFO "%s: RTC_UPDATE(%d)\n", __func__, data);
#endif

	ret = sec_rtc_read(info->iodev, S2M_RTC_CTRL, &tp_read);
	if (ret < 0) {
		dev_err(info->dev, "%s: fail to read control reg(%d)\n",
			__func__, ret);
		return ret;
	}

	/* Set RTC control register : Binary mode, 24hour mdoe */
	data = (1 << MODEL24_SHIFT) | tp_read;
	data &= ~(1 << BCD_EN_SHIFT);

	info->rtc_24hr_mode = 1;
	/* In first boot time, Set rtc time to 1/1/2012 00:00:00(SUN) */
	ret = sec_rtc_read(info->iodev, S2M_CAP_SEL, &tp_read1);
	if (ret < 0) {
		dev_err(info->dev, "%s: fail to read cap sel reg(%d)\n",
				__func__, ret);
		return ret;
	}
	/* In first boot time, Set rtc time to 1/1/2014 00:00:00(WEN) */
	if ((tp_read & MODEL24_MASK) == 0 || (tp_read1 != 0xf8)) {
		dev_info(info->dev, "rtc init\n");
		tm.tm_sec = 0;
		tm.tm_min = 0;
		tm.tm_hour = 0;
		tm.tm_wday = 3;
		tm.tm_mday = 1;
		tm.tm_mon = 0;
		tm.tm_year = 114;
		tm.tm_yday = 0;
		tm.tm_isdst = 0;
		ret = s2m_rtc_set_time(info->dev, &tm);
	}

	ret = sec_rtc_update(info->iodev, S2M_CAP_SEL, 0xf8, 0xff);
	if (ret < 0) {
		dev_err(info->dev, "%s: fail to update cap sel reg(%d)\n",
				__func__, ret);
		return ret;
	}
#if !defined(CONFIG_RTC_ALARM_BOOT)
	ret = sec_rtc_update(info->iodev, S2M_RTC_UPDATE, 0x00, 0x04);
	if (ret < 0) {
		dev_err(info->dev, "%s: fail to update rtc update reg(%d)\n",
				__func__, ret);
		return ret;
	}
#endif

	ret = sec_rtc_update(info->iodev, S2M_RTC_CTRL,
		data, 0x3);

	s2m_rtc_set_time_reg(info, 1);

	if (ret < 0) {
		dev_err(info->dev, "%s: fail to write control reg(%d)\n",
				__func__, ret);
		return ret;
	}

	return ret;
}

static int __devinit s2m_rtc_probe(struct platform_device *pdev)
{
	struct sec_pmic_dev *iodev = dev_get_drvdata(pdev->dev.parent);
	struct sec_pmic_platform_data *pdata = dev_get_platdata(iodev->dev);
	struct s2m_rtc_info *s2m;
	int ret;

	s2m = devm_kzalloc(&pdev->dev, sizeof(struct s2m_rtc_info),
				GFP_KERNEL);
	if (!s2m)
		return -ENOMEM;

	s2m->dev = &pdev->dev;
	s2m->iodev = iodev;

	s2m->wtsr_smpl = pdata->wtsr_smpl;

	switch (iodev->device_type) {
	case S2MPS11X:
		s2m->irq = pdata->irq_base + S2MPS11_IRQ_RTCA1;
		break;
	case S2MPA01:
		s2m->irq = pdata->irq_base + S2MPA01_IRQ_RTCA0;
#if defined(CONFIG_RTC_ALARM_BOOT)
		s2m->irq1 = pdata->irq_base + S2MPA01_IRQ_RTCA1;
#endif
		break;

	case S2MPU01X:
		s2m->irq = pdata->irq_base + S2MPU01_IRQ_RTCA0;
		break;
	case S2MPU01A:
		s2m->irq = pdata->irq_base + S2MPU01A_IRQ_RTCA0;
		break;
	default:
		/* If this happens the core funtion has a problem */
		BUG();
	}

	platform_set_drvdata(pdev, s2m);

	ret = s2m_rtc_init_reg(s2m);

	if (s2m->wtsr_smpl) {
		s2m_rtc_enable_wtsr(s2m, true);
		s2m_rtc_enable_smpl(s2m, true);
	}

	device_init_wakeup(&pdev->dev, true);

	s2m->rtc_dev = rtc_device_register("s2m-rtc", &pdev->dev,
			&s2m_rtc_ops, THIS_MODULE);

	if (IS_ERR(s2m->rtc_dev)) {
		ret = PTR_ERR(s2m->rtc_dev);
		dev_err(&pdev->dev, "Failed to register RTC device: %d\n", ret);
		goto out_rtc;
	}

	ret = request_threaded_irq(s2m->irq, NULL, s2m_rtc_alarm_irq, 0,
			"rtc-alarm0", s2m);

	if (ret < 0) {
		dev_err(&pdev->dev, "Failed to request alarm IRQ: %d: %d\n",
			s2m->irq, ret);
		goto out_rtc;
	}

#if defined(CONFIG_RTC_ALARM_BOOT)
	ret = request_threaded_irq(s2m->irq1, NULL, s2m_rtc_alarm1_irq, 0,
			"rtc-alarm1", s2m);

	if (ret < 0)
		dev_err(&pdev->dev, "Failed to request alarm IRQ: %d: %d\n",
			s2m->irq1, ret);

	s2m->lpm_mode = lpcharge;
	if(s2m->lpm_mode) {
		wake_lock_init(&s2m->alarm_wake_lock, WAKE_LOCK_SUSPEND,
												"alarm_wake_lock");

		s2m->sapa_workq = create_singlethread_workqueue("pwron_alarm_resume");
		if (s2m->sapa_workq == NULL) {
			pr_err("[SAPA] pwron_alarm work creating failed\n");
		}
		else {
			INIT_DELAYED_WORK(&s2m->sapa_reboot_work, sapa_reboot);
		}
	}
#endif

	return 0;

out_rtc:
	platform_set_drvdata(pdev, NULL);
	return ret;
}

static int __devexit s2m_rtc_remove(struct platform_device *pdev)
{
	struct s2m_rtc_info *s2m = platform_get_drvdata(pdev);

	if (s2m) {
		free_irq(s2m->irq, s2m);
#if defined(CONFIG_RTC_ALARM_BOOT)
		free_irq(s2m->irq1, s2m);
#endif
		rtc_device_unregister(s2m->rtc_dev);
	}

	return 0;
}

static void s2m_rtc_shutdown(struct platform_device *pdev)
{
	struct s2m_rtc_info *info = platform_get_drvdata(pdev);
	int i;
	unsigned int val = 0;

	if (info->wtsr_smpl) {
		for (i = 0; i < 3; i++) {
			s2m_rtc_enable_wtsr(info, false);
			sec_rtc_read(info->iodev,
					S2M_RTC_WTSR_SMPL, &val);
			pr_info("%s: WTSR_SMPL reg(0x%02x)\n", __func__, val);

			if (val & WTSR_ENABLE_MASK) {
				pr_emerg("%s: fail to disable WTSR\n",
				__func__);
			} else {
				pr_info("%s: success to disable WTSR\n",
					__func__);
				break;
			}
		}
	}

	/* Disable SMPL when power off */
	s2m_rtc_enable_smpl(info, false);

#if defined(CONFIG_RTC_ALARM_BOOT)
	/* Disable alarm0 interrupt */
	s2m_rtc_stop_alarm(info);
#endif
}

static const struct platform_device_id s2m_rtc_id[] = {
	{ "s2m-rtc", 0 },
};

static struct platform_driver s2m_rtc_driver = {
	.driver		= {
		.name	= "s2m-rtc",
		.owner	= THIS_MODULE,
	},
	.probe		= s2m_rtc_probe,
	.remove		= __devexit_p(s2m_rtc_remove),
	.shutdown	= s2m_rtc_shutdown,
	.id_table	= s2m_rtc_id,
};

static int __init s2m_rtc_init(void)
{
	return platform_driver_register(&s2m_rtc_driver);
}
module_init(s2m_rtc_init);

static void __exit s2m_rtc_exit(void)
{
	platform_driver_unregister(&s2m_rtc_driver);
}
module_exit(s2m_rtc_exit);

/* Module information */
MODULE_AUTHOR("Sangbeom Kim <sbkim73@samsung.com>");
MODULE_DESCRIPTION("Samsung RTC driver");
MODULE_LICENSE("GPL");
