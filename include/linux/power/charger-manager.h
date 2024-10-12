/*
 * Copyright (C) 2011 Samsung Electronics Co., Ltd.
 * MyungJoo.Ham <myungjoo.ham@samsung.com>
 *
 * Charger Manager.
 * This framework enables to control and multiple chargers and to
 * monitor charging even in the context of suspend-to-RAM with
 * an interface combining the chargers.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
**/

#ifndef _CHARGER_MANAGER_H
#define _CHARGER_MANAGER_H

#include <linux/alarmtimer.h>
#include <linux/extcon.h>
#include <linux/power_supply.h>
#include <linux/power/sprd_vote.h>
#include <linux/usb/tcpm.h>
/* Tab A8 code for SR-AX6300-01-5 by wenyaqi at 20210824 start */
#ifdef CONFIG_AFC
#include <linux/afc/afc.h>
#endif
/* Tab A8 code for SR-AX6300-01-5 by wenyaqi at 20210824 end */

enum power_supply_charger_type {
	POWER_SUPPLY_CHARGER_TYPE_UNKNOWN = 0,
	POWER_SUPPLY_USB_CHARGER_TYPE_SDP,		/* Standard Downstream Port */
	POWER_SUPPLY_USB_CHARGER_TYPE_DCP,		/* Dedicated Charging Port */
	POWER_SUPPLY_USB_CHARGER_TYPE_CDP,		/* Charging Downstream Port */
	POWER_SUPPLY_USB_CHARGER_TYPE_ACA,		/* Accessory Charger Adapters */
	POWER_SUPPLY_USB_CHARGER_TYPE_C,		/* Type C Port */
	POWER_SUPPLY_USB_CHARGER_TYPE_PD,		/* Power Delivery Port */
	POWER_SUPPLY_USB_CHARGER_TYPE_PD_DRP,		/* PD Dual Role Port */
	POWER_SUPPLY_USB_CHARGER_TYPE_PD_PPS,		/* PD Programmable Power Supply */
	POWER_SUPPLY_USB_CHARGER_TYPE_APPLE_BRICK_ID,	/* Apple Charging Method */
	POWER_SUPPLY_USB_CHARGER_TYPE_SFCP_1P0,		/* SFCP1.0 Port*/
	POWER_SUPPLY_USB_CHARGER_TYPE_SFCP_2P0,		/* SFCP2.0 Port*/
	/* Tab A8 code for SR-AX6300-01-5 by wenyaqi at 20210824 start */
	#ifdef CONFIG_AFC
	POWER_SUPPLY_USB_CHARGER_TYPE_AFC,			/* AFC Port */
	#endif
	/* Tab A8 code for SR-AX6300-01-5 by wenyaqi at 20210824 end */
/* HS03 code for SR-SL6215-01-545 by shixuanxuan at 20211026 start */
#if !defined(HQ_FACTORY_BUILD)
	POWER_SUPPLY_USB_CHARGER_TYPE_FLOAT,
#endif
/* HS03 code for SR-SL6215-01-545 by shixuanxuan at 20211026 end */
	POWER_SUPPLY_WIRELESS_CHARGER_TYPE_BPP,		/* BPP wireless method */
	POWER_SUPPLY_WIRELESS_CHARGER_TYPE_EPP,		/* EPP wiresess method */
};

enum cm_charge_info_cmd {
	CM_CHARGE_INFO_CHARGE_LIMIT = BIT(0),
	CM_CHARGE_INFO_INPUT_LIMIT = BIT(1),
	CM_CHARGE_INFO_THERMAL_LIMIT = BIT(2),
	CM_CHARGE_INFO_JEITA_LIMIT = BIT(3),
};

enum data_source {
	CM_BATTERY_PRESENT,
	CM_NO_BATTERY,
	CM_FUEL_GAUGE,
	CM_CHARGER_STAT,
};

enum polling_modes {
	CM_POLL_DISABLE = 0,
	CM_POLL_ALWAYS,
	CM_POLL_EXTERNAL_POWER_ONLY,
	CM_POLL_CHARGING_ONLY,
};

enum cm_event_types {
	CM_EVENT_UNKNOWN = 0,
	CM_EVENT_BATT_FULL,
	CM_EVENT_BATT_IN,
	CM_EVENT_BATT_OUT,
	CM_EVENT_BATT_OVERHEAT,
	CM_EVENT_BATT_COLD,
	CM_EVENT_EXT_PWR_IN_OUT,
	CM_EVENT_CHG_START_STOP,
	CM_EVENT_WL_CHG_START_STOP,
	CM_EVENT_FAST_CHARGE,
	CM_EVENT_INT,
	CM_EVENT_OTHERS,
};

enum cm_jeita_types {
	CM_JEITA_DCP = 0,
	CM_JEITA_SDP,
	CM_JEITA_CDP,
	CM_JEITA_UNKNOWN,
	CM_JEITA_FCHG,
	CM_JEITA_FLASH,
	CM_JEITA_WL_BPP,
	CM_JEITA_WL_EPP,
	/* Tab A8 code for SR-AX6300-01-5 by wenyaqi at 20210824 start */
	#ifdef CONFIG_AFC
	CM_JEITA_AFC,
	#endif
	/* Tab A8 code for SR-AX6300-01-5 by wenyaqi at 20210824 end */
	CM_JEITA_MAX,
};

enum cm_capacity_cmd {
	CM_CAPACITY = 0,
	CM_BOOT_CAPACITY,
};

