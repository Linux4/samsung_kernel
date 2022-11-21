/*
 * linux/arch/arm/mach-mmp/include/mach/addr-map.h
 *
 *   Common address map definitions
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __ASM_MACH_ADDR_MAP_H
#define __ASM_MACH_ADDR_MAP_H

/* APB - Application Subsystem Peripheral Bus
 *
 * NOTE: the DMA controller registers are actually on the AXI fabric #1
 * slave port to AHB/APB bridge, due to its close relationship to those
 * peripherals on APB, let's count it into the ABP mapping area.
 */
#define APB_PHYS_BASE		0xd4000000
#define APB_VIRT_BASE		IOMEM(0xfe000000)
#define APB_PHYS_SIZE		0x00200000

#define AXI_PHYS_BASE		0xd4200000
#define AXI_VIRT_BASE		IOMEM(0xfe200000)
#define AXI_PHYS_SIZE		0x00200000

/* Static Memory Controller - Chip Select 0 and 1 */
#define SMC_CS0_PHYS_BASE	0x80000000
#define SMC_CS0_PHYS_SIZE	0x10000000
#define SMC_CS1_PHYS_BASE	0x90000000
#define SMC_CS1_PHYS_SIZE	0x10000000

#define APMU_VIRT_BASE		(AXI_VIRT_BASE + 0x82800)
#define APMU_REG(x)		(APMU_VIRT_BASE + (x))

#define APBC_VIRT_BASE		(APB_VIRT_BASE + 0x015000)
#define APBC_REG(x)		(APBC_VIRT_BASE + (x))

#define APBCP_VIRT_BASE		(APB_VIRT_BASE + 0x03B000)
#define APBCP_REG(x)		(APBCP_VIRT_BASE + (x))

#define MPMU_VIRT_BASE		(APB_VIRT_BASE + 0x50000)
#define MPMU_REG(x)		(MPMU_VIRT_BASE + (x))

#define CIU_VIRT_BASE		(AXI_VIRT_BASE + 0x82c00)
#define CIU_REG(x)		(CIU_VIRT_BASE + (x))

#ifdef CONFIG_CPU_MMP3
#define AUD_PHYS_BASE		0xc0ffd800
#define AUD_VIRT_BASE		IOMEM(0xfeffd800)
#define AUD_PHYS_SIZE		SZ_2K

#define AUD_PHYS_BASE2		0xc0140000
#define AUD_VIRT_BASE2		IOMEM(0xfef40000)
#define AUD_PHYS_SIZE2		0x00010000
#endif

#if defined(CONFIG_CPU_PXA988)
#define PERI_PHYS_BASE		0xd1dfe000
#define SL2C_PHYS_BASE		0xd1dfb000
#elif defined(CONFIG_CPU_MMP3)
#define PERI_PHYS_BASE		0xe0000000
#define SL2C_PHYS_BASE		0xd0020000
#endif
#define PERI_VIRT_BASE		IOMEM(0xfe400000)

#define GIC_DIST_VIRT_BASE	(PERI_VIRT_BASE + 0x1000)
#if defined(CONFIG_CPU_CA9MP) || defined(CONFIG_CPU_PJ4B)
#define SCU_PHYS_BASE		PERI_PHYS_BASE
#define SCU_VIRT_BASE		PERI_VIRT_BASE
#define GIC_CPU_VIRT_BASE	(PERI_VIRT_BASE + 0x0100)
#define TWD_VIRT_BASE		(PERI_VIRT_BASE + 0x0600)
#define TWD_PHYS_BASE		(PERI_PHYS_BASE + 0x0600)
#define PERI_PHYS_SIZE		0x00002000
#elif defined(CONFIG_CPU_CA7MP)
#define GIC_CPU_VIRT_BASE	(PERI_VIRT_BASE + 0x2000)
#define PERI_PHYS_SIZE		0x00008000
#endif

#ifdef CONFIG_CPU_PXA988
#define DMCU_PHYS_BASE		0xc0100000
#define DMCU_VIRT_BASE		0xfe500000
#define DMCU_PHYS_SIZE		0x00010000
#endif

#define SL2C_VIRT_BASE		0xfe800000
#define SL2C_PHYS_SIZE		SZ_8K

#define SRAM_PHYS_BASE		0xd1000000

#ifdef CONFIG_CPU_PXA988
#define SRAM_CP_BASE		SRAM_PHYS_BASE
#define SRAM_CP_SIZE		0x4000

#define SRAM_AUDIO_BASE		(SRAM_CP_BASE + SRAM_CP_SIZE)
#define SRAM_AUDIO_SIZE		0xaa00

#define SRAM_VIDEO_BASE		(SRAM_AUDIO_BASE + SRAM_AUDIO_SIZE)
#define SRAM_VIDEO_SIZE		0x11600
#endif

#endif /* __ASM_MACH_ADDR_MAP_H */
