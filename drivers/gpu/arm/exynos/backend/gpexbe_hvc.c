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

/* Implements */
#include <gpexbe_hvc.h>

/* Uses */
#include <linux/version.h>
#include <linux/types.h>
#include <linux/spinlock.h>

#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 4, 0)
#include <linux/smc.h>
#else
#include <soc/samsung/exynos/exynos-hvc.h>
#endif

#include <gpex_utils.h>

struct _hvc_info {
	bool protection_enabled;
	spinlock_t lock;
};

static struct _hvc_info hvc_info;

int gpexbe_hvc_protection_enable()
{
	int err;
	unsigned long flags;

	spin_lock_irqsave(&hvc_info.lock, flags);
	if (hvc_info.protection_enabled) {
		spin_unlock_irqrestore(&hvc_info.lock, flags);
		return 0;
	}

	err = exynos_hvc(HVC_PROTECTION_SET, 0, PROT_G3D, HVC_PROTECTION_ENABLE,0);

	if (!err)
		hvc_info.protection_enabled = true;

	spin_unlock_irqrestore(&hvc_info.lock, flags);

	if (!err)
		GPU_LOG(MALI_EXYNOS_INFO, "%s: Enter Secure World by GPU\n", __func__);
	else
		GPU_LOG_DETAILED(MALI_EXYNOS_ERROR, LSI_GPU_SECURE, 0u, 0u,
				 "%s: failed to enter secure world ret : %d\n", __func__, err);

	return err;
}

int gpexbe_hvc_protection_disable()
{
	int err;
	unsigned long flags;

	spin_lock_irqsave(&hvc_info.lock, flags);
	if (!hvc_info.protection_enabled) {
		spin_unlock_irqrestore(&hvc_info.lock, flags);
		return 0;
	}

	err = exynos_hvc(HVC_PROTECTION_SET, 0, PROT_G3D, HVC_PROTECTION_DISABLE,0);

	if (!err)
		hvc_info.protection_enabled = false;

	spin_unlock_irqrestore(&hvc_info.lock, flags);

	if (!err)
		GPU_LOG(MALI_EXYNOS_INFO, "%s: Exit Secure World by GPU\n", __func__);
	else
		GPU_LOG_DETAILED(MALI_EXYNOS_ERROR, LSI_GPU_SECURE, 0u, 0u,
				 "%s: failed to exit secure world ret : %d\n", __func__, err);

	return err;
}

/*
	hvc power on notifcation is not needed for new secure hardware
*/
void gpexbe_hvc_notify_power_on()
{
}

/*
	 hvc power off notifcation is not needed for new secure hardware
*/
void gpexbe_hvc_notify_power_off()
{
}

int gpexbe_hvc_init(void)
{
	spin_lock_init(&hvc_info.lock);
	hvc_info.protection_enabled = false;

	gpex_utils_get_exynos_context()->hvc_info = &hvc_info;

	return 0;
}

void gpexbe_hvc_term(void)
{
	gpexbe_hvc_protection_disable();
	hvc_info.protection_enabled = false;
}
