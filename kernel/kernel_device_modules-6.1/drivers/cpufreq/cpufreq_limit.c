/* driver/cpufreq/cpufreq_limit.c
 *
 * CPU limit driver for MTK
 *
 * Copyright (c) 2022 Samsung Electronics Co., Ltd.
 *
 * Author: Sangyoung Son <hello.son@samsung.com>
 *
 * Change limit driver code for MTK chipset by Jonghyeon Cho
 * Author: Jonghyeon Cho <jongjaaa.cho@samsung.com>
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

#include <linux/module.h>
#include <linux/sysfs.h>
#include <linux/err.h>
#include <linux/suspend.h>
#include <linux/cpu.h>
#include <linux/kobject.h>
#include <linux/cpufreq.h>
#include <linux/cpufreq_limit.h>
#include <linux/platform_device.h>
#if IS_ENABLED(CONFIG_OF)
#include <linux/of.h>
#endif

#define MAX_BUF_SIZE 1024
#define LIMIT_RELEASE -1
#define MIN(a, b)     (((a) < (b)) ? (a) : (b))
#define MAX(a, b)     (((a) > (b)) ? (a) : (b))

static struct freq_qos_request *max_req[DVFS_MAX_ID];
static struct freq_qos_request *min_req[DVFS_MAX_ID];
static struct kobject *cpufreq_kobj;

/* boosted state */
#define BOOSTED	1
#define NOT_BOOSTED	0

struct freq_map {
	unsigned int in;
	unsigned int out;
};

struct input_info {
	int boosted;
	int min;
	int max;
	u64 time_in_min_limit;
	u64 time_in_max_limit;
	u64 time_in_over_limit;
	ktime_t last_min_limit_time;
	ktime_t last_max_limit_time;
	ktime_t last_over_limit_time;
};
static struct input_info freq_input[DVFS_MAX_ID];

/* cpu frequency table from cpufreq dt parse */
static struct cpufreq_frequency_table *cpuftbl_ltl;
static struct cpufreq_frequency_table *cpuftbl_big;

static int ltl_max_freq_div;
static int last_min_req_val;
static int last_max_req_val;
DEFINE_MUTEX(cpufreq_limit_mutex);

/*
 * The default value will be updated
 * from device tree in the future.
 */
static struct cpufreq_limit_parameter param = {
	.num_cpu				= 8,
	.freq_count				= 0,

	.ltl_cpu_start			= 0,
	.mid_cpu_start			= 4,
	.big_cpu_start			= 7,

	/* virt freq */
	.ltl_max_freq			= 0,
	.ltl_min_freq			= 0,
	.big_max_freq			= 0,
	.big_min_freq			= 0,

	.ltl_min_lock_freq		= 1100000,
	.big_max_lock_freq		= 750000,
	.ltl_divider			= 4,
	.over_limit				= 0,
};

/**
 * cpufreq_limit_unify_table - make virtual table for limit driver
 */
static void cpufreq_limit_unify_table(void)
{
	unsigned int freq, i, count = 0;
	unsigned int freq_count = 0;
	unsigned int ltl_max_freq = 0;

	/* big cluster table */
	for (i = 0; cpuftbl_big[i].frequency != CPUFREQ_TABLE_END; i++)
		count = i;

	for (i = 0; i < count + 1; i++) {
		freq = cpuftbl_big[i].frequency;

		if (freq == CPUFREQ_ENTRY_INVALID) {
			pr_info("%s: invalid entry in big freq", __func__);
			continue;
		}

		if (freq < param.big_min_freq || freq > param.big_max_freq)
			continue;

		param.unified_table[freq_count++] = freq;
	}

	/* little cluster table */
	for (i = 0; cpuftbl_ltl[i].frequency != CPUFREQ_TABLE_END; i++)
		count = i;

	for (i = 0; i < count + 1; i++) {
		freq = cpuftbl_ltl[i].frequency / param.ltl_divider;

		if (freq == CPUFREQ_ENTRY_INVALID) {
			pr_info("%s: invalid entry in ltl freq", __func__);
			continue;
		}

		if (freq < param.ltl_min_freq ||
				freq > param.ltl_max_freq)
			continue;

		param.unified_table[freq_count++] = freq;
	}

	last_min_req_val = -1;
	last_max_req_val = -1;

	ltl_max_freq_div = (int)(ltl_max_freq / param.ltl_divider);

	if (freq_count) {
		pr_debug("%s: unified table is made\n", __func__);
		param.freq_count = freq_count;
	} else {
		pr_err("%s: cannot make unified table\n", __func__);
	}
}

static int cpufreq_limit_get_ltl_boost(int freq)
{
	int i;

	for (i = 0; i < param.boost_map_size; i++)
		if (freq >= param.ltl_boost_map[i].in)
			return param.ltl_boost_map[i].out;
	return freq * param.ltl_divider;
}

