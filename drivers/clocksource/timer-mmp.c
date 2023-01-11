/*
 * driver/clocksource/timer-mmp.c
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
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/cpu.h>
#include <linux/mmp_timer.h>
#include <linux/clockchips.h>
#include <linux/sched_clock.h>

#define TMR_CCR		(0x0000)
#define TMR_TN_MM(n, m)	(0x0004 + ((n) << 3) + (((n) + (m)) << 2))
#define TMR_CR(n)	(0x0028 + ((n) << 2))
#define TMR_SR(n)	(0x0034 + ((n) << 2))
#define TMR_IER(n)	(0x0040 + ((n) << 2))
#define TMR_PLVR(n)	(0x004c + ((n) << 2))
#define TMR_PLCR(n)	(0x0058 + ((n) << 2))
#define TMR_WMER	(0x0064)
#define TMR_WMR		(0x0068)
#define TMR_WVR		(0x006c)
#define TMR_WSR		(0x0070)
#define TMR_ICR(n)	(0x0074 + ((n) << 2))
#define TMR_WICR	(0x0080)
#define TMR_CER		(0x0084)
#define TMR_CMR		(0x0088)
#define TMR_ILR(n)	(0x008c + ((n) << 2))
#define TMR_WCR		(0x0098)
#define TMR_WFAR	(0x009c)
#define TMR_WSAR	(0x00A0)
#define TMR_CVWR(n)	(0x00A4 + ((n) << 2))
#define TMR_CRSR        (0x00B0)

#define TMR_CCR_CS_0(x)	(((x) & 0x3) << 0)
#define TMR_CCR_CS_1(x)	(((x) & 0x3) << 2)
#define TMR_CCR_CS_2(x)	(((x) & 0x3) << 5)

#define MAX_EVT_NUM		5

#define MAX_DELTA		(0xfffffffe)
#define MIN_DELTA		(5)

#define MMP_MAX_COUNTER		3
#define MMP_MAX_TIMER		4

#define TMR_CER_COUNTER(cid)	(1 << (cid))
#define MMP_ALL_COUNTERS	((1 << MMP_MAX_COUNTER) - 1)

#define MMP_TIMER_CLOCK_32KHZ	32768
#define MMP_TIMER_CLOCK_1KHZ	1000

struct mmp_timer;
struct mmp_timer_counter {
	unsigned int id;
	unsigned int usage;
	unsigned int cnt_freq;
	int cpu;
	int loop_delay;
	struct mmp_timer *timer;
};

struct mmp_timer {
	unsigned int id;
	void __iomem *base;
	struct mmp_timer_counter counters[MMP_MAX_COUNTER];
	unsigned int flag;
	int loop_delay_fastclk;
	unsigned int fc_freq;
	/* lock to protect hw operation. */
	spinlock_t tm_lock;
};

struct mmp_timer_clkevt {
	struct mmp_timer_counter *counter;
	struct clock_event_device ced;
	struct irqaction irqa;
	struct notifier_block nb;
};

struct mmp_timer_clksrc {
	struct mmp_timer_counter *counter;
	struct clocksource cs;
};

#ifdef CONFIG_ARM
struct mmp_timer_dclk {
	struct mmp_timer_counter *counter;
	struct delay_timer *dt;
};
static struct mmp_timer_dclk *dclk;
#endif

static struct mmp_timer *mmp_timers[MMP_MAX_TIMER];
static struct mmp_timer_clksrc *clksrc;

