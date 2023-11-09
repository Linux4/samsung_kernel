/* linux/drivers/video/exynos/decon_7580/panels/s6e88a0_aid_dimming.h
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

#ifndef __S6E88AO_AID_DIMMING_H__
#define __S6E88AO_AID_DIMMING_H__

/* s6d8aa0 */

static const signed char rtbl5nit[10] = {0, 25, 22, 18, 14, 10, 6, 4, 3, 0};
static const signed char rtbl6nit[10] = {0, 23, 20, 15, 12, 8, 4, 3, 1, 0};
static const signed char rtbl7nit[10] = {0, 21, 18, 13, 10, 7, 3, 2, 1, 0};
static const signed char rtbl8nit[10] = {0, 19, 17, 12, 9, 6, 3, 2, 1, 0};
static const signed char rtbl9nit[10] = {0, 18, 15, 11, 8, 5, 2, 1, 1, 0};
static const signed char rtbl10nit[10] = {0, 18, 14, 10, 7, 4, 2, 1, 1, 0};
static const signed char rtbl11nit[10] = {0, 17, 14, 10, 7, 4, 2, 1, 1, 0};
static const signed char rtbl12nit[10] = {0, 16, 13, 9, 6, 4, 1, 1, 1, 0};
static const signed char rtbl13nit[10] = {0, 16, 13, 8, 6, 3, 1, 1, 1, 0};
static const signed char rtbl14nit[10] = {0, 15, 12, 8, 6, 3, 1, 1, 1, 0};
static const signed char rtbl15nit[10] = {0, 14, 12, 7, 5, 3, 1, 1, 1, 0};
static const signed char rtbl16nit[10] = {0, 14, 11, 7, 5, 2, 1, 1, 1, 0};
static const signed char rtbl17nit[10] = {0, 14, 11, 7, 5, 3, 1, 1, 1, 0};
static const signed char rtbl19nit[10] = {0, 13, 10, 6, 4, 2, 1, 1, 1, 0};
static const signed char rtbl20nit[10] = {0, 13, 11, 7, 5, 3, 2, 2, 1, 0};
static const signed char rtbl21nit[10] = {0, 13, 11, 7, 5, 3, 2, 2, 1, 0};
static const signed char rtbl22nit[10] = {0, 13, 11, 7, 6, 4, 2, 2, 1, 0};
static const signed char rtbl24nit[10] = {0, 13, 11, 7, 6, 4, 2, 2, 1, 0};
static const signed char rtbl25nit[10] = {0, 13, 11, 7, 6, 4, 3, 3, 1, 0};
static const signed char rtbl27nit[10] = {0, 13, 11, 7, 6, 4, 3, 3, 2, 0};
static const signed char rtbl29nit[10] = {0, 12, 11, 7, 6, 4, 3, 3, 2, 0};
static const signed char rtbl30nit[10] = {0, 12, 11, 7, 6, 4, 3, 3, 2, 0};
static const signed char rtbl32nit[10] = {0, 11, 10, 7, 5, 4, 3, 3, 2, 0};
static const signed char rtbl34nit[10] = {0, 11, 10, 7, 5, 3, 3, 3, 2, 0};
static const signed char rtbl37nit[10] = {0, 11, 9, 6, 5, 3, 3, 3, 2, 0};
static const signed char rtbl39nit[10] = {0, 11, 9, 6, 5, 3, 3, 3, 2, 0};
static const signed char rtbl41nit[10] = {0, 10, 8, 6, 4, 3, 3, 3, 2, 0};
static const signed char rtbl44nit[10] = {0, 10, 8, 5, 4, 3, 2, 2, 1, 0};
static const signed char rtbl47nit[10] = {0, 9, 7, 5, 4, 3, 2, 2, 1, 0};
static const signed char rtbl50nit[10] = {0, 9, 7, 5, 4, 3, 2, 2, 1, 0};
static const signed char rtbl53nit[10] = {0, 8, 7, 5, 4, 3, 2, 2, 1, 0};
static const signed char rtbl56nit[10] = {0, 8, 6, 4, 3, 3, 2, 2, 1, 0};
static const signed char rtbl60nit[10] = {0, 8, 6, 4, 3, 2, 2, 2, 1, 0};
static const signed char rtbl64nit[10] = {0, 7, 6, 4, 3, 2, 2, 2, 1, 0};
static const signed char rtbl68nit[10] = {0, 7, 5, 4, 3, 2, 2, 2, 1, 0};
static const signed char rtbl72nit[10] = {0, 6, 5, 4, 3, 2, 2, 2, 1, 0};
static const signed char rtbl77nit[10] = {0, 6, 4, 3, 2, 2, 2, 2, 2, 0};
static const signed char rtbl82nit[10] = {0, 6, 4, 3, 2, 2, 2, 2, 2, 0};
static const signed char rtbl87nit[10] = {0, 6, 4, 3, 2, 3, 2, 2, 2, 0};
static const signed char rtbl93nit[10] = {0, 6, 4, 3, 2, 3, 2, 2, 2, 0};
static const signed char rtbl98nit[10] = {0, 6, 4, 3, 3, 2, 2, 2, 1, 0};
static const signed char rtbl105nit[10] = {0, 6, 4, 3, 2, 2, 2, 2, 1, 0};
static const signed char rtbl111nit[10] = {0, 6, 4, 3, 2, 2, 2, 2, 1, 0};
static const signed char rtbl119nit[10] = {0, 6, 3, 3, 2, 2, 1, 2, 1, 0};
static const signed char rtbl126nit[10] = {0, 5, 2, 2, 1, 2, 1, 1, 0, 0};
static const signed char rtbl134nit[10] = {0, 5, 2, 2, 1, 1, 1, 1, 0, 0};
static const signed char rtbl143nit[10] = {0, 4, 2, 2, 1, 1, 1, 1, 0, 0};
static const signed char rtbl152nit[10] = {0, 4, 2, 2, 1, 1, 1, 1, 0, 0};
static const signed char rtbl162nit[10] = {0, 4, 1, 2, 0, 1, 1, 1, 0, 0};
static const signed char rtbl172nit[10] = {0, 3, 1, 2, 0, 1, 1, 1, 0, 0};
static const signed char rtbl183nit[10] = {0, 3, 1, 2, 1, 2, 1, 2, 1, 0};
static const signed char rtbl195nit[10] = {0, 2, 1, 1, 1, 2, 1, 1, 1, 0};
static const signed char rtbl207nit[10] = {0, 2, 1, 1, 1, 1, 0, 1, 0, 0};
static const signed char rtbl220nit[10] = {0, 2, 1, 1, 1, 0, 0, 1, 0, 0};
static const signed char rtbl234nit[10] = {0, 2, 1, 0, 1, 1, 0, 2, 0, 0};
static const signed char rtbl249nit[10] = {0, 1, 1, 1, 1, 1, 0, 1, 0, 0};
static const signed char rtbl265nit[10] = {0, 1, 0, 0, 0, 0, 0, 1, 0, 0};
static const signed char rtbl282nit[10] = {0, 1, 0, 0, 0, 0, 0, 1, 0, 0};
static const signed char rtbl300nit[10] = {0, 1, 0, 0, 0, -1, -1, 0, 0, 0};
static const signed char rtbl316nit[10] = {0, 1, 0, 0, 0, -1, -1, 0, -1, 0};
static const signed char rtbl333nit[10] = {0, 0, -1, -1, -1, -1, -2, 0, -1, 0};
static const signed char rtbl350nit[10] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

