// SPDX-License-Identifier: GPL-2.0
#include <linux/bug.h>
#include <linux/kernel.h>
#include <linux/bitops.h>
#include <linux/fixp-arith.h>
#include <linux/iio/adc/qcom-vadc-common.h>
#include <linux/math64.h>
#include <linux/log2.h>
#include <linux/err.h>
#include <linux/module.h>
#include <linux/units.h>

/**
 * struct vadc_map_pt - Map the graph representation for ADC channel
 * @x: Represent the ADC digitized code.
 * @y: Represent the physical data which can be temperature, voltage,
 *     resistance.
 */
struct vadc_map_pt {
	s32 x;
	s32 y;
};

/* Voltage to temperature */
static const struct vadc_map_pt adcmap_100k_104ef_104fb[] = {
	{1758,	-40000 },
	{1742,	-35000 },
	{1719,	-30000 },
	{1691,	-25000 },
	{1654,	-20000 },
	{1608,	-15000 },
	{1551,	-10000 },
	{1483,	-5000 },
	{1404,	0 },
	{1315,	5000 },
	{1218,	10000 },
	{1114,	15000 },
	{1007,	20000 },
	{900,	25000 },
	{795,	30000 },
	{696,	35000 },
	{605,	40000 },
	{522,	45000 },
	{448,	50000 },
	{383,	55000 },
	{327,	60000 },
	{278,	65000 },
	{237,	70000 },
	{202,	75000 },
	{172,	80000 },
	{146,	85000 },
	{125,	90000 },
	{107,	95000 },
	{92,	100000 },
	{79,	105000 },
	{68,	110000 },
	{59,	115000 },
	{51,	120000 },
	{44,	125000 }
};

/*
 * Voltage to temperature table for 100k pull up for NTCG104EF104 with
 * 1.875V reference.
 */
static const struct vadc_map_pt adcmap_100k_104ef_104fb_1875_vref[] = {
	{ 1831,	-40000 },
	{ 1814,	-35000 },
	{ 1791,	-30000 },
	{ 1761,	-25000 },
	{ 1723,	-20000 },
	{ 1675,	-15000 },
	{ 1616,	-10000 },
	{ 1545,	-5000 },
	{ 1463,	0 },
	{ 1370,	5000 },
	{ 1268,	10000 },
	{ 1160,	15000 },
	{ 1049,	20000 },
	{ 937,	25000 },
	{ 828,	30000 },
	{ 726,	35000 },
	{ 630,	40000 },
	{ 544,	45000 },
	{ 467,	50000 },
	{ 399,	55000 },
	{ 340,	60000 },
	{ 290,	65000 },
	{ 247,	70000 },
	{ 209,	75000 },
	{ 179,	80000 },
	{ 153,	85000 },
	{ 130,	90000 },
	{ 112,	95000 },
	{ 96,	100000 },
	{ 82,	105000 },
	{ 71,	110000 },
	{ 62,	115000 },
	{ 53,	120000 },
	{ 46,	125000 },
};

/*
 * Voltage to temperature table for 100k pull up for bat_therm with
 * Alium.
 */
static const struct vadc_map_pt adcmap_batt_therm_100k[] = {
	{1840,	-400},
	{1835,	-380},
	{1828,	-360},
	{1821,	-340},
	{1813,	-320},
	{1803,	-300},
	{1793,	-280},
	{1781,	-260},
	{1768,	-240},
	{1753,	-220},
	{1737,	-200},
	{1719,	-180},
	{1700,	-160},
	{1679,	-140},
	{1655,	-120},
	{1630,	-100},
	{1603,	-80},
	{1574,	-60},
	{1543,	-40},
	{1510,	-20},
	{1475,	0},
	{1438,	20},
	{1400,	40},
	{1360,	60},
	{1318,	80},
	{1276,	100},
	{1232,	120},
	{1187,	140},
	{1142,	160},
	{1097,	180},
	{1051,	200},
	{1005,	220},
	{960,	240},
	{915,	260},
	{871,	280},
	{828,	300},
	{786,	320},
	{745,	340},
	{705,	360},
	{666,	380},
	{629,	400},
	{594,	420},
	{560,	440},
	{527,	460},
	{497,	480},
	{467,	500},
	{439,	520},
	{413,	540},
	{388,	560},
	{365,	580},
	{343,	600},
	{322,	620},
	{302,	640},
	{284,	660},
	{267,	680},
	{251,	700},
	{235,	720},
	{221,	740},
	{208,	760},
	{195,	780},
	{184,	800},
	{173,	820},
	{163,	840},
	{153,	860},
	{144,	880},
	{136,	900},
	{128,	920},
	{120,	940},
	{114,	960},
	{107,	980}
};

/*
 * Voltage to temperature table for 30k pull up for bat_therm with
 * Alium.
 */
static const struct vadc_map_pt adcmap_batt_therm_30k[] = {
	{1864,	-400},
	{1863,	-380},
	{1861,	-360},
	{1858,	-340},
	{1856,	-320},
	{1853,	-300},
	{1850,	-280},
	{1846,	-260},
	{1842,	-240},
	{1837,	-220},
	{1831,	-200},
	{1825,	-180},
	{1819,	-160},
	{1811,	-140},
	{1803,	-120},
	{1794,	-100},
	{1784,	-80},
	{1773,	-60},
	{1761,	-40},
	{1748,	-20},
	{1734,	0},
	{1718,	20},
	{1702,	40},
	{1684,	60},
	{1664,	80},
	{1643,	100},
	{1621,	120},
	{1597,	140},
	{1572,	160},
	{1546,	180},
	{1518,	200},
	{1489,	220},
	{1458,	240},
	{1426,	260},
	{1393,	280},
	{1359,	300},
	{1324,	320},
	{1288,	340},
	{1252,	360},
	{1214,	380},
	{1176,	400},
	{1138,	420},
	{1100,	440},
	{1061,	460},
	{1023,	480},
	{985,	500},
	{947,	520},
	{910,	540},
	{873,	560},
	{836,	580},
	{801,	600},
	{766,	620},
	{732,	640},
	{699,	660},
	{668,	680},
	{637,	700},
	{607,	720},
	{578,	740},
	{550,	760},
	{524,	780},
	{498,	800},
	{474,	820},
	{451,	840},
	{428,	860},
	{407,	880},
	{387,	900},
	{367,	920},
	{349,	940},
	{332,	960},
	{315,	980}
};

