/*
 * Samsung Exynos5 SoC series FIMC-IS driver
 *
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef IS_RESOURCE_MGR_H
#define IS_RESOURCE_MGR_H

#include <linux/reboot.h>
#include "is-hw-config.h"
#include "pablo-resourcemgr.h"

enum is_resourcemgr_state {
	IS_RM_COM_POWER_ON,
	IS_RM_SS0_POWER_ON,
	IS_RM_SS1_POWER_ON,
	IS_RM_SS2_POWER_ON,
	IS_RM_SS3_POWER_ON,
	IS_RM_SS4_POWER_ON,
	IS_RM_SS5_POWER_ON,
	IS_RM_ISC_POWER_ON,
	IS_RM_POWER_ON
};

/* Reserved memory data */
struct is_resource_rmem {
	void *virt_addr;
	unsigned long phys_addr;
	size_t size;
};

int is_resourcemgr_probe(struct is_resourcemgr *resourcemgr, void *private_data, struct platform_device *pdev);
int is_resource_open(struct is_resourcemgr *resourcemgr, u32 rsc_type, void **device);
int is_resource_get(struct is_resourcemgr *resourcemgr, u32 rsc_type);
int is_resource_put(struct is_resourcemgr *resourcemgr, u32 rsc_type);
int is_resource_dump(void);
#ifdef ENABLE_HWACG_CONTROL
extern void is_hw_csi_qchannel_enable_all(bool enable);
#endif
int is_resource_cdump(void);
#define GET_RESOURCE(resourcemgr, type) \
	((type < RESOURCE_TYPE_MAX) ? &resourcemgr->resource_device[type] : NULL)
#endif
