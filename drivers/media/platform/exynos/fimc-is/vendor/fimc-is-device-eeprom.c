
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
#include "fimc-is-device-eeprom.h"
#include "fimc-is-sec-define.h"

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

int fimc_is_eeprom_parse_dt(struct i2c_client *client)
{
	int ret = 0;

	struct fimc_is_device_eeprom *device = i2c_get_clientdata(client);
	struct fimc_is_eeprom_gpio *gpio;
	struct device_node *np = client->dev.of_node;
#ifdef CONFIG_CAMERA_SUPPORT_MULTI_EEPROM
	struct fimc_is_eeprom_ext_module_info *module_info =  &device->ext_module_info;
#endif

	gpio = &device->gpio;

	if (device->driver_data == REAR_DATA) {
		gpio->use_i2c_pinctrl = of_property_read_bool(np, "use_i2c_pinctrl");
		if (gpio->use_i2c_pinctrl) {
			ret = of_property_read_string(np, "fimc_is_rear_eeprom_sda", (const char **) &gpio->sda);
			if (ret) {
				err("rear eeprom gpio: fail to read, fimc_is_rear_eeprom_sda\n");
				ret = -ENODEV;
				goto p_err;
			}
			ret = of_property_read_string(np, "fimc_is_rear_eeprom_scl",(const char **) &gpio->scl);
			if (ret) {
				err("rear eeprom gpio: fail to read, fimc_is_rear_eeprom_scl\n");
				ret = -ENODEV;
				goto p_err;
			}
			ret = of_property_read_string(np, "fimc_is_rear_eeprom_pinname",(const char **) &gpio->pinname);
			if (ret) {
				err("rear eeprom gpio: fail to read, fimc_is_rear_eeprom_pinname\n");
				ret = -ENODEV;
				goto p_err;
			}
			ret = of_property_read_u32(np, "pinfunc_on", &gpio->pinfunc_on);
			if (ret) {
				err("rear eeprom gpio: fail to read, pinfunc_on\n");
				ret = -ENODEV;
				goto p_err;
			}
			ret = of_property_read_u32(np, "pinfunc_off", &gpio->pinfunc_off);
			if (ret) {
				err("rear eeprom gpio: fail to read, pinfunc_off\n");
				ret = -ENODEV;
				goto p_err;
			}

			info("[%s eeprom] sda = %s, scl = %s, pinname = %s\n", "rear", gpio->sda, gpio->scl, gpio->pinname);
		}

#ifdef CONFIG_CAMERA_SUPPORT_MULTI_EEPROM
		module_info->use_module_i2c_check = of_property_read_bool(np, "use_module_i2c_check");
		if (module_info->use_module_i2c_check) {
			ret = of_property_read_u32(np, "actuator_id", &module_info->actuator_id);
			if (ret) {
				err("fail to read rear eeprom actuator_id");
				ret = -ENODEV;
				goto p_err;
			}
			info("use_module_i2c_check actuator_id(%d)", module_info->actuator_id);
		}
#endif
	} else if (device->driver_data == FRONT_DATA) {
		gpio->use_i2c_pinctrl = of_property_read_bool(np, "use_i2c_pinctrl");
		if (gpio->use_i2c_pinctrl) {
			ret = of_property_read_string(np, "fimc_is_front_eeprom_sda", (const char **) &gpio->sda);
			if (ret) {
				err("front eeprom gpio: fail to read, fimc_is_front_eeprom_sda\n");
				ret = -ENODEV;
				goto p_err;
			}
			ret = of_property_read_string(np, "fimc_is_front_eeprom_scl",(const char **) &gpio->scl);
			if (ret) {
				err("front eeprom gpio: fail to read, fimc_is_front_eeprom_scl\n");
				ret = -ENODEV;
				goto p_err;
			}
			ret = of_property_read_string(np, "fimc_is_front_eeprom_pinname",(const char **) &gpio->pinname);
			if (ret) {
				err("front eeprom gpio: fail to read, fimc_is_front_eeprom_pinname\n");
				ret = -ENODEV;
				goto p_err;
			}
			ret = of_property_read_u32(np, "pinfunc_on", &gpio->pinfunc_on);
			if (ret) {
				err("rear eeprom gpio: fail to read, pinfunc_on\n");
				ret = -ENODEV;
				goto p_err;
			}
			ret = of_property_read_u32(np, "pinfunc_off", &gpio->pinfunc_off);
			if (ret) {
				err("rear eeprom gpio: fail to read, pinfunc_off\n");
				ret = -ENODEV;
				goto p_err;
			}

			info("[%s eeprom] sda = %s, scl = %s, pinname = %s\n", "front", gpio->sda, gpio->scl, gpio->pinname);
		}
	} else {
		err("no eeprom driver");
		ret = -ENODEV;
		goto p_err;
	}

p_err:
	return ret;
}

