/*
 * arch/arm/mach-pxa/include/mach/dump_regs.h
 *
 * PXA988 Dump Soc registers and stack Head File
 *
 * Copyright (C) 2012 Marvell International Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __DUMP_REGS_H__
#define __DUMP_REGS_H__

struct reg_map {
	const unsigned int beg_addr;
	const unsigned int end_addr;
	unsigned int       vir_addr;
	const char * const reg_name;
};

extern struct reg_map pxa_reg_map[];
extern void dump_soc_regs(struct reg_map map[]);
extern void dump_stack_and_func(void);
extern unsigned int _stext, _etext;

#endif
