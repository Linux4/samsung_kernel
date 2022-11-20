// SPDX-License-Identifier: GPL-2.0
/*
 *
 * Copyright (C) 2021 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 */

#define pr_fmt(fmt) "[MUIC] vt_muic: " fmt

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/notifier.h>
#include <linux/workqueue.h>
#include <linux/of_gpio.h>
#include <linux/interrupt.h>
#include <linux/power_supply.h>
#include <linux/delay.h>
#include <linux/muic/common/muic.h>
#include <linux/muic/common/muic_notifier.h>
#if IS_ENABLED(CONFIG_PDIC_NOTIFIER) && \
	IS_ENABLED(CONFIG_USB_TYPEC_MANAGER_NOTIFIER)
#include <linux/usb/typec/common/pdic_notifier.h>
#include <linux/usb/typec/manager/usb_typec_manager_notifier.h>
#endif /* CONFIG_PDIC_NOTIFIER && CONFIG_USB_TYPEC_MANAGER_NOTIFIER */
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
#include <dt-bindings/battery/sec-battery.h>
#include <../../drivers/battery/common/sec_charging_common.h>
#endif /* CONFIG_BATTERY_SAMSUNG */
#if defined(CONFIG_AFC_CHARGER)
#include <linux/muic/afc_gpio/gpio_afc_charger.h>
#endif

extern struct muic_platform_data muic_pdata;

struct vt_muic_pdata {
	int usb_id_gpio;
	int use_psy_nb;
	int use_manager_nb;
};

struct vt_muic_struct {
	int irq;
	int attached_dev;
	int batt_cable;
	int rprd;
	int afc_dev;
	int id_ta;
	int is_afc_started;
	int buck_prev_status;
	unsigned long			irq_debounce_interval;
	struct device			*dev;
	struct vt_muic_pdata		*pdata;
	struct muic_platform_data	*muic_pdata;
	struct power_supply		*ppsy;
	struct work_struct		vt_muic_work;
	struct notifier_block		psy_nb;
	struct notifier_block		manager_nb;
};

static struct vt_muic_struct vt_muic;

static void vt_muic_work_func(struct work_struct *work)
{
	int attached_dev = vt_muic.attached_dev;
	int new_dev = ATTACHED_DEV_NONE_MUIC;
	bool attach_update = false;

	pr_info("%s attached_dev(%d) batt_cable(%d) rprd(%d) afc_dev(%d)\n",
			__func__, attached_dev, vt_muic.batt_cable,
			vt_muic.rprd, vt_muic.afc_dev);
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
	/* 1. update battery cable */
	switch (vt_muic.batt_cable) {
	case SEC_BATTERY_CABLE_USB:
		new_dev = ATTACHED_DEV_USB_MUIC;
		break;
	case SEC_BATTERY_CABLE_USB_CDP:
		new_dev = ATTACHED_DEV_CDP_MUIC;
		break;
	case SEC_BATTERY_CABLE_TA:
		new_dev = ATTACHED_DEV_TA_MUIC;
		break;
	case SEC_BATTERY_CABLE_PREPARE_TA:
		new_dev = ATTACHED_DEV_TA_MUIC;
#if IS_ENABLED(CONFIG_AFC_CHARGER)
		if (!vt_muic.is_afc_started) {
#if IS_ENABLED(CONFIG_CHARGER_SYV660)
			afc_dpdm_ctrl(1);
#endif
			if (vt_muic.id_ta)
				muic_afc_set_voltage(0x9);

			vt_muic.is_afc_started = 1;
		}
#endif
		break;
	case SEC_BATTERY_CABLE_TIMEOUT:
		new_dev = ATTACHED_DEV_TIMEOUT_OPEN_MUIC;
		break;
	case SEC_BATTERY_CABLE_UNKNOWN:
	case SEC_BATTERY_CABLE_NONE:
#if IS_ENABLED(CONFIG_AFC_CHARGER) && IS_ENABLED(CONFIG_CHARGER_SYV660)
		afc_dpdm_ctrl(0);
		/* fallthrough */
#endif
	default:
		new_dev = ATTACHED_DEV_NONE_MUIC;
		vt_muic.afc_dev = ATTACHED_DEV_NONE_MUIC;
		vt_muic.is_afc_started = 0;
		break;
	}
#endif
	/* 2. update type-c rd cable */
	if (vt_muic.rprd)
		new_dev = ATTACHED_DEV_OTG_MUIC;

	/* 3. update afc cable */
	if (new_dev == ATTACHED_DEV_TA_MUIC &&
			vt_muic.afc_dev != ATTACHED_DEV_NONE_MUIC) {
		new_dev = vt_muic.afc_dev;
		attach_update = true;
	}

	if (attached_dev == new_dev) {
		pr_info("%s: Duplicated(%d), ignore\n", __func__, new_dev);
		return;
	}

	if (new_dev != ATTACHED_DEV_NONE_MUIC) {
		if (!attach_update && attached_dev != ATTACHED_DEV_NONE_MUIC)	
			muic_notifier_detach_attached_dev(attached_dev);

		muic_notifier_attach_attached_dev(new_dev);
	} else {
		if (attached_dev != ATTACHED_DEV_NONE_MUIC)
			muic_notifier_detach_attached_dev(attached_dev);
	}

	vt_muic.attached_dev = new_dev;
}

