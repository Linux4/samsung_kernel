/*
 * linux/arch/arm/mach-mmp/time-apb.c
 *
 *   Support for clocksource and clockevents
 *
 * Copyright (C) 2013 Marvell International Ltd.
 * All rights reserved.
 *
 *   2013-02-19: Leo Song <liangs@marvell.com>
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

#include <asm/sched_clock.h>
#include <asm/delay.h>
#include <mach/addr-map.h>
#include <mach/regs-timers.h>
#include <mach/regs-apbc.h>
#include <mach/irqs.h>
#include <mach/cputype.h>
#include <asm/mach/time.h>
#include <asm/localtimer.h>
#include <mach/smp.h>

#include "clock.h"

#define CLOCK_TICK_RATE_32KHZ	32768

#define MAX_DELTA		(0xfffffffe)
#define MIN_DELTA		(16)

static void __iomem *group0_base = TIMERS1_VIRT_BASE;
static void __iomem *group1_base = TIMERS2_VIRT_BASE;
static void __iomem *group2_base = TIMERS3_VIRT_BASE;

/* FIXME: the timer needs some delay to stablize the counter capture */
static inline uint32_t read_clksrc_tick(void)
{
	u32 val1, val2;

	do {
		val1 = __raw_readl(group1_base + TMR_CR(2));
		val2 = __raw_readl(group1_base + TMR_CR(2));
	} while (val2 != val1);

	return val2;
}

#ifdef CONFIG_ARCH_PROVIDES_UDELAY
#define UDELAY_MAX_LOOP_STEP_US		20
#define UDELAY_MIN_CPU_FREQ_MHZ		156
static inline u32 read_udelay_tick(void)
{
	u32 val;
	u32 delay = 3;

	__raw_writel(1, group0_base + TMR_CVWR(2));

	while (delay--)
		val = __raw_readl(group0_base + TMR_CVWR(2));

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

	begin = read_udelay_tick();
	end = begin + ticks;

	do {
		__delay(loops);
		now = read_udelay_tick();
	} while ((now - begin) < ticks);
}

void __const_udelay(unsigned long usecs)
{
	return __udelay(usecs);
}
#endif /* CONFIG_ARCH_PROVIDES_UDELAY */

static u32 notrace mmp_read_sched_clock(void)
{
	return read_clksrc_tick();
}

/* must protect with local irq disabled or spin_lock_irqsave held! */
static void disable_timer(void __iomem *base, u32 timern)
{
	u32 tmp;
	u32 delay = 3;

	/* Disable timern */
	tmp = __raw_readl(base + TMR_CER);
	__raw_writel(tmp & (~(0x1 << timern)), base + TMR_CER);

	/* switch to fast clock for quick operation */
	tmp = __raw_readl(base + TMR_CCR);
	__raw_writel(tmp & (~(0x3 << timern * 2)), base + TMR_CCR);

	/* disable the matching interrupt */
	__raw_writel(0, base + TMR_IER(timern));

	/* Clear pending interrupt status */
	while (delay--)
		__raw_writel(0x1, base + TMR_ICR(timern));

	/* switch back to 32K */
	tmp = __raw_readl(base + TMR_CCR);
	__raw_writel((tmp & (~(0x3 << timern * 2))) | (0x1 << (timern * 2)),
		     base + TMR_CCR);
}

/* must protect with local irq disabled or spin_lock_irqsave held! */
static void reprogram_timer(void __iomem *base, u32 timern,
			     unsigned long delta)
{
	u32 tmp;
	u32 delay = 3;

	/* Disable timern */
	tmp = __raw_readl(base + TMR_CER);
	__raw_writel(tmp & (~(0x1 << timern)), base + TMR_CER);

	/* switch to fast clock for quick operation */
	tmp = __raw_readl(base + TMR_CCR);
	__raw_writel(tmp & (~(0x3 << timern * 2)), base + TMR_CCR);

	/* Clear pending interrupt status */
	while (delay--)
		__raw_writel(0x1, base + TMR_ICR(timern));

	/* enable the matching interrupt */
	__raw_writel(0x1, base + TMR_IER(timern));

	/* Setup new clockevent timer value */
	__raw_writel(delta - 1, base + TMR_TN_MM(timern, 0));

	/* switch back to 32K */
	tmp = __raw_readl(base + TMR_CCR);
	__raw_writel((tmp & (~(0x3 << timern * 2))) | (0x1 << (timern * 2)),
		     base + TMR_CCR);

	/* Enable timer */
	tmp = __raw_readl(base + TMR_CER);
	__raw_writel(tmp | (0x1 << timern), base + TMR_CER);
}

