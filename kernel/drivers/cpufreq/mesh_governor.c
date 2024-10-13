// SPDX-License-Identifier: GPL-2.0-only
/*
 *  linux/drivers/cpufreq/mesh_governor.c
 *
 *  Copyright (C) 2022 Samsung Electronics
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/init.h>
#include <linux/module.h>
#include <linux/min_heap.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <uapi/linux/sched/types.h>
#include <linux/sched/clock.h>
#include <linux/sched/cpufreq.h>
#include <linux/cpufreq.h>
#include <linux/kthread.h>
#include <linux/irq_work.h>

#include <soc/samsung/exynos_pm_qos.h>
#include <linux/mesh_governor.h>

/* header */
#define MESHGOV_DELAY_NS	400000

enum {
	MG_MIN_MID = 1,
	MG_MID_MAX,
	MG_RANGE_MAX,
};

struct meshgov_tunables {
	struct gov_attr_set attr_set;
	int mode;
	int min_freq;
	int max_freq;

	struct meshgov_policy 	*mg_policy;
	struct work_struct		frequtil_work;
};

struct cpufreq_stats {
	unsigned int total_trans;
	unsigned long long last_time;
	unsigned int max_state;
	unsigned int state_num;
	unsigned int last_index;
	u64 *time_in_state;
	unsigned int *freq_table;
	unsigned int *trans_table;

	/* Deferred reset */
	unsigned int reset_pending;
	unsigned long long reset_time;
};

struct meshgov_freqstat {
	int freq;
	unsigned long long time;
};

struct meshgov_policy {
	struct cpufreq_policy	*policy;
	struct meshgov_freqstat *freqstat;

	struct meshgov_tunables *tunables;
	struct list_head		tunables_hook;

	struct irq_work			irq_work;
	struct kthread_work		work;
	struct kthread_worker	worker;
	struct mutex			work_lock;
	struct task_struct		*thread;

	raw_spinlock_t			update_lock;
	raw_spinlock_t			time_in_state_lock;

	bool					work_in_progress;
	int						min_freq;
	int						max_freq;
	int 					min_idx;
	int 					max_idx;
	int						table_size;

	u64						last_freq_update_time;
};

struct meshgov_cpu {
	struct update_util_data	update_util;
	struct meshgov_policy *mg_policy;
	unsigned int cpu;

	u64	last_update;
};

/* header */

static DEFINE_PER_CPU(struct meshgov_cpu, meshgov_cpu);
static DEFINE_MUTEX(smpl_mutex);
static struct delayed_work smpl_work;
static struct exynos_pm_qos_request meshgov_lit_min_qos;
static struct exynos_pm_qos_request meshgov_lit_max_qos;

static struct exynos_pm_qos_request meshgov_mid_min_qos;
static struct exynos_pm_qos_request meshgov_mid_max_qos;

static struct exynos_pm_qos_request meshgov_big_min_qos;
static struct exynos_pm_qos_request meshgov_big_max_qos;

static struct exynos_pm_qos_request meshgov_mif_min_qos;
static struct exynos_pm_qos_request meshgov_mif_max_qos;

static struct exynos_pm_qos_request meshgov_int_min_qos;
static struct exynos_pm_qos_request meshgov_int_max_qos;

static struct exynos_pm_qos_request meshgov_cam_min_qos;
static struct exynos_pm_qos_request meshgov_cam_max_qos;

static struct exynos_pm_qos_request meshgov_g3d_min_qos;
static struct exynos_pm_qos_request meshgov_g3d_max_qos;

static bool smpl_warn;

static bool freqstat_less(const void *l, const void *r)
{
	const struct meshgov_freqstat *le = (const struct meshgov_freqstat *)l;
	const struct meshgov_freqstat *re = (const struct meshgov_freqstat *)r;

	return le->time < re->time; 
} 

static void freqstat_swap(void *l, void *r)
{
	struct meshgov_freqstat temp = *(struct meshgov_freqstat *)l;

	*(struct meshgov_freqstat *)l = *(struct meshgov_freqstat *)r;
	*(struct meshgov_freqstat *)r = temp;
}

static const struct min_heap_callbacks freqstat_funcs = {
	.elem_size = sizeof(struct meshgov_freqstat),
	.less = freqstat_less,
	.swp = freqstat_swap,
};

