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
static unsigned char m_gray_005_ea[] = {0, 17, 25, 30, 34, 41, 61, 96, 126, 154};
static unsigned char m_gray_006_ea[] = {0, 17, 24, 28, 32, 39, 60, 95, 126, 154};
static unsigned char m_gray_007_ea[] = {0, 22, 22, 27, 31, 38, 59, 95, 126, 154};
static unsigned char m_gray_008_ea[] = {0, 21, 22, 26, 30, 38, 58, 95, 126, 154};
static unsigned char m_gray_009_ea[] = {0, 20, 21, 24, 29, 37, 58, 95, 125, 154};
static unsigned char m_gray_010_ea[] = {0, 19, 20, 23, 29, 37, 58, 94, 125, 154};
static unsigned char m_gray_011_ea[] = {0, 16, 20, 23, 28, 36, 57, 94, 125, 154};
static unsigned char m_gray_012_ea[] = {0, 9, 14, 18, 23, 32, 54, 92, 125, 154};
static unsigned char m_gray_013_ea[] = {0, 15, 13, 18, 23, 32, 54, 92, 124, 154};
static unsigned char m_gray_014_ea[] = {0, 10, 13, 18, 23, 32, 54, 92, 125, 154};
static unsigned char m_gray_015_ea[] = {0, 11, 12, 18, 23, 32, 54, 92, 125, 154};
static unsigned char m_gray_016_ea[] = {0, 11, 12, 17, 23, 31, 54, 92, 124, 154};
static unsigned char m_gray_017_ea[] = {0, 8, 12, 17, 22, 31, 54, 92, 124, 154};
static unsigned char m_gray_019_ea[] = {0, 8, 12, 17, 22, 31, 54, 92, 124, 154};
static unsigned char m_gray_020_ea[] = {0, 14, 16, 20, 26, 34, 57, 94, 125, 154};
static unsigned char m_gray_021_ea[] = {0, 13, 16, 20, 26, 35, 57, 94, 126, 154};
static unsigned char m_gray_022_ea[] = {0, 14, 16, 20, 26, 35, 57, 95, 125, 154};
static unsigned char m_gray_024_ea[] = {0, 14, 16, 20, 26, 35, 57, 95, 126, 154};
static unsigned char m_gray_025_ea[] = {0, 13, 17, 21, 26, 35, 57, 95, 126, 154};
static unsigned char m_gray_027_ea[] = {0, 16, 16, 21, 26, 35, 58, 95, 126, 154};
static unsigned char m_gray_029_ea[] = {0, 12, 16, 20, 26, 35, 57, 95, 126, 154};
static unsigned char m_gray_030_ea[] = {0, 13, 16, 20, 26, 35, 58, 95, 126, 154};
static unsigned char m_gray_032_ea[] = {0, 10, 16, 20, 26, 34, 57, 95, 126, 154};
static unsigned char m_gray_034_ea[] = {0, 12, 15, 20, 26, 34, 57, 95, 126, 154};
static unsigned char m_gray_037_ea[] = {0, 12, 15, 19, 25, 34, 58, 94, 125, 154};
static unsigned char m_gray_039_ea[] = {0, 12, 14, 19, 25, 34, 57, 94, 125, 154};
static unsigned char m_gray_041_ea[] = {0, 11, 14, 19, 25, 34, 57, 95, 125, 154};
static unsigned char m_gray_044_ea[] = {0, 10, 14, 19, 25, 34, 57, 94, 125, 154};
static unsigned char m_gray_047_ea[] = {0, 10, 13, 18, 25, 34, 57, 95, 125, 154};
static unsigned char m_gray_050_ea[] = {0, 7, 13, 18, 24, 34, 57, 95, 125, 154};
static unsigned char m_gray_053_ea[] = {0, 8, 13, 18, 24, 34, 57, 94, 125, 154};
static unsigned char m_gray_056_ea[] = {0, 10, 12, 18, 24, 34, 57, 94, 125, 154};
static unsigned char m_gray_060_ea[] = {0, 8, 12, 18, 24, 33, 57, 94, 125, 154};
static unsigned char m_gray_064_ea[] = {0, 9, 11, 18, 24, 33, 57, 94, 125, 154};
static unsigned char m_gray_068_ea[] = {0, 8, 11, 17, 23, 33, 57, 94, 125, 154};
static unsigned char m_gray_072_ea[] = {0, 8, 11, 17, 24, 34, 58, 96, 128, 158};
static unsigned char m_gray_077_ea[] = {0, 9, 11, 18, 25, 35, 60, 99, 132, 163};
static unsigned char m_gray_082_ea[] = {0, 6, 12, 18, 25, 36, 62, 102, 135, 167};
static unsigned char m_gray_087_ea[] = {0, 6, 12, 19, 26, 37, 63, 105, 139, 172};
static unsigned char m_gray_093_ea[] = {0, 7, 12, 19, 27, 38, 65, 108, 143, 177};
static unsigned char m_gray_098_ea[] = {0, 7, 12, 19, 27, 39, 66, 110, 146, 180};
static unsigned char m_gray_105_ea[] = {0, 7, 12, 20, 28, 40, 69, 114, 150, 186};
static unsigned char m_gray_111_ea[] = {0, 8, 12, 20, 29, 41, 70, 117, 154, 191};
static unsigned char m_gray_119_ea[] = {0, 7, 12, 20, 29, 41, 70, 117, 154, 191};
static unsigned char m_gray_126_ea[] = {0, 6, 12, 20, 29, 41, 70, 117, 154, 191};
static unsigned char m_gray_134_ea[] = {0, 5, 12, 20, 29, 41, 69, 116, 154, 191};
static unsigned char m_gray_143_ea[] = {0, 6, 11, 19, 28, 40, 69, 116, 153, 191};
static unsigned char m_gray_152_ea[] = {0, 5, 11, 19, 28, 40, 69, 116, 153, 191};
static unsigned char m_gray_162_ea[] = {0, 3, 11, 19, 28, 40, 69, 116, 153, 191};
static unsigned char m_gray_172_ea[] = {0, 7, 10, 19, 28, 40, 69, 116, 153, 191};
static unsigned char m_gray_183_ea[] = {0, 4, 11, 20, 29, 42, 71, 118, 156, 194};
static unsigned char m_gray_195_ea[] = {0, 3, 11, 20, 29, 42, 72, 121, 159, 199};
static unsigned char m_gray_207_ea[] = {0, 3, 11, 20, 30, 43, 73, 124, 163, 204};
static unsigned char m_gray_220_ea[] = {0, 4, 11, 21, 31, 44, 75, 127, 168, 209};
static unsigned char m_gray_234_ea[] = {0, 4, 11, 21, 32, 45, 78, 131, 173, 216};
static unsigned char m_gray_249_ea[] = {0, 4, 11, 21, 32, 46, 78, 133, 177, 221};
static unsigned char m_gray_265_ea[] = {0, 4, 11, 22, 33, 47, 80, 137, 182, 227};
static unsigned char m_gray_282_ea[] = {0, 4, 11, 22, 34, 49, 82, 140, 187, 233};
static unsigned char m_gray_300_ea[] = {0, 3, 12, 23, 34, 50, 85, 144, 191, 239};
static unsigned char m_gray_316_ea[] = {0, 4, 11, 23, 35, 50, 85, 146, 195, 244};
static unsigned char m_gray_333_ea[] = {0, 4, 11, 23, 35, 51, 87, 149, 199, 250};
static unsigned char m_gray_350_ea[] = {0, 3, 11, 23, 35, 51, 87, 151, 203, 255};

