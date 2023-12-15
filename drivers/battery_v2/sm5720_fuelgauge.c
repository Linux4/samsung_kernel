/*
 *  sm5720_fuelgauge.c
 *  Samsung SM5720 Fuel Gauge Driver
 *
 *  Copyright (C) 2015 Samsung Electronics
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

/* #define BATTERY_LOG_MESSAGE */

#include <linux/mfd/sm5720-private.h>
#include "include/fuelgauge/sm5720_fuelgauge.h"
#include <linux/of_gpio.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>

static enum power_supply_property sm5720_fuelgauge_props[] = {
    POWER_SUPPLY_PROP_STATUS,
    POWER_SUPPLY_PROP_VOLTAGE_NOW,
    POWER_SUPPLY_PROP_VOLTAGE_AVG,
    POWER_SUPPLY_PROP_CURRENT_NOW,
    POWER_SUPPLY_PROP_CURRENT_AVG,
    POWER_SUPPLY_PROP_CHARGE_FULL,
    POWER_SUPPLY_PROP_ENERGY_NOW,
    POWER_SUPPLY_PROP_CAPACITY,
    POWER_SUPPLY_PROP_TEMP,
    POWER_SUPPLY_PROP_TEMP_AMBIENT,
#if defined(CONFIG_EN_OOPS)
    POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN,
#endif
    POWER_SUPPLY_PROP_ENERGY_FULL,
    POWER_SUPPLY_PROP_ENERGY_FULL_DESIGN,
    POWER_SUPPLY_PROP_TIME_TO_FULL_NOW,
    POWER_SUPPLY_PROP_CHARGE_ENABLED,
#if defined(CONFIG_BATTERY_AGE_FORECAST)
    POWER_SUPPLY_PROP_CAPACITY_LEVEL,
#endif
};

#define MINVAL(a, b) ((a <= b) ? a : b)
#define MAXVAL(a, b) ((a > b) ? a : b)

#define LIMIT_N_CURR_MIXFACTOR -2000
#define TABLE_READ_COUNT 2
#define FG_ABNORMAL_RESET -1
#define IGNORE_N_I_OFFSET 1
//#define ENABLE_FULL_OFFSET 1
//#define SM5720_FG_FULL_DEBUG 1

static int sm5720_device_id = -1;

enum sm5720_battery_table_type {
	DISCHARGE_TABLE = 0,
	Q_TABLE,
	TABLE_MAX,
};

bool sm5720_fg_fuelalert_init(struct sm5720_fuelgauge_data *fuelgauge,
                int soc);

#if !defined(CONFIG_SEC_FACTORY)
static void sm5720_fg_read_time(struct sm5720_fuelgauge_data *fuelgauge)
{
    pr_info("%s: sm5720_fg_read_time\n", __func__);

    return;
}

static void sm5720_fg_test_print(struct sm5720_fuelgauge_data *fuelgauge)
{
    pr_info("%s: sm5720_fg_test_print\n", __func__);

    sm5720_fg_read_time(fuelgauge);
}

static void sm5720_fg_periodic_read(struct sm5720_fuelgauge_data *fuelgauge)
{
    u8 reg;
    int i;
    int data[0x10];
    char *str = NULL;

    str = kzalloc(sizeof(char)*2048, GFP_KERNEL);
    if (!str)
        return;

    for (i = 0; i <= 0xc; i++) {
        for (reg = 0; reg < 0x10; reg++) {
            data[reg] = sm5720_read_word(fuelgauge->i2c, reg + (i * 0x10));
            if (data[reg] < 0) {
                kfree(str);
                return;
            }
        }
        if (i == 7)
            continue;
        sprintf(str+strlen(str),
            "%04xh,%04xh,%04xh,%04xh,%04xh,%04xh,%04xh,%04xh,",
            data[0x00], data[0x01], data[0x02], data[0x03],
            data[0x04], data[0x05], data[0x06], data[0x07]);
        sprintf(str+strlen(str),
            "%04xh,%04xh,%04xh,%04xh,%04xh,%04xh,%04xh,%04xh,",
            data[0x08], data[0x09], data[0x0a], data[0x0b],
            data[0x0c], data[0x0d], data[0x0e], data[0x0f]);
    }

    pr_info("[FG_ALL] %s\n", str);

    kfree(str);
}
#endif

static bool sm5720_fg_check_reg_init_need(struct sm5720_fuelgauge_data *fuelgauge)
{
	int ret;

	ret = sm5720_read_word(fuelgauge->i2c, SM5720_FG_REG_FG_OP_STATUS);

	if((ret & INIT_CHECK_MASK) == DISABLE_RE_INIT)
	{
		pr_info("%s: SM5720_REG_FG_OP_STATUS : 0x%x , return FALSE NO init need\n", __func__, ret);
		return 0;
	}
	else
	{
		pr_info("%s: SM5720_REG_FG_OP_STATUS : 0x%x , return TRUE init need!!!!\n", __func__, ret);
		return 1;
	}
}

void sm5720_cal_avg_vbat(struct sm5720_fuelgauge_data *fuelgauge)
{
	if (fuelgauge->info.batt_avgvoltage == 0)
		fuelgauge->info.batt_avgvoltage = fuelgauge->info.batt_voltage;

	else if (fuelgauge->info.batt_voltage == 0 && fuelgauge->info.p_batt_voltage == 0)
		fuelgauge->info.batt_avgvoltage = 3400;

	else if(fuelgauge->info.batt_voltage == 0)
		fuelgauge->info.batt_avgvoltage =
				((fuelgauge->info.batt_avgvoltage) + (fuelgauge->info.p_batt_voltage))/2;

	else if(fuelgauge->info.p_batt_voltage == 0)
		fuelgauge->info.batt_avgvoltage =
				((fuelgauge->info.batt_avgvoltage) + (fuelgauge->info.batt_voltage))/2;

	else
		fuelgauge->info.batt_avgvoltage =
				((fuelgauge->info.batt_avgvoltage*2) +
				 (fuelgauge->info.p_batt_voltage+fuelgauge->info.batt_voltage))/4;

#ifdef SM5720_FG_FULL_DEBUG
	pr_info("%s: batt_avgvoltage = %d\n", __func__, fuelgauge->info.batt_avgvoltage);
#endif

	return;
}

void sm5720_voffset_cancel(struct sm5720_fuelgauge_data *fuelgauge)
{
	int volt_slope, mohm_volt_cal;
    int fg_temp_gap=0, volt_cal=0, fg_delta_volcal=0, pn_volt_slope=0, volt_offset=0;

	//set vbat offset cancel start
	volt_slope = sm5720_read_word(fuelgauge->i2c, SM5720_FG_REG_VOLT_CAL);
	volt_slope = volt_slope & 0xFF00;
	mohm_volt_cal = fuelgauge->info.volt_cal & 0x00FF;
	if(fuelgauge->info.enable_v_offset_cancel_p)
	{
		if(fuelgauge->is_charging && (fuelgauge->info.batt_current > fuelgauge->info.v_offset_cancel_level))
		{
			if(mohm_volt_cal & 0x0080)
			{
				mohm_volt_cal = -(mohm_volt_cal & 0x007F);
			}
			mohm_volt_cal = mohm_volt_cal - (fuelgauge->info.batt_current/(fuelgauge->info.v_offset_cancel_mohm * 13)); // ((curr*0.001)*0.006)*2048 -> 6mohm
			if(mohm_volt_cal < 0)
			{
				mohm_volt_cal = -mohm_volt_cal;
				mohm_volt_cal = mohm_volt_cal|0x0080;
			}
		}
	}
	if(fuelgauge->info.enable_v_offset_cancel_n)
	{
		if(!(fuelgauge->is_charging) && (fuelgauge->info.batt_current < -(fuelgauge->info.v_offset_cancel_level)))
		{
			if(fuelgauge->info.volt_cal & 0x0080)
			{
				mohm_volt_cal = -(mohm_volt_cal & 0x007F);
			}
			mohm_volt_cal = mohm_volt_cal - (fuelgauge->info.batt_current/(fuelgauge->info.v_offset_cancel_mohm * 13)); // ((curr*0.001)*0.006)*2048 -> 6mohm
			if(mohm_volt_cal < 0)
			{
				mohm_volt_cal = -mohm_volt_cal;
				mohm_volt_cal = mohm_volt_cal|0x0080;
			}
		}
	}
	sm5720_write_word(fuelgauge->i2c, SM5720_FG_REG_VOLT_CAL, (mohm_volt_cal | volt_slope));
	pr_info("%s: <%d %d %d %d> volt_cal = 0x%x, volt_slope = 0x%x, mohm_volt_cal = 0x%x\n",
			__func__, fuelgauge->info.enable_v_offset_cancel_p, fuelgauge->info.enable_v_offset_cancel_n
			, fuelgauge->info.v_offset_cancel_level, fuelgauge->info.v_offset_cancel_mohm
			, fuelgauge->info.volt_cal, volt_slope, mohm_volt_cal);
	//set vbat offset cancel end

	fg_temp_gap = (fuelgauge->info.temp_fg/10) - fuelgauge->info.temp_std;

	volt_cal = sm5720_read_word(fuelgauge->i2c, SM5720_FG_REG_VOLT_CAL);
	volt_offset = volt_cal & 0x00FF;
	pn_volt_slope = fuelgauge->info.volt_cal & 0xFF00;

	if (fuelgauge->info.en_fg_temp_volcal)
	{
		fg_delta_volcal = (fg_temp_gap / fuelgauge->info.fg_temp_volcal_denom)*fuelgauge->info.fg_temp_volcal_fact;
		pn_volt_slope = pn_volt_slope + (fg_delta_volcal<<8);
		volt_cal = pn_volt_slope | volt_offset;
		sm5720_write_word(fuelgauge->i2c, SM5720_FG_REG_VOLT_CAL, volt_cal);
	}

    return;
}

static unsigned int sm5720_get_vbat(struct sm5720_fuelgauge_data *fuelgauge)
{
	int ret;
	unsigned int vbat;/* = 3500; 3500 means 3500mV*/

    sm5720_voffset_cancel(fuelgauge);

	ret = sm5720_read_word(fuelgauge->i2c, SM5720_FG_REG_VOLTAGE);
	if (ret<0) {
		pr_err("%s: read vbat reg fail", __func__);
		vbat = 4000;
	} else {
		vbat = ((ret&0x3800)>>11) * 1000; //integer;
		vbat = vbat + (((ret&0x07ff)*1000)/2048); // integer + fractional
	}
	fuelgauge->info.batt_voltage = vbat;

#ifdef SM5720_FG_FULL_DEBUG
	pr_info("%s: read = 0x%x, vbat = %d\n", __func__, ret, vbat);
#endif
	sm5720_cal_avg_vbat(fuelgauge);

    if ((fuelgauge->sw_v_empty == VEMPTY_MODE) && vbat > 3480) {
        fuelgauge->sw_v_empty = VEMPTY_RECOVERY_MODE;
        sm5720_fg_fuelalert_init(fuelgauge,
                       fuelgauge->pdata->fuel_alert_soc);
        pr_info("%s : SW V EMPTY DISABLE\n", __func__);
    }

	return vbat;
}

static unsigned int sm5720_get_ocv(struct sm5720_fuelgauge_data *fuelgauge)
{
	int ret;
	unsigned int ocv;// = 3500; /*3500 means 3500mV*/

	ret = sm5720_read_word(fuelgauge->i2c, SM5720_FG_REG_OCV);
	if (ret<0) {
		pr_err("%s: read ocv reg fail\n", __func__);
		ocv = 4000;
	} else {
		ocv = ((ret&0x7800)>>11) * 1000; //integer;
		ocv = ocv + (((ret&0x07ff)*1000)/2048); // integer + fractional
	}

	fuelgauge->info.batt_ocv = ocv;
#ifdef SM5720_FG_FULL_DEBUG
	pr_info("%s: read = 0x%x, ocv = %d\n", __func__, ret, ocv);
#endif

	return ocv;
}

void sm5720_cal_avg_current(struct sm5720_fuelgauge_data *fuelgauge)
{
	if (fuelgauge->info.batt_avgcurrent == 0)
		fuelgauge->info.batt_avgcurrent = fuelgauge->info.batt_current;

	else if (fuelgauge->info.batt_avgcurrent == 0 && fuelgauge->info.p_batt_current == 0)
		fuelgauge->info.batt_avgcurrent = fuelgauge->info.batt_current;

	else if(fuelgauge->info.batt_current == 0)
		fuelgauge->info.batt_avgcurrent =
				((fuelgauge->info.batt_avgcurrent) + (fuelgauge->info.p_batt_current))/2;

	else if(fuelgauge->info.p_batt_current == 0)
		fuelgauge->info.batt_avgcurrent =
				((fuelgauge->info.batt_avgcurrent) + (fuelgauge->info.batt_current))/2;

	else
		fuelgauge->info.batt_avgcurrent =
				((fuelgauge->info.batt_avgcurrent*2) +
				 (fuelgauge->info.p_batt_current+fuelgauge->info.batt_current))/4;

#ifdef SM5720_FG_FULL_DEBUG
	pr_info("%s: batt_avgcurrent = %d\n", __func__, fuelgauge->info.batt_avgcurrent);
#endif

	return;
}

static int sm5720_get_curr(struct sm5720_fuelgauge_data *fuelgauge)
{
	int ret;
	int curr;/* = 1000; 1000 means 1000mA*/

	ret = sm5720_read_word(fuelgauge->i2c, SM5720_FG_REG_CURRENT);
	if (ret<0) {
		pr_err("%s: read curr reg fail", __func__);
		curr = 0;
	} else {
		curr = ((ret&0x1800)>>11) * 1000; //integer;
		curr = curr + (((ret&0x07ff)*1000)/2048); // integer + fractional

		if(ret&0x8000) {
			curr *= -1;
		}
	}
	fuelgauge->info.batt_current = curr;

#ifdef SM5720_FG_FULL_DEBUG
		pr_info("%s: read = 0x%x, curr = %d\n", __func__, ret, curr);
#endif

	sm5720_cal_avg_current(fuelgauge);

	return curr;
}

static int sm5720_get_temperature(struct sm5720_fuelgauge_data *fuelgauge)
{
	int ret;
	int temp;/* = 250; 250 means 25.0oC*/

	ret = sm5720_read_word(fuelgauge->i2c, SM5720_FG_REG_TEMPERATURE);
	if (ret<0) {
		pr_err("%s: read temp reg fail", __func__);
		temp = 0;
	} else {
		temp = ((ret&0x7F00)>>8) * 10; //integer bit
		temp = temp + (((ret&0x00f0)*10)/256); // integer + fractional bit
		if(ret&0x8000) {
			temp *= -1;
		}
	}
	fuelgauge->info.temp_fg = temp;

#ifdef SM5720_FG_FULL_DEBUG
	pr_info("%s: read = 0x%x, temp_fg = %d\n", __func__, ret, temp);
#endif

	return temp;
}

static int sm5720_get_cycle(struct sm5720_fuelgauge_data *fuelgauge)
{
	int ret;
	int cycle;

	ret = sm5720_read_word(fuelgauge->i2c, SM5720_FG_REG_CYCLE);
	if (ret<0) {
		pr_err("%s: read cycle reg fail", __func__);
		cycle = 0;
	} else {
		cycle = ret&0x03FF;
	}
	fuelgauge->info.batt_soc_cycle = cycle;

#ifdef SM5720_FG_FULL_DEBUG
	pr_info("%s: read = 0x%x, soc_cycle = %d\n", __func__, ret, cycle);
#endif

	return cycle;
}

