/*
 * Samsung Exynos5 SoC series FIMC-IS driver
 *
 * exynos5 fimc-is core functions
 *
 * Copyright (c) 2023 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "is-core.h"
#include "is-device-sensor.h"
#include "is-resourcemgr.h"
#include "is-hw.h"
#include "is-device-rom.h"
#include "is-vendor-private.h"
#include "is-sec-define.h"

#define DRIVER_NAME "is-device-rom-i2c"

static int sensor_rom_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	struct is_core *core;
	static bool probe_retried = false;
	struct is_device_rom *rom;
	struct is_vendor_private *vendor_priv;
	struct device *is_dev;
	struct is_vendor_rom *rom_info;
	int ret;
	u32 rom_id;

	is_dev = is_get_is_dev();

	if (!is_dev)
		goto probe_defer;

	core = is_get_is_core();
	if (!core)
		goto probe_defer;

	vendor_priv = core->vendor.private_data;

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		probe_err("No I2C functionality found\n");
		return -ENODEV;
	}

	ret = of_property_read_u32(client->dev.of_node, "rom_id", &rom_id);
	BUG_ON(ret);

	if (rom_id >= ROM_ID_MAX) {
		probe_err("rom_id is invalid! %d", rom_id);
		return -ENODEV;
	}

	rom = kzalloc(sizeof(struct is_device_rom), GFP_KERNEL);
	if (!rom) {
		probe_err("is_device_rom is NULL");
		return -ENOMEM;
	}

	rom->client = client;
	rom->core = core;
	rom->rom_id = rom_id;

	i2c_set_clientdata(client, rom);

	is_sec_get_rom_info(&rom_info, rom_id);

	if (client->dev.of_node) {
		if(is_vendor_rom_parse_dt(client->dev.of_node, rom_id)) {
			probe_err("parsing device tree is fail");
			kfree(rom);
			return -ENODEV;
		}

		rom_info->buf = vzalloc(rom_info->rom_size);
		if (!rom_info->buf) {
			probe_err("failed to alloc rom buffer, id[%d] size[%d]", rom_id, rom_info->rom_size);
			kfree(rom);
			return -ENOMEM;
		}

		if (rom_info->use_standard_cal) {
			rom_info->sec_buf = vzalloc(rom_info->rom_size);
			if (!rom_info->sec_buf) {
				probe_err("failed to alloc sec_buf of rom_id[%d] rom_size[%d]",
					rom_id, rom_info->rom_size);
				kfree(rom);
				vfree(rom_info->buf);
				return -ENOMEM;
			}
		}
	}

	rom_info->client = client;
	rom_info->valid = true;

	probe_info("%s %s[%ld]: is_sensor_rom probed (rom_id=%d)!\n",
		dev_driver_string(&client->dev), dev_name(&client->dev), id->driver_data, rom_id);

	return 0;

probe_defer:
	if (probe_retried) {
		probe_err("probe has already been retried!!");
	}

	probe_retried = true;
	probe_err("core device is not yet probed");
	return -EPROBE_DEFER;
}

#ifdef CONFIG_OF
static const struct of_device_id exynos_is_sensor_rom_match[] = {
	{
		.compatible = "samsung,is-device-rom-i2c",
	},
	{},
};
#endif

static const struct i2c_device_id sensor_rom_idt[] = {
	{ DRIVER_NAME, 0 },
	{},
};

static struct i2c_driver sensor_rom_driver = {
	.driver = {
		.name	= DRIVER_NAME,
		.owner	= THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = exynos_is_sensor_rom_match
#endif
	},
	.probe	= sensor_rom_probe,
	.id_table = sensor_rom_idt,
};

#ifdef MODULE
struct i2c_driver *is_get_rom_driver(void)
{
	return &sensor_rom_driver;
}
#else
static int __init sensor_rom_init(void)
{
	int ret;

	ret = i2c_add_driver(&sensor_rom_driver);
	if (ret)
		err("failed to add %s driver: %d\n",
			sensor_rom_driver.driver.name, ret);

	return ret;
}
late_initcall_sync(sensor_rom_init);
#endif

MODULE_LICENSE("GPL");
