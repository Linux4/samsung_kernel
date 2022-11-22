/*
 * arch/arm/mach-pxa/include/mach/pxa3xx-regs.h
 *
 * PXA3xx specific register definitions
 *
 * Copyright (C) 2007 Marvell International Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __ASM_ARCH_PXA3XX_REGS_H
#define __ASM_ARCH_PXA3XX_REGS_H

#include <mach/hardware.h>

/*
 * Static Chip Selects
 */

#define PXA95X_CS0_PHYS		(0x00000000)
#define PXA95X_CS1_PHYS		(0x30000000)
#define PXA95X_CS2_PHYS		(0x10000000)
#define PXA95X_CS3_PHYS		(0x14000000)

/*
 * Oscillator Configuration Register (OSCC)
 */
#define OSCC           __REG(0x41350000)  /* Oscillator Configuration Register */

#define OSCC_VCTVSTB_OFFSET (20)
#define OSCC_VCTVCEN	(1 << 23)
#define OSCC_VCTVSTB	(1 << OSCC_VCTVSTB_OFFSET)
#define OSCC_DPLS	(1 << 17)
#define OSCC_PEN	(1 << 11)		/* 13MHz POUT */
#define OSCC_TENS3	(1 << 10)
#define OSCC_TENS2	(1 << 9)
#define OSCC_TENS0	(1 << 8)

/*
 * Main Clock Control Unit Registers
 */
#define DMEMVLR		__REG(0x4135000C)	/* DMC Voltage Level Configuration Register */

#define DMEMVLR_DMCHV_OFFSET (0)

/*
 * Service Power Management Unit (MPMU)
 */
#define PMCR		__REG(0x40F50000)	/* Power Manager Control Register */
#define PSR		__REG(0x40F50004)	/* Power Manager S2 Status Register */
#define PSPR		__REG(0x40F50008)	/* Power Manager Scratch Pad Register */
#define PCFR		__REG(0x40F5000C)	/* Power Manager General Configuration Register */
#define PWER		__REG(0x40F50010)	/* Power Manager Wake-up Enable Register */
#define PWSR		__REG(0x40F50014)	/* Power Manager Wake-up Status Register */
#define PECR		__REG(0x40F50018)	/* Power Manager EXT_WAKEUP[1:0] Control Register */
#define OVH		__REG(0x40F50020)	/* Overheating Control Register */
#define VLSCR		__REG(0x40F5005C)	/* Voltage Level Select Control Register */
#define DCDCSR		__REG(0x40F50080)	/* DC-DC Controller Status Register */
#define SDCR		__REG(0x40F5008C)	/* SRAM State-retentive Control Register */
#define AVCR		__REG(0x40F50094)	/* VCC_MAIN Voltage Control Register */
#define SVCR		__REG(0x40F50098)	/* VCC_SRAM Voltage Control Register */
#define PSBR		__REG(0x40F500A0)	/* Power Manager Safty Bits Register */
#define PVCR		__REG(0x40F50100)	/* Power Manager Voltage Change Control Register */
#define PCMD(x)		__REG(0x40F50110 + ((x) << 2))

