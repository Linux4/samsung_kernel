/*
 *sec_debug_test.c
 *
 * Copyright (c) 2016 Samsung Electronics Co., Ltd
 *              http://www.samsung.com
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include <linux/cpu.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <asm-generic/io.h>
#include <linux/ctype.h>
#include <linux/pm_qos.h>
#include <linux/sec_debug.h>
#include <linux/kthread.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/preempt.h>
#include <linux/moduleparam.h>

/* Override the default prefix for the compatibility with other models */
#undef MODULE_PARAM_PREFIX
#define MODULE_PARAM_PREFIX "sec_debug."

typedef void (*force_error_func)(char *arg);

static void simulate_KP(char *arg);
static void simulate_DP(char *arg);
static void simulate_PANIC(char *arg);
static void simulate_BUG(char *arg);
static void simulate_WARN(char *arg);
static void simulate_WQ_LOCKUP(char *arg);

enum {
	FORCE_KERNEL_PANIC = 0,		/* KP */
	FORCE_WATCHDOG,				/* DP */
	FORCE_PANIC,				/* PANIC */
	FORCE_BUG,					/* BUG */
	FORCE_WARN,					/* WARN */
	FORCE_WQ_LOCKUP,			/* WORKQUEUE LOCKUP */
	NR_FORCE_ERROR,
};

struct force_error_item {
	char errname[SZ_32];
	force_error_func errfunc;
};

struct force_error {
	struct force_error_item item[NR_FORCE_ERROR];
};

struct force_error force_error_vector = {
	.item = {
		{"KP",		&simulate_KP},
		{"DP",		&simulate_DP},
		{"panic",	&simulate_PANIC},
		{"bug",		&simulate_BUG},
		{"warn",	&simulate_WARN},
		{"wqlockup",	&simulate_WQ_LOCKUP},
	}
};

static struct work_struct lockup_work;

static int str_to_num(char *s)
{
	if (s) {
		switch (s[0]) {
		case '0':
		case '1':
		case '2':
		case '3':
		case '4':
		case '5':
		case '6':
		case '7':
			return (s[0] - '0');

		default:
			return -1;
		}
	}
	return -1;
}

/* timeout for dog bark/bite */
#define DELAY_TIME 30000

static void pull_down_other_cpus(void)
{
#ifdef CONFIG_HOTPLUG_CPU
	int cpu, ret;

	for (cpu = NR_CPUS - 1; cpu > 0 ; cpu--) {
		ret = cpu_down(cpu);
		if (ret)
			pr_crit("%s: CORE%d ret: %x\n", __func__, cpu, ret);
	}
#endif
}

static void simulate_KP(char *arg)
{
	pr_crit("%s()\n", __func__);
	*(volatile unsigned int *)0x0 = 0x0; /* SVACE: intended */
}

static void simulate_DP(char *arg)
{
	pr_crit("%s()\n", __func__);

	pull_down_other_cpus();

	pr_crit("%s() start to hanging\n", __func__);
	local_irq_disable();
	mdelay(DELAY_TIME);
	local_irq_enable();

	/* should not reach here */
}

static void simulate_PANIC(char *arg)
{
	pr_crit("%s()\n", __func__);
	panic("simulate_panic");
}

static void simulate_BUG(char *arg)
{
	pr_crit("%s()\n", __func__);
	BUG();
}

static void simulate_WARN(char *arg)
{
	pr_crit("%s()\n", __func__);
	WARN_ON(1);
}

static void sec_debug_wq_lockup(struct work_struct *work)
{
	pr_crit("%s\n", __func__);
	asm("b .");
}

static void simulate_WQ_LOCKUP(char *arg)
{
	int cpu;

	pr_crit("%s()\n", __func__);
	INIT_WORK(&lockup_work, sec_debug_wq_lockup);

	if (arg) {
		cpu = str_to_num(arg);

		if (cpu >= 0 && cpu <= num_possible_cpus() - 1) {
			pr_crit("Put works into cpu%d\n", cpu);
			schedule_work_on(cpu, &lockup_work);
		}
	}
}

static int sec_debug_get_force_error(char *buffer, const struct kernel_param *kp)
{
	int i;
	int size = 0;

	for (i = 0; i < NR_FORCE_ERROR; i++)
		size += scnprintf(buffer + size, PAGE_SIZE - size, "%s\n",
				  force_error_vector.item[i].errname);

	return size;
}

static int sec_debug_set_force_error(const char *val, const struct kernel_param *kp)
{
	int i;
	char *temp;
	char *ptr;

	for (i = 0; i < NR_FORCE_ERROR; i++)
		if (!strncmp(val, force_error_vector.item[i].errname,
			     strlen(force_error_vector.item[i].errname))) {
			temp = (char *)val;
			ptr = strsep(&temp, " ");	/* ignore the first token */
			ptr = strsep(&temp, " ");	/* take the second token */
			force_error_vector.item[i].errfunc(ptr);
	}
	return 0;
}

static const struct kernel_param_ops sec_debug_force_error_ops = {
	.set	= sec_debug_set_force_error,
	.get	= sec_debug_get_force_error,
};

module_param_cb(force_error, &sec_debug_force_error_ops, NULL, 0600);
