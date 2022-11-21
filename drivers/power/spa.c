#ifndef __SPA_C__
#define __SPA_C__ 

/*
 * Samsung Power & Accessory
 *
 *  driver for pxa918 - spa.c
 *
 *  Copyright (C) 2011, Samsung Electronics
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/errno.h>
#include <linux/power_supply.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>
#include <linux/i2c.h>
#include <linux/proc_fs.h>
#include <linux/mutex.h>
#include <linux/delay.h>

#include <asm/irq.h>
#include <mach/gpio.h>
#include <asm/uaccess.h>
#include <linux/power/spa.h>

#include <mach/hardware.h>
#include <mach/irqs.h>
#include <plat/mfp.h>

#include <linux/wakelock.h>

#if defined(CONFIG_SPA)
#if defined(CONFIG_SPA_LPM_MODE)
int spa_lpm_charging_mode_get(void);
#if defined(CONFIG_BATTERY_D1980)
#include <linux/d1982/d1980_reg.h>
#elif defined(CONFIG_MFD_88PM800)
#include <linux/mfd/88pm80x.h>
#endif
#endif
#endif

#ifdef CONFIG_MACH_ARUBA_TD
int spa_call_status = 0;
#endif


#define TEMP_TEST_BIN	0	
#if TEMP_TEST_BIN 
#include <linux/proc_fs.h>
#include <asm/uaccess.h>
#define SPA_TEMP_ADC_PROC_FILE	"driver/spa_temp_adc_test"
static struct proc_dir_entry *spa_temp_adc_proc_file; 
static int spa_temp_adc_test = 0;
static ssize_t spa_temp_adc_proc_read(struct file *filp,
                                      char *buffer, size_t length,
                                      loff_t * offset)
{
        printk(" %s :%d\n", __func__, spa_temp_adc_test);
        return 0;
}
static ssize_t spa_temp_adc_proc_write(struct file *filp,
                                       const char *buff, size_t len,
                                       loff_t * off)
{
        char messages[256];

        if (len > 256)
                len = 256;

        if (copy_from_user(messages, buff, len))
                return -EFAULT;

        if ('-' == messages[0]) {
                spa_temp_adc_test-=50;
        } else if('+' == messages[0]) {
                spa_temp_adc_test+=50;
        }
        
        printk(" %s: %d\n", __func__, spa_temp_adc_test);

        return len;
}
static struct file_operations spa_temp_adc_proc_ops = {
        .read = spa_temp_adc_proc_read,
        .write = spa_temp_adc_proc_write,
};

static void create_spa_proc_file(void)
{
        spa_temp_adc_proc_file =
            create_proc_entry(SPA_TEMP_ADC_PROC_FILE, 0644, NULL);
        if (spa_temp_adc_proc_file) {
                spa_temp_adc_proc_file->proc_fops = &spa_temp_adc_proc_ops;
        } else
                printk(KERN_INFO "proc file create failed!\n");
}

#endif


static struct spa_driver_data *ginfo = NULL;
#define PRECHARGE_STOP_CURRENT_MAX	200
#define PRECHARGE_STOP_CURRENT_MIN	50
static int pre_charging_stop = 0;
int spa_fuelgauge_current = 0;


static unsigned char spa_bat_get_status(struct spa_driver_data *info);


/* Generic Function */
/* SAMSUNG_STUB_ShiftTableAndSetLastElt function put new element in last of array */
/* Oldest element will be eliminated*/
static void spa_ShiftTableAndSetLastElt(s32 *ap_Table, u8 vp_EltNumber, s32 vp_LastElt)
{
        u8      i;

        for (i=1;i<vp_EltNumber;i++){
        ap_Table[i-1] = ap_Table[i];
        }

        ap_Table[vp_EltNumber-1] = vp_LastElt;

} /* spa_ShiftTableAndSetLastElt() */


/* Generic Function */
/* spa_bat_average function calculate average value with element table. */
static s32 spa_bat_average(s32 *ap_Table , u8 vp_EltNumber)
{
        u8      i;
        s32             vl_Sum=0;

        /* For each element in the table */
        for(i=0;i<vp_EltNumber;i++){
        vl_Sum += ap_Table[i];
        }

        return(vl_Sum/vp_EltNumber);
}

static unsigned int spa_bat_get_current_voltage(struct spa_driver_data *info)
{
	unsigned int current_voltage = 0;

	mutex_lock(&info->lock);
	current_voltage = info->battery.current_voltage;
	mutex_unlock(&info->lock);

	return current_voltage;
}

static unsigned int spa_bat_get_average_voltage(struct spa_driver_data *info)
{
	unsigned int average_voltage = 0;

	mutex_lock(&info->lock);
	average_voltage = info->battery.average_voltage;
	mutex_unlock(&info->lock);

	return average_voltage;
}

static unsigned int spa_bat_get_current_temp_adc(struct spa_driver_data *info)
{
	unsigned int current_temp_adc = 0; 

	mutex_lock(&info->lock);
	current_temp_adc = info->battery.current_temp_adc;
	mutex_unlock(&info->lock);

	return current_temp_adc; 
}

static unsigned int spa_bat_get_average_temp_adc(struct spa_driver_data *info)
{
	unsigned int average_temp_adc = 0; 

	mutex_lock(&info->lock);
	average_temp_adc = info->battery.average_temp_adc;
	mutex_unlock(&info->lock);

	return average_temp_adc; 
}

static int spa_bat_get_current_temperature(struct spa_driver_data *info)
{
	int current_temperature = 0; 

	mutex_lock(&info->lock);
	current_temperature = info->battery.current_temperature;
	mutex_unlock(&info->lock);

	return current_temperature; 
}

static int spa_bat_get_average_temperature(struct spa_driver_data *info)
{
	int average_temperature = 0; 

	mutex_lock(&info->lock);
	average_temperature = info->battery.average_temperature;
	mutex_unlock(&info->lock);

	return average_temperature; 
}

static int spa_bat_calculate_temperature_from_adc(struct spa_driver_data *info, int adc)
{
        unsigned char i;
        int vl_A1, vl_A2, vl_A3;
	int temperature = 0;

	unsigned int *a_TempAdcTable = info->temp_adc_table;
        int *a_TempDegreeTable = info->temp_degree_table;


        for(i=0; i<SPA_NB_TEMP_TABLE; i++){
                if(adc >= a_TempAdcTable[i])
                        break;
        } //for

        if(i==0){
                i = 1;
        } //if
        else if(i==SPA_NB_TEMP_TABLE) {
                i = (SPA_NB_TEMP_TABLE-1);
        } //else if

        vl_A1 = adc * (a_TempDegreeTable[i] - a_TempDegreeTable[i-1]);
        vl_A2 = a_TempAdcTable[i-1] * (a_TempDegreeTable[i] - a_TempDegreeTable[i-1])
                        + a_TempDegreeTable[i-1] * (a_TempAdcTable[i-1] - a_TempAdcTable[i]);
        vl_A3 = (a_TempAdcTable[i-1] - a_TempAdcTable[i]);

        temperature = (int)((vl_A2-vl_A1)*1000)/vl_A3;
        temperature *= 1000;

	return temperature;
}

static int spa_bat_get_capacity(struct spa_driver_data *info)
{
	int soc = 0;
	if(info->use_fuelgauge)
	{
		mutex_lock(&info->lock);
		soc = info->battery.soc;
		mutex_unlock(&info->lock);
	}
	else
	{
	}

	return soc;
}

static unsigned char spa_bat_get_health(struct spa_driver_data *info)
{
	unsigned char health;

	mutex_lock(&info->lock);
	health = info->battery.health;
	mutex_unlock(&info->lock);	

	return health;
}

static void spa_bat_set_health(struct spa_driver_data *info, unsigned int health)
{
	mutex_lock(&info->lock);
	info->battery.health = health;
	mutex_unlock(&info->lock);
}

static void spa_bat_set_end_of_charge(struct spa_driver_data *info, unsigned char end_of_charge)
{
	unsigned char result = 0;

	pre_charging_stop = 0;
	
	if(end_of_charge == SPA_END_OF_CHARGE_NONE)
	{
		mutex_lock(&info->lock);
		info->battery.end_of_charge = end_of_charge;
		mutex_unlock(&info->lock);
	}
	else
	{
		mutex_lock(&info->lock);
		result = info->battery.end_of_charge |= end_of_charge; 		
		mutex_unlock(&info->lock);
	}
	printk(" %s   end_of_charge %d result %d\n", __func__, end_of_charge, result);
}

static unsigned char spa_bat_clear_end_of_charge(struct spa_driver_data *info, unsigned char end_of_charge)
{
	unsigned char ret = 0;
	
	mutex_lock(&info->lock);
	ret = info->battery.end_of_charge;
	ret &= (~end_of_charge);
	info->battery.end_of_charge = ret;
	mutex_unlock(&info->lock);

	printk(" %s   end_of_charge %d ret %d\n", __func__, end_of_charge, ret);

	return ret; 
}

static char spa_bat_check_end_of_charge(struct spa_driver_data *info,unsigned char end_of_charge)
{
	char ret = 0;

	mutex_lock(&info->lock);
        ret = info->battery.end_of_charge;
        mutex_unlock(&info->lock);

	ret &= end_of_charge; 

	if(ret)
		ret = 0;
	else
		ret = -1; 

	return ret;
}

static int spa_bat_read_voltage(struct spa_driver_data *info, int *vbat)
{
	int data = 0, ret = 0; 
	mutex_lock(&info->api_lock);
	if(info->battery.read_voltage==NULL)
	{
		printk(" %s   read_voltage is NULL\n", __func__);
		*vbat = 0;
		mutex_unlock(&info->api_lock);
		return -1;
	}
	ret = info->battery.read_voltage(&data);
	mutex_unlock(&info->api_lock);
	if(ret < 0){
		printk(" %s   fail to read voltage\n", __func__);
		*vbat = 0;
		return -1; 
	}

	printk("%s voltage [%d] current [%d]\n", __func__, data, spa_fuelgauge_current);

	*vbat  = data;
	return 0;
}

static int spa_bat_read_soc(struct spa_driver_data *info, int *soc)
{
	int data = 0, ret = 0; 
	mutex_lock(&info->api_lock);
	if(info->battery.read_soc==NULL)
	{
		printk(" %s   read_soc is NULL\n", __func__);
		*soc = 0;
		mutex_unlock(&info->api_lock);
		return -1;
	}
	ret = info->battery.read_soc(&data);
	mutex_unlock(&info->api_lock);
	if(ret < 0){
		printk(" %s   fail to read soc\n", __func__);
		*soc = 0;
		return -1; 
	}

	printk("%s soc [%d]\n", __func__, data);	
	*soc = data;
	return 0;
}

static int spa_bat_read_temperature(struct spa_driver_data *info, int *temperature)
{
	int data = 0, ret = 0; 
	mutex_lock(&info->api_lock);
	if(info->battery.read_temperature==NULL)
	{
		printk(" %s   read_temperature is NULL\n", __func__);
		*temperature = 0;
		mutex_unlock(&info->api_lock);
		return -1;
	}
	ret = info->battery.read_temperature(&data);
	mutex_unlock(&info->api_lock);
	if(ret < 0){
		printk(" %s   fail to read_temperature\n", __func__);
		*temperature = 0;
		return -1; 
	}

	*temperature = data;
	return 0;
}

