/*
 * Secure RPMB Driver for Exynos scsi rpmb
 *
 * Copyright (C) 2016 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <ufs/ufshcd.h>
#include "ufs-cal-if.h"
#include "ufs-exynos.h"

#define RV_SUCCESS 0

struct ufs_hba *hba_srpmb;

int exynos_ufs_srpmb_config(struct ufs_hba *hba)
{
	if (hba) {
		hba_srpmb = hba;
		return RV_SUCCESS;
	}
	return -1;
}

struct ufs_hba *exynos_ufs_get_ufs_hba(void)
{
	if (!hba_srpmb)
		return NULL;

	return hba_srpmb;
}
EXPORT_SYMBOL(exynos_ufs_get_ufs_hba);

struct scsi_device *exynos_ufs_get_wlun_sdev(void)
{
	if (!hba_srpmb)
		return NULL;

	if (!hba_srpmb->ufs_device_wlun)
		return NULL;

	return hba_srpmb->ufs_device_wlun;
}
EXPORT_SYMBOL(exynos_ufs_get_wlun_sdev);
