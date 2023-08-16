/*
 * cyttsp5_device_access.h
 * Cypress TrueTouch(TM) Standard Product V5 Device Access module.
 * Configuration and Test command/status user interface.
 * For use with Cypress Txx5xx parts.
 * Supported parts include:
 * TMA5XX
 *
 * Copyright (C) 2012-2013 Cypress Semiconductor
 * Copyright (C) 2011 Sony Ericsson Mobile Communications AB.
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

#ifndef _LINUX_CYTTSP5_DEVICE_ACCESS_H
#define _LINUX_CYTTSP5_DEVICE_ACCESS_H

#define CYTTSP5_DEVICE_ACCESS_NAME "cyttsp5_device_access"

#define CYTTSP5_INPUT_ELEM_SZ (sizeof("0xHH") + 1)

#ifdef TTHE_TUNER_SUPPORT

#define CY_CMD_RET_PANEL_IN_DATA_OFFSET	0
#define CY_CMD_RET_PANEL_ELMNT_SZ_MASK	0x07
#define CY_CMD_RET_PANEL_HDR		0x0A
#define CY_CMD_RET_PANEL_ELMNT_SZ_MAX	0x2

enum scan_data_type_list {
	CY_MUT_RAW,
	CY_MUT_BASE,
	CY_MUT_DIFF,
	CY_SELF_RAW,
	CY_SELF_BASE,
	CY_SELF_DIFF,
	CY_BAL_RAW,
	CY_BAL_BASE,
	CY_BAL_DIFF,
};

struct heatmap_param {
	bool scan_start;
	enum scan_data_type_list data_type; /* raw, base, diff */
	int num_element;
};
#endif

struct cyttsp5_device_access_platform_data {
	char const *device_access_dev_name;
};

#include "cyttsp5_regs.h"
int cyttsp5_device_access_probe(struct device *dev, struct cyttsp5_sysinfo *sysinfo);
int cyttsp5_device_access_release(struct device *dev);


#endif /* _LINUX_CYTTSP5_DEVICE_ACCESS_H */