static int cpufreq_get_mesh_time_in_state(struct cpufreq_policy *policy)
{
	struct cpufreq_stats *stats = policy->stats;
	struct meshgov_policy *mg_policy = policy->governor_data;
	struct meshgov_freqstat *freqstat_heap, freqstat_elem;
	struct min_heap heap;
	bool pending = READ_ONCE(stats->reset_pending);
	unsigned long long time;
	int freqstat_size, freq, max_idx, min_idx, i;

	freqstat_size = mg_policy->table_size;
	freqstat_heap = mg_policy->freqstat;

	max_idx = mg_policy->max_idx;
	min_idx = mg_policy->min_idx;

	heap = (struct min_heap) {
		.data = freqstat_heap,
		.nr = 0,
		.size = freqstat_size,
	};

	for (i = max_idx; i >= min_idx; i--) {
		if (pending) {
			if (i == stats->last_index) {
				/*
				 * Prevent the reset_time read from occurring
				 * before the reset_pending read above.  */
				smp_rmb();
				time = local_clock() - READ_ONCE(stats->reset_time);
			} else {
				time = 0;
			}
		} else {
			time = stats->time_in_state[i];
			if (i == stats->last_index)
				time += local_clock() - stats->last_time;
		}

		pr_debug("%s: %u %llu\n", __func__, stats->freq_table[i],
			    nsec_to_clock_t(time));
		
		freqstat_elem.freq = stats->freq_table[i];
		freqstat_elem.time = nsec_to_clock_t(time);

		min_heap_push(&heap, &freqstat_elem, &freqstat_funcs);
	}

	freq = freqstat_heap[0].freq;
	return freq;
}

static void cpufreq_gov_mesh_limits(struct cpufreq_policy *policy)
{
	struct meshgov_policy *mg_policy = policy->governor_data;

	if (!policy->fast_switch_enabled) {
		mutex_lock(&mg_policy->work_lock);
		cpufreq_policy_apply_limits(policy);
		mutex_unlock(&mg_policy->work_lock);
	}
}

static bool meshgov_should_update_freq(struct meshgov_policy *mg_policy, u64 time)
{
	s64 delta_ns;

	delta_ns = time - mg_policy->last_freq_update_time;
	pr_debug("%s: delta_ns : %llu\n", __func__, delta_ns);

	return delta_ns >= MESHGOV_DELAY_NS;
}

static void meshgov_work(struct kthread_work *work)
{
	struct meshgov_policy *mg_policy = container_of(work, struct meshgov_policy, work);
	struct cpufreq_policy *policy = mg_policy->policy;
	int target_freq;

	mg_policy->work_in_progress = false;
	target_freq = cpufreq_get_mesh_time_in_state(policy);

	pr_debug("%s: called. target_freq(%d)\n", __func__, target_freq);
	if(policy->cur != target_freq) {
		pr_debug("%s: setting to %u kHz\n", __func__, target_freq);
		mutex_lock(&mg_policy->work_lock);
		__cpufreq_driver_target(policy, target_freq, CPUFREQ_RELATION_L);
		mutex_unlock(&mg_policy->work_lock);
	}
}

static void meshgov_irq_work(struct irq_work *irq_work)
{
	struct meshgov_policy *mg_policy;

	mg_policy = container_of(irq_work, struct meshgov_policy, irq_work);
	kthread_queue_work(&mg_policy->worker, &mg_policy->work);
}

