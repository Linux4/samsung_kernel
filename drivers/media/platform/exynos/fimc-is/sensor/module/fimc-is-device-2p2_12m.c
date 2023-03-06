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
#include "fimc-is-device-2p2_12m.h"

#define SENSOR_NAME "S5K2P2_12M"

static struct fimc_is_sensor_cfg config_2p2_12m[] = {
	/* 4624x2604@30fps */
	FIMC_IS_SENSOR_CFG(4624, 2604, 30, 22, 0),
	/* 4624x2604@24fps */
	FIMC_IS_SENSOR_CFG(4624, 2604, 24, 21, 1),
	/* 3472X2604@30fps */
	FIMC_IS_SENSOR_CFG(3472, 2604, 30, 17, 2),
	/* 3472X2604@24fps */
	FIMC_IS_SENSOR_CFG(3472, 2604, 24, 17, 3),
	/* 2608X2604@30fps */
	FIMC_IS_SENSOR_CFG(2608, 2604, 30, 17, 4),
	/* 2608X2604@24fps */
	FIMC_IS_SENSOR_CFG(2608, 2604, 24, 17, 5),
	/* 2312X1300@60fps */
	FIMC_IS_SENSOR_CFG(2312, 1300, 60, 13, 6),
	/* 2312X1300@30fps */
	FIMC_IS_SENSOR_CFG(2312, 1300, 30, 13, 7),
	/* 1156X650@120fps */
	FIMC_IS_SENSOR_CFG(1156, 650, 120, 13, 8),
};

static int sensor_2p2_12m_init(struct v4l2_subdev *subdev, u32 val)
{
	int ret = 0;
	struct fimc_is_module_enum *module;

	BUG_ON(!subdev);

	module = (struct fimc_is_module_enum *)v4l2_get_subdevdata(subdev);

	pr_info("[MOD:D:%d] %s(%d)\n", module->id, __func__, val);

	return ret;
}

static const struct v4l2_subdev_core_ops core_ops = {
	.init = sensor_2p2_12m_init
};

static const struct v4l2_subdev_ops subdev_ops = {
	.core = &core_ops
};

