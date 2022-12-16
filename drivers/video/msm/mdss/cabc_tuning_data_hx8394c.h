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

#ifndef _CABC_TUNING_DATA_H_
#define _CABC_TUNING_DATA_H_
////////////////// UI /// /////////////////////

static char CABC_OFF_1[] = {
	0x55,
	0x00
};

static char CABC_OFF_2[] = {
	0xCA,
	0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20
};


////////////////// UI /// /////////////////////

static char CABC_UI_1[] = {
	0x55,
	0x01
};

static char CABC_UI_2[] = {
	0xCA,
	0x27, 0x27, 0x27, 0x27, 0x27, 0x27, 0x27, 0x27, 0x27
};



////////////////// STILL /////////////////////
static char CABC_STILL_1[] = {
	0x55,
	0x02
};


static char CABC_STILL_2[] = {
	0xCA,
	0x27, 0x27, 0x27, 0x27, 0x27, 0x27, 0x27, 0x27, 0x27
};


////////////////// MOVING /////////////////////

static char CABC_MOVING_1[] = {
	0x55,
	0x03
};


static char CABC_MOVING_2[] = {
	0xCA,
	0x27, 0x27, 0x27, 0x27, 0x27, 0x27, 0x27, 0x27, 0x27
};



char *cabc_tune_value[CABC_MODE_MAX][2] = {
		/*
			CABC_OFF_MODE
			CABC_UI_MODE
			CABC_STILL_MODE
			CABC_MOVING_MODE
		*/
		// OFF
		{
			CABC_OFF_1, CABC_OFF_2,
		},
		// UI
		{
			CABC_UI_1, CABC_UI_2,
		},
		// STILL
		{
			CABC_STILL_1, CABC_STILL_2,
		},
		// MOVING
		{
			CABC_MOVING_1, CABC_MOVING_2,
		},
};
#endif
