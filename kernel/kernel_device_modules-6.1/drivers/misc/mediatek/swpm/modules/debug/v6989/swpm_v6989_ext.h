/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2023 MediaTek Inc.
 */

#ifndef __MTK_SWPM_SP_PLATFORM_H__
#define __MTK_SWPM_SP_PLATFORM_H__

#include <swpm_mem_v6989.h>
#include <swpm_core_v6989.h>

/* numbers of power state (active, idle, off) */
enum pmsr_power_state {
	PMSR_ACTIVE,
	PMSR_IDLE,
	PMSR_OFF,

	NR_POWER_STATE,
};
/* #define NR_POWER_STATE (3) */

enum xpu_ip_state {
	XPU_IP_DISP0,
	XPU_IP_DISP1,
	XPU_IP_VENC0,
	XPU_IP_VENC1,
	XPU_IP_VDEC0,
	XPU_IP_VDEC1,
	XPU_IP_SCP,
	XPU_IP_ADSP,
	XPU_IP_MCU,

	NR_XPU_IP,
};

/* ddr byte count ip (total read, total write, cpu, mm, gpu, others) */
enum ddr_bc_ip {
	DDR_BC_TOTAL_R,
	DDR_BC_TOTAL_W,
	DDR_BC_TOTAL_CPU,
	DDR_BC_TOTAL_GPU,
	DDR_BC_TOTAL_MM,
	DDR_BC_TOTAL_OTHERS,

	NR_DDR_BC_IP,
};
/* #define NR_DDR_BC_IP (6) */

enum spm_group {
	DDREN_REQ,
	APSRC_REQ,
	EMI_REQ,
	MAINPLL_REQ,
	INFRA_REQ,
	F26M_REQ,
	PMIC_REQ,
	VCORE_REQ,
	PWR_ACT,
	SYS_STA,

	NR_SPM_GRP,
};

/* core extension index structure */
struct core_index_ext {
	/* core voltage distribution (us) */
	unsigned int acc_time[NR_CORE_VOLT];
};

/* mem extension ip word count (1 word -> 8 bytes @ 64bits) */
struct mem_ip_bc {
	unsigned int word_cnt_L[NR_DDR_FREQ];
	unsigned int word_cnt_H[NR_DDR_FREQ];
};

/* dram extension structure */
struct mem_index_ext {
	/* dram freq in active state distribution (us) */
	unsigned int acc_time[NR_DDR_FREQ];

	/* dram in self-refresh state (us) */
	unsigned int acc_sr_time;

	/* dram in power-down state (us) */
	unsigned int acc_pd_time;

	/* dram ip byte count in freq distribution */
	struct mem_ip_bc data[NR_DDR_BC_IP];
};

struct xpu_ip_pwr_sta {
	unsigned int state[NR_POWER_STATE];
};

struct xpu_index_ext {
	/* xpu ip power state distribution */
	struct xpu_ip_pwr_sta pwr_state[NR_XPU_IP];
};

struct suspend_time {
	/* total suspended time H/L*/
	unsigned int time_L;
	unsigned int time_H;
};

struct duration_time {
	/* total duration time H/L*/
	unsigned int time_L;
	unsigned int time_H;
};

struct share_spm_sig {
	unsigned int spm_sig_addr;
	unsigned int spm_sig_num[NR_SPM_GRP];
	unsigned int win_len;
};

struct share_index_ext {
	struct core_index_ext core_idx_ext;
	struct mem_index_ext mem_idx_ext;
	struct xpu_index_ext xpu_idx_ext;
	struct suspend_time suspend;
	struct duration_time duration;

	/* last core volt index */
	unsigned int last_volt_idx;

	/* last ddr freq index */
	unsigned int last_freq_idx;
};

struct share_ctrl_ext {
	unsigned int read_lock;
	unsigned int write_lock;
	unsigned int clear_flag;
};

extern void swpm_v6989_ext_init(void);
extern void swpm_v6989_ext_exit(void);

#endif /* __MTK_SWPM_SP_PLATFORM_H__ */
