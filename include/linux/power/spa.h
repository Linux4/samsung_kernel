/*

 * Copyright (C) 2011, SAMSUNG Corporation.

 * Author: YongTaek Lee  <ytk.lee@samsung.com> 

 *

 * This program is free software; you can redistribute it and/or modify

 * it under the terms of the GNU General Public License version 2 as

 * published by the Free Software Foundation.

 */







#ifndef __MACH_SPA_H

#define __MACH_SPA_H



#include <linux/wakelock.h>

#include <linux/power_supply.h>



#define SPA_SLEEP_MONITOR_WAKELOCK_TIME	(0.7*HZ)



#define SPA_VOLTAGE_MONITOR_START	(HZ*1)

#define SPA_VOLTAGE_MONITOR_NORMAL	(HZ*10)

#define SPA_VOLTAGE_MONITOR_FAST	(HZ*5)

#define SPA_VOLTAGE_MONITOR_FAST_USB	(HZ*5)



#define SPA_TEMPERATURE_MONITOR_START	(HZ*1)

#define SPA_TEMPERATURE_MONITOR_NORMAL	(HZ*10)

#define SPA_TEMPERATURE_MONITOR_FAST	(HZ*1)



#define SPA_NOTIFY_INTERVAL		(HZ*60)



#define SPA_CHARGE_START_TIMER		0

#define SPA_CHARGE_RESTART_TIMER	1



#define SPA_BATTERY_CAPACITY_1500MA	(1500)

#define SPA_BATTERY_CAPACITY_2000MA	(2000)

#define SPA_BATTERY_CAPACITY_4500MA	(4500)

#define SPA_BATTERY_CAPACITY_7000MA	(7000)



#define SPA_RECHARGE_CHECK_TIMER_30SEC	(HZ*30)



#define SPA_CHARGING_RESTART_VOLTAGE	4150



#define SPA_POWER_OFF_VOLTAGE		3400



#define SPA_BAT_VOLTAGE_SAMPLE		16

#define SPA_BAT_TEMPERATURE_SAMPLE	4



#define SPA_NB_TEMP_TABLE	22



#define SPA_CHARGER_TYPE_NONE	0

#define SPA_CHARGER_TYPE_TA	1

#define SPA_CHARGER_TYPE_USB	2



#define SPA_CATEGORY_DEVICE	0

#define SPA_CATEGORY_BATTERY	1



#define SPA_DEVICE_EVENT_TA_ATTACHED			0

#define SPA_DEVICE_EVENT_TA_DETACHED			1

#define SPA_DEVICE_EVENT_USB_ATTACHED			2

#define SPA_DEVICE_EVENT_USB_DETACHED			3

#define SPA_DEVICE_EVENT_JIG_ATTACHED			4

#define SPA_DEVICE_EVENT_JIG_DETACHED			5

#define SPA_DEVICE_EVENT_FUEL_GAUGE_TA_ATTACHED		6

#define SPA_DEVICE_EVENT_FUEL_GAUGE_TA_DETACHED		7

#define SPA_DEVICE_EVENT_FUEL_START_QUICKSTART		8

#define SPA_DEVICE_EVENT_FUEL_END_QUICKSTART		9



#define SPA_BATTERY_EVENT_CHARGE_FULL		0

#define SPA_BATTERY_EVENT_OVP_CHARGE_STOP	1

#define SPA_BATTERY_EVENT_OVP_CHARGE_RESTART	2

#define SPA_BATTERY_EVENT_SLEEP_MONITOR		3



#define SPA_END_OF_CHARGE_NONE			0

#define SPA_END_OF_CHARGE_BY_FULL		(1 << 0)

#define SPA_END_OF_CHARGE_BY_TEMPERATURE	(1 << 1)

#define SPA_END_OF_CHARGE_BY_TIMER		(1 << 2)

#define SPA_END_OF_CHARGE_BY_OVP		(1 << 3) 

#define SPA_END_OF_CHARGE_BY_VF_OPEN		(1 << 4)

#define SPA_END_OF_CHARGE_BY_QUICKSTART		(1 << 5)





