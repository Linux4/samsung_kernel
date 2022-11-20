/*
 *  Copyright (C) 2010,Imagis Technology Co. Ltd. All Rights Reserved.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 */

#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/slab.h>

#include <linux/platform_device.h>
#include <linux/device.h>
#include <linux/mutex.h>
#include <linux/input/mt.h>

#include "ist30xx.h"
#include "ist30xxb/ist30xxb.h"
#include "ist30xxc/ist30xx.h"

static int ic_type = IMAGIS_NONE;
#define USE_TSP_IST3032B        0

#ifndef CONFIG_HAS_EARLYSUSPEND
static int ist30xx_suspend(struct device *dev)
{
    if (ic_type == IMAGIS_IST30XXB)
        ist30xxb_suspend(dev);
    else if (ic_type == IMAGIS_IST30XXC)
        ist30xxc_suspend(dev);

	return 0;
}

static int ist30xx_resume(struct device *dev)
{
	if (ic_type == IMAGIS_IST30XXB)
        ist30xxb_resume(dev);
    else if (ic_type == IMAGIS_IST30XXC)
        ist30xxc_resume(dev);

	return 0;
}
#endif

void ist30xx_set_ta_mode(int status)
{
     if (ic_type == IMAGIS_IST30XXB) {
        ist30xxb_set_ta_mode(status);
    } else if (ic_type == IMAGIS_IST30XXC) {
        ist30xxc_set_ta_mode(status);
    } else {
        ist30xxb_set_ta_mode(status);
        ist30xxc_set_ta_mode(status);
    }
}
EXPORT_SYMBOL(ist30xx_set_ta_mode);

void ist30xx_set_call_mode(int mode)
{
	 if (ic_type == IMAGIS_IST30XXB) {
        ist30xxb_set_call_mode(mode);
    } else if (ic_type == IMAGIS_IST30XXC) {
        ist30xxc_set_call_mode(mode);
    } else {
        ist30xxb_set_call_mode(mode);
        ist30xxc_set_call_mode(mode);
    }
}
EXPORT_SYMBOL(ist30xx_set_call_mode);

void ist30xx_set_cover_mode(int mode)
{
    if (ic_type == IMAGIS_IST30XXC)
        ist30xxc_set_cover_mode(mode);
}
EXPORT_SYMBOL(ist30xx_set_cover_mode);

static int ist30xx_probe(struct i2c_client *client, 
			const struct i2c_device_id *id)
{
#if USE_TSP_IST3032B
    if (ist30xxb_probe(client, id) == 0) {
        ic_type = IMAGIS_IST30XXB;
        printk("[ TSP ] TSP is IST3032B\n");
    } else if (ist30xxc_probe(client, id) == 0) {
        ic_type = IMAGIS_IST30XXC;
        printk("[ TSP ] TSP is IST3026C\n");
    } else
        printk("[ TSP ] TSP is unknown IC\n");
#else
    if (ist30xxc_probe(client, id) == 0) {
        ic_type = IMAGIS_IST30XXC;
        printk("[ TSP ] TSP is IST3026C\n");
    } else
        printk("[ TSP ] TSP is unknown IC\n");
#endif

	return 0;
}

static int ist30xx_remove(struct i2c_client *client)
{
    if (ic_type == IMAGIS_IST30XXB)
        ist30xxb_remove(client);
    else if (ic_type == IMAGIS_IST30XXC)
        ist30xxc_remove(client);

	return 0;
}

static void ist30xx_shutdown(struct i2c_client *client)
{
    if (ic_type == IMAGIS_IST30XXB)
        ist30xxb_shutdown(client);
    else if (ic_type == IMAGIS_IST30XXC)
        ist30xxc_shutdown(client);
}

static struct i2c_device_id ist30xx_idtable[] = {
	{ IST30XX_DEV_NAME, 0 },
	{},
};
MODULE_DEVICE_TABLE(i2c, ist30xx_idtable);

#ifndef CONFIG_HAS_EARLYSUSPEND
static const struct dev_pm_ops ist30xx_pm_ops = {
	.suspend	= ist30xx_suspend,
	.resume		= ist30xx_resume,
};
#endif

static struct i2c_driver ist30xx_i2c_driver = {
	.id_table			= ist30xx_idtable,
	.probe				= ist30xx_probe,
	.remove				= ist30xx_remove,
    .shutdown           = ist30xx_shutdown,
	.driver				= {
		.owner			= THIS_MODULE,
		.name			= IST30XX_DEV_NAME,
#ifndef CONFIG_HAS_EARLYSUSPEND
		.pm				= &ist30xx_pm_ops,
#endif
	},
};

static int __init ist30xx_init(void)
{
	return i2c_add_driver(&ist30xx_i2c_driver);
}

static void __exit ist30xx_exit(void)
{
	i2c_del_driver(&ist30xx_i2c_driver);
}

module_init(ist30xx_init);
module_exit(ist30xx_exit);

MODULE_DESCRIPTION("Imagis IST30XX touch driver");
MODULE_LICENSE("GPL");
