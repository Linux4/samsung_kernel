/*
 * Samsung Exynos5 SoC series Flash driver
 *
 * Copyright (c) 2023 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/gpio.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/platform_device.h>
#include <linux/of_gpio.h>

#include <linux/videodev2.h>
#include <linux/videodev2_exynos_camera.h>

#ifndef CONFIG_LEDS_AW36518
#include "is-flash.h"
#endif

#include "is-device-sensor.h"
#include "is-device-sensor-peri.h"
#include "is-core.h"

#if IS_ENABLED(CONFIG_LEDS_AW36518)
#include <linux/leds-aw36518.h>
#endif

static int flash_aw36518_init(struct v4l2_subdev *subdev, u32 val)
{
	int ret = 0;
	struct is_flash *flash;

	FIMC_BUG(!subdev);

	flash = (struct is_flash *)v4l2_get_subdevdata(subdev);

	FIMC_BUG(!flash);

	flash->flash_data.mode = CAM2_FLASH_MODE_OFF;
	flash->flash_data.intensity = 100; /* TODO: Need to figure out min/max range */
	flash->flash_data.firing_time_us = 1 * 1000 * 1000; /* Max firing time is 1sec */
	flash->flash_data.flash_fired = false;
	gpio_request_one(flash->flash_gpio, GPIOF_OUT_INIT_LOW, "CAM_FLASH_OUTPUT");
	gpio_free(flash->flash_gpio);
#if IS_ENABLED(CONFIG_LEDS_AW36518)
	ret = aw36518_fled_mode_ctrl(AW36518_FLED_MODE_OFF, 0);
#else
	ret = control_flash_gpio(flash->flash_gpio, 0);
#endif
	return ret;
}

int flash_aw36518_s_ctrl(struct v4l2_subdev *subdev, struct v4l2_control *ctrl)
{
	int ret = 0;
	int mode;
	struct is_flash *flash = NULL;

	FIMC_BUG(!subdev);

	flash = (struct is_flash *)v4l2_get_subdevdata(subdev);
	FIMC_BUG(!flash);

	switch (ctrl->id) {
	case V4L2_CID_FLASH_SET_INTENSITY:
		/* TODO : Check min/max intensity */
		if (ctrl->value < 0) {
			err("failed to flash set intensity: %d\n", ctrl->value);
			ret = -EINVAL;
			goto p_err;
		}
		flash->flash_data.intensity = ctrl->value;
		break;
	case V4L2_CID_FLASH_SET_FIRING_TIME:
		/* TODO : Check min/max firing time */
		if (ctrl->value < 0) {
			err("failed to flash set firing time: %d\n",
				ctrl->value);
			ret = -EINVAL;
			goto p_err;
		}
		flash->flash_data.firing_time_us = ctrl->value;
		break;
	case V4L2_CID_FLASH_SET_FIRE:
		mode = flash->flash_data.mode;
#if IS_ENABLED(CONFIG_LEDS_AW36518)
		if (mode == CAM2_FLASH_MODE_OFF) {
			ret = aw36518_fled_mode_ctrl(AW36518_FLED_MODE_OFF, 0);
			if (ret)
				err("%s off fail", __func__);
		} else if (mode == CAM2_FLASH_MODE_SINGLE) {
			ret = aw36518_fled_mode_ctrl(
				AW36518_FLED_MODE_MAIN_FLASH, 0);
			if (ret)
				err("%s capture flash on fail", __func__);
		} else if (mode == CAM2_FLASH_MODE_TORCH) {
			ret = aw36518_fled_mode_ctrl(
				AW36518_FLED_MODE_PRE_FLASH, 0);
			if (ret)
				err("%s torch flash on fail", __func__);
		} else {
			err("Invalid flash mode");
			ret = control_flash_gpio(flash->flash_gpio, 0);
			ret = -EINVAL;
		}
#else
		if (mode == CAM2_FLASH_MODE_OFF) {
			ret = control_flash_gpio(flash->flash_gpio, 0);
			if (ret)
				err("capture flash off fail");
		} else if (mode == CAM2_FLASH_MODE_SINGLE) {
			ret = control_flash_gpio(flash->flash_gpio, 0);
			if (ret)
				err("capture flash on fail");
		} else {
			err("Invalid flash mode");
			ret = -EINVAL;
			goto p_err;
		}
#endif
		break;
	default:
		err("err!!! Unknown CID(%#x)", ctrl->id);
		ret = -EINVAL;
		goto p_err;
	}

p_err:
	return ret;
}

