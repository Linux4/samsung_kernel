/*
 * linux/arch/arm/mach-mmp/include/mach/pxa988_lowpower.h
 *
 * Author:	Raul Xiong <xjian@marvell.com>
 * Copyright:	(C) 2012 Marvell International Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */


#ifndef __MMP_MACH_PXA988_LOWPOWER_H__
#define __MMP_MACH_PXA988_LOWPOWER_H__

#define LPM_NUM				16
#define MAX_CPU_NUM			CONFIG_NR_CPUS
#define OFFSET_SCU_SHUTDOWN		(MAX_CPU_NUM << 2)
#define OFFSET_SPINLOCK			(OFFSET_SCU_SHUTDOWN + 4)

#define	LPM_C1				0
#define	LPM_C2				1
#define	LPM_D1P				2
#define	LPM_D1				3
#define	LPM_D2				4
#define	LPM_D2_UDR			5

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
#define PMUM_AP2_TIMER_3	(1 << 13)
#define PMUM_AP2_TIMER_2	(1 << 12)
#define PMUM_AP2_TIMER_1	(1 << 11)
#define PMUM_AP1_TIMER_3	(1 << 10)
#define PMUM_AP1_TIMER_2	(1 << 9)
#define PMUM_AP1_TIMER_1	(1 << 8)
#define PMUM_WAKEUP7		(1 << 7)
#define PMUM_WAKEUP6		(1 << 6)
#define PMUM_WAKEUP5		(1 << 5)
#define PMUM_WAKEUP4		(1 << 4)
#define PMUM_WAKEUP3		(1 << 3)
#define PMUM_WAKEUP2		(1 << 2)
#define PMUM_WAKEUP1		(1 << 1)
#define PMUM_WAKEUP0		(1 << 0)

#ifndef __ASSEMBLER__

enum pxa988_lowpower_state {
	POWER_MODE_ACTIVE = 0, /* not used for PXA988 */
	POWER_MODE_CORE_INTIDLE, /* used for C1 */
	POWER_MODE_CORE_EXTIDLE, /* not used for PXA988*/
	POWER_MODE_CORE_POWERDOWN, /* used for C2 */
	POWER_MODE_APPS_IDLE, /* used for D1P */
	POWER_MODE_APPS_SLEEP, /* not used now */
	POWER_MODE_SYS_SLEEP, /* used for non-udr chip sleep, D1 */
	POWER_MODE_UDR_VCTCXO, /* used for udr with vctcxo, D2 */
	POWER_MODE_UDR, /* used for udr D2, suspend */
};

enum pxa988_lowpower_mode {
	/* POWER_MODE_CORE_INTIDLE */
	PXA988_LPM_C1 = LPM_C1,
	/* POWER_MODE_CORE_POWERDOWN with L1 shutdown, L2 retentive */
	PXA988_LPM_C2 = LPM_C2,
	/* POWER_MODE_APPS_IDLE with L2 retentive */
	PXA988_LPM_D1P = LPM_D1P,
	/* POWER_MODE_SYS_SLEEP with L2 retentive */
	PXA988_LPM_D1 = LPM_D1,
	/* POWER_MODE_UDR_VCTCXO with L2 retentive */
	PXA988_LPM_D2 = LPM_D2,
	/* POWER_MODE_UDR with L2 shutdown */
	PXA988_LPM_D2_UDR = LPM_D2_UDR,
	/* Maximum LPM index, must be the last one! */
	PXA988_MAX_LPM_INDEX = 15,
};

struct pxa988_lowpower_data {
	u32 power_state;	/* SoC specific LPM states */
	/* Whether this LPM is valid according to the constraints */
	u32 valid;
};

extern struct pxa988_lowpower_data pxa988_lpm_data[];
extern void pxa988_hotplug_enter(u32 cpu, u32 power_mode);
extern void pxa988_pm_suspend(u32 cpu, u32 power_mode);
extern int pxa988_enter_lowpower(u32 cpu, u32 power_mode);
extern void pxa988_enter_c1(u32 cpu);

extern void pl310_suspend(void);

extern void pmu_register_lock(void);
extern void pmu_register_unlock(void);

extern void pl310_disable(void);

#ifdef CONFIG_EOF_FC_WORKAROUND
extern atomic_t disable_c2;
#endif

#if defined(CONFIG_CPU_PXA988) && defined(CONFIG_SMP)
extern atomic_t freqchg_disable_c2;
#endif

#ifdef CONFIG_SMP
extern int core1_c2;
#endif
#endif

#endif
