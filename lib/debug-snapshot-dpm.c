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
#include <linux/version.h>
#include <linux/io.h>
#include <linux/device.h>
#include <linux/bootmem.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/module.h>
#include <linux/memblock.h>
#include <linux/of_address.h>
#include <linux/of_reserved_mem.h>
#include <linux/sched/clock.h>
#include <linux/of_fdt.h>
#include <linux/libfdt.h>

#include "debug-snapshot-local.h"

/* To Support Samsung SoC */
#include <soc/samsung/cal-if.h>
#include <asm/stacktrace.h>

const char *dpm_policy[] = {
	GO_DEFAULT,
	GO_PANIC,
	GO_WATCHDOG,
	GO_S2D,
	GO_ARRAYDUMP,
	GO_SCANDUMP,
};

struct dbg_snapshot_dpm_item dpm_items[] = {
	{DPM_P, DPM_P_ITMON, DPM_P_ITMON_ERR_DREX_TMOUT,	GO_DEFAULT_ID,	0},
	{DPM_P, DPM_P_ITMON, DPM_P_ITMON_ERR_IP,		GO_DEFAULT_ID,	0},
	{DPM_P, DPM_P_ITMON, DPM_P_ITMON_ERR_FATAL,		GO_DEFAULT_ID,	0},
	{DPM_P, DPM_P_ITMON, DPM_P_ITMON_ERR_CPU,		GO_DEFAULT_ID,	0},
	{DPM_P, DPM_P_ITMON, DPM_P_ITMON_ERR_CP,		GO_DEFAULT_ID,	0},
	{DPM_P, DPM_P_ITMON, DPM_P_ITMON_ERR_CHUB,		GO_DEFAULT_ID,	0},
	{DPM_C, DPM_C_ITMON, DPM_C_ITMON_PANIC_COUNT,		GO_DEFAULT_ID,	0},
	{DPM_C, DPM_C_ITMON, DPM_C_ITMON_PANIC_CPU_COUNT,	GO_DEFAULT_ID,	0},
};

struct dbg_snapshot_dpm dss_dpm;

int dbg_snapshot_set_dpm_item(char *first, char *second, char *node, int policy, int value)
{
	struct dbg_snapshot_dpm_item *item = NULL;
	unsigned long i;

	if (!first || !second || !node)
		return -ENODEV;

	for (i = 0; i < ARRAY_SIZE(dpm_items); i++) {
		if ((!strncmp(dpm_items[i].first_node, first, strlen(first))) &&
		   (!strncmp(dpm_items[i].second_node, second, strlen(second))) &&
		   (!strncmp(dpm_items[i].node, node, strlen(node)))) {
			item = &dpm_items[i];
			item->policy = policy;
			item->value = value;
			pr_info("debug-snapshot: item - [%s:%s:%s/policy:%d/value:%d]\n",
				item->first_node, item->second_node, item->node,
				item->policy, item->value);
			break;
		}
	}

	return 0;
}

int dbg_snapshot_get_dpm_item_policy(char *first, char *second, char *node)
{
	struct dbg_snapshot_dpm_item *item = NULL;
	unsigned long i;

	if (!first || !second || !node)
		return -ENODEV;

	for (i = 0; i < ARRAY_SIZE(dpm_items); i++) {
		if ((!strncmp(dpm_items[i].first_node, first, strlen(first))) &&
		   (!strncmp(dpm_items[i].second_node, second, strlen(second))) &&
		   (!strncmp(dpm_items[i].node, node, strlen(node)))) {
			item = &dpm_items[i];
			pr_info("dpm: item - %s:%s:%s - policy:%d, value:%d\n",
				item->first_node, item->second_node, item->node,
				item->policy, item->value);

			return item->policy;
		}
	}

	return -ENODEV;
}

int dbg_snapshot_get_dpm_item_value(char *first, char *second, char *node)
{
	struct dbg_snapshot_dpm_item *item = NULL;
	unsigned long i;

	if (!first || !second || !node)
		return -ENODEV;

	for (i = 0; i < ARRAY_SIZE(dpm_items); i++) {
		if ((!strncmp(dpm_items[i].first_node, first, strlen(first))) &&
		   (!strncmp(dpm_items[i].second_node, second, strlen(second))) &&
		   (!strncmp(dpm_items[i].node, node, strlen(node)))) {
			item = &dpm_items[i];
			pr_info("dpm: item - %s:%s:%s - policy:%d, value:%d\n",
				item->first_node, item->second_node, item->node,
				item->policy, item->value);

			return item->value;
		}
	}

	return -ENODEV;
}

