/*
 * Copyright (c) 2013 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Debug-SnapShot: Debug Framework for Ramdump based debugging method
 * The original code is Exynos-Snapshot for Exynos SoC
 *
 * Author: Hosung Kim <hosung0.kim@samsung.com>
 * Author: Changki Kim <changki.kim@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/io.h>
#include <linux/notifier.h>
#include <linux/reboot.h>
#include <linux/delay.h>
#include <linux/kallsyms.h>
#include <linux/input.h>
#include <linux/smc.h>
#include <linux/bitops.h>
#include <linux/sched/clock.h>
#include <linux/sched/debug.h>
#include <linux/nmi.h>
#include <linux/init_task.h>
#include <linux/ftrace.h>

#include <asm/cputype.h>
#include <asm/smp_plat.h>
#include <asm/core_regs.h>

#include "debug-snapshot-local.h"
#include <linux/debug-snapshot.h>
#include <linux/debug-snapshot-helper.h>

#include <linux/sec_debug.h>

static void dbg_snapshot_soc_dummy_func(void *dummy) {return;}
static int dbg_snapshot_soc_dummy_func_int(unsigned int dummy) {return 0;}
static int dbg_snapshot_soc_dummy_func_smc(unsigned long dummy1,
					unsigned long dummy2,
					unsigned long dummy3,
					unsigned long dummy4) {return 0;}

static struct dbg_snapshot_helper_ops dss_soc_dummy_ops = {
	.soc_early_panic		= dbg_snapshot_soc_dummy_func,
	.soc_prepare_panic_entry	= dbg_snapshot_soc_dummy_func,
	.soc_prepare_panic_exit		= dbg_snapshot_soc_dummy_func,
	.soc_post_panic_entry		= dbg_snapshot_soc_dummy_func,
	.soc_post_panic_exit		= dbg_snapshot_soc_dummy_func,
	.soc_post_reboot_entry		= dbg_snapshot_soc_dummy_func,
	.soc_post_reboot_exit		= dbg_snapshot_soc_dummy_func,
	.soc_save_context_entry		= dbg_snapshot_soc_dummy_func,
	.soc_save_context_exit		= dbg_snapshot_soc_dummy_func,
	.soc_save_core			= dbg_snapshot_soc_dummy_func,
	.soc_save_system		= dbg_snapshot_soc_dummy_func,
	.soc_dump_info			= dbg_snapshot_soc_dummy_func,
	.soc_start_watchdog		= dbg_snapshot_soc_dummy_func,
	.soc_expire_watchdog		= dbg_snapshot_soc_dummy_func,
	.soc_stop_watchdog		= dbg_snapshot_soc_dummy_func,
	.soc_kick_watchdog		= dbg_snapshot_soc_dummy_func,
	.soc_is_power_cpu		= dbg_snapshot_soc_dummy_func_int,
	.soc_smc_call			= dbg_snapshot_soc_dummy_func_smc,
	.soc_do_dpm_policy		= dbg_snapshot_soc_dummy_func,
};

struct dbg_snapshot_helper_ops *dss_soc_ops;

void __iomem *dbg_snapshot_get_header_vaddr(void)
{
	if (dbg_snapshot_get_enable_item(DSS_ITEM_HEADER))
		return (void __iomem *)(dss_items[DSS_ITEM_HEADER_ID].entry.vaddr);
	else
		return (void __iomem *)(0);
}

void __iomem *dbg_snapshot_get_base_vaddr(void)
{
	if (dbg_snapshot_get_enable())
		return (void __iomem *)(dss_base.vaddr);
	else
		return (void __iomem *)(0);
}

void __iomem *dbg_snapshot_get_base_paddr(void)
{
	if (dbg_snapshot_get_enable())
		return (void __iomem *)(dss_base.paddr);
	else
		return (void __iomem *)(0);
}

static void dbg_snapshot_set_core_power_stat(unsigned int val, unsigned cpu)
{
	if (dbg_snapshot_get_enable())
		__raw_writel(val, (dbg_snapshot_get_base_vaddr() +
					DSS_OFFSET_CORE_POWER_STAT + cpu * 4));
}

unsigned int dbg_snapshot_get_core_panic_stat(unsigned cpu)
{
	if (dbg_snapshot_get_enable())
		return __raw_readl(dbg_snapshot_get_base_vaddr() +
					DSS_OFFSET_PANIC_STAT + cpu * 4);
	else
		return 0;
}

void dbg_snapshot_set_core_pmu_val(unsigned int val, unsigned int cpu)
{
	if (dbg_snapshot_get_enable())
		__raw_writel(val, (dbg_snapshot_get_base_vaddr() +
					DSS_OFFSET_CORE_PMU_VAL + cpu * 4));
}

unsigned int dbg_snapshot_get_core_pmu_val(unsigned int cpu)
{
	if (dbg_snapshot_get_enable())
		return __raw_readl(dbg_snapshot_get_base_vaddr() +
					DSS_OFFSET_CORE_PMU_VAL + cpu * 4);
	else
		return 0;
}

void dbg_snapshot_set_core_ehld_stat(unsigned int val, unsigned int cpu)
{
	if (dbg_snapshot_get_enable())
		__raw_writel(val, (dbg_snapshot_get_base_vaddr() +
					DSS_OFFSET_CORE_EHLD_STAT + cpu * 4));
}

unsigned int dbg_snapshot_get_core_ehld_stat(unsigned int cpu)
{
	if (dbg_snapshot_get_enable())
		return __raw_readl(dbg_snapshot_get_base_vaddr() +
					DSS_OFFSET_CORE_EHLD_STAT + cpu * 4);
	else
		return 0;
}

void dbg_snapshot_set_core_panic_stat(unsigned int val, unsigned cpu)
{
	if (dbg_snapshot_get_enable())
		__raw_writel(val, (dbg_snapshot_get_base_vaddr() +
					DSS_OFFSET_PANIC_STAT + cpu * 4));
}

static void dbg_snapshot_report_reason(unsigned int val)
{
	if (dbg_snapshot_get_enable())
		__raw_writel(val, dbg_snapshot_get_base_vaddr() + DSS_OFFSET_EMERGENCY_REASON);
}

int dbg_snapshot_get_debug_level_reg(void)
{
	int ret = DSS_DEBUG_LEVEL_MID;

	if (dbg_snapshot_get_enable()) {
		int val = __raw_readl(dbg_snapshot_get_base_vaddr() + DSS_OFFSET_DEBUG_LEVEL);

		if ((val & GENMASK(31, 16)) == DSS_DEBUG_LEVEL_PREFIX)
			ret = val & GENMASK(15, 0);
	}

	return ret;
}

void dbg_snapshot_set_sjtag_status(void)
{
	int ret;

	ret = dss_soc_ops->soc_smc_call(SMC_CMD_GET_SJTAG_STATUS, 0x3, 0, 0);

	if (ret == true || ret == false) {
		dss_desc.sjtag_status = ret;
		pr_info("debug-snapshot: SJTAG is %sabled\n",
				ret == true ? "en" : "dis");
		return;
	}

	dss_desc.sjtag_status = -1;
}

int dbg_snapshot_get_sjtag_status(void)
{
	return dss_desc.sjtag_status;
}

void dbg_snapshot_scratch_reg(unsigned int val)
{
	if (dbg_snapshot_get_enable())
		__raw_writel(val, dbg_snapshot_get_base_vaddr() + DSS_OFFSET_SCRATCH);
}

void dbg_snapshot_scratch_clear(void)
{
	if (dbg_snapshot_get_enable())
		__raw_writel(DSS_SIGN_RESET, dbg_snapshot_get_base_vaddr() + DSS_OFFSET_SCRATCH);
}

bool dbg_snapshot_is_scratch(void)
{
	if (dbg_snapshot_get_enable())
		return __raw_readl(dbg_snapshot_get_base_vaddr() +
				DSS_OFFSET_SCRATCH) == DSS_SIGN_SCRATCH;
	else
		return false;
}

void dbg_snapshot_set_debug_test_reg(unsigned int val)
{
	if (dbg_snapshot_get_enable()) {
		if (val)
			__raw_writel(DSS_SIGN_DEBUG_TEST, dbg_snapshot_get_base_vaddr() + DSS_OFFSET_DEBUG_TEST);
		else
			__raw_writel(DSS_SIGN_RESET, dbg_snapshot_get_base_vaddr() + DSS_OFFSET_DEBUG_TEST);
	}
}

bool dbg_snapshot_debug_test_enabled(void)
{
	if (dbg_snapshot_get_enable())
		return __raw_readl(dbg_snapshot_get_base_vaddr() +
			DSS_OFFSET_DEBUG_TEST) == DSS_SIGN_DEBUG_TEST;
	else
		return false;
}

void dbg_snapshot_set_debug_test_case(unsigned int val)
{
	if (dbg_snapshot_get_enable())
		__raw_writel(val, dbg_snapshot_get_base_vaddr() + DSS_OFFSET_DEBUG_TEST_CASE);
}

unsigned int dbg_snapshot_get_debug_test_case(void)
{
	unsigned int ret = 0xffffffff;

	if (dbg_snapshot_get_enable())
		ret = __raw_readl(dbg_snapshot_get_base_vaddr() + DSS_OFFSET_DEBUG_TEST_CASE);
	return ret;
}

void dbg_snapshot_set_debug_test_next(unsigned int val)
{
	if (dbg_snapshot_get_enable())
		__raw_writel(val, dbg_snapshot_get_base_vaddr() + DSS_OFFSET_DEBUG_TEST_NEXT);
}

unsigned int dbg_snapshot_get_debug_test_next(void)
{
	unsigned int ret = 0xffffffff;

	if (dbg_snapshot_get_enable())
		ret = __raw_readl(dbg_snapshot_get_base_vaddr() + DSS_OFFSET_DEBUG_TEST_NEXT);
	return ret;
}

void dbg_snapshot_set_debug_test_panic(unsigned int val)
{
	if (dbg_snapshot_get_enable())
		__raw_writel(val, dbg_snapshot_get_base_vaddr() + DSS_OFFSET_DEBUG_TEST_PANIC);

}

unsigned int dbg_snapshot_get_debug_test_panic(void)
{
	unsigned int ret = 0xffffffff;

	if (dbg_snapshot_get_enable())
		ret = __raw_readl(dbg_snapshot_get_base_vaddr() + DSS_OFFSET_DEBUG_TEST_PANIC);
	return ret;
}

void dbg_snapshot_set_debug_test_wdt(unsigned int val)
{
	if (dbg_snapshot_get_enable())
		__raw_writel(val, dbg_snapshot_get_base_vaddr() + DSS_OFFSET_DEBUG_TEST_WDT);

}

unsigned int dbg_snapshot_get_debug_test_wdt(void)
{
	unsigned int ret = 0xffffffff;

	if (dbg_snapshot_get_enable())
		ret = __raw_readl(dbg_snapshot_get_base_vaddr() + DSS_OFFSET_DEBUG_TEST_WDT);
	return ret;
}

void dbg_snapshot_set_debug_test_wtsr(unsigned int val)
{
	if (dbg_snapshot_get_enable())
		__raw_writel(val, dbg_snapshot_get_base_vaddr() + DSS_OFFSET_DEBUG_TEST_WTSR);
}

unsigned int dbg_snapshot_get_debug_test_wtsr(void)
{
	unsigned int ret = 0xffffffff;

	if (dbg_snapshot_get_enable())
		ret = __raw_readl(dbg_snapshot_get_base_vaddr() + DSS_OFFSET_DEBUG_TEST_WTSR);
	return ret;
}

void dbg_snapshot_set_debug_test_smpl(unsigned int val)
{
	if (dbg_snapshot_get_enable())
		__raw_writel(val, dbg_snapshot_get_base_vaddr() + DSS_OFFSET_DEBUG_TEST_SMPL);
}

unsigned int dbg_snapshot_get_debug_test_smpl(void)
{
	unsigned int ret = 0xffffffff;

	if (dbg_snapshot_get_enable())
		ret = __raw_readl(dbg_snapshot_get_base_vaddr() + DSS_OFFSET_DEBUG_TEST_SMPL);
	return ret;
}

void dbg_snapshot_set_debug_test_curr(unsigned int val)
{
	if (dbg_snapshot_get_enable())
		__raw_writel(val, dbg_snapshot_get_base_vaddr() + DSS_OFFSET_DEBUG_TEST_CURR);
}

unsigned int dbg_snapshot_get_debug_test_curr(void)
{
	unsigned int ret = 0xffffffff;

	if (dbg_snapshot_get_enable())
		ret = __raw_readl(dbg_snapshot_get_base_vaddr() + DSS_OFFSET_DEBUG_TEST_CURR);
	return ret;
}

void dbg_snapshot_set_debug_test_total(unsigned int val)
{
	if (dbg_snapshot_get_enable())
		__raw_writel(val, dbg_snapshot_get_base_vaddr() + DSS_OFFSET_DEBUG_TEST_TOTAL);
}

unsigned int dbg_snapshot_get_debug_test_total(void)
{
	unsigned int ret = 0xffffffff;

	if (dbg_snapshot_get_enable())
		ret = __raw_readl(dbg_snapshot_get_base_vaddr() + DSS_OFFSET_DEBUG_TEST_TOTAL);
	return ret;
}

void dbg_snapshot_set_debug_test_run(unsigned int test_id, unsigned int var)
{
	unsigned int ret;

	if (dbg_snapshot_get_enable()) {
		ret = __raw_readl(dbg_snapshot_get_base_vaddr() + DSS_OFFSET_DEBUG_TEST_RUN);
		if (!var)
			ret &= ~(1 << test_id);
		else
			ret |= (1 << test_id);
		__raw_writel(ret, dbg_snapshot_get_base_vaddr() + DSS_OFFSET_DEBUG_TEST_RUN);
	}
}

unsigned int dbg_snapshot_get_debug_test_run(unsigned int test_id)
{
	unsigned int ret;

	if (!dbg_snapshot_get_enable())
		return 0;

	ret = __raw_readl(dbg_snapshot_get_base_vaddr() + DSS_OFFSET_DEBUG_TEST_RUN);
	return ret & (1 << test_id);
}

void dbg_snapshot_clear_debug_test_runflag(void)
{
	if (!dbg_snapshot_get_enable())
		return;

	__raw_writel(0, dbg_snapshot_get_base_vaddr() + DSS_OFFSET_DEBUG_TEST_RUN);
}

unsigned int dbg_snapshot_get_debug_test_runflag(void)
{
	if (!dbg_snapshot_get_enable())
		return 0;

	return __raw_readl(dbg_snapshot_get_base_vaddr() + DSS_OFFSET_DEBUG_TEST_RUN);
}

void dbg_snapshot_set_debug_test_buffer_addr(u64 paddr, unsigned int cpu)
{
	if (!dbg_snapshot_get_enable())
		return;

	__raw_writeq(paddr, dbg_snapshot_get_base_vaddr() + DSS_OFFSET_DEBUG_TEST_BUFFER(cpu));
}

unsigned int dbg_snapshot_get_debug_test_buffer_addr(unsigned int cpu)
{
	if (!dbg_snapshot_get_enable())
		return 0;

	return __raw_readq(dbg_snapshot_get_base_vaddr() + DSS_OFFSET_DEBUG_TEST_BUFFER(cpu));
}

unsigned long dbg_snapshot_get_last_pc_paddr(void)
{
	/*
	 * Basically we want to save the pc value to non-cacheable region
	 * if ESS is enabled. But we should also consider cases that are not so.
	 */

	if (dbg_snapshot_get_enable())
		return ((unsigned long)dbg_snapshot_get_base_paddr() + DSS_OFFSET_CORE_LAST_PC);
	else
		return virt_to_phys((void *)dss_desc.hardlockup_core_pc);
}

