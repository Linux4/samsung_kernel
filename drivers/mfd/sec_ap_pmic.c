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
#include <linux/mailbox_client.h>
#include <linux/mailbox/qmp.h>
#include <linux/uaccess.h>
#include <linux/mailbox_controller.h>

/* Changes to send aop messages + */
#define MAX_MSG_SIZE 96 /* Imposed by the remote*/

struct qmp_debug_data {
	struct qmp_pkt pkt;
	char buf[MAX_MSG_SIZE + 1];
};

static struct qmp_debug_data _data_pkt[MBOX_TX_QUEUE_LEN];
static struct mbox_chan *sec_chan;
static struct mbox_client *sec_cl;
static bool is_started = false;
static bool lpm_mon_active = false;

static DEFINE_MUTEX(qmp_debug_mutex);

#if IS_ENABLED(CONFIG_SEC_FACTORY)
const char aop_start_lpm_mon_msg[] = "{class: lpm_mon, type: cxpc, dur: 1000, flush: 10, ts_adj: 1}";
const char aop_stop_lpm_mon_msg[] = "{class: lpm_mon, type: cxpc, dur: 1000, flush: 10, ts_adj: 1, log_once: 1}";
const char aop_restart_lpm_mon_msg[] = "{class: lpm_mon, type: cxpc, dur: 1000, flush: 10, ts_adj: 1, log_once: 0}";
#else  // User bin
const char aop_start_lpm_mon_msg[] = "{class: lpm_mon, type: cxpc, dur: 6000, flush: 10, ts_adj: 1}";
const char aop_stop_lpm_mon_msg[] = "{class: lpm_mon, type: cxpc, dur: 6000, flush: 10, ts_adj: 1, log_once: 1}";
const char aop_restart_lpm_mon_msg[] = "{class: lpm_mon, type: cxpc, dur: 6000, flush: 10, ts_adj: 1, log_once: 0}";
#endif /* CONFIG_SEC_FACTORY */

int aop_lpm_mon_enable(bool enable)
{
	static int count;
	const char *msg = NULL;
	size_t len = 0;
	int err = 0;

	mutex_lock(&qmp_debug_mutex);

	if (count >= MBOX_TX_QUEUE_LEN)
		count = 0;

	memset(&(_data_pkt[count]), 0, sizeof(_data_pkt[count]));

	if (enable) {
		if(!is_started) {
			msg = aop_start_lpm_mon_msg;
			len = strlen(aop_start_lpm_mon_msg);
			is_started = true;
		} else {
			msg = aop_restart_lpm_mon_msg;
			len = strlen(aop_restart_lpm_mon_msg);
		}
	} else {
		msg = aop_stop_lpm_mon_msg;
		len = strlen(aop_stop_lpm_mon_msg);
	}

	strncpy(_data_pkt[count].buf, msg, len);

	_data_pkt[count].pkt.size = (len + 0x3) & ~0x3;
	_data_pkt[count].pkt.data = _data_pkt[count].buf;

	err = mbox_send_message(sec_chan, &(_data_pkt[count].pkt));

	if (err < 0) {
		pr_err("%s: Failed to send qmp request (%d)\n", __func__, err);
		return err;
	} else
		count++;

	lpm_mon_active = enable;

	mutex_unlock(&qmp_debug_mutex);

	pr_info("%s: %s\n", __func__, lpm_mon_active ? "enabled" : "disabled");
	return err;
}
EXPORT_SYMBOL(aop_lpm_mon_enable);

#if IS_ENABLED(CONFIG_QTI_SYS_PM_VX)
extern int debug_vx_show(void);
#endif /* CONFIG_QTI_SYS_PM_VX */

int _debug_vx_show(void)
{
	int ret = 0;
#if IS_ENABLED(CONFIG_QTI_SYS_PM_VX)
	if (lpm_mon_active)
		ret = debug_vx_show();
#endif /* CONFIG_QTI_SYS_PM_VX */
	return ret;

}
EXPORT_SYMBOL(_debug_vx_show);
/* Changes to send aop messages -*/

