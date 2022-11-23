/* drivers/battery/rt5033_charger.c
 * RT5033 Charger Driver
 *
 * Copyright (C) 2013
 * Author: Patrick Chang <patrick_chang@richtek.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 */

#include <linux/battery/sec_charger.h>
#ifdef CONFIG_FLED_RT5033
#include <linux/leds/rt5033_fled.h>
#include <linux/leds/rtfled.h>
#endif /* CONFIG_FLED_RT5033 */

#define EN_OVP_IRQ 0
#define EN_IEOC_IRQ 1
#define EN_RECHG_REQ_IRQ 0
#define EN_TR_IRQ 0
#define EN_MIVR_SW_REGULATION 0
#define EN_BST_IRQ 0
#define MINVAL(a, b) ((a <= b) ? a : b)

static int rt5033_reg_map[] = {
	RT5033_CHG_STAT_CTRL,
	RT5033_CHG_CTRL1,
	RT5033_CHG_CTRL2,
	RT5033_CHG_CTRL3,
	RT5033_CHG_CTRL4,
	RT5033_CHG_CTRL5,
	RT5033_CHG_RESET,
	RT5033_UUG,
	RT5033_CHG_IRQ_CTRL1,
	RT5033_CHG_IRQ_CTRL2,
	RT5033_CHG_IRQ_CTRL3,
};

struct rt5033_charger_data {
	rt5033_mfd_chip_t *rt5033;
	sec_battery_platform_data_t	*pdata;
	struct power_supply	psy_chg;


	int charging_current;
	int cable_type;
	bool is_charging;

	/* register programming */
	int reg_addr;
	int reg_data;
	int irq_base;

	bool full_charged;

	/*add from max77803*/
	int status;
#ifdef CONFIG_FLED_RT5033
	struct rt_fled_info *fled_info;
#endif /* CONFIG_FLED_RT5033 */
};

static enum power_supply_property sec_charger_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_CHARGE_TYPE,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_CURRENT_MAX,
	POWER_SUPPLY_PROP_CURRENT_NOW,
};

static void rt5033_read_regs(struct i2c_client *i2c, char *str)
{
	u8 data = 0;
	int i = 0;
	for (i = 0; i < ARRAY_SIZE(rt5033_reg_map); i++) {
		data = rt5033_reg_read(i2c, rt5033_reg_map[i]);
		sprintf(str+strlen(str), "0x%02x, ", data);
	}
}

#if 0 // never used in this version
static void rt5033_charger_otg_control(struct rt5033_charger_data *charger)
{

	struct i2c_client *i2c = charger->rt5033->i2c_client;
	if (charger->cable_type == POWER_SUPPLY_TYPE_BATTERY) {
		/* turn off OTG */
		rt5033_clr_bits(i2c, RT5033_CHG_CTRL1, RT5033_OPAMODE_MASK);
		/*  turn on UUG */
		rt5033_set_bits(i2c, 0x19, 0x02);
	} else {
		/* Set OTG boost vout = 5V, turn on OTG */
		rt5033_assign_bits(charger->rt5033->i2c_client,
				RT5033_CHG_CTRL2, RT5033_VOREG_MASK,
				0x37 << RT5033_VOREG_SHIFT);
		rt5033_set_bits(charger->rt5033->i2c_client,
				RT5033_CHG_CTRL1, RT5033_OPAMODE_MASK);
		rt5033_clr_bits(charger->rt5033->i2c_client,
				RT5033_CHG_CTRL1, RT5033_HZ_MASK);
	}
}
#endif

#define RT5033_REGULATOR_REG_OUTPUT_EN (0x41)
#define RT5033_REGULATOR_EN_MASK_LDO_SAFE (1<<6)

#ifdef CONFIG_FLED_RT5033
extern int rt5033_fled_set_ta_status(
                struct i2c_client *iic, int ta_good, int ta_exist);
#endif

