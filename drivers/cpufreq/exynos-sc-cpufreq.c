/*
 * copyright (c) 2010-2015 samsung electronics co., ltd.
 *		http://www.samsung.com
 *
 * exynos - cpu frequency scaling support for exynos single cluster series
 *
 * this program is free software; you can redistribute it and/or modify
 * it under the terms of the gnu general public license version 2 as
 * published by the free software foundation.
*/

#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <linux/regulator/consumer.h>
#include <linux/cpufreq.h>
#include <linux/suspend.h>
#include <linux/module.h>
#include <linux/reboot.h>
#include <linux/delay.h>
#include <linux/cpu.h>
#include <linux/pm_qos.h>
#include <linux/kobject.h>
#include <linux/sysfs.h>
#include <linux/cpumask.h>
#include <linux/exynos-ss.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/apm-exynos.h>

#include <asm/smp_plat.h>
#include <asm/cputype.h>

#include <soc/samsung/cpufreq.h>
#include <soc/samsung/exynos-powermode.h>
#include <soc/samsung/asv-exynos.h>
#include <soc/samsung/tmu.h>
#include <soc/samsung/ect_parser.h>

#ifdef CONFIG_SEC_EXT
#include <linux/sec_ext.h>
#endif

#ifdef CONFIG_SW_SELF_DISCHARGING
#include <linux/cpuidle.h>
#endif

#define FREQ_OSCCLK                     26000

static struct exynos_dvfs_info *exynos_info;
static struct cpufreq_freqs *freqs;
static const char *regulator_name;
static unsigned int freq_min __read_mostly;     /* Minimum clock frequency */
static unsigned int freq_max __read_mostly;     /* Maximum clock frequency */

static struct cpumask cluster_cpumask;		/* Include cpus of cluster */

static struct pm_qos_constraints core_max_qos_const;

static struct pm_qos_request boot_min_qos;
static struct pm_qos_request boot_max_qos;
static struct pm_qos_request core_min_qos;
static struct pm_qos_request core_max_qos;
static struct pm_qos_request core_min_qos_real;
static struct pm_qos_request core_max_qos_real;
static struct pm_qos_request exynos_mif_qos;
static struct pm_qos_request reboot_max_qos;

static int qos_max_class = PM_QOS_CLUSTER0_FREQ_MAX;
static int qos_min_class = PM_QOS_CLUSTER0_FREQ_MIN;
static int qos_min_default_value = PM_QOS_CLUSTER0_FREQ_MIN_DEFAULT_VALUE;

static DEFINE_MUTEX(cpufreq_lock);

#ifdef CONFIG_SW_SELF_DISCHARGING
static int self_discharging;
#endif

static unsigned int clk_get_freq(void)
{
        if (exynos_info->get_freq)
                return exynos_info->get_freq();

        pr_err("CL0 : clk_get_freq failed\n");

        return 0;
}

#ifdef CONFIG_SEC_BOOTSTAT
void sec_bootstat_get_cpuinfo(int *freq, int *online)
{
	int cpu;
	struct cpufreq_policy *policy;

	get_online_cpus();
	*online = cpumask_bits(cpu_online_mask)[0];
	for_each_online_cpu(cpu) {
		if (freq[0] == 0) {
			policy = cpufreq_cpu_get(cpu);
			if (!policy)
				continue;
			freq[0] = policy->cur;
			cpufreq_cpu_put(policy);
		}
	}
	put_online_cpus();
}
#endif

static void set_boot_freq(void)
{
	if (exynos_info) {
		exynos_info->boot_freq = clk_get_freq();
		pr_info("CL0 BootFreq: %dKHz\n", exynos_info->boot_freq);
	}
}

int exynos_verify_speed(struct cpufreq_policy *policy)
{
        return cpufreq_frequency_table_verify(policy, exynos_info->freq_table);
}

unsigned int exynos_getspeed_cluster(void)
{
        return clk_get_freq();
}

unsigned int exynos_getspeed(unsigned int cpu)
{
	/*
	 * In single cluster, we do not need to check cluster idle state
	 * because this function is executed by some core in other words,
	 * It means that cluster is running.
	 */
	return exynos_getspeed_cluster();
}

static void __free_info(void *objp)
{
	if (objp)
		kfree(objp);
}

static void free_info(void)
{
	__free_info(exynos_info->bus_table);
	__free_info(exynos_info->volt_table);
	__free_info(exynos_info->freq_table);

	__free_info(exynos_info);
	__free_info(freqs);
}


static int alloc_table(struct exynos_dvfs_info *ptr)
{
        int table_size = ptr->max_idx_num;

        ptr->freq_table = kzalloc(sizeof(struct cpufreq_frequency_table)
                                        * (table_size + 1), GFP_KERNEL);
        if (ptr->freq_table == NULL) {
                pr_err("%s: failed to allocate memory for freq_table\n", __func__);
                return -ENOMEM;
        }

        ptr->volt_table = kzalloc(sizeof(unsigned int) * table_size, GFP_KERNEL);
        if (ptr->volt_table == NULL) {
                pr_err("%s: failed to allocate for volt_table\n", __func__);
                kfree(ptr->freq_table);
                return -ENOMEM;
        }

        ptr->bus_table = kzalloc(sizeof(unsigned int) * table_size, GFP_KERNEL);
        if (ptr->bus_table == NULL) {
                pr_err("%s: failed to allocate for bus_table\n", __func__);
                kfree(ptr->freq_table);
                kfree(ptr->volt_table);
                return -ENOMEM;
        }

        return 0;
}

#if defined(CONFIG_ECT)
static void exynos_cpufreq_parse_miflock(char *cluster_name,
                                unsigned int frequency, unsigned int *mif_freq)
{
        int i;
        void *cpumif_block;
        struct ect_minlock_domain *domain;

        cpumif_block = ect_get_block(BLOCK_MINLOCK);
        if (cpumif_block == NULL)
                return;

        domain = ect_minlock_get_domain(cpumif_block, cluster_name);
        if (domain == NULL)
                return;

        for (i = 0; i < domain->num_of_level; ++i) {
                if (domain->level[i].main_frequencies == frequency) {
                        *mif_freq = domain->level[i].sub_frequencies;
                        return;
                }
        }

        *mif_freq = 0;
        return;
}

static int exynos_cpufreq_parse_frequency(char *cluster_name,
                                          struct exynos_dvfs_info *ptr)
{
        int ret, i;
        void *dvfs_block;
        struct ect_dvfs_domain *domain;

        dvfs_block = ect_get_block(BLOCK_DVFS);
        if (dvfs_block == NULL)
                return -ENODEV;

