/*
* General driver for Texas Instruments lm3560 LED Flash driver chip
* Copyright (C) 2013 Texas Instruments
*
* Author: Daniel Jeong <daniel.jeong@ti.com>
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 as
* published by the Free Software Foundation.
*/

#include <linux/module.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/gpio.h>
#include <linux/leds.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/fs.h>
#include <linux/regmap.h>
#include <linux/workqueue.h>
#include <linux/platform_data/leds-lm3560.h>
#include <linux/of.h>
#include <linux/of_gpio.h>

enum lm3560_type {
	CHIP_LM3560 = 0,
	CHIP_LM3561,
};

enum lm3560_mode {
	MODE_SHDN = 0,
	MODE_INDI,
	MODE_TORCH,
	MODE_FLASH,
	MODE_MAX
};

enum lm3560_common_dev {
	DEV_FLASH1 = 0,
	DEV_FLASH2,
	DEV_TORCH1,
	DEV_TORCH2,
	DEV_INDI,
	DEV_MAX,
};

enum lm3560_cmds {
	CMD_ENABLE = 0,
	CMD_LED1_ENABLE,
	CMD_LED1_FLASH_I,
	CMD_LED1_TORCH_I,
	CMD_LED2_ENABLE,
	CMD_LED2_FLASH_I,
	CMD_LED2_TORCH_I,
	CMD_FLASH_TOUT,
	CMD_INDI_I,
	CMD_INDI_LED1,
	CMD_INDI_LED2,
	CMD_INDI_TPULSE,
	CMD_INDI_TBLANK,
	CMD_INDI_PERIOD,
	CMD_PIN_TORCH,
	CMD_PIN_STROBE,
	CMD_MAX,
};

struct lm3560_cmd_data {
	u8 reg;
	u8 mask;
	u8 shift;
};

struct lm3560_chip_data {
	struct device *dev;
	struct i2c_client *client;
	enum lm3560_type type;

	struct led_classdev cdev_flash1;
	struct led_classdev cdev_flash2;
	struct led_classdev cdev_torch1;
	struct led_classdev cdev_torch2;
	struct led_classdev cdev_indicator;

	struct work_struct work_flash1;
	struct work_struct work_flash2;
	struct work_struct work_torch1;
	struct work_struct work_torch2;
	struct work_struct work_indicator;

	u8 br_flash1;
	u8 br_flash2;
	u8 br_torch1;
	u8 br_torch2;
	u8 br_indicator;

	struct mutex lock;
	struct regmap *regmap;
	struct lm3560_cmd_data *cmd;
	struct lm3560_platform_data *pdata;
};

static struct lm3560_cmd_data lm3560_cmds[CMD_MAX] = {
	[CMD_ENABLE] = {0x10, 0x03, 0},
	[CMD_LED1_ENABLE] = {0x10, 0x01, 3},
	[CMD_LED1_FLASH_I] = {0xB0, 0x0F, 0},
	[CMD_LED1_TORCH_I] = {0xA0, 0x07, 0},
	[CMD_LED2_ENABLE] = {0x10, 0x01, 4},
	[CMD_LED2_FLASH_I] = {0xB0, 0x0F, 4},
	[CMD_LED2_TORCH_I] = {0xA0, 0x07, 4},
	[CMD_FLASH_TOUT] = {0xC0, 0x3F, 0},
	[CMD_INDI_I] = {0x12, 0x07, 0},
	[CMD_INDI_TPULSE] = {0x13, 0x0F, 0},
	[CMD_INDI_TBLANK] = {0x13, 0x07, 4},
	[CMD_INDI_PERIOD] = {0x12, 0x07, 3},
	[CMD_INDI_LED1] = {0x11, 0x01, 4},
	[CMD_INDI_LED2] = {0x11, 0x01, 5},
	[CMD_PIN_TORCH] = {0xE0, 0x01, 7},
	[CMD_PIN_STROBE] = {0xE0, 0x01, 2},
};

