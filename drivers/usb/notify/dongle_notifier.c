
/*
 * Copyright (C) 2020 Samsung Electronics Co. Ltd.
 *  Jaejin Lee <jjinn.lee@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#define pr_fmt(fmt) "dongle_notifier: " fmt

#include <linux/module.h>
#ifdef CONFIG_OF
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#endif
#include <linux/usb_notify.h>
#include <linux/workqueue.h>

#include <linux/regulator/consumer.h>
#include <linux/interrupt.h>
#if defined(CONFIG_IFCONN_NOTIFIER)
#include <linux/ifconn/ifconn_notifier.h>
#endif
#if defined(CONFIG_PDIC_NOTIFIER)
#include <linux/usb/typec/common/pdic_notifier.h>
#endif
#if defined(CONFIG_MUIC_NOTIFIER) || defined(CONFIG_IFCONN_NOTIFIER)
#include <linux/muic/common/muic.h>
#endif
#if defined(CONFIG_MUIC_NOTIFIER)
#include <linux/muic/common/muic_notifier.h>
#endif
#if defined(CONFIG_USB_TYPEC_MANAGER_NOTIFIER)
#include <linux/usb/typec/manager/usb_typec_manager_notifier.h>
#endif
#include "usb_notifier.h"


#define TEST_ETHERNET (0)
struct dongle_notifier_platform_data {
#if defined(CONFIG_PDIC_NOTIFIER)
	struct	notifier_block pdic_usb_nb;
	int is_host;
	int is_client;
#endif
#if defined(CONFIG_MUIC_NOTIFIER)
	struct	notifier_block muic_usb_nb;
#endif
	int lanw_gpio;
	int lanw_gpio_irq;
	int sel_gpio;
	int sel_stat;
	int ethen_gpio;
	struct delayed_work eth_work;
};

enum u_switch_state {
	U_TYPEC = 0,
	U_ETH,
};

#ifdef CONFIG_OF
static void of_get_dongle_gpio_dt(struct device_node *np,
		struct dongle_notifier_platform_data *pdata)
{
	int gpio = 0;

	gpio = of_get_named_gpio(np, "gpios,lanw-gpio", 0);
	if (!gpio_is_valid(gpio)) {
		pdata->lanw_gpio = -1;
		pr_err("%s: lanw_gpio: Invalied gpio pins\n", __func__);
	} else
		pdata->lanw_gpio = gpio;

	gpio = of_get_named_gpio(np, "gpios,usbsel-gpio", 0);
	if (!gpio_is_valid(gpio)) {
		pdata->sel_gpio = -1;
		pr_err("%s: sel_gpio: Invalied gpio pins\n", __func__);
	} else
		pdata->sel_gpio = gpio;
	
	gpio = of_get_named_gpio(np, "gpios,ethen-gpio", 0);
	if (!gpio_is_valid(gpio)) {
		pdata->ethen_gpio = -1;
		pr_err("%s: ethen_gpio: Invalied gpio pins\n", __func__);
	} else
		pdata->ethen_gpio = gpio;

	pr_info("lan_en:%d sel:%d eth_en:%d\n",
			pdata->lanw_gpio, pdata->sel_gpio, pdata->ethen_gpio);

}

static int of_dongle_notifier_dt(struct device *dev,
		struct dongle_notifier_platform_data *pdata)
{
	struct device_node *np = dev->of_node;

	if (!np)
		return -EINVAL;

	of_get_dongle_gpio_dt(np, pdata);
	return 0;
}
#endif

static struct device_node *exynos_udc_parse_dt(void)
{
	struct platform_device *pdev = NULL;
	struct device *dev = NULL;
	struct device_node *np = NULL;

	/**
	 * For previous chips such as Exynos7420 and Exynos7890
	 */

	np = of_find_compatible_node(NULL, NULL, "samsung,exynos5-dwusb3");
	if (np)
		goto find;

	np = of_find_compatible_node(NULL, NULL, "samsung,usb-notifier");
	if (!np) {
		pr_err("%s: failed to get the usb-notifier device node\n",
			__func__);
		goto err;
	}

	pdev = of_find_device_by_node(np);
	if (!pdev) {
		pr_err("%s: failed to get platform_device\n", __func__);
		goto err;
	}

	dev = &pdev->dev;
	np = of_parse_phandle(dev->of_node, "udc", 0);
	if (!np) {
		dev_info(dev, "udc device is not available\n");
		goto err;
	}
