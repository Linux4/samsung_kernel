/* linux/drivers/video/exynos/decon_7580/panels/s6e3fa3_aid_dimming.h
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

#ifndef __S6E3FA3_WQHD_AID_DIMMING_H__
#define __S6E3FA3_WQHD_AID_DIMMING_H__

static signed char rtbl2nit[10] = {0, 43, 42, 38, 34, 30, 21, 13, 3, 0};
static signed char rtbl3nit[10] = {0, 38, 35, 31, 26, 24, 17, 11, 7, 0};
static signed char rtbl4nit[10] = {0, 34, 31, 28, 23, 19, 15, 10, 6, 0};
static signed char rtbl5nit[10] = {0, 30, 27, 24, 20, 16, 12, 9, 6, 0};
static signed char rtbl6nit[10] = {0, 29, 25, 21, 17, 15, 11, 8, 6, 0};
static signed char rtbl7nit[10] = {0, 27, 23, 19, 15, 14, 10, 7, 5, 0};
static signed char rtbl8nit[10] = {0, 25, 21, 18, 13, 12, 9, 7, 5, 0};
static signed char rtbl9nit[10] = {0, 23, 20, 17, 13, 12, 8, 6, 5, 0};
static signed char rtbl10nit[10] = {0, 20, 19, 15, 12, 10, 8, 6, 4, 0};
static signed char rtbl11nit[10] = {0, 20, 17, 14, 10, 9, 6, 5, 4, 0};
static signed char rtbl12nit[10] = {0, 20, 16, 13, 10, 8, 6, 5, 3, 0};
static signed char rtbl13nit[10] = {0, 19, 15, 12, 9, 7, 6, 5, 3, 0};
static signed char rtbl14nit[10] = {0, 19, 15, 12, 10, 8, 5, 4, 4, 0};
static signed char rtbl15nit[10] = {0, 18, 14, 11, 8, 7, 4, 4, 2, 0};
static signed char rtbl16nit[10] = {0, 17, 13, 10, 8, 6, 4, 3, 2, 0};
static signed char rtbl17nit[10] = {0, 17, 13, 10, 7, 6, 6, 4, 3, 0};
static signed char rtbl19nit[10] = {0, 16, 12, 9, 6, 6, 4, 8, 3, 0};
static signed char rtbl20nit[10] = {0, 15, 12, 9, 6, 6, 3, 4, 2, 0};
static signed char rtbl21nit[10] = {0, 13, 12, 8, 6, 6, 4, 4, 3, 0};
static signed char rtbl22nit[10] = {0, 15, 11, 8, 6, 6, 3, 5, 3, 0};
static signed char rtbl24nit[10] = {0, 15, 11, 8, 6, 6, 4, 4, 3, 0};
static signed char rtbl25nit[10] = {0, 13, 10, 7, 5, 4, 3, 3, 3, 0};
static signed char rtbl27nit[10] = {0, 9, 10, 7, 4, 3, 2, 2, 2, 0};
static signed char rtbl29nit[10] = {0, 12, 9, 7, 4, 4, 4, 4, 3, 0};
static signed char rtbl30nit[10] = {0, 12, 9, 6, 4, 4, 3, 4, 3, 0};
static signed char rtbl32nit[10] = {0, 11, 9, 6, 4, 4, 3, 4, 3, 0};
static signed char rtbl34nit[10] = {0, 11, 8, 5, 3, 3, 2, 3, 3, 0};
static signed char rtbl37nit[10] = {0, 10, 7, 5, 3, 3, 3, 3, 3, 0};
static signed char rtbl39nit[10] = {0, 10, 7, 5, 3, 3, 3, 4, 3, 0};
static signed char rtbl41nit[10] = {0, 10, 7, 5, 3, 3, 2, 3, 3, 0};
static signed char rtbl44nit[10] = {0, 9, 6, 5, 2, 3, 2, 4, 3, 0};
static signed char rtbl47nit[10] = {0, 9, 6, 4, 2, 3, 2, 3, 3, 0};
static signed char rtbl50nit[10] = {0, 8, 5, 4, 2, 3, 2, 3, 3, 0};
static signed char rtbl53nit[10] = {0, 8, 5, 4, 2, 3, 2, 3, 3, 0};
static signed char rtbl56nit[10] = {0, 8, 5, 4, 2, 2, 2, 3, 3, 0};
static signed char rtbl60nit[10] = {0, 8, 4, 3, 1, 2, 1, 2, 2, 0};
static signed char rtbl64nit[10] = {0, 4, 5, 4, 2, 3, 2, 5, 5, 4};
static signed char rtbl68nit[10] = {0, 6, 4, 2, 1, 1, 2, 3, 3, 0};
static signed char rtbl72nit[10] = {0, 6, 4, 2, 2, 2, 1, 2, 3, 0};
static signed char rtbl77nit[10] = {0, 7, 4, 3, 1, 2, 1, 2, 2, 0};
static signed char rtbl82nit[10] = {0, 3, 3, 2, 1, 1, 1, 2, 2, 0};
static signed char rtbl87nit[10] = {0, 5, 3, 2, 1, 1, 1, 4, 2, 0};
static signed char rtbl93nit[10] = {0, 2, 4, 2, 1, 2, 3, 3, 2, 0};
static signed char rtbl98nit[10] = {0, 5, 3, 2, 0, 2, 1, 3, 1, 0};
static signed char rtbl105nit[10] = {0, 8, 2, 1, 1, 1, 2, 3, 1, 0};
static signed char rtbl111nit[10] = {0, 8, 2, 2, 1, 2, 2, 4, 2, 0};
static signed char rtbl119nit[10] = {0, 3, 3, 2, 1, 2, 2, 4, 2, 0};
static signed char rtbl126nit[10] = {0, 4, 3, 1, 1, 2, 3, 3, 2, 0};
static signed char rtbl134nit[10] = {0, 3, 3, 2, 1, 1, 2, 4, 2, 0};
static signed char rtbl143nit[10] = {0, 4, 2, 1, 1, 2, 4, 3, 1, 0};
static signed char rtbl152nit[10] = {0, 2, 2, 1, 0, 1, 2, 3, 1, 0};
static signed char rtbl162nit[10] = {0, 5, 1, 0, 1, 1, 2, 3, 2, 0};
static signed char rtbl172nit[10] = {0, 4, 1, 0, 1, 0, 2, 3, 2, 0};
static signed char rtbl183nit[10] = {0, 4, 1, 0, 0, 0, 2, 2, 1, 0};
static signed char rtbl195nit[10] = {0, 0, 1, 0, 0, 0, 2, 2, 1, 0};
static signed char rtbl207nit[10] = {0, 4, 0, 0, 0, 0, 1, 2, 1, 0};
static signed char rtbl220nit[10] = {0, 2, 0, 0, 0, 0, 1, 2, 1, 0};
static signed char rtbl234nit[10] = {0, 1, 0, 0, 0, 0, 1, 2, 0, 0};
static signed char rtbl249nit[10] = {0, 0, 0, 0, 0, 0, 1, 2, 0, 1};
static signed char rtbl265nit[10] = {0, 0, 0, 0, 0, 0, 0, 1, 1, 1};
static signed char rtbl282nit[10] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 1};
static signed char rtbl300nit[10] = {0, 0, 0, 0, 0, 0, 1, 1, 1, 1};
static signed char rtbl316nit[10] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 1};
static signed char rtbl333nit[10] = {0, 0, 0, 0, 0, 0, 0, 1, 0, 1};
static signed char rtbl360nit[10] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

static signed char ctbl2nit[30] = {0, 0, 0, 0, 0, 0,-1, 2, -2, -2, 4, -3, -1, 4, -3, -7, 3, -12, -4, 3, -9, -2, 1, -4, -2, 1, -3, -3, 1, -1};
static signed char ctbl3nit[30] = {0, 0, 0, 0, 0, 0,-4, 4, -4, -2, 4, -4, -4, 4, -9, -5, 4, -8, -3, 3, -7, -1, 1, -2, -2, 1, -2, -3, 0, -1};
static signed char ctbl4nit[30] = {0, 0, 0, 0, 0, 0,-3, 3, -6, -3, 3, -6, -3, 3, -6, -6, 3, -11, -3, 3, -5, -1, 1, -2, -2, 0, -3, -2, 0, -1};
static signed char ctbl5nit[30] = {0, 0, 0, 0, 0, 0,-3, 3, -7, -3, 3, -7, -3, 3, -6, -6, 3, -10, -4, 3, -5, -1, 1, -2, -2, 0, -3, -1, 0, 0};
static signed char ctbl6nit[30] = {0, 0, 0, 0, 0, 0,-4, 4, -5, -4, 3, -8, -4, 3, -7, -5, 3, -10, -4, 2, -4, -1, 0, -2, -2, 0, -3, -1, 0, 0};
static signed char ctbl7nit[30] = {0, 0, 0, 0, 0, 0,-3, 3, -6, -4, 3, -9, -3, 3, -7, -7, 3, -10, -4, 2, -4, -1, 0, -2, -1, 0, -2, -1, 0, 0};
static signed char ctbl8nit[30] = {0, 0, 0, 0, 0, 0,-4, 3, -8, -2, 3, -6, -5, 3, -7, -6, 3, -10, -3, 2, -3, 0, 0, -1, -1, 0, -2, -1, 0, 0};
static signed char ctbl9nit[30] = {0, 0, 0, 0, 0, 0,-4, 3, -9, -3, 3, -6, -5, 3, -7, -5, 3, -7, -4, 1, -4, 0, 0, -1, -1, 0, -2, -1, 0, 0};
static signed char ctbl10nit[30] = {0, 0, 0, 0, 0, 0,-3, 3, -6, -5, 3, -10, -3, 3, -6, -6, 3, -8, -3, 1, -3, 0, 0, -1, -2, 0, -1, -1, 0, 0};
static signed char ctbl11nit[30] = {0, 0, 0, 0, 0, 0,-4, 3, -10, -3, 3, -6, -4, 3, -6, -6, 3, -8, -3, 2, -3, 0, 0, 0, -1, 0, -1, -1, 0, 0};
static signed char ctbl12nit[30] = {0, 0, 0, 0, 0, 0,-3, 3, -10, -5, 3, -9, -3, 3, -6, -5, 3, -6, -2, 1, -2, -1, 0, -2, -2, 0, -2, 0, 0, 1};
static signed char ctbl13nit[30] = {0, 0, 0, 0, 0, 0,-2, 4, -10, -4, 4, -6, -3, 3, -6, -5, 3, -7, -3, 1, -2, 0, 0, -1, -1, 0, -1, -1, 0, 0};
static signed char ctbl14nit[30] = {0, 0, 0, 0, 0, 0,-4, 4, -10, -4, 4, -8, -3, 2, -6, -7, 2, -7, -2, 1, -2, -1, 0, -2, -2, 0, -1, 0, 0, 1};
static signed char ctbl15nit[30] = {0, 0, 0, 0, 0, 0,-4, 4, -11, -5, 3, -7, -4, 2, -7, -7, 2, -7, -2, 1, -3, 0, 0, 0, -1, 0, -2, 0, 0, 1};
static signed char ctbl16nit[30] = {0, 0, 0, 0, 0, 0,-4, 4, -12, -4, 4, -8, -3, 3, -5, -5, 3, -6, -3, 2, -2, 0, 0, -1, -1, 0, -2, 0, 0, 1};
static signed char ctbl17nit[30] = {0, 0, 0, 0, 0, 0,-4, 4, -11, -5, 3, -8, -3, 3, -4, -5, 3, -5, -1, 1, -2, -1, 0, -1, -2, 0, -2, 0, 0, 1};
static signed char ctbl19nit[30] = {0, 0, 0, 0, 0, 0,-4, 3, -13, -4, 3, -7, -4, 3, -6, -4, 3, -5, -3, 1, -2, 0, 0, -1, -1, 0, -1, 0, 0, 1};
static signed char ctbl20nit[30] = {0, 0, 0, 0, 0, 0,-3, 3, -13, -5, 2, -8, -4, 2, -6, -5, 2, -5, -2, 0, -2, 0, 0, -1, 0, 0, -1, 0, 0, 1};
static signed char ctbl21nit[30] = {0, 0, 0, 0, 0, 0,-3, 3, -9, -4, 4, -7, -4, 2, -6, -5, 2, -5, -2, 1, -2, -1, 0, -2, -1, 0, -1, 0, 0, 1};
static signed char ctbl22nit[30] = {0, 0, 0, 0, 0, 0,-1, 6, -9, -4, 3, -8, -4, 2, -5, -4, 2, -5, -2, 0, -1, 0, 0, -1, -1, 0, -1, 0, 0, 1};
static signed char ctbl24nit[30] = {0, 0, 0, 0, 0, 0,-4, 3, -13, -4, 3, -7, -3, 2, -4, -5, 2, -5, -2, 0, -2, -2, 0, -2, 0, 0, -1, 0, 0, 1};
static signed char ctbl25nit[30] = {0, 0, 0, 0, 0, 0,-3, 3, -12, -4, 4, -8, -4, 2, -3, -2, 2, -3, -1, 1, -1, -1, 0, -1, -1, 0, -1, 0, 0, 1};
static signed char ctbl27nit[30] = {0, 0, 0, 0, 0, 0,-3, 3, -10, -4, 3, -6, -4, 1, -5, -4, 2, -4, -1, 0, -1, 0, 0, -2, -1, 0, -1, 0, 0, 1};
static signed char ctbl29nit[30] = {0, 0, 0, 0, 0, 0,-3, 6, -12, -4, 2, -7, -4, 2, -4, -4, 2, -4, -2, 0, -2, 0, 0, -1, -1, 0, -1, 0, 0, 1};
static signed char ctbl30nit[30] = {0, 0, 0, 0, 0, 0,-3, 3, -13, -6, 3, -8, -4, 2, -4, -3, 1, -3, 0, 0, 0, -1, 0, -2, -1, 0, -1, 0, 0, 1};
static signed char ctbl32nit[30] = {0, 0, 0, 0, 0, 0,-4, 3, -12, -4, 3, -7, -5, 1, -4, -2, 1, -3, -1, 0, -1, -1, 0, -1, -1, 0, -1, 0, 0, 1};
static signed char ctbl34nit[30] = {0, 0, 0, 0, 0, 0,-4, 3, -13, -5, 3, -7, -2, 2, -2, -3, 2, -4, -1, 0, -1, -1, 0, -1, -1, 0, -1, 0, 0, 1};
static signed char ctbl37nit[30] = {0, 0, 0, 0, 0, 0,-4, 5, -13, -4, 3, -7, -3, 1, -2, -3, 1, -4, 0, 0, 0, -1, 0, -1, -1, 0, -1, 0, 0, 1};
static signed char ctbl39nit[30] = {0, 0, 0, 0, 0, 0,-5, 4, -14, -5, 2, -7, -2, 1, -3, -3, 1, -3, -1, 0, 0, 0, 0, -1, -1, 0, -1, 0, 0, 1};
static signed char ctbl41nit[30] = {0, 0, 0, 0, 0, 0,-5, 4, -13, -5, 2, -6, -3, 1, -2, -2, 1, -3, -1, 0, -1, 0, 0, -1, -1, 0, 0, 0, 0, 1};
static signed char ctbl44nit[30] = {0, 0, 0, 0, 0, 0,-5, 5, -14, -4, 2, -5, -3, 2, -2, -3, 2, -3, 0, 0, 0, 0, 0, -1, -1, 0, -1, 0, 0, 1};
static signed char ctbl47nit[30] = {0, 0, 0, 0, 0, 0,-4, 4, -11, -5, 3, -6, -4, 2, -2, -2, 0, -3, -1, 0, -1, 0, 0, -1, -1, 0, -1, 0, 0, 1};
static signed char ctbl50nit[30] = {0, 0, 0, 0, 0, 0,-6, 5, -15, -4, 3, -4, -3, 1, -2, -2, 1, -3, -1, 0, 0, 0, 0, -1, -1, 0, 0, 0, 0, 0};
static signed char ctbl53nit[30] = {0, 0, 0, 0, 0, 0,-5, 6, -13, -5, 2, -5, -2, 1, -2, -3, 0, -3, -1, 0, 0, 0, 0, -1, -1, 0, 0, 0, 0, 0};
static signed char ctbl56nit[30] = {0, 0, 0, 0, 0, 0,-7, 4, -14, -4, 2, -5, -3, 0, -2, -2, 1, -3, -1, 0, 0, 0, 0, -1, -1, 0, 0, 0, 0, 0};
static signed char ctbl60nit[30] = {0, 0, 0, 0, 0, 0,-7, 5, -13, -3, 2, -4, -1, 1, -1, -1, 1, -1, -1, 0, 0, 0, 0, -1, 0, 0, 0, 0, 0, 0};
static signed char ctbl64nit[30] = {0, 0, 0, 0, 0, 0,-6, 3, -12, -4, 1, -4, -2, 1, -1, -3, 0, -2, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0};
static signed char ctbl68nit[30] = {0, 0, 0, 0, 0, 0,-6, 4, -12, -4, 1, -3, -2, 0, -2, -2, 1, -2, 0, 0, 0, 0, 0, -1, -1, 0, -1, 0, 0, 0};
static signed char ctbl72nit[30] = {0, 0, 0, 0, 0, 0,-7, 4, -11, -4, 1, -4, -2, 0, -2, -2, 0, -2, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static signed char ctbl77nit[30] = {0, 0, 0, 0, 0, 0,-7, 4, -13, -3, 2, -4, -1, 0, 0, -2, 0, -3, -1, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0};
static signed char ctbl82nit[30] = {0, 0, 0, 0, 0, 0,-5, 5, -9, -3, 2, -3, -1, 0, -1, -2, 1, -2, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 0, 0};
static signed char ctbl87nit[30] = {0, 0, 0, 0, 0, 0,-6, 4, -10, -3, 2, -3, -1, 1, 0, -1, 0, -1, 1, 0, 1, 1, 0, 0, -1, 0, -1, 0, 0, 0};
static signed char ctbl93nit[30] = {0, 0, 0, 0, 0, 0,-6, 3, -9, -2, 1, -2, -2, 0, -2, -2, 0, -2, 0, 0, 0, 0, 0, -1, -1, 0, 0, 0, 0, 0};
static signed char ctbl98nit[30] = {0, 0, 0, 0, 0, 0,-6, 4, -9, -2, 1, -2, -1, 0, -1, -2, 0, -1, 1, 0, 0, 1, 0, 0, -1, 0, 0, 0, 0, 0};
static signed char ctbl105nit[30] = {0, 0, 0, 0, 0, 0,-6, 4, -10, -3, 1, -3, -1, 0, -1, -1, 0, 0, 0, 0, 0, 0, 0, -1, 0, 0, 0, 0, 0, 1};
static signed char ctbl111nit[30] = {0, 0, 0, 0, 0, 0,-8, 4, -11, -2, 1, -1, -2, 0, -2, -1, 0, 0, 0, 0, 0, 0, 0, -1, -1, 0, -1, 0, 0, 1};
static signed char ctbl119nit[30] = {0, 0, 0, 0, 0, 0,-6, 3, -7, -2, 1, -3, -2, 0, -1, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static signed char ctbl126nit[30] = {0, 0, 0, 0, 0, 0,-6, 2, -8, -2, 1, -2, -1, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0, -1, -1, 0, -1, 0, 0, 1};
static signed char ctbl134nit[30] = {0, 0, 0, 0, 0, 0,-5, 3, -7, -2, 0, -2, 0, 0, 0, -2, 0, -1, 0, 0, 0, -1, 0, -1, 0, 0, 0, 0, 0, 0};
static signed char ctbl143nit[30] = {0, 0, 0, 0, 0, 0,-5, 3, -7, -3, 1, -3, -1, 0, -1, -1, 0, 0, 0, 0, 0, 0, 0, -1, -1, 0, 0, 0, 0, 0};
static signed char ctbl152nit[30] = {0, 0, 0, 0, 0, 0,-4, 3, -6, -2, 1, -2, -1, 0, -1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1};
static signed char ctbl162nit[30] = {0, 0, 0, 0, 0, 0,-6, 2, -6, -1, 0, -1, -1, 0, -1, -1, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static signed char ctbl172nit[30] = {0, 0, 0, 0, 0, 0,-5, 2, -5, -2, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static signed char ctbl183nit[30] = {0, 0, 0, 0, 0, 0,-6, 3, -6, -2, 0, -2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static signed char ctbl195nit[30] = {0, 0, 0, 0, 0, 0,-4, 2, -3, -1, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static signed char ctbl207nit[30] = {0, 0, 0, 0, 0, 0,-4, 1, -4, -1, 0, -2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -1, 0, 0, 1};
static signed char ctbl220nit[30] = {0, 0, 0, 0, 0, 0,-2, 1, -3, -1, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, -1, 0, 0, 0, 0, 0};
static signed char ctbl234nit[30] = {0, 0, 0, 0, 0, 0,-2, 1, -3, -1, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, -1, 0, -1, 0, 0, 0};
static signed char ctbl249nit[30] = {0, 0, 0, 0, 0, 0,0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static signed char ctbl265nit[30] = {0, 0, 0, 0, 0, 0,0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static signed char ctbl282nit[30] = {0, 0, 0, 0, 0, 0,0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static signed char ctbl300nit[30] = {0, 0, 0, 0, 0, 0,0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static signed char ctbl316nit[30] = {0, 0, 0, 0, 0, 0,0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static signed char ctbl333nit[30] = {0, 0, 0, 0, 0, 0,0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static signed char ctbl360nit[30] = {0, 0, 0, 0, 0, 0,0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

static signed char aid9845[3] = {0xB2, 0x70, 0x72};
static signed char aid9768[3] = {0xB2, 0x70, 0x63};
static signed char aid9685[3] = {0xB2, 0x70, 0x53};
static signed char aid9597[3] = {0xB2, 0x70, 0x42};
static signed char aid9520[3] = {0xB2, 0x70, 0x33};
static signed char aid9437[3] = {0xB2, 0x70, 0x23};
static signed char aid9354[3] = {0xB2, 0x70, 0x13};
static signed char aid9272[3] = {0xB2, 0x70, 0x03};
static signed char aid9189[3] = {0xB2, 0x60, 0xF3};
static signed char aid9096[3] = {0xB2, 0x60, 0xE1};
static signed char aid8982[3] = {0xB2, 0x60, 0xCB};
static signed char aid8931[3] = {0xB2, 0x60, 0xC1};
static signed char aid8802[3] = {0xB2, 0x60, 0xA8};
static signed char aid8714[3] = {0xB2, 0x60, 0x97};
static signed char aid8626[3] = {0xB2, 0x60, 0x86};
static signed char aid8574[3] = {0xB2, 0x60, 0x7C};
static signed char aid8414[3] = {0xB2, 0x60, 0x5D};
static signed char aid8270[3] = {0xB2, 0x60, 0x41};
static signed char aid8192[3] = {0xB2, 0x60, 0x32};
static signed char aid8115[3] = {0xB2, 0x60, 0x23};
static signed char aid7944[3] = {0xB2, 0x60, 0x02};
static signed char aid7862[3] = {0xB2, 0x50, 0xF2};
static signed char aid7639[3] = {0xB2, 0x50, 0xC7};
static signed char aid7531[3] = {0xB2, 0x50, 0xB2};
static signed char aid7454[3] = {0xB2, 0x50, 0xA3};
static signed char aid7309[3] = {0xB2, 0x50, 0x87};
static signed char aid7133[3] = {0xB2, 0x50, 0x65};
static signed char aid6865[3] = {0xB2, 0x50, 0x31};
static signed char aid6730[3] = {0xB2, 0x50, 0x17};
static signed char aid6560[3] = {0xB2, 0x40, 0xF6};
static signed char aid6312[3] = {0xB2, 0x40, 0xC6};
static signed char aid6059[3] = {0xB2, 0x40, 0x95};
static signed char aid5790[3] = {0xB2, 0x40, 0x61};
static signed char aid5496[3] = {0xB2, 0x40, 0x28};
static signed char aid5238[3] = {0xB2, 0x30, 0xF6};
static signed char aid4892[3] = {0xB2, 0x30, 0xB3};
static signed char aid4809[3] = {0xB2, 0x30, 0xA3};
static signed char aid4246[3] = {0xB2, 0x30, 0x36};
static signed char aid3807[3] = {0xB2, 0x20, 0xE1};
static signed char aid3404[3] = {0xB2, 0x20, 0x93};
static signed char aid2841[3] = {0xB2, 0x20, 0x26};
static signed char aid2319[3] = {0xB2, 0x10, 0xC1};
static signed char aid1772[3] = {0xB2, 0x10, 0x57};
static signed char aid1193[3] = {0xB2, 0x00, 0xE7};
static signed char aid0584[3] = {0xB2, 0x00, 0x71};
static signed char aid0093[3] = {0xB2, 0x00, 0x12};

static unsigned char delv0 [3] = {0xB6, 0xBC, 0x15};
static unsigned char delv1 [3] = {0xB6, 0xBC, 0x14};
static unsigned char delv2 [3] = {0xB6, 0xBC, 0x13};
static unsigned char delv3 [3] = {0xB6, 0xBC, 0x12};
static unsigned char delv4 [3] = {0xB6, 0xBC, 0x11};
static unsigned char delv5 [3] = {0xB6, 0xBC, 0x10};
static unsigned char delv6 [3] = {0xB6, 0xBC, 0x0F};
static unsigned char delv7 [3] = {0xB6, 0xBC, 0x0E};
static unsigned char delv8 [3] = {0xB6, 0xBC, 0x0D};
static unsigned char delv9 [3] = {0xB6, 0xBC, 0x0C};
static unsigned char delv10 [3] = {0xB6, 0xBC, 0x0B};
static unsigned char delv11 [3] = {0xB6, 0xBC, 0x0A};

static unsigned char delvAcl0 [3] = {0xB6, 0xAC, 0x15};
static unsigned char delvAcl1 [3] = {0xB6, 0xAC, 0x14};
static unsigned char delvAcl2 [3] = {0xB6, 0xAC, 0x13};
static unsigned char delvAcl3 [3] = {0xB6, 0xAC, 0x12};
static unsigned char delvAcl4 [3] = {0xB6, 0xAC, 0x11};
static unsigned char delvAcl5 [3] = {0xB6, 0xAC, 0x10};
static unsigned char delvAcl6 [3] = {0xB6, 0xAC, 0x0F};
static unsigned char delvAcl7 [3] = {0xB6, 0xAC, 0x0E};
static unsigned char delvAcl8 [3] = {0xB6, 0xAC, 0x0D};
static unsigned char delvAcl9 [3] = {0xB6, 0xAC, 0x0C};
static unsigned char delvAcl10 [3] = {0xB6, 0xAC, 0x0B};
static unsigned char delvAcl11 [3] = {0xB6, 0xAC, 0x0A};


#endif
