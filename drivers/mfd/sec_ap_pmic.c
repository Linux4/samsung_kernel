/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
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
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/gpio.h>
#include <linux/device.h>
#include <linux/proc_fs.h>
#include <linux/input/qpnp-power-on.h>
#include <linux/sec_debug.h>
#include <linux/seq_file.h>
#include <linux/kernel.h>

#define MAX_CABLE_DET	10

/* for enable/disable manual reset, from retail group's request */
extern void do_keyboard_notifier(int onoff);

struct sec_ap_pmic_info {
	struct device		*dev;
	struct device		*sysfs;
	struct proc_dir_entry *proc;
#ifdef CONFIG_SEC_FACTORY
	int cable_det_cnt;
	int cable_det[MAX_CABLE_DET];
#endif /* CONFIG_SEC_FACTORY */
};

static struct sec_ap_pmic_info *sec_ap_pmic_data;

static int pwrsrc_show(struct seq_file *m, void *v)
{
	char buf[SZ_1K];

	sec_get_pwrsrc(buf);
	seq_printf(m, buf);

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

static ssize_t manual_reset_show(struct device *in_dev,
				struct device_attribute *attr, char *buf)
{
	int ret = 0;

	ret = qpnp_get_s2_reset_onoff();

	pr_info("%s: ret = %d\n", __func__, ret);
	return sprintf(buf, "%d\n", ret);
}

static ssize_t manual_reset_store(struct device *in_dev,
		struct device_attribute *attr, const char *buf, size_t len)
{
	int onoff = 0;

	if (kstrtoint(buf, 10, &onoff))
		return -EINVAL;

	pr_info("%s: onoff(%d)\n", __func__, onoff);

	do_keyboard_notifier(onoff);
qpnp_control_s2_reset_onoff(onoff);

	return len;
}
static DEVICE_ATTR_RW(manual_reset);


static ssize_t chg_det_show(struct device *in_dev,
				struct device_attribute *attr, char *buf)
{
	int ret = 0;

	ret = qpnp_pon_check_chg_det();

	pr_info("%s: ret = %d\n", __func__, ret);
	return sprintf(buf, "%d\n", ret);
}
static DEVICE_ATTR_RO(chg_det);

#ifdef CONFIG_SEC_FACTORY
static ssize_t cable_det_show(struct device *in_dev,
				struct device_attribute *attr, char *buf)
{
	char values[MAX_CABLE_DET+1];
	int i;

	for (i = 0; i < sec_ap_pmic_data->cable_det_cnt; i++)
		values[i] = gpio_get_value(sec_ap_pmic_data->cable_det[i]) ? '0':'1';
	values[i] = '\0';

	pr_info("%s: values[%s]\n", __func__, values);
	return sprintf(buf, "%s\n", values);
}
static DEVICE_ATTR_RO(cable_det);
#endif /* CONFIG_SEC_FACTORY */

static struct attribute *sec_ap_pmic_attributes[] = {
	&dev_attr_chg_det.attr,
	&dev_attr_manual_reset.attr,
#ifdef CONFIG_SEC_FACTORY
	&dev_attr_cable_det.attr,
#endif /* CONFIG_SEC_FACTORY */
	NULL,
};

static struct attribute_group sec_ap_pmic_attr_group = {
	.attrs = sec_ap_pmic_attributes,
};

static int sec_ap_pmic_parse_dt(struct sec_ap_pmic_info *info, struct device_node *np)
{
#ifdef CONFIG_SEC_FACTORY
	struct device_node *cbl_det_np;
	int i;
#endif /* CONFIG_SEC_FACTORY */

#ifdef CONFIG_SEC_FACTORY
	cbl_det_np = of_find_node_by_name(np, "cable_det");
	if (cbl_det_np) {
		info->cable_det_cnt = of_gpio_count(cbl_det_np);
		info->cable_det_cnt = (info->cable_det_cnt >= MAX_CABLE_DET)
									? MAX_CABLE_DET : info->cable_det_cnt;
		pr_info("cable_det gpio cnt = %d\n", info->cable_det_cnt);

		for (i = 0; i < info->cable_det_cnt; i++) {
			info->cable_det[i] = of_get_gpio(cbl_det_np, i);
			if (gpio_is_valid(info->cable_det[i]))
				pr_info("[%d] cable_det: %d\n", i, info->cable_det[i]);
		}
	}
#endif /* CONFIG_SEC_FACTORY */

	return 0;
}

static int sec_ap_pmic_probe(struct platform_device *pdev)
{
	struct device_node *node = pdev->dev.of_node;
	struct sec_ap_pmic_info *info;
	int err;
	struct proc_dir_entry *entry;

	if (!node) {
		dev_err(&pdev->dev, "device-tree data is missing\n");
		return -ENXIO;
	}

	info = devm_kzalloc(&pdev->dev, sizeof(*info), GFP_KERNEL);
	if (!info) {
		err = -ENOMEM;
		goto err_device_create;
	}

	platform_set_drvdata(pdev, info);
	info->dev = &pdev->dev;
	sec_ap_pmic_data = info;

	sec_ap_pmic_parse_dt(info, node);

	info->sysfs = sec_device_create(NULL, "ap_pmic");

	if (unlikely(IS_ERR(info->sysfs))) {
		pr_err("%s: Failed to create ap_pmic device\n", __func__);
		err = PTR_ERR(info->sysfs);
		goto err_device_create;
	}

	err = sysfs_create_group(&info->sysfs->kobj,
				&sec_ap_pmic_attr_group);
	if (err < 0) {
		pr_err("%s: Failed to create sysfs group\n", __func__);
		goto err_sysfs_group;
	}


	/* pmic pon logging for eRR.p */
	entry = proc_create("pwrsrc", 0444, NULL, &proc_pwrsrc_operation);
	if (unlikely(!entry)) {
		err =  -ENOMEM;
		goto err_sysfs_group;
	}
	info->proc = entry;

	pr_info("%s: ap_pmic successfully inited.\n", __func__);

	return 0;

	/* proc_remove(info->proc); */
err_sysfs_group:
	sec_device_destroy(info->sysfs->devt);
err_device_create:
	return err;
}

static int sec_ap_pmic_remove(struct platform_device *pdev)
{
	struct sec_ap_pmic_info *info = platform_get_drvdata(pdev);

	proc_remove(info->proc);
	sec_device_destroy(info->sysfs->devt);
	return 0;
}

static const struct of_device_id sec_ap_pmic_match_table[] = {
	{ .compatible = "samsung,sec-ap-pmic" },
	{}
};

static struct platform_driver sec_ap_pmic_driver = {
	.driver = {
		.name = "samsung,sec-ap-pmic",
		.of_match_table = sec_ap_pmic_match_table,
	},
	.probe = sec_ap_pmic_probe,
	.remove = sec_ap_pmic_remove,
};

module_platform_driver(sec_ap_pmic_driver);

MODULE_DESCRIPTION("sec_ap_pmic driver");
MODULE_AUTHOR("Jiman Cho <jiman85.cho@samsung.com");
MODULE_LICENSE("GPL");