        domain = ect_dvfs_get_domain(dvfs_block, cluster_name);
        if (domain == NULL)
                return -ENODEV;

        ptr->max_idx_num = domain->num_of_level;

        ret = alloc_table(ptr);
        if (ret)
                return ret;

	for (i = 0; i < domain->num_of_level; ++i) {
		ptr->freq_table[i].driver_data = i;
		ptr->freq_table[i].frequency = domain->list_level[i].level;
		ptr->volt_table[i] = 0;
		exynos_cpufreq_parse_miflock(cluster_name,
				ptr->freq_table[i].frequency, &ptr->bus_table[i]);
	}

	ptr->resume_freq = ptr->freq_table[domain->resume_level_idx].frequency;
	pr_info("CL0 ResumeFreq: %dKHz\n", ptr->resume_freq);

        return 0;
}
#endif

static int exynos_cpufreq_parse_dt(struct device_node *np)
{
	struct exynos_dvfs_info *ptr = exynos_info;
        const char *cluster_domain_name;

        if (!np) {
		pr_info("%s: cpufreq_dt is not existed. \n", __func__);
                return -ENODEV;
	}

        if (of_property_read_u32(np, "cl0_idx_num", &ptr->max_idx_num))
                return -ENODEV;

	pr_info("max_idx_num = %d\n", ptr->max_idx_num);

	if (of_property_read_u32(np,
				"cl0_max_support_idx", &ptr->max_support_idx))
                return -ENODEV;

	pr_info("max_support_idx = %d\n", ptr->max_support_idx);

        if (of_property_read_u32(np,
				"cl0_min_support_idx", &ptr->min_support_idx))
                return -ENODEV;

        if (of_property_read_u32(np,
				"cl0_boot_max_qos", &ptr->boot_max_qos))
                return -ENODEV;

        if (of_property_read_u32(np, "cl0_boot_min_qos", &ptr->boot_min_qos))
                return -ENODEV;

        if (of_property_read_u32(np,
				"cl0_boot_lock_time", &ptr->boot_lock_time))
                return -ENODEV;

        if (of_property_read_u32(np, "cl0_en_ema", &ptr->en_ema))
                return -ENODEV;

        if (of_property_read_string(np, "cl0_regulator", &regulator_name))
                return -ENODEV;

	if (of_property_read_string(np,
				"cl0_dvfs_domain_name", &cluster_domain_name))
		return -ENODEV;

#if defined(CONFIG_ECT)
	if (exynos_cpufreq_parse_frequency((char *)cluster_domain_name, ptr))
		return -ENODEV;
#else
	/*
	 * we do not enable cpufreq driver if ect doesn't exist
	 */
	return -ENODEV;
#endif
	ptr->freq_table[ptr->max_idx_num].driver_data = ptr->max_idx_num;
	ptr->freq_table[ptr->max_idx_num].frequency = CPUFREQ_TABLE_END;
	return 0;
}

static int exynos_min_qos_handler(struct notifier_block *b,
		unsigned long val, void *v)
{
	int ret;
	unsigned long freq;
	struct cpufreq_policy *policy;
	int cpu = CL0_POLICY_CPU;

	freq = exynos_getspeed(cpu);
	if (freq >= val)
		goto good;

	policy = cpufreq_cpu_get(cpu);

	if (!policy)
		goto bad;

	if (!policy->user_policy.governor) {
		cpufreq_cpu_put(policy);
		goto bad;
	}

#if defined(CONFIG_CPU_FREQ_GOV_USERSPACE) || defined(CONFIG_CPU_FREQ_GOV_PERFORMANCE)
	if ((strcmp(policy->user_policy.governor->name, "userspace") == 0)
			|| strcmp(policy->user_policy.governor->name, "performance") == 0) {
		cpufreq_cpu_put(policy);
		goto good;
	}
#endif

	ret = __cpufreq_driver_target(policy, val, CPUFREQ_RELATION_H);

	cpufreq_cpu_put(policy);

	if (ret < 0)
		goto bad;

good:
	return NOTIFY_OK;
bad:
	return NOTIFY_BAD;
}

static struct notifier_block exynos_min_qos_notifier = {
	.notifier_call = exynos_min_qos_handler,
	.priority = INT_MAX,
};

static int exynos_max_qos_handler(struct notifier_block *b,
                                                unsigned long val, void *v)
{
        int ret;
        unsigned long freq;
        struct cpufreq_policy *policy;
        int cpu = CL0_POLICY_CPU;

        freq = exynos_getspeed(cpu);
        if (freq <= val)
                goto good;

        policy = cpufreq_cpu_get(cpu);

        if (!policy)
                goto bad;

        if (!policy->user_policy.governor) {
                cpufreq_cpu_put(policy);
                goto bad;
        }

#if defined(CONFIG_CPU_FREQ_GOV_USERSPACE) || defined(CONFIG_CPU_FREQ_GOV_PERFORMANCE)
        if ((strcmp(policy->user_policy.governor->name, "userspace") == 0)
                        || strcmp(policy->user_policy.governor->name, "performance") == 0) {
                cpufreq_cpu_put(policy);
                goto good;
        }
#endif

        ret = __cpufreq_driver_target(policy, val, CPUFREQ_RELATION_H);

        cpufreq_cpu_put(policy);

        if (ret < 0)
                goto bad;

good:
        return NOTIFY_OK;
bad:
        return NOTIFY_BAD;
}

static struct notifier_block exynos_max_qos_notifier = {
        .notifier_call = exynos_max_qos_handler,
        .priority = INT_MAX,
};

static bool suspend_prepared = false;
static int __cpuinit exynos_cpufreq_cpu_notifier(struct notifier_block *notifier,
                                        unsigned long action, void *hcpu)
{
        unsigned int cpu = (unsigned long)hcpu;
        struct device *dev;

        if (suspend_prepared)
                return NOTIFY_OK;

        dev = get_cpu_device(cpu);
        if (dev) {
                switch (action) {
                case CPU_DOWN_PREPARE:
                case CPU_DOWN_PREPARE_FROZEN:
                        mutex_lock(&cpufreq_lock);
                        exynos_info->blocked = true;
                        mutex_unlock(&cpufreq_lock);
                        break;
                case CPU_DOWN_FAILED:
                case CPU_DOWN_FAILED_FROZEN:
                case CPU_DEAD:
                        mutex_lock(&cpufreq_lock);
                        exynos_info->blocked = false;
                        mutex_unlock(&cpufreq_lock);
                        break;
                }
        }
        return NOTIFY_OK;
}

