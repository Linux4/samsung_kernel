/*
 *  dummy_charger.c
 *  Samsung Dummy Charger Driver
 *
 *  Copyright (C) 2012 Samsung Electronics
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/battery/sec_charger.h>

/*******************************************************************************
* Function Name  : STBCFG01_WriteChargerCnfData
* Description    : utility function to write the charger configuration data
*                  to STBCFG01
* Input          : ref to ChargerData structure
* Return         : error status (OK, !OK)
*******************************************************************************/
static int STBCFG01_WriteChargerCnfData(struct i2c_client *client, STBCFG01_ChargerDataTypeDef *ChargerData)
{
	unsigned char data[5];
	int res;

	data[0] = ChargerData->Cfg1;
	data[1] = ChargerData->Cfg2;
	data[2] = ChargerData->Cfg3;
	data[3] = ChargerData->Cfg4;
	data[4] = ChargerData->Cfg5;

	res = STBCFG01_Write(client, 5, STBCFG01_REG_CHG_CFG1, data);

	return(res);
}

/*******************************************************************************
* Function Name  : BatChg_Read_Ints
* Description    : Reads interrupt latch registers and clears them
* Input          : void
* Return         : word (0x1FFF mask) value or -1 if STBCFG01 not found or I2C error
* Affect         : device registers
*******************************************************************************/
static int STBCFG01_BatChg_Read_Ints(struct i2c_client *client) {
	int res = 0;
	int retval = 0;

	// clear CFG2 bit 7
	ChargerData.Cfg2 &= 0x7F;
	res = STBCFG01_WriteByte(client, STBCFG01_REG_CHG_CFG2, ChargerData.Cfg2);
	if (res<0)
		return(res);

	// set CFG2 bit 7
        ChargerData.Cfg2 |= 0x80;
	res = STBCFG01_WriteByte(client, STBCFG01_REG_CHG_CFG2, ChargerData.Cfg2);
	if (res<0)
		return(res);

	// read status
	retval = STBCFG01_ReadByte(client, STBCFG01_REG_CHG_INT_LATCH1);

	// clear status
	res = STBCFG01_WriteByte(client, STBCFG01_REG_CHG_INT_LATCH1, 0x00);
	if (res<0)
		return(res);

	return retval;
}

/*******************************************************************************
* Function Name  : STBCFG01_ReadChargerData
* Description    : utility function to read the charger data from STBCFG01
* Input          : ref to ChargerData structure
* Return         : error status (OK, !OK)
*******************************************************************************/
static int STBCFG01_ReadChargerData(struct i2c_client *client, STBCFG01_ChargerDataTypeDef *ChargerData)
{
	unsigned char data[9];
	int res;

	/* read STBCFG01 registers 0x90 to 0x98 */
	res=STBCFG01_Read(client, 9, 0x90, data);
	if (res<0)
		return(res);  /* read failed */

	/* fill the charger data structure */
	ChargerData->Cfg1 = data[0];
	ChargerData->Cfg2 = data[1];
	ChargerData->Cfg3 = data[2];
	ChargerData->Cfg4 = data[3];
	ChargerData->Cfg5 = data[4];
	ChargerData->Status1 = data[5];
	ChargerData->Status2 = data[6];

	ChargerData->IntEn1 = data[7];
	ChargerData->IntEn2 = data[8];

	return(OK);
}

/*******************************************************************************
* Function Name  : BatChg_Ints_En
* Description    : Enable interrupts
* Input          : void
* Return         : 0 is ok, -1 if STBCFG01 not found or I2C error
* Affect         : device registers
*******************************************************************************/
static int STBCFG01_BatChg_Ints_En(struct i2c_client *client) {
	int res = 0;

	ChargerData.IntEn1 = 0x00;   // all interrupts disabled
	ChargerData.IntEn2 = 0x00;   // all interrupts disabled

	res = STBCFG01_WriteByte(client, STBCFG01_REG_CHG_INT_EN1, ChargerData.IntEn1);
	if (res<0)
		return(res);

	res = STBCFG01_WriteByte(client, STBCFG01_REG_CHG_INT_EN2, ChargerData.IntEn2);
	if (res<0)
		return(res);

        ChargerData.Cfg2 |= 0x80;
	res = STBCFG01_WriteByte(client, STBCFG01_REG_CHG_CFG2, ChargerData.Cfg2);
	if (res<0)
		return(res);

	return(0);
}

