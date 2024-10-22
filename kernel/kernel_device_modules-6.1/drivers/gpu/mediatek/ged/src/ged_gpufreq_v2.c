// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */
#include <linux/types.h>
#include <linux/module.h>
#include <linux/random.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <gpufreq_v2.h>
#include <ged_base.h>
#include <ged_dcs.h>
#include <ged_dvfs.h>
#include <ged_global.h>
#include <ged_eb.h>
#include "ged_log.h"
#include <mt-plat/mtk_gpu_utility.h>

static int g_max_core_num;             /* core_num */
static int g_avail_mask_num;           /* mask_num */

static int g_working_oppnum;           /* opp_num */
static int g_virtual_oppnum;           /* opp_num + mask_num - 1 */
static int g_max_working_oppidx;       /* 0 */
static int g_min_working_oppidx;       /* opp_num - 1 */
static int g_min_virtual_oppidx;       /* opp_num - 1 + mask_num -1 */
/* the largest oppidx whose stack freq does not repeat */
static int g_min_stack_oppidx;
/* virtual table's oppnum when g_async_virtual_table_support */
static int g_virtual_async_oppnum;
/* top/stack ratio at min stack freq, Ex: 1.5, 1.0 */
static u32 *g_async_ratio_low;

static int g_max_freq_in_mhz;         /* max freq in opp table */

static unsigned int g_ud_mask_bit;     /* user-defined available core mask*/
static int g_stress_mask_idx;          /* dcs stress test */
static struct mutex g_ud_DCS_lock;

/**
 * g_oppnum_eachmask: opp num for each core mask, Ex: 2 opp for MC6
 * g_min_async_oppnum: Additional added opp num at min stack freq
 * g_async_ratio_support: Enable async ratio feature by dts.
 * g_async_virtual_table_support: create async ratio's own virtual table
 * g_cur_oppidx: save oppidx when commit
 */
static int g_oppnum_eachmask = 1;
static int g_min_async_oppnum;
static int g_async_ratio_support;
static int g_async_virtual_table_support;
static int g_cur_oppidx;

#define RATIO_SCAL 100

struct ged_opp_info {
	unsigned int topFreq;           /* KHz */
	unsigned int scFreq;            /* KHz */
	unsigned int scRealOpp;
	unsigned int topRealOpp;
};

struct gpufreq_core_mask_info *g_mask_table;	// DCS table
struct gpufreq_opp_info *g_working_table;		// real opp table from eb
struct gpufreq_opp_info *g_virtual_table;		// real opp table + DCS opp [stack]
struct gpufreq_opp_info *g_virtual_top_table;	// real opp table + DCS opp [top]
struct ged_opp_info *g_virtual_async_table;		// async ratio's own opp table [top & stack]


static int ged_get_top_idx_by_freq(int gpu_freq_tar, int minfreq_idx)
{
	for (int i = 0; i <= minfreq_idx; i++) {
		int gpu_freq = gpufreq_get_freq_by_idx(TARGET_GPU, i);

		if (gpu_freq_tar > gpu_freq) {
			if (i == 0)
				return 0;
			else
				return i-1;
			break;
		}
	}
	return minfreq_idx;
}

