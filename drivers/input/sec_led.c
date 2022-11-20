/*
 * LEDs driver for GPIOs
 *
 * Copyright (C) 2007 8D Technologies inc.
 * Raphael Assenat <raph@8d.com>
 * Copyright (C) 2008 Freescale Semiconductor, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/of_platform.h>
#include <linux/of_gpio.h>
#include <linux/slab.h>
#include <linux/workqueue.h>
#include <linux/module.h>
#include <linux/pinctrl/consumer.h>
#include <linux/err.h>
#include <linux/regulator/consumer.h>

#ifdef CONFIG_SEC_FACTORY
#define LED_TWINKLE_BOOTING
#endif

struct sec_leds_data {
	struct device *dev;
	bool flag;
	int gpio;
//	struct regulator *vdd;
#ifdef LED_TWINKLE_BOOTING
	struct delayed_work sec_led_twinkle_work;
	bool led_twinkle_check;
#endif
};


#ifdef LED_TWINKLE_BOOTING
static void sec_led_twinkle_work(struct work_struct *work);
#endif

static int sec_led_set(struct sec_leds_data *data, int param)
{
	if (data->gpio)
		gpio_direction_output(data->gpio, param);

	dev_info(data->dev, "%s: gpio:%d(%d)\n", __func__,
					gpio_get_value(data->gpio), param);

	return 0;
}
static ssize_t sec_led_control(struct device *dev,
		 struct device_attribute *attr, const char *buf,
		 size_t count)
{
	struct sec_leds_data *data = dev_get_drvdata(dev);
	int ret, param;

	ret = sscanf(buf, "%d", &param);
	if (ret != 1) {
		dev_err(dev, "%s: cmd read err\n", __func__);
		return count;
	}

	if (!(param == 0 || param == 1)) {
		dev_err(dev, "%s: wrong command(%d)\n",
			__func__, param);
		return count;
	}

#ifdef LED_TWINKLE_BOOTING
	if (data->led_twinkle_check == 1){
		data->led_twinkle_check = 0;
		cancel_delayed_work(&data->sec_led_twinkle_work);
	}
#endif

	sec_led_set(data, param);

	dev_notice(dev, "%s (%d)\n",__func__, param);

	return count;
}

#ifdef LED_TWINKLE_BOOTING
static void sec_led_twinkle_work(struct work_struct *work)
{
	struct sec_leds_data *data = container_of(work, struct sec_leds_data,
						sec_led_twinkle_work.work);
	static bool led_on = 1;
	printk("[sec_led] twinkle_work = %d \n", led_on);

	if(data->led_twinkle_check == 1){
		sec_led_set(data, led_on);

		if (led_on)
			led_on = 0;
		else
			led_on = 1;

		schedule_delayed_work(&data->sec_led_twinkle_work, msecs_to_jiffies(400));
	}
	else {
		if(led_on == 0)
			sec_led_set(data, 0);
	}
}
#endif


static DEVICE_ATTR(brightness, S_IRUGO | S_IWUSR | S_IWGRP, NULL,
			sec_led_control);

static struct attribute *sec_touchkey_attributes[] = {
	&dev_attr_brightness.attr,
	NULL,
};

static struct attribute_group sec_touchkey_attr_group = {
	.attrs = sec_touchkey_attributes,
};


#ifdef CONFIG_OF
/*
 * Translate OpenFirmware node properties into platform_data
 */
static struct sec_leds_data *
sec_leds_get_devtree_data(struct device *dev)
{
	struct device_node *node;
	struct sec_leds_data *data;
	int error, ret;

	node = dev->of_node;
	if (!node) {
		error = -ENODEV;
		goto err_out;
	}

	data = kzalloc(sizeof(*data), GFP_KERNEL);
	if (!data) {
		error = -ENOMEM;
		goto err_out;
	}

	data->gpio = of_get_named_gpio(node, "gpios", 0);
	if(data->gpio < 0){
		dev_err(dev, "unable to get gpio_int\n");
		goto err_free_data;
	}

	dev_info(dev, "%s: gpio:%d\n", __func__, data->gpio);

	ret = gpio_request(data->gpio, "gpios");
	if(ret < 0){
		dev_err(dev, "unable to request gpio_int\n");
		goto err_free_data;
	}

	return data;

err_free_data:
	kfree(data);
err_out:
	return ERR_PTR(error);
}

#else /* CONFIG_OF_GPIO */

static struct sec_leds_data *
sec_leds_get_devtree_data(struct device *dev)

{
	return NULL;
}
#endif /* CONFIG_OF_GPIO */


static const struct of_device_id of_sec_leds_match[] = {
	{ .compatible = "sec_led", },
	{},
};


extern struct class *sec_class;
extern int get_lcd_attached(void);

static int sec_led_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct sec_leds_data *data = dev_get_platdata(dev);
	struct pinctrl *pinctrl;
	struct device *sec_touchkey;
	int ret;

	if (get_lcd_attached() == 1) {
		dev_err(dev, "[LED] Unload the LED driver because LCD is connected\n");
		return 0;
	}

	pinctrl = devm_pinctrl_get_select_default(&pdev->dev);
	if (IS_ERR(pinctrl))
		dev_warn(dev,
			"pins are not configured from the driver\n");

	if (!data) {
		data = sec_leds_get_devtree_data(dev);
		if (IS_ERR_OR_NULL(data))
			return PTR_ERR(data);
	}

	platform_set_drvdata(pdev, data);
	data->dev = dev;

	sec_touchkey = device_create(sec_class,
		NULL, 9, data, "sec_touchkey");
	if (IS_ERR(sec_touchkey))
		dev_err(dev,
			"Failed to create device for the touchkey sysfs\n");

#ifdef LED_TWINKLE_BOOTING
	if (get_lcd_attached() == 0) {
		dev_err(dev,
			"%s : LCD is not connected. so start LED twinkle \n", __func__);
		INIT_DELAYED_WORK(&data->sec_led_twinkle_work, sec_led_twinkle_work);
		data->led_twinkle_check =  1;
		schedule_delayed_work(&data->sec_led_twinkle_work, msecs_to_jiffies(400));
	}
#endif

	ret = sysfs_create_group(&sec_touchkey->kobj,
		&sec_touchkey_attr_group);
	if (ret)
		dev_err(dev, "Failed to create sysfs group\n");

	return 0;
}

static int sec_led_remove(struct platform_device *pdev)
{
	platform_set_drvdata(pdev, NULL);

	return 0;
}

static struct platform_driver sec_led_driver = {
	.probe		= sec_led_probe,
	.remove		= sec_led_remove,
	.driver		= {
		.name	= "sec_led",
		.owner	= THIS_MODULE,
		.of_match_table = of_match_ptr(of_sec_leds_match),
	},
};

module_platform_driver(sec_led_driver);

MODULE_AUTHOR("Raphael Assenat <raph@8d.com>, Trent Piepho <tpiepho@freescale.com>");
MODULE_DESCRIPTION("GPIO LED driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:leds-gpio");
