/*
 * d2199 Battery/Power module declarations.
  *
 * Copyright(c) 2013 Dialog Semiconductor Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 *
 */


#ifndef __D2199_BATTERY_H__
#define __D2199_BATTERY_H__

#include <linux/mutex.h>
#include <linux/wakelock.h>
#include <linux/power_supply.h>


//#define D2199_REG_EOM_IRQ
//#undef D2199_REG_VDD_MON_IRQ
//#undef D2199_REG_VDD_LOW_IRQ
//#define D2199_REG_TBAT2_IRQ
#define CONFIG_D2199_MULTI_WEIGHT

#define D2199_MANUAL_READ_RETRIES			(5)
#define ADC2TEMP_LUT_SIZE					(22)
#define ADC2VBAT_LUT_SIZE					(10)
#define D2199_ADC_RESOLUTION				(10)

#define AVG_SIZE							(16)
#define AVG_SHIFT							(4)
#define MAX_INIT_TRIES              		(AVG_SIZE + 5)

#define DEGREEK_FOR_DEGREEC_0				(273)

#define C2K(c)								(273 + c)	/* convert from Celsius to Kelvin */
#define K2C(k)								(k - 273)	/* convert from Kelvin to Celsius */

#define BAT_HIGH_TEMPERATURE				C2K(700)
#define BAT_ROOM_TEMPERATURE				C2K(200) //C2K(250)
#define BAT_ROOM_LOW_TEMPERATURE			C2K(100)
#define BAT_LOW_TEMPERATURE					C2K(0)
#define BAT_LOW_MID_TEMPERATURE				C2K(-100)
#define BAT_LOW_LOW_TEMPERATURE				C2K(-200)

#define BAT_CAPACITY_1300MA					(1300)
#define BAT_CAPACITY_1500MA					(1500)
#define BAT_CAPACITY_1800MA					(1800)
#define BAT_CAPACITY_2000MA					(2000)
#define BAT_CAPACITY_2100MA					(2100)
#define BAT_CAPACITY_4500MA					(4500)
#define BAT_CAPACITY_7000MA					(7000)

#if defined(CONFIG_MACH_CS02)
#define USED_BATTERY_CAPACITY				BAT_CAPACITY_1800MA
#define MULTI_WEIGHT_SIZE					9
#elif defined(CONFIG_MACH_WILCOX) || defined(CONFIG_MACH_CS05)
#define USED_BATTERY_CAPACITY				BAT_CAPACITY_2100MA
#define MULTI_WEIGHT_SIZE					15
#elif defined(CONFIG_MACH_BAFFIN) || defined(CONFIG_MACH_BAFFINQ)
#define USED_BATTERY_CAPACITY				BAT_CAPACITY_2100MA
#define MULTI_WEIGHT_SIZE					15
#else
#define MULTI_WEIGHT_SIZE					1
#endif

#if USED_BATTERY_CAPACITY == BAT_CAPACITY_1800MA
#define CONFIG_SOC_LUT_15STEPS
#elif USED_BATTERY_CAPACITY == BAT_CAPACITY_2100MA
#define CONFIG_SOC_LUT_15STEPS
#else
#undef  CONFIG_SOC_LUT_15STEPS
#endif

#ifdef CONFIG_SOC_LUT_15STEPS
#define ADC2SOC_LUT_SIZE					(15)
#else
#define ADC2SOC_LUT_SIZE					(14)
#endif

#define D2199_VOLTAGE_MONITOR_START			(HZ*1)
#define D2199_VOLTAGE_MONITOR_NORMAL		(HZ*10)
#define D2199_VOLTAGE_MONITOR_FAST			(HZ*3)

#define D2199_TEMPERATURE_MONITOR_START		(HZ*1)
#define D2199_TEMPERATURE_MONITOR_NORMAL	(HZ*10)
#define D2199_TEMPERATURE_MONITOR_FAST		(HZ*1)

#define D2199_NOTIFY_INTERVAL				(HZ*10)

#define D2199_SLEEP_MONITOR_WAKELOCK_TIME	(0.35*HZ)

#define BAT_VOLTAGE_ADC_DIVISION			(1700)

#define BAT_POWER_OFF_VOLTAGE				(3400)

#define D2199_CHARGE_CV_ADC_LEVEL		    (3380)

#define D2199_CAL_HIGH_VOLT					(4200)
#define D2199_CAL_LOW_VOLT					(3400)

#define D2199_BASE_4P2V_ADC                 (3269)
#define D2199_BASE_3P4V_ADC                 (1581)
#define D2199_CAL_MAX_OFFSET				(10)