static struct notifier_block __refdata exynos_cpufreq_cpu_nb = {
        .notifier_call = exynos_cpufreq_cpu_notifier,
	 .priority = INT_MIN + 2,
};

static unsigned int get_freq_volt(unsigned int target_freq, int *idx)
{
        int index;
        int i;

        struct cpufreq_frequency_table *table = exynos_info->freq_table;

        for (i = 0; (table[i].frequency != CPUFREQ_TABLE_END); i++) {
                unsigned int freq = table[i].frequency;
                if (freq == CPUFREQ_ENTRY_INVALID)
                        continue;

                if (target_freq == freq) {
                        index = i;
                        break;
                }
        }

        if (table[i].frequency == CPUFREQ_TABLE_END) {
                if (exynos_info->boot_freq_idx != -1 &&
                        target_freq == exynos_info->boot_freq)
                        index = exynos_info->boot_freq_idx;
                else
                        return EINVAL;
        }

        if (idx)
                *idx = index;

        return exynos_info->volt_table[index];
}

static unsigned int get_boot_freq(void)
{
	if (!exynos_info) {
		BUG_ON(!exynos_info);
		return 0;
	}

	return exynos_info->boot_freq;
}

static unsigned int get_boot_volt(void)
{
	unsigned int boot_freq = get_boot_freq();

	return get_freq_volt(boot_freq, NULL);
}

static unsigned int volt_offset;
static unsigned int get_limit_voltage(unsigned int voltage)
{
        BUG_ON(!voltage);
        if (voltage > LIMIT_COLD_VOLTAGE)
                return voltage;

        if (voltage + volt_offset > LIMIT_COLD_VOLTAGE)
                return LIMIT_COLD_VOLTAGE;

        if (ENABLE_MIN_COLD && volt_offset
                && (voltage + volt_offset) < MIN_COLD_VOLTAGE)
                return MIN_COLD_VOLTAGE;

        return voltage + volt_offset;
}

/*
 * exynos_cpufreq_pm_notifier - block CPUFREQ's activities in suspend-resume
 *                      context
 * @notifier
 * @pm_event
 * @v
 *
 * While cpufreq_disable == true, target() ignores every frequency but
 * boot_freq. The boot_freq value is the initial frequency,
 * which is set by the bootloader. In order to eliminate possible
 * inconsistency in clock values, we save and restore frequencies during
 * suspend and resume and block CPUFREQ activities. Note that the standard
 * suspend/resume cannot be used as they are too deep (syscore_ops) for
 * regulator actions.
 */
static int exynos_cpufreq_pm_notifier(struct notifier_block *notifier,
                                       unsigned long pm_event, void *v)
{
        struct cpufreq_frequency_table *freq_table = NULL;
        unsigned int bootfreq;
        unsigned int abb_freq = 0;
        bool set_abb_first_than_volt = false;
        int volt, i;
        int idx;

        switch (pm_event) {
        case PM_SUSPEND_PREPARE:
		/* hold boot_min/max_qos to boot freq */
		pm_qos_update_request(&boot_min_qos, exynos_info->resume_freq);
		pm_qos_update_request(&boot_max_qos, exynos_info->resume_freq);

                mutex_lock(&cpufreq_lock);
                exynos_info->blocked = true;
                mutex_unlock(&cpufreq_lock);

		volt = max(get_freq_volt(exynos_info->resume_freq, &idx), get_freq_volt(freqs->old, &idx));
		if ( volt <= 0) {
			printk("oops, strange voltage CL0 -> boot volt:%d, get_freq_volt:%d, freq:%d \n",
			       get_boot_volt(), get_freq_volt(freqs->old, &idx), freqs->old);
			BUG_ON(volt <= 0);
		}
		volt = get_limit_voltage(volt);

		if (exynos_info->abb_table)
			set_abb_first_than_volt =
				is_set_abb_first(ID_CL0, freqs->old, bootfreq);

		if (!set_abb_first_than_volt)
			if (regulator_set_voltage(exynos_info->regulator,
						volt, exynos_info->regulator_max_support_volt))
				goto err;

		exynos_info->cur_volt = volt;

		if (exynos_info->abb_table) {
			abb_freq = max(bootfreq, freqs->old);
			freq_table = exynos_info->freq_table;
			for (i = 0; (freq_table[i].frequency != CPUFREQ_TABLE_END); i++) {
				if (freq_table[i].frequency == CPUFREQ_ENTRY_INVALID)
					continue;
				if (freq_table[i].frequency == abb_freq) {
					set_match_abb(ID_CL0, exynos_info->abb_table[i]);
					break;
				}
			}
		}

		if (set_abb_first_than_volt)
			if (regulator_set_voltage(exynos_info->regulator,
						volt, exynos_info->regulator_max_support_volt))
				goto err;

		exynos_info->cur_volt = volt;

		if (exynos_info->set_ema)
			exynos_info->set_ema(volt);

		set_abb_first_than_volt = false;

                suspend_prepared = true;

                pr_debug("PM_SUSPEND_PREPARE for CPUFREQ\n");

                break;
        case PM_POST_SUSPEND:
                pr_debug("PM_POST_SUSPEND for CPUFREQ\n");

		mutex_lock(&cpufreq_lock);
		exynos_info->blocked = false;
		mutex_unlock(&cpufreq_lock);

                suspend_prepared = false;
		/* recover boot_min/max_qos to default value */
		pm_qos_update_request(&boot_max_qos, core_max_qos_const.default_value);
		pm_qos_update_request(&boot_min_qos, qos_min_default_value );

                break;
        }
        return NOTIFY_OK;
err:
        pr_err("%s: failed to set voltage\n", __func__);

        return NOTIFY_BAD;
}

static struct notifier_block exynos_cpufreq_nb = {
        .notifier_call = exynos_cpufreq_pm_notifier,
};

