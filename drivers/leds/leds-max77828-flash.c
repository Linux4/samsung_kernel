/*
 * Flash-led driver for Maxim MAX77828
 *
 * Copyright (C) 2014 Kyoungho Yun <kyoungho.yun@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/leds.h>
#include <linux/err.h>
#include <linux/of.h>
#include <linux/mfd/max77828.h>
#include <linux/mfd/max77828-private.h>
#include <linux/leds-max77828-flash.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/regmap.h>
#include <linux/sec_sysfs.h>
#include <linux/i2c.h>
#include <linux/gpio.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/device.h>
#include <linux/of_gpio.h>

/* Registers */
#define MAX77828_REG_STATUS1		0x02
#define MAX77828_REG_STATUS2		0x03
#define MAX77828_REG_IFLASH1		0x04
#define MAX77828_REG_ITORCH1		0x05
#define MAX77828_REG_MODE_SEL		0x06
#define MAX77828_REG_FLASH_RAMP_SEL	0x07
#define MAX77828_REG_TORCH_RAMP_SEL	0x08
#define MAX77828_REG_FLASH_TMR_CNTL	0x09
#define MAX77828_REG_TORCH_TMR_CNTL	0x0A
#define MAX77828_REG_MAXFLASH1		0x0B
#define MAX77828_REG_MAXFLASH2		0x0C
#define MAX77828_REG_MAXFLASH3		0x0D
#define MAX77828_REG_DCDC_CNTL1		0x0E
#define MAX77828_REG_DCDC_CNTL2		0x0F
#define MAX77828_REG_DCDC_ILIM		0x10
#define MAX77828_REG_DCDC_OUT		0x11
#define MAX77828_REG_DCDC_OUT_MAX	0x12

/* MAX77828_REG_STATUS1 */
#define MAX77828_LED1_SHORT		0x80
#define MAX77828_IN_UVLO_THERM		0x10
#define MAX77828_OVP_A			0x02
#define MAX77828_OVP_D			0x01

/* MAX77828_REG_STATUS2 */
#define MAX77828_MAXFLASH		0x80
#define MAX77828_DONE			0x40
#define MAX77828_FLASH_TMR_S		0x08
#define MAX77828_TORCH_TMR_S		0x04
#define MAX77828_ILIM			0x02
#define MAX77828_NRESET			0x01

/* MAX77828_REG_IFLASH1 */
#define MAX77828_FLASH1_EN		0x80
#define MAX77828_FLASH1			0x3F

/* MAX77828_REG_ITORCH1 */
#define MAX77828_TORCH1_EN		0x80
#define MAX77828_TORCH1			0x7E
#define MAX77828_TORCH1_DIM		0x01

/* MAX77828_REG_MODE_SEL */
#define MAX77828_TORCHEN_PD		0x80
#define MAX77828_FLASHSTB_PD		0x40
#define MAX77828_TORCH_MODE		0x38//ok
#define MAX77828_TORCH_MD_DISABLED	0x00
#define MAX77828_TORCH_MD_TORCHEN	0x08//ok
#define MAX77828_TORCH_MD_FLASHSTB	0x10
#define MAX77828_TORCH_MD_ANY		0x18//ok
#define MAX77828_TORCH_MD_BOTH		0x20
#define MAX77828_TORCH_MD_ENABLED	0x21
#define MAX77828_FLASH_MODE		0x07
#define MAX77828_FLASH_MD_DISABLED	0x00
#define MAX77828_FLASH_MD_TORCHEN	0x01
#define MAX77828_FLASH_MD_FLASHSTB	0x02
#define MAX77828_FLASH_MD_ANY		0x03
#define MAX77828_FLASH_MD_BOTH		0x04
#define MAX77828_FLASH_MD_ENABLED	0x05

/* MAX77828_REG_FLASH_RAMP_SEL */
#define MAX77828_FLASH_RU		0x70
#define MAX77828_FLASH_RD		0x07

/* MAX77828_REG_TORCH_RAMP_SEL */
#define MAX77828_TORCH_RU		0x70
#define MAX77828_TORCH_RD		0x07

