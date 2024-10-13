/*
 * adc_fuelgauge.h
 * Samsung ADC Fuel Gauge Header
 *
 * Copyright (C) 2012 Samsung Electronics, Inc.
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

#ifndef __ADC_FUELGAUGE_H
#define __ADC_FUELGAUGE_H __FILE__

#include <linux/wakelock.h>
#include <linux/d2199/d2199_battery.h>
#include <linux/d2199/d2199_reg.h>
#include <linux/d2199/core.h>

/* Slave address should be shifted to the right 1bit.
 * R/W bit should NOT be included.
 */
static const char __initdata d2199_battery_banner[] = \
    "D2199 Battery, (c) 2013 Dialog Semiconductor Ltd.\n";

/***************************************************************************
 Pre-definition
***************************************************************************/
#define FALSE								(0)
#define TRUE								(1)

#define ADC_RES_MASK_LSB					(0x0F)
#define ADC_RES_MASK_MSB					(0xF0)

#if USED_BATTERY_CAPACITY == BAT_CAPACITY_1800MA

#define ADC_VAL_100_PERCENT	3645   // About 4280mV
#define CV_START_ADC		3275   // About 4100mV
#define MAX_FULL_CHARGED_ADC				3788   // About 4350mV

#define D2153_BAT_CHG_FRST_FULL_LVL         4310   // About EOC 160mA
#define D2153_BAT_CHG_BACK_FULL_LVL         4347   // 4340mV -> 4347mV

#define FIRST_VOLTAGE_DROP_ADC				115

#define FIRST_VOLTAGE_DROP_LL_ADC			340
#define FIRST_VOLTAGE_DROP_L_ADC			60
#define FIRST_VOLTAGE_DROP_RL_ADC			25

#define CHARGE_OFFSET_KRNL_H				880
#define CHARGE_OFFSET_KRNL_L				5

#define CHARGE_ADC_KRNL_H					1700
#define CHARGE_ADC_KRNL_L					2050
#define CHARGE_ADC_KRNL_F					2060

#define CHARGE_OFFSET_KRNL					77
#define CHARGE_OFFSET_KRNL2					50

#define LAST_CHARGING_WEIGHT				80   // 900
#define VBAT_3_4_VOLTAGE_ADC				1605

#elif USED_BATTERY_CAPACITY == BAT_CAPACITY_2100MA
#if defined(CONFIG_MACH_WILCOX)
#define ADC_VAL_100_PERCENT	3686   // About 4300mV
#elif defined(CONFIG_MACH_CS05)
#define ADC_VAL_100_PERCENT	3676   // About 4280mV
#else
#define ADC_VAL_100_PERCENT					3645   // About 4280mV
#endif

#define CV_START_ADC						3275   // About 4100mV
#define MAX_FULL_CHARGED_ADC				3788   // About 4350mV

#define D2153_BAT_CHG_FRST_FULL_LVL         4310   // About EOC 160mA
#define D2153_BAT_CHG_BACK_FULL_LVL         4340   // About EOC 50mA

#if defined(CONFIG_MACH_WILCOX) || defined(CONFIG_MACH_CS05)
#define FIRST_VOLTAGE_DROP_ADC				145    // 130 -> 145

#define FIRST_VOLTAGE_DROP_LL_ADC			340
#define FIRST_VOLTAGE_DROP_L_ADC			60
#define FIRST_VOLTAGE_DROP_RL_ADC			25

#define CHARGE_OFFSET_KRNL_H				360    // 400 -> 360
#define CHARGE_OFFSET_KRNL_L				160    // 170 -> 160

#define CHARGE_ADC_KRNL_H					1150   // 1155 -> 1150
#define CHARGE_ADC_KRNL_L					2304
#define CHARGE_ADC_KRNL_F					2305

#define CHARGE_OFFSET_KRNL					20
#define CHARGE_OFFSET_KRNL2					104    // 94 -> 104

#define LAST_CHARGING_WEIGHT				124
#else
#define FIRST_VOLTAGE_DROP_ADC				100     // 156

#define FIRST_VOLTAGE_DROP_LL_ADC			340
#define FIRST_VOLTAGE_DROP_L_ADC			60
#define FIRST_VOLTAGE_DROP_RL_ADC			25

#define CHARGE_OFFSET_KRNL_H				270
#define CHARGE_OFFSET_KRNL_L				20

#define CHARGE_ADC_KRNL_H					1700
#define CHARGE_ADC_KRNL_L					2050
#define CHARGE_ADC_KRNL_F					2060

#define CHARGE_OFFSET_KRNL					20
#define CHARGE_OFFSET_KRNL2					50

#define LAST_CHARGING_WEIGHT				55
#endif
#define VBAT_3_4_VOLTAGE_ADC				1605

#else

#define ADC_VAL_100_PERCENT					3445
#define CV_START_ADC						3338
#define MAX_FULL_CHARGED_ADC				3788  // About 4350mV

#define D2153_BAT_CHG_FRST_FULL_LVL         4160   // About EOC 160mA
#define D2153_BAT_CHG_BACK_FULL_LVL         4185   // About EOC 60mA

#define FIRST_VOLTAGE_DROP_ADC				165

#define FIRST_VOLTAGE_DROP_LL_ADC			340
#define FIRST_VOLTAGE_DROP_L_ADC			60
#define FIRST_VOLTAGE_DROP_RL_ADC			25