static int cpufreq_limit_get_ltl_limit(int freq)
{
	int i;

	/* big limit condition */
	for (i = 0; i < param.limit_map_size; i++)
		if (freq >= param.ltl_limit_map[i].in)
			return MIN(param.ltl_limit_map[i].out, param.l_fmax);

	/* little limit condition */
	return freq * param.ltl_divider;
}

static bool cpufreq_limit_check_over_limit_condition(void)
{
	if ((freq_input[DVFS_USER_ID].min > 0) ||
		(freq_input[DVFS_TOUCH_ID].min > 0)) {
		if (freq_input[DVFS_USER_ID].min > (int)param.ltl_max_freq) {
			pr_debug("%s: userspace minlock (%d) > ltl max (%d)\n",
				__func__, freq_input[DVFS_USER_ID].min, param.ltl_max_freq);
			return true;
		}
	}
	return false;
}

static bool cpufreq_limit_max_lock_need_restore(void)
{
	if ((int)param.over_limit <= 0)
		return false;
	return !cpufreq_limit_check_over_limit_condition();
}

static bool cpufreq_limit_high_pri_min_lock_required(void)
{
	if ((int)param.over_limit <= 0)
		return false;
	return cpufreq_limit_check_over_limit_condition();
}

/**
 * cpufreq_limit_set_table - little, big frequency table setting
 */
static void cpufreq_limit_set_table(int cpu,
		struct cpufreq_frequency_table *table)
{
	int i, count = 0;

	unsigned int max_freq_b = 0, min_freq_b = UINT_MAX;
	unsigned int max_freq_l = 0, min_freq_l = UINT_MAX;

	if (cpu == param.big_cpu_start)
		cpuftbl_big = table;
	else if (cpu == param.ltl_cpu_start)
		cpuftbl_ltl = table;

	/*
	 * if the freq table is configured,
	 * start producing the unified frequency table.
	 */
	if (!cpuftbl_ltl || !cpuftbl_big)
		return;

	pr_info("%s: cpufreq table is ready\n", __func__);

	/* update big config */
	for (i = 0; cpuftbl_big[i].frequency != CPUFREQ_TABLE_END; i++)
		count = i;

	for (i = 0; i < count + 1; i++) {
		if (cpuftbl_big[i].frequency == CPUFREQ_ENTRY_INVALID) {
			pr_info("%s: invalid entry in big freq", __func__);
			continue;
		}

		if ((cpuftbl_big[i].frequency < min_freq_b) &&
			(cpuftbl_big[i].frequency > param.ltl_max_freq))
			min_freq_b = cpuftbl_big[i].frequency;

		if (cpuftbl_big[i].frequency > max_freq_b)
			max_freq_b = cpuftbl_big[i].frequency;

	}
	if (!param.big_min_freq)
		param.big_min_freq = min_freq_b;
	if (!param.big_max_freq)
		param.big_max_freq = max_freq_b;

	/* update little config */
	for (i = 0; cpuftbl_ltl[i].frequency != CPUFREQ_TABLE_END; i++)
		count = i;

	for (i = 0; i < count + 1; i++) {
		if (cpuftbl_ltl[i].frequency == CPUFREQ_ENTRY_INVALID) {
			pr_info("%s: invalid entry in ltl freq", __func__);
			continue;
		}

		if (cpuftbl_ltl[i].frequency < min_freq_l)
			min_freq_l = cpuftbl_ltl[i].frequency;

		if (cpuftbl_ltl[i].frequency > max_freq_l)
			max_freq_l = cpuftbl_ltl[i].frequency;

	}
	if (!param.ltl_min_freq)
		param.ltl_min_freq = min_freq_l / param.ltl_divider;
	if (!param.ltl_max_freq)
		param.ltl_max_freq = max_freq_l / param.ltl_divider;

	pr_info("%s: updated: little(%u-%u), big(%u-%u)\n", __func__,
				param.ltl_min_freq, param.ltl_max_freq,
				param.big_min_freq, param.big_max_freq);

	cpufreq_limit_unify_table();
}

static s32 cpufreq_limit_freq_qos_read_value(struct freq_constraints *qos,
			enum freq_qos_req_type type)
{
	s32 ret;

	switch (type) {
	case FREQ_QOS_MIN:
		ret = IS_ERR_OR_NULL(qos) ?
			FREQ_QOS_MIN_DEFAULT_VALUE :
			READ_ONCE(qos->min_freq.target_value);
		break;
	case FREQ_QOS_MAX:
		ret = IS_ERR_OR_NULL(qos) ?
			FREQ_QOS_MAX_DEFAULT_VALUE :
			READ_ONCE(qos->max_freq.target_value);
		break;
	default:
		WARN_ON(1);
		ret = 0;
	}

