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
#include <linux/sec_debug.h>
#include <linux/seq_file.h>
#include <linux/samsung/debug/sec_crashkey_long.h>
#include <linux/mfd/sec_ap_pmic.h>
#include <trace/events/power.h>
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
static bool lpm_mon_active;
static const char aop_start_lpm_mon_msg[] = "{class: lpm_mon, type: cxpc, dur: 1000, flush: 10, ts_adj: 1}";

static DEFINE_MUTEX(qmp_debug_mutex);

int aop_lpm_mon_enable(bool enable)
{
	static int count;
	const char *msg = NULL;
	size_t len = 0;
	int err = 0;

	if (enable) {
		mutex_lock(&qmp_debug_mutex);

		if (count >= MBOX_TX_QUEUE_LEN)
			count = 0;

		memset(&(_data_pkt[count]), 0, sizeof(_data_pkt[count]));

		msg = aop_start_lpm_mon_msg;
		len = strlen(aop_start_lpm_mon_msg);
		strncpy(_data_pkt[count].buf, msg, len);

		_data_pkt[count].pkt.size = (len + 0x3) & ~0x3;
		_data_pkt[count].pkt.data = _data_pkt[count].buf;

		err = mbox_send_message(sec_chan, &(_data_pkt[count].pkt));
		if (err < 0)
			pr_err("%s: Failed to send qmp request (%d)\n", __func__, err);
		else
			count++;

		mutex_unlock(&qmp_debug_mutex);
	}

	lpm_mon_active = enable;

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
extern int sec_get_s2_reset(enum sec_pon_type type);
extern int sec_set_pm_key_wk_init(enum sec_pon_type type, int en);
extern int sec_get_pm_key_wk_init(enum sec_pon_type type);

extern void msm_gpio_print_enabled(void);
extern void pmic_gpio_sec_dbg_enabled(void);

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

	ret = sec_get_s2_reset(SEC_PON_KPDPWR_RESIN);

	pr_info("%s: ret=%d\n", __func__, ret);
	return sprintf(buf, "%d\n", !ret);
}

static ssize_t manual_reset_store(struct device *in_dev,
		struct device_attribute *attr, const char *buf, size_t len)
{
	int onoff = 0;

	if (kstrtoint(buf, 10, &onoff))
		return -EINVAL;

	pr_info("%s: onoff=%d\n", __func__, onoff);
#if IS_ENABLED(CONFIG_SEC_CRASHKEY_LONG)
	if (onoff)
		sec_crashkey_long_connect_to_input_evnet();
	else
		sec_crashkey_long_disconnect_from_input_event();
#endif

	return len;
}
static DEVICE_ATTR_RW(manual_reset);

static ssize_t chg_det_show(struct device *in_dev,
				struct device_attribute *attr, char *buf)
{
	int val;

	val = gpio_get_value(sec_ap_pmic_data->chg_det_gpio);

	pr_info("%s: ret=%d\n", __func__, val ? 0 : 1);
	return sprintf(buf, "%d\n", val ? 0 : 1);
}
static DEVICE_ATTR_RO(chg_det);

static ssize_t wake_enabled_show(struct device *in_dev,
				struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", (sec_get_pm_key_wk_init(SEC_PON_KPDPWR) && sec_get_pm_key_wk_init(SEC_PON_RESIN)) ? 1 : 0);
}

static ssize_t wake_enabled_store(struct device *in_dev,
		struct device_attribute *attr, const char *buf, size_t len)
{
	int onoff;
	int ret;

	if (kstrtoint(buf, 10, &onoff) < 0)
		return -EINVAL;

	pr_info("%s: onoff=%d\n", __func__, onoff);
	ret = sec_set_pm_key_wk_init(SEC_PON_KPDPWR, onoff);
	pr_info("%s: PWR ret=%d\n", __func__, ret);
	ret = sec_set_pm_key_wk_init(SEC_PON_RESIN, onoff);
	pr_info("%s: RESIN ret=%d\n", __func__, ret);

	return len;
}
static DEVICE_ATTR_RW(wake_enabled);


static ssize_t volkey_wakeup_show(struct device *in_dev,
				struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", sec_get_pm_key_wk_init(SEC_PON_RESIN));
}

static ssize_t volkey_wakeup_store(struct device *in_dev,
		struct device_attribute *attr, const char *buf, size_t len)
{
	int onoff;
	int ret;

	if (kstrtoint(buf, 10, &onoff) < 0)
		return -EINVAL;

	pr_info("%s: onoff=%d\n", __func__, onoff);
	ret = sec_set_pm_key_wk_init(SEC_PON_RESIN, onoff);
	pr_info("%s: ret=%d\n", __func__, ret);

	return len;
}
static DEVICE_ATTR_RW(volkey_wakeup);

static bool gpio_dump_enabled;
static ssize_t gpio_dump_show(struct device *in_dev,
				struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", (gpio_dump_enabled) ? 1 : 0);
}

static ssize_t gpio_dump_store(struct device *in_dev,
		struct device_attribute *attr, const char *buf, size_t len)
{
	int onoff, ret;

	if (kstrtoint(buf, 10, &onoff) < 0)
		return -EINVAL;

	pr_info("%s: onoff=%d\n", __func__, onoff);
	gpio_dump_enabled = (onoff) ? true : false;

	ret = aop_lpm_mon_enable(gpio_dump_enabled);
	if (ret) {
		pr_err("Unable to set aop_lpm_mon_enable: %d\n", ret);
		return ret;
	}

	return len;
}
static DEVICE_ATTR_RW(gpio_dump);

#define PARAM0_IVALID			1
#define PARAM0_LESS_THAN_0		2

#define DEFAULT_LEN_STR         1023

