/* drivers/battery/sm5703_charger.c
 * SM5703 Charger Driver
 *
 * Copyright (C) 2013 Siliconmitus Technology Corp.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 */

#include <linux/battery/sec_charger.h>
#ifdef CONFIG_USB_HOST_NOTIFY
#include <linux/host_notify.h>
#endif

#ifdef CONFIG_SM5703_MUIC
#include <linux/i2c/sm5703-muic.h>
#endif

#include <linux/mfd/sm5703.h> 

//#ifdef CONFIG_FLED_SM5703
#include <linux/leds/sm5703_fled.h>
#include <linux/leds/smfled.h>
#include <linux/mfd/smfled.h>
#include <linux/mfd/sm5703_fled.h>
//#endif /* CONFIG_FLED_SM5703 */
#include <linux/version.h>

#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/of_gpio.h>

#define ENABLE_AICL 1

#define EN_NOBAT_IRQ	1
#define EN_DONE_IRQ 1
#define EN_TOPOFF_IRQ 1
#define EN_CHGON_IRQ 0

#define MINVAL(a, b) ((a <= b) ? a : b)

#ifndef EN_TEST_READ
#define EN_TEST_READ 1
#endif

static int sm5703_reg_map[] = {
	SM5703_INTMSK1,
	SM5703_INTMSK2,
	SM5703_INTMSK3,
	SM5703_INTMSK4,
	SM5703_STATUS1,
	SM5703_STATUS2,
	SM5703_STATUS3,
	SM5703_STATUS4,
	SM5703_CNTL,		
	SM5703_VBUSCNTL,
	SM5703_CHGCNTL1,
	SM5703_CHGCNTL2,
	SM5703_CHGCNTL3,
	SM5703_CHGCNTL4,
	SM5703_CHGCNTL5,
	SM5703_CHGCNTL6,
	SM5703_OTGCURRENTCNTL,
	SM5703_Q3LIMITCNTL,
	SM5703_STATUS5,
};

typedef struct sm5703_charger_data {
	sm5703_mfd_chip_t *sm5703;
	struct power_supply psy_chg;
	sm5703_charger_platform_data_t *pdata;
	int charging_current;
	int siop_level;
	int cable_type;
	bool is_charging;
	struct mutex io_lock;
	/* register programming */
	int reg_addr;
	int reg_data;
	int nchgen;

	bool full_charged;
	bool ovp;
	struct workqueue_struct *wq;
	int status;
#ifdef CONFIG_FLED_SM5703
	struct sm_fled_info *fled_info;
#endif /* CONFIG_FLED_SM5703 */
} sm5703_charger_data_t;

static enum power_supply_property sec_charger_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_CURRENT_MAX,
	POWER_SUPPLY_PROP_CURRENT_AVG,
	POWER_SUPPLY_PROP_CURRENT_NOW,
#if defined(CONFIG_BATTERY_SWELLING) || defined(CONFIG_BATTERY_SWELLING_SELF_DISCHARGING)
	POWER_SUPPLY_PROP_VOLTAGE_MAX,
#endif
};

static int sm5703_get_charging_health(
		struct sm5703_charger_data *charger);
static void sm5703_read_regs(struct i2c_client *i2c, char *str)
{
	u8 data = 0;
	int i = 0;
	for (i = SM5703_INTMSK1; i < ARRAY_SIZE(sm5703_reg_map); i++) {
		data = sm5703_reg_read(i2c, sm5703_reg_map[i]);
		sprintf(str+strlen(str), "0x%02x, ", data);
	}
}

static void sm5703_test_read(struct i2c_client *i2c)
{
	int data;
	char str[1000] = {0,};
	int i;

	for (i = SM5703_INTMSK1; i <= SM5703_CHGCNTL6; i++) { /* SM5703 REG: 0x04 ~ 0x13 */
		data = sm5703_reg_read(i2c, i);
		sprintf(str+strlen(str), "0x%0x = 0x%02x, ", i, data);
	}

	sprintf(str+strlen(str), "0x%0x = 0x%02x, ", SM5703_OTGCURRENTCNTL, sm5703_reg_read(i2c, SM5703_OTGCURRENTCNTL));
	sprintf(str+strlen(str), "0x%0x = 0x%02x, ", SM5703_STATUS5, sm5703_reg_read(i2c, SM5703_STATUS5));
	sprintf(str+strlen(str), "0x%0x = 0x%02x, ", SM5703_Q3LIMITCNTL, sm5703_reg_read(i2c, SM5703_Q3LIMITCNTL));

	pr_info("%s: %s\n", __func__, str);
}

static void sm5703_charger_otg_control(struct sm5703_charger_data *charger,
				       bool enable)
{
	//struct i2c_client *i2c = charger->sm5703->i2c_client;

	pr_info("%s: called charger otg control : %s\n", __func__,
		enable ? "on" : "off");

	if (!enable) {
		/* turn off OTG */
		sm5703_assign_bits(charger->sm5703->i2c_client,
				   SM5703_FLEDCNTL6, SM5703_BSTOUT_MASK,
				   SM5703_BSTOUT_4P5);
        
		//sm5703_assign_bits(charger->sm5703->i2c_client,
		//		   SM5703_CNTL, SM5703_OPERATION_MODE_MASK,
		//		   SM5703_OPERATION_MODE_FLASH_BOOST_MODE);//111->110
	    
#ifdef CONFIG_FLED_SM5703
		if (charger->fled_info == NULL)
			charger->fled_info = sm_fled_get_info_by_name(NULL);
		if (charger->fled_info)
			sm5703_boost_notification(charger->fled_info, 0);
#endif /* CONFIG_FLED_SM5703 */
	} else {
		sm5703_assign_bits(charger->sm5703->i2c_client,
				   SM5703_FLEDCNTL6, SM5703_BSTOUT_MASK,
				   SM5703_BSTOUT_5P1);
        
        //sm5703_assign_bits(charger->sm5703->i2c_client,
		//		   SM5703_CNTL, SM5703_OPERATION_MODE_MASK,
		//		   SM5703_OPERATION_MODE_USB_OTG_MODE);//111
				   
		charger->cable_type = POWER_SUPPLY_TYPE_OTG;
#ifdef CONFIG_FLED_SM5703
		if (charger->fled_info == NULL)
			charger->fled_info = sm_fled_get_info_by_name(NULL);
		if (charger->fled_info)
			sm5703_boost_notification(charger->fled_info, 1);
#endif /* CONFIG_FLED_SM5703 */
	}
}

