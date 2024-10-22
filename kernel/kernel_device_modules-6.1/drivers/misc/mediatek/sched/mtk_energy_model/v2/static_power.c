// SPDX-License-Identifier: GPL-2.0
/*
 * static_power.c - static power api
 *
 * Copyright (c) 2022 MediaTek Inc.
 * Chung-Kai Yang <Chung-kai.Yang@mediatek.com>
 */

/* system includes */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/init.h>
#include <linux/kobject.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/pm_opp.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/cpufreq.h>
#include <linux/topology.h>
#include <linux/types.h>
#include "energy_model.h"

#define __LKG_PROCFS__ 0
#define __LKG_DEBUG__ 0
#define __DYN_CHECK__ 0

#define LUT_FREQ			GENMASK(11, 0)
#define LUT_NR_TOTAL_TYPE	GENMASK(7, 0)
#define LUT_NR_CPU_TYPE		GENMASK(11, 8)
#define LUT_NR_DSU_TYPE		GENMASK(15, 12)
#define LUT_NR_TOTAL_TYPE	GENMASK(7, 0)
#define LUT_DSU_WEIGHT		GENMASK(15, 0)
#define LUT_EMI_WEIGHT		GENMASK(31, 16)
#define LUT_ROW_SIZE		0x4

static struct eemsn_log *eemsn_log;
static void __iomem *usram_base, *csram_base, *wl_base, *curve_adj_base;
struct mtk_em_perf_domain *pds;

struct mtk_em_perf_domain *mtk_em_pd_ptr_public;
struct mtk_em_perf_domain *mtk_em_pd_ptr_private;
struct mtk_mapping mtk_mapping;
struct mtk_dsu_em mtk_dsu_em;
struct leakage_data info;
static unsigned int total_cpu, total_cluster;
static unsigned int cpu_mapping[16];
static bool wl_support, curve_adj_support;

bool is_wl_support(void)
{
	return wl_support;
}
EXPORT_SYMBOL_GPL(is_wl_support);

static void free_mtk_weighting(int cluster)
{
	int i;

	for (i = 0; i < cluster; i++)
		kfree(mtk_em_pd_ptr_public[cluster].mtk_weighting);
}

static int init_mtk_weighting(void)
{
	u32 data;
	int cluster, type;
	unsigned long offset = WL_WEIGHTING_OFFSET;
	struct mtk_em_perf_domain *pd_public;

	for (cluster = 0, type = 0; cluster < total_cluster; cluster++) {
		pd_public = &mtk_em_pd_ptr_public[cluster];
		pd_public->mtk_weighting =
			kcalloc(mtk_mapping.nr_dsu_type, sizeof(struct mtk_weighting),
				GFP_KERNEL);
		if (!pd_public->mtk_weighting)
			goto nomem;

		if (!is_wl_support()) {
			pd_public->mtk_weighting[type].dsu_weighting = 0;
			pd_public->mtk_weighting[type].emi_weighting = 0;
		} else {
			for (type = 0; type < mtk_mapping.nr_dsu_type; type++) {
				offset = WL_WEIGHTING_OFFSET + cluster * 0x4 + type * 0xC;
				data = readl_relaxed(wl_base + offset);
				pd_public->mtk_weighting[type].dsu_weighting =
					FIELD_GET(LUT_DSU_WEIGHT, data);
				pd_public->mtk_weighting[type].emi_weighting =
					FIELD_GET(LUT_EMI_WEIGHT, data);
			}
		}
		/* Set default weighting to be type 0 weighting */
		pd_public->cur_weighting = pd_public->mtk_weighting[0];
	}

	return 0;
nomem:
	free_mtk_weighting(cluster);
	return -ENOMEM;
}

static void free_dsu_em(int type)
{
	int i;

	for (i = 0; i < type; i++)
		kfree(mtk_dsu_em.dsu_wl_table[i]);
}

static int init_mtk_mapping(void)
{
	unsigned long offset = 0;
	int type = 0;
	u32 data;

	if (!is_wl_support())
		goto disable_wl;

	data = readl_relaxed(wl_base);
	mtk_mapping.total_type = FIELD_GET(LUT_NR_TOTAL_TYPE, data);
	mtk_mapping.nr_cpu_type = FIELD_GET(LUT_NR_CPU_TYPE, data);
	mtk_mapping.nr_dsu_type = FIELD_GET(LUT_NR_DSU_TYPE, data);
	pr_info("%s: %d, %d, %d\n",	__func__,
		mtk_mapping.total_type,
		mtk_mapping.nr_cpu_type,
		mtk_mapping.nr_dsu_type);

	offset += 0x4;
	if (mtk_mapping.total_type > MAX_NR_WL_TYPE) {
		pr_info("Invalid total type value: %d\n", mtk_mapping.total_type);
		goto disable_wl;
	}

	if (mtk_mapping.total_type < mtk_mapping.nr_cpu_type ||
		mtk_mapping.total_type < mtk_mapping.nr_dsu_type) {
		pr_info("Invalid type value: total_type: %d, nr_cpu_type: %d, nr_dsu_type: %d\n",
				mtk_mapping.total_type,
				mtk_mapping.nr_cpu_type,
				mtk_mapping.nr_dsu_type);
		goto disable_wl;
	}

	mtk_mapping.cpu_to_dsu =
		kcalloc(mtk_mapping.total_type, sizeof(struct mtk_relation),
					GFP_KERNEL);
	if (!mtk_mapping.cpu_to_dsu)
		return -ENOMEM;

	for (type = 0; type < mtk_mapping.total_type; type++) {
		mtk_mapping.cpu_to_dsu[type].cpu_type = ioread8(wl_base + offset);
		offset += 0x1;
		mtk_mapping.cpu_to_dsu[type].dsu_type = ioread8(wl_base + offset);
		offset += 0x1;
		pr_info("cpu type: %d, dsu type: %d\n",
			mtk_mapping.cpu_to_dsu[type].cpu_type,
			mtk_mapping.cpu_to_dsu[type].dsu_type);
	}

	return 0;

disable_wl:
	mtk_mapping.cpu_to_dsu =
		kcalloc(DEFAULT_NR_TYPE, sizeof(struct mtk_relation),
					GFP_KERNEL);
	if (!mtk_mapping.cpu_to_dsu)
		return -ENOMEM;

	mtk_mapping.total_type = DEFAULT_NR_TYPE;
	mtk_mapping.nr_cpu_type = DEFAULT_NR_TYPE;
	mtk_mapping.nr_dsu_type = DEFAULT_NR_TYPE;
	mtk_mapping.cpu_to_dsu[type].cpu_type = DEFAULT_TYPE;
	mtk_mapping.cpu_to_dsu[type].dsu_type = DEFAULT_TYPE;

	return 0;
}

