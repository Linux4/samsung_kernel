/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 * Author: Clouds Lee <clouds.lee@mediatek.com>
 */

#undef TRACE_SYSTEM
#define TRACE_SYSTEM cg_ppt

#if !defined(_TRACE_CG_PPT__H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_CG_PPT__H

#include <linux/tracepoint.h>

#ifndef __TRACE_CG_PPT_STRUCT__
#define __TRACE_CG_PPT_STRUCT__

struct cg_ppt_status_info {
	// ThermalCsramCtrlBlock
	int cpu_low_key;
	int g2c_pp_lmt_freq_ack_timeout;
	// DlptSramLayout->cswruninfo
	int cg_sync_enable;
	int is_fastdvfs_enabled;
	// DlptSramLayout->gswruninfo
	int gpu_preboost_time_us;
	int cgsync_action;
	int is_gpu_favor;
	int combo_idx;
	// DlptCsramCtrlBlock
	int peak_power_budget_mode;
	// DlptSramLayout->mo_info
	unsigned int mo_status;
};

struct cg_ppt_freq_info {
	//ThermalCsramCtrlBlock
	int g2c_b_pp_lmt_freq;
	int g2c_b_pp_lmt_freq_ack;
	int g2c_m_pp_lmt_freq;
	int g2c_m_pp_lmt_freq_ack;
	int g2c_l_pp_lmt_freq;
	int g2c_l_pp_lmt_freq_ack;
	// DlptSramLayout->gswruninfo
	int gpu_limit_freq_m;
};

struct cg_ppt_power_info {
	// DlptSramLayout->gswruninfo
	int cgppb_mw;
	//DlptCsramCtrlBlock
	int cg_min_power_mw;
	int vsys_power_budget_mw; /*include modem, wifi*/
	int vsys_power_budget_ack;
	int flash_peak_power_mw;
	int audio_peak_power_mw;
	int camera_peak_power_mw;
	int apu_peak_power_mw;
	int display_lcd_peak_power_mw;
	int dram_peak_power_mw;
	/*shadow mem*/
	int modem_peak_power_mw_shadow;
	int wifi_peak_power_mw_shadow;
	/*misc*/
	int apu_peak_power_ack;
};

struct cg_ppt_combo_info {
	// DlptSramLayout->gswruninfo
	int cgppb_mw;

	int gpu_combo0;
	int gpu_combo1;
	int gpu_combo2;
	int gpu_combo3;
	int gpu_combo4;

	int cpu_combo0;
	int cpu_combo1;
	int cpu_combo2;
	int cpu_combo3;
	int cpu_combo4;
};

struct cg_sm_info {
	int gacboost_mode;
	int gacboost_hint;
	int gf_ema_1;
	int gf_ema_2;
	int gf_ema_3;
	int gt_ema_1;
	int gt_ema_2;
	int gt_ema_3;
};

#endif /*__TRACE_CG_PPT_STRUCT__*/

TRACE_EVENT(
	cg_ppt_status_info, TP_PROTO(const struct cg_ppt_status_info *data),
	TP_ARGS(data),
	TP_STRUCT__entry(
		__field(int, cpu_low_key)
		__field(int, g2c_pp_lmt_freq_ack_timeout)
		__field(int, cg_sync_enable)
		__field(int, is_fastdvfs_enabled)
		__field(int, gpu_preboost_time_us)
		__field(int, cgsync_action)
		__field(int, is_gpu_favor)
		__field(int, combo_idx)
		__field(int, peak_power_budget_mode)
		__field(unsigned int, mo_status)),
	TP_fast_assign(
	__entry->cpu_low_key = data->cpu_low_key;
	__entry->g2c_pp_lmt_freq_ack_timeout =
		data->g2c_pp_lmt_freq_ack_timeout;
	__entry->cg_sync_enable = data->cg_sync_enable;
	__entry->is_fastdvfs_enabled = data->is_fastdvfs_enabled;
	__entry->gpu_preboost_time_us = data->gpu_preboost_time_us;
	__entry->cgsync_action = data->cgsync_action;
	__entry->is_gpu_favor = data->is_gpu_favor;
	__entry->combo_idx = data->combo_idx;
	__entry->peak_power_budget_mode = data->peak_power_budget_mode;
	__entry->mo_status = data->mo_status;),
	TP_printk("cpu_low_key=%d g2c_pp_lmt_freq_ack_timeout=%d cg_sync_enable=%d is_fastdvfs_enabled=%d gpu_preboost_time_us=%d cgsync_action=%d is_gpu_favor=%d combo_idx=%d peak_power_budget_mode=%d mo_status=%u",
		  __entry->cpu_low_key, __entry->g2c_pp_lmt_freq_ack_timeout,
		  __entry->cg_sync_enable, __entry->is_fastdvfs_enabled,
		  __entry->gpu_preboost_time_us, __entry->cgsync_action,
		  __entry->is_gpu_favor, __entry->combo_idx,
		  __entry->peak_power_budget_mode,
		  __entry->mo_status));