#ifdef CONFIG_OF
#ifdef CONFIG_COMPANION_USE
static int sensor_2p2_12m_power_setpin(struct device *dev)
{
	struct exynos_platform_fimc_is_sensor *pdata;
	struct device_node *dnode;
	int gpio_comp_en = 0, gpio_comp_rst = 0;
	int gpio_none = 0;
	int gpio_reset = 0;
	int gpios_cam_en = -EINVAL;
#ifdef CONFIG_OIS_USE
	int gpios_ois_en = 0;
#endif
	BUG_ON(!dev);
	BUG_ON(!dev->platform_data);

	dnode = dev->of_node;
	pdata = dev->platform_data;

	gpio_comp_en = of_get_named_gpio(dnode, "gpios_comp_en", 0);
	if (!gpio_is_valid(gpio_comp_en)) {
		dev_err(dev, "failed to get main comp en gpio\n");
	} else {
		gpio_request_one(gpio_comp_en, GPIOF_OUT_INIT_LOW, "CAM_GPIO_OUTPUT_LOW");
		gpio_free(gpio_comp_en);
	}

	gpio_comp_rst = of_get_named_gpio(dnode, "gpios_comp_reset", 0);
	if (!gpio_is_valid(gpio_comp_rst)) {
		dev_err(dev, "failed to get main comp reset gpio\n");
	} else {
		gpio_request_one(gpio_comp_rst, GPIOF_OUT_INIT_LOW, "CAM_GPIO_OUTPUT_LOW");
		gpio_free(gpio_comp_rst);
	}

	gpio_reset = of_get_named_gpio(dnode, "gpio_reset", 0);
	if (!gpio_is_valid(gpio_reset)) {
		dev_err(dev, "failed to get PIN_RESET\n");
		return -EINVAL;
	} else {
		gpio_request_one(gpio_reset, GPIOF_OUT_INIT_LOW, "CAM_GPIO_OUTPUT_LOW");
		gpio_free(gpio_reset);
	}

	if (of_get_property(dnode, "gpios_cam_en", NULL)) {
		gpios_cam_en = of_get_named_gpio(dnode, "gpios_cam_en", 0);
		if (!gpio_is_valid(gpios_cam_en)) {
			dev_err(dev, "failed to get main cam en gpio\n");
		} else {
			gpio_request_one(gpios_cam_en, GPIOF_OUT_INIT_LOW, "CAM_GPIO_OUTPUT_LOW");
			gpio_free(gpios_cam_en);
		}
	}

#ifdef CONFIG_OIS_USE
	gpios_ois_en = of_get_named_gpio(dnode, "gpios_ois_en", 0);
	pdata->pin_ois_en = gpios_ois_en;
	if (!gpio_is_valid(gpios_ois_en)) {
		dev_err(dev, "failed to get ois en gpio\n");
	} else {
		gpio_request_one(gpios_ois_en, GPIOF_OUT_INIT_LOW, "CAM_GPIO_OUTPUT_LOW");
		gpio_free(gpios_ois_en);
	}
#endif

	if (gpio_is_valid(gpios_cam_en)) {
		SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 0, gpios_cam_en, 0, NULL, 0, PIN_OUTPUT_HIGH);
	} else {
		SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 0, gpio_none, 0, "CAM_SEN_A2.8V_AP", 0, PIN_REGULATOR_ON);
	}
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 1, gpio_none, 0, "CAM_SEN_CORE_1.2V_AP", 0, PIN_REGULATOR_ON);
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 2, gpio_none, 0, "CAM_AF_2.8V_AP", 2000, PIN_REGULATOR_ON);
#ifdef CONFIG_OIS_USE
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 3, gpios_ois_en, 0, NULL, 0, PIN_OUTPUT_HIGH);
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 4, gpio_none, 0, "OIS_VM_2.8V", 0, PIN_REGULATOR_ON);
#endif
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 5, gpio_none, 0, "CAM_IO_1.8V_AP", 0, PIN_REGULATOR_ON);
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 6, gpio_none, 0, "VDDA_1.8V_COMP", 0, PIN_REGULATOR_ON);
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 7, gpio_comp_en, 0, NULL, 150, PIN_OUTPUT_HIGH);
	if (pdata->companion_use_pmic) {
		SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 8, gpio_none, 0, "VDD_MIPI_1.0V_COMP", 0, PIN_REGULATOR_ON);
	}
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 9, gpio_comp_rst, 0, NULL, 0, PIN_OUTPUT_HIGH);
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 10, gpio_none, 0, "ch", 0, PIN_FUNCTION);
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 11, gpio_none, 0, "af", 0, PIN_FUNCTION);
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 12, gpio_reset, 0, NULL, 0, PIN_OUTPUT_HIGH);
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 13, gpio_none, 0, NULL, 0, PIN_END);

	/* BACK CAMERA  - POWER OFF */
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, 0, gpio_none, 0, "CAM_AF_2.8V_AP", 2000, PIN_REGULATOR_OFF);
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, 1, gpio_none, 0, "off", 0, PIN_FUNCTION);
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, 2, gpio_reset, 0, NULL, 0, PIN_OUTPUT_LOW);
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, 3, gpio_comp_rst, 0, NULL, 0, PIN_OUTPUT_LOW);
	if (pdata->companion_use_pmic) {
		SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, 4, gpio_none, 0, "VDD_MIPI_1.0V_COMP", 0, PIN_REGULATOR_OFF);
	}
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, 5, gpio_comp_en, 0, NULL, 0, PIN_OUTPUT_LOW);
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, 6, gpio_none, 0, "VDDA_1.8V_COMP", 0, PIN_REGULATOR_OFF);
	if (gpio_is_valid(gpios_cam_en)) {
		SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, 7, gpios_cam_en, 0, NULL, 0, PIN_OUTPUT_LOW);
	} else {
		SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, 7, gpio_none, 0, "CAM_SEN_A2.8V_AP", 0, PIN_REGULATOR_OFF);
	}
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, 8, gpio_none, 0, "CAM_SEN_CORE_1.2V_AP", 0, PIN_REGULATOR_OFF);
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, 9, gpio_none, 0, "CAM_IO_1.8V_AP", 0, PIN_REGULATOR_OFF);
#ifdef CONFIG_OIS_USE
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, 10, gpios_ois_en, 0, NULL, 0, PIN_OUTPUT_LOW);
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, 11, gpio_none, 0, "OIS_VM_2.8V", 0, PIN_REGULATOR_OFF);
#endif
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, 12, gpio_none, 0, NULL, 0, PIN_END);

