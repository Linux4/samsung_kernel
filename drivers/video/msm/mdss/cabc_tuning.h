/* Copyright (c) 2009-2011, Code Aurora Forum. All rights reserved.
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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 *
 */

#ifndef _CABC_TUNING_H_
#define _CABC_TUNING_H_

#if defined(CONFIG_FB_MSM_MDSS_PANEL_COMMON)
#include "mdss_dsi_panel_common.h"
#endif

enum CABC_MODE {
	CABC_MODE_OFF,
	CABC_MODE_UI,
	CABC_MODE_STILL,
	CABC_MODE_MOVING,
	CABC_MODE_MAX,
};

struct cabc_type {
	bool enable;
	enum CABC_MODE mode;
};

void cabc_tuning_init(struct mdss_samsung_driver_data *msd);
void init_cabc_class(void);
void cabc_set_mode(void);


#endif /*_CABC_TUNING_H_*/