	return ret;
}

static void cpufreq_limit_update_current_freq(void)
{
	struct cpufreq_policy *policy;
	int l_min = 0, l_max = 0;
	int m_min = 0, m_max = 0;
	int b_min = 0, b_max = 0;

	policy = cpufreq_cpu_get(param.ltl_cpu_start);
	if (policy) {
		l_min = cpufreq_limit_freq_qos_read_value(&policy->constraints, FREQ_QOS_MIN);
		l_max = cpufreq_limit_freq_qos_read_value(&policy->constraints, FREQ_QOS_MAX);
		cpufreq_cpu_put(policy);
	}

	policy = cpufreq_cpu_get(param.mid_cpu_start);
	if (policy) {
		m_min = cpufreq_limit_freq_qos_read_value(&policy->constraints, FREQ_QOS_MIN);
		m_max = cpufreq_limit_freq_qos_read_value(&policy->constraints, FREQ_QOS_MAX);
		cpufreq_cpu_put(policy);
	}

	policy = cpufreq_cpu_get(param.big_cpu_start);
	if (policy) {
		b_min = cpufreq_limit_freq_qos_read_value(&policy->constraints, FREQ_QOS_MIN);
		b_max = cpufreq_limit_freq_qos_read_value(&policy->constraints, FREQ_QOS_MAX);
		cpufreq_cpu_put(policy);
	}

	pr_info("%s: current freq: ltl(%d ~ %d), mid(%d ~ %d), big(%d ~ %d)\n",
			__func__, l_min, l_max, m_min, m_max, b_min, b_max);
}

static void cpufreq_limit_process_over_limit(unsigned int id, bool need_update_user_max, int new_user_max)
{
	int l_max = param.l_fmax;
	int m_max = param.m_fmax;
	int b_max = param.b_fmax;

	if ((freq_input[id].min <= (int)param.ltl_max_freq ||
		new_user_max != (int)param.over_limit) &&
		freq_input[id].last_over_limit_time != 0) {
		freq_input[id].time_in_over_limit += ktime_to_ms(ktime_get() -
			freq_input[id].last_over_limit_time);
		freq_input[id].last_over_limit_time = 0;
	} else if (freq_input[id].min > (int)param.ltl_max_freq &&
		new_user_max == (int)param.over_limit &&
		freq_input[id].last_over_limit_time == 0) {
		freq_input[id].last_over_limit_time = ktime_get();
	}

	if (need_update_user_max) {
		pr_debug("%s: update_user_max is true\n", __func__);
		if (new_user_max > param.big_max_freq) {
			pr_debug("%s: too high freq(%d), set to %d\n",
			__func__, new_user_max, param.big_max_freq);
			new_user_max = param.big_max_freq;
		}

		l_max = cpufreq_limit_get_ltl_limit(new_user_max);
		if (new_user_max < param.big_min_freq) {
			m_max = param.m_fmin;
			b_max = param.b_fmin;
		} else {
			b_max = MIN(new_user_max, param.b_fmax);
			m_max = MIN(new_user_max, param.m_fmax);
		}

		pr_info("%s: freq_update_request : new userspace max %d %d %d\n", __func__, l_max, m_max, b_max);
		freq_qos_update_request(&max_req[DVFS_USER_ID][param.ltl_cpu_start], l_max);
		freq_qos_update_request(&max_req[DVFS_USER_ID][param.mid_cpu_start], m_max);
		freq_qos_update_request(&max_req[DVFS_USER_ID][param.big_cpu_start], b_max);
	}
}

