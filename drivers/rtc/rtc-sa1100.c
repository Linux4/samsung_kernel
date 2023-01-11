/*
 * Real Time Clock interface for StrongARM SA1x00 and XScale PXA2xx
 *
 * Copyright (c) 2000 Nils Faerber
 *
 * Based on rtc.c by Paul Gortmaker
 *
 * Original Driver by Nils Faerber <nils@kernelconcepts.de>
 *
 * Modifications from:
 *   CIH <cih@coventive.com>
 *   Nicolas Pitre <nico@fluxnic.net>
 *   Andrew Christian <andrew.christian@hp.com>
 *
 * Converted to the RTC subsystem and Driver Model
 *   by Richard Purdie <rpurdie@rpsys.net>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */

#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/clk.h>
#include <linux/rtc.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/of.h>
#include <linux/pm.h>
#include <linux/bitops.h>
#include <linux/io.h>
#include <linux/delay.h>

#define RCNR		0x00	/* RTC Count Register */
#define RTAR		0x04	/* RTC Alarm Register */
#define RTSR		0x08	/* RTC Status Register */
#define RTTR		0x0C	/* RTC Timer Trim Register */

#define RTSR_HZE	(1 << 3)	/* HZ interrupt enable */
#define RTSR_ALE	(1 << 2)	/* RTC alarm interrupt enable */
#define RTSR_HZ		(1 << 1)	/* HZ rising-edge detected */
#define RTSR_AL		(1 << 0)	/* RTC alarm detected */

#define RTC_DEF_DIVIDER		(32768 - 1)
#define RTC_DEF_TRIM		0

#define RTC_DIVIDER		RTC_DEF_DIVIDER
#define RTC_TRIM		RTC_DEF_TRIM
#define RTC_FREQ		1024

struct sa1100_rtc {
	spinlock_t		lock;
	void __iomem		*base;
	int			irq_1hz;
	int			irq_alarm;
	struct rtc_device	*rtc;
	struct clk		*clk;
};

static void __iomem		*rtc_reg_base;
int sync_time_to_soc(unsigned int ticks)
{
	if (rtc_reg_base) {
		writel(ticks, rtc_reg_base + RCNR);
		/*
		 * for soc rtc time setting, it needs some delay to ensure setting
		 * to take effective
		 */
		udelay(200);
	}
	return 0;
}
EXPORT_SYMBOL(sync_time_to_soc);

static irqreturn_t sa1100_rtc_interrupt(int irq, void *dev_id)
{
	struct sa1100_rtc *info = dev_get_drvdata(dev_id);
	struct rtc_device *rtc = info->rtc;
	unsigned int rtsr;
	unsigned long events = 0;

	spin_lock(&info->lock);

	rtsr = readl(info->base + RTSR);
	/* clear interrupt sources */
	writel(0, info->base + RTSR);
	/* Fix for a nasty initialization problem the in SA11xx RTSR register.
	 * See also the comments in sa1100_rtc_probe(). */
	if (rtsr & (RTSR_ALE | RTSR_HZE)) {
		/* This is the original code, before there was the if test
		 * above. This code does not clear interrupts that were not
		 * enabled. */
		writel((RTSR_AL | RTSR_HZ) & (rtsr >> 2), info->base + RTSR);
	} else {
		/* For some reason, it is possible to enter this routine
		 * without interruptions enabled, it has been tested with
		 * several units (Bug in SA11xx chip?).
		 *
		 * This situation leads to an infinite "loop" of interrupt
		 * routine calling and as a result the processor seems to
		 * lock on its first call to open(). */
		writel(RTSR_AL | RTSR_HZ, info->base + RTSR);
	}

	/* clear alarm interrupt if it has occurred */
	if (rtsr & RTSR_AL)
		rtsr &= ~RTSR_ALE;
	writel(rtsr & (RTSR_ALE | RTSR_HZE), info->base + RTSR);

	/* update irq data & counter */
	if (rtsr & RTSR_AL)
		events |= RTC_AF | RTC_IRQF;
	if (rtsr & RTSR_HZ)
		events |= RTC_UF | RTC_IRQF;

	rtc_update_irq(rtc, 1, events);

	spin_unlock(&info->lock);

	return IRQ_HANDLED;
}

static int sa1100_rtc_alarm_irq_enable(struct device *dev, unsigned int enabled)
{
	struct sa1100_rtc *info = dev_get_drvdata(dev);

	spin_lock_irq(&info->lock);
	if (enabled)
		writel(readl(info->base + RTSR) | RTSR_ALE, info->base + RTSR);
	else
		writel(readl(info->base + RTSR) & ~RTSR_ALE, info->base + RTSR);
	spin_unlock_irq(&info->lock);
	return 0;
}

