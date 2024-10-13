/*
 * Fuelgauge battery driver
 *
 * Copyright (C) 2018 Siliconmitus
 *
 * This package is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * THIS PACKAGE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 */

#define pr_fmt(fmt)	"<sm5602> [%s,%d] " fmt, __func__, __LINE__

#include <linux/module.h>
#include <linux/param.h>
#include <linux/jiffies.h>
#include <linux/workqueue.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/power_supply.h>
#include <linux/idr.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/acpi.h>
#include <asm/unaligned.h>
#include <linux/uaccess.h>
#include <linux/interrupt.h>
#include <linux/of_gpio.h>
#include <linux/gpio.h>
#include <linux/gpio/consumer.h>
#include <linux/debugfs.h>
#include <linux/kernel.h>
//#include <linux/power/sm5602_fg.h>
#include <linux/ktime.h>
#include <linux/time.h>
#include <linux/poll.h>

#include "qcom-pmic-voter.h"
#include "sm5602_iio.h"
#include <sm5602_fg.h>
#include <linux/iio/consumer.h>
#include <dt-bindings/iio/qti_power_supply_iio.h>
#if 0
#undef pr_debug
#define pr_debug pr_err
#undef pr_info
#define pr_info pr_err
#undef dev_dbg
#define dev_dbg dev_err
#endif
#define	INVALID_REG_ADDR	0xFF
#define RET_ERR 			-1

//#define SOC_SMOOTH_TRACKING
#define SHUTDOWN_DELAY
//#define FG_ENABLE_IRQ
#define ENABLE_IOCV_ADJ
#define ENABLE_INSPECTION_TABLE
//#define ENABLE_VLCM_MODE
#define ENABLE_TEMBASE_ZDSCON
//#define ENABLE_INIT_DELAY_TEMP
#define ENABLE_TEMP_AVG
//#define IN_PRO_ADJ
#define ENABLE_NTC_COMPENSATION_1
//#define IOCV_M_TYPE1
//#define ENABLE_MAP_SOC
//#define ENABLE_MIX_NTC_BATTDET
//#define ENABLE_WAIT_SOC_FULL
//#define ENABLE_LTIM_ACT
#define ENABLE_HCIC_ACT
//#define ENABLE_HCRSM_MODE
//#define ENABLE_FIT_ZR_NCUR
#define CHECK_ABNORMAL_TABLE
//#define	DELAY_SOC_ZERO
//#define ENABLE_FCHG_VSOC_MODE

#define EX_TEMP_MIN			(-20)
#define EX_TEMP_MAX			80

#ifdef ENABLE_MAP_SOC
#define MAP_MAX_SOC			99
#define MAP_RATE_SOC		995
#define MAP_MIN_SOC			4
#endif

#ifdef ENABLE_INIT_DELAY_TEMP
#define DELAY_TEMP_TIME_5000MS		5000
#endif

#ifdef ENABLE_IOCV_ADJ
#define IOCV_MAX_ADJ_LEVEL 		0x1F33
#define IOCV_MIN_ADJ_LEVEL 		0x1D70
#define IOCI_MAX_ADJ_LEVEL 		0x1000
#define IOCI_MIN_ADJ_LEVEL 		0xCC
#define IOCV_I_SLOPE 	   		100
#define IOCV_I_OFFSET  	   		0

#endif

#ifdef ENABLE_TEMBASE_ZDSCON
//#define ENABLE_TEM_RATE_CONTROL
#define ZDSCON_ACT_TEMP_GAP 	15
#define T_GAP_DENOM 			5
#define HMINMAN_T_VALUE_FACT 	125
#define I_GAP_DENOM 			1000
#define HMINMAN_I_VALUE_FACT 	150

#ifdef ENABLE_LTIM_ACT
#define LTIM_ACT_TEMP_GAP 		24
#define LTIM_I_LIMIT 			1500
#define LTIM_FACTOR 			42
#define LTIM_DENOM 				130
#define LTIM_MIN 				0xA
#endif
#endif

#ifdef ENABLE_HCIC_ACT
#define HCIC_MIN 4000
#define HCIC_FACTOR 2
#define HCIC_DENOM 200
#endif

#ifdef ENABLE_HCRSM_MODE
#ifdef ENABLE_FCHG_VSOC_MODE
#define FCHG_VSOC_VOLT 4460
#define FCHG_VSOC_SOC 700
#endif
#define HCRSM_VOLT 4450
#define HCRSM_CURR 1750
#endif

#ifdef ENABLE_WAIT_SOC_FULL
#define WAIT_SOC_GAP 			1
#endif

#ifdef ENABLE_FIT_ZR_NCUR
#define FIT_ZR_NCUR_VALUE 0x15
#define FIT_ZR_NCUR_SOC	100
#define FIT_ZR_NCUR_VOL	3420
#endif

#ifdef SHUTDOWN_DELAY	
#define SHUTDOWN_DELAY_H_VOL	3400
#define SHUTDOWN_DELAY_L_VOL	3300
#endif

#ifdef DELAY_SOC_ZERO
#define DELAY_SOC_ZERO_TIME_30000MS		30000
#endif

#ifdef CHECK_ABNORMAL_TABLE
#define TABLE_ERROR_OCV_MARGIN	32
#define TABLE_ERROR_SOC_MARGIN	48
#endif

#define FEATURE_BATT_LONG_LIFE_CYCLE

enum sm_fg_reg_idx {
	SM_FG_REG_DEVICE_ID = 0,
	SM_FG_REG_CNTL,
	SM_FG_REG_INT,
	SM_FG_REG_INT_MASK,
	SM_FG_REG_STATUS,
	SM_FG_REG_SOC,
	SM_FG_REG_OCV,
	SM_FG_REG_VOLTAGE,
	SM_FG_REG_CURRENT,
	SM_FG_REG_TEMPERATURE_IN,
	SM_FG_REG_TEMPERATURE_EX,
	SM_FG_REG_V_L_ALARM,
	SM_FG_REG_V_H_ALARM,	
	SM_FG_REG_A_H_ALARM,
	SM_FG_REG_T_IN_H_ALARM,
	SM_FG_REG_SOC_L_ALARM,
	SM_FG_REG_FG_OP_STATUS,
	SM_FG_REG_TOPOFFSOC,
	SM_FG_REG_PARAM_CTRL,
	SM_FG_REG_SHUTDOWN,	
	SM_FG_REG_VIT_PERIOD,
	SM_FG_REG_CURRENT_RATE,
	SM_FG_REG_BAT_CAP,
	SM_FG_REG_CURR_OFFSET,
	SM_FG_REG_CURR_SLOPE,	
	SM_FG_REG_MISC,
	SM_FG_REG_RESET,
	SM_FG_REG_RSNS_SEL,
	SM_FG_REG_VOL_COMP,
	NUM_REGS,
};

static u8 sm5602_regs[NUM_REGS] = {
	0x00, /* DEVICE_ID */
	0x01, /* CNTL */
	0x02, /* INT */
	0x03, /* INT_MASK */
	0x04, /* STATUS */
	0x05, /* SOC */
	0x06, /* OCV */
	0x07, /* VOLTAGE */
	0x08, /* CURRENT */
	0x09, /* TEMPERATURE_IN */
	0x0A, /* TEMPERATURE_EX */
	0x0C, /* V_L_ALARM */
	0x0D, /* V_H_ALARM */	
	0x0E, /* A_H_ALARM */
	0x0F, /* T_IN_H_ALARM */
	0x10, /* SOC_L_ALARM */
	0x11, /* FG_OP_STATUS */
	0x12, /* TOPOFFSOC */
	0x13, /* PARAM_CTRL */
	0x14, /* SHUTDOWN */
	0x1A, /* VIT_PERIOD */
	0x1B, /* CURRENT_RATE */
	0x62, /* BAT_CAP */	
	0x73, /* CURR_OFFSET */	
	0x74, /* CURR_SLOPE */
	0x90, /* MISC */
	0x91, /* RESET */
	0x95, /* RSNS_SEL */
	0x96, /* VOL_COMP */
};

int sm5602_cycle;
 EXPORT_SYMBOL_GPL(sm5602_cycle);

int sm5602_CV;
 EXPORT_SYMBOL_GPL(sm5602_CV);

enum sm_fg_device {
	SM5602,
};

enum sm_fg_temperature_type {
	TEMPERATURE_IN = 0,
	TEMPERATURE_EX,
	TEMPERATURE_3RD,
};

const unsigned char *device2str[] = {
	"sm5602",
};

enum battery_table_type {
	BATTERY_TABLE0 = 0,
	BATTERY_TABLE1,
	BATTERY_TABLE2,
	BATTERY_TABLE_MAX,
};

#if defined (FEATURE_BATT_LONG_LIFE_CYCLE)
enum sm5602_batt_cycle_step_e
{
	STEP0_BATT_CYCLE = 0,
	STEP1_BATT_CYCLE,
	STEP2_BATT_CYCLE,
	STEP3_BATT_CYCLE,
	STEP4_BATT_CYCLE,
	BATT_CYCLE_MAX,
};

enum sm5602_cv_voltage_step_e
{
	STEP0_CV_VOLTAGE = 0,
	STEP1_CV_VOLTAGE,
	STEP2_CV_VOLTAGE,
	STEP3_CV_VOLTAGE,
	STEP4_CV_VOLTAGE,
	STEP_CV_VOLTAGE_MAX,
};

enum sm5602_batt_cycle_count_e
{
	STEP0_MAX_SOC_CYCLE_COUNT = 299,
	STEP1_MAX_SOC_CYCLE_COUNT = 399,
	STEP2_MAX_SOC_CYCLE_COUNT = 699,
	STEP3_MAX_SOC_CYCLE_COUNT = 999,
	MAX_SOC_CYCLE_COUNT,
};

enum sm5602_batt_life_cycle_cv_voltage_e
{
	STEP0_MAX_SOC_CYCLE_CV_VOLTAGE = 4350,
	STEP1_MAX_SOC_CYCLE_CV_VOLTAGE = 4330,
	STEP2_MAX_SOC_CYCLE_CV_VOLTAGE = 4310,
	STEP3_MAX_SOC_CYCLE_CV_VOLTAGE = 4290,
	MAX_SOC_CYCLE_CV_VOLTAGE       = 4240,
};
#endif

#ifdef ENABLE_NTC_COMPENSATION_1
int tex_sim_uV[43] = {
	 	0,		 270,       480,        510, 	   910,      
	 1180,      1330,      2120,       2220, 	  2400,      
	 2660,      3290,      4000,       4790, 	  5910,      
	 6650,      7440,      8000,      11670, 	 11940,     
	13260,     13350,     14370,      16040, 	 20930,     
	23760,     26370,     28720,      42490, 	 47060,     
	64860,     75410,     87080,     114290, 	126320,    
   152320,    218180,    229150,     383190, 	508830,    
   783080,   1024200,    1374360
};

short tex_sim_adc_code[43] = {
   0x8001, 0x8D10,    0x8D1B,     
   0x8D1C, 0x8D30,    0x8D3E,    0x8D45,    0x8D6B,     
   0x8D70, 0x8D78,    0x8D86,    0x8DA5,    0x8DC8,     
   0x8DEE, 0x8E25,    0x8E4A,    0x8E70,    0x8E8B,     
   0x8F40, 0x8F4D,    0x8F8F,    0x8F93,    0x8FC4,     
   0x9018, 0x9106,    0x9192,    0x9213,    0x9288,     
   0x9530, 0x960B,    0x997E,    0x9B85,    0x9DD5,     
   0xA2FB, 0xA573,    0xAA5F,    0xB70C,    0xB9E2,     
   0xD720, 0xEFF9,    0x222B,    0x452A,    0x7FC0
};     

int tex_meas_uV[249] = {
		  0,    5000,   10000,   15000,   20000, 
	  25000,   30000,   35000,   40000,   45000, 
	  50000,   55000,   60000,   65000,   70000, 
	  75000,   80000,   85000,   90000,   95000, 
	 100000,  105000,  110000,  115000,  120000, 
	 125000,  130000,  135000,  140000,  145000, 
	 150000,  155000,  160000,  165000,  170000, 
	 175000,  180000,  185000,  190000,  195000, 
	 200000,  205000,  210000,  215000,  220000, 
	 225000,  230000,  235000,  240000,  245000, 
	 250000,  255000,  260000,  265000,  270000, 
	 275000,  280000,  285000,  290000,  295000, 
	 300000,  305000,  310000,  315000,  320000, 
	 325000,  330000,  335000,  340000,  345000, 
	 350000,  355000,  360000,  365000,  370000, 
	 375000,  380000,  385000,  390000,  395000, 
	 400000,  405000,  410000,  415000,  420000, 
	 425000,  430000,  435000,  440000,  445000, 
	 450000,  455000,  460000,  465000,  470000, 
	 475000,  480000,  485000,  490000,  495000, 
	 500000,  505000,  510000,  515000,  520000, 
	 525000,  530000,  535000,  540000,  545000, 
	 550000,  555000,  560000,  565000,  570000, 
	 575000,  580000,  585000,  590000,  595000, 
	 600000,  605000,  610000,  615000,  620000, 
	 625000,  630000,  635000,  640000,  645000, 
	 650000,  655000,  660000,  665000,  670000, 
	 675000,  680000,  685000,  690000,  695000, 
	 700000,  705000,  710000,  715000,  720000, 
	 725000,  730000,  735000,  740000,  745000, 
	 750000,  755000,  760000,  765000,  770000, 
	 775000,  780000,  785000,  790000,  795000, 
	 800000,  805000,  810000,  815000,  820000, 
	 825000,  830000,  835000,  840000,  845000, 
	 850000,  855000,  860000,  865000,  870000, 
	 875000,  880000,  885000,  890000,  895000, 
	 900000,  905000,  910000,  915000,  920000, 
	 925000,  930000,  935000,  940000,  945000, 
	 950000,  955000,  960000,  965000,  970000, 
	 975000,  980000,  985000,  990000,  995000, 
	1000000, 1005000, 1010000, 1015000, 1020000, 
	1025000, 1030000, 1035000, 1040000, 1045000, 
	1050000, 1055000, 1060000, 1065000, 1070000, 
	1075000, 1080000, 1085000, 1090000, 1095000, 
	1100000, 1105000, 1110000, 1115000, 1120000, 
	1125000, 1130000, 1135000, 1140000, 1145000, 
	1150000, 1155000, 1160000, 1165000, 1170000, 
	1175000, 1180000, 1185000, 1190000, 1195000, 
	1200000, 1205000, 1210000, 1215000, 1220000, 
	1225000, 1230000, 1235000, 1240000
}; 

short tex_meas_adc_code[249] = {
	0x8D18, 0x8DEF, 0x8ED4, 0x8FB5, 0x909C, 
	0x917F, 0x9267, 0x9348, 0x9430, 0x9516, 
	0x95FC, 0x96DF, 0x97C2, 0x98AA, 0x998B, 
	0x9A70, 0x9B54, 0x9C3C, 0x9D1F, 0x9E02, 
	0x9EEC, 0x9FD1, 0xA0B2, 0xA198, 0xA27D, 
	0xA35E, 0xA448, 0xA529, 0xA610, 0xA6F0, 
	0xA7DF, 0xA8C0, 0xA9A7, 0xAA8B, 0xAB6D, 
	0xAC53, 0xAD39, 0xAE20, 0xAF04, 0xAFEA, 
	0xB0CB, 0xB1B3, 0xB298, 0xB37E, 0xB461, 
	0xB547, 0xB62C, 0xB712, 0xB7F5, 0xB8D9, 
	0xB9C1, 0xBAA5, 0xBB88, 0xBC6F, 0xBD51, 
	0xBE3B, 0xBF1E, 0xC003, 0xC0E8, 0xC1CE, 
	0xC2B1, 0xC397, 0xC47C, 0xC562, 0xC644, 
	0xC72C, 0xC811, 0xC8F3, 0xC9D6, 0xCABE, 
	0xCB9F, 0xCC88, 0xCD6B, 0xCE52, 0xCF36, 
	0xD01A, 0xD101, 0xD1E6, 0xD2CB, 0xD3AE, 
	0xD492, 0xD578, 0xD65A, 0xD745, 0xD826, 
	0xD90D, 0xD9F2, 0xDAD9, 0xDBBE, 0xDCA1, 
	0xDD85, 0xDE6B, 0xDF4E, 0xE036, 0xE118, 
	0xE1FB, 0xE2E4, 0xE3C5, 0xE4AB, 0xE591, 
	0xE678, 0xE75C, 0xE840, 0xE925, 0xEA0B, 
	0xEAEC, 0xEBD1, 0xECBA, 0xED9E, 0xEE85, 
	0xEF85, 0xF061, 0xF148, 0xF22F, 0xF30A, 
	0xF3F6, 0xF4D9, 0xF5B0, 0xF6A1, 0xF78B, 
	0x9F, 0x17D, 0x26A, 0x34C, 0x43B, 
	0x520, 0x5F5, 0x6E0, 0x7C5, 0x8B5, 
	0x997, 0xA76, 0xB5B, 0xC3D, 0xD23, 
	0xE08, 0xEF4, 0xFDA, 0x10C0, 0x11A2, 
	0x128F, 0x1371, 0x142A, 0x150C, 0x15F7, 
	0x16D4, 0x17BD, 0x189E, 0x1950, 0x1A6A, 
	0x1B4F, 0x1C31, 0x1D1B, 0x1DFB, 0x1EDB, 
	0x1FBF, 0x20AE, 0x2191, 0x2273, 0x235B, 
	0x2445, 0x2530, 0x261B, 0x26FA, 0x27E5, 
	0x28BB, 0x29A4, 0x2A9F, 0x2B6F, 0x2C5C, 
	0x2D38, 0x2E11, 0x2F2A, 0x2FE9, 0x30D7, 
	0x31B9, 0x3296, 0x3381, 0x3467, 0x3553, 
	0x3637, 0x3717, 0x3802, 0x38E4, 0x39CA, 
	0x3AAD, 0x3B91, 0x3C79, 0x3D5B, 0x3E47, 
	0x3F20, 0x400B, 0x40F0, 0x41DB, 0x42C5, 
	0x43A0, 0x447F, 0x4566, 0x4653, 0x472D, 
	0x4815, 0x48F0, 0x49E1, 0x4ACA, 0x4BA8, 
	0x4C8D, 0x4D76, 0x4E5E, 0x4F36, 0x5021, 
	0x5110, 0x51F2, 0x52D3, 0x53BB, 0x54AA, 
	0x558D, 0x5671, 0x5759, 0x583B, 0x5926, 
	0x5A0E, 0x5AEF, 0x5BD5, 0x5CBD, 0x5D9C, 
	0x5E7E, 0x5F67, 0x604B, 0x612D, 0x6210, 
	0x62FB, 0x63E2, 0x64CE, 0x65A5, 0x668E, 
	0x6775, 0x6858, 0x6935, 0x6A22, 0x6B05, 
	0x6BEF, 0x6CCB, 0x6DD5, 0x6E9E, 0x6F6F, 
	0x7050, 0x7127, 0x7208, 0x72E7
};

#define OVERHEAT_TH_DEG		50
#define COLD_TH_DEG			0
#endif

#ifdef SOC_SMOOTH_TRACKING
#define BATT_MA_AVG_SAMPLES	8
struct batt_params {
	bool			update_now;
	int				batt_raw_soc; //x.x% ex)500 = 50.0%
	int				batt_soc; //x.x% ex)500 = 50.0%
	int				samples_num;
	int				samples_index;
	int				batt_ma_avg_samples[BATT_MA_AVG_SAMPLES];
	int				batt_ma_avg;
	int				batt_ma_prev;
	int				batt_ma;
	int				batt_mv;
	int				batt_temp;
//	struct timespec		last_soc_change_time;
};
#endif

#ifdef ENABLE_TEMP_AVG
#define BATT_TEMP_AVG_SAMPLES	8
struct batt_temp_params {
	bool		update_now;
	int			batt_raw_temp;
	int			batt_temp;
	int			samples_num;
	int			samples_index;
	int			batt_temp_avg_samples[BATT_TEMP_AVG_SAMPLES];
	int			batt_temp_avg;
	int			batt_temp_prev;
//	struct timespec		last_temp_change_time;
};
#endif

struct sm_fg_chip;

struct sm_fg_chip {
	struct device		*dev;
	struct i2c_client	*client;
	struct mutex i2c_rw_lock; /* I2C Read/Write Lock */
	struct mutex data_lock; /* Data Lock */
	u8 chip;
	u8 regs[NUM_REGS];
	int	batt_id;
	int gpio_int;
	
	/* Status Tracking */
	bool batt_present;
	bool batt_fc;	/* Battery Full Condition */
	bool batt_ot;	/* Battery Over Temperature */
	bool batt_ut;	/* Battery Under Temperature */
	bool batt_soc1;	/* SOC Low */
	bool batt_socp;	/* SOC Poor */
	bool batt_dsg;	/* Discharge Condition*/
	bool batt_id_first_flag;/* Get battery id flag for first */
	int	batt_soc;
	int	batt_ui_soc;
	int batt_ocv;
	int batt_fcc;	/* Full charge capacity */
	int	batt_volt;
	int	aver_batt_volt;
	int	batt_temp;
	int	batt_curr;
	int batt_rmc;
	int is_charging;	/* Charging informaion from charger IC */
    int batt_soc_cycle; /* Battery SOC cycle */
    int topoff_soc;
	int topoff_margin;
    int top_off;
	int iocv_error_count;
	int fg_reset_flag;
#ifdef SOC_SMOOTH_TRACKING
	bool	soc_reporting_ready;
#endif
#ifdef SHUTDOWN_DELAY
	bool	shutdown_delay_enable;
	bool	shutdown_delay;
#endif
	bool fast_mode;
	/* previous battery voltage current*/
    int p_batt_voltage;
    int p_batt_current;
	int p_report_soc;
	
	/* DT */
	bool en_temp_ex;
	bool en_temp_in;
	bool en_temp_3rd;
	bool en_batt_det;
	bool iocv_man_mode;
	int aging_ctrl;
	int batt_rsns;	/* Sensing resistor value */
	int cycle_cfg;
	int fg_irq_set;
	int low_soc1;
	int low_soc2;
	int v_l_alarm;
	int v_h_alarm;
	int battery_table_num;
	int misc;
    int batt_v_max;
	int min_cap;
	u32 common_param_version;
  	int table_check_flag;
	int t_l_alarm_in; 
	int t_h_alarm_in;
	u32 t_l_alarm_ex;
	u32 t_h_alarm_ex;
	int rpara;
	int curr_voffset;
	int curr_vslope;
	
	/* Battery Data */
	int battery_table[BATTERY_TABLE_MAX][FG_TABLE_LEN];
	signed short battery_temp_table[FG_TEMP_TABLE_CNT_MAX]; /* Degree */
	int alpha;
	int beta;
	int rs;
	int rs_value[4];
	int vit_period;
	int mix_value;
	const char		*battery_type;
	int volt_cal;
	int curr_offset;
	int curr_slope;
	int cap;
    int n_tem_poff;
    int n_tem_poff_offset;
	int batt_max_voltage_uv;
	int temp_std;
	int en_high_fg_temp_offset;
    int high_fg_temp_offset_denom;
    int high_fg_temp_offset_fact;
    int en_low_fg_temp_offset;
    int low_fg_temp_offset_denom;
    int low_fg_temp_offset_fact;
	int en_high_fg_temp_cal;
    int high_fg_temp_p_cal_denom;
    int high_fg_temp_p_cal_fact;
    int high_fg_temp_n_cal_denom;
    int high_fg_temp_n_cal_fact;
    int en_low_fg_temp_cal;
    int low_fg_temp_p_cal_denom;
    int low_fg_temp_p_cal_fact;
    int low_fg_temp_n_cal_denom;
    int low_fg_temp_n_cal_fact;
	int	en_high_temp_cal;
    int high_temp_p_cal_denom;
    int high_temp_p_cal_fact;
    int high_temp_n_cal_denom;
    int high_temp_n_cal_fact;
    int en_low_temp_cal;
    int low_temp_p_cal_denom;
    int low_temp_p_cal_fact;
    int low_temp_n_cal_denom;
    int low_temp_n_cal_fact;
	u32 battery_param_version;
#ifdef ENABLE_NTC_COMPENSATION_1	
	int rtrace;
#endif
	
	struct delayed_work monitor_work;
#ifdef ENABLE_INIT_DELAY_TEMP	
	struct delayed_work init_delay_temp_work;
#endif
	
#ifdef DELAY_SOC_ZERO	
	struct delayed_work delay_soc_zero_work;
#endif

#ifdef SOC_SMOOTH_TRACKING	
#if 0	
	struct delayed_work soc_monitor_work;
#endif
	int charge_full;
#endif
	//unsigned long last_update;
#ifdef ENABLE_INIT_DELAY_TEMP
	int en_init_delay_temp;
#endif
#ifdef DELAY_SOC_ZERO
		int en_delay_soc_zero;
		int recover_delay_soc_zero;
#endif
	/* Debug */
	int *dec_rate_seq;
	int dec_rate_len;
	int	skip_reads;
	int	skip_writes;
	int fake_soc;
	int fake_temp;
	struct dentry *debug_root;
	struct power_supply *batt_psy;
	struct power_supply *fg_psy;
	struct power_supply *bbc_psy;
	uint32_t bbc_status;
#ifdef SOC_SMOOTH_TRACKING	
	struct batt_params	param;
#endif
#ifdef ENABLE_TEMP_AVG
	struct batt_temp_params	temp_param;
#endif
	struct iio_dev  *indio_dev;
    struct iio_chan_spec    *iio_chan;
    struct iio_channel      *int_iio_chans;
	struct iio_channel		*bat_id_vol;

#if defined (FEATURE_BATT_LONG_LIFE_CYCLE)
	uint16_t v_max_table[BATT_CYCLE_MAX];
	uint16_t q_max_table[BATT_CYCLE_MAX];
	
	uint16_t v_max_now;
	uint16_t q_max_now;
	
	int ready_long_life_cv_voltage_check;
	int batt_cycle_index;
	int cv_voltage_index;
#endif	
};

static int show_registers(struct seq_file *m, void *data);
static bool fg_init(struct i2c_client *client);
#if defined(SOC_SMOOTH_TRACKING) || defined(ENABLE_TEMP_AVG)
static int calculate_delta_time(ktime_t time_stamp, int *delta_time_s);
#endif
static int fg_read_current(struct sm_fg_chip *sm);
static bool fg_reg_init(struct i2c_client *client);

static struct sm_fg_chip *g_sm_fg_chip;

