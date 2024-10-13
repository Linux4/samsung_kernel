/*
 * Samsung Exynos SoC series NPU driver
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/version.h>
#include <linux/delay.h>
#include <linux/random.h>
#include <linux/printk.h>
#include <linux/cpuidle.h>
#include <soc/samsung/bts.h>
#if IS_ENABLED(CONFIG_EXYNOS_PM_QOS)
#include <soc/samsung/exynos-devfreq.h>
#endif

#include "npu-config.h"
#include "npu-scheduler-governor.h"
#include "npu-device.h"
#include "npu-llc.h"
#include "npu-util-regs.h"
#include "npu-hw-device.h"

u32 npu_dvfs_devfreq_get_domain_freq(struct npu_scheduler_dvfs_info *d);
int npu_dvfs_cmd_map(struct npu_scheduler_info *info, const char *cmd_name);
void npu_dvfs_pm_qos_add_request(struct exynos_pm_qos_request *req,
					int exynos_pm_qos_class, s32 value);
#if IS_ENABLED(CONFIG_SOC_S5E9945)
void npu_dvfs_update_token_status(struct npu_system *system);
int npu_dvfs_token_work_probe(struct npu_device *device);
#endif
extern void npu_dvfs_pm_qos_update_request_boost(struct exynos_pm_qos_request *req,
							s32 new_value);
extern void npu_dvfs_pm_qos_update_request(struct exynos_pm_qos_request *req,
							s32 new_value);
int npu_dvfs_set_freq(struct npu_scheduler_dvfs_info *d, void *req, u32 freq);
int npu_dvfs_set_freq_boost(struct npu_scheduler_dvfs_info *d, void *req, u32 freq);
int npu_dvfs_pm_qos_get_class(struct exynos_pm_qos_request *req);
int npu_dvfs_init_cmd_list(struct npu_system *system, struct npu_scheduler_info *info);
void npu_dvfs_unset_freq(struct npu_scheduler_info *info);
void npu_dvfs_activate_peripheral(unsigned long freq);
#if IS_ENABLED(CONFIG_NPU_USE_IFD)
void npu_dvfs_unset_freq_work(struct work_struct *work);
#else
int npu_dvfs_set_mode_freq(struct npu_scheduler_info *info, int uid);
void npu_dvfs_set_initial_freq(struct npu_device *device, npu_uid_t session_uid);
#endif

#if !IS_ENABLED(CONFIG_NPU_USE_IFD) && !IS_ENABLED(CONFIG_NPU_GOVERNOR)
u32 npu_dvfs_get_freq_ceil(struct npu_scheduler_dvfs_info *d, u32 freq);
#endif
