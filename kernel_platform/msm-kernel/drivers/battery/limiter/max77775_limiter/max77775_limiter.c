/*
 * Copyright (c) 2023 Maxim Integrated Products, Inc.
 * Author: Maxim Integrated <opensource@maximintegrated.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include "max77775_limiter.h"
#include <linux/version.h>
#include <asm/unaligned.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/mutex.h>
#include <linux/err.h>
#include <linux/delay.h>
#include <linux/power_supply.h>
#include <linux/slab.h>
#include <linux/interrupt.h>

/* for Regmap */
#include <linux/regmap.h>

/* for Device Tree */
#include <linux/io.h>
#include <linux/of.h>
#include <linux/of_irq.h>

static int max77775_limiter_get_property(struct power_supply *psy,
		enum power_supply_property psp, union power_supply_propval *val)
{
	struct max77775_limiter_info *limiter =
		power_supply_get_drvdata(psy);
	enum power_supply_ext_property ext_psp = (enum power_supply_ext_property)psp;
	union power_supply_propval value = {0,};

	value.intval = val->intval;

	switch ((int)psp) {
	case POWER_SUPPLY_PROP_STATUS:
		val->intval = 0;
		break;
	case POWER_SUPPLY_PROP_CURRENT_AVG:
		psy_do_property(limiter->pdata->fuelgauge_name, get,
			POWER_SUPPLY_PROP_CURRENT_AVG, value);
		val->intval = value.intval;
		break;
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		psy_do_property(limiter->pdata->fuelgauge_name, get,
			POWER_SUPPLY_PROP_CURRENT_NOW, value);
		val->intval = value.intval;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_AVG:
		psy_do_property(limiter->pdata->fuelgauge_name, get,
			POWER_SUPPLY_PROP_VOLTAGE_AVG, value);
		val->intval = value.intval;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		psy_do_property(limiter->pdata->fuelgauge_name, get,
			POWER_SUPPLY_PROP_VOLTAGE_NOW, value);
		val->intval = value.intval;
		break;
	case POWER_SUPPLY_EXT_PROP_MIN ... POWER_SUPPLY_EXT_PROP_MAX:
		switch (ext_psp) {
		case POWER_SUPPLY_EXT_PROP_LIMITER_SHIPMODE:
			psy_do_property(limiter->pdata->charger_name, get,
				POWER_SUPPLY_EXT_PROP_SHIPMODE_TEST, value);
			val->intval = value.intval;
			break;
		case POWER_SUPPLY_EXT_PROP_MONITOR_WORK:
			break;
		case POWER_SUPPLY_EXT_PROP_FGSRC_SWITCHING:
			break;
		default:
			return -EINVAL;
		}
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int max77775_limiter_set_property(struct power_supply *psy,
								 enum power_supply_property psp,
								 const union power_supply_propval *val)
{
	struct max77775_limiter_info *limiter =
		power_supply_get_drvdata(psy);
	int ret = 0;
	enum power_supply_ext_property ext_psp = (enum power_supply_ext_property)psp;
	union power_supply_propval value = {0,};

	value.intval = val->intval;

	switch ((int)psp) {
	case POWER_SUPPLY_PROP_CHARGE_FULL:
		pr_info("%s: is it full? %d\n", __func__, val->intval);
		/* need to be set by max77775 charger driver */
		if (val->intval == SEC_DUAL_BATTERY_CHG_ON || val->intval == SEC_DUAL_BATTERY_BUCK_OFF)
			break;
		else {
			value.intval = SEC_BAT_CHG_MODE_CHARGING_OFF;
			psy_do_property(limiter->pdata->charger_name, set,
				POWER_SUPPLY_EXT_PROP_CHARGING_ENABLED, value);
		}
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE:
		break;
	case POWER_SUPPLY_EXT_PROP_MIN ... POWER_SUPPLY_EXT_PROP_MAX:
		switch (ext_psp) {
		case POWER_SUPPLY_EXT_PROP_SUPLLEMENT_MODE:
			break;
		case POWER_SUPPLY_EXT_PROP_CHG_MODE:
			break;
		case POWER_SUPPLY_EXT_PROP_FASTCHG_LIMIT_CURRENT:
			psy_do_property(limiter->pdata->charger_name, set,
				POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT, value);
			break;
		case POWER_SUPPLY_EXT_PROP_LIMITER_SHIPMODE:
			psy_do_property(limiter->pdata->charger_name, set,
				POWER_SUPPLY_EXT_PROP_SHIPMODE_TEST, value);
			break;
		case POWER_SUPPLY_EXT_PROP_SYS_TRACK_DIS:
			psy_do_property(limiter->pdata->charger_name, set,
				POWER_SUPPLY_EXT_PROP_SYS_TRACK_DIS, value);
			break;
		case POWER_SUPPLY_EXT_PROP_ARI_CNT:
			break;
		case POWER_SUPPLY_EXT_PROP_FGSRC_SWITCHING:
			{
				psy_do_property(limiter->pdata->charger_name, set,
					POWER_SUPPLY_EXT_PROP_FGSRC_SWITCHING, value);

				pr_info("%s: POWER_SUPPLY_EXT_PROP_FGSRC_SWITCHING(%d)\n", __func__, value.intval);
			}		
			break;
		default:
			return -EINVAL;
		}
		break;
	default:
		return -EINVAL;
	}

	return ret;
}

static enum power_supply_property max77775_limiter_props[] = {
	POWER_SUPPLY_PROP_STATUS,
};

static const struct power_supply_desc max77775_limiter_power_supply_desc = {
	.name = "max77775-limiter",
	.type = POWER_SUPPLY_TYPE_UNKNOWN,
	.properties = max77775_limiter_props,
	.num_properties = ARRAY_SIZE(max77775_limiter_props),
	.get_property = max77775_limiter_get_property,
	.set_property = max77775_limiter_set_property,
};

static int max77775_limiter_probe(struct platform_device *pdev)
{
	struct max77775_limiter_info *limiter;
	struct max77775_limiter_platform_data *pdata = NULL;
	struct device_node *np = of_find_node_by_name(NULL, "max77775-limiter");
	struct power_supply_config psy_cfg = {};
	int ret = 0;

	dev_info(&pdev->dev,
		"%s: MAX77775 limiter Wrapper Driver Loading\n", __func__);

	limiter = kzalloc(sizeof(*limiter), GFP_KERNEL);
	if (!limiter)
		return -ENOMEM;

	pdata = devm_kzalloc(&pdev->dev,
			sizeof(struct max77775_limiter_platform_data),
			GFP_KERNEL);
	if (!pdata) {
		dev_err(&pdev->dev, "Failed to allocate memory\n");
		ret = -ENOMEM;
		goto err_limiter_free;
	}

	ret = of_property_read_u32(np, "battery-type", &pdata->battery_type);
	if (ret < 0)
		pdata->battery_type = 1; /* main */

	pdata->charger_name = "max77775-charger";
	pdata->fuelgauge_name = "max77775-fuelgauge";

	limiter->pdata = pdata;
	platform_set_drvdata(pdev, limiter);
	limiter->dev = &pdev->dev;
	psy_cfg.drv_data = limiter;

	limiter->psy_sw = power_supply_register(&pdev->dev, &max77775_limiter_power_supply_desc, &psy_cfg);
	if (IS_ERR(limiter->psy_sw)) {
		ret = PTR_ERR(limiter->psy_sw);
		dev_err(limiter->dev,
			"%s: Failed to Register psy_sw(%d)\n", __func__, ret);
		goto err_pdata_free;
	}

	if (pdata->battery_type)
		sec_chg_set_dev_init(SC_DEV_MAIN_LIM);
	else
		sec_chg_set_dev_init(SC_DEV_SUB_LIM);

	dev_info(limiter->dev,
		"%s: MAX77775 limiter Wrapper Driver Loaded\n", __func__);
	return 0;

err_pdata_free:
	kfree(pdata);
err_limiter_free:
	kfree(limiter);

	return ret;
}

static int max77775_limiter_remove(struct platform_device *pdev)
{
	struct max77775_limiter_info *limiter = platform_get_drvdata(pdev);

	power_supply_unregister(limiter->psy_sw);

	kfree(limiter->pdata);
	kfree(limiter);

	return 0;
}

static int max77775_limiter_suspend(struct device *dev)
{
	return 0;
}

static int max77775_limiter_resume(struct device *dev)
{
	return 0;
}

static void max77775_limiter_shutdown(struct platform_device *pdev)
{
}

#ifdef CONFIG_OF
static const struct of_device_id max77775_limiter_dt_ids[] = {
	{ .compatible = "samsung,max77775-limiter" },
	{ }
};
MODULE_DEVICE_TABLE(of, max77775_limiter_dt_ids);
#endif /* CONFIG_OF */

static const struct dev_pm_ops max77775_limiter_pm_ops = {
	.suspend = max77775_limiter_suspend,
	.resume = max77775_limiter_resume,
};

static struct platform_driver max77775_limiter_driver = {
	.driver = {
		.name = "max77775-limiter",
		.owner = THIS_MODULE,
		.pm = &max77775_limiter_pm_ops,
#ifdef CONFIG_OF
		.of_match_table = max77775_limiter_dt_ids,
#endif
	},
	.probe = max77775_limiter_probe,
	.remove = max77775_limiter_remove,
	.shutdown = max77775_limiter_shutdown,
};

static int __init max77775_limiter_init(void)
{
	pr_info("%s\n", __func__);
	return platform_driver_register(&max77775_limiter_driver);
}

static void __exit max77775_limiter_exit(void)
{
	platform_driver_unregister(&max77775_limiter_driver);
}

module_init(max77775_limiter_init);
module_exit(max77775_limiter_exit);

MODULE_SOFTDEP("pre: max77775_charger");
MODULE_DESCRIPTION("Samsung MAX77775 limiter Wrapper Driver");
MODULE_AUTHOR("Samsung Electronics");
MODULE_LICENSE("GPL");
