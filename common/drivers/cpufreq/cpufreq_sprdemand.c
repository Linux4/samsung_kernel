/*
 *  drivers/cpufreq/cpufreq_sprdemand.c
 *
 *  Copyright (C)  2001 Russell King
 *            (C)  2003 Venkatesh Pallipadi <venkatesh.pallipadi@intel.com>.
 *                      Jun Nakajima <jun.nakajima@intel.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/cpufreq.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/kernel_stat.h>
#include <linux/kobject.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/percpu-defs.h>
#include <linux/slab.h>
#include <linux/sysfs.h>
#include <linux/tick.h>
#include <linux/types.h>
#include <linux/cpu.h>
#include <linux/thermal.h>
#include <linux/err.h>
#include <linux/earlysuspend.h>
#include <linux/suspend.h>

#include "cpufreq_governor.h"

/* On-demand governor macros */
#define DEF_FREQUENCY_DOWN_DIFFERENTIAL		(10)
#define DEF_FREQUENCY_UP_THRESHOLD		(80)
#define DEF_SAMPLING_DOWN_FACTOR		(1)
#define MAX_SAMPLING_DOWN_FACTOR		(100000)
#define MICRO_FREQUENCY_DOWN_DIFFERENTIAL	(3)
#define MICRO_FREQUENCY_UP_THRESHOLD		(95)
#define MICRO_FREQUENCY_MIN_SAMPLE_RATE		(10000)
#define MIN_FREQUENCY_UP_THRESHOLD		(11)
#define MAX_FREQUENCY_UP_THRESHOLD		(100)

/* whether plugin cpu according to this score up threshold */
#define DEF_CPU_SCORE_UP_THRESHOLD		(100)
/* whether unplug cpu according to this down threshold*/
#define DEF_CPU_LOAD_DOWN_THRESHOLD		(20)
#define DEF_CPU_DOWN_COUNT		(3)

#define LOAD_CRITICAL 100
#define LOAD_HI 90
#define LOAD_MID 80
#define LOAD_LIGHT 50
#define LOAD_LO 0

#define LOAD_CRITICAL_SCORE 10
#define LOAD_HI_SCORE 5
#define LOAD_MID_SCORE 0
#define LOAD_LIGHT_SCORE -10
#define LOAD_LO_SCORE -20

#define GOVERNOR_BOOT_TIME	(30*HZ)
static unsigned long boot_done;

unsigned int cpu_hotplug_disable_set = false;

struct unplug_work_info {
	unsigned int cpuid;
	struct delayed_work unplug_work;
	struct dbs_data *dbs_data;
};

struct delayed_work plugin_work;
static DEFINE_PER_CPU(struct unplug_work_info, uwi);

static DEFINE_SPINLOCK(g_lock);
static unsigned int percpu_total_load[CONFIG_NR_CPUS] = {0};
static unsigned int percpu_check_count[CONFIG_NR_CPUS] = {0};
static int cpu_score = 0;

struct thermal_cooling_info_t {
	struct thermal_cooling_device *cdev;
	unsigned long cooling_state;
} thermal_cooling_info = {
	.cdev = NULL,
	.cooling_state = 0,
};

static DEFINE_PER_CPU(struct od_cpu_dbs_info_s, sd_cpu_dbs_info);

static struct od_ops sd_ops;

#ifndef CONFIG_CPU_FREQ_DEFAULT_GOV_SPRDEMAND
static struct cpufreq_governor cpufreq_gov_sprdemand;
#endif

static void sprdemand_powersave_bias_init_cpu(int cpu)
{
	struct od_cpu_dbs_info_s *dbs_info = &per_cpu(sd_cpu_dbs_info, cpu);

	dbs_info->freq_table = cpufreq_frequency_get_table(cpu);
	dbs_info->freq_lo = 0;
}

/*
 * Not all CPUs want IO time to be accounted as busy; this depends on how
 * efficient idling at a higher frequency/voltage is.
 * Pavel Machek says this is not so for various generations of AMD and old
 * Intel systems.
 * Mike Chan (android.com) claims this is also not true for ARM.
 * Because of this, whitelist specific known (series) of CPUs by default, and
 * leave all others up to the user.
 */
static int should_io_be_busy(void)
{
#if defined(CONFIG_X86)
	/*
	 * For Intel, Core 2 (model 15) and later have an efficient idle.
	 */
	if (boot_cpu_data.x86_vendor == X86_VENDOR_INTEL &&
			boot_cpu_data.x86 == 6 &&
			boot_cpu_data.x86_model >= 15)
		return 1;
#endif
	return 0;
}

struct sd_dbs_tuners *g_sd_tuners = NULL;

/*
 * Find right freq to be set now with powersave_bias on.
 * Returns the freq_hi to be used right now and will set freq_hi_jiffies,
 * freq_lo, and freq_lo_jiffies in percpu area for averaging freqs.
 */
