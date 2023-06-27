/*
 * Real Time Clock driver for Marvell 88pm822 PMIC
 *
 * Copyright (c) 2012 Marvell International Ltd.
 *  Wenzeng Chen<wzch@marvell.com>
 *  Qiao Zhou <zhouqiao@marvell.com>
 *  Hongyan Song<hysong@marvell.com>
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
#include <linux/mfd/88pm822.h>
#include <linux/rtc.h>

#if defined(CONFIG_RTC_CHN_ALARM_BOOT)
#include <linux/reboot.h>
#include <linux/workqueue.h>
#endif

struct pm822_rtc_info {
	struct pm822_chip *chip;
	struct regmap *map;
	struct rtc_device *rtc_dev;
	struct device *dev;
	struct delayed_work calib_work;

	int irq;
	int vrtc;
	int (*sync) (unsigned int ticks);
};

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

static irqreturn_t rtc_update_handler(int irq, void *data)
{
	struct pm822_rtc_info *info = (struct pm822_rtc_info *)data;
	int mask;

	mask = PM822_ALARM | PM822_ALARM_WAKEUP;
	regmap_update_bits(info->map, PM822_RTC_CTRL, mask | PM822_ALARM1_EN,
			   mask);
	rtc_update_irq(info->rtc_dev, 1, RTC_AF);
	
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

static int pm822_rtc_alarm_irq_enable(struct device *dev, unsigned int enabled)
{
	struct pm822_rtc_info *info = dev_get_drvdata(dev);

	if (enabled)
		regmap_update_bits(info->map, PM822_RTC_CTRL,
				   PM822_ALARM1_EN, PM822_ALARM1_EN);
	else
		regmap_update_bits(info->map, PM822_RTC_CTRL,
				   PM822_ALARM1_EN, 0);
	return 0;
}

static int pm822_rtc_read_time(struct device *dev, struct rtc_time *tm)
{
	struct pm822_rtc_info *info = dev_get_drvdata(dev);
	unsigned char buf[4];
	unsigned long ticks, data;
	long base;

	regmap_raw_read(info->map, PM822_USER_DATA1, buf, 4);
	base = (buf[3] << 24) | (buf[2] << 16) | (buf[1] << 8) | buf[0];
	dev_dbg(info->dev, "%x-%x-%x-%x\n", buf[0], buf[1], buf[2], buf[3]);

	/* load 32-bit read-only counter */
	regmap_raw_read(info->map, PM822_RTC_COUNTER1, buf, 4);
	data = (buf[3] << 24) | (buf[2] << 16) | (buf[1] << 8) | buf[0];
	ticks = base + data;
	dev_dbg(info->dev, "get base:0x%lx, RO count:0x%lx, ticks:0x%lx\n",
		base, data, ticks);
	rtc_time_to_tm(ticks, tm);
	return 0;
}

static int pm822_rtc_set_time(struct device *dev, struct rtc_time *tm)
{
	struct pm822_rtc_info *info = dev_get_drvdata(dev);
	unsigned char buf[4];
	unsigned long ticks, data;
	long base;

	if ((tm->tm_year < 70) || (tm->tm_year > 138)) {
		dev_dbg(info->dev,
			"Set time %d out of range. Please set time between 1970 to 2038.\n",
			1900 + tm->tm_year);
		return -EINVAL;
	}
	rtc_tm_to_time(tm, &ticks);

	/* load 32-bit read-only counter */
	regmap_raw_read(info->map, PM822_RTC_COUNTER1, buf, 4);
	data = (buf[3] << 24) | (buf[2] << 16) | (buf[1] << 8) | buf[0];
	base = ticks - data;
	dev_dbg(info->dev, "set base:0x%lx, RO count:0x%lx, ticks:0x%lx\n",
		base, data, ticks);
	buf[0] = base & 0xFF;
	buf[1] = (base >> 8) & 0xFF;
	buf[2] = (base >> 16) & 0xFF;
	buf[3] = (base >> 24) & 0xFF;
	regmap_raw_write(info->map, PM822_USER_DATA1, buf, 4);

	if (info->sync)
		info->sync(ticks);

	return 0;
}