find:
	return np;
err:
	return NULL;
}

#if !TEST_ETHERNET
static irqreturn_t lanw_irq_thread(int irq, void *data)
{
	struct dongle_notifier_platform_data *pdata = data;
	struct otg_notify *o_notify = get_otg_notify();
	int on;
	int sel,en;

	on = gpio_get_value(pdata->lanw_gpio);
	sel = gpio_get_value(pdata->sel_gpio);

	pr_info("%s:Ethernet: %sconnect, Typec:%s, Switch:%s\n",
		__func__, (on) ? "" : "dis",
		(pdata->is_host) ? "host" : ((pdata->is_client) ? "client" : "none"),
		(pdata->sel_stat) ? "ETH" : "TYPEC");
	en = gpio_get_value(pdata->ethen_gpio);
	pr_info("%s en %d\n", __func__, en);

	if (on == pdata->sel_stat) {
		pr_info("%s:Ethernet cable is not changed\n", __func__);
		return IRQ_NONE;
	}

	if (on) {
		/* 1. switch chips set to RJ45*/
		/* LOW: Type C, HIGH: RJ45 */
		if (sel != U_ETH) {
			pdata->sel_stat = U_ETH;
			gpio_set_value(pdata->sel_gpio, pdata->sel_stat);
		}
		/* 2. usb host driver on */
		if (pdata->is_client) {
			pr_info("%s: Turn Off Device\n", __func__);
			send_otg_notify(o_notify, NOTIFY_EVENT_VBUS, 0);
		}
		pr_info("%s: Turn on Host\n", __func__);
		send_otg_notify(o_notify, NOTIFY_EVENT_HOST, 1);
	} else {
		/* 1. switch chips set to typec */
		if (sel != U_TYPEC) {
			pdata->sel_stat = U_TYPEC;
			gpio_set_value(pdata->sel_gpio, pdata->sel_stat);
		}
		/* 2. change usb driver state */
		if (pdata->is_host)
			send_otg_notify(o_notify, NOTIFY_EVENT_HOST, 1);
		else
			send_otg_notify(o_notify, NOTIFY_EVENT_HOST, 0);

		if (pdata->is_client)
			send_otg_notify(o_notify, NOTIFY_EVENT_VBUS, 1);
		else
			send_otg_notify(o_notify, NOTIFY_EVENT_VBUS, 0);

	}

	return IRQ_HANDLED;
}
#endif

