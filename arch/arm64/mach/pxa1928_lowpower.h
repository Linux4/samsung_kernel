/*
 * linux/arch/arm/mach-mmp/include/mach/pxa1928_lowpower.h
 *
 * Author:	Raul Xiong <xjian@marvell.com>
 * Copyright:	(C) 2012 Marvell International Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#ifndef __MMP_MACH_PXA1928_LOWPOWER_H__
#define __MMP_MACH_PXA1928_LOWPOWER_H__

/* APMU regs offset */
/* Core power mode configuration */
#define CORE0_PWRMODE		0x280
#define CORE1_PWRMODE		0x284
#define CORE2_PWRMODE		0x288
#define CORE3_PWRMODE		0x28c

/* CORE RSTCTRL */
#define CORE0_RSTCTRL		0x2a0
#define CORE1_RSTCTRL		0x2a4
#define CORE2_RSTCTRL		0x2a8
#define CORE3_RSTCTRL		0x2ac

/* APSS power mode configuration */
#define APPS_PWRMODE		0x2c0

/* APSS power island */
#define SP_PWRMODE			0x2d0
#define ISLD_PWR_STATUS		0x220
#define LCD_PWRCTRL			0x204
#define VPU_PWRCTRL			0x208
#define GC3D_PWRCTRL		0x20c
#define GC2D_PWRCTRL		0x210
#define ISP_PWRCTRL			0x214

#define ISLD_PWR_STAT		(0x1 << 4)

/* wake up mask */
#define WKUP_MASK		0x2c8
#define WKUP_STAT       0x2cc

/* wake up stat */
#define WKUP_STAT		0x2cc

/* wake up clear*/
#define WKUP_CLR		0x07c

/* MC clk reset control */
#define MC_CLK_RES_CTRL		0x258

/* MPMU regs offset */
#define APCR			0x1000
#define CPCR            0x0
#define CPSR			0x4
#define ANAGRP_CTRL1	0x1130

#define CURR_REF_PU		0x1

/* AP Core n Reset Control Register */
#define PMUA_SW_WAKEUP			(0x1 << 16)
#define PMUA_CORE_RSTCTRL_CPU_NRESET	(0x1<<1)
#define PMUA_CORE_RSTCTRL_CPU_NPORESET	(0x1<<0)

/* AP Core n Power Mode Register */
#define PMUA_CLR_C2_STATUS		(0x1 << 30)
#define PMUA_APCORESS_MP1		(0x2 << 8)
#define PMUA_APCORESS_MP2		(0x3 << 8)
#define PMUA_L2SR_FLUSHREQ_VT		(0x1 << 5)
#define PMUA_L2SR_PWROFF_VT		(0x1 << 4)
#define PMUA_CPU_PWRMODE_C1		(0x1 << 0)
#define PMUA_CPU_PWRMODE_C2		(0x3 << 0)

#define PMUA_CORE_PWRMODE		(3 << 0)
#define PMUA_CORESS_PWRMODE		(3 << 8)
#define PMUA_GIC_IRQ_GLOBAL_MASK	(1 << 12)
#define PMUA_GIC_FIQ_GLOBAL_MASK	(1 << 13)

#define PMUA_APPS_D2_STATUS		(1 << 31)
#define PMUA_APPS_CLR_D2_STATUS	(1 << 30)
#define PMUA_LCD_REFRESH_EN		(1 << 4)
#define PMUA_APPS_D0CG			(0x1 << 0)
#define PMUA_APPS_D1D2			(0x2 << 0)
#define PMUA_APPS_PWRMODE_MSK		(0x3 << 0)
#define PMUM_APCR_BASE		0xBF000000
#define PMUM_APCR_MASK		0x1F000
#define PMUM_INTCLR		(1 << 24)
#define PMUM_VCTCXOSD		(1 << 19)

