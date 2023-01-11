/*
 * Simple driver for Texas Instruments LM3632 Backlight + Flash LED driver chip
 * Copyright (C) 2012 Texas Instruments
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/leds.h>
#include <linux/backlight.h>
#include <linux/err.h>
#include <linux/delay.h>
#include <linux/uaccess.h>
#include <linux/interrupt.h>
#include <linux/regmap.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/gpio.h>
#include <linux/platform_data/lm3632_bl.h>

#define DEV_NAME ("lm3632")

#define REG_DEV_ID	(0x01)
#define REG_BL_CONF_1	(0x02)
#define REG_BL_CONF_2	(0x03)
#define REG_BL_CONF_3	(0x04)
#define REG_BL_CONF_4	(0x05)
#define REG_FL_CONF_1	(0x06)
#define REG_FL_CONF_2	(0x07)
#define REG_FL_CONF_3	(0x08)
#define REG_IO_CTRL	(0x09)
#define REG_ENABLE	(0x0A)
#define REG_FLAG	(0x0B)
#define REG_BIAS_CONF_1	(0x0C)
#define REG_BIAS_CONF_2	(0x0D)
#define REG_BIAS_CONF_3	(0x0E)
#define REG_BIAS_CONF_4	(0x0F)
#define REG_FLAG2	(0x10)
#define REG_MAX		(REG_FLAG2)

extern struct class *camera_class;
extern struct device *flash_dev;
struct lm3632_chip_data *g_pchip;

struct lm3632_chip_data {
	struct device *dev;

	struct backlight_device *bled;
	struct led_classdev cdev_flash;
	struct led_classdev cdev_torch;
	struct regmap *regmap;

	unsigned int bled_mode;
	unsigned int bled_map;
	unsigned int last_flag;
};

int blic_on;
int switching_freq = 1000000;
int lcd_strobe_on;

int lcd_tx;
unsigned int bl_current = 18000;

static const struct regmap_config lm3632_regmap = {
	.reg_bits = 8,
	.val_bits = 8,
	.max_register = REG_MAX,
};

static void lm3632_set_current(struct lm3632_chip_data *pchip,
		unsigned int max_current)
{
	unsigned int msb, lsb, value;

	if (max_current > 49900) {
		pr_warn("%s, invalid current %d\n", __func__, max_current);
		return;
	}

	value = (((max_current * 10000) - 378055) / 121945) + 1;
	msb = (value & 0x7F8) >> 3;
	lsb = (value & 0x07);
	regmap_write(pchip->regmap, REG_BL_CONF_3, lsb);
	regmap_write(pchip->regmap, REG_BL_CONF_4, msb);

	pr_info("%s, max_current %d, msb:0x%02X, lsb:0x%02X\n",
			__func__, max_current, msb, lsb);
}

static int lm3632_parse_dt(struct device_node *np)
{
	int ret, val;
	lcd_strobe_on =  of_get_named_gpio(np, "lcd_strobe_on-gpio", 0);
	if (lcd_strobe_on < 0) {
		pr_err("%s: of_get_named_gpio failed\n", __func__);
		return -EINVAL;
	}
	blic_on = of_get_named_gpio(np, "blic_on-gpio", 0);
	if (blic_on < 0) {
		pr_err("%s: of_get_named_gpio failed\n", __func__);
		return -EINVAL;
	}
	lcd_tx =  of_get_named_gpio(np, "lcd_tx-gpio", 0);
	if (lcd_tx < 0) {
		pr_err("%s: of_get_named_gpio failed\n", __func__);
		return -EINVAL;
	}

	if (gpio_request(lcd_strobe_on, "lm3632_lcd_strobe_on")) {
		pr_err("gpio %d request failed\n", lcd_strobe_on);
		return -EINVAL;
	}
	gpio_direction_output(lcd_strobe_on, 0);
	if (gpio_request(lcd_tx, "lm3632_lcd_tx")) {
		pr_err("gpio %d request failed\n", lcd_tx);
		return -EINVAL;
	}
	gpio_direction_output(lcd_tx, 0);
	if (gpio_request(blic_on, "lm3632_blic_on")) {
		pr_err("gpio %d request failed\n", blic_on);
		return -EINVAL;
	}

	ret = of_property_read_u32(np, "switching-freq", &val);
	if (!ret) {
		if (val == 500000 || val == 1000000) {
			switching_freq = val;
			pr_info("%s, switching_freq %d\n",
					__func__, switching_freq);
		}
	}

	ret = of_property_read_u32(np, "bl-current", &val);
	if (!ret) {
		bl_current = val;
		pr_info("%s, current %d\n", __func__, bl_current);
	}

	return 0;
}

static void lm3632_fl_torch(struct led_classdev *cdev, enum led_brightness value);

static ssize_t flash_store(struct device *dev, struct device_attribute *attr,
		const char *buf, size_t count)
{
	int i, nValue=0, ret =0;
	struct lm3632_chip_data *pchip = dev_get_drvdata(dev);

	for(i = 0; i < count; i++) {
		if(buf[i] < '0' || buf[i] > '9')
			break;
		nValue = nValue * 10 + (buf[i] - '0');
	}

	switch(nValue) {
		case 0:
			pr_err("Torch Off\n");
			ledtrig_ftorch_ctrl(0);
			break;

		case 1:
			pr_err("Torch High\n");
			ledtrig_ftorch_ctrl(1);
			break;

		case 100:
			pr_err("Torch F-High\n");
			ledtrig_ftorch_ctrl(1);
			break;

		default:
			pr_err("Torch NF\n");
			break;
	}
	return count;
}

static ssize_t bl_current_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct lm3632_chip_data *pchip = g_pchip;
	unsigned int LSB, MSB;

	regmap_read(pchip->regmap, REG_BL_CONF_3, &LSB);
	regmap_read(pchip->regmap, REG_BL_CONF_4, &MSB);

	return	sprintf(buf, "current %d, msb:0x%02X, lsb:0x%02X\n",
			(((((MSB << 3) | LSB) - 1) * 121945) + 378055) / 10000, MSB, LSB);
}

static ssize_t bl_current_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct lm3632_chip_data *pchip = g_pchip;
	unsigned int value;

	if (kstrtoul(buf, 0, (unsigned long *)&value))
		return -EINVAL;

	if (value > 49900) {
		pr_warn("%s, invalid current %u\n", __func__, value);
		return -EINVAL;
	}

	lm3632_set_current(pchip, value);
	return size;
}

static DEVICE_ATTR(front_flash, S_IWUSR|S_IWGRP, NULL, flash_store);
static DEVICE_ATTR(bl_current, 0644, bl_current_show, bl_current_store);

void lm3632_bl_sysfs_init(struct device *dev)
{
	device_create_file(dev, &dev_attr_bl_current);
}

int create_front_flash_sysfs(void)
{
    int err = -ENODEV;

    if (IS_ERR_OR_NULL(camera_class)) {
        pr_err("flash_sysfs: error, camera class not exist");
        return -ENODEV;
    }

    if (IS_ERR_OR_NULL(flash_dev)) {
        pr_err("flash_sysfs: error, flash dev not exist");
        return -ENODEV;
    }

    err = device_create_file(flash_dev, &dev_attr_front_flash);
    if (unlikely(err < 0)) {
        pr_err("flash_sysfs: failed to create device file, %s\n",
                dev_attr_front_flash.attr.name);
    }
    return 0;
}

static int lm3632_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	struct lm3632_chip_data *pchip;
	struct device_node *node = client->dev.of_node;
	unsigned int rev;
	int ret;

	pr_info("%s enter\n", __func__);
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		dev_err(&client->dev, "i2c functionality check fail.\n");
		return -EOPNOTSUPP;
	}

	pchip = devm_kzalloc(&client->dev,
			sizeof(struct lm3632_chip_data), GFP_KERNEL);
	if (unlikely(!pchip))
		return -ENOMEM;

	pchip->dev = &client->dev;

	pchip->regmap = devm_regmap_init_i2c(client, &lm3632_regmap);
	if (IS_ERR(pchip->regmap)) {
		ret = PTR_ERR(pchip->regmap);
		dev_err(&client->dev, "fail : allocate register map: %d\n",
				ret);
		return ret;
	}
	i2c_set_clientdata(client, pchip);
	lm3632_parse_dt(node);

	ret = regmap_read(pchip->regmap, REG_DEV_ID, &rev);
	if (unlikely(ret)) {
		dev_err(&client->dev, "%s not found\n", DEV_NAME);
		return ret;
	}

	pr_info("%s detected (rev %u)\n", DEV_NAME, rev);

	/* for Front flash register */
	pchip->cdev_torch.name = "ftorch";
	pchip->cdev_torch.brightness_set = lm3632_fl_torch;
	/* for camera trigger */
	pchip->cdev_torch.default_trigger = pchip->cdev_torch.name;

	ret = led_classdev_register(pchip->dev, &pchip->cdev_torch);
	if (ret < 0) {
		printk( "Failed to register LED: %d\n", ret);
		return ret;
	}
	g_pchip = pchip;

	return 0;