/*
 * Voltage to temperature table for 400k pull up for bat_therm with
 * Alium.
 */
static const struct vadc_map_pt adcmap_batt_therm_400k[] = {
	{1744,	-400},
	{1724,	-380},
	{1701,	-360},
	{1676,	-340},
	{1648,	-320},
	{1618,	-300},
	{1584,	-280},
	{1548,	-260},
	{1509,	-240},
	{1468,	-220},
	{1423,	-200},
	{1377,	-180},
	{1328,	-160},
	{1277,	-140},
	{1225,	-120},
	{1171,	-100},
	{1117,	-80},
	{1062,	-60},
	{1007,	-40},
	{953,	-20},
	{899,	0},
	{847,	20},
	{795,	40},
	{745,	60},
	{697,	80},
	{651,	100},
	{607,	120},
	{565,	140},
	{526,	160},
	{488,	180},
	{453,	200},
	{420,	220},
	{390,	240},
	{361,	260},
	{334,	280},
	{309,	300},
	{286,	320},
	{265,	340},
	{245,	360},
	{227,	380},
	{210,	400},
	{195,	420},
	{180,	440},
	{167,	460},
	{155,	480},
	{144,	500},
	{133,	520},
	{124,	540},
	{115,	560},
	{107,	580},
	{99,	600},
	{92,	620},
	{86,	640},
	{80,	660},
	{75,	680},
	{70,	700},
	{65,	720},
	{61,	740},
	{57,	760},
	{53,	780},
	{50,	800},
	{46,	820},
	{43,	840},
	{41,	860},
	{38,	880},
	{36,	900},
	{34,	920},
	{32,	940},
	{30,	960},
	{28,	980}
};

static const struct vadc_map_pt adcmap7_die_temp[] = {
	{ 857300, 160000 },
	{ 820100, 140000 },
	{ 782500, 120000 },
	{ 744600, 100000 },
	{ 706400, 80000 },
	{ 667900, 60000 },
	{ 629300, 40000 },
	{ 590500, 20000 },
	{ 551500, 0 },
	{ 512400, -20000 },
	{ 473100, -40000 },
	{ 433700, -60000 },
};

/*
 * Resistance to temperature table for 100k pull up for NTCG104EF104.
 */
static const struct vadc_map_pt adcmap7_100k[] = {
	{ 4397119, -40000 },
	{ 4092874, -39000 },
	{ 3811717, -38000 },
	{ 3551749, -37000 },
	{ 3311236, -36000 },
	{ 3088599, -35000},
	{ 2882396, -34000 },
	{ 2691310, -33000},
	{ 2514137, -32000 },
	{ 2349778, -31000 },
	{ 2197225, -30000 },
	{ 2055558, -29000 },
	{ 1923932, -28000 },
	{ 1801573, -27000 },
	{ 1687773, -26000 },
	{ 1581881, -25000 },
	{ 1483100, -24000 },
	{ 1391113, -23000 },
	{ 1305413, -22000 },
	{ 1225531, -21000 },
	{ 1151037, -20000 },
	{ 1081535, -19000 },
	{ 1016661, -18000 },
	{ 956079, -17000 },
	{ 899480, -16000 },
	{ 846578, -15000 },
	{ 797111, -14000 },
	{ 750834, -13000 },
	{ 707523, -12000 },
	{ 666972, -11000 },
	{ 628988, -10000 },
	{ 593342, -9000 },
	{ 559930, -8000 },
	{ 528601, -7000 },
	{ 499212, -6000 },
	{ 471632, -5000 },
	{ 445771, -4000 },
	{ 421479, -3000 },
	{ 398652, -2000 },
	{ 377192, -1000 },
	{ 357011, 0 },
	{ 338005, 1000 },
	{ 320121, 2000 },
	{ 303286, 3000 },
	{ 287433, 4000 },
	{ 272500, 5000 },
	{ 258426, 6000 },
	{ 245160, 7000 },
	{ 232649, 8000 },
	{ 220847, 9000 },
	{ 209710, 10000 },
	{ 199196, 11000 },
	{ 189268, 12000 },
	{ 179890, 13000 },
	{ 171028, 14000 },
	{ 162651, 15000 },
	{ 154726, 16000 },
	{ 147232, 17000 },
	{ 140142, 18000 },
	{ 133432, 19000 },
	{ 127080, 20000 },
	{ 121066, 21000 },
	{ 115368, 22000 },
	{ 109970, 23000 },
	{ 104852, 24000 },
	{ 100000, 25000 },
	{ 95398, 26000 },
	{ 91032, 27000 },
	{ 86889, 28000 },
	{ 82956, 29000 },
	{ 79222, 30000 },
	{ 75675, 31000 },
	{ 72306, 32000 },
	{ 69104, 33000 },
	{ 66061, 34000 },
	{ 63167, 35000 },
	{ 60415, 36000 },
	{ 57797, 37000 },
	{ 55306, 38000 },
	{ 52934, 39000 },
	{ 50677, 40000 },
	{ 48528, 41000 },
	{ 46482, 42000 },
	{ 44533, 43000 },
	{ 42675, 44000 },
	{ 40903, 45000 },
	{ 39213, 46000 },
	{ 37601, 47000 },
	{ 36063, 48000 },
	{ 34595, 49000 },
	{ 33195, 50000 },
	{ 31859, 51000 },
	{ 30584, 52000 },
	{ 29366, 53000 },
	{ 28203, 54000 },
	{ 27091, 55000 },
	{ 26028, 56000 },
	{ 25013, 57000 },
	{ 24041, 58000 },
	{ 23113, 59000},
	{ 22224, 60000 },
	{ 21374, 61000 },
	{ 20561, 62000 },
	{ 19782, 63000 },
	{ 19036, 64000 },
	{ 18323, 65000 },
	{ 17640, 66000 },
	{ 16986, 67000 },
	{ 16360, 68000 },
	{ 15760, 69000 },
	{ 15184, 70000 },
	{ 14631, 71000 },
	{ 14100, 72000 },
	{ 13592, 73000 },
	{ 13104, 74000 },
	{ 12635, 75000 },
	{ 12187, 76000 },
	{ 11757, 77000 },
	{ 11344, 78000 },
	{ 10947, 79000 },
	{ 10566, 80000 },
	{ 10200, 81000 },
	{ 9848, 82000},
	{ 9510, 83000 },
	{ 9185, 84000 },
	{ 8873, 85000 },
	{ 8572, 86000 },
	{ 8283, 87000 },
	{ 8006, 88000 },
	{ 7738, 89000 },
	{ 7481, 90000 },
	{ 7234, 91000 },
	{ 6997, 92000 },
	{ 6769, 93000 },
	{ 6548, 94000 },
	{ 6336, 95000 },
	{ 6132, 96000 },
	{ 5934, 97000 },
	{ 5744, 98000 },
	{ 5561, 99000 },
	{ 5384, 100000 },
	{ 5214, 101000 },
	{ 5051, 102000 },
	{ 4893, 103000 },
	{ 4741, 104000 },
	{ 4594, 105000 },
	{ 4453, 106000 },
	{ 4316, 107000 },
	{ 4184, 108000 },
	{ 4057, 109000 },
	{ 3934, 110000 },
	{ 3816, 111000 },
	{ 3701, 112000 },
	{ 3591, 113000 },
	{ 3484, 114000 },
	{ 3380, 115000 },
	{ 3281, 116000 },
	{ 3185, 117000 },
	{ 3093, 118000 },
	{ 3003, 119000 },
	{ 2916, 120000 },
	{ 2832, 121000 },
	{ 2751, 122000 },
	{ 2672, 123000 },
	{ 2596, 124000 },
	{ 2522, 125000 }
};

