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
#include <linux/printk.h>

#include "dsp-config.h"
#include "npu-log.h"

#define DSP_LOG_TAG	"DSP"

#if IS_ENABLED(CONFIG_EXYNOS_MEMORY_LOGGER)
#define dsp_err(fmt, args...)						\
do {									\
	dsp_memlog_printf(MEMLOG_LEVEL_ERR, "[%d: %15s:%5d]" "%s: ERR:%4d:" fmt,		\
			__smp_processor_id(), current->comm, current->pid, DSP_LOG_TAG, __LINE__, ##args);			\
} while (0)

#define dsp_warn(fmt, args...)						\
do {									\
	dsp_memlog_printf(MEMLOG_LEVEL_CAUTION, "[%d: %15s:%5d]" "%s: WAR:%4d:" fmt,	\
			__smp_processor_id(), current->comm, current->pid, DSP_LOG_TAG, __LINE__, ##args);			\
} while (0)

#define dsp_notice(fmt, args...)					\
do {									\
	dsp_memlog_printf(MEMLOG_LEVEL_CAUTION, "[%d: %15s:%5d]" "%s: NOT:%4d:" fmt,	\
			__smp_processor_id(), current->comm, current->pid, DSP_LOG_TAG, __LINE__, ##args);			\
} while (0)

#define dsp_info(fmt, args...)						\
do {									\
	dsp_memlog_printf(MEMLOG_LEVEL_CAUTION, "[%d: %15s:%5d]" "%s: INF:%4d:" fmt,	\
			__smp_processor_id(), current->comm, current->pid, DSP_LOG_TAG, __LINE__, ##args);			\
} while (0)

#define dsp_dump(fmt, args...)						\
do {									\
	dsp_dumplog_printf(MEMLOG_LEVEL_ERR, "[%d: %15s:%5d]" "%s: INF:%4d:" fmt,	\
			__smp_processor_id(), current->comm, current->pid, DSP_LOG_TAG, __LINE__, ##args);			\
} while (0)

#if defined(ENABLE_DYNAMIC_DEBUG_LOG)
#define dsp_dbg(fmt, args...)						\
do {									\
	dsp_memlog_printf(MEMLOG_LEVEL_DEBUG, "[%d: %15s:%5d]" "%s: DBG:%4d:" fmt,	\
			__smp_processor_id(), current->comm, current->pid, DSP_LOG_TAG, __LINE__, ##args);			\
} while (0)
#define dsp_dl_dbg(fmt, args...)					\
do {									\
	dsp_memlog_printf(MEMLOG_LEVEL_DEBUG, "[%d: %15s:%5d]" "%s: DL-DBG:%4d:" fmt,	\
			__smp_processor_id(), current->comm, current->pid, DSP_LOG_TAG, __LINE__, ##args);			\
} while (0)
#else  // ENABLE_DYNAMIC_DEBUG_LOG
#define dsp_dbg(fmt, args...)						\
	pr_debug("%s: DBG:%4d:" fmt, DSP_LOG_TAG, __LINE__, ##args)
#define dsp_dl_dbg(fmt, args...)					\
	pr_debug("%s: DL-DBG:%4d:" fmt, DSP_LOG_TAG, __LINE__, ##args)
#endif  // ENABLE_DYNAMIC_DEBUG_LOG

#else  // CONFIG_EXYNOS_MEMORY_LOGGER

#define dsp_err(fmt, args...)						\
	pr_err("%s %s: ERR:%4d:" fmt, DSP_LOG_TAG, __func__, __LINE__, ##args)
#define dsp_warn(fmt, args...)						\
	pr_warn("%s %s: WAR:%4d:" fmt, DSP_LOG_TAG, __func__, __LINE__, ##args)

#define dsp_notice(fmt, args...)					\
	pr_notice("%s %s: NOT:%4d:" fmt, DSP_LOG_TAG, __func__, __LINE__, ##args)

#if defined(ENABLE_INFO_LOG)
#define dsp_info(fmt, args...)						\
	pr_info("%s: INF:%4d:" fmt, DSP_LOG_TAG, __LINE__, ##args)
#else
#define dsp_info(fmt, args...)
#endif

#if defined(ENABLE_DYNAMIC_DEBUG_LOG)
#define dsp_dbg(fmt, args...)						\
do {									\							\
		pr_debug("%s: DBG:%4d" fmt, DSP_LOG_TAG, __LINE__,	\
				##args);				\
} while (0)
#define dsp_dl_dbg(fmt, args...)					\
do {									\						\
		pr_debug("%s: DL-DBG:%4d:" fmt, DSP_LOG_TAG, __LINE__,	\
				##args);				\
} while (0)
#else  // ENABLE_DYNAMIC_DEBUG_LOG
#define dsp_dbg(fmt, args...)					\
	pr_debug("%s: DBG:%4d:" fmt, DSP_LOG_TAG, __LINE__, ##args)
#define dsp_dl_dbg(fmt, args...)				\
	pr_debug("%s: DL-DBG:%4d:" fmt, DSP_LOG_TAG, __LINE__, ##args)
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

static inline unsigned int dsp_log_get_debug_ctrl(void)
{
	/* always-on in NPU */
	return 1;
}

#if IS_ENABLED(CONFIG_EXYNOS_MEMORY_LOGGER)
#define dsp_memlog_printf	npu_memlog_store
#define dsp_dumplog_printf	npu_dumplog_store
#endif

#endif  // __DSP_LOG_H__
