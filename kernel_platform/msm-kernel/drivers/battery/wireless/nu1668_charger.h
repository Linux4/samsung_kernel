/*
 * nu1668_charger.h
 * Samsung NU1668 Charger Header
 *
 * Copyright (C) 2022 Samsung Electronics, Inc.
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

#ifndef __WIRELESS_CHARGER_NU1668_H
#define __WIRELESS_CHARGER_NU1668_H __FILE__

#include <linux/mfd/core.h>
#include <linux/regulator/machine.h>
#include <linux/i2c.h>
#include <linux/alarmtimer.h>
#include <linux/pm_wakeup.h>
#include <linux/battery/sb_wireless.h>
#include "../common/sec_charging_common.h"
#include "nu1668_fod.h"
#include "nu1668_cmfet.h"

#define MFC_NU1668_DRIVER_VERSION			0x001E

#define MFC_FW_BIN_VERSION					0x0322

#define MFC_FLASH_FW_HEX_PATH               "mfc/mfc_fw_flash.bin"
#define MFC_FW_SDCARD_BIN_PATH              "wpc_fw_sdcard.bin"

#define MFC_FLASH_FW_HEX_NU1668_PATH        "mfc/mfc_fw_flash_nu1668.bin"

/* for SPU FW update */
#define MFC_FW_SPU_BIN_PATH                	"mfc/mfc_fw_spu_nu1668.bin"

#define NU1668								1
#define BIN_FILE_TOTAL_LENGTH				32768

#define NU1668_SRAM_BASE_ADDR				0x1200

/* REGISTER MAPS */
/************only for E3************/
#define MFC_CLOAK_REASON_REG				0x0000
#define MFC_FULL_MODE_TRANS_TYPE_REG			0x0001
#define MFC_POWER_LEVEL_SETTING_REG			0x0002
#define MFC_GP_STATE_REG				0x0020
#define MFC_NEGO_DONE_POWER_REG				0x0021
#define MFC_POTENTIAL_LOAD_POWER_REG			0x0022
#define MFC_NEGOTIABLE_LOAD_POWER_REG			0x0023
#define MFC_PTX_EXTENDED_ID0_REG			0x002C
#define MFC_PTX_EXTENDED_ID1_REG			0x002D
#define MFC_PTX_EXTENDED_ID2_REG			0x002E
#define MFC_DC_CURR_MOD_BASE_LIGHT_LOAD_REG		0x0003
#define MFC_DC_CURR_MOD_DEPTH_REG			0x0003
#define MFC_EXIT_CLOAK_REASON_REG			0x002f
/************only for E3************/

#define MFC_CHIP_ID_L_REG				0x00F1//	0x00F1//0x1258
#define MFC_CHIP_ID_H_REG				0x00F0//	0x00F0//0x1259
#define MFC_CHIP_REVISION_REG				0x125A
#define MFC_CUSTOMER_ID_REG					0x125B
#define MFC_FW_MAJOR_REV_L_REG				0x125C
#define MFC_FW_MAJOR_REV_H_REG				0x125D
#define MFC_FW_MINOR_REV_L_REG				0x125E
#define MFC_FW_MINOR_REV_H_REG				0x125F
#define MFC_PRMC_ID_L_REG					0x1278
#define MFC_PRMC_ID_H_REG					0x1279

/* RXID BIT[0:47] */
#define MFC_WPC_RXID_0_REG					0x1288
#define MFC_WPC_RXID_1_REG					0x1289
#define MFC_WPC_RXID_2_REG					0x128A
#define MFC_WPC_RXID_3_REG					0x128B
#define MFC_WPC_RXID_4_REG					0x128C
#define MFC_WPC_RXID_5_REG					0x128D

#define MFC_STATUS_L_REG					0x1274//E3:0x1274; Q5:0x1276
#define MFC_STATUS_H_REG					0x1275//E3:0x1275; Q5:0x1277
#define MFC_STATUS_L_REG1					0x1276//E3:0x1276; Q5:null--------
#define MFC_STATUS_H_REG1					0x1277//E3:0x1277; Q5:null--------

#define MFC_INT_A_L_REG						0x1270//E3:0x1270; Q5:0x1274
#define MFC_INT_A_H_REG						0x1271//E3:0x1271; Q5:0x1275
#define MFC_INT_A_L_REG1					0x1272//E3:0x1272; Q5:null--------
#define MFC_INT_A_H_REG1					0x1273//E3:0x1273; Q5:null--------

#define MFC_INT_A_ENABLE_L_REG				0x0004//E3:0x0004; Q5:0x1200
#define MFC_INT_A_ENABLE_H_REG				0x0005//E3:0x0005; Q5:0x1201
#define MFC_INT_A_ENABLE_L_REG1				0x0006//E3:0x0006; Q5:null--------
#define MFC_INT_A_ENABLE_H_REG1				0x0007//E3:0x0007; Q5:null--------

#define MFC_INT_A_CLEAR_L_REG				0x1200//E3:0x1200; Q5:0x1202
#define MFC_INT_A_CLEAR_H_REG				0x1201//E3:0x1201; Q5:0x1203
#define MFC_INT_A_CLEAR_L_REG1				0x1202//E3:0x1202; Q5:null--------
#define MFC_INT_A_CLEAR_H_REG1				0x1203//E3:0x1203; Q5:null--------

//#define MFC_CTRL_STS_REG					0x28  // no used in by Nuvolta

#define MFC_SYS_OP_MODE_REG					0x1268//E3:0x1268; Q5:0x126B
#define MFC_BATTERY_CHG_STATUS_REG			0x0008//E3:0x0008; Q5:0x1214
#define MFC_EPT_REG					0x0009//E3:0x0009; Q5:0x1215 /* EPT(End of Power Transfer) cases. PMA has only EOC case */
#define MFC_ADC_VOUT_L_REG					0x127A
#define MFC_ADC_VOUT_H_REG					0x127B

#define MFC_VOUT_SET_L_REG					0x000A//E3:0x000A; Q5:0x1216
//#define MFC_VOUT_SET_H_REG					0x1217
#define MFC_VRECT_ADJ_REG					0x000B//E3:0x000B; Q5:0x1218
#define MFC_ADC_VRECT_L_REG					0x127C
#define MFC_ADC_VRECT_H_REG					0x127D

#if NU1668
#define MFC_ADC_TX_ISENSE_L_REG				0x1282
#define MFC_ADC_TX_ISENSE_H_REG				0x1283
#else	// cps case
#define MFC_ADC_IRECT_L_REG					0x42
#define MFC_ADC_IRECT_H_REG					0x43
#endif

#define MFC_TX_IUNO_LIMIT_L_REG				0x123C//E3:0x123C; Q5:0x1238
#define MFC_TX_IUNO_LIMIT_H_REG				0x123D//E3:0x123D; Q5:0x1239
#define MFC_ADC_IOUT_L_REG					0x127E
#define MFC_ADC_IOUT_H_REG					0x127F
#define MFC_ADC_DIE_TEMP_L_REG				0x1280  /* 8 LSB field is used, Celsius */
#define MFC_ADC_DIE_TEMP_H_REG				0x1281  /* only 4 MSB[3:0] field is used, Celsius */
#define MFC_TRX_OP_FREQ_L_REG				0x1284 /* kHZ */
#define MFC_TRX_OP_FREQ_H_REG				0x1285 /* kHZ */
#define MFC_RX_PING_FREQ_L_REG				0x1286 /* kHZ */
#define MFC_RX_PING_FREQ_H_REG				0x1287 /* kHZ */
#define MFC_ILIM_SET_REG					0x000C//E3:0x000C; Q5:0x1219 /* ILim =  value * 0.1(A) + 0.1(A). 1.3A = 12*0.1 + 0.1 */
//#define MFC_ILIM_ADJ_REG					0x121A // Unused

#define MFC_AP2MFC_CMD_L_REG				0x1204
#define MFC_AP2MFC_CMD_H_REG				0x1205

/********************************************************************************/
/* Below register are functionally depends on the operation mode(TX or RX mode) */
/* RX mode */
#define MFC_WPC_PCKT_HEADER_REG				0x1208
#define MFC_WPC_RX_DATA_COM_REG         	0x1209 /* WPC Rx to Tx COMMAND */
#define MFC_WPC_RX_DATA_VALUE0_REG         	0x120A
#define MFC_WPC_RX_DATA_VALUE1_REG         	0x120B
#define MFC_WPC_RX_DATA_VALUE2_REG         	0x120C
#define MFC_WPC_RX_DATA_VALUE3_REG         	0x120D
#define MFC_WPC_RX_DATA_VALUE4_REG			0x120E//only for E3---------
#define MFC_WPC_RX_DATA_VALUE5_REG			0x120F//only for E3---------
#define MFC_WPC_RX_DATA_VALUE6_REG			0x1210//only for E3---------
#define MFC_WPC_RX_DATA_VALUE7_REG			0x1211//only for E3---------

/* TX mode */
#define MFC_WPC_TX_DATA_COM_REG				0x1208 /* WPC Tx to Rx COMMAND */
#define MFC_WPC_TX_DATA_VALUE0_REG         	0x1209 
#define MFC_WPC_TX_DATA_VALUE1_REG         	0x120A
#define MFC_WPC_TX_DATA_VALUE2_REG         	0x120B

/* Common */
#define MFC_WPC_TRX_DATA2_COM_REG			0x1214//E3:0x1214; Q5:0x1210
#define MFC_WPC_TRX_DATA2_VALUE0_REG		0x1215//E3:0x1215; Q5:0x1211
#define MFC_WPC_TRX_DATA2_VALUE1_REG		0x1216//E3:0x1216; Q5:0x1212
#define MFC_WPC_TRX_DATA2_VALUE2_REG		0x1217//only for E3---------
#define MFC_WPC_TRX_DATA2_VALUE3_REG		0x1218//only for E3---------
#define MFC_WPC_TRX_DATA2_VALUE4_REG		0x1219//only for E3---------
#define MFC_WPC_TRX_DATA2_VALUE5_REG		0x121A//only for E3---------
#define MFC_WPC_TRX_DATA2_VALUE6_REG		0x121B//only for E3---------
#define MFC_WPC_TRX_DATA2_VALUE7_REG		0x121C//only for E3---------
/********************************************************************************/

