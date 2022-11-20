/* drivers/gpu/arm/.../platform/gpu_notifier.c
 *
 * Copyright 2011 by S.LSI. Samsung Electronics Inc.
 * San#24, Nongseo-Dong, Giheung-Gu, Yongin, Korea
 *
 * Samsung SoC Mali-T Series platform-dependent codes
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software FoundatIon.
 */

/**
 * @file gpu_notifier.c
 */

#include <mali_kbase.h>

#include <linux/suspend.h>
#include <linux/pm_runtime.h>

#include "mali_kbase_platform.h"
#include "gpu_dvfs_handler.h"
#include "gpu_notifier.h"
#include "gpu_control.h"

#ifdef CONFIG_EXYNOS_THERMAL
#include <mach/tmu.h>
#endif /* CONFIG_EXYNOS_THERMAL */

extern struct kbase_device *pkbdev;

#ifdef CONFIG_EXYNOS_THERMAL
static int gpu_tmu_hot_check_and_work(struct kbase_device *kbdev, unsigned long event)
{
#ifdef CONFIG_MALI_DVFS
	struct exynos_context *platform;
	int lock_clock;

	KBASE_DEBUG_ASSERT(kbdev != NULL);

	platform = (struct exynos_context *)kbdev->platform_context;
	if (!platform)
		return -ENODEV;

	switch (event) {
	case GPU_THROTTLING1:
		lock_clock = platform->tmu_lock_clk[THROTTLING1];
		GPU_LOG(DVFS_INFO, DUMMY, 0u, 0u, "THROTTLING1\n");
		break;
	case GPU_THROTTLING2:
		lock_clock = platform->tmu_lock_clk[THROTTLING2];
		GPU_LOG(DVFS_INFO, DUMMY, 0u, 0u, "THROTTLING2\n");
		break;
	case GPU_THROTTLING3:
		lock_clock = platform->tmu_lock_clk[THROTTLING3];
		GPU_LOG(DVFS_INFO, DUMMY, 0u, 0u, "THROTTLING3\n");
		break;
	case GPU_THROTTLING4:
		lock_clock = platform->tmu_lock_clk[THROTTLING4];
		GPU_LOG(DVFS_INFO, DUMMY, 0u, 0u, "THROTTLING4\n");
		break;
	case GPU_TRIPPING:
		lock_clock = platform->tmu_lock_clk[TRIPPING];
		GPU_LOG(DVFS_INFO, DUMMY, 0u, 0u, "TRIPPING\n");
		break;
	default:
		GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u, "%s: wrong event, %lu\n", __func__, event);
		return 0;
	}

	gpu_dvfs_clock_lock(GPU_DVFS_MAX_LOCK, TMU_LOCK, lock_clock);
#endif /* CONFIG_MALI_DVFS */
	return 0;
}

static void gpu_tmu_normal_work(struct kbase_device *kbdev)
{
#ifdef CONFIG_MALI_DVFS
	struct exynos_context *platform = (struct exynos_context *)kbdev->platform_context;
	if (!platform)
		return;

	gpu_dvfs_clock_lock(GPU_DVFS_MAX_UNLOCK, TMU_LOCK, 0);
#endif /* CONFIG_MALI_DVFS */
}

static int gpu_tmu_notifier(struct notifier_block *notifier,
				unsigned long event, void *v)
{
	struct exynos_context *platform = (struct exynos_context *)pkbdev->platform_context;
	if (!platform)
		return -ENODEV;

	if (!platform->tmu_status)
		return NOTIFY_OK;

	platform->voltage_margin = 0;

	if (event == GPU_COLD) {
		platform->voltage_margin = platform->gpu_default_vol_margin;
	} else if (event == GPU_NORMAL) {
		gpu_tmu_normal_work(pkbdev);
	} else if (event >= GPU_THROTTLING1 && event <= GPU_TRIPPING) {
		if (gpu_tmu_hot_check_and_work(pkbdev, event))
			GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u, "%s: failed to open device", __func__);
	}

	GPU_LOG(DVFS_DEBUG, LSI_TMU_VALUE, 0u, event, "tmu event %ld\n", event);

	gpu_set_target_clk_vol(platform->cur_clock, false);

	return NOTIFY_OK;
}

static struct notifier_block gpu_tmu_nb = {
	.notifier_call = gpu_tmu_notifier,
};
#endif /* CONFIG_EXYNOS_THERMAL */

#ifdef CONFIG_MALI_RT_PM
static int gpu_pm_notifier(struct notifier_block *nb, unsigned long event, void *cmd)
{
	int err = NOTIFY_OK;
	switch (event) {
	case PM_SUSPEND_PREPARE:
		GPU_LOG(DVFS_DEBUG, LSI_SUSPEND, 0u, 0u, "%s: suspend event\n", __func__);
		break;
	case PM_POST_SUSPEND:
		GPU_LOG(DVFS_DEBUG, LSI_RESUME, 0u, 0u, "%s: resume event\n", __func__);
		break;
	default:
		break;
	}
	return err;
}

