/*
 *  sec_cisd.c
 *  Samsung Mobile Battery Driver
 *
 *  Copyright (C) 2012 Samsung Electronics
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include "sec_battery.h"
#include "sec_battery_sysfs.h"
#include "sec_cisd.h"

#if IS_ENABLED(CONFIG_SEC_ABC)
#include <linux/sti/abc_common.h>
#endif

const char *cisd_data_str[] = {
	"RESET_ALG", "ALG_INDEX", "FULL_CNT", "CAP_MAX", "CAP_MIN", "RECHARGING_CNT", "VALERT_CNT",
	"BATT_CYCLE", "WIRE_CNT", "WIRELESS_CNT", "HIGH_SWELLING_CNT", "LOW_SWELLING_CNT",
	"WC_HIGH_SWELLING_CNT", "SWELLING_FULL_CNT", "SWELLING_RECOVERY_CNT", "AICL_CNT", "BATT_THM_MAX",
	"BATT_THM_MIN", "CHG_THM_MAX", "CHG_THM_MIN", "WPC_THM_MAX", "WPC_THM_MIN", "USB_THM_MAX", "USB_THM_MIN",
	"CHG_BATT_THM_MAX", "CHG_BATT_THM_MIN", "CHG_CHG_THM_MAX", "CHG_CHG_THM_MIN", "CHG_WPC_THM_MAX",
	"CHG_WPC_THM_MIN", "CHG_USB_THM_MAX", "CHG_USB_THM_MIN", "USB_OVERHEAT_CHARGING", "UNSAFETY_VOLT",
	"UNSAFETY_TEMP", "SAFETY_TIMER", "VSYS_OVP", "VBAT_OVP", "USB_OVERHEAT_RAPID_CHANGE", "ASOC",
	"USB_OVERHEAT_ALONE", "CAP_NOM"
};
const char *cisd_data_str_d[] = {
	"FULL_CNT_D", "CAP_MAX_D", "CAP_MIN_D", "RECHARGING_CNT_D", "VALERT_CNT_D", "WIRE_CNT_D", "WIRELESS_CNT_D",
	"HIGH_SWELLING_CNT_D", "LOW_SWELLING_CNT_D", "WC_HIGH_SWELLING_CNT_D", "SWELLING_FULL_CNT_D",
	"SWELLING_RECOVERY_CNT_D", "AICL_CNT_D", "BATT_THM_MAX_D", "BATT_THM_MIN_D", "SUB_BATT_THM_MAX_D",
	"SUB_BATT_THM_MIN_D", "CHG_THM_MAX_D", "CHG_THM_MIN_D", "USB_THM_MAX_D", "USB_THM_MIN_D",
	"CHG_BATT_THM_MAX_D", "CHG_BATT_THM_MIN_D", "CHG_SUB_BATT_THM_MAX_D", "CHG_SUB_BATT_THM_MIN_D",
	"CHG_CHG_THM_MAX_D", "CHG_CHG_THM_MIN_D", "CHG_USB_THM_MAX_D", "CHG_USB_THM_MIN_D",
	"USB_OVERHEAT_CHARGING_D", "UNSAFETY_VOLT_D", "UNSAFETY_TEMP_D",
	"SAFETY_TIMER_D", "VSYS_OVP_D", "VBAT_OVP_D", "USB_OVERHEAT_RAPID_CHANGE_D", "BUCK_OFF_D",
	"USB_OVERHEAT_ALONE_D", "DROP_SENSOR_D"
};

const char *cisd_cable_data_str[] = {"TA", "AFC", "AFC_FAIL", "QC", "QC_FAIL", "PD", "PD_HIGH", "HV_WC_20"};
const char *cisd_tx_data_str[] = {"ON", "OTHER", "GEAR", "PHONE", "BUDS"};
const char *cisd_event_data_str[] = {"DC_ERR", "TA_OCP_DET", "TA_OCP_ON", "OVP_EVENT_POWER", "OVP_EVENT_SIGNAL"};

bool sec_bat_cisd_check(struct sec_battery_info *battery)
{
	union power_supply_propval capcurr_val = {0, };
	union power_supply_propval vbat_val = {0, };
	struct cisd *pcisd = &battery->cisd;
	bool ret = false;

	if (battery->factory_mode || battery->is_jig_on || battery->skip_cisd) {
		dev_info(battery->dev, "%s: No need to check in factory mode\n",
			__func__);
		return ret;
	}

	if ((battery->status == POWER_SUPPLY_STATUS_CHARGING) ||
		(battery->status == POWER_SUPPLY_STATUS_FULL)) {

		/* check abnormal vbat */
		pcisd->ab_vbat_check_count = battery->voltage_now > pcisd->max_voltage_thr ?
				pcisd->ab_vbat_check_count + 1 : 0;

		if ((pcisd->ab_vbat_check_count >= pcisd->ab_vbat_max_count) &&
			!(pcisd->state & CISD_STATE_OVER_VOLTAGE)) {
			dev_info(battery->dev, "%s : [CISD] Battery Over Voltage Protction !! vbat(%d)mV\n",
				__func__, battery->voltage_now);
			vbat_val.intval = true;
			psy_do_property("battery", set, POWER_SUPPLY_EXT_PROP_VBAT_OVP,
					vbat_val);
			pcisd->data[CISD_DATA_VBAT_OVP]++;
			pcisd->data[CISD_DATA_VBAT_OVP_PER_DAY]++;
			pcisd->state |= CISD_STATE_OVER_VOLTAGE;
#if IS_ENABLED(CONFIG_SEC_ABC)
#if IS_ENABLED(CONFIG_SEC_FACTORY)
			sec_abc_send_event("MODULE=battery@INFO=over_voltage");
#else
			sec_abc_send_event("MODULE=battery@WARN=over_voltage");
#endif
#endif
		}

		if (battery->temperature > pcisd->data[CISD_DATA_CHG_BATT_TEMP_MAX])
			pcisd->data[CISD_DATA_CHG_BATT_TEMP_MAX] = battery->temperature;
		if (battery->temperature < pcisd->data[CISD_DATA_CHG_BATT_TEMP_MIN])
			pcisd->data[CISD_DATA_CHG_BATT_TEMP_MIN] = battery->temperature;

		if (battery->chg_temp > pcisd->data[CISD_DATA_CHG_CHG_TEMP_MAX])
			pcisd->data[CISD_DATA_CHG_CHG_TEMP_MAX] = battery->chg_temp;
		if (battery->chg_temp < pcisd->data[CISD_DATA_CHG_CHG_TEMP_MIN])
			pcisd->data[CISD_DATA_CHG_CHG_TEMP_MIN] = battery->chg_temp;

		if (battery->wpc_temp > pcisd->data[CISD_DATA_CHG_WPC_TEMP_MAX])
			pcisd->data[CISD_DATA_CHG_WPC_TEMP_MAX] = battery->wpc_temp;
		if (battery->wpc_temp < pcisd->data[CISD_DATA_CHG_WPC_TEMP_MIN])
			pcisd->data[CISD_DATA_CHG_WPC_TEMP_MIN] = battery->wpc_temp;

		if (battery->usb_temp > pcisd->data[CISD_DATA_CHG_USB_TEMP_MAX])
			pcisd->data[CISD_DATA_CHG_USB_TEMP_MAX] = battery->usb_temp;
		if (battery->usb_temp < pcisd->data[CISD_DATA_CHG_USB_TEMP_MIN])
			pcisd->data[CISD_DATA_CHG_USB_TEMP_MIN] = battery->usb_temp;

		if (battery->temperature > pcisd->data[CISD_DATA_CHG_BATT_TEMP_MAX_PER_DAY])
			pcisd->data[CISD_DATA_CHG_BATT_TEMP_MAX_PER_DAY] = battery->temperature;
		if (battery->temperature < pcisd->data[CISD_DATA_CHG_BATT_TEMP_MIN_PER_DAY])
			pcisd->data[CISD_DATA_CHG_BATT_TEMP_MIN_PER_DAY] = battery->temperature;

		if (battery->sub_bat_temp > pcisd->data[CISD_DATA_CHG_SUB_BATT_TEMP_MAX_PER_DAY])
			pcisd->data[CISD_DATA_CHG_SUB_BATT_TEMP_MAX_PER_DAY] = battery->sub_bat_temp;
		if (battery->sub_bat_temp < pcisd->data[CISD_DATA_CHG_SUB_BATT_TEMP_MIN_PER_DAY])
			pcisd->data[CISD_DATA_CHG_SUB_BATT_TEMP_MIN_PER_DAY] = battery->sub_bat_temp;

		if (battery->chg_temp > pcisd->data[CISD_DATA_CHG_CHG_TEMP_MAX_PER_DAY])
			pcisd->data[CISD_DATA_CHG_CHG_TEMP_MAX_PER_DAY] = battery->chg_temp;
		if (battery->chg_temp < pcisd->data[CISD_DATA_CHG_CHG_TEMP_MIN_PER_DAY])
			pcisd->data[CISD_DATA_CHG_CHG_TEMP_MIN_PER_DAY] = battery->chg_temp;

		if (battery->usb_temp > pcisd->data[CISD_DATA_CHG_USB_TEMP_MAX_PER_DAY])
			pcisd->data[CISD_DATA_CHG_USB_TEMP_MAX_PER_DAY] = battery->usb_temp;
		if (battery->usb_temp < pcisd->data[CISD_DATA_CHG_USB_TEMP_MIN_PER_DAY])
			pcisd->data[CISD_DATA_CHG_USB_TEMP_MIN_PER_DAY] = battery->usb_temp;

		if (battery->usb_temp > 800 && !battery->usb_overheat_check) {
			battery->cisd.data[CISD_DATA_USB_OVERHEAT_CHARGING]++;
			battery->cisd.data[CISD_DATA_USB_OVERHEAT_CHARGING_PER_DAY]++;
			battery->usb_overheat_check = true;
		}
	} else  {
		/* discharging */
		if (battery->status == POWER_SUPPLY_STATUS_NOT_CHARGING) {
			/* check abnormal vbat */
			pcisd->ab_vbat_check_count = battery->voltage_now > pcisd->max_voltage_thr ?
				pcisd->ab_vbat_check_count + 1 : 0;

			if ((pcisd->ab_vbat_check_count >= pcisd->ab_vbat_max_count) &&
				!(pcisd->state & CISD_STATE_OVER_VOLTAGE)) {
				pcisd->data[CISD_DATA_VBAT_OVP]++;
				pcisd->data[CISD_DATA_VBAT_OVP_PER_DAY]++;
				pcisd->state |= CISD_STATE_OVER_VOLTAGE;
#if IS_ENABLED(CONFIG_SEC_ABC)
#if IS_ENABLED(CONFIG_SEC_FACTORY)
				sec_abc_send_event("MODULE=battery@INFO=over_voltage");
#else
				sec_abc_send_event("MODULE=battery@WARN=over_voltage");
#endif
#endif
			}
		}

		capcurr_val.intval = SEC_BATTERY_CAPACITY_FULL;
		psy_do_property(battery->pdata->fuelgauge_name, get,
			POWER_SUPPLY_PROP_ENERGY_NOW, capcurr_val);
		if (capcurr_val.intval == -1) {
			dev_info(battery->dev, "%s: [CISD] FG I2C fail. skip cisd check \n", __func__);
			return ret;
		}

		if (capcurr_val.intval > pcisd->data[CISD_DATA_CAP_MAX])
			pcisd->data[CISD_DATA_CAP_MAX] = capcurr_val.intval;
		if (capcurr_val.intval < pcisd->data[CISD_DATA_CAP_MIN])
			pcisd->data[CISD_DATA_CAP_MIN] = capcurr_val.intval;

		if (capcurr_val.intval > pcisd->data[CISD_DATA_CAP_MAX_PER_DAY])
			pcisd->data[CISD_DATA_CAP_MAX_PER_DAY] = capcurr_val.intval;
		if (capcurr_val.intval < pcisd->data[CISD_DATA_CAP_MIN_PER_DAY])
			pcisd->data[CISD_DATA_CAP_MIN_PER_DAY] = capcurr_val.intval;

		capcurr_val.intval = SEC_BATTERY_CAPACITY_AGEDCELL;
		psy_do_property(battery->pdata->fuelgauge_name, get,
			POWER_SUPPLY_PROP_ENERGY_NOW, capcurr_val);
		if (capcurr_val.intval == -1) {
			dev_info(battery->dev, "%s: [CISD] FG I2C fail. skip cisd check\n", __func__);
			return ret;
		}
		pcisd->data[CISD_DATA_CAP_NOM] = capcurr_val.intval;
		dev_info(battery->dev, "%s: [CISD] CAP_NOM %dmAh\n", __func__, pcisd->data[CISD_DATA_CAP_NOM]);
	}

	if (battery->temperature > pcisd->data[CISD_DATA_BATT_TEMP_MAX])
		pcisd->data[CISD_DATA_BATT_TEMP_MAX] = battery->temperature;
	if (battery->temperature < battery->cisd.data[CISD_DATA_BATT_TEMP_MIN])
		pcisd->data[CISD_DATA_BATT_TEMP_MIN] = battery->temperature;

	if (battery->chg_temp > pcisd->data[CISD_DATA_CHG_TEMP_MAX])
		pcisd->data[CISD_DATA_CHG_TEMP_MAX] = battery->chg_temp;
	if (battery->chg_temp < pcisd->data[CISD_DATA_CHG_TEMP_MIN])
		pcisd->data[CISD_DATA_CHG_TEMP_MIN] = battery->chg_temp;

	if (battery->wpc_temp > pcisd->data[CISD_DATA_WPC_TEMP_MAX])
		pcisd->data[CISD_DATA_WPC_TEMP_MAX] = battery->wpc_temp;
	if (battery->wpc_temp < battery->cisd.data[CISD_DATA_WPC_TEMP_MIN])
		pcisd->data[CISD_DATA_WPC_TEMP_MIN] = battery->wpc_temp;

	if (battery->usb_temp > pcisd->data[CISD_DATA_USB_TEMP_MAX])
		pcisd->data[CISD_DATA_USB_TEMP_MAX] = battery->usb_temp;
	if (battery->usb_temp < pcisd->data[CISD_DATA_USB_TEMP_MIN])
		pcisd->data[CISD_DATA_USB_TEMP_MIN] = battery->usb_temp;

	if (battery->temperature > pcisd->data[CISD_DATA_BATT_TEMP_MAX_PER_DAY])
		pcisd->data[CISD_DATA_BATT_TEMP_MAX_PER_DAY] = battery->temperature;
	if (battery->temperature < pcisd->data[CISD_DATA_BATT_TEMP_MIN_PER_DAY])
		pcisd->data[CISD_DATA_BATT_TEMP_MIN_PER_DAY] = battery->temperature;

	if (battery->sub_bat_temp > pcisd->data[CISD_DATA_SUB_BATT_TEMP_MAX_PER_DAY])
		pcisd->data[CISD_DATA_SUB_BATT_TEMP_MAX_PER_DAY] = battery->sub_bat_temp;
	if (battery->sub_bat_temp < pcisd->data[CISD_DATA_SUB_BATT_TEMP_MIN_PER_DAY])
		pcisd->data[CISD_DATA_SUB_BATT_TEMP_MIN_PER_DAY] = battery->sub_bat_temp;

	if (battery->chg_temp > pcisd->data[CISD_DATA_CHG_TEMP_MAX_PER_DAY])
		pcisd->data[CISD_DATA_CHG_TEMP_MAX_PER_DAY] = battery->chg_temp;
	if (battery->chg_temp < pcisd->data[CISD_DATA_CHG_TEMP_MIN_PER_DAY])
		pcisd->data[CISD_DATA_CHG_TEMP_MIN_PER_DAY] = battery->chg_temp;

	if (battery->usb_temp > pcisd->data[CISD_DATA_USB_TEMP_MAX_PER_DAY])
		pcisd->data[CISD_DATA_USB_TEMP_MAX_PER_DAY] = battery->usb_temp;
	if (battery->usb_temp < pcisd->data[CISD_DATA_USB_TEMP_MIN_PER_DAY])
		pcisd->data[CISD_DATA_USB_TEMP_MIN_PER_DAY] = battery->usb_temp;

	return ret;
}

