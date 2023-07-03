/*
 * bq24773_charger.h
 * Samsung bq24773 Charger Header
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

#ifndef __bq24773_CHARGER_H
#define __bq24773_CHARGER_H __FILE__

#include <linux/mfd/core.h>
#include <linux/regulator/machine.h>
#include <linux/battery/sec_charging_common.h>

#define BQ24773_CHG_OPTION0_1 0x00
#define BQ24773_CHG_OPTION0_2 0x01
#define BQ24773_CHG_OPTION1_1 0x02
#define BQ24773_CHG_OPTION1_2 0x03
#define BQ24773_CHG_OPTION2 0x38
#define BQ24773_PROCHOT_OPTION0_1 0x04
#define BQ24773_PROCHOT_OPTION0_2 0x05
#define BQ24773_PROCHOT_OPTION1_1 0x06
#define BQ24773_PROCHOT_OPTION1_2 0x07
#define BQ24773_CHG_CURR_1 0x0A
#define BQ24773_CHG_CURR_2 0x0B
#define BQ24773_MAX_CHG_VOLT_1 0x0C
#define BQ24773_MAX_CHG_VOLT_2 0x0D
#define BQ24773_MIN_SYS_VOLT 0x0E
#define BQ24773_INPUT_CURR 0x0F

#define SIOP_INPUT_LIMIT_CURRENT                1200
#define SIOP_CHARGING_LIMIT_CURRENT             1000
#define SIOP_WIRELESS_INPUT_LIMIT_CURRENT       660
#define SIOP_WIRELESS_CHARGING_LIMIT_CURRENT    780
#define SLOW_CHARGING_CURRENT_STANDARD          400

struct bq24773_charger {
	struct device           *dev;
	struct i2c_client       *i2c;

	struct mutex            i2c_lock;

	struct power_supply	psy_chg;
	struct power_supply	psy_otg;

	struct workqueue_struct *wqueue;
	struct delayed_work     afc_work;

	bool	        is_charging;
	unsigned int	charging_type;
	unsigned int	battery_state;
	unsigned int	battery_present;
	unsigned int	cable_type;
	unsigned int	input_current;
	unsigned int	charging_current;
	unsigned int	input_current_limit;
	unsigned int	vbus_state;
	unsigned int    chg_float_voltage;
	int		status;
	int		siop_level;

	int             otg_en;
	int             otg_en2;

	/* software regulation */
	bool		soft_reg_state;
	int		soft_reg_current;

	/* unsufficient power */
	bool		reg_loop_deted;

	bool afc_detect;

	int input_curr_limit_step;
	int wpc_input_curr_limit_step;
	int charging_curr_step;

	sec_charger_platform_data_t	*pdata;
};
#endif /* __bq24773_CHARGER_H */
