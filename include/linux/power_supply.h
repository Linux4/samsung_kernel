/*
 *  Universal power supply monitor class
 *
 *  Copyright © 2007  Anton Vorontsov <cbou@mail.ru>
 *  Copyright © 2004  Szabolcs Gyurko
 *  Copyright © 2003  Ian Molton <spyro@f2s.com>
 *
 *  Modified: 2004, Oct     Szabolcs Gyurko
 *
 *  You may use this code as per GPL version 2
 */

#ifndef __LINUX_POWER_SUPPLY_H__
#define __LINUX_POWER_SUPPLY_H__

#include <linux/device.h>
#include <linux/workqueue.h>
#include <linux/leds.h>
#include <linux/spinlock.h>
#include <linux/notifier.h>
#include <linux/types.h>

/*
 * All voltages, currents, charges, energies, time and temperatures in uV,
 * µA, µAh, µWh, seconds and tenths of degree Celsius unless otherwise
 * stated. It's driver's job to convert its raw values to units in which
 * this class operates.
 */

/*
 * For systems where the charger determines the maximum battery capacity
 * the min and max fields should be used to present these values to user
 * space. Unused/unknown fields will not appear in sysfs.
 */

/* HS03 code for SR-SL6215-01-545 by shixuanxuan at 20210818 start */
#if !defined(HQ_FACTORY_BUILD)
enum batt_misc_event_ss {
	BATT_MISC_EVENT_NONE = 0x000000,							/* Default value, nothing will be happen */
	BATT_MISC_EVENT_UNDEFINED_RANGE_TYPE = 0x00000001,					/* water detection - not support */
	BATT_MISC_EVENT_TIMEOUT_OPEN_TYPE = 0x00000004,						/* DCD timeout */
	BATT_MISC_EVENT_HICCUP_TYPE = 0x00000020,						/* It happens when water is detected in a interface port */
	/* Tab A8 code for P220915-04436 and AX6300TDEV-163  by  xuliqin at 20220920 start */
	BATT_MISC_EVENT_FULL_CAPACITY = 0x01000000,
	/* Tab A8 code for P220915-04436  and AX6300TDEV-163 by  xuliqin at 20220920 end */
};
#endif
/* HS03 code forSR-SL6215-01-545 by shixuanxuan at 20210818 end */

enum {
	POWER_SUPPLY_STATUS_UNKNOWN = 0,
	POWER_SUPPLY_STATUS_CHARGING,
	POWER_SUPPLY_STATUS_DISCHARGING,
	POWER_SUPPLY_STATUS_NOT_CHARGING,
	POWER_SUPPLY_STATUS_FULL,
};

enum {
	POWER_SUPPLY_CHARGE_TYPE_UNKNOWN = 0,
	POWER_SUPPLY_CHARGE_TYPE_NONE,
	POWER_SUPPLY_CHARGE_TYPE_TRICKLE,
	POWER_SUPPLY_CHARGE_TYPE_FAST,
	/* HS03 code for SR-SL6215-01-177 by jiahao at 20210810 start */
	POWER_SUPPLY_CHARGE_TYPE_TAPER,
	/* HS03 code for SR-SL6215-01-177 by jiahao at 20210810 end */
};

enum {
	POWER_SUPPLY_HEALTH_UNKNOWN = 0,
	POWER_SUPPLY_HEALTH_GOOD,
	POWER_SUPPLY_HEALTH_OVERHEAT,
	POWER_SUPPLY_HEALTH_DEAD,
	POWER_SUPPLY_HEALTH_OVERVOLTAGE,
	POWER_SUPPLY_HEALTH_UNSPEC_FAILURE,
	POWER_SUPPLY_HEALTH_COLD,
	POWER_SUPPLY_HEALTH_WATCHDOG_TIMER_EXPIRE,
	POWER_SUPPLY_HEALTH_SAFETY_TIMER_EXPIRE,
	POWER_SUPPLY_HEALTH_OVERCURRENT,
	POWER_SUPPLY_HEALTH_WARM,
	POWER_SUPPLY_HEALTH_COOL,
	POWER_SUPPLY_HEALTH_HOT,
};

