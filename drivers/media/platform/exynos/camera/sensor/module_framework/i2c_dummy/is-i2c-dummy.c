/*
 * Samsung Exynos SoC series Sensor driver
 *
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/i2c.h>
#include <linux/time.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/videodev2.h>
#include <videodev2_exynos_camera.h>

#include "is-device-sensor.h"
#include "is-device-sensor-peri.h"
#include "is-core.h"

#include "interface/is-interface-library.h"

#define DUMMY_NAME	"I2C_DUMMY"

static int sensor_i2c_dummy_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	int ret = 0;
	struct is_core *core;
	struct is_device_sensor *device = NULL;
	u32 sensor_id = 0;
	struct device *dev;
	struct device_node *dnode;

	FIMC_BUG(!client);

	core = pablo_get_core_async();
	if (!core) {
		err_probe("core device is not yet probed");
		ret = -EPROBE_DEFER;
		goto p_err;
	}

	dev = &client->dev;
	dnode = dev->of_node;

	ret = of_property_read_u32(dnode, "id", &sensor_id);
	if (ret) {
		err("id read is fail(%d)", ret);
		goto p_err;
	}

	probe_info("%s sensor_id(%d)\n", __func__, sensor_id);

	device = &core->sensor[sensor_id];
	if (!device->pdata) {
		err("device pdata is null");
		goto p_err;
	}

	if (!device->pdata->i2c_dummy_enable) {
		err("skip probe dummy");
		goto p_err;
	}

	if (!test_bit(IS_SENSOR_PROBE, &device->state)) {
		err("sensor device is not yet probed");
		ret = -EPROBE_DEFER;
		goto p_err;
	}

	/*
	 * it will not use slave addr that is defined at i2c_dummy node in dtsi
	 * it will use slave addr that is included in match seq that is defined at module node in dtsi
	 */
	client->addr = 0xFF;
	device->client = client;

	probe_info("%s done\n", __func__);
p_err:
	return ret;
}

static const struct of_device_id sensor_i2c_dummy_match[] = {
	{
		.compatible = "samsung,exynos-is-dummy",
	},
	{},
};
MODULE_DEVICE_TABLE(of, sensor_i2c_dummy_match);

static const struct i2c_device_id sensor_i2c_dummy_idt[] = {
	{ DUMMY_NAME, 0 },
	{},
};

static struct i2c_driver pablo_i2c_dummy_driver = {
	.probe  = sensor_i2c_dummy_probe,
	.driver = {
		.name	= DUMMY_NAME,
		.owner	= THIS_MODULE,
		.of_match_table = sensor_i2c_dummy_match,
		.suppress_bind_attrs = true,
	},
	.id_table = sensor_i2c_dummy_idt,
};

#ifdef MODULE
static int pablo_i2c_dummy_init(void)
{
	int ret;

	ret =  i2c_add_driver(&pablo_i2c_dummy_driver);
	if (ret)
		err("sensor_i2c_dummy_driver register failed(%d)", ret);

	return ret;
}
module_init(pablo_i2c_dummy_init);

void pablo_i2c_dummy_exit(void)
{
	i2c_del_driver(&pablo_i2c_dummy_driver);
}
module_exit(pablo_i2c_dummy_exit);
#else
static int __init sensor_i2c_dummy_init(void)
{
	int ret;

	ret = i2c_add_driver(&pablo_i2c_dummy_driver);
	if (ret)
		err("failed to add %s driver: %d\n",
				pablo_i2c_dummy_driver.driver.name, ret);

	return ret;
}
late_initcall(sensor_i2c_dummy_init);
#endif

MODULE_DESCRIPTION("Samsung EXYNOS SoC Pablo I2C Dummy driver");
MODULE_ALIAS("platform:samsung-pablo-i2c-dummy");
MODULE_LICENSE("GPL v2");
