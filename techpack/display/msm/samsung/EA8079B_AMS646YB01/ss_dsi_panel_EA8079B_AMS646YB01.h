/*
 * =================================================================
 *
 *
 *	Description:  samsung display panel file
 *
 *	Author: cj1225.jang
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
#ifndef SAMSUNG_DSI_PANEL_EA8079B_AMS646YB01_H
#define SAMSUNG_DSI_PANEL_EA8079B_AMS646YB01_H

#include <linux/completion.h>
#include "ss_dsi_panel_common.h"

#define MAX_BL_PF_LEVEL 255
#define MAX_HBM_PF_LEVEL 486
#define HBM_ELVSS_INTERPOLATION_VALUE 613 /*BL control register is 10 bit*/

static unsigned int elvss_table_hbm[HBM_ELVSS_INTERPOLATION_VALUE + 1] = {
        [0 ... 36]	=	0x25,
        [37 ... 74]	=	0x24,
        [75 ... 111]	=	0x23,
        [112 ... 151]	=	0x22,
        [152 ... 188]	=	0x21,
        [189 ... 226]	=	0x20,
        [227 ... 266]	=	0x1F,
        [267 ... 305]	=	0x1E,
        [306 ... 343]	=	0x1D,
        [344 ... 383]   =	0x1C,
        [384 ... 420]   =	0x1B,
        [421 ... 460]   =	0x1A,
        [461 ... 497]   =	0x19,
        [498 ... 535]   =	0x18,
        [536 ... 575]   =	0x17,
        [576 ... 613]   =	0x16,
};

#endif