long flash_aw36518_ioctl(struct v4l2_subdev *subdev, unsigned int cmd,
			 void *arg)
{
	int ret = 0;
	struct v4l2_control *ctrl;

	ctrl = (struct v4l2_control *)arg;
	switch (cmd) {
	case SENSOR_IOCTL_FLS_S_CTRL:
		ret = flash_aw36518_s_ctrl(subdev, ctrl);
		if (ret) {
			err("err!!! flash_gpio_s_ctrl failed(%d)", ret);
			goto p_err;
		}
		break;
	case SENSOR_IOCTL_FLS_G_CTRL:
		break;
	default:
		err("err!!! Unknown command(%#x)", cmd);
		ret = -EINVAL;
		goto p_err;
	}
p_err:
	return (long)ret;
}

static const struct v4l2_subdev_core_ops core_ops = {
	.init = flash_aw36518_init,
	.ioctl = flash_aw36518_ioctl,
};

static const struct v4l2_subdev_ops subdev_ops = {
	.core = &core_ops,
};

static int flash_aw36518_probe(struct device *dev, struct i2c_client *client)
{
	int ret = 0;
	struct is_core *core;
	struct v4l2_subdev *subdev_flash;
	struct is_device_sensor *device;
	struct is_flash *flash;
	struct device_node *dnode;
	const u32 *sensor_id_spec;
	u32 sensor_id_len;
	u32 sensor_id[IS_SENSOR_COUNT];
	int i;

	FIMC_BUG(!dev);

	probe_info("%s start\n", __func__);
	dnode = dev->of_node;

	core = is_get_is_core();
	if (!core) {
		probe_info("core device is not yet probed");
		ret = -EPROBE_DEFER;
		goto exit;
	}

	sensor_id_spec = of_get_property(dnode, "id", &sensor_id_len);
	if (!sensor_id_spec) {
		err("sensor_id num read is fail(%d)", ret);
		goto exit;
	}

	sensor_id_len /= (unsigned int)sizeof(*sensor_id_spec);

	ret = of_property_read_u32_array(dnode, "id", sensor_id, sensor_id_len);
	if (ret) {
		err("sensor_id read is fail(%d)", ret);
		goto exit;
	}

	for (i = 0; i < sensor_id_len; i++) {
		device = &core->sensor[sensor_id[i]];
		if (!device) {
			err("sensor device is NULL");
			ret = -EPROBE_DEFER;
			goto exit;
		}
	}

	flash = kzalloc(sizeof(struct is_flash) * sensor_id_len, GFP_KERNEL);
	if (!flash) {
		err("flash is NULL");
		ret = -ENOMEM;
		goto exit;
	}

	subdev_flash =
		kzalloc(sizeof(struct v4l2_subdev) * sensor_id_len, GFP_KERNEL);
	if (!subdev_flash) {
		err("subdev_flash is NULL");
		ret = -ENOMEM;
		kfree(flash);
		goto exit;
	}

	for (i = 0; i < sensor_id_len; i++) {
		probe_info("%s sensor_id %d\n", __func__, sensor_id[i]);
		flash[i].id = FLADRV_NAME_AW36518;
		flash[i].subdev = &subdev_flash[i];
		flash[i].client = client;

		flash[i].flash_gpio = of_get_named_gpio(dnode, "flash-gpio", 0);
		if (!gpio_is_valid(flash[i].flash_gpio)) {
			dev_err(dev, "failed to get FLASH_GPIO\n");
			kfree(subdev_flash);
			kfree(flash);
			ret = -EINVAL;
			goto exit;
		} else {
			gpio_request_one(flash[i].flash_gpio,
					 GPIOF_OUT_INIT_LOW,
					 "CAM_FLASH_OUTPUT");
			gpio_free(flash[i].flash_gpio);
		}

		flash[i].flash_data.mode = CAM2_FLASH_MODE_OFF;
		flash[i].flash_data.intensity =
			255; /* TODO: Need to figure out min/max range */
		flash[i].flash_data.firing_time_us =
			1 * 1000 * 1000; /* Max firing time is 1sec */

		device = &core->sensor[sensor_id[i]];
		device->subdev_flash = &subdev_flash[i];
		device->flash = &flash[i];

		if (client)
			v4l2_i2c_subdev_init(&subdev_flash[i], client,
						 &subdev_ops);
		else
			v4l2_subdev_init(&subdev_flash[i], &subdev_ops);

		v4l2_set_subdevdata(&subdev_flash[i], &flash[i]);
		v4l2_set_subdev_hostdata(&subdev_flash[i], device);
		snprintf(subdev_flash[i].name, V4L2_SUBDEV_NAME_SIZE,
			 "flash-subdev.%d", flash[i].id);

		probe_info("%s done\n", __func__);
	}
exit:
	return ret;
}

