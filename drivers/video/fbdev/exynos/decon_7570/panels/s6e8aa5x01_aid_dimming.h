/* linux/drivers/video/exynos/decon_7580/panels/s6e8aa5x01_aid_dimming.h
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

#ifndef __S6E8AA5X01_AID_DIMMING_H__
#define __S6E8AA5X01_AID_DIMMING_H__

/* s6e8aa5x01 */

static signed char ctbl5nit[30] = {0, 0, 0, 0, 0, 0, 0, 0, 1, -14, 0, -2, -6, 0, -3, -5, 0, 4, -3, 0, 0, -1, 0, 2, 0, 0, 1, 2, 0, 1};
static signed char ctbl6nit[30] = {0, 0, 0, 0, 0, 0, -3, 0, 1, -13, 0, -4, -6, 0, -3, -4, 0, 4, -2, 0, 0, -1, 0, 1, 0, 0, 1, 2, 0, 1};
static signed char ctbl7nit[30] = {0, 0, 0, 0, 0, 0, -6, 0, 0, -13, 0, -3, -5, 0, -3, -4, 0, 3, -3, 0, 0, 0, 0, 2, 0, 0, 1, 2, 0, 1};
static signed char ctbl8nit[30] = {0, 0, 0, 0, 0, 0, -8, 0, 0, -12, 0, -4, -5, 0, -4, -4, 0, 3, -1, 0, 1, 0, 0, 2, 0, 0, 1, 2, 0, 1};
static signed char ctbl9nit[30] = {0, 0, 0, 0, 0, 0, -10, 0, 1, -10, 0, -2, -5, 0, -5, -3, 0, 3, -1, 0, 1, 0, 0, 2, 0, 0, 1, 2, 0, 1};
static signed char ctbl10nit[30] = {0, 0, 0, 0, 0, 0, -12, 0, -1, -8, 0, -2, -4, 0, -3, -4, 0, 2, 0, 0, 1, -1, 0, 1, 0, 0, 1, 2, 0, 1};
static signed char ctbl11nit[30] = {0, 0, 0, 0, 0, 0, -13, 0, -1, -9, 0, -3, -4, 0, -3, -3, 0, 2, 0, 0, 1, -1, 0, 1, 0, 0, 1, 2, 0, 1};
static signed char ctbl12nit[30] = {0, 0, 0, 0, 0, 0, -13, 0, -1, -9, 0, -2, -3, 0, -5, -2, 0, 3, -1, 0, 1, -1, 0, 1, 0, 0, 1, 2, 0, 1};
static signed char ctbl13nit[30] = {0, 0, 0, 0, 0, 0, -11, 0, -1, -7, 0, -1, -3, 0, -5, -3, 0, 2, -1, 0, 1, 0, 0, 1, 0, 0, 1, 2, 0, 1};
static signed char ctbl14nit[30] = {0, 0, 0, 0, 0, 0, -11, 0, -1, -5, 0, -2, -2, 0, -5, -2, 0, 3, -2, 0, 0, 0, 0, 2, 0, 0, 1, 2, 0, 1};
static signed char ctbl15nit[30] = {0, 0, 0, 0, 0, 0, -11, 0, -2, -7, 0, -3, -1, 0, -4, -2, 0, 3, -2, 0, 0, 0, 0, 1, 0, 0, 1, 2, 0, 1};
static signed char ctbl16nit[30] = {0, 0, 0, 0, 0, 0, -13, 0, -2, -5, 0, -3, -1, 0, -4, -1, 0, 3, -2, 0, 0, 0, 0, 1, 0, 0, 1, 2, 0, 1};
static signed char ctbl17nit[30] = {0, 0, 0, 0, 0, 0, -11, 0, 0, -5, 0, -3, -1, 0, -4, -2, 0, 2, -2, 0, 0, 0, 0, 2, 0, 0, 1, 2, 0, 1};
static signed char ctbl19nit[30] = {0, 0, 0, 0, 0, 0, -9, 0, 1, -5, 0, -4, -1, 0, -4, -2, 0, 2, 0, 0, 1, 0, 0, 2, -1, 0, 0, 2, 0, 1};
static signed char ctbl20nit[30] = {0, 0, 0, 0, 0, 0, -8, 0, 2, -5, 0, -3, -1, 0, -4, -2, 0, 2, 0, 0, 1, 0, 0, 2, -1, 0, 0, 2, 0, 1};
static signed char ctbl21nit[30] = {0, 0, 0, 0, 0, 0, -7, 0, 0, -6, 0, -5, 0, 0, -4, -2, 0, 2, 0, 0, 1, 0, 0, 2, -1, 0, 0, 2, 0, 1};
static signed char ctbl22nit[30] = {0, 0, 0, 0, 0, 0, -7, 1, -3, -6, 0, -4, 0, 0, -4, -2, 0, 2, 0, 0, 1, 0, 0, 2, -1, 0, 0, 2, 0, 1};
static signed char ctbl24nit[30] = {0, 0, 0, 0, 0, 0, -7, 1, -3, -6, 0, -6, 0, 0, -4, -2, 0, 2, 0, 0, 1, 0, 0, 2, -1, 0, 0, 2, 0, 1};
static signed char ctbl25nit[30] = {0, 0, 0, 0, 0, 0, -9, 0, -4, -5, 0, -5, 0, 0, -5, -3, 0, 1, 0, 0, 1, 0, 0, 2, -1, 0, 0, 2, 0, 1};
static signed char ctbl27nit[30] = {0, 0, 0, 0, 0, 0, -9, 0, -5, -3, 0, -5, 0, 0, -5, -2, 0, 1, -1, 0, 0, 0, 0, 2, -1, 0, 1, 2, 0, 1};
static signed char ctbl29nit[30] = {0, 0, 0, 0, 0, 0, -9, 0, -5, -3, 0, -5, 0, 0, -5, -2, 0, 1, -1, 0, 0, 0, 0, 2, -1, 0, 1, 2, 0, 1};
static signed char ctbl30nit[30] = {0, 0, 0, 0, 0, 0, -9, 0, -7, -4, 0, -5, 0, 0, -6, -2, 0, 1, -1, 0, 0, 0, 0, 2, -1, 0, 1, 2, 0, 1};
static signed char ctbl32nit[30] = {0, 0, 0, 0, 0, 0, -9, 0, -6, -3, 0, -5, 1, 0, -6, -2, 0, 1, -1, 0, 0, 0, 0, 2, -1, 0, 1, 2, 0, 0};
static signed char ctbl34nit[30] = {0, 0, 0, 0, 0, 0, -9, 0, -6, -3, 0, -4, 1, 0, -6, -1, 0, 1, -1, 0, 0, 0, 0, 2, -1, 0, 1, 2, 0, 0};
static signed char ctbl37nit[30] = {0, 0, 0, 0, 0, 0, -9, 0, -7, -2, 0, -4, 1, 0, -6, -1, 0, 1, -1, 0, 0, 0, 0, 2, -1, 0, 1, 2, 0, 0};
static signed char ctbl39nit[30] = {0, 0, 0, 0, 0, 0, -8, 0, -4, -2, 0, -5, 1, 0, -6, -1, 0, 1, -1, 0, 0, 0, 0, 2, -1, 0, 1, 2, 0, 0};
static signed char ctbl41nit[30] = {0, 0, 0, 0, 0, 0, -8, 0, -3, -1, 0, -4, 2, 0, -6, -1, 0, 1, -1, 0, 0, -1, 0, 2, -1, 0, 1, 2, 0, 0};
static signed char ctbl44nit[30] = {0, 0, 0, 0, 0, 0, -7, 0, -2, -1, 0, -4, 2, 0, -6, -1, 0, 1, -1, 0, 0, -1, 0, 2, -1, 0, 1, 2, 0, 0};
static signed char ctbl47nit[30] = {0, 0, 0, 0, 0, 0, -6, 0, -3, -1, 0, -4, 2, 0, -6, -1, 0, 1, -1, 0, 0, -1, 0, 2, -1, 0, 1, 2, 0, 0};
static signed char ctbl50nit[30] = {0, 0, 0, 0, 0, 0, -5, 0, -1, -2, 0, -4, 3, 0, -5, -1, 0, 1, -1, 0, 0, -1, 0, 2, -1, 0, 1, 2, 0, 0};
static signed char ctbl53nit[30] = {0, 0, 0, 0, 0, 0, -5, 0, 0, -1, 0, -4, 3, 0, -5, -1, 0, 1, -1, 0, 0, -1, 0, 2, -1, 0, 1, 2, 0, 0};
static signed char ctbl56nit[30] = {0, 0, 0, 0, 0, 0, -5, 0, 1, -1, 0, -4, 3, 0, -5, -1, 0, 1, -1, 0, 0, -1, 0, 2, -1, 0, 1, 2, 0, 0};
static signed char ctbl60nit[30] = {0, 0, 0, 0, 0, 0, -5, 0, 3, 0, 0, -4, 3, 0, -5, -1, 0, 1, -1, 0, 0, -1, 0, 2, -1, 0, 1, 2, 0, 0};
static signed char ctbl64nit[30] = {0, 0, 0, 0, 0, 0, -5, 0, 4, 0, 0, -4, 3, 0, -5, -1, 0, 1, -1, 0, 0, -1, 0, 2, -1, 0, 1, 2, 0, 0};
static signed char ctbl68nit[30] = {0, 0, 0, 0, 0, 0, -6, 0, 0, 0, 0, -4, 3, 0, -5, 0, 0, 2, -1, 0, 0, -1, 0, 1, -1, 0, 0, 1, 0, 0};
static signed char ctbl72nit[30] = {0, 0, 0, 0, 0, 0, -5, 0, 0, 0, 0, -4, 2, 0, -4, 0, 0, 2, -1, 0, 0, -1, 0, 1, -1, 0, 0, 1, 0, 0};
static signed char ctbl77nit[30] = {0, 0, 0, 0, 0, 0, -3, 0, 0, 0, 0, -3, 1, 0, -5, 0, 0, 2, -1, 0, 0, -1, 0, 1, -1, 0, 0, 1, 0, 0};
static signed char ctbl82nit[30] = {0, 0, 0, 0, 0, 0, -5, 0, 0, 0, 0, -3, 1, 0, -4, 0, 0, 2, -1, 0, 0, -1, 0, 1, -1, 0, 0, 1, 0, 0};
static signed char ctbl87nit[30] = {0, 0, 0, 0, 0, 0, -3, 0, 1, 0, 0, -4, 1, 0, -4, 0, 0, 3, -1, 0, -1, -1, 0, 1, -1, 0, 0, 1, 0, 0};
static signed char ctbl93nit[30] = {0, 0, 0, 0, 0, 0, -3, 0, 0, 0, 0, -4, 3, 0, -4, -1, 0, 2, -1, 0, -1, -1, 0, 1, -1, 0, 0, 1, 0, 0};
static signed char ctbl98nit[30] = {0, 0, 0, 0, 0, 0, -3, 0, 0, 0, 0, -4, 3, 0, -3, 0, 0, 2, -1, 0, -1, -1, 0, 1, -1, 0, 0, 1, 0, 0};
static signed char ctbl105nit[30] = {0, 0, 0, 0, 0, 0, -4, 0, 0, 0, 0, -4, 1, 0, -4, 0, 0, 2, -1, 0, -1, 0, 0, 1, -1, 0, 0, 1, 0, 0};
static signed char ctbl111nit[30] = {0, 0, 0, 0, 0, 0, -3, 0, -1, 0, 0, -4, 1, 0, -3, 0, 0, 2, -1, 0, -1, 0, 0, 1, 0, 0, 0, 1, 0, 0};
static signed char ctbl119nit[30] = {0, 0, 0, 0, 0, 0, -2, 0, -1, 1, 0, -1, 0, 0, -3, 0, 0, 0, -1, 0, 0, -1, 0, 1, 1, 0, -1, 0, 0, 0};
static signed char ctbl126nit[30] = {0, 0, 0, 0, 0, 0, -1, 0, 0, 1, 0, -1, 1, 0, -3, -1, 0, 1, -1, 0, 0, -1, 0, 0, 1, 0, -1, 0, 0, 0};
static signed char ctbl134nit[30] = {0, 0, 0, 0, 0, 0, -1, 0, 0, 1, 0, -1, 1, 0, -3, -1, 0, 1, -1, 0, 0, -1, 0, 0, 1, 0, -1, 0, 0, 0};
static signed char ctbl143nit[30] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, -1, 1, 0, -3, -1, 0, 1, -1, 0, 0, -1, 0, 0, 1, 0, -1, 0, 0, 0};
static signed char ctbl152nit[30] = {0, 0, 0, 0, 0, 0, 1, 0, 1, 1, 0, -1, 1, 0, -3, -1, 0, 1, -1, 0, 0, -1, 0, 0, 1, 0, -1, 0, 0, 0};
static signed char ctbl162nit[30] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, -2, 1, 0, -3, -1, 0, 1, 0, 0, 1, -1, 0, 0, 1, 0, -1, 0, 0, 0};
static signed char ctbl172nit[30] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, -2, 1, 0, -3, -1, 0, 1, 0, 0, 1, -1, 0, 0, 1, 0, -1, 0, 0, 0};
static signed char ctbl183nit[30] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static signed char ctbl195nit[30] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static signed char ctbl207nit[30] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static signed char ctbl220nit[30] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static signed char ctbl234nit[30] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static signed char ctbl249nit[30] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static signed char ctbl265nit[30] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static signed char ctbl282nit[30] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static signed char ctbl300nit[30] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static signed char ctbl316nit[30] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static signed char ctbl333nit[30] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static signed char ctbl360nit[30] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

