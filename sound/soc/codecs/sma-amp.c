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
#include <sound/ff_prot_spk.h>

#include "sma1305.h"

#define SMA_AMP_CLASS_NAME "sma"
#define SMA_AMP_DEV_NAME "sma-amp"
#define MONO_SPK			1
#define STEREO_SPK			2

#ifdef CONFIG_SND_SOC_APS_ALGO
struct big_data {
	uint32_t temp_max;
	uint32_t temp_max_persist;
	uint32_t temp_over_count;
};
#endif

struct sma_amp_t {
	struct class *class;
	struct device *dev;
	struct mutex lock;
#ifdef CONFIG_SND_SOC_APS_ALGO
	struct big_data b_data[MAX_CHANNELS];
#endif
	uint8_t spk_count;
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

/* BigData Related Codes Start */
#ifdef CONFIG_SND_SOC_APS_ALGO
void sma_amp_update_big_data(void)
{
	uint8_t iter = 0;
	int32_t data[8] = {0};
	uint32_t param_id = 0;
	int32_t ret = 0;

	if (!sma_amp) {
		pr_err("[ID-APS:%s] memory not allocated yet for sma_amp",
			__func__);
	}

	for (iter = 0; iter < sma_amp->spk_count; iter++) {
		/* Reset data */
		memset(data, 0, 8*sizeof(int32_t));
		param_id = (SMA_APS_TEMP_STAT)|((iter+1)<<24)|
			((sizeof(data)/sizeof(int32_t))<<16);
		ret = afe_ff_prot_algo_ctrl((int *)(&(data[0])), param_id,
				SMA_GET_PARAM, sizeof(data));
		if (ret < 0) {
			pr_err("[ID-APS:%s] Failed to get Temperature Stats",
					__func__);
		} else {
			pr_err("[ID-APS:%s] Tmax[%d] %d, TOcount[%d] %d\n",
				__func__, iter, data[1],
					iter, data[3]);

			/* Update Temperature Data */
			sma_amp->b_data[iter].temp_max =
			(data[1] > sma_amp->b_data[iter].temp_max)
			? data[1]:sma_amp->b_data[iter].temp_max;
			sma_amp->b_data[iter].temp_max_persist =
			(data[1] > sma_amp->b_data[iter].temp_max_persist)
			? data[1]:sma_amp->b_data[iter].temp_max_persist;
			sma_amp->b_data[iter].temp_over_count += data[3];
		}
	}
}

static ssize_t temp_max_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	ssize_t ret;

	ret = sprintf(buf, "%d\n", sma_amp->b_data[0].temp_max);
	sma_amp->b_data[0].temp_max = 0;

	return ret;
}

static ssize_t temp_max_persist_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	ssize_t ret;

	ret = sprintf(buf, "%d\n", sma_amp->b_data[0].temp_max_persist);

	return ret;
}

static ssize_t temp_over_count_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	ssize_t ret;

	ret = sprintf(buf, "%d\n", sma_amp->b_data[0].temp_over_count);
	sma_amp->b_data[0].temp_over_count = 0;

	return ret;
}

static ssize_t temp_max_r_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	ssize_t ret;

	ret = sprintf(buf, "%d\n", sma_amp->b_data[1].temp_max);
	sma_amp->b_data[1].temp_max = 0;

	return ret;
}

static ssize_t temp_max_persist_r_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	ssize_t ret;

	ret = sprintf(buf, "%d\n", sma_amp->b_data[1].temp_max_persist);

	return ret;
}

static ssize_t temp_over_count_r_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	ssize_t ret;

	ret = sprintf(buf, "%d\n", sma_amp->b_data[1].temp_over_count);
	sma_amp->b_data[1].temp_over_count = 0;

	return ret;
}

static DEVICE_ATTR_RO(temp_max);
static DEVICE_ATTR_RO(temp_max_persist);
static DEVICE_ATTR_RO(temp_over_count);

static DEVICE_ATTR_RO(temp_max_r);
static DEVICE_ATTR_RO(temp_max_persist_r);
static DEVICE_ATTR_RO(temp_over_count_r);
#endif
/* BigData Related Codes End */

static struct attribute *sma_amp_attr[] = {
#ifdef CONFIG_SMA1305_FACTORY_RECOVERY_SYSFS
	&dev_attr_reinit.attr,
#endif
#ifdef CONFIG_SND_SOC_APS_ALGO
	&dev_attr_temp_max.attr,
	&dev_attr_temp_max_persist.attr,
	&dev_attr_temp_over_count.attr,
#endif
};

static struct attribute *sma_amp_attr_r[] = {
#ifdef CONFIG_SND_SOC_APS_ALGO
	&dev_attr_temp_max_r.attr,
	&dev_attr_temp_max_persist_r.attr,
	&dev_attr_temp_over_count_r.attr,
#endif
};

static struct attribute *sma_amp_attr_m[
	ARRAY_SIZE(sma_amp_attr) +
	ARRAY_SIZE(sma_amp_attr_r) + 1] = {NULL};

static struct attribute_group sma_amp_attr_group = {
	.attrs = sma_amp_attr_m,
};

static int sma_amp_probe(struct platform_device *pdev)
{
	int ret;

	dev_info(&pdev->dev, "%s\n", __func__);

	sma_amp = kzalloc(sizeof(struct sma_amp_t), GFP_KERNEL);
	if (sma_amp == NULL)
		return -ENOMEM;

	memcpy(sma_amp_attr_m, sma_amp_attr, sizeof(sma_amp_attr));
	memcpy(sma_amp_attr_m + ARRAY_SIZE(sma_amp_attr),
			sma_amp_attr_r, sizeof(sma_amp_attr_r));

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
	sma_amp->spk_count = STEREO_SPK;

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

