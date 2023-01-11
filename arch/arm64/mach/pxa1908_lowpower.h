/*
 * linux/arch/arm64/mach/pxa1908_lowpower.h
 *
 * Author:	Raul Xiong <xjian@marvell.com>
 * Copyright:	(C) 2012 Marvell International Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#ifndef __MMP_MACH_PXA1908_LOWPOWER_H__
#define __MMP_MACH_PXA1908_LOWPOWER_H__

#include <linux/cpuidle.h>
/* APMU regs offset */
/* Core IDLE configuration */
#define CORE0_IDLE		0x124
#define CORE1_IDLE		0x128
#define CORE2_IDLE		0x160
#define CORE3_IDLE		0x164
#define DVC_DFC_DEBUG		0x140

/* Core Wakeup registers */
#define CORE0_RSTCTRL		0x12c
#define CORE1_RSTCTRL		0x130
#define CORE2_RSTCTRL		0x134
#define CORE3_RSTCTRL		0x138

/* MP IDLE configuration */
#define MP_CFG0			0x120
#define MP_CFG1			0x0e4
#define MP_CFG2			0x150
#define MP_CFG3			0x154

#define STBL_TIMER		0x084
/* MPMU regs offset */
#define CPCR			0x0
#define CWUCRS			0x0048
#define CWUCRM			0x004c
#define APCR			0x1000
#define AWUCRS			0x1048
#define AWUCRM			0x104c
/* ICU regs offset */
#define CORE0_CA9_GLB_INT_MASK	0x114
#define CORE1_CA9_GLB_INT_MASK	0x144

#define CORE0_CA7_GLB_INT_MASK	0x228
#define CORE1_CA7_GLB_INT_MASK	0x238
#define CORE2_CA7_GLB_INT_MASK	0x248
#define CORE3_CA7_GLB_INT_MASK	0x258

/* APBC regs offset */
#define TIMER0          0x034
#define TIMER1          0x044
#define TIMER2          0x068

/* bits definition */
#define PMUA_CORE_IDLE			(1 << 0)
#define PMUA_CORE_POWER_DOWN		(1 << 1)
#define PMUA_CORE_L1_SRAM_POWER_DOWN	(1 << 2)
#define PMUA_GIC_IRQ_GLOBAL_MASK	(1 << 3)
#define PMUA_GIC_FIQ_GLOBAL_MASK	(1 << 4)

#define PMUA_MP_IDLE			(1 << 0)
#define PMUA_MP_POWER_DOWN		(1 << 1)
#define PMUA_MP_L2_SRAM_POWER_DOWN	(1 << 2)
#define PMUA_MP_SCU_SRAM_POWER_DOWN	(1 << 3)
#define PMUA_MP_MASK_CLK_OFF		(1 << 11)
#define PMUA_DIS_MP_SLP			(1 << 18)

#define AP_DVC_EN_FOR_FAST_WAKEUP	(1 << 4)

#define ICU_MASK_FIQ			(1 << 0)
#define ICU_MASK_IRQ			(1 << 1)

#define PMUM_AXISD		(1 << 31)
#define PMUM_DSPSD		(1 << 30)
#define PMUM_SLPEN		(1 << 29)
#define PMUM_DTCMSD		(1 << 28)
#define PMUM_DDRCORSD		(1 << 27)
#define PMUM_APBSD		(1 << 26)
#define PMUM_BBSD		(1 << 25)
#define PMUM_INTCLR		(1 << 24)
#define PMUM_SLPWP0		(1 << 23)
#define PMUM_SLPWP1		(1 << 22)
#define PMUM_SLPWP2		(1 << 21)
#define PMUM_SLPWP3		(1 << 20)
#define PMUM_VCTCXOSD		(1 << 19)
#define PMUM_SLPWP4		(1 << 18)
#define PMUM_SLPWP5		(1 << 17)
#define PMUM_SLPWP6		(1 << 16)
#define PMUM_SLPWP7		(1 << 15)
#define PMUM_MSASLPEN		(1 << 14)
#define PMUM_STBYEN		(1 << 13)
#define PMUM_SPDTCMSD		(1 << 12)
#define PMUM_LDMA_MASK		(1 << 3)

#define PMUM_GSM_WAKEUPWMX	(1 << 29)
#define PMUM_WCDMA_WAKEUPX	(1 << 28)
#define PMUM_GSM_WAKEUPWM	(1 << 27)
#define PMUM_WCDMA_WAKEUPWM	(1 << 26)
#define PMUM_AP_ASYNC_INT	(1 << 25)
#define PMUM_AP_FULL_IDLE	(1 << 24)
#define PMUM_SQU_SDH1		(1 << 23)
#define PMUM_SDH_23		(1 << 22)
#define PMUM_KEYPRESS		(1 << 21)
#define PMUM_TRACKBALL		(1 << 20)
#define PMUM_NEWROTARY		(1 << 19)
#define PMUM_WDT		(1 << 18)
#define PMUM_RTC_ALARM		(1 << 17)
#define PMUM_CP_TIMER_3		(1 << 16)
#define PMUM_CP_TIMER_2		(1 << 15)
#define PMUM_CP_TIMER_1		(1 << 14)
#define PMUM_AP1_TIMER_3	(1 << 13)
#define PMUM_AP1_TIMER_2	(1 << 12)
#define PMUM_AP1_TIMER_1	(1 << 11)
#define PMUM_AP0_2_TIMER_3	(1 << 10)
#define PMUM_AP0_2_TIMER_2	(1 << 9)
#define PMUM_AP0_2_TIMER_1	(1 << 8)
#define PMUM_WAKEUP7		(1 << 7)
#define PMUM_WAKEUP6		(1 << 6)
#define PMUM_WAKEUP5		(1 << 5)
#define PMUM_WAKEUP4		(1 << 4)
#define PMUM_WAKEUP3		(1 << 3)
#define PMUM_WAKEUP2		(1 << 2)
#define PMUM_WAKEUP1		(1 << 1)
#define PMUM_WAKEUP0		(1 << 0)
#define PMUM_AP_WAKEUP_MASK     (0xFFFFFFFF & ~(PMUM_GSM_WAKEUPWM | PMUM_WCDMA_WAKEUPWM))
/* Slow clock control */
#define SCCR			0x038

extern void gic_raise_softirq(const struct cpumask *mask, unsigned int irq);

#ifndef __ASSEMBLER__

#define MAX_LATENCY	0xFFFFFFFF

enum pxa1908_lowpower_state {
	POWER_MODE_CORE_INTIDLE,	/* used for C1 */
	POWER_MODE_CORE_POWERDOWN,	/* used for C2 */
	POWER_MODE_MP_POWERDOWN,	/* used for M2 */
	POWER_MODE_APPS_IDLE,		/* used for D1P */
	POWER_MODE_SYS_SLEEP,		/* used for non-udr chip sleep, D1 */
	POWER_MODE_UDR_VCTCXO,		/* used for udr with vctcxo, D2 */
	POWER_MODE_UDR,			/* used for udr D2, suspend */
	POWER_MODE_MAX = 15,		/* maximum lowpower states */
};

extern void __iomem *icu_get_base_addr(void);

extern int cpuidle_simple_enter(struct cpuidle_device *dev,
		struct cpuidle_driver *drv, int index);
#endif
#endif