#define MFC_ADT_TIMEOUT_PKT_REG				0x1220//E3:0x1220; Q5:0x121B
#define MFC_ADT_TIMEOUT_STR_REG				0x1221//E3:0x1221; Q5:0x121C

#define MFC_TX_IUNO_HYS_REG					0x123E//E3:0x123E; Q5:0x123A
#define MFC_TX_IUNO_OFFSET_L_REG			0x1240//E3:0x1240; Q5:0x123C
#define MFC_TX_IUNO_OFFSET_H_REG			0x1241//E3:0x1241; Q5:0x123D

#define MFC_TX_OC_FOD1_LIMIT_L_REG			0x124F//E3:0x124F; Q5:0x124C
//#define MFC_TX_OC_FOD1_LIMIT_H_REG			0x124D
#define MFC_TX_OC_FOD2_LIMIT_L_REG			0x1250//E3:0x1250; Q5:0x124E
//#define MFC_TX_OC_FOD2_LIMIT_H_REG			0x124F

#define MFC_STARTUP_EPT_COUNTER			    0x128F

#define MFC_TX_DUTY_CYCLE					0x128E /* default 0x80(50%) */

/* TX Max Operating Frequency = 96 MHz/value, default is 148kHz (96MHz/0x288=148KHz) */
#define MFC_TX_MAX_OP_FREQ_L_REG			0x1248//E3:0x1248; Q5:0x1244 /* default 0x88 */
#define MFC_TX_MAX_OP_FREQ_H_REG			0x1249//E3:0x1249; Q5:0x1245 /* default 0x02 */
/* TX Min Operating Frequency = 96 MHz/value, default is 113kHz (96MHz/0x351=113KHz) */
#define MFC_TX_MIN_OP_FREQ_L_REG			0x124A//E3:0x124A; Q5:0x1246 /* default 0x51 */
#define MFC_TX_MIN_OP_FREQ_H_REG			0x124B//E3:0x124B; Q5:0x1247 /* default 0x03 */
/* TX Digital Ping Frequency = 96 MHz/value, default is 145kHz (96MHz/0x296=145KHz) */
#define MFC_TX_PING_FREQ_L_REG				0x124C//E3:0x124C; Q5:0x1248 /* default 0x96 */
#define MFC_TX_PING_FREQ_H_REG				0x124D//E3:0x124D; Q5:0x1249 /* default 0x02 */
/* TX Mode Minimum Duty Setting Register, Min_Duty, default is 20%. value/256 * 100(0x33=20) */
#define MFC_TX_MIN_DUTY_SETTING_REG			0x124E//E3:0x124E; Q5:0x124A /* default 0x33 */

#define MFC_INVERTER_CTRL_REG				0x124B  // not implemented. P2P HALF : 0x4B[7]
#define MFC_CMFET_CTRL_REG					0x1223//E3:0x1223; Q5:0x121E

/* RX Mode Communication Modulation FET Ctrl */
#define MFC_MST_MODE_SEL_REG				0x1255
#define MFC_RX_OV_CLAMP_REG					0x1224//E3:0x1224; Q5:0x121F
//#define MFC_RX_COMM_MOD_FET_REG				0x1220 // unused in a code. Not used in Nuvolta

#define MFC_RECTMODE_REG					0x1223
#define MFC_START_EPT_COUNTER_REG			0x128F // In a code, MFC_START_EPT_COUNTER is used. need to check.
//#define MFC_CTRL_MODE_REG					0x6E // this is not used in a code. Need to check the operation fo this register from Samsung.
#define MFC_TX_FOD_CTRL_REG					0x1245//E3:0x1245; Q5:0x1241 // this ?
#define MFC_RC_PHM_PING_PERIOD_REG			0x1226//E3:0x1226; Q5:0x1221 //in RX mode. Not Implemented.

#define MIN_DUTY_SETTING_20_DATA	20
#define MIN_DUTY_SETTING_30_DATA	30
#define MIN_DUTY_SETTING_50_DATA	50

#define MFC_WPC_FOD_0A_REG					0x1228//E3:0x1228; Q5:0x1224
#define MFC_WPC_FOD_0B_REG					0x1229//E3:0x1229; Q5:0x1225
#define MFC_WPC_FOD_1A_REG					0x122A//E3:0x122A; Q5:0x1226
#define MFC_WPC_FOD_1B_REG					0x122B//E3:0x122B; Q5:0x1227
#define MFC_WPC_FOD_2A_REG					0x122C//E3:0x122C; Q5:0x1228
#define MFC_WPC_FOD_2B_REG					0x122D//E3:0x122D; Q5:0x1229
#define MFC_WPC_FOD_3A_REG					0x122E//E3:0x122E; Q5:0x122A
#define MFC_WPC_FOD_3B_REG					0x122F//E3:0x122F; Q5:0x122B
#define MFC_WPC_FOD_4A_REG					0x1230//E3:0x1230; Q5:0x122C
#define MFC_WPC_FOD_4B_REG					0x1231//E3:0x1231; Q5:0x122D
#define MFC_WPC_FOD_5A_REG					0x1232//E3:0x1232; Q5:0x122E
#define MFC_WPC_FOD_5B_REG					0x1233//E3:0x1233; Q5:0x122F
#define MFC_WPC_FOD_6A_REG					0x1234//E3:0x1234; Q5:0x1230
#define MFC_WPC_FOD_6B_REG					0x1235//E3:0x1235; Q5:0x1231
#define MFC_WPC_FOD_7A_REG					0x1236//E3:0x1236; Q5:0x1232
#define MFC_WPC_FOD_7B_REG					0x1237//E3:0x1237; Q5:0x1233
#define MFC_WPC_FOD_8A_REG					0x1238//E3:0x1238; Q5:0x1234
#define MFC_WPC_FOD_8B_REG					0x1239//E3:0x1239; Q5:0x1235
#define MFC_WPC_FOD_9A_REG					0x123A//E3:0x123A; Q5:0x1236
#define MFC_WPC_FOD_9B_REG					0x123B//E3:0x123B; Q5:0x1237

//#define MFC_WPC_PARA_MODE_REG				0x8C  // not used in a code. Need to check the operation of this regiser from Samsung

// used in a code. But, Nuvolta doesn't have these registers. Need to check from Samsung
#if 0
#define MFC_WPC_FWC_FOD_0A_REG				0x100
#define MFC_WPC_FWC_FOD_0B_REG				0x101
#define MFC_WPC_FWC_FOD_1A_REG				0x102
#define MFC_WPC_FWC_FOD_1B_REG				0x103
#define MFC_WPC_FWC_FOD_2A_REG				0x104
#define MFC_WPC_FWC_FOD_2B_REG				0x105
#define MFC_WPC_FWC_FOD_3A_REG				0x106
#define MFC_WPC_FWC_FOD_3B_REG				0x107
#define MFC_WPC_FWC_FOD_4A_REG				0x108
#define MFC_WPC_FWC_FOD_4B_REG				0x109
#define MFC_WPC_FWC_FOD_5A_REG				0x10A
#define MFC_WPC_FWC_FOD_5B_REG				0x10B
#define MFC_WPC_FWC_FOD_6A_REG				0x10C
#define MFC_WPC_FWC_FOD_6B_REG				0x10D
#define MFC_WPC_FWC_FOD_7A_REG				0x10E
#define MFC_WPC_FWC_FOD_7B_REG				0x10F
#define MFC_WPC_FWC_FOD_8A_REG				0x100
#define MFC_WPC_FWC_FOD_8B_REG				0x101
#define MFC_WPC_FWC_FOD_9A_REG				0x102
#define MFC_WPC_FWC_FOD_9B_REG				0x103

#define MFC_PMA_FOD_0A_REG					0x84
#define MFC_PMA_FOD_0B_REG					0x85
#define MFC_PMA_FOD_1A_REG					0x86
#define MFC_PMA_FOD_1B_REG					0x87
#define MFC_PMA_FOD_2A_REG					0x88
#define MFC_PMA_FOD_2B_REG					0x89
#define MFC_PMA_FOD_3A_REG					0x8A
#define MFC_PMA_FOD_3B_REG					0x8B
#endif
// used in a code. But, Nuvolta doesn't have these registers. Need to check from Samsung

#define MFC_ADT_ERROR_CODE_REG				0x1297

#define MFC_TX_FOD_GAIN_REG					0x1253//E3:0x1253; Q5:0x1254
#define MFC_TX_FOD_OFFSET_L_REG				0x1251//E3:0x1251; Q5:0x1250
//#define MFC_TX_FOD_OFFSET_H_REG				0x1251
#define MFC_TX_FOD_THRESH1_L_REG			0x1252//E3:0x1252; Q5:0x1252
//#define MFC_TX_FOD_THRESH1_H_REG			0x1253

// for FOD threshold setting, there are two functions.
// mfc_set_tx_fod_thresh1() and mfc_set_tx_fod_ta_thresh().  buds uses two functions. phone uses mfc_set_tx_fod_thresh1().
// Nuvolta doesn't have MFC_TX_FOD_TA_THRESH_L/H_REG. Need to check Samsung.
//#define MFC_TX_FOD_TA_THRESH_L_REG			0x98
//#define MFC_TX_FOD_TA_THRESH_H_REG			0x99


// MFX(Reference) or MFC(Nuvolta). need to check Samsung.
#define MFC_TX_ID_VALUE_L_REG				0x1246//E3:0x1246; Q5:0x1242
#define MFC_TX_ID_VALUE_H_REG				0x1247//E3:0x1247; Q5:0x1243

