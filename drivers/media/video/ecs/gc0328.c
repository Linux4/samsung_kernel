/*
 * gc0328 sensor driver.
 *
 * Copyright (C) 2013 Marvell Internation Ltd.
 * Copyright (C) 2013 GalaxyCore Ltd.
 *
 * Owen Zhang <xinzha@marvell.com>
 *
 * Based on drivers/media/video/ecs/ov5640.c
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 */

#include <linux/init.h>
#include <linux/module.h>
#include "gc0328.h"

static int __init gc0328_mod_init(void)
{
	return xsd_add_driver(gc0328_drv_table);
}

static void __exit gc0328_mod_exit(void)
{
	xsd_del_driver(gc0328_drv_table);
}

module_init(gc0328_mod_init);
module_exit(gc0328_mod_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Owen Zhang <xinzha@marvell.com>");
MODULE_DESCRIPTION("GC0328 Camera Driver");