enum {
	CHARGER_TYPE_NONE = 0,
	CHARGER_TYPE_TA,
	CHARGER_TYPE_USB,
	CHARGER_TYPE_MAX
};

enum {
	D2199_BATTERY_SOC = 0,
	D2199_BATTERY_TEMP_ADC,
	D2199_BATTERY_CUR_VOLTAGE,
	D2199_BATTERY_SLEEP_MONITOR,
	D2199_BATTERY_CHG_CURRENT,
	D2199_BATTERY_MAX
};

typedef enum d2199_adc_channel {
	D2199_ADC_VOLTAGE = 0,
	D2199_ADC_TEMPERATURE_1,
	D2199_ADC_TEMPERATURE_2,
	D2199_ADC_VF,
	D2199_ADC_AIN,
	D2199_ADC_TJUNC,
	D2199_ADC_CHANNEL_MAX    
} adc_channel;

typedef enum d2199_adc_mode {
	D2199_ADC_IN_AUTO = 1,
	D2199_ADC_IN_MANUAL,
	D2199_ADC_MODE_MAX
} adc_mode;

struct diff_tbl {
	u16 c1_diff[MULTI_WEIGHT_SIZE];
	u16 c2_diff[MULTI_WEIGHT_SIZE];
};

struct adc_man_res {
	u16 read_adc;
	u8  is_adc_eoc;
};

struct adc2temp_lookuptbl {
	u16 adc[ADC2TEMP_LUT_SIZE];
	u16 temp[ADC2TEMP_LUT_SIZE];
};

struct adc2vbat_lookuptbl {
	u16 adc[ADC2VBAT_LUT_SIZE];
	u16 vbat[ADC2VBAT_LUT_SIZE];
	u16 offset[ADC2VBAT_LUT_SIZE];
};

struct adc2soc_lookuptbl {
	u16 adc_ht[ADC2SOC_LUT_SIZE];
	u16 adc_rt[ADC2SOC_LUT_SIZE];
	u16 adc_rlt[ADC2SOC_LUT_SIZE];
	u16 adc_lt[ADC2SOC_LUT_SIZE];
	u16 adc_lmt[ADC2SOC_LUT_SIZE];
	u16 adc_llt[ADC2SOC_LUT_SIZE];
	u16 soc[ADC2SOC_LUT_SIZE];
};

struct adc_cont_in_auto {
	u8 adc_preset_val;
	u8 adc_cont_val;
	u8 adc_msb_res;
	u8 adc_lsb_res;
	u8 adc_lsb_mask;
};

struct d2199_battery_data {
	u8  is_charging;
	u8  vdd_hwmon_level;

	u32	current_level;

	// for voltage
	u32 origin_volt_adc;
	u32 current_volt_adc;
	u32 average_volt_adc;
	u32	current_voltage;
	u32	average_voltage;
	u32 sum_voltage_adc;
	int sum_total_adc;

	// for temperature
	u32	current_temp_adc;
	u32	average_temp_adc; 
	int	current_temperature;
	int	average_temperature;
	u32 sum_temperature_adc;
	
	u32	soc;
	u32 prev_soc;

	int battery_technology;
	int battery_present;
	u32	capacity;

	u16 vf_adc;
	u32 vf_ohm;
	u32	vf_lower;
	u32	vf_upper; 

	u32 voltage_adc[AVG_SIZE];
	u32 temperature_adc[AVG_SIZE];
	u8	voltage_idx;
	u8 	temperature_idx;

	u8  volt_adc_init_done;
	u8  temp_adc_init_done;
	struct wake_lock sleep_monitor_wakeup;
    struct adc_man_res adc_res[D2199_ADC_CHANNEL_MAX];
	u8 virtual_battery_full;
	int charging_current;
};

struct d2199_battery {
	struct d2199		*pd2199;
	struct device 		*dev;
	struct platform_device *pdev;

	u8 adc_mode;

	struct d2199_battery_data	battery_data;

	struct delayed_work	monitor_volt_work;
	struct delayed_work	monitor_temp_work;
	struct delayed_work	sleep_monitor_work;

	struct mutex		meoc_lock;
	struct mutex		lock;
	
	int (*d2199_read_adc)(struct d2199_battery *pbat, adc_channel channel);

};


int d2199_battery_read_status(int);
int d2199_battery_write_status(int type, int val);
int d2199_read_temperature_adc(int *tbat, int channel);

#endif /* __D2199_BATTERY_H__ */