static irqreturn_t timer_interrupt(int irq, void *dev_id)
{
	struct clock_event_device *c = dev_id;
	unsigned long flags;

	local_irq_save(flags);
	disable_timer(group2_base, 0);
	local_irq_restore(flags);

	c->event_handler(c);

	return IRQ_HANDLED;
}

static int timer_set_next_event(unsigned long delta,
				struct clock_event_device *dev)
{
	unsigned long flags;

	local_irq_save(flags);
	reprogram_timer(group2_base, 0, delta);
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
		disable_timer(group2_base, 0);
		break;
	case CLOCK_EVT_MODE_RESUME:
	case CLOCK_EVT_MODE_PERIODIC:
		break;
	}

	local_irq_restore(flags);
}

static struct clock_event_device ckevt = {
	.name		= "tmr2-clkevt0",
	.features	= CLOCK_EVT_FEAT_ONESHOT,
	.rating		= 200,
	.set_next_event	= timer_set_next_event,
	.set_mode	= timer_set_mode,
};

static cycle_t clksrc_read(struct clocksource *cs)
{
	return read_clksrc_tick();
}

static struct clocksource cksrc = {
	.name		= "tmr1-clksrc2",
	.rating		= 200,
	.read		= clksrc_read,
	.mask		= CLOCKSOURCE_MASK(32),
	.flags		= CLOCK_SOURCE_IS_CONTINUOUS,
};

static struct irqaction timer_irq = {
	.name		= "tmr2-clkevt0-irq",
	.flags		= IRQF_DISABLED | IRQF_TIMER | IRQF_IRQPOLL,
	.handler	= timer_interrupt,
	.dev_id		= &ckevt,
};

/*
 * tmr_group0_config:
 * group0_timer0: cpu0 local_timer, free running, 32K
 * group0_timer1: cpu1 local_timer, free running, 32K
 * group0_timer2: udelay timer, free running, 3.25M
 */
static void __init tmr_group0_config(void)
{
	/* Select the configurable timer clock rate to be 3.25MHz */
	__raw_writel(APBC_APBCLK | APBC_RST, APBC_PXA988_TIMERS);
	__raw_writel(APBC_APBCLK | APBC_FNCLK | APBC_FNCLKSEL(3),
		     APBC_PXA988_TIMERS);

	__raw_writel(0x0, group0_base + TMR_CER); /* disable */

	/* timer0:32K, timer1:32K, timer2:fast clock (3.25M) */
	__raw_writel(TMR_CCR_CS_0(1) | TMR_CCR_CS_1(1) | TMR_CCR_CS_2(0),
		     group0_base + TMR_CCR);

	/* set timer0/1/2 to free-running mode */
	__raw_writel(0x7, group0_base + TMR_CMR);

	__raw_writel(0x0, group0_base + TMR_PLCR(0)); /* free-running */
	__raw_writel(0x7, group0_base + TMR_ICR(0));  /* clear status */
	__raw_writel(0x0, group0_base + TMR_IER(0));  /* disable irq */

	__raw_writel(0x0, group0_base + TMR_PLCR(1)); /* free-running */
	__raw_writel(0x7, group0_base + TMR_ICR(1));  /* clear status */
	__raw_writel(0x0, group0_base + TMR_IER(1));  /* disable irq */

#ifdef CONFIG_ARCH_PROVIDES_UDELAY
	__raw_writel(0x0, group0_base + TMR_PLCR(2)); /* free-running */
	__raw_writel(0x7, group0_base + TMR_ICR(2));  /* clear status */
	__raw_writel(0x0, group0_base + TMR_IER(2));  /* disable irq */

	/* enable timer2 */
	__raw_writel(0x4, group0_base + TMR_CER);
#endif
}