static void sm5720_vbatocv_check(struct sm5720_fuelgauge_data *fuelgauge)
{
	int ret;

	if((abs(fuelgauge->info.batt_current)<40) ||
	   ((fuelgauge->is_charging) && (fuelgauge->info.batt_current<(fuelgauge->info.top_off)) &&
	   (fuelgauge->info.batt_current>(fuelgauge->info.top_off/3))))
	{
		if(abs(fuelgauge->info.batt_ocv-fuelgauge->info.batt_voltage)>30) // 30mV over
		{
			fuelgauge->info.iocv_error_count ++;
		}

		pr_info("%s: sm5720 FG iocv_error_count (%d)\n", __func__, fuelgauge->info.iocv_error_count);

		if(fuelgauge->info.iocv_error_count > 5) // prevent to overflow
			fuelgauge->info.iocv_error_count = 6;
	}
	else
	{
		fuelgauge->info.iocv_error_count = 0;
	}

	if(fuelgauge->info.iocv_error_count > 5)
	{
		pr_info("%s: p_v - v = (%d)\n", __func__, fuelgauge->info.p_batt_voltage - fuelgauge->info.batt_voltage);
		if(abs(fuelgauge->info.p_batt_voltage - fuelgauge->info.batt_voltage)>15) // 15mV over
		{
			fuelgauge->info.iocv_error_count = 0;
		}
		else
		{
			// mode change to mix RS manual mode
			pr_info("%s: mode change to mix RS manual mode\n", __func__);
			// run update set
			sm5720_write_word(fuelgauge->i2c, SM5720_FG_REG_PARAM_RUN_UPDATE, 1);
			// RS manual value write
			sm5720_write_word(fuelgauge->i2c, SM5720_FG_REG_RS_MAN, fuelgauge->info.rs_value[0]);
			// run update set
			sm5720_write_word(fuelgauge->i2c, SM5720_FG_REG_PARAM_RUN_UPDATE, 0);
			// mode change
			ret = sm5720_read_word(fuelgauge->i2c, SM5720_FG_REG_CNTL);
			ret = (ret | ENABLE_MIX_MODE) | ENABLE_RS_MAN_MODE; // +RS_MAN_MODE
			sm5720_write_word(fuelgauge->i2c, SM5720_FG_REG_CNTL, ret);
		}
	}
	else
	{
		if((fuelgauge->info.temperature/10) > 15)
		{
			if((fuelgauge->info.p_batt_voltage < fuelgauge->info.n_tem_poff) &&
				(fuelgauge->info.batt_voltage < fuelgauge->info.n_tem_poff) && (!fuelgauge->is_charging))
			{
				pr_info("%s: mode change to normal tem mix RS manual mode\n", __func__);
				// mode change to mix RS manual mode
				// run update init
				sm5720_write_word(fuelgauge->i2c, SM5720_FG_REG_PARAM_RUN_UPDATE, 1);
				// RS manual value write
				if((fuelgauge->info.p_batt_voltage <
					(fuelgauge->info.n_tem_poff - fuelgauge->info.n_tem_poff_offset)) &&
					(fuelgauge->info.batt_voltage <
					(fuelgauge->info.n_tem_poff - fuelgauge->info.n_tem_poff_offset)))
				{
					sm5720_write_word(fuelgauge->i2c, SM5720_FG_REG_RS_MAN, fuelgauge->info.rs_value[0]>>1);
				}
				else
				{
					sm5720_write_word(fuelgauge->i2c, SM5720_FG_REG_RS_MAN, fuelgauge->info.rs_value[0]);
				}
				// run update set
				sm5720_write_word(fuelgauge->i2c, SM5720_FG_REG_PARAM_RUN_UPDATE, 0);

				// mode change
				ret = sm5720_read_word(fuelgauge->i2c, SM5720_FG_REG_CNTL);
				ret = (ret | ENABLE_MIX_MODE) | ENABLE_RS_MAN_MODE; // +RS_MAN_MODE
				sm5720_write_word(fuelgauge->i2c, SM5720_FG_REG_CNTL, ret);
			}
			else
			{
				pr_info("%s: mode change to mix RS auto mode\n", __func__);

				// mode change to mix RS auto mode
				ret = sm5720_read_word(fuelgauge->i2c, SM5720_FG_REG_CNTL);
				ret = (ret | ENABLE_MIX_MODE) & ~ENABLE_RS_MAN_MODE; // -RS_MAN_MODE
				sm5720_write_word(fuelgauge->i2c, SM5720_FG_REG_CNTL, ret);
			}
		}
		else
		{
			if((fuelgauge->info.p_batt_voltage < fuelgauge->info.l_tem_poff) &&
				(fuelgauge->info.batt_voltage < fuelgauge->info.l_tem_poff) && (!fuelgauge->is_charging))
			{
				pr_info("%s: mode change to normal tem mix RS manual mode\n", __func__);
				// mode change to mix RS manual mode
				// run update init
				sm5720_write_word(fuelgauge->i2c, SM5720_FG_REG_PARAM_RUN_UPDATE, 1);
				// RS manual value write
				if((fuelgauge->info.p_batt_voltage <
					(fuelgauge->info.l_tem_poff - fuelgauge->info.l_tem_poff_offset)) &&
					(fuelgauge->info.batt_voltage <
					(fuelgauge->info.l_tem_poff - fuelgauge->info.l_tem_poff_offset)))
				{
					sm5720_write_word(fuelgauge->i2c, SM5720_FG_REG_RS_MAN, fuelgauge->info.rs_value[0]>>1);
				}
				else
				{
					sm5720_write_word(fuelgauge->i2c, SM5720_FG_REG_RS_MAN, fuelgauge->info.rs_value[0]);
				}
				// run update set
				sm5720_write_word(fuelgauge->i2c, SM5720_FG_REG_PARAM_RUN_UPDATE, 0);

				// mode change
				ret = sm5720_read_word(fuelgauge->i2c, SM5720_FG_REG_CNTL);
				ret = (ret | ENABLE_MIX_MODE) | ENABLE_RS_MAN_MODE; // +RS_MAN_MODE
				sm5720_write_word(fuelgauge->i2c, SM5720_FG_REG_CNTL, ret);
			}
			else
			{
				pr_info("%s: mode change to mix RS auto mode\n", __func__);

				// mode change to mix RS auto mode
				ret = sm5720_read_word(fuelgauge->i2c, SM5720_FG_REG_CNTL);
				ret = (ret | ENABLE_MIX_MODE) & ~ENABLE_RS_MAN_MODE; // -RS_MAN_MODE
				sm5720_write_word(fuelgauge->i2c, SM5720_FG_REG_CNTL, ret);
			}
		}
	}
	fuelgauge->info.p_batt_voltage = fuelgauge->info.batt_voltage;
	fuelgauge->info.p_batt_current = fuelgauge->info.batt_current;
	// iocv error case cover end
}

#ifdef ENABLE_FULL_OFFSET
void sm5720_adabt_full_offset(struct sm5720_fuelgauge_data *fuelgauge)
{
	int fg_temp_gap;
	int full_offset, i_offset, sign_offset, curr;
	int curr_off, sign_origin, i_origin;
	int curr, sign_curr, i_curr;

#ifdef SM5720_FG_FULL_DEBUG
	pr_info("%s: flag_charge_health=%d, flag_full_charge=%d\n", __func__, fuelgauge->info.flag_charge_health, fuelgauge->info.flag_full_charge);
#endif
	if(fuelgauge->info.flag_charge_health && fuelgauge->info.flag_full_charge)
	{
		fg_temp_gap = (fuelgauge->info.temp_fg/10) - fuelgauge->info.temp_std;
		if(abs(fg_temp_gap) < 10)
		{
			curr = sm5720_read_word(fuelgauge->i2c, SM5720_FG_REG_CURRENT);
			sign_curr = curr & 0x8000;
			i_curr = (curr & 0x7FFF)>>1;
			if(sign_curr == 1)
			{
				i_curr = -i_curr;
			}

			curr_off = sm5720_read_word(fuelgauge->i2c, SM5720_FG_REG_ECV_I_OFF);
			sign_origin = curr_off & 0x0080;
			i_origin = curr_off & 0x007F;
			if(sign_origin == 1)
			{
				i_origin = -i_origin;
			}

			full_offset = i_origin - i_curr;
			if(full_offset < 0)
			{
				i_offset = -full_offset;
				sign_offset = 1;
			}
			else
			{
				i_offset = full_offset;
				sign_offset = 0;
			}

			pr_info("%s: curr=%x, curr_off=%x, i_offset=%x, sign_offset=%d, full_offset_margin=%x, full_extra_offset=%x\n", __func__, curr, curr_off, i_offset, sign_offset, fuelgauge->info.full_offset_margin, fuelgauge->info.full_extra_offset);
			if(i_offset < ((fuelgauge->info.full_offset_margin<<10)/1000))
			{
				if(sign_offset == 1)
				{
					i_offset = -i_offset;
				}

				i_offset = i_offset + ((fuelgauge->info.full_extra_offset<<10)/1000);

				if(i_offset <= 0)
				{
					full_offset = -i_offset;
				}
				else
				{
					full_offset = i_offset|0x0080;
				}
				fuelgauge->info.curr_offset = full_offset;
				sm5720_write_word(fuelgauge->i2c, SM5720_FG_REG_ECV_I_OFF, full_offset);
				pr_info("%s: LAST i_offset=%x, sign_offset=%x, full_offset=%x\n", __func__, i_offset, sign_offset, full_offset);
			}

		}

	}

	return;
}
#endif

static void sm5720_dp_setup (struct sm5720_fuelgauge_data *fuelgauge)
{
    sm5720_write_word(fuelgauge->i2c, SM5720_FG_REG_DP_ECV_I_OFF, fuelgauge->info.dp_ecv_i_off);
    sm5720_write_word(fuelgauge->i2c, SM5720_FG_REG_DP_CSP_I_OFF, fuelgauge->info.dp_csp_i_off);
    sm5720_write_word(fuelgauge->i2c, SM5720_FG_REG_DP_CSN_I_OFF, fuelgauge->info.dp_csn_i_off);

    sm5720_write_word(fuelgauge->i2c, SM5720_FG_REG_DP_ECV_I_SLO, fuelgauge->info.dp_ecv_i_slo);
    sm5720_write_word(fuelgauge->i2c, SM5720_FG_REG_DP_CSP_I_SLO, fuelgauge->info.dp_csp_i_slo);
    sm5720_write_word(fuelgauge->i2c, SM5720_FG_REG_DP_CSN_I_SLO, fuelgauge->info.dp_csn_i_slo);

    pr_info("%s: dp_off : <0x%x 0x%x 0x%x> dp_slo : <0x%x 0x%x 0x%x>\n", 
		__func__,
        fuelgauge->info.dp_ecv_i_off, fuelgauge->info.dp_csp_i_off, fuelgauge->info.dp_csn_i_off, 
        fuelgauge->info.dp_ecv_i_slo, fuelgauge->info.dp_csp_i_slo, fuelgauge->info.dp_csn_i_slo);
}

static void sm5720_alg_setup (struct sm5720_fuelgauge_data *fuelgauge)
{
    sm5720_write_word(fuelgauge->i2c, SM5720_FG_REG_ECV_I_OFF, fuelgauge->info.ecv_i_off);
    sm5720_write_word(fuelgauge->i2c, SM5720_FG_REG_CSP_I_OFF, fuelgauge->info.csp_i_off);
    sm5720_write_word(fuelgauge->i2c, SM5720_FG_REG_CSN_I_OFF, fuelgauge->info.csn_i_off);

    sm5720_write_word(fuelgauge->i2c, SM5720_FG_REG_ECV_I_SLO, fuelgauge->info.ecv_i_slo);
    sm5720_write_word(fuelgauge->i2c, SM5720_FG_REG_CSP_I_SLO, fuelgauge->info.csp_i_slo);
    sm5720_write_word(fuelgauge->i2c, SM5720_FG_REG_CSN_I_SLO, fuelgauge->info.csn_i_slo);

    pr_info("%s: alg_off : <0x%x 0x%x 0x%x> alg_slo : <0x%x 0x%x 0x%x>\n", 
		__func__,
        fuelgauge->info.ecv_i_off, fuelgauge->info.csp_i_off, fuelgauge->info.csn_i_off, 
        fuelgauge->info.ecv_i_slo, fuelgauge->info.csp_i_slo, fuelgauge->info.csn_i_slo);
}

static void sm5720_cal_carc (struct sm5720_fuelgauge_data *fuelgauge)
{
	int curr_cal = 0, p_curr_cal=0, n_curr_cal=0, p_delta_cal=0, n_delta_cal=0, p_fg_delta_cal=0, n_fg_delta_cal=0, temp_curr_offset=0;
	int temp_gap, fg_temp_gap, mix_factor=0;

	sm5720_vbatocv_check(fuelgauge);
#ifdef ENABLE_FULL_OFFSET
	sm5720_adabt_full_offset(fuelgauge);
#endif

	if(fuelgauge->is_charging || (fuelgauge->info.batt_current < LIMIT_N_CURR_MIXFACTOR))
	{
		mix_factor = fuelgauge->info.rs_value[1];
	}
	else
	{
		mix_factor = fuelgauge->info.rs_value[2];
	}
	sm5720_write_word(fuelgauge->i2c, SM5720_FG_REG_RS_MIX_FACTOR, mix_factor);

	fg_temp_gap = (fuelgauge->info.temp_fg/10) - fuelgauge->info.temp_std;


	temp_curr_offset = fuelgauge->info.ecv_i_off;
	if(fuelgauge->info.en_high_fg_temp_offset && (fg_temp_gap > 0))
	{
		if(temp_curr_offset & 0x0080)
		{
			temp_curr_offset = -(temp_curr_offset & 0x007F);
		}
		temp_curr_offset = temp_curr_offset + (fg_temp_gap / fuelgauge->info.high_fg_temp_offset_denom)*fuelgauge->info.high_fg_temp_offset_fact;
		if(temp_curr_offset < 0)
		{
			temp_curr_offset = -temp_curr_offset;
			temp_curr_offset = temp_curr_offset|0x0080;
		}
	}
	else if (fuelgauge->info.en_low_fg_temp_offset && (fg_temp_gap < 0))
	{
		if(temp_curr_offset & 0x0080)
		{
			temp_curr_offset = -(temp_curr_offset & 0x007F);
		}
		temp_curr_offset = temp_curr_offset + ((-fg_temp_gap) / fuelgauge->info.low_fg_temp_offset_denom)*fuelgauge->info.low_fg_temp_offset_fact;
		if(temp_curr_offset < 0)
		{
			temp_curr_offset = -temp_curr_offset;
			temp_curr_offset = temp_curr_offset|0x0080;
		}
	}
    temp_curr_offset = temp_curr_offset | (temp_curr_offset<<8);
	sm5720_write_word(fuelgauge->i2c, SM5720_FG_REG_ECV_I_OFF, temp_curr_offset);


	n_curr_cal = (fuelgauge->info.ecv_i_slo & 0xFF00)>>8;
	p_curr_cal = (fuelgauge->info.ecv_i_slo & 0x00FF);

	if (fuelgauge->info.en_high_fg_temp_cal && (fg_temp_gap > 0))
	{
		p_fg_delta_cal = (fg_temp_gap / fuelgauge->info.high_fg_temp_p_cal_denom)*fuelgauge->info.high_fg_temp_p_cal_fact;
		n_fg_delta_cal = (fg_temp_gap / fuelgauge->info.high_fg_temp_n_cal_denom)*fuelgauge->info.high_fg_temp_n_cal_fact;
	}
	else if (fuelgauge->info.en_low_fg_temp_cal && (fg_temp_gap < 0))
	{
		fg_temp_gap = -fg_temp_gap;
		p_fg_delta_cal = (fg_temp_gap / fuelgauge->info.low_fg_temp_p_cal_denom)*fuelgauge->info.low_fg_temp_p_cal_fact;
		n_fg_delta_cal = (fg_temp_gap / fuelgauge->info.low_fg_temp_n_cal_denom)*fuelgauge->info.low_fg_temp_n_cal_fact;
	}
	p_curr_cal = p_curr_cal + (p_fg_delta_cal);
	n_curr_cal = n_curr_cal + (n_fg_delta_cal);

	pr_info("%s: <%d %d %d %d %d %d %d %d %d %d>, temp_fg = %d ,p_curr_cal = 0x%x, n_curr_cal = 0x%x, "
		"batt_temp = %d\n",
		__func__, 
		fuelgauge->info.en_high_fg_temp_cal,
		fuelgauge->info.high_fg_temp_p_cal_denom, fuelgauge->info.high_fg_temp_p_cal_fact, 
		fuelgauge->info.high_fg_temp_n_cal_denom, fuelgauge->info.high_fg_temp_n_cal_fact,
		fuelgauge->info.en_low_fg_temp_cal, 
		fuelgauge->info.low_fg_temp_p_cal_denom, fuelgauge->info.low_fg_temp_p_cal_fact, 
		fuelgauge->info.low_fg_temp_n_cal_denom, fuelgauge->info.low_fg_temp_n_cal_fact,
		fuelgauge->info.temp_fg, p_curr_cal, n_curr_cal, fuelgauge->info.temperature);

	temp_gap = (fuelgauge->info.temperature/10) - fuelgauge->info.temp_std;
	if (fuelgauge->info.en_high_temp_cal && (temp_gap > 0))
	{
		p_delta_cal = (temp_gap / fuelgauge->info.high_temp_p_cal_denom)*fuelgauge->info.high_temp_p_cal_fact;
		n_delta_cal = (temp_gap / fuelgauge->info.high_temp_n_cal_denom)*fuelgauge->info.high_temp_n_cal_fact;
	}
	else if (fuelgauge->info.en_low_temp_cal && (temp_gap < 0))
	{
		temp_gap = -temp_gap;
		p_delta_cal = (temp_gap / fuelgauge->info.low_temp_p_cal_denom)*fuelgauge->info.low_temp_p_cal_fact;
		n_delta_cal = (temp_gap / fuelgauge->info.low_temp_n_cal_denom)*fuelgauge->info.low_temp_n_cal_fact;
	}
	p_curr_cal = p_curr_cal + (p_delta_cal);
	n_curr_cal = n_curr_cal + (n_delta_cal);

    curr_cal = (n_curr_cal << 8) | p_curr_cal;

	sm5720_write_word(fuelgauge->i2c, SM5720_FG_REG_ECV_I_SLO, curr_cal);

	pr_info("%s: <%d %d %d %d %d %d %d %d %d %d>, "
		"p_curr_cal = 0x%x, n_curr_cal = 0x%x, mix_factor=0x%x ,curr_cal = 0x%x\n",
		__func__,
		fuelgauge->info.en_high_temp_cal,
		fuelgauge->info.high_temp_p_cal_denom, fuelgauge->info.high_temp_p_cal_fact, 
		fuelgauge->info.high_temp_n_cal_denom, fuelgauge->info.high_temp_n_cal_fact,
		fuelgauge->info.en_low_temp_cal, 
		fuelgauge->info.low_temp_p_cal_denom, fuelgauge->info.low_temp_p_cal_fact, 
		fuelgauge->info.low_temp_n_cal_denom, fuelgauge->info.low_temp_n_cal_fact,
		p_curr_cal, n_curr_cal, mix_factor, curr_cal);

	return;
}