#ifdef CONFIG_CAMERA_SUPPORT_MULTI_EEPROM
int fimc_is_check_eeprom_client(struct i2c_client *client)
{
	const u32 addr_size = 2, max_retry = 3;
	u8 addr_buf[2] = {0, };
	int retries = max_retry;
	int ret = 0;

	pr_info("%s: check eeprom clients!\n", __func__);
	if (fimc_is_sec_ldo_enable(fimc_is_dev, "CAM_IO_1.8V_AP", true))
		pr_err("%s : error, failed to cam_io(on)\n", __func__);
#ifdef CONFIG_CAMERA_ACTUATOR_DW9807
	if (fimc_is_sec_ldo_enable(fimc_is_dev, "CAM_AF_2.8V_AP", true))
		pr_err("%s : error, failed to cam_af(on)\n", __func__);
#endif

	for (retries = max_retry; retries > 0; retries--) {
		ret = i2c_master_send(client, addr_buf, addr_size);
		if (likely(addr_size == ret))
			break;

		pr_info("%s: i2c_master_send failed(%d), try %d\n", __func__, ret, retries);
		usleep_range(1000, 1000);
	}

#ifdef CONFIG_CAMERA_ACTUATOR_DW9807
	if (fimc_is_sec_ldo_enable(fimc_is_dev, "CAM_AF_2.8V_AP", false))
		pr_err("%s : error, failed to cam_af(off)\n", __func__);
#endif
	if (fimc_is_sec_ldo_enable(fimc_is_dev, "CAM_IO_1.8V_AP", false))
		pr_err("%s : error, failed to cam_io(off)\n", __func__);

	if (unlikely(ret <= 0)) {
		pr_err("%s: error %d, fail to module_i2c_check addr(0x%2x)\n", __func__, ret, client->addr);
		return -1;
	}

	return 0;
}
#endif

int sensor_eeprom_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	struct fimc_is_core *core;
	static bool probe_retried = false;
	struct fimc_is_device_eeprom *device;
#ifdef CONFIG_CAMERA_SUPPORT_MULTI_EEPROM
	struct fimc_is_eeprom_ext_module_info *module_info;
	struct fimc_is_device_sensor *sensor_device;
	struct fimc_is_device_eeprom *prev_device;
#endif

	if (!fimc_is_dev)
		goto probe_defer;

	core = (struct fimc_is_core *)dev_get_drvdata(fimc_is_dev);
	if (!core)
		goto probe_defer;

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		err("No I2C functionality found\n");
		return -ENODEV;
	}

	device = kzalloc(sizeof(struct fimc_is_device_eeprom), GFP_KERNEL);
	if (!device) {
		err("fimc_is_device_eeprom is NULL");
		return -ENOMEM;
	}
	device->client = client;
	device->core = core;
	device->driver_data = id->driver_data;

	i2c_set_clientdata(client, device);

	if (client->dev.of_node) {
		if(fimc_is_eeprom_parse_dt(client)) {
			err("parsing device tree is fail");
			i2c_set_clientdata(client, NULL);
			kfree(device);
			return -ENODEV;
		}
	}

#ifdef CONFIG_CAMERA_SUPPORT_MULTI_EEPROM
	module_info =  &device->ext_module_info;

	if (module_info->use_module_i2c_check) {
		if (fimc_is_check_eeprom_client(client) < 0) {
			pr_err("use_module_i2c_check : fail to check_eeprom_client(%s)\n", dev_name(&client->dev));
			i2c_set_clientdata(client, NULL);
			kfree(device);
			return -ENODEV;
		} else {
			if (core->eeprom_client0 != NULL) {
				info("remove prev_device eeprom");
				prev_device = i2c_get_clientdata(core->eeprom_client0);
				i2c_set_clientdata(core->eeprom_client0, NULL);
				kfree(prev_device);
			}
		}

		sensor_device = &core->sensor[0];
		sensor_device->pdata->actuator_id = module_info->actuator_id;
	}
#endif

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


