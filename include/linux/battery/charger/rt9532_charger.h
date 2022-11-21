/*
 * RT9532_charger.h
 * Richtek RT9532 Charger Header
 *
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

#ifndef __RT9532_CHARGER_H
#define __RT9532_CHARGER_H __FILE__

#include <linux/battery/sec_charging_common.h>

enum {
	ISET_MODE = 1,
	USB100_MODE,
	FACTORY_MODE,
	USB500_MODE,
};

#define RESET_TIME 2000
#define WAIT_TIME 2000
#define LOW_PERIOD 150
#define HIGH_PERIOD 150
#define CV_HIGH_PERIOD 800
#define SWELLING_PERIOD 500

/*
  mode = ISET_MODE / USB100_MODE / FACTORY_MODE / USB500_MODE
  cv_set = flase : 4.2V / true : 4.35V
*/

struct rt9532_charger {
	struct device *dev;
	sec_battery_platform_data_t	*pdata;
	struct power_supply	psy_chg;

	int en_set;
	int chg_state;
	int chg_det;

	spinlock_t io_lock;

	unsigned int	is_charging;
	unsigned int	cable_type;
	unsigned int	charging_current_max;
	unsigned int	charging_current;
	unsigned int	input_current_limit;
	unsigned int	vbus_state;
	bool is_fullcharged;
	int		aicl_on;
	int		status;
	int siop_level;
	int		input_curr_limit_step;
	int		charging_curr_step;
};

#endif /* __RT9532_CHARGER_H */
