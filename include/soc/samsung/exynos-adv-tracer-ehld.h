/*
 * Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Exynos Early Hardlockup Detector for Samsung EXYNOS SoC
 * By Hosung Kim (hosung0.kim@samsung.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef __EXYNOS_ADV_TRACER_EHLD_H__
#define __EXYNOS_ADV_TRACER_EHLD_H__

#ifdef CONFIG_EXYNOS_ADV_TRACER_EHLD
extern int adv_tracer_ehld_set_enable(int en);
extern int adv_tracer_ehld_set_interval(u32 interval);
extern int adv_tracer_ehld_set_warn_count(u32 count);
extern int adv_tracer_ehld_set_lockup_count(u32 count);
extern u32 adv_tracer_ehld_get_interval(void);
extern int adv_tracer_ehld_get_enable(void);
#else
inline int adv_tracer_ehld_set_enable(int en)
{
	return -1;
}
inline int adv_tracer_ehld_set_interval(u32 interval)
{
	return -1;
}
inline int adv_tracer_ehld_set_warn_count(u32 count)
{
	return -1;
}
inline int adv_tracer_ehld_set_lockup_count(u32 count)
{
	return -1;
}
inline u32 adv_tracer_ehld_get_interval(void)
{
	return -1;
}
inline int adv_tracer_ehld_get_enable(void)
{
	return -1;
}
#endif

#endif

