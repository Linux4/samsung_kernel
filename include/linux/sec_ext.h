/* sec_ext.h
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

#ifndef SEC_EXT_H
#define SEC_EXT_H


/*
 * Bootstat @ /proc/boot_stat
 */
#ifdef CONFIG_SEC_BOOTSTAT
extern bool init_command_debug;
extern int board_rev;

extern void sec_boot_stat_set_bl_boot_time(int pl_t, int lk_t);
extern void sec_boot_stat_get_start_kernel(void);
extern void sec_boot_stat_get_mct(u32);
extern void sec_boot_stat_add(const char *);
extern void sec_boot_stat_add_initcall(const char *);
extern void sec_boot_stat_add_init_command(const char *name,
	int duration);
#ifdef CONFIG_SEC_DEVICE_BOOTSTAT
#define DEVICE_INIT_TIME_100MS 100000
extern struct list_head device_init_time_list;
struct device_init_time_entry {
	struct list_head next;
	char *buf;
	unsigned long long duration;
};
#endif
#define MAX_LENGTH_OF_SYSTEMSERVER_LOG 90
struct systemserver_init_time_entry {
	struct list_head next;
	char buf[MAX_LENGTH_OF_SYSTEMSERVER_LOG];
};
#endif /* CONFIG_SEC_BOOT_STAT */

/*
 * Param op.
 */
#ifdef CONFIG_SEC_PARAM
#define CM_OFFSET				CONFIG_CM_OFFSET
#define CM_OFFSET_LIMIT				CM_OFFSET+4
#define WC_OFFSET				CONFIG_WC_OFFSET
#define WC_OFFSET_LIMIT				0
#define FMM_LOCK_OFFSET				CONFIG_FMM_LOCK_OFFSET

enum {
	PARAM_OFF = '0',
	PARAM_ON = '1',
};

extern int sec_set_param(unsigned long offset, char val);
extern int sec_set_param_str(unsigned long offset, const char *val, int size);
extern int sec_set_param_extra(unsigned long offset, void *extra, size_t size);
extern int sec_get_param_str(unsigned long offset, char *val);
extern void sec_debug_recovery_reboot(void);
#else
#define sec_set_param(a, b)			(-1)
#endif /* CONFIG_SEC_PARAM */

#endif /* CONFIG_SEC_EXT */
