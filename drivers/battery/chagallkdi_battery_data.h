/*
 * battery_data.h
 * Samsung Mobile Battery data Header
 *
 *
 * Copyright (C) 2012 Samsung Electronics, Inc.
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
#ifndef __SEC_BATTERY_DATA_H
#define __SEC_BATTERY_DATA_H __FILE__

static struct battery_data_t samsung_battery_data[] = {
	/* SDI battery data (High voltage 4.35V) */
	{
		.RCOMP0 = 0x80,
		.RCOMP_charging = 0x9A,
		.temp_cohot = -762,
		.temp_cocold = -5562,
		.is_using_model_data = true,
		.type_str = "SDI",
	}
};

#define CAPACITY_MAX			970
#define CAPACITY_MAX_MARGIN	50
#define CAPACITY_MIN			0


static sec_bat_adc_table_data_t temp_table[] = {
	{24947, 800},
	{24958, 750},
	{24970, 700},
	{25059, 650},
	{25242, 600},
	{25342, 550},
	{25431, 500},
	{25563, 450},
	{25709, 400},
	{25891, 350},
	{26104, 300},
	{26373, 250},
	{26685, 200},
	{27064, 150},
	{27526, 100},
	{28060, 50},
	{28645, 0},
	{29436, -50},
	{30294, -100},
	{31378, -150},
	{32390, -200},
};

static sec_bat_adc_table_data_t chg_temp_table[] = {
	{0, 0},
};

#define TEMP_HIGH_THRESHOLD_EVENT       560
#define TEMP_HIGH_RECOVERY_EVENT        480
#define TEMP_LOW_THRESHOLD_EVENT        -50
#define TEMP_LOW_RECOVERY_EVENT         0
#define TEMP_HIGH_THRESHOLD_NORMAL      560
#define TEMP_HIGH_RECOVERY_NORMAL       480
#define TEMP_LOW_THRESHOLD_NORMAL       -50
#define TEMP_LOW_RECOVERY_NORMAL        0
#define TEMP_HIGH_THRESHOLD_LPM         560
#define TEMP_HIGH_RECOVERY_LPM          480
#define TEMP_LOW_THRESHOLD_LPM          10
#define TEMP_LOW_RECOVERY_LPM           25

#define TEMP_HIGHLIMIT_THRESHOLD_EVENT          800
#define TEMP_HIGHLIMIT_RECOVERY_EVENT           750
#define TEMP_HIGHLIMIT_THRESHOLD_NORMAL         800
#define TEMP_HIGHLIMIT_RECOVERY_NORMAL          750
#define TEMP_HIGHLIMIT_THRESHOLD_LPM            800
#define TEMP_HIGHLIMIT_RECOVERY_LPM             750

#endif /* __SEC_BATTERY_DATA_H */