static int timer_counter_switch_clock(int tid, int cid, unsigned int freq)
{
	struct mmp_timer *tm = mmp_timers[tid];
	u32 ccr, mask;

	ccr = __raw_readl(tm->base + TMR_CCR);

	if (cid == 0)
		mask = TMR_CCR_CS_0(3);
	else if (cid == 1)
		mask = TMR_CCR_CS_1(3);
	else
		mask = TMR_CCR_CS_2(3);

	ccr &= ~mask;

	if (freq == MMP_TIMER_CLOCK_32KHZ) {
		if (cid == 2)
			ccr |= TMR_CCR_CS_2(2);
		else if (cid == 1)
			ccr |= TMR_CCR_CS_1(1);
		else if (cid == 0)
			ccr |= TMR_CCR_CS_0(1);
	} else if (freq == MMP_TIMER_CLOCK_1KHZ) {
		if (cid == 2)
			ccr |= TMR_CCR_CS_2(1);
		else if (cid == 1)
			ccr |= TMR_CCR_CS_1(2);
	} else if (freq == tm->fc_freq) {
		if (cid == 2)
			ccr |= TMR_CCR_CS_2(0);
		else if (cid == 1)
			ccr |= TMR_CCR_CS_1(0);
		else if (cid == 0)
			ccr |= TMR_CCR_CS_0(0);
	} else {
		pr_err("Timer %d:%d: invalid clock rate %d\n", tid, cid, freq);
		return -EINVAL;
	}

	__raw_writel(ccr, tm->base + TMR_CCR);

	return 0;
}

static void timer_counter_disable(struct mmp_timer_counter *cnt)
{
	struct mmp_timer *tm = cnt->timer;
	int delay = tm->loop_delay_fastclk;
	u32 cer;

	/*
	 * Stop the counter will need mutiple timer clock to take effect.
	 * Some operations can only be done when counter is disabled. So
	 * add delay here.
	 */
	/* Step1: disable counter */
	cer = __raw_readl(tm->base + TMR_CER);
	__raw_writel(cer & ~(1 << cnt->id), tm->base + TMR_CER);

	/*
	 * Step2: switch to fast clock, so the delay can be completed
	 * quickly.
	 */
	if (cnt->cnt_freq != tm->fc_freq)
		timer_counter_switch_clock(tm->id, cnt->id, tm->fc_freq);

	/*
	 * Step3: Loop for mutiple timer cycles. We do it by clearing
	 * pending interrupt status.
	 */
	while (delay--)
		__raw_writel(0x1, tm->base + TMR_ICR(cnt->id));
}

static void timer_counter_enable(struct mmp_timer_counter *cnt)
{
	struct mmp_timer *tm = cnt->timer;
	u32 cer;

	/* Switch to original clock */
	if (cnt->cnt_freq != tm->fc_freq)
		timer_counter_switch_clock(tm->id, cnt->id, cnt->cnt_freq);

	/* Enable timer */
	cer = __raw_readl(tm->base + TMR_CER);
	__raw_writel(cer | (1 << cnt->id), tm->base + TMR_CER);
}

static inline uint32_t timer_read(struct mmp_timer_counter *cnt)
{
	struct mmp_timer *tm = cnt->timer;
	int has_shadow = tm->flag & MMP_TIMER_FLAG_SHADOW;
	int delay = 3;
	u32 val1, val2;

	if (has_shadow) {
		return __raw_readl(tm->base + TMR_CR(cnt->id));
	} else {
		if (cnt->cnt_freq != tm->fc_freq) {
			/* slow clock */
			do {
				val1 = __raw_readl(tm->base + TMR_CR(cnt->id));
				val2 = __raw_readl(tm->base + TMR_CR(cnt->id));
			} while (val2 != val1);
		} else {
			/* fast clock */
			__raw_writel(1, tm->base + TMR_CVWR(cnt->id));
			while (delay--)
				val1 = __raw_readl(tm->base +
						TMR_CVWR(cnt->id));
		}
		return val1;
	}
}

