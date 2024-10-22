// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2022 MediaTek Inc.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <asm/io.h>
#include <linux/cpufreq.h>
#include <linux/kthread.h>
#include <linux/irq_work.h>
#include "sugov/cpufreq.h"
#include "sugov/dsu_interface.h"
#include "dsu_pwr.h"
#include "sched_trace.h"

/* bml weighting and the predict way for dsu and emi may different */
unsigned int predict_dsu_bw(int wl_type, int dst_cpu, unsigned long task_util,
		unsigned long total_util, unsigned int dsu_bw)
{
	unsigned int bml_weighting;
	unsigned int ret = 0;

	/* pd_get_dsu_weighting return percentage */
	bml_weighting = pd_get_dsu_weighting(wl_type, dst_cpu);
	ret = dsu_bw * bml_weighting * task_util / total_util / 100;
	ret += dsu_bw;

	return ret;
}

unsigned int predict_emi_bw(int wl_type, int dst_cpu, unsigned long task_util,
		unsigned long total_util, unsigned int emi_bw)
{
	unsigned int bml_weighting;
	unsigned int ret = 0;

	bml_weighting = pd_get_emi_weighting(wl_type, dst_cpu);
	ret = emi_bw * bml_weighting * task_util / total_util / 100;
	ret += emi_bw;

	return ret;
}

unsigned int dsu_dyn_pwr(int wl_type, struct dsu_info *p, unsigned int p_dsu_bw)
{
	int dsu_opp = 0;
	int real_co_point;
	unsigned int golden_bw_pwr, real_bw_pwr, real_bw, old_bw;
	int pwr_delta;
	struct dsu_state *dsu_tbl;
	int perbw_pwr_a, perbw_pwr_b;
	unsigned int ret = 0;
	unsigned int volt_temp;

	real_bw = p_dsu_bw/10;/* to gb/s */
	dsu_tbl = dsu_get_opp_ps(wl_type, dsu_opp);
	old_bw = dsu_tbl->BW/10;/* to gb/s */
	real_co_point = CO_POINT * p->dsu_freq / 1000000;/* to ghz */

#ifdef SEPA_DSU_EMI
	perbw_pwr_a = L3_PERBW_PWR_A;
	perbw_pwr_b = L3_PERBW_PWR_B;
#else
	perbw_pwr_a = L3_PERBW_PWR_A + EMI_PERBW_PWR * p->emi_bw / p->dsu_bw;
	perbw_pwr_b = L3_PERBW_PWR_B + EMI_PERBW_PWR * p->emi_bw / p->dsu_bw;
#endif

	if (old_bw > real_co_point) {
		golden_bw_pwr = real_co_point * perbw_pwr_a + (old_bw -
				real_co_point) * perbw_pwr_b;
	} else {
		golden_bw_pwr = old_bw * perbw_pwr_a;
	}

	if (real_bw > real_co_point) {
		real_bw_pwr = real_co_point * perbw_pwr_a + (real_bw - real_co_point)
			* perbw_pwr_b;
	} else {
		real_bw_pwr = real_bw * perbw_pwr_a;
	}

	/* mw to uw */
	volt_temp = 1000 * p->dsu_volt/100000 * p->dsu_volt/100000;
	pwr_delta = volt_temp * (real_bw_pwr - golden_bw_pwr);

	ret = dsu_tbl->dyn_pwr + pwr_delta;

	return ret;

}

