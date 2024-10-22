// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/module.h>
#include <linux/of_device.h>

#include "apummu_cmn.h"
#include "apummu_plat.h"
#include "apummu_device.h"

static struct apummu_plat mt6897_drv = {
	.slb_wait_time			= 0,
	.is_general_SLB_support	= true,
};

static struct apummu_plat mt6989_drv = {
	.slb_wait_time			= 0,
	.is_general_SLB_support	= true,
};

static struct apummu_plat mt6878_drv = {
	.slb_wait_time			= 0,
	.is_general_SLB_support	= false,
};

static const struct of_device_id apummu_of_match[] = {
	{ .compatible = "mediatek,rv-apummu",        .data = &mt6897_drv },
	{ .compatible = "mediatek,rv-apummu-mt6989", .data = &mt6989_drv },
	{ .compatible = "mediatek,rv-apummu-mt6878", .data = &mt6878_drv },
	{ /* end of list */ },
};

MODULE_DEVICE_TABLE(of, apummu_of_match);

const struct of_device_id *apummu_get_of_device_id(void)
{
	return apummu_of_match;
}
