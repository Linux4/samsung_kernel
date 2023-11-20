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

#ifndef _SPRD_CHG_HELP_CHARGE_H_
#define _SPRD_CHG_HELP_CHARGE_H_
#define SPRDBAT_ADC_CHANNEL_VCHG ADC_CHANNEL_VCHGSEN

#define SPRDBAT_TEMP_TRIGGER_TIMES 2

#define CHG_CUR_ADJUST
//#define CHG_CUR_JEITA
//#define JEITA_DEBUG

void sprdchg_timer_enable(uint32_t cycles);
void sprdchg_timer_disable(void);
int sprdchg_timer_init(int (*fn_cb) (void *data), void *data);
void sprdchg_init(struct sprd_battery_platform_data *pdata);
int sprdchg_read_temp(void);
int sprdchg_read_temp_adc(void);
uint32_t sprdchg_read_vchg_vol(void);
int sprdchg_charger_is_adapter(void);
uint32_t sprdchg_read_vbat_vol(void);
uint32_t sprdchg_read_chg_current(void);
void sprdchg_led_brightness_set(struct led_classdev *led_cdev,
				enum led_brightness brightness);

#endif 