#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
static int vt_muic_psy_nb_func(struct notifier_block *nb, unsigned long val,
		void *v)
{
	struct power_supply *ppsy = (struct power_supply *)v;
	union power_supply_propval pval;
#if IS_ENABLED(CONFIG_AFC_CHARGER) && IS_ENABLED(CONFIG_CHARGER_SYV660)
	int buck_status;
#endif
	int ret;

	if (val != PSY_EVENT_PROP_CHANGED) {
		pr_debug("%s: not changed(%ld)\n", __func__, val);
		return NOTIFY_DONE;
	}

	if (!ppsy || !ppsy->desc || !ppsy->desc->name) {
		pr_err("%s: ppsy is NULL\n", __func__);
		return NOTIFY_DONE;
	}

	if (strcmp("bc12", ppsy->desc->name)) {
		pr_debug("%s: name is not bc12 (%s)\n", __func__,
				ppsy->desc->name);
		return NOTIFY_DONE;
	}

#if IS_ENABLED(CONFIG_AFC_CHARGER) && IS_ENABLED(CONFIG_CHARGER_SYV660)
	ret = power_supply_get_property(ppsy,
				POWER_SUPPLY_PROP_STATUS, &pval);
	if (ret != 0) {
		pr_err("failed to get psy prop, ret=%d\n", ret);
		return NOTIFY_DONE;
	}
	buck_status = pval.intval;
#endif

	ret = power_supply_get_property(ppsy,
				POWER_SUPPLY_PROP_ONLINE, &pval);
	if (ret != 0) {
		pr_err("failed to get psy prop, ret=%d\n", ret);
		return NOTIFY_DONE;
	}

#if IS_ENABLED(CONFIG_AFC_CHARGER) && IS_ENABLED(CONFIG_CHARGER_SYV660)
	pr_info("%s: buck_status prev=%d new=%d\n", __func__,
				vt_muic.buck_prev_status, buck_status);

	if (vt_muic.buck_prev_status == SEC_BAT_CHG_MODE_BUCK_OFF &&
			(buck_status == SEC_BAT_CHG_MODE_CHARGING ||
			buck_status == SEC_BAT_CHG_MODE_CHARGING_OFF)) {
		afc_dpdm_ctrl(0);
		vt_muic.is_afc_started = 0;
		vt_muic.buck_prev_status = buck_status;
		return NOTIFY_DONE;
	} 
	vt_muic.buck_prev_status = buck_status;
#endif

	pr_info("%s: batt_cable prev=%d new=%d\n", __func__,
				vt_muic.batt_cable, pval.intval);

	vt_muic.batt_cable = pval.intval;
	schedule_work(&vt_muic.vt_muic_work);

	return NOTIFY_DONE;
}
#endif /* CONFIG_BATTERY_SAMSUNG */

#if IS_ENABLED(CONFIG_PDIC_NOTIFIER) && \
	IS_ENABLED(CONFIG_USB_TYPEC_MANAGER_NOTIFIER)
static void vt_muic_handle_pd_attach(PD_NOTI_ATTACH_TYPEDEF *pnoti)
{
	if (pnoti->rprd) {
		vt_muic.rprd = pnoti->rprd;
		schedule_work(&vt_muic.vt_muic_work);
	}
}

static void vt_muic_handle_pd_detach(PD_NOTI_ATTACH_TYPEDEF *pnoti)
{
	if (vt_muic.rprd) {
		vt_muic.rprd = 0;
		schedule_work(&vt_muic.vt_muic_work);
	}

	vt_muic.id_ta = 0;
}

static int vt_muic_manager_nb_func(struct notifier_block *nb,
		unsigned long action, void *data)
{
	PD_NOTI_TYPEDEF *pnoti = (PD_NOTI_TYPEDEF *)data;

	if (pnoti->dest != PDIC_NOTIFY_DEV_MUIC) {
		pr_err("%s: dest is invalid(%d)\n", __func__, pnoti->dest);
		return NOTIFY_DONE;
	}
	pr_info("%s: src:%d dest:%d id:%d sub[%d %d %d]\n", __func__,
		pnoti->src, pnoti->dest, pnoti->id,
		pnoti->sub1, pnoti->sub2, pnoti->sub3);

	switch (pnoti->id) {
	case PDIC_NOTIFY_ID_ATTACH:
		if (pnoti->sub1)
			vt_muic_handle_pd_attach((PD_NOTI_ATTACH_TYPEDEF *)
					pnoti);
		else
			vt_muic_handle_pd_detach((PD_NOTI_ATTACH_TYPEDEF *)
					pnoti);
		break;
	case PDIC_NOTIFY_ID_WATER:
		break;
	case PDIC_NOTIFY_ID_TA:
		if (pnoti->sub1)
			vt_muic.id_ta = 1;
		break;
	default:
		break;
	}

	return NOTIFY_DONE;
}
#endif /* CONFIG_PDIC_NOTIFIER && CONFIG_USB_TYPEC_MANAGER_NOTIFIER */

