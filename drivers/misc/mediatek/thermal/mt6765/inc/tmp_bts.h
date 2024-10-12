/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2019 MediaTek Inc.
 */

#ifndef __TMP_BTS_H__
#define __TMP_BTS_H__

/* chip dependent */

#define APPLY_PRECISE_NTC_TABLE
#define APPLY_AUXADC_CALI_DATA

#define AUX_IN0_NTC (0)
#define AUX_IN1_NTC (1)
//+chk 129980,jinyucheng.wt,add new NTC,20220728
#define AUX_IN2_NTC (2)
#define AUX_IN3_NTC (3)
//-chk 129980,jinyucheng.wt,add new NTC,20220728

#define BTS_RAP_PULL_UP_R		390000 /* 390K, pull up resister */

#define BTS_TAP_OVER_CRITICAL_LOW	4397119 /* base on 100K NTC temp
						 * default value -40 deg
						 */

#define BTS_RAP_PULL_UP_VOLTAGE		1800 /* 1.8V ,pull up voltage */

#define BTS_RAP_NTC_TABLE		7 /* default is NCP15WF104F03RC(100K) */

#define BTS_RAP_ADC_CHANNEL		AUX_IN0_NTC /* default is 0 */

#define BTSMDPA_RAP_PULL_UP_R		390000 /* 390K, pull up resister */

#define BTSMDPA_TAP_OVER_CRITICAL_LOW	4397119 /* base on 100K NTC temp
						 * default value -40 deg
						 */

#define BTSMDPA_RAP_PULL_UP_VOLTAGE	1800 /* 1.8V ,pull up voltage */

#define BTSMDPA_RAP_NTC_TABLE		7 /* default is NCP15WF104F03RC(100K) */

#define BTSMDPA_RAP_ADC_CHANNEL		AUX_IN1_NTC /* default is 1 */

//+chk 129980,jinyucheng.wt,add new NTC,20220728
#define BTSFLED_EXT_RAP_PULL_UP_R	390000 /* 390K, pull up resister */

#define BTSFLED_EXT_TAP_OVER_CRITICAL_LOW	4397119 /* base on 100K NTC temp default -40 deg */

#define BTSFLED_EXT_RAP_PULL_UP_VOLTAGE	1800 /* 1.8V ,pull up voltage */

#define BTSFLED_EXT_RAP_NTC_TABLE	7 /* default is NCP15WF104F03RC(100K) */

#define BTSFLED_EXT_RAP_ADC_CHANNEL	AUX_IN2_NTC /* default is 2 */

#define BTSCHARGER_EXT_RAP_PULL_UP_R	390000   /* 390K, pull up resister */

#define BTSCHARGER_EXT_TAP_OVER_CRITICAL_LOW	4397119 /* base on 100K NTC temp default -40 deg */

#define BTSCHARGER_EXT_RAP_PULL_UP_VOLTAGE	1800 /* 1.8V ,pull up voltage */

#define BTSCHARGER_EXT_RAP_NTC_TABLE	7 /* default is NCP15WF104F03RC(100K) */

#define BTSCHARGER_EXT_RAP_ADC_CHANNEL	AUX_IN3_NTC /* default is 3 */ 
////-chk 129980,jinyucheng.wt,add new NTC,20220728

extern int IMM_GetOneChannelValue(int dwChannel, int data[4], int *rawdata);
extern int IMM_IsAdcInitReady(void);

#endif	/* __TMP_BTS_H__ */