static irqreturn_t cisd_irq_thread(int irq, void *data)
{
	struct cisd *pcisd = data;

	pr_info("%s: irq(%d)\n", __func__, irq);
	if (irq == pcisd->irq_ovp_power &&
		!gpio_get_value(pcisd->gpio_ovp_power))
		pcisd->event_data[EVENT_OVP_POWER]++;

	if (irq == pcisd->irq_ovp_signal &&
		!gpio_get_value(pcisd->gpio_ovp_signal))
		pcisd->event_data[EVENT_OVP_SIGNAL]++;

	return IRQ_HANDLED;
}

#ifdef CONFIG_OF
static void sec_cisd_parse_dt(struct cisd *pcisd)
{
	struct device_node *np;
	int ret = 0;

	np = of_find_node_by_name(NULL, "sec-cisd");
	if (!np) {
		pr_err("%s: np NULL\n", __func__);
		return;
	}

	ret = of_get_named_gpio(np, "ovp_power", 0);
	if (ret >= 0) {
		pcisd->gpio_ovp_power = ret;
		pr_info("%s: set ovp_power gpio(%d)\n", __func__, pcisd->gpio_ovp_power);
		pcisd->irq_ovp_power = gpio_to_irq(pcisd->gpio_ovp_power);
		ret = request_threaded_irq(pcisd->irq_ovp_power, NULL,
			cisd_irq_thread, IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
			"cisd-ovp-power", pcisd);
		if (ret < 0) {
			pr_err("%s: failed to request ovp_power irq(ret = %d)\n",
				__func__, ret);
			pcisd->irq_ovp_power = 0;
		} else
			pr_info("%s: set irq_ovp_power(%d)\n", __func__, pcisd->irq_ovp_power);
	} else
		pr_err("%s: failed to get ovp_power\n", __func__);

	ret = of_get_named_gpio(np, "ovp_signal", 0);
	if (ret >= 0) {
		pcisd->gpio_ovp_signal = ret;
		pr_info("%s: set ovp_signal gpio(%d)\n", __func__, pcisd->gpio_ovp_signal);
		pcisd->irq_ovp_signal = gpio_to_irq(pcisd->gpio_ovp_signal);
		ret = request_threaded_irq(pcisd->irq_ovp_signal, NULL,
			cisd_irq_thread, IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
			"cisd-ovp-signal", pcisd);
		if (ret < 0) {
			pr_err("%s: failed to request ovp_signal irq(ret = %d)\n",
				__func__, ret);
			pcisd->irq_ovp_signal = 0;
		} else
			pr_info("%s: set irq_ovp_signal(%d)\n", __func__, pcisd->irq_ovp_signal);
	} else
		pr_err("%s: failed to get ovp_signal\n", __func__);
}
#else
static void sec_cisd_parse_dt(struct cisd *pcisd)
{
}
#endif