enum {
	POWER_SUPPLY_TECHNOLOGY_UNKNOWN = 0,
	POWER_SUPPLY_TECHNOLOGY_NiMH,
	POWER_SUPPLY_TECHNOLOGY_LION,
	POWER_SUPPLY_TECHNOLOGY_LIPO,
	POWER_SUPPLY_TECHNOLOGY_LiFe,
	POWER_SUPPLY_TECHNOLOGY_NiCd,
	POWER_SUPPLY_TECHNOLOGY_LiMn,
};

enum {
	POWER_SUPPLY_CAPACITY_LEVEL_UNKNOWN = 0,
	POWER_SUPPLY_CAPACITY_LEVEL_CRITICAL,
	POWER_SUPPLY_CAPACITY_LEVEL_LOW,
	POWER_SUPPLY_CAPACITY_LEVEL_NORMAL,
	POWER_SUPPLY_CAPACITY_LEVEL_HIGH,
	POWER_SUPPLY_CAPACITY_LEVEL_FULL,
};

enum {
	POWER_SUPPLY_SCOPE_UNKNOWN = 0,
	POWER_SUPPLY_SCOPE_SYSTEM,
	POWER_SUPPLY_SCOPE_DEVICE,
};
/* TabA8_U code for P231127-08057 by liufurong at 20231205 start */
enum sec_battery_slate_mode {
	SEC_SLATE_OFF = 0,
	SEC_SLATE_MODE,
	SEC_SMART_SWITCH_SLATE,
	SEC_SMART_SWITCH_SRC,
};
/* TabA8_U code for P231127-08057 by liufurong at 20231205 end */

