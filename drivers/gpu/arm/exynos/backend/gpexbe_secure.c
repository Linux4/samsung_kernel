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

#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 4, 0)
#include <linux/smc.h>
#else
#include <soc/samsung/exynos-smc.h>
#endif

#include <gpexbe_secure.h>

/* SMC CALL return value for Successfully works */
#define GPU_SMC_TZPC_OK 0

int gpexbe_secure_protection_enable()
{
	int ret;
	ret = exynos_smc(SMC_PROTECTION_SET, 0, PROT_G3D, SMC_PROTECTION_ENABLE);

	if (ret == GPU_SMC_TZPC_OK)
		return 0;
	else
		return -1;
}

int gpexbe_secure_protection_disable()
{
	int ret;
	ret = exynos_smc(SMC_PROTECTION_SET, 0, PROT_G3D, SMC_PROTECTION_DISABLE);

	if (ret == GPU_SMC_TZPC_OK)
		return 0;
	else
		return -1;
}
