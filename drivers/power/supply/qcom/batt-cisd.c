/*
 *  batt_cisd.c
 *  Battery Driver
 *
 *  Copyright (C) 2019 Samsung Electronics
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/alarmtimer.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/power_supply.h>
#include <uapi/linux/qg.h>
#include "qg-sdam.h"
#include "smb5-lib.h"
#include "batt-cisd.h"

ssize_t cisd_show_attrs(struct device *dev,
				struct device_attribute *attr, char *buf);

ssize_t cisd_store_attrs(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count);

#define CISD_ATTR(_name)						\
{									\
	.attr = {.name = #_name, .mode = 0664},	\
	.show = cisd_show_attrs,					\
	.store = cisd_store_attrs,					\
}

enum {
	CISD_DATA,
	CISD_DATA_JSON,
	CISD_DATA_D_JSON,
};

const char *cisd_data_str[] = {
	"RESET_ALG", "ALG_INDEX", "FULL_CNT", "CAP_MAX", "CAP_MIN", "RECHARGING_CNT", "VALERT_CNT",
	"BATT_CYCLE", "WIRE_CNT", "WIRELESS_CNT", "HIGH_SWELLING_CNT", "LOW_SWELLING_CNT",
	"WC_HIGH_SWELLING_CNT", "SWELLING_FULL_CNT", "SWELLING_RECOVERY_CNT", "AICL_CNT", "BATT_THM_MAX",
	"BATT_THM_MIN", "CHG_THM_MAX", "CHG_THM_MIN", "WPC_THM_MAX", "WPC_THM_MIN", "USB_THM_MAX", "USB_THM_MIN",
	"CHG_BATT_THM_MAX", "CHG_BATT_THM_MIN", "CHG_CHG_THM_MAX", "CHG_CHG_THM_MIN", "CHG_WPC_THM_MAX",
	"CHG_WPC_THM_MIN", "CHG_USB_THM_MAX", "CHG_USB_THM_MIN", "USB_OVERHEAT_CHARGING", "UNSAFETY_VOLT",
	"UNSAFETY_TEMP", "SAFETY_TIMER", "VSYS_OVP", "VBAT_OVP", "USB_OVERHEAT_RAPID_CHANGE", "BUCK_OFF",
	"USB_OVERHEAT_ALONE", "DROP_SENSOR"
};
const char *cisd_data_str_d[] = {
	"FULL_CNT_D", "CAP_MAX_D", "CAP_MIN_D", "RECHARGING_CNT_D", "VALERT_CNT_D", "WIRE_CNT_D", "WIRELESS_CNT_D",
	"HIGH_SWELLING_CNT_D", "LOW_SWELLING_CNT_D", "WC_HIGH_SWELLING_CNT_D", "SWELLING_FULL_CNT_D",
	"SWELLING_RECOVERY_CNT_D", "AICL_CNT_D", "BATT_THM_MAX_D", "BATT_THM_MIN_D", "CHG_THM_MAX_D",
	"CHG_THM_MIN_D", "WPC_THM_MAX_D", "WPC_THM_MIN_D", "USB_THM_MAX_D", "USB_THM_MIN_D",
	"CHG_BATT_THM_MAX_D", "CHG_BATT_THM_MIN_D", "CHG_CHG_THM_MAX_D", "CHG_CHG_THM_MIN_D",
	"CHG_WPC_THM_MAX_D", "CHG_WPC_THM_MIN_D", "CHG_USB_THM_MAX_D", "CHG_USB_THM_MIN_D",
	"USB_OVERHEAT_CHARGING_D", "UNSAFETY_VOLT_D", "UNSAFETY_TEMP_D", "SAFETY_TIMER_D", "VSYS_OVP_D",
	"VBAT_OVP_D", "USB_OVERHEAT_RAPID_CHANGE_D", "BUCK_OFF_D", "USB_OVERHEAT_ALONE_D", "DROP_SENSOR_D"
};

const char *cisd_cable_data_str[] = {"INDEX", "TA", "AFC", "AFC_FAIL", "QC", "QC_FAIL", "PD", "PD_HIGH", "HV_WC_20"};
const char *cisd_tx_data_str[] = {"INDEX", "ON", "GEAR", "PHONE", "OTHER"};

static struct device_attribute cisd_attrs[] = {
	CISD_ATTR(cisd_data),
	CISD_ATTR(cisd_data_json),
	CISD_ATTR(cisd_data_d_json),
};

void batt_cisd_init(struct smb_charger *chip)
{
	chip->cisd.state = CISD_STATE_NONE;

	//chip->cisd.data[CISD_DATA_ALG_INDEX] = battery->pdata->cisd_alg_index;
	chip->cisd.data[CISD_DATA_FULL_COUNT] = 1;
	chip->cisd.data[CISD_DATA_BATT_TEMP_MAX] = -300;
	chip->cisd.data[CISD_DATA_CHG_TEMP_MAX] = -300;
	chip->cisd.data[CISD_DATA_WPC_TEMP_MAX] = -300;
	chip->cisd.data[CISD_DATA_USB_TEMP_MAX] = -300;
	chip->cisd.data[CISD_DATA_BATT_TEMP_MIN] = 1000;
	chip->cisd.data[CISD_DATA_CHG_TEMP_MIN] = 1000;
	chip->cisd.data[CISD_DATA_WPC_TEMP_MIN] = 1000;
	chip->cisd.data[CISD_DATA_USB_TEMP_MIN] = 1000;
	chip->cisd.data[CISD_DATA_CHG_BATT_TEMP_MAX] = -300;
	chip->cisd.data[CISD_DATA_CHG_CHG_TEMP_MAX] = -300;
	chip->cisd.data[CISD_DATA_CHG_WPC_TEMP_MAX] = -300;
	chip->cisd.data[CISD_DATA_CHG_USB_TEMP_MAX] = -300;
	chip->cisd.data[CISD_DATA_CHG_BATT_TEMP_MIN] = 1000;
	chip->cisd.data[CISD_DATA_CHG_CHG_TEMP_MIN] = 1000;
	chip->cisd.data[CISD_DATA_CHG_WPC_TEMP_MIN] = 1000;
	chip->cisd.data[CISD_DATA_CHG_USB_TEMP_MIN] = 1000;	
	chip->cisd.data[CISD_DATA_CAP_MIN] = 0xFFFF;

	chip->cisd.data[CISD_DATA_FULL_COUNT_PER_DAY] = 1;
	chip->cisd.data[CISD_DATA_BATT_TEMP_MAX_PER_DAY] = -300;
	chip->cisd.data[CISD_DATA_CHG_TEMP_MAX_PER_DAY] = -300;
	chip->cisd.data[CISD_DATA_WPC_TEMP_MAX_PER_DAY] = -300;
	chip->cisd.data[CISD_DATA_USB_TEMP_MAX_PER_DAY] = -300;
	chip->cisd.data[CISD_DATA_BATT_TEMP_MIN_PER_DAY] = 1000;
	chip->cisd.data[CISD_DATA_CHG_TEMP_MIN_PER_DAY] = 1000;
	chip->cisd.data[CISD_DATA_WPC_TEMP_MIN_PER_DAY] = 1000;
	chip->cisd.data[CISD_DATA_USB_TEMP_MIN_PER_DAY] = 1000;

	chip->cisd.data[CISD_DATA_CHG_BATT_TEMP_MAX_PER_DAY] = -300;
	chip->cisd.data[CISD_DATA_CHG_CHG_TEMP_MAX_PER_DAY] = -300;
	chip->cisd.data[CISD_DATA_CHG_WPC_TEMP_MAX_PER_DAY] = -300;
	chip->cisd.data[CISD_DATA_CHG_USB_TEMP_MAX_PER_DAY] = -300;
	chip->cisd.data[CISD_DATA_CHG_BATT_TEMP_MIN_PER_DAY] = 1000;
	chip->cisd.data[CISD_DATA_CHG_CHG_TEMP_MIN_PER_DAY] = 1000;
	chip->cisd.data[CISD_DATA_CHG_WPC_TEMP_MIN_PER_DAY] = 1000;
	chip->cisd.data[CISD_DATA_CHG_USB_TEMP_MIN_PER_DAY] = 1000;

	chip->cisd.ab_vbat_max_count = 2; /* should be 2 */
	chip->cisd.ab_vbat_check_count = 0;
	//battery->cisd.max_voltage_thr = battery->pdata->max_voltage_thr;

	/* set cisd pointer */
	//gcisd = &battery->cisd;

	/* initialize pad data */
	//mutex_init(&battery->cisd.padlock);
	//init_cisd_pad_data(&battery->cisd);
	pr_info("%s\n", __func__);
}

