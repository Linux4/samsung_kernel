/*
 *  SM5414_charger.c
 *  SiliconMitus SM5414 Charger Driver
 *
 *  Copyright (C) 2013 SiliconMitus
 *
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#define DEBUG

#include <linux/battery/sec_charger.h>
#include <linux/seq_file.h>
#include <mach/mfp-pxa1088-degas.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>

static int SM5414_i2c_write(struct i2c_client *client,
				int reg, u8 *buf)
{
	int ret;
	ret = i2c_smbus_write_i2c_block_data(client, reg, 1, buf);
	if (ret < 0)
		dev_err(&client->dev, "%s: Error(%d)\n", __func__, ret);

    //printk("%s : reg = 0x%x, buf = 0x%x\n", __func__,reg,buf);

	return ret;
}

static int SM5414_i2c_read(struct i2c_client *client,
				int reg, u8 *buf)
{
	int ret;
	ret = i2c_smbus_read_i2c_block_data(client, reg, 1, buf);
	if (ret < 0)
		dev_err(&client->dev, "%s: Error(%d)\n", __func__, ret);

    //printk("%s : reg = 0x%x, buf = 0x%x\n", __func__,reg,buf);

	return ret;
}

/*
static void SM5414_i2c_write_array(struct i2c_client *client,
				u8 *buf, int size)
{
	int i;
	for (i = 0; i < size; i += 3)
		SM5414_i2c_write(client, (u8) (*(buf + i)), (buf + i) + 1);
}

static void SM5414_set_command(struct i2c_client *client,
				int reg, u8 datum)
{
	int val;
	u8 after_data;

	if (SM5414_i2c_write(client, reg, &datum) < 0)
		dev_err(&client->dev,
			"%s : error!\n", __func__);

	msleep(20);
	val = SM5414_i2c_read(client, reg, &after_data);
	if (val >= 0)
		dev_dbg(&client->dev,
			"%s : reg(0x%02x) 0x%02x => 0x%02x\n",
			__func__, reg, datum, after_data);
	else
		dev_err(&client->dev, "%s : error!\n", __func__);
}
*/

static void SM5414_test_read(struct i2c_client *client)
{
	u8 data = 0;
	u32 addr = 0;

    //0x00~02 are R/C
	for (addr = SM5414_INTMASK1; addr <= SM5414_CHGCTRL5; addr++) {
		SM5414_i2c_read(client, addr, &data);
		dev_info(&client->dev,
			"SM5414 addr : 0x%02x data : 0x%02x\n", addr, data);
	}
}

static void SM5414_read_regs(struct i2c_client *client, char *str)
{
	u8 data = 0;
	u32 addr = 0;

    //0x00~02 are R/C (read and clear)
	for (addr = SM5414_INTMASK1; addr <= SM5414_CHGCTRL5; addr++) {
		SM5414_i2c_read(client, addr, &data);
		sprintf(str+strlen(str), "0x%x, ", data);
	}
}