enum power_supply_property {
	/* Properties of type `int' */
	POWER_SUPPLY_PROP_STATUS = 0,
	/* HS03 code for SR-SL6215-01-549 by ditong at 20210823 start */
	#ifndef HQ_FACTORY_BUILD
	POWER_SUPPLY_PROP_BATT_CURRENT_EVENT,
	#endif
	/* HS03 code for SR-SL6215-01-549 by ditong at 20210823 end */
	/* HS03 code for SR-SL6215-01-606 by gaochao at 20210813 start */
	POWER_SUPPLY_PROP_CHARGE_DONE,
	/* HS03 code for SR-SL6215-01-606 by gaochao at 20210813 end */
	POWER_SUPPLY_PROP_CHARGE_TYPE,
	POWER_SUPPLY_PROP_HEALTH,
	/* HS03 code for SR-SL6215-01-552 by qiaodan at 20210831 start */
	#ifndef HQ_FACTORY_BUILD
	POWER_SUPPLY_PROP_STORE_MODE,
	#endif
	/* HS03 code for SR-SL6215-01-552 by qiaodan at 20210831 end */
	/* HS03 code for SR-SL6215-01-553 by qiaodan at 20210813 start */
	#ifndef HQ_FACTORY_BUILD
	POWER_SUPPLY_PROP_BATT_SLATE_MODE,
	#endif
	/* HS03 code for SR-SL6215-01-553 by qiaodan at 20210813 end */
	/* HS03 code for SR-SL6215-01-540 by qiaodan at 20210829 start */
	#ifndef HQ_FACTORY_BUILD
	POWER_SUPPLY_PROP_BATT_CURRENT_UA_NOW,
	POWER_SUPPLY_PROP_BATTERY_CYCLE,
	#endif
	/* HS03 code for SR-SL6215-01-540 by qiaodan at 20210829 end */
	/* Tab A8 code for P220915-04436  and AX6300TDEV-163 by  xuliqin at 20220920 start */
#if !defined(HQ_FACTORY_BUILD)
	POWER_SUPPLY_PROP_BATT_FULL_CAPACITY,
#endif
	/* Tab A8 code for P220915-04436  and AX6300TDEV-163 by  xuliqin at 20220920 end */
	/* HS03 code for SR-SL6215-01-238 by qiaodan at 20210802 start */
	#ifdef HQ_FACTORY_BUILD
	POWER_SUPPLY_PROP_BATT_CAP_CONTROL,
	#endif
	/* HS03 code for SR-SL6215-01-238 by qiaodan at 20210802 end */
	/* Tab A8 code for SR-AX6300-01-5 by wenyaqi at 20210824 start */
	#ifdef CONFIG_AFC
	POWER_SUPPLY_PROP_HV_CHARGER_STATUS,
	POWER_SUPPLY_PROP_AFC_RESULT,
	POWER_SUPPLY_PROP_HV_DISABLE,
	#endif
	/* Tab A8 code for SR-AX6300-01-5 by wenyaqi at 20210824 end */
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_ONLINE,
/* Tab A8 code for SR-AX6300-01-254 by wangjian at 20210810 start */
#ifdef CONFIG_TARGET_UMS512_1H10
	POWER_SUPPLY_PROP_TYPEC_CC_ORIENTATION, /* 0: N/C, 1: CC1, 2: CC2 */
#endif
/* Tab A8 code for SR-AX6300-01-254 by wangjian at 20210810 end */
/* Tab A8 code for SR-AX6301A-01-111 and SR-SL6217T-01-119 by  xuliqin at 20220914 start */
	POWER_SUPPLY_PROP_CHG_INFO,
/* Tab A8 code for SR-AX6301A-01-111 and SR-SL6217T-01-119 by  xuliqin at 20220914 end */
	POWER_SUPPLY_PROP_AUTHENTIC,
	POWER_SUPPLY_PROP_TECHNOLOGY,
	POWER_SUPPLY_PROP_CYCLE_COUNT,
	POWER_SUPPLY_PROP_VOLTAGE_MAX,
	POWER_SUPPLY_PROP_VOLTAGE_MIN,
	POWER_SUPPLY_PROP_VOLTAGE_MAX_DESIGN,
	POWER_SUPPLY_PROP_VOLTAGE_MIN_DESIGN,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_VOLTAGE_AVG,
	POWER_SUPPLY_PROP_VOLTAGE_OCV,
	POWER_SUPPLY_PROP_VOLTAGE_BOOT,
	POWER_SUPPLY_PROP_CURRENT_MAX,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_CURRENT_AVG,
	POWER_SUPPLY_PROP_CURRENT_BOOT,
	POWER_SUPPLY_PROP_POWER_NOW,
	POWER_SUPPLY_PROP_POWER_AVG,
	POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN,
	POWER_SUPPLY_PROP_CHARGE_EMPTY_DESIGN,
	POWER_SUPPLY_PROP_CHARGE_FULL,
	POWER_SUPPLY_PROP_CHARGE_EMPTY,
	POWER_SUPPLY_PROP_CHARGE_NOW,
	POWER_SUPPLY_PROP_CHARGE_AVG,
	POWER_SUPPLY_PROP_CHARGE_COUNTER,
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT,
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX,
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE,
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE_MAX,
	POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT,
	POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT_MAX,
	POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT,
	POWER_SUPPLY_PROP_INPUT_CURRENT_NOW,
	POWER_SUPPLY_PROP_ENERGY_FULL_DESIGN,
	POWER_SUPPLY_PROP_ENERGY_EMPTY_DESIGN,
	POWER_SUPPLY_PROP_ENERGY_FULL,
	POWER_SUPPLY_PROP_ENERGY_EMPTY,
	POWER_SUPPLY_PROP_ENERGY_NOW,
	POWER_SUPPLY_PROP_ENERGY_AVG,
	POWER_SUPPLY_PROP_CAPACITY, /* in percents! */
	/* HS03 code for SR-SL6215-01-545 by shixuanxuan at 20210818 start */
	#if !defined(HQ_FACTORY_BUILD)
	POWER_SUPPLY_PROP_BATT_MISC_EVENT,
	#endif
	/* HS03 code forSR-SL6215-01-545 by shixuanxuan at 20210818 end */
/* HS03 code for SL6216DEV-97 by shixuanxuan at 20211001 start */
#if !defined(HQ_FACTORY_BUILD)
	POWER_SUPPLY_PROP_CONSTANT_SOC_VAL,
#endif
/* HS03 code for SL6216DEV-97 by shixuanxuan at 20211001 end */
	POWER_SUPPLY_PROP_CAPACITY_ALERT_MIN, /* in percents! */
	POWER_SUPPLY_PROP_CAPACITY_ALERT_MAX, /* in percents! */
	POWER_SUPPLY_PROP_CAPACITY_LEVEL,
	POWER_SUPPLY_PROP_TEMP,
	POWER_SUPPLY_PROP_TEMP_MAX,
	POWER_SUPPLY_PROP_TEMP_MIN,
	POWER_SUPPLY_PROP_TEMP_ALERT_MIN,
	POWER_SUPPLY_PROP_TEMP_ALERT_MAX,
	POWER_SUPPLY_PROP_TEMP_AMBIENT,
	POWER_SUPPLY_PROP_TEMP_AMBIENT_ALERT_MIN,
	POWER_SUPPLY_PROP_TEMP_AMBIENT_ALERT_MAX,
	POWER_SUPPLY_PROP_TIME_TO_EMPTY_NOW,
	POWER_SUPPLY_PROP_TIME_TO_EMPTY_AVG,
	POWER_SUPPLY_PROP_TIME_TO_FULL_NOW,
	POWER_SUPPLY_PROP_TIME_TO_FULL_AVG,
	POWER_SUPPLY_PROP_TYPE, /* use power_supply.type instead */
	POWER_SUPPLY_PROP_USB_TYPE,
	POWER_SUPPLY_PROP_WIRELESS_TYPE,
	POWER_SUPPLY_PROP_SCOPE,
	POWER_SUPPLY_PROP_PRECHARGE_CURRENT,
	POWER_SUPPLY_PROP_CHARGE_TERM_CURRENT,
	POWER_SUPPLY_PROP_CALIBRATE,
	/* Local extensions */
	POWER_SUPPLY_PROP_USB_HC,
	POWER_SUPPLY_PROP_USB_OTG,
	POWER_SUPPLY_PROP_CHARGE_ENABLED,
	/* HS03 code for SR-SL6215-01-238 by qiaodan at 20210802 start */
	POWER_SUPPLY_PROP_INPUT_SUSPEND,
	POWER_SUPPLY_PROP_POWER_PATH_ENABLED,
	/* HS03 code for SR-SL6215-01-238 by qiaodan at 20210802 end */
	/* HS03 code for SR-SL6215-01-63 by gaochao at 20210805 start */
	POWER_SUPPLY_PROP_DUMP_CHARGER_IC,
	/* HS03 code for SR-SL6215-01-63 by gaochao at 20210805 end */
	/* HS03 code for SR-SL6215-01-194 by shixuanxuan at 20210809 start */
	POWER_SUPPLY_PROP_BATT_RESISTANCE,
	POWER_SUPPLY_PROP_BATT_TYPE,
	/* HS03 code for SR-SL6215-01-194 by shixuanxuan at 20210809 end */
	/* HS03 code for SR-SL6215-01-177 by jiahao at 20210810 start */
	POWER_SUPPLY_PROP_INPUT_VOLTAGE_REGULATION,
	/* HS03 code for SR-SL6215-01-177 by jiahao at 20210810 end */
	/* HS03 code for SR-SL6215-01-255 by shixuanxuan at 20210902 start */
	#if !defined(HQ_FACTORY_BUILD)
	POWER_SUPPLY_PROP_BATT_PROTECT_FLAG,
	POWER_SUPPLY_PROP_EN_BATT_PROTECT,
	#endif
	/* HS03 code for SR-SL6215-01-255 by shixuanxuan at 20210902 end */
	/* Local extensions of type int64_t */
	POWER_SUPPLY_PROP_CHARGE_COUNTER_EXT,
	/* Properties of type `const char *' */
	POWER_SUPPLY_PROP_MODEL_NAME,
	POWER_SUPPLY_PROP_MANUFACTURER,
	POWER_SUPPLY_PROP_SERIAL_NUMBER,
	POWER_SUPPLY_PROP_FEED_WATCHDOG,
	/* HS03 code for SL6215DEV-734 by shixuanxuan at 20210906 start */
	POWER_SUPPLY_PROP_EN_CHG_TIMER,
	/* HS03 code for SL6215DEV-734 by shixuanxuan at 20210906 end */
};

