/*
 * Copyright (c) 2016 Samsung Electronics Co., Ltd.
 *      http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/of.h>

#if IS_ENABLED(CONFIG_DRV_SAMSUNG)
extern int sec_class_create(void);
#endif

#if IS_ENABLED(CONFIG_SEC_CHIPID)
extern int chipid_sysfs_init(void);
#endif

static int __init sec_extention_init(void)
{
	pr_info("%s\n", __func__);

#if IS_ENABLED(CONFIG_DRV_SAMSUNG)
	sec_class_create();
#endif

#if IS_ENABLED(CONFIG_SEC_CHIPID)
	chipid_sysfs_init();
#endif

	return 0;
}
module_init(sec_extention_init);


MODULE_DESCRIPTION("Samsung Extention driver");
MODULE_LICENSE("GPL v2");
