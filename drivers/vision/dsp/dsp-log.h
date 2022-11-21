/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Samsung Exynos SoC series dsp driver
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 */

#ifndef __DSP_LOG_H__
#define __DSP_LOG_H__

#include <linux/device.h>
#if IS_ENABLED(CONFIG_EXYNOS_MEMORY_LOGGER)
#include <soc/samsung/memlogger.h>
#endif

#include "dsp-config.h"

#if IS_ENABLED(CONFIG_EXYNOS_MEMORY_LOGGER)
#define dsp_err(fmt, args...)						\
do {									\
	dev_err(dsp_global_dev, "ERR:%4d:" fmt, __LINE__, ##args);	\
	if (dsp_memlog_log_obj)						\
		memlog_write_printf(dsp_memlog_log_obj,			\
				MEMLOG_LEVEL_ERR, "ERR:%4d:" fmt,	\
				__LINE__, ##args);			\
} while (0)

#define dsp_warn(fmt, args...)						\
do {									\
	dev_warn(dsp_global_dev, "WAR:%4d:" fmt, __LINE__, ##args);	\
	if (dsp_memlog_log_obj)						\
		memlog_write_printf(dsp_memlog_log_obj,			\
				MEMLOG_LEVEL_CAUTION, "WAR:%4d:" fmt,	\
				__LINE__, ##args);			\
} while (0)

#define dsp_notice(fmt, args...)					\
do {									\
	dev_notice(dsp_global_dev, "NOT:%4d:" fmt, __LINE__, ##args);	\
	if (dsp_memlog_log_obj)						\
		memlog_write_printf(dsp_memlog_log_obj,			\
				MEMLOG_LEVEL_CAUTION, "NOT:%4d:" fmt,	\
				__LINE__, ##args);			\
} while (0)

#if defined(ENABLE_INFO_LOG)
#define dsp_info(fmt, args...)						\
do {									\
	dev_info(dsp_global_dev, "INF:%4d:" fmt, __LINE__, ##args);	\
	if (dsp_memlog_log_obj)						\
		memlog_write_printf(dsp_memlog_log_obj,			\
				MEMLOG_LEVEL_CAUTION, "INF:%4d:" fmt,	\
				__LINE__, ##args);			\
} while (0)
#else
#define dsp_info(fmt, args...)						\
do {									\
	if (dsp_memlog_log_obj)						\
		memlog_write_printf(dsp_memlog_log_obj,			\
				MEMLOG_LEVEL_NOTICE, "INF:%4d:" fmt,	\
				__LINE__, ##args);			\
} while (0)
#endif

#if defined(ENABLE_DYNAMIC_DEBUG_LOG)
#define dsp_dbg(fmt, args...)						\
do {									\
	if (dsp_debug_log_enable & 0x1)					\
		dsp_info("DBG:" fmt, ##args);				\
	else								\
		dev_dbg(dsp_global_dev, "DBG:%4d:" fmt, __LINE__,	\
				##args);				\
	if (dsp_memlog_log_obj)						\
		memlog_write_printf(dsp_memlog_log_obj,			\
				MEMLOG_LEVEL_DEBUG, "DBG:%4d:" fmt,	\
				__LINE__, ##args);			\
} while (0)
#define dsp_dl_dbg(fmt, args...)					\
do {									\
	if (dsp_debug_log_enable & 0x2)					\
		dsp_info("DL-DBG:" fmt, ##args);			\
	else								\
		dev_dbg(dsp_global_dev, "DL-DBG:%4d:" fmt, __LINE__,	\
				##args);				\
	if (dsp_memlog_log_obj)						\
		memlog_write_printf(dsp_memlog_log_obj,			\
				MEMLOG_LEVEL_DEBUG, "DBG:%4d:" fmt,	\
				__LINE__, ##args);			\
} while (0)
#else  // ENABLE_DYNAMIC_DEBUG_LOG
#define dsp_dbg(fmt, args...)						\
	dev_dbg(dsp_global_dev, "DBG:%4d:" fmt, __LINE__, ##args)
#define dsp_dl_dbg(fmt, args...)					\
	dev_dbg(dsp_global_dev, "DL-DBG:%4d:" fmt, __LINE__, ##args)
#endif  // ENABLE_DYNAMIC_DEBUG_LOG

#else  // CONFIG_EXYNOS_MEMORY_LOGGER

#define dsp_err(fmt, args...)						\
	dev_err(dsp_global_dev, "ERR:%4d:" fmt, __LINE__, ##args)
#define dsp_warn(fmt, args...)						\
	dev_warn(dsp_global_dev, "WAR:%4d:" fmt, __LINE__, ##args)

#define dsp_notice(fmt, args...)					\
	dev_notice(dsp_global_dev, "NOT:%4d:" fmt, __LINE__, ##args)

#if defined(ENABLE_INFO_LOG)
#define dsp_info(fmt, args...)						\
	dev_info(dsp_global_dev, "INF:%4d:" fmt, __LINE__, ##args)
#else
#define dsp_info(fmt, args...)
#endif

#if defined(ENABLE_DYNAMIC_DEBUG_LOG)
#define dsp_dbg(fmt, args...)						\
do {									\
	if (dsp_debug_log_enable & 0x1)					\
		dsp_info("DBG:" fmt, ##args);				\
	else								\
		dev_dbg(dsp_global_dev, "DBG:%4d" fmt, __LINE__,	\
				##args);				\
} while (0)
#define dsp_dl_dbg(fmt, args...)					\
do {									\
	if (dsp_debug_log_enable & 0x2)					\
		dsp_info("DL-DBG:" fmt, ##args);			\
	else								\
		dev_dbg(dsp_global_dev, "DL-DBG:%4d:" fmt, __LINE__,	\
				##args);				\
} while (0)
#else  // ENABLE_DYNAMIC_DEBUG_LOG
#define dsp_dbg(fmt, args...)					\
	dev_dbg(dsp_global_dev, "DBG:%4d:" fmt, __LINE__, ##args)
#define dsp_dl_dbg(fmt, args...)				\
	dev_dbg(dsp_global_dev, "DL-DBG:%4d:" fmt, __LINE__, ##args)
#endif  // ENABLE_DYNAMIC_DEBUG_LOG
#endif  // CONFIG_EXYNOS_MEMORY_LOGGER

#if defined(ENABLE_CALL_PATH_LOG)
#define dsp_enter()		dsp_info("[%s] enter\n", __func__)
#define dsp_leave()		dsp_info("[%s] leave\n", __func__)
#define dsp_check()		dsp_info("[%s] check\n", __func__)
#else
#define dsp_enter()
#define dsp_leave()
#define dsp_check()
#endif

extern struct device *dsp_global_dev;
extern unsigned int dsp_debug_log_enable;
#if IS_ENABLED(CONFIG_EXYNOS_MEMORY_LOGGER)
extern struct memlog_obj *dsp_memlog_log_obj;
#endif

#endif  // __DSP_LOG_H__