static int exynos_cpufreq_reboot_notifier_call(struct notifier_block *this,
                                   unsigned long code, void *_cmd)
{
        struct cpufreq_frequency_table *freq_table;
        unsigned int bootfreq;
        unsigned int abb_freq = 0;
        bool set_abb_first_than_volt = false;
        int volt, i, idx;

	if (exynos_info->reboot_limit_freq)
		pm_qos_add_request(&reboot_max_qos, qos_max_class,
					exynos_info->reboot_limit_freq);

	mutex_lock(&cpufreq_lock);
	exynos_info->blocked = true;
	mutex_unlock(&cpufreq_lock);

	bootfreq = get_boot_freq();

	volt = max(get_boot_volt(),
			get_freq_volt(freqs->old, &idx));
	volt = get_limit_voltage(volt);

	if (exynos_info->abb_table)
		set_abb_first_than_volt =
			is_set_abb_first(ID_CL0, freqs->old,
						max(bootfreq, freqs->old));

	if (!set_abb_first_than_volt)
		if (regulator_set_voltage(exynos_info->regulator,
					volt, exynos_info->regulator_max_support_volt))
			goto err;

	exynos_info->cur_volt = volt;

	if (exynos_info->abb_table) {
		abb_freq = max(bootfreq, freqs->old);
		freq_table = exynos_info->freq_table;
		for (i = 0; (freq_table[i].frequency != CPUFREQ_TABLE_END); i++) {
			if (freq_table[i].frequency == CPUFREQ_ENTRY_INVALID)
				continue;
			if (freq_table[i].frequency == abb_freq) {
				set_match_abb(ID_CL0, exynos_info->abb_table[i]);
				break;
			}
		}
	}

	if (set_abb_first_than_volt)
		if (regulator_set_voltage(exynos_info->regulator,
					volt, exynos_info->regulator_max_support_volt))
			goto err;

	exynos_info->cur_volt = volt;

	if (exynos_info->set_ema)
		exynos_info->set_ema(volt);

	set_abb_first_than_volt = false;

        return NOTIFY_DONE;
err:
        pr_err("%s: failed to set voltage\n", __func__);

        return NOTIFY_BAD;
}

static struct notifier_block exynos_cpufreq_reboot_notifier = {
        .notifier_call = exynos_cpufreq_reboot_notifier_call,
};

#ifdef CONFIG_EXYNOS_THERMAL
static int exynos_cpufreq_tmu_notifier(struct notifier_block *notifier,
                                       unsigned long event, void *v)
{
        int volt;
        int *on = (int *)v;
        int ret = NOTIFY_OK;
        int cold_offset = 0;

        if (event != TMU_COLD)
                return NOTIFY_OK;

        mutex_lock(&cpufreq_lock);
        if (*on) {
                if (volt_offset)
                        goto out;
                else
                        volt_offset = COLD_VOLT_OFFSET;

		volt = exynos_info->cur_volt;
		volt = get_limit_voltage(volt);
		ret = regulator_set_voltage(exynos_info->regulator,
					volt, exynos_info->regulator_max_support_volt);
		if (ret) {
			ret = NOTIFY_BAD;
			goto out;
		}

		exynos_info->cur_volt = volt;

		if (exynos_info->set_ema)
			exynos_info->set_ema(volt);
        } else {
                if (!volt_offset)
                        goto out;
                else {
                        cold_offset = volt_offset;
                        volt_offset = 0;
                }

		volt = get_limit_voltage(exynos_info->cur_volt - cold_offset);

		if (exynos_info->set_ema)
			exynos_info->set_ema(volt);

		ret = regulator_set_voltage(exynos_info->regulator,
					volt, exynos_info->regulator_max_support_volt);
		if (ret) {
			ret = NOTIFY_BAD;
			goto out;
		}

		exynos_info->cur_volt = volt;
        }

out:
        mutex_unlock(&cpufreq_lock);

        return ret;
}

static struct notifier_block exynos_tmu_nb = {
        .notifier_call = exynos_cpufreq_tmu_notifier,
};
#endif
inline ssize_t show_core_freq_table(char *buf)
{
        int i, count = 0;
        size_t tbl_sz = 0, pr_len;
        struct cpufreq_frequency_table *freq_table = exynos_info->freq_table;

        for (i = 0; freq_table[i].frequency != CPUFREQ_TABLE_END; i++)
                tbl_sz++;

        if (tbl_sz == 0)
                return -EINVAL;

        pr_len = (size_t)((PAGE_SIZE - 2) / tbl_sz);

        for (i = 0; freq_table[i].frequency != CPUFREQ_TABLE_END; i++) {
                if (freq_table[i].frequency != CPUFREQ_ENTRY_INVALID)
                        count += snprintf(&buf[count], pr_len, "%d ",
                                        freq_table[i].frequency);
        }

        count += snprintf(&buf[count], 2, "\n");
        return count;
}

inline ssize_t show_core_freq(char *buf,
                                bool qos_flag) /* qos_flag : false is Freq_min, true is Freq_max */
{
        int qos_class = (qos_flag ? qos_max_class : qos_min_class);
        unsigned int qos_value = pm_qos_request(qos_class);
        unsigned int *freq_info = (qos_flag ? &freq_max : &freq_min);

        if (qos_value == 0)
                qos_value = *freq_info;

        return snprintf(buf, PAGE_SIZE, "%u\n", qos_value);
}

inline ssize_t store_core_freq(const char *buf, size_t count,
                                bool qos_flag) /* qos_flag : false is Freq_min, true is Freq_max */
{
        struct pm_qos_request *core_qos_real = (qos_flag ? &core_max_qos_real : &core_min_qos_real);
        /* Need qos_flag's opposite freq_info for comparison */
        unsigned int *freq_info = (qos_flag ? &freq_min : &freq_max);
        int input;

        if (!sscanf(buf, "%8d", &input))
                return -EINVAL;

        if (input > 0) {
                if (qos_flag)
                        input = max(input, (int)*freq_info);
                else {
                        input = min(input, (int)*freq_info);
#ifdef CONFIG_SW_SELF_DISCHARGING
			input = max(input, self_discharging);
#endif
		}
        }

        if (pm_qos_request_active(core_qos_real))
                pm_qos_update_request(core_qos_real, input);

        return count;
}


static ssize_t show_cluster0_freq_table(struct kobject *kobj,
                             struct attribute *attr, char *buf)
{
        return show_core_freq_table(buf);
}

static ssize_t show_cluster0_min_freq(struct kobject *kobj,
                             struct attribute *attr, char *buf)
{
        return show_core_freq(buf,  false);
}

static ssize_t show_cluster0_max_freq(struct kobject *kobj,
                             struct attribute *attr, char *buf)
{
        return show_core_freq(buf, true);
}

static ssize_t store_cluster0_min_freq(struct kobject *kobj, struct attribute *attr,
                                        const char *buf, size_t count)
{
        return store_core_freq(buf, count, false);
}

static ssize_t store_cluster0_max_freq(struct kobject *kobj, struct attribute *attr,
                                        const char *buf, size_t count)
{
        return store_core_freq(buf, count, true);
}


define_one_global_ro(cluster0_freq_table);
define_one_global_rw(cluster0_min_freq);
define_one_global_rw(cluster0_max_freq);

static struct attribute *attributes[] = {
        &cluster0_freq_table.attr,
        &cluster0_min_freq.attr,
        &cluster0_max_freq.attr,
        NULL
};

