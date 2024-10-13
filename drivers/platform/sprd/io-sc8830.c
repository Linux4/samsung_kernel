/*
 * Copyright (C) 2012 Spreadtrum Communications Inc.
 * Author: steve.zhan <steve.zhan@spreadtrum.com>
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/bug.h>

#include <asm/io.h>
#include <asm/page.h>
#include <asm/mach/map.h>

#include <soc/sprd/hardware.h>
#include <soc/sprd/arch_misc.h>

#define SPRD_DEVICE(name) {			\
	.virtual = SPRD_##name##_BASE,		\
	.pfn = __phys_to_pfn(SPRD_##name##_PHYS),\
	.length = SPRD_##name##_SIZE,		\
	.type = MT_DEVICE_NONSHARED,		\
	}
#define SPRD_IRAM(name) {			\
	.virtual = SPRD_##name##_BASE,		\
	.pfn = __phys_to_pfn(SPRD_##name##_PHYS),\
	.length = SPRD_##name##_SIZE,		\
	.type = MT_MEMORY,			\
	}

static struct map_desc sprd_io_desc[] __initdata = {
	SPRD_DEVICE(UART0),
	SPRD_DEVICE(UART1),
	SPRD_DEVICE(UART2),
	SPRD_DEVICE(UART3),
	SPRD_DEVICE(UART4),
#ifdef CONFIG_ARCH_SCX20L
	SPRD_DEVICE(ARM7AHBRF),
#endif

	SPRD_IRAM(IRAM0),
	SPRD_IRAM(IRAM0H),
#if defined(CONFIG_ARCH_SCX30G)
        SPRD_IRAM(IRAM1),
	SPRD_DEVICE(LPDDR2_PHY),
#else
        SPRD_DEVICE(IRAM1),
#endif
#ifndef CONFIG_ARCH_SCX35L
	SPRD_DEVICE(IRAM2),
#endif
	SPRD_DEVICE(PWM),
	SPRD_DEVICE(LPDDR2),
};

void __init sci_map_io(void)
{
	iotable_init(sprd_io_desc, ARRAY_SIZE(sprd_io_desc));
}