/* this function will work well on CHIP_REV = 3 or later */
static void rt5033_enable_charger_switch(struct rt5033_charger_data *charger,
		int onoff)
{
    struct i2c_client *iic = charger->rt5033->i2c_client;
#ifdef CONFIG_FLED_RT5033
    int fled_mode = FLASHLIGHT_MODE_OFF;
#endif
    charger->is_charging = onoff ? true : false;
	pr_info("%s:[BATT] set charger switch(%d)\n", __func__, onoff);
	if (onoff) {
	    /* S/W workaround to force turning on SafeLDO */
	    rt5033_set_bits(iic, RT5033_REGULATOR_REG_OUTPUT_EN,
                     RT5033_REGULATOR_EN_MASK_LDO_SAFE);

#ifdef CONFIG_FLED_RT5033
        if (charger->fled_info == NULL)
            charger->fled_info = rt_fled_get_info_by_name(NULL);
        if (charger->fled_info)
        {
            fled_mode = charger->fled_info->hal->fled_get_mode(charger->fled_info);
            if (fled_mode == FLASHLIGHT_MODE_TORCH ||
                fled_mode == FLASHLIGHT_MODE_MIXED) {
                /* turn off led*/
                charger->fled_info->hal->fled_set_mode(charger->fled_info,
                                                       FLASHLIGHT_MODE_OFF);
            }
        }
#endif /* CONFIG_FLED_RT5033 */
	    rt5033_clr_bits(iic, RT5033_CHG_CTRL3, RT5033_COF_EN_MASK);
		rt5033_clr_bits(iic, RT5033_CHG_STAT_CTRL, RT5033_CHGENB_MASK);
		rt5033_clr_bits(iic, RT5033_CHG_CTRL1, RT5033_HZ_MASK);
		rt5033_set_bits(iic, RT5033_CHG_CTRL3, RT5033_COF_EN_MASK);
#ifdef CONFIG_FLED_RT5033
        if (charger->fled_info) {
            /* turn on again if original status is on*/
            if (fled_mode == FLASHLIGHT_MODE_TORCH ||
                fled_mode == FLASHLIGHT_MODE_MIXED)
                charger->fled_info->hal->fled_set_mode(charger->fled_info,
                                                       fled_mode);
        }
#endif /*CONFIG_FLED_RT5033*/

	} else {
        charger->charging_current = 0;
#ifdef CONFIG_FLED_RT5033
        if (charger->fled_info == NULL)
            charger->fled_info = rt_fled_get_info_by_name(NULL);
        if (charger->fled_info)
        {
            fled_mode = charger->fled_info->hal->fled_get_mode(charger->fled_info);
            if (fled_mode == FLASHLIGHT_MODE_TORCH ||
                fled_mode == FLASHLIGHT_MODE_MIXED) {
                /* turn off led*/
                charger->fled_info->hal->fled_set_mode(charger->fled_info,
                                                       FLASHLIGHT_MODE_OFF);
            }
        }
#endif /* CONFIG_FLED_RT5033 */
		rt5033_set_bits(iic, RT5033_CHG_STAT_CTRL, RT5033_CHGENB_MASK);
#ifdef CONFIG_FLED_RT5033
        if (charger->fled_info) {
            /* turn on again if original status is on*/
            if (fled_mode == FLASHLIGHT_MODE_TORCH ||
                fled_mode == FLASHLIGHT_MODE_MIXED)
                charger->fled_info->hal->fled_set_mode(charger->fled_info,
                                                       fled_mode);
        }
#endif /*CONFIG_FLED_RT5033*/
	}

}

static void rt5033_enable_charging_termination(struct i2c_client *i2c, int onoff)
{
	pr_info("%s:[BATT] Do termination set(%d)\n", __func__, onoff);
	if (onoff)
		rt5033_set_bits(i2c, RT5033_CHG_CTRL1, RT5033_TEEN_MASK);
	else
		rt5033_clr_bits(i2c, RT5033_CHG_CTRL1, RT5033_TEEN_MASK);
}

static int rt5033_input_current_limit[] = {
	100,
	500,
	700,
	900,
	1000,
	1500,
	2000,
};

static int __r5033_current_limit_2_setting(int current_limit)
{
	int i = 0;

	for (i = 0; i < ARRAY_SIZE(rt5033_input_current_limit); i++) {
		if (current_limit <= rt5033_input_current_limit[i])
			return i+1;
	}

	return -EINVAL;
}


static void rt5033_set_input_current_limit(struct i2c_client *i2c,
		int current_limit)
{
	int data = __r5033_current_limit_2_setting(current_limit);

	if (data < 0)
		data = 0;

	rt5033_assign_bits(i2c, RT5033_CHG_CTRL1, RT5033_AICR_LIMIT_MASK,
			(data) << RT5033_AICR_LIMIT_SHIFT);
}

