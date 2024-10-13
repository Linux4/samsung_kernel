/* * Copyright (C) 2012 Spreadtrum Communications Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __ADI_H__
#define __ADI_H__

#ifdef  CONFIG_SC_INTERNAL_ADI
int sci_adi_init(void);

int sci_adi_read(u32 reg);

int sci_is_adi_vaddr(u32 vaddr);

/*
 * using adi method to translate pysical address to virtual address
 * return 0 is ok.
 */
int sci_adi_p2v(u32 paddr, u32 *vaddr);


/*
 * WARN: the arguments (reg, value) is different from
 * the general __raw_writel(value, reg)
 * For sci_adi_write_fast: if set sync 1, then this function will
 * return until the val have reached hardware.otherwise, just
 * async write(is maybe in software buffer)
 */
int sci_adi_write_fast(u32 reg, u16 val, u32 sync);
int sci_adi_write(u32 reg, u16 or_val, u16 clear_msk);
void __init ana_init_irq(void);

#else

#include <asm-generic/errno-base.h>

static inline int sci_adi_init(void)
{
	return -ENODEV;
}

static inline int sci_adi_read(u32 reg)
{
	return -ENODEV;
}

static inline int sci_is_adi_vaddr(u32 vaddr)
{
	return 0;
}

/*
 * using adi method to translate pysical address to virtual address
 * return 0 is ok.
 */
static inline int sci_adi_p2v(u32 paddr, u32 *vaddr)
{
	return -ENODEV;
}

static inline int sci_adi_write_fast(u32 reg, u16 val, u32 sync)
{
	return -ENODEV;
}

static inline int sci_adi_write(u32 reg, u16 or_val, u16 clear_msk)
{
	return -ENODEV;
}

static inline void ana_init_irq(void){ }

#endif
static inline int sci_adi_raw_write(u32 reg, u16 val)
{
	return sci_adi_write_fast(reg, val, 1);
}

static inline int sci_adi_set(u32 reg, u16 bits)
{
	return sci_adi_write(reg, bits, 0);
}

static inline int sci_adi_clr(u32 reg, u16 bits)
{
	return sci_adi_write(reg, 0, bits);
}

#endif
