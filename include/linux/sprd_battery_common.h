/*
 * Copyright (C) 2012 Spreadtrum Communications Inc.
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
#include <linux/types.h>
#include <linux/power_supply.h>

struct sprdbat_table_data {
	int x;
	int y;
};

struct sprd_battery_platform_data {
	uint32_t chg_end_vol_h;	//charging
	uint32_t chg_end_vol_l;
	uint32_t chg_end_vol_pure;
	uint32_t chg_bat_safety_vol;
	uint32_t rechg_vol;
	uint32_t adp_cdp_cur;
	uint32_t adp_dcp_cur;
	uint32_t adp_sdp_cur;
	uint32_t ovp_stop;	//opt param
	uint32_t ovp_restart;
	uint32_t chg_timeout;
	uint32_t chgtimeout_show_full;	// when charging :1:timeout display 100%, 0 : timeout NOT display 100%
	uint32_t chg_rechg_timeout;
	uint32_t chg_cv_timeout;	//cv charging timeout
	uint32_t chg_eoc_level;
	uint32_t cccv_default;
	int chg_end_cur;
	int otp_high_stop;	//ovp param
	int otp_high_restart;
	int otp_low_stop;
	int otp_low_restart;
	uint32_t chg_polling_time;
	uint32_t chg_polling_time_fast;
	uint32_t bat_polling_time;
	uint32_t bat_polling_time_fast;
	uint32_t bat_polling_time_sleep;	//reserved
	uint32_t cap_one_per_time;
	uint32_t cap_one_per_time_fast;	//reserved
	int cap_valid_range_poweron;

	//temp param
	int temp_support;	/*0:do NOT support temperature,1:support temperature */
	int temp_adc_ch;
	int temp_adc_scale;
	int temp_adc_sample_cnt;	/*1-15,should be lower than 16 */
	int temp_table_mode;	//0:adc-degree;1:voltage-degree
	struct sprdbat_table_data *temp_tab;	/* OCV curve adjustment */
	int temp_tab_size;

	//resource
	uint32_t gpio_vchg_detect;
	uint32_t gpio_vbat_detect;	//reserved
	uint32_t gpio_cv_state;
	uint32_t gpio_vchg_ovi;
	uint32_t gpio_chg_en;	//reserved
	uint32_t irq_chg_timer;
	uint32_t irq_fgu;
	uint32_t chg_reg_base;
	uint32_t fgu_reg_base;

	//fgu param
	uint32_t fgu_mode;	/* 1=voltage mode, 0=mixed mode */
	uint32_t alm_soc;	/* SOC alm level % */
	uint32_t alm_vol;	/* Vbat alm level mV */
	uint32_t soft_vbat_uvlo;	/*reserved, if vbat voltage lower than it, should shutdown device */
	uint32_t soft_vbat_ovp;	/*reserved,if battery voltage higher than it maybe shutdown device */
	int rint;		/*battery internal impedance */
	int cnom;		/* nominal capacity in mAh */
	int rsense_real;	/* sense resistor 0.2mOhm from real environment */
	int rsense_spec;	/* sense resistor 0.2mOhm from specification */
	uint32_t relax_current;	/* current for relaxation in mA */
	int fgu_cal_ajust;	//reserved
	int qmax_update_period;	/*reserved */
	struct sprdbat_table_data *cnom_temp_tab;	/* capacity temp table */
	int cnom_temp_tab_size;
	struct sprdbat_table_data *rint_temp_tab;	/* impedance temp table */
	int rint_temp_tab_size;
	struct sprdbat_table_data *ocv_tab;	/* OCV curve adjustment */
	int ocv_tab_size;
	void *ext_data;		/*reserved, other battery data,fg ic data,and so on */
};
