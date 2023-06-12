/*
 * Copyright (C) 2012 Spreadtrum Communications Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef _SPRD_2713_CHARGE_H_
#define _SPRD_2713_CHARGE_H_

#define SPRDBAT_CCCV_MIN    0x00
#define SPRDBAT_CCCV_MAX   0x3F
#define SPRDBAT_CCCV_DEFAULT    0x0
#define ONE_CCCV_STEP_VOL   75	//7.5mV

#define SPRDBAT_CHG_END_VOL_PURE			(4200)
#define SPRDBAT_CHG_END_H			(4225)
#define SPRDBAT_CHG_END_L			(4190)
#define SPRDBAT_RECHG_VOL	4131	//recharge voltage

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

//sprdchg api
void sprdchg_timer_enable(uint32_t cycles);
void sprdchg_timer_disable(void);
int sprdchg_timer_init(int (*fn_cb) (void *data), void *data);
void sprdchg_init(void);
int sprdchg_read_temp(void);
uint32_t sprdchg_read_vchg_vol(void);
void sprdchg_start_charge(void);
void sprdchg_stop_charge(void);
void sprdchg_set_cccvpoint(unsigned int cvpoint);
uint32_t sprdchg_get_cccvpoint(void);
uint32_t sprdchg_tune_endvol_cccv(uint32_t chg_end_vol, uint32_t cal_cccv);
void sprdchg_set_chg_cur(uint32_t chg_current);
int sprdchg_charger_is_adapter(void);
void sprdchg_set_chg_ovp(uint32_t ovp_vol);
uint32_t sprdchg_read_chg_current(void);
void sprdchg_put_chgcur(uint32_t chging_current);
uint32_t sprdchg_get_chgcur_ave(void);
uint32_t sprdchg_read_vbat_vol(void);

#endif /* _CHG_DRVAPI_H_ */
