/*
 * sec_battery_sysfs_qc.h
 * Samsung Mobile Battery Header
 *
 *
 * Copyright (C) 2018 Samsung Electronics, Inc.
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

#ifndef __SEC_BATTERY_SYSFS_H
#define __SEC_BATTERY_SYSFS_H __FILE__

ssize_t sec_bat_show_attrs(struct device *dev,
				struct device_attribute *attr, char *buf);

ssize_t sec_bat_store_attrs(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count);

int sec_bat_create_attrs(struct device *dev);

#define SEC_BATTERY_ATTR(_name)						\
{									\
	.attr = {.name = #_name, .mode = 0664},	\
	.show = sec_bat_show_attrs,					\
	.store = sec_bat_store_attrs,					\
}

enum {
	BATT_SLATE_MODE = 0,
	HV_CHARGER_STATUS,
	CHECK_PS_READY,
	FG_ASOC,
	FG_FULL_VOLTAGE,
	BATT_MISC_EVENT,
	STORE_MODE,
	BATT_EVENT_LCD,
	SIOP_LEVEL,
	BATT_CHARGING_SOURCE,
	BATT_RESET_SOC,
	SAFETY_TIMER_SET,
	SAFETY_TIMER_INFO,
	BATT_SWELLING_CONTROL,
	BATT_CURRENT_EVENT,
	CHECK_SLAVE_CHG,
	SLAVE_CHG_TEMP,
	SLAVE_CHG_TEMP_ADC,
#if defined(CONFIG_DIRECT_CHARGING)
	DCHG_TEMP,
	DCHG_TEMP_ADC,
	DCHG_ADC_MODE_CTRL,
#endif
	BATT_CURRENT_UA_NOW,
	BATT_CURRENT_UA_AVG,
	BATT_VFOCV,
	BATT_TEMP,
	BATT_TEMP_ADC,
	BATT_CHG_TEMP,
	BATT_CHG_TEMP_ADC,
#if defined(CONFIG_ENG_BATTERY_CONCEPT)
	BATT_TEMP_TEST,
	STEP_CHG_TEST,
	PDP_LIMIT_W_DEFAULT,
	PDP_LIMIT_W_FINAL,
	PDP_LIMIT_W_INTERVAL,
#endif
	BATT_WDT_CONTROL,
	BATTERY_CYCLE,
	VBUS_VALUE,
	MODE,
	BATT_INBAT_VOLTAGE,
	BATT_INBAT_VOLTAGE_OCV,
	DIRECT_CHARGING_STATUS,
#if defined(CONFIG_DIRECT_CHARGING)
	DIRECT_CHARGING_STEP,
	DIRECT_CHARGING_IIN,
	DIRECT_CHARGING_VOL,
#endif
#if defined(CONFIG_BATTERY_CISD)
	CISD_FULLCAPREP_MAX,
	CISD_DATA,
	CISD_DATA_JSON,
	CISD_DATA_D_JSON,
	CISD_WIRE_COUNT,
	CISD_WC_DATA,
	CISD_WC_DATA_JSON,
	CISD_EVENT_DATA,
	CISD_EVENT_DATA_JSON,
	PREV_BATTERY_DATA,
	PREV_BATTERY_INFO,
	BATT_TYPE,
#endif
	BATT_CHIP_ID,
	BATT_PON_OCV,
	BATT_USER_PON_OCV,
	PMIC_DUMP,
	PMIC_DUMP_2,
	FACTORY_CHARGING_TEST,
	CHARGING_TYPE,
};

#endif /* __SEC_BATTERY_SYSFS_H */