static int init_dsu_em(void)
{
	u32 data;
	int type, opp, nr_opp = 0;
	unsigned long dsu_freq_offset, dsu_em_offset;

	if (!is_wl_support())
		goto out;

	mtk_dsu_em.nr_dsu_type = mtk_mapping.nr_dsu_type;
	mtk_dsu_em.dsu_wl_table =
		kcalloc(mtk_dsu_em.nr_dsu_type, sizeof(struct mtk_dsu_em_perf_state *),
				GFP_KERNEL);
	if (!mtk_dsu_em.dsu_wl_table)
		goto nomem;

	for (type = 0; type < mtk_dsu_em.nr_dsu_type; type++) {
		unsigned int cur_freq = 0, pre_freq = 0;

		dsu_em_offset = WL_DSU_EM_START + WL_DSU_EM_OFFSET * type;
		dsu_freq_offset = DSU_FREQ_OFFSET;
		mtk_dsu_em.dsu_wl_table[type] =
			kcalloc(MAX_NR_FREQ, sizeof(struct mtk_dsu_em_perf_state),
					GFP_KERNEL);
		if (!mtk_dsu_em.dsu_wl_table[type])
			goto err;

		for (opp = 0, pre_freq = 0; opp < MAX_NR_FREQ; opp++) {
			if (type == 0) {
				data = readl_relaxed(csram_base + dsu_freq_offset +
										opp * 0x4);
				cur_freq = FIELD_GET(LUT_FREQ, data) * 1000;
				if (cur_freq == pre_freq) {
					nr_opp = opp;
					break;
				}

				mtk_dsu_em.dsu_wl_table[type][opp].dsu_frequency
					= cur_freq;
				mtk_dsu_em.dsu_wl_table[type][opp].dsu_volt
					= (unsigned int) eemsn_log->det_log[total_cluster]
					.volt_tbl_init2[opp] * VOLT_STEP;
			} else {
				mtk_dsu_em.dsu_wl_table[type][opp].dsu_frequency
					= mtk_dsu_em.dsu_wl_table[0][opp].dsu_frequency;
				mtk_dsu_em.dsu_wl_table[type][opp].dsu_volt
					= mtk_dsu_em.dsu_wl_table[0][opp].dsu_volt;
			}

			mtk_dsu_em.dsu_wl_table[type][opp].dsu_bandwidth
				= ioread16(wl_base + dsu_em_offset);
			mtk_dsu_em.dsu_wl_table[type][opp].emi_bandwidth
				= ioread16(wl_base + dsu_em_offset + 0x2);
			mtk_dsu_em.dsu_wl_table[type][opp].dynamic_power
				= readl_relaxed(wl_base + dsu_em_offset + 0x4);
			dsu_em_offset += 0x8;
			pre_freq = cur_freq;
		}

		mtk_dsu_em.nr_perf_states = nr_opp;
	}

	mtk_dsu_em.dsu_table = mtk_dsu_em.dsu_wl_table[0];

out:
	return 0;
err:
	pr_info("%s: type%d: Init DSU wl table failed!!\n", __func__, type);
	free_dsu_em(type);
nomem:
	pr_info("%s: allocate DSU EM failed\n",	__func__);

	return -ENOENT;
}

static int check_wl_support(void)
{
	struct device_node *np;
	struct platform_device *pdev;
	int ret = 0, support;
	struct resource *res;

	np = of_find_node_by_name(NULL, "wl-info");
	if (!np) {
		pr_info("failed to find node @ %s\n", __func__);
		goto wl_disable;
	}

	pdev = of_find_device_by_node(np);
	if (!pdev) {
		pr_info("failed to find pdev @ %s\n", __func__);
		of_node_put(np);
		return -EINVAL;
	}

	ret = of_property_read_u32(np, "wl-support", &support);
	if (ret || support == 0)
		goto wl_disable;

	res = platform_get_resource(pdev, IORESOURCE_MEM, WL_MEM_RES_IND);
	if (!res) {
		ret = -ENODEV;
		goto no_res;
	}

	wl_base = ioremap(res->start, resource_size(res));
	if (!wl_base) {
		pr_info("%s failed to map resource %pR\n", __func__, res);
		ret = -ENOMEM;
		goto release_region;
	}

	wl_support = true;
	pr_info("%s: wl supports\n", __func__);
	of_node_put(np);

	return 0;
release_region:
	release_mem_region(res->start, resource_size(res));
no_res:
	pr_info("%s can't get mem resource %d\n", __func__, WL_MEM_RES_IND);
wl_disable:
	pr_info("%s wl-support is disabled: %d\n", __func__, ret);
	wl_support = false;
	of_node_put(np);

	return ret;
}

