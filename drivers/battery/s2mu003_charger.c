/* drivers/battery/s2mu003_charger.c
 * S2MU003 Charger Driver
 *
 * Copyright (C) 2013
 * Author: Patrick Chang <patrick_chang@richtek.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 */

#include <linux/mfd/samsung/s2mu003.h>
#include <linux/mfd/samsung/s2mu003-private.h>
#include <linux/battery/charger/s2mu003_charger.h>
#include <linux/version.h>
#include <linux/leds-s2mu003.h>
#include <linux/i2c.h>

#ifdef CONFIG_USB_HOST_NOTIFY
#include <linux/usb_notify.h>
#endif

#define ENABLE_MIVR 1
#define EN_CHG_WATCHDOG 1

#define EN_OVP_IRQ 1
#define EN_IEOC_IRQ 1
#define EN_TOPOFF_IRQ 1
#define EN_RECHG_REQ_IRQ 0
#define EN_TR_IRQ 0
#define EN_OTGFAIL_IRQ 1
#define EN_MIVR_SW_REGULATION 0
#define EN_BSTINL_IRQ 1
#define EN_BST_IRQ 0
#define MINVAL(a, b) ((a <= b) ? a : b)

#define EOC_DEBOUNCE_CNT 2
#define HEALTH_DEBOUNCE_CNT 3
#define DEFAULT_CHARGING_CURRENT 500

#define EOC_SLEEP 200
#define EOC_TIMEOUT (EOC_SLEEP * 6)
#ifndef EN_TEST_READ
#define EN_TEST_READ 0
#endif

static int s2mu003_reg_map[] = {
	S2MU003_CHG_STATUS1,
	S2MU003_CHG_CTRL1,
	S2MU003_CHG_CTRL2,
	S2MU003_CHG_CTRL3,
	S2MU003_CHG_CTRL4,
	S2MU003_CHG_CTRL5,
	S2MU003_SOFTRESET,
	S2MU003_CHG_CTRL6,
	S2MU003_CHG_CTRL7,
	S2MU003_CHG_CTRL8,
	S2MU003_CHG_STATUS2,
	S2MU003_CHG_STATUS3,
	S2MU003_CHG_STATUS4,
	S2MU003_CHG_CTRL9,
};

unsigned int swelling_charging_current = 0;

struct s2mu003_charger_data {
	struct i2c_client       *client;
	s2mu003_mfd_chip_t	*s2mu003;
	struct delayed_work	charger_work;
	struct workqueue_struct *charger_wqueue;
	struct power_supply	psy_chg;
	struct power_supply	psy_otg;

	s2mu003_charger_platform_data_t *pdata;
	int dev_id;
	int charging_current;
	int siop_level;
	int cable_type;
	bool is_charging;
	struct mutex io_lock;

	/* register programming */
	int reg_addr;
	int reg_data;

	bool full_charged;
	bool ovp;
	int unhealth_cnt;
	int status;
};

static enum power_supply_property sec_charger_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_CHARGE_TYPE,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_CURRENT_MAX,
	POWER_SUPPLY_PROP_CURRENT_AVG,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_CHARGE_OTG_CONTROL,
	POWER_SUPPLY_PROP_CHARGING_ENABLED,
	POWER_SUPPLY_PROP_ENERGY_NOW,
	POWER_SUPPLY_HEALTH_WATCHDOG_TIMER_EXPIRE,
	POWER_SUPPLY_PROP_CHARGE_INITIALIZE,
	POWER_SUPPLY_PROP_BOOST_DISABLE,
};

static enum power_supply_property s2mu003_otg_props[] = {
	POWER_SUPPLY_PROP_ONLINE,
};

int otg_enable_flag;

static int s2mu003_get_charging_health(struct s2mu003_charger_data *charger);

static void s2mu003_read_regs(struct i2c_client *i2c, char *str)
{
	u8 data = 0;
	int i = 0;
	for (i = 0; i < ARRAY_SIZE(s2mu003_reg_map); i++) {
		data = s2mu003_reg_read(i2c, s2mu003_reg_map[i]);
		sprintf(str+strlen(str), "0x%02x, ", data);
	}
}

static void s2mu003_test_read(struct i2c_client *i2c)
{
	int data;
	char str[1024] = {0,};
	int i;

	/* S2MU003 REG: 0x00 ~ 0x08 */
	for (i = 0x0; i <= 0x0E; i++) {
		data = s2mu003_reg_read(i2c, i);
		sprintf(str+strlen(str), "0x%02x:0x%02x, ", i, data);
	}

	data = s2mu003_reg_read(i2c, 0x24);
	sprintf(str+strlen(str), "0x%02x:0x%02x, ", 0x24, data);
	
	data = s2mu003_reg_read(i2c, 0x2A);
	sprintf(str+strlen(str), "0x%02x:0x%02x, ", 0x2A, data);

	data = s2mu003_reg_read(i2c, 0x89);
	sprintf(str+strlen(str), "0x%02x:0x%02x, ", 0x89, data);

	data = s2mu003_reg_read(i2c, S2MU003_CHG_IRQ_CTRL1);
	sprintf(str+strlen(str), "0x%02x:0x%02x, ", S2MU003_CHG_IRQ_CTRL1, data);

	data = s2mu003_reg_read(i2c, S2MU003_CHG_IRQ_CTRL2);
	sprintf(str+strlen(str), "0x%02x:0x%02x, ", S2MU003_CHG_IRQ_CTRL2, data);

	data = s2mu003_reg_read(i2c, S2MU003_CHG_IRQ_CTRL3);
	sprintf(str+strlen(str), "0x%02x:0x%02x, ", S2MU003_CHG_IRQ_CTRL3, data);

	pr_info("%s: %s\n", __func__, str);
}

static void s2mu003_charger_otg_control(struct s2mu003_charger_data *charger,
		bool enable)
{
	int ret;

	pr_info("%s: called charger otg control : %s\n", __func__,
			enable ? "on" : "off");

	otg_enable_flag = enable;

	if (!enable) {
		/* turn off OTG */
		s2mu003_clr_bits(charger->client,
				S2MU003_CHG_CTRL8, S2MU003_OTG_EN_MASK);
		s2mu003_set_bits(charger->client,
				S2MU003_CHG_CTRL1, S2MU003_SEL_SWFREQ_MASK);

		s2mu003_clr_bits(charger->client, S2MU003_CHG_CTRL1, S2MU003_OPAMODE_MASK);
	} else {
		/* Set OTG boost vout = 5V, turn on OTG */
		s2mu003_set_bits(charger->client,
				S2MU003_CHG_CTRL1, S2MU003_OPAMODE_MASK);
		s2mu003_clr_bits(charger->client,
				S2MU003_CHG_CTRL1, S2MU003_SEL_SWFREQ_MASK);
		s2mu003_assign_bits(charger->client,
				S2MU003_CHG_CTRL2, S2MU003_VOREG_MASK,
				0x37 << S2MU003_VOREG_SHIFT);
		s2mu003_set_bits(charger->client,
				S2MU003_CHG_CTRL8, S2MU003_OTG_EN_MASK);

		/* Set OTG Max current limit to 900mA */
		ret = s2mu003_reg_read(charger->client, S2MU003_CHG_CTRL7);
		if (!(ret & 0x02))
		{
			dev_info(&charger->client->dev, "%s : OTG Max current needs to be reset : 0x%x\n",
				__func__, ret);
			ret &= 0xFC;
			ret |= 0x02;
			s2mu003_reg_write(charger->client, S2MU003_CHG_CTRL7, ret);
		}

		charger->cable_type = POWER_SUPPLY_TYPE_OTG;
	}
}

static void s2mu003_otg_check_errors(struct s2mu003_charger_data *charger)
{
	int ret = 0;
#ifdef CONFIG_USB_HOST_NOTIFY
	struct otg_notify *o_notify;
	o_notify = get_otg_notify();
#endif

	pr_info("%s: OTG Failed\n", __func__);

	ret = s2mu003_reg_read(charger->client, S2MU003_CHG_IRQ3);
	if (ret & 0x40) {
		pr_info("%s: Boost overcurrent protection : 0x%x\n", __func__, ret);
#ifdef CONFIG_USB_HOST_NOTIFY
		send_otg_notify(o_notify, NOTIFY_EVENT_OVERCURRENT, 0);
#endif
		s2mu003_charger_otg_control(charger, false);
	} else {
		pr_info("%s: OTG Fail reason : 0x%x\n", __func__, ret);
	}
}

