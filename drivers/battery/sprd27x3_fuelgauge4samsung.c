/*
 * Copyright (C) 2013 Spreadtrum Communications Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/err.h>
#include <mach/hardware.h>
#include <mach/sci.h>
#include <mach/sci_glb_regs.h>
#include <linux/io.h>
#include <mach/adi.h>
#include <linux/wakelock.h>
#include <linux/hrtimer.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/power_supply.h>
#include <linux/battery/sec_fuelgauge.h>
#include <linux/battery/fuelgauge/sprd27x3_fuelgauge4samsung.h>
//#include <linux/sprd_battery_common.h>
#include "sprd_2713_fgu.h"
#include "sprd_battery.h"

int sprdbat_interpolate(int x, int n, struct sprdbat_table_data *tab)
{
	int index;
	int y;

	if (x >= tab[0].x)
		y = tab[0].y;
	else if (x <= tab[n - 1].x)
		y = tab[n - 1].y;
	else {
		/*  find interval */
		for (index = 1; index < n; index++)
			if (x > tab[index].x)
				break;
		/*  interpolate */
		y = (tab[index - 1].y - tab[index].y) * (x - tab[index].x)
		    * 2 / (tab[index - 1].x - tab[index].x);
		y = (y + 1) / 2;
		y += tab[index].y;
	}
	return y;
}
static void print_pdata(struct sprd_battery_platform_data *pdata)
{
#define PDATA_LOG(format, arg...) printk("sprdbat pdata: " format, ## arg)
	int i;


	PDATA_LOG("chg_bat_safety_vol:%d\n", pdata->chg_bat_safety_vol);

	PDATA_LOG("irq_fgu:%d\n", pdata->irq_fgu);
	PDATA_LOG("fgu_mode:%d\n", pdata->fgu_mode);
	PDATA_LOG("alm_soc:%d\n", pdata->alm_soc);
	PDATA_LOG("alm_vol:%d\n", pdata->alm_vol);
	PDATA_LOG("soft_vbat_uvlo:%d\n", pdata->soft_vbat_uvlo);
	PDATA_LOG("soft_vbat_ovp:%d\n", pdata->soft_vbat_ovp);
	PDATA_LOG("rint:%d\n", pdata->rint);
	PDATA_LOG("cnom:%d\n", pdata->cnom);
	PDATA_LOG("rsense_real:%d\n", pdata->rsense_real);
	PDATA_LOG("rsense_spec:%d\n", pdata->rsense_spec);
	PDATA_LOG("relax_current:%d\n", pdata->relax_current);
	PDATA_LOG("fgu_cal_ajust:%d\n", pdata->fgu_cal_ajust);
	PDATA_LOG("qmax_update_period:%d\n", pdata->qmax_update_period);
	PDATA_LOG("ocv_tab_size:%d\n", pdata->ocv_tab_size);
	for (i = 0; i < pdata->ocv_tab_size; i++) {
		PDATA_LOG("ocv_tab i=%d x:%d,y:%d\n", i, pdata->ocv_tab[i].x,
			  pdata->ocv_tab[i].y);
	}
	for (i = 0; i < pdata->temp_tab_size; i++) {
		PDATA_LOG("temp_tab_size i=%d x:%d,y:%d\n", i,
			  pdata->temp_tab[i].x, pdata->temp_tab[i].y);
	}

}

static uint32_t init_flag = 0;
#define SPRDBAT_ONE_PERCENT_TIME   (40)
#define SPRDBAT_AVOID_JUMPING_TEMI  (SPRDBAT_ONE_PERCENT_TIME)

static unsigned long sprdbat_update_capacity_time;
static uint32_t update_capacity;

static unsigned long sprdbat_last_query_time;
struct sprd_battery_platform_data *bat_pdata;

bool sec_hal_fg_init(fuelgauge_variable_t * fg_var)
{
	struct sprd_battery_platform_data *pdata = (&get_battery_data(fg_var))->pdata ;
	bat_pdata = pdata;
	pr_info("register sec_hal_fg_init\n");
	print_pdata(pdata); //debug
	sprdfgu_init(pdata);
	{
		struct timespec cur_time;
		get_monotonic_boottime(&cur_time);
		sprdbat_update_capacity_time = cur_time.tv_sec;
		update_capacity = sprdfgu_poweron_capacity();
	}
	init_flag = 1;
	return true;
}