#ifdef CONFIG_OIS_USE
	/* OIS_FACTORY  - POWER ON */
	SET_PIN(pdata, SENSOR_SCENARIO_OIS_FACTORY, GPIO_SCENARIO_ON, 0, gpio_none, 0, "CAM_AF_2.8V_AP", 2000, PIN_REGULATOR_ON);
	SET_PIN(pdata, SENSOR_SCENARIO_OIS_FACTORY, GPIO_SCENARIO_ON, 1, gpios_ois_en, 0, NULL, 0, PIN_OUTPUT_HIGH);
	SET_PIN(pdata, SENSOR_SCENARIO_OIS_FACTORY, GPIO_SCENARIO_ON, 2, gpio_none, 0, "OIS_VM_2.8V", 0, PIN_REGULATOR_ON);
	SET_PIN(pdata, SENSOR_SCENARIO_OIS_FACTORY, GPIO_SCENARIO_ON, 3, gpio_none, 0, "CAM_IO_1.8V_AP", 0, PIN_REGULATOR_ON);
	SET_PIN(pdata, SENSOR_SCENARIO_OIS_FACTORY, GPIO_SCENARIO_ON, 4, gpio_reset, 0, NULL, 0, PIN_OUTPUT_HIGH);
	SET_PIN(pdata, SENSOR_SCENARIO_OIS_FACTORY, GPIO_SCENARIO_ON, 5, gpio_none, 0, NULL, 0, PIN_END);

	/* OIS_FACTORY  - POWER OFF */
	SET_PIN(pdata, SENSOR_SCENARIO_OIS_FACTORY, GPIO_SCENARIO_OFF, 0, gpio_none, 0, "CAM_AF_2.8V_AP", 2000, PIN_REGULATOR_OFF);
	SET_PIN(pdata, SENSOR_SCENARIO_OIS_FACTORY, GPIO_SCENARIO_OFF, 1, gpio_reset, 0, NULL, 0, PIN_OUTPUT_LOW);
	SET_PIN(pdata, SENSOR_SCENARIO_OIS_FACTORY, GPIO_SCENARIO_OFF, 2, gpio_none, 0, "CAM_IO_1.8V_AP", 0, PIN_REGULATOR_OFF);
	SET_PIN(pdata, SENSOR_SCENARIO_OIS_FACTORY, GPIO_SCENARIO_OFF, 3, gpios_ois_en, 0, NULL, 0, PIN_OUTPUT_LOW);
	SET_PIN(pdata, SENSOR_SCENARIO_OIS_FACTORY, GPIO_SCENARIO_OFF, 4, gpio_none, 0, "OIS_VM_2.8V", 0, PIN_REGULATOR_OFF);
	SET_PIN(pdata, SENSOR_SCENARIO_OIS_FACTORY, GPIO_SCENARIO_OFF, 5, gpio_none, 0, NULL, 0, PIN_END);
#endif

	return 0;
}
#else
static int sensor_2p2_12m_power_setpin(struct device *dev)
{
	return 0;
}
#endif /* CONFIG_COMPANION_USE */
#endif /* CONFIG_OF */