static int spa_bat_read_VF(struct spa_driver_data *info, unsigned int *VF)
{
	unsigned int data = 0; 
	int ret = 0;
	mutex_lock(&info->api_lock);
	if(info->battery.read_VF==NULL)
	{
		printk(" %s   read_VF is NULL\n", __func__);
		*VF = 0;
		mutex_unlock(&info->api_lock);
		return -1;
	}
	ret = info->battery.read_VF(&data);
	mutex_unlock(&info->api_lock);
	if(ret < 0){
		printk(" %s   fail to read_VF\n", __func__);
		*VF = 0;
		return -1; 
	}

	*VF = data;
	return 0;
}
#if 0
static int spa_bat_power_down(struct spa_driver_data *info)
{
	mutex_lock(&info->api_lock);
	if(info->battery.power_down==NULL)
	{
		printk(" %s   power_down is NULL\n", __func__);
		mutex_unlock(&info->api_lock);
		return -1;
	}
	info->battery.power_down();
	mutex_unlock(&info->api_lock);

	return 0;
}
#endif
static int spa_chg_get_current_charger(struct spa_driver_data *info)
{
	int current_charger = SPA_CHARGER_TYPE_NONE;

	mutex_lock(&info->lock);
	current_charger = info->charger.current_charger;
	mutex_unlock(&info->lock);

	return current_charger;
}

static void spa_bat_fuelgauge_reset(struct spa_driver_data *info)
{
	if(info->use_fuelgauge)
	{
		printk(" %s  \n", __func__);
		mutex_lock(&info->api_lock);
		if(info->battery.fuelgauge_reset!=NULL)
		{
			if(info->battery.fuelgauge_reset() != 0)
				{
				printk(" %s   fuelgauge reset fail!\n", __func__);
				}
			else
				{
				printk(" %s   fuelgauge reset success!\n", __func__);
				}
		}
		else
		{
			printk(" %s   info->battery.fuelgauge_reset is NULL\n", __func__);
		}
		mutex_unlock(&info->api_lock);
	}
	else
	{
		printk(" %s   info->use_fuelgauge is 0\n", __func__);
	}
}
static int spa_bat_get_property(struct power_supply *bat_ps,
				enum power_supply_property psp,
				union power_supply_propval *val)
{
	struct spa_driver_data *info = dev_get_drvdata(bat_ps->dev->parent);

	switch(psp){
		case POWER_SUPPLY_PROP_STATUS:
			val->intval = spa_bat_get_status(info);
			break;
		case POWER_SUPPLY_PROP_HEALTH:
			val->intval = spa_bat_get_health(info);
			break;
		case POWER_SUPPLY_PROP_PRESENT:
			mutex_lock(&info->lock);
			val->intval = 1; 
			mutex_unlock(&info->lock);
			break;
		case POWER_SUPPLY_PROP_CAPACITY:
			val->intval = spa_bat_get_capacity(info);
			break;
		case POWER_SUPPLY_PROP_TECHNOLOGY:
			val->intval = POWER_SUPPLY_TECHNOLOGY_LION;
			break;
		case POWER_SUPPLY_PROP_CURRENT_NOW:
			break;
		case POWER_SUPPLY_PROP_VOLTAGE_NOW:
			val->intval = spa_bat_get_current_voltage(info) * 1000;
			break;
		case POWER_SUPPLY_PROP_BATT_VOL:
			val->intval = spa_bat_get_current_voltage(info);
			break;
		case POWER_SUPPLY_PROP_BATT_VOL_AVER:
			val->intval = spa_bat_get_average_voltage(info);
			break;
		case POWER_SUPPLY_PROP_TEMP:
		case POWER_SUPPLY_PROP_BATT_TEMP:
			val->intval = spa_bat_get_current_temperature(info)/100000;
			break;
		case POWER_SUPPLY_PROP_TEMP_ADC:
		case POWER_SUPPLY_PROP_BATT_TEMP_ADC:
			val->intval = 1400 - spa_bat_get_current_temp_adc(info);
			break;
		case POWER_SUPPLY_PROP_BATT_TEMP_AVER:
			val->intval = spa_bat_get_average_temperature(info)/100000;
			break;
		case POWER_SUPPLY_PROP_BATT_TEMP_ADC_AVER:
			val->intval = 1400 - spa_bat_get_average_temp_adc(info);
			break;
		case POWER_SUPPLY_PROP_BATT_CHARGING_SOURCE:
			if(spa_chg_get_current_charger(ginfo) == SPA_CHARGER_TYPE_NONE)
				val->intval = POWER_SUPPLY_TYPE_BATTERY;
			else
				val->intval = POWER_SUPPLY_TYPE_MAINS;
			break;	
		case POWER_SUPPLY_PROP_BATT_READ_RAW_SOC:
				val->intval = spa_bat_get_capacity(info);;
			break;
		default:
			return -EINVAL;
	}
	return 0;
}

static enum power_supply_property spa_bat_main_props[] = {
        POWER_SUPPLY_PROP_STATUS,
        POWER_SUPPLY_PROP_HEALTH,
        POWER_SUPPLY_PROP_PRESENT,
        POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_TECHNOLOGY,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_TEMP,
	POWER_SUPPLY_PROP_TEMP_ADC,

        POWER_SUPPLY_PROP_BATT_VOL,
        POWER_SUPPLY_PROP_BATT_VOL_AVER,
        POWER_SUPPLY_PROP_BATT_TEMP,
        POWER_SUPPLY_PROP_BATT_TEMP_ADC,
        POWER_SUPPLY_PROP_BATT_TEMP_AVER,
        POWER_SUPPLY_PROP_BATT_TEMP_ADC_AVER,
        POWER_SUPPLY_PROP_BATT_CHARGING_SOURCE,
        POWER_SUPPLY_PROP_BATT_READ_RAW_SOC,
};

static void spa_chg_set_current_charger(struct spa_driver_data *info, int new_charger)
{
	mutex_lock(&info->lock);
	info->charger.current_charger = new_charger;
	mutex_unlock(&info->lock);
}

static int spa_chg_get_jig_connected(struct spa_driver_data *info)
{
	int jig_connected = 0;
	mutex_lock(&info->lock);
	jig_connected = info->charger.jig_connected;
	mutex_unlock(&info->lock);

	return jig_connected;
}

static void spa_chg_set_jig_connected(struct spa_driver_data *info, int jig_connected)
{
	mutex_lock(&info->lock);
	info->charger.jig_connected = jig_connected;
	mutex_unlock(&info->lock);
}
#if defined(CONFIG_SPA_FUEL_GAUGE_TEST)
static void spa_chg_set_u200_ta_connected(struct spa_driver_data *info, int u200_ta_connected)
{
	printk(" %s   %d\n", __func__, u200_ta_connected);
	mutex_lock(&info->lock);
	info->charger.u200_ta_connected = u200_ta_connected;
	mutex_unlock(&info->lock);
}
#endif
static int spa_chg_get_u200_ta_connected(struct spa_driver_data *info)
{
	int u200_ta_connected = 0;
	mutex_lock(&info->lock);
	u200_ta_connected = info->charger.u200_ta_connected;
	mutex_unlock(&info->lock);

	return u200_ta_connected;
}

static int spa_power_get_property(struct power_supply *psy,
                                  enum power_supply_property psp,
                                  union power_supply_propval *val)
{
	struct spa_driver_data *info = dev_get_drvdata(psy->dev->parent);

        switch (psp)
        {
                case POWER_SUPPLY_PROP_ONLINE:
                        if (psy->type == POWER_SUPPLY_TYPE_MAINS)
                                val->intval = (spa_chg_get_current_charger(info)==SPA_CHARGER_TYPE_TA)? 1 : 0 ;
                        else
                                val->intval = (spa_chg_get_current_charger(info)==SPA_CHARGER_TYPE_USB)? 1 : 0 ;
                        break;
                default:
                        return -EINVAL;
        }
        return 0;
}


static enum power_supply_property spa_power_props[] = {
        POWER_SUPPLY_PROP_ONLINE,
};

static char *spa_power_supplied_to[] = {
        "main-battery",
        "backup-battery",
};

static void spa_bat_external_power_changed(struct power_supply *bat_ps)
{
        printk(" %s  \n",__func__);
}

static struct power_supply spa_psy_ac = {
        .name                                   = "ac",
        .type                                   = POWER_SUPPLY_TYPE_MAINS,
        .supplied_to                    = spa_power_supplied_to,
        .num_supplicants                = ARRAY_SIZE(spa_power_supplied_to),
        .properties                     = spa_power_props,
        .num_properties                 = ARRAY_SIZE(spa_power_props),
        .get_property                   = spa_power_get_property,
        .external_power_changed = spa_bat_external_power_changed,
};

static struct power_supply spa_psy_usb = {
        .name                                   = "usb",
        .type                                   = POWER_SUPPLY_TYPE_USB,
        .supplied_to                    = spa_power_supplied_to,
        .num_supplicants                = ARRAY_SIZE(spa_power_supplied_to),
        .properties                     = spa_power_props,
        .num_properties                 = ARRAY_SIZE(spa_power_props),
        .get_property                   = spa_power_get_property,
        .external_power_changed = spa_bat_external_power_changed,
};

static struct power_supply spa_psy_fuelgauge = {
        .name                                   = "fuelgauge",
        .type                                   = POWER_SUPPLY_TYPE_BATTERY,
        .supplied_to                    = spa_power_supplied_to,
        .num_supplicants                = ARRAY_SIZE(spa_power_supplied_to),
        .properties                     = spa_power_props,
        .num_properties                 = ARRAY_SIZE(spa_power_props),
        .get_property                   = spa_power_get_property,
        .external_power_changed = spa_bat_external_power_changed,
};

struct power_supply spa_psy_bat = {
        .name                                   = "battery",
        .type                                   = POWER_SUPPLY_TYPE_BATTERY,
        .properties                             = spa_bat_main_props,
        .num_properties                 = ARRAY_SIZE(spa_bat_main_props),
        .get_property                   = spa_bat_get_property,
        .external_power_changed = spa_bat_external_power_changed,
        .use_for_apm                    = 1,
};

static unsigned char spa_bat_get_status(struct spa_driver_data *info)
{
	unsigned char status;

	mutex_lock(&info->lock);
	status = info->battery.status;
	mutex_unlock(&info->lock);	

	return status;
}

static void spa_bat_set_status(struct spa_driver_data *info, unsigned int status)
{
	mutex_lock(&info->lock);
	info->battery.status = status;
	mutex_unlock(&info->lock);

	 power_supply_changed(&spa_psy_bat);	
}

static int spa_chg_enable_charge(struct spa_driver_data *info)
{
	enum power_supply_type current_charger = POWER_SUPPLY_TYPE_MAINS;
	int ret = 0;

	if(spa_chg_get_current_charger(info) == SPA_CHARGER_TYPE_TA)
	{
		if(spa_chg_get_u200_ta_connected(info))
			current_charger = POWER_SUPPLY_TYPE_USB;
		else
			current_charger = POWER_SUPPLY_TYPE_MAINS;
	}
	else if(spa_chg_get_current_charger(info) == SPA_CHARGER_TYPE_USB)
		current_charger = POWER_SUPPLY_TYPE_USB;


#ifdef CONFIG_MACH_ARUBA_TD
	if (spa_call_status)
		current_charger = POWER_SUPPLY_TYPE_UPS;
#endif	

	mutex_lock(&info->api_lock);
	if(info->charger.enable_charge)
	{
		info->charger.enable_charge(current_charger);
	}
	else
	{
		ret = -1;
		printk(" %s   info->charger.enable_charge is NULL\n", __func__);
	}
	mutex_unlock(&info->api_lock);
	return ret; 
}