/*
 * tmr_group1_config:
 * group1_timer0: cpu0 local_timer, free running, 32K
 * group1_timer1: cpu1 local_timer, free running, 32K
 * group1_timer2: clock source, free running, 32K
 */
static void __init tmr_group1_config(void)
{
	/* Select the configurable timer clock rate to be 3.25MHz */
	__raw_writel(APBC_APBCLK | APBC_RST, APBC_PXA988_TIMERS1);
	__raw_writel(APBC_APBCLK | APBC_FNCLK | APBC_FNCLKSEL(3),
		     APBC_PXA988_TIMERS1);

	__raw_writel(0x0, group1_base + TMR_CER); /* disable */

	/* timer0:32K, timer1:32K, timer2:32K */
	__raw_writel(TMR_CCR_CS_0(1) | TMR_CCR_CS_1(1) | TMR_CCR_CS_2(2),
		     group1_base + TMR_CCR);

	/* set timer0/1/2 to free-running mode */
	__raw_writel(0x7, group1_base + TMR_CMR);

	__raw_writel(0x0, group1_base + TMR_PLCR(0)); /* free-running */
	__raw_writel(0x7, group1_base + TMR_ICR(0));  /* clear status */
	__raw_writel(0x0, group1_base + TMR_IER(0));  /* disable irq */

	__raw_writel(0x0, group1_base + TMR_PLCR(1)); /* free-running */
	__raw_writel(0x7, group1_base + TMR_ICR(1));  /* clear status */
	__raw_writel(0x0, group1_base + TMR_IER(1));  /* disable irq */

	__raw_writel(0x0, group1_base + TMR_PLCR(2)); /* free-running */
	__raw_writel(0x7, group1_base + TMR_ICR(2));  /* clear status */
	__raw_writel(0x0, group1_base + TMR_IER(2));  /* disable irq */

	/* enable timer2 */
	__raw_writel(0x4, group1_base + TMR_CER);
}

/*
 * tmr_group2_config:
 * group2_timer0: global clockevent when kernel boot up, free running, 32K
 * group2_timer1/2: reserved for trust zone, settings unchanged
 * NOTICE:
 * make sure trust zone won't change group2 config at the same time!
 */
static void __init tmr_group2_config(void)
{
	u32 tmp;

	/* trust zone will init APBC_PXA1088_TIMERS2 prior to kernel */
#ifndef CONFIG_TZ_HYPERVISOR
	/* Select the configurable timer clock rate to be 3.25MHz */
	__raw_writel(APBC_APBCLK | APBC_RST, APBC_PXA1088_TIMERS2);
	__raw_writel(APBC_APBCLK | APBC_FNCLK | APBC_FNCLKSEL(3),
		     APBC_PXA1088_TIMERS2);
#endif /* CONFIG_TZ_HYPERVISOR */

	/* only disable timer0; timer1/2: unchanged */
	tmp = __raw_readl(group2_base + TMR_CER);
	__raw_writel(tmp & (~0x1), group2_base + TMR_CER);

	/* timer0: 32K; timer1/2: unchanged */
	tmp = __raw_readl(group2_base + TMR_CCR);
	__raw_writel((tmp & (~0x3)) | TMR_CCR_CS_0(1), group2_base + TMR_CCR);

	/* timer0: free-running mode; timer1/2: unchanged */
	tmp = __raw_readl(group2_base + TMR_CMR);
	__raw_writel(tmp | 0x1, group2_base + TMR_CMR);

	__raw_writel(0x0, group2_base + TMR_PLCR(0)); /* free-running */
	__raw_writel(0x7, group2_base + TMR_ICR(0));  /* clear status */
	__raw_writel(0x0, group2_base + TMR_IER(0));  /* disable irq */

}

struct percpu_timer_irq_map {
	void __iomem *base;
	int irq;
	struct irqaction irq_act;
};