static signed char aid5[5] = {0xB2, 0x04, 0xEE, 0x00, 0x0F};
static signed char aid6[5] = {0xB2, 0x04, 0xE1, 0x00, 0x0F};
static signed char aid7[5] = {0xB2, 0x04, 0xD4, 0x00, 0x0F};
static signed char aid8[5] = {0xB2, 0x04, 0xCF, 0x00, 0x0F};
static signed char aid9[5] = {0xB2, 0x04, 0xC2, 0x00, 0x0F};
static signed char aid10[5] = {0xB2, 0x04, 0xB4, 0x00, 0x0F};
static signed char aid11[5] = {0xB2, 0x04, 0xAF, 0x00, 0x0F};
static signed char aid12[5] = {0xB2, 0x04, 0xA3, 0x00, 0x0F};
static signed char aid13[5] = {0xB2, 0x04, 0x96, 0x00, 0x0F};
static signed char aid14[5] = {0xB2, 0x04, 0x91, 0x00, 0x0F};
static signed char aid15[5] = {0xB2, 0x04, 0x82, 0x00, 0x0F};
static signed char aid16[5] = {0xB2, 0x04, 0x75, 0x00, 0x0F};
static signed char aid17[5] = {0xB2, 0x04, 0x70, 0x00, 0x0F};
static signed char aid19[5] = {0xB2, 0x04, 0x54, 0x00, 0x0F};
static signed char aid20[5] = {0xB2, 0x04, 0x50, 0x00, 0x0F};
static signed char aid21[5] = {0xB2, 0x04, 0x42, 0x00, 0x0F};
static signed char aid22[5] = {0xB2, 0x04, 0x33, 0x00, 0x0F};
static signed char aid24[5] = {0xB2, 0x04, 0x1E, 0x00, 0x0F};
static signed char aid25[5] = {0xB2, 0x04, 0x10, 0x00, 0x0F};
static signed char aid27[5] = {0xB2, 0x03, 0xF2, 0x00, 0x0F};
static signed char aid29[5] = {0xB2, 0x03, 0xDE, 0x00, 0x0F};
static signed char aid30[5] = {0xB2, 0x03, 0xC6, 0x00, 0x0F};
static signed char aid32[5] = {0xB2, 0x03, 0xB1, 0x00, 0x0F};
static signed char aid34[5] = {0xB2, 0x03, 0x95, 0x00, 0x0F};
static signed char aid37[5] = {0xB2, 0x03, 0x73, 0x00, 0x0F};
static signed char aid39[5] = {0xB2, 0x03, 0x5F, 0x00, 0x0F};
static signed char aid41[5] = {0xB2, 0x03, 0x43, 0x00, 0x0F};
static signed char aid44[5] = {0xB2, 0x03, 0x20, 0x00, 0x0F};
static signed char aid47[5] = {0xB2, 0x02, 0xFE, 0x00, 0x0F};
static signed char aid50[5] = {0xB2, 0x02, 0xDF, 0x00, 0x0F};
static signed char aid53[5] = {0xB2, 0x02, 0xB2, 0x00, 0x0F};
static signed char aid56[5] = {0xB2, 0x02, 0x90, 0x00, 0x0F};
static signed char aid60[5] = {0xB2, 0x02, 0x5E, 0x00, 0x0F};
static signed char aid64[5] = {0xB2, 0x02, 0x30, 0x00, 0x0F};
static signed char aid68[5] = {0xB2, 0x01, 0xF4, 0x00, 0x0F};
static signed char aid72[5] = {0xB2, 0x01, 0xF4, 0x00, 0x0F};
static signed char aid77[5] = {0xB2, 0x01, 0xF4, 0x00, 0x0F};
static signed char aid82[5] = {0xB2, 0x01, 0xF4, 0x00, 0x0F};
static signed char aid87[5] = {0xB2, 0x01, 0xF4, 0x00, 0x0F};
static signed char aid93[5] = {0xB2, 0x01, 0xF4, 0x00, 0x0F};
static signed char aid98[5] = {0xB2, 0x01, 0xF4, 0x00, 0x0F};
static signed char aid105[5] = {0xB2, 0x01, 0xF4, 0x00, 0x0F};
static signed char aid110[5] = {0xB2, 0x01, 0xF4, 0x00, 0x0F};
static signed char aid119[5] = {0xB2, 0x01, 0xF4, 0x00, 0x0F};
static signed char aid126[5] = {0xB2, 0x01, 0xBF, 0x00, 0x0F};
static signed char aid134[5] = {0xB2, 0x01, 0x82, 0x00, 0x0F};
static signed char aid143[5] = {0xB2, 0x01, 0x40, 0x00, 0x0F};
static signed char aid152[5] = {0xB2, 0x01, 0x00, 0x00, 0x0F};
static signed char aid162[5] = {0xB2, 0x00, 0xB2, 0x00, 0x0F};
static signed char aid172[5] = {0xB2, 0x00, 0x62, 0x00, 0x0F};
static signed char aid183[5] = {0xB2, 0x00, 0x0F, 0x00, 0x0F};
static signed char aid195[5] = {0xB2, 0x00, 0x0F, 0x00, 0x0F};
static signed char aid207[5] = {0xB2, 0x00, 0x0F, 0x00, 0x0F};
static signed char aid220[5] = {0xB2, 0x00, 0x0F, 0x00, 0x0F};
static signed char aid234[5] = {0xB2, 0x00, 0x0F, 0x00, 0x0F};
static signed char aid249[5] = {0xB2, 0x00, 0x0F, 0x00, 0x0F};
static signed char aid265[5] = {0xB2, 0x00, 0x0F, 0x00, 0x0F};
static signed char aid282[5] = {0xB2, 0x00, 0x0F, 0x00, 0x0F};
static signed char aid300[5] = {0xB2, 0x00, 0x0F, 0x00, 0x0F};
static signed char aid316[5] = {0xB2, 0x00, 0x0F, 0x00, 0x0F};
static signed char aid333[5] = {0xB2, 0x00, 0x0F, 0x00, 0x0F};
static signed char aid360[5] = {0xB2, 0x00, 0x0F, 0x00, 0x0F};

