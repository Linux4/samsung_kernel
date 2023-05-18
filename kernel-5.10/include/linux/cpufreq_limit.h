/*
 * CPU limit driver for MTK
 *
 * Copyright (c) 2022 Samsung Electronics Co., Ltd.
 *
 * Author: Sangyoung Son <hello.son@samsung.com>
 *
 * Change limit driver code for MTK chipset by Jonghyeon Cho
 * Author: Jonghyeon Cho <jongjaaa.cho@samsung.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __LINUX_CPUFREQ_LIMIT_H__
#define __LINUX_CPUFREQ_LIMIT_H__

struct cpufreq_limit_parameter {
	unsigned int unified_table[50];
	unsigned int num_cpu;
	unsigned int freq_count;
	unsigned int ltl_cpu_start;
	unsigned int big_cpu_start;
	unsigned int ltl_max_freq;
	unsigned int ltl_min_lock_freq;
	unsigned int big_max_lock_freq;
	unsigned int ltl_divider;
	unsigned int over_limit;
};

enum {
	DVFS_USER_ID = 0,
	DVFS_TOUCH_ID,
	DVFS_FINGER_ID,
	DVFS_OVERLIMIT_ID,
	DVFS_MAX_ID
};

extern int set_freq_limit(unsigned int id, unsigned int freq);

#endif /* __LINUX_CPUFREQ_LIMIT_H__ */