static unsigned int generic_powersave_bias_target(struct cpufreq_policy *policy,
		unsigned int freq_next, unsigned int relation)
{
	unsigned int freq_req, freq_reduc, freq_avg;
	unsigned int freq_hi, freq_lo;
	unsigned int index = 0;
	unsigned int jiffies_total, jiffies_hi, jiffies_lo;
	struct od_cpu_dbs_info_s *dbs_info = &per_cpu(sd_cpu_dbs_info,
						   policy->cpu);
	struct dbs_data *dbs_data = policy->governor_data;
	struct sd_dbs_tuners *sd_tuners = NULL;

	if(NULL == dbs_data)
	{
		pr_info("generic_powersave_bias_target governor %s return\n", policy->governor->name);
		if (g_sd_tuners == NULL)
			return freq_next;
		sd_tuners = g_sd_tuners;
	}
	else
	{
		sd_tuners = dbs_data->tuners;
	}

	g_sd_tuners = sd_tuners;

	if (!dbs_info->freq_table) {
		dbs_info->freq_lo = 0;
		dbs_info->freq_lo_jiffies = 0;
		return freq_next;
	}

	cpufreq_frequency_table_target(policy, dbs_info->freq_table, freq_next,
			relation, &index);
	freq_req = dbs_info->freq_table[index].frequency;
	freq_reduc = freq_req * sd_tuners->powersave_bias / 1000;
	freq_avg = freq_req - freq_reduc;

	/* Find freq bounds for freq_avg in freq_table */
	index = 0;
	cpufreq_frequency_table_target(policy, dbs_info->freq_table, freq_avg,
			CPUFREQ_RELATION_H, &index);
	freq_lo = dbs_info->freq_table[index].frequency;
	index = 0;
	cpufreq_frequency_table_target(policy, dbs_info->freq_table, freq_avg,
			CPUFREQ_RELATION_L, &index);
	freq_hi = dbs_info->freq_table[index].frequency;

	/* Find out how long we have to be in hi and lo freqs */
	if (freq_hi == freq_lo) {
		dbs_info->freq_lo = 0;
		dbs_info->freq_lo_jiffies = 0;
		return freq_lo;
	}
	jiffies_total = usecs_to_jiffies(sd_tuners->sampling_rate);
	jiffies_hi = (freq_avg - freq_lo) * jiffies_total;
	jiffies_hi += ((freq_hi - freq_lo) / 2);
	jiffies_hi /= (freq_hi - freq_lo);
	jiffies_lo = jiffies_total - jiffies_hi;
	dbs_info->freq_lo = freq_lo;
	dbs_info->freq_lo_jiffies = jiffies_lo;
	dbs_info->freq_hi_jiffies = jiffies_hi;
	return freq_hi;
}

static void sprdemand_powersave_bias_init(void)
{
	int i;
	for_each_online_cpu(i) {
		sprdemand_powersave_bias_init_cpu(i);
	}
}

static void dbs_freq_increase(struct cpufreq_policy *p, unsigned int freq)
{
	struct dbs_data *dbs_data = p->governor_data;
	struct sd_dbs_tuners *sd_tuners = NULL;

	if(NULL == dbs_data)
	{
		pr_info("dbs_freq_increase governor %s return\n", p->governor->name);
		if (g_sd_tuners == NULL)
			return ;
		sd_tuners = g_sd_tuners;
	}
	else
	{
		sd_tuners = dbs_data->tuners;
	}

	g_sd_tuners = sd_tuners;

	if (sd_tuners->powersave_bias)
		freq = sd_ops.powersave_bias_target(p, freq,
				CPUFREQ_RELATION_H);
	else if (p->cur == p->max)
		return;

	__cpufreq_driver_target(p, freq, sd_tuners->powersave_bias ?
			CPUFREQ_RELATION_L : CPUFREQ_RELATION_H);
}

static void sprd_unplug_one_cpu(struct work_struct *work)
{
	struct unplug_work_info *puwi = container_of(work,
		struct unplug_work_info, unplug_work.work);
	struct dbs_data *dbs_data = puwi->dbs_data;
	struct sd_dbs_tuners *sd_tuners = NULL;

	if(NULL == dbs_data)
	{
		pr_info("sprd_unplug_one_cpu return\n");
		if (g_sd_tuners == NULL)
			return ;
		sd_tuners = g_sd_tuners;
	}
	else
	{
		sd_tuners = dbs_data->tuners;
	}


#ifdef CONFIG_HOTPLUG_CPU
	if (num_online_cpus() > 1) {
		if (!sd_tuners->cpu_hotplug_disable) {
			pr_info("!!  we gonna unplug cpu%d  !!\n", puwi->cpuid);
			cpu_down(puwi->cpuid);
		}
	}
#endif
	return;
}

static void sprd_plugin_one_cpu(struct work_struct *work)
{
	int cpuid;
	struct cpufreq_policy *policy = cpufreq_cpu_get(0);
	struct dbs_data *dbs_data = policy->governor_data;
	struct sd_dbs_tuners *sd_tuners = NULL;

	if(NULL == dbs_data)
	{
		pr_info("sprd_plugin_one_cpu return\n");
		if (g_sd_tuners == NULL)
			return ;
		sd_tuners = g_sd_tuners;
	}
	else
	{
		sd_tuners = dbs_data->tuners;
	}


#ifdef CONFIG_HOTPLUG_CPU
	if (num_online_cpus() < sd_tuners->cpu_num_limit) {
		cpuid = cpumask_next_zero(0, cpu_online_mask);
		if (!sd_tuners->cpu_hotplug_disable) {
			pr_info("!!  we gonna plugin cpu%d  !!\n", cpuid);
			cpu_up(cpuid);
		}
	}
#endif
	return;
}

static int cpu_evaluate_score(struct sd_dbs_tuners *sd_tunners , unsigned int load)
{
	int score = 0;

	if (load >= sd_tunners->load_critical)
		score = sd_tunners->load_critical_score;
	else if (load >= sd_tunners->load_hi)
		score = sd_tunners->load_hi_score;
	else if (load >= sd_tunners->load_mid)
		score = sd_tunners->load_mid_score;
	else if (load >= sd_tunners->load_light)
		score = sd_tunners->load_light_score;
	else if (load >= sd_tunners->load_lo)
		score = sd_tunners->load_lo_score;
	else
		score = 0;

	return score;
}

/*
 * Every sampling_rate, we check, if current idle time is less than 20%
 * (default), then we try to increase frequency. Every sampling_rate, we look
 * for the lowest frequency which can sustain the load while keeping idle time
 * over 30%. If such a frequency exist, we try to decrease to this frequency.
 *
 * Any frequency increase takes it to the maximum frequency. Frequency reduction
 * happens at minimum steps of 5% (default) of current frequency
 */
