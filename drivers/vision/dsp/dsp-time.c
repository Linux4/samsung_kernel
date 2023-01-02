// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series dsp driver
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 */

#include "dsp-log.h"
#include "dsp-time.h"

void dsp_time_get_timestamp(struct dsp_time *time, int opt)
{
	dsp_enter();
	if (opt == TIMESTAMP_START)
		getrawmonotonic(&time->start);
	else if (opt == TIMESTAMP_END)
		getrawmonotonic(&time->end);
	else
		dsp_warn("time opt is invaild(%d)\n", opt);
	dsp_leave();
}

void dsp_time_get_interval(struct dsp_time *time)
{
	time->interval = timespec_sub(time->end, time->start);
}

void dsp_time_print(struct dsp_time *time, const char *f, ...)
{
	char buf[128];
	int len, size;
	va_list args;

	dsp_enter();
	dsp_time_get_interval(time);

	len = snprintf(buf, sizeof(buf), "[%2lu.%09lu sec] ",
			time->interval.tv_sec, time->interval.tv_nsec);
	if (len < 0) {
		dsp_warn("Failed to print time\n");
		return;
	}
	size = len;

	va_start(args, f);
	len = vsnprintf(buf + size, sizeof(buf) - size, f, args);
	if (len > 0)
		size += len;
	va_end(args);
	if (buf[size - 1] != '\n')
		buf[size - 1] = '\n';

	dsp_info("%s", buf);
	dsp_leave();
}
