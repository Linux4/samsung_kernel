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

#ifndef __ASM_ARM_ARCH_PIN_SWITCH_H
#define __ASM_ARM_ARCH_PIN_SWITCH_H

struct sprd_pin_switch_platform_data {
	const char * power_domain;
	u32 reg;
	u32 bit_offset;
	u32 bit_width;
};

enum power_domain_id {
	VDD28,
	VDD28_1,
	VDDSIM0,
	VDDSIM1,
	VDDSIM2,
	VDDSD,
	VDD18,
	//PD_CNT,
};

#ifdef CONFIG_ARCH_SCX20
#define PD_CNT 5
#else
#define PD_CNT 7
#endif

#define VOL_THRESHOLD 1800000

#endif