static irqreturn_t timer_interrupt(int irq, void *dev_id)
{
	struct clock_event_device *c = dev_id;
	struct mmp_timer_clkevt *evt;
	struct mmp_timer_counter *counter;
	unsigned int cnt;
	unsigned long flags;
	void __iomem *base;
	int has_crsr;

	evt = container_of(c, struct mmp_timer_clkevt, ced);
	counter = evt->counter;
	cnt = counter->id;
	base = counter->timer->base;
	has_crsr = counter->timer->flag & MMP_TIMER_FLAG_CRSR;

	spin_lock_irqsave(&(counter->timer->tm_lock), flags);
	/* We only use match #0 for the counter. */
	if (__raw_readl(base + TMR_SR(cnt)) & 0x1) {
		if (!has_crsr)
			timer_counter_disable(counter);

		/* Disable the interrupt. */
		__raw_writel(0x00, base + TMR_IER(cnt));
		/* Clear interrupt status */
		__raw_writel(0x01, base + TMR_ICR(cnt));

		spin_unlock_irqrestore(&(counter->timer->tm_lock), flags);

		c->event_handler(c);

		return IRQ_HANDLED;
	}

	spin_unlock_irqrestore(&(counter->timer->tm_lock), flags);

	return IRQ_NONE;
}

static int timer_set_next_event(unsigned long delta,
				struct clock_event_device *dev)
{
	struct mmp_timer_counter *cnt;
	struct mmp_timer_clkevt *evt;
	unsigned long flags;
	unsigned int cid;
	u32 cer, crsr;
	void __iomem *base;
	int delay, has_crsr;

	evt = container_of(dev, struct mmp_timer_clkevt, ced);
	cnt = evt->counter;
	cid = cnt->id;
	base = cnt->timer->base;
	has_crsr = cnt->timer->flag & MMP_TIMER_FLAG_CRSR;

	spin_lock_irqsave(&(cnt->timer->tm_lock), flags);
	if (has_crsr) {
		/*
		 * Use TMR_CRSR to restart the counter and make match
		 * register take effect. This bit should be 0 before
		 * set it again.
		 * The polling loop is defined by loop_delay.
		 */
		delay = cnt->loop_delay;
		do {
			crsr = __raw_readl(base + TMR_CRSR);
			delay--;
		} while ((crsr & (1 << cid)) && delay > 0);

		BUG_ON(delay <= 0);

		__raw_writel(delta - 1, base + TMR_TN_MM(cid, 0));
		/*
		 * After counter is restart, clear the interrupt status for
		 * safe, and re-enable the interrupt for match #0.
		 */
		__raw_writel(0x01, base + TMR_ICR(cid));
		__raw_writel(0x01, base + TMR_IER(cid));
		__raw_writel((1 << cid), base + TMR_CRSR);
	} else {
		cer = __raw_readl(base + TMR_CER);

		/* If the timer counter is enabled, first disable it. */
		if (cer & (1 << cid))
			timer_counter_disable(cnt);

		/* Setup new counter value */
		__raw_writel(delta - 1, base + TMR_TN_MM(cid, 0));

		/* enable the matching interrupt */
		__raw_writel(0x1, base + TMR_IER(cid));

		timer_counter_enable(cnt);
	}
	spin_unlock_irqrestore(&(cnt->timer->tm_lock), flags);

	return 0;
}

static void timer_set_mode(enum clock_event_mode mode,
			   struct clock_event_device *dev)
{
	unsigned long flags;
	unsigned int cnt;
	struct mmp_timer_counter *counter;
	struct mmp_timer_clkevt *evt;
	void __iomem *base;

	evt = container_of(dev, struct mmp_timer_clkevt, ced);
	counter = evt->counter;
	cnt = counter->id;
	base = counter->timer->base;

	spin_lock_irqsave(&(counter->timer->tm_lock), flags);
	switch (mode) {
	case CLOCK_EVT_MODE_UNUSED:
	case CLOCK_EVT_MODE_SHUTDOWN:
		timer_counter_disable(counter);
		break;
	case CLOCK_EVT_MODE_RESUME:
	case CLOCK_EVT_MODE_PERIODIC:
	case CLOCK_EVT_MODE_ONESHOT:
		timer_counter_enable(counter);
		break;
	}
	spin_unlock_irqrestore(&(counter->timer->tm_lock), flags);
}

static cycle_t clksrc_read(struct clocksource *cs)
{
	return timer_read(clksrc->counter);
}