static int pm822_rtc_read_alarm(struct device *dev, struct rtc_wkalrm *alrm)
{
	struct pm822_rtc_info *info = dev_get_drvdata(dev);
	unsigned char buf[4];
	unsigned long ticks, base, data;
	int ret;
	unsigned int val;

	regmap_raw_read(info->map, PM822_USER_DATA1, buf, 4);
	base = (buf[3] << 24) | (buf[2] << 16) | (buf[1] << 8) | buf[0];
	dev_dbg(info->dev, "%x-%x-%x-%x\n", buf[0], buf[1], buf[2], buf[3]);

	regmap_raw_read(info->map, PM822_RTC_EXPIRE1_1, buf, 4);
	data = (buf[3] << 24) | (buf[2] << 16) | (buf[1] << 8) | buf[0];
	ticks = base + data;
	dev_dbg(info->dev, "get base:0x%lx, RO count:0x%lx, ticks:0x%lx\n",
		base, data, ticks);

	rtc_time_to_tm(ticks, &alrm->time);
	ret = regmap_read(info->map, PM822_RTC_CTRL, &val);
	if (ret < 0) {
		dev_err(info->dev, "Failed to rtc ctrl: %d\n", ret);
		return ret;
	}

	alrm->enabled = (val & PM822_ALARM1_EN) ? 1 : 0;
	alrm->pending = (val & (PM822_ALARM | PM822_ALARM_WAKEUP)) ? 1 : 0;
	return 0;
}

static int pm822_rtc_set_alarm(struct device *dev, struct rtc_wkalrm *alrm)
{
	struct pm822_rtc_info *info = dev_get_drvdata(dev);
	unsigned long ticks, base, data;
	unsigned char buf[4];
	int mask;

	regmap_update_bits(info->map, PM822_RTC_CTRL, PM822_ALARM1_EN, 0);

	regmap_raw_read(info->map, PM822_USER_DATA1, buf, 4);
	base = (buf[3] << 24) | (buf[2] << 16) | (buf[1] << 8) | buf[0];
	dev_dbg(info->dev, "%x-%x-%x-%x\n", buf[0], buf[1], buf[2], buf[3]);

	/* get new ticks for alarm */
	rtc_tm_to_time(&alrm->time, &ticks);
	dev_dbg(info->dev, "%s, alarm time: %lu\n", __func__, ticks);
	data = ticks - base;

	buf[0] = data & 0xff;
	buf[1] = (data >> 8) & 0xff;
	buf[2] = (data >> 16) & 0xff;
	buf[3] = (data >> 24) & 0xff;
	regmap_raw_write(info->map, PM822_RTC_EXPIRE1_1, buf, 4);
	if (alrm->enabled) {
		mask = PM822_ALARM | PM822_ALARM_WAKEUP | PM822_ALARM1_EN;
		regmap_update_bits(info->map, PM822_RTC_CTRL, mask, mask);
	} else {
		mask = PM822_ALARM | PM822_ALARM_WAKEUP | PM822_ALARM1_EN;
		regmap_update_bits(info->map, PM822_RTC_CTRL, mask,
				   PM822_ALARM | PM822_ALARM_WAKEUP);
	}
	return 0;
}

static const struct rtc_class_ops pm822_rtc_ops = {
	.read_time = pm822_rtc_read_time,
	.set_time = pm822_rtc_set_time,
	.read_alarm = pm822_rtc_read_alarm,
	.set_alarm = pm822_rtc_set_alarm,
	.alarm_irq_enable = pm822_rtc_alarm_irq_enable,
};