static void meshgov_add_pm_qos_request(void)
{
	/* lit */
	if (!exynos_pm_qos_request_active(&meshgov_lit_min_qos)) {
		exynos_pm_qos_add_request(&meshgov_lit_min_qos,
				PM_QOS_CLUSTER0_FREQ_MIN, EXYNOS_PM_QOS_DEFAULT_VALUE);
	}
	if (!exynos_pm_qos_request_active(&meshgov_lit_max_qos)) {
		exynos_pm_qos_add_request(&meshgov_lit_max_qos,
				PM_QOS_CLUSTER0_FREQ_MAX, EXYNOS_PM_QOS_DEFAULT_VALUE);
	}

	/* mid */
	if (!exynos_pm_qos_request_active(&meshgov_mid_min_qos)) {
		exynos_pm_qos_add_request(&meshgov_mid_min_qos,
				PM_QOS_CLUSTER1_FREQ_MIN, EXYNOS_PM_QOS_DEFAULT_VALUE);
	}
	if (!exynos_pm_qos_request_active(&meshgov_mid_max_qos)) {
		exynos_pm_qos_add_request(&meshgov_mid_max_qos,
				PM_QOS_CLUSTER1_FREQ_MAX, EXYNOS_PM_QOS_DEFAULT_VALUE);
	}

	/* big */
	if (!exynos_pm_qos_request_active(&meshgov_big_min_qos)) {
		exynos_pm_qos_add_request(&meshgov_big_min_qos,
				PM_QOS_CLUSTER2_FREQ_MIN, EXYNOS_PM_QOS_DEFAULT_VALUE);
	}
	if (!exynos_pm_qos_request_active(&meshgov_big_max_qos)) {
		exynos_pm_qos_add_request(&meshgov_big_max_qos,
				PM_QOS_CLUSTER2_FREQ_MAX, EXYNOS_PM_QOS_DEFAULT_VALUE);
	}

	/* mif */
	if (!exynos_pm_qos_request_active(&meshgov_mif_min_qos)) {
		exynos_pm_qos_add_request(&meshgov_mif_min_qos,
				PM_QOS_BUS_THROUGHPUT, EXYNOS_PM_QOS_DEFAULT_VALUE);
	}
	if (!exynos_pm_qos_request_active(&meshgov_mif_max_qos)) {
		exynos_pm_qos_add_request(&meshgov_mif_max_qos,
				PM_QOS_BUS_THROUGHPUT_MAX, EXYNOS_PM_QOS_DEFAULT_VALUE);
	}

	/* int */
	if (!exynos_pm_qos_request_active(&meshgov_int_min_qos)) {
		exynos_pm_qos_add_request(&meshgov_int_min_qos,
				PM_QOS_DEVICE_THROUGHPUT, EXYNOS_PM_QOS_DEFAULT_VALUE);
	}
	if (!exynos_pm_qos_request_active(&meshgov_int_max_qos)) {
		exynos_pm_qos_add_request(&meshgov_int_max_qos,
				PM_QOS_DEVICE_THROUGHPUT_MAX, EXYNOS_PM_QOS_DEFAULT_VALUE);
	}

	/* cam */
	if (!exynos_pm_qos_request_active(&meshgov_cam_min_qos)) {
		exynos_pm_qos_add_request(&meshgov_cam_min_qos,
				PM_QOS_CAM_THROUGHPUT, EXYNOS_PM_QOS_DEFAULT_VALUE);
	}
	if (!exynos_pm_qos_request_active(&meshgov_cam_max_qos)) {
		exynos_pm_qos_add_request(&meshgov_cam_max_qos,
				PM_QOS_CAM_THROUGHPUT_MAX, EXYNOS_PM_QOS_DEFAULT_VALUE);
	}

	/* g3d */
	if (!exynos_pm_qos_request_active(&meshgov_g3d_min_qos)) {
		exynos_pm_qos_add_request(&meshgov_g3d_min_qos,
				PM_QOS_GPU_THROUGHPUT_MIN, EXYNOS_PM_QOS_DEFAULT_VALUE);
	}
	if (!exynos_pm_qos_request_active(&meshgov_g3d_max_qos)) {
		exynos_pm_qos_add_request(&meshgov_g3d_max_qos,
				PM_QOS_GPU_THROUGHPUT_MAX, EXYNOS_PM_QOS_DEFAULT_VALUE);
	}
}

static void meshgov_smpl_limit_freq(struct work_struct *work)
{
	pr_info("%s:\n", __func__);

	/* release smpl warn status */
	if(smpl_warn)
		smpl_warn = false;
}