GED_ERROR ged_gpufreq_init(void)
{
	int i, j, k = 0;
	int min_freq, freq_scale = 0;
	unsigned int core_num = 0;
	const struct gpufreq_opp_info *opp_table;
	const struct gpufreq_opp_info *opp_top_table;
	struct gpufreq_core_mask_info *core_mask_table;
	struct device_node *async_dvfs_node = NULL;		// for dts
	/* top/stack ratio for 1-on-1 mapping (w/o min stack freq), Ex: 2 */
	int async_ratio_all;

	if (gpufreq_bringup())
		return GED_OK;

	GED_LOGI("%s: start to init GPU Freq\n", __func__);

	/* init gpu opp table */
	g_working_oppnum = gpufreq_get_opp_num(TARGET_DEFAULT) > 0 ?
						gpufreq_get_opp_num(TARGET_DEFAULT) : 1;
	g_min_working_oppidx = g_working_oppnum - 1;
	g_max_working_oppidx = 0;
	g_max_freq_in_mhz = gpufreq_get_freq_by_idx(TARGET_DEFAULT, 0) / 1000;
	g_min_stack_oppidx = g_min_working_oppidx;
	while (g_min_stack_oppidx - 1 > 0 &&
		gpufreq_get_freq_by_idx(TARGET_DEFAULT, g_min_stack_oppidx) ==
		gpufreq_get_freq_by_idx(TARGET_DEFAULT, g_min_stack_oppidx - 1)) {
		g_min_stack_oppidx--;
	}

	opp_table  = gpufreq_get_working_table(TARGET_DEFAULT);
	opp_top_table  = gpufreq_get_working_table(TARGET_GPU);
	g_working_table = kcalloc(g_working_oppnum,
						sizeof(struct gpufreq_opp_info), GFP_KERNEL);

	if (!g_working_table || !opp_table) {
		GED_LOGE("%s: Failed to init opp table", __func__);
		return GED_ERROR_FAIL;
	}

	for (i = 0; i < g_working_oppnum; i++)
		*(g_working_table + i) = *(opp_table + i);

	for (i = 0; i < g_working_oppnum; i++) {
		GED_LOGD("[%02d*] Freq: %d, Volt: %d, Vsram: %d",
			i, g_working_table[i].freq, g_working_table[i].volt,
			g_working_table[i].vsram);
	}

	/* init core mask table if support DCS policy*/
	mutex_init(&g_ud_DCS_lock);
	core_mask_table = dcs_get_avail_mask_table();
	g_max_core_num = dcs_get_max_core_num();
	g_avail_mask_num = dcs_get_avail_mask_num();

	async_dvfs_node = of_find_compatible_node(NULL, NULL, "mediatek,gpu_async_ratio");
	if (unlikely(!async_dvfs_node)) {
		GED_LOGE("Failed to find async_dvfs_node");
	} else {
		of_property_read_u32(async_dvfs_node, "async-ratio-support",
							&g_async_ratio_support);
		of_property_read_u32(async_dvfs_node, "async-virtual-table-support",
							&g_async_virtual_table_support);
		if (g_async_ratio_support) {
			of_property_read_u32(async_dvfs_node, "async-oppnum-eachmask",
								&g_oppnum_eachmask);
			of_property_read_u32(async_dvfs_node, "async-oppnum-low",
								&g_min_async_oppnum);
		}
		if (g_async_virtual_table_support) {
			g_async_ratio_low = kcalloc(g_min_async_oppnum, sizeof(u32), GFP_KERNEL);
			of_property_read_u32(async_dvfs_node, "async-ratio", &async_ratio_all);
			of_property_read_u32_array(async_dvfs_node, "async-ratio-low",
						g_async_ratio_low, g_min_async_oppnum);
		}
		// check dts setting is reasonable
		if (g_async_ratio_support &&
			!g_async_virtual_table_support &&
			g_min_stack_oppidx + g_min_async_oppnum != g_min_working_oppidx)
			GED_LOGE(
			"[DVFS_ASYNC] There are some error in DTS setting. async-oppnum-low(%d) mismatch opp table(g_min_working_oppidx %d)",
			g_min_async_oppnum, g_min_working_oppidx);
		if (g_async_ratio_support &&
			g_async_virtual_table_support && g_async_ratio_low) {
			for (i = 0; i < g_min_async_oppnum; i++) {
				if (!g_async_ratio_low[i])
					GED_LOGE(
						"[DVFS_ASYNC] g_async_ratio_low[%d] = %u is unreasonable",
						i, g_async_ratio_low[i]);
			}
		}
	}

	if (g_max_core_num > 0) {
		g_ud_mask_bit = ((1 << (g_max_core_num - 1)) - 1);
		if (DCS_DEFAULT_MIN_CORE > 0)
			g_ud_mask_bit &= (1 << (DCS_DEFAULT_MIN_CORE - 1));
		GED_LOGI("default g_ud_mask_bit: %x", g_ud_mask_bit);
	}

	g_mask_table = kcalloc(g_avail_mask_num,
					sizeof(struct gpufreq_core_mask_info), GFP_KERNEL);

	for (i = 0; i < g_avail_mask_num; i++)
		*(g_mask_table + i) = *(core_mask_table + i);

	if (!core_mask_table || !g_mask_table) {
		GED_LOGE("%s: Failed to init core mask table", __func__);
		return GED_OK;
	}

	for (i = 0; i < g_avail_mask_num; i++) {
		GED_LOGD("[%02d*] MC0%d : 0x%llX",
			i, g_mask_table[i].num, g_mask_table[i].mask);
	}

	/* init virtual opp table by core mask table */
	g_virtual_oppnum = g_working_oppnum + (g_avail_mask_num - 1) * g_oppnum_eachmask;
	g_min_virtual_oppidx = g_virtual_oppnum - 1;
	g_virtual_async_oppnum = (g_min_stack_oppidx + 1) + g_min_async_oppnum +
							(g_avail_mask_num - 1) * g_oppnum_eachmask;
	GED_LOGI(
			"[DVFS_ASYNC] %s: g_async_ratio_support(%d) g_async_virtual_table_support(%d) g_oppnum_eachmask(%d) g_min_async_oppnum(%d) async_ratio_all(%d) g_virtual_async_oppnum(%d) g_min_stack_oppidx(%d)",
			__func__, g_async_ratio_support, g_async_virtual_table_support,
			g_oppnum_eachmask, g_min_async_oppnum, async_ratio_all,
			g_virtual_async_oppnum,	g_min_stack_oppidx);
	if (g_async_virtual_table_support && g_async_ratio_low) {
		for (i = 0; i < g_min_async_oppnum; i++)
			GED_LOGI("[DVFS_ASYNC] g_async_ratio_low[%d] = %u",
							i, g_async_ratio_low[i]);
	}

	g_virtual_table = kcalloc(g_virtual_oppnum,
						sizeof(struct gpufreq_opp_info), GFP_KERNEL);

	if (!g_mask_table || !g_virtual_table) {
		GED_LOGE("%s Failed to init virtual opp table", __func__);
		return GED_ERROR_FAIL;
	}
	for (i = 0; i < g_working_oppnum; i++)
		*(g_virtual_table + i) = *(opp_table + i);

	/*Async ratio*/
	if (g_async_virtual_table_support) {
		g_virtual_async_table = kcalloc(g_virtual_async_oppnum,
							sizeof(struct ged_opp_info), GFP_KERNEL);

		if (!g_mask_table || !g_virtual_async_table) {
			GED_LOGE("%s Failed to init virtual async opp table", __func__);
			return GED_ERROR_FAIL;
		}
		if (!opp_table || !g_async_ratio_low) {
			GED_LOGE("%s opp_table or async_ratio_low is null", __func__);
			return GED_ERROR_FAIL;
		}

		// consider opp 0-g_min_stack_oppidx with top/stack ratio - async_ratio_all
		for (i = 0; i <= g_min_stack_oppidx; i++) {
			g_virtual_async_table[i].scRealOpp = i;
			g_virtual_async_table[i].scFreq = opp_table[i].freq;
			g_virtual_async_table[i].topRealOpp = ged_get_top_idx_by_freq(
				g_virtual_async_table[i].scFreq * async_ratio_all / RATIO_SCAL,
				g_min_working_oppidx);
			g_virtual_async_table[i].topFreq = gpufreq_get_freq_by_idx(
					TARGET_GPU, g_virtual_async_table[i].topRealOpp);
		}

		// consider additional opp with top/stack ratio from g_async_ratio_low
		for (i = 0; i < g_min_async_oppnum; i++) {
			j = g_min_stack_oppidx + 1 + i;
			g_virtual_async_table[j].scRealOpp = j;
			g_virtual_async_table[j].scFreq = opp_table[g_min_working_oppidx].freq;
			g_virtual_async_table[j].topRealOpp = ged_get_top_idx_by_freq(
				g_virtual_async_table[j].scFreq * g_async_ratio_low[i] / RATIO_SCAL,
				g_min_working_oppidx);
			g_virtual_async_table[j].topFreq = gpufreq_get_freq_by_idx(
					TARGET_GPU, g_virtual_async_table[j].topRealOpp);
		}

		// consider DCS opp
		min_freq = gpufreq_get_freq_by_idx(TARGET_DEFAULT, g_min_stack_oppidx);
		for (i = g_min_stack_oppidx + g_min_async_oppnum + 1;
			i < g_virtual_async_oppnum; i++) {
			j = (i - g_min_stack_oppidx - g_min_async_oppnum - 1) /
				g_oppnum_eachmask + 1;
			freq_scale = min_freq * g_mask_table[j].num / g_max_core_num;

			g_virtual_async_table[i].scRealOpp =
					g_virtual_async_table[i - g_oppnum_eachmask].scRealOpp;
			g_virtual_async_table[i].scFreq = freq_scale;
			g_virtual_async_table[i].topRealOpp =
					g_virtual_async_table[i - g_oppnum_eachmask].topRealOpp;
			g_virtual_async_table[i].topFreq =
					g_virtual_async_table[i - j * g_oppnum_eachmask].topFreq;
		}

		for (i = 0; i < g_virtual_async_oppnum; i++) {
			GED_LOGD_IF(g_default_log_level,
				"[DVFS_ASYNC][%02d*] topFreq: %d, topRealOpp: %d, scFreq: %d, scRealOpp: %d",
				i, g_virtual_async_table[i].topFreq,
				g_virtual_async_table[i].topRealOpp,
				g_virtual_async_table[i].scFreq,
				g_virtual_async_table[i].scRealOpp);
		}
	} else if (g_async_ratio_support) {
		g_virtual_top_table = kcalloc(g_virtual_oppnum,
						sizeof(struct gpufreq_opp_info), GFP_KERNEL);

		if (!g_mask_table || !g_virtual_top_table) {
			GED_LOGE("%s Failed to init virtual top opp table", __func__);
			return GED_ERROR_FAIL;
		}
		if (!opp_top_table) {
			GED_LOGE("%s opp_top_table is null", __func__);
			return GED_ERROR_FAIL;
		}

		for (i = 0; i < g_working_oppnum; i++)
			*(g_virtual_top_table + i) = *(opp_top_table + i);

		/* construct virtual opp from real freq and core num */
		for (i = g_working_oppnum ; i < g_virtual_oppnum; i++) {
			j = (i - g_working_oppnum) / g_oppnum_eachmask + 1;
			k = (i - g_working_oppnum) % g_oppnum_eachmask;

			freq_scale = min_freq * g_mask_table[j].num / g_max_core_num;

			g_virtual_top_table[i].freq =
				gpufreq_get_freq_by_idx(TARGET_GPU, i - g_oppnum_eachmask);
			g_virtual_top_table[i].volt = k;
			g_virtual_top_table[i].vsram = 0;
		}
	}

	min_freq = gpufreq_get_freq_by_idx(TARGET_DEFAULT, g_min_working_oppidx);

	/* construct virtual opp from real freq and core num */
	for (i = g_working_oppnum ; i < g_virtual_oppnum; i++) {
		j = (i - g_working_oppnum) / g_oppnum_eachmask + 1;

		freq_scale = min_freq * g_mask_table[j].num / g_max_core_num;

		g_virtual_table[i].freq =  freq_scale;
		g_virtual_table[i].volt = 0;
		g_virtual_table[i].vsram = 0;
	}

	for (i = 0; i < g_virtual_oppnum; i++) {
		GED_LOGD("[%02d*] Freq: %d, Volt: %d, Vsram: %d",
			i, g_virtual_table[i].freq, g_virtual_table[i].volt,
			g_virtual_table[i].vsram);
	}

	return GED_OK;
}

