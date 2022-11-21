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

#include <soc/sprd/hardware.h>
#include <soc/sprd/sci.h>
#include <soc/sprd/sci_glb_regs.h>
#include <soc/sprd/sprd_persistent_clock.h>
struct sprd_ap_system_timer sprd_apsystimer;
static __iomem void *base_bctimer = NULL;
static int bc_irqnr = 0;

#define	TIMER_LOAD	(base_bctimer + 0x0000)
#define	TIMER_VALUE	(base_bctimer + 0x0004)
#define	TIMER_CTL	(base_bctimer + 0x0008)
#define	TIMER_INT	(base_bctimer + 0x000C)
#define	TIMER_CNT_RD	(base_bctimer + 0x0010)

#define	ONETIME_MODE	(0 << 6)
#define	PERIOD_MODE	(1 << 6)
#define	TIMER_DISABLE	(0 << 7)
#define	TIMER_ENABLE	(1 << 7)
#define TIMER_NEW       (1 << 8)

#define	TIMER_INT_EN	(1 << 0)
#define	TIMER_INT_CLR	(1 << 3)
#define	TIMER_INT_BUSY	(1 << 4)

#define	SYST_ALARM		(sprd_apsystimer.base + 0x0000)
#define	SYST_VALUE		(sprd_apsystimer.base + 0x0004)
#define	SYST_INT		(sprd_apsystimer.base + 0x0008)
#define	SYST_VALUE_SHDW		(sprd_apsystimer.base + 0x000C)

u32 get_sys_cnt(void)
{
	u32 val = 0;
	val = __raw_readl(SYST_VALUE_SHDW);
	return val;
}

void set_ap_system_timer_expires(u32 expires_ms)
{
	u32 val = get_sys_cnt();
	val = val + expires_ms;
	__raw_writel(val, SYST_ALARM);
	__raw_writel(1, SYST_INT);
}

static irqreturn_t sys_cnt_isr(int irq, void *dev_id)
{
	__raw_writel(8, SYST_INT);
	return IRQ_HANDLED;
}
/*
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
*/
static int sprd_ap_system_timer_init(void)
{
	struct resource res;
	int ret = 0;
	struct device_node *np = NULL;

	np = of_find_node_by_name(NULL, "sprd_ap_system_timer");
	if (!np) {
		panic("Can't get the sprd_ap_system_timer node!\n");
	}

	ret = of_address_to_resource(np, 0, &res);
	if (ret < 0) {
		panic("Can't get syscnt registers!\n");
	}
	sprd_apsystimer.base = ioremap_nocache(res.start, resource_size(&res));
	if (!sprd_apsystimer.base) {
		panic("ioremap_nocache failed!\n");
	}

	sprd_apsystimer.irqnr = irq_of_parse_and_map(np, 0);
	if (sprd_apsystimer.irqnr < 0) {
		panic("Can't map ap system timer irq!\n");
	}

	ret = request_irq(sprd_apsystimer.irqnr,
			sys_cnt_isr,IRQF_TRIGGER_HIGH | IRQF_NO_SUSPEND,
			"sys_cnt", NULL);
	if(ret){
		panic("sys cnt isr register failed\n");
	}

	of_node_put(np);

	sci_glb_set(REG_AON_APB_APB_EB0, BIT_AP_SYST_EB);

	/* disable irq for syscnt */
	__raw_writel(0, SYST_INT);
	/*register_persistent_clock(NULL, sprd_read_persistent_clock); */
	printk("sprd ap system timer init ok!\n");
	return 0;
}

arch_initcall(sprd_ap_system_timer_init);

static inline void bctimer_ctl(int enable, int mode)
{
	__raw_writel(enable | mode, TIMER_CTL);
}

static int bctimer_set_next_event(unsigned long cycles,
				  struct clock_event_device *c)
{
	while (TIMER_INT_BUSY & __raw_readl(TIMER_INT)) ;
	bctimer_ctl(TIMER_DISABLE, ONETIME_MODE);
	__raw_writel(cycles, TIMER_LOAD);
	bctimer_ctl(TIMER_ENABLE, ONETIME_MODE);
	return 0;
}

