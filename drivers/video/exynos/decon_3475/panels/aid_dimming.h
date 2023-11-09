/* linux/drivers/video/exynos_decon/panel/aid_dimming.h
 *
 * Header file for Samsung AID Dimming Driver.
 *
 * Copyright (c) 2013 Samsung Electronics
 * Minwoo Kim <minwoo7945.kim@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#ifndef __AID_DIMMING_H__
#define __AID_DIMMING_H__

#if defined(CONFIG_PANEL_S6E3FA2_DYNAMIC)
#include "s6e3fa2_dimming.h"
#elif defined(CONFIG_PANEL_S6E3FA3_DYNAMIC)
#include "s6e3fa3_dimming.h"
#elif defined(CONFIG_PANEL_S6E3HF2_DYNAMIC)
#include "s6e3hf2_wqhd_dimming.h"
#elif defined(CONFIG_EXYNOS3475_DECON_LCD_S6E88A0)
#include "s6e88a0_dimming.h"
#elif defined(CONFIG_EXYNOS3475_DECON_LCD_S6E8AA5X01)
#include "s6e8aa5x01_dimming.h"
#elif defined(CONFIG_EXYNOS3475_DECON_LCD_EA8061S_J1)
#include "ea8061s_dimming_j1.h"
#else
#error "ERROR !! Check LCD Panel Header File"
#endif

enum {
	CI_RED = 0,
	CI_GREEN,
	CI_BLUE,
	CI_MAX,
};

struct dim_data {
	int t_gamma[NUM_VREF][CI_MAX];
	int mtp[NUM_VREF][CI_MAX];
	int volt[MAX_GRADATION][CI_MAX];
	int volt_vt[CI_MAX];
	int vt_mtp[CI_MAX];
	int look_volt[NUM_VREF][CI_MAX];
};

struct SmtDimInfo {
	unsigned int br;
	unsigned int refBr;
	const unsigned int *cGma;
	const signed char *rTbl;
	const signed char *cTbl;
	const unsigned char *aid;
	const unsigned char *elvAcl;
	const unsigned char *elv;
	unsigned char gamma[OLED_CMD_GAMMA_CNT];
};

#endif