#define MFC_DEMOD1_REG						0x1242//E3:0x1242; Q5:0x123E
#define MFC_DEMOD2_REG						0x1243//E3:0x1243; Q5:0x123F

#define MFC_PWR_HOLD_INTERVAL_REG			0x1244//E3:0x1244; Q5:0x1240
// Nuvolta doesn't have this register. Need to check Samsung.
// used in mfc_set_tx_conflict_current().
//#define MFC_TX_CONFLICT_CURRENT_REG			0xA0
//#define MFC_RECT_MODE_AP_CTRL				0xA2

#define MFC_CEP_TIME_OUT_REG				0x123F//E3:0x123F; Q5:0x123B

// check if DATA is ok or DATE is ok from Samsung.
#define MFC_FW_DATE_CODE_0					0x1260
#define MFC_FW_DATE_CODE_1					0x1261
#define MFC_FW_DATE_CODE_2					0x1262
#define MFC_FW_DATE_CODE_3					0x1263
#define MFC_FW_DATE_CODE_4					0x1264
#define MFC_FW_DATE_CODE_5					0x1265
#define MFC_FW_DATE_CODE_6					0x1266
#define MFC_FW_DATE_CODE_7					0x1267
//#define MFC_FW_DATE_CODE_8					0x1268
//#define MFC_FW_DATE_CODE_9					0x1269
//#define MFC_FW_DATE_CODE_A					0x126A

/* Timer code contains ASCII value. (ex. 31 means '1', 3A means ':') */
#define MFC_FW_TIMER_CODE_0					0x1269//E3:0x1269; Q5:0x126C
#define MFC_FW_TIMER_CODE_1					0x126A//E3:0x126A; Q5:0x126D
#define MFC_FW_TIMER_CODE_2					0x126B//E3:0x126B; Q5:0x126E
#define MFC_FW_TIMER_CODE_3					0x126C//E3:0x126C; Q5:0x126F
#define MFC_FW_TIMER_CODE_4					0x126D//E3:0x126D; Q5:0x1270
#define MFC_FW_TIMER_CODE_5					0x126E//E3:0x126E; Q5:0x1271
//#define MFC_FW_TIMER_CODE_6					0x1272
//#define MFC_FW_TIMER_CODE_7					0x1273

#define MFC_TX_WPC_AUTH_SUPPORT_REG	        0x126F

#define MFC_RX_PWR_L_REG					0x1290
#define MFC_RX_PWR_H_REG					0x1291


//#define MFC_TX_FOD_THRESH2_REG			0xE3
//#define MFC_TX_DUTY_CYCLE_REG				0xE6


// Not used in code. Need to check from Samsung if the function will be created for the usage of this register.
//#define MFC_TX_PWR_L_REG					0xEC
//#define MFC_TX_PWR_H_REG					0xED
#define MFC_TX_PWR_L_REG					0x1292
#define MFC_TX_PWR_H_REG					0x1293

#define MFC_RPP_SCALE_COEF_REG				0x1227//E3:0x1227; Q5:0x1222


//#define MFC_ACTIVE_LOAD_CONTROL_REG		0xF1
/* Parameter 1: Major and Minor Version */
#define MFC_TX_RXID1_READ_REG				0x1294
/* Parameter 2~3: Manufacturer Code */
#define MFC_TX_RXID2_READ_REG				0x1295
#define MFC_TX_RXID3_READ_REG				0x1296

/* Parameter QFOD */
#define MFC_TX_QAIR_L_REG					0x12A4
#define MFC_TX_QAIR_H_REG					0x12A5
#define MFC_TX_Q_L_REG						0x12A6
#define MFC_TX_Q_H_REG						0x12A7

#define MFC_TX_Q_SSPTH_REG					0x000D//E3:0x000D; Q5:0x121A
#define MFC_TX_Q_BASE_L_REG					0x1254//E3:0x1254; Q5:0x129C
//#define MFC_TX_Q_BASE_H_REG					0x129D
#define MFC_TX_COEFF_QFOD_TH0_REG			0x1222//E3:0x1222; Q5:0x121D
#define MFC_TX_COEFF_QFOD_TH1_REG			0x1225//E3:0x1225; Q5:0x1220
#define MFC_TX_COEFF_QFOD_TH2_REG			0x1256
#define MFC_TX_COEFF_QFOD_TH3_REG			0x1257
#define MFC_TX_COEFF_QFOD_TWS_TH3_L_REG		0x129D//E3:0x129D; Q5:0x129E
//#define MFC_TX_COEFF_QFOD_TWS_TH3_H_REG	0x129F
#define MFC_TX_COEFF_QFOD_PHONE_TH4_L_REG	0x129C//E3:0x129C; Q5:0x12A0
//#define MFC_TX_COEFF_QFOD_PHONE_TH4_H_REG	0x12A1
#define MFC_EPP_REF_FREQ_VALUE_REG			0x129E//NEW

#define MFC_MPP_FOD_QF_REG	MFC_TX_Q_BASE_L_REG
#define MFC_MPP_FOD_RF_REG	MFC_EPP_REF_FREQ_VALUE_REG

/* Parameter 4~10: Basic Device Identifier */
//#define MFC_TX_RXID4_READ_REG					0xF5
//#define MFC_TX_RXID5_READ_REG					0xF6
//#define MFC_TX_RXID6_READ_REG					0xF7
//#define MFC_TX_RXID7_READ_REG					0xF8
//#define MFC_TX_RXID8_READ_REG					0xF9
//#define MFC_TX_RXID9_READ_REG					0xFA
//#define MFC_TX_RXID10_READ_REG				0xFB


#define SS_ID		0x42
#define SS_CODE		0x64

/* Cloak reason Register, CLOAK_REASON (0x0000) */
#define MFC_TRX_MPP_CLOAK_GENERIC			    0x0
#define MFC_TRX_MPP_CLOAK_FORCED			    0x1
#define MFC_TRX_MPP_CLOAK_THERMALLY_CONSTRAINED 0x2
#define MFC_TRX_MPP_CLOAK_INSUFFICIENT_POWER    0x3
#define MFC_TRX_MPP_CLOAK_COEX_MITIGATION	    0x4
#define MFC_TRX_MPP_CLOAK_END_OF_CHARGE			0x5
#define MFC_TRX_MPP_CLOAK_PTX_INITIATED			0x6

/* nego power level Register, POWER_LEVEL_SETTING (0x0002) */
#define MFC_RX_MPP_NEGO_POWER_10W           10
#define MFC_RX_MPP_NEGO_POWER_15W           15

//#define TX_ID_CHECK_CNT					10

/* Target Vrect is ReadOnly register, and updated by every 10ms
 * Its default value is 0x1A90(6800mV).
 * Target_Vrect (Iout,Vout) = {Vout + 0.05} + { Vrect(Iout,5V)-Vrect(1A,5V) } * 5/9
 */

// not used in code. Nuvolta doesn't have this register. Need to check from Samsung.
#if 0
#define MFC_TARGET_VRECT_L_REG				0x0164 /* default 0x90 */
#define MFC_TARGET_VRECT_H_REG				0x0165 /* default 0x1A */

#define MFC_RX_CEP_PACKET_COUNTER0			0x029C
#define MFC_RX_CEP_PACKET_COUNTER1			0x029D
#define MFC_RX_CEP_PACKET_COUNTER2			0x029E
#define MFC_RX_CEP_PACKET_COUNTER3			0x029F
#define MFC_RX_RPP_PACKET_COUNTER0			0x02A0
#define MFC_RX_RPP_PACKET_COUNTER1			0x02A1
#define MFC_RX_RPP_PACKET_COUNTER2			0x02A2
#define MFC_RX_RPP_PACKET_COUNTER3			0x02A3
#define MFC_RX_CSP_PACKET_COUNTER0			0x02A4
#define MFC_RX_CSP_PACKET_COUNTER1			0x02A5
#define MFC_RX_CSP_PACKET_COUNTER2			0x02A6
#define MFC_RX_CSP_PACKET_COUNTER3			0x02A7
#define MFC_RX_PPP_PACKET_COUNTER0			0x02A8
#define MFC_RX_PPP_PACKET_COUNTER1			0x02A9
#define MFC_RX_PPP_PACKET_COUNTER2			0x02AA
#define MFC_RX_PPP_PACKET_COUNTER3			0x02AB
#endif

/* ADT Buffer Registers, (0x12A8 ~ 0x12FF) */
#define MFC_ADT_BUFFER_ADT_TYPE_REG				0x12A8
#define MFC_ADT_BUFFER_ADT_MSG_SIZE_REG			0x12A9
#define MFC_ADT_BUFFER_ADT_PARAM_REG			0x12AA
#define MFC_ADT_BUFFER_ADT_PARAM_MAX_REG		0x12FF

/* System Operating Mode Register, Sys_Op_Mode (0x126B) */
#define PAD_RX_MODE_AC_MISSING			0
#define PAD_RX_MODE_WPC_BPP				1
#define PAD_RX_MODE_WPC_EPP				2
#define PAD_RX_MODE_WPC_MPP_RESTRICT	3
#define PAD_RX_MODE_WPC_MPP_FULL		4
#define PAD_RX_MODE_WPC_MPP_CLOAK		5
#define PAD_RX_MODE_WPC_MPP_NEGO		6
#define PAD_RX_MODE_WPC_EPP_NEGO		7

#define PAD_TX_MODE_BACK_POWER_MISSING		0
#define PAD_TX_MODE_TX_POWERLOSS_FOD		2
#define PAD_TX_MODE_TX_PINGOC_FOD			3
#define PAD_TX_MODE_DCR_MST_ON				4
#define PAD_TX_MODE_PCR_MST_ON				5
#define PAD_TX_MODE_TX_MODE_ON				8
#define PAD_TX_MODE_TX_CONFLICT				9
#define PAD_TX_MODE_TX_PH_MODE				10
#define PAD_TX_MODE_TX_QTH0_FOD				11
#define PAD_TX_MODE_TX_QTH2_FOD				12
#define PAD_TX_MODE_TX_WATCH_ITH1_ITH2_FOD	13
#define PAD_TX_MODE_TX_TWS_ITH3_FOD			14
#define PAD_TX_MODE_TX_PHONE_ITH4_FOD		15

