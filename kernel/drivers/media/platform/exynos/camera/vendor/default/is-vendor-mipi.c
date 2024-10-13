/*
 * Samsung Exynos5 SoC series FIMC-IS driver
 *
 * exynos5 mipi vendor functions
 *
 * Copyright (c) 2023 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <exynos-is-module.h>
#include "is-vendor-mipi.h"
#include "is-vendor.h"

#include <linux/bsearch.h>

#include "is-device-sensor-peri.h"
#include "is-cis.h"

int is_vendor_update_mipi_info(struct is_device_sensor *device)
{
	return 0;
}
PST_EXPORT_SYMBOL(is_vendor_update_mipi_info);

int is_vendor_set_mipi_clock(struct is_device_sensor *device)
{
	return 0;
}
EXPORT_SYMBOL_GPL(is_vendor_set_mipi_clock);

int is_vendor_set_mipi_mode(struct is_cis *cis)
{
	return 0;
}
EXPORT_SYMBOL_GPL(is_vendor_set_mipi_mode);
