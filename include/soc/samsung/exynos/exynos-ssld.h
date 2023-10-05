/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 Samsung Electronics Co., Ltd.
 */

#ifndef EXYNOS_SSLD_H
#define EXYNOS_SSLD_H

#include <linux/sched.h>
#include <linux/device.h>
#include <linux/notifier.h>

struct s2r_trace_info {
	u64 time;
	struct device *dev;
	union {
		const char *action;
		const char *pm_ops;
	};
	union {
		int val;
		int event;
		int error;
	};
	bool start;
};

struct ssld_notifier_data {
	struct task_struct *s2r_leading_tsk;
	struct s2r_trace_info *last_info;
	u64 pm_prepare_jiffies;
};

struct ssld_reboot_notifier_data {
	struct task_struct *reboot_leading_tsk;
	unsigned long reboot_stage;
	u64 reboot_jiffies;
	u64 reboot_start_time_nsec;
};

#if IS_ENABLED(CONFIG_EXYNOS_SSLD)
void ssld_notifier_chain_register(struct notifier_block *nb);
void ssld_reboot_notifier_chain_register(struct notifier_block *nb);
#else
#define ssld_notifier_chain_register(a)	do { } while (0)
#define ssld_reboot_notifier_chain_register(a)	do { } while (0)
#endif
#endif
