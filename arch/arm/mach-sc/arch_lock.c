/*
 * Copyright (C) 2012 Spreadtrum Communications Inc.
 * Author: steve.zhan <steve.zhan@spreadtrum.com>
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/spinlock.h>
#include <linux/hwspinlock.h>
#include <linux/io.h>

#include <mach/sci.h>
#include <mach/arch_lock.h>
#include <linux/of_irq.h>
#include <linux/of.h>
#include <linux/of_device.h>

#include "devices.h"

/**
* Each physical lock must have a system-wide id number
* that is agreed upon, otherwise remote processors can't possibly assume
* they're using the same hardware lock.
*/
struct hwspinlock *hwlocks[HWSPINLOCK_ID_TOTAL_NUMS];
unsigned char __initdata hwlocks_implemented[HWSPINLOCK_ID_TOTAL_NUMS];
unsigned int hwspinlock_vid;
unsigned char local_hwlocks_status[HWSPINLOCK_ID_TOTAL_NUMS];/*array[0] standfor lock0*/

static int __init early_init_hwlocks(void)
{
	int i;
	struct hwspinlock **plock;
	arch_hwlocks_implemented();

	for (i = 0; i < HWSPINLOCK_ID_TOTAL_NUMS; ++i) {
		if (hwlocks_implemented[i]) {
			plock = &hwlocks[i];
			*plock = hwspin_lock_request_specific(i);
			if (WARN_ON(IS_ERR_OR_NULL(*plock)))
				*plock = NULL;
			else
				pr_info("early alloc hwspinlock id %d\n",
					hwspin_lock_get_id(*plock));
		}
	}
	return 0;
}

postcore_initcall_sync(early_init_hwlocks);

static int __init hwspinlocks_init(void)
{
	int ret;

#ifdef CONFIG_OF
	struct device_node *np;
	struct platform_device *pdev;

	np = of_find_node_by_name(NULL, "hwspinlock1");
	if (!np) {
		pr_warn("Can't get the hwspinlock1 node!\n");
		return -ENODEV;
	}

	pdev = of_platform_device_create(np, 0, NULL);
	if (!pdev) {
		pr_warn("register hwspinlock1 failed!\n");
		return -ENODEV;
	}
	pr_info("*****hwspinlock1's name is %s\n", pdev->name);
	pr_info("SPRD register hwspinlock1 ok!\n");
//	of_detach_node(np);

	return 0;

#else
	ret = platform_device_register(&sprd_hwspinlock_device1);
	if (WARN(ret != 0, "register hwspinlock device error!!"))
		platform_device_unregister(&sprd_hwspinlock_device1);

	return ret;

#endif
}

/* early board code might need to reserve specific hwspinlock instances */
postcore_initcall(hwspinlocks_init);

