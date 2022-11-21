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

#ifdef CONFIG_SHARK_PAD_HW_V102
#define SPRDBAT_TWO_CHARGE_CHANNEL
#endif

#define SPRDBAT_CCCV_MIN    0x00
#define SPRDBAT_CCCV_MAX   0x3F
#define ONE_CCCV_STEP_VOL   75	//7.5mV

#define SPRDBAT_CHG_OVP_LEVEL_MIN       5600
#define SPRDBAT_CHG_OVP_LEVEL_MAX   9000

/*charge current type*/
#define SPRDBAT_CHG_CUR_LEVEL_MIN       300
#define SPRDBAT_CHG_CUR_LEVEL_MAX	2300

#define SPRDBAT_ADC_CHANNEL_VCHG ADC_CHANNEL_VCHGSEN

#define SPRDBAT_TEMP_TRIGGER_TIMES 2

//sprdchg api
void sprdchg_timer_enable(uint32_t cycles);
void sprdchg_timer_disable(void);
int sprdchg_timer_init(int (*fn_cb) (void *data), void *data);
void sprdchg_init(struct platform_device *pdev);
int sprdchg_read_temp(void);
int sprdchg_read_temp_adc(void);
uint32_t sprdchg_read_vchg_vol(void);
void sprdchg_start_charge(void);
void sprdchg_stop_charge(void);
void sprdchg_set_eoc_level(int level);
int sprdchg_get_eoc_level(void);
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
void sprdchg_led_brightness_set(struct led_classdev *led_cdev,
				enum led_brightness brightness);

#endif /* _CHG_DRVAPI_H_ */
