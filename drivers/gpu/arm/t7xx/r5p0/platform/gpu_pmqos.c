/* drivers/gpu/arm/.../platform/gpu_pmqos.c
 *
 * Copyright 2011 by S.LSI. Samsung Electronics Inc.
 * San#24, Nongseo-Dong, Giheung-Gu, Yongin, Korea
 *
 * Samsung SoC Mali-T Series DVFS driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software FoundatIon.
 */

/**
 * @file gpu_pmqos.c
 * DVFS
 */

#include <mali_kbase.h>

#include <linux/pm_qos.h>

#include "mali_kbase_platform.h"
#include "gpu_dvfs_handler.h"

struct pm_qos_request exynos5_g3d_mif_qos;
struct pm_qos_request exynos5_g3d_int_qos;
struct pm_qos_request exynos5_g3d_cpu_kfc_min_qos;
struct pm_qos_request exynos5_g3d_cpu_egl_max_qos;
struct pm_qos_request exynos5_g3d_cpu_egl_min_qos;

int gpu_pm_qos_command(struct exynos_context *platform, gpu_pmqos_state state)
{
	DVFS_ASSERT(platform);

	if (!platform->devfreq_status)
		return 0;

	switch (state) {
	case GPU_CONTROL_PM_QOS_INIT:
		pm_qos_add_request(&exynos5_g3d_mif_qos, PM_QOS_BUS_THROUGHPUT, 0);
		pm_qos_add_request(&exynos5_g3d_int_qos, PM_QOS_DEVICE_THROUGHPUT, 0);
		pm_qos_add_request(&exynos5_g3d_cpu_kfc_min_qos, PM_QOS_KFC_FREQ_MIN, 0);
		pm_qos_add_request(&exynos5_g3d_cpu_egl_max_qos, PM_QOS_CPU_FREQ_MAX, PM_QOS_CPU_FREQ_MAX_DEFAULT_VALUE);
		if (platform->boost_egl_min_lock)
			pm_qos_add_request(&exynos5_g3d_cpu_egl_min_qos, PM_QOS_CPU_FREQ_MIN, 0);
		break;
	case GPU_CONTROL_PM_QOS_DEINIT:
		pm_qos_remove_request(&exynos5_g3d_mif_qos);
		pm_qos_remove_request(&exynos5_g3d_int_qos);
		pm_qos_remove_request(&exynos5_g3d_cpu_kfc_min_qos);
		pm_qos_remove_request(&exynos5_g3d_cpu_egl_max_qos);
		if (platform->boost_egl_min_lock)
			pm_qos_remove_request(&exynos5_g3d_cpu_egl_min_qos);
		break;
	case GPU_CONTROL_PM_QOS_SET:
		KBASE_DEBUG_ASSERT(platform->step >= 0);
		pm_qos_update_request(&exynos5_g3d_mif_qos, platform->table[platform->step].mem_freq);
		pm_qos_update_request(&exynos5_g3d_int_qos, platform->table[platform->step].int_freq);
		pm_qos_update_request(&exynos5_g3d_cpu_kfc_min_qos, platform->table[platform->step].cpu_freq);
		if (!platform->boost_is_enabled)
			pm_qos_update_request(&exynos5_g3d_cpu_egl_max_qos, platform->table[platform->step].cpu_max_freq);
		break;
	case GPU_CONTROL_PM_QOS_RESET:
		pm_qos_update_request(&exynos5_g3d_mif_qos, 0);
		pm_qos_update_request(&exynos5_g3d_int_qos, 0);
		pm_qos_update_request(&exynos5_g3d_cpu_kfc_min_qos, 0);
		pm_qos_update_request(&exynos5_g3d_cpu_egl_max_qos, PM_QOS_CPU_FREQ_MAX_DEFAULT_VALUE);
		break;
	case GPU_CONTROL_PM_QOS_EGL_SET:
		pm_qos_update_request(&exynos5_g3d_cpu_egl_min_qos, platform->boost_egl_min_lock);
		break;
	case GPU_CONTROL_PM_QOS_EGL_RESET:
		pm_qos_update_request(&exynos5_g3d_cpu_egl_min_qos, 0);
		break;
	default:
		break;
	}

	return 0;
}