static int spa_chg_disable_charge(struct spa_driver_data *info, unsigned char end_of_charge)
{
	int ret = 0;

	mutex_lock(&info->api_lock);
	if(info->charger.disable_charge)
	{
		info->charger.disable_charge(end_of_charge);
	}
	else
	{
		ret = -1;
		printk(" %s   info->charger.disable_charge is NULL\n", __func__);
	}
	mutex_unlock(&info->api_lock);
	return ret; 
}

static void spa_chg_start_charge(struct spa_driver_data *info, unsigned int timer_type)
{
	unsigned int time = 0;

	printk(" %s  \n", __func__);

	if(spa_chg_enable_charge(info) < 0)
	{
		printk(" %s   fail to enable charge\n", __func__);
	}

	if(timer_pending(&info->Charge_timer))
	{
		printk(" %s   Charge_timer is running. delete Charge_timer\n", __func__);
		del_timer(&info->Charge_timer);
	}

	if(timer_pending(&info->reCharge_start_timer))
	{
		printk(" %s   reCharge_start_timer is running. delete reCharge_start_timer\n", __func__);
		if(spa_bat_check_end_of_charge(info, SPA_END_OF_CHARGE_BY_TIMER)==0)
		{
			spa_bat_clear_end_of_charge(info, SPA_END_OF_CHARGE_BY_TIMER);
		}
		del_timer(&info->reCharge_start_timer);
	}

	if(timer_type == SPA_CHARGE_START_TIMER)
#ifdef CONFIG_MACH_HENDRIX
		time = info->battery.charge_timer*HZ;
#else
	{
		if(info->battery.capacity < SPA_BATTERY_CAPACITY_1500MA)
			time = 5*60*60*HZ;
		else if(info->battery.capacity < SPA_BATTERY_CAPACITY_2000MA)
			time = 6*60*60*HZ;
		else if(info->battery.capacity < SPA_BATTERY_CAPACITY_4500MA)
			time = 8*60*60*HZ;
		else if(info->battery.capacity < SPA_BATTERY_CAPACITY_7000MA)
			time = 10*60*60*HZ;
		else
		{
			time =5*60*60*HZ;
			printk(" %s   wrong battery capacity %d\n", __func__, info->battery.capacity);
		}		
	}
#endif
	else if(timer_type == SPA_CHARGE_RESTART_TIMER)
		time = info->battery.recharge_timer*HZ;

	info->Charge_timer.expires = jiffies + time; 
	add_timer(&info->Charge_timer);	
}

static void spa_chg_stop_charge(struct spa_driver_data *info, unsigned char end_of_charge)
{
	printk(" %s  \n", __func__);
	if(spa_chg_disable_charge(info,end_of_charge) < 0)
	{
		printk(" %s   fail to disable_charge\n", __func__);
	}

	if(timer_pending(&info->Charge_timer))
	{
		printk(" %s   Charge_timer is running. delete Charge_timer\n", __func__);
		del_timer(&info->Charge_timer);
	}

	if(timer_pending(&info->reCharge_start_timer))
	{
		printk(" %s   reCharge_start_timer is running. delete reCharge_start_timer\n", __func__);
		if(spa_bat_check_end_of_charge(info, SPA_END_OF_CHARGE_BY_TIMER)==0)
		{
			spa_bat_clear_end_of_charge(info, SPA_END_OF_CHARGE_BY_TIMER);
		}
		del_timer(&info->reCharge_start_timer);
	}
}


static void spa_chg_TA_attached(struct spa_driver_data *info)
{
	if(spa_chg_get_current_charger(info)== SPA_CHARGER_TYPE_NONE)
        {
		printk(" %s  \n", __func__);
		wake_lock(&info->charger.charger_wakeup);
		spa_chg_set_current_charger(info, SPA_CHARGER_TYPE_TA);
		spa_chg_start_charge(info, SPA_CHARGE_START_TIMER);
		spa_bat_set_status(info, POWER_SUPPLY_STATUS_CHARGING); 
		power_supply_changed(&spa_psy_ac);
	}
}

static void spa_chg_TA_detached(struct spa_driver_data *info)
{
	if(spa_chg_get_current_charger(info)== SPA_CHARGER_TYPE_TA)
        {
		printk(" %s  \n", __func__);
		spa_chg_set_current_charger(info, SPA_CHARGER_TYPE_NONE);
		spa_bat_set_end_of_charge(info, SPA_END_OF_CHARGE_NONE);
		spa_bat_set_status(info, POWER_SUPPLY_STATUS_DISCHARGING);
		spa_bat_set_health(info, POWER_SUPPLY_HEALTH_GOOD);
		power_supply_changed(&spa_psy_ac);
		spa_chg_stop_charge(info, SPA_END_OF_CHARGE_NONE);
		wake_unlock(&info->charger.charger_wakeup);
	}
}

static void spa_chg_USB_attached(struct spa_driver_data *info)
{
	if(spa_chg_get_current_charger(info)== SPA_CHARGER_TYPE_NONE)
        {
		printk(" %s  \n", __func__);
		wake_lock(&info->charger.charger_wakeup);
		spa_chg_set_current_charger(info, SPA_CHARGER_TYPE_USB);
		spa_chg_start_charge(info, SPA_CHARGE_START_TIMER);
		spa_bat_set_status(info, POWER_SUPPLY_STATUS_CHARGING);
		power_supply_changed(&spa_psy_usb);
	}
}

static void spa_chg_USB_detached(struct spa_driver_data *info)
{
	if(spa_chg_get_current_charger(info)== SPA_CHARGER_TYPE_USB)
        {
		printk(" %s  \n", __func__);
		spa_chg_set_current_charger(info, SPA_CHARGER_TYPE_NONE);
		spa_bat_set_end_of_charge(info, SPA_END_OF_CHARGE_NONE);
		spa_bat_set_status(info, POWER_SUPPLY_STATUS_DISCHARGING);
		spa_bat_set_health(info, POWER_SUPPLY_HEALTH_GOOD);
		power_supply_changed(&spa_psy_usb);
		spa_chg_stop_charge(info, SPA_END_OF_CHARGE_NONE);
		wake_unlock(&info->charger.charger_wakeup);
	}
}

static void spa_chg_JIG_attached(struct spa_driver_data *info)
{
	printk(" %s  \n", __func__);
	spa_chg_set_jig_connected(info, 1);
}

static void spa_chg_JIG_detached(struct spa_driver_data *info)
{
	printk(" %s  \n", __func__);
	spa_chg_set_jig_connected(info, 0);
#if defined(CONFIG_SPA_FUEL_GAUGE_TEST)
	if(spa_chg_get_current_charger(info)==SPA_CHARGER_TYPE_TA)
	{
		if(spa_chg_get_u200_ta_connected(info))
			spa_chg_set_u200_ta_connected(info, 0);
		spa_chg_TA_detached(info);
	}
#endif
}

#if defined(CONFIG_SPA_FUEL_GAUGE_TEST)
static void spa_chg_fuel_gauge_TA_attached(struct spa_driver_data *info)
{
	if(spa_chg_get_current_charger(info)== SPA_CHARGER_TYPE_NONE)
	{
		printk(" %s  \n", __func__);
		if(!spa_chg_get_jig_connected(info))
		{
			spa_chg_set_u200_ta_connected(info, 1);
		}

		spa_chg_TA_attached(info);
	}
}

static void spa_chg_fuel_gauge_TA_detached(struct spa_driver_data *info)
{
	int current_charger = spa_chg_get_current_charger(info);

	if(spa_chg_get_jig_connected(info) && (current_charger==SPA_CHARGER_TYPE_TA))
	{
		printk(" %s   remove fuelgauge TA\n", __func__);
		spa_chg_TA_detached(info);
	}
	else if(spa_chg_get_u200_ta_connected(info))
	{
		printk(" %s   remove u200 TA\n", __func__);
		spa_chg_set_u200_ta_connected(info, 0);
		spa_chg_TA_detached(info);
	}
	else
	{
		if(spa_bat_check_end_of_charge(info, SPA_END_OF_CHARGE_BY_OVP)!=0)
		{
			if(current_charger==SPA_CHARGER_TYPE_TA)
			{
				printk(" %s   remove TA\n", __func__);
				spa_chg_TA_detached(info);
			}
			else if(current_charger==SPA_CHARGER_TYPE_USB)
			{
				printk(" %s   remove USB\n", __func__);
				spa_chg_USB_detached(info);
			}
		}
		else
		{
			printk(" %s   do not care vbus interrupt with OVP status\n", __func__);
		}
	}
}
#endif

static void spa_chg_fuel_start_quickstart(struct spa_driver_data *info)
{
	if(spa_chg_disable_charge(info, SPA_END_OF_CHARGE_BY_QUICKSTART) < 0)
	{
		printk(" %s   fail to disable_charge\n", __func__);
	}
}

static void spa_chg_fuel_end_quickstart(struct spa_driver_data *info)
{
	if(spa_chg_get_current_charger(info) != SPA_CHARGER_TYPE_NONE)
	{
		if(spa_chg_enable_charge(info) < 0)
		{
			printk(" %s   fail to enable charge\n", __func__);
		}	
	}
	else
	{
		printk(" %s   no charger\n", __func__);
	}
}

static void spa_bat_charge_full(struct spa_driver_data *info)
{
	int soc = 0;
	if(spa_chg_get_current_charger(info) != SPA_CHARGER_TYPE_NONE)
	{
		printk(" %s  \n", __func__);
		soc = spa_bat_get_capacity(info);	
		if((soc != 100) && (spa_bat_get_status(info)==POWER_SUPPLY_STATUS_CHARGING))
		{
			printk(" %s   reset soc value because soc level is not a 100 : %d\n", __func__, soc);
			if(info->use_fuelgauge)
			{
				spa_bat_fuelgauge_reset(info);
#if defined(CONFIG_STC3115_FUELGAUGE)
				mdelay(1000);
#endif
				if(spa_bat_read_soc(info, &soc) < 0)
				{
					printk(" %s   fail to read soc\n", __func__);

					if(spa_chg_disable_charge(info,SPA_END_OF_CHARGE_BY_FULL) < 0)
					{
						printk(" %s   fail to disable_charge\n", __func__);
					}
					if(spa_chg_enable_charge(info) < 0)
					{
						printk(" %s   fail to enable charge\n", __func__);
					}

					return;
				}
				else
				{
					/* update soc value if new soc value is 100 after reset */ 
					if(soc == 100)
					{
						mutex_lock(&info->lock);
						info->battery.soc = soc;
						mutex_unlock(&info->lock);
					}
					else
					{
						if(spa_chg_disable_charge(info,SPA_END_OF_CHARGE_BY_FULL) < 0)
						{
							printk(" %s   fail to disable_charge\n", __func__);
						}
						if(spa_chg_enable_charge(info) < 0)
						{
							printk(" %s   fail to enable charge\n", __func__);
						}

						return;
					}
				}
			}
		}

		spa_bat_set_end_of_charge(info, SPA_END_OF_CHARGE_BY_FULL);
		spa_bat_set_status(info, POWER_SUPPLY_STATUS_FULL);
		spa_chg_stop_charge(info, SPA_END_OF_CHARGE_BY_FULL);
		power_supply_changed(&spa_psy_bat); 
	}
	else
	{
		printk(" %s   do not make battery full because there is no charger\n", __func__);
	}
}