static int sm5720_fg_verified_write_word(struct i2c_client *client,
		u8 reg_addr, u16 data)
{
	int ret;

	ret = sm5720_write_word(client, reg_addr, data);
	if(ret<0)
	{
		msleep(50);
		pr_info("1st fail i2c write %s: ret = %d, addr = 0x%x, data = 0x%x\n",
				__func__, ret, reg_addr, data);
		ret = sm5720_write_word(client, reg_addr, data);
		if(ret<0)
		{
			msleep(50);
			pr_info("2nd fail i2c write %s: ret = %d, addr = 0x%x, data = 0x%x\n",
					__func__, ret, reg_addr, data);
			ret = sm5720_write_word(client, reg_addr, data);
			if(ret<0)
			{
				pr_info("3rd fail i2c write %s: ret = %d, addr = 0x%x, data = 0x%x\n",
						__func__, ret, reg_addr, data);
			}
		}
	}

	return ret;
}

static int sm5720_fg_fs_read_word_table(struct i2c_client *client,
		u8 reg_addr, u8 count)
{
	int ret, i;

    for(i=0; i<count; i++)
    {
        ret = sm5720_read_word(client, reg_addr);
        if(ret<0)
        {
            pr_err("%s: 1st fail i2c write ret = %d, addr = 0x%x\n, count = %d",
                __func__, ret, reg_addr, count);
        }
        else
        {
            if(ret == 0xffff)
                ret = sm5720_read_word(client, reg_addr);
            else
                break;
        }
    }
	return ret;
}



int sm5720_fg_calculate_iocv(struct sm5720_fuelgauge_data *fuelgauge)
{
	bool only_lb=false, sign_i_offset=0; //valid_cb=false, 
	int roop_start=0, roop_max=0, i=0, cb_last_index = 0, cb_pre_last_index =0;
	int lb_v_buffer[6] = {0, 0, 0, 0, 0, 0};
	int lb_i_buffer[6] = {0, 0, 0, 0, 0, 0};
	int cb_v_buffer[6] = {0, 0, 0, 0, 0, 0};
	int cb_i_buffer[6] = {0, 0, 0, 0, 0, 0};
	int i_offset_margin = 0x14, i_vset_margin = 0x67;
	int v_max=0, v_min=0, v_sum=0, lb_v_avg=0, cb_v_avg=0, lb_v_set=0, lb_i_set=0, i_offset=0; //lb_v_minmax_offset=0, 
	int i_max=0, i_min=0, i_sum=0, lb_i_avg=0, cb_i_avg=0, cb_v_set=0, cb_i_set=0; //lb_i_minmax_offset=0, 
	int lb_i_p_v_min=0, lb_i_n_v_max=0, cb_i_p_v_min=0, cb_i_n_v_max=0;

	int v_ret=0, i_ret=0, ret=0;

	ret = sm5720_read_word(fuelgauge->i2c, SM5720_FG_REG_END_V_IDX);
	pr_info("%s: iocv_status_read = addr : 0x%x , data : 0x%x\n", __func__, SM5720_FG_REG_END_V_IDX, ret);

	// init start
	if((ret & 0x0010) == 0x0000)
	{
		only_lb = true;
	}

    /*
	if((ret & 0x0300) == 0x0300)
	{
		valid_cb = true;
	}
	*/
	// init end

	// lb get start
	roop_max = (ret & 0x000F);
	if(roop_max > 6)
		roop_max = 6;

	roop_start = SM5720_FG_REG_START_LB_V;
	for (i = roop_start; i < roop_start + roop_max; i++)
	{
		v_ret = sm5720_read_word(fuelgauge->i2c, i);
		i_ret = sm5720_read_word(fuelgauge->i2c, i+0x10);

		if((i_ret&0x4000) == 0x4000)
		{
			i_ret = -(i_ret&0x3FFF);
		}

		lb_v_buffer[i-roop_start] = v_ret;
		lb_i_buffer[i-roop_start] = i_ret;

		if (i == roop_start)
		{
			v_max = v_ret;
			v_min = v_ret;
			v_sum = v_ret;
			i_max = i_ret;
			i_min = i_ret;
			i_sum = i_ret;
		}
		else
		{
			if(v_ret > v_max)
				v_max = v_ret;
			else if(v_ret < v_min)
				v_min = v_ret;
			v_sum = v_sum + v_ret;

			if(i_ret > i_max)
				i_max = i_ret;
			else if(i_ret < i_min)
				i_min = i_ret;
			i_sum = i_sum + i_ret;
		}

		if(abs(i_ret) > i_vset_margin)
		{
			if(i_ret > 0)
			{
				if(lb_i_p_v_min == 0)
				{
					lb_i_p_v_min = v_ret;
				}
				else
				{
					if(v_ret < lb_i_p_v_min)
						lb_i_p_v_min = v_ret;
				}
			}
			else
			{
				if(lb_i_n_v_max == 0)
				{
					lb_i_n_v_max = v_ret;
				}
				else
				{
					if(v_ret > lb_i_n_v_max)
						lb_i_n_v_max = v_ret;
				}
			}
		}
	}
	v_sum = v_sum - v_max - v_min;
	i_sum = i_sum - i_max - i_min;

    /*
	lb_v_minmax_offset = v_max - v_min;
	lb_i_minmax_offset = i_max - i_min;
	*/

	lb_v_avg = v_sum / (roop_max-2);
	lb_i_avg = i_sum / (roop_max-2);
	// lb get end

	// lb_vset start
	if(abs(lb_i_buffer[roop_max-1]) < i_vset_margin)
	{
		if(abs(lb_i_buffer[roop_max-2]) < i_vset_margin)
		{
			lb_v_set = MAXVAL(lb_v_buffer[roop_max-2], lb_v_buffer[roop_max-1]);
			if(abs(lb_i_buffer[roop_max-3]) < i_vset_margin)
			{
				lb_v_set = MAXVAL(lb_v_buffer[roop_max-3], lb_v_set);
			}
		}
		else
		{
			lb_v_set = lb_v_buffer[roop_max-1];
		}
	}
	else
	{
		lb_v_set = lb_v_avg;
	}

	if(lb_i_n_v_max > 0)
	{
		lb_v_set = MAXVAL(lb_i_n_v_max, lb_v_set);
	}
	//else if(lb_i_p_v_min > 0)
	//{
	//	lb_v_set = MINVAL(lb_i_p_v_min, lb_v_set);
	//}
	// lb_vset end

	// lb offset make start
	if(roop_max > 3)
	{
		lb_i_set = (lb_i_buffer[2] + lb_i_buffer[3]) / 2;
	}

	if((abs(lb_i_buffer[roop_max-1]) < i_offset_margin) && (abs(lb_i_set) < i_offset_margin))
	{
		lb_i_set = MAXVAL(lb_i_buffer[roop_max-1], lb_i_set);
	}
	else if(abs(lb_i_buffer[roop_max-1]) < i_offset_margin)
	{
		lb_i_set = lb_i_buffer[roop_max-1];
	}
	else if(abs(lb_i_set) < i_offset_margin)
	{
		lb_i_set = lb_i_set;
	}
	else
	{
		lb_i_set = 0;
	}

	i_offset = lb_i_set;

	i_offset = i_offset + 4;	// add extra offset

	if(i_offset <= 0)
	{
		sign_i_offset = 1;
#ifdef IGNORE_N_I_OFFSET
		i_offset = 0;
#else
		i_offset = -i_offset;
#endif
	}

	i_offset = i_offset>>1;

	if(sign_i_offset == 0)
	{
		i_offset = i_offset|0x0080;
	}
    i_offset = i_offset | i_offset<<8;

	//do not write in kernel point.
	//sm5720_write_word(client, SM5720_FG_REG_DP_ECV_I_OFF, i_offset);
	// lb offset make end

	pr_info("%s: iocv_l_max=0x%x, iocv_l_min=0x%x, iocv_l_avg=0x%x, lb_v_set=0x%x, roop_max=%d \n",
			__func__, v_max, v_min, lb_v_avg, lb_v_set, roop_max);
	pr_info("%s: ioci_l_max=0x%x, ioci_l_min=0x%x, ioci_l_avg=0x%x, lb_i_set=0x%x, i_offset=0x%x, sign_i_offset=%d\n",
			__func__, i_max, i_min, lb_i_avg, lb_i_set, i_offset, sign_i_offset);

	if(!only_lb)
	{
		// cb get start
		roop_start = SM5720_FG_REG_START_CB_V;
		roop_max = 6;
		for (i = roop_start; i < roop_start + roop_max; i++)
		{
			v_ret = sm5720_read_word(fuelgauge->i2c, i);
			i_ret = sm5720_read_word(fuelgauge->i2c, i+0x10);
			if((i_ret&0x4000) == 0x4000)
			{
				i_ret = -(i_ret&0x3FFF);
			}

			cb_v_buffer[i-roop_start] = v_ret;
			cb_i_buffer[i-roop_start] = i_ret;

			if (i == roop_start)
			{
				v_max = v_ret;
				v_min = v_ret;
				v_sum = v_ret;
				i_max = i_ret;
				i_min = i_ret;
				i_sum = i_ret;
			}
			else
			{
				if(v_ret > v_max)
					v_max = v_ret;
				else if(v_ret < v_min)
					v_min = v_ret;
				v_sum = v_sum + v_ret;

				if(i_ret > i_max)
					i_max = i_ret;
				else if(i_ret < i_min)
					i_min = i_ret;
				i_sum = i_sum + i_ret;
			}

			if(abs(i_ret) > i_vset_margin)
			{
				if(i_ret > 0)
				{
					if(cb_i_p_v_min == 0)
					{
						cb_i_p_v_min = v_ret;
					}
					else
					{
						if(v_ret < cb_i_p_v_min)
							cb_i_p_v_min = v_ret;
					}
				}
				else
				{
					if(cb_i_n_v_max == 0)
					{
						cb_i_n_v_max = v_ret;
					}
					else
					{
						if(v_ret > cb_i_n_v_max)
							cb_i_n_v_max = v_ret;
					}
				}
			}
		}
		v_sum = v_sum - v_max - v_min;
		i_sum = i_sum - i_max - i_min;

		cb_v_avg = v_sum / (roop_max-2);
		cb_i_avg = i_sum / (roop_max-2);
		// cb get end

		// cb_vset start
		cb_last_index = (ret & 0x000F)-7; //-6-1
		if(cb_last_index < 0)
		{
			cb_last_index = 5;
		}

		for (i = roop_max; i > 0; i--)
		{
			if(abs(cb_i_buffer[cb_last_index]) < i_vset_margin)
			{
				cb_v_set = cb_v_buffer[cb_last_index];
				if(abs(cb_i_buffer[cb_last_index]) < i_offset_margin)
				{
					cb_i_set = cb_i_buffer[cb_last_index];
				}

				cb_pre_last_index = cb_last_index - 1;
				if(cb_pre_last_index < 0)
				{
					cb_pre_last_index = 5;
				}

				if(abs(cb_i_buffer[cb_pre_last_index]) < i_vset_margin)
				{
					cb_v_set = MAXVAL(cb_v_buffer[cb_pre_last_index], cb_v_set);
					if(abs(cb_i_buffer[cb_pre_last_index]) < i_offset_margin)
					{
						cb_i_set = MAXVAL(cb_i_buffer[cb_pre_last_index], cb_i_set);
					}
				}
			}
			else
			{
				cb_last_index--;
				if(cb_last_index < 0)
				{
					cb_last_index = 5;
				}
			}
		}

		if(cb_v_set == 0)
		{
			cb_v_set = cb_v_avg;
			if(cb_i_set == 0)
			{
				cb_i_set = cb_i_avg;
			}
		}

		if(cb_i_n_v_max > 0)
		{
			cb_v_set = MAXVAL(cb_i_n_v_max, cb_v_set);
		}
		//else if(cb_i_p_v_min > 0)
		//{
		//	cb_v_set = MINVAL(cb_i_p_v_min, cb_v_set);
		//}
		// cb_vset end

		// cb offset make start
		if(abs(cb_i_set) < i_offset_margin)
		{
			if(cb_i_set > lb_i_set)
			{
				i_offset = cb_i_set;
				i_offset = i_offset + 4;	// add extra offset

				if(i_offset <= 0)
				{
					sign_i_offset = 1;
#ifdef IGNORE_N_I_OFFSET
					i_offset = 0;
#else
					i_offset = -i_offset;
#endif
				}

				i_offset = i_offset>>1;

				if(sign_i_offset == 0)
				{
					i_offset = i_offset|0x0080;
				}
                i_offset = i_offset | i_offset<<8;

				//do not write in kernel point.
				//sm5720_write_word(fuelgauge->i2c, SM5720_FG_REG_DP_ECV_I_OFF, i_offset);
			}
		}
		// cb offset make end

		pr_info("%s: iocv_c_max=0x%x, iocv_c_min=0x%x, iocv_c_avg=0x%x, cb_v_set=0x%x, cb_last_index=%d \n",
				__func__, v_max, v_min, cb_v_avg, cb_v_set, cb_last_index);
		pr_info("%s: ioci_c_max=0x%x, ioci_c_min=0x%x, ioci_c_avg=0x%x, cb_i_set=0x%x, i_offset=0x%x, sign_i_offset=%d\n",
				__func__, i_max, i_min, cb_i_avg, cb_i_set, i_offset, sign_i_offset);

	}

	// final set
	if((abs(cb_i_set) > i_vset_margin) || only_lb)
	{
		ret = MAXVAL(lb_v_set, cb_i_n_v_max);
	}
	else
	{
		ret = cb_v_set;
	}

	return ret;
}

void sm5720_set_cycle_cfg(struct sm5720_fuelgauge_data *fuelgauge)
{
	int value;

	value = fuelgauge->info.cycle_limit_cntl|(fuelgauge->info.cycle_high_limit<<12)|(fuelgauge->info.cycle_low_limit<<8);

	sm5720_write_word(fuelgauge->i2c, SM5720_FG_REG_CYCLE_CFG, value);

	pr_info("%s: cycle cfg value = 0x%x\n", __func__, value);

}