static int STBCFG01_get_health(struct i2c_client *client)
{
	int health = POWER_SUPPLY_HEALTH_GOOD;
	int bc_int_status;

	bc_int_status = STBCFG01_BatChg_Read_Ints(client);

	pr_info("[%s]STATUS = 0x%x\n", __func__, bc_int_status);

	if (bc_int_status & 0x03) {
		bc_int_status = STBCFG01_BatChg_Read_Ints(client);
		pr_info("[%s(1)]STATUS = 0x%x\n", __func__, bc_int_status);
		if (bc_int_status & 0x02) {
			printk("STBCFG01 input-overvoltage condition\n");
			health = POWER_SUPPLY_HEALTH_OVERVOLTAGE;
		} else if (bc_int_status & 0x01) {
			printk("STBCFG01 input-UVLO condition\n");
			health = POWER_SUPPLY_HEALTH_UNDERVOLTAGE;
		}
	}

	return health;
}

int STBCFG01_get_status(void)
{
        int charger_status_3bit;
	int ret;

        charger_status_3bit = (ChargerData.Status1 & 0x38) >> 3;
        switch (charger_status_3bit) {
	case 0:   // idle
		ret = POWER_SUPPLY_STATUS_DISCHARGING;
		break;
	case 1:   // trickle charge
		ret = POWER_SUPPLY_STATUS_CHARGING;
		break;
	case 2:   // pre charge
		ret = POWER_SUPPLY_STATUS_CHARGING;
		break;
	case 3:   // fast charge
		ret = POWER_SUPPLY_STATUS_CHARGING;
		break;
	case 4:   // taper charge (CV phase)
		ret = POWER_SUPPLY_STATUS_CHARGING;
		break;
	case 5:   // charge done
		ret = POWER_SUPPLY_STATUS_FULL;
		break;
	case 6:   // OTG boost on
		ret = POWER_SUPPLY_STATUS_DISCHARGING;
		break;
	case 7:   // shutdown
		ret = POWER_SUPPLY_STATUS_DISCHARGING;
		break;
	default:   // should never happen
		ret = POWER_SUPPLY_STATUS_UNKNOWN;
		break;
        }

	return ret;
}

static int STBCFG01_get_fast_current_limit_data(int fast_charging_current)
{
	int i;
	int limit_current = 550;

	for (i = 0; limit_current <= 1250; i++) {
		if (fast_charging_current <= limit_current) {
			break;
		}
		limit_current += 100;
	}

	return i;
}

static int STBCFG01_get_termination_current_limit_data(int termination_current,
						       union power_supply_propval val)
{
	int i;
	int limit_current = 50;

	if (STBCFG01_get_status() == POWER_SUPPLY_STATUS_FULL)
		gpio_direction_output(ChargerData.GPIO_cd, 1);

	for (i = 0; limit_current <= 300; i++) {
		if (termination_current <= limit_current) {
			break;
		}
		limit_current += 25;
	}

	return i;
}

static int STBCFG01_get_input_current_limit_data(int input_current_limit)
{
	int data;

	if (input_current_limit <= 100)
		data = 0;
	else if (input_current_limit <= 500)
		data = 1;
	else if (input_current_limit <= 800)
		data = 2;
	else if (input_current_limit <= 1200)
		data = 3;
	else
		data = 4;

	return data;
}

static int STBCFG01_get_float_voltage_data(int float_voltage)
{
	int i;
	int base_voltage = 3520;

	for(i = 0; base_voltage <= 4780; i++) {
		if (float_voltage == base_voltage) {
			break;
		} else if (float_voltage < base_voltage) {
			i--;
			break;
		}
		base_voltage += 20;
	}
	return i;
}

/*******************************************************************************
* Function Name  : BatChg_Start
* Description    : Set the Battery Charger registers
* Input          : void
* Return         : 0 is ok, -1 if STBCFG01 not found or I2C error
*******************************************************************************/
static int STBCFG01_BatChg_Start(struct i2c_client *client) {
	int res = 0;

        res = STBCFG01_WriteChargerCnfData(client, &ChargerData);
        if (res!=0)
		return(-1);

        res = STBCFG01_ReadChargerData(client, &ChargerData);
        if (res!=0)
		return(-1);

	res = STBCFG01_BatChg_Read_Ints(client);  // to clear interrupt latch registers before enabling
	if (res<0)
		return(-1);

	res = STBCFG01_BatChg_Ints_En(client);
	if (res<0)
		return(-1);

	return(0);
}

