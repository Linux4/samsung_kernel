/*
 * Copyright (C) 2012 Spreadtrum Communications Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#ifndef _SIN_COS_H_
#define _SIN_COS_H_
#include <linux/types.h>

#define COSSIN_Q 30
#define pi              3.14159265359
#define PI_32           0x3243F6A88
#define ARC_32_COEF     0x80000000
#define ARC_32(arc) (int32_t)((arc) / (pi) * ARC_32_COEF);
int32_t sin_32(int32_t n);
int32_t cos_32(int32_t n);
#endif
