/* linux/arch/arm64/mach-exynos/include/mach/exynos-devfreq.h
 *
 * Copyright (c) 2015 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#ifndef __EXYNOS_DEVFREQ_H_
#define __EXYNOS_DEVFREQ_H_

#include <linux/devfreq.h>
#include <soc/samsung/exynos_pm_qos.h>
#include <linux/clk.h>
#include <soc/samsung/exynos-dm.h>

#include <soc/samsung/exynos-wow.h>
#include <soc/samsung/cal-if.h>

#if IS_ENABLED(CONFIG_SCHED_EMS)
#include <linux/ems.h>
#endif

#define EXYNOS_DEVFREQ_MODULE_NAME	"exynos-devfreq"
#define VOLT_STEP			25000
#define MAX_NR_CONSTRAINT		DM_TYPE_END
#define DATA_INIT			5
#define SET_CONST			1
#define RELEASE				2
#define KHZ				(1000)

/* DEVFREQ GOV TYPE */
#define SIMPLE_INTERACTIVE 0

struct devfreq_notifier_block {
       struct notifier_block nb;
       struct devfreq *df;
};

struct exynos_devfreq_policy {
	unsigned long min_freq;
	unsigned long max_freq;
	unsigned long governor_freq;
	int governor_win;
	enum sysbusy_state new_state;
	enum sysbusy_state old_state;
};

struct exynos_devfreq_opp_table {
	u32 idx;
	u32 freq;
	u32 volt;
};

struct um_exynos {
	struct list_head node;
	void __iomem **va_base;
	u32 *pa_base;
	u32 *mask_v;
	u32 *mask_a;
	u32 *channel;
	unsigned int um_count;
	u64 val_ccnt;
	u64 val_pmcnt;
};

struct exynos_devfreq_freq_infos {
	/* Basic freq infos */
	// min/max frequency
	u32 max_freq;
	u32 min_freq;
	// cur_freq pointer
	const u32 *cur_freq;
	// min/max pm_qos node
	u32 pm_qos_class;
	u32 pm_qos_class_max;
	// num of freqs
	u32 max_state;
};

struct exynos_devfreq_profile {
	int num_stats;
	int enabled;

	struct exynos_wow_profile last_wow_profile;
	/* Total time in state */
	ktime_t *time_in_state;
	ktime_t last_time_in_state;
	u64	*tdata_in_state;

	/* Active time in state */
	ktime_t *active_time_in_state;
	ktime_t last_active_time_in_state;

	u64 **freq_stats;
	u64 *last_freq_stats;

};

struct exynos_devfreq_data {
	struct device				*dev;
	struct devfreq				*devfreq;
	struct mutex				lock;
	spinlock_t				update_status_lock;
	struct clk				*clk;

	bool					devfreq_disabled;

	u32		devfreq_type;

	struct dvfs_rate_volt			*opp_list;

	u32					default_qos;

	u32					max_state;
	struct devfreq_dev_profile		devfreq_profile;

	const char				*governor_name;
	u32					cal_qos_max;
	u32					dfs_id;
	u32					old_freq;
	u32					new_freq;
	u32					min_freq;
	u32					max_freq;
	u32					reboot_freq;
	u32					boot_freq;
	u64					suspend_freq;
	u64					suspend_req;

	u32					pm_qos_class;
	u32					pm_qos_class_max;
	struct exynos_pm_qos_request		sys_pm_qos_min;
	struct exynos_pm_qos_request		sys_pm_qos_max;
#if IS_ENABLED(CONFIG_ARM_EXYNOS_DEVFREQ_DEBUG)
	struct exynos_pm_qos_request		debug_pm_qos_min;
	struct exynos_pm_qos_request		debug_pm_qos_max;
#endif
	struct exynos_pm_qos_request		default_pm_qos_min;
	struct exynos_pm_qos_request		default_pm_qos_max;
	struct exynos_pm_qos_request		boot_pm_qos;
	u32					boot_qos_timeout;

	struct notifier_block			reboot_notifier;

	bool					sysbusy;
	struct notifier_block			sysbusy_notifier;
	bool					emstune;
	struct notifier_block			emstune_notifier;
	int (*sysbusy_gov_callback)(struct devfreq *, enum sysbusy_state);
	int (*emstune_gov_callback)(struct devfreq *, int ems_mode);
	struct work_struct	update_devfreq_work;

