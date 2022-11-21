/*
 * battery_data.h
 * Samsung Mobile Battery data Header
 *
 *
 * Copyright (C) 2014 Samsung Electronics, Inc.
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

#define CAPACITY_MAX			1000
#define CAPACITY_MAX_MARGIN		70
#define CAPACITY_MIN			0

static sec_bat_adc_table_data_t temp_table[] = {
	{26007, 900},
	{26223, 850},
	{26475, 800},
	{26803, 750},
	{27167, 700},
	{27564, 650},
	{28091, 600},
	{28618, 550},
	{29259, 500},
	{30066, 450},
	{30909, 400},
	{31840, 350},
	{32849, 300},
	{33896, 250},
	{34968, 200},
	{36050, 150},
	{37090, 100},
	{38065, 50},
	{38928, 0},
	{39695, -50},
	{40513, -100},
	{41113, -150},
	{41606, -200},
};

static sec_bat_adc_table_data_t chg_temp_table[] = {
	{26007, 900},
	{26223, 850},
	{26475, 800},
	{26803, 750},
	{27167, 700},
	{27564, 650},
	{28091, 600},
	{28618, 550},
	{29259, 500},
	{30066, 450},
	{30909, 400},
	{31840, 350},
	{32849, 300},
	{33896, 250},
	{34968, 200},
	{36050, 150},
	{37090, 100},
	{38065, 50},
	{38928, 0},
	{39695, -50},
	{40513, -100},
	{41113, -150},
	{41606, -200},
};

#if defined(CONFIG_BATTERY_SWELLING)
#define BATT_SWELLING_HIGH_TEMP_BLOCK		450
#define BATT_SWELLING_HIGH_TEMP_RECOV		400
#define BATT_SWELLING_LOW_TEMP_BLOCK		50
#define BATT_SWELLING_LOW_TEMP_RECOV		100
#define BATT_SWELLING_RECHG_VOLTAGE		4150
#endif

#define TEMP_HIGHLIMIT_THRESHOLD	800
#define TEMP_HIGHLIMIT_RECOVERY		700

#define TEMP_HIGH_THRESHOLD_EVENT  550
#define TEMP_HIGH_RECOVERY_EVENT   500
#define TEMP_LOW_THRESHOLD_EVENT   (-50)
#define TEMP_LOW_RECOVERY_EVENT    0
#define TEMP_HIGH_THRESHOLD_NORMAL 550
#define TEMP_HIGH_RECOVERY_NORMAL  500
#define TEMP_LOW_THRESHOLD_NORMAL  (-50)
#define TEMP_LOW_RECOVERY_NORMAL   0
#define TEMP_HIGH_THRESHOLD_LPM    550
#define TEMP_HIGH_RECOVERY_LPM     500
#define TEMP_LOW_THRESHOLD_LPM     (-50)
#define TEMP_LOW_RECOVERY_LPM      0

#endif /* __SEC_BATTERY_DATA_H */
