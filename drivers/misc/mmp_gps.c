/*
 * Marvell gps driver
 *
 * Copyright (C) 2014 Marvell Technology Group Ltd.
 *
 * Authors: Yi Zhang <yizhang@marvell.com>
 *          Leilei Shang <shangll@marvell.com>
 *          Tim Wang <wangtt@marvell.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/gpio.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/delay.h>
#include <linux/regulator/machine.h>
#include <linux/platform_device.h>
#include <linux/pinctrl/consumer.h>

#define GPS_STATUS_LENS 16
struct mmp_gps_info {
	struct device *dev;
	struct regulator *gps_regulator;
	char chip_status[GPS_STATUS_LENS];
	int ldo_en_gpio;
	int reset_n_gpio;
	int on_off_gpio;
	int eclk_ctrl;
	int mfp_lpm:1;
	struct pinctrl *pinctrl;
	struct pinctrl_state *pin_lpm_drv_low;
	struct pinctrl_state *pin_lpm_drv_high;
};

static int mmp_gps_dt_init(struct device_node *np,
			   struct device *dev,
			   struct mmp_gps_info *info)
{
	/*
	 * ldo_en is not used as gpio control on some
	 * platform, like Eden. Egnore it if not set.
	 */
	info->ldo_en_gpio =
		of_get_named_gpio(dev->of_node, "ldo-en-gpio", 0);
	if (info->ldo_en_gpio < 0) {
		pr_warn("%s: of_get_named_gpio failed: %d\n", __func__,
		       info->ldo_en_gpio);
		info->ldo_en_gpio = 0;
	}


	if (of_get_property(np, "marvell,gps-eclk-ctrl", NULL))
		info->eclk_ctrl = 1;

	info->reset_n_gpio =
		of_get_named_gpio(dev->of_node, "reset-n-gpio", 0);
	if (info->reset_n_gpio < 0) {
		dev_err(dev, "%s: of_get_named_gpio failed: %d\n", __func__,
		       info->reset_n_gpio);
		goto out;
	}

	info->on_off_gpio =
		of_get_named_gpio(dev->of_node, "on-off-gpio", 0);
	if (info->on_off_gpio < 0) {
		dev_err(dev, "%s: of_get_named_gpio failed: %d\n", __func__,
		       info->on_off_gpio);
		goto out;
	}

	if (of_get_property(np, "marvell,mfp-lpm", NULL))
		info->mfp_lpm = 1;

	if (info->mfp_lpm) {
		/* Get gps lpm_drv_high pins status */
		info->pinctrl = devm_pinctrl_get(dev);
		if (IS_ERR(info->pinctrl)) {
			info->pinctrl = NULL;
			dev_warn(dev, "could not get gps pinctrl\n");
		} else {
			info->pin_lpm_drv_low =
					pinctrl_lookup_state(info->pinctrl, "lpm_drv_low");
			if (IS_ERR(info->pin_lpm_drv_low)) {
				dev_err(dev, "could not get gps lpm_drv_low pinstate\n");
				info->pin_lpm_drv_low = NULL;
			}

			info->pin_lpm_drv_high =
					pinctrl_lookup_state(info->pinctrl, "lpm_drv_high");
			if (IS_ERR(info->pin_lpm_drv_high)) {
				dev_err(dev, "could not get gps lpm_drv_high pinstate\n");
				info->pin_lpm_drv_high = NULL;
			}
		}
	}

	return 0;
out:
	return -EINVAL;
}

static void gps_eclk_ctrl(struct mmp_gps_info *info, int on)
{
	struct pinctrl *pinctrl;
	struct pinctrl_state *pins_state;
	int ret;

	if (!info->eclk_ctrl) {
		pr_warn("gps eclk ctrl not enable!\n");
		return;
	}

	pinctrl = devm_pinctrl_get(info->dev);
	if (IS_ERR(pinctrl)) {
		dev_err(info->dev, "gps pinctrl get fail!\n");
		return;
	}

	if (on) {
		pins_state = pinctrl_lookup_state(pinctrl, PINCTRL_STATE_DEFAULT);
		if (IS_ERR(pins_state)) {
			dev_err(info->dev, "gps pinctrl look up fail!\n");
			return;
		}
		ret = pinctrl_select_state(pinctrl, pins_state);
		if (ret) {
			dev_err(info->dev, "gps pinctrl set fail!\n");
			return;
		}
	} else {
		pins_state = pinctrl_lookup_state(pinctrl, PINCTRL_STATE_IDLE);
		if (IS_ERR(pins_state)) {
			dev_err(info->dev, "gps pinctrl look up fail!\n");
			return;
		}
		ret = pinctrl_select_state(pinctrl, pins_state);
		if (ret) {
			dev_err(info->dev, "gps pinctrl set fail!\n");
			return;
		}
	}
}

