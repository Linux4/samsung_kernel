/*
 * linux/drivers/misc/arm-coresight.c
 *
 * Author:	Neil Zhang <zhangwm@marvell.com>
 * Copyright:	(C) 2012 Marvell International Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/of.h>
#include <linux/cpu.h>
#include <linux/clk.h>
#include <linux/init.h>
#include <linux/cpu_pm.h>
#include <linux/notifier.h>
#include <linux/arm-coresight.h>
#include <linux/platform_device.h>

static struct clk *dbgclk;

static inline int enable_debug_clock(u32 cpu)
{
	if (!dbgclk) {
		pr_emerg("No DBGCLK is found!\n");
		return -1;
	}

	if (clk_enable(dbgclk)) {
		pr_emerg("No DBGCLK is found!\n");
		return -1;
	}

	return 0;
}

void coresight_dump_pcsr(u32 cpu)
{
	if (enable_debug_clock(cpu))
		return;

	arch_enable_access(cpu);
	arch_dump_pcsr(cpu);
}

void coresight_trigger_panic(int cpu)
{
	if (enable_debug_clock(cpu))
		return;

	arch_enable_access(cpu);
	arch_dump_pcsr(cpu);

	/* Halt the dest cpu */
	pr_emerg("Going to halt cpu%d\n", cpu);
	if (arch_halt_cpu(cpu)) {
		pr_emerg("Cannot halt cpu%d\n", cpu);
		return;
	}

	pr_emerg("Going to insert inst on cpu%d\n", cpu);
	arch_insert_inst(cpu);

	/* Restart target cpu */
	pr_emerg("Going to restart cpu%d\n", cpu);
	arch_restart_cpu(cpu);
}

static int coresight_core_notifier(struct notifier_block *nfb,
				      unsigned long action, void *hcpu)
{
	switch (action & ~CPU_TASKS_FROZEN) {
	case CPU_STARTING:
		arch_restore_coreinfo();
		return NOTIFY_OK;

	case CPU_DYING:
		arch_save_coreinfo();
		return NOTIFY_OK;

	default:
		return NOTIFY_DONE;
	}
}

static int coresight_notifier(struct notifier_block *self,
				unsigned long cmd, void *v)
{
	switch (cmd) {
	case CPU_PM_ENTER:
		arch_save_coreinfo();
		break;
	case CPU_PM_ENTER_FAILED:
	case CPU_PM_EXIT:
		arch_restore_coreinfo();
		break;
	case CPU_CLUSTER_PM_ENTER:
		arch_save_mpinfo();
		break;
	case CPU_CLUSTER_PM_ENTER_FAILED:
	case CPU_CLUSTER_PM_EXIT:
		arch_restore_mpinfo();
		break;
	}

	return NOTIFY_OK;
}

static struct notifier_block coresight_notifier_block = {
	.notifier_call = coresight_notifier,
};

#ifdef CONFIG_CORESIGHT_TRACE_SUPPORT
static u32 etm_enable_mask = (1 << CONFIG_NR_CPUS) - 1;
static int __init __init_etm_trace(char *arg)
{
	u32 cpu_mask;

	if (!get_option(&arg, &cpu_mask))
		return 0;

	etm_enable_mask &= cpu_mask;

	return 1;
}
__setup("etm_trace=", __init_etm_trace);


static inline int etm_need_enabled(void)
{
	return !!etm_enable_mask;
}
#endif

static int arm_coresight_probe(struct platform_device *pdev)
{
#ifdef CONFIG_CORESIGHT_TRACE_SUPPORT
	static struct clk *traceclk;
#endif

	dbgclk = clk_get(&pdev->dev, "DBGCLK");
	if (IS_ERR(dbgclk)) {
		pr_warn("No DBGCLK is defined...\n");
		dbgclk = NULL;
	}

	if (dbgclk)
		clk_prepare(dbgclk);

	arch_coresight_init();

#ifdef CONFIG_CORESIGHT_TRACE_SUPPORT

	/* enable etm trace by default */
	if (etm_need_enabled()) {
		traceclk = clk_get(&pdev->dev, "TRACECLK");
		if (IS_ERR(traceclk)) {
			pr_warn("No TRACECLK is defined...\n");
			traceclk = NULL;
		}
		if (traceclk)
			clk_prepare_enable(traceclk);

		arch_enable_trace(etm_enable_mask);
	}
#endif

	cpu_notifier(coresight_core_notifier, 0);
	cpu_pm_register_notifier(&coresight_notifier_block);

	return 0;
}


static struct of_device_id arm_coresight_dt_ids[] = {
	{ .compatible = "marvell,coresight", },
	{}
};

static struct platform_driver arm_coresight_driver = {
	.probe		= arm_coresight_probe,
	.driver		= {
		.name	= "arm-coresight",
		.of_match_table = of_match_ptr(arm_coresight_dt_ids),
	},
};

static int __init arm_coresight_init(void)
{
	return platform_driver_register(&arm_coresight_driver);
}

arch_initcall(arm_coresight_init);
