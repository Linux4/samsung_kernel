/*
 * sprd27x3_charger4samsung.h
 * Samsung SPRD27X3 Charger Header
 *
 * Copyright (C) 2012 Samsung Electronics, Inc.
 *
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

#ifndef __SPRD27X3_CHARGER4SAMSUNG_H
#define __SPRD27X3_CHARGER4SAMSUNG_H

/* Slave address should be shifted to the right 1bit.
 * R/W bit should NOT be included.
 */

#define SPRDBAT_AUXADC_CAL_NO         0
#define SPRDBAT_AUXADC_CAL_NV         1
#define SPRDBAT_AUXADC_CAL_CHIP      2

#define SPRDBAT_CHG_END_NONE_BIT			0
#define SPRDBAT_CHG_END_FULL_BIT		(1 << 0)
#define SPRDBAT_CHG_END_OTP_OVERHEAT_BIT     	(1 << 1)
#define SPRDBAT_CHG_END_OTP_COLD_BIT    (1 << 2)
#define SPRDBAT_CHG_END_TIMEOUT_BIT		(1 << 3)
#define SPRDBAT_CHG_END_OVP_BIT		(1 << 4)

#define SPRDBAT_AVERAGE_COUNT   3

#define SPRDBAT_AUXADC_CAL_TYPE_NO         0
#define SPRDBAT_AUXADC_CAL_TYPE_NV         1
#define SPRDBAT_AUXADC_CAL_TYPE_EFUSE      2

#define SPRDBAT_CCCV_MIN    0x00
#define SPRDBAT_CCCV_MAX   0x3F
#define SPRDBAT_CCCV_DEFAULT    0x0
#define ONE_CCCV_STEP_VOL   75	//7.5mV

#ifdef CONFIG_MACH_VIVALTO
#define SPRDBAT_CHG_END_VOL_PURE		(4350)
#elif defined(CONFIG_MACH_HIGGS)
#define SPRDBAT_CHG_END_VOL_PURE		(4350)
#elif defined(CONFIG_MACH_YOUNG2)
#define SPRDBAT_CHG_END_VOL_PURE		(4200)
#else
#define SPRDBAT_CHG_END_VOL_PURE		(4200)
#endif
#define SPRDBAT_CHG_END_H			(SPRDBAT_CHG_END_VOL_PURE + 25)
#define SPRDBAT_CHG_END_L			(SPRDBAT_CHG_END_VOL_PURE - 10)
#define SPRDBAT_RECHG_VOL	(SPRDBAT_CHG_END_VOL_PURE - 70)	//recharge voltage

#define SPRDBAT_CHG_OVP_LEVEL_MIN       5600
#define SPRDBAT_CHG_OVP_LEVEL_MAX   9000
#define SPRDBAT_OVP_STOP_VOL           6500
#define SPRDBAT_OVP_RESTERT_VOL	5800	//reserved

/*charge current type*/
#define SPRDBAT_CHG_CUR_LEVEL_MIN       300
#define SPRDBAT_CHG_CUR_LEVEL_MAX	2300
#define SPRDBAT_SDP_CUR_LEVEL    500	//usb
#define SPRDBAT_DCP_CUR_LEVEL    700	//AC
#define SPRDBAT_CDP_CUR_LEVEL    700	//AC&USB

#define SPRDBAT_CHG_END_CUR     70

#define SPRDBAT_OTP_HIGH_STOP     600	//60C
#define SPRDBAT_OTP_HIGH_RESTART    (550)	//55C
#define SPRDBAT_OTP_LOW_STOP   -50	//-5C
#define SPRDBAT_OTP_LOW_RESTART  0	//0C

#define SPRDBAT_CHG_NORMAL_TIMEOUT		(6*60*60)	/* set for charge over time, 6 hours */
#define SPRDBAT_CHG_SPECIAL_TIMEOUT		(90*60)	/* when charge timeout, recharge timeout is 90min */

#define SPRDBAT_TRICKLE_CHG_TIME		(1500)	//reserved

#define SPRDBAT_ADC_CHANNEL_VCHG ADC_CHANNEL_VCHGSEN

#define SPRDBAT_ADC_CHANNEL_TEMP ADC_CHANNEL_3
#define SPRDBAT_ADC_CHANNEL_TEMP_WPA ADC_CHANNEL_1
#define SPRDBAT_ADC_CHANNEL_TEMP_DCXO ADC_CHANNEL_2

#define SPRDBAT_POLL_TIMER_NORMAL   60
#define SPRDBAT_POLL_TIMER_TEMP   1

#define SPRDBAT_TEMP_TRIGGER_TIMES 2

#define SPRDBAT_CAPACITY_MONITOR_NORMAL	(HZ*10)
#define SPRDBAT_CAPACITY_MONITOR_FAST	(HZ*5)

#if !defined(CONFIG_CHARGER_SM5414)
struct sec_chg_info {
	bool dummy;
};
#endif

struct sprdbat_auxadc_cal {
	uint16_t p0_vol;	//4.2V
	uint16_t p0_adc;
	uint16_t p1_vol;	//3.6V
	uint16_t p1_adc;
	uint16_t cal_type;
};
#endif /* __SPRD27X3_CHARGER4SAMSUNG_H */