static int SM5414_abn_status_cond(struct i2c_client *client, u8 int1)
{
	struct sec_charger_info *charger = i2c_get_clientdata(client);
	int ret = 0;
	int nCHG;
	u8 chg_en;
	SM5414_i2c_read(client, SM5414_CTRL, &chg_en);

	if (int1 & SM5414_INT1_VBUSOVP)
	{
		ret = POWER_SUPPLY_STATUS_NOT_CHARGING;
		dev_dbg(&client->dev,"%s : OV detected\n", __func__);
	}
	else if (int1 & SM5414_INT1_VBUSUVLO)//VBUSUVLO
	{
		if (charger->cable_type == POWER_SUPPLY_TYPE_BATTERY)// cable disconnecting
		{
			dev_dbg(&client->dev,"%s : UVLO detected: Cable disconnected (battery)\n", __func__);
			ret = POWER_SUPPLY_STATUS_DISCHARGING;
		}
		else
		{
			dev_dbg(&client->dev,"%s : UVLO detected\n", __func__);
			ret = POWER_SUPPLY_STATUS_NOT_CHARGING;
		}
	}
	else if (int1 & SM5414_INT1_BATOVP){//BATOVP
		dev_dbg(&client->dev,"%s : BATOVP detected\n", __func__);
		ret = POWER_SUPPLY_STATUS_NOT_CHARGING;
	}
	else if (int1 & SM5414_INT1_VBUSLIMIT){
		dev_dbg(&client->dev,"%s : VBUSLIMIT-input current limit detected\n", __func__);
		if (chg_en & CHARGE_EN)
		{
			/*Check for nCHG*/
			nCHG = gpio_get_value(mfp_to_gpio(GPIO020_GPIO_20));
			if (nCHG) {
				ret = POWER_SUPPLY_STATUS_DISCHARGING;
				dev_dbg(&client->dev,"%s : DIS Charging\n", __func__);
			} else {
				ret = POWER_SUPPLY_STATUS_CHARGING;
				dev_dbg(&client->dev,"%s : Charging\n", __func__);
			}
		}
	}

	dev_dbg(&client->dev,"%s : charger->cable_type: %d\n", __func__, charger->cable_type);

	return (int)ret;
}

static int SM5414_abn_health_cond(struct i2c_client *client, u8 int1)
{
	struct sec_charger_info *charger = i2c_get_clientdata(client);
	int ret = 1;

	if (int1 & SM5414_INT1_VBUSOVP)
	{
		ret = POWER_SUPPLY_HEALTH_OVERVOLTAGE;
		dev_dbg(&client->dev,"%s : OV detected\n", __func__);
	}
	else if (int1 & SM5414_INT1_VBUSUVLO)//VBUSUVLO
	{
		if (charger->cable_type == POWER_SUPPLY_TYPE_BATTERY)// cable disconnecting
		{
			dev_dbg(&client->dev,"%s : UVLO detected: Cable disconnected (battery)\n", __func__);
			ret = POWER_SUPPLY_HEALTH_UNKNOWN;
		}
		else
		{
			dev_dbg(&client->dev,"%s : UVLO detected\n", __func__);
			ret = POWER_SUPPLY_HEALTH_UNDERVOLTAGE;
		}
	}
	else if (int1 & SM5414_INT1_BATOVP){//BATOVP
		dev_dbg(&client->dev,"%s : BATOVP detected\n", __func__);
		ret = POWER_SUPPLY_HEALTH_OVERVOLTAGE;
	}
	else if (int1 & SM5414_INT1_VBUSLIMIT){
			dev_dbg(&client->dev,"%s : VBUSLIMIT-input current limit detected\n", __func__);
			ret = POWER_SUPPLY_HEALTH_GOOD;
	}

	dev_dbg(&client->dev,"%s : charger->cable_type: %d\n", __func__, charger->cable_type);

	return (int)ret;
}