extern struct class *camera_class; /*sys/class/camera*/
struct lm3560_chip_data *t_pchip;

static int lm3560_reg_update(struct lm3560_chip_data *pchip,
			     u8 reg, u8 mask, u8 data)
{
	return regmap_update_bits(pchip->regmap, reg, mask, data);
}

int lm3560_reg_update_export(u8 reg, u8 mask, u8 data)
{
	return regmap_update_bits(t_pchip->regmap, reg, mask, data);
}

EXPORT_SYMBOL(lm3560_reg_update_export);

static int lm3560_reg_read(struct lm3560_chip_data *pchip, u8 reg)
{
	int ret;
	unsigned int reg_val;

	ret = regmap_read(pchip->regmap, reg, &reg_val);
	if (ret < 0)
		return ret;
	return reg_val & 0xFF;
}

static int lm3560_cmd_write(struct lm3560_chip_data *pchip,
			    enum lm3560_cmds cmd, u8 data)
{
	int ret;
	struct lm3560_cmd_data *pcmd = pchip->cmd;
	u8 reg = pcmd[cmd].reg;
	u8 mask = pcmd[cmd].mask << pcmd[cmd].shift;

	data = data << pcmd[cmd].shift;
	ret = lm3560_reg_update(pchip, reg, mask, data);
	if (ret < 0)
		return ret;
	return 0;
}

static int lm3560_chip_init(struct lm3560_chip_data *pchip)
{
	int rval;

	rval = lm3560_cmd_write(pchip, CMD_PIN_TORCH, pchip->pdata->torch);
	if (rval < 0)
		return rval;

	rval = lm3560_cmd_write(pchip, CMD_PIN_STROBE, pchip->pdata->strobe);
	if (rval < 0)
		return rval;

	rval =
	    lm3560_cmd_write(pchip, CMD_FLASH_TOUT, pchip->pdata->flash_time);
	if (rval < 0)
		return rval;

	rval = lm3560_cmd_write(pchip, CMD_INDI_LED1, pchip->pdata->indi_led1);
	if (rval < 0)
		return rval;

	rval = lm3560_cmd_write(pchip, CMD_INDI_LED2, pchip->pdata->indi_led2);
	if (rval < 0)
		return rval;

	return 0;
}

static int lm3560_pdata_init(struct lm3560_chip_data *pchip)
{
	int rval;
	struct lm3560_cmd_data *pcmd = pchip->cmd;

	pchip->pdata = devm_kzalloc(pchip->dev,
				    sizeof(struct lm3560_platform_data),
				    GFP_KERNEL);
	if (!pchip->pdata)
		return -ENOMEM;
	rval = lm3560_reg_read(pchip, pcmd[CMD_PIN_STROBE].reg);
	if (rval < 0)
		return rval;
	pchip->pdata->torch = (rval >> pcmd[CMD_PIN_TORCH].shift)
	    & pcmd[CMD_PIN_TORCH].mask;
	pchip->pdata->strobe = (rval >> pcmd[CMD_PIN_STROBE].shift)
	    & pcmd[CMD_PIN_STROBE].mask;

	if (pchip->type != CHIP_LM3560)
		goto out;

	rval = lm3560_reg_read(pchip, pcmd[CMD_INDI_LED1].reg);
	if (rval < 0)
		return rval;
	pchip->pdata->indi_led1 = (rval >> pcmd[CMD_INDI_LED1].shift)
	    & pcmd[CMD_INDI_LED1].mask;
	pchip->pdata->indi_led2 = (rval >> pcmd[CMD_INDI_LED2].shift)
	    & pcmd[CMD_INDI_LED2].mask;

	regmap_update_bits(pchip->regmap, 0xE0, 0xFF, 0xEF);
out:
	return 0;
}

