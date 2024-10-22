/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/string.h>
#include <linux/kernel.h>
#include "apusys_trace.h"

#if IS_ENABLED(CONFIG_FTRACE)
extern u8 apummu_apusys_trace;
#ifdef ammu_trace_begin
#undef ammu_trace_begin
#endif
#define _ammu_trace_begin(format, args...) \
	{ \
		char buf[256]; \
		int len; \
		if (apummu_apusys_trace) { \
			len = snprintf(buf, sizeof(buf), \
				format "%s", args); \
			trace_tag_begin(buf); \
		} \
	}

#define ammu_trace_begin(...) _ammu_trace_begin(__VA_ARGS__, "")
#ifdef ammu_trace_end
#undef ammu_trace_end
#endif
#define ammu_trace_end() \
	{ \
		if (apummu_apusys_trace) { \
			trace_tag_end(); \
		} \
	}
#else
#define ammu_trace_begin(...)
#define ammu_trace_end(...)
#endif /* CONFIG_FTRACE */