static void sm5703_enable_charger_switch(struct sm5703_charger_data *charger,
		int onoff)
{
	int prev_charging_status = charger->is_charging;
	//struct i2c_client *iic = charger->sm5703->i2c_client;
	union power_supply_propval val;
	//int prev_operation = SM5703_OPERATION_MODE_CHARGING_ON;
    
	charger->is_charging = onoff ? true : false;
	if ((onoff > 0) && (prev_charging_status == false)) {
		pr_info("%s: turn on charger\n", __func__);

#ifdef CONFIG_FLED_SM5703
		if (charger->fled_info == NULL)
			charger->fled_info = sm_fled_get_info_by_name(NULL);
		if (charger->fled_info)
			sm5703_charger_notification(charger->fled_info,1);
#endif /* CONFIG_FLED_SM5703 */
        charger->nchgen = false;
        gpio_direction_output(charger->pdata->chgen_gpio, charger->nchgen); //nCHG enable
        pr_info("%s : STATUS OF CHARGER ON(0)/OFF(1): %d\n", __func__, charger->nchgen);
		/* Reset EOC loop, and make it re-detect */
	} else if (onoff == 0) {
		psy_do_property("battery", get,
				POWER_SUPPLY_PROP_STATUS, val);
		if (val.intval != POWER_SUPPLY_STATUS_FULL)
			charger->full_charged = false;
		pr_info("%s: turn off charger\n", __func__);

		charger->charging_current = 0;
		charger->nchgen = true;
#ifdef CONFIG_FLED_SM5703
		if (charger->fled_info == NULL)
			charger->fled_info = sm_fled_get_info_by_name(NULL);
		if (charger->fled_info)
			sm5703_charger_notification(charger->fled_info,0);
#endif /* CONFIG_FLED_SM5703 */

        gpio_direction_output(charger->pdata->chgen_gpio, charger->nchgen); //nCHG disable
        pr_info("%s : STATUS OF CHARGER ON(0)/OFF(1): %d\n", __func__, charger->nchgen);

        //After nCHGEN gpio is set to high on the full charge state, operation mode is toggled. 
        //if (val.intval == POWER_SUPPLY_STATUS_FULL)
        //{
        //    prev_operation = sm5703_reg_read(iic, SM5703_CNTL);
        //    prev_operation &= SM5703_OPERATION_MODE_MASK;
        //    sm5703_assign_bits(iic,SM5703_CNTL,SM5703_OPERATION_MODE_MASK, SM5703_OPERATION_MODE_SUSPEND);
        //    sm5703_assign_bits(iic,SM5703_CNTL,SM5703_OPERATION_MODE_MASK, prev_operation);
        //    pr_info("%s : Operation Mode Toggle: %d\n", __func__, prev_operation);            
        //}
        
	} else {
	    pr_info("%s: repeated to set charger switch(%d), prev stat = %d\n",
             __func__, onoff, prev_charging_status ? 1 : 0);
	}
}

//
static void sm5703_enable_autostop(struct sm5703_charger_data *charger,
		int onoff)
{
	struct i2c_client *i2c = charger->sm5703->i2c_client;
	pr_info("%s:[BATT] Autostop set(%d)\n", __func__, onoff);
    
	mutex_lock(&charger->io_lock);
	if (onoff)
		sm5703_set_bits(i2c, SM5703_CHGCNTL4, SM5703_AUTOSTOP_MASK);
	else
		sm5703_clr_bits(i2c, SM5703_CHGCNTL4, SM5703_AUTOSTOP_MASK);
	mutex_unlock(&charger->io_lock);    
}

static void sm5703_enable_autoset(struct sm5703_charger_data *charger,
		int onoff)
{
	struct i2c_client *i2c = charger->sm5703->i2c_client;
	pr_info("%s:[BATT] Autoset set(%d)\n", __func__, onoff);

	mutex_lock(&charger->io_lock);
 	if (onoff)
		sm5703_set_bits(i2c, SM5703_CNTL, SM5703_AUTOSET_MASK);
	else
		sm5703_clr_bits(i2c, SM5703_CNTL, SM5703_AUTOSET_MASK);
	mutex_unlock(&charger->io_lock);
}

static void sm5703_enable_aiclen(struct sm5703_charger_data *charger,
		int onoff)
{
	struct i2c_client *i2c = charger->sm5703->i2c_client;
	pr_info("%s:[BATT] AICLEN set(%d)\n", __func__, onoff);

	mutex_lock(&charger->io_lock);
	if (onoff)
		sm5703_set_bits(i2c, SM5703_CHGCNTL5, SM5703_AICLEN_MASK);
	else
		sm5703_clr_bits(i2c, SM5703_CHGCNTL5, SM5703_AICLEN_MASK);
	mutex_unlock(&charger->io_lock);    
}

static void sm5703_set_aiclth(struct sm5703_charger_data *charger,
		int aiclth)
{
	struct i2c_client *i2c = charger->sm5703->i2c_client;
	int data = 0, temp = 0;

	mutex_lock(&charger->io_lock);
	data = sm5703_reg_read(i2c, SM5703_CHGCNTL5);
	data &= ~SM5703_AICLTH;

	if (aiclth >= 4900)
		aiclth = 4900;

	if(aiclth <= 4300)
		data &= ~SM5703_AICLTH;
	else {
		temp = (aiclth - 4300)/100;
		data |= temp;
	}

	sm5703_reg_write(i2c, SM5703_CHGCNTL5, data);

	data = sm5703_reg_read(i2c, SM5703_CHGCNTL5);
	pr_info("%s : SM5703_CHGCNTL5 (AICHTH) : 0x%02x\n",
		__func__, data);    
	mutex_unlock(&charger->io_lock);
}

