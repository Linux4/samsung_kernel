/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Samsung Exynos SoC series dsp driver
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 */

#ifndef __DSP_HW_SYSTEM_H__
#define __DSP_HW_SYSTEM_H__

#include <linux/device.h>
#include <linux/wait.h>

#include "hardware/dsp-pm.h"
#include "hardware/dsp-clk.h"
#include "hardware/dsp-memory.h"
#include "hardware/dsp-interface.h"
#include "hardware/dsp-ctrl.h"
#include "dsp-task.h"
#include "hardware/dsp-mailbox.h"
#include "hardware/dsp-debug.h"

struct dsp_device;

enum dsp_system_flag {
	DSP_SYSTEM_BOOT,
	DSP_SYSTEM_RESET,
};

enum dsp_system_boot_init {
	DSP_SYSTEM_DSP_INIT,
	DSP_SYSTEM_NPU_INIT,
};

enum dsp_system_wait {
	DSP_SYSTEM_WAIT_BOOT,
	DSP_SYSTEM_WAIT_MAILBOX,
	DSP_SYSTEM_WAIT_RESET,
	DSP_SYSTEM_WAIT_NUM,
};

struct dsp_system {
	struct device			*dev;
	phys_addr_t			sfr_pa;
	void __iomem			*sfr;
	resource_size_t			sfr_size;
	void __iomem			*boot_mem;
	resource_size_t			boot_mem_size;
	unsigned char			boot_bin[SZ_256];
	size_t				boot_bin_size;
	unsigned long			boot_init;
	void __iomem			*chip_id;
	wait_queue_head_t		system_wq;
	unsigned int			system_flag;
	unsigned int			wait[DSP_SYSTEM_WAIT_NUM];
	unsigned int			layer_start;
	unsigned int			layer_end;

	struct dsp_pm			pm;
	struct dsp_clk			clk;
	struct dsp_memory		memory;
	struct dsp_interface		interface;
	struct dsp_ctrl			ctrl;
	struct dsp_task_manager		task_manager;
	struct dsp_mailbox		mailbox;
	struct dsp_hw_debug		debug;
	struct dsp_device		*dspdev;
};

#endif