struct cisd *gcisd;
void sec_battery_cisd_init(struct sec_battery_info *battery)
{
	/* parse dt */
	sec_cisd_parse_dt(&battery->cisd);

	/* init cisd data */
	battery->cisd.state = CISD_STATE_NONE;

	battery->cisd.data[CISD_DATA_ALG_INDEX] = battery->pdata->cisd_alg_index;
	battery->cisd.data[CISD_DATA_FULL_COUNT] = 1;
	battery->cisd.data[CISD_DATA_BATT_TEMP_MAX] = -300;
	battery->cisd.data[CISD_DATA_CHG_TEMP_MAX] = -300;
	battery->cisd.data[CISD_DATA_WPC_TEMP_MAX] = -300;
	battery->cisd.data[CISD_DATA_USB_TEMP_MAX] = -300;
	battery->cisd.data[CISD_DATA_BATT_TEMP_MIN] = 1000;
	battery->cisd.data[CISD_DATA_CHG_TEMP_MIN] = 1000;
	battery->cisd.data[CISD_DATA_WPC_TEMP_MIN] = 1000;
	battery->cisd.data[CISD_DATA_USB_TEMP_MIN] = 1000;
	battery->cisd.data[CISD_DATA_CHG_BATT_TEMP_MAX] = -300;
	battery->cisd.data[CISD_DATA_CHG_CHG_TEMP_MAX] = -300;
	battery->cisd.data[CISD_DATA_CHG_WPC_TEMP_MAX] = -300;
	battery->cisd.data[CISD_DATA_CHG_USB_TEMP_MAX] = -300;
	battery->cisd.data[CISD_DATA_CHG_BATT_TEMP_MIN] = 1000;
	battery->cisd.data[CISD_DATA_CHG_CHG_TEMP_MIN] = 1000;
	battery->cisd.data[CISD_DATA_CHG_WPC_TEMP_MIN] = 1000;
	battery->cisd.data[CISD_DATA_CHG_USB_TEMP_MIN] = 1000;
	battery->cisd.data[CISD_DATA_CAP_MIN] = 0xFFFF;

	battery->cisd.data[CISD_DATA_FULL_COUNT_PER_DAY] = 1;
	battery->cisd.data[CISD_DATA_BATT_TEMP_MAX_PER_DAY] = -300;
	battery->cisd.data[CISD_DATA_SUB_BATT_TEMP_MAX_PER_DAY] = -300;
	battery->cisd.data[CISD_DATA_CHG_TEMP_MAX_PER_DAY] = -300;
	battery->cisd.data[CISD_DATA_USB_TEMP_MAX_PER_DAY] = -300;
	battery->cisd.data[CISD_DATA_BATT_TEMP_MIN_PER_DAY] = 1000;
	battery->cisd.data[CISD_DATA_SUB_BATT_TEMP_MIN_PER_DAY] = 1000;
	battery->cisd.data[CISD_DATA_CHG_TEMP_MIN_PER_DAY] = 1000;
	battery->cisd.data[CISD_DATA_USB_TEMP_MIN_PER_DAY] = 1000;

	battery->cisd.data[CISD_DATA_CHG_BATT_TEMP_MAX_PER_DAY] = -300;
	battery->cisd.data[CISD_DATA_CHG_SUB_BATT_TEMP_MAX_PER_DAY] = -300;
	battery->cisd.data[CISD_DATA_CHG_CHG_TEMP_MAX_PER_DAY] = -300;
	battery->cisd.data[CISD_DATA_CHG_USB_TEMP_MAX_PER_DAY] = -300;
	battery->cisd.data[CISD_DATA_CHG_BATT_TEMP_MIN_PER_DAY] = 1000;
	battery->cisd.data[CISD_DATA_CHG_SUB_BATT_TEMP_MIN_PER_DAY] = 1000;
	battery->cisd.data[CISD_DATA_CHG_CHG_TEMP_MIN_PER_DAY] = 1000;
	battery->cisd.data[CISD_DATA_CHG_USB_TEMP_MIN_PER_DAY] = 1000;

	battery->cisd.ab_vbat_max_count = 2; /* should be 2 */
	battery->cisd.ab_vbat_check_count = 0;
	battery->cisd.max_voltage_thr = battery->pdata->max_voltage_thr;

	/* set cisd pointer */
	gcisd = &battery->cisd;

	/* initialize pad data */
	mutex_init(&battery->cisd.padlock);
	mutex_init(&battery->cisd.powerlock);
	mutex_init(&battery->cisd.pdlock);
	init_cisd_pad_data(&battery->cisd);
	init_cisd_power_data(&battery->cisd);
	init_cisd_pd_data(&battery->cisd);
}

static struct pad_data* create_pad_data(unsigned int pad_id, unsigned int pad_count)
{
	struct pad_data* temp_data;

	temp_data = kzalloc(sizeof(struct pad_data), GFP_KERNEL);
	if (temp_data == NULL)
		return NULL;

	temp_data->id = pad_id;
	temp_data->count = pad_count;
	temp_data->prev = temp_data->next = NULL;

	return temp_data;
}

static struct pad_data* find_pad_data_by_id(struct cisd* cisd, unsigned int pad_id)
{
	struct pad_data* temp_data = cisd->pad_array->next;