/* this function will work well on CHIP_REV = 3 or later */
static void s2mu003_enable_charger_switch(struct s2mu003_charger_data *charger,
		int onoff)
{
	/* charger->is_charging = onoff ? true : false; */
	if (onoff > 0) {
		pr_info("%s: turn on charger\n", __func__);
		s2mu003_clr_bits(charger->client, S2MU003_CHG_CTRL3, S2MU003_CHG_EN_MASK);
		msleep(50);
		s2mu003_set_bits(charger->client, S2MU003_CHG_CTRL3, S2MU003_CHG_EN_MASK);
	} else {
		charger->full_charged = false;
		pr_info("%s: turn off charger\n", __func__);
		s2mu003_clr_bits(charger->client, S2MU003_CHG_CTRL3, S2MU003_CHG_EN_MASK);
	}
}

static void s2mu003_enable_charging_termination(struct i2c_client *i2c,
		int onoff)
{
	pr_info("%s:[BATT] Do termination set(%d)\n", __func__, onoff);
	if (onoff)
		s2mu003_set_bits(i2c, S2MU003_CHG_CTRL1, S2MU003_TEEN_MASK);
	else
		s2mu003_clr_bits(i2c, S2MU003_CHG_CTRL1, S2MU003_TEEN_MASK);
}

static int s2mu003_input_current_limit[] = {
	100,
	500,
	700,
	900,
	1000,
	1500,
	2000,
};

static void s2mu003_set_input_current_limit(struct s2mu003_charger_data *charger,
		int current_limit)
{
	int i = 0, curr_reg = 0, curr_limit;

	for (i = 0; i < ARRAY_SIZE(s2mu003_input_current_limit); i++) {
		if (current_limit <= s2mu003_input_current_limit[i]) {
			curr_reg = i+1;
			break;
		}
	}

	if (charger->dev_id == 0xB) {
		curr_limit = current_limit >= 1500 ? 0xA : 0x0;
		s2mu003_assign_bits(charger->client, 0x92, 0xF, curr_limit);
	}

	if (curr_reg < 0)
		curr_reg = 0;
	mutex_lock(&charger->io_lock);
	s2mu003_assign_bits(charger->client, S2MU003_CHG_CTRL1, S2MU003_AICR_LIMIT_MASK,
			curr_reg << S2MU003_AICR_LIMIT_SHIFT);
	mutex_unlock(&charger->io_lock);
}

static int s2mu003_get_input_current_limit(struct i2c_client *i2c)
{
	int ret;
	ret = s2mu003_reg_read(i2c, S2MU003_CHG_CTRL1);
	if (ret < 0)
		return ret;
	ret &= S2MU003_AICR_LIMIT_MASK;
	ret >>= S2MU003_AICR_LIMIT_SHIFT;
	if (ret == 0)
		return 2000 + 1; /* no limitation */

	return s2mu003_input_current_limit[ret - 1];
}

static void s2mu003_set_regulation_voltage(struct s2mu003_charger_data *charger,
		int float_voltage)
{
	int data;

	if (float_voltage < 3650)
		data = 0;
	else if (float_voltage >= 3650 && float_voltage <= 4375)
		data = (float_voltage - 3650) / 25;
	else
		data = 0x37;

	mutex_lock(&charger->io_lock);
	s2mu003_assign_bits(charger->client,
			S2MU003_CHG_CTRL2, S2MU003_VOREG_MASK,
			data << S2MU003_VOREG_SHIFT);
	mutex_unlock(&charger->io_lock);
}

static void s2mu003_set_fast_charging_current(struct i2c_client *i2c,
		int charging_current)
{
	int data;

	if (charging_current < 700)
		data = 0;
	else if (charging_current >= 700 && charging_current <= 2000)
		data = (charging_current - 700) / 100;
	else
		data = 0xd;

	s2mu003_assign_bits(i2c, S2MU003_CHG_CTRL5, S2MU003_ICHRG_MASK,
			data << S2MU003_ICHRG_SHIFT);
}

static int s2mu003_eoc_level[] = {
	0,
	150,
	200,
	250,
	300,
	400,
	500,
	125,
};

static int s2mu003_get_eoc_level(int eoc_current)
{
	int i;
	/* Workaround for new revision IF-PMIC that supports 125mA EOC */
	if (eoc_current == 125)
		return 7;
	for (i = 0; i < ARRAY_SIZE(s2mu003_eoc_level); i++) {
		if (eoc_current < s2mu003_eoc_level[i]) {
			if (i == 0)
				return 0;
			return i - 1;
		}
	}

	return ARRAY_SIZE(s2mu003_eoc_level) - 1;
}

static int s2mu003_get_current_eoc_setting(struct s2mu003_charger_data *charger)
{
	int ret;
	mutex_lock(&charger->io_lock);
	ret = s2mu003_reg_read(charger->client, S2MU003_CHG_CTRL4);
	mutex_unlock(&charger->io_lock);
	if (ret < 0) {
		pr_info("%s: warning --> fail to read i2c register(%d)\n", __func__, ret);
		return ret;
	}
	return s2mu003_eoc_level[(S2MU003_IEOC_MASK & ret) >> S2MU003_IEOC_SHIFT];
}

static int s2mu003_get_fast_charging_current(struct i2c_client *i2c)
{
	int data = s2mu003_reg_read(i2c, S2MU003_CHG_CTRL5);
	if (data < 0)
		return data;

	data = (data >> 4) & 0x0f;

	if (data > 0xd)
		data = 0xd;
	return data * 100 + 700;
}

static void s2mu003_set_termination_current_limit(struct i2c_client *i2c,
		int current_limit)
{
	int data = s2mu003_get_eoc_level(current_limit);
	int ret;

	pr_info("%s : Set Termination\n", __func__);

	ret = s2mu003_assign_bits(i2c, S2MU003_CHG_CTRL4, S2MU003_IEOC_MASK,
			data << S2MU003_IEOC_SHIFT);
}

/* eoc re set */
static void s2mu003_set_charging_current(struct s2mu003_charger_data *charger,
		int eoc)
{
	union power_supply_propval swelling_state;
	int adj_current = 0;

#if defined(CONFIG_BATTERY_SWELLING)
	psy_do_property("battery", get,
			POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT,
			swelling_state);
	if(swelling_state.intval && charger->charging_current > swelling_charging_current)
		adj_current = swelling_charging_current;
	else
		adj_current = charger->charging_current * charger->siop_level / 100;
#else
	adj_current = charger->charging_current * charger->siop_level / 100;
#endif
	mutex_lock(&charger->io_lock);
	s2mu003_set_fast_charging_current(charger->client,
			adj_current);
	if (eoc) {
		/* set EOC RESET */
		s2mu003_set_termination_current_limit(charger->client, eoc);
	}
	mutex_unlock(&charger->io_lock);
}

enum {
	S2MU003_MIVR_DISABLE = 0,
	S2MU003_MIVR_4200MV,
	S2MU003_MIVR_4300MV,
	S2MU003_MIVR_4400MV,
	S2MU003_MIVR_4500MV,
	S2MU003_MIVR_4600MV,
	S2MU003_MIVR_4700MV,
	S2MU003_MIVR_4800MV,
};

#if ENABLE_MIVR
/* charger input regulation voltage setting */
static void s2mu003_set_mivr_level(struct s2mu003_charger_data *charger)
{
	int mivr = S2MU003_MIVR_4500MV;

	mutex_lock(&charger->io_lock);
	s2mu003_assign_bits(charger->client,
			S2MU003_CHG_CTRL4, S2MU003_MIVR_MASK, mivr << S2MU003_MIVR_SHIFT);
	mutex_unlock(&charger->io_lock);
}
#endif /*ENABLE_MIVR*/

static void s2mu003_configure_charger(struct s2mu003_charger_data *charger)
{

	int eoc;
	union power_supply_propval val;
	union power_supply_propval chg_mode;
	union power_supply_propval swelling_state;

	pr_info("%s : Set config charging\n", __func__);
	if (charger->charging_current < 0) {
		pr_info("%s : OTG is activated. Ignore command!\n",
				__func__);
		return;
	}

#if ENABLE_MIVR
	s2mu003_set_mivr_level(charger);
#endif /*DISABLE_MIVR*/

	msleep(200);
	psy_do_property("battery", get,
			POWER_SUPPLY_PROP_CHARGE_NOW, val);

	/* TEMP_TEST : disable charging current termination for 2nd charging */
	s2mu003_enable_charging_termination(charger->s2mu003->i2c_client, 0);

	/* Input current limit */
	pr_info("%s : input current (%dmA)\n",
			__func__, charger->pdata->charging_current_table
			[charger->cable_type].input_current_limit);

	s2mu003_set_input_current_limit(charger,
			charger->pdata->charging_current_table
			[charger->cable_type].input_current_limit);

	/* Float voltage */
	pr_info("%s : float voltage (%dmV)\n",
			__func__, charger->pdata->chg_float_voltage);

	s2mu003_set_regulation_voltage(charger,
			charger->pdata->chg_float_voltage);

	charger->charging_current = charger->pdata->charging_current_table
		[charger->cable_type].fast_charging_current;

	if (charger->pdata->full_check_type_2nd == SEC_BATTERY_FULLCHARGED_CHGPSY) {
		psy_do_property("battery", get,
				POWER_SUPPLY_PROP_CHARGE_NOW,
				chg_mode);
#if defined(CONFIG_BATTERY_SWELLING)
		psy_do_property("battery", get,
				POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT,
				swelling_state);
#else
		swelling_state.intval = 0;
#endif
		if (chg_mode.intval == SEC_BATTERY_CHARGING_2ND || swelling_state.intval) {
			s2mu003_enable_charger_switch(charger, 0);
			eoc = charger->pdata->charging_current_table
				[charger->cable_type].full_check_current_2nd;
		} else {
			eoc = charger->pdata->charging_current_table
				[charger->cable_type].full_check_current_1st;
		}
	} else {
		eoc = charger->pdata->charging_current_table
			[charger->cable_type].full_check_current_1st;
	}

	/* Fast charge and Termination current */
	pr_info("%s : fast charging current (%dmA)\n",
			__func__, charger->charging_current);

	pr_info("%s : termination current (%dmA)\n",
			__func__, eoc);

	s2mu003_set_charging_current(charger, eoc);
}