void sm5720_set_arsm_cfg(struct sm5720_fuelgauge_data *fuelgauge)
{
	int value;

    value = fuelgauge->info.arsm[0]<<15 | fuelgauge->info.arsm[1]<<6 | fuelgauge->info.arsm[2]<<4 | fuelgauge->info.arsm[3];

	sm5720_write_word(fuelgauge->i2c, SM5720_FG_REG_AUTO_RS_MAN, value);

    pr_info("%s: arsm cfg value = 0x%x\n", __func__, value);
}

static bool sm5720_fg_reg_init(struct sm5720_fuelgauge_data *fuelgauge, bool is_surge)
{
	int i, j, value, ret;
	uint8_t table_reg;

	pr_info("%s: sm5720_fg_reg_init START!!\n", __func__);

	// init mark
	sm5720_write_word(fuelgauge->i2c, SM5720_FG_REG_RESET, FG_INIT_MARK);

	// start first param_ctrl unlock
	sm5720_write_word(fuelgauge->i2c, SM5720_FG_REG_PARAM_CTRL, FG_PARAM_UNLOCK_CODE);

	// RCE write
	for (i = 0; i < 3; i++)
	{
		sm5720_write_word(fuelgauge->i2c, SM5720_FG_REG_RCE0+i, fuelgauge->info.rce_value[i]);
		pr_info("%s: RCE write RCE%d = 0x%x : 0x%x\n",
				__func__,  i, SM5720_FG_REG_RCE0+i, fuelgauge->info.rce_value[i]);
	}

	// DTCD write
	sm5720_write_word(fuelgauge->i2c, SM5720_FG_REG_DTCD, fuelgauge->info.dtcd_value);
	pr_info("%s: DTCD write DTCD = 0x%x : 0x%x\n",
			__func__, SM5720_FG_REG_DTCD, fuelgauge->info.dtcd_value);

	// RS write
	sm5720_write_word(fuelgauge->i2c, SM5720_FG_REG_RS_MAN, fuelgauge->info.rs_value[0]);
	pr_info("%s: RS write RS = 0x%x : 0x%x\n",
			__func__, SM5720_FG_REG_AUTO_RS_MAN, fuelgauge->info.rs_value[0]);

	// VIT_PERIOD write
	sm5720_write_word(fuelgauge->i2c, SM5720_FG_REG_VIT_PERIOD, fuelgauge->info.vit_period);
	pr_info("%s: VIT_PERIOD write VIT_PERIOD = 0x%x : 0x%x\n",
			__func__, SM5720_FG_REG_VIT_PERIOD, fuelgauge->info.vit_period);

	// TABLE_LEN write & pram unlock
	sm5720_write_word(fuelgauge->i2c, SM5720_FG_REG_PARAM_CTRL,
					FG_PARAM_UNLOCK_CODE | FG_TABLE_LEN);

	for (i=0; i < TABLE_MAX; i++)
	{
		table_reg = SM5720_FG_REG_TABLE_START + (i<<4);
		for(j=0; j <= FG_TABLE_LEN; j++)
		{
			sm5720_write_word(fuelgauge->i2c, (table_reg + j), fuelgauge->info.battery_table[i][j]);
			msleep(10);
			if(fuelgauge->info.battery_table[i][j] != sm5720_fg_fs_read_word_table(fuelgauge->i2c, (table_reg + j), TABLE_READ_COUNT))
			{
				pr_info("%s: TABLE write FAIL retry[%d][%d] = 0x%x : 0x%x\n",
					__func__, i, j, (table_reg + j), fuelgauge->info.battery_table[i][j]);
				sm5720_write_word(fuelgauge->i2c, (table_reg + j), fuelgauge->info.battery_table[i][j]);
			}
			pr_info("%s: TABLE write OK [%d][%d] = 0x%x : 0x%x\n",
				__func__, i, j, (table_reg + j), fuelgauge->info.battery_table[i][j]);
		}
	}

	// MIX_MODE write
	sm5720_write_word(fuelgauge->i2c, SM5720_FG_REG_RS_MIX_FACTOR, fuelgauge->info.rs_value[2]);
	sm5720_write_word(fuelgauge->i2c, SM5720_FG_REG_RS_MAX, fuelgauge->info.rs_value[3]);
	sm5720_write_word(fuelgauge->i2c, SM5720_FG_REG_RS_MIN, fuelgauge->info.rs_value[4]);
	sm5720_write_word(fuelgauge->i2c, SM5720_FG_REG_MIX_RATE, fuelgauge->info.mix_value[0]);
	sm5720_write_word(fuelgauge->i2c, SM5720_FG_REG_MIX_INIT_BLANK, fuelgauge->info.mix_value[1]);

	pr_info("%s: RS_MIX_FACTOR = 0x%x, RS_MAX = 0x%x, RS_MIN = 0x%x, MIX_RATE = 0x%x, MIX_INIT_BLANK = 0x%x\n",
		__func__,
		fuelgauge->info.rs_value[2], fuelgauge->info.rs_value[3], fuelgauge->info.rs_value[4],
		fuelgauge->info.mix_value[0], fuelgauge->info.mix_value[1]);

    /*
	// v cal write
	sm5720_write_word(fuelgauge->i2c, SM5720_FG_REG_VOLT_CAL, fuelgauge->info.volt_cal);

	// i ecv write
	sm5720_write_word(fuelgauge->i2c, SM5720_FG_REG_ECV_I_OFF, fuelgauge->info.ecv_i_off);
	sm5720_write_word(fuelgauge->i2c, SM5720_FG_REG_ECV_I_SLO, fuelgauge->info.ecv_i_slo);
	sm5720_write_word(fuelgauge->i2c, SM5720_FG_REG_DP_ECV_I_OFF, fuelgauge->info.dp_ecv_i_off);
	sm5720_write_word(fuelgauge->i2c, SM5720_FG_REG_DP_ECV_I_SLO, fuelgauge->info.dp_ecv_i_slo);

	// i csp write
	sm5720_write_word(fuelgauge->i2c, SM5720_FG_REG_CSP_I_OFF, fuelgauge->info.csp_i_off);
	sm5720_write_word(fuelgauge->i2c, SM5720_FG_REG_CSP_I_SLO, fuelgauge->info.csp_i_slo);
	sm5720_write_word(fuelgauge->i2c, SM5720_FG_REG_DP_CSP_I_OFF, fuelgauge->info.dp_csp_i_off);
	sm5720_write_word(fuelgauge->i2c, SM5720_FG_REG_DP_CSP_I_SLO, fuelgauge->info.dp_csp_i_slo);

	// i csn write
	sm5720_write_word(fuelgauge->i2c, SM5720_FG_REG_CSN_I_OFF, fuelgauge->info.csn_i_off);
	sm5720_write_word(fuelgauge->i2c, SM5720_FG_REG_CSN_I_SLO, fuelgauge->info.csn_i_slo);
	sm5720_write_word(fuelgauge->i2c, SM5720_FG_REG_DP_CSN_I_OFF, fuelgauge->info.dp_csn_i_off);
	sm5720_write_word(fuelgauge->i2c, SM5720_FG_REG_DP_CSN_I_SLO, fuelgauge->info.dp_csn_i_slo);
	*/

    // need writing value print for debug

    // CAP write
	sm5720_write_word(fuelgauge->i2c, SM5720_FG_REG_MIN_CAP, fuelgauge->info.min_cap);
	sm5720_write_word(fuelgauge->i2c, SM5720_FG_REG_CAP, fuelgauge->info.cap);
	pr_info("%s: SM5720_REG_CAP 0x%x, 0x%x\n",
		__func__, fuelgauge->info.min_cap, fuelgauge->info.cap);

	// MISC write
	sm5720_write_word(fuelgauge->i2c, SM5720_FG_REG_MISC, fuelgauge->info.misc);
	pr_info("%s: SM5720_REG_MISC 0x%x : 0x%x\n",
		__func__, SM5720_FG_REG_MISC, fuelgauge->info.misc);

	// TOPOFF SOC
	sm5720_write_word(fuelgauge->i2c, SM5720_FG_REG_TOPOFFSOC, fuelgauge->info.topoff_soc);
	pr_info("%s: SM5720_REG_TOPOFFSOC 0x%x : 0x%x\n", __func__,
		SM5720_FG_REG_TOPOFFSOC, fuelgauge->info.topoff_soc);

	// INIT_last -  control register set
	value = sm5720_read_word(fuelgauge->i2c, SM5720_FG_REG_CNTL);
	if(value == CNTL_REG_DEFAULT_VALUE)
	{
		value = fuelgauge->info.cntl_value;
	}
	value = ENABLE_MIX_MODE | ENABLE_TEMP_MEASURE | ENABLE_MANUAL_OCV | (fuelgauge->info.enable_topoff_soc << 13);
	pr_info("%s: SM5720_REG_CNTL reg : 0x%x\n", __func__, value);

	ret = sm5720_write_word(fuelgauge->i2c, SM5720_FG_REG_CNTL, value);
	if (ret < 0)
		pr_info("%s: fail control register set(%d)\n", __func__, ret);

	pr_info("%s: LAST SM5720_REG_CNTL = 0x%x : 0x%x\n", __func__, SM5720_FG_REG_CNTL, value);

	// LOCK
	value = FG_PARAM_LOCK_CODE | FG_TABLE_LEN;
	sm5720_write_word(fuelgauge->i2c, SM5720_FG_REG_PARAM_CTRL, value);
	pr_info("%s: LAST PARAM CTRL VALUE = 0x%x : 0x%x\n", __func__, SM5720_FG_REG_PARAM_CTRL, value);

	// surge reset defence
	if(is_surge)
	{
		value = ((fuelgauge->info.batt_ocv<<8)/125);
	}
	else
	{
		value = sm5720_fg_calculate_iocv(fuelgauge);

		if((fuelgauge->info.volt_cal & 0x0080) == 0x0080)
		{
			value = value - (fuelgauge->info.volt_cal & 0x007F);
		}
		else
		{
			value = value + (fuelgauge->info.volt_cal & 0x007F);
		}
	}

	sm5720_write_word(fuelgauge->i2c, SM5720_FG_REG_IOCV_MAN, value);
	pr_info("%s: IOCV_MAN_WRITE = %d : 0x%x\n", __func__, SM5720_FG_REG_IOCV_MAN, value);

	// init delay
	msleep(20);

    // write cycle cfg
    sm5720_set_cycle_cfg(fuelgauge);
    // write auto_rs_man cfg
    sm5720_set_arsm_cfg(fuelgauge);

	// write batt data version
	value = (fuelgauge->info.data_ver << 4) & DATA_VERSION;
	sm5720_write_word(fuelgauge->i2c, SM5720_FG_REG_RESERVED, value);
	pr_info("%s: RESERVED = %d : 0x%x\n", __func__, SM5720_FG_REG_RESERVED, value);

    sm5720_dp_setup(fuelgauge);
    sm5720_alg_setup(fuelgauge);

	return 1;
}

static int sm5720_abnormal_reset_check(struct sm5720_fuelgauge_data *fuelgauge)
{
	int cntl_read, reset_read;

	reset_read = sm5720_read_word(fuelgauge->i2c, SM5720_FG_REG_RESET) & 0xF000;
	// abnormal case.... SW reset
	if((sm5720_fg_check_reg_init_need(fuelgauge) && (fuelgauge->info.is_FG_initialised == 1)) || (reset_read == 0))
	{
		cntl_read = sm5720_read_word(fuelgauge->i2c, SM5720_FG_REG_CNTL);
		pr_info("%s: SM5720 FG abnormal case!!!! SM5720_REG_CNTL : 0x%x, reset_read : 0x%x\n", __func__, cntl_read, reset_read);
		// SW reset code
		if(sm5720_fg_verified_write_word(fuelgauge->i2c, SM5720_FG_REG_RESET, SW_RESET_OTP_CODE) < 0)
		{
			pr_info("%s: Warning!!!! SM5720 FG abnormal case.... SW reset FAIL \n", __func__);
		}
		else
		{
			pr_info("%s: SM5720 FG abnormal case.... SW reset OK\n", __func__);
		}
		// delay 100ms
		msleep(100);
		// init code
		if(sm5720_fg_reg_init(fuelgauge, true))
            return FG_ABNORMAL_RESET;
	}
	return 0;
}

static unsigned int sm5720_get_device_id(struct sm5720_fuelgauge_data *fuelgauge)
{
	int ret;
	ret = sm5720_read_word(fuelgauge->i2c, SM5720_FG_REG_DEVICE_ID);
	sm5720_device_id = ret;
	pr_info("%s: SM5720 device_id = 0x%x\n", __func__, ret);

	return ret;
}

int sm5720_call_fg_device_id(void)
{
	pr_info("%s: extern call SM5720 fg_device_id = 0x%x\n", __func__, sm5720_device_id);

	return sm5720_device_id;
}

unsigned int sm5720_get_soc(struct sm5720_fuelgauge_data *fuelgauge)
{
	int ret;
	unsigned int soc;

	ret = sm5720_read_word(fuelgauge->i2c, SM5720_FG_REG_SOC);
	if (ret<0) {
		pr_err("%s: Warning!!!! read soc reg fail\n", __func__);
		soc = 500;
	} else {
		soc = ((ret&0x7f00)>>8) * 10; //integer bit;
		soc = soc + (((ret&0x00ff)*10)/256); // integer + fractional bit
	}

    if(ret&0x8000)
    {
        soc = 0;
    }
	fuelgauge->info.batt_soc = soc;

#ifdef SM5720_FG_FULL_DEBUG
	pr_info("%s: read = 0x%x, soc = %d\n", __func__, ret, soc);
#endif

	if (sm5720_abnormal_reset_check(fuelgauge) < 0)
		return fuelgauge->info.batt_soc;

	// for low temp power off test
	if(fuelgauge->info.volt_alert_flag && (fuelgauge->info.temperature < -100))
	{
		pr_info("%s: volt_alert_flag is TRUE!!!! SOC make force ZERO!!!!\n", __func__);
		fuelgauge->info.batt_soc = 0;
		return 0;
	}

	return soc;
}

static void sm5720_fg_test_read(struct sm5720_fuelgauge_data *fuelgauge)
{
	int ret0, ret1, ret2, ret3, ret4, ret5, ret6, ret7, ret8, ret9;

	ret0 = sm5720_read_word(fuelgauge->i2c, 0xA0);
	ret1 = sm5720_read_word(fuelgauge->i2c, 0xAC);
	ret2 = sm5720_read_word(fuelgauge->i2c, 0xAD);
	ret3 = sm5720_read_word(fuelgauge->i2c, 0xAE);
	ret4 = sm5720_read_word(fuelgauge->i2c, 0xAF);
	ret5 = sm5720_read_word(fuelgauge->i2c, 0x28);
	ret6 = sm5720_read_word(fuelgauge->i2c, 0x2F);
	ret7 = sm5720_read_word(fuelgauge->i2c, 0x01);
	pr_info("%s: 0xA0=0x%04x, 0xAC=0x%04x, 0xAD=0x%04x, 0xAE=0x%04x, 0xAF=0x%04x, 0x28=0x%04x, 0x2F=0x%04x, 0x01=0x%04x, SM5720_ID=0x%04x\n",
		__func__, ret0, ret1, ret2, ret3, ret4, ret5, ret6, ret7, sm5720_device_id);

	ret0 = sm5720_read_word(fuelgauge->i2c, 0xB0);
	ret1 = sm5720_read_word(fuelgauge->i2c, 0xBC);
	ret2 = sm5720_read_word(fuelgauge->i2c, 0xBD);
	ret3 = sm5720_read_word(fuelgauge->i2c, 0xBE);
	ret4 = sm5720_read_word(fuelgauge->i2c, 0xBF);
	ret5 = sm5720_read_word(fuelgauge->i2c, 0x85);
	ret6 = sm5720_read_word(fuelgauge->i2c, 0x86);
	ret7 = sm5720_read_word(fuelgauge->i2c, 0x87);
	ret8 = sm5720_read_word(fuelgauge->i2c, 0x1F);
	ret9 = sm5720_read_word(fuelgauge->i2c, 0x94);
	pr_info("%s: 0xB0=0x%04x, 0xBC=0x%04x, 0xBD=0x%04x, 0xBE=0x%04x, 0xBF=0x%04x, 0x85=0x%04x, 0x86=0x%04x, 0x87=0x%04x, 0x1F=0x%04x, 0x94=0x%04x\n",
		__func__, ret0, ret1, ret2, ret3, ret4, ret5, ret6, ret7, ret8, ret9);

	return;
}

