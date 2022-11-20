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

#if defined(CONFIG_PANEL_EA8064G_DYNAMIC)
#include "ea8064g_dimming.h"
#elif defined(CONFIG_PANEL_EA8061_DYNAMIC)
#include "ea8061_dimming.h"
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
	signed char *cTbl;
	unsigned char *aid;
	unsigned char *elvAcl;
	unsigned char *elv;
	unsigned char *m_gray;
	unsigned char gamma[OLED_CMD_GAMMA_CNT];
};

#endif

