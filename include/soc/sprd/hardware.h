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

#ifndef __ASM_ARCH_SCI_HARDWARE_H
#define __ASM_ARCH_SCI_HARDWARE_H
/*
#define SCI_IOMAP(x)	(SCI_IOMAP_BASE + (x))

#ifndef SCI_ADDR
#define SCI_ADDR(_b_, _o_)                              ( (u32)(_b_) + (_o_) )
#endif
*/
#include <asm/sizes.h>

#if defined(CONFIG_64BIT)
#define SPRD_IRAM2_PHYS         0x50002000
#define SPRD_IRAM2_SIZE         (SZ_16K + SZ_4K + SZ_2K + SZ_1K)
#endif

#if !defined(CONFIG_64BIT)
#if defined(CONFIG_ARCH_SCX35L)
#include "__hardware-sc9630.h"
#elif defined(CONFIG_ARCH_SCX30G)
#include "__hardware-sc7731.h"
#elif defined(CONFIG_ARCH_SCX35)
#include "__hardware-sc8830.h"
#elif defined(CONFIG_ARCH_SC8825)
#include "__hardware-sc8825.h"
#else
#error "Unknown architecture specification"
#endif
#endif

#endif
