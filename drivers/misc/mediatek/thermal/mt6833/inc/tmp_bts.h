/*
 * Copyright (C) 2018 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */
#ifndef __TMP_BTS_H__
#define __TMP_BTS_H__

#define APPLY_PRECISE_NTC_TABLE
#define APPLY_AUXADC_CALI_DATA
#define APPLY_PRECISE_BTS_TEMP

#define AUX_IN0_NTC (0)
#define AUX_IN1_NTC (1)
#define AUX_IN2_NTC (2)
/*bug[773028] w1 add new thermal ntc renjiawei 20220726 begin*/
#define AUX_IN3_NTC (3)
#define AUX_IN4_NTC (4)
#define AUX_IN6_NTC (6)
/*bug[773028] w1 add new thermal ntc renjiawei 20220726 end*/

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

/*bug[773028] w1 add new thermal ntc renjiawei 20220726 begin*/
#define BTSCHARGER_EXT_RAP_PULL_UP_R	100000	/* 100K,pull up resister */

#define BTSCHARGER_EXT_TAP_OVER_CRITICAL_LOW 4397119 /* base on 100K NTC temp
						 *default value -40 deg
						 */

#define BTSCHARGER_EXT_RAP_PULL_UP_VOLTAGE	1800	/* 1.8V ,pull up voltage */

#define BTSCHARGER_EXT_RAP_NTC_TABLE	7

#define BTSCHARGER_EXT_RAP_ADC_CHANNEL	AUX_IN3_NTC


#define BTSFLASHLIGHT_RAP_PULL_UP_R	100000	/* 100K,pull up resister */

#define BTSFLASHLIGHT_TAP_OVER_CRITICAL_LOW 4397119 /* base on 100K NTC temp
						 *default value -40 deg
						 */

#define BTSFLASHLIGHT_RAP_PULL_UP_VOLTAGE	1800	/* 1.8V ,pull up voltage */

#define BTSFLASHLIGHT_RAP_NTC_TABLE	7

#define BTSFLASHLIGHT_RAP_ADC_CHANNEL	AUX_IN4_NTC

#define BTSMBTHERMAL_RAP_PULL_UP_R	100000	/* 100K,pull up resister */

#define BTSMBTHERMAL_TAP_OVER_CRITICAL_LOW 4397119 /* base on 100K NTC temp
						 *default value -40 deg
						 */

#define BTSMBTHERMAL_RAP_PULL_UP_VOLTAGE	1800	/* 1.8V ,pull up voltage */

#define BTSMBTHERMAL_RAP_NTC_TABLE	7

#define BTSMBTHERMAL_RAP_ADC_CHANNEL	AUX_IN6_NTC
/*bug[773028] w1 add new thermal ntc renjiawei 20220726 end*/


extern int IMM_GetOneChannelValue(int dwChannel, int data[4], int *rawdata);
extern int IMM_IsAdcInitReady(void);

#endif	/* __TMP_BTS_H__ */
