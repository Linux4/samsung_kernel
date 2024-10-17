/*
 * Copyright (c) 2020 Samsung Electronics Co., Ltd.
 * Author: Minsung Kim <ms925.kim@samsung.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef _LINUX_SEC_PM_SMPL_WARN_H
#define _LINUX_SEC_PM_SMPL_WARN_H

#if IS_ENABLED(CONFIG_SEC_PM_SMPL_WARN)
int sec_pm_smpl_warn_throttle_by_one_step(void);
void sec_pm_smpl_warn_unthrottle(void);
#else
static inline int sec_pm_smpl_warn_throttle_by_one_step(void) { return 0; }
static inline void sec_pm_smpl_warn_unthrottle(void) {}
#endif /* CONFIG_SEC_PM_SMPL_WARN */
#endif /* _LINUX_SEC_PM_SMPL_WARN_H */
