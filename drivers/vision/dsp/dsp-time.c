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
	va_list args;
	int len;

	dsp_enter();
	dsp_time_get_interval(time);

	va_start(args, f);
	len = vsnprintf(buf, sizeof(buf), f, args);
	va_end(args);

	if (len > 0)
		dsp_info("[%5lu.%06lu ms] %s\n",
				time->interval.tv_sec * 1000UL +
				time->interval.tv_nsec / 1000000UL,
				(time->interval.tv_nsec % 1000000UL),
				buf);
	else
		dsp_info("[%5lu.%06lu ms] INVALID\n",
				time->interval.tv_sec * 1000UL +
				time->interval.tv_nsec / 1000000UL,
				(time->interval.tv_nsec % 1000000UL));
	dsp_leave();
}
