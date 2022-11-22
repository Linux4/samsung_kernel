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
#define BATT_SWELLING_HIGH_TEMP_BLOCK		410
#define BATT_SWELLING_HIGH_TEMP_RECOV		360
#define BATT_SWELLING_LOW_TEMP_BLOCK		100
#define BATT_SWELLING_LOW_TEMP_RECOV		150
#define BATT_SWELLING_RECHG_VOLTAGE		4100
#endif

#define TEMP_HIGHLIMIT_THRESHOLD	800
#define TEMP_HIGHLIMIT_RECOVERY		700

#define TEMP_HIGH_THRESHOLD_EVENT  500
#define TEMP_HIGH_RECOVERY_EVENT   450
#define TEMP_LOW_THRESHOLD_EVENT   0
#define TEMP_LOW_RECOVERY_EVENT    50
#define TEMP_HIGH_THRESHOLD_NORMAL 500
#define TEMP_HIGH_RECOVERY_NORMAL  450
#define TEMP_LOW_THRESHOLD_NORMAL  0
#define TEMP_LOW_RECOVERY_NORMAL   50
#define TEMP_HIGH_THRESHOLD_LPM    500
#define TEMP_HIGH_RECOVERY_LPM     450
#define TEMP_LOW_THRESHOLD_LPM     0
#define TEMP_LOW_RECOVERY_LPM      50

#endif /* __SEC_BATTERY_DATA_H */
