/*
 * linux/arch/arm/mach-mmp/include/mach/regs-apmu.h
 *
 *   Application Subsystem Power Management Unit
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __ASM_MACH_REGS_APMU_H
#define __ASM_MACH_REGS_APMU_H

#include <mach/addr-map.h>

#define APMU_VIRT_BASE	(AXI_VIRT_BASE + 0x82800)
#define APMU_REG(x)	(APMU_VIRT_BASE + (x))

/* Clock Reset Control */
#define APMU_CC_SP	APMU_REG(0x000)
#define APMU_CC_PJ	APMU_REG(0x004)
#define APMU_CC2_PJ	APMU_REG(0x150)
#define APMU_CC3_PJ	APMU_REG(0x188)
#define APMU_DM_CC_SP	APMU_REG(0x008)
#define APMU_DM_CC_PJ	APMU_REG(0x00c)
#define APMU_DM2_CC_PJ	APMU_REG(0x158)
#define APMU_SP_IDLE_CFG	APMU_REG(0x014)
#define APMU_IDLE_CFG	APMU_REG(0x018)

#define APMU_CCIC_GATE	APMU_REG(0x028)
#define APMU_CCIC2_GATE APMU_REG(0x118)
#define APMU_ISPDXO	APMU_REG(0x038)
#define APMU_DSI	APMU_REG(0x044)
#define APMU_IRE	APMU_REG(0x048)
#define APMU_LCD	APMU_REG(0x04c)
#define APMU_CCIC_RST	APMU_REG(0x050)
#define APMU_CCIC2_RST	APMU_REG(0x0f4)
#define APMU_SDH0	APMU_REG(0x054)
#define APMU_SDH1	APMU_REG(0x058)
#define APMU_USB	APMU_REG(0x05c)
#define APMU_NAND	APMU_REG(0x060)
#define APMU_DMA	APMU_REG(0x064)
#define APMU_GEU	APMU_REG(0x068)
#define APMU_BUS	APMU_REG(0x06c)
#define APMU_PWR_STBL_TIMER        APMU_REG(0x084)
#define APMU_CORE_STATUS APMU_REG(0x090)
#define APMU_CC2_AP	APMU_REG(0x100)
#define APMU_GC	APMU_REG(0x0cc)
#define APMU_GC_PD	APMU_REG(0x0d0)

#if defined(CONFIG_CPU_MMP2) || defined(CONFIG_CPU_MMP3)
#define APMU_SDH2	APMU_REG(0x0e8)
#define APMU_SDH3	APMU_REG(0x0ec)
#define APMU_SDH4	APMU_REG(0x15c)
#elif defined(CONFIG_CPU_PXA910) || defined(CONFIG_CPU_PXA988)
#define APMU_SDH2	APMU_REG(0x0e0)
#endif

/* pxa988 MCK4 AHB clock */
#if defined(CONFIG_CPU_PXA988)
#define APMU_MCK4_CTL	APMU_REG(0x0e8)
#endif

#define APMU_CCIC_DBG	APMU_REG(0x088)
#define APMU_CCIC2_DBG	APMU_REG(0x088)
#define APMU_SRAM_PWR_DWN	APMU_REG(0x08c)
#define APMU_VMETA	APMU_REG(0x0A4)
#define APMU_SMC	APMU_REG(0x0d4)

#define APMU_ISPPWR	APMU_REG(0x1FC)
#define APMU_ISPCLK	APMU_REG(0x224)
#define APMU_ISLD_CI_CTRL       APMU_REG(0x01E0)

#define APMU_CP_CCR             APMU_REG(0x0000)
#define APMU_CCR                APMU_REG(0x0004)
#define APMU_CP_CCSR            APMU_REG(0x0008)
#define APMU_CCSR               APMU_REG(0x000c)
#define APMU_SQU_CLK_GATE_CTRL	APMU_REG(0x001c)
#define APMU_LCD_CLK_RES_CTRL   APMU_REG(0x004c)
#define APMU_DEBUG              APMU_REG(0x0088)
#define APMU_IMR                APMU_REG(0x0098)
#define APMU_IRWC		APMU_REG(0x009c)
#define APMU_ISR                APMU_REG(0x00a0)
#define APMU_DX8_CLK_RES_CTRL   APMU_REG(0x00a4)
#define APMU_MC_HW_SLP_TYPE     APMU_REG(0x00b0)
#define APMU_PLL_SEL_STATUS     APMU_REG(0x00c4)
#define APMU_SMC_CLK_RES_CTRL   APMU_REG(0x00d4)
#define APMU_PWR_CTRL_REG		APMU_REG(0x00d8)
#define APMU_GC_CLK_RES_CTRL	APMU_REG(0x00cc)
#define APMU_PWR_BLK_TMR_REG	APMU_REG(0x00dc)
#define APMU_PWR_STATUS_REG     APMU_REG(0x00f0)

