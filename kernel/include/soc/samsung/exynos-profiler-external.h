/* soc/samsung/exynos-profiler-external.h
 *
 * Copyright (C) 2023 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * EXYNOS - Header file for Exynos Profiler support
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __EXYNOS_PROFILER_EXTERNAL_H__
#define __EXYNOS_PROFILER_EXTERNAL_H__

/* Profiler */
#if IS_ENABLED(CONFIG_EXYNOS_MAIN_PROFILER)
#include "../../../drivers/soc/samsung/profiler/include/exynos-profiler-fn.h"

extern void exynos_profiler_register_frame_cnt(void (*fn)(u64 *cnt, ktime_t *time));
extern void exynos_profiler_register_fence_cnt(void (*fn)(u64 *cnt, ktime_t *time));
extern void exynos_profiler_register_vsync_cnt(void (*fn)(u64 *cnt, ktime_t *time));
extern void exynos_profiler_update_fps_change(u32 new_fps);
#endif /* CONFIG_EXYNOS_MAIN_PROFILER */
#endif /* __EXYNOS_PROFILER_EXTERNAL_H__ */