static const signed char ctbl5nit_ea[30] = {0, 0, 0, 0, 0, 0, -3, 2, 3, -5, 1, -3, -3, 0, 0, -11, 2, -7, -4, 0, -2, -1, 0, 0, -1, 0, -1, 1, 0, -1};
static const signed char ctbl6nit_ea[30] = {0, 0, 0, 0, 0, 0, -6, 2, -3, -9, 1, -4, -3, 1, -1, -8, 1, -5, -3, 0, 0, -1, 0, 0, 0, 0, -1, 1, 0, -1};
static const signed char ctbl7nit_ea[30] = {0, 0, 0, 0, 0, 0, -4, 2, -2, -6, 1, -3, -3, 0, 0, -7, 1, -6, -3, 0, 0, -1, 0, 0, 0, 0, -1, 1, 0, -1};
static const signed char ctbl8nit_ea[30] = {0, 0, 0, 0, 0, 0, -9, 3, -7, -2, 1, -1, -5, 1, -6, -8, 0, -1, -1, 0, 0, 0, 0, 0, -1, 0, -1, 1, 0, -1};
static const signed char ctbl9nit_ea[30] = {0, 0, 0, 0, 0, 0, -5, 2, -5, -8, 3, -4, -3, 1, -2, -7, 1, -4, -1, 0, 0, -1, 0, 0, 0, 0, -1, 1, 0, -1};
static const signed char ctbl10nit_ea[30] = {0, 0, 0, 0, 0, 0, -6, 3, -6, -6, 0, -1, -3, 1, -4, -6, 0, -2, -2, 0, -1, 0, 0, 0, 0, 0, -1, 1, 0, -1};
static const signed char ctbl11nit_ea[30] = {0, 0, 0, 0, 0, 0, -6, 2, -6, -6, 0, -2, -4, 1, -4, -4, 0, -2, -2, 0, 0, 0, 0, 0, 0, 0, -1, 1, 0, -1};
static const signed char ctbl12nit_ea[30] = {0, 0, 0, 0, 0, 0, -10, 2, 0, -7, 2, -1, -4, 1, -1, -4, 0, -2, -1, 0, 1, -1, 0, -1, 0, 0, 0, 0, 0, -3};
static const signed char ctbl13nit_ea[30] = {0, 0, 0, 0, 0, 0, -15, 3, 0, -7, 2, -2, -4, 1, -1, -3, 0, -1, -2, 0, 0, -1, 0, -1, 0, 0, 0, 0, 0, -3};
static const signed char ctbl14nit_ea[30] = {0, 0, 0, 0, 0, 0, -11, 1, 2, -9, 2, -3, -3, 1, -2, -4, 0, -1, -1, 0, 1, -1, 0, -1, 0, 0, 0, 0, 0, -3};
static const signed char ctbl15nit_ea[30] = {0, 0, 0, 0, 0, 0, -11, 3, -5, -6, 1, -2, -3, 1, -2, -4, 0, -1, 0, 0, 1, -1, 0, -1, 0, 0, 0, 0, 0, -3};
static const signed char ctbl16nit_ea[30] = {0, 0, 0, 0, 0, 0, -14, 3, -3, -8, 1, -3, -2, 1, -1, -3, 0, -2, 0, 0, 2, -1, 0, -1, 0, 0, 0, 0, 0, -3};
static const signed char ctbl17nit_ea[30] = {0, 0, 0, 0, 0, 0, -9, 1, -1, -7, 1, -3, -3, 1, -2, -2, 0, -1, 0, 0, 1, -1, 0, -1, 0, 0, 0, 0, 0, -3};
static const signed char ctbl19nit_ea[30] = {0, 0, 0, 0, 0, 0, -10, 2, -2, -9, 1, -5, -2, 0, -2, -3, 0, -2, 1, 0, 2, -1, 0, -1, 0, 0, 0, 0, 0, -3};
static const signed char ctbl20nit_ea[30] = {0, 0, 0, 0, 0, 0, -2, 1, -5, -4, 1, -2, -1, 1, -2, -4, 0, -3, 0, 0, 1, 0, 0, -1, 0, 0, 0, 1, 0, -1};
static const signed char ctbl21nit_ea[30] = {0, 0, 0, 0, 0, 0, -1, 2, -4, -4, 1, -3, -2, 1, -5, 0, 0, 3, -1, 0, 0, 0, 0, 0, 0, 0, -1, 1, 0, 0};
static const signed char ctbl22nit_ea[30] = {0, 0, 0, 0, 0, 0, -3, 3, -8, -5, 1, -4, -3, 0, -6, 1, 0, 3, 0, 0, 1, -1, 0, 0, 1, 0, 0, 1, 0, 0};
static const signed char ctbl24nit_ea[30] = {0, 0, 0, 0, 0, 0, -3, 2, -7, -3, 1, -3, -2, 1, -4, -1, 0, 0, 0, 0, 1, 1, 0, 1, -1, 0, -1, 2, 0, 1};
static const signed char ctbl25nit_ea[30] = {0, 0, 0, 0, 0, 0, -3, 2, -7, -4, 1, -4, -2, 0, -4, -2, 0, -1, 0, 0, 1, 1, 0, 1, -1, 0, -1, 2, 0, 1};
static const signed char ctbl27nit_ea[30] = {0, 0, 0, 0, 0, 0, -6, 2, -8, -3, 1, -3, 0, 1, -3, -3, 0, -2, 0, 0, 1, 0, 0, 0, 1, 0, 0, 1, 0, 1};
static const signed char ctbl29nit_ea[30] = {0, 0, 0, 0, 0, 0, -3, 3, -6, -3, 1, -4, -2, 1, -5, -1, 0, 1, 1, 0, 2, 1, 0, 1, 0, 0, -1, 1, 0, 1};
static const signed char ctbl30nit_ea[30] = {0, 0, 0, 0, 0, 0, -4, 2, -9, -2, 1, -3, -3, 0, -5, -2, 0, -1, 0, 0, 1, 0, 0, 0, 1, 0, 0, 1, 0, 1};
static const signed char ctbl32nit_ea[30] = {0, 0, 0, 0, 0, 0, -3, 2, -7, -2, 0, -3, -3, 0, -4, -2, 0, -3, 0, 0, 1, -1, 0, 0, 1, 0, 0, 1, 0, 1};
static const signed char ctbl34nit_ea[30] = {0, 0, 0, 0, 0, 0, -5, 2, -8, -2, 1, -4, -2, 0, -4, -1, 0, -2, 0, 0, 1, -1, 0, 0, 1, 0, 0, 1, 0, 1};
static const signed char ctbl37nit_ea[30] = {0, 0, 0, 0, 0, 0, -8, 1, -8, -4, 1, -5, -2, 0, -5, 0, 0, 1, -1, 0, 0, 0, 0, 0, 1, 0, 0, 1, 0, 1};
static const signed char ctbl39nit_ea[30] = {0, 0, 0, 0, 0, 0, -7, 2, -9, -2, 1, -4, -3, 0, -5, -2, 0, -1, 0, 0, 1, 1, 0, 0, 0, 0, 0, 1, 0, 1};
static const signed char ctbl41nit_ea[30] = {0, 0, 0, 0, 0, 0, -7, 2, -10, -1, 1, -4, -3, 0, -5, -2, 0, -1, 1, 0, 2, 0, 0, 0, 0, 0, 0, 1, 0, 1};
static const signed char ctbl44nit_ea[30] = {0, 0, 0, 0, 0, 0, -3, 3, -6, -4, 1, -5, -2, 0, -5, -2, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 1, 0, 1};
static const signed char ctbl47nit_ea[30] = {0, 0, 0, 0, 0, 0, -6, 1, -6, -3, 1, -4, -2, 0, -5, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, -1, 1, 0, 2};
static const signed char ctbl50nit_ea[30] = {0, 0, 0, 0, 0, 0, -5, 2, -6, 1, 1, -3, -2, 0, -5, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, -1, 1, 0, 2};
static const signed char ctbl53nit_ea[30] = {0, 0, 0, 0, 0, 0, -5, 1, -5, -4, 1, -6, -1, 0, -5, 0, 0, 2, 0, 0, 1, 0, 0, 0, 0, 0, -1, 1, 0, 2};
static const signed char ctbl56nit_ea[30] = {0, 0, 0, 0, 0, 0, -6, 2, -6, -1, 1, -4, -1, 1, -5, 1, 0, 3, 0, 0, 1, 0, 0, 0, 0, 0, -1, 1, 0, 2};
static const signed char ctbl60nit_ea[30] = {0, 0, 0, 0, 0, 0, -5, 3, -7, -1, 1, -4, 0, 0, -3, -2, 0, -1, 1, 0, 2, 0, 0, 0, 0, 0, -1, 1, 0, 2};
static const signed char ctbl64nit_ea[30] = {0, 0, 0, 0, 0, 0, -6, 3, -8, -2, 0, -5, 0, 0, -3, -2, 0, -1, 1, 0, 2, 1, 0, 0, -1, 0, -1, 1, 0, 2};
static const signed char ctbl68nit_ea[30] = {0, 0, 0, 0, 0, 0, -6, 1, -8, -1, 1, -5, 0, 0, -4, -1, 0, 0, 0, 0, 2, 1, 0, 0, -1, 0, -1, 1, 0, 2};
static const signed char ctbl72nit_ea[30] = {0, 0, 0, 0, 0, 0, -3, 0, -4, -3, 0, -6, 1, 0, -4, 0, 0, 3, 0, 0, 1, 1, 0, 1, -1, 0, 0, 1, 0, 1};
static const signed char ctbl77nit_ea[30] = {0, 0, 0, 0, 0, 0, -4, 2, -6, -3, 0, -5, 1, 0, -3, -1, 0, 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1};
static const signed char ctbl82nit_ea[30] = {0, 0, 0, 0, 0, 0, -2, 1, -3, -3, 0, -5, 0, 0, -4, -1, 0, 1, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 0, 1};
static const signed char ctbl87nit_ea[30] = {0, 0, 0, 0, 0, 0, -3, 2, -4, -1, 0, -4, 1, 0, -3, -1, 0, 0, 0, 0, 2, -1, 0, 0, 1, 0, 1, 0, 0, 1};
static const signed char ctbl93nit_ea[30] = {0, 0, 0, 0, 0, 0, -3, 0, -3, 0, 0, -3, -1, 0, -5, 0, 0, 3, 1, 0, 1, 0, 0, 0, -1, 0, 0, 0, 0, 1};
static const signed char ctbl98nit_ea[30] = {0, 0, 0, 0, 0, 0, -3, 1, -3, 0, 0, -3, 0, 0, -3, 0, 0, 2, -1, 0, 1, 1, 0, 1, -1, 0, -1, 1, 0, 1};
static const signed char ctbl105nit_ea[30] = {0, 0, 0, 0, 0, 0, -3, 1, -2, -2, 0, -4, -1, 0, -3, 1, 0, 1, 0, 0, 1, 1, 0, 0, -1, 0, -1, 1, 0, 1};
static const signed char ctbl111nit_ea[30] = {0, 0, 0, 0, 0, 0, -5, 1, -6, 0, 0, -3, -1, 0, -4, -1, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1};
static const signed char ctbl119nit_ea[30] = {0, 0, 0, 0, 0, 0, -4, 0, -2, -1, 0, -4, 0, 0, -3, 0, 0, 1, -1, 0, 0, 1, 0, 1, -1, 0, 0, 0, 0, 1};
static const signed char ctbl126nit_ea[30] = {0, 0, 0, 0, 0, 0, -2, 0, 0, 0, 0, -3, -1, 0, -4, 0, 0, 2, 0, 0, 0, 0, 0, 1, 0, 0, 0, -1, 0, 1};
static const signed char ctbl134nit_ea[30] = {0, 0, 0, 0, 0, 0, -3, 0, -2, -1, 0, -3, -1, 0, -4, -1, -1, 2, 1, 0, 1, 0, 0, 1, 0, 0, 0, -1, 0, 1};
static const signed char ctbl143nit_ea[30] = {0, 0, 0, 0, 0, 0, -5, 1, -3, 1, 0, -2, 0, 0, -3, 0, 0, 2, 1, 0, 1, 0, 0, 1, 0, 0, 0, -1, 0, 1};
static const signed char ctbl152nit_ea[30] = {0, 0, 0, 0, 0, 0, -5, 0, -2, 1, 0, -4, 0, 0, -3, 1, 0, 3, 0, 0, 1, 0, 0, 1, 0, 0, 0, -1, 0, 1};
static const signed char ctbl162nit_ea[30] = {0, 0, 0, 0, 0, 0, -4, 0, 0, 0, 0, -4, 0, 0, -3, 1, 0, 2, 0, 0, 1, 0, 0, 1, 0, 0, 0, -1, 0, 1};
static const signed char ctbl172nit_ea[30] = {0, 0, 0, 0, 0, 0, -3, 0, 3, 0, 1, -3, 1, 0, -3, 0, 0, 2, 0, 0, 1, 1, 0, 1, -1, 0, 0, -1, 0, 1};
static const signed char ctbl183nit_ea[30] = {0, 0, 0, 0, 0, 0, -2, 0, 0, 0, 0, -3, 1, 0, -2, 0, 0, 1, 1, 0, 1, 0, 0, 0, 0, 0, -1, 0, 0, 2};
static const signed char ctbl195nit_ea[30] = {0, 0, 0, 0, 0, 0, -2, 0, 1, 0, 0, -4, 2, 0, -1, 1, 0, 2, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2};
static const signed char ctbl207nit_ea[30] = {0, 0, 0, 0, 0, 0, -2, 0, 1, 0, 0, -4, 1, 0, 0, -1, 0, -1, 1, 0, 2, -1, 0, 0, 0, 0, 0, -1, 0, 0};
static const signed char ctbl220nit_ea[30] = {0, 0, 0, 0, 0, 0, -3, 0, 1, 0, 0, -2, 0, 0, -2, 0, 0, 2, 0, 0, 1, 0, 0, -1, 0, 0, 0, 0, 0, 1};
static const signed char ctbl234nit_ea[30] = {0, 0, 0, 0, 0, 0, 0, 0, 3, 1, 0, -2, 0, 0, -1, 0, 0, 0, 0, 0, 1, 0, 0, -1, 0, 0, 0, 0, 0, 1};
static const signed char ctbl249nit_ea[30] = {0, 0, 0, 0, 0, 0, 0, 0, 3, 1, 0, -1, 0, 0, -2, 0, 0, 1, 1, 0, 1, 0, 0, 0, 0, 0, 0, -1, 0, 0};
static const signed char ctbl265nit_ea[30] = {0, 0, 0, 0, 0, 0, -1, 0, 2, 0, 0, 0, 0, 0, -1, 0, 0, 0, 1, 0, 2, 0, 0, 0, -1, 0, -1, 1, 0, 1};
static const signed char ctbl282nit_ea[30] = {0, 0, 0, 0, 0, 0, 0, 0, 3, 0, 0, -1, 0, 0, -1, 1, 0, 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1};
static const signed char ctbl300nit_ea[30] = {0, 0, 0, 0, 0, 0, 0, 0, 1, -1, 0, -1, 1, 0, -1, 0, 0, 1, -1, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static const signed char ctbl316nit_ea[30] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 1, -1, 0, 0, 0, 0, 0, 0, 0, 0};
static const signed char ctbl333nit_ea[30] = {0, 0, 0, 0, 0, 0, 2, 0, 1, 0, 0, 0, 3, 0, 1, 0, 0, 0, 1, 0, 1, -1, 0, 0, 0, 0, 0, 0, 0, 0};
static const signed char ctbl350nit_ea[30] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};