static const signed char ctbl5nit[30] = {0, 0, 0, 0, 0, 0, -2, 0, -3, -6, 0, -1, -7, 0, -1, -11, 0, -2, -6, 0, -2, -2, 0, 0, 0, 0, 0, -1, 0, 0};
static const signed char ctbl6nit[30] = {0, 0, 0, 0, 0, 0, -4, 0, -2, -6, 0, -5, -7, 0, -1, -11, 0, -2, -4, 0, -1, -1, 0, 0, 0, 0, 0, -1, 0, 0};
static const signed char ctbl7nit[30] = {0, 0, 0, 0, 0, 0, -5, 0, -3, -4, 0, -4, -8, 0, -3, -9, 0, -1, -4, 0, 0, -2, 0, 0, -1, 0, 0, 0, 0, 0};
static const signed char ctbl8nit[30] = {0, 0, 0, 0, 0, 0, -7, 0, -5, -5, 0, -2, -8, 0, -4, -9, 0, -1, -4, 0, 0, -2, 0, 0, -1, 0, 0, 0, 0, 0};
static const signed char ctbl9nit[30] = {0, 0, 0, 0, 0, 0, -8, 0, -8, -6, 0, -2, -8, 0, -4, -8, 0, -1, -3, 0, -1, 0, 0, 1, -1, 0, 0, 0, 0, 0};
static const signed char ctbl10nit[30] = {0, 0, 0, 0, 0, 0, -9, 0, -9, -9, 0, -5, -7, 0, -4, -8, 0, -1, -2, 0, 0, 0, 0, 1, -1, 0, 0, 0, 0, 0};
static const signed char ctbl11nit[30] = {0, 0, 0, 0, 0, 0, -10, 0, -9, -8, 0, -2, -5, 0, -3, -7, 0, 0, -2, 0, 0, -1, 0, 0, -1, 0, 0, 0, 0, 0};
static const signed char ctbl12nit[30] = {0, 0, 0, 0, 0, 0, -10, 0, -9, -7, 0, -2, -7, 0, -4, -7, 0, 0, -3, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0};
static const signed char ctbl13nit[30] = {0, 0, 0, 0, 0, 0, -9, 0, -5, -10, 0, -5, -6, 0, -3, -7, 0, 0, -2, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0};
static const signed char ctbl14nit[30] = {0, 0, 0, 0, 0, 0, -11, 0, -9, -10, 0, -5, -5, 0, -3, -7, 0, 0, -2, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0};
static const signed char ctbl15nit[30] = {0, 0, 0, 0, 0, 0, -12, 0, -5, -8, 0, -5, -7, 0, -5, -5, 0, 1, -2, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0};
static const signed char ctbl16nit[30] = {0, 0, 0, 0, 0, 0, -11, 0, -8, -9, 0, -4, -5, 0, -5, -5, 0, 1, -2, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0};
static const signed char ctbl17nit[30] = {0, 0, 0, 0, 0, 0, -11, 0, -8, -13, 0, -5, -4, 0, -4, -4, 0, 2, -3, 0, -1, -1, 0, 0, 0, 0, 0, 0, 0, 0};
static const signed char ctbl19nit[30] = {0, 0, 0, 0, 0, 0, -11, 0, -8, -11, 0, -6, -5, 0, -5, -4, 0, 2, -2, 0, -1, -1, 0, 0, 0, 0, 0, 0, 0, 0};
static const signed char ctbl20nit[30] = {0, 0, 0, 0, 0, 0, -10, 0, -7, -7, 0, -3, -5, 0, -5, -5, 0, 1, -2, 0, -1, -1, 0, 0, 0, 0, 0, 0, 0, 0};
static const signed char ctbl21nit[30] = {0, 0, 0, 0, 0, 0, -8, 0, -8, -7, 0, -4, -5, 0, -5, -5, 0, 1, -2, 0, -1, -1, 0, 0, 0, 0, 0, 0, 0, 0};
static const signed char ctbl22nit[30] = {0, 0, 0, 0, 0, 0, -8, 0, -8, -7, 0, -4, -5, 0, -5, -4, 0, 1, -1, 0, -1, -1, 0, 0, -1, 0, 0, 1, 0, 0};
static const signed char ctbl24nit[30] = {0, 0, 0, 0, 0, 0, -8, 0, -8, -7, 0, -4, -4, 0, -4, -4, 0, 0, -1, 0, -1, -1, 0, 0, -1, 0, 0, 1, 0, 0};
static const signed char ctbl25nit[30] = {0, 0, 0, 0, 0, 0, -9, 0, -10, -6, 0, -4, -5, 0, -5, -4, 0, -1, 0, 0, 0, -1, 0, 0, 0, 0, 0, 1, 0, 0};
static const signed char ctbl27nit[30] = {0, 0, 0, 0, 0, 0, -9, 0, -11, -6, 0, -4, -5, 0, -5, -2, 0, 0, -1, 0, -1, -1, 0, 0, 0, 0, 0, 1, 0, 0};
static const signed char ctbl29nit[30] = {0, 0, 0, 0, 0, 0, -8, 0, -8, -6, 0, -3, -4, 0, -5, -3, 0, -1, 0, 0, 0, -1, 0, 0, 0, 0, 0, 1, 0, 0};
static const signed char ctbl30nit[30] = {0, 0, 0, 0, 0, 0, -8, 0, -7, -6, 0, -4, -4, 0, -5, -3, 0, -1, 0, 0, 0, -1, 0, 0, 0, 0, 0, 1, 0, 0};
static const signed char ctbl32nit[30] = {0, 0, 0, 0, 0, 0, -9, 0, -11, -6, 0, -3, -3, 0, -5, -3, 0, -1, 0, 0, 0, -1, 0, 0, 0, 0, 0, 1, 0, 0};
static const signed char ctbl34nit[30] = {0, 0, 0, 0, 0, 0, -8, 0, -10, -5, 0, -2, -3, 0, -5, -3, 0, -1, 0, 0, 0, -1, 0, 0, 0, 0, 0, 1, 0, 0};
static const signed char ctbl37nit[30] = {0, 0, 0, 0, 0, 0, -8, 0, -12, -6, 0, -4, -3, 0, -5, -2, 0, 0, 0, 0, 0, -1, 0, 0, 0, 0, 0, 1, 0, 0};
static const signed char ctbl39nit[30] = {0, 0, 0, 0, 0, 0, -7, 0, -10, -6, 0, -4, -3, 0, -5, -2, 0, 0, 0, 0, 0, -1, 0, 0, 0, 0, 0, 1, 0, 0};
static const signed char ctbl41nit[30] = {0, 0, 0, 0, 0, 0, -7, 0, -12, -6, 0, -5, -3, 0, -5, -2, 0, 0, 0, 0, 0, -1, 0, 0, 0, 0, 0, 1, 0, 0};
static const signed char ctbl44nit[30] = {0, 0, 0, 0, 0, 0, -6, 0, -8, -6, 0, -6, -3, 0, -5, -2, 0, 2, 0, 0, -1, -2, 0, 0, 0, 0, 0, 1, 0, 0};
static const signed char ctbl47nit[30] = {0, 0, 0, 0, 0, 0, -7, 0, -11, -4, 0, -4, -3, 0, -5, -2, 0, 2, 0, 0, -1, -2, 0, 0, 0, 0, 0, 1, 0, 0};
static const signed char ctbl50nit[30] = {0, 0, 0, 0, 0, 0, -9, 0, -9, -3, 0, -6, -3, 0, -5, -2, 0, 2, 0, 0, -1, -2, 0, 0, 0, 0, 0, 1, 0, 0};
static const signed char ctbl53nit[30] = {0, 0, 0, 0, 0, 0, -8, 0, -8, -3, 0, -3, -3, 0, -5, -2, 0, 2, 0, 0, -1, -2, 0, 0, 0, 0, 0, 1, 0, 0};
static const signed char ctbl56nit[30] = {0, 0, 0, 0, 0, 0, -8, 0, -9, -4, 0, -6, -2, 0, -5, -2, 0, 2, 0, 0, -1, -2, 0, 0, 0, 0, 0, 1, 0, 0};
static const signed char ctbl60nit[30] = {0, 0, 0, 0, 0, 0, -8, 0, -8, -3, 0, -5, -2, 0, -5, -2, 0, 2, 0, 0, -1, -2, 0, 0, 0, 0, 0, 1, 0, 0};
static const signed char ctbl64nit[30] = {0, 0, 0, 0, 0, 0, -7, 0, -6, -3, 0, -4, -2, 0, -5, -2, 0, 2, 0, 0, -1, -2, 0, 0, 0, 0, 0, 1, 0, 0};
static const signed char ctbl68nit[30] = {0, 0, 0, 0, 0, 0, -8, 0, -9, -3, 0, -3, -1, 0, -4, -2, 0, 2, 0, 0, -1, -2, 0, 0, 0, 0, 0, 1, 0, 0};
static const signed char ctbl72nit[30] = {0, 0, 0, 0, 0, 0, -7, 0, -6, -2, 0, -3, -2, 0, -4, -2, 0, 1, 0, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0};
static const signed char ctbl77nit[30] = {0, 0, 0, 0, 0, 0, -7, 0, -6, -4, 0, -3, -1, 0, -5, -2, 0, 2, 0, 0, 0, -1, 0, 1, 0, 0, -1, 0, 0, 0};
static const signed char ctbl82nit[30] = {0, 0, 0, 0, 0, 0, -7, 0, -7, -2, 0, -3, -1, 0, -4, -1, 0, 2, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, -1};
static const signed char ctbl87nit[30] = {0, 0, 0, 0, 0, 0, -7, 0, -7, -3, 0, -4, 1, 0, -2, -1, 0, 2, 0, 0, 1, -1, 0, 0, 0, 0, 0, 1, 0, 0};
static const signed char ctbl93nit[30] = {0, 0, 0, 0, 0, 0, -6, 0, -5, -5, 0, -5, 1, 0, -2, -1, 0, 1, 0, 0, 1, 0, 0, 0, -1, 0, -1, 1, 0, 0};
static const signed char ctbl98nit[30] = {0, 0, 0, 0, 0, 0, -6, 0, -5, -1, 0, -3, 0, 0, -2, -1, 0, 1, 0, 0, 1, -1, 0, 0, 0, 0, 0, 2, 0, 2};
static const signed char ctbl105nit[30] = {0, 0, 0, 0, 0, 0, -5, 0, -4, -1, 0, -1, 0, 0, -2, 0, 0, 2, 0, 0, 1, 0, 0, -1, 0, 0, 0, 1, 0, 1};
static const signed char ctbl111nit[30] = {0, 0, 0, 0, 0, 0, -6, 0, -5, -2, 0, -2, 1, 0, -1, -1, 0, 1, -1, 0, 0, 0, 0, 0, 1, 0, 1, 1, 0, 1};
static const signed char ctbl119nit[30] = {0, 0, 0, 0, 0, 0, -5, 0, -5, -2, 0, -2, 1, 0, -1, -1, 0, 1, 0, 0, 1, 0, 0, 0, 1, 0, 1, 1, 0, 1};
static const signed char ctbl126nit[30] = {0, 0, 0, 0, 0, 0, -4, 0, -6, -1, 0, -3, 1, 0, -1, 0, 0, 2, 0, 0, 0, 1, 0, 0, 1, 0, 1, 1, 0, 1};
static const signed char ctbl134nit[30] = {0, 0, 0, 0, 0, 0, -3, 0, -3, -2, 0, -4, 1, 0, -1, 0, 0, 2, 0, 0, 0, 1, 0, 0, 1, 0, 1, 1, 0, 1};
static const signed char ctbl143nit[30] = {0, 0, 0, 0, 0, 0, -1, 0, -1, -2, 0, -4, 1, 0, -1, 0, 0, 2, 0, 0, 0, 1, 0, 0, 1, 0, 1, 1, 0, 1};
static const signed char ctbl152nit[30] = {0, 0, 0, 0, 0, 0, -1, 0, 0, -2, 0, -4, 1, 0, -2, 2, 0, 3, 0, 0, 0, 0, 0, 0, 1, 0, 1, 1, 0, 1};
static const signed char ctbl162nit[30] = {0, 0, 0, 0, 0, 0, -2, 0, 1, 0, 0, -1, 2, 0, -2, 1, 0, 2, 0, 0, 0, 0, 0, 0, 1, 0, 1, 1, 0, 1};
static const signed char ctbl172nit[30] = {0, 0, 0, 0, 0, 0, -1, 0, 3, -1, 0, -2, 2, 0, -2, 1, 0, 2, 0, 0, 0, 0, 0, 0, 1, 0, 1, 1, 0, 1};
static const signed char ctbl183nit[30] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static const signed char ctbl195nit[30] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static const signed char ctbl207nit[30] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static const signed char ctbl220nit[30] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static const signed char ctbl234nit[30] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static const signed char ctbl249nit[30] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static const signed char ctbl265nit[30] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static const signed char ctbl282nit[30] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static const signed char ctbl300nit[30] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static const signed char ctbl316nit[30] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static const signed char ctbl333nit[30] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static const signed char ctbl350nit[30] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

