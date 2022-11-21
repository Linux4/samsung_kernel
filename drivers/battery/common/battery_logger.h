/*
 * battery_logger.h
 * Samsung Mobile Charger Header
 *
 * Copyright (C) 2012 Samsung Electronics, Inc.
 *
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

#ifndef __BATTERY_LOGGER_H
#define __BATTERY_LOGGER_H __FILE__

extern void store_battery_log(const char *fmt, ...);
extern int register_batterylog_proc(void);
extern void unregister_batterylog_proc(void);
extern unsigned int lpcharge;

#endif /* __BATTERY_LOGGER_H */