static void STBCFG01_ChargerControl(struct sec_charger_info *charger)
{
	int IFast, ITerm, ILim, VFloat;
	int termination_current;
	int charger_en;
	union power_supply_propval val;

	pr_info("[%s]cable_type = %d\n", __func__, charger->cable_type);

	if (charger->cable_type ==
	    POWER_SUPPLY_TYPE_BATTERY) {
		gpio_direction_output(ChargerData.GPIO_cd, 1);
	} else {
		if((charger->cable_type == POWER_SUPPLY_TYPE_MAINS) ||
		  (charger->cable_type == POWER_SUPPLY_TYPE_USB)) {
			psy_do_property("battery", get,
					POWER_SUPPLY_PROP_CHARGE_NOW, val);
			if (val.intval == SEC_BATTERY_CHARGING_1ST) {
				termination_current =
				charger->pdata->charging_current[
				charger->cable_type].full_check_current_1st;
			} else if (val.intval == SEC_BATTERY_CHARGING_2ND) {
				termination_current =
				charger->pdata->charging_current[
					charger->cable_type].full_check_current_2nd;
			}

			pr_info("[%s]charging_mode = %d\n",
				__func__, val.intval);

			IFast = STBCFG01_get_fast_current_limit_data(
				charger->pdata->charging_current[
				charger->cable_type].fast_charging_current);

			ITerm = STBCFG01_get_termination_current_limit_data(
				termination_current, val);

			ChargerData.Cfg1 = (IFast & 0x07)
				| ((get_battery_data(charger).IPre & 0x01) <<3)
				| ((ITerm & 0x0F) << 4);

			VFloat = STBCFG01_get_float_voltage_data(
				charger->pdata->chg_float_voltage);

			ChargerData.Cfg2 = (VFloat & 0x3F)
				| ((get_battery_data(charger).ARChg & 0x01) << 6)
				& 0x7F;

			ILim = STBCFG01_get_input_current_limit_data(
				charger->pdata->charging_current[
				charger->cable_type].input_current_limit);

			ChargerData.Cfg3 = (ILim & 0x07)
				| ((get_battery_data(charger).DICL_en & 0x01) << 3)
				| ((get_battery_data(charger).VDICL & 0x03) << 4)
				| ((get_battery_data(charger).IBat_lim & 0x03) << 6);

			ChargerData.Cfg4 = (get_battery_data(charger).TPre & 0x01)
				| ((get_battery_data(charger).TFast & 0x01) << 1)
				| (0x01 << 2) | ((get_battery_data(charger).PreB_en & 0x01) << 3)
				| ((get_battery_data(charger).LDO_UVLO_th & 0x01) << 5)
				| ((get_battery_data(charger).LDO_en & 0x01) << 6)
				| ((get_battery_data(charger).WD & 0x01) << 7);

			ChargerData.Cfg5 = ((get_battery_data(charger).FSW & 0x01) << 1)
				| ((get_battery_data(charger).DIChg_adj & 0x01) << 2);

//			STBCFG01_BatChg_Start(charger->client);
			STBCFG01_WriteChargerCnfData(charger->client, &ChargerData);
			STBCFG01_ReadChargerData(charger->client, &ChargerData);

			gpio_direction_output(ChargerData.GPIO_cd, 0);
		}
	}

#ifdef STBCFG01_DEBUG_MESSAGES_EXTRA_REGISTERS
			printk(" Cfg1=%x, Cfg2=%x, Cfg3=%x, Cfg4=%x, Cfg5=%x, Sts1=%x, Sts2=%x, Ien1=%x, Ien2=%x\n",
			       ChargerData.Cfg1, ChargerData.Cfg2, ChargerData.Cfg3,
			       ChargerData.Cfg4, ChargerData.Cfg5, ChargerData.Status1,
			       ChargerData.Status2, ChargerData.IntEn1, ChargerData.IntEn2);
#endif
}