enum power_supply_type {
	POWER_SUPPLY_TYPE_UNKNOWN = 0,
	POWER_SUPPLY_TYPE_BATTERY,
	POWER_SUPPLY_TYPE_UPS,
	POWER_SUPPLY_TYPE_MAINS,
	POWER_SUPPLY_TYPE_USB,			/* Standard Downstream Port */
	POWER_SUPPLY_TYPE_USB_DCP,		/* Dedicated Charging Port */
	POWER_SUPPLY_TYPE_USB_CDP,		/* Charging Downstream Port */
	POWER_SUPPLY_TYPE_USB_ACA,		/* Accessory Charger Adapters */
	POWER_SUPPLY_TYPE_USB_TYPE_C,		/* Type C Port */
	POWER_SUPPLY_TYPE_USB_PD,		/* Power Delivery Port */
	POWER_SUPPLY_TYPE_USB_PD_DRP,		/* PD Dual Role Port */
	POWER_SUPPLY_TYPE_APPLE_BRICK_ID,	/* Apple Charging Method */
	POWER_SUPPLY_TYPE_USB_SFCP_1P0,		/* SFCP1.0 Port*/
	POWER_SUPPLY_TYPE_USB_SFCP_2P0,		/* SFCP2.0 Port*/
	POWER_SUPPLY_TYPE_WIRELESS,             /* wireless charger*/
	/* HS03 code for SR-SL6215-01-556 by shixuanxuan at 20210813 start */
	#ifndef HQ_FACTORY_BUILD
	POWER_SUPPLY_TYPE_OTG,
	#endif
	/* HS03 code for SR-SL6215-01-556 by shixuanxuan at 20210813 end */
	/* Tab A8 code for P210915-02129 by wenyaqi at 20210929 start */
	#ifdef CONFIG_AFC
	POWER_SUPPLY_TYPE_AFC,			/* AFC Port */
	#endif
	/* Tab A8 code for P210915-02129 by wenyaqi at 20210929 end */
/* HS03 code for SR-SL6215-01-545 by shixuanxuan at 20211026 start */
#if !defined(HQ_FACTORY_BUILD)
	POWER_SUPPLY_TYPE_USB_FLOAT,
#endif
/* HS03 code forSR-SL6215-01-545 by shixuanxuan at 20211026 end */
};