enum cm_ir_comp_state {
	CM_IR_COMP_STATE_UNKNOWN,
	CM_IR_COMP_STATE_NORMAL,
	CM_IR_COMP_STATE_CP,
};

enum cm_cp_state {
	CM_CP_STATE_UNKNOWN,
	CM_CP_STATE_RECOVERY,
	CM_CP_STATE_ENTRY,
	CM_CP_STATE_CHECK_VBUS,
	CM_CP_STATE_TUNE,
	CM_CP_STATE_EXIT,
};

enum cm_charge_status {
	CM_CHARGE_TEMP_OVERHEAT = BIT(0),
	CM_CHARGE_TEMP_COLD = BIT(1),
	CM_CHARGE_VOLTAGE_ABNORMAL = BIT(2),
	CM_CHARGE_HEALTH_ABNORMAL = BIT(3),
	CM_CHARGE_DURATION_ABNORMAL = BIT(4),
};

enum cm_fast_charge_command {
	CM_FAST_CHARGE_NORMAL_CMD = 1,
	CM_FAST_CHARGE_ENABLE_CMD,
	CM_FAST_CHARGE_DISABLE_CMD,
	CM_PPS_CHARGE_ENABLE_CMD,
	CM_PPS_CHARGE_DISABLE_CMD,
};

enum present_command {
	CM_USB_PRESENT_CMD,
	CM_BATTERY_PRESENT_CMD,
	CM_VBUS_PRESENT_CMD,
};

enum temperature_command {
	CMD_BATT_TEMP_CMD,
	CM_BUS_TEMP_CMD,
	CM_DIE_TEMP_CMD,
};

enum health_command {
	CM_FAULT_HEALTH_CMD,
	CM_ALARM_HEALTH_CMD,
	CM_BUS_ERR_HEALTH_CMD,
};
enum cm_charger_fault_status_mask {
	CM_CHARGER_BAT_OVP_FAULT_MASK = BIT(0),
	CM_CHARGER_BAT_OCP_FAULT_MASK = BIT(1),
	CM_CHARGER_BUS_OVP_FAULT_MASK = BIT(2),
	CM_CHARGER_BUS_OCP_FAULT_MASK = BIT(3),
	CM_CHARGER_BAT_THERM_FAULT_MASK = BIT(4),
	CM_CHARGER_BUS_THERM_FAULT_MASK = BIT(5),
	CM_CHARGER_DIE_THERM_FAULT_MASK = BIT(6),
	CM_CHARGER_BAT_OVP_ALARM_MASK = BIT(8),
	CM_CHARGER_BAT_OCP_ALARM_MASK = BIT(9),
	CM_CHARGER_BUS_OVP_ALARM_MASK = BIT(10),
	CM_CHARGER_BUS_OCP_ALARM_MASK = BIT(11),
	CM_CHARGER_BAT_THERM_ALARM_MASK = BIT(12),
	CM_CHARGER_BUS_THERM_ALARM_MASK = BIT(13),
	CM_CHARGER_DIE_THERM_ALARM_MASK = BIT(14),
	CM_CHARGER_BAT_UCP_ALARM_MASK = BIT(15),
	CM_CHARGER_BUS_ERR_LO_MASK = BIT(24),
	CM_CHARGER_BUS_ERR_HI_MASK = BIT(25),
};

enum cm_charger_fault_status_shift {
	CM_CHARGER_BAT_OVP_FAULT_SHIFT = 0,
	CM_CHARGER_BAT_OCP_FAULT_SHIFT = 1,
	CM_CHARGER_BUS_OVP_FAULT_SHIFT = 2,
	CM_CHARGER_BUS_OCP_FAULT_SHIFT = 3,
	CM_CHARGER_BAT_THERM_FAULT_SHIFT = 4,
	CM_CHARGER_BUS_THERM_FAULT_SHIFT = 5,
	CM_CHARGER_DIE_THERM_FAULT_SHIFT = 6,
	CM_CHARGER_BAT_OVP_ALARM_SHIFT = 8,
	CM_CHARGER_BAT_OCP_ALARM_SHIFT = 9,
	CM_CHARGER_BUS_OVP_ALARM_SHIFT = 10,
	CM_CHARGER_BUS_OCP_ALARM_SHIFT = 11,
	CM_CHARGER_BAT_THERM_ALARM_SHIFT = 12,
	CM_CHARGER_BUS_THERM_ALARM_SHIFT = 13,
	CM_CHARGER_DIE_THERM_ALARM_SHIFT = 14,
	CM_CHARGER_BAT_UCP_ALARM_SHIFT = 15,
	CM_CHARGER_BUS_ERR_LO_SHIFT = 24,
	CM_CHARGER_BUS_ERR_HI_SHIFT = 25,
};

/**
 * uvlo_shutdown_mode -
 * CM_SHUTDOWN_MODE_ORDERLY - if the file "/sbin/poweroff" exit, it will
 * shutdown from user layer to kernel layer depend on /sbin/poweroff.
 * you can use this mode if your system have the file /sbin/poweroff.
 *
 * CM_SHUTDOWN_MODE_KERNEL - emergency shutdown, data may not save.
 * system use this mode as default mode.
 *
 * CM_SHUTDOWN_MODE_ANDROID - set the UI cap to 0 and let android layer
 * to shutdown from android layer to kernel layer.
 * you can use this mode if you want to save data before shutdown.
 *
 */
enum uvlo_shutdown_modes {
	CM_SHUTDOWN_MODE_ORDERLY = 0,
	CM_SHUTDOWN_MODE_KERNEL,
	CM_SHUTDOWN_MODE_ANDROID,
};

