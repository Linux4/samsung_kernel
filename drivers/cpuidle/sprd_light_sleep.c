/*
 * Lightsleep config for Spreadtrum.
 *
 * Copyright (C) 2015 Spreadtrum Ltd.
 * Author: Icy Zhu <icy.zhu@spreadtrum.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/export.h>
#include <linux/sched.h>

#include <soc/sprd/hardware.h>
#include <soc/sprd/sci_glb_regs.h>
#include <soc/sprd/sci.h>
#include <soc/sprd/cpuidle.h>

#define INTC_IRQ_EN			(0x0008)
#define INTC_IRQ_DIS        (0x000C)

/* For MCU_SYS_SLEEP Prepare */
static BLOCKING_NOTIFIER_HEAD(sc_cpuidle_chain_head);
#if defined(CONFIG_ARCH_SCX35LT8)
static int light_sleep = 0;
#else
static int light_sleep = 1;
#endif
static int trace_debug = 0;
static int test_power = 0;
static int mcu_sleep_debug = 0;

/* Enable Lightsleep Function */
module_param_named(light_sleep, light_sleep, int, S_IRUGO | S_IWUSR);
/* Used to trace32 debug, because the value of trace32 is jumpy-changing while DAP clock be gatenning */
module_param_named(trace_debug, trace_debug, int, S_IRUGO | S_IWUSR);
/* Used to lightsleep state power test,if test_power be set to 1, the system will keep lightsleep state until to press powerkey */
module_param_named(test_power, test_power, int, S_IRUGO | S_IWUSR);
/* Used to MCU_SYS_SLEEP debug */
module_param_named(mcu_sleep_debug, mcu_sleep_debug, int, S_IRUGO | S_IWUSR);

int register_sc_cpuidle_notifier(struct notifier_block *nb)
{
	printk("*** %s, nb->notifier_call:%pf ***\n", __func__, nb->notifier_call);
	return blocking_notifier_chain_register(&sc_cpuidle_chain_head, nb);
}
EXPORT_SYMBOL_GPL(register_sc_cpuidle_notifier);

int unregister_sc_cpuidle_notifier(struct notifier_block *nb)
{
	return blocking_notifier_chain_unregister(&sc_cpuidle_chain_head, nb);
}
EXPORT_SYMBOL_GPL(unregister_sc_cpuidle_notifier);

int sc_cpuidle_notifier_call_chain(unsigned long val)
{
	int ret = blocking_notifier_call_chain(&sc_cpuidle_chain_head, val, NULL);
	return notifier_to_errno(ret);
}
EXPORT_SYMBOL_GPL(sc_cpuidle_notifier_call_chain);

void light_sleep_en(void)
{
	int error;
	if (light_sleep) {
		if (test_power) {
			/* Disable All INTC for test lightsleep power */
			__raw_writel(0xffffffef, (void *)(SPRD_INT_BASE + INTC_IRQ_DIS));
			__raw_writel(0xffffffff, (void *)(SPRD_INTC0_BASE + INTC_IRQ_DIS));
			__raw_writel(0xffffffff, (void *)(SPRD_INTC1_BASE + INTC_IRQ_DIS));
			__raw_writel(0xffffffff, (void *)(SPRD_INTC2_BASE + INTC_IRQ_DIS));
			__raw_writel(0xffffffff, (void *)(SPRD_INTC3_BASE + INTC_IRQ_DIS));
		}
		/* Cluster Gating */
		sci_glb_set(REG_AP_AHB_APCPU_AUTO_GATE_EN, BIT_APCPU_AUTO_GATE_EN);
		/* Light sleep */
		sci_glb_set(REG_AP_AHB_MCU_PAUSE, BIT_MCU_LIGHT_SLEEP_EN);
		/* Auto Gating */
		if (!trace_debug) {
			sci_glb_set(REG_AP_AHB_AP_SYS_AUTO_SLEEP_CFG, BIT_APCPU_DBG_AUTO_GATE_EN);
			sci_glb_set(REG_AP_AHB_AP_SYS_AUTO_SLEEP_CFG, BIT_APCPU_TRACE_AUTO_GATE_EN);
			/* Disable DAP */
			sci_glb_clr(REG_AON_APB_APB_EB0, BIT_APCPU_DAP_EB);
		}
		/* MCU sys sleep prepare */
		if (!(sci_glb_read(REG_AP_AHB_AHB_EB, -1UL) & BIT_DMA_EB)) {
			error = sc_cpuidle_notifier_call_chain(SC_CPUIDLE_PREPARE);
			if (error) {
				sci_glb_clr(REG_AP_AHB_MCU_PAUSE, BIT_MCU_SYS_SLEEP_EN);
				if (mcu_sleep_debug) {
					printk("[CPUIDLE]<mcu_sleep_pre>could not set %s ... \n", __func__);
				}
				return;
			}
			/* Enable MCU sys sleep */
			sci_glb_set(REG_AP_AHB_MCU_PAUSE, BIT_MCU_SYS_SLEEP_EN);
		}
	}
}
EXPORT_SYMBOL_GPL(light_sleep_en);

void light_sleep_dis(void)
{
	if (light_sleep) {
		/* Enable DAP */
		sci_glb_set(REG_AON_APB_APB_EB0, BIT_APCPU_DAP_EB);
		sci_glb_clr(REG_AP_AHB_AP_SYS_AUTO_SLEEP_CFG, BIT_APCPU_DBG_AUTO_GATE_EN);
		sci_glb_clr(REG_AP_AHB_AP_SYS_AUTO_SLEEP_CFG, BIT_APCPU_TRACE_AUTO_GATE_EN);
		/* Disable MCU sys sleep */
		if (test_power) {
			test_power = 0;
			sci_glb_set(SPRD_INT_BASE + INTC_IRQ_EN, BIT(14) | BIT(4) | BIT(2));
			__raw_writel(0x0010000c, (void *)(SPRD_INTC0_BASE + INTC_IRQ_EN));
			__raw_writel(0xf68f4cd0, (void *)(SPRD_INTC1_BASE + INTC_IRQ_EN));
			__raw_writel(0x00500000, (void *)(SPRD_INTC2_BASE + INTC_IRQ_EN));
			__raw_writel(0x11000000, (void *)(SPRD_INTC3_BASE + INTC_IRQ_EN));
		}
	}
}
EXPORT_SYMBOL_GPL(light_sleep_dis);

void __init light_sleep_init(void)
{
	/* Cluster Gating */
	sci_glb_set(REG_AP_AHB_AP_SYS_AUTO_SLEEP_CFG, BIT_APCPU_CORE_AUTO_GATE_EN);
	/* AP_AHB clock Auto gating while ap doesn't into mcu_sys_sleep */
	sci_glb_set(REG_AP_AHB_AP_SYS_AUTO_SLEEP_CFG, BIT_AP_AHB_AUTO_GATE_EN);
	/* For wakeup lightsleep */
	sci_glb_set(REG_PMU_APB_AP_WAKEUP_POR_CFG, BIT_AP_WAKEUP_POR_N);
	return;
}
arch_initcall(light_sleep_init);
