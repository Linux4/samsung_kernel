/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/
#ifndef __TMP_BTS_H__
#define __TMP_BTS_H__

#define APPLY_PRECISE_NTC_TABLE
#define APPLY_AUXADC_CALI_DATA
#define APPLY_PRECISE_BTS_TEMP

#define AUX_IN0_NTC (0)
#define AUX_IN1_NTC (1)
#define AUX_IN2_NTC (2)
//[+Chk 127645,songyuanqiao@wingtech.com,add the thermal ntc 20220704]
#define AUX_IN3_NTC (3)
#define AUX_IN4_NTC (4)
#define AUX_IN6_NTC (6)
//[-Chk 127645,songyuanqiao@wingtech.com,add the thermal ntc 20220704]


#define BTS_RAP_PULL_UP_R		100000 /* 100K, pull up resister */

#define BTS_TAP_OVER_CRITICAL_LOW	4397119 /* base on 100K NTC temp
						 * default value -40 deg
						 */

#define BTS_RAP_PULL_UP_VOLTAGE		1800 /* 1.8V ,pull up voltage */

#define BTS_RAP_NTC_TABLE		7 /* default is NCP15WF104F03RC(100K) */

#define BTS_RAP_ADC_CHANNEL		AUX_IN0_NTC /* default is 0 */

#define BTSMDPA_RAP_PULL_UP_R		100000 /* 100K, pull up resister */

#define BTSMDPA_TAP_OVER_CRITICAL_LOW	4397119 /* base on 100K NTC temp
						 * default value -40 deg
						 */

#define BTSMDPA_RAP_PULL_UP_VOLTAGE	1800 /* 1.8V ,pull up voltage */

#define BTSMDPA_RAP_NTC_TABLE		7 /* default is NCP15WF104F03RC(100K) */

#define BTSMDPA_RAP_ADC_CHANNEL		AUX_IN1_NTC /* default is 1 */


#define BTSNRPA_RAP_PULL_UP_R		100000	/* 100K,pull up resister */
#define BTSNRPA_TAP_OVER_CRITICAL_LOW	4397119	/* base on 100K NTC temp
						 *default value -40 deg
						 */

#define BTSNRPA_RAP_PULL_UP_VOLTAGE	1800	/* 1.8V ,pull up voltage */
#define BTSNRPA_RAP_NTC_TABLE		7

#define BTSNRPA_RAP_ADC_CHANNEL		AUX_IN2_NTC

//[+Chk 127645,songyuanqiao@wingtech.com,add the thermal ntc 20220704]
#define BTSCHARGER_RAP_PULL_UP_R		100000	/* 100K,pull up resister */
#define BTSCHARGER_TAP_OVER_CRITICAL_LOW	4397119	/* base on 100K NTC temp
						 *default value -40 deg
						 */

#define BTSCHARGER_RAP_PULL_UP_VOLTAGE	1800	/* 1.8V ,pull up voltage */
#define BTSCHARGER_RAP_NTC_TABLE		7

#define BTSCHARGER_RAP_ADC_CHANNEL		AUX_IN3_NTC

#define BTSFLASH_RAP_PULL_UP_R		100000	/* 100K,pull up resister */
#define BTSFLASH_TAP_OVER_CRITICAL_LOW	4397119	/* base on 100K NTC temp
						 *default value -40 deg
						 */

#define BTSFLASH_RAP_PULL_UP_VOLTAGE	1800	/* 1.8V ,pull up voltage */
#define BTSFLASH_RAP_NTC_TABLE		7

#define BTSFLASH_RAP_ADC_CHANNEL		AUX_IN4_NTC



#define BTSMAINBOARD_RAP_PULL_UP_R		100000	/* 100K,pull up resister */
#define BTSMAINBOARD_TAP_OVER_CRITICAL_LOW	4397119	/* base on 100K NTC temp
						 *default value -40 deg
						 */

#define BTSMAINBOARD_RAP_PULL_UP_VOLTAGE	1800	/* 1.8V ,pull up voltage */
#define BTSMAINBOARD_RAP_NTC_TABLE		7

#define BTSMAINBOARD_RAP_ADC_CHANNEL		AUX_IN6_NTC
//[-Chk 127645,songyuanqiao@wingtech.com,add the thermal ntc 20220704]
extern int IMM_GetOneChannelValue(int dwChannel, int data[4], int *rawdata);
extern int IMM_IsAdcInitReady(void);

#endif	/* __TMP_BTS_H__ */