void ged_gpufreq_exit(void)
{
	mutex_destroy(&g_ud_DCS_lock);
	kfree(g_working_table);
	kfree(g_virtual_table);
	kfree(g_mask_table);
	kfree(g_virtual_async_table);
	kfree(g_virtual_top_table);
	kfree(g_async_ratio_low);
}

unsigned int ged_get_cur_freq(void)
{
	unsigned int freq = 0;
	unsigned int core_num = 0;

	freq = gpufreq_get_cur_freq(TARGET_DEFAULT);

	if (!is_dcs_enable())
		return freq;

	core_num = dcs_get_cur_core_num();

	if (core_num < g_max_core_num)
		freq = freq * core_num / g_max_core_num;

	return freq;
}

int ged_get_min_oppidx(void)
{
	if (is_dcs_enable() && g_async_virtual_table_support)
		return g_virtual_async_oppnum - 1;

	if (is_dcs_enable() && g_min_virtual_oppidx)
		return g_min_virtual_oppidx;

	if (g_min_working_oppidx)
		return g_min_working_oppidx;
	else
		return gpufreq_get_opp_num(TARGET_DEFAULT) - 1;
}

unsigned int ged_get_freq_by_idx(int oppidx)
{
	if (oppidx < 0)
		return 0;

	if (is_dcs_enable() && g_virtual_table)
		return g_virtual_table[oppidx].freq;

	if (g_working_table == NULL)
		return gpufreq_get_freq_by_idx(TARGET_DEFAULT, oppidx);

	if (unlikely(oppidx > g_min_working_oppidx))
		oppidx = g_min_working_oppidx;

	return g_working_table[oppidx].freq;
}
int ged_get_sc_freq_by_virt_opp(int oppidx)
{
	// check oppidx is valid
	if (oppidx < 0)
		return 0;

	if (is_dcs_enable() && g_virtual_async_table)
		return g_virtual_async_table[oppidx].scFreq;

	return ged_get_freq_by_idx(oppidx);
}

