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

static unsigned char m_gray_005[] = {0, 27, 29, 33, 37, 44, 63, 101, 133, 162};
static unsigned char m_gray_006[] = {0, 25, 27, 30, 35, 42, 61, 100, 131, 162};
static unsigned char m_gray_007[] = {0, 23, 25, 28, 33, 41, 60, 99, 131, 162};
static unsigned char m_gray_008[] = {0, 21, 24, 27, 32, 40, 60, 99, 131, 162};
static unsigned char m_gray_009[] = {0, 20, 22, 26, 31, 39, 59, 98, 131, 162};
static unsigned char m_gray_010[] = {0, 20, 21, 25, 30, 38, 59, 98, 131, 162};
static unsigned char m_gray_011[] = {0, 19, 21, 25, 30, 38, 59, 98, 131, 162};
static unsigned char m_gray_012[] = {0, 18, 20, 24, 29, 38, 58, 98, 131, 162};
static unsigned char m_gray_013[] = {0, 18, 20, 23, 29, 37, 58, 98, 131, 162};
static unsigned char m_gray_014[] = {0, 17, 19, 23, 29, 37, 58, 98, 131, 162};
static unsigned char m_gray_015[] = {0, 16, 19, 22, 28, 37, 58, 98, 131, 162};
static unsigned char m_gray_016[] = {0, 16, 18, 22, 28, 36, 58, 98, 131, 162};
static unsigned char m_gray_017[] = {0, 16, 18, 22, 28, 37, 58, 98, 131, 162};
static unsigned char m_gray_019[] = {0, 15, 17, 21, 27, 36, 58, 98, 131, 162};
static unsigned char m_gray_020[] = {0, 15, 18, 22, 28, 37, 59, 99, 131, 162};
static unsigned char m_gray_021[] = {0, 15, 18, 22, 28, 37, 59, 99, 131, 162};
static unsigned char m_gray_022[] = {0, 15, 18, 22, 29, 38, 59, 99, 131, 162};
static unsigned char m_gray_024[] = {0, 15, 18, 22, 29, 38, 59, 99, 131, 162};
static unsigned char m_gray_025[] = {0, 15, 18, 22, 29, 38, 60, 100, 131, 162};
static unsigned char m_gray_027[] = {0, 15, 18, 22, 29, 38, 60, 100, 132, 162};
static unsigned char m_gray_029[] = {0, 14, 18, 22, 29, 38, 60, 100, 132, 162};
static unsigned char m_gray_030[] = {0, 14, 18, 22, 29, 38, 60, 100, 132, 162};
static unsigned char m_gray_032[] = {0, 13, 17, 22, 28, 38, 60, 100, 132, 162};
static unsigned char m_gray_034[] = {0, 13, 17, 22, 28, 37, 60, 100, 132, 162};
static unsigned char m_gray_037[] = {0, 13, 16, 21, 28, 37, 60, 100, 132, 162};
static unsigned char m_gray_039[] = {0, 13, 16, 21, 28, 37, 60, 100, 132, 162};
static unsigned char m_gray_041[] = {0, 12, 15, 21, 27, 37, 60, 100, 132, 162};
static unsigned char m_gray_044[] = {0, 12, 15, 20, 27, 37, 59, 99, 131, 162};
static unsigned char m_gray_047[] = {0, 11, 14, 20, 27, 37, 59, 99, 131, 162};
static unsigned char m_gray_050[] = {0, 11, 14, 20, 27, 37, 59, 99, 131, 162};
static unsigned char m_gray_053[] = {0, 10, 14, 20, 27, 37, 59, 99, 131, 162};
static unsigned char m_gray_056[] = {0, 10, 13, 19, 26, 37, 59, 99, 131, 162};
static unsigned char m_gray_060[] = {0, 10, 13, 19, 26, 36, 59, 99, 131, 162};
static unsigned char m_gray_064[] = {0, 9, 13, 19, 26, 36, 59, 99, 131, 162};
static unsigned char m_gray_068[] = {0, 9, 12, 19, 26, 36, 59, 99, 131, 162};
static unsigned char m_gray_072[] = {0, 8, 12, 19, 26, 36, 59, 99, 131, 162};
static unsigned char m_gray_077[] = {0, 8, 12, 19, 26, 36, 60, 101, 135, 166};
static unsigned char m_gray_082[] = {0, 8, 12, 19, 27, 38, 62, 105, 139, 171};
static unsigned char m_gray_087[] = {0, 8, 12, 20, 27, 39, 63, 107, 142, 175};
static unsigned char m_gray_093[] = {0, 8, 12, 20, 28, 40, 65, 110, 146, 180};
static unsigned char m_gray_098[] = {0, 8, 12, 20, 29, 40, 66, 112, 148, 183};
static unsigned char m_gray_105[] = {0, 8, 13, 21, 29, 41, 68, 116, 153, 189};
static unsigned char m_gray_111[] = {0, 8, 13, 21, 30, 42, 70, 118, 156, 194};
static unsigned char m_gray_119[] = {0, 8, 12, 21, 30, 42, 69, 118, 156, 194};
static unsigned char m_gray_126[] = {0, 7, 11, 20, 29, 42, 69, 117, 155, 194};
static unsigned char m_gray_134[] = {0, 7, 11, 20, 29, 41, 69, 117, 155, 194};
static unsigned char m_gray_143[] = {0, 6, 11, 20, 29, 41, 69, 117, 155, 194};
static unsigned char m_gray_152[] = {0, 6, 11, 20, 29, 41, 69, 117, 155, 194};
static unsigned char m_gray_162[] = {0, 6, 10, 20, 28, 41, 69, 117, 155, 194};
static unsigned char m_gray_172[] = {0, 5, 10, 20, 28, 41, 69, 117, 155, 194};
static unsigned char m_gray_183[] = {0, 5, 10, 20, 29, 42, 69, 118, 156, 194};
static unsigned char m_gray_195[] = {0, 5, 10, 20, 30, 43, 71, 121, 161, 200};
static unsigned char m_gray_207[] = {0, 5, 10, 20, 30, 43, 72, 124, 164, 205};
static unsigned char m_gray_220[] = {0, 5, 11, 21, 31, 44, 74, 127, 168, 210};
static unsigned char m_gray_234[] = {0, 5, 11, 21, 32, 46, 76, 131, 173, 216};
static unsigned char m_gray_249[] = {0, 4, 11, 22, 33, 47, 78, 134, 178, 222};
static unsigned char m_gray_265[] = {0, 4, 11, 22, 33, 47, 80, 137, 182, 228};
static unsigned char m_gray_282[] = {0, 4, 11, 22, 34, 48, 82, 141, 187, 234};
static unsigned char m_gray_300[] = {0, 4, 11, 23, 34, 49, 83, 144, 192, 240};
static unsigned char m_gray_316[] = {0, 4, 11, 23, 35, 50, 85, 147, 195, 245};
static unsigned char m_gray_333[] = {0, 3, 11, 23, 35, 51, 86, 150, 199, 250};
static unsigned char m_gray_350[] = {0, 3, 11, 23, 35, 51, 87, 151, 203, 255};

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
