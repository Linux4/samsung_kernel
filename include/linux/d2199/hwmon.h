/*
 * hwmon.h  --  HWMON driver for Dialog Semiconductor D2199 PMIC
 *
 * Copyright 2013 Dialog Semiconductor Ltd
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 */

#ifndef __LINUX_BOBCAT_HWMON_H
#define __LINUX_BOBCAT_HWMON_H

#include <linux/platform_device.h>

struct d2199_hwmon {
	struct d2199 *d2199;
	struct device *classdev;
	struct platform_device  *pdev;
};

#endif /* __LINUX_BOBCAT_HWMON_H */