#define CHARGE_OFFSET_KRNL_H				880
#define CHARGE_OFFSET_KRNL_L				5

#define CHARGE_ADC_KRNL_H					1700
#define CHARGE_ADC_KRNL_L					2050
#define CHARGE_ADC_KRNL_F					2060

#define CHARGE_OFFSET_KRNL					77
#define CHARGE_OFFSET_KRNL2					50

#define LAST_CHARGING_WEIGHT				60   // 900
#define VBAT_3_4_VOLTAGE_ADC				1605

#endif /* USED_BATTERY_CAPACITY == BAT_CAPACITY_????MA */

#define FULL_CAPACITY						1000

#define NORM_NUM							10000
#define NORM_CHG_NUM						100000

#define DISCHARGE_SLEEP_OFFSET              55    // 45
#define LAST_VOL_UP_PERCENT                 75


//////////////////////////////////////////////////////////////////////////////
//    Static Variable Declaration
//////////////////////////////////////////////////////////////////////////////

static struct sec_fuelgauge_info *gbat = NULL;
static u8  is_called_by_ticker = 0;
static u16 ACT_4P2V_ADC = 0;
static u16 ACT_3P4V_ADC = 0;


// This array is for setting ADC_CONT register about each channel.
static struct adc_cont_in_auto adc_cont_inven[D2199_ADC_CHANNEL_MAX - 1] = {
	// VBAT_S channel
	[D2199_ADC_VOLTAGE] = {
		.adc_preset_val = 0,
		.adc_cont_val = (D2199_ADC_AUTO_EN_MASK | D2199_ADC_MODE_MASK 
							| D2199_AUTO_VBAT_EN_MASK),
		.adc_msb_res = D2199_VDD_RES_VBAT_RES_REG,
		.adc_lsb_res = D2199_ADC_RES_AUTO1_REG,
		.adc_lsb_mask = ADC_RES_MASK_LSB,
	},
	// TEMP_1 channel
	[D2199_ADC_TEMPERATURE_1] = {
		.adc_preset_val = D2199_TEMP1_ISRC_EN_MASK,
		.adc_cont_val = (D2199_ADC_AUTO_EN_MASK | D2199_ADC_MODE_MASK
						| D2199_TEMP1_ISRC_EN_MASK),
		.adc_msb_res = D2199_TBAT1_RES_TEMP1_RES_REG,
		.adc_lsb_res = D2199_ADC_RES_AUTO1_REG,
		.adc_lsb_mask = ADC_RES_MASK_MSB,
	},
	// TEMP_2 channel
	[D2199_ADC_TEMPERATURE_2] = {
		.adc_preset_val =  D2199_TEMP2_ISRC_EN_MASK,
		.adc_cont_val = (D2199_ADC_AUTO_EN_MASK | D2199_ADC_MODE_MASK
						| D2199_TEMP2_ISRC_EN_MASK),
		.adc_msb_res = D2199_TBAT2_RES_TEMP2_RES_REG,
		.adc_lsb_res = D2199_ADC_RES_AUTO3_REG,
		.adc_lsb_mask = ADC_RES_MASK_LSB,
	},
	// VF channel
	[D2199_ADC_VF] = {
		.adc_preset_val = D2199_AD4_ISRC_ENVF_ISRC_EN_MASK,
		.adc_cont_val = (D2199_ADC_AUTO_EN_MASK | D2199_ADC_MODE_MASK 
						| D2199_AUTO_VF_EN_MASK 
						| D2199_AD4_ISRC_ENVF_ISRC_EN_MASK),
		.adc_msb_res = D2199_ADCIN4_RES_VF_RES_REG,
		.adc_lsb_res = D2199_ADC_RES_AUTO2_REG,
		.adc_lsb_mask = ADC_RES_MASK_LSB,
	},
	// AIN channel
	[D2199_ADC_AIN] = {
		.adc_preset_val = 0,
		.adc_cont_val = (D2199_ADC_AUTO_EN_MASK | D2199_ADC_MODE_MASK
							| D2199_AUTO_AIN_EN_MASK),
		.adc_msb_res = D2199_ADCIN5_RES_AIN_RES_REG,
		.adc_lsb_res = D2199_ADC_RES_AUTO2_REG,
		.adc_lsb_mask = ADC_RES_MASK_MSB
	},
};

/* Table for compensation of charge offset in CC mode */
static u16 initialize_charge_offset[] = {
//  500, 600, 700, 800, 900, 1000, 1100, 1200,
	 18,  22,  26,  30,  34,   36,   40,   44,
};

static u16 initialize_charge_up_cc[] = {
// 500, 600, 700, 800, 900, 1000, 1100, 1200,
	45
};

