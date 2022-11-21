/*
 * Real Time Clock driver for Marvell 88PM80x PMIC
 *
 * Copyright (c) 2012 Marvell International Ltd.
 *  Wenzeng Chen<wzch@marvell.com>
 *  Qiao Zhou <zhouqiao@marvell.com>
 *
 * This file is subject to the terms and conditions of the GNU General
 * Public License. See the file "COPYING" in the main directory of this
 * archive for more details.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/regmap.h>
#include <linux/mfd/core.h>
#include <linux/mfd/88pm80x.h>
#include <linux/rtc.h>

#if defined(CONFIG_RTC_CHN_ALARM_BOOT)
#include <linux/reboot.h>
#include <linux/workqueue.h>
#endif

#define PM800_RTC_COUNTER1		(0xD1)
#define PM800_RTC_COUNTER2		(0xD2)
#define PM800_RTC_COUNTER3		(0xD3)
#define PM800_RTC_COUNTER4		(0xD4)
#define PM800_RTC_EXPIRE1_1		(0xD5)
#define PM800_RTC_EXPIRE1_2		(0xD6)
#define PM800_RTC_EXPIRE1_3		(0xD7)
#define PM800_RTC_EXPIRE1_4		(0xD8)
#define PM800_RTC_TRIM1			(0xD9)
#define PM800_RTC_TRIM2			(0xDA)
#define PM800_RTC_TRIM3			(0xDB)
#define PM800_RTC_TRIM4			(0xDC)
#define PM800_RTC_EXPIRE2_1		(0xDD)
#define PM800_RTC_EXPIRE2_2		(0xDE)
#define PM800_RTC_EXPIRE2_3		(0xDF)
#define PM800_RTC_EXPIRE2_4		(0xE0)
#define PM800_USER_DATA3		(0xEA)
#define PM800_USER_DATA2		(0xEB)

#define PM800_POWER_DOWN_LOG1	(0xE5)
#define PM800_POWER_DOWN_LOG2	(0xE6)

struct pm80x_rtc_info {
	struct pm80x_chip *chip;
	struct regmap *map;
	struct rtc_device *rtc_dev;
	struct device *dev;
	struct delayed_work calib_work;

	int irq;
	int vrtc;
	int (*sync) (unsigned int ticks);
};

#if defined(CONFIG_RTC_CHN_ALARM_BOOT) && defined(CONFIG_SPA)
extern int spa_lpm_charging_mode_get(void);
struct work_struct reboot_work;
static void pm80x_rtc_reboot_work(struct work_struct *work)
{
	kernel_restart("alarm");
}
#endif

static irqreturn_t rtc_update_handler(int irq, void *data)
{
	struct pm80x_rtc_info *info = (struct pm80x_rtc_info *)data;
	int mask;

	mask = PM800_ALARM | PM800_ALARM_WAKEUP;
	regmap_update_bits(info->map, PM800_RTC_CONTROL, mask | PM800_ALARM1_EN,
			   mask);
	rtc_update_irq(info->rtc_dev, 1, RTC_AF);

#if defined(CONFIG_RTC_CHN_ALARM_BOOT) && defined(CONFIG_SPA)
	if(spa_lpm_charging_mode_get())
		schedule_work(&reboot_work);
#endif

	return IRQ_HANDLED;
}

static int pm80x_rtc_alarm_irq_enable(struct device *dev, unsigned int enabled)
{
	struct pm80x_rtc_info *info = dev_get_drvdata(dev);

	if (enabled)
		regmap_update_bits(info->map, PM800_RTC_CONTROL,
				   PM800_ALARM1_EN, PM800_ALARM1_EN);
	else
		regmap_update_bits(info->map, PM800_RTC_CONTROL,
				   PM800_ALARM1_EN, 0);
	return 0;
}

/*
 * Calculate the next alarm time given the requested alarm time mask
 * and the current time.
 */
static void rtc_next_alarm_time(struct rtc_time *next, struct rtc_time *now,
				struct rtc_time *alrm)
{
	unsigned long next_time;
	unsigned long now_time;

	next->tm_year = now->tm_year;
	next->tm_mon = now->tm_mon;
	next->tm_mday = now->tm_mday;
	next->tm_hour = alrm->tm_hour;
	next->tm_min = alrm->tm_min;
	next->tm_sec = alrm->tm_sec;

