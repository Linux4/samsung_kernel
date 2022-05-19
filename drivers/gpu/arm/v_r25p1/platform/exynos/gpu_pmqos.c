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

#if defined(PM_QOS_CLUSTER2_FREQ_MAX_DEFAULT_VALUE)
#define PM_QOS_CPU_CLUSTER_NUM 3
#else
#define PM_QOS_CPU_CLUSTER_NUM 2
#ifndef PM_QOS_CLUSTER1_FREQ_MAX_DEFAULT_VALUE
#define PM_QOS_CLUSTER1_FREQ_MAX_DEFAULT_VALUE INT_MAX
#endif
#endif

#if defined(CONFIG_SCHED_EMS)
#include <linux/ems.h>
#if defined(CONFIG_SCHED_EMS_TUNE)
#else
static struct gb_qos_request gb_req = {
	.name = "ems_boost",
};
#endif
#endif
#ifdef CONFIG_MALI_SEC_G3D_PEAK_NOTI
extern int g3d_notify_peak_mode_update(bool is_set);
static unsigned int is_peak_mode = 0;
#endif

struct pm_qos_request exynos5_g3d_mif_min_qos;
struct pm_qos_request exynos5_g3d_mif_max_qos;
struct pm_qos_request exynos5_g3d_cpu_cluster0_min_qos;
struct pm_qos_request exynos5_g3d_cpu_cluster1_max_qos;
struct pm_qos_request exynos5_g3d_cpu_cluster1_min_qos;
#if PM_QOS_CPU_CLUSTER_NUM == 3
struct pm_qos_request exynos5_g3d_cpu_cluster2_max_qos;
struct pm_qos_request exynos5_g3d_cpu_cluster2_min_qos;
#endif

#ifdef CONFIG_MALI_SUSTAINABLE_OPT
struct pm_qos_request exynos5_g3d_cpu_cluster0_max_qos;
#endif

extern struct kbase_device *pkbdev;