static unsigned char elv_ea_8B[6] = {0xB6, 0xCC, 0x8B, 0x00, 0x00, 0x00};
static unsigned char elv_ea_8C[6] = {0xB6, 0xCC, 0x8C, 0x00, 0x00, 0x00};
static unsigned char elv_ea_8D[6] = {0xB6, 0xCC, 0x8D, 0x00, 0x00, 0x00};
static unsigned char elv_ea_8E[6] = {0xB6, 0xCC, 0x8E, 0x00, 0x00, 0x00};
static unsigned char elv_ea_8F[6] = {0xB6, 0xCC, 0x8F, 0x00, 0x00, 0x00};
static unsigned char elv_ea_90[6] = {0xB6, 0xCC, 0x90, 0x00, 0x00, 0x00};
static unsigned char elv_ea_91[6] = {0xB6, 0xCC, 0x91, 0x00, 0x00, 0x00};
static unsigned char elv_ea_92[6] = {0xB6, 0xCC, 0x92, 0x00, 0x00, 0x00};
static unsigned char elv_ea_93[6] = {0xB6, 0xCC, 0x93, 0x00, 0x00, 0x00};
static unsigned char elv_ea_94[6] = {0xB6, 0xCC, 0x94, 0x00, 0x00, 0x00};
static unsigned char elv_ea_95[6] = {0xB6, 0xCC, 0x95, 0x00, 0x00, 0x00};
static unsigned char elv_ea_96[6] = {0xB6, 0xCC, 0x96, 0x00, 0x00, 0x00};
static unsigned char elv_ea_97[6] = {0xB6, 0xCC, 0x97, 0x00, 0x00, 0x00};
static unsigned char elv_ea_98[6] = {0xB6, 0xCC, 0x98, 0x00, 0x00, 0x00};
static unsigned char elv_ea_99[6] = {0xB6, 0xCC, 0x99, 0x00, 0x00, 0x00};
static unsigned char elv_ea_9A[6] = {0xB6, 0xCC, 0x9A, 0x00, 0x00, 0x00};

