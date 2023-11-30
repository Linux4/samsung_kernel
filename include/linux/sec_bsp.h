/* sec_bsp.h
 *
 * Copyright (C) 2014 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 *
 */

#ifndef SEC_BSP_H
#define SEC_BSP_H


extern void sec_boot_stat_add(const char * c);
extern void sec_bootstat_add_initcall(const char *s);
extern void sec_suspend_resume_add(const char * c);

extern struct list_head device_init_time_list;

struct device_init_time_entry {
	struct list_head next;
	char *buf;
	unsigned long long duration;
};

#define DEVICE_INIT_TIME_100MS 100000

#endif
