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
#include <plat/adc.h>
#include <mach/sec_thermistor.h>
#if defined(CONFIG_MFD_88PM800)
#include <linux/mfd/88pm80x.h>
#endif

#define ADC_SAMPLING_CNT	7
#define DEFAULT_POLLING_INTERVAL	(5 * 1000) /* 5 seconds */

struct sec_therm_info {
	struct device *dev;
	struct sec_therm_platform_data *pdata;
	struct delayed_work polling_work;
	struct mutex therm_mutex;

	int curr_temp;
	int curr_temp_adc;
	int forced_siop_level;
	int curr_siop_level;
};

static int sec_therm_get_adc_data(struct sec_therm_info *info)
{
	int adc_data;
	int adc_max = 0;
	int adc_min = 0;
	int adc_total = 0;
	int i;
	int err_value;

	for (i = 0; i < ADC_SAMPLING_CNT; i++) {
		pm80x_rf_read_temperature(&adc_data);

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

	if (!info->pdata->adc_table || !info->pdata->adc_arr_size) {
		/* using fake temp */
		return 300;
	}

	high = info->pdata->adc_arr_size - 1;

	while (low <= high) {
		mid = (low + high) / 2;
		if (info->pdata->adc_table[mid].adc > adc)
			high = mid - 1;
		else if (info->pdata->adc_table[mid].adc < adc)
			low = mid + 1;
		else
			break;
	}
	return info->pdata->adc_table[mid].temperature;
}

static void notify_change_of_temperature(struct sec_therm_info *info)
{
	char temp_buf[20];
	char siop_buf[20];
	char *envp[3];
	int env_offset = 0;
	int siop_level = -1;

	snprintf(temp_buf, sizeof(temp_buf), "TEMPERATURE=%d",
		 info->curr_temp);
	envp[env_offset++] = temp_buf;

	if (info->pdata->get_siop_level)
		siop_level = info->pdata->get_siop_level(info->curr_temp);

	if (info->forced_siop_level >= 0)
		siop_level = info->forced_siop_level;

	if (siop_level >= 0) {
		snprintf(siop_buf, sizeof(siop_buf), "SIOP_LEVEL=%d",
				siop_level);
		envp[env_offset++] = siop_buf;
		info->curr_siop_level = siop_level;
		dev_info(info->dev, "%s: uevent: %s\n", __func__, siop_buf);
	}
	envp[env_offset] = NULL;

	dev_info(info->dev, "%s: uevent: %s\n", __func__, temp_buf);
	kobject_uevent_env(&info->dev->kobj, KOBJ_CHANGE, envp);
}

static ssize_t sec_therm_show_temperature(struct device *dev,
				   struct device_attribute *attr,
				   char *buf)
{
	struct sec_therm_info *info = dev_get_drvdata(dev);

	return sprintf(buf, "%d\n", info->curr_temp);
}

static ssize_t sec_therm_show_temp_adc(struct device *dev,
				   struct device_attribute *attr,
				   char *buf)
{
	struct sec_therm_info *info = dev_get_drvdata(dev);

	return sprintf(buf, "%d\n", info->curr_temp_adc);
}

static ssize_t sec_therm_show_forced_siop_level(struct device *dev,
				struct device_attribute *attr,
				char *buf)
{
	struct sec_therm_info *info = dev_get_drvdata(dev);

	return sprintf(buf, "%d\n", info->forced_siop_level);
}

static ssize_t sec_therm_store_forced_siop_level(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t count)
{
	struct sec_therm_info *info = dev_get_drvdata(dev);
	unsigned int val;

	if (sscanf(buf, "%d", &val) == 1)
		info->forced_siop_level = val;
	else
		return -EINVAL;

	notify_change_of_temperature(info);

	return count;
}

static ssize_t sec_therm_show_curr_siop_level(struct device *dev,
				   struct device_attribute *attr,
				   char *buf)
{
	struct sec_therm_info *info = dev_get_drvdata(dev);

	return sprintf(buf, "%d\n", info->curr_siop_level);
}

static ssize_t sec_therm_show_timer_rate(struct device *dev,
				   struct device_attribute *attr,
				   char *buf)
{
	struct sec_therm_info *info = dev_get_drvdata(dev);

	return sprintf(buf, "%u\n", info->pdata->polling_interval / 1000);
}

static ssize_t sec_therm_store_timer_rate(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t count)
{
	struct sec_therm_info *info = dev_get_drvdata(dev);
	unsigned int val;

	if (sscanf(buf, "%u", &val) == 1) {
		if (val <= 10000)
			info->pdata->polling_interval = val * 1000;
		else
			return -EINVAL;
	}

	cancel_delayed_work_sync(&info->polling_work);
	schedule_delayed_work(&info->polling_work,
			msecs_to_jiffies(info->pdata->polling_interval));

	return count;
}

static ssize_t sec_therm_show_raw_temp_adc(struct device *dev,
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

static ssize_t sec_therm_show_raw_temperature(struct device *dev,
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

static DEVICE_ATTR(temperature, S_IRUGO, sec_therm_show_temperature, NULL);
static DEVICE_ATTR(temp_adc, S_IRUGO, sec_therm_show_temp_adc, NULL);
static DEVICE_ATTR(forced_siop_level, S_IWUSR | S_IRUGO, \
			sec_therm_show_forced_siop_level, \
			sec_therm_store_forced_siop_level);
static DEVICE_ATTR(curr_siop_level, S_IRUGO, \
		sec_therm_show_curr_siop_level, NULL);
static DEVICE_ATTR(timer_rate, S_IWUSR | S_IRUGO, \
			sec_therm_show_timer_rate, \
			sec_therm_store_timer_rate);
static DEVICE_ATTR(raw_temp_adc , S_IRUGO, \
			sec_therm_show_raw_temp_adc, NULL);
static DEVICE_ATTR(raw_temperature, S_IRUGO, \
			sec_therm_show_raw_temperature, NULL);

static struct attribute *sec_therm_attributes[] = {
	&dev_attr_temperature.attr,
	&dev_attr_temp_adc.attr,
	&dev_attr_forced_siop_level.attr,
	&dev_attr_curr_siop_level.attr,
	&dev_attr_timer_rate.attr,
	&dev_attr_raw_temp_adc.attr,
	&dev_attr_raw_temperature.attr,
	NULL
};

static const struct attribute_group sec_therm_group = {
	.attrs = sec_therm_attributes,
};

static void sec_therm_polling_work(struct work_struct *work)
{
	struct sec_therm_info *info =
		container_of(work, struct sec_therm_info, polling_work.work);
	int adc;
	int temper;

	mutex_lock(&info->therm_mutex);
	adc = sec_therm_get_adc_data(info);
	mutex_unlock(&info->therm_mutex);
	dev_dbg(info->dev, "%s: adc=%d\n", __func__, adc);

	if (adc < 0)
		goto out;

	temper = convert_adc_to_temper(info, adc);
	dev_dbg(info->dev, "%s: temper=%d\n", __func__, temper);

	/* if temperature was changed, notify to framework */
	if (info->curr_temp != temper) {
		info->curr_temp_adc = adc;
		info->curr_temp = temper;
		notify_change_of_temperature(info);
	}
out:
	schedule_delayed_work(&info->polling_work,
			msecs_to_jiffies(info->pdata->polling_interval));
}

static __devinit int sec_therm_probe(struct platform_device *pdev)
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
	info->forced_siop_level = -1;
	if (info->pdata->polling_interval < DEFAULT_POLLING_INTERVAL)
		info->pdata->polling_interval = DEFAULT_POLLING_INTERVAL;

	mutex_init(&info->therm_mutex);

	ret = sysfs_create_group(&info->dev->kobj, &sec_therm_group);

	if (ret) {
		dev_err(info->dev,
			"failed to create sysfs attribute group\n");
	}

	INIT_DELAYED_WORK_DEFERRABLE(&info->polling_work,
			sec_therm_polling_work);
	schedule_delayed_work(&info->polling_work, msecs_to_jiffies(90000));

	return ret;
}

static int __devexit sec_therm_remove(struct platform_device *pdev)
{
	struct sec_therm_info *info = platform_get_drvdata(pdev);

	if (!info)
		return 0;

	sysfs_remove_group(&info->dev->kobj, &sec_therm_group);

	cancel_delayed_work(&info->polling_work);
	kfree(info);

	return 0;
}

#ifdef CONFIG_PM
static int sec_therm_suspend(struct device *dev)
{
	struct sec_therm_info *info = dev_get_drvdata(dev);

	cancel_delayed_work(&info->polling_work);

	return 0;
}

static int sec_therm_resume(struct device *dev)
{
	struct sec_therm_info *info = dev_get_drvdata(dev);

	schedule_delayed_work(&info->polling_work,
			msecs_to_jiffies(info->pdata->polling_interval));
	return 0;
}
#else
#define sec_therm_suspend	NULL
#define sec_therm_resume	NULL
#endif /* CONFIG_PM */

static const struct dev_pm_ops sec_thermistor_pm_ops = {
	.suspend = sec_therm_suspend,
	.resume = sec_therm_resume,
};

static struct platform_driver sec_thermistor_driver = {
	.driver = {
		   .name = "sec-thermistor",
		   .owner = THIS_MODULE,
		   .pm = &sec_thermistor_pm_ops,
	},
	.probe = sec_therm_probe,
	.remove = __devexit_p(sec_therm_remove),
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