static struct attribute_group attr_group = {
        .attrs = attributes,
        .name = "sc-cpufreq",
};

#ifdef CONFIG_PM
static ssize_t show_cpufreq_table(struct kobject *kobj,
                                struct attribute *attr, char *buf)
{
        return show_core_freq_table(buf);
}

static ssize_t show_cpufreq_min_limit(struct kobject *kobj,
                                struct attribute *attr, char *buf)
{
        return show_core_freq(buf, false);
}

static ssize_t store_cpufreq_min_limit(struct kobject *kobj, struct attribute *attr,
                                        const char *buf, size_t count)
{
        return store_core_freq(buf, count, false);
}

static ssize_t show_cpufreq_max_limit(struct kobject *kobj,
                                struct attribute *attr, char *buf)
{
        return show_core_freq(buf, true);
}

static ssize_t store_cpufreq_max_limit(struct kobject *kobj, struct attribute *attr,
                                        const char *buf, size_t count)
{
        return store_core_freq(buf, count, true);
}

static struct global_attr cpufreq_table =
                __ATTR(cpufreq_table, S_IRUGO, show_cpufreq_table, NULL);
static struct global_attr cpufreq_min_limit =
                __ATTR(cpufreq_min_limit, S_IRUGO | S_IWUSR,
                        show_cpufreq_min_limit, store_cpufreq_min_limit);
static struct global_attr cpufreq_max_limit =
                __ATTR(cpufreq_max_limit, S_IRUGO | S_IWUSR,
                        show_cpufreq_max_limit, store_cpufreq_max_limit);
#endif

#ifdef CONFIG_SW_SELF_DISCHARGING
static ssize_t show_cpufreq_self_discharging(struct kobject *kobj,
			     struct attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", self_discharging);
}

static ssize_t store_cpufreq_self_discharging(struct kobject *kobj, struct attribute *attr,
			      const char *buf, size_t count)
{
	int input;

	if (!sscanf(buf, "%d", &input))
		return -EINVAL;

	if (input > 0) {
		self_discharging = input;
		cpu_idle_poll_ctrl(true);
	}
	else {
		self_discharging = 0;
		cpu_idle_poll_ctrl(false);
	}

        return store_core_freq(buf, count, false);
}

static struct global_attr cpufreq_self_discharging =
		__ATTR(cpufreq_self_discharging, S_IRUGO | S_IWUSR,
			show_cpufreq_self_discharging, store_cpufreq_self_discharging);
#endif

static unsigned int exynos_get_safe_volt(unsigned int old_index,
					unsigned int new_index)
{
        unsigned int safe_arm_volt = 0;
        struct cpufreq_frequency_table *freq_table
                                        = exynos_info->freq_table;
        unsigned int *volt_table = exynos_info->volt_table;

        /*
         * ARM clock source will be changed APLL to MPLL temporary
         * To support this level, need to control regulator for
         * reguired voltage level
         */
        if (exynos_info->need_apll_change != NULL) {
                if (exynos_info->need_apll_change(old_index, new_index) &&
                        (freq_table[new_index].frequency < exynos_info->mpll_freq_khz) &&
                        (freq_table[old_index].frequency < exynos_info->mpll_freq_khz)) {
                                safe_arm_volt = volt_table[exynos_info->pll_safe_idx];
                }
        }

        return safe_arm_volt;
}

/* Determine valid target frequency using freq_table */
int exynos_frequency_table_target(struct cpufreq_policy *policy,
                                   struct cpufreq_frequency_table *table,
                                   unsigned int target_freq,
                                   unsigned int relation,
                                   unsigned int *index)
{
        unsigned int i;

        if (!cpu_online(policy->cpu))
                return -EINVAL;

        for (i = 0; (table[i].frequency != CPUFREQ_TABLE_END); i++) {
                unsigned int freq = table[i].frequency;
                if (freq == CPUFREQ_ENTRY_INVALID)
                        continue;

                if (target_freq == freq) {
                        *index = i;
                        break;
                }
        }

        if (table[i].frequency == CPUFREQ_TABLE_END) {
                if (exynos_info->boot_freq_idx != -1 &&
                        target_freq == exynos_info->boot_freq)
                        *index = exynos_info->boot_freq_idx;
                else
                        return -EINVAL;
        }

        return 0;
}

static int exynos_set_voltage(unsigned int cur_index, unsigned int new_index,
								unsigned int volt)
{
        struct regulator *regulator = exynos_info->regulator;
        int regulator_max_support_volt = exynos_info->regulator_max_support_volt;
        struct cpufreq_frequency_table *freq_table = exynos_info->freq_table;
        bool set_abb_first_than_volt = false;
        int ret = 0;

        if (exynos_info->abb_table)
                set_abb_first_than_volt = is_set_abb_first(ID_CL0,
                                                freq_table[cur_index].frequency,
                                                freq_table[new_index].frequency);

        if (!set_abb_first_than_volt) {
                ret = regulator_set_voltage(regulator, volt, regulator_max_support_volt);
                if (ret)
                        goto out;

                exynos_info->cur_volt = volt;
        }

        if (exynos_info->abb_table)
                set_match_abb(ID_CL0, exynos_info->abb_table[new_index]);

        if (set_abb_first_than_volt) {
                ret = regulator_set_voltage(regulator, volt, regulator_max_support_volt);
                if (ret)
                        goto out;

                exynos_info->cur_volt = volt;
        }

out:
        return ret;
}

#ifdef CONFIG_SMP
struct lpj_info {
        unsigned long   ref;
        unsigned int    freq;
};

static struct lpj_info global_lpj_ref;
#endif