static void spa_bat_ovp_charge_stop(struct spa_driver_data *info)
{
	if(spa_chg_get_current_charger(info) != SPA_CHARGER_TYPE_NONE)
	{
		spa_bat_set_health(info, POWER_SUPPLY_HEALTH_OVERVOLTAGE);
		if(spa_bat_get_status(info) == POWER_SUPPLY_STATUS_CHARGING)
		{                                                          
			printk(" %s   stop charging by OVP\n", __func__);                                                         
			spa_bat_set_end_of_charge(info, SPA_END_OF_CHARGE_BY_OVP);          
			spa_bat_set_status(info, POWER_SUPPLY_STATUS_NOT_CHARGING);
			spa_chg_stop_charge(info, SPA_END_OF_CHARGE_BY_OVP);                         
		}                                                          
		else if((spa_bat_get_status(info) == POWER_SUPPLY_STATUS_NOT_CHARGING)
			|| (spa_bat_get_status(info) == POWER_SUPPLY_STATUS_FULL))                                                                     
		{       
			printk(" %s   already charging had been stopped\n", __func__);         
			spa_bat_set_end_of_charge(info, SPA_END_OF_CHARGE_BY_OVP);          
		}
	}
}

static void spa_bat_ovp_charge_restart(struct spa_driver_data *info)
{
	if(spa_chg_get_current_charger(info) != SPA_CHARGER_TYPE_NONE)
	{
		if(spa_bat_check_end_of_charge(info, SPA_END_OF_CHARGE_BY_OVP)==0)
		{
			printk(" %s   restart charge because device recorver OVP status\n", __func__);
			spa_bat_set_end_of_charge(info, SPA_END_OF_CHARGE_NONE);
			spa_bat_set_health(info, POWER_SUPPLY_HEALTH_GOOD);
			spa_bat_set_status(info, POWER_SUPPLY_STATUS_CHARGING);
			spa_chg_start_charge(info, SPA_CHARGE_START_TIMER);
		}
	}
}

static void spa_bat_VF_charge_stop(struct spa_driver_data *info)
{
	if(spa_chg_get_current_charger(info) != SPA_CHARGER_TYPE_NONE)
	{
		spa_bat_set_health(info, POWER_SUPPLY_HEALTH_UNSPEC_FAILURE);
		if(spa_bat_get_status(info) == POWER_SUPPLY_STATUS_CHARGING)
		{                                                          
			printk(" %s   stop charging by VF open\n", __func__);                                                         
			spa_bat_set_end_of_charge(info, SPA_END_OF_CHARGE_BY_VF_OPEN);          
			spa_bat_set_status(info, POWER_SUPPLY_STATUS_NOT_CHARGING);
			spa_chg_stop_charge(info, SPA_END_OF_CHARGE_BY_VF_OPEN);                         
		}                                                          
		else if((spa_bat_get_status(info) == POWER_SUPPLY_STATUS_NOT_CHARGING)
			|| (spa_bat_get_status(info) == POWER_SUPPLY_STATUS_FULL))                                                                     
		{       
			printk(" %s   already charging had been stopped\n", __func__);         
			spa_bat_set_end_of_charge(info, SPA_END_OF_CHARGE_BY_VF_OPEN);          
		}
	}
}

static void spa_bat_sleep_monitor(struct spa_driver_data *info)
{
	wake_lock_timeout(&info->battery.sleep_monitor_wakeup, SPA_SLEEP_MONITOR_WAKELOCK_TIME);

	schedule_delayed_work(&info->sleep_monitor_work, 0);
}

static void spa_monitor_voltage_work(struct work_struct *work)
{
	struct spa_driver_data *info = container_of(work, struct spa_driver_data, monitor_VOLT_work.work);
	static unsigned char vBatValueIndex = 0;
	int soc=0, vbat=0, ret=0;
	static int a_VbatValues[SPA_BAT_VOLTAGE_SAMPLE]={0,};
	unsigned char end_of_charge = 0;

	ret = spa_bat_read_voltage(info, &vbat);
	
	if(ret < 0)
	{
		printk(" %s   fail to read voltage\n", __func__);
		schedule_delayed_work(&info->monitor_VOLT_work, SPA_VOLTAGE_MONITOR_NORMAL);
		return;
	}
	
	mutex_lock(&info->lock);
	info->battery.current_voltage = vbat;
	mutex_unlock(&info->lock);

	if(info->use_fuelgauge)
	{
		ret = spa_bat_read_soc(info, &soc);
		if(ret < 0)
		{
			printk(" %s   fail to read soc\n", __func__);
			schedule_delayed_work(&info->monitor_VOLT_work, SPA_VOLTAGE_MONITOR_NORMAL);
			return;
		}

		if(soc <= 0)
		{
			soc = 0;

			if(spa_bat_get_current_voltage(info) >= SPA_POWER_OFF_VOLTAGE)
			{
				printk(" %s   current voltage is higher than %d mV\n", __func__, SPA_POWER_OFF_VOLTAGE);
				soc = 1;		
			}
		}
		else if(soc >= 100)
		{
			soc = 100;
		}
#if defined(CONFIG_SPA_POWER_OFF_BY_LOW_VOLTAGE)
		else
		{
			if(vBatValueIndex == SPA_BAT_VOLTAGE_SAMPLE)
			{
				if(spa_bat_get_average_voltage(info) <= SPA_POWER_OFF_VOLTAGE)
				{
					printk(" %s   average_voltage is lower than %d mV\n", __func__, SPA_POWER_OFF_VOLTAGE);
					soc = 0;	
				}
			}
		}
#endif

		mutex_lock(&info->lock);
		info->battery.soc = soc;
		mutex_unlock(&info->lock);

	}

	if((spa_chg_get_current_charger(info)!=SPA_CHARGER_TYPE_NONE) && (spa_bat_get_health(info) != POWER_SUPPLY_HEALTH_UNSPEC_FAILURE)
		&& (spa_chg_get_jig_connected(info)==0))
	{
		unsigned int batt_VF = 0;
		ret = spa_bat_read_VF(info, &batt_VF); 
		if(ret >= 0)
		{
			if(info->battery.VF_low >= info->battery.VF_high)
			{
                   printk(" %s   please check battery.VF_low(%d) and battery.VF_high(%d) in spa_platform_data\n",__func__, info->battery.VF_low, info->battery.VF_high);
			}
			else
			{
				if((batt_VF < info->battery.VF_low) || (batt_VF > info->battery.VF_high))
				{
                   printk(" %s   wrong battery detected. VF : %d, VF_low : %d, VF_high : %d\n", __func__, batt_VF, info->battery.VF_low, info->battery.VF_high);
					spa_bat_VF_charge_stop(info);
				}
			}
		}
		else
		{
			printk(" %s   fail to read_VF\n", __func__);
		}
	}

	if ((spa_chg_get_current_charger(info) != SPA_CHARGER_TYPE_NONE) && (spa_bat_get_status(info) == POWER_SUPPLY_STATUS_CHARGING) && (pre_charging_stop == 0))
	{
		if ((PRECHARGE_STOP_CURRENT_MIN < spa_fuelgauge_current) && (spa_fuelgauge_current < PRECHARGE_STOP_CURRENT_MAX))
		{
			if (soc == 100)
			{
				pre_charging_stop = 1;
				
				spa_bat_set_status(info, POWER_SUPPLY_STATUS_FULL);
				printk(" %s enter fake charging current [%d]\n",__func__, spa_fuelgauge_current);
			}
			else 
				printk(" %s fail to enter fake charging current [%d] soc [%d]\n",__func__, spa_fuelgauge_current, soc);
		}
	}
	
	if(vBatValueIndex < SPA_BAT_VOLTAGE_SAMPLE)
	{
		a_VbatValues[vBatValueIndex++] = vbat;	
		schedule_delayed_work(&info->monitor_VOLT_work, SPA_VOLTAGE_MONITOR_FAST);

		if(vBatValueIndex == SPA_BAT_VOLTAGE_SAMPLE)
		{
			mutex_lock(&info->lock);
			info->battery.average_voltage = spa_bat_average(a_VbatValues, SPA_BAT_VOLTAGE_SAMPLE);
			mutex_unlock(&info->lock);
			printk(" %s   average voltage : %d\n", __func__, info->battery.average_voltage);
			power_supply_changed(&spa_psy_bat);  	
			 
		}
		else  // do not check other condition before average voltage had been measured 
		{
			printk(" %s   voltage : %d\n", __func__, vbat);
			return; 
		}
	}
	else
	{
		spa_ShiftTableAndSetLastElt(a_VbatValues, SPA_BAT_VOLTAGE_SAMPLE, vbat);
		mutex_lock(&info->lock);
		info->battery.average_voltage = spa_bat_average(a_VbatValues, SPA_BAT_VOLTAGE_SAMPLE);
		mutex_unlock(&info->lock);
		if(spa_chg_get_current_charger(info) == SPA_CHARGER_TYPE_TA)
			schedule_delayed_work(&info->monitor_VOLT_work, SPA_VOLTAGE_MONITOR_FAST);
		else if(spa_chg_get_current_charger(info) == SPA_CHARGER_TYPE_USB)
			schedule_delayed_work(&info->monitor_VOLT_work, SPA_VOLTAGE_MONITOR_FAST_USB);
		else
			schedule_delayed_work(&info->monitor_VOLT_work, SPA_VOLTAGE_MONITOR_NORMAL);
	}

// check recharge condition 
	if((spa_bat_get_status(info) == POWER_SUPPLY_STATUS_FULL) && (spa_bat_get_average_voltage(info) < info->battery.charging_restart_voltage) && (spa_bat_check_end_of_charge(info, SPA_END_OF_CHARGE_BY_FULL)==0))
	{

		end_of_charge = spa_bat_clear_end_of_charge(info, SPA_END_OF_CHARGE_BY_FULL);

		if(end_of_charge == SPA_END_OF_CHARGE_NONE)
		{
		      
			printk(" %s   restart charge because voltage is lower than %dmv\n", __func__, info->battery.charging_restart_voltage);
			spa_chg_start_charge(info, SPA_CHARGE_RESTART_TIMER);
			
		}
		else
		{
			printk(" %s   can't restart charge because other reason %d\n", __func__, end_of_charge); 
		}
	}

}

