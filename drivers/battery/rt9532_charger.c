/*
 *  rt9532_charger.c
 *  Samsung rt9532 Charger Driver
 *
 *  Copyright (C) 2012 Samsung Electronics
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#define DEBUG

#include <linux/seq_file.h>
#include <linux/i2c.h>
#include <linux/mutex.h>
#include <linux/power_supply.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/battery/charger/rt9532_charger.h>
#include <linux/delay.h>
#include <soc/sprd/adc.h>

static enum power_supply_property rt9532_charger_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_CHARGE_TYPE,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_CURRENT_MAX,
	POWER_SUPPLY_PROP_CURRENT_AVG,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN,
	POWER_SUPPLY_PROP_CHARGE_OTG_CONTROL,
	POWER_SUPPLY_PROP_USB_HC,
	POWER_SUPPLY_PROP_CHARGE_NOW,
#if defined(CONFIG_BATTERY_SWELLING)
	POWER_SUPPLY_PROP_VOLTAGE_MAX,
#endif
	POWER_SUPPLY_PROP_CHARGING_ENABLED,
};

unsigned int jig_set;
EXPORT_SYMBOL(jig_set);

static int __init check_jig_uart_set(char *str)
{
	if (strncmp(str, "1", 1) == 0)
		jig_set = 1;
	pr_info("%s: Jig Cable Set : %d\n", __func__, jig_set);
}
__setup("jig_uart_set=", check_jig_uart_set);

static int __init check_jig_usb_set(char *str)
{
	if (strncmp(str, "1", 1) == 0)
		jig_set = 2;
	pr_info("%s: Jig Cable Set : %d\n", __func__, jig_set);
}
__setup("jig_usb_set=", check_jig_usb_set);

static void rt9532_select_mode(struct rt9532_charger *charger, int mode, bool cv_set)
{
	u64 ts;
	int i;
	unsigned long flags;

	spin_lock_irqsave(&charger->io_lock, flags);

	for (i = 0; i < mode; i++) {
		gpio_set_value(charger->en_set, 0);
		ts =  local_clock() + LOW_PERIOD * 1000;
		while(time_after64(ts, local_clock()));
		gpio_set_value(charger->en_set, 1);
		ts =  local_clock() + HIGH_PERIOD * 1000;
		while(time_after64(ts, local_clock()));
	}

	gpio_set_value(charger->en_set, 0);
	spin_unlock_irqrestore(&charger->io_lock, flags);

	msleep(WAIT_TIME / 1000);

	if (cv_set) {
		spin_lock_irqsave(&charger->io_lock, flags);
		gpio_set_value(charger->en_set, 1);
		ts =  local_clock() + CV_HIGH_PERIOD * 1000;
		while(time_after64(ts, local_clock()));
		gpio_set_value(charger->en_set, 0);
		spin_unlock_irqrestore(&charger->io_lock, flags);
		msleep(WAIT_TIME / 1000);
	}

	pr_info("%s : MODE(%d) CV(%d)\n", __func__, mode, cv_set);
}

static int vf_check = 0;
static int rt9532_charger_vf_check(void)
{
	int adc = 0;

	adc = sci_adc_get_value(0, false);
	pr_info("%s, vf adc : %d", __func__, adc);

	if (adc > 500) {
		vf_check = 0;
		return 0;
	} else {
		vf_check = 1;
		return 1;
	}
}

static void rt9532_charger_disable(struct rt9532_charger *charger)
{
	if (jig_set && !vf_check) {
		pr_info("%s : JIG SET\n", __func__);
		return;
	}

	gpio_set_value(charger->en_set, 1);
	udelay(RESET_TIME);

	pr_info("%s : CHG DISABLE(%d)\n", __func__, gpio_get_value(charger->en_set));
}

static void rt9532_charger_function_control(struct rt9532_charger *charger)
{
	if (jig_set && !vf_check) {
		pr_info("%s : JIG SET\n", __func__);
		return;
	} else if (charger->siop_level < 100) {
		if (charger->pdata->chg_float_voltage > 4200)
			rt9532_select_mode(charger, 4, true);
		else
			rt9532_select_mode(charger, 4, false);
	} else {
		if (charger->cable_type == POWER_SUPPLY_TYPE_MAINS) {
			if (charger->pdata->chg_float_voltage > 4200)
				rt9532_select_mode(charger, 1, true);
			else
				rt9532_select_mode(charger, 1, false);
		} else if (charger->cable_type == POWER_SUPPLY_TYPE_USB) {
			if (charger->pdata->chg_float_voltage > 4200)
				rt9532_select_mode(charger, 4, true);
			else
				rt9532_select_mode(charger, 4, false);
		} else if (charger->cable_type == POWER_SUPPLY_TYPE_BATTERY) {
			rt9532_select_mode(charger, 4, true);
		}
	}
}

static int rt9532_charger_topoff(struct rt9532_charger *charger) 
{
	int chgsb, pgb;
	int status = POWER_SUPPLY_STATUS_UNKNOWN;

	chgsb = gpio_get_value(charger->chg_state);
	pgb = gpio_get_value(charger->chg_det);

	if (chgsb && !pgb)
		status = POWER_SUPPLY_STATUS_FULL;
	else if (!chgsb && !pgb)
		status = POWER_SUPPLY_STATUS_CHARGING;

	pr_info("%s : CHGSB(%d) PGB(%d)\n", __func__, chgsb, pgb);

	return status;
}

static int rt9532_get_charging_health(struct rt9532_charger *charger) 
{
	int chgsb, pgb;
	int health = POWER_SUPPLY_HEALTH_GOOD;

	chgsb = gpio_get_value(charger->chg_state);
	pgb = gpio_get_value(charger->chg_det);

	if (chgsb && pgb)
		health = POWER_SUPPLY_HEALTH_OVERVOLTAGE;

	pr_info("%s : CHGSB(%d) PGB(%d)\n", __func__, chgsb, pgb);

	return health;
}

static int rt9532_chg_get_property(struct power_supply *psy,
			      enum power_supply_property psp,
			      union power_supply_propval *val)
{
	struct rt9532_charger *charger =
		container_of(psy, struct rt9532_charger, psy_chg);

	val->intval = 0;

	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = charger->cable_type;
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = rt9532_charger_vf_check();
		break;
	case POWER_SUPPLY_PROP_STATUS:
		val->intval = rt9532_charger_topoff(charger);
		break;
	case POWER_SUPPLY_PROP_CHARGE_TYPE:
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		val->intval = rt9532_get_charging_health(charger);
		break;
	case POWER_SUPPLY_PROP_CURRENT_MAX:
		break;
	case POWER_SUPPLY_PROP_CURRENT_AVG:
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		break;
	case POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN:
		break;
	case POWER_SUPPLY_PROP_CHARGE_OTG_CONTROL:
		break;
#if defined(CONFIG_BATTERY_SWELLING)
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		break;
#endif
	case POWER_SUPPLY_PROP_USB_HC:
		return -ENODATA;
	case POWER_SUPPLY_PROP_CHARGE_NOW:
		break;
	case POWER_SUPPLY_PROP_CHARGING_ENABLED:
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static int rt9532_chg_set_property(struct power_supply *psy,
			  enum power_supply_property psp,
			  const union power_supply_propval *val)
{
	struct rt9532_charger *charger =
		container_of(psy, struct rt9532_charger, psy_chg);

	switch (psp) {
	/* val->intval : type */
	case POWER_SUPPLY_PROP_STATUS:
		charger->status = val->intval;
		break;
	case POWER_SUPPLY_PROP_ONLINE:
		charger->cable_type = val->intval;
		break;
	/* val->intval : input charging current */
	case POWER_SUPPLY_PROP_CURRENT_MAX:
		break;
	/*  val->intval : charging current */
	case POWER_SUPPLY_PROP_CURRENT_AVG:
		break;
	/* val->intval : charging current */
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		break;
	case POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN:
		charger->siop_level = val->intval;
		if (!gpio_get_value(charger->en_set)) {
			rt9532_charger_disable(charger);
		}
		rt9532_charger_function_control(charger);
		break;
	case POWER_SUPPLY_PROP_USB_HC:
	case POWER_SUPPLY_PROP_CHARGE_OTG_CONTROL:
		break;