/*
 * Resistance to temperature table for batt_therm.
 */
static const struct vadc_map_pt adcmap_gen3_batt_therm_100k[] = {
	{ 5319890, -400 },
	{ 4555860, -380 },
	{ 3911780, -360 },
	{ 3367320, -340 },
	{ 2905860, -320 },
	{ 2513730, -300 },
	{ 2179660, -280 },
	{ 1894360, -260 },
	{ 1650110, -240 },
	{ 1440520, -220 },
	{ 1260250, -200 },
	{ 1104850, -180 },
	{ 970600,  -160 },
	{ 854370,  -140 },
	{ 753530,  -120 },
	{ 665860,  -100 },
	{ 589490,  -80 },
	{ 522830,  -60 },
	{ 464540,  -40 },
	{ 413470,  -20 },
	{ 368640,  0 },
	{ 329220,  20 },
	{ 294490,  40 },
	{ 263850,  60 },
	{ 236770,  80 },
	{ 212790,  100 },
	{ 191530,  120 },
	{ 172640,  140 },
	{ 155840,  160 },
	{ 140880,  180 },
	{ 127520,  200 },
	{ 115590,  220 },
	{ 104910,  240 },
	{ 95350,   260 },
	{ 86760,   280 },
	{ 79050,   300 },
	{ 72110,   320 },
	{ 65860,   340 },
	{ 60220,   360 },
	{ 55130,   380 },
	{ 50520,   400 },
	{ 46350,   420 },
	{ 42570,   440 },
	{ 39140,   460 },
	{ 36030,   480 },
	{ 33190,   500 },
	{ 30620,   520 },
	{ 28260,   540 },
	{ 26120,   560 },
	{ 24160,   580 },
	{ 22370,   600 },
	{ 20730,   620 },
	{ 19230,   640 },
	{ 17850,   660 },
	{ 16580,   680 },
	{ 15420,   700 },
	{ 14350,   720 },
	{ 13370,   740 },
	{ 12470,   760 },
	{ 11630,   780 },
	{ 10860,   800 },
	{ 10150,   820 },
	{ 9490,    840 },
	{ 8880,    860 },
	{ 8320,    880 },
	{ 7800,    900 },
	{ 7310,    920 },
	{ 6860,    940 },
	{ 6450,    960 },
	{ 6060,    980 }
};

static const struct vadc_prescale_ratio adc5_prescale_ratios[] = {
	{.num =  1, .den =  1},
	{.num =  1, .den =  3},
	{.num =  1, .den =  4},
	{.num =  1, .den =  6},
	{.num =  1, .den = 20},
	{.num =  1, .den =  8},
	{.num = 10, .den = 81},
	{.num =  1, .den = 10},
	{.num =  1, .den = 16},
	{.num = 40, .den = 41},		/* PM7_SMB_TEMP */
	/* Prescale ratios for current channels below */
	{.num = 32, .den = 100},	/* IIN_FB, IIN_SMB */
	{.num = 16, .den = 100},	/* ICHG_SMB */
	{.num = 1280, .den = 4100},	/* IIN_SMB_new */
	{.num = 640, .den = 4100},	/* ICHG_SMB_new */
	{.num = 1000, .den = 305185},	/* ICHG_FB */
	{.num = 1000, .den = 610370},	/* ICHG_FB_2X */
	{.num = 1000, .den = 366220},	/* ICHG_FB ADC5_GEN3 */
	{.num = 1000, .den = 732440}	/* ICHG_FB_2X ADC5_GEN3 */
};

static int qcom_vadc_scale_hw_calib_volt(
				const struct vadc_prescale_ratio *prescale,
				const struct adc5_data *data,
				u16 adc_code, int *result_uv);
/* Current scaling for PMIC7 */
static int qcom_vadc_scale_hw_calib_current(
				const struct vadc_prescale_ratio *prescale,
				const struct adc5_data *data,
				u16 adc_code, int *result_ua);
/* Raw current for PMIC7 */
static int qcom_vadc_scale_hw_calib_current_raw(
				const struct vadc_prescale_ratio *prescale,
				const struct adc5_data *data,
				u16 adc_code, int *result_ua);
/* Current scaling for PMIC5 */
static int qcom_vadc5_scale_hw_calib_current(
				const struct vadc_prescale_ratio *prescale,
				const struct adc5_data *data,
				u16 adc_code, int *result_ua);
static int qcom_vadc_scale_hw_calib_therm(
				const struct vadc_prescale_ratio *prescale,
				const struct adc5_data *data,
				u16 adc_code, int *result_mdec);
static int qcom_vadc_scale_hw_calib_batt_therm_100(
				const struct vadc_prescale_ratio *prescale,
				const struct adc5_data *data,
				u16 adc_code, int *result_mdec);
static int qcom_vadc_scale_hw_calib_batt_therm_30(
				const struct vadc_prescale_ratio *prescale,
				const struct adc5_data *data,
				u16 adc_code, int *result_mdec);
static int qcom_vadc_scale_hw_calib_batt_therm_400(
				const struct vadc_prescale_ratio *prescale,
				const struct adc5_data *data,
				u16 adc_code, int *result_mdec);