static uint32_t sprdbat_update_capacty(void)
{
	uint32_t fgu_capacity = sprdfgu_read_capacity();
	uint32_t flush_time = 0;
	uint32_t period_time = 0;
	struct timespec cur_time;
	union power_supply_propval value;

	psy_do_property("battery", get, POWER_SUPPLY_PROP_STATUS, value);

	get_monotonic_boottime(&cur_time);
	flush_time = cur_time.tv_sec - sprdbat_update_capacity_time;
	period_time = cur_time.tv_sec - sprdbat_last_query_time;
	sprdbat_last_query_time = cur_time.tv_sec;

	pr_info("fgu_capacity: = %d,flush_time: = %d,period_time:=%d\n",
		      fgu_capacity, flush_time, period_time);

	switch (value.intval) {
	case POWER_SUPPLY_STATUS_CHARGING:
		if (fgu_capacity < update_capacity) {
			if (sprdfgu_read_batcurrent() > 0) {
				pr_info("avoid vol jumping\n");
			fgu_capacity = update_capacity;
		} else {
				if (period_time <= SPRDBAT_AVOID_JUMPING_TEMI) {
					fgu_capacity =update_capacity - 1;
					pr_info("avoid jumping! fgu_capacity: = %d\n", fgu_capacity);
				}
				if ((update_capacity - fgu_capacity) >=
				    (flush_time / SPRDBAT_ONE_PERCENT_TIME)) {
					fgu_capacity =
					    update_capacity -
					    flush_time / SPRDBAT_ONE_PERCENT_TIME;
				}
			}
		} else if (fgu_capacity > update_capacity) {
			if (period_time <= SPRDBAT_AVOID_JUMPING_TEMI) {
				fgu_capacity =
					update_capacity + 1;
				pr_info("avoid  jumping! fgu_capacity: = %d\n",fgu_capacity);
			}
			if ((fgu_capacity - update_capacity) >=
			    (flush_time / SPRDBAT_ONE_PERCENT_TIME)) {
				fgu_capacity =
				    update_capacity +
				    flush_time / SPRDBAT_ONE_PERCENT_TIME;
			}
		}
		/*when soc=100 and adp plugin occur, keep on 100, */
		if (100 == update_capacity) {
			sprdbat_update_capacity_time = cur_time.tv_sec;
			fgu_capacity = 100;
		} 
#if 0		
		else {
			if (fgu_capacity >= 100) {
				fgu_capacity = 99;
			}
		}
#endif
		break;
	case POWER_SUPPLY_STATUS_NOT_CHARGING:
	case POWER_SUPPLY_STATUS_DISCHARGING:
		if (fgu_capacity >= update_capacity) {
			fgu_capacity = update_capacity;
		} else {
			if (period_time <= SPRDBAT_AVOID_JUMPING_TEMI) {
				fgu_capacity =
				    update_capacity - 1;
				pr_info
				    ("avoid jumping! fgu_capacity: = %d\n",
				     fgu_capacity);
			}
			if ((update_capacity - fgu_capacity) >=
			    (flush_time / SPRDBAT_ONE_PERCENT_TIME)) {
				fgu_capacity =
				    update_capacity -
				    flush_time / SPRDBAT_ONE_PERCENT_TIME;
			}
		}

		if (sprdfgu_read_vbat_vol() < bat_pdata->soft_vbat_uvlo) {
			pr_info("soft uvlo vol 0...\n");
			fgu_capacity = 0;
		}

		break;
	case POWER_SUPPLY_STATUS_FULL:
		sprdbat_update_capacity_time = cur_time.tv_sec;
		if (fgu_capacity != 100) {
			fgu_capacity = 100;
		}
		break;
	default:
		break;
	}

	if (fgu_capacity != update_capacity) {
		update_capacity = fgu_capacity;
		sprdbat_update_capacity_time = cur_time.tv_sec;
	}
	sprdfgu_record_cap(update_capacity);
	return update_capacity;

}