#if defined(CONFIG_BATTERY_SWELLING)
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		pr_info("%s: float voltage(%d)\n", __func__, val->intval);
		if (charger->pdata->chg_float_voltage == val->intval) {
			break;
		} else {
			charger->pdata->chg_float_voltage = val->intval;
			if (!gpio_get_value(charger->en_set)) {
				rt9532_charger_disable(charger);
			}
			rt9532_charger_function_control(charger);
			if (charger->pdata->chg_float_voltage <= 4200) {
				u64 ts;
				unsigned long flags;
				msleep(10);
				spin_lock_irqsave(&charger->io_lock, flags);
				gpio_set_value(charger->en_set, 1);
				ts =  local_clock() + SWELLING_PERIOD * 1000;
				while(time_after64(ts, local_clock()));
				gpio_set_value(charger->en_set, 0);
				spin_unlock_irqrestore(&charger->io_lock, flags);
			}
		}
		break;
#endif
	case POWER_SUPPLY_PROP_CHARGE_NOW:
		break;
	case POWER_SUPPLY_PROP_CHARGING_ENABLED:
		charger->is_charging = val->intval;
		if (!charger->is_charging) {
			rt9532_charger_disable(charger);
		} else {
			if (!gpio_get_value(charger->en_set)) {
				rt9532_charger_disable(charger);
			}
			rt9532_charger_function_control(charger);
		}
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

