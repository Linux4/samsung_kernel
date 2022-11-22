/*
 * Battery driver for Dialog D2199
 *   
 * Copyright(c) 2013 Dialog Semiconductor Ltd.
 *  
 * Author: Dialog Semiconductor Ltd. D. Chen, A Austin, E Jeong
 *  
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/errno.h>
#include <linux/power_supply.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>
#include <linux/i2c.h>
#include <linux/proc_fs.h>
#include <linux/mutex.h>
#include <linux/delay.h>
#include <linux/wakelock.h>
#include <linux/gpio.h>

#include <linux/kthread.h>
#include <mach/spa.h>

#include "linux/err.h"	// test only

#include <linux/d2199/core.h>
#include <linux/d2199/d2199_battery.h>
#include <linux/d2199/d2199_reg.h>

static const char __initdata d2199_battery_banner[] = \
    "D2199 Battery, (c) 2012 Dialog Semiconductor Ltd.\n";

/***************************************************************************
 Pre-definition
***************************************************************************/
#define FALSE								(0)
#define TRUE								(1)

#define ADC_RES_MASK_LSB					(0x0F)
#define ADC_RES_MASK_MSB					(0xF0)

#if USED_BATTERY_CAPACITY == BAT_CAPACITY_1800MA
#define ADC_VAL_100_PERCENT            		3645   // About 4280mV 
#define CV_START_ADC            			3275   // About 4100mV
#define MAX_FULL_CHARGED_ADC				3768   // About 4340mV
#define ORIGN_CV_START_ADC					3686   // About 4300mV
#define ORIGN_FULL_CHARGED_ADC				3780   // About 4345mV

#define D2153_BAT_CHG_FRST_FULL_LVL         4310   // About EOC 160mA
#define D2153_BAT_CHG_BACK_FULL_LVL         4347   // 4340mV -> 4347mV

#define FIRST_VOLTAGE_DROP_ADC			121

#define MAX_ADD_DIS_PERCENT_FOR_WEIGHT2	27    // 30    // 35
#define MAX_ADD_DIS_PERCENT_FOR_WEIGHT1	21    // 23    // 26 // 23
#define MAX_ADD_DIS_PERCENT_FOR_WEIGHT0_5   18    // 22 // 19  // 15
#define MAX_ADD_DIS_PERCENT_FOR_WEIGHT	14    // 18 15
#define MIN_ADD_DIS_PERCENT_FOR_WEIGHT	(-30)

#define LAST_CHARGING_WEIGHT			450   // 900
#define VBAT_3_4_VOLTAGE_ADC				1605
#elif USED_BATTERY_CAPACITY == BAT_CAPACITY_2100MA
#define ADC_VAL_100_PERCENT	3645   // About 4280mV
#define CV_START_ADC		3275   // About 4100mV
#define MAX_FULL_CHARGED_ADC				3768   // About 4340mV
#define ORIGN_CV_START_ADC					3686   // About 4300mV
#define ORIGN_FULL_CHARGED_ADC				3780   // About 4345mV

#define D2153_BAT_CHG_FRST_FULL_LVL         4310   // About EOC 160mA
#define D2153_BAT_CHG_BACK_FULL_LVL         4340   // About EOC 50mA

#define FIRST_VOLTAGE_DROP_ADC		156

#define MAX_ADD_DIS_PERCENT_FOR_WEIGHT2	27    // 30    // 35
#define MAX_ADD_DIS_PERCENT_FOR_WEIGHT1	21    // 23    // 26 // 23
#define MAX_ADD_DIS_PERCENT_FOR_WEIGHT0_5   18    // 22 // 19  // 15
#define MAX_ADD_DIS_PERCENT_FOR_WEIGHT	14    // 18 15
#define MIN_ADD_DIS_PERCENT_FOR_WEIGHT	(-30) // (-20) // (-63) // (-40)

#define LAST_CHARGING_WEIGHT		450   // 900
#define VBAT_3_4_VOLTAGE_ADC		1605

#else
#define ADC_VAL_100_PERCENT   		3445
#define CV_START_ADC			3338
#define MAX_FULL_CHARGED_ADC				3470
#define ORIGN_CV_START_ADC					3320
#define ORIGN_FULL_CHARGED_ADC				3480   // About 4345mV

#define D2153_BAT_CHG_FRST_FULL_LVL         4160   // About EOC 160mA
#define D2153_BAT_CHG_BACK_FULL_LVL         4185   // About EOC 60mA

#define FIRST_VOLTAGE_DROP_ADC		165

#define MAX_ADD_DIS_PERCENT_FOR_WEIGHT2	27    // 30    // 35
#define MAX_ADD_DIS_PERCENT_FOR_WEIGHT1	21    // 23    // 26 // 23
#define MAX_ADD_DIS_PERCENT_FOR_WEIGHT0_5   18    // 22 // 19  // 15
#define MAX_ADD_DIS_PERCENT_FOR_WEIGHT	14    // 18 15
#define MIN_ADD_DIS_PERCENT_FOR_WEIGHT	(-30)

#define LAST_CHARGING_WEIGHT		450   // 900
#define VBAT_3_4_VOLTAGE_ADC		1605
#endif /* USED_BATTERY_CAPACITY == BAT_CAPACITY_????MA */

#define FULL_CAPACITY						1000

#define NORM_NUM			10000
#define MAX_WEIGHT							10000
#define MAX_DIS_OFFSET_FOR_WEIGHT2			300
#define MAX_DIS_OFFSET_FOR_WEIGHT1	200
#define MAX_DIS_OFFSET_FOR_WEIGHT0_5	150
#define MAX_DIS_OFFSET_FOR_WEIGHT			100
#define MIN_DIS_OFFSET_FOR_WEIGHT   		30

#define DISCHARGE_SLEEP_OFFSET              55    // 45
#define LAST_VOL_UP_PERCENT                 75

//////////////////////////////////////////////////////////////////////////////
//    Static Function Prototype
//////////////////////////////////////////////////////////////////////////////
static int  d2199_read_adc_in_auto(struct d2199_battery *pbat, adc_channel channel);
static int  d2199_read_adc_in_manual(struct d2199_battery *pbat, adc_channel channel);

//////////////////////////////////////////////////////////////////////////////
//    Static Variable Declaration
//////////////////////////////////////////////////////////////////////////////

static struct d2199_battery *gbat = NULL;
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
static struct adc2soc_lookuptbl adc2soc_lut = {
	.adc_ht  = {1843, 1906, 2056, 2213, 2310, 2396, 2563, 2627, 2688, 2762, 2920, 3069, 3249, 3458, ADC_VAL_100_PERCENT,}, // ADC input @ high temp
	.adc_rt  = {1843, 1906, 2056, 2213, 2310, 2396, 2563, 2627, 2688, 2762, 2920, 3069, 3249, 3458, ADC_VAL_100_PERCENT,}, // ADC input @ room temp
	.adc_rlt = {1843, 1906, 2056, 2213, 2310, 2396, 2563, 2627, 2688, 2762, 2920, 3069, 3249, 3458, ADC_VAL_100_PERCENT,}, // ADC input @ low temp(0)
	.adc_lt  = {1843, 1906, 2056, 2213, 2310, 2396, 2563, 2627, 2688, 2762, 2920, 3069, 3249, 3458, ADC_VAL_100_PERCENT,}, // ADC input @ low temp(0)
	.adc_lmt = {1843, 1906, 2056, 2213, 2310, 2396, 2563, 2627, 2688, 2762, 2920, 3069, 3249, 3458, ADC_VAL_100_PERCENT,}, // ADC input @ low mid temp(-10)
	.adc_llt = {1843, 1906, 2056, 2213, 2310, 2396, 2563, 2627, 2688, 2762, 2920, 3069, 3249, 3458, ADC_VAL_100_PERCENT,}, // ADC input @ low low temp(-20)
	.soc	 = {   0,	10,   30,	50,   70,  100,  200,  300,  400,  500,  600,  700,  800,  900, 1000,}, // SoC in %
};

//Discharging Weight(Room/Low/low low)          //     0,    1,     3,    5,    7,   10,   20,   30,   40,   50,   60,   70,   80,   90,  100
static u16 adc_weight_section_discharge[]       = {14100, 9100,  7585, 2418, 1025,  375,  101,   98,  142,  356,  355,  514,  769, 1228, 2495};
static u16 adc_weight_section_discharge_rlt[]   = {14100, 9100,  8985, 1585,  525,  352,  132,  119,  165,  375,  355,  514,  769, 1228, 2495};
static u16 adc_weight_section_discharge_lt[]    = {14100, 9100,  8985, 1585,  525,  352,  132,  119,  165,  375,  355,  514,  769, 1228, 2495};
static u16 adc_weight_section_discharge_lmt[]   = {14100, 9100,  8985, 1585,  525,  352,  132,  119,  165,  375,  355,  514,  769, 1228, 2495};
static u16 adc_weight_section_discharge_llt[]   = {19100, 9400, 10985, 1985,  480,  337,  132,  119,  165,  375,  355,  514,  769, 1228, 2495};

//Charging Weight(Room/Low/low low)             //     0,    1,     3,    5,    7,   10,   20,   30,   40,   50,   60,   70,   80,   90, 100
static u16 adc_weight_section_charge[]          = { 9700,  734,   585,  453,  321,  248,   95,   93,  105,  223,  219,  265,  281,  288, LAST_CHARGING_WEIGHT};
static u16 adc_weight_section_charge_rlt[]		= { 9700,  734,   488,  385,  225,  225,   95,   93,  105,  218,  216,  259,  281,  303, LAST_CHARGING_WEIGHT};
static u16 adc_weight_section_charge_lt[]		= { 9700,  734,   488,  385,  225,  225,   95,   93,  105,  218,  216,  259,  281,  303, LAST_CHARGING_WEIGHT};
static u16 adc_weight_section_charge_lmt[]		= { 9700,  734,   488,  385,  225,  225,   95,   93,  105,  218,  216,  259,  281,  303, LAST_CHARGING_WEIGHT};
static u16 adc_weight_section_charge_llt[]		= { 9700,  734,   488,  385,  225,  225,   95,   93,  105,  218,  216,  259,  281,  303, LAST_CHARGING_WEIGHT};

#elif USED_BATTERY_CAPACITY == BAT_CAPACITY_2100MA
static struct adc2soc_lookuptbl adc2soc_lut = {
	.adc_ht  = {1843, 1891, 1980, 2081, 2171, 2259, 2423, 2468, 2515, 2607, 2744, 2937, 3162, 3402, ADC_VAL_100_PERCENT,}, // ADC input @ high temp
	.adc_rt  = {1843, 1891, 1980, 2081, 2171, 2259, 2423, 2468, 2515, 2607, 2744, 2937, 3162, 3402, ADC_VAL_100_PERCENT,}, // ADC input @ room temp
	.adc_rlt = {1843, 1891, 1980, 2081, 2171, 2259, 2423, 2468, 2515, 2607, 2744, 2937, 3162, 3402, ADC_VAL_100_PERCENT,}, // ADC input @ low temp(0)
	.adc_lt  = {1843, 1891, 1980, 2081, 2171, 2259, 2423, 2468, 2515, 2607, 2744, 2937, 3162, 3402, ADC_VAL_100_PERCENT,}, // ADC input @ low temp(0)
	.adc_lmt = {1843, 1891, 1980, 2081, 2171, 2259, 2423, 2468, 2515, 2607, 2744, 2937, 3162, 3402, ADC_VAL_100_PERCENT,}, // ADC input @ low mid temp(-10)
	.adc_llt = {1843, 1891, 1980, 2081, 2171, 2259, 2423, 2468, 2515, 2607, 2744, 2937, 3162, 3402, ADC_VAL_100_PERCENT,}, // ADC input @ low low temp(-20)
	.soc	 = {   0,	10,   30,	50,   70,  100,  200,  300,  400,  500,  600,  700,  800,  900, 1000,}, // SoC in %
};