/* mode control */
#if 0
static void lm3560_ctrl_mode(struct lm3560_chip_data *pchip,
			     const char *buf, enum lm3560_mode mode)
{
	ssize_t ret;
	unsigned int state;

	ret = kstrtouint(buf, 10, &state);
	if (ret)
		goto out;
	if (state != 0)
		ret = lm3560_cmd_write(pchip, CMD_ENABLE, mode);
	else
		ret = lm3560_cmd_write(pchip, CMD_ENABLE, MODE_SHDN);
	if (ret < 0)
		goto out;
	return;
out:
	dev_err(pchip->dev, "%s: fail to set mode %d\n", __func__, mode);
	return;
}
#endif
#define pchip_lm3560(_cdev)\
	container_of(led_cdev, struct lm3560_chip_data, _cdev)

static void lm3560_deferred_flash1_brightness_set(struct work_struct *work)
{
	struct lm3560_chip_data *pchip =
	    container_of(work, struct lm3560_chip_data, work_flash1);

	mutex_lock(&pchip->lock);
	if (lm3560_cmd_write(pchip, CMD_LED1_FLASH_I, pchip->br_flash1) < 0)
		dev_err(pchip->dev,
			"%s: fail to set flash brightness\n", __func__);
	mutex_unlock(&pchip->lock);
}

static void lm3560_flash1_brightness_set(struct led_classdev *cdev,
					 enum led_brightness brightness)
{
	struct lm3560_chip_data *pchip =
	    container_of(cdev, struct lm3560_chip_data, cdev_flash1);

	pchip->br_flash1 = brightness;
	schedule_work(&pchip->work_flash1);
}

/* torch1 control */
static ssize_t lm3560_torch1_ctrl_store(struct device *dev,
					struct device_attribute *devAttr,
					const char *buf, size_t size)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct lm3560_chip_data *pchip = pchip_lm3560(cdev_torch1);
	struct pinctrl *pinctrl;
	int state, ret;

	pinctrl = devm_pinctrl_get_select(dev, "driver-flash");
	if (IS_ERR(pinctrl)) {
		dev_err(dev, "can't get/select pinctrl for lm3560\n");
	}

	pchip->pdata->torch_pin = of_get_named_gpio(pchip->pdata->np, "lm3560,torch-gpio", 0);
	ret = gpio_request(pchip->pdata->torch_pin, "lm3560,torch-gpio");
	if (ret) {
		printk(KERN_ERR "%s: unable to request lm3560,torch-gpio [%d]\n",
				__func__, pchip->pdata->torch_pin);
		if (!IS_ERR(pinctrl))
			devm_pinctrl_put(pinctrl);
		return -ENODEV;
	}

	ret = kstrtouint(buf, 10, &state);
	if (ret)
		goto out;
	if (state != 0)
		gpio_direction_output(pchip->pdata->torch_pin, 1);
	else
		gpio_direction_output(pchip->pdata->torch_pin, 0);
	if (ret < 0)
		goto out;

out:
	if (!IS_ERR(pinctrl))
		devm_pinctrl_put(pinctrl);

	gpio_free(pchip->pdata->torch_pin);
	return size;
}
static DEVICE_ATTR(rear_flash, S_IWUSR, NULL, lm3560_torch1_ctrl_store);

static void lm3560_deferred_torch1_brightness_set(struct work_struct *work)
{
	struct lm3560_chip_data *pchip =
	    container_of(work, struct lm3560_chip_data, work_torch1);

	mutex_lock(&pchip->lock);
	if (lm3560_cmd_write(pchip, CMD_LED1_TORCH_I, pchip->br_torch1) < 0)
		dev_err(pchip->dev,
			"%s: fail to set torch brightness\n", __func__);
	mutex_unlock(&pchip->lock);
}

static void lm3560_torch1_brightness_set(struct led_classdev *cdev,
					 enum led_brightness brightness)
{
	struct lm3560_chip_data *pchip =
	    container_of(cdev, struct lm3560_chip_data, cdev_torch1);
	pchip->br_torch1 = brightness;
	schedule_work(&pchip->work_torch1);
}