int rt9532_charger_dt_init(struct device *dev,
			   struct rt9532_charger *charger)
{
	struct device_node *np = dev->of_node;
	sec_battery_platform_data_t *pdata = charger->pdata;
	int ret = 0, len = 0;

	if (!np)
		return -EINVAL;

	charger->en_set = of_get_named_gpio(np, "en-set", 0);
	if (charger->en_set < 0) {
		pr_err("%s: of_get_named_gpio(CHG EN) failed: %d\n", __func__,
		       charger->en_set);
		charger->en_set = 0;
	}

	charger->chg_state = of_get_named_gpio(np, "chg-state", 0);
	if (charger->chg_state < 0) {
		pr_err("%s: of_get_named_gpio(CHG STATE) failed: %d\n", __func__,
		       charger->chg_state);
		charger->chg_state = 0;
	}

	charger->chg_det = of_get_named_gpio(np, "chg-det", 0);
	if (charger->chg_det < 0) {
		pr_err("%s: of_get_named_gpio(CHG DET) failed: %d\n", __func__,
		       charger->chg_det);
		charger->chg_det = 0;
	}

	ret = of_property_read_u32(np, "chg-float-voltage",
					&pdata->chg_float_voltage);
	if (ret)
		return ret;

        np = of_find_node_by_name(NULL, "sec-battery");
        if (!np) {
                pr_err("%s np NULL\n", __func__);
        }
        else {
                int i = 0;
                const u32 *p;
                p = of_get_property(np, "battery,input_current_limit", &len);
                if (!p){

                        pr_err("%s charger,input_current_limit is Empty\n", __func__);
                        //	return 1;
                }
                else{

                        len = len / sizeof(u32);

                        pdata->charging_current = kzalloc(sizeof(sec_charging_current_t) * len,
                                        GFP_KERNEL);

                        for(i = 0; i < len; i++) {
                                ret = of_property_read_u32_index(np,
                                                "battery,input_current_limit", i,
                                                &pdata->charging_current[i].input_current_limit);
                                if (ret)
                                        pr_info("%s : Input_current_limit is Empty\n",
                                                        __func__);

                                ret = of_property_read_u32_index(np,
                                                "battery,fast_charging_current", i,
                                                &pdata->charging_current[i].fast_charging_current);
                                if (ret)
                                        pr_info("%s : Fast charging current is Empty\n",
                                                        __func__);

                                ret = of_property_read_u32_index(np,
                                                "battery,full_check_current_1st", i,
                                                &pdata->charging_current[i].full_check_current_1st);
                                if (ret)
                                        pr_info("%s : Full check current 1st is Empty\n",
                                                        __func__);

                                ret = of_property_read_u32_index(np,
                                                "battery,full_check_current_2nd", i,
                                                &pdata->charging_current[i].full_check_current_2nd);
                                if (ret)
                                        pr_info("%s : Full check current 2nd is Empty\n",
                                                        __func__);
                        }
                }
        }

	ret = of_property_read_u32(np, "battery,ovp_uvlo_check_type",
			&pdata->ovp_uvlo_check_type);
	if (ret)
		pr_info("%s : Ovp Uvlo check type is Empty\n", __func__);

	ret = of_property_read_u32(np, "battery,full_check_type",
			&pdata->full_check_type);
	if (ret)
		pr_info("%s : Full check type is Empty\n", __func__);

	ret = of_property_read_u32(np, "battery,full_check_type_2nd",
			&pdata->full_check_type_2nd);
	if (ret)
		pr_info("%s : Full check type 2nd is Empty\n", __func__);

	return 0;
}

