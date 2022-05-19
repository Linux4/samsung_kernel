/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Samsung Exynos SoC series dsp driver
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 */

#ifndef __DSP_HARDWARE_DEBUG_H__
#define __DSP_HARDWARE_DEBUG_H__

#include "dsp-util.h"

#include "dsp-hw-debug.h"

struct dsp_device;

struct dsp_hw_debug_log {
	struct timer_list	timer;
	struct dsp_util_queue	*queue;
	bool			log_file;
};

void dsp_hw_debug_log_flush(struct dsp_hw_debug *debug);

int dsp_hw_debug_log_start(struct dsp_hw_debug *debug);
int dsp_hw_debug_log_stop(struct dsp_hw_debug *debug);

int dsp_hw_debug_open(struct dsp_hw_debug *debug);
int dsp_hw_debug_close(struct dsp_hw_debug *debug);

int dsp_hw_debug_probe(struct dsp_device *dspdev);
void dsp_hw_debug_remove(struct dsp_hw_debug *debug);

#endif