unsigned long dbg_snapshot_get_last_pc(unsigned int cpu)
{
	if (dbg_snapshot_get_enable())
		return __raw_readq(dbg_snapshot_get_base_vaddr() +
				DSS_OFFSET_CORE_LAST_PC + cpu * 8);
	else
		return dss_desc.hardlockup_core_pc[cpu];
}

unsigned long dbg_snapshot_get_spare_vaddr(unsigned int offset)
{
	if (dbg_snapshot_get_enable())
		return (unsigned long)(dbg_snapshot_get_base_vaddr() +
				DSS_OFFSET_SPARE_BASE + offset);
	else
		return 0;
}

unsigned long dbg_snapshot_get_spare_paddr(unsigned int offset)
{
	unsigned long base_vaddr = 0;
	unsigned long base_paddr = (unsigned long)dbg_snapshot_get_base_paddr();

	if (dbg_snapshot_get_enable()) {
		if (base_paddr)
			base_vaddr = (unsigned long)(base_paddr +
				DSS_OFFSET_SPARE_BASE + offset);
	}

	return base_vaddr;
}

unsigned int dbg_snapshot_get_item_size(char* name)
{
	unsigned long i;

	if (dbg_snapshot_get_enable()) {
		for (i = 0; i < dss_desc.log_cnt; i++) {
			if (!strncmp(dss_items[i].name, name, strlen(name)))
				return dss_items[i].entry.size;
		}
	}
	return 0;
}
EXPORT_SYMBOL(dbg_snapshot_get_item_size);

