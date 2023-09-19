/* SPDX-License-Identifier: GPL-2.0-only
 *
 * Copyright (c) 2021 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Headef file for DPU PROFILER.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __EXYNOS_DRM_PROFILER_H__
#define __EXYNOS_DRM_PROFILER_H__

#include <linux/ktime.h>
#include <linux/types.h>

struct exynos_drm_crtc;

struct exynos_profiler {
	u64 frame_vsync_cnt;
	ktime_t frame_vsync_cnt_time;
	u64 ems_frame_cnt;
	ktime_t ems_frame_cnt_time;
	u64 ems_fence_cnt;
	ktime_t ems_fence_cnt_time;
};

#if IS_ENABLED(CONFIG_EXYNOS_GPU_PROFILER)
void exynos_profiler_update_vsync_cnt(struct exynos_profiler *profiler);
void exynos_profiler_update_ems_frame_cnt(struct exynos_profiler *profiler);
void exynos_profiler_update_ems_fence_cnt(struct exynos_profiler *profiler);
struct exynos_profiler *exynos_profiler_register(struct exynos_drm_crtc *exynos_crtc);
#else
static inline void exynos_profiler_update_vsync_cnt(struct exynos_profiler *profiler) {}
static inline void exynos_profiler_update_ems_frame_cnt(struct exynos_profiler *profiler) {}
static inline void exynos_profiler_update_ems_fence_cnt(struct exynos_profiler *profiler) {}
static inline struct exynos_profiler *exynos_profiler_register(struct exynos_drm_crtc *exynos_crtc) 
{ return NULL; }
#endif

#endif /* __EXYNOS_DRM_PROFILER_H__ */
