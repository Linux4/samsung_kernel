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

static unsigned char m_gray_005_ea[] = {0, 8, 30, 33, 37, 44, 62, 100, 131, 161};
static unsigned char m_gray_006_ea[] = {0, 8, 28, 31, 35, 42, 62, 101, 132, 161};
static unsigned char m_gray_007_ea[] = {0, 8, 26, 29, 34, 41, 61, 100, 131, 161};
static unsigned char m_gray_008_ea[] = {0, 8, 24, 28, 32, 40, 61, 100, 131, 161};
static unsigned char m_gray_009_ea[] = {0, 8, 23, 27, 32, 39, 60, 100, 131, 161};
static unsigned char m_gray_010_ea[] = {0, 8, 22, 26, 31, 38, 59, 100, 131, 161};
static unsigned char m_gray_011_ea[] = {0, 8, 21, 25, 30, 38, 59, 100, 131, 161};
static unsigned char m_gray_012_ea[] = {0, 8, 20, 24, 30, 38, 59, 99, 131, 161};
static unsigned char m_gray_013_ea[] = {0, 8, 20, 23, 29, 37, 59, 99, 131, 161};
static unsigned char m_gray_014_ea[] = {0, 8, 19, 23, 29, 37, 59, 99, 131, 161};
static unsigned char m_gray_015_ea[] = {0, 8, 19, 23, 29, 38, 59, 99, 131, 161};
static unsigned char m_gray_016_ea[] = {0, 7, 18, 22, 28, 36, 59, 99, 131, 161};
static unsigned char m_gray_017_ea[] = {0, 7, 18, 22, 27, 37, 58, 99, 131, 161};
static unsigned char m_gray_019_ea[] = {0, 7, 17, 21, 27, 36, 58, 99, 131, 161};
static unsigned char m_gray_020_ea[] = {0, 7, 17, 21, 27, 36, 58, 99, 131, 161};
static unsigned char m_gray_021_ea[] = {0, 7, 17, 21, 28, 36, 58, 99, 131, 161};
static unsigned char m_gray_022_ea[] = {0, 7, 18, 22, 28, 37, 59, 100, 131, 161};
static unsigned char m_gray_024_ea[] = {0, 7, 18, 22, 28, 37, 59, 100, 131, 161};
static unsigned char m_gray_025_ea[] = {0, 7, 18, 22, 28, 37, 60, 100, 131, 161};
static unsigned char m_gray_027_ea[] = {0, 7, 18, 22, 28, 37, 60, 100, 131, 161};
static unsigned char m_gray_029_ea[] = {0, 7, 18, 22, 28, 37, 60, 100, 131, 161};
static unsigned char m_gray_030_ea[] = {0, 7, 18, 22, 28, 37, 60, 100, 131, 161};
static unsigned char m_gray_032_ea[] = {0, 7, 17, 22, 28, 37, 60, 100, 131, 161};
static unsigned char m_gray_034_ea[] = {0, 7, 17, 21, 28, 37, 59, 100, 131, 161};
static unsigned char m_gray_037_ea[] = {0, 6, 16, 21, 27, 37, 59, 100, 131, 161};
static unsigned char m_gray_039_ea[] = {0, 6, 16, 21, 27, 37, 59, 100, 131, 161};
static unsigned char m_gray_041_ea[] = {0, 6, 15, 20, 27, 37, 59, 100, 131, 161};
static unsigned char m_gray_044_ea[] = {0, 6, 15, 20, 27, 36, 59, 100, 131, 161};
static unsigned char m_gray_047_ea[] = {0, 6, 15, 20, 26, 36, 59, 100, 131, 161};
static unsigned char m_gray_050_ea[] = {0, 6, 14, 20, 26, 36, 59, 100, 131, 161};
static unsigned char m_gray_053_ea[] = {0, 6, 14, 19, 26, 36, 59, 100, 131, 161};
static unsigned char m_gray_056_ea[] = {0, 6, 13, 19, 26, 36, 59, 100, 131, 161};
static unsigned char m_gray_060_ea[] = {0, 6, 13, 19, 26, 36, 59, 100, 131, 161};
static unsigned char m_gray_064_ea[] = {0, 6, 13, 19, 25, 36, 59, 100, 131, 161};
static unsigned char m_gray_068_ea[] = {0, 6, 12, 19, 25, 36, 59, 99, 131, 161};
static unsigned char m_gray_072_ea[] = {0, 5, 12, 18, 25, 35, 59, 100, 131, 161};
static unsigned char m_gray_077_ea[] = {0, 5, 12, 18, 26, 36, 61, 103, 135, 166};
static unsigned char m_gray_082_ea[] = {0, 5, 12, 19, 27, 37, 63, 106, 139, 171};
static unsigned char m_gray_087_ea[] = {0, 5, 12, 19, 27, 38, 64, 109, 141, 175};
static unsigned char m_gray_093_ea[] = {0, 5, 13, 20, 28, 40, 67, 112, 145, 180};
static unsigned char m_gray_098_ea[] = {0, 5, 13, 20, 29, 40, 68, 114, 148, 184};
static unsigned char m_gray_105_ea[] = {0, 5, 13, 21, 30, 42, 71, 118, 153, 190};
static unsigned char m_gray_111_ea[] = {0, 5, 13, 21, 30, 43, 72, 120, 156, 194};
static unsigned char m_gray_119_ea[] = {0, 5, 12, 21, 30, 43, 72, 119, 156, 194};
static unsigned char m_gray_126_ea[] = {0, 5, 12, 21, 30, 42, 71, 119, 156, 194};
static unsigned char m_gray_134_ea[] = {0, 5, 12, 20, 29, 42, 71, 119, 156, 194};
static unsigned char m_gray_143_ea[] = {0, 5, 12, 20, 29, 42, 71, 119, 156, 194};
static unsigned char m_gray_152_ea[] = {0, 5, 11, 20, 29, 42, 71, 119, 156, 194};
static unsigned char m_gray_162_ea[] = {0, 5, 11, 20, 29, 42, 71, 119, 156, 194};
static unsigned char m_gray_172_ea[] = {0, 5, 11, 20, 29, 41, 70, 118, 156, 194};
static unsigned char m_gray_183_ea[] = {0, 4, 11, 19, 29, 41, 70, 119, 156, 194};
static unsigned char m_gray_195_ea[] = {0, 5, 11, 20, 30, 43, 72, 122, 160, 200};
static unsigned char m_gray_207_ea[] = {0, 5, 11, 20, 30, 43, 73, 125, 164, 205};
static unsigned char m_gray_220_ea[] = {0, 4, 11, 20, 31, 44, 75, 128, 168, 210};
static unsigned char m_gray_234_ea[] = {0, 4, 11, 21, 32, 45, 77, 131, 173, 216};
static unsigned char m_gray_249_ea[] = {0, 4, 11, 21, 32, 46, 78, 134, 177, 221};
static unsigned char m_gray_265_ea[] = {0, 4, 11, 22, 33, 47, 79, 137, 182, 227};
static unsigned char m_gray_282_ea[] = {0, 4, 11, 22, 33, 48, 82, 141, 187, 233};
static unsigned char m_gray_300_ea[] = {0, 3, 11, 23, 34, 49, 84, 144, 191, 239};
static unsigned char m_gray_316_ea[] = {0, 3, 11, 23, 35, 50, 85, 147, 196, 245};
static unsigned char m_gray_333_ea[] = {0, 3, 11, 23, 35, 51, 86, 149, 199, 250};
static unsigned char m_gray_350_ea[] = {0, 3, 11, 23, 35, 51, 87, 151, 203, 255};

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