#if defined(CONFIG_PDIC_NOTIFIER)
static int pdic_usb_handle_notification(struct notifier_block *nb,
		unsigned long action, void *data)
{
	PD_NOTI_USB_STATUS_TYPEDEF usb_status = *(PD_NOTI_USB_STATUS_TYPEDEF *)data;
	struct otg_notify *o_notify = get_otg_notify();
	struct dongle_notifier_platform_data *pdata =
		container_of(nb, struct dongle_notifier_platform_data, pdic_usb_nb);

	if (usb_status.dest != PDIC_NOTIFY_DEV_USB)
		return 0;
#if TEST_ETHERNET
	pdata->sel_stat = U_TYPEC;
#endif

	switch (usb_status.drp) {
	case USB_STATUS_NOTIFY_ATTACH_DFP:
		pr_info("%s:Turn On Host(sel:%d)\n",
				__func__, pdata->sel_stat);
		if (pdata->sel_stat != U_ETH) {
			gpio_set_value(pdata->sel_gpio, pdata->sel_stat);
			send_otg_notify(o_notify, NOTIFY_EVENT_HOST, 1);
		}
		pdata->is_host = 1;
		break;
	case USB_STATUS_NOTIFY_ATTACH_UFP:
		pr_info("%s:Turn On Device(sel:%d)\n", __func__, pdata->sel_stat);
		if (pdata->sel_stat != U_ETH) {
			gpio_set_value(pdata->sel_gpio, pdata->sel_stat);
			send_otg_notify(o_notify, NOTIFY_EVENT_VBUS, 1);
		}
		pdata->is_client = 1;
		break;
	case USB_STATUS_NOTIFY_DETACH:
		if (pdata->is_host) {
			pr_info("%s:Turn Off Host(sel:%d)\n", __func__, pdata->sel_stat);
			if (pdata->sel_stat != U_ETH) {
				gpio_set_value(pdata->sel_gpio, pdata->sel_stat);
				send_otg_notify(o_notify, NOTIFY_EVENT_HOST, 0);
			}
			pdata->is_host = 0;
		}
		if (pdata->is_client) {
			pr_info("%s:Turn Off Device(sel:%d)\n", __func__, pdata->sel_stat);
			if (pdata->sel_stat != U_ETH) {
				gpio_set_value(pdata->sel_gpio, pdata->sel_stat);
				send_otg_notify(o_notify, NOTIFY_EVENT_VBUS, 0);
			}
			pdata->is_client = 0;
		}
#if TEST_ETHERNET
		pdata->sel_stat = U_ETH;
		gpio_set_value(pdata->sel_gpio, pdata->sel_stat);
		pr_info("%s:Turn On Host(sel:%d)\n", __func__, pdata->sel_stat);
		send_otg_notify(o_notify, NOTIFY_EVENT_HOST, 1);
#endif
		break;
	default:
		pr_info("%s:unsupported DRP type:%d.\n", __func__, usb_status.drp);
		break;
	}
	return 0;
}
#endif
#if defined(CONFIG_MUIC_NOTIFIER)
static int muic_usb_handle_notification(struct notifier_block *nb,
		unsigned long action, void *data)
{
#ifdef CONFIG_PDIC_NOTIFIER
	PD_NOTI_ATTACH_TYPEDEF *p_noti = (PD_NOTI_ATTACH_TYPEDEF *)data;
	muic_attached_dev_t attached_dev = p_noti->cable_type;
#else
	muic_attached_dev_t attached_dev = *(muic_attached_dev_t *)data;
#endif
	struct otg_notify *o_notify = get_otg_notify();
	struct dongle_notifier_platform_data *pdata =
		container_of(nb, struct dongle_notifier_platform_data, muic_usb_nb);
#if TEST_ETHERNET
	pdata->sel_stat = U_TYPEC;
#endif

	pr_info("%s action=%lu, attached_dev=%d switch %d\n",
		__func__, action, attached_dev, pdata->sel_stat);

	switch (attached_dev) {
	case ATTACHED_DEV_USB_MUIC:
	case ATTACHED_DEV_CDP_MUIC:
	case ATTACHED_DEV_UNOFFICIAL_ID_USB_MUIC:
	case ATTACHED_DEV_UNOFFICIAL_ID_CDP_MUIC:
	case ATTACHED_DEV_JIG_USB_OFF_MUIC:
	case ATTACHED_DEV_JIG_USB_ON_MUIC:
		if (action == MUIC_NOTIFY_CMD_DETACH) {
			pr_info("%s:Turn Off Device(sel:%d)\n", __func__, pdata->sel_stat);
			if (pdata->sel_stat != U_ETH) {
				gpio_set_value(pdata->sel_gpio, pdata->sel_stat);
				send_otg_notify(o_notify, NOTIFY_EVENT_VBUS, 0);
			}
			pdata->is_client = 0;
#if TEST_ETHERNET
			pdata->sel_stat = U_ETH;
			gpio_set_value(pdata->sel_gpio, pdata->sel_stat);
			pr_info("%s:Turn On Host(sel:%d)\n", __func__, pdata->sel_stat);
			send_otg_notify(o_notify, NOTIFY_EVENT_HOST, 1);
#endif
		} else if (action == MUIC_NOTIFY_CMD_ATTACH) {
			pr_info("%s:Turn On Device(sel:%d)\n", __func__, pdata->sel_stat);
			if (pdata->sel_stat != U_ETH) {
				gpio_set_value(pdata->sel_gpio, pdata->sel_stat);
				send_otg_notify(o_notify, NOTIFY_EVENT_VBUS, 1);
			}
			pdata->is_client = 1;
		} else
			pr_err("%s - ACTION Error!\n", __func__);
		break;
	case ATTACHED_DEV_OTG_MUIC:
		if (action == MUIC_NOTIFY_CMD_DETACH) {
			pr_info("%s:Turn Off Device(sel:%d)\n", __func__, pdata->sel_stat);
			if (pdata->sel_stat != U_ETH) {
				gpio_set_value(pdata->sel_gpio, pdata->sel_stat);
				send_otg_notify(o_notify, NOTIFY_EVENT_VBUS, 0);
			}
			pdata->is_host = 0;
#if TEST_ETHERNET
			pdata->sel_stat = U_ETH;
			gpio_set_value(pdata->sel_gpio, pdata->sel_stat);
			pr_info("%s:Turn On Host(sel:%d)\n", __func__, pdata->sel_stat);
			send_otg_notify(o_notify, NOTIFY_EVENT_HOST, 1);
#endif
		} else if (action == MUIC_NOTIFY_CMD_ATTACH) {
			pr_info("%s:Turn On Host(sel:%d)\n", __func__, pdata->sel_stat);
			if (pdata->sel_stat != U_ETH) {
				gpio_set_value(pdata->sel_gpio, pdata->sel_stat);
				send_otg_notify(o_notify, NOTIFY_EVENT_HOST, 1);
			}
			pdata->is_host = 1;
		} else
			pr_err("%s - ACTION Error!\n", __func__);
		break;
	default:
		break;
	}

	return 0;
}
#endif