#define CM_IBAT_BUFF_CNT 7

struct wireless_data {
	struct power_supply_desc psd;
	struct power_supply *psy;
	int WIRELESS_ONLINE;
};

struct ac_data {
	struct power_supply_desc psd;
	struct power_supply *psy;
	int AC_ONLINE;
};

struct usb_data {
	struct power_supply_desc psd;
	struct power_supply *psy;
	int USB_ONLINE;
};

/* HS03 code for SR-SL6215-01-556 by shixuanxuan at 20210813 start */
#ifndef HQ_FACTORY_BUILD
struct otg_data {
	struct power_supply_desc psd;
	struct power_supply *psy;
	int OTG_ONLINE;
};
#endif
/* HS03 code for SR-SL6215-01-556 by shixuanxuan at 20210813 end */

/**
 * struct charger_cable
 * @extcon_name: the name of extcon device.
 * @name: the name of charger cable(external connector).
 * @extcon_dev: the extcon device.
 * @wq: the workqueue to control charger according to the state of
 *	charger cable. If charger cable is attached, enable charger.
 *	But if charger cable is detached, disable charger.
 * @nb: the notifier block to receive changed state from EXTCON
 *	(External Connector) when charger cable is attached/detached.
 * @attached: the state of charger cable.
 *	true: the charger cable is attached
 *	false: the charger cable is detached
 * @charger: the instance of struct charger_regulator.
 * @cm: the Charger Manager representing the battery.
 */
struct charger_cable {
	const char *extcon_name;
	const char *name;

	/* The charger-manager use Extcon framework */
	struct extcon_dev *extcon_dev;
	struct notifier_block nb;

	/* The state of charger cable */
	bool attached;

	struct charger_regulator *charger;

	/*
	 * Set min/max current of regulator to protect over-current issue
	 * according to a kind of charger cable when cable is attached.
	 */
	int min_uA;
	int max_uA;

	struct charger_manager *cm;
};

/**
 * struct charger_regulator
 * @regulator_name: the name of regulator for using charger.
 * @consumer: the regulator consumer for the charger.
 * @externally_control:
 *	Set if the charger-manager cannot control charger,
 *	the charger will be maintained with disabled state.
 * @cables:
 *	the array of charger cables to enable/disable charger
 *	and set current limit according to constraint data of
 *	struct charger_cable if only charger cable included
 *	in the array of charger cables is attached/detached.
 * @num_cables: the number of charger cables.
 * @attr_g: Attribute group for the charger(regulator)
 * @attr_name: "name" sysfs entry
 * @attr_state: "state" sysfs entry
 * @attr_externally_control: "externally_control" sysfs entry
 * @attr_jeita_control: "jeita_control" sysfs entry
 * @attrs: Arrays pointing to attr_name/state/externally_control for attr_g
 */
struct charger_regulator {
	/* The name of regulator for charging */
	const char *regulator_name;
	struct regulator *consumer;

	/* charger never on when system is on */
	int externally_control;

	/*
	 * Store constraint information related to current limit,
	 * each cable have different condition for charging.
	 */
	struct charger_cable *cables;
	int num_cables;
	int cp_id;

	struct attribute_group attr_g;
	struct device_attribute attr_name;
	struct device_attribute attr_state;
	struct device_attribute attr_stop_charge;
	struct device_attribute attr_externally_control;
	struct device_attribute attr_jeita_control;
	struct device_attribute attr_cp_num;
	struct device_attribute attr_charge_pump_present;
	struct device_attribute attr_charge_pump_current;
	struct attribute *attrs[9];

	struct charger_manager *cm;
};

struct charger_jeita_table {
	int temp;
	int recovery_temp;
	int current_ua;
	int term_volt;
};

/*
 * struct cap_remap_table
 * @cnt: record the counts of battery capacity of this scope
 * @lcap: the lower boundary of the capacity scope before transfer
 * @hcap: the upper boundary of the capacity scope before transfer
 * @lb: the lower boundary of the capacity scope after transfer
 * @hb: the upper boundary of the capacity scope after transfer
*/
struct cap_remap_table {
	int cnt;
	int lcap;
	int hcap;
	int lb;
	int hb;
};

/*
 * struct cm_ir_compensation
 * @us: record the full charged battery voltage at normal condition.
 * @rc: compensation resistor value in mohm
 * @ibat_buf: record battery current
 * @us_upper_limit: limit the max battery voltage
 * @cp_upper_limit_offset: use for charge pump mode to adjust battery over
 *	voltage protection value.
 * @us_lower_limit: record the min battery voltage
 * @ir_compensation_en: enable/disable current and resistor compensation function.
 * ibat_index: record current battery current in the ibat_buf
 * @last_target_cccv: record last target cccv point;
 */
struct cm_ir_compensation {
	int us;
	int rc;
	int ibat_buf[CM_IBAT_BUFF_CNT];
	int us_upper_limit;
	int cp_upper_limit_offset;
	int us_lower_limit;
	bool ir_compensation_en;
	int ibat_index;
	int last_target_cccv;
};

/*
 * struct cm_fault_status
 * @bat_ovp_fault: record battery over voltage fault event
 * @bat_ocp_fault: record battery over current fault event
 * @bus_ovp_fault: record bus over voltage fault event
 * @bus_ocp_fault: record bus over current fault event
 * @bat_therm_fault: record battery over temperature fault event
 * @bus_therm_fault: record bus over temperature fault event
 * @die_therm_fault: record die over temperature fault event
 * @vbus_error_lo: record the bus voltage is low event
 * @vbus_error_hi: record the bus voltage is high event
 */
