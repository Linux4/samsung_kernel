/*
 * drivers/cpufreq/mmp-bL-cpufreq.c
 *
 * Copyright (C) 2014 Marvell Semiconductors Inc.
 *
 * Author: Bill Zhou <zhoub@marvell.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/clk.h>
#include <linux/cpufreq.h>
#include <linux/cpumask.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/sched.h>
#include <linux/suspend.h>
#include <linux/pm_qos.h>
#include <linux/clk-private.h>

#ifdef CONFIG_DEBUG_FS
#include <linux/debugfs.h>
#include <linux/debugfs-pxa.h>
#endif

/* Unit of various frequencies:
 * clock driver: Hz
 * cpufreq framework/driver & QoS framework/interface: KHz
 */

/* FIXME: too simple and may be conflicted with other clock names. */
#define CLUSTER_CLOCK_NAME	"clst"

/*
 * clients that requests cpufreq Qos min:
 * cpufreq target callback, input boost, wifi/BT/..., policy->cpuinfo.min
 * clients that requests cpufreq Qos max:
 * cpufreq Qos min, thermal driver, policy->cpuinfo.max
 */
/* used to generate names to request cpufreq Qos min from profiler */
#define PROFILER_REQ_MIN	"PROFILER_REQ_MIN_"
/* used to generate names to request cpufreq Qos max from cpufreq Qos min notifier */
#define QOSMIN_REQ_MAX		"QOSMIN_REQ_MAX_"
/* used to generate names to request cpufreq Qos min/max from policy->cpuinfo.min/max */
#define CPUFREQ_POLICY_REQ	"CPUFREQ_POLICY_REQ_"
/* used to generate names to request voltage level Qos min */
#define VL_REQ_MIN		"VL_REQ_MIN_"

/* NOTE:
 * Here, we don't assume the cluster order in system as below, but just
 * define what types of clusters we need handle.
 */
enum cluster_type {
	LITTLE_CLST = 0,
	BIG_CLST,
	NR_CLST_TYPE,
};

static int cpufreq_qos_min_id[NR_CLST_TYPE] = {
	PM_QOS_CPUFREQ_L_MIN,
	PM_QOS_CPUFREQ_B_MIN,
};

static int cpufreq_qos_max_id[NR_CLST_TYPE] = {
	PM_QOS_CPUFREQ_L_MAX,
	PM_QOS_CPUFREQ_B_MAX,
};

struct cpufreq_cluster {
	int initialized; /* whether this struct has been initialized */
	int clst_index;
	int is_big; /* all cores in this cluster are big or not */
	int nr_cores; /* core number in this cluster */
	int first_cpu_id; /* the first cpu id in this cluster */

	char *clk_name;
	struct clk *clk;

	struct cpufreq_policy *policy;
	struct cpufreq_frequency_table *freq_table;
	int freq_table_size;

	int cpufreq_qos_max_id;
	int cpufreq_qos_min_id;
	int vl_qos_min_id;
	struct pm_qos_request profiler_req_min;
	struct pm_qos_request qosmin_req_max;
	struct pm_qos_request cpufreq_policy_qos_req_min;
	struct pm_qos_request cpufreq_policy_qos_req_max;
	struct pm_qos_request vl_qos_req_min;

	struct notifier_block cpufreq_min_notifier;
	struct notifier_block cpufreq_max_notifier;
	struct notifier_block cpufreq_policy_notifier;
	struct notifier_block vl_min_notifier;

	struct list_head node;
};
static LIST_HEAD(clst_list);
#define min_nb_to_clst(ptr) (container_of(ptr, struct cpufreq_cluster, cpufreq_min_notifier))
#define max_nb_to_clst(ptr) (container_of(ptr, struct cpufreq_cluster, cpufreq_max_notifier))
#define policy_nb_to_clst(ptr) (container_of(ptr, struct cpufreq_cluster, cpufreq_policy_notifier))
#define vl_nb_to_clst(ptr) (container_of(ptr, struct cpufreq_cluster, vl_min_notifier))