static int rt5033_get_input_current_limit(struct i2c_client *i2c)
{
    int ret;
    ret = rt5033_reg_read(i2c, RT5033_CHG_CTRL1);
    if (ret < 0)
        return ret;
    ret&=RT5033_AICR_LIMIT_MASK;
    ret>>=RT5033_AICR_LIMIT_SHIFT;
    if (ret == 0)
        return 2000 + 1; // no limitation
    return rt5033_input_current_limit[ret - 1];
}

static void rt5033_set_regulation_voltage(struct i2c_client *i2c,
		int float_voltage)
{
	int data;

	if (float_voltage < 3650)
		data = 0;
	else if (float_voltage >= 3650 && float_voltage <= 4400)
		data = (float_voltage - 3650) / 25;
	else
		data = 0x3f;

	rt5033_assign_bits(i2c, RT5033_CHG_CTRL2, RT5033_VOREG_MASK,
			data << RT5033_VOREG_SHIFT);
}

static void __rt5033_set_fast_charging_current(struct i2c_client *i2c,
		int charging_current)
{
	int data;

	if (charging_current < 700)
		data = 0;
	else if (charging_current >= 700 && charging_current <= 2000)
		data = (charging_current-700)/100;
	else
		data = 0xd;

	rt5033_assign_bits(i2c, RT5033_CHG_CTRL5, RT5033_ICHRG_MASK,
			data << RT5033_ICHRG_SHIFT);
}


static int rt5033_eoc_level[] = {
	0,
	150,
	200,
	250,
	300,
	400,
	500,
	600,
};

static int rt5033_get_eoc_level(int eoc_current)
{
	int i;
	for (i = 0; i < ARRAY_SIZE(rt5033_eoc_level); i++) {
		if (eoc_current < rt5033_eoc_level[i]) {
			if (i == 0)
				return 0;
			return i - 1;
		}
	}
	return ARRAY_SIZE(rt5033_eoc_level) - 1;
}

static int rt5033_get_fast_charging_current(struct i2c_client *i2c)
{
	int data = rt5033_reg_read(i2c, RT5033_CHG_CTRL5);
	if (data < 0)
		return data;

	data = (data >> 4) & 0x0f;

	if (data > 0xd)
		data = 0xd;
	return data * 100 + 700;
}

static void __rt5033_set_termination_current_limit(struct i2c_client *i2c,
		int current_limit)
{
	int data = rt5033_get_eoc_level(current_limit);
	int ret;

	pr_info("%s : Set Termination\n", __func__);

	ret = rt5033_assign_bits(i2c, RT5033_CHG_CTRL4, RT5033_IEOC_MASK,
			data << RT5033_IEOC_SHIFT);
}
static void rt5033_set_charging_current(struct rt5033_charger_data *charger,
                                        int charging_current, int eoc)
{
    charger->charging_current = charging_current;
    __rt5033_set_fast_charging_current(charger->rt5033->i2c_client,
                                       charging_current);
    __rt5033_set_termination_current_limit(charger->rt5033->i2c_client, eoc);

}
enum {RT5033_MIVR_DISABLE = 0,
    RT5033_MIVR_4200MV,
    RT5033_MIVR_4300MV,
    RT5033_MIVR_4400MV,
    RT5033_MIVR_4500MV,
    RT5033_MIVR_4600MV,
    RT5033_MIVR_4700MV,
    RT5033_MIVR_4800MV,
};

/* Dedicated charger (non-USB) device
 * will use lower MIVR level to get better performance
 */
static void rt5033_set_mivr_level(struct rt5033_charger_data *charger)
{
    int mivr;
    switch (charger->cable_type)
    {
        case POWER_SUPPLY_TYPE_USB ... POWER_SUPPLY_TYPE_USB_ACA:
            mivr = RT5033_MIVR_4800MV;
        break;
        default:
            mivr = RT5033_MIVR_4400MV;
    }
    rt5033_assign_bits(charger->rt5033->i2c_client,
                       RT5033_CHG_CTRL4, RT5033_MIVR_MASK, mivr << RT5033_MIVR_SHIFT);
}

