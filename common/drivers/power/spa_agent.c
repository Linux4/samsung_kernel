/* Samsung Power and Charger Agent
 * 
 * drivers/power/spa_agent.c
 *
 * Agent driver for SPA driver.
 *
 * Copyright (C) 2012, Samsung Electronics.
 *
 * This program is free software. You can redistribute it and/or modify
 * it under the terms of the GNU(General Public License version 2) as 
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/power_supply.h>
#include <linux/wakelock.h>
#include <linux/spa_power.h>
#include <linux/spa_agent.h>

//#define SPA_AGENT_DIRECT_INTERMEDIATE
#define SPA_AGENT_PS_INTERMEDIATE

// debugging
#define SPA_AGENT_DBG_LEVEL0 0U // no log
#define SPA_AGENT_DBG_LEVEL1 1U // minimum log
#define SPA_AGENT_DBG_LEVEL2 2U
#define SPA_AGENT_DBG_LEVEL3 3U
#define SPA_AGENT_DBG_LEVEL4 4U
#define SPA_AGENT_DBG_LEVEL5 5U // maximum log
#define SPA_AGENT_DBG_LEVEL_INTERNAL SPA_AGENT_DBG_LEVEL1
#define SPA_AGENT_DBG_LEVEL_OUT SPA_AGENT_DBG_LEVEL1

#define SPA_AGENT_CHARGER_NAME "spa_agent_chrg"

#define pr_spa_agent_dbg(lvl, args...) \
	do { \
		if ( SPA_AGENT_DBG_LEVEL_OUT >= SPA_AGENT_DBG_##lvl ) \
		{ \
			pr_info(args); \
		} \
	} while(0)

struct spa_agent
{
#if defined(SPA_AGENT_PS_INTERMEDIATE)
	struct power_supply agent_power;
#endif
};

static struct spa_agent_fn spa_agent_fn[SPA_AGENT_MAX];
#if defined(SPA_AGENT_PS_INTERMEDIATE)
static enum power_supply_property spa_agent_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_TEMP,
	POWER_SUPPLY_PROP_TYPE,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN,
	POWER_SUPPLY_PROP_BATT_TEMP_ADC,

};
#endif

int spa_agent_register(unsigned int agent_id, void *fn, const char *agent_name)
{
	int ret=0;

	if(agent_id >= SPA_AGENT_MAX)
	{
		pr_spa_agent_dbg(LEVEL1, "%s: agent id is wrong(agent_id = %d), failed to register agent fn \n", __func__, agent_id);
		return -1;
	}
	if(fn == NULL)
	{
		pr_spa_agent_dbg(LEVEL1, "%s: agent fn is NULL(agent_id = %d), failed to register agent fn\n", __func__, agent_id);
		return -1;
	}

	if(spa_agent_fn[agent_id].fn.dummy == NULL)
	{
		spa_agent_fn[agent_id].fn.dummy= (int (*)(void))fn;
		spa_agent_fn[agent_id].agent_name = agent_name;
		pr_spa_agent_dbg(LEVEL1, "%s : agent id %d has been successfully registered!!\n", __func__, agent_id);
		pr_spa_agent_dbg(LEVEL1, "%s : agent_name = %s \n", __func__, spa_agent_fn[agent_id].agent_name);
	}
	else
	{
		pr_spa_agent_dbg(LEVEL1, "%s : agent id %d is already registered!!\n", __func__, agent_id);
		return -1;
	}
	return ret;
}
EXPORT_SYMBOL_GPL(spa_agent_register);

#if defined(SPA_AGENT_PS_INTERMEDIATE)
static int spa_agent_get_property(struct power_supply *ps, enum power_supply_property prop, union power_supply_propval *propval)
{
	int ret = 0;

	pr_spa_agent_dbg(LEVEL3, "%s : agent get request = %d \n", __func__, prop);
	propval->intval = 0;

	switch(prop) {
		case POWER_SUPPLY_PROP_STATUS:
			ret = -ENODATA;
			break;
		case POWER_SUPPLY_PROP_TYPE:
			pr_spa_agent_dbg(LEVEL3, "%s : agent id = %d \n", __func__, SPA_AGENT_GET_CHARGER_TYPE);
			if(spa_agent_fn[SPA_AGENT_GET_CHARGER_TYPE].fn.get_charger_type)
				propval->intval = spa_agent_fn[SPA_AGENT_GET_CHARGER_TYPE].fn.get_charger_type();
			else
			{
				propval->intval = POWER_SUPPLY_TYPE_BATTERY;
				ret = -ENODATA;
			}
			break;
		case POWER_SUPPLY_PROP_TEMP:
			pr_spa_agent_dbg(LEVEL3, "%s : agent id = %d \n", __func__, SPA_AGENT_GET_TEMP);
			if(spa_agent_fn[SPA_AGENT_GET_TEMP].fn.get_temp)
				propval->intval = spa_agent_fn[SPA_AGENT_GET_TEMP].fn.get_temp(0);
			else
				ret = -ENODATA;
			break;
		case POWER_SUPPLY_PROP_BATT_TEMP_ADC:
			pr_spa_agent_dbg(LEVEL3, "%s : agent id = %d \n", __func__, SPA_AGENT_GET_TEMP);
			if(spa_agent_fn[SPA_AGENT_GET_TEMP].fn.get_temp)
				propval->intval = spa_agent_fn[SPA_AGENT_GET_TEMP].fn.get_temp(1);
			else
				ret = -ENODATA;
			break;
		case POWER_SUPPLY_PROP_CAPACITY:
			pr_spa_agent_dbg(LEVEL3, "%s : agent id = %d \n", __func__, SPA_AGENT_GET_CAPACITY);
			if(spa_agent_fn[SPA_AGENT_GET_CAPACITY].fn.get_capacity)
			{	propval->intval = spa_agent_fn[SPA_AGENT_GET_CAPACITY].fn.get_capacity();
			}
			else
				ret = -ENODATA;
			break;
		case POWER_SUPPLY_PROP_VOLTAGE_NOW:
			pr_spa_agent_dbg(LEVEL3, "%s : agent id = %d \n", __func__, SPA_AGENT_GET_VOLTAGE);
			if(spa_agent_fn[SPA_AGENT_GET_VOLTAGE].fn.get_voltage)
				propval->intval = spa_agent_fn[SPA_AGENT_GET_VOLTAGE].fn.get_voltage(0);
			else
				ret = -ENODATA;
			break;
		case POWER_SUPPLY_PROP_PRESENT:
			pr_spa_agent_dbg(LEVEL3, "%s : agent id = %d \n", __func__, SPA_AGENT_GET_BATT_PRESENCE);
			//#ifdef CONFIG_STC3117_FUELGAUGE
			if(spa_agent_fn[SPA_AGENT_GET_BATT_PRESENCE].fn.get_batt_presence)
				propval->intval = spa_agent_fn[SPA_AGENT_GET_BATT_PRESENCE].fn.get_batt_presence(0);
			else
			//#endif
			{
				propval->intval = 1;
				ret = -ENODATA;
			}
			break;
		case POWER_SUPPLY_PROP_CURRENT_NOW:
			pr_spa_agent_dbg(LEVEL3, "%s : agent id = %d \n", __func__, SPA_AGENT_GET_CURRENT);
			if(spa_agent_fn[SPA_AGENT_GET_CURRENT].fn.get_batt_current)
				//propval->intval = spa_agent_fn[SPA_AGENT_GET_CURRENT].fn.get_current(1); // eoc check
				propval->intval= spa_agent_fn[SPA_AGENT_GET_CURRENT].fn.get_batt_current(0);
			else
				ret = -ENODATA;
			break;
		case POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN:
		default:
			ret = -ENODATA;
			break;
	}

	pr_spa_agent_dbg(LEVEL3, "%s : DONE !!  agent get request = %d \n", __func__, prop);
	return ret;
}

static int spa_agent_set_property(struct power_supply *ps, enum power_supply_property prop, const union power_supply_propval *propval)
{
	int ret = 0;

	pr_spa_agent_dbg(LEVEL3, "%s : agent set request = %d \n", __func__, prop);

	switch(prop)
	{
		case POWER_SUPPLY_PROP_STATUS:
			pr_spa_agent_dbg(LEVEL3, "%s : agent id = %d \n", __func__, SPA_AGENT_SET_CHARGE);
			{
				int en=0;
				if (propval->intval == POWER_SUPPLY_STATUS_CHARGING)
					en=1;

				if(spa_agent_fn[SPA_AGENT_SET_CHARGE].fn.set_charge)
					spa_agent_fn[SPA_AGENT_SET_CHARGE].fn.set_charge(en);
				else
					ret = -ENODATA;
			}
			break;
		case POWER_SUPPLY_PROP_CURRENT_NOW:
			pr_spa_agent_dbg(LEVEL3, "%s : agent id = %d \n", __func__, SPA_AGENT_SET_CHARGE_CURRENT);
			{
				int curr = propval->intval;
				if(spa_agent_fn[SPA_AGENT_SET_CHARGE_CURRENT].fn.set_charge_current)
					spa_agent_fn[SPA_AGENT_SET_CHARGE_CURRENT].fn.set_charge_current(curr);
				else
					ret = -ENODATA;
			}
			break;
		case POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN:
			pr_spa_agent_dbg(LEVEL3, "%s : agent id = %d \n", __func__, SPA_AGENT_SET_FULL_CHARGE);
			{
				int eoc_current=propval->intval;
				if(spa_agent_fn[SPA_AGENT_SET_FULL_CHARGE].fn.set_full_charge)
					spa_agent_fn[SPA_AGENT_SET_FULL_CHARGE].fn.set_full_charge(eoc_current);
				else
					ret = -ENODATA;
			}
			break;
		case POWER_SUPPLY_PROP_CAPACITY:
			pr_spa_agent_dbg(LEVEL3, "%s : agent id = %d \n", __func__, SPA_AGENT_CTRL_FG);
			{
				if(spa_agent_fn[SPA_AGENT_CTRL_FG].fn.ctrl_fg)
				{
					int opt = propval->intval;
					spa_agent_fn[SPA_AGENT_CTRL_FG].fn.ctrl_fg((void *)opt);
				}
			}
			break;
		default:
			ret = -ENODATA;
			break;
	}
	pr_spa_agent_dbg(LEVEL3, "%s : DONE !!  agent set request = %d \n", __func__, prop);

	return ret;
}
#endif

#if defined(SPA_AGENT_DIRECT_INTERMEDIATE)
int spa_agent_set(unsigned int agent_id, void *data)
{
}
int spa_agent_get(unsigned int agent_id, void *data)
{
}
int spa_agent_ctrl(unsigned int agent_id, void *data)
{
}
#endif

static int spa_agent_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct spa_agent *spa_agent;

	pr_spa_agent_dbg(LEVEL1, "%s : called\n", __func__);

	spa_agent = kzalloc(sizeof(struct spa_agent), GFP_KERNEL);
	if(spa_agent == NULL)
	{
		dev_err(&pdev->dev, "failed to alloc mem : %d\n", ret);
		return -ENOMEM;
	}

#if defined(SPA_AGENT_PS_INTERMEDIATE)
	spa_agent->agent_power.properties = spa_agent_props;
	spa_agent->agent_power.num_properties = ARRAY_SIZE(spa_agent_props);
	spa_agent->agent_power.get_property = spa_agent_get_property;
	spa_agent->agent_power.set_property = spa_agent_set_property;
	spa_agent->agent_power.name = "spa_agent_chrg";

	ret = power_supply_register(&pdev->dev, &spa_agent->agent_power);
	if(ret)
	{
		dev_err(&pdev->dev, "failed to register spa_agent: %d\n", ret);
		goto SPA_AGENT_ERR;
	}
#endif

	platform_set_drvdata(pdev, spa_agent);

	pr_spa_agent_dbg(LEVEL1, "%s : ended\n", __func__);
	return 0;
SPA_AGENT_ERR:
	dev_err(&pdev->dev, "Failed to init spa_agent\n");
	power_supply_unregister(&spa_agent->agent_power);
	kfree(spa_agent);
	return ret;
}

static int  spa_agent_remove(struct platform_device *pdev)
{
	struct spa_agent *spa_agent = platform_get_drvdata(pdev);

	pr_spa_agent_dbg(LEVEL4, "%s : enter\n", __func__);
	power_supply_unregister(&spa_agent->agent_power);
	kfree(spa_agent);

	return 0;
}

static int spa_agent_suspend(struct platform_device *pdev, pm_message_t state)
{
	return 0;
}

static int spa_agent_resume(struct platform_device *pdev)
{
	return 0;
}

static struct platform_driver spa_agent_driver = {
	.probe = spa_agent_probe,
	.remove = spa_agent_remove,
	.suspend = spa_agent_suspend,
	.resume = spa_agent_resume,
	.driver = {
		.name = "spa_agent",
		.owner = THIS_MODULE,
	},
};

static int __init spa_agent_init(void)
{
	int ret;
	ret = platform_driver_register(&spa_agent_driver);
	return ret;
}

static void __exit spa_agent_exit(void)
{
	platform_driver_unregister(&spa_agent_driver);
}

//module_init(spa_agent_init);
subsys_initcall_sync(spa_agent_init);
module_exit(spa_agent_exit);
