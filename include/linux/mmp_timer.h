/*
 * mmp_timer: Soc timer driver for mmp architecture.
 *
 * Copyright (C) 2008 Marvell International Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __LINUX_MMP_TIMER_H__
#define __LINUX_MMP_TIMER_H__

/* timer flag bit definition */
/* bit[0]: MMP_TIMER_FLAG_SHADOW
 *         Indicate if the timer has shadow registers. If it has,
 *         counter could be read out directly.
 * bit[1]: MMP_TIMER_FLAG_CRSR
 *         Indicate if timer has CRSR register. If it has,
 *         counter could be restarted by directly writing CRSR.
 */
#define MMP_TIMER_FLAG_SHADOW	(1 << 0)
#define MMP_TIMER_FLAG_CRSR	(1 << 1)

#define MMP_TIMER_COUNTER_CLKSRC	(1 << 0)
#define MMP_TIMER_COUNTER_CLKEVT	(1 << 1)
#define MMP_TIMER_COUNTER_DELAY		(1 << 2)

#define MMP_TIMER_ALL_CPU	(0xFFFFFFFF)

int __init mmp_timer_init(int id, void __iomem *base,
			  unsigned int flag, unsigned int fc_freq,
			  unsigned int apb_freq);

int __init mmp_counter_clocksource_init(int tid, int cid, unsigned int freq);
int __init mmp_counter_timer_delay_init(int tid, int cid, unsigned int freq);
int __init mmp_counter_clockevent_init(int tid, int cid, int irq,
				       int freq, int dynirq, unsigned int cpu);

#endif /* __LINUX_MMP_TIMER_H__ */
