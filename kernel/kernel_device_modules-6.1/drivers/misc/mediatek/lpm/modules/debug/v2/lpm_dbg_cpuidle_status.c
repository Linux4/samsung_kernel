// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/kernel.h>
#include <linux/cpuidle.h>
#include <linux/sched/clock.h>
#include <linux/cpu.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/pm_qos.h>
#include <linux/tick.h>
#include <linux/timer.h>

#include <lpm.h>
#include <lpm_dbg_syssram_v1.h>
#include <mtk_cpuidle_sysfs.h>
#include "mtk_cpupm_dbg.h"
#include "mtk_cpuidle_status.h"
#include "mtk_cpuidle_cpc.h"

#define DUMP_INTERVAL       sec_to_ns(5)
static unsigned long long mtk_lpm_last_cpuidle_dis;

/* stress test */
static unsigned int timer_interval = 10 * 1000;
static struct task_struct *stress_tsk[NR_CPUS];

/* mtk cpu idle configuration */
struct mtk_cpuidle_control {
	bool stress_en;
};

static struct mtk_cpuidle_control mtk_cpuidle_ctrl;

static int mtk_cpuidle_stress_task(void *arg)
{
	while (mtk_cpuidle_ctrl.stress_en)
		usleep_range(timer_interval - 10, timer_interval + 10);

	return 0;
}

static void mtk_cpuidle_stress_start(void)
{
	int i;
	char name[20] = {0};
	int ret = 0;

	if (mtk_cpuidle_ctrl.stress_en)
		return;

	mtk_cpuidle_ctrl.stress_en = true;

	for_each_online_cpu(i) {
		ret = scnprintf(name, sizeof(name), "mtk_cpupm_stress_%d", i);
		stress_tsk[i] =
			kthread_create(mtk_cpuidle_stress_task, NULL, name);

		if (!IS_ERR(stress_tsk[i])) {
			kthread_bind(stress_tsk[i], i);
			wake_up_process(stress_tsk[i]);
		}
	}
}

static void mtk_cpuidle_stress_stop(void)
{
	mtk_cpuidle_ctrl.stress_en = false;
	msleep(20);
}

void mtk_cpuidle_set_stress_test(bool en)
{
	if (en)
		mtk_cpuidle_stress_start();
	else
		mtk_cpuidle_stress_stop();
}

bool mtk_cpuidle_get_stress_status(void)
{
	return mtk_cpuidle_ctrl.stress_en;
}

void mtk_cpuidle_set_stress_time(unsigned int val)
{
	timer_interval = clamp_val(val, 100, 20000);
}

unsigned int mtk_cpuidle_get_stress_time(void)
{
	return timer_interval;
}

#define MTK_CPUIDLE_STATE_EN_GET	(0)
#define MTK_CPUIDLE_STATE_EN_SET	(1)

struct MTK_CSTATE_INFO {
	unsigned int type;
	long val;
};

static struct MTK_CSTATE_INFO state_info;

static int mtk_per_cpuidle_state_param(void *pData)
{
	int i;
	struct cpuidle_driver *drv = cpuidle_get_driver();
	struct MTK_CSTATE_INFO *info = (struct MTK_CSTATE_INFO *)pData;
	int suspend_type = lpm_suspend_type_get();

	if (!drv || !info)
		return 0;

	for (i = drv->state_count - 1; i > 0; i--) {
		if ((suspend_type == LPM_SUSPEND_S2IDLE) &&
			!strcmp(drv->states[i].name, S2IDLE_STATE_NAME))
			continue;
		if (info->type == MTK_CPUIDLE_STATE_EN_SET) {
			mtk_cpuidle_set_param(drv, i, IDLE_PARAM_EN,
					      !!info->val);
		} else if (info->type == MTK_CPUIDLE_STATE_EN_GET)
			info->val += mtk_cpuidle_get_param(drv, i,
					      IDLE_PARAM_EN);
		else
			break;
	}

	//do_exit(0);
	return 0;
}
void mtk_cpuidle_state_enable(bool en)
{
	int cpu;
	struct task_struct *task;

	state_info.type = MTK_CPUIDLE_STATE_EN_SET;
	state_info.val = (long)en;

	cpuidle_pause_and_lock();

	for_each_possible_cpu(cpu) {
		task = kthread_create(mtk_per_cpuidle_state_param,
				&state_info, "mtk_cpuidle_state_enable");
		if (IS_ERR(task)) {
			pr_info("[name:mtk_lpm][P] mtk_cpuidle_state_enable failed\n");
			cpuidle_resume_and_unlock();
			return;
		}
		kthread_bind(task, cpu);
		wake_up_process(task);
	}

	if (!en)
		mtk_lpm_last_cpuidle_dis = sched_clock();

	cpuidle_resume_and_unlock();
}

unsigned long long mtk_cpuidle_state_last_dis_ms(void)
{
	return (mtk_lpm_last_cpuidle_dis / 1000000);
}

long mtk_cpuidle_state_enabled(void)
{
	int cpu;
	long ret;
	struct task_struct *task;

	state_info.type = MTK_CPUIDLE_STATE_EN_GET;
	state_info.val = 0;
	for_each_possible_cpu(cpu) {
		task = kthread_create(mtk_per_cpuidle_state_param,
			(void *)&state_info, "mtk_cpuidle_state_enabled");
		if (IS_ERR(task)) {
			pr_info("[name:mtk_lpm][P] mtk_cpuidle_state_enabled failed\n");
			ret = (long)PTR_ERR(task);
			return ret;
		}
		kthread_bind(task, cpu);
		wake_up_process(task);
	}

	return state_info.val;
}

int mtk_cpuidle_status_init(void)
{
	mtk_cpuidle_ctrl.stress_en = false;

	return 0;
}
EXPORT_SYMBOL(mtk_cpuidle_status_init);

void mtk_cpuidle_status_exit(void)
{
}
EXPORT_SYMBOL(mtk_cpuidle_status_exit);