static void sd_check_cpu(int cpu, unsigned int load_freq)
{
	struct od_cpu_dbs_info_s *dbs_info = &per_cpu(sd_cpu_dbs_info, cpu);
	struct cpufreq_policy *policy = dbs_info->cdbs.cur_policy;
	struct dbs_data *dbs_data = policy->governor_data;
	struct sd_dbs_tuners *sd_tuners = dbs_data->tuners;
	unsigned int local_load = 0;
	unsigned int itself_avg_load = 0;
	struct unplug_work_info *puwi;

	/* skip cpufreq adjustment if system enter into suspend */
	if(true == sd_tuners->is_suspend) {
		pr_info("%s: is_suspend=%s, skip cpufreq adjust\n",
			__func__, sd_tuners->is_suspend?"true":"false");
		goto plug_check;
	}

	dbs_info->freq_lo = 0;

	local_load = load_freq/policy->cur;

        printk("[DVFS] load %d %x load_freq %d policy->cur %d\n",local_load,local_load,load_freq,policy->cur);
	/* Check for frequency increase */
	if (load_freq > sd_tuners->up_threshold * policy->cur) {
		/* If switching to max speed, apply sampling_down_factor */
		if (policy->cur < policy->max)
			dbs_info->rate_mult =
				sd_tuners->sampling_down_factor;
		if(num_online_cpus() == sd_tuners->cpu_num_limit)
			dbs_freq_increase(policy, policy->max);
		else
			dbs_freq_increase(policy, policy->max-1);
		goto plug_check;
	}

	/* Check for frequency decrease */
	/* if we cannot reduce the frequency anymore, break out early */
	if (policy->cur == policy->min)
		goto plug_check;

	/*
	 * The optimal frequency is the frequency that is the lowest that can
	 * support the current CPU usage without triggering the up policy. To be
	 * safe, we focus 10 points under the threshold.
	 */
	if (load_freq < sd_tuners->adj_up_threshold
			* policy->cur) {
		unsigned int freq_next;
		freq_next = load_freq / sd_tuners->adj_up_threshold;

		/* No longer fully busy, reset rate_mult */
		dbs_info->rate_mult = 1;

		if (freq_next < policy->min)
			freq_next = policy->min;

		if (!sd_tuners->powersave_bias) {
			__cpufreq_driver_target(policy, freq_next,
					CPUFREQ_RELATION_L);
			goto plug_check;
		}

		freq_next = sd_ops.powersave_bias_target(policy, freq_next,
					CPUFREQ_RELATION_L);
		__cpufreq_driver_target(policy, freq_next, CPUFREQ_RELATION_L);
	}

plug_check:

	/* skip cpu hotplug check if hotplug is disabled */
	if (sd_tuners->cpu_hotplug_disable)
		return;

	/* cpu plugin check */
	spin_lock(&g_lock);
	if(num_online_cpus() < sd_tuners->cpu_num_limit) {
		cpu_score += cpu_evaluate_score(sd_tuners, local_load);
		if (cpu_score < 0)
			cpu_score = 0;
		if (cpu_score >= sd_tuners->cpu_score_up_threshold) {
			pr_info("cpu_score=%d, begin plugin cpu!\n", cpu_score);
			schedule_delayed_work_on(0, &plugin_work, 0);
			cpu_score = 0;
		}
	}
	spin_unlock(&g_lock);

	/* cpu unplug check */
	puwi = &per_cpu(uwi, policy->cpu);
	if(num_online_cpus() > 1) {
		percpu_total_load[policy->cpu] += local_load;
		percpu_check_count[policy->cpu]++;
		if(percpu_check_count[policy->cpu] == sd_tuners->cpu_down_count) {
			/* calculate itself's average load */
			itself_avg_load = percpu_total_load[policy->cpu]/sd_tuners->cpu_down_count;
			pr_debug("check unplug: for cpu%u avg_load=%d\n", policy->cpu, itself_avg_load);
			if(itself_avg_load < sd_tuners->cpu_down_threshold) {
				if (policy->cpu) {
					pr_info("cpu%u's avg_load=%d,begin unplug cpu\n",
						policy->cpu, itself_avg_load);
					schedule_delayed_work_on(0, &puwi->unplug_work, 0);
				}
			}
			percpu_check_count[policy->cpu] = 0;
			percpu_total_load[policy->cpu] = 0;
		}
	}
}

static void sd_dbs_timer(struct work_struct *work)
{
	struct od_cpu_dbs_info_s *dbs_info =
		container_of(work, struct od_cpu_dbs_info_s, cdbs.work.work);
	unsigned int cpu = dbs_info->cdbs.cur_policy->cpu;
	struct od_cpu_dbs_info_s *core_dbs_info = &per_cpu(sd_cpu_dbs_info,
			cpu);
	struct dbs_data *dbs_data = dbs_info->cdbs.cur_policy->governor_data;
	struct sd_dbs_tuners *sd_tuners = dbs_data->tuners;
	int delay = 0, sample_type = core_dbs_info->sample_type;
	bool modify_all = true;

	mutex_lock(&core_dbs_info->cdbs.timer_mutex);
	if(time_before(jiffies, boot_done))
		goto max_delay;

	if (!need_load_eval(&core_dbs_info->cdbs, sd_tuners->sampling_rate)) {
		modify_all = false;
		goto max_delay;
	}

	/* Common NORMAL_SAMPLE setup */
	core_dbs_info->sample_type = OD_NORMAL_SAMPLE;
	if (sample_type == OD_SUB_SAMPLE) {
		delay = core_dbs_info->freq_lo_jiffies;
		__cpufreq_driver_target(core_dbs_info->cdbs.cur_policy,
				core_dbs_info->freq_lo, CPUFREQ_RELATION_H);
	} else {
		dbs_check_cpu(dbs_data, cpu);
		if (core_dbs_info->freq_lo) {
			/* Setup timer for SUB_SAMPLE */
			core_dbs_info->sample_type = OD_SUB_SAMPLE;
			delay = core_dbs_info->freq_hi_jiffies;
		}
	}

max_delay:
	if (!delay)
		delay = delay_for_sampling_rate(sd_tuners->sampling_rate
				* core_dbs_info->rate_mult);

	gov_queue_work(dbs_data, dbs_info->cdbs.cur_policy, delay, modify_all);
	mutex_unlock(&core_dbs_info->cdbs.timer_mutex);
}

/************************** sysfs interface ************************/
static struct common_dbs_data sd_dbs_cdata;

