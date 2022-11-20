/* drivers/gpu/arm/.../platform/gpu_control.h
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
 * @file gpu_control.h
 * DVFS
 */

#ifndef _GPU_CONTROL_H_
#define _GPU_CONTROL_H_

int get_cpu_clock_speed(u32 *cpu_clock);
int gpu_control_set_voltage(struct kbase_device *kbdev, int voltage);
int gpu_control_set_clock(struct kbase_device *kbdev, int clock);
int gpu_control_enable_clock(struct kbase_device *kbdev);
int gpu_control_disable_clock(struct kbase_device *kbdev);
int gpu_control_is_power_on(struct kbase_device *kbdev);

int gpu_is_power_on(void);
int gpu_power_init(struct kbase_device *kbdev);
int gpu_get_cur_voltage(struct exynos_context *platform);
int gpu_get_cur_clock(struct exynos_context *platform);
int gpu_clock_init(struct kbase_device *kbdev);
int gpu_control_ops_init(struct exynos_context *platform);

int gpu_control_enable_customization(struct kbase_device *kbdev);
int gpu_control_disable_customization(struct kbase_device *kbdev);

int gpu_regulator_init(struct exynos_context *platform);

int gpu_control_module_init(struct kbase_device *kbdev);
void gpu_control_module_term(struct kbase_device *kbdev);

#endif /* _GPU_CONTROL_H_ */