static void meshgov_freq_util_work(struct work_struct *work)
{
	struct meshgov_tunables *tunables = container_of(work, struct meshgov_tunables, frequtil_work);
	struct meshgov_policy *mg_policy = tunables->mg_policy;
	struct cpufreq_policy *policy = mg_policy->policy;

	if (policy->cur == policy->min) {
		pr_debug("++%s: policy : %u cur : %d cpu : %d\n", __func__, policy->policy, policy->cur, policy->cpu);
		exynos_pm_qos_update_request(&meshgov_mid_min_qos, 0);
		exynos_pm_qos_update_request(&meshgov_mid_max_qos, 0);

		exynos_pm_qos_update_request(&meshgov_big_min_qos, 0);
		exynos_pm_qos_update_request(&meshgov_big_max_qos, 0);
		
		exynos_pm_qos_update_request(&meshgov_mif_min_qos, 0);
		exynos_pm_qos_update_request(&meshgov_mif_max_qos, 0);

		exynos_pm_qos_update_request(&meshgov_int_min_qos, 0);
		exynos_pm_qos_update_request(&meshgov_int_max_qos, 0);

		exynos_pm_qos_update_request(&meshgov_g3d_min_qos, 0);
		exynos_pm_qos_update_request(&meshgov_g3d_max_qos, 0);
	} else {
		pr_debug("--%s: policy : %u cur : %d cpu : %d\n", __func__, policy->policy, policy->cur, policy->cpu);
		exynos_pm_qos_update_request(&meshgov_mid_min_qos, PM_QOS_MIN_FREQUENCY_DEFAULT_VALUE);
		exynos_pm_qos_update_request(&meshgov_mid_max_qos, PM_QOS_MAX_FREQUENCY_DEFAULT_VALUE);

		exynos_pm_qos_update_request(&meshgov_big_min_qos, PM_QOS_MIN_FREQUENCY_DEFAULT_VALUE);
		exynos_pm_qos_update_request(&meshgov_big_max_qos, PM_QOS_MAX_FREQUENCY_DEFAULT_VALUE);
		
		exynos_pm_qos_update_request(&meshgov_mif_min_qos, PM_QOS_MIN_FREQUENCY_DEFAULT_VALUE);
		exynos_pm_qos_update_request(&meshgov_mif_max_qos, PM_QOS_MAX_FREQUENCY_DEFAULT_VALUE);

		exynos_pm_qos_update_request(&meshgov_int_min_qos, PM_QOS_MIN_FREQUENCY_DEFAULT_VALUE);
		exynos_pm_qos_update_request(&meshgov_int_max_qos, PM_QOS_MAX_FREQUENCY_DEFAULT_VALUE);

		exynos_pm_qos_update_request(&meshgov_g3d_min_qos, PM_QOS_MIN_FREQUENCY_DEFAULT_VALUE);
		exynos_pm_qos_update_request(&meshgov_g3d_max_qos, PM_QOS_MAX_FREQUENCY_DEFAULT_VALUE);
	}

	schedule_work(&tunables->frequtil_work);
}

static struct meshgov_tunables *global_tunables;
static DEFINE_MUTEX(global_tunables_lock);
static DEFINE_MUTEX(global_smpl_lock);

static inline struct meshgov_tunables *to_meshgov_tunables(struct gov_attr_set *attr_set)
{
	return container_of(attr_set, struct meshgov_tunables, attr_set);
}

static ssize_t mode_show(struct gov_attr_set *attr_set, char *buf)
{
	ssize_t count = 0;
	return sprintf(buf, "%ld\n", count);
}

static ssize_t mode_store(struct gov_attr_set *attr_set, const char *buf, size_t count)
{
	struct meshgov_tunables *tunables = to_meshgov_tunables(attr_set);

	meshgov_add_pm_qos_request();
	schedule_work(&tunables->frequtil_work);
	return count;
}

static ssize_t block_show(struct gov_attr_set *attr_set, char *buf)
{
	ssize_t count = 0;
	return sprintf(buf, "%ld\n", count);
}

static ssize_t block_store(struct gov_attr_set *attr_set, const char *buf, size_t count)
{
	return count;
}

static ssize_t range_show(struct gov_attr_set *attr_set, char *buf)
{
	ssize_t count = 0;
	return sprintf(buf, "%ld\n", count);
}

