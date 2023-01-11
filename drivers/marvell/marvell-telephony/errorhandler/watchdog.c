/*

 *(C) Copyright 2007 Marvell International Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * All Rights Reserved
 */

#include <linux/gfp.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/cputype.h>
#include <linux/irq.h>
#include <linux/irqnr.h>
#include <linux/irqdesc.h>
#include "watchdog.h"
#include "common_regs.h"

#undef REQUEST_MEM_REGION

enum cp_watchdog_type {
	cp_wdt_type_wdt_timer,
	cp_wdt_type_soc_timer,
	cp_wdt_type_cp_timer,

	cp_wdt_type_cnt
};

struct timer_operation {
	int (*init)(struct cp_watchdog *watchdog);
	void (*exit)(void *data);
	void (*enable_write_access)(void *data);
	void (*enable)(void *data);
	void (*disable)(void *data);
	unsigned long (*get_enable_status)(void *data);
	unsigned long (*get_status)(void *data);
	unsigned long (*get_value)(void *data);
	unsigned long (*get_match)(void *data);
	void (*clear_interrupt)(void *data);
	void (*reset_counter)(void *data);
	int (*suspend)(struct cp_watchdog *watchdog);
	int (*resume)(struct cp_watchdog *watchdog);
};

struct wdt_timer {
	void __iomem *base;
};

static int wdt_init(struct cp_watchdog *watchdog)
{
	struct wdt_timer *wt = (struct wdt_timer *)watchdog->data;

	wt->base = watchdog->base;

	pr_info("%s: base %p\n", __func__, wt->base);

	return 0;
}

static void wdt_exit(void *data)
{
	(void)data;
}

static void wdt_enable_write_access(void *data)
{
	struct wdt_timer *wt = (struct wdt_timer *)data;

	writel(TMR_WFAR_KEY, wt->base + TMR_WFAR);
	writel(TMR_WSAR_KEY, wt->base + TMR_WSAR);
}

static void wdt_enable(void *data)
{
	struct wdt_timer *wt = (struct wdt_timer *)data;
	unsigned long value;

	wdt_enable_write_access(data);

	value = readl(wt->base + TMR_WMER);
	value |= TMR_WMER_WE;
	writel(value, wt->base + TMR_WMER);
}

static void wdt_disable(void *data)
{
	struct wdt_timer *wt = (struct wdt_timer *)data;
	unsigned long value;

	wdt_enable_write_access(data);

	value = readl(wt->base + TMR_WMER);
	value &= ~TMR_WMER_WE;
	writel(value, wt->base + TMR_WMER);
}

static unsigned long wdt_get_enable_status(void *data)
{
	struct wdt_timer *wt = (struct wdt_timer *)data;

	return readl(wt->base + TMR_WMER);
}

static unsigned long wdt_get_status(void *data)
{
	struct wdt_timer *wt = (struct wdt_timer *)data;

	return readl(wt->base + TMR_WSR);
}

static unsigned long wdt_get_value(void *data)
{
	struct wdt_timer *wt = (struct wdt_timer *)data;

	return readl(wt->base + TMR_WVR);
}

static unsigned long wdt_get_match(void *data)
{
	struct wdt_timer *wt = (struct wdt_timer *)data;

	return readl(wt->base + TMR_WMR);
}

static void wdt_clear_interrupt(void *data)
{
	struct wdt_timer *wt = (struct wdt_timer *)data;

	wdt_enable_write_access(data);
	writel(1, wt->base + TMR_WICR);
}

static void wdt_reset_counter(void *data)
{
	struct wdt_timer *wt = (struct wdt_timer *)data;

	wdt_enable_write_access(data);
	writel(1, wt->base + TMR_WCR);
}

struct soc_timer {
	void __iomem *base;

	u32 timer_num;
	u32 match_num;
};

static int soc_init(struct cp_watchdog *watchdog)
{
	struct soc_timer *st = (struct soc_timer *)watchdog->data;

	st->base = watchdog->base;
	st->timer_num = watchdog->timer_num;
	st->match_num = watchdog->match_num;

	pr_info("%s: base %p, timer_num %u, match_num %u\n",
		__func__, st->base, st->timer_num, st->match_num);

	return 0;
}