static unsigned int interpolate(unsigned int a, unsigned int b,
								unsigned int x, unsigned int y,
								unsigned int r)
{
	unsigned int i = x - a;
	unsigned int j = y - b;
	unsigned int l = r - a;

	return ((i == 0) ? b : (j * l / i + b));
}

static unsigned int mtk_convert_dyn_pwr(unsigned int base_freq,
									unsigned int cur_freq,
									unsigned int base_volt,
									unsigned int cur_volt,
									unsigned int base_dyn_pwr)
{
	u64 new_dyn_pwr;

	new_dyn_pwr = (u64)base_dyn_pwr * (u64)cur_volt;
	new_dyn_pwr = div64_u64(new_dyn_pwr, base_freq);
	new_dyn_pwr *= cur_volt;
	new_dyn_pwr = div64_u64(new_dyn_pwr, base_volt);
	new_dyn_pwr *= cur_freq;
	new_dyn_pwr = div64_u64(new_dyn_pwr, base_volt);

	return (unsigned int)new_dyn_pwr;
}

static unsigned int mtk_get_nr_perf_states(unsigned int cluster)
{
	unsigned int min_freq = mtk_em_pd_ptr_private[cluster].min_freq;
	unsigned int max_freq = mtk_em_pd_ptr_private[cluster].max_freq;
	unsigned int result;

	if (min_freq > max_freq) {
		pr_info("%s: %d: min freq is larger than max freq\n", __func__,
			__LINE__);
		return 0;
	}

	result = (max_freq - min_freq) / FREQ_STEP;
	result += ((max_freq - min_freq) % FREQ_STEP) ? 2 : 1;

	return result;
}

/**
 * This function is to free the memory which is reserved for wl tables of
 * private table.
 */
static void free_private_table(int pd_count, int type)
{
	int i, j, max_pd_count = total_cluster;

	if (!mtk_em_pd_ptr_private)
		return;

	for (i = 0; i <= type; i++) {
		if (i == type)
			max_pd_count = pd_count;
		for (j = 0; j < max_pd_count; j++)
			kfree(mtk_em_pd_ptr_public[j].wl_table[i]);
	}

	kfree(mtk_em_pd_ptr_private);
}

static int init_private_table(unsigned int cluster)
{
	unsigned int index = 0, base_freq_idx = 0, type = 0;
	unsigned int cur_freq;
	struct mtk_em_perf_domain *pd_public, *pd_private;
	struct mtk_em_perf_state *ps_base, *ps_next, *ps_new, ps_temp;

	pd_public = &mtk_em_pd_ptr_public[cluster];
	pd_private = &mtk_em_pd_ptr_private[cluster];
	if (!pd_private) {
		pr_info("failed to get private table for cluster%d\n", cluster);
		goto nomem;
	}
	/* Initialization for private table */
	pd_private->max_freq = pd_public->max_freq;
	pd_private->min_freq = pd_public->min_freq;
	pd_private->cpumask = pd_public->cpumask;
	pd_private->cluster_num = cluster;
	pd_private->nr_wl_tables = pd_public->nr_wl_tables;
	pd_private->nr_perf_states = mtk_get_nr_perf_states(cluster);
	if (is_wl_support()) {
		pd_private->mtk_weighting = pd_public->mtk_weighting;
		pd_private->cur_weighting = pd_public->cur_weighting;
	}

	pd_private->wl_table =
		kcalloc(pd_private->nr_wl_tables, sizeof(struct mtk_em_perf_state *),
			GFP_KERNEL);
	if (!pd_private->wl_table)
		goto nomem;

	for (type = 0; type < pd_private->nr_wl_tables; type++) {
		pd_private->wl_table[type] =
			kcalloc(pd_private->nr_perf_states, sizeof(struct mtk_em_perf_state),
					GFP_KERNEL);
		if (!pd_private->wl_table[type])
			goto nomem;

		base_freq_idx = 0;
		ps_base = &pd_public->wl_table[type][base_freq_idx];
		ps_next = &pd_public->wl_table[type][base_freq_idx + 1];
		for (index = 0, cur_freq = pd_private->max_freq;
			index < pd_private->nr_perf_states;
			index++, cur_freq -= FREQ_STEP) {
			/*
			 * If current frequency is over public table next opp, we
			 * need to update ps_base and ps_next.
			 */
			if (cur_freq < ps_next->freq && ps_next->freq != pd_private->min_freq) {
				base_freq_idx++;

				ps_base = &pd_public->wl_table[type][base_freq_idx];
				ps_next = &pd_public->wl_table[type][base_freq_idx + 1];
			}

			if (cur_freq == ps_base->freq) {
				ps_new = ps_base;
			} else if (cur_freq == ps_next->freq || cur_freq < pd_private->min_freq) {
				ps_new = ps_next;
			} else { // cur_freq < base_freq && cur_freq > next_freq
				if (type == 0) {
					ps_temp.volt =
						interpolate(ps_next->freq,
								ps_next->volt,
								ps_base->freq,
								ps_base->volt,
								cur_freq);
					ps_temp.leakage_para.a_b_para.a =
						interpolate(ps_next->volt,
								ps_next->leakage_para.a_b_para.a,
								ps_base->volt,
								ps_base->leakage_para.a_b_para.a,
								ps_temp.volt);
					ps_temp.leakage_para.a_b_para.b =
						interpolate(ps_next->volt,
								ps_next->leakage_para.a_b_para.b,
								ps_base->volt,
								ps_base->leakage_para.a_b_para.b,
								ps_temp.volt);
					ps_temp.leakage_para.c =
						interpolate(ps_next->volt,
								ps_next->leakage_para.c,
								ps_base->volt,
								ps_base->leakage_para.c,
								ps_temp.volt);
				} else {
					ps_temp.volt =
						pd_private->wl_table[0][index].volt;
					ps_temp.leakage_para =
						pd_private->wl_table[0][index].leakage_para;
				}
				ps_temp.capacity =
					interpolate(ps_next->freq,
								ps_next->capacity,
								ps_base->freq,
								ps_base->capacity,
								cur_freq);
				ps_temp.dyn_pwr =
					mtk_convert_dyn_pwr(ps_base->freq,
										cur_freq,
										ps_base->volt,
										ps_temp.volt,
										ps_base->dyn_pwr);
				if (is_wl_support())
					ps_temp.dsu_freq = ps_base->dsu_freq;

				ps_new = &ps_temp;
			}

			pd_private->wl_table[type][index].freq = cur_freq;
			pd_private->wl_table[type][index].volt = ps_new->volt;
			pd_private->wl_table[type][index].capacity = ps_new->capacity;
			pd_private->wl_table[type][index].leakage_para = ps_new->leakage_para;
			if (is_wl_support())
				pd_private->wl_table[type][index].dsu_freq = ps_new->dsu_freq;
			pd_private->wl_table[type][index].dyn_pwr = ps_new->dyn_pwr;
			/* Init for power efficiency(e.g., dyn_pwr / capacity) */
			pd_private->wl_table[type][index].pwr_eff =
				ps_new->dyn_pwr / ps_new->capacity;
		}
	}
	pd_private->cur_wl_table = 0;
	pd_private->table = pd_private->wl_table[0];

	return 0;
nomem:
	pr_info("%s: allocate private table for cluster %d failed\n",
			__func__, cluster);
	free_private_table(cluster, type);

	return -ENOENT;
}