	rtc_tm_to_time(now, &now_time);
	rtc_tm_to_time(next, &next_time);

	if (next_time < now_time) {
		/* Advance one day */
		next_time += 60 * 60 * 24;
		rtc_time_to_tm(next_time, next);
	}
}

static int pm80x_rtc_read_time(struct device *dev, struct rtc_time *tm)
{
	struct pm80x_rtc_info *info = dev_get_drvdata(dev);
	unsigned char buf[4];
	unsigned long ticks, base, data;

printk("pm80x_rtc_read_time\n");
	
	regmap_raw_read(info->map, PM800_USER_DATA2, buf, 4);
	base = (buf[3] << 24) | (buf[2] << 16) | (buf[1] << 8) | buf[0];
	dev_dbg(info->dev, "%x-%x-%x-%x\n", buf[0], buf[1], buf[2], buf[3]);

	/* load 32-bit read-only counter */
	regmap_raw_read(info->map, PM800_RTC_COUNTER1, buf, 4);
	data = (buf[3] << 24) | (buf[2] << 16) | (buf[1] << 8) | buf[0];
	ticks = base + data;
	dev_dbg(info->dev, "get base:0x%lx, RO count:0x%lx, ticks:0x%lx\n",
		base, data, ticks);
	rtc_time_to_tm(ticks, tm);
	return 0;
}

static int pm80x_rtc_set_time(struct device *dev, struct rtc_time *tm)
{
	struct pm80x_rtc_info *info = dev_get_drvdata(dev);
	unsigned char buf[4];
	unsigned long ticks, base, data;
	if ((tm->tm_year < 70) || (tm->tm_year > 138)) {
		dev_dbg(info->dev,
			"Set time %d out of range. Please set time between 1970 to 2038.\n",
			1900 + tm->tm_year);
		return -EINVAL;
	}
	rtc_tm_to_time(tm, &ticks);

	/* load 32-bit read-only counter */
	regmap_raw_read(info->map, PM800_RTC_COUNTER1, buf, 4);
	data = (buf[3] << 24) | (buf[2] << 16) | (buf[1] << 8) | buf[0];
	base = ticks - data;
	dev_dbg(info->dev, "set base:0x%lx, RO count:0x%lx, ticks:0x%lx\n",
		base, data, ticks);
	buf[0] = base & 0xFF;
	buf[1] = (base >> 8) & 0xFF;
	buf[2] = (base >> 16) & 0xFF;
	buf[3] = (base >> 24) & 0xFF;
	regmap_raw_write(info->map, PM800_USER_DATA2, buf, 4);

	if (info->sync)
		info->sync(ticks);

	return 0;
}

static int pm80x_rtc_read_alarm(struct device *dev, struct rtc_wkalrm *alrm)
{
	struct pm80x_rtc_info *info = dev_get_drvdata(dev);
	unsigned char buf[4];
	unsigned long ticks, base, data;
	int ret;
	unsigned int val;

printk("pm80x_rtc_read_alarm\n");

	regmap_raw_read(info->map, PM800_USER_DATA2, buf, 4);
	base = (buf[3] << 24) | (buf[2] << 16) | (buf[1] << 8) | buf[0];
	dev_dbg(info->dev, "%x-%x-%x-%x\n", buf[0], buf[1], buf[2], buf[3]);

	regmap_raw_read(info->map, PM800_RTC_EXPIRE1_1, buf, 4);
	data = (buf[3] << 24) | (buf[2] << 16) | (buf[1] << 8) | buf[0];
	ticks = base + data;
	dev_dbg(info->dev, "get base:0x%lx, RO count:0x%lx, ticks:0x%lx\n",
		base, data, ticks);

	rtc_time_to_tm(ticks, &alrm->time);
	ret = regmap_read(info->map, PM800_RTC_CONTROL, &val);
	if (ret < 0) {
		dev_err(info->dev, "Failed to rtc ctrl: %d\n", ret);
		return ret;
	}

	alrm->enabled = (val & PM800_ALARM1_EN) ? 1 : 0;
	alrm->pending = (val & (PM800_ALARM | PM800_ALARM_WAKEUP)) ? 1 : 0;
	return 0;
}