static int qcom_vadc7_scale_hw_calib_therm(
				const struct vadc_prescale_ratio *prescale,
				const struct adc5_data *data,
				u16 adc_code, int *result_mdec);
static int qcom_vadc_scale_hw_smb_temp(
				const struct vadc_prescale_ratio *prescale,
				const struct adc5_data *data,
				u16 adc_code, int *result_mdec);
static int qcom_vadc_scale_hw_pm7_smb_temp(
				const struct vadc_prescale_ratio *prescale,
				const struct adc5_data *data,
				u16 adc_code, int *result_mdec);
static int qcom_vadc_scale_hw_smb1398_temp(
				const struct vadc_prescale_ratio *prescale,
				const struct adc5_data *data,
				u16 adc_code, int *result_mdec);
static int qcom_vadc_scale_hw_pm2250_s3_die_temp(
				const struct vadc_prescale_ratio *prescale,
				const struct adc5_data *data,
				u16 adc_code, int *result_mdec);
static int qcom_adc5_gen3_scale_hw_calib_batt_therm_100(
				const struct vadc_prescale_ratio *prescale,
				const struct adc5_data *data,
				u16 adc_code, int *result_mdec);
static int qcom_adc5_gen3_scale_hw_calib_batt_id_100(
				const struct vadc_prescale_ratio *prescale,
				const struct adc5_data *data,
				u16 adc_code, int *result_mdec);
static int qcom_adc5_gen3_scale_hw_calib_usb_in_current(
				const struct vadc_prescale_ratio *prescale,
				const struct adc5_data *data,
				u16 adc_code, int *result_mdec);
static int qcom_vadc_scale_hw_chg5_temp(
				const struct vadc_prescale_ratio *prescale,
				const struct adc5_data *data,
				u16 adc_code, int *result_mdec);
static int qcom_vadc_scale_hw_pm7_chg_temp(
				const struct vadc_prescale_ratio *prescale,
				const struct adc5_data *data,
				u16 adc_code, int *result_mdec);
static int qcom_vadc_scale_hw_calib_die_temp(
				const struct vadc_prescale_ratio *prescale,
				const struct adc5_data *data,
				u16 adc_code, int *result_mdec);
static int qcom_vadc7_scale_hw_calib_die_temp(
				const struct vadc_prescale_ratio *prescale,
				const struct adc5_data *data,
				u16 adc_code, int *result_mdec);

static struct qcom_adc5_scale_type scale_adc5_fn[] = {
	[SCALE_HW_CALIB_DEFAULT] = {qcom_vadc_scale_hw_calib_volt},
	[SCALE_HW_CALIB_CUR] = {qcom_vadc_scale_hw_calib_current},
	[SCALE_HW_CALIB_CUR_RAW] = {qcom_vadc_scale_hw_calib_current_raw},
	[SCALE_HW_CALIB_PM5_CUR] = {qcom_vadc5_scale_hw_calib_current},
	[SCALE_HW_CALIB_THERM_100K_PULLUP] = {qcom_vadc_scale_hw_calib_therm},
	[SCALE_HW_CALIB_BATT_THERM_100K] = {
				qcom_vadc_scale_hw_calib_batt_therm_100},
	[SCALE_HW_CALIB_BATT_THERM_30K] = {
				qcom_vadc_scale_hw_calib_batt_therm_30},
	[SCALE_HW_CALIB_BATT_THERM_400K] = {
				qcom_vadc_scale_hw_calib_batt_therm_400},
	[SCALE_HW_CALIB_XOTHERM] = {qcom_vadc_scale_hw_calib_therm},
	[SCALE_HW_CALIB_THERM_100K_PU_PM7] = {
					qcom_vadc7_scale_hw_calib_therm},
	[SCALE_HW_CALIB_PMIC_THERM] = {qcom_vadc_scale_hw_calib_die_temp},
	[SCALE_HW_CALIB_PMIC_THERM_PM7] = {
					qcom_vadc7_scale_hw_calib_die_temp},
	[SCALE_HW_CALIB_PM5_CHG_TEMP] = {qcom_vadc_scale_hw_chg5_temp},
	[SCALE_HW_CALIB_PM5_SMB_TEMP] = {qcom_vadc_scale_hw_smb_temp},
	[SCALE_HW_CALIB_PM5_SMB1398_TEMP] = {qcom_vadc_scale_hw_smb1398_temp},
	[SCALE_HW_CALIB_PM2250_S3_DIE_TEMP] = {qcom_vadc_scale_hw_pm2250_s3_die_temp},
	[SCALE_HW_CALIB_PM5_GEN3_BATT_THERM_100K] = {qcom_adc5_gen3_scale_hw_calib_batt_therm_100},
	[SCALE_HW_CALIB_PM5_GEN3_BATT_ID_100K] = {qcom_adc5_gen3_scale_hw_calib_batt_id_100},
	[SCALE_HW_CALIB_PM5_GEN3_USB_IN_I] = {qcom_adc5_gen3_scale_hw_calib_usb_in_current},
	[SCALE_HW_CALIB_PM7_SMB_TEMP] = {qcom_vadc_scale_hw_pm7_smb_temp},
	[SCALE_HW_CALIB_PM7_CHG_TEMP] = {qcom_vadc_scale_hw_pm7_chg_temp},
};

static int qcom_vadc_map_voltage_temp(const struct vadc_map_pt *pts,
				      u32 tablesize, s32 input, int *output)
{
	u32 i = 0;

	if (!pts)
		return -EINVAL;

	while (i < tablesize && pts[i].x > input)
		i++;

	if (i == 0) {
		*output = pts[0].y;
	} else if (i == tablesize) {
		*output = pts[tablesize - 1].y;
	} else {
		/* interpolate linearly */
		*output = fixp_linear_interpolate(pts[i - 1].x, pts[i - 1].y,
						  pts[i].x, pts[i].y,
						  input);
	}

	return 0;
}

static s32 qcom_vadc_map_temp_voltage(const struct vadc_map_pt *pts,
				      u32 tablesize, int input)
{
	u32 i = 0;

	/*
	 * Table must be sorted, find the interval of 'y' which contains value
	 * 'input' and map it to proper 'x' value
	 */
	while (i < tablesize && pts[i].y < input)
		i++;

	if (i == 0)
		return pts[0].x;
	if (i == tablesize)
		return pts[tablesize - 1].x;

	/* interpolate linearly */
	return fixp_linear_interpolate(pts[i - 1].y, pts[i - 1].x,
			pts[i].y, pts[i].x, input);
}