/**
 * This function is to free the memory which is reserved for wl tables of
 * public table.
 */
static void free_public_table(int pd_count, int type)
{
	int i, j, max_pd_count = total_cluster;

	if (!mtk_em_pd_ptr_public)
		return;

	for (i = 0; i <= type; i++) {
		if (i == type)
			max_pd_count = pd_count;
		for (j = 0; j < max_pd_count; j++)
			kfree(mtk_em_pd_ptr_public[j].wl_table[i]);
	}

	kfree(mtk_em_pd_ptr_public);
}

#if __DYN_CHECK__
void check_dyn_pwr_is_valid(int type, int opp, int next_dyn_pwr,
			struct mtk_em_perf_domain *pd_public)
{
	if (pd_public->wl_table[type][opp].dyn_pwr != next_dyn_pwr)
		pr_info("dyn pwr err: type%d: pd%d:	opp%d: cur pwr: %d, next pwr: %d\n",
			type, pd_public->cluster_num, opp,
			pd_public->wl_table[type][opp].dyn_pwr, next_dyn_pwr);
}
#endif

int check_curve_adj_support(struct device_node *dvfs_node)
{
	struct device_node *curve_adj_node;

	curve_adj_node = of_parse_phandle(dvfs_node, "curve-adj-base", 0);
	if (curve_adj_node) {
		curve_adj_base = of_iomap(curve_adj_node, 0);
		if (!curve_adj_base) {
			curve_adj_support = false;
			return -ENODEV;
		}

		curve_adj_support = true;
		pr_info("%s: curve adjustment enable\n", __func__);
	} else
		curve_adj_support = false;

	return 0;
}

int mtk_get_volt_table(void)
{
	void __iomem *volt_base = curve_adj_base;
	unsigned int cur_cluster, cpu, opp, type;
	struct mtk_em_perf_domain *pd_public;
	u32 volt;

	/* CPU */
	for_each_possible_cpu(cpu) {
		cur_cluster = topology_cluster_id(cpu);
		pd_public = &mtk_em_pd_ptr_public[cur_cluster];
		for (opp = 0; opp < pd_public->nr_perf_states; opp++) {
			volt = readl_relaxed((volt_base + opp * LUT_ROW_SIZE));
			pd_public->wl_table[0][opp].volt = volt;
		}
		cpu = cpumask_last(pd_public->cpumask);
		volt_base += CLUSTER_VOLT_SIZE;
	}

	/* DSU */
	for (opp = 0; opp < mtk_dsu_em.nr_perf_states; opp++) {
		volt = readl_relaxed((volt_base + opp * LUT_ROW_SIZE));
		mtk_dsu_em.dsu_wl_table[0][opp].dsu_volt = volt;
	}

	for (type = 0; type < mtk_dsu_em.nr_dsu_type; type++) {
		for (opp = 0; opp < mtk_dsu_em.nr_perf_states; opp++)
			mtk_dsu_em.dsu_wl_table[type][opp].dsu_volt =
				mtk_dsu_em.dsu_wl_table[0][opp].dsu_volt;
	}

	return 0;
}