/**
 * update_sampling_rate - update sampling rate effective immediately if needed.
 * @new_rate: new sampling rate
 *
 * If new rate is smaller than the old, simply updating
 * dbs_tuners_int.sampling_rate might not be appropriate. For example, if the
 * original sampling_rate was 1 second and the requested new sampling rate is 10
 * ms because the user needs immediate reaction from ondemand governor, but not
 * sure if higher frequency will be required or not, then, the governor may
 * change the sampling rate too late; up to 1 second later. Thus, if we are
 * reducing the sampling rate, we need to make the new value effective
 * immediately.
 */
static void update_sampling_rate(struct dbs_data *dbs_data,
		unsigned int new_rate)
{
	struct sd_dbs_tuners *sd_tuners = dbs_data->tuners;
	int cpu;

	sd_tuners->sampling_rate = new_rate = max(new_rate,
			dbs_data->min_sampling_rate);

	for_each_online_cpu(cpu) {
		struct cpufreq_policy *policy;
		struct od_cpu_dbs_info_s *dbs_info;
		unsigned long next_sampling, appointed_at;

		policy = cpufreq_cpu_get(cpu);
		if (!policy)
			continue;
		if (policy->governor != &cpufreq_gov_sprdemand) {
			cpufreq_cpu_put(policy);
			continue;
		}
		dbs_info = &per_cpu(sd_cpu_dbs_info, cpu);
		cpufreq_cpu_put(policy);

		mutex_lock(&dbs_info->cdbs.timer_mutex);

		if (!delayed_work_pending(&dbs_info->cdbs.work)) {
			mutex_unlock(&dbs_info->cdbs.timer_mutex);
			continue;
		}

		next_sampling = jiffies + usecs_to_jiffies(new_rate);
		appointed_at = dbs_info->cdbs.work.timer.expires;

		if (time_before(next_sampling, appointed_at)) {

			mutex_unlock(&dbs_info->cdbs.timer_mutex);
			cancel_delayed_work_sync(&dbs_info->cdbs.work);
			mutex_lock(&dbs_info->cdbs.timer_mutex);

			gov_queue_work(dbs_data, dbs_info->cdbs.cur_policy,
					usecs_to_jiffies(new_rate), true);

		}
		mutex_unlock(&dbs_info->cdbs.timer_mutex);
	}
}

static ssize_t store_sampling_rate(struct dbs_data *dbs_data, const char *buf,
		size_t count)
{
	unsigned int input;
	int ret;
	ret = sscanf(buf, "%u", &input);
	if (ret != 1)
		return -EINVAL;

	update_sampling_rate(dbs_data, input);
	return count;
}

static ssize_t store_io_is_busy(struct dbs_data *dbs_data, const char *buf,
		size_t count)
{
	struct sd_dbs_tuners *sd_tuners = dbs_data->tuners;
	unsigned int input;
	int ret;
	unsigned int j;

	ret = sscanf(buf, "%u", &input);
	if (ret != 1)
		return -EINVAL;
	sd_tuners->io_is_busy = !!input;

	/* we need to re-evaluate prev_cpu_idle */
	for_each_online_cpu(j) {
		struct od_cpu_dbs_info_s *dbs_info = &per_cpu(sd_cpu_dbs_info,
									j);
		dbs_info->cdbs.prev_cpu_idle = get_cpu_idle_time(j,
			&dbs_info->cdbs.prev_cpu_wall, sd_tuners->io_is_busy);
	}
	return count;
}

static ssize_t store_up_threshold(struct dbs_data *dbs_data, const char *buf,
		size_t count)
{
	struct sd_dbs_tuners *sd_tuners = dbs_data->tuners;
	unsigned int input;
	int ret;
	ret = sscanf(buf, "%u", &input);

	if (ret != 1 || input > MAX_FREQUENCY_UP_THRESHOLD ||
			input < MIN_FREQUENCY_UP_THRESHOLD) {
		return -EINVAL;
	}
	/* Calculate the new adj_up_threshold */
	sd_tuners->adj_up_threshold += input;
	sd_tuners->adj_up_threshold -= sd_tuners->up_threshold;

	sd_tuners->up_threshold = input;
	return count;
}

static ssize_t store_sampling_down_factor(struct dbs_data *dbs_data,
		const char *buf, size_t count)
{
	struct sd_dbs_tuners *sd_tuners = dbs_data->tuners;
	unsigned int input, j;
	int ret;
	ret = sscanf(buf, "%u", &input);

	if (ret != 1 || input > MAX_SAMPLING_DOWN_FACTOR || input < 1)
		return -EINVAL;
	sd_tuners->sampling_down_factor = input;

	/* Reset down sampling multiplier in case it was active */
	for_each_online_cpu(j) {
		struct od_cpu_dbs_info_s *dbs_info = &per_cpu(sd_cpu_dbs_info,
				j);
		dbs_info->rate_mult = 1;
	}
	return count;
}

static ssize_t store_ignore_nice(struct dbs_data *dbs_data, const char *buf,
		size_t count)
{
	struct sd_dbs_tuners *sd_tuners = dbs_data->tuners;
	unsigned int input;
	int ret;

	unsigned int j;

	ret = sscanf(buf, "%u", &input);
	if (ret != 1)
		return -EINVAL;

	if (input > 1)
		input = 1;

	if (input == sd_tuners->ignore_nice) { /* nothing to do */
		return count;
	}
	sd_tuners->ignore_nice = input;

	/* we need to re-evaluate prev_cpu_idle */
	for_each_online_cpu(j) {
		struct od_cpu_dbs_info_s *dbs_info;
		dbs_info = &per_cpu(sd_cpu_dbs_info, j);
		dbs_info->cdbs.prev_cpu_idle = get_cpu_idle_time(j,
			&dbs_info->cdbs.prev_cpu_wall, sd_tuners->io_is_busy);
		if (sd_tuners->ignore_nice)
			dbs_info->cdbs.prev_cpu_nice =
				kcpustat_cpu(j).cpustat[CPUTIME_NICE];

	}
	return count;
}

