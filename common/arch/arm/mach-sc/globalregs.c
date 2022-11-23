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

/*
* Note: This File is only used in sc8825 project.!!!!!!!!!!!!!!
*/

#include <asm/io.h>
#include <mach/globalregs.h>
#include <linux/spinlock.h>
#include <linux/module.h>

/* vars definitions for controller REGS_GLB */
#define REG_GLB_SET(A)                  ( (A) + 0x1000 )
#define REG_GLB_CLR(A)                  ( (A) + 0x2000 )

static uint32_t globalregs[] = {
	SPRD_GREG_BASE,
	SPRD_AHB_BASE + 0x200,
};

int32_t sprd_greg_read(uint32_t type, uint32_t reg_offset)
{
	return __raw_readl(globalregs[type] + reg_offset);
}

void sprd_greg_write(uint32_t type, uint32_t value, uint32_t reg_offset)
{
	__raw_writel(value, globalregs[type] + reg_offset);
}

void sprd_greg_set_bits(uint32_t type, uint32_t bits, uint32_t reg_offset)
{
	__raw_writel(bits, REG_GLB_SET(globalregs[type] + reg_offset));
}

void sprd_greg_clear_bits(uint32_t type, uint32_t bits, uint32_t reg_offset)
{
	__raw_writel(bits, REG_GLB_CLR(globalregs[type] + reg_offset));
}

EXPORT_SYMBOL(sprd_greg_read);
EXPORT_SYMBOL(sprd_greg_write);
EXPORT_SYMBOL(sprd_greg_set_bits);
EXPORT_SYMBOL(sprd_greg_clear_bits);