#ifdef CONFIG_MALI_PM_QOS
int gpu_pm_qos_command(struct exynos_context *platform, gpu_pmqos_state state)
{
	int idx;

	DVFS_ASSERT(platform);

#ifdef CONFIG_MALI_ASV_CALIBRATION_SUPPORT
	if (platform->gpu_auto_cali_status)
		return 0;
#endif

	switch (state) {
	case GPU_CONTROL_PM_QOS_INIT:
		pm_qos_add_request(&exynos5_g3d_mif_min_qos, PM_QOS_BUS_THROUGHPUT, 0);
		if (platform->pmqos_mif_max_clock)
			pm_qos_add_request(&exynos5_g3d_mif_max_qos, PM_QOS_BUS_THROUGHPUT_MAX, PM_QOS_BUS_THROUGHPUT_MAX_DEFAULT_VALUE);
		pm_qos_add_request(&exynos5_g3d_cpu_cluster0_min_qos, PM_QOS_CLUSTER0_FREQ_MIN, 0);
		pm_qos_add_request(&exynos5_g3d_cpu_cluster1_max_qos, PM_QOS_CLUSTER1_FREQ_MAX, PM_QOS_CLUSTER1_FREQ_MAX_DEFAULT_VALUE);
#if PM_QOS_CPU_CLUSTER_NUM == 2
		if (platform->boost_egl_min_lock)
			pm_qos_add_request(&exynos5_g3d_cpu_cluster1_min_qos, PM_QOS_CLUSTER1_FREQ_MIN, 0);
#endif
#if PM_QOS_CPU_CLUSTER_NUM == 3
		pm_qos_add_request(&exynos5_g3d_cpu_cluster1_min_qos, PM_QOS_CLUSTER1_FREQ_MIN, 0);
		pm_qos_add_request(&exynos5_g3d_cpu_cluster2_max_qos, PM_QOS_CLUSTER2_FREQ_MAX, PM_QOS_CLUSTER2_FREQ_MAX_DEFAULT_VALUE);
		if (platform->boost_egl_min_lock)
			pm_qos_add_request(&exynos5_g3d_cpu_cluster2_min_qos, PM_QOS_CLUSTER2_FREQ_MIN, 0);
#ifdef CONFIG_MALI_SUSTAINABLE_OPT
		pm_qos_add_request(&exynos5_g3d_cpu_cluster0_max_qos, PM_QOS_CLUSTER0_FREQ_MAX, PM_QOS_CLUSTER0_FREQ_MAX_DEFAULT_VALUE);
#endif
#endif
		for (idx = 0; idx < platform->table_size; idx++)
			platform->save_cpu_max_freq[idx] = platform->table[idx].cpu_big_max_freq;
		platform->is_pm_qos_init = true;
		break;
	case GPU_CONTROL_PM_QOS_DEINIT:
		pm_qos_remove_request(&exynos5_g3d_mif_min_qos);
		if (platform->pmqos_mif_max_clock)
			pm_qos_remove_request(&exynos5_g3d_mif_max_qos);
		pm_qos_remove_request(&exynos5_g3d_cpu_cluster0_min_qos);
		pm_qos_remove_request(&exynos5_g3d_cpu_cluster1_max_qos);
#if PM_QOS_CPU_CLUSTER_NUM == 2
		if (platform->boost_egl_min_lock)
			pm_qos_remove_request(&exynos5_g3d_cpu_cluster1_min_qos);
#endif
#if PM_QOS_CPU_CLUSTER_NUM == 3
		pm_qos_remove_request(&exynos5_g3d_cpu_cluster1_min_qos);
		pm_qos_remove_request(&exynos5_g3d_cpu_cluster2_max_qos);
		if (platform->boost_egl_min_lock)
			pm_qos_remove_request(&exynos5_g3d_cpu_cluster2_min_qos);
#ifdef CONFIG_MALI_SUSTAINABLE_OPT
		pm_qos_remove_request(&exynos5_g3d_cpu_cluster0_max_qos);
#endif
#endif
		platform->is_pm_qos_init = false;
		break;
	case GPU_CONTROL_PM_QOS_SET:
		if (!platform->is_pm_qos_init) {
			GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u, "%s: PM QOS ERROR : pm_qos deinit -> set\n", __func__);
			return -ENOENT;
		}
		KBASE_DEBUG_ASSERT(platform->step >= 0);

/* point #1 : normal job & peak stable performance mode */
/* point #2 : cl job */
#if defined(CONFIG_MALI_SEC_CL_BOOST) && defined(CONFIG_MALI_CL_PMQOS)	/* point #1 start */
		if (pkbdev->pm.backend.metrics.is_full_compute_util == 0) {
#endif
#ifdef CONFIG_MALI_TSG
		if (platform->is_pm_qos_tsg == true) {
			pm_qos_update_request(&exynos5_g3d_mif_min_qos, 0);
		} else {
			pm_qos_update_request(&exynos5_g3d_mif_min_qos, platform->table[platform->step].mem_freq);
		}
#else
		pm_qos_update_request(&exynos5_g3d_mif_min_qos, platform->table[platform->step].mem_freq);
#endif /* CONFIG_MALI_TSG */
		if (platform->pmqos_mif_max_clock &&
				(platform->table[platform->step].clock >= platform->pmqos_mif_max_clock_base))
			pm_qos_update_request(&exynos5_g3d_mif_max_qos, platform->pmqos_mif_max_clock);
#ifdef CONFIG_MALI_SEC_VK_BOOST /* VK JOB Boost */
		mutex_lock(&platform->gpu_vk_boost_lock);
		if (platform->ctx_vk_need_qos && platform->max_lock == platform->gpu_vk_boost_max_clk_lock) {
			pm_qos_update_request(&exynos5_g3d_mif_min_qos, platform->gpu_vk_boost_mif_min_clk_lock);
		}
		mutex_unlock(&platform->gpu_vk_boost_lock);
#endif

#ifdef CONFIG_MALI_SEC_G3D_PEAK_NOTI
#ifdef CONFIG_MALI_SEC_G3D_PERF_STABLE_PMQOS
		if (!is_peak_mode && (platform->cur_clock >= platform->pmqos_g3d_clock[1]))
#else
		if (!is_peak_mode && (platform->cur_clock >= platform->gpu_max_clock))
#endif
		{
			g3d_notify_peak_mode_update(true);
			is_peak_mode = 1;
		}
#ifdef CONFIG_MALI_SEC_G3D_PERF_STABLE_PMQOS
		else if(is_peak_mode && (platform->cur_clock < platform->pmqos_g3d_clock[1]))
#else
		else if(is_peak_mode && (platform->cur_clock < platform->gpu_max_clock))
#endif
		{
			g3d_notify_peak_mode_update(false);
			is_peak_mode = 0;
		}
#endif
#ifdef CONFIG_MALI_TSG
		if (platform->is_pm_qos_tsg == true)
			pm_qos_update_request(&exynos5_g3d_cpu_cluster0_min_qos, 0);
		else
			pm_qos_update_request(&exynos5_g3d_cpu_cluster0_min_qos, platform->table[platform->step].cpu_little_min_freq); // LITTLE Min request
#else
		pm_qos_update_request(&exynos5_g3d_cpu_cluster0_min_qos, platform->table[platform->step].cpu_little_min_freq); // LITTLE Min request
#endif /* CONFIG_MALI_TSG */
#if PM_QOS_CPU_CLUSTER_NUM == 3
#if defined(CONFIG_MALI_SEC_G3D_PERF_STABLE_PMQOS) && defined(CONFIG_MALI_TSG) /* Keep going stable peak performance when using higher g3d frequency */
		if (platform->stay_count_no_peak_mode == STAY_COUNT_NO_PEAK_MODE_PERIOD) {
            if (platform->cur_clock == platform->pmqos_g3d_clock[0]) {
                platform->gpu_operation_mode_info = GL_PEAK_MODE1;
                pm_qos_update_request(&exynos5_g3d_cpu_cluster2_max_qos, platform->pmqos_cl2_max_clock[0]); /* Big Max request */
                pm_qos_update_request(&exynos5_g3d_cpu_cluster1_max_qos, platform->pmqos_cl1_max_clock[0]); /* Middle Max request */
            } else if (platform->cur_clock == platform->pmqos_g3d_clock[1]) {
                platform->gpu_operation_mode_info = GL_PEAK_MODE2;
                pm_qos_update_request(&exynos5_g3d_cpu_cluster2_max_qos, platform->pmqos_cl2_max_clock[1]); /* Big Max request */
                pm_qos_update_request(&exynos5_g3d_cpu_cluster1_max_qos, platform->pmqos_cl1_max_clock[1]); /* Middle Max request */
            }
        } else {
			if (platform->is_pm_qos_tsg == true) {
				platform->gpu_operation_mode_info = GL_MIGOV_MODE;
				pm_qos_update_request(&exynos5_g3d_cpu_cluster1_min_qos, 0);
				pm_qos_update_request(&exynos5_g3d_cpu_cluster1_max_qos, PM_QOS_CLUSTER1_FREQ_MAX_DEFAULT_VALUE);   /* Middle Max request */
            	pm_qos_update_request(&exynos5_g3d_cpu_cluster2_max_qos, PM_QOS_CLUSTER2_FREQ_MAX_DEFAULT_VALUE);
        	}
			else{
            	platform->gpu_operation_mode_info = GL_NORMAL;
            	pm_qos_update_request(&exynos5_g3d_cpu_cluster1_min_qos, platform->table[platform->step].cpu_middle_min_freq); /* MIDDLE Min request */
            	pm_qos_update_request(&exynos5_g3d_cpu_cluster1_max_qos, PM_QOS_CLUSTER1_FREQ_MAX_DEFAULT_VALUE);   /* Middle Max request */
            	pm_qos_update_request(&exynos5_g3d_cpu_cluster2_max_qos, platform->table[platform->step].cpu_big_max_freq); /* BIG Max request */
        	}
		}
#elif defined(CONFIG_MALI_TSG)
		if (platform->is_pm_qos_tsg == true) {
			platform->gpu_operation_mode_info = GL_MIGOV_MODE;
			pm_qos_update_request(&exynos5_g3d_cpu_cluster1_min_qos, 0);
			pm_qos_update_request(&exynos5_g3d_cpu_cluster1_max_qos, PM_QOS_CLUSTER1_FREQ_MAX_DEFAULT_VALUE);   /* Middle Max request */
			pm_qos_update_request(&exynos5_g3d_cpu_cluster2_max_qos, PM_QOS_CLUSTER2_FREQ_MAX_DEFAULT_VALUE);
		} else {
			platform->gpu_operation_mode_info = GL_NORMAL;
			pm_qos_update_request(&exynos5_g3d_cpu_cluster1_min_qos, platform->table[platform->step].cpu_middle_min_freq); /* MIDDLE Min request */
			pm_qos_update_request(&exynos5_g3d_cpu_cluster1_max_qos, PM_QOS_CLUSTER1_FREQ_MAX_DEFAULT_VALUE); /* Middle Max request */
			pm_qos_update_request(&exynos5_g3d_cpu_cluster2_max_qos, platform->table[platform->step].cpu_big_max_freq); /* BIG Max request */
}
#elif defined(CONFIG_MALI_SEC_G3D_PERF_STABLE_PMQOS)
		if (platform->stay_count_no_peak_mode == STAY_COUNT_NO_PEAK_MODE_PERIOD) {
			if (platform->cur_clock == platform->pmqos_g3d_clock[0]) {
				platform->gpu_operation_mode_info = GL_PEAK_MODE1;
				pm_qos_update_request(&exynos5_g3d_cpu_cluster2_max_qos, platform->pmqos_cl2_max_clock[0]); /* Big Max request */
				pm_qos_update_request(&exynos5_g3d_cpu_cluster1_max_qos, platform->pmqos_cl1_max_clock[0]); /* Middle Max request */
			} else if (platform->cur_clock == platform->pmqos_g3d_clock[1]) {
				platform->gpu_operation_mode_info = GL_PEAK_MODE2;
				pm_qos_update_request(&exynos5_g3d_cpu_cluster2_max_qos, platform->pmqos_cl2_max_clock[1]); /* Big Max request */
				pm_qos_update_request(&exynos5_g3d_cpu_cluster1_max_qos, platform->pmqos_cl1_max_clock[1]); /* Middle Max request */
			}
		} else {
			platform->gpu_operation_mode_info = GL_NORMAL;
			pm_qos_update_request(&exynos5_g3d_cpu_cluster1_min_qos, platform->table[platform->step].cpu_middle_min_freq); /* MIDDLE Min request */
			pm_qos_update_request(&exynos5_g3d_cpu_cluster1_max_qos, PM_QOS_CLUSTER1_FREQ_MAX_DEFAULT_VALUE);	/* Middle Max request */
			pm_qos_update_request(&exynos5_g3d_cpu_cluster2_max_qos, platform->table[platform->step].cpu_big_max_freq); /* BIG Max request */
		}
#else
		platform->gpu_operation_mode_info = GL_NORMAL;
		pm_qos_update_request(&exynos5_g3d_cpu_cluster1_min_qos, platform->table[platform->step].cpu_middle_min_freq); /* MIDDLE Min request */
		pm_qos_update_request(&exynos5_g3d_cpu_cluster1_max_qos, PM_QOS_CLUSTER1_FREQ_MAX_DEFAULT_VALUE);	/* Middle Max request */
		pm_qos_update_request(&exynos5_g3d_cpu_cluster2_max_qos, platform->table[platform->step].cpu_big_max_freq); /* BIG Max request */
#endif
#ifdef CONFIG_MALI_SUSTAINABLE_OPT
#if 0
		if (platform->sustainable.info_array[0] > 0) {
			if (((platform->cur_clock <= platform->sustainable.info_array[0])
						|| (platform->max_lock == platform->sustainable.info_array[0]))
					&& platform->env_data.utilization > platform->sustainable.info_array[1]) {
				platform->sustainable.status = true;
				if (platform->sustainable.info_array[2] != 0)
					pm_qos_update_request(&exynos5_g3d_cpu_cluster0_max_qos, platform->sustainable.info_array[2]);
				if (platform->sustainable.info_array[3] != 0)
					pm_qos_update_request(&exynos5_g3d_cpu_cluster1_max_qos, platform->sustainable.info_array[3]);
				if (platform->sustainable.info_array[4] != 0)
					pm_qos_update_request(&exynos5_g3d_cpu_cluster2_max_qos, platform->sustainable.info_array[4]);
			} else {
				platform->sustainable.status = false;
				pm_qos_update_request(&exynos5_g3d_cpu_cluster0_max_qos, PM_QOS_CLUSTER0_FREQ_MAX_DEFAULT_VALUE);
				pm_qos_update_request(&exynos5_g3d_cpu_cluster1_max_qos, PM_QOS_CLUSTER1_FREQ_MAX_DEFAULT_VALUE);
				pm_qos_update_request(&exynos5_g3d_cpu_cluster2_max_qos, platform->table[platform->step].cpu_big_max_freq);
			}
		}
#endif
#endif
#if defined(CONFIG_MALI_SEC_CL_BOOST) && defined(CONFIG_MALI_CL_PMQOS)
		}	/* point #1 end */
#endif

#if defined(CONFIG_MALI_SEC_CL_BOOST) && defined(CONFIG_MALI_CL_PMQOS)	/* point #2 start */
		/*	if (pkbdev->pm.backend.metrics.is_full_compute_util && platform->cl_boost_disable == false)	{	*/
		if (pkbdev->pm.backend.metrics.is_full_compute_util)	{
			platform->gpu_operation_mode_info = CL_FULL;
			pm_qos_update_request(&exynos5_g3d_mif_max_qos, platform->pmqos_compute_mif_max_clock);	/* MIF Min request */
			pm_qos_update_request(&exynos5_g3d_mif_min_qos, platform->cl_pmqos_table[platform->step].mif_min_freq);	/* MIF Min request */
			pm_qos_update_request(&exynos5_g3d_cpu_cluster0_min_qos, platform->cl_pmqos_table[platform->step].cpu_little_min_freq); /* LIT Min request */
#if PM_QOS_CPU_CLUSTER_NUM == 2
			/* pm_qos_update_request(&exynos5_g3d_cpu_cluster1_max_qos, platform->cl_pmqos_table[platform->step].cpu_big_max_freq); *//* BIG Max request */
			pm_qos_update_request(&exynos5_g3d_cpu_cluster1_max_qos, PM_QOS_CLUSTER1_FREQ_MAX_DEFAULT_VALUE);
#endif
#if PM_QOS_CPU_CLUSTER_NUM == 3
			pm_qos_update_request(&exynos5_g3d_cpu_cluster1_min_qos, platform->cl_pmqos_table[platform->step].cpu_middle_min_freq); /* MID Min request */
			pm_qos_update_request(&exynos5_g3d_cpu_cluster1_max_qos, PM_QOS_CLUSTER1_FREQ_MAX_DEFAULT_VALUE);	/* Middle Max request */
			/* pm_qos_update_request(&exynos5_g3d_cpu_cluster2_max_qos, platform->cl_pmqos_table[platform->step].cpu_big_max_freq); *//* BIG Max request */
			pm_qos_update_request(&exynos5_g3d_cpu_cluster2_max_qos, PM_QOS_CLUSTER2_FREQ_MAX_DEFAULT_VALUE);	/* Big Max request */
#endif
		}
#endif	/* point #2 end */

#ifdef CONFIG_MALI_SEC_CL_BOOST
		platform->previous_cl_job = pkbdev->pm.backend.metrics.is_full_compute_util;
#endif

#if 0
#ifdef CONFIG_MALI_SEC_CL_BOOST
		if (pkbdev->pm.backend.metrics.is_full_compute_util && platform->cl_boost_disable == false)
			pm_qos_update_request(&exynos5_g3d_cpu_cluster2_max_qos, PM_QOS_CLUSTER2_FREQ_MAX_DEFAULT_VALUE);
#endif
#endif
#endif
		break;
	case GPU_CONTROL_PM_QOS_RESET:
		if (!platform->is_pm_qos_init) {
			GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u, "%s: PM QOS ERROR : pm_qos deinit -> reset\n", __func__);
			return -ENOENT;
		}
		pm_qos_update_request(&exynos5_g3d_mif_min_qos, 0);
		if (platform->pmqos_mif_max_clock)
			pm_qos_update_request(&exynos5_g3d_mif_max_qos, PM_QOS_BUS_THROUGHPUT_MAX_DEFAULT_VALUE);
		pm_qos_update_request(&exynos5_g3d_cpu_cluster0_min_qos, 0);
		pm_qos_update_request(&exynos5_g3d_cpu_cluster1_max_qos, PM_QOS_CLUSTER1_FREQ_MAX_DEFAULT_VALUE);
