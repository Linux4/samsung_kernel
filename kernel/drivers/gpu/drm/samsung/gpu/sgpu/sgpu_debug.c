// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2023 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/pm_runtime.h>

#include "amdgpu.h"
#include "sgpu_debug.h"

#include <soc/samsung/exynos/debug-snapshot.h>

#if IS_ENABLED(CONFIG_EXYNOS_S2MPU)
#ifdef CONFIG_DRM_AMDGPU_DUMP
static void sgpu_show_asic_status(struct amdgpu_device *adev)
{
	if (adev->asic_funcs->get_asic_status)
		adev->asic_funcs->get_asic_status(adev, NULL, 0);
}
#else
#define sgpu_show_asic_status(adev) do { } while (0)
#endif

static const char * const s2mpu_fault_type[] = {"table walk", "permission", "unknown", "context"};
static const char *s2mpu_subsys[NR_SGPU_S2MPU_SUBSYS] = {"SYSTEM", "TZMP2_GPU", "G3D_TMR"};

static struct sgpu_debug *to_sgpu_debug(struct s2mpu_notifier_block *nb, const char *subsys)
{
	int i;

	for (i = 0; i < NR_SGPU_S2MPU_SUBSYS; i++)
		if (!strcmp(subsys, s2mpu_subsys[i]))
			return container_of(nb, struct sgpu_debug, s2mpu_nb[i]);

	return NULL;
}

static int sgpu_s2mpu_notifier_call(struct s2mpu_notifier_block *nb,
				    struct s2mpu_notifier_info *info)
{
	const char *faulttype = "unknown";
	struct sgpu_debug *debug;
	struct amdgpu_device *adev;

	debug = to_sgpu_debug(nb, info->subsystem);
	if (!debug)
		return S2MPU_NOTIFY_OK;

	adev = container_of(debug, struct amdgpu_device, sgpu_debug);

	if ((info->fault_type > 0) && (info->fault_type <= FAULT_TYPE_CONTEXT_FAULT))
		faulttype = s2mpu_fault_type[info->fault_type - 1];

	pr_crit("S2MPU FAULT on %s %#lx (size %#x bytes, type %d(%s))\n",
		info->fault_rw ? "WRITE to" : "READ from", info->fault_addr,
		info->fault_len, info->fault_type, faulttype);

	/* GL2ACEM_STUS_ADDR */
	pr_crit("GPU access address : 0x%x\n", readl_relaxed(adev->rmmio + 0x33a24));

	sgpu_show_asic_status(container_of(debug, struct amdgpu_device, sgpu_debug));

	return S2MPU_NOTIFY_OK;
}

static void sgpu_register_s2mpu_notifier(struct amdgpu_device *adev)
{
	int i;

	for (i = 0; i < NR_SGPU_S2MPU_SUBSYS; i++) {
		adev->sgpu_debug.s2mpu_nb[i].subsystem = s2mpu_subsys[i];
		adev->sgpu_debug.s2mpu_nb[i].notifier_call = sgpu_s2mpu_notifier_call;
		if (!exynos_s2mpu_notifier_call_register(&adev->sgpu_debug.s2mpu_nb[i]))
			dev_info(adev->dev, "Registered S2MPU %s Notifier", s2mpu_subsys[i]);
	}
}
#else
#define sgpu_register_s2mpu_notifier(adev) do { } while (0)
#endif

void sgpu_debug_snapshot_expire_watchdog(void)
{
	dbg_snapshot_expire_watchdog();
}

/* This sfr is to find the address that gpu approaches in s2mpu situation.
 * The GPU's access to the address area below is not recorded. */
void sgpu_debug_set_gl2acem_address(struct amdgpu_device *adev)
{
	if (adev->runpm)
		pm_runtime_get_sync(adev->ddev.dev);

	/* GL2ACEM_ADDR_UP */
	writel_relaxed(0xffffff, adev->rmmio + 0x33A1C);

	/* GL2ACEM_ADDR_LOW */
	writel_relaxed(0x220000, adev->rmmio + 0x33A20);

	if (adev->runpm)
		pm_runtime_put_sync(adev->ddev.dev);
}

void sgpu_debug_init(struct amdgpu_device *adev)
{
	sgpu_register_s2mpu_notifier(adev);
	sgpu_debug_set_gl2acem_address(adev);
}