enum power_supply_usb_type {
	POWER_SUPPLY_USB_TYPE_UNKNOWN = 0,
	POWER_SUPPLY_USB_TYPE_SDP,		/* Standard Downstream Port */
	POWER_SUPPLY_USB_TYPE_DCP,		/* Dedicated Charging Port */
	POWER_SUPPLY_USB_TYPE_CDP,		/* Charging Downstream Port */
	POWER_SUPPLY_USB_TYPE_ACA,		/* Accessory Charger Adapters */
	POWER_SUPPLY_USB_TYPE_C,		/* Type C Port */
	POWER_SUPPLY_USB_TYPE_PD,		/* Power Delivery Port */
	POWER_SUPPLY_USB_TYPE_PD_DRP,		/* PD Dual Role Port */
	POWER_SUPPLY_USB_TYPE_PD_PPS,		/* PD Programmable Power Supply */
	POWER_SUPPLY_USB_TYPE_APPLE_BRICK_ID,	/* Apple Charging Method */
	POWER_SUPPLY_USB_TYPE_SFCP_1P0,		/* SFCP1.0 Port*/
	POWER_SUPPLY_USB_TYPE_SFCP_2P0,		/* SFCP2.0 Port*/
	/* Tab A8 code for SR-AX6300-01-5 by wenyaqi at 20210824 start */
	#ifdef CONFIG_AFC
	POWER_SUPPLY_USB_TYPE_AFC,			/* AFC Port */
	#endif
	/* Tab A8 code for SR-AX6300-01-5 by wenyaqi at 20210824 end */
/* HS03 code for SR-SL6215-01-545 by shixuanxuan at 20211026 start */
#if !defined(HQ_FACTORY_BUILD)
	POWER_SUPPLY_USB_TYPE_FLOAT,
#endif
/* HS03 code forSR-SL6215-01-545 by shixuanxuan at 20211026 end */
};

enum power_supply_wireless_type {
	POWER_SUPPLY_WIRELESS_TYPE_UNKNOWN = 0x20,
	POWER_SUPPLY_WIRELESS_TYPE_BPP,		/* Standard wireless bpp mode */
	POWER_SUPPLY_WIRELESS_TYPE_EPP,		/* Standard wireless epp mode */
};

enum power_supply_charge_type {
	USB_CHARGE_TYPE_NORMAL = 0,		/* Charging Power <= 10W*/
	USB_CHARGE_TYPE_FAST,			/* 10W < Charging Power <= 20W */
	USB_CHARGE_TYPE_FLASH,			/* 20W < Charging Power <= 30W */
	USB_CHARGE_TYPE_TURBE,			/* 30W < Charging Power <= 50W */
	USB_CHARGE_TYPE_SUPER,			/* Charging Power > 50W */
	WIRELESS_CHARGE_TYPE_NORMAL,
	WIRELESS_CHARGE_TYPE_FAST,
	WIRELESS_CHARGE_TYPE_FLASH,
	CHARGE_MAX,
};