static void rt5033_configure_charger(struct rt5033_charger_data *charger)
{

	int eoc;
	int chg_current;

	pr_info("%s : Set cconfig harging\n", __func__);
	if (charger->charging_current < 0) {
		pr_info("%s : OTG is activated. Ignore command!\n",
				__func__);
		return;
	}

	if (charger->cable_type ==
			POWER_SUPPLY_TYPE_BATTERY) {
		rt5033_enable_charger_switch(charger, 0);
	} else {

		rt5033_enable_charging_termination(charger->rt5033->i2c_client, 1);
        rt5033_set_mivr_level(charger);
		/* Input current limit */
		pr_info("%s : input current (%dmA)\n",
				__func__, charger->pdata->charging_current
				[charger->cable_type].input_current_limit);

		rt5033_set_input_current_limit(charger->rt5033->i2c_client,
				charger->pdata->charging_current
				[charger->cable_type].input_current_limit);

		/* Float voltage */
		pr_info("%s : float voltage (%dmV)\n",
				__func__, charger->pdata->chg_float_voltage);

		rt5033_set_regulation_voltage(charger->rt5033->i2c_client,
				charger->pdata->chg_float_voltage);


        chg_current = charger->pdata->charging_current
				[charger->cable_type].fast_charging_current;
        eoc = charger->pdata->charging_current
                [charger->cable_type].full_check_current_1st;
  		/* Fast charge and Termination current */
		pr_info("%s : fast charging current (%dmA)\n",
				__func__, chg_current);

		pr_info("%s : termination current (%dmA)\n",
				__func__, eoc);

        rt5033_set_charging_current(charger, chg_current, eoc);
		rt5033_enable_charger_switch(charger, 1);
	}
}


/* here is set init charger data */
static bool rt5033_chg_init(struct rt5033_charger_data *charger)
{
    rt5033_clr_bits(charger->rt5033->i2c_client,
                    RT5033_CHG_CTRL3, RT5033_TIMEREN_MASK);
	return true;
}


static int rt5033_get_charging_status(struct rt5033_charger_data *charger)
{
	int status = POWER_SUPPLY_STATUS_UNKNOWN;
	int ret;

	ret = rt5033_reg_read(charger->rt5033->i2c_client, RT5033_CHG_STAT_CTRL);
	if (ret<0) {
	}
	if (charger->full_charged)
		return POWER_SUPPLY_STATUS_FULL;

	switch (ret & 0x30) {
	case 0x00:
		status = POWER_SUPPLY_STATUS_DISCHARGING;
		break;
	case 0x20:
		status = POWER_SUPPLY_STATUS_CHARGING;
		break;
	case 0x10:
		status = POWER_SUPPLY_STATUS_FULL;
		break;
	case 0x30:
		status = POWER_SUPPLY_STATUS_NOT_CHARGING;
		break;
	}

	return status;
}

static int rt5033_get_charge_type(struct i2c_client *iic)
{
	int status = POWER_SUPPLY_CHARGE_TYPE_UNKNOWN;
	int ret;

	ret = rt5033_reg_read(iic, RT5033_CHG_STAT_CTRL);
	if (ret<0)
		dev_err(&iic->dev, "%s fail\n", __func__);

	switch (ret&0x40) {
	case 0x40:
		status = POWER_SUPPLY_CHARGE_TYPE_FAST;
		break;
	default:
		/* pre-charge mode */
		status = POWER_SUPPLY_CHARGE_TYPE_TRICKLE;
		break;
	}

	return status;
}

static int rt5033_get_charging_health(struct i2c_client *iic)
{
	int status = POWER_SUPPLY_HEALTH_GOOD;
	return status;
}

static int sec_chg_get_property(struct power_supply *psy,
		enum power_supply_property psp,
		union power_supply_propval *val)
{