static void bctimer_set_mode(enum clock_event_mode mode,
			     struct clock_event_device *c)
{
	unsigned int saved;

	switch (mode) {
	case CLOCK_EVT_MODE_PERIODIC:
		bctimer_ctl(TIMER_DISABLE, PERIOD_MODE);
		__raw_writel(TIMER_INT_EN, TIMER_INT);
		break;
	case CLOCK_EVT_MODE_ONESHOT:
		bctimer_ctl(TIMER_DISABLE, ONETIME_MODE);
		__raw_writel(TIMER_INT_EN, TIMER_INT);
		break;
	case CLOCK_EVT_MODE_SHUTDOWN:
	case CLOCK_EVT_MODE_UNUSED:
		__raw_writel(TIMER_INT_CLR, TIMER_INT);
		saved = __raw_readl(TIMER_CTL) & PERIOD_MODE;
		bctimer_ctl(TIMER_DISABLE, saved);
		break;
	case CLOCK_EVT_MODE_RESUME:
		saved = __raw_readl(TIMER_CTL) & PERIOD_MODE;
		bctimer_ctl(TIMER_ENABLE, saved);
		break;
	}
}

static struct clock_event_device bctimer_event = {
	.features = CLOCK_EVT_FEAT_ONESHOT,
	.shift = 32,
	.rating = 150,
	.set_next_event = bctimer_set_next_event,
	.set_mode = bctimer_set_mode,
};

static irqreturn_t bctimer_interrupt(int irq, void *dev_id)
{
	unsigned int value;
	struct clock_event_device *evt = dev_id;

	value = __raw_readl(TIMER_INT);
	value |= TIMER_INT_CLR;
	__raw_writel(value, TIMER_INT);
	if (evt->event_handler)
		evt->event_handler(evt);

	return IRQ_HANDLED;
}

static struct irqaction bctimer_irq = {
	.name = "bctimer",
	.flags = IRQF_DISABLED | IRQF_TIMER | IRQF_IRQPOLL,
	.handler = bctimer_interrupt,
	.dev_id = &bctimer_event,
};

static void sprd_gptimer_clockevent_init(unsigned int irq, const char *name,
					 unsigned long hz)
{
	struct clock_event_device *evt = &bctimer_event;
	int ret = 0;

	__raw_writel(TIMER_DISABLE, TIMER_CTL);
	__raw_writel(TIMER_INT_CLR, TIMER_INT);

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

static void sci_enable_timer_early(void)
{
	/* enable timer, need modify */
	sci_glb_set(REG_AON_APB_APB_EB0,
		    BIT_AON_TMR_EB | BIT_AP_SYST_EB | BIT_AP_TMR0_EB);
	sci_glb_set(REG_AON_APB_APB_EB1, BIT_AP_TMR2_EB | BIT_AP_TMR1_EB);
}

static int sprd_init_timer_sharkl64(void)
{
	struct resource res;
	int ret = 0;
	struct device_node *np = NULL;
	unsigned int bc_timer_rate = 0;

	np = of_find_node_by_name(NULL, "sprd_timer");
	if (!np) {
		printk("Can't get the sprd_sharkl64_timer node!\n");
		return -ENODEV;
	}

	ret = of_address_to_resource(np, 0, &res);
	if (ret < 0) {
		panic("Can't get syscnt registers!\n");
	}
	base_bctimer = ioremap_nocache(res.start, resource_size(&res));
	if (!base_bctimer) {
		panic("ioremap_nocache failed!");
	}

	bc_irqnr = irq_of_parse_and_map(np, 0);
	if (bc_irqnr < 0) {
		panic("Can't map bc irq");
	}

	of_property_read_u32(np, "clock-frequency", &bc_timer_rate);
	if (bc_timer_rate == 0) {
		printk("bc timer frequency not available\n");
		of_node_put(np);
		return -ENODEV;
	}

	of_node_put(np);
	printk("%s,bc_irqnr = %d,clock-frequency is %d\n", __func__, bc_irqnr,
	       bc_timer_rate);
	sci_enable_timer_early();
	sprd_gptimer_clockevent_init(bc_irqnr, "bctimer", bc_timer_rate);
	return 0;
}

subsys_initcall(sprd_init_timer_sharkl64);