/* MAX77828_REG_FLASH_TMR_CNTL */
#define MAX77828_FLASH_TMR_CNTL		0x80
#define MAX77828_FLASH_TMR		0x7F

/* MAX77828_REG_TORCH_TMR_CNTL */
#define MAX77828_TORCH_TMR_CNTL		0x80
#define MAX77828_TORCH_TMR		0x7C

/* MAX77828_REG_MAXFLASH1 */
#define MAX77828_MAXFLASH_HYS		0xE0
#define MAX77828_MAXFLASH_TH		0x1F

/* MAX77828_REG_MAXFLASH2 */
#define MAX77828_LB_TMR_R		0xF0
#define MAX77828_LB_TMR_F		0x0F

/* MAX77828_REG_MAXFLASH3 */
#define MAX77828_MAX_FLASH1_IMIN	0x3F

/* MAX77828_REG_DCDC_CNTL1 */
#define MAX77828_OVP_TH			0xC0
#define MAX77828_FREQ_PWM		0x0C
#define MAX77828_DCDC_MODE		0x03

/* MAX77828_REG_DCDC_CNTL2 */
#define MAX77828_DCDC_ADPT_REG		0xC0
#define MAX77828_DCDC_GAIN		0x20
#define MAX77828_DCDC_OPERATION		0x1C

/* MAX77828_REG_DCDC_LIM */
#define MAX77828_DCDC_ILIM		0xC0
#define MAX77828_DCDC_SS		0x3F

/* MAX77828_REG_DCDC_OUT */
#define MAX77828_DCDC_OUT		0xFF

/* MAX77828_REG_DCDC_OUT_MAX */
#define MAX77828_DCDC_OUT_MAX		0xFF

extern struct class *camera_class; /*sys/class/camera*/

struct max77828_flash
{
	struct led_classdev led_flash;
	struct led_classdev led_torch;
	struct max77828 *max77828;
	struct max77828_flash_data *flash_data;
	//bool flash_en_control;
	//bool torch_en_control;
};

const struct regmap_config max77828_regmap_config =
{
	.reg_bits = 8,
	.val_bits = 8,
};

static void max77828_flash_set(struct led_classdev *led_cdev, enum led_brightness brightness)
{
	struct max77828_flash *max77828_flash = container_of(led_cdev, struct max77828_flash, led_flash);
	struct regmap *regmap = max77828_flash->max77828->regmap;
	struct device *dev = max77828_flash->led_flash.dev;
	int ret;

	if (brightness == LED_OFF)
	{
		/* Flash OFF */
		ret = regmap_update_bits(regmap, MAX77828_REG_IFLASH1,
				MAX77828_FLASH1_EN, 0);
		if (IS_ERR_VALUE(ret))
			dev_err(dev, "can't write IFLASH1 : %d\n", ret);
	}
	else
	{
		/* Flash ON, Set current */
		ret = regmap_write(regmap, MAX77828_REG_IFLASH1,
				MAX77828_FLASH1_EN |
				((unsigned int)brightness << M2SH(MAX77828_FLASH1)));
		if (IS_ERR_VALUE(ret))
			dev_err(dev, "can't write IFLASH1 : %d\n", ret);
	}
}

static enum led_brightness max77828_flash_get(struct led_classdev *led_cdev)
{
	struct max77828_flash *max77828_flash = container_of(led_cdev, struct max77828_flash, led_flash);
	struct regmap *regmap = max77828_flash->max77828->regmap;
	struct device *dev = max77828_flash->led_flash.dev;
	unsigned int value;
	int ret;

	/* Get status */
	ret = regmap_read(regmap, MAX77828_REG_IFLASH1, &value);
	if (IS_ERR_VALUE(ret))
		dev_err(dev, "can't read IFLASH1 : %d\n", ret);

	if (!(value & MAX77828_FLASH1_EN))
		return LED_OFF;

	/* Get current */
	return (enum led_brightness)((value & MAX77828_FLASH1) >> M2SH(MAX77828_FLASH1));
}

