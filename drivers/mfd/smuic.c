/*
 * Copyright (C) 2015, SAMSUNG Corporation.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/err.h>
#include <linux/power_supply.h>
#include <linux/types.h>
#include <linux/interrupt.h>
#include <linux/spinlock.h>
#include <linux/gpio.h>
#include <linux/irq.h>
#include <linux/slab.h>
#include <soc/sprd/adi.h>
#include <soc/sprd/sci_glb_regs.h>
#include <linux/workqueue.h>
#include <linux/mutex.h>
#include <linux/notifier.h>
#include <linux/mfd/muic_noti.h>
#include <linux/of_gpio.h>
#include <linux/delay.h>
#include <soc/sprd/pinmap.h>
#include <linux/io.h>
#include <linux/device.h>
#include <asm/sec/sec_debug.h>

#if defined(CONFIG_MUIC_SUPPORT_RUSTPROOF)
#include <linux/battery/sec_charger.h>
extern int get_sec_debug_level(void);
static bool is_close_uart_path;
#endif
#ifdef CONFIG_MUIC_FACTORY_EVENT
static bool is_factory_start;
#endif
extern int sec_vf_adc_check(void);

#define MAX_BUF_SIZE	32

struct smuic {
	struct work_struct detect_work;
	struct workqueue_struct *detect_wq;
	int vbus;
	spinlock_t slock;
	struct mutex mlock;
	int gpio_irq;
	int gpio_uart_txd;
	int gpio_uart_rxd;
	int type;
};
struct smuic *g_info;

enum adapter_type {
	ADP_TYPE_UNKNOWN = 0,	/* unknown */
	ADP_TYPE_CDP = 1,	/* Charging Downstream Port, USB&standard charger */
	ADP_TYPE_DCP = 2,	/* Dedicated Charging Port, standard charger */
	ADP_TYPE_SDP = 4,	/* Standard Downstrea Port, USB and nonstandard charge */
};

enum cable_type {
	CABLE_USB = 0,
	CABLE_TA,
	CABLE_UNKNOWN,
};

enum jig_type {
	JIG_NONE = 0,
	JIG_UART_BOOT_ON,
	JIG_USB_BOOT_ON,
};

#ifdef CONFIG_CHARGER_RT9532
extern unsigned int jig_set;
#endif

static void usb_detect_works(struct work_struct *);
static int simp_charger_is_adapter(void);
static void simp_charger_plugin(struct smuic *);
static void simp_charger_plugout(struct smuic *);
static void set_uart_off(int, int, bool);

static ATOMIC_NOTIFIER_HEAD(adapter_notifier_list);
int register_adapter_notifier(struct notifier_block *nb)
{
	return atomic_notifier_chain_register(&adapter_notifier_list, nb);
}
EXPORT_SYMBOL(register_adapter_notifier);
int unregister_adapter_notifier(struct notifier_block *nb)
{
	return atomic_notifier_chain_unregister(&adapter_notifier_list, nb);
}
EXPORT_SYMBOL(unregister_adapter_notifier);

static void set_uart_off(int gpio_uart_txd, int gpio_uart_rxd, bool mode)
{
	uint32_t val;

	if (mode) {
		/* set AP_UART_TXD to gpio */
		val = __raw_readl(CTL_PIN_BASE + REG_PIN_U1TXD) & ~BITS_PIN_AF(0) | BITS_PIN_AF(3);
		__raw_writel(val, CTL_PIN_BASE + REG_PIN_U1TXD);

		/* set AP_UART_RXD to gpio */
		val = __raw_readl(CTL_PIN_BASE + REG_PIN_U1RXD) & ~BITS_PIN_AF(0) | BITS_PIN_AF(3);
		__raw_writel(val, CTL_PIN_BASE + REG_PIN_U1RXD);

		gpio_direction_output(gpio_uart_txd, 0);
		gpio_direction_output(gpio_uart_rxd, 0);
#ifdef CONFIG_MUIC_SUPPORT_RUSTPROOF
		is_close_uart_path = true;
#endif
	} else {
		/* set AP_UART_TXD to UART */
		val = __raw_readl(CTL_PIN_BASE + REG_PIN_U1TXD) & ~BITS_PIN_AF(3) | BITS_PIN_AF(0);
		__raw_writel(val, CTL_PIN_BASE + REG_PIN_U1TXD);

		/* set AP_UART_RXD to UART */
		val = __raw_readl(CTL_PIN_BASE + REG_PIN_U1RXD) & ~BITS_PIN_AF(3) | BITS_PIN_AF(0);
		__raw_writel(val, CTL_PIN_BASE + REG_PIN_U1RXD);
#ifdef CONFIG_MUIC_SUPPORT_RUSTPROOF
		is_close_uart_path = false;
#endif
	}

}