//Discharging Weight(Room/Low/low low)          //     0,    1,     3,    5,    7,   10,   20,   30,   40,   50,   60,   70,   80,   90,  100
static u16 adc_weight_section_discharge[]       = { 9895, 4150,  3385,  935,  393,  335,  351,  595,  718,  733,  843,  935, 1002, 1328, 2495};
static u16 adc_weight_section_discharge_rlt[]   = { 9895, 4150,  8985, 1585,  525,  352,  132,  119,  165,  375,  355,  514,  769, 1228, 2495};
static u16 adc_weight_section_discharge_lt[]    = { 9895, 4150,  8985, 1585,  525,  352,  132,  119,  165,  375,  355,  514,  769, 1228, 2495};
static u16 adc_weight_section_discharge_lmt[]   = { 9895, 4150,  8985, 1585,  525,  352,  132,  119,  165,  375,  355,  514,  769, 1228, 2495};
static u16 adc_weight_section_discharge_llt[]   = { 9895, 4150, 10985, 1985,  480,  337,  132,  119,  165,  375,  355,  514,  769, 1228, 2495};

//Charging Weight(Room/Low/low low)             //     0,    1,     3,    5,    7,   10,   20,   30,   40,   50,   60,   70,   80,   90, 100
static u16 adc_weight_section_charge[]          = { 9700,  814,   473,  358,  221,  149,   46,   43,   81,  146,  219,  268,  287,  269, LAST_CHARGING_WEIGHT};
static u16 adc_weight_section_charge_rlt[]		= { 9700,  814,   488,  385,  225,  225,   95,   93,  105,  218,  216,  259,  281,  303, LAST_CHARGING_WEIGHT};
static u16 adc_weight_section_charge_lt[]		= { 9700,  814,   488,  385,  225,  225,   95,   93,  105,  218,  216,  259,  281,  303, LAST_CHARGING_WEIGHT};
static u16 adc_weight_section_charge_lmt[]		= { 9700,  814,   488,  385,  225,  225,   95,   93,  105,  218,  216,  259,  281,  303, LAST_CHARGING_WEIGHT};
static u16 adc_weight_section_charge_llt[]		= { 9700,  814,   488,  385,  225,  225,   95,   93,  105,  218,  216,  259,  281,  303, LAST_CHARGING_WEIGHT};

#else
static struct adc2soc_lookuptbl adc2soc_lut = {
	.adc_ht  = {1800, 1870, 2060, 2270, 2400, 2510, 2585, 2635, 2685, 2781, 2933, 3064, 3230, ADC_VAL_100_PERCENT,}, // ADC input @ high temp
	.adc_rt  = {1800, 1870, 2060, 2270, 2400, 2510, 2585, 2635, 2685, 2781, 2933, 3064, 3230, ADC_VAL_100_PERCENT,}, // ADC input @ room temp
	.adc_rlt = {1800, 1860, 2038, 2236, 2367, 2481, 2551, 2602, 2650, 2750, 2901, 3038, 3190, 3390,               }, // ADC input @ low temp(0)
	.adc_lt  = {1800, 1854, 2000, 2135, 2270, 2390, 2455, 2575, 2645, 2740, 2880, 3020, 3160, 3320,               }, // ADC input @ low temp(0)
	.adc_lmt = {1800, 1853, 1985, 2113, 2243, 2361, 2428, 2538, 2610, 2705, 2840, 2985, 3125, 3260,               }, // ADC input @ low mid temp(-10)
	.adc_llt = {1800, 1850, 1978, 2105, 2235, 2342, 2405, 2510, 2595, 2680, 2786, 2930, 3040, 3160,               }, // ADC input @ low low temp(-20)
	.soc	 = {   0,	10,   30,	50,  100,  200,  300,  400,	 500,  600,	 700,  800,	 900, 1000,               }, // SoC in %
};

//Discharging Weight(Room/Low/low low)          //    0,    1,    3,    5,   10,   20,   30,   40,   50,   60,   70,   80,   90,  100
static u16 adc_weight_section_discharge[]       = {5000, 3500, 2000,  600,  260,  220,  115,  112,  280,  450,  420,  600,  800, 1000};
static u16 adc_weight_section_discharge_rlt[]   = {3640, 2740,  966,  466,  140,  120,   93,   80,  128,  151,  150,  176,  186,  780};
static u16 adc_weight_section_discharge_lt[]    = {3200, 2120,  860,  356,  111,   90,   68,   64,   96,  106,  110,  130,  139,  710};
static u16 adc_weight_section_discharge_lmt[]   = {2920, 1850,  756,  326,   94,   79,   65,   57,   81,   96,   99,  121,  128,  670};
static u16 adc_weight_section_discharge_llt[]   = {2730, 1840,  710,  300,   70,   62,   55,   51,   63,   71,   73,   79,   85,  630};

//Charging Weight(Room/Low/low low)             //    0,    1,    3,    5,   10,   20,   30,   40,   50,   60,   70,   80,   90,  100
static u16 adc_weight_section_charge[]          = {7000, 5000, 2000,  500,  213,  130,   82,   86,  157,  263,  280,  310,  340,  600};
static u16 adc_weight_section_charge_rlt[]      = {7000, 5000, 2000,  500,  213,  130,   82,   86,  157,  263,  280,  310,  340,  600};
static u16 adc_weight_section_charge_lt[]       = {7000, 5000, 2000,  500,  213,  130,   82,   86,  157,  263,  280,  310,  340,  600};
static u16 adc_weight_section_charge_lmt[]      = {7000, 5000, 2000,  500,  213,  130,   82,   86,  157,  263,  280,  310,  340,  600};
static u16 adc_weight_section_charge_llt[]      = {7000, 5000, 2000,  500,  213,  130,   82,   86,  157,  263,  280,  310,  340,  600};

#endif /* USED_BATTERY_CAPACITY == BAT_CAPACITY_????MA */

static u16 adc2soc_lut_length = (u16)sizeof(adc2soc_lut.soc)/sizeof(u16);
static u16 adc2vbat_lut_length = (u16)sizeof(adc2vbat_lut.offset)/sizeof(u16);

/* 
 * Name : chk_lut
 *
 */
static int chk_lut (u16* x, u16* y, u16 v, u16 l) {
	int i;
	//u32 ret;
	int ret;

	if (v < x[0])
		ret = y[0];
	else if (v >= x[l-1])
		ret = y[l-1]; 
	else {          
		for (i = 1; i < l; i++) {          
			if (v < x[i])               
				break;      
		}       
		ret = y[i-1];       
		ret = ret + ((v-x[i-1])*(y[i]-y[i-1]))/(x[i]-x[i-1]);   
	}   
	//return (u16) ret;
	return ret;
}

/* 
 * Name : chk_lut_temp
 * return : The return value is Kelvin degree
 */
static int chk_lut_temp (u16* x, u16* y, u16 v, u16 l) {
	int i, ret;

	if (v >= x[0])
		ret = y[0];
	else if (v < x[l-1])
		ret = y[l-1]; 
	else {			
		for (i=1; i < l; i++) { 		 
			if (v > x[i])				
				break;		
		}		
		ret = y[i-1];		
		ret = ret + ((v-x[i-1])*(y[i]-y[i-1]))/(x[i]-x[i-1]);	
	}
	
	return ret;
}


/* 
 * Name : adc_to_soc_with_temp_compensat
 *
 */
u32 adc_to_soc_with_temp_compensat(u16 adc, u16 temp) {	
	int sh, sl;

	if(temp < BAT_LOW_LOW_TEMPERATURE)
		temp = BAT_LOW_LOW_TEMPERATURE;
	else if(temp > BAT_HIGH_TEMPERATURE)
		temp = BAT_HIGH_TEMPERATURE;

	if((temp <= BAT_HIGH_TEMPERATURE) && (temp > BAT_ROOM_TEMPERATURE)) {
		sh = chk_lut(adc2soc_lut.adc_ht, adc2soc_lut.soc, adc, adc2soc_lut_length);    
		sl = chk_lut(adc2soc_lut.adc_rt, adc2soc_lut.soc, adc, adc2soc_lut_length);
		sh = sl + (temp - BAT_ROOM_TEMPERATURE)*(sh - sl)
								/ (BAT_HIGH_TEMPERATURE - BAT_ROOM_TEMPERATURE);
	} else if((temp <= BAT_ROOM_TEMPERATURE) && (temp > BAT_ROOM_LOW_TEMPERATURE)) {
		sh = chk_lut(adc2soc_lut.adc_rt, adc2soc_lut.soc, adc, adc2soc_lut_length); 	   
		sl = chk_lut(adc2soc_lut.adc_rlt, adc2soc_lut.soc, adc, adc2soc_lut_length); 	   
		sh = sl + (temp - BAT_ROOM_LOW_TEMPERATURE)*(sh - sl)
								/ (BAT_ROOM_TEMPERATURE-BAT_ROOM_LOW_TEMPERATURE);
	} else if((temp <= BAT_ROOM_LOW_TEMPERATURE) && (temp > BAT_LOW_TEMPERATURE)) {
		sh = chk_lut(adc2soc_lut.adc_rlt, adc2soc_lut.soc, adc, adc2soc_lut_length); 	   
		sl = chk_lut(adc2soc_lut.adc_lt, adc2soc_lut.soc, adc, adc2soc_lut_length); 	   
		sh = sl + (temp - BAT_LOW_TEMPERATURE)*(sh - sl)
								/ (BAT_ROOM_LOW_TEMPERATURE-BAT_LOW_TEMPERATURE);
	} else if((temp <= BAT_LOW_TEMPERATURE) && (temp > BAT_LOW_MID_TEMPERATURE)) {
		sh = chk_lut(adc2soc_lut.adc_lt, adc2soc_lut.soc, adc, adc2soc_lut_length); 	   
		sl = chk_lut(adc2soc_lut.adc_lmt, adc2soc_lut.soc, adc, adc2soc_lut_length);		
		sh = sl + (temp - BAT_LOW_MID_TEMPERATURE)*(sh - sl)
								/ (BAT_LOW_TEMPERATURE-BAT_LOW_MID_TEMPERATURE);	
	} else {		
		sh = chk_lut(adc2soc_lut.adc_lmt, adc2soc_lut.soc,	adc, adc2soc_lut_length);		 
		sl = chk_lut(adc2soc_lut.adc_llt, adc2soc_lut.soc, adc, adc2soc_lut_length);
		sh = sl + (temp - BAT_LOW_LOW_TEMPERATURE)*(sh - sl)
								/ (BAT_LOW_MID_TEMPERATURE-BAT_LOW_LOW_TEMPERATURE); 
	}

	return sh;
}


/*
 * Name : soc_to_adc_with_temp_compensat
 *
 */