static void max77828_torch_set(struct led_classdev *led_cdev, enum led_brightness brightness)
{
	struct max77828_flash *max77828_flash = container_of(led_cdev, struct max77828_flash, led_torch);
	struct regmap *regmap = max77828_flash->max77828->regmap;
	struct device *dev = max77828_flash->led_torch.dev;
	int ret;

	if (brightness == LED_OFF)
	{
		/* Flash OFF */
		ret = regmap_update_bits(regmap, MAX77828_REG_ITORCH1,
				MAX77828_TORCH1_EN, 0);
		if (IS_ERR_VALUE(ret))
			dev_err(dev, "can't write ITORCH1 : %d\n", ret);
	}
	else
	{
		/* Flash ON, Set current */
		ret = regmap_write(regmap, MAX77828_REG_ITORCH1,
				MAX77828_TORCH1_EN |
				((unsigned int)brightness << M2SH(MAX77828_TORCH1)));
		if (IS_ERR_VALUE(ret))
			dev_err(dev, "can't write ITORCH1 : %d\n", ret);
	}

}

static enum led_brightness max77828_torch_get(struct led_classdev *led_cdev)
{
	struct max77828_flash *max77828_flash = container_of(led_cdev, struct max77828_flash, led_torch);
	struct regmap *regmap = max77828_flash->max77828->regmap;
	struct device *dev = max77828_flash->led_torch.dev;
	unsigned int value;
	int ret;

	/* Get status */
	ret = regmap_read(regmap, MAX77828_REG_ITORCH1, &value);
	if (IS_ERR_VALUE(ret))
		dev_err(dev, "can't read ITORCH1 : %d\n", ret);

	if (!(value & MAX77828_TORCH1_EN))
		return LED_OFF;

	/* Get current */
	return (enum led_brightness)((value & MAX77828_TORCH1) >> M2SH(MAX77828_TORCH1));
}

static int get_register(struct regmap *regmap, unsigned int reg)
{
	int ret;
	unsigned int value;

	ret = regmap_read(regmap, reg, &value);
	if (IS_ERR_VALUE(ret))
		printk(" max77828 can't read 0x%02x : %d\n", reg, ret);
	else
		printk(" max77828 register val 0x%02x : 0x%02x\n", reg, value);

	return 0;
}

static ssize_t max77828_flash(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct max77828_flash *max77828_flash = container_of(led_cdev, struct max77828_flash, led_torch);
	struct regmap *regmap = max77828_flash->max77828->regmap;
	int ret, brightness;

	ret = kstrtouint(buf, 10, &brightness);

	if (brightness == LED_OFF) {
		/* Flash OFF */
		ret = regmap_update_bits(regmap, MAX77828_REG_ITORCH1,
				MAX77828_TORCH1_EN, 0);
		if (IS_ERR_VALUE(ret))
			dev_err(dev, "max77828 can't write ITORCH1 : %d\n", ret);
	} else {
		/* Flash ON, Set current */
		get_register(regmap, MAX77828_REG_STATUS1);
		regmap_write(regmap, MAX77828_REG_MODE_SEL, MAX77828_TORCH_MD_TORCHEN);
		ret = regmap_write(regmap, MAX77828_REG_ITORCH1,
				MAX77828_TORCH1_EN |
				((unsigned int)brightness << M2SH(MAX77828_TORCH1)));
		if (IS_ERR_VALUE(ret))
			dev_err(dev, "max77828 can't write ITORCH1 : %d\n", ret);
	}

	return size;
}

static DEVICE_ATTR(rear_flash, S_IWUSR|S_IWGRP, NULL, max77828_flash);

