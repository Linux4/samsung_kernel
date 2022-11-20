/*
 * t7_charger.c - T7 USB/Adapter Charger Driver
 *
 * Copyright (c) 2013 Marvell Technology Ltd.
 * Yi Zhang<yizhang@marvell.com>
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */
#include <linux/gpio.h>
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/regmap.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/power_supply.h>
#include <linux/platform_device.h>
#include <linux/regulator/machine.h>
#include <linux/platform_data/mv_usb.h>
#include <linux/jiffies.h>
#include <linux/workqueue.h>
#include <linux/notifier.h>
#include <linux/err.h>
#include <linux/delay.h>
#include <linux/power/t7_charger.h>

#define UPDATE_INTERVAL	(30*HZ)

struct t7_info {
	struct t7_pdata *pdata;
	struct device *dev;
	struct power_supply ac;
	struct power_supply usb;
	struct notifier_block chg_notif;
	struct delayed_work chg_update_work;
	int interval;
	int ac_chg_online;
	int usb_chg_online;

	int chg_int;	/* status pin0 */
	int chg_det;	/* status pin1 */
	int chg_en;	/* ENABLE pin */

	int is_charging;
	int is_eoc;
};

static enum power_supply_property t7_ac_props[] = {
	POWER_SUPPLY_PROP_STATUS, /* Charger status output */
	POWER_SUPPLY_PROP_ONLINE, /* External power source */
};

static enum power_supply_property t7_usb_props[] = {
	POWER_SUPPLY_PROP_STATUS, /* Charger status output */
	POWER_SUPPLY_PROP_ONLINE, /* External power source */
};

static char *t7_supplied_to[] = {
	"battery",
};

static int t7_ac_get_property(struct power_supply *psy,
				   enum power_supply_property psp,
				   union power_supply_propval *val)
{
	struct t7_info *info =
	    container_of(psy, struct t7_info, ac);

	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = info->ac_chg_online;
		break;
	case POWER_SUPPLY_PROP_STATUS:
		if (info->is_charging)
			val->intval = POWER_SUPPLY_STATUS_CHARGING;
		else if (info->is_eoc)
			val->intval = POWER_SUPPLY_STATUS_FULL;
		else
			val->intval = POWER_SUPPLY_STATUS_DISCHARGING;
		break;

	default:
		return -EINVAL;
	}

	return 0;
}

static int t7_usb_get_property(struct power_supply *psy,
				   enum power_supply_property psp,
				   union power_supply_propval *val)
{
	struct t7_info *info =
	    container_of(psy, struct t7_info, usb);

	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = info->usb_chg_online;
		break;
	case POWER_SUPPLY_PROP_STATUS:
		if (info->is_charging)
			val->intval = POWER_SUPPLY_STATUS_CHARGING;
		else if (info->is_eoc)
			val->intval = POWER_SUPPLY_STATUS_FULL;
		else
			val->intval = POWER_SUPPLY_STATUS_DISCHARGING;
		break;

	default:
		return -EINVAL;
	}

	return 0;
}

static int t7_powersupply_init(struct t7_info *info,
				struct t7_pdata *pdata)
{
	int ret = 0;

	if (pdata->supplied_to) {
		info->ac.supplied_to = pdata->supplied_to;
		info->ac.num_supplicants = pdata->num_supplicants;
		info->usb.supplied_to = pdata->supplied_to;
		info->usb.num_supplicants = pdata->num_supplicants;
	} else {
		info->ac.supplied_to = t7_supplied_to;
		info->ac.num_supplicants =
			ARRAY_SIZE(t7_supplied_to);
		info->usb.supplied_to = t7_supplied_to;
		info->usb.num_supplicants =
			ARRAY_SIZE(t7_supplied_to);
	}
	/* register charger props */
	info->ac.name = "ac";
	info->ac.type = POWER_SUPPLY_TYPE_MAINS;
	info->ac.properties = t7_ac_props;
	info->ac.num_properties = ARRAY_SIZE(t7_ac_props);
	info->ac.get_property = t7_ac_get_property;
	ret = power_supply_register(info->dev, &info->ac);
	if (ret) {
		dev_err(info->dev, "AC power supply register failed!\n");
		goto out;
	}

	info->usb.name = "usb";
	info->usb.type = POWER_SUPPLY_TYPE_USB;
	info->usb.get_property = t7_usb_get_property;
	info->usb.properties = t7_usb_props;
	info->usb.num_properties = ARRAY_SIZE(t7_usb_props);
	ret = power_supply_register(info->dev, &info->usb);
	if (ret) {
		dev_err(info->dev, "USB power supply register failed!\n");
		goto out;
	}
out:
	return ret;

}

/* setup the control gpio */
static int t7_setup(struct t7_info *info)
{
	if (gpio_is_valid(info->chg_en)) {
		if (gpio_request(info->chg_en, "t7_chg_en")) {
			dev_err(info->dev, "%s: request gpio fail!\n", __func__);
			return -EINVAL;
		}
		/* enable charge by default */
		gpio_direction_output(info->chg_en, 0);
		gpio_free(info->chg_en);
	}

	if (gpio_is_valid(info->chg_det)) {
		if (gpio_request(info->chg_det, "t7_chg_det")) {
			return -EINVAL;
		}
		gpio_direction_output(info->chg_det, 0);
		gpio_free(info->chg_det);
	}

	return 0;
}

static int t7_start_charging(struct t7_info *info)
{
	dev_info(info->dev, "start charging...!\n");
	if (gpio_request(info->chg_en, "t7_chg_en")) {
		dev_err(info->dev, "%s: request gpio fail!\n", __func__);
		return -EINVAL;
	}
	gpio_direction_output(info->chg_en, 0);
	gpio_free(info->chg_en);
	return 0;
}