static int SM5414_get_charging_status(struct i2c_client *client, u8 int1, u8 int2, u8 int3)
{
	int status = POWER_SUPPLY_STATUS_UNKNOWN;
	struct sec_charger_info *charger = i2c_get_clientdata(client);
	int nCHG;
	u8 chg_en;
	SM5414_i2c_read(client, SM5414_CTRL, &chg_en);

	if((int3 & SM5414_INT3_DISLIMIT)||(int3 & SM5414_INT3_VSYSNG)||
	   (int3 & SM5414_INT3_VSYSOLP))//VSYSNG || VSYSOLP
	{
		status = POWER_SUPPLY_STATUS_DISCHARGING;
		dev_info(&client->dev,"%s : Discharging\n", __func__);
		goto charging_status_end;
	}
	/* At least one charge cycle terminated,
	 * Charge current < Termination Current
	 */
	else if((int2 & SM5414_INT2_DONE) ||(int2 & SM5414_INT2_TOPOFF)||
		(int2 & SM5414_INT2_CHGRSTF))//TOPOFF || CHGRSTF
	{
		status = POWER_SUPPLY_STATUS_FULL;
		charger->is_fullcharged =true;
		dev_info(&client->dev,"%s : Power Supply Full[%d]\n", __func__,
			charger->is_fullcharged);

		if(int2 & SM5414_INT2_TOPOFF)
		{
			// Disable charger
			chg_en &= ~CHARGE_EN;
			SM5414_i2c_write(client, SM5414_CTRL, &chg_en);
			dev_info(&client->dev, "%s: Charger disabled: Top-Off!! \n", __func__);
		}
		goto charging_status_end;
	}
	else if(int2 & SM5414_INT2_NOBAT)//NOBAT
	{
		status = POWER_SUPPLY_STATUS_NOT_CHARGING;
		dev_info(&client->dev,"%s : No Battery\n", __func__);
		goto charging_status_end;
	}
	else if((int3 & SM5414_INT3_VSYSOK)||(int1 & SM5414_INT1_VBUSINOK))// VSYSOK || VBUSINOK
	{
		status = POWER_SUPPLY_STATUS_CHARGING;
//		dev_dbg(&client->dev,"%s : Charging\n", __func__);
		goto charging_status_end;
	}
	else if(int1 & 0xDF)//abnormal condition : 1101 1111
    {
		dev_dbg(&client->dev,"%s : Abnormal Condition\n", __func__);
		if (int1 & SM5414_INT1_AICL)//AICL
		{
			dev_dbg(&client->dev,"%s : AICL detected\n", __func__);
		}
		else
		{
			status = SM5414_abn_status_cond(client, int1);
		}
		goto charging_status_end;
	}

	/* If charger enabled */
	if (chg_en & CHARGE_EN)
	{
		/*Check for nCHG*/
		nCHG = gpio_get_value(mfp_to_gpio(GPIO020_GPIO_20));
		if (nCHG) {
			status = POWER_SUPPLY_STATUS_DISCHARGING;
			dev_dbg(&client->dev,"%s : DIS Charging\n", __func__);
			goto charging_status_end;
		} else {
			status = POWER_SUPPLY_STATUS_CHARGING;
//			dev_dbg(&client->dev,"%s : Charging\n", __func__);
			goto charging_status_end;
		}
	}
	else
	{
		status = POWER_SUPPLY_STATUS_DISCHARGING;
//		dev_dbg(&client->dev,"%s : Discharging\n", __func__);
		goto charging_status_end;
	}

	//dev_dbg(&client->dev,"%s : charging status: %d\n", __func__, status);
charging_status_end:
//	SM5414_test_read(client);
	charger->sm5414_chg_inf.int_1_s = 0;
	charger->sm5414_chg_inf.int_2_s = 0;
	charger->sm5414_chg_inf.int_3_s = 0;
	return (int)status;
}

static int SM5414_get_charging_health(struct i2c_client *client, u8 int1, u8 int2, u8 int3)
{
	int health = POWER_SUPPLY_HEALTH_GOOD;
	struct sec_charger_info *charger = i2c_get_clientdata(client);
    if((int2 & SM5414_INT2_DONE)||(int2 & SM5414_INT2_TOPOFF)||//DONE || TOPOFF
		(int2 & SM5414_INT2_CHGRSTF)||(int3 & SM5414_INT3_VSYSOK)||//CHRGSTF || VSYSOK
		(int1 & SM5414_INT1_VBUSINOK))//VBUSINOK
    {
		health = POWER_SUPPLY_HEALTH_GOOD;
		dev_dbg(&client->dev,"%s : Good health\n", __func__);
	}
	else if((int3 & SM5414_INT3_VSYSNG)||(int3 & SM5414_INT3_VSYSOLP))//VSYSNG || VSYSOLP
	{
		health = POWER_SUPPLY_HEALTH_UNDERVOLTAGE;
		dev_dbg(&client->dev,"%s : Undervoltage\n", __func__);
	}
	else if(int1 & 0xDF)//abnormal condition : 1101 1111
	{
		dev_dbg(&client->dev,"%s : Abnormal Condition\n", __func__);
		if (int1 & SM5414_INT1_AICL)//AICL
		{
			dev_dbg(&client->dev,"%s : AICL detected\n", __func__);
		}
		else
		{
			health = SM5414_abn_health_cond(client, int1);
		}
	}

//	dev_dbg(&client->dev,"%s : health: %d\n", __func__, health);
	charger->sm5414_chg_inf.int_1_h = 0;
	charger->sm5414_chg_inf.int_2_h = 0;
	charger->sm5414_chg_inf.int_3_h = 0;
	return (int)health;
}