// below is used in the reference code. Need to change .c with the above code after review the function.
#define PAD_MODE_MISSING				0
#define PAD_MODE_WPC_BASIC				1
#define PAD_MODE_WPC_ADV				2
#define PAD_MODE_PMA_SR1				3
#define PAD_MODE_PMA_SR1E				4

#define PAD_MODE_UNKNOWN				5  // this is not used in the code

#if 0
/* MFC_RX_DATA_COM_REG (0x51) : RX Data Command VALUE of 0x19 PPP Heaader */
#define	WPC_COM_CLEAR_PACKET_COUNTING		0x01
#define	WPC_COM_START_PACKET_COUNTING		0x02
#define	WPC_COM_DISABLE_PACKET_COUNTING		0x03
#endif

/* RX Data Value1 Register (Data Sending), RX_Data_VALUE1_Out (0x51) : Function and Description : Reference Code */
/* RX Data Command MFC_WPC_RX_DATA_COM_REG */
#define	WPC_COM_UNKNOWN					0x00
#define	WPC_COM_TX_ID					0x01
#define	WPC_COM_CHG_STATUS				0x05
#define	WPC_COM_AFC_SET					0x06
#define	WPC_COM_AFC_DEBOUNCE			0x07 /* Data Values [ 0~1000mV : 0x0000~0x03E8 ], 2 bytes*/
#define	WPC_COM_SID_TAG					0x08
#define	WPC_COM_SID_TOKEN				0x09
#define	WPC_COM_TX_STANDBY				0x0A
#define	WPC_COM_LED_CONTROL				0x0B /* Data Value LED Enable(0x00), LED Disable(0xFF) */
#define	WPC_COM_REQ_AFC_TX				0x0C /* Data Value (0x00) */
#define	WPC_COM_COOLING_CTRL			0x0D /* Data Value ON(0x00), OFF(0xFF) */
#define	WPC_COM_RX_ID					0x0E /* Received RX ID */
#define	WPC_COM_CHG_LEVEL				0x0F /* Battery level */
#define	WPC_COM_ENTER_PHM				0x18 /* GEAR entered PHM */
#define	WPC_COM_DISABLE_TX				0x19 /* Turn off UNO of TX, OFF(0xFF) */
#define	WPC_COM_PAD_LED					0x20 /* PAD LED */
#define	WPC_COM_REQ_PWR_BUDG			0x21
#define	WPC_COM_OP_FREQ_SET				0xD1
#define	WPC_COM_WDT_ERR					0xE7 /* Data Value WDT Error */

/* RX Data Value 2~5 Register (Data Sending), RX_Data_Value2_5_Out : Function and Description */
#define	RX_DATA_VAL2_5V					0x05
#define	RX_DATA_VAL2_10V				0x2C
#define	RX_DATA_VAL2_12V				0x4B
#define	RX_DATA_VAL2_12_5V				0x69
#define	RX_DATA_VAL2_20V				0x9B
#define	RX_DATA_VAL2_TA_CONNECT_DURING_WC		0x55
#define	RX_DATA_VAL2_MISALIGN				0xFF
#define	RX_DATA_VAL2_ENABLE				0x01

#define	RX_DATA_VAL2_RXID_ACC_BUDS			0x70
#define	RX_DATA_VAL2_RXID_ACC_BUDS_MAX		0x8F

/* MFC_TX_DATA_COM_REG (0x58) : TX Command */
#define	WPC_TX_COM_UNKNOWN		0x00
#define	WPC_TX_COM_TX_ID		0x01
#define	WPC_TX_COM_AFC_SET		0x02
#define	WPC_TX_COM_ACK			0x03
#define	WPC_TX_COM_NAK			0x04
#define WPC_TX_COM_CHG_ERR		0x05
#define WPC_TX_COM_WPS		0x07
#define WPC_TX_COM_RX_POWER		0x0A
#define WPC_TX_COM_TX_PWR_BUDG	0x0C

/* value of WPC_TX_COM_AFC_SET(0x02) */
#define TX_AFC_SET_5V			0x00
#define TX_AFC_SET_10V			0x01
#define TX_AFC_SET_12V			0x02
#define TX_AFC_SET_18V			0x03
#define TX_AFC_SET_19V			0x04
#define TX_AFC_SET_20V			0x05
#define TX_AFC_SET_24V			0x06

/* value of WPC_TX_COM_TX_ID(0x01) */
#define TX_ID_UNKNOWN				0x00
#define TX_ID_SNGL_PORT_START		0x01
#define TX_ID_VEHICLE_PAD			0x11
#define TX_ID_PG950_D_PAD			0x14
#define TX_ID_P1100_PAD				0x16
#define TX_ID_P1300_PAD			0x17
#define TX_ID_P2400_PAD_NOAUTH			0x18
#define TX_ID_SNGL_PORT_END			0x1F
#define TX_ID_MULTI_PORT_START		0x20
#define TX_ID_P4300_PAD			0x25
#define TX_ID_P5400_PAD_NOAUTH			0x27
#define TX_ID_MULTI_PORT_END		0x2F
#define TX_ID_STAND_TYPE_START		0x30
#define TX_ID_PG950_S_PAD			0x31
#define TX_ID_STAR_STAND			0x32
#define TX_ID_N3300_V_PAD			0x35
#define TX_ID_N3300_H_PAD			0xF2
#define TX_ID_STAND_TYPE_END		0x3F
#define TX_ID_BATT_PACK_TA			0x41 /* 0x40 ~ 0x41 is N/C*/
#define TX_ID_BATT_PACK_U1200		0x42
#define TX_ID_BATT_PACK_U3300		0x43
#define TX_ID_BATT_PACK_END			0x4F /* reserved 0x40 ~ 0x4F for wireless battery pack */
#define TX_ID_UNO_TX				0x72
#define TX_ID_UNO_TX_B0				0x80
#define TX_ID_UNO_TX_B1				0x81
#define TX_ID_UNO_TX_B2				0x82
#define TX_ID_UNO_TX_MAX			0x9F

#define TX_ID_AUTH_PAD				0xA0
#define TX_ID_P5200_PAD				0xA0
#define TX_ID_N5200_V_PAD			0xA1
#define TX_ID_N5200_H_PAD			0xA2
#define TX_ID_P2400_PAD				0xA3
#define TX_ID_P5400_PAD				0xA4
#define TX_ID_AUTH_PAD_ACLASS_END	0xAF
#define TX_ID_AUTH_PAD_END			0xBF /* reserved 0xA1 ~ 0xBF for auth pad */
#define TX_ID_JIG_PAD				0xED /* for factory */
#define TX_ID_FG_PAD				0xEF /* Galaxy Friends */
#define TX_ID_NON_AUTH_PAD			0xF0
#define TX_ID_NON_AUTH_PAD_END		0xFF

/* value of WPC_TX_COM_CHG_ERR(0x05) */
#define TX_CHG_ERR_OTP			0x12
#define TX_CHG_ERR_OCP			0x13
#define TX_CHG_ERR_DARKZONE		0x14
#define TX_CHG_ERR_FOD			(0x20 ... 0x27)

/* value of WPC_TX_COM_WPS 0x07) */
#define WPS_AICL_RESET		0x01

/* value of WPC_TX_COM_RX_POWER(0x0A) */
#define TX_RX_POWER_0W			0x0
#define TX_RX_POWER_3W			0x1E
#define TX_RX_POWER_5W			0x32
#define TX_RX_POWER_6W			0x3C
#define TX_RX_POWER_6_5W		0x41
#define TX_RX_POWER_7_5W		0x4B
#define TX_RX_POWER_8W			0x50
#define TX_RX_POWER_10W			0x64
#define TX_RX_POWER_12W			0x78
#define TX_RX_POWER_15W			0x96
#define TX_RX_POWER_17_5W		0xAF
#define TX_RX_POWER_20W			0xC8

#define MFC_NUM_FOD_REG					20

/* BIT DEFINE of Command Register, COM_L(0x1204) */ // Done. Nu1668
#define MFC_CMD_TOGGLE_PHM_SHIFT			7
#define MFC_CMD_RESERVED6_SHIFT				6
#define MFC_CMD_CLEAR_INT_SHIFT				5
#define MFC_CMD_SEND_CHG_STS_SHIFT			4
#define MFC_CMD_SEND_EOP_SHIFT				3
#define MFC_CMD_MCU_RESET_SHIFT				2
#define MFC_CMD_TOGGLE_LDO_SHIFT			1
#define MFC_CMD_SEND_TRX_DATA_SHIFT			0
#define MFC_CMD_TOGGLE_PHM_MASK				(1 << MFC_CMD_TOGGLE_PHM_SHIFT)
#define MFC_CMD_RESERVED6_MASK				(1 << MFC_CMD_RESERVED6_SHIFT)
#define MFC_CMD_CLEAR_INT_MASK				(1 << MFC_CMD_CLEAR_INT_SHIFT)
#define MFC_CMD_SEND_CHG_STS_MASK			(1 << MFC_CMD_SEND_CHG_STS_SHIFT) /* MFC MCU sends ChgStatus packet to TX */
#define MFC_CMD_SEND_EOP_MASK				(1 << MFC_CMD_SEND_EOP_SHIFT)
#define MFC_CMD_MCU_RESET_MASK				(1 << MFC_CMD_MCU_RESET_SHIFT)
#define MFC_CMD_TOGGLE_LDO_MASK				(1 << MFC_CMD_TOGGLE_LDO_SHIFT)
#define MFC_CMD_SEND_TRX_DATA_MASK			(1 << MFC_CMD_SEND_TRX_DATA_SHIFT)

