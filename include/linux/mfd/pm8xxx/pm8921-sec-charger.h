/*
 * include/linux/mfd/pm8xxx/pm8921-sec-charger.h
 *
 * Copyright (c) 2011 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __PM8XXX_SEC_CHARGER_H
#define __PM8XXX_SEC_CHARGER_H
#if defined(CONFIG_CHARGER_MAX77XXX)
#if defined(CONFIG_SEC_PRODUCT_8930)
#include <linux/battery/sec_battery_8930.h>
#else
#include <linux/battery/sec_battery.h> 
#endif
#endif

/*******************************************
** Feature definition
********************************************/
#undef QUALCOMM_TEMPERATURE_CONTROL
#undef QUALCOMM_POWERSUPPLY_PROPERTY

/*******************************************
** Other definition
*******************************************/
#if defined(CONFIG_BATTERY_MAX17040) || \
	defined(CONFIG_BATTERY_MAX17042)
#define RCOMP0_TEMP	20	/* 'C */

#define FG_T_SOC	0
#define FG_T_VCELL	1
#define FG_T_PSOC	2
#define FG_T_RCOMP	3
#define FG_T_FSOC	4
#define FG_RESET	5
#endif

#if !defined(CONFIG_SEC_PRODUCT_8930)
enum cable_type_t {
	CABLE_TYPE_NONE = 0,
	CABLE_TYPE_USB,
	CABLE_TYPE_AC,
	CABLE_TYPE_MISC,
	CABLE_TYPE_CARDOCK,
	CABLE_TYPE_UARTOFF,
	CABLE_TYPE_JIG,
	CABLE_TYPE_UNKNOWN,
	CABLE_TYPE_CDP,
	CABLE_TYPE_SMART_DOCK,
#ifdef CONFIG_WIRELESS_CHARGING
	CABLE_TYPE_WPC = 10,
#endif
};
#endif

#define VUBS_IN_CURR_NONE	2
#define VBUS_IN_CURR_USB	500
#define BATT_IN_CURR_USB	475
#define	VBUS_IN_CURR_TA		1100
#define	BATT_IN_CURR_TA		1025
#ifdef CONFIG_WIRELESS_CHARGING
#define VBUS_IN_CURR_WPC	700
#define BATT_IN_CURR_WPC	675
#endif

#if defined(CONFIG_SEC_PRODUCT_8930)
#ifdef CONFIG_PM8921_SEC_CHARGER
#define TEMP_GPIO	PM8XXX_AMUX_MPP_3
#else
#define TEMP_GPIO	PM8XXX_AMUX_MPP_7
#endif
#endif

#define TEMP_ADC_CHNNEL	ADC_MPP_1_AMUX6

static const int temper_table[][2] = {
	{1600, -200},
	{1550, -150},
	{1500, -100},
	{1400, -50},
	{1300, 0},
	{1150, 100},
	{1050, 150},
	{950, 200},
	{850, 250},
	{750, 300},
	{650, 350},
	{600, 400},
	{500, 450},
	{400, 500},
	{330, 550},
	{300, 600},
};

extern bool ovp_state;

struct pm8921_charging_current {
	unsigned int		vbus;
	unsigned int		ibat;
};

struct pm8921_sec_battery_data {
	/* for absolute timer (first) */
	unsigned long		charging_total_time;
	/* for recharging timer (second) */
	unsigned long		recharging_total_time;
	/* temperature set */
	int				temp_high_block_event;
	int				temp_high_recover_event;
	int				temp_low_block_event;
	int				temp_low_recover_event;
	int				temp_high_block_normal;
	int				temp_high_recover_normal;
	int				temp_low_block_normal;
	int				temp_low_recover_normal;
	int				temp_high_block_lpm;
	int				temp_high_recover_lpm;
	int				temp_low_block_lpm;
	int				temp_low_recover_lpm;
#if defined(CONFIG_WIRELESS_CHARGING)
	int				temp_high_block_wc;
	int				temp_high_recover_wc;
	int				temp_low_block_wc;
	int				temp_low_recover_wc;
#endif
	/* event check model flag */
	bool				event_check;
	/* sustaining event after deactivated (second) */
	unsigned int		event_waiting_time;

	/* soc should be soc x 10 (0.1% degree)
	 * only for scaling
	 */
	unsigned int			capacity_max;
	unsigned int			capacity_max_margin;
	unsigned int			capacity_min;

	unsigned int			ui_term_current;	/* mA */
	unsigned int			charging_term_time;	/* sec */
	unsigned int			recharging_voltage;	/* mV */
	unsigned int			poweroff_check_soc;	/* % */