/*	SM5414 uses nINT pin to indicate device status change, need to check that
	through GPIO since interrupt registers are read/clear		*/
static int SM5414_get_stat_hlth(struct i2c_client *client, int check)
{
	int stat_is_chg = 1;
	int stat_is_hlth = 2;
	int ninterrupt_pres;
	u8 int1 = 0, int2 = 0, int3 = 0;
	int health, status;
	struct sec_charger_info *charger = i2c_get_clientdata(client);

	ninterrupt_pres = gpio_get_value(mfp_to_gpio(GPIO008_GPIO_8));

	pr_info("[%s][%d]!!!!!!!!!!!!!!!!!!!!!!!\n",
		__func__, ninterrupt_pres);

	if (!ninterrupt_pres) {
		dev_dbg(&client->dev,"%s : GPIO08 %d\n", __func__, ninterrupt_pres);
		SM5414_i2c_read(client, SM5414_INT1, &int1);
		SM5414_i2c_read(client, SM5414_INT2, &int2);
		SM5414_i2c_read(client, SM5414_INT3, &int3);

		charger->sm5414_chg_inf.int_1_s = int1;
		charger->sm5414_chg_inf.int_2_s = int2;
		charger->sm5414_chg_inf.int_3_s = int3;

		charger->sm5414_chg_inf.int_1_h = int1;
		charger->sm5414_chg_inf.int_2_h = int2;
		charger->sm5414_chg_inf.int_3_h = int3;

		dev_info(&client->dev,
			"%s : Interrupt Register 1 (0x%02x)\n", __func__, int1);
		dev_info(&client->dev,
			"%s : Interrupt Register 2 (0x%02x)\n", __func__, int2);
		dev_info(&client->dev,
			"%s : Interrupt Register 3 (0x%02x)\n", __func__, int3);
	}

	if (check == stat_is_chg) {
		dev_info(&client->dev,
			"%s : Interrupt Register s1 (0x%02x)\n", __func__, charger->sm5414_chg_inf.int_1_s);
		dev_info(&client->dev,
			"%s : Interrupt Register s2 (0x%02x)\n", __func__, charger->sm5414_chg_inf.int_2_s);
		dev_info(&client->dev,
			"%s : Interrupt Register s3 (0x%02x)\n", __func__, charger->sm5414_chg_inf.int_3_s);

		status = SM5414_get_charging_status(client,
			charger->sm5414_chg_inf.int_1_s, charger->sm5414_chg_inf.int_2_s, charger->sm5414_chg_inf.int_3_s);
		return status;
	}
	else if (check == stat_is_hlth) {
/*		dev_dbg(&client->dev,
			"%s : Interrupt Register h1 (0x%02x)\n", __func__, charger->sm5414_chg_inf.int_1_h);
		dev_dbg(&client->dev,
			"%s : Interrupt Register h2 (0x%02x)\n", __func__, charger->sm5414_chg_inf.int_2_h);
		dev_dbg(&client->dev,
			"%s : Interrupt Register h3 (0x%02x)\n", __func__, charger->sm5414_chg_inf.int_3_h);
*/
		health = SM5414_get_charging_health(client,
			charger->sm5414_chg_inf.int_1_h, charger->sm5414_chg_inf.int_2_h, charger->sm5414_chg_inf.int_3_h);
		return health;
	}
	else {
		dev_err(&client->dev, "%s: Error(%d)\n", __func__, check);
	}

return -1;
}