u32 soc_to_adc_with_temp_compensat(u16 soc, u16 temp) {
	int sh, sl;

	if(temp < BAT_LOW_LOW_TEMPERATURE)
		temp = BAT_LOW_LOW_TEMPERATURE;
	else if(temp > BAT_HIGH_TEMPERATURE)
		temp = BAT_HIGH_TEMPERATURE;

	pr_info("%s. Parameter. SOC = %d. temp = %d\n", __func__, soc, temp);

	if((temp <= BAT_HIGH_TEMPERATURE) && (temp > BAT_ROOM_TEMPERATURE)) {
		sh = chk_lut(adc2soc_lut.soc, adc2soc_lut.adc_ht, soc, adc2soc_lut_length);
		sl = chk_lut(adc2soc_lut.soc, adc2soc_lut.adc_rt, soc, adc2soc_lut_length);
		sh = sl + (temp - BAT_ROOM_TEMPERATURE)*(sh - sl)
								/ (BAT_HIGH_TEMPERATURE - BAT_ROOM_TEMPERATURE);
	} else if((temp <= BAT_ROOM_TEMPERATURE) && (temp > BAT_ROOM_LOW_TEMPERATURE)) {
		sh = chk_lut(adc2soc_lut.soc, adc2soc_lut.adc_rt, soc, adc2soc_lut_length);
		sl = chk_lut(adc2soc_lut.soc, adc2soc_lut.adc_rlt, soc, adc2soc_lut_length);
		sh = sl + (temp - BAT_ROOM_LOW_TEMPERATURE)*(sh - sl)
								/ (BAT_ROOM_TEMPERATURE-BAT_ROOM_LOW_TEMPERATURE);
	} else if((temp <= BAT_ROOM_LOW_TEMPERATURE) && (temp > BAT_LOW_TEMPERATURE)) {
		sh = chk_lut(adc2soc_lut.soc, adc2soc_lut.adc_rlt, soc, adc2soc_lut_length);
		sl = chk_lut(adc2soc_lut.soc, adc2soc_lut.adc_lt, soc, adc2soc_lut_length);
		sh = sl + (temp - BAT_LOW_TEMPERATURE)*(sh - sl)
								/ (BAT_ROOM_LOW_TEMPERATURE-BAT_LOW_TEMPERATURE);
	} else if((temp <= BAT_LOW_TEMPERATURE) && (temp > BAT_LOW_MID_TEMPERATURE)) {
		sh = chk_lut(adc2soc_lut.soc, adc2soc_lut.adc_lt, soc, adc2soc_lut_length);
		sl = chk_lut(adc2soc_lut.soc, adc2soc_lut.adc_lmt, soc, adc2soc_lut_length);
		sh = sl + (temp - BAT_LOW_MID_TEMPERATURE)*(sh - sl)
								/ (BAT_LOW_TEMPERATURE-BAT_LOW_MID_TEMPERATURE);
	} else {
		sh = chk_lut(adc2soc_lut.soc, adc2soc_lut.adc_lmt,	soc, adc2soc_lut_length);
		sl = chk_lut(adc2soc_lut.soc, adc2soc_lut.adc_llt, soc, adc2soc_lut_length);
		sh = sl + (temp - BAT_LOW_LOW_TEMPERATURE)*(sh - sl)
								/ (BAT_LOW_MID_TEMPERATURE-BAT_LOW_LOW_TEMPERATURE);
	}

	return sh;
}



static u16 pre_soc = 0xffff;
/* 
 * Name : soc_filter
 *
 */
u16 soc_filter(u16 new_soc, u8 is_charging) {
	u16 soc = new_soc;

	if(pre_soc == 0xffff)
		pre_soc = soc;
	else {
		if( soc > pre_soc)
		{
			if(is_charging)
			{
				if(soc <= pre_soc + 2)
					pre_soc = soc;
				else {
					soc = pre_soc + 1;
					pre_soc = soc;
				}
			} else
				soc = pre_soc; //in discharge, SoC never goes up
		} else {
			if(soc >= pre_soc - 2)
				pre_soc = soc;
			else {
				soc = pre_soc - 1;
				pre_soc = soc;
			}
		}
	}
	return (soc);
}


/* 
 * Name : adc_to_degree
 *
 */
int adc_to_degree_k(u16 adc) {
    return (chk_lut_temp(adc2temp_lut.adc, adc2temp_lut.temp, adc, temp_lut_length));
}

int degree_k2c(u16 k) {
	return (K2C(k));
}

/* 
 * Name : get_adc_offset
 *
 */
int get_adc_offset(u16 adc) {	
    return (chk_lut(adc2vbat_lut.adc, adc2vbat_lut.offset, adc, adc2vbat_lut_length));
}

/* 
 * Name : adc_to_vbat
 *
 */
u16 adc_to_vbat(u16 adc, u8 is_charging) {    
	u16 a = adc;

	if(is_charging)
		a = adc - get_adc_offset(adc); // deduct charging offset
	// return (chk_lut(adc2vbat_lut.adc, adc2vbat_lut.vbat, a, adc2vbat_lut_length));
	return (2500 + ((a * 2000) >> 12));
}

/* 
 * Name : adc_to_soc
 * get SOC (@ room temperature) according ADC input
 */
int adc_to_soc(u16 adc, u8 is_charging) { 

	u16 a = adc;

	if(is_charging)
		a = adc - get_adc_offset(adc); // deduct charging offset
	return (chk_lut(adc2soc_lut.adc_rt, adc2soc_lut.soc, a, adc2soc_lut_length));
}


/* 
 * Name : do_interpolation
 */
int do_interpolation(int x0, int x1, int y0, int y1, int x)
{
	int y = 0;
	
	if(!(x1 - x0 )) {
		pr_err("%s. Divied by Zero. Plz check x1(%d), x0(%d) value \n",
				__func__, x1, x0);
		return 0;
	}

	y = y0 + (x - x0)*(y1 - y0)/(x1 - x0);
	pr_info("%s. Interpolated y_value is = %d\n", __func__, y);

	return y;
}


/* 
 * Name : d2199_get_soc
 */
static int d2199_get_soc(struct d2199_battery *pbat)
{
	//u8 is_charger = 0;
	int soc;
 	struct d2199_battery_data *pbat_data = NULL;

	if((pbat == NULL) || (!pbat->battery_data.volt_adc_init_done)) {
		pr_err("%s. Invalid parameter. \n", __func__);
		return -EINVAL;
	}

	pbat_data = &pbat->battery_data;

	if(pbat_data->soc)
		pbat_data->prev_soc = pbat_data->soc;

	soc = adc_to_soc_with_temp_compensat(pbat_data->average_volt_adc, 
										C2K(pbat_data->average_temperature));
	if(soc <= 0) {
		pbat_data->soc = 0;
		if(pbat_data->current_voltage >= BAT_POWER_OFF_VOLTAGE
			|| (pbat_data->is_charging == TRUE)) {
			soc = 10;
		}
	}
	else if(soc >= FULL_CAPACITY) {
		soc = FULL_CAPACITY;
		if(pbat_data->virtual_battery_full == 1) {
			pbat_data->virtual_battery_full = 0;
			pbat_data->soc = FULL_CAPACITY;
		}
	}


	/* Don't allow soc goes up when battery is dicharged.
	 and also don't allow soc goes down when battey is charged. */
	if(pbat_data->is_charging != TRUE 
		&& (soc > pbat_data->prev_soc && pbat_data->prev_soc )) {
		soc = pbat_data->prev_soc;
	}
	else if(pbat_data->is_charging
		&& (soc < pbat_data->prev_soc) && pbat_data->prev_soc) {
		soc = pbat_data->prev_soc;
	}
	pbat_data->soc = soc;

	d2199_reg_write(pbat->pd2199, D2199_GP_ID_2_REG, (0xFF & soc));
	d2199_reg_write(pbat->pd2199, D2199_GP_ID_3_REG, (0x0F & (soc>>8)));

	d2199_reg_write(pbat->pd2199, D2199_GP_ID_4_REG,
							(0xFF & pbat_data->average_volt_adc));
	d2199_reg_write(pbat->pd2199, D2199_GP_ID_5_REG,
							(0xF & (pbat_data->average_volt_adc>>8)));

	return soc;
}


/* 
 * Name : d2199_get_weight_from_lookup
 */
static u16 d2199_get_weight_from_lookup(u16 tempk, u16 average_adc, u8 is_charging)
{
	u8 i = 0;
	u16 *plut = NULL;
	int weight = 0;

	// Sanity check.
	if (tempk < BAT_LOW_LOW_TEMPERATURE)		
		tempk = BAT_LOW_LOW_TEMPERATURE;
	else if (tempk > BAT_HIGH_TEMPERATURE)
		tempk = BAT_HIGH_TEMPERATURE;

	// Get the SOC look-up table
	if(tempk >= BAT_HIGH_TEMPERATURE) {
		plut = &adc2soc_lut.adc_ht[0];
	} else if(tempk < BAT_HIGH_TEMPERATURE && tempk >= BAT_ROOM_TEMPERATURE) {
		plut = &adc2soc_lut.adc_rt[0];
	} else if(tempk < BAT_ROOM_TEMPERATURE && tempk >= BAT_ROOM_LOW_TEMPERATURE) {
		plut = &adc2soc_lut.adc_rlt[0];
	} else if (tempk < BAT_ROOM_LOW_TEMPERATURE && tempk >= BAT_LOW_TEMPERATURE) {
		plut = &adc2soc_lut.adc_lt[0];
	} else if(tempk < BAT_LOW_TEMPERATURE && tempk >= BAT_LOW_MID_TEMPERATURE) {
		plut = &adc2soc_lut.adc_lmt[0];
	} else 
		plut = &adc2soc_lut.adc_llt[0];

	for(i = adc2soc_lut_length - 1; i; i--) {
		if(plut[i] <= average_adc)
			break;
	}

	if((tempk <= BAT_HIGH_TEMPERATURE) && (tempk > BAT_ROOM_TEMPERATURE)) {  
		if(is_charging) {
			if(average_adc < plut[0]) {
				// under 1% -> fast charging
				weight = adc_weight_section_charge[0];
			} else
				weight = adc_weight_section_charge[i];
		} else
			weight = adc_weight_section_discharge[i];
	} else if((tempk <= BAT_ROOM_TEMPERATURE) && (tempk > BAT_ROOM_LOW_TEMPERATURE)) {
		if(is_charging) {
			if(average_adc < plut[0]) i = 0;
		
			weight=adc_weight_section_charge_rlt[i];
			weight = weight + ((tempk-BAT_ROOM_LOW_TEMPERATURE)
				*(adc_weight_section_charge[i]-adc_weight_section_charge_rlt[i]))
							/(BAT_ROOM_TEMPERATURE-BAT_ROOM_LOW_TEMPERATURE);
		} else {
			weight=adc_weight_section_discharge_rlt[i];
			weight = weight + ((tempk-BAT_ROOM_LOW_TEMPERATURE)
				*(adc_weight_section_discharge[i]-adc_weight_section_discharge_rlt[i]))
							/(BAT_ROOM_TEMPERATURE-BAT_ROOM_LOW_TEMPERATURE); 
		}
	} else if((tempk <= BAT_ROOM_LOW_TEMPERATURE) && (tempk > BAT_LOW_TEMPERATURE)) {
		if(is_charging) {
			if(average_adc < plut[0]) i = 0;
		
			weight = adc_weight_section_charge_lt[i];
			weight = weight + ((tempk-BAT_LOW_TEMPERATURE)
				*(adc_weight_section_charge_rlt[i]-adc_weight_section_charge_lt[i]))
								/(BAT_ROOM_LOW_TEMPERATURE-BAT_LOW_TEMPERATURE);
		} else {
			weight = adc_weight_section_discharge_lt[i];
			weight = weight + ((tempk-BAT_LOW_TEMPERATURE)
				*(adc_weight_section_discharge_rlt[i]-adc_weight_section_discharge_lt[i]))
								/(BAT_ROOM_LOW_TEMPERATURE-BAT_LOW_TEMPERATURE); 
		}
	} else if((tempk <= BAT_LOW_TEMPERATURE) && (tempk > BAT_LOW_MID_TEMPERATURE)) {
		if(is_charging) {
			if(average_adc < plut[0]) i = 0;
		
			weight = adc_weight_section_charge_lmt[i];
			weight = weight + ((tempk-BAT_LOW_MID_TEMPERATURE)
				*(adc_weight_section_charge_lt[i]-adc_weight_section_charge_lmt[i]))
								/(BAT_LOW_TEMPERATURE-BAT_LOW_MID_TEMPERATURE);
		} else {
			weight = adc_weight_section_discharge_lmt[i];
			weight = weight + ((tempk-BAT_LOW_MID_TEMPERATURE)
				*(adc_weight_section_discharge_lt[i]-adc_weight_section_discharge_lmt[i]))
								/(BAT_LOW_TEMPERATURE-BAT_LOW_MID_TEMPERATURE); 
		}
	} else {		
		if(is_charging) {
			if(average_adc < plut[0]) i = 0;

			weight = adc_weight_section_charge_llt[i];
			weight = weight + ((tempk-BAT_LOW_LOW_TEMPERATURE)
				*(adc_weight_section_charge_lmt[i]-adc_weight_section_charge_llt[i]))
								/(BAT_LOW_MID_TEMPERATURE-BAT_LOW_LOW_TEMPERATURE); 
		} else {
			weight = adc_weight_section_discharge_llt[i];
			weight = weight + ((tempk-BAT_LOW_LOW_TEMPERATURE)
				*(adc_weight_section_discharge_lmt[i]-adc_weight_section_discharge_llt[i]))
								/(BAT_LOW_MID_TEMPERATURE-BAT_LOW_LOW_TEMPERATURE); 
		}
	}

	return weight;	

}