static void cpufreq_limit_process_min_freq(unsigned int id, int min_freq)
{
	int i = 0;
	int l_min = param.l_fmin;
	int m_min = param.m_fmin;
	int b_min = param.b_fmin;
	bool need_update_user_max = false;
	int new_user_max = FREQ_QOS_MAX_DEFAULT_VALUE;

	/* update input freq */
	freq_input[id].min = min_freq;
	if ((min_freq == LIMIT_RELEASE || min_freq == param.ltl_min_freq) &&
		freq_input[id].last_min_limit_time != 0) {
		freq_input[id].time_in_min_limit += ktime_to_ms(ktime_get()-
			freq_input[id].last_min_limit_time);
		freq_input[id].last_min_limit_time = 0;
		freq_input[id].boosted = NOT_BOOSTED;
		pr_debug("%s: type(%d), released(%d)\n",
			__func__, id, freq_input[id].boosted);
	} else if (min_freq != LIMIT_RELEASE && min_freq != param.ltl_min_freq &&
		freq_input[id].last_min_limit_time == 0) {
		freq_input[id].last_min_limit_time = ktime_get();
		freq_input[id].boosted = BOOSTED;
		pr_debug("%s: type(%d), boosted(%d)\n",
			__func__, id, freq_input[id].boosted);
	}

	/* apply min_freq */
	if (min_freq == LIMIT_RELEASE) {
		pr_info("%s: id(%u) min_lock_freq(%d)\n", __func__, id, min_freq);
		for_each_possible_cpu(i)
			freq_qos_update_request(&min_req[id][i],
					FREQ_QOS_MIN_DEFAULT_VALUE);

		if ((id == DVFS_USER_ID || id == DVFS_TOUCH_ID) &&
			cpufreq_limit_max_lock_need_restore()) {
			// if there is no high priority min lock and over limit is set
			if (freq_input[DVFS_USER_ID].max > 0) {
				need_update_user_max = true;
				new_user_max = freq_input[DVFS_USER_ID].max;
				pr_debug("%s: restore new_max => %d\n",
						__func__, new_user_max);
			}
		}
	} else if (min_freq > 0) {
		if (min_freq < param.ltl_min_freq) {
			pr_err("%s: too low freq(%d), set to %d\n",
				__func__, min_freq, param.ltl_min_freq);
			min_freq = param.ltl_min_freq;
		}

		pr_debug("%s: min_freq=%d, ltl_max=%d, over_limit=%d\n", __func__,
				min_freq, param.ltl_max_freq, param.over_limit);

		if ((id == DVFS_USER_ID || id == DVFS_TOUCH_ID)) {
			if (freq_input[DVFS_USER_ID].max > 0) {
				need_update_user_max = true;
				if (cpufreq_limit_high_pri_min_lock_required())
					new_user_max = MAX((int)param.over_limit, freq_input[DVFS_USER_ID].max);
				else
					new_user_max = freq_input[DVFS_USER_ID].max;
				pr_debug("%s: override new_max %d => %d,  userspace_min=%d, touch_min=%d, ltl_max=%d\n",
						__func__, freq_input[DVFS_USER_ID].max, new_user_max,
						freq_input[DVFS_USER_ID].min, freq_input[DVFS_TOUCH_ID].min,
						param.ltl_max_freq);
			}
		}

		l_min = cpufreq_limit_get_ltl_boost(min_freq);
		if (min_freq > param.ltl_max_freq) {
			m_min = MIN(min_freq, param.m_fmax);
			b_min = MIN(min_freq, param.b_fmax);
		} else {
			m_min = param.m_fmin;
			b_min = param.b_fmin;
		}

		pr_info("%s: little(%u-%u, set:%u), mid(set:%u), big(%u-%u, set:%u)\n", __func__,
				param.ltl_min_freq, param.ltl_max_freq, l_min, m_min,
				param.big_min_freq, param.big_max_freq, b_min);

		freq_qos_update_request(&min_req[id][param.ltl_cpu_start], l_min);
		freq_qos_update_request(&min_req[id][param.mid_cpu_start], m_min);
		freq_qos_update_request(&min_req[id][param.big_cpu_start], b_min);
	}
	cpufreq_limit_process_over_limit(id, need_update_user_max, new_user_max);
}