	if (cisd->pad_count <= 0 || temp_data == NULL)
		return NULL;

	while ((temp_data->id != pad_id) &&
		((temp_data = temp_data->next) != NULL));

	return temp_data;
}

static void add_pad_data(struct cisd* cisd, unsigned int pad_id, unsigned int pad_count)
{
	struct pad_data* temp_data = cisd->pad_array->next;
	struct pad_data* pad_data;

	if (pad_id >= MAX_PAD_ID)
		return;

	pad_data = create_pad_data(pad_id, pad_count);
	if (pad_data == NULL)
		return;

	pr_info("%s: id(0x%x), count(%d)\n", __func__, pad_id, pad_count);
	while (temp_data) {
		if (temp_data->id > pad_id) {
			temp_data->prev->next = pad_data;
			pad_data->prev = temp_data->prev;
			pad_data->next = temp_data;
			temp_data->prev = pad_data;
			cisd->pad_count++;
			return;
		}
		temp_data = temp_data->next;
	}

	pr_info("%s: failed to add pad_data(%d, %d)\n",
		__func__, pad_id, pad_count);
	kfree(pad_data);
}

void init_cisd_pad_data(struct cisd* cisd)
{
	struct pad_data *temp_data = NULL;
	mutex_lock(&cisd->padlock);
	temp_data = cisd->pad_array;
	while (temp_data) {
		struct pad_data* next_data = temp_data->next;

		kfree(temp_data);
		temp_data = next_data;
	}

	/* create dummy data */
	cisd->pad_array = create_pad_data(0, 0);
	if (cisd->pad_array == NULL)
		goto err_create_dummy_data;
	temp_data = create_pad_data(MAX_PAD_ID, 0);
	if (temp_data == NULL) {
		kfree(cisd->pad_array);
		cisd->pad_array = NULL;
		goto err_create_dummy_data;
	}
	cisd->pad_count = 0;
	cisd->pad_array->next = temp_data;
	temp_data->prev = cisd->pad_array;

err_create_dummy_data:
	mutex_unlock(&cisd->padlock);
}

void count_cisd_pad_data(struct cisd* cisd, unsigned int pad_id)
{
	struct pad_data* pad_data;

	if (cisd->pad_array == NULL) {
		pr_info("%s: can't update the connected count of pad_id(0x%x) because of null\n",
			__func__, pad_id);
		return;
	}

	mutex_lock(&cisd->padlock);
	if ((pad_data = find_pad_data_by_id(cisd, pad_id)) != NULL)
		pad_data->count++;
	else
		add_pad_data(cisd, pad_id, 1);
	mutex_unlock(&cisd->padlock);
}

static unsigned int convert_wc_index_to_pad_id(unsigned int wc_index)
{
	switch (wc_index) {
	case WC_UNKNOWN:
		return WC_PAD_ID_UNKNOWN;
	case WC_SNGL_NOBLE:
		return WC_PAD_ID_SNGL_NOBLE;
	case WC_SNGL_VEHICLE:
		return WC_PAD_ID_SNGL_VEHICLE;
	case WC_SNGL_MINI:
		return WC_PAD_ID_SNGL_MINI;
	case WC_SNGL_ZERO:
		return WC_PAD_ID_SNGL_ZERO;
	case WC_SNGL_DREAM:
		return WC_PAD_ID_SNGL_DREAM;
	case WC_STAND_HERO:
		return WC_PAD_ID_STAND_HERO;
	case WC_STAND_DREAM:
		return WC_PAD_ID_STAND_DREAM;
	case WC_EXT_PACK:
		return WC_PAD_ID_EXT_BATT_PACK;
	case WC_EXT_PACK_TA:
		return WC_PAD_ID_EXT_BATT_PACK_TA;
	default:
		break;
	}

	return 0;
}

void set_cisd_pad_data(struct sec_battery_info *battery, const char* buf)
{
	struct cisd* pcisd = &battery->cisd;
	unsigned int pad_total_count, pad_id, pad_count;
	struct pad_data* pad_data;
	int i, x;

	pr_info("%s: %s\n", __func__, buf);
	if (pcisd->pad_count > 0)
		init_cisd_pad_data(pcisd);

	if (pcisd->pad_array == NULL) {
		pr_info("%s: can't set the pad data because of null\n", __func__);
		return;
	}

	if (sscanf(buf, "%10u %n", &pad_total_count, &x) <= 0) {
		pr_info("%s: failed to read pad index\n", __func__);
		return;
	}
	buf += (size_t)x;
	pr_info("%s: stored pad_total_count(%d)\n", __func__, pad_total_count);

	if (!pad_total_count) {
		for (i = WC_DATA_INDEX + 1; i < WC_DATA_MAX; i++) {
			if (sscanf(buf, "%10u %n", &pad_count, &x) <= 0)
				break;
			buf += (size_t)x;

			if (pad_count > 0) {
				pad_id = convert_wc_index_to_pad_id(i);

				mutex_lock(&pcisd->padlock);
				if ((pad_data = find_pad_data_by_id(pcisd, pad_id)) != NULL)
					pad_data->count = pad_count;
				else
					add_pad_data(pcisd, pad_id, pad_count);
				mutex_unlock(&pcisd->padlock);
			}
		}
	} else {
		if (pad_total_count >= MAX_PAD_ID)
			return;

		pr_info("%s: add pad data(count: %d)\n", __func__, pad_total_count);
		for (i = 0; i < pad_total_count; i++) {
			if (sscanf(buf, "0x%02x:%10u %n", &pad_id, &pad_count, &x) != 2) {
				pr_info("%s: failed to read pad data(0x%x, %d, %d)!!!re-init pad data\n",
					__func__, pad_id, pad_count, x);
				init_cisd_pad_data(pcisd);
				break;
			}
			buf += (size_t)x;

			mutex_lock(&pcisd->padlock);
			if ((pad_data = find_pad_data_by_id(pcisd, pad_id)) != NULL)
				pad_data->count = pad_count;
			else
				add_pad_data(pcisd, pad_id, pad_count);
			mutex_unlock(&pcisd->padlock);
		}
	}
}

static struct power_data *create_power_data(unsigned int power, unsigned int power_count)
{
	struct power_data *temp_data;

	temp_data = kzalloc(sizeof(struct power_data), GFP_KERNEL);
	if (temp_data == NULL)
		return NULL;

	temp_data->power = power;
	temp_data->count = power_count;
	temp_data->prev = temp_data->next = NULL;

	return temp_data;
}

static struct power_data *find_data_by_power(struct cisd *cisd, unsigned int power)
{
	struct power_data *temp_data = cisd->power_array->next;

	if (cisd->power_count <= 0 || temp_data == NULL)
		return NULL;

	while ((temp_data->power != power) &&
		((temp_data = temp_data->next) != NULL))
		;

	return temp_data;
}

static void add_power_data(struct cisd *cisd, unsigned int power, unsigned int power_count)
{
	struct power_data *temp_data = cisd->power_array->next;
	struct power_data *power_data;

	power_data = create_power_data(power, power_count);
	if (power_data == NULL)
		return;

	pr_info("%s: power(%d), count(%d)\n", __func__, power, power_count);
	while (temp_data) {
		if (temp_data->power > power) {
			temp_data->prev->next = power_data;
			power_data->prev = temp_data->prev;
			power_data->next = temp_data;
			temp_data->prev = power_data;
			cisd->power_count++;
			return;
		}
		temp_data = temp_data->next;
	}

	pr_info("%s: failed to add pad_data(%d, %d)\n",
		__func__, power, power_count);
	kfree(power_data);
}

void init_cisd_power_data(struct cisd *cisd)
{
	struct power_data *temp_data = NULL;

	mutex_lock(&cisd->powerlock);
	temp_data = cisd->power_array;
	while (temp_data) {
		struct power_data *next_data = temp_data->next;

		kfree(temp_data);
		temp_data = next_data;
	}

	/* create dummy data */
	cisd->power_array = create_power_data(0, 0);
	if (cisd->power_array == NULL)
		goto err_create_dummy_data;
	temp_data = create_power_data(MAX_CHARGER_POWER, 0);
	if (temp_data == NULL) {
		kfree(cisd->power_array);
		cisd->power_array = NULL;
		goto err_create_dummy_data;
	}
	cisd->power_count = 0;
	cisd->power_array->next = temp_data;
	temp_data->prev = cisd->power_array;

err_create_dummy_data:
	mutex_unlock(&cisd->powerlock);
}