static void qcom_vadc_scale_calib(const struct vadc_linear_graph *calib_graph,
				  u16 adc_code,
				  bool absolute,
				  s64 *scale_voltage)
{
	*scale_voltage = (adc_code - calib_graph->gnd);
	*scale_voltage *= calib_graph->dx;
	*scale_voltage = div64_s64(*scale_voltage, calib_graph->dy);
	if (absolute)
		*scale_voltage += calib_graph->dx;

	if (*scale_voltage < 0)
		*scale_voltage = 0;
}

static int qcom_vadc_scale_volt(const struct vadc_linear_graph *calib_graph,
				const struct vadc_prescale_ratio *prescale,
				bool absolute, u16 adc_code,
				int *result_uv)
{
	s64 voltage = 0, result = 0;

	qcom_vadc_scale_calib(calib_graph, adc_code, absolute, &voltage);

	voltage = voltage * prescale->den;
	result = div64_s64(voltage, prescale->num);
	*result_uv = result;

	return 0;
}

static int qcom_vadc_scale_therm(const struct vadc_linear_graph *calib_graph,
				 const struct vadc_prescale_ratio *prescale,
				 bool absolute, u16 adc_code,
				 int *result_mdec)
{
	s64 voltage = 0;
	int ret;

	qcom_vadc_scale_calib(calib_graph, adc_code, absolute, &voltage);

	if (absolute)
		voltage = div64_s64(voltage, 1000);

	ret = qcom_vadc_map_voltage_temp(adcmap_100k_104ef_104fb,
					 ARRAY_SIZE(adcmap_100k_104ef_104fb),
					 voltage, result_mdec);
	if (ret)
		return ret;

	return 0;
}

static int qcom_vadc_scale_die_temp(const struct vadc_linear_graph *calib_graph,
				    const struct vadc_prescale_ratio *prescale,
				    bool absolute,
				    u16 adc_code, int *result_mdec)
{
	s64 voltage = 0;
	u64 temp; /* Temporary variable for do_div */

	qcom_vadc_scale_calib(calib_graph, adc_code, absolute, &voltage);

	if (voltage > 0) {
		temp = voltage * prescale->den;
		do_div(temp, prescale->num * 2);
		voltage = temp;
	} else {
		voltage = 0;
	}

	*result_mdec = milli_kelvin_to_millicelsius(voltage);

	return 0;
}

static int qcom_vadc_scale_chg_temp(const struct vadc_linear_graph *calib_graph,
				    const struct vadc_prescale_ratio *prescale,
				    bool absolute,
				    u16 adc_code, int *result_mdec)
{
	s64 voltage = 0, result = 0;

	qcom_vadc_scale_calib(calib_graph, adc_code, absolute, &voltage);

	voltage = voltage * prescale->den;
	voltage = div64_s64(voltage, prescale->num);
	voltage = ((PMI_CHG_SCALE_1) * (voltage * 2));
	voltage = (voltage + PMI_CHG_SCALE_2);
	result =  div64_s64(voltage, 1000000);
	*result_mdec = result;

	return 0;
}

/* convert voltage to ADC code, using 1.875V reference */
static u16 qcom_vadc_scale_voltage_code(s32 voltage,
					const struct vadc_prescale_ratio *prescale,
					const u32 full_scale_code_volt,
					unsigned int factor)
{
	s64 volt = voltage;
	s64 adc_vdd_ref_mv = 1875; /* reference voltage */

	volt *= prescale->num * factor * full_scale_code_volt;
	volt = div64_s64(volt, (s64)prescale->den * adc_vdd_ref_mv * 1000);

	return volt;
}

static int qcom_vadc_scale_code_voltage_factor(u16 adc_code,
				const struct vadc_prescale_ratio *prescale,
				const struct adc5_data *data,
				unsigned int factor)
{
	s64 voltage, temp, adc_vdd_ref_mv = 1875;

	/*
	 * The normal data range is between 0V to 1.875V. On cases where
	 * we read low voltage values, the ADC code can go beyond the
	 * range and the scale result is incorrect so we clamp the values
	 * for the cases where the code represents a value below 0V
	 */
	if (adc_code > VADC5_MAX_CODE)
		adc_code = 0;

	/* (ADC code * vref_vadc (1.875V)) / full_scale_code */
	voltage = (s64) adc_code * adc_vdd_ref_mv * 1000;
	voltage = div64_s64(voltage, data->full_scale_code_volt);
	if (voltage > 0) {
		voltage *= prescale->den;
		temp = prescale->num * factor;
		voltage = div64_s64(voltage, temp);
	} else {
		voltage = 0;
	}

	return (int) voltage;
}

static int qcom_vadc7_scale_hw_calib_therm(
				const struct vadc_prescale_ratio *prescale,
				const struct adc5_data *data,
				u16 adc_code, int *result_mdec)
{
	s64 resistance = adc_code;
	int ret, result;

	if (adc_code >= RATIO_MAX_ADC7)
		return -EINVAL;

	/* (ADC code * R_PULLUP (100Kohm)) / (full_scale_code - ADC code)*/
	resistance *= R_PU_100K;
	resistance = div64_s64(resistance, RATIO_MAX_ADC7 - adc_code);

	ret = qcom_vadc_map_voltage_temp(adcmap7_100k,
				 ARRAY_SIZE(adcmap7_100k),
				 resistance, &result);
	if (ret)
		return ret;

	*result_mdec = result;

	return 0;
}

static int qcom_vadc_scale_hw_calib_current_raw(
				const struct vadc_prescale_ratio *prescale,
				const struct adc5_data *data,
				u16 adc_code, int *result_ua)
{
	s64 temp;

	if (!prescale->num)
		return -EINVAL;

	temp = div_s64((s64)(s16)adc_code * prescale->den, prescale->num);
	*result_ua = (int) temp;
	pr_debug("raw adc_code: %#x result_ua: %d\n", adc_code, *result_ua);

	return 0;
}

static int qcom_vadc_scale_hw_calib_current(
				const struct vadc_prescale_ratio *prescale,
				const struct adc5_data *data,
				u16 adc_code, int *result_ua)
{
	u32 adc_vdd_ref_mv = 1875;
	s64 voltage;

	if (!prescale->num)
		return -EINVAL;

	/* (ADC code * vref_vadc (1.875V)) / full_scale_code */
	voltage = (s64)(s16) adc_code * adc_vdd_ref_mv * 1000;
	voltage = div_s64(voltage, data->full_scale_code_volt);
	voltage = div_s64(voltage * prescale->den, prescale->num);
	*result_ua = (int) voltage;
	pr_debug("adc_code: %#x result_ua: %d\n", adc_code, *result_ua);

	return 0;
}