static ssize_t store_powersave_bias(struct dbs_data *dbs_data, const char *buf,
		size_t count)
{
	struct sd_dbs_tuners *sd_tuners = dbs_data->tuners;
	unsigned int input;
	int ret;
	ret = sscanf(buf, "%u", &input);

	if (ret != 1)
		return -EINVAL;

	if (input > 1000)
		input = 1000;

	sd_tuners->powersave_bias = input;
	sprdemand_powersave_bias_init();
	return count;
}

static ssize_t store_cpu_num_limit(struct dbs_data *dbs_data, const char *buf,
		size_t count)
{
	struct sd_dbs_tuners *sd_tuners = dbs_data->tuners;
	unsigned int input;
	int ret;
	ret = sscanf(buf, "%u", &input);

	if (ret != 1) {
		return -EINVAL;
	}
	sd_tuners->cpu_num_limit = input;
	return count;
}

static ssize_t store_cpu_score_up_threshold(struct dbs_data *dbs_data, const char *buf,
		size_t count)
{
	struct sd_dbs_tuners *sd_tuners = dbs_data->tuners;
	unsigned int input;
	int ret;
	ret = sscanf(buf, "%u", &input);

	if (ret != 1) {
		return -EINVAL;
	}
	sd_tuners->cpu_score_up_threshold = input;
	return count;
}

static ssize_t store_load_critical(struct dbs_data *dbs_data, const char *buf,
		size_t count)
{
	struct sd_dbs_tuners *sd_tuners = dbs_data->tuners;
	unsigned int input;
	int ret;
	ret = sscanf(buf, "%u", &input);

	if (ret != 1) {
		return -EINVAL;
	}
	sd_tuners->load_critical = input;
	return count;
}

static ssize_t store_load_hi(struct dbs_data *dbs_data, const char *buf,
		size_t count)
{
	struct sd_dbs_tuners *sd_tuners = dbs_data->tuners;
	unsigned int input;
	int ret;
	ret = sscanf(buf, "%u", &input);

	if (ret != 1) {
		return -EINVAL;
	}
	sd_tuners->load_hi = input;
	return count;
}

static ssize_t store_load_mid(struct dbs_data *dbs_data, const char *buf,
		size_t count)
{
	struct sd_dbs_tuners *sd_tuners = dbs_data->tuners;
	unsigned int input;
	int ret;
	ret = sscanf(buf, "%u", &input);

	if (ret != 1) {
		return -EINVAL;
	}
	sd_tuners->load_mid = input;
	return count;
}

static ssize_t store_load_light(struct dbs_data *dbs_data, const char *buf,
		size_t count)
{
	struct sd_dbs_tuners *sd_tuners = dbs_data->tuners;
	unsigned int input;
	int ret;
	ret = sscanf(buf, "%u", &input);

	if (ret != 1) {
		return -EINVAL;
	}
	sd_tuners->load_light = input;
	return count;
}

static ssize_t store_load_lo(struct dbs_data *dbs_data, const char *buf,
		size_t count)
{
	struct sd_dbs_tuners *sd_tuners = dbs_data->tuners;
	unsigned int input;
	int ret;
	ret = sscanf(buf, "%u", &input);

	if (ret != 1) {
		return -EINVAL;
	}
	sd_tuners->load_lo = input;
	return count;
}

static ssize_t store_load_critical_score(struct dbs_data *dbs_data, const char *buf,
		size_t count)
{
	struct sd_dbs_tuners *sd_tuners = dbs_data->tuners;
	int input;
	int ret;
	ret = sscanf(buf, "%d", &input);

	if (ret != 1) {
		return -EINVAL;
	}
	sd_tuners->load_critical_score = input;
	return count;
}

static ssize_t store_load_hi_score(struct dbs_data *dbs_data, const char *buf,
		size_t count)
{
	struct sd_dbs_tuners *sd_tuners = dbs_data->tuners;
	int input;
	int ret;
	ret = sscanf(buf, "%d", &input);

	if (ret != 1) {
		return -EINVAL;
	}
	sd_tuners->load_hi_score = input;
	return count;
}


static ssize_t store_load_mid_score(struct dbs_data *dbs_data, const char *buf,
		size_t count)
{
	struct sd_dbs_tuners *sd_tuners = dbs_data->tuners;
	int input;
	int ret;
	ret = sscanf(buf, "%d", &input);

	if (ret != 1) {
		return -EINVAL;
	}
	sd_tuners->load_mid_score = input;
	return count;
}

static ssize_t store_load_light_score(struct dbs_data *dbs_data, const char *buf,
		size_t count)
{
	struct sd_dbs_tuners *sd_tuners = dbs_data->tuners;
	int input;
	int ret;
	ret = sscanf(buf, "%d", &input);

	if (ret != 1) {
		return -EINVAL;
	}
	sd_tuners->load_light_score = input;
	return count;
}

static ssize_t store_load_lo_score(struct dbs_data *dbs_data, const char *buf,
		size_t count)
{
	struct sd_dbs_tuners *sd_tuners = dbs_data->tuners;
	int input;
	int ret;
	ret = sscanf(buf, "%d", &input);

	if (ret != 1) {
		return -EINVAL;
	}
	sd_tuners->load_lo_score = input;
	return count;
}

static ssize_t store_cpu_down_threshold(struct dbs_data *dbs_data, const char *buf,
		size_t count)
{
	struct sd_dbs_tuners *sd_tuners = dbs_data->tuners;
	unsigned int input;
	int ret;
	ret = sscanf(buf, "%u", &input);

	if (ret != 1) {
		return -EINVAL;
	}
	sd_tuners->cpu_down_threshold = input;
	return count;
}

static ssize_t store_cpu_down_count(struct dbs_data *dbs_data, const char *buf,
		size_t count)
{
	struct sd_dbs_tuners *sd_tuners = dbs_data->tuners;
	unsigned int input;
	int ret;
	ret = sscanf(buf, "%u", &input);

	if (ret != 1) {
		return -EINVAL;
	}
	sd_tuners->cpu_down_count = input;
	return count;
}