/* Command Register, COM_H(0x1205) */ // Done. Nu1668
#define MFC_CMD2_MPP_EXIT_CLOAK_SHIFT			4
#define MFC_CMD2_MPP_EXIT_CLOAK_MASK			(1 << MFC_CMD2_MPP_EXIT_CLOAK_SHIFT)
#define MFC_CMD2_MPP_ENTER_CLOAK_SHIFT			3
#define MFC_CMD2_MPP_ENTER_CLOAK_MASK			(1 << MFC_CMD2_MPP_ENTER_CLOAK_SHIFT)
#define MFC_CMD2_SEND_ADT_SHIFT				1
#define MFC_CMD2_SEND_ADT_MASK				(1 << MFC_CMD2_SEND_ADT_SHIFT)

#if 0
#define MFC_CMD2_WP_ON_SHIFT				0
#define MFC_CMD2_WP_ON_MASK					(1 << MFC_CMD2_WP_ON_SHIFT)

#define MFC_CMD2_ADT_SENT_SHIFT				1
#define MFC_CMD2_ADT_SENT_MASK				(1 << MFC_CMD2_WP_ON_SHIFT)
#endif

/* Chip Revision Register, Chip_Rev (0x125A) */
#define MFC_CHIP_REVISION_MASK				0xFF
//#define MFC_CHIP_FONT_MASK				0x0f


/* BIT DEFINE of Status Registers, Status_L (0x20), Status_H (0x21) */
#define MFC_STAT_L_STAT_VOUT_SHIFT				7
#define MFC_STAT_L_STAT_VRECT_SHIFT				6
#define MFC_STAT_L_OP_MODE_SHIFT				5
#define MFC_STAT_L_OVER_VOL_SHIFT				4
#define MFC_STAT_L_OVER_CURR_SHIFT				3
#define MFC_STAT_L_OVER_TEMP_SHIFT				2
#define MFC_STAT_L_TXCONFLICT_SHIFT				1
#define MFC_STAT_L_ADT_ERROR_SHIFT				0
#define MFC_STAT_L_STAT_VOUT_MASK				(1 << MFC_STAT_L_STAT_VOUT_SHIFT)
#define MFC_STAT_L_STAT_VRECT_MASK				(1 << MFC_STAT_L_STAT_VRECT_SHIFT)
#define MFC_STAT_L_OP_MODE_MASK					(1 << MFC_STAT_L_OP_MODE_SHIFT)
#define MFC_STAT_L_OVER_VOL_MASK				(1 << MFC_STAT_L_OVER_VOL_SHIFT)
#define MFC_STAT_L_OVER_CURR_MASK				(1 << MFC_STAT_L_OVER_CURR_SHIFT)
#define MFC_STAT_L_OVER_TEMP_MASK				(1 << MFC_STAT_L_OVER_TEMP_SHIFT)
#define MFC_STAT_L_TXCONFLICT_MASK				(1 << MFC_STAT_L_TXCONFLICT_SHIFT)
#define MFC_STAT_L_ADT_ERROR_MASK				(1 << MFC_STAT_L_ADT_ERROR_SHIFT)

#define MFC_STAT_H_TRX_DATA_RECEIVED_SHIFT		7
#define MFC_STAT_H_TX_OCP_SHIFT					6
#define MFC_STAT_H_TX_MODE_RX_NOT_DET_SHIFT		5
#define MFC_STAT_H_TX_FOD_SHIFT					4
#define MFC_STAT_H_TX_CON_DISCON_SHIFT			3
#define MFC_STAT_H_AC_MISSING_DET_SHIFT			2
#define MFC_STAT_H_ADT_RECEIVED_SHIFT			1
#define MFC_STAT_H_ADT_SENT_SHIFT				0
#define MFC_STAT_H_TRX_DATA_RECEIVED_MASK		(1 << MFC_STAT_H_TRX_DATA_RECEIVED_SHIFT)
#define MFC_STAT_H_TX_OCP_MASK					(1 << MFC_STAT_H_TX_OCP_SHIFT)
#define MFC_STAT_H_TX_MODE_RX_NOT_DET_MASK		(1 << MFC_STAT_H_TX_MODE_RX_NOT_DET_SHIFT)
#define MFC_STAT_H_TX_FOD_MASK					(1 << MFC_STAT_H_TX_FOD_SHIFT)
#define MFC_STAT_H_TX_CON_DISCON_MASK			(1 << MFC_STAT_H_TX_CON_DISCON_SHIFT)
#define MFC_STAT_H_AC_MISSING_DET_MASK			(1 << MFC_STAT_H_AC_MISSING_DET_SHIFT)
#define MFC_STAT_H_ADT_RECEIVED_MASK			(1 << MFC_STAT_H_ADT_RECEIVED_SHIFT)
#define MFC_STAT_H_ADT_SENT_MASK				(1 << MFC_STAT_H_ADT_SENT_SHIFT)

/* BIT DEFINE of Interrupt_A1 Registers, Status1_L (0x1276), Status1_H (0x1277)  */
//for E3
#define MFC_STAT1_L_EPP_NEGO_FAIL_SHIFT				7
#define MFC_STAT1_L_EPP_NEGO_FAIL_MASK				(1 << MFC_STAT1_L_EPP_NEGO_FAIL_SHIFT)
#define MFC_STAT1_L_EPP_NEGO_PASS_SHIFT				6
#define MFC_STAT1_L_EPP_NEGO_PASS_MASK				(1 << MFC_STAT1_L_EPP_NEGO_PASS_SHIFT)
#define MFC_STAT1_L_EXIT_CLOAK_SHIFT				5
#define MFC_STAT1_L_EXIT_CLOAK_MASK					(1 << MFC_STAT1_L_EXIT_CLOAK_SHIFT)
#define MFC_STAT1_L_DECREASE_POWER_SHIFT			4
#define MFC_STAT1_L_DECREASE_POWER_MASK				(1 << MFC_STAT1_L_DECREASE_POWER_SHIFT)
#define MFC_STAT1_L_INCREASE_POWER_SHIFT			3 //Increase Power
#define MFC_STAT1_L_INCREASE_POWER_MASK				(1 << MFC_STAT1_L_INCREASE_POWER_SHIFT)
#define MFC_STAT1_L_NEGO_PASS_360KHZ_SHIFT			2
#define MFC_STAT1_L_NEGO_PASS_360KHZ_MASK			(1 << MFC_STAT1_L_NEGO_PASS_360KHZ_SHIFT)
#define MFC_STAT1_L_EPP_SUPPORT_SHIFT				1
#define MFC_STAT1_L_EPP_SUPPORT_MASK				(1 << MFC_STAT1_L_EPP_SUPPORT_SHIFT)
#define MFC_STAT1_L_MPP_SUPPORT_SHIFT				0
#define MFC_STAT1_L_MPP_SUPPORT_MASK				(1 << MFC_STAT1_L_MPP_SUPPORT_SHIFT)

/* BIT DEFINE of Interrupt_A Registers, INT_L (0x22), INT_H (0x23) */
#define MFC_INTA_L_STAT_VOUT_SHIFT				7
#define MFC_INTA_L_STAT_VRECT_SHIFT				6
#define MFC_INTA_L_OP_MODE_SHIFT				5
#define MFC_INTA_L_OVER_VOL_SHIFT				4
#define MFC_INTA_L_OVER_CURR_SHIFT				3
#define MFC_INTA_L_OVER_TEMP_SHIFT				2
#define MFC_INTA_L_TXCONFLICT_SHIFT				1
#define MFC_INTA_L_ADT_ERROR_SHIFT				0
#define MFC_INTA_L_STAT_VOUT_MASK				(1 << MFC_INTA_L_STAT_VOUT_SHIFT)
#define MFC_INTA_L_STAT_VRECT_MASK				(1 << MFC_INTA_L_STAT_VRECT_SHIFT)
#define MFC_INTA_L_OP_MODE_MASK					(1 << MFC_INTA_L_OP_MODE_SHIFT)
#define MFC_INTA_L_OVER_VOL_MASK				(1 << MFC_INTA_L_OVER_VOL_SHIFT)
#define MFC_INTA_L_OVER_CURR_MASK				(1 << MFC_STAT_L_OVER_CURR_SHIFT)
#define MFC_INTA_L_OVER_TEMP_MASK				(1 << MFC_STAT_L_OVER_TEMP_SHIFT)
#define MFC_INTA_L_TXCONFLICT_MASK				(1 << MFC_STAT_L_TXCONFLICT_SHIFT)
#define MFC_INTA_L_ADT_ERROR_MASK				(1 << MFC_INTA_L_ADT_ERROR_SHIFT)

#define MFC_INTA_H_TRX_DATA_RECEIVED_SHIFT		7
#define MFC_INTA_H_TX_OCP_SHIFT					6
#define MFC_INTA_H_TX_MODE_RX_NOT_DET			5
#define MFC_INTA_H_TX_FOD_SHIFT					4
#define MFC_INTA_H_TX_CON_DISCON_SHIFT			3
#define MFC_INTA_H_AC_MISSING_DET_SHIFT			2
#define MFC_INTA_H_ADT_RECEIVED_SHIFT			1
#define MFC_INTA_H_ADT_SENT_SHIFT				0
#define MFC_INTA_H_TRX_DATA_RECEIVED_MASK		(1 << MFC_INTA_H_TRX_DATA_RECEIVED_SHIFT)
#define MFC_INTA_H_TX_OCP_MASK					(1 << MFC_INTA_H_TX_OCP_SHIFT)
#define MFC_INTA_H_TX_MODE_RX_NOT_DET_MASK		(1 << MFC_INTA_H_TX_MODE_RX_NOT_DET)
#define MFC_INTA_H_TX_FOD_MASK					(1 << MFC_INTA_H_TX_FOD_SHIFT)
#define MFC_INTA_H_TX_CON_DISCON_MASK			(1 << MFC_INTA_H_TX_CON_DISCON_SHIFT)
#define MFC_INTA_H_AC_MISSING_DET_MASK			(1 << MFC_INTA_H_AC_MISSING_DET_SHIFT)
#define MFC_INTA_H_ADT_RECEIVED_MASK			(1 << MFC_INTA_H_ADT_RECEIVED_SHIFT)
#define MFC_INTA_H_ADT_SENT_MASK				(1 << MFC_INTA_H_ADT_SENT_SHIFT)