#define FIND_MAX_POWER 45000
#define FIND_POWER_STEP 10000
#define POWER_MARGIN 1000
void count_cisd_power_data(struct cisd *cisd, int power)
{
	struct power_data *power_data;
	int power_index = 0;

	pr_info("%s: power value : %d\n", __func__, power);
	if (cisd->power_array == NULL || power < 15000) {
		pr_info("%s: can't update the connected count of power(%d) because of null\n",
			__func__, power);
		return;
	}

	power_index = FIND_MAX_POWER;
	while (power_index >= 14000) {
		if (power + POWER_MARGIN - power_index >= 0) {
			power_index /= 1000;
			break;
		}

		power_index -= FIND_POWER_STEP;
	}

	mutex_lock(&cisd->powerlock);
	power_data = find_data_by_power(cisd, power_index);
	if (power_data != NULL)
		power_data->count++;
	else
		add_power_data(cisd, power_index, 1);
	mutex_unlock(&cisd->powerlock);
}

void set_cisd_power_data(struct sec_battery_info *battery, const char *buf)
{
	struct cisd *pcisd = &battery->cisd;
	unsigned int power_total_count, power_id, power_count;
	struct power_data *power_data;
	int i, x;

	pr_info("%s: %s\n", __func__, buf);
	if (pcisd->power_count > 0)
		init_cisd_power_data(pcisd);

	if (pcisd->power_array == NULL) {
		pr_info("%s: can't set the power data because of null\n", __func__);
		return;
	}

	if (sscanf(buf, "%10u %n", &power_total_count, &x) <= 0)
		return;

	buf += (size_t)x;
	pr_info("%s: add power data(count: %d)\n", __func__, power_total_count);
	for (i = 0; i < power_total_count; i++) {
		if (sscanf(buf, "%10u:%10u %n", &power_id, &power_count, &x) != 2) {
			pr_info("%s: failed to read power data(%d, %d, %d)!!!re-init power data\n",
				__func__, power_id, power_count, x);
			init_cisd_power_data(pcisd);
			break;
		}
		buf += (size_t)x;
		mutex_lock(&pcisd->powerlock);
		power_data = find_data_by_power(pcisd, power_id);
		if (power_data != NULL)
			power_data->count = power_count;
		else
			add_power_data(pcisd, power_id, power_count);
		mutex_unlock(&pcisd->powerlock);
	}
}

static struct pd_data *create_pd_data(unsigned short pid, unsigned int pd_count)
{
	struct pd_data *temp_data;

	temp_data = kzalloc(sizeof(struct pd_data), GFP_KERNEL);
	if (temp_data == NULL)
		return NULL;

	temp_data->pid = pid;
	temp_data->count = pd_count;
	temp_data->prev = temp_data->next = NULL;

	return temp_data;
}

static struct pd_data *find_data_by_pd(struct cisd *cisd, unsigned short pid)
{
	struct pd_data *temp_data = cisd->pd_array->next;

	if (cisd->pd_count <= 0 || temp_data == NULL)
		return NULL;

	while ((temp_data->pid != pid) &&
		((temp_data = temp_data->next) != NULL))
		;

	return temp_data;
}

static void add_pd_data(struct cisd *cisd, unsigned short pid, unsigned int pd_count)
{
	struct pd_data *temp_data = cisd->pd_array->next;
	struct pd_data *pd_data;

	pd_data = create_pd_data(pid, pd_count);
	if (pd_data == NULL)
		return;

	pr_info("%s: pid(0x%04x), count(%d)\n", __func__, pid, pd_count);
	while (temp_data) {
		if (temp_data->pid > pid) {
			temp_data->prev->next = pd_data;
			pd_data->prev = temp_data->prev;
			pd_data->next = temp_data;
			temp_data->prev = pd_data;
			cisd->pd_count++;
			return;
		}
		temp_data = temp_data->next;
	}

	pr_info("%s: failed to add pd_data(0x%04x, %d)\n",
		__func__, pid, pd_count);
	kfree(pd_data);
}

void init_cisd_pd_data(struct cisd *cisd)
{
	struct pd_data *temp_data = NULL;

	mutex_lock(&cisd->pdlock);
	temp_data = cisd->pd_array;
	while (temp_data) {
		struct pd_data *next_data = temp_data->next;

		kfree(temp_data);
		temp_data = next_data;
	}

	/* create dummy data */
	cisd->pd_array = create_pd_data(0, 0);
	if (cisd->pd_array == NULL)
		goto err_create_dummy_data;
	temp_data = create_pd_data(MAX_SS_PD_PID, 0);
	if (temp_data == NULL) {
		kfree(cisd->pd_array);
		cisd->pd_array = NULL;
		goto err_create_dummy_data;
	}
	cisd->pd_count = 0;
	cisd->pd_array->next = temp_data;
	temp_data->prev = cisd->pd_array;

err_create_dummy_data:
	mutex_unlock(&cisd->pdlock);
}
EXPORT_SYMBOL(init_cisd_pd_data);

void count_cisd_pd_data(unsigned short vid, unsigned short pid)
{
	struct power_supply *psy = power_supply_get_by_name("battery");
	struct sec_battery_info *battery = power_supply_get_drvdata(psy);
	struct cisd *cisd = &battery->cisd;
	struct pd_data *pd_data;

	pr_info("%s: vid : 0x%04x, pid : 0x%04x\n", __func__, vid, pid);
	if (cisd->pd_array == NULL) {
		pr_info("%s: can't update count of pid(0x%04x) because of null\n",
			__func__, pid);
		return;
	}

	if ((vid != SS_PD_VID) || ((vid == SS_PD_VID)
		&& (pid < MIN_SS_PD_PID || pid > MAX_SS_PD_PID))) {
		pr_info("%s: other pd ta(vid_0x%04x, pid_0x%04x). change pid to 0x0000\n",
			__func__, vid, pid);
		pid = 0x0;
	}

	mutex_lock(&cisd->pdlock);
	pd_data = find_data_by_pd(cisd, pid);
	if (pd_data != NULL)
		pd_data->count++;
	else
		add_pd_data(cisd, pid, 1);
	mutex_unlock(&cisd->pdlock);
}
EXPORT_SYMBOL(count_cisd_pd_data);

void set_cisd_pd_data(struct sec_battery_info *battery, const char *buf)
{
	struct cisd *pcisd = &battery->cisd;
	unsigned int pd_total_count, pid, pd_count;
	struct pd_data *pd_data;
	int i, x;

	pr_info("%s: %s\n", __func__, buf);
	if (pcisd->pd_count > 0)
		init_cisd_pd_data(pcisd);

	if (pcisd->pd_array == NULL) {
		pr_info("%s: can't set the pd data because of null\n", __func__);
		return;
	}

	if (sscanf(buf, "%10u %n", &pd_total_count, &x) <= 0)
		return;

	buf += (size_t)x;
	pr_info("%s: add pd data(count: %d)\n", __func__, pd_total_count);
	for (i = 0; i < pd_total_count; i++) {
		if (sscanf(buf, "0x%04x:%10u %n", &pid, &pd_count, &x) != 2) {
			pr_info("%s: failed to read pd data(0x%04x, %d, %d)!!!re-init pd data\n",
				__func__, pid, pd_count, x);
			init_cisd_pd_data(pcisd);
			break;
		}
		buf += (size_t)x;
		mutex_lock(&pcisd->pdlock);
		pd_data = find_data_by_pd(pcisd, pid);
		if (pd_data != NULL)
			pd_data->count = pd_count;
		else
			add_pd_data(pcisd, pid, pd_count);
		mutex_unlock(&pcisd->pdlock);
	}
}
EXPORT_SYMBOL(set_cisd_pd_data);


