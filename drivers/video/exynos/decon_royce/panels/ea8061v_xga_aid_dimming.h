/* linux/drivers/video/exynos_decon/panel/EA8061V_XGA_aid_dimming.h
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

#ifndef __EA8061V_XGA_AID_DIMMING_H__
#define __EA8061V_XGA_AID_DIMMING_H__

/* gradation_offset */
/* V3 ~ V255 */
static signed char EA_rtbl_5nit[10] =  { 0, 24, 28, 27, 25, 23, 18, 12, 7, 0};
static signed char EA_rtbl_6nit[10] =  { 0, 23, 25, 24, 22, 20, 16, 11, 6, 0};
static signed char EA_rtbl_7nit[10] =  { 0, 20, 24, 23, 21, 19, 15, 10, 5, 0};
static signed char EA_rtbl_8nit[10] =  { 0, 18, 22, 20, 18, 17, 13, 9, 5, 0};
static signed char EA_rtbl_9nit[10] =  { 0, 16, 20, 18, 16, 15, 12, 8, 5, 0};
static signed char EA_rtbl_10nit[10] = { 0, 13, 20, 18, 16, 14, 11, 8, 5, 0};
static signed char EA_rtbl_11nit[10] = { 0, 15, 18, 17, 14, 13, 11, 7, 4, 0};
static signed char EA_rtbl_12nit[10] = { 0, 11, 17, 15, 12, 12, 10, 7, 4, 0};
static signed char EA_rtbl_13nit[10] = { 0, 11, 16, 14, 12, 11, 9, 7, 4, 0};
static signed char EA_rtbl_14nit[10] = { 0, 12, 15, 13, 11, 11, 9, 6, 4, 0};
static signed char EA_rtbl_15nit[10] = { 0, 9, 15, 13, 11, 10, 8, 6, 4, 0};
static signed char EA_rtbl_16nit[10] = { 0, 11, 14, 12, 10, 9, 8, 6, 4, 0};
static signed char EA_rtbl_17nit[10] = { 0, 8, 14, 12, 10, 9, 7, 6, 4, 0};
static signed char EA_rtbl_19nit[10] = { 0, 8, 13, 11, 9, 8, 7, 5, 4, 0};
static signed char EA_rtbl_20nit[10] = { 0, 9, 12, 10, 8, 8, 6, 5, 4, 0};
static signed char EA_rtbl_21nit[10] = { 0, 7, 12, 10, 8, 7, 6, 5, 3, 0};
static signed char EA_rtbl_22nit[10] = { 0, 6, 12, 10, 8, 7, 6, 5, 3, 0};
static signed char EA_rtbl_24nit[10] = { 0, 7, 11, 9, 7, 6, 5, 5, 3, 0};
static signed char EA_rtbl_25nit[10] = { 0, 6, 11, 9, 7, 6, 6, 4, 3, 0};
static signed char EA_rtbl_27nit[10] = { 0, 7, 10, 8, 6, 6, 5, 5, 3, 0};
static signed char EA_rtbl_29nit[10] = { 0, 5, 10, 8, 6, 5, 5, 4, 3, 0};
static signed char EA_rtbl_30nit[10] = { 0, 5, 10, 8, 6, 5, 5, 4, 3, 0};
static signed char EA_rtbl_32nit[10] = { 0, 7, 9, 8, 6, 5, 4, 4, 3, 0};
static signed char EA_rtbl_34nit[10] = { 0, 5, 9, 7, 5, 5, 4, 4, 3, 0};
static signed char EA_rtbl_37nit[10] = { 0, 6, 8, 7, 5, 5, 4, 4, 3, 0};
static signed char EA_rtbl_39nit[10] = { 0, 7, 7, 6, 4, 4, 3, 4, 3, 0};
static signed char EA_rtbl_41nit[10] = { 0, 6, 7, 6, 4, 4, 3, 4, 3, 0};
static signed char EA_rtbl_44nit[10] = { 0, 4, 7, 6, 4, 3, 3, 3, 3, 0};
static signed char EA_rtbl_47nit[10] = { 0, 6, 6, 5, 3, 3, 3, 4, 3, 0};
static signed char EA_rtbl_50nit[10] = { 0, 3, 6, 5, 3, 3, 3, 3, 3, 0};
static signed char EA_rtbl_53nit[10] = { 0, 4, 5, 4, 3, 3, 3, 3, 3, 0};
static signed char EA_rtbl_56nit[10] = { 0, 4, 5, 4, 2, 2, 3, 3, 3, 0};
static signed char EA_rtbl_60nit[10] = { 0, 2, 5, 4, 2, 2, 3, 3, 3, 0};
static signed char EA_rtbl_64nit[10] = { 0, 2, 5, 4, 2, 2, 2, 3, 3, 0};
static signed char EA_rtbl_68nit[10] = { 0, 4, 4, 4, 2, 2, 2, 3, 3, 0};
static signed char EA_rtbl_72nit[10] = { 0, 3, 4, 3, 2, 2, 2, 3, 3, 0};
static signed char EA_rtbl_77nit[10] = { 0, 2, 4, 3, 2, 1, 2, 3, 3, 0};
static signed char EA_rtbl_82nit[10] = { 0, 3, 3, 2, 2, 1, 2, 2, 2, 0};
static signed char EA_rtbl_87nit[10] = { 0, 4, 3, 3, 2, 1, 3, 3, 2, 0};
static signed char EA_rtbl_93nit[10] = { 0, 4, 3, 3, 1, 1, 3, 2, 2, 0};
static signed char EA_rtbl_98nit[10] = { 0, 4, 3, 2, 2, 1, 2, 3, 1, 0};
static signed char EA_rtbl_105nit[10] ={ 0, 3, 4, 3, 3, 2, 3, 3, 2, 0};
static signed char EA_rtbl_111nit[10] ={ 0, 3, 3, 2, 2, 1, 2, 3, 1, 0};
static signed char EA_rtbl_119nit[10] ={ 0, 2, 3, 2, 1, 1, 2, 2, 2, 0};
static signed char EA_rtbl_126nit[10] ={ 0, 3, 3, 2, 1, 1, 3, 4, 1, 0};
static signed char EA_rtbl_134nit[10] ={ 0, 3, 3, 2, 1, 1, 3, 3, 1, 0};
static signed char EA_rtbl_143nit[10] ={ 0, 4, 2, 1, 1, 1, 3, 2, 2, 0};
static signed char EA_rtbl_152nit[10] ={ 0, 4, 2, 2, 0, 1, 2, 3, 1, 0};
static signed char EA_rtbl_162nit[10] ={ 0, 2, 3, 1, 1, 2, 2, 2, 2, 0};
static signed char EA_rtbl_172nit[10] ={ 0, 3, 2, 1, 1, 1, 2, 2, 2, 0};
static signed char EA_rtbl_183nit[10] ={ 0, 2, 2, 1, 1, 1, 2, 2, 2, 0};
static signed char EA_rtbl_195nit[10] ={ 0, 4, 1, 0, 1, 1, 2, 2, 2, 0};
static signed char EA_rtbl_207nit[10] ={ 0, 3, 1, 1, 0, 1, 2, 1, 2, 0};
static signed char EA_rtbl_220nit[10] ={ 0, 3, 1, 0, 0, 1, 1, 1, 2, 0};
static signed char EA_rtbl_234nit[10] ={ 0, 0, 1, 0, 0, 0, 1, 1, 2, 0};
static signed char EA_rtbl_249nit[10] ={ 0, 2, 1, 0, 0, 0, 1, 1, 1, 0};
static signed char EA_rtbl_265nit[10] ={ 0, 2, 1, 1, 0, -1, 1, 0, 0, 0};
static signed char EA_rtbl_282nit[10] ={ 0, 2, 0, 0, 0, 0, 0, 1, 1, 0};
static signed char EA_rtbl_300nit[10] ={ 0, 3, 0, -1, -1, 0, 0, 0, 0, 0};
static signed char EA_rtbl_316nit[10] ={ 0, 3, 0, 0, -1, 0, 0, 0, 2, 0};
static signed char EA_rtbl_333nit[10] ={ 0, 2, 1, 0, -1, 0, 0, 1, 0, 0};
static signed char EA_rtbl_360nit[10] ={ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};


