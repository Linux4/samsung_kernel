// SPDX-License-Identifier: GPL-2.0-only
/*
 * @file sgpu_pm_monitor.h
 * Copyright (C) 2024 Samsung Electronics
 */

#ifndef __SGPU_PM_MONITOR_H__
#define __SGPU_PM_MONITOR_H__

/* power states */
enum sgpu_power_states {
	SGPU_POWER_STATE_ON = 0,
	SGPU_POWER_STATE_IFPO,
	SGPU_POWER_STATE_SUSPEND,

	SGPU_POWER_STATE_NR
};

struct sgpu_pm_stats {
	enum sgpu_power_states	power_status;
	u64			state_time[SGPU_POWER_STATE_NR];
	u64			last_update;
};

struct sgpu_pm_monitor {
	spinlock_t		lock;
	struct sgpu_pm_stats	pm_stats;

	/* data for sgpu_pm_monitor sysfs node */
	struct sgpu_pm_stats	base_stats;
	u64			state_duration[SGPU_POWER_STATE_NR];
	u64			monitor_duration;
	bool			monitor_enable;

#ifdef CONFIG_DEBUG_FS
	struct dentry		*debugfs_power_state;
#endif
};

#ifdef CONFIG_DRM_SGPU_EXYNOS

void sgpu_pm_monitor_init(struct amdgpu_device *adev);
void sgpu_pm_monitor_update(struct amdgpu_device *adev, u32 new_status);
void sgpu_pm_monitor_get_stats(struct amdgpu_device *adev,
				struct sgpu_pm_stats *pm_stats);
uint64_t sgpu_pm_monitor_get_idle_time(struct amdgpu_device *adev);

#else

#define sgpu_pm_monitor_init(adev)			do { } while (0)
#define sgpu_pm_monitor_update(adev, new_status)	do { } while (0)
#define sgpu_pm_monitor_get_stats(adev, pm_stats)	do { } while (0)
#define sgpu_pm_monitor_get_idle_time(adev)		(0)

#endif /* CONFIG_DRM_SGPU_EXYNOS */

#endif /* _SGPU_PM_MONITOR_H_ */