#define PMCR_BIE	(1 << 0)		/* Interrupt Enable for nBATT_FAULT */
#define PMCR_BIS	(1 << 1)		/* Interrupt Status for nBATT_FAULT */
#define PMCR_CSMRIE	(1 << 4)		/* Interrupt Enable for Communication Subsystem Memory Request*/
#define PMCR_CSMRIS	(1 << 5)		/* Interrupt Status for Communication Subsystem Memory Request*/
#define PMCR_CSWDIE	(1 << 6)		/* Interrupt Enable for Communication Subsystem Watchdog Reset*/
#define PMCR_CSWDIS	(1 << 7)		/* Interrupt Status for Communication Subsystem Watchdog Reset*/
#define PMCR_CSIE	(1 << 8)		/* Interrupt Enable for Communication Subsystem D4 Entry Complete */
#define PMCR_CSIS	(1 << 9)		/* Interrupt Status for Communication Subsystem D4 Entry Complete */
#define PMCR_TIE	(1 << 10)		/* Interrupt Enable for XScale Core Frequency Change */
#define PMCR_TIS	(1 << 11)		/* Interrupt Status for XScale Core Frequency Change */
#define PMCR_VIE	(1 << 12)		/* Interrupt Enable for VCC_APPS and VCC_SRAM Voltage Change */
#define PMCR_VIS	(1 << 13)		/* Interrupt Status for VCC_APPS and VCC_SRAM Voltage Change */
#define PMCR_SWGR	(1 << 31)		/* Software GPIO Reset */
#define PMCR_STATUS_BITS (PMCR_TIS|PMCR_CSIS|PMCR_CSWDIS|PMCR_CSMRIS|PMCR_BIS)
#define PMCR_RSVD_CLR_ALWAYS_MASK	(0x7FFFF03F)
#define PSR_TSS_OFF	(12)
#define OVH_TEMP_EN	(1 << 0)		/* Enable for Temperature Sensor */
#define VLSCR_VLT_LVL_SEL_EN	(1 << 8)
#define VLSCR_LVL0_SINGLE_RAIL	(1 << 9)
#define VLSCR_LVL1_SINGLE_RAIL	(1 << 10)
#define VLSCR_LVL2_SINGLE_RAIL	(1 << 11)
#define VLSCR_LVL3_SINGLE_RAIL	(1 << 12)
#define VLSCR_LPM_SINGLE_RAIL	(1 << 13)
#define VLSCR_VCT0_LVL0_REMAP_MASK	(3 << 16)
#define VLSCR_VCT0_LVL1_REMAP_MASK	(3 << 18)
#define VLSCR_VCT0_LVL2_REMAP_MASK	(3 << 20)
#define VLSCR_VCT0_LVL3_REMAP_MASK	(3 << 22)

#ifdef CONFIG_CPU_PXA978
#define OVH_OWM		(1 << 19)		/* Over-heating WDT Enable */
#define OVH_OVWF_OFF	(10)			/* WDT Reset Temperature Over-heating Threshold */
#else
#define OVH_OWM		(1 << 7)		/* Over-heating WDT Enable */
#define OVH_OVWF_OFF	(4)			/* WDT Reset Temperature Over-heating Threshold */
#endif
#define OVH_OTIF_OFF	(1)			/* Over-heating Treshold Value for Generating TIS Software Interrupt */
#define PVCR_VCSA	(1 << 14)

#define AVCR_ALVL3_OFFSET		24
#define AVCR_ALVL3_MASK_5bit		(0x1F << AVCR_ALVL3_OFFSET)
#define AVCR_ALVL3_MASK			(0xFF << AVCR_ALVL3_OFFSET)
#define CORE_OVERHEATING_DETECTED	1
#define CORE_COLLING_DETECTED		0

/*
 * Slave Power Management Unit
 */