static void soc_exit(void *data)
{
	(void)data;
}

static void soc_enable(void *data)
{
	struct soc_timer *st = (struct soc_timer *)data;
	unsigned long value;

	value = readl(st->base + TMR_CER);
	value |= (0x1 << st->timer_num);
	writel(value, st->base + TMR_CER);
}

static void soc_disable(void *data)
{
	struct soc_timer *st = (struct soc_timer *)data;
	unsigned long value;

	value = readl(st->base + TMR_CER);
	value &= ~(0x1 << st->timer_num);
	writel(value, st->base + TMR_CER);
}

static unsigned long soc_get_enable_status(void *data)
{
	struct soc_timer *st = (struct soc_timer *)data;

	return readl(st->base + TMR_CER);
}

static unsigned long soc_get_status(void *data)
{
	struct soc_timer *st = (struct soc_timer *)data;

	return readl(st->base + TMR_SR(st->timer_num));
}

static unsigned long soc_get_value(void *data)
{
	struct soc_timer *st = (struct soc_timer *)data;

	return readl(st->base + TMR_CR(st->timer_num));
}

static unsigned long soc_get_match(void *data)
{
	struct soc_timer *st = (struct soc_timer *)data;

	return readl(st->base + TMR_TN_MM(st->timer_num, st->match_num));
}

static void soc_clear_interrupt(void *data)
{
	struct soc_timer *st = (struct soc_timer *)data;
	unsigned long value;

	value = readl(st->base + TMR_ICR(st->timer_num));
	value |= (0x1 << st->match_num);
	writel(value, st->base + TMR_ICR(st->timer_num));
}

struct cp_timer {
	int irq;
};

static int cp_timer_init(struct cp_watchdog *watchdog)
{
	struct cp_timer *cpt = (struct cp_timer *)watchdog->data;

	cpt->irq = watchdog->irq;
	return 0;
}

static void cp_timer_disable(void *data)
{
	struct cp_timer *cpt = (struct cp_timer *)data;

	struct irq_desc *desc = irq_to_desc(cpt->irq);
	if (desc->irq_data.chip->irq_mask) {
		/* mask gic interrupt*/
		desc->irq_data.chip->irq_mask(&desc->irq_data);
		desc->irq_data.state_use_accessors |= IRQD_IRQ_MASKED;
	} else {
		pr_err("%s : irq_mask is NULL\n", __func__);
	}
}

static int cp_timer_suspend(struct cp_watchdog *watchdog)
{
	return enable_irq_wake(watchdog->irq);
}

static int cp_timer_resume(struct cp_watchdog *watchdog)
{
	return disable_irq_wake(watchdog->irq);
}

static const struct timer_operation wdt_timer_ops = {
	.init				= wdt_init,
	.exit				= wdt_exit,
	.enable_write_access	= wdt_enable_write_access,
	.enable				= wdt_enable,
	.disable			= wdt_disable,
	.get_enable_status	= wdt_get_enable_status,
	.get_status			= wdt_get_status,
	.get_value			= wdt_get_value,
	.get_match			= wdt_get_match,
	.clear_interrupt	= wdt_clear_interrupt,
	.reset_counter		= wdt_reset_counter,
};

static const struct timer_operation soc_timer_ops = {
	.init				= soc_init,
	.exit				= soc_exit,
	.enable				= soc_enable,
	.disable			= soc_disable,
	.get_enable_status	= soc_get_enable_status,
	.get_status			= soc_get_status,
	.get_value			= soc_get_value,
	.get_match			= soc_get_match,
	.clear_interrupt	= soc_clear_interrupt,
};

static const struct timer_operation cp_timer_ops = {
	.init				= cp_timer_init,
	.disable			= cp_timer_disable,
	.suspend			= cp_timer_suspend,
	.resume				= cp_timer_resume,
};

struct cp_watchdog *cp_watchdog;