static u8 SM5414_set_float_voltage_data(
	struct i2c_client *client, int float_voltage)
{
	u8 data, float_reg;

	SM5414_i2c_read(client, SM5414_CHGCTRL3, &data);

	data &= BATREG_MASK;

	if (float_voltage < 4100)
		float_voltage = 4100;
    else if (float_voltage > 4475)
        float_voltage = 4475;

	float_reg = (float_voltage - 4100) / 25;
	data |= (float_reg << 4);

	SM5414_i2c_write(client, SM5414_CHGCTRL3, &data);

	SM5414_i2c_read(client, SM5414_CHGCTRL3, &data);
	dev_dbg(&client->dev,
				"%s : SM5414_CHGCTRL3 (float) : 0x%02x\n", __func__, data);

	return data;
}

static u8 SM5414_set_input_current_limit_data(
	struct i2c_client *client, int input_current)
{
	u8 data;

	if(input_current < 100)
		input_current = 100;
	else if (input_current >= 2050)
		input_current = 2050;

    data = (input_current - 100)/50;

    SM5414_i2c_write(client,SM5414_VBUSCTRL,&data);

	SM5414_i2c_read(client, SM5414_VBUSCTRL, &data);
	dev_dbg(&client->dev,
				"%s : SM5414_VBUSCTRL (Input limit) : 0x%02x\n", __func__, data);

	return data;
}

static u8 SM5414_set_topoff_current_limit_data(
	struct i2c_client *client, int topoff_current)
{
	u8 data;
	u8 topoff_reg;

    SM5414_i2c_read(client, SM5414_CHGCTRL4, &data);

    data &= TOPOFF_MASK;

	if(topoff_current < 100)
		topoff_current = 100;
	else if (topoff_current > 650)
		topoff_current = 650;

    topoff_reg = (topoff_current - 100) / 50;
    data = data | (topoff_reg<<3);

    SM5414_i2c_write(client, SM5414_CHGCTRL4, &data);

	SM5414_i2c_read(client, SM5414_CHGCTRL4, &data);
	dev_dbg(&client->dev,
				"%s : SM5414_CHGCTRL4 (Top-off limit) : 0x%02x\n", __func__, data);

	return data;
}

static u8 SM5414_set_fast_charging_current_data(
	struct i2c_client *client, int fast_charging_current)
{
	u8 data = 0;

	if(fast_charging_current < 100)
		fast_charging_current = 100;
	else if (fast_charging_current > 2500)
		fast_charging_current = 2500;

    data = (fast_charging_current - 100) / 50;

	SM5414_i2c_write(client, SM5414_CHGCTRL2, &data);
	SM5414_i2c_read(client, SM5414_CHGCTRL2, &data);

	dev_dbg(&client->dev,
				"%s : SM5414_CHGCTRL2 (fast) : 0x%02x\n", __func__, data);

	return data;
}

static u8 SM5414_set_toggle_charger(struct i2c_client *client, int enable)
{
	u8 chg_en=0;
	u8 data=0;

	SM5414_i2c_read(client, SM5414_CTRL, &chg_en);
	if (enable) {
		if (chg_en & CHARGE_EN) {
			dev_info(&client->dev, "%s: SM5414 already set \n", __func__);
			return chg_en;
		} else {
			chg_en |= CHARGE_EN;
			dev_info(&client->dev, "%s: Charger enabled \n", __func__);
		}
	} else {
		chg_en &= ~CHARGE_EN;
	}

	SM5414_i2c_write(client, SM5414_CTRL, &chg_en);

	dev_info(&client->dev, "%s: SM5414 Charger toggled!! \n", __func__);

	SM5414_i2c_read(client, SM5414_CTRL, &chg_en);
	dev_info(&client->dev,
		"%s : chg_en value(07h register): 0x%02x\n", __func__, chg_en);

	SM5414_i2c_read(client, SM5414_CHGCTRL2, &data);
	dev_info(&client->dev,
				"%s : SM5414_CHGCTRL2 value: 0x%02x", __func__, data);

	return chg_en;
}

