/*
 * Copyright (C) 2020 Spreadtrum Communications Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/cpumask.h>
#include <linux/debugfs.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/thermal.h>
#include <linux/of.h>
#include <linux/kernel.h>
#include "sprd_thm_comm.h"

enum sensor_type {TVIR_THM_AVG, TVIR_THM_MAX};

struct virtual_sensor {
	u16 sensor_id;
	int nsensor;
	struct sprd_thermal_zone *pzone;
	const char *sensor_names[THM_SENSOR_NUMBER];
	enum sensor_type type;
	struct thermal_zone_device *thm_zones[THM_SENSOR_NUMBER];
};

static int __sprd_get_temp(struct virtual_sensor *psensor, int *temp)
{
	int i, ret = 0;
	int virtual_temp = 0;
	int sum_temp = 0;
	int sensor_temp[THM_SENSOR_NUMBER];
	struct thermal_zone_device *tz = NULL;

	for (i = 0; i < psensor->nsensor; i++) {
		tz = psensor->thm_zones[i];
		if (!tz || IS_ERR(tz) || !tz->ops->get_temp) {
			pr_err("get thermal zone failed %d\n", i);
			return -ENODEV;
		}

		ret = tz->ops->get_temp(tz, &sensor_temp[i]);
		if (ret) {
			pr_err("get thermal %s temp failed\n", tz->type);
			return ret;
		}
	}

	/* get max or avg temperature of all thermal sensors */
	for (i = 0; i < psensor->nsensor; i++) {
		virtual_temp = max(virtual_temp, sensor_temp[i]);
		sum_temp += sensor_temp[i];
	}
	switch (psensor->type) {
	case TVIR_THM_AVG:
		*temp = sum_temp / psensor->nsensor;
		break;
	case TVIR_THM_MAX:
		*temp = virtual_temp;
		break;
	default:
		pr_info("sensor_type not support\n");
		break;
	}
	return ret;
}

static int sprd_temp_sensor_read(struct sprd_thermal_zone *pzone, int *temp)
{
	int ret = -EINVAL;
	struct virtual_sensor *pvirtual_sensor = NULL;

	pvirtual_sensor = (struct virtual_sensor *)dev_get_drvdata(pzone->dev);
	if (!pvirtual_sensor || !pzone || !temp)
		return ret;
	ret = __sprd_get_temp(pvirtual_sensor, temp);
	pr_debug("virtual_sensor_id:%d, temp:%d\n", pzone->id, *temp);

	return ret;
}

struct thm_handle_ops sprd_virtual_thm_ops = {
	.read_temp = sprd_temp_sensor_read,
};

static int sprd_temp_sen_parse_dt(struct device *dev, struct virtual_sensor *pvirtual_sensor)
{
	int count, i, ret = 0;
	const char *sensor_type;
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
	pvirtual_sensor->nsensor = count;
	for (i = 0; i < count; i++) {
		ret = of_property_read_string_index(np, "sensor-names", i,
						&pvirtual_sensor->sensor_names[i]);
		if (ret) {
			dev_err(dev, "fail to get sensor-names\n");
			return ret;
		}
	}

	ret = of_property_read_string(np, "sensor-type", &sensor_type);
	if (ret) {
		dev_err(dev, "fail to get sensor-type\n");
		return ret;
	}
	if (!strcmp("avg-temp", sensor_type))
		pvirtual_sensor->type = TVIR_THM_AVG;
	else if (!strcmp("max-temp", sensor_type))
		pvirtual_sensor->type = TVIR_THM_MAX;

	return ret;
}

static int sprd_virtual_thm_probe(struct platform_device *pdev)
{
	int i = 0;
	int ret = 0, sensor_id = 0;
	struct sprd_thermal_zone *pzone = NULL;
	struct virtual_sensor *pvirtual_sensor = NULL;
	struct device_node *np = pdev->dev.of_node;

	if (!np) {
		dev_err(&pdev->dev, "device node not found\n");
		return -EINVAL;
	}

	pvirtual_sensor = devm_kzalloc(&pdev->dev, sizeof(*pvirtual_sensor), GFP_KERNEL);
	if (!pvirtual_sensor)
		return -ENOMEM;

	ret = sprd_temp_sen_parse_dt(&pdev->dev, pvirtual_sensor);
	if (ret) {
		dev_err(&pdev->dev, "not found ptrips\n");
		return -EINVAL;
	}

	for (i = 0; i < pvirtual_sensor->nsensor; i++) {
		pvirtual_sensor->thm_zones[i] =
		thermal_zone_get_zone_by_name(pvirtual_sensor->sensor_names[i]);
		if (IS_ERR(pvirtual_sensor->thm_zones[i])) {
			pr_err("get thermal zone %s failed\n", pvirtual_sensor->sensor_names[i]);
			return -ENOMEM;
		}
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
	dev_info(&pdev->dev, "sprd virtual sensor id %d\n", sensor_id);

	pzone->dev = &pdev->dev;
	pzone->id = sensor_id;
	pzone->ops = &sprd_virtual_thm_ops;
	strlcpy(pzone->name, np->name, sizeof(pzone->name));

	ret = sprd_thermal_init(pzone);
	if (ret < 0) {
		dev_err(&pdev->dev,
			"virtual sensor sw init error id =%d\n", pzone->id);
		return ret;
	}

	pvirtual_sensor->pzone = pzone;
	platform_set_drvdata(pdev, pvirtual_sensor);

	return 0;
}

static int sprd_virtual_thm_remove(struct platform_device *pdev)
{
	struct virtual_sensor *pvirtual_sensor = platform_get_drvdata(pdev);
	struct sprd_thermal_zone *pzone = pvirtual_sensor->pzone;

	sprd_thermal_remove(pzone);
	return 0;
}

static const struct of_device_id virtual_thermal_of_match[] = {
	{ .compatible = "sprd,virtual-thermal" },
	{},
};
MODULE_DEVICE_TABLE(of, virtual_thermal_of_match);

static struct platform_driver sprd_virtual_thermal_driver = {
	.probe = sprd_virtual_thm_probe,
	.remove = sprd_virtual_thm_remove,
	.driver = {
		   .owner = THIS_MODULE,
		   .name = "virtual-thermal",
		   .of_match_table = of_match_ptr(virtual_thermal_of_match),
		   },
};

static int __init sprd_virtual_thermal_init(void)
{
	return platform_driver_register(&sprd_virtual_thermal_driver);
}

static void __exit sprd_virtual_thermal_exit(void)
{
	platform_driver_unregister(&sprd_virtual_thermal_driver);
}

device_initcall_sync(sprd_virtual_thermal_init);
module_exit(sprd_virtual_thermal_exit);

MODULE_DESCRIPTION("sprd thermal driver");
MODULE_LICENSE("GPL");