static int pm80x_rtc_set_alarm(struct device *dev, struct rtc_wkalrm *alrm)
{
	struct pm80x_rtc_info *info = dev_get_drvdata(dev);
	struct rtc_time now_tm, alarm_tm;
	unsigned long ticks, base, data;
	unsigned char buf[4];
	int mask;

	regmap_update_bits(info->map, PM800_RTC_CONTROL, PM800_ALARM1_EN, 0);

	regmap_raw_read(info->map, PM800_USER_DATA2, buf, 4);
	base = (buf[3] << 24) | (buf[2] << 16) | (buf[1] << 8) | buf[0];
	dev_dbg(info->dev, "%x-%x-%x-%x\n", buf[0], buf[1], buf[2], buf[3]);

	/* load 32-bit read-only counter */
	regmap_raw_read(info->map, PM800_RTC_COUNTER1, buf, 4);
	data = (buf[3] << 24) | (buf[2] << 16) | (buf[1] << 8) | buf[0];
	ticks = base + data;
	dev_dbg(info->dev, "get base:0x%lx, RO count:0x%lx, ticks:0x%lx\n",
		base, data, ticks);

	rtc_time_to_tm(ticks, &now_tm);
	dev_dbg(info->dev, "%s, now time : %lu\n", __func__, ticks);
	rtc_next_alarm_time(&alarm_tm, &now_tm, &alrm->time);
	/* get new ticks for alarm in 24 hours */
	rtc_tm_to_time(&alarm_tm, &ticks);
	dev_dbg(info->dev, "%s, alarm time: %lu\n", __func__, ticks);
	data = ticks - base;

	buf[0] = data & 0xff;
	buf[1] = (data >> 8) & 0xff;
	buf[2] = (data >> 16) & 0xff;
	buf[3] = (data >> 24) & 0xff;
	regmap_raw_write(info->map, PM800_RTC_EXPIRE1_1, buf, 4);
	if (alrm->enabled) {
		mask = PM800_ALARM | PM800_ALARM_WAKEUP | PM800_ALARM1_EN;
		regmap_update_bits(info->map, PM800_RTC_CONTROL, mask, mask);
	} else {
		mask = PM800_ALARM | PM800_ALARM_WAKEUP | PM800_ALARM1_EN;
		regmap_update_bits(info->map, PM800_RTC_CONTROL, mask,
				   PM800_ALARM | PM800_ALARM_WAKEUP);
	}
	return 0;
}

static const struct rtc_class_ops pm80x_rtc_ops = {
	.read_time = pm80x_rtc_read_time,
	.set_time = pm80x_rtc_set_time,
	.read_alarm = pm80x_rtc_read_alarm,
	.set_alarm = pm80x_rtc_set_alarm,
	.alarm_irq_enable = pm80x_rtc_alarm_irq_enable,
};

#ifdef CONFIG_PM
static int pm80x_rtc_suspend(struct device *dev)
{
	return pm80x_dev_suspend(dev);
}

static int pm80x_rtc_resume(struct device *dev)
{
	return pm80x_dev_resume(dev);
}
#endif

static SIMPLE_DEV_PM_OPS(pm80x_rtc_pm_ops, pm80x_rtc_suspend, pm80x_rtc_resume);

static int __devinit pm80x_rtc_probe(struct platform_device *pdev)
{
	struct pm80x_chip *chip = dev_get_drvdata(pdev->dev.parent);
	struct pm80x_platform_data *pm80x_pdata;
	struct pm80x_rtc_pdata *pdata = NULL;
	struct pm80x_rtc_info *info;
	struct rtc_time tm;
	unsigned long ticks = 0;
	int irq, ret;
	unsigned long now_ticks = 0, default_ticks = 0;
	struct rtc_time default_time; 

	pdata = pdev->dev.platform_data;
	if (pdata == NULL)
		dev_warn(&pdev->dev, "No platform data!\n");

	info =
	    devm_kzalloc(&pdev->dev, sizeof(struct pm80x_rtc_info), GFP_KERNEL);
	if (!info)
		return -ENOMEM;
	irq = platform_get_irq(pdev, 0);
	if (irq < 0) {
		dev_err(&pdev->dev, "No IRQ resource!\n");
		ret = -EINVAL;
		goto out;
	}
	info->irq = irq + chip->irq_base;

	info->chip = chip;
	info->map = chip->regmap;
	if (!info->map) {
		dev_err(&pdev->dev, "no regmap!\n");
		ret = -EINVAL;
		goto out;
	}

	info->dev = &pdev->dev;
	dev_set_drvdata(&pdev->dev, info);

	ret = pm80x_rtc_read_time(&pdev->dev, &tm);
	if (ret < 0) {
		dev_err(&pdev->dev, "Failed to read initial time.\n");
		goto out;
	}

#if 1
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
		tm.tm_year = 112;
		tm.tm_mon = 0;
		tm.tm_mday = 1;
		tm.tm_hour = 0;
		tm.tm_min = 0;
		tm.tm_sec = 0;
		ret = pm80x_rtc_set_time(&pdev->dev,&tm);
		if (ret < 0) {
			dev_err(&pdev->dev, "Failed to set initial time.\n");
			goto out;
		}
	}