static void usb_detect_works(struct work_struct *work)
{
	unsigned long flags;
	int plug_in;
	struct smuic *info = container_of(work, struct smuic, detect_work);

	spin_lock_irqsave(&info->slock, flags);
	plug_in = info->vbus;
	spin_unlock_irqrestore(&info->slock, flags);

	mutex_lock(&info->mlock);
	if (plug_in) {
		pr_debug("smuic: usb detect plug in,vbus pull up\n");
		simp_charger_plugin(info);
	} else {
		pr_debug("smuic: usb detect plug out,vbus pull down\n");
		simp_charger_plugout(info);
	}
	mutex_unlock(&info->mlock);
}

static irqreturn_t usb_detect_handler(int irq, void *data)
{
	int value;
	unsigned long flags;
	struct smuic *info = (struct muic *)data;

	value = !!gpio_get_value(info->gpio_irq);
	if (value)
		irq_set_irq_type(irq, IRQ_TYPE_LEVEL_LOW);
	else
		irq_set_irq_type(irq, IRQ_TYPE_LEVEL_HIGH);

	spin_lock_irqsave(&info->slock, flags);
	info->vbus = value;
	spin_unlock_irqrestore(&info->slock, flags);

	queue_work(info->detect_wq, &info->detect_work);

	return IRQ_HANDLED;
}

static int simp_charger_is_adapter(void)
{
	int charger_status;
	int ret;

#ifdef CONFIG_CHARGER_RT9532
	if (jig_set == JIG_USB_BOOT_ON) {
		pr_info("smuic: JIG_USB_BOOT_ON\n");
		return ADP_TYPE_SDP;
	} else if (jig_set == JIG_UART_BOOT_ON && sec_vf_adc_check()) {
		pr_info("smuic: JIG_UART_BOOT_ON && inBatt\n");
		return ADP_TYPE_DCP;
	}
#endif
	charger_status = sci_adi_read(ANA_REG_GLB_CHGR_STATUS)
	    & (BIT_CDP_INT | BIT_DCP_INT | BIT_SDP_INT);

	pr_info("smuic: charger_status is 0x%x\n", charger_status);

	switch (charger_status) {
	case BIT_DCP_INT:	/* TA: 0x40 */
		ret = ADP_TYPE_DCP;
		break;
	case BIT_SDP_INT:	/* USB: 0x80 */
		ret = ADP_TYPE_SDP;
		break;
	default:		/* unknown */
		ret = ADP_TYPE_UNKNOWN;
		break;
	}
	return ret;
}


static void simp_charger_plugin(struct smuic *data)
{
	int ret;
	struct muic_notifier_param param;

#ifdef CONFIG_CHARGER_RT9532
	if (jig_set == JIG_UART_BOOT_ON && !sec_vf_adc_check()) {
		pr_info("smuic: JIG Boot\n");
		return;
	}
#endif

	msleep(30);
	ret = simp_charger_is_adapter();
	if (ret == ADP_TYPE_SDP) {
		pr_info("smuic: USB attached!\n");
		data->type = CABLE_USB;
		param.cable_type = MUIC_CABLE_TYPE_USB;
		param.vbus_status = 1;
		atomic_notifier_call_chain(&adapter_notifier_list, MUIC_USB_ATTACH_NOTI, &param);
	} else {
		if (ret == ADP_TYPE_DCP)
			pr_info("smuic: TA attached!\n");
		else
			pr_info("smuic: Unknown cable attached!\n");
		data->type = CABLE_TA;
		param.cable_type = MUIC_CABLE_TYPE_REGULAR_TA;
		atomic_notifier_call_chain(&adapter_notifier_list, MUIC_CHG_ATTACH_NOTI, &param);
	}
}

static void simp_charger_plugout(struct smuic *data)
{
	struct muic_notifier_param param;

	param.cable_type = MUIC_CABLE_TYPE_NONE;
	if (data->type == CABLE_USB) {
		param.vbus_status = 0;
		atomic_notifier_call_chain(&adapter_notifier_list, MUIC_USB_DETACH_NOTI, &param);
		pr_info("smuic: USB detached!\n");
	} else if (data->type == CABLE_TA) {
		atomic_notifier_call_chain(&adapter_notifier_list, MUIC_CHG_DETACH_NOTI, &param);
		pr_info("smuic: TA detached!\n");
	} else
		pr_info("smuic: Unknown cable detached!\n");

	data->type = CABLE_UNKNOWN;
#ifdef CONFIG_CHARGER_RT9532
	if (jig_set)
		jig_set = JIG_NONE;
#endif
}