/* CPU0/1 and CPU2/3 use different timer groups, so use separate spinlock */
static spinlock_t soc_tmr_lock[2] = {
	__SPIN_LOCK_UNLOCKED(soc_tmr_lock),  /* for group0 */
	__SPIN_LOCK_UNLOCKED(soc_tmr_lock)   /* for group1 */
};

static irqreturn_t grp0_tmr0_irq(int irq, void *dev_id)
{
	struct clock_event_device *c = dev_id;
	u32 cpuid = hard_smp_processor_id();
	unsigned long flags;

	WARN_ON(cpuid != 0);

	spin_lock_irqsave(&soc_tmr_lock[0], flags);
	disable_timer(group0_base, 0);
	spin_unlock_irqrestore(&soc_tmr_lock[0], flags);

	c->event_handler(c);

	return IRQ_HANDLED;
}

static irqreturn_t grp0_tmr1_irq(int irq, void *dev_id)
{
	struct clock_event_device *c = dev_id;
	u32 cpuid = hard_smp_processor_id();
	unsigned long flags;

	WARN_ON(cpuid != 1);

	spin_lock_irqsave(&soc_tmr_lock[0], flags);
	disable_timer(group0_base, 1);
	spin_unlock_irqrestore(&soc_tmr_lock[0], flags);

	c->event_handler(c);

	return IRQ_HANDLED;
}

static irqreturn_t grp1_tmr0_irq(int irq, void *dev_id)
{
	struct clock_event_device *c = dev_id;
	u32 cpuid = hard_smp_processor_id();
	unsigned long flags;

	WARN_ON(cpuid != 2);

	spin_lock_irqsave(&soc_tmr_lock[1], flags);
	disable_timer(group1_base, 0);
	spin_unlock_irqrestore(&soc_tmr_lock[1], flags);

	c->event_handler(c);

	return IRQ_HANDLED;
}

static irqreturn_t grp1_tmr1_irq(int irq, void *dev_id)
{
	struct clock_event_device *c = dev_id;
	u32 cpuid = hard_smp_processor_id();
	unsigned long flags;

	WARN_ON(cpuid != 3);

	spin_lock_irqsave(&soc_tmr_lock[1], flags);
	disable_timer(group1_base, 1);
	spin_unlock_irqrestore(&soc_tmr_lock[1], flags);

	c->event_handler(c);

	return IRQ_HANDLED;
}

static struct percpu_timer_irq_map irq_map[4] = {
	{	TIMERS1_VIRT_BASE, IRQ_PXA988_AP_TIMER1   ,
		{
			.name	 = "cpu0_timer",
			.flags	 = IRQF_DISABLED | IRQF_TIMER
				  | IRQF_IRQPOLL | IRQF_PERCPU,
			.handler = grp0_tmr0_irq,
		},
	},
	{	TIMERS1_VIRT_BASE, IRQ_PXA988_AP_TIMER2_3,
		{
			.name	 = "cpu1_timer",
			.flags	 = IRQF_DISABLED | IRQF_TIMER
				  | IRQF_IRQPOLL | IRQF_PERCPU,
			.handler = grp0_tmr1_irq,
		},
	},
	{	TIMERS2_VIRT_BASE, IRQ_PXA988_AP2_TIMER1  ,
		{
			.name	 = "cpu2_timer",
			.flags	 = IRQF_DISABLED | IRQF_TIMER
				  | IRQF_IRQPOLL | IRQF_PERCPU,
			.handler = grp1_tmr0_irq,
		},
	},
	{	TIMERS2_VIRT_BASE, IRQ_PXA988_AP2_TIMER2_3,
		{
			.name	 = "cpu3_timer",
			.flags	 = IRQF_DISABLED | IRQF_TIMER
				  | IRQF_IRQPOLL | IRQF_PERCPU,
			.handler = grp1_tmr1_irq,
		},
	},
};


static int percpu_timer_set_next_event(unsigned long delta,
				struct clock_event_device *dev)
{
	unsigned long flags;
	u32 cpuid = hard_smp_processor_id();
	u32 timern = cpuid & 0x1;
	void __iomem *base = irq_map[cpuid].base;

	/* if cpu is offline, should not set next event on local timer */
	BUG_ON(!cpu_online(cpuid));