err_out:
	return ret;
}

static int lm3632_remove(struct i2c_client *client)
{
	struct lm3632_chip_data *pchip = i2c_get_clientdata(client);

	regmap_write(pchip->regmap, REG_ENABLE, 0x00);
	gpio_direction_output(blic_on, 0);

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int lm3632_suspend(struct device *dev)
{
	struct lm3632_chip_data *pchip = dev_get_drvdata(dev);

	regmap_write(pchip->regmap, REG_ENABLE, 0x00);
	gpio_direction_output(blic_on, 0);

	return 0;
}

static int lm3632_resume(struct device *dev)
{
	struct lm3632_chip_data *pchip = dev_get_drvdata(dev);
	unsigned int rev;
	int ret;

	gpio_direction_output(blic_on, 1);

	ret = regmap_read(pchip->regmap, REG_DEV_ID, &rev);
	if (unlikely(ret))
		pr_warn("%s not found (rev %u)\n", DEV_NAME, rev);

	/* LM3632 LCD Bias Control */
	regmap_write(pchip->regmap, REG_BIAS_CONF_2, 0x1E);
	regmap_write(pchip->regmap, REG_BIAS_CONF_3, 0x1E);
	regmap_write(pchip->regmap, REG_BIAS_CONF_4, 0x1E);
	regmap_write(pchip->regmap, REG_BIAS_CONF_1, 0x1F);

	/* LM3632 Backlight Control */
	regmap_write(pchip->regmap, REG_IO_CTRL, 0x41);
	regmap_write(pchip->regmap, REG_BL_CONF_1, 0x50);
	if (switching_freq == 500000)
		regmap_write(pchip->regmap, REG_BL_CONF_2, 0x0D);
	else
		regmap_write(pchip->regmap, REG_BL_CONF_2, 0x8D);
	lm3632_set_current(pchip, bl_current);
	regmap_write(pchip->regmap, REG_ENABLE, 0x19);

	return 0;
}
#endif

static void lm3632_fl_torch(struct led_classdev *cdev_torch, enum led_brightness value)
{

	struct lm3632_chip_data *pchip = container_of(cdev_torch, struct lm3632_chip_data, cdev_torch);
	if(value) {
		//set front torch current to 50mA
		regmap_update_bits(pchip->regmap, REG_FL_CONF_1, 0xF0, 0x10);
		regmap_update_bits(pchip->regmap, REG_IO_CTRL, 0x10, 0x10);
		regmap_update_bits(pchip->regmap, REG_ENABLE, 0x06, 0x02);
		gpio_direction_output(lcd_strobe_on, 1);
	} else {
		regmap_update_bits(pchip->regmap, REG_IO_CTRL, 0x10, 0x00);
		regmap_update_bits(pchip->regmap, REG_ENABLE, 0x06, 0x00);
		gpio_direction_output(lcd_strobe_on, 0);
	}
}

static const struct dev_pm_ops lm3632_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(lm3632_suspend, lm3632_resume)
};

static const struct i2c_device_id lm3632_ids[] = {
	{ DEV_NAME, 0 },
	{}
};
MODULE_DEVICE_TABLE(i2c, lm3632_ids);

#ifdef CONFIG_OF
static const struct of_device_id lm3632_of_match[] = {
	{ .compatible = "ti,lm3632", },
	{ }
};
MODULE_DEVICE_TABLE(of, lm3632_of_match);
#endif

static struct i2c_driver lm3632_driver = {
	.probe = lm3632_probe,
	.remove = lm3632_remove,
	.driver = {
		.name = DEV_NAME,
		.owner = THIS_MODULE,
		.pm     = &lm3632_pm_ops,
#ifdef CONFIG_OF
		.of_match_table = of_match_ptr(lm3632_of_match),
#endif
	},
	.id_table = lm3632_ids,
};

module_i2c_driver(lm3632_driver);

MODULE_DESCRIPTION("Texas Instruments Backlight+Flash LED driver for LM3632");
MODULE_AUTHOR("Daniel Jeong <daniel.jeong@ti.com>");
MODULE_AUTHOR("G.Shark Jeong <gshark.jeong@gmail.com>");
MODULE_LICENSE("GPL v2");
