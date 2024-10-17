/*
 * Samsung Exynos SoC series NPU driver
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef _NPU_GOVERNOR_H_
#define _NPU_GOVERNOR_H_

#include <linux/types.h>
#include <linux/mutex.h>
#include <linux/workqueue.h>
#include <linux/wait.h>
#include <soc/samsung/exynos_pm_qos.h>

#include <linux/version.h>
#include <linux/types.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/device.h>
#include <linux/sysfs.h>
#include <soc/samsung/exynos_pm_qos.h>
#include <linux/pm_opp.h>
#include <linux/pm_runtime.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/string.h>
#include <linux/thermal.h>
#include "npu-sessionmgr.h"

#define GOV_NPU_FREQ		(0)
#define	GOV_MIF_FREQ		(1)
#define GOV_INT_FREQ		(2)
#define NPU_FREQ_INFO_NUM	(2)
#define NPU_GOVERNOR_NUM	(3)

#define MAX_ALPHA (100000000)
#define MAX_BETA (100000000)
#define MIN_ALPHA (0)
#define MIN_BETA (0)
#if IS_ENABLED(CONFIG_SOC_S5E9945)
#define LOWEST_FREQ_IDX (1)
#define HIGEST_FREQ_IDX (6)
#elif IS_ENABLED(CONFIG_SOC_S5E8845)
#define LOWEST_FREQ_IDX (0)
#define HIGEST_FREQ_IDX (5)
#endif

enum npu_cmdq_status {
	CMDQ_FREE = 0,
	CMDQ_PROCESSING_INST1 = (1 << 0),
	CMDQ_PROCESSING_INSTN0 = (1 << 1),
	CMDQ_PROCESSING_INSTN1 = (1 << 2),
	CMDQ_COMPLETE = (1 << 3),
	CMDQ_PENDING = (1 << 4)
};

#define CMDQ_PROCESSING (CMDQ_PROCESSING_INST1 | CMDQ_PROCESSING_INSTN0 | CMDQ_PROCESSING_INSTN1)

struct cmdq_table {
	u32 status;
	u32 cmdq_pending_pc;
	u32 cmdq_total;
	u32 total_time;
	u32 dsp_time;
};

struct npu_freq_info {
	u32 dnc;
	u32 npu;
	//u32 dsp;
};

struct thr_data {
	struct cmdq_table_info *ptr;
};

struct cmdq_table_info {
	struct mutex lock;
	struct task_struct *kthread;
	struct thr_data thr_data;
};

int start_cmdq_table_read(struct cmdq_table_info *cmdq_info);
int npu_cmdq_table_read_probe(struct cmdq_table_info *cmdq_info);
void npu_cmdq_table_read_close(struct cmdq_table_info *cmdq_info);
void init_workload_params(struct npu_session *session);
void npu_update_frequency_from_queue(struct npu_session *session, u32 buff_cnt);
u32 npu_update_cmdq_progress_from_done(struct npu_queue *queue);
void npu_update_frequency_from_done(struct npu_queue *queue, u32 freq_idx);
void npu_revert_cmdq_list(struct npu_session *session);

#endif//_NPU_GOVERNOR_H_
