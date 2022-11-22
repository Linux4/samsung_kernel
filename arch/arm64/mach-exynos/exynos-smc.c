/*
 * Copyright (c) 2015 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * EXYNOS SMC
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#include <linux/sched.h>
#include <linux/spinlock.h>
#include <linux/smc.h>

#define CONFIG_EXYNOS_SMC_LOGGING

#ifdef CONFIG_EXYNOS_SMC_LOGGING
#define EXYNOS_SMC_CMD_MASK	0xFFFF0000
#define EXYNOS_SMC_LOG_SIZE	256
struct exynos_smc_log_entry {
        u64 cpu_clk;
        u32 cmd;
	u32 arg1;
	u32 arg2;
	u32 arg3;
};

static DEFINE_SPINLOCK(sip_smc_log_lock);
static DEFINE_SPINLOCK(oem_smc_log_lock);

struct exynos_smc_log_entry sip_smc_log[EXYNOS_SMC_LOG_SIZE];
struct exynos_smc_log_entry oem_smc_log[EXYNOS_SMC_LOG_SIZE];

static unsigned int sip_smc_log_idx;
static unsigned int oem_smc_log_idx;
#endif

int exynos_smc(unsigned long cmd, unsigned long arg1, unsigned long arg2, unsigned long arg3)
{
#ifdef CONFIG_EXYNOS_SMC_LOGGING
	unsigned long flags_sip;
	unsigned long flags_oem;
#endif
	int ret;

#ifdef CONFIG_EXYNOS_SMC_LOGGING
	if ((cmd & EXYNOS_SMC_CMD_MASK) == SMC_SIP_BASE) {
		pr_debug("%s: cmd: 0x%x, arg1: 0x%x, arg2: 0x%x, arg3: 0x%x\n",
			__func__, (u32)cmd, (u32)arg1, (u32)arg2, (u32)arg3);
		spin_lock_irqsave(&sip_smc_log_lock, flags_sip);
		sip_smc_log[sip_smc_log_idx].cpu_clk = local_clock();
		sip_smc_log[sip_smc_log_idx].cmd = (u32)cmd;
		sip_smc_log[sip_smc_log_idx].arg1 = (u32)arg1;
		sip_smc_log[sip_smc_log_idx].arg2 = (u32)arg2;
		sip_smc_log[sip_smc_log_idx].arg3 = (u32)arg3;
		sip_smc_log_idx++;
		if (sip_smc_log_idx == EXYNOS_SMC_LOG_SIZE)
			sip_smc_log_idx = 0;
		spin_unlock_irqrestore(&sip_smc_log_lock, flags_sip);
	} else if ((cmd & EXYNOS_SMC_CMD_MASK) == SMC_OEM_BASE) {
		pr_debug("%s: cmd: 0x%x, arg1: 0x%x, arg2: 0x%x, arg3: 0x%x\n",
			__func__, (u32)cmd, (u32)arg1, (u32)arg2, (u32)arg3);
		spin_lock_irqsave(&oem_smc_log_lock, flags_oem);
		oem_smc_log[oem_smc_log_idx].cpu_clk = local_clock();
		oem_smc_log[oem_smc_log_idx].cmd = (u32)cmd;
		oem_smc_log[oem_smc_log_idx].arg1 = (u32)arg1;
		oem_smc_log[oem_smc_log_idx].arg2 = (u32)arg2;
		oem_smc_log[oem_smc_log_idx].arg3 = (u32)arg3;
		oem_smc_log_idx++;
		if (oem_smc_log_idx == EXYNOS_SMC_LOG_SIZE)
			oem_smc_log_idx = 0;
		spin_unlock_irqrestore(&oem_smc_log_lock, flags_oem);
	}
#endif

	ret = _exynos_smc(cmd, arg1, arg2, arg3);

	return ret;
}