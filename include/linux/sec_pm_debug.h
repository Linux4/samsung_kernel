/*
 * sec_pm_debug.h - SEC Power Management Debug Support
 *
 *  Copyright (c) 2018 Samsung Electronics Co., Ltd.
 *      http://www.samsung.com
 *  Minsung Kim <ms925.kim@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#ifndef __LINUX_SEC_PM_DEBUG_H
#define __LINUX_SEC_PM_DEBUG_H __FILE__

#if IS_ENABLED(CONFIG_REGULATOR_S2MPU12)
extern int main_pmic_init_debug_sysfs(struct device *sec_pm_dev);
#else
static inline int main_pmic_init_debug_sysfs(struct device *sec_pm_dev) { return 0; }
#endif /* CONFIG_REGULATOR_S2MPU12 */

#endif /* __LINUX_SEC_PM_DEBUG_H */