static const signed char aid5_ea[5] = {0xB2, 0x00, 0x00, 0x03, 0x1A};
static const signed char aid6_ea[5] = {0xB2, 0x00, 0x00, 0x03, 0x13};
static const signed char aid7_ea[5] = {0xB2, 0x00, 0x00, 0x03, 0x0C};
static const signed char aid8_ea[5] = {0xB2, 0x00, 0x00, 0x03, 0x07};
static const signed char aid9_ea[5] = {0xB2, 0x00, 0x00, 0x02, 0xFE};
static const signed char aid10_ea[5] = {0xB2, 0x00, 0x00, 0x02, 0xF9};
static const signed char aid11_ea[5] = {0xB2, 0x00, 0x00, 0x02, 0xF1};
static const signed char aid12_ea[5] = {0xB2, 0x00, 0x00, 0x02, 0xEF};
static const signed char aid13_ea[5] = {0xB2, 0x00, 0x00, 0x02, 0xE8};
static const signed char aid14_ea[5] = {0xB2, 0x00, 0x00, 0x02, 0xE3};
static const signed char aid15_ea[5] = {0xB2, 0x00, 0x00, 0x02, 0xDB};
static const signed char aid16_ea[5] = {0xB2, 0x00, 0x00, 0x02, 0xD4};
static const signed char aid17_ea[5] = {0xB2, 0x00, 0x00, 0x02, 0xCE};
static const signed char aid19_ea[5] = {0xB2, 0x00, 0x00, 0x02, 0xC5};
static const signed char aid20_ea[5] = {0xB2, 0x00, 0x00, 0x02, 0xB2};
static const signed char aid21_ea[5] = {0xB2, 0x00, 0x00, 0x02, 0xAA};
static const signed char aid22_ea[5] = {0xB2, 0x00, 0x00, 0x02, 0xA1};
static const signed char aid24_ea[5] = {0xB2, 0x00, 0x00, 0x02, 0x92};
static const signed char aid25_ea[5] = {0xB2, 0x00, 0x00, 0x02, 0x88};
static const signed char aid27_ea[5] = {0xB2, 0x00, 0x00, 0x02, 0x78};
static const signed char aid29_ea[5] = {0xB2, 0x00, 0x00, 0x02, 0x6A};
static const signed char aid30_ea[5] = {0xB2, 0x00, 0x00, 0x02, 0x66};
static const signed char aid32_ea[5] = {0xB2, 0x00, 0x00, 0x02, 0x54};
static const signed char aid34_ea[5] = {0xB2, 0x00, 0x00, 0x02, 0x45};
static const signed char aid37_ea[5] = {0xB2, 0x00, 0x00, 0x02, 0x2F};
static const signed char aid39_ea[5] = {0xB2, 0x00, 0x00, 0x02, 0x1F};
static const signed char aid41_ea[5] = {0xB2, 0x00, 0x00, 0x02, 0x11};
static const signed char aid44_ea[5] = {0xB2, 0x00, 0x00, 0x02, 0x00};
static const signed char aid47_ea[5] = {0xB2, 0x00, 0x00, 0x01, 0xEA};
static const signed char aid50_ea[5] = {0xB2, 0x00, 0x00, 0x01, 0xD0};
static const signed char aid53_ea[5] = {0xB2, 0x00, 0x00, 0x01, 0xB9};
static const signed char aid56_ea[5] = {0xB2, 0x00, 0x00, 0x01, 0xA9};
static const signed char aid60_ea[5] = {0xB2, 0x00, 0x00, 0x01, 0x85};
static const signed char aid64_ea[5] = {0xB2, 0x00, 0x00, 0x01, 0x67};
static const signed char aid68_ea[5] = {0xB2, 0x00, 0x00, 0x01, 0x46};
static const signed char aid72_ea[5] = {0xB2, 0x00, 0x00, 0x01, 0x46};
static const signed char aid77_ea[5] = {0xB2, 0x00, 0x00, 0x01, 0x46};
static const signed char aid82_ea[5] = {0xB2, 0x00, 0x00, 0x01, 0x46};
static const signed char aid87_ea[5] = {0xB2, 0x00, 0x00, 0x01, 0x46};
static const signed char aid93_ea[5] = {0xB2, 0x00, 0x00, 0x01, 0x46};
static const signed char aid98_ea[5] = {0xB2, 0x00, 0x00, 0x01, 0x46};
static const signed char aid105_ea[5] = {0xB2, 0x00, 0x00, 0x01, 0x46};
static const signed char aid111_ea[5] = {0xB2, 0x00, 0x00, 0x01, 0x46};
static const signed char aid119_ea[5] = {0xB2, 0x00, 0x00, 0x01, 0x20};
static const signed char aid126_ea[5] = {0xB2, 0x00, 0x00, 0x00, 0xFD};
static const signed char aid134_ea[5] = {0xB2, 0x00, 0x00, 0x00, 0xD7};
static const signed char aid143_ea[5] = {0xB2, 0x00, 0x00, 0x00, 0xAD};
static const signed char aid152_ea[5] = {0xB2, 0x00, 0x00, 0x00, 0x80};
static const signed char aid162_ea[5] = {0xB2, 0x00, 0x00, 0x00, 0x52};
static const signed char aid172_ea[5] = {0xB2, 0x00, 0x00, 0x00, 0x21};
static const signed char aid183_ea[5] = {0xB2, 0x00, 0x00, 0x00, 0x0A};
static const signed char aid195_ea[5] = {0xB2, 0x00, 0x00, 0x00, 0x0A};
static const signed char aid207_ea[5] = {0xB2, 0x00, 0x00, 0x00, 0x0A};
static const signed char aid220_ea[5] = {0xB2, 0x00, 0x00, 0x00, 0x0A};
static const signed char aid234_ea[5] = {0xB2, 0x00, 0x00, 0x00, 0x0A};
static const signed char aid249_ea[5] = {0xB2, 0x00, 0x00, 0x00, 0x0A};
static const signed char aid265_ea[5] = {0xB2, 0x00, 0x00, 0x00, 0x0A};
static const signed char aid282_ea[5] = {0xB2, 0x00, 0x00, 0x00, 0x0A};
static const signed char aid300_ea[5] = {0xB2, 0x00, 0x00, 0x00, 0x0A};
static const signed char aid316_ea[5] = {0xB2, 0x00, 0x00, 0x00, 0x0A};
static const signed char aid333_ea[5] = {0xB2, 0x00, 0x00, 0x00, 0x0A};
static const signed char aid350_ea[5] = {0xB2, 0x00, 0x00, 0x00, 0x0A};

#endif

