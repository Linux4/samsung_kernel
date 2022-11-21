/*
 * linux/arch/arm/mach-mmp/include/mach/regs-map.h
 *
 *   Common soc registers map
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#ifndef __ASM_MACH_REGS_MAP_H
#define __ASM_MACH_REGS_MAP_H
#include <plat/dump_regs.h>

struct reg_map pxa_reg_map[] = {
#ifdef CONFIG_CPU_PXA988
	{0xD4050000, 0xD405004C, 0, "Main_PMU1"},
	{0xD4050100, 0xD4050100, 0, "Main_PMU2"},
	{0xD4050200, 0xD4050200, 0, "Main_PMU3"},
	{0xD4050400, 0xD4050410, 0, "Main_PMU4"},
	{0xD4051000, 0xD4051004, 0, "Main_PMU5"},
	{0xD4051020, 0xD4051028, 0, "Main_PMU6"},
	{0xD4051048, 0xD405104C, 0, "Main_PMU7"},
	{0xD4282800, 0xD4282804, 0, "Apps_PMU_1"},
	{0xD4282808, 0xD428280C, 0, "Apps_PMU_2"},
	{0xD4282810, 0xD4282820, 0, "Apps_PMU_3"},
	{0xD4282828, 0xD42828F0, 0, "Apps_PMU_4"},
	{0xD4282900, 0xD4282908, 0, "Apps_PMU_5"},
	{0xD4282920, 0xD4282928, 0, "Apps_PMU_6"},
	{0xF00E0000, 0xF00E0140, 0, "CCU"},
	{0xD403B000, 0xD403B03C, 0, "APB_ctl"},
	{0xD4015000, 0xD4015060, 0, "APB_clock"},
	{0xD4080000, 0xD40800AC, 0, "PMU_Timer"},
	{0xFFA60000, 0xFFA60018, 0, "Sleep_Timer"},
	{0xD4014000, 0xD40140B0, 0, "Timer"},
	{0xD401E000, 0xD401E32C, 0, "MFPR"},
	{0xD4282000, 0xD428214C, 0, "ICU"},
	{0xD4282C00, 0xD4282CE8, 0, "CIU"},
	{0xF0205100, 0xF020510C, 0, "DSSP0"},
	{0xF0206100, 0xF020610C, 0, "DSSP1"},
	{0xF0207100, 0xF020710C, 0, "DSSP2"},
	{0xF0208100, 0xF020810C, 0, "DSSP3"},
	{0xD4032000, 0xD4032044, 0, "USIM1"},
	{0xD4033000, 0xD4033044, 0, "USIM2"},
	{0xD4019000, 0xD4019014, 0, "GPIO1"},
	{0xD4019030, 0xD4019050, 0, "GPIO2"},
	{0xD401909C, 0xD40190A4, 0, "GPIO3"},
	{0xD4019800, 0xD401980C, 0, "GPIO_Edge"},
	{0xD401D000, 0xD401D000, 0, "APIPC"},
	{0xD4280000, 0xD42800FE, 0, "SD1"},
	{0xD4280800, 0xD42808FE, 0, "SD2"},
	{0xD4281000, 0xD42810FE, 0, "SD3"},
	{0xD4036000, 0xD403602C, 0, "UART0"},
	{0xD4017000, 0xD401702C, 0, "UART1"},
	{0xD4018000, 0xD401802C, 0, "UART1"},
	{0xD420A000, 0xD420A238, 0, "Camera"},
	{0xD420B000, 0xD420B1FC, 0, "LCD"},
	{0xD401B000, 0xD401B08C, 0, "SSP0"},
	{0xD42A0C00, 0xD42A0C8C, 0, "SSP1"},
	{0xD401C000, 0xD401C08C, 0, "SSP2"},
	{0xD4011000, 0xD401108C, 0, "TWSI0"},
	{0xD4010800, 0xD401088C, 0, "TWSI1"},
	{0xD4010000, 0xD4010024, 0, "RTC"},
	{0xD4012000, 0xD4012048, 0, "KPC"},
	{0xD4013200, 0xD4013224, 0, "DRO"},
	{0xD4000000, 0xD40003FC, 0, "DMAC"},
	{0xD4011800, 0xD4011810, 0, "1Wire"},
	{0xD4208000, 0xD42081FC, 0, "USB_OTG"},
	{0xD4207000, 0xD420707C, 0, "USB_PHY"},
	{0xC0100000, 0xC010045C, 0, "DDR_MC"},
#endif
	{0         , 0         , 0, NULL   }
};

#endif /* __ASM_MACH_REG_MAP_H
# */
