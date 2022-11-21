/*
 * devfreq-vpu: Generic Dynamic Voltage and Frequency Scaling (DVFS) Framework
 *		  for vpu Device.
 *
 * Copyright (C) 2010 Marvell International Ltd.
 *	Xiaoguang Chen <chenxg@marvell.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/clk.h>
#include <linux/devfreq.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <plat/devfreq.h>
#include <mach/dvfs.h>

#define DEVFREQ_DEFAULT_GOVERNOR	(&devfreq_userspace)

#define KHZ_TO_HZ	1000

struct vpu_devfreq_data {
	struct devfreq *devfreq;
	struct clk *vclk;
};

static struct vpu_devfreq_data *cur_data;

static int dvfs_notifier_freq(struct notifier_block *nb,
			      unsigned long val, void *data)
{
	struct dvfs_freqs *freqs = (struct dvfs_freqs *)data;
	if (strcmp(freqs->dvfs->clk_name, cur_data->vclk->name))
		return 0;

	switch (val) {
	case DVFS_FREQ_POSTCHANGE:
		cur_data->devfreq->previous_freq = freqs->new;
		break;
	default:
		break;
	}
	return 0;
}

static struct notifier_block notifier_freq_block = {
	.notifier_call = dvfs_notifier_freq,
};

static int vpu_target(struct device *dev, unsigned long *freq, u32 flags)
{
	struct platform_device *pdev = container_of(dev, struct platform_device,
						    dev);
	struct vpu_devfreq_data *data = platform_get_drvdata(pdev);
	int ret = 0;

	ret = clk_set_rate(data->vclk, *freq * KHZ_TO_HZ);
	if (!ret)
		*freq = clk_get_rate(data->vclk) / KHZ_TO_HZ;
	return ret;
}

static struct devfreq_dev_profile vpu_devfreq_profile = {
	.target = vpu_target,
};

static int vpu_devfreq_probe(struct platform_device *pdev)
{
	struct vpu_devfreq_data *data = NULL;
	struct devfreq_platform_data *pdata;
	int err = 0;
	int i = 0;
	struct device *dev = &pdev->dev;
	pdata = (struct devfreq_platform_data *)dev->platform_data;
	if (!pdata) {
		dev_err(dev, "No platform data!\n");
		goto out;
	}

	data = kzalloc(sizeof(struct vpu_devfreq_data), GFP_KERNEL);

	if (data == NULL) {
		dev_err(dev, "Cannot allocate memory for vpu devfreq!\n");
		return -ENOMEM;
	}
	platform_set_drvdata(pdev, data);
	data->vclk = clk_get(NULL, pdata->clk_name);
	if (IS_ERR(data->vclk)) {
		err = PTR_ERR(data->vclk);
		goto out;
	}
	vpu_devfreq_profile.initial_freq =
			clk_get_rate(data->vclk) / KHZ_TO_HZ;

	data->devfreq = devfreq_add_device(dev, &vpu_devfreq_profile,
					   DEVFREQ_DEFAULT_GOVERNOR, NULL);
	if (IS_ERR(data->devfreq)) {
		err = PTR_ERR(data->devfreq);
		goto out;
	}

	cur_data = data;

	dvfs_register_notifier(&notifier_freq_block, DVFS_FREQUENCY_NOTIFIER);

	if (pdata->freq_table){
		devfreq_set_freq_table(data->devfreq, pdata->freq_table);
		while (pdata->freq_table[i].frequency != DEVFREQ_TABLE_END)
			i++;
		data->devfreq->max_freq =  pdata->freq_table[i-1].frequency;
		data->devfreq->min_freq =  pdata->freq_table[0].frequency;
	}
	return 0;
out:
	kfree(data);
	return err;
}

static int vpu_devfreq_remove(struct platform_device *pdev)
{
	struct vpu_devfreq_data *data = platform_get_drvdata(pdev);
	devfreq_remove_device(data->devfreq);
	kfree(data);
	dvfs_unregister_notifier(&notifier_freq_block, DVFS_FREQUENCY_NOTIFIER);
	return 0;
}

static struct platform_driver vpu_devfreq_driver = {
	.probe = vpu_devfreq_probe,
	.remove = vpu_devfreq_remove,
	.driver = {
		   .name = "devfreq-vpu",
		   .owner = THIS_MODULE,
		   },
};

static int __init vpu_devfreq_init(void)
{
	return platform_driver_register(&vpu_devfreq_driver);
}

static void __init vpu_devfreq_exit(void)
{
	platform_driver_unregister(&vpu_devfreq_driver);
}

module_init(vpu_devfreq_init);
module_exit(vpu_devfreq_exit);

MODULE_DESCRIPTION("vpu devfreq device driver");
MODULE_LICENSE("GPL");
