/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 * Author: Samuel Hsieh <samuel.hsieh@mediatek.com>
 */
#undef TRACE_SYSTEM
#define TRACE_SYSTEM mtk_ppb
#if !defined(_TRACE_MTK_PPB_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_MTK_PPB_H
#include <linux/tracepoint.h>
#include "mtk_peak_power_budget.h"
TRACE_EVENT(peak_power_budget,
	TP_PROTO(struct ppb *ppb),
	TP_ARGS(ppb),
	TP_STRUCT__entry(
		__field(unsigned int, loading_flash)
		__field(unsigned int, loading_audio)
		__field(unsigned int, loading_camera)
		__field(unsigned int, loading_display)
		__field(unsigned int, loading_apu)
		__field(unsigned int, loading_dram)
		__field(unsigned int, vsys_budget)
		__field(unsigned int, remain_budget)
	),
	TP_fast_assign(
		__entry->loading_flash = ppb->loading_flash;
		__entry->loading_audio = ppb->loading_audio;
		__entry->loading_camera = ppb->loading_camera;
		__entry->loading_display = ppb->loading_display;
		__entry->loading_apu = ppb->loading_apu;
		__entry->loading_dram = ppb->loading_dram;
		__entry->vsys_budget = ppb->vsys_budget;
		__entry->remain_budget = ppb->remain_budget;
	),
	TP_printk("(S_BGT/R_BGT)=%u,%u (FLASH/AUD/CAM/DISP/APU/DRAM)=%u,%u,%u,%u,%u,%u\n",
		__entry->vsys_budget, __entry->remain_budget,
		__entry->loading_flash, __entry->loading_audio, __entry->loading_camera,
		__entry->loading_display, __entry->loading_apu, __entry->loading_dram)
);
#endif /* _TRACE_MTK_PPB_H */
/* This part must be outside protection */
#undef TRACE_INCLUDE_PATH
#undef TRACE_INCLUDE_FILE
#define TRACE_INCLUDE_PATH ../../drivers/misc/mediatek/pbm
#define TRACE_INCLUDE_FILE mtk_peak_power_budget_trace
#include <trace/define_trace.h>