	struct pm8921_charging_current *chg_current_table;
	unsigned int			*siop_table;
	void					(*monitor_additional_check)(void);
};

/*******************************************
** for Debug screen
*****************************************/
struct pm8921_reg {
	u8	chg_cntrl;
	u8	chg_cntrl_2;
	u8	chg_cntrl_3;
	u8	pbl_access1;
	u8	pbl_access2;
	u8	sys_config_1;
	u8	sys_config_2;
	u8	chg_vdd_max;
	u8	chg_vdd_safe;
	u8	chg_ibat_max;
	u8	chg_ibat_safe;
	u8	chg_iterm;
};
struct pm8921_irq {
	int	usbin_valid;
	int	usbin_ov;
	int usbin_uv;
	int batttemp_hot;
	int batttemp_cold;
	int batt_inserted;
	int trklchg;
	int fastchg;
	int batfet;
	int batt_removed;
	int vcp;
	int bat_temp_ok;
};

#if !defined(CONFIG_MACH_MELIUS) && !defined(CONFIG_MACH_KS02) && \
	!defined(CONFIG_CHARGER_MAX77XXX)
ssize_t sec_bat_show_attrs(struct device *dev,
				struct device_attribute *attr, char *buf);

ssize_t sec_bat_store_attrs(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count);

#define SEC_BATTERY_ATTR(_name)						\
{									\
	.attr = {.name = #_name, .mode = 0664},	\
	.show = sec_bat_show_attrs,					\
	.store = sec_bat_store_attrs,					\
}

#endif
ssize_t sec_fg_show_attrs(struct device *dev,
				struct device_attribute *attr, char *buf);

ssize_t sec_fg_store_attrs(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count);


#define SEC_FUELGAUGE_ATTR(_name)	\
{	\
	.attr = {.name = #_name, .mode = 0664},	\
	.show = sec_fg_show_attrs,		\
	.store = sec_fg_store_attrs,	\
}

#if !defined(CONFIG_MACH_MELIUS) && !defined(CONFIG_MACH_KS02) && \
	!defined(CONFIG_CHARGER_MAX77XXX)
/* event check */
#define EVENT_NONE				(0)
#define EVENT_2G_CALL			(0x1 << 0)
#define EVENT_3G_CALL			(0x1 << 1)
#define EVENT_MUSIC				(0x1 << 2)
#define EVENT_VIDEO				(0x1 << 3)
#define EVENT_BROWSER			(0x1 << 4)
#define EVENT_HOTSPOT			(0x1 << 5)
#define EVENT_CAMERA			(0x1 << 6)
#define EVENT_CAMCORDER			(0x1 << 7)
#define EVENT_DATA_CALL			(0x1 << 8)
#define EVENT_WIFI				(0x1 << 9)
#define EVENT_WIBRO				(0x1 << 10)
#define EVENT_LTE				(0x1 << 11)
#endif

enum {
	FG_CURR_UA = 0,
};


#if !defined(CONFIG_MACH_MELIUS) && !defined(CONFIG_MACH_KS02) && \
	!defined(CONFIG_CHARGER_MAX77XXX)
enum {
	BATT_RESET_SOC = 0,
	BATT_READ_RAW_SOC,
	BATT_READ_ADJ_SOC,
	BATT_TYPE,
	BATT_VFOCV,
	BATT_VOL_ADC,
	BATT_VOL_ADC_CAL,
	BATT_VOL_AVER,
	BATT_VOL_ADC_AVER,
	BATT_TEMP_ADC,
	BATT_TEMP_AVER,
	BATT_TEMP_ADC_AVER,
	BATT_VF_ADC,

	BATT_LP_CHARGING,
	SIOP_ACTIVATED,
	SIOP_LEVEL,
	BATT_CHARGING_SOURCE,
	FG_REG_DUMP,
	FG_RESET_CAP,
	FG_CAPACITY,
	AUTH,
	CHG_CURRENT_ADC,
	WC_ADC,
	WC_STATUS,
	FACTORY_MODE,
	UPDATE,
	TEST_MODE,

	BATT_EVENT_GSM_CALL,
	BATT_EVENT_WCDMA_CALL,
	BATT_EVENT_MUSIC,
	BATT_EVENT_VIDEO,
	BATT_EVENT_BROWSER,
	BATT_EVENT_HOTSPOT,
	BATT_EVENT_CAMERA,
	BATT_EVENT_DATA_CALL,
	BATT_EVENT_WIFI,
	BATT_EVENT_LTE,
	BATT_EVENT,
	BATT_SLATE_MODE,
};
#endif


#endif