#define ASCR		__REG(0x40f40000)	/* Application Subsystem Power Status/Configuration */
#define ARSR		__REG(0x40f40004)	/* Application Subsystem Reset Status */
#define AD3ER		__REG(0x40f40008)	/* Application Subsystem Wake-Up from D3 Enable */
#define AD3SR		__REG(0x40f4000c)	/* Application Subsystem Wake-Up from D3 Status */
#define AD2D0ER		__REG(0x40f40010)	/* Application Subsystem Wake-Up from D2 to D0 Enable */
#define AD2D0SR		__REG(0x40f40014)	/* Application Subsystem Wake-Up from D2 to D0 Status */
#define AD2D1ER		__REG(0x40f40018)	/* Application Subsystem Wake-Up from D2 to D1 Enable */
#define AD2D1SR		__REG(0x40f4001c)	/* Application Subsystem Wake-Up from D2 to D1 Status */
#define AD1D0ER		__REG(0x40f40020)	/* Application Subsystem Wake-Up from D1 to D0 Enable */
#define AD1D0SR		__REG(0x40f40024)	/* Application Subsystem Wake-Up from D1 to D0 Status */
#define AGENP		__REG(0x40f4002c)	/* Application Subsystem General Purpose */
#define AD3R		__REG(0x40f40030)	/* Application Subsystem D3 Configuration */
#define AD2R		__REG(0x40f40034)	/* Application Subsystem D2 Configuration */
#define AD1R		__REG(0x40f40038)	/* Application Subsystem D1 Configuration */
#define ACGD0ER		__REG(0x40F40040)	/* Application Subsystem CG to D0 state Wakeup Enable Register */
#define ACGD0SR		__REG(0x40F40044)	/* Application Subsystem CG to D0 state Wakeup Status Register */
#define ACGD0ER2	__REG(0x40F40048)	/* Application Subsystem CG to D0 state Wakeup Enable Register 2 */
#define ACGD0SR2	__REG(0x40F4004C)	/* Application Subsystem CG to D0 state Wakeup Status Register 2 */
#define PWRMODE		__REG(0x40F40080)	/* Application Subsystem Power Mode Register */
#define CPUPWR		__REG(0x40F40084)	/* Application Subsystem CPU Power Mode Register */
#define VMPWR		__REG(0x40F40090)	/* Application Subsystem VMeta Power Mode Register */
#define GCPWR		__REG(0x40F40094)	/* Application Subsystem GCU Power Mode Register */
#define PERI_PLL_CTRL	__REG(0x40F400B0)	/* Peripheral PLL Control Register */
#define PERI_PLL_PARAM	__REG(0x40F400B4)	/* Peripheral PLL Parameters Register */
#define MM_PLL_CTRL	__REG(0x40F400BC)	/* Multi-media PLL Control Register */
#define MM_PLL_PARAM	__REG(0x40F400C0)	/* Multi-media PLL Parameters Register */
#define TMP_CTRL	__REG(0x40F400C8)	/* Temperature Sensor Control Register */
#define AVLCR		__REG(0x40F400A8)	/* Application Subsystem Voltage Level Change Register */
#define AVLSR		__REG(0x40F400AC)	/* Application Subsystem Voltage Level Status Register */
#define SYS_PLL_416M_CTRL	__REG(0x40F400D8)	/* System PLL 416MHz Branch Apps Control Register */

#define VMPWR_PWON		(1 << 0)
#define VMPWR_PWR_ST		(1 << 2)
#define VMPWR_SETALLWAYS	(0xFF00)
#define GCPWR_PWON		(1 << 0)
#define GCPWR_RST_N		(1 << 1)
#define GCPWR_PWR_ST		(1 << 2)
#define GCPWR_SETALLWAYS	(0xFF00)

#define PERIPLL_PWRON		(1 << 0)
#define PERIPLL_PWR_ST		(1 << 8)
#define PERIPLL_VCODIV_SEL_MASK	(0xf << 20)	/* Post Divider */
#define PERIPLL_KVCO_MASK	(0xf << 16)	/* PERI PLL KVCO Value Configuratioin */
#define PERIPLL_FBDIV_MASK	(0x1ff << 5)	/* PERI PLL FBDIV Value Configuration */
#define PERIPLL_REFDIV_MASK	(0x1f << 0)	/* PERI PLL REFDIV Value Configuration */

#define MMPLL_PWRON		(1 << 0)
#define MMPLL_PWR_ST		(1 << 8)
#define MMPLL_VCODIV_SEL_MASK	(0xf << 20)	/* Post Divider */
#define MMPLL_KVCO_MASK		(0xf << 16)	/* MM PLL KVCO Value Configuratioin */
#define MMPLL_FBDIV_MASK	(0x1ff << 5)	/* MM PLL FBDIV Value Configuration */
#define MMPLL_REFDIV_MASK	(0x1f << 0)	/* MM PLL REFDIV Value Configuration */