static void s2mu003_toggle_watchdog(struct s2mu003_charger_data *charger, bool enable)
{
	int ret = 0;

	if (enable) {
		/* Enable Charger Watchdog (80s, 250mA auto-recharge) */
		s2mu003_reg_write(charger->client, S2MU003_CHG_CTRL6, 0xDD);
		ret = s2mu003_reg_read(charger->client, S2MU003_CHG_CTRL6);
		pr_info("%s: CHG_WDT Enable, S2MU003_CHG_CTRL6 : 0x%x\n",
			__func__, ret);
	} else {
		/* Disable Watchdog */
		s2mu003_reg_write(charger->client, S2MU003_CHG_CTRL6, 0x5D);
		ret = s2mu003_reg_read(charger->client, S2MU003_CHG_CTRL6);
		pr_info("%s: CHG_WDT Disable, S2MU003_CHG_CTRL6 : 0x%x\n",
			__func__, ret);
	}
}

/* here is set init charger data */
#define S2MU003_MRSTB_CTRL 0X47
static bool s2mu003_chg_init(struct s2mu003_charger_data *charger)
{
	int ret = 0, dev_id1, dev_id2, reg_98;

	/* Read Charger IC Dev ID */
	dev_id1 = s2mu003_reg_read(charger->client, 0xA5);
	dev_id2 = s2mu003_reg_read(charger->client, 0xAF);
	charger->dev_id = ((dev_id1 & 0xC0) >> 4) | ((dev_id2 & 0xC0) >> 6);

	dev_info(&charger->client->dev, "%s : DEV ID : 0x%x\n", __func__,
			charger->dev_id);

	/* WO for cable disconnection not occurring during booting */
	ret = s2mu003_reg_read(charger->client, 0x0F);
	if ((ret & 0x20) == 0) {
		pr_info("%s : WO for cable disconnection not occurring during booting\n", __func__);
		ret |= 0x20;
		s2mu003_reg_write(charger->client, 0x0F, ret);

		/* 0x93[6:4] += 2 */
		reg_98 = s2mu003_reg_read(charger->client, 0x98);
		reg_98 &= 0x70; 
		reg_98 = reg_98 >> 4;
		reg_98 += 2;

		if(reg_98 > 7)
			reg_98 = 7;

		reg_98 = reg_98 << 4;

		ret = s2mu003_reg_read(charger->client, 0x98);
		ret &= 0x8F;
		ret |= reg_98;
		s2mu003_reg_write(charger->client, 0x98, ret);

		ret = s2mu003_reg_read(charger->client, 0x98);
		dev_info(&charger->client->dev, "%s : 0x98 : 0x%x\n", __func__,
				ret);
	}

	if (charger->pdata->is_1MHz_switching)
		ret = s2mu003_set_bits(charger->client,
			S2MU003_CHG_CTRL1, S2MU003_SEL_SWFREQ_MASK);
	else
		ret = s2mu003_clr_bits(charger->client,
			S2MU003_CHG_CTRL1, S2MU003_SEL_SWFREQ_MASK);

	ret = s2mu003_set_bits(charger->client, 0x8D, 0x80);

	/* disable ic reset even if battery removed */
	s2mu003_clr_bits(charger->client, 0x8A, 0x80);

	/* Disable Timer function (Charging timeout fault) */
	s2mu003_clr_bits(charger->client,
			S2MU003_CHG_CTRL3, S2MU003_TIMEREN_MASK);

	/* Disable TE */
	s2mu003_enable_charging_termination(charger->client, 0);

	/* EMI improvement , let reg0x18 bit2~5 be 1100*/
	/* s2mu003_assign_bits(charger->s2mu003->i2c_client, 0x18, 0x3C, 0x30); */

	/* MUST set correct regulation voltage first
	 * Before MUIC pass cable type information to charger
	 * charger would be already enabled (default setting)
	 * it might cause EOC event by incorrect regulation voltage */
	s2mu003_set_regulation_voltage(charger,
			charger->pdata->chg_float_voltage);
#if !(ENABLE_MIVR)
	s2mu003_assign_bits(charger->client,
			S2MU003_CHG_CTRL4, S2MU003_MIVR_MASK,
			S2MU003_MIVR_DISABLE << S2MU003_MIVR_SHIFT);
#endif
	/* TOP-OFF debounce time set 256us */
	s2mu003_assign_bits(charger->client, S2MU003_CHG_CTRL2, 0x3, 0x3);

	/* Disable (set 0min TOP OFF Timer) */
	pr_info("%s : Disable Top-off timer\n", __func__);
	ret = s2mu003_reg_read(charger->client, S2MU003_CHG_CTRL7);
	ret &= ~0x1C;
	s2mu003_reg_write(charger->client, S2MU003_CHG_CTRL7, ret);

	s2mu003_reg_write(charger->client, S2MU003_CHG_DONE_DISABLE,
			S2MU003_CHG_DONE_SEL << S2MU003_CHG_DONE_SEL_SHIFT);

	s2mu003_clr_bits(charger->client,
			S2MU003_CHG_CTRL1, S2MU003_EN_CHGT_MASK);

	s2mu003_assign_bits(charger->client, 0x8A, 0x07, 0x03);

	/* Enable Charger Watchdog (80s, 250mA auto-recharge) */
	s2mu003_toggle_watchdog(charger, true);

	/* Set OTG Max current limit to 900mA */
	ret = s2mu003_reg_read(charger->client, S2MU003_CHG_CTRL7);
	ret &= 0xFC;
	ret |= 0x02;
	s2mu003_reg_write(charger->client, S2MU003_CHG_CTRL7, ret);

	return true;
}

static void s2mu003_flash_onoff(struct s2mu003_charger_data *charger, bool enable)
{
	int ret1, ret2;

	ret1 = s2mu003_reg_read(charger->client, 0x24);
	ret2 = s2mu003_reg_read(charger->client, 0x2A);
	ret1 &= 0x40;
	ret2 &= 0x40;

	if (enable) {
		if(!ret1 || !ret2) {
			s2mu003_assign_bits(charger->client, 0x24, 0xCC, 0x48);
			s2mu003_assign_bits(charger->client, 0x2A, 0xCC, 0x48);
			pr_info("%s enable, 0x24[7:6] = 01\n", __func__);
		}
	} else {
		if(ret1 || ret2) {
			s2mu003_assign_bits(charger->client, 0x24, 0xCC, 0x00);
			s2mu003_assign_bits(charger->client, 0x2A, 0xCC, 0x00);
			pr_info("%s disable, 0x24[7:6] = 00\n", __func__);
		}
	}
}

