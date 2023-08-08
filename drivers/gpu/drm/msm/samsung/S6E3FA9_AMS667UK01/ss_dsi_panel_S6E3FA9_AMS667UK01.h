/*
 * =================================================================
 *
 *
 *	Description:  samsung display panel file
 *
 *	Author: jb09.kim
 *	Company:  Samsung Electronics
 *
 * ================================================================
 */
/*
<one line to give the program's name and a brief idea of what it does.>
Copyright (C) 2012, Samsung Electronics. All rights reserved.

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
#ifndef SAMSUNG_DSI_PANEL_S6E3FA9_AMS667UK01_H
#define SAMSUNG_DSI_PANEL_S6E3FA9_AMS667UK01_H

#include <linux/completion.h>
#include "ss_dsi_panel_common.h"

#define MAX_BL_PF_LEVEL 255
#define MAX_HBM_PF_LEVEL 365
#define HBM_ELVSS_INTERPOLATION_VALUE 404 /*BL control register is 10 bit*/

static unsigned int elvss_table[HBM_ELVSS_INTERPOLATION_VALUE + 1] = {
	[0 ... 3]	=	0x1F,
	[4 ... 50]	=	0x1E,
	[51 ... 100]	=	0x1D,
	[101 ... 150]	=	0x1C,
	[151 ... 200]	=	0x1B,
	[201 ... 250]	=	0x19,
	[251 ... 300]	=	0x18,
	[301 ... 350]	=	0x17,
	[351 ... 404]	=	0x16,
};

#endif