static u64 notrace mmp_read_sched_clock(void)
{
	return timer_read(clksrc->counter);
}

#ifdef CONFIG_ARM
static unsigned long d_read_current_timer(void)
{
	return timer_read(dclk->counter);
}

static struct delay_timer d_timer = {
	.read_current_timer	= d_read_current_timer,
};
#endif

static int mmp_timer_cpu_notify(struct notifier_block *self,
				unsigned long action, void *hcpu)
{
	struct mmp_timer_clkevt *evt;
	struct mmp_timer_counter *cnt;

	evt = container_of(self, struct mmp_timer_clkevt, nb);
	cnt = evt->counter;

	if (cnt->cpu != (unsigned long)hcpu)
		return NOTIFY_OK;

	switch (action & ~CPU_TASKS_FROZEN) {
	case CPU_STARTING:
		clockevents_config_and_register(&evt->ced,
						cnt->cnt_freq,
						MIN_DELTA, MAX_DELTA);
		break;
	case CPU_ONLINE:
		irq_set_affinity(evt->ced.irq, evt->ced.cpumask);
		enable_irq(evt->ced.irq);
		break;
	case CPU_DYING:
		clockevents_set_mode(&evt->ced,
				     CLOCK_EVT_MODE_SHUTDOWN);
		disable_irq(evt->ced.irq);
		break;
	}

	return NOTIFY_OK;
}

int __init mmp_timer_init(int tid, void __iomem *base,
			  unsigned int flag, unsigned int fc_freq,
			  unsigned int apb_freq)
{
	struct mmp_timer *tm = mmp_timers[tid];
	u32 tmp, delay;
	int cid;

	if (tm)
		return -EINVAL;

	tm = kzalloc(sizeof(*tm), GFP_KERNEL);
	if (!tm)
		return -ENOMEM;

	/*
	 * The calculation formula for the loop cycle is:
	 *
	 * (1) need wait for 2 timer's clock cycle:
	 *        1             2
	 *     ------- x 2 = -------
	 *     fc_freq       fc_freq
	 *
	 * (2) convert to apb clock cycle:
	 *        2          1        apb_freq * 2
	 *     ------- / -------- = ----------------
	 *     fc_freq   apb_freq       fc_freq
	 *
	 * (3) every apb register's accessing will take 8 apb clock cycle,
	 *     also consider add extral one more time for safe way;
	 *     so finally need loop times for the apb register accessing:
	 *
	 *       (apb_freq * 2)
	 *     ------------------ / 8 + 1
	 *          fc_freq
	 */
	delay = ((apb_freq * 2) / fc_freq / 8) + 1;
	pr_debug("Timer %d: loop_delay_fastclk is %d\n", tid, delay);

	tm->id = tid;
	tm->base = base;
	tm->flag = flag;
	tm->loop_delay_fastclk = delay;
	tm->fc_freq = fc_freq;
	spin_lock_init(&(tm->tm_lock));

	mmp_timers[tid] = tm;

	for (cid = 0; cid < MMP_MAX_COUNTER; cid++) {
		tm->counters[cid].id = cid;
		tm->counters[cid].timer = tm;

		/* We will disable all counters. Switch to fastclk first. */
		timer_counter_switch_clock(tid, cid, fc_freq);
	}

	/* disalbe all counters */
	tmp = __raw_readl(base + TMR_CER) & ~MMP_ALL_COUNTERS;
	__raw_writel(tmp, base + TMR_CER);

	/* disable matching interrupt */
	__raw_writel(0x00, base + TMR_IER(0));
	__raw_writel(0x00, base + TMR_IER(1));
	__raw_writel(0x00, base + TMR_IER(2));

	while (delay--) {
		/* Clear pending interrupt status */
		__raw_writel(0x1, base + TMR_ICR(0));
		__raw_writel(0x1, base + TMR_ICR(1));
		__raw_writel(0x1, base + TMR_ICR(2));
		__raw_writel(tmp, base + TMR_CER);
	}

	return 0;
}

