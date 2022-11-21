/*
 * linux/arch/arm/mach-mmp/time.c
 *
 *   Support for clocksource and clockevents
 *
 * Copyright (C) 2008 Marvell International Ltd.
 * All rights reserved.
 *
 *   2008-04-11: Jason Chagas <Jason.chagas@marvell.com>
 *   2008-10-08: Bin Yang <bin.yang@marvell.com>
 *
 * The timers module actually includes three timers, each timer with up to
 * three match comparators. Timer #0 is used here in free-running mode as
 * the clock source, and match comparator #1 used as clock event device.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/clockchips.h>

#include <linux/io.h>
#include <linux/irq.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>

#include <asm/sched_clock.h>
#include <asm/delay.h>
#include <mach/addr-map.h>
#include <mach/regs-timers.h>
#include <mach/regs-apbc.h>
#include <mach/irqs.h>
#include <mach/cputype.h>
#include <asm/mach/time.h>

#include "clock.h"

#ifdef CONFIG_PXA_32KTIMER
#define CLOCK_TICK_RATE_32KHZ	32768
#endif

#ifdef CONFIG_ARCH_PROVIDES_UDELAY
#define TIMER2_EN	(0x1 << 2)
#else
#define TIMER2_EN	0x0
#endif

#define TIMERS_VIRT_BASE	TIMERS1_VIRT_BASE

#define MAX_DELTA		(0xfffffffe)
#define MIN_DELTA		(16)

static void __iomem *mmp_timer_base = TIMERS_VIRT_BASE;

/*
 * FIXME: the timer needs some delay to stablize the counter capture
 */
static inline uint32_t timer_read(void)
{
	u32 val = 0;

#ifdef CONFIG_PXA_32KTIMER
	u32 val2 = 0;
	do {
		val = __raw_readl(mmp_timer_base + TMR_CR(1));
		val2 = __raw_readl(mmp_timer_base + TMR_CR(1));
	} while (val2 != val);
#else /* CONFIG_PXA_32KTIMER */
	u32 delay = 3;

	__raw_writel(1, mmp_timer_base + TMR_CVWR(1));

	while (delay--)
		val = __raw_readl(mmp_timer_base + TMR_CVWR(1));
#endif /* CONFIG_PXA_32KTIMER */
	return val;
}

#ifdef CONFIG_ARCH_PROVIDES_UDELAY
#define UDELAY_MAX_LOOP_STEP_US		20
#define UDELAY_MIN_CPU_FREQ_MHZ		156
static inline u32 timer_read_udelay_tick(void)
{
	u32 val;
	u32 delay = 3;

	__raw_writel(1, mmp_timer_base + TMR_CVWR(2));

	while (delay--)
		val = __raw_readl(mmp_timer_base + TMR_CVWR(2));

	return val;
}

void __udelay(unsigned long usecs)
{
	u32 now, begin, end;
	u32 ticks, loops;

	ticks = usecs * CLOCK_TICK_RATE / 1000000;
	if (usecs < UDELAY_MAX_LOOP_STEP_US)
		loops = usecs * UDELAY_MIN_CPU_FREQ_MHZ;
	else
		loops = UDELAY_MAX_LOOP_STEP_US * UDELAY_MIN_CPU_FREQ_MHZ;

	begin = timer_read_udelay_tick();
	end = begin + ticks;

	do {
		__delay(loops);
		now = timer_read_udelay_tick();
	} while ((now - begin) < ticks);
}

void __const_udelay(unsigned long usecs)
{
	return __udelay(usecs);
}
#endif /* CONFIG_ARCH_PROVIDES_UDELAY */

static u32 notrace mmp_read_sched_clock(void)
{
	return timer_read();
}

static irqreturn_t timer_interrupt(int irq, void *dev_id)
{
	struct clock_event_device *c = dev_id;

	/*
	 * Clear pending interrupt status.
	 */
	__raw_writel(0x01, mmp_timer_base + TMR_ICR(0));

	/*
	 * Disable timer 0.
	 */
	__raw_writel(0x02 | TIMER2_EN, mmp_timer_base + TMR_CER);

	c->event_handler(c);

	return IRQ_HANDLED;
}

static int timer_set_next_event(unsigned long delta,
				struct clock_event_device *dev)
{
	unsigned long flags;

	local_irq_save(flags);

	/*
	 * Disable timer 0.
	 */
	__raw_writel(0x02 | TIMER2_EN, mmp_timer_base + TMR_CER);

#ifdef CONFIG_PXA_32KTIMER
	__raw_writel(TMR_CCR_CS_0(0) | TMR_CCR_CS_1(1),
		mmp_timer_base + TMR_CCR);
#endif
	/*
	 * Clear and enable timer match 0 interrupt.
	 */
	__raw_writel(0x01, mmp_timer_base + TMR_ICR(0));
	__raw_writel(0x01, mmp_timer_base + TMR_IER(0));

	/*
	 * Setup new clockevent timer value.
	 */
	__raw_writel(delta - 1, mmp_timer_base + TMR_TN_MM(0, 0));

#ifdef CONFIG_PXA_32KTIMER
	__raw_writel(TMR_CCR_CS_0(1) | TMR_CCR_CS_1(1),
		mmp_timer_base + TMR_CCR);
#endif
	/*
	 * Enable timer 0.
	 */
	__raw_writel(0x03 | TIMER2_EN, mmp_timer_base + TMR_CER);