/* [0xB6]  [1st] [2nd] [22th T > 0] [22th 0 >= T > -20] [22th T <= -20]*/
static unsigned char elv_HBM[6] = {0xB6, 0xBC, 0x0F, 0x00, 0x00, 0x00};
static unsigned char elv_360[6] = {0xB6, 0xBC, 0x0F, 0x00, 0x00, 0x00};
static unsigned char elv_333[6] = {0xB6, 0xBC, 0x10, 0x00, 0x00, 0x00};
static unsigned char elv_316[6] = {0xB6, 0xBC, 0x12, 0x00, 0x00, 0x00};
static unsigned char elv_300[6] = {0xB6, 0xBC, 0x13, 0x00, 0x00, 0x00};
static unsigned char elv_282[6] = {0xB6, 0xBC, 0x14, 0x00, 0x00, 0x00};
static unsigned char elv_265[6] = {0xB6, 0xBC, 0x16, 0x00, 0x00, 0x00};
static unsigned char elv_249[6] = {0xB6, 0xBC, 0x17, 0x00, 0x00, 0x00};
static unsigned char elv_234[6] = {0xB6, 0xBC, 0x18, 0x00, 0x00, 0x00};
static unsigned char elv_220[6] = {0xB6, 0xBC, 0x18, 0x00, 0x00, 0x00};
static unsigned char elv_207[6] = {0xB6, 0xBC, 0x1A, 0x00, 0x00, 0x00};
static unsigned char elv_195[6] = {0xB6, 0xBC, 0x1B, 0x00, 0x00, 0x00};
static unsigned char elv_183[6] = {0xB6, 0xBC, 0x1C, 0x00, 0x00, 0x00};
static unsigned char elv_143[6] = {0xB6, 0xBC, 0x1D, 0x00, 0x00, 0x00};
static unsigned char elv_111[6] = {0xB6, 0xBC, 0x1E, 0x00, 0x00, 0x00};
static unsigned char elv_41[6] = {0xB6, 0xBC, 0x1F, 0x00, 0x00, 0x00};
static unsigned char elv_30[6] = {0xB6, 0xAC, 0x1F, 0x00, 0x00, 0x00};
static unsigned char elv_29[6] = {0xB6, 0xAC, 0x1D, 0x07, 0x0A, 0x0A};
static unsigned char elv_27[6] = {0xB6, 0xAC, 0x1B, 0x07, 0x0B, 0x0C};
static unsigned char elv_25[6] = {0xB6, 0xAC, 0x19, 0x07, 0x0C, 0x0E};
static unsigned char elv_24[6] = {0xB6, 0xAC, 0x17, 0x07, 0x0D, 0x10};
static unsigned char elv_22[6] = {0xB6, 0xAC, 0x14, 0x07, 0x0E, 0x12};
static unsigned char elv_21[6] = {0xB6, 0xAC, 0x11, 0x07, 0x0F, 0x13};
static unsigned char elv_5[6] = {0xB6, 0xAC, 0x0F, 0x05, 0x0F, 0x13};