static int __init mmp_timer_counter_hw_init(struct mmp_timer *tm, int cid,
					    unsigned int freq)
{
	u32 tmp, delay;
	unsigned int ratio;
	int ret;

	ret = timer_counter_switch_clock(tm->id, cid, freq);
	if (ret)
		return ret;

	ratio = tm->fc_freq / tm->counters[cid].cnt_freq;
	tm->counters[cid].cnt_freq = freq;
	tm->counters[cid].loop_delay = tm->loop_delay_fastclk * ratio;

	/* set timer to free-running mode */
	tmp = __raw_readl(tm->base + TMR_CMR) | TMR_CER_COUNTER(cid);
	__raw_writel(tmp, tm->base + TMR_CMR);

	/* free-running */
	__raw_writel(0x0, tm->base + TMR_PLCR(cid));
	/* clear status */
	__raw_writel(0x7, tm->base + TMR_ICR(cid));

	/* enable counter */
	tmp = __raw_readl(tm->base + TMR_CER) | TMR_CER_COUNTER(cid);
	__raw_writel(tmp, tm->base + TMR_CER);

	delay = tm->counters[cid].loop_delay;
	while (delay--)
		__raw_writel(tmp, tm->base + TMR_CER);

	return 0;
}

int __init mmp_counter_clocksource_init(int tid, int cid, unsigned int freq)
{
	struct mmp_timer *mt = mmp_timers[tid];
	int ret;

	if (!mt)
		return -EINVAL;

	if (cid < 0 || cid >= MMP_MAX_COUNTER)
		return -EINVAL;

	if (clksrc) {
		pr_err("One clksrc has already been registered!\n");
		return -EINVAL;
	}
	clksrc = kzalloc(sizeof(*clksrc), GFP_KERNEL);
	if (!clksrc)
		return -ENOMEM;

	mt->counters[cid].usage |= MMP_TIMER_COUNTER_CLKSRC;
	mt->counters[cid].cnt_freq = freq;

	clksrc->counter = &mt->counters[cid];
	clksrc->cs.name = "clocksource-mmp";
	clksrc->cs.rating = 200;
	clksrc->cs.read = clksrc_read;
	clksrc->cs.mask = CLOCKSOURCE_MASK(32);
	clksrc->cs.flags = CLOCK_SOURCE_IS_CONTINUOUS;

	sched_clock_register(mmp_read_sched_clock, 32, freq);

	clocksource_register_hz(&(clksrc->cs), freq);

	ret = mmp_timer_counter_hw_init(mt, cid, freq);
	if (ret)
		return ret;

	return 0;
}

int __init mmp_counter_timer_delay_init(int tid, int cid, unsigned int freq)
{
#ifdef CONFIG_ARM
	struct mmp_timer *mt = mmp_timers[tid];
	int ret;

	if (!mt)
		return -EINVAL;

	if (cid < 0 || cid >= MMP_MAX_COUNTER)
		return -EINVAL;

	if (dclk) {
		pr_err("Delay clock has already been registered!\n");
		return -EINVAL;
	}
	dclk = kzalloc(sizeof(*dclk), GFP_KERNEL);
	if (!dclk)
		return -ENOMEM;

	mt->counters[cid].usage |= MMP_TIMER_COUNTER_DELAY;
	mt->counters[cid].cnt_freq = freq;

	dclk->counter = &mt->counters[cid];
	dclk->dt = &d_timer;
	d_timer.freq = freq;
	register_current_timer_delay(&d_timer);

	ret = mmp_timer_counter_hw_init(mt, cid, freq);
	if (ret)
		return ret;
#endif
	return 0;
}

