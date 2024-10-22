/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */


#ifndef __APUSYS_APUMMU_PLAT_H__
#define __APUSYS_APUMMU_PLAT_H__

/* apummu paltform data */
struct apummu_plat {
	unsigned int slb_wait_time;
	bool is_general_SLB_support;
};

int apummu_plat_init(struct platform_device *pdev);

#endif
