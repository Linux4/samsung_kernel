/* drivers/gpu/arm/.../platform/gpu_control.c
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
 * @file gpu_control.c
 * DVFS
 */

#include <mali_kbase.h>

#include <linux/pm_qos.h>
#include <mach/pm_domains_v2.h>

#include "mali_kbase_platform.h"
#include "gpu_dvfs_handler.h"
#include "gpu_control.h"

#ifdef CONFIG_MALI_RT_PM
static struct exynos_pm_domain *gpu_get_pm_domain(void)
{
	struct exynos_pm_domain *pd = NULL;
	pd = exynos_get_power_domain("pd-g3d");
	return pd;
}
#endif /* CONFIG_MALI_RT_PM */

int get_cpu_clock_speed(u32 *cpu_clock)
{
	struct clk *cpu_clk;
	u32 freq = 0;
	cpu_clk = clk_get(NULL, "armclk");
	if (IS_ERR(cpu_clk))
		return -1;
	freq = clk_get_rate(cpu_clk);
	*cpu_clock = (freq/MHZ);
	return 0;
}

int gpu_control_set_voltage(struct kbase_device *kbdev, int voltage)
{
	int ret = 0;
	bool is_up = false;
	static int prev_voltage = -1;
	struct exynos_context *platform = (struct exynos_context *) kbdev->platform_context;
	if (!platform) {
		GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u, "%s: platform context is null\n", __func__);
		return -ENODEV;
	}

	if (voltage < 0) {
		GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u, "%s: invalid voltage error (%d)\n", __func__, voltage);
		return -1;
	}

	is_up = prev_voltage < voltage;

	if (platform->ctr_ops.set_voltage_pre)
		platform->ctr_ops.set_voltage_pre(platform, is_up);

	if (platform->ctr_ops.set_voltage)
		ret = platform->ctr_ops.set_voltage(platform, voltage);

	if (platform->ctr_ops.set_voltage_post)
		platform->ctr_ops.set_voltage_post(platform, is_up);

	prev_voltage = voltage;

	return ret;
}

int gpu_control_set_clock(struct kbase_device *kbdev, int clock)
{
	int ret = 0;
	bool is_up = false;
	static int prev_clock = -1;
	struct exynos_context *platform = (struct exynos_context *) kbdev->platform_context;
	if (!platform) {
		GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u, "%s: platform context is null\n", __func__);
		return -ENODEV;
	}

	if (gpu_dvfs_get_level(clock) < 0) {
		GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u, "%s: mismatch clock error (%d)\n", __func__, clock);
		return -1;
	}

	is_up = prev_clock < clock;

	if (platform->ctr_ops.set_clock_pre)
		platform->ctr_ops.set_clock_pre(platform, clock, is_up);

	if (platform->ctr_ops.set_clock)
		ret = platform->ctr_ops.set_clock(platform, clock);

	if (platform->ctr_ops.set_clock_post)
		platform->ctr_ops.set_clock_post(platform, clock, is_up);

#ifdef CONFIG_MALI_DVFS
	if (!ret)
		gpu_pm_qos_command(platform, GPU_CONTROL_PM_QOS_SET);
#endif /* CONFIG_MALI_DVFS */

	gpu_dvfs_update_time_in_state(prev_clock);
	prev_clock = clock;

	return ret;
}

int gpu_control_enable_clock(struct kbase_device *kbdev)
{
	int ret = 0;
	struct exynos_context *platform = (struct exynos_context *) kbdev->platform_context;
	if (!platform) {
		GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u, "%s: platform context is null\n", __func__);
		return -ENODEV;
	}

	mutex_lock(&platform->gpu_clock_lock);
	if (platform->ctr_ops.enable_clock)
		ret = platform->ctr_ops.enable_clock(platform);
	mutex_unlock(&platform->gpu_clock_lock);

	gpu_dvfs_update_time_in_state(0);

	return ret;
}

int gpu_control_disable_clock(struct kbase_device *kbdev)
{
	int ret = 0;
	struct exynos_context *platform = (struct exynos_context *) kbdev->platform_context;
	if (!platform) {
		GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u, "%s: platform context is null\n", __func__);
		return -ENODEV;
	}

	mutex_lock(&platform->gpu_clock_lock);
	if (platform->ctr_ops.disable_clock)
		ret = platform->ctr_ops.disable_clock(platform);
	mutex_unlock(&platform->gpu_clock_lock);

	gpu_dvfs_update_time_in_state(platform->cur_clock);
#ifdef CONFIG_MALI_DVFS
	gpu_pm_qos_command(platform, GPU_CONTROL_PM_QOS_RESET);
#endif /* CONFIG_MALI_DVFS */

	return ret;
}

int gpu_control_is_power_on(struct kbase_device *kbdev)
{
	int ret = 0;
	struct exynos_context *platform = (struct exynos_context *) kbdev->platform_context;
	if (!platform) {
		GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u, "%s: platform context is null\n", __func__);
		return -ENODEV;
	}

	mutex_lock(&platform->gpu_clock_lock);
	if (platform->ctr_ops.is_power_on)
		ret = platform->ctr_ops.is_power_on();
	mutex_unlock(&platform->gpu_clock_lock);

	return ret;
}

int gpu_control_module_init(struct kbase_device *kbdev)
{
	struct exynos_context *platform = (struct exynos_context *)kbdev->platform_context;
	if (!platform)
		return -ENODEV;

#ifdef CONFIG_MALI_RT_PM
	platform->exynos_pm_domain = gpu_get_pm_domain();
#endif /* CONFIG_MALI_RT_PM */

	if (gpu_control_ops_init(platform) < 0) {
		GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u, "%s: failed to initialize ctr_ops\n", __func__);
		goto out;
	}

	if (gpu_power_init(kbdev) < 0) {
		GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u, "%s: failed to initialize power\n", __func__);
		goto out;
	}

	if (gpu_clock_init(kbdev) < 0) {
		GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u, "%s: failed to initialize clock\n", __func__);
		goto out;
	}

#ifdef CONFIG_REGULATOR
	if (gpu_regulator_init(platform) < 0) {
		GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u, "%s: failed to initialize regulator\n", __func__);
		goto out;
	}
#endif /* CONFIG_REGULATOR */

	return 0;
out:
	return -EPERM;
}

void gpu_control_module_term(struct kbase_device *kbdev)
{
	struct exynos_context *platform = (struct exynos_context *)kbdev->platform_context;
	if (!platform)
		return;

#ifdef CONFIG_MALI_RT_PM
	platform->exynos_pm_domain = NULL;
#endif /* CONFIG_MALI_RT_PM */
}
