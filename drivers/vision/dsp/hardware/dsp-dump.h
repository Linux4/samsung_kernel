/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Samsung Exynos SoC series dsp driver
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 */

#ifndef __DSP_DUMP_H__
#define __DSP_DUMP_H__

#include "dsp-hw-dump.h"

struct dsp_mailbox_pool;
struct dsp_task_manager;
struct dsp_kernel_manager;

void dsp_dump_set_value(unsigned int dump_value);

void dsp_dump_print_value(void);
void dsp_dump_print_status_user(struct seq_file *file);

void dsp_dump_ctrl(void);
void dsp_dump_ctrl_user(struct seq_file *file);

void dsp_dump_mailbox_pool_error(struct dsp_mailbox_pool *pool);
void dsp_dump_mailbox_pool_debug(struct dsp_mailbox_pool *pool);
void dsp_dump_task_manager_count(struct dsp_task_manager *tmgr);
void dsp_dump_kernel(struct dsp_kernel_manager *kmgr);

#endif