static void SM5414_charger_function_control(
				struct i2c_client *client)
{
	struct sec_charger_info *charger = i2c_get_clientdata(client);
//	union power_supply_propval val;
//	int full_check_type;
//	u8 data;

	if (charger->charging_current < 0) {
		dev_info(&client->dev,
			"%s : OTG is activated. Ignore command!\n", __func__);
		return;
	}

	if (charger->cable_type == POWER_SUPPLY_TYPE_BATTERY) {
		/* Disable Charger */
		dev_info(&client->dev,
			"%s : Disable Charger, Battery Supply!\n", __func__);
		// nCHG_EN is logic low so set 1 to disable charger
		charger->is_fullcharged = false;
		gpio_direction_output(mfp_to_gpio(GPIO020_GPIO_20), 1);
		SM5414_set_toggle_charger(client, 0);
	} else {
		dev_info(&client->dev, "%s : float voltage (%dmV)\n",
			__func__, charger->pdata->chg_float_voltage);

		/* Set float voltage */
		SM5414_set_float_voltage_data(
			client, charger->pdata->chg_float_voltage);

		/* Set fast charge current */
		dev_info(&client->dev, "%s : fast charging current (%dmA), siop_level=%d\n",
				__func__, charger->charging_current, charger->pdata->siop_level);
		SM5414_set_fast_charging_current_data(
			client, charger->charging_current);

		if ((charger->pdata->siop_level) &&
			((charger->cable_type == POWER_SUPPLY_TYPE_MAINS)||
			(charger->cable_type == POWER_SUPPLY_TYPE_MISC))){
				charger->charging_current = 50 + charger->pdata->siop_level*10;
					dev_info(&client->dev, "%s : fast charging current (%dmA), siop_level=%d\n",
						__func__, charger->charging_current, charger->pdata->siop_level);
				SM5414_set_fast_charging_current_data(
				client, charger->charging_current);
			}

		dev_info(&client->dev, "%s : topoff current (%dmA)\n",
			__func__, charger->pdata->charging_current[
			charger->cable_type].full_check_current_1st);
		SM5414_set_topoff_current_limit_data(
			client, charger->pdata->charging_current[
				charger->cable_type].full_check_current_1st);

		/* Input current limit */
		dev_info(&client->dev, "%s : input current (%dmA)\n",
			__func__, charger->pdata->charging_current
			[charger->cable_type].input_current_limit);

		SM5414_set_input_current_limit_data(
			client, charger->pdata->charging_current
			[charger->cable_type].input_current_limit);

		/* Enable charging & Termination setting */

		dev_info(&client->dev,
			"%s : Enable Charger!\n", __func__);
		// nCHG_EN is logic low so set 0 to enable charger
		gpio_direction_output(mfp_to_gpio(GPIO020_GPIO_20), 0);
		SM5414_set_toggle_charger(client, 1);
#if 0
		psy_do_property("battery", get,
			POWER_SUPPLY_PROP_CHARGE_NOW, val);
		if (val.intval == SEC_BATTERY_CHARGING_1ST)
			full_check_type = charger->pdata->full_check_type;
		else
			full_check_type = charger->pdata->full_check_type_2nd;
		/* Termination setting */
		switch (full_check_type) {
			case SEC_BATTERY_FULLCHARGED_CHGGPIO:
			case SEC_BATTERY_FULLCHARGED_CHGINT:
			case SEC_BATTERY_FULLCHARGED_CHGPSY:
			/* Enable Current Termination */
			data |= 0x08;
			break;
		}
#endif
	}
}

