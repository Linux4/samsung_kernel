/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */
/*
 * Copyright (c) 2023 Samsung Electronics Co., Ltd.
 *            http://www.samsung.com
 *
 * This program is free software and is provided to you under the terms of the
 * GNU General Public License version 2 as published by the Free Software
 * Foundation, and any use by you of this program is subject to the terms
 * of such GNU license.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, you can access it online at
 * http://www.gnu.org/licenses/gpl-2.0.html.
 */
#ifndef __SGPU_DEBUG_H__
#define __SGPU_DEBUG_H__

#if IS_ENABLED(CONFIG_EXYNOS_S2MPU)
#include <soc/samsung/exynos/exynos-s2mpu.h>
#define NR_SGPU_S2MPU_SUBSYS 3
#endif

struct sgpu_debug {
#if IS_ENABLED(CONFIG_EXYNOS_S2MPU)
	struct s2mpu_notifier_block s2mpu_nb[NR_SGPU_S2MPU_SUBSYS];
#endif
};

#ifdef CONFIG_DRM_SGPU_EXYNOS
void sgpu_debug_init(struct amdgpu_device *adev);
void sgpu_debug_snapshot_expire_watchdog(void);
#else
#define sgpu_debug_init(adev) do { } while (0)
#define sgpu_debug_snapshot_expire_watchdog() do { } while (0)
#endif

#endif /* __SGPU_DEBUG_H__ */