static void gps_ldo_control(struct mmp_gps_info *info, int enable)
{

	int ret;

	info->gps_regulator = regulator_get(info->dev, "vgps");
	if (IS_ERR(info->gps_regulator)) {
		dev_err(info->dev, "get gps ldo fail!\n");
		return;
	}

	if (enable) {
		regulator_set_voltage(info->gps_regulator, 1800000, 1800000);
		ret = regulator_enable(info->gps_regulator);
		if (ret)
			dev_err(info->dev, "enable gps ldo fail!\n");
	} else {
		ret = regulator_disable(info->gps_regulator);
		if (ret)
			dev_err(info->dev, "disable gps ldo fail!\n");
	}

	regulator_put(info->gps_regulator);
}

/* GPS: power on/off control */
static void gps_power_on(struct mmp_gps_info *info)
{

	gps_ldo_control(info, 1);
	gps_eclk_ctrl(info, 0);

	if (info->ldo_en_gpio)
		gpio_direction_output(info->ldo_en_gpio, 0);

	gpio_direction_output(info->reset_n_gpio, 0);
	usleep_range(1000, 1500);

	/* Confiure MFP LPM_DRIVE_LOW if reset pin output low when system active */
	if (info->mfp_lpm && info->pinctrl && info->pin_lpm_drv_low) {
		if (pinctrl_select_state(info->pinctrl, info->pin_lpm_drv_low))
			dev_err(info->dev, "could not set gps pins to lpm_drv_low states\n");
	}

	if (info->ldo_en_gpio)
		gpio_direction_output(info->ldo_en_gpio, 1);

	dev_info(info->dev, "gps chip powered on\n");

	return;
}

static void gps_power_off(struct mmp_gps_info *info)
{
	gps_eclk_ctrl(info, 0);

	if (info->ldo_en_gpio)
		gpio_direction_output(info->ldo_en_gpio, 0);

	gpio_direction_output(info->reset_n_gpio, 0);

	if (info->mfp_lpm && info->pinctrl && info->pin_lpm_drv_low) {
		if (pinctrl_select_state(info->pinctrl, info->pin_lpm_drv_low))
			dev_err(info->dev, "could not set gps pins to lpm_drv_low states\n");
	}

	if (info->ldo_en_gpio)
		gpio_direction_output(info->ldo_en_gpio, 0);
	gps_ldo_control(info, 0);

	dev_info(info->dev, "gps chip powered off\n");

	return;
}

static void gps_reset(struct mmp_gps_info *info, int flag)
{
	gpio_direction_output(info->reset_n_gpio, flag);

	if (info->mfp_lpm) {
		/* Confiure MFP LPM_DRIVE_HIGH if reset pin output high when system active */
		if (flag && info->pinctrl && info->pin_lpm_drv_high) {
			if (pinctrl_select_state(info->pinctrl, info->pin_lpm_drv_high))
				dev_err(info->dev, "could not set gps pins to lpm_drv_high state\n");
		} else if (info->pinctrl && info->pin_lpm_drv_low) {
			if (pinctrl_select_state(info->pinctrl, info->pin_lpm_drv_low))
				dev_err(info->dev, "could not set gps pins to lpm_drv_low states\n");
		}
	}
}

static void gps_on_off(struct mmp_gps_info *info, int flag)
{
	gpio_direction_output(info->on_off_gpio, flag);
}

