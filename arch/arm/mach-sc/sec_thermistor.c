/* sec_thermistor.c
 *
 * Copyright (C) 2011 Samsung Electronics
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
#include <linux/module.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <mach/sec_thermistor.h>
#include <mach/adc.h>
#include <mach/adi.h>

#define ADC_SAMPLING_CNT	7

enum {
	AP_TEMP  = 0,
	DCXO_TEMP = 1
};

static struct device *sec_thermistor_dev;
extern struct class *sec_class;

struct sec_therm_info {
	struct device *dev;
	struct sec_therm_platform_data *pdata;
};

extern struct sec_therm_platform_data * fill_therm_pdata(struct platform_device *);

static int sec_therm_get_adc_data(struct sec_therm_info *info, int thermistor)
{
	int adc_data;
	int adc_max = 0, adc_min = 0, adc_total = 0;
	int i;
	unsigned int adc_ch = ADC_CHANNEL_0;

	if (thermistor == AP_TEMP)
		adc_ch = info->pdata->ap_channel;
	else if (thermistor == DCXO_TEMP)
		adc_ch = info->pdata->dcxo_channel;

	for (i = 0; i < ADC_SAMPLING_CNT; i++) {
		adc_data = sci_adc_get_value(adc_ch,0);

		if (adc_data < 0) {
			dev_err(info->dev, "%s : err(%d) returned, skip read\n",
				__func__, adc_data);
			return adc_data;
		}

		if (i != 0) {
			if (adc_data > adc_max)
				adc_max = adc_data;
			else if (adc_data < adc_min)
				adc_min = adc_data;
		} else {
			adc_max = adc_data;
			adc_min = adc_data;
		}
		adc_total += adc_data;
	}

	return (adc_total - adc_max - adc_min) / (ADC_SAMPLING_CNT - 2);
}

static int convert_adc_to_temper(struct sec_therm_info *info, unsigned int adc, int thermistor)
{
	int low = 0;
	int high = 0;
	int mid = 0;
	int temp = 0;
	
	if (thermistor == AP_TEMP) {
		if (!info->pdata->ap_adc_table || !info->pdata->ap_adc_arr_size) {
			/* using fake temp */
			return 300;
		}
		if (info->pdata->ap_adc_table[0].adc >= adc) {
			temp = info->pdata->ap_adc_table[0].temperature;
			goto temp_by_adc_goto;
		} else if (info->pdata->ap_adc_table[info->pdata->ap_adc_arr_size-1].adc <= adc) {
			temp = info->pdata->ap_adc_table[info->pdata->ap_adc_arr_size-1].temperature;
			goto temp_by_adc_goto;
		}
		high = info->pdata->ap_adc_arr_size - 1;
		
		while (low <= high) {
			mid = (low + high) / 2;
			if (info->pdata->ap_adc_table[mid].adc > adc)
				high = mid - 1;
			else if (info->pdata->ap_adc_table[mid].adc < adc)
				low = mid + 1;
			else {
				temp = info->pdata->ap_adc_table[mid].temperature;
				goto temp_by_adc_goto;
			}
		}
		temp = info->pdata->ap_adc_table[high].temperature;
		temp +=
			(((int)info->pdata->ap_adc_table[low].temperature -
			(int)info->pdata->ap_adc_table[high].temperature) *
			((int)adc - (int)info->pdata->ap_adc_table[high].adc)) /
			((int)info->pdata->ap_adc_table[low].adc - (int)info->pdata->ap_adc_table[high].adc);
	}
	else if (thermistor == DCXO_TEMP) {
		if (!info->pdata->dcxo_adc_table || !info->pdata->dcxo_adc_arr_size) {
			/* using fake temp */
			return 300;
		}
		if (info->pdata->dcxo_adc_table[0].adc >= adc) {
			temp = info->pdata->dcxo_adc_table[0].temperature;
			goto temp_by_adc_goto;
		} else if (info->pdata->dcxo_adc_table[info->pdata->dcxo_adc_arr_size-1].adc <= adc) {
			temp = info->pdata->dcxo_adc_table[info->pdata->dcxo_adc_arr_size-1].temperature;
			goto temp_by_adc_goto;
		}
		high = info->pdata->dcxo_adc_arr_size - 1;
		
		while (low <= high) {
			mid = (low + high) / 2;
			if (info->pdata->dcxo_adc_table[mid].adc > adc)
				high = mid - 1;
			else if (info->pdata->dcxo_adc_table[mid].adc < adc)
				low = mid + 1;
			else {
				temp = info->pdata->dcxo_adc_table[mid].temperature;
				goto temp_by_adc_goto;
			}
		}
		temp = info->pdata->dcxo_adc_table[high].temperature;
		temp +=
			(((int)info->pdata->dcxo_adc_table[low].temperature -
			(int)info->pdata->dcxo_adc_table[high].temperature) *
			((int)adc - (int)info->pdata->dcxo_adc_table[high].adc)) /
			((int)info->pdata->dcxo_adc_table[low].adc - (int)info->pdata->dcxo_adc_table[high].adc);
	}
	
	temp_by_adc_goto:
	return temp;
}