unsigned long dbg_snapshot_get_item_vaddr(char *name)
{
	unsigned long i;

	if (dbg_snapshot_get_enable()) {
		for (i = 0; i < dss_desc.log_cnt; i++) {
			if (!strncmp(dss_items[i].name, name, strlen(name)))
				return dss_items[i].entry.vaddr;
		}
	}
	return 0;
}
EXPORT_SYMBOL(dbg_snapshot_get_item_vaddr);

unsigned int dbg_snapshot_get_item_paddr(char* name)
{
	unsigned long i;

	if (dbg_snapshot_get_enable()) {
		for (i = 0; i < dss_desc.log_cnt; i++) {
			if (!strncmp(dss_items[i].name, name, strlen(name)))
				return dss_items[i].entry.paddr;
		}
	}
	return 0;
}
EXPORT_SYMBOL(dbg_snapshot_get_item_paddr);

unsigned long dbg_snapshot_get_item_curr_ptr(char *name)
{
	unsigned long i;

	if (dbg_snapshot_get_enable()) {
		for (i = 0; i < dss_desc.log_cnt; i++) {
			if (!strncmp(dss_items[i].name, name, strlen(name))) {
				if (dss_items[i].entry.enabled)
					return (unsigned long)dss_items[i].curr_ptr;
				break;
			}
		}
	}
	return 0;
}
EXPORT_SYMBOL(dbg_snapshot_get_item_curr_ptr);

