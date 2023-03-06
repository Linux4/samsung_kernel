/*
 * Copyright (c) 2013 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 *              Sangkyu Kim(skwith.kim@samsung.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __DEVFREQ_EXYNOS_H
#define __DEVFREQ_EXYNOS_H __FILE__

#include <linux/clk.h>

#include <mach/pm_domains.h>

#define VOLT_STEP	(12500)

struct devfreq_opp_table {
	unsigned int idx;
	unsigned long freq;
	unsigned long volt;
};

struct devfreq_clk_state {
	int clk_idx;
	int parent_clk_idx;
};

struct devfreq_clk_states {
	struct devfreq_clk_state *state;
	unsigned int state_count;
};

struct devfreq_clk_value {
	unsigned int reg;
	unsigned int set_value;
	unsigned int clr_value;
};

struct devfreq_clk_info {
	unsigned int idx;
	unsigned long freq;
	int pll;
	struct devfreq_clk_states *states;
};

struct devfreq_clk_list {
	const char *clk_name;
	struct clk *clk;
};

struct exynos_devfreq_platdata {
	unsigned long default_qos;
};

struct devfreq_info {
	unsigned int old;
	unsigned int new;
};

struct devfreq_pm_domain_link {
	const char *pm_domain_name;
	struct exynos_pm_domain *pm_domain;
};

struct devfreq_dynamic_clkgate {
	unsigned long	paddr;
	unsigned long	vaddr;
	unsigned int	bit;
	unsigned long	freq;
};

#ifdef CONFIG_ARM_EXYNOS5433_BUS_DEVFREQ
struct devfreq_data_mif {
	struct device *dev;
	struct devfreq *devfreq;

	struct regulator *vdd_mif;
	unsigned long old_volt;
	unsigned long volt_offset;

	struct mutex lock;

	struct notifier_block tmu_notifier;

	bool use_dvfs;

	void __iomem *base_mif;
	void __iomem *base_sysreg_mif;
	void __iomem *base_drex0;
	void __iomem *base_drex1;
	void __iomem *base_lpddr_phy0;
	void __iomem *base_lpddr_phy1;

	int default_qos;
	int initial_freq;
	int cal_qos_max;
	int max_state;
	bool dll_status;

	unsigned int *mif_asv_abb_table;

	int (*mif_set_freq)(struct devfreq_data_mif *data, int index, int old_index);
	int (*mif_set_and_change_timing_set)(struct devfreq_data_mif *data, int index);
	int (*mif_set_timeout)(struct devfreq_data_mif *data, int index);
	int (*mif_set_dll)(struct devfreq_data_mif *data, unsigned long volt, int index);
	void  (*mif_dynamic_setting)(struct devfreq_data_mif *data, bool flag);
	int (*mif_set_volt)(struct devfreq_data_mif *data, unsigned long volt,  unsigned long volt_range);
	int (*mif_pre_process)(struct device *dev, struct devfreq_data_mif *data, int *index, int *old_index, unsigned long *freq, unsigned long *old_freq);
	int (*mif_post_process)(struct device *dev, struct devfreq_data_mif *data, int *index, int *old_index, unsigned long *freq, unsigned long *old_freq);
};

struct devfreq_data_int {
	struct device *dev;
	struct devfreq *devfreq;

	struct regulator *vdd_int;
	struct regulator *vdd_int_m;
	unsigned long old_volt;
	unsigned long volt_offset;

	struct mutex lock;

	unsigned long initial_freq;
	unsigned long default_qos;
	int max_state;

	unsigned int *int_asv_abb_table;

	unsigned long target_volt;
	unsigned long volt_constraint_isp;
	unsigned int use_dvfs;

	struct notifier_block tmu_notifier;

	int (*int_set_freq)(struct devfreq_data_int *data, int index, int old_index);
	int (*int_set_volt)(struct devfreq_data_int *data, unsigned long volt,  unsigned long volt_range);
};

struct devfreq_data_disp {
	struct device *dev;
	struct devfreq *devfreq;

	struct mutex lock;
	unsigned int use_dvfs;
	unsigned long initial_freq;
	unsigned long default_qos;
	int max_state;

	int (*disp_set_freq)(struct devfreq_data_disp *data, int index, int old_index);
	int (*disp_set_volt)(struct devfreq_data_disp *data, unsigned long volt,  unsigned long volt_range);
};

struct devfreq_data_isp {
	struct device *dev;
	struct devfreq *devfreq;

	struct regulator *vdd_isp;
	unsigned long old_volt;
	unsigned long volt_offset;
	int max_state;

	struct mutex lock;
	unsigned int use_dvfs;
	unsigned long initial_freq;
	unsigned long default_qos;
	struct notifier_block tmu_notifier;

	int (*isp_set_freq)(struct devfreq_data_isp *data, int index, int old_index);
	int (*isp_set_volt)(struct devfreq_data_isp *data, unsigned long volt,  unsigned long volt_range, bool tolower);
};

struct devfreq_thermal_work {
	struct delayed_work devfreq_mif_thermal_work;
	int channel;
	struct workqueue_struct *work_queue;
	unsigned int thermal_level_cs0;
	unsigned int thermal_level_cs1;
	unsigned int polling_period;
	unsigned long max_freq;
};

#define CTRL_LOCK_VALUE_SHIFT	(0x8)
#define CTRL_LOCK_VALUE_MASK	(0x1FF)
#define CTRL_FORCE_SHIFT	(0x7)
#define CTRL_FORCE_MASK		(0x1FF)
#define CTRL_FORCE_OFFSET	(8)

#define MIF_VOLT_STEP		(12500)
#define COLD_VOLT_OFFSET	(37500)
#define LIMIT_COLD_VOLTAGE	(1250000)

unsigned int get_limit_voltage(unsigned int voltage, unsigned int volt_offset);
int exynos5_devfreq_get_idx(struct devfreq_opp_table *table, unsigned int size, unsigned long freq);

int exynos5433_devfreq_mif_init(struct devfreq_data_mif *data);
int exynos5433_devfreq_mif_deinit(struct devfreq_data_mif *data);
int exynos5433_devfreq_int_init(struct devfreq_data_int *data);
int exynos5433_devfreq_int_deinit(struct devfreq_data_int *data);
int exynos5433_devfreq_disp_init(struct devfreq_data_disp *data);
int exynos5433_devfreq_disp_deinit(struct devfreq_data_disp *data);
int exynos5433_devfreq_isp_init(struct devfreq_data_isp *data);
int exynos5433_devfreq_isp_deinit(struct devfreq_data_isp *data);
#endif

#endif /* __DEVFREQ_EXYNOS_H */