static void spa_monitor_temperature_work(struct work_struct *work)
{
	struct spa_driver_data *info = container_of(work, struct spa_driver_data, monitor_TEMP_work.work);
	static unsigned char vTempValueIndex = 0;
	static int a_TempValues[SPA_BAT_TEMPERATURE_SAMPLE]={0,};
	static int a_TempADCValues[SPA_BAT_TEMPERATURE_SAMPLE] = {0,};
	int tbat = 0, temp_adc = 0, ret = 0;
	unsigned char end_of_charge = 0;

	ret = spa_bat_read_temperature(info, &temp_adc);
	if(ret < 0)
	{
		printk(" %s   fail to read_temperature\n",__func__);
		schedule_delayed_work(&info->monitor_TEMP_work, SPA_TEMPERATURE_MONITOR_NORMAL);
		return;
	}

#if TEMP_TEST_BIN
	temp_adc += spa_temp_adc_test;
#endif

	tbat =  spa_bat_calculate_temperature_from_adc(info, temp_adc);

	mutex_lock(&info->lock);
	info->battery.current_temp_adc = temp_adc; //ѹֵ
	info->battery.current_temperature = tbat; //¶
	mutex_unlock(&info->lock);

	if(vTempValueIndex < SPA_BAT_TEMPERATURE_SAMPLE)
	{
		a_TempADCValues[vTempValueIndex] = temp_adc; 
		a_TempValues[vTempValueIndex++] = tbat; 
		schedule_delayed_work(&info->monitor_TEMP_work, SPA_TEMPERATURE_MONITOR_FAST);
		// do not check other condition before average temperature had been measured 
		return; 
	}
	else
	{
		spa_ShiftTableAndSetLastElt(a_TempADCValues, SPA_BAT_TEMPERATURE_SAMPLE, temp_adc);
		spa_ShiftTableAndSetLastElt(a_TempValues, SPA_BAT_TEMPERATURE_SAMPLE, tbat);		
		mutex_lock(&info->lock);
		info->battery.average_temp_adc = spa_bat_average(a_TempADCValues, SPA_BAT_TEMPERATURE_SAMPLE);
		info->battery.average_temperature = spa_bat_average(a_TempValues, SPA_BAT_TEMPERATURE_SAMPLE);
		mutex_unlock(&info->lock);
		schedule_delayed_work(&info->monitor_TEMP_work, SPA_TEMPERATURE_MONITOR_NORMAL);
	}

	if((spa_bat_get_health(info) == POWER_SUPPLY_HEALTH_COLD) || (spa_bat_get_health(info) == POWER_SUPPLY_HEALTH_OVERHEAT))
	{
		if((spa_bat_get_average_temperature(info) <= info->battery.charging_restart_high_temp)
			 && (spa_bat_get_average_temperature(info) >= info->battery.charging_restart_low_temp))
		{
			spa_bat_set_health(info, POWER_SUPPLY_HEALTH_GOOD);
			if(spa_bat_check_end_of_charge(info, SPA_END_OF_CHARGE_BY_TEMPERATURE)==0)	
			{
				end_of_charge = spa_bat_clear_end_of_charge(info, SPA_END_OF_CHARGE_BY_TEMPERATURE);
				if(end_of_charge == SPA_END_OF_CHARGE_NONE)
				{
					printk(" %s   restart charge because device recorver temperature\n", __func__);
					spa_bat_set_status(info, POWER_SUPPLY_STATUS_CHARGING);
					spa_chg_start_charge(info, SPA_CHARGE_START_TIMER);
				}				
				else
				{
					printk(" %s   can't restart charge because other reason %d\n", __func__, end_of_charge); 
				}
			}
			else
			{
				printk(" %s   recorver the temperature\n", __func__);
			}
		}
		else
		{
			if(spa_bat_get_status(info) == POWER_SUPPLY_STATUS_CHARGING)
			{
				printk(" %s   stop charging. Insert TA during HEALTH_COLD or HEALTH_OVERHEAT\n", __func__);
				spa_bat_set_end_of_charge(info, SPA_END_OF_CHARGE_BY_TEMPERATURE);
				spa_bat_set_status(info, POWER_SUPPLY_STATUS_NOT_CHARGING);
				spa_chg_stop_charge(info, SPA_END_OF_CHARGE_BY_TEMPERATURE);
			}
		}
	}
	else
	{
		if((spa_chg_get_current_charger(info) != SPA_CHARGER_TYPE_NONE)  
			 && ((spa_bat_get_average_temperature(info) >= info->battery.charging_stop_high_temp) || 
			     (spa_bat_get_average_temperature(info) <= info->battery.charging_stop_low_temp)))
		{
			if(spa_bat_get_average_temperature(info) >= info->battery.charging_stop_high_temp)
				spa_bat_set_health(info, POWER_SUPPLY_HEALTH_OVERHEAT);
			else if(spa_bat_get_average_temperature(info) <= info->battery.charging_stop_low_temp)
				spa_bat_set_health(info, POWER_SUPPLY_HEALTH_COLD);

			if(spa_bat_get_status(info) == POWER_SUPPLY_STATUS_CHARGING)
			{
				printk(" %s   stop charging by HIGH_TEMPERATURE or LOW_TEMPERATURE\n", __func__);
				spa_bat_set_end_of_charge(info, SPA_END_OF_CHARGE_BY_TEMPERATURE);
				spa_bat_set_status(info, POWER_SUPPLY_STATUS_NOT_CHARGING);
				spa_chg_stop_charge(info, SPA_END_OF_CHARGE_BY_TEMPERATURE);
			}					
			else if((spa_bat_get_status(info) == POWER_SUPPLY_STATUS_NOT_CHARGING)
					|| (spa_bat_get_status(info) == POWER_SUPPLY_STATUS_FULL))
			{
				printk(" %s   already charging had been stopped\n", __func__);
				spa_bat_set_end_of_charge(info, SPA_END_OF_CHARGE_BY_TEMPERATURE);
			}
		}
	}
}

static void spa_info_notify_work(struct work_struct *work)
{
	struct spa_driver_data *info = container_of(work, struct spa_driver_data, info_notify_work.work);

	 power_supply_changed(&spa_psy_bat);
	schedule_delayed_work(&info->info_notify_work, SPA_NOTIFY_INTERVAL);
}

static void spa_external_event_handler(int category, int event)
{
	if(ginfo==NULL)
	{
		return;
	}

	switch(category)
	{
		case SPA_CATEGORY_DEVICE:
			switch(event)
			{
				case SPA_DEVICE_EVENT_TA_ATTACHED:
					spa_chg_TA_attached(ginfo);
					break;
				case SPA_DEVICE_EVENT_TA_DETACHED:
					spa_chg_TA_detached(ginfo);
					break;
				case SPA_DEVICE_EVENT_USB_ATTACHED:
					spa_chg_USB_attached(ginfo);
					break;
				case SPA_DEVICE_EVENT_USB_DETACHED:
					spa_chg_USB_detached(ginfo);
					break;
				case SPA_DEVICE_EVENT_JIG_ATTACHED:
					spa_chg_JIG_attached(ginfo);
					break;
				case SPA_DEVICE_EVENT_JIG_DETACHED:
					spa_chg_JIG_detached(ginfo);
					break;
#if defined(CONFIG_SPA_FUEL_GAUGE_TEST)
				case SPA_DEVICE_EVENT_FUEL_GAUGE_TA_ATTACHED:
					spa_chg_fuel_gauge_TA_attached(ginfo);
					break;
				case SPA_DEVICE_EVENT_FUEL_GAUGE_TA_DETACHED:
					spa_chg_fuel_gauge_TA_detached(ginfo);
					break;
#endif
				case SPA_DEVICE_EVENT_FUEL_START_QUICKSTART:
					spa_chg_fuel_start_quickstart(ginfo);	
					break;
				case SPA_DEVICE_EVENT_FUEL_END_QUICKSTART:
					spa_chg_fuel_end_quickstart(ginfo);
					break; 
				default:
					break;
			}
			break;
		case SPA_CATEGORY_BATTERY:
			switch(event)
			{
				case SPA_BATTERY_EVENT_CHARGE_FULL:
					spa_bat_charge_full(ginfo);
					break;
				case SPA_BATTERY_EVENT_OVP_CHARGE_STOP:
					spa_bat_ovp_charge_stop(ginfo);	
					break;
				case SPA_BATTERY_EVENT_OVP_CHARGE_RESTART:
					spa_bat_ovp_charge_restart(ginfo);
					break;
				case SPA_BATTERY_EVENT_SLEEP_MONITOR:
					spa_bat_sleep_monitor(ginfo);
					break;
				default:
					break;
			}
			break;
		default:
			break;
	}
}

void (*spa_get_external_event_handler(void))(int, int)
{
	return spa_external_event_handler;
}

int spa_bat_register_read_voltage(int (*read_voltage)(int *))
{
	struct spa_driver_data *info;	
	int soc;
	
	printk(" %s  \n", __func__);
	if(ginfo)
	{
		mutex_lock(&ginfo->api_lock);
		if(ginfo->battery.read_voltage != NULL)
		{
			printk(" %s   read_voltage is not NULL\n",__func__);
			mutex_unlock(&ginfo->api_lock);
			goto register_read_voltage_fail;
		}
		mutex_unlock(&ginfo->api_lock);
	}	
	else
	{
		printk(" %s   ginfo is NULL\n",__func__);
		goto register_read_voltage_fail;
	}

	mutex_lock(&ginfo->api_lock);
	ginfo->battery.read_voltage = read_voltage;
	mutex_unlock(&ginfo->api_lock);

	if (spa_lpm_charging_mode_get())
	{
		info = ginfo;
		spa_bat_read_soc(info, &soc);
		info->battery.soc = soc;

	        power_supply_changed(&spa_psy_bat);	
		schedule_delayed_work(&ginfo->monitor_VOLT_work, SPA_VOLTAGE_MONITOR_START*0.05);
	}
	else
        {
	schedule_delayed_work(&ginfo->monitor_VOLT_work, SPA_VOLTAGE_MONITOR_START);
	}
	return 0; 

register_read_voltage_fail:
	return -EINVAL;
}

int spa_bat_unregister_read_voltage(int (*read_voltage)(int *))
{
	printk(" %s  \n", __func__);
	if(ginfo)
	{
		mutex_lock(&ginfo->api_lock);
		if(ginfo->battery.read_voltage != read_voltage)
		{
			printk(" %s   unregister failed\n", __func__);
			mutex_unlock(&ginfo->api_lock);
			goto unregister_read_voltage_fail; 
		}
		mutex_unlock(&ginfo->api_lock);
	}
	else
	{
		printk(" %s   ginfo is NULL\n", __func__);
		goto unregister_read_voltage_fail;	
	}

	mutex_lock(&ginfo->api_lock);
	ginfo->battery.read_voltage = NULL; 
	mutex_unlock(&ginfo->api_lock);
	
	return 0; 

unregister_read_voltage_fail:
	return -EINVAL;
}


int spa_bat_register_read_soc(int (*read_soc)(int *))
{
	printk(" %s  \n", __func__);
	if(ginfo)
	{
		if(ginfo->use_fuelgauge)
		{
			mutex_lock(&ginfo->api_lock);
			if(ginfo->battery.read_soc != NULL)
			{
				printk(" %s   read_soc is not NULL\n", __func__);
				mutex_unlock(&ginfo->api_lock);
				goto register_read_soc_fail;
			}
			mutex_unlock(&ginfo->api_lock);
		}
		else
		{
			printk(" %s   use_fuelgauge is 0\n",__func__);
			goto register_read_soc_fail;
		}
	}	
	else
	{
		printk(" %s   ginfo is NULL\n",__func__);
		goto register_read_soc_fail;
	}

	mutex_lock(&ginfo->api_lock);
	ginfo->battery.read_soc = read_soc;
	mutex_unlock(&ginfo->api_lock);
	return 0; 

register_read_soc_fail:
	return -EINVAL;
}