int sensor_2p2_12m_probe(struct i2c_client *client,
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

	device = &core->sensor[SENSOR_2P2_12M_INSTANCE];

	subdev_module = kzalloc(sizeof(struct v4l2_subdev), GFP_KERNEL);
	if (!subdev_module) {
		err("subdev_module is NULL");
		ret = -ENOMEM;
		goto p_err;
	}

	module = &device->module_enum[atomic_read(&core->resourcemgr.rsccount_module)];
	atomic_inc(&core->resourcemgr.rsccount_module);
	module->id = SENSOR_NAME_S5K2P2_12M;
	module->subdev = subdev_module;
	module->device = SENSOR_2P2_12M_INSTANCE;
	module->client = client;
	module->active_width = 4608;
	module->active_height = 2594;
	module->margin_left = 8;
	module->margin_right = 8;
	module->margin_top = 6;
	module->margin_bottom = 4;
	module->pixel_width = module->active_width + 16;
	module->pixel_height = module->active_height + 10;
	module->max_framerate = 300;
	module->position = SENSOR_POSITION_REAR;
	module->mode = CSI_MODE_CH0_ONLY;
	module->lanes = CSI_DATA_LANES_4;
	module->sensor_maker = "SLSI";
	module->sensor_name = "S5K2P2";
	module->setfile_name = "setfile_2p2_12m.bin";
	module->cfgs = ARRAY_SIZE(config_2p2_12m);
	module->cfg = config_2p2_12m;
	module->ops = NULL;
	module->private_data = NULL;
#ifdef CONFIG_OF
	module->power_setpin = sensor_2p2_12m_power_setpin;
#endif
	ext = &module->ext;
	ext->mipi_lane_num = module->lanes;
	ext->I2CSclk = I2C_L0;

	ext->sensor_con.product_name = SENSOR_NAME_S5K2P2_12M;
	ext->sensor_con.peri_type = SE_I2C;
	ext->sensor_con.peri_setting.i2c.channel = SENSOR_CONTROL_I2C0;
	ext->sensor_con.peri_setting.i2c.slave_address = 0x5A;
	ext->sensor_con.peri_setting.i2c.speed = 400000;

	ext->actuator_con.product_name = ACTUATOR_NAME_AK7345;
	ext->actuator_con.peri_type = SE_I2C;
	ext->actuator_con.peri_setting.i2c.channel = SENSOR_CONTROL_I2C1;
	ext->actuator_con.peri_setting.i2c.slave_address = 0x5A;
	ext->actuator_con.peri_setting.i2c.speed = 400000;

#ifdef CONFIG_LEDS_MAX77804
	ext->flash_con.product_name = FLADRV_NAME_MAX77693;
#endif
#if defined(CONFIG_LEDS_LM3560) || !defined(CONFIG_USE_VENDER_FEATURE)
	ext->flash_con.product_name = FLADRV_NAME_LM3560;
#endif
#ifdef CONFIG_LEDS_SKY81296
	ext->flash_con.product_name = FLADRV_NAME_SKY81296;
#endif
	ext->flash_con.peri_type = SE_GPIO;
#ifdef CONFIG_USE_VENDER_FEATURE
	ext->flash_con.peri_setting.gpio.first_gpio_port_no = 1;
	ext->flash_con.peri_setting.gpio.second_gpio_port_no = 2;
#else
	ext->flash_con.peri_setting.gpio.first_gpio_port_no = 2;
	ext->flash_con.peri_setting.gpio.second_gpio_port_no = 3;
#endif

	ext->from_con.product_name = FROMDRV_NAME_NOTHING;

#ifdef CONFIG_COMPANION_USE
	ext->companion_con.product_name = COMPANION_NAME_73C1;
	ext->companion_con.peri_info0.valid = true;
	ext->companion_con.peri_info0.peri_type = SE_SPI;
	ext->companion_con.peri_info0.peri_setting.spi.channel = (int) core->companion_spi_channel;
	ext->companion_con.peri_info1.valid = true;
	ext->companion_con.peri_info1.peri_type = SE_I2C;
	ext->companion_con.peri_info1.peri_setting.i2c.channel = 0;
	ext->companion_con.peri_info1.peri_setting.i2c.slave_address = 0x7A;
	ext->companion_con.peri_info1.peri_setting.i2c.speed = 400000;
	ext->companion_con.peri_info2.valid = true;
	ext->companion_con.peri_info2.peri_type = SE_FIMC_LITE;
	ext->companion_con.peri_info2.peri_setting.fimc_lite.channel = FLITE_ID_D;
#else
	ext->companion_con.product_name = COMPANION_NAME_NOTHING;
#endif

	if (client) {
		v4l2_i2c_subdev_init(subdev_module, client, &subdev_ops);
	} else {
		v4l2_subdev_init(subdev_module, &subdev_ops);
	}

	v4l2_set_subdevdata(subdev_module, module);
	v4l2_set_subdev_hostdata(subdev_module, device);
	snprintf(subdev_module->name, V4L2_SUBDEV_NAME_SIZE, "sensor-subdev.%d", module->id);

p_err:
	info("%s(%d)\n", __func__, ret);
	return ret;
}