/* for enable/disable manual reset, from retail group's request */
extern void do_keyboard_notifier(int onoff);
extern int sec_set_s2_reset_onoff(int val);
extern int sec_get_s2_reset_onoff(void);

static struct device *sec_ap_pmic_dev;

struct sec_ap_pmic_info {
	struct device		*dev;
	int chg_det_gpio;
};

static struct sec_ap_pmic_info *sec_ap_pmic_data;

static ssize_t manual_reset_show(struct device *in_dev,
				struct device_attribute *attr, char *buf)
{
	int ret = 0;

	ret = sec_get_s2_reset_onoff();

	pr_info("%s: ret = %d\n", __func__, ret);
	return sprintf(buf, "%d\n", !ret);
}

static ssize_t manual_reset_store(struct device *in_dev,
		struct device_attribute *attr, const char *buf, size_t len)
{
	int onoff = 0;

	if (kstrtoint(buf, 10, &onoff))
		return -EINVAL;

	pr_info("%s: onoff(%d)\n", __func__, onoff);

	/* unregister keyboard_notifier first before register */
	if (onoff)
		do_keyboard_notifier(0);
	
	do_keyboard_notifier(onoff);
	sec_set_s2_reset_onoff(onoff);

	return len;
}
static DEVICE_ATTR_RW(manual_reset);

static ssize_t chg_det_show(struct device *in_dev,
				struct device_attribute *attr, char *buf)
{
	int val;

	val = gpio_get_value(sec_ap_pmic_data->chg_det_gpio);

	pr_info("%s: ret = %d\n", __func__, val ? 0 : 1);
	return sprintf(buf, "%d\n", val ? 0 : 1);
}
static DEVICE_ATTR_RO(chg_det);

static struct attribute *sec_ap_pmic_attributes[] = {
	&dev_attr_chg_det.attr,
	&dev_attr_manual_reset.attr,
	NULL,
};

static struct attribute_group sec_ap_pmic_attr_group = {
	.attrs = sec_ap_pmic_attributes,
};

#if IS_ENABLED(CONFIG_SEC_EXT_THERMAL_MONITOR)
typedef struct sec_temp_table_data {
	int adc;
	int temp;
} sec_temp_table_data_t;

typedef struct sec_adc_tm_data {
	sec_temp_table_data_t * batt_temp_table;
	sec_temp_table_data_t * usb_temp_table;
	unsigned int batt_temp_table_size;
	unsigned int usb_temp_table_size;
} sec_adc_tm_data_t;

#define USB_THM_CH	0x45
#define WPC_THM_CH	0x4B

static sec_adc_tm_data_t * sec_data;

int sec_convert_adc_to_temp(unsigned int adc_ch, int temp_adc)
{
	int temp = 25000;
	int low = 0;
	int high = 0;
	int mid = 0;
	const sec_temp_table_data_t *temp_adc_table = {0 , };
	unsigned int temp_adc_table_size = 0;

	if (!sec_data) {
		pr_info("%s: battery data is not ready yet\n", __func__);
		goto temp_to_adc_goto;
	}

	switch (adc_ch) {
	case USB_THM_CH:
		temp_adc_table = sec_data->usb_temp_table;
		temp_adc_table_size = sec_data->usb_temp_table_size;
		break;
	case WPC_THM_CH:
		temp_adc_table = sec_data->batt_temp_table;
		temp_adc_table_size = sec_data->batt_temp_table_size;
		break;
	default:
		pr_err("%s: Invalid channel\n", __func__);
		goto temp_to_adc_goto;
	}

	if (temp_adc_table[0].adc >= temp_adc) {
		temp = temp_adc_table[0].temp;
		goto temp_to_adc_goto;
	} else if (temp_adc_table[temp_adc_table_size-1].adc <= temp_adc) {
		temp = temp_adc_table[temp_adc_table_size-1].temp;
		goto temp_to_adc_goto;
	}

	high = temp_adc_table_size - 1;

	while (low <= high) {
		mid = (low + high) / 2;
		if (temp_adc_table[mid].adc > temp_adc)
			high = mid - 1;
		else if (temp_adc_table[mid].adc < temp_adc)
			low = mid + 1;
		else {
			temp = temp_adc_table[mid].temp;
			goto temp_to_adc_goto;
		}
	}

	temp = temp_adc_table[high].temp;
	temp += ((temp_adc_table[low].temp - temp_adc_table[high].temp) *
		 (temp_adc - temp_adc_table[high].adc)) /
		(temp_adc_table[low].adc - temp_adc_table[high].adc);

temp_to_adc_goto:
	return (temp == 25000 ? temp : temp * 100);
}
EXPORT_SYMBOL(sec_convert_adc_to_temp);

