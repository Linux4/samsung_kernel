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

#ifndef __ASM_ARCH_HARDWARE_SCX30G_H
#define __ASM_ARCH_HARDWARE_SCX30G_H

#ifndef __ASM_ARCH_SCI_HARDWARE_H
#error  "Don't include this file directly, include <mach/hardware.h>"
#endif

/*
 * 8830 internal I/O mappings
 * 0x30000000-0x50000000 AON.
 * We have the following mapping according to asic spec.
 * We have set some trap gaps in the vaddr.
 */
#define SCI_IOMAP_BASE	0xF5000000

#define SCI_IOMAP(x)	(SCI_IOMAP_BASE + (x))

#define SCI_IOMEMMAP_BASE	0xcc800000

#define SCI_IOMEMMAP(x)	(SCI_IOMEMMAP_BASE + (x))

#ifndef SCI_ADDR
#define SCI_ADDR(_b_, _o_)                              ( (u32)(_b_) + (_o_) )
#endif

#define LL_DEBUG_UART_PHYS		SPRD_UART1_PHYS
#define LL_DEBUG_UART_BASE		SPRD_UART1_BASE


#include <soc/sprd/iomap.h>

#if !defined __ASSEMBLER__
extern unsigned long sprd_adi_base;
extern unsigned long sprd_adi_phys;
#define SPRD_ADI_BASE		sprd_adi_base
#define SPRD_ADI_PHYS		sprd_adi_phys

#define SPRD_ADISLAVE_BASE		(SPRD_ADI_BASE+0x8000)
#define SPRD_ADISLAVE_PHYS		(SPRD_ADI_PHYS+0x8000)
#define REGS_ADISLAVE_BASE		SPRD_ADISLAVE_BASE
#define REGS_ADISLAVE_PHYS		SPRD_ADISLAVE_PHYS
#endif

#define SPRD_UART0_BASE		SCI_IOMAP(0x360000)
#define SPRD_UART0_PHYS		0x70000000
#define SPRD_UART0_SIZE		SZ_4K

#define SPRD_UART1_BASE		SCI_IOMAP(0x362000)
#define SPRD_UART1_PHYS		0x70100000
#define SPRD_UART1_SIZE		SZ_4K

#define SPRD_UART2_BASE		SCI_IOMAP(0x364000)
#define SPRD_UART2_PHYS		0x70200000
#define SPRD_UART2_SIZE		SZ_4K

#define SPRD_UART3_BASE		SCI_IOMAP(0x366000)
#define SPRD_UART3_PHYS		0x70300000
#define SPRD_UART3_SIZE		SZ_4K

#define SPRD_UART4_BASE		SCI_IOMAP(0x368000)
#define SPRD_UART4_PHYS		0x70400000
#define SPRD_UART4_SIZE		SZ_4K


#define SPRD_IRAM0_BASE		SCI_IOMAP(0x3d0000)
#define SPRD_IRAM0_PHYS		0x0
#define SPRD_IRAM0_SIZE		SZ_4K

#define SPRD_IRAM0H_BASE	SCI_IOMAP(0x3d1000)
#define SPRD_IRAM0H_PHYS	0x1000
#define SPRD_IRAM0H_SIZE	SZ_4K

#define SPRD_IRAM1_BASE		SCI_IOMAP(0x3d4000)
#define SPRD_IRAM1_PHYS		0x50000000
#define SPRD_IRAM1_SIZE		(SZ_8K + SZ_4K)

#define SPRD_IRAM2_BASE		SCI_IOMAP(0x3d7000)
#define SPRD_IRAM2_PHYS		0x50004000
#define SPRD_IRAM2_SIZE		(SZ_32K + SZ_4K)

#define SPRD_PWM_BASE		SCI_IOMAP(0x20a000)
#define SPRD_PWM_PHYS		0X40260000
#define SPRD_PWM_SIZE		SZ_4K

#define SPRD_LPDDR2_BASE	SCI_IOMAP(0x160000)
#define SPRD_LPDDR2_PHYS	0X30000000
#define SPRD_LPDDR2_SIZE	SZ_4K

#define SPRD_LPDDR2_PHY_BASE	SCI_IOMAP(0x170000)
#define SPRD_LPDDR2_PHY_PHYS	0X30010000
#define SPRD_LPDDR2_PHY_SIZE	SZ_4K

#define SPRD_SDIO1_PHYS		0X20400000
#define SPRD_SDIO1_SIZE		SZ_4K

#define SPRD_MALI_PHYS		0X60000000

