
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/version.h>
#include <linux/gpio.h>
#include <linux/clk.h>
#include <linux/regulator/consumer.h>
#include <linux/videodev2.h>
#include <linux/videodev2_exynos_camera.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/platform_device.h>
#include <mach/regs-gpio.h>
#include <mach/regs-clock.h>
#include <plat/clock.h>
#include <plat/gpio-cfg.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-device.h>
#include <media/v4l2-subdev.h>
#include <mach/exynos-fimc-is-sensor.h>
#include "fimc-is-core.h"
#include "fimc-is-device-sensor.h"
#include "fimc-is-resourcemgr.h"
#include "fimc-is-hw.h"

#define DRIVER_NAME "fimc_is_eeprom_i2c"
#define DRIVER_NAME_REAR "rear-eeprom-i2c"
#define DRIVER_NAME_FRONT "front-eeprom-i2c"
#define REAR_DATA 0
#define FRONT_DATA 1


/*
 * Samsung Exynos5 SoC series FIMC-IS driver
 *
 * exynos5 fimc-is core functions
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

int sensor_eeprom_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	struct fimc_is_core *core;
	static bool probe_retried = false;

	if (!fimc_is_dev)
		goto probe_defer;

	core = (struct fimc_is_core *)dev_get_drvdata(fimc_is_dev);
	if (!core)
		goto probe_defer;

	if (id->driver_data == REAR_DATA) {
		core->eeprom_client0 = client;
	} else if (id->driver_data == FRONT_DATA) {
		core->eeprom_client1 = client;
	} else {
		err("rear eeprom device is failed!");
	}

	pr_info("%s %s[%ld]: fimc_is_sensor_eeprom probed!\n",
		dev_driver_string(&client->dev), dev_name(&client->dev), id->driver_data);

	return 0;

probe_defer:
	if (probe_retried) {
		err("probe has already been retried!!");
	}

	probe_retried = true;
	err("core device is not yet probed");
	return -EPROBE_DEFER;

}

static int sensor_eeprom_remove(struct i2c_client *client)
{
	int ret = 0;
	return ret;
}

#ifdef CONFIG_OF
static const struct of_device_id exynos_fimc_is_sensor_eeprom_match[] = {
	{
		.compatible = "samsung,rear-eeprom-i2c", .data = (void *)REAR_DATA
	},
	{
		.compatible = "samsung,front-eeprom-i2c", .data = (void *)FRONT_DATA
	},
	{},
};
#endif

static const struct i2c_device_id sensor_eeprom_idt[] = {
	{ DRIVER_NAME_REAR, REAR_DATA },
	{ DRIVER_NAME_FRONT, FRONT_DATA },
};

static struct i2c_driver sensor_eeprom_driver = {
	.driver = {
		.name	= DRIVER_NAME,
		.owner	= THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = exynos_fimc_is_sensor_eeprom_match
#endif
	},
	.probe	= sensor_eeprom_probe,
	.remove	= sensor_eeprom_remove,
	.id_table = sensor_eeprom_idt
};

static int __init sensor_eeprom_load(void)
{
        return i2c_add_driver(&sensor_eeprom_driver);
}

static void __exit sensor_eeprom_unload(void)
{
        i2c_del_driver(&sensor_eeprom_driver);
}

module_init(sensor_eeprom_load);
module_exit(sensor_eeprom_unload);

MODULE_AUTHOR("Kyoungho Yun");
MODULE_DESCRIPTION("Camera eeprom driver");
MODULE_LICENSE("GPL v2");


