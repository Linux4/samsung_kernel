// SPDX-License-Identifier： GPL-2.0
//
// Sprd test thermal driver
//
// Copyright (C) 2020 Unisoc, Inc.
// Author: Di Shen <di.shen@unisoc.com>

#include <linux/cpu_cooling.h>
#include <linux/cpufreq.h>
#include <linux/cpumask.h>
#include <linux/debugfs.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/thermal.h>
#include <linux/of.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/string.h>
#include <linux/sysfs.h>
#include "sprd_thm_comm.h"

struct virtual_sensor {
	u16 sensor_id;
	int nsensor;
	struct sprd_thermal_zone *pzone;
	const char *sensor_names[THM_SENSOR_NUMBER];
	struct thermal_zone_device *thm_zones[THM_SENSOR_NUMBER];
};

static struct virtual_sensor temp_sensor;

static int dummy_temp = 25000;

/* sys I/F for thermal zone dummy_temp */
static ssize_t
dummy_temp_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", dummy_temp);
}

static ssize_t
dummy_temp_store(struct device *dev, struct device_attribute *attr,
		const char *buf, size_t count)
{
	if (kstrtoint(buf, 10, &dummy_temp))
		return -EINVAL;

	return count;
}
static DEVICE_ATTR(dummy_temp, 0644, dummy_temp_show, dummy_temp_store);

static int __get_dummy_temp(int *temp)
{
	*temp = dummy_temp;
	return 0;
}

static int virtual_temp_sensor_read(struct sprd_thermal_zone *pzone, int *temp)
{
	int ret = -EINVAL;

	if (!pzone || !temp)
		return ret;

	ret = __get_dummy_temp(temp);
	pr_debug("virtual_sensor_id:%d, temp:%d\n", pzone->id, *temp);

	return ret;
}

static struct thm_handle_ops test_soc_thm_ops = {
	.read_temp = virtual_temp_sensor_read,
};

static int test_temp_sen_parse_dt(struct device *dev,
				struct virtual_sensor *psoc_sensor)
{
	int count, i, ret = 0;
	struct device_node *np = dev->of_node;

	if (!np) {
		dev_err(dev, "device node not found\n");
		return -EINVAL;
	}

	count = of_property_count_strings(np, "sensor-names");
	if (count < 0) {
		dev_err(dev, "sensor names not found\n");
		return count;
	}

	psoc_sensor->nsensor = count;
	for (i = 0; i < count; i++) {
		ret = of_property_read_string_index(np, "sensor-names",
			i, &psoc_sensor->sensor_names[i]);
		if (ret) {
			dev_err(dev, "fail to get sensor-names\n");
			return ret;
		}
	}

	return ret;
}

static int test_thm_probe(struct platform_device *pdev)
{
	int i = 0;
	int ret = 0, sensor_id = 0;
	struct sprd_thermal_zone *pzone = NULL;
	struct virtual_sensor *psoc_sensor = &temp_sensor;
	struct device_node *np = pdev->dev.of_node;
	struct thermal_zone_device *tzd;

	if (!np) {
		dev_err(&pdev->dev, "device node not found\n");
		return -EINVAL;
	}

	ret = test_temp_sen_parse_dt(&pdev->dev, psoc_sensor);
	if (ret) {
		dev_err(&pdev->dev, "not found ptrips\n");
		return -EINVAL;
	}

	for (i = 0; i < psoc_sensor->nsensor; i++) {
		tzd =
		thermal_zone_get_zone_by_name(psoc_sensor->sensor_names[i]);
		if (IS_ERR(tzd)) {
			pr_err("get thermal zone %s failed\n",
					psoc_sensor->sensor_names[i]);
			return -ENOMEM;
		}

		/* create dummy_temp node */
		ret = device_create_file(&tzd->device, &dev_attr_dummy_temp);
		if (ret) {
			dev_err(&tzd->device, "dummy_temp sysfs node creation failed\n");
			return -ENODEV;
		}

		psoc_sensor->thm_zones[i] = tzd;
	}

	pzone = devm_kzalloc(&pdev->dev, sizeof(*pzone), GFP_KERNEL);
	if (!pzone)
		return -ENOMEM;

	mutex_init(&pzone->th_lock);

	sensor_id = of_alias_get_id(np, "thm-sensor");
	if (sensor_id < 0) {
		dev_err(&pdev->dev, "fail to get id\n");
		return -ENODEV;
	}
	pr_info("test sensor id %d\n", sensor_id);

	pzone->dev = &pdev->dev;
	pzone->id = sensor_id;
	pzone->ops = &test_soc_thm_ops;
	strlcpy(pzone->name, np->name, sizeof(pzone->name));

	ret = sprd_thermal_init(pzone);
	if (ret < 0) {
		dev_err(&pdev->dev,
			"virtual sensor sw init error id =%d\n", pzone->id);
		return ret;
	}

	psoc_sensor->pzone = pzone;
	platform_set_drvdata(pdev, pzone);

	return 0;
}

static int test_thm_remove(struct platform_device *pdev)
{
	struct sprd_thermal_zone *pzone = platform_get_drvdata(pdev);

	sprd_thermal_remove(pzone);
	return 0;
}

static const struct of_device_id test_thm_of_match[] = {
	{ .compatible = "sprd,virtual-thermal" },
	{},
};
MODULE_DEVICE_TABLE(of, test_thm_of_match);

static struct platform_driver sprd_test_thm_driver = {
	.probe = test_thm_probe,
	.remove = test_thm_remove,
	.driver = {
		   .owner = THIS_MODULE,
		   .name = "test-thermal",
		   .of_match_table = of_match_ptr(test_thm_of_match),
		   },
};

module_platform_driver(sprd_test_thm_driver);

MODULE_DESCRIPTION("sprd thermal driver");
MODULE_LICENSE("GPL v2");
