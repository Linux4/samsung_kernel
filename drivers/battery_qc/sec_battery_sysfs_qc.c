/*
 *  sec_battery_sysfs_qc.c
 *  Samsung Mobile Battery Driver
 *
 *  Copyright (C) 2018 Samsung Electronics
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include "include/sec_battery_qc.h"
#include "include/sec_battery_sysfs_qc.h"
#if defined(CONFIG_QPNP_SMB5)
#if defined(CONFIG_SEC_A90Q_PROJECT) || defined(CONFIG_SEC_A70S_PROJECT) || defined(CONFIG_SEC_A70Q_PROJECT)
#include "../power/supply/qcom_r1/smb5-lib.h"
#else
#include "../power/supply/qcom/smb5-lib.h"
#endif
#endif

static struct device_attribute sec_battery_attrs[] = {
	SEC_BATTERY_ATTR(batt_slate_mode),
	SEC_BATTERY_ATTR(hv_charger_status),
	SEC_BATTERY_ATTR(check_ps_ready),
	SEC_BATTERY_ATTR(fg_asoc),
	SEC_BATTERY_ATTR(fg_full_voltage),
	SEC_BATTERY_ATTR(batt_misc_event),
	SEC_BATTERY_ATTR(store_mode),
	SEC_BATTERY_ATTR(lcd),
	SEC_BATTERY_ATTR(siop_level),
	SEC_BATTERY_ATTR(batt_charging_source),
	SEC_BATTERY_ATTR(batt_reset_soc),
	SEC_BATTERY_ATTR(safety_timer_set),
	SEC_BATTERY_ATTR(safety_timer_info),
	SEC_BATTERY_ATTR(batt_swelling_control),
	SEC_BATTERY_ATTR(batt_current_event),
	SEC_BATTERY_ATTR(check_slave_chg),
	SEC_BATTERY_ATTR(slave_chg_temp),
	SEC_BATTERY_ATTR(slave_chg_temp_adc),
#if defined(CONFIG_DIRECT_CHARGING)
	SEC_BATTERY_ATTR(dchg_temp),
	SEC_BATTERY_ATTR(dchg_temp_adc),
	SEC_BATTERY_ATTR(dchg_adc_mode_ctrl),
#endif
	SEC_BATTERY_ATTR(batt_current_ua_now),
	SEC_BATTERY_ATTR(batt_current_ua_avg),
	SEC_BATTERY_ATTR(batt_vfocv),
	SEC_BATTERY_ATTR(batt_temp),
	SEC_BATTERY_ATTR(batt_temp_adc),
	SEC_BATTERY_ATTR(chg_temp),
	SEC_BATTERY_ATTR(chg_temp_adc),
#if defined(CONFIG_ENG_BATTERY_CONCEPT)
	SEC_BATTERY_ATTR(batt_temp_test),
	SEC_BATTERY_ATTR(step_chg_test),
	SEC_BATTERY_ATTR(pdp_limit_w_default),
	SEC_BATTERY_ATTR(pdp_limit_w_final),
	SEC_BATTERY_ATTR(pdp_limit_w_interval),
#endif
	SEC_BATTERY_ATTR(batt_wdt_control),
	SEC_BATTERY_ATTR(battery_cycle),
	SEC_BATTERY_ATTR(vbus_value),
	SEC_BATTERY_ATTR(mode),
	SEC_BATTERY_ATTR(batt_inbat_voltage),
	SEC_BATTERY_ATTR(batt_inbat_voltage_ocv),
	SEC_BATTERY_ATTR(direct_charging_status),
#if defined(CONFIG_DIRECT_CHARGING)
	SEC_BATTERY_ATTR(direct_charging_step),
	SEC_BATTERY_ATTR(direct_charging_iin),
	SEC_BATTERY_ATTR(direct_charging_vol),
#endif
#if defined(CONFIG_BATTERY_CISD)
	SEC_BATTERY_ATTR(cisd_fullcaprep_max),
	SEC_BATTERY_ATTR(cisd_data),
	SEC_BATTERY_ATTR(cisd_data_json),
	SEC_BATTERY_ATTR(cisd_data_d_json),
	SEC_BATTERY_ATTR(cisd_wire_count),
	SEC_BATTERY_ATTR(cisd_wc_data),
	SEC_BATTERY_ATTR(cisd_wc_data_json),
	SEC_BATTERY_ATTR(cisd_event_data),
	SEC_BATTERY_ATTR(cisd_event_data_json),
	SEC_BATTERY_ATTR(prev_battery_data),
	SEC_BATTERY_ATTR(prev_battery_info),
	SEC_BATTERY_ATTR(batt_type),
#endif
	SEC_BATTERY_ATTR(batt_chip_id),
	SEC_BATTERY_ATTR(batt_pon_ocv),
	SEC_BATTERY_ATTR(batt_user_pon_ocv),
	SEC_BATTERY_ATTR(pmic_dump),
	SEC_BATTERY_ATTR(pmic_dump_2),
	SEC_BATTERY_ATTR(factory_charging_test),
	SEC_BATTERY_ATTR(charging_type),
};

extern int get_factory_step_max(void);

static int get_pmic_dump(struct sec_battery_info *battery, u16 start, int size, char * buf)
{
	struct smb_charger *chg;
	u16 addr = start;
	int max_addr = addr+size;
	int i = 0;
	chg = power_supply_get_drvdata(battery->psy_bat);
	for (; addr < max_addr; addr++) {
		u8 reg_val;
		if ((addr&0xf) == 0)
			i += scnprintf(buf + i, PAGE_SIZE - i, "%04x:", addr);
		if (smblib_read(chg, addr, &reg_val) < 0)
			i += scnprintf(buf + i, PAGE_SIZE - i, "--");
		else
			i += scnprintf(buf + i, PAGE_SIZE - i, "%02x", reg_val);
		if ((addr&0xf) == 0xf)
			i += scnprintf(buf + i, PAGE_SIZE - i, "\n");
		else
			i += scnprintf(buf + i, PAGE_SIZE - i, ",");
	}
	dev_info(battery->dev,"%s PMIC_DUMP 0x%x~0x%x sz = %d\n", __func__, start, max_addr, i);
	return i;
}

ssize_t sec_bat_show_attrs(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	struct sec_battery_info *battery = get_sec_battery();
	const ptrdiff_t offset = attr - sec_battery_attrs;
	union power_supply_propval val = {0, };
	int rc = 0;
	int i = 0;
#if defined(CONFIG_ENG_BATTERY_CONCEPT)
#if defined(CONFIG_SEC_A90Q_PROJECT)
	struct smb_charger *chg = power_supply_get_drvdata(battery->psy_bat);
#endif
#endif

	switch (offset) {
	case BATT_SLATE_MODE:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
			is_slate_mode(battery));
		break;
	case HV_CHARGER_STATUS:
		rc = power_supply_get_property(battery->psy_usb,
				(enum power_supply_property)POWER_SUPPLY_EXT_PROP_HV_DISABLE, &val);
		if (rc < 0) {
			pr_err("%s: Fail to get POWER_SUPPLY_EXT_PROP_HV_DISABLE(%d)\n", __func__, rc);
			val.intval = rc;
		}
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
				val.intval ? 0 : sec_bat_get_hv_charger_status(battery));
		break;
	case CHECK_PS_READY:
		sec_bat_get_ps_ready(battery);
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
				battery->pdic_ps_rdy);
		break;
	case FG_ASOC:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
				sec_bat_get_fg_asoc(battery));
		break;
	case FG_FULL_VOLTAGE:
		rc = power_supply_get_property(battery->psy_usb,
			(enum power_supply_property)POWER_SUPPLY_EXT_FIXED_RECHARGE_VBAT, &val);
		if (rc < 0) {
			pr_err("%s: Fail to get POWER_SUPPLY_EXT_FIXED_RECHARGE_VBAT(%d)\n", __func__, rc);
			val.intval = rc;
		}		
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d %d\n",
			battery->float_voltage, val.intval);
		break;
	case BATT_MISC_EVENT:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
				battery->misc_event);
		break;
	case STORE_MODE:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
			battery->store_mode);
		break;
	case BATT_EVENT_LCD:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
			(battery->lcd_status) ? 1 : 0);
		break;
	case SIOP_LEVEL:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
			battery->siop_level);
		break;
	case BATT_CHARGING_SOURCE:
		rc = power_supply_get_property(battery->psy_usb,
			POWER_SUPPLY_PROP_PD_ACTIVE, &val);
		if (rc < 0) {
			pr_err("%s: Fail to get POWER_SUPPLY_PROP_PD_ACTIVE(%d)\n", __func__, rc);
			val.intval = 0;
		}
		if (val.intval == POWER_SUPPLY_PD_PPS_ACTIVE) {
			val.intval = 38; //APDO
		} else {
			rc = power_supply_get_property(battery->psy_usb,
					POWER_SUPPLY_PROP_REAL_TYPE, &val);
			if (rc < 0) {
				pr_err("%s: Fail to get usb real_type_prop (%d=>%d)\n", __func__, POWER_SUPPLY_PROP_REAL_TYPE, rc);
				val.intval = 0;
			} else { 
				if ((val.intval == POWER_SUPPLY_TYPE_AFC) || (val.intval == POWER_SUPPLY_TYPE_USB_HVDCP))
					val.intval = 6; //9V TA
				else if (val.intval == POWER_SUPPLY_TYPE_USB_DCP)
					val.intval = 3; //5V TA
				pr_info("%s: cable_type(%d)\n", __func__, val.intval);
			}
		}
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
			val.intval);
		break;
	case SAFETY_TIMER_SET:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
			       battery->safety_timer_set);
		break;
	case SAFETY_TIMER_INFO:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%ld\n",
			battery->cal_safety_time);
		break;
	case BATT_SWELLING_CONTROL:
		rc = power_supply_get_property(battery->psy_bat,
				POWER_SUPPLY_PROP_SW_JEITA_ENABLED, &val);
		if (rc < 0) {
			pr_err("%s: Fail to get batt SW JEITA ENABLED prop. rc=%d\n", __func__, rc);
			return -EINVAL;
		}
		i += scnprintf(buf + i, PAGE_SIZE - i, "%ld\n",
			!(val.intval));
		break;
	case BATT_CURRENT_EVENT:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
			battery->current_event);
		break;
	case CHECK_SLAVE_CHG:
		if (!battery->psy_slave)
			return -EINVAL;
		rc = power_supply_get_property(battery->psy_slave,
				POWER_SUPPLY_PROP_CP_STATUS1, &val);
		if (rc < 0) {
			pr_err("%s: Fail to get POWER_SUPPLY_PROP_CP_STATUS1(%d)\n", __func__, rc);
			val.intval = rc;
		} else {
			val.intval = 1;
		}
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
				val.intval);
		break;
	case SLAVE_CHG_TEMP:
		if (!battery->psy_slave)
			return -EINVAL;
		rc = power_supply_get_property(battery->psy_slave,
				POWER_SUPPLY_PROP_CP_DIE_TEMP, &val);
		if (rc < 0) {
			pr_err("%s: Fail to get POWER_SUPPLY_PROP_CP_DIE_TEMP(%d)\n", __func__, rc);
			val.intval = rc;
		}
		pr_info("%s: SLAVE_TEMP(%d)\n", __func__, val.intval);
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
				val.intval);
		break;
	case SLAVE_CHG_TEMP_ADC:
		if (!battery->psy_slave)
			return -EINVAL;
		rc = power_supply_get_property(battery->psy_slave,
				(enum power_supply_property)POWER_SUPPLY_EXT_PROP_CP_DIE_TEMP_ADC, &val);
		if (rc < 0) {
			pr_err("%s: Fail to get POWER_SUPPLY_PROP_CP_DIE_TEMP_ADC(%d)\n", __func__, rc);
			val.intval = rc;
		}
		pr_info("%s: SLAVE_TEMP_ADC(%d)\n", __func__, val.intval);
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
				val.intval);
		break;
#if defined(CONFIG_DIRECT_CHARGING)
	case DCHG_TEMP:
		if (!battery->psy_slave)
			return -EINVAL;
		rc = power_supply_get_property(battery->psy_slave,
				POWER_SUPPLY_PROP_CP_DIE_TEMP, &val);
		if (rc < 0) {
			pr_err("%s: Fail to get POWER_SUPPLY_PROP_CP_DIE_TEMP(%d)\n", __func__, rc);
			val.intval = rc;
		}
		pr_info("%s: DCHG_TEMP(%d)\n", __func__, val.intval);
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
				val.intval);
		break;
	case DCHG_TEMP_ADC:
		if (!battery->psy_slave)
			return -EINVAL;
		rc = power_supply_get_property(battery->psy_slave,
				(enum power_supply_property)POWER_SUPPLY_EXT_PROP_CP_DIE_TEMP_ADC, &val);
		if (rc < 0) {
			pr_err("%s: Fail to get POWER_SUPPLY_PROP_CP_DIE_TEMP_ADC(%d)\n", __func__, rc);
			val.intval = rc;
		}
		pr_info("%s: DCHG_TEMP_ADC(%d)\n", __func__, val.intval);
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
				val.intval);
		break;
#endif
	case BATT_CURRENT_UA_NOW:
	case BATT_CURRENT_UA_AVG:
		power_supply_get_property(battery->psy_bat,
			POWER_SUPPLY_PROP_CURRENT_NOW, &val);
		pr_info("%s: POWER_SUPPLY_PROP_CURRENT_NOW(%d)\n", __func__, val.intval);
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
			val.intval);
		break;
	case BATT_VFOCV:
		power_supply_get_property(battery->psy_bms,
			POWER_SUPPLY_PROP_VOLTAGE_OCV, &val);
		pr_info("%s: POWER_SUPPLY_PROP_VOLTAGE_OCV(%d)\n", __func__, val.intval);
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
			val.intval);
		break;
	case BATT_TEMP:
		rc = power_supply_get_property(battery->psy_bms,
				POWER_SUPPLY_PROP_TEMP, &val);
		if (rc < 0) {
			pr_err("%s: Fail to get POWER_SUPPLY_PROP_TEMP(%d)\n", __func__, rc);
			val.intval = rc;
		}
		pr_info("%s: BATT_TEMP(%d)\n", __func__, val.intval);
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
				val.intval);
		break;
	case BATT_TEMP_ADC:
		rc = power_supply_get_property(battery->psy_bms,
				(enum power_supply_property)POWER_SUPPLY_EXT_PROP_BATT_TEMP_ADC, &val);
		if (rc < 0) {
			pr_err("%s: Fail to get POWER_SUPPLY_EXT_PROP_BATT_TEMP_ADC(%d)\n", __func__, rc);
			val.intval = rc;
		}
		pr_info("%s: BATT_TEMP_ADC(%d)\n", __func__, val.intval);
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
				val.intval);
		break;
	case BATT_CHG_TEMP:
		rc = power_supply_get_property(battery->psy_bat,
				(enum power_supply_property)POWER_SUPPLY_EXT_PROP_BATT_CHG_TEMP, &val);
		if (rc < 0) {
			pr_err("%s: Fail to get POWER_SUPPLY_EXT_PROP_BATT_CHG_TEMP(%d)\n", __func__, rc);
			val.intval = rc;
		}
		pr_info("%s: BATT_CHG_TEMP(%d)\n", __func__, val.intval);
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
				val.intval);
		break;
	case BATT_CHG_TEMP_ADC:
		rc = power_supply_get_property(battery->psy_bat,
				(enum power_supply_property)POWER_SUPPLY_EXT_PROP_BATT_TEMP_ADC, &val);
		if (rc < 0) {
			pr_err("%s: Fail to get POWER_SUPPLY_EXT_PROP_BATT_TEMP_ADC(%d)\n", __func__, rc);
			val.intval = rc;
		}
		pr_info("%s: BATT_CHG_TEMP_ADC(%d)\n", __func__, val.intval);
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
				val.intval);
		break;
#if defined(CONFIG_ENG_BATTERY_CONCEPT)
	case BATT_TEMP_TEST:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d %d\n",
				battery->temp_test_batt_temp,
				battery->temp_test_chg_temp);
		break;
	case STEP_CHG_TEST:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d %d %d %d %d %d %d %d %d\n",
				battery->step_chg_test_value[0], battery->step_chg_test_value[1],
				battery->step_chg_test_value[2], battery->step_chg_test_value[3],
				battery->step_chg_test_value[4], battery->step_chg_test_value[5],
				battery->step_chg_test_value[6], battery->step_chg_test_value[7],
				battery->step_chg_test_value[8]);
		break;
	case PDP_LIMIT_W_DEFAULT:
#if defined(CONFIG_SEC_A90Q_PROJECT)
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
				chg->default_pdp_limit_w);
#endif
		break;
	case PDP_LIMIT_W_FINAL:
#if defined(CONFIG_SEC_A90Q_PROJECT)
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
				chg->final_pdp_limit_w);
#endif
		break;
	case PDP_LIMIT_W_INTERVAL:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
				battery->interval_pdp_limit_w);
		break;
#endif
	case BATT_WDT_CONTROL:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
				battery->wdt_kick_disable);
		break;
	case BATTERY_CYCLE:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
			battery->batt_cycle);
		break;
	case VBUS_VALUE:
		val.intval = get_usb_voltage_now(battery);
		pr_err("%s: VBUS_VALUE(%d)\n", __func__, val.intval);
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
			((val.intval + 500000) / 1000000));
		break;
	case MODE:
		val.strval = NULL;
		power_supply_get_property(battery->psy_bat,
			(enum power_supply_property)POWER_SUPPLY_EXT_PROP_MULTI_CHARGER_MODE, &val);
		i += scnprintf(buf + i, PAGE_SIZE - i, "%s\n",
			(val.strval) ? val.strval : "master");
		break;
	case BATT_INBAT_VOLTAGE:
	case BATT_INBAT_VOLTAGE_OCV:
		{
			int j, k, ocv, ocv_data[10];
			for (j = 0; j < 10; j++) {
				msleep(175);
				power_supply_get_property(battery->psy_bat,
					POWER_SUPPLY_PROP_VOLTAGE_NOW, &val);
				ocv_data[j] = val.intval;
			}

			for (j = 1; j < 10; j++) {
				ocv = ocv_data[j];
				k = j;
				while (k > 0 && ocv_data[k-1] > ocv) {
					ocv_data[k] = ocv_data[k-1];
					k--;
				}
				ocv_data[k] = ocv;
			}
			ocv = 0;
			for (j = 2; j < 8; j++) {
				ocv += ocv_data[j];
			}
			rc = ocv / 6000;	// mV
		}

		dev_info(battery->dev, "in-battery voltage ocv(%d)\n", rc);
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
				rc);
		break;
#if defined(CONFIG_DIRECT_CHARGING)
	case DIRECT_CHARGING_STATUS:
		rc = power_supply_get_property(battery->psy_usb,
			POWER_SUPPLY_PROP_PD_ACTIVE, &val);
		if (rc < 0) {
			pr_err("%s: Fail to get POWER_SUPPLY_PROP_PD_ACTIVE(%d)\n", __func__, rc);
			val.intval = rc;
		}
		pr_err("%s: DIRECT_CHARGING_STATUS(%d)\n", __func__, val.intval);
		battery->isApdo = (val.intval == POWER_SUPPLY_PD_PPS_ACTIVE) ? true : false;
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
			battery->isApdo);
		break;
	case DIRECT_CHARGING_STEP:
#if 1
		val.intval = get_factory_step_max();
#else
		rc = power_supply_get_property(battery->psy_bat,
				(enum power_supply_property)POWER_SUPPLY_EXT_PROP_DIRECT_CHARGING_STEP, &val);
		if (rc < 0) {
			pr_err("%s: Fail to get POWER_SUPPLY_EXT_PROP_DIRECT_CHARGING_STEP(%d)\n", __func__, rc);
			val.intval = rc;
		}
#endif
		pr_err("%s: DIRECT_CHARGING_STEP(%d)\n", __func__, val.intval);
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
				val.intval);
		break;
	case DIRECT_CHARGING_IIN:
		rc = power_supply_get_property(battery->psy_usb,
				POWER_SUPPLY_PROP_INPUT_CURRENT_NOW, &val);
		if (rc < 0) {
			pr_err("%s: Fail to get POWER_SUPPLY_PROP_INPUT_CURRENT_NOW(%d)\n", __func__, rc);
			val.intval = rc;
		}
		pr_err("%s: DIRECT_CHARGING_IIN(%d) , VBUS(%d)\n", __func__, val.intval, (get_usb_voltage_now(battery) / 1000));
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
				val.intval);
		break;
	case DIRECT_CHARGING_VOL:
		rc = power_supply_get_property(battery->psy_bat,
				(enum power_supply_property)POWER_SUPPLY_EXT_PROP_DIRECT_CHARGING_VOL, &val);
		if (rc < 0) {
			pr_err("%s: Fail to get POWER_SUPPLY_EXT_PROP_DIRECT_CHARGING_VOL(%d)\n", __func__, rc);
			val.intval = rc;
		}
		pr_err("%s: DIRECT_CHARGING_VOL(%d)\n", __func__, val.intval);
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
				val.intval);
		break;
#else
	case DIRECT_CHARGING_STATUS:
		rc = -1; /* DC not supported model returns -1 */
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n", rc);
		break;