/* 
 * Name : d2199_set_adc_mode
 */
static int d2199_set_adc_mode(struct d2199_battery *pbat, adc_mode type)
{
	if(unlikely(!pbat)) {
		pr_err("%s. Invalid parameter.\n", __func__);
		return -EINVAL;
	}

	if(pbat->adc_mode != type)
	{
		if(type == D2199_ADC_IN_AUTO) {
			pbat->d2199_read_adc = d2199_read_adc_in_auto;
			pbat->adc_mode = D2199_ADC_IN_AUTO;
		}
		else if(type == D2199_ADC_IN_MANUAL) {
			pbat->d2199_read_adc = d2199_read_adc_in_manual;
			pbat->adc_mode = D2199_ADC_IN_MANUAL;
		}
	}
	else {
		pr_info("%s: ADC mode is same before was set \n", __func__);
	}

	return 0;
}


/* 
 * Name : d2199_read_adc_in_auto
 * Desc : Read ADC raw data for each channel.
 * Param : 
 *    - d2199 : 
 *    - channel : voltage, temperature 1, temperature 2, VF and TJUNC* Name : d2199_set_end_of_charge
 */
static int d2199_read_adc_in_auto(struct d2199_battery *pbat, adc_channel channel)
{
	u8 msb_res, lsb_res;
	int ret = 0;
	struct d2199_battery_data *pbat_data = &pbat->battery_data;
	struct d2199 *d2199 = pbat->pd2199;

	if(unlikely(!pbat_data || !d2199)) {
		pr_err("%s. Invalid argument\n", __func__);
		return -EINVAL;
	}


	// The valid channel is from ADC_VOLTAGE to ADC_AIN in auto mode.
	if(channel >= D2199_ADC_CHANNEL_MAX - 1) {
		pr_err("%s. Invalid channel(%d) in auto mode\n", __func__, channel);
		return -EINVAL;
	}

	mutex_lock(&pbat->meoc_lock);

	pbat_data->adc_res[channel].is_adc_eoc = FALSE;
	pbat_data->adc_res[channel].read_adc = 0;

	// Set ADC_CONT register to select a channel.
	if(adc_cont_inven[channel].adc_preset_val) {
		ret = d2199_reg_write(d2199, D2199_ADC_CONT_REG, adc_cont_inven[channel].adc_preset_val);
		msleep(1);
		ret |= d2199_reg_write(d2199, D2199_ADC_CONT_REG, adc_cont_inven[channel].adc_cont_val);
		if(ret < 0)
			goto out;
	} else {
		ret = d2199_reg_write(d2199, D2199_ADC_CONT_REG, adc_cont_inven[channel].adc_cont_val);
		if(ret < 0)
			goto out;
	}
	msleep(2);

	// Read result register for requested adc channel
	ret = d2199_reg_read(d2199, adc_cont_inven[channel].adc_msb_res, &msb_res);
	ret |= d2199_reg_read(d2199, adc_cont_inven[channel].adc_lsb_res, &lsb_res);
	lsb_res &= adc_cont_inven[channel].adc_lsb_mask;
	if((ret = d2199_reg_write(d2199, D2199_ADC_CONT_REG, 0x00)) < 0)
		goto out;

	// Make ADC result
	pbat_data->adc_res[channel].is_adc_eoc = TRUE;
	pbat_data->adc_res[channel].read_adc =
		((msb_res << 4) | (lsb_res >> 
			(adc_cont_inven[channel].adc_lsb_mask == ADC_RES_MASK_MSB ? 4 : 0)));

out:
	mutex_unlock(&pbat->meoc_lock);
	
	return ret;
}


/* 
 * Name : d2199_read_adc_in_manual
 */
static int d2199_read_adc_in_manual(struct d2199_battery *pbat, adc_channel channel)
{
	u8 mux_sel, flag = FALSE;
	int ret, retries = D2199_MANUAL_READ_RETRIES;
	struct d2199_battery_data *pbat_data = &pbat->battery_data;
	struct d2199 *d2199 = pbat->pd2199;

	mutex_lock(&pbat->meoc_lock);

	pbat_data->adc_res[channel].is_adc_eoc = FALSE;
	pbat_data->adc_res[channel].read_adc = 0;

	switch(channel) {
		case D2199_ADC_VOLTAGE:
			mux_sel = D2199_MUXSEL_VBAT;
			break;
		case D2199_ADC_TEMPERATURE_1:
			mux_sel = D2199_MUXSEL_TEMP1;
			break;
		case D2199_ADC_TEMPERATURE_2:
			mux_sel = D2199_MUXSEL_TEMP2;
			break;
		case D2199_ADC_VF:
			mux_sel = D2199_MUXSEL_VF;
			break;
		case D2199_ADC_TJUNC:
			mux_sel = D2199_MUXSEL_TJUNC;
			break;
		default :
			pr_err("%s. Invalid channel(%d) \n", __func__, channel);
			ret = -EINVAL;
			goto out;
	}

	mux_sel |= D2199_MAN_CONV_MASK;
	if((ret = d2199_reg_write(d2199, D2199_ADC_MAN_REG, mux_sel)) < 0)
		goto out;

	do {
		schedule_timeout_interruptible(msecs_to_jiffies(1));
		flag = pbat_data->adc_res[channel].is_adc_eoc;
	} while(retries-- && (flag == FALSE));

	if(flag == FALSE) {
		pr_warn("%s. Failed manual ADC conversion. channel(%d)\n", __func__, channel);
		ret = -EIO;
	}

out:
	mutex_unlock(&pbat->meoc_lock);
	
	return ret;    
}


/* 
 * Name : d2199_check_offset_limits
 */
/*
static void d2199_check_offset_limits(int *A, int *B)
{
	if(*A > D2199_CAL_MAX_OFFSET)
		*A = D2199_CAL_MAX_OFFSET;
	else if(*A < -D2199_CAL_MAX_OFFSET)
		*A = -D2199_CAL_MAX_OFFSET;

	if(*B > D2199_CAL_MAX_OFFSET)
		*B = D2199_CAL_MAX_OFFSET;
	else if(*B < -D2199_CAL_MAX_OFFSET)
		*B = -D2199_CAL_MAX_OFFSET;

	return;
}
*/

/* 
 * Name : d2199_get_calibration_offset
 */
static int d2199_get_calibration_offset(int voltage, int y1, int y0)
{
	int x1 = D2199_CAL_HIGH_VOLT, x0 = D2199_CAL_LOW_VOLT;
	int x = voltage, y = 0;

	y = y0 + ((x-x0)*y1 - (x-x0)*y0) / (x1-x0);

	return y;
}


#if defined(CONFIG_BACKLIGHT_KTD253) || \
	defined(CONFIG_BACKLIGHT_KTD3102)
extern u8 ktd_backlight_is_dimming(void);
#else
#define LCD_DIM_ADC			1895
#endif

/* 
 * Name : d2153_reset_sw_fuelgauge
 */
static int d2199_reset_sw_fuelgauge(void)
{
	u8 i, j = 0;
	int read_adc = 0;
	u16 drop_offset[] = {172, 98};
	u32 average_adc, sum_read_adc = 0;
	struct d2199_battery *pbat = gbat;
	struct d2199_battery_data *pbatt_data = &pbat->battery_data;

	if(unlikely(!pbat || !pbatt_data)) {
		pr_err("%s. Invalid argument\n", __func__);
		return -EINVAL;
	}
	
	pr_info("++++++ Reset Software Fuelgauge +++++++++++\n");
	pbatt_data->volt_adc_init_done = FALSE;

	/* Initialize ADC buffer */
	memset(pbatt_data->voltage_adc, 0x0, ARRAY_SIZE(pbatt_data->voltage_adc));
	pbatt_data->sum_voltage_adc = 0;
	pbatt_data->soc = 0;
	pbatt_data->prev_soc = 0;

	/* Read VBAT_S ADC */
	for(i = 8, j = 0; i; i--) {
		read_adc = pbat->d2199_read_adc(pbat, D2199_ADC_VOLTAGE);
		if(pbatt_data->adc_res[D2199_ADC_VOLTAGE].is_adc_eoc) {
			read_adc = pbatt_data->adc_res[D2199_ADC_VOLTAGE].read_adc;
			//pr_info("%s. Read ADC %d : %d\n", __func__, i, read_adc);
			if(read_adc > 0) {
				sum_read_adc += read_adc;
				j++;
			}
		}
		msleep(10);
	}
	average_adc = read_adc = sum_read_adc / j;
	pr_info("%s. average = %d, j = %d \n", __func__, average_adc, j);

	/* To be compensated a read ADC */
#if defined(CONFIG_BACKLIGHT_KTD253) || \
	defined(CONFIG_BACKLIGHT_KTD3102)
	if(ktd_backlight_is_dimming())
		average_adc += drop_offset[1];
	else
		average_adc += drop_offset[0];
#else
	if(average_adc >= LCD_DIM_ADC )
		average_adc += drop_offset[0];
	else
		average_adc += drop_offset[1];
#endif /* CONFIG_BACKLIGHT_KTD253 ||  CONFIG_BACKLIGHT_KTD3102 */

	pr_info("%s. average = %d\n", __func__, average_adc);
	/* Reset the buffer from using a read ADC */
	for(i = AVG_SIZE; i ; i--) {
		pbatt_data->voltage_adc[i-1] = average_adc;
		pbatt_data->sum_voltage_adc += average_adc;
	}
	
	pbatt_data->current_volt_adc = average_adc;

	pbatt_data->origin_volt_adc = read_adc;
	pbatt_data->average_volt_adc = pbatt_data->sum_voltage_adc >> AVG_SHIFT;
	pbatt_data->voltage_idx = (pbatt_data->voltage_idx+1) % AVG_SIZE;
	pbatt_data->current_voltage = adc_to_vbat(pbatt_data->current_volt_adc,
										 pbatt_data->is_charging);
	pbatt_data->average_voltage = adc_to_vbat(pbatt_data->average_volt_adc,
										 pbatt_data->is_charging);
	pbat->battery_data.volt_adc_init_done = TRUE;

	pr_info("%s. Average. ADC = %d, Voltage =  %d\n", 
			__func__, pbatt_data->average_volt_adc, pbatt_data->average_voltage);

	return 0;
}