struct cm_fault_status {
	bool bat_ovp_fault;
	bool bat_ocp_fault;
	bool bus_ovp_fault;
	bool bus_ocp_fault;
	bool bat_therm_fault;
	bool bus_therm_fault;
	bool die_therm_fault;
	bool vbus_error_lo;
	bool vbus_error_hi;
};

/*
 * struct cm_alarm_status
 * @bat_ovp_alarm: record battery over voltage alarm event
 * @bat_ocp_alarm: record battery over current alarm event
 * @bus_ovp_alarm: record bus over voltage alarm event
 * @bus_ocp_alarm: record bus over current alarm event
 * @bat_ucp_alarm: record battery under current alarm event
 * @bat_therm_alarm: record battery over temperature alarm event
 * @bus_therm_alarm: record bus over temperature alarm event
 * @die_therm_alarm: record die over temperature alarm event
 */
struct cm_alarm_status {
	bool bat_ovp_alarm;
	bool bat_ocp_alarm;
	bool bus_ovp_alarm;
	bool bus_ocp_alarm;
	bool bat_ucp_alarm;
	bool bat_therm_alarm;
	bool bus_therm_alarm;
	bool die_therm_alarm;
};

/*
 * struct cm_charge_pump_status
 * @cp_running: record charge pumps running status
 * @check_cp_threshold: record the flag whether need to check charge pump
 *	start condition.
 * @cp_ocv_threshold: the ocv threshold of entry pps fast charge directly.
 * @recovery: record the flag whether need recover charge pump machine
 * @cp_state: record current charge pumps state
 * @cp_target_ibat: record target battery current
 * @cp_target_vbat: record target battery voltage
 * @cp_target_ibus: record target bus current
 * @cp_target_vbus: record target bus voltage
 * @cp_last_target_vbus: record the last request target bus voltage
 * @cp_max_ibat: record the upper limit of  battery current
 * @cp_max_ibus: record the upper limit of  bus current
 * @adapter_max_ibus: record the max current of bus
 * @adapter_max_vbus: record the max voltage of bus
 * @vbat_uV: record the current battery voltage
 * @ibat_uA: record the current battery current
 * @ibus_uA: record the current bus current
 * @ibus_uV: record the current bus voltage
 * @tune_vbus_retry: record the retry time from vbus low to vbus high
 * @cp_taper_trigger_cnt: record the count of battery current reach taper current
 * @cp_ibat_ucp_cnt: record the count of battery current reach ucp current
 * @cp_taper_current: record the battery current threshold of exit charge pump
 * @cp_fault_event: record the fault event
 * @flt: record the all fault status
 * @alm: record the all alarm status
 */
struct cm_charge_pump_status {
	bool cp_running;
	bool check_cp_threshold;
	bool recovery;
	int cp_state;
	int cp_target_ibat;
	int cp_target_vbat;
	int cp_target_ibus;
	int cp_target_vbus;
	int cp_last_target_vbus;
	int cp_max_ibat;
	int cp_max_ibus;
	int adapter_max_ibus;
	int adapter_max_vbus;
	int vbat_uV;
	int ibat_uA;
	int ibus_uA;
	int vbus_uV;
	int tune_vbus_retry;
	int cp_taper_trigger_cnt;
	int cp_adjust_cnt;
	int cp_ibat_ucp_cnt;
	int cp_taper_current;
	bool cp_fault_event;
	bool cp_state_tune_log;

	struct cm_fault_status  flt;
	struct cm_alarm_status  alm;
};

/**
 * struct cm_charge_pump_status
 * @adapter_default_charge_vol: record default charge voltage for
 *   different charger type
 * @thm_pwr(in mw): record thermal power to limit charge current.
 * @thm_adjust_cur: record thermal current to limit charge current, which
 *   is converted from adapter_default_charge_vol and thm_adjust_cur
 * @need_calib_charge_lmt: if need to calib charge limit current by thermal.
 */
struct cm_thermal_info {
	u32 adapter_default_charge_vol;
	u32 thm_pwr;
	int thm_adjust_cur;
	bool need_calib_charge_lmt;
};
/* HS03 code for SR-SL6215-01-540 by qiaodan at 20210829 start */
#ifndef HQ_FACTORY_BUILD
#define is_between(left, right, value) \
		(((left) >= (right) && (left) >= (value) \
			&& (value) >= (right)) \
		|| ((left) <= (right) && (left) <= (value) \
			&& (value) <= (right)))

#define BATT_MAX_CV_ENTRIES				8
#define BATT_MAX_CYCLE_COUNT		0xFFFF