static ssize_t range_store(struct gov_attr_set *attr_set, const char *buf, size_t count)
{
	struct meshgov_tunables *tunables = to_meshgov_tunables(attr_set);
	struct cpufreq_stats *stats;
	struct meshgov_policy *mg_policy;
	struct cpufreq_policy *policy;
	int range, min_idx, max_idx, i;

	if(!sscanf(buf, "%d", &range))
		return -EINVAL;

	list_for_each_entry(mg_policy, &attr_set->policy_list, tunables_hook) {
		policy = mg_policy->policy;
		stats = policy->stats;

		/* find coverage of freq table */
		for (i = 0; i < stats->state_num; i++) {
			if(stats->freq_table[i] == policy->min)
				min_idx = i;

			if(stats->freq_table[i] == policy->max)
				max_idx = i;
		}

		switch (range) {
			case MG_MIN_MID:
				mg_policy->min_idx = min_idx;
				mg_policy->max_idx = (min_idx + max_idx) / 2;
				break;
			case MG_MID_MAX:
				mg_policy->min_idx = (min_idx + max_idx) / 2;
				mg_policy->max_idx = max_idx;
				break;
		}
	}

	tunables->min_freq = mg_policy->min_freq;
	tunables->max_freq = mg_policy->max_freq;

	return count;
}

void meshgov_limit_maxfreq_by_smpl(void)
{
	pr_info("%s:\n", __func__);

	mutex_lock(&smpl_mutex);
	schedule_delayed_work(&smpl_work, msecs_to_jiffies(5000)); // 5 secs

	if(!smpl_warn)
		smpl_warn = true;
	mutex_unlock(&smpl_mutex);
}
EXPORT_SYMBOL_GPL(meshgov_limit_maxfreq_by_smpl);

static struct governor_attr mode = __ATTR_RW(mode);
static struct governor_attr block = __ATTR_RW(block);
static struct governor_attr range = __ATTR_RW(range);

static struct attribute *meshgov_attrs[] = {
	&mode.attr,
	&block.attr,
	&range.attr,
	NULL
};
ATTRIBUTE_GROUPS(meshgov);

static struct kobj_type meshgov_tunables_ktype = {
	.default_groups = meshgov_groups,
	.sysfs_ops = &governor_sysfs_ops,
};

static int meshgov_kthread_create(struct meshgov_policy *mg_policy)
{
	struct task_struct *thread;
	struct sched_param param = { .sched_priority = MAX_RT_PRIO / 2 };
	struct cpufreq_policy *policy = mg_policy->policy;
	int ret;

	pr_info("%s:\n", __func__);

	if (policy->fast_switch_enabled)
		return 0;

	kthread_init_work(&mg_policy->work, meshgov_work);
	kthread_init_worker(&mg_policy->worker);
	thread = kthread_create(kthread_worker_fn, &mg_policy->worker,
			"meshgov:%d",
			cpumask_first(policy->related_cpus));

	if(IS_ERR(thread)) {
		pr_err("failed to create meshgov thread: %ld\n", PTR_ERR(thread));
		return PTR_ERR(thread);
	}

	ret = sched_setscheduler_nocheck(thread, SCHED_FIFO, &param);
	if (ret) {
		kthread_stop(thread);
		pr_warn("%s: failed to set SCHED_FIFO\n", __func__);
		return ret;
	}

	mg_policy->thread = thread;
	kthread_bind_mask(thread, policy->related_cpus);
	init_irq_work(&mg_policy->irq_work, meshgov_irq_work);
	mutex_init(&mg_policy->work_lock);

	wake_up_process(thread);
	return 0;
}

static void meshgov_get_freq_table_size(struct cpufreq_policy *policy, struct meshgov_policy *mg_policy)
{
	struct cpufreq_stats *stats = policy->stats;
	int state_num, max_idx, min_idx, i;

	max_idx = min_idx = 0;

	/* find coverage of freq table */
	for (i = 0; i < stats->state_num; i++) {
		if (stats->freq_table[i] == policy->min)
			min_idx = i;

		if (stats->freq_table[i] == policy->max)
			max_idx = i;
	}

	state_num = max_idx - min_idx + 1;

	mg_policy->min_idx = min_idx;
	mg_policy->max_idx = max_idx;
	mg_policy->table_size = state_num;

	pr_info("%s: min idx(%d, %u), max idx(%d, %u)\n", __func__,
			min_idx, policy->min, max_idx, policy->max);

}