enum power_supply_notifier_events {
	PSY_EVENT_PROP_CHANGED,
};

/* HS03 code for SR-SL6215-01-549 by ditong at 20210823 start */
#ifndef HQ_FACTORY_BUILD
enum batt_current_event_ss{
	BATTERY_CURRENT_EVENT_NONE = 0x0000,
	BATTERY_CURRENT_EVENT_NONE_AFC = 0x0001,
	BATTERY_CURRENT_EVENT_NONE_DISCHARGE = 0x0002,
	BATTERY_SWELLING_MODE_LOW_TEMP_CHARGING_FLAG = 0x0010,
	BATTERY_SWELLING_MODE_HIGH_TEMP_CHARGING_FLAG = 0x0020,
 };
 #endif
	/* HS03 code for SR-SL6215-01-549 by ditong at 20210823 end */

union power_supply_propval {
	int intval;
	const char *strval;
	int64_t int64val;
};

struct device_node;
struct power_supply;

/* Run-time specific power supply configuration */
struct power_supply_config {
	struct device_node *of_node;
	struct fwnode_handle *fwnode;

	/* Driver private data */
	void *drv_data;

	char **supplied_to;
	size_t num_supplicants;
};

/* Description of power supply */
struct power_supply_desc {
	const char *name;
	enum power_supply_type type;
	enum power_supply_usb_type *usb_types;
	size_t num_usb_types;
	enum power_supply_property *properties;
	size_t num_properties;

	/*
	 * Functions for drivers implementing power supply class.
	 * These shouldn't be called directly by other drivers for accessing
	 * this power supply. Instead use power_supply_*() functions (for
	 * example power_supply_get_property()).
	 */
	int (*get_property)(struct power_supply *psy,
			    enum power_supply_property psp,
			    union power_supply_propval *val);
	int (*set_property)(struct power_supply *psy,
			    enum power_supply_property psp,
			    const union power_supply_propval *val);
	/*
	 * property_is_writeable() will be called during registration
	 * of power supply. If this happens during device probe then it must
	 * not access internal data of device (because probe did not end).
	 */
	int (*property_is_writeable)(struct power_supply *psy,
				     enum power_supply_property psp);
	void (*external_power_changed)(struct power_supply *psy);
	void (*set_charged)(struct power_supply *psy);

	/*
	 * Set if thermal zone should not be created for this power supply.
	 * For example for virtual supplies forwarding calls to actual
	 * sensors or other supplies.
	 */
	bool no_thermal;
	/* For APM emulation, think legacy userspace. */
	int use_for_apm;
};

struct power_supply {
	const struct power_supply_desc *desc;

	char **supplied_to;
	size_t num_supplicants;

	char **supplied_from;
	size_t num_supplies;
	struct device_node *of_node;

	/* Driver private data */
	void *drv_data;

	/* private */
	struct device dev;
	struct work_struct changed_work;
	struct delayed_work deferred_register_work;
	spinlock_t changed_lock;
	bool changed;
	bool initialized;
	bool removing;
	atomic_t use_cnt;
#ifdef CONFIG_THERMAL
	struct thermal_zone_device *tzd;
	struct thermal_cooling_device *tcd;
#endif

#ifdef CONFIG_LEDS_TRIGGERS
	struct led_trigger *charging_full_trig;
	char *charging_full_trig_name;
	struct led_trigger *charging_trig;
	char *charging_trig_name;
	struct led_trigger *full_trig;
	char *full_trig_name;
	struct led_trigger *online_trig;
	char *online_trig_name;
	struct led_trigger *charging_blink_full_solid_trig;
	char *charging_blink_full_solid_trig_name;
#endif
};

/*
 * This is recommended structure to specify static power supply parameters.
 * Generic one, parametrizable for different power supplies. Power supply
 * class itself does not use it, but that's what implementing most platform
 * drivers, should try reuse for consistency.
 */

struct power_supply_info {
	const char *name;
	int technology;
	int voltage_max_design;
	int voltage_min_design;
	int charge_full_design;
	int charge_empty_design;
	int energy_full_design;
	int energy_empty_design;
	int use_for_apm;
};