void dbg_snapshot_do_dpm_policy(unsigned int policy)
{
	pr_emerg("%s: %s\n", __func__, dpm_policy[policy]);
	dss_soc_ops->soc_do_dpm_policy(&policy);
}

asmlinkage void dbg_snapshot_do_dpm(struct pt_regs *regs)
{
	unsigned long tsk_stk = (unsigned long)current->stack;
	unsigned long irq_stk = (unsigned long)this_cpu_read(irq_stack_ptr);
#ifdef CONFIG_VMAP_STACK
	unsigned long ovf_stk = (unsigned long)this_cpu_ptr(overflow_stack);
#endif
	unsigned int esr = read_sysreg(esr_el1);
	unsigned long far = read_sysreg(far_el1);
	unsigned int val = 0;
	unsigned int policy = 0;

	if (!dss_dpm.enabled || !dss_dpm.enabled_debug)
		return;

	if (user_mode(regs))
		return;

	switch (ESR_ELx_EC(esr)) {
	case ESR_ELx_EC_DABT_CUR:
		val = esr & 63;
		if ((val >= 4 && val <= 7) ||	/* translation fault */
		     (val >= 9 && val <= 11) || /* page fault */
		     (val >= 12 && val <= 15))	/* page fault */
			policy = GO_DEFAULT_ID;
		else
			policy = dss_dpm.p_el1_da;
		break;
	case ESR_ELx_EC_IABT_CUR:
		policy = dss_dpm.p_el1_ia;
		break;
	case ESR_ELx_EC_SYS64:
		policy = dss_dpm.p_el1_undef;
		break;
	case ESR_ELx_EC_SP_ALIGN:
		policy = dss_dpm.p_el1_sp_pc;
		break;
	case ESR_ELx_EC_PC_ALIGN:
		policy = dss_dpm.p_el1_sp_pc;
		break;
	case ESR_ELx_EC_UNKNOWN:
		policy = dss_dpm.p_el1_undef;
		break;
	case ESR_ELx_EC_SOFTSTP_LOW:
	case ESR_ELx_EC_SOFTSTP_CUR:
	case ESR_ELx_EC_BREAKPT_LOW:
	case ESR_ELx_EC_BREAKPT_CUR:
	case ESR_ELx_EC_WATCHPT_LOW:
	case ESR_ELx_EC_WATCHPT_CUR:
	case ESR_ELx_EC_BRK64:
		policy = GO_DEFAULT_ID;
		break;
	default:
		policy = dss_dpm.p_el1_serror;
		break;
	}

	if (policy && policy != GO_DEFAULT_ID) {
		if (dss_dpm.pre_log) {
			pr_emerg("ESR: 0x%08x -- %s\n", esr, esr_get_class_string(esr));
			pr_emerg("FAR: 0x%016lx\n", far);
			pr_emerg("Task stack:     [0x%016lx..0x%016lx]\n",
				tsk_stk, tsk_stk + THREAD_SIZE);
			pr_emerg("IRQ stack:      [0x%016lx..0x%016lx]\n",
				irq_stk, irq_stk + THREAD_SIZE);
#ifdef CONFIG_VMAP_STACK
			pr_emerg("Overflow stack: [0x%016lx..0x%016lx]\n",
				ovf_stk, ovf_stk + OVERFLOW_STACK_SIZE);
#endif
		}
		dbg_snapshot_do_dpm_policy(policy);
	}
}

static const char *enabled = "enabled";
static const char *disabled = "disabled";