struct range_data {
	int low_threshold;
	int high_threshold;
	u32 value;
};
#endif
/* HS03 code for SR-SL6215-01-540 by qiaodan at 20210829 end */
/**
 * struct charger_desc
 * @psy_name: the name of power-supply-class for charger manager
 * @polling_mode:
 *	Determine which polling mode will be used
 * @fullbatt_vchkdrop_ms:
 * @fullbatt_vchkdrop_uV:
 *	Check voltage drop after the battery is fully charged.
 *	If it has dropped more than fullbatt_vchkdrop_uV after
 *	fullbatt_vchkdrop_ms, CM will restart charging.
 * @fullbatt_uV: voltage in microvolt
 *	If VBATT >= fullbatt_uV, it is assumed to be full.
 * @fullbatt_uA: battery current in microamp
 * @first_fullbatt_uA: battery current in microamp of first_full charged
 * @fullbatt_soc: state of Charge in %
 *	If state of Charge >= fullbatt_soc, it is assumed to be full.
 * @fullbatt_full_capacity: full capacity measure
 *	If full capacity of battery >= fullbatt_full_capacity,
 *	it is assumed to be full.
 * @polling_interval_ms: interval in millisecond at which
 *	charger manager will monitor battery health
 * @battery_present:
 *	Specify where information for existence of battery can be obtained
 * @psy_charger_stat: the names of power-supply for chargers
 * @psy_fast_charger_stat: the names of power-supply for fast chargers
 * @psy_cp_stat: the names of power-supply for charge pumps
 * @psy_wl_charger_stat: the names of power-supply for wireless chargers
 * @psy_cp_converter_stat: the names of power-supply for charge pump converters
 * @num_charger_regulator: the number of entries in charger_regulators
 * @charger_regulators: array of charger regulators
 * @psy_fuel_gauge: the name of power-supply for fuel gauge
 * @thermal_zone : the name of thermal zone for battery
 * @temp_min : Minimum battery temperature for charging.
 * @temp_max : Maximum battery temperature for charging.
 * @temp_diff : Temperature difference to restart charging.
 * @cap : Battery capacity report to user space.
 * @measure_battery_temp:
 *	true: measure battery temperature
 *	false: measure ambient temperature
 * @charging_max_duration_ms: Maximum possible duration for charging
 *	If whole charging duration exceed 'charging_max_duration_ms',
 *	cm stop charging.
 * @discharging_max_duration_ms:
 *	Maximum possible duration for discharging with charger cable
 *	after full-batt. If discharging duration exceed 'discharging
 *	max_duration_ms', cm start charging.
 * @normal_charge_voltage_max:
 *	maximum normal charge voltage in microVolts
 * @normal_charge_voltage_drop:
 *	drop voltage in microVolts to allow restart normal charging
 * @fast_charge_voltage_max:
 *	maximum fast charge voltage in microVolts
 * @fast_charge_voltage_drop:
 *	drop voltage in microVolts to allow restart fast charging
 * @flash_charge_voltage_max:
 *	maximum flash charge voltage in microVolts
 * @flash_charge_voltage_drop:
 *	drop voltage in microVolts to allow restart flash charging
 * @wireless_normal_charge_voltage_max:
 *	maximum wireless normal charge voltage in microVolts
 * @wireless_normal_charge_voltage_drop:
 *	drop voltage in microVolts to allow restart wireless charging
 * @wireless_fast_charge_voltage_max:
 *	maximum wireless fast charge voltage in microVolts
 * @wireless_fast_charge_voltage_drop:
 *	drop voltage in microVolts to allow restart wireless fast charging
 * @charger_status: Recording state of charge
 * @charger_type: Recording type of charge
 * @first_trigger_cnt: The number of times the battery is first_fully charged
 * @trigger_cnt: The number of times the battery is fully charged
 * @uvlo_trigger_cnt: The number of times the battery voltage is
 *	less than under voltage lock out
 * @low_temp_trigger_cnt: The number of times the battery temperature
 *	is less than 10 degree.
 * @uvlo_shutdown_mode:
 *	Determine which polling mode will be used
 * @cap_one_time: The percentage of electricity is not
 *	allowed to change by 1% in cm->desc->cap_one_time
 * @trickle_time_out: If 99% lasts longer than it , will force set full statu
 * @trickle_time: Record the charging time when battery
 *	capacity is larger than 99%.
 * @trickle_start_time: Record current time when battery capacity is 99%
 * @update_capacity_time: Record the battery capacity update time
 * @last_query_time: Record last time enter cm_batt_works
 * @force_set_full: The flag is indicate whether
 *	there is a mandatory setting of full status
 * @shutdown_voltage: If it has dropped more than shutdown_voltage,
 *	the phone will automatically shut down
 * @wdt_interval: Watch dog time pre-load value
 * @jeita_tab: Specify the jeita temperature table, which is used to
 *	adjust the charging current according to the battery temperature.
 * @jeita_tab_size: Specify the size of jeita temperature table.
 * @jeita_tab_array: Specify the jeita temperature table array, which is used to
 *	save the point of adjust the charging current according to the battery temperature.
 * @jeita_disabled: disable jeita function when needs
 * @force_jeita_status: force jeita to this status when disable jeita
 * @temperature: the battery temperature
 * @internal_resist: the battery internal resistance in mOhm
 * @cap_table_len: the length of ocv-capacity table
 * @cap_table: capacity table with corresponding ocv
 * @cap_remap_table: the table record the different scope of capacity
 *	information.
 * @cap_remap_table_len: the length of cap_remap_table
 * @cap_remap_total_cnt: the total count the whole battery capacity is divided
	into.
 * @is_fast_charge: if it is support fast charge or not
 * @enable_fast_charge: if is it start fast charge or not
 * @fast_charge_enable_count: to count the number that satisfy start
 *	fast charge condition.
 * @fast_charge_disable_count: to count the number that satisfy stop
 *	fast charge condition.
 * @double_IC_total_limit_current: if it use two charge IC to support
 *	fast charge, we use total limit current to campare with thermal_val,
 *	to limit the thermal_val under total limit current.
 * @cm_check_int: record the intterupt event
 * @cm_check_fault: record the flag whether need to check fault status
 * @fast_charger_type: record the charge type
 * @cp: record the charge pump status
 * @ir_comp: record the current and resistor compensation status
 * @wl_charge_en: if it is wireless charge enabled or not
 * @usb_charge_en: if it is usb charge enabled or not
 * @adapter_default_charge_vol(v): adapter charge voltage for thermal to calculate
 *     input limit current
 */