static int exynos_cpufreq_scale(unsigned int target_freq, unsigned int cpu)
{
        struct cpufreq_frequency_table *freq_table = exynos_info->freq_table;
        unsigned int *volt_table = exynos_info->volt_table;
        struct cpufreq_policy *policy = cpufreq_cpu_get(cpu);
        unsigned int new_index, old_index;
        unsigned int volt, safe_volt = 0;
        int ret = 0;

        if (!policy)
                return -EINVAL;

        freqs->cpu = cpu;
        freqs->new = target_freq;

        if (exynos_frequency_table_target(policy, freq_table,
                                freqs->old, CPUFREQ_RELATION_L, &old_index)) {
                ret = -EINVAL;
                goto put_policy;
        }

        if (exynos_frequency_table_target(policy, freq_table,
                                freqs->new, CPUFREQ_RELATION_L, &new_index)) {
                ret = -EINVAL;
                goto put_policy;
        }

        if (old_index == new_index)
                goto put_policy;

        /*
         * ARM clock source will be changed APLL to MPLL temporary
         * To support this level, need to control regulator for
         * required voltage level
         */
        safe_volt = exynos_get_safe_volt(old_index, new_index);
        if (safe_volt)
                safe_volt = get_limit_voltage(safe_volt);

        volt = get_limit_voltage(volt_table[new_index]);

        /* Update policy current frequency */
        cpufreq_freq_transition_begin(policy, freqs);

        if (old_index > new_index)
                if (exynos_info->set_int_skew)
                        exynos_info->set_int_skew(new_index);

        /* When the new frequency is higher than current frequency */
        if ((old_index > new_index) && !safe_volt) {
                /* Firstly, voltage up to increase frequency */
                ret = exynos_set_voltage(old_index, new_index, volt);
                if (ret)
                        goto fail_dvfs;

                if (exynos_info->set_ema)
                        exynos_info->set_ema(volt);
        }

        if (safe_volt) {
                ret = exynos_set_voltage(old_index, new_index, safe_volt);
                if (ret)
                        goto fail_dvfs;

                if (exynos_info->set_ema)
                        exynos_info->set_ema(safe_volt);
        }

        if (old_index > new_index) {
                if (pm_qos_request_active(&exynos_mif_qos))
                        pm_qos_update_request(&exynos_mif_qos,
                                        exynos_info->bus_table[new_index]);
        }

        exynos_info->set_freq(old_index, new_index);

        if (old_index < new_index) {
                if (pm_qos_request_active(&exynos_mif_qos))
                        pm_qos_update_request(&exynos_mif_qos,
                                        exynos_info->bus_table[new_index]);
        }

#ifdef CONFIG_SMP
        if (!global_lpj_ref.freq) {
                global_lpj_ref.ref = loops_per_jiffy;
                global_lpj_ref.freq = freqs->old;
        }

#ifndef CONFIG_ARM64
        loops_per_jiffy = cpufreq_scale(global_lpj_ref.ref,
                        global_lpj_ref.freq, freqs->new);
#endif
#endif

        cpufreq_freq_transition_end(policy, freqs, 0);

        /* When the new frequency is lower than current frequency */
        if ((old_index < new_index) || ((old_index > new_index) && safe_volt)) {
                /* down the voltage after frequency change */
                if (exynos_info->set_ema)
                         exynos_info->set_ema(volt);

                ret = exynos_set_voltage(old_index, new_index, volt);
                if (ret) {
                        /* save current frequency as frequency is changed*/
                        freqs->old = target_freq;
                        goto put_policy;
                }
        }

        if (old_index < new_index)
                if (exynos_info->set_int_skew)
                        exynos_info->set_int_skew(new_index);

        cpufreq_cpu_put(policy);

        return 0;

fail_dvfs:
        cpufreq_freq_transition_end(policy, freqs, ret);
put_policy:
        cpufreq_cpu_put(policy);

        return ret;
}

static unsigned int exynos_verify_pm_qos_limit(struct cpufreq_policy *policy,
						unsigned int target_freq)
{
#if defined(CONFIG_CPU_FREQ_GOV_USERSPACE) || defined(CONFIG_CPU_FREQ_GOV_PERFORMANCE)
        if ((strcmp(policy->governor->name, "userspace") == 0)
                        || strcmp(policy->governor->name, "performance") == 0)
                return target_freq;
#endif
        target_freq = max((unsigned int)pm_qos_request(qos_min_class), target_freq);
        target_freq = min((unsigned int)pm_qos_request(qos_max_class), target_freq);

        return target_freq;
}

/* Set clock frequency */
static int exynos_target(struct cpufreq_policy *policy,
                          unsigned int target_freq,
                          unsigned int relation)
{
        struct cpufreq_frequency_table *freq_table = exynos_info->freq_table;
        unsigned int index;
        int ret = 0;

        mutex_lock(&cpufreq_lock);

        if (exynos_info->blocked)
                goto out;

        if (target_freq == 0)
                target_freq = policy->min;

        /* if PLL bypass, frequency scale is skip */
        if (exynos_getspeed(policy->cpu) <= FREQ_OSCCLK)
                goto out;

        /* verify old frequency */
        if (freqs->old != exynos_getspeed(policy->cpu)) {
                printk("oops, sombody change clock  old clk:%d, cur clk:%d \n",
                                        freqs->old, exynos_getspeed(policy->cpu));
                BUG_ON(freqs->old != exynos_getspeed(policy->cpu));
        }

        /* verify pm_qos_lock */
        target_freq = exynos_verify_pm_qos_limit(policy, target_freq);

        if (cpufreq_frequency_table_target(policy, freq_table,
                                target_freq, relation, &index)) {
                ret = -EINVAL;
                goto out;
        }

        target_freq = freq_table[index].frequency;

        pr_debug("%s: new_freq[%d], index[%d]\n",
                                __func__, target_freq, index);

#ifdef CONFIG_EXYNOS_SNAPSHOT_FREQ
        exynos_ss_freq(0, freqs->old, target_freq, ESS_FLAG_IN);
#endif
        /* frequency and volt scaling */
        ret = exynos_cpufreq_scale(target_freq, policy->cpu);
#ifdef CONFIG_EXYNOS_SNAPSHOT_FREQ
        exynos_ss_freq(0, freqs->old, target_freq, ESS_FLAG_OUT);
#endif
        if (ret < 0)
                goto out;

        /* save current frequency */
        freqs->old = target_freq;

out:
        mutex_unlock(&cpufreq_lock);

        return ret;
}

static int exynos_cpufreq_init(struct cpufreq_policy *policy)
{
        pr_debug("%s: cpu[%d]\n", __func__, policy->cpu);

        policy->cur = policy->min = policy->max = exynos_getspeed(policy->cpu);

        cpufreq_table_validate_and_show(policy, exynos_info->freq_table);

        /* set the transition latency value */
        policy->cpuinfo.transition_latency = 100000;

	cpumask_copy(policy->cpus, &cluster_cpumask);
	cpumask_copy(policy->related_cpus, &cluster_cpumask);

        return cpufreq_frequency_table_cpuinfo(policy, exynos_info->freq_table);
}

#ifdef CONFIG_PM
static int exynos_cpufreq_suspend(struct cpufreq_policy *policy)
{
        return 0;
}

static int exynos_cpufreq_resume(struct cpufreq_policy *policy)
{
	unsigned int freq = exynos_getspeed_cluster();
        freqs->old = freq;

        /* Update policy->cur value to resume frequency */
        policy->cur = freq;

        return 0;
}
#endif

