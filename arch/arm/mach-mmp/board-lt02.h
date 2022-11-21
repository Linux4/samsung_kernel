/*
 * Copyright (c) 2012 Samsung Electronics Co., Ltd.
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

#ifndef __MACH_MARVELL_BOARD_LT02_H
#define __MACH_MARVELL_BOARD_LT02_H

#if defined(CONFIG_BATTERY_SAMSUNG)
#include <linux/battery/sec_charging_common.h>
extern int current_cable_type;
extern sec_battery_platform_data_t sec_battery_pdata;
#endif

void pxa986_lt02_charger_init(void);
void sec_charger_cb(u8 attached);

void mmp_pins_init(void);

/** @category IRLED */
#ifdef CONFIG_SEC_IRLED
void pxa986_lt02_irled_init(void);
#endif

#endif