#define CLK_EN			(1 << 0)	/* System PLL 416Mhz branch enable*/
/*
 * Application Subsystem Configuration bits.
 */
#define ASCR_RDH		(1 << 31)
#define ASCR_D1S		(1 << 2)
#define ASCR_D2S		(1 << 1)
#define ASCR_D3S		(1 << 0)

/*
 * Application Reset Status bits.
 */
#define ARSR_GPR		(1 << 3)
#define ARSR_LPMR		(1 << 2)
#define ARSR_WDT		(1 << 1)
#define ARSR_HWR		(1 << 0)

/*
 * Application Subsystem Wake-Up bits.
 */
#define ADXER_WRTC		(1 << 31)	/* RTC */
#define ADXER_WOST		(1 << 30)	/* OS Timer */
#define ADXER_WTSI		(1 << 29)	/* Touchscreen */
#define ADXER_WUSBH		(1 << 28)	/* USB host */
#define ADXER_WUSB2		(1 << 26)	/* USB client 2.0 */
#define ADXER_WMSL0		(1 << 24)	/* MSL port 0*/
#define ADXER_WDMUX3		(1 << 23)	/* USB EDMUX3 */
#define ADXER_WDMUX2		(1 << 22)	/* USB EDMUX2 */
#define ADXER_WKP		(1 << 21)	/* Keypad */
#define ADXER_WUSIM1		(1 << 20)	/* USIM Port 1 */
#define ADXER_WUSIM0		(1 << 19)	/* USIM Port 0 */
#define ADXER_WOTG		(1 << 16)	/* USBOTG input */
#define ADXER_MFP_WFLASH	(1 << 15)	/* MFP: Data flash busy */
#define ADXER_MFP_GEN12		(1 << 14)	/* MFP: MMC3/GPIO/OST inputs */
#define ADXER_MFP_WMMC2		(1 << 13)	/* MFP: MMC2 */
#define ADXER_MFP_WMMC1		(1 << 12)	/* MFP: MMC1 */
#define ADXER_MFP_WI2C		(1 << 11)	/* MFP: I2C */
#define ADXER_MFP_WSSP4		(1 << 10)	/* MFP: SSP4 */
#define ADXER_MFP_WSSP3		(1 << 9)	/* MFP: SSP3 */
#define ADXER_MFP_WMAXTRIX	(1 << 8)	/* MFP: matrix keypad */
#define ADXER_MFP_WUART3	(1 << 7)	/* MFP: UART3 */
#define ADXER_MFP_WUART2	(1 << 6)	/* MFP: UART2 */
#define ADXER_MFP_WUART1	(1 << 5)	/* MFP: UART1 */
#define ADXER_MFP_WSSP2		(1 << 4)	/* MFP: SSP2 */
#define ADXER_MFP_WSSP1		(1 << 3)	/* MFP: SSP1 */
#define ADXER_MFP_WAC97		(1 << 2)	/* MFP: AC97 */
#define ADXER_WEXTWAKE1		(1 << 1)	/* External Wake 1 */
#define ADXER_WEXTWAKE0		(1 << 0)	/* External Wake 0 */

/*
 * AD3R/AD2R/AD1R bits.  R2-R5 are only defined for PXA320.
 */
#define ADXR_L2			(1 << 8)
#define ADXR_R5			(1 << 5)
#define ADXR_R4			(1 << 4)
#define ADXR_R3			(1 << 3)
#define ADXR_R2			(1 << 2)
#define ADXR_R1			(1 << 1)
#define ADXR_R0			(1 << 0)

/*
 * Values for PWRMODE CP15 register
 */
#define PXA3xx_PM_S3D4C4	0x07	/* aka deep sleep */
#define PXA3xx_PM_S2D3C4	0x06	/* aka sleep */
#define PXA3xx_PM_S0D2C2	0x03	/* aka standby */
#define PXA3xx_PM_S0D1C2	0x02	/* aka LCD refresh */
#define PXA3xx_PM_S0D0C1	0x01