/* Locks used to serialize cpufreq target request. */
static DEFINE_MUTEX(mmp_cpufreq_lock);

static char *__cpufreq_printf(const char *fmt, ...)
{
	va_list vargs;
	char *buf;

	va_start(vargs, fmt);
	buf = kvasprintf(GFP_KERNEL, fmt, vargs);
	va_end(vargs);

	return buf;
}

/*
 * We have two modes to handle cpufreq on dual clusters:
 * 1. When there are no limitations from policy, thermal, etc, frequencies
 *    of dual clusters are always aligned to the same voltage level.
 * 2. Each cluster can change its frequency freely.
 *
 * We can
 * append "vl_cpufreq=1 (or 0)" to uboot cmdline
 * or "echo 1 (or 0) > /sys/kernel/debug/pxa/vl_cpufreq"
 * to enable or disable voltage based cpufreq.
 */
/* For LITTLE min lock issue */
static unsigned int vl_cpufreq_enable = 0;
static int __init __cpufreq_mode_setup(char *str)
{
	unsigned int n;

	if (!get_option(&str, &n))
		return 0;

	if (n < 2)
		vl_cpufreq_enable = n;

	return 1;
}
__setup("vl_cpufreq=", __cpufreq_mode_setup);

/*
 * This function comes from cpufreq_frequency_table_target, but remove
 *         if ((freq < policy->min) || (freq > policy->max))
 *             continue;
 * to make sure critical setrate request will not be skipped.
 * For example, thermal wants to set max qos to 312MHz, but
 * policy->min is 624MHz. Then it fails to set rate to 312MHz.
 */
static int __cpufreq_frequency_table_target(
			struct cpufreq_cluster *clst,
			unsigned int target_freq,
			unsigned int relation,
			unsigned int *index)
{
	struct cpufreq_frequency_table *table = clst->freq_table;
	struct cpufreq_frequency_table optimal = {
		.driver_data = ~0,
		.frequency = 0,
	};
	struct cpufreq_frequency_table suboptimal = {
		.driver_data = ~0,
		.frequency = 0,
	};
	unsigned int i;

	switch (relation) {
	case CPUFREQ_RELATION_H:
		suboptimal.frequency = ~0;
		break;
	case CPUFREQ_RELATION_L:
		optimal.frequency = ~0;
		break;
	}

	for (i = 0; (table[i].frequency != CPUFREQ_TABLE_END); i++) {
		unsigned int freq = table[i].frequency;
		if (freq == CPUFREQ_ENTRY_INVALID)
			continue;
/*
		if ((freq < policy->min) || (freq > policy->max))
			continue;
*/
		switch (relation) {
		case CPUFREQ_RELATION_H:
			if (freq <= target_freq) {
				if (freq >= optimal.frequency) {
					optimal.frequency = freq;
					optimal.driver_data = i;
				}
			} else {
				if (freq <= suboptimal.frequency) {
					suboptimal.frequency = freq;
					suboptimal.driver_data = i;
				}
			}
			break;
		case CPUFREQ_RELATION_L:
			if (freq >= target_freq) {
				if (freq <= optimal.frequency) {
					optimal.frequency = freq;
					optimal.driver_data = i;
				}
			} else {
				if (freq >= suboptimal.frequency) {
					suboptimal.frequency = freq;
					suboptimal.driver_data = i;
				}
			}
			break;
		}
	}
	if (optimal.driver_data > i) {
		if (suboptimal.driver_data > i)
			return -EINVAL;
		*index = suboptimal.driver_data;
	} else {
		*index = optimal.driver_data;
	}
	return 0;
}

/* check whether all cores in a cluster are offline. */
static int __cpufreq_clst_allcores_off(struct cpufreq_cluster *clst)
{
	int i;

	for (i = clst->first_cpu_id; i < (clst->first_cpu_id + clst->nr_cores); i++) {
		if (cpu_online(i))
			return 0;
	}
	return 1;
}