#endif
#if defined(CONFIG_BATTERY_CISD)
	case CISD_FULLCAPREP_MAX:
		{
			union power_supply_propval fullcaprep_val;

			fullcaprep_val.intval = SEC_BATTERY_CAPACITY_FULL;
			//psy_do_property(battery->pdata->fuelgauge_name, get,
			//	POWER_SUPPLY_PROP_ENERGY_NOW, fullcaprep_val); // need to check cisd
		
			i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
					fullcaprep_val.intval);
		}
		break;
	case CISD_DATA:
		{
			struct cisd *pcisd = &battery->cisd;
			char temp_buf[1024] = {0,};
			int j = 0;
			int size = 0;
			
			snprintf(temp_buf, sizeof(temp_buf), "%d", pcisd->data[CISD_DATA_RESET_ALG]);
			size = sizeof(temp_buf) - strlen(temp_buf);
		
			for (j = CISD_DATA_RESET_ALG + 1; j < CISD_DATA_MAX_PER_DAY; j++) {
				snprintf(temp_buf+strlen(temp_buf), size, " %d", pcisd->data[j]);
				size = sizeof(temp_buf) - strlen(temp_buf);
			}
			i += scnprintf(buf + i, PAGE_SIZE - i, "%s\n", temp_buf);
		}
		break;
	case CISD_DATA_JSON:
		{
			struct cisd *pcisd = &battery->cisd;
			char temp_buf[1024] = {0,};
			int j = 0;
			int size = 0;
			
			snprintf(temp_buf, sizeof(temp_buf), "\"%s\":\"%d\"",
					cisd_data_str[CISD_DATA_RESET_ALG], pcisd->data[CISD_DATA_RESET_ALG]);
			size = sizeof(temp_buf) - strlen(temp_buf);
		
			for (j = CISD_DATA_RESET_ALG + 1; j < CISD_DATA_MAX; j++) {
				snprintf(temp_buf+strlen(temp_buf), size, ",\"%s\":\"%d\"", cisd_data_str[j], pcisd->data[j]);
				size = sizeof(temp_buf) - strlen(temp_buf);
			}
			i += scnprintf(buf + i, PAGE_SIZE - i, "%s\n", temp_buf);
		}
		break;
	case CISD_DATA_D_JSON:
		{
			struct cisd *pcisd = &battery->cisd;
			char temp_buf[1024] = {0,};
			int j = 0;
			int size = 0;
			
			snprintf(temp_buf, sizeof(temp_buf), "\"%s\":\"%d\"",
				cisd_data_str_d[CISD_DATA_FULL_COUNT_PER_DAY-CISD_DATA_MAX],
				pcisd->data[CISD_DATA_FULL_COUNT_PER_DAY]);
			size = sizeof(temp_buf) - strlen(temp_buf);
		
			for (j = CISD_DATA_FULL_COUNT_PER_DAY + 1; j < CISD_DATA_MAX_PER_DAY; j++) {
				snprintf(temp_buf+strlen(temp_buf), size, ",\"%s\":\"%d\"",
				cisd_data_str_d[j-CISD_DATA_MAX], pcisd->data[j]);
				size = sizeof(temp_buf) - strlen(temp_buf);
			}
		
			/* Clear Daily Data */
			for (j = CISD_DATA_FULL_COUNT_PER_DAY; j < CISD_DATA_MAX_PER_DAY; j++)
				pcisd->data[j] = 0;
		
			pcisd->data[CISD_DATA_FULL_COUNT_PER_DAY] = 1;
			pcisd->data[CISD_DATA_BATT_TEMP_MAX_PER_DAY] = -300;
			pcisd->data[CISD_DATA_CHG_TEMP_MAX_PER_DAY] = -300;
			pcisd->data[CISD_DATA_WPC_TEMP_MAX_PER_DAY] = -300;
			pcisd->data[CISD_DATA_USB_TEMP_MAX_PER_DAY] = -300;
			pcisd->data[CISD_DATA_BATT_TEMP_MIN_PER_DAY] = 1000;
			pcisd->data[CISD_DATA_CHG_TEMP_MIN_PER_DAY] = 1000;
			pcisd->data[CISD_DATA_WPC_TEMP_MIN_PER_DAY] = 1000;
			pcisd->data[CISD_DATA_USB_TEMP_MIN_PER_DAY] = 1000;
		
			pcisd->data[CISD_DATA_CHG_BATT_TEMP_MAX_PER_DAY] = -300;
			pcisd->data[CISD_DATA_CHG_CHG_TEMP_MAX_PER_DAY] = -300;
			pcisd->data[CISD_DATA_CHG_WPC_TEMP_MAX_PER_DAY] = -300;
			pcisd->data[CISD_DATA_CHG_USB_TEMP_MAX_PER_DAY] = -300;
			pcisd->data[CISD_DATA_CHG_BATT_TEMP_MIN_PER_DAY] = 1000;
			pcisd->data[CISD_DATA_CHG_CHG_TEMP_MIN_PER_DAY] = 1000;
			pcisd->data[CISD_DATA_CHG_WPC_TEMP_MIN_PER_DAY] = 1000;
			pcisd->data[CISD_DATA_CHG_USB_TEMP_MIN_PER_DAY] = 1000;
	
			pcisd->data[CISD_DATA_CAP_MIN_PER_DAY] = 0xFFFF;
			i += scnprintf(buf + i, PAGE_SIZE - i, "%s\n", temp_buf);
		}
		break;
	case CISD_WIRE_COUNT:
		{
			struct cisd *pcisd = &battery->cisd;
			
			i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
				pcisd->data[CISD_DATA_WIRE_COUNT]);
		}
		break;
	case CISD_WC_DATA:
		{
			struct cisd *pcisd = &battery->cisd;
			//struct pad_data *pad_data = pcisd->pad_array;
			char temp_buf[1024] = {0,};
			//int j = 0;
			
			sprintf(temp_buf+strlen(temp_buf), "%d %d",
				PAD_INDEX_VALUE, pcisd->pad_count);
		/*	while (((pad_data = pad_data->next) != NULL) &&
					(pad_data->id < MAX_PAD_ID) &&
					(j++ < pcisd->pad_count))
				sprintf(temp_buf+strlen(temp_buf), " 0x%02x:%d", pad_data->id, pad_data->count);
				*/ // need to check cisd
	
			i += scnprintf(buf + i, PAGE_SIZE - i, "%s\n", temp_buf);
		}
		break;
	case CISD_WC_DATA_JSON:
		{
			//struct cisd *pcisd = &battery->cisd;
			//struct pad_data *pad_data = pcisd->pad_array;
			char temp_buf[1024] = {0,};
			//int j = 0;
			
			sprintf(temp_buf+strlen(temp_buf), "\"%s\":\"%d\"",
					PAD_INDEX_STRING, PAD_INDEX_VALUE);
			/*
			while (((pad_data = pad_data->next) != NULL) &&
					(pad_data->id < MAX_PAD_ID) &&
					(j++ < pcisd->pad_count))
				sprintf(temp_buf+strlen(temp_buf), ",\"%s%02x\":\"%d\"",
					PAD_JSON_STRING, pad_data->id, pad_data->count);
					*/ // need to check cisd
			i += scnprintf(buf + i, PAGE_SIZE - i, "%s\n", temp_buf);
		}
		break;
	case CISD_EVENT_DATA:
		{
			struct cisd *pcisd = &battery->cisd;
			char temp_buf[1024] = {0,};
			int j = 0;
			int size = 0;
		
			snprintf(temp_buf, sizeof(temp_buf), "%d", pcisd->event_data[EVENT_DC_ERR]);
			size = sizeof(temp_buf) - strlen(temp_buf);

			for (j = EVENT_DC_ERR + 1; j < EVENT_DATA_MAX; j++) {
				snprintf(temp_buf+strlen(temp_buf), size, " %d", pcisd->event_data[j]);
				size = sizeof(temp_buf) - strlen(temp_buf);
			}
			i += scnprintf(buf + i, PAGE_SIZE - i, "%s\n", temp_buf);
		}
		break;
	case CISD_EVENT_DATA_JSON:
		{
			struct cisd *pcisd = &battery->cisd;
			char temp_buf[1024] = {0,};
			int j = 0;
			int size = 0;

			snprintf(temp_buf, sizeof(temp_buf), "\"%s\":\"%d\"",
				cisd_event_data_str[EVENT_DC_ERR], pcisd->event_data[EVENT_DC_ERR]);
			size = sizeof(temp_buf) - strlen(temp_buf);

			for (j = EVENT_DC_ERR + 1; j < EVENT_DATA_MAX; j++) {
				snprintf(temp_buf+strlen(temp_buf), size, ",\"%s\":\"%d\"",
					cisd_event_data_str[j], pcisd->event_data[j]);
				size = sizeof(temp_buf) - strlen(temp_buf);
			}

			/* Clear Daily Event Data */
			for (j = EVENT_DC_ERR; j < EVENT_DATA_MAX; j++)
				pcisd->event_data[j] = 0;

			i += scnprintf(buf + i, PAGE_SIZE - i, "%s\n", temp_buf);
		}
		break;
	case PREV_BATTERY_DATA:
		{
			if (battery->enable_update_data)
				i += scnprintf(buf + i, PAGE_SIZE - i, "%d, %d, %d, %d\n",
					battery->voltage_now, battery->temperature,
					battery->is_jig_on, !battery->charging_block);
		}
		break;
	case PREV_BATTERY_INFO:
		{
			i += scnprintf(buf + i, PAGE_SIZE - i, "%d,%d,%d,%d\n",
				battery->prev_volt, battery->prev_temp,
				battery->prev_jig_on, battery->prev_chg_on);
			pr_info("%s: Read Prev Battery Info : %d, %d, %d, %d\n", __func__,
				battery->prev_volt, battery->prev_temp,
				battery->prev_jig_on, battery->prev_chg_on);
		}
		break;
	case BATT_TYPE:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%s\n",
				battery->batt_type);
		break;