static void sm5703_set_freqsel(struct sm5703_charger_data *charger,
		int freqsel_hz)
{
	struct i2c_client *i2c = charger->sm5703->i2c_client;
	int data = 0;

	mutex_lock(&charger->io_lock);
	data = sm5703_reg_read(i2c, SM5703_CHGCNTL6);
	data &= ~SM5703_FREQSEL_MASK;
	data |= (freqsel_hz << SM5703_FREQSEL_SHIFT);

	sm5703_reg_write(i2c, SM5703_CHGCNTL6, data);

	data = sm5703_reg_read(i2c, SM5703_CHGCNTL6);
	pr_info("%s : SM5703_CHGCNTL6 (FREQSEL) : 0x%02x\n",
		__func__, data);    
	mutex_unlock(&charger->io_lock);
}


static void sm5703_set_input_current_limit(struct sm5703_charger_data *charger,
		int current_limit)
{
	struct i2c_client *i2c = charger->sm5703->i2c_client;
	int data = 0, temp = 0;

	mutex_lock(&charger->io_lock);
	data = sm5703_reg_read(i2c, SM5703_VBUSCNTL);
	data &= ~SM5703_VBUSLIMIT;

	if (current_limit >= 2100)
		current_limit = 2100;

	if(current_limit <= 100)
		data &= ~SM5703_VBUSLIMIT;
	else {
		temp = (current_limit - 100) /50;
		data |= temp;
	}

	sm5703_reg_write(i2c, SM5703_VBUSCNTL, data);

	data = sm5703_reg_read(i2c, SM5703_VBUSCNTL);
	pr_info("%s : SM5703_VBUSCNTL (Input current limit) : 0x%02x\n",
		__func__, data);    
	mutex_unlock(&charger->io_lock);
}

static int sm5703_get_input_current_limit(struct i2c_client *i2c)
{
	int ret, current_limit = 0;
	ret = sm5703_reg_read(i2c, SM5703_VBUSCNTL);
	if (ret < 0)
		return ret;
	ret&=SM5703_VBUSLIMIT_MASK;
	current_limit = (100 + (ret*50));

	return current_limit;
}

static void sm5703_set_regulation_voltage(struct sm5703_charger_data *charger,
		int float_voltage)
{
	struct i2c_client *i2c = charger->sm5703->i2c_client;
	int data = 0;

	data = sm5703_reg_read(i2c, SM5703_CHGCNTL3);

	data &= ~SM5703_BATREG_MASK;

	if ((float_voltage) <= 4120)
		data = 0x00;
	else if ((float_voltage) >= 4430)
		data = 0x1f;
	else
		data = ((float_voltage - 4120) / 10);
    
	mutex_lock(&charger->io_lock);
	sm5703_reg_write(i2c, SM5703_CHGCNTL3, data);
	data = sm5703_reg_read(i2c, SM5703_CHGCNTL3);
	pr_info("%s : SM5703_CHGCNTL3 (Battery regulation voltage) : 0x%02x\n",
		__func__, data);    
	mutex_unlock(&charger->io_lock);
}

#if defined(CONFIG_BATTERY_SWELLING)
static int sm5703_get_regulation_voltage(struct sm5703_charger_data *charger)
{
	struct i2c_client *i2c = charger->sm5703->i2c_client;
	int data;
	
	data = sm5703_reg_read(i2c, SM5703_CHGCNTL3);
	if (data < 0) {
		pr_info("%s: warning --> fail to read i2c register(%d)\n", __func__, data);
		return data;
	}
	
	data &= SM5703_BATREG_MASK;
	pr_info("%s: battery cv voltage 0x%x\n", __func__, data);

	return data;
}
#endif

static void __sm5703_set_fast_charging_current(struct i2c_client *i2c,
		int charging_current)
{
	int data = 0;

	if(charging_current <= 100)
		charging_current = 100;
	else if (charging_current >= 2500)
		charging_current = 2500;

	data = (charging_current - 100) / 50;

	sm5703_reg_write(i2c, SM5703_CHGCNTL2, data);

	data = sm5703_reg_read(i2c, SM5703_CHGCNTL2);
	pr_info("%s : SM5703_CHGCNTL2 (fastchg current) : 0x%02x\n",
		__func__, data);
}

static int sm5703_get_fast_charging_current(struct i2c_client *i2c)
{
	int data = sm5703_reg_read(i2c, SM5703_CHGCNTL2);
	int charging_current = 0;
    
	if (data < 0)
		return data;

	data &= SM5703_FASTCHG_MASK;
	charging_current = (100 + (data*50));
        
	return charging_current;
}

static int sm5703_get_current_topoff_setting(struct sm5703_charger_data *charger)
{
	int ret, data = 0, topoff_current = 0;
	mutex_lock(&charger->io_lock);
	ret = sm5703_reg_read(charger->sm5703->i2c_client, SM5703_CHGCNTL4);
	mutex_unlock(&charger->io_lock);
	if (ret < 0) {
		pr_info("%s: warning --> fail to read i2c register(%d)\n", __func__, ret);
		return ret;
	}

	data = ((ret & SM5703_TOPOFF_MASK) >> SM5703_TOPOFF_SHIFT);  
	topoff_current = (100 + (data*25));
    
	return topoff_current;
}

static void __sm5703_set_termination_current_limit(struct i2c_client *i2c,
		int current_limit)
{
	int data = 0, temp = 0;

	pr_info("%s : Set Termination\n", __func__);

	data = sm5703_reg_read(i2c, SM5703_CHGCNTL4);
	data &= ~SM5703_TOPOFF_MASK;

	if(current_limit <= 100)
		current_limit = 100;
	else if (current_limit >= 475)
		current_limit = 475;

	temp = (current_limit - 100) / 25;
	data |= (temp << SM5703_TOPOFF_SHIFT);
    
	sm5703_reg_write(i2c, SM5703_CHGCNTL4, data);

	data = sm5703_reg_read(i2c, SM5703_CHGCNTL4);
	pr_info("%s : SM5703_CHGCNTL4 (Top-off current threshold) : 0x%02x\n",
		__func__, data);    
}

