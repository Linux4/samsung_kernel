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

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/clockchips.h>
#include <linux/spinlock.h>
#include <linux/bitops.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/sched.h>
#include <linux/clocksource.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>

#include <asm/sched_clock.h>
#include <asm/localtimer.h>
#include <asm/mach/time.h>

#include <mach/hardware.h>
#include <mach/sci.h>
#include <mach/sci_glb_regs.h>
#include <mach/irqs.h>
#include <asm/mach/time.h>
#include <mach/pm_debug.h>

static __iomem void *base_gptimer[4] = {
#ifndef CONFIG_OF
	(__iomem void *)SPRD_GPTIMER_BASE,
	(__iomem void *)SPRD_APTIMER0_BASE,
	(__iomem void *)SPRD_APTIMER1_BASE,
	(__iomem void *)SPRD_APTIMER2_BASE,
#else
	0,
#endif
};

static int irq_nr[4] = {
#ifndef CONFIG_OF
	IRQ_AONTMR0_INT,
	IRQ_APTMR0_INT,
	IRQ_APTMR2_INT,
	IRQ_APTMR4_INT,
#else
	0,
#endif
};
static struct clock_event_device *local_evt[4] = { 0 };

static int e_cpu = 0;
#define	TIMER_LOAD(ind, id)	(base_gptimer[ind] + 0x20 * (id) + 0x0000)
#define	TIMER_VALUE(ind, id)	(base_gptimer[ind] + 0x20 * (id) + 0x0004)
#define	TIMER_CTL(ind, id)	(base_gptimer[ind] + 0x20 * (id) + 0x0008)
#define	TIMER_INT(ind, id)	(base_gptimer[ind] + 0x20 * (id) + 0x000C)
#define	TIMER_CNT_RD(ind, id)	(base_gptimer[ind] + 0x20 * (id) + 0x0010)

#define	ONETIME_MODE	(0 << 6)
#define	PERIOD_MODE	(1 << 6)

#define	TIMER_DISABLE	(0 << 7)
#define	TIMER_ENABLE	(1 << 7)

#define	TIMER_INT_EN	(1 << 0)
#define	TIMER_INT_CLR	(1 << 3)
#define	TIMER_INT_BUSY	(1 << 4)
#define TIMER_NEW       (1 << 8)

/**
 * timer0 is used as clockevent,
 * timer1 of aptimer0 is used as broadcast timer,
 * timer2 is used as clocksource
 */
#define	EVENT_TIMER	0
#define	BC_TIMER	1
#define	SOURCE_TIMER	1

#define BC_CPU 1
#ifndef CONFIG_OF
static int BC_IRQ = IRQ_APTMR1_INT;
#else
static int BC_IRQ = 0;
#endif

#ifndef CONFIG_OF
static __iomem void *base_syscnt = (__iomem void *)SPRD_SYSCNT_BASE;
#else
static __iomem void *base_syscnt = (__iomem void *)NULL;
#endif
#define	SYSCNT_COUNT	(base_syscnt + 0x0004)
#define	SYSCNT_CTL	(base_syscnt + 0x0008)
#define	SYSCNT_SHADOW_CNT	(base_syscnt + 0x000C)

#ifdef CONFIG_SEC_DEBUG
void sec_debug_print_gptimer_reg(void)
{
	
	printk("%s: gptimer-clockevent\n",__func__); 
	printk("load reg: 0x%x\n",__raw_readl(TIMER_LOAD(0,0)));
	printk("value reg: 0x%x\n",__raw_readl(TIMER_VALUE(0,0)));
	printk("control reg: 0x%x\n",__raw_readl(TIMER_CTL(0,0)));
	printk("interrupt reg: 0x%x\n",__raw_readl(TIMER_INT(0,0)));
	printk("%s: gptimer-clocksource\n",__func__); 
	printk("load reg: 0x%x\n",__raw_readl(TIMER_LOAD(0,2)));
	printk("value reg: 0x%x\n",__raw_readl(TIMER_VALUE(0,2)));
	printk("control reg: 0x%x\n",__raw_readl(TIMER_CTL(0,2)));
	printk("interrupt reg: 0x%x\n",__raw_readl(TIMER_INT(0,2)));
}
#endif
static int sched_clock_source_freq;
static int gptimer_clock_source_freq;

