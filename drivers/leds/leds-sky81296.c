/*
* General driver for sky81296 LED Flash driver chip
*
* Author: Kyoungho yun <kyoungho.yun@samsung.com>
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
#include <linux/platform_data/leds-sky81296.h>
#include <linux/of.h>
#include <linux/of_gpio.h>

enum sky81296_common_dev {
	DEV_FLASH1 = 0,
	DEV_FLASH2,
	DEV_TORCH1,
	DEV_MAX,
};

struct sky81296_chip_data {
	struct device *dev;
	struct i2c_client *client;

	struct led_classdev cdev_flash1;
	struct led_classdev cdev_flash2;
	struct led_classdev cdev_torch1;

	struct work_struct work_flash1;
	struct work_struct work_flash2;
	struct work_struct work_torch1;

	u8 br_flash1;
	u8 br_flash2;
	u8 br_torch1;

	struct mutex lock;
	struct regmap *regmap;
	struct sky81296_platform_data *pdata;
};

extern struct class *camera_class; /*sys/class/camera*/
struct sky81296_chip_data *t_pchip;


#define pchip_sky81296(_cdev)\
	container_of(led_cdev, struct sky81296_chip_data, _cdev)

/* torch1 control */
static ssize_t sky81296_torch1_ctrl_store(struct device *dev,
					struct device_attribute *devAttr,
					const char *buf, size_t size)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct sky81296_chip_data *pchip = pchip_sky81296(cdev_torch1);
	struct pinctrl *pinctrl;
	int state, ret;

	pinctrl = devm_pinctrl_get_select(dev, "sky-flash"); 
	if (IS_ERR(pinctrl)) {
		dev_err(dev, "can't get/select pinctrl for sky81296\n");
	}

	pchip->pdata->flash1_pin = of_get_named_gpio(pchip->pdata->np, "sky81296,flash1-gpio", 0);
	pchip->pdata->flash2_pin = of_get_named_gpio(pchip->pdata->np, "sky81296,flash2-gpio", 0);
	pchip->pdata->torch_pin = of_get_named_gpio(pchip->pdata->np, "sky81296,torch-gpio", 0);

	ret = gpio_request(pchip->pdata->flash1_pin, "sky81296,flash1-gpio");
	if (ret) {
		printk(KERN_ERR "%s: unable to request sky81296,flash1-gpio [%d]\n",
				__func__, pchip->pdata->flash1_pin);
		goto out_flash1;
	}
	ret = gpio_request(pchip->pdata->flash2_pin, "sky81296,flash2-gpio");
	if (ret) {
		printk(KERN_ERR "%s: unable to request sky81296,flash2-gpio [%d]\n",
				__func__, pchip->pdata->flash2_pin);
		goto out_flash2;
	}
	ret = gpio_request(pchip->pdata->torch_pin, "sky81296,torch-gpio");
	if (ret) {
		printk(KERN_ERR "%s: unable to request sky81296,torch-gpio [%d]\n",
				__func__, pchip->pdata->torch_pin);
		goto out_torch;
	}

	ret = kstrtouint(buf, 10, &state);
	if (ret)
		goto out;
	if (state != 0) {
		ret = gpio_direction_output(pchip->pdata->torch_pin, 1);
		ret |= gpio_direction_output(pchip->pdata->flash1_pin, 1);
		ret |= gpio_direction_output(pchip->pdata->flash2_pin, 1);
	} else {
		ret = gpio_direction_output(pchip->pdata->torch_pin, 0);
		ret |= gpio_direction_output(pchip->pdata->flash1_pin, 0);
		ret |= gpio_direction_output(pchip->pdata->flash2_pin, 0);
	}
	if (ret < 0)
		printk(KERN_ERR "%s: SKY81296 GPIO control failed.\n",__func__);

out:
	gpio_free(pchip->pdata->torch_pin);
out_torch:
	gpio_free(pchip->pdata->flash2_pin);
out_flash2:
	gpio_free(pchip->pdata->flash1_pin);
out_flash1:
	if (!IS_ERR(pinctrl))
		devm_pinctrl_put(pinctrl);

	return size;
}
static DEVICE_ATTR(rear_flash, S_IWUSR, NULL, sky81296_torch1_ctrl_store);

/* module initialize */
static const struct regmap_config sky81296_regmap = {
	.reg_bits = 8,
	.val_bits = 8,
	.max_register = 0xFF,
};

