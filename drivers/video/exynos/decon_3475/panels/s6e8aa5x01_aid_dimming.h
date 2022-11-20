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

static const signed char rtbl5nit[10] = {0, 5, 9, 7, 3, 3, 2, 2, 3, 0};
static const signed char rtbl6nit[10] = {0, 5, 9, 6, 3, 3, 2, 1, 2, 0};
static const signed char rtbl7nit[10] = {0, 5, 9, 6, 3, 3, 2, 2, 3, 0};
static const signed char rtbl8nit[10] = {0, 5, 8, 5, 3, 2, 1, 2, 3, 0};
static const signed char rtbl9nit[10] = {0, 5, 7, 5, 2, 2, 1, 2, 3, 0};
static const signed char rtbl10nit[10] = {0, 5, 7, 5, 2, 2, 1, 1, 2, 0};
static const signed char rtbl11nit[10] = {0, 5, 7, 4, 2, 1, 1, 1, 2, 0};
static const signed char rtbl12nit[10] = {0, 5, 6, 4, 2, 2, 1, 2, 3, 0};
static const signed char rtbl13nit[10] = {0, 5, 6, 4, 2, 1, 1, 1, 2, 0};
static const signed char rtbl14nit[10] = {0, 5, 5, 4, 2, 2, 1, 2, 3, 0};
static const signed char rtbl15nit[10] = {0, 5, 5, 3, 2, 1, 1, 1, 2, 0};
static const signed char rtbl16nit[10] = {0, 5, 4, 3, 1, 1, 1, 1, 2, 0};
static const signed char rtbl17nit[10] = {0, 5, 4, 3, 1, 1, 1, 2, 3, 0};
static const signed char rtbl19nit[10] = {0, 5, 4, 3, 1, 1, 1, 1, 2, 0};
static const signed char rtbl20nit[10] = {0, 5, 4, 3, 1, 1, 1, 2, 3, 0};
static const signed char rtbl21nit[10] = {0, 5, 4, 3, 1, 1, 1, 2, 3, 0};
static const signed char rtbl22nit[10] = {0, 5, 4, 3, 2, 1, 1, 2, 3, 0};
static const signed char rtbl24nit[10] = {0, 5, 4, 3, 2, 2, 1, 2, 3, 0};
static const signed char rtbl25nit[10] = {0, 5, 5, 4, 2, 2, 2, 2, 3, 0};
static const signed char rtbl27nit[10] = {0, 5, 5, 4, 2, 2, 2, 3, 3, 0};
static const signed char rtbl29nit[10] = {0, 5, 5, 4, 2, 2, 2, 3, 3, 0};
static const signed char rtbl30nit[10] = {0, 5, 5, 4, 2, 2, 2, 3, 3, 0};
static const signed char rtbl32nit[10] = {0, 5, 5, 4, 2, 2, 2, 3, 3, 0};
static const signed char rtbl34nit[10] = {0, 5, 5, 4, 2, 2, 2, 3, 3, 0};
static const signed char rtbl37nit[10] = {0, 5, 4, 4, 2, 2, 2, 3, 3, 0};
static const signed char rtbl39nit[10] = {0, 5, 4, 3, 2, 2, 2, 3, 3, 0};
static const signed char rtbl41nit[10] = {0, 5, 4, 3, 2, 2, 2, 3, 3, 0};
static const signed char rtbl44nit[10] = {0, 4, 4, 3, 2, 2, 2, 3, 3, 0};
static const signed char rtbl47nit[10] = {0, 4, 3, 3, 2, 2, 2, 3, 3, 0};
static const signed char rtbl50nit[10] = {0, 4, 3, 3, 2, 2, 2, 3, 3, 0};
static const signed char rtbl53nit[10] = {0, 4, 3, 2, 2, 2, 2, 3, 3, 0};
static const signed char rtbl56nit[10] = {0, 4, 3, 2, 2, 2, 2, 3, 3, 0};
static const signed char rtbl60nit[10] = {0, 3, 3, 2, 2, 2, 2, 3, 3, 0};
static const signed char rtbl64nit[10] = {0, 3, 3, 2, 2, 2, 2, 3, 3, 0};
static const signed char rtbl68nit[10] = {0, 3, 3, 2, 2, 2, 2, 3, 3, 0};
static const signed char rtbl72nit[10] = {0, 3, 3, 2, 2, 2, 2, 3, 3, 0};
static const signed char rtbl77nit[10] = {0, 3, 2, 2, 2, 2, 2, 3, 3, 0};
static const signed char rtbl82nit[10] = {0, 3, 2, 2, 2, 2, 3, 3, 3, 0};
static const signed char rtbl87nit[10] = {0, 3, 2, 2, 2, 2, 4, 3, 3, 0};
static const signed char rtbl93nit[10] = {0, 3, 2, 2, 2, 2, 4, 3, 3, 0};
static const signed char rtbl98nit[10] = {0, 3, 2, 2, 2, 2, 4, 3, 3, 0};
static const signed char rtbl105nit[10] = {0, 3, 2, 2, 2, 3, 4, 3, 2, 0};
static const signed char rtbl111nit[10] = {0, 3, 2, 2, 2, 3, 4, 3, 2, 0};
static const signed char rtbl119nit[10] = {0, 3, 2, 2, 2, 3, 4, 3, 2, 0};
static const signed char rtbl126nit[10] = {0, 3, 2, 2, 2, 3, 4, 3, 2, 0};
static const signed char rtbl134nit[10] = {0, 3, 2, 2, 2, 3, 4, 3, 2, 0};
static const signed char rtbl143nit[10] = {0, 2, 2, 2, 2, 3, 4, 3, 2, 0};
static const signed char rtbl152nit[10] = {0, 2, 2, 2, 2, 2, 4, 3, 2, 0};
static const signed char rtbl162nit[10] = {0, 2, 2, 2, 2, 2, 3, 3, 2, 0};
static const signed char rtbl172nit[10] = {0, 2, 2, 2, 2, 2, 3, 3, 2, 0};
static const signed char rtbl183nit[10] = {0, 2, 2, 2, 2, 2, 3, 3, 1, 0};
static const signed char rtbl195nit[10] = {0, 2, 2, 2, 2, 2, 3, 3, 1, 0};
static const signed char rtbl207nit[10] = {0, 2, 2, 2, 2, 2, 3, 3, 1, 0};
static const signed char rtbl220nit[10] = {0, 2, 1, 2, 2, 2, 3, 3, 1, 0};
static const signed char rtbl234nit[10] = {0, 2, 1, 2, 2, 2, 3, 3, 1, 0};
static const signed char rtbl249nit[10] = {0, 2, 1, 2, 2, 2, 3, 3, 1, 0};
static const signed char rtbl265nit[10] = {0, 1, 0, 2, 2, 2, 3, 3, 1, 0};
static const signed char rtbl282nit[10] = {0, 1, 0, 1, 2, 2, 2, 2, 1, 0};
static const signed char rtbl300nit[10] = {0, 0, 0, 1, 2, 2, 2, 1, 1, 0};
static const signed char rtbl316nit[10] = {0, 0, 0, 1, 1, 2, 2, 1, 0, 0};
static const signed char rtbl333nit[10] = {0, 0, 0, 1, 1, 2, 1, 1, 0, 0};
static const signed char rtbl360nit[10] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