#define APMU_CC2R               APMU_REG(0x0100)
#define APMU_CC2SR              APMU_REG(0x0104)
#define APMU_TRACE              APMU_REG(0x0108)

#define APMU_SLIM_CLK_RES_CTRL	APMU_REG(0x0104)
#define APMU_FSIC3_CLK_RES_CTRL APMU_REG(0x0100)
#define APMU_LCD2_CLK_RES_CTRL  APMU_REG(0x0110)

/* coda7542 */
#define APMU_VPU_CLK_RES_CTRL	APMU_REG(0x00a4)

/* CNM clock and power on/off register*/
#define APMU_DX8_CLK_RES_CTRL   APMU_REG(0x00a4)
#define APMU_PWR_CTRL_REG       APMU_REG(0x00d8)
#define APMU_PWR_BLK_TMR_REG    APMU_REG(0x00dc)
#define APMU_PWR_STATUS_REG     APMU_REG(0x00f0)

#define APMU_ISLD_CI_PDWN_CTRL		APMU_REG(0x01e0)
#define APMU_ISLD_DSPA_PDWN_CTRL	APMU_REG(0x01e4)
#define APMU_ISLD_BCM_PDWN_CTRL		APMU_REG(0x01e8)
#define APMU_ISLD_LCDMIPI_PDWN_CTRL	APMU_REG(0x01ec)
#define APMU_ISLD_VMETA_PDWN_CTRL	APMU_REG(0x01f0)
#define APMU_ISLD_GC2000_PDWN_CTRL	APMU_REG(0x01f4)
#define APMU_ISLD_CPUMC_PDWN_CTRL	APMU_REG(0x01f8)

#define APMU_ISLD_CMEM_CLKGATE_BYPASS	(1u << 5)
#define APMU_ISLD_CMEM_DMMYCLK_EN	(1u << 4)

#define APMU_MC_PAR_CTRL		APMU_REG(0x011c)

/* Debug register */
#define APMU_DEBUG		APMU_REG(0x0088)
#define APMU_DEBUG2		APMU_REG(0x0190)

#define APMU_FNCLK_EN	(1 << 4)
#define APMU_AXICLK_EN	(1 << 3)
#define APMU_FNRST_DIS	(1 << 1)
#define APMU_AXIRST_DIS	(1 << 0)

/* Wake Clear Register */
#define APMU_WAKE_CLR	APMU_REG(0x07c)

#define APMU_PXA168_KP_WAKE_CLR		(1 << 7)
#define APMU_PXA168_CFI_WAKE_CLR	(1 << 6)
#define APMU_PXA168_XD_WAKE_CLR		(1 << 5)
#define APMU_PXA168_MSP_WAKE_CLR	(1 << 4)
#define APMU_PXA168_SD4_WAKE_CLR	(1 << 3)
#define APMU_PXA168_SD3_WAKE_CLR	(1 << 2)
#define APMU_PXA168_SD2_WAKE_CLR	(1 << 1)
#define APMU_PXA168_SD1_WAKE_CLR	(1 << 0)

#define APMU_PXA910_KP_WAKE_CLR		(1 << 3)
#define APMU_PXA988_KP_WAKE_CLR		(1 << 3)
#define APMU_PXA988_SD3_WAKE_CLR	(1 << 6)
#define APMU_PXA988_SD2_WAKE_CLR	(1 << 1)
#define APMU_PXA988_SD1_WAKE_CLR	(1 << 0)

#define APMU_GC_156M		0x0
#define APMU_GC_312M		0x40
#define APMU_GC_PLL2		0x80
#define APMU_GC_PLL2_DIV2	0xc0
#define APMU_GC_624M		0xc0 /* added according to Aspen SW spec v2.8*/

#define APMU_VMETA_CLK_RES_CTRL	APMU_VMETA
/* VMeta Technology Power Mode */
#define APMU_VMETA_CLK_DIV_MASK			(0xF << 16)
#define APMU_VMETA_CLK_DIV_1			(0x1 << 16)
#define APMU_VMETA_CLK_DIV_2                    (0x2 << 16)
#define APMU_VMETA_CLK_DIV_4                    (0x4 << 16)
#define APMU_VMETA_CLK_DIV_8                    (0x8 << 16)
#define APMU_VMETA_CLK_DIV_SHIFT		16
#define APMU_VMETA_CLK_PLL2			(0x1 << 6)
#define APMU_VMETA_CLK_SEL_SHIFT		6
/* VMeta Technology Power Up */
#define APMU_VMETA_PWRUP_ON			(3 << 9)
#define APMU_VMETA_PWRUP_SLOW_RAMP		(1 << 9)
#define APMU_VMETA_PWRUP_MASK			(3 << 9)
/* VMeta Technology Isolation Enable */
#define APMU_VMETA_ISB				(1 << 8)
/* VMeta Technology Clock Select */
#define APMU_VMETA_CLK_SEL_MASK			(3 << 6)
/* VMeta Technology Peripheral Clock Enable */
#define APMU_VMETA_CLK_EN			(1 << 4)
/* VMeta Technology AXI Clock Enable */
#define APMU_VMETA_AXICLK_EN			(1 << 3)
/* VMeta Technology Memory Redundancy Repair Start */
#define APMU_VMETA_REDUN_START			(1 << 2)
/* VMeta Technology Peripheral Reset 1 */
#define APMU_VMETA_RST				(1 << 1)
/* VMeta Technology AXI Reset */
#define APMU_VMETA_AXI_RST			(1 << 0)
#define APMU_VMETA_CLK_RES_CTRL_DFT		(APMU_VMETA_CLK_DIV_4)

