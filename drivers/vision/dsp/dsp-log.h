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

#define ENABLE_DYNAMIC_DEBUG_LOG
//#define ENABLE_CALL_PATH_LOG
#define ENABLE_INFO_LOG

#define dsp_err(fmt, args...)					\
	dev_err(dsp_global_dev, "(%4d)" fmt, __LINE__, ##args)
#define dsp_warn(fmt, args...)					\
	dev_warn(dsp_global_dev, "(%4d)" fmt, __LINE__, ##args)
#define dsp_notice(fmt, args...)				\
	dev_notice(dsp_global_dev, "(%4d)" fmt, __LINE__, ##args)

#if defined(ENABLE_INFO_LOG)
#define dsp_info(fmt, args...)					\
	dev_info(dsp_global_dev, "(%4d)" fmt, __LINE__, ##args)
#else
#define dsp_info(fmt, args...)
#endif

#if defined(ENABLE_DYNAMIC_DEBUG_LOG)
#define dsp_dbg(fmt, args...)						\
do {									\
	if (dsp_debug_log_enable)					\
		dsp_info("[DBG]" fmt, ##args);				\
	else								\
		dev_dbg(dsp_global_dev, "(%4d)" fmt, __LINE__, ##args);	\
} while (0)
#else
#define dsp_dbg(fmt, args...)					\
	dev_dbg(dsp_global_dev, "(%4d)" fmt, __LINE__, ##args)
#endif

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

#endif