static int qcom_vadc5_scale_hw_calib_current(
				const struct vadc_prescale_ratio *prescale,
				const struct adc5_data *data,
				u16 adc_code, int *result_ua)
{
	s64 voltage = 0, result = 0;
	bool positive = true;

	if (adc_code & ADC5_USR_DATA_CHECK) {
		adc_code = ~adc_code + 1;
		positive = false;
	}

	voltage = (s64)(s16) adc_code * data->full_scale_code_cur * 1000;
	voltage = div64_s64(voltage, VADC5_MAX_CODE);
	result = div64_s64(voltage * prescale->den, prescale->num);
	*result_ua = result;

	if (!positive)
		*result_ua = -result;

	return 0;
}

static int qcom_vadc_scale_hw_calib_volt(
				const struct vadc_prescale_ratio *prescale,
				const struct adc5_data *data,
				u16 adc_code, int *result_uv)
{
	*result_uv = qcom_vadc_scale_code_voltage_factor(adc_code,
				prescale, data, 1);

	return 0;
}

static int qcom_vadc_scale_hw_calib_therm(
				const struct vadc_prescale_ratio *prescale,
				const struct adc5_data *data,
				u16 adc_code, int *result_mdec)
{
	int voltage;

	voltage = qcom_vadc_scale_code_voltage_factor(adc_code,
				prescale, data, 1000);

	/* Map voltage to temperature from look-up table */
	return qcom_vadc_map_voltage_temp(adcmap_100k_104ef_104fb_1875_vref,
				 ARRAY_SIZE(adcmap_100k_104ef_104fb_1875_vref),
				 voltage, result_mdec);
}

static int qcom_vadc_scale_hw_calib_batt_therm_100(
				const struct vadc_prescale_ratio *prescale,
				const struct adc5_data *data,
				u16 adc_code, int *result_mdec)
{
	int voltage;

	voltage = qcom_vadc_scale_code_voltage_factor(adc_code,
				prescale, data, 1000);

	/* Map voltage to temperature from look-up table */
	return qcom_vadc_map_voltage_temp(adcmap_batt_therm_100k,
				 ARRAY_SIZE(adcmap_batt_therm_100k),
				 voltage, result_mdec);
}

static int qcom_vadc_scale_hw_calib_batt_therm_30(
				const struct vadc_prescale_ratio *prescale,
				const struct adc5_data *data,
				u16 adc_code, int *result_mdec)
{
	int voltage;

	voltage = qcom_vadc_scale_code_voltage_factor(adc_code,
				prescale, data, 1000);

	/* Map voltage to temperature from look-up table */
	return qcom_vadc_map_voltage_temp(adcmap_batt_therm_30k,
				 ARRAY_SIZE(adcmap_batt_therm_30k),
				 voltage, result_mdec);
}

static int qcom_vadc_scale_hw_calib_batt_therm_400(
				const struct vadc_prescale_ratio *prescale,
				const struct adc5_data *data,
				u16 adc_code, int *result_mdec)
{
	int voltage;

	voltage = qcom_vadc_scale_code_voltage_factor(adc_code,
				prescale, data, 1000);

	/* Map voltage to temperature from look-up table */
	return qcom_vadc_map_voltage_temp(adcmap_batt_therm_400k,
				 ARRAY_SIZE(adcmap_batt_therm_400k),
				 voltage, result_mdec);
}

static int qcom_vadc_scale_hw_calib_die_temp(
				const struct vadc_prescale_ratio *prescale,
				const struct adc5_data *data,
				u16 adc_code, int *result_mdec)
{
	*result_mdec = qcom_vadc_scale_code_voltage_factor(adc_code,
				prescale, data, 2);
	*result_mdec = milli_kelvin_to_millicelsius(*result_mdec);

	return 0;
}

static int qcom_vadc7_scale_hw_calib_die_temp(
				const struct vadc_prescale_ratio *prescale,
				const struct adc5_data *data,
				u16 adc_code, int *result_mdec)
{

	int voltage;

	voltage = qcom_vadc_scale_code_voltage_factor(adc_code,
				prescale, data, 1);

	return qcom_vadc_map_voltage_temp(adcmap7_die_temp, ARRAY_SIZE(adcmap7_die_temp),
			voltage, result_mdec);
}

static int qcom_vadc_scale_hw_pm7_chg_temp(
				const struct vadc_prescale_ratio *prescale,
				const struct adc5_data *data,
				u16 adc_code, int *result_mdec)
{
	s64 temp;
	int result_uv;

	result_uv = qcom_vadc_scale_code_voltage_factor(adc_code,
				prescale, data, 1);

	/* T(C) = Vadc/0.0033 – 277.12 */
	temp = div_s64((30303LL * result_uv) - (27712 * 1000000LL), 100000);
	pr_debug("adc_code: %u result_uv: %d temp: %lld\n", adc_code, result_uv,
		temp);
	*result_mdec = temp > 0 ? temp : 0;

	return 0;
}

static int qcom_vadc_scale_hw_pm7_smb_temp(
				const struct vadc_prescale_ratio *prescale,
				const struct adc5_data *data,
				u16 adc_code, int *result_mdec)
{
	s64 temp;
	int result_uv;

	result_uv = qcom_vadc_scale_code_voltage_factor(adc_code,
				prescale, data, 1);

	/* T(C) = 25 + (25*Vadc - 24.885) / 0.0894 */
	temp = div_s64(((25000LL * result_uv) - (24885 * 1000000LL)) * 10000,
			894 * 1000000) + 25000;
	pr_debug("adc_code: %#x result_uv: %d temp: %lld\n", adc_code,
		result_uv, temp);
	*result_mdec = temp > 0 ? temp : 0;

	return 0;
}

static int qcom_vadc_scale_hw_smb_temp(
				const struct vadc_prescale_ratio *prescale,
				const struct adc5_data *data,
				u16 adc_code, int *result_mdec)
{
	*result_mdec = qcom_vadc_scale_code_voltage_factor(adc_code * 100,
				prescale, data, PMIC5_SMB_TEMP_SCALE_FACTOR);
	*result_mdec = PMIC5_SMB_TEMP_CONSTANT - *result_mdec;

	return 0;
}