static ssize_t store_cpu_hotplug_disable(struct dbs_data *dbs_data, const char *buf,
		size_t count)
{
	struct sd_dbs_tuners *sd_tuners = dbs_data->tuners;
	unsigned int input, cpu;
	int ret;
	ret = sscanf(buf, "%u", &input);

	if (ret != 1) {
		return -EINVAL;
	}

	if (sd_tuners->cpu_hotplug_disable == input) {
		return count;
	}
	if (sd_tuners->cpu_num_limit > 1)
		sd_tuners->cpu_hotplug_disable = input;

	if(sd_tuners->cpu_hotplug_disable > 0)
		cpu_hotplug_disable_set = true;
	else
		cpu_hotplug_disable_set = false;

	smp_wmb();
	/* plug-in all offline cpu mandatory if we didn't
	 * enbale CPU_DYNAMIC_HOTPLUG
         */
#ifdef CONFIG_HOTPLUG_CPU
	if (sd_tuners->cpu_hotplug_disable) {
		for_each_cpu(cpu, cpu_possible_mask) {
			if (!cpu_online(cpu))
				{
				cpu_up(cpu);
			  }
		}
	}
#endif
	return count;
}


show_store_one(sd, sampling_rate);
show_store_one(sd, io_is_busy);
show_store_one(sd, up_threshold);
show_store_one(sd, sampling_down_factor);
show_store_one(sd, ignore_nice);
show_store_one(sd, powersave_bias);
declare_show_sampling_rate_min(sd);
show_store_one(sd, cpu_score_up_threshold);
show_store_one(sd, load_critical);
show_store_one(sd, load_hi);
show_store_one(sd, load_mid);
show_store_one(sd, load_light);
show_store_one(sd, load_lo);
show_store_one(sd, load_critical_score);
show_store_one(sd, load_hi_score);
show_store_one(sd, load_mid_score);
show_store_one(sd, load_light_score);
show_store_one(sd, load_lo_score);
show_store_one(sd, cpu_down_threshold);
show_store_one(sd, cpu_down_count);
show_store_one(sd, cpu_hotplug_disable);
show_store_one(sd, cpu_num_limit);

gov_sys_pol_attr_rw(sampling_rate);
gov_sys_pol_attr_rw(io_is_busy);
gov_sys_pol_attr_rw(up_threshold);
gov_sys_pol_attr_rw(sampling_down_factor);
gov_sys_pol_attr_rw(ignore_nice);
gov_sys_pol_attr_rw(powersave_bias);
gov_sys_pol_attr_ro(sampling_rate_min);
gov_sys_pol_attr_rw(cpu_score_up_threshold);
gov_sys_pol_attr_rw(load_critical);
gov_sys_pol_attr_rw(load_hi);
gov_sys_pol_attr_rw(load_mid);
gov_sys_pol_attr_rw(load_light);
gov_sys_pol_attr_rw(load_lo);
gov_sys_pol_attr_rw(load_critical_score);
gov_sys_pol_attr_rw(load_hi_score);
gov_sys_pol_attr_rw(load_mid_score);
gov_sys_pol_attr_rw(load_light_score);
gov_sys_pol_attr_rw(load_lo_score);
gov_sys_pol_attr_rw(cpu_down_threshold);
gov_sys_pol_attr_rw(cpu_down_count);
gov_sys_pol_attr_rw(cpu_hotplug_disable);
gov_sys_pol_attr_rw(cpu_num_limit);

static struct attribute *dbs_attributes_gov_sys[] = {
	&sampling_rate_min_gov_sys.attr,
	&sampling_rate_gov_sys.attr,
	&up_threshold_gov_sys.attr,
	&sampling_down_factor_gov_sys.attr,
	&ignore_nice_gov_sys.attr,
	&powersave_bias_gov_sys.attr,
	&io_is_busy_gov_sys.attr,
	&cpu_score_up_threshold_gov_sys.attr,
	&load_critical_gov_sys.attr,
	&load_hi_gov_sys.attr,
	&load_mid_gov_sys.attr,
	&load_light_gov_sys.attr,
	&load_lo_gov_sys.attr,
	&load_critical_score_gov_sys.attr,
	&load_hi_score_gov_sys.attr,
	&load_mid_score_gov_sys.attr,
	&load_light_score_gov_sys.attr,
	&load_lo_score_gov_sys.attr,
	&cpu_down_threshold_gov_sys.attr,
	&cpu_down_count_gov_sys.attr,
	&cpu_hotplug_disable_gov_sys.attr,
	&cpu_num_limit_gov_sys.attr,
	NULL
};

static struct attribute_group sd_attr_group_gov_sys = {
	.attrs = dbs_attributes_gov_sys,
	.name = "sprdemand",
};

static struct attribute *dbs_attributes_gov_pol[] = {
	&sampling_rate_min_gov_pol.attr,
	&sampling_rate_gov_pol.attr,
	&up_threshold_gov_pol.attr,
	&sampling_down_factor_gov_pol.attr,
	&ignore_nice_gov_pol.attr,
	&powersave_bias_gov_pol.attr,
	&io_is_busy_gov_pol.attr,
	&cpu_score_up_threshold_gov_pol.attr,
	&load_critical_gov_pol.attr,
	&load_hi_gov_pol.attr,
	&load_mid_gov_pol.attr,
	&load_light_gov_pol.attr,
	&load_lo_gov_pol.attr,
	&load_critical_score_gov_pol.attr,
	&load_hi_score_gov_pol.attr,
	&load_mid_score_gov_pol.attr,
	&load_light_score_gov_pol.attr,
	&load_lo_score_gov_pol.attr,
	&cpu_down_threshold_gov_pol.attr,
	&cpu_down_count_gov_pol.attr,
	&cpu_hotplug_disable_gov_pol.attr,
	&cpu_num_limit_gov_pol.attr,
	NULL
};

static struct attribute_group sd_attr_group_gov_pol = {
	.attrs = dbs_attributes_gov_pol,
	.name = "sprdemand",
};

/************************** sysfs end ************************/