	spin_lock_irqsave(&soc_tmr_lock[cpuid>>1], flags);
	reprogram_timer(base, timern, delta);
	spin_unlock_irqrestore(&soc_tmr_lock[cpuid>>1], flags);

	return 0;
}


static void percpu_timer_set_mode(enum clock_event_mode mode,
			   struct clock_event_device *dev)
{
	unsigned long flags;
	u32 cpuid = hard_smp_processor_id();
	u32 timern = cpuid & 0x1;
	void __iomem *base = irq_map[cpuid].base;

	spin_lock_irqsave(&soc_tmr_lock[cpuid>>1], flags);

	switch (mode) {
	case CLOCK_EVT_MODE_ONESHOT:
	case CLOCK_EVT_MODE_UNUSED:
	case CLOCK_EVT_MODE_SHUTDOWN:
		disable_timer(base, timern);
		break;
	case CLOCK_EVT_MODE_RESUME:
	case CLOCK_EVT_MODE_PERIODIC:
		break;
	}

	spin_unlock_irqrestore(&soc_tmr_lock[cpuid>>1], flags);
}

static int __cpuinit mmp_percpu_timer_setup(struct clock_event_device *clk)
{
	u32 cpuid = hard_smp_processor_id();

	clk->features = CLOCK_EVT_FEAT_ONESHOT;
	clk->name = "apb_percpu_timer";
	clk->rating = 300;

	clk->irq = irq_map[cpuid].irq;
	clk->cpumask = cpumask_of(cpuid);
	clk->set_mode = percpu_timer_set_mode;
	clk->set_next_event = percpu_timer_set_next_event;

	clk->set_mode(CLOCK_EVT_MODE_SHUTDOWN, NULL);

	irq_map[cpuid].irq_act.dev_id = clk;
	irq_set_affinity(clk->irq, clk->cpumask);

	clockevents_config_and_register(clk, CLOCK_TICK_RATE_32KHZ,
					MIN_DELTA, MAX_DELTA);

	enable_irq(clk->irq);

	/* the broadcast clockevent is no longer needed */
	if (cpuid == 0) {
		remove_irq(ckevt.irq, &timer_irq);

#ifndef CONFIG_TZ_HYPERVISOR
		/* reset APB and functional domain */
		__raw_writel(APBC_RST, APBC_PXA1088_TIMERS2);
#endif /* CONFIG_TZ_HYPERVISOR */
	}

	return 0;
}

static void __cpuinit mmp_percpu_timer_stop(struct clock_event_device *clk)
{
	clockevents_set_mode(clk, CLOCK_EVT_MODE_SHUTDOWN);
	disable_irq(clk->irq);
	return;
}

static struct local_timer_ops mmp_percpu_timer_ops __cpuinitdata = {
	.setup	= mmp_percpu_timer_setup,
	.stop	= mmp_percpu_timer_stop,
};

void __init apb_timer_init(void)
{
	int cpu;
	tmr_group0_config();
	tmr_group1_config();
	tmr_group2_config();

	setup_sched_clock(mmp_read_sched_clock, 32, CLOCK_TICK_RATE_32KHZ);

	clockevents_calc_mult_shift(&ckevt, CLOCK_TICK_RATE_32KHZ, 4);
	ckevt.max_delta_ns = clockevent_delta2ns(MAX_DELTA, &ckevt);
	ckevt.min_delta_ns = clockevent_delta2ns(MIN_DELTA, &ckevt);
	ckevt.cpumask = cpumask_of(0);
	ckevt.irq = IRQ_PXA1088_AP3_TIMER1;
	setup_irq(ckevt.irq, &timer_irq);

	clocksource_register_hz(&cksrc, CLOCK_TICK_RATE_32KHZ);
	clockevents_register_device(&ckevt);

	for (cpu = 0; cpu < num_possible_cpus(); cpu++) {
		setup_irq(irq_map[cpu].irq, &irq_map[cpu].irq_act);
		disable_irq(irq_map[cpu].irq);
	}

	local_timer_register(&mmp_percpu_timer_ops);
}
