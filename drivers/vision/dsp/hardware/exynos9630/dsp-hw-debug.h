/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Samsung Exynos SoC series dsp driver
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 */

#ifndef __DSP_HW_DEBUG_H__
#define __DSP_HW_DEBUG_H__

#include <linux/dcache.h>

#include "hardware/dsp-memory.h"

struct dsp_hw_debug_log;
struct dsp_device;

struct dsp_hw_debug {
	struct dentry		*root;
	struct dentry		*power;
	struct dentry		*clk;
	struct dentry		*devfreq;
	struct dentry		*sfr;
	struct dentry		*mem;
	struct dentry		*fw_log;
	struct dentry		*wait_time;
	struct dentry		*layer_range;
	struct dentry		*mailbox;
	struct dentry		*userdefined;
	struct dentry		*dump_value;
	struct dentry		*firmware_mode;
	struct dentry		*bus;
	struct dentry		*npu_test;
	struct dsp_priv_mem	npu_fw;
	struct dentry		*test;

	struct dsp_hw_debug_log	*log;
	struct dsp_device	*dspdev;
};

#endif