int dbg_snapshot_get_hardlockup(void)
{
	if (dbg_snapshot_get_enable()) {
		return dss_desc.hardlockup_detected;
	} else
		return 0;
}
EXPORT_SYMBOL(dbg_snapshot_get_hardlockup);

int dbg_snapshot_set_hardlockup(int val)
{
	unsigned long flags;

	if (!dbg_snapshot_get_enable())
		return 0;

	raw_spin_lock_irqsave(&dss_desc.ctrl_lock, flags);
	dss_desc.hardlockup_detected = val;
	raw_spin_unlock_irqrestore(&dss_desc.ctrl_lock, flags);
	return 0;
}
EXPORT_SYMBOL(dbg_snapshot_set_hardlockup);

int dbg_snapshot_is_hardlockup(void)
{
	return !!dss_desc.hardlockup_core_mask;
}
EXPORT_SYMBOL(dbg_snapshot_is_hardlockup);

int dbg_snapshot_early_panic(void)
{
	if (dbg_snapshot_get_enable())
		dss_soc_ops->soc_early_panic(NULL);

	return 0;
}

int dbg_snapshot_prepare_panic(void)
{
	unsigned long cpu;

	if (!dbg_snapshot_get_enable())
		return 0;
	/*
	 * kick watchdog to prevent unexpected reset during panic sequence
	 * and it prevents the hang during panic sequence by watchedog
	 */
	dss_soc_ops->soc_start_watchdog(NULL);

	dss_soc_ops->soc_prepare_panic_entry(NULL);

	/* Again disable log_kevents */
	dbg_snapshot_set_enable_item("log_kevents", false);

	for_each_possible_cpu(cpu) {
		if (dss_soc_ops->soc_is_power_cpu(cpu))
			dbg_snapshot_set_core_power_stat(DSS_SIGN_ALIVE, cpu);
		else
			dbg_snapshot_set_core_power_stat(DSS_SIGN_DEAD, cpu);
	}
	dss_soc_ops->soc_prepare_panic_exit(NULL);

	return 0;
}
EXPORT_SYMBOL(dbg_snapshot_prepare_panic);