static void check_usb_vbus_state(int state)
{
	struct device_node *np = NULL;
	struct platform_device *pdev = NULL;

	pr_info("%s vbus state = %d\n", __func__, state);

	np = exynos_udc_parse_dt();
	if (np) {
		pdev = of_find_device_by_node(np);
		of_node_put(np);
		if (pdev) {
			pr_info("%s: get the %s platform_device\n",
				__func__, pdev->name);
			dwc3_exynos_vbus_event(&pdev->dev, state);
			goto end;
		}
	}

	pr_err("%s: failed to get the platform_device\n", __func__);
end:
	return;
}

static void check_usb_id_state(int state)
{
	struct device_node *np = NULL;
	struct platform_device *pdev = NULL;

	pr_info("%s id state = %d\n", __func__, state);

	np = exynos_udc_parse_dt();
	if (np) {
		pdev = of_find_device_by_node(np);
		of_node_put(np);
		if (pdev) {
			pr_info("%s: get the %s platform_device\n",
			__func__, pdev->name);
			dwc3_exynos_id_event(&pdev->dev, state);
			goto end;
		}
	}
	pr_err("%s: failed to get the platform_device\n", __func__);
end:
	return;
}

static int exynos_set_host(bool enable)
{
	if (!enable) {
		pr_info("%s USB_HOST_DETACHED\n", __func__);
#ifdef CONFIG_OF
		check_usb_id_state(1);
#endif
	} else {
		pr_info("%s USB_HOST_ATTACHED\n", __func__);
#ifdef CONFIG_OF
		check_usb_id_state(0);
#endif
	}

	return 0;
}

static int exynos_set_peripheral(bool enable)
{
	if (enable) {
		pr_info("%s usb attached\n", __func__);
		check_usb_vbus_state(1);
	} else {
		pr_info("%s usb detached\n", __func__);
		check_usb_vbus_state(0);
	}
	return 0;
}