static int sd_init(struct dbs_data *dbs_data)
{
	struct sd_dbs_tuners *tuners;
	u64 idle_time;
	int cpu, i;
	struct unplug_work_info *puwi;

	tuners = kzalloc(sizeof(struct sd_dbs_tuners), GFP_KERNEL);
	if (!tuners) {
		pr_err("%s: kzalloc failed\n", __func__);
		return -ENOMEM;
	}

	cpu = get_cpu();
	idle_time = get_cpu_idle_time_us(cpu, NULL);
	put_cpu();
	if (idle_time != -1ULL) {
		/* Idle micro accounting is supported. Use finer thresholds */
		tuners->up_threshold = MICRO_FREQUENCY_UP_THRESHOLD;
		tuners->adj_up_threshold = MICRO_FREQUENCY_UP_THRESHOLD -
			MICRO_FREQUENCY_DOWN_DIFFERENTIAL;
		/*
		 * In nohz/micro accounting case we set the minimum frequency
		 * not depending on HZ, but fixed (very low). The deferred
		 * timer might skip some samples if idle/sleeping as needed.
		*/
		dbs_data->min_sampling_rate = MICRO_FREQUENCY_MIN_SAMPLE_RATE;
	} else {
		tuners->up_threshold = DEF_FREQUENCY_UP_THRESHOLD;
		tuners->adj_up_threshold = DEF_FREQUENCY_UP_THRESHOLD -
			DEF_FREQUENCY_DOWN_DIFFERENTIAL;

		/* For correct statistics, we need 10 ticks for each measure */
		dbs_data->min_sampling_rate = MIN_SAMPLING_RATE_RATIO *
			jiffies_to_usecs(10);
	}

	tuners->sampling_down_factor = DEF_SAMPLING_DOWN_FACTOR;
	tuners->ignore_nice = 0;
	tuners->powersave_bias = 0;
	tuners->io_is_busy = should_io_be_busy();

	tuners->cpu_hotplug_disable = true;
	tuners->is_suspend = false;
	tuners->cpu_score_up_threshold = DEF_CPU_SCORE_UP_THRESHOLD;
	tuners->load_critical = LOAD_CRITICAL;
	tuners->load_hi = LOAD_HI;
	tuners->load_mid = LOAD_MID;
	tuners->load_light = LOAD_LIGHT;
	tuners->load_lo = LOAD_LO;
	tuners->load_critical_score = LOAD_CRITICAL_SCORE;
	tuners->load_hi_score = LOAD_HI_SCORE;
	tuners->load_mid_score = LOAD_MID_SCORE;
	tuners->load_light_score = LOAD_LIGHT_SCORE;
	tuners->load_lo_score = LOAD_LO_SCORE;
	tuners->cpu_down_threshold = DEF_CPU_LOAD_DOWN_THRESHOLD;
	tuners->cpu_down_count = DEF_CPU_DOWN_COUNT;
	tuners->cpu_num_limit = nr_cpu_ids;
	if (tuners->cpu_num_limit > 1)
		tuners->cpu_hotplug_disable = false;

	dbs_data->tuners = tuners;
	mutex_init(&dbs_data->mutex);

	INIT_DELAYED_WORK(&plugin_work, sprd_plugin_one_cpu);
	for_each_possible_cpu(i) {
		puwi = &per_cpu(uwi, i);
		puwi->cpuid = i;
		puwi->dbs_data = dbs_data;
		INIT_DELAYED_WORK(&puwi->unplug_work, sprd_unplug_one_cpu);
	}

	return 0;
}

static void sd_exit(struct dbs_data *dbs_data)
{
	kfree(dbs_data->tuners);
}

define_get_cpu_dbs_routines(sd_cpu_dbs_info);

static struct od_ops sd_ops = {
	.powersave_bias_init_cpu = sprdemand_powersave_bias_init_cpu,
	.powersave_bias_target = generic_powersave_bias_target,
	.freq_increase = dbs_freq_increase,
};

static struct common_dbs_data sd_dbs_cdata = {
	/* sprdemand belong to ondemand gov */
	.governor = GOV_ONDEMAND,
	.attr_group_gov_sys = &sd_attr_group_gov_sys,
	.attr_group_gov_pol = &sd_attr_group_gov_pol,
	.get_cpu_cdbs = get_cpu_cdbs,
	.get_cpu_dbs_info_s = get_cpu_dbs_info_s,
	.gov_dbs_timer = sd_dbs_timer,
	.gov_check_cpu = sd_check_cpu,
	.gov_ops = &sd_ops,
	.init = sd_init,
	.exit = sd_exit,
};

static int sd_cpufreq_governor_dbs(struct cpufreq_policy *policy,
		unsigned int event)
{
	return cpufreq_governor_dbs(policy, &sd_dbs_cdata, event);
}

#ifndef CONFIG_CPU_FREQ_DEFAULT_GOV_SPRDEMAND
static
#endif
struct cpufreq_governor cpufreq_gov_sprdemand = {
	.name			= "sprdemand",
	.governor		= sd_cpufreq_governor_dbs,
	.max_transition_latency	= TRANSITION_LATENCY_LIMIT,
	.owner			= THIS_MODULE,
};

static int get_max_state(struct thermal_cooling_device *cdev,
			 unsigned long *state)
{
	int ret = 0;

	*state = 2;

	return ret;
}

static int get_cur_state(struct thermal_cooling_device *cdev,
			 unsigned long *state)
{
	int ret = 0;

	*state = thermal_cooling_info.cooling_state;

	return ret;
}