#if PM_QOS_CPU_CLUSTER_NUM == 3
		pm_qos_update_request(&exynos5_g3d_cpu_cluster1_min_qos, 0);
		pm_qos_update_request(&exynos5_g3d_cpu_cluster2_max_qos, PM_QOS_CLUSTER2_FREQ_MAX_DEFAULT_VALUE);
#ifdef CONFIG_MALI_SUSTAINABLE_OPT
#if 0
		pm_qos_update_request(&exynos5_g3d_cpu_cluster0_max_qos, PM_QOS_CLUSTER0_FREQ_MAX_DEFAULT_VALUE);
		pm_qos_update_request(&exynos5_g3d_cpu_cluster1_max_qos, PM_QOS_CLUSTER1_FREQ_MAX_DEFAULT_VALUE);
		pm_qos_update_request(&exynos5_g3d_cpu_cluster2_max_qos, PM_QOS_CLUSTER2_FREQ_MAX_DEFAULT_VALUE);
#endif
#endif
#endif
		break;
	case GPU_CONTROL_PM_QOS_EGL_SET:
		if (!platform->is_pm_qos_init) {
			GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u, "%s: PM QOS ERROR : pm_qos deinit -> egl_set\n", __func__);
			return -ENOENT;
		}
		pm_qos_update_request_timeout(&exynos5_g3d_cpu_cluster1_min_qos, platform->boost_egl_min_lock, 30000);
		for (idx = 0; idx < platform->table_size; idx++) {
			platform->table[idx].cpu_big_max_freq = PM_QOS_CLUSTER1_FREQ_MAX_DEFAULT_VALUE;
		}
#if PM_QOS_CPU_CLUSTER_NUM == 3
		pm_qos_update_request_timeout(&exynos5_g3d_cpu_cluster2_min_qos, platform->boost_egl_min_lock, 30000);
		for (idx = 0; idx < platform->table_size; idx++) {
			platform->table[idx].cpu_big_max_freq = PM_QOS_CLUSTER2_FREQ_MAX_DEFAULT_VALUE;
		}
#endif
		break;
	case GPU_CONTROL_PM_QOS_EGL_RESET:
		if (!platform->is_pm_qos_init) {
			GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u, "%s: PM QOS ERROR : pm_qos deinit -> egl_reset\n", __func__);
			return -ENOENT;
		}
		for (idx = 0; idx < platform->table_size; idx++)
			platform->table[idx].cpu_big_max_freq = platform->save_cpu_max_freq[idx];
		break;
	default:
		break;
	}

	return 0;
}
#endif