static void __cpufreq_update_clst_topology(struct cpufreq_cluster *clst)
{
/* FIXME:
 * Don't assume the cluster order and core number in each cluster.
 * For example, Versatile Express TC2 has 2 Cortex-A15 and 3 Cortex-A7.
 * Now we just add hardcode here, but we need retrieve the information
 * from device tree or GTS configuration.
 */
	if (clst->clst_index == 1) {
		clst->is_big = 1;
		clst->nr_cores = 4;
		clst->first_cpu_id = 4;
	} else {
		clst->is_big = 0;
		clst->nr_cores = 4;
		clst->first_cpu_id = 0;
	}
}

/* map a target freq to its voltage level */
static unsigned int __cpufreq_freq_2_vl(
			struct cpufreq_cluster *clst,
			unsigned int freq)
{
	int i;

	if (freq >= clst->freq_table[clst->freq_table_size - 1].frequency)
		return clst->freq_table[clst->freq_table_size - 1].driver_data;

	for (i = clst->freq_table_size - 1; i >= 1; i--)
		if ((clst->freq_table[i - 1].frequency < freq) && (freq <= clst->freq_table[i].frequency))
			return clst->freq_table[i].driver_data;

	/*
	 * When target freq < all other freqs, or any other condition,
	 * return the lowest volt level.
	 */
	return clst->freq_table[0].driver_data;
}

/* map a voltage level to the max freq it could support */
static unsigned int __cpufreq_vl_2_max_freq(
			struct cpufreq_cluster *clst,
			unsigned int vl)
{
	int i;

	for (i = clst->freq_table_size - 1; i >= 0; i--)
		if (clst->freq_table[i].driver_data <= vl)
			return clst->freq_table[i].frequency;
	return clst->freq_table[0].frequency;
}

static int __cpufreq_freq_min_notify(
			struct notifier_block *nb,
			unsigned long min,
			void *v)
{
	int ret, index;
	unsigned int volt_level;
	struct cpufreq_cluster *clst;

	if (!nb)
		return NOTIFY_BAD;
	clst = min_nb_to_clst(nb);

	/* CPUFREQ_RELATION_L means "at or above target":
	 * i.e:
	 * If target freq is 400MHz, we round up to the closest 416MHz in freq table.
	 */
	ret = __cpufreq_frequency_table_target(clst, min, CPUFREQ_RELATION_L, &index);
	if (ret != 0) {
		pr_err("%s: cannot get a valid index from freq_table\n", __func__);
		return NOTIFY_BAD;
	}

	if (vl_cpufreq_enable) {
		volt_level = __cpufreq_freq_2_vl(clst, clst->freq_table[index].frequency);
		if (pm_qos_request_active(&clst->vl_qos_req_min))
			pm_qos_update_request(&clst->vl_qos_req_min, volt_level);
	} else {
		if (pm_qos_request_active(&clst->qosmin_req_max))
			pm_qos_update_request(&clst->qosmin_req_max, clst->freq_table[index].frequency);
	}
	return NOTIFY_OK;
}

static int __cpufreq_vl_min_notify(
			struct notifier_block *nb,
			unsigned long vl_min,
			void *v)
{
	unsigned long freq = 0;
	struct cpufreq_cluster *clst;

	if (!nb)
		return NOTIFY_BAD;

	if (!list_empty(&clst_list)) {
		list_for_each_entry(clst, &clst_list, node) {
			freq = __cpufreq_vl_2_max_freq(clst, vl_min);
			if (pm_qos_request_active(&clst->qosmin_req_max))
				pm_qos_update_request(&clst->qosmin_req_max, freq);
		}
	}

	return NOTIFY_OK;
}

int cpufreq_lfreq_index;
int cpufreq_lfreq_table_size;