static int set_cur_state(struct thermal_cooling_device *cdev,
			 unsigned long state)
{
	int ret = 0, cpu;
	struct cpufreq_policy *policy = cpufreq_cpu_get(0);
	struct dbs_data *dbs_data = policy->governor_data;
	struct sd_dbs_tuners *sd_tuners = NULL;

	if(NULL == dbs_data)
	{
		pr_info("set_cur_state governor %s return\n", policy->governor->name);
		if (g_sd_tuners == NULL)
			return ret;
		sd_tuners = g_sd_tuners;
	}
	else
	{
		sd_tuners = dbs_data->tuners;
	}


	thermal_cooling_info.cooling_state = state;
	if (state) {
		pr_info("%s:cpufreq heating up\n", __func__);
		if (sd_tuners->cpu_num_limit > 1)
			if(cpu_hotplug_disable_set == false)
				sd_tuners->cpu_hotplug_disable = true;
		dbs_freq_increase(policy, policy->max-1);
		/* unplug all online cpu except cpu0 mandatory */
#ifdef CONFIG_HOTPLUG_CPU
		for_each_online_cpu(cpu) {
			if (cpu)
				{
				cpu_down(cpu);
			  }
		}
#endif
	} else {
		pr_info("%s:cpufreq cooling down\n", __func__);
		if (sd_tuners->cpu_num_limit > 1)
			if(cpu_hotplug_disable_set == false)
				sd_tuners->cpu_hotplug_disable = false;
		/* plug-in all offline cpu mandatory if we didn't
		  * enbale CPU_DYNAMIC_HOTPLUG
		 */
#ifdef CONFIG_HOTPLUG_CPU
		for_each_cpu(cpu, cpu_possible_mask) {
			if (!cpu_online(cpu))
				{
				cpu_up(cpu);
			  }
		}
#endif
	}

	g_sd_tuners = sd_tuners;
	return ret;
}

static struct thermal_cooling_device_ops sprd_cpufreq_cooling_ops = {
	.get_max_state = get_max_state,
	.get_cur_state = get_cur_state,
	.set_cur_state = set_cur_state,
};

static int sprdemand_gov_pm_notifier_call(struct notifier_block *nb,
	unsigned long event, void *dummy)
{
	struct cpufreq_policy *policy = cpufreq_cpu_get(0);
	struct dbs_data *dbs_data = policy->governor_data;
	struct sd_dbs_tuners *sd_tuners = NULL;

	if(NULL == dbs_data)
	{
		pr_info("sprdemand_gov_pm_notifier_call governor %s return\n", policy->governor->name);
		if (g_sd_tuners == NULL)
			return NOTIFY_OK;
		sd_tuners = g_sd_tuners;
	}
	else
	{
		sd_tuners = dbs_data->tuners;
	}

	/* in suspend and hibernation process, we need set frequency to the orignal
	 * one to make sure all things go right */
	if (event == PM_SUSPEND_PREPARE || event == PM_HIBERNATION_PREPARE) {
		pr_info(" %s, recv pm suspend notify\n", __func__ );
		if (sd_tuners->cpu_num_limit > 1)
			if(cpu_hotplug_disable_set == false)
				sd_tuners->cpu_hotplug_disable = true;
		sd_tuners->is_suspend = true;
		dbs_freq_increase(policy, policy->max);
		pr_info(" %s, recv pm suspend notify done\n", __func__ );
	}

	g_sd_tuners = sd_tuners;
  
	return NOTIFY_OK;
}

static struct notifier_block sprdemand_gov_pm_notifier = {
	.notifier_call = sprdemand_gov_pm_notifier_call,
};

#ifdef CONFIG_HAS_EARLYSUSPEND
static void sprdemand_gov_early_suspend(struct early_suspend *h)
{
	pr_info("%s do nothing\n", __func__);
	return;
}

struct sd_dbs_tuners *g_sd_tuners = NULL;

static void sprdemand_gov_late_resume(struct early_suspend *h)
{
	struct cpufreq_policy *policy = cpufreq_cpu_get(0);
	struct dbs_data *dbs_data = policy->governor_data;
	struct sd_dbs_tuners *sd_tuners = NULL;

        if(NULL == dbs_data)
        {
	    pr_info("sprdemand_gov_late_resume governor %s return\n", policy->governor->name);
	    if (g_sd_tuners == NULL)
	    	return;
	    if (g_sd_tuners->cpu_num_limit > 1)
		    if(cpu_hotplug_disable_set == false)
			    g_sd_tuners->cpu_hotplug_disable = false;
	    g_sd_tuners->is_suspend = false;
  		return;
        }
        sd_tuners = dbs_data->tuners;

	if (sd_tuners->cpu_num_limit > 1)
		if(cpu_hotplug_disable_set == false)
			sd_tuners->cpu_hotplug_disable = false;
	sd_tuners->is_suspend = false;

        g_sd_tuners = sd_tuners;
	return;
}

static struct early_suspend sprdemand_gov_earlysuspend_handler = {
	.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN,
	.suspend = sprdemand_gov_early_suspend,
	.resume = sprdemand_gov_late_resume,
};
#endif

static int __init cpufreq_gov_dbs_init(void)
{
	boot_done = jiffies + GOVERNOR_BOOT_TIME;

	thermal_cooling_info.cdev = thermal_cooling_device_register("thermal-cpufreq-0", 0,
						&sprd_cpufreq_cooling_ops);
	if (IS_ERR(thermal_cooling_info.cdev))
		return PTR_ERR(thermal_cooling_info.cdev);

	register_pm_notifier(&sprdemand_gov_pm_notifier);
#ifdef CONFIG_HAS_EARLYSUSPEND
	register_early_suspend(&sprdemand_gov_earlysuspend_handler);
#endif

	return cpufreq_register_governor(&cpufreq_gov_sprdemand);
}

static void __exit cpufreq_gov_dbs_exit(void)
{
	cpufreq_unregister_governor(&cpufreq_gov_sprdemand);
#ifdef CONFIG_HAS_EARLYSUSPEND
	unregister_early_suspend(&sprdemand_gov_earlysuspend_handler);
#endif
	unregister_pm_notifier(&sprdemand_gov_pm_notifier);
	thermal_cooling_device_unregister(thermal_cooling_info.cdev);
}

MODULE_AUTHOR("Venkatesh Pallipadi <venkatesh.pallipadi@intel.com>");
MODULE_AUTHOR("Alexey Starikovskiy <alexey.y.starikovskiy@intel.com>");
MODULE_DESCRIPTION("'cpufreq_sprdemand' - A dynamic cpufreq governor for "
	"Low Latency Frequency Transition capable processors");
MODULE_LICENSE("GPL");

#ifdef CONFIG_CPU_FREQ_DEFAULT_GOV_SPRDEMAND
fs_initcall(cpufreq_gov_dbs_init);
#else
module_init(cpufreq_gov_dbs_init);
#endif
module_exit(cpufreq_gov_dbs_exit);