#define default_scnprintf(buf, offset, fmt, ...)			\
do {									\
	offset += scnprintf(&(buf)[offset], DEFAULT_LEN_STR - (size_t)offset, \
			fmt, ##__VA_ARGS__);				\
} while (0)

static void check_format(char *buf, ssize_t *size, int max_len_str)
{
	int i = 0, cnt = 0, pos = 0;

	if (!buf || *size <= 0)
		return;

	if (*size >= max_len_str)
		*size = max_len_str - 1;

	while (i < *size && buf[i]) {
		if (buf[i] == '"') {
			cnt++;
			pos = i;
		}

		if ((buf[i] < 0x20) || (buf[i] == 0x5C) || (buf[i] > 0x7E))
			buf[i] = ' ';
		i++;
	}

	if (cnt % 2) {
		if (pos == *size - 1) {
			buf[*size - 1] = '\0';
		} else {
			buf[*size - 1] = '"';
			buf[*size] = '\0';
		}
	}
}

static int get_param0(const char *name)
{
	struct device_node *np = of_find_node_by_path("/soc/sec_ap_param");
	u32 val;
	int ret;

	if (!np) {
		pr_err("No sec_avi_data found\n");
		return -PARAM0_IVALID;
	}

	ret = of_property_read_u32(np, name, &val);
	if (ret) {
		pr_err("failed to get %s from node\n", name);
		return -PARAM0_LESS_THAN_0;
	}

	return val;
}

#define GET_V(A)	((A) / 1000)
#define GET_MV(A)	((A) % 1000)
static ssize_t show_ap_info(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	ssize_t info_size = 0;

	default_scnprintf(buf, info_size, "\"GC_OPV_3\":\"%d.%03d\"", GET_V(get_param0("go")), GET_MV(get_param0("go")));
	default_scnprintf(buf, info_size, ",\"SC_OPV_3\":\"%d.%03d\"", GET_V(get_param0("so")), GET_MV(get_param0("so")));
	default_scnprintf(buf, info_size, ",\"GC_PRM\":\"%d\"",	get_param0("gi"));
	default_scnprintf(buf, info_size, ",\"SC_PRM\":\"%d\"",	get_param0("si"));

	default_scnprintf(buf, info_size, ",\"DOUR\":\"%d\"", get_param0("dour"));
	default_scnprintf(buf, info_size, ",\"DOUB\":\"%d\"", get_param0("doub"));

	check_format(buf, &info_size, DEFAULT_LEN_STR);

	return info_size;
}
static DEVICE_ATTR(ap_info, 0440, show_ap_info, NULL);


static struct attribute *sec_ap_pmic_attributes[] = {
	&dev_attr_manual_reset.attr,
	&dev_attr_chg_det.attr,
	&dev_attr_wake_enabled.attr,
	&dev_attr_volkey_wakeup.attr,
	&dev_attr_gpio_dump.attr,
	&dev_attr_ap_info.attr,
	NULL,
};

static struct attribute_group sec_ap_pmic_attr_group = {
	.attrs = sec_ap_pmic_attributes,
};

static void gpio_state_debug_suspend_trace_probe(void *unused,
					const char *action, int val, bool start)
{
	/* SUSPEND: start(1), val(1), action(machine_suspend) */
	if (gpio_dump_enabled && start && val > 0 && !strcmp("machine_suspend", action)) {
		msm_gpio_print_enabled();
		pmic_gpio_sec_dbg_enabled();
	}
}

static int sec_ap_pmic_probe(struct platform_device *pdev)
{
	struct device_node *node = pdev->dev.of_node;
	struct sec_ap_pmic_info *info;
	int err;

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

#if IS_ENABLED(CONFIG_SEC_CLASS)
	sec_ap_pmic_dev = sec_device_create(NULL, "ap_pmic");

	if (unlikely(IS_ERR(sec_ap_pmic_dev))) {
		pr_err("%s: Failed to create ap_pmic device\n", __func__);
		err = PTR_ERR(sec_ap_pmic_dev);
		goto err_device_create;
	}

	err = sysfs_create_group(&sec_ap_pmic_dev->kobj,
				&sec_ap_pmic_attr_group);
	if (err < 0) {
		pr_err("%s: Failed to create sysfs group\n", __func__);
		goto err_device_create;
	}
#endif

	/* Register callback for cheking subsystem stats */
	err = register_trace_suspend_resume(
		gpio_state_debug_suspend_trace_probe, NULL);
	if (err) {
		pr_err("%s: Failed to register suspend trace callback, ret=%d\n",
			__func__, err);
	}
	
	pr_info("%s: ap_pmic successfully inited.\n", __func__);

	return 0;

#if IS_ENABLED(CONFIG_SEC_CLASS)
err_device_create:
	sec_device_destroy(sec_ap_pmic_dev->devt);
	/* Free mailbox to send aop message + */
	mbox_free_channel(sec_chan);
	sec_chan = NULL;
	/* Free mailbox to send aop message - */
	return err;
#endif
}

static int sec_ap_pmic_remove(struct platform_device *pdev)
{
	int ret;

#if IS_ENABLED(CONFIG_SEC_CLASS)
	if (sec_ap_pmic_dev) {
		sec_device_destroy(sec_ap_pmic_dev->devt);
	}
#endif

	ret = unregister_trace_suspend_resume(
		gpio_state_debug_suspend_trace_probe, NULL);

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
MODULE_SOFTDEP("pre: sec_class");
MODULE_SOFTDEP("pre: sec_crashkey_long");
MODULE_SOFTDEP("pre: pm8941-pwrkey");
MODULE_AUTHOR("Jiman Cho <jiman85.cho@samsung.com");
MODULE_LICENSE("GPL");