int cp_watchdog_probe(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	struct resource *res;
	int ret = -EINVAL;
	u32 type;

	cp_watchdog = devm_kzalloc(&pdev->dev,
		sizeof(struct cp_watchdog), GFP_KERNEL);

	if (!cp_watchdog)
		return -ENOMEM;

	if (of_property_read_u32(np, "watchdog-type", &type)) {
		dev_err(&pdev->dev, "%s: no watchdog type defined\n", __func__);
		ret = -EINVAL;
		goto freemem;
	}

	dev_info(&pdev->dev, "%s: watchdog type %d\n", __func__, type);

	switch (type) {
	case cp_wdt_type_wdt_timer:

		cp_watchdog->data = devm_kzalloc(&pdev->dev,
			sizeof(struct wdt_timer), GFP_KERNEL);
		if (!cp_watchdog->data) {
			ret = -ENOMEM;
			goto freemem;
		}
		cp_watchdog->ops = &wdt_timer_ops;
		break;

	case cp_wdt_type_soc_timer:

		cp_watchdog->data = devm_kzalloc(&pdev->dev,
			sizeof(struct soc_timer), GFP_KERNEL);
		if (!cp_watchdog->data) {
			ret = -ENOMEM;
			goto freemem;
		}
		cp_watchdog->ops = &soc_timer_ops;

		if (of_property_read_u32(np, "timer-num",
				&cp_watchdog->timer_num)) {
			dev_err(&pdev->dev,
				"%s: no timer num defined\n", __func__);
			ret = -EINVAL;
			goto freemem;
		}
		if (of_property_read_u32(np, "match-num",
				&cp_watchdog->match_num)) {
			dev_err(&pdev->dev,
				"%s: no match num defined\n", __func__);
			ret = -EINVAL;
			goto freemem;
		}
		dev_info(&pdev->dev,
			"%s: timer-num %u, match-num %u\n", __func__,
			cp_watchdog->timer_num, cp_watchdog->match_num);
		break;

	case cp_wdt_type_cp_timer:
		cp_watchdog->data = devm_kzalloc(&pdev->dev,
			sizeof(struct cp_timer), GFP_KERNEL);
		if (!cp_watchdog->data) {
			ret = -ENOMEM;
			goto freemem;
		}

		cp_watchdog->ops = &cp_timer_ops;
		break;

	default:
		dev_err(&pdev->dev, "%s: wrong watchdog type %u\n",
			__func__, type);
		ret = -EINVAL;
		goto freemem;
	}

	cp_watchdog->irq = platform_get_irq(pdev, 0);
	if (cp_watchdog->irq < 0) {
		dev_err(&pdev->dev, "%s: no irq defined\n", __func__);
		ret = -ENXIO;
		goto freemem;
	}

	if (cp_wdt_type_cp_timer != type) {
		res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
		if (!res) {
			dev_err(&pdev->dev, "%s: no iomem defined\n", __func__);
			ret = -ENOMEM;
			goto freemem;
		}

#ifdef REQUEST_MEM_REGION
		if (!devm_request_mem_region(&pdev->dev, res->start,
				resource_size(res), "cp-watchdog")) {
			dev_err(&pdev->dev,
				"%s: can't request region for resource %pR\n",
				__func__, res);
			ret = -EINVAL;
			goto freemem;
		}
#endif

		cp_watchdog->base = devm_ioremap_nocache(&pdev->dev,
			res->start, resource_size(res));
		if (!cp_watchdog->base) {
			dev_err(&pdev->dev,
				"%s: map res %lx - %lx failed\n",
				__func__, (unsigned long)res->start,
				(unsigned long)res->end);
			ret = -ENOMEM;
			goto freememreg;
		}
	}
	if (cp_watchdog->ops->init && cp_watchdog->ops->init(cp_watchdog) < 0) {
		pr_err("%s: init watchdog error\n", __func__);
		ret = -ENODEV;
		goto freeiomap;
	}

	cp_watchdog->type = type;
	platform_set_drvdata(pdev, cp_watchdog);

	dev_info(&pdev->dev, "%s: init watchdog success\n", __func__);

	return 0;

freeiomap:
	if (cp_wdt_type_cp_timer != type)
		devm_iounmap(&pdev->dev, cp_watchdog->base);
freememreg:
#ifdef REQUEST_MEM_REGION
	if (cp_wdt_type_cp_timer != type)
		devm_release_mem_region(&pdev->dev, res->start,
			resource_size(res));
#endif
freemem:
	devm_kfree(&pdev->dev, cp_watchdog->data);
	devm_kfree(&pdev->dev, cp_watchdog);
	cp_watchdog = NULL;
	return ret;
}

