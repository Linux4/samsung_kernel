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
#include <linux/reboot.h>

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

#ifdef CONFIG_RTC_DRV_SA1100
extern int sync_time_to_soc(unsigned int ticks);
#endif

static void deferred_restart(struct work_struct *dummy)
{
	kernel_restart("charger-alarm");
	BUG();
}
static DECLARE_WORK(restart_work, deferred_restart);

static irqreturn_t rtc_update_handler(int irq, void *data)
{
	struct pm80x_rtc_info *info = (struct pm80x_rtc_info *)data;
	int mask;

	/*
	 * write 1 to clear PM800_ALARM (bit 5 of 0xd0),
	 * which indicates the alarm event has happened;
	 *
	 * write 1 to PM800_ALARM_WAKEUP (bit 4 of 0xd0),
	 * which is must for "expired-alarm powers up system",
	 * it triggers falling edge of RTC_ALARM_WU signal,
	 * while the "power down" triggers the rising edge of RTC_ALARM_WU signal;
	 *
	 * write 0 to PM800_ALARM1_EN (bit 0 0f 0xd0) to disable alarm;
	 */
	mask = PM800_ALARM | PM800_ALARM_WAKEUP;
	regmap_update_bits(info->map, PM800_RTC_CONTROL, mask | PM800_ALARM1_EN,
			   mask);
	rtc_update_irq(info->rtc_dev, 1, RTC_AF);

	if (strstr(saved_command_line, "androidboot.mode=charger")) {
		/*
		 * for uboot,
		 * this bit indicates the system powers up because of "reboot",
		 * then it boots up to the generic android instead of entering
		 * power-off charge
		 */
		regmap_update_bits(info->map, PM800_USER_DATA6, (1 << 1), (1 << 1));
		schedule_work(&restart_work);
	}

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

static int pm80x_rtc_read_time(struct device *dev, struct rtc_time *tm)
{
	struct pm80x_rtc_info *info = dev_get_drvdata(dev);
	unsigned char buf[4];
	unsigned long ticks, data;
	long base;
	regmap_raw_read(info->map, PM800_USER_DATA1, buf, 4);
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
	regmap_raw_read(info->map, PM800_RTC_COUNTER1, buf, 4);
	data = (buf[3] << 24) | (buf[2] << 16) | (buf[1] << 8) | buf[0];
	base = ticks - data;
	dev_dbg(info->dev, "set base:0x%lx, RO count:0x%lx, ticks:0x%lx\n",
		base, data, ticks);
	buf[0] = base & 0xFF;
	buf[1] = (base >> 8) & 0xFF;
	buf[2] = (base >> 16) & 0xFF;
	buf[3] = (base >> 24) & 0xFF;
	regmap_raw_write(info->map, PM800_USER_DATA1, buf, 4);

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

	regmap_raw_read(info->map, PM800_USER_DATA1, buf, 4);
	base = (buf[3] << 24) | (buf[2] << 16) | (buf[1] << 8) | buf[0];
	dev_dbg(info->dev, "%x-%x-%x-%x\n", buf[0], buf[1], buf[2], buf[3]);

	regmap_raw_read(info->map, PM800_RTC_EXPIRE1_1, buf, 4);
	data = (buf[3] << 24) | (buf[2] << 16) | (buf[1] << 8) | buf[0];
	ticks = base + data;
	dev_dbg(info->dev, "get base:0x%lx, RO count:0x%lx, ticks:0x%lx\n",
		base, data, ticks);

	rtc_time_to_tm(ticks, &alrm->time);
	regmap_read(info->map, PM800_RTC_CONTROL, &ret);
	alrm->enabled = (ret & PM800_ALARM1_EN) ? 1 : 0;
	alrm->pending = (ret & (PM800_ALARM | PM800_ALARM_WAKEUP)) ? 1 : 0;
	return 0;
}

static int pm80x_rtc_set_alarm(struct device *dev, struct rtc_wkalrm *alrm)
{
	struct pm80x_rtc_info *info = dev_get_drvdata(dev);
	unsigned long ticks, base, data;
	unsigned char buf[4];
	int mask;

	regmap_update_bits(info->map, PM800_RTC_CONTROL, PM800_ALARM1_EN, 0);

	regmap_raw_read(info->map, PM800_USER_DATA1, buf, 4);
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

#ifdef CONFIG_PM_SLEEP
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

static int pm80x_rtc_probe(struct platform_device *pdev)
{
	struct pm80x_chip *chip = dev_get_drvdata(pdev->dev.parent);
	struct pm80x_rtc_pdata *pdata = NULL;
	struct pm80x_rtc_info *info;
	struct rtc_time tm;
	unsigned long ticks = 0;
	int ret;

	pdata = dev_get_platdata(&pdev->dev);
	if (IS_ENABLED(CONFIG_OF)) {
		if (!pdata) {
			pdata = devm_kzalloc(&pdev->dev,
					     sizeof(*pdata), GFP_KERNEL);
			if (!pdata)
				return -ENOMEM;
		}
	} else if (!pdata) {
		return -EINVAL;
	}

	info =
	    devm_kzalloc(&pdev->dev, sizeof(struct pm80x_rtc_info), GFP_KERNEL);
	if (!info)
		return -ENOMEM;
	info->irq = platform_get_irq(pdev, 0);
	if (info->irq < 0) {
		dev_err(&pdev->dev, "No IRQ resource!\n");
		ret = -EINVAL;
		goto out;
	}

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
	rtc_tm_to_time(&tm, &ticks);

	info->sync = NULL; /*initialize*/
#ifdef CONFIG_RTC_DRV_SA1100
	sync_time_to_soc(ticks);
	info->sync = sync_time_to_soc;
#endif

	dev_info(&pdev->dev, "pmic rtc initial time: %d-%d-%d-->%d:%d:%d\n",
	 tm.tm_year, tm.tm_mon, tm.tm_mday,
	 tm.tm_hour, tm.tm_min, tm.tm_sec);

	device_init_wakeup(&pdev->dev, 1);

	info->rtc_dev = devm_rtc_device_register(&pdev->dev, "88pm80x-rtc",
					    &pm80x_rtc_ops, THIS_MODULE);
	if (IS_ERR(info->rtc_dev)) {
		ret = PTR_ERR(info->rtc_dev);
		dev_err(&pdev->dev, "Failed to register RTC device: %d\n", ret);
		goto out;
	}

	/*
	 * enable internal XO instead of internal 3.25MHz clock since it can
	 * free running in PMIC power-down state.
	 */
	regmap_update_bits(info->map, PM800_RTC_CONTROL, PM800_RTC1_USE_XO,
			   PM800_RTC1_USE_XO);

	/* remeber whether this power up is caused by PMIC RTC or not. */
	info->rtc_dev->dev.platform_data = &pdata->rtc_wakeup;

	ret = pm80x_request_irq(chip, info->irq, rtc_update_handler,
				IRQF_ONESHOT | IRQF_NO_SUSPEND, "rtc", info);
	if (ret < 0) {
		dev_err(chip->dev, "Failed to request IRQ: #%d: %d\n",
			info->irq, ret);
		goto out;
	}

	return 0;
out:
	return ret;
}

static int pm80x_rtc_remove(struct platform_device *pdev)
{
	struct pm80x_rtc_info *info = platform_get_drvdata(pdev);
	platform_set_drvdata(pdev, NULL);
	pm80x_free_irq(info->chip, info->irq, info);
	return 0;
}

static struct platform_driver pm80x_rtc_driver = {
	.driver = {
		   .name = "88pm80x-rtc",
		   .owner = THIS_MODULE,
		   .pm = &pm80x_rtc_pm_ops,
		   },
	.probe = pm80x_rtc_probe,
	.remove = pm80x_rtc_remove,
};

module_platform_driver(pm80x_rtc_driver);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Marvell 88PM80x RTC driver");
MODULE_AUTHOR("Qiao Zhou <zhouqiao@marvell.com>");
MODULE_ALIAS("platform:88pm80x-rtc");