static inline void __gptimer_ctl(int cpu, int timer_id, int enable, int mode)
{
	__raw_writel(enable | mode, TIMER_CTL(cpu, timer_id));
}

static int __gptimer_set_next_event(unsigned long cycles,
				    struct clock_event_device *c)
{
	int cpu = smp_processor_id();

	while (TIMER_INT_BUSY & __raw_readl(TIMER_INT(cpu, EVENT_TIMER))) ;
	__gptimer_ctl(cpu, EVENT_TIMER, TIMER_DISABLE, ONETIME_MODE);
	__raw_writel(cycles, TIMER_LOAD(cpu, EVENT_TIMER));
	__gptimer_ctl(cpu, EVENT_TIMER, TIMER_ENABLE, ONETIME_MODE);
	return 0;
}

static void __gptimer_set_mode(enum clock_event_mode mode,
			       struct clock_event_device *c)
{
	unsigned int saved;
	int cpu = smp_processor_id();

	switch (mode) {
	case CLOCK_EVT_MODE_PERIODIC:
		__gptimer_ctl(cpu, EVENT_TIMER, TIMER_DISABLE, PERIOD_MODE);
		__raw_writel(LATCH, TIMER_LOAD(cpu, EVENT_TIMER));
		__gptimer_ctl(cpu, EVENT_TIMER, TIMER_ENABLE, PERIOD_MODE);
		__raw_writel(TIMER_INT_EN, TIMER_INT(cpu, EVENT_TIMER));
		break;
	case CLOCK_EVT_MODE_ONESHOT:
		__raw_writel(LATCH, TIMER_LOAD(cpu, EVENT_TIMER));
		__gptimer_ctl(cpu, EVENT_TIMER, TIMER_ENABLE, ONETIME_MODE);
		__raw_writel(TIMER_INT_EN, TIMER_INT(cpu, EVENT_TIMER));
		break;
	case CLOCK_EVT_MODE_SHUTDOWN:
	case CLOCK_EVT_MODE_UNUSED:
		__raw_writel(TIMER_INT_CLR, TIMER_INT(cpu, EVENT_TIMER));
		saved = __raw_readl(TIMER_CTL(cpu, EVENT_TIMER)) & PERIOD_MODE;
		__gptimer_ctl(cpu, EVENT_TIMER, TIMER_DISABLE, saved);
		break;
	case CLOCK_EVT_MODE_RESUME:
		saved = __raw_readl(TIMER_CTL(cpu, EVENT_TIMER)) & PERIOD_MODE;
		__gptimer_ctl(cpu, EVENT_TIMER, TIMER_ENABLE, saved);
		break;
	}
}

static int __bctimer_set_next_event(unsigned long cycles,
				    struct clock_event_device *c)
{
	while (TIMER_INT_BUSY & __raw_readl(TIMER_INT(BC_CPU, BC_TIMER))) ;
	__gptimer_ctl(BC_CPU, BC_TIMER, TIMER_DISABLE, ONETIME_MODE);
	__raw_writel(cycles, TIMER_LOAD(BC_CPU, BC_TIMER));
	__gptimer_ctl(BC_CPU, BC_TIMER, TIMER_ENABLE, ONETIME_MODE);
	return 0;
}

static void __bctimer_set_mode(enum clock_event_mode mode,
			       struct clock_event_device *c)
{
	unsigned int saved;