#ifdef CONFIG_MUIC_FACTORY_EVENT
static ssize_t smuic_show_apo_factory(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	if (is_factory_start)
		return snprintf(buf, MAX_BUF_SIZE, "%s\n", "FACTORY_MODE");

	return snprintf(buf, MAX_BUF_SIZE, "%s\n", "NOT_FACTORY_MODE");
}

static ssize_t smuic_set_apo_factory(struct device *dev,
		struct device_attribute *attr, char *buf, size_t count)
{
	if (!strncmp(buf, "FACTORY_START", 13)) {
		is_factory_start = true;
		pr_info("smuic: %s: set factory mode\n", __func__);
	} else
		pr_err("smuic: %s: wrong command!\n", __func__);

	return count;
}
static DEVICE_ATTR(apo_factory, S_IRUGO | S_IWUGO, smuic_show_apo_factory, smuic_set_apo_factory);
#endif /* CONFIG_MUIC_FACTORY_EVENT */

#ifdef CONFIG_MUIC_SUPPORT_RUSTPROOF
static ssize_t uart_sel_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return snprintf(buf, MAX_BUF_SIZE, "%d\n", is_close_uart_path);
}
static ssize_t uart_en_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return snprintf(buf, MAX_BUF_SIZE, "%d\n", !is_close_uart_path);
}

static ssize_t uart_en_store(struct device *dev,
		struct device_attribute *attr, char *buf, size_t size)
{
	if (!strncmp(buf, "1", 1)) {
		set_uart_off(g_info->gpio_uart_txd, g_info->gpio_uart_rxd, false);
		pr_info("smuic: %s: uart enabled\n", __func__);
	} else {
		set_uart_off(g_info->gpio_uart_txd, g_info->gpio_uart_rxd, true);
		pr_info("smuic: %s: uart disabled\n", __func__);
	}

	return size;
}
static DEVICE_ATTR(uart_sel, S_IRUGO, uart_sel_show, NULL);
static DEVICE_ATTR(uart_en, S_IRUGO | S_IWUSR, uart_en_show, uart_en_store);
#endif /* CONFIG_MUIC_SUPPORT_RUSTPROOF */

static ssize_t usb_state_show_attrs(struct device *dev,
		struct device_Attribute *attr, char *buf)
{
	if (g_info->type == CABLE_USB)
		return snprintf(buf, MAX_BUF_SIZE, "USB_STATE_CONFIGURED\n");

	return snprintf(buf, MAX_BUF_SIZE, "USB_STATE_NOTCONFIGURED\n");
}

static ssize_t usb_sel_show_attrs(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return snprintf(buf, MAX_BUF_SIZE, "PDA");
}
static ssize_t adc_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
#ifdef CONFIG_CHARGER_RT9532
	if (jig_set == JIG_UART_BOOT_ON) {
		pr_info("smuic: %s: JIG UART BOOT ON\n", __func__);
		return snprintf(buf, MAX_BUF_SIZE, "1d\n");
	} else if (jig_set == JIG_USB_BOOT_ON) {
		pr_info("smuic: %s: JIG USB BOOT ON\n", __func__);
		return snprintf(buf, MAX_BUF_SIZE, "19\n");
	}

#endif

	pr_info("smuic: %s: no detect!\n", __func__);
	return snprintf(buf, MAX_BUF_SIZE, "0\n");
}

static ssize_t dev_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
#ifdef CONFIG_CHARGER_RT9532
	if (jig_set == JIG_UART_BOOT_ON) {
		pr_info("smuic: %s: JIG UART BOOT ON\n", __func__);
		return snprintf(buf, MAX_BUF_SIZE, "%s\n", "JIG UART ON");
	} else if (jig_set == JIG_USB_BOOT_ON) {
		pr_info("smuic: %s: JIG USB BOOT ON\n", __func__);
		return snprintf(buf, MAX_BUF_SIZE, "%s\n", "JIG USB ON");
	}

#endif

	pr_info("smuic: %s: no detect!\n", __func__);
	return snprintf(buf, MAX_BUF_SIZE, "0\n");
}

static DEVICE_ATTR(usb_state, S_IRUGO, usb_state_show_attrs, NULL);
static DEVICE_ATTR(usb_sel, S_IRUGO, usb_sel_show_attrs, NULL);
static DEVICE_ATTR(adc, S_IRUGO, adc_show, NULL);
static DEVICE_ATTR(attached_dev, S_IRUGO, dev_show, NULL);