int sec_get_thr_voltage(unsigned int adc_ch, int temp)
{
	int volt = 0;
	int low = 0;
	int high = 0;
	int mid = 0;
	const sec_temp_table_data_t *temp_adc_table = {0 , };
	unsigned int temp_adc_table_size = 0;

	if (!sec_data) {
		pr_info("%s: battery data is not ready yet\n", __func__);
		goto get_thr_voltage_goto;
	}

	switch (adc_ch) {
	case USB_THM_CH:
		temp_adc_table = sec_data->usb_temp_table;
		temp_adc_table_size = sec_data->usb_temp_table_size;
		break;
	case WPC_THM_CH:
		temp_adc_table = sec_data->batt_temp_table;
		temp_adc_table_size = sec_data->batt_temp_table_size;
		break;
	default:
		pr_err("%s: Invalid channel\n", __func__);
		goto get_thr_voltage_goto;
	}

	if (temp > 900 || temp < -200) {
		pr_err("%s: out of temperature\n", __func__);
		goto get_thr_voltage_goto;
	}

	high = temp_adc_table_size - 1;

	while (low <= high) {
		mid = (low + high) / 2;
		if (temp_adc_table[mid].temp < temp)
			high = mid - 1;
		else if (temp_adc_table[mid].temp > temp)
			low = mid + 1;
		else {
			volt = temp_adc_table[mid].adc;
			goto get_thr_voltage_goto;
		}
	}

	volt = temp_adc_table[mid].adc;
get_thr_voltage_goto:
	return volt;
}
EXPORT_SYMBOL(sec_get_thr_voltage);
#endif /* CONFIG_SEC_EXT_THERMAL_MONITOR */

