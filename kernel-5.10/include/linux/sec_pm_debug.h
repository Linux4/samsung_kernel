/*
 * sec_pm_debug.h
 *
 *  Copyright (c) 2022 Samsung Electronics Co., Ltd.
 *      http://www.samsung.com
 *  Jonghyeon Cho <jongjaaa.cho.cho@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __LINUX_SEC_PM_DEBUG_H__
#define __LINUX_SEC_PM_DEBUG_H__

#define THERMAL_ZONE_MAX	20
#define DEFAULT_LOGGING_MS	5000

struct sec_pm_dbg {
	struct delayed_work ws_work;
	unsigned int ws_work_ms;

	int tz_cnt;
	char *tz_list[THERMAL_ZONE_MAX];
	struct delayed_work tz_work;
	unsigned int tz_work_ms;
};

#endif /* __LINUX_SEC_PM_DEBUG_H__ */
