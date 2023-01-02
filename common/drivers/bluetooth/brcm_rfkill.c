/*
 * Copyright (C) 2011 Google, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/rfkill.h>
#include <linux/gpio.h>
#include <linux/ioport.h>
#include <linux/clk.h>
#include <mach/board.h>
#include <linux/of_gpio.h>


static struct rfkill *bt_rfk;
static const char bt_name[] = "bluetooth";

static  unsigned long bt_power;
static  unsigned long bt_reset;

static void bt_clk_init(void)
{
	struct clk *bt_clk;
	bt_clk = clk_get(NULL, "clk_aux0");
	if (IS_ERR(bt_clk)) {
		printk("clock: failed to get clk_aux0\n");
	}
	clk_set_rate(bt_clk, 32000);
	clk_enable(bt_clk);
}


static void getIoResource(struct platform_device  *pdev)
{
	struct resource *res;

	printk("rfkill get gpio\n");

	res = platform_get_resource_byname(pdev, IORESOURCE_IO,"bt_power");
	if (!res) {
		printk("couldn't bt_power gpio\n");
	}
        else
	bt_power = res->start;

	res = platform_get_resource_byname(pdev, IORESOURCE_IO,"bt_reset");
	if (!res) {
		printk("couldn't find bt_reset gpio\n");
	}
        else
	bt_reset = res->start;

	printk("bt_reset = %ld, bt_power = %ld\n", bt_reset, bt_power);
}

static void getIoResourceDT(struct platform_device  *pdev)
{
	struct device_node *np = pdev->dev.of_node;

	bt_power = of_get_gpio(np, 0);
	if (bt_power < 0) {
		printk("couldn't bt_power gpio\n");
	}
        bt_reset = of_get_gpio(np, 1);
        if (bt_reset < 0) {
                printk("couldn't bt_reset gpio\n");
        }

	printk("bt_reset = %ld, bt_power = %ld\n", bt_reset, bt_power);
}

static int bluetooth_set_power(void *data, bool blocked)
{
	printk("%s: block=%d\n",__func__, blocked);

	if (!blocked) {
		gpio_direction_output(bt_power, 0);
		gpio_direction_output(bt_reset, 0);
		mdelay(10);
		gpio_direction_output(bt_power, 1);
		gpio_direction_output(bt_reset, 1);
		mdelay(150);
                bt_clk_init();
	} else {
 		gpio_direction_output(bt_power, 0);
		gpio_direction_output(bt_reset, 0);
		mdelay(10);
	}

	return 0;
}

static struct rfkill_ops rfkill_bluetooth_ops = {
	.set_block = bluetooth_set_power,
};

static void rfkill_gpio_init(void)
{
	if(gpio_request(bt_power,"bt_power")){
		printk("request bt power fail\n");
	}
	if(gpio_request(bt_reset,"bt_reset")){
		printk("request bt reset fail\n");
	}
}

static void rfkill_gpio_deinit(void)
{
	gpio_free(bt_power);
	gpio_free(bt_reset);
}
static int rfkill_bluetooth_probe(struct platform_device *pdev)
{

	int rc = 0;
	bool default_state = true;

	printk(KERN_INFO "-->%s\n", __func__);
#ifndef CONFIG_OF
	getIoResource(pdev);
#else
	if (pdev->dev.of_node)
		getIoResourceDT(pdev);
#endif
	bt_rfk = rfkill_alloc(bt_name, &pdev->dev, RFKILL_TYPE_BLUETOOTH,
	   &rfkill_bluetooth_ops, NULL);
	if (!bt_rfk) {
	 rc = -ENOMEM;
	 goto err_rfkill_alloc;
	}
        rfkill_gpio_init();
	/* userspace cannot take exclusive control */
	rfkill_init_sw_state(bt_rfk,false);
	rc = rfkill_register(bt_rfk);
	if (rc)
		goto err_rfkill_reg;

	rfkill_set_sw_state(bt_rfk,true);
	bluetooth_set_power(NULL, default_state);

	printk(KERN_INFO "<--%s\n", __func__);
	return 0;

err_rfkill_reg:
	rfkill_destroy(bt_rfk);
err_rfkill_alloc:
	return rc;
}

static int rfkill_bluetooth_remove(struct platform_device *dev)
{
	printk(KERN_INFO "-->%s\n", __func__);
	rfkill_gpio_deinit();
	rfkill_unregister(bt_rfk);
	rfkill_destroy(bt_rfk);
	printk(KERN_INFO "<--%s\n", __func__);
	return 0;
}
#ifdef CONFIG_OF
static const struct of_device_id rfkill_of_match[] = {
       { .compatible = "broadcom,rfkill", },
       { }
};

MODULE_DEVICE_TABLE(of, rfkill_of_match);
#endif

static struct platform_driver rfkill_bluetooth_driver = {
	.probe  = rfkill_bluetooth_probe,
	.remove = rfkill_bluetooth_remove,
	.driver = {
		.name = "rfkill",
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = rfkill_of_match,
#endif
	},
};

static int __init rfkill_bluetooth_init(void)
{
	printk(KERN_INFO "-->%s\n", __func__);
	return platform_driver_register(&rfkill_bluetooth_driver);
}

static void __exit rfkill_bluetooth_exit(void)
{
	printk(KERN_INFO "-->%s\n", __func__);
	platform_driver_unregister(&rfkill_bluetooth_driver);
}

//late_initcall(rfkill_bluetooth_init);
module_init(rfkill_bluetooth_init);
module_exit(rfkill_bluetooth_exit);
MODULE_DESCRIPTION("bluetooth rfkill");
MODULE_AUTHOR("Yale Wu <ye.wu@spreadtrum.com>");
MODULE_LICENSE("GPL");