static const signed char aid5[6] = {0xB2, 0x40, 0x00, 0x28, 0x03, 0xB8};
static const signed char aid6[6] = {0xB2, 0x40, 0x00, 0x28, 0x03, 0xB1};
static const signed char aid7[6] = {0xB2, 0x40, 0x00, 0x28, 0x03, 0xA9};
static const signed char aid8[6] = {0xB2, 0x40, 0x00, 0x28, 0x03, 0xA3};
static const signed char aid9[6] = {0xB2, 0x40, 0x00, 0x28, 0x03, 0x9B};
static const signed char aid10[6] = {0xB2, 0x40, 0x00, 0x28, 0x03, 0x93};
static const signed char aid11[6] = {0xB2, 0x40, 0x00, 0x28, 0x03, 0x8D};
static const signed char aid12[6] = {0xB2, 0x40, 0x00, 0x28, 0x03, 0x84};
static const signed char aid13[6] = {0xB2, 0x40, 0x00, 0x28, 0x03, 0x7D};
static const signed char aid14[6] = {0xB2, 0x40, 0x00, 0x28, 0x03, 0x76};
static const signed char aid15[6] = {0xB2, 0x40, 0x00, 0x28, 0x03, 0x6E};
static const signed char aid16[6] = {0xB2, 0x40, 0x00, 0x28, 0x03, 0x66};
static const signed char aid17[6] = {0xB2, 0x40, 0x00, 0x28, 0x03, 0x60};
static const signed char aid19[6] = {0xB2, 0x40, 0x00, 0x28, 0x03, 0x51};
static const signed char aid20[6] = {0xB2, 0x40, 0x00, 0x28, 0x03, 0x48};
static const signed char aid21[6] = {0xB2, 0x40, 0x00, 0x28, 0x03, 0x3E};
static const signed char aid22[6] = {0xB2, 0x40, 0x00, 0x28, 0x03, 0x35};
static const signed char aid24[6] = {0xB2, 0x40, 0x00, 0x28, 0x03, 0x23};
static const signed char aid25[6] = {0xB2, 0x40, 0x00, 0x28, 0x03, 0x19};
static const signed char aid27[6] = {0xB2, 0x40, 0x00, 0x28, 0x03, 0x07};
static const signed char aid29[6] = {0xB2, 0x40, 0x00, 0x28, 0x02, 0xF7};
static const signed char aid30[6] = {0xB2, 0x40, 0x00, 0x28, 0x02, 0xED};
static const signed char aid32[6] = {0xB2, 0x40, 0x00, 0x28, 0x02, 0xDD};
static const signed char aid34[6] = {0xB2, 0x40, 0x00, 0x28, 0x02, 0xCD};
static const signed char aid37[6] = {0xB2, 0x40, 0x00, 0x28, 0x02, 0xB5};
static const signed char aid39[6] = {0xB2, 0x40, 0x00, 0x28, 0x02, 0xA5};
static const signed char aid41[6] = {0xB2, 0x40, 0x00, 0x28, 0x02, 0x95};
static const signed char aid44[6] = {0xB2, 0x40, 0x00, 0x28, 0x02, 0x7B};
static const signed char aid47[6] = {0xB2, 0x40, 0x00, 0x28, 0x02, 0x63};
static const signed char aid50[6] = {0xB2, 0x40, 0x00, 0x28, 0x02, 0x4B};
static const signed char aid53[6] = {0xB2, 0x40, 0x00, 0x28, 0x02, 0x31};
static const signed char aid56[6] = {0xB2, 0x40, 0x00, 0x28, 0x02, 0x18};
static const signed char aid60[6] = {0xB2, 0x40, 0x00, 0x28, 0x01, 0xF6};
static const signed char aid64[6] = {0xB2, 0x40, 0x00, 0x28, 0x01, 0xD5};
static const signed char aid68[6] = {0xB2, 0x40, 0x00, 0x28, 0x01, 0xB5};
static const signed char aid72[6] = {0xB2, 0x40, 0x00, 0x28, 0x01, 0x8F};
static const signed char aid119[6] = {0xB2, 0x40, 0x00, 0x28, 0x01, 0x63};
static const signed char aid126[6] = {0xB2, 0x40, 0x00, 0x28, 0x01, 0x3B};
static const signed char aid134[6] = {0xB2, 0x40, 0x00, 0x28, 0x01, 0x13};
static const signed char aid143[6] = {0xB2, 0x40, 0x00, 0x28, 0x00, 0xDF};
static const signed char aid152[6] = {0xB2, 0x40, 0x00, 0x28, 0x00, 0xB1};
static const signed char aid162[6] = {0xB2, 0x40, 0x00, 0x28, 0x00, 0x78};
static const signed char aid172[6] = {0xB2, 0x40, 0x00, 0x28, 0x00, 0x45};
static const signed char aid183[6] = {0xB2, 0x40, 0x00, 0x28, 0x00, 0x0A};

