/* SPDX-License-Identifier: GPL-2.0 */

/*
 * (C) COPYRIGHT 2021 Samsung Electronics Inc. All rights reserved.
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

#include <linux/version.h>
#include <linux/protected_mode_switcher.h>

#include <gpex_utils.h>
#include <gpex_secure.h>
#include <gpexbe_secure.h>
#include <gpexbe_smc.h>

struct _secure_info {
	bool exynos_smc_enabled;
	spinlock_t exynos_smc_lock;
};

static struct _secure_info secure_info;

static int exynos_secure_mode_enable(void)
{
	/* enable secure mode : TZPC */
	unsigned long flags;
	int ret = 0;

	spin_lock_irqsave(&secure_info.exynos_smc_lock, flags);
	if (secure_info.exynos_smc_enabled) {
		spin_unlock_irqrestore(&secure_info.exynos_smc_lock, flags);
		goto secure_out;
	}

	ret = gpexbe_secure_protection_enable();

	if (0 == ret) {
		secure_info.exynos_smc_enabled = true;
	}

	spin_unlock_irqrestore(&secure_info.exynos_smc_lock, flags);

	GPU_LOG_DETAILED(MALI_EXYNOS_INFO, LSI_SECURE_WORLD_ENTER, 0u, 0u,
			 "LSI_SECURE_WORLD_ENTER\n");

	if (0 == ret) {
		GPU_LOG(MALI_EXYNOS_INFO, "%s: Enter Secure World by GPU\n", __func__);
	} else {
		GPU_LOG_DETAILED(MALI_EXYNOS_ERROR, LSI_GPU_SECURE, 0u, 0u,
				 "%s: failed to enter secure world ret : %d\n", __func__, ret);
	}

secure_out:
	return ret;
}

static int exynos_secure_mode_disable(void)
{
	/* Turn off secure mode and reset GPU : TZPC */
	unsigned long flags;
	int ret = 0;

	spin_lock_irqsave(&secure_info.exynos_smc_lock, flags);
	if (!secure_info.exynos_smc_enabled) {
		spin_unlock_irqrestore(&secure_info.exynos_smc_lock, flags);
		goto secure_out;
	}

	ret = gpexbe_secure_protection_disable();

	if (0 == ret) {
		secure_info.exynos_smc_enabled = false;
	}
	spin_unlock_irqrestore(&secure_info.exynos_smc_lock, flags);

	GPU_LOG_DETAILED(MALI_EXYNOS_INFO, LSI_SECURE_WORLD_EXIT, 0u, 0u,
			 "LSI_SECURE_WORLD_EXIT\n");

	if (0 == ret) {
		GPU_LOG(MALI_EXYNOS_INFO, "%s: Exit Secure World by GPU\n", __func__);
	} else {
		GPU_LOG_DETAILED(MALI_EXYNOS_ERROR, LSI_GPU_SECURE, 0u, 0u,
				 "%s: failed to exit secure world ret : %d\n", __func__, ret);
	}

secure_out:
	return ret;
}

/*
 * Call back functions for PROTECTED_CALLBACKS
 */
static int exynos_secure_mode_enable_arm(struct protected_mode_device *pdev)
{
	int ret = 0;

	if (!pdev)
		return -EINVAL;

	ret = pdev->ops.protected_mode_enable(pdev);
	if (ret != 0)
		return ret;

	return exynos_secure_mode_enable();
}

static int exynos_secure_mode_disable_arm(struct protected_mode_device *pdev)
{
	int ret = 0;

	if (!pdev)
		return -EINVAL;

	ret = pdev->ops.protected_mode_disable(pdev);
	if (ret != 0)
		return ret;

	return exynos_secure_mode_disable();
}

struct protected_mode_ops exynos_protected_ops_arm = { .protected_mode_enable =
							       exynos_secure_mode_enable_arm,
						       .protected_mode_disable =
							       exynos_secure_mode_disable_arm };

int gpex_secure_init(void)
{
	spin_lock_init(&secure_info.exynos_smc_lock);
	secure_info.exynos_smc_enabled = false;

	return 0;
}

void gpex_secure_term(void)
{
	exynos_secure_mode_disable();
	secure_info.exynos_smc_enabled = false;
}