#endif
	case BATT_CHIP_ID:
		rc = power_supply_get_property(battery->psy_bat,
				(enum power_supply_property)POWER_SUPPLY_EXT_PROP_CHIP_ID, &val);
		if (rc < 0) {
			pr_err("%s: Fail to get POWER_SUPPLY_EXT_PROP_CHIP_ID(%d)\n", __func__, rc);
			val.intval = 0;
		}
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
				val.intval);
		break;
	case BATT_PON_OCV:
		dev_info(battery->dev, "pon ocv(%d)\n", battery->batt_pon_ocv);
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
				battery->batt_pon_ocv);
		break;
	case BATT_USER_PON_OCV:
		if (sec_bat_get_ship_mode()) {
			sec_bat_set_fet_control(1);
			sec_bat_set_ship_mode(0);
			msleep(1000);
			power_supply_get_property(battery->psy_bat,
					POWER_SUPPLY_PROP_VOLTAGE_NOW, &val);
			qg_set_sdam_prifile_version(0xfa);	// fg_reset in next booting
			sec_bat_set_ship_mode(1);
			sec_bat_set_fet_control(0);
		} else
			power_supply_get_property(battery->psy_bat,
					POWER_SUPPLY_PROP_VOLTAGE_NOW, &val);

		dev_info(battery->dev, "user pon ocv(%d)\n", val.intval / 1000);
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n", val.intval / 1000);
		break;
	case PMIC_DUMP: //0x1000~0x1399
		i = get_pmic_dump(battery, 0x1000, 0x400, buf);
		break;
	case PMIC_DUMP_2: //0x1400~0x1799
		i = get_pmic_dump(battery, 0x1400, 0x400, buf);
		break;
	case FACTORY_CHARGING_TEST:
		rc = power_supply_get_property(battery->psy_bat,
				POWER_SUPPLY_PROP_SS_FACTORY_MODE, &val);
		if (rc < 0) {
			pr_err("%s: Fail to get POWER_SUPPLY_EXT_PROP_CHIP_ID(%d)\n", __func__, rc);
			val.intval = 0;
		}
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n", val.intval);
		break;
	case CHARGING_TYPE:
		rc = power_supply_get_property(battery->psy_usb,
			POWER_SUPPLY_PROP_PD_ACTIVE, &val);
		if (rc < 0) {
			pr_err("%s: Fail to get POWER_SUPPLY_PROP_PD_ACTIVE(%d)\n", __func__, rc);
			val.intval = 0;
		}
		if (val.intval == POWER_SUPPLY_PD_PPS_ACTIVE) 
			val.strval = "PDIC_APDO"; //APDO
		else {
			rc = power_supply_get_property(battery->psy_usb,
					POWER_SUPPLY_PROP_REAL_TYPE, &val);
			if (rc < 0) {
				pr_err("%s: Fail to get usb real_type_prop (%d=>%d)\n", __func__, POWER_SUPPLY_PROP_REAL_TYPE, rc);
				val.intval = 0;
			}
			val.strval = sec_cable_type[val.intval];
		}
		pr_info("%s: CHARGING_TYPE = %s\n",__func__, val.strval);
		i += scnprintf(buf + i, PAGE_SIZE - i, "%s\n", val.strval);
		break;
	default:
		i = -EINVAL;
		break;
	}

	return i;
}