static int __cpufreq_freq_max_notify(
			struct notifier_block *nb,
			unsigned long max,
			void *v)
{
	int ret, index;
	struct cpufreq_cluster *clst;
	struct cpufreq_freqs freqs;
	struct cpufreq_policy *policy;

	if (!nb)
		return NOTIFY_BAD;
	clst = max_nb_to_clst(nb);

	if(__cpufreq_clst_allcores_off(clst))
		return NOTIFY_BAD;

	/* CPUFREQ_RELATION_H means "below or at target":
	 * i.e:
	 * If target freq is 400MHz, we round down to the closest 312MHz in freq table.
	 */
	ret = __cpufreq_frequency_table_target(clst, max, CPUFREQ_RELATION_H, &index);
	if (ret != 0) {
		pr_err("%s: cannot get a valid index from freq_table\n", __func__);
		return NOTIFY_BAD;
	}

	policy = clst->policy;
	freqs.old = policy->cur;
	freqs.new = clst->freq_table[index].frequency;

	cpufreq_notify_transition(policy, &freqs, CPUFREQ_PRECHANGE);

	if (!clk_set_rate(policy->clk, freqs.new * 1000)) {
		ret = NOTIFY_OK;
		if (clst->clst_index == 0)
			cpufreq_lfreq_index = index;
	} else {
		freqs.new = freqs.old;
		ret = NOTIFY_BAD;
	}

	cpufreq_notify_transition(policy, &freqs, CPUFREQ_POSTCHANGE);

	return ret;
}

static int __cpufreq_policy_notify(
			struct notifier_block *nb,
			unsigned long val,
			void *data)
{
	struct cpufreq_policy *policy = data;
	struct cpufreq_cluster *clst;

	if (!nb)
		return NOTIFY_BAD;
	clst = policy_nb_to_clst(nb);

	if ((policy->clk == clst->clk) && (val == CPUFREQ_ADJUST)) {
		pm_qos_update_request(&clst->cpufreq_policy_qos_req_min, policy->min);
		pm_qos_update_request(&clst->cpufreq_policy_qos_req_max, policy->max);
	}
	return NOTIFY_OK;
}

static unsigned int mmp_bL_cpufreq_get(unsigned int cpu)
{
	int clst_index = topology_physical_package_id(cpu);
	struct cpufreq_cluster *clst;

	list_for_each_entry(clst, &clst_list, node)
		if (clst->clst_index == clst_index)
			break;
	BUG_ON(!clst);
	return clk_get_rate(clst->clk) / 1000;
}

/*
 * We don't handle param "relation" here, but use "CPUFREQ_RELATION_L" in
 * cpufreq qos min notifier to round up and use "CPUFREQ_RELATION_H" in
 * cpufreq qos max notifier to round down. This method can meet our
 * requirement.
 */
static int mmp_bL_cpufreq_target(
			struct cpufreq_policy *policy,
			unsigned int target_freq,
			unsigned int relation)
{
	int ret = 0;
	struct cpufreq_cluster *clst;

	list_for_each_entry(clst, &clst_list, node)
		if (clst->policy == policy)
			break;
	BUG_ON(!clst);

	mutex_lock(&mmp_cpufreq_lock);
	pm_qos_update_request(&clst->profiler_req_min, target_freq);
	mutex_unlock(&mmp_cpufreq_lock);

	return ret;
}