#ifdef CONFIG_PM
static int pm822_rtc_suspend(struct device *dev)
{
	return pm822_dev_suspend(dev);
}

static int pm822_rtc_resume(struct device *dev)
{
	return pm822_dev_resume(dev);
}
#endif

static SIMPLE_DEV_PM_OPS(pm822_rtc_pm_ops, pm822_rtc_suspend, pm822_rtc_resume);

static int __devinit pm822_rtc_probe(struct platform_device *pdev)
{
	struct pm822_chip *chip = dev_get_drvdata(pdev->dev.parent);
	struct pm822_platform_data *pm822_pdata;
	struct pm822_rtc_pdata *pdata = NULL;
	struct pm822_rtc_info *info;
	struct rtc_time tm;
	unsigned long ticks = 0;
	int irq, ret;
	unsigned long now_ticks = 0, default_ticks = 0;
	struct rtc_time default_time; 

	pdata = pdev->dev.platform_data;
	if (pdata == NULL)
		dev_warn(&pdev->dev, "No platform data!\n");

	info =
	    devm_kzalloc(&pdev->dev, sizeof(struct pm822_rtc_info), GFP_KERNEL);
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

	ret = pm822_rtc_read_time(&pdev->dev, &tm);
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
		tm.tm_year = 114;
		tm.tm_mon = 0;
		tm.tm_mday = 1;
		tm.tm_hour = 0;
		tm.tm_min = 0;
		tm.tm_sec = 0;
		ret = pm822_rtc_set_time(&pdev->dev,&tm);
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
		ret = pm822_rtc_set_time(&pdev->dev, &tm);
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

	info->rtc_dev = rtc_device_register("88pm822-rtc", &pdev->dev,
					    &pm822_rtc_ops, THIS_MODULE);
	ret = PTR_ERR(info->rtc_dev);
	if (IS_ERR(info->rtc_dev)) {
		dev_err(&pdev->dev, "Failed to register RTC device: %d\n", ret);
		goto out;
	}
	/*
	 * enable internal XO instead of internal 3.25MHz clock since it can
	 * free running in PMIC power-down state.
	 */
	regmap_update_bits(info->map, PM822_RTC_CTRL, PM822_RTC1_USE_XO,
			   PM822_RTC1_USE_XO);

	if (pdev->dev.parent->platform_data) {
		pm822_pdata = pdev->dev.parent->platform_data;
		pdata = pm822_pdata->rtc;
		if (pdata)
			info->rtc_dev->dev.platform_data = &pdata->rtc_wakeup;
	}

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

	ret = pm822_request_irq(chip, info->irq, rtc_update_handler,
				IRQF_ONESHOT, "88pm822-rtc", info);
	if (ret < 0) {
		dev_err(chip->dev, "Failed to request IRQ: #%d: %d\n",
			info->irq, ret);
		goto out_rtc;
	}

	device_init_wakeup(&pdev->dev, 1);

	return 0;
out_rtc:
	pm822_free_irq(chip, info->irq, info);
out:
	return ret;
}

static int __devexit pm822_rtc_remove(struct platform_device *pdev)
{
	struct pm822_rtc_info *info = platform_get_drvdata(pdev);
	platform_set_drvdata(pdev, NULL);
	rtc_device_unregister(info->rtc_dev);
	pm822_free_irq(info->chip, info->irq, info);
	return 0;
}

static struct platform_driver pm822_rtc_driver = {
	.driver = {
		   .name = "88pm822-rtc",
		   .owner = THIS_MODULE,
		   .pm = &pm822_rtc_pm_ops,
		   },
	.probe = pm822_rtc_probe,
	.remove = __devexit_p(pm822_rtc_remove),
};

module_platform_driver(pm822_rtc_driver);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Marvell 88PM822 RTC driver");
MODULE_AUTHOR("Hongyan Song <hysong@marvell.com>");
MODULE_ALIAS("platform:88pm822-rtc");
