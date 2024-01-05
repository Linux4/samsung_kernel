/*
 * cyttsp5_devtree.h
 * Cypress TrueTouch(TM) Standard Product V4 Device Tree Support Driver
 * For use with Cypress Txx4xx parts.
 * Supported parts include:
 * TMA4XX
 * TMA1036
 *
 * Copyright (C) 2013 Cypress Semiconductor
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2, and only version 2, as published by the
 * Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * Contact Cypress Semiconductor at www.cypress.com <ttdrivers@cypress.com>
 *
 */

#include <linux/cyttsp5/cyttsp5_core.h>

extern struct cyttsp5_loader_platform_data _cyttsp5_loader_platform_data;

extern int cyttsp5_xres(struct cyttsp5_core_platform_data *pdata,
		struct device *dev);
extern int cyttsp5_init(struct cyttsp5_core_platform_data *pdata,
		int on, struct device *dev);
extern int cyttsp5_power(struct cyttsp5_core_platform_data *pdata,
		int on, struct device *dev, atomic_t *ignore_irq);
extern int cyttsp5_irq_stat(struct cyttsp5_core_platform_data *pdata,
		struct device *dev);
