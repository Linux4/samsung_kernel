/*
 * ** (C) Copyright 2009 Marvell International Ltd.
 * **              All Rights Reserved
 *
 * ** This software file (the "File") is distributed by Marvell International Ltd.
 * ** under the terms of the GNU General Public License Version 2, June 1991 (the "License").
 * ** You may use, redistribute and/or modify this File in accordance with the terms and
 * ** conditions of the License, a copy of which is available along with the File in the
 * ** license.txt file or by writing to the Free Software Foundation, Inc., 59 Temple Place,
 * ** Suite 330, Boston, MA 02111-1307 or on the worldwide web at http://www.gnu.org/licenses/gpl.txt.
 * ** THE FILE IS DISTRIBUTED AS-IS, WITHOUT WARRANTY OF ANY KIND, AND THE IMPLIED WARRANTIES
 * ** OF MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE ARE EXPRESSLY DISCLAIMED.
 * ** The License provides additional details about this warranty disclaimer.
 * */

#ifndef __TIMER_A9_H__
#define __TIMER_A9_H__

#include <linux/version.h>
#include <mach/irqs.h>

#ifdef PX_SOC_PXA688

#if defined(LINUX_VERSION_CODE) && (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 28))
	#include <mach/addr-map.h>
	#define AP_TIMER_BASE(t)     (APB_VIRT_BASE + 0x00014000 + (0x66000 * (t-1))) /* the base address of AP Timer t */
	#define APB_CLOCK_UNIT_BASE  (APB_VIRT_BASE + 0X00015000)
	#define __PX_REG(x)          (*(volatile u32 *)(x))

#else
	#include <asm/arch/pxa910.h>
	#define AP_TIMER_BASE(t)     (0xd4000000 + 0x00014000 + (0x2000 * (t-1))) /* the base address of AP Timer t */
	#define APB_CLOCK_UNIT_BASE  (0xd4000000 + 0X00015000)
	#define __PX_REG(x)          __REG_PXA910(x)
#endif

#define TMR_CCR(t)          __PX_REG(AP_TIMER_BASE(t) + 0x0000)                 /* Timer Clock Control Register */
#define TMR_Tn_Mm(t, n, m)  __PX_REG(AP_TIMER_BASE(t) + 0x0004 + (n)*12+(m)*4)  /* Timer Match Registers */
#define TMR_CRn(t, n)       __PX_REG(AP_TIMER_BASE(t) + 0x0028 + (n)*4)         /* Timer Count Registers */
#define TMR_SRn(t, n)       __PX_REG(AP_TIMER_BASE(t) + 0x0034 + (n)*4)         /* Timer Status Registers */
#define TMR_IERn(t, n)      __PX_REG(AP_TIMER_BASE(t) + 0x0040 + (n)*4)         /* Timer Interrupt Enable Registers */
#define TMR_PLVRn(t, n)     __PX_REG(AP_TIMER_BASE(t) + 0x004c + (n)*4)         /* Timer Preload Value Registers */
#define TMR_PLCRn(t, n)     __PX_REG(AP_TIMER_BASE(t) + 0x0058 + (n)*4)         /* Timer Preload Control Registers */
#define TMR_ICRn(t, n)      __PX_REG(AP_TIMER_BASE(t) + 0x0074 + (n)*4)         /* Timer Interrupt Clear Registers */
#define TMR_CER(t)          __PX_REG(AP_TIMER_BASE(t) + 0x0084)                 /* Timer Count Enable Register */
#define TMR_CMR(t)          __PX_REG(AP_TIMER_BASE(t) + 0x0088)                 /* Timer Count Mode Register */
#define TMR_CVWRn(t, n)     __PX_REG(AP_TIMER_BASE(t) + 0x00A4 + (n)*4)         /* Timer Counters Value Write for Read Request */

#define APBC_TIMERS0_CLK_RST  __PX_REG(APB_CLOCK_UNIT_BASE + 0x0024)            /* Clock/Reset Control Register for Timers 0 */
//#define APBC_TIMERS1_CLK_RST  __PX_REG(APB_CLOCK_UNIT_BASE + 0x0044)            /* Clock Reset Control Register for Timers 1 */

#if defined(LINUX_VERSION_CODE) && (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 30))

#define PX_IRQ_MMP2_TIMER1_1 IRQ_MMP2_TIMER1
#define PX_IRQ_MMP2_TIMER1_2 IRQ_MMP2_TIMER2
#define PX_IRQ_MMP2_TIMER1_3 IRQ_MMP2_TIMER3
#define PX_IRQ_MMP2_TIMER2_1 IRQ_MMP2_PMU_TIMER1
#define PX_IRQ_MMP2_TIMER2_2 IRQ_MMP2_PMU_TIMER2
#define PX_IRQ_MMP2_TIMER2_3 IRQ_MMP2_PMU_TIMER3

#else

#define PX_IRQ_MMP2_TIMER1_1 IRQ_PXA688_TIMER1_1
#define PX_IRQ_MMP2_TIMER1_2 IRQ_PXA688_TIMER1_2
#define PX_IRQ_MMP2_TIMER1_3 IRQ_PXA688_TIMER1_1
#define PX_IRQ_MMP2_TIMER2_1 IRQ_PXA688_TIMER2_1
#define PX_IRQ_MMP2_TIMER2_2 IRQ_PXA688_TIMER2_2
#define PX_IRQ_MMP2_TIMER2_3 IRQ_PXA688_TIMER2_3

#endif
#endif /* PX_SOC_PXA688 */

#ifdef PX_SOC_PXA968
#define PX_IRQ_MG1_TIMER IRQ_OST_4_11
#endif /* PX_SOC_PXA968 */

#endif /* __TIMER_A9_H__ */