static const signed char ctbl5nit[30] = {0, 0, 0, 0, 0, 0,0, 0, 1, -14, 0, -2, -6, 0, -3, -5, 0, 4, -3, 0, 0, -1, 0, 2, 0, 0, 1, 2, 0, 1};
static const signed char ctbl6nit[30] = {0, 0, 0, 0, 0, 0,-3, 0, 1, -13, 0, -4, -6, 0, -3, -4, 0, 4, -2, 0, 0, -1, 0, 1, 0, 0, 1, 2, 0, 1};
static const signed char ctbl7nit[30] = {0, 0, 0, 0, 0, 0,-6, 0, 0, -13, 0, -3, -5, 0, -3, -4, 0, 3, -3, 0, 0, 0, 0, 2, 0, 0, 1, 2, 0, 1};
static const signed char ctbl8nit[30] = {0, 0, 0, 0, 0, 0,-8, 0, 0, -12, 0, -4, -5, 0, -4, -4, 0, 3, -1, 0, 1, 0, 0, 2, 0, 0, 1, 2, 0, 1};
static const signed char ctbl9nit[30] = {0, 0, 0, 0, 0, 0,-10, 0, 1, -10, 0, -2, -5, 0, -5, -3, 0, 3, -1, 0, 1, 0, 0, 2, 0, 0, 1, 2, 0, 1};
static const signed char ctbl10nit[30] = {0, 0, 0, 0, 0, 0,-12, 0, -1, -8, 0, -2, -4, 0, -3, -4, 0, 2, 0, 0, 1, -1, 0, 1, 0, 0, 1, 2, 0, 1};
static const signed char ctbl11nit[30] = {0, 0, 0, 0, 0, 0,-13, 0, -1, -9, 0, -3, -4, 0, -3, -3, 0, 2, 0, 0, 1, -1, 0, 1, 0, 0, 1, 2, 0, 1};
static const signed char ctbl12nit[30] = {0, 0, 0, 0, 0, 0,-13, 0, -1, -9, 0, -2, -3, 0, -5, -2, 0, 3, -1, 0, 1, -1, 0, 1, 0, 0, 1, 2, 0, 1};
static const signed char ctbl13nit[30] = {0, 0, 0, 0, 0, 0,-11, 0, -1, -7, 0, -1, -3, 0, -5, -3, 0, 2, -1, 0, 1, 0, 0, 1, 0, 0, 1, 2, 0, 1};
static const signed char ctbl14nit[30] = {0, 0, 0, 0, 0, 0,-11, 0, -1, -5, 0, -2, -2, 0, -5, -2, 0, 3, -2, 0, 0, 0, 0, 2, 0, 0, 1, 2, 0, 1};
static const signed char ctbl15nit[30] = {0, 0, 0, 0, 0, 0,-11, 0, -2, -7, 0, -3, -1, 0, -4, -2, 0, 3, -2, 0, 0, 0, 0, 1, 0, 0, 1, 2, 0, 1};
static const signed char ctbl16nit[30] = {0, 0, 0, 0, 0, 0,-13, 0, -2, -5, 0, -3, -1, 0, -4, -1, 0, 3, -2, 0, 0, 0, 0, 1, 0, 0, 1, 2, 0, 1};
static const signed char ctbl17nit[30] = {0, 0, 0, 0, 0, 0,-11, 0, 0, -5, 0, -3, -1, 0, -4, -2, 0, 2, -2, 0, 0, 0, 0, 2, 0, 0, 1, 2, 0, 1};
static const signed char ctbl19nit[30] = {0, 0, 0, 0, 0, 0,-9, 0, 1, -5, 0, -4, -1, 0, -4, -2, 0, 2, 0, 0, 1, 0, 0, 2, -1, 0, 0, 2, 0, 1};
static const signed char ctbl20nit[30] = {0, 0, 0, 0, 0, 0,-8, 0, 2, -5, 0, -3, -1, 0, -4, -2, 0, 2, 0, 0, 1, 0, 0, 2, -1, 0, 0, 2, 0, 1};
static const signed char ctbl21nit[30] = {0, 0, 0, 0, 0, 0,-7, 0, 0, -6, 0, -5, 0, 0, -4, -2, 0, 2, 0, 0, 1, 0, 0, 2, -1, 0, 0, 2, 0, 1};
static const signed char ctbl22nit[30] = {0, 0, 0, 0, 0, 0,-7, 1, -3, -6, 0, -4, 0, 0, -4, -2, 0, 2, 0, 0, 1, 0, 0, 2, -1, 0, 0, 2, 0, 1};
static const signed char ctbl24nit[30] = {0, 0, 0, 0, 0, 0,-7, 1, -3, -6, 0, -6, 0, 0, -4, -2, 0, 2, 0, 0, 1, 0, 0, 2, -1, 0, 0, 2, 0, 1};
static const signed char ctbl25nit[30] = {0, 0, 0, 0, 0, 0,-9, 0, -4, -5, 0, -5, 0, 0, -5, -3, 0, 1, 0, 0, 1, 0, 0, 2, -1, 0, 0, 2, 0, 1};
static const signed char ctbl27nit[30] = {0, 0, 0, 0, 0, 0,-9, 0, -5, -3, 0, -5, 0, 0, -5, -2, 0, 1, -1, 0, 0, 0, 0, 2, -1, 0, 1, 2, 0, 1};
static const signed char ctbl29nit[30] = {0, 0, 0, 0, 0, 0,-9, 0, -5, -3, 0, -5, 0, 0, -5, -2, 0, 1, -1, 0, 0, 0, 0, 2, -1, 0, 1, 2, 0, 1};
static const signed char ctbl30nit[30] = {0, 0, 0, 0, 0, 0,-9, 0, -7, -4, 0, -5, 0, 0, -6, -2, 0, 1, -1, 0, 0, 0, 0, 2, -1, 0, 1, 2, 0, 1};
static const signed char ctbl32nit[30] = {0, 0, 0, 0, 0, 0,-9, 0, -6, -3, 0, -5, 1, 0, -6, -2, 0, 1, -1, 0, 0, 0, 0, 2, -1, 0, 1, 2, 0, 0};
static const signed char ctbl34nit[30] = {0, 0, 0, 0, 0, 0,-9, 0, -6, -3, 0, -4, 1, 0, -6, -1, 0, 1, -1, 0, 0, 0, 0, 2, -1, 0, 1, 2, 0, 0};
static const signed char ctbl37nit[30] = {0, 0, 0, 0, 0, 0,-9, 0, -7, -2, 0, -4, 1, 0, -6, -1, 0, 1, -1, 0, 0, 0, 0, 2, -1, 0, 1, 2, 0, 0};
static const signed char ctbl39nit[30] = {0, 0, 0, 0, 0, 0,-8, 0, -4, -2, 0, -5, 1, 0, -6, -1, 0, 1, -1, 0, 0, 0, 0, 2, -1, 0, 1, 2, 0, 0};
static const signed char ctbl41nit[30] = {0, 0, 0, 0, 0, 0,-8, 0, -3, -1, 0, -4, 2, 0, -6, -1, 0, 1, -1, 0, 0, -1, 0, 2, -1, 0, 1, 2, 0, 0};
static const signed char ctbl44nit[30] = {0, 0, 0, 0, 0, 0,-7, 0, -2, -1, 0, -4, 2, 0, -6, -1, 0, 1, -1, 0, 0, -1, 0, 2, -1, 0, 1, 2, 0, 0};
static const signed char ctbl47nit[30] = {0, 0, 0, 0, 0, 0,-6, 0, -3, -1, 0, -4, 2, 0, -6, -1, 0, 1, -1, 0, 0, -1, 0, 2, -1, 0, 1, 2, 0, 0};
static const signed char ctbl50nit[30] = {0, 0, 0, 0, 0, 0,-5, 0, -1, -2, 0, -4, 3, 0, -5, -1, 0, 1, -1, 0, 0, -1, 0, 2, -1, 0, 1, 2, 0, 0};
static const signed char ctbl53nit[30] = {0, 0, 0, 0, 0, 0,-5, 0, 0, -1, 0, -4, 3, 0, -5, -1, 0, 1, -1, 0, 0, -1, 0, 2, -1, 0, 1, 2, 0, 0};
static const signed char ctbl56nit[30] = {0, 0, 0, 0, 0, 0,-5, 0, 1, -1, 0, -4, 3, 0, -5, -1, 0, 1, -1, 0, 0, -1, 0, 2, -1, 0, 1, 2, 0, 0};
static const signed char ctbl60nit[30] = {0, 0, 0, 0, 0, 0,-5, 0, 3, 0, 0, -4, 3, 0, -5, -1, 0, 1, -1, 0, 0, -1, 0, 2, -1, 0, 1, 2, 0, 0};
static const signed char ctbl64nit[30] = {0, 0, 0, 0, 0, 0,-5, 0, 4, 0, 0, -4, 3, 0, -5, -1, 0, 1, -1, 0, 0, -1, 0, 2, -1, 0, 1, 2, 0, 0};
static const signed char ctbl68nit[30] = {0, 0, 0, 0, 0, 0,-6, 0, 0, 0, 0, -4, 3, 0, -5, 0, 0, 2, -1, 0, 0, -1, 0, 1, -1, 0, 0, 1, 0, 0};
static const signed char ctbl72nit[30] = {0, 0, 0, 0, 0, 0,-5, 0, 0, 0, 0, -4, 2, 0, -4, 0, 0, 2, -1, 0, 0, -1, 0, 1, -1, 0, 0, 1, 0, 0};
static const signed char ctbl77nit[30] = {0, 0, 0, 0, 0, 0,-3, 0, 0, 0, 0, -3, 1, 0, -5, 0, 0, 2, -1, 0, 0, -1, 0, 1, -1, 0, 0, 1, 0, 0};
static const signed char ctbl82nit[30] = {0, 0, 0, 0, 0, 0,-5, 0, 0, 0, 0, -3, 1, 0, -4, 0, 0, 2, -1, 0, 0, -1, 0, 1, -1, 0, 0, 1, 0, 0};
static const signed char ctbl87nit[30] = {0, 0, 0, 0, 0, 0,-3, 0, 1, 0, 0, -4, 1, 0, -4, 0, 0, 3, -1, 0, -1, -1, 0, 1, -1, 0, 0, 1, 0, 0};
static const signed char ctbl93nit[30] = {0, 0, 0, 0, 0, 0,-3, 0, 0, 0, 0, -4, 3, 0, -4, -1, 0, 2, -1, 0, -1, -1, 0, 1, -1, 0, 0, 1, 0, 0};
static const signed char ctbl98nit[30] = {0, 0, 0, 0, 0, 0,-3, 0, 0, 0, 0, -4, 3, 0, -3, 0, 0, 2, -1, 0, -1, -1, 0, 1, -1, 0, 0, 1, 0, 0};
static const signed char ctbl105nit[30] = {0, 0, 0, 0, 0, 0,-4, 0, 0, 0, 0, -4, 1, 0, -4, 0, 0, 2, -1, 0, -1, 0, 0, 1, -1, 0, 0, 1, 0, 0};
static const signed char ctbl111nit[30] = {0, 0, 0, 0, 0, 0,-3, 0, -1, 0, 0, -4, 1, 0, -3, 0, 0, 2, -1, 0, -1, 0, 0, 1, 0, 0, 0, 1, 0, 0};
static const signed char ctbl119nit[30] = {0, 0, 0, 0, 0, 0,-2, 0, -1, 1, 0, -1, 0, 0, -3, 0, 0, 0, -1, 0, 0, -1, 0, 1, 1, 0, -1, 0, 0, 0};
static const signed char ctbl126nit[30] = {0, 0, 0, 0, 0, 0,-1, 0, 0, 1, 0, -1, 1, 0, -3, -1, 0, 1, -1, 0, 0, -1, 0, 0, 1, 0, -1, 0, 0, 0};
static const signed char ctbl134nit[30] = {0, 0, 0, 0, 0, 0,-1, 0, 0, 1, 0, -1, 1, 0, -3, -1, 0, 1, -1, 0, 0, -1, 0, 0, 1, 0, -1, 0, 0, 0};
static const signed char ctbl143nit[30] = {0, 0, 0, 0, 0, 0,0, 0, 1, 1, 0, -1, 1, 0, -3, -1, 0, 1, -1, 0, 0, -1, 0, 0, 1, 0, -1, 0, 0, 0};
static const signed char ctbl152nit[30] = {0, 0, 0, 0, 0, 0,1, 0, 1, 1, 0, -1, 1, 0, -3, -1, 0, 1, -1, 0, 0, -1, 0, 0, 1, 0, -1, 0, 0, 0};
static const signed char ctbl162nit[30] = {0, 0, 0, 0, 0, 0,0, 0, 1, 0, 0, -2, 1, 0, -3, -1, 0, 1, 0, 0, 1, -1, 0, 0, 1, 0, -1, 0, 0, 0};
static const signed char ctbl172nit[30] = {0, 0, 0, 0, 0, 0,0, 0, 1, 0, 0, -2, 1, 0, -3, -1, 0, 1, 0, 0, 1, -1, 0, 0, 1, 0, -1, 0, 0, 0};
static const signed char ctbl183nit[30] = {0, 0, 0, 0, 0, 0,0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static const signed char ctbl195nit[30] = {0, 0, 0, 0, 0, 0,0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static const signed char ctbl207nit[30] = {0, 0, 0, 0, 0, 0,0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static const signed char ctbl220nit[30] = {0, 0, 0, 0, 0, 0,0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static const signed char ctbl234nit[30] = {0, 0, 0, 0, 0, 0,0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static const signed char ctbl249nit[30] = {0, 0, 0, 0, 0, 0,0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static const signed char ctbl265nit[30] = {0, 0, 0, 0, 0, 0,0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static const signed char ctbl282nit[30] = {0, 0, 0, 0, 0, 0,0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static const signed char ctbl300nit[30] = {0, 0, 0, 0, 0, 0,0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static const signed char ctbl316nit[30] = {0, 0, 0, 0, 0, 0,0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static const signed char ctbl333nit[30] = {0, 0, 0, 0, 0, 0,0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static const signed char ctbl360nit[30] = {0, 0, 0, 0, 0, 0,0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

static const signed char aid5[5] = {0xB2, 0x04, 0xEE, 0x00, 0x0F};
static const signed char aid6[5] = {0xB2, 0x04, 0xE1, 0x00, 0x0F};
static const signed char aid7[5] = {0xB2, 0x04, 0xD4, 0x00, 0x0F};
static const signed char aid8[5] = {0xB2, 0x04, 0xCF, 0x00, 0x0F};
static const signed char aid9[5] = {0xB2, 0x04, 0xC2, 0x00, 0x0F};
static const signed char aid10[5] = {0xB2, 0x04, 0xB4, 0x00, 0x0F};
static const signed char aid11[5] = {0xB2, 0x04, 0xAF, 0x00, 0x0F};
static const signed char aid12[5] = {0xB2, 0x04, 0xA3, 0x00, 0x0F};
static const signed char aid13[5] = {0xB2, 0x04, 0x96, 0x00, 0x0F};
static const signed char aid14[5] = {0xB2, 0x04, 0x91, 0x00, 0x0F};
static const signed char aid15[5] = {0xB2, 0x04, 0x82, 0x00, 0x0F};
static const signed char aid16[5] = {0xB2, 0x04, 0x75, 0x00, 0x0F};
static const signed char aid17[5] = {0xB2, 0x04, 0x70, 0x00, 0x0F};
static const signed char aid19[5] = {0xB2, 0x04, 0x54, 0x00, 0x0F};
static const signed char aid20[5] = {0xB2, 0x04, 0x50, 0x00, 0x0F};
static const signed char aid21[5] = {0xB2, 0x04, 0x42, 0x00, 0x0F};
static const signed char aid22[5] = {0xB2, 0x04, 0x33, 0x00, 0x0F};
static const signed char aid24[5] = {0xB2, 0x04, 0x1E, 0x00, 0x0F};
static const signed char aid25[5] = {0xB2, 0x04, 0x10, 0x00, 0x0F};
static const signed char aid27[5] = {0xB2, 0x03, 0xF2, 0x00, 0x0F};
static const signed char aid29[5] = {0xB2, 0x03, 0xDE, 0x00, 0x0F};
static const signed char aid30[5] = {0xB2, 0x03, 0xC6, 0x00, 0x0F};
static const signed char aid32[5] = {0xB2, 0x03, 0xB1, 0x00, 0x0F};
static const signed char aid34[5] = {0xB2, 0x03, 0x95, 0x00, 0x0F};
static const signed char aid37[5] = {0xB2, 0x03, 0x73, 0x00, 0x0F};
static const signed char aid39[5] = {0xB2, 0x03, 0x5F, 0x00, 0x0F};
static const signed char aid41[5] = {0xB2, 0x03, 0x43, 0x00, 0x0F};
static const signed char aid44[5] = {0xB2, 0x03, 0x20, 0x00, 0x0F};
static const signed char aid47[5] = {0xB2, 0x02, 0xFE, 0x00, 0x0F};
static const signed char aid50[5] = {0xB2, 0x02, 0xDF, 0x00, 0x0F};
static const signed char aid53[5] = {0xB2, 0x02, 0xB2, 0x00, 0x0F};
static const signed char aid56[5] = {0xB2, 0x02, 0x90, 0x00, 0x0F};
static const signed char aid60[5] = {0xB2, 0x02, 0x5E, 0x00, 0x0F};
static const signed char aid64[5] = {0xB2, 0x02, 0x30, 0x00, 0x0F};
static const signed char aid68[5] = {0xB2, 0x01, 0xF4, 0x00, 0x0F};
static const signed char aid72[5] = {0xB2, 0x01, 0xF4, 0x00, 0x0F};
static const signed char aid77[5] = {0xB2, 0x01, 0xF4, 0x00, 0x0F};
static const signed char aid82[5] = {0xB2, 0x01, 0xF4, 0x00, 0x0F};
static const signed char aid87[5] = {0xB2, 0x01, 0xF4, 0x00, 0x0F};
static const signed char aid93[5] = {0xB2, 0x01, 0xF4, 0x00, 0x0F};
static const signed char aid98[5] = {0xB2, 0x01, 0xF4, 0x00, 0x0F};
static const signed char aid105[5] = {0xB2, 0x01, 0xF4, 0x00, 0x0F};
static const signed char aid110[5] = {0xB2, 0x01, 0xF4, 0x00, 0x0F};
static const signed char aid119[5] = {0xB2, 0x01, 0xF4, 0x00, 0x0F};
static const signed char aid126[5] = {0xB2, 0x01, 0xBF, 0x00, 0x0F};
static const signed char aid134[5] = {0xB2, 0x01, 0x82, 0x00, 0x0F};
static const signed char aid143[5] = {0xB2, 0x01, 0x40, 0x00, 0x0F};
static const signed char aid152[5] = {0xB2, 0x01, 0x00, 0x00, 0x0F};
static const signed char aid162[5] = {0xB2, 0x00, 0xB2, 0x00, 0x0F};
static const signed char aid172[5] = {0xB2, 0x00, 0x62, 0x00, 0x0F};
static const signed char aid183[5] = {0xB2, 0x00, 0x0F, 0x00, 0x0F};
static const signed char aid195[5] = {0xB2, 0x00, 0x0F, 0x00, 0x0F};
static const signed char aid207[5] = {0xB2, 0x00, 0x0F, 0x00, 0x0F};
static const signed char aid220[5] = {0xB2, 0x00, 0x0F, 0x00, 0x0F};
static const signed char aid234[5] = {0xB2, 0x00, 0x0F, 0x00, 0x0F};
static const signed char aid249[5] = {0xB2, 0x00, 0x0F, 0x00, 0x0F};
static const signed char aid265[5] = {0xB2, 0x00, 0x0F, 0x00, 0x0F};
static const signed char aid282[5] = {0xB2, 0x00, 0x0F, 0x00, 0x0F};
static const signed char aid300[5] = {0xB2, 0x00, 0x0F, 0x00, 0x0F};
static const signed char aid316[5] = {0xB2, 0x00, 0x0F, 0x00, 0x0F};
static const signed char aid333[5] = {0xB2, 0x00, 0x0F, 0x00, 0x0F};
static const signed char aid360[5] = {0xB2, 0x00, 0x0F, 0x00, 0x0F};

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

#endif
