/*
 * linux/arch/arm/mach-mmp/gpio-edge.c
 *
 * The GPIO Edge is the edge detect signals coming from the I/O pads.
 * Although the name of this module is the GPIO Edge Unit, it can be
 * used by other I/Os as it is not necessarily for use only by the
 * GPIOs. It's normally used to wake up the system from low power mode.
 *
 * Copyright:   (C) 2012 Marvell International Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/module.h>
#include <linux/io.h>
#include <mach/regs-icu.h>
#include <mach/gpio-edge.h>
#include <mach/irqs.h>
#include <plat/mfp.h>

#ifdef CONFIG_CPU_PXA988
#define ICU_IRQ_ENABLE	((1 << 6) | (1 << 4) | (1 << 0))
#define ICU_IRQ_GPIO_EDGE (IRQ_PXA988_GPIO_EDGE - IRQ_PXA988_START)
#endif

/* The list head of the gpio edge wakeup sources */
static struct list_head gpio_edge_list;
/* The virtual address of the GPIO Edge Unit */
static void __iomem *gpio_edge_base;
/* The total number of GPIOs in the SoC */
static int gpio_edge_gpio_num;
/* The total number of MFPs in the SoC */
static int gpio_edge_mfp_num;
/* The gpio edge detect is enabled */
static int gpio_edge_enabled;

static DEFINE_SPINLOCK(gpio_edge_lock);
static unsigned long flags;

static void gpio_edge_icu_enable(void)
{
	__raw_writel(ICU_IRQ_ENABLE, ICU_INT_CONF(ICU_IRQ_GPIO_EDGE));
}

static void gpio_edge_icu_disable(void)
{
	__raw_writel(0, ICU_INT_CONF(ICU_IRQ_GPIO_EDGE));
}

void gpio_edge_wakeup_enable_default(void)
{
}
void gpio_edge_wakeup_enable(void)
	__attribute__((weak, alias("gpio_edge_wakeup_enable_default")));

void gpio_edge_wakeup_disable_default(void)
{
}
void gpio_edge_wakeup_disable(void)
	__attribute__((weak, alias("gpio_edge_wakeup_disable_default")));

/* Add one gpio edge wakeup source to the list */
int mmp_gpio_edge_add(struct gpio_edge_desc *edge)
{
	struct gpio_edge_desc *e;

	if (!edge || edge->mfp >= gpio_edge_mfp_num) {
		pr_err("error: gpio edge add: wrong param!\n");
		return -EINVAL;
	}

	if (edge->mfp < gpio_edge_gpio_num)
		edge->gpio = edge->mfp;
	else
		BUG_ON(edge->gpio == 0);

	if (edge->gpio >= gpio_edge_gpio_num) {
		pr_err("error: gpio edge add: wrong gpio %u\n", edge->gpio);
		return -EINVAL;
	}

	if (edge->type == 0)
		edge->type = MFP_LPM_EDGE_BOTH;

	spin_lock_irqsave(&gpio_edge_lock, flags);

	list_for_each_entry(e, &gpio_edge_list, list) {
		if (e == edge) {
			pr_err("error: gpio edge: adding exist gpio: %d\n",
				edge->gpio);
			spin_unlock_irqrestore(&gpio_edge_lock, flags);
			return -EEXIST;
		}
	}

	list_add(&edge->list, &gpio_edge_list);

	spin_unlock_irqrestore(&gpio_edge_lock, flags);
	return 0;
}

EXPORT_SYMBOL(mmp_gpio_edge_add);

/* Remove one gpio edge wakeup source from the list */
int mmp_gpio_edge_del(struct gpio_edge_desc *edge)
{
	struct gpio_edge_desc *e;

	if (!edge) {
		pr_err("error: gpio edge del: wrong param!\n");
		return -EINVAL;
	}

	spin_lock_irqsave(&gpio_edge_lock, flags);

	list_for_each_entry(e, &gpio_edge_list, list) {
		if (e == edge) {
			list_del(&edge->list);
			spin_unlock_irqrestore(&gpio_edge_lock, flags);
			return 0;
		}
	}

	spin_unlock_irqrestore(&gpio_edge_lock, flags);

	pr_err("error: gpio edge: del none exist gpio: %d\n", edge->gpio);
	return -ENXIO;
}

EXPORT_SYMBOL(mmp_gpio_edge_del);

/*
 * Enable each gpio edge wakeup source in the list, and enable the corresponing
 * interrupt in the ICU so as to be able to wake up the core in low power mode.
 * Call this function before entering low power mode.
 */
void mmp_gpio_edge_enable(void)
{
	struct gpio_edge_desc *e;

	spin_lock_irqsave(&gpio_edge_lock, flags);

	if (list_empty(&gpio_edge_list)) {
		spin_unlock_irqrestore(&gpio_edge_lock, flags);
		return;
	}

	list_for_each_entry(e, &gpio_edge_list, list) {
		mfp_config_edge(e->mfp, e->type);
	}

	gpio_edge_icu_enable();
	gpio_edge_wakeup_enable();

	gpio_edge_enabled = 1;
	spin_unlock_irqrestore(&gpio_edge_lock, flags);
}

/*
 * Disable the corresponing interrupt in the ICU. Check the gpio edge status
 * and call the handler if exist. Disable all the edge wakeup source in the
 * list. Call the function after exit low power mode.
 */
void mmp_gpio_edge_disable(void)
{
	struct gpio_edge_desc *e;
	int i;

	unsigned long gpioe_rer[6];

	BUG_ON(!gpio_edge_base);

	spin_lock_irqsave(&gpio_edge_lock, flags);

	if (!gpio_edge_enabled) {
		spin_unlock_irqrestore(&gpio_edge_lock, flags);
		return;
	}

	gpio_edge_wakeup_disable();
	gpio_edge_icu_disable();

	for (i = 0; i < (gpio_edge_gpio_num / 32); i++)
		gpioe_rer[i] = __raw_readl(gpio_edge_base + i * 4);

	list_for_each_entry(e, &gpio_edge_list, list) {
		if (test_and_clear_bit(e->gpio, gpioe_rer) && e->handler)
			e->handler(e->mfp, e->data);
		mfp_config_edge(e->mfp, MFP_LPM_EDGE_NONE);
	}

	i = find_first_bit(gpioe_rer, gpio_edge_gpio_num);
	while (i < gpio_edge_gpio_num) {
		pr_err("error: gpio edge: unexpected detect: %d\n", i);
		mfp_config_edge(i, MFP_LPM_EDGE_NONE);
		i = find_next_bit(gpioe_rer, gpio_edge_gpio_num, i + 1);
	}

	gpio_edge_enabled = 0;
	spin_unlock_irqrestore(&gpio_edge_lock, flags);
}

void mmp_gpio_edge_init(void __iomem *base, int mfp_num, int gpio_num)
{
	INIT_LIST_HEAD(&gpio_edge_list);
	gpio_edge_base = base;
	gpio_edge_mfp_num = mfp_num;
	gpio_edge_gpio_num = gpio_num;
}