int cp_watchdog_remove(struct platform_device *pdev)
{
#ifdef REQUEST_MEM_REGION
	struct resource *res = NULL;
#endif

	if (cp_watchdog->ops->exit)
		cp_watchdog->ops->exit(cp_watchdog->data);

	if (cp_wdt_type_cp_timer != cp_watchdog->type) {
		devm_iounmap(&pdev->dev, cp_watchdog->base);
#ifdef REQUEST_MEM_REGION
		res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
		if (res)
			devm_release_mem_region(&pdev->dev, res->start,
				resource_size(res));
#endif
	}
	devm_kfree(&pdev->dev, cp_watchdog->data);
	devm_kfree(&pdev->dev, cp_watchdog);
	cp_watchdog = NULL;
	platform_set_drvdata(pdev, NULL);

	return 0;
}

void watchdog_count_stop(void)
{
	pr_info("stop watchdog\n");

	if (cp_watchdog && cp_watchdog->ops &&
		cp_watchdog->ops->disable)
		cp_watchdog->ops->disable(cp_watchdog->data);

	pr_info("done!\n");
}

void watchdog_interrupt_clear(void)
{
	pr_info("clear watchdog interrupt\n");

	if (cp_watchdog && cp_watchdog->ops &&
		cp_watchdog->ops->clear_interrupt)
		cp_watchdog->ops->clear_interrupt(cp_watchdog->data);

	pr_info("done!\n");
}

void watchdog_hw_kick(void)
{
	pr_info("kick watchdog\n");

	if (cp_watchdog && cp_watchdog->ops &&
		cp_watchdog->ops->reset_counter)
		cp_watchdog->ops->reset_counter(cp_watchdog->data);

	pr_info("done!\n");
}

bool watchdog_deactive(void)
{
	unsigned long val0 = 0;
	unsigned long val1 = 0;
	unsigned long match = 0;
	unsigned long enable_status = 0;

	if (!cp_watchdog || !cp_watchdog->ops)
		return false;

	if (cp_watchdog->ops->get_value)
		val0 = cp_watchdog->ops->get_value(cp_watchdog->data);

	if (cp_watchdog->ops->get_value)
		val1 = cp_watchdog->ops->get_value(cp_watchdog->data);

	if (cp_watchdog->ops->get_match)
		match = cp_watchdog->ops->get_match(cp_watchdog->data);

	pr_info("watch value before stop: 0x%lx 0x%lx match 0x%lx\n",
		val0, val1, match);

	watchdog_count_stop();
	watchdog_interrupt_clear();
	watchdog_hw_kick();

	disable_irq_nosync(cp_watchdog->irq);

	if (cp_watchdog->ops->get_value)
		val0 = cp_watchdog->ops->get_value(cp_watchdog->data);

	if (cp_watchdog->ops->get_value)
		val1 = cp_watchdog->ops->get_value(cp_watchdog->data);

	if (cp_watchdog->ops->get_match)
		match = cp_watchdog->ops->get_match(cp_watchdog->data);

	if (cp_watchdog->ops->get_enable_status)
		enable_status =
			cp_watchdog->ops->get_enable_status(cp_watchdog->data);

	pr_info("watch value after stop: 0x%lx 0x%lx match 0x%lx enable 0x%lx\n",
		val0, val1, match, enable_status);

	return true;
}

int watchdog_suspend(void)
{
	if (!cp_watchdog || !cp_watchdog->ops)
		return 0;

	if (cp_watchdog->ops->suspend)
		return cp_watchdog->ops->suspend(cp_watchdog);

	return 0;
}

int watchdog_resume(void)
{
	if (!cp_watchdog || !cp_watchdog->ops)
		return 0;

	if (cp_watchdog->ops->resume)
		return cp_watchdog->ops->resume(cp_watchdog);

	return 0;
}