static int mmp_bL_cpufreq_init(struct cpufreq_policy *policy)
{
	int i = 0, ret, clst_index, found = 0;
	unsigned long cur_freq;
	struct cpufreq_cluster *clst;

	if (policy->cpu >= num_possible_cpus())
		return -EINVAL;

	clst_index = topology_physical_package_id(policy->cpu);
	pr_debug("%s: start initialization on cluster-%d\n", __func__, clst_index);

	if (!list_empty(&clst_list))
		list_for_each_entry(clst, &clst_list, node)
			if (clst->clst_index == clst_index) {
				found = 1;
				break;
			}

	if (!found) {
		clst = kzalloc(sizeof(struct cpufreq_cluster), GFP_KERNEL);
		if (!clst) {
			pr_err("%s: fail to allocate memory for clst.\n", __func__);
			return -ENOMEM;
		}
	}

	if (!clst->initialized) {
		char clst_id[2];
		clst->clst_index = clst_index;
		__cpufreq_update_clst_topology(clst);

		clst->clk_name = __cpufreq_printf("%s%d", CLUSTER_CLOCK_NAME, clst_index);
		if (!clst->clk_name) {
			pr_err("%s: fail to allocate memory for clock name.\n", __func__);
			goto err_out;
		}

		clst->clk = __clk_lookup(clst->clk_name);
		if (!clst->clk) {
			pr_err("%s: fail to get clock for cluster-%d\n", __func__, clst_index);
			goto err_out;
		}

		clst->freq_table = cpufreq_frequency_get_table(policy->cpu);

		/* get freq table size from cpufreq_frequency_table structure. */
		while (clst->freq_table[i++].frequency != CPUFREQ_TABLE_END)
			;
		/* driver_data in last entry save freq table size, but that of other entries store voltage level. */
		clst->freq_table_size = clst->freq_table[i - 1].driver_data;

		if (clst->is_big) {
			strcpy(clst_id, "B");
			clst->cpufreq_qos_min_id = cpufreq_qos_min_id[BIG_CLST];
			clst->cpufreq_qos_max_id = cpufreq_qos_max_id[BIG_CLST];
		} else {
			strcpy(clst_id, "L");
			clst->cpufreq_qos_min_id = cpufreq_qos_min_id[LITTLE_CLST];
			clst->cpufreq_qos_max_id = cpufreq_qos_max_id[LITTLE_CLST];
			cpufreq_lfreq_table_size = clst->freq_table_size;
		}
		clst->vl_qos_min_id = PM_QOS_CLST_VL_MIN;

		clst->profiler_req_min.name = __cpufreq_printf("%s%s", PROFILER_REQ_MIN, clst_id);
		if (!clst->profiler_req_min.name) {
			pr_err("%s: no mem for profiler_req_min.name.\n", __func__);
			goto err_out;
		}

		clst->qosmin_req_max.name = __cpufreq_printf("%s%s", QOSMIN_REQ_MAX, clst_id);
		if (!clst->qosmin_req_max.name) {
			pr_err("%s: no mem for qosmin_req_max.name.\n", __func__);
			goto err_out;
		}

		if (vl_cpufreq_enable) {
			clst->vl_qos_req_min.name = __cpufreq_printf("%s%s", VL_REQ_MIN, clst_id);
			if (!clst->vl_qos_req_min.name) {
				pr_err("%s: no mem for vl_qos_req_min.name.\n", __func__);
				goto err_out;
			}
		}

		clst->cpufreq_policy_qos_req_min.name = __cpufreq_printf("%s%s%s", CPUFREQ_POLICY_REQ, "MIN_", clst_id);
		if (!clst->cpufreq_policy_qos_req_min.name) {
			pr_err("%s: no mem for cpufreq_policy_qos_req_min.name.\n", __func__);
			goto err_out;
		}

		clst->cpufreq_policy_qos_req_max.name = __cpufreq_printf("%s%s%s", CPUFREQ_POLICY_REQ, "MAX_", clst_id);
		if (!clst->cpufreq_policy_qos_req_max.name) {
			pr_err("%s: no mem for cpufreq_policy_qos_req_max.name.\n", __func__);
			goto err_out;
		}

		clst->cpufreq_min_notifier.notifier_call = __cpufreq_freq_min_notify;
		clst->cpufreq_max_notifier.notifier_call = __cpufreq_freq_max_notify;
		if (vl_cpufreq_enable)
			clst->vl_min_notifier.notifier_call = __cpufreq_vl_min_notify;
		clst->cpufreq_policy_notifier.notifier_call = __cpufreq_policy_notify;

		pm_qos_add_notifier(clst->cpufreq_qos_min_id, &clst->cpufreq_min_notifier);
		pm_qos_add_notifier(clst->cpufreq_qos_max_id, &clst->cpufreq_max_notifier);
		if (vl_cpufreq_enable)
			pm_qos_add_notifier(clst->vl_qos_min_id, &clst->vl_min_notifier);
		cpufreq_register_notifier(&clst->cpufreq_policy_notifier, CPUFREQ_POLICY_NOTIFIER);

	}

	ret = cpufreq_table_validate_and_show(policy, clst->freq_table);
	if (ret) {
		pr_err("%s: invalid frequency table for cluster-%d\n", __func__, clst_index);
		goto err_out;
	}
	cpumask_copy(policy->cpus, topology_core_cpumask(policy->cpu));
	cur_freq = clk_get_rate(clst->clk) / 1000;
	policy->clk = clst->clk;
	policy->cpuinfo.transition_latency = 10 * 1000;
	policy->cur = cur_freq;
	clst->policy = policy;

	if (!clst->initialized) {
		pm_qos_add_request(&clst->profiler_req_min, clst->cpufreq_qos_min_id, policy->cur);
		pm_qos_add_request(&clst->qosmin_req_max, clst->cpufreq_qos_max_id, pm_qos_request(clst->cpufreq_qos_min_id));
		if (vl_cpufreq_enable)
			pm_qos_add_request(&clst->vl_qos_req_min, clst->vl_qos_min_id, __cpufreq_freq_2_vl(clst, cur_freq));
		pm_qos_add_request(&clst->cpufreq_policy_qos_req_min, clst->cpufreq_qos_min_id, policy->min);
		pm_qos_add_request(&clst->cpufreq_policy_qos_req_max, clst->cpufreq_qos_max_id, policy->max);

		clst->initialized = 1;
		list_add_tail(&clst->node, &clst_list);

		pr_info("vl_cpufreq %s\n", (vl_cpufreq_enable == 1) ? "enabled" : "disabled");
	}

	pr_debug("%s: finish initialization on cluster-%d\n", __func__, clst_index);

	return 0;

err_out:

	if (clst) {
		kfree(clst->clk_name);
		kfree(clst->profiler_req_min.name);
		kfree(clst->qosmin_req_max.name);
		if (vl_cpufreq_enable)
			kfree(clst->vl_qos_req_min.name);
		kfree(clst->cpufreq_policy_qos_req_min.name);
		kfree(clst->cpufreq_policy_qos_req_max.name);
		kfree(clst);
	}
	return -EINVAL;
}