static struct cpufreq_driver exynos_driver = {
        .flags          = CPUFREQ_STICKY | CPUFREQ_HAVE_GOVERNOR_PER_POLICY,
        .verify         = exynos_verify_speed,
        .get            = exynos_getspeed,
        .target         = exynos_target,
        .init           = exynos_cpufreq_init,
        .name           = "exynos_cpufreq",
#ifdef CONFIG_PM
        .suspend        = exynos_cpufreq_suspend,
        .resume         = exynos_cpufreq_resume,
#endif
};

static void __remove_pm_qos(struct pm_qos_request *req)
{
        if (pm_qos_request_active(req))
                pm_qos_remove_request(req);
}

static void remove_pm_qos(void)
{
        if (exynos_info->bus_table)
                __remove_pm_qos(&exynos_mif_qos);

        __remove_pm_qos(&boot_max_qos);
        __remove_pm_qos(&boot_min_qos);
        __remove_pm_qos(&core_max_qos);
        __remove_pm_qos(&core_min_qos);
        __remove_pm_qos(&core_min_qos_real);
        __remove_pm_qos(&core_max_qos_real);

}

static int exynos_sc_cpufreq_driver_init(struct device *dev)
{
	int i, ret = -EINVAL;
	struct cpufreq_frequency_table *freq_table;
	struct cpufreq_policy *policy;

	/* setting cluster cpumask */
	cpumask_setall(&cluster_cpumask);
	exynos_info->regulator = regulator_get(dev, regulator_name);
	if (IS_ERR(exynos_info->regulator)) {
		pr_err("%s:failed to get regulator %s\n",
				__func__, regulator_name);
			goto err_init;
	}

	exynos_info->regulator_max_support_volt =
		regulator_get_max_support_voltage(exynos_info->regulator);

	freq_max = exynos_info->freq_table[exynos_info->max_support_idx].frequency;
	freq_min = exynos_info->freq_table[exynos_info->min_support_idx].frequency;

	set_boot_freq();

	exynos_info->cur_volt = regulator_get_voltage(exynos_info->regulator);

	regulator_set_voltage(exynos_info->regulator, get_freq_volt(exynos_info->boot_freq, NULL),
					exynos_info->regulator_max_support_volt);

	/* set initial old frequency */
	freqs->old = exynos_getspeed_cluster();

	/*
	 * boot freq index is needed if minimum supported frequency
	 * greater than boot freq
	 */
	freq_table = exynos_info->freq_table;
	exynos_info->boot_freq_idx = -1;

	for (i = L0; (freq_table[i].frequency != CPUFREQ_TABLE_END); i++) {
		if (exynos_info->boot_freq == freq_table[i].frequency)
			exynos_info->boot_freq_idx = i;

		if (freq_table[i].frequency > freq_max ||
				freq_table[i].frequency < freq_min)
			freq_table[i].frequency = CPUFREQ_ENTRY_INVALID;
	}

	/* setup default qos constraints */
	core_max_qos_const.target_value = freq_max;
	core_max_qos_const.default_value = freq_max;
	pm_qos_update_constraints(qos_max_class, &core_max_qos_const);

	pm_qos_add_notifier(qos_min_class, &exynos_min_qos_notifier);
	pm_qos_add_notifier(qos_max_class, &exynos_max_qos_notifier);

        register_hotcpu_notifier(&exynos_cpufreq_cpu_nb);
        register_pm_notifier(&exynos_cpufreq_nb);
        register_reboot_notifier(&exynos_cpufreq_reboot_notifier);
#ifdef CONFIG_EXYNOS_THERMAL
        exynos_tmu_add_notifier(&exynos_tmu_nb);
#endif

        /* block frequency scale before acquire boot lock */
#if !defined(CONFIG_CPU_FREQ_DEFAULT_GOV_PERFORMANCE) && !defined(CONFIG_CPU_FREQ_DEFAULT_GOV_POWERSAVE)
        mutex_lock(&cpufreq_lock);
        exynos_info->blocked = true;
	mutex_unlock(&cpufreq_lock);
#endif
        if (cpufreq_register_driver(&exynos_driver)) {
                pr_err("%s: failed to register cpufreq driver\n", __func__);
                goto err_cpufreq;
        }

	/*
	 * Setting PMQoS
	 */
	pm_qos_add_request(&core_min_qos, qos_min_class, qos_min_default_value);
	pm_qos_add_request(&core_max_qos, qos_max_class,
			   core_max_qos_const.default_value);
	pm_qos_add_request(&core_min_qos_real, qos_min_class,
			   qos_min_default_value);
	pm_qos_add_request(&core_max_qos_real, qos_max_class,
			   core_max_qos_const.default_value);

	pm_qos_add_request(&boot_min_qos, qos_min_class, qos_min_default_value);
	pm_qos_update_request_timeout(&boot_min_qos,
					exynos_info->boot_min_qos,
					exynos_info->boot_lock_time);
	pm_qos_add_request(&boot_max_qos, qos_max_class,
			   core_max_qos_const.default_value);
	pm_qos_update_request_timeout(&boot_max_qos,
				      exynos_info->boot_max_qos,
				      exynos_info->boot_lock_time);

	if (exynos_info->bus_table)
		pm_qos_add_request(&exynos_mif_qos, PM_QOS_BUS_THROUGHPUT, 0);

        /* unblock frequency scale */
#if !defined(CONFIG_CPU_FREQ_DEFAULT_GOV_PERFORMANCE) && !defined(CONFIG_CPU_FREQ_DEFAULT_GOV_POWERSAVE)
        mutex_lock(&cpufreq_lock);
        exynos_info->blocked = false;
        mutex_unlock(&cpufreq_lock);
#endif

        /*
         * forced call cpufreq target function.
         * If target function is called by interactive governor when blocked cpufreq scale,
         * interactive governor's target_freq is updated to new_freq. But, frequency is
         * not changed because blocking cpufreq scale. And if governor request same frequency
         * after unblocked scale, speedchange_task is not wakeup because new_freq and target_freq
         * is same.
         */

        policy = cpufreq_cpu_get(0);
        if (!policy)
                goto err_policy;

        if (!policy->user_policy.governor) {
                cpufreq_cpu_put(policy);
                goto err_policy;
        }

	__cpufreq_driver_target(policy, policy->min, CPUFREQ_RELATION_H);
        cpufreq_cpu_put(policy);

        ret = cpufreq_sysfs_create_group(&attr_group);
        if (ret) {
                pr_err("%s: failed to create mp-cpufreq sysfs interface\n", __func__);
                goto err_attr;
        }

#ifdef CONFIG_PM
        ret = sysfs_create_file(power_kobj, &cpufreq_table.attr);
        if (ret) {
                pr_err("%s: failed to create cpufreq_table sysfs interface\n", __func__);
                goto err_cpu_table;
        }

        ret = sysfs_create_file(power_kobj, &cpufreq_min_limit.attr);
        if (ret) {
                pr_err("%s: failed to create cpufreq_min_limit sysfs interface\n", __func__);
                goto err_cpufreq_min_limit;
        }

        ret = sysfs_create_file(power_kobj, &cpufreq_max_limit.attr);
        if (ret) {
                pr_err("%s: failed to create cpufreq_max_limit sysfs interface\n", __func__);
                goto err_cpufreq_max_limit;
        }
#endif
#ifdef CONFIG_SW_SELF_DISCHARGING
	self_discharging = 0;
	ret = sysfs_create_file(power_kobj, &cpufreq_self_discharging.attr);
	if (ret) {
		pr_err("%s: failed to create cpufreq_self_discharging sysfs interface\n", __func__);
		goto err_cpufreq_self_discharging;
	}
#endif

	pr_info("%s: Single cluster CPUFreq Initialization Complete\n", __func__);
	return 0;

#ifdef CONFIG_SW_SELF_DISCHARGING
err_cpufreq_self_discharging:
	sysfs_remove_file(power_kobj, &cpufreq_max_limit.attr);
#endif
#ifdef CONFIG_PM
err_cpufreq_max_limit:
        sysfs_remove_file(power_kobj, &cpufreq_min_limit.attr);
err_cpufreq_min_limit:
        sysfs_remove_file(power_kobj, &cpufreq_table.attr);
err_cpu_table:
        cpufreq_sysfs_remove_group(&attr_group);
#endif
err_policy:
err_attr:
	if (exynos_info)
		remove_pm_qos();

	cpufreq_unregister_driver(&exynos_driver);
err_cpufreq:
        unregister_reboot_notifier(&exynos_cpufreq_reboot_notifier);
        unregister_pm_notifier(&exynos_cpufreq_nb);
        unregister_hotcpu_notifier(&exynos_cpufreq_cpu_nb);
err_init:
	if (exynos_info) {
		/* Remove pm_qos notifiers */
		pm_qos_remove_notifier(qos_min_class, &exynos_min_qos_notifier);
		pm_qos_remove_notifier(qos_max_class, &exynos_max_qos_notifier);

		/* Release regulater handles */
		if (exynos_info->regulator)
			regulator_put(exynos_info->regulator);
	}

        pr_err("%s: failed initialization\n", __func__);

        return ret;
}