static ssize_t gps_ctrl(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t count)
{
	static char msg[256];
	int flag, ret;


	struct mmp_gps_info *info = dev_get_drvdata(dev);

	count = (count > 255) ? 255 : count;
	memset(msg, 0, count);

	if (1 != sscanf(buf, "%s", info->chip_status))
		dev_warn(info->dev, "wrong cmd");
	if (!strncmp(buf, "off", 3)) {
		strncpy(info->chip_status, "off", 4);
		gps_power_off(info);
	} else if (!strncmp(buf, "on", 2)) {
		strncpy(info->chip_status, "on", 3);
		gps_power_on(info);
	} else if (!strncmp(buf, "reset", 5)) {
		strncpy(info->chip_status, "reset", 6);
		ret = sscanf(buf, "%s %d", msg, &flag);
		if (ret == 2)
			gps_reset(info, flag);
	} else if (!strncmp(buf, "wakeup", 6)) {
		strncpy(info->chip_status, "wakeup", 7);
		ret = sscanf(buf, "%s %d", msg, &flag);
		if (ret == 2)
			gps_on_off(info, flag);
	} else if (!strncmp(buf, "eclk", 4)) {
		strncpy(info->chip_status, "eclk", 5);
		ret = sscanf(buf, "%s %d", msg, &flag);
		if (ret == 2)
			gps_eclk_ctrl(info, flag);
	} else
		dev_info(info->dev, "usage wrong\n");

	return count;
}

static ssize_t gps_status(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct mmp_gps_info *info = dev_get_drvdata(dev);

	return sprintf(buf, "status: %s\n", info->chip_status);
}

static DEVICE_ATTR(ctrl, S_IWUSR, gps_status, gps_ctrl);
static DEVICE_ATTR(status, S_IRUSR, gps_status, NULL);

static const struct attribute *gps_attrs[] = {
	&dev_attr_ctrl.attr,
	&dev_attr_status.attr,
	NULL,
};

static const struct attribute_group gps_attr_group = {
	.attrs = (struct attribute **)gps_attrs,
};

static int mmp_gps_probe(struct platform_device *pdev)
{
	struct mmp_gps_info *info;

	struct device_node *np = pdev->dev.of_node;
	int ret;

	info = devm_kzalloc(&pdev->dev, sizeof(struct mmp_gps_info), GFP_KERNEL);
	if (!info)
		return -ENOMEM;

	if (IS_ENABLED(CONFIG_OF)) {
		ret = mmp_gps_dt_init(np, &pdev->dev, info);
		if (ret) {
			dev_err(&pdev->dev, "GPS probe failed!\n");
			return ret;
		}
	} else {
		dev_err(&pdev->dev, "GPS Not support DT, exit!\n");
	}

	strncpy(info->chip_status, "off", 4);
	info->dev = &pdev->dev;

	if (info->ldo_en_gpio) {
		ret = devm_gpio_request(info->dev,
				info->ldo_en_gpio, "gpio_gps_ldo");
		if (ret) {
			dev_err(info->dev,
				"request gpio %d failed\n", info->ldo_en_gpio);
			return ret;
		}
	}

	ret = devm_gpio_request(info->dev,
				info->reset_n_gpio, "gpio_gps_rst");
	if (ret) {
		dev_err(info->dev,
			"request gpio %d failed\n", info->reset_n_gpio);
		return ret;
	}

	ret = devm_gpio_request(info->dev,
				info->on_off_gpio, "gpio_gps_on");
	if (ret) {
		dev_err(info->dev,
			"request gpio %d failed\n", info->on_off_gpio);
		return ret;
	}

	platform_set_drvdata(pdev, info);

	ret = sysfs_create_group(&pdev->dev.kobj, &gps_attr_group);
	if (ret) {
		dev_err(&pdev->dev, "GPS create sysfs fail!\n");
		return ret;
	}

	return 0;
}

static int mmp_gps_remove(struct platform_device *pdev)
{
	sysfs_remove_group(&pdev->dev.kobj, &gps_attr_group);
	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id mmp_gps_dt_match[] = {
	{ .compatible = "marvell,mmp-gps", },
	{},
};
#endif

static struct platform_driver mmp_gps_driver = {
	.driver		= {
		.name	= "mmp-gps",
		.owner	= THIS_MODULE,
		.of_match_table = of_match_ptr(mmp_gps_dt_match),
	},
	.probe		= mmp_gps_probe,
	.remove		= mmp_gps_remove,
};

static int mmp_gps_init(void)
{
	return platform_driver_register(&mmp_gps_driver);
}

static void mmp_gps_exit(void)
{
	platform_driver_unregister(&mmp_gps_driver);
}
module_init(mmp_gps_init);
module_exit(mmp_gps_exit);

MODULE_AUTHOR("Yi Zhang <yizhang@marvell.com>");
MODULE_DESCRIPTION("driver for Marvell GPS solution");
MODULE_LICENSE("GPL v2");
