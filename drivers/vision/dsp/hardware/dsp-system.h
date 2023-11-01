/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Samsung Exynos SoC series dsp driver
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 */

#ifndef __DSP_SYSTEM_H__
#define __DSP_SYSTEM_H__

#include "dsp-hw-system.h"

struct dsp_device;

int dsp_system_execute_task(struct dsp_system *sys, struct dsp_task *task);
void dsp_system_iovmm_fault_dump(struct dsp_system *sys);

int dsp_system_boot(struct dsp_system *sys);
int dsp_system_reset(struct dsp_system *sys);

int dsp_system_power_active(struct dsp_system *sys);
int dsp_system_runtime_resume(struct dsp_system *sys);
int dsp_system_runtime_suspend(struct dsp_system *sys);
int dsp_system_resume(struct dsp_system *sys);
int dsp_system_suspend(struct dsp_system *sys);

int dsp_system_npu_start(struct dsp_system *sys, bool boot, dma_addr_t fw_iova);
int dsp_system_start(struct dsp_system *sys);
int dsp_system_stop(struct dsp_system *sys);

int dsp_system_open(struct dsp_system *sys);
int dsp_system_close(struct dsp_system *sys);
int dsp_system_probe(struct dsp_device *dspdev);
void dsp_system_remove(struct dsp_system *sys);

#endif
