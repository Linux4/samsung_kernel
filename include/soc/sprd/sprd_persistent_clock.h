/*
 * Copyright (C) 2014 Spreadtrum Communications Inc.
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

#ifndef __PERSISTENT_CLOCK_SPRD_H
#define __PERSISTENT_CLOCK_SPRD_H

struct sprd_ap_system_timer {
	void __iomem *base;
	int irqnr;
};

u32 get_sys_cnt(void);
void set_ap_system_timer_expires(u32 expires_ms);
#endif
