// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include <linux/cpu.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/notifier.h>
#include <linux/perf_event.h>
#include <linux/spinlock.h>
#include <linux/timer.h>
#include <linux/workqueue.h>

#if IS_ENABLED(CONFIG_MTK_TINYSYS_SSPM_SUPPORT)
#include <sspm_reservedmem.h>
#endif

#if IS_ENABLED(CONFIG_MTK_THERMAL)
#include <thermal_interface.h>
#endif

#include <mtk_swpm_common_sysfs.h>
#include <mtk_swpm_sysfs.h>
#include <swpm_dbg_common_v1.h>
#include <swpm_module.h>
#include <swpm_v6989.h>
#include <swpm_mml_v6989.h>

#include <linux/pm_runtime.h>
#include <linux/platform_device.h>

#include "mtk-mml-driver.h"

/****************************************************************************
 *  Local Variables
 ****************************************************************************/
/* share sram for swpm cpu data */
static struct mml_swpm_data *mml_swpm_data_ptr;

void update_mml_wrot_status (u32 wrot0_sts, u32 wrot1_sts, u32 wrot2_sts, u32 wrot3_sts)
{
	mml_swpm_data_ptr->WROT0_status = wrot0_sts;
	mml_swpm_data_ptr->WROT1_status = wrot1_sts;
	mml_swpm_data_ptr->WROT2_status = wrot2_sts;
	mml_swpm_data_ptr->WROT3_status = wrot3_sts;
}

void update_mml_cg_status (u32 cg_con0, u32 cg_con1)
{
	mml_swpm_data_ptr->CG_CON0 = cg_con0;
	mml_swpm_data_ptr->CG_CON1 = cg_con1;
}

void update_mml_freq_status (u32 freq)
{
	mml_swpm_data_ptr->MML_freq = freq;
}

void update_mml_pq_status (u32 pq)
{
	mml_swpm_data_ptr->MML_pq = pq;
}

int swpm_mml_v6989_init(void)
{
	unsigned int offset_mml;
	struct mml_swpm_func *pMMLFunc;

	/* (args_0 = 0, args_1 = 0, action = GET_ADDR = 0) */
	offset_mml = swpm_set_and_get_cmd(0, 0, 0, MML_CMD_TYPE);

	mml_swpm_data_ptr = (struct mml_swpm_data *)
		sspm_sbuf_get(offset_mml);
	/* exception control for illegal sbuf request */
	if (!mml_swpm_data_ptr) {
		pr_notice("swpm mml share sram offset fail\n");
		return -1;
	}

	pr_notice("swpm init offset of mml = 0x%x\n", offset_mml);
	// swpm_register_event_notifier(&mml_swpm_notifier);

	/* Init function pointer for mml */
	pMMLFunc = mml_get_swpm_func();
	pMMLFunc->set_func = true;
	pMMLFunc->update_wrot = update_mml_wrot_status;
	pMMLFunc->update_cg = update_mml_cg_status;
	pMMLFunc->update_freq = update_mml_freq_status;
	pMMLFunc->update_pq = update_mml_pq_status;

	return 0;
}

void swpm_mml_v6989_exit(void)
{
	// swpm_unregister_event_notifier(&mml_swpm_notifier);
}
