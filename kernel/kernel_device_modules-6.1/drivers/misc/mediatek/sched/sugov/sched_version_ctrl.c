// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2022 MediaTek Inc.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <asm/io.h>
#include <linux/cpufreq.h>
#include <linux/kthread.h>
#include <linux/irq_work.h>
#include "sugov/cpufreq.h"
#include "dsu_interface.h"
/* You can define module value in sched_version_ctrl.h */
#include "sched_version_ctrl.h"

bool vip_enable;
bool gear_hints_enable;
bool updown_migration_enable;
bool skip_hiIRQ_enable;
bool rt_aggre_preempt_enable;
bool post_init_util_ctl;

int init_sched_ctrl(void)
{
	struct device_node *eas_node;
	int sched_ctrl = 0;
	int ret = 0;

	eas_node = of_find_node_by_name(NULL, "eas-info");
	if (eas_node == NULL) {
		pr_info("failed to find node @ %s\n", __func__);
	} else {
		ret = of_property_read_u32(eas_node, "version", &sched_ctrl);
		if (ret < 0)
			pr_info("no share_buck err_code=%d %s\n", ret,  __func__);
	}
	switch(sched_ctrl) {
	case EAS_5_5:
		am_support = 0;
		grp_dvfs_support_mode = 0;
		// TODO
		gear_hints_enable = false;
		updown_migration_enable = false;
		skip_hiIRQ_enable = false;
		rt_aggre_preempt_enable = false;
		vip_enable = false;
		post_init_util_ctl = false;
		break;
	case EAS_5_5_1:
		am_support = 0;
		grp_dvfs_support_mode = 0;
		// TODO
		gear_hints_enable = false;
		updown_migration_enable = true;
		skip_hiIRQ_enable = false;
		rt_aggre_preempt_enable = false;
		vip_enable = false;
		post_init_util_ctl = false;
		break;
	case EAS_6_1:
		am_support = 1;
		grp_dvfs_support_mode = 1;
		// TODO
		gear_hints_enable = true;
		updown_migration_enable = true;
		skip_hiIRQ_enable = true;
		rt_aggre_preempt_enable = false;
		vip_enable = true;
		post_init_util_ctl = true;
		break;
	default:
		am_support = 0;
		grp_dvfs_support_mode = 0;
		// TODO
		gear_hints_enable = false;
		updown_migration_enable = false;
		skip_hiIRQ_enable = false;
		rt_aggre_preempt_enable = false;
		vip_enable = false;
		post_init_util_ctl = false;
		break;
	}
	return 0;
}

bool sched_vip_enable_get(void)
{
	return vip_enable;
}
EXPORT_SYMBOL_GPL(sched_vip_enable_get);

bool sched_gear_hints_enable_get(void)
{
	return gear_hints_enable;
}
EXPORT_SYMBOL_GPL(sched_gear_hints_enable_get);

bool sched_updown_migration_enable_get(void)
{
	return updown_migration_enable;
}
EXPORT_SYMBOL_GPL(sched_updown_migration_enable_get);

bool sched_skip_hiIRQ_enable_get(void)
{
	return skip_hiIRQ_enable;
}
EXPORT_SYMBOL_GPL(sched_skip_hiIRQ_enable_get);

bool sched_rt_aggre_preempt_enable_get(void)
{
	return rt_aggre_preempt_enable;
}
EXPORT_SYMBOL_GPL(sched_rt_aggre_preempt_enable_get);

bool sched_post_init_util_enable_get(void)
{
	return post_init_util_ctl;
}
EXPORT_SYMBOL_GPL(sched_post_init_util_enable_get);

void sched_post_init_util_set(bool enable)
{
	post_init_util_ctl = enable;
}
EXPORT_SYMBOL_GPL(sched_post_init_util_set);
