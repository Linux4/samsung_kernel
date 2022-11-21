/*
 * linux/arch/arm/mach-mmp/include/mach/regs-mcu.h
 *
 *   Memory Control Unit
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __ASM_MACH_MCU_H
#define __ASM_MACH_MCU_H

#include <mach/addr-map.h>

#ifdef CONFIG_CPU_MMP3
#define FIXADDR(base, offset)		((u32 *)(((u32)base)+offset))
#define DMCU_HWTCTRL(base)		FIXADDR(base, 0x1c0)
#define DMCU_HWTDAT0(base)		FIXADDR(base, 0x1c8)
#define DMCU_HWTDAT1(base)		FIXADDR(base, 0x1cc)
#else
#define DMCU_VIRT_REG(x)		(DMCU_VIRT_BASE + (x))
#define DMCU_PHYS_REG(x)		(DMCU_PHYS_BASE + (x))
#define DMCU_HWTCTRL			(0x1c0)
#define DMCU_HWTDAT0			(0x1c8)
#define DMCU_HWTDAT1			(0x1cc)
#endif
#define DMCU_HWTPAUSE			(0x00010000)
#define DMCU_HWTEND			(0x00020000)
#define DMCU_HWTWRITE			(0x80000000)

#define DMCU_CPU_ID_REV			(0x00)
#define DMCU_STATUS			(0x04)
#define DMCU_DRAM_STATUS		(0x08)

#define DMCU_MAP_CS0			(0x10)
#define DMCU_MAP_CS1			(0x14)
#define DMCU_MAP_VALID			(1u << 0)
#define DMCU_CMD_CSSEL_CS0		(1u << 24)
#define DMCU_CMD_CSSEL_CS1		(1u << 25)
#ifdef CONFIG_CPU_PXA988
#define DMCU_MAP_CS2			(0x18)
#define DMCU_MAP_CS3			(0x1c)
#endif

#define DMCU_SDRAM_CFG0_TYPE1		(0x20)
#define DMCU_SDRAM_CFG1_TYPE1		(0x24)
#ifdef CONFIG_CPU_PXA988
#define DMCU_SDRAM_CFG2_TYPE1		(0x28)
#define DMCU_SDRAM_CFG3_TYPE1		(0x2c)
#endif

#define DMCU_SDRAM_CFG0_TYPE2		(0x30)
#define DMCU_SDRAM_CFG1_TYPE2		(0x34)
#ifdef CONFIG_CPU_PXA988
#define DMCU_SDRAM_CFG2_TYPE2		(0x38)
#define DMCU_SDRAM_CFG3_TYPE2		(0x3c)
#endif

#define DMCU_SDRAM_CTRL1		(0x50)
#define DMCU_SDRAM_CTRL2		(0x54)
#define DMCU_SDRAM_CTRL4		(0x58)
#define DMCU_SDRAM_TYPE_MASK		(7u << 2)
#define DMCU_SDRAM_TYPE_DDR3		(2u << 2)
#define DMCU_SDRAM_TYPE_LPDDR2		(5u << 2)
#define DMCU_SDRAM_CTRL4_CL_SHIFT	(13)
#define DMCU_SDRAM_CTRL4_CL_MASK	(0xf << DMCU_SDRAM_CTRL4_CL_SHIFT)
#define DMCU_SDRAM_CTRL6		(0x5c)
#define DMCU_SDRAM_CTRL7		(0x60)
#define DMCU_SDRAM_CTRL13		(0x64)
#define DMCU_SDRAM_CTRL14		(0x68)
#define DMCU_SDRAM_TIMING1		(0x80)
#define DMCU_SDRAM_TIMING2		(0x84)
#define DMCU_SDRAM_TIMING3		(0x88)
#define DMCU_SDRAM_TIMING4		(0x8c)
#define DMCU_SDRAM_TIMING5		(0x90)
#define DMCU_SDRAM_TIMING6		(0x94)
#define DMCU_SDRAM_TIMING7		(0x98)
#define DMCU_SDRAM_TIMING8		(0x9c)
#define DMCU_EXCLUSIVE_MONITOR_CTRL	(0x100)
#define DMCU_DATA_COH_CTRL		(0x110)
#define DMCU_TRUSTZONE_SEL		(0x120)
#define DMCU_TRUSTZONE_RANGE0		(0x124)
#define DMCU_TRUSTZONE_RANGE1		(0x128)
#define DMCU_TRUSTZONE_PERMISSION	(0x12C)
#define DMCU_PORT_PRIORITY		(0x140)
#define DMCU_BQ_STARV_PREVENTION	(0x144)
#define DMCU_RRB_STARV_PREVENTION0	(0x148)
#define DMCU_RRB_STARV_PREVENTION1	(0x14C)
#define DMCU_SRAM_CTRL1			(0x150)
#define DMCU_SRAM_CTRL2			(0x154)
#define DMCU_SRAM_CTRL3			(0x158)
#define DMCU_USER_COMMAND0		(0x160)
#define DMCU_USER_COMMAND1		(0x164)
#define DMCU_MODE_RD_DATA		(0x170)

#ifdef CONFIG_CPU_PXA988
#define DMCU_SMR1			(0x180)
#define DMCU_SMR2			(0x184)
#endif

#define DMCU_PHY_CTRL3			(0x220)
#define DMCU_PHY_CTRL7			(0x230)
#define DMCU_PHY_CTRL8			(0x234)
#define DMCU_PHY_CTRL9			(0x238)
#define DMCU_PHY_CTRL10			(0x23c)
#define DMCU_PHY_CTRL11			(0x240)

#ifdef CONFIG_CPU_PXA988
#define DMCU_PHY_CTRL12			(0x244)
#endif

#define DMCU_PHY_CTRL13			(0x248)
#define DMCU_PHY_CTRL14			(0x24c)
#define DMCU_PHY_CTRL15			(0x250)
#define DMCU_PHY_CTRL16			(0x254)
#define DMCU_PHY_CTRL21			(0x258)
#define DMCU_PHY_CTRL19			(0x280)
#define DMCU_PHY_CTRL20			(0x284)
#define DMCU_PHY_CTRL22			(0x288)
#define DMCU_PHY_DQ_BYTE_SEL		(0x300)
#define DMCU_PHY_DLL_CTRL_BYTE1		(0x304)
#define DMCU_PHY_DLL_WL_SEL		(0x380)
#define DMCU_PHY_DLL_WL_CTRL0		(0x384)
#define DMCU_PHY_DLL_WL_CTRL1		(0x388)
#define DMCU_PHY_DLL_WL_CTRL2		(0x38C)
#define DMCU_PHY_DLL_RL_CTRL		(0x390)

#define PHY_CTRL14_DLL_RESET		(1u << 29)
#define PHY_CTRL14_DLL_UPDATE		(1u << 30)
#define PHY_CTRL14_PHY_SYNC		(1u << 31)

#ifdef CONFIG_CPU_PXA988
#define DMCU_PHY_CTRL_TESTMODE		(0x400)
#endif

#define DMCU_TEST_MODE0			(0x410)
#define DMCU_TEST_MODE1			(0x414)
#define DMCU_PERF_CNT_CTRL0		(0x440)
#define DMCU_PERF_CNT_STATUS		(0x444)
#define DMCU_PERF_CNT_SEL		(0x448)
#define DMCU_PERF_CNT0			(0x450)
#define DMCU_PERF_CNT1			(0x454)
#define DMCU_PERF_CNT2			(0x458)
#define DMCU_PERF_CNT3			(0x45c)

#define INSERT_ENTRY(value, regid, index)			\
	do {							\
		__raw_writel(value, DMCU_VIRT_REG(DMCU_HWTDAT0));\
		__raw_writel(regid, DMCU_VIRT_REG(DMCU_HWTDAT1));\
		__raw_writel(index, DMCU_VIRT_REG(DMCU_HWTCTRL));\
	} while (0)

#endif /* __ASM_MACH_MCU_H */