static unsigned char m_gray_005[] = {0, 7, 15, 20, 23, 32, 51, 87, 117, 143};
static unsigned char m_gray_006[] = {0, 7, 15, 19, 23, 32, 51, 86, 116, 143};
static unsigned char m_gray_007[] = {0, 7, 15, 19, 23, 32, 51, 87, 117, 143};
static unsigned char m_gray_008[] = {0, 7, 14, 18, 23, 31, 50, 87, 117, 143};
static unsigned char m_gray_009[] = {0, 7, 13, 18, 22, 31, 50, 87, 117, 143};
static unsigned char m_gray_010[] = {0, 7, 13, 18, 22, 31, 50, 86, 116, 143};
static unsigned char m_gray_011[] = {0, 7, 13, 17, 22, 30, 50, 86, 116, 143};
static unsigned char m_gray_012[] = {0, 7, 12, 17, 22, 31, 50, 87, 117, 143};
static unsigned char m_gray_013[] = {0, 7, 12, 17, 22, 30, 50, 86, 116, 143};
static unsigned char m_gray_014[] = {0, 7, 11, 17, 22, 31, 50, 87, 117, 143};
static unsigned char m_gray_015[] = {0, 7, 11, 16, 22, 30, 50, 86, 116, 143};
static unsigned char m_gray_016[] = {0, 7, 10, 16, 21, 30, 50, 86, 116, 143};
static unsigned char m_gray_017[] = {0, 7, 10, 16, 21, 30, 50, 87, 117, 143};
static unsigned char m_gray_019[] = {0, 7, 10, 16, 21, 30, 50, 86, 116, 143};
static unsigned char m_gray_020[] = {0, 7, 10, 16, 21, 30, 50, 87, 117, 143};
static unsigned char m_gray_021[] = {0, 7, 10, 16, 21, 30, 50, 87, 117, 143};
static unsigned char m_gray_022[] = {0, 7, 10, 16, 22, 30, 50, 87, 117, 143};
static unsigned char m_gray_024[] = {0, 7, 10, 16, 22, 31, 50, 87, 117, 143};
static unsigned char m_gray_025[] = {0, 7, 11, 17, 22, 31, 51, 87, 117, 143};
static unsigned char m_gray_027[] = {0, 7, 11, 17, 22, 31, 51, 88, 117, 143};
static unsigned char m_gray_029[] = {0, 7, 11, 17, 22, 31, 51, 88, 117, 143};
static unsigned char m_gray_030[] = {0, 7, 11, 17, 22, 31, 51, 88, 117, 143};
static unsigned char m_gray_032[] = {0, 7, 11, 17, 22, 31, 51, 88, 117, 143};
static unsigned char m_gray_034[] = {0, 7, 11, 17, 22, 31, 51, 88, 117, 143};
static unsigned char m_gray_037[] = {0, 7, 10, 17, 22, 31, 51, 88, 117, 143};
static unsigned char m_gray_039[] = {0, 7, 10, 16, 22, 31, 51, 88, 117, 143};
static unsigned char m_gray_041[] = {0, 7, 10, 16, 22, 31, 51, 88, 117, 143};
static unsigned char m_gray_044[] = {0, 6, 10, 16, 22, 31, 51, 88, 117, 143};
static unsigned char m_gray_047[] = {0, 6, 9, 16, 22, 31, 51, 88, 117, 143};
static unsigned char m_gray_050[] = {0, 6, 9, 16, 22, 31, 51, 88, 117, 143};
static unsigned char m_gray_053[] = {0, 6, 9, 15, 22, 31, 51, 88, 117, 143};
static unsigned char m_gray_056[] = {0, 6, 9, 15, 22, 31, 51, 88, 117, 143};
static unsigned char m_gray_060[] = {0, 5, 9, 15, 22, 31, 51, 88, 117, 143};
static unsigned char m_gray_064[] = {0, 5, 9, 15, 22, 31, 51, 88, 117, 143};
static unsigned char m_gray_068[] = {0, 5, 9, 15, 22, 31, 51, 88, 117, 143};
static unsigned char m_gray_072[] = {0, 5, 9, 15, 22, 31, 52, 89, 119, 146};
static unsigned char m_gray_077[] = {0, 5, 9, 16, 23, 32, 54, 93, 124, 151};
static unsigned char m_gray_082[] = {0, 5, 9, 16, 23, 33, 56, 95, 127, 156};
static unsigned char m_gray_087[] = {0, 5, 9, 16, 24, 34, 59, 98, 130, 160};
static unsigned char m_gray_093[] = {0, 5, 9, 17, 25, 35, 60, 101, 134, 165};
static unsigned char m_gray_098[] = {0, 5, 9, 17, 25, 36, 62, 104, 138, 170};
static unsigned char m_gray_105[] = {0, 5, 10, 18, 26, 38, 64, 107, 142, 176};
static unsigned char m_gray_111[] = {0, 5, 10, 18, 27, 39, 66, 110, 146, 180};
static unsigned char m_gray_119[] = {0, 5, 10, 19, 28, 40, 68, 113, 150, 186};
static unsigned char m_gray_126[] = {0, 5, 10, 19, 28, 40, 68, 113, 150, 186};
static unsigned char m_gray_134[] = {0, 5, 10, 19, 28, 40, 68, 113, 150, 186};
static unsigned char m_gray_143[] = {0, 4, 10, 19, 28, 40, 68, 113, 150, 186};
static unsigned char m_gray_152[] = {0, 4, 10, 19, 28, 39, 68, 113, 150, 186};
static unsigned char m_gray_162[] = {0, 4, 10, 19, 28, 39, 67, 113, 150, 186};
static unsigned char m_gray_172[] = {0, 4, 10, 19, 28, 39, 67, 113, 150, 186};
static unsigned char m_gray_183[] = {0, 4, 10, 19, 28, 39, 67, 113, 149, 186};
static unsigned char m_gray_195[] = {0, 4, 10, 19, 28, 40, 68, 117, 154, 192};
static unsigned char m_gray_207[] = {0, 4, 10, 20, 29, 41, 70, 120, 158, 197};
static unsigned char m_gray_220[] = {0, 4, 10, 20, 30, 43, 72, 123, 163, 203};
static unsigned char m_gray_234[] = {0, 4, 10, 21, 31, 44, 74, 126, 167, 208};
static unsigned char m_gray_249[] = {0, 4, 10, 21, 31, 45, 76, 130, 172, 215};
static unsigned char m_gray_265[] = {0, 4, 10, 22, 32, 46, 78, 134, 177, 221};
static unsigned char m_gray_282[] = {0, 4, 10, 21, 33, 47, 80, 137, 182, 227};
static unsigned char m_gray_300[] = {0, 3, 10, 22, 34, 49, 82, 140, 187, 234};
static unsigned char m_gray_316[] = {0, 3, 10, 23, 34, 50, 84, 143, 191, 240};
static unsigned char m_gray_333[] = {0, 3, 11, 23, 35, 51, 85, 147, 196, 246};
static unsigned char m_gray_360[] = {0, 3, 11, 23, 35, 51, 87, 151, 203, 255};

#endif
