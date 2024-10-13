/* linux/drivers/video/exynos_decon/panel/s6e3ha2_wqhd_aid_dimming.h
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

#ifndef __S6E3HA2_WQHD_AID_DIMMING_H__
#define __S6E3HA2_WQHD_AID_DIMMING_H__

/********************************************** HA3 **********************************************/

// start of aid sheet in rev.D
static signed char HA3_rtbl2nit[10] = {0, 12, 18, 17, 14, 14, 14, 11, 7, 0};
static signed char HA3_rtbl3nit[10] = {0, 20, 19, 18, 16, 16, 16, 13, 7, 0};
static signed char HA3_rtbl4nit[10] = {0, 18, 20, 18, 16, 16, 15, 11, 7, 0};
static signed char HA3_rtbl5nit[10] = {0, 20, 19, 17, 15, 15, 14, 11, 7, 0};
static signed char HA3_rtbl6nit[10] = {0, 22, 20, 18, 15, 14, 14, 10, 6, 0};
static signed char HA3_rtbl7nit[10] = {0, 20, 20, 17, 15, 14, 13, 9, 6, 0};
static signed char HA3_rtbl8nit[10] = {0, 22, 19, 17, 14, 14, 13, 10, 6, 0};
static signed char HA3_rtbl9nit[10] = {0, 19, 19, 16, 14, 13, 13, 9, 6, 0};
static signed char HA3_rtbl10nit[10] = {0, 22, 18, 15, 13, 13, 12, 9, 6, 0};
static signed char HA3_rtbl11nit[10] = {0, 20, 18, 15, 13, 13, 12, 9, 5, 0};
static signed char HA3_rtbl12nit[10] = {0, 19, 17, 14, 12, 12, 11, 8, 6, 0};
static signed char HA3_rtbl13nit[10] = {0, 20, 16, 14, 12, 12, 11, 8, 5, 0};
static signed char HA3_rtbl14nit[10] = {0, 18, 16, 13, 11, 11, 10, 8, 5, 0};
static signed char HA3_rtbl15nit[10] = {0, 17, 16, 13, 11, 11, 9, 7, 5, 0};
static signed char HA3_rtbl16nit[10] = {0, 19, 15, 12, 11, 10, 9, 7, 5, 0};
static signed char HA3_rtbl17nit[10] = {0, 17, 15, 12, 11, 10, 9, 7, 4, 0};
static signed char HA3_rtbl19nit[10] = {0, 16, 14, 11, 10, 10, 9, 7, 4, 0};
static signed char HA3_rtbl20nit[10] = {0, 20, 13, 10, 10, 9, 8, 6, 4, 0};
static signed char HA3_rtbl21nit[10] = {0, 14, 14, 10, 10, 9, 8, 6, 4, 0};
static signed char HA3_rtbl22nit[10] = {0, 16, 13, 10, 9, 9, 8, 6, 4, 0};
static signed char HA3_rtbl24nit[10] = {0, 17, 12, 9, 9, 8, 7, 6, 4, 0};
static signed char HA3_rtbl25nit[10] = {0, 15, 12, 9, 8, 8, 7, 6, 4, 0};
static signed char HA3_rtbl27nit[10] = {0, 11, 12, 9, 8, 7, 7, 5, 4, 0};
static signed char HA3_rtbl29nit[10] = {0, 12, 11, 8, 7, 7, 7, 5, 4, 0};
static signed char HA3_rtbl30nit[10] = {0, 14, 10, 8, 7, 6, 6, 4, 3, 0};
static signed char HA3_rtbl32nit[10] = {0, 13, 10, 8, 7, 6, 6, 5, 4, 0};
static signed char HA3_rtbl34nit[10] = {0, 12, 10, 7, 7, 6, 6, 5, 4, 0};
static signed char HA3_rtbl37nit[10] = {0, 13, 9, 7, 6, 5, 6, 5, 4, 0};
static signed char HA3_rtbl39nit[10] = {0, 10, 9, 6, 6, 6, 5, 5, 4, 0};
static signed char HA3_rtbl41nit[10] = {0, 9, 8, 6, 5, 5, 5, 5, 5, 0};
static signed char HA3_rtbl44nit[10] = {0, 9, 8, 6, 5, 5, 4, 4, 4, 0};
static signed char HA3_rtbl47nit[10] = {0, 9, 8, 6, 5, 5, 5, 4, 4, 0};
static signed char HA3_rtbl50nit[10] = {0, 9, 7, 5, 5, 4, 4, 4, 3, 0};
static signed char HA3_rtbl53nit[10] = {0, 7, 7, 5, 5, 4, 4, 4, 3, 0};
static signed char HA3_rtbl56nit[10] = {0, 6, 7, 5, 5, 4, 4, 4, 3, 0};
static signed char HA3_rtbl60nit[10] = {0, 8, 6, 5, 4, 4, 4, 3, 3, 0};
static signed char HA3_rtbl64nit[10] = {0, 6, 6, 4, 4, 4, 4, 3, 3, 0};
static signed char HA3_rtbl68nit[10] = {0, 7, 5, 4, 4, 4, 3, 3, 3, 0};
static signed char HA3_rtbl72nit[10] = {0, 7, 5, 4, 4, 3, 3, 3, 3, 0};
static signed char HA3_rtbl77nit[10] = {0, 7, 5, 4, 3, 4, 4, 5, 4, 0};
static signed char HA3_rtbl82nit[10] = {0, 6, 5, 4, 4, 3, 4, 4, 3, 0};
static signed char HA3_rtbl87nit[10] = {0, 8, 5, 4, 3, 3, 4, 4, 3, 0};
static signed char HA3_rtbl93nit[10] = {0, 5, 4, 3, 3, 2, 3, 4, 2, 0};
static signed char HA3_rtbl98nit[10] = {0, 6, 4, 3, 3, 3, 4, 5, 3, 0};
static signed char HA3_rtbl105nit[10] = {0, 4, 5, 4, 3, 3, 4, 5, 3, 0};
static signed char HA3_rtbl111nit[10] = {0, 6, 4, 3, 3, 4, 4, 5, 2, 0};
static signed char HA3_rtbl119nit[10] = {0, 8, 4, 4, 3, 3, 4, 4, 2, 0};
static signed char HA3_rtbl126nit[10] = {0, 6, 4, 3, 4, 4, 5, 5, 2, 1};
static signed char HA3_rtbl134nit[10] = {0, 6, 4, 3, 3, 4, 5, 6, 3, 1};
static signed char HA3_rtbl143nit[10] = {0, 5, 4, 3, 3, 4, 4, 6, 3, 1};
static signed char HA3_rtbl152nit[10] = {0, 5, 4, 3, 3, 4, 4, 6, 4, 1};
static signed char HA3_rtbl162nit[10] = {0, 6, 4, 3, 4, 4, 5, 6, 5, 2};
static signed char HA3_rtbl172nit[10] = {0, 3, 4, 3, 4, 5, 6, 7, 5, 2};
static signed char HA3_rtbl183nit[10] = {0, 2, 4, 4, 4, 4, 5, 7, 6, 2};
static signed char HA3_rtbl195nit[10] = {0, 4, 4, 3, 3, 4, 5, 8, 6, 3};
static signed char HA3_rtbl207nit[10] = {0, 2, 3, 3, 2, 4, 4, 6, 5, 3};
static signed char HA3_rtbl220nit[10] = {0, 4, 2, 3, 2, 3, 4, 6, 5, 3};
static signed char HA3_rtbl234nit[10] = {0, 3, 2, 3, 2, 3, 4, 6, 5, 3};
static signed char HA3_rtbl249nit[10] = {0, 2, 2, 3, 2, 3, 3, 4, 2, 2};
static signed char HA3_rtbl265nit[10] = {0, 1, 2, 2, 2, 2, 3, 4, 2, 2};
static signed char HA3_rtbl282nit[10] = {0, 2, 2, 2, 2, 1, 3, 3, 1, 1};
static signed char HA3_rtbl300nit[10] = {0, 2, 0, 0, 1, 0, 1, 2, 2, 1};
static signed char HA3_rtbl316nit[10] = {0, 3, 0, 1, 0, 0, 1, 2, 2, 0};
static signed char HA3_rtbl333nit[10] = {0, 3, 0, 0, 0, 1, 2, 2, 1, 0};
static signed char HA3_rtbl350nit[10] = {0, 0, 1, 1, 0, 1, 2, 1, 2, 0};
static signed char HA3_rtbl357nit[10] = {0, 0, 1, 1, 1, 1, 1, 1, 2, 0};
static signed char HA3_rtbl365nit[10] = {0, 0, 0, 0, 0, 0, 1, 1, 1, 0};
static signed char HA3_rtbl372nit[10] = {0, 0, 0, 0, 0, 0, 1, 1, 1, 0};
static signed char HA3_rtbl380nit[10] = {0, 0, 0, -1, -1, 0, 0, 0, 1, 0};
static signed char HA3_rtbl387nit[10] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static signed char HA3_rtbl395nit[10] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static signed char HA3_rtbl403nit[10] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 0};
static signed char HA3_rtbl412nit[10] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 0};
static signed char HA3_rtbl420nit[10] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};