static int exynos_sc_cpufreq_probe(struct platform_device *pdev)
{
	int ret;

	/*
	 * Memory allocation for exynos_info, freqs vairable.
	 * Allocation memory is set to zero.
	 */
	exynos_info = kzalloc(sizeof(struct exynos_dvfs_info), GFP_KERNEL);
	if (!exynos_info) {
		pr_err("Memory allocation failed for exynos_dvfs_info\n");
		return -ENOMEM;
	}

	freqs = kzalloc(sizeof(struct cpufreq_freqs), GFP_KERNEL);
	if (!freqs) {
		pr_err("Memory allocation failed for cpufreq_freqs\n");
		ret = -ENOMEM;
		goto alloc_err;
	}

	/*
	 * Get the data of SoC dependent through parsing DT
	 */
	if (of_device_is_compatible(pdev->dev.of_node,
				"samsung,exynos-sc-cpufreq")) {
		ret = exynos_cpufreq_parse_dt(pdev->dev.of_node);
		if (ret < 0) {
			pr_err("Error of parsing DT\n");
			goto alloc_err;
		}
	}

	/*
	 * CAL initialization for CPUFreq
	 */
	if (exynos_sc_cpufreq_cal_init(exynos_info)) {
		pr_err("Error of initializing cal\n");
		goto alloc_err;
	}

	ret = exynos_sc_cpufreq_driver_init(&pdev->dev);

	return ret;

alloc_err:
	free_info();
	return 0;
}

static int exynos_sc_cpufreq_remove(struct platform_device *pdev)
{
	/* Remove pm_qos and notifiers */
	remove_pm_qos();
	pm_qos_remove_notifier(qos_min_class, &exynos_min_qos_notifier);
	pm_qos_remove_notifier(qos_max_class, &exynos_max_qos_notifier);

	/* Release regulater handles */
	if (exynos_info->regulator)
		regulator_put(exynos_info->regulator);

	/* free driver information */
	free_info();

#ifdef CONFIG_PM
	sysfs_remove_file(power_kobj, &cpufreq_min_limit.attr);
	sysfs_remove_file(power_kobj, &cpufreq_table.attr);
#endif
	cpufreq_sysfs_remove_group(&attr_group);

	unregister_reboot_notifier(&exynos_cpufreq_reboot_notifier);
	unregister_pm_notifier(&exynos_cpufreq_nb);
	unregister_hotcpu_notifier(&exynos_cpufreq_cpu_nb);

	cpufreq_unregister_driver(&exynos_driver);

	return 0;
}

static const struct of_device_id exynos_sc_cpufreq_match[] = {
	{
		.compatible = "samsung,exynos-sc-cpufreq",
	},
	{},
};
MODULE_DEVICE_TABLE(of, exynos_sc_cpufreq);

static struct platform_device_id exynos_sc_cpufreq_driver_ids[] = {
	{
		.name	= "exynos-sc-cpufreq",
	},
	{},
};
MODULE_DEVICE_TABLE(platform, exynos_sc_cpufreq_driver_ids);

static struct platform_driver exynos_sc_cpufreq_driver = {
	.probe		= exynos_sc_cpufreq_probe,
	.remove		= exynos_sc_cpufreq_remove,
	.id_table	= exynos_sc_cpufreq_driver_ids,
	.driver		= {
		.name	= "exynos-sc-cpufreq",
		.owner	= THIS_MODULE,
		.of_match_table = of_match_ptr(exynos_sc_cpufreq_match),
	},
};

static struct platform_device exynos_sc_cpufreq_device = {
	.name   = "exynos-cpufreq",
	.id	= -1,
};

static int __init exynos_sc_cpufreq_init(void)
{
        int ret = 0;

        ret = platform_device_register(&exynos_sc_cpufreq_device);
        if (ret)
                return ret;

        return platform_driver_register(&exynos_sc_cpufreq_driver);
}
device_initcall(exynos_sc_cpufreq_init);

static void __exit exynos_sc_cpufreq_exit(void)
{
        platform_driver_unregister(&exynos_sc_cpufreq_driver);
        platform_device_unregister(&exynos_sc_cpufreq_device);
}
module_exit(exynos_sc_cpufreq_exit);