/*
 * Application Subsystem Clock
 */
#define ACCR		__REG(0x41340000)	/* Application Subsystem Clock Configuration Register */
#define ACSR		__REG(0x41340004)	/* Application Subsystem Clock Status Register */
#define AICSR		__REG(0x41340008)	/* Application Subsystem Interrupt Control/Status Register */
#define CKENA		__REG(0x4134000C)	/* A Clock Enable Register */
#define CKENB		__REG(0x41340010)	/* B Clock Enable Register */
#define AC97_DIV	__REG(0x41340014)	/* AC97 clock divisor value register */
#define ACCR1		__REG(0x41340020)	/* Application Subsystem Clock Configuration Register 1 */
#define CKENC		__REG(0x41340024)	/* C Clock Enable Register */
#define DDR_CLK_PROFILES __REG(0x41340028)	/* DDR Clock Profiles Register */
#define DDR_FC_REG_TBL	__REG(0x41340028)	/* DDR Frequency Change Register Table Register */
#define DDR_FC_CTRL	__REG(0x4134002C)	/* DDR Frequency Change Control Register */
#define DDR_SC_CTRL1	__REG(0x4134002C)	/* DDR Sequence Control 1 Register */
#define CCLKCFG		__REG(0x41340040)	/* Core Clock Configuration Register */
#define ACCR0		__REG(0x41340050)	/* Application Subsystem Clock Configuration Register 0 */
#define ACSR0		__REG(0x41340054)	/* Application Subsystem Clock Status Register 0 */
#define COREPLLR	__REG(0x41340058)	/* Core PLL Cnfiguration Register */
#define FRQ_CHANGE_CTL	__REG(0x4134005C)	/* Apps Core Frequency Change Control Register */
#define FRQ_CHANGE_ST	__REG(0x41340060)	/* Apps Core Frequency Change Status Register */
#define COREPLL_TIMERS	__REG(0x41340064)	/* Core PLL Times Register */

#define DDRPLLR		__REG(0x41350004)	/* DDR PLL Cnfiguration Register */

#define ACCR_XPDIS		(1 << 31)	/* Core PLL Output Disable */
#define ACCR_SPDIS		(1 << 30)	/* System PLL Output Disable */
#define ACCR_D0CS		(1 << 26)	/* D0 Mode Clock Select */
#define ACCR_PCCE		(1 << 11)	/* Power Mode Change Clock Enable */
#define ACCR_DDR_D0CS		(1 << 7)	/* DDR SDRAM clock frequency in D0CS (PXA31x only) */
#define ACCR_DMCFS_312		(1 << 6)	/* DDR SDRAM clock frequency 312MHz */

#define ACCR_XPDIS_MASK		(0x1 << 31)	/* Core PLL Output Disable */
#define ACCR_SPDIS_MASK		(0x1 << 30)	/* System PLL Output Disable */
#define ACCR_AXIFS_MASK		(0x3 << 28)	/* AXI Bus Frequency Select */
#define ACCR_SMCFS_MASK		(0x7 << 23)	/* Static Memory Controller Frequency Select */
#define ACCR_GCFS_MASK		(0x3 << 20)	/* Graphics Controller Frequency Select */
#define ACCR_SFLFS_MASK		(0x3 << 18)	/* Frequency Select for Internal Memory Controller */
#define ACCR_XSPCLK_MASK	(0x3 << 16)	/* Core Frequency during Frequency Change */
#define ACCR_HSS_MASK		(0x3 << 14)	/* System Bus-Clock Frequency Select */
#define ACCR_DMCFS_MASK_978	(0x7 << 11)	/* Dynamic Memory Controller Clock Frequency Select */
#define ACCR_DMCFS_MASK		(0x3 << 12)	/* Dynamic Memory Controller Clock Frequency Select */
#define ACCR_XN_MASK		(0x7 << 8)	/* Core PLL Turbo-Mode-to-Run-Mode Ratio */
#define ACCR_DMCFS_312_MASK	(0x1 << 6)	/* DMC PLL Select */
#ifdef CONFIG_CPU_PXA3XX
#define ACCR_XL_MASK		(0x1f)		/* Core PLL Run-Mode-to-Oscillator Ratio */
#else
#define ACCR_XL_MASK		(0x3f)		/* Core PLL Run-Mode-to-Oscillator Ratio */
#endif
#define ACCR_RESERVED_MASK_978	(0xC47307FF)		/* ACCR Reserved bits mask in Nevo C0*/