static int qcom_vadc_scale_hw_smb1398_temp(
				const struct vadc_prescale_ratio *prescale,
				const struct adc5_data *data,
				u16 adc_code, int *result_mdec)
{
	s64 voltage = 0, adc_vdd_ref_mv = 1875;
	u64 temp;

	if (adc_code > VADC5_MAX_CODE)
		adc_code = 0;

	/* (ADC code * vref_vadc (1.875V)) / full_scale_code */
	voltage = (s64) adc_code * adc_vdd_ref_mv * 1000;
	voltage = div64_s64(voltage, data->full_scale_code_volt);
	if (voltage > 0) {
		temp = voltage * prescale->den;
		temp *= 100;
		do_div(temp, prescale->num * PMIC5_SMB1398_TEMP_SCALE_FACTOR);
		voltage = temp;
	} else {
		voltage = 0;
	}

	voltage = voltage - PMIC5_SMB1398_TEMP_CONSTANT;
	*result_mdec = voltage;

	return 0;
}

static int qcom_vadc_scale_hw_pm2250_s3_die_temp(
				const struct vadc_prescale_ratio *prescale,
				const struct adc5_data *data,
				u16 adc_code, int *result_mdec)
{
	s64 voltage = 0, adc_vdd_ref_mv = 1875;

	if (adc_code > VADC5_MAX_CODE)
		adc_code = 0;

	/* (ADC code * vref_vadc (1.875V)) / full_scale_code */
	voltage = (s64) adc_code * adc_vdd_ref_mv * 1000;
	voltage = div64_s64(voltage, data->full_scale_code_volt);
	if (voltage > 0) {
		voltage *= prescale->den;
		voltage = div64_s64(voltage, prescale->num);
	} else {
		voltage = 0;
	}

	voltage = PMIC5_PM2250_S3_DIE_TEMP_CONSTANT - voltage;
	voltage *= 100000;
	voltage = div64_s64(voltage, PMIC5_PM2250_S3_DIE_TEMP_SCALE_FACTOR);

	*result_mdec = voltage;

	return 0;
}

static int qcom_adc5_gen3_scale_hw_calib_batt_therm_100(
				const struct vadc_prescale_ratio *prescale,
				const struct adc5_data *data,
				u16 adc_code, int *result_mdec)
{
	s64 resistance = 0;
	int ret, result = 0;

	if (adc_code >= RATIO_MAX_ADC7)
		return -EINVAL;

	/* (ADC code * R_PULLUP (100Kohm)) / (full_scale_code - ADC code)*/
	resistance = (s64) adc_code * R_PU_100K;
	resistance = div64_s64(resistance, (RATIO_MAX_ADC7 - adc_code));

	ret = qcom_vadc_map_voltage_temp(adcmap_gen3_batt_therm_100k,
				 ARRAY_SIZE(adcmap_gen3_batt_therm_100k),
				 resistance, &result);
	if (ret)
		return ret;

	*result_mdec = result;

	return 0;
}

static int qcom_adc5_gen3_scale_hw_calib_batt_id_100(
				const struct vadc_prescale_ratio *prescale,
				const struct adc5_data *data,
				u16 adc_code, int *result_mdec)
{
	s64 resistance = 0;

	if (adc_code >= RATIO_MAX_ADC7)
		return -EINVAL;

	/* (ADC code * R_PULLUP (100Kohm)) / (full_scale_code - ADC code)*/
	resistance = (s64) adc_code * R_PU_100K;
	resistance = div64_s64(resistance, (RATIO_MAX_ADC7 - adc_code));

	*result_mdec = (int)resistance;

	return 0;
};

static int qcom_adc5_gen3_scale_hw_calib_usb_in_current(
				const struct vadc_prescale_ratio *prescale,
				const struct adc5_data *data,
				u16 adc_code, int *result_ua)
{
	s64 voltage = 0, result = 0;
	bool positive = true;

	if (adc_code & ADC5_USR_DATA_CHECK) {
		adc_code = ~adc_code + 1;
		positive = false;
	}

	voltage = (s64)(s16) adc_code * 1000000;
	voltage = div64_s64(voltage, PMIC5_GEN3_USB_IN_I_SCALE_FACTOR);
	result = div64_s64(voltage * prescale->den, prescale->num);
	*result_ua = (int)result;

	if (!positive)
		*result_ua = -(int)result;

	return 0;
};

static int qcom_vadc_scale_hw_chg5_temp(
				const struct vadc_prescale_ratio *prescale,
				const struct adc5_data *data,
				u16 adc_code, int *result_mdec)
{
	*result_mdec = qcom_vadc_scale_code_voltage_factor(adc_code,
				prescale, data, 4);
	*result_mdec = PMIC5_CHG_TEMP_SCALE_FACTOR - *result_mdec;

	return 0;
}

void adc_tm_scale_therm_voltage_100k_gen3(struct adc_tm_config *param)
{
	int temp, ret;
	int64_t resistance = 0;

	/*
	 * High temperature maps to lower threshold voltage.
	 * Same API can be used for resistance-temperature table
	 */
	resistance = qcom_vadc_map_temp_voltage(adcmap7_100k,
						ARRAY_SIZE(adcmap7_100k),
						param->high_thr_temp);

	param->low_thr_voltage = resistance * RATIO_MAX_ADC7;
	param->low_thr_voltage = div64_s64(param->low_thr_voltage,
						(resistance + R_PU_100K));

	/*
	 * low_thr_voltage is ADC raw code corresponding to upper temperature
	 * threshold.
	 * Instead of returning the ADC raw code obtained at this point,we first
	 * do a forward conversion on the (low voltage / high temperature) threshold code,
	 * to temperature, to check if that code, when read by TM, would translate to
	 * a temperature greater than or equal to the upper temperature limit (which is
	 * expected). If it is instead lower than the upper limit (not expected for correct
	 * TM functionality), we lower the raw code of the threshold written by 1
	 * to ensure TM does see a violation when it reads raw code corresponding
	 * to the upper limit temperature specified.
	 */
	ret = qcom_vadc7_scale_hw_calib_therm(NULL, NULL, param->low_thr_voltage, &temp);
	if (ret < 0)
		return;

	if (temp < param->high_thr_temp)
		param->low_thr_voltage--;

	/*
	 * Low temperature maps to higher threshold voltage
	 * Same API can be used for resistance-temperature table
	 */
	resistance = qcom_vadc_map_temp_voltage(adcmap7_100k,
						ARRAY_SIZE(adcmap7_100k),
						param->low_thr_temp);

	param->high_thr_voltage = resistance * RATIO_MAX_ADC7;
	param->high_thr_voltage = div64_s64(param->high_thr_voltage,
						(resistance + R_PU_100K));

	/*
	 * high_thr_voltage is ADC raw code corresponding to lower temperature
	 * threshold.
	 * Similar to what is done above for low_thr voltage, we first
	 * do a forward conversion on the (high voltage / low temperature)threshold code,
	 * to temperature, to check if that code, when read by TM, would translate to a
	 * temperature less than or equal to the lower temperature limit (which is expected).
	 * If it is instead greater than the lower limit (not expected for correct
	 * TM functionality), we increase the raw code of the threshold written by 1
	 * to ensure TM does see a violation when it reads raw code corresponding
	 * to the lower limit temperature specified.
	 */
	ret = qcom_vadc7_scale_hw_calib_therm(NULL, NULL, param->high_thr_voltage, &temp);
	if (ret < 0)
		return;

	if (temp > param->low_thr_temp)
		param->high_thr_voltage++;
}
EXPORT_SYMBOL(adc_tm_scale_therm_voltage_100k_gen3);

