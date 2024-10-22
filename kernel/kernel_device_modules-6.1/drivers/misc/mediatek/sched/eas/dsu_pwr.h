/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2022 MediaTek Inc.
 */

#ifndef _DSU_PWR_H
#define _DSU_PWR_H

/* SWPM */
#define L3_PERBW_PWR_A  (29/10)
#define L3_PERBW_PWR_B  (145/100)
#define CO_POINT    8
#define EMI_PERBW_PWR   (63/10)

/* CPU LEAKAGE SRAM */
/* a = bit 0 ~ 11, b = bit 12 ~ 31 */
#define LKG_BASE_OFFSET 0x240
#define LKG_TYPES_OFFSET 0x120
#define LKG_COEF_A_MASK 0xFFF
#define LKG_COEF_A_BIT_NUM 12
#define LKG_COEF_B_MASK 0xFFFFF

enum cpu_lkg_type {
	CPU_L_LKG,
	CPU_B_LKG,
	DSU_LKG,
	NR_CPU_LKG_TYPE
};

enum dsu_pwr_detail {
	DSU_DYN_PWR,
	DSU_LKG_PWR,
	MCU_DYN_PWR,
	DSU_PWR_TAL,
	RES_PWR
};

/* bw : 100 mb/s, temp : degree, freq : khz, volt : 10uv */
struct dsu_info {
	unsigned int dsu_bw;
	unsigned int emi_bw;
	int temp;
	unsigned int dsu_freq;
	unsigned int dsu_volt;
};

unsigned int predict_dsu_bw(int wl_type, int dst_cpu, unsigned long task_util,
		unsigned long total_util, unsigned int dsu_bw);
unsigned int predict_emi_bw(int wl_type, int dst_cpu, unsigned long task_util,
		unsigned long total_util, unsigned int emi_bw);
unsigned int dsu_dyn_pwr(int wl_type, struct dsu_info *p,
		unsigned int p_dsu_bw);
unsigned int dsu_lkg_pwr(int wl_type, struct dsu_info *p);
#ifdef SEPA_DSU_EMI
unsigned int mcusys_dyn_pwr(int wl_type, struct dsu_info *p,
		unsigned int p_emi_bw);
#endif
unsigned long get_dsu_pwr(int wl_type, int dst_cpu, unsigned long task_util,
		unsigned long total_util, struct dsu_info *dsu);
#endif
