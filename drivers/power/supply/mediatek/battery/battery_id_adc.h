/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2020.
 */
/* hs14 code for  SR-AL6528A-01-313 by zhouyuhang at 20220907 start*/

#ifndef __BATTERY_ID_ADC__
#define __BATTERY_ID_ADC__

#ifdef CONFIG_MEDIATEK_MT6768_AUXADC
int bat_id_get_adc_num(void);
int bat_id_get_adc_val(void);
#endif

int bat_id_get_adc_num(void);
signed int battery_get_bat_id_voltage(void);

#endif
/* hs14 code for  SR-AL6528A-01-313 by zhouyuhang at 20220907 end*/