/* BIT DEFINE of Interrupt_A1 Registers, INT1_L (0x1272), INT1_H (0x1273);  */
//for E3
#define MFC_INTA1_L_EPP_NEGO_FAIL_SHIFT				7
#define MFC_INTA1_L_EPP_NEGO_FAIL_MASK				(1 << MFC_INTA1_L_EPP_NEGO_FAIL_SHIFT)
#define MFC_INTA1_L_EPP_NEGO_PASS_SHIFT				6
#define MFC_INTA1_L_EPP_NEGO_PASS_MASK				(1 << MFC_INTA1_L_EPP_NEGO_PASS_SHIFT)
#define MFC_INTA1_L_EXIT_CLOAK_SHIFT				5
#define MFC_INTA1_L_EXIT_CLOAK_MASK					(1 << MFC_INTA1_L_EXIT_CLOAK_SHIFT)
#define MFC_INTA1_L_DECREASE_POWER_SHIFT			4
#define MFC_INTA1_L_DECREASE_POWER_MASK				(1 << MFC_INTA1_L_DECREASE_POWER_SHIFT)
#define MFC_INTA1_L_INCREASE_POWER_SHIFT			3 //Increase Power
#define MFC_INTA1_L_INCREASE_POWER_MASK				(1 << MFC_INTA1_L_INCREASE_POWER_SHIFT)
#define MFC_INTA1_L_NEGO_PASS_360KHZ_SHIFT			2
#define MFC_INTA1_L_NEGO_PASS_360KHZ_MASK			(1 << MFC_INTA1_L_NEGO_PASS_360KHZ_SHIFT)
#define MFC_INTA1_L_EPP_SUPPORT_SHIFT				1
#define MFC_INTA1_L_EPP_SUPPORT_MASK				(1 << MFC_INTA1_L_EPP_SUPPORT_SHIFT)
#define MFC_INTA1_L_MPP_SUPPORT_SHIFT				0
#define MFC_INTA1_L_MPP_SUPPORT_MASK				(1 << MFC_INTA1_L_MPP_SUPPORT_SHIFT)

/* System Operating Mode Register, Sys_op_mode(0x2B) */
/* RX MODE[7:5] */
#define MFC_RX_MODE_AC_MISSING					0x0
#define MFC_RX_MODE_WPC_BASIC					0x1
#define MFC_RX_MODE_WPC_ADV						0x2
#define MFC_RX_MODE_PMA_SR1						0x3
#define MFC_RX_MODE_PMA_SR1E					0x4
#define MFC_RX_MODE_RESERVED1					0x5
#define MFC_RX_MODE_RESERVED2					0x6
#define MFC_RX_MODE_UNKNOWN						0x7

/* TX MODE[3:0] */
#define MFC_TX_MODE_RX_MODE						0x0
#define MFC_TX_MODE_MST_MODE1					0x1
#define MFC_TX_MODE_MST_MODE2					0x2
#define MFC_TX_MODE_TX_MODE						0x3
#define MFC_TX_MODE_MST_PCR_MODE1				0x7
#define MFC_TX_MODE_MST_PCR_MODE2				0xF

//#endif
/* TX MODE[3:0] */
#define MFC_TX_MODE_BACK_PWR_MISSING			0x0
#define MFC_TX_MODE_MST_ON						0x4
#define MFC_TX_MODE_TX_MODE_ON					0x8
#define MFC_TX_MODE_TX_ERROR					0x9 /* TX FOD, TX conflict */
#define MFC_TX_MODE_TX_PWR_HOLD					0xA

/* End of Power Transfer Register, EPT (0x3B) (RX only) */
#define MFC_WPC_EPT_UNKNOWN						0
#define MFC_WPC_EPT_END_OF_CHG					1
#define MFC_WPC_EPT_INT_FAULT					2
#define MFC_WPC_EPT_OVER_TEMP					3
#define MFC_WPC_EPT_OVER_VOL					4
#define MFC_WPC_EPT_OVER_CURR					5
#define MFC_WPC_EPT_BATT_FAIL					6
#define MFC_WPC_EPT_RECONFIG					7
#define MFC_WPC_EPT_NO_RESPONSE					8

/* Proprietary Packet Header Register, PPP_Header VALUE(0x1208, MFC_WPC_PCKT_HEADER_REG) */
#define MFC_HEADER_END_SIG_STRENGTH			0x01 /* Message Size 1 */
#define MFC_HEADER_END_POWER_TRANSFER		0x02 /* Message Size 1 */
#define MFC_HEADER_END_CTR_ERROR			0x03 /* Message Size 1 */
#define MFC_HEADER_END_RECEIVED_POWER		0x04 /* Message Size 1 */
#define MFC_HEADER_END_CHARGE_STATUS		0x05 /* Message Size 1 */
#define MFC_HEADER_POWER_CTR_HOLD_OFF		0x06 /* Message Size 1 */
#define MFC_HEADER_PROPRIETARY_1_BYTE		0x18 /* Message Size 1 */
#define MFC_HEADER_PACKET_COUNTING			0x19 /* Message Size 1 */
#define MFC_HEADER_AFC_CONF					0x28 /* Message Size 2 */
#define MFC_HEADER_CONFIGURATION			0x51 /* Message Size 5 */
#define MFC_HEADER_IDENTIFICATION			0x71 /* Message Size 7 */
#define MFC_HEADER_EXTENDED_IDENT			0x81 /* Message Size 8 */

/* END CHARGE STATUS CODES IN WPC */
#define	MFC_ECS_CS100					0x64 /* CS 100 */

/* TX Data Command Register, TX Data_COM VALUE(0x50) */
#define MFC_TX_DATA_COM_TX_ID				0x01

/* END POWER TRANSFER CODES IN WPC */
#define MFC_EPT_CODE_UNKNOWN				0x00
#define MFC_EPT_CODE_CHARGE_COMPLETE		0x01
#define MFC_EPT_CODE_INTERNAL_FAULT		0x02
#define MFC_EPT_CODE_OVER_TEMPERATURE		0x03
#define MFC_EPT_CODE_OVER_VOLTAGE			0x04
#define MFC_EPT_CODE_OVER_CURRENT			0x05
#define MFC_EPT_CODE_BATTERY_FAILURE		0x06
#define MFC_EPT_CODE_RECONFIGURE			0x07
#define MFC_EPT_CODE_NO_RESPONSE			0x08

#define MFC_POWER_MODE_MASK				(0x1 << 0)
#define MFC_SEND_USER_PKT_DONE_MASK		(0x1 << 7)
#define MFC_SEND_USER_PKT_ERR_MASK		(0x3 << 5)
#define MFC_SEND_ALIGN_MASK				(0x1 << 3)
#define MFC_SEND_EPT_CC_MASK			(0x1 << 0)
#define MFC_SEND_EOC_MASK				(0x1 << 0)

#define MFC_PTK_ERR_NO_ERR				0x00
#define MFC_PTK_ERR_ERR					0x01
#define MFC_PTK_ERR_ILLEGAL_HD			0x02
#define MFC_PTK_ERR_NO_DEF				0x03

#define MFC_FW_RESULT_DOWNLOADING		2
#define MFC_FW_RESULT_PASS				1
#define MFC_FW_RESULT_FAIL				0

#define REQ_AFC_DLY	300

#define MFC_FW_MSG		"@MFC_FW "

/* value of TX POWER BUDGET */
#define MFC_TX_PWR_BUDG_NONE		0x00
#define MFC_TX_PWR_BUDG_2W		0x14
#define MFC_TX_PWR_BUDG_5W		0x32
#define MFC_TX_PWR_BUDG_7_5W		0x4B
#define MFC_TX_PWR_BUDG_10W		0x64
#define MFC_TX_PWR_BUDG_12W		0x78
#define MFC_TX_PWR_BUDG_15W		0x96

#define RUNNING             0x66
#define PASS                0x55
#define FAIL                0xAA
#define ILLEGAL             0x40

#if defined(CONFIG_MST_V2)
#define MST_MODE_ON				1		// ON Message to MFC ic
#define MST_MODE_OFF			0		// OFF Message to MFC ic
#define DELAY_FOR_MST			100		// S.LSI : 100 ms
#define MFC_MST_LDO_CONFIG_1	0x7400
#define MFC_MST_LDO_CONFIG_2	0x7409
#define MFC_MST_LDO_CONFIG_3	0x7418
#define MFC_MST_LDO_CONFIG_4	0x3014
#define MFC_MST_LDO_CONFIG_5	0x3405
#define MFC_MST_LDO_CONFIG_6	0x3010
#define MFC_MST_LDO_TURN_ON		0x301c
#define MFC_MST_LDO_CONFIG_8	0x343c
#define MFC_MST_OVER_TEMP_INT	0x0024
#endif

			
/****************************************************************/
/********************* SPECIAL REGISTER MAP *********************/
/****************************************************************/
#define NU1668_GEN_REQUEST_0_REG				0x0000

#define NU1668_GENERAL_REG_000F					0x000F

#define NU1668_MTP_ADDR_0_REG					0x0010
#define NU1668_MTP_ADDR_1_REG					0x0011
#define NU1668_MTP_SECTOR_REG					0x0012
#define NU1668_MTP_DATA_0_REG					0x0013
#define NU1668_MTP_DATA_1_REG					0x0014
#define NU1668_MTP_DATA_2_REG					0x0015
#define NU1668_MTP_DATA_3_REG					0x0016
#define NU1668_MTP_CTRL_0_REG					0x0017
#define NU1668_MTP_CTRL_1_REG					0x0018
#define NU1668_MTP_CTRL_2_REG					0x0019
#define NU1668_MTP_CTRL_OP_REG					0x001A
#define NU1668_MTP_STATUS_REG					0x001B
#define NU1668_MTP_WDATA_0_REG					0x001C
#define NU1668_MTP_WDATA_1_REG					0x001D
#define NU1668_MTP_WDATA_2_REG					0x001E
#define NU1668_MTP_WDATA_3_REG					0x001F
										 