static void lm3560_deferred_flash2_brightness_set(struct work_struct *work)
{
	struct lm3560_chip_data *pchip =
	    container_of(work, struct lm3560_chip_data, work_flash2);

	mutex_lock(&pchip->lock);
	if (lm3560_cmd_write(pchip, CMD_LED2_FLASH_I, pchip->br_flash2) < 0)
		dev_err(pchip->dev,
			"%s: fail to set flash brightness\n", __func__);
	mutex_unlock(&pchip->lock);
}

static void lm3560_flash2_brightness_set(struct led_classdev *cdev,
					 enum led_brightness brightness)
{
	struct lm3560_chip_data *pchip =
	    container_of(cdev, struct lm3560_chip_data, cdev_flash2);

	pchip->br_flash2 = brightness;
	schedule_work(&pchip->work_flash2);
}

static void lm3560_deferred_torch2_brightness_set(struct work_struct *work)
{
	struct lm3560_chip_data *pchip =
	    container_of(work, struct lm3560_chip_data, work_torch2);

	mutex_lock(&pchip->lock);
	if (lm3560_cmd_write(pchip, CMD_LED2_TORCH_I, pchip->br_torch2) < 0)
		dev_err(pchip->dev,
			"%s: fail to set torch brightness\n", __func__);
	mutex_unlock(&pchip->lock);
}

static void lm3560_torch2_brightness_set(struct led_classdev *cdev,
					 enum led_brightness brightness)
{
	struct lm3560_chip_data *pchip =
	    container_of(cdev, struct lm3560_chip_data, cdev_torch2);
	pchip->br_torch2 = brightness;
	schedule_work(&pchip->work_torch2);
}

static void lm3560_deferred_indicator_brightness_set(struct work_struct *work)
{
	struct lm3560_chip_data *pchip =
	    container_of(work, struct lm3560_chip_data, work_indicator);

	mutex_lock(&pchip->lock);
	if (lm3560_cmd_write(pchip, CMD_INDI_I, pchip->br_indicator) < 0)
		dev_err(pchip->dev,
			"%s: fail to set indicator brightness\n", __func__);
	mutex_unlock(&pchip->lock);
}

static void lm3560_indicator_brightness_set(struct led_classdev *cdev,
					    enum led_brightness brightness)
{
	struct lm3560_chip_data *pchip =
	    container_of(cdev, struct lm3560_chip_data, cdev_indicator);

	pchip->br_indicator = brightness;
	schedule_work(&pchip->work_indicator);
}

/* module initialize */
static const struct regmap_config lm3560_regmap = {
	.reg_bits = 8,
	.val_bits = 8,
	.max_register = 0xFF,
};

#ifdef CONFIG_OF
static int lm3560_parse_dt(struct device *dev,
		struct lm3560_platform_data *pdata)
{
	struct device_node *np = dev->of_node;
	char *str = NULL;
	int ret;

	ret = of_property_read_string(np, "lm3560,dev_name", (const char **)&str);
	if (ret) {
		pr_err("lm3560: fail to read, lm3560\n");
		return -ENODEV;
	}

	if (str)
		dev_err(dev, "lm3560: DT dev name = %s\n", str);

	pdata->np = np;
	dev_err(dev, "[%s::%d]\n", __func__, __LINE__);
	return 0;
}
#else
static int lm3560_parse_dt(struct device *dev,
		struct lm3560_platform_data *pdata)
{
	return -ENODEV;
}
#endif