static int flash_aw36518_remove(struct platform_device *pdev)
{
	struct v4l2_subdev *subdev = NULL;
	struct is_flash *flash = NULL;

	subdev = platform_get_drvdata(pdev);
	FIMC_BUG(!subdev);

	flash = (struct is_flash *)v4l2_get_subdevdata(subdev);
	FIMC_BUG(!flash);

	aw36518_fled_mode_ctrl(AW36518_FLED_MODE_OFF, 0);
	platform_set_drvdata(pdev, NULL);

	kfree(subdev);
	kfree(flash);

	return 0;
}

static int flash_aw36518_platform_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct device *dev;

	FIMC_BUG(!pdev);

	dev = &pdev->dev;

	ret = flash_aw36518_probe(dev, NULL);
	if (ret < 0) {
		pr_err("flash gpio probe fail(%d)\n", ret);
		goto p_err;
	}

	pr_info("%s done\n", __func__);
p_err:
	return ret;
}

static const struct of_device_id exynos_is_sensor_flash_aw36518_match[] = {
	{
		.compatible = "samsung,sensor-flash-aw36518",
	},
	{},
};
MODULE_DEVICE_TABLE(of, exynos_is_sensor_flash_aw36518_match);

/* register platform driver */
static struct platform_driver
	sensor_flash_aw36518_platform_driver = {
		.probe = flash_aw36518_platform_probe,
		.remove = flash_aw36518_remove,
		.driver = {
			.name = "FIMC-IS-SENSOR-FLASH-AW36518-PLATFORM",
			.owner = THIS_MODULE,
			.of_match_table = exynos_is_sensor_flash_aw36518_match,
	}
};

#ifdef MODULE
static int flash_aw36518_module_init(void)
{
	int ret = 0;

	ret = platform_driver_register(&sensor_flash_aw36518_platform_driver);
	if (ret) {
		err("flash_aw36518 driver register failed(%d)", ret);
		return ret;
	}

	return ret;
}

static void flash_aw36518_module_exit(void)
{
	platform_driver_unregister(&sensor_flash_aw36518_platform_driver);
}
module_init(flash_aw36518_module_init);
module_exit(flash_aw36518_module_exit);
#else
static int __init is_sensor_flash_aw36518_init(void)
{
	int ret;

	ret = platform_driver_probe(&sensor_flash_aw36518_platform_driver,
					flash_aw36518_platform_probe);
	if (ret)
		err("failed to probe %s driver: %d\n",
			sensor_flash_aw36518_platform_driver.driver.name, ret);

	return ret;
}
late_initcall_sync(is_sensor_flash_aw36518_init);
#endif

MODULE_LICENSE("GPL");
MODULE_SOFTDEP("pre: fimc-is");
