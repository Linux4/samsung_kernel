/*
 * Copyright (c) 2016 DAEYEONG LEE, Samsung Electronics Co., Ltd <daeyeong.lee@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Exynos UFC(User Frequency Change) driver implementation
 */

enum exynos_ufc_execution_mode {
	AARCH64_MODE = 0,
	AARCH32_MODE,
	MODE_END,
};

enum exynos_ufc_ctrl_type {
	PM_QOS_MIN_LIMIT = 0,
	PM_QOS_MIN_WO_BOOST_LIMIT,
	PM_QOS_MAX_LIMIT,
	TYPE_END,
};

struct ufc_table_info {
	int			ctrl_type;
	int			mode;
	unsigned int		cur_index;
	u32			**ufc_table;
	struct list_head	list;

	struct kobject		kobj;
};

struct ufc_domain {
	struct cpumask		cpus;
	int			pm_qos_min_class;
	int			pm_qos_max_class;

	unsigned int		table_idx;
	unsigned int		min_freq;
	unsigned int		max_freq;
	unsigned int		clear_freq;

	struct pm_qos_request	user_min_qos_req;
	struct pm_qos_request	user_max_qos_req;
	struct pm_qos_request	user_min_qos_wo_boost_req;

	struct list_head	list;
};

struct exynos_ufc {
	unsigned int		table_row;
	unsigned int		table_col;
	unsigned int		lit_table_row;

	int			fill_flag;

	int			sse_mode;

	int			last_min_input;
	int			last_min_wo_boost_input;
	int			last_max_input;

	u32			**ufc_lit_table;
	struct list_head	ufc_domain_list;
	struct list_head	ufc_table_list;
};

#ifdef CONFIG_ARM_EXYNOS_UFC
extern void __init exynos_ufc_init(void);
extern unsigned int get_cpufreq_max_limit(void);
#else
static inline void __init exynos_ufc_init(void) { }
static inline unsigned int  get_cpufreq_max_limit(void) { return UINT_MAX; }
#endif