static ssize_t sysfs_cisd_show_attrs(struct device *dev,
				struct device_attribute *attr, char *buf);

static ssize_t sysfs_cisd_store_attrs(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count);

#define SYSFS_CISD_ATTR(_name)						\
{									\
	.attr = {.name = #_name, .mode = 0664},	\
	.show = sysfs_cisd_show_attrs,					\
	.store = sysfs_cisd_store_attrs,					\
}

static struct device_attribute sysfs_cisd_attrs[] = {
	SYSFS_CISD_ATTR(cisd_data),
	SYSFS_CISD_ATTR(cisd_data_json),
	SYSFS_CISD_ATTR(cisd_data_d_json),
	SYSFS_CISD_ATTR(cisd_wire_count),
	SYSFS_CISD_ATTR(cisd_wc_data),
	SYSFS_CISD_ATTR(cisd_wc_data_json),
	SYSFS_CISD_ATTR(cisd_power_data),
	SYSFS_CISD_ATTR(cisd_power_data_json),
	SYSFS_CISD_ATTR(cisd_pd_data),
	SYSFS_CISD_ATTR(cisd_pd_data_json),
	SYSFS_CISD_ATTR(cisd_cable_data),
	SYSFS_CISD_ATTR(cisd_cable_data_json),
	SYSFS_CISD_ATTR(cisd_tx_data),
	SYSFS_CISD_ATTR(cisd_tx_data_json),
	SYSFS_CISD_ATTR(cisd_event_data),
	SYSFS_CISD_ATTR(cisd_event_data_json),
	SYSFS_CISD_ATTR(prev_battery_data),
	SYSFS_CISD_ATTR(prev_battery_info),
};

enum {
	CISD_DATA = 0,
	CISD_DATA_JSON,
	CISD_DATA_D_JSON,
	CISD_WIRE_COUNT,
	CISD_WC_DATA,
	CISD_WC_DATA_JSON,
	CISD_POWER_DATA,
	CISD_POWER_DATA_JSON,
	CISD_PD_DATA,
	CISD_PD_DATA_JSON,
	CISD_CABLE_DATA,
	CISD_CABLE_DATA_JSON,
	CISD_TX_DATA,
	CISD_TX_DATA_JSON,
	CISD_EVENT_DATA,
	CISD_EVENT_DATA_JSON,
	PREV_BATTERY_DATA,
	PREV_BATTERY_INFO,
};


ssize_t sysfs_cisd_show_attrs(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	struct power_supply *psy = dev_get_drvdata(dev);
	struct sec_battery_info *battery = power_supply_get_drvdata(psy);
	const ptrdiff_t offset = attr - sysfs_cisd_attrs;
	int i = 0;

	switch (offset) {
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
				if (battery->pdata->ignore_cisd_index[j / 32] & (0x1 << (j % 32)))
					continue;
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
				if (battery->pdata->ignore_cisd_index_d[(j - CISD_DATA_FULL_COUNT_PER_DAY) / 32] & (0x1 << ((j - CISD_DATA_FULL_COUNT_PER_DAY) % 32)))
					continue;
				snprintf(temp_buf+strlen(temp_buf), size, ",\"%s\":\"%d\"",
				cisd_data_str_d[j-CISD_DATA_MAX], pcisd->data[j]);
				size = sizeof(temp_buf) - strlen(temp_buf);
			}

			/* Clear Daily Data */
			for (j = CISD_DATA_FULL_COUNT_PER_DAY; j < CISD_DATA_MAX_PER_DAY; j++)
				pcisd->data[j] = 0;

			pcisd->data[CISD_DATA_FULL_COUNT_PER_DAY] = 1;
			pcisd->data[CISD_DATA_BATT_TEMP_MAX_PER_DAY] = -300;
			pcisd->data[CISD_DATA_SUB_BATT_TEMP_MAX_PER_DAY] = -300;
			pcisd->data[CISD_DATA_CHG_TEMP_MAX_PER_DAY] = -300;
			pcisd->data[CISD_DATA_USB_TEMP_MAX_PER_DAY] = -300;
			pcisd->data[CISD_DATA_BATT_TEMP_MIN_PER_DAY] = 1000;
			pcisd->data[CISD_DATA_SUB_BATT_TEMP_MIN_PER_DAY] = 1000;
			pcisd->data[CISD_DATA_CHG_TEMP_MIN_PER_DAY] = 1000;
			pcisd->data[CISD_DATA_USB_TEMP_MIN_PER_DAY] = 1000;

			pcisd->data[CISD_DATA_CHG_BATT_TEMP_MAX_PER_DAY] = -300;
			pcisd->data[CISD_DATA_CHG_SUB_BATT_TEMP_MAX_PER_DAY] = -300;
			pcisd->data[CISD_DATA_CHG_CHG_TEMP_MAX_PER_DAY] = -300;
			pcisd->data[CISD_DATA_CHG_USB_TEMP_MAX_PER_DAY] = -300;
			pcisd->data[CISD_DATA_CHG_BATT_TEMP_MIN_PER_DAY] = 1000;
			pcisd->data[CISD_DATA_CHG_SUB_BATT_TEMP_MIN_PER_DAY] = 1000;
			pcisd->data[CISD_DATA_CHG_CHG_TEMP_MIN_PER_DAY] = 1000;
			pcisd->data[CISD_DATA_CHG_USB_TEMP_MIN_PER_DAY] = 1000;

			pcisd->data[CISD_DATA_CAP_MIN_PER_DAY] = 0xFFFF;
			i += scnprintf(buf + i, PAGE_SIZE - i, "%s\n", temp_buf);

#if defined(CONFIG_WIRELESS_TX_MODE)
			/* clear accumulated power consumption by Tx */
			battery->tx_clear_cisd = true;
#endif
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
			struct pad_data *pad_data = NULL;
			char temp_buf[1024] = {0,};
			int j = 0, size = 1024;

			mutex_lock(&pcisd->padlock);
			pad_data = pcisd->pad_array;
			snprintf(temp_buf, size, "%d", pcisd->pad_count);
			while ((pad_data != NULL) && ((pad_data = pad_data->next) != NULL) &&
					(pad_data->id < MAX_PAD_ID) && (j++ < pcisd->pad_count)) {
				snprintf(temp_buf+strlen(temp_buf), size, " 0x%02x:%d", pad_data->id, pad_data->count);
				size = sizeof(temp_buf) - strlen(temp_buf);
			}
			mutex_unlock(&pcisd->padlock);
			i += scnprintf(buf + i, PAGE_SIZE - i, "%s\n", temp_buf);
		}
		break;
	case CISD_WC_DATA_JSON:
		{
			struct cisd *pcisd = &battery->cisd;
			struct pad_data *pad_data = NULL;
			char temp_buf[1024] = {0,};
			int j = 0, size = 1024;

			mutex_lock(&pcisd->padlock);
			pad_data = pcisd->pad_array;
			snprintf(temp_buf+strlen(temp_buf), size, "\"%s\":\"%d\"",
				PAD_INDEX_STRING, pcisd->pad_count);
			while ((pad_data != NULL) && ((pad_data = pad_data->next) != NULL) &&
					(pad_data->id < MAX_PAD_ID) && (j++ < pcisd->pad_count)) {
				snprintf(temp_buf+strlen(temp_buf), size, ",\"%s%02x\":\"%d\"",
					PAD_JSON_STRING, pad_data->id, pad_data->count);
				size = sizeof(temp_buf) - strlen(temp_buf);
			}
			mutex_unlock(&pcisd->padlock);
			i += scnprintf(buf + i, PAGE_SIZE - i, "%s\n", temp_buf);
		}
		break;
	case CISD_POWER_DATA:
		{
			struct cisd *pcisd = &battery->cisd;
			struct power_data *power_data = NULL;
			char temp_buf[1024] = {0,};
			int j = 0, size = 1024;

			mutex_lock(&pcisd->powerlock);
			power_data = pcisd->power_array;
			snprintf(temp_buf+strlen(temp_buf), size, "%d", pcisd->power_count);
			while ((power_data != NULL) && ((power_data = power_data->next) != NULL) &&
					(power_data->power < MAX_CHARGER_POWER) && (j++ < pcisd->power_count)) {
				snprintf(temp_buf+strlen(temp_buf), size, " %d:%d",
					power_data->power, power_data->count);
				size = sizeof(temp_buf) - strlen(temp_buf);
			}
			mutex_unlock(&pcisd->powerlock);
			i += scnprintf(buf + i, PAGE_SIZE - i, "%s\n", temp_buf);
		}
		break;
	case CISD_POWER_DATA_JSON:
		{
			struct cisd *pcisd = &battery->cisd;
			struct power_data *power_data = NULL;
			char temp_buf[1024] = {0,};
			int j = 0, size = 1024;

			mutex_lock(&pcisd->powerlock);
			power_data = pcisd->power_array;
			snprintf(temp_buf+strlen(temp_buf), size, "\"%s\":\"%d\"",
				POWER_COUNT_JSON_STRING, pcisd->power_count);
			while ((power_data != NULL) && ((power_data = power_data->next) != NULL) &&
					(power_data->power < MAX_CHARGER_POWER) && (j++ < pcisd->power_count)) {
				snprintf(temp_buf+strlen(temp_buf), size, ",\"%s%d\":\"%d\"",
					POWER_JSON_STRING, power_data->power, power_data->count);
				size = sizeof(temp_buf) - strlen(temp_buf);
			}
			mutex_unlock(&pcisd->powerlock);

			/* clear daily power data */
			init_cisd_power_data(&battery->cisd);

			i += scnprintf(buf + i, PAGE_SIZE - i, "%s\n", temp_buf);
		}
		break;
	case CISD_PD_DATA:
		{
			struct cisd *pcisd = &battery->cisd;
			struct pd_data *pd_data = NULL;
			char temp_buf[1024] = {0,};
			int j = 0, size = 1024;

			mutex_lock(&pcisd->pdlock);
			pd_data = pcisd->pd_array;
			snprintf(temp_buf+strlen(temp_buf), size, "%d", pcisd->pd_count);
			while ((pd_data != NULL) && ((pd_data = pd_data->next) != NULL) &&
					(pd_data->pid < MAX_SS_PD_PID) && (j++ < pcisd->pd_count)) {
				snprintf(temp_buf+strlen(temp_buf), size, " 0x%04x:%d", pd_data->pid, pd_data->count);
				size = sizeof(temp_buf) - strlen(temp_buf);
			}
			mutex_unlock(&pcisd->pdlock);
			i += scnprintf(buf + i, PAGE_SIZE - i, "%s\n", temp_buf);
		}
		break;
	case CISD_PD_DATA_JSON:
		{
			struct cisd *pcisd = &battery->cisd;
			struct pd_data *pd_data = NULL;
			char temp_buf[1024] = {0,};
			int j = 0, size = 1024;

			mutex_lock(&pcisd->pdlock);
			pd_data = pcisd->pd_array;
			snprintf(temp_buf+strlen(temp_buf), size, "\"%s\":\"%d\"",
				PD_COUNT_JSON_STRING, pcisd->pd_count);
			while ((pd_data != NULL) && ((pd_data = pd_data->next) != NULL) &&
					(pd_data->pid < MAX_SS_PD_PID) && (j++ < pcisd->pd_count)) {
				if (pd_data->pid == 0x0)
					snprintf(temp_buf+strlen(temp_buf), size, ",\"PID_OTHER\":\"%d\"",
						pd_data->count);
				else
					snprintf(temp_buf+strlen(temp_buf), size, ",\"%s%04x\":\"%d\"",
						PD_JSON_STRING, pd_data->pid, pd_data->count);
				size = sizeof(temp_buf) - strlen(temp_buf);
			}
			mutex_unlock(&pcisd->pdlock);
			i += scnprintf(buf + i, PAGE_SIZE - i, "%s\n", temp_buf);
		}
		break;
	case CISD_CABLE_DATA:
		{
			struct cisd *pcisd = &battery->cisd;
			char temp_buf[1024] = {0,};
			int j = 0;
			int size = 0;

			snprintf(temp_buf, sizeof(temp_buf), "%d", pcisd->cable_data[CISD_CABLE_TA]);
			size = sizeof(temp_buf) - strlen(temp_buf);

			for (j = CISD_CABLE_TA + 1; j < CISD_CABLE_TYPE_MAX; j++) {
				snprintf(temp_buf+strlen(temp_buf), size, " %d", pcisd->cable_data[j]);
				size = sizeof(temp_buf) - strlen(temp_buf);
			}
			i += scnprintf(buf + i, PAGE_SIZE - i, "%s\n", temp_buf);

		}
		break;
	case CISD_CABLE_DATA_JSON:
		{
			struct cisd *pcisd = &battery->cisd;
			char temp_buf[1024] = {0,};
			int j = 0;
			int size = 0;

			snprintf(temp_buf, sizeof(temp_buf), "\"%s\":\"%d\"",
				cisd_cable_data_str[CISD_CABLE_TA], pcisd->cable_data[CISD_CABLE_TA]);
			size = sizeof(temp_buf) - strlen(temp_buf);

			for (j = CISD_CABLE_TA + 1; j < CISD_CABLE_TYPE_MAX; j++) {
				snprintf(temp_buf+strlen(temp_buf), size, ",\"%s\":\"%d\"",
					cisd_cable_data_str[j], pcisd->cable_data[j]);
				size = sizeof(temp_buf) - strlen(temp_buf);
			}

			/* Clear Daily Cable Data */
			for (j = CISD_CABLE_TA; j < CISD_CABLE_TYPE_MAX; j++)
				pcisd->cable_data[j] = 0;

			i += scnprintf(buf + i, PAGE_SIZE - i, "%s\n", temp_buf);
		}
		break;
	case CISD_TX_DATA:
		{
			struct cisd *pcisd = &battery->cisd;
			char temp_buf[1024] = {0,};
			int j = 0;
			int size = 0;

			snprintf(temp_buf, sizeof(temp_buf), "%d", pcisd->tx_data[TX_ON]);
			size = sizeof(temp_buf) - strlen(temp_buf);

			for (j = TX_ON + 1; j < TX_DATA_MAX; j++) {
				snprintf(temp_buf+strlen(temp_buf), size, " %d", pcisd->tx_data[j]);
				size = sizeof(temp_buf) - strlen(temp_buf);
			}
			i += scnprintf(buf + i, PAGE_SIZE - i, "%s\n", temp_buf);
		}
		break;
	case CISD_TX_DATA_JSON:
		{
			struct cisd *pcisd = &battery->cisd;
			char temp_buf[1024] = {0,};
			int j = 0;
			int size = 0;

			snprintf(temp_buf, sizeof(temp_buf), "\"%s\":\"%d\"",
				cisd_tx_data_str[TX_ON], pcisd->tx_data[TX_ON]);
			size = sizeof(temp_buf) - strlen(temp_buf);

			for (j = TX_ON + 1; j < TX_DATA_MAX; j++) {
				snprintf(temp_buf+strlen(temp_buf), size, ",\"%s\":\"%d\"",
					cisd_tx_data_str[j], pcisd->tx_data[j]);
				size = sizeof(temp_buf) - strlen(temp_buf);
			}

			/* Clear Daily Tx Data */
			for (j = TX_ON; j < TX_DATA_MAX; j++)
				pcisd->tx_data[j] = 0;

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
	default:
		i = -EINVAL;
		break;
	}

	return i;
}