static int mmp_bL_cpufreq_exit(struct cpufreq_policy *policy)
{
	return 0;
}

static struct cpufreq_driver mmp_bL_cpufreq_driver = {
	.name = "mmp-bL-cpufreq",
	.flags = CPUFREQ_HAVE_GOVERNOR_PER_POLICY,
	.verify = cpufreq_generic_frequency_table_verify,
	.get = mmp_bL_cpufreq_get,
	.target = mmp_bL_cpufreq_target,
	.init = mmp_bL_cpufreq_init,
	.exit = mmp_bL_cpufreq_exit,
	.attr = cpufreq_generic_attr,
};

static struct dentry *vl_cpufreq;

static int __init mmp_bL_cpufreq_register(void)
{
#ifdef CONFIG_DEBUG_FS
	vl_cpufreq = debugfs_create_u32("vl_cpufreq", 0664, pxa, &vl_cpufreq_enable);
	if (vl_cpufreq == NULL)
		pr_err("%s: Error to create file node vl_cpufreq.\n", __func__);
#endif
	return cpufreq_register_driver(&mmp_bL_cpufreq_driver);
}

static void __exit mmp_bL_cpufreq_unregister(void)
{
#ifdef CONFIG_DEBUG_FS
	if (vl_cpufreq)
		debugfs_remove(vl_cpufreq);
#endif
	cpufreq_unregister_driver(&mmp_bL_cpufreq_driver);
}

MODULE_DESCRIPTION("cpufreq driver for Marvell MMP SoC w/ bL arch");
MODULE_LICENSE("GPL");
module_init(mmp_bL_cpufreq_register);
module_exit(mmp_bL_cpufreq_unregister);
