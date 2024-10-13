/* SPDX-License-Identifier: GPL-2.0 */
/*
 * @file sgpu_profiler.h
 * @copyright 2022 Samsung Electronics
 */

#ifndef _SGPU_PROFILER_H_
#define _SGPU_PROFILER_H_
#if IS_ENABLED(CONFIG_EXYNOS_GPU_PROFILER)
#include <soc/samsung/exynos-profiler-external.h>

#if (PROFILER_VERSION == 0)
#include "sgpu_profiler_v0.h"
#else
#include "sgpu_profiler_v1.h"
#endif

#if (PROFILER_VERSION == 0)
inline void profiler_pb_set_cur_freq(int id, int idx)
{
	profiler_stc_set_cur_freqlv(id, idx);
}
#endif
#endif /* CONFIG_EXYNOS_GPU_PROFILER */
#endif /* _SGPU_PROFILER_H_ */