/* LUT for NCP15XW223 thermistor with 10uA current source selected */
static struct adc2temp_lookuptbl adc2temp_lut = {
	// Case of NCP03XH223
#if defined(CONFIG_MACH_BAFFIN) || defined(CONFIG_MACH_BAFFINQ)
	.adc  = {  // ADC-12 input value
		2516,	1890,	1551,	1218,	950,	911,	802,
		652,	505,	437,	362,	300,	252,	210,
		192,	175,	150,	128,	110,	100,	92,
		86,
	},
	.temp = {	// temperature (degree K)
		C2K(-200), C2K(-150), C2K(-100), C2K(-50), C2K(0),	 C2K(20),  C2K(50),
		C2K(100),  C2K(150),  C2K(200),  C2K(250), C2K(300), C2K(350), C2K(400),
		C2K(430),  C2K(450),  C2K(500),  C2K(550), C2K(600), C2K(630), C2K(650),
		C2K(670),
	},
#elif defined(CONFIG_MACH_WILCOX) || defined(CONFIG_MACH_CS05)
	#ifdef CONFIG_MACH_WILCOX_CMCC  
	//For Wilcox TD_SCDMA
	.adc  = {  // ADC-12 input value
		2000,     1633,      1275,       1031,      831,      772,      685,
		554,       471,       388,       314,      268,      224,      196,
		180,       167,       148,       125,       107,       102,       97,
		83,
	},
	#else
	//For Wilcox CU and Baffin RD
	.adc  = {  // ADC-12 input value
		2000,     1633,      1275,       1031,      831,      772,      685,
		554,       471,       388,       314,      268,      224,      189,
		171,       159,       136,       117,       100,       93,       88,
		75,
	},
	#endif
	.temp = {	// temperature (degree K)
		C2K(-200), C2K(-150), C2K(-100), C2K(-50), C2K(0),	 C2K(20),  C2K(50),
		C2K(100),  C2K(150),  C2K(200),  C2K(250), C2K(300), C2K(350), C2K(400),
		C2K(430),  C2K(450),  C2K(500),  C2K(550), C2K(600), C2K(630), C2K(650),
		C2K(700),
	},
#elif defined(CONFIG_MACH_CS02)
	.adc  = {  // ADC-12 input value
		2091,     1601,      1246,       1012,      855,      661,      543,
		444,       370,       347,       337,      327,      317,      257,
		221,       183,       155,       135,       113,       99,       85,
		76,
	},
	.temp = {	// temperature (degree K)
		C2K(-200), C2K(-150), C2K(-100), C2K(-50), C2K(0), C2K(50),  C2K(100),
		C2K(150),  C2K(200),  C2K(220),  C2K(230), C2K(240), C2K(250), C2K(300),
		C2K(350),  C2K(400),  C2K(450),  C2K(500), C2K(550), C2K(600), C2K(650),
		C2K(700),
	},
#else
	.adc  = {  // ADC-12 input value
		1458,      1156,      859,       761,      614,      566,      510,
		418,       344,       290,       244,      207,      176,      145,
		130,       123,       107,       88,       76,       70,       66,
		63, 
	},
	.temp = {	// temperature (degree K)
		C2K(-200), C2K(-150), C2K(-100), C2K(-50), C2K(0),	 C2K(20),  C2K(50),
		C2K(100),  C2K(150),  C2K(200),  C2K(250), C2K(300), C2K(350), C2K(400),
		C2K(430),  C2K(450),  C2K(500),  C2K(550), C2K(600), C2K(630), C2K(650),
		C2K(670),
	},
#endif
};

static u16 temp_lut_length = (u16)sizeof(adc2temp_lut.adc)/sizeof(u16);

// adc = (vbat-2500)/2000*2^12
// vbat (mV) = 2500 + adc*2000/2^12
static struct adc2vbat_lookuptbl adc2vbat_lut = {
	.adc	 = {1843, 1946, 2148, 2253, 2458, 2662, 2867, 2683, 3072, 3482,}, // ADC-12 input value
	.offset  = {   0,	 0,    0,	 0,    0,	 0,    0,	 0,    0,    0,}, // charging mode ADC offset
	.vbat	 = {3400, 3450, 3500, 3600, 3700, 3800, 3900, 4000, 4100, 4200,}, // VBAT (mV)
};

#if USED_BATTERY_CAPACITY == BAT_CAPACITY_1800MA
// For CS02 Project
static struct adc2soc_lookuptbl adc2soc_lut = {
	.adc_ht  = {1843, 1906, 2155, 2310, 2338, 2367, 2563, 2627, 2688, 2762, 2920, 3069, 3249, 3458, ADC_VAL_100_PERCENT,}, // ADC input @ high temp
	.adc_rt  = {1843, 1906, 2155, 2310, 2338, 2367, 2563, 2627, 2688, 2762, 2920, 3069, 3249, 3458, ADC_VAL_100_PERCENT,}, // ADC input @ room temp
	.adc_rlt = {1843, 1906, 2155, 2310, 2338, 2367, 2563, 2627, 2688, 2762, 2920, 3069, 3249, 3458, ADC_VAL_100_PERCENT,}, // ADC input @ low temp(0)
	.adc_lt  = {1843, 1906, 2155, 2310, 2338, 2367, 2563, 2627, 2688, 2762, 2920, 3069, 3249, 3458, ADC_VAL_100_PERCENT,}, // ADC input @ low temp(0)
	.adc_lmt = {1843, 1906, 2155, 2310, 2338, 2367, 2563, 2627, 2688, 2762, 2920, 3069, 3249, 3458, ADC_VAL_100_PERCENT,}, // ADC input @ low mid temp(-10)
	.adc_llt = {1843, 1906, 2155, 2310, 2338, 2367, 2563, 2627, 2688, 2762, 2920, 3069, 3249, 3458, ADC_VAL_100_PERCENT,}, // ADC input @ low low temp(-20)
	.soc	 = {   0,	10,   30,	50,   70,  100,  200,  300,  400,  500,  600,  700,  800,  900, 1000,}, // SoC in %
};

