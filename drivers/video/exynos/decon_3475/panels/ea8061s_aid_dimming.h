/* linux/drivers/video/exynos/decon_7580/panels/ea8061s_aid_dimming.h
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

#ifndef __EA8061S_AID_DIMMING_H__
#define __EA8061S_AID_DIMMING_H__

/* ea8061s */

static const signed char rtbl5nit_ea[10] = {0, 6, 23, 18, 14, 11, 6, 3, 2, 0};
static const signed char rtbl6nit_ea[10] = {0, 6, 21, 16, 12, 9, 6, 4, 3, 0};
static const signed char rtbl7nit_ea[10] = {0, 6, 19, 14, 11, 8, 5, 3, 2, 0};
static const signed char rtbl8nit_ea[10] = {0, 6, 17, 13, 9, 7, 5, 3, 2, 0};
static const signed char rtbl9nit_ea[10] = {0, 6, 16, 12, 9, 6, 4, 3, 2, 0};
static const signed char rtbl10nit_ea[10] = {0, 6, 15, 11, 8, 5, 3, 3, 2, 0};
static const signed char rtbl11nit_ea[10] = {0, 6, 14, 10, 7, 5, 3, 3, 2, 0};
static const signed char rtbl12nit_ea[10] = {0, 6, 13, 9, 7, 5, 3, 2, 2, 0};
static const signed char rtbl13nit_ea[10] = {0, 6, 13, 8, 6, 4, 3, 2, 2, 0};
static const signed char rtbl14nit_ea[10] = {0, 6, 12, 8, 6, 4, 3, 2, 2, 0};
static const signed char rtbl15nit_ea[10] = {0, 6, 12, 8, 6, 5, 3, 2, 2, 0};
static const signed char rtbl16nit_ea[10] = {0, 5, 11, 7, 5, 3, 3, 2, 2, 0};
static const signed char rtbl17nit_ea[10] = {0, 5, 11, 7, 4, 4, 2, 2, 2, 0};
static const signed char rtbl19nit_ea[10] = {0, 5, 10, 6, 4, 3, 2, 2, 2, 0};
static const signed char rtbl20nit_ea[10] = {0, 5, 10, 6, 4, 3, 2, 2, 2, 0};
static const signed char rtbl21nit_ea[10] = {0, 5, 10, 6, 5, 3, 2, 2, 2, 0};
static const signed char rtbl22nit_ea[10] = {0, 5, 11, 7, 5, 4, 3, 3, 2, 0};
static const signed char rtbl24nit_ea[10] = {0, 5, 11, 7, 5, 4, 3, 3, 2, 0};
static const signed char rtbl25nit_ea[10] = {0, 5, 11, 7, 5, 4, 4, 3, 2, 0};
static const signed char rtbl27nit_ea[10] = {0, 5, 11, 7, 5, 4, 4, 3, 2, 0};
static const signed char rtbl29nit_ea[10] = {0, 5, 11, 7, 5, 4, 4, 3, 2, 0};
static const signed char rtbl30nit_ea[10] = {0, 5, 11, 7, 5, 4, 4, 3, 2, 0};
static const signed char rtbl32nit_ea[10] = {0, 5, 10, 7, 5, 4, 4, 3, 2, 0};
static const signed char rtbl34nit_ea[10] = {0, 5, 10, 6, 5, 4, 3, 3, 2, 0};
static const signed char rtbl37nit_ea[10] = {0, 4, 9, 6, 4, 4, 3, 3, 2, 0};
static const signed char rtbl39nit_ea[10] = {0, 4, 9, 6, 4, 4, 3, 3, 2, 0};
static const signed char rtbl41nit_ea[10] = {0, 4, 8, 5, 4, 4, 3, 3, 2, 0};
static const signed char rtbl44nit_ea[10] = {0, 4, 8, 5, 4, 3, 3, 3, 2, 0};
static const signed char rtbl47nit_ea[10] = {0, 4, 8, 5, 3, 3, 3, 3, 2, 0};
static const signed char rtbl50nit_ea[10] = {0, 4, 7, 5, 3, 3, 3, 3, 2, 0};
static const signed char rtbl53nit_ea[10] = {0, 4, 7, 4, 3, 3, 3, 3, 2, 0};
static const signed char rtbl56nit_ea[10] = {0, 4, 6, 4, 3, 3, 3, 3, 2, 0};
static const signed char rtbl60nit_ea[10] = {0, 4, 6, 4, 3, 3, 3, 3, 2, 0};
static const signed char rtbl64nit_ea[10] = {0, 4, 6, 4, 2, 3, 3, 3, 2, 0};
static const signed char rtbl68nit_ea[10] = {0, 4, 5, 4, 2, 3, 3, 2, 2, 0};
static const signed char rtbl72nit_ea[10] = {0, 3, 5, 3, 2, 2, 3, 3, 2, 0};
static const signed char rtbl77nit_ea[10] = {0, 3, 4, 2, 2, 1, 3, 3, 2, 0};
static const signed char rtbl82nit_ea[10] = {0, 3, 4, 3, 2, 1, 3, 3, 2, 0};
static const signed char rtbl87nit_ea[10] = {0, 3, 4, 2, 2, 2, 3, 4, 1, 0};
static const signed char rtbl93nit_ea[10] = {0, 3, 5, 3, 2, 3, 4, 4, 1, 0};
static const signed char rtbl98nit_ea[10] = {0, 3, 4, 2, 3, 2, 4, 4, 1, 0};
static const signed char rtbl105nit_ea[10] = {0, 3, 4, 3, 3, 3, 5, 4, 1, 0};
static const signed char rtbl111nit_ea[10] = {0, 3, 4, 3, 2, 3, 4, 4, 1, 0};
static const signed char rtbl119nit_ea[10] = {0, 3, 3, 3, 2, 3, 4, 3, 1, 0};
static const signed char rtbl126nit_ea[10] = {0, 3, 3, 3, 2, 2, 3, 3, 1, 0};
static const signed char rtbl134nit_ea[10] = {0, 3, 3, 2, 1, 2, 3, 3, 1, 0};
static const signed char rtbl143nit_ea[10] = {0, 3, 3, 2, 1, 2, 3, 3, 1, 0};
static const signed char rtbl152nit_ea[10] = {0, 3, 2, 2, 1, 2, 3, 3, 1, 0};
static const signed char rtbl162nit_ea[10] = {0, 3, 2, 2, 1, 2, 3, 3, 1, 0};
static const signed char rtbl172nit_ea[10] = {0, 3, 2, 2, 1, 1, 2, 2, 1, 0};
static const signed char rtbl183nit_ea[10] = {0, 2, 2, 1, 1, 1, 2, 3, 1, 0};
static const signed char rtbl195nit_ea[10] = {0, 2, 2, 1, 1, 1, 2, 2, 0, 0};
static const signed char rtbl207nit_ea[10] = {0, 2, 2, 0, 1, 0, 1, 2, 0, 0};
static const signed char rtbl220nit_ea[10] = {0, 1, 1, 0, 1, 0, 1, 2, 0, 0};
static const signed char rtbl234nit_ea[10] = {0, 1, 1, 0, 1, 0, 1, 2, 0, 0};
static const signed char rtbl249nit_ea[10] = {0, 1, 1, 0, 0, 0, 1, 1, 0, 0};
static const signed char rtbl265nit_ea[10] = {0, 1, 0, 0, 0, 0, 0, 1, 0, 0};
static const signed char rtbl282nit_ea[10] = {0, 1, 0, 0, -1, 0, 0, 1, 0, 0};
static const signed char rtbl300nit_ea[10] = {0, 0, 0, 0, 0, -1, 0, 1, 0, 0};
static const signed char rtbl316nit_ea[10] = {0, 0, 0, 0, 0, -1, -1, 0, 0, 0};
static const signed char rtbl333nit_ea[10] = {0, 0, -1, -1, -1, -1, -1, -1, -1, 0};
static const signed char rtbl350nit_ea[10] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

