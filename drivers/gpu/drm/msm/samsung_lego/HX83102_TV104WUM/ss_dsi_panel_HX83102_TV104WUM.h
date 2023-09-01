/*
 * =================================================================
 *
 *
 *	Description:  samsung display panel file
 *
 *	Company:  Samsung Electronics
 *
 * ================================================================
 */
/*
<one line to give the program's name and a brief idea of what it does.>
Copyright (C) 2020, Samsung Electronics. All rights reserved.

*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 *
*/
#ifndef SAMSUNG_DSI_PANEL_TD4150_BOE531_H
#define SAMSUNG_DSI_PANEL_TD4150_BOE531_H

#include <linux/completion.h>
#include "ss_dsi_panel_common.h"
#include "ss_buck_isl98609_i2c.h"

#define MAX_BL_PF_LEVEL 255
#define MAX_HBM_PF_LEVEL 365 /*BL control register is 10 bit*/

static unsigned int elvss_table[MAX_HBM_PF_LEVEL + 1] = {
	[0 ... 255] = 0x0A,
	[256 ... 266] = 0x13,
	[267 ... 277] = 0x12,
	[278 ... 288] = 0x11,
	[289 ... 299] = 0x10,
	[300 ... 310] = 0x0F,
	[311 ... 321] = 0x0E,
	[322 ... 332] = 0x0D,
	[333 ... 343] = 0x0C,
	[344 ... 354] = 0x0B,
	[355 ... MAX_HBM_PF_LEVEL] = 0x0A,
};

#endif
