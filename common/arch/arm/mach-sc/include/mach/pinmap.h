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

#ifndef __ASM_ARM_ARCH_PINMAP_H
#define __ASM_ARM_ARCH_PINMAP_H

#include "hardware.h"

#define BIT_NONE            0x00
#define BIT_0               0x01
#define BIT_1               0x02
#define BIT_2               0x04
#define BIT_3               0x08
#define BIT_4               0x10
#define BIT_5               0x20
#define BIT_6               0x40
#define BIT_7               0x80
#define BIT_8               0x0100
#define BIT_9               0x0200
#define BIT_10              0x0400
#define BIT_11              0x0800
#define BIT_12              0x1000
#define BIT_13              0x2000
#define BIT_14              0x4000
#define BIT_15              0x8000
#define BIT_16              0x010000
#define BIT_17              0x020000
#define BIT_18              0x040000
#define BIT_19              0x080000
#define BIT_20              0x100000
#define BIT_21              0x200000
#define BIT_22              0x400000
#define BIT_23              0x800000
#define BIT_24              0x01000000
#define BIT_25              0x02000000
#define BIT_26              0x04000000
#define BIT_27              0x08000000
#define BIT_28              0x10000000
#define BIT_29              0x20000000
#define BIT_30              0x40000000
#define BIT_31              0x80000000

#if defined(CONFIG_ARCH_SCX15)
#include "chip_x15/__regs_pinmap.h"
#elif defined(CONFIG_ARCH_SCX30G)
#include "chip_x30g/__regs_pinmap.h"
#elif defined(CONFIG_ARCH_SCX35)
#include "chip_x35/__regs_pinmap.h"
#else
#error "Unknown architecture specification"
#endif	/* end of CONFIG_ARCH_SCX15 */

u32 pinmap_get(u32 offset);
int pinmap_set(u32 offset, u32 value);

#endif	/* end of __ASM_ARM_ARCH_PINMAP_H */