static void s2mu003_charger_initialize(struct s2mu003_charger_data *charger)
{
	int temp, temp1;

	/* MRSTB enable & set 3 sec debounce time */
	temp = s2mu003_reg_read(charger->client, 0x47);
  	temp |= 0x08;
  	temp &= 0xF8;
	s2mu003_reg_write(charger->client, 0x47, temp);

	/* 0. enable ULDO */
	temp = s2mu003_reg_read(charger->client, 0x41);
	temp |= 0x40;
	s2mu003_reg_write(charger->client, 0x41, temp);

	if(charger->dev_id == 0) {
		temp = s2mu003_reg_read(charger->client, 0xAE);
		temp &= 0x0F;
		temp |= 0x10;
		s2mu003_reg_write(charger->client, 0xAE, temp);

		temp = s2mu003_reg_read(charger->client, 0x07);
		temp |= 0x10;
		s2mu003_reg_write(charger->client, 0x07, temp);

		msleep(10);

		temp = s2mu003_reg_read(charger->client, 0xA5);
		temp1 = s2mu003_reg_read(charger->client, 0xAF);
		charger->dev_id = ((temp & 0xC0) >> 4) | ((temp1 & 0xC0) >> 6);
		pr_info("%s: Reload CHARGER ID : 0x%x\n", __func__, charger->dev_id);
	}

	temp = s2mu003_reg_read(charger->client, S2MU003_CHG_STATUS2);
	temp = (temp & 0x08) ? 0 : 1;

	if(!temp) {
		pr_err("%s : battery is Disconnected\n", __func__);
		temp = s2mu003_reg_read(charger->client, 0x8A);
		temp &= 0xF8;
		temp |= 0x01;
		s2mu003_reg_write(charger->client, 0x8A, temp);

		temp = s2mu003_reg_read(charger->client, 0x64);
		temp |= 0x08;
		s2mu003_reg_write(charger->client, 0x64, temp);

		temp = s2mu003_reg_read(charger->client, 0x75);
		temp |= 0x01;
		s2mu003_reg_write(charger->client, 0x75, temp);
	} else {
		temp = s2mu003_reg_read(charger->client, 0x8A);
		temp &= 0xF8;
		temp |= 0x03;
		s2mu003_reg_write(charger->client, 0x8A, temp); /* [2:0] = 0x3 */

		temp = s2mu003_reg_read(charger->client, 0x64);
		temp &= 0xF7;
		s2mu003_reg_write(charger->client, 0x64, temp);

		temp = s2mu003_reg_read(charger->client, 0x75);
		temp &= 0xFE;
		s2mu003_reg_write(charger->client, 0x75, temp);
	}

	temp = s2mu003_reg_read(charger->client, S2MU003_CHG_CTRL1);
	temp1 = s2mu003_reg_read(charger->client, S2MU003_CHG_CTRL5);
	pr_info("%s: chg curr(0x%x), in curr(0x%x)\n",
		__func__, temp & 0xF0, temp1 & 0xF0);

	temp = s2mu003_reg_read(charger->client, 0x8A);
	temp &= ~0x20;
	s2mu003_reg_write(charger->client, 0x8A, temp);

	temp = s2mu003_reg_read(charger->client, 0x96);
	temp |= 0x03;
	s2mu003_reg_write(charger->client, 0x96, temp);   /* [1:0] = 11 */

	temp = s2mu003_reg_read(charger->client, 0x97);
	temp &= 0x0F;
	temp |= 0xE0;
	s2mu003_reg_write(charger->client, 0x97, temp);   /* [7:4] = 1110 */

	temp = s2mu003_reg_read(charger->client, 0x9C);
	temp &= 0xF7;
	s2mu003_reg_write(charger->client, 0x9C, temp);   /* [3] = 0 */

	temp = s2mu003_reg_read(charger->client, 0x8D);
	temp |= 0x80;
	s2mu003_reg_write(charger->client, 0x8D, temp);   /* [7] = 1 */

	temp = s2mu003_reg_read(charger->client, 0x96);
	temp &= 0xF3;
	s2mu003_reg_write(charger->client, 0x96, temp);   /* 0x96[3:2] = 00 Dead time */

	temp = s2mu003_reg_read(charger->client, 0x95);
	temp &= 0xF8;
	s2mu003_reg_write(charger->client, 0x95, temp);   /* 0X95[2:0] = 000 N Slew */

	temp = s2mu003_reg_read(charger->client, 0x8E);
	temp &= 0xF8;
	temp |= 0X03;
	s2mu003_reg_write(charger->client, 0x8E, temp);   /* 0X8E[2:0] = 011 */

	temp = s2mu003_reg_read(charger->client, 0x8C);
	temp &= 0x8F;
	temp |= 0x70;
	s2mu003_reg_write(charger->client, 0x8C, temp);   /* 0x8C[6:4] = 111 P Slew */

	temp = s2mu003_reg_read(charger->client, 0x9B);
	temp |= 0x80;
	s2mu003_reg_write(charger->client, 0x9B, temp);   /* 0x9B[7] = 1 */

	/* disable ic reset even if battery removed */
	temp = s2mu003_reg_read(charger->client, 0x8A);
	temp &= 0x7F;
	s2mu003_reg_write(charger->client, 0x8A, temp);   /* [7] = 0 */

	/* [7] = 1 done function disabled */
	temp = s2mu003_reg_read(charger->client, 0x74);
	temp |= 0x80;
	s2mu003_reg_write(charger->client, 0x74, temp);

	/* Set top-off timer to 0 minutes */
	temp = s2mu003_reg_read(charger->client, S2MU003_CHG_CTRL7);
	temp &= ~0x1C;
	s2mu003_reg_write(charger->client, S2MU003_CHG_CTRL7, temp);

	temp = s2mu003_reg_read(charger->client, S2MU003_CHG_CTRL7);
	pr_info("%s: S2MU003_CHG_CTRL7: 0x%x\n", __func__, temp);

	temp = s2mu003_reg_read(charger->client, 0x02);
	temp |= 0x3;
	s2mu003_reg_write(charger->client, 0x2, temp);    /* 0x2[1:0] = 3 */

	/* Float voltage, otg voltage, eoc de-glitch time */
	s2mu003_reg_write(charger->client, S2MU003_CHG_CTRL2, 0x70);

	/* Timer setting */
	s2mu003_reg_write(charger->client, S2MU003_CHG_CTRL3, 0x78);

	/* Disable Charger Watchdog (set 250mA auto-recharge) */
	s2mu003_reg_write(charger->client, S2MU003_CHG_CTRL6, 0x5D);

	if(charger->is_charging)
		s2mu003_configure_charger(charger);

	s2mu003_chg_init(charger);
	
	dev_info(&charger->client->dev, "%s: Re-initialize Charger completely\n", __func__);
}