static int max77828_flash_hw_init(struct max77828 *max77828, struct max77828_flash_platform_data *pdata)
{
	const int flash_ramp[] = {384, 640, 1152, 2176, 4224, 8320, 16512, 32896};
	const int torch_ramp[] = {16392, 32776, 65544, 131080, 262152, 524296, 1048000, 2097000};
	struct regmap *regmap = max77828->regmap;
	unsigned int value;
	int i, ret = 0;

	/* pin control setting */
	ret = regmap_write(regmap, MAX77828_REG_MODE_SEL,
			(pdata->enable_pulldown_resistor ? (MAX77828_TORCHEN_PD | MAX77828_FLASHSTB_PD) : 0) |
			(pdata->flash_en_control ? MAX77828_FLASH_MD_FLASHSTB : MAX77828_FLASH_MD_ENABLED) |
			(pdata->torch_stb_control ?  MAX77828_TORCH_MD_TORCHEN : MAX77828_TORCH_MD_ENABLED));
	if (IS_ERR_VALUE(ret))
		return ret;

	/* ramp up/down setting */
	for (i = 0; i < ARRAY_SIZE(flash_ramp); i++)
		if (pdata->flash_ramp_up <= flash_ramp[i])
			break;
	if (unlikely(i == ARRAY_SIZE(flash_ramp)))
		return -EINVAL;
	value = (unsigned int)i << M2SH(MAX77828_FLASH_RU);

	for (i = 0; i < ARRAY_SIZE(flash_ramp); i++)
		if (pdata->flash_ramp_down <= flash_ramp[i])
			break;
	if (unlikely(i == ARRAY_SIZE(flash_ramp)))
		return -EINVAL;
	value |= (unsigned int)i << M2SH(MAX77828_FLASH_RD);

	ret = regmap_write(regmap, MAX77828_REG_FLASH_RAMP_SEL, value);
	if (IS_ERR_VALUE(ret))
		return ret;

	for (i = 0; i < ARRAY_SIZE(torch_ramp); i++)
		if (pdata->flash_ramp_up <= torch_ramp[i])
			break;
	if (unlikely(i == ARRAY_SIZE(torch_ramp)))
		return -EINVAL;
	value = (unsigned int)i << M2SH(MAX77828_TORCH_RU);

	for (i = 0; i < ARRAY_SIZE(torch_ramp); i++)
		if (pdata->flash_ramp_down <= torch_ramp[i])
			break;
	if (unlikely(i == ARRAY_SIZE(torch_ramp)))
		return -EINVAL;
	value |= (unsigned int)i << M2SH(MAX77828_TORCH_RD);

	ret = regmap_write(regmap, MAX77828_REG_TORCH_RAMP_SEL, value);
	if (IS_ERR_VALUE(ret))
		return ret;

	/* flash timer control */
	value = MAX77828_FLASH_TMR_CNTL | MAX77828_FLASH_TMR;
	ret = regmap_write(regmap, MAX77828_REG_FLASH_TMR_CNTL, value);
	if (IS_ERR_VALUE(ret))
		return ret;

	/* torch timer control */
	value = MAX77828_TORCH_TMR_CNTL | MAX77828_TORCH_TMR;
	ret = regmap_write(regmap, MAX77828_REG_TORCH_TMR_CNTL, value);
	if (IS_ERR_VALUE(ret))
		return ret;

	/* MAXFLASH setting */
	if (pdata->enable_maxflash)
	{
		if (pdata->maxflash_rising_timer > 4096)
			return -EINVAL;
		value = ((pdata->maxflash_rising_timer / 256 - 1) << M2SH(MAX77828_LB_TMR_R)) |
				((pdata->maxflash_falling_timer / 256 - 1) << M2SH(MAX77828_LB_TMR_F));
		ret = regmap_write(regmap, MAX77828_REG_MAXFLASH1, value);
		if (IS_ERR_VALUE(ret))
			return ret;

		switch(pdata->maxflash_hysteresis)
		{
		case 0:
			value = 0;
			break;
		case 50000:
			value = 1;
			break;
		case 100000:
			value = 2;
			break;
		case 300000:
			value = 6;
			break;
		case 350000:
			value = 7;
			break;
		default:
			return -EINVAL;
		}

		if (likely(pdata->maxflash_threshold >= 2400000 && pdata->maxflash_threshold <= 3400000))
			value |= (((pdata->maxflash_threshold - 2400000) / 33000 + 1) << M2SH(MAX77828_MAXFLASH_HYS));
		else
			return -EINVAL;
	}
	else
		value = 0;

	ret = regmap_write(regmap, MAX77828_REG_MAXFLASH1, value);
	if (IS_ERR_VALUE(ret))
		return ret;

	return ret;
}