int dbg_snapshot_post_panic(void)
{
	if (!dbg_snapshot_get_enable())
		return 0;

	dbg_snapshot_recall_hardlockup_core();

	dbg_snapshot_dump_sfr();

	dbg_snapshot_save_context(NULL);

	dbg_snapshot_print_panic_report();

	dss_soc_ops->soc_post_panic_entry(NULL);

	if (!dss_desc.no_wdt_dev) {
		if (dss_desc.hardlockup_detected || num_online_cpus() > 1) {
			/* for stall cpu */
			dbg_snapshot_spin_func();
		}
	}
#if defined(CONFIG_SEC_DEBUG)
	secdbg_base_post_panic_handler();
#endif

	dss_soc_ops->soc_post_panic_exit(NULL);

	/* for stall cpu when not enabling panic reboot */
	dbg_snapshot_spin_func();

	/* Never run this function */
	dev_emerg(dss_desc.dev, "debug-snapshot: %s DO NOT RUN this function (CPU:%d)\n",
					__func__, raw_smp_processor_id());
	return 0;
}
EXPORT_SYMBOL(dbg_snapshot_post_panic);

int dbg_snapshot_dump_panic(char *str, size_t len)
{
	if (!dbg_snapshot_get_enable())
		return 0;

	/*  This function is only one which runs in panic funcion */
	if (str && len && len < SZ_1K)
		memcpy(dbg_snapshot_get_base_vaddr() + DSS_OFFSET_PANIC_STRING, str, len);

	return 0;
}
EXPORT_SYMBOL(dbg_snapshot_dump_panic);

