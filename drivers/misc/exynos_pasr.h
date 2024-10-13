/*
 * Defines for the exynos PASR driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#ifndef __EXYNOS_PASR_H
#define __EXYNOS_PASR_H

#include <linux/kthread.h>

#define MODULE_NAME		"exynos-pasr"

#define REG_SMC_MODE_REG_ADDR	0x0
#define REG_SET_RANK_MR17(x)	((((x) == 1 ? 0x1 : 0x3) << 28) | (0x11 << 20))
#define REG_SMC_MPR_MR_CTRL	0x4
#define REG_SMC_SEND_MRWR	(1 << 4)
#define REG_SMC_MODE_REG_WRITE	0x8

struct pasr_segment {
	int index;
	unsigned long addr;
};

struct pasr_thread {
	struct task_struct *thread;
	struct kthread_worker worker;
	struct kthread_work work;
	struct completion completion;
};

struct pasr_dev {
	struct device *dev;

	struct pasr_segment *segment_info;
	struct pasr_thread on_thread;
	struct pasr_thread off_thread;

	unsigned long offline_flag;
	unsigned int pasr_mask;
	int num_segment;
};
#endif /* __EXYNOS_PASR_H */