static unsigned char elv_0B[6] = {0xB6, 0x2C, 0x0B, 0x11, 0x1E, 0x1E};
static unsigned char elv_0C[6] = {0xB6, 0x2C, 0x0C, 0x00, 0x00, 0x00};
static unsigned char elv_0D[6] = {0xB6, 0x2C, 0x0D, 0x11, 0x1C, 0x1C};
static unsigned char elv_0E[6] = {0xB6, 0x2C, 0x0E, 0x00, 0x00, 0x00};
static unsigned char elv_0F[6] = {0xB6, 0x2C, 0x0F, 0x11, 0x1A, 0x1A};
static unsigned char elv_10[6] = {0xB6, 0x2C, 0x10, 0x00, 0x00, 0x00};
static unsigned char elv_11[6] = {0xB6, 0x2C, 0x11, 0x11, 0x18, 0x18};
static unsigned char elv_12[6] = {0xB6, 0x2C, 0x12, 0x00, 0x00, 0x00};
static unsigned char elv_13[6] = {0xB6, 0x2C, 0x13, 0x11, 0x16, 0x16};
static unsigned char elv_14[6] = {0xB6, 0x2C, 0x14, 0x00, 0x00, 0x00};
static unsigned char elv_15[6] = {0xB6, 0x2C, 0x15, 0x11, 0x14, 0x14};
static unsigned char elv_16[6] = {0xB6, 0x2C, 0x16, 0x00, 0x00, 0x00};
static unsigned char elv_17[6] = {0xB6, 0x2C, 0x17, 0x11, 0x12, 0x12};
static unsigned char elv_18[6] = {0xB6, 0x2C, 0x18, 0x00, 0x00, 0x00};
#endif