/* 
 * Name : d2199_read_voltage
 */

static int d2199_read_voltage(struct d2199_battery *pbat,struct power_supply *ps)
{
	int new_vol_adc = 0, base_weight, new_vol_orign;
	int offset_with_old, offset_with_new = 0;
	int ret = 0;
	static int calOffset_4P2, calOffset_3P4 = 0;
	int num_multi=0;
	struct d2199_battery_data *pbat_data = &pbat->battery_data;
	u16 offset_charging = 0;
	u8  ta_status = 0;
	//int charging_index;

	if(pbat_data->volt_adc_init_done == FALSE ) {
		ret = d2199_reg_read(pbat->pd2199, D2199_STATUS_C_REG, &ta_status);
		pr_info("%s. STATUS_C register = 0x%X\n", __func__, ta_status);
		if(ta_status & D2199_GPI_3_TA_MASK)
			pbat_data->is_charging = 1;
	}
	pr_info("##### is_charging mode = %d\n", pbat_data->is_charging);

	// Read voltage ADC
	ret = pbat->d2199_read_adc(pbat, D2199_ADC_VOLTAGE);

	if(ret < 0)
		return ret;
	// Getting calibration result.
#if 0
	if(ACT_4P2V_ADC == 0 && SYSPARM_GetIsInitialized()) {
		ACT_4P2V_ADC = SYSPARM_GetActual4p2VoltReading();
		ACT_3P4V_ADC = SYSPARM_GetActualLowVoltReading();

		if(ACT_4P2V_ADC && ACT_3P4V_ADC) {
			calOffset_4P2 = D2199_BASE_4P2V_ADC - ACT_4P2V_ADC;
			calOffset_3P4 = D2199_BASE_3P4V_ADC - ACT_3P4V_ADC;

			d2199_check_offset_limits(&calOffset_4P2, &calOffset_3P4);
		}
	}

	if(pbat_data->is_charging)
	{			
		offset_charging = initialize_charge_offset[charging_index];

		adc2soc_lut.adc_ht[ADC2SOC_LUT_SIZE-1] = ADC_VAL_100_PERCENT+offset_charging;
		adc2soc_lut.adc_rt[ADC2SOC_LUT_SIZE-1] = ADC_VAL_100_PERCENT+offset_charging;
		adc2soc_lut.adc_rlt[ADC2SOC_LUT_SIZE-1] = 3390+offset_charging;
		adc2soc_lut.adc_lt[ADC2SOC_LUT_SIZE-1] = 3320+offset_charging;
		adc2soc_lut.adc_lmt[ADC2SOC_LUT_SIZE-1] = 3260+offset_charging;
		adc2soc_lut.adc_llt[ADC2SOC_LUT_SIZE-1] = 3160+offset_charging;

	} else {
		adc2soc_lut.adc_ht[ADC2SOC_LUT_SIZE-1] = ADC_VAL_100_PERCENT;
		adc2soc_lut.adc_rt[ADC2SOC_LUT_SIZE-1] = ADC_VAL_100_PERCENT;
		adc2soc_lut.adc_rlt[ADC2SOC_LUT_SIZE-1] = 3390;
		adc2soc_lut.adc_lt[ADC2SOC_LUT_SIZE-1] = 3320;
		adc2soc_lut.adc_lmt[ADC2SOC_LUT_SIZE-1] = 3260;
		adc2soc_lut.adc_llt[ADC2SOC_LUT_SIZE-1] = 3160;
	}
#endif

	
	if(pbat_data->adc_res[D2199_ADC_VOLTAGE].is_adc_eoc) {
		int offset = 0;

		new_vol_orign = new_vol_adc = pbat_data->adc_res[D2199_ADC_VOLTAGE].read_adc;

		// To be made a new VBAT_S ADC by interpolation with calibration result.
		if(ACT_4P2V_ADC != 0 && ACT_3P4V_ADC != 0) {
			if(calOffset_4P2 && calOffset_3P4) {
				offset = d2199_get_calibration_offset(pbat_data->average_voltage, 
														calOffset_4P2, 
														calOffset_3P4);
			}
			pr_info("%s. new_vol_adc = %d, offset = %d new_vol_adc + offset = %d \n", 
							__func__, new_vol_adc, offset, (new_vol_adc + offset));
			new_vol_adc = new_vol_adc + offset;
		}
		
		if(pbat->battery_data.volt_adc_init_done) {

			//battery_status = d2199_get_battery_status(pbat);

			base_weight = d2199_get_weight_from_lookup(
											C2K(pbat_data->average_temperature),
											pbat_data->average_volt_adc,
											pbat_data->is_charging);

			if(pbat_data->is_charging) {

				offset_with_new = new_vol_adc - pbat_data->average_volt_adc; 
				// Case of Charging
				// The battery may be discharged, even if a charger is attached.

				if(pbat_data->average_volt_adc > CV_START_ADC)
					base_weight = base_weight + ((pbat_data->average_volt_adc 
							- CV_START_ADC)*(LAST_CHARGING_WEIGHT-base_weight))
							/ ((ADC_VAL_100_PERCENT+offset) - CV_START_ADC);
				if(pbat_data->virtual_battery_full == 1)
					base_weight = MAX_WEIGHT;
				
				if(offset_with_new > 0) {
					pbat_data->sum_total_adc += (offset_with_new * base_weight);

					num_multi = pbat_data->sum_total_adc / NORM_NUM;
					if(num_multi > 0) {						
						new_vol_adc = pbat_data->average_volt_adc + num_multi;
						pbat_data->sum_total_adc = pbat_data->sum_total_adc - (num_multi*NORM_NUM);
					}
					else
						new_vol_adc = pbat_data->average_volt_adc;
				}
				else
					new_vol_adc = pbat_data->average_volt_adc;

				pbat_data->current_volt_adc = new_vol_adc;
				pbat_data->sum_voltage_adc += new_vol_adc;
				pbat_data->sum_voltage_adc -= pbat_data->average_volt_adc;
				pbat_data->voltage_adc[pbat_data->voltage_idx] = new_vol_adc;
			}
			else {
				pbat_data->virtual_battery_full = 0;

				// Case of Discharging.
				offset_with_new = pbat_data->average_volt_adc - new_vol_adc;
				offset_with_old = pbat_data->voltage_adc[pbat_data->voltage_idx]
								- pbat_data->average_volt_adc;
			
				if(is_called_by_ticker ==1)	
				{
					new_vol_adc = new_vol_adc + DISCHARGE_SLEEP_OFFSET;
					pr_info("##### is_called_by_ticker = %d, base_weight = %d\n",
								is_called_by_ticker, base_weight);
					if(offset_with_new > 100) {
						base_weight = (base_weight * 28 / 10);
					} else {
						base_weight = (base_weight * 26 / 10);
					}
					pr_info("##### base_weight = %d\n", base_weight);
				}


				if(offset_with_new > 0) {
					u8 which_condition = 0;
					int x1 = 0, x0 = 0, y1 = 0, y0 = 0, y = 0;

					pr_info("Discharging. base_weight = %d, new_vol_adc = %d\n", base_weight, new_vol_adc);

					// Battery was discharged by some reason. 
					// So, ADC will be calculated again
					if(offset_with_new >= MAX_DIS_OFFSET_FOR_WEIGHT2) {
						base_weight = base_weight 
							+ (base_weight*MAX_ADD_DIS_PERCENT_FOR_WEIGHT2)/100;
						which_condition = 0;
					} else if(offset_with_new >= MAX_DIS_OFFSET_FOR_WEIGHT1) {
						x1 = MAX_DIS_OFFSET_FOR_WEIGHT2;
						x0 = MAX_DIS_OFFSET_FOR_WEIGHT1;
						y1 = MAX_ADD_DIS_PERCENT_FOR_WEIGHT2;
						y0 = MAX_ADD_DIS_PERCENT_FOR_WEIGHT1;
						which_condition = 1;
					} else if(offset_with_new >= MAX_DIS_OFFSET_FOR_WEIGHT0_5) {
						x1 = MAX_DIS_OFFSET_FOR_WEIGHT1;
						x0 = MAX_DIS_OFFSET_FOR_WEIGHT0_5;
						y1 = MAX_ADD_DIS_PERCENT_FOR_WEIGHT1;
						y0 = MAX_ADD_DIS_PERCENT_FOR_WEIGHT0_5;
						which_condition = 2;					
					} else if(offset_with_new >= MAX_DIS_OFFSET_FOR_WEIGHT) {
						x1 = MAX_DIS_OFFSET_FOR_WEIGHT0_5;
						x0 = MAX_DIS_OFFSET_FOR_WEIGHT;
						y1 = MAX_ADD_DIS_PERCENT_FOR_WEIGHT0_5;
						y0 = MAX_ADD_DIS_PERCENT_FOR_WEIGHT;
						which_condition = 3;
					} else if(offset_with_new < MIN_DIS_OFFSET_FOR_WEIGHT) {
						base_weight = base_weight 
							+ (base_weight*MIN_ADD_DIS_PERCENT_FOR_WEIGHT)/100;
						which_condition = 4;
					} else {
						base_weight = base_weight + (base_weight 
							* ( MAX_ADD_DIS_PERCENT_FOR_WEIGHT 
							- (((MAX_DIS_OFFSET_FOR_WEIGHT - offset_with_new)
							* (MAX_ADD_DIS_PERCENT_FOR_WEIGHT
								- MIN_ADD_DIS_PERCENT_FOR_WEIGHT))
							/ (MAX_DIS_OFFSET_FOR_WEIGHT
							    - MIN_DIS_OFFSET_FOR_WEIGHT))))/100;
						which_condition = 5;
					}

					pr_info("%s. Discharging condition : %d\n", __func__, which_condition);
					if(which_condition >= 1 && which_condition <= 3) {
						y = do_interpolation(x0, x1, y0, y1, offset_with_new);
						base_weight = base_weight + (base_weight * y) / 100;
					}
					pbat_data->sum_total_adc -= (offset_with_new * base_weight);

					pr_info("Discharging. Recalculated base_weight = %d\n",
								base_weight);

					num_multi = pbat_data->sum_total_adc / NORM_NUM;
					if(num_multi < 0) {
						new_vol_adc = pbat_data->average_volt_adc + num_multi;
						pbat_data->sum_total_adc = pbat_data->sum_total_adc 
													- (num_multi * NORM_NUM);
					} else {
						new_vol_adc = pbat_data->average_volt_adc;
					}
				} else {
					new_vol_adc = pbat_data->average_volt_adc;
				}

				if(is_called_by_ticker == 0) {
					pbat_data->current_volt_adc = new_vol_adc;
					pbat_data->sum_voltage_adc += new_vol_adc;
					pbat_data->sum_voltage_adc -=
								pbat_data->voltage_adc[pbat_data->voltage_idx];
					pbat_data->voltage_adc[pbat_data->voltage_idx] = new_vol_adc;
				} else {
					int i;
					
					for(i = AVG_SIZE; i ; i--) {				
						pbat_data->current_volt_adc = new_vol_adc;
						pbat_data->sum_voltage_adc += new_vol_adc;
						pbat_data->sum_voltage_adc -= 
										pbat_data->voltage_adc[pbat_data->voltage_idx];
						pbat_data->voltage_adc[pbat_data->voltage_idx] = new_vol_adc;	
						pbat_data->voltage_idx = (pbat_data->voltage_idx+1) % AVG_SIZE;
					}

					is_called_by_ticker=0;
				}
			}
		}
		else {
			u8 i = 0;
			u8 res_msb, res_lsb, is_convert = 0;
			u32 capacity = 0, convert_vbat_adc = 0;
			int X1, X0;
			int Y1, Y0 = FIRST_VOLTAGE_DROP_ADC;
			int X = C2K(pbat_data->average_temperature);

			d2199_reg_read(pbat->pd2199, D2199_GP_ID_2_REG, &res_lsb);
			d2199_reg_read(pbat->pd2199, D2199_GP_ID_3_REG, &res_msb);
			capacity = (((res_msb & 0x0F) << 8) | (res_lsb & 0xFF));

			if(capacity) {
				u32 vbat_adc = 0;
				if(capacity == FULL_CAPACITY) {
					d2199_reg_read(pbat->pd2199, D2199_GP_ID_4_REG, &res_lsb);
					d2199_reg_read(pbat->pd2199, D2199_GP_ID_5_REG, &res_msb);
					vbat_adc = (((res_msb & 0x0F) << 8) | (res_lsb & 0xFF));
					pr_info("[L%d] %s. Read vbat_adc is %d\n", __LINE__, __func__, vbat_adc);
				}
				// If VBAT_ADC is zero then, getting new_vol_adc from capacity(SOC)
				if(vbat_adc >= ADC_VAL_100_PERCENT) {
					convert_vbat_adc = vbat_adc;
				} else {
					convert_vbat_adc = soc_to_adc_with_temp_compensat(capacity, 
											C2K(pbat_data->average_temperature));
					is_convert = TRUE;
				}
				pr_info("[L%4d]%s. read SOC = %d, convert_vbat_adc = %d, is_convert = %d\n", 
						__LINE__, __func__, capacity, convert_vbat_adc, is_convert);
			}

			pbat->pd2199->average_vbat_init_adc = 
									(pbat->pd2199->vbat_init_adc[0] +
										pbat->pd2199->vbat_init_adc[1] +
										pbat->pd2199->vbat_init_adc[2]) / 3;

			if(pbat_data->is_charging) {
				pr_info("%s. Charging. vbat_init_adc = %d\n", __func__, 
								pbat->pd2199->average_vbat_init_adc);
				offset = initialize_charge_up_cc[0];
				if(VBAT_3_4_VOLTAGE_ADC > pbat->pd2199->average_vbat_init_adc) {
					new_vol_adc = pbat->pd2199->average_vbat_init_adc;
				} else {
					new_vol_adc = pbat->pd2199->average_vbat_init_adc 
											+ FIRST_VOLTAGE_DROP_ADC;
				}
			} else {
				new_vol_adc = pbat->pd2199->average_vbat_init_adc;
				pr_info("[L%d] %s discharging new_vol_adc = %d\n",
					__LINE__, __func__, new_vol_adc);

				Y0 = FIRST_VOLTAGE_DROP_ADC;
				if(C2K(pbat_data->average_temperature) <= BAT_LOW_LOW_TEMPERATURE) {
					//pr_info("### BAT_LOW_LOW_TEMPERATURE new ADC is %4d \n", new_vol_adc);
					new_vol_adc += (Y0 + 340);
				} else if(C2K(pbat_data->average_temperature) >= BAT_ROOM_TEMPERATURE) {
					new_vol_adc += Y0;
					//pr_info("### BAT_ROOM_TEMPERATURE new ADC is %4d \n", new_vol_adc);
				} else {
					if(C2K(pbat_data->average_temperature) <= BAT_LOW_MID_TEMPERATURE) {
						Y1 = Y0 + 115;	Y0 = Y0 + 340;
						X0 = BAT_LOW_LOW_TEMPERATURE;
						X1 = BAT_LOW_MID_TEMPERATURE;
					} else if(C2K(pbat_data->average_temperature) <= BAT_LOW_TEMPERATURE) {
						Y1 = Y0 + 60;	Y0 = Y0 + 115;
						X0 = BAT_LOW_MID_TEMPERATURE;
						X1 = BAT_LOW_TEMPERATURE;
					} else {
						Y1 = Y0 + 25;	Y0 = Y0 + 60;
						X0 = BAT_LOW_TEMPERATURE;
						X1 = BAT_ROOM_LOW_TEMPERATURE;
					}
					new_vol_adc = new_vol_adc + Y0 
									+ ((X - X0) * (Y1 - Y0)) / (X1 - X0);
				}
			}
			pr_info("[L%d] %s Calculated new_vol_adc is %4d \n", __LINE__, __func__, new_vol_adc);
			if(((is_convert == TRUE) && (capacity < FULL_CAPACITY)
						&& (convert_vbat_adc >= 614))
				|| ((is_convert == FALSE) && (capacity == FULL_CAPACITY)
						&& (convert_vbat_adc >= ADC_VAL_100_PERCENT))) {
				new_vol_adc = convert_vbat_adc;
				pr_info("[L%d] %s. convert_vbat_adc is assigned to new_vol_adc\n", __LINE__, __func__);
			}

			if(new_vol_adc > MAX_FULL_CHARGED_ADC) {
				new_vol_adc = MAX_FULL_CHARGED_ADC;
				pr_info("%s. Set new_vol_adc to max. ADC value\n", __func__);
			}
				
			for(i = AVG_SIZE; i ; i--) {
				pbat_data->voltage_adc[i-1] = new_vol_adc;
				pbat_data->sum_voltage_adc += new_vol_adc;
			}
			
			pbat_data->current_volt_adc = new_vol_adc;
			pbat->battery_data.volt_adc_init_done = TRUE;
		}

		pbat_data->origin_volt_adc = new_vol_orign;
		pbat_data->average_volt_adc = pbat_data->sum_voltage_adc >> AVG_SHIFT;
		pbat_data->voltage_idx = (pbat_data->voltage_idx+1) % AVG_SIZE;
		pbat_data->current_voltage = adc_to_vbat(pbat_data->current_volt_adc,
											 pbat_data->is_charging);
		pbat_data->average_voltage = adc_to_vbat(pbat_data->average_volt_adc,
											 pbat_data->is_charging);
	}
	else {
		pr_err("%s. Voltage ADC read failure \n", __func__);
		ret = -EIO;
	}

	return ret;
}


