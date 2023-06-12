/*
 * drivers/battery/rt5033_fuelgauge.h
 *
 * Header of Richtek RT5033 Fuelgauge Driver
 *
 * Copyright (C) 2013 Richtek Technology Corp.
 * Author: Patrick Chang <patrick_chang@richtek.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 */

#ifndef RT5033_FUELGAUGE_H
#define RT5033_FUELGAUGE_H

#include <linux/i2c.h>
#include <linux/mfd/rt5033.h>

#define GG_VERSION "2.00a"
#define RT5033_SLAVE_ADDRESS	0x35

typedef struct{
	int x;
	int y1;
	int y2;
	int y3;
	int y4;
}gain_table_prop;

typedef struct{
	int x;
	int y;
}offset_table_prop;

struct battery_data_t{
	gain_table_prop *param1;
	gain_table_prop *param2;
	gain_table_prop *param3;
	gain_table_prop *param4;
	offset_table_prop *offset1;
	offset_table_prop *offset2;
	offset_table_prop *offset3;
	offset_table_prop *offset4;
	int param1_size;
	int param2_size;
	int param3_size;
	int param4_size;
	int offset1_size;
	int offset2_size;
	int offset3_size;
	int offset4_size;
};

struct sec_fg_info {
	/* State Of Connect */
	int online;
	/* battery SOC (capacity) */
	int batt_soc;
	/* battery voltage */
	int batt_voltage;
	/* battery AvgVoltage */
	int batt_avgvoltage;
	/* battery OCV */
	int batt_ocv;
	/* Current */
	int batt_current;
	/* battery Avg Current */
	int batt_avgcurrent;
	/* battery temperatue */
	u32 current_temp_adc;

	struct battery_data_t *comp_pdata;
	struct mutex io_lock;
	struct device *dev;
	int32_t temperature;; /* 0.1 deg C*/
	/* register programming */
	int reg_addr;
	u8 reg_data[2];

	uint32_t move_avg_offset[6];
	uint32_t pre_soc;
	uint32_t move_avg_offset_cnt;
	uint32_t flag_full_charge : 1; /* 0 : no , 1 : yes*/
	uint32_t flag_chg_status : 1; /* 0 : discharging, 1: charging*/
	uint32_t offs_speci_case : 1;
	uint32_t init_once : 1;
	uint32_t volt_alert_flag : 1; /* 0 : nu-occur, 1: occur */
	uint32_t soc_alert_flag : 1;  /* 0 : nu-occur, 1: occur */
	uint32_t bat_pres_flag : 1; /* 0 : removed, 1: inserted */
	uint32_t flag_once_full_soc : 1;
};

/*
struct rt5033_battery_info {
    struct rt_battery_info base;

};
*/


#endif // RT5033_FUELGAUGE_H
