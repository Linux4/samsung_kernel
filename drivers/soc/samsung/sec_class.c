/* Copyright (c) 2011-2014, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/platform_device.h>
#include <linux/err.h>

struct sec_class_device {
	struct list_head list;
	bool initialized;
	const char *name;
};

static int sec_class_dev_probe(struct platform_device *pdev)
{
	int ret = 0;
	return ret;
}

static int sec_class_dev_remove(struct platform_device *pdev)
{
	struct sec_class_device *dev = platform_get_drvdata(pdev);
	list_del(&dev->list);
	return 0;
}

static struct of_device_id sec_class_match_table[] = {
	{.compatible = "qcom,sec_class"},
	{},
};

static struct platform_driver sec_class_device_driver = {
	.probe = sec_class_dev_probe,
	.remove = sec_class_dev_remove,
	.driver = {
		.name = "sec_class",
		.owner = THIS_MODULE,
		.of_match_table = sec_class_match_table,
	},
};

/**
 * sec_class_init(): Device tree initialization function
 */

struct class *sec_class;
EXPORT_SYMBOL(sec_class);
int __init sec_class_init(void)
{
	static bool registered;
	if (registered)
		return 0;
	registered = true;

	pr_info("samsung sys class init.\n");

	sec_class = class_create(THIS_MODULE, "sec");

	if (IS_ERR(sec_class)) {
		pr_err("Failed to create class(sec)!\n");
		return 0;
	}
	
	return platform_driver_register(&sec_class_device_driver);
}

arch_initcall(sec_class_init);