int spa_bat_unregister_read_soc(int (*read_soc)(int *))
{
	printk(" %s  \n", __func__);
	if(ginfo)
	{
		if(ginfo->use_fuelgauge)
		{
			mutex_lock(&ginfo->api_lock);
			if(ginfo->battery.read_soc != read_soc)
			{
				printk(" %s   unregister failed\n", __func__);	
				mutex_unlock(&ginfo->api_lock);
				goto unregister_read_soc_fail;
			}
			mutex_unlock(&ginfo->api_lock);
		}
		else
		{
			printk(" %s   use_fuelgauge is 0\n",__func__);
			goto unregister_read_soc_fail;
		}
	}	
	else
	{
		printk(" %s   ginfo is NULL\n",__func__);
		goto unregister_read_soc_fail;
	}

	mutex_lock(&ginfo->api_lock);
	ginfo->battery.read_soc = NULL;
	mutex_unlock(&ginfo->api_lock);
	return 0; 

unregister_read_soc_fail:
	return -EINVAL;
}


int spa_bat_register_read_temperature(int (*read_temperature)(int *))
{
	if(ginfo)
	{
		mutex_lock(&ginfo->api_lock);
		if(ginfo->battery.read_temperature != NULL)
		{
			printk(" %s   read_temperature is not NULL\n",__func__);
			mutex_unlock(&ginfo->api_lock);
			goto register_read_temperature_fail;
		}
		mutex_unlock(&ginfo->api_lock);
	}	
	else
	{
		printk(" %s   ginfo is NULL\n",__func__);
		goto register_read_temperature_fail;
	}

	mutex_lock(&ginfo->api_lock);
	ginfo->battery.read_temperature = read_temperature;
	mutex_unlock(&ginfo->api_lock);
	schedule_delayed_work(&ginfo->monitor_TEMP_work, SPA_TEMPERATURE_MONITOR_START);
	return 0; 

register_read_temperature_fail:
	return -EINVAL;
}

int spa_bat_register_read_VF(int (*read_VF)(unsigned int *))
{
	printk(" %s  \n", __func__);
	if(ginfo)
	{
		mutex_lock(&ginfo->api_lock);
		if(ginfo->battery.read_VF != NULL)
		{
			printk(" %s   read_VF is not NULL\n",__func__);
		mutex_unlock(&ginfo->api_lock);
			goto register_read_VF_fail;
		}
		mutex_unlock(&ginfo->api_lock);
	}	
	else
	{
		printk(" %s   ginfo is NULL\n",__func__);
		goto register_read_VF_fail;
	}

	mutex_lock(&ginfo->api_lock);
	ginfo->battery.read_VF = read_VF;
	mutex_unlock(&ginfo->api_lock);
	return 0; 

register_read_VF_fail:
	return -EINVAL;
}

int spa_bat_register_fuelgauge_reset(int (*fuelgauge_reset)(void))
{
	printk(" %s  \n", __func__);
	if(ginfo)
	{
		mutex_lock(&ginfo->api_lock);
		if(ginfo->battery.fuelgauge_reset != NULL)
		{
			printk(" %s   fg_rest is not NULL\n",__func__);
			mutex_unlock(&ginfo->api_lock);
			goto register_fuelgauge_reset_fail;
		}
		mutex_unlock(&ginfo->api_lock);
	}	
	else
	{
		printk(" %s   ginfo is NULL\n",__func__);
		goto register_fuelgauge_reset_fail;
	}

	mutex_lock(&ginfo->api_lock);
	ginfo->battery.fuelgauge_reset = fuelgauge_reset;
	mutex_unlock(&ginfo->api_lock);
	
	return 0; 

register_fuelgauge_reset_fail:
	return -EINVAL;
}

int spa_bat_unregister_fuelgauge_reset(int (*fuelgauge_reset)(void))
{
	printk(" %s  \n", __func__);
	if(ginfo)
	{
		mutex_lock(&ginfo->api_lock);
		if(ginfo->battery.fuelgauge_reset != fuelgauge_reset)
		{
			printk(" %s   unregister failed\n", __func__);
			mutex_unlock(&ginfo->api_lock);
			goto unregister_fuelgauge_reset_fail; 
		}
		mutex_unlock(&ginfo->api_lock);
	}
	else
	{
		printk(" %s   ginfo is NULL\n", __func__);
		goto unregister_fuelgauge_reset_fail;	
	}

	mutex_lock(&ginfo->api_lock);
	ginfo->battery.fuelgauge_reset = NULL; 
	mutex_unlock(&ginfo->api_lock);
	
	return 0; 

unregister_fuelgauge_reset_fail:
	return -EINVAL;
}

int spa_bat_register_power_down(void (*power_down)(void))
{
	printk(" %s  \n", __func__);
	if(ginfo)
	{
		mutex_lock(&ginfo->api_lock);
		if(ginfo->battery.power_down != NULL)
		{
			printk(" %s   power_down is not NULL\n",__func__);
			mutex_unlock(&ginfo->api_lock);
			goto register_power_down_fail;
		}
		mutex_unlock(&ginfo->api_lock);
	}	
	else
	{
		printk(" %s   ginfo is NULL\n",__func__);
		goto register_power_down_fail;
	}

	mutex_lock(&ginfo->api_lock);
	ginfo->battery.power_down = power_down;
	mutex_unlock(&ginfo->api_lock);
	return 0; 

register_power_down_fail:
	return -EINVAL;
}

int spa_chg_register_enable_charge(void (*enable_charge)(enum power_supply_type))
{
	if(ginfo)
	{
		mutex_lock(&ginfo->api_lock);
		if(ginfo->charger.enable_charge != NULL)
		{
			printk(" %s   enable_charge is not NULL\n",__func__);
			mutex_unlock(&ginfo->api_lock);
			goto register_enable_charge_fail;
		}
		mutex_unlock(&ginfo->api_lock);
	}	
	else
	{
		printk(" %s   ginfo is NULL\n",__func__);
		goto register_enable_charge_fail;
	}

	mutex_lock(&ginfo->api_lock);
	ginfo->charger.enable_charge = enable_charge;
	mutex_unlock(&ginfo->api_lock);

	spa_chg_enable_charge(ginfo);
	
	return 0; 

register_enable_charge_fail:
	return -EINVAL;
}

int spa_chg_unregister_enable_charge(void (*enable_charge)(enum power_supply_type))
{
         if(ginfo)
        {
		mutex_lock(&ginfo->api_lock);
		if(ginfo->charger.enable_charge != enable_charge)
		{
			printk(" %s   unregister failed\n", __func__);
			mutex_unlock(&ginfo->api_lock);
			goto unregister_enable_charge_fail;
		}
		mutex_unlock(&ginfo->api_lock);
	}
        else
        {
                printk(" %s   ginfo is NULL\n",__func__);
                goto unregister_enable_charge_fail;
        }

        mutex_lock(&ginfo->api_lock);
        ginfo->charger.enable_charge = NULL;
        mutex_unlock(&ginfo->api_lock);
        return 0;

unregister_enable_charge_fail:
        return -EINVAL;
}



int spa_chg_register_disable_charge(void (*disable_charge)(unsigned char))
{
	if(ginfo)
	{
		mutex_lock(&ginfo->api_lock);
		if(ginfo->charger.disable_charge != NULL)
		{
			printk(" %s   disable_charge is not NULL\n",__func__);
			mutex_unlock(&ginfo->api_lock);
			goto register_disable_charge_fail;
		}
		mutex_unlock(&ginfo->api_lock);
	}	
	else
	{
		printk(" %s   ginfo is NULL\n",__func__);
		goto register_disable_charge_fail;
	}

	mutex_lock(&ginfo->api_lock);
	ginfo->charger.disable_charge = disable_charge;
	mutex_unlock(&ginfo->api_lock);

	if(spa_chg_get_current_charger(ginfo) == SPA_CHARGER_TYPE_NONE)
		disable_charge(SPA_END_OF_CHARGE_NONE);

	return 0; 

register_disable_charge_fail:
	return -EINVAL;
}

int spa_chg_unregister_disable_charge(void (*disable_charge)(unsigned char))
{
        if(ginfo)
        {
                mutex_lock(&ginfo->api_lock);
                if(ginfo->charger.disable_charge != disable_charge)
                {
                        printk(" %s   unregister failed\n", __func__);
                        mutex_unlock(&ginfo->api_lock);
                        goto unregister_disable_charge_fail;
                }
                mutex_unlock(&ginfo->api_lock);
        }
        else
        {
                printk(" %s   ginfo is NULL\n",__func__);
                goto unregister_disable_charge_fail;
        }

        mutex_lock(&ginfo->api_lock);
        ginfo->charger.disable_charge = NULL;
        mutex_unlock(&ginfo->api_lock);
        return 0;

unregister_disable_charge_fail:
        return -EINVAL;
}



static void spa_bat_charge_timer_work(struct work_struct *work)
{
	struct spa_driver_data *info = container_of(work, struct spa_driver_data, charge_timer_work.work);

	printk(" %s  \n", __func__);

	spa_bat_set_end_of_charge(info, SPA_END_OF_CHARGE_BY_TIMER);
	spa_bat_set_status(info, POWER_SUPPLY_STATUS_FULL);
	spa_chg_stop_charge(info, SPA_END_OF_CHARGE_BY_TIMER);

	info->reCharge_start_timer.expires = jiffies + SPA_RECHARGE_CHECK_TIMER_30SEC;
	add_timer(&info->reCharge_start_timer);
	
}

static void spa_bat_charge_timer_expired(unsigned long data)
{
	struct spa_driver_data *info = (struct spa_driver_data*)data; 
	
	printk(" %s  \n", __func__);

	schedule_delayed_work(&info->charge_timer_work, 0);

}

static void spa_bat_recharge_start_timer_work(struct work_struct *work)
{
	struct spa_driver_data *info = container_of(work, struct spa_driver_data, recharge_start_timer_work.work);
	unsigned char end_of_charge = 0;	

	printk(" %s  \n", __func__);

	if(spa_bat_check_end_of_charge(info, SPA_END_OF_CHARGE_BY_TIMER)==0)
	{
		end_of_charge = spa_bat_clear_end_of_charge(info, SPA_END_OF_CHARGE_BY_TIMER);
		if(end_of_charge == SPA_END_OF_CHARGE_NONE)
		{
			if((spa_bat_get_status(info) == POWER_SUPPLY_STATUS_FULL) && (spa_bat_get_average_voltage(info) < info->battery.charging_restart_voltage))
			{
				printk(" %s   restart charge because voltage is lower than %dmv\n", __func__, info->battery.charging_restart_voltage);
				spa_chg_start_charge(info, SPA_CHARGE_RESTART_TIMER);
			}
			else
			{
				printk(" %s   set SPA_END_OF_CHARGE_BY_FULL because voltage is higher than %dmv\n", __func__, info->battery.charging_restart_voltage);
				spa_bat_set_end_of_charge(info, SPA_END_OF_CHARGE_BY_FULL);	
			}
		}
		else
		{
			printk(" %s   can't restart charge because other reason %d\n", __func__, end_of_charge);	
		}
	
	
	}
	else
	{
		printk(" %s   SPA_END_OF_CHARGE_BY_TIMER had been clear by other reason \n", __func__); 
	}
}

static void spa_bat_sleep_monitor_work(struct work_struct *work)
{
	struct spa_driver_data *info = container_of(work, struct spa_driver_data, sleep_monitor_work.work);

	if(schedule_delayed_work(&info->monitor_VOLT_work, 0)==0)
	{
		cancel_delayed_work_sync(&info->monitor_VOLT_work);
		schedule_delayed_work(&info->monitor_VOLT_work, 0);
	}
	if(schedule_delayed_work(&info->monitor_TEMP_work, 0)==0)
	{
		cancel_delayed_work_sync(&info->monitor_TEMP_work);
		schedule_delayed_work(&info->monitor_TEMP_work, 0);
	}
	if(schedule_delayed_work(&info->info_notify_work, 0)==0)
	{
		cancel_delayed_work_sync(&info->info_notify_work);
		schedule_delayed_work(&info->info_notify_work, 0);
	}
	
}

