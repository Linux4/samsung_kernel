/*
 * sec_pm_qos_monitor.h - SEC PM QoS Monitor
 *
 *  Copyright (c) 2022 Samsung Electronics Co., Ltd.
 *      http://www.samsung.com
 *  Minsung Kim <ms925.kim@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __LINUX_SEC_PM_QOS_MONITOR_H
#define __LINUX_SEC_PM_QOS_MONITOR_H __FILE__

#include <soc/samsung/exynos_pm_qos.h>

struct sec_pm_qos_object {
	struct exynos_pm_qos_constraints *constraints;
	char *name;
};

#if IS_ENABLED(CONFIG_SEC_PM_QOS_MONITOR)
extern void sec_pm_qos_monitor_init_data(struct sec_pm_qos_object *objs);
#else
static inline void sec_pm_qos_monitor_init_data(struct sec_pm_qos_object *objs) {}
#endif

#endif /* __LINUX_SEC_PM_DEBUG_H */
