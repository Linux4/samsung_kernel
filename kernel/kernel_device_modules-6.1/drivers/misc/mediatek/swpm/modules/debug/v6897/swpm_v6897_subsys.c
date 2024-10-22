// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2022 MediaTek Inc.
 */

#include <linux/cpu.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/perf_event.h>
#include <linux/spinlock.h>
#include <linux/timer.h>
#include <linux/workqueue.h>

#if IS_ENABLED(CONFIG_MTK_TINYSYS_SSPM_SUPPORT)
#include <sspm_reservedmem.h>
#endif

#define CREATE_TRACE_POINTS
#include <swpm_tracker_trace.h>

#include <mtk_swpm_common_sysfs.h>
#include <mtk_swpm_sysfs.h>
#include <swpm_dbg_common_v1.h>
#include <swpm_module.h>
#include <swpm_v6897.h>
#include <swpm_v6897_subsys.h>
#include <swpm_audio_v6897.h>

/****************************************************************************
 *  Global Variables
 ****************************************************************************/
struct power_rail_data swpm_power_rail[NR_POWER_RAIL] = {
	[VPROC3] = {0, "VPROC3"},
	[VPROC2] = {0, "VPROC2"},
	[VPROC1] = {0, "VPROC1"},
	[VGPU] = {0, "VGPU"},
	[VCORE] = {0, "VCORE"},
	[VDRAM] = {0, "VDRAM"},
	[VIO18_DRAM] = {0, "VIO18_DRAM"},
};

/****************************************************************************
 *  Local Variables
 ****************************************************************************/
static unsigned int swpm_log_mask = DEFAULT_LOG_MASK;
static char pwr_buf[POWER_CHAR_SIZE] = { 0 };

/* TODO: subsys index pointer */
static struct share_sub_index_ext *share_sub_idx_ref_ext;

/****************************************************************************
 *  Static Function
 ****************************************************************************/
static unsigned int swpm_get_avg_power(enum power_rail type)
{
	if (type >= NR_POWER_RAIL)
		pr_notice("Invalid SWPM type = %d\n", type);

	/* Remove the calculation of the power rail power and return 0 directly) */
	return 0;
}

void swpm_v6897_sub_ext_update(void)
{
/* TODO: do something from share_sub_idx_ref_ext */
	if (share_sub_idx_ref_ext)
		audio_subsys_index_update(&share_sub_idx_ref_ext->audio_idx_ext);
}

void swpm_v6897_sub_ext_init(void)
{
/* TODO: instance init of subsys index */
	if (wrap_d) {
		share_sub_idx_ref_ext =
			(struct share_sub_index_ext *)
			sspm_sbuf_get(wrap_d->share_index_sub_ext_addr);

		audio_subsys_index_init();
	} else {
		share_sub_idx_ref_ext = NULL;
	}
}

void swpm_v6897_power_log(void)
{
	char *ptr = pwr_buf;
	int i;

	for (i = 0; i < NR_POWER_RAIL; i++) {
		if ((1 << i) & swpm_log_mask) {
			ptr += scnprintf(ptr, 256, "%s/",
					swpm_power_rail[i].name);
		}
	}
	ptr--;
	ptr += sprintf(ptr, " = ");

	for (i = 0; i < NR_POWER_RAIL; i++) {
		if ((1 << i) & swpm_log_mask) {
			swpm_power_rail[i].avg_power =
				swpm_get_avg_power((enum power_rail)i);
			ptr += scnprintf(ptr, 256, "%d/",
					swpm_power_rail[i].avg_power);
		}
	}
	ptr--;
	ptr += sprintf(ptr, " uA");
}