static int sec_ap_pmic_probe(struct platform_device *pdev)
{
	struct device_node *node = pdev->dev.of_node;
	struct sec_ap_pmic_info *info;
	int err;
#if IS_ENABLED(CONFIG_SEC_EXT_THERMAL_MONITOR)
	int i, value, len = 0;
	struct device_node *np;
	const u32 *p;
#endif /* CONFIG_SEC_EXT_THERMAL_MONITOR */

	if (!node) {
		dev_err(&pdev->dev, "device-tree data is missing\n");
		return -ENXIO;
	}

	info = devm_kzalloc(&pdev->dev, sizeof(*info), GFP_KERNEL);
	if (!info) {
		dev_err(&pdev->dev, "%s: Fail to alloc info\n", __func__);
		return -ENOMEM;
	}

	platform_set_drvdata(pdev, info);
	info->dev = &pdev->dev;
	sec_ap_pmic_data = info;

	info->chg_det_gpio = of_get_named_gpio(node, "chg_det_gpio", 0);

#if IS_ENABLED(CONFIG_SEC_EXT_THERMAL_MONITOR)
	sec_data = devm_kzalloc(&pdev->dev, sizeof(*sec_data), GFP_KERNEL);
	if (!sec_data)
		return -ENOMEM;

	np = of_parse_phandle(node, "batt_node", 0);
	if (!np) {
		pr_err("%s: np is NULL\n", __func__);
		return -ENOENT;
	}

	p = of_get_property(np, "battery,temp_table_adc", &len);
	if (!p) {
		pr_err("%s: p is NULL\n", __func__);
		return -ENOENT;
	}

	sec_data->batt_temp_table_size = len / sizeof(u32);
	sec_data->batt_temp_table = 
			devm_kzalloc(&pdev->dev, sizeof(sec_temp_table_data_t) *
			sec_data->batt_temp_table_size, GFP_KERNEL);
	if(!sec_data->batt_temp_table) {
		pr_err("%s : batt table is NULL\n", __func__);
		return -ENOMEM;
	}

	for (i = 0; i < sec_data->batt_temp_table_size; i++) {
		err = of_property_read_u32_index(np,
						"battery,temp_table_adc", i, &value);
		sec_data->batt_temp_table[i].adc = (int)value;
		if (err)
			pr_err("%s : batt adc is empty\n", __func__);

		err = of_property_read_u32_index(np,
						 "battery,temp_table_data", i, &value);
		sec_data->batt_temp_table[i].temp = (int)value;
		if (err)
			pr_err("%s : batt temp is empty\n", __func__);
	}

	p = of_get_property(np, "battery,usb_temp_table_adc", &len);
	if (!p) {
		pr_err("%s: p is NULL\n", __func__);
		return -ENOENT;
	}

	sec_data->usb_temp_table_size =  len / sizeof(u32);
	sec_data->usb_temp_table = 
			devm_kzalloc(&pdev->dev, sizeof(sec_temp_table_data_t) *
			sec_data->usb_temp_table_size, GFP_KERNEL);
	if(!sec_data->usb_temp_table) {
		pr_err("%s : usb table is NULL\n", __func__);
		return -ENOMEM;
	}

	for (i = 0; i < sec_data->usb_temp_table_size; i++) {
		err = of_property_read_u32_index(np,
						 "battery,usb_temp_table_adc", i, &value);
		sec_data->usb_temp_table[i].adc = (int)value;
		if (err)
			pr_err("%s : usb adc is empty\n", __func__);

		err = of_property_read_u32_index(np,
						 "battery,usb_temp_table_data", i, &value);
		sec_data->usb_temp_table[i].temp = (int)value;
		if (err)
			pr_err("%s : usb temp is empty\n", __func__);
	}
#endif /* CONFIG_SEC_EXT_THERMAL_MONITOR */
	
	sec_ap_pmic_dev = sec_device_create(NULL, "ap_pmic");

	if (unlikely(IS_ERR(sec_ap_pmic_dev))) {
		pr_err("%s: Failed to create ap_pmic device\n", __func__);
		err = PTR_ERR(sec_ap_pmic_dev);
		goto err_device_create;
	}

	/* Setup mailbox to send aop message + */
	sec_cl = devm_kzalloc(&pdev->dev, sizeof(*sec_cl), GFP_KERNEL);
	if (!sec_cl)
		return -ENOMEM;
	
	sec_cl->dev = &pdev->dev;
	sec_cl->tx_block = true;
	sec_cl->tx_tout = 1000;
	sec_cl->knows_txdone = false;

	sec_chan = mbox_request_channel(sec_cl, 0);
	if (IS_ERR(sec_chan)) {
		dev_err(&pdev->dev, "Failed to acquire mbox channel (%d)\n", PTR_ERR(sec_chan));
		return PTR_ERR(sec_chan);
	}
	/* Setup mailbox to send aop message - */

	err = sysfs_create_group(&sec_ap_pmic_dev->kobj,
				&sec_ap_pmic_attr_group);
	if (err < 0) {
		pr_err("%s: Failed to create sysfs group\n", __func__);
		goto err_device_create;
	}
	
	pr_info("%s: ap_pmic successfully inited.\n", __func__);

	return 0;

err_device_create:
	/* Free mailbox to send aop message + */
	mbox_free_channel(sec_chan);
	sec_chan = NULL;
	/* Free mailbox to send aop message - */
	return err;
}

static int sec_ap_pmic_remove(struct platform_device *pdev)
{
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