static ssize_t sec_therm_show_temperature(struct device *dev,
				   struct device_attribute *attr,
				   char *buf)
{
	struct sec_therm_info *info = dev_get_drvdata(dev);
	int adc, temp;

	adc = sec_therm_get_adc_data(info, AP_TEMP);

	if (adc < 0)
		return adc;
	else
		temp = convert_adc_to_temper(info, adc, AP_TEMP);

	return sprintf(buf, "%d\n", temp);
}

static ssize_t sec_therm_show_temp_adc(struct device *dev,
				   struct device_attribute *attr,
				   char *buf)
{
	struct sec_therm_info *info = dev_get_drvdata(dev);
	int adc;

	adc = sec_therm_get_adc_data(info, AP_TEMP);

	return sprintf(buf, "%d\n", adc);
}

static ssize_t sec_therm_show_dcxo_temperature(struct device *dev,
				   struct device_attribute *attr,
				   char *buf)
{
	struct sec_therm_info *info = dev_get_drvdata(dev);
	int adc, temp;

	adc = sec_therm_get_adc_data(info, DCXO_TEMP);

	if (adc < 0)
		return adc;
	else
		temp = convert_adc_to_temper(info, adc, DCXO_TEMP);

	return sprintf(buf, "%d\n", temp);
}

static ssize_t sec_therm_show_dcxo_temp_adc(struct device *dev,
				   struct device_attribute *attr,
				   char *buf)
{
	struct sec_therm_info *info = dev_get_drvdata(dev);
	int adc;

	adc = sec_therm_get_adc_data(info, DCXO_TEMP);

	return sprintf(buf, "%d\n", adc);
}

static DEVICE_ATTR(temperature, S_IRUGO, sec_therm_show_temperature, NULL);
static DEVICE_ATTR(temp_adc, S_IRUGO, sec_therm_show_temp_adc, NULL);
static DEVICE_ATTR(dcxo_temperature, S_IRUGO, sec_therm_show_dcxo_temperature, NULL);
static DEVICE_ATTR(dcxo_temp_adc, S_IRUGO, sec_therm_show_dcxo_temp_adc, NULL);

static struct attribute *sec_therm_attributes[] = {
	&dev_attr_temperature.attr,
	&dev_attr_temp_adc.attr,
	&dev_attr_dcxo_temperature.attr,
	&dev_attr_dcxo_temp_adc.attr,
	NULL
};

static const struct attribute_group sec_therm_group = {
	.attrs = sec_therm_attributes,
};

static int sec_therm_probe(struct platform_device *pdev)
{
	struct sec_therm_platform_data *pdata = fill_therm_pdata(pdev);
	struct sec_therm_info *info;
	int ret = 0;

	dev_info(&pdev->dev, "%s: SEC Thermistor Driver Loading\n", __func__);

	info = devm_kzalloc(&pdev->dev, sizeof(*info), GFP_KERNEL);
	if (!info)
		return -ENOMEM;


	info->dev = &pdev->dev;
	info->pdata = pdata;

	platform_set_drvdata(pdev, info);

    BUG_ON(!sec_class);
	info->dev = device_create(sec_class, NULL, 0, info, "sec-thermistor");
	
	ret = sysfs_create_group(&info->dev->kobj, &sec_therm_group);
	if (ret) {
		dev_err(info->dev,
			"failed to create sysfs attribute group\n");

		kfree(info);
	}
	return ret;
}

static int __exit sec_therm_remove(struct platform_device *pdev)
{
	struct sec_therm_info *info = platform_get_drvdata(pdev);

	if (!info)
		return 0;

	sysfs_remove_group(&info->dev->kobj, &sec_therm_group);

	kfree(info);

	return 0;
}

static const struct of_device_id sec_therm_dt_match[] = {
	{ .compatible = "sec,thermistor" },
	{ }
};

MODULE_DEVICE_TABLE(of, sec_therm_dt_match);

static struct platform_driver sec_thermistor_driver = {
	.driver = {
		.name = "sec-thermistor",
		.owner = THIS_MODULE,
		.of_match_table = sec_therm_dt_match,
	},
	.probe = sec_therm_probe,
	.remove = sec_therm_remove,
};

static int __init sec_therm_init(void)
{
		return platform_driver_register(&sec_thermistor_driver);
}
module_init(sec_therm_init);

static void __exit sec_therm_exit(void)
{
	platform_driver_unregister(&sec_thermistor_driver);
}
module_exit(sec_therm_exit);

MODULE_AUTHOR("ms925.kim@samsung.com");
MODULE_DESCRIPTION("sec thermistor driver");
MODULE_LICENSE("GPL");