#ifdef CONFIG_OF
static struct max77828_flash_platform_data *max77828_flash_parse_dt(struct device *dev)
{
	struct max77828_flash_platform_data *pdata;
	struct device_node *nproot = dev->parent->of_node;
	struct device_node *np;


	pdata = devm_kzalloc(dev, sizeof(*pdata), GFP_KERNEL);
	if (unlikely(pdata == NULL))
		return ERR_PTR(-ENOMEM);

	np = of_find_node_by_name(nproot, "flash");
	if (unlikely(np == NULL)) {
		pr_err("max77828 flash node not found\n");
		devm_kfree(dev, pdata);
		return ERR_PTR(-EINVAL);
	}

	pdata->flash_en_control = of_property_read_bool(np, "flash-en-control");
	pdata->torch_stb_control = of_property_read_bool(np, "torch-en-control");
	pdata->enable_maxflash = of_property_read_bool(np, "enable-maxflash");
	pdata->enable_pulldown_resistor = of_property_read_bool(np, "enable-pulldown-resistor");
	pdata->np = np;

	if (!of_property_read_u32(np, "flash_ramp_up", &pdata->flash_ramp_up))
		pdata->flash_ramp_up = 384;

	if (!of_property_read_u32(np, "flash_ramp_down", &pdata->flash_ramp_down))
		pdata->flash_ramp_down = 384;

	if (!of_property_read_u32(np, "torch_ramp_up", &pdata->torch_ramp_up))
		pdata->torch_ramp_up = 16392;

	if (!of_property_read_u32(np, "torch_ramp_down", &pdata->torch_ramp_down))
		pdata->torch_ramp_down = 16392;

	if (!of_property_read_u32(np, "maxflash-threshold", &pdata->maxflash_threshold))
		pdata->maxflash_threshold = 2400000;

	if (!of_property_read_u32(np, "maxflash-hysteresis", &pdata->maxflash_hysteresis))
		pdata->maxflash_hysteresis = 50000;

	if (!of_property_read_u32(np, "maxflash-falling-timer", &pdata->maxflash_falling_timer))
		pdata->maxflash_falling_timer = 256;

	if (!of_property_read_u32(np, "maxflash-rising-timer", &pdata->maxflash_rising_timer))
		pdata->maxflash_rising_timer = 256;

	return pdata;
}
#endif

int flash_classdev_register(struct device *parent, struct led_classdev *led_cdev)
{
	if (camera_class == NULL) {
		camera_class = class_create(THIS_MODULE, "camera2");
		if (IS_ERR(camera_class)) {
			pr_err(" Failed to create class(camera)!\n");
			return PTR_ERR(camera_class);
		}
	}

	led_cdev->dev = device_create(camera_class, parent, 0, led_cdev, "flash");
	if (IS_ERR(led_cdev->dev))
		return PTR_ERR(led_cdev->dev);

	dev_dbg(parent, " Registered led device: %s\n",
			led_cdev->name);

	return 0;
}

void flash_classdev_unregister(void)
{
	if (camera_class != NULL) {
		device_destroy(camera_class, 0);
		class_destroy(camera_class);
	}
}

static int max77828_flash_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct max77828_flash_platform_data *pdata;
	struct max77828_flash *max77828_flash;
	struct max77828 *max77828;
	struct max77828_dev *max77828_dev = dev_get_drvdata(dev->parent);
	struct i2c_client *client = NULL;
	int ret = 0;

#ifdef CONFIG_OF
	pdata = max77828_flash_parse_dt(dev);
	if (unlikely(IS_ERR(pdata)))
		return PTR_ERR(pdata);
#else
	pdata = dev_get_platdata(dev);
