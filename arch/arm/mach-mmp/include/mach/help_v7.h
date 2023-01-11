/*
 * linux/arch/arm/mach-mmp/include/mach/help_v7.h
 *
 * Author:	Fangsuo Wu <fswu@marvell.com>
 * Copyright:	(C) 2012 Marvell International Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#ifndef __MMP_MACH_HELP_V7_H__
#define __MMP_MACH_HELP_V7_H__

#include <asm/cp15.h>
#include <asm/cputype.h>

#ifdef CONFIG_SMP
static inline void core_exit_coherency(void)
{
	unsigned int v;
	asm volatile(
	"       mrc     p15, 0, %0, c1, c0, 1\n"
	"       bic     %0, %0, #(1 << 6)\n"
	"       mcr     p15, 0, %0, c1, c0, 1\n"
	: "=&r" (v) : : "cc");
	isb();
}

static inline void core_enter_coherency(void)
{
	unsigned int v;
	asm volatile(
	"       mrc     p15, 0, %0, c1, c0, 1\n"
	"       orr     %0, %0, #(1 << 6)\n"
	"       mcr     p15, 0, %0, c1, c0, 1\n"
	: "=&r" (v) : : "cc");
	isb();
}
#endif

static inline void disable_l1_dcache(void)
{
	unsigned int v;
	asm volatile(
	"       mrc     p15, 0, %0, c1, c0, 0\n"
	"       bic     %0, %0, %1\n"
	"       mcr     p15, 0, %0, c1, c0, 0\n"
#ifdef CONFIG_ARM_ERRATA_794322
	"	ldr	%0, =_stext\n"
	"	mcr     p15, 0, %0, c8, c7, 1\n"
	"	dsb\n"
#endif
	: "=&r" (v) : "Ir" (CR_C) : "cc");
	isb();
}

static inline void enable_l1_dcache(void)
{
	unsigned int v;
	asm volatile(
	"       mrc     p15, 0, %0, c1, c0, 0\n"
	"       orr     %0, %0, %1\n"
	"       mcr     p15, 0, %0, c1, c0, 0\n"
	: "=&r" (v) : "Ir" (CR_C) : "cc");
	isb();
}

static inline void ca7_power_down(void)
{
	disable_l1_dcache();
	flush_cache_louis();
	asm volatile("clrex\n");
	core_exit_coherency();
}

static inline void ca7_power_down_udr(void)
{
	disable_l1_dcache();
	flush_cache_louis();
	asm volatile("clrex\n");
	flush_cache_all();
	core_exit_coherency();
}
#endif