    int chg_curr,aicr;
	struct rt5033_charger_data *charger =
		container_of(psy, struct rt5033_charger_data, psy_chg);

	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = charger->charging_current ? 1 : 0;
		break;
	case POWER_SUPPLY_PROP_STATUS:
		val->intval = rt5033_get_charging_status(charger);
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		val->intval = rt5033_get_charging_health(charger->rt5033->i2c_client);
		break;
	case POWER_SUPPLY_PROP_CURRENT_MAX:
        val->intval = 2000;
		break;
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		if (charger->charging_current) {
		    aicr = rt5033_get_input_current_limit(charger->rt5033->i2c_client);
			chg_curr = rt5033_get_fast_charging_current(charger->rt5033->i2c_client);
            val->intval = MINVAL(aicr, chg_curr);
		} else
			val->intval = 0;
		break;
	case POWER_SUPPLY_PROP_CHARGE_TYPE:
		val->intval = rt5033_get_charge_type(charger->rt5033->i2c_client);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int sec_chg_set_property(struct power_supply *psy,
		enum power_supply_property psp,
		const union power_supply_propval *val)
{
	struct rt5033_charger_data *charger =
		container_of(psy, struct rt5033_charger_data, psy_chg);
	int chg_current;

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		charger->status = val->intval;
		break;
		/* val->intval : type */
	case POWER_SUPPLY_PROP_ONLINE:
		charger->cable_type = val->intval;
		charger->full_charged = false;
		if (charger->cable_type  == POWER_SUPPLY_TYPE_BATTERY) {
			pr_info("%s:[BATT] Type Battery\n", __func__);
			rt5033_enable_charger_switch(charger, 0);
		} else {
			pr_info("%s:[BATT] Set charging\n", __func__);
			/* Enable charger */
			rt5033_configure_charger(charger);
		}
		break;
	case POWER_SUPPLY_PROP_CURRENT_NOW:
        //set charging current
		if (charger->is_charging) {
			/* decrease the charging current according to siop level */
			chg_current = charger->charging_current * val->intval / 100;
			rt5033_set_charging_current(charger, chg_current, chg_current);
		}
		break;
    case POWER_SUPPLY_PROP_POWER_NOW:
        rt5033_set_charging_current(charger, val->intval, val->intval);
        break;
	default:
		return -EINVAL;
	}

	return true;
}

ssize_t rt5033_chg_show_attrs(struct device *dev,
		const ptrdiff_t offset, char *buf)
{
	struct power_supply *psy = dev_get_drvdata(dev);
	struct rt5033_charger_data *charger =
		container_of(psy, struct rt5033_charger_data, psy_chg);
	int i = 0;
	char *str = NULL;

	switch (offset) {
	case CHG_REG:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%x\n",
				charger->reg_addr);
		break;
	case CHG_DATA:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%x\n",
				charger->reg_data);
		break;
	case CHG_REGS:
		str = kzalloc(sizeof(char) * 256, GFP_KERNEL);
		if (!str)
			return -ENOMEM;

		rt5033_read_regs(charger->rt5033->i2c_client, str);
		i += scnprintf(buf + i, PAGE_SIZE - i, "%s\n",
				str);

		kfree(str);
		break;
	default:
		i = -EINVAL;
		break;
	}

	return i;
}