static const signed char ctbl5nit_ea[30] = {0, 0, 0, 0, 0, 0, 1, 0, -1, -18, 0, -7, -16, 0, -7, -14, 0, -1, -1, 0, 0, -2, 0, -2, -1, 0, 0, 0, 0, 1};
static const signed char ctbl6nit_ea[30] = {0, 0, 0, 0, 0, 0, -1, 0, 0, -15, 0, -5, -4, 0, 1, -18, 0, -5, -1, 0, 0, -2, 0, -2, -1, 0, -1, 0, 0, 1};
static const signed char ctbl7nit_ea[30] = {0, 0, 0, 0, 0, 0, -9, 0, -6, -7, 0, 2, -6, 0, -2, -17, 0, 1, -1, 0, -1, -2, 0, -2, 0, 0, 0, 0, 0, 1};
static const signed char ctbl8nit_ea[30] = {0, 0, 0, 0, 0, 0, -1, 0, 3, -16, 0, -7, -11, 0, -3, -15, 0, -5, -1, 0, -1, -2, 0, -2, 0, 0, 0, 0, 0, 1};
static const signed char ctbl9nit_ea[30] = {0, 0, 0, 0, 0, 0, -10, 0, -6, -12, 0, -1, -7, 0, 1, -13, 0, -5, 2, 0, 1, -2, 0, -2, 0, 0, 0, 0, 0, 1};
static const signed char ctbl10nit_ea[30] = {0, 0, 0, 0, 0, 0, -9, 0, -5, -15, 0, -3, -7, 0, -6, -12, 0, 3, 2, 0, 0, -2, 0, -2, 0, 0, 0, 0, 0, 1};
static const signed char ctbl11nit_ea[30] = {0, 0, 0, 0, 0, 0, -8, 0, -6, -14, 0, -5, -7, 0, -5, -13, 0, 3, 2, 0, 1, -2, 0, -2, 0, 0, 0, 0, 0, 1};
static const signed char ctbl12nit_ea[30] = {0, 0, 0, 0, 0, 0, -12, 0, -11, -9, 0, -1, -6, 0, 0, -11, 0, -4, 0, 0, 2, -2, 0, -1, 0, 0, 0, 0, 0, 0};
static const signed char ctbl13nit_ea[30] = {0, 0, 0, 0, 0, 0, -14, 0, -13, -8, 0, 2, -5, 0, 0, -10, 0, -2, -2, 0, -1, -1, 0, -1, 0, 0, 0, 0, 0, 0};
static const signed char ctbl14nit_ea[30] = {0, 0, 0, 0, 0, 0, -14, 0, -12, -11, 0, -2, -4, 0, 0, -10, 0, -2, -1, 0, -1, -1, 0, -1, 0, 0, 0, 0, 0, 0};
static const signed char ctbl15nit_ea[30] = {0, 0, 0, 0, 0, 0, -13, 0, -10, -9, 0, -2, -7, 0, -6, -10, 0, 3, 1, 0, 2, -1, 0, -1, 0, 0, 0, 0, 0, 0};
static const signed char ctbl16nit_ea[30] = {0, 0, 0, 0, 0, 0, -18, 0, -14, -6, 0, 0, -8, 0, -3, -7, 0, 0, 1, 0, 1, -1, 0, -1, 0, 0, 0, 0, 0, 0};
static const signed char ctbl17nit_ea[30] = {0, 0, 0, 0, 0, 0, -18, 0, -12, -9, 0, -2, -6, 0, -5, -4, 0, 5, 3, 0, 2, -1, 0, -1, 0, 0, 0, 0, 0, 0};
static const signed char ctbl19nit_ea[30] = {0, 0, 0, 0, 0, 0, -18, 0, -15, -6, 0, 0, -2, 0, -5, -8, 0, 5, 1, 0, 0, -1, 0, -1, 0, 0, 0, 0, 0, 0};
static const signed char ctbl20nit_ea[30] = {0, 0, 0, 0, 0, 0, -20, 0, -12, -1, 0, -1, -3, 0, -5, -8, 0, 5, 1, 0, 0, -1, 0, -1, 0, 0, 0, 0, 0, 0};
static const signed char ctbl21nit_ea[30] = {0, 0, 0, 0, 0, 0, -12, 0, -9, -13, 0, -5, -2, 0, -7, -7, 0, 4, 1, 0, 0, -1, 0, -1, 0, 0, 0, 0, 0, 0};
static const signed char ctbl22nit_ea[30] = {0, 0, 0, 0, 0, 0, -15, 0, -9, -3, 0, 2, -4, 0, -3, -7, 0, -3, 1, 0, 1, -1, 0, -1, 0, 0, 0, 0, 0, 0};
static const signed char ctbl24nit_ea[30] = {0, 0, 0, 0, 0, 0, -14, 0, -9, -2, 0, 2, -4, 0, -2, -7, 0, -2, 2, 0, -1, -1, 0, -1, 0, 0, 0, 0, 0, 0};
static const signed char ctbl25nit_ea[30] = {0, 0, 0, 0, 0, 0, -14, 0, -11, -2, 0, 1, -4, 0, -2, -6, 0, -1, 2, 0, -1, -1, 0, -1, 0, 0, 0, 1, 0, 0};
static const signed char ctbl27nit_ea[30] = {0, 0, 0, 0, 0, 0, -13, 0, -12, -3, 0, 1, -3, 0, -2, -6, 0, -1, 2, 0, -1, -1, 0, -1, 0, 0, 0, 1, 0, 0};
static const signed char ctbl29nit_ea[30] = {0, 0, 0, 0, 0, 0, -14, 0, -12, -1, 0, 1, -3, 0, -2, -6, 0, -1, 2, 0, -1, -1, 0, -1, 0, 0, 0, 1, 0, 0};
static const signed char ctbl30nit_ea[30] = {0, 0, 0, 0, 0, 0, -14, 0, -12, -1, 0, 1, -4, 0, -3, -5, 0, -1, 2, 0, -1, -1, 0, -1, 0, 0, 0, 1, 0, 0};
static const signed char ctbl32nit_ea[30] = {0, 0, 0, 0, 0, 0, -11, 0, -14, -11, 0, -6, -2, 0, -1, -5, 0, -1, 2, 0, -1, -1, 0, -1, 0, 0, 0, 1, 0, 0};
static const signed char ctbl34nit_ea[30] = {0, 0, 0, 0, 0, 0, -10, 0, -8, -10, 0, -7, 0, 0, 0, -7, 0, -4, 2, 0, 1, -1, 0, -1, 0, 0, 0, 1, 0, 0};
static const signed char ctbl37nit_ea[30] = {0, 0, 0, 0, 0, 0, -12, 0, -12, -6, 0, -4, -2, 0, -7, -6, 0, 4, 2, 0, 0, -1, 0, -1, 0, 0, 0, 1, 0, 0};
static const signed char ctbl39nit_ea[30] = {0, 0, 0, 0, 0, 0, -11, 0, -10, -5, 0, -3, -2, 0, -7, -6, 0, 4, 2, 0, 0, -1, 0, -1, 0, 0, 0, 1, 0, 0};
static const signed char ctbl41nit_ea[30] = {0, 0, 0, 0, 0, 0, -12, 0, -15, -7, 0, -1, -1, 0, -6, -5, 0, 4, 2, 0, 0, -1, 0, -1, 0, 0, 0, 1, 0, 0};
static const signed char ctbl44nit_ea[30] = {0, 0, 0, 0, 0, 0, -13, 0, -11, -6, 0, -1, 1, 0, -1, -4, 0, -1, 2, 0, 0, -1, 0, -1, 0, 0, 0, 1, 0, 0};
static const signed char ctbl47nit_ea[30] = {0, 0, 0, 0, 0, 0, -14, 0, -7, 2, 0, -1, 0, 0, -3, -6, 0, 0, 2, 0, -1, -1, 0, -1, 0, 0, 0, 1, 0, 0};
static const signed char ctbl50nit_ea[30] = {0, 0, 0, 0, 0, 0, -12, 0, -11, -7, 0, -5, 2, 0, -2, -6, 0, 0, 2, 0, -1, -1, 0, -1, 0, 0, 0, 1, 0, 0};
static const signed char ctbl53nit_ea[30] = {0, 0, 0, 0, 0, 0, -12, 0, -8, -5, 0, -5, 3, 0, 0, -6, 0, 0, 2, 0, -1, -1, 0, -1, 0, 0, 0, 1, 0, 0};
static const signed char ctbl56nit_ea[30] = {0, 0, 0, 0, 0, 0, -15, 0, -13, 2, 0, -2, 3, 0, 0, -6, 0, 0, 2, 0, -1, -1, 0, -1, 0, 0, 0, 1, 0, 0};
static const signed char ctbl60nit_ea[30] = {0, 0, 0, 0, 0, 0, -14, 0, -11, 2, 0, -1, 2, 0, 1, -5, 0, -1, 2, 0, -1, -1, 0, -1, 0, 0, 0, 1, 0, 0};
static const signed char ctbl64nit_ea[30] = {0, 0, 0, 0, 0, 0, -10, 0, -4, -2, 0, -4, 0, 0, -3, -6, 0, 1, 2, 0, -1, -1, 0, -1, 0, 0, 0, 1, 0, 0};
static const signed char ctbl68nit_ea[30] = {0, 0, 0, 0, 0, 0, -11, 0, -10, -3, 0, -2, 0, 0, -3, -6, 0, 0, 2, 0, 0, -1, 0, -1, 0, 0, 0, 1, 0, 0};
static const signed char ctbl72nit_ea[30] = {0, 0, 0, 0, 0, 0, -12, 0, -8, 1, 0, 0, -1, 0, -2, -4, 0, 0, 4, 0, 1, -1, 0, -1, 0, 0, 0, 1, 0, 0};
static const signed char ctbl77nit_ea[30] = {0, 0, 0, 0, 0, 0, -13, 0, -8, -3, 0, -4, -1, 0, -2, -3, 0, 1, 2, 0, 1, -1, 0, -2, 0, 0, 0, 1, 0, 1};
static const signed char ctbl82nit_ea[30] = {0, 0, 0, 0, 0, 0, -13, 0, -11, 2, 0, 0, -1, 0, -2, -2, 0, 1, 3, 0, 1, -1, 0, -1, 0, 0, 0, 0, 0, 0};
static const signed char ctbl87nit_ea[30] = {0, 0, 0, 0, 0, 0, -12, 0, -10, 2, 0, 0, 2, 0, 0, -3, 0, 0, 0, 0, 0, -1, 0, -1, 0, 0, 0, 1, 0, 1};
static const signed char ctbl93nit_ea[30] = {0, 0, 0, 0, 0, 0, -11, 0, -7, 0, 0, -1, 1, 0, 1, -3, 0, 0, 2, 0, 0, -2, 0, -2, 0, 0, 0, 1, 0, 0};
static const signed char ctbl98nit_ea[30] = {0, 0, 0, 0, 0, 0, -10, 0, -6, -2, 0, -2, -3, 0, -1, 0, 0, 0, 2, 0, 0, -1, 0, -2, 0, 0, 0, 1, 0, 1};
static const signed char ctbl105nit_ea[30] = {0, 0, 0, 0, 0, 0, -10, 0, -6, -4, 0, -2, -2, 0, 0, 0, 0, 0, 0, 0, -1, 0, 0, -1, 0, 0, 0, 1, 0, 1};
static const signed char ctbl111nit_ea[30] = {0, 0, 0, 0, 0, 0, -10, 0, -6, -5, 0, -3, 3, 0, 0, -1, 0, 5, -1, 0, -3, 1, 0, -1, 0, 0, 1, 1, 0, 1};
static const signed char ctbl119nit_ea[30] = {0, 0, 0, 0, 0, 0, -9, 0, -7, -4, 0, -3, 1, 0, 1, -1, 0, 2, 0, 0, -2, 0, 0, -1, 0, 0, 1, 1, 0, 1};
static const signed char ctbl126nit_ea[30] = {0, 0, 0, 0, 0, 0, -9, 0, -5, -5, 0, -3, 2, 0, 2, -1, 0, 0, -1, 0, -2, 0, 0, -1, 0, 0, 1, 1, 0, 1};
static const signed char ctbl134nit_ea[30] = {0, 0, 0, 0, 0, 0, -10, 0, -5, 1, 0, 0, 2, 0, 1, -2, 0, 0, -1, 0, -2, 0, 0, -1, 0, 0, 1, 1, 0, 1};
static const signed char ctbl143nit_ea[30] = {0, 0, 0, 0, 0, 0, -7, 0, -2, 1, 0, 0, 2, 0, 1, -2, 0, 0, -1, 0, -2, 0, 0, -1, 0, 0, 1, 1, 0, 1};
static const signed char ctbl152nit_ea[30] = {0, 0, 0, 0, 0, 0, -6, 0, -2, -3, 0, -3, 2, 0, 1, -2, 0, 0, -1, 0, -2, 0, 0, -1, 0, 0, 1, 1, 0, 1};
static const signed char ctbl162nit_ea[30] = {0, 0, 0, 0, 0, 0, -6, 0, 0, -2, 0, -5, 2, 0, 0, -2, 0, 1, -1, 0, -2, 0, 0, -1, 0, 0, 1, 1, 0, 1};
static const signed char ctbl172nit_ea[30] = {0, 0, 0, 0, 0, 0, -3, 0, 2, -2, 0, -6, 1, 0, 0, -1, 0, 2, -1, 0, -3, 0, 0, -1, 0, 0, 1, 1, 0, 1};
static const signed char ctbl183nit_ea[30] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static const signed char ctbl195nit_ea[30] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static const signed char ctbl207nit_ea[30] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static const signed char ctbl220nit_ea[30] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static const signed char ctbl234nit_ea[30] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static const signed char ctbl249nit_ea[30] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static const signed char ctbl265nit_ea[30] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static const signed char ctbl282nit_ea[30] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static const signed char ctbl300nit_ea[30] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static const signed char ctbl316nit_ea[30] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static const signed char ctbl333nit_ea[30] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static const signed char ctbl350nit_ea[30] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