#define PMUA_CP_IPC		(1 << 31)
#define PMUA_CP_TIMER1  (1 << 30)
#define PMUA_PMIC		(1 << 29)
#define PMUA_USB		(1 << 28)
#define PMUA_AUDIO		(1 << 26)
#define PMUA_GPIO		(1 << 25)
#define PMUA_GPIO_SEC   (1 << 23)
#define PMUA_SSP2		(1 << 21)
#define PMUA_SSP1		(1 << 20)
#define PMUA_SDH2		(1 << 17)
#define PMUA_SDH1		(1 << 16)
#define PMUA_KEYPAD		(1 << 14)
#define PMUA_TRACKBALL		(1 << 13)
#define PMUA_ROTARYKEY		(1 << 12)
#define PMUA_RTC_ALARM		(1 << 11)
#define PMUA_WTD_TIMER_2	(1 << 10)
#define PMUA_WTD_TIMER_1	(1 << 9)
#define PMUA_TIMER_3_3		(1 << 8)
#define PMUA_TIMER_3_2		(1 << 7)
#define PMUA_TIMER_3_1		(1 << 6)
#define PMUA_TIMER_2_3		(1 << 5)
#define PMUA_TIMER_2_2		(1 << 4)
#define PMUA_TIMER_2_1		(1 << 3)
#define PMUA_TIMER_1_3		(1 << 2)
#define PMUA_TIMER_1_2		(1 << 1)
#define PMUA_TIMER_1_1		(1 << 0)

#if defined(CONFIG_PXA1928_SOC_TIMER1)
#define PMUA_TIMER_WK_MSK	PMUA_TIMER_1_1
#elif defined(CONFIG_PXA1928_SOC_TIMER2)
#define PMUA_TIMER_WK_MSK	PMUA_TIMER_2_1
#else
#define PMUA_TIMER_WK_MSK	PMUA_TIMER_1_1
#endif

/* ICU related */
#define	ICU_CORE0_GLB_IRQ_MASK	0x114
#define	ICU_CORE1_GLB_IRQ_MASK	0x1d8
#define	ICU_CORE2_GLB_IRQ_MASK	0x208
#define	ICU_CORE3_GLB_IRQ_MASK	0x238
#define	ICU_CORE0_GLB_FIQ_MASK	0x110
#define	ICU_CORE1_GLB_FIQ_MASK	0x190
#define	ICU_CORE2_GLB_FIQ_MASK	0x1f0
#define	ICU_CORE3_GLB_FIQ_MASK	0x220

#define ICU_MASK	0x1

/* CIU regsiter for release cores */
#define CIU_APCORE0_CFG_CTL			(0x0008)
#define CIU_APCORE1_CFG_CTL			(0x0088)
#define CIU_APCORE2_CFG_CTL			(0x00E4)
#define CIU_APCORE3_CFG_CTL			(0x00E8)

#define CIU_APCORE_CFG_64BIT			(0x1 << 31)
#define CIU_APCORE_CFG_NMFI_EN			(0x1 << 27)
#define CIU_APCORE_CFG_NIDEN			(0x1 << 26)
#define CIU_APCORE_CFG_SPNIDEN			(0x1 << 25)
#define CIU_APCORE_CFG_SPIDEN			(0x1 << 24)

extern void gic_raise_softirq(const struct cpumask *mask, unsigned int irq);

#ifndef __ASSEMBLER__

enum pxa1928_lowpower_state {
	POWER_MODE_CORE_INTIDLE,	/* used for C1 */
	POWER_MODE_CORE_POWERDOWN,	/* used for C2 */
	POWER_MODE_MP_POWERDOWN,	/* used for MP2 */
	POWER_MODE_APPS_IDLE,		/* used for D1P */
	POWER_MODE_APPS_SLEEP,		/* used for non-udr chip sleep, D1 */
	POWER_MODE_UDR_VCTCXO,		/* used for udr with vctcxo, D2 */
	POWER_MODE_UDR,			/* used for udr D2, suspend */
	POWER_MODE_MAX = 15,		/* maximum lowpower states */
};

#endif
#endif
