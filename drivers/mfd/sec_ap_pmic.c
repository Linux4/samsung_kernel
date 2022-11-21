/*
 * Copyright (c) 2015, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/sec_class.h>
#include <linux/device.h>
#include <linux/proc_fs.h>
#include <linux/input/qpnp-power-on.h>
#include <linux/sec_debug.h>
#include <linux/seq_file.h>

static struct device *sec_ap_pmic_dev;

static int pwrsrc_show(struct seq_file *m, void *v)
{
	char buf[SZ_1K];

	sec_get_pwrsrc(buf);
	seq_printf(m, "%s", buf);

	return 0;
}

static int pwrsrc_open(struct inode *inode, struct file *file)
{
	return single_open(file, pwrsrc_show, NULL);
}

static const struct file_operations proc_pwrsrc_operation = {
	.open		= pwrsrc_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= seq_release,
};

#ifdef CONFIG_MUIC_NOTIFIER
static ssize_t chg_det_show(struct device *in_dev,
				struct device_attribute *attr, char *buf)
{
	int ret = 0;

	ret = qpnp_pon_check_chg_det();

	pr_info("%s: ret = %d\n", __func__, ret);
	return sprintf(buf, "%d\n", ret);
}
static DEVICE_ATTR_RO(chg_det);
#endif

static ssize_t off_reason_show(struct device *in_dev,
				struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%s", qpnp_pon_get_off_reason());
}
static DEVICE_ATTR_RO(off_reason);

ssize_t print_gpio_exp(char *buf)
{
#ifdef CONFIG_GPIO_PCAL6524
	extern ssize_t get_gpio_exp(char *buf);
	return get_gpio_exp(buf);
#else
	return 0;
#endif
}
EXPORT_SYMBOL(print_gpio_exp);

static ssize_t gpio_exp_show(struct device *in_dev,
				struct device_attribute *attr, char *buf)
{
	return print_gpio_exp(buf);
}
static DEVICE_ATTR_RO(gpio_exp);

static ssize_t debug_show(struct device *in_dev,
				struct device_attribute *attr, char *buf)
{
	ssize_t size = 0;

#ifdef CONFIG_MUIC_NOTIFIER
	size += chg_det_show(in_dev, attr, buf+size);
#endif
	size += off_reason_show(in_dev, attr, buf+size);
	size += gpio_exp_show(in_dev, attr, buf+size);

	return size;
}
static DEVICE_ATTR_RO(debug);

static int __init sec_ap_pmic_init(void)
{
	int err;
	struct proc_dir_entry *entry;

	sec_ap_pmic_dev = sec_device_create(0, NULL, "ap_pmic");

	if (unlikely(IS_ERR(sec_ap_pmic_dev))) {
		pr_err("%s: Failed to create ap_pmic device\n", __func__);
		err = PTR_ERR(sec_ap_pmic_dev);
		goto err_device_create;
	}

#ifdef CONFIG_MUIC_NOTIFIER
	err = device_create_file(sec_ap_pmic_dev, &dev_attr_chg_det);
	if (unlikely(err)) {
		pr_err("%s: Failed to create chg_det\n", __func__);
		goto err_device_create;
	}
#endif

	err = device_create_file(sec_ap_pmic_dev, &dev_attr_off_reason);
	if (unlikely(err)) {
		pr_err("%s: Failed to create off_reason\n", __func__);
		goto err_device_create;
	}

	err = device_create_file(sec_ap_pmic_dev, &dev_attr_gpio_exp);
	if (unlikely(err)) {
		pr_err("%s: Failed to create gpio_exp\n", __func__);
		goto err_device_create;
	}

	err = device_create_file(sec_ap_pmic_dev, &dev_attr_debug);
	if (unlikely(err)) {
		pr_err("%s: Failed to create debug\n", __func__);
		goto err_device_create;
	}
	
	/* pmic pon logging for eRR.p */
	entry = proc_create("pwrsrc", 0444, NULL, &proc_pwrsrc_operation);
	if (unlikely(!entry)) {
		err =  -ENOMEM;
		goto err_device_create;
	}

	pr_info("%s: ap_pmic successfully inited.\n", __func__);

	return 0;

err_device_create:
	return err;
}
module_init(sec_ap_pmic_init);

MODULE_DESCRIPTION("sec_ap_pmic driver");
MODULE_AUTHOR("Jiman Cho <jiman85.cho@samsung.com");
MODULE_LICENSE("GPL");
