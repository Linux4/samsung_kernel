/*
 * Copyright (C) 2014 Samsung Electronics Co.Ltd
 * http://www.samsung.com
 *
 * CP PMU driver
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
*/
#ifndef PMU_CP_H
#define PMU_CP_H

int exynos_pmu_get_cp_on_status(void);
int exynos_pmu_cp_on(void);
int exynos_pmu_cp_clear_active(void);
int exynos_pmu_cp_clear_reset(void);
int exynos_pmu_cp_reset(void);
int exynos_pmu_cp_release(void);

#define PR_REG(name, reg) pr_info("%s: 0x%08x\n", name, __raw_readl(reg));
void pmu_cp_reg_dump(void);
#endif
