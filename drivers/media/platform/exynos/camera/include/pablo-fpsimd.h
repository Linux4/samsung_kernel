// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * Copyright (c) 2022 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef IS_FPSIMD_H
#define IS_FPSIMD_H

#if defined(ENABLE_FPSIMD_FOR_USER)
#if defined(VH_FPSIMD_API) /* Using android vendor hook to save and restore fpsimd regs */

struct is_fpsimd_state {
	__uint128_t	vregs[32];
	__u32		fpsr;
	__u32		fpcr;
	__u32		__reserved[2];
};

struct is_kernel_fpsimd_state {
	struct is_fpsimd_state pre;
	struct is_fpsimd_state cur;
	atomic_t depth;
};

extern void is_fpsimd_save_state(struct is_fpsimd_state *state);
extern void is_fpsimd_load_state(struct is_fpsimd_state *state);
void is_fpsimd_get_isr(void);
void is_fpsimd_put_isr(void);
void is_fpsimd_get_func(void);
void is_fpsimd_put_func(void);
void is_fpsimd_get_task(void);
void is_fpsimd_put_task(void);
void is_fpsimd_set_task_using(struct task_struct *t, struct is_kernel_fpsimd_state *kst);
int is_fpsimd_probe(void);
#else
#define is_fpsimd_get_isr()			kernel_neon_begin()
#define is_fpsimd_put_isr()			kernel_neon_end()
#define is_fpsimd_get_func()			fpsimd_get()
#define is_fpsimd_put_func()			fpsimd_put()
#define is_fpsimd_get_task()			do { } while(0)
#define is_fpsimd_put_task()			do { } while(0)
#define is_fpsimd_set_task_using(t, kst)	fpsimd_set_task_using(t)
#define is_fpsimd_probe()			({0;})
#endif
#else
#define is_fpsimd_get_isr()			do { } while(0)
#define is_fpsimd_put_isr()			do { } while(0)
#define is_fpsimd_get_func()			do { } while(0)
#define is_fpsimd_put_func()			do { } while(0)
#define is_fpsimd_get_task()			do { } while(0)
#define is_fpsimd_put_task()			do { } while(0)
#define is_fpsimd_set_task_using(t, kst)	do { } while(0)
#define is_fpsimd_probe()			({0;})
#endif

void _is_clean_dcache_area(void *kaddr, u32 size);
#endif
