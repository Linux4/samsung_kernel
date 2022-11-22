/*
 * Samsung Exynos5 SoC series Sensor driver
 *
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

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
#ifdef CONFIG_OF
#include <linux/of_gpio.h>
#endif
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
#include "fimc-is-device-4h5.h"

#define SENSOR_NAME "S5K4H5"

static struct fimc_is_sensor_cfg config_4h5[] = {
	/* 3280x2458@30fps */
	FIMC_IS_SENSOR_CFG(3280, 2458, 30, 15, 0),
	/* 3280x1846@30fps */
	FIMC_IS_SENSOR_CFG(3280, 1846, 30, 15, 1),
	/* 1640x924@60fps */
	FIMC_IS_SENSOR_CFG(1640, 924, 60, 15, 2),
	/* 816x460@120fps */
	FIMC_IS_SENSOR_CFG(816, 460, 120, 15, 3),
	/* 3280x2458@24fps */
	FIMC_IS_SENSOR_CFG(3280, 2458, 24, 15, 4),
	/* 3280x1846@24fps */
	FIMC_IS_SENSOR_CFG(3280, 1846, 24, 15, 5),
	/* 816x604@115fps */
	FIMC_IS_SENSOR_CFG(816, 604, 115, 15, 6),
	/* 1640x1228@15fps */
	FIMC_IS_SENSOR_CFG(1640, 1228, 15, 15, 7),
};

static int sensor_4h5_init(struct v4l2_subdev *subdev, u32 val)
{
	int ret = 0;
	struct fimc_is_module_enum *module;

	BUG_ON(!subdev);

	module = (struct fimc_is_module_enum *)v4l2_get_subdevdata(subdev);

	pr_info("[MOD:D:%d] %s(%d)\n", module->id, __func__, val);

	return ret;
}

static const struct v4l2_subdev_core_ops core_ops = {
	.init = sensor_4h5_init
};

static const struct v4l2_subdev_ops subdev_ops = {
	.core = &core_ops
};

#ifdef CONFIG_OF
#ifdef CONFIG_COMPANION_USE
static int sensor_4h5_power_setpin(struct device *dev)
{
	return 0;
}
#else
static int sensor_4h5_power_setpin(struct device *dev)
{
	struct exynos_platform_fimc_is_sensor *pdata;
	struct device_node *dnode;
	int gpio_none = 0;
	int gpio_reset = 0, gpios_cam_en = -1;

	BUG_ON(!dev);
	BUG_ON(!dev->platform_data);

	dnode = dev->of_node;
	pdata = dev->platform_data;

	gpio_reset = of_get_named_gpio(dnode, "gpio_reset", 0);
	if (!gpio_is_valid(gpio_reset)) {
		err("failed to get PIN_RESET");
		return -EINVAL;
	} else {
		gpio_request_one(gpio_reset, GPIOF_IN, "CAM_GPIO_INPUT");
		gpio_free(gpio_reset);
	}

	if (of_find_property(dnode, "gpios_cam_en", NULL)) {
		gpios_cam_en = of_get_named_gpio(dnode, "gpios_cam_en", 0);
		if (!gpio_is_valid(gpios_cam_en)) {
			err("failed to get main cam en gpio");
		} else {
			gpio_request_one(gpios_cam_en, GPIOF_IN, "CAM_GPIO_INPUT");
			gpio_free(gpios_cam_en);
		}
	}

	/* BACK CAMERA	- POWER ON */
	if (gpio_is_valid(gpios_cam_en)) {
		SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 0, gpios_cam_en, 0, NULL, 0, PIN_OUTPUT_HIGH);
	} else {
		SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 0, gpio_none, 0, "CAM_SEN_A2.8V_AP", 0, PIN_REGULATOR_ON);
	}
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 1, gpio_none, 0, "CAM_SEN_CORE_1.2V_AP", 0, PIN_REGULATOR_ON);
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 2, gpio_none, 0, "CAM_AF_2.8V_AP", 2000, PIN_REGULATOR_ON);
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 3, gpio_none, 0, "CAM_IO_1.8V_AP", 2000, PIN_REGULATOR_ON);

	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 4, gpio_none, 0, "ch", 0, PIN_FUNCTION);
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 5, gpio_reset, 0, NULL, 0, PIN_OUTPUT_HIGH);
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 6, gpio_none, 0, "af", 0, PIN_FUNCTION);
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 7, gpio_none, 0, NULL, 0, PIN_END);

	/* BACK CAMERA	- POWER OFF */
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, 0, gpio_none, 0, "CAM_AF_2.8V_AP", 2000, PIN_REGULATOR_OFF);
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, 1, gpio_none, 0, "off", 0, PIN_FUNCTION);
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, 2, gpio_reset, 0, NULL, 0, PIN_OUTPUT_LOW);
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, 3, gpio_none, 0, "CAM_IO_1.8V_AP", 0, PIN_REGULATOR_OFF);
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, 4, gpio_none, 0, "CAM_SEN_CORE_1.2V_AP", 0, PIN_REGULATOR_OFF);
	if (gpio_is_valid(gpios_cam_en)) {
		SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, 5, gpios_cam_en, 0, NULL, 0, PIN_OUTPUT_LOW);
	} else {
		SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, 5, gpio_none, 0, "CAM_SEN_A2.8V_AP", 0, PIN_REGULATOR_OFF);
	}
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, 6, gpio_none, 0, NULL, 0, PIN_END);

	return 0;
}
#endif /* CONFIG_COMPANION_USE */
#endif /* CONFIG_OF */

