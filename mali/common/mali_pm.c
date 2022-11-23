/*
 * Copyright (C) 2011-2013 ARM Limited. All rights reserved.
 * 
 * This program is free software and is provided to you under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation, and any use by you of this program is subject to the terms of such GNU licence.
 * 
 * A copy of the licence is included with the program, and can also be obtained from Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include "mali_pm.h"
#include "mali_kernel_common.h"
#include "mali_osk.h"
#include "mali_gp_scheduler.h"
#include "mali_pp_scheduler.h"
#include "mali_scheduler.h"
#include "mali_kernel_utilization.h"
#include "mali_group.h"
#include "mali_pm_domain.h"
#include "mali_pmu.h"

#define GPU_DDR_DFS_FREQ 0

#if GPU_DDR_DFS_FREQ
extern void dfs_freq_raise_quirk(unsigned int req_bw);

struct workqueue_struct *gpu_ddr_dfs_workqueue = NULL;

static void gpu_ddr_dfs_func(struct work_struct *work);
static DECLARE_WORK(gpu_ddr_dfs_work, &gpu_ddr_dfs_func);

static void gpu_ddr_dfs_func(struct work_struct *work)
{
	dfs_freq_raise_quirk(400000);
}
#endif

static mali_bool mali_power_on = MALI_FALSE;

static u32 num_active_gps = 0;
static u32 num_active_pps = 0;

_mali_osk_errcode_t mali_pm_initialize(void)
{
	_mali_osk_pm_dev_enable();
	
#if GPU_DDR_DFS_FREQ
	if(gpu_ddr_dfs_workqueue == NULL)
	{
		gpu_ddr_dfs_workqueue = create_singlethread_workqueue("gpu_ddr_dfs");
	}
#endif

	return _MALI_OSK_ERR_OK;
}

void mali_pm_terminate(void)
{
	mali_pm_domain_terminate();
	_mali_osk_pm_dev_disable();

#if GPU_DDR_DFS_FREQ
	if (gpu_ddr_dfs_workqueue != NULL) {
		destroy_workqueue(gpu_ddr_dfs_workqueue);
		gpu_ddr_dfs_workqueue = NULL;
	}
#endif
}

extern void mali_platform_power_mode_change(int power_mode);

/* Reset GPU after power up */
static void mali_pm_reset_gpu(void)
{
	/* Reset all L2 caches */
	mali_l2_cache_reset_all();

	/* Reset all groups */
	mali_scheduler_reset_all_groups();
}

void mali_pm_os_suspend(void)
{
	MALI_DEBUG_PRINT(3, ("Mali PM: OS suspend\n"));
	mali_gp_scheduler_suspend();
	mali_pp_scheduler_suspend();
	mali_utilization_suspend();
	mali_group_power_off(MALI_TRUE);
	mali_power_on = MALI_FALSE;
	mali_platform_power_mode_change(2);
}

void mali_pm_os_resume(void)
{
	struct mali_pmu_core *pmu = mali_pmu_get_global_pmu_core();
	mali_bool do_reset = MALI_FALSE;

	MALI_DEBUG_PRINT(3, ("Mali PM: OS resume\n"));

	if (MALI_TRUE != mali_power_on) {
		mali_platform_power_mode_change(0);
		do_reset = MALI_TRUE;
	}

	if (NULL != pmu) {
		mali_pmu_reset(pmu);
	}

	mali_power_on = MALI_TRUE;
	_mali_osk_write_mem_barrier();

	if (do_reset) {
		mali_pm_reset_gpu();
		mali_group_power_on();
	}

	mali_gp_scheduler_resume();
	mali_pp_scheduler_resume();
}

void mali_pm_runtime_suspend(void)
{
	MALI_DEBUG_PRINT(3, ("Mali PM: Runtime suspend\n"));
	mali_group_power_off(MALI_TRUE);
	mali_power_on = MALI_FALSE;
	mali_platform_power_mode_change(1);
}

void mali_pm_runtime_resume(void)
{
	struct mali_pmu_core *pmu = mali_pmu_get_global_pmu_core();
	mali_bool do_reset = MALI_FALSE;

	MALI_DEBUG_PRINT(3, ("Mali PM: Runtime resume\n"));

	if (MALI_TRUE != mali_power_on) {
		mali_platform_power_mode_change(0);
		do_reset = MALI_TRUE;
	}

	if (NULL != pmu) {
		mali_pmu_reset(pmu);
	}

	mali_power_on = MALI_TRUE;
	_mali_osk_write_mem_barrier();

	if (do_reset) {
		mali_pm_reset_gpu();
		mali_group_power_on();
	}
}

void mali_pm_set_power_is_on(void)
{
	mali_power_on = MALI_TRUE;
}

mali_bool mali_pm_is_power_on(void)
{
	return mali_power_on;
}

void mali_pm_core_event(enum mali_core_event core_event)
{
#if GPU_DDR_DFS_FREQ
	mali_bool transition_working = MALI_FALSE;
	mali_bool transition_idle = MALI_FALSE;

	switch (core_event)
	{
		case MALI_CORE_EVENT_GP_START:
			if (num_active_pps + num_active_gps == 0)
			{
				transition_working = MALI_TRUE;
			}
			num_active_gps++;
			break;
		case MALI_CORE_EVENT_GP_STOP:
			if (num_active_pps + num_active_gps == 1)
			{
				transition_idle = MALI_TRUE;
			}
			num_active_gps--;
			break;
		case MALI_CORE_EVENT_PP_START:
			if (num_active_pps + num_active_gps == 0)
			{
				transition_working = MALI_TRUE;
			}
			num_active_pps++;
			break;
		case MALI_CORE_EVENT_PP_STOP:
			if (num_active_pps + num_active_gps == 1)
			{
				transition_idle = MALI_TRUE;
			}
			num_active_pps--;
			break;
	}

	if ((transition_working == MALI_TRUE) &&
		(gpu_ddr_dfs_workqueue != NULL))
	{
		queue_work(gpu_ddr_dfs_workqueue, &gpu_ddr_dfs_work);
	}
#endif

}