int ged_get_top_freq_by_virt_opp(int oppidx)
{
	// check oppidx is valid
	if (oppidx < 0)
		return 0;

	if (is_dcs_enable() && g_virtual_async_table)
		return g_virtual_async_table[oppidx].topFreq;

	if (is_dcs_enable() && g_virtual_top_table)
		return g_virtual_top_table[oppidx].freq;

	return gpufreq_get_freq_by_idx(TARGET_GPU, oppidx);
}

unsigned int ged_get_cur_top_freq(void)
{
	unsigned int cur_minfreq = ged_get_freq_by_idx(ged_get_min_oppidx());
	unsigned int cur_top_freq = gpufreq_get_cur_freq(TARGET_GPU);

	//if GPU power off the freq is 26M ,the current oppidx is last oppidx. So get the freq
	if (cur_top_freq < cur_minfreq)
		return ged_get_top_freq_by_virt_opp(gpufreq_get_cur_oppidx(TARGET_GPU));
	else
		return cur_top_freq;
}

unsigned int ged_get_cur_stack_freq(void)
{
	unsigned int cur_minfreq = ged_get_freq_by_idx(ged_get_min_oppidx());
	unsigned int core_num = 0;
	int i = 0;
	int oppidx = 0;
	unsigned int cur_stack_freq = ged_get_cur_freq();

	//if GPU power off the freq is 26M ,the current oppidx is last oppidx. So get the freq
	if ( cur_stack_freq < cur_minfreq) {

		oppidx = gpufreq_get_cur_oppidx(TARGET_DEFAULT);

		if (!is_dcs_enable())
			return ged_get_freq_by_idx(oppidx);

		core_num = dcs_get_cur_core_num();

		if (core_num == g_max_core_num)
			return ged_get_freq_by_idx(oppidx);

		for (i = 0; i < g_avail_mask_num; i++) {
			if (g_mask_table[i].num == core_num)
				break;
		}

		if (g_async_ratio_support)
			oppidx += i * g_oppnum_eachmask;
		else
			oppidx = g_min_working_oppidx + i;

		return ged_get_freq_by_idx(oppidx);
	}else
		return cur_stack_freq;

}