/*
 * Name : d2199_read_temperature
 */
static int d2199_read_temperature(struct d2199_battery *pbat)
{
	u16 new_temp_adc = 0;
	int ret = 0;
	struct d2199_battery_data *pbat_data = &pbat->battery_data;

	/* Read temperature ADC
	 * Channel : D2199_ADC_TEMPERATURE_1 -> TEMP_BOARD
	 * Channel : D2199_ADC_TEMPERATURE_2 -> TEMP_RF
	 */

	// To read a temperature ADC of BOARD
	ret = pbat->d2199_read_adc(pbat, D2199_ADC_TEMPERATURE_1);
	if(pbat_data->adc_res[D2199_ADC_TEMPERATURE_1].is_adc_eoc) {
		new_temp_adc = pbat_data->adc_res[D2199_ADC_TEMPERATURE_1].read_adc;

		pbat_data->current_temp_adc = new_temp_adc;

		if(pbat_data->temp_adc_init_done) {
			pbat_data->sum_temperature_adc += new_temp_adc;
			pbat_data->sum_temperature_adc -= 
						pbat_data->temperature_adc[pbat_data->temperature_idx];
			pbat_data->temperature_adc[pbat_data->temperature_idx] = new_temp_adc;
		} else {
			u8 i;

			for(i = 0; i < AVG_SIZE; i++) {
				pbat_data->temperature_adc[i] = new_temp_adc;
				pbat_data->sum_temperature_adc += new_temp_adc;
			}
			pbat_data->temp_adc_init_done = TRUE;
		}

		pbat_data->average_temp_adc =
								pbat_data->sum_temperature_adc >> AVG_SHIFT;
		pbat_data->temperature_idx = (pbat_data->temperature_idx+1) % AVG_SIZE;
		pbat_data->average_temperature = 
					degree_k2c(adc_to_degree_k(pbat_data->average_temp_adc));
		pbat_data->current_temperature = 
									degree_k2c(adc_to_degree_k(new_temp_adc)); 

	}
	else {
		pr_err("%s. Temperature ADC read failed \n", __func__);
		ret = -EIO;
	}

	return ret;
}


/* 
 * Name : d2199_read_rf_temperature_adc
 */
int d2199_read_rf_temperature_adc(int *tbat)
{
	u8 i, j, channel;
	int sum_temp_adc, ret = 0;
	struct d2199_battery *pbat = gbat;
	struct d2199_battery_data *pbat_data = &gbat->battery_data;

	if(pbat == NULL || pbat_data == NULL) {
		pr_err("%s. battery_data is NULL\n", __func__);
		return -EINVAL;
	}

	/* To read a temperature2 ADC */
	sum_temp_adc = 0;
	channel = D2199_ADC_TEMPERATURE_2;
	for(i = 5, j = 0; i; i--) {
		ret = pbat->d2199_read_adc(pbat, channel);
		if(ret == 0) {
			sum_temp_adc += pbat_data->adc_res[channel].read_adc;
			if(++j == 2)
				break;
		} else
			msleep(20);
	}
	if (j) {
		*tbat = (sum_temp_adc / j);
		pr_info("%s. RF_TEMP_ADC = %d\n", __func__, *tbat);
		return 0;
	} else {
		pr_err("%s. Error in reading RF temperature.\n", __func__);
		return -EIO;
	}
}
EXPORT_SYMBOL(d2199_read_rf_temperature_adc);


/******************************************************************************
    Interrupt Handler
******************************************************************************/
/* 
 * Name : d2199_battery_adceom_handler
 */