#else	
	if ((tm.tm_year < 70) || (tm.tm_year > 138)) {
		tm.tm_year = 70;
		tm.tm_mon = 0;
		tm.tm_mday = 1;
		tm.tm_hour = 0;
		tm.tm_min = 0;
		tm.tm_sec = 0;
		ret = pm80x_rtc_set_time(&pdev->dev, &tm);
		if (ret < 0) {
			dev_err(&pdev->dev, "Failed to set initial time.\n");
			goto out;
		}
	}
#endif

	rtc_tm_to_time(&tm, &ticks);
	if (pdata && pdata->sync) {
		pdata->sync(ticks);
		info->sync = pdata->sync;
	}

	info->rtc_dev = rtc_device_register("88pm80x-rtc", &pdev->dev,
					    &pm80x_rtc_ops, THIS_MODULE);
	ret = PTR_ERR(info->rtc_dev);
	if (IS_ERR(info->rtc_dev)) {
		dev_err(&pdev->dev, "Failed to register RTC device: %d\n", ret);
		goto out;
	}
	/*
	 * enable internal XO instead of internal 3.25MHz clock since it can
	 * free running in PMIC power-down state.
	 */
	regmap_update_bits(info->map, PM800_RTC_CONTROL, PM800_RTC1_USE_XO,
			   PM800_RTC1_USE_XO);

	if (pdev->dev.parent->platform_data) {
		pm80x_pdata = pdev->dev.parent->platform_data;
		pdata = pm80x_pdata->rtc;
		if (pdata)
			info->rtc_dev->dev.platform_data = &pdata->rtc_wakeup;
	}

	ret = pm80x_request_irq(chip, info->irq, rtc_update_handler,
				IRQF_ONESHOT, "rtc", info);
	if (ret < 0) {
		dev_err(chip->dev, "Failed to request IRQ: #%d: %d\n",
			info->irq, ret);
		goto out_rtc;
	}

#if defined(CONFIG_RTC_CHN_ALARM_BOOT) && defined(CONFIG_SPA)
	if(spa_lpm_charging_mode_get())
	{
		INIT_WORK(&reboot_work, pm80x_rtc_reboot_work);
#if 0
		pm80x_rtc_alarm_irq_enable(&pdev->dev, 1);
#endif
	}
#endif

	device_init_wakeup(&pdev->dev, 1);

	return 0;
out_rtc:
	pm80x_free_irq(chip, info->irq, info);
out:
	devm_kfree(&pdev->dev, info);
	return ret;
}

static int __devexit pm80x_rtc_remove(struct platform_device *pdev)
{
	struct pm80x_rtc_info *info = platform_get_drvdata(pdev);
	platform_set_drvdata(pdev, NULL);
	rtc_device_unregister(info->rtc_dev);
	pm80x_free_irq(info->chip, info->irq, info);
	devm_kfree(&pdev->dev, info);
	return 0;
}

static struct platform_driver pm80x_rtc_driver = {
	.driver = {
		   .name = "88pm80x-rtc",
		   .owner = THIS_MODULE,
		   .pm = &pm80x_rtc_pm_ops,
		   },
	.probe = pm80x_rtc_probe,
	.remove = __devexit_p(pm80x_rtc_remove),
};

module_platform_driver(pm80x_rtc_driver);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Marvell 88PM80x RTC driver");
MODULE_AUTHOR("Qiao Zhou <zhouqiao@marvell.com>");
MODULE_ALIAS("platform:88pm80x-rtc");
