/*
 * Marvell RGB LEDs driver
 *
 * Copyright (C) 2014 Marvell Technology Group Ltd.
 *
 * Authors: Jane Li <jiel@marvell.com>
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
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/miscled-rgb.h>

static struct led_rgb_platform_data *led_rgb_pdata;
static DEFINE_SPINLOCK(led_rgb_lock);

void led_rgb_output(enum led_rgb_mode led_mode, int on)
{
	if (led_rgb_pdata == NULL)
		return;

	spin_lock(&led_rgb_lock);
	switch (led_mode) {
	case LED_R:
		if (led_rgb_pdata->gpio_r)
			gpio_direction_output(led_rgb_pdata->gpio_r, on);
		else
			pr_warn("Do not get LED R's GPIO!\n");
		break;
	case LED_G:
		if (led_rgb_pdata->gpio_g)
			gpio_direction_output(led_rgb_pdata->gpio_g, on);
		else
			pr_warn("Do not get LED G's GPIO!\n");
		break;
	case LED_B:
		if (led_rgb_pdata->gpio_b)
			gpio_direction_output(led_rgb_pdata->gpio_b, on);
		else
			pr_warn("Do not get LED B's GPIO!\n");
		break;
	}
	spin_unlock(&led_rgb_lock);
}

#ifdef CONFIG_OF
static const struct of_device_id led_of_match[] = {
	{
		.compatible = "marvell,led-rgb",
	},
	{},
};

MODULE_DEVICE_TABLE(of, led_of_match);

static int led_rgb_probe_dt(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	int gpio;

	/* get gpio_r from dt */
	gpio = of_get_named_gpio(np, "led_r", 0);
	if (unlikely(gpio < 0)) {
		dev_err(&pdev->dev, "GPIO LED R undefined\n");
		return -1;
	} else {
		led_rgb_pdata->gpio_r = gpio;
		dev_info(&pdev->dev, "GPIO LED R:%3d\n", gpio);
	}

	/* get gpio_g from dt */
	gpio = of_get_named_gpio(np, "led_g", 0);
	if (unlikely(gpio < 0)) {
		dev_err(&pdev->dev, "GPIO LED G undefined\n");
		return -1;
	} else {
		led_rgb_pdata->gpio_g = gpio;
		dev_info(&pdev->dev, "GPIO LED G:%3d\n", gpio);
	}

	/* get gpio_b from dt */
	gpio = of_get_named_gpio(np, "led_b", 0);
	if (unlikely(gpio < 0)) {
		dev_err(&pdev->dev, "GPIO LED B undefined\n");
		return -1;
	} else {
		led_rgb_pdata->gpio_b = gpio;
		dev_info(&pdev->dev, "GPIO LED B:%3d\n", gpio);
	}

	return 0;
}
#else
static int led_rgb_probe_dt(struct platform_device *pdev)
{
	return 0;
}
#endif

static int led_rgb_probe(struct platform_device *pdev)
{
	const struct of_device_id *match;
	int ret;

	led_rgb_pdata = devm_kzalloc(&pdev->dev, sizeof(*led_rgb_pdata),
				     GFP_KERNEL);
	if (!led_rgb_pdata)
		goto err_pdata;

	match = of_match_device(of_match_ptr(led_of_match), &pdev->dev);
	if (match) {
		ret = led_rgb_probe_dt(pdev);
		if (ret)
			goto err_dt;
	} else
		goto err_dt;


	/* request LED RGB GPIO */
	gpio_request(led_rgb_pdata->gpio_r, "LED_R");
	gpio_request(led_rgb_pdata->gpio_g, "LED_G");
	gpio_request(led_rgb_pdata->gpio_b, "LED_B");
	/* turn off LED RGB */
	led_rgb_output(LED_R, 0);
	led_rgb_output(LED_G, 0);
	led_rgb_output(LED_B, 0);

	return 0;
err_dt:
	devm_kfree(&pdev->dev, led_rgb_pdata);
err_pdata:
	return -1;
}

static int led_rgb_remove(struct platform_device *pdev)
{
	/* turn off LED RGB */
	led_rgb_output(LED_R, 0);
	led_rgb_output(LED_G, 0);
	led_rgb_output(LED_B, 0);
	/* free LED RGB GPIO */
	gpio_free(led_rgb_pdata->gpio_r);
	gpio_free(led_rgb_pdata->gpio_g);
	gpio_free(led_rgb_pdata->gpio_b);

	devm_kfree(&pdev->dev, led_rgb_pdata);
	return 0;
}

static struct platform_driver led_platform_driver = {
	.probe = led_rgb_probe,
	.remove = led_rgb_remove,
	.driver = {
		   .name = "led-rgb",
		   .owner = THIS_MODULE,
#ifdef CONFIG_OF
		   .of_match_table = led_of_match,
#endif
		   },
};

static int __init led_init(void)
{
	return platform_driver_register(&led_platform_driver);
}

static void __exit led_exit(void)
{
	platform_driver_unregister(&led_platform_driver);
}

module_init(led_init);
module_exit(led_exit);

MODULE_DESCRIPTION("led:RGB");
MODULE_AUTHOR("Marvell");
MODULE_LICENSE("GPL V2");