static void spa_bat_recharge_timer_expired(unsigned long data)
{
	struct spa_driver_data *info = (struct spa_driver_data*)data; 

	printk(" %s  \n", __func__);

	schedule_delayed_work(&info->recharge_start_timer_work, 0);
}

static ssize_t batttest_show_attrs(struct device *dev,
                                              struct device_attribute *attr,
                                              char *buf)
{
        int voltage=0,ret=0;
        char batt_status[5]={'\0',};
        struct spa_driver_data *info = dev_get_drvdata(dev);

	if(spa_chg_get_current_charger(info)!=SPA_CHARGER_TYPE_NONE)
	{
		memcpy(batt_status, "CHAR", sizeof("CHAR"));
	}
	else if(spa_chg_get_jig_connected(info))
	{
		memcpy(batt_status, "JIGO", sizeof("JIGO"));
	}
	else
	{
		unsigned char batt_present = 0;
		if((info->battery.read_VF!=NULL) && (info->battery.VF_low < info->battery.VF_high))
		{
			unsigned int batt_VF = 0;
			if(info->battery.read_VF(&batt_VF)==0)
			{
				if((info->battery.VF_low <= batt_VF) && (batt_VF <= info->battery.VF_high))
				{
					batt_present = 1;
				}
			}
		}

		if(batt_present)
                        memcpy(batt_status,"BAOK",sizeof("BAOK"));
                else
                        memcpy(batt_status,"BANO",sizeof("BANO"));
			
	}
	
	spa_bat_fuelgauge_reset(info);
	mdelay(1000);
	ret = spa_bat_read_voltage(info, &voltage);
	mdelay(2000);
	ret = spa_bat_read_voltage(info, &voltage);

        if(ret < 0)
                return snprintf(buf, PAGE_SIZE, "%s 0000\n",batt_status);

        return snprintf(buf, PAGE_SIZE, "%s %d\n", batt_status, voltage);
}

static ssize_t batgetlevel_show_attrs(struct device *dev,
						struct device_attribute *attr,
						char *buf)
{
	int vbat = 0, level = 0, ret = 0, soc = 0;
	struct spa_driver_data *info = dev_get_drvdata(dev);

	ret = spa_bat_read_voltage(info, &vbat);
	if(ret < 0)
		vbat = 0;
	
	if(info->use_fuelgauge)
	{
		ret = spa_bat_read_soc(info, &soc);
		if(ret < 0)
			soc = 0;
	}
	else
	{
	}

	if((0 <= soc)&& (soc < 5))
		level = 0;
	if((5 <= soc)&& (soc < 20))
		level = 0;
	if((20 <= soc)&& (soc < 35))
		level = 1;
	if((35 <= soc)&& (soc < 50))
		level = 2;
	if((50 <= soc)&& (soc < 65))
		level = 3;
	if((65 <= soc)&& (soc < 80))
		level = 4;
	if((80 <= soc)&& (soc <= 100))
		level = 5;

	return snprintf(buf, PAGE_SIZE, "BATGETLEVEL=%d VOLT=%dmV", level, vbat);
}

static ssize_t fuelgaic_show_attrs(struct device *dev,
                                              struct device_attribute *attr,
                                              char *buf)
{
        int data = 0;
        struct spa_driver_data *info = dev_get_drvdata(dev);
		
	if(info->use_fuelgauge)
	{
		data = spa_bat_get_capacity(info);
	}

        if(data < 0)
                data=0;
        else if(data > 100)
                data = 100;

        return snprintf(buf, PAGE_SIZE, "%d",data);
}

static ssize_t fuelgaic_store_attrs(struct device *dev,
                                              struct device_attribute *attr,
                                              const char *buf, size_t count)
{
        struct spa_driver_data *info = dev_get_drvdata(dev);

	if(info->use_fuelgauge)
	{
		spa_bat_fuelgauge_reset(info);
#if defined(CONFIG_STC3115_FUELGAUGE)
		mdelay(1000);
#endif
	}
		
        return 1;
}

#ifdef CONFIG_MACH_ARUBA_TD
static ssize_t call_status_store_attrs(struct device *dev,
                                              struct device_attribute *attr,
                                              const char *buf, size_t count)
{
	struct spa_driver_data *info = dev_get_drvdata(dev);
	int value, ret;
		
	sscanf(buf, "%d", &value);

	printk("%s status [%d]\n", __func__, value);

	if (value == 1)
	{
		spa_call_status = 1;
		
		if((spa_chg_get_current_charger(info) == SPA_CHARGER_TYPE_TA) || (spa_chg_get_current_charger(info) == SPA_CHARGER_TYPE_USB))
		{
			printk("%s call start\n", __func__);

			mutex_lock(&info->api_lock);
			if(info->charger.enable_charge)
			{
				info->charger.enable_charge(POWER_SUPPLY_TYPE_UPS);
			}
			else
			{
				ret = -1;
				printk(" %s   info->charger.enable_charge is NULL\n", __func__);
			}
			mutex_unlock(&info->api_lock);			
		}
	}
	else
	{
		spa_call_status = 0;		

		mutex_lock(&info->api_lock);
		if(info->charger.enable_charge)
		{
			if (spa_chg_get_current_charger(info) == SPA_CHARGER_TYPE_TA)
				info->charger.enable_charge(POWER_SUPPLY_TYPE_MAINS);
			else
				info->charger.enable_charge(POWER_SUPPLY_TYPE_USB);
		}
		else
		{
			ret = -1;
			printk(" %s   info->charger.enable_charge is NULL\n", __func__);
		}
		mutex_unlock(&info->api_lock);	
	}
	
#if 0	
	if((spa_chg_get_current_charger(info) == SPA_CHARGER_TYPE_TA) || (spa_chg_get_current_charger(info) == SPA_CHARGER_TYPE_USB))
	{
		if (value)
		{
			printk("%s call start\n", __func__);

			mutex_lock(&info->api_lock);
			if(info->charger.enable_charge)
			{
				info->charger.enable_charge(POWER_SUPPLY_TYPE_UPS);
			}
			else
			{
				ret = -1;
				printk(" %s   info->charger.enable_charge is NULL\n", __func__);
			}
			mutex_unlock(&info->api_lock);			
		}
		else
		{
			mutex_lock(&info->api_lock);
			if(info->charger.enable_charge)
			{
				if (spa_chg_get_current_charger(info) == SPA_CHARGER_TYPE_TA)
					info->charger.enable_charge(POWER_SUPPLY_TYPE_MAINS);
				else
					info->charger.enable_charge(POWER_SUPPLY_TYPE_USB);
			}
			else
			{
				ret = -1;
				printk(" %s   info->charger.enable_charge is NULL\n", __func__);
			}
			mutex_unlock(&info->api_lock);	
		}
	}
#endif
	
	return 1;
}
#endif

ssize_t usb_state_show_attrs(struct device *dev, struct device_attribute *attr, char *buf)
{
	if(spa_chg_get_current_charger(ginfo) == SPA_CHARGER_TYPE_USB)
		return sprintf(buf, "USB_STATE_CONFIGURED\n");		
	else
		return sprintf(buf, "USB_STATE_NOTCONFIGURED\n");
}

static DEVICE_ATTR(batttest, S_IRUGO, batttest_show_attrs, NULL);
static DEVICE_ATTR(batgetlevel, S_IRUSR, batgetlevel_show_attrs, NULL);
static DEVICE_ATTR(fuelgaic, S_IRUGO | S_IWUGO, fuelgaic_show_attrs, fuelgaic_store_attrs);
#ifdef CONFIG_MACH_ARUBA_TD
static DEVICE_ATTR(call_status, S_IRUGO | S_IWUSR | S_IWGRP, NULL, call_status_store_attrs);
#endif

static struct attribute *battery_attributes[] = {
        &dev_attr_fuelgaic.attr,
	&dev_attr_batttest.attr,
	&dev_attr_batgetlevel.attr,
#ifdef CONFIG_MACH_ARUBA_TD
	&dev_attr_call_status.attr,
#endif
	NULL,
};

static struct attribute_group battery_attr_group = {
        .attrs = battery_attributes,
};

#if defined(CONFIG_SPA_LPM_MODE)

static ssize_t spa_lpm_show_property(struct device *dev, struct device_attribute *attr, char *buf);

#define SEC_LPM_ATTR(_name) \
{ \
	.attr = { .name = #_name, \
		  .mode = S_IRUGO}, \
	.show = spa_lpm_show_property, \
}

static struct device_attribute spa_lpm_attrs[] = {
	SEC_LPM_ATTR(charging_mode_booting),
	SEC_LPM_ATTR(charging_source),
	SEC_LPM_ATTR(fg_soc),
	SEC_LPM_ATTR(batt_temp_check),
	SEC_LPM_ATTR(batt_full_check),
};

enum {
	CHARGING_MODE_BOOTING,
	CHARGING_SOURCE,
	FG_SOC,
	BATT_TEMP_CHECK,
	BATT_FULL_CHECK,
};

int spa_lpm_charging_mode_get(void)
{
	int lpm_charging_mode = 0;

	printk(" %s  \n",__func__);

#if defined(CONFIG_BATTERY_D1980)
	if(d1980_get_power_on_reason() == PMIC_GENERAL_USE_BOOT_BY_CHG)
		lpm_charging_mode = 1; 
	else
		lpm_charging_mode = 0;
#else
	if(pm80x_get_power_on_reason() == PMIC_GENERAL_USE_BOOT_BY_CHG)
	{
		printk(" %s   lpm charging mode\n",__func__);
		lpm_charging_mode = 1; 
	}
	else
	{
		printk(" %s   normal mode\n",__func__);
		lpm_charging_mode = 0; 
	}
	
#endif
	return lpm_charging_mode;
}

static ssize_t spa_lpm_show_property(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct spa_driver_data *info = dev_get_drvdata(dev->parent);
	const ptrdiff_t off = attr - spa_lpm_attrs; 
	int i = 0;

	printk(" %s   off : %d\n",__func__, off);

	switch (off)
	{
		case CHARGING_MODE_BOOTING:
			i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n", spa_lpm_charging_mode_get());
			break;
		case CHARGING_SOURCE:
			if(spa_chg_get_current_charger(ginfo) == SPA_CHARGER_TYPE_NONE)
				i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n", false);
			else
				i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n", true);
			break;
		case FG_SOC:
			i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n", spa_bat_get_capacity(info));
			break;
		case BATT_TEMP_CHECK:
			i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n", spa_bat_get_health(info));
			break;
		case BATT_FULL_CHECK:
			if(spa_bat_get_status(info) == POWER_SUPPLY_STATUS_FULL)
				i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n", true);
                        else
        	                i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n", false);	
			break;
		default:
			i = -EINVAL;
	}
	return i;
}

static int spa_lpm_create_attrs(struct device *dev)
{
        int i, rc, j;

        for (i = 0; i < ARRAY_SIZE(spa_lpm_attrs); i++)
        {
                rc = device_create_file(dev, &spa_lpm_attrs[i]);
                if (rc)
                {
                        goto spa_lpm_create_attrs_failed;
                }
        }
        goto succeed;

spa_lpm_create_attrs_failed:
	for ( j = 0; j < i; j++)
	{
                device_remove_file(dev, &spa_lpm_attrs[j]);
	}
succeed:
        return rc;

}
#endif