unsigned int ged_get_cur_real_stack_freq(void)
{
	unsigned int cur_minfreq = ged_get_freq_by_idx(ged_get_min_oppidx());
	unsigned int cur_real_stack_freq = gpufreq_get_cur_freq(TARGET_DEFAULT);

	//if GPU power off the freq is 26M ,the current oppidx is last oppidx. So get the freq
	if (cur_real_stack_freq < cur_minfreq)
		return ged_get_freq_by_idx(gpufreq_get_cur_oppidx(TARGET_DEFAULT));
	else
		return cur_real_stack_freq;
}

unsigned int ged_get_cur_volt(void)
{
	return gpufreq_get_cur_volt(TARGET_DEFAULT);
}

unsigned int ged_get_cur_top_volt(void)
{
	return gpufreq_get_cur_volt(TARGET_GPU);
}

static int ged_get_cur_virtual_oppidx(void)
{
	int top_freq = ged_get_top_freq_by_virt_opp(g_cur_oppidx);
	int sc_freq = ged_get_sc_freq_by_virt_opp(g_cur_oppidx);
	int cur_top = ged_get_cur_top_freq();
	int cur_stack = ged_get_cur_stack_freq();
	int cur_minfreq = ged_get_freq_by_idx(g_min_virtual_oppidx);

	// Make sure commit success
	if (cur_top == top_freq && cur_stack == sc_freq)
		return g_cur_oppidx;

	// if cur freq is 26M, use g_cur_oppidx
	if ((cur_top < cur_minfreq || cur_stack < cur_minfreq) &&
		g_cur_oppidx >= 0 && g_cur_oppidx < g_virtual_async_oppnum)
		return g_cur_oppidx;

	// check cur oppidx < g_min_stack_oppidx
	if (sc_freq > ged_get_sc_freq_by_virt_opp(g_min_stack_oppidx)) {
		g_cur_oppidx = gpufreq_get_cur_oppidx(TARGET_DEFAULT);
		return g_cur_oppidx;
	}
	// check opp > g_min_stack_oppidx and not in DCS
	g_cur_oppidx = g_min_stack_oppidx + g_min_async_oppnum;
	for (int i = g_min_stack_oppidx; i < g_virtual_async_oppnum; i++) {
		if (top_freq == gpufreq_get_freq_by_idx(TARGET_GPU, i)) {
			g_cur_oppidx = i - 1;
			break;
		}
	}
	// check not in DCS
	if (dcs_get_cur_core_num() == g_max_core_num)
		return g_cur_oppidx;

	for (int i = 0; i < g_avail_mask_num; i++) {
		if (g_mask_table[i].num == dcs_get_cur_core_num())
			return g_cur_oppidx += i * g_oppnum_eachmask;
	}

	// It should be returned in the above condition.
	return g_cur_oppidx;
}

int ged_get_cur_oppidx(void)
{
	unsigned int core_num = 0;
	int i = 0;
	int oppidx = 0;

	if (g_async_ratio_support && g_async_virtual_table_support)
		return ged_get_cur_virtual_oppidx();

	oppidx = gpufreq_get_cur_oppidx(TARGET_DEFAULT);

	if (!is_dcs_enable())
		return oppidx;

	core_num = dcs_get_cur_core_num();

	if (core_num == g_max_core_num)
		return oppidx;

	for (i = 0; i < g_avail_mask_num; i++) {
		if (g_mask_table[i].num == core_num)
			break;
	}

	if (g_async_ratio_support)
		oppidx += i * g_oppnum_eachmask;
	else
		oppidx = g_min_working_oppidx + i;

	return oppidx;
}

int ged_get_max_freq_in_opp(void)
{
	return g_max_freq_in_mhz;
}

int ged_get_max_oppidx(void)
{
	return g_max_working_oppidx;
}

