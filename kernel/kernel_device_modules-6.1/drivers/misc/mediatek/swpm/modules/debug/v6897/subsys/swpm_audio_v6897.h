/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2023 MediaTek Inc.
 */

#ifndef __SWPM_AUDIO_V6985_H__
#define __SWPM_AUDIO_V6985_H__

#include <linux/cpu.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/notifier.h>
#include <linux/timer.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <swpm_cpu_v6897.h>
#include <swpm_module_ext.h>

/*****************************************************************************
 *  Type Definiations
 *****************************************************************************/
enum audio_cmd_action {
	AUDIO_GET_SCENARIO,
};

struct audio_index_ext {
	/* s0 time */
	unsigned int s0_time;
	/* s1 time */
	unsigned int s1_time;
	/* MCUSYS active time*/
	unsigned int mcusys_active_time;
	/* MCUSYS power-down time*/
	unsigned int mcusys_pd_time;
	/* Cluster 0 active time*/
	unsigned int cluster_active_time;
	/* Cluster 0 pd time*/
	unsigned int cluster_pd_time;
	/* ADSP active time */
	unsigned int adsp_active_time;
	/* ADSP IDLE time */
	unsigned int adsp_wfi_time;
	/* ADSP CORE RST time */
	unsigned int adsp_core_rst_time;
	/* ADSP CORE OFF time */
	unsigned int adsp_core_off_time;
	/* ADSP power-down time */
	unsigned int adsp_pd_time;
	/* SCP active time */
	unsigned int scp_active_time;
	/* SCP WFI time */
	unsigned int scp_wfi_time;
	/* SCP cpu off time */
	unsigned int scp_cpu_off_time;
	/* SCP sleep time */
	unsigned int scp_sleep_time;
	/* SCP power off time */
	unsigned int scp_pd_time;
	/* CPU active time */
	unsigned int cpu_active_time[NR_CPU_CORE];
	/* cpu wfi time */
	unsigned int cpu_wfi_time[NR_CPU_CORE];
	/* cpu pd time */
	unsigned int cpu_pd_time[NR_CPU_CORE];
};

struct audio_index_duration {
	int64_t timestamp;
	int64_t s0_time;
	int64_t s1_time;
	int64_t mcusys_active_time;
	int64_t mcusys_pd_time;
	int64_t cluster_active_time;
	int64_t cluster_pd_time;
	int64_t adsp_active_time;
	int64_t adsp_wfi_time;
	int64_t adsp_core_rst_time;
	int64_t adsp_core_off_time;
	int64_t adsp_pd_time;
	int64_t scp_active_time;
	int64_t scp_wfi_time;
	int64_t scp_cpu_off_time;
	int64_t scp_sleep_time;
	int64_t scp_pd_time;
	int64_t cpu_active_time[NR_CPU_CORE];
	int64_t cpu_wfi_time[NR_CPU_CORE];
	int64_t cpu_pd_time[NR_CPU_CORE];
};

extern spinlock_t audio_swpm_spinlock;
extern void audio_subsys_index_init(void);
extern void audio_subsys_index_update(struct audio_index_ext *audio_idx_ext);
extern void get_audio_power_index(struct audio_index_duration *audio_idx_duration);
#endif