static void sm5703_set_charging_current(struct sm5703_charger_data *charger,
					int topoff, int reset_topoff)
{
	int adj_current = 0;

	adj_current = charger->charging_current * charger->siop_level / 100;
#if CONFIG_SIOP_CHARGING_LIMIT_CURRENT
	if(charger->siop_level < 100 && adj_current > CONFIG_SIOP_CHARGING_LIMIT_CURRENT)
		adj_current = CONFIG_SIOP_CHARGING_LIMIT_CURRENT;
#endif
	pr_info("%s adj_current = %dmA charger->siop_level = %d\n",__func__, adj_current,charger->siop_level);
	mutex_lock(&charger->io_lock);
	__sm5703_set_fast_charging_current(charger->sm5703->i2c_client,
					   adj_current);

	__sm5703_set_termination_current_limit(
			charger->sm5703->i2c_client, topoff);
	mutex_unlock(&charger->io_lock);
}

static void sm5703_set_otgcurrent(struct sm5703_charger_data *charger,
		int otg_current)
{
	struct i2c_client *i2c = charger->sm5703->i2c_client;
	int data = 0;

	data = sm5703_reg_read(i2c, SM5703_OTGCURRENTCNTL);
	data &= ~SM5703_OTGCURRENT_MASK;

	if (otg_current <= 500)
		data = 0x00;
	else if (otg_current <= 700)
		data = 0x01;
	else if (otg_current <= 900)
		data = 0x02;    
	else
		data = 0x3;
    
	mutex_lock(&charger->io_lock);
	sm5703_reg_write(i2c, SM5703_OTGCURRENTCNTL, data);
	data = sm5703_reg_read(i2c, SM5703_OTGCURRENTCNTL);
	pr_info("%s : SM5703_OTGCURRENTCNTL (OTG current) : 0x%02x\n",	__func__, data);
	mutex_unlock(&charger->io_lock);
}

static void sm5703_set_bst_iq3limit(struct sm5703_charger_data *charger,
		int iq3limit)
{
	struct i2c_client *i2c = charger->sm5703->i2c_client;
	int data = 0;

	mutex_lock(&charger->io_lock);
	data = sm5703_reg_read(i2c, SM5703_Q3LIMITCNTL);
	data &= ~SM5703_BST_IQ3LIMIT_MASK;
    data |= (iq3limit << SM5703_BST_IQ3LIMIT_SHIFT);

	sm5703_reg_write(i2c, SM5703_Q3LIMITCNTL, data);

	data = sm5703_reg_read(i2c, SM5703_Q3LIMITCNTL);
	pr_info("%s : SM5703_Q3LIMITCNTL (BST_IQ3LIMIT) : 0x%02x\n",
		__func__, data);
	mutex_unlock(&charger->io_lock);
}

enum {
	SM5703_AICL_4300MV = 0,
	SM5703_AICL_4400MV,
	SM5703_AICL_4500MV,
	SM5703_AICL_4600MV,
	SM5703_AICL_4700MV,
	SM5703_AICL_4800MV,
	SM5703_AICL_4900MV,
};

#if ENABLE_AICL
/* Dedicated charger (non-USB) device
 * will use lower AICL level to get better performance
 */
static void sm5703_set_aicl_level(struct sm5703_charger_data *charger)
{
	int aicl;
	switch(charger->cable_type) {
	case POWER_SUPPLY_TYPE_USB ... POWER_SUPPLY_TYPE_USB_ACA:
		aicl = SM5703_AICL_4500MV;
		break;
	default:
		aicl = SM5703_AICL_4500MV;
	}
	mutex_lock(&charger->io_lock);
	sm5703_assign_bits(charger->sm5703->i2c_client,
			SM5703_CHGCNTL5, SM5703_AICLTH_MASK, aicl);
	mutex_unlock(&charger->io_lock);
}
#endif /*ENABLE_AICL*/

static void sm5703_configure_charger(struct sm5703_charger_data *charger)
{

	int topoff;
	union power_supply_propval val;

	pr_info("%s : Set config charging\n", __func__);
	if (charger->charging_current < 0) {
		pr_info("%s : OTG is activated. Ignore command!\n",
				__func__);
		return;
	}
    
#if ENABLE_AICL
        sm5703_set_aicl_level(charger);
#endif /*DISABLE_AICL*/
	psy_do_property("battery", get,
			POWER_SUPPLY_PROP_CHARGE_NOW, val);

	/* Input current limit */
	pr_info("%s : input current (%dmA)\n",
			__func__, charger->pdata->charging_current_table
				[charger->cable_type].input_current_limit);

	sm5703_set_input_current_limit(charger,
			charger->pdata->charging_current_table
				[charger->cable_type].input_current_limit);

	/* Float voltage */
	pr_info("%s : float voltage (%dmV)\n",
			__func__, charger->pdata->chg_float_voltage);

	sm5703_set_regulation_voltage(charger,
			charger->pdata->chg_float_voltage);

	charger->charging_current = charger->pdata->charging_current_table
			[charger->cable_type].fast_charging_current;
	topoff = charger->pdata->charging_current_table
			[charger->cable_type].full_check_current_1st;
	/* Fast charge and Termination current */
	pr_info("%s : fast charging current (%dmA)\n",
			__func__, charger->charging_current);

	pr_info("%s : termination current (%dmA)\n",
			__func__, topoff);

	sm5703_set_charging_current(charger, topoff, 1);//Fastcharging/Topoff Current
	sm5703_enable_charger_switch(charger, 1); //Charging Enable/Disable.
}

int sm5703_chg_fled_init(struct i2c_client *client)
{
	int ret = 0;//, rev_id;
#if 0    
	sm5703_mfd_chip_t *chip = i2c_get_clientdata(client);
	struct sm5703_charger_data *charger = chip->charger;
#endif
	return ret;
}
EXPORT_SYMBOL(sm5703_chg_fled_init);

