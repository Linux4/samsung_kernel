/*
 * Copyright (C) 2012 Spreadtrum Communications Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

/**
 * sci_glb_read - read value from d-die global register
 * @reg: global register address
 * @msk:
 *
 * If read all bits, set msk = -1
 *
 * Return read value and mask.
 */
u32 sci_glb_read(u32 reg, u32 msk);

/**
 * sci_glb_write - safely write value to d-die global register
 * @reg: global register address
 * @val:
 * @msk:
 *
 * If write all bits, set msk = -1
 *
 * Returns success (0) or negative errno.
 */
int sci_glb_write(u32 reg, u32 val, u32 msk);

/**
 * sci_glb_set - force set bit to d-die global register
 * @reg: global register address
 * @bit: commonly, only set one bit
 *
 *
 * tiger use stand-alone xxx_set/xxx_clr address for all global register
 *
 * Returns success (0) or negative errno.
 */
int sci_glb_set(u32 reg, u32 bit);

/**
 * sci_glb_clr - force clr bit to d-die gglobal register
 * @reg: global register address
 * @bit: commonly, only clear one bit
 *
 *
 * tiger use stand-alone xxx_set/xxx_clr address for all d-die global register
 *
 * Returns success (0) or negative errno.
 */
int sci_glb_clr(u32 reg, u32 bit);

#define sci_glb_raw_read(reg)           sci_glb_read(reg, -1UL)
#define sci_glb_raw_write(reg, val)     sci_glb_write(reg, val, -1UL)

#define sci_assert(condition)	do { WARN_ON(!(condition)); }while(0)