//Discharging Weight(Room/Low/low low)
// latest 0.1C
//Discharging Weight(Room/Low/low low)           //   0,   1,   3,   5,   7,  10,  20,  30,  40,  50,  60,  70,  80,  90, 100
// This is for 0.1C
//static u16 adc_weight_section_discharge_1c[]   = {  5,   8,  10,  45,  90, 115,  43,  50,  75,  80, 100, 100, 100, 100, 145};
static u16 adc_weight_section_discharge_1c[]     = {110, 160, 230, 280, 240, 101,  17,  13,  18,  33,  34,  68,  95, 125, 195};
static u16 adc_weight_section_discharge_rlt_1c[] = {110, 160, 230, 280, 240, 101,  17,  13,  18,  33,  34,  68,  95, 125, 195};
static u16 adc_weight_section_discharge_lt_1c[]  = {110, 160, 230, 280, 240, 101,  17,  13,  18,  33,  34,  68,  95, 125, 195};
static u16 adc_weight_section_discharge_lmt_1c[] = {110, 160, 230, 280, 240, 101,  17,  13,  18,  33,  34,  68,  95, 125, 195};
static u16 adc_weight_section_discharge_llt_1c[] = {110, 160, 230, 280, 240, 101,  17,  13,  18,  33,  34,  68,  95, 125, 195};

// This is for 0.2C
//Discharging Weight(Room/Low/low low)           //   0,   1,   3,   5,   7,  10,  20,  30,  40,  50,  60,  70,  80,  90, 100
                                                 //{ 40,  80, 150, 150, 120,  59,  18,  16,  21,  39,  36,  58,  73,  80, 125};
static u16 adc_weight_section_discharge_2c[]     = {140, 180, 190, 150, 120, 105,  18,  16,  21,  39,  36,  58,  73,  80, 125};
static u16 adc_weight_section_discharge_rlt_2c[] = {140, 180, 190, 150, 120, 105,  18,  16,  21,  39,  36,  58,  73,  80, 125};
static u16 adc_weight_section_discharge_lt_2c[]  = {140, 180, 190, 150, 120, 105,  18,  16,  21,  39,  36,  58,  73,  80, 125};
static u16 adc_weight_section_discharge_lmt_2c[] = {140, 180, 190, 150, 120, 105,  18,  16,  21,  39,  36,  58,  73,  80, 125};
static u16 adc_weight_section_discharge_llt_2c[] = {140, 180, 190, 150, 120, 105,  18,  16,  21,  39,  36,  58,  73,  80, 125};

//Charging Weight(Room/Low/low low)              //    0,   1,   3,   5,   7,  10,  20,  30,  40,  50,  60,  70,  80,  90, 100
static u16 adc_weight_section_charge[]           = {2950, 355, 265,  66,  45,  93,  32,  33,  42,  92,  97, 110, 120,  90, LAST_CHARGING_WEIGHT};
static u16 adc_weight_section_charge_rlt[]       = {2950, 355, 265,  66,  45,  93,  32,  33,  42,  92,  97, 110, 120,  90, LAST_CHARGING_WEIGHT};
static u16 adc_weight_section_charge_lt[]        = {2950, 355, 265,  66,  45,  93,  32,  33,  42,  92,  97, 110, 120,  90, LAST_CHARGING_WEIGHT};
static u16 adc_weight_section_charge_lmt[]       = {2950, 355, 265,  66,  45,  93,  32,  33,  42,  92,  97, 110, 120,  90, LAST_CHARGING_WEIGHT};
static u16 adc_weight_section_charge_llt[]       = {2950, 355, 265,  66,  45,  93,  32,  33,  42,  92,  97, 110, 120,  90, LAST_CHARGING_WEIGHT};

static struct diff_tbl dischg_diff_tbl = {
   //  0,  2-1,  4-3,  6-5,  9-7,19-10,29-20,39-30,49-40
	{173,  173,  143,    8,  15,   54,   40,   37,   50},  // 0.1C
	{314,  314,  314,  145, 145,  177,  209,  221,  216},  // 0.2C
};

static u16 fg_reset_drop_offset[] = {172-50, 98-40};

#elif USED_BATTERY_CAPACITY == BAT_CAPACITY_2100MA

#if defined(CONFIG_MACH_WILCOX) || defined(CONFIG_MACH_CS05)
static struct adc2soc_lookuptbl adc2soc_lut = {
	.adc_ht  = {1843, 1891, 2170, 2314, 2395, 2438, 2573, 2623, 2666, 2746, 2898, 3057, 3246, 3466, ADC_VAL_100_PERCENT,}, // ADC input @ high temp
	.adc_rt  = {1843, 1891, 2170, 2314, 2395, 2438, 2573, 2623, 2666, 2746, 2898, 3057, 3246, 3466, ADC_VAL_100_PERCENT,}, // ADC input @ room temp
	.adc_rlt = {1843, 1891, 2170, 2314, 2395, 2438, 2573, 2623, 2666, 2746, 2898, 3057, 3246, 3466, ADC_VAL_100_PERCENT,}, // ADC input @ low temp(0)
	.adc_lt  = {1843, 1891, 2170, 2314, 2395, 2438, 2573, 2623, 2666, 2746, 2898, 3057, 3246, 3466, ADC_VAL_100_PERCENT,}, // ADC input @ low temp(0)
	.adc_lmt = {1843, 1891, 2170, 2314, 2395, 2438, 2573, 2623, 2666, 2746, 2898, 3057, 3246, 3466, ADC_VAL_100_PERCENT,}, // ADC input @ low mid temp(-10)
	.adc_llt = {1843, 1891, 2170, 2314, 2395, 2438, 2573, 2623, 2666, 2746, 2898, 3057, 3246, 3466, ADC_VAL_100_PERCENT,}, // ADC input @ low low temp(-20)
	.soc	 = {   0,	10,   30,	50,   70,  100,  200,  300,  400,  500,  600,  700,  800,  900, 1000,}, // SoC in %
};