static void SM5414_charger_otg_control(
				struct i2c_client *client)
{
	struct sec_charger_info *charger = i2c_get_clientdata(client);
	u8 data;
//turn on/off ENBOOST
	if (charger->cable_type ==
		POWER_SUPPLY_TYPE_BATTERY) {
		dev_info(&client->dev, "%s : turn off OTG\n", __func__);
		/* turn off OTG */
		SM5414_i2c_read(client, SM5414_CTRL, &data);
		data &= 0xfe;
		SM5414_i2c_write(client, SM5414_CTRL, &data);
	} else {
		dev_info(&client->dev, "%s : turn on OTG\n", __func__);
		/* turn on OTG */
		SM5414_i2c_read(client, SM5414_CTRL, &data);
		data |= 0x01;
		SM5414_i2c_write(client, SM5414_CTRL, &data);
	}
}

static int SM5414_debugfs_show(struct seq_file *s, void *data)
{
	struct sec_charger_info *charger = s->private;
	u8 reg;
	u8 reg_data;

	seq_printf(s, "SM CHARGER IC :\n");
	seq_printf(s, "==================\n");
	for (reg = SM5414_INTMASK1; reg <= SM5414_CHGCTRL5; reg++) {
		SM5414_i2c_read(charger->client, reg, &reg_data);
		seq_printf(s, "0x%02x:\t0x%02x\n", reg, reg_data);
	}

	seq_printf(s, "\n");
	return 0;
}

static int SM5414_debugfs_open(struct inode *inode, struct file *file)
{
	return single_open(file, SM5414_debugfs_show, inode->i_private);
}

static const struct file_operations SM5414_debugfs_fops = {
	.open           = SM5414_debugfs_open,
	.read           = seq_read,
	.llseek         = seq_lseek,
	.release        = single_release,
};

bool sec_hal_chg_init(struct i2c_client *client)
{
	u8 ctrl = 0;
	u8 reg_data;
	struct sec_charger_info *charger = i2c_get_clientdata(client);

	dev_info(&client->dev, "%s: SM5414 Charger init (Starting)!! \n", __func__);

	// Set encomparator bit
	SM5414_i2c_read(client, SM5414_CTRL, &ctrl);
	ctrl |= 0x40;
	SM5414_i2c_write(client, SM5414_CTRL, &ctrl);

	charger->is_fullcharged = false;

	reg_data = 0x3F;
	SM5414_i2c_write(client, SM5414_INTMASK1, &reg_data);

	reg_data = 0xFE;
	SM5414_i2c_write(client, SM5414_INTMASK2, &reg_data);

	SM5414_i2c_read(client, SM5414_CHGCTRL1, &reg_data);
	reg_data &= ~SM5414_CHGCTRL1_AUTOSTOP;
	SM5414_i2c_write(client, SM5414_CHGCTRL1, &reg_data);

	SM5414_test_read(client);

	(void) debugfs_create_file("SM5414_regs",
		S_IRUGO, NULL, (void *)charger, &SM5414_debugfs_fops);

	return true;
}

bool sec_hal_chg_suspend(struct i2c_client *client)
{
	dev_info(&client->dev,
                "%s: CHARGER - SM5414(suspend mode)!!\n", __func__);

	return true;
}

bool sec_hal_chg_resume(struct i2c_client *client)
{
	dev_info(&client->dev,
                "%s: CHARGER - SM5414(resume mode)!!\n", __func__);

	return true;
}