struct charger_desc {
	const char *psy_name;

	enum polling_modes polling_mode;
	unsigned int polling_interval_ms;

	unsigned int fullbatt_vchkdrop_ms;
	unsigned int fullbatt_vchkdrop_uV;
	unsigned int fullbatt_uV;
	unsigned int fullbatt_uA;
	unsigned int first_fullbatt_uA;
	unsigned int fullbatt_soc;
	unsigned int fullbatt_full_capacity;

	enum data_source battery_present;

	const char **psy_charger_stat;
	const char **psy_fast_charger_stat;
	const char **psy_cp_stat;
	const char **psy_wl_charger_stat;
	const char **psy_cp_converter_stat;
	/* HS03 code for SR-SL6215-01-178 by gaochao at 20210724 start */
	const char **psy_stat;
	/* HS03 code for SR-SL6215-01-178 by gaochao at 20210724 end */

	int num_charger_regulators;
	struct charger_regulator *charger_regulators;

	const char *psy_fuel_gauge;

	const char *thermal_zone;

	int temp_min;
	int temp_max;
	int temp_diff;

	int cap;
	bool measure_battery_temp;

	u32 charging_max_duration_ms;
	u32 discharging_max_duration_ms;

	u32 charge_voltage_max;
	u32 charge_voltage_drop;
	u32 normal_charge_voltage_max;
	u32 normal_charge_voltage_drop;
	u32 fast_charge_voltage_max;
	u32 fast_charge_voltage_drop;
	u32 flash_charge_voltage_max;
	u32 flash_charge_voltage_drop;
	u32 wireless_normal_charge_voltage_max;
	u32 wireless_normal_charge_voltage_drop;
	u32 wireless_fast_charge_voltage_max;
	u32 wireless_fast_charge_voltage_drop;

	int charger_status;
	u32 charger_type;
	u32 charger_type_cnt;
	/* Tab A8 code for SR-AX6300-01-5 by wenyaqi at 20210824 start */
	#ifdef CONFIG_AFC
	int afc_sts;
	int hv_disable;
	u32 afc_charge_voltage_max;
	u32 afc_charge_voltage_drop;
	#endif
	/* Tab A8 code for SR-AX6300-01-5 by wenyaqi at 20210824 end */
	int trigger_cnt;
	int first_trigger_cnt;
	int uvlo_trigger_cnt;
	enum uvlo_shutdown_modes uvlo_shutdown_mode;
	int low_temp_trigger_cnt;

	u32 cap_one_time;

	u32 trickle_time_out;
	u64 trickle_time;
	u64 trickle_start_time;

	u64 update_capacity_time;
	u64 last_query_time;

	bool force_set_full;
	u32 shutdown_voltage;

	u32 wdt_interval;

	int charge_limit_cur;
	int input_limit_cur;

	struct charger_jeita_table *jeita_tab;
	u32 jeita_tab_size;
	struct charger_jeita_table *jeita_tab_array[CM_JEITA_MAX];

	bool jeita_disabled;
	int force_jeita_status;

	int temperature;
	/* Tab A8 code for AX6300DEV-632 by wenyaqi at 20210921 start */
	int last_temp;
	/* Tab A8 code for AX6300DEV-632 by wenyaqi at 20210921 end */

	int internal_resist;
	int cap_table_len;
	struct power_supply_battery_ocv_table *cap_table;
	struct cap_remap_table *cap_remap_table;
	struct power_supply_charge_current cur;
	u32 cap_remap_table_len;
	int cap_remap_total_cnt;
	int cap_remap_full_percent;
	bool is_fast_charge;
	bool enable_fast_charge;
	u32 fast_charge_disable_count;
	u32 double_ic_total_limit_current;
	u32 cp_nums;
	/* HS03 code for SR-SL6215-01-178 by gaochao at 20210724 start */
	u32 buck_nums;
	u32 psy_nums;
	/* HS03 code for SR-SL6215-01-178 by gaochao at 20210724 end */

	bool cm_check_int;
	bool cm_check_fault;
	u32 fast_charger_type;

	int fchg_ocv_threshold;

	struct cm_charge_pump_status cp;
	struct cm_ir_compensation ir_comp;

	bool wl_charge_en;
	bool usb_charge_en;

	struct cm_thermal_info thm_info;

	struct mutex charger_type_mtx;
	/* Tab A7 Lite T618 code for AX6189DEV-731 by qiaodan at 20220126 start */
	u32 pd_port_partner;
	/* Tab A7 Lite T618 code for AX6189DEV-731 by qiaodan at 20220126 end */
	/* HS03 code for SR-SL6215-01-238 by qiaodan at 20210802 start */
	#ifdef HQ_FACTORY_BUILD
	bool batt_cap_control;
	/* HS03 code for SL6215DEV-610 by jiahao at 20210907 start */
	bool batt_cap_stop_charge;
	/* HS03 code for SL6215DEV-610 by jiahao at 20210907 end */
	#endif
	/* HS03 code for SR-SL6215-01-238 by qiaodan at 20210802 end */
	#ifndef HQ_FACTORY_BUILD
	int ss_vbatfloat;
	int ss_vbatfloat_buf;
	struct range_data batt_cv_data[BATT_MAX_CV_ENTRIES];
	struct range_data batt_recharge_data[BATT_MAX_CV_ENTRIES];
	int batt_cycle;
	bool ss_batt_aging_enable;
	#endif
	/* HS03 code for SR-SL6215-01-540 by qiaodan at 20210829 end*/
	/* HS03 code for SR-SL6215-01-552 by qiaodan at 20210831 start */
	#ifndef HQ_FACTORY_BUILD
	bool batt_store_mode;
	#endif
	/* HS03 code for SR-SL6215-01-552 by qiaodan at 20210831 end */
	/* Tab A8 code for P220915-04436  and AX6300TDEV-163 by  xuliqin at 20220920 start */
#if !defined(HQ_FACTORY_BUILD)
	int batt_full_cap;
#endif
	/* Tab A8 code for P220915-04436 and AX6300TDEV-163 by  xuliqin at 20220920 end */
};

