/*
 * Samsung Exynos SoC series NPU driver
 *
 * Copyright (c) 2017 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef _NPU_SYSTEM_H_
#define _NPU_SYSTEM_H_

#include <linux/platform_device.h>
#include <linux/wakelock.h>
#include "npu-scheduler.h"
#include "npu-qos.h"
#include "npu-clock.h"

#ifdef CONFIG_NPU_HARDWARE
#include "npu-interface.h"
#include "mailbox.h"
#else
#ifdef CONFIG_NPU_LOOPBACK
#include "interface/loopback/npu-interface.h"
#include "interface/loopback/mailbox.h"
#endif

#endif

#ifdef CONFIG_NPU_USE_SPROFILER
#include "npu-profile.h"
#endif

#include "npu-exynos.h"
#if 0
#include "npu-interface.h"
#endif

#include "npu-memory.h"

#include "npu-binary.h"
#if 0
#include "npu-tv.h"
#endif

#include "npu-fw-test-handler.h"

struct npu_system {
	struct platform_device	*pdev;

	struct npu_iomem_area	tcu_sram;
	struct npu_iomem_area	idp_sram;

	struct npu_iomem_area	sfr_dnc;
	struct npu_iomem_area	sfr_npuc[2];
	struct npu_iomem_area	sfr_mbox[2];

	struct npu_iomem_area	sfr_npu[4];
#ifdef CONFIG_CORESIGHT_STM
	struct npu_iomem_area	sfr_coresight;
	struct npu_iomem_area	sfr_stm;
	struct npu_iomem_area	sfr_mct_g;
#endif
	struct npu_iomem_area	pmu_npu;
	struct npu_iomem_area	pmu_npu_cpu;
	struct npu_iomem_area	mbox_sfr;
	struct npu_iomem_area	pwm_npu;
	struct npu_fw_test_handler	npu_fw_test_handler;
	struct npu_memory_buffer	*fw_npu_memory_buffer;

	struct npu_memory_buffer	*fw_npu_unittest_buffer;
	struct npu_memory_buffer	*fw_npu_log_buffer;

	int			irq0;
	int			irq1;

	struct npu_qos_setting	qos_setting;

	u32			max_npu_core;
	struct npu_clocks	clks;
#ifdef CONFIG_PM_SLEEP
	/* maintain to be awake */
	struct wake_lock 	npu_wake_lock;
#endif

	struct npu_exynos exynos;
	struct npu_memory memory;
	struct npu_binary binary;

	volatile struct mailbox_hdr	*mbox_hdr;
	volatile struct npu_interface	*interface;

#ifdef CONFIG_NPU_USE_SPROFILER
	struct npu_profile_control	profile_ctl;
#endif
	/* Open status (Bitfield of npu_system_resume_steps) */
	unsigned long			resume_steps;
	unsigned long			resume_soc_steps;
};

int npu_system_probe(struct npu_system *system, struct platform_device *pdev);
int npu_system_release(struct npu_system *system, struct platform_device *pdev);
int npu_system_open(struct npu_system *system);
int npu_system_close(struct npu_system *system);
int npu_system_resume(struct npu_system *system, u32 mode);
int npu_system_suspend(struct npu_system *system);
int npu_system_start(struct npu_system *system);
int npu_system_stop(struct npu_system *system);
#endif
