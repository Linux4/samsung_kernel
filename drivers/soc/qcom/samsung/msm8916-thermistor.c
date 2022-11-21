/*
 * msm8916-thermistor.c - thermistor for MSM8916 projects
 *
 * Copyright (C) 2014 Samsung Electronics
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

#include <linux/msm8916-thermistor.h>
#include <linux/sec_thermistor.h>

#ifdef CONFIG_SEC_THERMISTOR
/* default table from KLTE(MSM8974Pro) project */
static struct sec_therm_adc_table temper_table_ap[] = {
	{25954,	900},
	{26005,	890},
	{26052,	880},
	{26105,	870},
	{26151,	860},
	{26207,	850},
	{26253,	840},
	{26302,	830},
	{26354,	820},
	{26405,	810},
	{26454,	800},
	{26503,	790},
	{26554,	780},
	{26602,	770},
	{26657,	760},
	{26691,	750},
	{26757,	740},
	{26823,	730},
	{26889,	720},
	{26955,	710},
	{27020,	700},
	{27081,	690},
	{27142,	680},
	{27203,	670},
	{27264,	660},
	{27327,	650},
	{27442,	640},
	{27557,	630},
	{27672,	620},
	{27787,	610},
	{27902,	600},
	{28004,	590},
	{28106,	580},
	{28208,	570},
	{28310,	560},
	{28415,	550},
	{28608,	540},
	{28801,	530},
	{28995,	520},
	{28944,	510},
	{28893,	500},
	{29148,	490},
	{29347,	480},
	{29546,	470},
	{29746,	460},
	{29911,	450},
	{30076,	440},
	{30242,	430},
	{30490,	420},
	{30738,	410},
	{30986,	400},
	{31101,	390},
	{31216,	380},
	{31331,	370},
	{31446,	360},
	{31561,	350},
	{31768,	340},
	{31975,	330},
	{32182,	320},
	{32389,	310},
	{32596,	300},
	{32962,	290},
	{32974,	280},
	{32986,	270},
	{33744,	260},
	{33971,	250},
	{34187,	240},
	{34403,	230},
	{34620,	220},
	{34836,	210},
	{35052,	200},
	{35261,	190},
	{35470,	180},
	{35679,	170},
	{35888,	160},
	{36098,	150},
	{36317,	140},
	{36537,	130},
	{36756,	120},
	{36976,	110},
	{37195,	100},
	{37413,	90},
	{37630,	80},
	{37848,	70},
	{38065,	60},
	{38282,	50},
	{38458,	40},
	{38635,	30},
	{38811,	20},
	{38987,	10},
	{39163,	0},
	{39317,	-10},
	{39470,	-20},
	{39624,	-30},
	{39777,	-40},
	{39931,	-50},
	{40065,	-60},
	{40199,	-70},
	{40333,	-80},
	{40467,	-90},
	{40601,	-100},
	{40728,	-110},
	{40856,	-120},
	{40983,	-130},
	{41110,	-140},
	{41237,	-150},
	{41307,	-160},
	{41378,	-170},
	{41448,	-180},
	{41518,	-190},
	{41588,	-200},
};

static struct sec_therm_platform_data sec_therm_pdata = {
	.adc_arr_size	= ARRAY_SIZE(temper_table_ap),
	.adc_table	= temper_table_ap,
	.polling_interval = 30 * 1000, /* msecs */
	.get_siop_level = NULL,
	.no_polling	= 1,
};


struct platform_device sec_device_thermistor = {
	.name = "sec-thermistor",
	.id = -1,
	.dev.platform_data = &sec_therm_pdata,
};

struct sec_therm_platform_data * fill_therm_pdata(struct platform_device *pdev)
{
	pdev->dev.platform_data = &sec_therm_pdata;
	pdev->id = -1;

	return &sec_therm_pdata;
}
#endif