/*
static irqreturn_t d2199_battery_adceom_handler(int irq, void *data)
{
	u8 read_msb, read_lsb, channel;
	int ret = 0;
	struct d2199_battery *pbat = (struct d2199_battery *)data;
	struct d2199 *d2199 = NULL;

	if(unlikely(!pbat)) {
		pr_err("%s. Invalid driver data\n", __func__);
		return -EINVAL;
	}

	d2199 = pbat->pd2199;
	
	ret = d2199_reg_read(d2199, D2199_ADC_RES_H_REG, &read_msb);
	ret |= d2199_reg_read(d2199, D2199_ADC_RES_L_REG, &read_lsb);
	ret |= d2199_reg_read(d2199, D2199_ADC_MAN_REG, &channel);
	
	channel = (channel & 0xF);
	
	switch(channel) {
		case D2199_MUXSEL_VBAT:
			channel = D2199_ADC_VOLTAGE;
			break;
		case D2199_MUXSEL_TEMP1:
			channel = D2199_ADC_TEMPERATURE_1;
			break;
		case D2199_MUXSEL_TEMP2:
			channel = D2199_ADC_TEMPERATURE_2;
			break;
		case D2199_MUXSEL_VF:
			channel = D2199_ADC_VF;
			break;
		case D2199_MUXSEL_TJUNC:
			channel = D2199_ADC_TJUNC;
			break;
		default :
			pr_err("%s. Invalid channel(%d) \n", __func__, channel);
			goto out;
	}

	pbat->battery_data.adc_res[channel].is_adc_eoc = TRUE;
	pbat->battery_data.adc_res[channel].read_adc = 
						((read_msb << 4) | (read_lsb & ADC_RES_MASK_LSB));

out:
	//pr_info("%s. Manual ADC (%d) = %d\n", 
	//			__func__, channel,
	//			pbat->battery_data.adc_res[channel].read_adc);

	return IRQ_HANDLED;
}
*/

/* 
 * Name : d2199_sleep_monitor
 */
/*
static void d2199_sleep_monitor(struct d2199_battery *pbat)
{
	schedule_delayed_work(&pbat->sleep_monitor_work, 0);

	return;
}
*/


/* 
 * Name : d2199_battery_read_status
 */
int d2199_battery_read_status(int type)
{
	int ret, val = 0;
	struct d2199_battery *pbat = NULL;
	struct power_supply *ps = NULL;

	if (gbat == NULL) {
		dlg_err("%s. driver data is NULL\n", __func__);
		return -EINVAL;
	}

	pbat = gbat;

	switch(type) {
		case D2199_BATTERY_SOC:
			val = d2199_get_soc(pbat);
			val = (val)/10;
			break;

		case D2199_BATTERY_CUR_VOLTAGE:
			ps = power_supply_get_by_name("battery");
			if(ps == NULL){
				pr_info("spa is not registered yet !!!");
				return -EINVAL;
			}
			ret = d2199_read_voltage(pbat, ps);
			if(ret < 0) {
				pr_err("%s. Read voltage ADC failure\n", __func__);
				return ret;
			}
			val = pbat->battery_data.current_voltage;
			break;

#ifndef CONFIG_SPA
		case D2199_BATTERY_TEMP_ADC:
			val = pbat->battery_data.average_temp_adc;
			break;

		case D2199_BATTERY_SLEEP_MONITOR:
			is_called_by_ticker = 1;
			wake_lock_timeout(&pbat->battery_data.sleep_monitor_wakeup,
									D2199_SLEEP_MONITOR_WAKELOCK_TIME);
			cancel_delayed_work_sync(&pbat->monitor_temp_work);
			cancel_delayed_work_sync(&pbat->monitor_volt_work);
			schedule_delayed_work(&pbat->monitor_temp_work, 0);
			schedule_delayed_work(&pbat->monitor_volt_work, 0);
			break;
#endif
	}
	
	return val;
}
EXPORT_SYMBOL(d2199_battery_read_status);


/* 
 * Name : d2199_battery_write_status
 */
int d2199_battery_write_status(int type, int val)
{
	int ret = 0;
	struct d2199_battery *pbat = NULL;
	struct power_supply *ps = NULL;

	if (gbat == NULL) {
		dlg_err("%s. driver data is NULL\n", __func__);
		return -EINVAL;
	}

	pbat = gbat;

	switch(type) {
		case D2199_BATTERY_CHG_CURRENT:
			if(val < 0 ) {
				pr_err("%. Invalid charge current. Please check\n", __func__);
				return -EINVAL;
			}

			pbat->battery_data.charging_current = val;
			if(val)
				pbat->battery_data.is_charging = 1;
			else 
				pbat->battery_data.is_charging = 0;
		
			pr_info("%s. Updated the charging status = %d\n", 
								__func__, pbat->battery_data.is_charging);
			break;
	}
	
	return ret;
}
EXPORT_SYMBOL(d2199_battery_write_status);


static void d2199_monitor_voltage_work(struct work_struct *work)
{
	int ret=0; 
	struct d2199_battery *pbat = container_of(work, struct d2199_battery, monitor_volt_work.work);
	struct d2199_battery_data *pbat_data = &pbat->battery_data;
	struct power_supply *ps;
	
	if(unlikely(!pbat || !pbat_data)) {
		pr_err("%s. Invalid driver data\n", __func__);
		goto err_adc_read;
	}

	ps = power_supply_get_by_name("battery");

	if(ps == NULL) {
		pr_info("spa is not registered yet !!!");
		schedule_delayed_work(&pbat->monitor_volt_work, D2199_VOLTAGE_MONITOR_START);
		return;
	}
	
	ret = d2199_read_voltage(pbat,ps);
	if(ret < 0) {
		pr_err("%s. Read voltage ADC failure\n", __func__);
		goto err_adc_read;
	}

	if(pbat_data->is_charging ==0) {
		schedule_delayed_work(&pbat->monitor_volt_work, D2199_VOLTAGE_MONITOR_NORMAL);
	} else {
		schedule_delayed_work(&pbat->monitor_volt_work, D2199_VOLTAGE_MONITOR_FAST);
	}

	pr_info("# SOC = %3d.%d %%, ADC(read) = %4d, ADC(avg) = %4d, Voltage(avg) = %4d mV, ADC(VF) = %4d\n",
				(pbat->battery_data.soc/10),
				(pbat->battery_data.soc%10),
				pbat->battery_data.origin_volt_adc,
				pbat->battery_data.average_volt_adc,
				pbat->battery_data.average_voltage, 
				pbat->battery_data.vf_adc);

	return;

err_adc_read:
	schedule_delayed_work(&pbat->monitor_volt_work, D2199_VOLTAGE_MONITOR_START);
	return;
}


static void d2199_monitor_temperature_work(struct work_struct *work)
{
	struct d2199_battery *pbat = container_of(work, struct d2199_battery, monitor_temp_work.work);
	int ret = 0;

	ret = d2199_read_temperature(pbat);
	if(ret < 0) {
		pr_err("%s. Failed to read_temperature\n", __func__);
		schedule_delayed_work(&pbat->monitor_temp_work, D2199_TEMPERATURE_MONITOR_FAST);
		return;
	}

	if(pbat->battery_data.temp_adc_init_done) {
		schedule_delayed_work(&pbat->monitor_temp_work, D2199_TEMPERATURE_MONITOR_NORMAL);
	}
	else {
		schedule_delayed_work(&pbat->monitor_temp_work, D2199_TEMPERATURE_MONITOR_FAST);
	}

	pr_info("# TEMP_BOARD(ADC) = %4d, Board Temperauter(Celsius) = %3d.%d\n",
				pbat->battery_data.average_temp_adc,
				(pbat->battery_data.average_temperature/10),
				(pbat->battery_data.average_temperature%10));

	return ;
}


#if 0
/* 
 * Name : d2199_info_notify_work
 */
static void d2199_info_notify_work(struct work_struct *work)
{
	struct d2199_battery *pbat = container_of(work, 
												struct d2199_battery, 
												info_notify_work.work);

	power_supply_changed(&pbat->battery);	
	schedule_delayed_work(&pbat->info_notify_work, D2199_NOTIFY_INTERVAL);
}


/* 
 * Name : d2199_charge_timer_work
 */
static void d2199_charge_timer_work(struct work_struct *work)
{
	struct d2199_battery *pbat = container_of(work, struct d2199_battery, charge_timer_work.work);

	pr_info("%s. Start\n", __func__);

	d2199_set_end_of_charge(pbat, BAT_END_OF_CHARGE_BY_TIMER);
	d2199_set_battery_status(pbat, POWER_SUPPLY_STATUS_FULL);
	d2199_stop_charge(pbat, BAT_END_OF_CHARGE_BY_TIMER);

	pbat->recharge_start_timer.expires = jiffies + BAT_RECHARGE_CHECK_TIMER_30SEC;
	add_timer(&pbat->recharge_start_timer);

	return;
}


/* 
 * Name : d2199_recharge_start_timer_work
 */
static void d2199_recharge_start_timer_work(struct work_struct *work)
{
	u8 end_of_charge = 0;	
	struct d2199_battery *pbat = container_of(work, 
												struct d2199_battery, 
												recharge_start_timer_work.work);

	pr_info("%s. Start\n", __func__);

	if(d2199_check_end_of_charge(pbat, BAT_END_OF_CHARGE_BY_TIMER) == 0)
	{
		end_of_charge = d2199_clear_end_of_charge(pbat, BAT_END_OF_CHARGE_BY_TIMER);
		if(end_of_charge == BAT_END_OF_CHARGE_NONE)
		{
			if((d2199_get_battery_status(pbat) == POWER_SUPPLY_STATUS_FULL) 
				&& (d2199_get_average_voltage(pbat) < BAT_CHARGING_RESTART_VOLTAGE))
			{
				pr_info("%s. Restart charge. Voltage is lower than %04d mV\n", 
									__func__, BAT_CHARGING_RESTART_VOLTAGE);
				d2199_start_charge(pbat, BAT_CHARGE_RESTART_TIMER);
			}
			else
			{
				pr_info("%s. set BAT_END_OF_CHARGE_BY_FULL. Voltage is higher than %04d mV\n", 
								__func__, BAT_CHARGING_RESTART_VOLTAGE);
				d2199_set_end_of_charge(pbat, BAT_END_OF_CHARGE_BY_FULL);	
			}
		}
		else {
			pr_info("%s. Can't restart charge. The reason why is %d\n", __func__, end_of_charge);	
		}
	}
	else {
		pr_info("%s. SPA_END_OF_CHARGE_BY_TIMER had been cleared by other reason\n", __func__); 
	}
}


/* 
 * Name : d2199_sleep_monitor_work
 */
static void d2199_sleep_monitor_work(struct work_struct *work)
{
	struct d2199_battery *pbat = container_of(work, struct d2199_battery, 
												sleep_monitor_work.work);

	is_called_by_ticker = 1;
	wake_lock_timeout(&pbat->battery_data.sleep_monitor_wakeup, 
									D2199_SLEEP_MONITOR_WAKELOCK_TIME);
	pr_info("%s. Start. Ticker was set to 1\n", __func__);
	if(schedule_delayed_work(&pbat->monitor_volt_work, 0) == 0) {
		cancel_delayed_work_sync(&pbat->monitor_volt_work);
		schedule_delayed_work(&pbat->monitor_volt_work, 0);
	}
	if(schedule_delayed_work(&pbat->monitor_temp_work, 0) == 0) {
		cancel_delayed_work_sync(&pbat->monitor_temp_work);
		schedule_delayed_work(&pbat->monitor_temp_work, 0);
	}
	if(schedule_delayed_work(&pbat->info_notify_work, 0) == 0) {
		cancel_delayed_work_sync(&pbat->info_notify_work);
		schedule_delayed_work(&pbat->info_notify_work, 0);
	}

	return ;	
}
#endif

