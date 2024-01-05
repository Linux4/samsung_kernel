/*
 * Copyright (c) 2012 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef __EXYNOS_BTS_H_
#define __EXYNOS_BTS_H_

#if defined(CONFIG_EXYNOS5410_BTS) || defined(CONFIG_EXYNOS5420_BTS) \
	|| defined(CONFIG_EXYNOS5260_BTS) || defined(CONFIG_EXYNOS4_BTS) \
	|| defined(CONFIG_EXYNOS4415_BTS) || defined(CONFIG_EXYNOS3472_BTS)

void bts_initialize(const char *pd_name, bool on);
#else
#define bts_initialize(a, b) do {} while (0)
#endif

#if defined(CONFIG_EXYNOS4415_BTS)
void set_mfc_scen(bool enable);
#else
#define set_mfc_scen(a) do () while (0)
#endif

#ifdef CONFIG_EXYNOS5410_BTS
#include <mach/devfreq.h>

void bts_change_bustraffic(struct devfreq_info *info, unsigned long event);
void bts_set_bw(unsigned int bw);
#else
#define bts_change_bustraffic(a, b) do {} while (0)
#define bts_set_bw(a) do {} while (0)
#endif

#ifdef CONFIG_EXYNOS5420_BTS
void bts_change_g3d_state(unsigned int freq);
#else
#define bts_change_g3d_state(a) do {} while(0)
#endif

#endif