int ged_get_min_oppidx_real(void)
{
	if (g_async_virtual_table_support && g_min_stack_oppidx)
		return g_min_stack_oppidx + g_min_async_oppnum;
	if (g_min_working_oppidx)
		return g_min_working_oppidx;
	else
		return gpufreq_get_opp_num(TARGET_DEFAULT) - 1;
}

unsigned int ged_get_opp_num(void)
{
	if (is_dcs_enable() && g_async_virtual_table_support)
		return g_virtual_async_oppnum;

	if (is_dcs_enable() && g_virtual_oppnum)
		return g_virtual_oppnum;

	if (g_working_oppnum)
		return g_working_oppnum;
	else
		return gpufreq_get_opp_num(TARGET_DEFAULT);
}

unsigned int ged_get_opp_num_real(void)
{
	if (g_async_virtual_table_support && g_min_stack_oppidx)
		return ged_get_min_oppidx_real() + 1;
	if (g_working_oppnum)
		return g_working_oppnum;
	else
		return gpufreq_get_opp_num(TARGET_DEFAULT);
}

unsigned int ged_get_power_by_idx(int oppidx)
{
	if (oppidx < 0 || oppidx >= g_working_oppnum)
		return 0;

	if (g_working_table == NULL)
		return gpufreq_get_power_by_idx(TARGET_DEFAULT, oppidx);

	if (unlikely(oppidx > g_min_working_oppidx))
		oppidx = g_min_working_oppidx;

	return g_working_table[oppidx].power;
}

int ged_get_oppidx_by_freq(unsigned int freq)
{
	int i = 0;

	if (g_working_table == NULL)
		return gpufreq_get_oppidx_by_freq(TARGET_DEFAULT, freq);

	if (is_dcs_enable() && g_virtual_table) {
		for (i = g_min_virtual_oppidx; i >= g_max_working_oppidx; i--) {
			if (g_virtual_table[i].freq >= freq)
				break;
		}
		return (i > g_max_working_oppidx) ? i : g_max_working_oppidx;
	}

	for (i = g_min_working_oppidx; i >= g_max_working_oppidx; i--) {
		if (g_working_table[i].freq >= freq)
			break;
	}
	return (i > g_max_working_oppidx) ? i : g_max_working_oppidx;
}

int ged_get_oppidx_by_stack_freq(unsigned int freq)
{
	int i = 0;

	if (!g_async_virtual_table_support || !g_virtual_async_table)
		return ged_get_oppidx_by_freq(freq);

	for (i = ged_get_min_oppidx(); i >= g_max_working_oppidx; i--) {
		if (g_virtual_async_table[i].scFreq >= freq)
			break;
	}
	return (i > g_max_working_oppidx) ? i : g_max_working_oppidx;
}

unsigned int ged_get_leakage_power(unsigned int volt)
{
	return gpufreq_get_leakage_power(TARGET_DEFAULT, volt);
}

unsigned int ged_get_dynamic_power(unsigned int freq, unsigned int volt)
{
	return gpufreq_get_dynamic_power(TARGET_DEFAULT, freq, volt);
}

int ged_get_cur_limit_idx_ceil(void)
{
	return gpufreq_get_cur_limit_idx(TARGET_DEFAULT, GPUPPM_CEILING);
}

int ged_get_cur_limit_idx_floor(void)
{
	int cur_floor = 0;

	cur_floor =  gpufreq_get_cur_limit_idx(TARGET_DEFAULT, GPUPPM_FLOOR);

	if (is_dcs_enable() && cur_floor == g_min_working_oppidx)
		cur_floor = g_min_virtual_oppidx;

	return cur_floor;
}

unsigned int ged_get_cur_limiter_ceil(void)
{
	return gpufreq_get_cur_limiter(TARGET_DEFAULT, GPUPPM_CEILING);
}

unsigned int ged_get_cur_limiter_floor(void)
{
	return gpufreq_get_cur_limiter(TARGET_DEFAULT, GPUPPM_FLOOR);
}

int ged_set_limit_ceil(int limiter, int ceil)
{
	if (ceil > g_min_working_oppidx)
		ceil = g_min_working_oppidx;

	if (limiter)
		return gpufreq_set_limit(TARGET_DEFAULT,
			LIMIT_APIBOOST, ceil, GPUPPM_KEEP_IDX);
	else
		return gpufreq_set_limit(TARGET_DEFAULT,
			LIMIT_FPSGO, ceil, GPUPPM_KEEP_IDX);
}

int ged_set_limit_floor(int limiter, int floor)
{
	if (floor > g_min_working_oppidx)
		floor = g_min_working_oppidx;

	if (limiter)
		return gpufreq_set_limit(TARGET_DEFAULT,
			LIMIT_APIBOOST, GPUPPM_KEEP_IDX, floor);
	else
		return gpufreq_set_limit(TARGET_DEFAULT,
			LIMIT_FPSGO, GPUPPM_KEEP_IDX, floor);
}

