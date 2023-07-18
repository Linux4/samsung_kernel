// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Core file for Samsung EXYNOS Power Domain Manager driver
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "camerapp-pd-mgr.h"

static struct camerapp_pd_mgr_dev *pd_mgr;

int camerapp_pd_mgr_on(void)
{
	int ret = -ENODEV;

	if (!pd_mgr)
		return ret;

	if (pd_mgr->on == OFF) {
		ret = pm_runtime_get_sync(pd_mgr->dev);
		if (ret < 0) {
			dev_err(pd_mgr->dev,
				"failed to get_sync. ret(%d)\n", ret);
			return ret;
		}
		pd_mgr->on = ON;
		dev_info(pd_mgr->dev, "State Change[OFF -> ON]\n");
	} else
		dev_info(pd_mgr->dev, "already on (%d)\n", pd_mgr->on);

	return ret;
}
EXPORT_SYMBOL(camerapp_pd_mgr_on);

int camerapp_pd_mgr_off(void)
{
	int ret = -ENODEV;

	if (!pd_mgr)
		return ret;

	if (pd_mgr->on == ON) {
		ret = pm_runtime_put_sync(pd_mgr->dev);
		if (ret < 0) {
			dev_err(pd_mgr->dev,
				"failed to put_sync. ret(%d)\n", ret);
			return ret;
		}
		pd_mgr->on = OFF;
		dev_info(pd_mgr->dev, "State Change[ON->OFF]\n");
	} else
		dev_info(pd_mgr->dev, "already off (%d)\n", pd_mgr->on);

	return ret;
}
EXPORT_SYMBOL(camerapp_pd_mgr_off);

static ssize_t camerapp_pd_enable_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "slave state(%d)\n", pd_mgr->on);
}

static ssize_t camerapp_pd_enable_store(struct device *dev,
				 struct device_attribute *attr,
				 const char *buf, size_t count)
{
	if (buf[0] == '1') {
		if (camerapp_pd_mgr_on() < 0)
			dev_err(dev, "camerapp_pd_mgr_on fail\n");
		else
			dev_info(dev, "camerapp_pd_mgr_on success\n");
	} else {
		if (camerapp_pd_mgr_off() < 0)
			dev_err(dev, "camerapp_pd_mgr_off fail\n");
		else
			dev_info(dev, "camerapp_pd_mgr_off success\n");
	}
	return count;
}

static DEVICE_ATTR(camerapp_pd_enable, 0644,
	camerapp_pd_enable_show, camerapp_pd_enable_store);

static struct attribute *camerapp_pd_mgr_entries[] = {
	&dev_attr_camerapp_pd_enable.attr,
	NULL,
};

static struct attribute_group camerapp_pd_mgr_attr_group = {
	.name	= "pd_control",
	.attrs	= camerapp_pd_mgr_entries,
};

#ifdef CONFIG_PM
static int camerapp_pd_mgr_pm_suspend(struct device *dev)
{
	struct camerapp_pd_mgr_dev *data = dev_get_drvdata(dev);
	int err = 0;

	if (data->on == ON) {
		err = pm_runtime_put_sync(data->dev);
		if (err < 0)
			dev_err(data->dev,
				"failed to put_sync. ret(%d)\n", err);
	}
	return err;
}

static int camerapp_pd_mgr_pm_resume(struct device *dev)
{
	struct camerapp_pd_mgr_dev *data = dev_get_drvdata(dev);
	int err = 0;

	if (data->on == ON) {
		err = pm_runtime_get_sync(data->dev);
		if (err < 0)
			dev_err(data->dev,
				"failed to get_sync. ret(%d)\n", err);
	}
	return err;
}

static const struct dev_pm_ops camerapp_pd_mgr_pm_ops = {
	.suspend = camerapp_pd_mgr_pm_suspend,
	.resume = camerapp_pd_mgr_pm_resume
};
#endif

int camerapp_pd_mgr_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct device_node *np;

	pd_mgr = devm_kzalloc(&pdev->dev,
		sizeof(struct camerapp_pd_mgr_dev), GFP_KERNEL);
	if (!pd_mgr) {
		dev_err(&pdev->dev, "memory alloc failed\n");
		return -ENOMEM;
	}

	np = pdev->dev.of_node;
	ret = of_property_read_u32(np, "slave_addr", &pd_mgr->slave_addr);
	if (ret) {
		dev_err(&pdev->dev, "failed to get slave address\n");
		goto err;
	}

	pd_mgr->dev = &pdev->dev;
	pd_mgr->on = OFF;

	platform_set_drvdata(pdev, pd_mgr);
	pm_runtime_enable(&pdev->dev);
	ret = sysfs_create_group(&pdev->dev.kobj, &camerapp_pd_mgr_attr_group);
	if (ret < 0) {
		dev_err(&pdev->dev, "failed to create sysfs group\n");
		goto err;
	}

	pr_info("%s successfully done.\n", __func__);
	return 0;
err:
	devm_kfree(&pdev->dev, pd_mgr);
	platform_set_drvdata(pdev, NULL);
	return ret;
}

int camerapp_pd_mgr_remove(struct platform_device *pdev)
{
	devm_kfree(&pdev->dev, pd_mgr);
	platform_set_drvdata(pdev, NULL);
	return 0;
}

void camerapp_pd_mgr_shutdown(struct platform_device *pdev)
{
}

const struct of_device_id exynos_camerapp_pd_mgr_match[] = {
	{
		.compatible = "samsung,exynos-camerapp-pd-mgr",
	},
	{},
};
MODULE_DEVICE_TABLE(of, exynos_camerapp_pd_mgr_match);

struct platform_driver camerapp_pd_mgr_driver = {
	.probe		= camerapp_pd_mgr_probe,
	.remove		= camerapp_pd_mgr_remove,
	.shutdown	= camerapp_pd_mgr_shutdown,
	.driver = {
		.name	= "camerapp-pd-mgr",
		.owner	= THIS_MODULE,
#if defined(CONFIG_PM)
		.pm	= &camerapp_pd_mgr_pm_ops,
#endif
		.of_match_table = of_match_ptr(exynos_camerapp_pd_mgr_match),
	}
};

module_platform_driver(camerapp_pd_mgr_driver);

MODULE_AUTHOR("SamsungLSI Camera");
MODULE_DESCRIPTION("EXYNOS CameraPP PD Manager driver");
MODULE_LICENSE("GPL");
