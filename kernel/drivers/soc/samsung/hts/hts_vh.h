/*
 * Copyright (c) 2023 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#pragma once

#include "hts_data.h"
#if IS_ENABLED(CONFIG_SCHED_EMS)
#include "../../../../kernel/sched/ems/ems.h"
#endif

#define SYS_ECTLR				(sys_reg(3, 0, 15, 1, 4))
#define SYS_ECTLR2				(sys_reg(3, 0, 15, 1, 5))

#define MAX_ENTRIES_MASK			(4)
#if IS_ENABLED(CONFIG_SCHED_EMS)
#define TASK_CONFIG_CONTROL(task)		(task_avd(task)->hts_config_control)
#else
#define TASK_CONFIG_CONTROL(task)		(task->android_vendor_data1[32])
#endif

enum reg_list {
	REG_CTRL1,
	REG_CTRL2,
	REG_CTRL_END
};

struct ext_ctrl_reg
{
	bool enable;
	unsigned long ext_ctrl[MAX_ENTRIES_MASK];
};

struct hts_config_control
{
	struct ext_ctrl_reg ctrl[REG_CTRL_END];
	struct hts_app_event_data *app_event;
};

struct hts_config_control *hts_get_or_alloc_task_config(struct task_struct *task);
void hts_free_task_config(struct task_struct *task);

int register_hts_vh(struct platform_device *pdev);

int register_hts_ctx_switch_vh(struct hts_data *data);
int unregister_hts_ctx_switch_vh(struct hts_data *data);
