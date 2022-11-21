/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef __GED_SKI_H__
#define __GED_SKI_H__

#include <linux/kobject.h>
#include <linux/sysfs.h>
#include <linux/cpufreq.h>

#include "ged_base.h"

ssize_t show_gpu_available_governor(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf);
ssize_t show_gpu_busy(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf);
ssize_t show_gpu_clock(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf);
ssize_t show_gpu_freq_table(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf);
ssize_t show_gpu_governor(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf);
ssize_t show_gpu_max_clock(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf);
ssize_t show_gpu_min_clock(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf);
ssize_t show_gpu_model(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf);
ssize_t show_gpu_tmu(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf);

GED_ERROR ged_ski_init(void);
void ged_ski_exit(void);

extern struct kobject *kernel_kobj;

#endif