int dbg_snapshot_post_reboot(char *cmd)
{
	int cpu;

	if (!dbg_snapshot_get_enable())
		return 0;

	dss_soc_ops->soc_post_reboot_entry(NULL);

	dbg_snapshot_report_reason(DSS_SIGN_NORMAL_REBOOT);

	if (!cmd)
		dbg_snapshot_scratch_clear();
	else if (strcmp((char *)cmd, "bootloader") && strcmp((char *)cmd, "ramdump"))
		dbg_snapshot_scratch_clear();

	dev_emerg(dss_desc.dev, "debug-snapshot: normal reboot done\n");

	dbg_snapshot_save_context(NULL);

	/* clear DSS_SIGN_PANIC when normal reboot */
	for_each_possible_cpu(cpu) {
		dbg_snapshot_set_core_panic_stat(DSS_SIGN_RESET, cpu);
	}

	dss_soc_ops->soc_post_reboot_exit(NULL);

	return 0;
}
EXPORT_SYMBOL(dbg_snapshot_post_reboot);

static int dbg_snapshot_reboot_handler(struct notifier_block *nb,
				    unsigned long l, void *p)
{
	if (!dbg_snapshot_get_enable())
		return 0;

	dev_emerg(dss_desc.dev, "debug-snapshot: normal reboot starting\n");

	return 0;
}

static int dbg_snapshot_panic_handler(struct notifier_block *nb,
				   unsigned long l, void *buf)
{
	if (!dbg_snapshot_get_enable())
		return 0;

	dbg_snapshot_report_reason(DSS_SIGN_PANIC);

	local_irq_disable();
	dev_emerg(dss_desc.dev, "debug-snapshot: panic - reboot[%s]\n", __func__);
	dbg_snapshot_dump_task_info();
	dev_emerg(dss_desc.dev, "linux_banner: %s\n", linux_banner);

#if defined(CONFIG_SEC_DEBUG)
	secdbg_base_panic_handler(buf, true);
#endif

	return 0;
}

static struct notifier_block nb_reboot_block = {
	.notifier_call = dbg_snapshot_reboot_handler
};

static struct notifier_block nb_panic_block = {
	.notifier_call = dbg_snapshot_panic_handler,
};

void dbg_snapshot_soc_do_dpm_policy(int policy)
{
	dss_soc_ops->soc_do_dpm_policy(&policy);
}

void dbg_snapshot_panic_handler_safe(void)
{
	char *cpu_num[SZ_16] = {"0", "1", "2", "3", "4", "5", "6", "7", "8", "9"};
	char text[SZ_32] = "safe panic handler at cpu ";
	int cpu = raw_smp_processor_id();
	size_t len;

	if (!dbg_snapshot_get_enable())
		return;

	strncat(text, cpu_num[cpu], 1);
	len = strnlen(text, SZ_32);

	dbg_snapshot_report_reason(DSS_SIGN_SAFE_FAULT);
	dbg_snapshot_dump_panic(text, len);
#ifdef CONFIG_SEC_DEBUG_EMERG_WDT_CALLER
	dss_soc_ops->soc_expire_watchdog((void *)_RET_IP_);
#else
	dss_soc_ops->soc_expire_watchdog((void *)NULL);
#endif
}

void dbg_snapshot_register_soc_ops(struct dbg_snapshot_helper_ops *ops)
{
	if (ops)
		dss_soc_ops = ops;
}

void __init dbg_snapshot_init_helper(void)
{
	register_reboot_notifier(&nb_reboot_block);
	atomic_notifier_chain_register(&panic_notifier_list, &nb_panic_block);
	dss_soc_ops = &dss_soc_dummy_ops;

	/* hardlockup_detector function should be called before secondary booting */
	dbg_snapshot_soc_helper_init();
}