void vt_muic_set_attached_afc_dev(int attached_afc_dev)
{
	pr_info("%s: (%d)->(%d)\n", __func__, vt_muic.afc_dev, attached_afc_dev);
	if (vt_muic.afc_dev == attached_afc_dev)
		return;
	vt_muic.afc_dev = attached_afc_dev;
	schedule_work(&vt_muic.vt_muic_work);
}
EXPORT_SYMBOL(vt_muic_set_attached_afc_dev);

int vt_muic_get_attached_dev(void)
{
	return vt_muic.attached_dev;
}
EXPORT_SYMBOL(vt_muic_get_attached_dev);

#if IS_ENABLED(CONFIG_OF)
static struct vt_muic_pdata *vt_muic_get_dt(struct device *dev)
{
	struct device_node *node;
	struct vt_muic_pdata *pdata;
	int ret = 0;

	node = dev->of_node;
	if (!node) {
		ret = -ENODEV;
		goto err_out;
	}

	pdata = devm_kzalloc(dev, sizeof(*pdata), GFP_KERNEL);
	if (!pdata) {
		ret = -ENOMEM;
		goto err_out;
	}

	pdata->usb_id_gpio = of_get_named_gpio(node, "vt_muic,usb_id", 0);
	if (gpio_is_valid(pdata->usb_id_gpio)) {
		ret = gpio_request(pdata->usb_id_gpio, "usb_id");
		if (ret) {
			pr_err("%s: usb_id gpio request fail(%d)\n",
				__func__, ret);
			goto err_out;
		}
	} else {
		pr_info("%s: gpio isn't used\n", __func__);
	}

	if (of_property_read_bool(node, "use_battery_notifier_callback"))
		pdata->use_psy_nb = 1;
	else
		pdata->use_psy_nb = 0;

	if (of_property_read_bool(node, "use_manager_notifier_callback"))
		pdata->use_manager_nb = 1;
	else
		pdata->use_manager_nb = 0;

	pr_info("%s: use_psy_nb(%d) use_manager_nb(%d)\n", __func__,
			pdata->use_psy_nb, pdata->use_manager_nb);

	return pdata;

err_out:
	return ERR_PTR(ret);
}
#endif /* CONFIG_OF */

static irqreturn_t vt_muic_irq_thread(int irq, void *data)
{
	pr_info("%s\n", __func__);

	if (time_after(jiffies, vt_muic.irq_debounce_interval)) {
		vt_muic.irq_debounce_interval = jiffies + msecs_to_jiffies(300);
		muic_send_lcd_on_uevent(vt_muic.muic_pdata);
	}

	return IRQ_HANDLED;
}

static int vt_muic_irq_init(void)
{
	struct vt_muic_pdata *pdata = vt_muic.pdata;
	int ret = 0;

	vt_muic.irq_debounce_interval = 0;

	if (!pdata)
		goto out;

	if (!gpio_is_valid(pdata->usb_id_gpio)) {
		pr_info("%s: do not need irq, return\n", __func__);
		goto out;
	}

	if (!IS_ENABLED(CONFIG_SEC_FACTORY))
		goto out;

	vt_muic.irq = gpio_to_irq(pdata->usb_id_gpio);
	ret = request_threaded_irq(vt_muic.irq, NULL, vt_muic_irq_thread,
			(IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING | IRQF_ONESHOT),
			"vt_muic", &vt_muic);
	if (ret) {
		pr_err("%s: failed request irq %d\n", __func__, ret);
		goto out;
	}

	ret = enable_irq_wake(vt_muic.irq);
	if (ret)
		pr_err("%s: failed enable irq %d\n", __func__, ret);

out:
	return ret;
}