	switch (mode) {
	case CLOCK_EVT_MODE_PERIODIC:
		__gptimer_ctl(BC_CPU, BC_TIMER, TIMER_DISABLE, PERIOD_MODE);
		__raw_writel(LATCH, TIMER_LOAD(BC_CPU, BC_TIMER));
		__gptimer_ctl(BC_CPU, BC_TIMER, TIMER_ENABLE, PERIOD_MODE);
		__raw_writel(TIMER_INT_EN, TIMER_INT(BC_CPU, BC_TIMER));
		break;
	case CLOCK_EVT_MODE_ONESHOT:
		__raw_writel(LATCH, TIMER_LOAD(BC_CPU, BC_TIMER));
		__gptimer_ctl(BC_CPU, BC_TIMER, TIMER_ENABLE, ONETIME_MODE);
		__raw_writel(TIMER_INT_EN, TIMER_INT(BC_CPU, BC_TIMER));
		break;
	case CLOCK_EVT_MODE_SHUTDOWN:
	case CLOCK_EVT_MODE_UNUSED:
		__raw_writel(TIMER_INT_CLR, TIMER_INT(BC_CPU, BC_TIMER));
		saved = __raw_readl(TIMER_CTL(BC_CPU, BC_TIMER)) & PERIOD_MODE;
		__gptimer_ctl(BC_CPU, BC_TIMER, TIMER_DISABLE, saved);
		break;
	case CLOCK_EVT_MODE_RESUME:
		saved = __raw_readl(TIMER_CTL(BC_CPU, BC_TIMER)) & PERIOD_MODE;
		__gptimer_ctl(BC_CPU, BC_TIMER, TIMER_ENABLE, saved);
		break;
	}
}

static struct clock_event_device bctimer_event = {
	.features = CLOCK_EVT_FEAT_ONESHOT,
	.shift = 32,
	.rating = 150,
	.set_next_event = __bctimer_set_next_event,
	.set_mode = __bctimer_set_mode,
};

static irqreturn_t __gptimer_interrupt(int irq, void *dev_id);
#ifdef CONFIG_LOCAL_TIMERS
#if !defined (CONFIG_HAVE_ARM_ARCH_TIMER)
static int __cpuinit sprd_local_timer_setup(struct clock_event_device *evt)
{
	int cpu = smp_processor_id();

	evt->irq = irq_nr[cpu];
	evt->name = "local_timer";
	evt->features = CLOCK_EVT_FEAT_ONESHOT;
	evt->rating = 200;
	evt->set_mode = __gptimer_set_mode;
	evt->set_next_event = __gptimer_set_next_event;
	evt->shift = 32;
	evt->mult = div_sc(32768, NSEC_PER_SEC, evt->shift);
	evt->max_delta_ns = clockevent_delta2ns(0xf0000000, evt);
	evt->min_delta_ns = clockevent_delta2ns(4, evt);

	local_evt[cpu] = evt;
	irq_set_affinity(evt->irq, cpumask_of(cpu));

	clockevents_register_device(evt);
	return 0;
}

static void sprd_local_timer_stop(struct clock_event_device *evt)
{

	evt->set_mode(CLOCK_EVT_MODE_UNUSED, evt);
}

static struct local_timer_ops sprd_local_timer_ops __cpuinitdata = {
	.setup = sprd_local_timer_setup,
	.stop = sprd_local_timer_stop,
};
#endif
#endif /* CONFIG_LOCAL_TIMERS */

static irqreturn_t __gptimer_interrupt(int irq, void *dev_id)
{
	unsigned int value;
	int cpu = smp_processor_id();
	struct clock_event_device **evt = dev_id;

	value = __raw_readl(TIMER_INT(cpu, EVENT_TIMER));
	value |= TIMER_INT_CLR;
	__raw_writel(value, TIMER_INT(cpu, EVENT_TIMER));

	if (evt[cpu]->event_handler)
		evt[cpu]->event_handler(evt[cpu]);

	return IRQ_HANDLED;
}

static irqreturn_t __bctimer_interrupt(int irq, void *dev_id)
{
	unsigned int value;
	struct clock_event_device *evt = dev_id;

	value = __raw_readl(TIMER_INT(BC_CPU, BC_TIMER));
	value |= TIMER_INT_CLR;
	__raw_writel(value, TIMER_INT(BC_CPU, BC_TIMER));

	if (evt->event_handler)
		evt->event_handler(evt);

	return IRQ_HANDLED;
}

static struct irqaction bctimer_irq = {
	.name = "bctimer",
	.flags = IRQF_DISABLED | IRQF_TIMER | IRQF_IRQPOLL,
	.handler = __bctimer_interrupt,
	.dev_id = &bctimer_event,
};