#define ACCR_AXI(x)		(((x) & 0x3) << 28)
#define ACCR_SMCFS(x)		(((x) & 0x7) << 23)
#define ACCR_GCFS(x)		(((x) & 0x3) << 20)
#define ACCR_SFLFS(x)		(((x) & 0x3) << 18)
#define ACCR_XSPCLK(x)		(((x) & 0x3) << 16)
#define ACCR_HSS(x)		(((x) & 0x3) << 14)
#define ACCR_DMCFS_978(x)	(((x) & 0x7) << 11)
#define ACCR_DMCFS(x)		(((x) & 0x3) << 12)
#define ACCR_XN(x)		(((x) & 0x7) << 8)
#define ACCR_XL(x)		((x) & ACCR_XL_MASK)

#define ACCR0_DCFS(x)		(((x) & 0x7) << 0)
#define ACCR0_VMFS(x)		(((x) & 0x7) << 3)
#define ACCR0_GCAXIFS(x)	(((x) & 0x7) << 6)
#define ACCR0_GCFS(x)		(((x) & 0x7) << 9)

#define ACCR0_DCFS_MASK		(0x7 << 0)	/* Display Controller Frequency Select */
#define ACCR0_VMFC_MASK		(0x7 << 3)	/* vMeta Controller Frequency Select */
#define ACCR0_GCAXIFS_MASK	(0x7 << 6)	/* Graphics AXI Bus Frequency Select */
#define ACCR0_GCFS_MASK		(0x7 << 9)	/* Graphics Controller Frequency Select */

#define ACCR1_DIS_DRX		(1 << 31)	/* Disable DRX */
#define ACCR1_VMETA_156_312	(1 << 21)	/* VMeta Frequency Control: 0 = 156Mhz, 1 = 312Mhz */
#define ACCR1_PU_OTG		(1 << 12)	/* USB 2.0 PHY OTG power up */
#define ACCR1_PU_PLL		(1 << 11)	/* USB 2.0 PHY PLL power up */
#define ACCR1_PU		(1 << 10)	/* USB 2.0 PHY power up */
#define ACCR1_I2C_33_52		(1 << 8)	/* I2C frequency control: 0 = 624/19 Mhz, 1 = 624/12 Mhz */
#define ACCR1_MMC6_48_52	(1 << 6)	/* MMC6 frequency control: 0 = 624/13 Mhz, 1 = 624/12 Mhz */
#define ACCR1_MMC5_48_52	(1 << 4)	/* MMC5 frequency control: 0 = 624/13 Mhz, 1 = 624/12 Mhz */
#define ACCR1_MMC4_48_52	(1 << 2)	/* MMC4 frequency control: 0 = 624/13 Mhz, 1 = 624/12 Mhz */
#define ACCR1_MMC3_48_52	(1 << 0)	/* MMC3 frequency control: 0 = 624/13 Mhz, 1 = 624/12 Mhz */

#define AC_GO_MASK		(1 << 31)	/* Core PLL Automatic Change GO */
#define ACLK_RATIO_OFFSET	(4)		/* FRQ_CHANGE_CTL ACLK_RATIO offset*/
#define ACLK_RATIO_MASK		(3 << 4)	/* ACLK Ratio setting: 0 = core, 1 = core/2, 2 = core/3 */
#define SYS_FREQ_SEL_MASK	(7 << 1)	/* System PLL Clock Selection */
#define CLK_SRC_MASK		(1 << 0)	/* Clock Source Selection */