static unsigned char elv_ea_8B[6] = {0xB6, 0xCC, 0x8B, 0x11, 0x1E, 0x1E};
static unsigned char elv_ea_8C[6] = {0xB6, 0xCC, 0x8C, 0x00, 0x00, 0x00};
static unsigned char elv_ea_8D[6] = {0xB6, 0xCC, 0x8D, 0x11, 0x1C, 0x1C};
static unsigned char elv_ea_8E[6] = {0xB6, 0xCC, 0x8E, 0x00, 0x00, 0x00};
static unsigned char elv_ea_8F[6] = {0xB6, 0xCC, 0x8F, 0x11, 0x1A, 0x1A};
static unsigned char elv_ea_90[6] = {0xB6, 0xCC, 0x90, 0x00, 0x00, 0x00};
static unsigned char elv_ea_91[6] = {0xB6, 0xCC, 0x91, 0x11, 0x18, 0x18};
static unsigned char elv_ea_92[6] = {0xB6, 0xCC, 0x92, 0x00, 0x00, 0x00};
static unsigned char elv_ea_93[6] = {0xB6, 0xCC, 0x93, 0x11, 0x16, 0x16};
static unsigned char elv_ea_94[6] = {0xB6, 0xCC, 0x94, 0x00, 0x00, 0x00};
static unsigned char elv_ea_95[6] = {0xB6, 0xCC, 0x95, 0x11, 0x14, 0x14};
static unsigned char elv_ea_96[6] = {0xB6, 0xCC, 0x96, 0x00, 0x00, 0x00};
static unsigned char elv_ea_97[6] = {0xB6, 0xCC, 0x97, 0x11, 0x12, 0x12};
static unsigned char elv_ea_98[6] = {0xB6, 0xCC, 0x98, 0x00, 0x00, 0x00};