static int s2mu003_get_charging_status(struct s2mu003_charger_data *charger)
{
	int status = POWER_SUPPLY_STATUS_UNKNOWN;
	int ret, chg_sts1, chg_sts2;

	chg_sts1 = s2mu003_reg_read(charger->client, S2MU003_CHG_STATUS1);
	if (chg_sts1 < 0) {
		pr_info("Error : can't get charging status (0x%x)\n", chg_sts1);

	}

	dev_info(&charger->client->dev, "%s : S2MU003_CHG_STATUS1 : 0x%x\n", __func__,
			chg_sts1);

	switch (chg_sts1 & 0x30) {
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

	chg_sts2 = s2mu003_reg_read(charger->client, S2MU003_CHG_STATUS2);
	if (chg_sts2 < 0) {
		pr_info("Error : can't get charging status II (%d)\n", chg_sts2);

	}

	if (chg_sts2 & 0x20)
		status = POWER_SUPPLY_STATUS_FULL;

	/* TEMP_TEST : when OTG is enabled(charging_current -1), handle OTG func. */
	if (charger->charging_current < 0) {
		/* For OTG mode, S2MU003 would still report "charging" */
		status = POWER_SUPPLY_STATUS_DISCHARGING;
		ret = s2mu003_reg_read(charger->client, S2MU003_CHG_IRQ3);
		if (ret & 0x80) {
			pr_info("%s: OTG overcurrent in polling \n", __func__);
			s2mu003_charger_otg_control(charger, false);
		}
	}

	return status;
}

static int s2mu003_get_charge_type(struct i2c_client *iic)
{
	int status = POWER_SUPPLY_CHARGE_TYPE_UNKNOWN;
	int ret;

	ret = s2mu003_reg_read(iic, S2MU003_CHG_STATUS1);
	if (ret < 0)
		dev_err(&iic->dev, "%s fail\n", __func__);

	pr_info("%s: S2MU003_CHG_STATUS1 : 0x%x\n", __func__, ret);

	switch (ret&0x20) {
	case 0x20:
		status = POWER_SUPPLY_CHARGE_TYPE_FAST;
		break;
	default:
		status = POWER_SUPPLY_CHARGE_TYPE_NONE;
		break;
	}

	return status;
}

static bool s2mu003_get_batt_present(struct i2c_client *iic)
{
	int ret = s2mu003_reg_read(iic, S2MU003_CHG_STATUS2);
	if (ret < 0)
		return false;

	return (ret & 0x08) ? false : true;
}

static int s2mu003_get_charging_health(struct s2mu003_charger_data *charger)
{
	int ret = s2mu003_reg_read(charger->client, S2MU003_CHG_STATUS1);
	int watchdog = s2mu003_reg_read(charger->client, S2MU003_CHG_CTRL6);

	/* Clear charger watchdog */
	if (charger->is_charging) {
		watchdog &= 0xFC;
		watchdog |= 0x01;
		s2mu003_reg_write(charger->client, S2MU003_CHG_CTRL6, watchdog);

		/* Check IF_PMIC WD status */
		watchdog = s2mu003_reg_read(charger->client, 0x6D);
		if (watchdog & 0x01) {
			/* Watchdog was set, clear WD timer and re-start charger */
			pr_info("%s: WTD enabled, clear, restart charger : 0x%x\n",
				__func__, watchdog);
			watchdog = s2mu003_reg_read(charger->client, S2MU003_CHG_CTRL6);
			watchdog &= 0xFC;
			watchdog |= 0x01;
			s2mu003_reg_write(charger->client, S2MU003_CHG_CTRL6, watchdog);
			return POWER_SUPPLY_HEALTH_WATCHDOG_TIMER_EXPIRE;
		}
	}

	if (ret < 0)
		return POWER_SUPPLY_HEALTH_UNKNOWN;

	if (ret & (0x03 << 2)) {
		charger->ovp = false;
		charger->unhealth_cnt = 0;
		return POWER_SUPPLY_HEALTH_GOOD;
	}
	charger->unhealth_cnt++;
	if (charger->unhealth_cnt < HEALTH_DEBOUNCE_CNT)
		return POWER_SUPPLY_HEALTH_GOOD;

	charger->unhealth_cnt = HEALTH_DEBOUNCE_CNT;
	if (charger->ovp)
		return POWER_SUPPLY_HEALTH_OVERVOLTAGE;
	return POWER_SUPPLY_HEALTH_UNDERVOLTAGE;
}

static int sec_chg_get_property(struct power_supply *psy,
		enum power_supply_property psp,
		union power_supply_propval *val)
{
	int chg_curr, aicr;
	struct s2mu003_charger_data *charger =
		container_of(psy, struct s2mu003_charger_data, psy_chg);

	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = charger->charging_current ? 1 : 0;
		break;
	case POWER_SUPPLY_PROP_STATUS:
		val->intval = s2mu003_get_charging_status(charger);
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		val->intval = s2mu003_get_charging_health(charger);
		s2mu003_test_read(charger->client);
		break;
	case POWER_SUPPLY_PROP_CURRENT_MAX:
		val->intval = s2mu003_get_fast_charging_current(charger->client);
		break;
	case POWER_SUPPLY_PROP_CURRENT_AVG:
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		if (charger->charging_current) {
			aicr = s2mu003_get_input_current_limit(charger->client);
			chg_curr = s2mu003_get_fast_charging_current(charger->client);
			val->intval = MINVAL(aicr, chg_curr);
		} else
			val->intval = 0;
		break;
	case POWER_SUPPLY_PROP_CHARGE_TYPE:
		val->intval = s2mu003_get_charge_type(charger->client);
		break;
#if defined(CONFIG_BATTERY_SWELLING) || defined(CONFIG_BATTERY_SWELLING_SELF_DISCHARGING)
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		val->intval = charger->pdata->chg_float_voltage;
		break;
#endif
	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = s2mu003_get_batt_present(charger->client);
		break;
	case POWER_SUPPLY_PROP_CHARGING_ENABLED:
		val->intval = charger->is_charging;
		break;
	case POWER_SUPPLY_PROP_ENERGY_NOW:
		return -ENODATA;
	default:
		return -EINVAL;
	}

	return 0;
}