#define MC_GO_MASK		(1 << 31)	/* Core PLL Manual Change GO */
#define PLL_EN_MASK		(1 << 26)	/* PLL On/Off control */
#define PPDIV_MASK		(1 << 25)	/* External Divider PPDIV Value COnfiguration */
#define KVCO_MASK		(0xf << 21)	/* Core/DDR PLL KVCO Value Configuratioin */
#define VCODIV_SEL_MASK		(0xf << 17)	/* Core/DDR PLL VCODIV_SEL Value Configuration */
#define FBDIV_MASK		(0x1ff << 5)	/* Core/DDR PLL FBDIV Value Configuration */
#define REFDIV_MASK		(0x1f << 0)	/* Core/DDR PLL REFDIV Value Configuration */

#define DDRPLL_FC_OFFSET	(16)
#define DDRPLL_FC_MASK		(1 << 16)	/* DDR PLL frequency change request*/

#define AVLCR_VC_GO_MASK	(0x1 << 0)
#define AVLCR_LEVEL_OFFSET	1
#define AVLCR_LEVEL_MASK	(0x3 << AVLCR_LEVEL_OFFSET)

#define VLT_LEVEL_0		0
#define VLT_LEVEL_1		1
#define VLT_LEVEL_2		2
#define VLT_LEVEL_3		3

/*
 * Clock Enable Bit
 */