void ged_set_ud_mask_bit(unsigned int ud_mask_bit)
{
	mutex_lock(&g_ud_DCS_lock);
	g_ud_mask_bit = ud_mask_bit;
	mutex_unlock(&g_ud_DCS_lock);
}

unsigned int ged_get_ud_mask_bit(void)
{
	return g_ud_mask_bit;
}

int ged_gpufreq_commit(int oppidx, int commit_type, int *bCommited)
{
	int ret = GED_OK;
	int oppidx_tar = 0;
	int oppidx_cur = 0;
	int mask_idx = 0;
	int freqScaleUpFlag = false;
	unsigned int freq = 0, core_mask_tar = 0, core_num_tar = 0;
	unsigned int ud_mask_bit = 0;

	int dvfs_state = 0;
	g_cur_oppidx = oppidx;

	oppidx_cur = ged_get_cur_oppidx();
	if (oppidx_cur > oppidx) /* freq scale up */
		freqScaleUpFlag = true;

	/* convert virtual opp to working opp with corresponding core mask */
	if (oppidx > g_min_working_oppidx && g_async_ratio_support) {
		mask_idx = g_avail_mask_num - 1;
		oppidx_tar = g_min_working_oppidx - (g_min_virtual_oppidx - oppidx);
	} else if (oppidx > g_min_working_oppidx) {
		mask_idx = g_avail_mask_num - 1;
		oppidx_tar = g_min_stack_oppidx;
	} else {
		mask_idx = 0;
		oppidx_tar = oppidx;
	}

	/* write working opp to sysram */
	ged_dvfs_set_sysram_last_commit_top_idx(oppidx_tar);
	ged_dvfs_set_sysram_last_commit_stack_idx(oppidx_tar);
	ged_dvfs_set_sysram_last_commit_dual_idx(oppidx_tar, oppidx_tar);

	/* scaling cores to max if freq. is fixed */
	dvfs_state = gpufreq_get_dvfs_state();

	if (dvfs_state & DVFS_FIX_OPP || dvfs_state & DVFS_FIX_FREQ_VOLT) {
		mask_idx = 0;
		oppidx_tar = oppidx;
	}

	if (is_dcs_enable()) {
		if (dcs_get_dcs_stress()) {
			oppidx_tar = g_min_working_oppidx;
			commit_type = GED_DVFS_DCS_STRESS_COMMIT;
		}
	}

	if (!freqScaleUpFlag) /* freq scale down: commit freq. --> set core_mask */
		ged_dvfs_gpu_freq_commit_fp(oppidx_tar, commit_type, bCommited);

	/* DCS policy enabled */
	if (is_dcs_enable()) {
		if (dcs_get_dcs_stress()) {
			unsigned int rand;
			int cnt = 0;

			/* Stress test: routine random */
			get_random_bytes(&rand, sizeof(rand));
			rand = rand % g_avail_mask_num;
			while (g_stress_mask_idx == rand) {
				get_random_bytes(&rand, sizeof(rand));
				rand = rand % g_avail_mask_num;
				cnt++;
				if (cnt > 5)
					break;
			}

			if (dcs_get_dcs_stress() > 1) {
				GED_LOGI("[DCS stress] set core %d -> %d, cnt=%d",
					g_mask_table[g_stress_mask_idx].num,
					g_mask_table[rand].num, cnt);
			}

			g_stress_mask_idx = rand;
			mask_idx = g_stress_mask_idx;
		}

		core_mask_tar = g_mask_table[mask_idx].mask;
		core_num_tar = g_mask_table[mask_idx].num;
		ud_mask_bit = (ged_get_ud_mask_bit() |
			(1 << (g_max_core_num-1))) & ((1 << (g_max_core_num)) - 1);

		if ((ud_mask_bit > 0) && (mask_idx > 0)) {
			while (!((1 << (core_num_tar-1)) & ud_mask_bit) && mask_idx) {
				mask_idx -= 1;
				core_mask_tar = g_mask_table[mask_idx].mask;
				core_num_tar = g_mask_table[mask_idx].num;
			}
		}

		dcs_set_core_mask(core_mask_tar, core_num_tar);
	} else
		dcs_restore_max_core_mask();

	if (freqScaleUpFlag) /* freq scale up: set core_mask --> commit freq. */
		ged_dvfs_gpu_freq_commit_fp(oppidx_tar, commit_type, bCommited);

	/* TODO: return value handling */
	return ret;
}