/* VMeta Technology Clock/Reset Control Register For MMP2 */
/* [31:16]	Reserved */
/* [15]		Bus Clock Source */
#define APMU_MMP2_VMETA_ACLK_SRC		(1 << 15)
/* [14:12]	Clock Select */
#define APMU_MMP2_VMETA_ACLK_SEL_MASK	(7 << 12)
#define APMU_MMP2_VMETA_ACLK_SEL_SHIFT	12
/* [11]		Power Mode */
#define APMU_MMP2_VMETA_PWR_CTRL		(1 << 11)
/* [10]		Power Up */
#define APMU_MMP2_VMETA_PWRUP			(1 << 10)
/* [9]		Input Isolation Enable */
#define APMU_MMP2_VMETA_INP_ISB			(1 << 9)
/* [8]		Isolation Enable */
#define APMU_MMP2_VMETA_ISB				(1 << 8)
/* [7:5]	Clock Select */
#define APMU_MMP2_VMETA_CLK_SEL_MASK	(7 << 5)
#define APMU_MMP2_VMETA_CLK_SEL_SHIFT	5
/* [4]		Peripheral Clock Enable */
#define APMU_MMP2_VMETA_CLK_EN			(1 << 4)
/* [3]		AXI Clock Enable */
#define APMU_MMP2_VMETA_AXICLK_EN		(1 << 3)
/* [2]		Reserved */
/* [1]		Peripheral Reset 1 */
#define APMU_MMP2_VMETA_RST1	(1 << 1)
/* [0]		AXI Reset */
#define APMU_MMP2_VMETA_AXI_RST	(1 << 0)

/* USB HSIC/FSIC*/
#define APMU_USBHSIC1   APMU_REG(0x0f8)
#define APMU_USBHSIC2   APMU_REG(0x0fc)
#define APMU_USBFSIC    APMU_REG(0x100)

/* Audio Island */
#define APMU_AUDIO_CLK_RES_CTRL		APMU_REG(0x010c)
#define APMU_AUDIO_DSA			APMU_REG(0x0164)
#define APMU_ISLD_DSPA_CTRL		APMU_REG(0x01e4)
#define APMU_AUDIO_SRAM_PWR		APMU_REG(0x0240)

#define APMU_PJ_C0_CC4			APMU_REG(0x0248)
#define APMU_PJ_C1_CC4			APMU_REG(0x024C)
#define APMU_PJ_C2_CC4			APMU_REG(0x0250)

/* PXA988 CA9 core idle configuration */
#define PMU_CA9_CORE0_IDLE_CFG		APMU_REG(0x0124)
#define PMU_CA9_CORE1_IDLE_CFG		APMU_REG(0x0128)
#define APMU_COREn_WAKEUP_CTL(n)	(APMU_REG(0x012C) + 4 * (n & 0x3))
#define APMU_WAKEUP_CORE(n)		(1 << (n & 0x3))

/* PXA988 CA9 MP idle configuration */
#define PMU_CA9MP_IDLE_CFG0		APMU_REG(0x0120)
#define PMU_CA9MP_IDLE_CFG1		APMU_REG(0x00e4)

/* PXA988 AP Clock Control Register2 */
#define PMU_CC2_AP			APMU_REG(0x0100)

#define APMU_AUDIO_PWR_UP		(3 << 9)
#define APMU_AUDIO_PWR_DOWN		(0 << 9)
#define APMU_AUDIO_ISO_DIS		(1 << 8)
#define APMU_AUDIO_CLK_SEL_PLL1_DIV_2	(0 << 6)
#define APMU_AUDIO_CLK_SEL_PLL2_DIV_2	(1 << 6)
#define APMU_AUDIO_CLK_SEL_PLL2_DIV_3	(2 << 6)
#define APMU_AUDIO_CLK_SEL_PLL1_DIV_8	(3 << 6)
#define APMU_AUDIO_CLK_ENA		(1 << 4)
#define APMU_AUDIO_RST_DIS		(1 << 1)

#endif /* __ASM_MACH_REGS_APMU_H */