static void sm5720_update_all_value(struct sm5720_fuelgauge_data *fuelgauge)
{
	union power_supply_propval value;

	// check charging.
	value.intval = POWER_SUPPLY_HEALTH_UNKNOWN;
	psy_do_property("sm5720-charger", get,
			POWER_SUPPLY_PROP_HEALTH, value);
#ifdef SM5720_FG_FULL_DEBUG
	pr_info("%s: get POWER_SUPPLY_PROP_HEALTH = 0x%x\n", __func__, value.intval);
#endif
	fuelgauge->info.flag_charge_health =
		(value.intval == POWER_SUPPLY_HEALTH_GOOD) ? 1 : 0;

	fuelgauge->is_charging = (fuelgauge->info.flag_charge_health |
		fuelgauge->ta_exist) && (fuelgauge->info.batt_current>=30);

	// check charger status
	psy_do_property("sm5720-charger", get,
			POWER_SUPPLY_PROP_STATUS, value);
	fuelgauge->info.flag_full_charge =
		(value.intval == POWER_SUPPLY_STATUS_FULL) ? 1 : 0;
	fuelgauge->info.flag_chg_status =
		(value.intval == POWER_SUPPLY_STATUS_CHARGING) ? 1 : 0;

	//vbat
	sm5720_get_vbat(fuelgauge);
	//current
	sm5720_get_curr(fuelgauge);
	//ocv
	sm5720_get_ocv(fuelgauge);
	//temperature
	sm5720_get_temperature(fuelgauge);
	//cycle
	sm5720_get_cycle(fuelgauge);
	//carc
	sm5720_cal_carc(fuelgauge);
	//soc
	sm5720_get_soc(fuelgauge);

	sm5720_fg_test_read(fuelgauge);

	pr_info("%s: chg_h=%d, chg_f=%d, chg_s=%d, is_chg=%d, ta_exist=%d, "
		"v=%d, v_avg=%d, i=%d, i_avg=%d, ocv=%d, fg_t=%d, b_t=%d, cycle=%d, soc=%d, state=0x%x\n",
		__func__, fuelgauge->info.flag_charge_health, fuelgauge->info.flag_full_charge,
		fuelgauge->info.flag_chg_status, fuelgauge->is_charging, fuelgauge->ta_exist,
		fuelgauge->info.batt_voltage, fuelgauge->info.batt_avgvoltage,
		fuelgauge->info.batt_current, fuelgauge->info.batt_avgcurrent, fuelgauge->info.batt_ocv,
		fuelgauge->info.temp_fg, fuelgauge->info.temperature, fuelgauge->info.batt_soc_cycle,
		fuelgauge->info.batt_soc, sm5720_read_word(fuelgauge->i2c, SM5720_FG_REG_OCV_STATE));

#if !defined(CONFIG_SEC_FACTORY)
        sm5720_fg_test_print(fuelgauge);
        sm5720_fg_periodic_read(fuelgauge);
#endif

    return;
}

static int sm5720_fg_check_battery_present(struct sm5720_fuelgauge_data *fuelgauge)
{
	// SM5720 is not suport batt present
	pr_info("%s: sm5720_fg_get_batt_present\n", __func__);

	return true;

}

static bool sm5720_check_jig_status(struct sm5720_fuelgauge_data *fuelgauge)
{
    bool ret = false;
    if(fuelgauge->pdata->jig_gpio)
        ret = gpio_get_value(fuelgauge->pdata->jig_gpio);

    return ret;
}

int sm5720_fg_reset_soc(struct sm5720_fuelgauge_data *fuelgauge)
{
	pr_info("%s: Start quick-start\n", __func__);
	// SW reset code
	sm5720_fg_verified_write_word(fuelgauge->i2c, SM5720_FG_REG_RESET, SW_RESET_CODE);
	// delay 1000ms
	msleep(1000);
	// init code
	if(sm5720_fg_reg_init(fuelgauge, false))
    {
        pr_info("%s: End quick-start\n", __func__);
        return 0;
    }

    return -1;
}

void sm5720_fg_reset_capacity_by_jig_connection(struct sm5720_fuelgauge_data *fuelgauge)
{
    union power_supply_propval value;
	int ret;

    pr_info("%s: voltage = 1.079v (Jig Connection)\n", __func__);

	ret = sm5720_read_word(fuelgauge->i2c, SM5720_FG_REG_RESERVED);
	ret |= JIG_CONNECTED;
	sm5720_write_word(fuelgauge->i2c, SM5720_FG_REG_RESERVED, ret);
	/* If JIG is attached, the voltage is set as 1079 */
	value.intval = 1079;
	psy_do_property("battery", set,
			POWER_SUPPLY_PROP_VOLTAGE_NOW, value);

    return;
}

int sm5720_fg_alert_init(struct sm5720_fuelgauge_data *fuelgauge, int soc)
{
    int ret;
    int value_v_alarm, value_soc_alarm;

    fuelgauge->is_fuel_alerted = false;

    // remove interrupt
    //ret = sm5720_read_word(fuelgauge->i2c, SM5720_REG_INTFG);

    // check status
    ret = sm5720_read_word(fuelgauge->i2c, SM5720_FG_REG_STATUS);
    if(ret < 0)
    {
        pr_err("%s: Failed to read SM5720_FG_REG_STATUS\n", __func__);
        return -1;
    }

    // remove all mask
    //sm5720_write_word(fuelgauge->i2c,SM5720_FG_REG_INTFG_MASK, 0x0000);

    /* enable volt alert only, other alert mask*/
    //ret = MASK_L_SOC_INT|MASK_H_TEM_INT|MASK_L_TEM_INT;
    //sm5720_write_word(fuelgauge->i2c,SM5720_FG_REG_INTFG_MASK,ret);
    //fuelgauge->info.irq_ctrl = ~(ret);

    /* set volt and soc alert threshold */
    value_v_alarm = (((fuelgauge->info.value_v_alarm)<<8)/1000);
    if (sm5720_write_word(fuelgauge->i2c, SM5720_FG_REG_V_ALARM, value_v_alarm)<0)
    {
        pr_err("%s: Failed to write SM5720_FG_REG_V_ALARM\n", __func__);
        return -1;
    }

    value_soc_alarm = (soc<<8);//0x0100 = 1.00%
    if (sm5720_write_word(fuelgauge->i2c, SM5720_FG_REG_SOC_ALARM, value_soc_alarm)<0)
    {
        pr_err("%s: Failed to write SM5720_FG_REG_SOC_ALARM\n", __func__);
        return -1;
    }

    // enabel volt alert control, other alert disable
    ret = sm5720_read_word(fuelgauge->i2c, SM5720_FG_REG_CNTL);
    if(ret < 0)
    {
        pr_err("%s: Failed to read SM5720_FG_REG_CNTL\n", __func__);
        return -1;
    }
    ret = ret | ENABLE_V_ALARM;
    ret = ret & (~ENABLE_SOC_ALARM & ~ENABLE_T_H_ALARM & ~ENABLE_T_L_ALARM);
    if (sm5720_write_word(fuelgauge->i2c, SM5720_FG_REG_CNTL, ret)<0)
    {
        pr_err("%s: Failed to write SM5720_REG_CNTL\n", __func__);
        return -1;
    }

    pr_info("%s: fg_irq= 0x%x, REG_CNTL=0x%x, V_ALARM=%d, SOC_ALARM=0x%x \n",
        __func__, fuelgauge->fg_irq, ret, value_v_alarm, value_soc_alarm);

    return 1;
}

static irqreturn_t sm5720_jig_irq_thread(int irq, void *irq_data)
{
    struct sm5720_fuelgauge_data *fuelgauge = irq_data;

    if (sm5720_check_jig_status(fuelgauge))
        sm5720_fg_reset_capacity_by_jig_connection(fuelgauge);
    else
        pr_info("%s: jig removed\n", __func__);
    return IRQ_HANDLED;
}

static void sm5720_fg_buffer_read(struct sm5720_fuelgauge_data *fuelgauge)
{
	int ret0, ret1, ret2, ret3, ret4, ret5;

	ret0 = sm5720_read_word(fuelgauge->i2c, SM5720_FG_REG_START_LB_V);
	ret1 = sm5720_read_word(fuelgauge->i2c, SM5720_FG_REG_START_LB_V+1);
	ret2 = sm5720_read_word(fuelgauge->i2c, SM5720_FG_REG_START_LB_V+2);
	ret3 = sm5720_read_word(fuelgauge->i2c, SM5720_FG_REG_START_LB_V+3);
	ret4 = sm5720_read_word(fuelgauge->i2c, SM5720_FG_REG_START_LB_V+4);
	ret5 = sm5720_read_word(fuelgauge->i2c, SM5720_FG_REG_START_LB_V+5);
	pr_info("%s: sm5720 FG buffer 0x30_0x35 lb_V = 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x \n",
		__func__, ret0, ret1, ret2, ret3, ret4, ret5);

	ret0 = sm5720_read_word(fuelgauge->i2c, SM5720_FG_REG_START_CB_V);
	ret1 = sm5720_read_word(fuelgauge->i2c, SM5720_FG_REG_START_CB_V+1);
	ret2 = sm5720_read_word(fuelgauge->i2c, SM5720_FG_REG_START_CB_V+2);
	ret3 = sm5720_read_word(fuelgauge->i2c, SM5720_FG_REG_START_CB_V+3);
	ret4 = sm5720_read_word(fuelgauge->i2c, SM5720_FG_REG_START_CB_V+4);
	ret5 = sm5720_read_word(fuelgauge->i2c, SM5720_FG_REG_START_CB_V+5);
	pr_info("%s: sm5720 FG buffer 0x36_0x3B cb_V = 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x \n",
		__func__, ret0, ret1, ret2, ret3, ret4, ret5);


	ret0 = sm5720_read_word(fuelgauge->i2c, SM5720_FG_REG_START_LB_I);
	ret1 = sm5720_read_word(fuelgauge->i2c, SM5720_FG_REG_START_LB_I+1);
	ret2 = sm5720_read_word(fuelgauge->i2c, SM5720_FG_REG_START_LB_I+2);
	ret3 = sm5720_read_word(fuelgauge->i2c, SM5720_FG_REG_START_LB_I+3);
	ret4 = sm5720_read_word(fuelgauge->i2c, SM5720_FG_REG_START_LB_I+4);
	ret5 = sm5720_read_word(fuelgauge->i2c, SM5720_FG_REG_START_LB_I+5);
	pr_info("%s: sm5720 FG buffer 0x40_0x45 lb_I = 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x \n",
		__func__, ret0, ret1, ret2, ret3, ret4, ret5);


	ret0 = sm5720_read_word(fuelgauge->i2c, SM5720_FG_REG_START_CB_I);
	ret1 = sm5720_read_word(fuelgauge->i2c, SM5720_FG_REG_START_CB_I+1);
	ret2 = sm5720_read_word(fuelgauge->i2c, SM5720_FG_REG_START_CB_I+2);
	ret3 = sm5720_read_word(fuelgauge->i2c, SM5720_FG_REG_START_CB_I+3);
	ret4 = sm5720_read_word(fuelgauge->i2c, SM5720_FG_REG_START_CB_I+4);
	ret5 = sm5720_read_word(fuelgauge->i2c, SM5720_FG_REG_START_CB_I+5);
	pr_info("%s: sm5720 FG buffer 0x46_0x4B cb_I = 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x \n",
		__func__, ret0, ret1, ret2, ret3, ret4, ret5);

	return;
}

static bool sm5720_fg_init(struct sm5720_fuelgauge_data *fuelgauge)
{
    if(sm5720_get_device_id(fuelgauge)<0)
    {
        return false;
    }
    sm5720_fg_check_battery_present(fuelgauge);

    if (fuelgauge->pdata->jig_gpio) {
        int ret;
        ret = request_threaded_irq(fuelgauge->pdata->jig_irq,
                NULL, sm5720_jig_irq_thread,
                IRQF_TRIGGER_RISING | IRQF_ONESHOT,
                "jig-irq", fuelgauge);
        if (ret) {
            pr_info("%s: Failed to Request IRQ\n",
                    __func__);
        }
        pr_info("%s: jig_result : %d\n", __func__, sm5720_check_jig_status(fuelgauge));

        /* initial check for the JIG */
        if (sm5720_check_jig_status(fuelgauge))
            sm5720_fg_reset_capacity_by_jig_connection(fuelgauge);
    }

    //sm5720_dp_setup(fuelgauge);
    sm5720_alg_setup(fuelgauge);
    
	// for debug
	sm5720_fg_buffer_read(fuelgauge);

    return true;
}

bool sm5720_fg_fuelalert_init(struct sm5720_fuelgauge_data *fuelgauge,
                int soc)
{
    /* 1. Set sm5720 alert configuration. */
    if (sm5720_fg_alert_init(fuelgauge, soc) > 0)
        return true;
    else
        return false;
}

void sm5720_fg_fuelalert_set(struct sm5720_fuelgauge_data *fuelgauge,
                   int enable)
{
    u16 ret;

    ret = sm5720_read_word(fuelgauge->i2c, SM5720_FG_REG_STATUS);
    pr_info("%s: SM5720_FG_REG_STATUS(0x%x)\n",
        __func__, ret);
    
    /* not use SOC alarm
    if(ret & fuelgauge->info.irq_ctrl & ENABLE_SOC_ALARM) {
        fuelgauge->info.soc_alert_flag = true;
        // todo more action
    }
    */

    if (ret & ENABLE_V_ALARM) {
        pr_info("%s : Battery Voltage is Very Low!! SW V EMPTY ENABLE\n", __func__);
        fuelgauge->sw_v_empty = VEMPTY_MODE;
    }
}


bool sm5720_fg_fuelalert_process(void *irq_data)
{
    struct sm5720_fuelgauge_data *fuelgauge =
        (struct sm5720_fuelgauge_data *)irq_data;

    sm5720_fg_fuelalert_set(fuelgauge, 0);

    return true;
}

bool sm5720_fg_reset(struct sm5720_fuelgauge_data *fuelgauge)
{
    if (!sm5720_fg_reset_soc(fuelgauge))
        return true;
    else
        return false;
}

static int sm5720_fg_check_capacity_max(
    struct sm5720_fuelgauge_data *fuelgauge, int capacity_max)
{
    int cap_max, cap_min;

    cap_max = (fuelgauge->pdata->capacity_max +
            fuelgauge->pdata->capacity_max_margin) * 100 / 101;
    cap_min = (fuelgauge->pdata->capacity_max -
        fuelgauge->pdata->capacity_max_margin) * 100 / 101;

    return (capacity_max < cap_min) ? cap_min :
        ((capacity_max > cap_max) ? cap_max : capacity_max);
}

static void sm5720_fg_get_scaled_capacity(
    struct sm5720_fuelgauge_data *fuelgauge,
    union power_supply_propval *val)
{
    val->intval = (val->intval < fuelgauge->pdata->capacity_min) ?
        0 : ((val->intval - fuelgauge->pdata->capacity_min) * 1000 /
        (fuelgauge->capacity_max - fuelgauge->pdata->capacity_min));

    pr_info("%s: scaled capacity (%d.%d)\n",
        __func__, val->intval/10, val->intval%10);
}


/* capacity is integer */
static void sm5720_fg_get_atomic_capacity(
    struct sm5720_fuelgauge_data *fuelgauge,
    union power_supply_propval *val)
{

    pr_debug("%s : NOW(%d), OLD(%d)\n",
        __func__, val->intval, fuelgauge->capacity_old);

    if (fuelgauge->pdata->capacity_calculation_type &
        SEC_FUELGAUGE_CAPACITY_TYPE_ATOMIC) {
    if (fuelgauge->capacity_old < val->intval)
        val->intval = fuelgauge->capacity_old + 1;
    else if (fuelgauge->capacity_old > val->intval)
        val->intval = fuelgauge->capacity_old - 1;
    }

    /* keep SOC stable in abnormal status */
    if (fuelgauge->pdata->capacity_calculation_type &
        SEC_FUELGAUGE_CAPACITY_TYPE_SKIP_ABNORMAL) {
        if (!fuelgauge->is_charging &&
            fuelgauge->capacity_old < val->intval) {
            pr_err("%s: capacity (old %d : new %d)\n",
                __func__, fuelgauge->capacity_old, val->intval);
            val->intval = fuelgauge->capacity_old;
        }
    }

    /* updated old capacity */
    fuelgauge->capacity_old = val->intval;
}

