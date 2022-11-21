/*
 * linux/arch/arm/mach-mmp/include/mach/regs-mpmu.h
 *
 *   Main Power Management Unit
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __ASM_MACH_REGS_MPMU_H
#define __ASM_MACH_REGS_MPMU_H

#include <mach/addr-map.h>

#define MPMU_CPCR	MPMU_REG(0x0000)
#define MPMU_CPSR	MPMU_REG(0x0004)
#define MPMU_FCCR	MPMU_REG(0x0008)
#define MPMU_POCR	MPMU_REG(0x000c)
#define MPMU_POSR	MPMU_REG(0x0010)
#define MPMU_SUCCR	MPMU_REG(0x0014)
#define MPMU_VRCR	MPMU_REG(0x0018)
#define MPMU_OHCR	MPMU_REG(0x001c)
#define MPMU_CPRR	MPMU_REG(0x0020)
#define MPMU_CCGR	MPMU_REG(0x0024)
#define MPMU_GPCR	MPMU_REG(0x0030)
#define MPMU_PLL2CR	MPMU_REG(0x0034)
#define MPMU_PLL3CR	MPMU_REG(0x001c)
#define MPMU_SCCR	MPMU_REG(0x0038)
#define MPMU_ISCCRX0	MPMU_REG(0x0040)
#define MPMU_ISCCRX1	MPMU_REG(0x0044)
#define MPMU_CWUCRM	MPMU_REG(0x004c)
#define MPMU_PLL1_REG1	MPMU_REG(0x0050)
#define MPMU_PLL1_REG2	MPMU_REG(0x0054)
#define MPMU_PLL1_SSC	MPMU_REG(0x0058)
#define MPMU_PLL2_REG1	MPMU_REG(0x0060)
#define MPMU_PLL2_REG2	MPMU_REG(0x0064)
#define MPMU_PLL2_SSC	MPMU_REG(0x0068)
#define MPMU_SD_ROT_WAKE_CLR	MPMU_REG(0x007c)
#define MPMU_PLL2_CTRL1 MPMU_REG(0x0414)
#define MPMU_PLL2_CTRL2 MPMU_REG(0x0418)

#if defined(CONFIG_CPU_MMP3)
#define MPMU_PLL1_CTRL  MPMU_REG(0x005c)    /* MMP3 PLL1 control reg */
#else
#define MPMU_PLL1_CTRL  MPMU_REG(0x0418)    /* MMP2 PLL1 control reg */
#endif

#define MPMU_TS		MPMU_REG(0x0080)
#define MPMU_WDTPCR	MPMU_REG(0x0200)
#define MPMU_APCR	MPMU_REG(0x1000)
#define MPMU_APSR	MPMU_REG(0x1004)
#define MPMU_APRR	MPMU_REG(0x1020)
#define MPMU_ACGR	MPMU_REG(0x1024)
#define MPMU_ARSR	MPMU_REG(0x1028)
#define MPMU_AWUCRS	MPMU_REG(0x1048)
#define MPMU_AWUCRM	MPMU_REG(0x104c)
#define MPMU_HSI_CLK_RES_CTRL MPMU_REG(0x1050)

/*MMP3 PLL3 registers*/
#define PMUM_PLL3_CR	MPMU_REG(0x0050)
#define PMUM_POSR2	MPMU_REG(0x0054)
#define PMUM_PLL3_CTRL1	MPMU_REG(0x0058)
#define PMUM_PLL3_CTRL2	MPMU_REG(0x0060)
#define PMUM_PLL3_CTRL3	MPMU_REG(0x0064)

#define PMUM_PLL_DIFF_CTRL	MPMU_REG(0x0068)
#define PMUM_DVC_AP		MPMU_REG(0x2020)
#define PMUM_DVC_CP		MPMU_REG(0x2024)
#define PMUM_DVC_DP		MPMU_REG(0x2028)
#define PMUM_DVC_APSUB		MPMU_REG(0x202C)
#define PMUM_DVC_CHIP		MPMU_REG(0x2030)
#define PMUM_DVC_STATUS		MPMU_REG(0x2040)
#define PMUM_DVCR		MPMU_REG(0x2000)
#define PMUM_VL01STR		MPMU_REG(0x2004)
#define PMUM_VL12STR		MPMU_REG(0x2008)
#define PMUM_VL23STR		MPMU_REG(0x200c)
#define PMUM_VL34STR		MPMU_REG(0x2010)
#define PMUM_VL45STR		MPMU_REG(0x2014)
#define PMUM_VL56STR		MPMU_REG(0x2018)
#define PMUM_VL67STR		MPMU_REG(0x201c)
#define PMUM_DVC_IMR		MPMU_REG(0x2050)
#define PMUM_DVC_ISR		MPMU_REG(0x2054)

#define DVC_AP_LPM_AVC_EN	(1 << 3)
#define DVC_CP_LPM_AVC_EN	(1 << 3)
#define DVC_DP_LPM_AVC_EN	(1 << 3)
#define UDR_AP_SLP_AVC_EN	(1 << 15)
#define nUDR_AP_SLP_AVC_EN	(1 << 11)
#define AP_IDLE_DDROFF_AVC_EN	(1 << 7)
#define AP_IDLE_DDRON_AVC_EN	(1 << 3)
#define UDR_SLP_AVC_EN		(1 << 7)
#define nUDR_SLP_AVC_EN		(1 << 3)
#define DVCR_VC_EN		(1 << 1)
#define DVCR_LPM_AVC_EN		(1 << 0)
#define VLXX_ST_MASK		(0xFFFF)
#define AP_VC_DONE_INTR_MASK	(1 << 0)
#define CP_VC_DONE_INTR_MASK	(1 << 1)
#define DP_VC_DONE_INTR_MASK	(1 << 2)
#define AP_VC_DONE_INTR_ISR	(1 << 0)
#define CP_VC_DONE_INTR_ISR	(1 << 1)
#define DP_VC_DONE_INTR_ISR	(1 << 2)




#endif /* __ASM_MACH_REGS_APMU_H */
