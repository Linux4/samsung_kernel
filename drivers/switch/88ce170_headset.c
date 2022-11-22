/*
 *	drivers/switch/88ce170_headset.c
 *
 *	headset detect driver for ce170
 *
 *	Copyright (C) 2013, Marvell Corporation
 *	Author: Zhao Ye <zhaoy@marvell.com>
 *
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License version 2 as
 *	published by the Free Software Foundation.
 */
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/mfd/88ce170.h>
#include <linux/switch.h>
#include <linux/workqueue.h>
#include <linux/uaccess.h>
#include <linux/miscdevice.h>

/* Headset detection info */
struct ce170_headset_info {
	struct ce170_chip *chip;
	struct device *dev;
	struct regmap *map;
	int irq_headset;
	struct work_struct work_headset;
	struct headset_switch_data *psw_data_headset;
};

struct headset_switch_data {
	struct switch_dev sdev;
	int irq;
	int state;
};

#define CE170_HEADSET_REMOVE			0
#define CE170_HEADSET_ADD			1
#define CE170_HEADPHONE_ADD			2
#define CE170_HEADSET_MODE_STEREO		0
#define CE170_HEADSET_MODE_MONO			1

static void ce170_headset_switch_work(struct work_struct *work)
{
	struct ce170_headset_info *info =
	    container_of(work, struct ce170_headset_info, work_headset);
	unsigned int value;
	int ret;
	struct headset_switch_data *switch_data;

	if (info == NULL) {
		pr_debug("Invalid headset info!\n");
		return;
	}

	switch_data = info->psw_data_headset;
	if (switch_data == NULL) {
		pr_debug("Invalid headset switch data!\n");
		return;
	}

	ret = regmap_read(info->map, CE170_INT_STATUS2, &value);
	if (ret < 0) {
		dev_err(info->dev,
			"Failed to read CE170_INT_STATUS2 : 0x%x\n", ret);
		return;
	}

	/* clear the irq register status */
	regmap_update_bits(info->map, CE170_INT_CONTROL_REG1, 0x01, 0x01);
	regmap_update_bits(info->map, CE170_INT_CONTROL_REG1, 0x01, 0x00);

	if (value & CE170_INT2_HSINSERT) {
		switch_data->state = CE170_HEADSET_ADD;
		if (value & CE170_INT2_EXTMIC)
			switch_data->state = CE170_HEADPHONE_ADD;
	} else
		switch_data->state = CE170_HEADSET_REMOVE;

	pr_info("headset_switch_work to %d\n", switch_data->state);
	switch_set_state(&switch_data->sdev, switch_data->state);
}

static ssize_t switch_headset_print_state(struct switch_dev *sdev, char *buf)
{
	return sprintf(buf, "%d\n", switch_get_state(sdev));
}

static ssize_t switch_headset_print_name(struct switch_dev *sdev, char *buf)
{
	switch (switch_get_state(sdev)) {
	case CE170_HEADSET_ADD:
		return sprintf(buf, "Headset\n");
	case CE170_HEADPHONE_ADD:
		return sprintf(buf, "Headphone\n");
	case CE170_HEADSET_REMOVE:
		return sprintf(buf, "No deviece\n");
	}
	return sprintf(buf, "No device\n");
}

static irqreturn_t ce170_headset_handler(int irq, void *data)
{
	struct ce170_headset_info *info = data;
	if (irq == info->irq_headset) {
		/* headset interrupt */
		if (info->psw_data_headset != NULL)
			queue_work(system_wq, &info->work_headset);
	}
	return IRQ_HANDLED;
}