static int vt_muic_probe(struct platform_device *pdev)
{
	struct vt_muic_pdata *pdata = pdev->dev.platform_data;
	int ret = 0;

	pr_info("%s\n", __func__);
	if (!pdata) {
#if IS_ENABLED(CONFIG_OF)
		pdata = vt_muic_get_dt(&pdev->dev);
		if (IS_ERR(pdata)) {
			pr_err("there is no device tree!\n");
			ret = -ENODEV;
			goto out;
		}
#else
		ret = -ENODEV;
		pr_err("there is no platform data!\n");
		goto out;
#endif /* CONFIG_OF */
	}
	vt_muic.dev = &pdev->dev;
	vt_muic.pdata = pdata;
	ret = vt_muic_irq_init();
	if (ret)
		goto out;

	vt_muic.muic_pdata = &muic_pdata;
	if (vt_muic.muic_pdata && vt_muic.muic_pdata->init_switch_dev_cb)
		vt_muic.muic_pdata->init_switch_dev_cb();

	if (pdata->use_psy_nb || pdata->use_manager_nb) {
		muic_notifier_detach_attached_dev(ATTACHED_DEV_UNKNOWN_MUIC);
		vt_muic.attached_dev = ATTACHED_DEV_NONE_MUIC;
		vt_muic.batt_cable = 0;
		vt_muic.rprd = 0;
		vt_muic.afc_dev = ATTACHED_DEV_NONE_MUIC;
		vt_muic.id_ta = 0;
		vt_muic.buck_prev_status = -1;
		vt_muic.is_afc_started = 0;
		INIT_WORK(&vt_muic.vt_muic_work, vt_muic_work_func);
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
		if (pdata->use_psy_nb) {
			vt_muic.psy_nb.notifier_call = vt_muic_psy_nb_func;
			vt_muic.psy_nb.priority = 0;
			ret = power_supply_reg_notifier(&vt_muic.psy_nb);
			if (ret < 0)
				pr_err("%s: register power supply notifier fail\n",
						__func__);
		}
#endif /* CONFIG_BATTERY_SAMSUNG */
#if IS_ENABLED(CONFIG_PDIC_NOTIFIER) && \
	IS_ENABLED(CONFIG_USB_TYPEC_MANAGER_NOTIFIER)
		if (pdata->use_manager_nb) {
			ret = manager_notifier_register(&vt_muic.manager_nb,
					vt_muic_manager_nb_func,
					MANAGER_NOTIFY_PDIC_MUIC);
			if (ret < 0)
				pr_err("%s: register manager notifier fail\n",
						__func__);
		}
#endif /* CONFIG_PDIC_NOTIFIER && CONFIG_USB_TYPEC_MANAGER_NOTIFIER */
	}

	pr_info("%s: done\n", __func__);

out:
	return ret;
}

static int vt_muic_remove(struct platform_device *pdev)
{
	struct vt_muic_pdata *pdata = vt_muic.pdata;

	if (vt_muic.muic_pdata && vt_muic.muic_pdata->cleanup_switch_dev_cb)
		vt_muic.muic_pdata->cleanup_switch_dev_cb();

	if (vt_muic.irq) {
		disable_irq_wake(vt_muic.irq);
		free_irq(vt_muic.irq, &vt_muic);
	}

	if (pdata) {
		if (gpio_is_valid(pdata->usb_id_gpio))
			gpio_free(pdata->usb_id_gpio);
#if IS_ENABLED(CONFIG_PDIC_NOTIFIER) && \
		IS_ENABLED(CONFIG_USB_TYPEC_MANAGER_NOTIFIER)
		if (pdata->use_manager_nb)
			manager_notifier_unregister(&vt_muic.manager_nb);
#endif /* CONFIG_PDIC_NOTIFIER && CONFIG_USB_TYPEC_MANAGER_NOTIFIER */
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
		if (pdata->use_psy_nb)
			power_supply_unreg_notifier(&vt_muic.psy_nb);
#endif /* CONFIG_BATTERY_SAMSUNG */
		devm_kfree(vt_muic.dev, pdata);
	}

	return 0;
}

#if IS_ENABLED(CONFIG_OF)
static const struct of_device_id vt_muic_dt_ids[] = {
	{ .compatible = "samsung,vt_muic" },
	{ }
};
MODULE_DEVICE_TABLE(of, vt_muic_dt_ids);
#endif /* CONFIG_OF */

static struct platform_driver vt_muic_driver = {
	.probe		= vt_muic_probe,
	.remove		= vt_muic_remove,
	.driver		= {
		.name		= "vt_muic",
		.owner		= THIS_MODULE,
#if IS_ENABLED(CONFIG_OF)
		.of_match_table	= of_match_ptr(vt_muic_dt_ids),
#endif /* CONFIG_OF */
	},
};

static int vt_muic_init(void)
{
	return platform_driver_register(&vt_muic_driver);
}

static void __exit vt_muic_exit(void)
{
	platform_driver_unregister(&vt_muic_driver);
}

device_initcall(vt_muic_init);
module_exit(vt_muic_exit);

MODULE_AUTHOR("Samsung USB Team");
MODULE_DESCRIPTION("Virtual Muic");
MODULE_LICENSE("GPL");