static signed char HA3_ctbl2nit[30] = {0, 0, 0, 0, 0, 0,10, 0, 5, 2, 0, -2, -7, 3, 4, -13, 3, 3, -6, 2, 2, -2, 1, -1, -1, 0, -1, -8, 0, -6};
static signed char HA3_ctbl3nit[30] = {0, 0, 0, 0, 0, 0,10, 0, 0, -3, 1, -2, -8, 3, 1, -11, 3, 1, -6, 1, 0, -3, 0, -3, -2, 0, -2, -4, 1, -3};
static signed char HA3_ctbl4nit[30] = {0, 0, 0, 0, 0, 0,8, 0, -2, -5, 1, -3, -10, 3, 0, -9, 2, -1, -7, 1, -2, -1, 1, -2, -2, 0, -2, -3, 1, -2};
static signed char HA3_ctbl5nit[30] = {0, 0, 0, 0, 0, 0,8, 0, -3, -6, 1, -3, -10, 3, -1, -9, 3, -2, -5, 1, -2, -2, 1, -2, -1, 0, -1, -3, 0, -3};
static signed char HA3_ctbl6nit[30] = {0, 0, 0, 0, 0, 0,2, 1, -7, -6, 1, -3, -9, 2, -2, -10, 2, -5, -5, 1, -3, -2, 0, -2, 0, 0, -1, -2, 1, -1};
static signed char HA3_ctbl7nit[30] = {0, 0, 0, 0, 0, 0,-1, 1, -7, -7, 1, -4, -9, 2, -2, -9, 2, -5, -5, 1, -3, -1, 1, -1, 0, 0, -1, -1, 1, 0};
static signed char HA3_ctbl8nit[30] = {0, 0, 0, 0, 0, 0,0, 2, -8, -7, 1, -4, -8, 2, -3, -10, 2, -5, -4, 1, -3, -2, 0, -2, -1, 0, -1, -1, 0, -1};
static signed char HA3_ctbl9nit[30] = {0, 0, 0, 0, 0, 0,-5, 1, -9, -6, 2, -3, -9, 2, -4, -9, 1, -6, -4, 0, -3, -2, 0, -2, -1, 0, -1, 0, 0, 0};
static signed char HA3_ctbl10nit[30] = {0, 0, 0, 0, 0, 0,-4, 2, -10, -6, 2, -4, -9, 2, -4, -9, 2, -5, -5, 0, -4, -2, 0, -2, 0, 0, 0, 0, 0, 0};
static signed char HA3_ctbl11nit[30] = {0, 0, 0, 0, 0, 0,-5, 2, -10, -6, 2, -4, -8, 2, -4, -9, 2, -5, -5, 0, -4, -1, 0, -2, -1, 0, -1, 0, 0, 0};
static signed char HA3_ctbl12nit[30] = {0, 0, 0, 0, 0, 0,-4, 2, -10, -6, 2, -5, -8, 2, -5, -6, 2, -4, -3, 1, -2, -2, 0, -3, 0, 0, 0, 0, 0, 1};
static signed char HA3_ctbl13nit[30] = {0, 0, 0, 0, 0, 0,-5, 2, -11, -6, 1, -5, -7, 2, -4, -7, 2, -5, -4, 0, -3, -1, 0, -2, 0, 0, -1, 0, 0, 1};
static signed char HA3_ctbl14nit[30] = {0, 0, 0, 0, 0, 0,-6, 2, -10, -4, 2, -4, -6, 2, -5, -7, 2, -5, -3, 0, -3, -1, 0, -2, 0, 0, -1, 0, 0, 1};
static signed char HA3_ctbl15nit[30] = {0, 0, 0, 0, 0, 0,-9, 1, -11, -5, 2, -5, -5, 2, -4, -8, 2, -5, -3, 0, -2, -1, 0, -2, 0, 0, -1, 1, 0, 2};
static signed char HA3_ctbl16nit[30] = {0, 0, 0, 0, 0, 0,-6, 3, -11, -7, 1, -7, -6, 2, -4, -7, 2, -4, -3, 0, -2, -1, 0, -2, 0, 0, -1, 1, 0, 2};
static signed char HA3_ctbl17nit[30] = {0, 0, 0, 0, 0, 0,-8, 2, -12, -6, 1, -6, -7, 0, -5, -8, 2, -5, -2, 0, -1, -1, 0, -2, 0, 0, -1, 1, 0, 2};
static signed char HA3_ctbl19nit[30] = {0, 0, 0, 0, 0, 0,-6, 2, -11, -6, 2, -5, -6, 1, -4, -8, 1, -5, -2, 0, -1, -2, 0, -2, 1, 0, -1, 1, 0, 2};
static signed char HA3_ctbl20nit[30] = {0, 0, 0, 0, 0, 0,-7, 3, -13, -5, 2, -6, -5, 1, -3, -8, 1, -5, -2, 0, -2, 0, 0, -1, 0, 0, -1, 1, 0, 2};
static signed char HA3_ctbl21nit[30] = {0, 0, 0, 0, 0, 0,-9, 0, -11, -6, 1, -7, -6, 0, -4, -8, 1, -5, -2, 0, -2, 0, 0, -1, 0, 0, -1, 1, 0, 2};
static signed char HA3_ctbl22nit[30] = {0, 0, 0, 0, 0, 0,-8, 1, -11, -6, 2, -5, -6, 2, -4, -8, 0, -5, -2, 0, -2, 0, 0, -1, 0, 0, -1, 1, 0, 2};
static signed char HA3_ctbl24nit[30] = {0, 0, 0, 0, 0, 0,-6, 3, -12, -6, 1, -6, -7, 1, -4, -8, 0, -6, -1, 0, 0, -1, 0, -2, 1, 0, 0, 1, 0, 2};
static signed char HA3_ctbl25nit[30] = {0, 0, 0, 0, 0, 0,-9, 2, -13, -6, 1, -6, -7, 1, -3, -6, 1, -5, -1, 0, 0, -1, 0, -2, 1, 0, 0, 1, 0, 2};
static signed char HA3_ctbl27nit[30] = {0, 0, 0, 0, 0, 0,-9, 1, -11, -7, 0, -7, -5, 1, -3, -8, 1, -5, -1, 0, 0, 0, 0, -1, 0, 0, -1, 2, 0, 3};
static signed char HA3_ctbl29nit[30] = {0, 0, 0, 0, 0, 0,-8, 1, -10, -6, 1, -6, -5, 1, -3, -7, 1, -4, -1, 0, 0, 0, 0, -1, 0, 0, -1, 2, 0, 3};
static signed char HA3_ctbl30nit[30] = {0, 0, 0, 0, 0, 0,-6, 3, -11, -6, 1, -5, -5, 1, -2, -7, 1, -5, -1, 0, -1, 0, 0, -1, 1, 1, 0, 2, 0, 3};
static signed char HA3_ctbl32nit[30] = {0, 0, 0, 0, 0, 0,-7, 3, -11, -7, 0, -6, -5, 1, -2, -6, 1, -4, -1, 0, -1, 0, 0, -1, 0, 0, -1, 2, 0, 3};
static signed char HA3_ctbl34nit[30] = {0, 0, 0, 0, 0, 0,-10, 1, -12, -6, 1, -5, -6, 0, -2, -6, 1, -5, 0, 0, 0, 0, 0, -1, 0, 0, -1, 2, 0, 3};
static signed char HA3_ctbl37nit[30] = {0, 0, 0, 0, 0, 0,-10, 2, -14, -6, 0, -5, -4, 1, -2, -5, 1, -3, 0, 0, 0, 0, 0, -1, 0, 0, -1, 2, 0, 3};
static signed char HA3_ctbl39nit[30] = {0, 0, 0, 0, 0, 0,-10, 0, -12, -5, 2, -4, -4, 1, -2, -4, 1, -3, -1, 0, 0, 0, 0, -1, 0, 0, -1, 2, 0, 3};
static signed char HA3_ctbl41nit[30] = {0, 0, 0, 0, 0, 0,-4, 2, -10, -3, 1, -4, -3, 1, 0, -2, 1, -4, 0, 0, 1, 0, 0, -1, 0, 0, 0, 0, 0, 0};
static signed char HA3_ctbl44nit[30] = {0, 0, 0, 0, 0, 0,-11, 1, -13, -3, 1, -2, -5, 1, -3, -5, 1, -3, 0, 0, 0, 1, 0, 0, 0, 0, -1, 2, 0, 3};
static signed char HA3_ctbl47nit[30] = {0, 0, 0, 0, 0, 0,-11, 1, -13, -5, 0, -3, -6, 0, -4, -4, 1, -3, -1, 0, 0, 1, 0, 0, 0, 0, -1, 2, 0, 3};
static signed char HA3_ctbl50nit[30] = {0, 0, 0, 0, 0, 0,-10, 1, -13, -4, 1, -2, -3, 0, -2, -4, 1, -3, 0, 0, 0, 0, 0, -1, 1, 0, 0, 2, 0, 3};
static signed char HA3_ctbl53nit[30] = {0, 0, 0, 0, 0, 0,-10, 1, -12, -4, 1, -2, -3, 0, -2, -5, 0, -4, 0, 0, 0, 0, 0, -1, 1, 0, 0, 2, 0, 3};
static signed char HA3_ctbl56nit[30] = {0, 0, 0, 0, 0, 0,-12, 0, -13, -4, 0, -2, -3, 0, -2, -4, 0, -3, 0, 0, 0, 0, 0, -1, 1, 0, 0, 2, 0, 3};
static signed char HA3_ctbl60nit[30] = {0, 0, 0, 0, 0, 0,-12, 2, -13, -4, 0, -2, -2, 0, -1, -5, 0, -4, 1, 0, 1, 1, 0, -1, 1, 0, 0, 2, 0, 3};
static signed char HA3_ctbl64nit[30] = {0, 0, 0, 0, 0, 0,-11, 0, -12, -4, 0, -2, -3, 0, -2, -4, 0, -3, 1, 0, 1, 1, 0, -1, 1, 0, 0, 2, 0, 3};
static signed char HA3_ctbl68nit[30] = {0, 0, 0, 0, 0, 0,-12, 1, -13, -2, 1, -1, -2, 0, -1, -4, 0, -3, 1, 0, 1, 1, 0, -1, 1, 0, 0, 2, 0, 3};
static signed char HA3_ctbl72nit[30] = {0, 0, 0, 0, 0, 0,-13, 1, -13, -3, 0, -2, -3, 0, -1, -3, 0, -3, 1, 0, 1, 1, 0, -1, 1, 0, 0, 2, 0, 3};
static signed char HA3_ctbl77nit[30] = {0, 0, 0, 0, 0, 0,-12, 1, -12, -3, 0, -2, -1, 0, -1, -2, 0, -2, 1, 0, 1, 0, 0, 0, -1, 0, -1, 2, 0, 3};
static signed char HA3_ctbl82nit[30] = {0, 0, 0, 0, 0, 0,-11, 2, -11, -2, 0, -1, -2, 0, -1, -3, 0, -3, 1, 0, 1, 0, 0, 0, 1, 0, 0, 1, 0, 3};
static signed char HA3_ctbl87nit[30] = {0, 0, 0, 0, 0, 0,-12, 2, -12, -2, 0, -1, -2, 0, -1, -4, 0, -3, 1, 0, 1, 0, 0, -1, 1, 0, 0, 2, 0, 3};
static signed char HA3_ctbl93nit[30] = {0, 0, 0, 0, 0, 0,-5, 2, -7, -2, 0, -2, -1, 0, 1, -2, 0, -4, 1, 0, 1, 0, 0, 0, 1, 0, 1, 1, 0, 3};
static signed char HA3_ctbl98nit[30] = {0, 0, 0, 0, 0, 0,-10, 2, -9, -3, 0, -2, -2, 0, -1, -2, 0, -1, 1, 0, 2, 1, 0, 0, 0, 0, 0, 1, -1, 2};
static signed char HA3_ctbl105nit[30] = {0, 0, 0, 0, 0, 0,-12, 0, -10, -2, 0, -2, -2, 0, -1, -2, 0, -2, 1, 0, 1, 0, 0, -1, 0, 0, 0, 2, 0, 3};
static signed char HA3_ctbl111nit[30] = {0, 0, 0, 0, 0, 0,-12, 2, -8, -1, 0, -1, -1, 0, -1, -2, 0, -1, 1, 0, 1, -1, 0, -1, 0, 0, 0, 2, 0, 3};
static signed char HA3_ctbl119nit[30] = {0, 0, 0, 0, 0, 0,-12, 1, -9, -2, 0, -1, -1, 0, -1, -3, 0, -2, 1, 0, 1, 1, 0, 0, 0, 0, 0, 2, 0, 3};
static signed char HA3_ctbl126nit[30] = {0, 0, 0, 0, 0, 0,-11, 1, -7, -2, 0, -1, -2, 0, -1, -1, 0, -1, 1, 0, 1, 0, 0, -1, 0, 0, 0, 2, 0, 3};
static signed char HA3_ctbl134nit[30] = {0, 0, 0, 0, 0, 0,-10, 2, -6, -1, 0, 0, -2, 0, -1, -2, 0, -1, 1, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 2};
static signed char HA3_ctbl143nit[30] = {0, 0, 0, 0, 0, 0,-9, 1, -6, -1, 0, 0, -1, 0, -1, -1, 0, -1, 1, 0, 1, 0, 0, 0, 0, 0, 0, 1, 0, 2};
static signed char HA3_ctbl152nit[30] = {0, 0, 0, 0, 0, 0,-8, 2, -5, 0, 0, 0, -3, 0, -1, -1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 2};
static signed char HA3_ctbl162nit[30] = {0, 0, 0, 0, 0, 0,-10, 1, -6, 0, 0, 0, -2, 0, 0, -2, 0, -2, 1, 0, 1, 1, 0, 0, 0, 0, 0, 1, 0, 2};
static signed char HA3_ctbl172nit[30] = {0, 0, 0, 0, 0, 0,-10, 0, -4, -1, 0, -1, -2, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, -1, 0, 0, 1, 0, 1};
static signed char HA3_ctbl183nit[30] = {0, 0, 0, 0, 0, 0,-10, 0, -4, 0, 0, 0, -2, 0, 0, -1, 0, -1, 1, 0, 1, 0, 0, 0, 0, 0, 0, 1, 0, 1};
static signed char HA3_ctbl195nit[30] = {0, 0, 0, 0, 0, 0,-8, 1, -3, 0, 0, 0, -2, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1};
static signed char HA3_ctbl207nit[30] = {0, 0, 0, 0, 0, 0,-8, 0, -3, 1, 0, 0, -2, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1};
static signed char HA3_ctbl220nit[30] = {0, 0, 0, 0, 0, 0,-6, 0, -1, 0, 0, 0, -1, 0, -1, -1, 0, -1, 1, 0, 1, 0, 0, 1, 0, 0, 0, 1, 0, 1};
static signed char HA3_ctbl234nit[30] = {0, 0, 0, 0, 0, 0,-7, 0, -2, 1, 0, 1, -1, 0, -1, -1, 0, -1, 1, 0, 1, 0, 0, 1, 0, 0, 0, 1, 0, 1};
static signed char HA3_ctbl249nit[30] = {0, 0, 0, 0, 0, 0,-6, 0, 0, 1, 0, 0, 0, 0, 0, -1, 0, -1, 1, 0, 1, -1, 0, 0, 0, 0, 1, 1, 0, 1};
static signed char HA3_ctbl265nit[30] = {0, 0, 0, 0, 0, 0,-5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -1, 0, 0, 0, 0, 0, 1, 0, 1};
static signed char HA3_ctbl282nit[30] = {0, 0, 0, 0, 0, 0,-6, 0, 0, 1, 0, 0, -4, 0, -1, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1};
static signed char HA3_ctbl300nit[30] = {0, 0, 0, 0, 0, 0,-4, 0, 1, 0, 0, -1, -2, 0, 0, 0, 0, 0, 1, 0, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1};
static signed char HA3_ctbl316nit[30] = {0, 0, 0, 0, 0, 0,-4, 0, 1, 1, 0, 0, -2, 0, 0, 0, 0, -1, 0, 0, 0, 0, 0, 1, 1, 0, 0, 1, 0, 1};
static signed char HA3_ctbl333nit[30] = {0, 0, 0, 0, 0, 0,-6, 0, -1, -1, 0, -2, -1, 0, -2, 1, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 1};
static signed char HA3_ctbl350nit[30] = {0, 0, 0, 0, 0, 0,-3, 0, 1, 1, 0, 1, -2, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 0, -1, 0, 0, 1};
static signed char HA3_ctbl357nit[30] = {0, 0, 0, 0, 0, 0,-3, 0, 1, 1, 0, 1, -2, 0, -1, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 1};
static signed char HA3_ctbl365nit[30] = {0, 0, 0, 0, 0, 0,-3, 0, 1, 1, 0, 0, -3, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0};
static signed char HA3_ctbl372nit[30] = {0, 0, 0, 0, 0, 0,-3, 0, 1, 1, 0, 0, -2, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0};
static signed char HA3_ctbl380nit[30] = {0, 0, 0, 0, 0, 0,-5, 0, -1, 0, 0, -1, 0, 0, -2, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0};
static signed char HA3_ctbl387nit[30] = {0, 0, 0, 0, 0, 0,-5, 0, 0, -1, 0, -2, 0, 0, -1, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0};
static signed char HA3_ctbl395nit[30] = {0, 0, 0, 0, 0, 0,-2, 0, 0, -1, 0, -2, 0, 0, -2, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0};
static signed char HA3_ctbl403nit[30] = {0, 0, 0, 0, 0, 0,0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static signed char HA3_ctbl412nit[30] = {0, 0, 0, 0, 0, 0,0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static signed char HA3_ctbl420nit[30] = {0, 0, 0, 0, 0, 0,0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};


static signed char HA3_aid9818[3] = {0xB1, 0x90, 0xE5};
static signed char HA3_aid9740[3] = {0xB1, 0x90, 0xD1};
static signed char HA3_aid9655[3] = {0xB1, 0x90, 0xBB};
static signed char HA3_aid9550[3] = {0xB1, 0x90, 0xA0};
static signed char HA3_aid9434[3] = {0xB1, 0x90, 0x82};
static signed char HA3_aid9372[3] = {0xB1, 0x90, 0x72};
static signed char HA3_aid9287[3] = {0xB1, 0x90, 0x5C};
static signed char HA3_aid9186[3] = {0xB1, 0x90, 0x42};
static signed char HA3_aid9124[3] = {0xB1, 0x90, 0x32};
static signed char HA3_aid9058[3] = {0xB1, 0x90, 0x21};
static signed char HA3_aid8996[3] = {0xB1, 0x90, 0x11};
static signed char HA3_aid8891[3] = {0xB1, 0x80, 0xF6};
static signed char HA3_aid8826[3] = {0xB1, 0x80, 0xE5};
static signed char HA3_aid8764[3] = {0xB1, 0x80, 0xD5};
static signed char HA3_aid8686[3] = {0xB1, 0x80, 0xC1};
static signed char HA3_aid8601[3] = {0xB1, 0x80, 0xAB};
static signed char HA3_aid8434[3] = {0xB1, 0x80, 0x80};
static signed char HA3_aid8341[3] = {0xB1, 0x80, 0x68};
static signed char HA3_aid8256[3] = {0xB1, 0x80, 0x52};
static signed char HA3_aid8182[3] = {0xB1, 0x80, 0x3F};
static signed char HA3_aid8008[3] = {0xB1, 0x80, 0x12};
static signed char HA3_aid7922[3] = {0xB1, 0x70, 0xFC};
static signed char HA3_aid7756[3] = {0xB1, 0x70, 0xD1};
static signed char HA3_aid7609[3] = {0xB1, 0x70, 0xAB};
static signed char HA3_aid7519[3] = {0xB1, 0x70, 0x94};
static signed char HA3_aid7364[3] = {0xB1, 0x70, 0x6C};
static signed char HA3_aid7198[3] = {0xB1, 0x70, 0x41};
static signed char HA3_aid6934[3] = {0xB1, 0x60, 0xFD};
static signed char HA3_aid6764[3] = {0xB1, 0x60, 0xD1};
static signed char HA3_aid6581[3] = {0xB1, 0x60, 0xA2};
static signed char HA3_aid6329[3] = {0xB1, 0x60, 0x61};
static signed char HA3_aid6070[3] = {0xB1, 0x60, 0x1E};
static signed char HA3_aid5791[3] = {0xB1, 0x50, 0xD6};
static signed char HA3_aid5531[3] = {0xB1, 0x50, 0x93};
static signed char HA3_aid5260[3] = {0xB1, 0x50, 0x4D};
static signed char HA3_aid4907[3] = {0xB1, 0x40, 0xF2};
static signed char HA3_aid4543[3] = {0xB1, 0x40, 0x94};
static signed char HA3_aid4178[3] = {0xB1, 0x40, 0x36};
static signed char HA3_aid3802[3] = {0xB1, 0x30, 0xD5};
static signed char HA3_aid3764[3] = {0xB1, 0x30, 0xCB};
static signed char HA3_aid3845[3] = {0xB1, 0x30, 0xE0};
static signed char HA3_aid3771[3] = {0xB1, 0x30, 0xCD};
static signed char HA3_aid3783[3] = {0xB1, 0x30, 0xD0};
static signed char HA3_aid3779[3] = {0xB1, 0x30, 0xCF};
static signed char HA3_aid3806[3] = {0xB1, 0x30, 0xD6};
static signed char HA3_aid3295[3] = {0xB1, 0x30, 0x52};
static signed char HA3_aid2876[3] = {0xB1, 0x20, 0xE6};
static signed char HA3_aid2302[3] = {0xB1, 0x20, 0x52};
static signed char HA3_aid1736[3] = {0xB1, 0x10, 0xC0};
static signed char HA3_aid1081[3] = {0xB1, 0x10, 0x17};
static signed char HA3_aid1004[3] = {0xB1, 0x10, 0x03};
static signed char HA3_aid744[3] = {0xB1, 0x00, 0xC0};
static signed char HA3_aid574[3] = {0xB1, 0x00, 0x94};
static signed char HA3_aid399[3] = {0xB1, 0x00, 0x67};
static signed char HA3_aid132[3] = {0xB1, 0x00, 0x22};
static signed char HA3_aid39[3] = {0xB1, 0x00, 0x0A};



static  unsigned char HA3_elv1[3] = {
        0xB5, 0xAC, 0x17
};
static  unsigned char HA3_elv2[3] = {
        0xB5, 0xAC, 0x16
};
static  unsigned char HA3_elv3[3] = {
        0xB5, 0xAC, 0x15
};
static  unsigned char HA3_elv4[3] = {
        0xB5, 0xAC, 0x14
};
static  unsigned char HA3_elv5[3] = {
        0xB5, 0xAC, 0x13
};
static  unsigned char HA3_elv6[3] = {
        0xB5, 0xAC, 0x12
};
static  unsigned char HA3_elv7[3] = {
        0xB5, 0xAC, 0x11
};
static  unsigned char HA3_elv8[3] = {
        0xB5, 0xAC, 0x10
};
static  unsigned char HA3_elv9[3] = {
        0xB5, 0xAC, 0x0F
};
static  unsigned char HA3_elv10[3] = {
        0xB5, 0xAC, 0x0E
};
static  unsigned char HA3_elv11[3] = {
        0xB5, 0xAC, 0x0D
};
static  unsigned char HA3_elv12[3] = {
        0xB5, 0xAC, 0x0C
};
static	unsigned char HA3_elv13[3] = {
		0xB5, 0xAC, 0x0B
};
static  unsigned char HA3_elv14[3] = {
        0xB5, 0xAC, 0x0A
};
static  unsigned char HA3_elv15[3] = {
        0xB5, 0xAC, 0x08
};
static  unsigned char HA3_elv16[3] = {
        0xB5, 0xAC, 0x07
};
static  unsigned char HA3_elv17[3] = {
        0xB5, 0xAC, 0x05
};
static  unsigned char HA3_elv18[3] = {
        0xB5, 0xAC, 0x0A
};


static  unsigned char HA3_elvCaps1[3] = {
        0xB5, 0xBC, 0x17
};
static  unsigned char HA3_elvCaps2[3] = {
        0xB5, 0xBC, 0x16
};
static  unsigned char HA3_elvCaps3[3] = {
        0xB5, 0xBC, 0x15
};
static  unsigned char HA3_elvCaps4[3] = {
        0xB5, 0xBC, 0x14
};
static  unsigned char HA3_elvCaps5[3] = {
        0xB5, 0xBC, 0x13
};
static  unsigned char HA3_elvCaps6[3] = {
        0xB5, 0xBC, 0x12
};
static  unsigned char HA3_elvCaps7[3] = {
        0xB5, 0xBC, 0x11
};
static  unsigned char HA3_elvCaps8[3] = {
        0xB5, 0xBC, 0x10
};
static  unsigned char HA3_elvCaps9[3] = {
        0xB5, 0xBC, 0x0F
};
static  unsigned char HA3_elvCaps10[3] = {
        0xB5, 0xBC, 0x0E
};
static  unsigned char HA3_elvCaps11[3] = {
        0xB5, 0xBC, 0x0D
};
static  unsigned char HA3_elvCaps12[3] = {
        0xB5, 0xBC, 0x0C
};
static	unsigned char HA3_elvCaps13[3] = {
		0xB5, 0xBC, 0x0B
};
static  unsigned char HA3_elvCaps14[3] = {
        0xB5, 0xBC, 0x0A
};
static  unsigned char HA3_elvCaps15[3] = {
        0xB5, 0xBC, 0x08
};
static  unsigned char HA3_elvCaps16[3] = {
        0xB5, 0xBC, 0x07
};
static  unsigned char HA3_elvCaps17[3] = {
        0xB5, 0xBC, 0x05
};
static  unsigned char HA3_elvCaps18[3] = {
        0xB5, 0xBC, 0x0A
};


#ifdef CONFIG_LCD_HMT
static signed char HA3_HMTrtbl10nit[10] = {0, 9, 4, 3, 3, 2, 1, 0, 1, 0};
static signed char HA3_HMTrtbl11nit[10] = {0, 8, 4, 3, 2, 2, 2, 1, 1, 0};
static signed char HA3_HMTrtbl12nit[10] = {0, 4, 5, 3, 3, 2, 1, 2, 1, 0};
static signed char HA3_HMTrtbl13nit[10] = {0, 7, 5, 4, 4, 2, 1, 2, 0, 0};
static signed char HA3_HMTrtbl14nit[10] = {0, 7, 5, 4, 3, 2, 2, 3, 1, 0};
static signed char HA3_HMTrtbl15nit[10] = {0, 9, 5, 4, 3, 2, 2, 3, 0, 0};
static signed char HA3_HMTrtbl16nit[10] = {0, 8, 5, 4, 2, 2, 2, 3, 1, 0};
static signed char HA3_HMTrtbl17nit[10] = {0, 7, 5, 4, 3, 2, 2, 3, 1, 0};
static signed char HA3_HMTrtbl19nit[10] = {0, 8, 4, 3, 2, 3, 2, 3, 2, 0};
static signed char HA3_HMTrtbl20nit[10] = {0, 8, 4, 4, 3, 2, 2, 3, 2, 0};
static signed char HA3_HMTrtbl21nit[10] = {0, 10, 4, 4, 3, 2, 2, 3, 2, 0};
static signed char HA3_HMTrtbl22nit[10] = {0, 5, 5, 4, 2, 2, 0, 2, 2, 0};
static signed char HA3_HMTrtbl23nit[10] = {0, 6, 5, 3, 3, 2, 0, 2, 2, 0};
static signed char HA3_HMTrtbl25nit[10] = {0, 7, 5, 4, 2, 3, 1, 2, 2, 0};
static signed char HA3_HMTrtbl27nit[10] = {0, 8, 4, 4, 2, 3, 1, 3, 3, 0};
static signed char HA3_HMTrtbl29nit[10] = {0, 11, 4, 4, 2, 3, 1, 3, 4, 0};
static signed char HA3_HMTrtbl31nit[10] = {0, 15, 4, 4, 3, 2, 1, 4, 4, 0};
static signed char HA3_HMTrtbl33nit[10] = {0, 6, 5, 3, 2, 2, 2, 4, 5, 0};
static signed char HA3_HMTrtbl35nit[10] = {0, 6, 5, 4, 3, 3, 3, 5, 4, 0};
static signed char HA3_HMTrtbl37nit[10] = {0, 7, 5, 4, 3, 3, 2, 5, 4, 0};
static signed char HA3_HMTrtbl39nit[10] = {0, 8, 4, 4, 4, 3, 3, 6, 4, 0};
static signed char HA3_HMTrtbl41nit[10] = {0, 5, 4, 3, 2, 1, 3, 4, 4, 0};
static signed char HA3_HMTrtbl44nit[10] = {0, 6, 4, 4, 3, 2, 4, 5, 4, 0};
static signed char HA3_HMTrtbl47nit[10] = {0, 6, 4, 3, 3, 1, 3, 5, 3, 0};
static signed char HA3_HMTrtbl50nit[10] = {0, 8, 4, 3, 3, 2, 4, 5, 4, 0};
static signed char HA3_HMTrtbl53nit[10] = {0, 5, 4, 3, 3, 2, 5, 6, 4, 0};
static signed char HA3_HMTrtbl56nit[10] = {0, 6, 4, 3, 3, 3, 4, 5, 3, 0};
static signed char HA3_HMTrtbl60nit[10] = {0, 7, 4, 3, 3, 3, 5, 6, 4, 0};
static signed char HA3_HMTrtbl64nit[10] = {0, 7, 4, 3, 2, 3, 5, 7, 4, 0};
static signed char HA3_HMTrtbl68nit[10] = {0, 5, 5, 3, 3, 3, 5, 8, 5, 0};
static signed char HA3_HMTrtbl72nit[10] = {0, 4, 4, 3, 3, 3, 5, 7, 4, 0};
static signed char HA3_HMTrtbl77nit[10] = {0, 2, 2, 2, 2, 2, 3, 4, 2, 0};
static signed char HA3_HMTrtbl82nit[10] = {0, 3, 2, 2, 2, 2, 2, 5, 2, 0};
static signed char HA3_HMTrtbl87nit[10] = {0, 4, 2, 1, 2, 1, 3, 5, 3, 0};
static signed char HA3_HMTrtbl93nit[10] = {0, 5, 2, 2, 2, 1, 3, 5, 3, 0};
static signed char HA3_HMTrtbl99nit[10] = {0, 1, 2, 1, 2, 2, 3, 5, 3, 0};
static signed char HA3_HMTrtbl105nit[10] = {0, 4, 2, 2, 3, 2, 4, 6, 4, 0};

static signed char HA3_HMTctbl10nit[30] = {0, 0, 0, 0, 0, 0,-22, 0, 0, -7, 1, -4, -2, 1, -2, -3, 2, -5, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static signed char HA3_HMTctbl11nit[30] = {0, 0, 0, 0, 0, 0,-20, 0, 0, -6, 1, -4, -2, 1, -2, -2, 2, -4, 0, 0, -1, 1, 0, 0, 0, 0, 0, 0, 0, 0};
static signed char HA3_HMTctbl12nit[30] = {0, 0, 0, 0, 0, 0,-14, 2, -5, -5, 2, -5, -2, 1, -3, -3, 2, -4, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static signed char HA3_HMTctbl13nit[30] = {0, 0, 0, 0, 0, 0,-15, 2, -6, -5, 2, -5, -2, 1, -4, -4, 1, -4, -1, 0, -1, 0, 0, -1, 0, 0, 0, 0, 0, 0};
static signed char HA3_HMTctbl14nit[30] = {0, 0, 0, 0, 0, 0,-17, 2, -6, -4, 2, -5, -3, 1, -3, -3, 1, -3, 0, 0, 0, 0, 0, -1, 0, 0, 0, 0, 0, 0};
static signed char HA3_HMTctbl15nit[30] = {0, 0, 0, 0, 0, 0,-17, 3, -8, -3, 1, -3, -3, 1, -4, -3, 1, -4, 0, 0, -1, -1, 0, -1, 0, 0, 0, 0, 0, 0};
static signed char HA3_HMTctbl16nit[30] = {0, 0, 0, 0, 0, 0,-15, 3, -7, -4, 1, -4, -3, 1, -4, -3, 1, -4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static signed char HA3_HMTctbl17nit[30] = {0, 0, 0, 0, 0, 0,-13, 3, -7, -3, 2, -4, -3, 1, -4, -2, 1, -3, 0, 0, 0, 0, 0, -1, 0, 0, 0, 0, 0, 0};
static signed char HA3_HMTctbl19nit[30] = {0, 0, 0, 0, 0, 0,-13, 3, -7, -4, 2, -5, -2, 1, -3, -1, 1, -3, 0, 0, 0, 0, 0, -1, 0, 0, 0, 0, 0, 0};
static signed char HA3_HMTctbl20nit[30] = {0, 0, 0, 0, 0, 0,-14, 3, -8, -3, 1, -4, -3, 1, -4, -2, 1, -3, -1, 0, -1, 0, 0, -1, 0, 0, 0, -1, 0, 0};
static signed char HA3_HMTctbl21nit[30] = {0, 0, 0, 0, 0, 0,-16, 4, -8, -3, 1, -4, -3, 1, -4, -2, 1, -3, 0, 0, -1, 0, 0, 0, 0, 0, 0, -1, 0, 0};
static signed char HA3_HMTctbl22nit[30] = {0, 0, 0, 0, 0, 0,-13, 3, -7, -4, 2, -4, -3, 2, -4, -2, 1, -3, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static signed char HA3_HMTctbl23nit[30] = {0, 0, 0, 0, 0, 0,-14, 3, -7, -3, 2, -4, -3, 1, -3, -2, 1, -3, 0, 0, -1, 0, 0, 0, -1, 0, 0, 0, 0, 0};
static signed char HA3_HMTctbl25nit[30] = {0, 0, 0, 0, 0, 0,-13, 4, -8, -3, 1, -3, -3, 1, -4, -1, 1, -2, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0, 1};
static signed char HA3_HMTctbl27nit[30] = {0, 0, 0, 0, 0, 0,-14, 4, -8, -3, 1, -4, -3, 1, -3, -1, 1, -2, 0, 0, -1, 0, 0, -1, 0, 0, 0, 0, 0, 1};
static signed char HA3_HMTctbl29nit[30] = {0, 0, 0, 0, 0, 0,-16, 3, -8, -3, 1, -4, -3, 1, -3, -2, 1, -2, -1, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static signed char HA3_HMTctbl31nit[30] = {0, 0, 0, 0, 0, 0,-15, 3, -8, -2, 2, -4, -2, 1, -3, -1, 0, -2, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0, 1};
static signed char HA3_HMTctbl33nit[30] = {0, 0, 0, 0, 0, 0,-11, 3, -7, -2, 1, -4, -4, 1, -4, -1, 0, -2, 0, 0, 0, 1, 0, -1, 0, 0, 0, 0, 0, 1};
static signed char HA3_HMTctbl35nit[30] = {0, 0, 0, 0, 0, 0,-11, 4, -8, -2, 1, -3, -3, 1, -3, -1, 0, -2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static signed char HA3_HMTctbl37nit[30] = {0, 0, 0, 0, 0, 0,-11, 4, -8, -3, 1, -4, -3, 1, -3, -2, 0, -2, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0, 1};
static signed char HA3_HMTctbl39nit[30] = {0, 0, 0, 0, 0, 0,-12, 4, -8, -3, 2, -4, -2, 1, -2, -1, 0, -2, -1, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static signed char HA3_HMTctbl41nit[30] = {0, 0, 0, 0, 0, 0,-11, 3, -8, -3, 1, -4, -1, 1, -2, -2, 0, -2, 0, 0, 0, 0, 0, -1, 1, 0, 0, 0, 0, 2};
static signed char HA3_HMTctbl44nit[30] = {0, 0, 0, 0, 0, 0,-9, 3, -8, -2, 2, -4, -2, 1, -2, -1, 0, -2, 0, 0, 0, 0, 0, -1, 0, 0, -1, 0, 0, 2};
static signed char HA3_HMTctbl47nit[30] = {0, 0, 0, 0, 0, 0,-9, 3, -8, -2, 1, -4, -2, 1, -2, -1, 0, -1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 2};
static signed char HA3_HMTctbl50nit[30] = {0, 0, 0, 0, 0, 0,-9, 3, -8, -3, 1, -4, -2, 1, -2, -2, 0, -2, 0, 0, 0, 0, 0, -1, 0, 0, 0, 1, 0, 2};
static signed char HA3_HMTctbl53nit[30] = {0, 0, 0, 0, 0, 0,-9, 3, -8, -3, 1, -4, -1, 1, -2, -1, 0, -1, 0, 0, 0, 0, 0, -1, 0, 0, 0, 0, 0, 2};
static signed char HA3_HMTctbl56nit[30] = {0, 0, 0, 0, 0, 0,-8, 3, -8, -3, 1, -4, -2, 0, -2, -3, 0, -2, 0, 0, -1, 0, 0, -1, 0, 0, 0, 1, 0, 2};
static signed char HA3_HMTctbl60nit[30] = {0, 0, 0, 0, 0, 0,-8, 4, -8, -2, 1, -3, -1, 0, -2, -2, 0, -1, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0, 2};
static signed char HA3_HMTctbl64nit[30] = {0, 0, 0, 0, 0, 0,-8, 4, -8, -2, 1, -4, -2, 1, -2, -1, 0, -1, 0, 0, -1, 0, 0, -1, 0, 0, 0, 1, 0, 2};
static signed char HA3_HMTctbl68nit[30] = {0, 0, 0, 0, 0, 0,-6, 3, -8, -2, 1, -3, -1, 0, -1, -3, 0, -2, 0, 0, -1, 0, 0, -1, 0, 0, 0, 1, 0, 2};
static signed char HA3_HMTctbl72nit[30] = {0, 0, 0, 0, 0, 0,-5, 3, -7, -2, 1, -3, -1, 0, -1, -2, 0, -2, 0, 0, 0, 0, 0, -1, 0, 0, 0, 2, 0, 3};
static signed char HA3_HMTctbl77nit[30] = {0, 0, 0, 0, 0, 0,-1, 2, -4, -1, 1, -2, 0, 1, -2, -1, 0, -2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2};
static signed char HA3_HMTctbl82nit[30] = {0, 0, 0, 0, 0, 0,-3, 2, -5, -1, 1, -2, 0, 0, -2, -1, 0, -2, 0, 0, 0, 1, 0, -1, 0, 0, 1, 1, 0, 2};
static signed char HA3_HMTctbl87nit[30] = {0, 0, 0, 0, 0, 0,-3, 2, -5, 0, 1, -2, 0, 0, -2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2};
static signed char HA3_HMTctbl93nit[30] = {0, 0, 0, 0, 0, 0,-3, 2, -5, -1, 0, -2, 0, 1, -2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 2};
static signed char HA3_HMTctbl99nit[30] = {0, 0, 0, 0, 0, 0,-2, 2, -4, -1, 0, -2, -1, 1, -2, 0, 0, -1, 0, 0, -1, 1, 0, 0, 0, 0, 0, 1, 0, 3};
static signed char HA3_HMTctbl105nit[30] = {0, 0, 0, 0, 0, 0,-3, 2, -5, -2, 0, -2, -1, 1, -2, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 2};

static signed char HA3_HMTaid8004[3] = {0xB1, 0x20, 0x03};
static signed char HA3_HMTaid7004[3] = {0xB1, 0X30, 0x05};

static unsigned char HA3_HMTelv[3] = {
        0xB5, 0xAC, 0x0A
};
static unsigned char HA3_HMTelvCaps[3] = {
        0xB5, 0xBC, 0x0A
};

#endif

/********************************************** HF3 **********************************************/

// start of aid sheet in rev.b opmanual
static signed char HF3_rtbl2nit[10] = {0, 32, 34, 35, 32, 28, 24, 16, 10, 0};
static signed char HF3_rtbl3nit[10] = {0, 32, 34, 31, 28, 24, 20, 14, 9, 0};
static signed char HF3_rtbl4nit[10] = {0, 28, 27, 24, 22, 20, 17, 12, 8, 0};
static signed char HF3_rtbl5nit[10] = {0, 28, 27, 24, 22, 20, 17, 12, 8, 0};
static signed char HF3_rtbl6nit[10] = {0, 29, 26, 24, 21, 19, 17, 11, 8, 0};
static signed char HF3_rtbl7nit[10] = {0, 23, 24, 22, 20, 18, 16, 10, 7, 0};
static signed char HF3_rtbl8nit[10] = {0, 25, 23, 20, 19, 17, 15, 10, 7, 0};
static signed char HF3_rtbl9nit[10] = {0, 25, 21, 18, 17, 15, 14, 9, 7, 0};
static signed char HF3_rtbl10nit[10] = {0, 22, 19, 16, 15, 13, 12, 9, 7, 0};
static signed char HF3_rtbl11nit[10] = {0, 17, 18, 16, 15, 13, 12, 8, 7, 0};
static signed char HF3_rtbl12nit[10] = {0, 18, 18, 15, 14, 12, 11, 8, 7, 0};
static signed char HF3_rtbl13nit[10] = {0, 19, 18, 16, 15, 12, 11, 8, 7, 0};
static signed char HF3_rtbl14nit[10] = {0, 20, 16, 14, 13, 11, 9, 6, 5, 0};
static signed char HF3_rtbl15nit[10] = {0, 14, 15, 13, 12, 10, 9, 6, 5, 0};
static signed char HF3_rtbl16nit[10] = {0, 16, 15, 13, 12, 10, 9, 6, 5, 0};
static signed char HF3_rtbl17nit[10] = {0, 18, 14, 12, 11, 9, 8, 6, 5, 0};
static signed char HF3_rtbl19nit[10] = {0, 12, 13, 11, 10, 8, 8, 6, 5, 0};
static signed char HF3_rtbl20nit[10] = {0, 14, 13, 10, 10, 8, 8, 5, 5, 0};
static signed char HF3_rtbl21nit[10] = {0, 11, 12, 10, 10, 8, 7, 5, 5, 0};
static signed char HF3_rtbl22nit[10] = {0, 16, 12, 9, 10, 8, 7, 5, 5, 0};
static signed char HF3_rtbl24nit[10] = {0, 12, 12, 9, 9, 7, 7, 5, 5, 0};
static signed char HF3_rtbl25nit[10] = {0, 10, 11, 9, 9, 7, 7, 5, 5, 0};
static signed char HF3_rtbl27nit[10] = {0, 9, 10, 8, 8, 7, 6, 4, 4, 0};
static signed char HF3_rtbl29nit[10] = {0, 13, 10, 8, 8, 6, 6, 4, 4, 0};
static signed char HF3_rtbl30nit[10] = {0, 10, 10, 8, 8, 6, 6, 4, 4, 0};
static signed char HF3_rtbl32nit[10] = {0, 8, 9, 7, 7, 6, 5, 4, 4, 0};
static signed char HF3_rtbl34nit[10] = {0, 11, 9, 7, 7, 5, 5, 4, 4, 0};
static signed char HF3_rtbl37nit[10] = {0, 12, 8, 6, 7, 5, 5, 4, 4, 0};
static signed char HF3_rtbl39nit[10] = {0, 9, 8, 6, 6, 5, 5, 3, 4, 0};
static signed char HF3_rtbl41nit[10] = {0, 11, 7, 5, 6, 4, 4, 3, 4, 0};
static signed char HF3_rtbl44nit[10] = {0, 9, 7, 5, 5, 4, 4, 3, 4, 0};
static signed char HF3_rtbl47nit[10] = {0, 8, 7, 5, 5, 4, 4, 3, 4, 0};
static signed char HF3_rtbl50nit[10] = {0, 8, 6, 5, 5, 4, 3, 3, 4, 0};
static signed char HF3_rtbl53nit[10] = {0, 8, 6, 4, 5, 4, 3, 3, 4, 0};
static signed char HF3_rtbl56nit[10] = {0, 9, 5, 4, 4, 3, 3, 2, 4, 0};
static signed char HF3_rtbl60nit[10] = {0, 8, 5, 4, 4, 3, 3, 2, 3, 0};
static signed char HF3_rtbl64nit[10] = {0, 3, 4, 3, 3, 3, 2, 2, 3, 0};
static signed char HF3_rtbl68nit[10] = {0, 7, 5, 4, 3, 3, 3, 2, 3, 0};
static signed char HF3_rtbl72nit[10] = {0, 8, 5, 4, 3, 3, 4, 2, 3, 0};
static signed char HF3_rtbl77nit[10] = {0, 8, 5, 3, 3, 3, 3, 3, 4, 0};
static signed char HF3_rtbl82nit[10] = {0, 8, 5, 4, 3, 3, 4, 3, 3, 0};
static signed char HF3_rtbl87nit[10] = {0, 8, 5, 4, 3, 3, 4, 4, 4, 0};
static signed char HF3_rtbl93nit[10] = {0, 8, 4, 3, 3, 3, 4, 3, 3, 0};
static signed char HF3_rtbl98nit[10] = {0, 8, 4, 4, 4, 4, 5, 4, 4, 0};
static signed char HF3_rtbl105nit[10] = {0, 8, 4, 3, 4, 4, 5, 5, 3, 0};
static signed char HF3_rtbl111nit[10] = {0, 8, 4, 3, 3, 3, 4, 4, 3, 0};
static signed char HF3_rtbl119nit[10] = {0, 7, 4, 3, 4, 4, 5, 5, 3, 0};
static signed char HF3_rtbl126nit[10] = {0, 9, 3, 3, 3, 3, 4, 5, 2, 0};
static signed char HF3_rtbl134nit[10] = {0, 8, 4, 4, 3, 4, 5, 5, 2, 0};
static signed char HF3_rtbl143nit[10] = {0, 8, 4, 3, 4, 4, 5, 5, 3, 0};
static signed char HF3_rtbl152nit[10] = {0, 7, 4, 3, 3, 3, 4, 6, 4, 0};
static signed char HF3_rtbl162nit[10] = {0, 7, 3, 2, 3, 3, 5, 6, 3, 0};
static signed char HF3_rtbl172nit[10] = {0, 7, 2, 2, 2, 3, 5, 5, 3, 0};
static signed char HF3_rtbl183nit[10] = {0, 7, 3, 3, 4, 5, 6, 6, 4, 0};
static signed char HF3_rtbl195nit[10] = {0, 6, 2, 2, 4, 5, 5, 6, 3, 0};
static signed char HF3_rtbl207nit[10] = {0, 7, 2, 2, 3, 5, 4, 6, 3, 0};
static signed char HF3_rtbl220nit[10] = {0, 6, 2, 2, 3, 5, 4, 5, 3, 0};
static signed char HF3_rtbl234nit[10] = {0, 5, 2, 2, 3, 5, 5, 5, 3, 0};
static signed char HF3_rtbl249nit[10] = {0, 5, 2, 2, 2, 3, 4, 4, 2, 0};
static signed char HF3_rtbl265nit[10] = {0, 5, 1, 2, 2, 3, 3, 3, 2, 0};
static signed char HF3_rtbl282nit[10] = {0, 5, 2, 2, 2, 3, 3, 3, 2, 0};
static signed char HF3_rtbl300nit[10] = {0, 5, 1, 1, 2, 3, 3, 3, 1, 0};
static signed char HF3_rtbl316nit[10] = {0, 5, 1, 2, 2, 3, 3, 3, 1, 0};
static signed char HF3_rtbl333nit[10] = {0, 5, 1, 1, 2, 3, 2, 3, 1, 0};
static signed char HF3_rtbl350nit[10] = {0, 5, 1, 2, 2, 3, 1, 2, 1, 0};
static signed char HF3_rtbl357nit[10] = {0, 5, 2, 2, 2, 3, 1, 2, 1, 0};
static signed char HF3_rtbl365nit[10] = {0, 5, 1, 2, 2, 3, 2, 3, 1, 0};
static signed char HF3_rtbl372nit[10] = {0, 5, 1, 2, 2, 3, 2, 3, 1, 0};
static signed char HF3_rtbl380nit[10] = {0, 5, 1, 1, 2, 3, 2, 3, 1, 0};
static signed char HF3_rtbl387nit[10] = {0, 5, 1, 1, 2, 3, 2, 3, 1, 0};
static signed char HF3_rtbl395nit[10] = {0, 5, 1, 1, 2, 3, 2, 3, 1, 0};
static signed char HF3_rtbl403nit[10] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static signed char HF3_rtbl412nit[10] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static signed char HF3_rtbl420nit[10] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};


static signed char HF3_ctbl2nit[30] = {0, 0, 0, 0, 0, 0,-8, 2, -5, -8, 0, -5, -8, 1, -4, -13, 2, -6, -9, 0, -6, -4, 0, -3, -3, 1, -2, -6, 3, -5};
static signed char HF3_ctbl3nit[30] = {0, 0, 0, 0, 0, 0,-8, 2, -5, -8, 0, -5, -9, 0, -4, -11, 0, -7, -9, 0, -6, -2, 0, -2, -3, 1, -2, -4, 3, -3};
static signed char HF3_ctbl4nit[30] = {0, 0, 0, 0, 0, 0,-8, 2, -5, -8, 1, -7, -10, 1, -6, -11, 1, -8, -8, 0, -7, -2, 0, -2, -3, 1, -2, -2, 3, -1};
static signed char HF3_ctbl5nit[30] = {0, 0, 0, 0, 0, 0,-8, 3, -7, -6, 1, -6, -10, 0, -7, -10, 0, -8, -7, 0, -7, -2, 0, -2, -3, 0, -2, -2, 3, -1};
static signed char HF3_ctbl6nit[30] = {0, 0, 0, 0, 0, 0,-8, 3, -9, -6, 1, -6, -10, 0, -9, -9, 0, -8, -8, 0, -8, -2, 0, -2, -2, 0, -1, -2, 2, -1};
static signed char HF3_ctbl7nit[30] = {0, 0, 0, 0, 0, 0,-8, 3, -9, -6, 1, -6, -10, 0, -9, -8, 0, -8, -8, 0, -8, -1, 0, -1, -2, 0, -1, -1, 2, 0};
static signed char HF3_ctbl8nit[30] = {0, 0, 0, 0, 0, 0,-8, 2, -9, -6, 3, -5, -7, 0, -8, -8, 0, -8, -7, 0, -7, -1, 0, -1, -2, 0, -1, -1, 1, 0};
static signed char HF3_ctbl9nit[30] = {0, 0, 0, 0, 0, 0,-8, 2, -7, -8, 2, -9, -7, 0, -8, -8, 0, -8, -7, 0, -7, -1, 0, -1, -2, 0, -1, 0, 1, 1};
static signed char HF3_ctbl10nit[30] = {0, 0, 0, 0, 0, 0,-8, 2, -7, -10, 2, -11, -6, 0, -8, -7, 0, -8, -6, 0, -6, -1, 0, -1, -2, 0, -1, -1, 0, 0};
static signed char HF3_ctbl11nit[30] = {0, 0, 0, 0, 0, 0,-6, 2, -7, -8, 2, -10, -5, 0, -7, -7, 0, -8, -6, 0, -6, -1, 0, -1, -2, 0, -1, -1, 0, 0};
static signed char HF3_ctbl12nit[30] = {0, 0, 0, 0, 0, 0,-6, 2, -7, -9, 2, -12, -6, 0, -8, -8, 0, -9, -5, 0, -5, -2, 0, -2, -2, 0, -1, 1, 0, 2};
static signed char HF3_ctbl13nit[30] = {0, 0, 0, 0, 0, 0,-6, 2, -9, -8, 1, -10, -5, 0, -6, -8, 0, -9, -4, 0, -5, -2, 0, -1, -2, 0, -1, 1, 0, 2};
static signed char HF3_ctbl14nit[30] = {0, 0, 0, 0, 0, 0,-6, 2, -9, -8, 1, -12, -5, 0, -8, -8, 0, -7, -4, 0, -5, -1, 0, 0, 0, 0, 1, 1, 0, 2};
static signed char HF3_ctbl15nit[30] = {0, 0, 0, 0, 0, 0,-6, 3, -9, -7, 1, -11, -7, 0, -9, -8, 0, -7, -4, 0, -5, 0, 0, 1, 0, 0, 1, 1, 0, 2};
static signed char HF3_ctbl16nit[30] = {0, 0, 0, 0, 0, 0,-6, 3, -11, -5, 1, -10, -7, 0, -8, -7, 0, -6, -4, 0, -5, 0, 0, 1, 0, 0, 1, 1, 0, 2};
static signed char HF3_ctbl17nit[30] = {0, 0, 0, 0, 0, 0,-3, 0, -9, -8, 1, -12, -7, 0, -8, -6, 0, -8, -4, 0, -5, 0, 0, 1, 0, 0, 1, 1, 0, 2};
static signed char HF3_ctbl19nit[30] = {0, 0, 0, 0, 0, 0,-6, 5, -11, -6, 1, -11, -8, 0, -8, -5, 0, -7, -4, 0, -4, 0, 0, 1, 0, 0, 0, 2, 0, 3};
static signed char HF3_ctbl20nit[30] = {0, 0, 0, 0, 0, 0,-6, 5, -12, -6, 3, -11, -7, 0, -7, -5, 0, -7, -4, 0, -4, 0, 0, 1, 0, 0, 0, 2, 0, 3};
static signed char HF3_ctbl21nit[30] = {0, 0, 0, 0, 0, 0,-6, 5, -12, -6, 3, -11, -7, 0, -7, -4, 0, -5, -4, 0, -4, 0, 0, 1, 0, 0, 0, 3, 0, 4};
static signed char HF3_ctbl22nit[30] = {0, 0, 0, 0, 0, 0,-6, 5, -12, -6, 3, -11, -6, 0, -6, -4, 0, -5, -4, 0, -4, 0, 0, 1, 0, 0, 0, 3, 0, 4};
static signed char HF3_ctbl24nit[30] = {0, 0, 0, 0, 0, 0,-6, 5, -14, -6, 2, -10, -6, 0, -6, -4, 0, -5, -4, 0, -4, 0, 0, 1, 0, 0, 0, 3, 0, 4};
static signed char HF3_ctbl25nit[30] = {0, 0, 0, 0, 0, 0,-6, 5, -14, -6, 2, -10, -6, 0, -6, -4, 0, -5, -3, 0, -4, 0, 0, 1, 0, 0, 0, 3, 0, 4};
static signed char HF3_ctbl27nit[30] = {0, 0, 0, 0, 0, 0,-6, 5, -14, -6, 2, -10, -6, 0, -6, -4, 0, -5, -3, 0, -4, 0, 0, 1, 0, 0, 0, 3, 0, 4};
static signed char HF3_ctbl29nit[30] = {0, 0, 0, 0, 0, 0,-5, 5, -13, -5, 1, -9, -5, 0, -5, -3, 0, -4, -3, 0, -4, 0, 0, 1, 0, 0, 0, 3, 0, 4};
static signed char HF3_ctbl30nit[30] = {0, 0, 0, 0, 0, 0,-5, 5, -13, -5, 1, -9, -5, 0, -5, -3, 0, -4, -3, 0, -4, 0, 0, 1, 0, 0, 0, 3, 0, 4};
static signed char HF3_ctbl32nit[30] = {0, 0, 0, 0, 0, 0,-6, 5, -14, -6, 2, -11, -5, 0, -5, -3, 0, -4, -3, 0, -4, 0, 0, 1, 0, 0, 0, 3, 0, 4};
static signed char HF3_ctbl34nit[30] = {0, 0, 0, 0, 0, 0,-6, 5, -14, -5, 2, -9, -4, 0, -4, -2, 0, -4, -3, 0, -3, 0, 0, 1, 0, 0, 0, 3, 0, 4};
static signed char HF3_ctbl37nit[30] = {0, 0, 0, 0, 0, 0,-6, 5, -14, -5, 2, -9, -4, 0, -4, -2, 0, -3, -3, 0, -3, 0, 0, 1, 0, 0, 0, 3, 0, 4};
static signed char HF3_ctbl39nit[30] = {0, 0, 0, 0, 0, 0,-6, 5, -14, -5, 2, -8, -4, 0, -4, -2, 0, -3, -3, 0, -3, 0, 0, 1, 0, 0, 0, 3, 0, 4};
static signed char HF3_ctbl41nit[30] = {0, 0, 0, 0, 0, 0,-5, 6, -15, -5, 3, -8, -4, 0, -3, -2, 0, -3, -2, 0, -2, 0, 0, 1, 0, 0, 0, 3, 0, 4};
static signed char HF3_ctbl44nit[30] = {0, 0, 0, 0, 0, 0,-4, 6, -13, -4, 2, -7, -5, 0, -5, -1, 0, -2, -2, 0, -2, 0, 0, 1, 0, 0, 0, 3, 0, 4};
static signed char HF3_ctbl47nit[30] = {0, 0, 0, 0, 0, 0,-3, 6, -12, -3, 2, -6, -5, 0, -4, -1, 0, -2, -2, 0, -2, 0, 0, 1, 0, 0, 0, 3, 0, 4};
static signed char HF3_ctbl50nit[30] = {0, 0, 0, 0, 0, 0,-4, 7, -14, -3, 2, -5, -5, 0, -4, -1, 0, -2, -1, 0, -2, 0, 0, 1, 0, 0, 0, 3, 0, 4};
static signed char HF3_ctbl53nit[30] = {0, 0, 0, 0, 0, 0,-3, 6, -10, -3, 3, -6, -4, 0, -3, -1, 0, -2, -1, 0, -2, 0, 0, 1, 0, 0, 0, 3, 0, 4};
static signed char HF3_ctbl56nit[30] = {0, 0, 0, 0, 0, 0,-3, 6, -11, -2, 2, -4, -4, 0, -3, 0, 0, -1, 0, 0, -1, 0, 0, 1, 0, 0, 0, 3, 0, 4};
static signed char HF3_ctbl60nit[30] = {0, 0, 0, 0, 0, 0,-3, 6, -11, -2, 2, -4, -4, 0, -3, 0, 0, -1, 0, 0, -1, 0, 0, 1, 0, 0, 0, 3, 0, 4};
static signed char HF3_ctbl64nit[30] = {0, 0, 0, 0, 0, 0,-3, 6, -11, -2, 2, -4, -4, 0, -3, 0, 0, -1, 0, 0, -1, 0, 0, 1, 0, 0, 0, 3, 0, 4};
static signed char HF3_ctbl68nit[30] = {0, 0, 0, 0, 0, 0,-4, 4, -10, -2, 0, -4, -2, 0, -2, 0, 0, -1, 0, 0, -1, 0, 0, 1, 0, 0, 0, 2, 0, 4};
static signed char HF3_ctbl72nit[30] = {0, 0, 0, 0, 0, 0,-4, 4, -8, -1, 1, -4, -3, 0, -2, 0, 1, -1, -1, 0, -1, 1, 0, 1, 1, 0, 1, 3, 0, 4};
static signed char HF3_ctbl77nit[30] = {0, 0, 0, 0, 0, 0,-5, 4, -8, -2, 1, -4, -1, 0, -1, -1, 2, -4, 1, 0, 1, 1, 0, 2, 0, 0, -1, 2, 0, 4};
static signed char HF3_ctbl82nit[30] = {0, 0, 0, 0, 0, 0,-6, 2, -16, -2, 0, -2, 1, 0, -1, -1, 0, -1, -1, 0, -1, 0, 0, 1, -1, 0, 0, 2, 0, 3};
static signed char HF3_ctbl87nit[30] = {0, 0, 0, 0, 0, 0,-6, 2, -16, -2, 0, -2, 1, 0, -1, -1, 0, -1, -1, 0, -1, 0, 0, 1, -1, 0, 0, 2, 0, 3};
static signed char HF3_ctbl93nit[30] = {0, 0, 0, 0, 0, 0,-6, 2, -12, -2, 0, -2, 1, 0, -1, -1, 0, -1, -1, 0, -1, 0, 0, 1, -1, 0, 0, 2, 0, 3};
static signed char HF3_ctbl98nit[30] = {0, 0, 0, 0, 0, 0,-6, 2, -12, -2, 0, -2, 1, 0, -1, -1, 0, -1, -1, 0, -1, 0, 0, 1, -1, 0, 0, 2, 0, 3};
static signed char HF3_ctbl105nit[30] = {0, 0, 0, 0, 0, 0,-6, 2, -12, -2, 0, -2, 1, 0, -1, -1, 0, -1, -1, 0, -1, 0, 0, 1, -1, 0, 0, 2, 0, 3};
static signed char HF3_ctbl111nit[30] = {0, 0, 0, 0, 0, 0,-6, 2, -6, -2, 0, -2, 1, 0, -1, -1, 0, -1, -1, 0, -1, 0, 0, 1, -1, 0, 0, 2, 0, 3};
static signed char HF3_ctbl119nit[30] = {0, 0, 0, 0, 0, 0,-6, 2, -6, -2, 0, -2, 1, 0, -1, -1, 0, -1, -1, 0, -1, 0, 0, 1, -1, 0, 0, 2, 0, 3};
static signed char HF3_ctbl126nit[30] = {0, 0, 0, 0, 0, 0,-6, 2, -6, -2, 0, -2, 1, 0, -1, -1, 0, -1, -1, 0, -1, 0, 0, 1, -1, 0, 0, 2, 0, 3};
static signed char HF3_ctbl134nit[30] = {0, 0, 0, 0, 0, 0,-6, 2, -6, -2, 0, -2, 1, 0, -1, 2, 0, 0, -1, 0, -1, 0, 0, 1, -1, 0, 0, 2, 0, 3};
static signed char HF3_ctbl143nit[30] = {0, 0, 0, 0, 0, 0,-6, 2, -6, -2, 0, -2, 1, 0, -1, -1, 0, -1, -1, 0, -1, 0, 0, 1, -1, 0, 0, 2, 0, 3};
static signed char HF3_ctbl152nit[30] = {0, 0, 0, 0, 0, 0,-5, 2, -6, -1, 0, -2, -1, 0, -1, 0, 0, -2, 0, 0, 1, 0, 0, 1, 0, 0, 1, 1, 0, 2};
static signed char HF3_ctbl162nit[30] = {0, 0, 0, 0, 0, 0,-6, 2, -6, -2, 0, -2, 1, 0, -1, -1, 0, -1, -1, 0, -1, 0, 0, 1, -1, 0, 0, 2, 0, 3};
static signed char HF3_ctbl172nit[30] = {0, 0, 0, 0, 0, 0,-6, 2, -6, -2, 0, -2, 1, 0, -1, -1, 0, -1, -1, 0, -1, 0, 0, 1, -1, 0, 0, 2, 0, 3};
static signed char HF3_ctbl183nit[30] = {0, 0, 0, 0, 0, 0,-2, 2, -7, 0, 0, 0, 0, 0, -1, -1, 0, -2, -1, 0, -1, 0, 0, 1, -1, 0, 0, 3, 0, 3};
static signed char HF3_ctbl195nit[30] = {0, 0, 0, 0, 0, 0,-3, 2, -7, 0, 0, -1, 0, 0, -1, 1, 0, 1, 1, 0, 1, -2, 0, -2, 0, 0, 0, 1, 0, 2};
static signed char HF3_ctbl207nit[30] = {0, 0, 0, 0, 0, 0,-3, 2, -6, 0, 0, -1, 0, 0, -1, 0, 0, -1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 1, 0, 2};
static signed char HF3_ctbl220nit[30] = {0, 0, 0, 0, 0, 0,-3, 1, -6, 0, 0, -1, 0, 0, -1, 0, 0, -1, -1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 2};
static signed char HF3_ctbl234nit[30] = {0, 0, 0, 0, 0, 0,-2, 1, -5, 0, 0, -1, 1, 0, 0, 1, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 2};
static signed char HF3_ctbl249nit[30] = {0, 0, 0, 0, 0, 0,-1, 1, -4, 0, 0, -1, 0, 0, -1, 1, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 1, 0, 2};
static signed char HF3_ctbl265nit[30] = {0, 0, 0, 0, 0, 0,-1, 2, -4, 1, 0, 0, 1, 0, 0, -1, 0, -2, 1, 0, 1, 0, 0, 0, 0, 0, 0, 1, 0, 2};
static signed char HF3_ctbl282nit[30] = {0, 0, 0, 0, 0, 0,-1, 2, -4, 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 2};
static signed char HF3_ctbl300nit[30] = {0, 0, 0, 0, 0, 0,-1, 2, -4, 1, 0, 0, 1, 0, -1, 0, 0, 0, 1, 0, 1, 0, 0, -1, 0, 0, 0, 1, 0, 2};
static signed char HF3_ctbl316nit[30] = {0, 0, 0, 0, 0, 0,-1, 2, -3, 1, 0, 0, 0, 0, -1, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 1, 0, 2};
static signed char HF3_ctbl333nit[30] = {0, 0, 0, 0, 0, 0,-1, 2, -3, 1, 0, 0, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static signed char HF3_ctbl350nit[30] = {0, 0, 0, 0, 0, 0,-1, 2, -2, 1, 0, 0, 0, 0, -1, 0, 0, -1, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 0, 0};
static signed char HF3_ctbl357nit[30] = {0, 0, 0, 0, 0, 0,0, 1, -2, 1, 0, -1, 0, 0, -1, -1, 0, -1, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 0, 0};
static signed char HF3_ctbl365nit[30] = {0, 0, 0, 0, 0, 0,0, 1, -3, 1, 0, 1, 0, 0, -1, -1, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0};
static signed char HF3_ctbl372nit[30] = {0, 0, 0, 0, 0, 0,1, 1, -2, 1, 0, 0, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0};
static signed char HF3_ctbl380nit[30] = {0, 0, 0, 0, 0, 0,1, 1, -2, 1, 0, 0, 1, 0, 0, -1, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0};
static signed char HF3_ctbl387nit[30] = {0, 0, 0, 0, 0, 0,0, 1, -2, 1, 0, 0, 1, 0, 0, -1, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0};
static signed char HF3_ctbl395nit[30] = {0, 0, 0, 0, 0, 0,1, 1, -1, -1, 0, -1, 1, 0, 0, -1, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0};
static signed char HF3_ctbl403nit[30] = {0, 0, 0, 0, 0, 0,0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static signed char HF3_ctbl412nit[30] = {0, 0, 0, 0, 0, 0,0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static signed char HF3_ctbl420nit[30] = {0, 0, 0, 0, 0, 0,0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

static signed char HF3_aid9783[3] = {0xB1, 0x90, 0xDC};
static signed char HF3_aid9678[3] = {0xB1, 0x90, 0xC1};
static signed char HF3_aid9558[3] = {0xB1, 0x90, 0xA2};
static signed char HF3_aid9446[3] = {0xB1, 0x90, 0x85};
static signed char HF3_aid9345[3] = {0xB1, 0x90, 0x6B};
static signed char HF3_aid9248[3] = {0xB1, 0x90, 0x52};
static signed char HF3_aid9159[3] = {0xB1, 0x90, 0x3b};
static signed char HF3_aid9062[3] = {0xB1, 0x90, 0x22};
static signed char HF3_aid8973[3] = {0xB1, 0x90, 0x0B};
static signed char HF3_aid8907[3] = {0xB1, 0x80, 0xFA};
static signed char HF3_aid8814[3] = {0xB1, 0x80, 0xE2};
static signed char HF3_aid8744[3] = {0xB1, 0x80, 0xD0};
static signed char HF3_aid8667[3] = {0xB1, 0x80, 0xBC};
static signed char HF3_aid8574[3] = {0xB1, 0x80, 0xA4};
static signed char HF3_aid8477[3] = {0xB1, 0x80, 0x8B};
static signed char HF3_aid8380[3] = {0xB1, 0x80, 0x72};
static signed char HF3_aid8202[3] = {0xB1, 0x80, 0x44};
static signed char HF3_aid8120[3] = {0xB1, 0x80, 0x2F};
static signed char HF3_aid8008[3] = {0xB1, 0x80, 0x12};
static signed char HF3_aid7934[3] = {0xB1, 0x70, 0xFF};
static signed char HF3_aid7733[3] = {0xB1, 0x70, 0xCB};
static signed char HF3_aid7632[3] = {0xB1, 0x70, 0xB1};
static signed char HF3_aid7442[3] = {0xB1, 0x70, 0x80};
static signed char HF3_aid7240[3] = {0xB1, 0x70, 0x4C};
static signed char HF3_aid7143[3] = {0xB1, 0x70, 0x33};
static signed char HF3_aid6961[3] = {0xB1, 0x70, 0x04};
static signed char HF3_aid6760[3] = {0xB1, 0x60, 0xD0};
static signed char HF3_aid6469[3] = {0xB1, 0x60, 0x85};
static signed char HF3_aid6260[3] = {0xB1, 0x60, 0x4F};
static signed char HF3_aid6074[3] = {0xB1, 0x60, 0x1F};
static signed char HF3_aid5771[3] = {0xB1, 0x50, 0xD1};
static signed char HF3_aid5469[3] = {0xB1, 0x50, 0x83};
static signed char HF3_aid5295[3] = {0xB1, 0x50, 0x56};
static signed char HF3_aid4860[3] = {0xB1, 0x40, 0xE6};
static signed char HF3_aid4585[3] = {0xB1, 0x40, 0x9F};
static signed char HF3_aid4322[3] = {0xB1, 0x40, 0x5B};
static signed char HF3_aid3702[3] = {0xB1, 0x30, 0xBB};
static signed char HF3_aid3016[3] = {0xB1, 0x30, 0x0A};
static signed char HF3_aid2601[3] = {0xB1, 0x20, 0x9F};
static signed char HF3_aid2023[3] = {0xB1, 0x20, 0x0A};
static signed char HF3_aid1403[3] = {0xB1, 0x10, 0x6A};
static signed char HF3_aid1004[3] = {0xB1, 0x10, 0x03};
static signed char HF3_aid798[3] = {0xB1, 0x00, 0xCE};
static signed char HF3_aid601[3] = {0xB1, 0x00, 0x9B};
static signed char HF3_aid399[3] = {0xB1, 0x00, 0x67};
static signed char HF3_aid202[3] = {0xB1, 0x00, 0x34};
static signed char HF3_aid39[3] = {0xB1, 0x00, 0x0A};



// end of aid sheet in rev.E opmanual


static  unsigned char HF3_elv1[3] = {
        0xB5, 0xAC, 0x17
};
static  unsigned char HF3_elv2[3] = {
        0xB5, 0xAC, 0x16
};
static  unsigned char HF3_elv3[3] = {
        0xB5, 0xAC, 0x15
};
static  unsigned char HF3_elv4[3] = {
        0xB5, 0xAC, 0x14
};
static  unsigned char HF3_elv5[3] = {
        0xB5, 0xAC, 0x13
};
static  unsigned char HF3_elv6[3] = {
        0xB5, 0xAC, 0x12
};
static  unsigned char HF3_elv7[3] = {
        0xB5, 0xAC, 0x11
};
static  unsigned char HF3_elv8[3] = {
        0xB5, 0xAC, 0x10
};
static  unsigned char HF3_elv9[3] = {
        0xB5, 0xAC, 0x0F
};
static  unsigned char HF3_elv10[3] = {
        0xB5, 0xAC, 0x0E
};
static  unsigned char HF3_elv11[3] = {
        0xB5, 0xAC, 0x0D
};
static  unsigned char HF3_elv12[3] = {
        0xB5, 0xAC, 0x0C
};
static  unsigned char HF3_elv13[3] = {
        0xB5, 0xAC, 0x0B
};
static  unsigned char HF3_elv14[3] = {
        0xB5, 0xAC, 0x0A
};
static  unsigned char HF3_elv15[3] = {
        0xB5, 0xAC, 0x08
};
static  unsigned char HF3_elv16[3] = {
        0xB5, 0xAC, 0x07
};
static  unsigned char HF3_elv17[3] = {
        0xB5, 0xAC, 0x05
};
static  unsigned char HF3_elv18[3] = {
        0xB5, 0xAC, 0x0A
};


static  unsigned char HF3_elvCaps1[3] = {
        0xB5, 0xBC, 0x17
};
static  unsigned char HF3_elvCaps2[3] = {
        0xB5, 0xBC, 0x16
};
static  unsigned char HF3_elvCaps3[3] = {
        0xB5, 0xBC, 0x15
};
static  unsigned char HF3_elvCaps4[3] = {
        0xB5, 0xBC, 0x14
};
static  unsigned char HF3_elvCaps5[3] = {
        0xB5, 0xBC, 0x13
};
static  unsigned char HF3_elvCaps6[3] = {
        0xB5, 0xBC, 0x12
};
static  unsigned char HF3_elvCaps7[3] = {
        0xB5, 0xBC, 0x11
};
static  unsigned char HF3_elvCaps8[3] = {
        0xB5, 0xBC, 0x10
};
static  unsigned char HF3_elvCaps9[3] = {
        0xB5, 0xBC, 0x0F
};
static  unsigned char HF3_elvCaps10[3] = {
        0xB5, 0xBC, 0x0E
};
static  unsigned char HF3_elvCaps11[3] = {
        0xB5, 0xBC, 0x0D
};
static  unsigned char HF3_elvCaps12[3] = {
        0xB5, 0xBC, 0x0C
};
static  unsigned char HF3_elvCaps13[3] = {
        0xB5, 0xBC, 0x0B
};
static  unsigned char HF3_elvCaps14[3] = {
        0xB5, 0xBC, 0x0A
};
static  unsigned char HF3_elvCaps15[3] = {
        0xB5, 0xBC, 0x08
};
static  unsigned char HF3_elvCaps16[3] = {
        0xB5, 0xBC, 0x07
};
static  unsigned char HF3_elvCaps17[3] = {
        0xB5, 0xBC, 0x05
};
static  unsigned char HF3_elvCaps18[3] = {
        0xB5, 0xBC, 0x0A
};

/////////////// rev.d	///////////////

static signed char HF3_rtbl_revd_2nit[10] = {0, 28, 30, 28, 26, 23, 21, 15, 9, 0};
static signed char HF3_rtbl_revd_3nit[10] = {0, 28, 26, 24, 23, 21, 19, 14, 8, 0};
static signed char HF3_rtbl_revd_4nit[10] = {0, 28, 26, 24, 23, 21, 19, 14, 8, 0};
static signed char HF3_rtbl_revd_5nit[10] = {0, 28, 23, 20, 19, 18, 17, 12, 7, 0};
static signed char HF3_rtbl_revd_6nit[10] = {0, 28, 23, 20, 18, 17, 16, 11, 7, 0};
static signed char HF3_rtbl_revd_7nit[10] = {0, 28, 23, 20, 18, 17, 16, 11, 7, 0};
static signed char HF3_rtbl_revd_8nit[10] = {0, 28, 21, 18, 16, 16, 15, 9, 6, 0};
static signed char HF3_rtbl_revd_9nit[10] = {0, 28, 21, 18, 15, 14, 14, 9, 6, 0};
static signed char HF3_rtbl_revd_10nit[10] = {0, 28, 21, 18, 15, 14, 13, 9, 6, 0};
static signed char HF3_rtbl_revd_11nit[10] = {0, 28, 20, 17, 14, 13, 12, 8, 6, 0};
static signed char HF3_rtbl_revd_12nit[10] = {0, 28, 19, 16, 14, 13, 12, 8, 6, 0};
static signed char HF3_rtbl_revd_13nit[10] = {0, 28, 18, 15, 13, 12, 12, 8, 6, 0};
static signed char HF3_rtbl_revd_14nit[10] = {0, 23, 17, 15, 13, 12, 11, 7, 6, 0};
static signed char HF3_rtbl_revd_15nit[10] = {0, 19, 17, 14, 12, 11, 10, 7, 6, 0};
static signed char HF3_rtbl_revd_16nit[10] = {0, 18, 16, 14, 12, 11, 10, 7, 5, 0};
static signed char HF3_rtbl_revd_17nit[10] = {0, 18, 16, 14, 12, 11, 10, 7, 5, 0};
static signed char HF3_rtbl_revd_19nit[10] = {0, 17, 15, 13, 11, 10, 9, 7, 5, 0};
static signed char HF3_rtbl_revd_20nit[10] = {0, 15, 13, 10, 11, 10, 9, 7, 6, 0};
static signed char HF3_rtbl_revd_21nit[10] = {0, 15, 13, 10, 10, 9, 9, 6, 5, 0};
static signed char HF3_rtbl_revd_22nit[10] = {0, 15, 13, 10, 9, 9, 9, 6, 5, 0};
static signed char HF3_rtbl_revd_24nit[10] = {0, 14, 12, 10, 9, 8, 8, 6, 5, 0};
static signed char HF3_rtbl_revd_25nit[10] = {0, 13, 12, 9, 9, 8, 8, 6, 5, 0};
static signed char HF3_rtbl_revd_27nit[10] = {0, 13, 11, 9, 9, 8, 8, 6, 5, 0};
static signed char HF3_rtbl_revd_29nit[10] = {0, 13, 11, 9, 8, 7, 8, 6, 5, 0};
static signed char HF3_rtbl_revd_30nit[10] = {0, 12, 11, 9, 8, 7, 7, 5, 4, 0};
static signed char HF3_rtbl_revd_32nit[10] = {0, 12, 11, 9, 8, 7, 7, 5, 4, 0};
static signed char HF3_rtbl_revd_34nit[10] = {0, 11, 10, 9, 8, 7, 7, 5, 4, 0};
static signed char HF3_rtbl_revd_37nit[10] = {0, 11, 9, 8, 7, 7, 7, 5, 4, 0};
static signed char HF3_rtbl_revd_39nit[10] = {0, 10, 9, 7, 7, 6, 7, 5, 4, 0};
static signed char HF3_rtbl_revd_41nit[10] = {0, 9, 8, 7, 6, 5, 6, 5, 4, 0};
static signed char HF3_rtbl_revd_44nit[10] = {0, 9, 8, 7, 6, 5, 6, 5, 4, 0};
static signed char HF3_rtbl_revd_47nit[10] = {0, 8, 7, 7, 6, 5, 6, 5, 4, 0};
static signed char HF3_rtbl_revd_50nit[10] = {0, 8, 6, 6, 5, 5, 5, 5, 4, 0};
static signed char HF3_rtbl_revd_53nit[10] = {0, 8, 6, 6, 5, 5, 5, 5, 4, 0};
static signed char HF3_rtbl_revd_56nit[10] = {0, 7, 5, 5, 5, 5, 5, 5, 4, 0};
static signed char HF3_rtbl_revd_60nit[10] = {0, 7, 5, 5, 5, 5, 5, 5, 4, 0};
static signed char HF3_rtbl_revd_64nit[10] = {0, 7, 4, 4, 3, 4, 5, 5, 3, 0};
static signed char HF3_rtbl_revd_68nit[10] = {0, 6, 3, 3, 3, 4, 4, 4, 4, 0};
static signed char HF3_rtbl_revd_72nit[10] = {0, 6, 4, 4, 4, 3, 5, 4, 4, 0};
static signed char HF3_rtbl_revd_77nit[10] = {0, 6, 5, 5, 4, 4, 6, 5, 4, 0};
static signed char HF3_rtbl_revd_82nit[10] = {0, 6, 5, 5, 5, 5, 6, 5, 4, 0};
static signed char HF3_rtbl_revd_87nit[10] = {0, 6, 5, 5, 4, 4, 5, 5, 4, 0};
static signed char HF3_rtbl_revd_93nit[10] = {0, 6, 3, 3, 3, 4, 6, 4, 3, 0};
static signed char HF3_rtbl_revd_98nit[10] = {0, 6, 4, 4, 4, 4, 5, 5, 2, 0};
static signed char HF3_rtbl_revd_105nit[10] = {0, 6, 4, 4, 4, 4, 5, 5, 2, 0};
static signed char HF3_rtbl_revd_111nit[10] = {0, 6, 4, 4, 4, 4, 5, 5, 2, 0};
static signed char HF3_rtbl_revd_119nit[10] = {0, 6, 4, 4, 5, 5, 6, 7, 2, 0};
static signed char HF3_rtbl_revd_126nit[10] = {0, 6, 4, 3, 4, 5, 5, 5, 2, 0};
static signed char HF3_rtbl_revd_134nit[10] = {0, 6, 4, 4, 4, 5, 5, 6, 2, 0};
static signed char HF3_rtbl_revd_143nit[10] = {0, 6, 4, 4, 4, 5, 6, 6, 2, 0};
static signed char HF3_rtbl_revd_152nit[10] = {0, 5, 4, 4, 4, 5, 7, 6, 2, 0};
static signed char HF3_rtbl_revd_162nit[10] = {0, 5, 4, 3, 4, 5, 7, 7, 3, 0};
static signed char HF3_rtbl_revd_172nit[10] = {0, 5, 4, 3, 4, 4, 6, 6, 4, 0};
static signed char HF3_rtbl_revd_183nit[10] = {0, 4, 4, 3, 4, 5, 6, 6, 3, 0};
static signed char HF3_rtbl_revd_195nit[10] = {0, 4, 3, 3, 4, 5, 6, 6, 3, 0};
static signed char HF3_rtbl_revd_207nit[10] = {0, 3, 3, 2, 3, 4, 5, 6, 3, 0};
static signed char HF3_rtbl_revd_220nit[10] = {0, 3, 3, 2, 3, 4, 5, 5, 3, 0};
static signed char HF3_rtbl_revd_234nit[10] = {0, 3, 3, 2, 3, 4, 5, 5, 3, 0};
static signed char HF3_rtbl_revd_249nit[10] = {0, 2, 3, 2, 3, 4, 5, 5, 2, 0};
static signed char HF3_rtbl_revd_265nit[10] = {0, 2, 3, 2, 3, 3, 4, 4, 2, 0};
static signed char HF3_rtbl_revd_282nit[10] = {0, 2, 2, 2, 2, 3, 4, 3, 1, 0};
static signed char HF3_rtbl_revd_300nit[10] = {0, 2, 2, 2, 2, 2, 4, 3, 1, 0};
static signed char HF3_rtbl_revd_316nit[10] = {0, 2, 1, 1, 2, 2, 3, 3, 1, 0};
static signed char HF3_rtbl_revd_333nit[10] = {0, 2, 1, 1, 2, 2, 3, 2, 1, 0};
static signed char HF3_rtbl_revd_350nit[10] = {0, 2, 1, 1, 2, 2, 3, 3, 2, 0};
static signed char HF3_rtbl_revd_357nit[10] = {0, 1, 1, 1, 2, 2, 3, 4, 2, 0};
static signed char HF3_rtbl_revd_365nit[10] = {0, 1, 1, 1, 1, 2, 3, 2, 2, 0};
static signed char HF3_rtbl_revd_372nit[10] = {0, 1, 1, 1, 1, 2, 3, 2, 2, 0};
static signed char HF3_rtbl_revd_380nit[10] = {0, 1, 0, 1, 1, 2, 2, 2, 2, 0};
static signed char HF3_rtbl_revd_387nit[10] = {0, 1, 0, 1, 1, 2, 2, 2, 2, 0};
static signed char HF3_rtbl_revd_395nit[10] = {0, 1, 0, 1, 1, 2, 2, 2, 2, 0};
static signed char HF3_rtbl_revd_403nit[10] = {0, 1, 0, 1, 1, 2, 2, 1, 2, 0};
static signed char HF3_rtbl_revd_412nit[10] = {0, 1, 0, 0, 1, 1, 1, 0, 1, 0};
static signed char HF3_rtbl_revd_420nit[10] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

static signed char HF3_ctbl_revd_2nit[30] = {0, 0, 0, 0, 0, 0,-6, 2, -6, -7, 4, -3, -8, 3, -3, -14, 2, -7, -8, 2, -5, -3, 0, -2, -2, 0, -2, -8, 2, -7};
static signed char HF3_ctbl_revd_3nit[30] = {0, 0, 0, 0, 0, 0,-7, 3, -7, -8, 4, -4, -8, 3, -3, -10, 2, -7, -7, 1, -6, -3, 0, -2, -2, 0, -1, -7, 1, -7};
static signed char HF3_ctbl_revd_4nit[30] = {0, 0, 0, 0, 0, 0,-7, 3, -7, -8, 4, -4, -8, 3, -3, -10, 2, -7, -7, 1, -6, -3, 0, -2, -2, 0, -1, -7, 1, -7};
static signed char HF3_ctbl_revd_5nit[30] = {0, 0, 0, 0, 0, 0,-8, 3, -9, -7, 4, -5, -6, 3, -4, -7, 2, -5, -6, 1, -6, -2, 0, -2, -1, 0, -2, -5, 1, -4};
static signed char HF3_ctbl_revd_6nit[30] = {0, 0, 0, 0, 0, 0,-8, 3, -11, -7, 4, -4, -6, 1, -7, -6, 2, -5, -6, 1, -6, -1, 0, -1, -1, 0, -2, -4, 1, -3};
static signed char HF3_ctbl_revd_7nit[30] = {0, 0, 0, 0, 0, 0,-7, 4, -10, -7, 2, -4, -6, 1, -7, -6, 2, -5, -6, 1, -6, -1, 0, -2, 0, 0, -1, -3, 1, -2};
static signed char HF3_ctbl_revd_8nit[30] = {0, 0, 0, 0, 0, 0,-7, 4, -9, -7, 3, -5, -6, 1, -8, -5, 2, -5, -5, 1, -5, -2, 0, -2, 0, 0, -1, -3, 0, -2};
static signed char HF3_ctbl_revd_9nit[30] = {0, 0, 0, 0, 0, 0,-7, 4, -9, -7, 1, -5, -6, 1, -9, -5, 2, -5, -5, 1, -5, -2, 0, -2, 0, 0, -1, -3, 0, -2};
static signed char HF3_ctbl_revd_10nit[30] = {0, 0, 0, 0, 0, 0,-6, 4, -8, -7, 0, -6, -6, 0, -9, -5, 2, -5, -5, 1, -5, 0, 0, 0, 0, 0, -1, -2, 0, -1};
static signed char HF3_ctbl_revd_11nit[30] = {0, 0, 0, 0, 0, 0,-6, 5, -8, -6, 0, -7, -6, 0, -9, -5, 2, -5, -5, 1, -5, 0, 0, -1, 0, 0, 0, -2, 0, -1};
static signed char HF3_ctbl_revd_12nit[30] = {0, 0, 0, 0, 0, 0,-6, 5, -9, -6, 0, -7, -5, 0, -8, -4, 2, -5, -5, 1, -5, -1, 0, -1, 0, 0, 0, -1, 0, 0};
static signed char HF3_ctbl_revd_13nit[30] = {0, 0, 0, 0, 0, 0,-6, 5, -9, -7, 1, -8, -5, 0, -9, -4, 2, -5, -3, 1, -4, -1, 0, -1, 0, 0, 0, -1, 0, 1};
static signed char HF3_ctbl_revd_14nit[30] = {0, 0, 0, 0, 0, 0,-6, 5, -9, -8, 1, -10, -5, 0, -9, -4, 2, -5, -3, 1, -4, -1, 0, -1, 0, 0, 0, 0, 0, 1};
static signed char HF3_ctbl_revd_15nit[30] = {0, 0, 0, 0, 0, 0,-6, 4, -10, -7, 0, -9, -5, 0, -9, -4, 2, -4, -3, 1, -4, 0, 0, -1, 0, 0, 0, 0, 0, 1};
static signed char HF3_ctbl_revd_16nit[30] = {0, 0, 0, 0, 0, 0,-8, 5, -13, -6, 0, -8, -5, 0, -8, -5, 1, -5, -3, 1, -4, 0, 0, -1, 0, 0, 0, 0, 0, 1};
static signed char HF3_ctbl_revd_17nit[30] = {0, 0, 0, 0, 0, 0,-7, 5, -12, -6, 0, -8, -5, 0, -7, -5, 1, -5, -2, 1, -3, 0, 0, -1, 0, 0, 0, 0, 0, 1};
static signed char HF3_ctbl_revd_19nit[30] = {0, 0, 0, 0, 0, 0,-6, 5, -11, -6, 0, -8, -5, 0, -7, -5, 0, -5, -2, 1, -4, 0, 0, -1, 0, 0, 0, 0, 0, 2};
static signed char HF3_ctbl_revd_20nit[30] = {0, 0, 0, 0, 0, 0,-7, 4, -14, -3, 4, -7, -3, 2, -4, -4, 1, -4, -2, 1, -3, 0, 0, -1, 0, 0, 0, 1, 0, 2};
static signed char HF3_ctbl_revd_21nit[30] = {0, 0, 0, 0, 0, 0,-7, 4, -14, -3, 2, -6, -2, 2, -4, -4, 1, -4, -3, 1, -4, 0, 0, 0, 0, 0, 0, 1, 0, 2};
static signed char HF3_ctbl_revd_22nit[30] = {0, 0, 0, 0, 0, 0,-6, 4, -12, -3, 2, -6, -2, 2, -4, -4, 1, -4, -3, 1, -4, 0, 0, 0, 0, 0, 0, 1, 0, 2};
static signed char HF3_ctbl_revd_24nit[30] = {0, 0, 0, 0, 0, 0,-6, 5, -14, -3, 2, -5, -1, 1, -3, -3, 1, -4, -2, 1, -2, 0, 0, 0, 0, 0, 0, 1, 0, 2};
static signed char HF3_ctbl_revd_25nit[30] = {0, 0, 0, 0, 0, 0,-7, 4, -13, -3, 2, -6, -1, 1, -3, -2, 1, -3, -2, 1, -2, 0, 0, 0, 0, 0, 0, 1, 0, 2};
static signed char HF3_ctbl_revd_27nit[30] = {0, 0, 0, 0, 0, 0,-8, 5, -15, -3, 2, -6, -1, 1, -3, -2, 0, -3, -2, 1, -2, 0, 0, 0, 0, 0, 0, 1, 0, 2};
static signed char HF3_ctbl_revd_29nit[30] = {0, 0, 0, 0, 0, 0,-7, 4, -15, -4, 0, -6, -2, 1, -3, -2, 1, -4, -2, 1, -2, 0, 0, 0, 0, 0, 0, 2, 0, 3};
static signed char HF3_ctbl_revd_30nit[30] = {0, 0, 0, 0, 0, 0,-6, 3, -15, -4, 0, -6, -2, 1, -3, -3, 0, -3, -2, 1, -3, 0, 0, 0, 1, 1, 1, 2, 0, 3};
static signed char HF3_ctbl_revd_32nit[30] = {0, 0, 0, 0, 0, 0,-6, 3, -15, -4, 0, -5, -2, 0, -3, -3, 0, -3, -2, 1, -3, 0, 0, 0, 0, 0, 0, 2, 0, 3};
static signed char HF3_ctbl_revd_34nit[30] = {0, 0, 0, 0, 0, 0,-6, 5, -16, -4, 0, -5, -2, 0, -3, -3, 0, -3, -2, 0, -3, 0, 0, 0, 0, 0, 0, 2, 0, 3};
static signed char HF3_ctbl_revd_37nit[30] = {0, 0, 0, 0, 0, 0,-6, 5, -15, -4, 0, -4, -1, 0, -3, -3, 0, -3, -2, 0, -3, 0, 0, 0, 0, 0, 0, 2, 0, 3};
static signed char HF3_ctbl_revd_39nit[30] = {0, 0, 0, 0, 0, 0,-5, 4, -15, -4, 1, -4, -1, 0, -3, -3, 0, -3, -2, 0, -3, 0, 0, 0, 0, 0, 0, 2, 0, 3};
static signed char HF3_ctbl_revd_41nit[30] = {0, 0, 0, 0, 0, 0,-5, 5, -14, -4, 0, -4, -1, 1, -3, -2, 0, -2, 0, 0, -1, 0, 0, 0, 0, 0, 0, 2, 0, 3};
static signed char HF3_ctbl_revd_44nit[30] = {0, 0, 0, 0, 0, 0,-5, 4, -13, -4, 0, -4, -1, 1, -2, -2, 0, -2, 0, 0, -1, 0, 0, 0, 0, 0, 0, 2, 0, 3};
static signed char HF3_ctbl_revd_47nit[30] = {0, 0, 0, 0, 0, 0,-2, 5, -13, -4, 0, -3, -1, 0, -2, -2, 0, -2, 0, 0, -1, 0, 0, 0, 0, 0, 0, 2, 0, 3};
static signed char HF3_ctbl_revd_50nit[30] = {0, 0, 0, 0, 0, 0,-2, 7, -13, -4, 0, -3, 0, 1, -2, -3, 0, -3, 0, 1, -1, 0, 0, 0, 0, -1, 0, 2, 0, 3};
static signed char HF3_ctbl_revd_53nit[30] = {0, 0, 0, 0, 0, 0,-1, 7, -10, -4, 0, -3, 0, 1, -2, -3, 0, -3, 0, 1, -1, 0, 0, 0, 0, -1, 0, 2, 0, 3};
static signed char HF3_ctbl_revd_56nit[30] = {0, 0, 0, 0, 0, 0,-1, 8, -11, 1, 3, 1, 0, 1, -2, -2, 0, -2, 0, 1, -1, -2, -2, -2, 1, 0, 1, 1, 0, 3};
static signed char HF3_ctbl_revd_60nit[30] = {0, 0, 0, 0, 0, 0,-2, 7, -11, 1, 3, 1, 0, 0, -2, -2, -1, -3, 0, 1, -1, -2, -2, -2, 1, 0, 1, 1, 0, 3};
static signed char HF3_ctbl_revd_64nit[30] = {0, 0, 0, 0, 0, 0,-4, 4, -15, -2, 0, -1, 1, 1, -1, 1, 1, 1, 1, 1, 0, -1, -2, -2, 1, 0, 2, 1, -1, 2};
static signed char HF3_ctbl_revd_68nit[30] = {0, 0, 0, 0, 0, 0,2, 8, -5, 1, 3, 1, 0, 0, -1, 2, 2, 2, 1, 1, 0, 1, -1, 1, -1, 0, 1, 2, -1, 2};
static signed char HF3_ctbl_revd_72nit[30] = {0, 0, 0, 0, 0, 0,-5, 4, -14, 0, 1, 0, -1, 0, -1, 1, 2, 0, 0, 0, 0, -1, -1, 0, 1, 0, 1, 2, 0, 3};
static signed char HF3_ctbl_revd_77nit[30] = {0, 0, 0, 0, 0, 0,-6, 2, -14, -1, 0, -1, 0, 0, -1, -1, 0, -1, -1, 0, -1, 1, 0, 0, 0, 0, 0, 2, 0, 3};
static signed char HF3_ctbl_revd_82nit[30] = {0, 0, 0, 0, 0, 0,-4, 4, -12, -2, -1, -1, -1, 0, -2, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 2, 0, 3};
static signed char HF3_ctbl_revd_87nit[30] = {0, 0, 0, 0, 0, 0,-4, 4, -12, -2, -1, -1, 0, 0, -1, 0, 0, 0, 0, 0, -1, 2, 0, 1, 0, 0, 1, 2, 0, 3};
static signed char HF3_ctbl_revd_93nit[30] = {0, 0, 0, 0, 0, 0,-5, 5, -12, -1, 0, -3, -1, 0, 1, 0, 0, 0, 0, 0, -1, 1, 0, 0, 0, 0, 1, 2, 0, 3};
static signed char HF3_ctbl_revd_98nit[30] = {0, 0, 0, 0, 0, 0,-4, 5, -9, -1, 0, -1, 0, 0, -1, -1, 0, -1, -1, 0, -1, 1, 0, 1, 1, 0, 1, 1, 0, 3};
static signed char HF3_ctbl_revd_105nit[30] = {0, 0, 0, 0, 0, 0,-4, 5, -9, -2, 0, -1, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 1, 1, 0, 2};
static signed char HF3_ctbl_revd_111nit[30] = {0, 0, 0, 0, 0, 0,-4, 4, -9, -2, 0, 0, 0, 0, -1, 0, 0, 0, 1, 0, -1, 0, 0, 1, 0, 0, 1, 1, 0, 2};
static signed char HF3_ctbl_revd_119nit[30] = {0, 0, 0, 0, 0, 0,-4, 4, -9, 0, 0, 0, -1, 0, -1, -1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 2};
static signed char HF3_ctbl_revd_126nit[30] = {0, 0, 0, 0, 0, 0,-4, 3, -8, 0, 0, 0, 1, 0, 0, -1, 0, 0, 0, 0, -1, 1, 0, 1, 0, 0, 1, 2, 0, 2};
static signed char HF3_ctbl_revd_134nit[30] = {0, 0, 0, 0, 0, 0,-4, 1, -9, 0, 0, 2, 1, 0, 0, -1, 0, 0, 1, 0, 0, 1, 0, 1, 0, 0, 1, 2, 0, 2};
static signed char HF3_ctbl_revd_143nit[30] = {0, 0, 0, 0, 0, 0,-5, 1, -9, 0, 0, 2, 1, 0, 0, -1, 0, -1, -1, 0, -1, 1, 0, 1, 0, 0, 1, 2, 0, 1};
static signed char HF3_ctbl_revd_152nit[30] = {0, 0, 0, 0, 0, 0,-5, 1, -9, -2, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 2, 0, 2};
static signed char HF3_ctbl_revd_162nit[30] = {0, 0, 0, 0, 0, 0,-5, 0, -8, -2, 0, 1, 0, 0, -1, -1, 0, 0, 1, 0, 1, 0, 0, 1, -1, 0, 0, 2, 0, 2};
static signed char HF3_ctbl_revd_172nit[30] = {0, 0, 0, 0, 0, 0,-5, 0, -8, -2, 0, 1, -1, 0, -2, 0, 0, 1, 2, 0, 1, 0, 0, 0, 0, 0, 1, 1, 0, 1};
static signed char HF3_ctbl_revd_183nit[30] = {0, 0, 0, 0, 0, 0,0, 3, -4, 0, 0, 2, -1, 0, -1, 0, 0, 0, 0, 0, 0, 1, 0, 1, -1, 0, 0, 2, 0, 2};
static signed char HF3_ctbl_revd_195nit[30] = {0, 0, 0, 0, 0, 0,-1, 3, -6, 0, 0, 2, 0, 0, 0, -1, 0, -1, 1, 0, 1, 1, 0, 1, -1, 0, 0, 2, 0, 2};
static signed char HF3_ctbl_revd_207nit[30] = {0, 0, 0, 0, 0, 0,-2, 2, -5, 0, 0, 2, 1, 0, 1, 0, 0, 0, 2, 0, 1, -1, 0, 0, 0, 0, 1, 1, 0, 1};
static signed char HF3_ctbl_revd_220nit[30] = {0, 0, 0, 0, 0, 0,-2, 2, -5, 0, 0, 2, 0, 0, 0, 1, 0, 1, -1, 0, -1, 1, 0, 1, 0, 0, 1, 1, 0, 1};
static signed char HF3_ctbl_revd_234nit[30] = {0, 0, 0, 0, 0, 0,-3, 0, -6, 0, 0, 2, 0, 0, 0, 1, 0, 1, -1, 0, -1, 1, 0, 1, 0, 0, 1, 1, 0, 1};
static signed char HF3_ctbl_revd_249nit[30] = {0, 0, 0, 0, 0, 0,-3, 0, -5, 2, 0, 2, -1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, -1, 0, 0, 1, 1, 0, 1};
static signed char HF3_ctbl_revd_265nit[30] = {0, 0, 0, 0, 0, 0,-2, 0, -4, 0, 0, 2, 0, 0, 0, 0, 0, 0, 1, 0, 1, 2, 0, 1, 0, 0, 1, 0, 0, 0};
static signed char HF3_ctbl_revd_282nit[30] = {0, 0, 0, 0, 0, 0,0, 0, -3, 0, 0, 2, 0, 0, 0, 1, 0, 1, 1, 0, 1, 1, 0, 1, 0, 0, 1, 0, 0, 0};
static signed char HF3_ctbl_revd_300nit[30] = {0, 0, 0, 0, 0, 0,0, 0, -3, 0, 0, 2, 1, 0, 0, -1, 0, 0, 1, 0, 1, 0, 0, 0, 1, 0, 1, 0, 0, -1};
static signed char HF3_ctbl_revd_316nit[30] = {0, 0, 0, 0, 0, 0,0, 0, -3, 0, 0, 2, -1, 0, 0, 0, 0, 0, 1, 0, 1, 0, 0, 0, 1, 0, 1, 0, 0, -1};
static signed char HF3_ctbl_revd_333nit[30] = {0, 0, 0, 0, 0, 0,0, 0, -3, 0, 0, 2, 0, 0, 0, -1, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0};
static signed char HF3_ctbl_revd_350nit[30] = {0, 0, 0, 0, 0, 0,-1, 0, -3, 0, 0, 2, 0, 0, 0, 0, 0, 1, 2, 0, 1, -1, 0, 0, 0, 0, 0, 0, 0, -1};
static signed char HF3_ctbl_revd_357nit[30] = {0, 0, 0, 0, 0, 0,0, 0, -2, 1, 0, 3, 0, 0, 0, 0, 0, 1, 2, 0, 1, -1, 0, 0, 0, 0, 0, 0, 0, -1};
static signed char HF3_ctbl_revd_365nit[30] = {0, 0, 0, 0, 0, 0,0, 0, -2, 1, 0, 3, 0, 0, 0, -1, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, -1};
static signed char HF3_ctbl_revd_372nit[30] = {0, 0, 0, 0, 0, 0,0, 0, -2, 1, 0, 3, 0, 0, 0, -1, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, -1};
static signed char HF3_ctbl_revd_380nit[30] = {0, 0, 0, 0, 0, 0,-1, 0, -2, 1, 0, 3, 0, 0, 0, -1, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, -1};
static signed char HF3_ctbl_revd_387nit[30] = {0, 0, 0, 0, 0, 0,0, 0, -2, 0, 0, 3, 0, 0, 0, -1, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, -1};
static signed char HF3_ctbl_revd_395nit[30] = {0, 0, 0, 0, 0, 0,0, 0, -4, 0, 0, 2, 0, 0, 0, -1, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, -1};
static signed char HF3_ctbl_revd_403nit[30] = {0, 0, 0, 0, 0, 0,0, 0, -4, 0, 0, 2, 0, 0, 0, -1, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, -1};
static signed char HF3_ctbl_revd_412nit[30] = {0, 0, 0, 0, 0, 0,0, 0, -4, 0, 0, 2, 0, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -1, 0, -1};
static signed char HF3_ctbl_revd_420nit[30] = {0, 0, 0, 0, 0, 0,0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

static signed char HF3_aid9806[3] = {0xB1, 0x90, 0xDE};
static signed char HF3_aid9686[3] = {0xB1, 0x90, 0xBF};
static signed char HF3_aid9561[3] = {0xB1, 0x90, 0x9F};
static signed char HF3_aid9441[3] = {0xB1, 0x90, 0x80};
static signed char HF3_aid9340[3] = {0xB1, 0x90, 0x66};
static signed char HF3_aid9251[3] = {0xB1, 0x90, 0x4F};
static signed char HF3_aid9150[3] = {0xB1, 0x90, 0x35};
static signed char HF3_aid9068[3] = {0xB1, 0x90, 0x20};
static signed char HF3_aid8987[3] = {0xB1, 0x90, 0x0B};
static signed char HF3_aid8898[3] = {0xB1, 0x80, 0xF4};
static signed char HF3_aid8824[3] = {0xB1, 0x80, 0xE1};
static signed char HF3_aid8754[3] = {0xB1, 0x80, 0xCF};
static signed char HF3_aid8680[3] = {0xB1, 0x80, 0xBC};
static signed char HF3_aid8587[3] = {0xB1, 0x80, 0xA4};
static signed char HF3_aid8505[3] = {0xB1, 0x80, 0x8F};
static signed char HF3_aid8405[3] = {0xB1, 0x80, 0x75};
static signed char HF3_aid8203[3] = {0xB1, 0x80, 0x41};
static signed char HF3_aid8117[3] = {0xB1, 0x80, 0x2B};
static signed char HF3_aid8016[3] = {0xB1, 0x80, 0x11};
static signed char HF3_aid7908[3] = {0xB1, 0x70, 0xF5};
static signed char HF3_aid7729[3] = {0xB1, 0x70, 0xC7};
static signed char HF3_aid7620[3] = {0xB1, 0x70, 0xAB};
static signed char HF3_aid7434[3] = {0xB1, 0x70, 0x7B};
static signed char HF3_aid7224[3] = {0xB1, 0x70, 0x45};
// static signed char HF3_aid7143[3] = {0xB1, 0x70, 0x30};		// redefine
static signed char HF3_aid6960[3] = {0xB1, 0x70, 0x01};
static signed char HF3_aid6755[3] = {0xB1, 0x60, 0xCC};
static signed char HF3_aid6444[3] = {0xB1, 0x60, 0x7C};
static signed char HF3_aid6273[3] = {0xB1, 0x60, 0x50};
static signed char HF3_aid6068[3] = {0xB1, 0x60, 0x1B};
static signed char HF3_aid5761[3] = {0xB1, 0x50, 0xCC};
static signed char HF3_aid5446[3] = {0xB1, 0x50, 0x7B};
static signed char HF3_aid5155[3] = {0xB1, 0x50, 0x30};
static signed char HF3_aid4852[3] = {0xB1, 0x40, 0xE2};
static signed char HF3_aid4515[3] = {0xB1, 0x40, 0x8B};
static signed char HF3_aid4057[3] = {0xB1, 0x40, 0x15};
static signed char HF3_aid3793[3] = {0xB1, 0x30, 0xD1};
static signed char HF3_aid3335[3] = {0xB1, 0x30, 0x5B};
static signed char HF3_aid2853[3] = {0xB1, 0x20, 0xDF};
static signed char HF3_aid2306[3] = {0xB1, 0x20, 0x52};
static signed char HF3_aid1727[3] = {0xB1, 0x10, 0xBD};
static signed char HF3_aid1184[3] = {0xB1, 0x10, 0x31};
static signed char HF3_aid1005[3] = {0xB1, 0x10, 0x03};
static signed char HF3_aid800[3] = {0xB1, 0x00, 0xCE};
static signed char HF3_aid606[3] = {0xB1, 0x00, 0x9C};
static signed char HF3_aid392[3] = {0xB1, 0x00, 0x65};
static signed char HF3_aid113[3] = {0xB1, 0x00, 0x1D};

// a3

static signed char HF3_rtbl_a3_2nit[10] = {0, 19, 32, 30, 28, 25, 20, 14, 9, 0};
static signed char HF3_rtbl_a3_3nit[10] = {0, 19, 30, 28, 26, 23, 19, 13, 7, 0};
static signed char HF3_rtbl_a3_4nit[10] = {0, 19, 29, 26, 24, 21, 18, 11, 7, 0};
static signed char HF3_rtbl_a3_5nit[10] = {0, 19, 25, 22, 21, 19, 17, 11, 7, 0};
static signed char HF3_rtbl_a3_6nit[10] = {0, 19, 24, 21, 20, 18, 15, 11, 7, 0};
static signed char HF3_rtbl_a3_7nit[10] = {0, 19, 23, 20, 19, 17, 14, 11, 8, 0};
static signed char HF3_rtbl_a3_8nit[10] = {0, 19, 23, 20, 18, 16, 13, 9, 6, 0};
static signed char HF3_rtbl_a3_9nit[10] = {0, 19, 23, 19, 18, 16, 13, 9, 6, 0};
static signed char HF3_rtbl_a3_10nit[10] = {0, 19, 21, 18, 16, 15, 12, 7, 6, 0};
static signed char HF3_rtbl_a3_11nit[10] = {0, 19, 20, 17, 16, 15, 12, 7, 6, 0};
static signed char HF3_rtbl_a3_12nit[10] = {0, 19, 19, 16, 15, 14, 11, 7, 6, 0};
static signed char HF3_rtbl_a3_13nit[10] = {0, 19, 18, 15, 14, 13, 10, 7, 5, 0};
static signed char HF3_rtbl_a3_14nit[10] = {0, 18, 17, 14, 13, 12, 9, 6, 5, 0};
static signed char HF3_rtbl_a3_15nit[10] = {0, 18, 17, 14, 13, 12, 9, 7, 4, 0};
static signed char HF3_rtbl_a3_16nit[10] = {0, 18, 17, 14, 13, 12, 9, 6, 5, 0};
static signed char HF3_rtbl_a3_17nit[10] = {0, 17, 16, 13, 12, 11, 8, 6, 5, 0};
static signed char HF3_rtbl_a3_19nit[10] = {0, 17, 14, 11, 11, 10, 7, 7, 3, 0};
static signed char HF3_rtbl_a3_20nit[10] = {0, 16, 14, 11, 11, 10, 7, 5, 4, 0};
static signed char HF3_rtbl_a3_21nit[10] = {0, 15, 14, 11, 11, 10, 7, 5, 6, 0};
static signed char HF3_rtbl_a3_22nit[10] = {0, 15, 13, 10, 10, 8, 6, 5, 4, 0};
static signed char HF3_rtbl_a3_24nit[10] = {0, 14, 12, 9, 9, 8, 6, 5, 4, 0};
static signed char HF3_rtbl_a3_25nit[10] = {0, 14, 12, 9, 9, 8, 6, 5, 4, 0};
static signed char HF3_rtbl_a3_27nit[10] = {0, 14, 12, 9, 9, 8, 6, 5, 4, 0};
static signed char HF3_rtbl_a3_29nit[10] = {0, 14, 12, 9, 9, 8, 6, 5, 4, 0};
static signed char HF3_rtbl_a3_30nit[10] = {0, 13, 11, 8, 8, 7, 6, 5, 4, 0};
static signed char HF3_rtbl_a3_32nit[10] = {0, 13, 11, 8, 8, 7, 5, 4, 4, 0};
static signed char HF3_rtbl_a3_34nit[10] = {0, 12, 10, 7, 7, 6, 5, 4, 4, 0};
static signed char HF3_rtbl_a3_37nit[10] = {0, 9, 8, 6, 6, 5, 4, 4, 4, 0};
static signed char HF3_rtbl_a3_39nit[10] = {0, 9, 8, 6, 6, 5, 4, 4, 4, 0};
static signed char HF3_rtbl_a3_41nit[10] = {0, 9, 7, 5, 5, 5, 4, 4, 4, 0};
static signed char HF3_rtbl_a3_44nit[10] = {0, 9, 7, 5, 5, 5, 4, 4, 4, 0};
static signed char HF3_rtbl_a3_47nit[10] = {0, 8, 6, 5, 5, 5, 4, 4, 4, 0};
static signed char HF3_rtbl_a3_50nit[10] = {0, 8, 6, 5, 5, 5, 3, 3, 4, 0};
static signed char HF3_rtbl_a3_53nit[10] = {0, 8, 6, 5, 5, 5, 3, 3, 4, 0};
static signed char HF3_rtbl_a3_56nit[10] = {0, 8, 6, 5, 5, 5, 3, 3, 4, 0};
static signed char HF3_rtbl_a3_60nit[10] = {0, 7, 5, 4, 4, 4, 3, 3, 4, 0};
static signed char HF3_rtbl_a3_64nit[10] = {0, 7, 5, 4, 4, 4, 3, 3, 3, 0};
static signed char HF3_rtbl_a3_68nit[10] = {0, 7, 5, 4, 3, 3, 2, 2, 3, 0};
static signed char HF3_rtbl_a3_72nit[10] = {0, 7, 5, 4, 3, 2, 2, 2, 3, 0};
static signed char HF3_rtbl_a3_77nit[10] = {0, 5, 3, 3, 3, 3, 4, 3, 4, 0};
static signed char HF3_rtbl_a3_82nit[10] = {0, 5, 5, 4, 3, 3, 4, 4, 4, 0};
static signed char HF3_rtbl_a3_87nit[10] = {0, 2, 5, 4, 4, 5, 5, 6, 4, 0};
static signed char HF3_rtbl_a3_93nit[10] = {0, 5, 4, 4, 4, 4, 4, 4, 4, 0};
static signed char HF3_rtbl_a3_98nit[10] = {0, 4, 4, 4, 4, 4, 5, 5, 4, 0};
static signed char HF3_rtbl_a3_105nit[10] = {0, 3, 4, 4, 3, 3, 5, 4, 3, 0};
static signed char HF3_rtbl_a3_111nit[10] = {0, 3, 4, 3, 4, 3, 4, 4, 3, 0};
static signed char HF3_rtbl_a3_119nit[10] = {0, 3, 5, 4, 4, 4, 5, 6, 3, 0};
static signed char HF3_rtbl_a3_126nit[10] = {0, 2, 3, 3, 4, 4, 6, 6, 3, 0};
static signed char HF3_rtbl_a3_134nit[10] = {0, 2, 3, 3, 4, 4, 5, 7, 4, 0};
static signed char HF3_rtbl_a3_143nit[10] = {0, 2, 3, 3, 4, 4, 5, 7, 4, 0};
static signed char HF3_rtbl_a3_152nit[10] = {0, 2, 3, 3, 4, 4, 6, 7, 4, 0};
static signed char HF3_rtbl_a3_162nit[10] = {0, 2, 3, 3, 4, 4, 6, 7, 4, 0};
static signed char HF3_rtbl_a3_172nit[10] = {0, 2, 4, 4, 5, 5, 6, 6, 4, 0};
static signed char HF3_rtbl_a3_183nit[10] = {0, 1, 3, 3, 4, 4, 4, 6, 3, 0};
static signed char HF3_rtbl_a3_195nit[10] = {0, 1, 2, 2, 3, 3, 4, 6, 3, 0};
static signed char HF3_rtbl_a3_207nit[10] = {0, 1, 2, 2, 3, 3, 4, 6, 3, 0};
static signed char HF3_rtbl_a3_220nit[10] = {0, 1, 2, 2, 3, 3, 4, 6, 3, 0};
static signed char HF3_rtbl_a3_234nit[10] = {0, 1, 2, 2, 3, 3, 4, 6, 3, 0};
static signed char HF3_rtbl_a3_249nit[10] = {0, 1, 1, 1, 2, 2, 3, 5, 3, 0};
static signed char HF3_rtbl_a3_265nit[10] = {0, 1, 2, 2, 2, 2, 3, 3, 1, 0};
static signed char HF3_rtbl_a3_282nit[10] = {0, 0, 1, 1, 1, 2, 3, 4, 2, 0};
static signed char HF3_rtbl_a3_300nit[10] = {0, 0, 0, 1, 2, 2, 2, 2, 2, 0};
static signed char HF3_rtbl_a3_316nit[10] = {0, 0, 0, 0, 1, 1, 1, 2, 2, 0};
static signed char HF3_rtbl_a3_333nit[10] = {0, 0, 0, 0, 0, 1, 1, 2, 1, 0};
static signed char HF3_rtbl_a3_350nit[10] = {0, 0, 1, 1, 1, 1, 1, 2, 1, 0};
static signed char HF3_rtbl_a3_357nit[10] = {0, 0, 1, 0, 0, 1, 2, 2, 1, 0};
static signed char HF3_rtbl_a3_365nit[10] = {0, 0, 0, 0, 0, 0, 2, 2, 0, 0};
static signed char HF3_rtbl_a3_372nit[10] = {0, 0, 0, 1, 1, 1, 2, 1, 1, 0};
static signed char HF3_rtbl_a3_380nit[10] = {0, 0, 0, 0, 0, 0, 1, 1, 1, 0};
static signed char HF3_rtbl_a3_387nit[10] = {0, 0, 0, 0, 0, 0, 1, 1, 1, 0};
static signed char HF3_rtbl_a3_395nit[10] = {0, 0, 0, 0, 0, 0, 1, 1, 1, 0};
static signed char HF3_rtbl_a3_403nit[10] = {0, 0, 0, 0, 0, 0, 1, 1, 1, 0};
static signed char HF3_rtbl_a3_412nit[10] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static signed char HF3_rtbl_a3_420nit[10] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};


static signed char HF3_ctbl_a3_2nit[30] = {0, 0, 0, 0, 0, 0,-10, 1, -4, -11, 1, -7, -12, 1, -6, -16, -1, -8, -10, 1, -5, -2, 0, -2, -3, 0, -4, -7, 0, -8};
static signed char HF3_ctbl_a3_3nit[30] = {0, 0, 0, 0, 0, 0,-14, 1, -5, -6, 0, -8, -13, 0, -7, -14, -1, -9, -9, -1, -7, -3, 0, -4, -2, 0, -3, -3, 2, -4};
static signed char HF3_ctbl_a3_4nit[30] = {0, 0, 0, 0, 0, 0,-17, 0, -10, -9, -1, -10, -12, 0, -7, -13, -1, -8, -8, -1, -7, -2, 0, -3, -2, 0, -3, -1, 3, -2};
static signed char HF3_ctbl_a3_5nit[30] = {0, 0, 0, 0, 0, 0,-13, 1, -8, -9, 1, -8, -8, 1, -5, -10, 0, -7, -7, 0, -7, -3, 0, -3, -1, 0, -2, 0, 3, -1};
static signed char HF3_ctbl_a3_6nit[30] = {0, 0, 0, 0, 0, 0,-18, 2, -8, -7, 1, -9, -9, 1, -5, -11, -1, -8, -6, 1, -6, -3, 0, -3, 0, 0, -2, 0, 3, -1};
static signed char HF3_ctbl_a3_7nit[30] = {0, 0, 0, 0, 0, 0,-13, 2, -10, -8, 1, -8, -8, 1, -8, -8, 0, -6, -5, 1, -5, -2, 0, -2, -2, 0, -3, 0, 1, -1};
static signed char HF3_ctbl_a3_8nit[30] = {0, 0, 0, 0, 0, 0,-12, 2, -11, -10, -2, -10, -10, -1, -10, -7, 1, -7, -5, 0, -5, -1, 0, -2, -1, 0, -2, 0, 1, -1};
static signed char HF3_ctbl_a3_9nit[30] = {0, 0, 0, 0, 0, 0,-16, -2, -16, -8, 0, -8, -9, -1, -10, -8, -1, -8, -5, 0, -5, 0, 0, -2, -2, 0, -2, 0, 0, -1};
static signed char HF3_ctbl_a3_10nit[30] = {0, 0, 0, 0, 0, 0,-10, 1, -12, -10, -1, -10, -6, 1, -8, -8, 0, -7, -5, 0, -5, -1, 0, -2, -1, 0, -1, 0, 0, -1};
static signed char HF3_ctbl_a3_11nit[30] = {0, 0, 0, 0, 0, 0,-11, 1, -12, -7, 0, -9, -6, 1, -7, -8, -1, -9, -6, -1, -5, -1, 0, -1, 0, 0, -2, 0, 0, 0};
static signed char HF3_ctbl_a3_12nit[30] = {0, 0, 0, 0, 0, 0,-11, 1, -13, -6, 1, -7, -5, 1, -8, -8, -1, -8, -5, -1, -5, 0, 0, -1, -1, 0, -2, 1, 0, 1};
static signed char HF3_ctbl_a3_13nit[30] = {0, 0, 0, 0, 0, 0,-12, 1, -14, -5, 1, -7, -6, 1, -7, -7, -1, -8, -4, 0, -4, 0, 0, -1, 0, 1, -1, 2, 0, 2};
static signed char HF3_ctbl_a3_14nit[30] = {0, 0, 0, 0, 0, 0,-14, 1, -15, -4, 0, -8, -6, 1, -6, -6, 0, -7, -3, 0, -4, 0, 0, 0, 0, 1, -1, 2, 0, 2};
static signed char HF3_ctbl_a3_15nit[30] = {0, 0, 0, 0, 0, 0,-16, 0, -16, -5, 0, -9, -6, 0, -7, -6, 0, -6, -3, 0, -4, 0, 0, 0, -1, 0, -2, 2, 0, 2};
static signed char HF3_ctbl_a3_16nit[30] = {0, 0, 0, 0, 0, 0,-15, -1, -16, -6, -1, -10, -6, 0, -6, -7, -1, -8, -3, -1, -4, -1, 0, -1, 0, 0, -1, 2, 0, 2};
static signed char HF3_ctbl_a3_17nit[30] = {0, 0, 0, 0, 0, 0,-17, -1, -17, -7, 0, -11, -6, 0, -6, -5, -1, -7, -3, 1, -2, 0, 0, -1, 0, 0, -1, 2, 0, 2};
static signed char HF3_ctbl_a3_19nit[30] = {0, 0, 0, 0, 0, 0,-12, 1, -15, -4, 1, -7, -6, 0, -6, -5, 0, -6, -2, 0, -2, 1, 0, -1, 0, 0, -1, 2, 0, 2};
static signed char HF3_ctbl_a3_20nit[30] = {0, 0, 0, 0, 0, 0,-13, 0, -15, -6, 0, -9, -5, 0, -6, -6, -1, -6, -3, -1, -4, 1, 0, 0, 0, 0, -1, 2, 0, 2};
static signed char HF3_ctbl_a3_21nit[30] = {0, 0, 0, 0, 0, 0,-12, 0, -15, -5, 0, -9, -7, -1, -6, -6, -1, -7, -1, 0, -2, 0, 0, 0, 0, 0, -1, 2, 0, 2};
static signed char HF3_ctbl_a3_22nit[30] = {0, 0, 0, 0, 0, 0,-11, 1, -14, -6, 1, -9, -6, -2, -7, -4, 1, -4, -1, 1, -1, 1, 0, 0, 0, 0, -1, 2, 0, 2};
static signed char HF3_ctbl_a3_24nit[30] = {0, 0, 0, 0, 0, 0,-10, 1, -14, -7, 0, -8, -4, 1, -5, -5, -1, -5, -1, 1, -1, 1, 0, 0, 0, 0, -1, 2, 0, 2};
static signed char HF3_ctbl_a3_25nit[30] = {0, 0, 0, 0, 0, 0,-11, 1, -14, -7, -1, -9, -4, 1, -4, -5, -1, -5, -1, 1, -1, 1, 0, 0, 0, 0, -1, 2, 0, 2};
static signed char HF3_ctbl_a3_27nit[30] = {0, 0, 0, 0, 0, 0,-12, 0, -15, -8, -1, -9, -5, 0, -5, -5, -1, -5, -1, 0, -1, 0, -1, -1, 0, 1, -1, 3, 0, 3};
static signed char HF3_ctbl_a3_29nit[30] = {0, 0, 0, 0, 0, 0,-13, -1, -17, -9, -3, -10, -6, -1, -5, -4, -1, -5, -1, 0, -1, 0, 0, -1, 0, 0, -1, 3, 0, 3};
static signed char HF3_ctbl_a3_30nit[30] = {0, 0, 0, 0, 0, 0,-11, 0, -15, -6, -1, -9, -5, 0, -5, -2, 1, -2, -2, 0, -2, 1, 0, 0, 0, 0, -1, 3, 0, 3};
static signed char HF3_ctbl_a3_32nit[30] = {0, 0, 0, 0, 0, 0,-12, -2, -17, -7, -1, -9, -6, -1, -5, -4, -1, -5, 0, 0, -1, 1, 0, 0, 0, 0, -1, 3, 0, 3};
static signed char HF3_ctbl_a3_34nit[30] = {0, 0, 0, 0, 0, 0,-13, -2, -18, -5, -1, -7, -4, -1, -4, -1, 1, -2, -1, 0, -1, 1, 0, 0, 0, 0, -1, 3, 0, 3};
static signed char HF3_ctbl_a3_37nit[30] = {0, 0, 0, 0, 0, 0,-8, 2, -13, -3, 0, -5, -5, -1, -5, 0, 2, 0, 0, 1, 0, 1, 0, 0, 0, 0, -1, 3, 0, 3};
static signed char HF3_ctbl_a3_39nit[30] = {0, 0, 0, 0, 0, 0,-9, 0, -14, -4, 0, -5, -4, -1, -5, 0, 2, -1, 0, 1, 1, 1, 0, 0, 0, 0, -1, 3, 0, 3};
static signed char HF3_ctbl_a3_41nit[30] = {0, 0, 0, 0, 0, 0,-8, 2, -14, -1, 1, -3, -2, 1, -1, 1, 2, -1, -1, 0, 0, 1, 0, 0, 0, 0, -1, 3, 0, 3};
static signed char HF3_ctbl_a3_44nit[30] = {0, 0, 0, 0, 0, 0,-11, 0, -16, -1, 0, -4, -1, 1, -1, -1, 1, -1, 0, 0, 0, 1, 0, 0, 0, 0, -1, 3, 0, 3};
static signed char HF3_ctbl_a3_47nit[30] = {0, 0, 0, 0, 0, 0,-6, 4, -10, -2, 0, -4, 0, 1, -1, -2, 0, -2, 0, 0, 0, 1, 0, 0, 0, 0, -1, 3, 0, 3};
static signed char HF3_ctbl_a3_50nit[30] = {0, 0, 0, 0, 0, 0,-6, 3, -11, -3, -2, -5, -1, 1, -1, -2, -1, -2, 1, 0, 0, 1, 0, 0, 0, 0, -1, 3, 0, 3};
static signed char HF3_ctbl_a3_53nit[30] = {0, 0, 0, 0, 0, 0,-8, 3, -11, -2, -2, -5, -2, -1, -2, -2, -1, -3, 0, 0, 0, 2, 0, 1, 0, 0, -1, 3, 0, 3};
static signed char HF3_ctbl_a3_56nit[30] = {0, 0, 0, 0, 0, 0,-8, 2, -12, -3, -2, -5, -3, -1, -2, -1, -1, -3, 0, -1, 0, 1, 0, 0, 0, 0, -1, 3, 0, 3};
static signed char HF3_ctbl_a3_60nit[30] = {0, 0, 0, 0, 0, 0,-8, 1, -13, -1, -1, -3, -2, 0, -2, 0, 1, 0, 1, 0, 0, 1, -1, 0, 0, 0, -1, 3, 0, 3};
static signed char HF3_ctbl_a3_64nit[30] = {0, 0, 0, 0, 0, 0,-10, 0, -13, -3, -2, -4, -1, 0, -2, 0, 0, -1, 0, 0, 0, 1, -1, 0, 0, 0, -1, 3, 0, 3};
static signed char HF3_ctbl_a3_68nit[30] = {0, 0, 0, 0, 0, 0,-10, -1, -14, -2, -3, -4, -2, 0, -2, 1, 1, 0, 0, 0, 1, 1, 0, 0, 0, 1, 0, 3, -1, 3};
static signed char HF3_ctbl_a3_72nit[30] = {0, 0, 0, 0, 0, 0,-3, -1, -1, 0, 1, 0, -1, -2, -1, 1, 1, 1, 1, 1, 0, -1, 0, 0, 1, 0, 2, 3, -1, 3};
static signed char HF3_ctbl_a3_77nit[30] = {0, 0, 0, 0, 0, 0,4, 10, 0, 3, 0, -1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 1, 0, 0, 2, -1, 3};
static signed char HF3_ctbl_a3_82nit[30] = {0, 0, 0, 0, 0, 0,-9, 1, -11, -1, -3, -4, 0, 1, 0, 0, 0, -1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 2, -1, 3};
static signed char HF3_ctbl_a3_87nit[30] = {0, 0, 0, 0, 0, 0,-10, 0, -11, 0, -2, -3, -1, 1, 0, 1, 0, -1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 2, -1, 3};
static signed char HF3_ctbl_a3_93nit[30] = {0, 0, 0, 0, 0, 0,-5, 3, -7, 0, -2, -2, -3, -1, -3, -1, -1, -1, 1, 0, 0, 1, 0, 1, 0, 0, -1, 2, -1, 2};
static signed char HF3_ctbl_a3_98nit[30] = {0, 0, 0, 0, 0, 0,-7, 2, -7, 0, -1, -2, -1, 0, -1, 0, 1, 0, 0, -1, 0, 1, 0, 0, 0, 0, 0, 2, -1, 3};
static signed char HF3_ctbl_a3_105nit[30] = {0, 0, 0, 0, 0, 0,-5, 1, -8, -2, -2, -2, -1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 1, 2, 0, 3};
static signed char HF3_ctbl_a3_111nit[30] = {0, 0, 0, 0, 0, 0,-9, -1, -9, 3, 1, 1, -2, -1, -2, 1, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, -1, 3, 0, 3};
static signed char HF3_ctbl_a3_119nit[30] = {0, 0, 0, 0, 0, 0,-10, -2, -10, 0, -1, -2, -2, 0, 0, 1, 1, 0, 0, -1, 0, 1, 0, 0, -1, 0, 0, 3, 0, 3};
static signed char HF3_ctbl_a3_126nit[30] = {0, 0, 0, 0, 0, 0,-5, 2, -4, 2, 2, 1, 0, 0, -1, 0, 1, 0, 0, -1, 0, 0, 0, -1, 0, 0, 1, 2, -1, 1};
static signed char HF3_ctbl_a3_134nit[30] = {0, 0, 0, 0, 0, 0,-4, 2, -4, 1, 1, 1, 1, 2, 1, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 1, 0, 2, -1, 2};
static signed char HF3_ctbl_a3_143nit[30] = {0, 0, 0, 0, 0, 0,-3, 4, -1, 0, -1, -1, -1, 0, 0, 0, 0, -1, 1, 0, 1, 0, 0, 0, -1, 0, 0, 2, -1, 2};
static signed char HF3_ctbl_a3_152nit[30] = {0, 0, 0, 0, 0, 0,-2, 3, -1, 2, 2, 2, -1, 0, -1, 1, 0, 1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, -1, 2};
static signed char HF3_ctbl_a3_162nit[30] = {0, 0, 0, 0, 0, 0,-3, 1, -2, -1, 0, 0, -2, 0, -2, 1, 0, 1, 0, 0, -1, 1, 0, 0, -1, 0, 0, 2, -1, 1};
static signed char HF3_ctbl_a3_172nit[30] = {0, 0, 0, 0, 0, 0,-7, -1, -5, -1, 0, 0, -1, 0, -1, 0, -1, -1, 0, 0, -1, 0, 0, 1, 0, 0, 0, 2, -1, 2};
static signed char HF3_ctbl_a3_183nit[30] = {0, 0, 0, 0, 0, 0,-1, 4, 0, 0, 0, 0, -2, -1, -2, -1, -1, -1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, -1, 2};
static signed char HF3_ctbl_a3_195nit[30] = {0, 0, 0, 0, 0, 0,1, 4, 2, -1, 0, 0, -1, 0, -1, 1, 0, 0, 0, 1, 1, 1, 0, 0, -1, 0, 0, 1, -1, 1};
static signed char HF3_ctbl_a3_207nit[30] = {0, 0, 0, 0, 0, 0,0, 3, 1, -1, 0, 0, -1, 0, -1, 1, 0, 0, -1, 0, 0, 1, 0, 0, -1, 0, 0, 1, -1, 1};
static signed char HF3_ctbl_a3_220nit[30] = {0, 0, 0, 0, 0, 0,0, 3, 1, -1, 0, 0, -1, 0, -1, 1, 0, 0, -1, 0, 0, 1, 0, 0, -1, 0, 0, 1, -1, 1};
static signed char HF3_ctbl_a3_234nit[30] = {0, 0, 0, 0, 0, 0,0, 3, 1, -1, 0, 0, -1, 0, -1, 1, 0, 0, -1, 0, 0, 1, 0, 0, -1, 0, 0, 1, -1, 1};
static signed char HF3_ctbl_a3_249nit[30] = {0, 0, 0, 0, 0, 0,0, 1, 0, 0, -1, 0, 0, 1, 0, 1, 1, 1, 0, -1, -1, 0, 0, 0, -1, 0, 0, 1, -1, 1};
static signed char HF3_ctbl_a3_265nit[30] = {0, 0, 0, 0, 0, 0,-3, 0, -1, -2, -3, -2, -3, -1, -2, 1, -1, 0, -1, 0, 0, 0, 0, 0, 0, 0, 1, 1, -1, 0};
static signed char HF3_ctbl_a3_282nit[30] = {0, 0, 0, 0, 0, 0,-2, 0, -1, -3, -3, -2, 1, 1, 1, -1, -1, -1, 1, 0, 0, 0, -1, 0, 0, 0, 0, 0, -1, 0};
static signed char HF3_ctbl_a3_300nit[30] = {0, 0, 0, 0, 0, 0,1, 2, 2, 0, 0, 1, 0, 0, -1, -1, -2, -1, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, -1, 0};
static signed char HF3_ctbl_a3_316nit[30] = {0, 0, 0, 0, 0, 0,1, 2, 2, 0, -1, 1, -1, 0, -1, 1, 0, 1, 1, 1, 0, 0, 0, 1, 0, 0, 0, 0, -1, -1};
static signed char HF3_ctbl_a3_333nit[30] = {0, 0, 0, 0, 0, 0,0, 1, 2, 1, 0, 1, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -1, -1};
static signed char HF3_ctbl_a3_350nit[30] = {0, 0, 0, 0, 0, 0,-3, -2, 0, -1, -2, -1, 1, 1, 1, 0, 0, 0, 0, 0, -1, 0, 0, 1, 0, 0, 0, 1, 1, 1};
static signed char HF3_ctbl_a3_357nit[30] = {0, 0, 0, 0, 0, 0,-2, -2, 0, -1, -1, 0, 1, 1, 1, 1, 1, 1, 0, 0, -1, 0, -1, 0, -1, 0, 0, 2, 1, 1};
static signed char HF3_ctbl_a3_365nit[30] = {0, 0, 0, 0, 0, 0,-2, -1, 0, -1, -2, -1, 1, 2, 2, 1, 0, 1, 0, 0, -1, 0, 0, 0, 0, 0, 0, 2, 1, 2};
static signed char HF3_ctbl_a3_372nit[30] = {0, 0, 0, 0, 0, 0,-1, 0, 2, -1, -2, -1, -1, 0, -1, 0, -1, 0, 0, 0, -1, 0, 0, 1, 0, 0, 0, 2, 1, 1};
static signed char HF3_ctbl_a3_380nit[30] = {0, 0, 0, 0, 0, 0,0, 0, 2, -1, -2, -1, 0, 1, 1, 0, 0, 0, 1, 1, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0};
static signed char HF3_ctbl_a3_387nit[30] = {0, 0, 0, 0, 0, 0,0, 0, 2, -1, -1, 0, 1, 1, 0, 0, 0, 1, 0, 0, -1, 1, 0, 1, 0, 0, 0, 0, 0, 0};
static signed char HF3_ctbl_a3_395nit[30] = {0, 0, 0, 0, 0, 0,0, 0, 2, -1, -1, 0, 1, 1, 0, 0, 0, 1, 0, 0, -1, 1, 0, 1, 0, 0, 0, 0, 0, 0};
static signed char HF3_ctbl_a3_403nit[30] = {0, 0, 0, 0, 0, 0,0, 0, 2, -1, -1, 0, 1, 1, 0, 0, 0, 1, 0, 0, -1, 1, 0, 1, 0, 0, 0, 0, 0, 0};
static signed char HF3_ctbl_a3_412nit[30] = {0, 0, 0, 0, 0, 0,0, 0, 2, -1, -1, 0, 1, 1, 0, 0, 0, 1, 0, 0, -1, 1, 0, 1, 0, 0, 0, 0, 0, 0};
static signed char HF3_ctbl_a3_420nit[30] = {0, 0, 0, 0, 0, 0,0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

static signed char HF3_aid9802[3] = {0xB1, 0x90, 0xE1};
static signed char HF3_aid9717[3] = {0xB1, 0x90, 0xCB};
static signed char HF3_aid9612[3] = {0xB1, 0x90, 0xB0};
static signed char HF3_aid9527[3] = {0xB1, 0x90, 0x9A};
static signed char HF3_aid9426[3] = {0xB1, 0x90, 0x80};
static signed char HF3_aid9318[3] = {0xB1, 0x90, 0x64};
static signed char HF3_aid9233[3] = {0xB1, 0x90, 0x4E};
static signed char HF3_aid9132[3] = {0xB1, 0x90, 0x34};
static signed char HF3_aid9039[3] = {0xB1, 0x90, 0x1C};
static signed char HF3_aid8957[3] = {0xB1, 0x90, 0x07};
static signed char HF3_aid8880[3] = {0xB1, 0x80, 0xF3};
static signed char HF3_aid8810[3] = {0xB1, 0x80, 0xE1};
static signed char HF3_aid8740[3] = {0xB1, 0x80, 0xCF};
static signed char HF3_aid8640[3] = {0xB1, 0x80, 0xB5};
static signed char HF3_aid8539[3] = {0xB1, 0x80, 0x9B};
static signed char HF3_aid8461[3] = {0xB1, 0x80, 0x87};
static signed char HF3_aid8252[3] = {0xB1, 0x80, 0x51};
static signed char HF3_aid8190[3] = {0xB1, 0x80, 0x41};
static signed char HF3_aid8089[3] = {0xB1, 0x80, 0x27};
static signed char HF3_aid7996[3] = {0xB1, 0x80, 0x0F};
static signed char HF3_aid7779[3] = {0xB1, 0x70, 0xD7};
static signed char HF3_aid7709[3] = {0xB1, 0x70, 0xC5};
static signed char HF3_aid7523[3] = {0xB1, 0x70, 0x95};
static signed char HF3_aid7337[3] = {0xB1, 0x70, 0x65};
static signed char HF3_aid7267[3] = {0xB1, 0x70, 0x53};
static signed char HF3_aid7074[3] = {0xB1, 0x70, 0x21};
static signed char HF3_aid6888[3] = {0xB1, 0x60, 0xF1};
static signed char HF3_aid6601[3] = {0xB1, 0x60, 0xA7};
static signed char HF3_aid6430[3] = {0xB1, 0x60, 0x7B};
static signed char HF3_aid6213[3] = {0xB1, 0x60, 0x43};
static signed char HF3_aid5919[3] = {0xB1, 0x50, 0xF7};
static signed char HF3_aid5647[3] = {0xB1, 0x50, 0xB1};
static signed char HF3_aid5322[3] = {0xB1, 0x50, 0x5D};
static signed char HF3_aid5035[3] = {0xB1, 0x50, 0x13};
static signed char HF3_aid4740[3] = {0xB1, 0x40, 0xC7};
static signed char HF3_aid4345[3] = {0xB1, 0x40, 0x61};
static signed char HF3_aid3922[3] = {0xB1, 0x30, 0xF4};
static signed char HF3_aid3829[3] = {0xB1, 0x30, 0xDC};
static signed char HF3_aid3368[3] = {0xB1, 0x30, 0x65};
static signed char HF3_aid2946[3] = {0xB1, 0x20, 0xF8};
static signed char HF3_aid2391[3] = {0xB1, 0x20, 0x69};
static signed char HF3_aid1818[3] = {0xB1, 0x10, 0xD5};
static signed char HF3_aid1260[3] = {0xB1, 0x10, 0x45};
static signed char HF3_aid1143[3] = {0xB1, 0x10, 0x27};
static signed char HF3_aid938[3] = {0xB1, 0x00, 0xF2};
static signed char HF3_aid771[3] = {0xB1, 0x00, 0xC7};
static signed char HF3_aid570[3] = {0xB1, 0x00, 0x93};
static signed char HF3_aid318[3] = {0xB1, 0x00, 0x52};



#ifdef CONFIG_LCD_HMT

static signed char HF3_HMTrtbl10nit[10] = {0, 7, 7, 6, 5, 4, 3, 0, 2, 0};
static signed char HF3_HMTrtbl11nit[10] = {0, 8, 7, 5, 5, 3, 2, -1, 0, 0};
static signed char HF3_HMTrtbl12nit[10] = {0, 6, 7, 5, 4, 3, 2, -1, -1, 0};
static signed char HF3_HMTrtbl13nit[10] = {0, 11, 6, 6, 5, 4, 3, 1, 2, 0};
static signed char HF3_HMTrtbl14nit[10] = {0, 14, 6, 5, 4, 3, 2, 1, 1, 0};
static signed char HF3_HMTrtbl15nit[10] = {0, 11, 6, 5, 4, 2, 1, 0, 1, 0};
static signed char HF3_HMTrtbl16nit[10] = {0, 10, 6, 6, 5, 4, 1, 0, 1, 0};
static signed char HF3_HMTrtbl17nit[10] = {0, 10, 6, 5, 4, 3, 2, 1, 1, 0};
static signed char HF3_HMTrtbl19nit[10] = {0, 13, 6, 5, 4, 3, 2, 1, 1, 0};
static signed char HF3_HMTrtbl20nit[10] = {0, 12, 6, 5, 4, 3, 1, 0, 1, 0};
static signed char HF3_HMTrtbl21nit[10] = {0, 11, 5, 4, 4, 3, 2, 0, 1, 0};
static signed char HF3_HMTrtbl22nit[10] = {0, 12, 6, 5, 4, 3, 1, 1, 1, 0};
static signed char HF3_HMTrtbl23nit[10] = {0, 9, 6, 5, 4, 3, 1, 1, 2, 0};
static signed char HF3_HMTrtbl25nit[10] = {0, 15, 6, 5, 3, 3, 2, 2, 2, 0};
static signed char HF3_HMTrtbl27nit[10] = {0, 8, 7, 5, 4, 4, 2, 3, 3, 0};
static signed char HF3_HMTrtbl29nit[10] = {0, 9, 7, 6, 4, 5, 2, 3, 3, 0};
static signed char HF3_HMTrtbl31nit[10] = {0, 10, 6, 6, 5, 4, 3, 4, 4, 0};
static signed char HF3_HMTrtbl33nit[10] = {0, 10, 6, 5, 4, 4, 3, 4, 4, 0};
static signed char HF3_HMTrtbl35nit[10] = {0, 5, 7, 6, 5, 4, 3, 6, 4, 0};
static signed char HF3_HMTrtbl37nit[10] = {0, 6, 7, 6, 4, 4, 4, 5, 4, 0};
static signed char HF3_HMTrtbl39nit[10] = {0, 7, 7, 5, 5, 4, 3, 5, 4, 0};
static signed char HF3_HMTrtbl41nit[10] = {0, 10, 6, 5, 4, 4, 4, 4, 3, 0};
static signed char HF3_HMTrtbl44nit[10] = {0, 7, 6, 5, 5, 3, 4, 4, 3, 0};
static signed char HF3_HMTrtbl47nit[10] = {0, 7, 6, 5, 4, 3, 4, 6, 4, 0};
static signed char HF3_HMTrtbl50nit[10] = {0, 8, 6, 5, 4, 3, 4, 5, 3, 0};
static signed char HF3_HMTrtbl53nit[10] = {0, 10, 6, 5, 5, 5, 5, 5, 3, 0};
static signed char HF3_HMTrtbl56nit[10] = {0, 10, 6, 5, 4, 4, 5, 7, 4, 0};
static signed char HF3_HMTrtbl60nit[10] = {0, 6, 7, 5, 5, 4, 5, 7, 3, 0};
static signed char HF3_HMTrtbl64nit[10] = {0, 7, 6, 4, 5, 4, 5, 7, 3, 0};
static signed char HF3_HMTrtbl68nit[10] = {0, 7, 6, 5, 4, 4, 4, 6, 3, 0};
static signed char HF3_HMTrtbl72nit[10] = {0, 9, 6, 4, 5, 4, 5, 7, 3, 0};
static signed char HF3_HMTrtbl77nit[10] = {0, 3, 2, 2, 3, 1, 2, 4, 3, 0};
static signed char HF3_HMTrtbl82nit[10] = {0, 1, 3, 3, 3, 2, 4, 5, 3, 0};
static signed char HF3_HMTrtbl87nit[10] = {0, 1, 2, 2, 2, 2, 4, 6, 3, 0};
static signed char HF3_HMTrtbl93nit[10] = {0, 1, 2, 3, 3, 3, 4, 5, 2, 0};
static signed char HF3_HMTrtbl99nit[10] = {0, 1, 2, 2, 2, 2, 5, 5, 3, 0};
static signed char HF3_HMTrtbl105nit[10] = {0, 2, 2, 3, 3, 3, 5, 7, 4, 0};

static signed char HF3_HMTctbl10nit[30] = {0, 0, 0, 0, 0, 0,-14, 2, -5, -7, 2, -5, -6, 3, -6, -4, 4, -8, -6, 2, -4, 0, 0, 0, 4, 0, 2, 0, 0, 0};
static signed char HF3_HMTctbl11nit[30] = {0, 0, 0, 0, 0, 0,-12, 2, -4, -9, 3, -6, -6, 2, -6, -5, 3, -8, -5, 2, -4, 0, 0, -1, 4, 0, 3, 0, 0, 0};
static signed char HF3_HMTctbl12nit[30] = {0, 0, 0, 0, 0, 0,-10, 1, -3, -9, 2, -4, -5, 1, -3, -6, 3, -7, -4, 1, -4, 0, 0, 0, 5, 0, 3, 0, 0, 0};
static signed char HF3_HMTctbl13nit[30] = {0, 0, 0, 0, 0, 0,-13, 1, -3, -8, 2, -4, -7, 1, -4, -6, 3, -7, -2, 1, -2, 1, 0, 0, 0, 0, 0, 0, 0, 0};
static signed char HF3_HMTctbl14nit[30] = {0, 0, 0, 0, 0, 0,-13, 1, -3, -8, 2, -4, -7, 1, -4, -6, 3, -8, -3, 1, -3, 3, 0, 2, -1, 0, -1, 1, 0, 1};
static signed char HF3_HMTctbl15nit[30] = {0, 0, 0, 0, 0, 0,-13, 1, -3, -8, 2, -4, -6, 1, -4, -7, 3, -8, -3, 1, -2, 4, 0, 2, 0, 0, -1, 1, 0, 2};
static signed char HF3_HMTctbl16nit[30] = {0, 0, 0, 0, 0, 0,-15, 2, -4, -7, 1, -4, -5, 1, -4, -5, 2, -6, -3, 1, -3, 3, 0, 2, 0, 0, -2, 1, 0, 2};
static signed char HF3_HMTctbl17nit[30] = {0, 0, 0, 0, 0, 0,-14, 1, -4, -10, 2, -5, -5, 1, -4, -5, 3, -6, -3, 1, -3, 3, 0, 2, 0, 0, -2, 2, 0, 3};
static signed char HF3_HMTctbl19nit[30] = {0, 0, 0, 0, 0, 0,-16, 1, -4, -6, 1, -4, -5, 2, -4, -6, 2, -6, -2, 1, -2, 2, 0, 1, 0, 0, 0, 2, 0, 3};
static signed char HF3_HMTctbl20nit[30] = {0, 0, 0, 0, 0, 0,-14, 2, -4, -6, 1, -3, -4, 2, -4, -6, 2, -6, -2, 1, -2, 3, 0, 2, 0, 0, -1, 3, 0, 4};
static signed char HF3_HMTctbl21nit[30] = {0, 0, 0, 0, 0, 0,-14, 2, -4, -7, 1, -4, -4, 2, -4, -6, 3, -6, -1, 1, -2, 3, 0, 1, -1, 0, -1, 3, 0, 4};
static signed char HF3_HMTctbl22nit[30] = {0, 0, 0, 0, 0, 0,-15, 3, -8, -7, 3, -7, -5, 2, -6, -6, 2, -6, -3, 0, -2, 3, 0, 2, -1, 0, -2, 3, 0, 4};
static signed char HF3_HMTctbl23nit[30] = {0, 0, 0, 0, 0, 0,-15, 3, -8, -6, 3, -6, -5, 3, -6, -5, 2, -5, -1, 1, -2, 3, 0, 2, -2, 0, -2, 4, 0, 4};
static signed char HF3_HMTctbl25nit[30] = {0, 0, 0, 0, 0, 0,-16, 3, -8, -5, 3, -6, -6, 3, -6, -4, 2, -5, 0, 0, -1, -2, 0, -2, 0, 0, 0, 4, 0, 4};
static signed char HF3_HMTctbl27nit[30] = {0, 0, 0, 0, 0, 0,-13, 3, -8, -5, 2, -6, -6, 3, -7, -3, 2, -4, 0, 0, -1, -2, 0, -2, -1, 0, -1, 4, -1, 4};
static signed char HF3_HMTctbl29nit[30] = {0, 0, 0, 0, 0, 0,-15, 4, -10, -5, 2, -6, -4, 3, -6, -5, 1, -4, 0, 0, -1, -2, 0, -2, -1, 0, -1, 4, -1, 4};
static signed char HF3_HMTctbl31nit[30] = {0, 0, 0, 0, 0, 0,-13, 4, -9, -5, 3, -6, -5, 3, -6, -5, 1, -4, 1, 0, 0, -4, 0, -4, -1, 0, -1, 4, -1, 4};
static signed char HF3_HMTctbl33nit[30] = {0, 0, 0, 0, 0, 0,-15, 4, -9, -5, 3, -6, -5, 3, -6, -5, 1, -4, 1, 0, 0, -3, 0, -3, -1, 0, -1, 5, -1, 5};
static signed char HF3_HMTctbl35nit[30] = {0, 0, 0, 0, 0, 0,-12, 4, -8, -5, 3, -6, -5, 2, -6, -5, 1, -4, 1, 0, 0, -2, 0, -2, -1, 0, -1, 5, -1, 5};
static signed char HF3_HMTctbl37nit[30] = {0, 0, 0, 0, 0, 0,-11, 4, -8, -4, 2, -6, -6, 3, -6, -3, 1, -4, 0, 0, 0, -3, 0, -2, 0, 0, -1, 5, -1, 5};
static signed char HF3_HMTctbl39nit[30] = {0, 0, 0, 0, 0, 0,-12, 4, -9, -5, 2, -6, -6, 2, -6, -3, 1, -4, 1, 0, 0, -4, 0, -2, -1, 0, -1, 7, -1, 6};
static signed char HF3_HMTctbl41nit[30] = {0, 0, 0, 0, 0, 0,-15, 4, -8, -5, 3, -6, -5, 2, -6, -2, 1, -3, 0, 0, 0, -3, 0, -3, -1, 0, 0, 6, -1, 5};
static signed char HF3_HMTctbl44nit[30] = {0, 0, 0, 0, 0, 0,-13, 4, -9, -4, 2, -5, -5, 2, -6, -4, 1, -3, 2, 0, 1, -3, 0, -2, -1, 0, -1, 7, -1, 6};
static signed char HF3_HMTctbl47nit[30] = {0, 0, 0, 0, 0, 0,-14, 4, -9, -3, 2, -5, -6, 2, -6, -3, 1, -2, 1, 0, 0, -2, 0, -1, -1, 0, -1, 7, -1, 6};
static signed char HF3_HMTctbl50nit[30] = {0, 0, 0, 0, 0, 0,-13, 4, -9, -4, 3, -6, -4, 2, -6, -3, 1, -3, 1, 0, 0, -3, 0, -2, -1, 0, 0, 8, -1, 6};
static signed char HF3_HMTctbl53nit[30] = {0, 0, 0, 0, 0, 0,-14, 4, -9, -3, 2, -6, -6, 2, -6, -2, 1, -2, 1, 0, 1, -4, 0, -3, 0, 0, 0, 7, -1, 6};
static signed char HF3_HMTctbl56nit[30] = {0, 0, 0, 0, 0, 0,-13, 4, -9, -4, 2, -6, -5, 2, -5, -1, 1, -2, 0, 0, 0, -3, 0, -2, 0, 0, 0, 8, -1, 7};
static signed char HF3_HMTctbl60nit[30] = {0, 0, 0, 0, 0, 0,-12, 3, -8, -4, 3, -6, -5, 2, -5, -2, 0, -2, 0, 0, 0, -3, 0, -2, -1, 0, -2, 9, -1, 8};
static signed char HF3_HMTctbl64nit[30] = {0, 0, 0, 0, 0, 0,-12, 3, -8, -4, 3, -6, -5, 2, -4, -2, 1, -2, 0, 0, 0, -2, 0, -1, -1, 0, -1, 9, -1, 8};
static signed char HF3_HMTctbl68nit[30] = {0, 0, 0, 0, 0, 0,-12, 3, -8, -2, 3, -6, -6, 2, -5, -1, 1, -2, 0, 0, 0, -1, 0, -1, 0, 0, 0, 9, -1, 7};
static signed char HF3_HMTctbl72nit[30] = {0, 0, 0, 0, 0, 0,-12, 3, -8, -4, 3, -7, -4, 2, -4, -4, 1, -3, 1, 0, 0, -4, 0, -2, 0, 0, -1, 9, -1, 8};
static signed char HF3_HMTctbl77nit[30] = {0, 0, 0, 0, 0, 0,-3, 4, -10, -2, 2, -6, -5, 1, -2, -1, 0, -1, 0, 0, 0, -2, 0, -1, 0, 0, -1, 6, -1, 7};
static signed char HF3_HMTctbl82nit[30] = {0, 0, 0, 0, 0, 0,-1, 5, -11, -3, 2, -4, -5, 0, -2, -1, 0, -2, 0, 0, 0, -2, 0, -1, 0, 0, -1, 5, -1, 6};
static signed char HF3_HMTctbl87nit[30] = {0, 0, 0, 0, 0, 0,-1, 5, -11, -3, 1, -4, -3, 1, -2, 0, 0, 1, -1, 0, -1, -2, 0, -2, 0, 0, 0, 5, -1, 6};
static signed char HF3_HMTctbl93nit[30] = {0, 0, 0, 0, 0, 0,0, 6, -12, -4, 1, -4, -3, 1, -2, 1, 0, 0, -3, 0, -1, -1, 0, -1, -1, 0, -2, 7, -1, 8};
static signed char HF3_HMTctbl99nit[30] = {0, 0, 0, 0, 0, 0,0, 6, -12, -3, 1, -4, -4, 0, -2, 1, 0, 1, -3, 0, -2, 0, 0, -1, 0, 0, -2, 7, -1, 8};
static signed char HF3_HMTctbl105nit[30] = {0, 0, 0, 0, 0, 0,-1, 6, -12, -5, 1, -3, -1, 0, -1, 0, 0, 0, -3, 0, -2, 0, 0, -1, -1, 0, -2, 6, -2, 8};


static signed char HF3_HMTaid8001[3] = {0xB1, 0x20, 0x03};
static signed char HF3_HMTaid6999[3] = {0xB1, 0X30, 0x05};

static unsigned char HF3_HMTelv[3] = {
        0xB5, 0xAC, 0x0A
};
static unsigned char HF3_HMTelvCaps[3] = {
        0xB5, 0xBC, 0x0A
};


static signed char HF3_A3_HMTrtbl10nit[10] = {0, 2, 4, 3, 3, 2, 1, -1, 1, 0};
static signed char HF3_A3_HMTrtbl11nit[10] = {0, 6, 4, 2, 2, 1, 1, -2, 0, 0};
static signed char HF3_A3_HMTrtbl12nit[10] = {0, 9, 4, 3, 2, 1, 0, -1, 0, 0};
static signed char HF3_A3_HMTrtbl13nit[10] = {0, 11, 4, 3, 2, 1, 0, -1, 1, 0};
static signed char HF3_A3_HMTrtbl14nit[10] = {0, 11, 3, 2, 2, 1, -1, -1, 1, 0};
static signed char HF3_A3_HMTrtbl15nit[10] = {0, 3, 4, 3, 2, 1, 0, 0, 1, 0};
static signed char HF3_A3_HMTrtbl16nit[10] = {0, 2, 4, 3, 3, 1, 0, -1, 0, 0};
static signed char HF3_A3_HMTrtbl17nit[10] = {0, 3, 4, 3, 2, 1, 0, -1, 0, 0};
static signed char HF3_A3_HMTrtbl19nit[10] = {0, 4, 4, 2, 2, 1, -1, -1, 1, 0};
static signed char HF3_A3_HMTrtbl20nit[10] = {0, 5, 4, 3, 2, 1, -1, 0, 2, 0};
static signed char HF3_A3_HMTrtbl21nit[10] = {0, 6, 3, 3, 2, 1, 0, 0, 1, 0};
static signed char HF3_A3_HMTrtbl22nit[10] = {0, 5, 3, 2, 2, 1, -1, -1, 0, 0};
static signed char HF3_A3_HMTrtbl23nit[10] = {0, 7, 3, 3, 2, 1, -1, -1, 1, 0};
static signed char HF3_A3_HMTrtbl25nit[10] = {0, 7, 3, 3, 3, 1, 0, 2, 3, 0};
static signed char HF3_A3_HMTrtbl27nit[10] = {0, 7, 3, 3, 2, 1, 1, 1, 2, 0};
static signed char HF3_A3_HMTrtbl29nit[10] = {0, 12, 3, 3, 2, 2, 0, 1, 2, 0};
static signed char HF3_A3_HMTrtbl31nit[10] = {0, 1, 4, 3, 2, 2, 0, 2, 3, 0};
static signed char HF3_A3_HMTrtbl33nit[10] = {0, 4, 3, 3, 2, 1, 1, 2, 3, 0};
static signed char HF3_A3_HMTrtbl35nit[10] = {0, 3, 3, 3, 2, 2, 1, 2, 3, 0};
static signed char HF3_A3_HMTrtbl37nit[10] = {0, 4, 3, 3, 1, 2, 1, 3, 3, 0};
static signed char HF3_A3_HMTrtbl39nit[10] = {0, 6, 3, 3, 2, 1, 1, 4, 3, 0};
static signed char HF3_A3_HMTrtbl41nit[10] = {0, 2, 4, 3, 2, 1, 2, 4, 4, 0};
static signed char HF3_A3_HMTrtbl44nit[10] = {0, 4, 3, 3, 3, 1, 2, 4, 3, 0};
static signed char HF3_A3_HMTrtbl47nit[10] = {0, 3, 3, 3, 2, 1, 2, 5, 2, 0};
static signed char HF3_A3_HMTrtbl50nit[10] = {0, 4, 3, 3, 1, 1, 2, 4, 2, 0};
static signed char HF3_A3_HMTrtbl53nit[10] = {0, 6, 3, 3, 2, 1, 2, 4, 2, 0};
static signed char HF3_A3_HMTrtbl56nit[10] = {0, 8, 3, 3, 2, 2, 2, 4, 2, 0};
static signed char HF3_A3_HMTrtbl60nit[10] = {0, 4, 3, 2, 2, 1, 2, 5, 1, 0};
static signed char HF3_A3_HMTrtbl64nit[10] = {0, 5, 3, 3, 2, 1, 2, 4, 1, 0};
static signed char HF3_A3_HMTrtbl68nit[10] = {0, 7, 3, 2, 2, 1, 3, 5, 1, 0};
static signed char HF3_A3_HMTrtbl72nit[10] = {0, 1, 4, 3, 2, 1, 3, 4, 1, 0};
static signed char HF3_A3_HMTrtbl77nit[10] = {0, 3, 1, 1, 1, 0, 1, 3, 1, 0};
static signed char HF3_A3_HMTrtbl82nit[10] = {0, 5, 1, 1, 0, 0, 2, 3, 2, 0};
static signed char HF3_A3_HMTrtbl87nit[10] = {0, 1, 1, 1, 0, 0, 3, 3, 2, 0};
static signed char HF3_A3_HMTrtbl93nit[10] = {0, 1, 1, 1, 1, 0, 2, 4, 2, 0};
static signed char HF3_A3_HMTrtbl99nit[10] = {0, 3, 1, 1, 1, 0, 3, 4, 2, 0};
static signed char HF3_A3_HMTrtbl105nit[10] = {0, 3, 1, 1, 0, 0, 3, 4, 2, 0};

static signed char HF3_A3_HMTctbl10nit[30] = {0, 0, 0, 0, 0, 0,-3, 0, 2, -3, 1, -4, -3, 1, -3, -4, 2, -4, -4, 1, -3, -1, 0, 0, 4, 0, 4, -1, 0, 0};
static signed char HF3_A3_HMTctbl11nit[30] = {0, 0, 0, 0, 0, 0,-8, -1, 2, -4, 1, -4, -3, 1, -4, -4, 2, -4, -5, 1, -4, 0, 0, 0, 4, 0, 4, -2, 0, 0};
static signed char HF3_A3_HMTctbl12nit[30] = {0, 0, 0, 0, 0, 0,-7, 0, 0, -3, 1, -3, -1, 1, -3, -6, 2, -6, -4, 1, -3, 1, 0, 1, 2, 0, 2, -1, 0, 0};
static signed char HF3_A3_HMTctbl13nit[30] = {0, 0, 0, 0, 0, 0,-6, 0, 1, -3, 1, -3, -2, 1, -3, -6, 2, -6, -2, 1, -2, 4, 0, 4, 0, 0, 0, -1, 0, 0};
static signed char HF3_A3_HMTctbl14nit[30] = {0, 0, 0, 0, 0, 0,-9, 0, 0, -3, 1, -3, -2, 1, -3, -6, 2, -5, -3, 1, -2, 4, 0, 4, -1, 0, 0, 0, 0, 1};
static signed char HF3_A3_HMTctbl15nit[30] = {0, 0, 0, 0, 0, 0,-5, 0, 0, -1, 1, -3, -3, 1, -4, -6, 2, -6, -3, 1, -2, 4, 0, 3, -2, 0, -2, 0, 0, 1};
static signed char HF3_A3_HMTctbl16nit[30] = {0, 0, 0, 0, 0, 0,-4, 0, 0, -2, 2, -4, -1, 1, -2, -6, 2, -6, -3, 0, -2, 4, 0, 4, -2, 0, -2, 0, 0, 2};
static signed char HF3_A3_HMTctbl17nit[30] = {0, 0, 0, 0, 0, 0,-3, 0, 0, -3, 1, -4, -2, 1, -4, -6, 2, -5, -2, 1, -2, 3, 0, 3, -2, 0, -2, 1, 0, 3};
static signed char HF3_A3_HMTctbl19nit[30] = {0, 0, 0, 0, 0, 0,-5, 0, -1, -4, 1, -4, -2, 1, -2, -5, 2, -5, -3, 0, -2, 5, 0, 4, -3, 0, -2, 1, 0, 2};
static signed char HF3_A3_HMTctbl20nit[30] = {0, 0, 0, 0, 0, 0,-7, 0, -2, -1, 1, -3, -3, 1, -3, -5, 2, -5, -2, 0, -2, 3, 0, 3, -2, 0, -2, 1, 0, 3};
static signed char HF3_A3_HMTctbl21nit[30] = {0, 0, 0, 0, 0, 0,-8, 1, -2, -2, 1, -3, -2, 1, -4, -7, 2, -6, -1, 0, -1, 3, 0, 3, -2, 0, -2, 0, 0, 2};
static signed char HF3_A3_HMTctbl22nit[30] = {0, 0, 0, 0, 0, 0,-4, 0, -1, -2, 1, -4, -1, 0, -2, -7, 2, -5, -2, 0, -2, 4, 0, 3, -1, 0, -1, 2, 0, 4};
static signed char HF3_A3_HMTctbl23nit[30] = {0, 0, 0, 0, 0, 0,-8, 0, -2, -1, 1, -4, -3, 1, -3, -6, 2, -5, -3, 0, -2, 3, 0, 2, -1, 0, -2, 2, 0, 4};
static signed char HF3_A3_HMTctbl25nit[30] = {0, 0, 0, 0, 0, 0,-8, 0, -2, -1, 1, -4, -2, 1, -2, -5, 1, -4, 0, 0, 0, 0, 0, 0, -1, 0, -2, 2, -1, 4};
static signed char HF3_A3_HMTctbl27nit[30] = {0, 0, 0, 0, 0, 0,-7, 1, -3, 0, 1, -2, -3, 1, -3, -5, 1, -4, 0, 0, 0, -2, 0, -2, -1, 0, 0, 3, 0, 4};
static signed char HF3_A3_HMTctbl29nit[30] = {0, 0, 0, 0, 0, 0,-6, 1, -2, 0, 1, -3, -3, 1, -2, -5, 1, -4, 0, 0, 1, -2, 0, -2, 0, 0, 0, 2, 0, 4};
static signed char HF3_A3_HMTctbl31nit[30] = {0, 0, 0, 0, 0, 0,-3, 1, -2, -1, 1, -3, -4, 1, -3, -3, 1, -3, 0, 0, 0, -2, 0, -1, -1, 0, -2, 2, -1, 4};
static signed char HF3_A3_HMTctbl33nit[30] = {0, 0, 0, 0, 0, 0,-6, 1, -4, -2, 1, -3, -4, 1, -3, -4, 1, -3, 0, 0, 0, -2, 0, -2, -1, 0, -1, 2, -1, 4};
static signed char HF3_A3_HMTctbl35nit[30] = {0, 0, 0, 0, 0, 0,-4, 1, -3, -1, 1, -3, -6, 1, -4, -2, 1, -2, 0, 0, 0, -3, 0, -2, 0, 0, 0, 3, -1, 5};
static signed char HF3_A3_HMTctbl37nit[30] = {0, 0, 0, 0, 0, 0,-6, 1, -3, -1, 1, -3, -5, 1, -4, -3, 1, -2, 0, 0, 0, -1, 0, -1, -2, 0, -2, 4, -1, 6};
static signed char HF3_A3_HMTctbl39nit[30] = {0, 0, 0, 0, 0, 0,-6, 1, -3, -1, 1, -3, -5, 1, -4, -3, 1, -2, 0, 0, 0, -2, 0, -2, -1, 0, -1, 4, -1, 6};
static signed char HF3_A3_HMTctbl41nit[30] = {0, 0, 0, 0, 0, 0,-6, 2, -5, -2, 1, -4, -5, 1, -4, -3, 1, -3, 2, 0, 2, -2, 0, -2, -1, 0, -2, 4, -1, 5};
static signed char HF3_A3_HMTctbl44nit[30] = {0, 0, 0, 0, 0, 0,-7, 2, -5, -2, 1, -3, -6, 1, -4, -2, 1, -3, 1, 0, 2, -2, 0, -2, -1, 0, -2, 5, -1, 5};
static signed char HF3_A3_HMTctbl47nit[30] = {0, 0, 0, 0, 0, 0,-7, 2, -5, -1, 1, -2, -5, 1, -4, -2, 1, -3, 1, 0, 2, -1, 0, -1, -2, 0, -2, 5, -1, 5};
static signed char HF3_A3_HMTctbl50nit[30] = {0, 0, 0, 0, 0, 0,-5, 2, -5, -2, 1, -3, -6, 1, -4, -1, 1, -2, 2, 0, 2, -3, 0, -3, -1, 0, -1, 6, -1, 6};
static signed char HF3_A3_HMTctbl53nit[30] = {0, 0, 0, 0, 0, 0,-6, 2, -6, -1, 1, -2, -6, 1, -4, -1, 1, -2, 1, 0, 2, -3, 0, -3, 0, 0, -1, 6, -1, 6};
static signed char HF3_A3_HMTctbl56nit[30] = {0, 0, 0, 0, 0, 0,-7, 2, -6, -2, 1, -3, -5, 1, -4, -1, 1, -2, 1, 0, 1, -3, 0, -2, 0, 0, 0, 6, -1, 6};
static signed char HF3_A3_HMTctbl60nit[30] = {0, 0, 0, 0, 0, 0,-6, 2, -5, -2, 1, -4, -4, 1, -3, -2, 1, -2, 0, 0, 1, -1, 0, -2, -2, 0, -1, 6, -1, 6};
static signed char HF3_A3_HMTctbl64nit[30] = {0, 0, 0, 0, 0, 0,-5, 2, -5, -3, 1, -3, -5, 1, -4, -2, 0, -1, 1, 0, 1, -2, 0, -1, -1, 0, -1, 7, -1, 7};
static signed char HF3_A3_HMTctbl68nit[30] = {0, 0, 0, 0, 0, 0,-5, 2, -5, -2, 1, -4, -5, 1, -3, -1, 0, -2, 0, 0, 1, -2, 0, -1, -2, 0, -2, 8, -1, 8};
static signed char HF3_A3_HMTctbl72nit[30] = {0, 0, 0, 0, 0, 0,-5, 2, -5, -3, 1, -4, -4, 1, -3, -3, 0, -2, 2, 0, 2, -4, 0, -3, -1, 0, -1, 9, -1, 8};
static signed char HF3_A3_HMTctbl77nit[30] = {0, 0, 0, 0, 0, 0,-1, 0, -1, 0, 0, -2, -3, 1, -3, -2, 0, 0, 0, 0, 0, -1, 0, -1, 0, 0, 0, 5, -1, 7};
static signed char HF3_A3_HMTctbl82nit[30] = {0, 0, 0, 0, 0, 0,-1, 0, -1, 0, 0, -2, -4, 1, -3, -1, 0, 0, 0, 0, 0, -2, 0, -2, 0, 0, 0, 6, -1, 8};
static signed char HF3_A3_HMTctbl87nit[30] = {0, 0, 0, 0, 0, 0,-1, 1, -2, 0, 0, -2, -2, 1, -2, 0, 0, 1, -2, 1, -2, -2, 0, -1, 0, 0, -1, 6, -1, 7};
static signed char HF3_A3_HMTctbl93nit[30] = {0, 0, 0, 0, 0, 0,-1, 1, -2, 0, 1, -2, -2, 0, -2, 0, 0, 1, -2, 1, -2, -2, 0, -1, 0, 0, 0, 6, -1, 8};
static signed char HF3_A3_HMTctbl99nit[30] = {0, 0, 0, 0, 0, 0,-2, 1, -3, -1, 0, -2, 0, 0, -2, 0, 0, 0, -3, 1, -2, -1, 0, -2, 0, 0, 0, 7, -1, 8};
static signed char HF3_A3_HMTctbl105nit[30] = {0, 0, 0, 0, 0, 0,-3, 1, -3, 0, 0, -2, -1, 0, -1, 0, 0, 1, -3, 1, -2, -1, 0, -2, 0, 0, 0, 7, -1, 8};

#endif


#endif

