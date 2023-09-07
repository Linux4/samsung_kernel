/*
 * kexec for arm64
 *
 * Copyright (C) Linaro.
 * Copyright (C) Huawei Futurewei Technologies.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _ARM64_KEXEC_H
#define _ARM64_KEXEC_H

/* Maximum physical address we can use pages from */

#define KEXEC_SOURCE_MEMORY_LIMIT (-1UL)

/* Maximum address we can reach in physical address mode */

#define KEXEC_DESTINATION_MEMORY_LIMIT (-1UL)

/* Maximum address we can use for the control code buffer */

#define KEXEC_CONTROL_MEMORY_LIMIT (-1UL)

#define KEXEC_CONTROL_PAGE_SIZE 4096

#define KEXEC_ARCH KEXEC_ARCH_AARCH64

#ifndef __ASSEMBLY__

/**
 * crash_setup_regs() - save registers for the panic kernel
 *
 * @newregs: registers are saved here
 * @oldregs: registers to be saved (may be %NULL)
 */

static inline void crash_setup_regs(struct pt_regs *newregs,
				    struct pt_regs *oldregs)
{
	if (oldregs)
		memcpy(newregs, oldregs, sizeof(*newregs));
	else {
		asm volatile (
			"stp x0, x1, [%0, #8 * 0]\n\t"
			"stp x2, x3, [%0, #8 * 2]\n\t"
			"stp x4, x5, [%0, #8 * 4]\n\t"
			"stp x6, x7, [%0, #8 * 6]\n\t"
			"stp x8, x9, [%0, #8 * 8]\n\t"
			"stp x10, x11, [%0, #8 * 10]\n\t"
			"stp x12, x13, [%0, #8 * 12]\n\t"
			"stp x14, x15, [%0, #8 * 14]\n\t"
			"stp x16, x17, [%0, #8 * 16]\n\t"
			"stp x18, x19, [%0, #8 * 18]\n\t"
			"stp x20, x21, [%0, #8 * 20]\n\t"
			"stp x22, x23, [%0, #8 * 22]\n\t"
			"stp x24, x25, [%0, #8 * 24]\n\t"
			"stp x26, x27, [%0, #8 * 26]\n\t"
			"stp x28, x29, [%0, #8 * 28]\n\t"
			"stp x5, x6, [sp,#-32]!\n\t"
			"str x7, [sp,#16]\n\t"
			"mov x5, sp\n\t"
			"stp x30, x5, [%0, #8 * 30]\n\t"
			"adr x6, 1f\n\t"
			"mrs x7, spsr_el1\n\t"
			"stp x6, x7, [%0, #8 * 32]\n\t"
			"ldr x7, [sp,#16]\n\t"
			"ldp x5, x6, [sp],#32\n\t"
		"1:"
			:
			: "r" (&newregs->regs[0])
			: "memory"
		);
	}
}

#endif /* __ASSEMBLY__ */

#endif