struct spa_charger_data{

	int	current_charger;

	int	jig_connected; 
	int	u200_ta_connected;



	struct wake_lock charger_wakeup;

#if defined(CONFIG_SPA_FUEL_GAUGE_TEST)

	struct wake_lock fuelgaic_idle_wake;

	struct wake_lock fuelgaic_suspend_wake;

#endif



	void	(*enable_charge)(enum power_supply_type);

	void 	(*disable_charge)(unsigned char);

};



struct spa_battery_data{

	unsigned char 	health;

	unsigned char 	status;

	unsigned int 	current_level;

	unsigned int 	current_voltage;

	unsigned int 	average_voltage;

	signed int	current_temperature;

	signed int	average_temperature;

	unsigned int	current_temp_adc;

	unsigned int	average_temp_adc; 

	unsigned int 	soc;

	unsigned char	end_of_charge;

	unsigned int	capacity;

	unsigned int		VF_low;

	unsigned int		VF_high;

	int charging_stop_high_temp;

	int charging_restart_high_temp;

	int charging_restart_low_temp;

	int charging_stop_low_temp;

	int charging_restart_voltage;

	unsigned int charge_timer;

	unsigned int recharge_timer;



	struct wake_lock sleep_monitor_wakeup;



	int	(*read_voltage)(int*);

	int	(*read_soc)(int*);

	int	(*read_temperature)(int*);

	int	(*read_VF)(unsigned int*);

	void	(*power_down)(void);
	int	(*fuelgauge_reset)(void);

};



struct spa_driver_data{

	struct spa_charger_data	charger;

	struct spa_battery_data	battery;

	

	struct delayed_work	monitor_VOLT_work;

	struct delayed_work	monitor_TEMP_work;

	struct delayed_work	info_notify_work;

	struct delayed_work	charge_timer_work;

	struct delayed_work	recharge_start_timer_work;

	struct delayed_work	sleep_monitor_work;



	struct mutex		lock;

	struct mutex		api_lock;

	unsigned int		use_fuelgauge;



	struct timer_list	Charge_timer;

	struct timer_list	reCharge_start_timer; 

	unsigned int		temp_adc_table[SPA_NB_TEMP_TABLE];
	int			temp_degree_table[SPA_NB_TEMP_TABLE];
};



struct spa_platform_data{

	unsigned int 	use_fuelgauge;

	unsigned int 	battery_capacity;

	unsigned int	VF_low;

	unsigned int	VF_high;
	unsigned int	temp_adc_table[SPA_NB_TEMP_TABLE];
	int		temp_degree_table[SPA_NB_TEMP_TABLE];

	int charging_stop_high_temp;

	int charging_restart_high_temp;

	int charging_restart_low_temp;

	int charging_stop_low_temp;

	int charging_restart_voltage;

	unsigned int charge_timer;

	unsigned int recharge_timer;

};



int spa_bat_register_read_voltage(int (*read_voltage)(int *));

int spa_bat_unregister_read_voltage(int (*read_voltage)(int*));

int spa_bat_register_read_soc(int (*read_soc)(int*));

int spa_bat_unregister_read_soc(int (*read_soc)(int*));

int spa_bat_register_read_temperature(int (*read_temperature)(int *));

int spa_bat_register_read_VF(int (*read_VF)(unsigned int *));

int spa_bat_register_power_down(void (*power_down)(void)); 

int spa_chg_register_enable_charge(void (*enable_charge)(enum power_supply_type));

int spa_chg_unregister_enable_charge(void (*enable_charge)(enum power_supply_type));

int spa_chg_register_disable_charge(void (*disable_charge)(unsigned char));

int spa_chg_unregister_disable_charge(void (*disable_charge)(unsigned char));
int spa_bat_register_fuelgauge_reset(int (*fuelgauge_reset)(void));
int spa_bat_unregister_fuelgauge_reset(int (*fuelgauge_reset)(void));


void (*spa_get_external_event_handler(void))(int, int);



#endif /* __MACH_SPA_H */