ssize_t rt5033_chg_store_attrs(struct device *dev,
		const ptrdiff_t offset,
		const char *buf, size_t count)
{
	struct power_supply *psy = dev_get_drvdata(dev);
	struct rt5033_charger_data *charger =
		container_of(psy, struct rt5033_charger_data, psy_chg);

	int ret = 0;
	int x = 0;
	uint8_t data = 0;

	switch (offset) {
	case CHG_REG:
		if (sscanf(buf, "%x\n", &x) == 1) {
			charger->reg_addr = x;
			data = rt5033_reg_read(charger->rt5033->i2c_client,
					charger->reg_addr);
			charger->reg_data = data;
			dev_dbg(dev, "%s: (read) addr = 0x%x, data = 0x%x\n",
					__func__, charger->reg_addr, charger->reg_data);
			ret = count;
		}
		break;
	case CHG_DATA:
		if (sscanf(buf, "%x\n", &x) == 1) {
			data = (u8)x;

			dev_dbg(dev, "%s: (write) addr = 0x%x, data = 0x%x\n",
					__func__, charger->reg_addr, data);
			ret = rt5033_reg_write(charger->rt5033->i2c_client,
					charger->reg_addr, data);
			if (ret < 0)
			{
				dev_dbg(dev, "I2C write fail Reg0x%x = 0x%x\n",
						(int)charger->reg_addr, (int)data);
			}
			ret = count;
		}
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

struct rt5033_chg_irq_handler
{
	char *name;
	int irq_index;
	irqreturn_t (*handler)(int irq, void *data);
};

#if EN_OVP_IRQ
static irqreturn_t rt5033_chg_vin_ovp_irq_handler(int irq, void *data)
{
	struct rt5033_charger_data *info = data;
	struct regulator *safe_ldo;
	union power_supply_propval value;

	pr_info("%s: OVP, force to shutdown charger\n", __func__);

	rt5033_enable_charger_switch(info, 0);
	safe_ldo = regulator_get(info->rt5033->dev, "rt5033_safe_ldo");

	if (safe_ldo) {
		regulator_force_disable(safe_ldo);
		regulator_put(safe_ldo);
	}

	value.intval = POWER_SUPPLY_HEALTH_OVERVOLTAGE;
	psy_do_property("battery", set,
			POWER_SUPPLY_PROP_HEALTH, value);
	return IRQ_HANDLED;
}
#endif /* EN_OVP_IRQ */

#if EN_IEOC_IRQ
static irqreturn_t rt5033_chg_ieoc_irq_handler(int irq, void *data)
{
	struct rt5033_charger_data *info = data;
	struct i2c_client *iic = info->rt5033->i2c_client;
	pr_info("%s : Full charged\n", __func__);
	info->full_charged = true;
	/* set full charged flag
        until TA/USB unplug event / stop charging by PSY
        / recharging event
    */
    BUG_ON(iic == NULL);

    rt5033_enable_charging_termination(iic, 0);
    /* report EOC event here */

	return IRQ_HANDLED;
}
#endif /* EN_IEOC_IRQ */

#if EN_RECHG_REQ_IRQ
static irqreturn_t rt5033_chg_rechg_request_irq_handler(int irq, void *data)
{
	struct rt5033_charger_data *info = data;
	pr_info("%s: Recharging requesting\n", __func__);

	info->full_charged = false;

    /* report recharge request event here */
	return IRQ_HANDLED;
}
#endif /* EN_RECHG_REQ_IRQ */

#if EN_TR_IRQ
static irqreturn_t rt5033_chg_otp_tr_irq_handler(int irq, void *data)
{
	pr_info("%s : Over temperature : thermal regulation loop active\n",
			__func__);
	// if needs callback, do it here
	return IRQ_HANDLED;
}
#endif

const struct rt5033_chg_irq_handler rt5033_chg_irq_handlers[] = {
#if EN_OVP_IRQ
	{
		.name = "VIN_OVP",
		.handler = rt5033_chg_vin_ovp_irq_handler,
		.irq_index = RT5033_VINOVPI_IRQ,
	},
#endif /* EN_OVP_IRQ */
#if EN_IEOC_IRQ
	{
		.name = "IEOC",
		.handler = rt5033_chg_ieoc_irq_handler,
		.irq_index = RT5033_IEOC_IRQ,
	},
#endif /* EN_IEOC_IRQ */
#if EN_RECHG_REQ_IRQ
	{
		.name = "RECHG_RQUEST",
		.handler = rt5033_chg_rechg_request_irq_handler,
		.irq_index = RT5033_CHRCHGI_IRQ,
	},
#endif /* EN_RECHG_REQ_IRQ*/
#if EN_TR_IRQ
	{
		.name = "OTP ThermalRegulation",
		.handler = rt5033_chg_otp_tr_irq_handler,
		.irq_index = RT5033_CHTREGI_IRQ,
	},
#endif /* EN_TR_IRQ */

#if EN_MIVR_SW_REGULATION
	{
		.name = "MIVR",
		.handler = rt5033_chg_mivr_irq_handler,
		.irq_index = RT5033_CHMIVRI_IRQ,
	},
#endif /* EN_MIVR_SW_REGULATION */
#if EN_BST_IRQ
	{
		.name = "OTG Low Batt",
		.handler = rt5033_chg_otg_fail_irq_handler,
		.irq_index = RT5033_BSTLOWVI_IRQ,
	},
	{
		.name = "OTG Over Load",
		.handler = rt5033_chg_otg_fail_irq_handler,
		.irq_index = RT5033_BSTOLI_IRQ,
	},
	{
		.name = "OTG OVP",
		.handler = rt5033_chg_otg_fail_irq_handler,
		.irq_index = RT5033_BSTVMIDOVP_IRQ,
	},
#endif //EN_BST_IRQ
};

static int register_irq(struct platform_device *pdev,
		struct rt5033_charger_data *info)
{
	int irq;
	int i, j;
	int ret;
	const struct rt5033_chg_irq_handler *irq_handler = rt5033_chg_irq_handlers;
	const char *irq_name;
	for (i = 0; i < ARRAY_SIZE(rt5033_chg_irq_handlers); i++) {
		irq_name = rt5033_get_irq_name_by_index(irq_handler[i].irq_index);
		irq = platform_get_irq_byname(pdev, irq_name);
		ret = request_threaded_irq(irq, NULL, irq_handler[i].handler,
				IRQF_ONESHOT, irq_name, info);
		if (ret < 0) {
			pr_err("%s : Failed to request IRQ (%s): #%d: %d\n",
					__func__, irq_name, irq, ret);
			goto err_irq;
		}

		pr_info("%s : Register IRQ%d(%s) successfully\n",
				__func__, irq, irq_name);
	}

	return 0;
err_irq:
	for (j = 0; j < i; j++) {
		irq_name = rt5033_get_irq_name_by_index(irq_handler[j].irq_index);
		irq = platform_get_irq_byname(pdev, irq_name);
		free_irq(irq, info);
	}

	return ret;
}

static void unregister_irq(struct platform_device *pdev,
		struct rt5033_charger_data *info)
{
	int irq;
	int i;
	const char *irq_name;
	const struct rt5033_chg_irq_handler *irq_handler = rt5033_chg_irq_handlers;

	for (i = 0; i < ARRAY_SIZE(rt5033_chg_irq_handlers); i++) {
		irq_name = rt5033_get_irq_name_by_index(irq_handler[i].irq_index);
		irq = platform_get_irq_byname(pdev, irq_name);
		free_irq(irq, info);
	}
}

static int __init rt5033_charger_probe(struct platform_device *pdev)
{
	rt5033_mfd_chip_t *chip = dev_get_drvdata(pdev->dev.parent);
	struct rt5033_mfd_platform_data *pdata = dev_get_platdata(chip->dev);
	struct rt5033_charger_data *charger;

	int ret = 0;

	pr_info("%s:[BATT] RT5033 Charger driver probe\n", __func__);

	charger = kzalloc(sizeof(*charger), GFP_KERNEL);
	if (!charger)
		return -ENOMEM;

	charger->rt5033= chip;
	charger->pdata = pdata->charger_data;

	platform_set_drvdata(pdev, charger);

	charger->psy_chg.name           = "sec-charger";
	charger->psy_chg.type           = POWER_SUPPLY_TYPE_UNKNOWN;
	charger->psy_chg.get_property   = sec_chg_get_property;
	charger->psy_chg.set_property   = sec_chg_set_property;
	charger->psy_chg.properties     = sec_charger_props;
	charger->psy_chg.num_properties = ARRAY_SIZE(sec_charger_props);


	rt5033_chg_init(charger);

	ret = power_supply_register(&pdev->dev, &charger->psy_chg);
	if (ret) {
		pr_err("%s: Failed to Register psy_chg\n", __func__);
		goto err_power_supply_register;
	}
	ret = register_irq(pdev, charger);
	if (ret < 0)
        goto err_reg_irq;

	pr_info("%s:[BATT] RT5033 Charger driver Succeed\n", __func__);

	return 0;
err_reg_irq:
    power_supply_unregister(&charger->psy_chg);
err_power_supply_register:
	kfree(charger);

	return ret;
}

static int __exit rt5033_charger_remove(struct platform_device *pdev)
{
	struct rt5033_charger_data *charger =
		platform_get_drvdata(pdev);
    unregister_irq(pdev, charger);
	power_supply_unregister(&charger->psy_chg);
	kfree(charger);

	return 0;
}

#if defined CONFIG_PM
static int rt5033_charger_suspend(struct device *dev)
{
	return 0;
}

static int rt5033_charger_resume(struct device *dev)
{
	return 0;
}
#else
#define rt5033_charger_suspend NULL
#define rt5033_charger_resume NULL
#endif

static void rt5033_charger_shutdown(struct device *dev)
{
	pr_info("%s: RT5033 Charger driver shutdown\n", __func__);
}

static SIMPLE_DEV_PM_OPS(rt5033_charger_pm_ops, rt5033_charger_suspend,
		rt5033_charger_resume);


static struct platform_driver rt5033_charger_driver = {
	.driver		= {
		.name	= "rt5033-charger",
		.owner	= THIS_MODULE,
		.pm 	= &rt5033_charger_pm_ops,
		.shutdown = rt5033_charger_shutdown,
	},
	.probe		= rt5033_charger_probe,
	.remove		= rt5033_charger_remove,
};

static int __init rt5033_charger_init(void)
{
	int ret = 0;

	ret = platform_driver_register(&rt5033_charger_driver);

	return ret;
}
subsys_initcall(rt5033_charger_init);

static void __exit rt5033_charger_exit(void)
{
	platform_driver_unregister(&rt5033_charger_driver);
}
module_exit(rt5033_charger_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Patrick Chang <patrick_chang@richtek.com");
MODULE_DESCRIPTION("Charger driver for RT5033");