static void cpufreq_limit_process_max_freq(unsigned int id, int max_freq)
{
	int i = 0;
	int l_max = param.l_fmax;
	int m_max = param.m_fmax;
	int b_max = param.b_fmax;
	bool need_update_user_max = false;
	int new_user_max = FREQ_QOS_MAX_DEFAULT_VALUE;

	/* update input freq */
	freq_input[id].max = max_freq;
	if ((max_freq == LIMIT_RELEASE || max_freq == param.big_max_freq) &&
		freq_input[id].last_max_limit_time != 0) {
		freq_input[id].time_in_max_limit += ktime_to_ms(ktime_get() -
			freq_input[id].last_max_limit_time);
		freq_input[id].last_max_limit_time = 0;
	} else if (max_freq != LIMIT_RELEASE && max_freq != param.big_max_freq &&
		freq_input[id].last_max_limit_time == 0) {
		freq_input[id].last_max_limit_time = ktime_get();
	}

	/* apply max freq */
	if (max_freq == LIMIT_RELEASE) {
		pr_info("%s: id(%u) max_lock_freq(%d)\n", __func__, id, max_freq);
		for_each_possible_cpu(i)
			freq_qos_update_request(&max_req[id][i],
					FREQ_QOS_MAX_DEFAULT_VALUE);
	} else if (max_freq > 0) {
		if (max_freq > param.big_max_freq) {
			pr_err("%s: too high freq(%d), set to %d\n",
				__func__, max_freq, param.big_max_freq);
			max_freq = param.big_max_freq;
		}
		pr_info("%s: id(%u) max_lock_freq(%d)\n", __func__, id, max_freq);

		if ((id == DVFS_USER_ID) && // if userspace maxlock is being set
			cpufreq_limit_high_pri_min_lock_required()) {
			need_update_user_max = true;
			new_user_max = MAX((int)param.over_limit, freq_input[DVFS_USER_ID].max);
			pr_debug("%s: force up max_freq %d => %d, userspace_min=%d, touch_min=%d, ltl_max=%d\n",
					__func__, max_freq, new_user_max,
					freq_input[DVFS_USER_ID].min, freq_input[DVFS_TOUCH_ID].min,
					param.ltl_max_freq);
		}

		l_max = cpufreq_limit_get_ltl_limit(max_freq);
		if (max_freq < param.big_min_freq) {
			m_max = param.m_fmin;
			b_max = param.b_fmin;
		} else {
			b_max = MIN(max_freq, param.b_fmax);
			m_max = MIN(max_freq, param.m_fmax);
		}

		pr_info("%s: little(%u-%u, set:%u), mid(set:%u), big(%u-%u, set:%u)\n", __func__,
						param.ltl_min_freq, param.ltl_max_freq, l_max, m_max,
						param.big_min_freq, param.big_max_freq, b_max);

		freq_qos_update_request(&max_req[id][param.ltl_cpu_start], l_max);
		freq_qos_update_request(&max_req[id][param.mid_cpu_start], m_max);
		freq_qos_update_request(&max_req[id][param.big_cpu_start], b_max);
	}
	cpufreq_limit_process_over_limit(id, need_update_user_max, new_user_max);
}
/**
 * _set_freq_limit - core function to request frequencies
 * @id			request id
 * @min_req		frequency value to request min lock
 * @max_req		frequency value to request max lock
 */
static int _set_freq_limit(unsigned int id, int min_freq, int max_freq)
{
	pr_info("%s: input: id(%d), min(%d), max(%d)\n",
		__func__, id, min_freq, max_freq);

	mutex_lock(&cpufreq_limit_mutex);

	if (min_freq != 0)
		cpufreq_limit_process_min_freq(id, min_freq);

	if (max_freq != 0)
		cpufreq_limit_process_max_freq(id, max_freq);

	cpufreq_limit_update_current_freq();

	mutex_unlock(&cpufreq_limit_mutex);

	return 0;
}

/**
 * set_freq_limit - API provided for frequency min lock operation
 * @id		request id
 * @freq	value to change frequency
 */
int set_freq_limit(unsigned int id, unsigned int freq)
{
	_set_freq_limit(id, freq, 0);
	return 0;
}
EXPORT_SYMBOL(set_freq_limit);

/**
 * cpufreq_limit_get_table - fill the cpufreq table to support HMP
 * @buf		a buf that has been requested to fill the cpufreq table
 */
static ssize_t cpufreq_limit_get_table(char *buf)
{
	ssize_t len = 0;
	int i;

	if (!param.freq_count)
		return len;

	for (i = 0; i < param.freq_count; i++)
		len += snprintf(buf + len, MAX_BUF_SIZE, "%u ",
					param.unified_table[i]);

	len--;
	len += snprintf(buf + len, MAX_BUF_SIZE, "\n");

	return len;
}

/******************************/
/*  Kernel object functions   */
/******************************/

#define cpufreq_limit_attr(_name)				\
static struct kobj_attribute _name##_attr = {	\
	.attr	= {									\
		.name = __stringify(_name),				\
		.mode = 0644,							\
	},											\
	.show	= _name##_show,						\
	.store	= _name##_store,					\
}

#define cpufreq_limit_attr_ro(_name)			\
static struct kobj_attribute _name##_attr = {	\
	.attr	= {									\
		.name = __stringify(_name),				\
		.mode = 0444,							\
	},											\
	.show	= _name##_show,						\
}

static ssize_t cpufreq_table_show(struct kobject *kobj,
			struct kobj_attribute *attr, char *buf)
{
	return cpufreq_limit_get_table(buf);
}

static ssize_t cpufreq_max_limit_show(struct kobject *kobj,
					struct kobj_attribute *attr,
					char *buf)
{
	return sprintf(buf, "%d\n", last_max_req_val);
}

static ssize_t cpufreq_max_limit_store(struct kobject *kobj,
					struct kobj_attribute *attr,
					const char *buf, size_t count)
{
	int val;
	ssize_t ret = -EINVAL;

	if (sscanf(buf, "%d", &val) != 1) {
		pr_err("%s: Invalid cpufreq format\n", __func__);
		goto out;
	}

	_set_freq_limit(DVFS_USER_ID, 0, val);
	last_max_req_val = val;
	ret = count;
out:
	return ret;
}

