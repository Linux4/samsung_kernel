/*
 * Copyright (C) 2014 Samsung Electronics Co. Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#define pr_fmt(fmt) "usb_notifier: " fmt

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/delay.h>
#ifdef CONFIG_OF
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#endif
#include <linux/notifier.h>
#include <linux/usb_notify.h>

struct usb_notify *u_notify=NULL;

static int usb_notifier_probe(struct platform_device *pdev)
{
	int ret;

	u_notify = kzalloc(sizeof(struct usb_notify), GFP_KERNEL);
	if (!u_notify) {
		pr_err("unable to alloc usb_notify struct\n");
		ret = -ENOMEM;
		goto err;
	}

	ret = usb_notify_class_init(u_notify);
	if (ret) {
		pr_err("unable to do usb_notify_class_init\n");
		goto alloc_data_failed;
	}

	u_notify->otg_notifier = usb_otg_notifier;
	u_notify->notify_dev.name = "usb_control";
	u_notify->device_dev.name = "usb_device";
	ret = usb_notify_dev_register(&u_notify->notify_dev);
	if (ret < 0) {
		pr_err("usb_notify_dev_register is failed\n");
		goto notify_dev_register_fail;
	}

	ret = usb_device_dev_register(&u_notify->device_dev);
	if (ret < 0) {
		pr_err("usb_device_dev_register is failed\n");
		goto device_dev_register_fail;
	}
    u_notify->sec_whitelist_enable = 0;

	dev_info(&pdev->dev, "usb notifier probe\n");
	return 0;

device_dev_register_fail:
	usb_device_dev_unregister(&u_notify->device_dev);

notify_dev_register_fail:
	usb_notify_dev_unregister(&u_notify->notify_dev);

alloc_data_failed:
	kfree(u_notify);

err:
	return ret;
}

static int usb_notifier_remove(struct platform_device *pdev)
{
	usb_notify_dev_unregister(&u_notify->notify_dev);
	usb_notify_class_exit(u_notify);
	kfree(u_notify);
	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id usb_notifier_dt_ids[] = {
	{ .compatible = "samsung,usb-notifier",
	},
	{ },
};
MODULE_DEVICE_TABLE(of, usb_notifier_dt_ids);
#endif

static struct platform_driver usb_notifier_driver = {
	.probe		= usb_notifier_probe,
	.remove		= usb_notifier_remove,
	.driver		= {
		.name	= "usb_notifier",
		.owner	= THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table	= of_match_ptr(usb_notifier_dt_ids),
#endif
	},
};

static int __init usb_notifier_init(void)
{
	return platform_driver_register(&usb_notifier_driver);
}

static void __init usb_notifier_exit(void)
{
	platform_driver_unregister(&usb_notifier_driver);
}

late_initcall(usb_notifier_init);
module_exit(usb_notifier_exit);

MODULE_DESCRIPTION("USB Notifier");
MODULE_LICENSE("GPL");