void arch_get_cluster_cpus(struct cpumask *cpus, int cluster_id)
{
	unsigned int cpu;

	cpumask_clear(cpus);
	for_each_possible_cpu(cpu) {
		int cpu_cluster_id = topology_cluster_id(cpu);

		if (cpu_cluster_id == cluster_id)
			cpumask_set_cpu(cpu, cpus);
	}
}

int mtk_create_freq_table(void)
{
	struct mtk_em_perf_domain *pd_public;
	void __iomem *freq_base = csram_base + CSRAM_TBL_OFFSET;
	void __iomem *pwr_base = csram_base + CSRAM_PWR_OFFSET;
	unsigned int i, cpu, cur_cluster = 0, freq, prev_freq;
	struct cpumask cpu_mask;
	u32 data;

	mtk_em_pd_ptr_public = kcalloc(MAX_PD_COUNT, sizeof(struct mtk_em_perf_domain),
			GFP_KERNEL);
	if (!mtk_em_pd_ptr_public)
		return -ENOMEM;

	for_each_possible_cpu(cpu) {
		cur_cluster = topology_cluster_id(cpu);
		pd_public = &mtk_em_pd_ptr_public[cur_cluster];
		pd_public->cluster_num = cur_cluster;
		pd_public->nr_wl_tables = mtk_mapping.nr_cpu_type;
		pd_public->wl_table =
			kcalloc(pd_public->nr_wl_tables, sizeof(struct mtk_em_perf_state *),
				GFP_KERNEL);
		if (!pd_public->wl_table)
			return -ENOMEM;

		pd_public->wl_table[0] =
			kcalloc(MAX_NR_FREQ, sizeof(struct mtk_em_perf_state),
					GFP_KERNEL);
		if (!pd_public->wl_table[0])
			return -ENOMEM;

		pd_public->cpumask = topology_cluster_cpumask(cpu);
		arch_get_cluster_cpus(&cpu_mask, cur_cluster);
		cpumask_copy(pd_public->cpumask, &cpu_mask);
		pr_info("%s: cpu: %d, %*pbl, last: %d\n", __func__, cpu,
			cpumask_pr_args(pd_public->cpumask), cpumask_last(pd_public->cpumask));
		prev_freq = 0;
		for (i = 0; i < MAX_NR_FREQ; i++) {
			/* Frequency */
			data = readl_relaxed((freq_base + i * LUT_ROW_SIZE));
			freq = FIELD_GET(LUT_FREQ, data) * 1000;
			pd_public->wl_table[0][i].freq = freq;
			/* Dynamic Power */
			data = readl_relaxed((pwr_base + i * LUT_ROW_SIZE));
			pd_public->wl_table[0][i].dyn_pwr = data;
			if (freq == prev_freq)
				break;

			prev_freq = freq;
		}

		pd_public->nr_perf_states = i;
		pd_public->max_freq = pd_public->wl_table[0][0].freq;
		pd_public->min_freq = pd_public->wl_table[0][i - 1].freq;
		cpu = cpumask_last(pd_public->cpumask);
		freq_base += CLUSTER_SIZE;
		pwr_base += CLUSTER_SIZE;
	}

	return 0;
}

/**
 * This function initializes frequency, capacity, dynamic power, and leakage
 * parameters of public table.
 */
