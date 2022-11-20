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
#include <linux/input/qpnp-power-on.h>
#include <linux/sec_debug.h>

static struct device *sec_ap_pmic_dev;

static ssize_t chg_det_show(struct device *in_dev,
				struct device_attribute *attr, char *buf)
{
	int ret = 0;

	ret = qpnp_pon_check_chg_det();

	pr_info("%s: ret = %d\n", __func__, ret);
	return sprintf(buf, "%d\n", ret);
}

static DEVICE_ATTR_RO(chg_det);

static ssize_t off_reason_show(struct device *in_dev,
				struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%s", qpnp_pon_get_off_reason());
}
static DEVICE_ATTR_RO(off_reason);

#define BOB_SID				3
#define BOB_CONFIG_ADDR		0xA000
#define BOB_DUMP_SIZE		0x100
extern int qpnp_get_dump(int sid, unsigned int reg, void *buf, size_t count);

static ssize_t bob_config_show(struct device *in_dev,
				struct device_attribute *attr, char *buf)
{
	char bob_dump[BOB_DUMP_SIZE];
	ssize_t size = 0;
	int i, j, rc = 0;

	if (!sec_debug_is_enabled())
		return 0;

	rc = qpnp_get_dump(BOB_SID, BOB_CONFIG_ADDR, (void*)bob_dump, BOB_DUMP_SIZE);
	if (rc) {
		pr_err("%s: failed rc=%d\n", __func__, rc);
		return 0;
	}
	
	for(i=0; i<BOB_DUMP_SIZE; i+=0x10) {
		size += sprintf(buf+size, "[%04X]", BOB_CONFIG_ADDR + i);
		for(j=0; j<0x10; j++) {
			size += sprintf(buf+size, " %02X", bob_dump[i+j]);
		}
		size += sprintf(buf+size, "\n");
	}
	
	return size;
}
static DEVICE_ATTR_RO(bob_config);

struct pmgpio_dev {
	int sid;
	unsigned int status;
	unsigned int config;
	int status_size;
	int config_size;
	int gpio_count;
};

ssize_t sec_get_pm_gpio(char *buf)
{
	char dump[20];
	char line[60];
	char* pline;
	ssize_t size = 0;
	ssize_t total_size = 0;
	int q, i, j, rc = 0;
	struct pmgpio_dev devs[2] = {
		{ 0, 0xC008, 0xC040, 1, 7, 13 },
		{ 2, 0xC008, 0xC040, 1, 7, 12 },
	};

	if (!sec_debug_is_enabled())
		return 0;

	for(q=0; q<sizeof(devs)/sizeof(struct pmgpio_dev); q++) {
		for(i=0; i<devs[q].gpio_count; i++) {
			pline = (buf) ? buf + total_size : line;
			size = 0;

			size += sprintf(pline+size, "PM%d GPIO_%02d : ", q, i+1);
			rc = qpnp_get_dump(devs[q].sid, devs[q].status + i*0x100, (void*)dump, devs[q].status_size);
			if (rc) {
				pr_err("%s: failed rc=%d\n", __func__, rc);
				return size;
			}
			size += sprintf(pline+size, "[%04X]", devs[q].status + i*0x100);
			for(j=0; j<devs[q].status_size; j++)
				size += sprintf(pline+size, " %02X", dump[j]);

			rc = qpnp_get_dump(devs[q].sid, devs[q].config + i*0x100, (void*)dump, devs[q].config_size);
			if (rc) {
				pr_err("%s: failed rc=%d\n", __func__, rc);
				return size;
			}
			size += sprintf(pline+size, "  [%04X]", devs[q].config + i*0x100);
			for(j=0; j<devs[q].config_size; j++)
				size += sprintf(pline+size, " %02X", dump[j]);

			if (buf) {
				size += sprintf(pline+size, "\n");
				total_size += size;
			}
			else
				pr_info("%s\n", line);
		}
	}

	return total_size;
}
EXPORT_SYMBOL(sec_get_pm_gpio);

static ssize_t gpios_show(struct device *in_dev,
				struct device_attribute *attr, char *buf)
{
	return sec_get_pm_gpio(buf);
}
static DEVICE_ATTR_RO(gpios);

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

	size += chg_det_show(in_dev, attr, buf+size);
	size += off_reason_show(in_dev, attr, buf+size);
	size += bob_config_show(in_dev, attr, buf+size);
	size += gpios_show(in_dev, attr, buf+size);
	size += gpio_exp_show(in_dev, attr, buf+size);

	return size;
}
static DEVICE_ATTR_RO(debug);

static int __init sec_ap_pmic_init(void)
{
	int err;

	sec_ap_pmic_dev = sec_device_create(0, NULL, "ap_pmic");

	if (unlikely(IS_ERR(sec_ap_pmic_dev))) {
		pr_err("%s: Failed to create ap_pmic device\n", __func__);
		err = PTR_ERR(sec_ap_pmic_dev);
		goto err_device_create;
	}

	err = device_create_file(sec_ap_pmic_dev, &dev_attr_chg_det);
	if (unlikely(err)) {
		pr_err("%s: Failed to create chg_det\n", __func__);
		goto err_device_create;
	}

	err = device_create_file(sec_ap_pmic_dev, &dev_attr_off_reason);
	if (unlikely(err)) {
		pr_err("%s: Failed to create off_reason\n", __func__);
		goto err_device_create;
	}

	err = device_create_file(sec_ap_pmic_dev, &dev_attr_bob_config);
	if (unlikely(err)) {
		pr_err("%s: Failed to create bob_config\n", __func__);
		goto err_device_create;
	}

	err = device_create_file(sec_ap_pmic_dev, &dev_attr_gpios);
	if (unlikely(err)) {
		pr_err("%s: Failed to create gpios\n", __func__);
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

	pr_info("%s: ap_pmic successfully inited.\n", __func__);

	return 0;

err_device_create:
	return err;
}
module_init(sec_ap_pmic_init);

MODULE_DESCRIPTION("sec_ap_pmic driver");
MODULE_AUTHOR("Jiman Cho <jiman85.cho@samsung.com");
MODULE_LICENSE("GPL");