int __init dbg_snapshot_early_init_dt_scan_dpm_feature(unsigned long node, const char *uname, void *data)
{
	unsigned long item1, item2;
	unsigned int val;
	const __be32 *prop;
	const char *method;
	char *move;
	int len, len2;

	item1 = of_get_flat_dt_subnode_by_name(node, "debug");
	if (item1 == -FDT_ERR_NOTFOUND) {
		dss_dpm.enabled_debug = false;
		pr_info("dpm: No such ramdump node, [debug feature] %s\n", disabled);
		goto exit_dss;
	}

	prop = of_get_flat_dt_prop(item1, "enabled", &len);
	if (!prop) {
		pr_info("dpm: No such enabled of ramdump node, [debug feature] %s\n", disabled);
	} else {
		val = be32_to_cpup(prop);
		dss_dpm.enabled_debug = val;
		pr_info("dpm: debug feature is %s\n", val ? enabled : disabled);
		if (!val)
			goto exit_dss;
	}

	item1 = of_get_flat_dt_subnode_by_name(node, "dump-mode");
	if (item1 == -FDT_ERR_NOTFOUND) {
		pr_info("dpm: No such ramdump node, [dump-mode] %s\n", disabled);
		goto exit_dss;
	}

	prop = of_get_flat_dt_prop(item1, "enabled", &len);
	if (!prop) {
		pr_info("dpm: No such enabled of dump-mode, [dump-mode] %s\n", disabled);
	} else {
		val = be32_to_cpup(prop);
		dss_dpm.enabled_dump_mode = val;
		pr_info("dpm: dump-mode is %s\n", val ? enabled : disabled);
	}

	prop = of_get_flat_dt_prop(item1, "file-support", &len);
	if (!prop) {
		pr_info("dpm: No such file-support of dump-mode node, [file-support] %s\n", disabled);
	} else {
		val = be32_to_cpup(prop);
		dss_dpm.enabled_dump_mode_file = val;
		pr_info("dpm: file-support of dump-mode is %s\n", val ? enabled : disabled);
	}

	item1 = of_get_flat_dt_subnode_by_name(node, "dss");
	if (item1 == -FDT_ERR_NOTFOUND) {
		pr_info("dpm: No such enabled of dss, [dss] %s\n", disabled);
		goto exit_dss;
	}

	method = of_get_flat_dt_prop(item1, "method", &len);
	if (!method) {
		pr_warn("dpm: No such methods of dss\n");
		goto exit_dss;
	}

	dbg_snapshot_set_enable_item(DSS_ITEM_HEADER, true);

	move = (char *)(method);
	while (len > 0) {
		dbg_snapshot_set_enable_item(move, true);
		len2 = strlen(move) + 1;
		move = (char *)(move + len2);
		len -= len2;
	}

	item2 = of_get_flat_dt_subnode_by_name(item1, "event");
	if (item2 == -FDT_ERR_NOTFOUND) {
		pr_warn("dpm: No such methods of kernel event\n");
		goto exit_dss;
	}

	method = of_get_flat_dt_prop(item2, "method", &len);
	if (!method) {
		pr_warn("dpm: No such methods of kevents\n");
		goto exit_dss;
	}

	move = (char *)(method);
	while (len > 0) {
		dbg_snapshot_early_init_log_enabled(move, true);
		len2 = strlen(move) + 1;
		move = (char *)(move + len2);
		len -= len2;
	}

exit_dss:
	return 1;
}