void d2199_battery_start(void)
{
	schedule_delayed_work(&gbat->monitor_volt_work, 0);
}
EXPORT_SYMBOL_GPL(d2199_battery_start);

/* 
 * Name : d2199_battery_data_init
 */
static void d2199_battery_data_init(struct d2199_battery *pbat)
{
	struct d2199_battery_data *pbat_data = &pbat->battery_data;

	if(unlikely(!pbat_data)) {
		pr_err("%s. Invalid platform data\n", __func__);
		return;
	}

	pbat->adc_mode = D2199_ADC_MODE_MAX;

	pbat_data->sum_total_adc = 0;
	pbat_data->vdd_hwmon_level = 0;
	pbat_data->volt_adc_init_done = FALSE;
	pbat_data->temp_adc_init_done = FALSE;
	pbat_data->battery_present = TRUE;
	wake_lock_init(&pbat_data->sleep_monitor_wakeup, WAKE_LOCK_SUSPEND, "sleep_monitor");		

	return;
}

int d2199_extern_read_temperature(int *tbat)
{
	int sum = 0, ret = -1;
	struct d2199_battery_data *pbat_data = &gbat->battery_data;

	*tbat = 0;
	ret = gbat->d2199_read_adc(gbat, D2199_ADC_TEMPERATURE_1);

	if (ret < 0)
        {
                pr_err("%s. read adc_temp1 error %d\n", __func__, ret);
		return ret;
        }
#ifdef CONFIG_SPA
	pr_info("d2199_extern_read_temperature  adc %d\n",
				pbat_data->average_temp_adc);
	sum = pbat_data->average_temp_adc;
	*tbat = sum;
#else
	sum = pbat_data->adc_res[D2199_ADC_TEMPERATURE_1].read_adc;
	sum = ((sum & 0xFFF) * 1400) >> 12;
	*tbat = sum;
#endif

	return 0;
}
EXPORT_SYMBOL(d2199_extern_read_temperature);


/* 
 * Name : read_voltage
 */
static int read_voltage(int *vbat)
{

	*vbat = d2199_battery_read_status(D2199_BATTERY_CUR_VOLTAGE);
	pr_info("%s voltage : %d\n", __func__, *vbat);

	return 0;
}

/* 
 * Name : read_soc
 */
static int read_soc(int *soc)
{
	*soc = d2199_battery_read_status(D2199_BATTERY_SOC);	
	pr_info("%s soc : %d\n", __func__, *soc);

	return 0;
}

/* 
 * Name : read_VF
 */
static int read_VF(unsigned int *VF)
{
	u8 is_first = TRUE;
	int j, i, ret = 0;
	unsigned int sum = 0, read_adc;
	struct d2199_battery *pbat = gbat;
	struct d2199_battery_data *pbat_data = &pbat->battery_data;

	if(pbat == NULL || pbat_data == NULL) {
		pr_err("%s. Invalid Parameter \n", __func__);
		return -EINVAL;
	}

	// Read VF ADC
re_measure:
	sum = 0; read_adc = 0;
	for(i = 4, j = 0; i; i--) {
		ret = pbat->d2199_read_adc(pbat, D2199_ADC_VF);

		if(pbat_data->adc_res[D2199_ADC_VF].is_adc_eoc) {
			read_adc = pbat_data->adc_res[D2199_ADC_VF].read_adc;
			sum += read_adc;
			j++;
		}
		else {
			pr_err("%s. VF ADC read failure \n", __func__);
			ret = -EIO;
		}
	}

	if(j) {
		read_adc = (sum / j);
	}

	if(is_first == TRUE && (read_adc == 0xFFF)) {
		msleep(50);
		is_first = FALSE;
		goto re_measure;
	} else {
		*VF = pbat_data->vf_adc = read_adc;
	}
	pr_info("%s. j = %d, ADC(VF) = %d\n", __func__, j, pbat_data->vf_adc);

	return 0;
}

/* 
 * Name : d2199_battery_probe
 */
static __devinit int d2199_battery_probe(struct platform_device *pdev)
{
	struct d2199 *d2199 = platform_get_drvdata(pdev);
	struct d2199_battery *pbat = &d2199->batt;
	int ret;

	pr_info(" Start %s\n", __func__);

	if(unlikely(!d2199 || !pbat)) {
		pr_err("%s. Invalid platform data\n", __func__);
		return -EINVAL;
	}

	gbat = pbat;
	pbat->pd2199 = d2199;

	// Initialize a resource locking
	mutex_init(&pbat->lock);
	mutex_init(&pbat->meoc_lock);

	// Store a driver data structure to platform.
	platform_set_drvdata(pdev, pbat);

	d2199_battery_data_init(pbat);
	d2199_set_adc_mode(pbat, D2199_ADC_IN_AUTO);
	// Disable 50uA current source in Manual ctrl register
	d2199_reg_write(d2199, D2199_ADC_MAN_REG, 0x00);

#ifndef CONFIG_SPA
	INIT_DELAYED_WORK(&pbat->monitor_volt_work, d2199_monitor_voltage_work);
#if 0	
	INIT_DELAYED_WORK(&pbat->info_notify_work, d2199_info_notify_work);
	INIT_DELAYED_WORK(&pbat->charge_timer_work, d2199_charge_timer_work);
	INIT_DELAYED_WORK(&pbat->recharge_start_timer_work, d2199_recharge_start_timer_work);
	INIT_DELAYED_WORK(&pbat->sleep_monitor_work, d2199_sleep_monitor_work);
#endif	

	// Start schedule of dealyed work for temperature.
	schedule_delayed_work(&pbat->monitor_temp_work, 0);
#endif
	device_init_wakeup(&pdev->dev, 1);	

#ifdef CONFIG_SPA
	ret = spa_bat_register_read_temperature(d2199_extern_read_temperature);
	if (ret) {
		pr_err("%s fail to register read_voltage function\n", __func__);
	}
	ret = spa_bat_register_read_voltage(read_voltage);
	if (ret) {
		pr_err("%s fail to register read_voltage function\n", __func__);
	}
	ret = spa_bat_register_read_soc(read_soc);
	if(ret) {
		pr_err("%s fail to register read_soc function\n", __func__);
	}
	ret = spa_bat_register_read_VF(read_VF);
	if(ret) {
		pr_err("%s fail to register read_VF function\n", __func__);
	}
	ret = spa_bat_register_fuelgauge_reset(d2199_reset_sw_fuelgauge);
	if(ret) {
		pr_err("%s fail to register read_VF function\n", __func__);
	}
#endif

	INIT_DELAYED_WORK(&pbat->monitor_temp_work, d2199_monitor_temperature_work);
	schedule_delayed_work(&pbat->monitor_temp_work, 0);

	pr_info(" %s. End...\n", __func__);

	return 0;

err_default:
	kfree(pbat);

	return ret;

}


/*
 * Name : d2199_battery_suspend
 */
static int d2199_battery_suspend(struct platform_device *pdev, pm_message_t state)
{
	struct d2199_battery *pbat = platform_get_drvdata(pdev);
	struct d2199 *d2199 = pbat->pd2199;

	pr_info("%s. Enter\n", __func__);

#ifndef CONFIG_SPA
/*
	if(unlikely(!pbat || !d2199)) {
		pr_err("%s. Invalid parameter\n", __func__);
		return -EINVAL;
	}
*/

	cancel_delayed_work(&pbat->monitor_temp_work);
	cancel_delayed_work(&pbat->monitor_volt_work);
#endif
	pr_info("%s. Leave\n", __func__);
	
	return 0;
}


/*
 * Name : d2199_battery_resume
 */
static int d2199_battery_resume(struct platform_device *pdev)
{
	struct d2199_battery *pbat = platform_get_drvdata(pdev);
	struct d2199 *d2199 = pbat->pd2199;

	pr_info("%s. Enter\n", __func__);

#ifndef CONFIG_SPA
/*
	if(unlikely(!pbat || !d2199)) {
		pr_err("%s. Invalid parameter\n", __func__);
		return -EINVAL;
	}
*/

	// Start schedule of dealyed work for monitoring voltage and temperature.
	if(!is_called_by_ticker) {
//		wake_lock_timeout(&pbat->battery_data.sleep_monitor_wakeup, 
//										D2199_SLEEP_MONITOR_WAKELOCK_TIME);
		schedule_delayed_work(&pbat->monitor_temp_work, 0);
		schedule_delayed_work(&pbat->monitor_volt_work, 0);
	}
#endif
	pr_info("%s. Leave\n", __func__);

	return 0;
}


/*
 * Name : d2199_battery_remove
 */
static __devexit int d2199_battery_remove(struct platform_device *pdev)
{
	struct d2199_battery *pbat = platform_get_drvdata(pdev);
	struct d2199 *d2199 = pbat->pd2199;

	if(unlikely(!d2199)) {
		pr_err("%s. Invalid parameter\n", __func__);
		return -EINVAL;
	}

	// Free IRQ
#ifdef D2199_REG_EOM_IRQ
	d2199_free_irq(d2199, D2199_IRQ_EADCEOM);
#endif /* D2199_REG_EOM_IRQ */
#ifdef D2199_REG_VDD_MON_IRQ
	d2199_free_irq(d2199, D2199_IRQ_EVDD_MON);
#endif /* D2199_REG_VDD_MON_IRQ */
#ifdef D2199_REG_VDD_LOW_IRQ
	d2199_free_irq(d2199, D2199_IRQ_EVDD_LOW);
#endif /* D2199_REG_VDD_LOW_IRQ */
#ifdef D2199_REG_TBAT2_IRQ
	d2199_free_irq(d2199, D2199_IRQ_ETBAT2);
#endif /* D2199_REG_TBAT2_IRQ */

	return 0;
}

static struct platform_driver d2199_battery_driver = {
	.probe    = d2199_battery_probe,
	.suspend  = d2199_battery_suspend,
	.resume   = d2199_battery_resume,
	.remove   = d2199_battery_remove,
	.driver   = {
		.name  = "d2199-battery",
		.owner = THIS_MODULE,
    },
};

static int __init d2199_battery_init(void)
{
	printk(d2199_battery_banner);
	return platform_driver_register(&d2199_battery_driver);
}
//module_init(d2199_battery_init);
//subsys_initcall(d2199_battery_init);
subsys_initcall_sync(d2199_battery_init);


static void __exit d2199_battery_exit(void)
{
	flush_scheduled_work();
	platform_driver_unregister(&d2199_battery_driver);
}
module_exit(d2199_battery_exit);

MODULE_AUTHOR("Dialog Semiconductor Ltd. < eric.jeong@diasemi.com >");
MODULE_DESCRIPTION("Battery driver for the Dialog D2199 PMIC");
MODULE_LICENSE("GPL");
MODULE_ALIAS("Power supply : d2199-battery");
