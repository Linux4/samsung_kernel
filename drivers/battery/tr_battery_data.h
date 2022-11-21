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

#if defined(CONFIG_SEC_TRLTE_PROJECT)
static struct max77843_fuelgauge_battery_data_t max77843_battery_data[] = {
	/* SDI battery data (High voltage 4.4V) */
	{
#if defined(CONFIG_SEC_TRLTE_CHNDUOS)
		.rcomp0 = 0x0038,
		.rcomp_charging = 0x0038,
		.QResidual20 = 0x1007,
		.QResidual30 = 0x0B08,
		.Capacity = 0x16E3, /* TR CHN: 2929mAh */
#else
		.rcomp0 = 0x0063,
		.rcomp_charging = 0x0063,
		.QResidual20 = 0x0900,
		.QResidual30 = 0x0780,
		.Capacity = 0x19C8, /* TR : 3300mAh */
#endif
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
/* TBLTE_PROJECT */
#else
static struct max77843_fuelgauge_battery_data_t max77843_battery_data[] = {
	/* SDI battery data (High voltage 4.4V) */
	{
		.rcomp0 = 0x0059,
		.rcomp_charging = 0x0059,
		.QResidual20 = 0x0A80,
		.QResidual30 = 0x0782,
		.Capacity = 0x184C, /* TB : 3110mAh */
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

#if defined(CONFIG_SEC_TRLTE_PROJECT)
static sec_bat_adc_table_data_t temp_table[] = {
	{27067, 900},
	{27398, 850},
	{27771, 800},
	{28177, 750},
	{28598, 700},
	{29076, 650},
	{29601, 600},
	{30218, 550},
	{30836, 500},
	{31509, 450},
	{32098, 400},
	{32747, 350},
	{33370, 300},
	{33939, 250},
	{34463, 200},
	{34931, 150},
	{35340, 100},
	{35626, 50},
	{35684, 40},
	{35747, 30},
	{35805, 20},
	{35864, 10},
	{35918, 0},
	{35976, -10},
	{36028, -20},
	{36031, -30},
	{36130, -40},
	{36170, -50},
	{36419, -100},
	{36563, -150},
	{36689, -200},
	{36814, -250},
	{36873, -300},
};

static sec_bat_adc_table_data_t chg_temp_table[] = {
	{25954, 900},
	{26005, 890},
	{26052, 880},
	{26105, 870},
	{26151, 860},
	{26207, 850},
	{26253, 840},
	{26302, 830},
	{26354, 820},
	{26405, 810},
	{26454, 800},
	{26503, 790},
	{26554, 780},
	{26602, 770},
	{26657, 760},
	{26691, 750},
	{26757, 740},
	{26823, 730},
	{26889, 720},
	{26955, 710},
	{27020, 700},
	{27081, 690},
	{27142, 680},
	{27203, 670},
	{27264, 660},
	{27327, 650},
	{27442, 640},
	{27557, 630},
	{27672, 620},
	{27787, 610},
	{27902, 600},
	{28004, 590},
	{28106, 580},
	{28208, 570},
	{28310, 560},
	{28415, 550},
	{28608, 540},
	{28716, 530},
	{28824, 520},
	{28932, 510},
	{29040, 500},
	{29148, 490},
	{29347, 480},
	{29546, 470},
	{29746, 460},
	{29911, 450},
	{30076, 440},
	{30242, 430},
	{30490, 420},
	{30738, 410},
	{30986, 400},
	{31101, 390},
	{31216, 380},
	{31331, 370},
	{31446, 360},
	{31561, 350},
	{31768, 340},
	{31975, 330},
	{32182, 320},
	{32389, 310},
	{32596, 300},
	{32962, 290},
	{32974, 280},
	{32986, 270},
	{33744, 260},
	{33971, 250},
	{34187, 240},
	{34403, 230},
	{34620, 220},
	{34836, 210},
	{35052, 200},
	{35261, 190},
	{35470, 180},
	{35679, 170},
	{35888, 160},
	{36098, 150},
	{36317, 140},
	{36537, 130},
	{36756, 120},
	{36976, 110},
	{37195, 100},
	{37413, 90},
	{37630, 80},
	{37848, 70},
	{38065, 60},
	{38282, 50},
	{38458, 40},
	{38635, 30},
	{38811, 20},
	{38987, 10},
	{39163, 0},
	{39317, -10},
	{39470, -20},
	{39624, -30},
	{39777, -40},
	{39931, -50},
	{40065, -60},
	{40199, -70},
	{40333, -80},
	{40467, -90},
	{40601, -100},
	{40728, -110},
	{40856, -120},
	{40983, -130},
	{41110, -140},
	{41237, -150},
	{41307, -160},
	{41378, -170},
	{41448, -180},
	{41518, -190},
	{41588, -200},
};
/* TBLTE_PROJECT */
#else
#if defined(CONFIG_SEC_TBLTE_JPN)
static sec_bat_adc_table_data_t temp_table[] = {
	{27502, 900},
	{27514, 890},
	{27551, 880},
	{27576, 870},
	{27597, 860},
	{27617, 850},
	{27651, 840},
	{27693, 830},
	{27721, 820},
	{27763, 810},
	{27802, 800},
	{27867, 790},
	{27961, 780},
	{28030, 770},
	{28116, 760},
	{28254, 750},
	{28370, 740},
	{28377, 730},
	{28502, 720},
	{28589, 710},
	{28723, 700},
	{28849, 690},
	{28884, 680},
	{29044, 670},
	{29203, 660},
	{29267, 650},
	{29355, 640},
	{29451, 630},
	{29583, 620},
	{29729, 610},
	{29779, 600},
	{29922, 590},
	{30041, 580},
	{30113, 570},
	{30261, 560},
	{30357, 550},
	{30482, 540},
	{30610, 530},
	{30731, 520},
	{30858, 510},
	{31021, 500},
	{31121, 490},
	{31246, 480},
	{31388, 470},
	{31509, 460},
	{31633, 450},
	{31760, 440},
	{31872, 430},
	{32037, 420},
	{32150, 410},
	{32282, 400},
	{32401, 390},
	{32554, 380},
	{32685, 370},
	{32787, 360},
	{32919, 350},
	{33055, 340},
	{33164, 330},
	{33274, 320},
	{33413, 310},
	{33541, 300},
	{33647, 290},
	{33699, 280},
	{33838, 270},
	{33948, 260},
	{34060, 250},
	{34170, 240},
	{34273, 230},
	{34381, 220},
	{34480, 210},
	{34585, 200},
	{34687, 190},
	{34777, 180},
	{34865, 170},
	{34946, 160},
	{35044, 150},
	{35129, 140},
	{35214, 130},
	{35289, 120},
	{35372, 110},
	{35428, 100},
	{35450, 90},
	{35517, 80},
	{35585, 70},
	{35652, 60},
	{35720, 50},
	{35779, 40},
	{35838, 30},
	{35897, 20},
	{35956, 10},
	{36015, 0},
	{36062, -10},
	{36109, -20},
	{36156, -30},
	{36203, -40},
	{36250, -50},
	{36288, -60},
	{36325, -70},
	{36363, -80},
	{36401, -90},
	{36439, -100},
	{36470, -110},
	{36501, -120},
	{36531, -130},
	{36562, -140},
	{36593, -150},
	{36616, -160},
	{36639, -170},
	{36661, -180},
	{36684, -190},
	{36707, -200},
};
#elif defined(CONFIG_MACH_TBLTE_ATT) || defined(CONFIG_MACH_TBLTE_SPR) || \
	defined(CONFIG_MACH_TBLTE_TMO) || defined(CONFIG_MACH_TBLTE_USC) || \
	defined(CONFIG_MACH_TBLTE_VZW) || defined(CONFIG_MACH_TBLTE_CAN)
static sec_bat_adc_table_data_t temp_table[] = {
	{28540, 700},
	{29117, 650},
	{29659, 600},
	{30268, 550},
	{30840, 500},
	{31468, 450},
	{31984, 400},
	{32713, 350},
	{33329, 300},
	{33913, 250},
	{34478, 200},
	{34885, 150},
	{35237, 100},
	{35676, 50},
	{35984, 0},
	{36171, -50},
	{36348, -100},
	{36506, -150},
	{36660, -200},
};
#else
static sec_bat_adc_table_data_t temp_table[] = {
	{28520, 700},
	{29097, 650},
	{29669, 600},
	{30248, 550},
	{30840, 500},
	{31227, 460},
	{31449, 450},
	{31962, 400},
	{32931, 350},
	{33314, 300},
	{33899, 250},
	{34468, 200},
	{34877, 150},
	{35231, 100},
	{35674, 50},
	{35716, 40},
	{35780, 30},
	{35828, 20},
	{35888, 10},
	{35950, 0},
	{36010, -10},
	{36056, -20},
	{36099, -30},
	{36147, -40},
	{36174, -50},
	{36347, -100},
	{36503, -150},
	{36564, -200},
};
#endif

static sec_bat_adc_table_data_t chg_temp_table[] = {
	{26143, 900},
	{26157, 890},
	{26186, 880},
	{26201, 870},
	{26236, 860},
	{26251, 850},
	{26274, 840},
	{26299, 830},
	{26330, 820},
	{26376, 810},
	{26404, 800},
	{26445, 790},
	{26506, 780},
	{26547, 770},
	{26614, 760},
	{26682, 750},
	{26700, 740},
	{26791, 730},
	{26884, 720},
	{26951, 710},
	{27032, 700},
	{27155, 690},
	{27188, 680},
	{27190, 670},
	{27199, 660},
	{27234, 650},
	{27290, 640},
	{27375, 630},
	{27465, 620},
	{27581, 610},
	{27662, 600},
	{27773, 590},
	{27872, 580},
	{27949, 570},
	{28083, 560},
	{28165, 550},
	{28302, 540},
	{28406, 530},
	{28532, 520},
	{28653, 510},
	{29055, 500},
	{29160, 490},
	{29301, 480},
	{29456, 470},
	{29603, 460},
	{29741, 450},
	{29892, 440},
	{30052, 430},
	{30233, 420},
	{30380, 410},
	{30560, 400},
	{30729, 390},
	{30929, 380},
	{31121, 370},
	{31302, 360},
	{31491, 350},
	{31700, 340},
	{31885, 330},
	{32046, 320},
	{32261, 310},
	{32476, 300},
	{32667, 290},
	{32709, 280},
	{32947, 270},
	{33165, 260},
	{33384, 250},
	{33623, 240},
	{33840, 230},
	{34041, 220},
	{34288, 210},
	{34519, 200},
	{34704, 190},
	{34946, 180},
	{35123, 170},
	{35320, 160},
	{35531, 150},
	{35764, 140},
	{35973, 130},
	{36180, 120},
	{36408, 110},
	{36590, 100},
	{36858, 90},
	{37037, 80},
	{37264, 70},
	{37462, 60},
	{37658, 50},
	{37844, 40},
	{38049, 30},
	{38215, 20},
	{38413, 10},
	{38623, 0},
	{38814, -10},
	{38946, -20},
	{39047, -30},
	{39209, -40},
	{39333, -50},
	{39372, -60},
	{39427, -70},
	{39684, -80},
	{39876, -90},
	{40161, -100},
	{40254, -110},
	{40404, -120},
	{40541, -130},
	{40683, -140},
	{40800, -150},
	{40934, -160},
	{40976, -170},
	{41137, -180},
	{41211, -190},
	{41324, -200},
};
#endif

#if defined(CONFIG_BATTERY_SWELLING)
#define BATT_SWELLING_HIGH_TEMP_BLOCK		500
#define BATT_SWELLING_HIGH_TEMP_RECOV		450
#define BATT_SWELLING_LOW_TEMP_BLOCK		50
#define BATT_SWELLING_LOW_TEMP_RECOV		100
#define BATT_SWELLING_HIGH_CHG_CURRENT			0
#define BATT_SWELLING_LOW_CHG_CURRENT			1500
#define BATT_SWELLING_DROP_FLOAT_VOLTAGE		4200
#define BATT_SWELLING_HIGH_RECHG_VOLTAGE		4150
#define BATT_SWELLING_LOW_RECHG_VOLTAGE			4050
#endif

#define TEMP_HIGHLIMIT_THRESHOLD	800
#define TEMP_HIGHLIMIT_RECOVERY	700

#if defined(CONFIG_MACH_TRLTE_ATT) || defined(CONFIG_MACH_TRLTE_SPR) || \
	defined(CONFIG_MACH_TRLTE_TMO) || defined(CONFIG_MACH_TRLTE_USC) || \
	defined(CONFIG_MACH_TRLTE_VZW) || defined(CONFIG_MACH_TRLTE_CAN)
#define TEMP_HIGH_THRESHOLD_EVENT	600
#define TEMP_HIGH_RECOVERY_EVENT		460
#define TEMP_LOW_THRESHOLD_EVENT		-20
#define TEMP_LOW_RECOVERY_EVENT		10
#define TEMP_HIGH_THRESHOLD_NORMAL	560
#define TEMP_HIGH_RECOVERY_NORMAL	470
#define TEMP_LOW_THRESHOLD_NORMAL	-40
#define TEMP_LOW_RECOVERY_NORMAL	0
#define TEMP_HIGH_THRESHOLD_LPM		530
#define TEMP_HIGH_RECOVERY_LPM		490
#define TEMP_LOW_THRESHOLD_LPM		-20
#define TEMP_LOW_RECOVERY_LPM		20
#elif defined(CONFIG_MACH_TBLTE_ATT) || defined(CONFIG_MACH_TBLTE_SPR) || \
	defined(CONFIG_MACH_TBLTE_TMO) || defined(CONFIG_MACH_TBLTE_USC) || \
	defined(CONFIG_MACH_TBLTE_VZW) || defined(CONFIG_MACH_TBLTE_CAN)
#define TEMP_HIGH_THRESHOLD_EVENT	600
#define TEMP_HIGH_RECOVERY_EVENT		460
#define TEMP_LOW_THRESHOLD_EVENT		-20
#define TEMP_LOW_RECOVERY_EVENT		0
#define TEMP_HIGH_THRESHOLD_NORMAL	560
#define TEMP_HIGH_RECOVERY_NORMAL	470
#define TEMP_LOW_THRESHOLD_NORMAL	-40
#define TEMP_LOW_RECOVERY_NORMAL	0
#define TEMP_HIGH_THRESHOLD_LPM		520
#define TEMP_HIGH_RECOVERY_LPM		480
#define TEMP_LOW_THRESHOLD_LPM		-20
#define TEMP_LOW_RECOVERY_LPM		20
#else
#define TEMP_HIGH_THRESHOLD_EVENT	580
#define TEMP_HIGH_RECOVERY_EVENT		530
#define TEMP_LOW_THRESHOLD_EVENT		-50
#define TEMP_LOW_RECOVERY_EVENT		0
#define TEMP_HIGH_THRESHOLD_NORMAL	580
#define TEMP_HIGH_RECOVERY_NORMAL	530
#define TEMP_LOW_THRESHOLD_NORMAL	-50
#define TEMP_LOW_RECOVERY_NORMAL	0
#define TEMP_HIGH_THRESHOLD_LPM		580
#define TEMP_HIGH_RECOVERY_LPM		530
#define TEMP_LOW_THRESHOLD_LPM		-50
#define TEMP_LOW_RECOVERY_LPM		0
#endif

#endif /* __SEC_BATTERY_DATA_H */