static int ce170_headset_switch_probe(struct platform_device *pdev)
{
	struct ce170_chip *chip = dev_get_drvdata(pdev->dev.parent);
	struct ce170_headset_info *info;
	struct gpio_switch_platform_data *pdata_headset =
	    pdev->dev.platform_data;
	struct headset_switch_data *switch_data_headset = NULL;
	int irq_headset, ret = 0;

	if (pdata_headset == NULL) {
		pr_debug("Invalid gpio switch platform data!\n");
		return -EBUSY;
	}
	irq_headset = platform_get_irq(pdev, 0);
	if (irq_headset < 0) {
		dev_err(&pdev->dev, "No IRQ resource for headset!\n");
		return -EINVAL;
	}
	info =
	    devm_kzalloc(&pdev->dev, sizeof(struct ce170_headset_info),
			 GFP_KERNEL);
	if (!info)
		return -ENOMEM;

	info->chip = chip;
	info->map = chip->regmap;
	info->dev = &pdev->dev;
	info->irq_headset = irq_headset + chip->irq_base;

	switch_data_headset =
	    devm_kzalloc(&pdev->dev, sizeof(struct headset_switch_data),
			 GFP_KERNEL);
	if (!switch_data_headset) {
		dev_err(&pdev->dev, "Failed to allocate headset data\n");
		return -ENOMEM;
	}

	switch_data_headset->sdev.name = pdata_headset->name;
	switch_data_headset->sdev.print_state = switch_headset_print_state;
	switch_data_headset->sdev.print_name = switch_headset_print_name;
	info->psw_data_headset = switch_data_headset;
	ret = switch_dev_register(&switch_data_headset->sdev);
	if (ret < 0)
		return ret;

	/* irq trigger falling */
	regmap_update_bits(info->map, CE170_INT_CONTROL_REG1, 0x02, 0x00);
	ret =
	    request_threaded_irq(info->irq_headset, NULL, ce170_headset_handler,
				 IRQF_ONESHOT, "headset", info);
	if (ret < 0) {
		dev_err(chip->dev, "Failed to request IRQ: #%d: %d\n",
			info->irq_headset, ret);
		goto out_irq_headset;
	}

	platform_set_drvdata(pdev, info);
	INIT_WORK(&info->work_headset, ce170_headset_switch_work);
	ce170_headset_switch_work(&info->work_headset);

	return 0;

out_irq_headset:
	switch_dev_unregister(&switch_data_headset->sdev);

	return ret;
}

static int __devexit ce170_headset_switch_remove(struct platform_device *pdev)
{
	struct ce170_headset_info *info = platform_get_drvdata(pdev);
	struct headset_switch_data *switch_data_headset =
	    info->psw_data_headset;

	cancel_work_sync(&info->work_headset);

	free_irq(info->irq_headset, info);

	switch_dev_unregister(&switch_data_headset->sdev);

	return 0;
}

static int ce170_headset_switch_suspend(struct platform_device *pdev,
					pm_message_t state)
{
	return 0;
}

static int ce170_headset_switch_resume(struct platform_device *pdev)
{
	return 0;
}

static struct platform_driver ce170_headset_switch_driver = {
	.probe = ce170_headset_switch_probe,
	.remove = __devexit_p(ce170_headset_switch_remove),
	.suspend = ce170_headset_switch_suspend,
	.resume = ce170_headset_switch_resume,
	.driver = {
		   .name = "88ce170-headset",
		   .owner = THIS_MODULE,
		   },
};

static struct miscdevice ce170_hsdetect_miscdev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "88ce170-headset",
};

static int __init headset_switch_init(void)
{
	int ret = -EINVAL;
	ret = misc_register(&ce170_hsdetect_miscdev);
	if (ret < 0)
		return ret;
	ret = platform_driver_register(&ce170_headset_switch_driver);
	return ret;
}

module_init(headset_switch_init);
static void __exit headset_switch_exit(void)
{
	platform_driver_unregister(&ce170_headset_switch_driver);
	misc_deregister(&ce170_hsdetect_miscdev);
}

module_exit(headset_switch_exit);

MODULE_DESCRIPTION("Marvell 88CE170 Headset driver");
MODULE_AUTHOR("Zhao Ye <zhaoy@marvell.com>");
MODULE_LICENSE("GPL");