#if defined (FEATURE_BATT_LONG_LIFE_CYCLE)
static int fg_get_batt_long_life_index_by_cv_voltage(struct sm_fg_chip *sm);
#endif

static int __fg_read_word(struct i2c_client *client, u8 reg, u16 *val)
{
	s32 ret;
	int retry = 0;

	for(retry = 0; retry < 3; retry++){
		ret = i2c_smbus_read_word_data(client, reg);
		if (ret < 0) {
			pr_err("i2c read word fail: can't read from reg 0x%02X, retry = %d\n", reg, retry);
			mdelay(10);
		} else {
			break;
		}
	}

	if(retry >= 3){
		*val = 0;
		return ret;
	}

	*val = (u16)ret;

	return 0;
}

static int __fg_write_word(struct i2c_client *client, u8 reg, u16 val)
{
	s32 ret;
	int retry = 0;

	for(retry = 0; retry < 3; retry++){
		ret = i2c_smbus_write_word_data(client, reg, val);
		if (ret < 0) {
			pr_err("i2c write word fail: can't write 0x%02X to reg 0x%02X, retry %d\n",
					val, reg, retry);
			mdelay(10);
		} else {
			break;
		}
	}

	if(retry >= 3){
		return ret;
	}

	return 0;
}

static int fg_read_word(struct sm_fg_chip *sm, u8 reg, u16 *val)
{
	int ret;

	if (sm->skip_reads) {
		*val = 0;
		return 0;
	}
	/* TODO:check little endian */
	mutex_lock(&sm->i2c_rw_lock);
	ret = __fg_read_word(sm->client, reg, val);
	mutex_unlock(&sm->i2c_rw_lock);

	return ret;
}

static int fg_write_word(struct sm_fg_chip *sm, u8 reg, u16 val)
{
	int ret;

	if (sm->skip_writes)
		return 0;

	/* TODO:check little endian */
	mutex_lock(&sm->i2c_rw_lock);
	ret = __fg_write_word(sm->client, reg, val);
	mutex_unlock(&sm->i2c_rw_lock);

	return ret;
}

#define	FG_STATUS_SLEEP				BIT(10)
#define	FG_STATUS_BATT_PRESENT		BIT(9)
#define	FG_STATUS_SOC_UPDATE		BIT(8)
#define	FG_STATUS_TOPOFF			BIT(7)
#define	FG_STATUS_LOW_SOC2			BIT(6)
#define	FG_STATUS_LOW_SOC1			BIT(5)
#define	FG_STATUS_HIGH_CURRENT		BIT(4)
#define	FG_STATUS_HIGH_TEMPERATURE	BIT(3)
#define	FG_STATUS_LOW_TEMPERATURE	BIT(2)
#define	FG_STATUS_HIGH_VOLTAGE		BIT(1)
#define	FG_STATUS_LOW_VOLTAGE		BIT(0)

#define	FG_OP_STATUS_CHG_DISCHG		BIT(15) //if can use the charger information, plz use the charger information for CHG/DISCHG condition.

static int fg_read_status(struct sm_fg_chip *sm)
{
	int ret;
	u16 flags1, flags2;
#ifdef ENABLE_MIX_NTC_BATTDET	
	u16 data = 0;
	signed short uval = 0;
#endif

	ret = fg_read_word(sm, sm->regs[SM_FG_REG_STATUS], &flags1);
	if (ret < 0)
		return ret;

	ret = fg_read_word(sm, sm->regs[SM_FG_REG_FG_OP_STATUS], &flags2);
		if (ret < 0)
			return ret;

#ifdef ENABLE_MIX_NTC_BATTDET		
	if (sm->en_temp_ex) {
		ret = fg_read_word(sm, sm->regs[SM_FG_REG_TEMPERATURE_EX], &data);
		if (ret < 0) {
			pr_err("could not read temperature ex , ret = %d\n", ret);
			return ret;
		} else {
			uval = data;
		}
	}
#endif	
	mutex_lock(&sm->data_lock);

	if (sm->en_batt_det) {
#if defined(ENABLE_MIX_NTC_BATTDET) && defined(ENABLE_INIT_DELAY_TEMP)
		// Present Case
		// - FG_STATUS_BATT_PRESENT 
		// - Out of 0x7700~0x77FF range and en_init_delay_temp = 0
		sm->batt_present	= (!!(flags1 & FG_STATUS_BATT_PRESENT) 
								|| (!!!(flags1 & FG_STATUS_BATT_PRESENT) && (!(0x7700 <= uval && 0x77FF >= uval) && (sm->en_init_delay_temp == 0))));
#elif defined(ENABLE_MIX_NTC_BATTDET)
		// Present Case
		// - FG_STATUS_BATT_PRESENT 
		// - Out of 0x7700~0x77FF range
		sm->batt_present	= (!!(flags1 & FG_STATUS_BATT_PRESENT) 
								|| (!!!(flags1 & FG_STATUS_BATT_PRESENT) && (!(0x7700 <= uval && 0x77FF >= uval))));
#else
		sm->batt_present	= !!(flags1 & FG_STATUS_BATT_PRESENT);
#endif
	} else
		sm->batt_present	= true; //Always battery presented
	sm->batt_ot			= !!(flags1 & FG_STATUS_HIGH_TEMPERATURE);
	sm->batt_ut			= !!(flags1 & FG_STATUS_LOW_TEMPERATURE);
	sm->batt_fc			= !!(flags1 & FG_STATUS_TOPOFF);
	sm->batt_soc1		= !!(flags1 & FG_STATUS_LOW_SOC2);
	sm->batt_socp		= !!(flags1 & FG_STATUS_LOW_SOC1);
	sm->batt_dsg		= !!!(flags2 & FG_OP_STATUS_CHG_DISCHG);
	mutex_unlock(&sm->data_lock);

	return 0;
}

#ifdef FG_ENABLE_IRQ
static int fg_status_changed(struct sm_fg_chip *sm)
{
	cancel_delayed_work(&sm->monitor_work);
	schedule_delayed_work(&sm->monitor_work, 0);
	power_supply_changed(sm->fg_psy);

	return IRQ_HANDLED;
}

static irqreturn_t fg_irq_thread(int irq, void *dev_id)
{
	struct sm_fg_chip *sm = dev_id;
	int ret;
	u16 data_int, data_int_mask;

	/* Read INT */
	ret = fg_read_word(sm, sm->regs[SM_FG_REG_INT_MASK], &data_int_mask);
	if (ret < 0){
		pr_err("Failed to read INT_MASK, ret = %d\n", ret);	
		return ret;	
	}
	
	ret = fg_write_word(sm, sm->regs[SM_FG_REG_INT_MASK], 0x8000 | data_int_mask);
    if (ret < 0) {
		pr_err("Failed to write 0x8000 | INIT_MARK, ret = %d\n", ret);
		return ret;
	}

	ret = fg_read_word(sm, sm->regs[SM_FG_REG_INT], &data_int);
	if (ret < 0) {
		pr_err("Failed to write REG_INT, ret = %d\n", ret);
		return ret;
	}

	ret = fg_write_word(sm, sm->regs[SM_FG_REG_INT_MASK], 0x03FF & data_int_mask);
    if (ret < 0) {
		pr_err("Failed to write INIT_MARK, ret = %d\n", ret);
		return ret;
	}

	fg_status_changed(sm);

	pr_info("fg_read_int = 0x%x\n", data_int);

	return 0;
}
#endif

static int fg_read_soc(struct sm_fg_chip *sm)
{
	int ret;
	unsigned int soc = 0;
	u16 data = 0;

	ret = fg_read_word(sm, sm->regs[SM_FG_REG_SOC], &data);
	if (ret < 0) {
		pr_err("could not read SOC, ret = %d\n", ret);
		return ret;
	} else {
		/*integer bit;*/
		soc = ((data&0x7f00)>>8) * 10;
		/* integer + fractional bit*/
		soc = soc + (((data&0x00ff)*10)/256);
		
		if (data & 0x8000) {
			pr_err("fg_read_soc data=%d\n",data);
			soc *= -1;
		}
	}

	 pr_info("soc=%d(0x%x)\n",soc, data);
	return soc;
}

static int fg_read_ui_soc(struct sm_fg_chip *sm)
{
	int rawsoc = 0;
	int batt_ui_soc = 0;

	rawsoc = fg_read_soc(sm);
	batt_ui_soc = rawsoc * 100 / 93;
	if (batt_ui_soc > 1000) {
		batt_ui_soc = 1000;
	} else if (batt_ui_soc < 0) {
		batt_ui_soc = 0;
	}
	//pr_info("rawsoc=%d, batt_ui_soc=%d\n", rawsoc, batt_ui_soc);
	return batt_ui_soc;
}

static unsigned int fg_read_ocv(struct sm_fg_chip *sm)
{
	int ret;	
	u16 data = 0;
	unsigned int ocv;// = 3500; /*3500 means 3500mV*/

	ret = fg_read_word(sm, sm->regs[SM_FG_REG_OCV], &data);
	if (ret<0) {
		pr_err("could not read OCV, ret = %d\n", ret);
		ocv = 4000;
	} else {
		ocv = (((data&0x0fff)*1000)/2048) + (((data&0xf000)>>11)*1000);
	}

	pr_info("ocv=%d(0x%x)\n",ocv, data);
	
	return ocv; //mV
}

#ifdef ENABLE_NTC_COMPENSATION_1
short interp_meas_to_adc(int len, int X, int *pX, short *pY)
{
	int i;
#if 0	
	float slope, tmpf;
#else
	int slope, tmp;
#endif
	short new_y; 

	pr_info("interp_meas_to_adc\n\n");
	
	if (X < pX[0])
		return pY[0];
	else if (X > pX[len-1])
		return pY[len-1];

	for (i=0; i<len-1; i++) {
		if (X >= pX[i] && X <= pX[i+1])	
			break;
	}

	slope = (pY[i+1]-pY[i]);
	slope *= 100000;
	slope /= (pX[i+1] - pX[i]);
	tmp = slope * (X - pX[i])/100000 + (pY[i]);
	new_y = tmp;

	pr_info("%d, (%d, %d), (%4X, %4X), %4X\n\n", X, tex_meas_uV[i], tex_meas_uV[i+1], tex_meas_adc_code[i], tex_meas_adc_code[i+1], new_y);

	return new_y;
}

int interp_adc_to_meas(int len, short X, short *pX, int *pY)
{
	int i;
#if 0	
	float slope, tmpf;
#else
	int slope, tmp;
#endif
	int new_y; 
	

	pr_info("interp_adc_to_meas\n\n");
	
	if (X < pX[0])
		return pY[0];
	else if (X > pX[len-1])
		return pY[len-1];

	for (i=0; i<len-1; i++) {
		if (X >= pX[i] && X <= pX[i+1])	
			break;
	}

	slope = (pY[i+1]-pY[i]);
	slope *= 100000;
	slope /= (pX[i+1] - pX[i]);
	tmp = (slope * (X - pX[i]))/100000 + pY[i];
	new_y = tmp;

	pr_info("%4X, (%4X, %4X), (%d, %d), %d\n\n", X, tex_meas_adc_code[i], tex_meas_adc_code[i+1], tex_meas_uV[i], tex_meas_uV[i+1], new_y);

	return new_y;
}
#endif