#define NU1668_GEN_REPORT_2_REG					0x0022
#define NU1668_GEN_REPORT_3_REG					0x0023
#define NU1668_GEN_REPORT_4_REG					0x0024
#define NU1668_GEN_REPORT_5_REG					0x0025
#define NU1668_GEN_REPORT_6_REG					0x0026
#define NU1668_GEN_REPORT_7_REG					0x0027
#define NU1668_GEN_REPORT_8_REG					0x0028
										 
#define NU1668_SP_CTRL0_REG					0x0090
#define NU1668_TEST_MODE_CTRL_0_REG				0x1000
#define NU1668_TEST_MODE_VECTOR_1_REG			0x1002
#define NU1668_GEN_OPT0_BYTE2_REG				0x1152
	
#define NU1668_KEY_OPEN_REG					0x2017
#define NU1668_KEY0_REG						0x2018
#define NU1668_KEY1_REG						0x2019
#define NU1668_TM_EN_ANA_REG					0x2020
/****************************************************************/

/* F/W Update & Verification ERROR CODES */
enum {
	MFC_FWUP_ERR_COMMON_FAIL = 0,
	MFC_FWUP_ERR_SUCCEEDED,
	MFC_FWUP_ERR_RUNNING,

	MFC_FWUP_ERR_REQUEST_FW_BIN,

	/* F/W update error */
	MFC_FWUP_ERR_WRITE_KEY_ERR,
	MFC_FWUP_ERR_CLK_TIMING_ERR1,  /* 5 */
	MFC_FWUP_ERR_CLK_TIMING_ERR2,
	MFC_FWUP_ERR_CLK_TIMING_ERR3,
	MFC_FWUP_ERR_CLK_TIMING_ERR4,
	MFC_FWUP_ERR_INFO_PAGE_EMPTY,
	MFC_FWUP_ERR_HALT_M0_ERR, /* 10 */
	MFC_FWUP_ERR_FAIL,
	MFC_FWUP_ERR_ADDR_READ_FAIL,
	MFC_FWUP_ERR_DATA_NOT_MATCH,
	MFC_FWUP_ERR_OTP_LOADER_IN_RAM_ERR,
	MFC_FWUP_ERR_CLR_MTP_STATUS_BYTE, /* 15 */
	MFC_FWUP_ERR_MAP_RAM_TO_OTP_ERR,
	MFC_FWUP_ERR_WRITING_TO_OTP_BUFFER,
	MFC_FWUP_ERR_OTF_BUFFER_VALIDATION,
	MFC_FWUP_ERR_READING_OTP_BUFFER_STATUS,
	MFC_FWUP_ERR_TIMEOUT_ON_BUFFER_TO_OTP, /* 20 */
	MFC_FWUP_ERR_MTP_WRITE_ERR,
	MFC_FWUP_ERR_PKT_CHECKSUM_ERR,
	MFC_FWUP_ERR_UNKNOWN_ERR,
	MFC_FWUP_ERR_BUFFER_WRITE_IN_SECTOR,
	MFC_FWUP_ERR_WRITING_FW_VERION, /* 25 */

	/* F/W verification error */
	MFC_VERIFY_ERR_WRITE_KEY_ERR,
	MFC_VERIFY_ERR_HALT_M0_ERR,
	MFC_VERIFY_ERR_KZALLOC_ERR,
	MFC_VERIFY_ERR_FAIL,
	MFC_VERIFY_ERR_ADDR_READ_FAIL, /* 30 */
	MFC_VERIFY_ERR_DATA_NOT_MATCH,
	MFC_VERIFY_ERR_MTP_VERIFIER_IN_RAM_ERR,
	MFC_VERIFY_ERR_CLR_MTP_STATUS_BYTE,
	MFC_VERIFY_ERR_MAP_RAM_TO_OTP_ERR,
	MFC_VERIFY_ERR_UNLOCK_SYS_REG_ERR, /* 35 */
	MFC_VERIFY_ERR_LDO_CLK_2MHZ_ERR,
	MFC_VERIFY_ERR_LDO_OUTPUT_5_5V_ERR,
	MFC_VERIFY_ERR_ENABLE_LDO_ERR,
	MFC_VERIFY_ERR_WRITING_TO_MTP_VERIFY_BUFFER,
	MFC_VERIFY_ERR_START_MTP_VERIFY_ERR, /* 40 */
	MFC_VERIFY_ERR_READING_MTP_VERIFY_STATUS,
	MFC_VERIFY_ERR_CRC_BUSY,
	MFC_VERIFY_ERR_READING_MTP_VERIFY_PASS_FAIL,
	MFC_VERIFY_ERR_CRC_ERROR,
	MFC_VERIFY_ERR_UNKNOWN_ERR, /* 45 */
	MFC_VERIFY_ERR_BUFFER_WRITE_IN_SECTOR,

	MFC_REPAIR_ERR_HALT_M0_ERR,
	MFC_REPAIR_ERR_MTP_REPAIR_IN_RAM,
	MFC_REPAIR_ERR_CLR_MTP_STATUS_BYTE,
	MFC_REPAIR_ERR_START_MTP_REPAIR_ERR, /* 50 */
	MFC_REPAIR_ERR_READING_MTP_REPAIR_STATUS,
	MFC_REPAIR_ERR_READING_MTP_REPAIR_PASS_FAIL,
	MFC_REPAIR_ERR_BUFFER_WRITE_IN_SECTOR,
};

/* PAD Vout */
enum {
	PAD_VOUT_5V = 0,
	PAD_VOUT_9V,
	PAD_VOUT_10V,
	PAD_VOUT_12V,
	PAD_VOUT_18V,
	PAD_VOUT_19V,
	PAD_VOUT_20V,
	PAD_VOUT_24V,
};

enum {
	MFC_ADC_VOUT = 0,
	MFC_ADC_VRECT,
	MFC_ADC_RX_IOUT,
	MFC_ADC_DIE_TEMP,
	MFC_ADC_OP_FRQ,
	MFC_ADC_TX_MAX_OP_FRQ,
	MFC_ADC_TX_MIN_OP_FRQ,
	MFC_ADC_PING_FRQ,
	MFC_ADC_TX_IOUT,
	MFC_ADC_TX_VOUT,
};

enum {
	MFC_ADDR = 0,
	MFC_SIZE,
	MFC_DATA,
	MFC_PACKET,
};

ssize_t nu1668_show_attrs(struct device *dev,
				struct device_attribute *attr, char *buf);

ssize_t nu1668_store_attrs(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count);