int __init dbg_snapshot_early_init_dt_scan_dpm_custom_policy(unsigned long node)
{
	unsigned long item;
	unsigned int val;
	const __be32 *policy;
	int len;

	item = of_get_flat_dt_subnode_by_name(node, DPM_P_ITMON);
	if (item == -FDT_ERR_NOTFOUND) {
		pr_info("dpm: No such itmon node, nothing to [policy] in [exception]");
		goto exit_custom_policy;
	}

	policy = of_get_flat_dt_prop(item, DPM_P_ITMON_ERR_FATAL, &len);
	if (!policy) {
		pr_info("dpm: No such [%s]\n", DPM_P_ITMON_ERR_FATAL);
	} else {
		val = be32_to_cpup(policy);
		dbg_snapshot_set_dpm_item(DPM_P, DPM_P_ITMON, DPM_P_ITMON_ERR_FATAL, val, -1);
		pr_info("dpm: run [%s] at [%s]\n", dpm_policy[val], DPM_P_ITMON_ERR_FATAL);
	}

	policy = of_get_flat_dt_prop(item, DPM_P_ITMON_ERR_DREX_TMOUT, &len);
	if (!policy) {
		pr_info("dpm: No such [%s]\n", DPM_P_ITMON_ERR_DREX_TMOUT);
	} else {
		val = be32_to_cpup(policy);
		dbg_snapshot_set_dpm_item(DPM_P, DPM_P_ITMON, DPM_P_ITMON_ERR_DREX_TMOUT, val, -1);
		pr_info("dpm: run [%s] at [%s]\n", dpm_policy[val], DPM_P_ITMON_ERR_DREX_TMOUT);
	}

	policy = of_get_flat_dt_prop(item, DPM_P_ITMON_ERR_IP, &len);
	if (!policy) {
		pr_info("dpm: No such [%s]\n", DPM_P_ITMON_ERR_IP);
	} else {
		val = be32_to_cpup(policy);
		dbg_snapshot_set_dpm_item(DPM_P, DPM_P_ITMON, DPM_P_ITMON_ERR_IP, val, -1);
		pr_info("dpm: run [%s] at [%s]\n", dpm_policy[val], DPM_P_ITMON_ERR_IP);
	}

	policy = of_get_flat_dt_prop(item, DPM_P_ITMON_ERR_CPU, &len);
	if (!policy) {
		pr_info("dpm: No such [%s]\n", DPM_P_ITMON_ERR_CPU);
	} else {
		val = be32_to_cpup(policy);
		dbg_snapshot_set_dpm_item(DPM_P, DPM_P_ITMON, DPM_P_ITMON_ERR_CPU, val, -1);
		pr_info("dpm: run [%s] at [%s]\n", dpm_policy[val], DPM_P_ITMON_ERR_CPU);
	}

	policy = of_get_flat_dt_prop(item, DPM_P_ITMON_ERR_CP, &len);
	if (!policy) {
		pr_info("dpm: No such [%s]\n", DPM_P_ITMON_ERR_CP);
	} else {
		val = be32_to_cpup(policy);
		dbg_snapshot_set_dpm_item(DPM_P, DPM_P_ITMON, DPM_P_ITMON_ERR_CP, val, -1);
		pr_info("dpm: run [%s] at [%s]\n", dpm_policy[val], DPM_P_ITMON_ERR_CP);
	}

	policy = of_get_flat_dt_prop(item, DPM_P_ITMON_ERR_CHUB, &len);
	if (!policy) {
		pr_info("dpm: No such [%s]\n", DPM_P_ITMON_ERR_CHUB);
	} else {
		val = be32_to_cpup(policy);
		dbg_snapshot_set_dpm_item(DPM_P, DPM_P_ITMON, DPM_P_ITMON_ERR_CHUB, val, -1);
		pr_info("dpm: run [%s] at [%s]\n", dpm_policy[val], DPM_P_ITMON_ERR_CHUB);
	}

exit_custom_policy:
	return 1;
}

int __init dbg_snapshot_early_init_dt_scan_dpm_policy(unsigned long node, const char *uname, void *data)
{
	unsigned long item;
	unsigned int val;
	const __be32 *policy;
	int len;

	item = of_get_flat_dt_subnode_by_name(node, "exception");
	if (item == -FDT_ERR_NOTFOUND) {
		pr_info("dpm: No such exception node, nothing to [policy] in [exception]");
		goto exit_policy;
	}

	policy = of_get_flat_dt_prop(item, "pre_log", &len);
	if (!policy) {
		pr_info("dpm: No such [%s]\n", DPM_P_EL1_DA);
	} else {
		val = be32_to_cpup(policy);
		dss_dpm.pre_log = val;
	}

	policy = of_get_flat_dt_prop(item, DPM_P_EL1_DA, &len);
	if (!policy) {
		pr_info("dpm: No such [%s]\n", DPM_P_EL1_DA);
	} else {
		val = be32_to_cpup(policy);
		dss_dpm.p_el1_da = val;
		pr_info("dpm: run [%s] at [%s]\n", dpm_policy[val], DPM_P_EL1_DA);
	}

	policy = of_get_flat_dt_prop(item, DPM_P_EL1_IA, &len);
	if (!policy) {
		pr_info("dpm: No such [%s]\n", DPM_P_EL1_IA);
	} else {
		val = be32_to_cpup(policy);
		dss_dpm.p_el1_ia = val;
		pr_info("dpm: run [%s] at [%s]\n", dpm_policy[val], DPM_P_EL1_IA);
	}

	policy = of_get_flat_dt_prop(item, DPM_P_EL1_UNDEF, &len);
	if (!policy) {
		pr_info("dpm: No such [%s]\n", DPM_P_EL1_UNDEF);
	} else {
		val = be32_to_cpup(policy);
		dss_dpm.p_el1_undef = val;
		pr_info("dpm: run [%s] at [%s]\n", dpm_policy[val], DPM_P_EL1_UNDEF);
	}

	policy = of_get_flat_dt_prop(item, DPM_P_EL1_SP_PC, &len);
	if (!policy) {
		pr_info("dpm: No such [%s]\n", DPM_P_EL1_SP_PC);
	} else {
		val = be32_to_cpup(policy);
		dss_dpm.p_el1_sp_pc = val;
		pr_info("dpm: run [%s] at [%s]\n", dpm_policy[val], DPM_P_EL1_SP_PC);
	}

	policy = of_get_flat_dt_prop(item, DPM_P_EL1_INV, &len);
	if (!policy) {
		pr_info("dpm: No such [%s]\n", DPM_P_EL1_INV);
	} else {
		val = be32_to_cpup(policy);
		dss_dpm.p_el1_inv = val;
		pr_info("dpm: run [%s] at [%s]\n", dpm_policy[val], DPM_P_EL1_INV);
	}

	policy = of_get_flat_dt_prop(item, DPM_P_EL1_SERROR, &len);
	if (!policy) {
		pr_info("dpm: No such [%s]\n", DPM_P_EL1_SERROR);
	} else {
		val = be32_to_cpup(policy);
		dss_dpm.p_el1_serror = val;
		pr_info("dpm: run [%s] at [%s]\n", dpm_policy[val], DPM_P_EL1_SERROR);
	}

	return dbg_snapshot_early_init_dt_scan_dpm_custom_policy(node);

exit_policy:
	return 1;
}

