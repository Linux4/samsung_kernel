#ifndef __ASMARM_ARCH_TIMER_H
#define __ASMARM_ARCH_TIMER_H

#include <linux/clocksource.h>

#ifdef CONFIG_ARM_ARCH_TIMER

enum ppi_nr {
	PHYS_SECURE_PPI,
	PHYS_NONSECURE_PPI,
	VIRT_PPI,
	HYP_PPI,
	MAX_TIMER_PPI
};

struct arch_timer {
	unsigned long rate;
	int ppi[MAX_TIMER_PPI];
	bool use_virtual;
	void __iomem *base;
};

/* PPI IRQs in GIC for generic timers */
#define IRQ_GIC_HYP_PPI			26
#define IRQ_GIC_VIRT_PPI		27
#define IRQ_GIC_PHYS_SECURE_PPI		29
#define IRQ_GIC_PHYS_NONSECURE_PPI	30

/* memory maped registers for generic timers */
#define CNTCR		0x00	/* Counter Control Register */
#define CNTCR_EN	0x1	/* The counter is enabled and incrementing */
#define CNTCR_HDBG	0x2	/* Halt on debug */
#define CNTSR		0x04	/* Counter Status Register */
#define CNTCVLW		0x08	/* Current value of counter[31:0] */
#define CNTCVUP		0x0C	/* Current value of counter[63:32] */
#define CNTFID0		0x20	/* Base frequency ID */

int arch_timer_init(struct arch_timer *at);

int arch_timer_register(void);
int arch_timer_of_register(void);
int arch_timer_sched_clock_init(void);
struct timecounter *arch_timer_get_timecounter(void);
#else /* #ifdef CONFIG_ARM_ARCH_TIMER */
static inline int arch_timer_init(struct arch_timer *at)
{
	return -ENXIO;
}

static inline int arch_timer_register(void)
{
	return -ENXIO;
}

static inline int arch_timer_of_register(void)
{
	return -ENXIO;
}

static inline int arch_timer_sched_clock_init(void)
{
	return -ENXIO;
}

static inline struct timecounter *arch_timer_get_timecounter(void)
{
	return NULL;
}
#endif

#endif