struct power_supply_battery_ocv_table {
	int ocv;	/* microVolts */
	int capacity;	/* percent */
};

struct power_supply_vol_temp_table {
	int vol;	/* microVolts */
	int temp;	/* celsius */
};

struct power_supply_capacity_temp_table {
	int temp;	/* celsius */
	int cap;	/* capacity percentage */
};

struct power_supply_resistance_temp_table {
	int temp;	/* celsius */
	int resistance;	/* internal resistance percentage */
};

struct power_supply_charge_current {
	int sdp_limit;
	int sdp_cur;
	int dcp_limit;
	int dcp_cur;
	int cdp_limit;
	int cdp_cur;
	int aca_limit;
	int aca_cur;
	int unknown_limit;
	int unknown_cur;
	int fchg_limit;
	int fchg_cur;
	int flash_limit;
	int flash_cur;
	/* Tab A8 code for SR-AX6300-01-5 by wenyaqi at 20210824 start */
	#ifdef CONFIG_AFC
	int afc_limit;
	int afc_cur;
	#endif
	/* Tab A8 code for SR-AX6300-01-5 by wenyaqi at 20210824 end */
	int wl_bpp_cur;
	int wl_bpp_limit;
	int wl_epp_cur;
	int wl_epp_limit;
};

#define POWER_SUPPLY_OCV_TEMP_MAX 20

/*
 * This is the recommended struct to manage static battery parameters,
 * populated by power_supply_get_battery_info(). Most platform drivers should
 * use these for consistency.
 * Its field names must correspond to elements in enum power_supply_property.
 * The default field value is -EINVAL.
 * Power supply class itself doesn't use this.
 */

struct power_supply_battery_info {
	int energy_full_design_uwh;	    /* microWatt-hours */
	int charge_full_design_uah;	    /* microAmp-hours */
	int voltage_min_design_uv;	    /* microVolts */
	int precharge_current_ua;	    /* microAmps */
	int charge_term_current_ua;	    /* microAmps */
	int constant_charge_current_max_ua; /* microAmps */
	int constant_charge_voltage_max_uv; /* microVolts */
	int factory_internal_resistance_uohm;   /* microOhms */
	int ocv_temp[POWER_SUPPLY_OCV_TEMP_MAX];/* celsius */
	struct power_supply_battery_ocv_table *ocv_table[POWER_SUPPLY_OCV_TEMP_MAX];
	int ocv_table_size[POWER_SUPPLY_OCV_TEMP_MAX];
	struct power_supply_vol_temp_table *temp_table;
	struct power_supply_capacity_temp_table *cap_table;
	struct power_supply_resistance_temp_table *resistance_table;
	int temp_table_size;
	int cap_table_size;
	int resistance_table_size;
	struct power_supply_charge_current cur;
	int fchg_ocv_threshold;
};

extern struct atomic_notifier_head power_supply_notifier;
extern int power_supply_reg_notifier(struct notifier_block *nb);
extern void power_supply_unreg_notifier(struct notifier_block *nb);
extern struct power_supply *power_supply_get_by_name(const char *name);
extern void power_supply_put(struct power_supply *psy);
#ifdef CONFIG_OF
extern struct power_supply *power_supply_get_by_phandle(struct device_node *np,
							const char *property);
extern struct power_supply *devm_power_supply_get_by_phandle(
				    struct device *dev, const char *property);
#else /* !CONFIG_OF */
static inline struct power_supply *
power_supply_get_by_phandle(struct device_node *np, const char *property)
{ return NULL; }
static inline struct power_supply *
devm_power_supply_get_by_phandle(struct device *dev, const char *property)
{ return NULL; }
#endif /* CONFIG_OF */

extern int power_supply_get_battery_info(struct power_supply *psy,
					 struct power_supply_battery_info *info, int num);
extern void power_supply_put_battery_info(struct power_supply *psy,
					  struct power_supply_battery_info *info);
extern int power_supply_ocv2cap_simple(struct power_supply_battery_ocv_table *table,
				       int table_len, int ocv);
extern struct power_supply_battery_ocv_table *
power_supply_find_ocv2cap_table(struct power_supply_battery_info *info,
				int temp, int *table_len);