static ssize_t reset_soc_store_attrs(struct device *dev,
                                              struct device_attribute *attr,
                                              const char *buf, size_t count)
{
	struct spa_driver_data *info = dev_get_drvdata(dev->parent);
	int voltage;
	
	if(info->use_fuelgauge)
	{
		printk(" %s\n", __func__);
		
		spa_bat_fuelgauge_reset(info);
#if defined(CONFIG_STC3115_FUELGAUGE)
		mdelay(1000);
		spa_bat_read_voltage(info, &voltage);
		mdelay(2000);
		spa_bat_read_voltage(info, &voltage);
#endif
	}
		
        return 1;
}

static DEVICE_ATTR(reset_soc, S_IRUGO | S_IWUGO, NULL, reset_soc_store_attrs);

static ssize_t spa_fuelgauge_show_property(struct device *dev, struct device_attribute *attr, char *buf);

#define SEC_FUELGAUGE_ATTR(_name) \
{ \
	.attr = { .name = #_name, \
		  .mode = S_IRUGO}, \
	.show = spa_fuelgauge_show_property, \
}

static struct device_attribute spa_fuelgauge_attrs[] = {
	SEC_FUELGAUGE_ATTR(fg_curr_ua),
};

enum {
	FG_CURR_UA,
};

static ssize_t spa_fuelgauge_show_property(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct spa_driver_data *info = dev_get_drvdata(dev->parent);
	const ptrdiff_t off = attr - spa_fuelgauge_attrs; 
	int i = 0;

	printk(" %s   off : %d\n",__func__, off);
#if 1
	switch (off)
	{
		case FG_CURR_UA:
			i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n", spa_fuelgauge_current/*spa_bat_read_current(info)*/);
			break;
		default:
			i = -EINVAL;
	}
#else
	i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n", 0);
#endif
	return i;
}

static int spa_fuelgauge_create_attrs(struct device *dev)
{
        int i, rc, j;

        for (i = 0; i < ARRAY_SIZE(spa_fuelgauge_attrs); i++)
        {
                rc = device_create_file(dev, &spa_fuelgauge_attrs[i]);
                if (rc)
                {
                        goto spa_lpm_create_attrs_failed;
                }
        }
        goto succeed;

spa_lpm_create_attrs_failed:
	for ( j = 0; j < i; j++)
	{
                device_remove_file(dev, &spa_fuelgauge_attrs[j]);
	}
succeed:
        return rc;

}


static void spa_init_data(struct spa_driver_data *info, struct spa_platform_data *pdata)
{
	int i = 0;
	
	info->battery.health= POWER_SUPPLY_HEALTH_GOOD;
	info->battery.status = POWER_SUPPLY_STATUS_DISCHARGING;
	info->charger.current_charger = SPA_CHARGER_TYPE_NONE;
	info->battery.end_of_charge = SPA_END_OF_CHARGE_NONE;
	wake_lock_init(&info->charger.charger_wakeup, WAKE_LOCK_SUSPEND, "spa_charger_wakeups");
	wake_lock_init(&info->battery.sleep_monitor_wakeup, WAKE_LOCK_SUSPEND, "spa_sleep_monitor");

	init_timer(&info->Charge_timer);
	info->Charge_timer.function = spa_bat_charge_timer_expired;
	info->Charge_timer.data = (u_long)info; 

	init_timer(&info->reCharge_start_timer);
	info->reCharge_start_timer.function = spa_bat_recharge_timer_expired;
	info->reCharge_start_timer.data = (u_long)info; 

	info->use_fuelgauge = pdata->use_fuelgauge;
	info->battery.capacity = pdata->battery_capacity;
	info->battery.charging_stop_high_temp = pdata->charging_stop_high_temp*1000000;
	info->battery.charging_restart_high_temp = pdata->charging_restart_high_temp*1000000;
	info->battery.charging_restart_low_temp = pdata->charging_restart_low_temp*1000000;
	info->battery.charging_stop_low_temp = pdata->charging_stop_low_temp*1000000;
	info->battery.charging_restart_voltage = pdata->charging_restart_voltage;
	info->battery.charge_timer = pdata->charge_timer;
	info->battery.recharge_timer = pdata->recharge_timer;

	if(pdata->VF_low >= pdata->VF_high)
	{
		printk(" %s   please check VF_low %d, VF_high %d in spa_platform_data\n", __func__, pdata->VF_low, pdata->VF_high);
	}
	else
	{
		info->battery.VF_low = pdata->VF_low;
		info->battery.VF_high = pdata->VF_high;
	}

	memcpy(info->temp_adc_table, pdata->temp_adc_table, sizeof(pdata->temp_adc_table));
	memcpy(info->temp_degree_table, pdata->temp_degree_table, sizeof(pdata->temp_degree_table));

	for(i=0; i<SPA_NB_TEMP_TABLE-1; i++)
	{
		if(info->temp_adc_table[i] <= info->temp_adc_table[i+1])
		{
			printk("%s please check temp_adc_table[%d]:%d and temp_adc_table[%d]:%d\n", __func__, i, info->temp_adc_table[i],
				i+1, info->temp_adc_table[i+1]);
		}
	}

	for(i=0; i<SPA_NB_TEMP_TABLE-1; i++)
	{
		if(info->temp_degree_table[i] >= info->temp_degree_table[i+1])
		{
			printk("%s please check temp_degree_table[%d]:%d and temp_degree_table[%d]:%d\n", __func__, i, info->temp_degree_table[i], 
				i+1, info->temp_degree_table[i+1]);
		}
	}
}

static __devinit int spa_probe(struct platform_device *pdev)
{
	struct spa_platform_data *pdata = pdev->dev.platform_data;
	struct spa_driver_data *info;
	int ret = 0;
      
	 printk(" %s  \n",__func__);
      
	info = kzalloc(sizeof(struct spa_driver_data),GFP_KERNEL);
	if(!info)
		return -ENOMEM;

	ginfo = info;

	mutex_init(&info->lock);
	mutex_init(&info->api_lock);
	platform_set_drvdata(pdev, info);	

	spa_init_data(info, pdata);
      
	 ret = power_supply_register(&pdev->dev, &spa_psy_ac);
	 
        if (ret)
        {
                printk(KERN_ALERT " %s power_supply_register failed\n", __func__);
		goto err_power_supply_register_ac; 
        }

	ret = power_supply_register(&pdev->dev, &spa_psy_usb);
	if (ret)
        {
                printk(KERN_ALERT " %s power_supply_register failed\n", __func__);
		goto err_power_supply_register_usb;
        }
           
	 ret = power_supply_register(&pdev->dev, &spa_psy_fuelgauge);
	 if (ret)
       {
                printk(KERN_ALERT " %s power_supply_register failed\n", __func__);
		goto err_power_supply_register_fuelgauge;
        }	   
	 
        ret = power_supply_register(&pdev->dev, &spa_psy_bat);
        if (ret) 
        {
                printk(KERN_ALERT " %s power_supply_register failed\n", __func__);
		goto err_power_supply_register_bat;
        } 

        ret = sysfs_create_group(&pdev->dev.kobj, &battery_attr_group);
        if (ret < 0) {
		printk(KERN_ALERT " %s sysfs_create_group failed\n", __func__);
                goto err_sysfs_create_group;
        }
    
	ret = device_create_file(spa_psy_bat.dev, &dev_attr_reset_soc);
	if (ret)
		goto err_sysfs_create_group; 
                
	ret = spa_fuelgauge_create_attrs(spa_psy_fuelgauge.dev);
	if(ret)
		goto err_sysfs_create_group; 
	
#if defined(CONFIG_SPA_LPM_MODE)
	ret = spa_lpm_create_attrs(spa_psy_usb.dev);
	if(ret)
		goto err_lpm_create_attrs; 
#endif

	INIT_DELAYED_WORK(&info->monitor_VOLT_work, spa_monitor_voltage_work);
	INIT_DELAYED_WORK(&info->monitor_TEMP_work, spa_monitor_temperature_work);
	INIT_DELAYED_WORK(&info->info_notify_work, spa_info_notify_work);
	INIT_DELAYED_WORK(&info->charge_timer_work, spa_bat_charge_timer_work);
	INIT_DELAYED_WORK(&info->recharge_start_timer_work, spa_bat_recharge_start_timer_work);
	INIT_DELAYED_WORK(&info->sleep_monitor_work, spa_bat_sleep_monitor_work);
	
	schedule_delayed_work(&info->info_notify_work, SPA_NOTIFY_INTERVAL);

#if TEMP_TEST_BIN
	create_spa_proc_file();
#endif

	return 0;

#if defined(CONFIG_SPA_LPM_MODE)
err_lpm_create_attrs:
#endif
err_sysfs_create_group: 
	power_supply_unregister(&spa_psy_bat);
err_power_supply_register_bat:
		power_supply_unregister(&spa_psy_fuelgauge);
err_power_supply_register_fuelgauge:
	power_supply_unregister(&spa_psy_usb);
err_power_supply_register_usb:
	power_supply_unregister(&spa_psy_ac);
err_power_supply_register_ac:
	
	kfree(info);
	return ret; 

}

static int __devexit spa_remove(struct platform_device *pdev)
{
	printk(" %s  \n",__func__);
	return 0;
}


#ifdef CONFIG_PM
static int spa_suspend(struct device *dev)
{
	printk(" %s  \n",__func__);
	return 0;
}

static int spa_resume(struct device *dev)
{
	printk(" %s  \n",__func__);
	return 0;
}

void spa_shutdown(struct platform_device *pdev)
{
	struct spa_driver_data *info = platform_get_drvdata(pdev);
	printk(" %s  \n", __func__);
	cancel_delayed_work_sync(&info->monitor_VOLT_work);
	cancel_delayed_work_sync(&info->monitor_TEMP_work);
	cancel_delayed_work_sync(&info->info_notify_work);

	if(timer_pending(&info->Charge_timer))
	{
		del_timer(&info->Charge_timer);
	}

	if(timer_pending(&info->reCharge_start_timer))
	{
		del_timer(&info->reCharge_start_timer);
	}
}

static struct dev_pm_ops spa_pm_ops = {
	.suspend	= spa_suspend,
	.resume		= spa_resume,
}; 
#endif

/* Probe & Remove function */
static struct platform_driver spa_driver = {
        .driver         = {
                .name   = "Sec_BattMonitor",
                .owner  = THIS_MODULE,
#ifdef CONFIG_PM
                .pm     = &spa_pm_ops,
#endif
        },
        .probe          = spa_probe,
        .remove         = __devexit_p(spa_remove),
	  .shutdown	= spa_shutdown, 
};


/* spa Init Function */
static int __init spa_init(void)
{
	int retVal = -EINVAL;
	retVal = platform_driver_register(&spa_driver);	      
	return (retVal);
}

/* spa Exit Function */
static void __exit spa_exit(void)
{
	printk("%s\n",__func__);
	flush_scheduled_work();
	platform_driver_unregister(&spa_driver);
}


subsys_initcall(spa_init);
module_exit(spa_exit);

MODULE_AUTHOR("YongTaek Lee <ytk.lee@samsung.com>");
MODULE_DESCRIPTION("Linux Driver for battery monitor");
MODULE_LICENSE("GPL");

#endif //__SPA_C__
