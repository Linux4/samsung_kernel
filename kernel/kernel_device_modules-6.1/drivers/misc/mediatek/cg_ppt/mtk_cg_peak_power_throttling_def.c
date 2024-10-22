// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 MediaTek Inc.
 * Author: Clouds Lee <clouds.lee@mediatek.com>
 */

#include "mtk_cg_peak_power_throttling_def.h"

/*
 * ========================================================
 * Kernel
 * ========================================================
 */
#if defined(__KERNEL__)

uintptr_t THERMAL_CSRAM_BASE_REMAP;
uintptr_t THERMAL_CSRAM_CTRL_BASE_REMAP;
uintptr_t DLPT_CSRAM_BASE_REMAP;
uintptr_t DLPT_CSRAM_CTRL_BASE_REMAP;

void cg_ppt_thermal_sram_remap(uintptr_t virtual_addr)
{
	THERMAL_CSRAM_BASE_REMAP = virtual_addr;
	THERMAL_CSRAM_CTRL_BASE_REMAP = THERMAL_CSRAM_BASE_REMAP + 0x360;
}

void cg_ppt_dlpt_sram_remap(uintptr_t virtual_addr)
{
	DLPT_CSRAM_BASE_REMAP = virtual_addr;
	DLPT_CSRAM_CTRL_BASE_REMAP = DLPT_CSRAM_BASE_REMAP + DLPT_CSRAM_SIZE -
				     DLPT_CSRAM_CTRL_RESERVED_SIZE;
}

#endif /*__KERNEL__*/

/*
 * ========================================================
 * CPU SW Runner
 * ========================================================
 */
#if defined(CFG_CPU_PEAKPOWERTHROTTLING)
#endif /*CFG_GPU_PEAKPOWERTHROTTLING*/

/*
 * ========================================================
 * GPU SW Runner
 * ========================================================
 */
#if defined(CFG_GPU_PEAKPOWERTHROTTLING)
#include <FreeRTOS.h>
#include <task.h>

void ppt_critical_enter(void)
{
	if (!_is_in_isr())
		taskENTER_CRITICAL();
}

void ppt_critical_exit(void)
{
	if (!_is_in_isr())
		taskEXIT_CRITICAL();
}
#endif /*CFG_GPU_PEAKPOWERTHROTTLING*/

/*
 * ...................................
 * Thermal CSRAM Ctrl Block
 * ...................................
 */
struct ThermalCsramCtrlBlock *thermal_csram_ctrl_block_get(void)
{
	void *remap_status_base = (void *)THERMAL_CSRAM_CTRL_BASE_REMAP;

	return (struct ThermalCsramCtrlBlock *)remap_status_base;
}

void thermal_csram_ctrl_block_release(
	struct ThermalCsramCtrlBlock *remap_status_base)
{
}

/*
 * ...................................
 * DLTP CSRAM Ctrl Block
 * ...................................
 */
struct DlptCsramCtrlBlock *dlpt_csram_ctrl_block_get(void)
{
	void *remap_status_base = (void *)DLPT_CSRAM_CTRL_BASE_REMAP;

	return (struct DlptCsramCtrlBlock *)remap_status_base;
}

void dlpt_csram_ctrl_block_release(struct DlptCsramCtrlBlock *remap_status_base)
{
}
