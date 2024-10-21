/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#if IS_ENABLED(CONFIG_FTRACE)

#ifdef TRACE_LEN
#undef TRACE_LEN
#endif

#define TRACE_LEN 256

void trace_tag_begin(const char *format, ...);
void trace_tag_end(void);
void trace_tag_customer(const char *fmt, ...);
void trace_async_tag(bool isBegin, const char *format, ...);

#else
static inline void trace_tag_begin(const char *format, ...)
{
}

static inline void trace_tag_end(void)
{
}

static inline void trace_tag_customer(const char *fmt, ...)
{
}

static inline void trace_async_tag(const char *fmt, ...)
{
}
#endif