static void sprd_gptimer_clockevent_init(unsigned int irq, const char *name,
					 unsigned long hz)
{
	struct clock_event_device *evt = &bctimer_event;
	int ret = 0;

	__raw_writel(TIMER_DISABLE, TIMER_CTL(BC_CPU, BC_TIMER));
	__raw_writel(TIMER_INT_CLR, TIMER_INT(BC_CPU, BC_TIMER));

	evt->name = name;
	evt->irq = irq;
	evt->mult = div_sc(hz, NSEC_PER_SEC, evt->shift);
	evt->max_delta_ns = clockevent_delta2ns(ULONG_MAX, evt);
	evt->min_delta_ns = clockevent_delta2ns(2, evt);
	evt->cpumask = cpu_all_mask;

	ret = setup_irq(irq, &bctimer_irq);
	if (ret)
		BUG_ON(1);
	clockevents_register_device(evt);
}

/* ****************************************************************** */
void __gptimer_clocksource_resume(struct clocksource *cs)
{
	__gptimer_ctl(e_cpu, SOURCE_TIMER, TIMER_ENABLE|TIMER_NEW, PERIOD_MODE);
	pr_debug("%s: timer_val=0x%x\n", __FUNCTION__,
		 __raw_readl(TIMER_CNT_RD(e_cpu, SOURCE_TIMER)));
}

void __gptimer_clocksource_suspend(struct clocksource *cs)
{
	__gptimer_ctl(e_cpu, SOURCE_TIMER, TIMER_DISABLE, PERIOD_MODE);
	pr_debug("%s: timer_val=0x%x\n", __FUNCTION__,
		 __raw_readl(TIMER_CNT_RD(e_cpu, SOURCE_TIMER)));
}

static DEFINE_SPINLOCK(clocksource_read_lock);
cycle_t __gptimer_clocksource_read(struct clocksource *cs)
{
	return ~readl_relaxed(TIMER_CNT_RD(e_cpu, SOURCE_TIMER));
}

struct clocksource clocksource_sprd = {
	.name = "aon_timer_timer1",
	.rating = 300,
	.read = __gptimer_clocksource_read,
	.mask = CLOCKSOURCE_MASK(32),
	.flags = CLOCK_SOURCE_IS_CONTINUOUS,
	.resume = __gptimer_clocksource_resume,
	.suspend = __gptimer_clocksource_suspend,
};

static void __gptimer_clocksource_init(void)
{
	/* disalbe irq since it's just a read source */
	__raw_writel(0, TIMER_INT(e_cpu, SOURCE_TIMER));
	__gptimer_ctl(e_cpu, SOURCE_TIMER, TIMER_DISABLE|TIMER_NEW, PERIOD_MODE);
	__raw_writel(ULONG_MAX, TIMER_LOAD(e_cpu, SOURCE_TIMER));
	__gptimer_ctl(e_cpu, SOURCE_TIMER, TIMER_NEW, PERIOD_MODE);
	__gptimer_ctl(e_cpu, SOURCE_TIMER, TIMER_ENABLE|TIMER_NEW, PERIOD_MODE);
	if (clocksource_register_hz
	    (&clocksource_sprd, gptimer_clock_source_freq))
		printk("%s: can't register clocksource\n",
		       clocksource_sprd.name);
}

static void __syscnt_clocksource_init(const char *name, unsigned long hz)
{
	/* disable irq for syscnt */
	__raw_writel(0, SYSCNT_CTL);

	clocksource_mmio_init(SYSCNT_SHADOW_CNT, name,
			      hz, 200, 32, clocksource_mmio_readw_up);
}

/* ****************************************************************** */
static u32 notrace __update_sched_clock(void)
{
	return ~(readl_relaxed(TIMER_CNT_RD(0, SOURCE_TIMER)));
}

static void __init __sched_clock_init(unsigned long rate)
{
	setup_sched_clock(__update_sched_clock, 32, rate);
}