// These table are for 0.1C load currents.
// latest 0.1C
//Discharging Weight(Room/Low/low low)           //   0,   1,   3,   5,   7,  10,  20,  30,  40,  50,  60,  70,  80,  90, 100
static u16 adc_weight_section_discharge_1c[]     = { 41, 123,  84,  36,  18,  25,  12,  10,  19,  32,  41,  60,  82,  94, 303};
static u16 adc_weight_section_discharge_rlt_1c[] = { 41, 123,  84,  36,  18,  25,  12,  10,  19,  32,  41,  60,  82,  94, 303};
static u16 adc_weight_section_discharge_lt_1c[]  = { 41, 123,  84,  36,  18,  25,  12,  10,  19,  32,  41,  60,  82,  94, 303};
static u16 adc_weight_section_discharge_lmt_1c[] = { 41, 123,  84,  36,  18,  25,  12,  10,  19,  32,  41,  60,  82,  94, 303};
static u16 adc_weight_section_discharge_llt_1c[] = { 41, 123,  84,  36,  18,  25,  12,  10,  19,  32,  41,  60,  82,  94, 303};

// These table are for 0.2C load currents.
//Discharging Weight(Room/Low/low low)           //   0,   1,   3,   5,   7,  10,  20,  30,  40,  50,  60,  70,  80,  90, 100
static u16 adc_weight_section_discharge_2c[]     = { 47, 141,  93,  59,  25,  33,  15,  12,  24,  43,  45,  65,  89,  95, 303};
static u16 adc_weight_section_discharge_rlt_2c[] = { 47, 141,  93,  59,  25,  33,  15,  12,  24,  43,  45,  65,  89,  95, 303};
static u16 adc_weight_section_discharge_lt_2c[]  = { 47, 141,  93,  59,  25,  33,  15,  12,  24,  43,  45,  65,  89,  95, 303};
static u16 adc_weight_section_discharge_lmt_2c[] = { 47, 141,  93,  59,  25,  33,  15,  12,  24,  43,  45,  65,  89,  95, 303};
static u16 adc_weight_section_discharge_llt_2c[] = { 47, 141,  93,  59,  25,  33,  15,  12,  24,  43,  45,  65,  89,  95, 303};

#ifdef CONFIG_MACH_WILCOX_CMCC  
// For Wilcox TDSCDMA
//Charging Weight(Room/Low/low low)              //    0,   1,   3,   5,   7,  10,  20,  30,  40,  50,  60,  70,  80,  90, 100
static u16 adc_weight_section_charge[]           = {2950, 954, 468, 317,  89,  73,  26,  22,  42,  79,  88, 106, 120, 115, LAST_CHARGING_WEIGHT};
static u16 adc_weight_section_charge_rlt[]		 = {2950, 954, 468, 317,  89,  73,  26,  22,  42,  79,  88, 106, 120, 115, LAST_CHARGING_WEIGHT};
static u16 adc_weight_section_charge_lt[]		 = {2950, 954, 468, 317,  89,  73,  26,  22,  42,  79,  88, 106, 120, 115, LAST_CHARGING_WEIGHT};
static u16 adc_weight_section_charge_lmt[]		 = {2950, 954, 468, 317,  89,  73,  26,  22,  42,  79,  88, 106, 120, 115, LAST_CHARGING_WEIGHT};
static u16 adc_weight_section_charge_llt[]		 = {2950, 954, 468, 317,  89,  73,  26,  22,  42,  79,  88, 106, 120, 115, LAST_CHARGING_WEIGHT};
#else
// For Wilcox CU and Baffin RD
//Charging Weight(Room/Low/low low)              //    0,   1,   3,   5,   7,  10,  20,  30,  40,  50,  60,  70,  80,  90, 100
static u16 adc_weight_section_charge[]           = {2950, 954, 468, 325, 102,  94,  33,  30,  52, 101, 112, 132, 141, 115, LAST_CHARGING_WEIGHT};
static u16 adc_weight_section_charge_rlt[]		 = {2950, 954, 468, 325, 102,  94,  33,  30,  52, 101, 112, 132, 141, 115, LAST_CHARGING_WEIGHT};
static u16 adc_weight_section_charge_lt[]		 = {2950, 954, 468, 325, 102,  94,  33,  30,  52, 101, 112, 132, 141, 115, LAST_CHARGING_WEIGHT};
static u16 adc_weight_section_charge_lmt[]		 = {2950, 954, 468, 325, 102,  94,  33,  30,  52, 101, 112, 132, 141, 115, LAST_CHARGING_WEIGHT};
static u16 adc_weight_section_charge_llt[]		 = {2950, 954, 468, 325, 102,  94,  33,  30,  52, 101, 112, 132, 141, 115, LAST_CHARGING_WEIGHT};
#endif

