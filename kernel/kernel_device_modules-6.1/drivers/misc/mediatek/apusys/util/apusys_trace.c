// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/sched.h>
#include <linux/string.h>
#include <linux/kernel.h>
#include "apusys_trace.h"
#define CREATE_TRACE_POINTS
#include "mtk_apu_events.h"
#include "sw_logger.h"

void trace_tag_customer(const char *fmt, ...)
{
	char buf[TRACE_LEN];
	int32_t ret;
	va_list args;

	va_start(args, fmt);
	ret = vsnprintf(buf, TRACE_LEN, fmt, args);
	if (ret)
		pr_info("%s: vsnprintf error\n", __func__);
	va_end(args);

	trace_tracing_mark_write(buf);
}

void trace_tag_begin(const char *format, ...)
{
	char buf[TRACE_LEN];

	int len = snprintf(buf, sizeof(buf),
		"B|%d|%s", task_tgid_nr(current), format);

	if (len >= TRACE_LEN)
		len = TRACE_LEN - 1;

	trace_tracing_mark_write(buf);
}

void trace_tag_end(void)
{
	char buf[TRACE_LEN];

	int len = snprintf(buf, sizeof(buf),
		"E|%d", task_tgid_nr(current));

	if (len >= TRACE_LEN)
		len = TRACE_LEN - 1;

	trace_tracing_mark_write(buf);
}

void trace_async_tag(bool isBegin, const char *format, ...)
{
	char buf[TRACE_LEN];
	int len = 0;

	if (isBegin)
		len = snprintf(buf, sizeof(buf),
			       "S|%d|%s", task_tgid_nr(current), format);
	else
		len = snprintf(buf, sizeof(buf),
			       "F|%d|%s", task_tgid_nr(current), format);

	if (len >= TRACE_LEN)
		len = TRACE_LEN - 1;

	trace_tracing_mark_write(buf);
}