/* here is set init charger data */
static bool sm5703_chg_init(struct sm5703_charger_data *charger)
{
	sm5703_mfd_chip_t *chip = i2c_get_clientdata(charger->sm5703->i2c_client);
	chip->charger = charger;
	sm5703_chg_fled_init(charger->sm5703->i2c_client);
	//int data = 0;
	charger->full_charged = false;

	/* AUTOSTOP */
	sm5703_enable_autostop(chip->charger, (int)charger->pdata->chg_autostop);
	/* AUTOSET */
	sm5703_enable_autoset(chip->charger, (int)charger->pdata->chg_autoset);
	/* AICLEN */
	sm5703_enable_aiclen(chip->charger, (int)charger->pdata->chg_aiclen);
	/* AICLTH */
	sm5703_set_aiclth(chip->charger, (int)charger->pdata->chg_aiclth);
	/* FREQSEL */
	sm5703_set_freqsel(chip->charger, SM5703_FREQSEL_1P5MHZ);
	/* BST_IQ3LIMIT */
	sm5703_set_bst_iq3limit(chip->charger, SM5703_BST_IQ3LIMIT_0P7X);

	/* MUST set correct regulation voltage first
 	* Before MUIC pass cable type information to charger
 	* charger would be already enabled (default setting)
	* it might cause EOC event by incorrect regulation voltage */
	sm5703_set_regulation_voltage(charger,
				charger->pdata->chg_float_voltage);
	sm5703_set_otgcurrent(charger,1200);//OTGCURRENT : 1.2A

	sm5703_test_read(charger->sm5703->i2c_client);

	return true;
}


static int sm5703_get_charging_status(struct sm5703_charger_data *charger)
{
	int status = POWER_SUPPLY_STATUS_UNKNOWN;
	int ret;
	int nCHG = 0;

	ret = sm5703_reg_read(charger->sm5703->i2c_client, SM5703_STATUS3);
	
	if (ret<0) {
		pr_info("Error : can't get charging status (%d)\n", ret);
	}
	pr_info("%s charger->full_charged = %d \n",__func__,charger->full_charged);
//    pr_info("%s charger->pdata->fg_vol_val = %d,charger->pdata->fg_soc_val = %d, charger->pdata->fg_curr_avr_val = %d\n",__func__,charger->pdata->fg_vol_val,charger->pdata->fg_soc_val,charger->pdata->fg_curr_avr_val);

	nCHG = gpio_get_value(charger->pdata->chgen_gpio);

	if((ret & SM5703_STATUS3_DONE) || (ret & SM5703_STATUS3_TOPOFF)
//        || ((vol_val.intval >= charger->pdata->fg_vol_val) && (soc_val.intval >= charger->pdata->fg_soc_val) && (curr_avr_val.intval <= charger->pdata->fg_curr_avr_val) && (!nCHG))
	) 
	{    
		status = POWER_SUPPLY_STATUS_FULL;
		charger->full_charged = true;
		pr_info("%s : Status, Power Supply Full \n", __func__);
	}else if(ret & SM5703_STATUS3_CHGON){
		status = POWER_SUPPLY_STATUS_CHARGING;    
	}else {
		if (nCHG)
			status = POWER_SUPPLY_STATUS_DISCHARGING;
		else
			status = POWER_SUPPLY_STATUS_NOT_CHARGING;
	}


	/* TEMP_TEST : when OTG is enabled(charging_current -1), handle OTG func. */
	if (charger->charging_current < 0) {
		/* For OTG mode, SM5703 would still report "charging" */
		status = POWER_SUPPLY_STATUS_DISCHARGING;
		ret = sm5703_reg_read(charger->sm5703->i2c_client, SM5703_STATUS1);

		if (ret & SM5703_STATUS1_OTGFAIL) {
			pr_info("%s: otg overcurrent limit\n", __func__);

			sm5703_charger_otg_control(charger, false);
		}

	}

	return status;
}

static int sm5703_get_charging_health(struct sm5703_charger_data *charger)
{
	int vbus_status = sm5703_reg_read(charger->sm5703->i2c_client, SM5703_STATUS5);
	int health = POWER_SUPPLY_HEALTH_GOOD;

	pr_info("%s : charger->is_charging = %d, charger->cable_type = %d\n", __func__, charger->is_charging,charger->cable_type);

	if (vbus_status < 0) {
		health = POWER_SUPPLY_HEALTH_UNKNOWN;
		pr_info("%s : Health : %d, vbus_status : %d\n", __func__, health,vbus_status);
		return (int)health;
	}

	if (vbus_status & SM5703_STATUS5_VBUSOK)
		health = POWER_SUPPLY_HEALTH_GOOD;
	else if (vbus_status & SM5703_STATUS5_VBUSOVP)
		health = POWER_SUPPLY_HEALTH_OVERVOLTAGE;
	else if (vbus_status & SM5703_STATUS5_VBUSUVLO)
		health = POWER_SUPPLY_HEALTH_UNDERVOLTAGE;
	else
		health = POWER_SUPPLY_HEALTH_UNKNOWN;

	pr_info("%s : Health : %d\n", __func__, health);

	return (int)health;
}

static int sec_chg_get_property(struct power_supply *psy,
		enum power_supply_property psp,
		union power_supply_propval *val)
{

	int chg_curr,aicr;
	struct sm5703_charger_data *charger =
		container_of(psy, struct sm5703_charger_data, psy_chg);

	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = charger->charging_current ? 1 : 0;
		break;
	case POWER_SUPPLY_PROP_STATUS:
        if (charger->full_charged)
        {
            val->intval = POWER_SUPPLY_STATUS_FULL;
            pr_info("%s: POWER_SUPPLY_PROP_STATUS = POWER_SUPPLY_STATUS_FULL\n", __func__);
        }
        else
		    val->intval = sm5703_get_charging_status(charger);
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		val->intval = sm5703_get_charging_health(charger);
		break;
	case POWER_SUPPLY_PROP_CURRENT_MAX:
		sm5703_test_read(charger->sm5703->i2c_client);
		val->intval = sm5703_get_fast_charging_current(charger->sm5703->i2c_client);
		break;
        case POWER_SUPPLY_PROP_CURRENT_AVG:
		break;
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		if (charger->charging_current) {
			aicr = sm5703_get_input_current_limit(charger->sm5703->i2c_client);
			chg_curr = sm5703_get_fast_charging_current(charger->sm5703->i2c_client);
			val->intval = MINVAL(aicr, chg_curr);
		} else
			val->intval = 0;
		break;
	case POWER_SUPPLY_PROP_CHARGE_TYPE:
		val->intval = POWER_SUPPLY_CHARGE_TYPE_UNKNOWN;
        break;
#if defined(CONFIG_BATTERY_SWELLING) || defined(CONFIG_BATTERY_SWELLING_SELF_DISCHARGING)
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		val->intval = sm5703_get_regulation_voltage(charger);
		break;
#endif
	default:
		return -EINVAL;
	}

	return 0;
}