int __init mmp_counter_clockevent_init(int tid, int cid, int irq,
				       int freq, int dynirq, unsigned int cpu)
{
	struct mmp_timer *mt = mmp_timers[tid];
	struct mmp_timer_clkevt *clkevt;
	int broadcast = 0;
	int ret;

	if (!mt)
		return -EINVAL;

	if (cid < 0 || cid >= MMP_MAX_COUNTER)
		return -EINVAL;

	if (cpu == MMP_TIMER_ALL_CPU)
		broadcast = 1;
	else if (cpu >= num_possible_cpus())
		return -EINVAL;

	mt->counters[cid].usage |= MMP_TIMER_COUNTER_CLKEVT;
	mt->counters[cid].cnt_freq = freq;
	mt->counters[cid].cpu = cpu;

	clkevt = kzalloc(sizeof(*clkevt), GFP_KERNEL);
	if (!clkevt)
		return -ENOMEM;
	clkevt->counter = &mt->counters[cid];
	clkevt->ced.name = "clockevent-mmp";
	clkevt->ced.features = CLOCK_EVT_FEAT_ONESHOT;
	clkevt->ced.rating = 200;
	clkevt->ced.set_next_event = timer_set_next_event;
	clkevt->ced.set_mode = timer_set_mode;
	clkevt->ced.irq = irq;

	clkevt->irqa.flags = IRQF_TIMER | IRQF_IRQPOLL;
	clkevt->irqa.handler = timer_interrupt;
	clkevt->irqa.dev_id = &(clkevt->ced);

	ret = mmp_timer_counter_hw_init(mt, cid, freq);
	if (ret)
		return ret;

	if (broadcast) {
		clkevt->irqa.name = "broadcast-timer";
		if (dynirq)
			clkevt->ced.features |= CLOCK_EVT_FEAT_DYNIRQ;
		clkevt->ced.cpumask = cpu_possible_mask;
		setup_irq(clkevt->ced.irq, &(clkevt->irqa));
		clockevents_config_and_register(&clkevt->ced,
						freq, MIN_DELTA, MAX_DELTA);
	} else {
		clkevt->irqa.name = "local-timer";
		clkevt->ced.cpumask = cpumask_of(cpu);
		clkevt->nb.notifier_call = mmp_timer_cpu_notify;
		clkevt->irqa.flags |= IRQF_PERCPU;
		register_cpu_notifier(&clkevt->nb);
		setup_irq(clkevt->ced.irq, &(clkevt->irqa));
		/* Enable clock event device for boot CPU. */
		if (cpu == smp_processor_id()) {
			clockevents_config_and_register(&clkevt->ced,
							freq, MIN_DELTA,
							MAX_DELTA);
			/* Only online CPU can be set affinity. */
			irq_set_affinity(clkevt->ced.irq, cpumask_of(cpu));
		} else {
			/* disable none boot CPU's irq at first */
			disable_irq(clkevt->ced.irq);
		}
	}

	return 0;
}

#ifdef CONFIG_OF

struct of_device_id mmp_counter_of_id[] = {
	{
		.compatible = "marvell,timer-counter-clksrc",
		.data = (void *)MMP_TIMER_COUNTER_CLKSRC,
	},
	{
		.compatible = "marvell,timer-counter-clkevt",
		.data = (void *)MMP_TIMER_COUNTER_CLKEVT,
	},
	{
		.compatible = "marvell,timer-counter-delay",
		.data = (void *)MMP_TIMER_COUNTER_DELAY,
	},
	{ },
};