static int gpu_power_on(struct kbase_device *kbdev)
{
	struct exynos_context *platform = (struct exynos_context *) kbdev->platform_context;
	if (!platform)
		return -ENODEV;

	GPU_LOG(DVFS_INFO, DUMMY, 0u, 0u, "power on\n");

	gpu_control_disable_customization(kbdev);

	if (pm_runtime_resume(kbdev->dev)) {
		if (platform->early_clk_gating_status) {
			GPU_LOG(DVFS_INFO, DUMMY, 0u, 0u, "already power on\n");
			gpu_control_enable_clock(kbdev);
		}
		return 0;
	} else {
		return 1;
	}
}

static void gpu_power_off(struct kbase_device *kbdev)
{
	struct exynos_context *platform = (struct exynos_context *) kbdev->platform_context;
	if (!platform)
		return;

	GPU_LOG(DVFS_INFO, DUMMY, 0u, 0u, "power off\n");
	gpu_control_enable_customization(kbdev);

	pm_schedule_suspend(kbdev->dev, platform->runtime_pm_delay_time);

	if (platform->early_clk_gating_status)
		gpu_control_disable_clock(kbdev);
}

static struct notifier_block gpu_pm_nb = {
	.notifier_call = gpu_pm_notifier
};

static mali_error gpu_device_runtime_init(struct kbase_device *kbdev)
{
	pm_suspend_ignore_children(kbdev->dev, true);
	return 0;
}

static void gpu_device_runtime_disable(struct kbase_device *kbdev)
{
	pm_runtime_disable(kbdev->dev);
}

static int pm_callback_runtime_on(struct kbase_device *kbdev)
{
	struct exynos_context *platform = (struct exynos_context *) kbdev->platform_context;
	if (!platform)
		return -ENODEV;

	GPU_LOG(DVFS_INFO, LSI_GPU_ON, 0u, 0u, "runtime on callback\n");

	gpu_control_enable_clock(kbdev);
	gpu_dvfs_start_env_data_gathering(kbdev);
#ifdef CONFIG_MALI_DVFS
	gpu_dvfs_timer_control_locked(true);
	if (platform->dvfs_pending)
		platform->dvfs_pending = 0;

	if (platform->dvfs_status && platform->wakeup_lock)
		gpu_set_target_clk_vol(platform->gpu_dvfs_start_clock, false);
	else
#endif /* CONFIG_MALI_DVFS */
		gpu_set_target_clk_vol(platform->cur_clock, false);

	return 0;
}

static void pm_callback_runtime_off(struct kbase_device *kbdev)
{
	struct exynos_context *platform = (struct exynos_context *) kbdev->platform_context;
	if (!platform)
		return;

	GPU_LOG(DVFS_INFO, LSI_GPU_OFF, 0u, 0u, "runtime off callback\n");

	gpu_dvfs_stop_env_data_gathering(kbdev);
#ifdef CONFIG_MALI_DVFS
	gpu_dvfs_timer_control_locked(false);
	if (platform->dvfs_pending)
		platform->dvfs_pending = 0;
#endif /* CONFIG_MALI_DVFS */
	if (!platform->early_clk_gating_status)
		gpu_control_disable_clock(kbdev);
}

kbase_pm_callback_conf pm_callbacks = {
	.power_on_callback = gpu_power_on,
	.power_off_callback = gpu_power_off,
#ifdef CONFIG_MALI_RT_PM
	.power_runtime_init_callback = gpu_device_runtime_init,
	.power_runtime_term_callback = gpu_device_runtime_disable,
	.power_runtime_on_callback = pm_callback_runtime_on,
	.power_runtime_off_callback = pm_callback_runtime_off,
#else /* CONFIG_MALI_RT_PM */
	.power_runtime_init_callback = NULL,
	.power_runtime_term_callback = NULL,
	.power_runtime_on_callback = NULL,
	.power_runtime_off_callback = NULL,
#endif /* CONFIG_MALI_RT_PM */
};
#endif /* CONFIG_MALI_RT_PM */

int gpu_notifier_init(struct kbase_device *kbdev)
{
	struct exynos_context *platform = (struct exynos_context *)kbdev->platform_context;
	if (!platform)
		return -ENODEV;

	platform->voltage_margin = 0;
#ifdef CONFIG_EXYNOS_THERMAL
	exynos_gpu_add_notifier(&gpu_tmu_nb);
#endif /* CONFIG_EXYNOS_THERMAL */

#ifdef CONFIG_MALI_RT_PM
	if (register_pm_notifier(&gpu_pm_nb))
		return -1;
#endif /* CONFIG_MALI_RT_PM */

	pm_runtime_enable(kbdev->dev);

	return 0;
}

void gpu_notifier_term(void)
{
#ifdef CONFIG_MALI_RT_PM
	unregister_pm_notifier(&gpu_pm_nb);
#endif /* CONFIG_MALI_RT_PM */
	return;
}
