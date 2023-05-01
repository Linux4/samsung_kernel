/*
 * midas-thermistor.c - thermistor of MIDAS Project
 *
 * Copyright (C) 2011 Samsung Electrnoics
 * SangYoung Son <hello.son@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <mach/midas-thermistor.h>
#include <mach/sec_thermistor.h>


#ifdef CONFIG_SEC_THERMISTOR
/*Below adc table is same as batt_temp_adc table*/
#if defined(CONFIG_MACH_M2_ATT)
static struct sec_therm_adc_table temper_table_ap[] = {
	{26465,	 800},
	{26749,	 750},
	{27017,	 700},
	{27414,	 650},
	{27870,	 600},
	{28424,	 550},
	{29350,	 500},
	{29831,	 450},
	{30595,	 400},
	{31561,	 350},
	{32603,	 300},
	{33647,	 250},
	{34655,	 200},
	{35741,	 150},
	{36747,	 100},
	{37755,	  50},
	{38605,	   0},
	{39412,	 -50},
	{40294,	-100},
	{40845,	-150},
	{41353,	-200},
};

#elif defined(CONFIG_MACH_M2_SPR)
static struct sec_therm_adc_table temper_table_ap[] = {
	{26385,	 800},
	{26787,	 750},
	{27136,	 700},
	{27540,	 650},
	{28031,	 600},
	{28601,	 550},
	{29255,	 500},
	{29568,	 450},
	{30967,	 400},
	{31880,	 350},
	{32846,	 300},
	{33694,	 250},
	{34771,	 200},
	{35890,	 150},
	{37045,	 100},
	{38144,	  50},
	{39097,	   0},
	{39885,	 -50},
	{40595,	-100},
	{41190,	-150},
	{41954,	-200},
};

#elif defined(CONFIG_MACH_M2_VZW)
static struct sec_therm_adc_table temper_table_ap[] = {
	{26582,	 800},
	{26932,	 750},
	{27307,	 700},
	{27764,	 650},
	{28294,	 600},
	{28929,	 550},
	{29390,	 500},
	{30294,	 450},
	{31032,	 400},
	{31964,	 350},
	{32630,	 300},
};
#endif

#if 0
static struct sec_therm_adc_table temper_table_ap[] = {
	{196,	700},
	{211,	690},
	{242,	685},
	{249,	680},
	{262,	670},
	{275,	660},
	{288,	650},
	{301,	640},
	{314,	630},
	{328,	620},
	{341,	610},
	{354,	600},
	{366,	590},
	{377,	580},
	{389,	570},
	{404,	560},
	{419,	550},
	{434,	540},
	{452,	530},
	{469,	520},
	{487,	510},
	{498,	500},
	{509,	490},
	{520,	480},
	{529,	460},
	{538,	470},
	{547,	450},
	{556,	440},
	{564,	430},
	{573,	420},
	{581,	410},
	{590,	400},
	{615,	390},
	{640,	380},
	{665,	370},
	{690,	360},
	{715,	350},
	{736,	340},
	{758,	330},
	{779,	320},
	{801,	310},
	{822,	300},
};
#endif

/* when the next level is same as prev, returns -1 */
static int get_midas_siop_level(int temp)
{
	static int prev_temp = 400;
	static int prev_level = 0;
	int level = -1;

#if defined(CONFIG_MACH_C1_KOR_SKT) || defined(CONFIG_MACH_C1_KOR_KT) || \
	defined(CONFIG_MACH_C1_KOR_LGT)
	if (temp > prev_temp) {
		if (temp >= 490)
			level = 4;
		else if (temp >= 480)
			level = 3;
		else if (temp >= 450)
			level = 2;
		else if (temp >= 420)
			level = 1;
		else
			level = 0;
	} else {
		if (temp < 400)
			level = 0;
		else if (temp < 420)
			level = 1;
		else if (temp < 450)
			level = 2;
		else if (temp < 480)
			level = 3;
		else
			level = 4;

		if (level > prev_level)
			level = prev_level;
	}
#elif defined(CONFIG_MACH_P4NOTE)
	if (temp > prev_temp) {
		if (temp >= 620)
			level = 4;
		else if (temp >= 610)
			level = 3;
		else if (temp >= 580)
			level = 2;
		else if (temp >= 550)
			level = 1;
		else
			level = 0;
	} else {
		if (temp < 520)
			level = 0;
		else if (temp < 550)
			level = 1;
		else if (temp < 580)
			level = 2;
		else if (temp < 610)
			level = 3;
		else
			level = 4;

		if (level > prev_level)
			level = prev_level;
	}
#else
	if (temp > prev_temp) {
		if (temp >= 540)
			level = 4;
		else if (temp >= 530)
			level = 3;
		else if (temp >= 480)
			level = 2;
		else if (temp >= 440)
			level = 1;
		else
			level = 0;
	} else {
		if (temp < 410)
			level = 0;
		else if (temp < 440)
			level = 1;
		else if (temp < 480)
			level = 2;
		else if (temp < 530)
			level = 3;
		else
			level = 4;

		if (level > prev_level)
			level = prev_level;
	}
#endif

	prev_temp = temp;
	if (prev_level == level)
		return -1;

	prev_level = level;

	return level;
}

static struct sec_therm_platform_data sec_therm_pdata = {
	//.adc_channel	= 1,
	.adc_arr_size	= ARRAY_SIZE(temper_table_ap),
	.adc_table	= temper_table_ap,
	.polling_interval = 30 * 1000, /* msecs */
	.get_siop_level = get_midas_siop_level,
	.no_polling     = 1,
};

struct platform_device sec_device_thermistor = {
	.name = "sec-thermistor",
	.id = -1,
	.dev.platform_data = &sec_therm_pdata,
};

#endif