#define PSY_NAME_MAX	30
#define PSY_OTG_NUM_MAX	10

/**
 * struct charger_manager
 * @entry: entry for list
 * @dev: device pointer
 * @desc: instance of charger_desc
 * @fuel_gauge: power_supply for fuel gauge
 * @charger_stat: array of power_supply for chargers
 * @tzd_batt : thermal zone device for battery
 * @charger_enabled: the state of charger
 * @fullbatt_vchk_jiffies_at:
 *	jiffies at the time full battery check will occur.
 * @fullbatt_vchk_work: work queue for full battery check
 * @uvlo_work: work queue to check uvlo state
 * @ir_compensation_work: work queue to check current and resistor
 *	compensation state
 * @emergency_stop:
 *	When setting true, stop charging
 * @psy_name_buf: the name of power-supply-class for charger manager
 * @charger_psy: power_supply for charger manager
 * @status_save_ext_pwr_inserted:
 *	saved status of external power before entering suspend-to-RAM
 * @status_save_batt:
 *	saved status of battery before entering suspend-to-RAM
 * @charging_start_time: saved start time of enabling charging
 * @charging_end_time: saved end time of disabling charging
 * @charging_status: saved charging status, 0 means charging normal
 * @battery_status: Current battery status
 * @charge_ws: wakeup source to prevent ap enter sleep mode in charge
 *	pump mode
 */
struct charger_manager {
	struct list_head entry;
	struct device *dev;
	struct charger_desc *desc;

#ifdef CONFIG_THERMAL
	struct thermal_zone_device *tzd_batt;
#endif
	bool charger_enabled;

	unsigned long fullbatt_vchk_jiffies_at;
	struct delayed_work fullbatt_vchk_work;
	struct delayed_work cap_update_work;
	struct delayed_work uvlo_work;
	struct delayed_work ir_compensation_work;
	struct delayed_work cp_work;
	/* HS03 code for SR-SL6215-01-552 by qiaodan at 20210831 start */
	#ifndef HQ_FACTORY_BUILD
	struct delayed_work retail_app_status_change_work;
	struct wakeup_source *charger_wakelock_app;
	#endif
	/* HS03 code for SR-SL6215-01-552 by qiaodan at 20210831 end */
	/* HS03 code for SR-SL6215-01-255 by shixuanxuan at 20210902 start */
	#if !defined(HQ_FACTORY_BUILD)
	struct delayed_work charging_count_work;
	#endif
	/* HS03 code for SR-SL6215-01-255 by shixuanxuan at 20210902 end */
	/* HS03 code for SR-SL6215-01-192 by shixuanxuan at 20210818 start */
	struct delayed_work update_vbus_state_work;
	u32 vbus_control_gpio;
	bool vbus_conn_flag;
	/* HS03 code for SR-SL6215-01-192 by shixuanxuan at 20210818 end */
	int emergency_stop;

	char psy_name_buf[PSY_NAME_MAX + 1];
	struct power_supply_desc charger_psy_desc;
	struct power_supply *charger_psy;

	u64 charging_start_time;
	u64 charging_end_time;
	u32 charging_status;
	int battery_status;
	/* TabA8_U code for P231127-08057 by liufurong at 20231205 start */
	int batt_slate_mode;
	/* TabA8_U code for P231127-08057 by liufurong at 20231205 end */
	/* HS03 code for SR-SL6215-01-238 by qiaodan at 20210802 start */
	bool input_suspend;
	bool power_path_enabled;
	/* HS03 code for SR-SL6215-01-238 by qiaodan at 20210802 end */
	/* HS03 code for SR-SL6215-01-607 by gaochao at 20210825 start */
	int cool_warm_health;
	/* HS03 code for SR-SL6215-01-607 by gaochao at 20210825 end */
	/* HS03 code for SR-SL6215-01-255 by shixuanxuan at 20210902 start */
	#if !defined(HQ_FACTORY_BUILD)
	u64 charging_dur_time;
	u64 charging_count_start;
	/* Tab A8 code for P230719-01290 by shixuanxuan at 20230724 start */
	u32 batt_protect_flag;
	/* Tab A8 code for P230719-01290 by shixuanxuan at 20230724 end */
	bool en_batt_protect;
	#endif
	/* HS03 code for SR-SL6215-01-255 by shixuanxuan at 20210902 end */
	/* Tab A8 code for P220915-04436  and AX6300TDEV-163 by  xuliqin at 20220920 start */
#if !defined(HQ_FACTORY_BUILD)
	bool batt_full_flag;
#endif
	/* Tab A8 code for P220915-04436  and AX6300TDEV-163 by  xuliqin at 20220920 end */
/* HS03 code for SL6216DEV-97 by shixuanxuan at 20211001 start */
#if !defined(HQ_FACTORY_BUILD)
	bool en_constant_soc_val;
#endif
/* HS03 code for SL6216DEV-97 by shixuanxuan at 20211001 end */