static int sec_chg_set_property(struct power_supply *psy,
		enum power_supply_property psp,
		const union power_supply_propval *val)
{
	struct sm5703_charger_data *charger =
	    container_of(psy, struct sm5703_charger_data, psy_chg);
	int topoff;
	int previous_cable_type = charger->cable_type;

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		charger->status = val->intval;
		break;
		/* val->intval : type */
	case POWER_SUPPLY_PROP_ONLINE:
		charger->cable_type = val->intval;
		if (charger->cable_type == POWER_SUPPLY_TYPE_BATTERY) {
			pr_info("%s:[BATT] Type Battery\n", __func__);
			sm5703_enable_charger_switch(charger, 0);
			if (previous_cable_type == POWER_SUPPLY_TYPE_OTG)
				sm5703_charger_otg_control(charger, false);
		} else if (charger->cable_type == POWER_SUPPLY_TYPE_OTG) {
			pr_info("%s: OTG mode\n", __func__);
			sm5703_charger_otg_control(charger, true);
		} else {
			pr_info("%s:[BATT] Set charging"
					", Cable type = %d\n", __func__, charger->cable_type);
			/* Enable charger */
			sm5703_configure_charger(charger);
		}
#if EN_TEST_READ
		//msleep(100);
		sm5703_test_read(charger->sm5703->i2c_client);
#endif
		break;
	case POWER_SUPPLY_PROP_CURRENT_AVG:
#if defined(CONFIG_BATTERY_SWELLING)
		if (val->intval > charger->pdata->charging_current_table
			[charger->cable_type].fast_charging_current) {
			break;
		}
#endif
		charger->charging_current = val->intval;
		sm5703_set_charging_current(charger,
			val->intval, 0);
		break;
	case POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN:
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		/* set charging current */
		if (charger->is_charging) {
			sm5703_configure_charger(charger);
		}
		break;
	case POWER_SUPPLY_PROP_POWER_NOW:
		topoff = sm5703_get_current_topoff_setting(charger);
		pr_info("%s:Set Power Now -> chg current = %d mA, topoff = %d mA\n", __func__,
			val->intval, topoff);
		sm5703_set_charging_current(charger, topoff, 0);
		break;
#if defined(CONFIG_BATTERY_SWELLING) || defined(CONFIG_BATTERY_SWELLING_SELF_DISCHARGING)
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		pr_info("%s: float voltage(%d)\n", __func__, val->intval);
		sm5703_set_regulation_voltage(charger, val->intval);
		break;
#endif
	case POWER_SUPPLY_PROP_HEALTH:
        //charger->ovp = val->intval;
        break;
	default:
		return -EINVAL;
	}

	return 0;
}