static int sec_chg_set_property(struct power_supply *psy,
		enum power_supply_property psp,
		const union power_supply_propval *val)
{
	struct s2mu003_charger_data *charger =
		container_of(psy, struct s2mu003_charger_data, psy_chg);

	int eoc;
	union power_supply_propval value;
	int previous_cable_type = charger->cable_type;
	int watchdog = 0, reg_AE;

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		charger->status = val->intval;
		break;
		/* val->intval : type */
	case POWER_SUPPLY_PROP_ONLINE:
		charger->cable_type = val->intval;

		/* When TA or USB disconnected, set 0xAE[7:4]=1000 */
		reg_AE = s2mu003_reg_read(charger->client, 0xAE);
		reg_AE &= 0x0F;
		reg_AE |= 0x80;
		s2mu003_reg_write(charger->client, 0xAE, reg_AE);

		if (val->intval == POWER_SUPPLY_TYPE_POWER_SHARING) {
			charger->is_charging = false;
			psy_do_property("ps", get,
					POWER_SUPPLY_PROP_STATUS, value);
			pr_info("%s : PS notification: %d \n", __func__, value.intval);
			s2mu003_charger_otg_control(charger, value.intval);
		} else if (charger->cable_type == POWER_SUPPLY_TYPE_BATTERY ||
				charger->cable_type == POWER_SUPPLY_TYPE_UNKNOWN) {
			pr_info("%s:[BATT] Type Battery\n", __func__);

			if (previous_cable_type == POWER_SUPPLY_TYPE_OTG)
				s2mu003_charger_otg_control(charger, false);

			s2mu003_clr_bits(charger->client, 0x8A, 0x20);

			charger->charging_current = charger->pdata->charging_current_table
				[POWER_SUPPLY_TYPE_USB].fast_charging_current;
			s2mu003_set_input_current_limit(charger,
					charger->pdata->charging_current_table
					[POWER_SUPPLY_TYPE_USB].input_current_limit);
			s2mu003_set_charging_current(charger,
					charger->pdata->charging_current_table
					[POWER_SUPPLY_TYPE_USB].full_check_current_1st);

			charger->is_charging = false;
			charger->full_charged = false;
			/* s2mu003_enable_charger_switch(charger, charger->is_charging); */
		} else if (charger->cable_type == POWER_SUPPLY_TYPE_OTG) {
			charger->is_charging = false;
			pr_info("%s: OTG mode\n", __func__);
			s2mu003_charger_otg_control(charger, true);
		} else {
			pr_info("%s:[BATT] Set charging"
				", Cable type = %d\n", __func__, charger->cable_type);
			/* Enable charger */
			s2mu003_set_bits(charger->client, 0x8A, 0x20);
			s2mu003_configure_charger(charger);
			charger->is_charging = true;

			/* When USB/TA connect, 0xAE[7:4]=0000 */
			reg_AE = s2mu003_reg_read(charger->client, 0xAE);
			reg_AE &= 0x0F;
			s2mu003_reg_write(charger->client, 0xAE, reg_AE);
		}

		/* If always enabled concept is removed we need to handle
		 * the charger on/off manually */
		if (!(charger->pdata->always_enable)) {
			s2mu003_enable_charger_switch(charger, charger->is_charging);
		}

		/* Toggle charger watchdog based on device's status / disable when charger is off */
		s2mu003_toggle_watchdog(charger, charger->is_charging);

#if EN_TEST_READ
		msleep(100);
		s2mu003_test_read(charger->s2mu003->i2c_client);
#endif
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		if (val->intval == POWER_SUPPLY_HEALTH_WATCHDOG_TIMER_EXPIRE) {
			watchdog = s2mu003_reg_read(charger->client, S2MU003_CHG_CTRL6);
			watchdog &= 0xFC;
			watchdog |= 0x01;
			s2mu003_reg_write(charger->client, S2MU003_CHG_CTRL6, watchdog);

			/* Check WD status */
			watchdog = s2mu003_reg_read(charger->client, S2MU003_CHG_CTRL6);
			pr_info("%s: Clear WDT  after IRQ, S2MU003_CHG_CTRL6 : 0x%x\n",
				__func__, watchdog);
			/* Re-enable charger IC */
			s2mu003_enable_charger_switch(charger, true);
		}
		break;
	case POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN:
                /* set charging current */
                if (charger->is_charging) {
                        /* decrease the charging current according to siop level */
                        charger->siop_level = val->intval;
                        pr_info("%s:SIOP level = %d, chg current = %d\n", __func__,
                                        val->intval, charger->charging_current);
                        eoc = s2mu003_get_current_eoc_setting(charger);
                        s2mu003_set_charging_current(charger, 0);
                }
		break;
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		/* Slate mode ON case */
		if (val->intval == 0) {
			s2mu003_set_fast_charging_current(charger->client, 0);
			s2mu003_set_input_current_limit(charger, 0);
			s2mu003_enable_charger_switch(charger, 0);
			break;
		} else if (charger->is_charging) {
                        /* decrease the charging current according to siop level */
                        charger->siop_level = val->intval;
                        pr_info("%s:SIOP level = %d, chg current = %d\n", __func__,
                                        val->intval, charger->charging_current);
                        eoc = s2mu003_get_current_eoc_setting(charger);
                        s2mu003_set_charging_current(charger, 0);
                }
		break;
#if defined(CONFIG_BATTERY_SWELLING) || defined(CONFIG_BATTERY_SWELLING_SELF_DISCHARGING)
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		pr_info("%s: float voltage(%d)\n", __func__, val->intval);
		charger->pdata->chg_float_voltage = val->intval;
		s2mu003_set_regulation_voltage(charger,
				charger->pdata->chg_float_voltage);
		break;
#endif
	case POWER_SUPPLY_PROP_CURRENT_AVG:
		swelling_charging_current = val->intval;
		s2mu003_set_fast_charging_current(charger->client, val->intval);
		break;
	case POWER_SUPPLY_PROP_POWER_NOW:
		eoc = s2mu003_get_current_eoc_setting(charger);
		pr_info("%s:Set Power Now -> chg current = %d mA, eoc = %d mA\n", __func__,
				val->intval, eoc);
		s2mu003_set_charging_current(charger, 0);
		break;
	case POWER_SUPPLY_PROP_CHARGE_OTG_CONTROL:
		s2mu003_charger_otg_control(charger, val->intval);
		break;
	case POWER_SUPPLY_PROP_CHARGING_ENABLED:
		/* charger->is_charging = val->intval; */
		s2mu003_enable_charger_switch(charger, val->intval);
		break;
	case POWER_SUPPLY_PROP_CHARGE_INITIALIZE:
		s2mu003_charger_initialize(charger);
		break;
	case POWER_SUPPLY_PROP_BOOST_DISABLE:
		s2mu003_flash_onoff(charger, val->intval);
		break;
	case POWER_SUPPLY_PROP_ENERGY_NOW:
		/* Switch-off charger if JIG is connected */
		if (val->intval) {
			pr_info("%s: JIG Connection status: %d \n", __func__,
				val->intval);
			s2mu003_enable_charger_switch(charger, false);
		}
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

ssize_t s2mu003_chg_show_attrs(struct device *dev,
		const ptrdiff_t offset, char *buf)
{
	struct power_supply *psy = dev_get_drvdata(dev);
	struct s2mu003_charger_data *charger =
		container_of(psy, struct s2mu003_charger_data, psy_chg);
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

		s2mu003_read_regs(charger->client, str);
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

ssize_t s2mu003_chg_store_attrs(struct device *dev,
		const ptrdiff_t offset,
		const char *buf, size_t count)
{
	struct power_supply *psy = dev_get_drvdata(dev);
	struct s2mu003_charger_data *charger =
		container_of(psy, struct s2mu003_charger_data, psy_chg);

	int ret = 0;
	int x = 0;
	uint8_t data = 0;

	switch (offset) {
	case CHG_REG:
		if (sscanf(buf, "%x\n", &x) == 1) {
			charger->reg_addr = x;
			data = s2mu003_reg_read(charger->client,
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
			ret = s2mu003_reg_write(charger->client,
					charger->reg_addr, data);
			if (ret < 0) {
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

struct s2mu003_chg_irq_handler {
	char *name;
	int irq_index;
	irqreturn_t (*handler)(int irq, void *data);
};

#if EN_OVP_IRQ
static void s2mu003_ovp_work(struct work_struct *work)
{
	struct s2mu003_charger_data *charger =
		container_of(work, struct s2mu003_charger_data, charger_work.work);
	union power_supply_propval value;
	int status;

	status = s2mu003_reg_read(charger->client, S2MU003_CHG_STATUS1);

	/* PWR ready = 0*/
	if ((status & (0x04)) == 0) {
		/* No need to disable charger,
		 * H/W will do it automatically */
		charger->unhealth_cnt = HEALTH_DEBOUNCE_CNT;
		charger->ovp = true;
		pr_info("%s: OVP triggered\n", __func__);
		value.intval = POWER_SUPPLY_HEALTH_OVERVOLTAGE;
		psy_do_property("battery", set,
				POWER_SUPPLY_PROP_HEALTH, value);
	} else {
		charger->unhealth_cnt = 0;
		charger->ovp = false;
	}
}

static int s2mu003_led_init(struct i2c_client *i2c)
{
	int ret = 0;
#ifdef CONFIG_S2MU003_LEDS_I2C
	int mask, value;
#endif
	ret = s2mu003_assign_bits(i2c, 0x89, 0x0f, 0x0f);
	if (ret < 0)
		goto out;

	ret = s2mu003_assign_bits(i2c, S2MU003_FLED_CTRL0, 0x87, 0x07);
	if (ret < 0)
		goto out;

	ret = s2mu003_assign_bits(i2c, S2MU003_FLED_CTRL2,
			S2MU003_EN_CHANNEL_SHARE_MASK, 0x80);
	if (ret < 0)
		goto out;

	ret = s2mu003_assign_bits(i2c, S2MU003_FLED_CTRL2,
			S2MU003_BOOST_VOUT_FLASH_MASK, 0x23);
	if (ret < 0)
		goto out;

	ret = s2mu003_assign_bits(i2c, S2MU003_Buck_LDO_CTRL, 0x01, 0x01);
	if (ret < 0)
		goto out;

	ret = s2mu003_assign_bits(i2c, S2MU003_FLED_CH1_CTRL3, 0x80, 0x80);
	if (ret < 0)
		goto out;

	ret = s2mu003_assign_bits(i2c, S2MU003_FLED_CH1_CTRL3,
			S2MU003_TIMEOUT_MAX, S2MU003_FLASH_TIMEOUT_992MS);
	if (ret < 0)
		goto out;

	ret = s2mu003_assign_bits(i2c, S2MU003_FLED_CH1_CTRL2,
			S2MU003_TIMEOUT_MAX, S2MU003_TORCH_TIMEOUT_15728MS);
	if (ret < 0)
		goto out;

#ifdef CONFIG_S2MU003_LEDS_I2C
	value =	S2MU003_FLASH_TORCH_OFF;
	mask = S2MU003_TORCH_ENABLE_MASK | S2MU003_FLASH_ENABLE_MASK;
	ret = s2mu003_assign_bits(i2c, S2MU003_FLED_CH1_CTRL4,
		mask, value);
	if (ret < 0)
		goto out;
#endif
	pr_info("%s : led setup complete\n", __func__);
	return ret;

out:
	pr_err("%s : led setup fail\n", __func__);
	return ret;
}

static irqreturn_t s2mu003_chg_vin_ovp_irq_handler(int irq, void *data)
{
	struct s2mu003_charger_data *charger = data;

	/* Delay 100ms for debounce */
	queue_delayed_work(charger->charger_wqueue, &charger->charger_work, msecs_to_jiffies(100));

	return IRQ_HANDLED;
}
#endif /* EN_OVP_IRQ */

#if EN_IEOC_IRQ
static irqreturn_t s2mu003_chg_ieoc_irq_handler(int irq, void *data)
{
	struct s2mu003_charger_data *charger = data;

	pr_info("%s : Full charged\n", __func__);
	charger->full_charged = true;
	return IRQ_HANDLED;
}
#endif /* EN_IEOC_IRQ */

#if EN_TOPOFF_IRQ
static irqreturn_t s2mu003_chg_toff_irq_handler(int irq, void *data)
{
	/* struct s2mu003_charger_data *charger = data; */

	pr_info("%s: TopOff intr occured\n", __func__);
	/* charger->full_charged = true; */

	return IRQ_HANDLED;
}
#endif /* EN_TOPOFF_IRQ */

#if EN_OTGFAIL_IRQ
static irqreturn_t s2mu003_chg_otgfail_handler(int irq, void *data)
{
	struct s2mu003_charger_data *charger = data;

	s2mu003_otg_check_errors(charger);

	return IRQ_HANDLED;
}
#endif /*EN_OTGFAIL_IRQ*/

static irqreturn_t s2mu003_chg_batp_irq_handler(int irq, void *data)
{
	struct s2mu003_charger_data *charger = data;
	int ret;

	pr_info("%s : battery is Disconnected\n", __func__);

        ret = s2mu003_reg_read(charger->client, S2MU003_CHG_STATUS2);
        if(ret & 0x08) {
		s2mu003_assign_bits(charger->client, 0x8A, 0x07, 0x01);
		s2mu003_clr_bits(charger->client, 0x8A, 0x20);
		s2mu003_enable_charger_switch(charger, 0);
	}

	return IRQ_HANDLED;
}

#if EN_RECHG_REQ_IRQ
static irqreturn_t s2mu003_chg_rechg_request_irq_handler(int irq, void *data)
{
	struct s2mu003_charger_data *charger = data;
	pr_info("%s: Recharging requesting\n", __func__);

	charger->full_charged = false;

	return IRQ_HANDLED;
}
#endif /* EN_RECHG_REQ_IRQ */

#if EN_CHG_WATCHDOG
static irqreturn_t s2mu003_chg_wdt_irq_handler(int irq, void *data)
{
	struct s2mu003_charger_data *charger = data;
	union power_supply_propval value;
	pr_info("%s: Charger Watchdog Timer expired\n", __func__);

	value.intval = POWER_SUPPLY_HEALTH_WATCHDOG_TIMER_EXPIRE;
	psy_do_property(charger->pdata->charger_name, set,
			POWER_SUPPLY_PROP_HEALTH, value);

	return IRQ_HANDLED;
}
#endif /* EN_CHG_WATCHDOG */

#if EN_TR_IRQ
static irqreturn_t s2mu003_chg_otp_tr_irq_handler(int irq, void *data)
{
	pr_info("%s : Over temperature : thermal regulation loop active\n",
			__func__);
	/* if needs callback, do it here */
	return IRQ_HANDLED;
}
#endif

#if EN_BSTINL_IRQ
static irqreturn_t s2mu003_chg_lbp_bst_irq_handler(int irq, void *data)
{
	int temp;
	int intc1m, intc2m, intc3m, intfm;
	struct s2mu003_charger_data *charger = data;
	pr_info("%s: Low battery on boost mode interrupt\n", __func__);

	intc1m = s2mu003_reg_read(charger->client, S2MU003_CHG_INT1M);
	intc2m = s2mu003_reg_read(charger->client, S2MU003_CHG_INT2M);
	intc3m = s2mu003_reg_read(charger->client, S2MU003_CHG_INT3M);
	intfm = s2mu003_reg_read(charger->client, S2MU003_FLED_INTM);

	s2mu003_led_forced_control(0);

	temp = s2mu003_reg_read(charger->client, 0x07);
	temp |= 0x81;
	s2mu003_reg_write_once(charger->client, 0x07, temp);

	temp = s2mu003_reg_read(charger->client, 0x2E);
	pr_info("%s: Charger, Flash LED reset 0x2E:0x%x\n", __func__, temp);
	temp = s2mu003_reg_read(charger->client, 0x02);
	pr_info("%s: 0x02 : 0x%x\n", __func__, temp);

	s2mu003_reg_write(charger->client, S2MU003_CHG_INT1M, intc1m);
	s2mu003_reg_write(charger->client, S2MU003_CHG_INT2M, intc2m);
	s2mu003_reg_write(charger->client, S2MU003_CHG_INT3M, intc3m);
	s2mu003_reg_write(charger->client, S2MU003_FLED_INTM, intfm);

	s2mu003_led_init(charger->client);
	s2mu003_charger_initialize(charger);

	s2mu003_assign_bits(charger->client, 0x24, 0xCC, 0x00);
	s2mu003_assign_bits(charger->client, 0x2A, 0xCC, 0x00);

	s2mu003_led_forced_control(1);

	return IRQ_HANDLED;
}
#endif

const struct s2mu003_chg_irq_handler s2mu003_chg_irq_handlers[] = {
{
		.name = "chg_batp",
		.handler = s2mu003_chg_batp_irq_handler,
		.irq_index = S2MU003_BATP_IRQ,
},
#if EN_OVP_IRQ
	{
		.name = "chg_cinovp",
		.handler = s2mu003_chg_vin_ovp_irq_handler,
		.irq_index = S2MU003_CINOVP_IRQ,
	},
#endif /* EN_OVP_IRQ */
#if EN_IEOC_IRQ
	{
		.name = "chg_eoc",
		.handler = s2mu003_chg_ieoc_irq_handler,
		.irq_index = S2MU003_EOC_IRQ,
	},
#endif /* EN_IEOC_IRQ */
#if EN_RECHG_REQ_IRQ
	{
		.name = "chg_rechg",
		.handler = s2mu003_chg_rechg_request_irq_handler,
		.irq_index = S2MU003_RECHG_IRQ,
	},
#endif /* EN_RECHG_REQ_IRQ*/

#if EN_TOPOFF_IRQ
	{
		.name = "chg_topoff",
		.handler = s2mu003_chg_toff_irq_handler,
		.irq_index = S2MU003_TOPOFF_IRQ,
	},
#endif /* EN_TOPOFF_IRQ */

#if EN_OTGFAIL_IRQ
	{
		.name = "chg_bstilim",
		.handler = s2mu003_chg_otgfail_handler,
		.irq_index = S2MU003_BSTILIM_IRQ,
	},
#endif /* EN_OTGFAIL_IRQ */

#if EN_TR_IRQ
	{
		.name = "chg_chgtr",
		.handler = s2mu003_chg_otp_tr_irq_handler,
		.irq_index = S2MU003_CHGTR_IRQ,
	},
#endif /* EN_TR_IRQ */

#if EN_MIVR_SW_REGULATION
	{
		.name = "chg_chgvinvr",
		.handler = s2mu003_chg_mivr_irq_handler,
		.irq_index = S2MU003_CHGVINVR_IRQ,
	},
#endif /* EN_MIVR_SW_REGULATION */
#if EN_BSTINL_IRQ
	{
		.name = "chg_bstinlv",
		.handler = s2mu003_chg_lbp_bst_irq_handler,
		.irq_index = S2MU003_BSTINLV_IRQ,
	},
#endif
#if EN_BST_IRQ
	{
		.name = "chg_bstilim",
		.handler = s2mu003_chg_otg_fail_irq_handler,
		.irq_index = S2MU003_BSTILIM_IRQ,
	},
#endif /* EN_BST_IRQ */
#if EN_CHG_WATCHDOG
	{
		.name = "pmic_wdt",
		.handler = s2mu003_chg_wdt_irq_handler,
		.irq_index = S2MU003_WDT_IRQ,
	},
#endif /* EN_CHG_WATCHDOG */
};

static int register_irq(struct platform_device *pdev,
		struct s2mu003_charger_data *charger)
{
	int irq;
	int i, j;
	int ret;
	const struct s2mu003_chg_irq_handler *irq_handler = s2mu003_chg_irq_handlers;
	const char *irq_name;
	for (i = 0; i < ARRAY_SIZE(s2mu003_chg_irq_handlers); i++) {
		irq_name = s2mu003_get_irq_name_by_index(irq_handler[i].irq_index);
		irq = platform_get_irq_byname(pdev, irq_name);
		ret = request_threaded_irq(irq, NULL, irq_handler[i].handler,
				IRQF_ONESHOT | IRQF_TRIGGER_RISING |
				IRQF_NO_SUSPEND, irq_name, charger);
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
		irq_name = s2mu003_get_irq_name_by_index(irq_handler[j].irq_index);
		irq = platform_get_irq_byname(pdev, irq_name);
		free_irq(irq, charger);
	}

	return ret;
}

static void unregister_irq(struct platform_device *pdev,
		struct s2mu003_charger_data *charger)
{
	int irq;
	int i;
	const char *irq_name;
	const struct s2mu003_chg_irq_handler *irq_handler = s2mu003_chg_irq_handlers;

	for (i = 0; i < ARRAY_SIZE(s2mu003_chg_irq_handlers); i++) {
		irq_name = s2mu003_get_irq_name_by_index(irq_handler[i].irq_index);
		irq = platform_get_irq_byname(pdev, irq_name);
		free_irq(irq, charger);
	}
}

static int s2mu003_otg_get_property(struct power_supply *psy,
				enum power_supply_property psp,
				union power_supply_propval *val)
{
	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = otg_enable_flag;
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static int s2mu003_otg_set_property(struct power_supply *psy,
				enum power_supply_property psp,
				const union power_supply_propval *val)
{
	struct s2mu003_charger_data *charger =
		container_of(psy, struct s2mu003_charger_data, psy_otg);
	union power_supply_propval value;

	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		value.intval = val->intval;
		pr_info("%s: OTG %s\n", __func__, value.intval > 0 ? "on" : "off");
		psy_do_property(charger->pdata->charger_name, set,
					POWER_SUPPLY_PROP_CHARGE_OTG_CONTROL, value);
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

#ifdef CONFIG_OF
static int s2mu003_charger_parse_dt(struct device *dev,
		struct s2mu003_charger_platform_data *pdata)
{
	struct device_node *np = of_find_node_by_name(NULL, "s2mu003-charger");
	const u32 *p;
	int ret, i, len;

	if (of_find_property(np, "battery,is_1MHz_switching", NULL))
		pdata->is_1MHz_switching = 1;
	pr_info("%s : is_1MHz_switching = %d\n", __func__,
			pdata->is_1MHz_switching);

	if (np == NULL) {
		pr_err("%s np NULL\n", __func__);
	} else {
		ret = of_property_read_u32(np, "battery,chg_float_voltage",
				&pdata->chg_float_voltage);
	}

	np = of_find_node_by_name(NULL, "battery");
	if (!np) {
		pr_err("%s np NULL\n", __func__);
	} else {
		ret = of_property_read_string(np,
			"battery,charger_name", (char const **)&pdata->charger_name);

		ret = of_property_read_u32(np, "battery,full_check_type_2nd",
				&pdata->full_check_type_2nd);
		if (ret)
			pr_info("%s : Full check type 2nd is Empty\n", __func__);

		pdata->always_enable = of_property_read_bool(np,
					"battery,always_enable");

		p = of_get_property(np, "battery,input_current_limit", &len);
		if (!p)
			return 1;

		len = len / sizeof(u32);

		pdata->charging_current_table = kzalloc(sizeof(sec_charging_current_t) * len,
				GFP_KERNEL);

		for (i = 0; i < len; i++) {
			ret = of_property_read_u32_index(np,
					"battery,input_current_limit", i,
					&pdata->charging_current_table[i].input_current_limit);
			ret = of_property_read_u32_index(np,
					"battery,fast_charging_current", i,
					&pdata->charging_current_table[i].fast_charging_current);
			ret = of_property_read_u32_index(np,
					"battery,full_check_current_1st", i,
					&pdata->charging_current_table[i].full_check_current_1st);
			ret = of_property_read_u32_index(np,
					"battery,full_check_current_2nd", i,
					&pdata->charging_current_table[i].full_check_current_2nd);
		}
	}

	dev_info(dev, "s2mu003 charger parse dt retval = %d\n", ret);
	return ret;
}

static struct of_device_id s2mu003_charger_match_table[] = {
	{ .compatible = "samsung,s2mu003-charger",},
	{},
};
#else
static int s2mu003_charger_parse_dt(struct device *dev,
		struct s2mu003_charger_platform_data *pdata)
{
	return -ENOSYS;
}

#define s2mu003_charger_match_table NULL
#endif /* CONFIG_OF */

static int s2mu003_charger_probe(struct platform_device *pdev)
{
	s2mu003_mfd_chip_t *chip = dev_get_drvdata(pdev->dev.parent);
#ifndef CONFIG_OF
	struct s2mu003_mfd_platform_data *mfd_pdata = dev_get_platdata(chip->dev);
#endif
	struct s2mu003_charger_data *charger;
	int ret = 0;

	otg_enable_flag = 0;

	pr_info("%s:[BATT] S2MU003 Charger driver probe\n", __func__);
	charger = kzalloc(sizeof(*charger), GFP_KERNEL);
	if (!charger)
		return -ENOMEM;

	mutex_init(&charger->io_lock);

	charger->s2mu003 = chip;
	charger->client = chip->i2c_client;

#ifdef CONFIG_OF
	charger->pdata = devm_kzalloc(&pdev->dev, sizeof(*(charger->pdata)),
			GFP_KERNEL);
	if (!charger->pdata) {
		dev_err(&pdev->dev, "Failed to allocate memory\n");
		ret = -ENOMEM;
		goto err_parse_dt_nomem;
	}
	ret = s2mu003_charger_parse_dt(&pdev->dev, charger->pdata);
	if (ret < 0)
		goto err_parse_dt;
#else
	charger->pdata = mfd_pdata->charger_platform_data;
#endif

	platform_set_drvdata(pdev, charger);

	if (charger->pdata->charger_name == NULL)
		charger->pdata->charger_name = "sec-charger";

	charger->psy_chg.name           = charger->pdata->charger_name;
	charger->psy_chg.type           = POWER_SUPPLY_TYPE_UNKNOWN;
	charger->psy_chg.get_property   = sec_chg_get_property;
	charger->psy_chg.set_property   = sec_chg_set_property;
	charger->psy_chg.properties     = sec_charger_props;
	charger->psy_chg.num_properties = ARRAY_SIZE(sec_charger_props);
	charger->psy_otg.name				= "otg";
	charger->psy_otg.type				= POWER_SUPPLY_TYPE_OTG;
	charger->psy_otg.get_property		= s2mu003_otg_get_property;
	charger->psy_otg.set_property		= s2mu003_otg_set_property;
	charger->psy_otg.properties		= s2mu003_otg_props;
	charger->psy_otg.num_properties	= ARRAY_SIZE(s2mu003_otg_props);

	charger->siop_level = 100;
	s2mu003_chg_init(charger);

	ret = power_supply_register(&pdev->dev, &charger->psy_chg);
	if (ret) {
		pr_err("%s: Failed to Register psy_chg\n", __func__);
		goto err_power_supply_register;
	}

	ret = power_supply_register(&pdev->dev, &charger->psy_otg);
	if (ret) {
		pr_err("%s: Failed to Register otg_chg\n", __func__);
		goto err_power_supply_register_otg;
	}

	charger->charger_wqueue = create_singlethread_workqueue("charger-wq");
	if (!charger->charger_wqueue) {
		dev_info(chip->dev, "%s: failed to create wq.\n", __func__);
		ret = -ESRCH;
		goto err_create_wq;
	}

	INIT_DELAYED_WORK(&charger->charger_work, s2mu003_ovp_work);

        ret = register_irq(pdev, charger);
        if (ret < 0)
                goto err_reg_irq;

	s2mu003_test_read(charger->client);

	pr_info("%s:[BATT] S2MU003 charger driver loaded OK\n", __func__);

	return 0;
err_reg_irq:
	destroy_workqueue(charger->charger_wqueue);
err_create_wq:
	power_supply_unregister(&charger->psy_otg);
err_power_supply_register_otg:
	power_supply_unregister(&charger->psy_chg);
err_power_supply_register:
err_parse_dt:
err_parse_dt_nomem:
	mutex_destroy(&charger->io_lock);
	kfree(charger);
	return ret;
}

static int s2mu003_charger_remove(struct platform_device *pdev)
{
	struct s2mu003_charger_data *charger =
		platform_get_drvdata(pdev);

	unregister_irq(pdev, charger);
	power_supply_unregister(&charger->psy_chg);
	mutex_destroy(&charger->io_lock);
	kfree(charger);
	return 0;
}

#if defined CONFIG_PM
static int s2mu003_charger_suspend(struct device *dev)
{
	return 0;
}

static int s2mu003_charger_resume(struct device *dev)
{
	return 0;
}
#else
#define s2mu003_charger_suspend NULL
#define s2mu003_charger_resume NULL
#endif

static void s2mu003_charger_shutdown(struct device *dev)
{
	int temp;

	struct s2mu003_charger_data *charger =
		dev_get_drvdata(dev);

	pr_info("%s: S2MU003 Charger driver shutdown\n", __func__);

	/* Boost Mode Disable */
	s2mu003_charger_otg_control(charger, false);

	/* Clear MRSTB to prevent FG reset during power-off/on */
	s2mu003_clr_bits(charger->client, 0x47, 1 << 3);

	if (!(charger->pdata->always_enable)) {
		pr_info("%s: turn on charger\n", __func__);
		s2mu003_clr_bits(charger->client, S2MU003_CHG_CTRL3, S2MU003_CHG_EN_MASK);
		msleep(50);
		s2mu003_set_bits(charger->client, S2MU003_CHG_CTRL3, S2MU003_CHG_EN_MASK);
	}

	temp = s2mu003_reg_read(charger->client, S2MU003_CHG_STATUS);
	pr_info("%s: S2MU003_CHG_STATUS : 0x%x\n", __func__, temp);
	if (temp & 0x08) {
		/* Charger reset : 0x07[7]=1, FLED reset : 0x07[0]=1  */
		pr_info("%s: Charger, FLED reset for work-around\n", __func__);
		temp = s2mu003_reg_read(charger->client, 0x07);
		temp |= 0x81;
		s2mu003_reg_write_once(charger->client, 0x07, temp);

		temp = s2mu003_reg_read(charger->client, 0x2E);
		pr_info("%s: Flash LED/Charger reset 0x2E:0x%x\n", __func__, temp);
		temp = s2mu003_reg_read(charger->client, 0x02);
		pr_info("%s: 0x02 : 0x%x\n", __func__, temp);
	}	
}

static SIMPLE_DEV_PM_OPS(s2mu003_charger_pm_ops, s2mu003_charger_suspend,
		s2mu003_charger_resume);

static struct platform_driver s2mu003_charger_driver = {
	.driver         = {
		.name   = "s2mu003-charger",
		.owner  = THIS_MODULE,
		.of_match_table = s2mu003_charger_match_table,
		.pm     = &s2mu003_charger_pm_ops,
		.shutdown = s2mu003_charger_shutdown,
	},
	.probe          = s2mu003_charger_probe,
	.remove		= s2mu003_charger_remove,
};

static int __init s2mu003_charger_init(void)
{
	int ret = 0;

	ret = platform_driver_register(&s2mu003_charger_driver);

	return ret;
}
subsys_initcall(s2mu003_charger_init);

static void __exit s2mu003_charger_exit(void)
{
	platform_driver_unregister(&s2mu003_charger_driver);
}
module_exit(s2mu003_charger_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Samsung Electronics");
MODULE_DESCRIPTION("Charger driver for S2MU003");