	u32					ess_flag;

#if IS_ENABLED(CONFIG_EXYNOS_ESCA_DVFS_MANAGER) || IS_ENABLED(CONFIG_EXYNOS_DVFS_MANAGER)
	u32		dm_type;
	u32		nr_constraint;
	struct exynos_dm_constraint		**constraint;
#endif
	void					*private_data;
	bool					use_acpm;
	bool					bts_update;
	bool					update_fvp;
	bool					use_get_dev;
	bool					use_dtm;

	struct devfreq_notifier_block nb;
	struct devfreq_notifier_block nb_max;
	struct devfreq_notifier_block		*um_nb;
	struct um_exynos			um_data;
	u32					last_um_usage_rate;

	struct exynos_pm_domain *pm_domain;
	const char				*pd_name;
	unsigned long *time_in_state;
	unsigned long last_stat_updated;
	struct exynos_devfreq_profile		*profile;
	struct exynos_devfreq_policy policy;
};

struct exynos_devfreq_profile_data {
	u64 start_time;
	u64 duration;
	struct exynos_devfreq_profile_data_wow *wow;
};

struct exynos_devfreq_profile_data_wow {
	unsigned long long total_time_aggregated;
	unsigned long long busy_time_aggregated;
	unsigned long long total_time_cpu_path;
	unsigned long long busy_time_cpu_path;
	struct exynos_wow_profile wow_profile;
	struct exynos_wow_profile prev_wow_profile;
};

#if IS_ENABLED(CONFIG_EXYNOS_ALT_DVFS)
extern int devfreq_simple_interactive_init(void);
extern void exynos_alt_call_chain(void);
#endif

#if IS_ENABLED(CONFIG_EXYNOS_WOW)
extern void exynos_devfreq_get_profile(unsigned int devfreq_type, ktime_t **time_in_state, u64 *tdata_in_state);
#else
static inline void exynos_devfreq_get_profile(unsigned int devfreq_type, ktime_t **time_in_state, u64 *tdata_in_state)
{
	return;
}
#endif

#if IS_ENABLED(CONFIG_ARM_EXYNOS_DEVFREQ) || IS_ENABLED(CONFIG_ARM_EXYNOS_ESCA_DEVFREQ)
extern unsigned long exynos_devfreq_get_domain_freq(unsigned int devfreq_type);
extern void exynos_devfreq_get_freq_infos(unsigned int devfreq_type, struct exynos_devfreq_freq_infos *infos);
extern void exynos_devfreq_set_profile(unsigned int devfreq_type, int enable);
extern unsigned long exynos_devfreq_policy_update(struct exynos_devfreq_data *data,
		unsigned long governor_freq);
extern void register_get_dev_status(struct exynos_devfreq_data *data, struct devfreq *devfreq,
		struct device_node *get_dev_np);
extern int exynos_devfreq_governor_nop_init(void);
extern unsigned long exynos_devfreq_freq_mapping(
					struct exynos_devfreq_data *data,
					 unsigned long freq);
extern int exynos_devfreq_update_status(struct exynos_devfreq_data *devdata,
				 bool call_by_external);
static inline int exynos_devfreq_update_status_external(
					struct exynos_devfreq_data *devdata)
{
	return exynos_devfreq_update_status(devdata, true);
}
static inline int exynos_devfreq_update_status_internal(
					struct exynos_devfreq_data *devdata)
{
	return exynos_devfreq_update_status(devdata, false);
}
#else
static inline unsigned long exynos_devfreq_get_domain_freq(unsigned int devfreq_type)
{
	return 0;
}
extern inline void exynos_devfreq_get_freq_infos(unsigned int devfreq_type, struct exynos_devfreq_freq_infos *infos)
{
}
static inline void exynos_devfreq_set_profile(unsigned int devfreq_type, int enable)
{
}
static unsigned long exynos_devfreq_policy_update(struct exynos_devfreq_data *data,
		unsigned long governor_freq)
{
	return -ENXIO;
}
#endif
#endif	/* __EXYNOS_DEVFREQ_H_ */