	local_irq_restore(flags);

	return 0;
}

static void timer_set_mode(enum clock_event_mode mode,
			   struct clock_event_device *dev)
{
	unsigned long flags;

	local_irq_save(flags);
	switch (mode) {
	case CLOCK_EVT_MODE_ONESHOT:
	case CLOCK_EVT_MODE_UNUSED:
	case CLOCK_EVT_MODE_SHUTDOWN:
		/* disable the matching interrupt */
		__raw_writel(0x00, mmp_timer_base + TMR_IER(0));
		break;
	case CLOCK_EVT_MODE_RESUME:
	case CLOCK_EVT_MODE_PERIODIC:
		break;
	}
	local_irq_restore(flags);
}

static struct clock_event_device ckevt = {
	.name		= "clockevent",
	.features	= CLOCK_EVT_FEAT_ONESHOT,
	.rating		= 200,
	.set_next_event	= timer_set_next_event,
	.set_mode	= timer_set_mode,
};

static cycle_t clksrc_read(struct clocksource *cs)
{
	return timer_read();
}

static struct clocksource cksrc = {
	.name		= "clocksource",
	.rating		= 200,
	.read		= clksrc_read,
	.mask		= CLOCKSOURCE_MASK(32),
	.flags		= CLOCK_SOURCE_IS_CONTINUOUS,
};

static void __init timer_config(void)
{
	uint32_t ccr;

	__raw_writel(0x0, mmp_timer_base + TMR_CER); /* disable */

#ifdef CONFIG_PXA_32KTIMER
	ccr = TMR_CCR_CS_0(1) | TMR_CCR_CS_1(1);
#else
	ccr = TMR_CCR_CS_0(0) | TMR_CCR_CS_1(0);
#endif
	__raw_writel(ccr, mmp_timer_base + TMR_CCR);

	/* set timer 0 to periodic mode, and timer 1/2 to free-running mode */
	__raw_writel(0x2 | TIMER2_EN, mmp_timer_base + TMR_CMR);

	__raw_writel(0x1, mmp_timer_base + TMR_PLCR(0)); /* periodic */
	__raw_writel(0x7, mmp_timer_base + TMR_ICR(0));  /* clear status */
	__raw_writel(0x0, mmp_timer_base + TMR_IER(0));

	__raw_writel(0x0, mmp_timer_base + TMR_PLCR(1)); /* free-running */
	__raw_writel(0x7, mmp_timer_base + TMR_ICR(1));  /* clear status */
	__raw_writel(0x0, mmp_timer_base + TMR_IER(1));

#ifdef CONFIG_ARCH_PROVIDES_UDELAY
	__raw_writel(0x0, mmp_timer_base + TMR_PLCR(2)); /* free-running */
	__raw_writel(0x7, mmp_timer_base + TMR_ICR(2));  /* clear status */
	__raw_writel(0x0, mmp_timer_base + TMR_IER(2));
#endif
	/* enable timer 1/2 counter */
	__raw_writel(0x2 | TIMER2_EN, mmp_timer_base + TMR_CER);
}

static struct irqaction timer_irq = {
	.name		= "timer",
	.flags		= IRQF_DISABLED | IRQF_TIMER | IRQF_IRQPOLL,
	.handler	= timer_interrupt,
	.dev_id		= &ckevt,
};

void __init timer_init(int irq)
{
	timer_config();

#ifdef CONFIG_PXA_32KTIMER
	setup_sched_clock(mmp_read_sched_clock, 32, CLOCK_TICK_RATE_32KHZ);
	clockevents_calc_mult_shift(&ckevt, CLOCK_TICK_RATE_32KHZ, 4);
#else
	setup_sched_clock(mmp_read_sched_clock, 32, CLOCK_TICK_RATE);
	clockevents_calc_mult_shift(&ckevt, CLOCK_TICK_RATE, 4);
#endif
	ckevt.max_delta_ns = clockevent_delta2ns(MAX_DELTA, &ckevt);
	ckevt.min_delta_ns = clockevent_delta2ns(MIN_DELTA, &ckevt);
	ckevt.cpumask = cpumask_of(0);
	ckevt.irq = irq;

	setup_irq(irq, &timer_irq);
#ifdef CONFIG_PXA_32KTIMER
	clocksource_register_hz(&cksrc, CLOCK_TICK_RATE_32KHZ);
#else
	clocksource_register_hz(&cksrc, CLOCK_TICK_RATE);
#endif
	clockevents_register_device(&ckevt);
}

#ifdef CONFIG_OF
static struct of_device_id mmp_timer_dt_ids[] = {
	{ .compatible = "mrvl,mmp-timer", },
	{}
};

void __init mmp_dt_init_timer(void)
{
	struct device_node *np;
	int irq, ret;

	np = of_find_matching_node(NULL, mmp_timer_dt_ids);
	if (!np) {
		ret = -ENODEV;
		goto out;
	}

	irq = irq_of_parse_and_map(np, 0);
	if (!irq) {
		ret = -EINVAL;
		goto out;
	}
	mmp_timer_base = of_iomap(np, 0);
	if (!mmp_timer_base) {
		ret = -ENOMEM;
		goto out;
	}
	timer_init(irq);
	return;
out:
	pr_err("Failed to get timer from device tree with error:%d\n", ret);
}
#endif
