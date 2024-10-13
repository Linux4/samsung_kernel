/* drivers/gpu/arm/.../platform/gpu_dvfs_handler.c
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
 * @file gpu_dvfs_handler.c
 * DVFS
 */

#include <mali_kbase.h>

#include "mali_kbase_platform.h"
#include "gpu_control.h"
#include "gpu_dvfs_handler.h"
#include "gpu_dvfs_governor.h"

extern struct kbase_device *pkbdev;

static void gpu_dvfs_event_proc(struct work_struct *q)
{
	struct kbase_device *kbdev = pkbdev;
	struct exynos_context *platform;

	platform = (struct exynos_context *) kbdev->platform_context;

	DVFS_ASSERT(platform);

	gpu_dvfs_calculate_env_data(kbdev);

#ifdef CONFIG_MALI_DVFS
	mutex_lock(&platform->gpu_dvfs_handler_lock);
	if (gpu_control_is_power_on(kbdev)) {
		int clk = 0;
		clk = gpu_dvfs_decide_next_freq(kbdev, platform->env_data.utilization);
		gpu_set_target_clk_vol(clk, true);
	}
	mutex_unlock(&platform->gpu_dvfs_handler_lock);
#endif /* CONFIG_MALI_DVFS */
}
static DECLARE_WORK(gpu_dvfs_work, gpu_dvfs_event_proc);

int kbase_platform_dvfs_event(struct kbase_device *kbdev, u32 utilisation)
{
	struct exynos_context *platform = (struct exynos_context *) kbdev->platform_context;

	DVFS_ASSERT(platform);

	if (platform->dvfs_wq)
		queue_work_on(0, platform->dvfs_wq, &gpu_dvfs_work);

	GPU_LOG(DVFS_DEBUG, DUMMY, 0u, 0u, "dvfs hanlder is called\n");

	return 0;
}

#ifdef CONFIG_MALI_DVFS
int gpu_dvfs_handler_init(struct kbase_device *kbdev)
{
	struct exynos_context *platform = (struct exynos_context *) kbdev->platform_context;

	DVFS_ASSERT(platform);

	if (!platform->dvfs_wq)
		platform->dvfs_wq = create_singlethread_workqueue("g3d_dvfs");

	if (!platform->dvfs_status)
		platform->dvfs_status = true;

	gpu_pm_qos_command(platform, GPU_CONTROL_PM_QOS_INIT);

	gpu_set_target_clk_vol(platform->table[platform->step].clock, false);

	GPU_LOG(DVFS_INFO, DUMMY, 0u, 0u, "dvfs handler initialized\n");
	return 0;
}

int gpu_dvfs_handler_deinit(struct kbase_device *kbdev)
{
	struct exynos_context *platform = (struct exynos_context *) kbdev->platform_context;

	DVFS_ASSERT(platform);

	if (platform->dvfs_wq)
		destroy_workqueue(platform->dvfs_wq);
	platform->dvfs_wq = NULL;

	if (platform->dvfs_status)
		platform->dvfs_status = false;

	gpu_pm_qos_command(platform, GPU_CONTROL_PM_QOS_DEINIT);

	GPU_LOG(DVFS_INFO, DUMMY, 0u, 0u, "dvfs handler de-initialized\n");
	return 0;
}
#endif /* CONFIG_MALI_DVFS */
