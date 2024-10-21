/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2023 MediaTek Inc.
 */

#include <linux/string.h>
#include <linux/kernel.h>
#include "apusys_trace.h"

#if IS_ENABLED(CONFIG_FTRACE)
#ifdef apusys_rv_trace_begin
#undef apusys_rv_trace_begin
#endif
#define _apusys_rv_trace_begin(format, args...) \
	{ \
		char buf[256]; \
		int len; \
		len = snprintf(buf, sizeof(buf), \
			format "%s", args); \
		trace_tag_begin(buf); \
	}

#define apusys_rv_trace_begin(...) _apusys_rv_trace_begin(__VA_ARGS__, "")
#ifdef apusys_rv_trace_end
#undef apusys_rv_trace_end
#endif
#define apusys_rv_trace_end() \
	{ \
		trace_tag_end(); \
	}
#else
#define apusys_rv_trace_begin(...)
#define apusys_rv_trace_end(...)
#endif /* CONFIG_FTRACE */