static int rt9532_charger_probe(struct platform_device *pdev)
{
	sec_battery_platform_data_t *pdata;
	struct rt9532_charger *charger;
	int ret = 0;

	pr_info("%s: rt9532 Charger Driver Loading\n", __func__);

	charger = kzalloc(sizeof(*charger), GFP_KERNEL);
	if (!charger)
		return -ENOMEM;

	if (pdev->dev.of_node) {
		pdata = devm_kzalloc(&pdev->dev,
				sizeof(sec_battery_platform_data_t),
				GFP_KERNEL);
		if (!pdata) {
			dev_err(&pdev->dev, "Failed to allocate memory\n");
			ret = -ENOMEM;
			goto err_charger_free;
		}
		charger->pdata = pdata;
#ifdef CONFIG_OF
		if (rt9532_charger_dt_init(&pdev->dev, charger))
			dev_err(&pdev->dev,
				"%s: Failed to get battery init\n", __func__);
#endif
	} else {
		pdata = dev_get_platdata(&pdev->dev);
		charger->pdata = pdata;
	}

	platform_set_drvdata(pdev, charger);

        charger->dev = &pdev->dev;

	charger->psy_chg.name		= "sec-charger";
	charger->psy_chg.type		= POWER_SUPPLY_TYPE_UNKNOWN;
	charger->psy_chg.get_property	= rt9532_chg_get_property;
	charger->psy_chg.set_property	= rt9532_chg_set_property;
	charger->psy_chg.properties	= rt9532_charger_props;
	charger->psy_chg.num_properties	= ARRAY_SIZE(rt9532_charger_props);

	charger->siop_level = 100;

	ret = power_supply_register(&pdev->dev, &charger->psy_chg);
	if (ret) {
		pr_err("%s: Failed to Register psy_chg\n", __func__);
		goto err_data_free;
	}

	if (charger->en_set > 0) {
		ret = gpio_request(charger->en_set, "en-set");
		if (ret) {
			pr_err("%s gpio_request failed: %d\n", __func__, charger->en_set);
			return ret;
		}
		gpio_direction_output(charger->en_set, 0);
	}

	if (charger->chg_state > 0) {
		ret = gpio_request(charger->chg_state, "chg-state");
		if (ret) {
			pr_err("%s gpio_request failed: %d\n", __func__, charger->chg_state);
			return ret;
		}
	}

	if (charger->chg_det > 0) {
		ret = gpio_request(charger->chg_det, "chg-det");
		if (ret) {
			pr_err("%s gpio_request failed: %d\n", __func__, charger->chg_det);
			return ret;
		}
	}

	pr_info("%s: rt9532 Charger Driver Loaded\n", __func__);

	return 0;

err_data_free:
	kfree(pdata);
err_charger_free:
	kfree(charger);
	return ret;
}

static int rt9532_charger_prepare(struct device *dev)
{
	return 0;
}

static void rt9532_charger_shutdown(struct device *dev)
{
}

static int rt9532_charger_remove(struct device *dev)
{
	return 0;
}

static int rt9532_charger_suspend(struct device *dev)
{
	return 0;
}

static int rt9532_charger_resume(struct device *dev)
{
	return 0;
}
static void rt9532_charger_complete(struct device *dev)
{
}

#ifdef CONFIG_OF
static struct of_device_id rt9532_charger_dt_ids[] = {
	{ .compatible = "samsung,rt9532-charger"},
	{ }
};
MODULE_DEVICE_TABLE(of, rt9532_charger_dt_ids);
#endif /* CONFIG_OF */

static const struct dev_pm_ops rt9532_charger_pm_ops = {
	.prepare = rt9532_charger_prepare,
	.suspend = rt9532_charger_suspend,
	.resume = rt9532_charger_resume,
	.complete = rt9532_charger_complete,
};

static struct platform_driver rt9532_charger_driver = {
	.driver = {
		.name = "rt9532-charger",
		.owner = THIS_MODULE,
		.shutdown = rt9532_charger_shutdown,
		.pm = &rt9532_charger_pm_ops,
#ifdef CONFIG_OF
		.of_match_table = rt9532_charger_dt_ids,
#endif
	},
	.probe = rt9532_charger_probe,
	.remove = rt9532_charger_remove,
};

static int __init rt9532_charger_init(void)
{
	pr_info("%s : \n", __func__);
	return platform_driver_register(&rt9532_charger_driver);
}

static void __exit rt9532_charger_exit(void)
{
	platform_driver_unregister(&rt9532_charger_driver);
}

module_init(rt9532_charger_init);
module_exit(rt9532_charger_exit);

MODULE_DESCRIPTION("Samsung rt9532 Charger Driver");
MODULE_AUTHOR("Samsung Electronics");
MODULE_LICENSE("GPL");
