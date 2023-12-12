/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2020.
 */
#ifndef __BATTERY_ID_ADC__
#define __BATTERY_ID_ADC__
#define    BATTERY_ATL_ID_VOLT_HIGH            990
#define    BATTERY_ATL_ID_VOLT_LOW             810
/*Tab A9 code for AX6739A-1103 by lina at 20230613 start*/
#define    BATTERY_BYD_ID_VOLT_HIGH            1289
#define    BATTERY_BYD_ID_VOLT_LOW             1059
/*Tab A9 code for AX6739A-1103 by lina at 20230613 end*/
#ifdef CONFIG_MEDIATEK_MT6577_AUXADC
int bat_id_get_adc_num(void);
int bat_id_get_adc_val(void);
int battery_get_profile_id(void);
#endif

//static int bat_id_get_adc_info(struct device *dev);
int bat_id_get_adc_num(void);
signed int battery_get_bat_id_voltage(void);
int battery_get_profile_id(void);

#endif