static const signed char aid5_ea[5] = {0xB2, 0x00, 0x00, 0x03, 0xB7};
static const signed char aid6_ea[5] = {0xB2, 0x00, 0x00, 0x03, 0xB0};
static const signed char aid7_ea[5] = {0xB2, 0x00, 0x00, 0x03, 0xA8};
static const signed char aid8_ea[5] = {0xB2, 0x00, 0x00, 0x03, 0xA1};
static const signed char aid9_ea[5] = {0xB2, 0x00, 0x00, 0x03, 0x9A};
static const signed char aid10_ea[5] = {0xB2, 0x00, 0x00, 0x03, 0x93};
static const signed char aid11_ea[5] = {0xB2, 0x00, 0x00, 0x03, 0x8B};
static const signed char aid12_ea[5] = {0xB2, 0x00, 0x00, 0x03, 0x84};
static const signed char aid13_ea[5] = {0xB2, 0x00, 0x00, 0x03, 0x7D};
static const signed char aid14_ea[5] = {0xB2, 0x00, 0x00, 0x03, 0x75};
static const signed char aid15_ea[5] = {0xB2, 0x00, 0x00, 0x03, 0x6E};
static const signed char aid16_ea[5] = {0xB2, 0x00, 0x00, 0x03, 0x67};
static const signed char aid17_ea[5] = {0xB2, 0x00, 0x00, 0x03, 0x5F};
static const signed char aid19_ea[5] = {0xB2, 0x00, 0x00, 0x03, 0x50};
static const signed char aid20_ea[5] = {0xB2, 0x00, 0x00, 0x03, 0x48};
static const signed char aid21_ea[5] = {0xB2, 0x00, 0x00, 0x03, 0x3F};
static const signed char aid22_ea[5] = {0xB2, 0x00, 0x00, 0x03, 0x36};
static const signed char aid24_ea[5] = {0xB2, 0x00, 0x00, 0x03, 0x24};
static const signed char aid25_ea[5] = {0xB2, 0x00, 0x00, 0x03, 0x1A};
static const signed char aid27_ea[5] = {0xB2, 0x00, 0x00, 0x03, 0x06};
static const signed char aid29_ea[5] = {0xB2, 0x00, 0x00, 0x02, 0xF4};
static const signed char aid30_ea[5] = {0xB2, 0x00, 0x00, 0x02, 0xEA};
static const signed char aid32_ea[5] = {0xB2, 0x00, 0x00, 0x02, 0xD9};
static const signed char aid34_ea[5] = {0xB2, 0x00, 0x00, 0x02, 0xC8};
static const signed char aid37_ea[5] = {0xB2, 0x00, 0x00, 0x02, 0xB0};
static const signed char aid39_ea[5] = {0xB2, 0x00, 0x00, 0x02, 0x9F};
static const signed char aid41_ea[5] = {0xB2, 0x00, 0x00, 0x02, 0x8F};
static const signed char aid44_ea[5] = {0xB2, 0x00, 0x00, 0x02, 0x76};
static const signed char aid47_ea[5] = {0xB2, 0x00, 0x00, 0x02, 0x5D};
static const signed char aid50_ea[5] = {0xB2, 0x00, 0x00, 0x02, 0x45};
static const signed char aid53_ea[5] = {0xB2, 0x00, 0x00, 0x02, 0x2B};
static const signed char aid56_ea[5] = {0xB2, 0x00, 0x00, 0x02, 0x12};
static const signed char aid60_ea[5] = {0xB2, 0x00, 0x00, 0x01, 0xF0};
static const signed char aid64_ea[5] = {0xB2, 0x00, 0x00, 0x01, 0xCE};
static const signed char aid68_ea[5] = {0xB2, 0x00, 0x00, 0x01, 0xAF};
static const signed char aid72_ea[5] = {0xB2, 0x00, 0x00, 0x01, 0x92};
static const signed char aid119_ea[5] = {0xB2, 0x00, 0x00, 0x01, 0x66};
static const signed char aid126_ea[5] = {0xB2, 0x00, 0x00, 0x01, 0x3E};
static const signed char aid134_ea[5] = {0xB2, 0x00, 0x00, 0x01, 0x16};
static const signed char aid143_ea[5] = {0xB2, 0x00, 0x00, 0x00, 0xE3};
static const signed char aid152_ea[5] = {0xB2, 0x00, 0x00, 0x00, 0xB5};
static const signed char aid162_ea[5] = {0xB2, 0x00, 0x00, 0x00, 0x7B};
static const signed char aid172_ea[5] = {0xB2, 0x00, 0x00, 0x00, 0x47};
static const signed char aid183_ea[5] = {0xB2, 0x00, 0x00, 0x00, 0x0A};

#endif