static struct meshgov_policy *meshgov_policy_alloc(struct cpufreq_policy *policy)
{
	struct meshgov_policy *mg_policy;
	struct meshgov_freqstat *freqstat;

	pr_info("%s:\n", __func__);

	mg_policy = kzalloc(sizeof(*mg_policy), GFP_KERNEL);
	if(!mg_policy)
		return NULL;

	meshgov_get_freq_table_size(policy, mg_policy);
	freqstat = kzalloc(sizeof(struct meshgov_freqstat) * mg_policy->table_size, GFP_KERNEL);
	if (!freqstat)
		return NULL;

	mg_policy->freqstat = freqstat;
	mg_policy->policy = policy;
	raw_spin_lock_init(&mg_policy->update_lock);
	raw_spin_lock_init(&mg_policy->time_in_state_lock);

	return mg_policy;
}

static void meshgov_kthread_stop(struct meshgov_policy *mg_policy)
{
	if (mg_policy->policy->fast_switch_enabled)
		return;

	kthread_flush_worker(&mg_policy->worker);
	kthread_stop(mg_policy->thread);
	mutex_destroy(&mg_policy->work_lock);
}

static void meshgov_tunables_free(struct meshgov_tunables *tunables)
{
	if(!have_governor_per_policy())
		global_tunables = NULL;

	kfree(tunables);
}

static void meshgov_exit(struct cpufreq_policy *policy)
{
	struct meshgov_policy *mg_policy = policy->governor_data;
	struct meshgov_tunables *tunables = mg_policy->tunables;
	unsigned int count;

	pr_info("%s:\n", __func__);

	mutex_lock(&global_tunables_lock);

	count = gov_attr_set_put(&tunables->attr_set, &mg_policy->tunables_hook);
	policy->governor_data = NULL;

	if(!count)
		meshgov_tunables_free(tunables);

	mutex_unlock(&global_tunables_lock);

	cancel_delayed_work_sync(&smpl_work);
	meshgov_kthread_stop(mg_policy);
	kfree(mg_policy);
	cpufreq_disable_fast_switch(policy);
	policy->governor_data = NULL;
}

static void meshgov_update_shared(struct update_util_data *hook, u64 time, unsigned int flags)
{
	struct meshgov_cpu	*mgc = container_of(hook, struct meshgov_cpu, update_util);
	struct meshgov_policy *mg_policy = mgc->mg_policy;
	struct cpufreq_policy *policy = mg_policy->policy;
	int target_freq;
	
	raw_spin_lock(&mg_policy->update_lock);
	target_freq = cpufreq_get_mesh_time_in_state(policy);

	mgc->last_update = time;
	if (meshgov_should_update_freq(mg_policy, time)) {
		if (policy->cur != target_freq) {
			pr_debug("%s: setting to %u kHz min(%u) max(%u)\n", __func__,
					target_freq, policy->min, policy->max);

			mg_policy->last_freq_update_time = time;

			if (mg_policy->policy->fast_switch_enabled) {
				cpufreq_driver_fast_switch(mg_policy->policy, target_freq);
			} else {
				if (!mg_policy->work_in_progress) {
					mg_policy->work_in_progress = true;
					irq_work_queue(&mg_policy->irq_work);
				}
			}
		}	
	}
	raw_spin_unlock(&mg_policy->update_lock);
}

static int meshgov_start(struct cpufreq_policy *policy)
{
	struct meshgov_policy *mg_policy = policy->governor_data;
	unsigned int cpu;
	
	pr_info("%s:\n", __func__);

	mg_policy->work_in_progress = false;
	mg_policy->last_freq_update_time = 0;

	for_each_cpu(cpu, policy->cpus) {
		struct meshgov_cpu *mg_cpu = &per_cpu(meshgov_cpu, cpu);

		memset(mg_cpu, 0, sizeof(*mg_cpu));
		mg_cpu->cpu = cpu;
		mg_cpu->mg_policy = mg_policy;
	}

	for_each_cpu(cpu, policy->cpus) {
		struct meshgov_cpu *mgc = &per_cpu(meshgov_cpu, cpu);

		cpufreq_add_update_util_hook(cpu,
				&mgc->update_util, meshgov_update_shared);
	}
	return 0;
}

