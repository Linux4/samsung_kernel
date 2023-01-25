/* Copyright (c) 2011-2012, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/mfd/pm8xxx/batterydata-lib.h>

static struct single_row_lut fcc_temp = {
	.x		= {-20, 0, 25, 40, 60},
	.y		= {2073, 2079, 2071, 2071, 2061},
	.cols	= 5
};

static struct single_row_lut fcc_sf = {
	.x		= {0},
	.y		= {100},
	.cols	= 1
};


static struct sf_lut rbatt_sf = {
	.rows		= 28,
	.cols			= 5,
	/* row_entries are temperature */
	.row_entries	= {-20, 0, 25, 40, 60},
	.percent		= {100, 95, 90, 85, 80, 75, 70, 65, 60,
		55, 50, 45, 40, 35, 30, 25, 20, 15, 10, 9, 8, 7, 6,
		5, 4, 3, 2, 1},
	.sf			= {
		{741, 246, 100, 83, 78},
		{705, 246, 103, 85, 80},
		{679, 244, 105, 87, 82},
		{680, 239, 108, 89, 84},
		{647, 247, 114, 93, 85},
		{643, 224, 110, 93, 88},
		{645, 225, 116, 97, 90},
		{648, 228, 113, 97, 92},
		{659, 225, 100, 88, 87},
		{677, 224, 98, 83, 79},
		{702, 225, 98, 84, 81},
		{731, 225, 100, 85, 83},
		{766, 229, 102, 87, 86},
		{820, 238, 103, 88, 87},
		{902, 253, 106, 90, 83},
		{1006, 274, 108, 89, 82},
		{1142, 308, 108, 89, 84},
		{1339, 366, 103, 86, 81},
		{806, 274, 97, 82, 81},
		{904, 280, 98, 84, 83},
		{998, 291, 101, 86, 86},
		{1109, 306, 104, 89, 88},
		{1248, 319, 107, 92, 91},
		{1419, 333, 110, 94, 91},
		{1672, 350, 107, 88, 85},
		{2114, 384, 107, 89, 87},
		{4411, 837, 117, 97, 95},
		{4411, 837, 117, 97, 95},
	}
};

static struct pc_temp_ocv_lut pc_temp_ocv = {
	.rows		= 29,
	.cols		= 5,
	.temp		= {-20, 0, 25, 40, 60},
	.percent	= {100, 95, 90, 85, 80, 75, 70, 65, 60, 55, 50, 45, 40,
		35, 30, 25, 20, 15, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0},
	.ocv		= {
				{4327, 4325, 4321, 4316, 4309},
				{4228, 4250, 4252, 4250, 4246},
				{4161, 4193, 4196, 4195, 4191},
				{4098, 4140, 4143, 4141, 4138},
				{4051, 4086, 4093, 4090, 4087},
				{3960, 4038, 4050, 4044, 4039},
				{3913, 3958, 3989, 3994, 3996},
				{3876, 3920, 3957, 3959, 3957},
				{3842, 3891, 3916, 3919, 3919},
				{3816, 3860, 3868, 3872, 3876},
				{3796, 3832, 3838, 3838, 3838},
				{3781, 3807, 3815, 3815, 3814},
				{3767, 3786, 3797, 3796, 3796},
				{3754, 3770, 3782, 3780, 3780},
				{3741, 3757, 3771, 3768, 3763},
				{3725, 3743, 3760, 3754, 3741},
				{3706, 3728, 3740, 3732, 3719},
				{3680, 3714, 3708, 3699, 3688},
				{3642, 3696, 3686, 3679, 3668},
				{3630, 3691, 3685, 3678, 3667},
				{3616, 3686, 3684, 3677, 3666},
				{3598, 3678, 3681, 3675, 3664},
				{3576, 3663, 3676, 3669, 3655},
				{3547, 3636, 3656, 3647, 3627},
				{3506, 3591, 3613, 3601, 3579},
				{3446, 3522, 3550, 3538, 3517},
				{3350, 3418, 3465, 3456, 3435},
				{3218, 3252, 3338, 3328, 3301},
				{3000, 3000, 3000, 3000, 3000}
	}
};


struct bms_battery_data Samsung_8930_Express2_2000mAh_data = {
	.fcc				= 2000,
	.fcc_temp_lut		= &fcc_temp,
	.fcc_sf_lut			= &fcc_sf,
	.pc_temp_ocv_lut	= &pc_temp_ocv,
	.rbatt_sf_lut		= &rbatt_sf,
	.default_rbatt_mohm		= 166,
	.rbatt_capacitive_mohm	= 60,
};