static struct diff_tbl dischg_diff_tbl = {
   //  0,  1-2,  3-4,  5-6,  7-9,10-19,20-29,30-39,40-49,50-59,60-69,70-79,80-89,90-99, 100
	{355,  277,  267,  264,  180,  117,  110,  117,  115,  115,   95,   79,   69,   30,  35},  // 0.1C
	{710,  706,  513,  409,  350,  249,  212,  202,  197,  211,  206,  182,  155,  135, 164},  // 0.2C
};

static u16 fg_reset_drop_offset[] = {86, 86};

#else
// For BAFFIN-RD project
static struct adc2soc_lookuptbl adc2soc_lut = {
	.adc_ht  = {1843, 1911, 2206, 2375, 2395, 2415, 2567, 2623, 2670, 2755, 2921, 3069, 3263, 3490, ADC_VAL_100_PERCENT,}, // ADC input @ high temp
	.adc_rt  = {1843, 1911, 2206, 2375, 2395, 2415, 2567, 2623, 2670, 2755, 2921, 3069, 3263, 3490, ADC_VAL_100_PERCENT,}, // ADC input @ room temp
	.adc_rlt = {1843, 1911, 2206, 2375, 2395, 2415, 2567, 2623, 2670, 2755, 2921, 3069, 3263, 3490, ADC_VAL_100_PERCENT,}, // ADC input @ low temp(0)
	.adc_lt  = {1843, 1911, 2206, 2375, 2395, 2415, 2567, 2623, 2670, 2755, 2921, 3069, 3263, 3490, ADC_VAL_100_PERCENT,}, // ADC input @ low temp(0)
	.adc_lmt = {1843, 1911, 2206, 2375, 2395, 2415, 2567, 2623, 2670, 2755, 2921, 3069, 3263, 3490, ADC_VAL_100_PERCENT,}, // ADC input @ low mid temp(-10)
	.adc_llt = {1843, 1911, 2206, 2375, 2395, 2415, 2567, 2623, 2670, 2755, 2921, 3069, 3263, 3490, ADC_VAL_100_PERCENT,}, // ADC input @ low low temp(-20)
	.soc	 = {   0,	10,   30,	50,   70,  100,  200,  300,  400,  500,  600,  700,  800,  900, 1000,}, // SoC in %
};

//Discharging Weight(Room/Low/low low) for 0.1C
//Discharging Weight(Room/Low/low low)           //   0,   1,   3,   5,   7,  10,  20,  30,  40,  50,  60,  70,  80,  90, 100
//static u16 adc_weight_section_discharge_1c[]   = { 50,  95,  75,  15,  15,  25,  10,  10,  17,  28,  32,  54,  75, 100, 295};
static u16 adc_weight_section_discharge_1c[]     = { 49,  40,  15,  10,  14,  23,  10,  10,  17,  28,  32,  54,  75, 100, 295};
static u16 adc_weight_section_discharge_rlt_1c[] = { 49,  40,  15,  10,  14,  23,  10,  10,  17,  28,  32,  54,  75, 100, 295};
static u16 adc_weight_section_discharge_lt_1c[]  = { 49,  40,  15,  10,  14,  23,  10,  10,  17,  28,  32,  54,  75, 100, 295};
static u16 adc_weight_section_discharge_lmt_1c[] = { 49,  40,  15,  10,  14,  23,  10,  10,  17,  28,  32,  54,  75, 100, 295};
static u16 adc_weight_section_discharge_llt_1c[] = { 49,  40,  15,  10,  14,  23,  10,  10,  17,  28,  32,  54,  75, 100, 295};

//Discharging Weight(Room/Low/low low) for 0.2C
//Discharging Weight(Room/Low/low low)           //   0,   1,   3,   5,   7,  10,  20,  30,  40,  50,  60,  70,  80,  90, 100
//static u16 adc_weight_section_discharge_2c[]   = { 50, 245, 210,  24,  18,  40,  14,  12,  23,  38,  37,  54,  75,  75, 265};
static u16 adc_weight_section_discharge_2c[]     = { 50, 215, 201,  24,  19,  42,  15,  13,  24,  38,  37,  54,  75,  75, 265};
static u16 adc_weight_section_discharge_rlt_2c[] = { 50, 215, 201,  24,  19,  42,  15,  13,  24,  38,  37,  54,  75,  75, 265};
static u16 adc_weight_section_discharge_lt_2c[]  = { 50, 215, 201,  24,  19,  42,  15,  13,  24,  38,  37,  54,  75,  75, 265};
static u16 adc_weight_section_discharge_lmt_2c[] = { 50, 215, 201,  24,  19,  42,  15,  13,  24,  38,  37,  54,  75,  75, 265};
static u16 adc_weight_section_discharge_llt_2c[] = { 50, 215, 201,  24,  19,  42,  15,  13,  24,  38,  37,  54,  75,  75, 265};