static struct attribute *smuic_attributes[] = {
	&dev_attr_usb_state.attr,
	&dev_attr_usb_sel.attr,
	&dev_attr_adc.attr,
	&dev_attr_attached_dev.attr,
#ifdef CONFIG_MUIC_FACTORY_EVENT
	&dev_attr_apo_factory.attr,
#endif
#ifdef CONFIG_MUIC_SUPPORT_RUSTPROOF
	&dev_attr_uart_sel.attr,
	&dev_attr_uart_en.attr,
#endif
	NULL
};
static const struct attribute_group smuic_group = {
	.attrs = smuic_attributes,
};

static void smuic_gpio_init(int irq, int txd, int rxd)
{
	int ret;

	ret = gpio_request(irq, "UART_IRQ");
	if (ret < 0)
		pr_debug("smuic: failed to request UART_IRQ\n");

	ret = gpio_request(txd, "UART_TXD");
	if (ret < 0)
		pr_err("smuic: failed to request UART_TXD\n");

	ret = gpio_request(rxd, "UART_RXD");
	if (ret < 0)
		pr_err("smuic: failed to request UART_RXD\n");

}

static int __init smuic_init(void)
{
	int ret;
	int plugirq;
	static struct smuic *info;
	struct device *switch_dev;

	info = kzalloc(sizeof(struct smuic), GFP_KERNEL);
	if (!info) {
		pr_err("smuic: failed to alloc smuic info!\n");
		return -ENOMEM;
	}

#ifdef CONFIG_OF
	struct device_node *np;

	np = of_find_node_by_name(NULL, "sec-smuic");
	if (!np) {
		pr_err("smuic: can't find sec,smuic\n");
		ret = -EINVAL;
		goto err1;
	} else {
		ret = of_get_gpio(np, 0);
		if (ret < 0) {
			pr_err("smuic: failed to get GPIO irq\n");
			ret = -EINVAL;
			goto err1;
		}
		info->gpio_irq = ret;

		ret = of_get_gpio(np, 1);
		if (ret < 0) {
			pr_err("smuic: failed to get GPIO UART\n");
			ret = -EINVAL;
			goto err1;
		}
		info->gpio_uart_txd = ret;

		ret = of_get_gpio(np, 2);
		if (ret < 0) {
			pr_err("smuic: failed to get GPIO_UART_RXD\n");
			ret = -EINVAL;
			goto err1;
		}
		info->gpio_uart_rxd = ret;

		smuic_gpio_init(info->gpio_irq, info->gpio_uart_txd, info->gpio_uart_rxd);
		pr_info("smuic: irq:%d txd:%d, rxd:%d\n",
				info->gpio_irq, info->gpio_uart_txd, info->gpio_uart_rxd);
	}
#endif

	mutex_init(&info->mlock);
	INIT_WORK(&info->detect_work, usb_detect_works);
	info->detect_wq = create_singlethread_workqueue("usb_detect_wq");
	if (!info->detect_wq) {
		pr_err("smuic: failed to create workqueue\n");
		ret = -ENOMEM;
		goto err1;
	}

	plugirq = gpio_to_irq(info->gpio_irq);
	ret = request_irq(plugirq, usb_detect_handler,
				IRQF_ONESHOT | IRQF_NO_SUSPEND, "usb_detect", info);
	if (ret) {
		pr_err("smuic: failed to request_irq\n");
		ret = -EINVAL;
		goto err2;
	}

#if defined(CONFIG_MUIC_SUPPORT_RUSTPROOF) && !defined(CONFIG_MUIC_SUPPORT_FACTORY) && !defined(CONFIG_MUIC_SUPPORT_RUSTPROOF_INBATT)
	if (sec_vf_adc_check() && !get_sec_debug_level()) {
		pr_err("smuic: disable UART path!\n");
		set_uart_off(info->gpio_uart_txd, info->gpio_uart_rxd, true);
	}
#endif
	switch_dev = device_create(sec_class, NULL, 0, NULL, "switch");
	if (unlikely(!switch_dev)) {
		pr_err("smuic: failed to create switch_dev\n");
		ret = -ENODEV;
		goto err2;

	}
	ret = sysfs_create_group(&switch_dev->kobj, &smuic_group);
	if (unlikely(ret)) {
		pr_err("smuic: failed to create sysfs group\n");
		ret = -ENODEV;
		goto err3;
	}
	g_info = info;
	pr_info("smuic: successfully initialized!\n");

	return 0;

err3:
	device_destroy(sec_class, 0);
err2:
	destroy_workqueue(info->detect_wq);
err1:
	kfree(info);

	return ret;
}
late_initcall_sync(smuic_init);

MODULE_AUTHOR("SEC");
MODULE_DESCRIPTION("Identify TA or USB");
MODULE_LICENSE("GPL");