static int t7_stop_charging(struct t7_info *info)
{
	dev_info(info->dev, "stop charging...!\n");
	if (gpio_request(info->chg_en, "t7_chg_en")) {
		dev_err(info->dev, "%s: request gpio fail!\n", __func__);
		return -EINVAL;
	}
	gpio_direction_output(info->chg_en, 1);
	gpio_free(info->chg_en);
	return 0;
}

static int t7_chg_notifier_callback(struct notifier_block *nb,
					 unsigned long type, void *chg_event)
{
	struct t7_info *info =
	    container_of(nb, struct t7_info, chg_notif);
	switch (type) {
	case NULL_CHARGER:
		info->ac_chg_online = 0;
		info->usb_chg_online = 0;
		break;
	case VBUS_CHARGER:
		info->ac_chg_online = 0;
		info->usb_chg_online = 1;
		break;
	case DEFAULT_CHARGER:
		info->ac_chg_online = 0;
		info->usb_chg_online = 1;
		break;
	case AC_CHARGER_STANDARD:
	case AC_CHARGER_OTHER:
		info->ac_chg_online = 1;
		info->usb_chg_online = 0;
		break;
	default:
		break;
	}
	if (info->usb_chg_online || info->ac_chg_online) {
		 if (t7_start_charging(info))
			 return -EINVAL;
		schedule_delayed_work(&info->chg_update_work, HZ/2);
	} else {
		 if (t7_stop_charging(info))
			 return -EINVAL;
		schedule_delayed_work(&info->chg_update_work, HZ/2);
	}
	power_supply_changed(&info->ac);
	power_supply_changed(&info->usb);
	return 0;
}

static void chg_work_func(struct work_struct *work)
{
	int chg_status, eoc_status;
	struct t7_info *info =
		container_of(work, struct t7_info,
			     chg_update_work.work);

	if (gpio_request(info->chg_det, "t7_chg_det"))
		return;
	if (gpio_request(info->chg_int, "t7_chg_int")) {
		gpio_free(info->chg_det);
		return;
	}
	gpio_direction_input(info->chg_int);

	/* get charging or not status */
	gpio_direction_output(info->chg_det, 0);
	udelay(200);
	chg_status = gpio_get_value(info->chg_int);
	if (!chg_status)
		info->is_charging = 1;
	else {
		info->is_charging = 0;

		/* get eoc status */
		gpio_direction_output(info->chg_det, 1);
		udelay(200);
		eoc_status = gpio_get_value(info->chg_int);
		if (!eoc_status)
			info->is_eoc = 1;
		else
			info->is_eoc = 0;

		gpio_direction_output(info->chg_det, 0);

	}
	gpio_free(info->chg_det);
	gpio_free(info->chg_int);

	schedule_delayed_work(&info->chg_update_work,
			      info->interval);
}

static __devinit int t7_charger_probe(struct platform_device *pdev)
{
	struct t7_info *info;
	struct t7_pdata *pdata = pdev->dev.platform_data;
	int ret = 0;

	info = devm_kzalloc(&pdev->dev, sizeof(struct t7_info), GFP_KERNEL);
	if (info == NULL) {
		dev_err(&pdev->dev, "Cannot allocate memory.\n");
		return -ENOMEM;
	}

	info->pdata = pdata;
	info->dev = &pdev->dev;
	info->interval = UPDATE_INTERVAL;

	info->chg_int = pdata->chg_int;
	info->chg_det = pdata->chg_det;
	info->chg_en  = pdata->chg_en;

	/* set up t7 registers and control pin */
	ret = t7_setup(info);
	if (ret) {
		dev_err(info->dev, "%s fail!\n", __func__);
		goto out;
	}

	/* register powersupply */
	ret = t7_powersupply_init(info, pdata);
	if (ret) {
		dev_err(info->dev, "register powersupply fail!\n");
		goto out;
	}

	/* register charger event notifier */
	info->chg_notif.notifier_call = t7_chg_notifier_callback;
	/* init delayed work queue to update the status */
	INIT_DELAYED_WORK(&info->chg_update_work, chg_work_func);

#ifdef CONFIG_USB_MV_UDC
	ret = mv_udc_register_client(&info->chg_notif);
	if (ret < 0) {
		dev_err(info->dev, "failed to register client!\n");
		goto err_psy;
	}
#endif

	schedule_delayed_work(&info->chg_update_work, info->interval);
	dev_info(info->dev, "%s is successful!\n", __func__);

	return 0;

err_psy:
	power_supply_unregister(&info->ac);
	power_supply_unregister(&info->usb);
out:
	return ret;
}

static int __devexit t7_charger_remove(struct platform_device *pdev)
{
	struct t7_info *info = platform_get_drvdata(pdev);

	if (info) {
		t7_stop_charging(info);
#ifdef CONFIG_USB_MV_UDC
		mv_udc_unregister_client(&info->chg_notif);
#endif
		cancel_delayed_work_sync(&info->chg_update_work);
		power_supply_unregister(&info->ac);
		power_supply_unregister(&info->usb);
	}

	return 0;
}

static struct platform_driver t7_charger_driver = {
	.probe = t7_charger_probe,
	.remove = __devexit_p(t7_charger_remove),
	.driver = {
		.name = "t7-charger",
		.owner = THIS_MODULE,
	},
};

module_platform_driver(t7_charger_driver);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("T7 Charger Driver");
MODULE_AUTHOR("Yi Zhang<yizhang@marvell.com>");
MODULE_ALIAS("tablet: t7-charger");
