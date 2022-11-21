// SPDX-License-Identifier: GPL-2.0-or-later
/* Extended support for SMA AMP
 *
 * Copyright 2021 Silicon Mitus Corporation / Iron Device Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/device.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/slab.h>

#include "sma1305.h"

#define SMA_AMP_CLASS_NAME "sma"
#define SMA_AMP_DEV_NAME "sma-amp"

struct sma_amp_t {
	struct class *class;
	struct device *dev;
	struct mutex lock;
};

static struct sma_amp_t *sma_amp;

#ifdef CONFIG_SMA1305_FACTORY_RECOVERY_SYSFS
static ssize_t reinit_show(struct device *dev,
	struct device_attribute *devattr, char *buf)
{
	return sprintf(buf, "\n");
}

static ssize_t reinit_store(struct device *dev,
	struct device_attribute *devattr, const char *buf, size_t count)
{
	struct snd_soc_component *amp_component;
	long reinit;
	int ret;

	ret = kstrtol(buf, 10, &reinit);
	if (ret)
		return (ssize_t)count;

	mutex_lock(&sma_amp->lock);

	if (sma_amp) {
		get_sma_amp_component(&amp_component);
		if (amp_component) {
			if (reinit == 1) {
				sma1305_reinit(amp_component);
				dev_info(amp_component->dev, "%s reinit done\n",
					SMA_AMP_DEV_NAME);
			}
		} else
			dev_err(dev, "sma-amp component is not configured\n");
	} else
		dev_err(dev, "sma-amp is not configured\n");

	mutex_unlock(&sma_amp->lock);

	return (ssize_t)count;
}

static DEVICE_ATTR_RW(reinit);
#endif

static struct attribute *sma_amp_attr[] = {
#ifdef CONFIG_SMA1305_FACTORY_RECOVERY_SYSFS
	&dev_attr_reinit.attr,
#endif
	NULL,
};

static struct attribute_group sma_amp_attr_group = {
	.attrs = sma_amp_attr,
};

static int sma_amp_probe(struct platform_device *pdev)
{
	int ret;

	dev_info(&pdev->dev, "%s\n", __func__);

	sma_amp = kzalloc(sizeof(struct sma_amp_t), GFP_KERNEL);
	if (sma_amp == NULL)
		return -ENOMEM;

	sma_amp->class = class_create(THIS_MODULE,
					SMA_AMP_CLASS_NAME);

	if (IS_ERR(sma_amp->class)) {
		dev_err(&pdev->dev, "Failed to register [%s] class\n",
				SMA_AMP_CLASS_NAME);
		return -EINVAL;
	}

	/* Create sma sysfs attributes */
	sma_amp->dev = device_create(sma_amp->class, NULL, 0, NULL,
			SMA_AMP_DEV_NAME);

	if (IS_ERR(sma_amp->dev)) {
		dev_err(&pdev->dev, "Failed to create [%s] device\n",
				SMA_AMP_DEV_NAME);
		class_destroy(sma_amp->class);
		kfree(sma_amp);
		return -ENODEV;
	}

	ret = sysfs_create_group(&sma_amp->dev->kobj,
			&sma_amp_attr_group);

	if (ret) {
		dev_err(&pdev->dev,
			"failed to create sysfs group [%d]\n", ret);
		kfree(sma_amp);
		return ret;
	}

	mutex_init(&sma_amp->lock);

	return ret;
}

static int sma_amp_remove(struct platform_device *pdev)
{
	kfree(sma_amp);

	return 0;
}

static const struct of_device_id sma_amp_of_match[] = {
	{ .compatible = "sma-amp", },
	{},
};
MODULE_DEVICE_TABLE(of, sma_amp_of_match);

static struct platform_driver sma_amp_conf = {
	.probe = sma_amp_probe,
	.remove = sma_amp_remove,
	.driver = {
		.name = "sma-amp",
		.owner = THIS_MODULE,
		.of_match_table = sma_amp_of_match,
	},
};

module_platform_driver(sma_amp_conf);

MODULE_DESCRIPTION("ALSA SoC SMA sysfs driver");
MODULE_AUTHOR("Gyuhwa Park <gyuhwa.park@irondevice.com>");
MODULE_LICENSE("GPL v2");