static int init_public_table(void)
{
	unsigned int cpu, opp, cluster = 0, type = 0;
	int ret = 0, pre_cluster = -1;
	void __iomem *base = csram_base;
	unsigned long offset = CAPACITY_TBL_OFFSET, wl_offset;
	unsigned long cap, next_cap, end_cap;
	struct mtk_em_perf_domain *pd_public;

	ret = mtk_create_freq_table();
	if (ret)
		goto nomem;

	for_each_possible_cpu(cpu) {
		cluster = topology_cluster_id(cpu);
		if (cluster == -1) {
			pr_info("%s: default topology cluster: %d\n",
					__func__, cluster);
			goto err;
		}

		if (pre_cluster == cluster)
			continue;

		pd_public = &mtk_em_pd_ptr_public[cluster];
		for (opp = 0; opp < pd_public->nr_perf_states; opp++) {
			u32 data;

			cap = ioread16(base + offset);
			next_cap = ioread16(base + offset + CAPACITY_ENTRY_SIZE);
			if (cap == 0 || next_cap == 0)
				goto err;

			pd_public->wl_table[0][opp].capacity = cap;
			if (opp == pd_public->nr_perf_states - 1)
				next_cap = -1;

			pd_public->wl_table[0][opp].volt =
				(unsigned int) eemsn_log->det_log[cluster].volt_tbl_init2[opp]
								* VOLT_STEP;
			data = readl_relaxed((usram_base + 0x240 +
								cluster * CLUSTER_SIZE
								+ opp * 8));
			pd_public->wl_table[0][opp].leakage_para.c =
					readl_relaxed((usram_base + 0x240 +
								   cluster * CLUSTER_SIZE +
								   opp * 8 + 4));
			pd_public->wl_table[0][opp].leakage_para.a_b_para.b =
							((data >> 12) & 0xFFFFF);
			pd_public->wl_table[0][opp].leakage_para.a_b_para.a = data & 0xFFF;
			pd_public->wl_table[0][opp].pwr_eff = pd_public->wl_table[0][opp].dyn_pwr
					/ pd_public->wl_table[0][opp].capacity;

			offset += CAPACITY_ENTRY_SIZE;
		}

		pd_public->cur_wl_table = 0;
		pd_public->table = pd_public->wl_table[0];
		/* repeated last cap 0 between each cluster */
		end_cap = ioread16(base + offset);
		if (end_cap != cap)
			goto err;
		offset += CAPACITY_ENTRY_SIZE;
		cpu_mapping[cpu] = cluster;
		pre_cluster = cluster;
	}

	total_cluster = cluster + 1;
	total_cpu = cpu + 1;
	ret = init_dsu_em();
	if (ret < 0) {
		pr_info("%s: initialize DSU EM failed, ret: %d\n",
				__func__, ret);
		return ret;
	}

	if (curve_adj_support)
		mtk_get_volt_table();

	for (type = 0; type < mtk_mapping.nr_cpu_type; type++) {
		wl_offset = WL_TBL_START_OFFSET + WL_OFFSET * type;
		for (cluster = 0; cluster < total_cluster; cluster++) {
			pd_public = &mtk_em_pd_ptr_public[cluster];
			if (type > 0) {
				pd_public->wl_table[type] =
					kcalloc(pd_public->nr_perf_states,
						sizeof(struct mtk_em_perf_state), GFP_KERNEL);
				if (!pd_public->wl_table[type])
					goto nomem;
			}
			for (opp = 0; opp < pd_public->nr_perf_states; opp++, wl_offset += 0x8) {
				/* All types are same */
				pd_public->wl_table[type][opp].freq =
					pd_public->wl_table[0][opp].freq;
				pd_public->wl_table[type][opp].volt =
					pd_public->wl_table[0][opp].volt;
				pd_public->wl_table[type][opp].leakage_para =
					pd_public->wl_table[0][opp].leakage_para;

				if (is_wl_support()) {
					/* Init for CPU to DSU relationship */
					pd_public->wl_table[type][opp].dsu_freq =
						ioread16(wl_base + wl_offset) * 1000;
					pd_public->wl_table[type][opp].capacity =
						ioread16(wl_base + wl_offset + 0x2);
					pd_public->wl_table[type][opp].dyn_pwr =
						readl_relaxed(wl_base + wl_offset + 0x4);
					/* Check whether data is correct */
					if (opp == pd_public->nr_perf_states - 1) {
#if __DYN_CHECK__
						int next_dyn_pwr = readl_relaxed(wl_base +
							wl_offset + 0xC);
						check_dyn_pwr_is_valid(type, opp,
									next_dyn_pwr, pd_public);
#endif
						wl_offset += 0x8;
					}
				} else {
					pd_public->wl_table[type][opp].capacity =
						pd_public->wl_table[0][opp].capacity;
					pd_public->wl_table[type][opp].dyn_pwr =
						pd_public->wl_table[0][opp].dyn_pwr;
				}

				/* Init for power efficiency(e.g., dyn_pwr / capacity) */
				if (pd_public->wl_table[type][opp].capacity == 0)
					pd_public->wl_table[type][opp].capacity = 1;
				if (pd_public->wl_table[type][opp].capacity > 1024)
					pd_public->wl_table[type][opp].capacity = 1024;
				pd_public->wl_table[type][opp].pwr_eff =
					pd_public->wl_table[type][opp].dyn_pwr /
					pd_public->wl_table[type][opp].capacity;
			}
		}
	}
	pr_info("%s: total_cluster: %d\n", __func__, total_cluster);

	ret = init_mtk_weighting();
	if (ret < 0) {
		pr_info("%s: initialize weighting failed, ret: %d\n",
				__func__, ret);
	}

	return 0;
nomem:
	pr_info("%s: allocate public table for cluster %d and type%d failed\n",
			__func__, cluster, type);
err:
	pr_info("%s: capacity count or value does not match on cluster %d\n",
			__func__, cluster);
	free_public_table(cluster, type);

	return -ENOENT;
}

inline unsigned int mtk_get_leakage(unsigned int cpu, unsigned int idx, unsigned int degree)
{
	int a, b, c, power, cluster;
	struct mtk_em_perf_domain *pd_public;

	if (info.init != 0x5A5A) {
		pr_info("[leakage] not yet init!\n");
		return 0;
	}

	if (cpu > total_cpu)
		return 0;
	cluster = topology_cluster_id(cpu);
	pd_public = &mtk_em_pd_ptr_private[cluster];
	if (idx >= pd_public->nr_perf_states) {
		pr_debug("%s: %d: input index is out of nr_perf_states\n", __func__,
				__LINE__);
		return 0;
	}

	a = pd_public->table[idx].leakage_para.a_b_para.a;
	b = pd_public->table[idx].leakage_para.a_b_para.b;
	c = pd_public->table[idx].leakage_para.c;

	power = degree * (degree * a - b) + c;

	return (power /= (cluster > 0) ? 10 : 100);
}
EXPORT_SYMBOL_GPL(mtk_get_leakage);