#ifdef CONFIG_OF
static int sky81296_parse_dt(struct device *dev,
		struct sky81296_platform_data *pdata)
{
	struct device_node *np = dev->of_node;
	char *str = NULL;
	int ret;

	ret = of_property_read_string(np, "sky81296,dev_name", (const char **)&str);
	if (ret) {
		pr_err("sky81296: fail to read, sky81296\n");
		return -ENODEV;
	}

	if (str)
		dev_err(dev, "sky81296: DT dev name = %s\n", str);

	pdata->np = np;
	dev_err(dev, "[%s::%d]\n", __func__, __LINE__);
	return 0;
}
#else
static int sky81296_parse_dt(struct device *dev,
		struct sky81296_platform_data *pdata)
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

int sky81296_torch_ctrl(int state)
{
	struct pinctrl *pinctrl;
	int ret, result = 0;

	pinctrl = devm_pinctrl_get_select(t_pchip->dev, "sky-torch");
	if (IS_ERR(pinctrl)) {
		printk(KERN_ERR "can't get/select pinctrl for sky81296 torch\n");
	}

	t_pchip->pdata->torch_pin = of_get_named_gpio(t_pchip->pdata->np, "sky81296,torch-gpio", 0);

	ret = gpio_request(t_pchip->pdata->torch_pin, "sky81296,torch-gpio");
	if (ret) {
		printk(KERN_ERR "%s: unable to request sky81296,torch-gpio [%d]\n",
				__func__, t_pchip->pdata->torch_pin);
		result = -ENODEV;
		goto out_torch;
	}

	if (state != 0) {
		ret = gpio_direction_output(t_pchip->pdata->torch_pin, 1);
	} else {
		ret = gpio_direction_output(t_pchip->pdata->torch_pin, 0);
	}

out_torch:
	if (!IS_ERR(pinctrl))
		devm_pinctrl_put(pinctrl);

	gpio_free(t_pchip->pdata->torch_pin);
	return result;
}
EXPORT_SYMBOL(sky81296_torch_ctrl);