//Charging Weight(Room/Low/low low)              //    0,   1,   3,   5,   7,  10,  20,  30,  40,  50,  60,  70,  80,  90, 100
//static u16 adc_weight_section_charge[]         = {2800, 642, 620,  63,  53, 108,  40,  33,  58, 113, 107, 133, 142,  63, LAST_CHARGING_WEIGHT};
static u16 adc_weight_section_charge[]           = {2800, 642, 620,  63,  53, 106,  37,  31,  54, 108, 102, 130, 147,  70, LAST_CHARGING_WEIGHT};
static u16 adc_weight_section_charge_rlt[]		 = {2800, 642, 620,  63,  53, 106,  37,  31,  54, 108, 102, 130, 147,  70, LAST_CHARGING_WEIGHT};
static u16 adc_weight_section_charge_lt[]		 = {2800, 642, 620,  63,  53, 106,  37,  31,  54, 108, 102, 130, 147,  70, LAST_CHARGING_WEIGHT};
static u16 adc_weight_section_charge_lmt[]		 = {2800, 642, 620,  63,  53, 106,  37,  31,  54, 108, 102, 130, 147,  70, LAST_CHARGING_WEIGHT};
static u16 adc_weight_section_charge_llt[]		 = {2800, 642, 620,  63,  53, 106,  37,  31,  54, 108, 102, 130, 147,  70, LAST_CHARGING_WEIGHT};

static struct diff_tbl dischg_diff_tbl = {
   //  0,  1-2,  3-4,  5-6,  7-9,10-19,20-29,30-39,40-49,50-59,60-69,70-79,80-89,90-99, 100
	{400,  360,  230,  120,  120,  130,  110,  110,  110,  115,  100,   75,   60,   30,  20},  // 0.1C
	{635,  465,  270,  245,  220,  245,  230,  225,  217,  245,  245,  213,  178,  153, 250},  // 0.2C
};

static u16 fg_reset_drop_offset[] = {172, 98};

#endif

#else
static struct adc2soc_lookuptbl adc2soc_lut = {
	.adc_ht  = {1843, 1906, 2155, 2310, 2338, 2367, 2563, 2627, 2688, 2762, 2920, 3069, 3249, 3458, ADC_VAL_100_PERCENT,}, // ADC input @ high temp
	.adc_rt  = {1843, 1906, 2155, 2310, 2338, 2367, 2563, 2627, 2688, 2762, 2920, 3069, 3249, 3458, ADC_VAL_100_PERCENT,}, // ADC input @ room temp
	.adc_rlt = {1843, 1906, 2155, 2310, 2338, 2367, 2563, 2627, 2688, 2762, 2920, 3069, 3249, 3458, ADC_VAL_100_PERCENT,}, // ADC input @ low temp(0)
	.adc_lt  = {1843, 1906, 2155, 2310, 2338, 2367, 2563, 2627, 2688, 2762, 2920, 3069, 3249, 3458, ADC_VAL_100_PERCENT,}, // ADC input @ low temp(0)
	.adc_lmt = {1843, 1906, 2155, 2310, 2338, 2367, 2563, 2627, 2688, 2762, 2920, 3069, 3249, 3458, ADC_VAL_100_PERCENT,}, // ADC input @ low mid temp(-10)
	.adc_llt = {1843, 1906, 2155, 2310, 2338, 2367, 2563, 2627, 2688, 2762, 2920, 3069, 3249, 3458, ADC_VAL_100_PERCENT,}, // ADC input @ low low temp(-20)
	.soc	 = {   0,	10,   30,	50,   70,  100,  200,  300,  400,  500,  600,  700,  800,  900, 1000,}, // SoC in %
};

//Discharging Weight(Room/Low/low low)
// latest 0.1C
//Discharging Weight(Room/Low/low low)           //   0,   1,   3,   5,   7,  10,  20,  30,  40,  50,  60,  70,  80,  90, 100
// This is for 0.1C
//static u16 adc_weight_section_discharge_1c[]   = {  5,   8,  10,  45,  90, 115,  43,  50,  75,  80, 100, 100, 100, 100, 145};
static u16 adc_weight_section_discharge_1c[]     = {110, 160, 230, 280, 240, 101,  17,  13,  18,  33,  34,  68,  95, 125, 195};
static u16 adc_weight_section_discharge_rlt_1c[] = {110, 160, 230, 280, 240, 101,  17,  13,  18,  33,  34,  68,  95, 125, 195};
static u16 adc_weight_section_discharge_lt_1c[]  = {110, 160, 230, 280, 240, 101,  17,  13,  18,  33,  34,  68,  95, 125, 195};
static u16 adc_weight_section_discharge_lmt_1c[] = {110, 160, 230, 280, 240, 101,  17,  13,  18,  33,  34,  68,  95, 125, 195};
static u16 adc_weight_section_discharge_llt_1c[] = {110, 160, 230, 280, 240, 101,  17,  13,  18,  33,  34,  68,  95, 125, 195};

// This is for 0.2C
//Discharging Weight(Room/Low/low low)           //   0,   1,   3,   5,   7,  10,  20,  30,  40,  50,  60,  70,  80,  90, 100
                                                 //{ 40,  80, 150, 150, 120,  59,  18,  16,  21,  39,  36,  58,  73,  80, 125};