int mtk_update_wl_table(int cluster, int type)
{
	struct mtk_em_perf_domain *pd_public, *pd_private;
	int cpu_type = 0, dsu_type = 0;

	if (cluster < 0 || cluster >= total_cluster) {
		pr_info("%s: %d: return NULL for cluster#%d", __func__,
					__LINE__, cluster);
		return -1;
	}

	if (type < 0 || type >= mtk_mapping.total_type) {
		pr_info("%s: input value is over total types for cluster%d and type%d",
			__func__, cluster, type);
		return -1;
	}

	pd_public = &mtk_em_pd_ptr_public[cluster];
	pd_private = &mtk_em_pd_ptr_private[cluster];
	cpu_type = mtk_mapping.cpu_to_dsu[type].cpu_type;
	dsu_type = mtk_mapping.cpu_to_dsu[type].dsu_type;
	if (is_wl_support()) {
		if (!mtk_dsu_em.dsu_wl_table[dsu_type]) {
			pr_info("%s: %d: DSU EM return NULL for type#%d", __func__,
						__LINE__, dsu_type);
			return -1;
		}

		mtk_dsu_em.cur_dsu_type = dsu_type;
		mtk_dsu_em.dsu_table = mtk_dsu_em.dsu_wl_table[dsu_type];
		if (!pd_public->mtk_weighting) {
			pr_info("%s: %d: public table weighting return NULL for cluster%d and type%d",
				__func__, __LINE__, cluster, cpu_type);
			return -1;
		}
		pd_public->cur_weighting = pd_public->mtk_weighting[cpu_type];

		if (!pd_private->mtk_weighting) {
			pr_info("%s: %d: private table weighting return NULL for cluster%d and type%d",
				__func__, __LINE__, cluster, cpu_type);
			return -1;
		}
		pd_private->cur_weighting = pd_private->mtk_weighting[cpu_type];
	}

	if (!pd_public->wl_table[cpu_type]) {
		pr_info("%s: public table return NULL for cluster%d and type%d",
			__func__, cluster, cpu_type);
		return -1;
	}

	if (!pd_private->wl_table[cpu_type]) {
		pr_info("%s: private return NULL for cluster%d and type%d",
			__func__, cluster, cpu_type);
		return -1;
	}

	pd_public->cur_wl_table = cpu_type;
	pd_private->cur_wl_table = cpu_type;
	pd_public->table = pd_public->wl_table[cpu_type];
	pd_private->table = pd_private->wl_table[cpu_type];

	return 0;
}
EXPORT_SYMBOL_GPL(mtk_update_wl_table);

unsigned int mtk_get_dsu_freq(void)
{
	return csram_read(PHY_CLK_OFF + (total_cluster * 0x4));
}
EXPORT_SYMBOL_GPL(mtk_get_dsu_freq);

#if __LKG_PROCFS__
#define PROC_FOPS_RW(name)                                              \
	static int name ## _proc_open(struct inode *inode, struct file *file)\
	{                                                                       \
		return single_open(file, name ## _proc_show, pde_data(inode));  \
	}                                                                       \
static const struct proc_ops name ## _proc_fops = {             \
		.proc_open           = name ## _proc_open,                              \
		.proc_read           = seq_read,                                        \
		.proc_lseek         = seq_lseek,                                        \
		.proc_release        = single_release,                          \
		.proc_write          = name ## _proc_write,                             \
}

#define PROC_FOPS_RO(name)                                                     \
	static int name##_proc_open(struct inode *inode, struct file *file)    \
	{                                                                      \
		return single_open(file, name##_proc_show, pde_data(inode));   \
	}                                                                      \
	static const struct proc_ops name##_proc_fops = {               \
		.proc_open = name##_proc_open,                                      \
		.proc_read = seq_read,                                              \
		.proc_lseek = seq_lseek,                                           \
		.proc_release = single_release,                                     \
	}

