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

#if defined(CONFIG_FUELGAUGE_MAX17048)
static struct max17048_fuelgauge_battery_data_t max17048_battery_data[] = {
	/* SDI battery data (High voltage 4.35V) */
	{
		.RCOMP0 = 0x5D,
		.RCOMP_charging = 0x5D,
		.temp_cohot = -175,
		.temp_cocold = -5825,
		.is_using_model_data = true,
		.type_str = "SDI",
	}
};
#endif

#if defined(CONFIG_FUELGAUGE_MAX77823)
static struct max77823_fuelgauge_battery_data_t max77823_battery_data[] = {
	/* SDI battery data (High voltage 4.4V) */
	{
		.Capacity = 0x187E, /* TR/TB : 3220mAh */
		.low_battery_comp_voltage = 3500,
		.low_battery_table = {
			/* range, slope, offset */
			{-5000,	0,	0},	/* dummy for top limit */
			{-1250, 0,	3320},
			{-750, 97,	3451},
			{-100, 96,	3461},
			{0, 0,	3456},
		},
		.temp_adjust_table = {
			/* range, slope, offset */
			{47000, 122,	8950},
			{60000, 200,	51000},
			{100000, 0,	0},	/* dummy for top limit */
		},
		.type_str = "SDI",
	}
};
#endif

#define CAPACITY_MAX			990
#define CAPACITY_MAX_MARGIN	50
#define CAPACITY_MIN			-7

static sec_bat_adc_table_data_t temp_table[] = {
	{26009, 900},
	{26280, 850},
	{26600, 800},
	{26950, 750},
	{27325, 700},
	{27737, 650},
	{28180, 600},
	{28699, 550},
	{29360, 500},
	{29970, 450},
	{30995, 400},
	{32046, 350},
	{32985, 300},
	{34050, 250},
	{35139, 200},
	{36179, 150},
	{37208, 100},
	{38237, 50},
	{38414, 40},
	{38598, 30},
	{38776, 20},
	{38866, 10},
	{38956, 0},
	{39102, -10},
	{39247, -20},
	{39393, -30},
	{39538, -40},
	{39684, -50},
	{40490, -100},
	{41187, -150},
	{41652, -200},
	{42030, -250},
	{42327, -300},
};

static sec_bat_adc_table_data_t chg_temp_table[] = {
	{0, 0},
};

#define TEMP_HIGH_THRESHOLD_EVENT	600
#define TEMP_HIGH_RECOVERY_EVENT		460
#define TEMP_LOW_THRESHOLD_EVENT		-50
#define TEMP_LOW_RECOVERY_EVENT		0
#define TEMP_HIGH_THRESHOLD_NORMAL	600
#define TEMP_HIGH_RECOVERY_NORMAL	460
#define TEMP_LOW_THRESHOLD_NORMAL	-50
#define TEMP_LOW_RECOVERY_NORMAL	0
#define TEMP_HIGH_THRESHOLD_LPM		600
#define TEMP_HIGH_RECOVERY_LPM		460
#define TEMP_LOW_THRESHOLD_LPM		-50
#define TEMP_LOW_RECOVERY_LPM		0

#endif /* __SEC_BATTERY_DATA_H */