static u16 adc_weight_section_discharge_2c[]     = {140, 180, 190, 150, 120, 105,  18,  16,  21,  39,  36,  58,  73,  80, 125};
static u16 adc_weight_section_discharge_rlt_2c[] = {140, 180, 190, 150, 120, 105,  18,  16,  21,  39,  36,  58,  73,  80, 125};
static u16 adc_weight_section_discharge_lt_2c[]  = {140, 180, 190, 150, 120, 105,  18,  16,  21,  39,  36,  58,  73,  80, 125};
static u16 adc_weight_section_discharge_lmt_2c[] = {140, 180, 190, 150, 120, 105,  18,  16,  21,  39,  36,  58,  73,  80, 125};
static u16 adc_weight_section_discharge_llt_2c[] = {140, 180, 190, 150, 120, 105,  18,  16,  21,  39,  36,  58,  73,  80, 125};

//Charging Weight(Room/Low/low low)              //    0,   1,   3,   5,   7,  10,  20,  30,  40,  50,  60,  70,  80,  90, 100
//static u16 adc_weight_section_charge[]         = {1000,  90, 110,  95,  68,  38,  14,  13,  15,  32,  35,  36,  40,  36, LAST_CHARGING_WEIGHT};
static u16 adc_weight_section_charge[]           = {1000, 105, 143, 121,  78,  40,  18,  17,  20,  44,  44,  48,  53,  39, LAST_CHARGING_WEIGHT};
static u16 adc_weight_section_charge_rlt[]		 = {1000, 105, 143, 121,  78,  40,  18,  17,  20,  44,  44,  48,  53,  39, LAST_CHARGING_WEIGHT};
static u16 adc_weight_section_charge_lt[]		 = {1000, 105, 143, 121,  78,  40,  18,  17,  20,  44,  44,  48,  53,  39, LAST_CHARGING_WEIGHT};
static u16 adc_weight_section_charge_lmt[]		 = {1000, 105, 143, 121,  78,  40,  18,  17,  20,  44,  44,  48,  53,  39, LAST_CHARGING_WEIGHT};
static u16 adc_weight_section_charge_llt[]		 = {1000, 105, 143, 121,  78,  40,  18,  17,  20,  44,  44,  48,  53,  39, LAST_CHARGING_WEIGHT};

static struct diff_tbl dischg_diff_tbl = {
   //  0,  2-1,  4-3,  6-5,  9-7,19-10,29-20,39-30,49-40
	{173,  173,  143,    8,  15,   54,   40,   37,   50},  // 0.1C
	{314,  314,  314,  145, 145,  177,  209,  221,  216},  // 0.2C
};

static u16 fg_reset_drop_offset[] = {172, 98};

#endif /* USED_BATTERY_CAPACITY == BAT_CAPACITY_????MA */

static u16 adc2soc_lut_length = (u16)sizeof(adc2soc_lut.soc)/sizeof(u16);
static u16 adc2vbat_lut_length = (u16)sizeof(adc2vbat_lut.offset)/sizeof(u16);

#define SEC_FUELGAUGE_I2C_SLAVEADDR (0x6D >> 1)

#define ADC_HISTORY_SIZE	30
enum {
	ADC_CHANNEL_VOLTAGE_AVG = 0,
	ADC_CHANNEL_VOLTAGE_OCV,
	ADC_CHANNEL_NUM
};
		
/* typedef for MFD */
#define sec_fuelgauge_dev_t	struct d2199
#define sec_fuelgauge_pdata_t	struct d2199_platform_data

struct adc_sample_data {
	unsigned int cnt;
	int total_adc;
	int average_adc;
	int adc_arr[ADC_HISTORY_SIZE];
	int index;
};

struct battery_data_t {
	unsigned int adc_check_count;
	unsigned int monitor_polling_time;
	unsigned int channel_voltage;
	unsigned int channel_current;
	const sec_bat_adc_table_data_t *ocv2soc_table;
	unsigned int ocv2soc_table_size;
	const sec_bat_adc_table_data_t *adc2vcell_table;
	unsigned int adc2vcell_table_size;
	u8 *type_str;
};

struct sec_fg_info {
	struct d2199		*pd2199;
	u8 vdd_hwmon_level;
	u32 current_level;

	// for voltage
	u32 origin_volt_adc;
	u32 current_volt_adc;
	u32 average_volt_adc;
	u32 current_voltage;
	u32 average_voltage;
	u32 sum_voltage_adc;
	u8  reset_total_adc;
	int sum_total_adc;

	// for temperature
	u32 current_temp_adc;
	u32 average_temp_adc;
	int current_temperature;
	int average_temperature;
	u32 sum_temperature_adc;

	u32 soc;
	u32 prev_soc;

	int battery_technology;
	int battery_present;
	u32 capacity;

	u16 average_vbat_init_adc;
	u16 vbat_init_adc[3];

	u16 vf_adc;
	u32 vf_ohm;
	u32 vf_lower;
	u32 vf_upper; 

	u32 voltage_adc[AVG_SIZE];
	u32 temperature_adc[AVG_SIZE];
	u8 voltage_idx;
	u8 temperature_idx;

	u8 volt_adc_init_done;
	u8 temp_adc_init_done;
	struct wake_lock sleep_monitor_wakeup;
	struct delayed_work monitor_volt_work;
	struct delayed_work monitor_temp_work;
	struct adc_man_res adc_res[D2199_ADC_CHANNEL_MAX];
	u8 virtual_battery_full;
	int charging_current;

	u8 adc_mode;
	struct mutex adclock;

	int (*d2199_read_adc)(struct sec_fuelgauge_info *fuelgauge, adc_channel channel);
};

#endif /* __ADC_FUELGAUGE_H */