#define PROC_ENTRY(name)        {__stringify(name), &name ## _proc_fops}
#define PROC_ENTRY_DATA(name)   \
{__stringify(name), &name ## _proc_fops, g_ ## name}


#define LAST_LL_CORE	3

static int input_cluster, input_opp, input_temp;
u32 *g_leakage_trial;

static int leakage_trial_proc_show(struct seq_file *m, void *v)
{
	struct mtk_em_perf_domain *pd;
	struct mtk_em_perf_state *ps;
	int a, b, c, power;

	if (input_cluster >= total_cluster || input_cluster < 0)
		return 0;

	if (input_temp > 150 || input_temp < 0)
		return 0;

	if (input_opp > 32 || input_opp < 0)
		return 0;

	pd = &mtk_em_pd_ptr_public[input_cluster];
	ps = &pd->table[input_opp];
	a = ps->leakage_para.a_b_para.a;
	b = ps->leakage_para.a_b_para.b;
	c = ps->leakage_para.c;
	power = input_temp * (input_temp * a - b) + c;
	power /= (input_cluster > 0) ? 10 : 100;

	seq_printf(m, "power: %d, a, b, c = (%d %d %d) %*pbl\n",
				power, a, b, c,
				cpumask_pr_args(pd->cpumask));

	return 0;
}


static ssize_t leakage_trial_proc_write(struct file *file,
	const char __user *buffer, size_t count, loff_t *pos)
{
	char *buf = (char *) __get_free_page(GFP_USER);

	if (!buf)
		return -ENOMEM;

	if (count > 255) {
		free_page((unsigned long)buf);
		return -EINVAL;
	}

	if (copy_from_user(buf, buffer, count)) {
		free_page((unsigned long)buf);
		return -EINVAL;
	}

	if (sscanf(buf, "%d %d %d", &input_cluster, &input_opp, &input_temp) != 3) {
		free_page((unsigned long)buf);
		return -EINVAL;
	}
	return count;
}

PROC_FOPS_RW(leakage_trial);
#endif
static int create_spower_debug_fs(void)
{
#if __LKG_PROCFS__
	struct proc_dir_entry *dir = NULL;

	struct pentry {
		const char *name;
		const struct proc_ops *fops;
		void *data;
	};

	const struct pentry entries[] = {
		PROC_ENTRY_DATA(leakage_trial),
	};


	/* create /proc/cpuhvfs */
	dir = proc_mkdir("static_power", NULL);
	if (!dir) {
		pr_info("fail to create /proc/static_power @ %s()\n",
								__func__);
		return -ENOMEM;
	}

	proc_create_data(entries[0].name, 0664, dir, entries[0].fops, info.base);
#endif
	return 0;
}

static int mtk_static_power_probe(struct platform_device *pdev)
{
#if __LKG_DEBUG__
	unsigned int i, power;
#endif
	int ret = 0, err = 0;
	unsigned int cpu, cluster = 0, pre_cluster = 0;
	struct device_node *dvfs_node;
	struct platform_device *pdev_temp;
	struct resource *usram_res, *csram_res, *eem_res;

	pr_info("[Static Power v2.1.1] Start to parse DTS\n");
	dvfs_node = of_find_node_by_name(NULL, "cpuhvfs");
	if (dvfs_node == NULL) {
		pr_info("failed to find node @ %s\n", __func__);
		return -ENODEV;
	}

	pdev_temp = of_find_device_by_node(dvfs_node);
	if (pdev_temp == NULL) {
		pr_info("failed to find pdev @ %s\n", __func__);
		return -EINVAL;
	}

	usram_res = platform_get_resource(pdev_temp, IORESOURCE_MEM, 0);
	if (usram_res)
		usram_base = ioremap(usram_res->start, resource_size(usram_res));
	else {
		pr_info("%s can't get resource, ret: %d\n", __func__, err);
		return -ENODEV;
	}

	csram_res = platform_get_resource(pdev_temp, IORESOURCE_MEM, 1);
	if (csram_res)
		csram_base = ioremap(csram_res->start, resource_size(csram_res));
	else {
		pr_info("%s can't get resource, ret: %d\n", __func__, err);
		return -ENODEV;
	}

	eem_res = platform_get_resource(pdev_temp, IORESOURCE_MEM, 2);
	if (eem_res)
		eemsn_log = ioremap(eem_res->start, resource_size(eem_res));
	else {
		ret = -ENODEV;
		pr_info("%s can't get EEM resource, ret: %d\n", __func__, ret);
		goto error;
	}

	ret = check_curve_adj_support(dvfs_node);
	if (ret)
		pr_info("%s: Failed to check curve adj support: %d\n", __func__, ret);

	ret = check_wl_support();
	if (ret)
		pr_info("%s: Failed to check wl support: %d\n", __func__, ret);

	ret = init_mtk_mapping();
	if (ret) {
		pr_info("%s: Failed to init mtk mapping: %d\n", __func__, ret);
		goto error;
	}

	pr_info("[Static Power v2.1.1] MTK EM start\n");
	ret = init_public_table();
	if (ret < 0) {
		pr_info("%s: initialize public table failed, ret: %d\n",
				__func__, ret);
		return ret;
	}

	mtk_em_pd_ptr_private = kcalloc(MAX_PD_COUNT, sizeof(struct mtk_em_perf_domain),
			GFP_KERNEL);
	if (!mtk_em_pd_ptr_private) {
		pr_info("%s can't get private table ptr, ret: %d\n", __func__, err);
		return -ENOMEM;
	}

	for_each_possible_cpu(cpu) {
		cluster = topology_cluster_id(cpu);
		if (cpu != 0 && cluster == pre_cluster)
			continue;

		ret = init_private_table(cluster);
		if (ret) {
			pr_info("failed to init private table for cpu%d and cluster%d\n",
					cpu, cluster);
			return -ENODEV;
		}

		pre_cluster = cluster;
		pr_info("%s: MTK_EM: CPU %d: created perf domain\n", __func__, cpu);
	}

	/* Create debug fs */
	info.base = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(info.base))
		return PTR_ERR(info.base);

	create_spower_debug_fs();

#if __LKG_DEBUG__
	for_each_possible_cpu(cpu) {
		for (i = 0; i < 16; i++) {
			power = mtk_get_leakage(cpu, i, 40);
			pr_info("[leakage] power = %d\n", power);
		}
	}
#endif

	info.init = 0x5A5A;

	pr_info("[Static Power v2.1.1] MTK EM done\n");

	return ret;

error:
	return ret;
}

static const struct of_device_id mtk_static_power_match[] = {
	{ .compatible = "mediatek,mtk-lkg" },
	{}
};

static struct platform_driver mtk_static_power_driver = {
	.probe = mtk_static_power_probe,
	.driver = {
		.name = "mtk-lkg",
		.of_match_table = mtk_static_power_match,
	},
};

int __init mtk_static_power_init(void)
{
	int ret = 0;

	ret = platform_driver_register(&mtk_static_power_driver);
	return ret;
}

MODULE_DESCRIPTION("MTK static power Platform Driver v2.1.1");
MODULE_AUTHOR("Chung-Kai Yang <Chung-kai.Yang@mediatek.com>");
MODULE_LICENSE("GPL v2");