ssize_t sysfs_cisd_store_attrs(
					struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	struct power_supply *psy = dev_get_drvdata(dev);
	struct sec_battery_info *battery = power_supply_get_drvdata(psy);
	const ptrdiff_t offset = attr - sysfs_cisd_attrs;
	int ret = -EINVAL;
	int x = 0;
	int i = 0;

	switch (offset) {
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

					pcisd->data[CISD_DATA_ALG_INDEX] = battery->pdata->cisd_alg_index;
					pcisd->data[CISD_DATA_FULL_COUNT] = temp_data[0];
					pcisd->data[CISD_DATA_CAP_MAX] = temp_data[1];
					pcisd->data[CISD_DATA_CAP_MIN] = temp_data[2];
					pcisd->data[CISD_DATA_VALERT_COUNT] = temp_data[16];
					pcisd->data[CISD_DATA_CYCLE] = temp_data[17];
					pcisd->data[CISD_DATA_WIRE_COUNT] = temp_data[18];
					pcisd->data[CISD_DATA_WIRELESS_COUNT] = temp_data[19];
					pcisd->data[CISD_DATA_HIGH_TEMP_SWELLING] = temp_data[20];
					pcisd->data[CISD_DATA_LOW_TEMP_SWELLING] = temp_data[21];
					pcisd->data[CISD_DATA_WC_HIGH_TEMP_SWELLING] = temp_data[22];
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
					if (sscanf(p, "%10d%n", &pcisd->data[i], &x) > 0) {
						p += (size_t)x;
						if (pcisd->data[CISD_DATA_ALG_INDEX] != battery->pdata->cisd_alg_index) {
							pr_info("%s: ALG_INDEX is changed %d -> %d\n", __func__,
								pcisd->data[CISD_DATA_ALG_INDEX], battery->pdata->cisd_alg_index);
							temp_data[CISD_DATA_RESET_ALG] = -1;
							break;
						}
					} else {
						pr_info("%s: NO DATA (cisd_data)\n", __func__);
						temp_data[CISD_DATA_RESET_ALG] = -1;
						break;
					}
				}

				pr_info("%s: %s cisd data\n", __func__,
					((temp_data[CISD_DATA_RESET_ALG] < 0 || battery->fg_reset) ?
					"init" : "update"));

				if (temp_data[CISD_DATA_RESET_ALG] < 0 || battery->fg_reset) {
					/* initialize data */
					for (i = CISD_DATA_RESET_ALG; i < CISD_DATA_MAX_PER_DAY; i++)
						pcisd->data[i] = 0;

					battery->fg_reset = 0;

					pcisd->data[CISD_DATA_ALG_INDEX] = battery->pdata->cisd_alg_index;

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
					pcisd->data[CISD_DATA_SUB_BATT_TEMP_MAX_PER_DAY] = -300;
					pcisd->data[CISD_DATA_CHG_TEMP_MAX_PER_DAY] = -300;
					pcisd->data[CISD_DATA_USB_TEMP_MAX_PER_DAY] = -300;
					pcisd->data[CISD_DATA_BATT_TEMP_MIN_PER_DAY] = 1000;
					pcisd->data[CISD_DATA_SUB_BATT_TEMP_MIN_PER_DAY] = 1000;
					pcisd->data[CISD_DATA_CHG_TEMP_MIN_PER_DAY] = 1000;
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
					pcisd->data[CISD_DATA_CHG_SUB_BATT_TEMP_MAX_PER_DAY] = -300;
					pcisd->data[CISD_DATA_CHG_CHG_TEMP_MAX_PER_DAY] = -300;
					pcisd->data[CISD_DATA_CHG_USB_TEMP_MAX_PER_DAY] = -300;
					pcisd->data[CISD_DATA_CHG_BATT_TEMP_MIN_PER_DAY] = 1000;
					pcisd->data[CISD_DATA_CHG_SUB_BATT_TEMP_MIN_PER_DAY] = 1000;
					pcisd->data[CISD_DATA_CHG_CHG_TEMP_MIN_PER_DAY] = 1000;
					pcisd->data[CISD_DATA_CHG_USB_TEMP_MIN_PER_DAY] = 1000;

					pcisd->data[CISD_DATA_CAP_MIN_PER_DAY] = 0xFFFF;
					pcisd->data[CISD_DATA_USB_OVERHEAT_RAPID_CHANGE] = 0;
					pcisd->data[CISD_DATA_USB_OVERHEAT_RAPID_CHANGE_PER_DAY] = 0;
					pcisd->data[CISD_DATA_USB_OVERHEAT_ALONE] = 0;

					/* initialize pad data */
					init_cisd_pad_data(&battery->cisd);

					/* initialize power data */
					init_cisd_power_data(&battery->cisd);

					/* initialize pd data */
					init_cisd_pd_data(&battery->cisd);
				}
			}
			ret = count;
			__pm_stay_awake(battery->monitor_ws);
			queue_delayed_work(battery->monitor_wqueue, &battery->monitor_work, 0);
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
					pcisd->data[CISD_DATA_SUB_BATT_TEMP_MAX_PER_DAY] = -300;
					pcisd->data[CISD_DATA_CHG_TEMP_MAX_PER_DAY] = -300;
					pcisd->data[CISD_DATA_USB_TEMP_MAX_PER_DAY] = -300;
					pcisd->data[CISD_DATA_BATT_TEMP_MIN_PER_DAY] = 1000;
					pcisd->data[CISD_DATA_SUB_BATT_TEMP_MIN_PER_DAY] = 1000;
					pcisd->data[CISD_DATA_CHG_TEMP_MIN_PER_DAY] = 1000;
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
					pcisd->data[CISD_DATA_CHG_SUB_BATT_TEMP_MAX_PER_DAY] = -300;
					pcisd->data[CISD_DATA_CHG_CHG_TEMP_MAX_PER_DAY] = -300;
					pcisd->data[CISD_DATA_CHG_USB_TEMP_MAX_PER_DAY] = -300;
					pcisd->data[CISD_DATA_CHG_BATT_TEMP_MIN_PER_DAY] = 1000;
					pcisd->data[CISD_DATA_CHG_SUB_BATT_TEMP_MIN_PER_DAY] = 1000;
					pcisd->data[CISD_DATA_CHG_CHG_TEMP_MIN_PER_DAY] = 1000;
					pcisd->data[CISD_DATA_CHG_USB_TEMP_MIN_PER_DAY] = 1000;

					pcisd->data[CISD_DATA_CAP_MIN_PER_DAY] = 0xFFFF;
					pcisd->data[CISD_DATA_USB_OVERHEAT_RAPID_CHANGE] = 0;
					pcisd->data[CISD_DATA_USB_OVERHEAT_RAPID_CHANGE_PER_DAY] = 0;
					pcisd->data[CISD_DATA_USB_OVERHEAT_ALONE] = 0;
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
	case CISD_POWER_DATA:
		set_cisd_power_data(battery, buf);
		ret = count;
		break;
	case CISD_POWER_DATA_JSON:
		break;
	case CISD_PD_DATA:
		set_cisd_pd_data(battery, buf);
		ret = count;
		break;
	case CISD_PD_DATA_JSON:
		break;
	case CISD_CABLE_DATA:
		{
			struct cisd *pcisd = &battery->cisd;
			const char *p = buf;

			pr_info("%s: %s\n", __func__, buf);
			for (i = CISD_CABLE_TA; i < CISD_CABLE_TYPE_MAX; i++) {
				if (sscanf(p, "%10d%n", &pcisd->cable_data[i], &x) > 0) {
					p += (size_t)x;
				} else {
					pr_info("%s: NO DATA (CISD_CABLE_TYPE)\n", __func__);
					pcisd->cable_data[i] = 0;
					break;
				}
			}
		}
		ret = count;
		break;
	case CISD_CABLE_DATA_JSON:
		break;
	case CISD_TX_DATA:
		{
			struct cisd *pcisd = &battery->cisd;
			const char *p = buf;

			pr_info("%s: %s\n", __func__, buf);
			for (i = TX_ON; i < TX_DATA_MAX; i++) {
				if (sscanf(p, "%10d%n", &pcisd->tx_data[i], &x) > 0) {
					p += (size_t)x;
				} else {
					pr_info("%s: NO DATA (CISD_TX_DATA)\n", __func__);
					pcisd->tx_data[i] = 0;
					break;
				}
			}
		}
		ret = count;
		break;
	case CISD_TX_DATA_JSON:
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
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

static struct sec_sysfs sysfs_cisd_list = {
	.attr = sysfs_cisd_attrs,
	.size = ARRAY_SIZE(sysfs_cisd_attrs),
};

static int __init sysfs_cisd_init(void)
{
	add_sec_sysfs(&sysfs_cisd_list.list);
	return 0;
}
late_initcall(sysfs_cisd_init);
