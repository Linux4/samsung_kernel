/* SPDX-License-Identifier: GPL */
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * Exynos Pablo image subsystem functions
 *
 * Copyright (c) 2021 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef PABLO_ICPU_LOG_H
#define PABLO_ICPU_LOG_H

#define LOGLEVEL_CUSTOM1 (LOGLEVEL_DEBUG + 1)

struct icpu_logger {
	int level;
	bool trace;
	const char *prefix;
};

/* #define ENABLE_ICPU_TRACE */

#define _PREFIX "%s%s:%d: "

#define ICPU_ERR(fmt, args...) \
	do { \
		if (_log.level >= LOGLEVEL_ERR) \
			pr_err("[@][ERR]" _PREFIX fmt, _log.prefix, __FUNCTION__, __LINE__, ##args); \
	} while (0)

/* Print err if _cond is true */
#define ICPU_ERR_IF(_cond, fmt, args...) \
	do { \
		if (_cond) \
			ICPU_ERR(fmt, ##args); \
	} while (0)

#define ICPU_WARN(fmt, args...) \
	do { \
		if (_log.level >= LOGLEVEL_WARNING) \
			pr_warn("[@][WARN]" _PREFIX fmt, _log.prefix, __FUNCTION__, __LINE__, ##args); \
	} while (0)

#define ICPU_INFO(fmt, args...) \
	do { \
		if (_log.level >= LOGLEVEL_INFO) \
			pr_info(_PREFIX fmt, _log.prefix, __FUNCTION__, __LINE__, ##args); \
	} while (0)

#define ICPU_DEBUG(fmt, args...) \
	do { \
		if (_log.level >= LOGLEVEL_DEBUG) \
			pr_info(_PREFIX fmt, _log.prefix, __FUNCTION__, __LINE__, ##args); \
	} while (0)

#ifdef ENABLE_ICPU_TRACE
#define ICPU_TRACE(fmt, args...) \
	do { \
		if (_log.trace) \
			trace_printk(fmt"\n", ##args); \
	} while (0)
#else
#define ICPU_TRACE(fmt, args...)
#endif

#define ICPU_TRACE_BEGIN(fmt, args...) ICPU_TRACE("B|"fmt, ##args)
#define ICPU_TRACE_END(fmt, args...) ICPU_TRACE("E|"fmt, ##args)

#define ICPU_TIME_DECLARE() ktime_t __time_dgb_st
#define ICPU_TIME_BEGIN() \
	do { \
		if (_log.level >= LOGLEVEL_DEBUG) \
			__time_dgb_st = ktime_get(); \
	} while (0)
#define ICPU_TIME_END(fmt, args...) \
	ICPU_DEBUG(fmt " - %lld us", ##args, ktime_to_us(ktime_sub(ktime_get(), __time_dgb_st)))
#endif