static int sm5720_fg_calculate_dynamic_scale(
    struct sm5720_fuelgauge_data *fuelgauge, int capacity)
{
    union power_supply_propval raw_soc_val;
    raw_soc_val.intval = sm5720_get_soc(fuelgauge);

    if (raw_soc_val.intval <
        fuelgauge->pdata->capacity_max -
        fuelgauge->pdata->capacity_max_margin) {
        pr_info("%s: raw soc(%d) is very low, skip routine\n",
            __func__, raw_soc_val.intval);
        return fuelgauge->capacity_max;
    } else {
        fuelgauge->capacity_max =
            (raw_soc_val.intval >
            fuelgauge->pdata->capacity_max +
            fuelgauge->pdata->capacity_max_margin) ?
            (fuelgauge->pdata->capacity_max +
            fuelgauge->pdata->capacity_max_margin) :
            raw_soc_val.intval;
        pr_debug("%s: raw soc (%d)", __func__,
             fuelgauge->capacity_max);
    }

    fuelgauge->capacity_max =
        (fuelgauge->capacity_max * 100 / (capacity + 1));
    fuelgauge->capacity_old = capacity;

    pr_info("%s: %d is used for capacity_max, capacity(%d)\n",
        __func__, fuelgauge->capacity_max, capacity);

    return fuelgauge->capacity_max;
}

#if defined(CONFIG_EN_OOPS)
static void sm5720_set_full_value(struct sm5720_fuelgauge_data *fuelgauge,
                    int cable_type)
{
    pr_info("%s : sm5720 todo\n",
        __func__);
}
#endif

static int calc_ttf(struct sm5720_fuelgauge_data *fuelgauge, union power_supply_propval *val)
{
    union power_supply_propval chg_val2;
    int i;
    int cc_time = 0;

    int soc = fuelgauge->raw_capacity;
    int current_now = fuelgauge->current_now;
    int current_avg = fuelgauge->current_avg;
    int charge_current = (current_avg > 0)? current_avg : current_now;
    struct cv_slope *cv_data = fuelgauge->cv_data;
    int design_cap = fuelgauge->battery_data->Capacity / 2;

    if(!cv_data || (val->intval <= 0)) {
        pr_info("%s: no cv_data or val: %d\n", __func__, val->intval);
        return -1;
    }
    /* To prevent overflow if charge current is 30 under, change value*/
    if (charge_current <= 30) {
        charge_current = val->intval;
    }
    psy_do_property("sm5720-charger", get, POWER_SUPPLY_PROP_CHARGE_NOW,
            chg_val2);
    if (!strcmp(chg_val2.strval, "CC Mode") || !strcmp(chg_val2.strval, "NONE")) { //CC mode || NONE
        charge_current = val->intval;
    }
    for (i = 0; i < fuelgauge->cv_data_lenth ;i++) {
        if (charge_current >= cv_data[i].fg_current)
            break;
    }
    if (cv_data[i].soc  < soc) {
        for (i = 0; i < fuelgauge->cv_data_lenth; i++) {
            if (soc <= cv_data[i].soc)
                break;
        }
    } else if (!strcmp(chg_val2.strval, "CC Mode") || !strcmp(chg_val2.strval, "NONE")) { //CC mode || NONE
        cc_time = design_cap * (cv_data[i].soc - soc)\
                / val->intval * 3600 / 1000;
        pr_debug("%s: cc_time: %d\n", __func__, cc_time);
        if (cc_time < 0) {

            cc_time = 0;
        }
    }

        pr_debug("%s: cap: %d, soc: %4d, T: %6d, now: %4d, avg: %4d, cv soc: %4d, i: %4d, val: %d, %s\n",
         __func__, design_cap, soc, cv_data[i].time + cc_time, current_now, current_avg, cv_data[i].soc, i, val->intval, chg_val2.strval);

        if (cv_data[i].time + cc_time >= 60)
                return cv_data[i].time + cc_time;
        else
                return 60; //minimum 1minutes
}

static void sm5720_fg_set_vempty(struct sm5720_fuelgauge_data *fuelgauge, bool en)
{
    if (en) {
        pr_info("%s : Low Capacity HW V EMPTY Enable\n", __func__);
        fuelgauge->sw_v_empty = NORMAL_MODE;
        fuelgauge->hw_v_empty = true;
    } else {
        fuelgauge->hw_v_empty = false;
    }
}

static int sm5720_fg_get_property(struct power_supply *psy,
                 enum power_supply_property psp,
                 union power_supply_propval *val)
{
    struct sm5720_fuelgauge_data *fuelgauge =
        container_of(psy, struct sm5720_fuelgauge_data, psy_fg);
    //static int abnormal_current_cnt = 0;
    //union power_supply_propval value;

    switch (psp) {
        /* Cell voltage (VCELL, mV) */
    case POWER_SUPPLY_PROP_VOLTAGE_NOW:
        sm5720_get_vbat(fuelgauge);
        val->intval = fuelgauge->info.batt_voltage;
        break;
    case POWER_SUPPLY_PROP_VOLTAGE_AVG:
        switch (val->intval) {
        case SEC_BATTERY_VOLTAGE_OCV:
			sm5720_get_ocv(fuelgauge);
			val->intval = fuelgauge->info.batt_ocv;
            break;
        case SEC_BATTERY_VOLTAGE_AVERAGE:
            val->intval = fuelgauge->info.batt_avgvoltage;
            break;
        }
        break;
        /* Current */
    case POWER_SUPPLY_PROP_CURRENT_NOW:
		sm5720_get_curr(fuelgauge);
		if (val->intval == SEC_BATTERY_CURRENT_UA)
			val->intval = fuelgauge->info.batt_current * 1000;
		else
			val->intval = fuelgauge->info.batt_current;
		break;
        /* Average Current */
    case POWER_SUPPLY_PROP_CURRENT_AVG:
		if (val->intval == SEC_BATTERY_CURRENT_UA)
			val->intval = fuelgauge->info.batt_avgcurrent * 1000;
		else
			val->intval = fuelgauge->info.batt_avgcurrent;
		break;
        /* Full Capacity */
    case POWER_SUPPLY_PROP_ENERGY_NOW:
        switch (val->intval) {
        case SEC_BATTERY_CAPACITY_DESIGNED:
            break;
        case SEC_BATTERY_CAPACITY_ABSOLUTE:
            break;
        case SEC_BATTERY_CAPACITY_TEMPERARY:
            break;
        case SEC_BATTERY_CAPACITY_CURRENT:
            break;
        case SEC_BATTERY_CAPACITY_AGEDCELL:
            break;
        case SEC_BATTERY_CAPACITY_CYCLE:
            sm5720_get_cycle(fuelgauge);
            val->intval = fuelgauge->info.batt_soc_cycle;
            break;
        }
        break;
        /* SOC (%) */
    case POWER_SUPPLY_PROP_CAPACITY:
		sm5720_update_all_value(fuelgauge);
		/* SM5720 F/G unit is 0.1%, raw ==> convert the unit to 0.01% */
		if (val->intval == SEC_FUELGAUGE_CAPACITY_TYPE_RAW) {
			val->intval = fuelgauge->info.batt_soc * 10;
			break;
		} else
			val->intval = fuelgauge->info.batt_soc;


            fuelgauge->raw_capacity = val->intval;

            if (fuelgauge->pdata->capacity_calculation_type &
                (SEC_FUELGAUGE_CAPACITY_TYPE_SCALE |
                 SEC_FUELGAUGE_CAPACITY_TYPE_DYNAMIC_SCALE))
                sm5720_fg_get_scaled_capacity(fuelgauge, val);

            /* capacity should be between 0% and 100%
             * (0.1% degree)
             */
            if (val->intval > 1000)
                val->intval = 1000;
            if (val->intval < 0)
                val->intval = 0;

            /* get only integer part */
            val->intval /= 10;

            if (fuelgauge->using_hw_vempty) {
                if ((fuelgauge->raw_capacity <= 50) &&
                    !fuelgauge->hw_v_empty){
                    sm5720_fg_set_vempty(fuelgauge, true);
                } else if ((fuelgauge->raw_capacity > 50) &&
                       fuelgauge->hw_v_empty){
                    sm5720_fg_set_vempty(fuelgauge, false);
                }
            }

            if (!fuelgauge->is_charging &&
                !fuelgauge->hw_v_empty && (fuelgauge->sw_v_empty == VEMPTY_MODE)) {
                pr_info("%s : SW V EMPTY. Decrease SOC\n", __func__);
                val->intval = 0;
            } else if ((fuelgauge->sw_v_empty == VEMPTY_RECOVERY_MODE) &&
                   (val->intval == fuelgauge->capacity_old)) {
                fuelgauge->sw_v_empty = NORMAL_MODE;
            }

            /* check whether doing the wake_unlock */
            if ((val->intval > fuelgauge->pdata->fuel_alert_soc) &&
                 fuelgauge->is_fuel_alerted) {
                sm5720_fg_fuelalert_init(fuelgauge,
                      fuelgauge->pdata->fuel_alert_soc);
            }

            /* (Only for atomic capacity)
             * In initial time, capacity_old is 0.
             * and in resume from sleep,
             * capacity_old is too different from actual soc.
             * should update capacity_old
             * by val->intval in booting or resume.
             */
            if ((fuelgauge->initial_update_of_soc) &&
                (fuelgauge->sw_v_empty == NORMAL_MODE)){
                /* updated old capacity */
                fuelgauge->capacity_old = val->intval;
                fuelgauge->initial_update_of_soc = false;
                break;
            }

            if (fuelgauge->pdata->capacity_calculation_type &
                (SEC_FUELGAUGE_CAPACITY_TYPE_ATOMIC |
                 SEC_FUELGAUGE_CAPACITY_TYPE_SKIP_ABNORMAL)){
                sm5720_fg_get_atomic_capacity(fuelgauge, val);
        }
        break;
        /* Battery Temperature */
    case POWER_SUPPLY_PROP_TEMP:
        /* Target Temperature */
    case POWER_SUPPLY_PROP_TEMP_AMBIENT:
        sm5720_get_temperature(fuelgauge);
        val->intval = fuelgauge->info.temp_fg;
        break;
#if defined(CONFIG_EN_OOPS)
    case POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN:
        return -ENODATA;
#endif
    case POWER_SUPPLY_PROP_ENERGY_FULL:
    case POWER_SUPPLY_PROP_ENERGY_FULL_DESIGN:
        val->intval = fuelgauge->capacity_max;
        break;
    case POWER_SUPPLY_PROP_TIME_TO_FULL_NOW:
        val->intval = calc_ttf(fuelgauge, val);
        break;
    case POWER_SUPPLY_PROP_CHARGE_ENABLED:
        break;
#if defined(CONFIG_BATTERY_AGE_FORECAST)
    case POWER_SUPPLY_PROP_CAPACITY_LEVEL:
        return -ENODATA;
#endif
    default:
        return -EINVAL;
    }
    return 0;
}

#if defined(CONFIG_UPDATE_BATTERY_DATA)
static int sm5720_fuelgauge_parse_dt(struct sm5720_fuelgauge_data *fuelgauge);
#endif
static int sm5720_fg_set_property(struct power_supply *psy,
                 enum power_supply_property psp,
                 const union power_supply_propval *val)
{
    struct sm5720_fuelgauge_data *fuelgauge =
        container_of(psy, struct sm5720_fuelgauge_data, psy_fg);
    //u8 data[2] = {0, 0};

    switch (psp) {
    case POWER_SUPPLY_PROP_STATUS:
        break;
    case POWER_SUPPLY_PROP_CHARGE_FULL:
        if (fuelgauge->pdata->capacity_calculation_type &
            SEC_FUELGAUGE_CAPACITY_TYPE_DYNAMIC_SCALE)
            sm5720_fg_calculate_dynamic_scale(fuelgauge, val->intval);
        break;
#if defined(CONFIG_EN_OOPS)
    case POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN:
        sm5720_set_full_value(fuelgauge, val->intval);
        break;
#endif
    case POWER_SUPPLY_PROP_ONLINE:
        fuelgauge->cable_type = val->intval;
        if (val->intval == POWER_SUPPLY_TYPE_BATTERY) {
			fuelgauge->ta_exist = false;
            fuelgauge->is_charging = false;
        } else {
            fuelgauge->ta_exist = true;
            fuelgauge->is_charging = true;

            if (fuelgauge->sw_v_empty != NORMAL_MODE) {
                fuelgauge->sw_v_empty = NORMAL_MODE;
                fuelgauge->initial_update_of_soc = true;
                sm5720_fg_fuelalert_init(fuelgauge,
                               fuelgauge->pdata->fuel_alert_soc);
            }
        }
        break;
        /* Battery Temperature */
    case POWER_SUPPLY_PROP_CAPACITY:
        if (val->intval == SEC_FUELGAUGE_CAPACITY_TYPE_RESET) {
            fuelgauge->initial_update_of_soc = true;
            if (!sm5720_fg_reset(fuelgauge))
                return -EINVAL;
            else
                break;
        }
    case POWER_SUPPLY_PROP_TEMP:
		fuelgauge->info.temperature = val->intval;
        if(val->intval < 0) {
                pr_info("%s: set the low temp reset! temp : %d\n",
                        __func__, val->intval);
            }
        break;
    case POWER_SUPPLY_PROP_TEMP_AMBIENT:
        break;
    case POWER_SUPPLY_PROP_ENERGY_NOW:
        sm5720_fg_reset_capacity_by_jig_connection(fuelgauge);
        break;
    case POWER_SUPPLY_PROP_ENERGY_FULL_DESIGN:
        pr_info("%s: capacity_max changed, %d -> %d\n",
            __func__, fuelgauge->capacity_max, val->intval);
        fuelgauge->capacity_max = sm5720_fg_check_capacity_max(fuelgauge, val->intval);
        fuelgauge->initial_update_of_soc = true;
        break;
    case POWER_SUPPLY_PROP_CHARGE_ENABLED:
        break;
#if defined(CONFIG_BATTERY_AGE_FORECAST)
    case POWER_SUPPLY_PROP_CAPACITY_LEVEL:
        pr_info("%s: need BATTERY_AGE_FORECAST!!!!\n", __func__);
        break;
#endif
#if defined(CONFIG_UPDATE_BATTERY_DATA)
    case POWER_SUPPLY_PROP_POWER_DESIGN:
        sm5720_fuelgauge_parse_dt(fuelgauge);
        break;
#endif
    default:
        return -EINVAL;
    }
    return 0;
}

static void sm5720_fg_isr_work(struct work_struct *work)
{
    struct sm5720_fuelgauge_data *fuelgauge =
        container_of(work, struct sm5720_fuelgauge_data, isr_work.work);

    /* process for fuel gauge chip */
    sm5720_fg_fuelalert_process(fuelgauge);

    wake_unlock(&fuelgauge->fuel_alert_wake_lock);
}

static irqreturn_t sm5720_fg_irq_thread(int irq, void *irq_data)
{
    struct sm5720_fuelgauge_data *fuelgauge = irq_data;

    pr_info("%s\n", __func__);

    if (fuelgauge->is_fuel_alerted) {
        return IRQ_HANDLED;
    } else {
        wake_lock(&fuelgauge->fuel_alert_wake_lock);
        fuelgauge->is_fuel_alerted = true;
        schedule_delayed_work(&fuelgauge->isr_work, 0);
    }

    return IRQ_HANDLED;
}

static int sm5720_fuelgauge_debugfs_show(struct seq_file *s, void *data)
{
    struct sm5720_fuelgauge_data *fuelgauge = s->private;
    int i;
    u8 reg;
    u8 reg_data;

    seq_printf(s, "SM5720 FUELGAUGE IC :\n");
    seq_printf(s, "===================\n");
    for (i = 0; i < 16; i++) {
        if (i == 12)
            continue;
        for (reg = 0; reg < 0x10; reg++) {
            reg_data = sm5720_read_word(fuelgauge->i2c, reg + i * 0x10);
            seq_printf(s, "0x%02x:\t0x%04x\n", reg + i * 0x10, reg_data);
        }
        if (i == 4)
            i = 10;
    }
    seq_printf(s, "\n");
    return 0;
}

static int sm5720_fuelgauge_debugfs_open(struct inode *inode, struct file *file)
{
    return single_open(file, sm5720_fuelgauge_debugfs_show, inode->i_private);
}

