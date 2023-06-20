/* Copyright (c) 2010, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#define I2C_M_WR 0		/* for i2c */
#define I2c_M_RD 1		/* for i2c */

bool bl_I2cWrite8(unsigned char Addr, unsigned long Data);

int bl_I2cRead8(u8 reg, u16 *val);
int samsung_bl_init(void);
