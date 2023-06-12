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
#if defined(CONFIG_MFD_88PM822)
#include <linux/mfd/88pm822.h>
#elif defined(CONFIG_MFD_88PM800)
#include <linux/mfd/88pm80x.h>
#elif defined(CONFIG_MFD_D2199)
#include <linux/d2199/d2199_battery.h>
#include <linux/battery/sec_battery.h>
#elif defined(CONFIG_ARCH_SC)
#include <mach/adc.h>
#include <mach/sci.h>
#include <linux/battery/sec_battery.h>
#endif

#define ADC_SAMPLING_CNT	7

struct sec_therm_info {
	struct device *dev;
	struct sec_therm_platform_data *pdata;
	struct mutex therm_mutex;

	int curr_temp;
	int curr_temp_adc;
	int curr_siop_level;
};

static int sec_therm_get_adc_data(struct sec_therm_info *info)
{
	int adc_data;
	int adc_max = 0;
	int adc_min = 0;
	int adc_total = 0;
	int i, j;
	int err_value;

#if defined(CONFIG_MFD_88PM822)
	/* bias register value */
	unsigned int bias_value[5] = {0x06, 0x0C, 0x03, 0x02, 0x01};
	/* bias current value: mA; _MUST_ be aligned with bias_value */
	unsigned int bias_current[5] = {31, 61, 16, 11, 6};
#endif

	for (i = 0; i < ADC_SAMPLING_CNT; i++) {
#if defined(CONFIG_MFD_88PM822)
		pm822_read_gpadc(&adc_data, info->pdata->adc_channel);
		/*
		 *1) set bias as 31uA firstly for root remp environment,
		 *2) check the voltage whether it's in [0.3V, 1.25V];
		 *   if yes, this adc_data is OK; break;
		 *   else
		 *   set bias as 61uA /16uA / 11uA /6uA ....for lower or higher temp environment.
		 */
		//		for(j = 0; j < 5; j++) {
		//			get_dynamicbiasgpadc(&adc_data, bias_value[j], info->pdata->adc_channel);
		//			if ((adc_data > 300) && (adc_data < 1250)) {
		//				adc_data /= bias_current[j];
		//				break;
		//			}
		//		}
#elif defined(CONFIG_MFD_88PM800)
		pm80x_read_gpadc(&adc_data, info->pdata->adc_channel);
#elif defined(CONFIG_MFD_D2199)
		d2199_read_temperature_adc(&adc_data, info->pdata->adc_channel);
#elif defined(CONFIG_ARCH_SCX15)
		adc_data = sci_adc_get_value(info->pdata->adc_channel, false);
#endif /* CONFIG_MFD_88PM822 */

		if (adc_data < 0) {
			dev_err(info->dev, "%s : err(%d) returned, skip read\n",
					__func__, adc_data);
			err_value = adc_data;
			goto err;
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
err:
	return err_value;
}

static int convert_adc_to_temper(struct sec_therm_info *info, unsigned int adc)
{
	int low = 0;
	int high = 0;
	int mid = 0;
	int temp = 0;

	if (!info->pdata->adc_table || !info->pdata->adc_arr_size) {
		/* using fake temp */
		return 300;
	}

	if (info->pdata->adc_table[0].adc >= adc) {
		temp = info->pdata->adc_table[0].temperature;
		goto temp_by_adc_goto;
	} else if (info->pdata->adc_table[info->pdata->adc_arr_size-1].adc <= adc) {
		temp = info->pdata->adc_table[info->pdata->adc_arr_size-1].temperature;
		goto temp_by_adc_goto;
	}

	high = info->pdata->adc_arr_size - 1;

	while (low <= high) {
		mid = (low + high) / 2;
		if (info->pdata->adc_table[mid].adc > adc)
			high = mid - 1;
		else if (info->pdata->adc_table[mid].adc < adc)
			low = mid + 1;
		else {
			temp = info->pdata->adc_table[mid].temperature;
			goto temp_by_adc_goto;
	}
	}

	temp = info->pdata->adc_table[high].temperature;
	temp +=
		(((int)info->pdata->adc_table[low].temperature -
		(int)info->pdata->adc_table[high].temperature) *
		((int)adc - (int)info->pdata->adc_table[high].adc)) /
		((int)info->pdata->adc_table[low].adc - (int)info->pdata->adc_table[high].adc);

temp_by_adc_goto:

	return temp;
}

static ssize_t sec_therm_show_temp_adc(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	struct sec_therm_info *info = dev_get_drvdata(dev);
	int adc;

	mutex_lock(&info->therm_mutex);
	adc = sec_therm_get_adc_data(info);
	mutex_unlock(&info->therm_mutex);

	return sprintf(buf, "%d\n", adc);
}

static ssize_t sec_therm_show_temperature(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	struct sec_therm_info *info = dev_get_drvdata(dev);
	int adc, temp;
	mutex_lock(&info->therm_mutex);
	adc = sec_therm_get_adc_data(info);
	mutex_unlock(&info->therm_mutex);

	if (adc < 0)
		temp = -1;
	else
		temp = convert_adc_to_temper(info, adc);

	return sprintf(buf, "%d\n", temp);
}

static ssize_t sec_therm_show_rf_temperature(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	struct sec_therm_info *info = dev_get_drvdata(dev);
	int adc, temp;

	mutex_lock(&info->therm_mutex);
	adc = sec_therm_get_adc_data(info);
	mutex_unlock(&info->therm_mutex);

	if (adc < 0)
		temp = -1;
	else
		temp = convert_adc_to_temper(info, adc);

	return sprintf(buf, "%d\n", temp);
}

static DEVICE_ATTR(temp_adc , S_IRUGO, \
		sec_therm_show_temp_adc, NULL);
static DEVICE_ATTR(temperature, S_IRUGO, \
		sec_therm_show_temperature, NULL);
static DEVICE_ATTR(rf_temperature, S_IRUGO, \
		sec_therm_show_rf_temperature, NULL);

static struct attribute *sec_therm_attributes[] = {
	&dev_attr_temp_adc.attr,
	&dev_attr_temperature.attr,
	&dev_attr_rf_temperature.attr,
	NULL
};

static const struct attribute_group sec_therm_group = {
	.attrs = sec_therm_attributes,
};

static int sec_therm_probe(struct platform_device *pdev)
{
	struct sec_therm_platform_data *pdata = dev_get_platdata(&pdev->dev);
	struct sec_therm_info *info;
	int ret = 0;

	dev_info(&pdev->dev, "%s: SEC Thermistor Driver Loading\n", __func__);

	info = kzalloc(sizeof(*info), GFP_KERNEL);
	if (!info)
		return -ENOMEM;

	platform_set_drvdata(pdev, info);

	info->dev = &pdev->dev;
	info->pdata = pdata;

	mutex_init(&info->therm_mutex);

	ret = sysfs_create_group(&info->dev->kobj, &sec_therm_group);

	if (ret) {
		dev_err(info->dev,
				"failed to create sysfs attribute group\n");
	}

	return ret;
}

static int sec_therm_remove(struct platform_device *pdev)
{
	struct sec_therm_info *info = platform_get_drvdata(pdev);

	if (!info)
		return 0;

	sysfs_remove_group(&info->dev->kobj, &sec_therm_group);
	kfree(info);

	return 0;
}

static struct platform_driver sec_thermistor_driver = {
	.driver = {
		.name = "sec-thermistor",
		.owner = THIS_MODULE,
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