int sensor_4h5_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	int ret = 0;
	struct fimc_is_core *core;
	struct v4l2_subdev *subdev_module;
	struct fimc_is_module_enum *module;
	struct fimc_is_device_sensor *device;
	struct sensor_open_extended *ext;

	BUG_ON(!fimc_is_dev);

	core = (struct fimc_is_core *)dev_get_drvdata(fimc_is_dev);
	if (!core) {
		err("core device is not yet probed");
		return -EPROBE_DEFER;
	}

	device = &core->sensor[SENSOR_S5K4H5_INSTANCE];

	subdev_module = kzalloc(sizeof(struct v4l2_subdev), GFP_KERNEL);
	if (!subdev_module) {
		err("subdev_module is NULL");
		ret = -ENOMEM;
		goto p_err;
	}

	module = &device->module_enum[atomic_read(&core->resourcemgr.rsccount_module)];
	atomic_inc(&core->resourcemgr.rsccount_module);
	module->id = SENSOR_NAME_S5K4H5;
	module->subdev = subdev_module;
	module->device = SENSOR_S5K4H5_INSTANCE;
	module->ops = NULL;
	module->client = client;
	module->active_width = 3264;
	module->active_height = 2448;
	module->margin_left = 8;
	module->margin_right = 8;
	module->margin_top = 6;
	module->margin_bottom = 4;
	module->pixel_width = module->active_width + 16;
	module->pixel_height = module->active_height + 10;
	module->max_framerate = 120;
	module->position = SENSOR_POSITION_REAR;
	module->mode = CSI_MODE_CH0_ONLY;
	module->lanes = CSI_DATA_LANES_4;
	module->sensor_maker = "SLSI";
	module->sensor_name = "S5K4H5";
	module->setfile_name = "setfile_4h5.bin";
	module->cfgs = ARRAY_SIZE(config_4h5);
	module->cfg = config_4h5;
	module->ops = NULL;
	module->private_data = NULL;
#ifdef CONFIG_OF
	module->power_setpin = sensor_4h5_power_setpin;
#endif

	ext = &module->ext;
	ext->mipi_lane_num = module->lanes;
	ext->I2CSclk = I2C_L0;

	ext->sensor_con.product_name = SENSOR_NAME_S5K4H5;
	ext->sensor_con.peri_type = SE_I2C;
	ext->sensor_con.peri_setting.i2c.channel = SENSOR_CONTROL_I2C0;
	ext->sensor_con.peri_setting.i2c.slave_address = 0x6E;
	ext->sensor_con.peri_setting.i2c.speed = 400000;

	ext->actuator_con.product_name = ACTUATOR_NAME_DW9804;
	ext->actuator_con.peri_type = SE_I2C;
	ext->actuator_con.peri_setting.i2c.channel = SENSOR_CONTROL_I2C1;
	ext->actuator_con.peri_setting.i2c.slave_address = 0x18;
	ext->actuator_con.peri_setting.i2c.speed = 400000;

	ext->from_con.product_name = FROMDRV_NAME_NOTHING;

	ext->companion_con.product_name = COMPANION_NAME_NOTHING;

	if (client)
		v4l2_i2c_subdev_init(subdev_module, client, &subdev_ops);
	else
		v4l2_subdev_init(subdev_module, &subdev_ops);

	v4l2_set_subdevdata(subdev_module, module);
	v4l2_set_subdev_hostdata(subdev_module, device);
	snprintf(subdev_module->name, V4L2_SUBDEV_NAME_SIZE, "sensor-subdev.%d", module->id);

p_err:
	info("%s(%d)\n", __func__, ret);
	return ret;
}