bool sec_hal_chg_get_property(struct i2c_client *client,
			      enum power_supply_property psp,
			      union power_supply_propval *val)
{
	struct sec_charger_info *charger = i2c_get_clientdata(client);
	u8 data;
	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		if (charger->is_fullcharged)
			val->intval = POWER_SUPPLY_STATUS_FULL;
		else
			val->intval = SM5414_get_stat_hlth(client,1);
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		val->intval = SM5414_get_stat_hlth(client,2);
		break;
	case POWER_SUPPLY_PROP_CHARGE_TYPE:
		val->intval = POWER_SUPPLY_CHARGE_TYPE_UNKNOWN;
				break;
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = POWER_SUPPLY_TYPE_BATTERY;
		break;
	// Have to mention the issue about POWER_SUPPLY_PROP_CHARGE_NOW to Marvel
	case POWER_SUPPLY_PROP_CHARGE_NOW:
	case POWER_SUPPLY_PROP_CURRENT_MAX:
	case POWER_SUPPLY_PROP_CURRENT_AVG:
	case POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN:
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		if (charger->charging_current) {
			SM5414_i2c_read(client, SM5414_VBUSCTRL, &data);
			data &= 0x3f;
			val->intval = (100 + (data * 50));
			if (val->intval < 2050)
				val->intval = (100 + (data * 50));
			/*dev_dbg(&client->dev,
				"%s 1: set-current(%dmA), current now(%dmA)\n",
				__func__, charger->charging_current, val->intval);*/
		} else {
			val->intval = 100;
			/*dev_dbg(&client->dev,
				"%s 2: set-current(%dmA), current now(%dmA)\n",
				__func__, charger->charging_current, val->intval);*/
		}
		break;
	default:
		return false;
	}
	return true;
}

bool sec_hal_chg_set_property(struct i2c_client *client,
			      enum power_supply_property psp,
			      const union power_supply_propval *val)
{
	struct sec_charger_info *charger = i2c_get_clientdata(client);
	switch (psp) {
	/* val->intval : type */
	case POWER_SUPPLY_PROP_ONLINE:
	/* val->intval : charging current */
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		if (charger->charging_current < 0)
			SM5414_charger_otg_control(client);
		else if (charger->charging_current > 0)
			SM5414_charger_function_control(client);
		else {
			SM5414_charger_function_control(client);
			SM5414_charger_otg_control(client);
		}
	    dev_info(&client->dev, "%s: \n", __func__);
		SM5414_test_read(client);
		break;
	default:
		return false;
	}

	return true;
}

ssize_t sec_hal_chg_show_attrs(struct device *dev,
				const ptrdiff_t offset, char *buf)
{
	struct power_supply *psy = dev_get_drvdata(dev);
	struct sec_charger_info *chg =
		container_of(psy, struct sec_charger_info, psy_chg);
	int i = 0;
	char *str = NULL;

	switch (offset) {
/*	case CHG_REG: */
/*		break; */
	case CHG_DATA:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%x\n",
			chg->reg_data);
		break;
	case CHG_REGS:
		str = kzalloc(sizeof(char)*1024, GFP_KERNEL);
		if (!str)
			return -ENOMEM;

		SM5414_read_regs(chg->client, str);
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

ssize_t sec_hal_chg_store_attrs(struct device *dev,
				const ptrdiff_t offset,
				const char *buf, size_t count)
{
	struct power_supply *psy = dev_get_drvdata(dev);
	struct sec_charger_info *chg =
		container_of(psy, struct sec_charger_info, psy_chg);
	int ret = 0;
	int x = 0;
	u8 data = 0;

	switch (offset) {
	case CHG_REG:
		if (sscanf(buf, "%x\n", &x) == 1) {
			chg->reg_addr = x;
			SM5414_i2c_read(chg->client,
				chg->reg_addr, &data);
			chg->reg_data = data;
			dev_dbg(dev, "%s: (read) addr = 0x%x, data = 0x%x\n",
				__func__, chg->reg_addr, chg->reg_data);
			ret = count;
		}
		break;
	case CHG_DATA:
		if (sscanf(buf, "%x\n", &x) == 1) {
			data = (u8)x;
			dev_dbg(dev, "%s: (write) addr = 0x%x, data = 0x%x\n",
				__func__, chg->reg_addr, data);
			SM5414_i2c_write(chg->client,
				chg->reg_addr, &data);
			ret = count;
		}
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}