#define CORE_GIC_CPU_VA		(SPRD_CORE_BASE + 0x2000)
#define CORE_GIC_DIS_VA		(SPRD_CORE_BASE + 0x1000)

#define HOLDING_PEN_VADDR	(SPRD_AHB_BASE + 0x14)
#define CPU_JUMP_VADDR		(HOLDING_PEN_VADDR + 0X4)

/* registers for watchdog ,RTC, touch panel, aux adc, analog die... */
#define SPRD_MISC_BASE	((unsigned int)SPRD_ADI_BASE)
#define SPRD_MISC_PHYS	((unsigned int)SPRD_ADI_PHYS)

#define ANA_PWM_BASE			(SPRD_ADISLAVE_BASE + 0x20 )
#define ANA_WDG_BASE			(SPRD_ADISLAVE_BASE + 0x40 )
#define ANA_RTC_BASE			(SPRD_ADISLAVE_BASE + 0x80 )
#define ANA_EIC_BASE			(SPRD_ADISLAVE_BASE + 0x100 )
#define ANA_PIN_BASE			(SPRD_ADISLAVE_BASE + 0x180 )
#define ANA_THM_BASE			(SPRD_ADISLAVE_BASE + 0x280 )
#define ADC_BASE				(SPRD_ADISLAVE_BASE + 0x300 )
#define ANA_CTL_INT_BASE		(SPRD_ADISLAVE_BASE + 0x380 )
#define ANA_BTLC_INT_BASE		(SPRD_ADISLAVE_BASE + 0x3c0 )
#define ANA_AUDIFA_INT_BASE		(SPRD_ADISLAVE_BASE + 0x400 )
#define ANA_GPIO_INT_BASE		(SPRD_ADISLAVE_BASE + 0x480 )
#define ANA_FPU_INT_BASE		(SPRD_ADISLAVE_BASE + 0x500 )
#define ANA_AUDCFGA_INT_BASE		(SPRD_ADISLAVE_BASE + 0x600 )
#define ANA_HDT_INT_BASE		(SPRD_ADISLAVE_BASE + 0x700 )
#define ANA_CTL_GLB_BASE		(SPRD_ADISLAVE_BASE + 0x800 )

#define SPRD_ANA_PIN_BASE		((unsigned int)SPRD_ADI_BASE + 0x8180)

#ifndef REGS_AHB_BASE
#define REGS_AHB_BASE                                   ( SPRD_AHB_BASE  + 0x200)
#endif

#define SPRD_IRAM_BASE		SPRD_IRAM0_BASE + 0x1000
#define SPRD_IRAM_PHYS		SPRD_IRAM0_PHYS + 0x1000
#define SPRD_IRAM_SIZE		SZ_4K
#define SPRD_GREG_BASE		SPRD_AONAPB_BASE
#define SPRD_GREG_PHYS		SPRD_AONAPB_PHYS
#define SPRD_GREG_SIZE		SZ_64K

#ifndef REGS_GLB_BASE
#define REGS_GLB_BASE                                   ( SPRD_GREG_BASE )
#define ANA_REGS_GLB_BASE                               ( ANA_CTL_GLB_BASE )
#endif

#define CHIP_ID_LOW_REG		(SPRD_AHB_BASE + 0xfc)

#define SPRD_GPTIMER_BASE	SPRD_GPTIMER0_BASE
//#define REG_GLB_GEN0 		SPRD_AONAPB_BASE
#define SPRD_EFUSE_BASE		SPRD_UIDEFUSE_BASE

#define REGS_AP_AHB_BASE	SPRD_AHB_BASE
#define REGS_AP_APB_BASE	SPRD_APBREG_BASE
#define REGS_AON_APB_BASE	SPRD_AONAPB_BASE
#define REGS_GPU_APB_BASE	SPRD_GPUAPB_BASE
#define REGS_MM_AHB_BASE	SPRD_MMAHB_BASE
#define REGS_PMU_APB_BASE	SPRD_PMU_BASE
#define REGS_AON_CLK_BASE	SPRD_AONCKG_BASE
#define REGS_AP_CLK_BASE	SPRD_APBCKG_BASE
#define REGS_GPU_CLK_BASE	SPRD_GPUCKG_BASE
#define REGS_MM_CLK_BASE	SPRD_MMCKG_BASE
#define REGS_PUB_APB_BASE	SPRD_PUB_BASE

