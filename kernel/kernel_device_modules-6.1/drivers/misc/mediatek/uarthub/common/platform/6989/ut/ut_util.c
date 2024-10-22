// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 MediaTek Inc.
 */
#include <linux/stdarg.h>
#include <linux/kernel.h>
#include "ut_util.h"

int g_uh_ut_verbose = 1;

#define MAX_PRINTF 256

int _do_assert( int result, const char *func,
	const char *file, int line, const char *errmsg_fmt, ...)
{
	int len;
	va_list ap;
	char buf[MAX_PRINTF] = {0};

	if (!result) {
		va_start(ap, errmsg_fmt);
		len = vsnprintf(buf, MAX_PRINTF - 1, errmsg_fmt, ap);
		va_end(ap);
		UTLOG(" [%s] %s: %s:%d", func, buf, file, line);
	}
	return result;
}
