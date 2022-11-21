/*
 * Copyright (c) 2015 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com
 *
 * EXYNOS - CPU frequency scaling support for single cluster EXYNOS SoC series
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed "as is" WITHOUT ANY WARRANTY of any
 * kind, whether express or implied; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <linux/cpufreq.h>
#include <linux/of.h>

#include <soc/samsung/cpufreq.h>
#include <soc/samsung/exynos-pmu.h>

#include "../../drivers/soc/samsung/pwrcal/pwrcal.h"

#if defined(CONFIG_SOC_EXYNOS7570)
#include "../../drivers/soc/samsung/pwrcal/S5E7570/S5E7570-vclk.h"
#endif

static struct exynos_dvfs_info *exynos_info;

static void exynos_sc_cpufreq_set_freq(unsigned int old_index,
                                                unsigned int new_index)
{
        if (cal_dfs_set_rate(dvfs_cpucl0, (unsigned long)exynos_info->freq_table[new_index].frequency) < 0)
                pr_err("CL0 : failed to set_freq(%d -> %d)\n",old_index, new_index);
}

static unsigned int exynos_sc_cpufreq_get_freq(void)
{
        unsigned int freq;

        freq = (unsigned int)cal_dfs_get_rate(dvfs_cpucl0);
        if (!freq)
                pr_err("CL_ZERO: failed cal_dfs_get_rate(%dKHz)\n", freq);

        return freq;
}

static void exynos_sc_cpufreq_set_cal_ops(void)
{
        /* set cal ops for cpucl0 core */
	exynos_info->set_freq = exynos_sc_cpufreq_set_freq;
	exynos_info->get_freq = exynos_sc_cpufreq_get_freq;

	exynos_info->set_ema = NULL;
	exynos_info->need_apll_change = NULL;
	exynos_info->is_alive = NULL;
	exynos_info->set_int_skew = NULL;
	exynos_info->check_smpl = NULL;
	exynos_info->clear_smpl = NULL;
	exynos_info->init_smpl = NULL;
}

static int exynos_sc_cpufreq_init_cal_table(void)
{
        int table_size, i, cl_id = dvfs_cpucl0;
        struct dvfs_rate_volt *ptr_temp_table;
        struct exynos_dvfs_info *ptr = exynos_info;
        unsigned int cal_max_freq;
        unsigned int cal_max_support_idx = 0;

        if (!ptr->freq_table || !ptr->volt_table) {
                pr_err("%s: freq of volt table is NULL\n", __func__);
                return -EINVAL;
        }

        /* allocate to temporary memory for getting table from cal */
        ptr_temp_table = kzalloc(sizeof(struct dvfs_rate_volt)
                                * ptr->max_idx_num, GFP_KERNEL);

        /* check freq_table with cal */
        table_size = cal_dfs_get_rate_asv_table(cl_id, ptr_temp_table);

        if (ptr->max_idx_num != table_size) {
                pr_err("%s: DT is not matched cal table size\n", __func__);
                kfree(ptr_temp_table);
                return -EINVAL;
        }

        cal_max_freq = cal_dfs_get_max_freq(cl_id);
        if (!cal_max_freq) {
                pr_err("%s: failed get max frequency from PWRCAL\n", __func__);
                return -EINVAL;
        }

        for (i = 0; i< ptr->max_idx_num; i++) {
                if (ptr->freq_table[i].frequency != (unsigned int)ptr_temp_table[i].rate) {
                        pr_err("%s: DT is not matched cal frequency_table(dt : %d, cal : %d\n",
                                        __func__, ptr->freq_table[i].frequency,
                                        (unsigned int)ptr_temp_table[i].rate);
                        kfree(ptr_temp_table);
                        return -EINVAL;
                } else {
                        /* copy cal voltage to cpufreq driver voltage table */
                        ptr->volt_table[i] = ptr_temp_table[i].volt;
                }

                if (ptr_temp_table[i].rate == cal_max_freq)
                        cal_max_support_idx = i;
        }

        pr_info("CPUFREQ of CL0 CAL max_freq %lu KHz, DT max_freq %lu\n",
                        ptr_temp_table[cal_max_support_idx].rate,
                        ptr_temp_table[ptr->max_support_idx].rate);

        if (ptr->max_support_idx < cal_max_support_idx)
                ptr->max_support_idx = cal_max_support_idx;

        pr_info("CPUFREQ of CL0 Current max freq %lu KHz\n",
                                ptr_temp_table[ptr->max_support_idx].rate);

        /* free temporary memory */
        kfree(ptr_temp_table);

        return 0;
}

static void exynos_sc_cpufreq_print_info(void)
{
        int i;

        pr_info("CPUFREQ of CL0 max_support_idx %d, min_support_idx %d\n",
                        exynos_info->max_support_idx,
                        exynos_info->min_support_idx);

        pr_info("CPUFREQ of CL0 max_boot_qos %d, min_boot_qos %d\n",
                        exynos_info->boot_max_qos,
                        exynos_info->boot_min_qos);

	for (i = 0; i < exynos_info->max_idx_num; i++) {
		pr_info("CPUFREQ of CL0 : %2dL  %8d KHz  %7d uV (mif%8d KHz)\n",
			exynos_info->freq_table[i].driver_data,
			exynos_info->freq_table[i].frequency,
			exynos_info->volt_table[i],
			exynos_info->bus_table[i]);
	}
}

int exynos_sc_cpufreq_cal_init(struct exynos_dvfs_info *dvfs_info)
{
	int ret;

	exynos_info = dvfs_info;
	/* parsing cal rate, voltage table */
        ret = exynos_sc_cpufreq_init_cal_table();
        if (ret < 0) {
		pr_err("%s: init_cal_table failed\n", __func__);
		return ret;
        }

        /* print cluster DT information */
        exynos_sc_cpufreq_print_info();

        /* set ops for cal */
        exynos_sc_cpufreq_set_cal_ops();

        return 0;
}
