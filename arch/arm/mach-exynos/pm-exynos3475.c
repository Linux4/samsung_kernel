/*
 * Copyright (c) 2014 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * EXYNOS - Power Management support
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/init.h>
#include <linux/suspend.h>
#include <linux/syscore_ops.h>
#include <linux/io.h>
#include <linux/err.h>
#include <asm/suspend.h>
#include <asm/cacheflush.h>
#include <asm/firmware.h>
#include <linux/delay.h>

#include <mach/exynos-powermode.h>
#include <mach/pmu_cal_sys.h>
#include <mach/pmu.h>
#include <mach/smc.h>
#include <mach/regs-pmu-exynos3475.h>
#include <sound/s2803x.h>

#define REG_INFORM0		(S5P_VA_SYSRAM_NS + 0x8)
#define REG_INFORM1		(S5P_VA_SYSRAM_NS + 0xC)
#define REG_DIRECTGO_FLAG	(S5P_VA_SYSRAM_NS + 0x20)
#define REG_DIRECTGO_ADDR	(S5P_VA_SYSRAM_NS + 0x24)
#define REG_CPU_STATE_ADDR	(S5P_VA_SYSRAM_NS + 0x28)

#define EXYNOS_CHECK_DIRECTGO	0xFCBA0D10

#ifdef CONFIG_SEC_GPIO_DVS
extern void gpio_dvs_check_sleepgpio(void);
#endif

static bool cp_call_mode;

static void exynos_pm_boot_flag(bool enable)
{
	unsigned int val;

	if (enable) {
		val = __raw_readl(REG_CPU_STATE_ADDR);
		val &= ~BOOT_MODE_MASK;
		val |= C2_STATE;
		__raw_writel(val, REG_CPU_STATE_ADDR);
	} else {
		val = __raw_readl(REG_CPU_STATE_ADDR);
		val &= ~BOOT_MODE_MASK;
		__raw_writel(val, REG_CPU_STATE_ADDR);
	}
}

static int exynos_pm_suspend(void)
{
	unsigned int cp_stat;

	cp_stat = __raw_readl(EXYNOS_PMU_CP_STAT);
	if (!cp_stat) {
		printk("%s: sleep canceled by CP reset\n", __func__);
		return -EINVAL;
	} else
		printk("%s: CP_STAT 0x%08x\n", __func__, cp_stat);

	exynos_pm_boot_flag(true);

	/*
	 * Exynos3475 enters into:
	 *	1. LPD when CP Call, else
	 *	2. DSTOP (SLEEP not supported)
	 */
	cp_call_mode = is_cp_aud_enabled();

	exynos_prepare_sys_powerdown(cp_call_mode? SYS_CP_CALL: SYS_DSTOP);

	printk("%s: entering %s\n", __func__, cp_call_mode? "LPD" : "DSTOP");

	return 0;
}

static void exynos_pm_resume(void)
{
	bool early_wakeup = exynos_sys_powerdown_enabled();

	exynos_wakeup_sys_powerdown(cp_call_mode? SYS_CP_CALL : SYS_DSTOP, early_wakeup);
	exynos_pm_boot_flag(false);

	exynos_show_wakeup_reason();
	return;
}

static void exynos_cpu_prepare(void)
{
	__raw_writel(virt_to_phys(cpu_resume), REG_DIRECTGO_ADDR);
	__raw_writel(EXYNOS_CHECK_DIRECTGO, REG_DIRECTGO_FLAG);
}

static int exynos_cpu_suspend(unsigned long arg)
{
	flush_cache_all();	/* FIXME */

	exynos_smc(SMC_CMD_SAVE, OP_TYPE_CORE, SMC_POWERSTATE_SLEEP, 0);
	exynos_smc(SMC_CMD_SHUTDOWN, OP_TYPE_CLUSTER, SMC_POWERSTATE_SLEEP, 0);

	pr_info("%s: return to originator\n", __func__);

	return 1; /* abort suspend */
}

static int exynos_pm_enter(suspend_state_t state)
{
	int ret;

	printk("%s: state %d\n", __func__, state);

	exynos_cpu_prepare();

#ifdef CONFIG_SEC_GPIO_DVS
	/* This function must be located in appropriate SLEEP position
	 * in accordance with the specification of each BB vendor.
	 */
	gpio_dvs_check_sleepgpio();
#endif

	ret = cpu_suspend(0, exynos_cpu_suspend);
	if(ret)
		return ret;

	printk("%s: post sleep, preparing to return\n", __func__);
	return 0;
}

static const struct platform_suspend_ops exynos_pm_ops = {
	.enter		= exynos_pm_enter,
	.valid		= suspend_valid_only_mem,
};

static __init int exynos_pm_drvinit(void)
{
	pr_info("Exynos PM intialize\n");

	suspend_set_ops(&exynos_pm_ops);
	return 0;
}
arch_initcall(exynos_pm_drvinit);

static struct syscore_ops exynos_pm_syscore_ops = {
	.suspend	= exynos_pm_suspend,
	.resume		= exynos_pm_resume,
};

static __init int exynos_pm_syscore_init(void)
{
	pr_info("Exynos PM syscore intialize\n");

	register_syscore_ops(&exynos_pm_syscore_ops);
	return 0;
}
arch_initcall(exynos_pm_syscore_init);