static ssize_t cpufreq_min_limit_show(struct kobject *kobj,
					struct kobj_attribute *attr,
					char *buf)
{
	return sprintf(buf, "%d\n", last_min_req_val);
}

static ssize_t cpufreq_min_limit_store(struct kobject *kobj,
					struct kobj_attribute *attr,
					const char *buf, size_t count)
{
	int val;
	ssize_t ret = -EINVAL;

	if (sscanf(buf, "%d", &val) != 1) {
		pr_err("%s: Invalid cpufreq format\n", __func__);
		goto out;
	}

	_set_freq_limit(DVFS_USER_ID, val, 0);
	last_min_req_val = val;
	ret = count;
out:
	return ret;
}

static ssize_t over_limit_show(struct kobject *kobj,
					struct kobj_attribute *attr,
					char *buf)
{
	return sprintf(buf, "%d\n", param.over_limit);
}

static ssize_t over_limit_store(struct kobject *kobj,
					struct kobj_attribute *attr,
					const char *buf, size_t count)
{
	unsigned int val;
	ssize_t ret = -EINVAL;

	ret = kstrtoint(buf, 10, &val);
	if (ret < 0) {
		pr_err("%s: Invalid cpufreq format\n", __func__);
		goto out;
	}

	param.over_limit = (unsigned int)val;
	if (last_max_req_val > 0)
		_set_freq_limit(DVFS_USER_ID, 0, last_max_req_val);
	ret = count;
out:
	return ret;
}

cpufreq_limit_attr_ro(cpufreq_table);
cpufreq_limit_attr(cpufreq_max_limit);
cpufreq_limit_attr(cpufreq_min_limit);
cpufreq_limit_attr(over_limit);

static struct attribute *cpufreq_limit_attr_g[] = {
	&cpufreq_table_attr.attr,
	&cpufreq_max_limit_attr.attr,
	&cpufreq_min_limit_attr.attr,
	&over_limit_attr.attr,
	NULL,
};

static const struct attribute_group cpufreq_limit_attr_group = {
	.attrs = cpufreq_limit_attr_g,
};

#define show_one(_name, object)										\
static ssize_t _name##_show											\
	(struct kobject *kobj, struct kobj_attribute *attr, char *buf)	\
{																	\
	return sprintf(buf, "%u\n", param.object);						\
}

show_one(ltl_cpu_start, ltl_cpu_start);
show_one(big_cpu_start, big_cpu_start);
show_one(ltl_max_freq, ltl_max_freq);
show_one(ltl_min_lock_freq, ltl_min_lock_freq);
show_one(big_max_lock_freq, big_max_lock_freq);
show_one(ltl_divider, ltl_divider);

static ssize_t ltl_max_freq_store(struct kobject *kobj,
					struct kobj_attribute *attr,
					const char *buf, size_t count)
{
	int val;
	ssize_t ret = -EINVAL;

	if (sscanf(buf, "%d", &val) != 1) {
		pr_err("%s: Invalid cpufreq format\n", __func__);
		goto out;
	}

	param.ltl_max_freq = val;
	ltl_max_freq_div = (int)(param.ltl_max_freq / param.ltl_divider);

	ret = count;
out:
	return ret;
}

static ssize_t ltl_min_lock_freq_store(struct kobject *kobj,
					struct kobj_attribute *attr,
					const char *buf, size_t count)
{
	int val;
	ssize_t ret = -EINVAL;

	if (sscanf(buf, "%d", &val) != 1) {
		pr_err("%s: Invalid cpufreq format\n", __func__);
		goto out;
	}

	param.ltl_min_lock_freq = val;

	ret = count;
out:
	return ret;
}

static ssize_t big_max_lock_freq_store(struct kobject *kobj,
					struct kobj_attribute *attr,
					const char *buf, size_t count)
{
	int val;
	ssize_t ret = -EINVAL;

	if (sscanf(buf, "%d", &val) != 1) {
		pr_err("%s: Invalid cpufreq format\n", __func__);
		goto out;
	}

	param.big_max_lock_freq = val;

	ret = count;
out:
	return ret;
}

static ssize_t ltl_divider_store(struct kobject *kobj,
					struct kobj_attribute *attr,
					const char *buf, size_t count)
{
	int val;
	ssize_t ret = -EINVAL;

	if (sscanf(buf, "%d", &val) != 1) {
		pr_err("%s: Invalid cpufreq format\n", __func__);
		goto out;
	}

	param.ltl_divider = val;
	ltl_max_freq_div = (int)(param.ltl_max_freq / param.ltl_divider);

	ret = count;
out:
	return ret;
}