void __init sci_enable_timer_early(void)
{
	/* enable timer & syscnt in global regs */
	int i = 0, j = 0;
	u32 val = -1;
	sci_glb_set(REG_AON_APB_APB_EB0,
		    BIT_AON_TMR_EB | BIT_AP_SYST_EB | BIT_AP_TMR0_EB);
#if defined CONFIG_LOCAL_TIMERS && !defined CONFIG_HAVE_ARM_ARCH_TIMER
	sci_glb_set(REG_AON_APB_APB_EB1, BIT_AP_TMR2_EB | BIT_AP_TMR1_EB);
	for (i = 0; i < 4; i++) {
#else
	sci_glb_clr(REG_AON_APB_APB_EB1, BIT_AP_TMR2_EB | BIT_AP_TMR1_EB);
	for (i = 0; i < 2; i++) {
#endif
		for (j = 0; j < 3; j++) {
			__gptimer_ctl(i, j, TIMER_DISABLE, 0);
			__raw_writel(TIMER_INT_CLR, TIMER_INT(i, j));
		}
	}

#if defined(CONFIG_ARCH_SCX30G)	/*timer1 fixed 32768 clk */
	sched_clock_source_freq = 32768;
#else /*timer2 clk source is from apb clk */
	val = sci_glb_read(REG_AON_CLK_AON_APB_CFG, -1) & 0x3;
	if (val == 0x1)
		sched_clock_source_freq = 76800000;
	else if (val == 0x2)
		sched_clock_source_freq = 96000000;
	else if (val == 0x3)
		sched_clock_source_freq = 128000000;
	else
		sched_clock_source_freq = 26000000;	/*default setting */
#endif

	gptimer_clock_source_freq = sched_clock_source_freq;
#if !defined (CONFIG_HAVE_ARM_ARCH_TIMER)
	__sched_clock_init(sched_clock_source_freq);
#endif
}

static struct timespec persistent_ts;
static u64 persistent_ms, last_persistent_ms;
static void sprd_read_persistent_clock(struct timespec *ts)
{
	u64 delta;
	struct timespec *tsp = &persistent_ts;

	last_persistent_ms = persistent_ms;
	persistent_ms = get_sys_cnt();
	delta = persistent_ms - last_persistent_ms;

	timespec_add_ns(tsp, delta * NSEC_PER_MSEC);
	*ts = *tsp;
}

void __init sci_timer_init(void)
{
#ifdef CONFIG_LOCAL_TIMERS
#if !defined (CONFIG_HAVE_ARM_ARCH_TIMER)
	int i = 0, ret = 0;
	local_timer_register(&sprd_local_timer_ops);
	for (i = 0; i < CONFIG_NR_CPUS; i++) {
		ret = request_irq(irq_nr[i], __gptimer_interrupt,
				  IRQF_TIMER | IRQF_NOBALANCING | IRQF_DISABLED
				  | IRQF_PERCPU, "local_timer", local_evt);
		if (ret) {
			printk(KERN_ERR "request local timer irq %d failed\n",
			       irq_nr[i]);
		}
	}
#endif
#endif
	/* setup aon timer timer1 and syscnt as clocksource */
	__gptimer_clocksource_init();
	__syscnt_clocksource_init("syscnt", 1000);
	/* setup timer1 of aon timer as clockevent. */
	sprd_gptimer_clockevent_init(BC_IRQ, "bctimer", 32768);
	register_persistent_clock(NULL, sprd_read_persistent_clock);

	printk(KERN_INFO "sci_timer_init\n");
}

#ifdef CONFIG_OF
#define LOCAL_TIMER_CNT 4
static void __init sprd_init_timer(struct device_node *np)
{
	struct clk *clk;
	struct resource res;
	int i, ret;

	printk("%s \n", __func__);
	ret = of_address_to_resource(np, 0, &res);
	base_syscnt = res.start;
	if (ret < 0) {
		pr_err("Can't get syscnt registers");
		BUG();
	}
	for (i = 0; i < LOCAL_TIMER_CNT; i++) {
		ret = of_address_to_resource(np, i + 1, &res);
		if (ret < 0) {
			pr_err("Can't get timer %d registers for local timer",
			       i + 1);
			BUG();
		}
		*(base_gptimer + i) = res.start;
	}
	BC_IRQ = irq_of_parse_and_map(np, 0);
	if (BC_IRQ < 0) {
		pr_err("Can't map bc irq");
		BUG();
	}
	for (i = 0; i < LOCAL_TIMER_CNT; i++) {
		*(irq_nr + i) = irq_of_parse_and_map(np, i + 1);
		if (*(irq_nr + i) < 0) {
			pr_err("Can't map bc irq");
			BUG();
		}
	}
	of_node_put(np);
	sci_enable_timer_early();
	sci_timer_init();
}

CLOCKSOURCE_OF_DECLARE(scx35_timer, "sprd,scx35-timer", sprd_init_timer);
#endif