extern int power_supply_batinfo_ocv2cap(struct power_supply_battery_info *info,
					int ocv, int temp);
extern void power_supply_changed(struct power_supply *psy);
extern int power_supply_am_i_supplied(struct power_supply *psy);
extern int power_supply_set_input_current_limit_from_supplier(
					 struct power_supply *psy);
extern int power_supply_set_battery_charged(struct power_supply *psy);

#ifdef CONFIG_POWER_SUPPLY
extern int power_supply_is_system_supplied(void);
#else
static inline int power_supply_is_system_supplied(void) { return -ENOSYS; }
#endif

extern int power_supply_get_property(struct power_supply *psy,
			    enum power_supply_property psp,
			    union power_supply_propval *val);
extern int power_supply_set_property(struct power_supply *psy,
			    enum power_supply_property psp,
			    const union power_supply_propval *val);
extern int power_supply_property_is_writeable(struct power_supply *psy,
					enum power_supply_property psp);
extern void power_supply_external_power_changed(struct power_supply *psy);

extern struct power_supply *__must_check
power_supply_register(struct device *parent,
				 const struct power_supply_desc *desc,
				 const struct power_supply_config *cfg);
extern struct power_supply *__must_check
power_supply_register_no_ws(struct device *parent,
				 const struct power_supply_desc *desc,
				 const struct power_supply_config *cfg);
extern struct power_supply *__must_check
devm_power_supply_register(struct device *parent,
				 const struct power_supply_desc *desc,
				 const struct power_supply_config *cfg);
extern struct power_supply *__must_check
devm_power_supply_register_no_ws(struct device *parent,
				 const struct power_supply_desc *desc,
				 const struct power_supply_config *cfg);
extern void power_supply_unregister(struct power_supply *psy);
extern int power_supply_powers(struct power_supply *psy, struct device *dev);

extern void *power_supply_get_drvdata(struct power_supply *psy);
/* For APM emulation, think legacy userspace. */
extern struct class *power_supply_class;

static inline bool power_supply_is_amp_property(enum power_supply_property psp)
{
	switch (psp) {
	case POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN:
	case POWER_SUPPLY_PROP_CHARGE_EMPTY_DESIGN:
	case POWER_SUPPLY_PROP_CHARGE_FULL:
	case POWER_SUPPLY_PROP_CHARGE_EMPTY:
	case POWER_SUPPLY_PROP_CHARGE_NOW:
	case POWER_SUPPLY_PROP_CHARGE_AVG:
	case POWER_SUPPLY_PROP_CHARGE_COUNTER:
	case POWER_SUPPLY_PROP_PRECHARGE_CURRENT:
	case POWER_SUPPLY_PROP_CHARGE_TERM_CURRENT:
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX:
	case POWER_SUPPLY_PROP_CURRENT_MAX:
	case POWER_SUPPLY_PROP_CURRENT_NOW:
	case POWER_SUPPLY_PROP_CURRENT_AVG:
	case POWER_SUPPLY_PROP_CURRENT_BOOT:
		return 1;
	default:
		break;
	}

	return 0;
}

static inline bool power_supply_is_watt_property(enum power_supply_property psp)
{
	switch (psp) {
	case POWER_SUPPLY_PROP_ENERGY_FULL_DESIGN:
	case POWER_SUPPLY_PROP_ENERGY_EMPTY_DESIGN:
	case POWER_SUPPLY_PROP_ENERGY_FULL:
	case POWER_SUPPLY_PROP_ENERGY_EMPTY:
	case POWER_SUPPLY_PROP_ENERGY_NOW:
	case POWER_SUPPLY_PROP_ENERGY_AVG:
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
	case POWER_SUPPLY_PROP_VOLTAGE_MIN:
	case POWER_SUPPLY_PROP_VOLTAGE_MAX_DESIGN:
	case POWER_SUPPLY_PROP_VOLTAGE_MIN_DESIGN:
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
	case POWER_SUPPLY_PROP_VOLTAGE_AVG:
	case POWER_SUPPLY_PROP_VOLTAGE_OCV:
	case POWER_SUPPLY_PROP_VOLTAGE_BOOT:
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE:
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE_MAX:
	case POWER_SUPPLY_PROP_POWER_NOW:
		return 1;
	default:
		break;
	}

	return 0;
}

#endif /* __LINUX_POWER_SUPPLY_H__ */