cpufreq_limit_attr_ro(ltl_cpu_start);
cpufreq_limit_attr_ro(big_cpu_start);
cpufreq_limit_attr(ltl_max_freq);
cpufreq_limit_attr(ltl_min_lock_freq);
cpufreq_limit_attr(big_max_lock_freq);
cpufreq_limit_attr(ltl_divider);

static struct attribute *limit_param_attributes[] = {
	&ltl_cpu_start_attr.attr,
	&big_cpu_start_attr.attr,
	&ltl_max_freq_attr.attr,
	&ltl_min_lock_freq_attr.attr,
	&big_max_lock_freq_attr.attr,
	&ltl_divider_attr.attr,
	NULL,
};

static struct attribute_group limit_param_attr_group = {
	.attrs = limit_param_attributes,
	.name = "parameters",
};

static int cpufreq_limit_add_qos(void)
{
	struct cpufreq_policy *policy;
	unsigned int j;
	unsigned int i = 0;
	int ret = 0;

	for_each_possible_cpu(i) {
		policy = cpufreq_cpu_get(i);
		if (!policy) {
			pr_err("%s: no policy for cpu %d\n", __func__, i);
			ret = -EPROBE_DEFER;
			break;
		}

		for (j = 0; j < DVFS_MAX_ID; j++) {
			ret = freq_qos_add_request(&policy->constraints,
							&min_req[j][i],
							FREQ_QOS_MIN, policy->cpuinfo.min_freq);
			if (ret < 0) {
				pr_err("%s: no policy for cpu %d\n", __func__, i);
				break;
			}

			ret = freq_qos_add_request(&policy->constraints,
							&max_req[j][i],
							FREQ_QOS_MAX, policy->cpuinfo.max_freq);
			if (ret < 0) {
				pr_err("%s: no policy for cpu %d\n", __func__, i);
				break;
			}
		}
		if (ret < 0) {
			cpufreq_cpu_put(policy);
			break;
		}

		if (i == param.ltl_cpu_start) {
			param.l_fmin = policy->cpuinfo.min_freq;
			param.l_fmax = policy->cpuinfo.max_freq;
		}
		if (i == param.mid_cpu_start) {
			param.m_fmin = policy->cpuinfo.min_freq;
			param.m_fmax = policy->cpuinfo.max_freq;
		}
		if (i == param.big_cpu_start) {
			param.b_fmin = policy->cpuinfo.min_freq;
			param.b_fmax = policy->cpuinfo.max_freq;
		}
		/*
		 * Set cpufreq table.
		 * Create a virtual table when internal condition are met.
		 */
		cpufreq_limit_set_table(policy->cpu, policy->freq_table);

		cpufreq_cpu_put(policy);
	}

	return ret;
}

static int cpufreq_limit_req_table_alloc(void)
{
	int i;

	for (i = 0; i < DVFS_MAX_ID; i++) {
		max_req[i] = kcalloc(param.num_cpu, sizeof(struct freq_qos_request),
							GFP_KERNEL);
		min_req[i] = kcalloc(param.num_cpu, sizeof(struct freq_qos_request),
							GFP_KERNEL);

		if (!max_req[i] || !min_req[i])
			return -ENOMEM;
	}
	return 0;
}

#if IS_ENABLED(CONFIG_OF)
static int cpufreq_limit_parse_dt(struct device_node *np)
//static int cpufreq_limit_parse_dt(struct platform_device *pdev)
{
	int ret;
	u32 val;

	if (!np) {
		pr_info("%s: no device tree\n", __func__);
		return -EINVAL;
	}

	ret = of_property_read_u32(np, "num_cpu", &val);
	if (ret)
		return -EINVAL;
	param.num_cpu = val;

	ret = of_property_read_u32(np, "ltl_cpu_start", &val);
	if (ret)
		return -EINVAL;
	param.ltl_cpu_start = val;

	ret = of_property_read_u32(np, "big_cpu_start", &val);
	if (ret)
		return -EINVAL;
	param.big_cpu_start = val;

	ret = of_property_read_u32(np, "mid_cpu_start", &val);
	if (ret)
		return -EINVAL;
	param.mid_cpu_start = val;

	ret = of_property_read_u32(np, "ltl_min_lock_freq", &val);
	if (ret)
		return -EINVAL;
	param.ltl_min_lock_freq = val;

	ret = of_property_read_u32(np, "big_max_lock_freq", &val);
	if (ret)
		return -EINVAL;
	param.big_max_lock_freq = val;

	ret = of_property_read_u32(np, "ltl_divider", &val);
	if (ret)
		return -EINVAL;
	param.ltl_divider = val;

	of_get_property(np, "ltl_limit_table", &val);
	if (val) {
		//param.ltl_limit_map = devm_kzalloc(pdev->dev, val, GFP_KERNEL);
		param.ltl_limit_map = kcalloc(1, val, GFP_KERNEL);
		of_property_read_u32_array(np, "ltl_limit_table",
				(u32 *)param.ltl_limit_map, val / sizeof(u32));
		param.limit_map_size = val / sizeof(*param.ltl_limit_map);
	}
	pr_info("%s: param: limit map size(%d)\n", __func__, param.limit_map_size);

	of_get_property(np, "ltl_boost_table", &val);
	if (val) {
		param.ltl_boost_map = kcalloc(1, val, GFP_KERNEL);
		of_property_read_u32_array(np, "ltl_boost_table",
				(u32 *)param.ltl_boost_map, val / sizeof(u32));
		param.boost_map_size = val / sizeof(*param.ltl_boost_map);
	}
	pr_info("%s: param: boost map size(%d)\n", __func__, param.boost_map_size);
	return ret;
}
#endif