ssize_t sec_bat_store_attrs(
					struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	struct sec_battery_info *battery = get_sec_battery();
	const ptrdiff_t offset = attr - sec_battery_attrs;
	union power_supply_propval val = {0, };
	int ret = -EINVAL;
	int x = 0;
#if defined(CONFIG_BATTERY_CISD)
	int i = 0;
#endif
#if defined(CONFIG_ENG_BATTERY_CONCEPT)
#if defined(CONFIG_SEC_A90Q_PROJECT)
	struct smb_charger *chg = power_supply_get_drvdata(battery->psy_bat);
#endif
#endif

	switch (offset) {
	case BATT_SLATE_MODE:
		if (sscanf(buf, "%10d\n", &x) == 1) {
			if (x == is_slate_mode(battery)) {
				dev_info(battery->dev,
					 "%s : skip same slate mode : %d\n", __func__, x);
				return count;
			} else if (x == 1) {
				sec_bat_set_current_event(SEC_BAT_CURRENT_EVENT_SLATE, SEC_BAT_CURRENT_EVENT_SLATE);
				battery->slate_mode = true;
				dev_info(battery->dev,
					"%s: enable slate mode : %d\n", __func__, x);
			} else if (x == 0) {
				sec_bat_set_current_event(0, SEC_BAT_CURRENT_EVENT_SLATE);
				battery->slate_mode = false;
				dev_info(battery->dev,
					"%s: disable slate mode : %d\n", __func__, x);
			} else {
				dev_info(battery->dev,
					"%s: SLATE MODE unknown command\n", __func__);
				return -EINVAL;
			}
			sec_bat_set_slate_mode(battery);
			ret = count;
		}
		break;
	case BATT_MISC_EVENT:
		if (sscanf(buf, "%10d\n", &x) == 1) {
			pr_info("%s: PMS sevice hiccup read done : %d ", __func__, x);
			if (!battery->hiccup_status &&
				(battery->misc_event & BATT_MISC_EVENT_HICCUP_TYPE)) {
				sec_bat_set_misc_event(0, BATT_MISC_EVENT_HICCUP_TYPE);
			}
		}
		ret = count;
		break;
	case STORE_MODE:
		if (sscanf(buf, "%10d\n", &x) == 1) {
			if (x) {
				if (!battery->store_mode) {
					pr_info("%s: enable store_mode. x : %d \n", __func__, x);
					sec_start_store_mode();
				}
			}
			else {
				if (battery->store_mode) {
					pr_info("%s: disable store_mode, x : %d \n", __func__, x);
					sec_stop_store_mode();
				}
			}
			ret = count;
		}
		break;
	case BATT_EVENT_LCD:
		if (sscanf(buf, "%d\n", &x) == 1) {
			/* we need to test
			sec_bat_event_set(battery, EVENT_LCD, x);
			*/
		if (x) {
			battery->lcd_status = true;
		} else {
			battery->lcd_status = false;
		}
			ret = count;
		}
		break;
	case SIOP_LEVEL:
		if (sscanf(buf, "%10d\n", &x) == 1) {
			pr_info("%s: siop level : %d \n", __func__, x);
			if (x == battery->siop_level) {
				pr_info("%s: skip same siop level : %d \n", __func__, x);
				return count;
			} else if (x >= 0 && x <= 100) {
				battery->siop_level = x;
			}else {
				battery->siop_level = 100;
			}
			sec_bat_run_siop_work();
			ret = count;
		}
		break;
	case BATT_RESET_SOC:
		/* vph_power from 4.4 to 3.8V -> disableship mode -> AT:FUELGAIC=0(reset qg) -> AT:FUELGAIC=1(read qg) -> enable shipmode */
		if (sec_bat_get_ship_mode()) {
			sec_bat_set_fet_control(1);
			sec_bat_set_ship_mode(0);
			msleep(1000);
		}
		
		sec_batt_reset_soc();
		ret = count;
		break;
	case SAFETY_TIMER_SET:
		if (sscanf(buf, "%10d\n", &x) == 1) {
			if (x) {
				battery->safety_timer_set = true;
			} else {
				battery->safety_timer_set = false;
			}
			ret = count;
		}
		break;
	case BATT_SWELLING_CONTROL:
		if (sscanf(buf, "%10d\n", &x) == 1) {
			if (x) {
				pr_info("%s : 15TEST START!! SWELLING MODE DISABLE\n", __func__);
				val.intval = false;
			} else {
				pr_info("%s : 15TEST END!! SWELLING MODE END\n", __func__);
				val.intval = true;
			}
			power_supply_set_property(battery->psy_bat,
					POWER_SUPPLY_PROP_SW_JEITA_ENABLED, &val);
			ret = count;
		}
		break;
#if defined(CONFIG_DIRECT_CHARGING)
	case DCHG_ADC_MODE_CTRL:
		if (sscanf(buf, "%d\n", &x) == 1) {
			val.intval = x;
			power_supply_set_property(battery->psy_bat,
					(enum power_supply_property)POWER_SUPPLY_EXT_PROP_DCHG_ENABLE, &val);
			ret = count;
		}
		break;
#endif
#if defined(CONFIG_ENG_BATTERY_CONCEPT)
	case BATT_TEMP_TEST:
	{
		char tc;
		if (sscanf(buf, "%c %10d\n", &tc, &x) == 2) {
			pr_info("%s : temperature t: %c, temp: %d\n", __func__, tc, x);
			if (tc == 'b') {
				battery->temp_test_batt_temp = x;
				val.intval = x;
				power_supply_set_property(battery->psy_bms,
						(enum power_supply_property)POWER_SUPPLY_EXT_PROP_BATT_TEMP_TEST, &val);
			} else if (tc == 'c') {
				battery->temp_test_chg_temp = x;
				val.intval = x;
				power_supply_set_property(battery->psy_bat,
						(enum power_supply_property)POWER_SUPPLY_EXT_PROP_CHG_TEMP_TEST, &val);
			} else {
				pr_info("%s : Undefined test code(%c)\n", __func__, tc);
			}
			ret = count;
		}
		break;
	}
	case STEP_CHG_TEST:
	{
		int t[9];
		int i = 0;
		if (sscanf(buf, "%10d %10d %10d %10d %10d %10d %10d %10d %10d\n",
					&t[0], &t[1], &t[2], &t[3], &t[4], &t[5], &t[6], &t[7], &t[8]) == 9) {
			pr_info("%s: Update step chg Set\n", __func__);
			for (i = 0; i < 9; i++)
				battery->step_chg_test_value[i] = t[i];
			update_step_chg_data(t);
		}
		ret = count;
		break;
	}
	case PDP_LIMIT_W_DEFAULT:
#if defined(CONFIG_SEC_A90Q_PROJECT)
		if (sscanf(buf, "%d\n", &x) == 1) {
			chg->default_pdp_limit_w = x;
			ret = count;
		}
#endif
		break;
	case PDP_LIMIT_W_FINAL:
#if defined(CONFIG_SEC_A90Q_PROJECT)
		if (sscanf(buf, "%d\n", &x) == 1) {
			chg->final_pdp_limit_w = x;
			ret = count;
		}
#endif
		break;
	case PDP_LIMIT_W_INTERVAL:
		if (sscanf(buf, "%d\n", &x) == 1) {
			battery->interval_pdp_limit_w = x;
			ret = count;
		}
		break;
#endif
	case BATT_WDT_CONTROL:
		if (sscanf(buf, "%10d\n", &x) == 1) {
			pr_info("%s: Charger WDT Set : %d\n", __func__, x);
			battery->wdt_kick_disable = !!x;
		}
		ret = count;
		break;
	case BATTERY_CYCLE:
		if (sscanf(buf, "%10d\n", &x) == 1) {
			dev_info(battery->dev, "%s: BATTERY_CYCLE(%d)\n", __func__, x);
			if (x >= 0) {
				battery->batt_cycle = x;
				val.intval = x;
				power_supply_set_property(battery->psy_bms,
						(enum power_supply_property)POWER_SUPPLY_EXT_PROP_BATT_CYCLE, &val);
#if defined(CONFIG_BATTERY_CISD)
				battery->cisd.data[CISD_DATA_CYCLE] = x;
#endif
			}
			ret = count;
		}
		break;
	case MODE:
		if (sscanf(buf, "%10d\n", &x) == 1) {
			val.intval = x;
			power_supply_set_property(battery->psy_bat,
				(enum power_supply_property)POWER_SUPPLY_EXT_PROP_MULTI_CHARGER_MODE, &val);
			ret = count;
		}
		break;
#if defined(CONFIG_BATTERY_CISD)
	case CISD_FULLCAPREP_MAX:
		break;
	case CISD_DATA:
		{
			struct cisd *pcisd = &battery->cisd;
			int temp_data[CISD_DATA_MAX_PER_DAY] = {0,};
			
			sscanf(buf, "%10d\n", &temp_data[0]);

			if (temp_data[CISD_DATA_RESET_ALG] > 0) {
				if (sscanf(buf, "%10d, %10d, %10d, %10d, %10d, %10d, %10d, %10d, %10d, %10d, %10d, %10d, %10d, %10d, %10d, %10d, %10d, %10d, %10d, %10d, %10d, %10d, %10d, %10d, %10d, %10d, %10d, %10d, %10d, %10d, %10d, %10d, %10d, %10d, %10d, %10d, %10d, %10d, %10d, %10d, %10d, %10d, %10d, %10d, %10d, %10d, %10d, %10d, %10d, %10d, %10d, %10d, %10d, %10d, %10d, %10d, %10d, %10d, %10d, %10d, %10d, %10d, %10d, %10d, %10d, %10d, %10d, %10d, %10d, %10d, %10d, %10d, %10d, %10d, %10d, %10d, %10d\n",
					&temp_data[0], &temp_data[1], &temp_data[2],
					&temp_data[3], &temp_data[4], &temp_data[5],
					&temp_data[6], &temp_data[7], &temp_data[8],
					&temp_data[9], &temp_data[10], &temp_data[11],
					&temp_data[12], &temp_data[13], &temp_data[14],
					&temp_data[15], &temp_data[16], &temp_data[17],
					&temp_data[18], &temp_data[19], &temp_data[20],
					&temp_data[21], &temp_data[22], &temp_data[23],
					&temp_data[24], &temp_data[25], &temp_data[26],
					&temp_data[27], &temp_data[28], &temp_data[29],
					&temp_data[30], &temp_data[31], &temp_data[32],
					&temp_data[33], &temp_data[34], &temp_data[35],
					&temp_data[36], &temp_data[37], &temp_data[38],
					&temp_data[39], &temp_data[40], &temp_data[41],
					&temp_data[42], &temp_data[43], &temp_data[44],
					&temp_data[45], &temp_data[46], &temp_data[47],
					&temp_data[48], &temp_data[49], &temp_data[50],
					&temp_data[51], &temp_data[52], &temp_data[53],
					&temp_data[54], &temp_data[55], &temp_data[56],
					&temp_data[57], &temp_data[58], &temp_data[59],
					&temp_data[60], &temp_data[61], &temp_data[62],
					&temp_data[63], &temp_data[64], &temp_data[65],
					&temp_data[66], &temp_data[67], &temp_data[68],
					&temp_data[69], &temp_data[70], &temp_data[71],
					&temp_data[72], &temp_data[73], &temp_data[74],
					&temp_data[75], &temp_data[76]) <= CISD_DATA_MAX_PER_DAY) {
					for (i = 0; i < CISD_DATA_MAX_PER_DAY; i++)
						pcisd->data[i] = 0;

					pcisd->data[CISD_DATA_ALG_INDEX] = battery->cisd_alg_index;
					pcisd->data[CISD_DATA_FULL_COUNT] = temp_data[0];
					pcisd->data[CISD_DATA_CAP_MAX] = temp_data[1];
					pcisd->data[CISD_DATA_CAP_MIN] = temp_data[2];
					pcisd->data[CISD_DATA_VALERT_COUNT] = temp_data[16];
					pcisd->data[CISD_DATA_CYCLE] = temp_data[17];
					pcisd->data[CISD_DATA_WIRE_COUNT] = temp_data[18];
					pcisd->data[CISD_DATA_WIRELESS_COUNT] = temp_data[19];
					pcisd->data[CISD_DATA_HIGH_TEMP_SWELLING] = temp_data[20];
					pcisd->data[CISD_DATA_LOW_TEMP_SWELLING] = temp_data[21];
					pcisd->data[CISD_DATA_SWELLING_CHARGING_COUNT] = temp_data[22];
					pcisd->data[CISD_DATA_AICL_COUNT] = temp_data[26];
					pcisd->data[CISD_DATA_BATT_TEMP_MAX] = temp_data[27];
					pcisd->data[CISD_DATA_BATT_TEMP_MIN] = temp_data[28];
					pcisd->data[CISD_DATA_CHG_TEMP_MAX] = temp_data[29];
					pcisd->data[CISD_DATA_CHG_TEMP_MIN] = temp_data[30];
					pcisd->data[CISD_DATA_WPC_TEMP_MAX] = temp_data[31];
					pcisd->data[CISD_DATA_WPC_TEMP_MIN] = temp_data[32];
					pcisd->data[CISD_DATA_UNSAFETY_VOLTAGE] = temp_data[33];
					pcisd->data[CISD_DATA_UNSAFETY_TEMPERATURE] = temp_data[34];
					pcisd->data[CISD_DATA_SAFETY_TIMER] = temp_data[35];
					pcisd->data[CISD_DATA_VSYS_OVP] = temp_data[36];
					pcisd->data[CISD_DATA_VBAT_OVP] = temp_data[37];
				}
			} else {
				const char *p = buf;

				pr_info("%s: %s\n", __func__, buf);
				for (i = CISD_DATA_RESET_ALG; i < CISD_DATA_MAX_PER_DAY; i++) {
					if (sscanf(p, "%10d%n", &pcisd->data[i], &x) > 0)
						p += (size_t)x;
					else {
						pr_info("%s: NO DATA (cisd_data)\n", __func__);
						temp_data[CISD_DATA_RESET_ALG] = -1;
						break;
					}
				}

				pr_info("%s: %s cisd data\n", __func__,
					((temp_data[CISD_DATA_RESET_ALG] < 0 || battery->fg_reset) ? "init" : "update"));

				if (temp_data[CISD_DATA_RESET_ALG] < 0 || battery->fg_reset) {
					/* initialize data */
					for (i = CISD_DATA_RESET_ALG; i < CISD_DATA_MAX_PER_DAY; i++)
						pcisd->data[i] = 0;

					battery->fg_reset = 0;

					pcisd->data[CISD_DATA_ALG_INDEX] = battery->cisd_alg_index;

					pcisd->data[CISD_DATA_FULL_COUNT] = 1;
					pcisd->data[CISD_DATA_BATT_TEMP_MAX] = -300;
					pcisd->data[CISD_DATA_CHG_TEMP_MAX] = -300;
					pcisd->data[CISD_DATA_WPC_TEMP_MAX] = -300;
					pcisd->data[CISD_DATA_USB_TEMP_MAX] = -300;
					pcisd->data[CISD_DATA_BATT_TEMP_MIN] = 1000;
					pcisd->data[CISD_DATA_CHG_TEMP_MIN] = 1000;
					pcisd->data[CISD_DATA_WPC_TEMP_MIN] = 1000;
					pcisd->data[CISD_DATA_USB_TEMP_MIN] = 1000;
					pcisd->data[CISD_DATA_CAP_MIN] = 0xFFFF;

					pcisd->data[CISD_DATA_FULL_COUNT_PER_DAY] = 1;
					pcisd->data[CISD_DATA_BATT_TEMP_MAX_PER_DAY] = -300;
					pcisd->data[CISD_DATA_CHG_TEMP_MAX_PER_DAY] = -300;
					pcisd->data[CISD_DATA_WPC_TEMP_MAX_PER_DAY] = -300;
					pcisd->data[CISD_DATA_USB_TEMP_MAX_PER_DAY] = -300;
					pcisd->data[CISD_DATA_BATT_TEMP_MIN_PER_DAY] = 1000;
					pcisd->data[CISD_DATA_CHG_TEMP_MIN_PER_DAY] = 1000;
					pcisd->data[CISD_DATA_WPC_TEMP_MIN_PER_DAY] = 1000;
					pcisd->data[CISD_DATA_USB_TEMP_MIN_PER_DAY] = 1000;

					pcisd->data[CISD_DATA_CHG_BATT_TEMP_MAX] = -300;
					pcisd->data[CISD_DATA_CHG_CHG_TEMP_MAX] = -300;
					pcisd->data[CISD_DATA_CHG_WPC_TEMP_MAX] = -300;
					pcisd->data[CISD_DATA_CHG_USB_TEMP_MAX] = -300;
					pcisd->data[CISD_DATA_CHG_BATT_TEMP_MIN] = 1000;
					pcisd->data[CISD_DATA_CHG_CHG_TEMP_MIN] = 1000;
					pcisd->data[CISD_DATA_CHG_WPC_TEMP_MIN] = 1000;
					pcisd->data[CISD_DATA_CHG_USB_TEMP_MIN] = 1000;

					pcisd->data[CISD_DATA_CHG_BATT_TEMP_MAX_PER_DAY] = -300;
					pcisd->data[CISD_DATA_CHG_CHG_TEMP_MAX_PER_DAY] = -300;
					pcisd->data[CISD_DATA_CHG_WPC_TEMP_MAX_PER_DAY] = -300;
					pcisd->data[CISD_DATA_CHG_USB_TEMP_MAX_PER_DAY] = -300;
					pcisd->data[CISD_DATA_CHG_BATT_TEMP_MIN_PER_DAY] = 1000;
					pcisd->data[CISD_DATA_CHG_CHG_TEMP_MIN_PER_DAY] = 1000;
					pcisd->data[CISD_DATA_CHG_WPC_TEMP_MIN_PER_DAY] = 1000;
					pcisd->data[CISD_DATA_CHG_USB_TEMP_MIN_PER_DAY] = 1000;

					pcisd->data[CISD_DATA_CAP_MIN_PER_DAY] = 0xFFFF;

					/* initialize pad data */
					init_cisd_pad_data(&battery->cisd);
				}
			}
			ret = count;
			//wake_lock(&battery->monitor_wake_lock); // need to check cisd
			//queue_delayed_work(battery->monitor_wqueue, &battery->monitor_work, 0);
		}
		break;
	case CISD_DATA_JSON:
		{
			char tc;
			struct cisd *pcisd = &battery->cisd;
			
			if (sscanf(buf, "%1c\n", &tc) == 1) {
				if (tc == 'c') {
					for (i = 0; i < CISD_DATA_MAX; i++)
						pcisd->data[i] = 0;

					pcisd->data[CISD_DATA_FULL_COUNT] = 1;
					pcisd->data[CISD_DATA_BATT_TEMP_MAX] = -300;
					pcisd->data[CISD_DATA_CHG_TEMP_MAX] = -300;
					pcisd->data[CISD_DATA_WPC_TEMP_MAX] = -300;
					pcisd->data[CISD_DATA_USB_TEMP_MAX] = -300;
					pcisd->data[CISD_DATA_BATT_TEMP_MIN] = 1000;
					pcisd->data[CISD_DATA_CHG_TEMP_MIN] = 1000;
					pcisd->data[CISD_DATA_WPC_TEMP_MIN] = 1000;
					pcisd->data[CISD_DATA_USB_TEMP_MIN] = 1000;
					pcisd->data[CISD_DATA_CAP_MIN] = 0xFFFF;

					pcisd->data[CISD_DATA_FULL_COUNT_PER_DAY] = 1;
					pcisd->data[CISD_DATA_BATT_TEMP_MAX_PER_DAY] = -300;
					pcisd->data[CISD_DATA_CHG_TEMP_MAX_PER_DAY] = -300;
					pcisd->data[CISD_DATA_WPC_TEMP_MAX_PER_DAY] = -300;
					pcisd->data[CISD_DATA_USB_TEMP_MAX_PER_DAY] = -300;
					pcisd->data[CISD_DATA_BATT_TEMP_MIN_PER_DAY] = 1000;
					pcisd->data[CISD_DATA_CHG_TEMP_MIN_PER_DAY] = 1000;
					pcisd->data[CISD_DATA_WPC_TEMP_MIN_PER_DAY] = 1000;
					pcisd->data[CISD_DATA_USB_TEMP_MIN_PER_DAY] = 1000;

					pcisd->data[CISD_DATA_CHG_BATT_TEMP_MAX] = -300;
					pcisd->data[CISD_DATA_CHG_CHG_TEMP_MAX] = -300;
					pcisd->data[CISD_DATA_CHG_WPC_TEMP_MAX] = -300;
					pcisd->data[CISD_DATA_CHG_USB_TEMP_MAX] = -300;
					pcisd->data[CISD_DATA_CHG_BATT_TEMP_MIN] = 1000;
					pcisd->data[CISD_DATA_CHG_CHG_TEMP_MIN] = 1000;
					pcisd->data[CISD_DATA_CHG_WPC_TEMP_MIN] = 1000;
					pcisd->data[CISD_DATA_CHG_USB_TEMP_MIN] = 1000;

					pcisd->data[CISD_DATA_CHG_BATT_TEMP_MAX_PER_DAY] = -300;
					pcisd->data[CISD_DATA_CHG_CHG_TEMP_MAX_PER_DAY] = -300;
					pcisd->data[CISD_DATA_CHG_WPC_TEMP_MAX_PER_DAY] = -300;
					pcisd->data[CISD_DATA_CHG_USB_TEMP_MAX_PER_DAY] = -300;
					pcisd->data[CISD_DATA_CHG_BATT_TEMP_MIN_PER_DAY] = 1000;
					pcisd->data[CISD_DATA_CHG_CHG_TEMP_MIN_PER_DAY] = 1000;
					pcisd->data[CISD_DATA_CHG_WPC_TEMP_MIN_PER_DAY] = 1000;
					pcisd->data[CISD_DATA_CHG_USB_TEMP_MIN_PER_DAY] = 1000;

					pcisd->data[CISD_DATA_CAP_MIN_PER_DAY] = 0xFFFF;

				}
			}
			ret = count;
		}
		break;
	case CISD_DATA_D_JSON:
		break;
	case CISD_WIRE_COUNT:
		if (sscanf(buf, "%10d\n", &x) == 1) {
			struct cisd *pcisd = &battery->cisd;
			pr_info("%s: Wire Count : %d\n", __func__, x);
			pcisd->data[CISD_DATA_WIRE_COUNT] = x;
			pcisd->data[CISD_DATA_WIRE_COUNT_PER_DAY]++;
		}
		ret = count;
		break;
	case CISD_WC_DATA:
		set_cisd_pad_data(battery, buf);
		ret = count;
		break;
	case CISD_WC_DATA_JSON:
		break;
	case CISD_EVENT_DATA:
		{
			struct cisd *pcisd = &battery->cisd;
			const char *p = buf;

			pr_info("%s: %s\n", __func__, buf);
			for (i = EVENT_DC_ERR; i < EVENT_DATA_MAX; i++) {
				if (sscanf(p, "%10d%n", &pcisd->event_data[i], &x) > 0) {
					p += (size_t)x;
				} else {
					pr_info("%s: NO DATA (CISD_EVENT_DATA)\n", __func__);
					pcisd->event_data[i] = 0;
					break;
				}
			}
		}
		ret = count;
		break;
	case CISD_EVENT_DATA_JSON:
		break;
	case PREV_BATTERY_DATA:
		if (sscanf(buf, "%10d, %10d, %10d, %10d\n",
				&battery->prev_volt, &battery->prev_temp,
				&battery->prev_jig_on, &battery->prev_chg_on) >= 4) {
			pr_info("%s: prev voltage : %d, prev_temp : %d, prev_jig_on : %d, prev_chg_on : %d\n",
				__func__, battery->prev_volt, battery->prev_temp,
				battery->prev_jig_on, battery->prev_chg_on);

			if (battery->prev_volt >= 3700 && battery->prev_temp >= 150 &&
					!battery->prev_jig_on && battery->fg_reset)
				pr_info("%s: Battery have been Removed\n", __func__);

			ret = count;
		}
		battery->enable_update_data = 1;
		break;
	case PREV_BATTERY_INFO:
		break;
	case BATT_TYPE:
		strncpy(battery->batt_type, buf, sizeof(battery->batt_type) - 1);
		battery->batt_type[sizeof(battery->batt_type)-1] = '\0';
		ret = count;
		break;
#endif
	case FACTORY_CHARGING_TEST:
		pr_err("%s: FACTORY_CHARGING_TEST\n", __func__);
		if (sscanf(buf, "%10d\n", &x) == 1) {
			pr_err("%s: FACTORY_CHARGING_TEST(%d)\n", __func__, x);
			val.intval = x;
			power_supply_set_property(battery->psy_bat,
					POWER_SUPPLY_PROP_SS_FACTORY_MODE, &val);
			power_supply_set_property(battery->psy_bms,
					POWER_SUPPLY_PROP_SS_FACTORY_MODE, &val);
		}
		ret = count;
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

int sec_bat_create_attrs(struct device *dev)
{
	unsigned long i = 0;
	int rc = 0;

	for (i = 0; i < ARRAY_SIZE(sec_battery_attrs); i++) {
		rc = device_create_file(dev, &sec_battery_attrs[i]);
		if (rc)
			goto create_attrs_failed;
	}
	goto create_attrs_succeed;

create_attrs_failed:
	while (i--)
		device_remove_file(dev, &sec_battery_attrs[i]);
create_attrs_succeed:
	return rc;
}
