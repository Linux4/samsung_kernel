/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2019 MediaTek Inc.
 */

#ifndef __TMP_BTS_CHARGER_H__
#define __TMP_BTS_CHARGER_H__

/* chip dependent */

#define APPLY_PRECISE_NTC_TABLE
#define APPLY_AUXADC_CALI_DATA
#if defined(CONFIG_HQ_PROJECT_OT8)
/*TabA7 Lite code for SR-AX3565-01-55 modify charger temp ntc by wenyaqi at 20201123 start*/
#define AUX_IN4_NTC (4)
/* Lite code for SR-AX3565-01-55 modify charger temp ntc by wenyaqi at 20201123 end*/
#else
#define AUX_IN2_NTC (2)
#endif
/* 390K, pull up resister */
#define BTSCHARGER_RAP_PULL_UP_R		390000
/* base on 100K NTC temp
 * default value -40 deg
 */
#define BTSCHARGER_TAP_OVER_CRITICAL_LOW	4397119
/* 1.8V ,pull up voltage */
#define BTSCHARGER_RAP_PULL_UP_VOLTAGE	1800
/* default is NCP15WF104F03RC(100K) */
#define BTSCHARGER_RAP_NTC_TABLE		7

#if defined(CONFIG_HQ_PROJECT_OT8)
/*TabA7 Lite code for SR-AX3565-01-55 modify charger temp ntc by wenyaqi at 20201123 start*/
#define BTSCHARGER_RAP_ADC_CHANNEL		AUX_IN4_NTC /* default is 4 */
/*TabA7 Lite code for SR-AX3565-01-55 modify charger temp ntc by wenyaqi at 20201123 end*/
#else
#define BTSCHARGER_RAP_ADC_CHANNEL		AUX_IN2_NTC /* default is 2 */
#endif
extern int IMM_GetOneChannelValue(int dwChannel, int data[4], int *rawdata);
extern int IMM_IsAdcInitReady(void);

#endif	/* __TMP_BTS_CHARGER_H__ */