int cpufreq_limit_probe(struct platform_device *pdev)
{
	int ret;

	pr_info("%s: start\n", __func__);

#if IS_ENABLED(CONFIG_OF)
	ret = cpufreq_limit_parse_dt(pdev->dev.of_node);
	if (ret < 0) {
		pr_err("%s: fail to parsing device tree\n", __func__);
		goto probe_failed;
	}
#endif
	ret = cpufreq_limit_req_table_alloc();
	if (ret < 0) {
		pr_err("%s: fail to allocation memory\n", __func__);
		goto probe_failed;
	}

	/* Add QoS request */
	ret = cpufreq_limit_add_qos();
	if (ret) {
		pr_err("%s: add qos failed %d\n", __func__, ret);
		goto probe_failed;
	}

	cpufreq_kobj = kobject_create_and_add("cpufreq_limit",
						&cpu_subsys.dev_root->kobj);

	if (!cpufreq_kobj) {
		pr_err("%s: fail to create cpufreq kobj\n", __func__);
		ret = -EAGAIN;
		goto probe_failed;
	}

	ret = sysfs_create_group(cpufreq_kobj, &cpufreq_limit_attr_group);
	if (ret) {
		pr_err("%s: create cpufreq object failed %d\n", __func__, ret);
		goto probe_failed;
	}

	ret = sysfs_create_group(cpufreq_kobj, &limit_param_attr_group);
	if (ret) {
		pr_err("%s: create cpufreq param object failed %d\n", __func__, ret);
		goto probe_failed;
	}

	pr_info("%s: done\n", __func__);

probe_failed:
	return ret;
}

static int cpufreq_limit_remove(struct platform_device *pdev)
{
	int i = 0, j;
	int ret = 0;

	pr_info("%s: start\n", __func__);

	/* remove kernel object */
	if (cpufreq_kobj) {
		sysfs_remove_group(cpufreq_kobj, &limit_param_attr_group);
		sysfs_remove_group(cpufreq_kobj, &cpufreq_limit_attr_group);
		kobject_put(cpufreq_kobj);
		cpufreq_kobj = NULL;
	}

	/* remove qos request */
	for_each_possible_cpu(i) {
		for (j = 0; j < DVFS_MAX_ID; j++) {
			ret = freq_qos_remove_request(&min_req[j][i]);
			if (ret < 0)
				pr_err("%s: failed to remove min_req %d\n", __func__, ret);

			ret = freq_qos_remove_request(&max_req[j][i]);
			if (ret < 0)
				pr_err("%s: failed to remove max_req %d\n", __func__, ret);
		}
	}

	/* deallocation memory */
	for (i = 0; i < DVFS_MAX_ID; i++) {
		kfree(min_req[i]);
		kfree(max_req[i]);
	}

	pr_info("%s: done\n", __func__);
	return ret;
}

static const struct of_device_id cpufreq_limit_match_table[] = {
	{ .compatible = "cpufreq_limit" },
	{}
};

static struct platform_driver cpufreq_limit_driver = {
	.driver = {
		.name = "cpufreq_limit",
		.of_match_table = cpufreq_limit_match_table,
	},
	.probe = cpufreq_limit_probe,
	.remove = cpufreq_limit_remove,
};

static int __init cpufreq_limit_init(void)
{
	return platform_driver_register(&cpufreq_limit_driver);
}

static void __exit cpufreq_limit_exit(void)
{
	platform_driver_unregister(&cpufreq_limit_driver);
}

MODULE_AUTHOR("Sangyoung Son <hello.son@samsung.com>");
MODULE_AUTHOR("Jonghyeon Cho <jongjaaa.cho@samsung.com>");
MODULE_DESCRIPTION("A driver to limit cpu frequency");
MODULE_LICENSE("GPL");

late_initcall(cpufreq_limit_init);
module_exit(cpufreq_limit_exit);
