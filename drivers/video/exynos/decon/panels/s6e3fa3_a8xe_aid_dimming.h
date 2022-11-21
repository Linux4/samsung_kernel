/* linux/drivers/video/exynos/decon/panel/s6e3fa3_a8xe_aid_dimming.h
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

#ifndef __S6E3FA3_A8XE_AID_DIMMING_H__
#define __S6E3FA3_A8XE_AID_DIMMING_H__

static signed char rtbl2nit[10] = {0, 27, 23, 19, 15, 12, 8, 3, 1, 0 };
static signed char rtbl3nit[10] = {0, 24, 21, 16, 13, 11, 11, 5, 2, 0 };
static signed char rtbl4nit[10] = {0, 21, 17, 14, 12, 10, 7, 4, 2, 0 };
static signed char rtbl5nit[10] = {0, 20, 17, 13, 11, 9, 8, 2, 1, 0 };
static signed char rtbl6nit[10] = {0, 20, 15, 12, 9, 9, 6, 2, 0, 0 };
static signed char rtbl7nit[10] = {0, 19, 15, 12, 9, 9, 6, 5, 3, 0 };
static signed char rtbl8nit[10] = {0, 18, 15, 11, 10, 8, 6, 3, 1, 0 };
static signed char rtbl9nit[10] = {0, 18, 15, 11, 9, 8, 5, 2, 1, 0 };
static signed char rtbl10nit[10] = {0, 18, 14, 11, 9, 6, 4, 2, 1, 0 };
static signed char rtbl11nit[10] = {0, 18, 15, 10, 8, 6, 5, 3, 2, 0 };
static signed char rtbl12nit[10] = {0, 18, 14, 10, 8, 6, 5, 3, 3, 0 };
static signed char rtbl13nit[10] = {0, 18, 14, 9, 8, 6, 4, 3, 2, 0 };
static signed char rtbl14nit[10] = {0, 17, 13, 9, 7, 6, 4, 3, 2, 0 };
static signed char rtbl15nit[10] = {0, 16, 13, 9, 7, 5, 4, 3, 2, 0 };
static signed char rtbl16nit[10] = {0, 17, 13, 10, 8, 6, 5, 2, 2, 0 };
static signed char rtbl17nit[10] = {0, 17, 14, 10, 8, 6, 5, 4, 3, 0 };
static signed char rtbl19nit[10] = {0, 18, 14, 10, 9, 7, 4, 3, 3, 0 };
static signed char rtbl20nit[10] = {0, 18, 15, 11, 9, 7, 5, 4, 4, 0 };
static signed char rtbl21nit[10] = {0, 18, 14, 10, 9, 7, 6, 4, 4, 0 };
static signed char rtbl22nit[10] = {0, 17, 14, 11, 9, 7, 5, 4, 3, 0 };
static signed char rtbl24nit[10] = {0, 17, 13, 10, 8, 6, 5, 3, 3, 0 };
static signed char rtbl25nit[10] = {0, 17, 13, 9, 8, 6, 5, 4, 3, 0 };
static signed char rtbl27nit[10] = {0, 16, 12, 8, 7, 5, 4, 3, 3, 0 };
static signed char rtbl29nit[10] = {0, 15, 12, 8, 7, 5, 5, 4, 3, 0 };
static signed char rtbl30nit[10] = {0, 15, 11, 8, 6, 5, 4, 3, 3, 0 };
static signed char rtbl32nit[10] = {0, 14, 11, 8, 6, 5, 4, 3, 3, 0 };
static signed char rtbl34nit[10] = {0, 14, 11, 7, 6, 5, 4, 3, 3, 0 };
static signed char rtbl37nit[10] = {0, 13, 9, 6, 5, 4, 4, 3, 3, 0 };
static signed char rtbl39nit[10] = {0, 12, 9, 6, 5, 4, 3, 3, 3, 0 };
static signed char rtbl41nit[10] = {0, 11, 9, 6, 5, 3, 3, 2, 3, 0 };
static signed char rtbl44nit[10] = {0, 11, 8, 6, 4, 4, 3, 2, 3, 0 };
static signed char rtbl47nit[10] = {0, 10, 8, 6, 4, 3, 4, 4, 4, 0 };
static signed char rtbl50nit[10] = {0, 10, 7, 5, 3, 3, 3, 4, 4, 0 };
static signed char rtbl53nit[10] = {0, 9, 7, 5, 3, 3, 2, 3, 4, 0 };
static signed char rtbl56nit[10] = {0, 9, 6, 4, 3, 3, 3, 3, 2, 0 };
static signed char rtbl60nit[10] = {0, 8, 6, 4, 3, 3, 3, 3, 3, 0 };
static signed char rtbl64nit[10] = {0, 8, 6, 4, 3, 2, 2, 2, 3, 0 };
static signed char rtbl68nit[10] = {0, 9, 5, 4, 2, 2, 2, 2, 3, 0 };
static signed char rtbl72nit[10] = {0, 7, 5, 3, 3, 1, 2, 3, 3, 0 };
static signed char rtbl77nit[10] = {0, 7, 5, 4, 3, 3, 3, 2, 2, 0 };
static signed char rtbl82nit[10] = {0, 6, 5, 4, 2, 2, 1, 1, 2, 0 };
static signed char rtbl87nit[10] = {0, 6, 5, 3, 2, 2, 2, 2, 2, 0 };
static signed char rtbl93nit[10] = {0, 6, 5, 3, 2, 2, 3, 3, 3, 0 };
static signed char rtbl98nit[10] = {0, 7, 4, 3, 3, 2, 2, 2, 4, 0 };
static signed char rtbl105nit[10] = {0, 6, 5, 3, 3, 2, 2, 4, 3, 0 };
static signed char rtbl111nit[10] = {0, 6, 4, 3, 2, 2, 3, 4, 3, 0 };
static signed char rtbl119nit[10] = {0, 6, 4, 3, 3, 2, 3, 3, 3, 0 };
static signed char rtbl126nit[10] = {0, 6, 4, 3, 2, 2, 2, 4, 3, 0 };
static signed char rtbl134nit[10] = {0, 6, 3, 2, 2, 2, 3, 4, 3, 0 };
static signed char rtbl143nit[10] = {0, 6, 3, 3, 2, 2, 3, 4, 3, 0 };
static signed char rtbl152nit[10] = {0, 6, 4, 2, 3, 3, 4, 4, 3, 0 };
static signed char rtbl162nit[10] = {0, 4, 4, 3, 2, 3, 4, 5, 3, 0 };
static signed char rtbl172nit[10] = {0, 4, 3, 2, 2, 2, 4, 4, 3, 0 };
static signed char rtbl183nit[10] = {0, 4, 3, 3, 3, 3, 4, 5, 3, 0 };
static signed char rtbl195nit[10] = {0, 4, 3, 2, 2, 2, 3, 3, 3, 0 };
static signed char rtbl207nit[10] = {0, 3, 2, 2, 2, 2, 3, 4, 3, 0 };
static signed char rtbl220nit[10] = {0, 3, 2, 2, 2, 2, 2, 4, 3, 0 };
static signed char rtbl234nit[10] = {0, 3, 2, 2, 2, 1, 2, 3, 2, 0 };
static signed char rtbl249nit[10] = {0, 2, 1, 2, 1, 1, 3, 4, 2, 0 };
static signed char rtbl265nit[10] = {0, 3, 2, 1, 2, 1, 2, 3, 2, 0 };
static signed char rtbl282nit[10] = {0, 2, 1, 1, 1, 1, 2, 4, 1, 0 };
static signed char rtbl300nit[10] = {0, 2, 1, 1, 1, 0, 1, 2, 1, 0 };
static signed char rtbl316nit[10] = {0, 1, 1, 1, 0, 0, 1, 2, 1, 0 };
static signed char rtbl333nit[10] = {0, 1, 1, 0, 0, 0, 2, 2, 1, 0 };
static signed char rtbl350nit[10] = {0, 1, 1, 1, 1, 1, 1, 2, 1, 0 };
static signed char rtbl357nit[10] = {0, 1, 1, 0, 0, 0, 0, 0, 0, 0 };
static signed char rtbl365nit[10] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
static signed char rtbl372nit[10] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
static signed char rtbl380nit[10] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
static signed char rtbl387nit[10] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
static signed char rtbl395nit[10] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
static signed char rtbl403nit[10] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
static signed char rtbl412nit[10] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
static signed char rtbl420nit[10] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

static signed char ctbl2nit[30] = { 0, 0, 0, 0, 0, 0, -11, 0, -5, -6, 0, -2, -13, 0, -1, -14, 0, 0, -11, 0, -5, -2, 0, -1, -3, 0, -2, -3, 0, 1};
static signed char ctbl3nit[30] = { 0, 0, 0, 0, 0, 0, -7, 0, 0, -13, 0, -3, -12, 0, 1, -12, 0, -1, -9, 0, -5, -2, 0, -1, -2, 0, -2, -2, 0, 1};
static signed char ctbl4nit[30] = { 0, 0, 0, 0, 0, 0, -23, 0, -10, -10, 0, -1, -10, 0, 0, -9, 0, -2, -8, 2, -4, -1, 0, -1, -2, 0, -2, -1, 0, 2};
static signed char ctbl5nit[30] = { 0, 0, 0, 0, 0, 0, -19, 0, -6, -9, 0, -2, -10, 0, 1, -9, 0, -3, -7, 0, -4, -2, 0, -1, -1, 0, -2, -1, 0, 2};
static signed char ctbl6nit[30] = { 0, 0, 0, 0, 0, 0, -16, 0, -5, -9, 0, -3, -12, 0, -1, -8, 0, -4, -6, 0, -4, -1, 0, 0, -1, 0, -2, -1, 0, 2};
static signed char ctbl7nit[30] = { 0, 0, 0, 0, 0, 0, -20, 0, -12, -8, 0, -3, -11, 0, -2, -7, 0, -3, -6, 0, -4, 0, 0, 0, -2, 0, -2, 0, 0, 2};
static signed char ctbl8nit[30] = { 0, 0, 0, 0, 0, 0, -12, 0, -5, -12, 0, -5, -8, 0, -2, -6, 0, -3, -6, 0, -4, -1, 0, -1, -1, 0, -1, 0, 0, 2};
static signed char ctbl9nit[30] = { 0, 0, 0, 0, 0, 0, -12, 0, -6, -11, 0, -4, -9, 0, -4, -7, 0, -4, -7, 0, -5, 0, 0, 0, -1, 0, -1, 0, 0, 2};
static signed char ctbl10nit[30] = { 0, 0, 0, 0, 0, 0, -23, 0, -14, -9, 0, -3, -8, 0, -3, -7, 0, -4, -5, 0, -4, 0, 0, 0, -1, 0, -1, 0, 0, 2};
static signed char ctbl11nit[30] = { 0, 0, 0, 0, 0, 0, -12, 0, -6, -11, 0, -7, -8, 0, -4, -7, 0, -4, -6, 0, -5, 0, 0, 0, -1, 0, -1, 0, 0, 2};
static signed char ctbl12nit[30] = { 0, 0, 0, 0, 0, 0, -14, 0, -10, -9, 0, -6, -9, 0, -5, -6, 0, -4, -5, 0, -4, 0, 0, 0, -1, 0, -1, 0, 0, 2};
static signed char ctbl13nit[30] = { 0, 0, 0, 0, 0, 0, -11, 0, -7, -11, 0, -7, -7, 0, -4, -6, 0, -4, -5, 0, -4, 0, 0, 0, -1, 0, -1, 0, 0, 2};
static signed char ctbl14nit[30] = { 0, 0, 0, 0, 0, 0, -13, 0, -9, -10, 0, -6, -10, 0, -6, -5, 0, -3, -5, 0, -4, 1, 0, 0, -1, 0, -1, 0, 0, 2};
static signed char ctbl15nit[30] = { 0, 0, 0, 0, 0, 0, -12, 0, -7, -8, 0, -5, -7, 0, -4, -7, 0, -5, -4, 0, -3, 1, 0, 0, -1, 0, -1, 0, 0, 2};
static signed char ctbl16nit[30] = { 0, 0, 0, 0, 0, 0, -15, 0, -12, -7, 0, -5, -8, 0, -5, -5, 0, -4, -5, 0, -4, 0, 0, 0, 0, 0, -1, 0, 0, 2};
static signed char ctbl17nit[30] = { 0, 0, 0, 0, 0, 0, -10, 0, -9, -9, 0, -6, -8, 0, -6, -6, 0, -4, -4, 0, -4, 0, 0, 0, 0, 0, -1, 0, 0, 2};
static signed char ctbl19nit[30] = { 0, 0, 0, 0, 0, 0, -12, 0, -12, -11, 0, -8, -7, 0, -6, -6, 0, -4, -4, 0, -4, 0, 0, 0, -1, 0, -1, 0, 0, 2};
static signed char ctbl20nit[30] = { 0, 0, 0, 0, 0, 0, -9, 0, -9, -9, 0, -8, -7, 0, -6, -6, 0, -6, -4, 0, -4, 0, 0, 0, -1, 0, -1, 0, 0, 1};
static signed char ctbl21nit[30] = { 0, 0, 0, 0, 0, 0, -15, 0, -15, -11, 0, -10, -7, 0, -6, -6, 0, -5, -3, 0, -3, 1, 0, 0, -1, 0, -1, 0, 0, 1};
static signed char ctbl22nit[30] = { 0, 0, 0, 0, 0, 0, -13, 0, -12, -8, 0, -7, -7, 0, -6, -5, 0, -4, -4, 0, -4, 1, 0, 0, -1, 0, -1, 0, 0, 1};
static signed char ctbl24nit[30] = { 0, 0, 0, 0, 0, 0, -13, 0, -14, -8, 0, -8, -7, 0, -5, -4, 0, -4, -4, 0, -4, 1, 0, 0, -1, 0, -1, 0, 0, 1};
static signed char ctbl25nit[30] = { 0, 0, 0, 0, 0, 0, -11, 0, -12, -8, 0, -8, -7, 0, -5, -4, 0, -4, -4, 0, -4, 1, 0, 0, -1, 0, -1, 0, 0, 1};
static signed char ctbl27nit[30] = { 0, 0, 0, 0, 0, 0, -15, 0, -16, -9, 0, -9, -7, 0, -5, -5, 0, -5, -3, 0, -3, 1, 0, 0, -1, 0, -1, 0, 0, 1};
static signed char ctbl29nit[30] = { 0, 0, 0, 0, 0, 0, -9, 0, -10, -8, 0, -8, -6, 0, -4, -5, 0, -5, -3, 0, -3, 1, 0, 0, -1, 0, -1, 0, 0, 1};
static signed char ctbl30nit[30] = { 0, 0, 0, 0, 0, 0, -11, 2, -12, -7, 0, -7, -8, 0, -6, -4, 0, -4, -3, 0, -3, 1, 0, 0, -1, 0, -1, 0, 0, 1};
static signed char ctbl32nit[30] = { 0, 0, 0, 0, 0, 0, -13, 0, -13, -6, 0, -7, -7, 0, -5, -4, 0, -4, -3, 0, -3, 1, 0, 0, -1, 0, -1, 0, 0, 1};
static signed char ctbl34nit[30] = { 0, 0, 0, 0, 0, 0, -11, 0, -10, -7, 0, -8, -7, 0, -5, -4, 0, -4, -2, 0, -2, 1, 0, 0, -1, 0, -1, 0, 0, 1};
static signed char ctbl37nit[30] = { 0, 0, 0, 0, 0, 0, -14, 0, -16, -9, 0, -9, -7, 0, -4, -3, 0, -3, -2, 0, -3, 1, 0, 1, -1, 0, -1, 0, 0, 1};
static signed char ctbl39nit[30] = { 0, 0, 0, 0, 0, 0, -13, 0, -14, -6, 0, -7, -7, 0, -4, -3, 0, -3, -3, 0, -3, 2, 0, 1, -1, 0, -1, 0, 0, 1};
static signed char ctbl41nit[30] = { 0, 0, 0, 0, 0, 0, -12, 0, -11, -6, 0, -7, -5, 0, -2, -3, 0, -3, -3, 0, -3, 2, 0, 1, -1, 0, -1, 0, 0, 1};
static signed char ctbl44nit[30] = { 0, 0, 0, 0, 0, 0, -14, 0, -16, -6, 0, -7, -3, 0, -1, -5, 0, -5, -3, 0, -3, 2, 0, 1, -1, 0, -1, 0, 0, 1};
static signed char ctbl47nit[30] = { 0, 0, 0, 0, 0, 0, -9, 2, -10, -6, 0, -6, -4, 0, -3, -2, 0, -3, -3, 0, -3, 2, 0, 1, -1, 0, -1, 0, 0, 1};
static signed char ctbl50nit[30] = { 0, 0, 0, 0, 0, 0, -16, 0, -16, -5, 0, -6, -5, 0, -3, -2, 0, -2, -2, 0, -2, 2, 0, 1, -1, 0, -1, 0, 0, 1};
static signed char ctbl53nit[30] = { 0, 0, 0, 0, 0, 0, -14, 0, -14, -4, 0, -5, -6, 0, -3, -1, 0, -2, -2, 0, -2, 2, 0, 1, -1, 0, -1, 0, 0, 1};
static signed char ctbl56nit[30] = { 0, 0, 0, 0, 0, 0, -14, 0, -15, -5, 0, -6, -5, 0, -2, -1, 0, -2, -2, 0, -2, 2, 0, 1, -1, 0, -1, 0, 0, 1};
static signed char ctbl60nit[30] = { 0, 0, 0, 0, 0, 0, -13, 0, -13, -4, 0, -5, -5, 0, -2, -1, 0, -2, -2, 0, -2, 2, 0, 1, -1, 0, -1, 0, 0, 1};
static signed char ctbl64nit[30] = { 0, 0, 0, 0, 0, 0, -12, 0, -11, -4, 0, -4, -4, 0, -3, -1, 0, -2, -2, 0, -2, 2, 0, 1, -1, 0, -1, 0, 0, 1};
static signed char ctbl68nit[30] = { 0, 0, 0, 0, 0, 0, -15, 0, -15, -2, 0, -4, -5, 0, -3, -1, 0, -2, -2, 0, -2, 1, 0, 1, 0, 0, -1, 0, 0, 1};
static signed char ctbl72nit[30] = { 0, 0, 0, 0, 0, 0, -12, 0, -12, -5, 0, -6, -3, 0, -2, -1, 0, -3, -1, 0, -1, 1, 0, 1, 0, 0, -1, 0, 0, 1};
static signed char ctbl77nit[30] = { 0, 0, 0, 0, 0, 0, -11, 0, -10, -3, 0, -4, -3, 0, -1, -1, 0, -2, -1, 0, -2, 1, 0, 1, -2, 0, -2, 0, 0, 1};
static signed char ctbl82nit[30] = { 0, 0, 0, 0, 0, 0, -12, 0, -11, -2, 0, -3, -4, 0, -2, 0, 0, -2, -1, 0, -1, 1, 0, 0, -1, 0, -1, 0, 0, 1};
static signed char ctbl87nit[30] = { 0, 0, 0, 0, 0, 0, -11, 0, -9, -2, 0, -3, -3, 0, -2, 0, 0, -2, -1, 0, -1, 3, 0, 2, -1, 0, -1, 0, 0, 1};
static signed char ctbl93nit[30] = { 0, 0, 0, 0, 0, 0, -11, 0, -9, -3, 0, -4, -3, 0, -1, 0, 0, 0, -1, 0, -2, 0, 0, 0, 0, 0, -1, -1, 0, 0};
static signed char ctbl98nit[30] = { 0, 0, 0, 0, 0, 0, -12, 0, -11, -2, 0, -3, -4, 0, -3, 0, 0, -1, -1, 0, -1, 1, 0, 0, -2, 0, -2, 0, 0, 1};
static signed char ctbl105nit[30] = { 0, 0, 0, 0, 0, 0, -8, 0, -7, -4, 0, -3, -3, 0, -2, 0, 0, -1, 0, 0, -1, 0, 0, 0, 0, 0, -1, 0, 0, 1};
static signed char ctbl111nit[30] = { 0, 0, 0, 0, 0, 0, -11, 0, -9, -2, 0, -3, -2, 0, -1, 0, 0, -1, -1, 0, -1, 1, 0, 1, -1, 0, -2, 0, 0, 1};
static signed char ctbl119nit[30] = { 0, 0, 0, 0, 0, 0, -12, 0, -10, -3, 0, -3, -2, 0, -1, 0, 0, -1, 0, 0, -1, 2, 0, 2, -2, 0, -2, 0, 0, 1};
static signed char ctbl126nit[30] = { 0, 0, 0, 0, 0, 0, -12, 0, -10, -2, 0, -3, -2, 0, -1, 0, 0, 0, 0, 0, -1, 0, 0, 0, 0, 0, -1, 0, 0, 1};
static signed char ctbl134nit[30] = { 0, 0, 0, 0, 0, 0, -11, 0, -8, -3, 0, -4, -1, 0, 0, 0, 0, -1, -1, 0, 0, 1, 0, 0, -2, 0, -2, 1, 0, 2};
static signed char ctbl143nit[30] = { 0, 0, 0, 0, 0, 0, -11, 0, -9, -2, 0, -3, -1, 0, -1, 0, 0, 0, -1, 0, -1, 0, 0, 0, 0, 0, -1, 0, 0, 1};
static signed char ctbl152nit[30] = { 0, 0, 0, 0, 0, 0, -10, 0, -7, -3, 0, -4, 0, 0, 0, -1, 0, -1, -1, 0, 0, 1, 0, 0, -2, 0, -2, 1, 0, 1};
static signed char ctbl162nit[30] = { 0, 0, 0, 0, 0, 0, -9, 0, -6, -2, 0, -2, 0, 0, -1, -1, 0, 0, 0, 0, -1, 0, 0, 0, 0, 0, -1, 0, 0, 2};
static signed char ctbl172nit[30] = { 0, 0, 0, 0, 0, 0, -8, 0, -6, -2, 0, -2, 0, 0, 0, 0, 0, -1, -1, 0, 0, 1, 0, 0, -1, 0, -1, 1, 0, 2};
static signed char ctbl183nit[30] = { 0, 0, 0, 0, 0, 0, -8, 0, -6, -3, 0, -2, 1, 0, 0, -2, 0, -1, 1, 0, 0, 0, 0, 0, -1, 0, -2, 1, 0, 2};
static signed char ctbl195nit[30] = { 0, 0, 0, 0, 0, 0, -7, 0, -6, -2, 0, -1, 0, 0, 0, -1, 0, -1, -1, 0, -1, 1, 0, 1, -1, 0, -2, 1, 0, 2};
static signed char ctbl207nit[30] = { 0, 0, 0, 0, 0, 0, -8, 0, -7, -1, 0, -1, 0, 0, 0, -1, 0, -1, 0, 0, 0, 1, 0, 1, -1, 0, -2, 1, 0, 2};
static signed char ctbl220nit[30] = { 0, 0, 0, 0, 0, 0, -7, 0, -6, -1, 0, -1, 0, 0, 0, -1, 0, -1, 0, 0, 0, 1, 0, 1, -2, 0, -2, 1, 0, 1};
static signed char ctbl234nit[30] = { 0, 0, 0, 0, 0, 0, -5, 0, -5, -2, 0, -2, 1, 0, 1, 0, 0, -1, 0, 0, 0, 1, 0, 0, -1, 0, -1, 1, 0, 1};
static signed char ctbl249nit[30] = { 0, 0, 0, 0, 0, 0, -6, 0, -5, -1, 0, -1, 1, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -1, 1, 0, 2};
static signed char ctbl265nit[30] = { 0, 0, 0, 0, 0, 0, -5, 0, -5, 0, 0, -1, 0, 0, 0, 0, 0, 0, -1, 0, -1, 1, 0, 1, -1, 0, -1, 1, 0, 1};
static signed char ctbl282nit[30] = { 0, 0, 0, 0, 0, 0, -5, 0, -4, -1, 0, -1, 0, 0, -1, 0, 0, 0, 0, 0, 0, 1, 0, 0, -1, 0, -1, 0, 0, 1};
static signed char ctbl300nit[30] = { 0, 0, 0, 0, 0, 0, -5, 0, -3, -1, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -1, 0, 0, 1};
static signed char ctbl316nit[30] = { 0, 0, 0, 0, 0, 0, -3, 0, -2, -2, 0, -1, 1, 0, 0, 0, 0, 0, -1, 0, 0, 2, 0, 1, -1, 0, -1, 0, 0, 1};
static signed char ctbl333nit[30] = { 0, 0, 0, 0, 0, 0, -4, 0, -2, 0, 0, -1, 1, 0, 1, -1, 0, -1, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1};
static signed char ctbl350nit[30] = { 0, 0, 0, 0, 0, 0, -3, 0, -3, -1, 0, -1, 1, 0, 1, -1, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static signed char ctbl357nit[30] = { 0, 0, 0, 0, 0, 0, -2, 0, -2, -1, 0, -1, 0, 0, 0, 0, 0, 0, -1, 0, 0, 0, 0, -1, 0, 0, 0, 1, 0, 2};
static signed char ctbl365nit[30] = { 0, 0, 0, 0, 0, 0, -3, 0, -2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static signed char ctbl372nit[30] = { 0, 0, 0, 0, 0, 0, -3, 0, -2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static signed char ctbl380nit[30] = { 0, 0, 0, 0, 0, 0, -3, 0, -2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static signed char ctbl387nit[30] = { 0, 0, 0, 0, 0, 0, -1, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static signed char ctbl395nit[30] = { 0, 0, 0, 0, 0, 0, -1, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static signed char ctbl403nit[30] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static signed char ctbl412nit[30] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static signed char ctbl420nit[30] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

static unsigned char elvss_2nit[] = { 0xB6, 0xAC, 0x0E };
static unsigned char elvss_3nit[] = { 0xB6, 0xAC, 0x0E };
static unsigned char elvss_4nit[] = { 0xB6, 0xAC, 0x0E };
static unsigned char elvss_5nit[] = { 0xB6, 0xAC, 0x0E };
static unsigned char elvss_6nit[] = { 0xB6, 0xAC, 0x0E };
static unsigned char elvss_7nit[] = { 0xB6, 0xAC, 0x0E };
static unsigned char elvss_8nit[] = { 0xB6, 0xAC, 0x0E };
static unsigned char elvss_9nit[] = { 0xB6, 0xAC, 0x0E };
static unsigned char elvss_10nit[] = { 0xB6, 0xAC, 0x0E };
static unsigned char elvss_11nit[] = { 0xB6, 0xAC, 0x0E };
static unsigned char elvss_12nit[] = { 0xB6, 0xAC, 0x0E };
static unsigned char elvss_13nit[] = { 0xB6, 0xAC, 0x0E };
static unsigned char elvss_14nit[] = { 0xB6, 0xAC, 0x0E };
static unsigned char elvss_15nit[] = { 0xB6, 0xAC, 0x0E };
static unsigned char elvss_16nit[] = { 0xB6, 0xAC, 0x10 };
static unsigned char elvss_17nit[] = { 0xB6, 0xAC, 0x12 };
static unsigned char elvss_19nit[] = { 0xB6, 0xAC, 0x15 };
static unsigned char elvss_20nit[] = { 0xB6, 0xAC, 0x18 };
static unsigned char elvss_21nit[] = { 0xB6, 0xAC, 0x18 };
static unsigned char elvss_22nit[] = { 0xB6, 0xAC, 0x18 };
static unsigned char elvss_24nit[] = { 0xB6, 0xAC, 0x18 };
static unsigned char elvss_25nit[] = { 0xB6, 0xAC, 0x18 };
static unsigned char elvss_27nit[] = { 0xB6, 0xAC, 0x18 };
static unsigned char elvss_29nit[] = { 0xB6, 0xAC, 0x18 };
static unsigned char elvss_30nit[] = { 0xB6, 0xAC, 0x18 };
static unsigned char elvss_32nit[] = { 0xB6, 0xAC, 0x18 };
static unsigned char elvss_34nit[] = { 0xB6, 0xAC, 0x18 };
static unsigned char elvss_37nit[] = { 0xB6, 0xAC, 0x17 };
static unsigned char elvss_39nit[] = { 0xB6, 0xAC, 0x17 };
static unsigned char elvss_41nit[] = { 0xB6, 0xBC, 0x17 };
static unsigned char elvss_44nit[] = { 0xB6, 0xBC, 0x17 };
static unsigned char elvss_47nit[] = { 0xB6, 0xBC, 0x17 };
static unsigned char elvss_50nit[] = { 0xB6, 0xBC, 0x17 };
static unsigned char elvss_53nit[] = { 0xB6, 0xBC, 0x17 };
static unsigned char elvss_56nit[] = { 0xB6, 0xBC, 0x17 };
static unsigned char elvss_60nit[] = { 0xB6, 0xBC, 0x17 };
static unsigned char elvss_64nit[] = { 0xB6, 0xBC, 0x17 };
static unsigned char elvss_68nit[] = { 0xB6, 0xBC, 0x17 };
static unsigned char elvss_72nit[] = { 0xB6, 0xBC, 0x17 };
static unsigned char elvss_77nit[] = { 0xB6, 0xBC, 0x16 };
static unsigned char elvss_82nit[] = { 0xB6, 0xBC, 0x16 };
static unsigned char elvss_87nit[] = { 0xB6, 0xBC, 0x15 };
static unsigned char elvss_93nit[] = { 0xB6, 0xBC, 0x15 };
static unsigned char elvss_98nit[] = { 0xB6, 0xBC, 0x15 };
static unsigned char elvss_105nit[] = { 0xB6, 0xBC, 0x15 };
static unsigned char elvss_111nit[] = { 0xB6, 0xBC, 0x14 };
static unsigned char elvss_119nit[] = { 0xB6, 0xBC, 0x14 };
static unsigned char elvss_126nit[] = { 0xB6, 0xBC, 0x14 };
static unsigned char elvss_134nit[] = { 0xB6, 0xBC, 0x13 };
static unsigned char elvss_143nit[] = { 0xB6, 0xBC, 0x13 };
static unsigned char elvss_152nit[] = { 0xB6, 0xBC, 0x13 };
static unsigned char elvss_162nit[] = { 0xB6, 0xBC, 0x12 };
static unsigned char elvss_172nit[] = { 0xB6, 0xBC, 0x12 };
static unsigned char elvss_183nit[] = { 0xB6, 0xBC, 0x12 };
static unsigned char elvss_195nit[] = { 0xB6, 0xBC, 0x12 };
static unsigned char elvss_207nit[] = { 0xB6, 0xBC, 0x12 };
static unsigned char elvss_220nit[] = { 0xB6, 0xBC, 0x12 };
static unsigned char elvss_234nit[] = { 0xB6, 0xBC, 0x12 };
static unsigned char elvss_249nit[] = { 0xB6, 0xBC, 0x12 };
static unsigned char elvss_265nit[] = { 0xB6, 0xBC, 0x11 };
static unsigned char elvss_282nit[] = { 0xB6, 0xBC, 0x10 };
static unsigned char elvss_300nit[] = { 0xB6, 0xBC, 0x0F };
static unsigned char elvss_316nit[] = { 0xB6, 0xBC, 0x0E };
static unsigned char elvss_333nit[] = { 0xB6, 0xBC, 0x0E };
static unsigned char elvss_350nit[] = { 0xB6, 0xBC, 0x0D };
static unsigned char elvss_357nit[] = { 0xB6, 0xBC, 0x0C };
static unsigned char elvss_365nit[] = { 0xB6, 0xBC, 0x0C };
static unsigned char elvss_372nit[] = { 0xB6, 0xBC, 0x0C };
static unsigned char elvss_380nit[] = { 0xB6, 0xBC, 0x0C };
static unsigned char elvss_387nit[] = { 0xB6, 0xBC, 0x0B };
static unsigned char elvss_395nit[] = { 0xB6, 0xBC, 0x0B };
static unsigned char elvss_403nit[] = { 0xB6, 0xBC, 0x0B };
static unsigned char elvss_412nit[] = { 0xB6, 0xBC, 0x0A };
static unsigned char elvss_420nit[] = { 0xB6, 0xBC, 0x0A };
static unsigned char elvss_443nit[] = { 0xB6, 0xBC, 0x12 };
static unsigned char elvss_465nit[] = { 0xB6, 0xBC, 0x11 };
static unsigned char elvss_488nit[] = { 0xB6, 0xBC, 0x0F };
static unsigned char elvss_510nit[] = { 0xB6, 0xBC, 0x0E };
static unsigned char elvss_533nit[] = { 0xB6, 0xBC, 0x0C };
static unsigned char elvss_555nit[] = { 0xB6, 0xBC, 0x0B };
static unsigned char elvss_578nit[] = { 0xB6, 0xBC, 0x0B };
static unsigned char elvss_600nit[] = { 0xB6, 0xBC, 0x0A };

static unsigned char aor_table[366 * 2] = {
	0x70, 0x7B,
	0x70, 0x79,
	0x70, 0x6F,
	0x70, 0x6D,
	0x70, 0x6C,
	0x70, 0x6B,
	0x70, 0x69,
	0x70, 0x5F,
	0x70, 0x5D,
	0x70, 0x5B,
	0x70, 0x59,
	0x70, 0x53,
	0x70, 0x4F,
	0x70, 0x4D,
	0x70, 0x4B,
	0x70, 0x49,
	0x70, 0x3F,
	0x70, 0x3D,
	0x70, 0x3B,
	0x70, 0x3A,
	0x70, 0x39,
	0x70, 0x2F,
	0x70, 0x2D,
	0x70, 0x2C,
	0x70, 0x2B,
	0x70, 0x29,
	0x70, 0x1F,
	0x70, 0x1D,
	0x70, 0x1C,
	0x70, 0x1B,
	0x70, 0x19,
	0x70, 0x0F,
	0x70, 0x0D,
	0x70, 0x0B,
	0x70, 0x09,
	0x70, 0x08,
	0x70, 0x07,
	0x60, 0xFF,
	0x60, 0xFD,
	0x60, 0xFB,
	0x60, 0xF9,
	0x60, 0xEF,
	0x60, 0xED,
	0x60, 0xEB,
	0x60, 0xE9,
	0x60, 0xDD,
	0x60, 0xDA,
	0x60, 0xD2,
	0x60, 0xCB,
	0x60, 0xC4,
	0x60, 0xBC,
	0x60, 0xB3,
	0x60, 0xAB,
	0x60, 0x9B,
	0x60, 0x8B,
	0x60, 0x82,
	0x60, 0x78,
	0x60, 0x71,
	0x60, 0x69,
	0x60, 0x62,
	0x60, 0x5B,
	0x60, 0x4B,
	0x60, 0x3A,
	0x60, 0x33,
	0x60, 0x2C,
	0x60, 0x1D,
	0x60, 0x0E,
	0x60, 0x02,
	0x50, 0xF6,
	0x50, 0xEA,
	0x50, 0xDE,
	0x50, 0xD3,
	0x50, 0xC8,
	0x50, 0xBA,
	0x50, 0xAB,
	0x50, 0x95,
	0x50, 0x7E,
	0x50, 0x6E,
	0x50, 0x5E,
	0x50, 0x4D,
	0x50, 0x3C,
	0x50, 0x25,
	0x50, 0x0F,
	0x40, 0xF7,
	0x40, 0xDF,
	0x40, 0xC6,
	0x40, 0xAD,
	0x40, 0x9B,
	0x40, 0x8A,
	0x40, 0x6F,
	0x40, 0x54,
	0x40, 0x36,
	0x40, 0x18,
	0x30, 0xF7,
	0x30, 0xD6,
	0x30, 0xB6,
	0x30, 0x96,
	0x30, 0x76,
	0x30, 0x55,
	0x30, 0x64,
	0x30, 0x40,
	0x30, 0x62,
	0x30, 0x40,
	0x30, 0x60,
	0x30, 0x40,
	0x30, 0x64,
	0x30, 0x40,
	0x30, 0x5C,
	0x30, 0x40,
	0x30, 0x65,
	0x30, 0x40,
	0x30, 0x5E,
	0x30, 0x40,
	0x30, 0x65,
	0x30, 0x40,
	0x30, 0x5F,
	0x30, 0x40,
	0x30, 0x61,
	0x30, 0x40,
	0x30, 0x63,
	0x30, 0x40,
	0x30, 0x61,
	0x30, 0x40,
	0x30, 0x62,
	0x30, 0x40,
	0x30, 0x60,
	0x30, 0x40,
	0x30, 0x61,
	0x30, 0x40,
	0x30, 0x34,
	0x30, 0x29,
	0x30, 0x1D,
	0x30, 0x12,
	0x30, 0x06,
	0x20, 0xFB,
	0x20, 0xEF,
	0x20, 0xE3,
	0x20, 0xD7,
	0x20, 0xCB,
	0x20, 0xC0,
	0x20, 0xB4,
	0x20, 0xA8,
	0x20, 0x9C,
	0x20, 0x8E,
	0x20, 0x80,
	0x20, 0x72,
	0x20, 0x64,
	0x20, 0x56,
	0x20, 0x48,
	0x20, 0x3A,
	0x20, 0x2C,
	0x20, 0x1E,
	0x20, 0x11,
	0x20, 0x03,
	0x10, 0xF5,
	0x10, 0xE7,
	0x10, 0xDA,
	0x10, 0xCC,
	0x10, 0xE1,
	0x10, 0xD5,
	0x10, 0xCA,
	0x10, 0xBE,
	0x10, 0xB3,
	0x10, 0xA7,
	0x10, 0x9C,
	0x10, 0x90,
	0x10, 0xE2,
	0x10, 0xD8,
	0x10, 0xCE,
	0x10, 0xC4,
	0x10, 0xB9,
	0x10, 0xAF,
	0x10, 0xA5,
	0x10, 0x9A,
	0x10, 0x90,
	0x10, 0xE2,
	0x10, 0xD8,
	0x10, 0xCE,
	0x10, 0xC3,
	0x10, 0xB9,
	0x10, 0xAF,
	0x10, 0xA5,
	0x10, 0x9A,
	0x10, 0x90,
	0x10, 0xE3,
	0x10, 0xDA,
	0x10, 0xD1,
	0x10, 0xC7,
	0x10, 0xBE,
	0x10, 0xB5,
	0x10, 0xAC,
	0x10, 0xA2,
	0x10, 0x99,
	0x10, 0x90,
	0x10, 0xD4,
	0x10, 0xCA,
	0x10, 0xC1,
	0x10, 0xB7,
	0x10, 0xAD,
	0x10, 0xA3,
	0x10, 0x9A,
	0x10, 0x90,
	0x10, 0xD6,
	0x10, 0xCD,
	0x10, 0xC4,
	0x10, 0xBC,
	0x10, 0xB3,
	0x10, 0xAA,
	0x10, 0xA1,
	0x10, 0x99,
	0x10, 0x90,
	0x10, 0xD2,
	0x10, 0xCA,
	0x10, 0xC2,
	0x10, 0xB9,
	0x10, 0xB1,
	0x10, 0xA9,
	0x10, 0xA1,
	0x10, 0x98,
	0x10, 0x90,
	0x10, 0xA7,
	0x10, 0x9F,
	0x10, 0x98,
	0x10, 0x90,
	0x10, 0xA9,
	0x10, 0xA1,
	0x10, 0x98,
	0x10, 0x90,
	0x10, 0x84,
	0x10, 0x79,
	0x10, 0x6D,
	0x10, 0x61,
	0x10, 0x56,
	0x10, 0x4A,
	0x10, 0x3E,
	0x10, 0x36,
	0x10, 0x2D,
	0x10, 0x25,
	0x10, 0x1D,
	0x10, 0x11,
	0x10, 0x06,
	0x00, 0xFA,
	0x00, 0xEE,
	0x00, 0xDC,
	0x00, 0xD3,
	0x00, 0xCB,
	0x00, 0xC2,
	0x00, 0xDF,
	0x00, 0xD5,
	0x00, 0xCC,
	0x00, 0xC2,
	0x00, 0xDB,
	0x00, 0xD3,
	0x00, 0xCA,
	0x00, 0xC2,
	0x00, 0xC2,
	0x00, 0xC2,
	0x00, 0xC2,
	0x00, 0xC2,
	0x00, 0xC2,
	0x00, 0xC2,
	0x00, 0xC2,
	0x00, 0xC2,
	0x00, 0xC2,
	0x00, 0xC2,
	0x00, 0xC2,
	0x00, 0xC2,
	0x00, 0xC2,
	0x00, 0xC2,
	0x00, 0xC2,
	0x00, 0xC2,
	0x00, 0xC2,
	0x00, 0xC2,
	0x00, 0xC2,
	0x00, 0xC2,
	0x00, 0xC2,
	0x00, 0xC2,
	0x00, 0xC2,
	0x00, 0xC2,
	0x00, 0xC2,
	0x00, 0xC2,
	0x00, 0xC2,
	0x00, 0xC2,
	0x00, 0xC2,
	0x00, 0xC2,
	0x00, 0xC2,
	0x00, 0xC2,
	0x00, 0xC2,
	0x00, 0xC2,
	0x00, 0xC2,
	0x00, 0xC2,
	0x00, 0xC2,
	0x00, 0xC2,
	0x00, 0xC2,
	0x00, 0xC2,
	0x00, 0xC2,
	0x00, 0xC2,
	0x00, 0xC2,
	0x00, 0xC2,
	0x00, 0xC2,
	0x00, 0xC2,
	0x00, 0xC2,
	0x00, 0xC2,
	0x00, 0xC2,
	0x00, 0xC2,
	0x00, 0xC2,
	0x00, 0xC2,
	0x00, 0xC2,
	0x00, 0xC2,
	0x00, 0xC2,
	0x00, 0x9C,
	0x00, 0x9C,
	0x00, 0x9C,
	0x00, 0x9C,
	0x00, 0x9C,
	0x00, 0x9C,
	0x00, 0x9C,
	0x00, 0x9C,
	0x00, 0x9C,
	0x00, 0x9C,
	0x00, 0x9C,
	0x00, 0x9C,
	0x00, 0x9C,
	0x00, 0x9C,
	0x00, 0x75,
	0x00, 0x75,
	0x00, 0x75,
	0x00, 0x75,
	0x00, 0x75,
	0x00, 0x75,
	0x00, 0x75,
	0x00, 0x75,
	0x00, 0x75,
	0x00, 0x75,
	0x00, 0x75,
	0x00, 0x75,
	0x00, 0x75,
	0x00, 0x4e,
	0x00, 0x4e,
	0x00, 0x4e,
	0x00, 0x4e,
	0x00, 0x4e,
	0x00, 0x4e,
	0x00, 0x4e,
	0x00, 0x4e,
	0x00, 0x4e,
	0x00, 0x4e,
	0x00, 0x4e,
	0x00, 0x4e,
	0x00, 0x4e,
	0x00, 0x4e,
	0x00, 0x30,
	0x00, 0x30,
	0x00, 0x30,
	0x00, 0x30,
	0x00, 0x30,
	0x00, 0x30,
	0x00, 0x30,
	0x00, 0x30,
	0x00, 0x30,
	0x00, 0x30,
	0x00, 0x30,
	0x00, 0x30,
	0x00, 0x30,
	0x00, 0x30,
	0x00, 0x12
};

//7mask
static signed char rtbl2nit_7mask[10] = {0, 42, 40, 36, 32, 26, 17, 8, 6, 0 };
static signed char rtbl3nit_7mask[10] = {0, 34, 32, 28, 24, 19, 12, 6, 5, 0 };
static signed char rtbl4nit_7mask[10] = {0, 29, 27, 23, 20, 15, 9, 4, 4, 0 };
static signed char rtbl5nit_7mask[10] = {0, 26, 24, 20, 17, 13, 8, 4, 4, 0 };
static signed char rtbl6nit_7mask[10] = {0, 25, 23, 19, 16, 12, 8, 4, 4, 0 };
static signed char rtbl7nit_7mask[10] = {0, 25, 23, 19, 15, 11, 7, 4, 4, 0 };
static signed char rtbl8nit_7mask[10] = {0, 23, 21, 17, 14, 10, 6, 3, 4, 0 };
static signed char rtbl9nit_7mask[10] = {0, 22, 20, 16, 13, 9, 5, 3, 4, 0 };
static signed char rtbl10nit_7mask[10] = {0, 22, 20, 16, 13, 9, 5, 3, 4, 0 };
static signed char rtbl11nit_7mask[10] = {0, 22, 20, 16, 13, 9, 5, 4, 5, 0 };
static signed char rtbl12nit_7mask[10] = {0, 22, 20, 16, 13, 9, 5, 4, 4, 0 };
static signed char rtbl13nit_7mask[10] = {0, 21, 19, 15, 12, 8, 5, 4, 4, 0 };
static signed char rtbl14nit_7mask[10] = {0, 21, 19, 15, 12, 8, 5, 4, 4, 0 };
static signed char rtbl15nit_7mask[10] = {0, 21, 19, 15, 12, 8, 5, 4, 4, 0 };
static signed char rtbl16nit_7mask[10] = {0, 20, 18, 14, 11, 8, 5, 4, 4, 0 };
static signed char rtbl17nit_7mask[10] = {0, 20, 18, 14, 11, 8, 5, 4, 4, 0 };
static signed char rtbl19nit_7mask[10] = {0, 20, 18, 14, 11, 7, 5, 4, 5, 0 };
static signed char rtbl20nit_7mask[10] = {0, 20, 18, 14, 11, 7, 5, 4, 4, 0 };
static signed char rtbl21nit_7mask[10] = {0, 20, 18, 14, 11, 8, 6, 4, 5, 0 };
static signed char rtbl22nit_7mask[10] = {0, 20, 18, 14, 11, 8, 5, 4, 5, 0 };
static signed char rtbl24nit_7mask[10] = {0, 19, 17, 13, 10, 7, 5, 3, 5, 0 };
static signed char rtbl25nit_7mask[10] = {0, 19, 16, 12, 10, 7, 5, 3, 5, 0 };
static signed char rtbl27nit_7mask[10] = {0, 18, 15, 11, 9, 6, 4, 3, 5, 0 };
static signed char rtbl29nit_7mask[10] = {0, 18, 15, 11, 9, 6, 4, 3, 4, 0 };
static signed char rtbl30nit_7mask[10] = {0, 17, 14, 10, 8, 5, 4, 3, 4, 0 };
static signed char rtbl32nit_7mask[10] = {0, 17, 14, 10, 8, 5, 3, 3, 4, 0 };
static signed char rtbl34nit_7mask[10] = {0, 16, 14, 10, 8, 5, 3, 3, 4, 0 };
static signed char rtbl37nit_7mask[10] = {0, 15, 13, 9, 7, 4, 3, 3, 4, 0 };
static signed char rtbl39nit_7mask[10] = {0, 14, 12, 8, 7, 4, 3, 3, 4, 0 };
static signed char rtbl41nit_7mask[10] = {0, 14, 12, 8, 6, 4, 3, 3, 4, 0 };
static signed char rtbl44nit_7mask[10] = {0, 13, 11, 8, 6, 3, 3, 3, 4, 0 };
static signed char rtbl47nit_7mask[10] = {0, 12, 10, 7, 5, 3, 3, 3, 4, 0 };
static signed char rtbl50nit_7mask[10] = {0, 12, 10, 7, 5, 3, 3, 3, 4, 0 };
static signed char rtbl53nit_7mask[10] = {0, 11, 9, 6, 5, 3, 2, 3, 4, 0 };
static signed char rtbl56nit_7mask[10] = {0, 11, 9, 6, 5, 3, 2, 3, 4, 0 };
static signed char rtbl60nit_7mask[10] = {0, 10, 9, 6, 4, 3, 2, 3, 4, 0 };
static signed char rtbl64nit_7mask[10] = {0, 10, 8, 5, 4, 2, 2, 3, 4, 0 };
static signed char rtbl68nit_7mask[10] = {0, 9, 7, 5, 4, 2, 2, 3, 4, 0 };
static signed char rtbl72nit_7mask[10] = {0, 9, 7, 5, 4, 3, 3, 3, 4, 0 };
static signed char rtbl77nit_7mask[10] = {0, 9, 7, 5, 3, 2, 2, 2, 3, 0 };
static signed char rtbl82nit_7mask[10] = {0, 9, 7, 4, 3, 2, 2, 2, 3, 0 };
static signed char rtbl87nit_7mask[10] = {0, 8, 7, 4, 3, 2, 2, 2, 2, 0 };
static signed char rtbl93nit_7mask[10] = {0, 8, 6, 4, 3, 2, 2, 3, 3, 0 };
static signed char rtbl98nit_7mask[10] = {0, 8, 6, 4, 3, 2, 2, 3, 3, 0 };
static signed char rtbl105nit_7mask[10] = {0, 8, 6, 4, 2, 2, 2, 3, 3, 0 };
static signed char rtbl111nit_7mask[10] = {0, 7, 5, 3, 2, 2, 2, 3, 3, 0 };
static signed char rtbl119nit_7mask[10] = {0, 7, 5, 3, 2, 2, 2, 4, 2, 0 };
static signed char rtbl126nit_7mask[10] = {0, 7, 6, 4, 3, 3, 3, 5, 3, 0 };
static signed char rtbl134nit_7mask[10] = {0, 7, 5, 3, 2, 2, 2, 4, 3, 0 };
static signed char rtbl143nit_7mask[10] = {0, 7, 5, 3, 2, 2, 2, 4, 4, 0 };
static signed char rtbl152nit_7mask[10] = {0, 6, 5, 3, 2, 2, 2, 4, 3, 0 };
static signed char rtbl162nit_7mask[10] = {0, 6, 4, 3, 2, 2, 2, 4, 3, 0 };
static signed char rtbl172nit_7mask[10] = {0, 5, 3, 2, 2, 2, 2, 4, 3, 0 };
static signed char rtbl183nit_7mask[10] = {0, 5, 3, 2, 1, 2, 2, 4, 3, 0 };
static signed char rtbl195nit_7mask[10] = {0, 5, 3, 2, 1, 2, 2, 3, 3, 0 };
static signed char rtbl207nit_7mask[10] = {0, 4, 3, 2, 1, 2, 2, 3, 2, 0 };
static signed char rtbl220nit_7mask[10] = {0, 4, 2, 2, 1, 2, 1, 3, 2, 0 };
static signed char rtbl234nit_7mask[10] = {0, 3, 2, 2, 1, 2, 1, 3, 2, 0 };
static signed char rtbl249nit_7mask[10] = {0, 3, 1, 2, 1, 1, 1, 2, 1, 0 };
static signed char rtbl265nit_7mask[10] = {0, 3, 1, 1, 1, 1, 1, 2, 2, 0 };
static signed char rtbl282nit_7mask[10] = {0, 2, 1, 1, 1, 0, 0, 2, 1, 0 };
static signed char rtbl300nit_7mask[10] = {0, 2, 0, 0, 0, 0, 0, 1, 1, 0 };
static signed char rtbl316nit_7mask[10] = {0, 2, 0, 0, 0, 0, 1, 1, 0, 0 };
static signed char rtbl333nit_7mask[10] = {0, 1, 0, 0, 0, 0, 0, 1, 0, 0 };
static signed char rtbl350nit_7mask[10] = {0, 1, 0, 0, 0, 0, 0, 0, 0, 0 };
static signed char rtbl357nit_7mask[10] = {0, 0, 0, -1, -1, 0, 0, 0, 0, 0 };
static signed char rtbl365nit_7mask[10] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
static signed char rtbl372nit_7mask[10] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
static signed char rtbl380nit_7mask[10] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
static signed char rtbl387nit_7mask[10] = {0, 0, -1, -1, -1, -1, -1, 0, 0, 0 };
static signed char rtbl395nit_7mask[10] = {0, 0, -1, -1, -1, -1, -1, 0, 0, 0 };
static signed char rtbl403nit_7mask[10] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
static signed char rtbl412nit_7mask[10] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
static signed char rtbl420nit_7mask[10] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

static signed char ctbl2nit_7mask[30] = { 0, 0, 0, 0, 0, 0, -9, 0, -1, -8, 0, -2, -12, -1, -4, -17, 0, -7, -15, 0, -6, -5, 0, -2, -4, 0, -3, -5, 0, -9};
static signed char ctbl3nit_7mask[30] = { 0, 0, 0, 0, 0, 0, -12, 0, -3, -11, -1, -4, -12, 2, -2, -18, 0, -7, -12, 0, -4, -5, 0, -2, -3, 0, -2, -2, 0, -7};
static signed char ctbl4nit_7mask[30] = { 0, 0, 0, 0, 0, 0, -12, 1, -2, -11, 0, -3, -13, 0, -4, -16, 1, -7, -11, 0, -5, -4, 0, -1, -2, 0, -1, -1, 0, -6};
static signed char ctbl5nit_7mask[30] = { 0, 0, 0, 0, 0, 0, -13, 1, -3, -11, 1, -3, -12, 1, -3, -13, 2, -7, -10, 1, -4, -3, -1, -1, -2, 0, -2, -1, 0, -5};
static signed char ctbl6nit_7mask[30] = { 0, 0, 0, 0, 0, 0, -14, 2, -4, -10, 2, -2, -13, 0, -4, -11, 2, -6, -11, -1, -5, -2, -1, -1, -3, 0, -2, 0, 0, -4};
static signed char ctbl7nit_7mask[30] = { 0, 0, 0, 0, 0, 0, -12, 1, -4, -11, -1, -4, -12, 1, -5, -11, 2, -7, -9, 0, -4, -3, -1, -1, -2, 0, -2, 0, 0, -3};
static signed char ctbl8nit_7mask[30] = { 0, 0, 0, 0, 0, 0, -12, 1, -5, -10, 3, -3, -11, 0, -5, -11, 2, -7, -8, 0, -3, -3, -1, -1, -1, 0, -1, 0, 0, -3};
static signed char ctbl9nit_7mask[30] = { 0, 0, 0, 0, 0, 0, -11, 2, -4, -10, 1, -4, -10, 1, -4, -12, 2, -7, -6, 1, -2, -2, 0, 0, -2, 0, -2, 1, 0, -2};
static signed char ctbl10nit_7mask[30] = { 0, 0, 0, 0, 0, 0, -11, 0, -6, -10, 2, -4, -10, 1, -4, -12, 1, -7, -7, 0, -3, -1, 0, 0, -1, 0, -1, 0, 0, -2};
static signed char ctbl11nit_7mask[30] = { 0, 0, 0, 0, 0, 0, -11, 1, -5, -10, 1, -5, -10, 0, -5, -12, 1, -7, -6, 0, -2, -1, 0, 0, -2, 0, -2, 1, 0, -1};
static signed char ctbl12nit_7mask[30] = { 0, 0, 0, 0, 0, 0, -11, 1, -6, -9, 1, -4, -10, 0, -5, -12, 0, -7, -7, 0, -3, 0, 0, 0, -2, 0, -1, 1, 0, -1};
static signed char ctbl13nit_7mask[30] = { 0, 0, 0, 0, 0, 0, -11, 1, -6, -9, 2, -5, -10, 0, -6, -10, 1, -6, -5, 0, -2, -1, 0, 0, -2, 0, -2, 1, 0, 0};
static signed char ctbl14nit_7mask[30] = { 0, 0, 0, 0, 0, 0, -11, 1, -6, -9, 2, -5, -10, 0, -6, -9, 0, -6, -6, 0, -2, -2, 0, -1, -1, 0, -1, 1, 0, 0};
static signed char ctbl15nit_7mask[30] = { 0, 0, 0, 0, 0, 0, -10, 1, -6, -11, 1, -6, -9, 0, -6, -10, 0, -6, -5, 0, -2, -2, -1, -1, -1, 0, -1, 0, 0, 0};
static signed char ctbl16nit_7mask[30] = { 0, 0, 0, 0, 0, 0, -9, 1, -6, -9, 1, -6, -6, 2, -4, -11, 0, -7, -5, 0, -2, -1, -1, 0, -1, 0, -1, 0, 0, 0};
static signed char ctbl17nit_7mask[30] = { 0, 0, 0, 0, 0, 0, -9, 1, -7, -8, 1, -6, -8, 1, -5, -9, 0, -6, -4, 1, -1, -1, -1, -1, -1, 0, -1, 0, 0, 1};
static signed char ctbl19nit_7mask[30] = { 0, 0, 0, 0, 0, 0, -10, 1, -8, -8, 0, -7, -10, -1, -5, -6, 2, -4, -4, 1, -2, -1, 0, 0, -1, 0, -1, 1, 0, 2};
static signed char ctbl20nit_7mask[30] = { 0, 0, 0, 0, 0, 0, -9, 1, -7, -7, 0, -7, -9, 0, -5, -7, 1, -5, -3, 1, -1, -1, 0, 0, -1, 0, -1, 0, 0, 2};
static signed char ctbl21nit_7mask[30] = { 0, 0, 0, 0, 0, 0, -8, 1, -7, -8, 1, -6, -8, 0, -5, -7, 0, -6, -4, 0, -3, -1, 0, 0, -1, 0, -1, 1, 0, 3};
static signed char ctbl22nit_7mask[30] = { 0, 0, 0, 0, 0, 0, -9, 0, -8, -9, 0, -7, -7, 0, -5, -9, -1, -6, -4, 0, -2, -1, 0, 0, -1, 0, -1, 1, 0, 3};
static signed char ctbl24nit_7mask[30] = { 0, 0, 0, 0, 0, 0, -11, 0, -9, -9, 0, -7, -7, 1, -4, -7, 0, -5, -4, 0, -2, 1, 0, 0, -2, 0, -1, 1, 0, 3};
static signed char ctbl25nit_7mask[30] = { 0, 0, 0, 0, 0, 0, -11, 0, -9, -8, 3, -5, -7, 0, -4, -7, 0, -5, -4, 0, -2, 1, 0, 0, -1, 0, -1, 0, 0, 3};
static signed char ctbl27nit_7mask[30] = { 0, 0, 0, 0, 0, 0, -11, 0, -8, -7, 2, -5, -6, 1, -4, -7, 0, -6, -3, 0, -1, 1, 0, 0, -1, 0, -1, 0, 0, 3};
static signed char ctbl29nit_7mask[30] = { 0, 0, 0, 0, 0, 0, -11, 0, -8, -8, 1, -6, -7, 0, -4, -7, -1, -6, -2, 0, -1, 0, 0, 1, -1, 0, -1, 0, 0, 3};
static signed char ctbl30nit_7mask[30] = { 0, 0, 0, 0, 0, 0, -11, 1, -8, -7, 2, -5, -7, 1, -4, -5, 0, -5, -3, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0, 3};
static signed char ctbl32nit_7mask[30] = { 0, 0, 0, 0, 0, 0, -11, 1, -8, -8, 1, -6, -8, 0, -4, -6, 0, -5, -2, 0, -1, 0, 0, 1, -1, 0, -1, 0, 0, 3};
static signed char ctbl34nit_7mask[30] = { 0, 0, 0, 0, 0, 0, -12, 0, -9, -9, 0, -6, -8, 0, -4, -6, 0, -5, -2, 0, -1, 0, 0, 1, -1, 0, -1, 1, 0, 4};
static signed char ctbl37nit_7mask[30] = { 0, 0, 0, 0, 0, 0, -11, -1, -9, -10, 0, -6, -8, -1, -4, -4, 1, -3, -2, 0, -1, 0, 0, 1, -1, 0, -1, 1, 0, 4};
static signed char ctbl39nit_7mask[30] = { 0, 0, 0, 0, 0, 0, -12, 0, -9, -7, 3, -5, -7, 0, -2, -5, 0, -4, -2, 0, -1, 0, 0, 1, -1, 0, -1, 1, 0, 4};
static signed char ctbl41nit_7mask[30] = { 0, 0, 0, 0, 0, 0, -13, -1, -10, -9, -1, -6, -5, 2, -2, -4, 0, -3, -2, 0, -1, 0, 0, 1, -1, 0, -1, 1, 0, 4};
static signed char ctbl44nit_7mask[30] = { 0, 0, 0, 0, 0, 0, -10, 3, -9, -9, -2, -6, -7, -1, -3, -3, 2, -3, -1, 0, 0, 0, 0, 1, -1, 0, -1, 1, 0, 4};
static signed char ctbl47nit_7mask[30] = { 0, 0, 0, 0, 0, 0, -11, 2, -9, -8, 0, -5, -5, 1, -2, -2, 2, -3, -2, 0, 0, 0, 0, 1, -1, 0, -1, 1, 0, 4};
static signed char ctbl50nit_7mask[30] = { 0, 0, 0, 0, 0, 0, -12, 1, -10, -7, -1, -5, -5, 1, -2, -3, 1, -3, -2, 0, 0, 0, 0, 0, -1, 0, 0, 1, 0, 4};
static signed char ctbl53nit_7mask[30] = { 0, 0, 0, 0, 0, 0, -11, 2, -8, -3, 3, -3, -5, 1, -2, -4, 0, -3, -2, 0, -1, 1, 0, 1, -1, 0, 0, 1, 0, 4};
static signed char ctbl56nit_7mask[30] = { 0, 0, 0, 0, 0, 0, -13, 1, -9, -3, 2, -3, -6, 0, -2, -4, 0, -3, -2, 0, -1, 1, 0, 1, 0, 0, 0, 0, 0, 4};
static signed char ctbl60nit_7mask[30] = { 0, 0, 0, 0, 0, 0, -14, -1, -9, -6, -1, -4, -4, 2, -2, -3, 0, -2, -2, 0, -1, 1, 0, 1, 0, 0, 0, 0, 0, 4};
static signed char ctbl64nit_7mask[30] = { 0, 0, 0, 0, 0, 0, -14, -1, -9, -3, 1, -2, -6, 1, -2, -2, 1, -2, -1, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 4};
static signed char ctbl68nit_7mask[30] = { 0, 0, 0, 0, 0, 0, -10, 3, -8, -4, 1, -2, -5, 0, -2, -3, 1, -2, 0, 0, 0, 0, 0, 1, -1, 0, -1, 1, 0, 5};
static signed char ctbl72nit_7mask[30] = { 0, 0, 0, 0, 0, 0, -11, 2, -8, -4, 0, -2, -4, 0, -2, -2, 0, -2, -1, 0, 0, 0, 0, 1, -1, 0, -1, 1, 0, 5};
static signed char ctbl77nit_7mask[30] = { 0, 0, 0, 0, 0, 0, -12, 0, -9, -5, 0, -2, -5, 0, -2, -2, 0, -3, -1, 0, 1, 1, 0, 1, 0, 0, -1, 1, 0, 5};
static signed char ctbl82nit_7mask[30] = { 0, 0, 0, 0, 0, 0, -15, -2, -10, -4, 0, -2, -3, 0, -1, -2, 0, -2, -1, 0, 0, 1, 0, 0, -1, 0, 0, 1, 0, 5};
static signed char ctbl87nit_7mask[30] = { 0, 0, 0, 0, 0, 0, -13, -2, -9, -5, 2, -2, -3, 0, -1, -2, 0, -2, 0, 0, 0, 0, 0, 0, 0, 0, -1, 2, 0, 5};
static signed char ctbl93nit_7mask[30] = { 0, 0, 0, 0, 0, 0, -11, 2, -7, -3, 2, -1, -3, 0, -1, -1, 0, -1, 0, 0, 0, 0, 0, 1, -1, 0, -1, 1, 0, 5};
static signed char ctbl98nit_7mask[30] = { 0, 0, 0, 0, 0, 0, -11, 1, -7, -6, 0, -3, -3, 0, -1, -2, 0, -2, 0, 0, 0, 0, 0, 0, -1, 0, -1, 1, 0, 5};
static signed char ctbl105nit_7mask[30] = { 0, 0, 0, 0, 0, 0, -13, -1, -8, -6, -1, -3, -1, 2, 0, -2, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 4};
static signed char ctbl111nit_7mask[30] = { 0, 0, 0, 0, 0, 0, -9, 3, -6, -5, 0, -2, -1, 2, 0, -2, 0, -2, 1, 0, 1, 0, 0, 1, 0, 0, 0, 1, 0, 4};
static signed char ctbl119nit_7mask[30] = { 0, 0, 0, 0, 0, 0, -10, 2, -6, -3, 1, -1, -2, 1, 0, -1, 0, -1, 0, 0, 1, 1, 0, 0, -1, 0, 1, 1, 0, 4};
static signed char ctbl126nit_7mask[30] = { 0, 0, 0, 0, 0, 0, -12, 1, -7, -5, 0, -2, -1, 1, 0, -2, 0, -1, 0, 0, 1, 1, 0, 0, -1, 0, 0, 2, 0, 4};
static signed char ctbl134nit_7mask[30] = { 0, 0, 0, 0, 0, 0, -14, -1, -8, -5, -1, -2, -1, 2, 1, -1, 0, -1, 0, 0, 1, 1, 0, 0, -1, 0, 0, 1, 0, 3};
static signed char ctbl143nit_7mask[30] = { 0, 0, 0, 0, 0, 0, -13, -2, -7, -4, 0, -2, -1, 0, 0, -1, 0, -1, -1, 0, 0, 0, 0, 1, 0, 0, 0, 2, 0, 3};
static signed char ctbl152nit_7mask[30] = { 0, 0, 0, 0, 0, 0, -12, 0, -7, -5, -2, -3, -2, 0, -1, 0, 0, 0, -1, 0, 0, 2, 0, 1, -1, 0, 0, 1, 0, 3};
static signed char ctbl162nit_7mask[30] = { 0, 0, 0, 0, 0, 0, -9, 3, -5, -5, -1, -2, -1, 0, 0, -1, 0, 0, 1, 0, 1, 0, 0, 0, -1, 0, 0, 2, 0, 3};
static signed char ctbl172nit_7mask[30] = { 0, 0, 0, 0, 0, 0, -9, 3, -4, -2, 0, -1, -1, 0, 0, -2, 0, -1, 1, 0, 1, -1, 0, 0, -1, 0, 0, 2, 0, 3};
static signed char ctbl183nit_7mask[30] = { 0, 0, 0, 0, 0, 0, -8, 2, -3, -1, 2, 0, 0, 0, 0, -1, 0, 0, 0, 0, 1, 0, 0, 0, -1, 0, 0, 2, 0, 2};
static signed char ctbl195nit_7mask[30] = { 0, 0, 0, 0, 0, 0, -10, 0, -5, -2, 0, -1, 1, 0, 1, -1, 0, 0, 0, 0, 0, 0, 0, 0, -1, 0, 0, 2, 0, 2};
static signed char ctbl207nit_7mask[30] = { 0, 0, 0, 0, 0, 0, -9, 0, -3, -1, 0, -1, 0, 0, 1, -1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, -1, 1, 0, 2};
static signed char ctbl220nit_7mask[30] = { 0, 0, 0, 0, 0, 0, -8, 0, -4, 0, 0, 0, 0, 0, 1, -1, 0, 0, -1, 0, 0, 0, 0, 1, 0, 0, -1, 1, 0, 2};
static signed char ctbl234nit_7mask[30] = { 0, 0, 0, 0, 0, 0, -7, 0, -3, 0, 0, 0, 0, 0, 1, -1, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 2};
static signed char ctbl249nit_7mask[30] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static signed char ctbl265nit_7mask[30] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static signed char ctbl282nit_7mask[30] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static signed char ctbl300nit_7mask[30] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static signed char ctbl316nit_7mask[30] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static signed char ctbl333nit_7mask[30] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static signed char ctbl350nit_7mask[30] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static signed char ctbl357nit_7mask[30] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static signed char ctbl365nit_7mask[30] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static signed char ctbl372nit_7mask[30] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static signed char ctbl380nit_7mask[30] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static signed char ctbl387nit_7mask[30] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static signed char ctbl395nit_7mask[30] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static signed char ctbl403nit_7mask[30] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static signed char ctbl412nit_7mask[30] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static signed char ctbl420nit_7mask[30] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

static unsigned char elvss_2nit_7mask[] = { 0xB6, 0xAC, 0x04 };
static unsigned char elvss_3nit_7mask[] = { 0xB6, 0xAC, 0x04 };
static unsigned char elvss_4nit_7mask[] = { 0xB6, 0xAC, 0x04 };
static unsigned char elvss_5nit_7mask[] = { 0xB6, 0xAC, 0x04 };
static unsigned char elvss_6nit_7mask[] = { 0xB6, 0xAC, 0x05 };
static unsigned char elvss_7nit_7mask[] = { 0xB6, 0xAC, 0x06 };
static unsigned char elvss_8nit_7mask[] = { 0xB6, 0xAC, 0x07 };
static unsigned char elvss_9nit_7mask[] = { 0xB6, 0xAC, 0x08 };
static unsigned char elvss_10nit_7mask[] = { 0xB6, 0xAC, 0x09 };
static unsigned char elvss_11nit_7mask[] = { 0xB6, 0xAC, 0x0A };
static unsigned char elvss_12nit_7mask[] = { 0xB6, 0xAC, 0x0B };
static unsigned char elvss_13nit_7mask[] = { 0xB6, 0xAC, 0x0C };
static unsigned char elvss_14nit_7mask[] = { 0xB6, 0xAC, 0x0D };
static unsigned char elvss_15nit_7mask[] = { 0xB6, 0xAC, 0x0E };
static unsigned char elvss_16nit_7mask[] = { 0xB6, 0xAC, 0x0F };
static unsigned char elvss_17nit_7mask[] = { 0xB6, 0xAC, 0x11 };
static unsigned char elvss_19nit_7mask[] = { 0xB6, 0xAC, 0x13 };
static unsigned char elvss_20nit_7mask[] = { 0xB6, 0xAC, 0x15 };
static unsigned char elvss_21nit_7mask[] = { 0xB6, 0xAC, 0x16 };
static unsigned char elvss_22nit_7mask[] = { 0xB6, 0xAC, 0x16 };
static unsigned char elvss_24nit_7mask[] = { 0xB6, 0xAC, 0x16 };
static unsigned char elvss_25nit_7mask[] = { 0xB6, 0xAC, 0x16 };
static unsigned char elvss_27nit_7mask[] = { 0xB6, 0xAC, 0x16 };
static unsigned char elvss_29nit_7mask[] = { 0xB6, 0xAC, 0x16 };
static unsigned char elvss_30nit_7mask[] = { 0xB6, 0xAC, 0x16 };
static unsigned char elvss_32nit_7mask[] = { 0xB6, 0xAC, 0x16 };
static unsigned char elvss_34nit_7mask[] = { 0xB6, 0xAC, 0x16 };
static unsigned char elvss_37nit_7mask[] = { 0xB6, 0xAC, 0x16 };
static unsigned char elvss_39nit_7mask[] = { 0xB6, 0xAC, 0x16 };
static unsigned char elvss_41nit_7mask[] = { 0xB6, 0xBC, 0x16 };
static unsigned char elvss_44nit_7mask[] = { 0xB6, 0xBC, 0x16 };
static unsigned char elvss_47nit_7mask[] = { 0xB6, 0xBC, 0x16 };
static unsigned char elvss_50nit_7mask[] = { 0xB6, 0xBC, 0x16 };
static unsigned char elvss_53nit_7mask[] = { 0xB6, 0xBC, 0x16 };
static unsigned char elvss_56nit_7mask[] = { 0xB6, 0xBC, 0x16 };
static unsigned char elvss_60nit_7mask[] = { 0xB6, 0xBC, 0x16 };
static unsigned char elvss_64nit_7mask[] = { 0xB6, 0xBC, 0x16 };
static unsigned char elvss_68nit_7mask[] = { 0xB6, 0xBC, 0x16 };
static unsigned char elvss_72nit_7mask[] = { 0xB6, 0xBC, 0x16 };
static unsigned char elvss_77nit_7mask[] = { 0xB6, 0xBC, 0x16 };
static unsigned char elvss_82nit_7mask[] = { 0xB6, 0xBC, 0x15 };
static unsigned char elvss_87nit_7mask[] = { 0xB6, 0xBC, 0x15 };
static unsigned char elvss_93nit_7mask[] = { 0xB6, 0xBC, 0x14 };
static unsigned char elvss_98nit_7mask[] = { 0xB6, 0xBC, 0x14 };
static unsigned char elvss_105nit_7mask[] = { 0xB6, 0xBC, 0x13 };
static unsigned char elvss_111nit_7mask[] = { 0xB6, 0xBC, 0x13 };
static unsigned char elvss_119nit_7mask[] = { 0xB6, 0xBC, 0x12 };
static unsigned char elvss_126nit_7mask[] = { 0xB6, 0xBC, 0x12 };
static unsigned char elvss_134nit_7mask[] = { 0xB6, 0xBC, 0x11 };
static unsigned char elvss_143nit_7mask[] = { 0xB6, 0xBC, 0x10 };
static unsigned char elvss_152nit_7mask[] = { 0xB6, 0xBC, 0x10 };
static unsigned char elvss_162nit_7mask[] = { 0xB6, 0xBC, 0x0F };
static unsigned char elvss_172nit_7mask[] = { 0xB6, 0xBC, 0x0E };
static unsigned char elvss_183nit_7mask[] = { 0xB6, 0xBC, 0x0E };
static unsigned char elvss_195nit_7mask[] = { 0xB6, 0xBC, 0x0E };
static unsigned char elvss_207nit_7mask[] = { 0xB6, 0xBC, 0x0E };
static unsigned char elvss_220nit_7mask[] = { 0xB6, 0xBC, 0x0D };
static unsigned char elvss_234nit_7mask[] = { 0xB6, 0xBC, 0x0D };
static unsigned char elvss_249nit_7mask[] = { 0xB6, 0xBC, 0x0D };
static unsigned char elvss_265nit_7mask[] = { 0xB6, 0xBC, 0x0C };
static unsigned char elvss_282nit_7mask[] = { 0xB6, 0xBC, 0x0B };
static unsigned char elvss_300nit_7mask[] = { 0xB6, 0xBC, 0x0A };
static unsigned char elvss_316nit_7mask[] = { 0xB6, 0xBC, 0x09 };
static unsigned char elvss_333nit_7mask[] = { 0xB6, 0xBC, 0x08 };
static unsigned char elvss_350nit_7mask[] = { 0xB6, 0xBC, 0x07 };
static unsigned char elvss_357nit_7mask[] = { 0xB6, 0xBC, 0x06 };
static unsigned char elvss_365nit_7mask[] = { 0xB6, 0xBC, 0x06 };
static unsigned char elvss_372nit_7mask[] = { 0xB6, 0xBC, 0x06 };
static unsigned char elvss_380nit_7mask[] = { 0xB6, 0xBC, 0x06 };
static unsigned char elvss_387nit_7mask[] = { 0xB6, 0xBC, 0x05 };
static unsigned char elvss_395nit_7mask[] = { 0xB6, 0xBC, 0x05 };
static unsigned char elvss_403nit_7mask[] = { 0xB6, 0xBC, 0x05 };
static unsigned char elvss_412nit_7mask[] = { 0xB6, 0xBC, 0x04 };
static unsigned char elvss_420nit_7mask[] = { 0xB6, 0xBC, 0x04 };
static unsigned char elvss_443nit_7mask[] = { 0xB6, 0xBC, 0x0D };
static unsigned char elvss_465nit_7mask[] = { 0xB6, 0xBC, 0x0C };
static unsigned char elvss_488nit_7mask[] = { 0xB6, 0xBC, 0x0A };
static unsigned char elvss_510nit_7mask[] = { 0xB6, 0xBC, 0x09 };
static unsigned char elvss_533nit_7mask[] = { 0xB6, 0xBC, 0x08 };
static unsigned char elvss_555nit_7mask[] = { 0xB6, 0xBC, 0x07 };
static unsigned char elvss_578nit_7mask[] = { 0xB6, 0xBC, 0x05 };
static unsigned char elvss_600nit_7mask[] = { 0xB6, 0xBC, 0x04 };

static unsigned char aor_table_7mask[366 * 2] = {
	0x70, 0x76,
	0x70, 0x75,
	0x70, 0x73,
	0x70, 0x71,
	0x70, 0x6F,
	0x70, 0x67,
	0x70, 0x65,
	0x70, 0x63,
	0x70, 0x62,
	0x70, 0x61,
	0x70, 0x57,
	0x70, 0x55,
	0x70, 0x54,
	0x70, 0x53,
	0x70, 0x51,
	0x70, 0x47,
	0x70, 0x45,
	0x70, 0x43,
	0x70, 0x41,
	0x70, 0x37,
	0x70, 0x36,
	0x70, 0x35,
	0x70, 0x33,
	0x70, 0x31,
	0x70, 0x27,
	0x70, 0x25,
	0x70, 0x23,
	0x70, 0x21,
	0x70, 0x1B,
	0x70, 0x17,
	0x70, 0x15,
	0x70, 0x13,
	0x70, 0x11,
	0x70, 0x07,
	0x70, 0x05,
	0x70, 0x03,
	0x70, 0x02,
	0x70, 0x01,
	0x60, 0xF7,
	0x60, 0xF5,
	0x60, 0xF3,
	0x60, 0xF1,
	0x60, 0xE5,
	0x60, 0xE3,
	0x60, 0xE0,
	0x60, 0xD8,
	0x60, 0xD3,
	0x60, 0xCA,
	0x60, 0xC1,
	0x60, 0xB4,
	0x60, 0xB0,
	0x60, 0xA3,
	0x60, 0x97,
	0x60, 0x87,
	0x60, 0x77,
	0x60, 0x6D,
	0x60, 0x63,
	0x60, 0x5B,
	0x60, 0x53,
	0x60, 0x4B,
	0x60, 0x43,
	0x60, 0x33,
	0x60, 0x22,
	0x60, 0x1B,
	0x60, 0x13,
	0x60, 0x03,
	0x50, 0xF2,
	0x50, 0xE3,
	0x50, 0xD3,
	0x50, 0xCB,
	0x50, 0xC3,
	0x50, 0xB3,
	0x50, 0xA2,
	0x50, 0x93,
	0x50, 0x85,
	0x50, 0x6D,
	0x50, 0x55,
	0x50, 0x44,
	0x50, 0x34,
	0x50, 0x24,
	0x50, 0x14,
	0x40, 0xFC,
	0x40, 0xE4,
	0x40, 0xCB,
	0x40, 0xB3,
	0x40, 0x9A,
	0x40, 0x82,
	0x40, 0x68,
	0x40, 0x4D,
	0x40, 0x32,
	0x40, 0x17,
	0x30, 0xF5,
	0x30, 0xD4,
	0x30, 0xB3,
	0x30, 0x91,
	0x30, 0x72,
	0x30, 0x53,
	0x30, 0x32,
	0x30, 0x10,
	0x30, 0x0C,
	0x20, 0xE5,
	0x30, 0x09,
	0x20, 0xE5,
	0x30, 0x07,
	0x20, 0xE5,
	0x30, 0x0C,
	0x20, 0xE5,
	0x30, 0x03,
	0x20, 0xE5,
	0x30, 0x0D,
	0x20, 0xE5,
	0x30, 0x05,
	0x20, 0xE5,
	0x30, 0x0D,
	0x20, 0xE5,
	0x30, 0x06,
	0x20, 0xE5,
	0x30, 0x09,
	0x20, 0xE5,
	0x30, 0x0B,
	0x20, 0xE5,
	0x30, 0x08,
	0x20, 0xE5,
	0x30, 0x0A,
	0x20, 0xE5,
	0x30, 0x08,
	0x20, 0xE5,
	0x30, 0x09,
	0x20, 0xE5,
	0x20, 0xD7,
	0x20, 0xCA,
	0x20, 0xBC,
	0x20, 0xAE,
	0x20, 0xA0,
	0x20, 0x93,
	0x20, 0x85,
	0x20, 0x78,
	0x20, 0x6A,
	0x20, 0x5D,
	0x20, 0x4F,
	0x20, 0x42,
	0x20, 0x34,
	0x20, 0x27,
	0x20, 0x19,
	0x20, 0x0C,
	0x10, 0xFE,
	0x10, 0xF1,
	0x10, 0xE3,
	0x10, 0xD5,
	0x10, 0xC8,
	0x10, 0xBB,
	0x10, 0xAF,
	0x10, 0xA2,
	0x10, 0x95,
	0x10, 0x88,
	0x10, 0x7B,
	0x10, 0x6F,
	0x10, 0x62,
	0x10, 0x30,
	0x10, 0x23,
	0x10, 0x16,
	0x10, 0x09,
	0x00, 0xFC,
	0x00, 0xEF,
	0x00, 0xE2,
	0x00, 0xD5,
	0x10, 0x31,
	0x10, 0x26,
	0x10, 0x1A,
	0x10, 0x0F,
	0x10, 0x03,
	0x00, 0xF8,
	0x00, 0xEC,
	0x00, 0xE1,
	0x00, 0xD5,
	0x10, 0x31,
	0x10, 0x26,
	0x10, 0x1A,
	0x10, 0x0F,
	0x10, 0x03,
	0x00, 0xF8,
	0x00, 0xEC,
	0x00, 0xE1,
	0x00, 0xD5,
	0x10, 0x32,
	0x10, 0x28,
	0x10, 0x1D,
	0x10, 0x13,
	0x10, 0x09,
	0x00, 0xFE,
	0x00, 0xF4,
	0x00, 0xEA,
	0x00, 0xDF,
	0x00, 0xD5,
	0x10, 0x21,
	0x10, 0x16,
	0x10, 0x0C,
	0x10, 0x01,
	0x00, 0xF6,
	0x00, 0xEB,
	0x00, 0xE0,
	0x00, 0xD5,
	0x10, 0x23,
	0x10, 0x19,
	0x10, 0x10,
	0x10, 0x06,
	0x00, 0xFC,
	0x00, 0xF2,
	0x00, 0xE9,
	0x00, 0xDF,
	0x00, 0xD5,
	0x10, 0x1F,
	0x10, 0x16,
	0x10, 0x0D,
	0x10, 0x03,
	0x00, 0xFA,
	0x00, 0xF1,
	0x00, 0xE8,
	0x00, 0xDE,
	0x00, 0xD5,
	0x00, 0xEE,
	0x00, 0xE6,
	0x00, 0xDD,
	0x00, 0xD5,
	0x00, 0xF1,
	0x00, 0xE8,
	0x00, 0xDE,
	0x00, 0xD5,
	0x00, 0xC8,
	0x00, 0xBB,
	0x00, 0xAE,
	0x00, 0xA3,
	0x00, 0x98,
	0x00, 0x8C,
	0x00, 0x81,
	0x00, 0x79,
	0x00, 0x71,
	0x00, 0x69,
	0x00, 0x61,
	0x00, 0x54,
	0x00, 0x47,
	0x00, 0x3A,
	0x00, 0x2D,
	0x00, 0x29,
	0x00, 0x1F,
	0x00, 0x16,
	0x00, 0x0C,
	0x00, 0x2C,
	0x00, 0x21,
	0x00, 0x17,
	0x00, 0x0C,
	0x00, 0x27,
	0x00, 0x1E,
	0x00, 0x15,
	0x00, 0x0C,
	0x00, 0x0C,
//256~
	0x00, 0x0C,
	0x00, 0x0C,
	0x00, 0x0C,
	0x00, 0x0C,
	0x00, 0x0C,
	0x00, 0x0C,
	0x00, 0x0C,
	0x00, 0x0C,
	0x00, 0x0C,
	0x00, 0x0C,
	0x00, 0x0C,
	0x00, 0x0C,
	0x00, 0x0C,
	0x00, 0x0C,
	0x00, 0x0C,
	0x00, 0x0C,
	0x00, 0x0C,
	0x00, 0x0C,
	0x00, 0x0C,
	0x00, 0x0C,
	0x00, 0x0C,
	0x00, 0x0C,
	0x00, 0x0C,
	0x00, 0x0C,
	0x00, 0x0C,
	0x00, 0x0C,
	0x00, 0x0C,
	0x00, 0x0C,
	0x00, 0x0C,
	0x00, 0x0C,
	0x00, 0x0C,
	0x00, 0x0C,
	0x00, 0x0C,
	0x00, 0x0C,
	0x00, 0x0C,
	0x00, 0x0C,
	0x00, 0x0C,
	0x00, 0x0C,
	0x00, 0x0C,
	0x00, 0x0C,
	0x00, 0x0C,
	0x00, 0x0C,
	0x00, 0x0C,
	0x00, 0x0C,
	0x00, 0x0C,
	0x00, 0x0C,
	0x00, 0x0C,
	0x00, 0x0C,
	0x00, 0x0C,
	0x00, 0x0C,
	0x00, 0x0C,
	0x00, 0x0C,
	0x00, 0x0C,
	0x00, 0x0C,
	0x00, 0x0C,
	0x00, 0x0C,
	0x00, 0x0C,
	0x00, 0x0C,
	0x00, 0x0C,
	0x00, 0x0C,
	0x00, 0x0C,
	0x00, 0x0C,
	0x00, 0x0C,
	0x00, 0x0C,
	0x00, 0x0C,
	0x00, 0x0C,
	0x00, 0x0C,
	0x00, 0x0C,
	0x00, 0x0C,
	0x00, 0x0C,
	0x00, 0x0C,
	0x00, 0x0C,
	0x00, 0x0C,
	0x00, 0x0C,
	0x00, 0x0C,
	0x00, 0x0C,
	0x00, 0x0C,
	0x00, 0x0C,
	0x00, 0x0C,
	0x00, 0x0C,
	0x00, 0x0C,
	0x00, 0x0C,
	0x00, 0x0C,
	0x00, 0x0C,
	0x00, 0x0C,
	0x00, 0x0C,
	0x00, 0x0C,
	0x00, 0x0C,
	0x00, 0x0C,
	0x00, 0x0C,
	0x00, 0x0C,
	0x00, 0x0C,
	0x00, 0x0C,
	0x00, 0x0C,
	0x00, 0x0C,
	0x00, 0x0C,
	0x00, 0x0C,
	0x00, 0x0C,
	0x00, 0x0C,
	0x00, 0x0C,
	0x00, 0x0C,
	0x00, 0x0C,
	0x00, 0x0C,
	0x00, 0x0C,
	0x00, 0x0C,
	0x00, 0x0C,
	0x00, 0x0C,
	0x00, 0x0C,
	0x00, 0x0C,
	0x00, 0x0C
};
#endif