#if 0
static int sky81296_torch1_ctrl(struct sky81296_chip_data *pchip)
{
	struct pinctrl *pinctrl;
	int state = 1;
	int ret, result = 0;

	pinctrl = devm_pinctrl_get_select(pchip->dev, "sky-flash"); 
	if (IS_ERR(pinctrl)) {
		dev_err(pchip->dev, "can't get/select pinctrl for sky81296\n");
	}

	pchip->pdata->flash1_pin = of_get_named_gpio(pchip->pdata->np, "sky81296,flash1-gpio", 0);
	pchip->pdata->flash2_pin = of_get_named_gpio(pchip->pdata->np, "sky81296,flash2-gpio", 0);
	pchip->pdata->torch_pin = of_get_named_gpio(pchip->pdata->np, "sky81296,torch-gpio", 0);

	ret = gpio_request(pchip->pdata->flash1_pin, "sky81296,flash1-gpio");
	if (ret) {
		printk(KERN_ERR "%s: unable to request sky81296,flash1-gpio [%d]\n",
				__func__, pchip->pdata->flash1_pin);
		result = -ENODEV;
		goto out_flash1;
	}
	ret = gpio_request(pchip->pdata->flash2_pin, "sky81296,flash2-gpio");
	if (ret) {
		printk(KERN_ERR "%s: unable to request sky81296,flash2-gpio [%d]\n",
				__func__, pchip->pdata->flash2_pin);
		result = -ENODEV;
		goto out_flash2;
	}
	ret = gpio_request(pchip->pdata->torch_pin, "sky81296,torch-gpio");
	if (ret) {
		printk(KERN_ERR "%s: unable to request sky81296,torch-gpio [%d]\n",
				__func__, pchip->pdata->torch_pin);
		result = -ENODEV;
		goto out_torch;
	}

	if (state != 0) {
		ret = gpio_direction_output(pchip->pdata->torch_pin, 1);
		ret |= gpio_direction_output(pchip->pdata->flash1_pin, 1);
		ret |= gpio_direction_output(pchip->pdata->flash2_pin, 1);
	} else {
		ret = gpio_direction_output(pchip->pdata->torch_pin, 0);
		ret |= gpio_direction_output(pchip->pdata->flash1_pin, 0);
		ret |= gpio_direction_output(pchip->pdata->flash2_pin, 0);
	}

	gpio_free(pchip->pdata->torch_pin);
out_torch:
	gpio_free(pchip->pdata->flash2_pin);
out_flash2:
	gpio_free(pchip->pdata->flash1_pin);
out_flash1:
	if (!IS_ERR(pinctrl))
		devm_pinctrl_put(pinctrl);

	return result;
}
#endif
static int sky81296_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct sky81296_platform_data *pdata;
	struct sky81296_chip_data *pchip;
	int err;

	if (client->dev.of_node) {
		pdata = devm_kzalloc(&client->dev, 
			sizeof(struct sky81296_platform_data), GFP_KERNEL);
		if (!pdata) {
			dev_err(&client->dev, "Failed to allocate memory\n");
			return -ENOMEM;
		}

		err = sky81296_parse_dt(&client->dev, pdata);
		if (err) {
			dev_err(&client->dev, "[%s] sky81296 parse dt failed\n", __func__);
			return err;
		}	
	} else
		pdata = client->dev.platform_data;

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		dev_err(&client->dev, "i2c functionality check fail\n");
		return -EOPNOTSUPP;
	}

	pchip = devm_kzalloc(&client->dev,
			     sizeof(struct sky81296_chip_data), GFP_KERNEL);
	if (!pchip)
		return -ENOMEM;
	pchip->client = client;
	pchip->dev = &client->dev;
		
	/* regmap */
	pchip->regmap = devm_regmap_init_i2c(client, &sky81296_regmap);
	if (IS_ERR(pchip->regmap)) {
		err = PTR_ERR(pchip->regmap);
		return err;
	}
	pchip->pdata = pdata;
	regmap_update_bits(pchip->regmap, 0x00, 0x1F, 0x0F);
	regmap_update_bits(pchip->regmap, 0x01, 0x1F, 0x0F);
	regmap_update_bits(pchip->regmap, 0x03, 0x0F, 0x07);

	mutex_init(&pchip->lock);
	i2c_set_clientdata(client, pchip);

	/* flash1 initialize */
	pchip->cdev_flash1.name = "flash1";
	pchip->cdev_flash1.max_brightness = 15;
	pchip->cdev_flash1.default_trigger = "flash1";
	err = led_classdev_register((struct device *)
				    &client->dev, &pchip->cdev_flash1);
	if (err < 0)
		goto err_out_flash1;

	/* torch1 initialize */
	pchip->cdev_torch1.name = "torch1";
	pchip->cdev_torch1.max_brightness = 7;
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

	/* flash2 initialize */
	pchip->cdev_flash2.name = "flash2";
	pchip->cdev_flash2.max_brightness = 15;
	pchip->cdev_flash2.default_trigger = "flash2";
	err = led_classdev_register((struct device *)
				    &client->dev, &pchip->cdev_flash2);
	if (err < 0)
		goto err_out_flash2;

	t_pchip = pchip;
	return 0;

err_out_flash2:
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

static int sky81296_remove(struct i2c_client *client)
{
	struct sky81296_chip_data *pchip = i2c_get_clientdata(client);

	device_remove_file(pchip->cdev_torch1.dev, &dev_attr_rear_flash);
	class_destroy(camera_class);
	led_classdev_unregister(&pchip->cdev_flash2);
	flush_work(&pchip->work_flash2);
	led_classdev_unregister(&pchip->cdev_torch1);
	flush_work(&pchip->work_torch1);
	led_classdev_unregister(&pchip->cdev_flash1);
	flush_work(&pchip->work_flash1);
	return 0;
}

#ifdef CONFIG_OF
static struct of_device_id sky81296_dt_ids[] = {
	{ .compatible = "sky81296",},
	{},
};
/*MODULE_DEVICE_TABLE(of, sky81296_dt_ids);*/
#endif

static const struct i2c_device_id sky81296_id[] = {
	{sky81296_NAME, 0},
	{}
};

MODULE_DEVICE_TABLE(i2c, sky81296_id);

static struct i2c_driver sky81296_i2c_driver = {
	.driver = {
		   .name = sky81296_NAME,
		   .owner = THIS_MODULE,
		   .pm = NULL,
		   
#ifdef CONFIG_OF
		   .of_match_table = sky81296_dt_ids,
#endif
		   },
	.probe = sky81296_probe,
	.remove = sky81296_remove,
	.id_table = sky81296_id,
};

module_i2c_driver(sky81296_i2c_driver);

MODULE_DESCRIPTION("Flash Lighting driver for sky81296");
MODULE_AUTHOR("Kyoungho Yun <kyoungho.yun@samsung.com>");
MODULE_LICENSE("GPL v2");