bool sec_hal_chg_init(struct sec_charger_info *charger)
{
	int ret;

	/* init battery charger data structure */
	if (charger->pdata->battery_data)
	{
		ChargerData.GPIO_cd = get_battery_data(charger).GPIO_cd;
		ChargerData.GPIO_shdn = get_battery_data(charger).GPIO_shdn;
	} else {
		ChargerData.GPIO_cd = 0;
		ChargerData.GPIO_shdn = 0;
	}

	if (ChargerData.GPIO_cd) {
		ret = gpio_request(ChargerData.GPIO_cd, "stbcfg01_cd_pin");
		if (ret) {
			dev_err(&charger->client->dev, "STBCFG01 : Unable to get cd pin %d\n",
				ChargerData.GPIO_cd);
			return (ret);
		}
	} else {
		printk("STBCFG01 : CD pin not assigned in platformdata, will not be used\n");
	}

	ChargerData.Cfg1 = (get_battery_data(charger).IFast & 0x07)
		| ((get_battery_data(charger).IPre & 0x01) <<3)
		| ((get_battery_data(charger).ITerm & 0x0F) << 4);

	ChargerData.Cfg2 = (get_battery_data(charger).VFloat & 0x3F)
		| ((get_battery_data(charger).ARChg & 0x01) << 6)
		& 0x7F;

	ChargerData.Cfg3 = (get_battery_data(charger).Iin_lim & 0x07)
		| ((get_battery_data(charger).DICL_en & 0x01) << 3)
		| ((get_battery_data(charger).VDICL & 0x03) << 4)
		| ((get_battery_data(charger).IBat_lim & 0x03) << 6);

	ChargerData.Cfg4 = (get_battery_data(charger).TPre & 0x01)
		| ((get_battery_data(charger).TFast & 0x01) << 1)
		| (0x01 << 2) | ((get_battery_data(charger).PreB_en & 0x01) << 3)
		| ((get_battery_data(charger).LDO_UVLO_th & 0x01) << 5)
		| ((get_battery_data(charger).LDO_en & 0x01) << 6)
		| ((get_battery_data(charger).WD & 0x01) << 7);

	ChargerData.Cfg5 = ((get_battery_data(charger).FSW & 0x01) << 1)
		| ((get_battery_data(charger).DIChg_adj & 0x01) << 2);

	STBCFG01_BatChg_Start(charger->client);

#ifdef STBCFG01_DEBUG_MESSAGES_EXTRA_REGISTERS
			printk(" Cfg1=%x, Cfg2=%x, Cfg3=%x, Cfg4=%x, Cfg5=%x, Sts1=%x, Sts2=%x, Ien1=%x, Ien2=%x\n",
			       ChargerData.Cfg1, ChargerData.Cfg2, ChargerData.Cfg3,
			       ChargerData.Cfg4, ChargerData.Cfg5, ChargerData.Status1,
			       ChargerData.Status2, ChargerData.IntEn1, ChargerData.IntEn2);
#endif

	return true;
}

bool sec_hal_chg_suspend(struct sec_charger_info *charger)
{
	return true;
}

bool sec_hal_chg_resume(struct sec_charger_info *charger)
{
	return true;
}

bool sec_hal_chg_get_property(struct sec_charger_info *charger,
				enum power_supply_property psp,
				union power_supply_propval *val)
{
	u8 data;
	val->intval = 1;
	STBCFG01_ReadChargerData(charger->client, &ChargerData);

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		val->intval = STBCFG01_get_status();
		break;
	case POWER_SUPPLY_PROP_CHARGE_TYPE:
		val->intval = POWER_SUPPLY_CHARGE_TYPE_NONE;
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		val->intval = STBCFG01_get_health(charger->client);
		break;
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = charger->cable_type;
		break;
	case POWER_SUPPLY_PROP_CURRENT_MAX:
	case POWER_SUPPLY_PROP_CURRENT_AVG:
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		val->intval = 1;
		break;
	default:
		return false;
	}
	return true;
}

bool sec_hal_chg_set_property(struct sec_charger_info *charger,
				enum power_supply_property psp,
				const union power_supply_propval *val)
{
	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		STBCFG01_ChargerControl(charger);
		break;
	case POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN:
		break;
	default:
		return false;
	}
	return true;
}


ssize_t sec_hal_chg_show_attrs(struct device *dev,
		const ptrdiff_t offset, char *buf)
{

	int i = 0;

	switch (offset) {
	case CHG_REG:
	case CHG_DATA:
	case CHG_REGS:
	default:
		i = -EINVAL;
		break;
	}

	return i;
}

ssize_t sec_hal_chg_store_attrs(struct device *dev,
		const ptrdiff_t offset,
		const char *buf, size_t count)
{

	int ret = 0;

	switch (offset) {
	case CHG_REG:
	case CHG_DATA:
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}