int32_t adc_tm_absolute_rthr_gen3(struct adc_tm_config *tm_config)
{
	int64_t low_thr = 0, high_thr = 0;

	low_thr =  tm_config->low_thr_voltage;
	low_thr *= ADC5_FULL_SCALE_CODE;

	low_thr = div64_s64(low_thr, ADC_VDD_REF);
	tm_config->low_thr_voltage = low_thr;

	high_thr =  tm_config->high_thr_voltage;
	high_thr *= ADC5_FULL_SCALE_CODE;

	high_thr = div64_s64(high_thr, ADC_VDD_REF);
	tm_config->high_thr_voltage = high_thr;

	return 0;
}
EXPORT_SYMBOL(adc_tm_absolute_rthr_gen3);

int qcom_vadc_scale(enum vadc_scale_fn_type scaletype,
		    const struct vadc_linear_graph *calib_graph,
		    const struct vadc_prescale_ratio *prescale,
		    bool absolute,
		    u16 adc_code, int *result)
{
	switch (scaletype) {
	case SCALE_DEFAULT:
		return qcom_vadc_scale_volt(calib_graph, prescale,
					    absolute, adc_code,
					    result);
	case SCALE_THERM_100K_PULLUP:
	case SCALE_XOTHERM:
		return qcom_vadc_scale_therm(calib_graph, prescale,
					     absolute, adc_code,
					     result);
	case SCALE_PMIC_THERM:
		return qcom_vadc_scale_die_temp(calib_graph, prescale,
						absolute, adc_code,
						result);
	case SCALE_PMI_CHG_TEMP:
		return qcom_vadc_scale_chg_temp(calib_graph, prescale,
						absolute, adc_code,
						result);
	default:
		return -EINVAL;
	}
}
EXPORT_SYMBOL(qcom_vadc_scale);

u16 qcom_adc_tm5_temp_volt_scale(unsigned int prescale_ratio,
				 u32 full_scale_code_volt, int temp)
{
	const struct vadc_prescale_ratio *prescale = &adc5_prescale_ratios[prescale_ratio];
	s32 voltage;

	voltage = qcom_vadc_map_temp_voltage(adcmap_100k_104ef_104fb_1875_vref,
					     ARRAY_SIZE(adcmap_100k_104ef_104fb_1875_vref),
					     temp);
	return qcom_vadc_scale_voltage_code(voltage, prescale, full_scale_code_volt, 1000);
}
EXPORT_SYMBOL(qcom_adc_tm5_temp_volt_scale);

int qcom_adc5_hw_scale(enum vadc_scale_fn_type scaletype,
		    unsigned int prescale_ratio,
		    const struct adc5_data *data,
		    u16 adc_code, int *result)
{
	const struct vadc_prescale_ratio *prescale = &adc5_prescale_ratios[prescale_ratio];

	if (!(scaletype >= SCALE_HW_CALIB_DEFAULT &&
		scaletype < SCALE_HW_CALIB_INVALID)) {
		pr_err("Invalid scale type %d\n", scaletype);
		return -EINVAL;
	}

	return scale_adc5_fn[scaletype].scale_fn(prescale, data,
					adc_code, result);
}
EXPORT_SYMBOL(qcom_adc5_hw_scale);

int qcom_adc5_prescaling_from_dt(u32 num, u32 den)
{
	unsigned int pre;

	for (pre = 0; pre < ARRAY_SIZE(adc5_prescale_ratios); pre++)
		if (adc5_prescale_ratios[pre].num == num &&
		    adc5_prescale_ratios[pre].den == den)
			break;

	if (pre == ARRAY_SIZE(adc5_prescale_ratios))
		return -EINVAL;

	return pre;
}
EXPORT_SYMBOL(qcom_adc5_prescaling_from_dt);

int qcom_adc5_hw_settle_time_from_dt(u32 value,
				     const unsigned int *hw_settle)
{
	unsigned int i;

	for (i = 0; i < VADC_HW_SETTLE_SAMPLES_MAX; i++) {
		if (value == hw_settle[i])
			return i;
	}

	return -EINVAL;
}
EXPORT_SYMBOL(qcom_adc5_hw_settle_time_from_dt);

int qcom_adc5_avg_samples_from_dt(u32 value)
{
	if (!is_power_of_2(value) || value > ADC5_AVG_SAMPLES_MAX)
		return -EINVAL;

	return __ffs(value);
}
EXPORT_SYMBOL(qcom_adc5_avg_samples_from_dt);

int qcom_adc5_decimation_from_dt(u32 value, const unsigned int *decimation)
{
	unsigned int i;

	for (i = 0; i < ADC5_DECIMATION_SAMPLES_MAX; i++) {
		if (value == decimation[i])
			return i;
	}

	return -EINVAL;
}
EXPORT_SYMBOL(qcom_adc5_decimation_from_dt);

int qcom_vadc_decimation_from_dt(u32 value)
{
	if (!is_power_of_2(value) || value < VADC_DECIMATION_MIN ||
	    value > VADC_DECIMATION_MAX)
		return -EINVAL;

	return __ffs64(value / VADC_DECIMATION_MIN);
}
EXPORT_SYMBOL(qcom_vadc_decimation_from_dt);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Qualcomm ADC common functionality");
