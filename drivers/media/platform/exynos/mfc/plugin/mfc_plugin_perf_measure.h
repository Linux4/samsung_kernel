/*
 * drivers/media/platform/exynos/mfc/plugin/mfc_plugin_perf_measure.h
 *
 * Copyright (c) 2022 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef __MFC_PLUGIN_PERF_MEASURE_H
#define __MFC_PLUGIN_PERF_MEASURE_H __FILE__

#include <linux/time.h>
#include <linux/seq_file.h>

#include "base/mfc_common.h"

void mfc_plugin_perf_print(struct mfc_core *core);
void mfc_plugin_perf_result(struct seq_file *s, int id);

#ifndef PERF_MEASURE

static inline void mfc_plugin_perf_init(struct mfc_core *core) {}
static inline void mfc_plugin_perf_measure_on(struct mfc_core *core) {}
static inline void mfc_plugin_perf_measure_off(struct mfc_core *core) {}

#else

void mfc_plugin_measure_store(struct mfc_core *core, int diff);

static inline void mfc_plugin_perf_init(struct mfc_core *core)
{
	core->perf.new_start = 0;
	core->perf.count = 0;
	core->perf.drv_margin = 0;
	core->perf.latency = 0;

	core->perf.data.count_A = 0;
}

static inline void mfc_plugin_perf_measure_on(struct mfc_core *core)
{
	struct timespec64 time_diff;

	if (core->perf.drv_margin) {
		ktime_get_ts64(&core->perf.end);

		time_diff = timespec64_sub(core->perf.end, core->perf.begin);
		core->perf.latency = (unsigned int)(timespec64_to_ns(&time_diff) / NSEC_PER_USEC);
		core->perf.drv_margin = 0;
	}

	ktime_get_ts64(&core->perf.begin);

	core->perf.new_start = 1;
	core->perf.count++;
}

static inline void mfc_plugin_perf_measure_off(struct mfc_core *core)
{
	struct timespec64 time_diff;
	unsigned int diff;

	if ((core->perf.new_start) && (core->perf.count > 0)) {
		ktime_get_ts64(&core->perf.end);

		time_diff = timespec64_sub(core->perf.end, core->perf.begin);
		diff = (unsigned int)(timespec64_to_ns(&time_diff) / NSEC_PER_USEC);

		mfc_plugin_measure_store(core, diff);

		core->perf.drv_margin = 1;

		ktime_get_ts64(&core->perf.begin);
	}

	core->perf.new_start = 0;
}
#endif

#endif /* __MFC_PLUGIN_PERF_MEASURE_H */