ssize_t sm5703_chg_show_attrs(struct device *dev,
		const ptrdiff_t offset, char *buf)
{
	struct power_supply *psy = dev_get_drvdata(dev);
	struct sm5703_charger_data *charger =
		container_of(psy, struct sm5703_charger_data, psy_chg);
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

		sm5703_read_regs(charger->sm5703->i2c_client, str);
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

ssize_t sm5703_chg_store_attrs(struct device *dev,
		const ptrdiff_t offset,
		const char *buf, size_t count)
{
	struct power_supply *psy = dev_get_drvdata(dev);
	struct sm5703_charger_data *charger =
		container_of(psy, struct sm5703_charger_data, psy_chg);

	int ret = 0;
	int x = 0;
	uint8_t data = 0;

	switch (offset) {
	case CHG_REG:
		if (sscanf(buf, "%x\n", &x) == 1) {
			charger->reg_addr = x;
			data = sm5703_reg_read(charger->sm5703->i2c_client,
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
			ret = sm5703_reg_write(charger->sm5703->i2c_client,
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

struct sm5703_chg_irq_handler {
	char *name;
	int irq_index;
	irqreturn_t (*handler)(int irq, void *data);
};
#if EN_NOBAT_IRQ
static irqreturn_t sm5703_chg_nobat_irq_handler(int irq, void *data)
{
	struct sm5703_charger_data *info = data;
	struct i2c_client *iic = info->sm5703->i2c_client;

	/* set full charged flag
	 * until TA/USB unplug event / stop charging by PSY
	 */

	 pr_info("%s : Nobat\n", __func__);

#if EN_TEST_READ
         sm5703_test_read(iic);
#endif

	return IRQ_HANDLED;
}
#endif /*EN_NOBAT_IRQ*/

#if EN_DONE_IRQ
static irqreturn_t sm5703_chg_done_irq_handler(int irq, void *data)
{
	struct sm5703_charger_data *info = data;
	struct i2c_client *iic = info->sm5703->i2c_client;

	/* set full charged flag
	 * until TA/USB unplug event / stop charging by PSY
	 */

	 pr_info("%s : Full charged(done)\n", __func__);
	 info->full_charged = true;

#if EN_TEST_READ
	sm5703_test_read(iic);
#endif

	return IRQ_HANDLED;
}
#endif/*EN_DONE_IRQ*/

#if EN_TOPOFF_IRQ
static irqreturn_t sm5703_chg_topoff_irq_handler(int irq, void *data)
{
	struct sm5703_charger_data *info = data;
	struct i2c_client *iic = info->sm5703->i2c_client;

	/* set full charged flag
	 * until TA/USB unplug event / stop charging by PSY
	 */

	 pr_info("%s : Full charged(topoff)\n", __func__);
	 info->full_charged = true;
     
#if EN_TEST_READ
	sm5703_test_read(iic);
#endif

	return IRQ_HANDLED;
}
#endif /*EN_TOPOFF_IRQ*/

#if EN_CHGON_IRQ
static irqreturn_t sm5703_chg_chgon_irq_handler(int irq, void *data)
{
	struct sm5703_charger_data *info = data;
	struct i2c_client *iic = info->sm5703->i2c_client;

	 pr_info("%s : Chgon\n", __func__);

#if EN_TEST_READ
	sm5703_test_read(iic);
#endif

	return IRQ_HANDLED;
}
#endif /*EN_CHGON_IRQ*/

const struct sm5703_chg_irq_handler sm5703_chg_irq_handlers[] = {
#if EN_NOBAT_IRQ    
	{
		.name = "NOBAT",
		.handler = sm5703_chg_nobat_irq_handler,
		.irq_index = SM5703_NOBAT_IRQ,
	},
#endif /*EN_NOBAT_IRQ*/
#if EN_DONE_IRQ
    {
		.name = "DONE",
		.handler = sm5703_chg_done_irq_handler,
		.irq_index = SM5703_DONE_IRQ,
	},
#endif/*EN_DONE_IRQ*/	
#if EN_TOPOFF_IRQ	
    {
		.name = "TOPOFF",
		.handler = sm5703_chg_topoff_irq_handler,
		.irq_index = SM5703_TOPOFF_IRQ,
	},
#endif /*EN_TOPOFF_IRQ*/
#if EN_CHGON_IRQ
    {
        .name = "CHGON",
        .handler = sm5703_chg_chgon_irq_handler,
        .irq_index = SM5703_CHGON_IRQ,
    },	
#endif/*EN_CHGON_IRQ*/    
};


static int register_irq(struct platform_device *pdev,
		struct sm5703_charger_data *info)
{
	int irq;
	int i, j;
	int ret;
	const struct sm5703_chg_irq_handler *irq_handler = sm5703_chg_irq_handlers;
	const char *irq_name;
	for (i = 0; i < ARRAY_SIZE(sm5703_chg_irq_handlers); i++) {
		irq_name = sm5703_get_irq_name_by_index(irq_handler[i].irq_index);
		irq = platform_get_irq_byname(pdev, irq_name);
		ret = request_threaded_irq(irq, NULL, irq_handler[i].handler,
					   IRQF_ONESHOT | IRQF_TRIGGER_FALLING |
					   IRQF_NO_SUSPEND, irq_name, info);
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
		irq_name = sm5703_get_irq_name_by_index(irq_handler[j].irq_index);
		irq = platform_get_irq_byname(pdev, irq_name);
		free_irq(irq, info);
	}

	return ret;
}

static void unregister_irq(struct platform_device *pdev,
		struct sm5703_charger_data *info)
{
	int irq;
	int i;
	const char *irq_name;
	const struct sm5703_chg_irq_handler *irq_handler = sm5703_chg_irq_handlers;

	for (i = 0; i < ARRAY_SIZE(sm5703_chg_irq_handlers); i++) {
		irq_name = sm5703_get_irq_name_by_index(irq_handler[i].irq_index);
		irq = platform_get_irq_byname(pdev, irq_name);
		free_irq(irq, info);
	}
}

#ifdef CONFIG_OF

static int sec_bat_read_u32_index_dt(const struct device_node *np,
				       const char *propname,
				       u32 index, u32 *out_value)
{
	struct property *prop = of_find_property(np, propname, NULL);
	u32 len = (index + 1) * sizeof(*out_value);

	if (!prop)
		return (-EINVAL);
	if (!prop->value)
		return (-ENODATA);
	if (len > prop->length)
		return (-EOVERFLOW);

	*out_value = be32_to_cpup(((__be32 *)prop->value) + index);

	return 0;
}

static int sm5703_charger_parse_dt(struct device *dev,
                           struct sm5703_charger_platform_data *pdata)
{
	struct device_node *np = dev->of_node;
	const u32 *p;
	int ret, i, len;

	// chg_autostop
	ret = of_property_read_u32(np, "chg_autostop",
			&pdata->chg_autostop);
	if (ret < 0) {
		pr_info("%s : cannot get chg autostop\n", __func__);
		return -ENODATA;
	}
	// chg_autoset
	ret = of_property_read_u32(np, "chg_autoset",
			&pdata->chg_autoset);
	if (ret < 0) {
		pr_info("%s : cannot get chg autoset\n", __func__);
            return -ENODATA;
	}
	// chg_aiclen
	ret = of_property_read_u32(np, "chg_aiclen",
			&pdata->chg_aiclen);
	if (ret < 0) {
		pr_info("%s : cannot get chg aiclen\n", __func__);
		return -ENODATA;
	}
	// chg_aiclth
	ret = of_property_read_u32(np, "chg_aiclth",
			&pdata->chg_aiclth);
	if (ret < 0) {
		pr_info("%s : cannot get chg aiclth\n", __func__);            
		return -ENODATA;
	}

	// fg_vol_val
	ret = of_property_read_u32(np, "fg_vol_val",
			&pdata->fg_vol_val);
	if (ret < 0) {
		pr_info("%s : cannot get fg_vol_val\n", __func__);            
		return -ENODATA;
	}

	// fg_soc_val
	ret = of_property_read_u32(np, "fg_soc_val",
			&pdata->fg_soc_val);
	if (ret < 0) {
		pr_info("%s : cannot get fg_soc_val\n", __func__);            
		return -ENODATA;
	}

	// fg_curr_avr_val
	ret = of_property_read_u32(np, "fg_curr_avr_val",
			&pdata->fg_curr_avr_val);
	if (ret < 0) {
		pr_info("%s : cannot get fg_curr_avr_val\n", __func__);            
		return -ENODATA;
	}

	np = of_find_node_by_name(NULL, "charger");
	if (!np) {
		pr_info("%s : np NULL\n", __func__);
		return -ENODATA;
	}
	// battery,chg_float_voltage
	ret = of_property_read_u32(np, "battery,chg_float_voltage",
			&pdata->chg_float_voltage);
	if (ret < 0) {
		pr_info("%s : cannot get chg float voltage\n", __func__);
		return -ENODATA;
	}

	p = of_get_property(np, "battery,input_current_limit", &len);

	len = len / sizeof(u32);

	pdata->charging_current_table =
	kzalloc(sizeof(sec_charging_current_t) * len, GFP_KERNEL);

	for(i = 0; i < len; i++) {
		ret = sec_bat_read_u32_index_dt(np,
				 "battery,input_current_limit", i,
				 &pdata->charging_current_table[i].input_current_limit);
		ret = sec_bat_read_u32_index_dt(np,
				 "battery,fast_charging_current", i,
				 &pdata->charging_current_table[i].fast_charging_current);
		ret = sec_bat_read_u32_index_dt(np,
				 "battery,full_check_current_1st", i,
				 &pdata->charging_current_table[i].full_check_current_1st);
		ret = sec_bat_read_u32_index_dt(np,
				 "battery,full_check_current_2nd", i,
				 &pdata->charging_current_table[i].full_check_current_2nd);
	}

	pdata->chgen_gpio = of_get_named_gpio(np, "battery,chg_gpio_en",0); //nCHGEN
	pr_info("%s: SM5703_parse_dt chgen: %d\n", __func__, pdata->chgen_gpio);

	dev_info(dev,"sm5703 charger parse dt retval = %d\n", ret);
	return ret;
}

static struct of_device_id sm5703_charger_match_table[] = {
	{ .compatible = "siliconmitus,sm5703-charger",},
	{},
};
#else
static int sm5703_charger_parse_dt(struct device *dev,
                           struct sm5703_charger_platform_data *pdata)
{
    return -ENOSYS;
}
#define sm5703_charger_match_table NULL
#endif /* CONFIG_OF */

static int sm5703_charger_probe(struct platform_device *pdev)
{
	sm5703_mfd_chip_t *chip = dev_get_drvdata(pdev->dev.parent);
	struct sm5703_mfd_platform_data *mfd_pdata = dev_get_platdata(chip->dev);
	struct sm5703_charger_data *charger;
	int ret = 0;

	pr_info("%s:[BATT] SM5703 Charger driver probe..0x%x\n", __func__, (unsigned int)mfd_pdata);

	charger = kzalloc(sizeof(*charger), GFP_KERNEL);
	if (!charger)
		return -ENOMEM;

#ifdef CONFIG_OF
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,10,0))
	if (pdev->dev.parent->of_node) {
		pdev->dev.of_node = of_find_compatible_node(
			of_node_get(pdev->dev.parent->of_node), NULL,
			sm5703_charger_match_table[0].compatible);
	}
#endif
#endif
	mutex_init(&charger->io_lock);
	charger->wq = create_workqueue("sm5703chg_workqueue");
	charger->sm5703= chip;
	//if (pdev->dev.of_node)
	if(1)
	{
		charger->pdata = devm_kzalloc(&pdev->dev, sizeof(*(charger->pdata)), GFP_KERNEL);
		if (!charger->pdata) {
			dev_err(&pdev->dev, "Failed to allocate memory\n");
			ret = -ENOMEM;
			goto err_parse_dt_nomem;
		}
		ret = sm5703_charger_parse_dt(&pdev->dev, charger->pdata);
		if (ret < 0)
            goto err_parse_dt;
	} else
        charger->pdata = mfd_pdata->charger_platform_data;

	platform_set_drvdata(pdev, charger);

	charger->psy_chg.name           = "sm5703-charger";
	charger->psy_chg.type           = POWER_SUPPLY_TYPE_UNKNOWN;
	charger->psy_chg.get_property   = sec_chg_get_property;
	charger->psy_chg.set_property   = sec_chg_set_property;
	charger->psy_chg.properties     = sec_charger_props;
	charger->psy_chg.num_properties = ARRAY_SIZE(sec_charger_props);

	charger->siop_level = 100;
	charger->ovp = 0;
	sm5703_chg_init(charger);

	ret = power_supply_register(&pdev->dev, &charger->psy_chg);
	if (ret) {
		pr_err("%s: Failed to Register psy_chg\n", __func__);
		goto err_power_supply_register;
	}
	ret = register_irq(pdev, charger);
	if (ret < 0)
        goto err_reg_irq;

	ret = gpio_request(charger->pdata->chgen_gpio, "sm5703_nCHGEN");
	if (ret) {
		pr_info("%s : Request GPIO %d failed\n",
		       __func__, (int)charger->pdata->chgen_gpio);
    }

	sm5703_test_read(charger->sm5703->i2c_client);
	pr_info("%s:[BATT] SM5703 charger driver loaded OK\n", __func__);

	return 0;
err_reg_irq:
    power_supply_unregister(&charger->psy_chg);
err_power_supply_register:
err_parse_dt:
err_parse_dt_nomem:
	destroy_workqueue(charger->wq);
	mutex_destroy(&charger->io_lock);
	kfree(charger);
	return ret;
}

static int sm5703_charger_remove(struct platform_device *pdev)
{
	struct sm5703_charger_data *charger =
	platform_get_drvdata(pdev);
	unregister_irq(pdev, charger);
	power_supply_unregister(&charger->psy_chg);
	destroy_workqueue(charger->wq);
	mutex_destroy(&charger->io_lock);
	kfree(charger);
	return 0;
}

#if defined CONFIG_PM
static int sm5703_charger_suspend(struct device *dev)
{
	return 0;
}

static int sm5703_charger_resume(struct device *dev)
{
	return 0;
}
#else
#define sm5703_charger_suspend NULL
#define sm5703_charger_resume NULL
#endif

static void sm5703_charger_shutdown(struct device *dev)
{
	pr_info("%s: SM5703 Charger driver shutdown\n", __func__);
}

static SIMPLE_DEV_PM_OPS(sm5703_charger_pm_ops, sm5703_charger_suspend,
		sm5703_charger_resume);

static struct platform_driver sm5703_charger_driver = {
	.driver		= {
		.name	= "sm5703-charger",
		.owner	= THIS_MODULE,
		.of_match_table = sm5703_charger_match_table,
		.pm 	= &sm5703_charger_pm_ops,
		.shutdown = sm5703_charger_shutdown,
	},
	.probe		= sm5703_charger_probe,
	.remove		= sm5703_charger_remove,
};

static int __init sm5703_charger_init(void)
{
	int ret = 0;

	pr_info("%s \n", __func__);
	ret = platform_driver_register(&sm5703_charger_driver);

	return ret;
}
subsys_initcall(sm5703_charger_init);

static void __exit sm5703_charger_exit(void)
{
	platform_driver_unregister(&sm5703_charger_driver);
}
module_exit(sm5703_charger_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Charger driver for SM5703");