static int sa1100_rtc_read_time(struct device *dev, struct rtc_time *tm)
{
	struct sa1100_rtc *info = dev_get_drvdata(dev);
	rtc_time_to_tm(readl(info->base + RCNR), tm);
	return 0;
}

static int sa1100_rtc_set_time(struct device *dev, struct rtc_time *tm)
{
	unsigned long time;
	int ret;
	struct sa1100_rtc *info = dev_get_drvdata(dev);

	if ((tm->tm_year < 70) || (tm->tm_year > 138))
		return -EINVAL;

	ret = rtc_tm_to_time(tm, &time);
	if (ret == 0)
		writel(time, info->base + RCNR);
	/* stabilize the register value */
	udelay(200);
	return ret;
}

static int sa1100_rtc_read_alarm(struct device *dev, struct rtc_wkalrm *alrm)
{
	u32	rtsr;
	struct sa1100_rtc *info = dev_get_drvdata(dev);

	rtsr = readl(info->base + RTSR);
	alrm->enabled = (rtsr & RTSR_ALE) ? 1 : 0;
	alrm->pending = (rtsr & RTSR_AL) ? 1 : 0;
	return 0;
}

static int sa1100_rtc_set_alarm(struct device *dev, struct rtc_wkalrm *alrm)
{
	struct sa1100_rtc *info = dev_get_drvdata(dev);
	unsigned long time;
	int ret;

	spin_lock_irq(&info->lock);
	ret = rtc_tm_to_time(&alrm->time, &time);
	if (ret != 0)
		goto out;
	writel(readl(info->base + RTSR) & (RTSR_HZE|RTSR_ALE|RTSR_AL),
						info->base + RTSR);
	writel(time, info->base + RTAR);
	if (alrm->enabled)
		writel(readl(info->base + RTSR) | RTSR_ALE, info->base + RTSR);
	else
		writel(readl(info->base + RTSR) & ~RTSR_ALE, info->base + RTSR);
out:
	spin_unlock_irq(&info->lock);

	return ret;
}

static int sa1100_rtc_proc(struct device *dev, struct seq_file *seq)
{
	struct sa1100_rtc *info = dev_get_drvdata(dev);
	seq_printf(seq, "trim/divider\t\t: 0x%08x\n", readl(info->base + RTTR));
	seq_printf(seq, "RTSR\t\t\t: 0x%08x\n", readl(info->base + RTSR));

	return 0;
}

static const struct rtc_class_ops sa1100_rtc_ops = {
	.read_time = sa1100_rtc_read_time,
	.set_time = sa1100_rtc_set_time,
	.read_alarm = sa1100_rtc_read_alarm,
	.set_alarm = sa1100_rtc_set_alarm,
	.proc = sa1100_rtc_proc,
	.alarm_irq_enable = sa1100_rtc_alarm_irq_enable,
};

