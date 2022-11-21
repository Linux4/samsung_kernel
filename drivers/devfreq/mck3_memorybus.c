/*
 * mck3_memorybus: devfreq driver for MCK3 DDR Device.
 *
 * Copyright (C) 2012 Marvell International Ltd.
 *	Xiaoguang Chen <chenxg@marvell.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/devfreq.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <mach/pxa95x_pm.h>
#include <plat/devfreq.h>

#define KHZ_TO_HZ	1000
#define DEF_GOVERNOR	(&devfreq_simple_ondemand)

struct ddr_devfreq_data {
	struct devfreq *devfreq;
	struct clk *ddr_clk;
};

extern void get_ddr_count(unsigned long *total, unsigned long *busy);
static int ddr_get_dev_status(struct device *dev,
			      struct devfreq_dev_status *stat)
{
	struct platform_device *pdev = container_of(dev, struct platform_device,
						    dev);
	struct ddr_devfreq_data *data = platform_get_drvdata(pdev);
	stat->current_frequency = clk_get_rate(data->ddr_clk) / KHZ_TO_HZ;
	get_ddr_count(&(stat->total_time), &(stat->busy_time));

	return 0;
}

static int ddr_target(struct device *dev, unsigned long *freq, u32 flags)
{
	struct platform_device *pdev = container_of(dev, struct platform_device,
						    dev);
	struct ddr_devfreq_data *data = platform_get_drvdata(pdev);
	int ret = 0;

	ret = clk_set_rate(data->ddr_clk, *freq * KHZ_TO_HZ);
	if (!ret)
		*freq = clk_get_rate(data->ddr_clk) / KHZ_TO_HZ;

	return ret;
}

static ssize_t ddr_freq_show(struct device *dev, struct device_attribute *attr,
			       char *buf)
{
	struct platform_device *pdev = container_of(dev, struct platform_device,
						    dev);
	struct ddr_devfreq_data *data = platform_get_drvdata(pdev);
	return sprintf(buf, "%lu\n", clk_get_rate(data->ddr_clk) /KHZ_TO_HZ);
}


static ssize_t ddr_freq_store(struct device *dev, struct device_attribute *attr,
			     const char *buf, size_t count)
{
	struct platform_device *pdev = container_of(dev, struct platform_device,
						    dev);
	struct ddr_devfreq_data *data = platform_get_drvdata(pdev);
	unsigned long freq;

	if (0x1 != sscanf(buf, "%lu", &freq)) {
		dev_err(dev, "Wrong ddr frequency setting!\n");
		return -E2BIG;
	}
	clk_set_rate(data->ddr_clk, freq * KHZ_TO_HZ);
	return count;
}

static ssize_t upthreshold_show(struct device *dev, struct device_attribute *attr,
			       char *buf)
{
	struct platform_device *pdev = container_of(dev, struct platform_device,
						    dev);
	struct ddr_devfreq_data *data = platform_get_drvdata(pdev);
	struct devfreq_simple_ondemand_data *ondemand_data = data->devfreq->data;
	return sprintf(buf, "%u\n", ondemand_data->upthreshold);
}


static ssize_t upthreshold_store(struct device *dev, struct device_attribute *attr,
			     const char *buf, size_t count)
{
	struct platform_device *pdev = container_of(dev, struct platform_device,
						    dev);
	struct ddr_devfreq_data *data = platform_get_drvdata(pdev);
	struct devfreq_simple_ondemand_data *ondemand_data = data->devfreq->data;
	unsigned int upthreshold;

	if (0x1 != sscanf(buf, "%u", &upthreshold)) {
		dev_err(dev, "Wrong ddr upthreshold setting!\n");
		return -E2BIG;
	}
	ondemand_data->upthreshold = upthreshold;
	return count;
}

static ssize_t downdifferential_show(struct device *dev, struct device_attribute *attr,
				     char *buf)
{
	struct platform_device *pdev = container_of(dev, struct platform_device,
						    dev);
	struct ddr_devfreq_data *data = platform_get_drvdata(pdev);
	struct devfreq_simple_ondemand_data *ondemand_data = data->devfreq->data;
	return sprintf(buf, "%u\n", ondemand_data->downdifferential);
}


static ssize_t downdifferential_store(struct device *dev, struct device_attribute *attr,
				     const char *buf, size_t count)
{
	struct platform_device *pdev = container_of(dev, struct platform_device,
						    dev);
	struct ddr_devfreq_data *data = platform_get_drvdata(pdev);
	struct devfreq_simple_ondemand_data *ondemand_data = data->devfreq->data;
	unsigned int downdifferential;

	if (0x1 != sscanf(buf, "%u", &downdifferential)) {
		dev_err(dev, "Wrong ddr upthreshold setting!\n");
		return -E2BIG;
	}
	ondemand_data->downdifferential = downdifferential;
	return count;
}

static DEVICE_ATTR(ddr_freq, S_IRUGO | S_IWUGO, ddr_freq_show, ddr_freq_store);
static DEVICE_ATTR(upthreshold, S_IRUGO | S_IWUGO, upthreshold_show, upthreshold_store);
static DEVICE_ATTR(downdifferential, S_IRUGO | S_IWUGO, downdifferential_show, downdifferential_store);

static struct devfreq_dev_profile ddr_devfreq_profile = {
	.polling_ms = 100,
	.target = ddr_target,
	.get_dev_status = ddr_get_dev_status,
};

static int ddr_devfreq_probe(struct platform_device *pdev)
{
	struct ddr_devfreq_data *data = NULL;
	struct devfreq_platform_data *pdata;
	struct devfreq_simple_ondemand_data *ondemand_data;
	int err = 0, i = 1;

	struct device *dev = &pdev->dev;
	pdata = (struct devfreq_platform_data *)dev->platform_data;
	if (!pdata) {
		dev_err(dev, "No platform data!\n");
		goto out;
	}

	data = kzalloc(sizeof(struct ddr_devfreq_data), GFP_KERNEL);

	if (data == NULL) {
		dev_err(dev, "Cannot allocate memory for ddr devfreq!\n");
		return -ENOMEM;
	}
	ondemand_data = kzalloc(sizeof(struct devfreq_simple_ondemand_data), GFP_KERNEL);
	if (ondemand_data == NULL) {
		dev_err(dev, "Cannot allocate memory for ddr devfreq ondemand data!\n");
		kfree(data);
		return -ENOMEM;
	}

	platform_set_drvdata(pdev, data);
	data->ddr_clk = clk_get(NULL, pdata->clk_name);
	if (IS_ERR(data->ddr_clk)) {
		err = PTR_ERR(data->ddr_clk);
		goto out;
	}
	ddr_devfreq_profile.initial_freq =
	    clk_get_rate(data->ddr_clk) / KHZ_TO_HZ;

	data->devfreq = devfreq_add_device(dev, &ddr_devfreq_profile,
					   DEF_GOVERNOR, NULL);
	if (IS_ERR(data->devfreq)) {
		err = PTR_ERR(data->devfreq);
		goto out;
	}
	data->devfreq->data = (void *)ondemand_data;

	if (pdata->freq_table) {
		devfreq_set_freq_table(data->devfreq, pdata->freq_table);
		data->devfreq->min_freq = pdata->freq_table[0].frequency;
		while (pdata->freq_table[i].frequency != DEVFREQ_TABLE_END)
			i++;
		data->devfreq->max_freq = pdata->freq_table[i - 1].frequency;
	}
	err = device_create_file(&pdev->dev, &dev_attr_ddr_freq);
	if (err) {
		dev_err(dev, "Device attr ddr_freq create failed: %d\n", err);
		goto out;
	}
	err = device_create_file(&pdev->dev, &dev_attr_upthreshold);
	if (err) {
		dev_err(dev, "Device attr upthreshold create failed: %d\n", err);
		goto out;
	}
	err = device_create_file(&pdev->dev, &dev_attr_downdifferential);
	if (err) {
		dev_err(dev, "Device attr downdifferential create failed: %d\n", err);
		goto out;
	}

	return 0;
out:
	kfree(data);
	return err;
}

static int ddr_devfreq_remove(struct platform_device *pdev)
{
	struct ddr_devfreq_data *data = platform_get_drvdata(pdev);
	devfreq_remove_device(data->devfreq);
	kfree(data);
	return 0;
}

static struct platform_driver ddr_devfreq_driver = {
	.probe = ddr_devfreq_probe,
	.remove = ddr_devfreq_remove,
	.driver = {
		   .name = "devfreq-ddr",
		   .owner = THIS_MODULE,
		   },
};

static int __init ddr_devfreq_init(void)
{
	return platform_driver_register(&ddr_devfreq_driver);
}

static void __init ddr_devfreq_exit(void)
{
	platform_driver_unregister(&ddr_devfreq_driver);
}

module_init(ddr_devfreq_init);
module_exit(ddr_devfreq_exit);

MODULE_DESCRIPTION("MCK3 DDR devfreq device driver");
MODULE_LICENSE("GPL");