/* rgb_offset*/
/* R11(R,G,B) ~ R255(R,G,B) */
static signed char EA_ctbl_5nit[30] =   {0, 0, 0, 0, 0, 0, -1, 4, -9, -1, 4, -10, -5, 4, -9, -8, 4, -8, -7, 3, -8, -5, 1, -4, -2, 0, -2, -3, 0, -3};
static signed char EA_ctbl_6nit[30] =   {0, 0, 0, 0, 0, 0, -1, 4, -10, -2, 5, -11, -5, 4, -9, -7, 3, -7, -6, 3, -7, -5, 0, -4, -2, 0, -2, -2, 0, -2};
static signed char EA_ctbl_7nit[30] =   {0, 0, 0, 0, 0, 0, -1, 5, -11, -2, 4, -10, -5, 4, -9, -7, 3, -8, -5, 3, -7, -5, 0, -4, -2, 0, -2, -1, 0, -1};
static signed char EA_ctbl_8nit[30] =   {0, 0, 0, 0, 0, 0, -1, 4, -9, -3, 5, -11, -5, 4, -10, -7, 3, -8, -4, 2, -6, -4, 0, -3, -2, 0, -2, -1, 0, -1};
static signed char EA_ctbl_9nit[30] =   {0, 0, 0, 0, 0, 0, -1, 4, -10, -3, 5, -10, -3, 4, -8, -6, 4, -8, -4, 2, -5, -4, 0, -3, -1, 0, -1, -1, 0, -1};
static signed char EA_ctbl_10nit[30] =  {0, 0, 0, 0, 0, 0, -1, 5, -10, -3, 4, -10, -6, 4, -10, -8, 3, -8, -2, 2, -5, -4, 0, -3, -1, 0, -1, -1, 0, -1};
static signed char EA_ctbl_11nit[30] =  {0, 0, 0, 0, 0, 0, -1, 5, -12, -3, 4, -10, -5, 4, -9, -7, 3, -8, -2, 2, -5, -3, 0, -1, -2, 0, -2, 0, 0, 0};
static signed char EA_ctbl_12nit[30] =  {0, 0, 0, 0, 0, 0, 0, 4, -10, -2, 4, -8, -5, 4, -10, -5, 3, -8, -1, 2, -4, -4, 0, -2, -1, 0, -1, 0, 0, 0};
static signed char EA_ctbl_13nit[30] =  {0, 0, 0, 0, 0, 0, -1, 4, -10, -4, 4, -10, -4, 3, -8, -5, 3, -7, -1, 2, -4, -3, 0, -1, -1, 0, -1, 0, 0, 0};
static signed char EA_ctbl_14nit[30] =  {0, 0, 0, 0, 0, 0, 0, 4, -10, -4, 4, -10, -4, 4, -8, -5, 3, -7, -1, 1, -4, -2, 0, -1, -1, 0, -1, 0, 0, 0};
static signed char EA_ctbl_15nit[30] =  {0, 0, 0, 0, 0, 0, 0, 4, -10, -2, 4, -9, -5, 3, -7, -5, 4, -8, 0, 1, -4, -2, 0, -1, -1, 0, -1, 0, 0, 0};
static signed char EA_ctbl_16nit[30] =  {0, 0, 0, 0, 0, 0, 0, 4, -10, -2, 4, -10, -5, 3, -7, -5, 3, -8, 0, 2, -4, -1, 0, 0, -1, 0, -1, 0, 0, 0};
static signed char EA_ctbl_17nit[30] =  {0, 0, 0, 0, 0, 0, 0, 4, -10, -2, 4, -9, -4, 3, -6, -4, 3, -6, -1, 1, -4, -1, 0, 0, -1, 0, -1, 0, 0, 0};
static signed char EA_ctbl_19nit[30] =  {0, 0, 0, 0, 0, 0, 1, 4, -10, -4, 4, -10, -4, 3, -6, -4, 3, -7, 0, 1, -3, -2, 0, -1, 0, 0, 0, 0, 0, 0};
static signed char EA_ctbl_20nit[30] =  {0, 0, 0, 0, 0, 0, 1, 5, -11, -2, 4, -9, -4, 3, -6, -2, 3, -6, 0, 1, -3, -2, 0, -1, 0, 0, 0, 0, 0, 0};
static signed char EA_ctbl_21nit[30] =  {0, 0, 0, 0, 0, 0, 0, 5, -11, -2, 4, -8, -4, 2, -5, -3, 3, -8, 0, 1, -2, -1, 0, -1, 0, 0, 0, 0, 0, 0};
static signed char EA_ctbl_22nit[30] =  {0, 0, 0, 0, 0, 0, 0, 4, -10, -2, 4, -9, -4, 2, -4, -2, 3, -7, 0, 1, -3, -1, 0, 0, 0, 0, 0, 0, 0, 0};
static signed char EA_ctbl_24nit[30] =  {0, 0, 0, 0, 0, 0, 0, 5, -12, -3, 3, -8, -3, 2, -4, -3, 3, -6, 0, 1, -3, -1, 0, 0, 0, 0, 0, 0, 0, 0};
static signed char EA_ctbl_25nit[30] =  {0, 0, 0, 0, 0, 0, 1, 5, -11, -3, 3, -8, -3, 2, -4, -3, 3, -7, 1, 1, -2, -2, 0, -1, 0, 0, 0, 0, 0, 0};
static signed char EA_ctbl_27nit[30] =  {0, 0, 0, 0, 0, 0, 0, 4, -10, -1, 4, -8, -5, 2, -4, -1, 3, -6, 0, 1, -2, -1, 0, 0, 0, 0, 0, 0, 0, 0};
static signed char EA_ctbl_29nit[30] =  {0, 0, 0, 0, 0, 0, 0, 4, -10, -1, 3, -8, -4, 2, -4, -2, 3, -6, 0, 0, -2, -1, 0, 0, 0, 0, 0, 0, 0, 0};
static signed char EA_ctbl_30nit[30] =  {0, 0, 0, 0, 0, 0, 0, 4, -10, -2, 3, -8, -4, 1, -4, -2, 3, -6, 0, 0, -2, -1, 0, 0, 0, 0, 0, 0, 0, 0};
static signed char EA_ctbl_32nit[30] =  {0, 0, 0, 0, 0, 0, 1, 5, -11, -2, 3, -8, -4, 1, -4, -2, 2, -6, 1, 0, -2, -1, 0, 0, 0, 0, 0, 0, 0, 0};
static signed char EA_ctbl_34nit[30] =  {0, 0, 0, 0, 0, 0, 0, 4, -10, -1, 3, -7, -3, 1, -2, -2, 2, -6, 0, 0, -1, 0, 0, -1, 0, 0, 0, 0, 0, 0};
static signed char EA_ctbl_37nit[30] =  {0, 0, 0, 0, 0, 0, 0, 5, -12, -1, 3, -6, -3, 0, -2, -1, 2, -5, 0, 0, -2, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static signed char EA_ctbl_39nit[30] =  {0, 0, 0, 0, 0, 0, 1, 5, -12, 0, 2, -6, -4, 1, -2, 0, 2, -4, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static signed char EA_ctbl_41nit[30] =  {0, 0, 0, 0, 0, 0, 0, 5, -12, 0, 2, -6, -4, 0, -2, -1, 2, -5, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static signed char EA_ctbl_44nit[30] =  {0, 0, 0, 0, 0, 0, 0, 4, -10, 0, 2, -6, -4, 1, -2, 0, 2, -5, 2, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static signed char EA_ctbl_47nit[30] =  {0, 0, 0, 0, 0, 0, 0, 4, -10, 0, 2, -6, -4, 0, -2, 0, 2, -5, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static signed char EA_ctbl_50nit[30] =  {0, 0, 0, 0, 0, 0, 0, 4, -9, 0, 2, -5, -3, 0, -1, 0, 2, -5, 2, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static signed char EA_ctbl_53nit[30] =  {0, 0, 0, 0, 0, 0, 1, 5, -10, 0, 2, -5, -2, 0, -1, 0, 2, -4, 2, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static signed char EA_ctbl_56nit[30] =  {0, 0, 0, 0, 0, 0, 1, 4, -10, 1, 2, -4, -4, 0, -1, 0, 2, -4, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static signed char EA_ctbl_60nit[30] =  {0, 0, 0, 0, 0, 0, 2, 4, -9, 0, 2, -4, -4, 0, -1, 0, 1, -4, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static signed char EA_ctbl_64nit[30] =  {0, 0, 0, 0, 0, 0, 2, 4, -9, 0, 1, -4, -4, 0, -1, -1, 1, -4, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static signed char EA_ctbl_68nit[30] =  {0, 0, 0, 0, 0, 0, 2, 4, -10, 0, 1, -4, -3, 0, 0, -1, 1, -4, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static signed char EA_ctbl_72nit[30] =  {0, 0, 0, 0, 0, 0, 2, 4, -9, 1, 1, -3, -3, 0, 0, 0, 1, -4, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static signed char EA_ctbl_77nit[30] =  {0, 0, 0, 0, 0, 0, 2, 4, -9, 1, 1, -2, -2, 0, 0, 0, 1, -3, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static signed char EA_ctbl_82nit[30] =  {0, 0, 0, 0, 0, 0, 2, 4, -8, 1, 1, -3, -2, 0, 1, 1, 1, -4, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static signed char EA_ctbl_87nit[30] =  {0, 0, 0, 0, 0, 0, 2, 3, -8, 1, 1, -2, -2, 0, 1, 1, 1, -3, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static signed char EA_ctbl_93nit[30] =  {0, 0, 0, 0, 0, 0, 2, 3, -8, 1, 1, -2, -3, 0, 0, 2, 1, -3, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static signed char EA_ctbl_98nit[30] =  {0, 0, 0, 0, 0, 0, 2, 3, -8, 0, 0, -2, -2, 0, 0, 2, 1, -3, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static signed char EA_ctbl_105nit[30] = {0, 0, 0, 0, 0, 0, 1, 3, -8, 0, 1, -2, -2, 0, -1, 0, 1, -4, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static signed char EA_ctbl_111nit[30] = {0, 0, 0, 0, 0, 0, 0, 3, -8, 0, 0, -2, -2, 0, -1, 2, 1, -3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static signed char EA_ctbl_119nit[30] = {0, 0, 0, 0, 0, 0, 0, 3, -7, 0, 0, -2, -1, 0, 0, 2, 1, -3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static signed char EA_ctbl_126nit[30] = {0, 0, 0, 0, 0, 0, 0, 3, -7, -1, 0, -2, 0, 0, 0, 3, 1, -2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1};
static signed char EA_ctbl_134nit[30] = {0, 0, 0, 0, 0, 0, 1, 3, -6, -2, 0, -2, -1, 0, -1, 3, 1, -2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1};
static signed char EA_ctbl_143nit[30] = {0, 0, 0, 0, 0, 0, 1, 2, -6, -1, 0, -1, -1, 0, -1, 3, 1, -3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1};
static signed char EA_ctbl_152nit[30] = {0, 0, 0, 0, 0, 0, 1, 2, -6, 0, 0, 0, -1, 0, -1, 3, 1, -2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1};
static signed char EA_ctbl_162nit[30] = {0, 0, 0, 0, 0, 0, 1, 1, -4, -1, 0, 0, 0, 1, -2, 3, 0, -2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1};
static signed char EA_ctbl_172nit[30] = {0, 0, 0, 0, 0, 0, 2, 2, -5, -1, 0, 0, -1, 0, -2, 4, 0, -2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1};
static signed char EA_ctbl_183nit[30] = {0, 0, 0, 0, 0, 0, 2, 1, -4, -1, 0, 0, -1, 0, -2, 4, 0, -2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1};
static signed char EA_ctbl_195nit[30] = {0, 0, 0, 0, 0, 0, 2, 2, -4, -1, 0, 1, 0, 0, -2, 4, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static signed char EA_ctbl_207nit[30] = {0, 0, 0, 0, 0, 0, 2, 1, -4, 0, 0, 1, -1, 0, -2, 4, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static signed char EA_ctbl_220nit[30] = {0, 0, 0, 0, 0, 0, 2, 1, -4, 0, 0, 0, 0, 0, -1, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static signed char EA_ctbl_234nit[30] = {0, 0, 0, 0, 0, 0, 3, 1, -2, 0, 0, 0, 0, 0, 0, 3, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static signed char EA_ctbl_249nit[30] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static signed char EA_ctbl_265nit[30] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static signed char EA_ctbl_282nit[30] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static signed char EA_ctbl_300nit[30] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static signed char EA_ctbl_316nit[30] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static signed char EA_ctbl_333nit[30] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static signed char EA_ctbl_360nit[30] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};


// ACL ON, CAPS OFF, 4 members: register address, ACL&CAPS setting, elvss DIM off set, elvss DIM off set (temperature < 0)
static unsigned char elv_360[4] = {0xb6, 0x48, 0x8A, 0x80}; /* 360 */
static unsigned char elv_333[4] = {0xb6, 0x48, 0x8B, 0x81}; /* 333 */
static unsigned char elv_316[4] = {0xb6, 0x48, 0x8B, 0x81}; /* 316 */
static unsigned char elv_300[4] = {0xb6, 0x48, 0x8C, 0x82}; /* 300 */
static unsigned char elv_282[4] = {0xb6, 0x48, 0x8C, 0x82}; /* 282 */
static unsigned char elv_265[4] = {0xb6, 0x48, 0x8D, 0x83}; /* 265 */
static unsigned char elv_249[4] = {0xb6, 0x48, 0x8E, 0x84}; /* 249 */
static unsigned char elv_234[4] = {0xb6, 0x48, 0x8F, 0x85}; /* 234 */
static unsigned char elv_220[4] = {0xb6, 0x48, 0x8F, 0x85}; /* 220 */
static unsigned char elv_207[4] = {0xb6, 0x48, 0x8F, 0x85}; /* 207 */
static unsigned char elv_195[4] = {0xb6, 0x48, 0x8F, 0x85}; /* 195 */
static unsigned char elv_183[4] = {0xb6, 0x48, 0x8F, 0x85}; /* 183 */
static unsigned char elv_172[4] = {0xb6, 0x48, 0x8F, 0x85}; /* 172 */
static unsigned char elv_162[4] = {0xb6, 0x48, 0x8F, 0x85}; /* 162 */
static unsigned char elv_152[4] = {0xb6, 0x48, 0x8F, 0x85}; /* 152 */
static unsigned char elv_143[4] = {0xb6, 0x48, 0x90, 0x86}; /* 143 */
static unsigned char elv_134[4] = {0xb6, 0x48, 0x91, 0x87}; /* 134 */
static unsigned char elv_126[4] = {0xb6, 0x48, 0x91, 0x87}; /* 126 */
static unsigned char elv_119[4] = {0xb6, 0x48, 0x92, 0x88}; /* 119 */
static unsigned char elv_111[4] = {0xb6, 0x48, 0x92, 0x88}; /* 111 */
static unsigned char elv_105[4] = {0xb6, 0x48, 0x93, 0x89}; /* 105 */
static unsigned char elv_98[4]  = {0xb6, 0x48, 0x93, 0x89}; /* 98 */
static unsigned char elv_92[4]  = {0xb6, 0x48, 0x94, 0x8A}; /* 92 */
static unsigned char elv_88[4]  = {0xb6, 0x48, 0x94, 0x8A}; /* 88 */
static unsigned char elv_82[4]  = {0xb6, 0x48, 0x94, 0x8A}; /* 82 */
static unsigned char elv_77[4]  = {0xb6, 0x48, 0x95, 0x8B}; /* 77 */
static unsigned char elv_72[4]  = {0xb6, 0x48, 0x95, 0x8B}; /* 72~5 */


// ACL OFF, CAPS ON, 4 members: register address, ACL&CAPS setting, elvss DIM off set, elvss DIM off set (temperature < 0)
static unsigned char elv_caps_360[4] = {0xb6, 0x58, 0x8A, 0x80}; /* 360 */
static unsigned char elv_caps_333[4] = {0xb6, 0x58, 0x8B, 0x81}; /* 333 */
static unsigned char elv_caps_316[4] = {0xb6, 0x58, 0x8B, 0x81}; /* 316 */
static unsigned char elv_caps_300[4] = {0xb6, 0x58, 0x8C, 0x82}; /* 300 */
static unsigned char elv_caps_282[4] = {0xb6, 0x58, 0x8C, 0x82}; /* 282 */
static unsigned char elv_caps_265[4] = {0xb6, 0x58, 0x8D, 0x83}; /* 265 */
static unsigned char elv_caps_249[4] = {0xb6, 0x58, 0x8E, 0x84}; /* 249 */
static unsigned char elv_caps_234[4] = {0xb6, 0x58, 0x8F, 0x85}; /* 234 */
static unsigned char elv_caps_220[4] = {0xb6, 0x58, 0x8F, 0x85}; /* 220 */
static unsigned char elv_caps_207[4] = {0xb6, 0x58, 0x8F, 0x85}; /* 207 */
static unsigned char elv_caps_195[4] = {0xb6, 0x58, 0x8F, 0x85}; /* 195 */
static unsigned char elv_caps_183[4] = {0xb6, 0x58, 0x8F, 0x85}; /* 183 */
static unsigned char elv_caps_172[4] = {0xb6, 0x58, 0x8F, 0x85}; /* 172 */
static unsigned char elv_caps_162[4] = {0xb6, 0x58, 0x8F, 0x85}; /* 162 */
static unsigned char elv_caps_152[4] = {0xb6, 0x58, 0x8F, 0x85}; /* 152 */
static unsigned char elv_caps_143[4] = {0xb6, 0x58, 0x90, 0x86}; /* 143 */
static unsigned char elv_caps_134[4] = {0xb6, 0x58, 0x91, 0x87}; /* 134 */
static unsigned char elv_caps_126[4] = {0xb6, 0x58, 0x91, 0x87}; /* 126 */
static unsigned char elv_caps_119[4] = {0xb6, 0x58, 0x92, 0x88}; /* 119 */
static unsigned char elv_caps_111[4] = {0xb6, 0x58, 0x92, 0x88}; /* 111 */
static unsigned char elv_caps_105[4] = {0xb6, 0x58, 0x93, 0x89}; /* 105 */
static unsigned char elv_caps_98[4]  = {0xb6, 0x58, 0x93, 0x89}; /* 98 */
static unsigned char elv_caps_92[4]  = {0xb6, 0x58, 0x94, 0x8A}; /* 92 */
static unsigned char elv_caps_88[4]  = {0xb6, 0x58, 0x94, 0x8A}; /* 88 */
static unsigned char elv_caps_82[4]  = {0xb6, 0x58, 0x94, 0x8A}; /* 82 */
static unsigned char elv_caps_77[4]  = {0xb6, 0x58, 0x95, 0x8B}; /* 77 */
static unsigned char elv_caps_72[4]  = {0xb6, 0x58, 0x95, 0x8B}; /* 72~5 */
#endif