static int __init mmp_of_counter_init(struct device_node *np, int tid,
				      unsigned int usage)
{
	int irq, ret, dynirq;
	unsigned int cid, freq, cpu;

	if (!np)
		return -EINVAL;

	ret = of_property_read_u32(np, "marvell,timer-counter-id", &cid);
	if (ret) {
		pr_err("Timer %d: fail to get counter id\n", tid);
		return ret;
	}

	ret = of_property_read_u32(np, "marvell,timer-counter-frequency",
				   &freq);
	if (ret) {
		pr_err("Timer %d:%d: fail to get counter frequency\n",
		       tid, cid);
		return ret;
	}

	if (usage & MMP_TIMER_COUNTER_DELAY) {
		ret = mmp_counter_timer_delay_init(tid, cid, freq);
		if (ret) {
			pr_err("Timer %d:%d: fail to create delay timer\n",
			       tid, cid);
			return ret;
		}
	}

	if (usage & MMP_TIMER_COUNTER_CLKSRC) {
		ret = mmp_counter_clocksource_init(tid, cid, freq);
		if (ret) {
			pr_err("Timer %d:%d: fail to create clksrc\n",
			       tid, cid);
			return ret;
		}
	}
	if (usage & MMP_TIMER_COUNTER_CLKEVT) {
		if (of_property_read_bool(np,
					  "marvell,timer-counter-broadcast")) {
			cpu = MMP_TIMER_ALL_CPU;
		} else {
			ret = of_property_read_u32(np,
						   "marvell,timer-counter-cpu",
						   &cpu);
			if (ret) {
				pr_err("Timer %d:%d: fail to get cpu\n",
				       tid, cid);
				return ret;
			}
		}
		dynirq = !of_property_read_bool(np,
				"marvell,timer-counter-nodynirq");
		irq = irq_of_parse_and_map(np, 0);
		ret = mmp_counter_clockevent_init(tid, cid,
						  irq, freq, dynirq, cpu);
		if (ret) {
			pr_err("Timer %d:%d: fail to create clkevt\n",
			       tid, cid);
			return ret;
		}
	}

	return 0;
}

static void __init mmp_of_timer_init(struct device_node *np)
{
	int tid;
	unsigned int flag, fc_freq, apb_freq;
	void __iomem *base;
	struct device_node *child_np;
	const struct of_device_id *match;
	int ret = 0;

	for (tid = 0; tid < MMP_MAX_TIMER; tid++)
		if (mmp_timers[tid] == NULL)
			break;

	if (tid >= MMP_MAX_TIMER)
		return;

	/* timer initialization */
	base = of_iomap(np, 0);
	if (!base) {
		pr_err("Timer: fail to map register space\n");
		ret = -EINVAL;
		goto out;
	}

	flag = 0;
	if (of_property_read_bool(np, "marvell,timer-has-crsr"))
		flag |= MMP_TIMER_FLAG_CRSR;

	if (of_property_read_bool(np, "marvell,timer-has-shadow"))
		flag |= MMP_TIMER_FLAG_SHADOW;

	/* timer's fast clock and apb frequency */
	ret = of_property_read_u32(np, "marvell,timer-fastclk-frequency",
			&fc_freq);
	if (ret) {
		pr_err("Timer %d: fail to get fastclk-frequency with err %d\n",
		       tid, ret);
		goto out;
	}
	ret = of_property_read_u32(np, "marvell,timer-apb-frequency",
				   &apb_freq);
	if (ret) {
		pr_err("Timer %d: fail to get apb-frequency with err %d\n",
		       tid, ret);
		goto out;
	}

	/*
	 * Need use loop for more safe register's accessing,
	 * so at here dynamically calculate the loop time.
	 */
	if (!fc_freq || !apb_freq) {
		pr_err("mmp timer's fast clock or apb freq are incorrect!\n");
		ret = -EINVAL;
		goto out;
	}

	ret = mmp_timer_init(tid, base, flag, fc_freq, apb_freq);
	if (ret)
		goto out;

	/*
	 * If device node is marked as not available,
	 * we then don't try to enable the counter again
	 */
	if (!of_device_is_available(np)) {
		pr_warn("Timer %d: is not used\n", tid);
		return;
	}

	/* counter initialization */
	for_each_child_of_node(np, child_np) {
		match = of_match_node(mmp_counter_of_id, child_np);
		ret = mmp_of_counter_init(child_np, tid,
					  (unsigned long)match->data);
		if (ret)
			goto out;
	}

	return;
out:
	if (ret)
		pr_err("Failed to get timer from dtb with error:%d\n", ret);
	return;
}

CLOCKSOURCE_OF_DECLARE(mmp_timer, "marvell,mmp-timer", mmp_of_timer_init);
#endif