	struct wakeup_source charge_ws;
	struct sprd_vote *cm_charge_vote;
};

/* HS03 code for SR-SL6215-01-607 by gaochao at 20210825 start */
enum cool_warm_health {
	CM_JEITA_INIT_HEALTH = -1,
	CM_JEITA_COOL_BETWEEN_0_5_HEALTH = 1,
	CM_JEITA_COOL_BETWEEN_5_12_HEALTH = 2,
	CM_JEITA_NORMAL_HEALTH = 3,
	CM_JEITA_WARM_HEALTH = 4,

};
/* HS03 code for SL6215DEV-729 by lina at 20210903 start */
enum battery_full_recharge_voltage {
	/* Tab A8 code for AX6300DEV-2798 by zhaichao at 2021/11/11 start */
	#ifdef  CONFIG_TARGET_UMS9230_4H10
	CM_FULL_VOLTAGE_HEALTH_WARM = 4175000,
	#elif  CONFIG_TARGET_UMS512_1H10
	/* Tab A8 code for AX6300DEV-3563 by zhaichao at 20211206 start */
	CM_FULL_VOLTAGE_HEALTH_WARM = 4160000,
	/* Tab A8 code for AX6300DEV-3563 by zhaichao at 20211206 start */
	#endif
	/* Tab A8 code for AX6300DEV-2798 by zhaichao at 2021/11/11 end */
	/* Tab A8 code for SR-AX6300-01-3 by qiaodan at 20210906 start */
	#ifdef CONFIG_TARGET_UMS512_1H10
	/* Tab A8 code for AX6300DEV-3574 by zhaichao at 20211209 start */
	CM_FULL_VOLTAGE_HEALTH_COOLL = 4100000,
	/* Tab A8 code for AX6300DEV-3574 by zhaichao at 20211209 end */
	#endif
	/* Tab A8 code for SR-AX6300-01-3 by qiaodan at 20210906 end */
/* HS03 code for SL6215DEV-3535 by lina at 20211116 start */
#ifdef  CONFIG_TARGET_UMS9230_4H10
	CM_FULL_VOLTAGE_HEALTH_COOL = 4160000,
#elif  CONFIG_TARGET_UMS512_1H10
	/* Tab A8 code for AX6300DEV-3574 by zhaichao at 20211217 start */
	CM_FULL_VOLTAGE_HEALTH_COOL = 4300000,
	/* Tab A8 code for AX6300DEV-3574 by zhaichao at 20211217 end */
#endif
/* HS03 code for SL6215DEV-3535 by lina at 20211116 end */
	/* Tab A8 code for AX6300DEV-3562 by zhaichao at 20211206 start */
	CM_FULL_VOLTAGE_HEALTH_GOOD = 4360000,
	/* Tab A8 code for AX6300DEV-3562 by zhaichao at 20211206 end */
};
/* HS03 code for SL6215DEV-729 by lina at 20210903 end */
/* HS03 code for SR-SL6215-01-607 by gaochao at 20210825 end */

/* Tab A8 code for SR-AX6300-01-5 by wenyaqi at 20210827 start */
#ifdef CONFIG_AFC
struct sc2730_fchg_info {
	struct device *dev;
	struct regmap *regmap;
	struct usb_phy *usb_phy;
	struct notifier_block usb_notify;
	struct notifier_block pd_notify;
	struct power_supply *psy_usb;
	struct power_supply *psy_tcpm;
	struct delayed_work work;
	struct work_struct pd_change_work;
	struct mutex lock;
	struct completion completion;
	struct adapter_power_cap pd_source_cap;
	u32 state;
	u32 base;
	int input_vol;
	int pd_fixed_max_uw;
	u32 limit;
	bool detected;
	bool pd_enable;
	bool sfcp_enable;
	bool pps_enable;
	/* Tab A8 code for SR-AX6300-01-5 by wenyaqi at 20210824 start */
	#ifdef CONFIG_AFC
	bool afc_enable;
	#endif
	/* Tab A8 code for SR-AX6300-01-5 by wenyaqi at 20210824 end */
	bool pps_active;
	bool support_pd_pps;
	/* Tab A8 code for AX6300DEV-2595 by wenyaqi at 20211109 start */
	bool support_sfcp;
	/* Tab A8 code for AX6300DEV-2595 by wenyaqi at 20211109 end */
	const struct sc27xx_fast_chg_data *pdata;
};
#endif
/* Tab A8 code for SR-AX6300-01-5 by wenyaqi at 20210827 end */

#ifdef CONFIG_CHARGER_MANAGER
extern void cm_notify_event(struct power_supply *psy,
				enum cm_event_types type, char *msg);
/* Tab A7 Lite T618 code for AX6189DEV-731 by qiaodan at 20220126 start */
extern void cm_check_pd_port_partner(bool is_pd_hub);
/* Tab A7 Lite T618 code for AX6189DEV-731 by qiaodan at 20220126 end */
#else
static inline void cm_notify_event(struct power_supply *psy,
				enum cm_event_types type, char *msg) { }
/* Tab A7 Lite T618 code for AX6189DEV-731 by qiaodan at 20220126 start */
void cm_check_pd_port_partner(bool is_pd_hub) { }
/* Tab A7 Lite T618 code for AX6189DEV-731 by qiaodan at 20220126 end */
#endif
#endif /* _CHARGER_MANAGER_H */
