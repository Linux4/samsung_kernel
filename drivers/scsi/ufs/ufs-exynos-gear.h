/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Gear scale with UFS
 *
 * Copyright (C) 2020 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 */
#ifndef _UFS_EXYNOS_GEAR_H_
#define _UFS_EXYNOS_GEAR_H_

#include <linux/workqueue.h>

enum ufs_perf_state {
	UFS_EXYNOS_BOOT,
	UFS_EXYNOS_PERF_OPERATIONAL,
	UFS_EXYNOS_PERF_DISABLED,
	UFS_EXYNOS_SHUTDOWN,
};

struct ufs_perf_stat_v2 {
	/* threshold for TP */
	u32 th_qd_max;
	s64 th_duration;
	u32 th_max_count;
	u32 th_min_count;

	/* stats */
	s64 start_count_time;
	u32 count;
	u32 req_size;
	u8 o_traffic;
	bool g_scale_en;
	u8 exynos_stat;

	/* notifier */
	struct notifier_block reboot_nb;

	struct workqueue_struct *scale_wq;
	struct work_struct gear_work;
	struct delayed_work devfreq_work;
};

#endif /* _UFS_EXYNOS_GEAR_H_ */
