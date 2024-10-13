/*
 * Copyright (C) 2015 Samsung Electronics, Inc.
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

#ifndef __TZ_HOTPLUG_H__
#define __TZ_HOTPLUG_H__

#include <linux/compiler.h>

struct tz_iwd_cpu_mask {
	uint16_t mask;
	uint16_t counter;
} __packed;

#ifdef CONFIG_TZDEV_HOTPLUG
void tzdev_check_cpu_mask(struct tz_iwd_cpu_mask *cpu_mask);

int tzdev_init_hotplug(void);
void tzdev_exit_hotplug(void);
void tzdev_update_nw_cpu_mask(unsigned long new_mask);
#else
static inline void tzdev_check_cpu_mask(struct tz_iwd_cpu_mask *cpu_mask)
{
	(void) cpu_mask;
}
static inline int tzdev_init_hotplug(void)
{
	return 0;
}
static inline void tzdev_exit_hotplug(void)
{
}
static inline void tzdev_update_nw_cpu_mask(unsigned long new_mask)
{
	(void) new_mask;
}
#endif /* CONFIG_TZDEV_HOTPLUG */

#endif /* __TZ_HOTPLUG_H__ */
