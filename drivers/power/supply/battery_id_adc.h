/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2020.
 */
/*HS03s for SR-AL5625-01-251 by wenyaqi at 20210425 start*/

#ifndef __BATTERY_ID_ADC__
#define __BATTERY_ID_ADC__

#ifdef CONFIG_MEDIATEK_MT6577_AUXADC
int bat_id_get_adc_num(void);
int bat_id_get_adc_val(void);
#endif

//static int bat_id_get_adc_info(struct device *dev);
int bat_id_get_adc_num(void);
signed int battery_get_bat_id_voltage(void);

#endif
/*HS03s for SR-AL5625-01-251 by wenyaqi at 20210425 end*/