static const struct file_operations sm5720_fuelgauge_debugfs_fops = {
    .open           = sm5720_fuelgauge_debugfs_open,
    .read           = seq_read,
    .llseek         = seq_lseek,
    .release        = single_release,
};

#ifdef CONFIG_OF
#define PROPERTY_NAME_SIZE 128

#define PINFO(format, args...) \
	printk(KERN_INFO "%s() line-%d: " format, \
		__func__, __LINE__, ## args)

static int sm5720_fuelgauge_parse_dt(struct sm5720_fuelgauge_data *fuelgauge)
{
    char prop_name[PROPERTY_NAME_SIZE];
    int battery_id = -1;
    int table[16];
    int rce_value[3];
    int rs_value[5];
    int mix_value[2];
    int battery_type[3];
    int topoff_soc[3];
    int cycle_cfg[3];
    int v_offset_cancel[4];
    int temp_volcal[3];
    int temp_offset[6];
    int temp_cal[10];
    int ext_temp_cal[10];
    int set_temp_poff[4];
    int curr_offset[7];
    int curr_cal[6];
    int arsm[4];
#ifdef ENABLE_FULL_OFFSET
    int full_offset[2]; 
#endif

    int ret;
    int i, j;

    struct device_node *np = of_find_node_by_name(NULL, "sm5720-fuelgauge");

    /* reset, irq gpio info */
    if (np == NULL) {
        pr_err("%s np NULL\n", __func__);
    } else {
		ret = of_property_read_u32(np, "fuelgauge,capacity_max",
				&fuelgauge->pdata->capacity_max);
		if (ret < 0)
			pr_err("%s error reading capacity_max %d\n", __func__, ret);

		ret = of_property_read_u32(np, "fuelgauge,capacity_max_margin",
				&fuelgauge->pdata->capacity_max_margin);
		if (ret < 0)
			pr_err("%s error reading capacity_max_margin %d\n", __func__, ret);

		ret = of_property_read_u32(np, "fuelgauge,capacity_min",
				&fuelgauge->pdata->capacity_min);
		if (ret < 0)
			pr_err("%s error reading capacity_min %d\n", __func__, ret);

		pr_info("%s: capacity_max: %d, "
				"capacity_max_margin: 0x%x, "
				"capacity_min: %d\n", __func__, fuelgauge->pdata->capacity_max,
				fuelgauge->pdata->capacity_max_margin,
				fuelgauge->pdata->capacity_min);

		ret = of_property_read_u32(np, "fuelgauge,capacity_calculation_type",
				&fuelgauge->pdata->capacity_calculation_type);
		if (ret < 0)
			pr_err("%s error reading capacity_calculation_type %d\n",
					__func__, ret);
		ret = of_property_read_u32(np, "fuelgauge,fuel_alert_soc",
				&fuelgauge->pdata->fuel_alert_soc);
		if (ret < 0)
			pr_err("%s error reading pdata->fuel_alert_soc %d\n",
					__func__, ret);
		fuelgauge->pdata->repeated_fuelalert = of_property_read_bool(np,
				"fuelgaguge,repeated_fuelalert");

		pr_info("%s: "
				"calculation_type: 0x%x, fuel_alert_soc: %d,\n"
				"repeated_fuelalert: %d\n", __func__,
				fuelgauge->pdata->capacity_calculation_type,
				fuelgauge->pdata->fuel_alert_soc, fuelgauge->pdata->repeated_fuelalert);


        fuelgauge->using_temp_compensation = of_property_read_bool(np,
                           "fuelgauge,using_temp_compensation");
        if (fuelgauge->using_temp_compensation) {
            ret = of_property_read_u32(np, "fuelgauge,low_temp_limit",
                           &fuelgauge->low_temp_limit);
            if (ret < 0)
                pr_err("%s error reading low temp limit %d\n", __func__, ret);

            ret = of_property_read_u32(np, "fuelgauge,low_temp_recovery",
                           &fuelgauge->low_temp_recovery);
            if (ret < 0)
                pr_err("%s error reading low temp recovery %d\n", __func__, ret);

            pr_info("%s : LOW TEMP LIMIT(%d) RECOVERY(%d)\n",
                __func__, fuelgauge->low_temp_limit, fuelgauge->low_temp_recovery);
        }

        fuelgauge->using_hw_vempty = of_property_read_bool(np,
                                   "fuelgauge,using_hw_vempty");
        if (fuelgauge->using_hw_vempty) {
            ret = of_property_read_u32(np, "fuelgauge,v_empty",
                           &fuelgauge->battery_data->V_empty);
            if (ret < 0)
                pr_err("%s error reading v_empty %d\n",
                       __func__, ret);

            ret = of_property_read_u32(np, "fuelgauge,v_empty_origin",
                           &fuelgauge->battery_data->V_empty_origin);
            if(ret < 0)
                pr_err("%s error reading v_empty_origin %d\n",
                       __func__, ret);
        }

        fuelgauge->pdata->jig_gpio = of_get_named_gpio(np, "fuelgauge,jig_gpio", 0);
        if (fuelgauge->pdata->jig_gpio < 0) {
            pr_err("%s error reading jig_gpio = %d\n",
                    __func__,fuelgauge->pdata->jig_gpio);
            fuelgauge->pdata->jig_gpio = 0;
        } else {
            fuelgauge->pdata->jig_irq = gpio_to_irq(fuelgauge->pdata->jig_gpio);
        }

        ret = of_property_read_u32(np, "fuelgauge,fg_resistor",
                &fuelgauge->fg_resistor);
        if (ret < 0) {
            pr_err("%s error reading fg_resistor %d\n",
                    __func__, ret);
            fuelgauge->fg_resistor = 1;
        }


#if defined(CONFIG_EN_OOPS)
        // todo cap redesign
#endif

        ret = of_property_read_u32(np, "fuelgauge,capacity",
                       &fuelgauge->battery_data->Capacity);
        if (ret < 0)
            pr_err("%s error reading Capacity %d\n",
                    __func__, ret);

#if defined(CONFIG_BATTERY_AGE_FORECAST)
        ret = of_property_read_u32(np, "battery,full_condition_soc",
            &fuelgauge->pdata->full_condition_soc);
        if (ret) {
            fuelgauge->pdata->full_condition_soc = 93;
            pr_info("%s : Full condition soc is Empty\n", __func__);
        }
#endif
    }



    // get battery_params node for reg init
    np = of_find_node_by_name(of_node_get(np), "battery_params");
    if (np == NULL) {
        PINFO("Cannot find child node \"battery_params\"\n");
        return -EINVAL;
    }

    // get battery_id
    if (of_property_read_u32(np, "battery,id", &battery_id) < 0)
        PINFO("not battery,id property\n");
    PINFO("battery id = %d\n", battery_id);

    // get battery_table
    for (i = DISCHARGE_TABLE; i < TABLE_MAX; i++) {
        snprintf(prop_name, PROPERTY_NAME_SIZE,
             "battery%d,%s%d", battery_id, "battery_table", i);

        ret = of_property_read_u32_array(np, prop_name, table, 16);
        if (ret < 0) {
            PINFO("Can get prop %s (%d)\n", prop_name, ret);
        }
        for (j = 0; j <= FG_TABLE_LEN; j++)
        {
            fuelgauge->info.battery_table[i][j] = table[j];
            PINFO("%s = <table[%d][%d] 0x%x>\n", prop_name, i, j, table[j]);
        }
    }

    // get rce
    for (i = 0; i < 3; i++) {
        snprintf(prop_name, PROPERTY_NAME_SIZE, "battery%d,%s", battery_id, "rce_value");
        ret = of_property_read_u32_array(np, prop_name, rce_value, 3);
        if (ret < 0) {
            PINFO("Can get prop %s (%d)\n", prop_name, ret);
        }
        fuelgauge->info.rce_value[i] = rce_value[i];
    }
    PINFO("%s = <0x%x 0x%x 0x%x>\n", prop_name, rce_value[0], rce_value[1], rce_value[2]);

    // get dtcd_value
    snprintf(prop_name, PROPERTY_NAME_SIZE, "battery%d,%s", battery_id, "dtcd_value");
    ret = of_property_read_u32_array(np, prop_name, &fuelgauge->info.dtcd_value, 1);
    if (ret < 0)
        PINFO("Can get prop %s (%d)\n", prop_name, ret);
    PINFO("%s = <0x%x>\n",prop_name, fuelgauge->info.dtcd_value);

    // get rs_value
    for (i = 0; i < 5; i++) {
        snprintf(prop_name, PROPERTY_NAME_SIZE, "battery%d,%s", battery_id, "rs_value");
        ret = of_property_read_u32_array(np, prop_name, rs_value, 5);
        if (ret < 0) {
            PINFO("Can get prop %s (%d)\n", prop_name, ret);
        }
        fuelgauge->info.rs_value[i] = rs_value[i];
    }
    PINFO("%s = <0x%x 0x%x 0x%x 0x%x 0x%x>\n", prop_name, rs_value[0], rs_value[1], rs_value[2], rs_value[3], rs_value[4]);

    // get vit_period
    snprintf(prop_name, PROPERTY_NAME_SIZE, "battery%d,%s", battery_id, "vit_period");
    ret = of_property_read_u32_array(np, prop_name, &fuelgauge->info.vit_period, 1);
    if (ret < 0)
        PINFO("Can get prop %s (%d)\n", prop_name, ret);
    PINFO("%s = <0x%x>\n",prop_name, fuelgauge->info.vit_period);

    // get mix_value
    for (i = 0; i < 2; i++) {
        snprintf(prop_name, PROPERTY_NAME_SIZE, "battery%d,%s", battery_id, "mix_value");
        ret = of_property_read_u32_array(np, prop_name, mix_value, 2);
        if (ret < 0) {
            PINFO("Can get prop %s (%d)\n", prop_name, ret);
        }
        fuelgauge->info.mix_value[i] = mix_value[i];
    }
    PINFO("%s = <0x%x 0x%x>\n", prop_name, mix_value[0], mix_value[1]);

    // battery_type
    snprintf(prop_name, PROPERTY_NAME_SIZE, "battery%d,%s", battery_id, "battery_type");
    ret = of_property_read_u32_array(np, prop_name, battery_type, 3);
    if (ret < 0)
        PINFO("Can get prop %s (%d)\n", prop_name, ret);
    fuelgauge->info.batt_v_max = battery_type[0];
    fuelgauge->info.min_cap = battery_type[1];
    fuelgauge->info.cap = battery_type[2];

    PINFO("%s = <%d %d %d>\n", prop_name,
        fuelgauge->info.batt_v_max, fuelgauge->info.min_cap, fuelgauge->info.cap);

    // MISC
    snprintf(prop_name, PROPERTY_NAME_SIZE, "battery%d,%s", battery_id, "misc");
    ret = of_property_read_u32_array(np, prop_name, &fuelgauge->info.misc, 1);
    if (ret < 0)
        PINFO("Can get prop %s (%d)\n", prop_name, ret);
    PINFO("%s = <0x%x>\n", prop_name, fuelgauge->info.misc);

    // V_ALARM
    snprintf(prop_name, PROPERTY_NAME_SIZE, "battery%d,%s", battery_id, "v_alarm");
    ret = of_property_read_u32_array(np, prop_name, &fuelgauge->info.value_v_alarm, 1);
    if (ret < 0)
        PINFO("Can get prop %s (%d)\n", prop_name, ret);
    PINFO("%s = <%d>\n", prop_name, fuelgauge->info.value_v_alarm);

    // TOP OFF SOC
    snprintf(prop_name, PROPERTY_NAME_SIZE, "battery%d,%s", battery_id, "topoff_soc");
    ret = of_property_read_u32_array(np, prop_name, topoff_soc, 3);
    if (ret < 0)
        PINFO("Can get prop %s (%d)\n", prop_name, ret);
    fuelgauge->info.enable_topoff_soc = topoff_soc[0];
    fuelgauge->info.topoff_soc = topoff_soc[1];
    fuelgauge->info.top_off = topoff_soc[2];

    PINFO("%s = <%d %d %d>\n", prop_name,
        fuelgauge->info.enable_topoff_soc, fuelgauge->info.topoff_soc, fuelgauge->info.top_off);

    // SOC cycle cfg
    snprintf(prop_name, PROPERTY_NAME_SIZE, "battery%d,%s", battery_id, "cycle_cfg");
    ret = of_property_read_u32_array(np, prop_name, cycle_cfg, 3);
    if (ret < 0)
        PINFO("Can get prop %s (%d)\n", prop_name, ret);
    fuelgauge->info.cycle_high_limit = cycle_cfg[0];
    fuelgauge->info.cycle_low_limit = cycle_cfg[1];
    fuelgauge->info.cycle_limit_cntl = cycle_cfg[2];

    PINFO("%s = <%d %d %d>\n", prop_name,
        fuelgauge->info.cycle_high_limit, fuelgauge->info.cycle_low_limit, fuelgauge->info.cycle_limit_cntl);

    // v_offset_cancel
    snprintf(prop_name, PROPERTY_NAME_SIZE, "battery%d,%s", battery_id, "v_offset_cancel");
    ret = of_property_read_u32_array(np, prop_name, v_offset_cancel, 4);
    if (ret < 0)
        PINFO("Can get prop %s (%d)\n", prop_name, ret);
    fuelgauge->info.enable_v_offset_cancel_p = v_offset_cancel[0];
    fuelgauge->info.enable_v_offset_cancel_n = v_offset_cancel[1];
    fuelgauge->info.v_offset_cancel_level = v_offset_cancel[2];
    fuelgauge->info.v_offset_cancel_mohm = v_offset_cancel[3];

    PINFO("%s = <%d %d %d %d>\n", prop_name,
        fuelgauge->info.enable_v_offset_cancel_p, fuelgauge->info.enable_v_offset_cancel_n, fuelgauge->info.v_offset_cancel_level, fuelgauge->info.v_offset_cancel_mohm);   

    // VOL & CURR CAL
    snprintf(prop_name, PROPERTY_NAME_SIZE, "battery%d,%s", battery_id, "volt_cal");
    ret = of_property_read_u32_array(np, prop_name, &fuelgauge->info.volt_cal, 1);
    if (ret < 0)
        PINFO("Can get prop %s (%d)\n", prop_name, ret);
    PINFO("%s = <0x%x>\n", prop_name, fuelgauge->info.volt_cal);

    snprintf(prop_name, PROPERTY_NAME_SIZE, "battery%d,%s", battery_id, "curr_offset");
    ret = of_property_read_u32_array(np, prop_name, curr_offset, 7);
    if (ret < 0)
        PINFO("Can get prop %s (%d)\n", prop_name, ret);
    fuelgauge->info.en_auto_i_offset = curr_offset[0];
    fuelgauge->info.ecv_i_off = curr_offset[1];
    fuelgauge->info.csp_i_off = curr_offset[2];
    fuelgauge->info.csn_i_off = curr_offset[3];
    fuelgauge->info.dp_ecv_i_off = curr_offset[4];
    fuelgauge->info.dp_csp_i_off = curr_offset[5];
    fuelgauge->info.dp_csn_i_off = curr_offset[6];

    PINFO("%s = <%d 0x%x 0x%x 0x%x>\n", prop_name, fuelgauge->info.en_auto_i_offset, fuelgauge->info.ecv_i_off, fuelgauge->info.csp_i_off, fuelgauge->info.csn_i_off);

    snprintf(prop_name, PROPERTY_NAME_SIZE, "battery%d,%s", battery_id, "curr_cal");
    ret = of_property_read_u32_array(np, prop_name, curr_cal, 6);
    if (ret < 0)
        PINFO("Can get prop %s (%d)\n", prop_name, ret);
    fuelgauge->info.ecv_i_slo = curr_cal[0];
    fuelgauge->info.csp_i_slo = curr_cal[1];
    fuelgauge->info.csn_i_slo = curr_cal[2];
    fuelgauge->info.dp_ecv_i_slo = curr_cal[3];
    fuelgauge->info.dp_csp_i_slo = curr_cal[4];
    fuelgauge->info.dp_csn_i_slo = curr_cal[5];

    PINFO("%s = <0x%x 0x%x 0x%x>\n", prop_name, fuelgauge->info.ecv_i_slo, fuelgauge->info.csp_i_slo, fuelgauge->info.csn_i_slo);


#ifdef ENABLE_FULL_OFFSET
    snprintf(prop_name, PROPERTY_NAME_SIZE, "battery%d,%s", battery_id, "full_offset");
    ret = of_property_read_u32_array(np, prop_name, full_offset, 2);
    if (ret < 0)
        PINFO("Can get prop %s (%d)\n", prop_name, ret);
    fuelgauge->info.full_offset_margin = full_offset[0];
    fuelgauge->info.full_extra_offset = full_offset[1];

    PINFO("%s = <%d %d>\n", prop_name, fuelgauge->info.full_offset_margin, fuelgauge->info.full_extra_offset);  
#endif

    // temp_std
    snprintf(prop_name, PROPERTY_NAME_SIZE, "battery%d,%s", battery_id, "temp_std");
    ret = of_property_read_u32_array(np, prop_name, &fuelgauge->info.temp_std, 1);
    if (ret < 0)
        PINFO("Can get prop %s (%d)\n", prop_name, ret);
    PINFO("%s = <%d>\n", prop_name, fuelgauge->info.temp_std);

    // temp_volcal
    snprintf(prop_name, PROPERTY_NAME_SIZE, "battery%d,%s", battery_id, "temp_volcal");
    ret = of_property_read_u32_array(np, prop_name, temp_volcal, 3);
    if (ret < 0)
        PINFO("Can get prop %s (%d)\n", prop_name, ret);
    fuelgauge->info.en_fg_temp_volcal = temp_volcal[0];
    fuelgauge->info.fg_temp_volcal_denom = temp_volcal[1];
    fuelgauge->info.fg_temp_volcal_fact = temp_volcal[2];
    PINFO("%s = <%d, %d, %d>\n", prop_name,
        fuelgauge->info.en_fg_temp_volcal, fuelgauge->info.fg_temp_volcal_denom, fuelgauge->info.fg_temp_volcal_fact);

    // temp_offset
    snprintf(prop_name, PROPERTY_NAME_SIZE, "battery%d,%s", battery_id, "temp_offset");
    ret = of_property_read_u32_array(np, prop_name, temp_offset, 6);
    if (ret < 0)
        PINFO("Can get prop %s (%d)\n", prop_name, ret);
    fuelgauge->info.en_high_fg_temp_offset = temp_offset[0];
    fuelgauge->info.high_fg_temp_offset_denom = temp_offset[1];
    fuelgauge->info.high_fg_temp_offset_fact = temp_offset[2];
    fuelgauge->info.en_low_fg_temp_offset = temp_offset[3];
    fuelgauge->info.low_fg_temp_offset_denom = temp_offset[4];
    fuelgauge->info.low_fg_temp_offset_fact = temp_offset[5];
    PINFO("%s = <%d, %d, %d, %d, %d, %d>\n", prop_name,
        fuelgauge->info.en_high_fg_temp_offset, 
        fuelgauge->info.high_fg_temp_offset_denom, fuelgauge->info.high_fg_temp_offset_fact,
        fuelgauge->info.en_low_fg_temp_offset,
        fuelgauge->info.low_fg_temp_offset_denom, fuelgauge->info.low_fg_temp_offset_fact);

    // temp_calc
    snprintf(prop_name, PROPERTY_NAME_SIZE, "battery%d,%s", battery_id, "temp_cal");
    ret = of_property_read_u32_array(np, prop_name, temp_cal, 10);
    if (ret < 0)
        PINFO("Can get prop %s (%d)\n", prop_name, ret);
    fuelgauge->info.en_high_fg_temp_cal = temp_cal[0];
    fuelgauge->info.high_fg_temp_p_cal_denom = temp_cal[1];
    fuelgauge->info.high_fg_temp_p_cal_fact = temp_cal[2];
    fuelgauge->info.high_fg_temp_n_cal_denom = temp_cal[3];
    fuelgauge->info.high_fg_temp_n_cal_fact = temp_cal[4];
    fuelgauge->info.en_low_fg_temp_cal = temp_cal[5];
    fuelgauge->info.low_fg_temp_p_cal_denom = temp_cal[6];
    fuelgauge->info.low_fg_temp_p_cal_fact = temp_cal[7];
    fuelgauge->info.low_fg_temp_n_cal_denom = temp_cal[8];
    fuelgauge->info.low_fg_temp_n_cal_fact = temp_cal[9];
    PINFO("%s = <%d, %d, %d, %d, %d, %d, %d, %d, %d, %d>\n", prop_name,
        fuelgauge->info.en_high_fg_temp_cal, 
        fuelgauge->info.high_fg_temp_p_cal_denom, fuelgauge->info.high_fg_temp_p_cal_fact,
        fuelgauge->info.high_fg_temp_n_cal_denom, fuelgauge->info.high_fg_temp_n_cal_fact,
        fuelgauge->info.en_low_fg_temp_cal,
        fuelgauge->info.low_fg_temp_p_cal_denom, fuelgauge->info.low_fg_temp_p_cal_fact,
        fuelgauge->info.low_fg_temp_n_cal_denom, fuelgauge->info.low_fg_temp_n_cal_fact);

    // ext_temp_calc
    snprintf(prop_name, PROPERTY_NAME_SIZE, "battery%d,%s", battery_id, "ext_temp_cal");
    ret = of_property_read_u32_array(np, prop_name, ext_temp_cal, 10);
    if (ret < 0)
        PINFO("Can get prop %s (%d)\n", prop_name, ret);
    fuelgauge->info.en_high_temp_cal = ext_temp_cal[0];
    fuelgauge->info.high_temp_p_cal_denom = ext_temp_cal[1];
    fuelgauge->info.high_temp_p_cal_fact = ext_temp_cal[2];
    fuelgauge->info.high_temp_n_cal_denom = ext_temp_cal[3];
    fuelgauge->info.high_temp_n_cal_fact = ext_temp_cal[4];
    fuelgauge->info.en_low_temp_cal = ext_temp_cal[5];
    fuelgauge->info.low_temp_p_cal_denom = ext_temp_cal[6];
    fuelgauge->info.low_temp_p_cal_fact = ext_temp_cal[7];
    fuelgauge->info.low_temp_n_cal_denom = ext_temp_cal[8];
    fuelgauge->info.low_temp_n_cal_fact = ext_temp_cal[9];
    PINFO("%s = <%d, %d, %d, %d, %d, %d, %d, %d, %d, %d>\n", prop_name,
        fuelgauge->info.en_high_temp_cal, 
        fuelgauge->info.high_temp_p_cal_denom, fuelgauge->info.high_temp_p_cal_fact,
        fuelgauge->info.high_temp_n_cal_denom, fuelgauge->info.high_temp_n_cal_fact,
        fuelgauge->info.en_low_temp_cal,
        fuelgauge->info.low_temp_p_cal_denom, fuelgauge->info.low_temp_p_cal_fact,
        fuelgauge->info.low_temp_n_cal_denom, fuelgauge->info.low_temp_n_cal_fact);

    // tem poff level
    snprintf(prop_name, PROPERTY_NAME_SIZE, "battery%d,%s", battery_id, "tem_poff");
    ret = of_property_read_u32_array(np, prop_name, set_temp_poff, 4);
    if (ret < 0)
        PINFO("Can get prop %s (%d)\n", prop_name, ret);
    fuelgauge->info.n_tem_poff = set_temp_poff[0];
    fuelgauge->info.n_tem_poff_offset = set_temp_poff[1];
    fuelgauge->info.l_tem_poff = set_temp_poff[2];
    fuelgauge->info.l_tem_poff_offset = set_temp_poff[3];

    PINFO("%s = <%d, %d, %d, %d>\n",
        prop_name,
        fuelgauge->info.n_tem_poff, fuelgauge->info.n_tem_poff_offset,
        fuelgauge->info.l_tem_poff, fuelgauge->info.l_tem_poff_offset);

    // arsm setting
    snprintf(prop_name, PROPERTY_NAME_SIZE, "battery%d,%s", battery_id, "arsm");
    ret = of_property_read_u32_array(np, prop_name, arsm, 4);
    if (ret < 0)
        PINFO("Can get prop %s (%d)\n", prop_name, ret);
    fuelgauge->info.arsm[0] = arsm[0];
    fuelgauge->info.arsm[1] = arsm[1];
    fuelgauge->info.arsm[2] = arsm[2];
    fuelgauge->info.arsm[3] = arsm[3];

    PINFO("%s = <%d, %d, %d, %d>\n",
        prop_name,
        fuelgauge->info.arsm[0], fuelgauge->info.arsm[1],
        fuelgauge->info.arsm[2], fuelgauge->info.arsm[3]);

    // batt data version
    snprintf(prop_name, PROPERTY_NAME_SIZE, "battery%d,%s", battery_id, "data_ver");
    ret = of_property_read_u32_array(np, prop_name, &fuelgauge->info.data_ver, 1);
    if (ret < 0)
        PINFO("Can get prop %s (%d)\n", prop_name, ret);
    PINFO("%s = <%d>\n", prop_name, fuelgauge->info.data_ver);




    return 0;
}
#endif

static int sm5720_fuelgauge_probe(struct platform_device *pdev)
{
    struct sm5720_dev *sm5720 = dev_get_drvdata(pdev->dev.parent);
    struct sm5720_platform_data *pdata = dev_get_platdata(sm5720->dev);
    struct sm5720_fuelgauge_data *fuelgauge;
    int ret = 0;
    union power_supply_propval raw_soc_val;
#if defined(CONFIG_DISABLE_SAVE_CAPACITY_MAX)   
    u16 reg_data;
#endif

    pr_info("%s: SM5720 Fuelgauge Driver Loading\n", __func__);

    fuelgauge = kzalloc(sizeof(*fuelgauge), GFP_KERNEL);
    if (!fuelgauge)
        return -ENOMEM;

    pdata->fuelgauge_data = kzalloc(sizeof(sec_fuelgauge_platform_data_t), GFP_KERNEL);
    if (!pdata->fuelgauge_data) {
        ret = -ENOMEM;
        goto err_free;
    }

    mutex_init(&fuelgauge->fg_lock);

    fuelgauge->dev = &pdev->dev;
    fuelgauge->pdata = pdata->fuelgauge_data;
    fuelgauge->i2c = sm5720->fuelgauge;
    //fuelgauge->pmic = sm5720->i2c;
    fuelgauge->sm5720_pdata = pdata;

#if defined(CONFIG_OF)
    fuelgauge->battery_data = kzalloc(sizeof(struct battery_data_t),
                      GFP_KERNEL);
    if(!fuelgauge->battery_data) {
        pr_err("Failed to allocate memory\n");
        ret = -ENOMEM;
        goto err_pdata_free;
    }
    ret = sm5720_fuelgauge_parse_dt(fuelgauge);
    if (ret < 0) {
        pr_err("%s not found charger dt! ret[%d]\n",
               __func__, ret);
    }
#endif

    platform_set_drvdata(pdev, fuelgauge);

    fuelgauge->psy_fg.name      = "sm5720-fuelgauge";
    fuelgauge->psy_fg.type      = POWER_SUPPLY_TYPE_UNKNOWN;
    fuelgauge->psy_fg.get_property  = sm5720_fg_get_property;
    fuelgauge->psy_fg.set_property  = sm5720_fg_set_property;
    fuelgauge->psy_fg.properties    = sm5720_fuelgauge_props;
    fuelgauge->psy_fg.num_properties =
        ARRAY_SIZE(sm5720_fuelgauge_props);
    fuelgauge->capacity_max = fuelgauge->pdata->capacity_max;
    raw_soc_val.intval = sm5720_get_soc(fuelgauge);

#if defined(CONFIG_DISABLE_SAVE_CAPACITY_MAX)
    pr_info("%s : CONFIG_DISABLE_SAVE_CAPACITY_MAX\n",
        __func__);
#endif

    if (raw_soc_val.intval > fuelgauge->capacity_max)
        sm5720_fg_calculate_dynamic_scale(fuelgauge, 100);

    (void) debugfs_create_file("sm5720-fuelgauge-regs",
        S_IRUGO, NULL, (void *)fuelgauge, &sm5720_fuelgauge_debugfs_fops);

    if (!sm5720_fg_init(fuelgauge)) {
        pr_err("%s: Failed to Initialize Fuelgauge\n", __func__);
        goto err_data_free;
    }

    ret = power_supply_register(&pdev->dev, &fuelgauge->psy_fg);
    if (ret) {
        pr_err("%s: Failed to Register psy_fg\n", __func__);
        goto err_data_free;
    }

    fuelgauge->fg_irq = pdata->irq_base + SM5720_FG_IRQ_INT_LOW_VOLTAGE;
    pr_info("[%s]IRQ_BASE(%d) FG_IRQ(%d)\n",
        __func__, pdata->irq_base, fuelgauge->fg_irq);

    fuelgauge->is_fuel_alerted = false;
    if (fuelgauge->pdata->fuel_alert_soc >= 0) {
        if (sm5720_fg_fuelalert_init(fuelgauge,
                       fuelgauge->pdata->fuel_alert_soc)) {
            wake_lock_init(&fuelgauge->fuel_alert_wake_lock,
                       WAKE_LOCK_SUSPEND, "fuel_alerted");
            if (fuelgauge->fg_irq) {
                INIT_DELAYED_WORK(&fuelgauge->isr_work, sm5720_fg_isr_work);

                ret = request_threaded_irq(fuelgauge->fg_irq,
                       NULL, sm5720_fg_irq_thread,
                       IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
                       "fuelgauge-irq", fuelgauge);
                if (ret) {
                    pr_err("%s: Failed to Request IRQ\n", __func__);
                    goto err_supply_unreg;
                }
            }
        } else {
            pr_err("%s: Failed to Initialize Fuel-alert\n",
                   __func__);
            goto err_supply_unreg;
        }
    }

    fuelgauge->hw_v_empty = false;
    fuelgauge->initial_update_of_soc = true;
    fuelgauge->low_temp_compensation_en = false;
    fuelgauge->sw_v_empty = NORMAL_MODE;

    pr_info("%s: SM5720 Fuelgauge Driver Loaded\n", __func__);
    return 0;

err_supply_unreg:
    power_supply_unregister(&fuelgauge->psy_fg);
err_data_free:
#if defined(CONFIG_OF)
    kfree(fuelgauge->battery_data);
#endif
err_pdata_free:
    kfree(pdata->fuelgauge_data);
    mutex_destroy(&fuelgauge->fg_lock);
err_free:
    kfree(fuelgauge);

    return ret;
}

static int sm5720_fuelgauge_remove(struct platform_device *pdev)
{
    struct sm5720_fuelgauge_data *fuelgauge =
        platform_get_drvdata(pdev);

    if (fuelgauge->pdata->fuel_alert_soc >= 0)
        wake_lock_destroy(&fuelgauge->fuel_alert_wake_lock);

    return 0;
}

static int sm5720_fuelgauge_suspend(struct device *dev)
{
    return 0;
}

static int sm5720_fuelgauge_resume(struct device *dev)
{
    struct sm5720_fuelgauge_data *fuelgauge = dev_get_drvdata(dev);

    fuelgauge->initial_update_of_soc = true;

    return 0;
}

static void sm5720_fuelgauge_shutdown(struct device *dev)
{
    struct sm5720_fuelgauge_data *fuelgauge = dev_get_drvdata(dev);

    if (fuelgauge->using_hw_vempty)
        sm5720_fg_set_vempty(fuelgauge, false);
}

static SIMPLE_DEV_PM_OPS(sm5720_fuelgauge_pm_ops, sm5720_fuelgauge_suspend,
             sm5720_fuelgauge_resume);

static struct platform_driver sm5720_fuelgauge_driver = {
    .driver = {
           .name = "sm5720-fuelgauge",
           .owner = THIS_MODULE,
#ifdef CONFIG_PM
           .pm = &sm5720_fuelgauge_pm_ops,
#endif
        .shutdown = sm5720_fuelgauge_shutdown,
    },
    .probe  = sm5720_fuelgauge_probe,
    .remove = sm5720_fuelgauge_remove,
};

static int __init sm5720_fuelgauge_init(void)
{
    pr_info("%s: \n", __func__);
    return platform_driver_register(&sm5720_fuelgauge_driver);
}

static void __exit sm5720_fuelgauge_exit(void)
{
    platform_driver_unregister(&sm5720_fuelgauge_driver);
}
module_init(sm5720_fuelgauge_init);
module_exit(sm5720_fuelgauge_exit);

MODULE_DESCRIPTION("Samsung SM5720 Fuel Gauge Driver");
MODULE_AUTHOR("Samsung Electronics");
MODULE_LICENSE("GPL");