int __init dbg_snapshot_early_init_dt_scan_dpm_config(unsigned long node, const char *uname, void *data)
{
	unsigned long item;
	unsigned int val;
	const __be32 *config;
	int len;

	item = of_get_flat_dt_subnode_by_name(node, DPM_C_ITMON);
	if (item == -FDT_ERR_NOTFOUND) {
		pr_info("dpm: No such itmon node, nothing to itmon in config");
		goto exit_config;
	}

	config = of_get_flat_dt_prop(item, DPM_C_ITMON_PANIC_COUNT, &len);
	if (!config) {
		pr_info("dpm: No such %s\n", DPM_C_ITMON_PANIC_COUNT);
	} else {
		val = be32_to_cpup(config);
		dbg_snapshot_set_dpm_item(DPM_C, DPM_C_ITMON, DPM_C_ITMON_PANIC_COUNT, -1, val);
		pr_info("dpm: set to %s - 0x%x\n", DPM_C_ITMON_PANIC_COUNT, val);
	}

	config = of_get_flat_dt_prop(item, DPM_C_ITMON_PANIC_CPU_COUNT, &len);
	if (!config) {
		pr_info("dpm: No such %s\n", DPM_C_ITMON_PANIC_CPU_COUNT);
	} else {
		val = be32_to_cpup(config);
		dbg_snapshot_set_dpm_item(DPM_C, DPM_C_ITMON, DPM_C_ITMON_PANIC_CPU_COUNT, -1, val);
		pr_info("dpm: set to %s - 0x%x\n", DPM_C_ITMON_PANIC_CPU_COUNT, val);
	}

exit_config:
	return 1;
}

int __init dbg_snapshot_early_init_dt_scan_dpm(unsigned long node, const char *uname,
		int depth, void *data)
{
	unsigned long next, root = node;
	unsigned int val;
	const __be32 *prop;
	int len;

	if (depth != 1 || (strcmp(uname, "dpm") != 0))
		return 0;

	/* version */
	prop = of_get_flat_dt_prop(root, "version", &len);
	if (prop) {
		val = be32_to_cpup(prop);
		dss_dpm.version = val;
		dss_dpm.enabled = true;
		pr_info("dpm: v%01d.%02d\n", val / 100, val % 100);
	} else {
		pr_info("dpm: version is not found\n");
	}

	/* feature setting */
	next = of_get_flat_dt_subnode_by_name(root, DPM_F);
	if (next == -FDT_ERR_NOTFOUND) {
		pr_warn("dpm: No such features of exynos debug policy\n");
	} else {
		pr_warn("dpm: found features of exynos debug policy\n");
		dbg_snapshot_early_init_dt_scan_dpm_feature(next, uname, NULL);
	}

	/* policy setting */
	next = of_get_flat_dt_subnode_by_name(root, DPM_P);
	if (next == -FDT_ERR_NOTFOUND) {
		pr_warn("dpm: No such policy of exynos debug policy\n");
	} else {
		pr_warn("dpm: found policy of exynos debug policy\n");
		dbg_snapshot_early_init_dt_scan_dpm_policy(next, uname, NULL);
	}

	/* config setting */
	next = of_get_flat_dt_subnode_by_name(root, DPM_C);
	if (next == -FDT_ERR_NOTFOUND) {
		pr_warn("dpm: No such config of exynos debug policy\n");
	} else {
		pr_warn("dpm: found config of exynos debug policy\n");
		dbg_snapshot_early_init_dt_scan_dpm_config(next, uname, NULL);
	}

	return 1;
}
