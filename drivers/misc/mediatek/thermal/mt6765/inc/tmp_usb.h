/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2019 MediaTek Inc.
 */
/*TabA7 Lite code for  SR-AX3565-01-55 add usb thermal node by wenyaqi at 20201130 start*/
#ifndef __TMP_BTS_CHARGER_H__
#define __TMP_BTS_CHARGER_H__

/* chip dependent */

#define APPLY_PRECISE_NTC_TABLE
#define APPLY_AUXADC_CALI_DATA


#define AUX_IN3_NTC (3)
/* 390K, pull up resister */
#define USB_RAP_PULL_UP_R		390000
/* base on 100K NTC temp
 * default value -40 deg
 */
#define USB_TAP_OVER_CRITICAL_LOW	4397119
/* 1.8V ,pull up voltage */
#define USB_RAP_PULL_UP_VOLTAGE	1800
/* default is NCP15WF104F03RC(100K) */
#define USB_RAP_NTC_TABLE		7


#define USB_RAP_ADC_CHANNEL		AUX_IN3_NTC /* default is 3 */
extern int IMM_GetOneChannelValue(int dwChannel, int data[4], int *rawdata);
extern int IMM_IsAdcInitReady(void);

#endif	/* __TMP_BTS_CHARGER_H__ */
/*TabA7 Lite code for  SR-AX3565-01-55 add usb thermal node by wenyaqi at 20201130 end*/