#define CKEN_LCD	1	/* < LCD Clock Enable */
#define CKEN_USBH	2	/* < USB host clock enable */
#define CKEN_CAMERA	3	/* < Camera interface clock enable */
#define CKEN_NAND	4	/* < NAND Flash Controller Clock Enable */
#define CKEN_USB2	6	/* < USB 2.0 client clock enable. */
#define CKEN_DMC	8	/* < Dynamic Memory Controller clock enable */
#define CKEN_SMC	9	/* < Static Memory Controller clock enable */
#define CKEN_ISC	10	/* < Internal SRAM Controller clock enable */
#define CKEN_BOOT	11	/* < Boot rom clock enable */
#define CKEN_MMC1	12	/* < MMC1 Clock enable */
#define CKEN_MMC2	13	/* < MMC2 clock enable */
#define CKEN_KEYPAD	14	/* < Keypand Controller Clock Enable */
#define CKEN_CIR	15	/* < Consumer IR Clock Enable */
#define CKEN_USIM0	17	/* < USIM[0] Clock Enable */
#define CKEN_USIM1	18	/* < USIM[1] Clock Enable */
#define CKEN_TPM	19	/* < TPM clock enable */
#define CKEN_UDC	20	/* < UDC clock enable */
#define CKEN_BTUART	21	/* < BTUART clock enable */
#define CKEN_FFUART	22	/* < FFUART clock enable */
#define CKEN_STUART	23	/* < STUART clock enable */
#define CKEN_AC97	24	/* < AC97 clock enable */
#define CKEN_TOUCH	25	/* < Touch screen Interface Clock Enable */
#define CKEN_SSP1	26	/* < SSP1 clock enable */
#define CKEN_SSP2	27	/* < SSP2 clock enable */
#define CKEN_SSP3	28	/* < SSP3 clock enable */
#define CKEN_SSP4	29	/* < SSP4 clock enable */
#define CKEN_MSL0	30	/* < MSL0 clock enable */
#define CKEN_PWM0	32	/* < PWM[0] clock enable */
#define CKEN_PWM1	33	/* < PWM[1] clock enable */
#define CKEN_HSI	34	/* < HSI clock enable */
#define CKEN_VMETA	35	/* < VMeta clock enable */
#define CKEN_I2C	36	/* < I2C clock enable */
#define CKEN_INTC	38	/* < Interrupt controller clock enable */
#define CKEN_GPIO	39	/* < GPIO clock enable */
#define CKEN_1WIRE	40	/* < 1-wire clock enable */
#define CKEN_HSIO2	41	/* < HSIO2 clock enable */
#define CKEN_MINI_IM	48	/* < Mini-IM */
#define CKEN_MINI_LCD	49	/* < Mini LCD */
#define CKEN_ABU	59	/* < ABU clock enable */
#define CKEN_HSIO	61	/* < System Bus (HSIO) clock enable */
#define CKEN_CSI_TX	64	/* < CSI TX Escape clock enable */
#define CKEN_PXA95x_MMC1	65	/* < MMC1 clock enable */
#define CKEN_PXA95x_MMC2	66	/* < MMC2 clock enable */
#define CKEN_PXA95x_MMC3	67	/* < MMC3 clock enable */
#define CKEN_PXA95x_MMC4	68	/* < MMC4 clock enable */
#define CKEN_AXI_2X	69	/* < AXI 2X clock enable */
#define CKEN_USB_PRL	70	/* < USB2(OTG) Peripheral clock enable */
#define CKEN_USBH_PRL	71	/* < USB2 Host1 Peripheral clock enable */
#define CKEN_USB_BUS	74	/* < USB2(OTG) System Bus clock enable */
#define CKEN_USBH_BUS	75	/* < USB2 Host1 System Bus clock enable */
#define CKEN_MMC1_BUS	78	/* < MMC1 System Bus clock enable */
#define CKEN_MMC_BUS	78	/* < All 4 MMC System Bus clock enable */
#define CKEN_MMC2_BUS	79	/* < MMC2 System Bus clock enable */
#define CKEN_MMC3_BUS	80	/* < MMC3 System Bus clock enable */
#define CKEN_MMC4_BUS	81	/* < MMC4 System Bus clock enable */
#define CKEN_IMU	82	/* < ISLAND MMC USB System Bus clock enable */
#define CKEN_AXI	83	/* < AXI clock enable */
#define CKEN_DISPLAY	86	/* < Display clock enable */
#define CKEN_PIXEL	87	/* < Pixel clock enable */
#define CKEN_I2C2	88	/* < I2C2 clock enable */
#define CKEN_I2C3	89	/* < I2C3 clock enable */
#define CKEN_SCI1	90	/* < Camera SCI1 clock enable */
#define CKEN_SCI2	91	/* < Camera SCI2 clock enable */
#define CKEN_GC_1X	92	/* < Graphics 1x clock enable */
#define CKEN_GC_2X	93	/* < Graphics 2x clock enable */
#define CKEN_DSI_TX1	94	/* < DSI TX1 Escape clock enable */
#define CKEN_DSI_TX2	95	/* < DSI TX2 Escape clock enable */

#define CKEN_MMC3	5	/* < MMC3 Clock Enable */
#define CKEN_MVED	43	/* < MVED clock enable */

/* Note: GCU clock enable bit differs on PXA300/PXA310 and PXA320 */
#define CKEN_PXA300_GCU		42	/* Graphics controller clock enable */
#define CKEN_PXA320_GCU		7	/* Graphics controller clock enable */

#define CKEN_PWM4	0	/* < PWMCCR4 offset  */
#define CKEN_PWM5	4	/* < PWMCCR4 offset  */
#define CKEN_PWM6	8	/* < PWMCCR4 offset  */
#define CKEN_PWM7	12	/* < PWMCCR4 offset  */
#define PWMCLKEN_SLOW	(1 << 6)	/* PWM SLOW CLK enble Reset */

/*
 * Application Subsystem Clock - Continued
 */
#define DDR_SC_CTRL2	__REG(0x40A000E4)	/* DDR Sequence Control 2 Register */

/*
 * General registers
 */
#define GEN_REG3		(0x42404008)
#define GEN_REG3_WO_MASK	(0xBFF00)
#define GEN_REG3_SPLGEN		(1 << 19)
#define GEN_REG3_SPLGEN_MASK	(1 << 19)


#endif /* __ASM_ARCH_PXA3XX_REGS_H */