#endif

	max77828 = devm_kzalloc(dev, sizeof(struct max77828), GFP_KERNEL);
	if (unlikely(!max77828)) {
		ret = -ENOMEM;
		goto out;
	}

	client = max77828_dev->led;
	max77828->regmap = devm_regmap_init_i2c(client, &max77828_regmap_config);
	if (IS_ERR(max77828->regmap)) {
		ret = -ENOMEM;
		goto out_max77828;
	}

	max77828_flash = devm_kzalloc(dev, sizeof(struct max77828_flash), GFP_KERNEL);
	if (unlikely(!max77828_flash)) {
		ret = -ENOMEM;
		goto out_max77828;
	}

	max77828_flash->max77828 = max77828;

	// initial setup
	ret = max77828_flash_hw_init(max77828, pdata);
	if (IS_ERR_VALUE(ret))
	{
		dev_err(&pdev->dev, "hw init fail : %d\n", ret);
		ret = -ENOMEM;
		goto out_max77828_flash;
	}

	max77828_flash->led_flash.name = "max77828:white:flash";
	max77828_flash->led_flash.default_trigger = "flash";
	max77828_flash->led_flash.brightness_set = max77828_flash_set;
	max77828_flash->led_flash.brightness_get = max77828_flash_get;
	max77828_flash->led_flash.max_brightness = MAX77828_FLASH1 >> M2SH(MAX77828_FLASH1);
	max77828_flash->led_torch.name = "max77828:white:torch";
	max77828_flash->led_torch.default_trigger = "torch";
	max77828_flash->led_torch.brightness_set = max77828_torch_set;
	max77828_flash->led_torch.brightness_get = max77828_torch_get;
	max77828_flash->led_torch.max_brightness = MAX77828_TORCH1 >> M2SH(MAX77828_TORCH1);

	ret = flash_classdev_register(dev, &max77828_flash->led_torch);
	if (IS_ERR_VALUE(ret))
	{
		dev_err(dev, " max77828 unable to register TORCH : %d\n", ret);
		goto out_max77828_flash;
	}

	if (device_create_file(max77828_flash->led_torch.dev, &dev_attr_rear_flash) < 0) {
		dev_err(dev, " max77828 failed to create device file , %s\n",
				dev_attr_rear_flash.attr.name);
		goto out_unregister_flash;
	}

	platform_set_drvdata(pdev, max77828_flash);
#ifdef CONFIG_OF
	devm_kfree(dev, pdata);
#endif
	return 0;

out_unregister_flash:
	flash_classdev_unregister();
out_max77828_flash:
	devm_kfree(dev, max77828_flash);
out_max77828:
	devm_kfree(dev, max77828);
out:
#ifdef CONFIG_OF
	devm_kfree(dev, pdata);
#endif
	return ret;
}

static int __devexit max77828_flash_remove(struct platform_device *pdev)
{
	struct max77828_flash *max77828_flash = platform_get_drvdata(pdev);
	struct device *dev = &pdev->dev;

	device_remove_file(max77828_flash->led_torch.dev, &dev_attr_rear_flash);
	flash_classdev_unregister();
	devm_kfree(dev, max77828_flash);
	devm_kfree(dev, max77828_flash->max77828);

	return 0;
}

void max77828_flash_shutdown(struct device *dev)
{
}

static struct platform_driver max77828_flash_driver = {
	.driver		= {
		.name	= "max77828-flash",
		.owner	= THIS_MODULE,
		.shutdown = max77828_flash_shutdown,
	},
	.probe		= max77828_flash_probe,
	.remove		= __devexit_p(max77828_flash_remove),
};


static int __init max77828_flash_init(void)
{
	return platform_driver_register(&max77828_flash_driver);
}
module_init(max77828_flash_init);

static void __exit max77828_flash_exit(void)
{
	platform_driver_unregister(&max77828_flash_driver);
}
module_exit(max77828_flash_exit);

MODULE_ALIAS("platform:max77828-flash");
MODULE_AUTHOR("Kyoungho Yun<kyoungho.yun@samsung.com>");
MODULE_DESCRIPTION("MAX77828 FLED driver");
MODULE_LICENSE("GPL v2");
MODULE_VERSION("1.0");