static void meshgov_stop(struct cpufreq_policy *policy)
{
	struct meshgov_policy *mg_policy = policy->governor_data;
	unsigned int cpu;
	
	pr_info("%s:\n", __func__);

	for_each_cpu(cpu, policy->cpus)
		cpufreq_remove_update_util_hook(cpu);

	synchronize_rcu();

	if (!policy->fast_switch_enabled) {
		irq_work_sync(&mg_policy->irq_work);
		kthread_cancel_work_sync(&mg_policy->work);
	}
}

static struct meshgov_tunables* meshgov_tunables_alloc(struct meshgov_policy *mg_policy)
{
	struct meshgov_tunables *tunables;

	tunables = kzalloc(sizeof(*tunables), GFP_KERNEL);

	if(tunables) {
		gov_attr_set_init(&tunables->attr_set, &mg_policy->tunables_hook);
		if(!have_governor_per_policy())
			global_tunables = tunables;
	}
	return tunables;
}

static int meshgov_init(struct cpufreq_policy *policy)
{
	struct meshgov_policy *mg_policy;
	struct meshgov_tunables *tunables;
	int ret = 0;

	pr_info("%s:\n", __func__);

	if(policy->governor_data)
		return -EBUSY;

	cpufreq_enable_fast_switch(policy);

	mg_policy = meshgov_policy_alloc(policy);
	if(!mg_policy) {
		ret = -ENOMEM;
		goto disable_fast_switch;
	}

	if(meshgov_kthread_create(mg_policy)) {
		pr_err("%s: failed to create thread\n", __func__);
		ret = -1;
		goto free_mg_policy;
	}

	mutex_lock(&global_tunables_lock);

	if(global_tunables) {
		if (WARN_ON(have_governor_per_policy())) {
			ret = -EINVAL;
			goto stop_kthread;
		}
		policy->governor_data = mg_policy;
		mg_policy->tunables = global_tunables;

		gov_attr_set_get(&global_tunables->attr_set, &mg_policy->tunables_hook);
		goto out;
	}

	tunables = meshgov_tunables_alloc(mg_policy);

	if(!tunables) {
		ret = -ENOMEM;
		goto free_mg_policy;
	}

	INIT_WORK(&tunables->frequtil_work, meshgov_freq_util_work);
	INIT_DELAYED_WORK(&smpl_work, meshgov_smpl_limit_freq);
	smpl_warn = false;
	policy->governor_data = mg_policy;
	tunables->mg_policy = mg_policy; // add
	mg_policy->tunables = tunables;

	ret = kobject_init_and_add(&tunables->attr_set.kobj, &meshgov_tunables_ktype,
			get_governor_parent_kobj(policy), "%s", "mesh");

	if(ret)
		goto fail;
out:
	mutex_unlock(&global_tunables_lock);
	return 0;

fail:
	kobject_put(&tunables->attr_set.kobj);
	policy->governor_data = NULL;
	meshgov_tunables_free(tunables);

stop_kthread:
	meshgov_kthread_stop(mg_policy);
	mutex_unlock(&global_tunables_lock);

free_mg_policy:
	kfree(mg_policy);

disable_fast_switch:
	cpufreq_disable_fast_switch(policy);
	pr_err("initialize failed. (err : %d)\n", ret);
	return ret;
}

static struct cpufreq_governor cpufreq_gov_mesh = {
	.name		= "mesh",
	.owner		= THIS_MODULE,
	.flags		= CPUFREQ_GOV_DYNAMIC_SWITCHING,
	.init		= meshgov_init,
	.exit		= meshgov_exit,
	.start		= meshgov_start,
	.stop		= meshgov_stop,
	.limits		= cpufreq_gov_mesh_limits,
};

#ifdef CONFIG_CPU_FREQ_DEFAULT_GOV_MESH
struct cpufreq_governor *cpufreq_default_governor(void)
{
	return &cpufreq_gov_mesh;
}
#endif

MODULE_AUTHOR("Hyeokseon Yu <hyeokseon.yu@samsung.com>");
MODULE_DESCRIPTION("CPUfreq policy mesh governor");
MODULE_LICENSE("GPL");

cpufreq_governor_init(cpufreq_gov_mesh);
cpufreq_governor_exit(cpufreq_gov_mesh);