unsigned int dsu_lkg_pwr(int wl_type, struct dsu_info *p)
{
	int temperature;
	unsigned int coef_ab = 0, coef_a = 0, coef_b = 0, coef_c = 0, type, opp;
	void __iomem *clkg_sram_base_addr;

	clkg_sram_base_addr = get_clkg_sram_base_addr();
	temperature = p->temp;
	type = DSU_LKG;
	/* freq to opp for calculating offset */
	opp = dsu_get_freq_opp(p->dsu_freq);
	/* read coef from sysram, real value = value/10000 */
	coef_ab = ioread32(clkg_sram_base_addr + LKG_BASE_OFFSET +
			type * LKG_TYPES_OFFSET + opp * 8);
	coef_c = ioread32(clkg_sram_base_addr + LKG_BASE_OFFSET +
			type * LKG_TYPES_OFFSET + opp * 8 + 4);
	coef_a = coef_ab & LKG_COEF_A_MASK;
	coef_b = (coef_ab >> LKG_COEF_A_BIT_NUM) & LKG_COEF_B_MASK;

	return (temperature * (temperature * coef_a - coef_b) + coef_c)/10000 *
	   1000;/* uw */
}

#ifdef SEPA_DSU_EMI
unsigned int mcusys_dyn_pwr(int wl_type, struct dsu_info *p,
		unsigned int p_emi_bw)
{
	int dsu_opp = 0;
	unsigned int old_bw_pwr, old_bw, new_bw, new_bw_pwr;
	int pwr_delta;
	struct dsu_state *dsu_tbl;
	unsigned int volt_temp;

	dsu_tbl = dsu_get_opp_ps(wl_type, dsu_opp);
	old_bw = dsu_tbl->EMI_BW;/* 100mb/s */
	new_bw = p_emi_bw;/* 100mb/s */

	old_bw_pwr = EMI_PERBW_PWR * old_bw/10;
	new_bw_pwr = EMI_PERBW_PWR * new_bw/10;
	/* mw to uw */
	volt_temp = 1000 * p->dsu_volt/100000 * p->dsu_volt/100000;
	pwr_delta = volt_temp * (new_bw_pwr - old_bw_pwr);

	return dsu_tbl->mcusys_dyn_pwr + pwr_delta;/* uw */
}
#endif

/* bw : 100 mb/s, temp : degree, freq : khz, volt : 10uv */
unsigned long get_dsu_pwr(int wl_type, int dst_cpu, unsigned long task_util,
		unsigned long total_util, struct dsu_info *dsu)
{
	unsigned int dsu_pwr[RES_PWR];
	unsigned int p_dsu_bw, p_emi_bw; /* predict dsu and emi bw */
	int i;

	/* predict task bw */
	if (dst_cpu >= 0) {
		/* predict dsu bw */
		p_dsu_bw = predict_dsu_bw(wl_type, dst_cpu, task_util, total_util,
				dsu->dsu_bw);
		/* predict emi bw */
		p_emi_bw = predict_emi_bw(wl_type, dst_cpu, task_util, total_util,
				dsu->emi_bw);
	} else {
		p_dsu_bw = dsu->dsu_bw;
		p_emi_bw = dsu->emi_bw;
	}

	/*SWPM in uw */
	dsu_pwr[DSU_PWR_TAL] = 0;
	dsu_pwr[DSU_DYN_PWR] = dsu_dyn_pwr(wl_type, dsu, p_dsu_bw);
	dsu_pwr[DSU_LKG_PWR] = dsu_lkg_pwr(wl_type, dsu);
#ifdef SEPA_DSU_EMI
	dsu_pwr[MCU_DYN_PWR] = mcusys_dyn_pwr(wl_type, dsu, p_emi_bw);
#else
	dsu_pwr[MCU_DYN_PWR] = 0;
#endif
	for (i = DSU_DYN_PWR; i < DSU_PWR_TAL ; i++)
		dsu_pwr[DSU_PWR_TAL] += dsu_pwr[i];

	if (trace_dsu_pwr_cal_enabled()) {
		trace_dsu_pwr_cal(dst_cpu, task_util, total_util, dsu->dsu_bw,
				dsu->emi_bw, dsu->temp, dsu->dsu_freq, dsu->dsu_volt,
				dsu_pwr[DSU_DYN_PWR], dsu_pwr[DSU_LKG_PWR],
				dsu_pwr[MCU_DYN_PWR], dsu_pwr[DSU_PWR_TAL]);
	}

	return dsu_pwr[DSU_PWR_TAL];
}