int flash_classdev_register(struct device *parent, struct led_classdev *led_cdev)
{

	if (camera_class == NULL) {
		camera_class = class_create(THIS_MODULE, "camera");
		if (IS_ERR(camera_class)) {
			pr_err("Failed to create class(camera)!\n");
			return PTR_ERR(camera_class);
		}
	}

	led_cdev->dev = device_create(camera_class, parent, 0, led_cdev,
				      "flash");
	if (IS_ERR(led_cdev->dev))
		return PTR_ERR(led_cdev->dev);

	dev_dbg(parent, "Registered led device: %s\n",
			led_cdev->name);

	return 0;
}

static int lm3560_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct lm3560_platform_data *pdata;
	struct lm3560_chip_data *pchip;
	int err;

	if (client->dev.of_node) {
		pdata = devm_kzalloc(&client->dev, 
			sizeof(struct lm3560_platform_data), GFP_KERNEL);
		if (!pdata) {
			dev_err(&client->dev, "Failed to allocate memory\n");
			return -ENOMEM;
		}

		err = lm3560_parse_dt(&client->dev, pdata);
		if (err) {
			dev_err(&client->dev, "[%s] lm3560 parse dt failed\n", __func__);
			return err;
		}	
	} else
		pdata = client->dev.platform_data;

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		dev_err(&client->dev, "i2c functionality check fail\n");
		return -EOPNOTSUPP;
	}

	pchip = devm_kzalloc(&client->dev,
			     sizeof(struct lm3560_chip_data), GFP_KERNEL);
	if (!pchip)
		return -ENOMEM;
	pchip->client = client;
	pchip->dev = &client->dev;
	pchip->cmd = lm3560_cmds;
		
	/* regmap */
	pchip->regmap = devm_regmap_init_i2c(client, &lm3560_regmap);
	if (IS_ERR(pchip->regmap)) {
		err = PTR_ERR(pchip->regmap);
		return err;
	}
	/* platform data */
	if (pchip->pdata == NULL) {
		err = lm3560_pdata_init(pchip);
		if (err < 0)
			return err;
		pchip->pdata = pdata;
	} else {
		err = lm3560_chip_init(pchip);
		if (err < 0)
			return err;
	}
	mutex_init(&pchip->lock);
	i2c_set_clientdata(client, pchip);

	/* flash1 initialize */
	INIT_WORK(&pchip->work_flash1, lm3560_deferred_flash1_brightness_set);
	pchip->cdev_flash1.name = "flash1";
	pchip->cdev_flash1.max_brightness = 15;
	pchip->cdev_flash1.brightness_set = lm3560_flash1_brightness_set;
	pchip->cdev_flash1.default_trigger = "flash1";
	err = led_classdev_register((struct device *)
				    &client->dev, &pchip->cdev_flash1);
	if (err < 0)
		goto err_out_flash1;

	/* torch1 initialize */
	INIT_WORK(&pchip->work_torch1, lm3560_deferred_torch1_brightness_set);
	pchip->cdev_torch1.name = "torch1";
	pchip->cdev_torch1.max_brightness = 7;
	pchip->cdev_torch1.brightness_set = lm3560_torch1_brightness_set;
	pchip->cdev_torch1.default_trigger = "torch1";
	err = flash_classdev_register((struct device *)
				    &client->dev, &pchip->cdev_torch1);
	if (err < 0)
		goto err_out_torch1;

	if (device_create_file(pchip->cdev_torch1.dev, &dev_attr_rear_flash) < 0) {
		dev_err(&client->dev, "failed to create device file, %s\n",
				dev_attr_rear_flash.attr.name);
		goto err_out_torch1_ctrl;
	}

	/* indicator initialize */
	INIT_WORK(&pchip->work_indicator,
		  lm3560_deferred_indicator_brightness_set);
	pchip->cdev_indicator.name = "indicator";
	pchip->cdev_indicator.max_brightness = 7;
	pchip->cdev_indicator.brightness_set = lm3560_indicator_brightness_set;
	err = led_classdev_register((struct device *)
				    &client->dev, &pchip->cdev_indicator);
	if (err < 0)
		goto err_out_indicator;


	/* flash2 initialize */
	INIT_WORK(&pchip->work_flash2,
		  lm3560_deferred_flash2_brightness_set);
	pchip->cdev_flash2.name = "flash2";
	pchip->cdev_flash2.max_brightness = 15;
	pchip->cdev_flash2.brightness_set =
	    lm3560_flash2_brightness_set;
	pchip->cdev_flash2.default_trigger = "flash2";
	err = led_classdev_register((struct device *)
				    &client->dev, &pchip->cdev_flash2);
	if (err < 0)
		goto err_out_flash2;

	/* torch2 initialize */
	INIT_WORK(&pchip->work_torch2,
		  lm3560_deferred_torch2_brightness_set);
	pchip->cdev_torch2.name = "torch2";
	pchip->cdev_torch2.max_brightness = 7;
	pchip->cdev_torch2.brightness_set =
	    lm3560_torch2_brightness_set;
	pchip->cdev_torch2.default_trigger = "torch2";
	err = led_classdev_register((struct device *)
				    &client->dev, &pchip->cdev_torch2);
	if (err < 0)
		goto err_out_torch2;
	dev_info(&client->dev, "LM3560 is initialized\n");

	t_pchip = pchip;
	return 0;