bool sec_hal_fg_reset(fuelgauge_variable_t * fg_var)
{
	msleep(1000);  //wait a moment,insure voltage REG of FG is updated after power supply voltage be changed
	sprdfgu_reset();
	{
		struct timespec cur_time;
		get_monotonic_boottime(&cur_time);
		sprdbat_update_capacity_time = cur_time.tv_sec; //time need to be updated,too.
		update_capacity = sprdfgu_poweron_capacity(); //same as your change
	}
	return true;
}
bool sec_hal_fg_suspend(fuelgauge_variable_t * fg_var)
{
	sprdfgu_pm_op(1);
	return true;
}

bool sec_hal_fg_resume(fuelgauge_variable_t * fg_var)
{
	sprdfgu_pm_op(0);
	return true;
}
bool sec_hal_fg_fuelalert_init(fuelgauge_variable_t * fg_var , int val)
{
	return true;
}
bool sec_hal_fg_is_fuelalerted(fuelgauge_variable_t * fg_var)
{
	return false;
}
bool sec_hal_fg_full_charged(fuelgauge_variable_t * fg_var)
{
	return true;
}

bool sec_hal_fg_fuelalert_process(void *irq_data, bool is_fuel_alerted)
{
	return true;
}
bool sec_hal_fg_get_property(fuelgauge_variable_t * fg_var,
			       enum power_supply_property psp,
			       union power_supply_propval *val)
{

	switch (psp) {

	/* Cell voltage (VCELL, mV) */
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		val->intval = sprdfgu_read_vbat_vol();
		break;

	/* Additional Voltage Information (mV) */
	case POWER_SUPPLY_PROP_VOLTAGE_AVG:
		switch (val->intval) {
		case SEC_BATTEY_VOLTAGE_AVERAGE:
			val->intval = sprdfgu_read_vbat_vol();
			break;
		case SEC_BATTEY_VOLTAGE_OCV:
			val->intval = sprdfgu_read_vbat_ocv();
			break;
		}
		break;

	/* Current (mA) */
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		val->intval = sprdfgu_read_batcurrent();
		break;

	/* Average Current (mA) */
	case POWER_SUPPLY_PROP_CURRENT_AVG:
		val->intval = sprdfgu_read_batcurrent();
		break;

	/* SOC (%) */
	case POWER_SUPPLY_PROP_CAPACITY:
		if (init_flag) {
#if 0
			int cap = sprdfgu_read_capacity();
			if (100 == cap) {
				union power_supply_propval value;
				psy_do_property("battery", get,
						POWER_SUPPLY_PROP_STATUS,
						value);
				if (POWER_SUPPLY_STATUS_CHARGING ==
				    value.intval) {
					pr_info
					    ("sec_hal_fg_get_property soc == 100, but is charging\n",
					     psp);
					cap = 99;
				}
			}
#else
			int cap = sprdbat_update_capacty();
#endif
			val->intval = cap * 10;
		} else {
			val->intval = 500;
		}
		break;

	/* Battery Temperature */
	case POWER_SUPPLY_PROP_TEMP:

	/* Target Temperature */
	case POWER_SUPPLY_PROP_TEMP_AMBIENT:
	default:
		return false;
	}
	pr_info("sec_hal_fg_get_property-%d,val->intval = %d\n", psp,
		val->intval);
	return true;
}

bool sec_hal_fg_set_property(fuelgauge_variable_t * fg_var,
			       enum power_supply_property psp,
			       const union power_supply_propval *val)
{
	switch (psp) {

	case POWER_SUPPLY_PROP_ONLINE:
		if (val->intval == POWER_SUPPLY_TYPE_BATTERY) {
			sprdfgu_adp_status_set(0);
		} else {
			sprdfgu_adp_status_set(1);
		}
		break;
	/* Battery Temperature */
	case POWER_SUPPLY_PROP_TEMP:

	/* Target Temperature */
	case POWER_SUPPLY_PROP_TEMP_AMBIENT:
		break;
	default:
		return false;
	}
	return true;
}