ssize_t cisd_show_attrs(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	struct power_supply *psy = dev_get_drvdata(dev);
	struct smb_charger *chip = power_supply_get_drvdata(psy);
	const ptrdiff_t offset = attr - cisd_attrs;
	//union power_supply_propval value = {0, };
	int i = 0;

	switch (offset) {
	case CISD_DATA:
		{
			struct cisd *pcisd = &chip->cisd;
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
			struct cisd *pcisd = &chip->cisd;
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
			struct cisd *pcisd = &chip->cisd;
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
	default:
		i = -EINVAL;
		break;
	}

	return i;
}

ssize_t cisd_store_attrs(
					struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	//struct power_supply *psy = dev_get_drvdata(dev);
	//struct qpnp_qg *chip = power_supply_get_drvdata(psy);
	const ptrdiff_t offset = attr - cisd_attrs;
	int ret = -EINVAL;

	switch (offset) {
	case CISD_DATA:
		break;
	case CISD_DATA_JSON:
		break;
	case CISD_DATA_D_JSON:
		break;
	default:
		ret = -EINVAL;
		break;
	}
	return ret;
}

int batt_create_attrs(struct device *dev)
{
	unsigned long i = 0;
	int rc = 0;

	for (i = 0; i < ARRAY_SIZE(cisd_attrs); i++) {
		rc = device_create_file(dev, &cisd_attrs[i]);
		if (rc)
			goto create_attrs_failed;
	}
	goto create_attrs_succeed;

create_attrs_failed:
	while (i--)
		device_remove_file(dev, &cisd_attrs[i]);
create_attrs_succeed:
	return rc;
}