TRACE_EVENT(
	cg_ppt_freq_info, TP_PROTO(const struct cg_ppt_freq_info *data),
	TP_ARGS(data),
	TP_STRUCT__entry(__field(int, g2c_b_pp_lmt_freq) __field(
		int, g2c_b_pp_lmt_freq_ack) __field(int, g2c_m_pp_lmt_freq)
				 __field(int, g2c_m_pp_lmt_freq_ack) __field(
					 int, g2c_l_pp_lmt_freq)
					 __field(int, g2c_l_pp_lmt_freq_ack)
						 __field(int,
							 gpu_limit_freq_m)),
	TP_fast_assign(
		__entry->g2c_b_pp_lmt_freq = data->g2c_b_pp_lmt_freq;
		__entry->g2c_b_pp_lmt_freq_ack = data->g2c_b_pp_lmt_freq_ack;
		__entry->g2c_m_pp_lmt_freq = data->g2c_m_pp_lmt_freq;
		__entry->g2c_m_pp_lmt_freq_ack = data->g2c_m_pp_lmt_freq_ack;
		__entry->g2c_l_pp_lmt_freq = data->g2c_l_pp_lmt_freq;
		__entry->g2c_l_pp_lmt_freq_ack = data->g2c_l_pp_lmt_freq_ack;
		__entry->gpu_limit_freq_m = data->gpu_limit_freq_m;),
	TP_printk("g2c_b_pp_lmt_freq=%d g2c_b_pp_lmt_freq_ack=%d g2c_m_pp_lmt_freq=%d g2c_m_pp_lmt_freq_ack=%d g2c_l_pp_lmt_freq=%d g2c_l_pp_lmt_freq_ack=%d gpu_limit_freq_m=%d",
		  __entry->g2c_b_pp_lmt_freq, __entry->g2c_b_pp_lmt_freq_ack,
		  __entry->g2c_m_pp_lmt_freq, __entry->g2c_m_pp_lmt_freq_ack,
		  __entry->g2c_l_pp_lmt_freq, __entry->g2c_l_pp_lmt_freq_ack,
		  __entry->gpu_limit_freq_m));

TRACE_EVENT(
	cg_ppt_power_info, TP_PROTO(const struct cg_ppt_power_info *data),
	TP_ARGS(data),
	TP_STRUCT__entry(
		__field(int, cgppb_mw)
		__field(int, cg_min_power_mw)
		__field(int, vsys_power_budget_mw)
		__field(int, vsys_power_budget_ack)
		__field(int, flash_peak_power_mw)
		__field(int, audio_peak_power_mw)
		__field(int, camera_peak_power_mw)
		__field(int, apu_peak_power_mw)
		__field(int, display_lcd_peak_power_mw)
		__field(int, dram_peak_power_mw)
		__field(int, modem_peak_power_mw_shadow)
		__field(int, wifi_peak_power_mw_shadow)
		__field(int, apu_peak_power_ack)),
	TP_fast_assign(
		__entry->cgppb_mw = data->cgppb_mw;
		__entry->cg_min_power_mw = data->cg_min_power_mw;
		__entry->vsys_power_budget_mw = data->vsys_power_budget_mw;
		__entry->vsys_power_budget_ack = data->vsys_power_budget_ack;
		__entry->flash_peak_power_mw = data->flash_peak_power_mw;
		__entry->audio_peak_power_mw = data->audio_peak_power_mw;
		__entry->camera_peak_power_mw = data->camera_peak_power_mw;
		__entry->apu_peak_power_mw = data->apu_peak_power_mw;
		__entry->display_lcd_peak_power_mw =
			data->display_lcd_peak_power_mw;
		__entry->dram_peak_power_mw = data->dram_peak_power_mw;
		__entry->modem_peak_power_mw_shadow =
			data->modem_peak_power_mw_shadow;
		__entry->wifi_peak_power_mw_shadow =
			data->wifi_peak_power_mw_shadow;
		__entry->apu_peak_power_ack = data->apu_peak_power_ack;),
	TP_printk("cgppb_mw=%d cg_min_power_mw=%d vsys_power_budget_mw=%d vsys_power_budget_ack=%d flash_peak_power_mw=%d audio_peak_power_mw=%d camera_peak_power_mw=%d apu_peak_power_mw=%d display_lcd_peak_power_mw=%d dram_peak_power_mw=%d modem_peak_power_mw_shadow=%d wifi_peak_power_mw_shadow=%d apu_peak_power_ack=%d",
		  __entry->cgppb_mw, __entry->cg_min_power_mw,
		  __entry->vsys_power_budget_mw, __entry->vsys_power_budget_ack,
		  __entry->flash_peak_power_mw, __entry->audio_peak_power_mw,
		  __entry->camera_peak_power_mw, __entry->apu_peak_power_mw,
		  __entry->display_lcd_peak_power_mw,
		  __entry->dram_peak_power_mw,
		  __entry->modem_peak_power_mw_shadow,
		  __entry->wifi_peak_power_mw_shadow,
		  __entry->apu_peak_power_ack));


