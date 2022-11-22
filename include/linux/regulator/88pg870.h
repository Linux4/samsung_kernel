/*
 * 88pg870.h - Marvell DC/DC Regulator 88PG870 Driver
 *
 * Copyright (C) 2013 Marvell Technology Ltd.
 * Yipeng Yao <ypyao@marvell.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __88PG870_H__

/* Ramp rate limiting during DVC change.
 * -----------------------
 *   Bin |Ramp Rate(mV/uS)
 * ------|----------------
 *   000 |     0.25
 * ------|----------------
 *   001 |     0.50
 * ------|----------------
 *   010 |     1.00
 * ------|----------------
 *   011 |     2.00
 * ------|----------------
 *   100 |     3.50
 * ------|----------------
 *   101 |     7.00
 * ------|----------------
 *   110 |    14.00
 * ------|----------------
 *   111 |  Reserved
 * -----------------------
 */
enum {
	PG870_RAMP_RATE_250UV = 0,
	PG870_RAMP_RATE_500UV,
	PG870_RAMP_RATE_1000UV,
	PG870_RAMP_RATE_2000UV,
	PG870_RAMP_RATE_3500UV,
	PG870_RAMP_RATE_7000UV,
	PG870_RAMP_RATE_14000UV,
};

/* System Control Register Setting.
 * ----------------------------------------
 *   Bit |  Description
 * ------|---------------------------------
 *   [0] | VSEL0 pull down resistor enable
 * ------|---------------------------------
 *   [1] | VSEL1 pull down resistor enable
 * ------|---------------------------------
 *   [2] | ENABLE pull down resistor enable
 * ------|---------------------------------
 *   [3] | Always set to 1
 * ------|---------------------------------
 *   [4] | Always set to 1
 * ------|---------------------------------
 *   [5] | PWM mode enable
 * ------|---------------------------------
 *   [6] | Relaxed mode enable
 * ------|---------------------------------
 *   [7] | Always set to 0
 * ------|---------------------------------
 */

/* Sleep Mode select */
enum {
	/* turn off buck converter in sleep mode */
	PG870_SM_TURN_OFF = 0,
	/* use sleep voltage in sleep mode */
	PG870_SM_RUN_SLEEP,
	/* use sleep voltage and enter low power in sleep mode */
	PG870_SM_LPM_SLEEP,
	/* use active voltage in sleep mode */
	PG870_SM_RUN_ACTIVE,
	/* sleep mode disable */
	PG870_SM_DISABLE,
};

struct pg870_platform_data {
	struct regulator_init_data *regulator;
	unsigned int ramp_rate;
	/* Sleep System Control Setting */
	unsigned int sysctrl;
	/* Sleep Mode */
	unsigned int sleep_mode;
	/* Sleep Voltage */
	unsigned int sleep_vol;
	/* Voltage select table idx */
	unsigned int vsel_idx;
};

#endif /* __88PG870_H__ */