static int usb_switch_init(struct dongle_notifier_platform_data *pdata)
{
	int temp, en;
	int ret;
#if TEST_ETHERNET
	struct otg_notify *o_notify = get_otg_notify();
#endif

	if (!pdata->sel_gpio) {
		pr_err("%s No switch sel gpio\n", __func__);
		return -EINVAL;
	}

	pr_info("%s: request sel gpio %d\n", __func__, pdata->sel_gpio);
	ret = gpio_request(pdata->sel_gpio, "sel_gpio");
	if (ret) {
		pr_err("%s: failed request gpio %d\n", __func__, pdata->sel_gpio);
		return ret;
	}

	pr_info("%s: request ethen  gpio %d\n", __func__, pdata->sel_gpio);
	ret = gpio_request(pdata->ethen_gpio, "ethen_gpio");
	if (ret) {
		pr_err("%s: failed request gpio %d\n", __func__, pdata->sel_gpio);
		return ret;
	}

#if TEST_ETHERNET
	pdata->sel_stat = U_ETH;
	gpio_direction_output(pdata->sel_gpio, pdata->sel_stat);
	/* test for rtl8156b */
	send_otg_notify(o_notify, NOTIFY_EVENT_HOST, 1);
#else
	pdata->sel_stat = U_TYPEC;
	gpio_direction_output(pdata->sel_gpio, pdata->sel_stat);
	gpio_direction_output(pdata->ethen_gpio, 0);
#endif
	temp = gpio_get_value(pdata->sel_gpio);
	en = gpio_get_value(pdata->ethen_gpio);
	pr_info("%s sel_stat:%d switch_sel:%d en:%d\n", __func__, pdata->sel_stat, temp, en);
	return ret;
}

#if !TEST_ETHERNET
static int dongle_notifier_irq_init(struct dongle_notifier_platform_data *pdata)
{
	int ret;
	int val;
	int sel;
	struct otg_notify *o_notify = get_otg_notify();

	if (!pdata->lanw_gpio) {
		pr_err("%s No lan wake gpio\n", __func__);
		return -EINVAL;
	}

	pdata->lanw_gpio_irq = gpio_to_irq(pdata->lanw_gpio);
	pr_info("%s lan wake irq%d, irq_gpio%d\n", __func__, pdata->lanw_gpio_irq,
			pdata->lanw_gpio);

	pr_info("%s: request lanw gpio %d\n", __func__, pdata->lanw_gpio);
	ret = gpio_request(pdata->lanw_gpio, "lanw_irq");
	if (ret) {
		pr_err("%s: failed request gpio %d\n", __func__, pdata->lanw_gpio);
		return ret;
	}
	gpio_direction_input(pdata->lanw_gpio);

	ret = request_threaded_irq(pdata->lanw_gpio_irq, NULL, lanw_irq_thread,
		IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING | IRQF_ONESHOT, "lan-wake-irq", pdata);
	if (ret) {
		pr_err("%s Failed to request IRQ %d: %d\n", __func__,
				pdata->lanw_gpio_irq, ret);
		return ret;

	}
	val = gpio_get_value(pdata->lanw_gpio);
	sel = gpio_get_value(pdata->sel_gpio);
	if (val) {
		pr_info("%s: ehternet is connected\n", __func__);
		if (sel != U_ETH) {
			pdata->sel_stat = U_ETH;
			gpio_set_value(pdata->sel_gpio, pdata->sel_stat);
		}
		if (pdata->is_client) {
			pr_info("%s: Turn Off Device switch %d\n", __func__, pdata->sel_stat);
			if (pdata->sel_stat != U_ETH)
				send_otg_notify(o_notify, NOTIFY_EVENT_VBUS, 0);
		}
		send_otg_notify(o_notify, NOTIFY_EVENT_HOST, 1);
	}
	return ret;
}
#endif

static int otg_accessory_power(bool enable)
{
	u8 on = (u8)!!enable;

	pr_info("otg accessory power = %d\n", on);

	return 0;
}

static void ethernet_power(struct work_struct *work) {
	struct dongle_notifier_platform_data *pdata = container_of(work,
			struct dongle_notifier_platform_data,eth_work.work);
	
	pr_info("%s\n", __func__);
	gpio_direction_output(pdata->ethen_gpio, 1);
}

static struct otg_notify dwc_lsi_notify = {
	.vbus_drive = otg_accessory_power,
	.set_host = exynos_set_host,
	.set_peripheral	= exynos_set_peripheral,
	.is_host_wakelock = 1,
	.is_wakelock = 1,
#if defined(CONFIG_IFCONN_NOTIFIER)
	.charger_detect = 0,
#endif
	.booting_delay_sec = 10,
	.vbus_detect_gpio = -1,
};