int ged_gpufreq_dual_commit(int gpu_oppidx, int stack_oppidx, int commit_type, int *bCommited)
{
	int ret = GED_OK;
	int oppidx_tar = 0;
	int oppidx_cur = 0;
	int mask_idx = 0;
	int freqScaleUpFlag = false;
	unsigned int freq = 0, core_mask_tar = 0, core_num_tar = 0;
	unsigned int ud_mask_bit = 0;

	int dvfs_state = 0;

	if (gpu_oppidx < 0 || stack_oppidx < 0) {
		GED_LOGE("[DVFS_ASYNC] oppidx in dual_commit is invalid: gpu(%d) stack(%d)",
				gpu_oppidx, stack_oppidx);
		return GED_ERROR_FAIL;
	}

	g_cur_oppidx = stack_oppidx;
	oppidx_cur = ged_get_cur_oppidx();
	if (oppidx_cur > stack_oppidx) /* freq scale up */
		freqScaleUpFlag = true;

	/* convert virtual opp to working opp with corresponding core mask */
	if (stack_oppidx > g_min_working_oppidx && g_async_ratio_support) {
		mask_idx = g_avail_mask_num - 1;
		oppidx_tar = g_min_working_oppidx - (g_min_virtual_oppidx - stack_oppidx);
	} else if (stack_oppidx > g_min_working_oppidx) {
		mask_idx = g_avail_mask_num - 1;
		oppidx_tar = g_min_stack_oppidx;
	} else {
		mask_idx = 0;
		oppidx_tar = stack_oppidx;
	}

	/* convert top virtual opp to working opp but neglecting core mask */
	if (gpu_oppidx > g_min_working_oppidx)
		gpu_oppidx = g_min_working_oppidx - (g_min_virtual_oppidx - gpu_oppidx);

	// replace virtual opp with real opp if g_async_virtual_table_support
	if (g_async_virtual_table_support && g_virtual_async_table) {
		oppidx_tar = g_virtual_async_table[stack_oppidx].scRealOpp;
		gpu_oppidx = g_virtual_async_table[stack_oppidx].topRealOpp;
	}


	/* write working opp to sysram */
	ged_dvfs_set_sysram_last_commit_top_idx(gpu_oppidx);
	ged_dvfs_set_sysram_last_commit_stack_idx(oppidx_tar);
	ged_dvfs_set_sysram_last_commit_dual_idx(gpu_oppidx, oppidx_tar);

	/* scaling cores to max if freq. is fixed */
	dvfs_state = gpufreq_get_dvfs_state();

	if (dvfs_state & DVFS_FIX_OPP || dvfs_state & DVFS_FIX_FREQ_VOLT) {
		mask_idx = 0;
		oppidx_tar = stack_oppidx;
	}

	if (is_dcs_enable()) {
		if (dcs_get_dcs_stress()) {
			oppidx_tar = g_min_working_oppidx;
			commit_type = GED_DVFS_DCS_STRESS_COMMIT;
		}
	}
	if (!freqScaleUpFlag) /* freq scale down: commit freq. --> set core_mask */
		ged_dvfs_gpu_freq_dual_commit_fp(gpu_oppidx, oppidx_tar, bCommited);

	/* DCS policy enabled */
	if (is_dcs_enable()) {
		if (dcs_get_dcs_stress()) {
			unsigned int rand;
			int cnt = 0;

			/* Stress test: routine random */
			get_random_bytes(&rand, sizeof(rand));
			rand = rand % g_avail_mask_num;
			while (g_stress_mask_idx == rand) {
				get_random_bytes(&rand, sizeof(rand));
				rand = rand % g_avail_mask_num;
				cnt++;
				if (cnt > 5)
					break;
			}

			if (dcs_get_dcs_stress() > 1) {
				GED_LOGI("[DCS stress] set core %d -> %d, cnt=%d",
					g_mask_table[g_stress_mask_idx].num,
					g_mask_table[rand].num, cnt);
			}

			g_stress_mask_idx = rand;
			mask_idx = g_stress_mask_idx;
		}

		core_mask_tar = g_mask_table[mask_idx].mask;
		core_num_tar = g_mask_table[mask_idx].num;
		ud_mask_bit = (ged_get_ud_mask_bit() |
			(1 << (g_max_core_num-1))) & ((1 << (g_max_core_num)) - 1);

		if ((ud_mask_bit > 0) && (mask_idx > 0)) {
			while (!((1 << (core_num_tar-1)) & ud_mask_bit) && mask_idx) {
				mask_idx -= 1;
				core_mask_tar = g_mask_table[mask_idx].mask;
				core_num_tar = g_mask_table[mask_idx].num;
			}
		}

		dcs_set_core_mask(core_mask_tar, core_num_tar);
	} else
		dcs_restore_max_core_mask();

	if (freqScaleUpFlag) /* freq scale up: set core_mask --> commit freq. */
		ged_dvfs_gpu_freq_dual_commit_fp(gpu_oppidx, oppidx_tar, bCommited);

	/* TODO: return value handling */
	return ret;
}

unsigned int ged_gpufreq_bringup(void)
{
	return gpufreq_bringup();
}

unsigned int ged_gpufreq_get_power_state(void)
{
	return gpufreq_get_power_state();
}