err_out_torch2:
	led_classdev_unregister(&pchip->cdev_flash2);
	flush_work(&pchip->work_flash2);
err_out_flash2:
	led_classdev_unregister(&pchip->cdev_indicator);
	flush_work(&pchip->work_indicator);
err_out_indicator:
	device_remove_file(pchip->cdev_torch1.dev, &dev_attr_rear_flash);
err_out_torch1_ctrl:
	led_classdev_unregister(&pchip->cdev_torch1);
	flush_work(&pchip->work_torch1);
err_out_torch1:
	led_classdev_unregister(&pchip->cdev_flash1);
	flush_work(&pchip->work_flash1);
err_out_flash1:
	return err;
}

static int lm3560_remove(struct i2c_client *client)
{
	struct lm3560_chip_data *pchip = i2c_get_clientdata(client);

	device_remove_file(pchip->cdev_torch1.dev, &dev_attr_rear_flash);
	class_destroy(camera_class);
	led_classdev_unregister(&pchip->cdev_torch2);
	flush_work(&pchip->work_torch2);
	led_classdev_unregister(&pchip->cdev_flash2);
	flush_work(&pchip->work_flash2);

	led_classdev_unregister(&pchip->cdev_indicator);
	flush_work(&pchip->work_indicator);
	led_classdev_unregister(&pchip->cdev_torch1);
	flush_work(&pchip->work_torch1);
	led_classdev_unregister(&pchip->cdev_flash1);
	flush_work(&pchip->work_flash1);
	return 0;
}

#ifdef CONFIG_OF
static struct of_device_id lm3560_dt_ids[] = {
	{ .compatible = "lm3560",},
	{},
};
/*MODULE_DEVICE_TABLE(of, lm3560_dt_ids);*/
#endif

static const struct i2c_device_id lm3560_id[] = {
	{LM3560_NAME, CHIP_LM3560},
	{}
};

MODULE_DEVICE_TABLE(i2c, lm3560_id);

static struct i2c_driver lm3560_i2c_driver = {
	.driver = {
		   .name = LM3560_NAME,
		   .owner = THIS_MODULE,
		   .pm = NULL,
		   
#ifdef CONFIG_OF
		   .of_match_table = lm3560_dt_ids,
#endif
		   },
	.probe = lm3560_probe,
	.remove = lm3560_remove,
	.id_table = lm3560_id,
};

module_i2c_driver(lm3560_i2c_driver);

MODULE_DESCRIPTION("Texas Instruments Flash Lighting driver for lm3560");
MODULE_AUTHOR("Daniel Jeong <daniel.jeong@ti.com>");
MODULE_LICENSE("GPL v2");
