/* SPDX-License-Identifier: GPL-2.0 */

/*
 * (C) COPYRIGHT 2022 Samsung Electronics Inc. All rights reserved.
 *
 * This program is free software and is provided to you under the terms of the
 * GNU General Public License version 2 as published by the Free Software
 * Foundation, and any use by you of this program is subject to the terms
 * of such GNU licence.
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

/* Implements */
#ifdef CONFIG_MALI_EXYNOS_SECURE_HVC
#include <gpexbe_hvc.h>
#else
#include <gpexbe_smc.h>
#endif

int gpexbe_smc_hvc_protection_enable(void)
{
#if IS_ENABLED(CONFIG_MALI_EXYNOS_SECURE_HVC)
	return gpexbe_hvc_protection_enable();
#else
	return gpexbe_smc_protection_enable();
#endif
}

int gpexbe_smc_hvc_protection_disable(void)
{
#if IS_ENABLED(CONFIG_MALI_EXYNOS_SECURE_HVC)
	return gpexbe_hvc_protection_disable();
#else
	return gpexbe_smc_protection_disable();
#endif
}

void gpexbe_smc_hvc_notify_power_on(void)
{
#if IS_ENABLED(CONFIG_MALI_EXYNOS_SECURE_HVC)
	return;
#else
	gpexbe_smc_notify_power_on();
#endif
}
void gpexbe_smc_hvc_notify_power_off(void)
{
#if IS_ENABLED(CONFIG_MALI_EXYNOS_SECURE_HVC)
	return;
#else
	gpexbe_smc_notify_power_off();
#endif
}

int gpexbe_smc_hvc_init(void)
{
#if IS_ENABLED(CONFIG_MALI_EXYNOS_SECURE_HVC)
	return gpexbe_hvc_init();
#else
	return gpexbe_smc_init();
#endif
}

void gpexbe_smc_hvc_term(void)
{
#if IS_ENABLED(CONFIG_MALI_EXYNOS_SECURE_HVC)
	return gpexbe_hvc_term();
#else
	return gpexbe_smc_term();
#endif
}
