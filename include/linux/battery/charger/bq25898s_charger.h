/*
 * bq25898s_charger.h
 * Samsung BQ25898S Charger Header
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

#ifndef __BQ25898S_CHARGER_H
#define __BQ25898S_CHARGER_H __FILE__

#include <linux/battery/sec_charging_common.h>

#define BQ25898S_CHG_REG_00			0x00
#define BQ25898S_CHG_ENABLE_HIZ_MODE_SHIFT	7
#define BQ25898S_CHG_ENABLE_HIZ_MODE_MASK	(1 << BQ25898S_CHG_ENABLE_HIZ_MODE_SHIFT)
#define BQ25898S_CHG_IINLIM_MASK		0x3F

#define BQ25898S_CHG_REG_03			0x03
#define BQ25898S_CHG_CONFIG_SHIFT		4
#define BQ25898S_CHG_CONFIG_MASK		(1 << BQ25898S_CHG_CONFIG_SHIFT)

#define BQ25898S_CHG_REG_04			0x04
#define BQ25898S_CHG_ICHG_MASK			0x3F

#define BQ25898S_CHG_REG_06			0x06
#define BQ25898S_CHG_VREG_MASK			0xFC

#define BQ25898S_CHG_REG_07			0x07
#define BQ25898S_CHG_WATCHDOG_SHIFT		4
#define BQ25898S_CHG_WATCHDOG_MASK		(0x3 << BQ25898S_CHG_WATCHDOG_SHIFT)

#define SIOP_INPUT_LIMIT_CURRENT                1400
#define SIOP_CHARGING_LIMIT_CURRENT             1400
#define SLOW_CHARGING_CURRENT_STANDARD          400

enum bq25898s_watchdog_timer {
	WATCHDOG_TIMER_DISABLE = 0,
	WATCHDOG_TIMER_40S,
	WATCHDOG_TIMER_80S,
	WATCHDOG_TIMER_160S,
};

struct bq25898s_charger {
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

	int input_curr_limit_step;
	int wpc_input_curr_limit_step;
	int charging_curr_step;

	sec_charger_platform_data_t	*pdata;
};
#endif /* __BQ25898S_CHARGER_H */