#ifdef ENABLE_TEMP_AVG
#define CHANGE_TEMP_TIME_LIMIT_1	1 //1sec
#define CHANGE_TEMP_TIME_LIMIT_3	3 //3sec
#define CHANGE_TEMP_TIME_LIMIT_5	5 //5sec
static void calculate_average_temperature(struct sm_fg_chip *sm)
{
	int i;
	int avg_temp = sm->temp_param.batt_temp;
	int time_since_last_change_sec;
	int delta_time = 0;
	static ktime_t last_change_time;
	//struct timespec last_change_time = sm->temp_param.last_temp_change_time;

	calculate_delta_time(last_change_time, &time_since_last_change_sec);
	delta_time = time_since_last_change_sec / CHANGE_TEMP_TIME_LIMIT_1;

	pr_info("last_change_time = %d, time_since_last_change_sec = %d, delta_time = %d\n",
			last_change_time, time_since_last_change_sec, delta_time);

#if 0
	/* continue if temperature has changed, if exceed time */
	if ((sm->temp_param.batt_temp == sm->temp_param.batt_temp_prev)	
		&& (delta_time < 1)) {
#else
	/* continue if exceed time */
	if (delta_time < 1) {
#endif
		goto skip_avg;
	} else {
		sm->temp_param.batt_temp_prev = sm->temp_param.batt_temp;
		last_change_time = ktime_get();
	}

	pr_info("last_change_time = %d, time_since_last_change_sec = %d, delta_time = %d\n",
			last_change_time, time_since_last_change_sec, delta_time);


	sm->temp_param.batt_temp_avg_samples[sm->temp_param.samples_index] = avg_temp;
	sm->temp_param.samples_index = (sm->temp_param.samples_index + 1) % BATT_TEMP_AVG_SAMPLES;
	sm->temp_param.samples_num++;

	if (sm->temp_param.samples_num >= BATT_TEMP_AVG_SAMPLES)
		sm->temp_param.samples_num = BATT_TEMP_AVG_SAMPLES;

	if (sm->temp_param.samples_num) {
		avg_temp = 0;
		/* maintain a BATT_TEMP_AVG_SAMPLES sample average of battery temp */
		for (i = 0; i < sm->temp_param.samples_num; i++) {
			// pr_debug("temp_samples[%d] = %d\n", i, sm->temp_param.batt_temp_avg_samples[i]);
			avg_temp += sm->temp_param.batt_temp_avg_samples[i];
		}
		// BATT_TEMP_AVG_SAMPLES = 8
		pr_info("temp_samples[0:7] = %d %d %d %d %d %d %d %d, samples_index = %d\n",
					sm->temp_param.batt_temp_avg_samples[0], sm->temp_param.batt_temp_avg_samples[1],
					sm->temp_param.batt_temp_avg_samples[2], sm->temp_param.batt_temp_avg_samples[3],
					sm->temp_param.batt_temp_avg_samples[4], sm->temp_param.batt_temp_avg_samples[5],
					sm->temp_param.batt_temp_avg_samples[6], sm->temp_param.batt_temp_avg_samples[7],
					sm->temp_param.samples_index
					);
		
		sm->temp_param.batt_temp_avg = DIV_ROUND_CLOSEST(avg_temp, sm->temp_param.samples_num);
	}

skip_avg:
	pr_info("temp_now = %d, averaged_avg_temp = %d\n",
			sm->temp_param.batt_temp, sm->temp_param.batt_temp_avg);
}
#endif

static int _calculate_battery_temp_ex(struct sm_fg_chip *sm, u16 uval)
{
	int i = 0, temp = 0;
	signed short val = 0;
#ifdef ENABLE_NTC_COMPENSATION_1
	int len_meas_data;
	signed short code_adc = 0;
	int code_meas, temp_mv = 0.0;
	int rtrace, curr = 0;
#endif

	if ((uval >= 0x8001) && (uval <= 0x823B)) {
		pr_info("sp_range uval = 0x%x\n",uval);
		uval = 0x0000;
	}

	val = uval;

#ifdef ENABLE_NTC_COMPENSATION_1
	pr_info("ENABLE_NTC_COMPENSATION : val = 0x%x\n",val);

	len_meas_data = sizeof(tex_meas_uV)/sizeof(int);

	curr = fg_read_current(sm); 		//fg_read_current(sm) must return mA

	rtrace = sm->rtrace;//uohm : 7300uohm = 7.3mohm

	code_meas = interp_adc_to_meas(len_meas_data, val, tex_meas_adc_code, tex_meas_uV);

	//Charging : Vthem = Vntc-I*Rtrace, Discharging : Vthem = Vntc+I*Rtrace
	temp_mv = (code_meas) - (curr * rtrace)/1000;  

	code_adc = interp_meas_to_adc(len_meas_data, temp_mv, tex_meas_uV, tex_meas_adc_code);

	val = code_adc;

	pr_info("ENABLE_NTC_COMPENSATION : val(code_adc) = 0x%x, temp_mv = %d, code_meas = %d, curr = %d, rtrace = %d\n", 
		val, temp_mv, code_meas, curr, rtrace);	
#endif	
	if (val >= sm->battery_temp_table[0]) {
		temp = EX_TEMP_MIN; //Min : -18
	} else if (val <= sm->battery_temp_table[FG_TEMP_TABLE_CNT_MAX-1]) {
		temp = EX_TEMP_MAX; //Max : 80
	} else { 		
		for (i = 0; i < FG_TEMP_TABLE_CNT_MAX; i++) {
			if  (val >= sm->battery_temp_table[i]) {
				temp = EX_TEMP_MIN + i; 								//[ex] ~-20 : -20(skip), -19.9~-19.0 : 19, -18.9~-18 : 18, .., -0.9~0 : 0 				
				if ((temp >= 1) && (val != sm->battery_temp_table[i])) //+ range 0~79 degree. In same value case, no needed (temp-1)
					temp = temp -1; 								  //[ex] 0.1~0.9 : 0, 1.1~1.9 : 1, .., 79.1~79.9 : 79  
				break;
			}
		}
	}

	pr_info("uval = 0x%x, val = 0x%x, temp = %d\n",uval, val, temp);

	return temp;		
}

static int fg_read_temperature(struct sm_fg_chip *sm, enum sm_fg_temperature_type temperature_type)
{
	int ret, temp = 0;
	u16 data = 0;
	union power_supply_propval psp_temp = {0,};

	switch (temperature_type) {
	case TEMPERATURE_IN:	
		ret = fg_read_word(sm, sm->regs[SM_FG_REG_TEMPERATURE_IN], &data);
		if (ret < 0) {
			pr_err("could not read temperature in , ret = %d\n", ret);
			return ret;
		} else {
			/*integer bit*/
			temp = ((data & 0x00FF));
			if (data & 0x8000)
				temp *= -1;
		}
		pr_info("fg_read_temperature_in temp_in=%d\n", temp);
		break;
	case TEMPERATURE_EX:
		ret = fg_read_word(sm, sm->regs[SM_FG_REG_TEMPERATURE_EX], &data);
		if (ret < 0) {
			pr_err("could not read temperature ex , ret = %d\n", ret);
			return ret;
		} else {
			temp = _calculate_battery_temp_ex(sm, data);
		}
#ifdef ENABLE_INIT_DELAY_TEMP		
		if ((sm->en_init_delay_temp == 1) && (data == 0x0)) {
			temp = 25;	
			pr_info("fg_read_temperature_ex temp_ex=%d (en_init_delay_temp = [%d])\n", temp, sm->en_init_delay_temp);
		} else {
			pr_info("fg_read_temperature_ex temp_ex=%d (en_init_delay_temp = [%d])\n", temp, sm->en_init_delay_temp);
		}
#else
		pr_info("fg_read_temperature_ex temp_ex=%d\n", temp);
#endif

#ifdef ENABLE_TEMP_AVG
#ifdef ENABLE_INIT_DELAY_TEMP
	if ((sm->en_init_delay_temp == 1) && (data == 0x0)) {
	} else {
		sm->temp_param.batt_temp = temp;
		calculate_average_temperature(sm);
	}
#else
	sm->temp_param.batt_temp = temp;
	calculate_average_temperature(sm);
#endif
#endif
	break;
	case TEMPERATURE_3RD:	
		// If temperature value is obtained from external IC, pls add code in below case.
		// The unit of temperaute shoud be matched with integer type. ex.Return value(temp) 10 is 10 deg.   
		if (sm->batt_psy == NULL)
				sm->batt_psy = power_supply_get_by_name("battery");

		if (sm->batt_psy) {
			power_supply_get_property(sm->batt_psy,
				POWER_SUPPLY_PROP_TEMP, &psp_temp);
		}
		temp = psp_temp.intval / 10;
		pr_info("fg_read_temperature_3rd temp_3rd=%d\n", temp);
		break;

	default:
		return -EINVAL;
	}
	
	return temp;
}

/*
 *	Return : mV
 */
static int fg_read_volt(struct sm_fg_chip *sm)
{
	int ret = 0;
	int volt = 0;
	u16 data = 0;

	ret = fg_read_word(sm, sm->regs[SM_FG_REG_VOLTAGE], &data);
	if (ret < 0) {
		pr_err("could not read voltage, ret = %d\n", ret);
		return ret;
	}  else {
		/* */
		volt = 1800 * (data & 0x7FFF) / 19622;
		if (data&0x8000)
			volt *= -1;
		
		volt += 2700; 
	}

	/*cal avgvoltage*/
	sm->aver_batt_volt = (((sm->aver_batt_volt)*4) + volt)/5;

	pr_err("volt = %d(0x%x)\n",volt, data);

	return volt;
}

static int fg_get_cycle(struct sm_fg_chip *sm)
{
	int ret;
	int cycle;
	u16 data = 0;

	ret = fg_read_word(sm, FG_REG_SOC_CYCLE, &data);
	if (ret<0) {
		pr_err("read cycle reg fail ret = %d\n", ret);
		cycle = 0;
	} else {
		cycle = data&0x01FF;
	}


	pr_err("cycle = %d(0x%x)\n",cycle, data);

	return cycle;
}

#if defined (FEATURE_BATT_LONG_LIFE_CYCLE)
static int fg_get_batt_long_life_index_by_cv_voltage(struct sm_fg_chip *sm)
{
	int cycle_index = 0, cycle = 0;
	int cv_voltage_index = 0; 
  	int est_cv_voltage = sm5602_CV;

	//cycle index
	if(sm5602_cycle)
		cycle = sm5602_cycle;
	else
		cycle = fg_get_cycle(sm);

	if (cycle >= 0 && cycle <= STEP0_MAX_SOC_CYCLE_COUNT) 
	{
    	cycle_index = STEP0_BATT_CYCLE;
	}
	else if (cycle > STEP0_MAX_SOC_CYCLE_COUNT && cycle <= STEP1_MAX_SOC_CYCLE_COUNT) 
	{
		cycle_index = STEP1_BATT_CYCLE;
	}
	else if (cycle > STEP1_MAX_SOC_CYCLE_COUNT && cycle <= STEP2_MAX_SOC_CYCLE_COUNT) 
	{
		cycle_index = STEP2_BATT_CYCLE;
	}
	else if (cycle > STEP2_MAX_SOC_CYCLE_COUNT && cycle <= STEP3_MAX_SOC_CYCLE_COUNT) 
	{
		cycle_index = STEP3_BATT_CYCLE;
	}
	else
	{
		cycle_index = STEP4_BATT_CYCLE;
	}
	
	sm->batt_cycle_index = cycle_index;
	
	pr_info("batt_cycle = (%d) / cycle_index= (%d)\n",cycle,cycle_index);

	//cv_voltage index
	if(est_cv_voltage == STEP0_MAX_SOC_CYCLE_CV_VOLTAGE)
	{
		cv_voltage_index = STEP0_CV_VOLTAGE;
	}
	else if(est_cv_voltage == STEP1_MAX_SOC_CYCLE_CV_VOLTAGE)
	{
		cv_voltage_index = STEP1_CV_VOLTAGE;
	}
	else if(est_cv_voltage == STEP2_MAX_SOC_CYCLE_CV_VOLTAGE)
	{
		cv_voltage_index = STEP2_CV_VOLTAGE;
	}
	else if(est_cv_voltage == STEP3_MAX_SOC_CYCLE_CV_VOLTAGE)
	{
		cv_voltage_index = STEP3_CV_VOLTAGE;
	}
	else if(est_cv_voltage == MAX_SOC_CYCLE_CV_VOLTAGE)
	{
		cv_voltage_index = STEP4_CV_VOLTAGE;
	}
	else
	{
		pr_err("est_cv_voltage check error = (%d) set index(0)\n",est_cv_voltage);
		cv_voltage_index = STEP0_CV_VOLTAGE;
	}

	sm->cv_voltage_index = cv_voltage_index;

	pr_info("est_cv_voltage = (%d) / cv_voltage_index= (%d)\n",est_cv_voltage,cv_voltage_index);
	
	return cv_voltage_index;
}
#endif

static int fg_read_current(struct sm_fg_chip *sm)
{
	int ret = 0, rsns = 0;
	u16 data = 0;
	int curr = 0;

	ret = fg_read_word(sm, sm->regs[SM_FG_REG_CURRENT], &data);
	if (ret < 0) {
		pr_err("could not read current, ret = %d\n", ret);
		return ret;
	} else {
		if (sm->batt_rsns == -EINVAL) {
			pr_err("could not read sm->batt_rsns, rsns = 10mohm\n");
			rsns = 10;
		} else {
			sm->batt_rsns == 0 ? rsns = 5 : (rsns = sm->batt_rsns*10);
		}

		curr = ((data & 0x7FFF) * 1250 / 511 / rsns );

		if (data & 0x8000)
			curr *= -1;
	}
	
	pr_err("curr = %d(0x%x)\n",curr,data);

	return curr;
}

static int fg_read_fcc(struct sm_fg_chip *sm)
{
	int ret = 0;
	int fcc = 0;	
	u16 data = 0;
	int64_t temp = 0;

	ret = fg_read_word(sm, sm->regs[SM_FG_REG_BAT_CAP], &data);
	if (ret < 0) {
		pr_err("could not read FCC, ret=%d\n", ret);
		return ret;
	} else {
		/* */
		temp = div_s64((data & 0x7FFF) * 1000, 2048);
		fcc = temp;
	}

	pr_err("fcc = %d(0x%x)\n",fcc, data);

	return fcc;
}

static int fg_read_rmc(struct sm_fg_chip *sm)
{
	int ret = 0;
	int rmc = 0;	
	u16 data = 0;

	ret = fg_read_word(sm, FG_REG_RMC, &data);
	if (ret < 0) {
		pr_err("could not read RMC, ret=%d\n", ret);
		return ret;
	} else {
		/* */
		rmc = ((data & 0x7FFF) * 1000) >> 11;
	}

	pr_err("rmc = %d(0x%x)\n",rmc, data);

	return rmc;
}

#ifdef IN_PRO_ADJ
static int fg_in_pro(struct i2c_client *client)
{
	struct sm_fg_chip *sm = i2c_get_clientdata(client);
	int ret;
	u16 data;

	ret = fg_write_word(sm, 0x93, 0x0F00);
	if (ret < 0) {
		pr_err("Failed to write 0x93, ret = %d\n", ret);
		return ret;
	}

	fg_read_word(sm, 0x93, &data);
	pr_info("0x93 = 0x%x\n", data);

	return 0;
}
#endif

#define FG_SOFT_RESET	0x1A6 
static int fg_reset(struct sm_fg_chip *sm)
{
    int ret;

    ret = fg_write_word(sm, sm->regs[SM_FG_REG_RESET], FG_SOFT_RESET);
	if (ret < 0) {
		pr_err("could not reset, ret=%d\n", ret);
		return ret;
	}
	sm->fg_reset_flag = 0;
	pr_info("=== %s ===\n", __func__);

	msleep(800);

    return 0;
}

static int get_battery_status(struct sm_fg_chip *sm)
{
	union power_supply_propval ret = {0,};
	int rc;

	if (sm->batt_psy == NULL)
		sm->batt_psy = power_supply_get_by_name("battery");
	if (sm->batt_psy) {
		/* if battery has been registered, use the status property */
		rc = power_supply_get_property(sm->batt_psy,
					POWER_SUPPLY_PROP_STATUS, &ret);
		if (rc) {
			pr_err("Battery does not export status: %d\n", rc);
			return POWER_SUPPLY_STATUS_UNKNOWN;
		}
		return ret.intval;
	}

	/* Default to false if the battery power supply is not registered. */
	pr_err("battery power supply is not registered\n");
	return POWER_SUPPLY_STATUS_UNKNOWN;
}

static bool is_battery_charging(struct sm_fg_chip *sm)
{
	return get_battery_status(sm) == POWER_SUPPLY_STATUS_CHARGING; //From Charger
}

#ifdef SOC_SMOOTH_TRACKING
static bool is_battery_full(struct sm_fg_chip *sm)
{
	return get_battery_status(sm) == POWER_SUPPLY_STATUS_FULL; //From charger
}
#endif

#ifdef ENABLE_TEMBASE_ZDSCON
static void fg_tembase_zdscon(struct sm_fg_chip *sm)
{
	int hminman_value = 0;
	u16 data = 0;
#ifdef ENABLE_LTIM_ACT		
	u16	ltim_value = 0;
#endif
	int ret = 0;
	int fg_temp_gap = sm->batt_temp - sm->temp_std;

#ifdef ENABLE_TEM_RATE_CONTROL
	if (fg_temp_gap > -5)
	{
		ret = fg_read_word(sm, FG_REG_AGING_CTRL, &data);
		if (data != (sm->aging_ctrl | 0x0C))
		{
			pr_info("%s: last rate control data 0x%x change to fast rate\n", __func__, data);
			ret = fg_write_word(sm, FG_REG_AGING_CTRL, sm->aging_ctrl | 0x0C);
		}
	}
	else
	{
		ret = fg_read_word(sm, FG_REG_AGING_CTRL, &data);
		if (data != sm->aging_ctrl)
		{
			pr_info("%s: last rate control data 0x%x change to normal rate\n", __func__, data);
			ret = fg_write_word(sm, FG_REG_AGING_CTRL, sm->aging_ctrl);
		}
	}

	if (ret < 0) 
	{
		pr_err("could not control 0x%x, fg_temp_gap = %d, ret = %d, data = 0x%x \n", FG_REG_AGING_CTRL, fg_temp_gap, ret, data);
	}
#endif

	if ((fg_temp_gap < 0) && !(sm->is_charging))
	{
		fg_temp_gap = abs(fg_temp_gap);
		if (fg_temp_gap > ZDSCON_ACT_TEMP_GAP)
		{
#ifdef ENABLE_LTIM_ACT
			if (fg_temp_gap > LTIM_ACT_TEMP_GAP) {
				if (abs(sm->batt_curr) > LTIM_I_LIMIT) {
					ltim_value = sm->rs_value[1] - (((LTIM_FACTOR * abs(sm->batt_curr)) / 1000) + LTIM_DENOM);
					if (ltim_value < LTIM_MIN) {
						ltim_value = LTIM_MIN;
					}
					ret = fg_read_word(sm, FG_REG_RS_1, &data);
					if (ret < 0) {
						pr_err("could not read 0x%d, ret = %d\n", FG_REG_RS_1, ret);
					}
					else
					{
						if (data != ltim_value)
						{
							fg_write_word(sm, FG_REG_RS_1, ltim_value);
							pr_info("%s: ltim_value value set 0x%x tem(%d) i(%d)\n", __func__, ltim_value, sm->batt_temp, sm->batt_curr);
						}
					}
				}
				else
				{
					ret = fg_read_word(sm, FG_REG_RS_1, &data);
					if (ret < 0) {
						pr_err("could not read 0x%x, ret = %d\n", FG_REG_RS_1, ret);
					}
					else
					{
						if (data != sm->rs_value[1])
						{
							fg_write_word(sm, FG_REG_RS_1, sm->rs_value[1]);
							pr_info("%s: ltimfactor value restore 0x%x -> 0x%x tem(%d)\n", __func__, data, sm->rs_value[1], sm->batt_temp);
						}
					}
				}
			}
			else
			{
				ret = fg_read_word(sm, FG_REG_RS_1, &data);
				if (ret < 0) {
					pr_err("could not read 0x%x, ret = %d\n", FG_REG_RS_1, ret);
				}
				else
				{
					if (data != sm->rs_value[1])
					{
						fg_write_word(sm, FG_REG_RS_1, sm->rs_value[1]);
						pr_info("%s: ltimfactor value restore 0x%x -> 0x%x tem(%d)\n", __func__, data, sm->rs_value[1], sm->batt_temp);
					}
				}
			}
#endif
			hminman_value = sm->rs_value[3] + (((fg_temp_gap - ZDSCON_ACT_TEMP_GAP) * HMINMAN_T_VALUE_FACT) / T_GAP_DENOM);
			hminman_value = hminman_value - ((abs(sm->batt_curr) * HMINMAN_I_VALUE_FACT) / I_GAP_DENOM);
			if (hminman_value < sm->rs_value[3]) {
				hminman_value = sm->rs_value[3];
			}
			ret = fg_read_word(sm, FG_REG_RS_3, &data);
			if (ret < 0) {
				pr_err("could not read , ret = %d\n", ret);
			}
			else
			{
				if (data != hminman_value)
				{
					fg_write_word(sm, FG_REG_RS_3, hminman_value);
					fg_write_word(sm, FG_REG_RS_0, hminman_value+2);
					pr_info("%s: hminman value set 0x%x tem(%d) i(%d)\n", __func__, hminman_value, sm->batt_temp, sm->batt_curr);
				}
			}
		}
		else
		{
			ret = fg_read_word(sm, FG_REG_RS_3, &data);
			if (ret < 0) {
				pr_err("could not read , ret = %d\n", ret);
			}
			else
			{
				if (data != sm->rs_value[3])
				{
					fg_write_word(sm, FG_REG_RS_3, sm->rs_value[3]);
					fg_write_word(sm, FG_REG_RS_0, sm->rs_value[0]);
					pr_info("%s: hminman value restore 0x%x -> 0x%x tem(%d)\n", __func__, data, sm->rs_value[3], sm->batt_temp);
				}
			}
#ifdef ENABLE_LTIM_ACT
			ret = fg_read_word(sm, FG_REG_RS_1, &data);
			if (ret < 0) {
				pr_err("could not read 0x%x, ret = %d\n", FG_REG_RS_1, ret);
			}
			else
			{
				if (data != sm->rs_value[1])
				{
					fg_write_word(sm, FG_REG_RS_1, sm->rs_value[1]);
					pr_info("%s: ltimfactor value restore 0x%x -> 0x%x tem(%d)\n", __func__, data, sm->rs_value[1], sm->batt_temp);
				}
			}
#endif
		}
	}
	else
	{
		ret = fg_read_word(sm, FG_REG_RS_3, &data);
		if (ret < 0) {
			pr_err("could not read 0x%x, ret = %d\n", FG_REG_RS_3, ret);
		}
		else
		{
			if (data != sm->rs_value[3])
			{
				fg_write_word(sm, FG_REG_RS_3, sm->rs_value[3]);
				fg_write_word(sm, FG_REG_RS_0, sm->rs_value[0]);
				pr_info("%s: hminman value restore 0x%x -> 0x%x tem(%d)\n", __func__, data, sm->rs_value[3], sm->batt_temp);
			}
		}
#ifdef ENABLE_LTIM_ACT
		ret = fg_read_word(sm, FG_REG_RS_1, &data);
		if (ret < 0) {
			pr_err("could not read 0x%x, ret = %d\n", FG_REG_RS_1, ret);
		}
		else
		{
			if (data != sm->rs_value[1])
			{
				fg_write_word(sm, FG_REG_RS_1, sm->rs_value[1]);
				pr_info("%s: ltimfactor value restore 0x%x -> 0x%x tem(%d)\n", __func__, data, sm->rs_value[1], sm->batt_temp);
			}
		}
#endif
	}

           return;
}
#endif

static void fg_vbatocv_check(struct sm_fg_chip *sm)
{
	int ret = 0;
	u16 data = 0;	

	pr_info("sm->batt_curr (%d), sm->is_charging (%d), sm->top_off (%d), sm->batt_soc (%d), sm->topoff_margin (%d)\n", 
			sm->batt_curr, sm->is_charging, sm->top_off, sm->batt_soc, sm->topoff_margin);

	ret = fg_read_word(sm, FG_REG_RS_0, &data);
	if (ret < 0) {
		pr_err("could not read , ret = %d\n", ret);
	}

#ifdef ENABLE_VLCM_MODE
		if((abs(sm->batt_curr)<50) ||
#else
		if(
#endif
	((sm->is_charging) && (abs(sm->batt_curr)<(sm->top_off+sm->topoff_margin)) &&
	(abs(sm->batt_curr)>(sm->top_off-sm->topoff_margin)) && (sm->batt_ui_soc>=900)))
	{
		if(abs(sm->batt_ocv-sm->batt_volt)>30)
		{
			sm->iocv_error_count ++;
		}

		pr_info("sm5602 FG iocv_error_count (%d)\n", sm->iocv_error_count);

		if(sm->iocv_error_count > 5)
			sm->iocv_error_count = 6;
	}
#ifdef ENABLE_HCRSM_MODE
	else if ((sm->is_charging) && (sm->batt_volt>HCRSM_VOLT) && (sm->p_batt_voltage>HCRSM_VOLT)
	&& (abs(sm->batt_curr)<HCRSM_CURR) && (abs(sm->p_batt_current)<HCRSM_CURR))
	{
		sm->iocv_error_count = 10;
	}
#ifdef ENABLE_FCHG_VSOC_MODE
	else if ((sm->is_charging) && (sm->batt_volt>FCHG_VSOC_VOLT) && (sm->p_batt_voltage>FCHG_VSOC_VOLT)
	&& (sm->batt_soc<FCHG_VSOC_SOC))
	{
		sm->iocv_error_count = 10;
	}
#endif
#endif
	else
	{
		sm->iocv_error_count = 0;
	}

	if(sm->iocv_error_count > 5)
	{
		pr_info("p_v - v = (%d)\n", sm->p_batt_voltage - sm->batt_volt);
		if(abs(sm->p_batt_voltage - sm->batt_volt)>15)
		{
			sm->iocv_error_count = 0;
		}
		else
		{
			fg_write_word(sm, FG_REG_RS_2, data);
			pr_info("mode change to RS m mode 0x%x\n", data);
		}
	}
	else
	{
		if((sm->p_batt_voltage < sm->n_tem_poff) &&
			(sm->batt_volt < sm->n_tem_poff) && (!sm->is_charging))
		{
			if((sm->p_batt_voltage <
				(sm->n_tem_poff - sm->n_tem_poff_offset)) &&
				(sm->batt_volt <
				(sm->n_tem_poff - sm->n_tem_poff_offset)))
			{
				fg_write_word(sm, FG_REG_RS_2, data>>1);
				pr_info("mode change to normal tem RS m mode 0x%x\n", data>>1);
			}
			else
			{
				fg_write_word(sm, FG_REG_RS_2, data);
				pr_info("mode change to normal tem RS m mode 0x%x\n", data);
			}
		}
		else
		{
			pr_info("mode change to RS a mode\n");

			fg_write_word(sm, FG_REG_RS_2, sm->rs_value[2]);
		}
	}
	sm->p_batt_voltage = sm->batt_volt;
	sm->p_batt_current = sm->batt_curr;
	// iocv error case cover end
}


static int fg_cal_carc (struct sm_fg_chip *sm)
{
	int curr_cal = 0, p_curr_cal=0, n_curr_cal=0, p_delta_cal=0, n_delta_cal=0, p_fg_delta_cal=0, n_fg_delta_cal=0, temp_curr_offset=0;
	int temp_gap, fg_temp_gap = 0;
	int ret = 0;
	u16 data[10] = {0,};

	sm->is_charging = is_battery_charging(sm); //From Charger Driver

#ifdef ENABLE_TEMBASE_ZDSCON
	fg_tembase_zdscon(sm);
#endif
	fg_vbatocv_check(sm);

	//fg_temp_gap = (sm->batt_temp/10) - sm->temp_std;
	fg_temp_gap = sm->batt_temp - sm->temp_std;
	
	temp_curr_offset = sm->curr_offset;
	if(sm->en_high_fg_temp_offset && (fg_temp_gap > 0))
	{
		if(temp_curr_offset & 0x0080)
		{
			temp_curr_offset = -(temp_curr_offset & 0x007F);
		}
		temp_curr_offset = temp_curr_offset + (fg_temp_gap / sm->high_fg_temp_offset_denom)*sm->high_fg_temp_offset_fact;
		if(temp_curr_offset < 0)
		{
			temp_curr_offset = -temp_curr_offset;
			temp_curr_offset = temp_curr_offset|0x0080;
		}
	}
	else if (sm->en_low_fg_temp_offset && (fg_temp_gap < 0))
	{
		if(temp_curr_offset & 0x0080)
		{
			temp_curr_offset = -(temp_curr_offset & 0x007F);
		}
		temp_curr_offset = temp_curr_offset + ((-fg_temp_gap) / sm->low_fg_temp_offset_denom)*sm->low_fg_temp_offset_fact;
		if(temp_curr_offset < 0)
		{
			temp_curr_offset = -temp_curr_offset;
			temp_curr_offset = temp_curr_offset|0x0080;
		}
	}

#ifdef ENABLE_FIT_ZR_NCUR
	if (sm->batt_soc < FIT_ZR_NCUR_SOC && sm->batt_volt < FIT_ZR_NCUR_VOL)
	{
		if(temp_curr_offset & 0x0080)
		{
			temp_curr_offset = -(temp_curr_offset & 0x007F);
		}
		temp_curr_offset = temp_curr_offset - FIT_ZR_NCUR_VALUE;
		if(temp_curr_offset < 0)
		{
			temp_curr_offset = -temp_curr_offset;
			temp_curr_offset = temp_curr_offset|0x0080;
		}
	}
#endif
    temp_curr_offset = temp_curr_offset | (temp_curr_offset<<8);

	ret = fg_write_word(sm, FG_REG_CURR_IN_OFFSET, temp_curr_offset);
	if (ret < 0) {
		pr_err("Failed to write CURR_IN_OFFSET, ret = %d\n", ret);
		return ret;
	} else {
		pr_err("CURR_IN_OFFSET [0x%x] = 0x%x\n", FG_REG_CURR_IN_OFFSET, temp_curr_offset);
	}

	n_curr_cal = (sm->curr_slope & 0xFF00)>>8;
	p_curr_cal = (sm->curr_slope & 0x00FF);
	
	if (sm->en_high_fg_temp_cal && (fg_temp_gap > 0))
	{
		p_fg_delta_cal = (fg_temp_gap / sm->high_fg_temp_p_cal_denom)*sm->high_fg_temp_p_cal_fact;
		n_fg_delta_cal = (fg_temp_gap / sm->high_fg_temp_n_cal_denom)*sm->high_fg_temp_n_cal_fact;
	}
	else if (sm->en_low_fg_temp_cal && (fg_temp_gap < 0))
	{
		fg_temp_gap = -fg_temp_gap;
		p_fg_delta_cal = (fg_temp_gap / sm->low_fg_temp_p_cal_denom)*sm->low_fg_temp_p_cal_fact;
		n_fg_delta_cal = (fg_temp_gap / sm->low_fg_temp_n_cal_denom)*sm->low_fg_temp_n_cal_fact;
	}
	p_curr_cal = p_curr_cal + (p_fg_delta_cal);
	n_curr_cal = n_curr_cal + (n_fg_delta_cal);

	pr_info("<%d %d %d %d %d %d %d %d %d %d>, temp_fg = %d ,p_curr_cal = 0x%x, n_curr_cal = 0x%x, "
		"batt_temp = %d\n",	
		sm->en_high_fg_temp_cal,
		sm->high_fg_temp_p_cal_denom, sm->high_fg_temp_p_cal_fact, 
		sm->high_fg_temp_n_cal_denom, sm->high_fg_temp_n_cal_fact,
		sm->en_low_fg_temp_cal, 
		sm->low_fg_temp_p_cal_denom, sm->low_fg_temp_p_cal_fact, 
		sm->low_fg_temp_n_cal_denom, sm->low_fg_temp_n_cal_fact,
		sm->batt_temp, p_curr_cal, n_curr_cal, sm->batt_temp);

	//temp_gap = (sm->batt_temp/10) - sm->temp_std;
	temp_gap = sm->batt_temp - sm->temp_std;	
	if (sm->en_high_temp_cal && (temp_gap > 0))
	{
		p_delta_cal = (temp_gap / sm->high_temp_p_cal_denom)*sm->high_temp_p_cal_fact;
		n_delta_cal = (temp_gap / sm->high_temp_n_cal_denom)*sm->high_temp_n_cal_fact;
	}
	else if (sm->en_low_temp_cal && (temp_gap < 0))
	{
		temp_gap = -temp_gap;
		p_delta_cal = (temp_gap / sm->low_temp_p_cal_denom)*sm->low_temp_p_cal_fact;
		n_delta_cal = (temp_gap / sm->low_temp_n_cal_denom)*sm->low_temp_n_cal_fact;
	}
	p_curr_cal = p_curr_cal + (p_delta_cal);
	n_curr_cal = n_curr_cal + (n_delta_cal);

#ifdef ENABLE_HCIC_ACT
	if (abs(sm->batt_curr) > HCIC_MIN) {
		if (HCIC_DENOM == 0) {
			p_delta_cal = HCIC_FACTOR;
		} else {
			p_delta_cal = ((abs(sm->batt_curr) - HCIC_MIN) / HCIC_DENOM) * HCIC_FACTOR;
		}
		p_curr_cal = p_curr_cal + (p_delta_cal);
		n_curr_cal = n_curr_cal + (p_delta_cal);
		pr_info("%s: HCIC_ACT i_%d, p_curr_cal = 0x%x, n_curr_cal = 0x%x\n", __func__, sm->batt_curr, p_curr_cal, n_curr_cal);
	}
#endif

    curr_cal = (n_curr_cal << 8) | p_curr_cal;

	ret = fg_write_word(sm, FG_REG_CURR_IN_SLOPE, curr_cal);
	if (ret < 0) {
		pr_err("Failed to write CURR_IN_SLOPE, ret = %d\n", ret);
		return ret;
	} else {
		pr_err("write CURR_IN_SLOPE [0x%x] = 0x%x\n", FG_REG_CURR_IN_SLOPE, curr_cal);
	}

	pr_info("<%d %d %d %d %d %d %d %d %d %d>, "
		"p_curr_cal = 0x%x, n_curr_cal = 0x%x, curr_cal = 0x%x\n",	
		sm->en_high_temp_cal,
		sm->high_temp_p_cal_denom, sm->high_temp_p_cal_fact, 
		sm->high_temp_n_cal_denom, sm->high_temp_n_cal_fact,
		sm->en_low_temp_cal, 
		sm->low_temp_p_cal_denom, sm->low_temp_p_cal_fact, 
		sm->low_temp_n_cal_denom, sm->low_temp_n_cal_fact,
		p_curr_cal, n_curr_cal, curr_cal);

	ret = fg_write_word(sm, sm->regs[SM_FG_REG_VOL_COMP], sm->rpara);
	if (ret < 0) {
		pr_err("Failed to write SM_FG_REG_VOL_COMP, ret = %d\n", ret);
		return ret;
	} else {
		pr_err("write SM_FG_REG_VOL_COMP [0x%x] = 0x%x\n", sm->regs[SM_FG_REG_VOL_COMP], sm->rpara);
	}

	ret = fg_write_word(sm, sm->regs[SM_FG_REG_CURR_OFFSET], sm->curr_voffset);
	if (ret < 0) {
		pr_err("Failed to write SM_FG_REG_CURR_OFFSET, ret = %d\n", ret);
		return ret;
	} else {
		pr_err("write SM_FG_REG_CURR_OFFSET [0x%x] = 0x%x\n", FG_REG_CURR_IN_SLOPE, sm->curr_voffset);
	}

	ret = fg_write_word(sm, sm->regs[SM_FG_REG_CURR_SLOPE], sm->curr_vslope);
	if (ret < 0) {
		pr_err("Failed to write SM_FG_REG_CURR_SLOPE, ret = %d\n", ret);
		return ret;
	} else {
		pr_err("write SM_FG_REG_CURR_SLOPE [0x%x] = 0x%x\n", SM_FG_REG_CURR_SLOPE, sm->curr_vslope);
	}

	ret |= fg_read_word(sm, 0x06, &data[0]);
	ret |= fg_read_word(sm, 0x1E, &data[1]);
	ret |= fg_read_word(sm, 0x28, &data[2]);
	ret |= fg_read_word(sm, 0x80, &data[3]);	
	ret |= fg_read_word(sm, 0x83, &data[4]);
	ret |= fg_read_word(sm, 0x84, &data[5]);
	ret |= fg_read_word(sm, 0x86, &data[6]);
	ret |= fg_read_word(sm, 0x87, &data[7]);
	ret |= fg_read_word(sm, 0x93, &data[8]);
	ret |= fg_read_word(sm, 0x9C, &data[9]);	
	if (ret < 0) {
		pr_err("could not read , ret = %d\n", ret);
		return ret;
	} else
		pr_info("0x06=0x%x, 0x1E=0x%x, 0x28=0x%x, 0x80=0x%x, 0x83=0x%x, 0x84=0x%x, 0x86=0x%x, 0x87=0x%x, 0x93=0x%x, 0x9C=0x%x\n",
			data[0], data[1], data[2], data[3], data[4], data[5], data[6], data[7], data[8], data[9]);

	return 1;
}

static int fg_get_batt_status(struct sm_fg_chip *sm)
{
	if (!sm->batt_present)
		return POWER_SUPPLY_STATUS_UNKNOWN;
	//else if (sm->batt_fc)
	//	return POWER_SUPPLY_STATUS_FULL;
	else if (sm->batt_dsg)
		return POWER_SUPPLY_STATUS_DISCHARGING;
	else if (sm->batt_curr > 0)
		return POWER_SUPPLY_STATUS_CHARGING;
	else
		return POWER_SUPPLY_STATUS_NOT_CHARGING;

}


static int fg_get_batt_capacity_level(struct sm_fg_chip *sm)
{
	if (!sm->batt_present)
		return POWER_SUPPLY_CAPACITY_LEVEL_UNKNOWN;
	else if (sm->batt_fc)
		return POWER_SUPPLY_CAPACITY_LEVEL_FULL;
	else if (sm->batt_soc1)
		return POWER_SUPPLY_CAPACITY_LEVEL_LOW;
	else if (sm->batt_socp)
		return POWER_SUPPLY_CAPACITY_LEVEL_CRITICAL;
	else
		return POWER_SUPPLY_CAPACITY_LEVEL_NORMAL;

}

static int fg_get_batt_health(struct sm_fg_chip *sm)
{
	if (!sm->batt_present)
		return POWER_SUPPLY_HEALTH_UNKNOWN;
#ifdef ENABLE_NTC_COMPENSATION_1
	else if (sm->batt_temp >= OVERHEAT_TH_DEG)
		return POWER_SUPPLY_HEALTH_OVERHEAT;
	else if (sm->batt_temp <= COLD_TH_DEG)
		return POWER_SUPPLY_HEALTH_COLD;
#else
	else if (sm->batt_ot)
		return POWER_SUPPLY_HEALTH_OVERHEAT;
	else if (sm->batt_ut)
		return POWER_SUPPLY_HEALTH_COLD;
#endif
	else
		return POWER_SUPPLY_HEALTH_GOOD;
}

static enum power_supply_property fg_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_CAPACITY_LEVEL,
	POWER_SUPPLY_PROP_TEMP,
	POWER_SUPPLY_PROP_CHARGE_FULL,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_TECHNOLOGY,
	POWER_SUPPLY_PROP_CHARGE_COUNTER,
	POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN,
	POWER_SUPPLY_PROP_MODEL_NAME,
	POWER_SUPPLY_PROP_TIME_TO_FULL_NOW,
};

static void fg_monitor_workfunc(struct work_struct *work);


enum {
	PLUGIN_CHARGING_TO_FULL = 0,
	PLUGIN_FULL,
};

static int Get_ChargerProp_To_ChangeSoc(struct sm_fg_chip *sm, union power_supply_propval *soc_val)
{
	int ret = 0;
	union power_supply_propval online_val;
	union power_supply_propval status_val;

	if(sm->bbc_psy == NULL) {
		sm->bbc_psy = power_supply_get_by_name("bbc");
		if (sm->bbc_psy == NULL) {
			pr_err("%s : fail to get psy ac\n", __func__);
			return -ENODEV;
		}
	}

	ret = power_supply_get_property(sm->bbc_psy, POWER_SUPPLY_PROP_ONLINE, &online_val);
	ret = power_supply_get_property(sm->bbc_psy, POWER_SUPPLY_PROP_STATUS, &status_val);
	if (ret < 0) {
		pr_err("%s : set ac psy type prop fail ret:%d\n", __func__, ret);
		return ret;
	}
	if(!online_val.intval)
	{
		sm->bbc_status = PLUGIN_CHARGING_TO_FULL;
	} else{
		if(status_val.intval != POWER_SUPPLY_STATUS_FULL)
			sm->bbc_status = PLUGIN_CHARGING_TO_FULL;

		if((sm->bbc_status == PLUGIN_CHARGING_TO_FULL)
			&& (status_val.intval == POWER_SUPPLY_STATUS_FULL)
			&& (soc_val->intval == 100))
			sm->bbc_status = PLUGIN_FULL;

		if(sm->bbc_status == PLUGIN_FULL)
			soc_val->intval = 100;
	}
	pr_err("%s soc_val=%d,bbc_status:%d\n", __func__, soc_val->intval, sm->bbc_status);
	return ret;
}

static int fg_get_property(struct power_supply *psy, enum power_supply_property psp,
					union power_supply_propval *val)
{
	struct sm_fg_chip *sm = power_supply_get_drvdata(psy);
	int ret;
	static int old_soc;
#ifdef SHUTDOWN_DELAY	
	int vbat_mv;
	static bool last_shutdown_delay;
#endif
	/* pr_debug("fg_get_property\n"); */

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		val->intval = fg_get_batt_status(sm);
/*		pr_info("fg POWER_SUPPLY_PROP_STATUS:%d\n", val->intval); */
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		ret = fg_read_volt(sm);
		mutex_lock(&sm->data_lock);
		if (ret >= 0)
			sm->batt_volt = ret;
		val->intval = sm->batt_volt * 1000; //uV
		mutex_unlock(&sm->data_lock);
		break;
		
	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = sm->batt_present;
		break;
		
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		mutex_lock(&sm->data_lock);
		sm->batt_curr = fg_read_current(sm);
		val->intval = sm->batt_curr * 1000; //uA
		//val->intval = -val->intval;
		mutex_unlock(&sm->data_lock);
		break;

	case POWER_SUPPLY_PROP_CAPACITY:
		if (sm->fake_soc >= 0) {
			val->intval = sm->fake_soc;
			break;
		}
		//ret = fg_read_soc(sm);
		ret = fg_read_ui_soc(sm);
		mutex_lock(&sm->data_lock);
		if((1 == sm->fg_reset_flag) && (ret != 0))
		{
			pr_err("%s fg_reset_flag=%d,old_soc:%d,ret = %d\n", __func__, sm->fg_reset_flag, old_soc,ret);
			old_soc = ret;
		}	
		if(((0 == sm->fg_reset_flag) || (ret == 0)))
		{
			pr_err("%s fg_reset_flag=%d,old_soc:%d,ret = %d\n", __func__, sm->fg_reset_flag, old_soc,ret);
			ret = old_soc ;
		}
#ifdef SOC_SMOOTH_TRACKING		
		if (ret >= 0)
			sm->batt_soc = ret;
		if (sm->param.batt_soc >= 0)
			val->intval = sm->param.batt_soc/10;
		else if ((ret >= 0) && (sm->param.batt_soc == -EINVAL)) {
#ifdef ENABLE_MAP_SOC
			val->intval = (((sm->batt_soc*10+MAP_MAX_SOC)*100/MAP_RATE_SOC)-MAP_MIN_SOC)/10;
#else
			val->intval = sm->batt_soc/10;
#endif
		} else
			val->intval = sm->batt_soc/10;
#else
		if (ret >= 0)
			sm->batt_ui_soc = ret;
		if (ret >= 0) {
#ifdef ENABLE_MAP_SOC
			val->intval = (((sm->batt_soc*10+MAP_MAX_SOC)*100/MAP_RATE_SOC)-MAP_MIN_SOC)/10;
#else
			val->intval = sm->batt_ui_soc/10;
#endif
		} else
			val->intval = sm->batt_ui_soc/10;
#endif

		sm->is_charging = is_battery_charging(sm); //From Charger Driver
#ifdef ENABLE_WAIT_SOC_FULL
		if ((sm->p_report_soc < 100) && (val->intval > 99)) {
			if ((abs((sm->batt_curr + sm->p_batt_current)/2) > (sm->top_off+sm->topoff_margin)) && (sm->is_charging)) {
				val->intval = val->intval - WAIT_SOC_GAP;
				pr_info("soc wait for full chg - %d %d %d %d %d\n",sm->p_batt_current, sm->batt_curr, sm->top_off, sm->topoff_margin, sm->is_charging);
			}
		}
#endif

		pr_info("sm->batt_ui_soc: %d, val->intval: %d\n", sm->batt_ui_soc, val->intval);
		mutex_unlock(&sm->data_lock);
#ifdef SHUTDOWN_DELAY		
		if (sm->shutdown_delay_enable) {
			//Under 1%
			if (val->intval == 0) {
				vbat_mv = fg_read_volt(sm);
				pr_info("vbat_mv: %d, is_charging: %d, shutdown_delay: %d",
					vbat_mv, sm->is_charging, sm->shutdown_delay);
				if (sm->is_charging && sm->shutdown_delay) {
					sm->shutdown_delay = false;
					val->intval = 1;
				} else {
					if (vbat_mv > SHUTDOWN_DELAY_H_VOL) {
						val->intval = 1;
					} else if (vbat_mv > SHUTDOWN_DELAY_L_VOL) {
						if (!sm->is_charging) {
							sm->shutdown_delay = true;							
						}
						val->intval = 1;
					} else {
						sm->shutdown_delay = false;						
					}
				}
			} else {
				sm->shutdown_delay = false;				
			}
#ifdef DELAY_SOC_ZERO
			if (val->intval == 0) {
				if (sm->en_delay_soc_zero == 0) {
					if (sm->recover_delay_soc_zero == 1) {
						sm->en_delay_soc_zero = 1;
						sm->recover_delay_soc_zero = 0;
						cancel_delayed_work_sync(&sm->delay_soc_zero_work);
						schedule_delayed_work(&sm->delay_soc_zero_work, msecs_to_jiffies(DELAY_SOC_ZERO_TIME_30000MS)); /* 30 sec */
						val->intval = 1;
					}
					// Case 'sm->en_delay_soc_zero == 0'&& 'sm->recover_delay_soc_zero == 0' is '0'
				} else if (sm->en_delay_soc_zero == 1) {
					pr_info("DELAY SOC Zero Working sm->en_delay_soc_zero (%d), sm->shutdown_delay (%d), sm->recover_delay_soc_zero (%d)",
					sm->en_delay_soc_zero, sm->shutdown_delay, sm->recover_delay_soc_zero);
					val->intval = 1;
				}
			} else if (val->intval > 0) {
					sm->en_delay_soc_zero = 0;
					sm->recover_delay_soc_zero = 1;
					cancel_delayed_work_sync(&sm->delay_soc_zero_work);
					pr_info("DELAY SOC Zero Stoped sm->en_delay_soc_zero (%d), sm->shutdown_delay (%d) sm->recover_delay_soc_zero (%d)",
					sm->en_delay_soc_zero, sm->shutdown_delay, sm->recover_delay_soc_zero);
			}
#endif			
			if (last_shutdown_delay != sm->shutdown_delay) {
				pr_info("last_shutdown_delay (%d), sm->shutdown_delay (%d)",
					last_shutdown_delay, sm->shutdown_delay);
				last_shutdown_delay = sm->shutdown_delay;
				if (sm->batt_psy) {
					power_supply_changed(sm->batt_psy);
				}
				if (sm->fg_psy) {
					power_supply_changed(sm->fg_psy);
				}
			}
		}		
#endif

		sm->p_report_soc = val->intval;
		Get_ChargerProp_To_ChangeSoc(sm, val);
		break;

	case POWER_SUPPLY_PROP_CAPACITY_LEVEL:
		val->intval = fg_get_batt_capacity_level(sm);
		break;

	case POWER_SUPPLY_PROP_TEMP:
		if (sm->fake_temp != -EINVAL) {
			val->intval = sm->fake_temp;
			break;
		}
		if (sm->en_temp_in)
			ret = fg_read_temperature(sm, TEMPERATURE_IN);
		else if (sm->en_temp_ex)
			ret = fg_read_temperature(sm, TEMPERATURE_EX);
		else if (sm->en_temp_3rd)
			ret = fg_read_temperature(sm, TEMPERATURE_3RD);		
		else 
			ret = -ENODATA;
		mutex_lock(&sm->data_lock);
		if (ret > 0)
			sm->batt_temp = ret;
#ifdef ENABLE_TEMP_AVG	
		if (sm->temp_param.batt_temp >= EX_TEMP_MIN && sm->temp_param.batt_temp <= EX_TEMP_MAX)
			val->intval = sm->temp_param.batt_temp_avg * 10; //1.0degree = 10
		else if (sm->temp_param.batt_temp == -EINVAL)
			val->intval = sm->batt_temp * 10; //1.0degree = 10		
		else
			val->intval = 25 * 10;
#else
		val->intval = sm->batt_temp * 10; //1.0degree = 10		
#endif
		mutex_unlock(&sm->data_lock);
		break;
	case POWER_SUPPLY_PROP_TIME_TO_FULL_NOW:
		if (get_battery_status(sm) == POWER_SUPPLY_STATUS_FULL) {
			val->intval = 0;
		} else if (sm->batt_curr > 0){
			if ((sm->batt_fcc > 0) &&( sm->batt_rmc > 0)) {
				ret = sm->batt_fcc - sm->batt_rmc;
				if (ret > 0)
					val->intval =  ret * 3600 / sm->batt_curr;
				else
					val->intval = 0;
			} else {
				val->intval = -1;
			}
		} else {
			val->intval = -1;
		}
		break;
	case POWER_SUPPLY_PROP_CHARGE_FULL:
		ret = fg_read_fcc(sm);
		mutex_lock(&sm->data_lock);
		if (ret > 0)
			sm->batt_fcc = ret;
		val->intval = sm->batt_fcc * 1000; //uAh
		mutex_unlock(&sm->data_lock);
		break;
	case POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN:
		val->intval = fg_read_fcc(sm) * 1000; //uAh
		//val->intval = 5000 * 1000; //uAh : Fixed 5000mAh
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		val->intval = fg_get_batt_health(sm);
		break;

	case POWER_SUPPLY_PROP_TECHNOLOGY:
		val->intval = POWER_SUPPLY_TECHNOLOGY_LION;
		break;

	case POWER_SUPPLY_PROP_CYCLE_COUNT:
		val->intval = sm->batt_soc_cycle;
		break;

	case POWER_SUPPLY_PROP_CHARGE_COUNTER:
		ret = fg_read_rmc(sm);
		if (ret >= 0)
			sm->batt_rmc = ret;
		else
			sm->batt_rmc = 2500 * 1000; //uAh : Fixed 2500mAh
		val->intval =  sm->batt_rmc * 1000;
		break;
	case POWER_SUPPLY_PROP_MODEL_NAME:
		val->strval = sm5602_driver_name;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int fg_set_property(struct power_supply *psy,
				       enum power_supply_property prop,
				       const union power_supply_propval *val)
{
	struct sm_fg_chip *sm = power_supply_get_drvdata(psy);

	switch (prop) {
	case POWER_SUPPLY_PROP_TEMP:
		sm->fake_temp = val->intval;
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
		sm->fake_soc = val->intval;
		power_supply_changed(sm->fg_psy);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int fg_prop_is_writeable(struct power_supply *psy,
				       enum power_supply_property prop)
{
	int ret;

	switch (prop) {
	case POWER_SUPPLY_PROP_TEMP:
	case POWER_SUPPLY_PROP_CAPACITY:
		ret = 1;
		break;
	default:
		ret = 0;
		break;
	}
	return ret;
}

static void fg_external_power_changed(struct power_supply *psy)
{
	struct sm_fg_chip *sm = power_supply_get_drvdata(psy);

	cancel_delayed_work(&sm->monitor_work);
	schedule_delayed_work(&sm->monitor_work, 0);
}

static char *sm5602_fg_supplied_to[] = {
	"battery",
};

static const struct power_supply_desc fg_power_supply_desc = {
	.name			= "bms",
	.type			= POWER_SUPPLY_TYPE_MAINS, /* POWER_SUPPLY_TYPE_BMS(kernel:4.19)  -> POWER_SUPPLY_TYPE_MAINS(kernel:5.4) */
	.get_property	= fg_get_property,
	.set_property	= fg_set_property,
	.properties		= fg_props,
	.num_properties	= ARRAY_SIZE(fg_props),
	.external_power_changed = fg_external_power_changed,
	.property_is_writeable = fg_prop_is_writeable,
};

static int fg_psy_register(struct sm_fg_chip *sm)
{
	struct power_supply_config psy_cfg = {};

	psy_cfg.drv_data = sm;
	psy_cfg.supplied_to = sm5602_fg_supplied_to;
	psy_cfg.num_supplicants = ARRAY_SIZE(sm5602_fg_supplied_to);	

	sm->fg_psy = power_supply_register(sm->dev, &fg_power_supply_desc, &psy_cfg);
	if (!sm->fg_psy) {
		pr_err("Failed to register fg_psy\n");
		return (-1);
	}

	return 0;
}

static void fg_psy_unregister(struct sm_fg_chip *sm)
{
	power_supply_unregister(sm->fg_psy);
}

static const u8 fg_dump_regs[] = {
	0x00, 0x01, 0x03, 0x04,
	0x05, 0x06, 0x07, 0x08,
	0x09, 0x0A, 0x0C, 0x0D,
	0x0E, 0x0F, 0x10, 0x11,
	0x12, 0x13, 0x14, 0x1A, 
	0x1B, 0x1C, 0x1E, 0x1f,
	0x62, 0x73, 0x74, 0x80,
	0x90, 0x91,	0x95, 0x96
};

static int fg_dump_debug(struct sm_fg_chip *sm)
{
	int i;
	int ret;
	u16 val = 0;

	for (i = 0; i < ARRAY_SIZE(fg_dump_regs); i++) {
		ret = fg_read_word(sm, fg_dump_regs[i], &val);
		if (!ret)
			pr_info("Reg[0x%02X] = 0x%02X\n",
						fg_dump_regs[i], val);
	}
	return 0;
}


static int reg_debugfs_open(struct inode *inode, struct file *file)
{
	struct sm_fg_chip *sm = inode->i_private;

	return single_open(file, show_registers, sm);
}

static const struct file_operations reg_debugfs_ops = {
	.owner		= THIS_MODULE,
	.open		= reg_debugfs_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static void create_debugfs_entry(struct sm_fg_chip *sm)
{
	sm->debug_root = debugfs_create_dir("sm_fg", NULL);
	if (!sm->debug_root)
		pr_err("Failed to create debug dir\n");

	if (sm->debug_root) {

		debugfs_create_file("registers", S_IFREG | S_IRUGO,
						sm->debug_root, sm, &reg_debugfs_ops);

		debugfs_create_x32("fake_soc",
					  S_IFREG | S_IWUSR | S_IRUGO,
					  sm->debug_root,
					  &(sm->fake_soc));

		debugfs_create_x32("fake_temp",
					  S_IFREG | S_IWUSR | S_IRUGO,
					  sm->debug_root,
					  &(sm->fake_temp));

		debugfs_create_x32("skip_reads",
					  S_IFREG | S_IWUSR | S_IRUGO,
					  sm->debug_root,
					  &(sm->skip_reads));
		debugfs_create_x32("skip_writes",
					  S_IFREG | S_IWUSR | S_IRUGO,
					  sm->debug_root,
					  &(sm->skip_writes));
	}
}

static int show_registers(struct seq_file *m, void *data)
{
	struct sm_fg_chip *sm = m->private;
	int i;
	int ret;
	u16 val = 0;

	for (i = 0; i < ARRAY_SIZE(fg_dump_regs); i++) {
		ret = fg_read_word(sm, fg_dump_regs[i], &val);
		if (!ret)
			seq_printf(m, "Reg[0x%02X] = 0x%02X\n",
						fg_dump_regs[i], val);
	}
	return 0;
}

#if defined(SOC_SMOOTH_TRACKING) || defined(ENABLE_TEMP_AVG)
/*
static inline void get_monotonic_boottime(struct timespec *ts)
{
        *ts = ktime_to_timespec(ktime_get_boottime());
}
*/


static int calculate_delta_time(ktime_t time_stamp, int *delta_time_s)
{
	ktime_t now_time;

	now_time = ktime_get();
	*delta_time_s = ktime_ms_delta(now_time, time_stamp);
	if (*delta_time_s < 0)
		*delta_time_s = 0;

	// bq_dbg(PR_DEBUG,  "now:%ld, last:%ld, delta:%d\n", time_now, time_last, *delta_time);
	return 0;
}
#endif

#ifdef SOC_SMOOTH_TRACKING
static void calculate_average_current(struct sm_fg_chip *sm)
{
	int i;
	int iavg_ma = sm->param.batt_ma;

	/* only continue if ibat has changed */
	if (sm->param.batt_ma == sm->param.batt_ma_prev)
		goto unchanged;
	else
		sm->param.batt_ma_prev = sm->param.batt_ma;

	sm->param.batt_ma_avg_samples[sm->param.samples_index] = iavg_ma;
	sm->param.samples_index = (sm->param.samples_index + 1) % BATT_MA_AVG_SAMPLES;
	sm->param.samples_num++;

	if (sm->param.samples_num >= BATT_MA_AVG_SAMPLES)
		sm->param.samples_num = BATT_MA_AVG_SAMPLES;

	if (sm->param.samples_num) {
		iavg_ma = 0;
		/* maintain a AVG_SAMPLES sample average of ibat */
		for (i = 0; i < sm->param.samples_num; i++) {
			pr_debug("iavg_samples_ma[%d] = %d\n", i, sm->param.batt_ma_avg_samples[i]);
			iavg_ma += sm->param.batt_ma_avg_samples[i];
		}
		sm->param.batt_ma_avg = DIV_ROUND_CLOSEST(iavg_ma, sm->param.samples_num);
	}

unchanged:
	pr_info("current_now_ma = %d, averaged_iavg_ma = %d\n",
			sm->param.batt_ma, sm->param.batt_ma_avg);
}

#define LOW_TBAT_THRESHOLD			15//Degree
#define CHANGE_SOC_TIME_LIMIT_10S	10
#define CHANGE_SOC_TIME_LIMIT_20S	20
#define CHANGE_SOC_TIME_LIMIT_30S	30
#define CHANGE_SOC_TIME_LIMIT_40S	40
#define CHANGE_SOC_TIME_LIMIT_50S	50
#define CHANGE_SOC_TIME_LIMIT_60S	60
#define HEAVY_DISCHARGE_CURRENT		(-1000)//mA //1000
#define FORCE_TO_FULL_SOC			970 //SOC 97.0 = 970
#define MIN_DISCHARGE_CURRENT		(-25)//-25 //mA
#define MIN_CHARGING_CURRENT		10//(10) //mA
#define FULL_SOC					1000 //SOC 100.0 = 1000
#define SOC_DISCHARGING_GAP			20 //SOC 2.0 = 20
static void fg_soc_smooth_tracking(struct sm_fg_chip *sm)
{
	int delta_time = 0;
	int soc_changed;
	int last_batt_soc = sm->param.batt_soc;
	int time_since_last_change_sec;
	static ktime_t last_change_time;

	sm->charge_full = is_battery_full(sm);

	calculate_delta_time(last_change_time, &time_since_last_change_sec);
	last_change_time = ktime_get();
	if (sm->param.batt_temp > LOW_TBAT_THRESHOLD) {
		/* Battery in normal temperture */
		/* Discharging */
		if (sm->param.batt_ma < 0 || abs(sm->param.batt_raw_soc - sm->param.batt_soc) > SOC_DISCHARGING_GAP) //20 = 2%
			delta_time = time_since_last_change_sec / CHANGE_SOC_TIME_LIMIT_20S;
		else
			/* Charging */
			delta_time = time_since_last_change_sec / CHANGE_SOC_TIME_LIMIT_60S;
	} else {
		/* Battery in low temperture */
		calculate_average_current(sm);
		if (sm->param.batt_ma_avg < HEAVY_DISCHARGE_CURRENT || abs(sm->param.batt_raw_soc - sm->param.batt_soc > SOC_DISCHARGING_GAP)) //20 = 2%
			/* Heavy loading current, ignore battery soc limit*/
			delta_time = time_since_last_change_sec / CHANGE_SOC_TIME_LIMIT_10S;
		else
			/* Heavy loading current, battery soc limit*/
			delta_time = time_since_last_change_sec / CHANGE_SOC_TIME_LIMIT_20S;
	}

	if (delta_time < 0)
		delta_time = 0;

	soc_changed = min(1, delta_time);
	soc_changed = soc_changed *10; //SOC Changed rate 0.x~1.0%

	pr_info("soc:%d, last_soc:%d, raw_soc:%d, update_now:%d, batt_ma:%d, time_since_last_change_sec:%d, soc_changed:%d, sm->charge_full:%d\n",
			sm->param.batt_soc, last_batt_soc, sm->param.batt_raw_soc, sm->param.update_now,
			sm->param.batt_ma, time_since_last_change_sec,soc_changed, sm->charge_full);

	if (last_batt_soc >= 0) {
		if (last_batt_soc != FULL_SOC && sm->param.batt_raw_soc >= FORCE_TO_FULL_SOC && sm->charge_full)
			/* Unlikely status */
			last_batt_soc = sm->param.update_now ? FULL_SOC : last_batt_soc + soc_changed;
		else if (last_batt_soc < sm->param.batt_raw_soc && MIN_CHARGING_CURRENT < sm->param.batt_ma)
			/* Battery in charging status */
			last_batt_soc = sm->param.update_now ? sm->param.batt_raw_soc : last_batt_soc + soc_changed;
		else if (last_batt_soc > sm->param.batt_raw_soc && MIN_DISCHARGE_CURRENT > sm->param.batt_ma)
			/* Battery in discharging status */
			last_batt_soc = sm->param.update_now ? sm->param.batt_raw_soc : last_batt_soc - soc_changed;

		sm->param.update_now = false;
	} else {
		last_batt_soc = sm->param.batt_raw_soc;
	}

	if (last_batt_soc > FULL_SOC)
		last_batt_soc = FULL_SOC;
	else if (last_batt_soc < 0)
		last_batt_soc = 0;

	if (sm->param.batt_soc != last_batt_soc) {
		sm->param.batt_soc = last_batt_soc;
	//	sm->param.last_soc_change_time = last_change_time;
		if (sm->batt_psy)
			power_supply_changed(sm->batt_psy);
	}
}

#if 0
#define MONITOR_SOC_WAIT_MS		1000
#define MONITOR_SOC_WAIT_PER_MS		10000
static void soc_monitor_work(struct work_struct *work)
{
	struct sm_fg_chip *sm = container_of(work,
				struct sm_fg_chip,
				soc_monitor_work.work);

	/* Update battery information */
	sm->param.batt_ma = fg_read_current(sm);

	if (sm->en_temp_in)
		sm->param.batt_temp = fg_read_temperature(sm, TEMPERATURE_IN);
	else if (sm->en_temp_ex)
		sm->param.batt_temp = fg_read_temperature(sm, TEMPERATURE_EX);
	else if (sm->en_temp_3rd)
		sm->param.batt_temp = fg_read_temperature(sm, TEMPERATURE_3RD);

	sm->param.batt_raw_soc = fg_read_soc(sm);

	if (sm->soc_reporting_ready)
		fg_soc_smooth_tracking(sm);

	pr_err("soc:%d, raw_soc:%d, c:%d, s:%d\n",
			sm->param.batt_soc, sm->param.batt_raw_soc,
			sm->param.batt_ma, sm->charge_status);

	schedule_delayed_work(&sm->soc_monitor_work, msecs_to_jiffies(MONITOR_SOC_WAIT_PER_MS));
}
#endif
#endif

#ifdef CHECK_ABNORMAL_TABLE
static void sm5602_table_error_check(struct sm_fg_chip *sm)
{
	bool err_flag = false;
	u16 data = 0, ocv = 0, soc = 0, qest = 0, soc_min = 0;
	int ret = 0;
  	if(!sm->table_check_flag){
		pr_err("sm->table_check_flag = %d\n", sm->table_check_flag);
		return;
	}

	{
		ret = fg_read_word(sm, 0x80, &data);
		ret |= fg_read_word(sm, 0x05, &soc);
		ret |= fg_read_word(sm, 0x06, &ocv);
		ret |= fg_read_word(sm, 0x84, &qest);
		if (ret < 0) {
			pr_err("Failed to read data, ret = 0x%x, data = 0x%x, soc = 0x%x, ocv = 0x%x, qest = 0x%x\n", ret, data, soc, ocv, qest);
			err_flag = false;
		}
		else {
			data = data & 0x001F;
			if (data != 0) {
				if ((ocv > (sm->battery_table[0][data-1] - TABLE_ERROR_OCV_MARGIN)) && (ocv < sm->battery_table[0][data] + TABLE_ERROR_OCV_MARGIN)) {
					if (data == 1) {
						soc_min = 0;
					}
					else {
						soc_min = sm->battery_table[1][data-1] - TABLE_ERROR_SOC_MARGIN;
					}
					if ((soc >= soc_min) && (soc < sm->battery_table[1][data] + TABLE_ERROR_SOC_MARGIN)) {
						err_flag = false;
					}
					else {
						err_flag = true;
						pr_info("err_true.. ref ocv = 0x%x, data = 0x%x, soc = 0x%x, ocv = 0x%x, qest = 0x%x\n", sm->battery_table[0][data], data, soc, ocv, qest);
					}
				}
				else {
					err_flag = true;
					pr_info("err_true.. ref soc = 0x%x, data = 0x%x, soc = 0x%x, ocv = 0x%x, qest = 0x%x\n", sm->battery_table[1][data], data, soc, ocv, qest);
				}

				if(!err_flag) {
					if (qest > (sm->cap + (sm->cap/100))) {
						pr_info("err_true.. ref soc = 0x%x, data = 0x%x, soc = 0x%x, ocv = 0x%x, qest = 0x%x\n", sm->battery_table[1][data], data, soc, ocv, qest);
						err_flag = true;
					}
				}
			}
			else {
				pr_info("state is zero, ref ocv = 0x%x, data = 0x%x, soc = 0x%x, ocv = 0x%x, qest = 0x%x\n", sm->battery_table[0][data], data, soc, ocv, qest);
			}
		}
	}

	if(err_flag) {
		pr_err("sm5602_table_error_check err_flag true,do reset\n", err_flag);
		fg_reset(sm);
		mutex_lock(&sm->data_lock);
		fg_reg_init(sm->client);
		mutex_unlock(&sm->data_lock);
		sm->fg_reset_flag = 1;
	}
}
#endif

static void fg_refresh_status(struct sm_fg_chip *sm)
{
	bool last_batt_inserted;
	bool last_batt_fc;
	bool last_batt_ot;
	bool last_batt_ut;	
	static int last_soc = -1, last_temp = -22;

	last_batt_inserted	= sm->batt_present;
	last_batt_fc		= sm->batt_fc;
	last_batt_ot		= sm->batt_ot;
	last_batt_ut		= sm->batt_ut;

	fg_read_status(sm);

	pr_info("batt_present=%d", sm->batt_present);

	if (!last_batt_inserted && sm->batt_present) {/* battery inserted */
		pr_info("Battery inserted\n");
	} else if (last_batt_inserted && !sm->batt_present) {/* battery removed */
		pr_info("Battery removed\n");
		sm->batt_soc	= -ENODATA;
		sm->batt_fcc	= -ENODATA;
		sm->batt_volt	= -ENODATA;
		sm->batt_curr	= -ENODATA;
		sm->batt_temp	= -ENODATA;
		sm->batt_rmc	= -ENODATA;
	}

	if (sm->batt_present) {	
		//last_soc = sm->batt_soc;
		//last_temp = sm->batt_temp;
	
		sm->batt_soc = fg_read_soc(sm);
		sm->batt_ui_soc = fg_read_ui_soc(sm);
		sm->batt_ocv = fg_read_ocv(sm);
		sm->batt_volt = fg_read_volt(sm);
		sm->batt_curr = fg_read_current(sm);
		sm->batt_soc_cycle = fg_get_cycle(sm);
		if (sm->en_temp_in)
			sm->batt_temp = fg_read_temperature(sm, TEMPERATURE_IN);
		else if (sm->en_temp_ex)
			sm->batt_temp = fg_read_temperature(sm, TEMPERATURE_EX);
		else if (sm->en_temp_3rd)
			sm->batt_temp = fg_read_temperature(sm, TEMPERATURE_3RD);		
		else 
			sm->batt_temp = -ENODATA;
		sm->batt_rmc = fg_read_rmc(sm);
		fg_cal_carc(sm);

		//if ((last_soc != sm->batt_soc)
		//		|| (last_temp != sm->batt_temp)) {
		//	pr_info("fg_psy changed\n");
		//	power_supply_changed(sm->fg_psy);
		//}
		
		pr_info("UISOC: %d,RSOC:%d, Volt:%d, Current:%d, Temperature:%d, OCV:%d, RMC:%d\n",
			sm->batt_ui_soc, sm->batt_soc, sm->batt_volt, sm->batt_curr, sm->batt_temp, sm->batt_ocv, sm->batt_rmc);
			
#ifdef SOC_SMOOTH_TRACKING		
		/* Update battery information */
		sm->param.batt_ma = sm->batt_curr;
#ifdef ENABLE_MAP_SOC
		sm->param.batt_raw_soc = (((sm->batt_soc*10+MAP_MAX_SOC)*100/MAP_RATE_SOC)-MAP_MIN_SOC);
		sm->param.batt_raw_soc = (sm->param.batt_raw_soc >=1000) ? 1000 : sm->param.batt_raw_soc;
#else
		sm->param.batt_raw_soc = sm->batt_soc;
#endif
		sm->soc_reporting_ready = 1;
		sm->param.batt_temp = sm->batt_temp;

		if (sm->soc_reporting_ready)
			fg_soc_smooth_tracking(sm);
#endif		
	}

#ifdef CHECK_ABNORMAL_TABLE
	sm5602_table_error_check(sm);
#endif

	if ((last_soc != sm->batt_ui_soc) || (last_temp != sm->batt_temp)) {
		if (sm->fg_psy)
			power_supply_changed(sm->fg_psy);
		if (sm->batt_psy == NULL)
			sm->batt_psy = power_supply_get_by_name("battery");
		if (sm->batt_psy)
			power_supply_changed(sm->batt_psy);

		last_soc = sm->batt_ui_soc;
		last_temp = sm->batt_temp;
	}
	//sm->last_update = jiffies;

#if defined (FEATURE_BATT_LONG_LIFE_CYCLE)
	if( (sm->batt_ui_soc >= 1000) && (sm->is_charging == 0))
	{	
		sm->ready_long_life_cv_voltage_check = 1;

		fg_get_batt_long_life_index_by_cv_voltage(sm);
		
		if(sm->q_max_now != sm->q_max_table[sm->cv_voltage_index])
		{
			pr_info("START RESET for change to FG Table\n");
			fg_reset(sm);
			mutex_lock(&sm->data_lock);
			fg_reg_init(sm->client);
			mutex_unlock(&sm->data_lock);
			sm->fg_reset_flag = 1;
		}
	}
	else
	{
		sm->ready_long_life_cv_voltage_check = 0;
	}
#endif
}

static unsigned int poll_interval = 10; /*  10 sec */
static void fg_monitor_workfunc(struct work_struct *work)
{
	struct sm_fg_chip *sm = container_of(work, struct sm_fg_chip,
								monitor_work.work);

	mutex_lock(&sm->data_lock);
	fg_init(sm->client);
	mutex_unlock(&sm->data_lock);

	fg_refresh_status(sm);

	if (poll_interval > 0) {
		schedule_delayed_work(&sm->monitor_work, msecs_to_jiffies(poll_interval * 1000)); /* poll_interval(10) * 1000 = 10 sec */
	}

}

#ifdef ENABLE_INIT_DELAY_TEMP
static void fg_init_delay_temp_workfunc(struct work_struct *work)
{
	struct sm_fg_chip *sm = container_of(work, struct sm_fg_chip,
								init_delay_temp_work.work);
	sm->en_init_delay_temp = 0;
	
	pr_info("Disable init delay temperatue work : en_init_delay_temp : [%x]\n", sm->en_init_delay_temp);
}
#endif

#ifdef DELAY_SOC_ZERO
static void fg_delay_soc_zero_workfunc(struct work_struct *work)
{
	struct sm_fg_chip *sm = container_of(work, struct sm_fg_chip,
								delay_soc_zero_work.work);
	sm->en_delay_soc_zero = 0;
	
	pr_info("Disable  delay soc zero work : en_delay_soc_zero : [%x]\n", sm->en_delay_soc_zero);
}
#endif

#define COMMON_PARAM_MASK		0xFF00
#define COMMON_PARAM_SHIFT		8
#define BATTERY_PARAM_MASK		0x00FF
static bool fg_check_reg_init_need(struct i2c_client *client)
{
	struct sm_fg_chip *sm = i2c_get_clientdata(client); 
	int ret = 0;
	u16 data = 0;
	u16 param_ver = 0;
	
	ret = fg_read_word(sm, sm->regs[SM_FG_REG_FG_OP_STATUS], &data);
	if (ret < 0) {
			pr_err("Failed to read param_ctrl unlock, ret = %d\n", ret);
			return ret;
	} else {
		pr_info("FG_OP_STATUS = 0x%x\n", data);	

		ret = fg_read_word(sm, FG_PARAM_VERION, &param_ver);
		if (ret < 0) {
				pr_err("Failed to read FG_PARAM_VERION, ret = %d\n", ret);
				return ret;
		} 

		pr_info("param_ver = 0x%x, common_param_version = 0x%x, battery_param_version = 0x%x\n", param_ver, sm->common_param_version, sm->battery_param_version);

		if ((((param_ver & COMMON_PARAM_MASK) >> COMMON_PARAM_SHIFT) >= sm->common_param_version) 
			&& ((param_ver & BATTERY_PARAM_MASK) >= sm->battery_param_version)) {

			if ((data & INIT_CHECK_MASK) == DISABLE_RE_INIT) {
				pr_info("SM_FG_REG_FG_OP_STATUS : 0x%x , return FALSE NO init need\n", data);
				return 0;
			}
			else
			{
				pr_info("SM_FG_REG_FG_OP_STATUS : 0x%x , return TRUE init need!!!!\n", data);
				return 1;
			}
		}
		else
		{
			if (((data & INIT_CHECK_MASK) == DISABLE_RE_INIT)&&(sm->batt_ui_soc >= 1000) &&(sm->is_charging)) {
				// Step1. Turn off charger
				// Step2. FG Reset
                          	sm->table_check_flag = true;
				ret = fg_reset(sm);
				if (ret < 0) {
					pr_err("%s: fail to do reset(%d) retry\n", __func__, ret);
					ret = fg_reset(sm);
					if (ret < 0) {
						pr_err("%s: fail to do reset(%d) retry_2\n", __func__, ret);
						ret = fg_reset(sm);
						if (ret < 0) {
							pr_err("%s: fail to do reset(%d) reset fail 3 times!!!! return 0!!!!\n", __func__, ret);
							return 0;
						}
					}
				}
				// Step3. If Step1 was charging-on, turn on charger.
				
				pr_info("SM_FG_REG_FG_OP_STATUS : 0x%x , return TRUE init need because diff_ver SW reset!!!!\n", data);
				return 1;

			}
			else if((data & INIT_CHECK_MASK) != DISABLE_RE_INIT){
				pr_info("SM_FG_REG_FG_OP_STATUS : 0x%x , return TRUE init need!!!!\n", data);
				return 1;
			}
                  	else {
                          	sm->table_check_flag = false;
				pr_info("SM_FG_REG_FG_OP_STATUS : 0x%x , return FALSE  no init need!!!!\n", data);
				return 0;
			}
		}
	}
}

#define MINVAL(a, b) ((a <= b) ? a : b)
#define MAXVAL(a, b) ((a > b) ? a : b)
static int fg_calculate_iocv(struct sm_fg_chip *sm)
{
	bool only_lb=false, sign_i_offset=0; //valid_cb=false, 
	int roop_start=0, roop_max=0, i=0, cb_last_index = 0, cb_pre_last_index =0;
	int lb_v_buffer[FG_INIT_B_LEN+1] = {0, 0, 0, 0, 0, 0, 0, 0};
	int lb_i_buffer[FG_INIT_B_LEN+1] = {0, 0, 0, 0, 0, 0, 0, 0};
	int cb_v_buffer[FG_INIT_B_LEN+1] = {0, 0, 0, 0, 0, 0, 0, 0};
	int cb_i_buffer[FG_INIT_B_LEN+1] = {0, 0, 0, 0, 0, 0, 0, 0};
	int i_offset_margin = 0x14, i_vset_margin = 0x67;
	int v_max=0, v_min=0, v_sum=0, lb_v_avg=0, cb_v_avg=0, lb_v_set=0, lb_i_set=0, i_offset=0;
	int i_max=0, i_min=0, i_sum=0, lb_i_avg=0, cb_i_avg=0, cb_v_set=0, cb_i_set=0;
	int lb_i_p_v_min=0, lb_i_n_v_max=0, cb_i_p_v_min=0, cb_i_n_v_max=0;

	u16 v_ret, i_ret = 0;
	int ret=0;

	u16 data = 0;

	ret = fg_read_word(sm, FG_REG_END_V_IDX, &data);
	if (ret < 0) {
		pr_err("Failed to read FG_REG_END_V_IDX, ret = %d\n", ret);
		return ret;
	} else {
		pr_info("iocv_status_read = addr : 0x%x , data : 0x%x\n", FG_REG_END_V_IDX, data);
	}

	if((data & 0x0010) == 0x0000)
	{
		only_lb = true;
	}

    roop_max = (data & 0x000F);
    if(roop_max > FG_INIT_B_LEN)
    {
        roop_max = FG_INIT_B_LEN;
    }
    else if (roop_max < 3)
    {
        pr_info("iocv buffer count is too small, ret : 0x%x\n",ret);
        lb_v_set = ((fg_read_volt(sm)<<8)/125);
        lb_i_set = ((fg_read_current(sm)<<8)/125);
        ret = lb_v_set;
#ifdef ENABLE_IOCV_ADJ
        if (((lb_v_set < IOCV_MAX_ADJ_LEVEL) && (lb_v_set > IOCV_MIN_ADJ_LEVEL)) && ((abs(lb_i_set) < IOCI_MAX_ADJ_LEVEL) && (abs(lb_i_set) > IOCI_MIN_ADJ_LEVEL)))
        {
            ret = ret - (((lb_i_set * IOCV_I_SLOPE) + IOCV_I_OFFSET) / 1000);
            pr_info("first boot vbat-soc adjust 1st_v=0x%x, 2nd_v=0x%x, all_i=0x%x\n", lb_v_set, ret, lb_i_set);
        }
#endif
        pr_info("iocv ret=%d, table=%d \n",ret, sm->battery_table[BATTERY_TABLE0][FG_TABLE_LEN-1]);
        if(ret > sm->battery_table[BATTERY_TABLE0][FG_TABLE_LEN-1])
        {
            pr_info("iocv ret change hex 0x%x -> 0x%x ret is big than table data\n",ret, sm->battery_table[BATTERY_TABLE0][FG_TABLE_LEN-1]);
            pr_info("iocv ret change dec %d -> %d ret is big than table data\n",ret, sm->battery_table[BATTERY_TABLE0][FG_TABLE_LEN-1]);
            ret = sm->battery_table[BATTERY_TABLE0][FG_TABLE_LEN-1];
        }
        else if(ret < sm->battery_table[BATTERY_TABLE0][0])
        {
            pr_info("iocv ret change 0x%x -> 0x%x \n", ret, (sm->battery_table[BATTERY_TABLE0][0] + 0x10));
            ret = sm->battery_table[BATTERY_TABLE0][0] + 0x10;
        }

        return ret;
    }

	roop_start = FG_REG_START_LB_V;
	for (i = roop_start; i < roop_start + roop_max; i++)
	{
		//ret = fg_read_word(sm, i, &v_ret);
		ret = fg_write_word(sm, 0x8C, i);
		if (ret < 0) {
			pr_err("Failed to write 0x8C, %d ret = %d\n", i, ret);
			return ret;
		}
		msleep(15); //15mS delay 		
		ret = fg_read_word(sm, 0x8D, &v_ret);
		if (v_ret & 0x8000) {
			ret = fg_read_word(sm, 0x8D, &v_ret);
			msleep(15); //15mS delay
		}		
		if (ret < 0) {
			pr_err("Failed to read 0x%x, ret = %d\n",i, ret);
			return ret;
		}
		pr_info("iocv read = addr : 0x%x , data : 0x%x\n", i, v_ret);
		
		//ret = fg_read_word(sm, i+0x10, &i_ret);
		ret = fg_write_word(sm, 0x8C, i+0x10);
		if (ret < 0) {
			pr_err("Failed to write 0x8C, %d ret = %d\n", i+0x10, ret);
			return ret;
		}
		msleep(15); //15mS delay 		
		ret = fg_read_word(sm, 0x8D, &i_ret);
		if (i_ret & 0x8000) {
			ret = fg_read_word(sm, 0x8D, &i_ret);
			msleep(15); //15mS delay
		}		
		if (ret < 0) {
			pr_err("Failed to read 0x%x, ret = %d\n",i, ret);
			return ret;
		}
		pr_info("ioci read = addr : 0x%x , data : 0x%x\n", i+0x10, i_ret);

		if((i_ret&0x4000) == 0x4000)
		{
			i_ret = -(i_ret&0x3FFF);
		}

		lb_v_buffer[i-roop_start] = v_ret;
		lb_i_buffer[i-roop_start] = i_ret;

		if (i == roop_start)
		{
			v_max = v_ret;
			v_min = v_ret;
			v_sum = v_ret;
			i_max = i_ret;
			i_min = i_ret;
			i_sum = i_ret;
		}
		else
		{
			if(v_ret > v_max)
				v_max = v_ret;
			else if(v_ret < v_min)
				v_min = v_ret;
			v_sum = v_sum + v_ret;

			if(i_ret > i_max)
				i_max = i_ret;
			else if(i_ret < i_min)
				i_min = i_ret;
			i_sum = i_sum + i_ret;
		}

		if(abs(i_ret) > i_vset_margin)
		{
			if(i_ret > 0)
			{
				if(lb_i_p_v_min == 0)
				{
					lb_i_p_v_min = v_ret;
				}
				else
				{
					if(v_ret < lb_i_p_v_min)
						lb_i_p_v_min = v_ret;
				}
			}
			else
			{
				if(lb_i_n_v_max == 0)
				{
					lb_i_n_v_max = v_ret;
				}
				else
				{
					if(v_ret > lb_i_n_v_max)
						lb_i_n_v_max = v_ret;
				}
			}
		}
	}
	v_sum = v_sum - v_max - v_min;
	i_sum = i_sum - i_max - i_min;

	lb_v_avg = v_sum / (roop_max-2);
	lb_i_avg = i_sum / (roop_max-2);

	if(abs(lb_i_buffer[roop_max-1]) < i_vset_margin)
	{
		if(abs(lb_i_buffer[roop_max-2]) < i_vset_margin)
		{
			lb_v_set = MAXVAL(lb_v_buffer[roop_max-2], lb_v_buffer[roop_max-1]);
			if(abs(lb_i_buffer[roop_max-3]) < i_vset_margin)
			{
				lb_v_set = MAXVAL(lb_v_buffer[roop_max-3], lb_v_set);
			}
		}
		else
		{
			lb_v_set = lb_v_buffer[roop_max-1];
		}
	}
	else
	{
		lb_v_set = lb_v_avg;
	}

	if(lb_i_n_v_max > 0)
	{
		lb_v_set = MAXVAL(lb_i_n_v_max, lb_v_set);
	}

	if(roop_max > 3)
	{
		lb_i_set = (lb_i_buffer[2] + lb_i_buffer[3]) / 2;
	}

	if((abs(lb_i_buffer[roop_max-1]) < i_offset_margin) && (abs(lb_i_set) < i_offset_margin))
	{
		lb_i_set = MAXVAL(lb_i_buffer[roop_max-1], lb_i_set);
	}
	else if(abs(lb_i_buffer[roop_max-1]) < i_offset_margin)
	{
		lb_i_set = lb_i_buffer[roop_max-1];
	}
	else if(abs(lb_i_set) < i_offset_margin)
	{
		//lb_i_set = lb_i_set;
	}
	else
	{
		lb_i_set = 0;
	}

	i_offset = lb_i_set;

	i_offset = i_offset + 4;

	if(i_offset <= 0)
	{
		sign_i_offset = 1;
#ifdef IGNORE_N_I_OFFSET
		i_offset = 0;
#else
		i_offset = -i_offset;
#endif
	}

	i_offset = i_offset>>1;

	if(sign_i_offset == 0)
	{
		i_offset = i_offset|0x0080;
	}
    i_offset = i_offset | i_offset<<8;

	pr_info("iocv_l_max=0x%x, iocv_l_min=0x%x, iocv_l_avg=0x%x, lb_v_set=0x%x, roop_max=%d \n",
			v_max, v_min, lb_v_avg, lb_v_set, roop_max);
	pr_info("ioci_l_max=0x%x, ioci_l_min=0x%x, ioci_l_avg=0x%x, lb_i_set=0x%x, i_offset=0x%x, sign_i_offset=%d\n",
			i_max, i_min, lb_i_avg, lb_i_set, i_offset, sign_i_offset);

	if(!only_lb)
	{
		roop_start = FG_REG_START_CB_V;
		roop_max = 6;
		for (i = roop_start; i < roop_start + roop_max; i++)
		{
			//ret = fg_read_word(sm, i, &v_ret);
			ret = fg_write_word(sm, 0x8C, i);
			if (ret < 0) {
				pr_err("Failed to write 0x8C, %d ret = %d\n", i, ret);
				return ret;
			}
			msleep(15); //15mS delay 		
			ret = fg_read_word(sm, 0x8D, &v_ret);
			if (v_ret & 0x8000) {
				ret = fg_read_word(sm, 0x8D, &v_ret);
				msleep(15); //15mS delay
			}				
			if (ret < 0) {
				pr_err("Failed to read 0x%x, ret = %d\n",i, ret);
				return ret;
			}
			//ret = fg_read_word(sm, i+0x10, &i_ret);
			ret = fg_write_word(sm, 0x8C, i+0x10);
			if (ret < 0) {
				pr_err("Failed to write 0x8C, %d ret = %d\n", i+0x10, ret);
				return ret;
			}
			msleep(15); //15mS delay 		
			ret = fg_read_word(sm, 0x8D, &i_ret);
			if (i_ret & 0x8000) {
				ret = fg_read_word(sm, 0x8D, &i_ret);
				msleep(15); //15mS delay
			}				
			if (ret < 0) {
				pr_err("Failed to read 0x%x, ret = %d\n",i, ret);
				return ret;
			}

			if((i_ret&0x4000) == 0x4000)
			{
				i_ret = -(i_ret&0x3FFF);
			}

			cb_v_buffer[i-roop_start] = v_ret;
			cb_i_buffer[i-roop_start] = i_ret;

			if (i == roop_start)
			{
				v_max = v_ret;
				v_min = v_ret;
				v_sum = v_ret;
				i_max = i_ret;
				i_min = i_ret;
				i_sum = i_ret;
			}
			else
			{
				if(v_ret > v_max)
					v_max = v_ret;
				else if(v_ret < v_min)
					v_min = v_ret;
				v_sum = v_sum + v_ret;

				if(i_ret > i_max)
					i_max = i_ret;
				else if(i_ret < i_min)
					i_min = i_ret;
				i_sum = i_sum + i_ret;
			}

			if(abs(i_ret) > i_vset_margin)
			{
				if(i_ret > 0)
				{
					if(cb_i_p_v_min == 0)
					{
						cb_i_p_v_min = v_ret;
					}
					else
					{
						if(v_ret < cb_i_p_v_min)
							cb_i_p_v_min = v_ret;
					}
				}
				else
				{
					if(cb_i_n_v_max == 0)
					{
						cb_i_n_v_max = v_ret;
					}
					else
					{
						if(v_ret > cb_i_n_v_max)
							cb_i_n_v_max = v_ret;
					}
				}
			}
		}
		v_sum = v_sum - v_max - v_min;
		i_sum = i_sum - i_max - i_min;

		cb_v_avg = v_sum / (roop_max-2);
		cb_i_avg = i_sum / (roop_max-2);

		cb_last_index = (data & 0x000F)-7; //-6-1
		if(cb_last_index < 0)
		{
			cb_last_index = 5;
		}

    		if(cb_last_index > FG_INIT_B_LEN)
    		{
        		cb_last_index = FG_INIT_B_LEN;
		}

		for (i = roop_max; i > 0; i--)
		{
			if(abs(cb_i_buffer[cb_last_index]) < i_vset_margin)
			{
				cb_v_set = cb_v_buffer[cb_last_index];
				if(abs(cb_i_buffer[cb_last_index]) < i_offset_margin)
				{
					cb_i_set = cb_i_buffer[cb_last_index];
				}

				cb_pre_last_index = cb_last_index - 1;
				if(cb_pre_last_index < 0)
				{
					cb_pre_last_index = 5;
				}

				if(abs(cb_i_buffer[cb_pre_last_index]) < i_vset_margin)
				{
					cb_v_set = MAXVAL(cb_v_buffer[cb_pre_last_index], cb_v_set);
					if(abs(cb_i_buffer[cb_pre_last_index]) < i_offset_margin)
					{
						cb_i_set = MAXVAL(cb_i_buffer[cb_pre_last_index], cb_i_set);
					}
				}
			}
			else
			{
				cb_last_index--;
				if(cb_last_index < 0)
				{
					cb_last_index = 5;
				}
			}
		}

		if(cb_v_set == 0)
		{
			cb_v_set = cb_v_avg;
			if(cb_i_set == 0)
			{
				cb_i_set = cb_i_avg;
			}
		}

		if(cb_i_n_v_max > 0)
		{
			cb_v_set = MAXVAL(cb_i_n_v_max, cb_v_set);
		}

		if(abs(cb_i_set) < i_offset_margin)
		{
			if(cb_i_set > lb_i_set)
			{
				i_offset = cb_i_set;
				i_offset = i_offset + 4;

				if(i_offset <= 0)
				{
					sign_i_offset = 1;
#ifdef IGNORE_N_I_OFFSET
					i_offset = 0;
#else
					i_offset = -i_offset;
#endif
				}

				i_offset = i_offset>>1;

				if(sign_i_offset == 0)
				{
					i_offset = i_offset|0x0080;
				}
                i_offset = i_offset | i_offset<<8;

			}
		}

		pr_info("iocv_c_max=0x%x, iocv_c_min=0x%x, iocv_c_avg=0x%x, cb_v_set=0x%x, cb_last_index=%d\n",
				v_max, v_min, cb_v_avg, cb_v_set, cb_last_index);
		pr_info("ioci_c_max=0x%x, ioci_c_min=0x%x, ioci_c_avg=0x%x, cb_i_set=0x%x, i_offset=0x%x, sign_i_offset=%d\n",
				i_max, i_min, cb_i_avg, cb_i_set, i_offset, sign_i_offset);

	}

    /* final set */
	pr_info("cb_i_set=%d, i_vset_margin=%d, only_lb=%d\n", cb_i_set, i_vset_margin, only_lb);
	if (only_lb) 
	{
		ret = MAXVAL(lb_v_set, cb_i_n_v_max);
		cb_i_set = lb_i_avg;
	}
	else 
	{
		ret = cb_v_set;
		cb_i_set = cb_i_avg;
	}

#ifdef ENABLE_IOCV_ADJ
	if (((ret < IOCV_MAX_ADJ_LEVEL) && (ret > IOCV_MIN_ADJ_LEVEL)) && ((abs(cb_i_set) < IOCI_MAX_ADJ_LEVEL) && (abs(cb_i_set) > IOCI_MIN_ADJ_LEVEL)))
	{
		cb_v_set = ret;
		ret = ret - (((cb_i_set * IOCV_I_SLOPE) + IOCV_I_OFFSET) / 1000);
		pr_info("first boot vbat-soc adjust 1st_v=0x%x, 2nd_v=0x%x, all_i=0x%x\n", cb_v_set, ret, cb_i_set);
	}
#endif

    pr_info("iocv ret=%d, table=%d \n",ret, sm->battery_table[BATTERY_TABLE0][FG_TABLE_LEN-1]);
    if(ret > sm->battery_table[BATTERY_TABLE0][FG_TABLE_LEN-1])
    {
        pr_info("iocv ret change hex 0x%x -> 0x%x ret is big than table data\n",ret, sm->battery_table[BATTERY_TABLE0][FG_TABLE_LEN-1]);
	    pr_info("iocv ret change dec %d -> %d ret is big than table data\n",ret, sm->battery_table[BATTERY_TABLE0][FG_TABLE_LEN-1]);
        ret = sm->battery_table[BATTERY_TABLE0][FG_TABLE_LEN-1];
    }
    else if(ret < sm->battery_table[BATTERY_TABLE0][0])
    {
        pr_info("iocv ret change 0x%x -> 0x%x \n", ret, (sm->battery_table[BATTERY_TABLE0][0] + 0x10));
        ret = sm->battery_table[BATTERY_TABLE0][0] + 0x10;
    }

	return ret;
}

static bool fg_reg_init(struct i2c_client *client)
{
	struct sm_fg_chip *sm = i2c_get_clientdata(client);
	int i, j, value, ret, cnt = 0;
	uint8_t table_reg;
	u16 tem_value, data, data_int_mask = 0;
#ifdef ENABLE_INSPECTION_TABLE	
	int k, m = 0;
#endif
	pr_info("sm5602_fg_reg_init START!!\n");

	do {		
		ret = fg_write_word(sm, sm->regs[SM_FG_REG_PARAM_CTRL], (FG_PARAM_UNLOCK_CODE | ((sm->battery_table_num & 0x0003) << 6) | (FG_TABLE_LEN-1)));
		if (ret < 0) {
			pr_err("Failed to write param_ctrl unlock, ret = %d\n", ret);
			return ret;
		} else {
			pr_info("Param Unlock\n");
		}
		//msleep(3);
		msleep(60);
		ret = fg_read_word(sm, sm->regs[SM_FG_REG_FG_OP_STATUS], &data);
		if (ret < 0){
			pr_err("Failed to read FG_OP_STATUS, ret = %d\n", ret);	
		} else {
			pr_info(" FG_OP_STATUS = 0x%x\n", data);
		}
		cnt++;

		if((data & 0x03)!=0x03)
			msleep(300);

	} while(((data & 0x03)!=0x03) && cnt <= 5);

	/*Temperature/Batt Det -  control register set*/
	ret = fg_read_word(sm, sm->regs[SM_FG_REG_CNTL], &data);
	if (ret < 0) {
			pr_err("Failed to read CNTL, ret = %d\n", ret);
			return ret;
	}

	if (sm->en_temp_in)
		data |= ENABLE_EN_TEMP_IN;
	if (sm->en_temp_ex)
		data |= ENABLE_EN_TEMP_EX;
	if (sm->en_batt_det)
		data |= ENABLE_EN_BATT_DET;

	ret = fg_write_word(sm, sm->regs[SM_FG_REG_CNTL], data);
	if (ret < 0) {
		pr_err("Failed to write CNTL, ret = %d\n", ret);
		return ret;
	} else {
		pr_info("CNTL = 0x%x : 0x%x\n", sm->regs[SM_FG_REG_CNTL], data);
	}

	/* RPARA */
	ret = fg_write_word(sm, sm->regs[SM_FG_REG_VOL_COMP], sm->rpara);
	if (ret < 0) {
		pr_err("Failed to write SM_FG_REG_VOL_COMP, ret = %d\n", ret);
		return ret;
	} else {
			pr_info("Write SM_FG_REG_VOL_COMP = 0x%x : 0x%x\n", sm->regs[SM_FG_REG_VOL_COMP], sm->rpara);
	}

	/* CURRENT OFFSET */
	ret = fg_write_word(sm, sm->regs[SM_FG_REG_CURR_OFFSET], sm->curr_voffset);
	if (ret < 0) {
		pr_err("Failed to write SM_FG_REG_CURR_OFFSET, ret = %d\n", ret);
		return ret;
	} else {
			pr_info("Write SM_FG_REG_CURR_OFFSET = 0x%x : 0x%x\n", sm->regs[SM_FG_REG_CURR_OFFSET], sm->curr_voffset);
	}

	/* CURRENT SLOPE */
	ret = fg_write_word(sm, sm->regs[SM_FG_REG_CURR_SLOPE], sm->curr_voffset);
	if (ret < 0) {
		pr_err("Failed to write SM_FG_REG_CURR_SLOPE, ret = %d\n", ret);
		return ret;
	} else {
			pr_info("Write SM_FG_REG_CURR_SLOPE = 0x%x : 0x%x\n", sm->regs[SM_FG_REG_CURR_SLOPE], sm->curr_voffset);
	}

	/* Init mark */
	if (sm->fg_irq_set == -EINVAL) {
		pr_err("sm->fg_irq_set is invalid");
	} else {
		ret = fg_read_word(sm, sm->regs[SM_FG_REG_INT_MASK], &data_int_mask);
		if (ret < 0){
			pr_err("Failed to read INT_MASK, ret = %d\n", ret);	
			return ret;	
		}	
		ret = fg_write_word(sm, sm->regs[SM_FG_REG_INT_MASK], 0x4000 | (data_int_mask | sm->fg_irq_set));
	    if (ret < 0) {
			pr_err("Failed to write 0x4000 | INIT_MASK, ret = %d\n", ret);
			return ret;
		}
		ret = fg_write_word(sm, sm->regs[SM_FG_REG_INT_MASK], 0x07FF & (data_int_mask | sm->fg_irq_set));
	    if (ret < 0) {
			pr_err("Failed to write INIT_MASK, ret = %d\n", ret);
			return ret;
		}
	}

	/* Low SOC1  */
	if (sm->low_soc1 == -EINVAL) {
		pr_err("sm->low_soc1 is invalid");
	} else {
		ret = fg_read_word(sm, sm->regs[SM_FG_REG_SOC_L_ALARM], &data);
		if (ret < 0){
			pr_err("Failed to read SOC_L_ALARM (LOW_SOC1), ret = %d\n", ret);	
			return ret;	
		}		
		ret = fg_write_word(sm, sm->regs[SM_FG_REG_SOC_L_ALARM], ((data & 0xFFE0) | sm->low_soc1));
	    if (ret < 0) {
			pr_err("Failed to write SOC_L_ALARM (LOW_SOC1), ret = %d\n", ret);
			return ret;
		}
	}

	/* Low SOC2  */	
	if (sm->low_soc2 == -EINVAL) {
		pr_err("sm->low_soc2 is invalid");
	} else {
		ret = fg_read_word(sm, sm->regs[SM_FG_REG_SOC_L_ALARM], &data);
		if (ret < 0){
			pr_err("Failed to read SOC_L_ALARM (LOW_SOC2), ret = %d\n", ret);	
			return ret;	
		}	
		ret = fg_write_word(sm, sm->regs[SM_FG_REG_SOC_L_ALARM],((data & 0xE0FF) | (sm->low_soc2 << 8)));
	    if (ret < 0) {
			pr_err("Failed to write LOW_SOC2, ret = %d\n", ret);
			return ret;
		}
	}

	/* V L ALARM  */
	if (sm->v_l_alarm == -EINVAL) {
		pr_err("sm->v_l_alarm is invalid");
	} else {
		if (sm->v_l_alarm >= 2000 && sm->v_l_alarm < 3000)
			data = (((sm->v_l_alarm-2000)<<8)/1000);
		else if (sm->v_l_alarm >= 3000 && sm->v_l_alarm < 4000)
			data = (((sm->v_l_alarm-3000)<<8)/1000)|0x0100;
		else {
			ret = -EINVAL;
			pr_err("Failed to calculate V_L_ALARM, ret = %d\n", ret);
			return ret;
		}
		
		ret = fg_write_word(sm, sm->regs[SM_FG_REG_V_L_ALARM], data);
	    if (ret < 0) {
			pr_err("Failed to write V_L_ALARM, ret = %d\n", ret);
			return ret;
		}
	}

	/* V H ALARM  */
	if (sm->v_h_alarm == -EINVAL) {
		pr_err("sm->v_h_alarm is invalid");
	} else {
		if (sm->v_h_alarm >= 3000 && sm->v_h_alarm < 4000)
			data = (((sm->v_h_alarm-3000)<<8)/1000);
		else if (sm->v_h_alarm >= 4000 && sm->v_h_alarm < 5000)
			data = (((sm->v_h_alarm-4000)<<8)/1000)|0x0100;
		else {
			ret = -EINVAL;
			pr_err("Failed to calculate V_H_ALARM, ret = %d\n", ret);
			return ret;
		}
		
		ret = fg_write_word(sm, sm->regs[SM_FG_REG_V_H_ALARM], data);
		if (ret < 0) {
			pr_err("Failed to write V_H_ALARM, ret = %d\n", ret);
			return ret;
		}
	}

	/* T IN H/L ALARM  */
	if (sm->t_h_alarm_in == -EINVAL 
		|| sm->t_l_alarm_in == -EINVAL) {
		pr_err("sm->t_h_alarm_in || sm->t_l_alarm_in is invalid");
	} else {
		data = 0; //clear value
		//T IN H ALARM
		if (sm->t_h_alarm_in < 0) {
			data |= 0x8000;
			data |= ((((-1)*sm->t_h_alarm_in) & 0x7F) << 8);
		} else {
			data |= (((sm->t_h_alarm_in) & 0x7F) << 8);
		}
		//T IN L ALARM
		if (sm->t_l_alarm_in < 0) {
			data |= 0x0080;
			data |= ((((-1)*sm->t_l_alarm_in) & 0x7F));
		} else {
			data |= (((sm->t_l_alarm_in) & 0x7F));
		}
		
		ret = fg_write_word(sm, sm->regs[SM_FG_REG_T_IN_H_ALARM], data);
		if (ret < 0) {
			pr_err("Failed to write SM_FG_REG_T_IN_H_ALARM, ret = %d\n", ret);
			return ret;
		}
	}
	
	/* VIT_PERIOD write */
	ret = fg_write_word(sm, sm->regs[SM_FG_REG_VIT_PERIOD], sm->vit_period);
	if (ret < 0) {
		pr_err("Failed to write VIT PERIOD, ret = %d\n", ret);
		return ret;
	} else {
			pr_info("Write VIT_PERIOD = 0x%x : 0x%x\n", sm->regs[SM_FG_REG_VIT_PERIOD], sm->vit_period);
	}

	/* Aging ctrl write */
	ret = fg_write_word(sm, FG_REG_AGING_CTRL, sm->aging_ctrl);
	if (ret < 0) {
		pr_err("Failed to write FG_REG_AGING_CTRL, ret = %d\n", ret);
		return ret;
	} else {
			pr_info("Write FG_REG_AGING_CTRL = 0x%x : 0x%x\n", FG_REG_AGING_CTRL, sm->aging_ctrl);
	}

	/* SOC Cycle ctrl write */
	ret = fg_write_word(sm, FG_REG_SOC_CYCLE_CFG, sm->cycle_cfg);
	if (ret < 0) {
		pr_err("Failed to write FG_REG_SOC_CYCLE_CFG, ret = %d\n", ret);
		return ret;
	} else {
			pr_info("Write FG_REG_SOC_CYCLE_CFG = 0x%x : 0x%x\n", FG_REG_SOC_CYCLE_CFG, sm->cycle_cfg);
	}

	/*RSNS write */
	ret = fg_write_word(sm, sm->regs[SM_FG_REG_RSNS_SEL], sm->batt_rsns);
	if (ret < 0) {
		pr_err("Failed to write SM_FG_REG_RSNS_SEL, ret = %d\n", ret);
		return ret;
	} else {
			pr_info("Write SM_FG_REG_RSNS_SEL = 0x%x : 0x%x\n", sm->regs[SM_FG_REG_RSNS_SEL], sm->batt_rsns);
	}
	
	/* BAT CAP write */
#if defined (FEATURE_BATT_LONG_LIFE_CYCLE)

	i = fg_get_batt_long_life_index_by_cv_voltage(sm);

	if( (sm->cv_voltage_index >= STEP0_CV_VOLTAGE) &&(sm->cv_voltage_index < STEP_CV_VOLTAGE_MAX) )
	{	
		sm->q_max_now = sm->q_max_table[i];
		sm->v_max_now = sm->v_max_table[i];

		sm->cap = sm->q_max_now;

		ret = fg_write_word(sm, sm->regs[SM_FG_REG_BAT_CAP], sm->cap);
		
		if (ret < 0)
		{
			pr_err("Failed to write BAT_CAP, ret = %d\n", ret);
			return ret;
		}
		else
		{
			pr_info("BAT_CAP = 0x%x : 0x%x\n", sm->regs[SM_FG_REG_BAT_CAP], sm->cap);
		}
	}
	else
	{
		pr_err("FG cv_voltage index(%d) error!!! default table(index 0) LOAD\n", sm->cv_voltage_index);
		fg_write_word(sm, sm->regs[SM_FG_REG_BAT_CAP], sm->q_max_table[STEP0_CV_VOLTAGE]);
	}
#else
	ret = fg_write_word(sm, sm->regs[SM_FG_REG_BAT_CAP], sm->cap);
	if (ret < 0) {
		pr_err("Failed to write BAT_CAP, ret = %d\n", ret);
		return ret;
	}
	else {
		pr_info("BAT_CAP = 0x%x : 0x%x\n", sm->regs[SM_FG_REG_BAT_CAP], sm->cap);
	}
#endif

	/* Battery_Table write */
	for (i = BATTERY_TABLE0; i < BATTERY_TABLE2; i++) {
		table_reg = 0xA0 + (i*FG_TABLE_MAX_LEN);
		for (j = 0; j < FG_TABLE_LEN; j++) {
			
#if defined (FEATURE_BATT_LONG_LIFE_CYCLE)
                  
			if(i == 0) 
			{
				if(j == FG_TABLE_LEN - 2)
				{
					sm->battery_table[i][FG_TABLE_LEN - 2] = sm->v_max_now;

					if( sm->battery_table[i][FG_TABLE_LEN - 2] < sm->battery_table[i][FG_TABLE_LEN - 3])
					{
						sm->battery_table[i][FG_TABLE_LEN - 3] = sm->battery_table[i][FG_TABLE_LEN - 2] - 0xE;
						sm->battery_table[1][FG_TABLE_LEN - 3] = (sm->battery_table[1][FG_TABLE_LEN - 2] *99) / 100;
					}
				}
			}
			else if(i == 1) 
			{	
				sm->battery_table[i][j] = (sm->battery_table[i][j] * 2) - (sm->battery_table[i][j] * sm->q_max_now / sm->cap);
				if(j == FG_TABLE_LEN - 1)
				{
					sm->battery_table[i][FG_TABLE_LEN - 2] = 100 * 256;
					sm->battery_table[i][FG_TABLE_LEN - 1] = 100 * 256 + 230;
				}
			}
#endif
			ret = fg_write_word(sm, (table_reg + j), sm->battery_table[i][j]);
						
			if (ret < 0) {
				pr_err("Failed to write Battery Table, ret = %d\n", ret);
				msleep(10);
				ret = fg_write_word(sm, (table_reg + j), sm->battery_table[i][j]);
				if (ret < 0) {
					pr_err("twice Failed to write Battery Table, ret = %d\n", ret);
					return ret;
				}
			} else {
				pr_info("TABLE write OK [%d][%d] = 0x%x : 0x%x\n",
					i, j, (table_reg + j), sm->battery_table[i][j]);
			}
			msleep(10); //10mS delay
		}
	}	
#ifdef ENABLE_INSPECTION_TABLE
	/*--------------for verify table data write----------------------------*/	
	for (i = BATTERY_TABLE0; i < BATTERY_TABLE2; i++) {
		table_reg = 0xA0 + (i*FG_TABLE_MAX_LEN);
		for (j = 0; j < FG_TABLE_LEN; j++) {
			/* 1. Read Table Value */
			ret = fg_write_word(sm, 0x8C, j+(i*FG_TABLE_MAX_LEN));
			if (ret < 0) {
				pr_err("Failed to write 0x8C, %d ret = %d\n", j+(i*FG_TABLE_MAX_LEN), ret);
			}
			msleep(15); //15mS delay 		
			ret = fg_read_word(sm, 0x8D, &data);
			ret = fg_read_word(sm, 0x8D, &data); /* Twice */
			for (k = 1; k <= I2C_ERROR_COUNT_MAX; k++) {
				if (data & 0x8000) {
					msleep(15); //15mS delay
					ret = fg_read_word(sm, 0x8D, &data);
					ret = fg_read_word(sm, 0x8D, &data); /* Twice */	
				} else {
					break;
				}
			}
			if (ret < 0) {
				pr_err("Failed to read 0x8D, %d ret = %d\n", data, ret);
			}			

			/* 2. Compare Table Value */
			if (sm->battery_table[i][j] == data) {
				// 2-1. Table value is same as dtsi value 
				pr_info("TABLE data verify OK [%d][%d] = 0x%x : 0x%x\n",
					i, j, (table_reg + j), data);
			} else {
				/* 3. Table value is not same as dtsi value */
				for (k = 1; k <= I2C_ERROR_COUNT_MAX; k++) {
					pr_info("TABLE write data ERROR!!!! rewrite [%d][%d] = 0x%x : 0x%x, count=%d\n",
					i, j, (table_reg + j), sm->battery_table[i][j], k);
					// 3-1. Rewrite Battery Table
					ret = fg_write_word(sm, (table_reg + j), sm->battery_table[i][j]);
					if (ret < 0) {
						pr_err("Failed to write Battery Table, ret = %d\n", ret);
					} else {
						pr_info("TABLE write OK [%d][%d] = 0x%x : 0x%x\n",
							i, j, (table_reg + j), sm->battery_table[i][j]);
					}
					msleep(15); //15mS delay

					// 3-2 Read Battery Table 
					ret = fg_write_word(sm, 0x8C, j+(i*FG_TABLE_MAX_LEN));					
					if (ret < 0) {
						pr_err("Failed to write Battery Table, ret = %d\n", ret);
					}
					msleep(15); //15mS delay					
					ret = fg_read_word(sm, 0x8D, &data);
					ret = fg_read_word(sm, 0x8D, &data); /* Twice */
					for (m = 1; m <= I2C_ERROR_COUNT_MAX; m++) {
						if (data & 0x8000) {
							msleep(15); //15mS delay
							ret = fg_read_word(sm, 0x8D, &data);
							ret = fg_read_word(sm, 0x8D, &data); /* Twice */	
						} else {
							break;
						}
					}
					// 3-3 Recompare Battery Table with dtsi value
					if (sm->battery_table[i][j] == data) {
						pr_info("TABLE rewrite OK [%d][%d] = 0x%x : 0x%x, count=%d\n",
						i, j, (table_reg + j), data, k);
						break;
					}
					// 3-4 Under I2C_ERROR_COUNT_MAX(5), re-try verify process.
					if (k >= I2C_ERROR_COUNT_MAX) {
						pr_err("rewrite %d times, Fail\n", I2C_ERROR_COUNT_MAX);
					}
				}
			}
		}
	}
#endif
	for(j=0; j < FG_ADD_TABLE_LEN; j++)
	{
		table_reg = 0xD0 + j;
		ret = fg_write_word(sm, table_reg, sm->battery_table[i][j]);
		if (ret < 0) {
			pr_err("Failed to write Battery Table, ret = %d\n", ret);
			return ret;
		} else {
			pr_info("TABLE write OK [%d][%d] = 0x%x : 0x%x\n",
				i, j, table_reg, sm->battery_table[i][j]);
		}
		msleep(10); //10mS delay
	}
#ifdef ENABLE_INSPECTION_TABLE
	/*--------------for verify table data write----------------------------*/
	for (j = 0; j < FG_ADD_TABLE_LEN; j++) {
		i = BATTERY_TABLE2;
		table_reg = 0xD0 + j;
		/* 1. Read Table Value */
		ret = fg_write_word(sm, 0x8C, (j+0x60));
		if (ret < 0) {
			pr_err("Failed to write 0x8C, %d ret = %d\n", table_reg, ret);
		}
		msleep(15); //15mS delay		
		ret = fg_read_word(sm, 0x8D, &data);
		ret = fg_read_word(sm, 0x8D, &data); /* Twice */
		for (k = 1; k <= I2C_ERROR_COUNT_MAX; k++) {
			if (data & 0x8000) {
				msleep(15); //15mS delay
				ret = fg_read_word(sm, 0x8D, &data);
				ret = fg_read_word(sm, 0x8D, &data); /* Twice */	
			} else {
				break;
			}
		}
		if (ret < 0) {
			pr_err("Failed to read 0x8D, %d ret = %d\n", data, ret);
		}			
			/* 2. Compare Table Value */
		if (sm->battery_table[i][j] == data) {
			// 2-1. Table value is same as dtsi value 
			pr_info("TABLE data verify OK [%d][%d] = 0x%x : 0x%x\n",
				i, j, table_reg, data);
		} else {
			/* 3. Table value is not same as dtsi value */
			for (k = 1; k <= I2C_ERROR_COUNT_MAX; k++) {
				pr_info("TABLE write data ERROR!!!! rewrite [%d][%d] = 0x%x : 0x%x, count=%d\n",
				i, j, table_reg, sm->battery_table[i][j], k);
				// 3-1. Rewrite Battery Table
				ret = fg_write_word(sm, table_reg, sm->battery_table[i][j]);
				if (ret < 0) {
					pr_err("Failed to write Battery Table, ret = %d\n", ret);
				} else {
					pr_info("TABLE write OK [%d][%d] = 0x%x : 0x%x\n",
						i, j, table_reg, sm->battery_table[i][j]);
				}
				msleep(15); //15mS delay

				// 3-2 Read Battery Table 
				ret = fg_write_word(sm, 0x8C, (j+0x60));					
				if (ret < 0) {
					pr_err("Failed to write Battery Table, ret = %d\n", ret);
				}
				msleep(15); //15mS delay					
				ret = fg_read_word(sm, 0x8D, &data);
				ret = fg_read_word(sm, 0x8D, &data); /* Twice */
				for (m = 1; m <= I2C_ERROR_COUNT_MAX; m++) {
					if (data & 0x8000) {
						msleep(15); //15mS delay
						ret = fg_read_word(sm, 0x8D, &data);
						ret = fg_read_word(sm, 0x8D, &data); /* Twice */	
					} else {
						break;
					}
				}
				// 3-3 Recompare Battery Table with dtsi value
				if (sm->battery_table[i][j] == data) {
					pr_info("TABLE rewrite OK [%d][%d] = 0x%x : 0x%x, count=%d\n",
					 i, j, table_reg, data, k);
					break;
				}
				// 3-4 Over I2C_ERROR_COUNT_MAX(5), stopped re-try verify process.
				if (k >= I2C_ERROR_COUNT_MAX){
					pr_err("rewrite %d times, Fail\n", I2C_ERROR_COUNT_MAX);
				}
			}
		}
	}
#endif
	/*  RS write */
	ret = fg_write_word(sm, FG_REG_RS, sm->rs);
	if (ret < 0) {
		pr_err("Failed to write RS, ret = %d\n", ret);
		return ret;
	} else {
		pr_info("RS = 0x%x : 0x%x\n",FG_REG_RS, sm->rs);
	}

	/*  alpha write */
	ret = fg_write_word(sm, FG_REG_ALPHA, sm->alpha);
	if (ret < 0) {
		pr_err("Failed to write FG_REG_ALPHA, ret = %d\n", ret);
		return ret;
	} else {
		pr_info("ALPHA = 0x%x : 0x%x\n",FG_REG_ALPHA, sm->alpha);
	}

	/*  beta write */
	ret = fg_write_word(sm, FG_REG_BETA, sm->beta);
	if (ret < 0) {
		pr_err("Failed to write FG_REG_BETA, ret = %d\n", ret);
		return ret;
	} else {
		pr_info("BETA = 0x%x : 0x%x\n",FG_REG_BETA, sm->beta);
	}

	/*  RS write */
	ret = fg_write_word(sm, FG_REG_RS_0, sm->rs_value[0]);
	if (ret < 0) {
		pr_err("Failed to write RS_0, ret = %d\n", ret);
		return ret;
	} else {
		pr_info("RS = 0x%x : 0x%x\n",FG_REG_RS_0, sm->rs_value[0]);
	}

	ret = fg_write_word(sm, FG_REG_RS_1, sm->rs_value[1]);
	if (ret < 0) {
		pr_err("Failed to write RS_1, ret = %d\n", ret);
		return ret;
	} else {
		pr_info("RS_1 = 0x%x : 0x%x\n", FG_REG_RS_1, sm->rs_value[1]);
	}
		
	ret = fg_write_word(sm, FG_REG_RS_2, sm->rs_value[2]);
	if (ret < 0) {
		pr_err("Failed to write RS_2, ret = %d\n", ret);
		return ret;
	} else {
		pr_info("RS_2 = 0x%x : 0x%x\n", FG_REG_RS_2, sm->rs_value[2]);
	}

	ret = fg_write_word(sm, FG_REG_RS_3, sm->rs_value[3]);
	if (ret < 0) {
		pr_err("Failed to write RS_3, ret = %d\n", ret);
		return ret;
	} else {
		pr_info("RS_3 = 0x%x : 0x%x\n", FG_REG_RS_3, sm->rs_value[3]);
	}

	ret = fg_write_word(sm, sm->regs[SM_FG_REG_CURRENT_RATE], sm->mix_value);
	if (ret < 0) {
		pr_err("Failed to write CURRENT_RATE, ret = %d\n", ret);
		return ret;
	} else {
		pr_info("CURRENT_RATE = 0x%x : 0x%x\n", sm->regs[SM_FG_REG_CURRENT_RATE], sm->mix_value);
	}

	pr_info("RS_0 = 0x%x, RS_1 = 0x%x, RS_2 = 0x%x, RS_3 = 0x%x, CURRENT_RATE = 0x%x\n",
		sm->rs_value[0], sm->rs_value[1], sm->rs_value[2], sm->rs_value[3], sm->mix_value);

	/* VOLT_CAL write*/
	ret = fg_write_word(sm, FG_REG_VOLT_CAL, sm->volt_cal);
	if (ret < 0) {
		pr_err("Failed to write FG_REG_VOLT_CAL, ret = %d\n", ret);
		return ret;
	} else {
		pr_info("FG_REG_VOLT_CAL = 0x%x : 0x%x\n", FG_REG_VOLT_CAL, sm->volt_cal);
	}		

	/* CAL write*/	
	ret = fg_write_word(sm, FG_REG_CURR_IN_OFFSET, sm->curr_offset);
	if (ret < 0) {
		pr_err("Failed to write CURR_IN_OFFSET, ret = %d\n", ret);
		return ret;
	} else {
		pr_info("CURR_IN_OFFSET = 0x%x : 0x%x\n", FG_REG_CURR_IN_OFFSET, sm->curr_offset);
	}		
	ret = fg_write_word(sm, FG_REG_CURR_IN_SLOPE, sm->curr_slope);
	if (ret < 0) {
		pr_err("Failed to write CURR_IN_SLOPE, ret = %d\n", ret);
		return ret;
	} else {
		pr_info("CURR_IN_SLOPE = 0x%x : 0x%x\n", FG_REG_CURR_IN_SLOPE, sm->curr_slope);
	}

	/* MISC write */
	ret = fg_write_word(sm, sm->regs[SM_FG_REG_MISC], sm->misc);
	if (ret < 0) {
		pr_err("Failed to write REG_MISC, ret = %d\n", ret);
		return ret;
	} else {
		pr_info("REG_MISC 0x%x : 0x%x\n", sm->regs[SM_FG_REG_MISC], sm->misc);
	}

	/* TOPOFF SOC */
	ret = fg_write_word(sm, sm->regs[SM_FG_REG_TOPOFFSOC], sm->topoff_soc);	
	if (ret < 0) {
		pr_err("Failed to write SM_FG_REG_TOPOFFSOC, ret = %d\n", ret);
		return ret;
	} else {
		pr_info("SM_REG_TOPOFFSOC 0x%x : 0x%x\n", sm->regs[SM_FG_REG_TOPOFFSOC], sm->topoff_soc);
	}
	
	/*INIT_last -  control register set*/
	ret = fg_read_word(sm, sm->regs[SM_FG_REG_CNTL], &data);
	if (ret < 0) {
			pr_err("Failed to read CNTL, ret = %d\n", ret);
			return ret;
	}
	
	if (sm->iocv_man_mode)
		data |= ENABLE_IOCV_MAN_MODE;

	ret = fg_write_word(sm, sm->regs[SM_FG_REG_CNTL], data);
	if (ret < 0) {
		pr_err("Failed to write CNTL, ret = %d\n", ret);
		return ret;
	} else {
		pr_info("CNTL = 0x%x : 0x%x\n", sm->regs[SM_FG_REG_CNTL], data);
	}

	/* Parameter Version [COMMON(0~255) | BATTERY(0~255)] */
	ret = fg_write_word(sm, FG_PARAM_VERION, ((sm->common_param_version << 8) | sm->battery_param_version));	
	if (ret < 0) {
		pr_err("Failed to write FG_PARAM_VERION, ret = %d\n", ret);
		return ret;
	}

	/* T EX L ALARM  */
	if (sm->t_l_alarm_ex == -EINVAL) {
		pr_err("sm->t_l_alarm_ex is invalid");
	} else {
		data = (sm->t_l_alarm_ex) >> 1; //NTC Value/2

		ret = fg_write_word(sm, FG_REG_SWADDR, 0x6A);
		if (ret < 0) {
			pr_err("Failed to write FG_REG_SWADDR, ret = %d\n", ret);
			return ret;
		}
		ret = fg_write_word(sm, FG_REG_SWDATA, data);
		if (ret < 0) {
			pr_err("Failed to write FG_REG_SWADDR, ret = %d\n", ret);
			return ret;
		}	

		pr_info("write to T_EX_H_ALARM = 0x%x\n", data);
	}

	/* T EX H ALARM  */
	if (sm->t_h_alarm_ex == -EINVAL) {
		pr_err("sm->t_h_alarm_ex is invalid");
	} else {
		data = (sm->t_h_alarm_ex) >> 1; //NTC Value/2

		ret = fg_write_word(sm, FG_REG_SWADDR, 0x6B);
		if (ret < 0) {
			pr_err("Failed to write FG_REG_SWADDR, ret = %d\n", ret);
			return ret;
		}
		ret = fg_write_word(sm, FG_REG_SWDATA, data);
		if (ret < 0) {
			pr_err("Failed to write FG_REG_SWADDR, ret = %d\n", ret);
			return ret;
		}	

		pr_info("write to T_EX_L_ALARM = 0x%x\n", data);
	}

	if (sm->iocv_man_mode) {
		value = fg_calculate_iocv(sm);

		for (i = 1; i <= I2C_ERROR_COUNT_MAX; i++) {
		    msleep(10);
#ifdef IOCV_M_TYPE1	
			ret = fg_write_word(sm, 0x6D, value);
			if (ret < 0) {
				pr_err("Failed to write 0x6D, ret = %d\n", ret);
				return ret;
			}
#else
			ret = fg_write_word(sm, FG_REG_SWADDR, 0x75);
			if (ret < 0) {
				pr_err("Failed to write FG_REG_SWADDR, ret = %d\n", ret);
				return ret;
			}
			ret = fg_write_word(sm, FG_REG_SWDATA, value);
			if (ret < 0) {
				pr_err("Failed to write FG_REG_SWADDR, ret = %d\n", ret);
				return ret;
			}
#endif
			ret = fg_write_word(sm, 0x8C, 0x75);
			if (ret < 0) {
				pr_err("Failed to write 0x8C, %d ret = %d\n", 0x75, ret);
				return ret;
			}
			msleep(15); //15mS delay 		
			ret = fg_read_word(sm, 0x8D, &tem_value);
			ret |= fg_read_word(sm, 0x8D, &tem_value);
			if (tem_value & 0x8000) {
				msleep(15); //15mS delay
				ret = fg_read_word(sm, 0x8D, &tem_value);
				ret |= fg_read_word(sm, 0x8D, &tem_value);
			}		
			if (ret < 0) {
				pr_err("Failed to read 0x%x, ret = %d\n",0x8D, ret);
				return ret;
			} else {
				pr_info("IOCV_MAN(tem_value) :  0x%x\n", tem_value);
			}

			if (value == tem_value) {
				pr_info("value = 0x%x : 0x%x, count=%d\n",
				value, tem_value, i);
				break;
			}
			// Over I2C_ERROR_COUNT_MAX(5), stopped re-try verify process.
			if (i >= I2C_ERROR_COUNT_MAX) {
				pr_err("rewrite %d times, Fail\n", I2C_ERROR_COUNT_MAX);
			}
		}
		pr_info("IOCV_MAN : 0x%x\n", value);
	}
	
	msleep(20);

	ret = fg_write_word(sm, sm->regs[SM_FG_REG_PARAM_CTRL], ((FG_PARAM_LOCK_CODE | (sm->battery_table_num & 0x0003) << 6) | (FG_TABLE_LEN-1)));
	if (ret < 0) {
		pr_err("Failed to write param_ctrl lock, ret = %d\n", ret);
		return ret;
	} else {
		pr_info("Param Lock\n");
	}

	msleep(160);

	if (sm->en_temp_ex)
		msleep(150);

	return 1;
}

static unsigned int fg_get_device_id(struct i2c_client *client)
{
	struct sm_fg_chip *sm = i2c_get_clientdata(client);
	int ret;
	u16 data;
	int retry = 0;

	for(retry = 0; retry < 3; retry++) {
		ret = fg_read_word(sm, sm->regs[SM_FG_REG_DEVICE_ID], &data);
		if (ret < 0) {
			pr_err("Failed to read DEVICE_ID, ret = %d\n", ret);
			mdelay(10);
		} else {
			pr_info("revision_id = 0x%x\n",(data & 0x000f));
			pr_info("device_id = 0x%x\n",(data & 0x00f0)>>4);
			break;
		}
	}

	return ret;
}

static bool fg_init(struct i2c_client *client)
{
	int ret;//int i = 0;
	u16 data, cntl_ref = 0;
	
	struct sm_fg_chip *sm = i2c_get_clientdata(client);


	if (fg_check_reg_init_need(client)) {
		fg_reg_init(client);
		sm->fg_reset_flag = 1;
#ifdef ENABLE_INIT_DELAY_TEMP		
		sm->en_init_delay_temp = 1;
		cancel_delayed_work_sync(&sm->init_delay_temp_work);
		schedule_delayed_work(&sm->init_delay_temp_work, msecs_to_jiffies(DELAY_TEMP_TIME_5000MS)); /* 5 sec */
	} else {
		sm->en_init_delay_temp = 0;

		/*control register check*/
		ret = fg_read_word(sm, sm->regs[SM_FG_REG_CNTL], &data);
		if (ret < 0) {
			pr_err("Failed to read CNTL, ret = %d\n", ret);
			return false;
		}

		if (sm->en_temp_in)
			cntl_ref |= ENABLE_EN_TEMP_IN;
		if (sm->en_temp_ex)
			cntl_ref |= ENABLE_EN_TEMP_EX;
		if (sm->en_batt_det)
			cntl_ref |= ENABLE_EN_BATT_DET;
		if (sm->iocv_man_mode)
			cntl_ref |= ENABLE_IOCV_MAN_MODE;

		if (data != cntl_ref) {
			ret = fg_write_word(sm, sm->regs[SM_FG_REG_CNTL], cntl_ref);
			if (ret < 0) {
				return false;
				pr_err("CNTL data is WRONG!!!! but Failed to write CNTL, ret = %d\n", ret);
			} else {
				pr_info("CNTL data is WRONG!!!! rewrite try CNTL = 0x%x : 0x%x\n", sm->regs[SM_FG_REG_CNTL], cntl_ref);
			}
		}
	}
#else
	} else {
		/*control register check*/
		ret = fg_read_word(sm, sm->regs[SM_FG_REG_CNTL], &data);
		if (ret < 0) {
			pr_err("Failed to read CNTL, ret = %d\n", ret);
			return false;
		}

		if (sm->en_temp_in)
			cntl_ref |= ENABLE_EN_TEMP_IN;
		if (sm->en_temp_ex)
			cntl_ref |= ENABLE_EN_TEMP_EX;
		if (sm->en_batt_det)
			cntl_ref |= ENABLE_EN_BATT_DET;
		if (sm->iocv_man_mode)
			cntl_ref |= ENABLE_IOCV_MAN_MODE;

		if (data != cntl_ref) {
			ret = fg_write_word(sm, sm->regs[SM_FG_REG_CNTL], cntl_ref);
			if (ret < 0) {
				return false;
				pr_err("CNTL data is WRONG!!!! but Failed to write CNTL, ret = %d\n", ret);
			} else {
				pr_info("CNTL data is WRONG!!!! rewrite try CNTL = 0x%x : 0x%x\n", sm->regs[SM_FG_REG_CNTL], cntl_ref);
			}
		}		
	}
#endif		

	/* BAT CAP write */
/*if defined (FEATURE_BATT_LONG_LIFE_CYCLE)
	

	i = fg_get_batt_long_life_index_by_cv_voltage(sm);

	if ((i >= STEP0_CV_VOLTAGE) && (i < STEP_CV_VOLTAGE_MAX))
	{
		pr_info(" v_max_now is change %x -> %x / cycle_index = (%d) \n", sm->v_max_now, sm->v_max_table[i], i);
		pr_info(" q_max_now is change %x -> %x / cycle_index = (%d) \n", sm->q_max_now, sm->q_max_table[i], i);

		sm->v_max_now  = sm->v_max_table[i];
    	sm->q_max_now  = sm->q_max_table[i];
	}
	else
	{
		pr_info("FG cv_voltage index(%d) error!!! default table(index 0) LOAD\n", i);

		sm->v_max_now = sm->v_max_table[STEP0_CV_VOLTAGE];
    	sm->q_max_now = sm->q_max_table[STEP0_CV_VOLTAGE];
	}
	sm->cap = sm->q_max_now;
#endif*/
	
	ret = fg_write_word(sm, sm->regs[SM_FG_REG_BAT_CAP], sm->cap);
	if (ret < 0) {
		pr_err("Failed to write BAT_CAP, ret = %d\n", ret);
		return ret;
	} else {
		pr_info("BAT_CAP = 0x%x : 0x%x\n", sm->regs[SM_FG_REG_BAT_CAP], sm->cap);
	}

	return true;
}

#define PROPERTY_NAME_SIZE 128
static int fg_common_parse_dt(struct sm_fg_chip *sm)
{
	struct device *dev = &sm->client->dev;
	struct device_node *np = dev->of_node;
    int rc;
    
	BUG_ON(dev == 0);
	BUG_ON(np == 0);
#ifdef FG_ENABLE_IRQ
	sm->gpio_int = of_get_named_gpio(np, "qcom,irq-gpio", 0);
	pr_info("gpio_int=%d\n", sm->gpio_int);

	if (!gpio_is_valid(sm->gpio_int)) {
		pr_info("gpio_int is not valid\n");
		sm->gpio_int = -EINVAL;
	}
#endif
    /* EN TEMP EX/IN */
    if (of_property_read_bool(np, "sm,en_temp_ex"))
        sm->en_temp_ex = true;
	else
		sm->en_temp_ex = 0;
    pr_info("Temperature EX enabled = %d\n", sm->en_temp_ex);

    if (of_property_read_bool(np, "sm,en_temp_in"))
        sm->en_temp_in = true;
	else
		sm->en_temp_in = 0;
    pr_info("Temperature IN enabled = %d\n", sm->en_temp_in);

	if (of_property_read_bool(np, "sm,en_temp_3rd"))
        sm->en_temp_3rd = true;
	else
		sm->en_temp_3rd = 0;
    pr_info("Temperature from 3rd party enabled = %d\n", sm->en_temp_3rd);

	/* EN BATT DET  */
	if (of_property_read_bool(np, "sm,en_batt_det"))
		sm->en_batt_det = true;
	else
		sm->en_batt_det = 0;
	pr_info("Batt Det enabled = %d\n", sm->en_batt_det);

    /* MISC */
    rc = of_property_read_u32(np, "sm,misc",
                        &sm->misc);
    if (rc < 0)
    {
        sm->misc = 0x0800;
    }

	/* IOCV MAN MODE */
	if (of_property_read_bool(np, "sm,iocv_man_mode"))
		sm->iocv_man_mode = true;
	else
		sm->iocv_man_mode = 0;
	pr_info("IOCV_MAN_MODE = %d\n", sm->iocv_man_mode);

    /* Aging */
    rc = of_property_read_u32(np, "sm,aging_ctrl",
                        &sm->aging_ctrl);
    if (rc < 0)
    {
        sm->aging_ctrl = -EINVAL;
    }

    /* SOC Cycle cfg */
    rc = of_property_read_u32(np, "sm,cycle_cfg",
                        &sm->cycle_cfg);
    if (rc < 0)
    {
        sm->cycle_cfg = -EINVAL;
    }

    /* RSNS */
    rc = of_property_read_u32(np, "sm,rsns",
                        &sm->batt_rsns);
    if (rc < 0)
    {
        sm->batt_rsns = -EINVAL;
    }

    /* IRQ Mask */
    rc = of_property_read_u32(np, "sm,fg_irq_set",
                        &sm->fg_irq_set);
    if (rc < 0)
    {
        sm->fg_irq_set = -EINVAL;
    }

    /* LOW SOC1/2 */
    rc = of_property_read_u32(np, "sm,low_soc1", 
    					&sm->low_soc1);
	if (rc < 0)
	{
        sm->low_soc1 = -EINVAL;
	}
    pr_info("low_soc1 = %d\n", sm->low_soc1);

    rc = of_property_read_u32(np, "sm,low_soc2", 
					&sm->low_soc2);
	if (rc < 0)
	{
        sm->low_soc2 = -EINVAL;	
	}
    pr_info("low_soc2 = %d\n", sm->low_soc2);

    /* V_L/H_ALARM */
    rc = of_property_read_u32(np, "sm,v_l_alarm", 
    					&sm->v_l_alarm);
	if (rc < 0)
	{
        sm->v_l_alarm = -EINVAL;
	}
    pr_info("v_l_alarm = %d\n", sm->v_l_alarm);

    rc = of_property_read_u32(np, "sm,v_h_alarm", 
					&sm->v_h_alarm);
	if (rc < 0)
	{
        sm->v_h_alarm = -EINVAL;
	}
    pr_info("v_h_alarm = %d\n", sm->v_h_alarm);

    /* T_IN_H/L_ALARM */
    rc = of_property_read_u32(np, "sm,t_l_alarm_in", 
    					&sm->t_l_alarm_in);
	if (rc < 0)
	{
        sm->t_l_alarm_in = -EINVAL;	
	}
    pr_info("t_l_alarm_in = %d\n", sm->t_l_alarm_in);

    rc = of_property_read_u32(np, "sm,t_h_alarm_in", 
					&sm->t_h_alarm_in);
	if (rc < 0)
	{
        sm->t_h_alarm_in = -EINVAL;
	}
    pr_info("t_h_alarm_in = %d\n", sm->t_h_alarm_in);

    /* T_EX_H/L_ALARM */
    rc = of_property_read_u32(np, "sm,t_l_alarm_ex", 
    					&sm->t_l_alarm_ex);
	if (rc < 0)
	{
        sm->t_l_alarm_ex = -EINVAL;
	}
    pr_info("t_l_alarm_ex = %d\n", sm->t_l_alarm_ex);

    rc = of_property_read_u32(np, "sm,t_h_alarm_ex", 
					&sm->t_h_alarm_ex);
	if (rc < 0)
	{
        sm->t_h_alarm_ex = -EINVAL;
	}
    pr_info("t_h_alarm_ex = %d\n", sm->t_h_alarm_ex);

    /* Battery Table Number */
    rc = of_property_read_u32(np, "sm,battery_table_num",
                        &sm->battery_table_num);
    if (rc < 0)
    {
        sm->battery_table_num = -EINVAL;
    }

    /* Paramater Number */
    rc = of_property_read_u32(np, "sm,param_version",
                        &sm->common_param_version);
    if (rc < 0)
    {
        sm->common_param_version = -EINVAL;
    }
    pr_info("common_param_version = %d\n", sm->common_param_version);

#ifdef SHUTDOWN_DELAY
	/* Shutdown feature */
	if (of_property_read_bool(np, "sm, shutdown-delay-enable"))
		sm->shutdown_delay_enable = 1;
	else
		sm->shutdown_delay_enable = 0;
#endif
#ifdef ENABLE_NTC_COMPENSATION_1
    /* Rtrace */
    rc = of_property_read_u32(np, "sm,rtrace",
                        &sm->rtrace);
    if (rc < 0)
    {
        sm->rtrace = 0;
    }
#endif

	/* RPARA */
    rc = of_property_read_u32(np, "sm,rpara",
                        &sm->rpara);
    if (rc < 0)
    {
        sm->rpara = 0;
    }
	pr_info("rpara = %d\n", sm->rpara);

	/* CURR VOFFSET */
    rc = of_property_read_u32(np, "sm,curr_voffset",
                        &sm->curr_voffset);
    if (rc < 0)
    {
        sm->curr_voffset = 0;
    }
	pr_info("curr_voffset = %d\n", sm->curr_voffset);


	/* CURR VSLOPE */
    rc = of_property_read_u32(np, "sm,curr_vslope",
                        &sm->curr_vslope);
    if (rc < 0)
    {
        sm->curr_vslope = 0x8080;
    }
	pr_info("curr_vslope = %d\n", sm->curr_vslope);



	return 0;
}

static int get_battery_id(struct sm_fg_chip *sm)
{
	int rc, bat_id_uv = 0;
	int batt_id_volt = 0;

	if(sm->batt_id_first_flag){
		pr_err("Needn't retry get batt_id,sm->batt_id: %d\n",sm->batt_id);
		return sm->batt_id;
	}

	if(IS_ERR_OR_NULL(sm)){
		pr_err("sm is NULL, Fail !\n");
		return BATTERY_VENDOR_START;
	}

	sm->batt_id_first_flag = true;

	sm->bat_id_vol = iio_channel_get(sm->dev, "batt_id");
	if (IS_ERR_OR_NULL(sm->bat_id_vol)) {
		pr_err("Error iio_channel_get\n");
		return BATTERY_VENDOR_START;
	}
	pr_info("sm->bat_id_vol: %d\n",sm->bat_id_vol);

	rc = iio_read_channel_processed(sm->bat_id_vol, &bat_id_uv);
	if (rc < 0) {
		pr_err("Error in reading batt_id channel, rc:%d\n", rc);
		return BATTERY_VENDOR_START;
	}

	batt_id_volt = bat_id_uv / 1000;
	pr_info("batt_id_volt: %d, bat_id_uv: %d\n",batt_id_volt, bat_id_uv);

	if(batt_id_volt > 400)
		sm->batt_id = BATTERY_VENDOR_SCUD;
	else if(batt_id_volt > 100)
		sm->batt_id = BATTERY_VENDOR_NVT;
	else
		sm->batt_id = BATTERY_VENDOR_START;

	pr_info("batt_id: %d\n",sm->batt_id);

	return sm->batt_id;
}

static int fg_battery_parse_dt(struct sm_fg_chip *sm)
{
	struct device *dev = &sm->client->dev;
	struct device_node *np = dev->of_node;
	char prop_name[PROPERTY_NAME_SIZE];
	int battery_id = -1;
	int battery_temp_table[FG_TEMP_TABLE_CNT_MAX];
	int table[FG_TABLE_LEN];
	int rs_value[4];
	int topoff_soc[3];
	int temp_offset[6];
	int temp_cal[10];
	int ext_temp_cal[10];
	int battery_type[3];
	int set_temp_poff[4];
	int ret;
	int i, j;
    
#if defined (FEATURE_BATT_LONG_LIFE_CYCLE)
	int v_max_table[STEP_CV_VOLTAGE_MAX];
	int q_max_table[STEP_CV_VOLTAGE_MAX];
#endif

	BUG_ON(dev == 0);
	BUG_ON(np == 0);

	/* battery_params node*/
	np = of_find_node_by_name(of_node_get(np), "battery_params");
	if (np == NULL) {
		pr_info("Cannot find child node \"battery_params\"\n");
		return -EINVAL;
	}

	/* battery_id*/
	if (of_property_read_u32(np, "battery,id", &battery_id) < 0)
		pr_err("not battery,id property\n");
	if (battery_id == -1)
		battery_id = get_battery_id(sm);
	pr_info("battery id = %d\n", battery_id);

	/*  battery_table*/
	for (i = BATTERY_TABLE0; i < BATTERY_TABLE2; i++) {
		snprintf(prop_name, PROPERTY_NAME_SIZE,
			 "battery%d,%s%d", battery_id, "battery_table", i);

		ret = of_property_read_u32_array(np, prop_name, table, FG_TABLE_LEN);
		if (ret < 0)
			pr_info("Can get prop %s (%d)\n", prop_name, ret);
		for (j = 0; j < FG_TABLE_LEN; j++) {
			sm->battery_table[i][j] = table[j];
			pr_info("%s = <table[%d][%d] 0x%x>\n",
				prop_name, i, j, table[j]);
		}
	}

	i = BATTERY_TABLE2;
	snprintf(prop_name, PROPERTY_NAME_SIZE,
		 "battery%d,%s%d", battery_id, "battery_table", i);
	ret = of_property_read_u32_array(np, prop_name, table, FG_ADD_TABLE_LEN);
	if (ret < 0)
		pr_info("Can get prop %s (%d)\n", prop_name, ret);
	else {
		for(j=0; j < FG_ADD_TABLE_LEN; j++)
		{
			sm->battery_table[i][j] = table[j];
			pr_info("%s = <table[%d][%d] 0x%x>\n",
				prop_name, i, j, table[j]);
		
		}
	}
	
    /* rs */
	snprintf(prop_name, PROPERTY_NAME_SIZE, "battery%d,%s", battery_id, "rs");
	ret = of_property_read_u32_array(np, prop_name, &sm->rs, 1);
	if (ret < 0)
		pr_err("Can get prop %s (%d)\n", prop_name, ret);
	pr_info("%s = <0x%x>\n", prop_name, sm->rs);

    /* alpha */
	snprintf(prop_name, PROPERTY_NAME_SIZE, "battery%d,%s", battery_id, "alpha");
	ret = of_property_read_u32_array(np, prop_name, &sm->alpha, 1);
	if (ret < 0)
		pr_err("Can get prop %s (%d)\n", prop_name, ret);
	pr_info("%s = <0x%x>\n", prop_name, sm->alpha);

    /* beta */
	snprintf(prop_name, PROPERTY_NAME_SIZE, "battery%d,%s", battery_id, "beta");
	ret = of_property_read_u32_array(np, prop_name, &sm->beta, 1);
	if (ret < 0)
		pr_err("Can get prop %s (%d)\n", prop_name, ret);
	pr_info("%s = <0x%x>\n", prop_name, sm->beta);

	/* rs_value*/
	for (i = 0; i < 4; i++) {
		snprintf(prop_name,
			PROPERTY_NAME_SIZE, "battery%d,%s",
			battery_id, "rs_value");
		ret = of_property_read_u32_array(np, prop_name, rs_value, 4);
		if (ret < 0)
			pr_err("Can get prop %s (%d)\n", prop_name, ret);
		sm->rs_value[i] = rs_value[i];
	}
	pr_info("%s = <0x%x 0x%x 0x%x 0x%x>\n",
		prop_name, rs_value[0], rs_value[1], rs_value[2], rs_value[3]);

	/* vit_period*/
	snprintf(prop_name,
		PROPERTY_NAME_SIZE, "battery%d,%s",
		battery_id, "vit_period");
	ret = of_property_read_u32_array(np,
		prop_name, &sm->vit_period, 1);
	if (ret < 0)
		pr_info("Can get prop %s (%d)\n", prop_name, ret);
	pr_info("%s = <0x%x>\n", prop_name, sm->vit_period);

    /* battery_type*/
    snprintf(prop_name, PROPERTY_NAME_SIZE, "battery%d,%s", battery_id, "battery_type");
    ret = of_property_read_u32_array(np, prop_name, battery_type, 3);
    if (ret < 0)
        pr_err("Can get prop %s (%d)\n", prop_name, ret);
    sm->batt_v_max = battery_type[0];
    sm->min_cap = battery_type[1];
    sm->cap = battery_type[2];

    pr_info("%s = <%d %d %d>\n", prop_name,
        sm->batt_v_max, sm->min_cap, sm->cap);

	/* tem poff level */
	snprintf(prop_name, PROPERTY_NAME_SIZE, "battery%d,%s", battery_id, "tem_poff");
	ret = of_property_read_u32_array(np, prop_name, set_temp_poff, 2);
	if (ret < 0)
		pr_err("Can get prop %s (%d)\n", prop_name, ret);
	sm->n_tem_poff = set_temp_poff[0];
	sm->n_tem_poff_offset = set_temp_poff[1];

	pr_info("%s = <%d, %d>\n",
		prop_name,
		sm->n_tem_poff, sm->n_tem_poff_offset);

    /* max-voltage -mv*/
    snprintf(prop_name,
		PROPERTY_NAME_SIZE, "battery%d,%s", battery_id, "max_voltage_uv");    
	ret = of_property_read_u32(np, prop_name,
				        &sm->batt_max_voltage_uv);
	if (ret < 0)
	    pr_err("couldn't find battery max voltage\n");        

    // TOPOFF SOC
    snprintf(prop_name, 
    	PROPERTY_NAME_SIZE, "battery%d,%s", battery_id, "topoff_soc");
    ret = of_property_read_u32_array(np, prop_name, topoff_soc, 3);
    if (ret < 0)
        pr_err("Can get prop %s (%d)\n", prop_name, ret);
    sm->topoff_soc = topoff_soc[0];
    sm->top_off = topoff_soc[1];
	sm->topoff_margin = topoff_soc[2];

    pr_info("%s = <%d %d %d>\n", prop_name,
        sm->topoff_soc, sm->top_off, sm->topoff_margin);

    // Mix
    snprintf(prop_name, 
    	PROPERTY_NAME_SIZE, "battery%d,%s", battery_id, "mix_value");
    ret = of_property_read_u32_array(np, prop_name, &sm->mix_value, 1);
    if (ret < 0)
        pr_err("Can get prop %s (%d)\n", prop_name, ret);

    pr_info("%s = <%d>\n", prop_name,
        sm->mix_value);

    /* VOLT CAL */
    snprintf(prop_name, PROPERTY_NAME_SIZE, "battery%d,%s", battery_id, "volt_cal");
    ret = of_property_read_u32_array(np, prop_name, &sm->volt_cal, 1);
    if (ret < 0)
        pr_err("Can get prop %s (%d)\n", prop_name, ret);
    pr_info("%s = <0x%x>\n", prop_name, sm->volt_cal);

	/* CURR OFFSET */
	snprintf(prop_name,
		PROPERTY_NAME_SIZE, "battery%d,%s", battery_id, "curr_offset");
	ret = of_property_read_u32_array(np,
		prop_name, &sm->curr_offset, 1);
	if (ret < 0)
		pr_err("Can get prop %s (%d)\n", prop_name, ret);
	pr_info("%s = <0x%x>\n", prop_name, sm->curr_offset);

	/* CURR SLOPE */
	snprintf(prop_name,
		PROPERTY_NAME_SIZE, "battery%d,%s", battery_id, "curr_slope");
	ret = of_property_read_u32_array(np,
		prop_name, &sm->curr_slope, 1);
	if (ret < 0)
		pr_err("Can get prop %s (%d)\n", prop_name, ret);
	pr_info("%s = <0x%x>\n", prop_name, sm->curr_slope);

	/* temp_std */
	snprintf(prop_name, 
		PROPERTY_NAME_SIZE, "battery%d,%s", battery_id, "temp_std");
	ret = of_property_read_u32_array(np, prop_name, &sm->temp_std, 1);
	if (ret < 0)
		pr_err("Can get prop %s (%d)\n", prop_name, ret);
	pr_info("%s = <%d>\n", prop_name, sm->temp_std);

	/* temp_offset */
	snprintf(prop_name, 
		PROPERTY_NAME_SIZE, "battery%d,%s", battery_id, "temp_offset");
	ret = of_property_read_u32_array(np, prop_name, temp_offset, 6);
	if (ret < 0)
		pr_err("Can get prop %s (%d)\n", prop_name, ret);
	sm->en_high_fg_temp_offset = temp_offset[0];
	sm->high_fg_temp_offset_denom = temp_offset[1];
	sm->high_fg_temp_offset_fact = temp_offset[2];
	sm->en_low_fg_temp_offset = temp_offset[3];
	sm->low_fg_temp_offset_denom = temp_offset[4];
	sm->low_fg_temp_offset_fact = temp_offset[5];
	pr_info("%s = <%d, %d, %d, %d, %d, %d>\n", prop_name,
		sm->en_high_fg_temp_offset, 
		sm->high_fg_temp_offset_denom, sm->high_fg_temp_offset_fact,
		sm->en_low_fg_temp_offset,
		sm->low_fg_temp_offset_denom, sm->low_fg_temp_offset_fact);

	/* temp_calc */
	snprintf(prop_name, 
		PROPERTY_NAME_SIZE, "battery%d,%s", battery_id, "temp_cal");
	ret = of_property_read_u32_array(np, prop_name, temp_cal, 10);
	if (ret < 0)
		pr_err("Can get prop %s (%d)\n", prop_name, ret);
	sm->en_high_fg_temp_cal = temp_cal[0];
	sm->high_fg_temp_p_cal_denom = temp_cal[1];
	sm->high_fg_temp_p_cal_fact = temp_cal[2];
	sm->high_fg_temp_n_cal_denom = temp_cal[3];
	sm->high_fg_temp_n_cal_fact = temp_cal[4];
	sm->en_low_fg_temp_cal = temp_cal[5];
	sm->low_fg_temp_p_cal_denom = temp_cal[6];
	sm->low_fg_temp_p_cal_fact = temp_cal[7];
	sm->low_fg_temp_n_cal_denom = temp_cal[8];
	sm->low_fg_temp_n_cal_fact = temp_cal[9];
	pr_info("%s = <%d, %d, %d, %d, %d, %d, %d, %d, %d, %d>\n", prop_name,
		sm->en_high_fg_temp_cal, 
		sm->high_fg_temp_p_cal_denom, sm->high_fg_temp_p_cal_fact,
		sm->high_fg_temp_n_cal_denom, sm->high_fg_temp_n_cal_fact,
		sm->en_low_fg_temp_cal,
		sm->low_fg_temp_p_cal_denom, sm->low_fg_temp_p_cal_fact,
		sm->low_fg_temp_n_cal_denom, sm->low_fg_temp_n_cal_fact);

	/* ext_temp_calc */
	snprintf(prop_name, 
		PROPERTY_NAME_SIZE, "battery%d,%s", battery_id, "ext_temp_cal");
	ret = of_property_read_u32_array(np, prop_name, ext_temp_cal, 10);
	if (ret < 0)
		pr_err("Can get prop %s (%d)\n", prop_name, ret);
	sm->en_high_temp_cal = ext_temp_cal[0];
	sm->high_temp_p_cal_denom = ext_temp_cal[1];
	sm->high_temp_p_cal_fact = ext_temp_cal[2];
	sm->high_temp_n_cal_denom = ext_temp_cal[3];
	sm->high_temp_n_cal_fact = ext_temp_cal[4];
	sm->en_low_temp_cal = ext_temp_cal[5];
	sm->low_temp_p_cal_denom = ext_temp_cal[6];
	sm->low_temp_p_cal_fact = ext_temp_cal[7];
	sm->low_temp_n_cal_denom = ext_temp_cal[8];
	sm->low_temp_n_cal_fact = ext_temp_cal[9];
	pr_info("%s = <%d, %d, %d, %d, %d, %d, %d, %d, %d, %d>\n", prop_name,
		sm->en_high_temp_cal, 
		sm->high_temp_p_cal_denom, sm->high_temp_p_cal_fact,
		sm->high_temp_n_cal_denom, sm->high_temp_n_cal_fact,
		sm->en_low_temp_cal,
		sm->low_temp_p_cal_denom, sm->low_temp_p_cal_fact,
		sm->low_temp_n_cal_denom, sm->low_temp_n_cal_fact);

	/* get battery_temp_table*/
	 snprintf(prop_name, PROPERTY_NAME_SIZE,
		  "battery%d,%s", battery_id, "thermal_table");
	
	 ret = of_property_read_u32_array(np, prop_name, battery_temp_table, FG_TEMP_TABLE_CNT_MAX);
	 if (ret < 0)
		 pr_err("Can get prop %s (%d)\n", prop_name, ret);
	 for (i = 0; i < FG_TEMP_TABLE_CNT_MAX; i++) {
		 sm->battery_temp_table[i] = battery_temp_table[i];
		 pr_err("%s = <battery_temp_table[%d] 0x%x>\n",
			 prop_name, i,	battery_temp_table[i]);
	 }

    /* Battery Paramter */
	snprintf(prop_name, PROPERTY_NAME_SIZE, "battery%d,%s", battery_id, "param_version");
	ret = of_property_read_u32_array(np, prop_name, &sm->battery_param_version, 1);
	if (ret < 0)
		pr_err("Can get prop %s (%d)\n", prop_name, ret);
	pr_info("%s = <0x%x>\n", prop_name, sm->battery_param_version);
	
	/* Battery long life Paramter*/
#if defined (FEATURE_BATT_LONG_LIFE_CYCLE)
	// V_max_table
	for (i = 0; i < STEP_CV_VOLTAGE_MAX; i++) 
	{
		snprintf(prop_name,	PROPERTY_NAME_SIZE, "battery%d,%s", battery_id, "v_max_table");
		ret = of_property_read_u32_array(np, prop_name, v_max_table, STEP_CV_VOLTAGE_MAX);
		if (ret < 0) pr_err("Can get prop %s (%d)\n", prop_name, ret);
		sm->v_max_table[i] = v_max_table[i];
	}
	pr_info("%s = <0x%x 0x%x 0x%x 0x%x 0x%x>\n",
		prop_name, v_max_table[0], v_max_table[1], v_max_table[2], v_max_table[3], v_max_table[4]);

	// q_max_table
	for (i = 0; i < STEP_CV_VOLTAGE_MAX; i++) 
	{
		snprintf(prop_name,	PROPERTY_NAME_SIZE, "battery%d,%s", battery_id, "q_max_table");
		ret = of_property_read_u32_array(np, prop_name, q_max_table, STEP_CV_VOLTAGE_MAX);
		if (ret < 0) pr_err("Can get prop %s (%d)\n", prop_name, ret);
		sm->q_max_table[i] = q_max_table[i];
	}
	pr_info("%s = <0x%x 0x%x 0x%x 0x%x 0x%x>\n",
		prop_name, q_max_table[0], q_max_table[1], q_max_table[2], q_max_table[3], q_max_table[4]);
#endif

	return 0;
}
static int fg_get_soc_decimal(struct sm_fg_chip *sm)
{
        int raw_soc;
        raw_soc = fg_read_soc(sm);
        return raw_soc % 10 * 10;
}

static int fg_get_soc_decimal_rate(struct sm_fg_chip *sm)
{
        int soc,i;

        if(sm->dec_rate_len <= 0)
                return 0;

        soc = fg_read_soc(sm);
        soc /= 10;

        for(i = 0; i < sm->dec_rate_len; i += 2){
                if(soc < sm->dec_rate_seq[i]){
                        return sm->dec_rate_seq[i-1];
                }
        }
        return sm->dec_rate_seq[sm->dec_rate_len-1];
}
bool hal_fg_init(struct i2c_client *client)
{
	int ret;
	struct sm_fg_chip *sm = i2c_get_clientdata(client);

	pr_info("sm5602 hal_fg_init...\n");
		/*sm5602 i2c read check*/
	ret = fg_get_device_id(client);
	if (ret < 0) {
		pr_err("%s: fail to do i2c read(%d)\n", __func__, ret);
		return false;
	}
	mutex_lock(&sm->data_lock);
	if (client->dev.of_node) {
		/* Load common data from DTS*/
		fg_common_parse_dt(sm);
		/* Load battery data from DTS*/
		fg_battery_parse_dt(sm);
	}
	if(!fg_init(client))
        return false;

	mutex_unlock(&sm->data_lock);
	pr_info("hal fg init OK\n");
	return true;
}
static int sm5602_iio_read_raw(struct iio_dev *indio_dev,
		struct iio_chan_spec const *chan, int *val1,
		int *val2, long mask)
{
	struct sm_fg_chip *sm = iio_priv(indio_dev);
	int rc = 0;

	*val1 = 0;

	switch (chan->channel) {
	case PSY_IIO_SHUTDOWN_DELAY:
		*val1 = sm->shutdown_delay;
		break;
	case PSY_IIO_RESISTANCE:
		*val1 = 0;
		break;
	case PSY_IIO_RESISTANCE_ID:
		if(sm->batt_id == BATTERY_VENDOR_NVT)
			*val1 = BATTERY_VENDOR_NVT;
		else if (sm->batt_id == BATTERY_VENDOR_SCUD)
			*val1 = BATTERY_VENDOR_SCUD;
		else
			*val1 = 0;
		break;
	case PSY_IIO_SOC_DECIMAL:
		*val1 = fg_get_soc_decimal(sm);
		break;
	case PSY_IIO_SOC_DECIMAL_RATE:
		*val1 = fg_get_soc_decimal_rate(sm);
		break;
	case PSY_IIO_FASTCHARGE_MODE:
		*val1 = sm->fast_mode;
		break;
	case PSY_IIO_BATTERY_TYPE:
#if 0
		switch (get_battery_id(sm)) {
			case 0:
				*val1 = 0;		//"M376-NVT-5000mAh";
				break;
		}
#endif
		break;
	case PSY_IIO_SOH:
		*val1 = 100;
		break;

	default:
		pr_debug("Unsupported sm5602 IIO chan %d\n", chan->channel);
		rc = -EINVAL;
		break;
	}

	if (rc < 0) {
		pr_err("Couldn't read IIO channel %d, rc = %d\n",
			chan->channel, rc);
		return rc;
	}

	return IIO_VAL_INT;
}
static int fg_set_fastcharge_mode(struct sm_fg_chip *sm, bool enable)
{
        int ret = 0;

        sm->fast_mode = enable;

        return ret;
}
static int sm5602_iio_write_raw(struct iio_dev *indio_dev,
		struct iio_chan_spec const *chan, int val1,
		int val2, long mask)
{
	struct sm_fg_chip *sm = iio_priv(indio_dev);
	int rc = 0;

	switch (chan->channel) {
	case PSY_IIO_FASTCHARGE_MODE:
		fg_set_fastcharge_mode(sm, !!val1);
		break;

	default:
		pr_err("Unsupported SM5602 IIO chan %d\n", chan->channel);
		rc = -EINVAL;
		break;
	}
	if (rc < 0)
		pr_err("Couldn't write IIO channel %d, rc = %d\n",
			chan->channel, rc);
	return rc;
}

static int sm5602_iio_of_xlate(struct iio_dev *indio_dev,
				const struct of_phandle_args *iiospec)
{
	struct sm_fg_chip *sm = iio_priv(indio_dev);
	struct iio_chan_spec *iio_chan = sm->iio_chan;
	int i =0;

	for (i=0; i < ARRAY_SIZE(sm5602_iio_psy_channels);
					i++, iio_chan++)
		if (iio_chan->channel == iiospec->args[0])
			return i;
	return -EINVAL;
}

static const struct iio_info sm5602_iio_info = {
	.read_raw	= sm5602_iio_read_raw,
	.write_raw	= sm5602_iio_write_raw,
	.of_xlate	= sm5602_iio_of_xlate,
};

static int sm5602_init_iio_psy(struct sm_fg_chip *chip)
{
	struct iio_dev *indio_dev = chip->indio_dev;
	struct iio_chan_spec *chan = NULL;
	int num_iio_channels = ARRAY_SIZE(sm5602_iio_psy_channels);
	int rc = 0, i = 0;

	pr_info("sm5602_init_iio_psy start\n");
	chip->iio_chan = devm_kcalloc(chip->dev, num_iio_channels,
				sizeof(*chip->iio_chan), GFP_KERNEL);
	if (!chip->iio_chan)
		return -ENOMEM;

	chip->int_iio_chans = devm_kcalloc(chip->dev,
				num_iio_channels,
				sizeof(*chip->int_iio_chans),
				GFP_KERNEL);

	if (!chip->int_iio_chans)
		return -ENOMEM;

	indio_dev->info = &sm5602_iio_info;
	indio_dev->dev.parent = chip->dev;
	indio_dev->dev.of_node = chip->dev->of_node;
	indio_dev->modes = INDIO_DIRECT_MODE;
	indio_dev->channels = chip->iio_chan;
	indio_dev->num_channels = num_iio_channels;
	indio_dev->name = "sm5602";

	for (i = 0; i < num_iio_channels; i++) {
		chip->int_iio_chans[i].indio_dev = indio_dev;
		chan = &chip->iio_chan[i];
		chip->int_iio_chans[i].channel = chan;
		chan->address = i;
		chan->channel = sm5602_iio_psy_channels[i].channel_num;
		chan->type = sm5602_iio_psy_channels[i].type;
		chan->datasheet_name =
			sm5602_iio_psy_channels[i].datasheet_name;
		chan->extend_name =
			sm5602_iio_psy_channels[i].datasheet_name;
		chan->info_mask_separate =
			sm5602_iio_psy_channels[i].info_mask;
	}

	rc = devm_iio_device_register(chip->dev, indio_dev);

	if (rc)
		pr_err("Failed to register SM5602 IIO device, rc=%d\n", rc);

	pr_info("battery IIO device, rc=%d\n", rc);

	return rc;
}

int sm5602_set_shutdown(int en_shutdown)
{
	int ret = 0;
	uint16_t data = 0;
	int retry = 0;

	for(retry = 0; retry < 3; retry++) {
		if (en_shutdown) {
			// Control Register for en_shutdown bit
			ret = fg_read_word(g_sm_fg_chip, g_sm_fg_chip->regs[SM_FG_REG_SHUTDOWN], &data);
			if (ret < 0) {
				pr_err("Failed to read CNTL, ret = %d\n", ret);
				continue;
			}

			//data |= ENABLE_EN_SHUTDOWN;
			data |= 1;
			ret = fg_write_word(g_sm_fg_chip, g_sm_fg_chip->regs[SM_FG_REG_SHUTDOWN], data);
			if (ret < 0) {
				pr_err("Failed to write SHUTDOWN, ret = %d\n", ret);
				continue;
			} else {
				pr_info("SHUTDOWN = 0x%x : 0x%x\n", g_sm_fg_chip->regs[SM_FG_REG_SHUTDOWN], data);
			}

			// Control Register for shutdown bit
			ret = fg_read_word(g_sm_fg_chip, g_sm_fg_chip->regs[SM_FG_REG_CNTL], &data);
			if (ret < 0) {
				pr_err("Failed to read CNTL, ret = %d\n", ret);
				continue;
			}

			data |= ENABLE_SHUTDOWN;
			ret = fg_write_word(g_sm_fg_chip, g_sm_fg_chip->regs[SM_FG_REG_CNTL], data);
			if (ret < 0) {
				pr_err("Failed to write CNTL, ret = %d\n", ret);
				continue;
			} else {
				pr_info("CNTL = 0x%x : 0x%x\n", g_sm_fg_chip->regs[SM_FG_REG_CNTL], data);
			}
			break;
		} else {
			// Control Register for en_shutdown bit
			ret = fg_read_word(g_sm_fg_chip, g_sm_fg_chip->regs[SM_FG_REG_SHUTDOWN], &data);
			if (ret < 0) {
				pr_err("Failed to read CNTL, ret = %d\n", ret);
				continue;
			}

			//data &= ~ENABLE_EN_SHUTDOWN;
			data &= ~1;
			ret = fg_write_word(g_sm_fg_chip, g_sm_fg_chip->regs[SM_FG_REG_SHUTDOWN], data);
			if (ret < 0) {
				pr_err("Failed to write SHUTDOWN, ret = %d\n", ret);
				continue;
			} else {
				pr_info("SHUTDOWN = 0x%x : 0x%x\n", g_sm_fg_chip->regs[SM_FG_REG_SHUTDOWN], data);
			}

			// Read Register for shutdown bit
			ret = fg_read_word(g_sm_fg_chip, g_sm_fg_chip->regs[SM_FG_REG_CNTL], &data);
			if (ret < 0) {
				pr_err("Failed to read CNTL, ret = %d\n", ret);
				continue;
			} else {
				pr_info("CNTL = 0x%x : 0x%x\n", g_sm_fg_chip->regs[SM_FG_REG_CNTL], data);
			}
			// Control Register for shutdown bit
			ret = fg_read_word(g_sm_fg_chip, g_sm_fg_chip->regs[SM_FG_REG_CNTL], &data);
			if (ret < 0) {
				pr_err("Failed to read CNTL, ret = %d\n", ret);
				continue;
			}
			break;
		}
	}

	msleep(1);

	if (en_shutdown)
		pr_info("Enabled shutdown mode (%d)", retry);
	else
		pr_info("Disabled shutdown mode (%d)", retry);

	return ret;
}
EXPORT_SYMBOL_GPL(sm5602_set_shutdown);

bool first_power_on = false;
EXPORT_SYMBOL_GPL(first_power_on);

static int sm_fg_probe(struct i2c_client *client,
							const struct i2c_device_id *id)
{

	int ret = 0;
	struct sm_fg_chip *sm;
	u8 *regs;
	struct iio_dev *indio_dev =NULL;
	pr_err("enter\n");
	indio_dev = devm_iio_device_alloc(&client->dev, sizeof(*sm));

	if (!indio_dev){
		pr_err("Failed to allocate memory\n");
		return -ENOMEM;
	}

	sm = iio_priv(indio_dev);
	sm->indio_dev = indio_dev;

	sm->dev = &client->dev;
	sm->client = client;
	sm->chip = id->driver_data;

	sm->batt_soc	= -ENODATA;
	sm->batt_fcc	= -ENODATA;
	//sm->batt_dc		= -ENODATA;
	sm->batt_volt	= -ENODATA;
	sm->batt_temp	= -ENODATA;
	sm->batt_curr	= -ENODATA;
	sm->fake_soc	= -EINVAL;
	sm->fake_temp	= -EINVAL;
	sm->batt_id_first_flag = false;
  	sm->table_check_flag = true;
	sm->fg_reset_flag = 1;
  
#ifdef ENABLE_INIT_DELAY_TEMP	
	sm->en_init_delay_temp = 0;
#endif
#ifdef DELAY_SOC_ZERO
	sm->en_delay_soc_zero = 0;
	sm->recover_delay_soc_zero = 1;
#endif
	first_power_on = false;
	if (sm->chip == SM5602) {
		regs = sm5602_regs;
	} else {
		pr_err("unexpected fuel gauge: %d\n", sm->chip);
		regs = sm5602_regs;
	}
	ret = sm5602_init_iio_psy(sm);
    if (ret < 0) {
        pr_err("Failed to initialize sm5602 IIO PSY, ret=%d\n", ret);
        goto err_0;
    }
	memcpy(sm->regs, regs, NUM_REGS);

	i2c_set_clientdata(client, sm);

	ret = fg_get_device_id(client);
	if (ret < 0) {
		pr_err("do not sm5602 detect ic, exit\n");
		goto err_0;
	}

	mutex_init(&sm->i2c_rw_lock);
	mutex_init(&sm->data_lock);

	sm->bat_id_vol = iio_channel_get(sm->dev, "batt_id");
	if (IS_ERR_OR_NULL(sm->bat_id_vol)) {
		pr_err("Error iio_channel_get\n");
		return -EPROBE_DEFER;
	}

	if (fg_check_reg_init_need(client)) {
		first_power_on = true;
	}
	pr_err("first_power_on[%d]\n",first_power_on);

	fg_psy_register(sm);

#ifdef ENABLE_INIT_DELAY_TEMP
	INIT_DELAYED_WORK(&sm->init_delay_temp_work, fg_init_delay_temp_workfunc);
#endif
#ifdef DELAY_SOC_ZERO
	INIT_DELAYED_WORK(&sm->delay_soc_zero_work, fg_delay_soc_zero_workfunc);
#endif

	if (hal_fg_init(client) == false) {
	    pr_err("Failed to Initialize Fuelgauge\n");
        goto err_0;
	}

#ifdef IN_PRO_ADJ
	if (fg_in_pro(client) < 0) {
		pr_err("Failed to fg_in_pro\n");
	}
#endif

#ifdef SOC_SMOOTH_TRACKING
	sm->param.batt_soc = -EINVAL;
	sm->param.update_now = true;
#endif
#ifdef ENABLE_TEMP_AVG
	sm->temp_param.batt_temp = -EINVAL;
	sm->temp_param.update_now = true;
#endif
	INIT_DELAYED_WORK(&sm->monitor_work, fg_monitor_workfunc);

#ifdef FG_ENABLE_IRQ
	if (sm->gpio_int > 0) {
		ret = gpio_request(sm->gpio_int, "sm5602_int");
		if (ret) {
			pr_err("failed to request int_gpio\n");
			//return rc;
		}
		gpio_direction_input(sm->gpio_int);
		client->irq = gpio_to_irq(sm->gpio_int);
	} else {
	    pr_err("Failed to registe gpio interrupt\n");
        goto err_0;
	}

	if (client->irq) {
		ret = devm_request_threaded_irq(&client->dev, client->irq, NULL,
			fg_irq_thread,
			IRQF_TRIGGER_LOW | IRQF_ONESHOT | IRQF_NO_SUSPEND,
			"sm fuel gauge irq", sm);
		if (ret < 0) {
			pr_err("request irq for irq=%d failed, ret = %d\n", client->irq, ret);
			goto err_1;
		}
	}

	fg_irq_thread(client->irq, sm); // if IRQF_TRIGGER_FALLING or IRQF_TRIGGER_RISING is needed, enable initial irq.
#endif

	create_debugfs_entry(sm);

	fg_dump_debug(sm);

	schedule_delayed_work(&sm->monitor_work, msecs_to_jiffies(10000)); /* 10 sec */

#if 0
	schedule_delayed_work(&sm->soc_monitor_work, msecs_to_jiffies(MONITOR_SOC_WAIT_MS));
#endif
	g_sm_fg_chip = sm;
	pr_info("sm fuel gauge probe successfully, %s\n",device2str[sm->chip]);
	pr_err("end\n");

	return 0;
#ifdef FG_ENABLE_IRQ
err_1:
#endif
	fg_psy_unregister(sm);
err_0:
	return -ENODEV;
}


static int sm_fg_remove(struct i2c_client *client)
{
	struct sm_fg_chip *sm = i2c_get_clientdata(client);

	cancel_delayed_work_sync(&sm->monitor_work);
#ifdef ENABLE_INIT_DELAY_TEMP
	cancel_delayed_work_sync(&sm->init_delay_temp_work);
#endif
#ifdef DELAY_SOC_ZERO
	cancel_delayed_work_sync(&sm->delay_soc_zero_work);
#endif
	pr_info("mode change to shutdown mode\n");
	fg_write_word(sm, FG_REG_RS_2, sm->rs_value[2]);

	fg_psy_unregister(sm);

	if (sm->gpio_int > 0)
		gpio_free(sm->gpio_int);
	mutex_destroy(&sm->data_lock);
	mutex_destroy(&sm->i2c_rw_lock);

	debugfs_remove_recursive(sm->debug_root);

	return 0;

}

static void sm_fg_shutdown(struct i2c_client *client)
{
	pr_info("sm fuel gauge driver shutdown!\n");
}

static const struct of_device_id sm_fg_match_table[] = {
	{.compatible = "sm,sm5602",},
	{},
};
MODULE_DEVICE_TABLE(of, sm_fg_match_table);

static const struct i2c_device_id sm_fg_id[] = {
	{ "sm5602", SM5602 },
	{},
};
MODULE_DEVICE_TABLE(i2c, sm_fg_id);

static struct i2c_driver sm_fg_driver = {
	.driver		= {
		.name		= "sm5602",
		.owner		= THIS_MODULE,
		.of_match_table	= sm_fg_match_table,
	},
	.id_table   = sm_fg_id,
	.probe		= sm_fg_probe,
	.remove		= sm_fg_remove,
	.shutdown     = sm_fg_shutdown,
};


module_i2c_driver(sm_fg_driver);

MODULE_DESCRIPTION("SM SM5602 Gauge Driver");
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Siliconmitus");