static int sa1100_rtc_probe(struct platform_device *pdev)
{
	struct rtc_device *rtc;
	struct sa1100_rtc *info;
	struct resource *iores;
	int irq_1hz, irq_alarm, ret = 0;

	irq_1hz = platform_get_irq_byname(pdev, "rtc 1Hz");
	irq_alarm = platform_get_irq_byname(pdev, "rtc alarm");
	if (irq_1hz < 0 || irq_alarm < 0)
		return -ENODEV;

	info = devm_kzalloc(&pdev->dev, sizeof(struct sa1100_rtc), GFP_KERNEL);
	if (!info)
		return -ENOMEM;
	info->clk = devm_clk_get(&pdev->dev, NULL);
	if (IS_ERR(info->clk)) {
		dev_err(&pdev->dev, "failed to find rtc clock source\n");
		return PTR_ERR(info->clk);
	}
	info->irq_1hz = irq_1hz;
	info->irq_alarm = irq_alarm;

	iores = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	info->base = devm_ioremap_resource(&pdev->dev, iores);
	rtc_reg_base = info->base;
	if (IS_ERR(info->base))
		return PTR_ERR(info->base);

	spin_lock_init(&info->lock);
	platform_set_drvdata(pdev, info);

	ret = clk_prepare_enable(info->clk);
	if (ret)
		return ret;

	device_init_wakeup(&pdev->dev, 1);

	rtc = devm_rtc_device_register(&pdev->dev, pdev->name, &sa1100_rtc_ops,
					THIS_MODULE);

	if (IS_ERR(rtc)) {
		ret = PTR_ERR(rtc);
		goto err_dev;
	}
	info->rtc = rtc;

	/* Fix for a nasty initialization problem the in SA11xx RTSR register.
	 * See also the comments in sa1100_rtc_interrupt().
	 *
	 * Sometimes bit 1 of the RTSR (RTSR_HZ) will wake up 1, which means an
	 * interrupt pending, even though interrupts were never enabled.
	 * In this case, this bit it must be reset before enabling
	 * interruptions to avoid a nonexistent interrupt to occur.
	 *
	 * In principle, the same problem would apply to bit 0, although it has
	 * never been observed to happen.
	 *
	 * This issue is addressed both here and in sa1100_rtc_interrupt().
	 * If the issue is not addressed here, in the times when the processor
	 * wakes up with the bit set there will be one spurious interrupt.
	 *
	 * The issue is also dealt with in sa1100_rtc_interrupt() to be on the
	 * safe side, once the condition that lead to this strange
	 * initialization is unknown and could in principle happen during
	 * normal processing.
	 *
	 * Notice that clearing bit 1 and 0 is accomplished by writting ONES to
	 * the corresponding bits in RTSR. */
	writel(RTSR_AL | RTSR_HZ, info->base + RTSR);

	ret = devm_request_irq(&pdev->dev, info->irq_1hz,
			       sa1100_rtc_interrupt, 0, "rtc 1Hz", &pdev->dev);
	if (ret) {
		dev_err(&pdev->dev, "IRQ %d request error.\n", info->irq_1hz);
		goto err_dev;
	}
	ret = devm_request_irq(&pdev->dev, info->irq_alarm,
			       sa1100_rtc_interrupt, 0, "rtc Alrm", &pdev->dev);
	if (ret) {
		dev_err(&pdev->dev, "IRQ %d request error.\n", info->irq_alarm);
		goto err_dev;
	}
	rtc->max_user_freq = RTC_FREQ;
	rtc_irq_set_freq(rtc, NULL, RTC_FREQ);

	/* RTTR is set as default value 0x7fff when the clock is enabled */
	if ((RTC_TRIM != RTC_DEF_TRIM)
	    || (RTC_DIVIDER != RTC_DEF_DIVIDER)) {
		writel(RTC_DIVIDER + (RTC_TRIM << 16), info->base + RTTR);
		dev_warn(&pdev->dev, "warning: "
			"initializing default clock divider/trim value\n");
		/* The current RTC value probably doesn't make sense either */
		writel(0, info->base + RCNR);
	}

	return 0;
err_dev:
	clk_disable_unprepare(info->clk);
	return ret;
}

static int sa1100_rtc_remove(struct platform_device *pdev)
{
	struct sa1100_rtc *info = platform_get_drvdata(pdev);

	if (info) {
		spin_lock_irq(&info->lock);
		writel(0, info->base + RTSR);
		spin_unlock_irq(&info->lock);
		clk_disable_unprepare(info->clk);
	}

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int sa1100_rtc_suspend(struct device *dev)
{
	struct sa1100_rtc *info = dev_get_drvdata(dev);
	if (device_may_wakeup(dev))
		enable_irq_wake(info->irq_alarm);
	return 0;
}

static int sa1100_rtc_resume(struct device *dev)
{
	struct sa1100_rtc *info = dev_get_drvdata(dev);
	if (device_may_wakeup(dev))
		disable_irq_wake(info->irq_alarm);
	return 0;
}
#endif

static SIMPLE_DEV_PM_OPS(sa1100_rtc_pm_ops, sa1100_rtc_suspend,
			sa1100_rtc_resume);

#ifdef CONFIG_OF
static struct of_device_id sa1100_rtc_dt_ids[] = {
	{ .compatible = "mrvl,sa1100-rtc", },
	{ .compatible = "mrvl,mmp-rtc", },
	{}
};
MODULE_DEVICE_TABLE(of, sa1100_rtc_dt_ids);
#endif

static struct platform_driver sa1100_rtc_driver = {
	.probe		= sa1100_rtc_probe,
	.remove		= sa1100_rtc_remove,
	.driver		= {
		.name	= "sa1100-rtc",
		.pm	= &sa1100_rtc_pm_ops,
		.of_match_table = of_match_ptr(sa1100_rtc_dt_ids),
	},
};

module_platform_driver(sa1100_rtc_driver);

MODULE_AUTHOR("Richard Purdie <rpurdie@rpsys.net>");
MODULE_DESCRIPTION("SA11x0/PXA2xx Realtime Clock Driver (RTC)");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:sa1100-rtc");