#define NU1668_ATTR(_name)				\
{							\
	.attr = {.name = #_name, .mode = 0660},	\
	.show = nu1668_show_attrs,			\
	.store = nu1668_store_attrs,			\
}

enum mfc_irq {
	MFC_IRQ_STAT_VOUT = 0,
	MFC_IRQ_STAT_VRECT,
	MFC_IRQ_MODE_CHANGE,
	MFC_IRQ_TX_DATA_RECEIVED,
	MFC_IRQ_OVER_VOLT,
	MFC_IRQ_OVER_CURR,
	MFC_IRQ_OVER_TEMP,
	MFC_IRQ_TX_OVER_CURR,
	MFC_IRQ_TX_OVER_TEMP,
	MFC_IRQ_TX_FOD,
	MFC_IRQ_TX_CONNECT,
	MFC_IRQ_NR,
};

enum mfc_firmware_mode {
	MFC_RX_FIRMWARE = 0,
	MFC_TX_FIRMWARE,
};

enum mfc_ic_revision {
	MFC_IC_REVISION = 0,
	MFC_IC_FONT,
};

enum mfc_chip_id {
	MFC_CHIP_IDT = 1,
	MFC_CHIP_LSI,
	MFC_CHIP_CPS,
	MFC_CHIP_NUVOLTA,
};

enum mfc_headroom {
	MFC_HEADROOM_0 = 0,
	MFC_HEADROOM_1, /* 0.273V */
	MFC_HEADROOM_2, /* 0.500V */
	MFC_HEADROOM_3, /* 0.648V */
	MFC_HEADROOM_4, /* 0.031V */
	MFC_HEADROOM_5, /* 0.078V */
	MFC_HEADROOM_6, /* 0.093V */
	MFC_HEADROOM_7, /* -0.601V */
};

# define RX_MAX_NUM  10
struct mfc_qfod_data {
	int rx_id;
};
static int sspth_value[RX_MAX_NUM];
static int qbase_value[RX_MAX_NUM];
static int qfod_th0[RX_MAX_NUM];
static int qfod_th1[RX_MAX_NUM];
static int qfod_th2[RX_MAX_NUM];
static int qfod_th3[RX_MAX_NUM];
static int qfod_tws_th3[RX_MAX_NUM];
static int qfod_phone_th4[RX_MAX_NUM];

#if defined(CONFIG_WIRELESS_IC_PARAM)
extern unsigned int wireless_fw_ver_param;
extern unsigned int wireless_chip_id_param;
extern unsigned int wireless_fw_mode_param;
#endif

struct mfc_charger_platform_data {
	int pad_mode;
	int wpc_det;
	int irq_wpc_det;
	int wpc_int;
	int mst_pwr_en;
	int wpc_en;
	int mag_det;
	int coil_sw_en;
	int wpc_pdrc;
	int irq_wpc_pdrc;
	int ping_nen;
	int irq_wpc_int;
	int wpc_pdet_b;
	int irq_wpc_pdet_b;
	int cs100_status;
	int vout_status;
	int siop_level;
	int cable_type;
	bool default_voreg;
	int is_charging;
	u32 *wireless20_vout_list;
	u32 *wireless20_vrect_list;
	u32 *wireless20_max_power_list;
	u8 len_wc20_list;
	bool ic_on_mode;
	int hw_rev_changed; /* this is only for noble/zero2 */
	int otp_firmware_result;
	int tx_firmware_result;
	int wc_ic_grade;
	int wc_ic_rev;
	int otp_firmware_ver;
	int tx_firmware_ver;
	int vout;
	int vrect;
	u8 trx_data_cmd;
	u8 trx_data_val;
	char *wireless_charger_name;
	char *wired_charger_name;
	char *fuelgauge_name;
	int opfq_cnt;
	int mst_switch_delay;
	int wc_cover_rpp;
	int wc_hv_rpp;
	u32 phone_fod_thresh1;
	u32 buds_fod_thresh1;
	u32 buds_fod_ta_thresh;
	u32 tx_max_op_freq;
	u32 tx_min_op_freq;
	u32 cep_timeout;
	int no_hv;
	bool keep_tx_vout;
	u32 wpc_vout_ctrl_full;
	bool wpc_headroom_ctrl_full;
	bool mis_align_guide;
	bool unknown_cmb_ctrl;
	bool enable_qfod_param;
	bool default_clamp_volt;
	u32 mis_align_target_vout;
	u32 mis_align_offset;
	struct mfc_qfod_data *qfod_list;
	int qfod_data_num;
	int tx_conflict_curr;

	u32 mpp_epp_vout;
	u32 mpp_epp_def_power;

	bool pdrc_vrect_clear;
};

#define mfc_charger_platform_data_t \
	struct mfc_charger_platform_data

#define MST_MODE_0			0
#define MST_MODE_2			1

#define MFC_BAT_DUMP_SIZE	256

struct mfc_charger_data {
	struct i2c_client				*client;
	struct device					*dev;
	mfc_charger_platform_data_t		*pdata;
	struct mutex io_lock;
	struct mutex wpc_en_lock;
	struct mutex fw_lock;
	const struct firmware *firm_data_bin;

	u8 det_state; /* ACTIVE HIGH */
	u8 pdrc_state; /* ACTIVE LOW */

	struct power_supply *psy_chg;
	struct wakeup_source *wpc_ws;
	struct wakeup_source *wpc_det_ws;
	struct wakeup_source *wpc_tx_ws;
	struct wakeup_source *wpc_rx_ws;
	struct wakeup_source *wpc_update_ws;
	struct wakeup_source *wpc_tx_duty_min_ws;
	struct wakeup_source *wpc_afc_vout_ws;
	struct wakeup_source *wpc_vout_mode_ws;
	struct wakeup_source *wpc_rx_det_ws;
	struct wakeup_source *wpc_tx_phm_ws;
	struct wakeup_source *wpc_tx_id_ws;
	struct wakeup_source *wpc_tx_pwr_budg_ws;
	struct wakeup_source *wpc_pdrc_ws;
	struct wakeup_source *align_check_ws;
	struct wakeup_source *mode_change_ws;
	struct wakeup_source *wpc_cs100_ws;
	struct wakeup_source *wpc_pdet_b_ws;
	struct wakeup_source *wpc_rx_phm_ws;
	struct wakeup_source *wpc_check_rx_power_ws;
	struct wakeup_source *wpc_vrect_check_ws;
	struct wakeup_source *wpc_phm_exit_ws;
	struct wakeup_source *epp_clear_ws;
	struct workqueue_struct *wqueue;
	struct work_struct wcin_work;
	struct delayed_work wpc_det_work;
	struct delayed_work wpc_pdrc_work;
	struct delayed_work wpc_isr_work;
	struct delayed_work wpc_tx_isr_work;
	struct delayed_work wpc_tx_id_work;
	struct delayed_work wpc_tx_pwr_budg_work;
	struct delayed_work mst_off_work;
	struct delayed_work wpc_int_req_work;
	struct delayed_work wpc_fw_update_work;
	struct delayed_work wpc_afc_vout_work;
	struct delayed_work wpc_fw_booting_work;
	struct delayed_work wpc_vout_mode_work;
	struct delayed_work wpc_i2c_error_work;
	struct delayed_work wpc_rx_type_det_work;
	struct delayed_work wpc_rx_connection_work;
	struct delayed_work wpc_tx_op_freq_work;
	struct delayed_work wpc_tx_duty_min_work;
	struct delayed_work wpc_tx_phm_work;
	struct delayed_work wpc_vrect_check_work;
	struct delayed_work wpc_rx_power_work;
	struct delayed_work wpc_cs100_work;
	struct delayed_work wpc_init_work;
	struct delayed_work align_check_work;
	struct delayed_work mode_change_work;
	struct delayed_work wpc_rx_phm_work;
	struct delayed_work wpc_check_rx_power_work;
	struct delayed_work wpc_phm_exit_work;
	struct delayed_work epp_clear_timer_work;

	struct alarm phm_alarm;

	struct mfc_fod *fod;
	struct mfc_cmfet *cmfet;

	u16 addr;
	int size;
	int is_afc;
	int pad_vout;
	int is_mst_on; /* mst */
	int chip_id;
	u32 rx_op_mode;
	int fw_cmd;
	int vout_mode;
	u32 vout_by_txid;
	u32 vrect_by_txid;
	u32 max_power_by_txid;
	int is_full_status;
	int mst_off_lock;
	bool is_otg_on;
	int led_cover;
	bool is_probed;
	bool is_afc_tx;
	bool pad_ctrl_by_lcd;
	bool tx_id_done;
	bool is_suspend;
	int tx_id;
	int tx_id_cnt;
	bool initial_vrect;
	bool rx_phm_status;
	int rx_phm_state;

	int flicker_delay;
	int flicker_vout_threshold;

	/* wireless tx */
	int tx_status;
	bool initial_wc_check;
	bool wc_tx_enable;
	int wc_rx_type;
	bool wc_rx_connected;
	bool wc_rx_fod;
	bool wc_ldo_status;
	int non_sleep_mode_cnt;
	u8 adt_transfer_status;
	u8 current_rx_power;
	u8 tx_pwr_budg;
	u8 device_event;
	int i2c_error_count;
	int input_current;
	int duty_min;
	int wpc_en_flag;
	bool tx_device_phm;

	bool req_tx_id;
	bool afc_tx_done;
	int req_afc_delay;

	bool sleep_mode;
	bool wc_checking_align;
	struct timespec64 wc_align_check_start;
	int vout_strength;
	u32 mis_align_tx_try_cnt;
	bool skip_phm_work_in_sleep;
	bool reg_access_lock;
	bool check_rx_power;

	int mfc_adc_tx_vout;
	int mfc_adc_tx_iout;
	int mfc_adc_ping_frq;
	int mfc_adc_tx_min_op_frq;
	int mfc_adc_tx_max_op_frq;
	int mfc_adc_vout;
	int mfc_adc_vrect;
	int mfc_adc_rx_iout;
	int mfc_adc_op_frq;
	union mfc_fod_state now_fod_state;
	union mfc_cmfet_state now_cmfet_state;

#if defined(CONFIG_WIRELESS_IC_PARAM)
	unsigned int wireless_param_info;
	unsigned int wireless_fw_ver_param;
	unsigned int wireless_chip_id_param;
	unsigned int wireless_fw_mode_param;
#endif

	/* EPP flag - T.B.D. */
	//bool epp_supported;
	//bool epp_nego_pass;
	//bool epp_nego_fail;

	/* MPP flag - T.B.D. */
	//bool mpp_supported;
	//bool mpp_nego_pass_360;
	//bool mpp_power_inc;
	//bool mpp_power_dec;
	//bool mpp_exit_cloak;

	// EPP & MPP NEGO data - T.B.D.
	u32 mpp_epp_tx_id;
	u8 nego_done_power;
	u8 potential_load_power;
	u8 negotiable_load_power;
	u8 mpp_cloak;

	int epp_time;

	char d_buf[MFC_BAT_DUMP_SIZE];
};

#define fan_ctrl_pad(pad_id) (\
	(pad_id >= 0x14 && pad_id <= 0x1f) || \
	(pad_id >= 0x25 && pad_id <= 0x2f) || \
	(pad_id >= 0x30 && pad_id <= 0x3f) || \
	(pad_id >= 0x46 && pad_id <= 0x4f) || \
	(pad_id >= 0xa1 && pad_id <= 0xcf) || \
	(pad_id >= 0xd0 && pad_id <= 0xff))

#define opfreq_ctrl_pad(pad_id) (\
	((pad_id >= TX_ID_NON_AUTH_PAD) && (pad_id <= TX_ID_NON_AUTH_PAD_END)) || \
	((pad_id >= TX_ID_N5200_V_PAD) && (pad_id <= TX_ID_AUTH_PAD_ACLASS_END)) || \
	(pad_id == TX_ID_P1300_PAD) || \
	(pad_id == TX_ID_N3300_V_PAD) || \
	(pad_id == TX_ID_N3300_H_PAD) || \
	(pad_id == TX_ID_P4300_PAD))

#define volt_ctrl_pad(pad_id) (\
	(pad_id != TX_ID_PG950_S_PAD) && \
	(pad_id != TX_ID_PG950_D_PAD))

#define bpp_mode(op_mode) (\
	(op_mode == PAD_RX_MODE_WPC_BPP))

#define mpp_mode(op_mode) (\
	(op_mode == PAD_RX_MODE_WPC_MPP_RESTRICT) || \
	(op_mode == PAD_RX_MODE_WPC_MPP_FULL) || \
	(op_mode == PAD_RX_MODE_WPC_MPP_CLOAK) || \
	(op_mode == PAD_RX_MODE_WPC_MPP_NEGO))

#define epp_mode(op_mode) (\
	(op_mode == PAD_RX_MODE_WPC_EPP) || \
	(op_mode == PAD_RX_MODE_WPC_EPP_NEGO))

#endif /* __WIRELESS_CHARGER_NU1668_H */