static int dongle_notifier_probe(struct platform_device *pdev)
{
	struct dongle_notifier_platform_data *pdata = NULL;
	int ret = 0;
#if TEST_ETHERNET
	struct otg_notify *o_notify;
#endif

	if (pdev->dev.of_node) {
		pdata = devm_kzalloc(&pdev->dev,
				sizeof(struct dongle_notifier_platform_data), GFP_KERNEL);
		if (!pdata) {
			dev_err(&pdev->dev, "Failed to allocate memory\n");
			return -ENOMEM;
		}

		ret = of_dongle_notifier_dt(&pdev->dev, pdata);
		if (ret < 0) {
			dev_err(&pdev->dev, "Failed to get device of_node\n");
			return ret;
		}

		pdev->dev.platform_data = pdata;
	} else
		pdata = pdev->dev.platform_data;
	dwc_lsi_notify.redriver_en_gpio = -1;
	dwc_lsi_notify.vbus_detect_gpio = -1;
	set_otg_notify(&dwc_lsi_notify);
	set_notify_data(&dwc_lsi_notify, pdata);

	ret = usb_switch_init(pdata);
	if (ret < 0) {
		dev_err(&pdev->dev, "Failed to init usb switch\n");
		return ret;
	}
	pdata->is_host = 0;
	pdata->is_client = 0;
#ifdef CONFIG_USB_TYPEC_MANAGER_NOTIFIER
	manager_notifier_register(&pdata->pdic_usb_nb, pdic_usb_handle_notification,
			MANAGER_NOTIFY_PDIC_USB);
#else
	pdic_notifier_register(&pdata->pdic_usb_nb, pdic_usb_handle_notification,
			PDIC_NOTIFY_DEV_USB);
#endif
#if defined(CONFIG_MUIC_NOTIFIER)
	muic_notifier_register(&pdata->muic_usb_nb, muic_usb_handle_notification,
			MUIC_NOTIFY_DEV_USB);
#endif
#if !TEST_ETHERNET
	ret = dongle_notifier_irq_init(pdata);
	if (ret < 0) {
		dev_err(&pdev->dev, "Failed to init lanw_gpio_irq\n");
		return ret;
	}
#endif
#if TEST_ETHERNET
	o_notify = get_otg_notify();
	pdata->sel_stat = U_ETH;
	gpio_direction_output(pdata->sel_gpio, pdata->sel_stat);
	/* test for rtl8156b */
	send_otg_notify(o_notify, NOTIFY_EVENT_HOST, 1);
#endif
	INIT_DELAYED_WORK(&pdata->eth_work, ethernet_power);
	schedule_delayed_work(&pdata->eth_work, 20*HZ);
	dev_info(&pdev->dev, "dongle notifier probe\n");
	return 0;
}

static int dongle_notifier_remove(struct platform_device *pdev)
{
	struct dongle_notifier_platform_data *pdata = dev_get_platdata(&pdev->dev);

	cancel_delayed_work_sync(&pdata->eth_work);
	gpio_free(pdata->lanw_gpio);
	gpio_free(pdata->sel_gpio);

	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id dongle_notifier_dt_ids[] = {
	{ .compatible = "samsung,dongle-notifier",
	},
	{ },
};
MODULE_DEVICE_TABLE(of, dongle_notifier_dt_ids);
#endif

static struct platform_driver dongle_notifier_driver = {
	.probe		= dongle_notifier_probe,
	.remove		= dongle_notifier_remove,
	.driver		= {
		.name	= "dongle_notifier",
		.owner	= THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table	= of_match_ptr(dongle_notifier_dt_ids),
#endif
	},
};

static int __init dongle_notifier_init(void)
{
	return platform_driver_register(&dongle_notifier_driver);
}

static void __init dongle_notifier_exit(void)
{
	platform_driver_unregister(&dongle_notifier_driver);
}

late_initcall(dongle_notifier_init);
module_exit(dongle_notifier_exit);

MODULE_AUTHOR("jjinn.lee <jjinn.lee@samsung.com>");
MODULE_DESCRIPTION("dongle notifier");
MODULE_LICENSE("GPL");
