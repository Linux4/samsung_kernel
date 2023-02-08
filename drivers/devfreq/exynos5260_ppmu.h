/*
 * Copyright (C) 2013 Samsung Electronics
 *               http://www.samsung.com/
 *               Sangkyu Kim <skwith.kim@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __DEVFREQ_EXYNOS5260_PPMU_H
#define __DEVFREQ_EXYNOS5260_PPMU_H __FILE__

#include <linux/notifier.h>

#include "exynos_ppmu2.h"

enum DEVFREQ_TYPE {
	MIF,
	INT,
	DEVFREQ_TYPE_COUNT,
};

enum DEVFREQ_PPMU {
	PPMU_DREX0_S0,
	PPMU_DREX0_S1,
	PPMU_DREX1_S0,
	PPMU_DREX1_S1,
	PPMU_COUNT,
};

enum DEVFREQ_PPMU_ADDR {
	PPMU_DREX0_S0_ADDR = 0x10C60000,
	PPMU_DREX0_S1_ADDR = 0x10C70000,
	PPMU_DREX1_S0_ADDR = 0x10C80000,
	PPMU_DREX1_S1_ADDR = 0x10C90000,
	PPMU_FIMD0X_ADDR = 0x145B0000,
	PPMU_FIMD1X_ADDR = 0x145C0000,
};

struct devfreq_exynos {
	struct list_head node;
	struct ppmu_info *ppmu_list;
	unsigned int ppmu_count;
	unsigned long long val_ccnt;
	unsigned long long val_pmcnt;
};

int exynos5260_devfreq_init(struct devfreq_exynos *de);
int exynos5260_devfreq_register(struct devfreq_exynos *de);
int exynos5260_ppmu_register_notifier(enum DEVFREQ_TYPE type, struct notifier_block *nb);
int exynos5260_ppmu_unregister_notifier(enum DEVFREQ_TYPE type, struct notifier_block *nb);

#endif /* __DEVFREQ_EXYNOS5260_PPMU_H */