#if defined(CONFIG_ARCH_SCX15)
#if defined(CONFIG_SPRD_MODEM_TD)
#define SIPC_SMEM_ADDR 		(CONFIG_PHYS_OFFSET + 120 * SZ_1M)
#define CPT_START_ADDR		(CONFIG_PHYS_OFFSET + 128 * SZ_1M)
#define CPT_TOTAL_SIZE		(SZ_1M * 18)
#define CPT_RING_ADDR		(CPT_START_ADDR + CPT_TOTAL_SIZE - SZ_4K)
#define CPT_RING_SIZE		(SZ_4K)
#define CPT_SMEM_SIZE		(SZ_1M + SZ_256K)
#define WCN_START_ADDR		(CONFIG_PHYS_OFFSET + 168 * SZ_1M)
#define WCN_TOTAL_SIZE		0x281000//(SZ_1M * 5)
#define WCN_RING_ADDR		(WCN_START_ADDR + WCN_TOTAL_SIZE - SZ_4K)
#define WCN_RING_SIZE		(SZ_4K)
#define WCN_SMEM_SIZE		(SZ_512K + SZ_256K)
#else
#define SIPC_SMEM_ADDR 		(CONFIG_PHYS_OFFSET + 120 * SZ_1M)
#define CPW_START_ADDR		(CONFIG_PHYS_OFFSET + 128 * SZ_1M)
#define CPW_TOTAL_SIZE		0x18A0000 //(SZ_1M * 32)
#define CPW_RING_ADDR		(CPW_START_ADDR + CPW_TOTAL_SIZE - SZ_4K)
#define CPW_RING_SIZE		(SZ_4K)
#define CPW_SMEM_SIZE		(SZ_1M + SZ_256K)
#define WCN_START_ADDR		(CONFIG_PHYS_OFFSET + 168 * SZ_1M)
#define WCN_TOTAL_SIZE		0x281000//(SZ_1M * 5)
#define WCN_RING_ADDR		(WCN_START_ADDR + WCN_TOTAL_SIZE - SZ_4K)
#define WCN_RING_SIZE		(SZ_4K)
#define WCN_SMEM_SIZE		(SZ_512K + SZ_256K)
#endif
#else
#define SIPC_SMEM_ADDR 		(CONFIG_PHYS_OFFSET + 120 * SZ_1M)

#define CPT_START_ADDR		(CONFIG_PHYS_OFFSET + 128 * SZ_1M)
#define CPT_TOTAL_SIZE                  (SZ_1M * 18)
#define CPT_RING_ADDR			(CPT_START_ADDR + CPT_TOTAL_SIZE - SZ_4K)
#define CPT_RING_SIZE			(SZ_4K)
#define CPT_SMEM_SIZE			(SZ_1M + SZ_256K)

#ifdef CONFIG_ARCH_SCX30G
#define CPW_START_ADDR		(CONFIG_PHYS_OFFSET + 128 * SZ_1M)
#else
#define CPW_START_ADDR		(CONFIG_PHYS_OFFSET + 256 * SZ_1M)
#endif
#if defined(CONFIG_MODEM_W_MEMCUT)
#define CPW_TOTAL_SIZE		(SZ_1M * 29)
#elif defined CONFIG_ARCH_SCX30G
#if (defined CONFIG_ARCH_SCX30G3 && !defined CONFIG_MACH_SP8730SEEA_T3)
#define CPW_TOTAL_SIZE		(SZ_1M * 29)
#else
#define CPW_TOTAL_SIZE		(SZ_1M * 29)
#endif
#else
#define CPW_TOTAL_SIZE		(SZ_1M * 29)
#endif
#define CPW_RING_ADDR		(CPW_START_ADDR + CPW_TOTAL_SIZE - SZ_4K)
#define CPW_RING_SIZE			(SZ_4K)
#define CPW_SMEM_SIZE		(SZ_1M + SZ_256K)

#if defined(CONFIG_ARCH_SCX30G)
#define WCN_START_ADDR		(CONFIG_PHYS_OFFSET + 168 * SZ_1M)//Tshark moves cp2 to 168M
#define WCN_TOTAL_SIZE		0x201000//Tshark 8830gea memcut to 2M+4k
#else
#define WCN_START_ADDR		(CONFIG_PHYS_OFFSET + 320 * SZ_1M)
#define WCN_TOTAL_SIZE		0x281000//(SZ_1M * 5)
#endif
#define WCN_RING_ADDR		(WCN_START_ADDR + WCN_TOTAL_SIZE - SZ_4K)
#define WCN_RING_SIZE			(SZ_4K)
#define WCN_SMEM_SIZE		(SZ_512K + SZ_256K)
#endif

#endif
