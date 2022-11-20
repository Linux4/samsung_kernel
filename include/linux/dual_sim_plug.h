/*
 * Copyright (C) 2012 Spreadtrum Communications Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __DUAL_SIM_PLUG_H
#define __DUAL_SIM_PLUG_H

struct dual_sim_plug_init_data {
	char	*name;
	uint8_t	dst;
	uint8_t	channel;
	uint32_t	sim1_gpio;
	uint32_t	sim2_gpio;
};

#endif