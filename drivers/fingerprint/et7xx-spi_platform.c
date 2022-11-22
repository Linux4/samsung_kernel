/*
 * Copyright (C) 2016 Samsung Electronics. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 */

#include "fingerprint.h"
#include "et7xx.h"

int fps_resume_set(void) {
	return 0;
}

int fps_suspend_set(struct et7xx_data *etspi) {
	return 0;
}

int et7xx_register_platform_variable(struct et7xx_data *etspi)
{
	return 0;
}

int et7xx_unregister_platform_variable(struct et7xx_data *etspi)
{
	return 0;
}

int et7xx_spi_clk_enable(struct et7xx_data *etspi)
{
	int retval = 0;

#ifdef ENABLE_SENSORS_FPRINT_SECURE
	if (!etspi->enabled_clk) {
		__pm_stay_awake(etspi->fp_spi_lock);
		etspi->enabled_clk = true;
	}
#endif

	return retval;
}

int et7xx_spi_clk_disable(struct et7xx_data *etspi)
{
	int retval = 0;

#ifdef ENABLE_SENSORS_FPRINT_SECURE
	if (etspi->enabled_clk) {
		__pm_relax(etspi->fp_spi_lock);
		etspi->enabled_clk = false;
	}
#endif

	return retval;
}