TRACE_EVENT(cg_ppt_combo_info,
	TP_PROTO(const struct cg_ppt_combo_info *data),
	TP_ARGS(data),
	TP_STRUCT__entry(
		__field(int, cgppb_mw)
		__field(int, gpu_combo0)
		__field(int, gpu_combo1)
		__field(int, gpu_combo2)
		__field(int, gpu_combo3)
		__field(int, gpu_combo4)
		__field(int, cpu_combo0)
		__field(int, cpu_combo1)
		__field(int, cpu_combo2)
		__field(int, cpu_combo3)
		__field(int, cpu_combo4)
	),
	TP_fast_assign(
		__entry->cgppb_mw = data->cgppb_mw;
		__entry->gpu_combo0 = data->gpu_combo0;
		__entry->gpu_combo1 = data->gpu_combo1;
		__entry->gpu_combo2 = data->gpu_combo2;
		__entry->gpu_combo3 = data->gpu_combo3;
		__entry->gpu_combo4 = data->gpu_combo4;
		__entry->cpu_combo0 = data->cpu_combo0;
		__entry->cpu_combo1 = data->cpu_combo1;
		__entry->cpu_combo2 = data->cpu_combo2;
		__entry->cpu_combo3 = data->cpu_combo3;
		__entry->cpu_combo4 = data->cpu_combo4;
	),
	TP_printk("cgppb_mw=%d gpu_combo0=%d gpu_combo1=%d gpu_combo2=%d gpu_combo3=%d gpu_combo4=%d cpu_combo0=%d cpu_combo1=%d cpu_combo2=%d cpu_combo3=%d cpu_combo4=%d",
	__entry->cgppb_mw,
	__entry->gpu_combo0,
	__entry->gpu_combo1,
	__entry->gpu_combo2,
	__entry->gpu_combo3,
	__entry->gpu_combo4,
	__entry->cpu_combo0,
	__entry->cpu_combo1,
	__entry->cpu_combo2,
	__entry->cpu_combo3,
	__entry->cpu_combo4)
);


TRACE_EVENT(cg_sm_info,
	TP_PROTO(const struct cg_sm_info *data),
	TP_ARGS(data),
	TP_STRUCT__entry(
		__field(int, gacboost_mode)
		__field(int, gacboost_hint)
		__field(int, gf_ema_1)
		__field(int, gf_ema_2)
		__field(int, gf_ema_3)
		__field(int, gt_ema_1)
		__field(int, gt_ema_2)
		__field(int, gt_ema_3)
	),
	TP_fast_assign(
		__entry->gacboost_mode                 = data->gacboost_mode;
		__entry->gacboost_hint                 = data->gacboost_hint;
		__entry->gf_ema_1                      = data->gf_ema_1;
		__entry->gf_ema_2                      = data->gf_ema_2;
		__entry->gf_ema_3                      = data->gf_ema_3;
		__entry->gt_ema_1                      = data->gt_ema_1;
		__entry->gt_ema_2                      = data->gt_ema_2;
		__entry->gt_ema_3                      = data->gt_ema_3;
	),
	TP_printk("gacboost_mode=%d gacboost_hint=%d gf_ema_1=%d gf_ema_2=%d gf_ema_3=%d gt_ema_1=%d gt_ema_2=%d gt_ema_3=%d",
	__entry->gacboost_mode,
	__entry->gacboost_hint,
	__entry->gf_ema_1,
	__entry->gf_ema_2,
	__entry->gf_ema_3,
	__entry->gt_ema_1,
	__entry->gt_ema_2,
	__entry->gt_ema_3)
);


#endif /* _TRACE_CG_PPT__H */

/* This part must be outside protection */
#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH .
#undef TRACE_INCLUDE_FILE
#define TRACE_INCLUDE_FILE mtk_cg_peak_power_throttling_trace
#include <trace/define_trace.h>
