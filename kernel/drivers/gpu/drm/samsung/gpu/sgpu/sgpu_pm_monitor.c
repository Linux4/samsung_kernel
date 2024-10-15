// SPDX-License-Identifier: GPL-2.0-only
/*
 * @file sgpu_pm_monitor.c
 * Copyright (C) 2024 Samsung Electronics
 */

#include "amdgpu.h"
#include "sgpu_pm_monitor.h"
#include "amdgpu_trace.h"

void sgpu_pm_monitor_init(struct amdgpu_device *adev)
{
	struct sgpu_pm_monitor *monitor = &adev->pm_monitor;

	spin_lock_init(&monitor->lock);

	monitor->pm_stats.power_status = SGPU_POWER_STATE_ON;
	monitor->pm_stats.last_update = ktime_get_ns();

	memset(&monitor->base_stats, 0, sizeof(struct sgpu_pm_stats));
	monitor->monitor_enable = false;
}

static void sgpu_pm_monitor_calculate(struct amdgpu_device *adev)
{
	struct sgpu_pm_stats *stats = &adev->pm_monitor.pm_stats;
	uint64_t cur_time, diff_time;

	cur_time = ktime_get_ns();
	diff_time = cur_time - stats->last_update;
	stats->state_time[stats->power_status] += diff_time;
	stats->last_update = cur_time;
}

void sgpu_pm_monitor_update(struct amdgpu_device *adev, u32 new_status)
{
	struct sgpu_pm_monitor *monitor = &adev->pm_monitor;
	unsigned long flags;

	BUG_ON(new_status >= SGPU_POWER_STATE_NR);

	spin_lock_irqsave(&monitor->lock, flags);

	sgpu_pm_monitor_calculate(adev);
	monitor->pm_stats.power_status = new_status;
	trace_sgpu_pm_monitor_update(new_status);

	spin_unlock_irqrestore(&monitor->lock, flags);
}

void sgpu_pm_monitor_get_stats(struct amdgpu_device *adev,
				struct sgpu_pm_stats *pm_stats)
{
	struct sgpu_pm_monitor *monitor = &adev->pm_monitor;
	unsigned long flags;

	spin_lock_irqsave(&monitor->lock, flags);

	sgpu_pm_monitor_calculate(adev);
	memcpy(pm_stats, &monitor->pm_stats, sizeof(struct sgpu_pm_stats));

	spin_unlock_irqrestore(&monitor->lock, flags);
}

uint64_t sgpu_pm_monitor_get_idle_time(struct amdgpu_device *adev)
{
	struct sgpu_pm_stats pm_stats;

	sgpu_pm_monitor_get_stats(adev, &pm_stats);

	return pm_stats.state_time[SGPU_POWER_STATE_IFPO]
		+ pm_stats.state_time[SGPU_POWER_STATE_SUSPEND];
}
