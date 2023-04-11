// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * Copyright (c) 2020 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <asm/fpsimd.h>
#include <asm/neon.h>

#include "is-hw.h"
#include "is-config.h"

#if defined(ENABLE_FPSIMD_FOR_USER)
#if defined(LOCAL_FPSIMD_API)
#define NUM_OF_MAX_CORE		8
static struct is_fpsimd_state isr_fpsimd_state[NUM_OF_MAX_CORE];
static struct is_fpsimd_state func_fpsimd_state[NUM_OF_MAX_CORE];
static struct is_fpsimd_state task_fpsimd_state[NUM_OF_MAX_CORE];

void is_fpsimd_get_isr(void)
{
	is_fpsimd_save_state(&isr_fpsimd_state[smp_processor_id()]);
}

void is_fpsimd_put_isr(void)
{
	is_fpsimd_load_state(&isr_fpsimd_state[smp_processor_id()]);
}

void is_fpsimd_get_func(void)
{
	local_irq_disable();
	preempt_disable();

	is_fpsimd_save_state(&func_fpsimd_state[smp_processor_id()]);
}

void is_fpsimd_put_func(void)
{
	is_fpsimd_load_state(&func_fpsimd_state[smp_processor_id()]);

	preempt_enable();
	local_irq_enable();
}

void is_fpsimd_get_task(void)
{
	preempt_disable();

	is_fpsimd_save_state(&task_fpsimd_state[smp_processor_id()]);
}

void is_fpsimd_put_task(void)
{
	is_fpsimd_load_state(&task_fpsimd_state[smp_processor_id()]);

	preempt_enable();
}

void is_fpsimd_set_task_using(struct task_struct *t)
{
	return;
}
#else
void is_fpsimd_get_isr(void)
{
	kernel_neon_begin();
}

void is_fpsimd_put_isr(void)
{
	kernel_neon_end();
}

void is_fpsimd_get_func(void)
{
	fpsimd_get();
}

void is_fpsimd_put_func(void)
{
	fpsimd_put();
}

void is_fpsimd_get_task(void)
{
}

void is_fpsimd_put_task(void)
{
}

void is_fpsimd_set_task_using(struct task_struct *t)
{
	fpsimd_set_task_using(t);
}
#endif
#endif
